/* QLogic (R)NIC Driver/Library
 * Copyright (c) 2010-2017  Cavium, Inc.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/types.h>
#include <asm/byteorder.h>
#include <asm/cache.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/if_ether.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/log2.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#define __PREVENT_DUMP_MEM_ARR__
#define __PREVENT_PXP_GLOBAL_WIN__
#include "qed.h"
#include "qed_chain.h"
#include "qed_cxt.h"
#include "qed_dbg_hsi.h"
#include "qed_dcbx.h"
#include "qed_dev_api.h"
#include "qed_fcoe.h"
#include "qed_hsi.h"
#include "qed_hw.h"
#include "qed_init_ops.h"
#include "qed_int.h"
#include "qed_iro_hsi.h"
#include "qed_iscsi.h"
#include "qed_l2.h"
#include "qed_ll2.h"
#include "qed_mcp.h"
#include "qed_mfw_hsi.h"
#include "qed_ooo.h"
#include "qed_rdma.h"
#include "qed_reg_addr.h"
#include "qed_sp.h"
#include "qed_sriov.h"
#include "qed_vf.h"
#include "qed_debug.h"
#include "qed_eth_if.h"

/* TODO - there's a bug in DCBx re-configuration flows in MF, as the QM
 * registers involved are not split and thus configuration is a race where
 * some of the PFs configuration might be lost.
 * Eventually, this needs to move into a MFW-covered HW-lock as arbitration
 * mechanism as this doesn't cover some cases [E.g., PDA or scenarios where
 * there's more than a single compiled qed component in system].
 */
static spinlock_t qm_lock;
static u32 qm_lock_ref_cnt;

#ifndef ASIC_ONLY
static bool b_ptt_gtt_init;
#endif

void qed_set_ilt_page_size(struct qed_dev *cdev, u8 ilt_page_size)
{
	cdev->ilt_page_size = ilt_page_size;
}

/******************** Doorbell Recovery *******************/
/* The doorbell recovery mechanism consists of a list of entries which represent
 * doorbelling entities (l2 queues, roce sq/rq/cqs, the slowpath spq, etc). Each
 * entity needs to register with the mechanism and provide the parameters
 * describing it's doorbell, including a location where last used doorbell data
 * can be found. The doorbell execute function will traverse the list and
 * doorbell all of the registered entries.
 */
struct qed_db_recovery_entry {
	struct list_head list_entry;
	void __iomem *db_addr;
	void *db_data;
	enum qed_db_rec_width db_width;
	enum qed_db_rec_space db_space;
	u8 hwfn_idx;
};

/* display a single doorbell recovery entry */
static void qed_db_recovery_dp_entry(struct qed_hwfn *p_hwfn,
				     struct qed_db_recovery_entry *db_entry,
				     char *action)
{
	DP_VERBOSE(p_hwfn,
		   QED_MSG_SPQ,
		   "(%s: db_entry %p, addr %p, data %p, width %s, %s space, hwfn %d)\n",
		   action,
		   db_entry,
		   db_entry->db_addr,
		   db_entry->db_data,
		   db_entry->db_width == DB_REC_WIDTH_32B ? "32b" : "64b",
		   db_entry->db_space == DB_REC_USER ? "user" : "kernel",
		   db_entry->hwfn_idx);
}

/* find hwfn according to the doorbell address */
static struct qed_hwfn *qed_db_rec_find_hwfn(struct qed_dev *cdev,
					     void __iomem * db_addr)
{
	struct qed_hwfn *p_hwfn;

	/* in CMT doorbell bar is split down the middle between engine 0 and enigne 1 */
	if (QED_IS_CMT(cdev))
		p_hwfn = db_addr < cdev->hwfns[1].doorbells ?
		    &cdev->hwfns[0] : &cdev->hwfns[1];
	else
		p_hwfn = QED_LEADING_HWFN(cdev);

	return p_hwfn;
}

/* doorbell address sanity (address within doorbell bar range) */
static bool qed_db_rec_sanity(struct qed_dev *cdev,
			      void __iomem * db_addr,
			      enum qed_db_rec_width db_width, void *db_data)
{
	struct qed_hwfn *p_hwfn = qed_db_rec_find_hwfn(cdev, db_addr);
	u32 width = (db_width == DB_REC_WIDTH_32B) ? 32 : 64;

	/* make sure doorbell address is within the doorbell bar */
	if (db_addr < p_hwfn->doorbells ||
	    (u8 __iomem *) db_addr + width >
	    (u8 __iomem *) p_hwfn->doorbells + p_hwfn->db_size) {
		WARN(true,
		     "Illegal doorbell address: %p. Legal range for doorbell addresses is [%p..%p]\n",
		     db_addr,
		     p_hwfn->doorbells,
		     (u8 __iomem *) p_hwfn->doorbells + p_hwfn->db_size);

		return false;
	}

	/* make sure doorbell data pointer is not null */
	if (!db_data) {
		WARN(true, "Illegal doorbell data pointer: %p", db_data);
		return false;
	}

	return true;
}

/* add a new entry to the doorbell recovery mechanism */
int qed_db_recovery_add(struct qed_dev *cdev,
			void __iomem * db_addr,
			void *db_data,
			enum qed_db_rec_width db_width,
			enum qed_db_rec_space db_space)
{
	struct qed_db_recovery_entry *db_entry;
	struct qed_hwfn *p_hwfn;

	/* sanitize doorbell address */
	if (!qed_db_rec_sanity(cdev, db_addr, db_width, db_data))
		return -EINVAL;

	/* obtain hwfn from doorbell address */
	p_hwfn = qed_db_rec_find_hwfn(cdev, db_addr);

	/* create entry */
	db_entry = kzalloc(sizeof(*db_entry), GFP_KERNEL);
	if (!db_entry) {
		DP_NOTICE(cdev, "Failed to allocate a db recovery entry\n");
		return -ENOMEM;
	}

	/* populate entry */
	db_entry->db_addr = db_addr;
	db_entry->db_data = db_data;
	db_entry->db_width = db_width;
	db_entry->db_space = db_space;
	db_entry->hwfn_idx = p_hwfn->my_id;

	/* display */
	qed_db_recovery_dp_entry(p_hwfn, db_entry, "Adding");

	/* protect the list */
	spin_lock_bh(&p_hwfn->db_recovery_info.lock);
	list_add_tail(&db_entry->list_entry, &p_hwfn->db_recovery_info.list);
	spin_unlock_bh(&p_hwfn->db_recovery_info.lock);

	return 0;
}

/* remove an entry from the doorbell recovery mechanism */
int qed_db_recovery_del(struct qed_dev *cdev,
			void __iomem * db_addr, void *db_data)
{
	struct qed_db_recovery_entry *db_entry = NULL;
	int rc = -EINVAL;
	struct qed_hwfn *p_hwfn;

	/* obtain hwfn from doorbell address */
	p_hwfn = qed_db_rec_find_hwfn(cdev, db_addr);

	/* protect the list */
	spin_lock_bh(&p_hwfn->db_recovery_info.lock);
	list_for_each_entry(db_entry,
			    &p_hwfn->db_recovery_info.list, list_entry) {
		/* search according to db_data addr since db_addr is not unique (roce) */
		if (db_entry->db_data == db_data) {
			qed_db_recovery_dp_entry(p_hwfn, db_entry, "Deleting");
			list_del(&db_entry->list_entry);
			rc = 0;
			break;
		}
	}

	spin_unlock_bh(&p_hwfn->db_recovery_info.lock);

	if (rc == -EINVAL)
		/*WARN(true, */
		DP_NOTICE(p_hwfn,
			  "Failed to find element in list. Key (db_data addr) was %p. db_addr was %p\n",
			  db_data, db_addr);
	else
		kfree(db_entry);

	return rc;
}

/* initialize the doorbell recovery mechanism */
static int qed_db_recovery_setup(struct qed_hwfn *p_hwfn)
{
	DP_VERBOSE(p_hwfn, QED_MSG_SPQ, "Setting up db recovery\n");

	/* make sure db_size was set in cdev */
	if (!p_hwfn->db_size) {
		DP_ERR(p_hwfn->cdev, "db_size not set\n");
		return -EINVAL;
	}

	INIT_LIST_HEAD(&p_hwfn->db_recovery_info.list);
	spin_lock_init(&p_hwfn->db_recovery_info.lock);
	p_hwfn->db_recovery_info.count = 0;
	p_hwfn->db_recovery_info.setup_done = true;

	return 0;
}

/* destroy the doorbell recovery mechanism */
static void qed_db_recovery_teardown(struct qed_hwfn *p_hwfn)
{
	struct qed_db_recovery_entry *db_entry = NULL;

	if (!p_hwfn->db_recovery_info.setup_done)
		return;

	DP_VERBOSE(p_hwfn, QED_MSG_SPQ, "Tearing down db recovery\n");
	if (!list_empty(&p_hwfn->db_recovery_info.list)) {
		DP_VERBOSE(p_hwfn,
			   QED_MSG_SPQ,
			   "Doorbell Recovery teardown found the doorbell recovery list was not empty (Expected in disorderly driver unload (e.g. recovery) otherwise this probably means some flow forgot to db_recovery_del). Prepare to purge doorbell recovery list...\n");
		while (!list_empty(&p_hwfn->db_recovery_info.list)) {
			db_entry =
			    list_first_entry(&p_hwfn->db_recovery_info.list,
					     struct qed_db_recovery_entry,
					     list_entry);
			qed_db_recovery_dp_entry(p_hwfn, db_entry, "Purging");
			list_del(&db_entry->list_entry);
			kfree(db_entry);
		}
	}
	p_hwfn->db_recovery_info.count = 0;
	p_hwfn->db_recovery_info.setup_done = false;
}

/* print the content of the doorbell recovery mechanism */
void qed_db_recovery_dp(struct qed_hwfn *p_hwfn)
{
	struct qed_db_recovery_entry *db_entry = NULL;
	u32 dp_module;
	u8 dp_level;

	DP_NOTICE(p_hwfn,
		  "Displaying doorbell recovery database. Counter is %d\n",
		  p_hwfn->db_recovery_info.count);

	if (IS_PF(p_hwfn->cdev))
		if (p_hwfn->pf_iov_info->max_db_rec_count > 0)
			DP_NOTICE(p_hwfn,
				  "Max VF counter is %u (VF %u)\n",
				  p_hwfn->pf_iov_info->max_db_rec_count,
				  p_hwfn->pf_iov_info->max_db_rec_vfid);

	/* Save dp_module/dp_level values and enable QED_MSG_SPQ verbosity
	 * to force print the db entries.
	 */
	dp_module = p_hwfn->dp_module;
	p_hwfn->dp_module |= QED_MSG_SPQ;
	dp_level = p_hwfn->dp_level;
	p_hwfn->dp_level = QED_LEVEL_VERBOSE;

	/* protect the list */
	spin_lock_bh(&p_hwfn->db_recovery_info.lock);
	list_for_each_entry(db_entry,
			    &p_hwfn->db_recovery_info.list, list_entry) {
		qed_db_recovery_dp_entry(p_hwfn, db_entry, "Printing");
	}

	spin_unlock_bh(&p_hwfn->db_recovery_info.lock);

	/* Get back to saved dp_module/dp_level values */
	p_hwfn->dp_module = dp_module;
	p_hwfn->dp_level = dp_level;
}

/* ring the doorbell of a single doorbell recovery entry */
static void qed_db_recovery_ring(struct qed_hwfn *p_hwfn,
				 struct qed_db_recovery_entry *db_entry)
{
	/* Print according to width */
	if (db_entry->db_width == DB_REC_WIDTH_32B) {
		DP_VERBOSE(p_hwfn, QED_MSG_SPQ,
			   "ringing doorbell address %p data %x\n",
			   db_entry->db_addr, *(u32 *) db_entry->db_data);
	} else {
		DP_VERBOSE(p_hwfn, QED_MSG_SPQ,
			   "ringing doorbell address %p data %llx\n",
			   db_entry->db_addr, *(u64 *) (db_entry->db_data));
	}

	/* Flush the write combined buffer. Since there are multiple doorbelling
	 * entities using the same address, if we don't flush, a transaction
	 * could be lost.
	 */
	wmb();

	/* Ring the doorbell */
	if (db_entry->db_width == DB_REC_WIDTH_32B)
		DIRECT_REG_WR(db_entry->db_addr, *(u32 *) (db_entry->db_data));
	else
		DIRECT_REG_WR64(db_entry->db_addr,
				*(u64 *) (db_entry->db_data));

	/* Flush the write combined buffer. Next doorbell may come from a
	 * different entity to the same address...
	 */
	wmb();
}

/* traverse the doorbell recovery entry list and ring all the doorbells */
void qed_db_recovery_execute(struct qed_hwfn *p_hwfn)
{
	struct qed_db_recovery_entry *db_entry = NULL;

	DP_NOTICE(p_hwfn,
		  "Executing doorbell recovery. Counter is %d\n",
		  ++p_hwfn->db_recovery_info.count);

	/* protect the list */
	spin_lock_bh(&p_hwfn->db_recovery_info.lock);
	list_for_each_entry(db_entry,
			    &p_hwfn->db_recovery_info.list,
			    list_entry) qed_db_recovery_ring(p_hwfn, db_entry);
	spin_unlock_bh(&p_hwfn->db_recovery_info.lock);
}

/******************** Doorbell Recovery end ****************/

/********************************** NIG LLH ***********************************/

enum qed_llh_filter_type {
	QED_LLH_FILTER_TYPE_MAC,
	QED_LLH_FILTER_TYPE_PROTOCOL,
};

struct qed_llh_mac_filter {
	u8 addr[ETH_ALEN];
};

struct qed_llh_protocol_filter {
	enum qed_llh_prot_filter_type_t type;
	u16 source_port_or_eth_type;
	u16 dest_port;
};

union qed_llh_filter {
	struct qed_llh_mac_filter mac;
	struct qed_llh_protocol_filter protocol;
};

struct qed_llh_filter_info {
	bool b_enabled;
	u32 ref_cnt;
	enum qed_llh_filter_type type;
	union qed_llh_filter filter;
};

struct qed_llh_info {
	/* Number of LLH filters banks */
	u8 num_ppfid;

#define MAX_NUM_PPFID   8
	u8 ppfid_array[MAX_NUM_PPFID];

	/* Array of filters arrays:
	 * "num_ppfid" elements of filters banks, where each is an array of
	 * "NIG_REG_LLH_FUNC_FILTER_EN_SIZE" filters.
	 */
	struct qed_llh_filter_info **pp_filters;
};

static void qed_llh_free(struct qed_dev *cdev)
{
	struct qed_llh_info *p_llh_info = cdev->p_llh_info;
	u32 i;

	if (p_llh_info != NULL) {
		if (p_llh_info->pp_filters != NULL)
			for (i = 0; i < p_llh_info->num_ppfid; i++)
				kfree(p_llh_info->pp_filters[i]);

		kfree(p_llh_info->pp_filters);
	}

	kfree(p_llh_info);
	cdev->p_llh_info = NULL;
}

static int qed_llh_alloc(struct qed_dev *cdev)
{
	struct qed_llh_info *p_llh_info;
	u32 size;
	u8 i;

	p_llh_info = kzalloc(sizeof(*p_llh_info), GFP_KERNEL);
	if (!p_llh_info)
		return -ENOMEM;
	cdev->p_llh_info = p_llh_info;

	for (i = 0; i < MAX_NUM_PPFID; i++) {
		if (!(cdev->ppfid_bitmap & (0x1 << i)))
			continue;

		p_llh_info->ppfid_array[p_llh_info->num_ppfid] = i;
		DP_VERBOSE(cdev, QED_MSG_SP, "ppfid_array[%d] = %hhd\n",
			   p_llh_info->num_ppfid, i);
		p_llh_info->num_ppfid++;
	}

	size = p_llh_info->num_ppfid * sizeof(*p_llh_info->pp_filters);
	p_llh_info->pp_filters = kzalloc(size, GFP_KERNEL);
	if (!p_llh_info->pp_filters)
		return -ENOMEM;

	size = NIG_REG_LLH_FUNC_FILTER_EN_SIZE *
	    sizeof(**p_llh_info->pp_filters);
	for (i = 0; i < p_llh_info->num_ppfid; i++) {
		p_llh_info->pp_filters[i] = kzalloc(size, GFP_KERNEL);
		if (!p_llh_info->pp_filters[i])
			return -ENOMEM;
	}

	return 0;
}

static int qed_llh_shadow_sanity(struct qed_dev *cdev,
				 u8 ppfid, u8 filter_idx, const char *action)
{
	struct qed_llh_info *p_llh_info = cdev->p_llh_info;

	if (ppfid >= p_llh_info->num_ppfid) {
		DP_NOTICE(cdev,
			  "LLH shadow [%s]: using ppfid %d while only %d ppfids are available\n",
			  action, ppfid, p_llh_info->num_ppfid);
		return -EINVAL;
	}

	if (filter_idx >= NIG_REG_LLH_FUNC_FILTER_EN_SIZE) {
		DP_NOTICE(cdev,
			  "LLH shadow [%s]: using filter_idx %d while only %d filters are available\n",
			  action, filter_idx, NIG_REG_LLH_FUNC_FILTER_EN_SIZE);
		return -EINVAL;
	}

	return 0;
}

#define QED_LLH_INVALID_FILTER_IDX      0xff

static int
qed_llh_shadow_search_filter(struct qed_dev *cdev,
			     u8 ppfid,
			     union qed_llh_filter *p_filter, u8 * p_filter_idx)
{
	struct qed_llh_info *p_llh_info = cdev->p_llh_info;
	struct qed_llh_filter_info *p_filters;
	int rc;
	u8 i;

	rc = qed_llh_shadow_sanity(cdev, ppfid, 0, "search");
	if (rc)
		return rc;

	*p_filter_idx = QED_LLH_INVALID_FILTER_IDX;

	p_filters = p_llh_info->pp_filters[ppfid];
	for (i = 0; i < NIG_REG_LLH_FUNC_FILTER_EN_SIZE; i++) {
		if (!memcmp(p_filter, &p_filters[i].filter, sizeof(*p_filter))) {
			*p_filter_idx = i;
			break;
		}
	}

	return 0;
}

static int
qed_llh_shadow_get_free_idx(struct qed_dev *cdev, u8 ppfid, u8 * p_filter_idx)
{
	struct qed_llh_info *p_llh_info = cdev->p_llh_info;
	struct qed_llh_filter_info *p_filters;
	int rc;
	u8 i;

	rc = qed_llh_shadow_sanity(cdev, ppfid, 0, "get_free_idx");
	if (rc)
		return rc;

	*p_filter_idx = QED_LLH_INVALID_FILTER_IDX;

	p_filters = p_llh_info->pp_filters[ppfid];
	for (i = 0; i < NIG_REG_LLH_FUNC_FILTER_EN_SIZE; i++) {
		if (!p_filters[i].b_enabled) {
			*p_filter_idx = i;
			break;
		}
	}

	return 0;
}

static int
__qed_llh_shadow_add_filter(struct qed_dev *cdev,
			    u8 ppfid,
			    u8 filter_idx,
			    enum qed_llh_filter_type type,
			    union qed_llh_filter *p_filter, u32 * p_ref_cnt)
{
	struct qed_llh_info *p_llh_info = cdev->p_llh_info;
	struct qed_llh_filter_info *p_filters;
	int rc;

	rc = qed_llh_shadow_sanity(cdev, ppfid, filter_idx, "add");
	if (rc)
		return rc;

	p_filters = p_llh_info->pp_filters[ppfid];
	if (!p_filters[filter_idx].ref_cnt) {
		p_filters[filter_idx].b_enabled = true;
		p_filters[filter_idx].type = type;
		memcpy(&p_filters[filter_idx].filter, p_filter,
		       sizeof(p_filters[filter_idx].filter));
	}

	*p_ref_cnt = ++p_filters[filter_idx].ref_cnt;

	return 0;
}

static int
qed_llh_shadow_add_filter(struct qed_dev *cdev,
			  u8 ppfid,
			  enum qed_llh_filter_type type,
			  union qed_llh_filter *p_filter,
			  u8 * p_filter_idx, u32 * p_ref_cnt)
{
	int rc;

	/* Check if the same filter already exist */
	rc = qed_llh_shadow_search_filter(cdev, ppfid, p_filter, p_filter_idx);
	if (rc)
		return rc;

	/* Find a new entry in case of a new filter */
	if (*p_filter_idx == QED_LLH_INVALID_FILTER_IDX) {
		rc = qed_llh_shadow_get_free_idx(cdev, ppfid, p_filter_idx);
		if (rc)
			return rc;
	}

	/* No free entry was found */
	if (*p_filter_idx == QED_LLH_INVALID_FILTER_IDX) {
		DP_NOTICE(cdev,
			  "Failed to find an empty LLH filter to utilize [ppfid %d]\n",
			  ppfid);
		return -EINVAL;
	}

	return __qed_llh_shadow_add_filter(cdev, ppfid, *p_filter_idx, type,
					   p_filter, p_ref_cnt);
}

static int
__qed_llh_shadow_remove_filter(struct qed_dev *cdev,
			       u8 ppfid, u8 filter_idx, u32 * p_ref_cnt)
{
	struct qed_llh_info *p_llh_info = cdev->p_llh_info;
	struct qed_llh_filter_info *p_filters;
	int rc;

	rc = qed_llh_shadow_sanity(cdev, ppfid, filter_idx, "remove");
	if (rc)
		return rc;

	p_filters = p_llh_info->pp_filters[ppfid];
	if (!p_filters[filter_idx].ref_cnt) {
		DP_NOTICE(cdev,
			  "LLH shadow: trying to remove a filter with ref_cnt=0\n");
		return -EINVAL;
	}

	*p_ref_cnt = --p_filters[filter_idx].ref_cnt;
	if (!p_filters[filter_idx].ref_cnt)
		memset(&p_filters[filter_idx],
		       0, sizeof(p_filters[filter_idx]));

	return 0;
}

static int
qed_llh_shadow_remove_filter(struct qed_dev *cdev,
			     u8 ppfid,
			     union qed_llh_filter *p_filter,
			     u8 * p_filter_idx, u32 * p_ref_cnt)
{
	int rc;

	rc = qed_llh_shadow_search_filter(cdev, ppfid, p_filter, p_filter_idx);
	if (rc)
		return rc;

	/* No matching filter was found */
	if (*p_filter_idx == QED_LLH_INVALID_FILTER_IDX) {
		DP_NOTICE(cdev, "Failed to find a filter in the LLH shadow\n");
		return -EINVAL;
	}

	return __qed_llh_shadow_remove_filter(cdev, ppfid, *p_filter_idx,
					      p_ref_cnt);
}

static int qed_llh_shadow_remove_all_filters(struct qed_dev *cdev, u8 ppfid)
{
	struct qed_llh_info *p_llh_info = cdev->p_llh_info;
	struct qed_llh_filter_info *p_filters;
	int rc;

	rc = qed_llh_shadow_sanity(cdev, ppfid, 0, "remove_all");
	if (rc)
		return rc;

	p_filters = p_llh_info->pp_filters[ppfid];
	memset(p_filters, 0, NIG_REG_LLH_FUNC_FILTER_EN_SIZE *
	       sizeof(*p_filters));

	return 0;
}

int qed_abs_ppfid(struct qed_dev *cdev, u8 rel_ppfid, u8 * p_abs_ppfid)
{
	struct qed_llh_info *p_llh_info = cdev->p_llh_info;

	if (rel_ppfid >= p_llh_info->num_ppfid) {
		DP_NOTICE(cdev,
			  "rel_ppfid %d is not valid, available indices are 0..%hhu\n",
			  rel_ppfid, p_llh_info->num_ppfid - 1);
		return -EINVAL;
	}

	*p_abs_ppfid = p_llh_info->ppfid_array[rel_ppfid];

	return 0;
}

int qed_llh_map_ppfid_to_pfid(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, u8 ppfid, u8 pfid)
{
	u8 abs_ppfid;
	u32 addr;
	int rc;

	rc = qed_abs_ppfid(p_hwfn->cdev, ppfid, &abs_ppfid);
	if (rc)
		return rc;

	addr = NIG_REG_LLH_PPFID2PFID_TBL_0 + abs_ppfid * 0x4;
	qed_wr(p_hwfn, p_ptt, addr, pfid);

	return 0;
}

static int
__qed_llh_set_engine_affin(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	enum qed_eng eng;
	u8 ppfid;
	int rc;

	rc = qed_mcp_get_engine_config(p_hwfn, p_ptt);
	if (rc != 0 && rc != -EOPNOTSUPP) {
		DP_NOTICE(p_hwfn,
			  "Failed to get the engine affinity configuration\n");
		return rc;
	}

	/* RoCE PF is bound to a single engine */
	if (QED_IS_ROCE_PERSONALITY(p_hwfn)) {
		eng = cdev->fir_affin ? QED_ENG1 : QED_ENG0;
		rc = qed_llh_set_roce_affinity(cdev, eng);
		if (rc) {
			DP_NOTICE(cdev,
				  "Failed to set the RoCE engine affinity\n");
			return rc;
		}

		DP_VERBOSE(cdev,
			   QED_MSG_SP,
			   "LLH: Set the engine affinity of RoCE packets as %d\n",
			   eng);
	}

	/* Storage PF is bound to a single engine while L2 PF uses both */
	if (QED_IS_FCOE_PERSONALITY(p_hwfn) || QED_IS_ISCSI_PERSONALITY(p_hwfn))
		eng = cdev->fir_affin ? QED_ENG1 : QED_ENG0;
	else			/* L2_PERSONALITY */
		eng = QED_BOTH_ENG;

	for (ppfid = 0; ppfid < cdev->p_llh_info->num_ppfid; ppfid++) {
		rc = qed_llh_set_ppfid_affinity(cdev, ppfid, eng);
		if (rc) {
			DP_NOTICE(cdev,
				  "Failed to set the engine affinity of ppfid %d\n",
				  ppfid);
			return rc;
		}
	}

	DP_VERBOSE(cdev, QED_MSG_SP,
		   "LLH: Set the engine affinity of non-RoCE packets as %d\n",
		   eng);

	return 0;
}

static int
qed_llh_set_engine_affin(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt, bool avoid_eng_affin)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	int rc;

	/* Backwards compatible mode:
	 * - RoCE packets     - Use engine 0.
	 * - Non-RoCE packets - Use connection based classification for L2 PFs,
	 *                      and engine 0 otherwise.
	 */
	if (avoid_eng_affin) {
		enum qed_eng eng;
		u8 ppfid;

		if (QED_IS_ROCE_PERSONALITY(p_hwfn)) {
			eng = QED_ENG0;
			rc = qed_llh_set_roce_affinity(cdev, eng);
			if (rc) {
				DP_NOTICE(cdev,
					  "Failed to set the RoCE engine affinity\n");
				return rc;
			}

			DP_VERBOSE(cdev,
				   QED_MSG_SP,
				   "LLH [backwards compatible mode]: Set the engine affinity of RoCE packets as %d\n",
				   eng);
		}

		eng = (QED_IS_FCOE_PERSONALITY(p_hwfn) ||
		       QED_IS_ISCSI_PERSONALITY(p_hwfn)) ? QED_ENG0
		    : QED_BOTH_ENG;
		for (ppfid = 0; ppfid < cdev->p_llh_info->num_ppfid; ppfid++) {
			rc = qed_llh_set_ppfid_affinity(cdev, ppfid, eng);
			if (rc) {
				DP_NOTICE(cdev,
					  "Failed to set the engine affinity of ppfid %d\n",
					  ppfid);
				return rc;
			}
		}

		DP_VERBOSE(cdev,
			   QED_MSG_SP,
			   "LLH [backwards compatible mode]: Set the engine affinity of non-RoCE packets as %d\n",
			   eng);

		return 0;
	}

	return __qed_llh_set_engine_affin(p_hwfn, p_ptt);
}

static int qed_llh_hw_init_pf(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, bool avoid_eng_affin)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	u8 ppfid;
	int rc;

	for (ppfid = 0; ppfid < cdev->p_llh_info->num_ppfid; ppfid++) {
		rc = qed_llh_map_ppfid_to_pfid(p_hwfn, p_ptt, ppfid,
					       p_hwfn->rel_pf_id);
		if (rc) {
			DP_NOTICE(cdev,
				  "Failed to map ppfid %d to pfid %d\n",
				  ppfid, p_hwfn->rel_pf_id);
			return rc;
		}
	}

	if (test_bit(QED_MF_LLH_MAC_CLSS, &cdev->mf_bits) &&
	    !QED_IS_FCOE_PERSONALITY(p_hwfn)) {
		rc = qed_llh_add_mac_filter(cdev, 0,
					    p_hwfn->hw_info.hw_mac_addr);
		if (rc)
			DP_NOTICE(cdev,
				  "Failed to add an LLH filter with the primary MAC\n");
	}

	if (QED_IS_CMT(cdev)) {
		rc = qed_llh_set_engine_affin(p_hwfn, p_ptt, avoid_eng_affin);
		if (rc)
			return rc;
	}

	return 0;
}

u8 qed_llh_get_num_ppfid(struct qed_dev * cdev)
{
	return cdev->p_llh_info->num_ppfid;
}

enum qed_eng qed_llh_get_l2_affinity_hint(struct qed_dev *cdev)
{
	return cdev->l2_affin_hint ? QED_ENG1 : QED_ENG0;
}

/* TBD -
 * When the relevant definitions are available in reg_addr.h, the SHIFT
 * definitions should be removed, and the MASK definitions should be revised.
 */
#define NIG_REG_PPF_TO_ENGINE_SEL_ROCE_MASK             0x3
#define NIG_REG_PPF_TO_ENGINE_SEL_ROCE_SHIFT            0
#define NIG_REG_PPF_TO_ENGINE_SEL_NON_ROCE_MASK         0x3
#define NIG_REG_PPF_TO_ENGINE_SEL_NON_ROCE_SHIFT        2

int qed_llh_set_ppfid_affinity(struct qed_dev *cdev, u8 ppfid, enum qed_eng eng)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	u32 addr, val, eng_sel;
	int rc = 0;
	u8 abs_ppfid;

	if (p_ptt == NULL)
		return -EAGAIN;

	if (!QED_IS_CMT(cdev))
		goto out;

	rc = qed_abs_ppfid(cdev, ppfid, &abs_ppfid);
	if (rc)
		goto out;

	switch (eng) {
	case QED_ENG0:
		eng_sel = 0;
		break;
	case QED_ENG1:
		eng_sel = 1;
		break;
	case QED_BOTH_ENG:
		eng_sel = 2;
		break;
	default:
		DP_NOTICE(cdev, "Invalid affinity value for ppfid [%d]\n", eng);
		rc = -EINVAL;
		goto out;
	}

	addr = NIG_REG_PPF_TO_ENGINE_SEL + abs_ppfid * 0x4;
	val = qed_rd(p_hwfn, p_ptt, addr);
	SET_FIELD(val, NIG_REG_PPF_TO_ENGINE_SEL_NON_ROCE, eng_sel);
	qed_wr(p_hwfn, p_ptt, addr, val);

	/* The iWARP affinity is set as the affinity of ppfid 0 */
	if (!ppfid && QED_IS_IWARP_PERSONALITY(p_hwfn))
		cdev->iwarp_affin = (eng == QED_ENG1) ? 1 : 0;
out:
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

int qed_llh_set_roce_affinity(struct qed_dev *cdev, enum qed_eng eng)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	u32 addr, val, eng_sel;
	int rc = 0;
	u8 ppfid, abs_ppfid;

	if (p_ptt == NULL)
		return -EAGAIN;

	if (!QED_IS_CMT(cdev))
		goto out;

	switch (eng) {
	case QED_ENG0:
		eng_sel = 0;
		break;
	case QED_ENG1:
		eng_sel = 1;
		break;
	case QED_BOTH_ENG:
		eng_sel = 2;
		qed_wr(p_hwfn, p_ptt, NIG_REG_LLH_ENG_CLS_ROCE_QP_SEL,
		       0xf /* QP bit 15 */ );
		break;
	default:
		DP_NOTICE(cdev, "Invalid affinity value for RoCE [%d]\n", eng);
		rc = -EINVAL;
		goto out;
	}

	for (ppfid = 0; ppfid < cdev->p_llh_info->num_ppfid; ppfid++) {
		rc = qed_abs_ppfid(cdev, ppfid, &abs_ppfid);
		if (rc)
			goto out;

		addr = NIG_REG_PPF_TO_ENGINE_SEL + abs_ppfid * 0x4;
		val = qed_rd(p_hwfn, p_ptt, addr);
		SET_FIELD(val, NIG_REG_PPF_TO_ENGINE_SEL_ROCE, eng_sel);
		qed_wr(p_hwfn, p_ptt, addr, val);
	}
out:
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

struct qed_llh_filter_details {
	u64 value;
	u32 mode;
	u32 protocol_type;
	u32 hdr_sel;
	u32 enable;
};

static int
qed_llh_access_filter(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt,
		      u8 abs_ppfid,
		      u8 filter_idx,
		      struct qed_llh_filter_details *p_details,
		      bool b_write_access)
{
	u8 pfid = QED_PFID_BY_PPFID(p_hwfn, abs_ppfid);
	struct dmae_params params;
	int rc;
	u32 addr;

	/* The NIG/LLH registers that are accessed in this function have only 16
	 * rows which are exposed to a PF. I.e. only the 16 filters of its
	 * default ppfid
	 * Accessing filters of other ppfids requires pretending to other PFs,
	 * and thus the usage of the qed_ppfid_rd/wr() functions.
	 */

	/* Filter enable - should be done first when removing a filter */
	if (b_write_access && !p_details->enable) {
		addr = NIG_REG_LLH_FUNC_FILTER_EN + filter_idx * 0x4;
		qed_ppfid_wr(p_hwfn, p_ptt, abs_ppfid, addr, p_details->enable);
	}

	/* Filter value */
	addr = NIG_REG_LLH_FUNC_FILTER_VALUE + 2 * filter_idx * 0x4;
	memset(&params, 0, sizeof(params));

	if (b_write_access) {
		SET_FIELD(params.flags, DMAE_PARAMS_DST_PF_VALID, 0x1);
		params.dst_pf_id = pfid;
		rc = qed_dmae_host2grc(p_hwfn,
				       p_ptt,
				       (u64) (uintptr_t) &
				       p_details->value,
				       addr, 2 /* size_in_dwords */ ,
				       &params);
	} else {
		SET_FIELD(params.flags, DMAE_PARAMS_SRC_PF_VALID, 0x1);
		SET_FIELD(params.flags, DMAE_PARAMS_COMPLETION_DST, 0x1);
		params.src_pf_id = pfid;
		rc = qed_dmae_grc2host(p_hwfn,
				       p_ptt,
				       addr,
				       (u64) (uintptr_t) &
				       p_details->value,
				       2 /* size_in_dwords */ ,
				       &params);
	}

	if (rc)
		return rc;

	/* Filter mode */
	addr = NIG_REG_LLH_FUNC_FILTER_MODE + filter_idx * 0x4;
	if (b_write_access)
		qed_ppfid_wr(p_hwfn, p_ptt, abs_ppfid, addr, p_details->mode);
	else
		p_details->mode = qed_ppfid_rd(p_hwfn, p_ptt, abs_ppfid, addr);

	/* Filter protocol type */
	addr = NIG_REG_LLH_FUNC_FILTER_PROTOCOL_TYPE + filter_idx * 0x4;
	if (b_write_access)
		qed_ppfid_wr(p_hwfn, p_ptt, abs_ppfid, addr,
			     p_details->protocol_type);
	else
		p_details->protocol_type = qed_ppfid_rd(p_hwfn, p_ptt,
							abs_ppfid, addr);

	/* Filter header select */
	addr = NIG_REG_LLH_FUNC_FILTER_HDR_SEL + filter_idx * 0x4;
	if (b_write_access)
		qed_ppfid_wr(p_hwfn, p_ptt, abs_ppfid, addr,
			     p_details->hdr_sel);
	else
		p_details->hdr_sel = qed_ppfid_rd(p_hwfn, p_ptt, abs_ppfid,
						  addr);

	/* Filter enable - should be done last when adding a filter */
	if (!b_write_access || p_details->enable) {
		addr = NIG_REG_LLH_FUNC_FILTER_EN + filter_idx * 0x4;
		if (b_write_access)
			qed_ppfid_wr(p_hwfn, p_ptt, abs_ppfid, addr,
				     p_details->enable);
		else
			p_details->enable = qed_ppfid_rd(p_hwfn, p_ptt,
							 abs_ppfid, addr);
	}

	return 0;
}

static int
qed_llh_add_filter(struct qed_hwfn *p_hwfn,
		   struct qed_ptt *p_ptt,
		   u8 abs_ppfid,
		   u8 filter_idx, u8 filter_prot_type, u32 high, u32 low)
{
	struct qed_llh_filter_details filter_details;

	filter_details.enable = 1;
	filter_details.value = ((u64) high << 32) | low;
	filter_details.hdr_sel = test_bit(QED_MF_OVLAN_CLSS, &p_hwfn->cdev->mf_bits) ? 1 :	/* inner/encapsulated header */
	    0;			/* outer/tunnel header */
	filter_details.protocol_type = filter_prot_type;
	filter_details.mode = filter_prot_type ? 1 :	/* protocol-based classification */
	    0;			/* MAC-address based classification */

	return qed_llh_access_filter(p_hwfn, p_ptt, abs_ppfid, filter_idx,
				     &filter_details, true /* write access */ );
}

static int
qed_llh_remove_filter(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, u8 abs_ppfid, u8 filter_idx)
{
	struct qed_llh_filter_details filter_details;

	memset(&filter_details, 0, sizeof(filter_details));

	return qed_llh_access_filter(p_hwfn, p_ptt, abs_ppfid, filter_idx,
				     &filter_details, true /* write access */ );
}

int qed_llh_add_mac_filter(struct qed_dev *cdev,
			   u8 ppfid, const u8 mac_addr[ETH_ALEN])
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	union qed_llh_filter filter;
	u8 filter_idx, abs_ppfid;
	struct qed_ptt *p_ptt;
	u32 high, low, ref_cnt;
	int rc = 0;

	if (!test_bit(QED_MF_LLH_MAC_CLSS, &cdev->mf_bits))
		return rc;

	if (IS_VF(p_hwfn->cdev)) {
		DP_NOTICE(cdev, "Setting MAC to LLH is not supported to VF\n");
		return -EOPNOTSUPP;
	}

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (p_ptt == NULL)
		return -EAGAIN;

	memset(&filter, 0, sizeof(filter));
	memcpy(filter.mac.addr, mac_addr, ETH_ALEN);
	rc = qed_llh_shadow_add_filter(cdev, ppfid,
				       QED_LLH_FILTER_TYPE_MAC,
				       &filter, &filter_idx, &ref_cnt);
	if (rc)
		goto err;

	rc = qed_abs_ppfid(cdev, ppfid, &abs_ppfid);
	if (rc)
		goto err;

	/* Configure the LLH only in case of a new the filter */
	if (ref_cnt == 1) {
		high = mac_addr[1] | (mac_addr[0] << 8);
		low = mac_addr[5] |
		    (mac_addr[4] << 8) | (mac_addr[3] << 16) |
		    (mac_addr[2] << 24);
		rc = qed_llh_add_filter(p_hwfn, p_ptt, abs_ppfid, filter_idx,
					0, high, low);
		if (rc)
			goto err;
	}

	DP_VERBOSE(cdev,
		   QED_MSG_SP,
		   "LLH: Added MAC filter [%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx] to ppfid %hhd [abs %hhd] at idx %hhd [ref_cnt %d]\n",
		   mac_addr[0],
		   mac_addr[1],
		   mac_addr[2],
		   mac_addr[3],
		   mac_addr[4],
		   mac_addr[5], ppfid, abs_ppfid, filter_idx, ref_cnt);

	goto out;

err:	DP_NOTICE(cdev,
		  "LLH: Failed to add MAC filter [%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx] to ppfid %hhd\n",
		  mac_addr[0],
		  mac_addr[1],
		  mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], ppfid);
out:
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

static int
qed_llh_protocol_filter_stringify(struct qed_dev *cdev,
				  enum qed_llh_prot_filter_type_t type,
				  u16
				  source_port_or_eth_type,
				  u16 dest_port, char *str, size_t str_len)
{
	switch (type) {
	case QED_LLH_FILTER_ETHERTYPE:
		scnprintf(str, str_len, "Ethertype 0x%04x",
			  source_port_or_eth_type);
		break;
	case QED_LLH_FILTER_TCP_SRC_PORT:
		scnprintf(str, str_len, "TCP src port 0x%04x",
			  source_port_or_eth_type);
		break;
	case QED_LLH_FILTER_UDP_SRC_PORT:
		scnprintf(str, str_len, "UDP src port 0x%04x",
			  source_port_or_eth_type);
		break;
	case QED_LLH_FILTER_TCP_DEST_PORT:
		scnprintf(str, str_len, "TCP dst port 0x%04x", dest_port);
		break;
	case QED_LLH_FILTER_UDP_DEST_PORT:
		scnprintf(str, str_len, "UDP dst port 0x%04x", dest_port);
		break;
	case QED_LLH_FILTER_TCP_SRC_AND_DEST_PORT:
		scnprintf(str, str_len, "TCP src/dst ports 0x%04x/0x%04x",
			  source_port_or_eth_type, dest_port);
		break;
	case QED_LLH_FILTER_UDP_SRC_AND_DEST_PORT:
		scnprintf(str, str_len, "UDP src/dst ports 0x%04x/0x%04x",
			  source_port_or_eth_type, dest_port);
		break;
	default:
		DP_NOTICE(cdev,
			  "Non valid LLH protocol filter type %d\n", type);
		return -EINVAL;
	}

	return 0;
}

static int
qed_llh_protocol_filter_to_hilo(struct qed_dev *cdev,
				enum qed_llh_prot_filter_type_t type,
				u16
				source_port_or_eth_type,
				u16 dest_port, u32 * p_high, u32 * p_low)
{
	*p_high = 0;
	*p_low = 0;

	switch (type) {
	case QED_LLH_FILTER_ETHERTYPE:
		*p_high = source_port_or_eth_type;
		break;
	case QED_LLH_FILTER_TCP_SRC_PORT:
	case QED_LLH_FILTER_UDP_SRC_PORT:
		*p_low = source_port_or_eth_type << 16;
		break;
	case QED_LLH_FILTER_TCP_DEST_PORT:
	case QED_LLH_FILTER_UDP_DEST_PORT:
		*p_low = dest_port;
		break;
	case QED_LLH_FILTER_TCP_SRC_AND_DEST_PORT:
	case QED_LLH_FILTER_UDP_SRC_AND_DEST_PORT:
		*p_low = (source_port_or_eth_type << 16) | dest_port;
		break;
	default:
		DP_NOTICE(cdev,
			  "Non valid LLH protocol filter type %d\n", type);
		return -EINVAL;
	}

	return 0;
}

int
qed_llh_add_protocol_filter(struct qed_dev *cdev,
			    u8 ppfid,
			    enum qed_llh_prot_filter_type_t type,
			    u16 source_port_or_eth_type, u16 dest_port)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	u8 filter_idx, abs_ppfid, type_bitmap;
	union qed_llh_filter filter;
	u32 high, low, ref_cnt;
	char str[32];
	int rc = 0;

	if (p_ptt == NULL)
		return -EAGAIN;

	if (!test_bit(QED_MF_LLH_PROTO_CLSS, &cdev->mf_bits))
		goto out;

	rc = qed_llh_protocol_filter_stringify(cdev, type,
					       source_port_or_eth_type,
					       dest_port, str, sizeof(str));
	if (rc)
		goto err;

	memset(&filter, 0, sizeof(filter));
	filter.protocol.type = type;
	filter.protocol.source_port_or_eth_type = source_port_or_eth_type;
	filter.protocol.dest_port = dest_port;
	rc = qed_llh_shadow_add_filter(cdev,
				       ppfid,
				       QED_LLH_FILTER_TYPE_PROTOCOL,
				       &filter, &filter_idx, &ref_cnt);
	if (rc)
		goto err;

	rc = qed_abs_ppfid(cdev, ppfid, &abs_ppfid);
	if (rc)
		goto err;

	/* Configure the LLH only in case of a new the filter */
	if (ref_cnt == 1) {
		rc = qed_llh_protocol_filter_to_hilo(cdev, type,
						     source_port_or_eth_type,
						     dest_port, &high, &low);
		if (rc)
			goto err;

		type_bitmap = 0x1 << type;
		rc = qed_llh_add_filter(p_hwfn,
					p_ptt,
					abs_ppfid,
					filter_idx, type_bitmap, high, low);
		if (rc)
			goto err;
	}

	DP_VERBOSE(cdev,
		   QED_MSG_SP,
		   "LLH: Added protocol filter [%s] to ppfid %hhd [abs %hhd] at idx %hhd [ref_cnt %d]\n",
		   str, ppfid, abs_ppfid, filter_idx, ref_cnt);

	goto out;

err:	DP_NOTICE(p_hwfn,
		  "LLH: Failed to add protocol filter [%s] to ppfid %hhd\n",
		  str, ppfid);
out:
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

void qed_llh_remove_mac_filter(struct qed_dev *cdev,
			       u8 ppfid, u8 mac_addr[ETH_ALEN])
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	union qed_llh_filter filter;
	u8 filter_idx, abs_ppfid;
	struct qed_ptt *p_ptt;
	int rc = 0;
	u32 ref_cnt;

	if (!test_bit(QED_MF_LLH_MAC_CLSS, &cdev->mf_bits))
		return;

	if (IS_VF(p_hwfn->cdev)) {
		DP_NOTICE(cdev,
			  "Removing MAC from LLH is not supported to VF\n");
		return;
	}

	p_ptt = qed_ptt_acquire(p_hwfn);

	if (p_ptt == NULL)
		return;

	memset(&filter, 0, sizeof(filter));
	memcpy(filter.mac.addr, mac_addr, ETH_ALEN);
	rc = qed_llh_shadow_remove_filter(cdev, ppfid, &filter, &filter_idx,
					  &ref_cnt);
	if (rc)
		goto err;

	rc = qed_abs_ppfid(cdev, ppfid, &abs_ppfid);
	if (rc)
		goto err;

	/* Remove from the LLH in case the filter is not in use */
	if (!ref_cnt) {
		rc = qed_llh_remove_filter(p_hwfn, p_ptt, abs_ppfid,
					   filter_idx);
		if (rc)
			goto err;
	}

	DP_VERBOSE(cdev,
		   QED_MSG_SP,
		   "LLH: Removed MAC filter [%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx] from ppfid %hhd [abs %hhd] at idx %hhd [ref_cnt %d]\n",
		   mac_addr[0],
		   mac_addr[1],
		   mac_addr[2],
		   mac_addr[3],
		   mac_addr[4],
		   mac_addr[5], ppfid, abs_ppfid, filter_idx, ref_cnt);

	goto out;

err:	DP_NOTICE(cdev,
		  "LLH: Failed to remove MAC filter [%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx] from ppfid %hhd\n",
		  mac_addr[0],
		  mac_addr[1],
		  mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], ppfid);
out:
	qed_ptt_release(p_hwfn, p_ptt);
}

void qed_llh_remove_protocol_filter(struct qed_dev *cdev,
				    u8 ppfid,
				    enum qed_llh_prot_filter_type_t type,
				    u16 source_port_or_eth_type, u16 dest_port)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	u8 filter_idx, abs_ppfid;
	union qed_llh_filter filter;
	int rc = 0;
	char str[32];
	u32 ref_cnt;

	if (p_ptt == NULL)
		return;

	if (!test_bit(QED_MF_LLH_PROTO_CLSS, &cdev->mf_bits))
		goto out;

	rc = qed_llh_protocol_filter_stringify(cdev, type,
					       source_port_or_eth_type,
					       dest_port, str, sizeof(str));
	if (rc)
		goto err;

	memset(&filter, 0, sizeof(filter));
	filter.protocol.type = type;
	filter.protocol.source_port_or_eth_type = source_port_or_eth_type;
	filter.protocol.dest_port = dest_port;
	rc = qed_llh_shadow_remove_filter(cdev,
					  ppfid,
					  &filter, &filter_idx, &ref_cnt);
	if (rc)
		goto err;

	rc = qed_abs_ppfid(cdev, ppfid, &abs_ppfid);
	if (rc)
		goto err;

	/* Remove from the LLH in case the filter is not in use */
	if (!ref_cnt) {
		rc = qed_llh_remove_filter(p_hwfn, p_ptt, abs_ppfid,
					   filter_idx);
		if (rc)
			goto err;
	}

	DP_VERBOSE(cdev,
		   QED_MSG_SP,
		   "LLH: Removed protocol filter [%s] from ppfid %hhd [abs %hhd] at idx %hhd [ref_cnt %d]\n",
		   str, ppfid, abs_ppfid, filter_idx, ref_cnt);

	goto out;

err:	DP_NOTICE(cdev,
		  "LLH: Failed to remove protocol filter [%s] from ppfid %hhd\n",
		  str, ppfid);
out:
	qed_ptt_release(p_hwfn, p_ptt);
}

void qed_llh_clear_ppfid_filters(struct qed_dev *cdev, u8 ppfid)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	u8 filter_idx, abs_ppfid;
	int rc = 0;

	if (p_ptt == NULL)
		return;

	if (!test_bit(QED_MF_LLH_PROTO_CLSS, &cdev->mf_bits) &&
	    !test_bit(QED_MF_LLH_MAC_CLSS, &cdev->mf_bits))
		goto out;

	rc = qed_abs_ppfid(cdev, ppfid, &abs_ppfid);
	if (rc)
		goto out;

	rc = qed_llh_shadow_remove_all_filters(cdev, ppfid);
	if (rc)
		goto out;

	for (filter_idx = 0; filter_idx < NIG_REG_LLH_FUNC_FILTER_EN_SIZE;
	     filter_idx++) {
		rc = qed_llh_remove_filter(p_hwfn, p_ptt,
					   abs_ppfid, filter_idx);
		if (rc)
			goto out;
	}
out:
	qed_ptt_release(p_hwfn, p_ptt);
}

void qed_llh_clear_all_filters(struct qed_dev *cdev)
{
	u8 ppfid;

	if (!test_bit(QED_MF_LLH_PROTO_CLSS, &cdev->mf_bits) &&
	    !test_bit(QED_MF_LLH_MAC_CLSS, &cdev->mf_bits))
		return;

	for (ppfid = 0; ppfid < cdev->p_llh_info->num_ppfid; ppfid++)
		qed_llh_clear_ppfid_filters(cdev, ppfid);
}

int qed_all_ppfids_wr(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, u32 addr, u32 val)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	u8 ppfid, abs_ppfid;
	int rc;

	for (ppfid = 0; ppfid < cdev->p_llh_info->num_ppfid; ppfid++) {
		rc = qed_abs_ppfid(cdev, ppfid, &abs_ppfid);
		if (rc)
			return rc;

		qed_ppfid_wr(p_hwfn, p_ptt, abs_ppfid, addr, val);
	}

	return 0;
}

int qed_llh_dump_ppfid(struct qed_dev *cdev, u8 ppfid)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	struct qed_llh_filter_details filter_details;
	u8 abs_ppfid, filter_idx;
	u32 addr;
	int rc;

	if (!p_ptt)
		return -EAGAIN;

	rc = qed_abs_ppfid(p_hwfn->cdev, ppfid, &abs_ppfid);
	if (rc)
		goto out;

	addr = NIG_REG_PPF_TO_ENGINE_SEL + abs_ppfid * 0x4;
	DP_NOTICE(p_hwfn,
		  "[rel_pf_id %hhd, ppfid={rel %hhd, abs %hhd}, engine_sel 0x%x]\n",
		  p_hwfn->rel_pf_id,
		  ppfid, abs_ppfid, qed_rd(p_hwfn, p_ptt, addr));

	for (filter_idx = 0; filter_idx < NIG_REG_LLH_FUNC_FILTER_EN_SIZE;
	     filter_idx++) {
		memset(&filter_details, 0, sizeof(filter_details));
		rc = qed_llh_access_filter(p_hwfn, p_ptt, abs_ppfid,
					   filter_idx, &filter_details,
					   false /* read access */ );
		if (rc)
			goto out;

		DP_NOTICE(p_hwfn,
			  "filter %2hhd: enable %d, value 0x%016llx, mode %d, protocol_type 0x%x, hdr_sel 0x%x\n",
			  filter_idx,
			  filter_details.enable,
			  filter_details.value,
			  filter_details.mode,
			  filter_details.protocol_type, filter_details.hdr_sel);
	}
out:
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

int qed_llh_dump_all(struct qed_dev *cdev)
{
	u8 ppfid;
	int rc;

	for (ppfid = 0; ppfid < cdev->p_llh_info->num_ppfid; ppfid++) {
		rc = qed_llh_dump_ppfid(cdev, ppfid);
		if (rc)
			return rc;
	}

	return 0;
}

/******************************* NIG LLH - End ********************************/

static u32 qed_hw_bar_size(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, enum BAR_ID bar_id)
{
	u32 bar_reg = (bar_id == BAR_ID_0 ?
		       PGLUE_B_REG_PF_BAR0_SIZE : PGLUE_B_REG_PF_BAR1_SIZE);
	u32 val;

	if (IS_VF(p_hwfn->cdev))
		return qed_vf_hw_bar_size(p_hwfn, bar_id);

	val = qed_rd(p_hwfn, p_ptt, bar_reg);
	if (val)
		return 1 << (val + 15);

	/* The above registers were updated in the past only in CMT mode. Since
	 * they were found to be useful MFW started updating them from 8.7.7.0.
	 * In older MFW versions they are set to 0 which means disabled.
	 */
	if (QED_IS_CMT(p_hwfn->cdev)) {
		DP_INFO(p_hwfn,
			"BAR size not configured. Assuming BAR size of 256kB for GRC and 512kB for DB\n");
		return BAR_ID_0 ? 256 * 1024 : 512 * 1024;
	} else {
		DP_INFO(p_hwfn,
			"BAR size not configured. Assuming BAR size of 512kB for GRC and 512kB for DB\n");
		return 512 * 1024;
	}
}

void qed_init_dp(struct qed_dev *cdev, u32 dp_module, u8 dp_level, void *dp_ctx)
{
	u32 i;

	cdev->dp_level = dp_level;
	cdev->dp_module = dp_module;
	cdev->dp_ctx = dp_ctx;
	for (i = 0; i < MAX_HWFNS_PER_DEVICE; i++) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];

		p_hwfn->dp_level = dp_level;
		p_hwfn->dp_module = dp_module;
		p_hwfn->dp_ctx = dp_ctx;
	}
}

void qed_init_int_dp(struct qed_dev *cdev, u32 dp_module, u8 dp_level)
{
	u32 i;

	cdev->dp_int_level = dp_level;
	cdev->dp_int_module = dp_module;
	for (i = 0; i < MAX_HWFNS_PER_DEVICE; i++) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];

		p_hwfn->dp_int_level = dp_level;
		p_hwfn->dp_int_module = dp_module;
	}
}

void qed_dp_internal_log(struct qed_dev *cdev, char *fmt, ...)
{
	struct qed_timestamp cur_time = { };
	char buff[QED_DP_INT_LOG_MAX_STR_SIZE];
	struct qed_internal_trace *p_int_log;
	u32 len = 0, partial_len;
	unsigned long flags;
	va_list args;
	char *buf = buff;
	u32 prod;

	if (!cdev)
		return;

	p_int_log = &cdev->internal_trace;

	qed_get_current_timestamp(&cur_time);
	if (cur_time.sec || cur_time.rem_usec)
		len += sprintf(&buf[len], "[%5llu.%06lu] ", cur_time.sec,
			       cur_time.rem_usec);

	va_start(args, fmt);
	len += vsnprintf(&buf[len], QED_DP_INT_LOG_MAX_STR_SIZE - len,
			 fmt, args);
	va_end(args);

	if (len > QED_DP_INT_LOG_MAX_STR_SIZE) {
		len = QED_DP_INT_LOG_MAX_STR_SIZE;
		buf[len - 1] = '\n';
	}

	partial_len = len;

	spin_lock_irqsave(&p_int_log->lock, flags);
	if (!p_int_log->buf)
		goto exit;

	prod = p_int_log->prod % p_int_log->size;

	if (p_int_log->size - prod <= len) {
		partial_len = p_int_log->size - prod;
		memcpy(p_int_log->buf + prod, buf, partial_len);
		p_int_log->prod += partial_len;
		prod = p_int_log->prod % p_int_log->size;
		buf += partial_len;
		partial_len = len - partial_len;
	}

	memcpy(p_int_log->buf + prod, buf, partial_len);

	p_int_log->prod += partial_len;

exit:
	spin_unlock_irqrestore(&p_int_log->lock, flags);
}

int qed_init_struct(struct qed_dev *cdev)
{
	u8 i;

	for (i = 0; i < MAX_HWFNS_PER_DEVICE; i++) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];

		p_hwfn->cdev = cdev;
		p_hwfn->my_id = i;
		p_hwfn->b_active = false;
		p_hwfn->p_dummy_cb = qed_int_dummy_comp_cb;

		spin_lock_init(&p_hwfn->dmae_info.lock);
	}

	spin_lock_init(&cdev->internal_trace.lock);

	cdev->cdev = cdev;

	/* hwfn 0 is always active */
	cdev->hwfns[0].b_active = true;

	/* set the default cache alignment to 128 (may be overridden later) */
	cdev->cache_shift = 7;

	cdev->ilt_page_size = QED_DEFAULT_ILT_PAGE_SIZE;

	return 0;
}

static void qed_qm_info_free(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	kfree(qm_info->qm_pq_params);
	qm_info->qm_pq_params = NULL;
	kfree(qm_info->qm_vport_params);
	qm_info->qm_vport_params = NULL;

	if (qm_info->qm_rl_params) {
		kfree(qm_info->qm_rl_params);
		qm_info->qm_rl_params = NULL;
	}

	kfree(qm_info->qm_port_params);
	qm_info->qm_port_params = NULL;
	kfree(qm_info->wfq_data);
	qm_info->wfq_data = NULL;
}

static void qed_dbg_user_data_free(struct qed_hwfn *p_hwfn)
{
	kfree(p_hwfn->dbg_user_info);
	p_hwfn->dbg_user_info = NULL;
}

void qed_resc_free(struct qed_dev *cdev)
{
	int i;

	kfree(cdev->reset_stats);
	cdev->reset_stats = NULL;

	if (IS_PF(cdev)) {
		kfree(cdev->fw_data);
		cdev->fw_data = NULL;

		qed_llh_free(cdev);
	}

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];

		qed_spq_free(p_hwfn);
#ifdef CONFIG_QED_LL2
		qed_ll2_free(p_hwfn);
#endif
		qed_l2_free(p_hwfn);

		if (IS_VF(cdev)) {
			if (QED_IS_RDMA_PERSONALITY(p_hwfn) &&
			    p_hwfn->p_rdma_info)
				qed_rdma_info_free(p_hwfn);

			qed_db_recovery_teardown(p_hwfn);
			continue;
		}

		qed_cxt_mngr_free(p_hwfn);
		qed_qm_info_free(p_hwfn);
		qed_eq_free(p_hwfn);
		qed_consq_free(p_hwfn);
		qed_int_free(p_hwfn);

		if (p_hwfn->hw_info.personality == QED_PCI_FCOE)
			qed_fcoe_free(p_hwfn);

		if (p_hwfn->hw_info.personality == QED_PCI_ISCSI) {
			qed_iscsi_free(p_hwfn);
			qed_ooo_free(p_hwfn);
		}
#if IS_ENABLED(CONFIG_QED_RDMA)
		if (QED_IS_RDMA_PERSONALITY(p_hwfn) && p_hwfn->p_rdma_info) {
			u8 proto = p_hwfn->p_rdma_info->proto;

			qed_spq_unregister_async_cb(p_hwfn, proto);
			qed_rdma_info_free(p_hwfn);
		}
#endif
		qed_spq_unregister_async_cb(p_hwfn, PROTOCOLID_COMMON);
		qed_iov_free(p_hwfn);
		qed_dmae_info_free(p_hwfn);
		qed_dcbx_info_free(p_hwfn);
		qed_dbg_user_data_free(p_hwfn);
		qed_fw_overlay_mem_free(p_hwfn, &p_hwfn->fw_overlay_mem);
		/* @@@TBD Flush work-queue ? */

		/* destroy doorbell recovery mechanism */
		qed_db_recovery_teardown(p_hwfn);
	}

	if (IS_PF(cdev)) {
		kfree(cdev->fw_data);
		cdev->fw_data = NULL;
	}
}

/******************** QM initialization *******************/
/* bitmaps for indicating active traffic classes. Special case for Arrowhead 4 port */
#define ACTIVE_TCS_BMAP 0x9f	/* 0..3 actualy used, 4 serves OOO, 7 serves high priority stuff (e.g. DCQCN) */
#define ACTIVE_TCS_BMAP_4PORT_K2 0xf	/* 0..3 actually used, OOO and high priority stuff all use 3 */

static u16 qed_init_qm_get_num_active_vfs(struct qed_hwfn *p_hwfn)
{
	return IS_QED_SRIOV(p_hwfn->cdev) ?
	    p_hwfn->pf_iov_info->max_active_vfs : 0;
}

/* determines the physical queue flags for a given PF. */
static u32 qed_get_pq_flags(struct qed_hwfn *p_hwfn)
{
	u32 flags;

	/* common flags */
	flags = PQ_FLAGS_LB;

	/* feature flags */
	if (qed_init_qm_get_num_active_vfs(p_hwfn))
		flags |= PQ_FLAGS_VFS;

	if (IS_QED_DCQCN(p_hwfn) || IS_QED_PACING(p_hwfn))
		flags |= PQ_FLAGS_RLS;

	/* protocol flags */
	switch (p_hwfn->hw_info.personality) {
	case QED_PCI_ETH:
		if (!IS_QED_PACING(p_hwfn))
			flags |= PQ_FLAGS_MCOS;
		break;
	case QED_PCI_FCOE:
		flags |= PQ_FLAGS_OFLD;
		break;
	case QED_PCI_ISCSI:
		flags |= PQ_FLAGS_ACK | PQ_FLAGS_OOO | PQ_FLAGS_OFLD;
		break;
	case QED_PCI_ETH_ROCE:
		if (!IS_QED_DCQCN(p_hwfn)) {
			flags |= PQ_FLAGS_OFLD | PQ_FLAGS_LLT;
			if (IS_QED_MULTI_TC_ROCE(p_hwfn))
				flags |= PQ_FLAGS_MTC;
		}

		if (!IS_QED_PACING(p_hwfn))
			flags |= PQ_FLAGS_MCOS;

		if (flags & PQ_FLAGS_VFS)
			flags |=
			    IS_QED_QM_VF_RDMA(p_hwfn) ? PQ_FLAGS_VFR :
			    PQ_FLAGS_VSR;

		break;
	case QED_PCI_ETH_IWARP:
		flags |= PQ_FLAGS_ACK | PQ_FLAGS_OOO | PQ_FLAGS_OFLD;
		if (!IS_QED_PACING(p_hwfn))
			flags |= PQ_FLAGS_MCOS;
		if (flags & PQ_FLAGS_VFS)
			flags |=
			    IS_QED_QM_VF_RDMA(p_hwfn) ? PQ_FLAGS_VFR :
			    PQ_FLAGS_VSR;
		break;
	default:
		DP_ERR(p_hwfn,
		       "unknown personality %d\n", p_hwfn->hw_info.personality);
		return 0;
	}

	if (QED_PQSET_SUPPORTED(p_hwfn)) {
		/* Following PQ assignments will be handled by the qed
		 * In current version MTC RoCE not supported.
		 */
		u32 pqset_reset_flags = PQ_FLAGS_VFS | PQ_FLAGS_VFR |
		    PQ_FLAGS_VSR | PQ_FLAGS_MTC | PQ_FLAGS_RLS;

		/* Remove these from config flags. */
		flags = flags & (~pqset_reset_flags);

		flags |= PQ_FLAGS_PQSET;
	}

	return flags;
}

/* Getters for resource amounts necessary for qm initialization */
u8 qed_init_qm_get_num_tcs(struct qed_hwfn * p_hwfn)
{
	return p_hwfn->hw_info.num_hw_tc;
}

u16 qed_init_qm_get_num_vfs(struct qed_hwfn * p_hwfn)
{
	return IS_QED_SRIOV(p_hwfn->cdev) ? p_hwfn->cdev->p_iov_info->total_vfs
	    : 0;
}

static u16 qed_init_qm_get_num_vfs_pqs(struct qed_hwfn *p_hwfn)
{
	u16 num_pqs, num_vfs = qed_init_qm_get_num_active_vfs(p_hwfn);
	u32 pq_flags = qed_get_pq_flags(p_hwfn);

	/* One L2 PQ per VF */
	num_pqs = num_vfs;

	/* Separate RDMA PQ per VF */
	if ((PQ_FLAGS_VFR & pq_flags))
		num_pqs += num_vfs;

	/* Separate RDMA PQ for all VFs */
	if ((PQ_FLAGS_VSR & pq_flags))
		num_pqs += 1;

	return num_pqs;
}

static u16 qed_init_qm_get_num_pqset_pqs(struct qed_hwfn *p_hwfn)
{
	return p_hwfn->qm_info.num_pqset * p_hwfn->qm_info.num_pqs_per_pqset;
}

static u16 qed_init_qm_get_num_pqset_vports(struct qed_hwfn *p_hwfn)
{
	u8 max_tc = qed_init_qm_get_num_tcs(p_hwfn);
	u16 num_vports;

	if (!p_hwfn->qm_info.num_pqset)
		return 0;

	num_vports = p_hwfn->qm_info.num_pqset * max_tc;

	if (p_hwfn->qm_info.start_pqset_num)
		num_vports += max_tc;
	else
		num_vports++;

	return num_vports;
}

static bool qed_lag_support(struct qed_hwfn *p_hwfn)
{
	return QED_IS_AH(p_hwfn->cdev) &&
	    QED_IS_ROCE_PERSONALITY(p_hwfn) &&
	    test_bit(QED_MF_ROCE_LAG, &p_hwfn->cdev->mf_bits);
}

static u8 qed_init_qm_get_num_mtc_tcs(struct qed_hwfn *p_hwfn)
{
	u32 pq_flags = qed_get_pq_flags(p_hwfn);

	if (!(PQ_FLAGS_MTC & pq_flags))
		return 1;

	return qed_init_qm_get_num_tcs(p_hwfn);
}

static u8 qed_init_qm_get_num_mtc_pqs(struct qed_hwfn *p_hwfn)
{
	u32 num_ports, num_tcs;

	num_ports = qed_lag_support(p_hwfn) ? LAG_MAX_PORT_NUM : 1;
	num_tcs = qed_init_qm_get_num_mtc_tcs(p_hwfn);

	return num_ports * num_tcs;
}

#define NUM_DEFAULT_RLS 1

static u16 qed_init_qm_get_num_pqset_rls(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u16 num_pf_rls;

	if (!p_hwfn->qm_info.num_pqset)
		return 0;

	num_pf_rls = NUM_DEFAULT_RLS +
	    (qm_info->num_pqset + qm_info->start_pqset_num);
	if (num_pf_rls > RESC_NUM(p_hwfn, QED_RL)) {
		DP_NOTICE(p_hwfn,
			  "required rate limiters (%d) exceeds available resource (%d)\n",
			  num_pf_rls, RESC_NUM(p_hwfn, QED_RL));
		num_pf_rls = (u16) RESC_NUM(p_hwfn, QED_RL);
	}

	return num_pf_rls;
}

u16 qed_init_qm_get_num_pf_rls(struct qed_hwfn * p_hwfn)
{
	u16 num_pf_rls, num_vfs = qed_init_qm_get_num_vfs(p_hwfn);

	/* num RLs can't exceed resource amount of rls or vports or the dcqcn qps */
	num_pf_rls =
	    (u16) min_t(u32,
			RESC_NUM(p_hwfn, QED_RL), RESC_NUM(p_hwfn, QED_VPORT));

	/* make sure after we reserve the default and VF rls we'll have something left */
	if (num_pf_rls < num_vfs + NUM_DEFAULT_RLS) {
		if (IS_QED_DCQCN(p_hwfn))
			DP_NOTICE(p_hwfn,
				  "no rate limiters left for PF rate limiting [num_pf_rls %d num_vfs %d]\n",
				  num_pf_rls, num_vfs);
		return 0;
	}

	/* subtract rls necessary for VFs and one default one for the PF */
	num_pf_rls -= num_vfs + NUM_DEFAULT_RLS;

	/* in dcqcn FW requires num of RLs used to be a power of 2 */
	if (IS_QED_DCQCN(p_hwfn))
		num_pf_rls = 1 << ilog2(num_pf_rls);

	return num_pf_rls;
}

static u16 qed_init_qm_get_num_rls(struct qed_hwfn *p_hwfn)
{
	u32 pq_flags = qed_get_pq_flags(p_hwfn);
	u16 num_rls = 0;

	num_rls += (! !(PQ_FLAGS_RLS & pq_flags)) *
	    qed_init_qm_get_num_pf_rls(p_hwfn);

	/* RL for each VF L2 PQ */
	num_rls += (! !(PQ_FLAGS_VFS & pq_flags)) *
	    qed_init_qm_get_num_active_vfs(p_hwfn);

	/* RL for each VF RDMA PQ */
	num_rls += (! !(PQ_FLAGS_VFR & pq_flags)) *
	    qed_init_qm_get_num_active_vfs(p_hwfn);

	/* RL for VF RDMA single PQ */
	num_rls += (! !(PQ_FLAGS_VSR & pq_flags));

	/* RL for each PQ Set */
	num_rls += (! !(PQ_FLAGS_PQSET & pq_flags)) *
	    qed_init_qm_get_num_pqset_rls(p_hwfn);

	return num_rls;
}

u16 qed_init_qm_get_num_vports(struct qed_hwfn * p_hwfn)
{
	u32 pq_flags = qed_get_pq_flags(p_hwfn);
	u16 num_vports;

	/* all pqs share the same vport (hence the 1), except for vfs,
	 * pf_rl pqs and pq_sets
	 */
	num_vports = 1 +
	    ((! !(PQ_FLAGS_RLS & pq_flags)) *
	     qed_init_qm_get_num_pf_rls(p_hwfn)) +
	    ((! !(PQ_FLAGS_VFS & pq_flags)) *
	     qed_init_qm_get_num_vfs(p_hwfn)) +
	    ((! !(PQ_FLAGS_PQSET & pq_flags)) *
	     qed_init_qm_get_num_pqset_vports(p_hwfn));

	return num_vports;
}

static u8 qed_init_qm_get_group_count(struct qed_hwfn *p_hwfn)
{
	return p_hwfn->qm_info.offload_group_count;
}

/* calc amount of PQs according to the requested flags */
u16 qed_init_qm_get_num_pqs(struct qed_hwfn * p_hwfn)
{
	u32 pq_flags = qed_get_pq_flags(p_hwfn);
	u16 pqs_num;

	pqs_num =
	    ((! !(PQ_FLAGS_MCOS & pq_flags)) *
	     qed_init_qm_get_num_tcs(p_hwfn)) +
	    (! !(PQ_FLAGS_LB & pq_flags)) +
	    (! !(PQ_FLAGS_OOO & pq_flags)) +
	    (! !(PQ_FLAGS_ACK & pq_flags)) +
	    ((! !(PQ_FLAGS_OFLD & pq_flags)) *
	     qed_init_qm_get_num_mtc_pqs(p_hwfn)) +
	    ((! !(PQ_FLAGS_LLT & pq_flags)) *
	     qed_init_qm_get_num_mtc_pqs(p_hwfn)) +
	    ((! !(PQ_FLAGS_RLS & pq_flags)) *
	     qed_init_qm_get_num_pf_rls(p_hwfn)) +
	    ((! !(PQ_FLAGS_GRP & pq_flags)) * OFLD_GRP_SIZE) +
	    ((! !(PQ_FLAGS_VFS & pq_flags)) *
	     qed_init_qm_get_num_vfs_pqs(p_hwfn)) +
	    ((! !(PQ_FLAGS_PQSET & pq_flags)) *
	     qed_init_qm_get_num_pqset_pqs(p_hwfn));

	return pqs_num;
}

/* initialize the top level QM params */
static void qed_init_qm_params(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	bool four_port;

	/* pq and vport bases for this PF */
	qm_info->start_pq = (u16) RESC_START(p_hwfn, QED_PQ);
	/* we need at the max, one vport per PQ */
	qm_info->start_vport = (u16) RESC_START(p_hwfn, QED_PQ);
	qm_info->start_rl = (u16) RESC_START(p_hwfn, QED_RL);

	/* rate limiting and weighted fair queueing are always enabled */
	qm_info->vport_rl_en = 1;
	qm_info->vport_wfq_en = 1;

	/* TC config is different for AH 4 port */
	four_port = p_hwfn->cdev->num_ports_in_engine == MAX_NUM_PORTS_K2;

	/* in AH 4 port we have fewer TCs per port */
	qm_info->max_phys_tcs_per_port =
	    four_port ? NUM_PHYS_TCS_4PORT_K2 : NUM_OF_PHYS_TCS;

	/* unless MFW indicated otherwise, ooo_tc should be 3 for AH 4 port and 4 otherwise */
	if (!qm_info->ooo_tc)
		qm_info->ooo_tc =
		    four_port ? DCBX_TCP_OOO_K2_4PORT_TC : DCBX_TCP_OOO_TC;
}

/* initialize qm vport params */
static void qed_init_qm_vport_params(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u16 i;

	/* all vports participate in weighted fair queueing */
	for (i = 0; i < qed_init_qm_get_num_vports(p_hwfn); i++)
		qm_info->qm_vport_params[i].wfq = 1;
}

/* initialize qm port params */
static void qed_init_qm_port_params(struct qed_hwfn *p_hwfn)
{
	/* Initialize qm port parameters */
	u8 i, active_phys_tcs, num_ports = p_hwfn->cdev->num_ports_in_engine;
	struct qed_dev *cdev = p_hwfn->cdev;

	/* indicate how ooo and high pri traffic is dealt with */
	active_phys_tcs = num_ports == MAX_NUM_PORTS_K2 ?
	    ACTIVE_TCS_BMAP_4PORT_K2 : ACTIVE_TCS_BMAP;

	for (i = 0; i < num_ports; i++) {
		struct init_qm_port_params *p_qm_port =
		    &p_hwfn->qm_info.qm_port_params[i];
		u16 pbf_max_cmd_lines;

		p_qm_port->active = 1;
		p_qm_port->active_phys_tcs = active_phys_tcs;
		pbf_max_cmd_lines = (u16) NUM_OF_PBF_CMD_LINES(cdev);
		p_qm_port->num_pbf_cmd_lines = pbf_max_cmd_lines / num_ports;
		p_qm_port->num_btb_blocks = NUM_OF_BTB_BLOCKS(cdev) / num_ports;
	}
}

/* Reset the params which must be reset for qm init. QM init may be called as
 * a result of flows other than driver load (e.g. dcbx renegotiation). Other
 * params may be affected by the init but would simply recalculate to the same
 * values. The allocations made for QM init, ports, vports, pqs and vfqs are not
 * affected as these amounts stay the same.
 */
static void qed_init_qm_reset_params(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	qm_info->num_pqs = 0;
	qm_info->num_vports = 0;
	qm_info->num_rls = 0;
	qm_info->num_pf_rls = 0;
	qm_info->num_vf_pqs = 0;
	qm_info->first_vf_pq = 0;
	qm_info->first_mcos_pq = 0;
	qm_info->first_rl_pq = 0;
	qm_info->single_vf_rdma_pq = 0;
	qm_info->pq_overflow = false;
}

static void qed_init_qm_advance_vport(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	if (qm_info->num_vports >= qed_init_qm_get_num_vports(p_hwfn)) {
		qm_info->pq_overflow = true;
		DP_NOTICE(p_hwfn,
			  "vport overflow! qm_info->num_vports %d qm_init_get_num_vports() %d\n",
			  qm_info->num_vports,
			  qed_init_qm_get_num_vports(p_hwfn));
	}

	qm_info->num_vports++;
}

static void qed_init_qm_advance_rl(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	if (qm_info->num_rls >= RESC_NUM(p_hwfn, QED_RL)) {
		qm_info->pq_overflow = true;
		DP_NOTICE(p_hwfn,
			  "RLs overflow! qm_info->num_rls %d, res_num RL %d\n",
			  qm_info->num_rls, RESC_NUM(p_hwfn, QED_RL));
	}

	qm_info->num_rls++;
}

/* initialize a single pq and manage qm_info resources accounting.
 * The pq_init_flags param determines whether the PQ is rate limited (for VF or PF)
 * and whether a new vport is allocated to the pq or not (i.e. vport will be shared)
 */

/* flags for pq init */
#define PQ_INIT_SHARE_VPORT     (1 << 0)
#define PQ_INIT_PF_RL           (1 << 1)
#define PQ_INIT_VF_RL           (1 << 2)

/* defines for pq init */
#define PQ_INIT_DEFAULT_WRR_GROUP       1
#define PQ_INIT_DEFAULT_TC              0

void qed_hw_info_set_offload_tc(struct qed_hw_info *p_info, u8 tc)
{
	p_info->offload_tc = tc;
	p_info->offload_tc_set = true;
}

static bool qed_is_offload_tc_set(struct qed_hwfn *p_hwfn)
{
	return p_hwfn->hw_info.offload_tc_set;
}

u8 qed_get_offload_tc(struct qed_hwfn * p_hwfn)
{
	if (qed_is_offload_tc_set(p_hwfn))
		return p_hwfn->hw_info.offload_tc;

	return PQ_INIT_DEFAULT_TC;
}

static void qed_init_qm_pq_port(struct qed_hwfn *p_hwfn,
				struct qed_qm_info *qm_info,
				u8 tc, u32 pq_init_flags, u8 port)
{
	u16 pq_idx = qm_info->num_pqs, max_pq = qed_init_qm_get_num_pqs(p_hwfn);
	u16 num_pf_pqs;

	if (pq_idx >= max_pq) {
		qm_info->pq_overflow = true;
		DP_ERR(p_hwfn,
		       "pq overflow! pq %d, max pq %d\n", pq_idx, max_pq);
	}

	/* init pq params */
	qm_info->qm_pq_params[pq_idx].port_id = port;
	qm_info->qm_pq_params[pq_idx].vport_id = qm_info->start_vport +
	    qm_info->num_vports;
	qm_info->qm_pq_params[pq_idx].tc_id = tc;
	qm_info->qm_pq_params[pq_idx].wrr_group = PQ_INIT_DEFAULT_WRR_GROUP;

	if (pq_init_flags & (PQ_INIT_PF_RL | PQ_INIT_VF_RL)) {
		qm_info->qm_pq_params[pq_idx].rl_valid = 1;
		qm_info->qm_pq_params[pq_idx].rl_id =
		    qm_info->start_rl + qm_info->num_rls++;
	}

	/* qm params accounting */
	qm_info->num_pqs++;
	if (pq_init_flags & PQ_INIT_VF_RL) {
		qm_info->num_vf_pqs++;
	} else {
		num_pf_pqs = qm_info->num_pqs - qm_info->num_vf_pqs;
		if (qm_info->ilt_pf_pqs && num_pf_pqs > qm_info->ilt_pf_pqs) {
			qm_info->pq_overflow = true;
			DP_ERR(p_hwfn,
			       "ilt overflow! num_pf_pqs %d, qm_info->ilt_pf_pqs %d\n",
			       num_pf_pqs, qm_info->ilt_pf_pqs);
		}
	}

	if (!(pq_init_flags & PQ_INIT_SHARE_VPORT))
		qm_info->num_vports++;

	if (pq_init_flags & PQ_INIT_PF_RL)
		qm_info->num_pf_rls++;

	if (qm_info->num_vports > qed_init_qm_get_num_vports(p_hwfn)) {
		qm_info->pq_overflow = true;
		DP_NOTICE(p_hwfn,
			  "vport overflow! qm_info->num_vports %d, qm_init_get_num_vports() %d\n",
			  qm_info->num_vports,
			  qed_init_qm_get_num_vports(p_hwfn));
	}

	if (qm_info->num_pf_rls > qed_init_qm_get_num_pf_rls(p_hwfn)) {
		qm_info->pq_overflow = true;
		DP_NOTICE(p_hwfn,
			  "rl overflow! qm_info->num_pf_rls %d, qm_init_get_num_pf_rls() %d\n",
			  qm_info->num_pf_rls,
			  qed_init_qm_get_num_pf_rls(p_hwfn));
	}
}

/* init one qm pq, assume port of the PF */
static void qed_init_qm_pq(struct qed_hwfn *p_hwfn,
			   struct qed_qm_info *qm_info,
			   u8 tc, u32 pq_init_flags)
{
	qed_init_qm_pq_port(p_hwfn, qm_info, tc, pq_init_flags,
			    p_hwfn->port_id);
}

/* get pq index according to PQ_FLAGS */
static u16 *qed_init_qm_get_idx_from_flags(struct qed_hwfn *p_hwfn,
					   unsigned long pq_flags)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	/* Can't have multiple flags set here */
	if (bitmap_weight(&pq_flags, sizeof(pq_flags) * BITS_PER_BYTE) > 1) {
		DP_ERR(p_hwfn, "requested multiple pq flags 0x%lx\n", pq_flags);
		goto err;
	}

	if (!(qed_get_pq_flags(p_hwfn) & pq_flags)) {
		DP_ERR(p_hwfn, "pq flag 0x%lx is not set\n", pq_flags);
		goto err;
	}

	switch (pq_flags) {
	case PQ_FLAGS_RLS:
		return &qm_info->first_rl_pq;
	case PQ_FLAGS_MCOS:
		return &qm_info->first_mcos_pq;
	case PQ_FLAGS_LB:
		return &qm_info->pure_lb_pq;
	case PQ_FLAGS_OOO:
		return &qm_info->ooo_pq;
	case PQ_FLAGS_ACK:
		return &qm_info->pure_ack_pq;
	case PQ_FLAGS_OFLD:
		return &qm_info->first_ofld_pq;
	case PQ_FLAGS_LLT:
		return &qm_info->first_llt_pq;
	case PQ_FLAGS_VFS:
		return &qm_info->first_vf_pq;
	case PQ_FLAGS_GRP:
		return &qm_info->first_ofld_grp_pq;
	case PQ_FLAGS_VSR:
		return &qm_info->single_vf_rdma_pq;
	default:
		goto err;
	}
err:
	return &qm_info->start_pq;
}

/* save pq index in qm info */
static void qed_init_qm_set_idx(struct qed_hwfn *p_hwfn,
				u32 pq_flags, u16 pq_val)
{
	u16 *base_pq_idx = qed_init_qm_get_idx_from_flags(p_hwfn, pq_flags);

	*base_pq_idx = p_hwfn->qm_info.start_pq + pq_val;
}

static void qed_init_qm_pqset_idx(struct qed_hwfn *p_hwfn, u16 pq_val)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	qm_info->first_pqset_pq = qm_info->start_pq + pq_val;
}

/* get 1st pq of PQ set */
static u16 qed_get_pqset_base(struct qed_hwfn *p_hwfn, u16 pq_set)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u16 pq_set_offset = 0, pq_id;

	if (QED_IS_SHARED_DEFAULT_PQSET(p_hwfn, pq_set)) {
		pq_id = qm_info->first_mcos_pq;
	} else {
		pq_set_offset = pq_set - qm_info->start_pqset_num;
		pq_id = qm_info->first_pqset_pq +
		    pq_set_offset * qm_info->num_pqs_per_pqset;
	}

	return pq_id;
}

static u16 qed_qm_get_start_pq(struct qed_hwfn *p_hwfn)
{
	u16 start_pq;

	spin_lock_bh(&p_hwfn->qm_info.qm_info_lock);
	start_pq = p_hwfn->qm_info.start_pq;
	spin_unlock_bh(&p_hwfn->qm_info.qm_info_lock);

	return start_pq;
}

/* get tx pq index, with the PQ TX base already set (ready for context init) */
u16 qed_get_cm_pq_idx(struct qed_hwfn * p_hwfn, u32 pq_flags)
{
	u16 *base_pq_idx;
	u16 pq_idx;

	spin_lock_bh(&p_hwfn->qm_info.qm_info_lock);
	base_pq_idx = qed_init_qm_get_idx_from_flags(p_hwfn, pq_flags);
	pq_idx = *base_pq_idx + CM_TX_PQ_BASE;
	spin_unlock_bh(&p_hwfn->qm_info.qm_info_lock);

	return pq_idx;
}

u16 qed_get_cm_pq_idx_grp(struct qed_hwfn * p_hwfn, u8 idx)
{
	u16 pq_idx = qed_get_cm_pq_idx(p_hwfn, PQ_FLAGS_GRP);
	u8 max_idx = qed_init_qm_get_group_count(p_hwfn);

	if (max_idx == 0) {
		DP_ERR(p_hwfn, "pq with flag 0x%x do not exist\n",
		       PQ_FLAGS_GRP);
		return qed_qm_get_start_pq(p_hwfn);
	}

	if (idx > max_idx)
		DP_ERR(p_hwfn, "idx %d must be smaller than %d\n",
		       idx, max_idx);

	return pq_idx + (idx % max_idx);
}

static u16 qed_pqset_get_mcos_pq_idx(struct qed_hwfn *p_hwfn, u16 pq_set, u8 tc)
{
	u16 pqset_base = qed_get_pqset_base(p_hwfn, pq_set);

	return pqset_base + tc;
}

static u16 qed_pqset_get_ofld_pq_idx(struct qed_hwfn *p_hwfn, u16 pq_set, u8 tc)
{
	u16 pqset_base = qed_get_pqset_base(p_hwfn,
					    pq_set);
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 max_tc = qed_init_qm_get_num_tcs(p_hwfn);
	u16 pq_id = QM_INVALID_PQ_ID;

	if (QED_IS_SHARED_DEFAULT_PQSET(p_hwfn, pq_set)) {
		pq_id = qm_info->first_ofld_pq;
		if (qed_init_qm_get_num_mtc_tcs(p_hwfn) != 1)
			pq_id += tc;
	} else {
		switch (qm_info->offload_type) {
		case QED_OFFLOAD_SHARED_PQ_SET:
			/* ofld shared with mcos PQs */
			pq_id = pqset_base + tc;
			break;
		case QED_OFFLOAD_DEDICATED_PQ_SET:
			/* offld PQ(s) after pq set MCOS PQs */
			pq_id = pqset_base + max_tc;
			if (p_hwfn->qm_info.pqset_num_offload_pqs == max_tc)
				pq_id += tc;
			break;
		default:
			break;
		}
	}

	return pq_id;
}

static u16 qed_pqset_get_aux_pq_idx(struct qed_hwfn *p_hwfn,
				    u32 pq_flags, u16 pq_set, u8 tc)
{
	u16 pqset_base = qed_get_pqset_base(p_hwfn,
					    pq_set);
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 max_tc = qed_init_qm_get_num_tcs(p_hwfn);
	u16 pq_id = QM_INVALID_PQ_ID;

	if (!(pq_flags & (PQ_FLAGS_ACK | PQ_FLAGS_LLT))) {
		DP_NOTICE(p_hwfn,
			  "aux pq must be LLT or ACK! pq_flags 0x%x\n",
			  pq_flags);

		return pq_id;
	}

	if (QED_IS_SHARED_DEFAULT_PQSET(p_hwfn, pq_set)) {
		/* get default PQs */
		if (pq_flags & PQ_FLAGS_ACK) {
			pq_id = qm_info->pure_ack_pq;
		} else if (pq_flags & PQ_FLAGS_LLT) {
			pq_id = qm_info->first_llt_pq;
			if (qed_init_qm_get_num_mtc_tcs(p_hwfn) != 1)
				pq_id += tc;
		}

		return pq_id;
	}

	switch (qm_info->aux_offload_type) {
	case QED_AUX_OFFLOAD_SHARED_DEFAULT:
		if (pq_flags & PQ_FLAGS_ACK) {
			pq_id = qm_info->pure_ack_pq;
		} else if (pq_flags & PQ_FLAGS_LLT) {
			pq_id = qm_info->first_llt_pq;
			if (qed_init_qm_get_num_mtc_tcs(p_hwfn) != 1)
				pq_id += tc;
		}

		break;
	case QED_AUX_OFFLOAD_SHARED_PQ_SET:
		pq_id = pqset_base;
		if (qm_info->offload_type == QED_OFFLOAD_DEDICATED_PQ_SET) {
			/* AUX PQ(s) shared with ofld PQ(s) */
			pq_id += max_tc;
			if (qm_info->pqset_num_offload_pqs == max_tc)
				pq_id += tc;
		} else {
			/* AUX & OFLD PQ(s) shared with MCOS PQ(s) */
			pq_id += tc;
		}

		break;
	case QED_AUX_OFFLOAD_DEDICATED_PQ_SET:
		pq_id = pqset_base;
		if (qm_info->offload_type == QED_OFFLOAD_DEDICATED_PQ_SET)
			/* AUX PQ(s) after OFLD PQ(s) */
			pq_id += max_tc + qm_info->pqset_num_offload_pqs;
		else
			/* AUX PQ(s) after MCOS PQ(s) which are shared with
			 * OFLD
			 */
			pq_id += max_tc;

		if (qm_info->pqset_num_aux_pqs == max_tc)
			pq_id += tc;

		break;
	default:
		break;
	}

	return pq_id;
}

static u16 qed_pqset_get_pq_idx(struct qed_hwfn *p_hwfn,
				u32 pq_flags, u16 pq_set, u8 tc)
{
	u16 pq_id = QM_INVALID_PQ_ID;

	if (pq_flags & PQ_FLAGS_MCOS)
		pq_id = qed_pqset_get_mcos_pq_idx(p_hwfn, pq_set, tc);
	else if (pq_flags & PQ_FLAGS_OFLD)
		pq_id = qed_pqset_get_ofld_pq_idx(p_hwfn, pq_set, tc);
	else if (pq_flags & (PQ_FLAGS_ACK | PQ_FLAGS_LLT))
		pq_id = qed_pqset_get_aux_pq_idx(p_hwfn, pq_flags, pq_set, tc);
	else
		DP_NOTICE(p_hwfn, "PQ flag 0x%x is not supported\n", pq_flags);

	return pq_id;
}

u16 qed_get_pq_idx(struct qed_hwfn * p_hwfn, u32 pq_flags, u16 pq_set, u8 tc)
{
	u8 max_tc = qed_init_qm_get_num_tcs(p_hwfn);
	u16 pq_id = QM_INVALID_PQ_ID;
	u16 *p_pq_id = NULL;

	if (max_tc == 0) {
		DP_ERR(p_hwfn, "pq with flag 0x%x do not exist\n",
		       PQ_FLAGS_MCOS);
		return qed_qm_get_start_pq(p_hwfn);
	}

	if (tc > max_tc)
		DP_NOTICE(p_hwfn, "tc %d must be smaller than %d\n",
			  tc, max_tc);

	if (QED_PQSET_SUPPORTED(p_hwfn))
		return qed_pqset_get_pq_idx(p_hwfn, pq_flags, pq_set, tc);

	p_pq_id = qed_init_qm_get_idx_from_flags(p_hwfn, pq_flags);

	if (p_pq_id != NULL) {
		pq_id = *p_pq_id;
		if (pq_flags & PQ_FLAGS_MCOS)
			pq_id += tc;
	}

	return pq_id;
}

u16 qed_get_cm_pq_idx_tc(struct qed_hwfn * p_hwfn,
			 u32 pq_flags, u16 pq_set, u8 tc)
{
	u16 pq_id = qed_get_pq_idx(p_hwfn, pq_flags, pq_set, tc);

	return pq_id + CM_TX_PQ_BASE;
}

static u8 qed_qm_get_pqs_per_vf(struct qed_hwfn *p_hwfn)
{
	u8 pqs_per_vf;
	u32 pq_flags;

	/* When VFR is set, there is pair of PQs per VF. If VSR is set,
	 * no additional action required in computing the per VF PQ.
	 */
	spin_lock_bh(&p_hwfn->qm_info.qm_info_lock);
	pq_flags = qed_get_pq_flags(p_hwfn);
	pqs_per_vf = (PQ_FLAGS_VFR & pq_flags) ? 2 : 1;
	spin_unlock_bh(&p_hwfn->qm_info.qm_info_lock);

	return pqs_per_vf;
}

u16 qed_get_cm_pq_idx_vf(struct qed_hwfn * p_hwfn, u16 vf, u16 pq_set_id, u8 tc)
{
	u16 max_vf, pq_idx;
	u8 pqs_per_vf;

	if (QED_PQSET_SUPPORTED(p_hwfn))
		return qed_get_cm_pq_idx_tc(p_hwfn, PQ_FLAGS_MCOS,
					    pq_set_id, tc);

	max_vf = qed_init_qm_get_num_active_vfs(p_hwfn);
	if (max_vf == 0) {
		DP_ERR(p_hwfn, "pq with flag 0x%x do not exist\n",
		       PQ_FLAGS_VFS);
		return qed_qm_get_start_pq(p_hwfn);
	}

	if (vf > max_vf)
		DP_ERR(p_hwfn, "vf %d must be smaller than %d\n", vf, max_vf);

	pq_idx = qed_get_cm_pq_idx(p_hwfn, PQ_FLAGS_VFS);
	pqs_per_vf = qed_qm_get_pqs_per_vf(p_hwfn);

	return pq_idx + ((vf % max_vf) * pqs_per_vf);
}

u16 qed_get_cm_pq_idx_vf_ll2(struct qed_hwfn * p_hwfn,
			     u16 vf, u16 pq_set_id, u8 tc)
{
	if (QED_PQSET_SUPPORTED(p_hwfn))
		return qed_get_cm_pq_idx_tc(p_hwfn, PQ_FLAGS_OFLD,
					    pq_set_id, tc);

	return qed_get_cm_pq_idx_vf(p_hwfn, vf, 0, 0);
}

u16 qed_get_cm_pq_idx_vf_rdma(struct qed_hwfn * p_hwfn,
			      u16 vf, u8 tc, u16 pq_set_id)
{
	u32 pq_flags;
	u16 pq_idx;

	spin_lock_bh(&p_hwfn->qm_info.qm_info_lock);
	pq_flags = qed_get_pq_flags(p_hwfn);
	spin_unlock_bh(&p_hwfn->qm_info.qm_info_lock);

	if (QED_PQSET_SUPPORTED(p_hwfn))
		return qed_get_cm_pq_idx_tc(p_hwfn, PQ_FLAGS_OFLD,
					    pq_set_id, tc);

	/* If VSR is set, dedicated single PQ for VFs RDMA */
	if (PQ_FLAGS_VSR & pq_flags)
		pq_idx = qed_get_cm_pq_idx(p_hwfn, PQ_FLAGS_VSR);
	else
		/* pq_set_id and tc are not in use */
		pq_idx = qed_get_cm_pq_idx_vf(p_hwfn, vf, 0, 0);

	/* If VFR is set, VF's 2nd PQ is for RDMA */
	if ((PQ_FLAGS_VFR & pq_flags))
		pq_idx++;

	return pq_idx;
}

u16 qed_get_cm_pq_idx_vf_rdma_llt(struct qed_hwfn * p_hwfn,
				  u16 vf, u8 tc, u16 pq_set_id)
{
	if (QED_PQSET_SUPPORTED(p_hwfn))
		return qed_get_cm_pq_idx_tc(p_hwfn, PQ_FLAGS_LLT,
					    pq_set_id, tc);

	return qed_get_cm_pq_idx_vf_rdma(p_hwfn, vf, tc, pq_set_id);
}

u16 qed_get_cm_pq_idx_rl(struct qed_hwfn * p_hwfn, u16 rl)
{
	u16 pq_idx = qed_get_cm_pq_idx(p_hwfn, PQ_FLAGS_RLS);
	u16 max_rl = qed_init_qm_get_num_pf_rls(p_hwfn);

	if (max_rl == 0) {
		DP_ERR(p_hwfn, "pq with flag 0x%x do not exist\n",
		       PQ_FLAGS_RLS);
		return qed_qm_get_start_pq(p_hwfn);
	}

	/* When an invalid RL index is requested, return the highest
	 * available RL PQ. "max_rl - 1" is the relative index of the
	 * last PQ reserved for RLs.
	 */
	if (rl >= max_rl) {
		DP_ERR(p_hwfn,
		       "rl %hu is not a valid rate limiter, returning rl %hu\n",
		       rl, max_rl - 1);
		return pq_idx + max_rl - 1;
	}

	return pq_idx + rl;
}

static u16 qed_get_qm_pq_from_cm_pq(struct qed_hwfn *p_hwfn, u16 cm_pq_id)
{
	u16 start_pq = qed_qm_get_start_pq(p_hwfn);

	return cm_pq_id - CM_TX_PQ_BASE - start_pq;
}

static u16 qed_get_vport_id_from_pq(struct qed_hwfn *p_hwfn, u16 pq_id)
{
	u16 vport_id;

	spin_lock_bh(&p_hwfn->qm_info.qm_info_lock);
	vport_id = p_hwfn->qm_info.qm_pq_params[pq_id].vport_id;
	spin_unlock_bh(&p_hwfn->qm_info.qm_info_lock);

	return vport_id;
}

static u16 qed_get_rl_id_from_pq(struct qed_hwfn *p_hwfn, u16 pq_id)
{
	u16 rl_id;

	spin_lock_bh(&p_hwfn->qm_info.qm_info_lock);
	rl_id = p_hwfn->qm_info.qm_pq_params[pq_id].rl_id;
	spin_unlock_bh(&p_hwfn->qm_info.qm_info_lock);

	return rl_id;
}

u16 qed_get_pq_vport_id_from_rl(struct qed_hwfn * p_hwfn, u16 rl)
{
	u16 cm_pq_id = qed_get_cm_pq_idx_rl(p_hwfn, rl);
	u16 qm_pq_id = qed_get_qm_pq_from_cm_pq(p_hwfn, cm_pq_id);

	return qed_get_vport_id_from_pq(p_hwfn, qm_pq_id);
}

u16 qed_get_pq_vport_id_from_vf(struct qed_hwfn * p_hwfn, u16 vf)
{
	u16 cm_pq_id = qed_get_cm_pq_idx_vf(p_hwfn, vf, 0, 0);
	u16 qm_pq_id = qed_get_qm_pq_from_cm_pq(p_hwfn, cm_pq_id);

	return qed_get_vport_id_from_pq(p_hwfn, qm_pq_id);
}

u16 qed_get_pq_rl_id_from_rl(struct qed_hwfn * p_hwfn, u16 rl)
{
	u16 cm_pq_id = qed_get_cm_pq_idx_rl(p_hwfn, rl);
	u16 qm_pq_id = qed_get_qm_pq_from_cm_pq(p_hwfn, cm_pq_id);

	return qed_get_rl_id_from_pq(p_hwfn, qm_pq_id);
}

u16 qed_get_pq_rl_id_from_vf(struct qed_hwfn * p_hwfn, u16 vf)
{
	u16 cm_pq_id = qed_get_cm_pq_idx_vf(p_hwfn, vf, 0, 0);
	u16 qm_pq_id = qed_get_qm_pq_from_cm_pq(p_hwfn, cm_pq_id);

	return qed_get_rl_id_from_pq(p_hwfn, qm_pq_id);
}

static u16 qed_get_cm_pq_offset_mtc(struct qed_hwfn *p_hwfn, u16 idx, u8 tc)
{
	u16 pq_offset = 0, max_pqs;
	u8 num_ports, num_tcs;

	num_ports = qed_lag_support(p_hwfn) ? LAG_MAX_PORT_NUM : 1;
	num_tcs = qed_init_qm_get_num_mtc_tcs(p_hwfn);

	/* add the port offset */
	pq_offset += (idx % num_ports) * num_tcs;
	/* add the tc offset */
	pq_offset += tc % num_tcs;

	/* Verify that the pq returned is within pqs range */
	max_pqs = qed_init_qm_get_num_mtc_pqs(p_hwfn);
	if (pq_offset >= max_pqs) {
		DP_ERR(p_hwfn,
		       "pq_offset %d must be smaller than %d (idx %d tc %d)\n",
		       pq_offset, max_pqs, idx, tc);
		return 0;
	}

	return pq_offset;
}

u16 qed_get_cm_pq_idx_ofld_mtc(struct qed_hwfn * p_hwfn,
			       u16 idx, u8 tc, u16 pq_set_id)
{
	u16 first_ofld_pq, pq_offset;

#ifdef CONFIG_DCQCN
	if (p_hwfn->p_rdma_info->roce.dcqcn_enabled)
		return qed_get_cm_pq_idx_rl(p_hwfn, idx);
#endif

	if (QED_PQSET_SUPPORTED(p_hwfn))
		return qed_get_cm_pq_idx_tc(p_hwfn, PQ_FLAGS_OFLD,
					    pq_set_id, tc);

	first_ofld_pq = qed_get_cm_pq_idx(p_hwfn, PQ_FLAGS_OFLD);
	pq_offset = qed_get_cm_pq_offset_mtc(p_hwfn, idx, tc);

	return first_ofld_pq + pq_offset;
}

u16 qed_get_cm_pq_idx_llt_mtc(struct qed_hwfn * p_hwfn,
			      u16 idx, u8 tc, u16 pq_set_id)
{
	u16 first_llt_pq, pq_offset;

#ifdef CONFIG_DCQCN
	if (p_hwfn->p_rdma_info->roce.dcqcn_enabled)
		return qed_get_cm_pq_idx_rl(p_hwfn, idx);
#endif

	if (QED_PQSET_SUPPORTED(p_hwfn))
		return qed_get_cm_pq_idx_tc(p_hwfn, PQ_FLAGS_LLT,
					    pq_set_id, tc);

	first_llt_pq = qed_get_cm_pq_idx(p_hwfn, PQ_FLAGS_LLT);
	pq_offset = qed_get_cm_pq_offset_mtc(p_hwfn, idx, tc);

	return first_llt_pq + pq_offset;
}

u16 qed_get_cm_pq_idx_ll2(struct qed_hwfn * p_hwfn, u8 tc)
{
	switch (tc) {
	case PURE_LB_TC:
		return qed_get_cm_pq_idx(p_hwfn, PQ_FLAGS_LB);
	case PKT_LB_TC:
		return qed_get_cm_pq_idx(p_hwfn, PQ_FLAGS_OOO);
	default:
#ifdef CONFIG_DCQCN
		/* In RoCE, when DCQCN is enabled, there are no OFLD pqs,
		 * get the first RL pq.
		 */
		if (QED_IS_ROCE_PERSONALITY(p_hwfn) &&
		    p_hwfn->p_rdma_info->roce.dcqcn_enabled)
			return qed_get_cm_pq_idx_rl(p_hwfn, 0);
#endif
		return qed_get_cm_pq_idx(p_hwfn, PQ_FLAGS_OFLD);
	}
}

/* Functions for creating specific types of pqs */
static void qed_init_qm_lb_pq(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	if (!(qed_get_pq_flags(p_hwfn) & PQ_FLAGS_LB))
		return;

	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_LB, qm_info->num_pqs);
	qed_init_qm_pq(p_hwfn, qm_info, PURE_LB_TC, PQ_INIT_SHARE_VPORT);
}

static void qed_init_qm_ooo_pq(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	if (!(qed_get_pq_flags(p_hwfn) & PQ_FLAGS_OOO))
		return;

	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_OOO, qm_info->num_pqs);
	qed_init_qm_pq(p_hwfn, qm_info, qm_info->ooo_tc, PQ_INIT_SHARE_VPORT);
}

static void qed_init_qm_pure_ack_pq(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	if (!(qed_get_pq_flags(p_hwfn) & PQ_FLAGS_ACK))
		return;

	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_ACK, qm_info->num_pqs);
	qed_init_qm_pq(p_hwfn, qm_info, qed_get_offload_tc(p_hwfn),
		       PQ_INIT_SHARE_VPORT);
}

static void qed_init_qm_mtc_pqs(struct qed_hwfn *p_hwfn)
{
	u8 num_tcs = qed_init_qm_get_num_mtc_tcs(p_hwfn);
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 second_port = p_hwfn->port_id;
	u8 first_port = p_hwfn->port_id;
	u8 tc;

	/* if lag is not active, init all pqs with p_hwfn's default port */
	if (qed_lag_is_active(p_hwfn)) {
		first_port = p_hwfn->lag_info.first_port;
		second_port = p_hwfn->lag_info.second_port;
	}

	/* override pq's TC if offload TC is set */
	for (tc = 0; tc < num_tcs; tc++)
		qed_init_qm_pq_port(p_hwfn, qm_info,
				    qed_is_offload_tc_set(p_hwfn) ?
				    p_hwfn->hw_info.offload_tc : tc,
				    PQ_INIT_SHARE_VPORT, first_port);
	if (qed_lag_support(p_hwfn)) {
		/* initialize second port's pqs even if lag is not active */
		for (tc = 0; tc < num_tcs; tc++)
			qed_init_qm_pq_port(p_hwfn, qm_info,
					    qed_is_offload_tc_set(p_hwfn) ?
					    p_hwfn->hw_info.offload_tc : tc,
					    PQ_INIT_SHARE_VPORT, second_port);
	}
}

static void qed_init_qm_offload_pq(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	if (!(qed_get_pq_flags(p_hwfn) & PQ_FLAGS_OFLD))
		return;

	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_OFLD, qm_info->num_pqs);
	qed_init_qm_mtc_pqs(p_hwfn);
}

static void qed_init_qm_low_latency_pq(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	if (!(qed_get_pq_flags(p_hwfn) & PQ_FLAGS_LLT))
		return;

	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_LLT, qm_info->num_pqs);
	qed_init_qm_mtc_pqs(p_hwfn);
}

static void qed_init_qm_offload_pq_group(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 idx;

	if (!(qed_get_pq_flags(p_hwfn) & PQ_FLAGS_GRP))
		return;

	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_GRP, qm_info->num_pqs);

	/* iterate over offload pqs */
	for (idx = 0; idx < qed_init_qm_get_group_count(p_hwfn); idx++)
		qed_init_qm_pq_port(p_hwfn, qm_info,
				    qm_info->offload_group[idx].tc,
				    PQ_INIT_SHARE_VPORT,
				    qm_info->offload_group[idx].port);
}

static void qed_init_qm_mcos_pqs(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 tc_idx;

	if (!(qed_get_pq_flags(p_hwfn) & PQ_FLAGS_MCOS))
		return;

	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_MCOS, qm_info->num_pqs);
	for (tc_idx = 0; tc_idx < qed_init_qm_get_num_tcs(p_hwfn); tc_idx++)
		qed_init_qm_pq(p_hwfn, qm_info, tc_idx, PQ_INIT_SHARE_VPORT);
}

static void qed_init_qm_vf_single_rdma_pq(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u32 pq_flags = qed_get_pq_flags(p_hwfn);

	if (!(pq_flags & PQ_FLAGS_VSR))
		return;

	/* qed_init_qm_pq_params() is going to increment vport ID anyway,
	 * so keep it shared here so we don't waste a vport.
	 */
	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_VSR, qm_info->num_pqs);
	qed_init_qm_pq(p_hwfn, qm_info, qed_get_offload_tc(p_hwfn),
		       PQ_INIT_VF_RL | PQ_INIT_SHARE_VPORT);
}

static void qed_init_qm_vf_pqs(struct qed_hwfn *p_hwfn)
{
	u16 vf_idx, num_vfs = qed_init_qm_get_num_active_vfs(p_hwfn);
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u32 pq_flags = qed_get_pq_flags(p_hwfn);
	u32 l2_pq_init_flags = PQ_INIT_VF_RL;

	if (!(pq_flags & PQ_FLAGS_VFS))
		return;

	/* Mark PQ starting VF range */
	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_VFS, qm_info->num_pqs);

	/* If VFR is set, the L2 PQ will share the rate limiter with the rdma PQ */
	if (pq_flags & PQ_FLAGS_VFR)
		l2_pq_init_flags |= PQ_INIT_SHARE_VPORT;

	/* Init the per PF PQs */
	for (vf_idx = 0; vf_idx < num_vfs; vf_idx++) {
		/* Per VF L2 PQ */
		qed_init_qm_pq(p_hwfn, qm_info, PQ_INIT_DEFAULT_TC,
			       l2_pq_init_flags);

		/* Per VF Rdma PQ */
		if (pq_flags & PQ_FLAGS_VFR)
			qed_init_qm_pq(p_hwfn, qm_info,
				       qed_get_offload_tc(p_hwfn),
				       PQ_INIT_VF_RL);
	}
}

static void qed_init_qm_rl_pqs(struct qed_hwfn *p_hwfn)
{
	u16 pf_rls_idx, num_pf_rls = qed_init_qm_get_num_pf_rls(p_hwfn);
	struct qed_lag_info *lag_info = &p_hwfn->lag_info;
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 port = p_hwfn->port_id, tc;

	if (!(qed_get_pq_flags(p_hwfn) & PQ_FLAGS_RLS))
		return;

	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_RLS, qm_info->num_pqs);
	tc = qed_get_offload_tc(p_hwfn);
	for (pf_rls_idx = 0; pf_rls_idx < num_pf_rls; pf_rls_idx++) {
		/* if lag is present, set these pqs per port according to parity */
		if (lag_info->is_master &&
		    lag_info->lag_type != QED_LAG_TYPE_NONE &&
		    lag_info->port_num > 0)
			port = (pf_rls_idx % lag_info->port_num == 0) ?
			    lag_info->first_port : lag_info->second_port;

		qed_init_qm_pq_port(p_hwfn, qm_info, tc, PQ_INIT_PF_RL, port);
	}
}

static void qed_init_qm_pqset_pqs(struct qed_hwfn *p_hwfn,
				  u16 pq_set_id,
				  u16 abs_pq_idx,
				  u16 pq_idx,
				  u16 vport_idx,
				  u16 rl_idx, u8 tc, bool b_init_vport_params)
{
	u16 rl_id, rel_vp_id, abs_vp_id, abs_vp_id_def_pq_set;
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	struct init_qm_vport_params *vp_params;
	u8 tc_idx;

	/* Get vp_id for this pq_set from qed client. If vp_id is invalid,
	 * set it to default value of start_vport_idx.
	 */
	abs_vp_id = 0;
	if (abs_vp_id == QM_INVALID_WFQ_ID)
		abs_vp_id = vport_idx;
	rel_vp_id = abs_vp_id - qm_info->start_vport;

	/* Like vp_id try to query it, if invalid, set to default */
	rl_id = 0;
	if (rl_id == QM_INVALID_RL_ID)
		rl_id = rl_idx;

	/* init pq params */
	qm_info->qm_pq_params[pq_idx].port_id = p_hwfn->port_id;
	qm_info->qm_pq_params[pq_idx].vport_id = abs_vp_id;
	qm_info->qm_pq_params[pq_idx].tc_id = tc;
	qm_info->qm_pq_params[pq_idx].wrr_group = PQ_INIT_DEFAULT_WRR_GROUP;
	qm_info->qm_pq_params[pq_idx].rl_valid = true;
	qm_info->qm_pq_params[pq_idx].rl_id = rl_id;

	if (!b_init_vport_params)
		return;

	abs_vp_id_def_pq_set = tc +
	    qed_get_pqset_base(p_hwfn, QED_DEFAULT_PQ_SET);

	/* init vport params only for default pq_set or non-default pq_set
	 * which associated with non-default vport.
	 */
	if (QED_IS_SHARED_DEFAULT_PQSET(p_hwfn, pq_set_id) ||
	    abs_vp_id != abs_vp_id_def_pq_set) {
		/* init vport param - one vport per pq/tc */
		vp_params = &qm_info->qm_vport_params[rel_vp_id];

		for (tc_idx = 0; tc_idx < NUM_OF_TCS; tc_idx++) {
			if (tc_idx == tc)
				vp_params->first_tx_pq_id[tc_idx] = abs_pq_idx;
			else
				vp_params->first_tx_pq_id[tc_idx] = 0xFFFF;
		}
	}
}

static void
qed_init_qm_pqset_offld_pqs(struct qed_hwfn *p_hwfn,
			    u16 pq_set_id,
			    u8 num_offld_pqs,
			    u16 abs_start_pq_idx,
			    u16 start_pq_idx,
			    u16 start_vport_idx, u16 start_rl_id)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 offld_tc = qed_get_offload_tc(p_hwfn);
	u8 pq_idx;

	if (num_offld_pqs == 1) {
		/* use offload TC if set */
		qed_init_qm_pqset_pqs(p_hwfn, pq_set_id, abs_start_pq_idx,
				      start_pq_idx,
				      start_vport_idx + offld_tc,
				      start_rl_id, offld_tc, false);
	} else {
		/* allocate pqset_num_offload_pqs pqs */
		for (pq_idx = 0;
		     pq_idx < qm_info->pqset_num_offload_pqs; pq_idx++)
			qed_init_qm_pqset_pqs(p_hwfn, pq_set_id,
					      abs_start_pq_idx + pq_idx,
					      start_pq_idx + pq_idx,
					      start_vport_idx + pq_idx,
					      start_rl_id, pq_idx, false);
	}
}

static void qed_init_qm_single_pqset(struct qed_hwfn *p_hwfn, u16 pq_set_id)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u16 abs_start_pq, start_pq, start_vport, rl_id;
	u8 max_tc = qed_init_qm_get_num_tcs(p_hwfn);
	u8 pq_idx;

	start_vport = qm_info->start_vport + qm_info->num_vports;
	rl_id = qm_info->start_rl + qm_info->num_rls;
	abs_start_pq = qm_info->first_pqset_pq +
	    (pq_set_id * qm_info->num_pqs_per_pqset);
	start_pq = abs_start_pq - qm_info->start_pq;

	/* Init MCOS PQs and Vports */
	for (pq_idx = 0; pq_idx < max_tc; pq_idx++)
		qed_init_qm_pqset_pqs(p_hwfn, pq_set_id,
				      abs_start_pq + pq_idx,
				      start_pq + pq_idx,
				      start_vport + pq_idx, rl_id,
				      pq_idx, true);

	start_pq += max_tc;
	if (qm_info->offload_type == QED_OFFLOAD_DEDICATED_PQ_SET) {
		qed_init_qm_pqset_offld_pqs(p_hwfn, pq_set_id,
					    qm_info->pqset_num_offload_pqs,
					    abs_start_pq, start_pq,
					    start_vport, rl_id);

		start_pq += qm_info->pqset_num_offload_pqs;
	}

	if (qm_info->aux_offload_type == QED_AUX_OFFLOAD_DEDICATED_PQ_SET) {
		qed_init_qm_pqset_offld_pqs(p_hwfn, pq_set_id,
					    qm_info->pqset_num_aux_pqs,
					    abs_start_pq, start_pq,
					    start_vport, rl_id);
	}

	/* qm params accounting */
	qm_info->num_pqs += qm_info->num_pqs_per_pqset;
	qm_info->num_vports += max_tc;
	qed_init_qm_advance_rl(p_hwfn);
}

static void qed_init_qm_default_pqset(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 max_tc = qed_init_qm_get_num_tcs(p_hwfn);
	u32 pq_flags = qed_get_pq_flags(p_hwfn);
	u16 mcos_abs_start_pq, mcos_start_pq;
	u16 ofld_abs_start_pq, ofld_start_pq;
	u16 llt_abs_start_pq, llt_start_pq;
	u16 ack_abs_start_pq, ack_start_pq;
	u8 num_ofld_pqs, pq_idx;
	u16 start_vport, rl_id;

	start_vport = qm_info->start_vport + qm_info->num_vports;
	rl_id = qm_info->start_rl + qm_info->num_rls;
	mcos_abs_start_pq = qm_info->first_mcos_pq;
	mcos_start_pq = mcos_abs_start_pq - qm_info->start_pq;

	/* init default pq_set MCOS pqs */
	for (pq_idx = 0; pq_idx < max_tc; pq_idx++)
		qed_init_qm_pqset_pqs(p_hwfn, QED_DEFAULT_PQ_SET,
				      mcos_abs_start_pq + pq_idx,
				      mcos_start_pq + pq_idx,
				      start_vport + pq_idx, rl_id,
				      pq_idx, true);

	if (!(pq_flags & PQ_FLAGS_OFLD))
		return;

	ofld_abs_start_pq = qm_info->first_ofld_pq;
	ofld_start_pq = ofld_abs_start_pq - qm_info->start_pq;
	num_ofld_pqs = qed_init_qm_get_num_mtc_tcs(p_hwfn);

	/* init offload pqs */
	qed_init_qm_pqset_offld_pqs(p_hwfn, QED_DEFAULT_PQ_SET,
				    num_ofld_pqs,
				    ofld_abs_start_pq, ofld_start_pq,
				    start_vport, rl_id);

	/* init LLT or ACK pqs */
	if (pq_flags & PQ_FLAGS_LLT) {
		llt_abs_start_pq = qm_info->first_llt_pq;
		llt_start_pq = llt_abs_start_pq - qm_info->start_pq;
		qed_init_qm_pqset_offld_pqs(p_hwfn, QED_DEFAULT_PQ_SET,
					    num_ofld_pqs,
					    llt_abs_start_pq, llt_start_pq,
					    start_vport, rl_id);
	} else if (pq_flags & PQ_FLAGS_ACK) {
		ack_abs_start_pq = qm_info->pure_ack_pq;
		ack_start_pq = ack_abs_start_pq - qm_info->start_pq;
		qed_init_qm_pqset_offld_pqs(p_hwfn, QED_DEFAULT_PQ_SET, 1,
					    ack_abs_start_pq, ack_start_pq,
					    start_vport, rl_id);
	} else {
		DP_NOTICE(p_hwfn, "Neither LLT nor ACK\n");
	}

	/* qm params accounting */
	qm_info->num_vports += max_tc;
	qed_init_qm_advance_rl(p_hwfn);
}

static void qed_init_qm_pqset(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u32 pq_flags = qed_get_pq_flags(p_hwfn);
	u16 pq_set_idx;

	if (!(pq_flags & PQ_FLAGS_PQSET))
		return;

	qed_init_qm_pqset_idx(p_hwfn, qm_info->num_pqs);

	if (qm_info->start_pqset_num)
		/* will allocate and increase vports */
		qed_init_qm_default_pqset(p_hwfn);
	else
		/* done with default vport */
		qed_init_qm_advance_vport(p_hwfn);

	for (pq_set_idx = 0; pq_set_idx < qm_info->num_pqset; pq_set_idx++)
		qed_init_qm_single_pqset(p_hwfn, pq_set_idx);
}

static void qed_init_qm_pq_params(struct qed_hwfn *p_hwfn)
{
	/* rate limited pqs, must come first (FW assumption) */
	qed_init_qm_rl_pqs(p_hwfn);

	/* pqs for multi cos */
	qed_init_qm_mcos_pqs(p_hwfn);

	/* pure loopback pq */
	qed_init_qm_lb_pq(p_hwfn);

	/* out of order pq */
	qed_init_qm_ooo_pq(p_hwfn);

	/* pure ack pq */
	qed_init_qm_pure_ack_pq(p_hwfn);

	/* pq for offloaded protocol */
	qed_init_qm_offload_pq(p_hwfn);

	/* low latency pq */
	qed_init_qm_low_latency_pq(p_hwfn);

	/* per offload group pqs */
	qed_init_qm_offload_pq_group(p_hwfn);

	/* Single VF-RDMA PQ, in case there weren't enough for each VF */
	qed_init_qm_vf_single_rdma_pq(p_hwfn);

	/* PF done sharing vports, advance vport for first VF.
	 * Vport ID is incremented in a separate function because we can't
	 * rely on the last PF PQ to not use PQ_INIT_SHARE_VPORT, which can
	 * be different in every QM reconfiguration.
	 */
	qed_init_qm_advance_vport(p_hwfn);

	/* pqs for vfs */
	qed_init_qm_vf_pqs(p_hwfn);

	/* init pqs for pq sets */
	qed_init_qm_pqset(p_hwfn);
}

/* Finds the optimal features configuration to maximize PQs utilization */
static int qed_init_qm_features(struct qed_hwfn *p_hwfn)
{
	if (qed_init_qm_get_num_vports(p_hwfn) > RESC_NUM(p_hwfn, QED_PQ)) {
		DP_ERR(p_hwfn, "requested amount of vports exceeds resource\n");
		return -EINVAL;
	}

	if (qed_init_qm_get_num_pf_rls(p_hwfn) == 0) {
		if (IS_QED_PACING(p_hwfn)) {
			DP_ERR(p_hwfn, "No rate limiters available for PF\n");
			return -EINVAL;
		}

		if (IS_QED_DCQCN(p_hwfn)) {
			DP_ERR(p_hwfn,
			       "No rate limiters available for PF, disabling DCQCN\n");
			p_hwfn->b_en_dcqcn = 0;
		}
	}

	/* For VF RDMA try to provide 2 PQs (separate PQ for RDMA) per VF */
	if (QED_IS_RDMA_PERSONALITY(p_hwfn) && QED_IS_VF_RDMA(p_hwfn) &&
	    qed_init_qm_get_num_active_vfs(p_hwfn))
		p_hwfn->qm_info.vf_rdma_en = true;

	while (qed_init_qm_get_num_pqs(p_hwfn) > RESC_NUM(p_hwfn, QED_PQ) ||
	       qed_init_qm_get_num_rls(p_hwfn) > RESC_NUM(p_hwfn, QED_RL)) {
		if (IS_QED_QM_VF_RDMA(p_hwfn)) {
			p_hwfn->qm_info.vf_rdma_en = false;
			DP_NOTICE(p_hwfn,
				  "PQ per rdma vf was disabled to reduce requested amount of pqs/rls. A single PQ for all rdma VFs will be used\n");
			continue;
		}

		if (IS_QED_MULTI_TC_ROCE(p_hwfn)) {
			p_hwfn->hw_info.multi_tc_roce_en = false;
			DP_NOTICE(p_hwfn,
				  "multi-tc roce was disabled to reduce requested amount of pqs/rls\n");
			continue;
		}

		DP_ERR(p_hwfn,
		       "Requested amount: %d pqs %d rls, Actual amount: %d pqs %d rls\n",
		       qed_init_qm_get_num_pqs(p_hwfn),
		       qed_init_qm_get_num_rls(p_hwfn),
		       RESC_NUM(p_hwfn, QED_PQ), RESC_NUM(p_hwfn, QED_RL));
		return -EINVAL;
	}

	return 0;
}

/*
 * Function for verbose printing of the qm initialization results
 */
static void qed_dp_init_qm_params(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	struct init_qm_vport_params *vport;
	struct init_qm_port_params *port;
	struct init_qm_pq_params *pq;
	int i, tc;

	if (qm_info->pq_overflow)
		return;

	/* top level params */
	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "qm init params: pq_flags 0x%x, num_pqs %d, num_vf_pqs %d, start_pq %d\n",
		   qed_get_pq_flags(p_hwfn),
		   qm_info->num_pqs, qm_info->num_vf_pqs, qm_info->start_pq);
	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "qm init params: pf_rl_en %d, pf_wfq_en %d, vport_rl_en %d, vport_wfq_en %d\n",
		   qm_info->pf_rl_en,
		   qm_info->pf_wfq_en,
		   qm_info->vport_rl_en, qm_info->vport_wfq_en);
	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "qm init params: num_vports %d, start_vport %d, num_rls %d, num_pf_rls %d, start_rl %d, pf_rl %d\n",
		   qm_info->num_vports,
		   qm_info->start_vport,
		   qm_info->num_rls,
		   qm_info->num_pf_rls, qm_info->start_rl, qm_info->pf_rl);
	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "qm init params: pure_lb_pq %d, ooo_pq %d, pure_ack_pq %d, first_ofld_pq %d, first_llt_pq %d\n",
		   qm_info->pure_lb_pq,
		   qm_info->ooo_pq,
		   qm_info->pure_ack_pq,
		   qm_info->first_ofld_pq, qm_info->first_llt_pq);
	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "qm init params: single_vf_rdma_pq %d, first_vf_pq %d, max_phys_tcs_per_port %d, pf_wfq %d\n",
		   qm_info->single_vf_rdma_pq,
		   qm_info->first_vf_pq,
		   qm_info->max_phys_tcs_per_port, qm_info->pf_wfq);
	/* pq_set params */
	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "qm init params: num_pqset %d, start_pqset_num %d, first_pqset_pq %d, pqset_num_offload_pqs %d\n",
		   qm_info->num_pqset,
		   qm_info->start_pqset_num,
		   qm_info->first_pqset_pq, qm_info->pqset_num_offload_pqs);
	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "qm init params: pqset_num_aux_pqs %d num_pqs_per_pqset %d offload_type %d aux_offload_type %d\n",
		   qm_info->pqset_num_aux_pqs,
		   qm_info->num_pqs_per_pqset,
		   qm_info->offload_type, qm_info->aux_offload_type);

	/* port table */
	for (i = 0; i < p_hwfn->cdev->num_ports_in_engine; i++) {
		port = &(qm_info->qm_port_params[i]);
		DP_VERBOSE(p_hwfn,
			   QED_MSG_SP,
			   "port idx %d, active %d, active_phys_tcs %d, num_pbf_cmd_lines %d, num_btb_blocks %d, reserved %d\n",
			   i,
			   port->active,
			   port->active_phys_tcs,
			   port->num_pbf_cmd_lines,
			   port->num_btb_blocks, port->reserved);
	}

	/* vport table */
	for (i = 0; i < qm_info->num_vports; i++) {
		vport = &(qm_info->qm_vport_params[i]);
		DP_VERBOSE(p_hwfn, QED_MSG_SP,
			   "vport idx %d, wfq %d, first_tx_pq_id [ ",
			   qm_info->start_vport + i, vport->wfq);
		for (tc = 0; tc < NUM_OF_TCS; tc++)
			if (0xFFFF != vport->first_tx_pq_id[tc])
				DP_VERBOSE(p_hwfn, QED_MSG_SP, "TC%d(%d) ",
					   tc, vport->first_tx_pq_id[tc]);
		DP_VERBOSE(p_hwfn, QED_MSG_SP, "]\n");
	}

	/* pq table */
	for (i = 0; i < qm_info->num_pqs; i++) {
		pq = &(qm_info->qm_pq_params[i]);
		DP_VERBOSE(p_hwfn,
			   QED_MSG_SP,
			   "pq idx %d, port %d, vport_id %d, tc %d, wrr_grp %d, rl_valid %d, rl_id %d\n",
			   qm_info->start_pq + i,
			   pq->port_id,
			   pq->vport_id,
			   pq->tc_id, pq->wrr_group, pq->rl_valid, pq->rl_id);
	}
}

static void qed_init_qm_info(struct qed_hwfn *p_hwfn)
{
	/* reset params required for init run */
	qed_init_qm_reset_params(p_hwfn);

	/* init QM top level params */
	qed_init_qm_params(p_hwfn);

	/* init QM port params */
	qed_init_qm_port_params(p_hwfn);

	/* init QM vport params */
	qed_init_qm_vport_params(p_hwfn);

	/* init QM physical queue params */
	qed_init_qm_pq_params(p_hwfn);

	/* display all that init */
	qed_dp_init_qm_params(p_hwfn);
}

static int qed_qm_config_pqset(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 max_tc = qed_init_qm_get_num_tcs(p_hwfn);
	struct qed_eth_pf_params *p_eth_pf_params;
	u32 pq_flags = qed_get_pq_flags(p_hwfn);
	u16 num_pqs_in_pq_set = 0;
	u16 num_required_rls;
	u16 num_required_pqs;
	u16 num_pqset;
	u32 num_pqs;

	/* init pq set qm_info */
	qm_info->num_pqset = 0;
	qm_info->start_pqset_num = 0;
	qm_info->first_pqset_pq = 0;
	qm_info->pqset_num_offload_pqs = 0;
	qm_info->pqset_num_aux_pqs = 0;
	qm_info->relative_wfq_speed = 0;

	p_eth_pf_params = &p_hwfn->pf_params.eth_pf_params;

	if (!p_eth_pf_params->num_desired_pqset)
		return 0;

	if (p_eth_pf_params->pqset_offload_type >= QED_OFFLOAD_PQ_SET_MAX)
		return -EINVAL;
	if (p_eth_pf_params->pqset_aux_offload_type >=
	    QED_AUX_OFFLOAD_PQ_SET_MAX)
		return -EINVAL;
	if (p_eth_pf_params->pqset_offload_pqs_num > max_tc)
		return -EINVAL;
	if (p_eth_pf_params->pqset_aux_offload_pqs_num > max_tc)
		return -EINVAL;

	qm_info->pqset_num_offload_pqs = p_eth_pf_params->pqset_offload_pqs_num;

	if (qm_info->pqset_num_offload_pqs > max_tc)
		qm_info->pqset_num_offload_pqs = max_tc;

	qm_info->pqset_num_aux_pqs = p_eth_pf_params->pqset_aux_offload_pqs_num;

	if (qm_info->pqset_num_aux_pqs > max_tc)
		qm_info->pqset_num_aux_pqs = max_tc;

	qm_info->offload_type = p_eth_pf_params->pqset_offload_type;
	qm_info->aux_offload_type = p_eth_pf_params->pqset_aux_offload_type;
	qm_info->start_pqset_num = ! !p_eth_pf_params->pqset_shared_default;
	qm_info->relative_wfq_speed = true;

	num_pqs = RESC_NUM(p_hwfn, QED_PQ);

	/* PQs which reserved */
	num_required_pqs =
	    (! !(PQ_FLAGS_MCOS & pq_flags)) *
	    qed_init_qm_get_num_tcs(p_hwfn) +
	    (! !(PQ_FLAGS_LB & pq_flags)) +
	    (! !(PQ_FLAGS_OOO & pq_flags)) +
	    (! !(PQ_FLAGS_ACK & pq_flags)) +
	    (! !(PQ_FLAGS_OFLD & pq_flags)) *
	    qed_init_qm_get_num_mtc_pqs(p_hwfn) +
	    (! !(PQ_FLAGS_LLT & pq_flags)) *
	    qed_init_qm_get_num_mtc_pqs(p_hwfn);

	if (pq_flags & PQ_FLAGS_MCOS)
		num_pqs_in_pq_set += max_tc;

	if (pq_flags & PQ_FLAGS_OFLD &&
	    qm_info->offload_type == QED_OFFLOAD_DEDICATED_PQ_SET)
		num_pqs_in_pq_set += qm_info->pqset_num_offload_pqs;

	if (pq_flags & PQ_FLAGS_ACK &&
	    qm_info->aux_offload_type == QED_AUX_OFFLOAD_DEDICATED_PQ_SET)
		num_pqs_in_pq_set += qm_info->pqset_num_aux_pqs;

	if (pq_flags & PQ_FLAGS_LLT &&
	    qm_info->aux_offload_type == QED_AUX_OFFLOAD_DEDICATED_PQ_SET)
		num_pqs_in_pq_set += qm_info->pqset_num_aux_pqs;

	/* subtract PQs which consumed for other purposes */
	num_pqs -= num_required_pqs;

	if (!num_pqs_in_pq_set)
		return -EINVAL;

	qm_info->num_pqs_per_pqset = num_pqs_in_pq_set;

	num_pqset = num_pqs / num_pqs_in_pq_set;

	if (num_pqset < p_eth_pf_params->num_desired_pqset)
		qm_info->num_pqset = num_pqset;
	else
		qm_info->num_pqset = p_eth_pf_params->num_desired_pqset -
		    qm_info->start_pqset_num;

	/* One RL for all PQs of PQ set - One RL per each PQ set */
	num_required_rls = NUM_DEFAULT_RLS + qm_info->num_pqset;
	if (num_required_rls > RESC_NUM(p_hwfn, QED_RL))
		qm_info->num_pqset = RESC_NUM(p_hwfn, QED_RL) - NUM_DEFAULT_RLS;

	return 0;
}

/* This function reconfigures the QM pf on the fly.
 * For this purpose we:
 * 1. reconfigure the QM database
 * 2. set new values to runtime array
 * 3. send an sdm_qm_cmd through the rbc interface to stop the QM
 * 4. activate init tool in QM_PF stage
 * 5. send an sdm_qm_cmd through rbc interface to release the QM
 */
static int __qed_qm_reconf(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, bool b_can_sleep)
{
	struct qed_resc_unlock_params resc_unlock_params;
	struct qed_resc_lock_params resc_lock_params;
	int rc = 0;
	bool b_rc, b_mfw_unlock = true;
	struct qed_qm_info *qm_info;

	qm_info = &p_hwfn->qm_info;

	/* Obtain MFW resource lock to sync with PFs with driver instances not
	 * covered by the static global qm_lock (monolithic, dpdk, PDA).
	 */
	qed_mcp_resc_lock_default_init(p_hwfn, &resc_lock_params,
				       &resc_unlock_params,
				       QED_RESC_LOCK_QM_RECONF, false);
	resc_lock_params.sleep_b4_retry = b_can_sleep;
	rc = qed_mcp_resc_lock(p_hwfn, p_ptt, &resc_lock_params);

	/* If lock is taken we must abort. If MFW does not support the feature
	 * or took too long to acquire the lock we soldier on.
	 */
	if (rc != 0 && rc != -EOPNOTSUPP && rc != -EBUSY) {
		DP_ERR(p_hwfn,
		       "QM reconf MFW lock is stuck. Failing reconf flow\n");
		return -EINVAL;
	}

	/* if MFW doesn't support, no need to unlock. There is no harm in
	 * trying, but we would need to tweak the rc value in case of
	 * -EOPNOTSUPP, so seems nicer to avoid.
	 */
	if (rc == -EOPNOTSUPP)
		b_mfw_unlock = false;

	/* Multiple hwfn flows can issue qm reconf. Need to lock between hwfn
	 * flows.
	 */
	qed_qm_acquire_access(p_hwfn);

	/* qm_info is invalid while this lock is taken */
	spin_lock_bh(&p_hwfn->qm_info.qm_info_lock);

	rc = qed_init_qm_features(p_hwfn);
	if (rc) {
		spin_unlock_bh(&p_hwfn->qm_info.qm_info_lock);
		goto unlock;
	}

	/* initialize qed's qm data structure */
	qed_init_qm_info(p_hwfn);

	spin_unlock_bh(&p_hwfn->qm_info.qm_info_lock);

	if (qm_info->pq_overflow) {
		rc = -EINVAL;
		goto unlock;
	}

	/* stop PF's qm queues */
	b_rc = qed_send_qm_stop_cmd(p_hwfn, p_ptt, false, true,
				    qm_info->start_pq, qm_info->num_pqs);
	if (!b_rc) {
		rc = -EINVAL;
		goto unlock;
	}

	/* prepare QM portion of runtime array */
	qed_qm_init_pf(p_hwfn, p_ptt, false);

	/* activate init tool on runtime array */
	rc = qed_init_run(p_hwfn, p_ptt, PHASE_QM_PF, p_hwfn->rel_pf_id,
			  p_hwfn->hw_info.hw_mode);

	/* start PF's qm queues */
	b_rc = qed_send_qm_stop_cmd(p_hwfn, p_ptt, true, true,
				    qm_info->start_pq, qm_info->num_pqs);
	if (!b_rc)
		rc = -EINVAL;

unlock:
	qed_qm_release_access(p_hwfn);

	if (b_mfw_unlock)
		rc = qed_mcp_resc_unlock(p_hwfn, p_ptt, &resc_unlock_params);

	return rc;
}

int qed_qm_reconf(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	return __qed_qm_reconf(p_hwfn, p_ptt, true);
}

int qed_qm_reconf_intr(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	return __qed_qm_reconf(p_hwfn, p_ptt, false);
}

static int qed_alloc_qm_data(struct qed_hwfn *p_hwfn)
{
	u16 max_vports_num = (u16) RESC_NUM(p_hwfn, QED_PQ);
	u16 max_pqs_num = (u16) RESC_NUM(p_hwfn, QED_PQ);
	u16 max_rls_num = (u16) RESC_NUM(p_hwfn, QED_RL);
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	int rc;

	spin_lock_init(&qm_info->qm_info_lock);

	rc = qed_init_qm_features(p_hwfn);
	if (rc)
		goto alloc_err;

	qm_info->qm_pq_params =
	    kzalloc(sizeof(struct init_qm_pq_params) * max_pqs_num, GFP_KERNEL);
	if (!qm_info->qm_pq_params)
		goto alloc_err;

	qm_info->qm_vport_params =
	    kzalloc(sizeof(struct init_qm_vport_params) * max_vports_num,
		    GFP_KERNEL);
	if (!qm_info->qm_vport_params)
		goto alloc_err;

	if (max_rls_num) {
		qm_info->qm_rl_params =
		    kzalloc(sizeof(struct init_qm_rl_params) * max_rls_num,
			    GFP_KERNEL);
		if (!qm_info->qm_rl_params)
			goto alloc_err;
	}

	qm_info->qm_port_params = kzalloc(sizeof(struct init_qm_port_params) *
					  p_hwfn->cdev->num_ports_in_engine,
					  GFP_KERNEL);
	if (!qm_info->qm_port_params)
		goto alloc_err;

	qm_info->wfq_data =
	    kzalloc(sizeof(struct qed_wfq_data) * max_vports_num, GFP_KERNEL);
	if (!qm_info->wfq_data)
		goto alloc_err;

	return 0;

alloc_err:
	DP_NOTICE(p_hwfn, "Failed to allocate memory for QM params\n");
	qed_qm_info_free(p_hwfn);
	return -ENOMEM;
}

/******************** End QM initialization ***************/

static int qed_lag_create_slave(struct qed_hwfn *p_hwfn, u8 master_pfid)
{
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	u8 slave_ppfid = 1;	/* TODO: Need some sort of resource management function
				 * to return a free entry */
	int rc;

	if (!p_ptt)
		return -EAGAIN;

	rc = qed_llh_map_ppfid_to_pfid(p_hwfn, p_ptt, slave_ppfid, master_pfid);
	qed_ptt_release(p_hwfn, p_ptt);
	if (rc)
		return rc;

	/* Protocol filter for RoCE v1 */
	rc = qed_llh_add_protocol_filter(p_hwfn->cdev, slave_ppfid,
					 QED_LLH_FILTER_ETHERTYPE, 0x8915, 0);
	if (rc)
		return rc;

	/* Protocol filter for RoCE v2 */
	return qed_llh_add_protocol_filter(p_hwfn->cdev, slave_ppfid,
					   QED_LLH_FILTER_UDP_DEST_PORT, 0,
					   4791);
}

static void qed_lag_destroy_slave(struct qed_hwfn *p_hwfn)
{
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	u8 slave_ppfid = 1;	/* Need some sort of resource management function
				 * to return a free entry */

	/* Protocol filter for RoCE v1 */
	qed_llh_remove_protocol_filter(p_hwfn->cdev, slave_ppfid,
				       QED_LLH_FILTER_ETHERTYPE, 0x8915, 0);

	/* Protocol filter for RoCE v2 */
	qed_llh_remove_protocol_filter(p_hwfn->cdev, slave_ppfid,
				       QED_LLH_FILTER_UDP_DEST_PORT, 0, 4791);

	if (p_ptt) {
		qed_llh_map_ppfid_to_pfid(p_hwfn, p_ptt, slave_ppfid,
					  p_hwfn->rel_pf_id);
		qed_ptt_release(p_hwfn, p_ptt);
	}
}

/* Map ports:
 *      port 0/2 - 0/2
 *      port 1/3 - 1/3
 * If port 0/2 is down, map both to port 1/3, if port 1/3 is down, map both to
 * port 0/2, and if both are down, it doesn't really matter.
 */
static void qed_lag_map_ports(struct qed_hwfn *p_hwfn)
{
	struct qed_lag_info *lag_info = &p_hwfn->lag_info;

	/* for now support only 2 ports in the bond */
	if (lag_info->master_pf == 0) {
		lag_info->first_port =
		    (lag_info->active_ports & BIT(0)) ? 0 : 1;
		lag_info->second_port =
		    (lag_info->active_ports & BIT(1)) ? 1 : 0;
	} else if (lag_info->master_pf == 2) {
		lag_info->first_port =
		    (lag_info->active_ports & BIT(2)) ? 2 : 3;
		lag_info->second_port =
		    (lag_info->active_ports & BIT(3)) ? 3 : 2;
	}
	lag_info->port_num = LAG_MAX_PORT_NUM;
}

/* The following function strongly assumes two ports only */
static int qed_lag_create_master(struct qed_hwfn *p_hwfn)
{
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	int rc;

	if (!p_ptt)
		return -EAGAIN;

	qed_lag_map_ports(p_hwfn);
	rc = qed_qm_reconf_intr(p_hwfn, p_ptt);
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

/* The following function strongly assumes two ports only */
static int qed_lag_destroy_master(struct qed_hwfn *p_hwfn)
{
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	int rc;

	if (!p_ptt)
		return -EAGAIN;

	p_hwfn->qm_info.offload_group_count = 0;

	rc = qed_qm_reconf_intr(p_hwfn, p_ptt);
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

int qed_lag_create(struct qed_dev *dev,
		   enum qed_lag_type lag_type,
		   void (*link_change_cb) (void *cxt),
		   void *cxt, u8 active_ports)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(dev);
	u8 master_pfid = p_hwfn->abs_pf_id < 2 ? 0 : 2;

	if (!qed_lag_support(p_hwfn)) {
		DP_NOTICE(p_hwfn,
			  "RDMA bonding will not be configured - only supported on AH devices on default mode\n");
		return -EINVAL;
	}

	/* TODO: Check Supported MFW */
	p_hwfn->lag_info.lag_type = lag_type;
	p_hwfn->lag_info.link_change_cb = link_change_cb;
	p_hwfn->lag_info.cxt = cxt;
	p_hwfn->lag_info.active_ports = active_ports;
	p_hwfn->lag_info.is_master = p_hwfn->abs_pf_id == master_pfid;
	p_hwfn->lag_info.master_pf = master_pfid;

	/* Configure RX for LAG */
	if (p_hwfn->lag_info.is_master)
		return qed_lag_create_master(p_hwfn);

	return qed_lag_create_slave(p_hwfn, master_pfid);
}

/* Modify the link state of a given port */
int qed_lag_modify(struct qed_dev *dev, u8 port_id, u8 link_active)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(dev);
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	struct qed_lag_info *lag_info = &p_hwfn->lag_info;
	int rc = 0;
	unsigned long active_ports;
	u8 curr_active;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "Active ports changed before %lx link active %x port_id=%d\n",
		   lag_info->active_ports, link_active, port_id);

	if (!p_ptt)
		return -EAGAIN;

	active_ports = lag_info->active_ports;
	curr_active = ! !test_bit(port_id, &lag_info->active_ports);
	if (curr_active != link_active) {
		test_and_change_bit(port_id, &lag_info->active_ports);

		/* Reconfigure QM according to active_ports */
		if (lag_info->is_master) {
			qed_lag_map_ports(p_hwfn);
			rc = qed_qm_reconf_intr(p_hwfn, p_ptt);
		}

		DP_VERBOSE(p_hwfn,
			   QED_MSG_SP,
			   "Active ports changed before %lx after %lx\n",
			   active_ports, lag_info->active_ports);
	} else {
		/* No change in active ports, triggered from port event */
		/* call dcbx related code */
		DP_VERBOSE(p_hwfn, QED_MSG_SP, "Nothing changed\n");
	}

	qed_ptt_release(p_hwfn, p_ptt);
	return rc;
}

int qed_lag_destroy(struct qed_dev *dev)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(dev);
	struct qed_lag_info *lag_info = &p_hwfn->lag_info;

	lag_info->lag_type = QED_LAG_TYPE_NONE;
	lag_info->link_change_cb = NULL;
	lag_info->cxt = NULL;

	if (!lag_info->is_master) {
		qed_lag_destroy_slave(p_hwfn);
		return 0;
	}

	return qed_lag_destroy_master(p_hwfn);
}

bool qed_lag_is_active(struct qed_hwfn * p_hwfn)
{
	return !(p_hwfn->lag_info.lag_type == QED_LAG_TYPE_NONE);
}

#if 0
static void qed_lag_notify(struct qed_hwfn *p_hwfn)
{
	void (*link_change_cb) (void *cxt);

	if (!qed_lag_is_active(p_hwfn))
		return;

	link_change_cb = p_hwfn->lag_info.link_change_cb;
	if (link_change_cb)
		link_change_cb(p_hwfn->lag_info.cxt);
}
#endif

int qed_resc_alloc(struct qed_dev *cdev)
{
	int rc = 0;
	u32 rdma_tasks = RDMA_MAX_TIDS, excess_tasks;
	u32 line_count;
	int i;

	if (IS_PF(cdev)) {
		cdev->fw_data = kzalloc(sizeof(*cdev->fw_data), GFP_KERNEL);
		if (!cdev->fw_data)
			return -ENOMEM;
	}

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];
		u32 n_eqes, num_cons;

		/* initialize the doorbell recovery mechanism */
		rc = qed_db_recovery_setup(p_hwfn);
		if (rc)
			goto alloc_err;

		rc = qed_l2_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

		if (IS_VF(cdev)) {
#ifdef CONFIG_QED_LL2
			if (p_hwfn->using_ll2) {
				rc = qed_ll2_alloc(p_hwfn);
				if (rc)
					goto alloc_err;
			}
#endif
			if (QED_IS_RDMA_PERSONALITY(p_hwfn)) {
				rc = qed_rdma_info_alloc(p_hwfn);
				if (rc)
					goto alloc_err;
			} else {
				DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
					   "Non RDMA personality.\n");
			}

			/* Allocating the entire spq struct although only the
			 * async_comp callbacks are used by VF.
			 */
			rc = qed_spq_alloc(p_hwfn);
			if (rc)
				return rc;

			continue;
		}

		/* qed_iov_alloc must be called before qed_cxt_set_pf_params() */
		rc = qed_iov_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

		/* First allocate the context manager structure */
		rc = qed_cxt_mngr_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

		/* Set the HW cid/tid numbers (in the context manager)
		 * Must be done prior to any further computations.
		 */
		rc = qed_cxt_set_pf_params(p_hwfn, rdma_tasks);
		if (rc)
			goto alloc_err;

		rc = qed_qm_config_pqset(p_hwfn);
		if (rc)
			goto alloc_err;

		rc = qed_alloc_qm_data(p_hwfn);
		if (rc)
			goto alloc_err;

		/* init qm info */
		qed_init_qm_info(p_hwfn);

		/* Compute the ILT client partition */
		rc = qed_cxt_cfg_ilt_compute(p_hwfn, &line_count);
		if (rc) {
			u32 ilt_page_size_kb =
			    qed_cxt_get_ilt_page_size(p_hwfn) >> 10;

			DP_NOTICE(p_hwfn,
				  "First ILT compute failed - requested %u lines but only %u are available. line size is %u KB; re-computing with less lines\n",
				  line_count,
				  RESC_NUM(p_hwfn, QED_ILT), ilt_page_size_kb);

			/* In case there are not enough ILT lines we reduce the
			 * number of RDMA tasks and re-compute.
			 */
			excess_tasks =
			    qed_cxt_cfg_ilt_compute_excess(p_hwfn, line_count);
			if (!excess_tasks)
				goto alloc_err;

			if (excess_tasks < RDMA_MAX_TIDS) {
				DP_NOTICE(p_hwfn,
					  "re-computing after reducing %d tasks\n",
					  excess_tasks);
				rdma_tasks = RDMA_MAX_TIDS - excess_tasks;
				rc = qed_cxt_set_pf_params(p_hwfn, rdma_tasks);
				if (rc)
					goto alloc_err;

				rc = qed_cxt_cfg_ilt_compute(p_hwfn,
							     &line_count);
				if (rc)
					DP_NOTICE(p_hwfn,
						  "Failed to re-compute after reducing tasks\n");
			}

			if (rc && QED_IS_VF_RDMA(p_hwfn)) {
				DP_NOTICE(p_hwfn,
					  "re-computing after disabling VF RDMA\n");
				p_hwfn->pf_iov_info->rdma_enable = false;

				/* After disabling VF RDMA, we must call
				 * qed_cxt_set_pf_params(), in order to
				 * recalculate the VF cids/tids amount.
				 */
				rc = qed_cxt_set_pf_params(p_hwfn, rdma_tasks);
				if (rc)
					goto alloc_err;

				rc = qed_cxt_cfg_ilt_compute(p_hwfn,
							     &line_count);
				if (rc)
					DP_NOTICE(p_hwfn,
						  "Failed to re-compute after disabling VF-RDMA\n");
			}

			if (rc) {
				DP_ERR(p_hwfn,
				       "ILT compute failed - requested %u lines but only %u are available. Need to increase ILT line size (current size is %u KB)\n",
				       line_count,
				       RESC_NUM(p_hwfn, QED_ILT),
				       ilt_page_size_kb);
				goto alloc_err;
			}
		}

		/* CID map / ILT shadow table / T2
		 * The talbes sizes are determined by the computations above
		 */
		rc = qed_cxt_tables_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

		/* SPQ, must follow ILT because initializes SPQ context */
		rc = qed_spq_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

		/* SP status block allocation */
		p_hwfn->p_dpc_ptt = qed_get_reserved_ptt(p_hwfn,
							 RESERVED_PTT_DPC);

		rc = qed_int_alloc(p_hwfn, p_hwfn->p_main_ptt);
		if (rc)
			goto alloc_err;

		/* EQ */
		n_eqes = qed_chain_get_capacity(&p_hwfn->p_spq->chain);
		if (QED_IS_RDMA_PERSONALITY(p_hwfn)) {
			u32 n_srq = qed_cxt_get_srq_count(p_hwfn, IOV_PF) +
			    qed_cxt_get_xrc_srq_count(p_hwfn);

			/* Calculate the EQ size
			 * ---------------------
			 * Each ICID may generate up to one event at a time i.e.
			 * the event must be handled/cleared before a new one
			 * can be generated. We calculate the sum of events per
			 * protocol and create an EQ deep enough to handle the
			 * worst case:
			 * - Core - according to SPQ.
			 * - RoCE - per QP there are a couple of ICIDs, one
			 *        responder and one requester, each can
			 *        generate max 2 EQE (err+qp_destroyed) =>
			 *        n_eqes_qp = 4 * n_qp.
			 *        Each CQ can generate an EQE. There are 2 CQs
			 *        per QP => n_eqes_cq = 2 * n_qp.
			 *        Hence the RoCE total is 6 * n_qp or
			 *        3 * num_cons.
			 *        On top of that one eqe shoule be added for
			 *        each XRC SRQ and SRQ.
			 * - iWARP - can generate three async per QP (error
			 *        detected and qp in error) and an
			 *        additional error per CQ. 4* num_cons.
			 *        On top of that one eqe shoule be added for
			 *        each SRQ and XRC SRQ.
			 * - ENet - There can be up to two events per VF. One
			 *        for VF-PF channel and another for VF FLR
			 *        initial cleanup. The number of VFs is
			 *        bounded by MAX_NUM_VFS_BB, and is much
			 *        smaller than RoCE's so we avoid exact
			 *        calculation.
			 */
			if (p_hwfn->hw_info.personality == QED_PCI_ETH_ROCE) {
				num_cons =
				    qed_cxt_get_proto_cid_count(p_hwfn,
								PROTOCOLID_ROCE,
								NULL);
				num_cons *= 3;
			} else {
				num_cons =
				    qed_cxt_get_proto_cid_count(p_hwfn,
								PROTOCOLID_IWARP,
								NULL);
				num_cons *= 4;
			}
			n_eqes += num_cons + 2 * MAX_NUM_VFS_BB + n_srq;
		} else if (p_hwfn->hw_info.personality == QED_PCI_ISCSI) {
			num_cons =
			    qed_cxt_get_proto_cid_count(p_hwfn,
							PROTOCOLID_ISCSI, NULL);
			n_eqes += 2 * num_cons;
		}

		if (n_eqes > QED_EQ_MAX_ELEMENTS) {
			DP_INFO(p_hwfn, "EQs maxing out at 0x%x elements\n",
				QED_EQ_MAX_ELEMENTS);
			n_eqes = QED_EQ_MAX_ELEMENTS;
		}

		rc = qed_eq_alloc(p_hwfn, (u16) n_eqes);
		if (rc)
			goto alloc_err;

		rc = qed_consq_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

#ifdef CONFIG_QED_LL2
		if (p_hwfn->using_ll2) {
			rc = qed_ll2_alloc(p_hwfn);
			if (rc)
				goto alloc_err;
		}
#endif
		if (p_hwfn->hw_info.personality == QED_PCI_FCOE) {
			rc = qed_fcoe_alloc(p_hwfn);
			if (rc)
				goto alloc_err;
		}

		if (p_hwfn->hw_info.personality == QED_PCI_ISCSI) {
			rc = qed_iscsi_alloc(p_hwfn);
			if (rc)
				goto alloc_err;

			rc = qed_ooo_alloc(p_hwfn);
			if (rc)
				goto alloc_err;
		}
#if IS_ENABLED(CONFIG_QED_RDMA)
		if (QED_IS_RDMA_PERSONALITY(p_hwfn)) {
			rc = qed_rdma_info_alloc(p_hwfn);
			if (rc)
				goto alloc_err;
		}
#endif

		/* DMA info initialization */
		rc = qed_dmae_info_alloc(p_hwfn);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "Failed to allocate memory for dmae_info structure\n");
			goto alloc_err;
		}

		/* DCBX initialization */
		rc = qed_dcbx_info_alloc(p_hwfn);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "Failed to allocate memory for dcbx structure\n");
			goto alloc_err;
		}

		rc = qed_dbg_alloc_user_data(p_hwfn, &p_hwfn->dbg_user_info);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "Failed to allocate dbg user info structure\n");
			goto alloc_err;
		}
	}			/* hwfn loop */

	if (IS_PF(cdev)) {
		rc = qed_llh_alloc(cdev);
		if (rc) {
			DP_NOTICE(cdev,
				  "Failed to allocate memory for the llh_info structure\n");
			goto alloc_err;
		}
	}

	cdev->reset_stats = kzalloc(sizeof(*cdev->reset_stats), GFP_KERNEL);
	if (!cdev->reset_stats) {
		DP_NOTICE(cdev, "Failed to allocate reset statistics\n");
		goto alloc_no_mem;
	}

	return 0;

alloc_no_mem:
	rc = -ENOMEM;
alloc_err:
	qed_resc_free(cdev);
	return rc;
}

static int qed_fw_err_handler(struct qed_hwfn *p_hwfn,
			      u8 opcode,
			      u16 echo,
			      union event_ring_data *data, u8 fw_return_code)
{
	if (fw_return_code != COMMON_ERR_CODE_ERROR)
		goto eqe_unexpected;

	if ((data->err_data.recovery_scope == ERR_SCOPE_FUNC) &&
	    (data->err_data.entity_id >= MAX_NUM_PFS)) {
		qed_sriov_vfpf_malicious(p_hwfn, &data->err_data);
		return 0;
	}

eqe_unexpected:
	DP_ERR(p_hwfn,
	       "Skipping unexpected eqe 0x%02x, FW return code 0x%x, echo 0x%x\n",
	       opcode, fw_return_code, echo);
	return -EINVAL;
}

static int qed_common_eqe_event(struct qed_hwfn *p_hwfn,
				u8 opcode,
				__le16 echo,
				union event_ring_data *data,
				u8 fw_return_code, u8 vf_id)
{
	u16 echo_print = le16_to_cpu(echo);

	switch (opcode) {
	case COMMON_EVENT_VF_PF_CHANNEL:
	case COMMON_EVENT_VF_FLR:
		return qed_sriov_eqe_event(p_hwfn, opcode, echo_print, data,
					   fw_return_code, vf_id);
	case COMMON_EVENT_FW_ERROR:
		return qed_fw_err_handler(p_hwfn, opcode, echo_print, data,
					  fw_return_code);
	default:
		DP_INFO(p_hwfn->cdev, "Unknown eqe event 0x%02x, echo 0x%x\n",
			opcode, echo_print);
		return -EINVAL;
	}
}

void qed_resc_setup(struct qed_dev *cdev)
{
	int i;

	if (IS_VF(cdev)) {
		for_each_hwfn(cdev, i) {
			qed_l2_setup(&cdev->hwfns[i]);

#ifdef CONFIG_QED_LL2
			if (cdev->hwfns[i].using_ll2)
				qed_ll2_setup(&cdev->hwfns[i]);
#endif
		}

		return;
	}

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];

		qed_cxt_mngr_setup(p_hwfn);
		qed_spq_setup(p_hwfn);
		qed_eq_setup(p_hwfn);
		qed_consq_setup(p_hwfn);

		/* Read shadow of current MFW mailbox */
		qed_mcp_read_mb(p_hwfn, p_hwfn->p_main_ptt);
		memcpy(p_hwfn->mcp_info->mfw_mb_shadow,
		       p_hwfn->mcp_info->mfw_mb_cur,
		       p_hwfn->mcp_info->mfw_mb_length);

		qed_int_setup(p_hwfn, p_hwfn->p_main_ptt);

		qed_l2_setup(p_hwfn);
		qed_iov_setup(p_hwfn);
		qed_spq_register_async_cb(p_hwfn, PROTOCOLID_COMMON,
					  qed_common_eqe_event);
#ifdef CONFIG_QED_LL2
		if (p_hwfn->using_ll2)
			qed_ll2_setup(p_hwfn);
#endif
		if (p_hwfn->hw_info.personality == QED_PCI_FCOE)
			qed_fcoe_setup(p_hwfn);

		if (p_hwfn->hw_info.personality == QED_PCI_ISCSI) {
			qed_iscsi_setup(p_hwfn);
			qed_ooo_setup(p_hwfn);
		}
	}
}

#define FINAL_CLEANUP_POLL_CNT  (100)
#define FINAL_CLEANUP_POLL_TIME (10)
int qed_final_cleanup(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, u16 id, bool is_vf)
{
	u32 count = FINAL_CLEANUP_POLL_CNT, poll_time = FINAL_CLEANUP_POLL_TIME;
	u32 command = 0, addr;
	int rc = -EBUSY;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_TEDIBEAR(p_hwfn->cdev) ||
	    CHIP_REV_IS_SLOW(p_hwfn->cdev)) {
		DP_INFO(p_hwfn, "Skipping final cleanup for non-ASIC\n");
		return 0;
	}

	if (CHIP_REV_IS_SLOW(p_hwfn->cdev))
		poll_time *= 10;
#endif

	addr = GET_GTT_REG_ADDR(GTT_BAR0_MAP_REG_USDM_RAM,
				USTORM_FLR_FINAL_ACK, p_hwfn->rel_pf_id);

	if (is_vf)
		id += 0x10;

	SET_FIELD(command, SDM_AGG_INT_COMP_PARAMS_AGG_INT_INDEX,
		  X_FINAL_CLEANUP_AGG_INT);
	SET_FIELD(command, SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_ENABLE, 1);
	SET_FIELD(command, SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_BIT, id);
	SET_FIELD(command, SDM_OP_GEN_COMP_TYPE, SDM_COMP_TYPE_AGG_INT);

	/* Make sure notification is not set before initiating final cleanup */
	if (REG_RD(p_hwfn, addr)) {
		DP_NOTICE(p_hwfn,
			  "Unexpected; Found final cleanup notification before initiating final cleanup\n");
		REG_WR(p_hwfn, addr, 0);
	}

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Sending final cleanup for PFVF[%d] [Command %08x]\n",
		   id, command);

	qed_wr(p_hwfn, p_ptt, XSDM_REG_OPERATION_GEN, command);

	/* Poll until completion */
	while (!REG_RD(p_hwfn, addr) && count--)
		msleep(poll_time);

	if (REG_RD(p_hwfn, addr))
		rc = 0;
	else
		DP_NOTICE(p_hwfn,
			  "Failed to receive FW final cleanup notification\n");

	/* Cleanup afterwards */
	REG_WR(p_hwfn, addr, 0);

	return rc;
}

static int qed_calc_hw_mode(struct qed_hwfn *p_hwfn)
{
	int hw_mode = 0;

	if (QED_IS_BB(p_hwfn->cdev)) {
		hw_mode |= 1 << MODE_BB;
	} else if (QED_IS_AH(p_hwfn->cdev)) {
		hw_mode |= 1 << MODE_K2;
	} else {
		DP_NOTICE(p_hwfn, "Unknown chip type %#x\n",
			  p_hwfn->cdev->type);
		return -EINVAL;
	}

	/* Ports per engine is based on the values in CNIG_REG_NW_PORT_MODE */
	switch (p_hwfn->cdev->num_ports_in_engine) {
	case 1:
		hw_mode |= 1 << MODE_PORTS_PER_ENG_1;
		break;
	case 2:
		hw_mode |= 1 << MODE_PORTS_PER_ENG_2;
		break;
	case 4:
		hw_mode |= 1 << MODE_PORTS_PER_ENG_4;
		break;
	default:
		DP_NOTICE(p_hwfn, "num_ports_in_engine = %d not supported\n",
			  p_hwfn->cdev->num_ports_in_engine);
		return -EINVAL;
	}

	if (test_bit(QED_MF_OVLAN_CLSS, &p_hwfn->cdev->mf_bits))
		hw_mode |= 1 << MODE_MF_SD;
	else
		hw_mode |= 1 << MODE_MF_SI;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_SLOW(p_hwfn->cdev)) {
		if (CHIP_REV_IS_FPGA(p_hwfn->cdev)) {
			hw_mode |= 1 << MODE_FPGA;
		} else {
			if (p_hwfn->cdev->b_is_emul_full)
				hw_mode |= 1 << MODE_EMUL_FULL;
			else
				hw_mode |= 1 << MODE_EMUL_REDUCED;
		}
	} else
#endif
		hw_mode |= 1 << MODE_ASIC;

	if (QED_IS_CMT(p_hwfn->cdev))
		hw_mode |= 1 << MODE_100G;

	p_hwfn->hw_info.hw_mode = hw_mode;

	DP_VERBOSE(p_hwfn, (NETIF_MSG_PROBE | NETIF_MSG_IFUP),
		   "Configuring function for hw_mode: 0x%08x\n",
		   p_hwfn->hw_info.hw_mode);

	return 0;
}

#ifndef ASIC_ONLY
/* MFW-replacement initializations for emulation */
static int qed_hw_init_chip(struct qed_dev *cdev, struct qed_ptt *p_ptt)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	u32 pl_hv, wr_mbs;
	int i, pos;
	u16 ctrl = 0;

	if (!CHIP_REV_IS_EMUL(cdev)) {
		DP_NOTICE(cdev,
			  "qed_hw_init_chip() shouldn't be called in a non-emulation environment\n");
		return -EINVAL;
	}

	pl_hv = QED_IS_BB(cdev) ? 0x1 : 0x401;
	qed_wr(p_hwfn, p_ptt, MISCS_REG_RESET_PL_HV + 4, pl_hv);

	if (QED_IS_AH(cdev))
		qed_wr(p_hwfn, p_ptt, MISCS_REG_RESET_PL_HV_2_K2, 0x3ffffff);

	/* Initialize port mode to 4x10G_E (10G with 4x10 SERDES) */
	if (QED_IS_BB(cdev))
		qed_wr(p_hwfn, p_ptt, CNIG_REG_NW_PORT_MODE_BB, 4);

	if (QED_IS_AH(cdev)) {
		/* 2 for 4-port, 1 for 2-port, 0 for 1-port */
		qed_wr(p_hwfn, p_ptt, MISC_REG_PORT_MODE,
		       cdev->num_ports_in_engine >> 1);

		qed_wr(p_hwfn, p_ptt, MISC_REG_BLOCK_256B_EN,
		       cdev->num_ports_in_engine == 4 ? 0 : 3);
	}

	/* Signal the PSWRQ block to start initializing internal memories */
	qed_wr(p_hwfn, p_ptt, PSWRQ2_REG_RBC_DONE, 1);
	for (i = 0; i < 100; i++) {
		udelay(50);
		if (qed_rd(p_hwfn, p_ptt, PSWRQ2_REG_CFG_DONE) == 1)
			break;
	}
	if (i == 100) {
		DP_NOTICE(p_hwfn, "RBC done failed to complete in PSWRQ2\n");
		return -EBUSY;
	}

	/* Indicate PSWRQ to initialize steering tag table with zeros */
	qed_wr(p_hwfn, p_ptt, PSWRQ2_REG_RESET_STT, 1);
	for (i = 0; i < 100; i++) {
		udelay(50);
		if (!qed_rd(p_hwfn, p_ptt, PSWRQ2_REG_RESET_STT))
			break;
	}
	if (i == 100) {
		DP_NOTICE(p_hwfn,
			  "Steering tag table initialization failed to complete in PSWRQ2\n");
		return -EBUSY;
	}

	/* Clear a possible PSWRQ2 STT parity which might have been generated by
	 * a previous MSI-X read.
	 */
	qed_wr(p_hwfn, p_ptt, PSWRQ2_REG_PRTY_STS_WR_H_0, 0x8);

	/* Configure PSWRQ2_REG_WR_MBS0 according to the MaxPayloadSize field in
	 * the PCI configuration space. The value is common for all PFs, so it
	 * is okay to do it according to the first loading PF.
	 */
	pos = pci_find_capability(cdev->pdev, PCI_CAP_ID_EXP);
	if (!pos) {
		DP_NOTICE(cdev,
			  "Failed to find the PCI Express Capability structure in the PCI config space\n");
		return -EIO;
	}

	pci_read_config_word(cdev->pdev, pos + PCI_EXP_DEVCTL, &ctrl);
	wr_mbs = (ctrl & PCI_EXP_DEVCTL_PAYLOAD) >> 5;
	qed_wr(p_hwfn, p_ptt, PSWRQ2_REG_WR_MBS0, wr_mbs);

	/* Configure the PGLUE_B to discard mode */
	qed_wr(p_hwfn, p_ptt, PGLUE_B_REG_MASTER_DISCARD_NBLOCK, 0x3f);

	return 0;
}
#endif

/* Init run time data for all PFs and their VFs on an engine.
 * TBD - for VFs - Once we have parent PF info for each VF in
 * shmem available as CAU requires knowledge of parent PF for each VF.
 */
static void qed_init_cau_rt_data(struct qed_dev *cdev)
{
	u32 offset = CAU_REG_SB_VAR_MEMORY_RT_OFFSET;
	u16 igu_sb_id;
	int i;

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];
		struct qed_igu_info *p_igu_info;
		struct qed_igu_block *p_block;
		struct cau_sb_entry sb_entry;

		p_igu_info = p_hwfn->hw_info.p_igu_info;

		for (igu_sb_id = 0;
		     igu_sb_id < QED_MAPPING_MEMORY_SIZE(cdev); igu_sb_id++) {
			p_block = &p_igu_info->entry[igu_sb_id];

			if (!p_block->is_pf)
				continue;

			qed_init_cau_sb_entry(p_hwfn, &sb_entry,
					      p_block->function_id, 0, 0);
			STORE_RT_REG_AGG(p_hwfn, offset + igu_sb_id * 2,
					 sb_entry);
		}
	}
}

static void qed_init_cache_line_size(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt)
{
	u32 val, wr_mbs, cache_line_size;

	val = qed_rd(p_hwfn, p_ptt, PSWRQ2_REG_WR_MBS0);
	switch (val) {
	case 0:
		wr_mbs = 128;
		break;
	case 1:
		wr_mbs = 256;
		break;
	case 2:
		wr_mbs = 512;
		break;
	default:
		DP_INFO(p_hwfn,
			"Unexpected value of PSWRQ2_REG_WR_MBS0 [0x%x]. Avoid configuring PGLUE_B_REG_CACHE_LINE_SIZE.\n",
			val);
		return;
	}

	cache_line_size = min_t(u32, L1_CACHE_BYTES, wr_mbs);
	switch (cache_line_size) {
	case 32:
		val = 0;
		break;
	case 64:
		val = 1;
		break;
	case 128:
		val = 2;
		break;
	case 256:
		val = 3;
		break;
	default:
		DP_INFO(p_hwfn,
			"Unexpected value of cache line size [0x%x]. Avoid configuring PGLUE_B_REG_CACHE_LINE_SIZE.\n",
			cache_line_size);
	}

	if (L1_CACHE_BYTES > wr_mbs)
		DP_INFO(p_hwfn,
			"The cache line size for padding is suboptimal for performance [OS cache line size 0x%x, wr mbs 0x%x]\n",
			L1_CACHE_BYTES, wr_mbs);

	STORE_RT_REG(p_hwfn, PGLUE_REG_B_CACHE_LINE_SIZE_RT_OFFSET, val);
	if (val > 0) {
		STORE_RT_REG(p_hwfn, PSWRQ2_REG_DRAM_ALIGN_WR_RT_OFFSET, val);
		STORE_RT_REG(p_hwfn, PSWRQ2_REG_DRAM_ALIGN_RD_RT_OFFSET, val);
	}
}

static int qed_hw_init_common(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, int hw_mode)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	struct qed_dev *cdev = p_hwfn->cdev;
	u8 vf_id, max_num_vfs;
	u16 num_pfs, pf_id;
	u32 concrete_fid;
	int rc = 0;

	qed_init_cau_rt_data(cdev);

	/* Program GTT windows */
	qed_gtt_init(p_hwfn);

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(cdev) && IS_LEAD_HWFN(p_hwfn)) {
		rc = qed_hw_init_chip(cdev, p_ptt);
		if (rc)
			return rc;
	}
#endif

	if (p_hwfn->mcp_info) {
		if (p_hwfn->mcp_info->func_info.bandwidth_max)
			qm_info->pf_rl_en = 1;
		if (p_hwfn->mcp_info->func_info.bandwidth_min)
			qm_info->pf_wfq_en = 1;
	}

	qed_qm_common_rt_init(p_hwfn,
			      cdev->num_ports_in_engine,
			      qm_info->max_phys_tcs_per_port,
			      qm_info->pf_rl_en, qm_info->pf_wfq_en,
			      qm_info->vport_rl_en, qm_info->vport_wfq_en,
			      qm_info->qm_port_params,
			      NULL /* global RLs are not configured */ );

	qed_cxt_hw_init_common(p_hwfn);

	qed_init_cache_line_size(p_hwfn, p_ptt);

	rc = qed_init_run(p_hwfn, p_ptt, PHASE_ENGINE, QED_PATH_ID(p_hwfn),
			  hw_mode);
	if (rc)
		return rc;

	/* @@TBD MichalK - should add VALIDATE_VFID to init tool...
	 * need to decide with which value, maybe runtime
	 */
	qed_wr(p_hwfn, p_ptt, PSWRQ2_REG_L2P_VALIDATE_VFID, 0);
	qed_wr(p_hwfn, p_ptt, PGLUE_B_REG_USE_CLIENTID_IN_TAG, 1);

	if (QED_IS_BB(cdev)) {
		/* Workaround clears ROCE search for all functions to prevent
		 * involving non initialized function in processing ROCE packet.
		 */
		num_pfs = (u16) NUM_OF_ENG_PFS(cdev);
		for (pf_id = 0; pf_id < num_pfs; pf_id++) {
			qed_fid_pretend(p_hwfn, p_ptt, pf_id);
			qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_ROCE, 0x0);
			qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_TCP, 0x0);
		}
		/* pretend to original PF */
		qed_fid_pretend(p_hwfn, p_ptt, p_hwfn->rel_pf_id);
	}

	/* Workaround for avoiding CCFC execution error when getting packets
	 * with CRC errors, and allowing instead the invoking of the FW error
	 * handler.
	 * This is not done inside the init tool since it currently can't
	 * perform a pretending to VFs.
	 */
	max_num_vfs = (u8) NUM_OF_VFS(cdev);
	for (vf_id = 0; vf_id < max_num_vfs; vf_id++) {
		concrete_fid = qed_vfid_to_concrete(p_hwfn, vf_id);
		qed_fid_pretend(p_hwfn, p_ptt, (u16) concrete_fid);
		qed_wr(p_hwfn, p_ptt, CCFC_REG_STRONG_ENABLE_VF, 0x1);
		qed_wr(p_hwfn, p_ptt, CCFC_REG_WEAK_ENABLE_VF, 0x0);
		qed_wr(p_hwfn, p_ptt, TCFC_REG_STRONG_ENABLE_VF, 0x1);
		qed_wr(p_hwfn, p_ptt, TCFC_REG_WEAK_ENABLE_VF, 0x0);
		/* mark vf channel as dead */
		qed_wr(p_hwfn, p_ptt, YSEM_REG_FAST_MEMORY +
		       SEM_FAST_REG_INT_RAM + YSTORM_VF_ZONE_OFFSET(vf_id),
		       0xdead);
	}
	/* pretend to original PF */
	qed_fid_pretend(p_hwfn, p_ptt, p_hwfn->rel_pf_id);

	return rc;
}

#ifndef ASIC_ONLY
#define MISC_REG_RESET_REG_2_XMAC_BIT (1 << 4)
#define MISC_REG_RESET_REG_2_XMAC_SOFT_BIT (1 << 5)

#define PMEG_IF_BYTE_COUNT      8

static void qed_wr_nw_port(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt,
			   u32 addr, u64 data, u8 reg_type, u8 port)
{
	DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
		   "CMD: %08x, ADDR: 0x%08x, DATA: %08x:%08x\n",
		   qed_rd(p_hwfn, p_ptt, CNIG_REG_PMEG_IF_CMD_BB) |
		   (8 << PMEG_IF_BYTE_COUNT),
		   (reg_type << 25) | (addr << 8) | port,
		   (u32) ((data >> 32) & 0xffffffff),
		   (u32) (data & 0xffffffff));

	qed_wr(p_hwfn, p_ptt, CNIG_REG_PMEG_IF_CMD_BB,
	       (qed_rd(p_hwfn, p_ptt, CNIG_REG_PMEG_IF_CMD_BB) &
		0xffff00fe) | (8 << PMEG_IF_BYTE_COUNT));
	qed_wr(p_hwfn, p_ptt, CNIG_REG_PMEG_IF_ADDR_BB,
	       (reg_type << 25) | (addr << 8) | port);
	qed_wr(p_hwfn, p_ptt, CNIG_REG_PMEG_IF_WRDATA_BB, data & 0xffffffff);
	qed_wr(p_hwfn, p_ptt, CNIG_REG_PMEG_IF_WRDATA_BB,
	       (data >> 32) & 0xffffffff);
}

#define XLPORT_MODE_REG (0x20a)
#define XLPORT_MAC_CONTROL (0x210)
#define XLPORT_FLOW_CONTROL_CONFIG (0x207)
#define XLPORT_ENABLE_REG (0x20b)

#define XLMAC_CTRL (0x600)
#define XLMAC_MODE (0x601)
#define XLMAC_RX_MAX_SIZE (0x608)
#define XLMAC_TX_CTRL (0x604)
#define XLMAC_PAUSE_CTRL (0x60d)
#define XLMAC_PFC_CTRL (0x60e)

static void qed_emul_link_init_bb(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt)
{
	u8 loopback = 0, port = p_hwfn->port_id * 2;

	qed_wr_nw_port(p_hwfn, p_ptt, XLPORT_MODE_REG, (0x4 << 4) | 0x4, 1, port);	/* XLPORT MAC MODE *//* 0 Quad, 4 Single... */
	qed_wr_nw_port(p_hwfn, p_ptt, XLPORT_MAC_CONTROL, 0, 1, port);
	qed_wr_nw_port(p_hwfn, p_ptt, XLMAC_CTRL, 0x40, 0, port);	/*XLMAC: SOFT RESET */
	qed_wr_nw_port(p_hwfn, p_ptt, XLMAC_MODE, 0x40, 0, port);	/*XLMAC: Port Speed >= 10Gbps */
	qed_wr_nw_port(p_hwfn, p_ptt, XLMAC_RX_MAX_SIZE, 0x3fff, 0, port);	/* XLMAC: Max Size */
	qed_wr_nw_port(p_hwfn, p_ptt, XLMAC_TX_CTRL,
		       0x01000000800ULL | (0xa << 12) | ((u64) 1 << 38),
		       0, port);
	qed_wr_nw_port(p_hwfn, p_ptt, XLMAC_PAUSE_CTRL, 0x7c000, 0, port);
	qed_wr_nw_port(p_hwfn, p_ptt, XLMAC_PFC_CTRL, 0x30ffffc000ULL, 0, port);
	qed_wr_nw_port(p_hwfn, p_ptt, XLMAC_CTRL, 0x3 | (loopback << 2), 0, port);	/* XLMAC: TX_EN, RX_EN */
	qed_wr_nw_port(p_hwfn, p_ptt, XLMAC_CTRL, 0x1003 | (loopback << 2), 0, port);	/* XLMAC: TX_EN, RX_EN, SW_LINK_STATUS */
	qed_wr_nw_port(p_hwfn, p_ptt, XLPORT_FLOW_CONTROL_CONFIG, 1, 0, port);	/* Enabled Parallel PFC interface */
	qed_wr_nw_port(p_hwfn, p_ptt, XLPORT_ENABLE_REG, 0xf, 1, port);	/* XLPORT port enable */
}

static void qed_emul_link_init_ah(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt)
{
	u32 mac_base, mac_config_val = 0xa853;
	u8 port = p_hwfn->port_id;

	qed_wr(p_hwfn, p_ptt, CNIG_REG_NIG_PORT0_CONF_K2 + (port << 2),
	       BIT(CNIG_REG_NIG_PORT0_CONF_NIG_PORT_ENABLE_0_K2_SHIFT) |
	       (port <<
		CNIG_REG_NIG_PORT0_CONF_NIG_PORT_NWM_PORT_MAP_0_K2_SHIFT) |
	       (0 << CNIG_REG_NIG_PORT0_CONF_NIG_PORT_RATE_0_K2_SHIFT));

	mac_base = NWM_REG_MAC0_K2 + (port << 2) * NWM_REG_MAC0_SIZE;

	qed_wr(p_hwfn, p_ptt, mac_base + ETH_MAC_REG_XIF_MODE_K2,
	       1 << ETH_MAC_REG_XIF_MODE_XGMII_K2_SHIFT);

	qed_wr(p_hwfn, p_ptt, mac_base + ETH_MAC_REG_FRM_LENGTH_K2,
	       9018 << ETH_MAC_REG_FRM_LENGTH_FRM_LENGTH_K2_SHIFT);

	qed_wr(p_hwfn, p_ptt, mac_base + ETH_MAC_REG_TX_IPG_LENGTH_K2,
	       0xc << ETH_MAC_REG_TX_IPG_LENGTH_TXIPG_K2_SHIFT);

	qed_wr(p_hwfn, p_ptt, mac_base + ETH_MAC_REG_RX_FIFO_SECTIONS_K2,
	       8 << ETH_MAC_REG_RX_FIFO_SECTIONS_RX_SECTION_FULL_K2_SHIFT);

	qed_wr(p_hwfn, p_ptt, mac_base + ETH_MAC_REG_TX_FIFO_SECTIONS_K2,
	       (0xA <<
		ETH_MAC_REG_TX_FIFO_SECTIONS_TX_SECTION_EMPTY_K2_SHIFT) |
	       (8 << ETH_MAC_REG_TX_FIFO_SECTIONS_TX_SECTION_FULL_K2_SHIFT));

	/* Strip the CRC field from the frame */
	mac_config_val &= ~ETH_MAC_REG_COMMAND_CONFIG_CRC_FWD_K2;
	qed_wr(p_hwfn, p_ptt, mac_base + ETH_MAC_REG_COMMAND_CONFIG_K2,
	       mac_config_val);
}

static void qed_emul_link_init(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u8 port = QED_IS_BB(p_hwfn->cdev) ? p_hwfn->port_id * 2
	    : p_hwfn->port_id;

	DP_INFO(p_hwfn->cdev, "Emulation: Configuring Link [port %02x]\n",
		port);

	if (QED_IS_BB(p_hwfn->cdev))
		qed_emul_link_init_bb(p_hwfn, p_ptt);
	else
		qed_emul_link_init_ah(p_hwfn, p_ptt);

	return;
}

static void qed_link_init_bb(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, u8 port)
{
	int port_offset = port ? 0x800 : 0;
	u32 xmac_rxctrl = 0;

	/* Reset of XMAC */
	/* FIXME: move to common start */
	qed_wr(p_hwfn, p_ptt, MISC_REG_RESET_PL_PDA_VAUX + 2 * sizeof(u32), MISC_REG_RESET_REG_2_XMAC_BIT);	/* Clear */
	usleep_range(1000, 2000);
	qed_wr(p_hwfn, p_ptt, MISC_REG_RESET_PL_PDA_VAUX + sizeof(u32), MISC_REG_RESET_REG_2_XMAC_BIT);	/* Set */

	qed_wr(p_hwfn, p_ptt, MISC_REG_XMAC_CORE_PORT_MODE_BB, 1);

	/* Set the number of ports on the Warp Core to 10G */
	qed_wr(p_hwfn, p_ptt, MISC_REG_XMAC_PHY_PORT_MODE_BB, 3);

	/* Soft reset of XMAC */
	qed_wr(p_hwfn, p_ptt, MISC_REG_RESET_PL_PDA_VAUX + 2 * sizeof(u32),
	       MISC_REG_RESET_REG_2_XMAC_SOFT_BIT);
	usleep_range(1000, 2000);
	qed_wr(p_hwfn, p_ptt, MISC_REG_RESET_PL_PDA_VAUX + sizeof(u32),
	       MISC_REG_RESET_REG_2_XMAC_SOFT_BIT);

	/* FIXME: move to common end */
	if (CHIP_REV_IS_FPGA(p_hwfn->cdev))
		qed_wr(p_hwfn, p_ptt, XMAC_REG_MODE_BB + port_offset, 0x20);

	/* Set Max packet size: initialize XMAC block register for port 0 */
	qed_wr(p_hwfn, p_ptt, XMAC_REG_RX_MAX_SIZE_BB + port_offset, 0x2710);

	/* CRC append for Tx packets: init XMAC block register for port 1 */
	qed_wr(p_hwfn, p_ptt, XMAC_REG_TX_CTRL_LO_BB + port_offset, 0xC800);

	/* Enable TX and RX: initialize XMAC block register for port 1 */
	qed_wr(p_hwfn, p_ptt, XMAC_REG_CTRL_BB + port_offset,
	       XMAC_REG_CTRL_TX_EN_BB | XMAC_REG_CTRL_RX_EN_BB);
	xmac_rxctrl = qed_rd(p_hwfn, p_ptt, XMAC_REG_RX_CTRL_BB + port_offset);
	xmac_rxctrl |= XMAC_REG_RX_CTRL_PROCESS_VARIABLE_PREAMBLE_BB;
	qed_wr(p_hwfn, p_ptt, XMAC_REG_RX_CTRL_BB + port_offset, xmac_rxctrl);
}
#endif

static u32 qed_hw_get_norm_region_conn(struct qed_hwfn *p_hwfn)
{
	u32 norm_region_conn;

	/* The order of CIDs allocation is according to the order of
	 * 'enum protocol_type'. Therefore, the number of CIDs for the normal
	 * region is calculated based on the CORE CIDs, in case of non-ETH
	 * personality, and otherwise - based on the ETH CIDs.
	 */
	norm_region_conn =
	    qed_cxt_get_proto_cid_start(p_hwfn, PROTOCOLID_CORE,
					QED_CXT_PF_CID) +
	    qed_cxt_get_proto_cid_count(p_hwfn, PROTOCOLID_CORE,
					NULL) +
	    qed_cxt_get_proto_cid_count(p_hwfn, PROTOCOLID_ETH, NULL);

	return norm_region_conn;
}

int
qed_hw_init_dpi_size(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt,
		     struct qed_dpi_info *dpi_info,
		     u32 pwm_region_size, u32 n_cpus)
{
	u32 dpi_bit_shift, dpi_count, dpi_page_size;
	u32 min_dpis;
	u32 n_wids;

	/* Calculate DPI size
	 * ------------------
	 * The PWM region contains Doorbell Pages. The first is reserverd for
	 * the kernel for, e.g, L2. The others are free to be used by non-
	 * trusted applications, typically from user space. Each page, called a
	 * doorbell page is sectioned into windows that allow doorbells to be
	 * issued in parallel by the kernel/application. The size of such a
	 * window (a.k.a. WID) is 1kB.
	 * Summary:
	 *    1kB WID x N WIDS = DPI page size
	 *    DPI page size x N DPIs = PWM region size
	 * Notes:
	 * The size of the DPI page size must be in multiples of PAGE_SIZE
	 * in order to ensure that two applications won't share the same page.
	 * It also must contain at least one WID per CPU to allow parallelism.
	 * It also must be a power of 2, since it is stored as a bit shift.
	 *
	 * The DPI page size is stored in a register as 'dpi_bit_shift' so that
	 * 0 is 4kB, 1 is 8kB and etc. Hence the minimum size is 4,096
	 * containing 4 WIDs.
	 */
	n_wids = max_t(u32, QED_MIN_WIDS, n_cpus);
	dpi_page_size = QED_WID_SIZE * roundup_pow_of_two(n_wids);
	dpi_page_size = (dpi_page_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	dpi_bit_shift = ilog2(dpi_page_size / 4096);
	dpi_count = pwm_region_size / dpi_page_size;

	min_dpis = p_hwfn->pf_params.rdma_pf_params.min_dpis;
	min_dpis = max_t(u32, QED_MIN_DPIS, min_dpis);

	dpi_info->dpi_size = dpi_page_size;
	dpi_info->dpi_count = dpi_count;
	qed_wr(p_hwfn, p_ptt, dpi_info->dpi_bit_shift_addr, dpi_bit_shift);

	if (dpi_count < min_dpis)
		return -EINVAL;

	return 0;
}

bool qed_edpm_enabled(struct qed_hwfn * p_hwfn)
{
	if (p_hwfn->dcbx_no_edpm || p_hwfn->db_bar_no_edpm)
		return false;

	return true;
}

static int
qed_hw_init_pf_doorbell_bar(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_dpi_info *dpi_info;
	u32 norm_region_conn, min_addr_reg1;
	u32 pwm_regsize, norm_regsize;
	u32 db_bar_size, n_cpus = 1;
	u32 roce_edpm_mode;
	u32 dems_shift;
	int rc = 0;
	u8 cond;

	db_bar_size = qed_hw_bar_size(p_hwfn, p_ptt, BAR_ID_1);

	/* In CMT, doorbell bar should be split over both engines */
	if (QED_IS_CMT(p_hwfn->cdev))
		db_bar_size /= 2;

	/* Calculate doorbell regions
	 * --------------------------
	 * The doorbell BAR is made of two regions. The first is called normal
	 * region and the second is called PWM region. In the normal region
	 * each ICID has its own set of addresses so that writing to that
	 * specific address identifies the ICID. In the Process Window Mode
	 * region the ICID is given in the data written to the doorbell. The
	 * above per PF register denotes the offset in the doorbell BAR in which
	 * the PWM region begins.
	 * The normal region has QED_PF_DEMS_SIZE bytes per ICID, that is per
	 * non-PWM connection. The calculation below computes the total non-PWM
	 * connections. The DORQ_REG_PF_MIN_ADDR_REG1 register is
	 * in units of 4,096 bytes.
	 */
	norm_region_conn = qed_hw_get_norm_region_conn(p_hwfn);
	norm_regsize = roundup(QED_PF_DEMS_SIZE * norm_region_conn, PAGE_SIZE);
	min_addr_reg1 = norm_regsize / 4096;
	pwm_regsize = db_bar_size - norm_regsize;

	/* Check that the normal and PWM sizes are valid */
	if (db_bar_size < norm_regsize) {
		DP_ERR(p_hwfn->cdev,
		       "Doorbell BAR size 0x%x is too small (normal region is 0x%0x )\n",
		       db_bar_size, norm_regsize);
		return -EINVAL;
	}
	if (pwm_regsize < QED_MIN_PWM_REGION) {
		DP_ERR(p_hwfn->cdev,
		       "PWM region size 0x%0x is too small. Should be at least 0x%0x (Doorbell BAR size is 0x%x and normal region size is 0x%0x)\n",
		       pwm_regsize,
		       QED_MIN_PWM_REGION, db_bar_size, norm_regsize);
		return -EINVAL;
	}

	dpi_info = &p_hwfn->dpi_info;

	dpi_info->dpi_bit_shift_addr = DORQ_REG_PF_DPI_BIT_SHIFT;

	if (p_hwfn->roce_edpm_mode <= QED_ROCE_EDPM_MODE_DISABLE) {
		roce_edpm_mode = p_hwfn->roce_edpm_mode;
	} else {
		DP_ERR(p_hwfn->cdev,
		       "roce edpm mode was configured to an illegal value of %u. Resetting it to 0-Enable EDPM if BAR size is adequate\n",
		       p_hwfn->roce_edpm_mode);
		roce_edpm_mode = 0;
	}

	if ((roce_edpm_mode == QED_ROCE_EDPM_MODE_ENABLE) ||
	    ((roce_edpm_mode == QED_ROCE_EDPM_MODE_FORCE_ON))) {
		/* Either EDPM is mandatory, or we are attempting to allocate a
		 * WID per CPU.
		 */
		n_cpus = num_present_cpus();
		rc = qed_hw_init_dpi_size(p_hwfn, p_ptt, dpi_info,
					  pwm_regsize, n_cpus);
	}

	cond = ((rc != 0) &&
		(roce_edpm_mode == QED_ROCE_EDPM_MODE_ENABLE)) ||
	    (roce_edpm_mode == QED_ROCE_EDPM_MODE_DISABLE);
	if (cond || p_hwfn->dcbx_no_edpm) {
		/* Either EDPM is disabled from user configuration, or it is
		 * disabled via DCBx, or it is not mandatory and we failed to
		 * allocated a WID per CPU.
		 */
		n_cpus = 1;
		rc = qed_hw_init_dpi_size(p_hwfn, p_ptt, dpi_info,
					  pwm_regsize, n_cpus);

#if IS_ENABLED(CONFIG_QED_RDMA)
		/* If we entered this flow due to DCBX then the DPM register is
		 * already configured.
		 */
		if (cond)
			qed_rdma_dpm_bar(p_hwfn, p_ptt);
#endif
	}

	dpi_info->wid_count = (u16) n_cpus;

	/* Check return codes from above calls */
	if (rc) {
		DP_ERR(p_hwfn,
		       "Failed to allocate enough DPIs. Allocated %d but the current minimum is set to %d. You can reduce this minimum down to %d via the module parameter min_rdma_dpis or by disabling EDPM by setting the module parameter roce_edpm to 2\n",
		       dpi_info->dpi_count,
		       p_hwfn->pf_params.rdma_pf_params.min_dpis, QED_MIN_DPIS);
		DP_ERR(p_hwfn,
		       "PF doorbell bar: normal_region_size=0x%x, pwm_region_size=0x%x, dpi_size=0x%x, dpi_count=%d, roce_edpm=%s, page_size=%lu\n",
		       norm_regsize,
		       pwm_regsize,
		       dpi_info->dpi_size,
		       dpi_info->dpi_count,
		       (!qed_edpm_enabled(p_hwfn)) ?
		       "disabled" : "enabled", PAGE_SIZE);

		return -EINVAL;
	}

	DP_INFO(p_hwfn,
		"PF doorbell bar: db_bar_size=0x%x normal_region_size=0x%x, pwm_region_size=0x%x, dpi_size=0x%x, dpi_count=%d, roce_edpm=%s, page_size=%lu\n",
		db_bar_size,
		norm_regsize,
		pwm_regsize,
		dpi_info->dpi_size,
		dpi_info->dpi_count,
		(!qed_edpm_enabled(p_hwfn)) ?
		"disabled" : "enabled", PAGE_SIZE);

	/* Update hwfn */
	dpi_info->dpi_start_offset = norm_regsize;	/* this is later used to
							 * calculate the doorbell
							 * address
							 */

	/* Update the DORQ registers.
	 * DEMS size is configured as log2 of DWORDs, hence the division by 4.
	 */

	dems_shift = ilog2(QED_PF_DEMS_SIZE / 4);
	qed_wr(p_hwfn, p_ptt, DORQ_REG_PF_ICID_BIT_SHIFT_NORM, dems_shift);
	qed_wr(p_hwfn, p_ptt, DORQ_REG_PF_MIN_ADDR_REG1, min_addr_reg1);

	return 0;
}

static void qed_set_port_bw_default(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt)
{
	qed_wr(p_hwfn, p_ptt, PRS_REG_WFQ_PORT_ARB_CREDIT_UPPER_BOUND, 0x4bc8);
	qed_wr(p_hwfn, p_ptt, PRS_REG_WFQ_PORT_ARB_CREDIT_WEIGHT, 0x25e4);
}

static void qed_set_port_bw_high(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	qed_wr(p_hwfn, p_ptt, PRS_REG_WFQ_PORT_ARB_CREDIT_UPPER_BOUND, 0x1900);
	qed_wr(p_hwfn, p_ptt, PRS_REG_WFQ_PORT_ARB_CREDIT_WEIGHT, 0xC80);
}

static void qed_set_port_bw_low(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	qed_wr(p_hwfn, p_ptt, PRS_REG_WFQ_PORT_ARB_CREDIT_UPPER_BOUND, 0x6);
	qed_wr(p_hwfn, p_ptt, PRS_REG_WFQ_PORT_ARB_CREDIT_WEIGHT, 0x3);
}

static void qed_rx_asymmetric_bw(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u8 rx_asymmetric_bw_mode =
	    p_hwfn->pf_params.eth_pf_params.rx_asymmetric_bw_mode;
	u32 num_ports = qed_device_num_ports(p_hwfn->cdev);
	struct qed_dev *pdev = p_hwfn->cdev;
	u8 port_id;

	port_id = QED_LEADING_HWFN(pdev)->port_id;

	DP_VERBOSE(p_hwfn,
		   NETIF_MSG_HW,
		   "Asymmetric bandwidth mode configured:%d port:%d\n",
		   rx_asymmetric_bw_mode, port_id);

	switch (rx_asymmetric_bw_mode) {
	case 0:		/* fair treatment for all ports (default value of module parameter) */
		qed_set_port_bw_default(p_hwfn, p_ptt);
		break;
	case 1:		/* 1 low indexed ports get high bandwidth [2port device: 0 is high,
				 * 1 is low. 4 port device: 0 & 1 are high, 2 & 3 are low]
				 */
		(port_id < num_ports / 2) ? qed_set_port_bw_high(p_hwfn,
								 p_ptt) :
		    qed_set_port_bw_low(p_hwfn, p_ptt);
		break;
	case 2:		/* high indexed ports get high bandwidth [2port device: 1 is high,
				 * 0 is low. 4 port device: 2 & 3 are high, 0 & 1 are low]
				 */
		(port_id < num_ports / 2) ? qed_set_port_bw_low(p_hwfn,
								p_ptt) :
		    qed_set_port_bw_high(p_hwfn, p_ptt);
		break;
	case 3:		/* even indexed ports get high bandwidth [2port device: 0 is high,
				 * 1 is low. 4 port device: 0 & 2 are high, 1 & 3 are low]
				 */
		((port_id + 1) % 2 == 0) ? qed_set_port_bw_low(p_hwfn, p_ptt) :
		    qed_set_port_bw_high(p_hwfn, p_ptt);
		break;
	case 4:		/* odd indexed ports get high bandwidth [2port device: 1 is high,
				 * 0 is low. 4 port device: 1 & 3 are high, 0 & 2 are low]
				 */
		((port_id + 1) % 2 == 0) ? qed_set_port_bw_high(p_hwfn,
								p_ptt) :
		    qed_set_port_bw_low(p_hwfn, p_ptt);
		break;
	default:
		DP_ERR(p_hwfn, "invalid rx asymmetric bandwitdh mode:%d\n",
		       rx_asymmetric_bw_mode);
	}
}

static int qed_hw_init_port(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, int hw_mode)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	int rc = 0;

	/* In CMT the gate should be cleared by the 2nd hwfn */
	if (!QED_IS_CMT(cdev) || !IS_LEAD_HWFN(p_hwfn))
		STORE_RT_REG(p_hwfn, NIG_REG_BRB_GATE_DNTFWD_PORT_RT_OFFSET, 0);

	rc = qed_init_run(p_hwfn, p_ptt, PHASE_PORT, p_hwfn->port_id, hw_mode);
	if (rc)
		return rc;

	qed_wr(p_hwfn, p_ptt, PGLUE_B_REG_MASTER_WRITE_PAD_ENABLE, 0);

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_FPGA(cdev) && QED_IS_BB(cdev))
		qed_link_init_bb(p_hwfn, p_ptt, p_hwfn->port_id);

	if (CHIP_REV_IS_EMUL(cdev)) {
		if (QED_IS_CMT(cdev)) {
			/* Activate OPTE in CMT */
			u32 val;

			val = qed_rd(p_hwfn, p_ptt, MISCS_REG_RESET_PL_HV);
			val |= 0x10;
			qed_wr(p_hwfn, p_ptt, MISCS_REG_RESET_PL_HV, val);
			qed_wr(p_hwfn, p_ptt, MISC_REG_CLK_100G_MODE, 1);
			qed_wr(p_hwfn, p_ptt, MISCS_REG_CLK_100G_MODE, 1);
			qed_wr(p_hwfn, p_ptt, MISC_REG_OPTE_MODE, 1);
			qed_wr(p_hwfn, p_ptt,
			       NIG_REG_LLH_ENG_CLS_TCP_4_TUPLE_SEARCH, 1);
			qed_wr(p_hwfn, p_ptt,
			       NIG_REG_LLH_ENG_CLS_ENG_ID_TBL, 0x55555555);
			qed_wr(p_hwfn, p_ptt,
			       NIG_REG_LLH_ENG_CLS_ENG_ID_TBL + 0x4,
			       0x55555555);
		}

		/* Set the TAGMAC default function on the port if needed.
		 * The ppfid should be set in the vector, except in BB which has
		 * a bug in the LLH where the ppfid is actually engine based.
		 */
		if (test_bit(QED_MF_NEED_DEF_PF, &cdev->mf_bits)) {
			u8 pf_id = p_hwfn->rel_pf_id;

			if (!QED_IS_BB(cdev))
				pf_id /= cdev->num_ports_in_engine;
			qed_wr(p_hwfn, p_ptt,
			       NIG_REG_LLH_TAGMAC_DEF_PF_VECTOR, 1 << pf_id);
		}

		qed_emul_link_init(p_hwfn, p_ptt);
	}
#endif
	/* Set aysmmetric bandwidth to ports */
	qed_rx_asymmetric_bw(p_hwfn, p_ptt);

	return 0;
}

static int
qed_hw_init_pf(struct qed_hwfn *p_hwfn,
	       struct qed_ptt *p_ptt,
	       int hw_mode, struct qed_hw_init_params *p_params)
{
	int rc = 0;
	u8 rel_pf_id = p_hwfn->rel_pf_id;
	u16 ctrl = 0;
	u32 prs_reg;
	int pos;

	if (p_hwfn->mcp_info) {
		struct qed_mcp_function_info *p_info;

		p_info = &p_hwfn->mcp_info->func_info;
		if (p_info->bandwidth_min)
			p_hwfn->qm_info.pf_wfq = p_info->bandwidth_min;

		/* Update rate limit once we'll actually have a link */
		p_hwfn->qm_info.pf_rl = 100000;
	}

	qed_cxt_hw_init_pf(p_hwfn, p_ptt);

	qed_int_igu_init_rt(p_hwfn);

	/* Set VLAN in NIG if needed */
	if (hw_mode & BIT(MODE_MF_SD)) {
		DP_VERBOSE(p_hwfn, NETIF_MSG_HW, "Configuring LLH_FUNC_TAG\n");
		STORE_RT_REG(p_hwfn, NIG_REG_LLH_FUNC_TAG_EN_RT_OFFSET, 1);
		STORE_RT_REG(p_hwfn, NIG_REG_LLH_FUNC_TAG_VALUE_RT_OFFSET,
			     p_hwfn->hw_info.ovlan);

		DP_VERBOSE(p_hwfn, NETIF_MSG_HW,
			   "Configuring LLH_FUNC_FILTER_HDR_SEL\n");
		STORE_RT_REG(p_hwfn, NIG_REG_LLH_FUNC_FILTER_HDR_SEL_RT_OFFSET,
			     1);
	}

	/* Enable classification by MAC if needed */
	if (hw_mode & BIT(MODE_MF_SI)) {
		DP_VERBOSE(p_hwfn,
			   NETIF_MSG_HW, "Configuring TAGMAC_CLS_TYPE\n");
		STORE_RT_REG(p_hwfn,
			     NIG_REG_LLH_FUNC_TAGMAC_CLS_TYPE_RT_OFFSET, 1);
	}

	/* Protocl Configuration  - @@@TBD - should we set 0 otherwise? */
	STORE_RT_REG(p_hwfn, PRS_REG_SEARCH_TCP_RT_OFFSET,
		     (p_hwfn->hw_info.personality == QED_PCI_ISCSI) ? 1 : 0);
	STORE_RT_REG(p_hwfn, PRS_REG_SEARCH_FCOE_RT_OFFSET,
		     (p_hwfn->hw_info.personality == QED_PCI_FCOE) ? 1 : 0);
	STORE_RT_REG(p_hwfn, PRS_REG_SEARCH_ROCE_RT_OFFSET, 0);

	/* perform debug configuration when chip is out of reset */
	qed_copy_preconfig_to_bus((void *)p_hwfn->cdev, p_hwfn->my_id);

	/* Sanity check before the PF init sequence that uses DMAE */
	rc = qed_dmae_sanity(p_hwfn, p_ptt, "pf_phase");
	if (rc)
		return rc;

	/* PF Init sequence */
	rc = qed_init_run(p_hwfn, p_ptt, PHASE_PF, rel_pf_id, hw_mode);
	if (rc)
		return rc;

	/* QM_PF Init sequence (may be invoked separately e.g. for DCB) */
	rc = qed_init_run(p_hwfn, p_ptt, PHASE_QM_PF, rel_pf_id, hw_mode);
	if (rc)
		return rc;

	qed_fw_overlay_init_ram(p_hwfn, p_ptt, p_hwfn->fw_overlay_mem);

	/* Pure runtime initializations - directly to the HW  */
	qed_int_igu_init_pure_rt(p_hwfn, p_ptt, true, true);

	/* PCI relaxed ordering is generally beneficial for performance,
	 * but can hurt performance or lead to instability on some setups.
	 * If management FW is taking care of it go with that, otherwise
	 * disable to be on the safe side.
	 */
	pos = pci_find_capability(p_hwfn->cdev->pdev, PCI_CAP_ID_EXP);
	if (!pos) {
		DP_NOTICE(p_hwfn,
			  "Failed to find the PCI Express Capability structure in the PCI config space\n");

		return -EIO;
	}

	pci_read_config_word(p_hwfn->cdev->pdev, pos + PCI_EXP_DEVCTL, &ctrl);

	if (p_params->pci_rlx_odr_mode == QED_ENABLE_RLX_ODR) {
		ctrl |= PCI_EXP_DEVCTL_RELAX_EN;
		pci_write_config_word(p_hwfn->cdev->pdev,
				      pos + PCI_EXP_DEVCTL, ctrl);
	} else if (p_params->pci_rlx_odr_mode == QED_DISABLE_RLX_ODR) {
		ctrl &= ~PCI_EXP_DEVCTL_RELAX_EN;
		pci_write_config_word(p_hwfn->cdev->pdev,
				      pos + PCI_EXP_DEVCTL, ctrl);
	} else if (qed_mcp_rlx_odr_supported(p_hwfn)) {
		DP_INFO(p_hwfn, "PCI relax ordering configured by MFW\n");
	} else {
		ctrl &= ~PCI_EXP_DEVCTL_RELAX_EN;
		pci_write_config_word(p_hwfn->cdev->pdev,
				      pos + PCI_EXP_DEVCTL, ctrl);
	}

	rc = qed_hw_init_pf_doorbell_bar(p_hwfn, p_ptt);
	if (rc)
		return rc;

	rc = qed_iov_init_vf_doorbell_bar(p_hwfn, p_ptt);
	if (rc != 0 && QED_IS_VF_RDMA(p_hwfn))
		p_hwfn->pf_iov_info->rdma_enable = false;

	/* Use the leading hwfn since in CMT only NIG #0 is operational */
	if (IS_LEAD_HWFN(p_hwfn)) {
		rc = qed_llh_hw_init_pf(p_hwfn, p_ptt,
					p_params->avoid_eng_affin);
		if (rc)
			return rc;
	}

	if (p_params->b_hw_start) {
		/* enable interrupts */
		rc = qed_int_igu_enable(p_hwfn, p_ptt, p_params->int_mode);
		if (rc)
			return rc;

		/* send function start command */
		rc = qed_sp_pf_start(p_hwfn, p_ptt, p_params->p_tunn,
				     p_params->allow_npar_tx_switch);
		if (rc) {
			DP_NOTICE(p_hwfn, "Function start ramrod failed\n");
			return rc;
		}

		prs_reg = qed_rd(p_hwfn, p_ptt, PRS_REG_SEARCH_TAG1);
		DP_VERBOSE(p_hwfn, QED_MSG_STORAGE,
			   "PRS_REG_SEARCH_TAG1: %x\n", prs_reg);

		if (p_hwfn->hw_info.personality == QED_PCI_FCOE) {
			qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_TAG1, BIT(2));
			qed_wr(p_hwfn, p_ptt,
			       PRS_REG_PKT_LEN_STAT_TAGS_NOT_COUNTED_FIRST,
			       0x100);
		}

		DP_VERBOSE(p_hwfn, QED_MSG_STORAGE,
			   "PRS_REG_SEARCH registers after start PFn\n");
		prs_reg = qed_rd(p_hwfn, p_ptt, PRS_REG_SEARCH_TCP);
		DP_VERBOSE(p_hwfn, QED_MSG_STORAGE,
			   "PRS_REG_SEARCH_TCP: %x\n", prs_reg);
		prs_reg = qed_rd(p_hwfn, p_ptt, PRS_REG_SEARCH_UDP);
		DP_VERBOSE(p_hwfn, QED_MSG_STORAGE,
			   "PRS_REG_SEARCH_UDP: %x\n", prs_reg);
		prs_reg = qed_rd(p_hwfn, p_ptt, PRS_REG_SEARCH_FCOE);
		DP_VERBOSE(p_hwfn, QED_MSG_STORAGE,
			   "PRS_REG_SEARCH_FCOE: %x\n", prs_reg);
		prs_reg = qed_rd(p_hwfn, p_ptt, PRS_REG_SEARCH_ROCE);
		DP_VERBOSE(p_hwfn, QED_MSG_STORAGE,
			   "PRS_REG_SEARCH_ROCE: %x\n", prs_reg);
		prs_reg = qed_rd(p_hwfn, p_ptt, PRS_REG_SEARCH_TCP_FIRST_FRAG);
		DP_VERBOSE(p_hwfn, QED_MSG_STORAGE,
			   "PRS_REG_SEARCH_TCP_FIRST_FRAG: %x\n", prs_reg);
		prs_reg = qed_rd(p_hwfn, p_ptt, PRS_REG_SEARCH_TAG1);
		DP_VERBOSE(p_hwfn, QED_MSG_STORAGE,
			   "PRS_REG_SEARCH_TAG1: %x\n", prs_reg);
	}

	return 0;
}

int qed_pglueb_set_pfid_enable(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, bool b_enable)
{
	u32 delay_idx = 0, val, set_val = b_enable ? 1 : 0;

	/* Configure the PF's internal FID_enable for master transactions */
	qed_wr(p_hwfn, p_ptt, PGLUE_B_REG_INTERNAL_PFID_ENABLE_MASTER, set_val);

	/* Wait until value is set - try for 1 second every 50us */
	for (delay_idx = 0; delay_idx < 20000; delay_idx++) {
		val = qed_rd(p_hwfn, p_ptt,
			     PGLUE_B_REG_INTERNAL_PFID_ENABLE_MASTER);
		if (val == set_val)
			break;

		udelay(50);
	}

	if (val != set_val) {
		DP_NOTICE(p_hwfn,
			  "PFID_ENABLE_MASTER wasn't changed after a second val=%d\n",
			  val);
		return -EAGAIN;
	}

	return 0;
}

static void qed_reset_mb_shadow(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_main_ptt)
{
	/* Read shadow of current MFW mailbox */
	qed_mcp_read_mb(p_hwfn, p_main_ptt);
	memcpy(p_hwfn->mcp_info->mfw_mb_shadow,
	       p_hwfn->mcp_info->mfw_mb_cur, p_hwfn->mcp_info->mfw_mb_length);
}

static int
qed_fill_load_req_params(struct qed_hwfn *p_hwfn,
			 struct qed_load_req_params *p_load_req,
			 struct qed_drv_load_params *p_drv_load)
{
	/* Make sure that if qed-client didn't provide inputs, all the
	 * expected defaults are indeed zero.
	 */
	BUILD_BUG_ON(QED_DRV_ROLE_OS != 0);
	BUILD_BUG_ON(QED_LOAD_REQ_LOCK_TO_DEFAULT != 0);
	BUILD_BUG_ON(QED_OVERRIDE_FORCE_LOAD_NONE != 0);

	memset(p_load_req, 0, sizeof(*p_load_req));

	if (p_drv_load == NULL)
		goto out;

	p_load_req->drv_role = p_drv_load->is_crash_kernel ?
	    QED_DRV_ROLE_KDUMP : QED_DRV_ROLE_OS;
	p_load_req->avoid_eng_reset = p_drv_load->avoid_eng_reset;
	p_load_req->override_force_load = p_drv_load->override_force_load;

	/* Old MFW versions don't support timeout values other than default and
	 * none, so these values are replaced according to the fall-back action.
	 */

	if (p_drv_load->mfw_timeout_val == QED_LOAD_REQ_LOCK_TO_DEFAULT ||
	    p_drv_load->mfw_timeout_val == QED_LOAD_REQ_LOCK_TO_NONE ||
	    (p_hwfn->mcp_info->capabilities &
	     FW_MB_PARAM_FEATURE_SUPPORT_DRV_LOAD_TO)) {
		p_load_req->timeout_val = p_drv_load->mfw_timeout_val;
		goto out;
	}

	switch (p_drv_load->mfw_timeout_fallback) {
	case QED_TO_FALLBACK_TO_NONE:
		p_load_req->timeout_val = QED_LOAD_REQ_LOCK_TO_NONE;
		break;
	case QED_TO_FALLBACK_TO_DEFAULT:
		p_load_req->timeout_val = QED_LOAD_REQ_LOCK_TO_DEFAULT;
		break;
	case QED_TO_FALLBACK_FAIL_LOAD:
		DP_NOTICE(p_hwfn,
			  "Received %d as a value for MFW timeout while the MFW supports only default [%d] or none [%d]. Abort.\n",
			  p_drv_load->mfw_timeout_val,
			  QED_LOAD_REQ_LOCK_TO_DEFAULT,
			  QED_LOAD_REQ_LOCK_TO_NONE);
		return -EBUSY;
	}

	DP_INFO(p_hwfn,
		"Modified the MFW timeout value from %d to %s [%d] due to lack of MFW support\n",
		p_drv_load->mfw_timeout_val,
		(p_load_req->timeout_val == QED_LOAD_REQ_LOCK_TO_DEFAULT) ?
		"default" : "none", p_load_req->timeout_val);
out:
	return 0;
}

static int qed_vf_start(struct qed_hwfn *p_hwfn,
			struct qed_hw_init_params *p_params)
{
	if (p_params->p_tunn) {
		qed_vf_set_vf_start_tunn_update_param(p_params->p_tunn);
		qed_vf_pf_tunnel_param_update(p_hwfn, p_params->p_tunn);
	}

	p_hwfn->b_int_enabled = 1;

	return 0;
}

static void qed_pglueb_clear_err(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	qed_wr(p_hwfn, p_ptt, PGLUE_B_REG_WAS_ERROR_PF_31_0_CLR,
	       1 << p_hwfn->abs_pf_id);
}

int qed_hw_init(struct qed_dev *cdev, struct qed_hw_init_params *p_params)
{
	struct qed_load_req_params load_req_params;
	u32 load_code, resp, param, drv_mb_param;
	int rc = 0, cancel_load_rc;
	bool b_default_mtu = true, old_recov_state;
	struct qed_hwfn *p_hwfn;
	const u32 *fw_overlays;
	u32 fw_overlays_len;
	u16 ether_type;
	int i;

	if ((p_params->int_mode == QED_INT_MODE_MSI) && QED_IS_CMT(cdev)) {
		DP_NOTICE(cdev, "MSI mode is not supported for CMT devices\n");
		return -EINVAL;
	}

	if (IS_PF(cdev)) {
		rc = qed_init_fw_data(cdev, p_params->bin_fw_data);
		if (rc)
			return rc;
	}

	for_each_hwfn(cdev, i) {
		p_hwfn = &cdev->hwfns[i];

		/* If management didn't provide a default, set one of our own */
		if (!p_hwfn->hw_info.mtu) {
			p_hwfn->hw_info.mtu = 1500;
			b_default_mtu = false;
		}

		if (IS_VF(cdev)) {
			qed_vf_start(p_hwfn, p_params);
			continue;
		}

		/* Some flows may keep variable set */
		p_hwfn->mcp_info->mcp_handling_status = 0;

		rc = qed_calc_hw_mode(p_hwfn);
		if (rc)
			return rc;

		if (IS_PF(cdev) && (test_bit(QED_MF_8021Q_TAGGING,
					     &cdev->mf_bits) ||
				    test_bit(QED_MF_8021AD_TAGGING,
					     &cdev->mf_bits))) {
			if (test_bit(QED_MF_8021Q_TAGGING, &cdev->mf_bits))
				ether_type = ETH_P_8021Q;
			else
				ether_type = ETH_P_8021AD;
			STORE_RT_REG(p_hwfn, PRS_REG_TAG_ETHERTYPE_0_RT_OFFSET,
				     ether_type);
			STORE_RT_REG(p_hwfn, NIG_REG_TAG_ETHERTYPE_0_RT_OFFSET,
				     ether_type);
			STORE_RT_REG(p_hwfn, PBF_REG_TAG_ETHERTYPE_0_RT_OFFSET,
				     ether_type);
			STORE_RT_REG(p_hwfn, DORQ_REG_TAG1_ETHERTYPE_RT_OFFSET,
				     ether_type);
		}

		qed_set_spq_block_timeout(p_hwfn, p_params->spq_timeout_ms);

		rc = qed_fill_load_req_params(p_hwfn, &load_req_params,
					      p_params->p_drv_load_params);
		if (rc)
			return rc;

		rc = qed_mcp_load_req(p_hwfn, p_hwfn->p_main_ptt,
				      &load_req_params);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "Failed sending a LOAD_REQ command\n");
			return rc;
		}

		load_code = load_req_params.load_code;
		DP_VERBOSE(p_hwfn, QED_MSG_SP,
			   "Load request was sent. Load code: 0x%x\n",
			   load_code);

		/* Only relevant for recovery:
		 * Clear the indication after LOAD_REQ is responded by the MFW.
		 * Also save recovery state to restore during the load fails.
		 */
		old_recov_state = cdev->recov_in_prog;
		cdev->recov_in_prog = false;

		qed_mcp_set_capabilities(p_hwfn, p_hwfn->p_main_ptt);

		/* CQ75580:
		 * When coming back from hibernate state, the registers from
		 * which shadow is read initially are not initialized. It turns
		 * out that these registers get initialized during the call to
		 * qed_mcp_load_req request. So we need to reread them here
		 * to get the proper shadow register value.
		 * Note: This is a workaround for the missing MFW
		 * initialization. It may be removed once the implementation
		 * is done.
		 */
		qed_reset_mb_shadow(p_hwfn, p_hwfn->p_main_ptt);

		if (!qm_lock_ref_cnt)
			spin_lock_init(&qm_lock);

		p_hwfn->qm_lock = &qm_lock;
		++qm_lock_ref_cnt;

		/* Clean up chip from previous driver if such remains exist.
		 * This is not needed when the PF is the first one on the
		 * engine, since afterwards we are going to init the FW.
		 */
		if (load_code != FW_MSG_CODE_DRV_LOAD_ENGINE) {
			rc = qed_final_cleanup(p_hwfn, p_hwfn->p_main_ptt,
					       p_hwfn->rel_pf_id, false);
			if (rc) {
				qed_hw_err_notify(p_hwfn, p_hwfn->p_main_ptt,
						  QED_HW_ERR_RAMROD_FAIL,
						  "Final cleanup failed\n");
				goto load_err;
			}
		}

		/* Log and clear previous pglue_b errors if such exist */
		qed_pglueb_rbc_attn_handler(p_hwfn, p_hwfn->p_main_ptt, true);

		/* Enable the PF's internal FID_enable in the PXP */
		rc = qed_pglueb_set_pfid_enable(p_hwfn, p_hwfn->p_main_ptt,
						true);
		if (rc)
			goto load_err;

		/* Clear the pglue_b was_error indication.
		 * It must be done after the BME and the internal FID_enable for
		 * the PF are set, since VDMs may cause the indication to be set
		 * again.
		 */
		qed_pglueb_clear_err(p_hwfn, p_hwfn->p_main_ptt);

		fw_overlays = cdev->fw_data->fw_overlays;
		fw_overlays_len = cdev->fw_data->fw_overlays_len;
		p_hwfn->fw_overlay_mem =
		    qed_fw_overlay_mem_alloc(p_hwfn, fw_overlays,
					     fw_overlays_len);
		if (!p_hwfn->fw_overlay_mem) {
			DP_NOTICE(p_hwfn,
				  "Failed to allocate fw overlay memory\n");
			rc = -ENOMEM;
			goto load_err;
		}

		switch (load_code) {
		case FW_MSG_CODE_DRV_LOAD_ENGINE:
			rc = qed_hw_init_common(p_hwfn, p_hwfn->p_main_ptt,
						p_hwfn->hw_info.hw_mode);
			if (rc)
				break;
			COMPAT_FALLTHROUGH;
			/* fallthrough */
		case FW_MSG_CODE_DRV_LOAD_PORT:
			rc = qed_hw_init_port(p_hwfn, p_hwfn->p_main_ptt,
					      p_hwfn->hw_info.hw_mode);
			if (rc)
				break;
			COMPAT_FALLTHROUGH;
			/* fallthrough */
		case FW_MSG_CODE_DRV_LOAD_FUNCTION:
			rc = qed_hw_init_pf(p_hwfn, p_hwfn->p_main_ptt,
					    p_hwfn->hw_info.hw_mode, p_params);
			break;
		default:
			DP_NOTICE(p_hwfn,
				  "Unexpected load code [0x%08x]", load_code);
			rc = -EOPNOTSUPP;
			break;
		}

		if (rc) {
			DP_NOTICE(p_hwfn,
				  "init phase failed for loadcode 0x%x (rc %d)\n",
				  load_code, rc);
			goto load_err;
		}

		rc = qed_mcp_load_done(p_hwfn, p_hwfn->p_main_ptt);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "Sending load done failed, rc = %d\n", rc);
			if (rc == -ENOMEM) {
				DP_NOTICE(p_hwfn,
					  "Sending load done was failed due to memory allocation failure\n");
				goto load_err;
			}

			goto recov_state;
		}

		/* send DCBX attention request command */
		DP_VERBOSE(p_hwfn,
			   QED_MSG_DCB,
			   "sending phony dcbx set command to trigger DCBx attention handling\n");
		drv_mb_param = 0;
		SET_MFW_FIELD(drv_mb_param, DRV_MB_PARAM_DCBX_NOTIFY, 1);
		rc = qed_mcp_cmd(p_hwfn, p_hwfn->p_main_ptt,
				 DRV_MSG_CODE_SET_DCBX,
				 drv_mb_param, &resp, &param);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "Failed to send DCBX attention request\n");
			goto recov_state;
		}

		p_hwfn->hw_init_done = true;
	}

	if (IS_PF(cdev)) {
		/* Get pre-negotiated values for stag, bandwidth etc. */
		p_hwfn = QED_LEADING_HWFN(cdev);
		DP_VERBOSE(p_hwfn,
			   QED_MSG_SPQ,
			   "Sending GET_OEM_UPDATES command to trigger stag/bandwidth attention handling\n");
		drv_mb_param = 0;
		SET_MFW_FIELD(drv_mb_param, DRV_MB_PARAM_DUMMY_OEM_UPDATES, 1);
		rc = qed_mcp_cmd(p_hwfn, p_hwfn->p_main_ptt,
				 DRV_MSG_CODE_GET_OEM_UPDATES,
				 drv_mb_param, &resp, &param);
		if (rc)
			DP_NOTICE(p_hwfn,
				  "Failed to send GET_OEM_UPDATES attention request\n");
	}

	if (IS_PF(cdev)) {
		p_hwfn = QED_LEADING_HWFN(cdev);
		drv_mb_param = STORM_FW_VERSION;
		rc = qed_mcp_cmd(p_hwfn,
				 p_hwfn->p_main_ptt,
				 DRV_MSG_CODE_OV_UPDATE_STORM_FW_VER,
				 drv_mb_param, &resp, &param);
		if (rc)
			DP_INFO(p_hwfn, "Failed to update firmware version\n");

		if (!b_default_mtu) {
			rc = qed_mcp_ov_update_mtu(p_hwfn, p_hwfn->p_main_ptt,
						   p_hwfn->hw_info.mtu);
			if (rc)
				DP_INFO(p_hwfn,
					"Failed to update default mtu\n");
		}

		rc = qed_mcp_ov_update_driver_state(p_hwfn,
						    p_hwfn->p_main_ptt,
						    QED_OV_DRIVER_STATE_DISABLED);
		if (rc)
			DP_INFO(p_hwfn, "Failed to update driver state\n");

		rc = qed_mcp_ov_update_eswitch(p_hwfn, p_hwfn->p_main_ptt,
					       QED_OV_ESWITCH_NONE);
		if (rc)
			DP_INFO(p_hwfn, "Failed to update eswitch mode\n");

		if (!QED_IS_BB(p_hwfn->cdev))
			cdev->hot_reset_count = (u16) qed_rd(p_hwfn,
							     p_hwfn->p_main_ptt,
							     MISCS_REG_HOT_RESET_PREPARED_CNT_K2);
	}

	return rc;

load_err:
	--qm_lock_ref_cnt;
	/* The MFW load lock should be released also when initialization fails.
	 * If supported, use a cancel_load request to update the MFW with the
	 * load failure.
	 */
	cancel_load_rc = qed_mcp_cancel_load_req(p_hwfn, p_hwfn->p_main_ptt);
	if (cancel_load_rc == -EOPNOTSUPP) {
		DP_INFO(p_hwfn,
			"Send a load done request instead of cancel load\n");
		qed_mcp_load_done(p_hwfn, p_hwfn->p_main_ptt);
	}
recov_state:
	cdev->recov_in_prog = old_recov_state;

	return rc;
}

#define QED_HW_STOP_RETRY_LIMIT (10)
static void qed_hw_timers_stop(struct qed_dev *cdev,
			       struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	int i;

	/* close timers */
	qed_wr(p_hwfn, p_ptt, TM_REG_PF_ENABLE_CONN, 0x0);
	qed_wr(p_hwfn, p_ptt, TM_REG_PF_ENABLE_TASK, 0x0);

	if (QED_RECOV_IN_PROG(cdev))
		return;

	for (i = 0; i < QED_HW_STOP_RETRY_LIMIT; i++) {
		if ((!qed_rd(p_hwfn, p_ptt,
			     TM_REG_PF_SCAN_ACTIVE_CONN)) &&
		    (!qed_rd(p_hwfn, p_ptt, TM_REG_PF_SCAN_ACTIVE_TASK)))
			break;

		/* Dependent on number of connection/tasks, possibly
		 * 1ms sleep is required between polls
		 */
		usleep_range(1000, 2000);
	}

	if (i < QED_HW_STOP_RETRY_LIMIT)
		return;

	DP_NOTICE(p_hwfn,
		  "Timers linear scans are not over [Connection %02x Tasks %02x]\n",
		  (u8) qed_rd(p_hwfn, p_ptt, TM_REG_PF_SCAN_ACTIVE_CONN),
		  (u8) qed_rd(p_hwfn, p_ptt, TM_REG_PF_SCAN_ACTIVE_TASK));
}

void qed_hw_timers_stop_all(struct qed_dev *cdev)
{
	int j;

	for_each_hwfn(cdev, j) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[j];
		struct qed_ptt *p_ptt = p_hwfn->p_main_ptt;

		qed_hw_timers_stop(cdev, p_hwfn, p_ptt);
	}
}

static int qed_verify_reg_val(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, u32 addr, u32 expected_val)
{
	u32 val = qed_rd(p_hwfn, p_ptt, addr);

	if (val != expected_val) {
		DP_NOTICE(p_hwfn,
			  "Value at address 0x%08x is 0x%08x while the expected value is 0x%08x\n",
			  addr, val, expected_val);
		return -EINVAL;
	}

	return 0;
}

int qed_hw_stop(struct qed_dev *cdev)
{
	struct qed_hwfn *p_hwfn;
	struct qed_ptt *p_ptt;
	int rc, rc2 = 0;
	int j;

	for_each_hwfn(cdev, j) {
		p_hwfn = &cdev->hwfns[j];
		p_ptt = p_hwfn->p_main_ptt;

		DP_VERBOSE(p_hwfn, NETIF_MSG_IFDOWN, "Stopping hw/fw\n");

		if (IS_VF(cdev)) {
			qed_vf_pf_int_cleanup(p_hwfn);
			rc = qed_vf_pf_reset(p_hwfn);
			if (rc) {
				DP_NOTICE(p_hwfn,
					  "qed_vf_pf_reset failed. rc = %d.\n",
					  rc);
				rc2 = rc;
			}
			continue;
		}

		/* mark the hw as uninitialized... */
		p_hwfn->hw_init_done = false;

		/* Send unload command to MCP */
		if (!QED_RECOV_IN_PROG(cdev)) {
			rc = qed_mcp_unload_req(p_hwfn, p_ptt);
			if (rc) {
				DP_NOTICE(p_hwfn,
					  "Failed sending a UNLOAD_REQ command. rc = %d.\n",
					  rc);
				rc2 = -EINVAL;
			}
		}

		qed_slowpath_irq_sync(p_hwfn);

		/* After this point no MFW attentions are expected, e.g. prevent
		 * race between pf stop and dcbx pf update.
		 */

		rc = qed_sp_pf_stop(p_hwfn);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "Failed to close PF against FW [rc = %d]. Continue to stop HW to prevent illegal host access by the device.\n",
				  rc);
			rc2 = -EINVAL;
		}

		qed_slowpath_irq_sync(p_hwfn);

		/* After this point we don't expect the FW to send us async
		 * events
		 */

		/* perform debug action after PF stop was sent */
		qed_copy_bus_to_postconfig((void *)cdev, p_hwfn->my_id);

		/* close NIG to BRB gate */
		qed_wr(p_hwfn, p_ptt,
		       NIG_REG_RX_LLH_BRB_GATE_DNTFWD_PERPF, 0x1);

		/* close parser */
		qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_TCP, 0x0);
		qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_UDP, 0x0);
		qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_FCOE, 0x0);
		qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_ROCE, 0x0);
		qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_OPENFLOW, 0x0);

		/* @@@TBD - clean transmission queues (5.b) */
		/* @@@TBD - clean BTB (5.c) */

		qed_hw_timers_stop(cdev, p_hwfn, p_ptt);

		/* @@@TBD - verify DMAE requests are done (8) */

		/* Disable Attention Generation */
		qed_int_igu_disable_int(p_hwfn, p_ptt);
		qed_wr(p_hwfn, p_ptt, IGU_REG_LEADING_EDGE_LATCH, 0);
		qed_wr(p_hwfn, p_ptt, IGU_REG_TRAILING_EDGE_LATCH, 0);
		if (!cdev->aer_in_prog)
			qed_int_igu_init_pure_rt(p_hwfn, p_ptt, false, true);
		rc = qed_int_igu_reset_cam_default(p_hwfn, p_ptt);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "Failed to return IGU CAM to default\n");
			rc2 = -EINVAL;
		}

		/* Need to wait 1ms to guarantee SBs are cleared */
		usleep_range(1000, 2000);

		if (!QED_RECOV_IN_PROG(cdev)) {
			qed_verify_reg_val(p_hwfn, p_ptt,
					   QM_REG_USG_CNT_PF_TX, 0);
			qed_verify_reg_val(p_hwfn, p_ptt,
					   QM_REG_USG_CNT_PF_OTHER, 0);
			/* @@@TBD - assert on incorrect xCFC values (10.b) */
		}

		/* Disable PF in HW blocks */
		qed_wr(p_hwfn, p_ptt, DORQ_REG_PF_DB_ENABLE, 0);
		qed_wr(p_hwfn, p_ptt, QM_REG_PF_EN, 0);

		if (IS_LEAD_HWFN(p_hwfn) &&
		    test_bit(QED_MF_LLH_MAC_CLSS, &cdev->mf_bits) &&
		    !QED_IS_FCOE_PERSONALITY(p_hwfn))
			qed_llh_remove_mac_filter(cdev, 0,
						  p_hwfn->hw_info.hw_mac_addr);

		--qm_lock_ref_cnt;

		if (!QED_RECOV_IN_PROG(cdev)) {
			rc = qed_mcp_unload_done(p_hwfn, p_ptt);
			if (rc == -ENOMEM) {
				DP_NOTICE(p_hwfn,
					  "Failed sending an UNLOAD_DONE command due to a memory allocation failure. Resending.\n");
				rc = qed_mcp_unload_done(p_hwfn, p_ptt);
			}
			if (rc) {
				DP_NOTICE(p_hwfn,
					  "Failed sending a UNLOAD_DONE command. rc = %d.\n",
					  rc);
				rc2 = -EINVAL;
			}
		}
	}			/* hwfn loop */

	if (IS_PF(cdev) && !QED_RECOV_IN_PROG(cdev)) {
		p_hwfn = QED_LEADING_HWFN(cdev);
		p_ptt = QED_LEADING_HWFN(cdev)->p_main_ptt;

		/* Clear the PF's internal FID_enable in the PXP.
		 * In CMT this should only be done for first hw-function, and
		 * only after all transactions have stopped for all active
		 * hw-functions.
		 */
		rc = qed_pglueb_set_pfid_enable(p_hwfn, p_ptt, false);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "qed_pglueb_set_pfid_enable() failed. rc = %d.\n",
				  rc);
			rc2 = -EINVAL;
		}
	}

	return rc2;
}

int qed_hw_stop_fastpath(struct qed_dev *cdev)
{
	int j;

	for_each_hwfn(cdev, j) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[j];
		struct qed_ptt *p_ptt;

		if (IS_VF(cdev)) {
			qed_vf_pf_int_cleanup(p_hwfn);
			continue;
		}
		p_ptt = qed_ptt_acquire(p_hwfn);
		if (!p_ptt)
			return -EAGAIN;

		DP_VERBOSE(p_hwfn,
			   NETIF_MSG_IFDOWN, "Shutting down the fastpath\n");

		qed_wr(p_hwfn, p_ptt,
		       NIG_REG_RX_LLH_BRB_GATE_DNTFWD_PERPF, 0x1);

		/* @@@TBD - clean transmission queues (5.b) */
		/* @@@TBD - clean BTB (5.c) */

		/* @@@TBD - verify DMAE requests are done (8) */

		qed_int_igu_init_pure_rt(p_hwfn, p_ptt, false, false);
		/* Need to wait 1ms to guarantee SBs are cleared */
		usleep_range(1000, 2000);
		qed_ptt_release(p_hwfn, p_ptt);
	}

	return 0;
}

int qed_hw_start_fastpath(struct qed_hwfn *p_hwfn)
{
	struct qed_ptt *p_ptt;

	if (IS_VF(p_hwfn->cdev))
		return 0;

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return -EAGAIN;

	/* If roce info is allocated it means roce is initialized and should
	 * be enabled in searcher.
	 */
	if (p_hwfn->p_rdma_info &&
	    p_hwfn->p_rdma_info->active && p_hwfn->b_rdma_enabled_in_prs)
		qed_wr(p_hwfn, p_ptt, p_hwfn->rdma_prs_search_reg, 0x1);

	/* Re-open incoming traffic */
	qed_wr(p_hwfn, p_ptt, NIG_REG_RX_LLH_BRB_GATE_DNTFWD_PERPF, 0x0);
	qed_ptt_release(p_hwfn, p_ptt);

	return 0;
}

static void qed_wol_wr(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt, u32 hw_addr, u32 val)
{
	if (QED_IS_BB(p_hwfn->cdev))
		qed_wr(p_hwfn, p_ptt, hw_addr, val);
	else
		qed_mcp_wol_wr(p_hwfn, p_ptt, hw_addr, val);
}

int qed_set_nwuf_reg(struct qed_dev *cdev,
		     u32 reg_idx, u32 pattern_size, u32 crc)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	int rc = 0;
	struct qed_ptt *p_ptt;
	u32 reg_len = 0;
	u32 reg_crc = 0;

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return -EAGAIN;

	/* Get length and CRC register offsets */
	switch (reg_idx) {
	case 0:
		reg_len = QED_IS_BB(cdev) ? NIG_REG_ACPI_PAT_0_LEN_BB :
		    WOL_REG_ACPI_PAT_0_LEN_K2;
		reg_crc = QED_IS_BB(cdev) ? NIG_REG_ACPI_PAT_0_CRC_BB :
		    WOL_REG_ACPI_PAT_0_CRC_K2;
		break;
	case 1:
		reg_len = QED_IS_BB(cdev) ? NIG_REG_ACPI_PAT_1_LEN_BB :
		    WOL_REG_ACPI_PAT_1_LEN_K2;
		reg_crc = QED_IS_BB(cdev) ? NIG_REG_ACPI_PAT_1_CRC_BB :
		    WOL_REG_ACPI_PAT_1_CRC_K2;
		break;
	case 2:
		reg_len = QED_IS_BB(cdev) ? NIG_REG_ACPI_PAT_2_LEN_BB :
		    WOL_REG_ACPI_PAT_2_LEN_K2;
		reg_crc = QED_IS_BB(cdev) ? NIG_REG_ACPI_PAT_2_CRC_BB :
		    WOL_REG_ACPI_PAT_2_CRC_K2;
		break;
	case 3:
		reg_len = QED_IS_BB(cdev) ? NIG_REG_ACPI_PAT_3_LEN_BB :
		    WOL_REG_ACPI_PAT_3_LEN_K2;
		reg_crc = QED_IS_BB(cdev) ? NIG_REG_ACPI_PAT_3_CRC_BB :
		    WOL_REG_ACPI_PAT_3_CRC_K2;
		break;
	case 4:
		reg_len = QED_IS_BB(cdev) ? NIG_REG_ACPI_PAT_4_LEN_BB :
		    WOL_REG_ACPI_PAT_4_LEN_K2;
		reg_crc = QED_IS_BB(cdev) ? NIG_REG_ACPI_PAT_4_CRC_BB :
		    WOL_REG_ACPI_PAT_4_CRC_K2;
		break;
	case 5:
		reg_len = QED_IS_BB(cdev) ? NIG_REG_ACPI_PAT_5_LEN_BB :
		    WOL_REG_ACPI_PAT_5_LEN_K2;
		reg_crc = QED_IS_BB(cdev) ? NIG_REG_ACPI_PAT_5_CRC_BB :
		    WOL_REG_ACPI_PAT_5_CRC_K2;
		break;
	case 6:
		reg_len = QED_IS_BB(cdev) ? NIG_REG_ACPI_PAT_6_LEN_BB :
		    WOL_REG_ACPI_PAT_6_LEN_K2;
		reg_crc = QED_IS_BB(cdev) ? NIG_REG_ACPI_PAT_6_CRC_BB :
		    WOL_REG_ACPI_PAT_6_CRC_K2;
		break;
	case 7:
		reg_len = QED_IS_BB(cdev) ? NIG_REG_ACPI_PAT_7_LEN_BB :
		    WOL_REG_ACPI_PAT_7_LEN_K2;
		reg_crc = QED_IS_BB(cdev) ? NIG_REG_ACPI_PAT_7_CRC_BB :
		    WOL_REG_ACPI_PAT_7_CRC_K2;
		break;
	default:
		rc = -EINVAL;
		goto out;
	}

	/* Allign pattern size to 4 */
	while (pattern_size % 4)
		pattern_size++;

	/* Write pattern length */
	qed_wol_wr(p_hwfn, p_ptt, reg_len, pattern_size);

	/* Write crc value */
	qed_wol_wr(p_hwfn, p_ptt, reg_crc, crc);

	DP_INFO(cdev,
		"qed_set_nwuf_reg: idx[%d] reg_crc[0x%x=0x%08x] "
		"reg_len[0x%x=0x%x]\n",
		reg_idx, reg_crc, crc, reg_len, pattern_size);
out:
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

void qed_wol_buffer_clear(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	const u32 wake_buffer_clear_offset =
	    QED_IS_BB(p_hwfn->cdev) ?
	    NIG_REG_WAKE_BUFFER_CLEAR_BB : WOL_REG_WAKE_BUFFER_CLEAR_K2;

	DP_INFO(p_hwfn->cdev,
		"qed_wol_buffer_clear: reset "
		"REG_WAKE_BUFFER_CLEAR offset=0x%08x\n",
		wake_buffer_clear_offset);

	qed_wol_wr(p_hwfn, p_ptt, wake_buffer_clear_offset, 1);
	qed_wol_wr(p_hwfn, p_ptt, wake_buffer_clear_offset, 0);
}

int qed_get_wake_info(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, struct qed_wake_info *wake_info)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	u32 *buf = NULL;
	u32 i = 0;
	const u32 reg_wake_buffer_offest =
	    QED_IS_BB(cdev) ? NIG_REG_WAKE_BUFFER_BB : WOL_REG_WAKE_BUFFER_K2;

	wake_info->wk_info = qed_rd(p_hwfn, p_ptt,
				    QED_IS_BB(cdev) ? NIG_REG_WAKE_INFO_BB :
				    WOL_REG_WAKE_INFO_K2);
	wake_info->wk_details = qed_rd(p_hwfn, p_ptt,
				       QED_IS_BB(cdev) ? NIG_REG_WAKE_DETAILS_BB
				       : WOL_REG_WAKE_DETAILS_K2);
	wake_info->wk_pkt_len = qed_rd(p_hwfn, p_ptt,
				       QED_IS_BB(cdev) ? NIG_REG_WAKE_PKT_LEN_BB
				       : WOL_REG_WAKE_PKT_LEN_K2);

	DP_INFO(cdev,
		"qed_get_wake_info: REG_WAKE_INFO=0x%08x "
		"REG_WAKE_DETAILS=0x%08x "
		"REG_WAKE_PKT_LEN=0x%08x\n",
		wake_info->wk_info,
		wake_info->wk_details, wake_info->wk_pkt_len);

	buf = (u32 *) wake_info->wk_buffer;

	for (i = 0; i < (wake_info->wk_pkt_len / sizeof(u32)); i++) {
		if ((i * sizeof(u32)) >= sizeof(wake_info->wk_buffer)) {
			DP_INFO(cdev,
				"qed_get_wake_info: i index to 0 high=%d\n", i);
			break;
		}
		buf[i] = qed_rd(p_hwfn, p_ptt,
				reg_wake_buffer_offest + (i * sizeof(u32)));
		DP_INFO(cdev, "qed_get_wake_info: wk_buffer[%u]: 0x%08x\n",
			i, buf[i]);
	}

	qed_wol_buffer_clear(p_hwfn, p_ptt);

	return 0;
}

/* Free hwfn memory and resources acquired in hw_hwfn_prepare */
static void qed_hw_hwfn_free(struct qed_hwfn *p_hwfn)
{
	qed_ptt_pool_free(p_hwfn);
	kfree(p_hwfn->hw_info.p_igu_info);
	p_hwfn->hw_info.p_igu_info = NULL;
}

/* Setup bar access */
static void qed_hw_hwfn_prepare(struct qed_hwfn *p_hwfn)
{
	/* clear indirect access */
	if (QED_IS_AH(p_hwfn->cdev)) {
		qed_wr(p_hwfn, p_hwfn->p_main_ptt,
		       PGLUE_B_REG_PGL_ADDR_E8_F0_K2, 0);
		qed_wr(p_hwfn, p_hwfn->p_main_ptt,
		       PGLUE_B_REG_PGL_ADDR_EC_F0_K2, 0);
		qed_wr(p_hwfn, p_hwfn->p_main_ptt,
		       PGLUE_B_REG_PGL_ADDR_F0_F0_K2, 0);
		qed_wr(p_hwfn, p_hwfn->p_main_ptt,
		       PGLUE_B_REG_PGL_ADDR_F4_F0_K2, 0);
	} else {
		qed_wr(p_hwfn, p_hwfn->p_main_ptt,
		       PGLUE_B_REG_PGL_ADDR_88_F0_BB, 0);
		qed_wr(p_hwfn, p_hwfn->p_main_ptt,
		       PGLUE_B_REG_PGL_ADDR_8C_F0_BB, 0);
		qed_wr(p_hwfn, p_hwfn->p_main_ptt,
		       PGLUE_B_REG_PGL_ADDR_90_F0_BB, 0);
		qed_wr(p_hwfn, p_hwfn->p_main_ptt,
		       PGLUE_B_REG_PGL_ADDR_94_F0_BB, 0);
	}

	/* Clean previous pglue_b errors if such exist */
	qed_pglueb_clear_err(p_hwfn, p_hwfn->p_main_ptt);

	/* enable internal target-read */
	qed_wr(p_hwfn, p_hwfn->p_main_ptt,
	       PGLUE_B_REG_INTERNAL_PFID_ENABLE_TARGET_READ, 1);
}

static void get_function_id(struct qed_hwfn *p_hwfn)
{
	/* ME Register */
	p_hwfn->hw_info.opaque_fid = (u16) REG_RD(p_hwfn,
						  PXP_PF_ME_OPAQUE_ADDR);

	p_hwfn->hw_info.concrete_fid = REG_RD(p_hwfn, PXP_PF_ME_CONCRETE_ADDR);

	/* Bits 16-19 from the ME registers are the pf_num */
	p_hwfn->abs_pf_id = (p_hwfn->hw_info.concrete_fid >> 16) & 0xf;
	p_hwfn->rel_pf_id = GET_FIELD(p_hwfn->hw_info.concrete_fid,
				      PXP_CONCRETE_FID_PFID);
	p_hwfn->port_id = GET_FIELD(p_hwfn->hw_info.concrete_fid,
				    PXP_CONCRETE_FID_PORT);

	DP_VERBOSE(p_hwfn, NETIF_MSG_PROBE,
		   "Read ME register: Concrete 0x%08x Opaque 0x%04x\n",
		   p_hwfn->hw_info.concrete_fid, p_hwfn->hw_info.opaque_fid);
}

void qed_hw_set_feat(struct qed_hwfn *p_hwfn)
{
	u32 *feat_num = p_hwfn->hw_info.feat_num;
	struct qed_sb_cnt_info sb_cnt;
	u32 non_l2_sbs = 0, non_l2_iov_sbs = 0;

	memset(&sb_cnt, 0, sizeof(sb_cnt));
	qed_int_get_num_sbs(p_hwfn, &sb_cnt);

#if IS_ENABLED(CONFIG_QED_RDMA)
	/* Roce CNQ require each: 1 status block. 1 CNQ, we divide the
	 * status blocks equally between L2 / RoCE but with consideration as
	 * to how many l2 queues / cnqs we have
	 */
	if (QED_IS_RDMA_PERSONALITY(p_hwfn)) {
		feat_num[QED_RDMA_CNQ] =
		    min_t(u32,
			  sb_cnt.cnt / 2, RESC_NUM(p_hwfn, QED_RDMA_CNQ_RAM));

		feat_num[QED_VF_RDMA_CNQ] =
		    min_t(u32,
			  sb_cnt.iov_cnt / 2,
			  RESC_NUM(p_hwfn, QED_VF_RDMA_CNQ_RAM));

		non_l2_sbs = feat_num[QED_RDMA_CNQ];
		non_l2_iov_sbs = feat_num[QED_VF_RDMA_CNQ];
	}
#endif

	/* L2 Queues require each: 1 status block. 1 L2 queue */
	if (QED_IS_L2_PERSONALITY(p_hwfn)) {
		/* Start by allocating VF queues, then PF's */
		feat_num[QED_VF_L2_QUE] =
		    min_t(u32,
			  sb_cnt.iov_cnt - non_l2_iov_sbs,
			  RESC_NUM(p_hwfn, QED_L2_QUEUE));

		feat_num[QED_PF_L2_QUE] =
		    min_t(u32,
			  sb_cnt.cnt - non_l2_sbs,
			  RESC_NUM(p_hwfn, QED_L2_QUEUE) -
			  FEAT_NUM(p_hwfn, QED_VF_L2_QUE));
	}

	if (QED_IS_FCOE_PERSONALITY(p_hwfn) || QED_IS_ISCSI_PERSONALITY(p_hwfn)) {
		u32 *p_storage_feat = QED_IS_FCOE_PERSONALITY(p_hwfn) ?
		    &feat_num[QED_FCOE_CQ] : &feat_num[QED_ISCSI_CQ];
		u32 limit = sb_cnt.cnt;

		/* The number of queues should not exceed the number of FP SBs.
		 * In storage target, the queues are divided into pairs of a CQ
		 * and a CmdQ, and each pair uses a single SB. The limit in
		 * this case should allow a max ratio of 2:1 instead of 1:1.
		 */
		if (p_hwfn->cdev->b_is_target)
			limit *= 2;
		*p_storage_feat = min_t(u32, limit,
					RESC_NUM(p_hwfn, QED_CMDQS_CQS));

		/* The size of "cq_cmdq_sb_num_arr" in the fcoe/iscsi init
		 * ramrod is limited to "SCSI_MAX_NUM_OF_CMDQS".
		 */
		*p_storage_feat = min_t(u32, *p_storage_feat,
					SCSI_MAX_NUM_OF_CMDQS);
	}

	DP_VERBOSE(p_hwfn,
		   NETIF_MSG_PROBE,
		   "#PF_L2_QUEUE=%d VF_L2_QUEUES=%d #PF_ROCE_CNQ=%d #VF_ROCE_CNQ=%d #FCOE_CQ=%d #ISCSI_CQ=%d #SB=%d\n",
		   (int)FEAT_NUM(p_hwfn, QED_PF_L2_QUE),
		   (int)FEAT_NUM(p_hwfn, QED_VF_L2_QUE),
		   (int)FEAT_NUM(p_hwfn, QED_RDMA_CNQ),
		   (int)FEAT_NUM(p_hwfn, QED_VF_RDMA_CNQ),
		   (int)FEAT_NUM(p_hwfn, QED_FCOE_CQ),
		   (int)FEAT_NUM(p_hwfn, QED_ISCSI_CQ), (int)sb_cnt.cnt);
}

const char *qed_hw_get_resc_name(enum qed_resources res_id)
{
	switch (res_id) {
	case QED_L2_QUEUE:
		return "L2_QUEUE";
	case QED_VPORT:
		return "VPORT";
	case QED_RSS_ENG:
		return "RSS_ENG";
	case QED_PQ:
		return "PQ";
	case QED_RL:
		return "RL";
	case QED_MAC:
		return "MAC";
	case QED_VLAN:
		return "VLAN";
	case QED_VF_RDMA_CNQ_RAM:
		return "VF_RDMA_CNQ_RAM";
	case QED_RDMA_CNQ_RAM:
		return "RDMA_CNQ_RAM";
	case QED_ILT:
		return "ILT";
	case QED_LL2_RAM_QUEUE:
		return "LL2_RAM_QUEUE";
	case QED_LL2_CTX_QUEUE:
		return "LL2_CTX_QUEUE";
	case QED_CMDQS_CQS:
		return "CMDQS_CQS";
	case QED_RDMA_STATS_QUEUE:
		return "RDMA_STATS_QUEUE";
	case QED_BDQ:
		return "BDQ";
	case QED_VF_MAC_ADDR:
		return "VF_MAC_ADDR";
	case QED_SB:
		return "SB";
	default:
		return "UNKNOWN_RESOURCE";
	}
}

static int
__qed_hw_set_soft_resc_size(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt,
			    enum qed_resources res_id,
			    u32 resc_max_val, u32 * p_mcp_resp)
{
	int rc;

	rc = qed_mcp_set_resc_max_val(p_hwfn, p_ptt, res_id,
				      resc_max_val, p_mcp_resp);
	if (rc) {
		DP_NOTICE(p_hwfn,
			  "MFW response failure for a max value setting of resource %d [%s]\n",
			  res_id, qed_hw_get_resc_name(res_id));
		return rc;
	}

	if (*p_mcp_resp != FW_MSG_CODE_RESOURCE_ALLOC_OK)
		DP_INFO(p_hwfn,
			"Failed to set the max value of resource %d [%s]. mcp_resp = 0x%08x.\n",
			res_id, qed_hw_get_resc_name(res_id), *p_mcp_resp);

	return 0;
}

static u16 qed_hsi_def_val[][MAX_CHIP_IDS] = {
	{MAX_NUM_VFS_BB, MAX_NUM_VFS_K2},
	{MAX_NUM_L2_QUEUES_BB, MAX_NUM_L2_QUEUES_K2},
	{MAX_NUM_PORTS_BB, MAX_NUM_PORTS_K2},
	{MAX_SB_PER_PATH_BB, MAX_SB_PER_PATH_K2,},
	{MAX_NUM_PFS_BB, MAX_NUM_PFS_K2},
	{MAX_NUM_VPORTS_BB, MAX_NUM_VPORTS_K2},
	{ETH_RSS_ENGINE_NUM_BB, ETH_RSS_ENGINE_NUM_K2},
	{MAX_QM_TX_QUEUES_BB, MAX_QM_TX_QUEUES_K2},
	{PXP_NUM_ILT_RECORDS_BB, PXP_NUM_ILT_RECORDS_K2},
	{RDMA_NUM_STATISTIC_COUNTERS_BB, RDMA_NUM_STATISTIC_COUNTERS_K2},
	{MAX_QM_GLOBAL_RLS, MAX_QM_GLOBAL_RLS},
	{PBF_MAX_CMD_LINES, PBF_MAX_CMD_LINES},
	{BTB_MAX_BLOCKS_BB, BTB_MAX_BLOCKS_K2},
};

u16 qed_get_hsi_def_val(struct qed_dev *cdev, enum qed_hsi_def_type type)
{
	enum chip_ids chip_id = QED_IS_BB(cdev) ? CHIP_BB : CHIP_K2;

	if (type >= QED_NUM_HSI_DEFS || type < 0) {
		DP_ERR(cdev, "Unexpected HSI definition type [%d]\n", type);
		return 0;
	}

	return qed_hsi_def_val[type][chip_id];
}

static int
qed_hw_set_soft_resc_size(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 resc_max_val, mcp_resp;
	u8 num_vf_cnqs, res_id;
	int rc;

	num_vf_cnqs = p_hwfn->num_vf_cnqs;

	for (res_id = 0; res_id < QED_MAX_RESC; res_id++) {
		switch (res_id) {
		case QED_LL2_RAM_QUEUE:
			resc_max_val = MAX_NUM_LL2_RX_RAM_QUEUES;
			break;
		case QED_LL2_CTX_QUEUE:
			resc_max_val = MAX_NUM_LL2_RX_CTX_QUEUES;
			break;
		case QED_VF_RDMA_CNQ_RAM:
			resc_max_val = num_vf_cnqs;
			break;
		case QED_RDMA_CNQ_RAM:
			/* No need for a case for QED_CMDQS_CQS since
			 * CNQ/CMDQS are the same resource.
			 */
			resc_max_val = NUM_OF_GLOBAL_QUEUES - num_vf_cnqs;
			break;
		case QED_RDMA_STATS_QUEUE:
			resc_max_val =
			    NUM_OF_RDMA_STATISTIC_COUNTERS(p_hwfn->cdev);
			break;
		case QED_BDQ:
			resc_max_val = SCSI_MAX_NUM_STORAGE_FUNCS;
			break;
		default:
			continue;
		}

		rc = __qed_hw_set_soft_resc_size(p_hwfn, p_ptt, res_id,
						 resc_max_val, &mcp_resp);
		if (rc)
			return rc;

		/* There's no point to continue to the next resource if the
		 * command is not supported by the MFW.
		 * We do continue if the command is supported but the resource
		 * is unknown to the MFW. Such a resource will be later
		 * configured with the default allocation values.
		 */
		if (mcp_resp == FW_MSG_CODE_UNSUPPORTED)
			return -EOPNOTSUPP;
	}

	return 0;
}

static
int qed_hw_get_dflt_resc(struct qed_hwfn *p_hwfn,
			 enum qed_resources res_id,
			 u32 * p_resc_num, u32 * p_resc_start)
{
	u8 num_funcs = p_hwfn->num_funcs_on_engine;
	struct qed_dev *cdev = p_hwfn->cdev;
	u8 num_vf_cnqs = p_hwfn->num_vf_cnqs;

	switch (res_id) {
	case QED_L2_QUEUE:
		*p_resc_num = NUM_OF_L2_QUEUES(cdev) / num_funcs;
		break;
	case QED_VPORT:
		*p_resc_num = NUM_OF_VPORTS(cdev) / num_funcs;
		break;
	case QED_RSS_ENG:
		*p_resc_num = NUM_OF_RSS_ENGINES(cdev) / num_funcs;
		break;
	case QED_PQ:
		*p_resc_num = NUM_OF_QM_TX_QUEUES(cdev) / num_funcs;
		*p_resc_num &= ~0x7;	/* The granularity of the PQs is 8 */
		break;
	case QED_RL:
		*p_resc_num = NUM_OF_QM_GLOBAL_RLS(cdev) / num_funcs;
		break;
	case QED_MAC:
	case QED_VLAN:
		/* Each VFC resource can accommodate both a MAC and a VLAN */
		*p_resc_num = ETH_NUM_MAC_FILTERS / num_funcs;
		break;
	case QED_ILT:
		*p_resc_num = NUM_OF_PXP_ILT_RECORDS(cdev) / num_funcs;
		break;
	case QED_LL2_RAM_QUEUE:
		*p_resc_num = MAX_NUM_LL2_RX_RAM_QUEUES / num_funcs;
		break;
	case QED_LL2_CTX_QUEUE:
		*p_resc_num = MAX_NUM_LL2_RX_CTX_QUEUES / num_funcs;
		break;
	case QED_VF_RDMA_CNQ_RAM:
		*p_resc_num = num_vf_cnqs / num_funcs;
		break;
	case QED_RDMA_CNQ_RAM:
	case QED_CMDQS_CQS:
		/* CNQ/CMDQS are the same resource */
		*p_resc_num = (NUM_OF_GLOBAL_QUEUES - num_vf_cnqs) / num_funcs;
		break;
	case QED_RDMA_STATS_QUEUE:
		*p_resc_num = NUM_OF_RDMA_STATISTIC_COUNTERS(cdev) / num_funcs;
		break;
	case QED_BDQ:
		if (p_hwfn->hw_info.personality != QED_PCI_ISCSI &&
		    p_hwfn->hw_info.personality != QED_PCI_FCOE)
			*p_resc_num = 0;
		else
			*p_resc_num = 1;
		break;
	case QED_VF_MAC_ADDR:
		*p_resc_num = 0;
		break;
	case QED_SB:
		/* Since we want its value to reflect whether MFW supports
		 * the new scheme, have a default of 0.
		 */
		*p_resc_num = 0;
		break;
	default:
		return -EINVAL;
	}

	switch (res_id) {
	case QED_BDQ:
		if (!*p_resc_num)
			*p_resc_start = 0;
		else if (p_hwfn->cdev->num_ports_in_engine == 4)
			*p_resc_start = p_hwfn->port_id;
		else if (p_hwfn->hw_info.personality == QED_PCI_ISCSI)
			*p_resc_start = p_hwfn->port_id;
		else if (p_hwfn->hw_info.personality == QED_PCI_FCOE)
			*p_resc_start = p_hwfn->port_id + 2;
		break;
	default:
		*p_resc_start = *p_resc_num * p_hwfn->enabled_func_idx;
		break;
	}

	return 0;
}

static int
__qed_hw_set_resc_info(struct qed_hwfn *p_hwfn,
		       enum qed_resources res_id, bool drv_resc_alloc)
{
	u32 dflt_resc_num = 0, dflt_resc_start = 0;
	u32 mcp_resp, *p_resc_num, *p_resc_start;
	int rc;

	p_resc_num = &RESC_NUM(p_hwfn, res_id);
	p_resc_start = &RESC_START(p_hwfn, res_id);

	rc = qed_hw_get_dflt_resc(p_hwfn, res_id, &dflt_resc_num,
				  &dflt_resc_start);
	if (rc) {
		DP_ERR(p_hwfn,
		       "Failed to get default amount for resource %d [%s]\n",
		       res_id, qed_hw_get_resc_name(res_id));
		return rc;
	}
#ifndef ASIC_ONLY
	if (CHIP_REV_IS_SLOW(p_hwfn->cdev)) {
		*p_resc_num = dflt_resc_num;
		*p_resc_start = dflt_resc_start;
		goto out;
	}
#endif

	rc = qed_mcp_get_resc_info(p_hwfn, p_hwfn->p_main_ptt, res_id,
				   &mcp_resp, p_resc_num, p_resc_start);
	if (rc) {
		DP_NOTICE(p_hwfn,
			  "MFW response failure for an allocation request for resource %d [%s]\n",
			  res_id, qed_hw_get_resc_name(res_id));
		return rc;
	}

	/* Default driver values are applied in the following cases:
	 * - The resource allocation MB command is not supported by the MFW
	 * - There is an internal error in the MFW while processing the request
	 * - The resource ID is unknown to the MFW
	 */
	if (mcp_resp != FW_MSG_CODE_RESOURCE_ALLOC_OK) {
		DP_INFO(p_hwfn,
			"Failed to receive allocation info for resource %d [%s]. mcp_resp = 0x%x. Applying default values [%d,%d].\n",
			res_id,
			qed_hw_get_resc_name(res_id),
			mcp_resp, dflt_resc_num, dflt_resc_start);
		*p_resc_num = dflt_resc_num;
		*p_resc_start = dflt_resc_start;
		goto out;
	}

	if ((*p_resc_num != dflt_resc_num ||
	     *p_resc_start != dflt_resc_start) && res_id != QED_SB) {
		DP_INFO(p_hwfn,
			"MFW allocation for resource %d [%s] differs from default values [%d,%d vs. %d,%d]%s\n",
			res_id,
			qed_hw_get_resc_name(res_id),
			*p_resc_num,
			*p_resc_start,
			dflt_resc_num,
			dflt_resc_start,
			drv_resc_alloc ? " - Applying default values" : "");
		if (drv_resc_alloc) {
			*p_resc_num = dflt_resc_num;
			*p_resc_start = dflt_resc_start;
		}
	}
out:
	/* PQs have to divide by 8 [that's the HW granularity].
	 * Reduce number so it would fit.
	 */
	if ((res_id == QED_PQ) && ((*p_resc_num % 8) || (*p_resc_start % 8))) {
		DP_INFO(p_hwfn,
			"PQs need to align by 8; Number %08x --> %08x, Start %08x --> %08x\n",
			*p_resc_num,
			(*p_resc_num) & ~0x7,
			*p_resc_start, (*p_resc_start) & ~0x7);
		*p_resc_num &= ~0x7;
		*p_resc_start &= ~0x7;
	}

	/* The last RSS engine is used by the FW for TPA hash calculation.
	 * Old MFW versions allocate it to the drivers, so it needs to be
	 * truncated.
	 */
	if (res_id == QED_RSS_ENG &&
	    *p_resc_num &&
	    (*p_resc_start + *p_resc_num - 1) ==
	    NUM_OF_RSS_ENGINES(p_hwfn->cdev))
		-- * p_resc_num;

	/* In case personality is not RDMA or there is no seperate doorbell bar
	 * for VFs, VF-RDMA will not be supported, thus no need CNQs for VFs.
	 */
	if ((res_id == QED_VF_RDMA_CNQ_RAM) &&
	    (!QED_IS_RDMA_PERSONALITY(p_hwfn) ||
	     !qed_iov_vf_db_bar_size(p_hwfn, p_hwfn->p_main_ptt)))
		*p_resc_num = 0;

	return 0;
}

static int qed_hw_set_resc_info(struct qed_hwfn *p_hwfn, bool drv_resc_alloc)
{
	int rc;
	u8 res_id;

	for (res_id = 0; res_id < QED_MAX_RESC; res_id++) {
		rc = __qed_hw_set_resc_info(p_hwfn, res_id, drv_resc_alloc);
		if (rc)
			return rc;
	}

	return 0;
}

#define QED_NONUSED_PPFID_MASK_BB_4P_LO_PORTS   0xaa
#define QED_NONUSED_PPFID_MASK_BB_4P_HI_PORTS   0x55
#define QED_NONUSED_PPFID_MASK_AH_4P            0xf0

static int qed_hw_get_ppfid_bitmap(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt)
{
	u8 native_ppfid_idx = QED_PPFID_BY_PFID(p_hwfn), new_bitmap;
	struct qed_dev *cdev = p_hwfn->cdev;
	int rc;

	rc = qed_mcp_get_ppfid_bitmap(p_hwfn, p_ptt);
	if (rc != 0 && rc != -EOPNOTSUPP)
		return rc;
	else if (rc == -EOPNOTSUPP)
		cdev->ppfid_bitmap = 0x1 << native_ppfid_idx;

	/* 4-ports mode has limitations that should be enforced:
	 * - BB: the MFW can access only PPFIDs which their corresponding PFIDs
	 *       belong to this certain port.
	 * - AH: only 4 PPFIDs per port are available.
	 */
	if (qed_device_num_ports(cdev) == 4) {
		u8 mask;

		if (QED_IS_BB(cdev))
			mask = MFW_PORT(p_hwfn) > 1 ?
			    QED_NONUSED_PPFID_MASK_BB_4P_HI_PORTS :
			    QED_NONUSED_PPFID_MASK_BB_4P_LO_PORTS;
		else
			mask = QED_NONUSED_PPFID_MASK_AH_4P;

		if (cdev->ppfid_bitmap & mask) {
			new_bitmap = cdev->ppfid_bitmap & ~mask;
			DP_INFO(p_hwfn,
				"Fix the PPFID bitmap for 4-ports mode: 0x%hhx -> 0x%hhx\n",
				cdev->ppfid_bitmap, new_bitmap);
			cdev->ppfid_bitmap = new_bitmap;
		}
	}

	/* The native PPFID is expected to be part of the allocated bitmap */
	if (!(cdev->ppfid_bitmap & (0x1 << native_ppfid_idx))) {
		new_bitmap = 0x1 << native_ppfid_idx;
		DP_INFO(p_hwfn,
			"Fix the PPFID bitmap to inculde the native PPFID: %hhd -> 0x%hhx\n",
			cdev->ppfid_bitmap, new_bitmap);
		cdev->ppfid_bitmap = new_bitmap;
	}

	return 0;
}

static int qed_hw_get_resc(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, bool drv_resc_alloc)
{
	struct qed_resc_unlock_params resc_unlock_params;
	struct qed_resc_lock_params resc_lock_params;
	struct qed_dev *cdev = p_hwfn->cdev;
	u32 max_ilt_lines;
	u8 res_id;
	int rc;

#ifndef ASIC_ONLY
	u32 *resc_start = p_hwfn->hw_info.resc_start;
	u32 *resc_num = p_hwfn->hw_info.resc_num;
	/* For AH, an equal share of the ILT lines between the maximal number of
	 * PFs is not enough for RoCE. This would be solved by the future
	 * resource allocation scheme, but isn't currently present for
	 * FPGA/emulation. For now we keep a number that is sufficient for RoCE
	 * to work - the BB number of ILT lines divided by its max PFs number.
	 */
	u32 roce_min_ilt_lines = PXP_NUM_ILT_RECORDS_BB / MAX_NUM_PFS_BB;
#endif

	/* Setting the max values of the soft resources and the following
	 * resources allocation queries should be atomic. Since several PFs can
	 * run in parallel - a resource lock is needed.
	 * If either the resource lock or resource set value commands are not
	 * supported - skip the the max values setting, release the lock if
	 * needed, and proceed to the queries. Other failures, including a
	 * failure to acquire the lock, will cause this function to fail.
	 * Old drivers that don't acquire the lock can run in parallel, and
	 * their allocation values won't be affected by the updated max values.
	 */

	qed_mcp_resc_lock_default_init(p_hwfn, &resc_lock_params,
				       &resc_unlock_params,
				       QED_RESC_LOCK_RESC_ALLOC, false);

	/* Changes on top of the default values to accommodate parallel attempts
	 * of several PFs.
	 * [10 x 10 msec by default ==> 20 x 50 msec]
	 */
	resc_lock_params.retry_num *= 2;
	resc_lock_params.retry_interval *= 5;

	rc = qed_mcp_resc_lock(p_hwfn, p_ptt, &resc_lock_params);
	if (rc != 0 && rc != -EOPNOTSUPP) {
		return rc;
	} else if (rc == -EOPNOTSUPP) {
		DP_INFO(p_hwfn,
			"Skip the max values setting of the soft resources since the resource lock is not supported by the MFW\n");
	} else if (rc == 0 && !resc_lock_params.b_granted) {
		DP_NOTICE(p_hwfn,
			  "Failed to acquire the resource lock for the resource allocation commands\n");
		return -EBUSY;
	} else {
		rc = qed_hw_set_soft_resc_size(p_hwfn, p_ptt);
		if (rc != 0 && rc != -EOPNOTSUPP) {
			DP_NOTICE(p_hwfn,
				  "Failed to set the max values of the soft resources\n");
			goto unlock_and_exit;
		} else if (rc == -EOPNOTSUPP) {
			DP_INFO(p_hwfn,
				"Skip the max values setting of the soft resources since it is not supported by the MFW\n");
			rc = qed_mcp_resc_unlock(p_hwfn, p_ptt,
						 &resc_unlock_params);
			if (rc)
				DP_INFO(p_hwfn,
					"Failed to release the resource lock for the resource allocation commands\n");
		}
	}

	rc = qed_hw_set_resc_info(p_hwfn, drv_resc_alloc);
	if (rc)
		goto unlock_and_exit;

	if (resc_lock_params.b_granted && !resc_unlock_params.b_released) {
		rc = qed_mcp_resc_unlock(p_hwfn, p_ptt, &resc_unlock_params);
		if (rc)
			DP_INFO(p_hwfn,
				"Failed to release the resource lock for the resource allocation commands\n");
	}

	/* PPFID bitmap */
	if (IS_LEAD_HWFN(p_hwfn)) {
		rc = qed_hw_get_ppfid_bitmap(p_hwfn, p_ptt);
		if (rc)
			return rc;
	}
#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(cdev)) {
		/* Reduced build contains less PQs */
		if (!(cdev->b_is_emul_full)) {
			resc_num[QED_PQ] = 32;
			resc_start[QED_PQ] = resc_num[QED_PQ] *
			    p_hwfn->enabled_func_idx;
		}

		/* For AH emulation, since we have a possible maximal number of
		 * 16 enabled PFs, in case there are not enough ILT lines -
		 * allocate only first PF as RoCE and have all the other as
		 * ETH-only with less ILT lines.
		 * In case we increase the number of ILT lines for PF0, we need
		 * also to correct the start value for PF1-15.
		 */
		if (QED_IS_AH(cdev) && cdev->b_is_emul_full) {
			if (!p_hwfn->rel_pf_id) {
				resc_num[QED_ILT] =
				    max_t(u32, resc_num[QED_ILT],
					  roce_min_ilt_lines);
			} else if (resc_num[QED_ILT] < roce_min_ilt_lines) {
				resc_start[QED_ILT] += roce_min_ilt_lines -
				    resc_num[QED_ILT];
			}
		}
	}
#endif

	/* Sanity for ILT */
	max_ilt_lines = NUM_OF_PXP_ILT_RECORDS(cdev);
	if (RESC_END(p_hwfn, QED_ILT) > max_ilt_lines) {
		DP_NOTICE(p_hwfn, "Can't assign ILT pages [%08x,...,%08x]\n",
			  RESC_START(p_hwfn, QED_ILT),
			  RESC_END(p_hwfn, QED_ILT) - 1);
		return -EINVAL;
	}

	/* This will also learn the number of SBs from MFW */
	if (qed_int_igu_reset_cam(p_hwfn, p_ptt))
		return -EINVAL;

	qed_hw_set_feat(p_hwfn);

	DP_VERBOSE(p_hwfn, NETIF_MSG_PROBE,
		   "The numbers for each resource are:\n");
	for (res_id = 0; res_id < QED_MAX_RESC; res_id++)
		DP_VERBOSE(p_hwfn, NETIF_MSG_PROBE, "%s = %d start = %d\n",
			   qed_hw_get_resc_name(res_id),
			   RESC_NUM(p_hwfn, res_id),
			   RESC_START(p_hwfn, res_id));

	return 0;

unlock_and_exit:
	if (resc_lock_params.b_granted && !resc_unlock_params.b_released)
		qed_mcp_resc_unlock(p_hwfn, p_ptt, &resc_unlock_params);
	return rc;
}

static bool qed_is_dscp_mapping_allowed(struct qed_hwfn *p_hwfn, u32 mf_mode)
{
	/* HW bug:
	 * The NIG accesses the "NIG_REG_DSCP_TO_TC_MAP_ENABLE" PORT_PF register
	 * with the PFID, as set in "NIG_REG_LLH_PPFID2PFID_TBL", instead of
	 * with the PPFID. AH is being affected from this bug, and thus DSCP to
	 * TC mapping is only allowed on the following configurations:
	 * - QPAR, 2-ports
	 * - QPAR, 4-ports, single PF on a port
	 * There is no impact on BB since it has another bug in which the PPFID
	 * is actually engine based.
	 */
	return !QED_IS_AH(p_hwfn->cdev) ||
	    (mf_mode == NVM_CFG1_GLOB_MF_MODE_DEFAULT &&
	     (qed_device_num_ports(p_hwfn->cdev) == 2 ||
	      (qed_device_num_ports(p_hwfn->cdev) == 4 &&
	       p_hwfn->num_funcs_on_port == 1)));
}

#ifndef ASIC_ONLY
static int qed_emul_hw_get_nvm_info(struct qed_hwfn *p_hwfn)
{
	if (IS_LEAD_HWFN(p_hwfn)) {
		struct qed_dev *cdev = p_hwfn->cdev;

		/* The MF mode on emulation is either default or NPAR 1.0 */
		cdev->mf_bits = 1 << QED_MF_LLH_MAC_CLSS |
		    1 << QED_MF_LLH_PROTO_CLSS | 1 << QED_MF_LL2_NON_UNICAST;
		if (p_hwfn->num_funcs_on_port > 1)
			cdev->mf_bits |= 1 << QED_MF_INTER_PF_SWITCH |
			    1 << QED_MF_DISABLE_ARFS;
		else
			cdev->mf_bits |= 1 << QED_MF_NEED_DEF_PF;
	}

	return 0;
}
#endif

static int
qed_hw_get_nvm_info(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    struct qed_hw_prepare_params *p_params)
{
	u32 port_cfg_addr, link_temp, nvm_cfg_addr, device_capabilities;
	u32 nvm_cfg1_offset, mf_mode, addr, generic_cont0, core_cfg;
	struct qed_mcp_link_capabilities *p_caps;
	struct qed_mcp_link_params *link;
	int rc;

	u32 i;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_SLOW(p_hwfn->cdev) || p_hwfn->mcp_info->recovery_mode)
		return qed_emul_hw_get_nvm_info(p_hwfn);
#endif

	/* Read global nvm_cfg address */
	nvm_cfg_addr = qed_rd(p_hwfn, p_ptt, MISC_REG_GEN_PURP_CR0);

	/* Verify MCP has initialized it */
	if (!nvm_cfg_addr) {
		DP_NOTICE(p_hwfn, "Shared memory not initialized\n");
		if (p_params->b_relaxed_probe)
			p_params->p_relaxed_res = QED_HW_PREPARE_FAILED_NVM;
		return -EINVAL;
	}

	/* Read nvm_cfg1  (Notice this is just offset, and not offsize (TBD) */
	nvm_cfg1_offset = qed_rd(p_hwfn, p_ptt, nvm_cfg_addr + 4);

	addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
	    offsetof(struct nvm_cfg1, glob)+offsetof(struct nvm_cfg1_glob,
						     core_cfg);

	core_cfg = qed_rd(p_hwfn, p_ptt, addr);

	switch (GET_MFW_FIELD(core_cfg, NVM_CFG1_GLOB_NETWORK_PORT_MODE)) {
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_2X40G:
		p_hwfn->hw_info.port_mode = QED_PORT_MODE_DE_2X40G;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X50G:
		p_hwfn->hw_info.port_mode = QED_PORT_MODE_DE_2X50G;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_1X100G:
		p_hwfn->hw_info.port_mode = QED_PORT_MODE_DE_1X100G;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_4X10G_F:
		p_hwfn->hw_info.port_mode = QED_PORT_MODE_DE_4X10G_F;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_4X10G_E:
		p_hwfn->hw_info.port_mode = QED_PORT_MODE_DE_4X10G_E;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_4X20G:
		p_hwfn->hw_info.port_mode = QED_PORT_MODE_DE_4X20G;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_1X40G:
		p_hwfn->hw_info.port_mode = QED_PORT_MODE_DE_1X40G;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X25G:
		p_hwfn->hw_info.port_mode = QED_PORT_MODE_DE_2X25G;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X10G:
		p_hwfn->hw_info.port_mode = QED_PORT_MODE_DE_2X10G;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_1X25G:
		p_hwfn->hw_info.port_mode = QED_PORT_MODE_DE_1X25G;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_4X25G:
		p_hwfn->hw_info.port_mode = QED_PORT_MODE_DE_4X25G;
		break;
	default:
		DP_NOTICE(p_hwfn, "Unknown port mode in 0x%08x\n", core_cfg);
		break;
	}

	/* Read default link configuration */
	link = &p_hwfn->mcp_info->link_input;
	p_caps = &p_hwfn->mcp_info->link_capabilities;
	port_cfg_addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
	    offsetof(struct nvm_cfg1, port[MFW_PORT(p_hwfn)]);
	link_temp = qed_rd(p_hwfn, p_ptt,
			   port_cfg_addr +
			   offsetof(struct nvm_cfg1_port, speed_cap_mask));
	link_temp &= NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_MASK;
	link->speed.advertised_speeds = link_temp;
	p_caps->speed_capabilities = link->speed.advertised_speeds;

	link_temp = qed_rd(p_hwfn, p_ptt,
			   port_cfg_addr +
			   offsetof(struct nvm_cfg1_port, link_settings));
	switch (GET_MFW_FIELD(link_temp, NVM_CFG1_PORT_DRV_LINK_SPEED)) {
	case NVM_CFG1_PORT_DRV_LINK_SPEED_AUTONEG:
		link->speed.autoneg = true;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_1G:
		link->speed.forced_speed = 1000;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_10G:
		link->speed.forced_speed = 10000;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_20G:
		link->speed.forced_speed = 20000;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_25G:
		link->speed.forced_speed = 25000;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_40G:
		link->speed.forced_speed = 40000;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_50G:
		link->speed.forced_speed = 50000;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_BB_AHP_100G:
		link->speed.forced_speed = 100000;
		break;
	default:
		DP_NOTICE(p_hwfn, "Unknown Speed in 0x%08x\n", link_temp);
	}

	p_caps->default_speed_autoneg = link->speed.autoneg;

	link_temp = GET_MFW_FIELD(link_temp, NVM_CFG1_PORT_DRV_FLOW_CONTROL);
	link->pause.autoneg = ! !(link_temp &
				  NVM_CFG1_PORT_DRV_FLOW_CONTROL_AUTONEG);
	link->pause.forced_rx = ! !(link_temp &
				    NVM_CFG1_PORT_DRV_FLOW_CONTROL_RX);
	link->pause.forced_tx = ! !(link_temp &
				    NVM_CFG1_PORT_DRV_FLOW_CONTROL_TX);
	link->loopback_mode = 0;

	if (p_hwfn->mcp_info->capabilities &
	    FW_MB_PARAM_FEATURE_SUPPORT_FEC_CONTROL) {
		link_temp = qed_rd(p_hwfn, p_ptt, port_cfg_addr +
				   offsetof(struct nvm_cfg1_port,
					    link_settings));
		switch (GET_MFW_FIELD(link_temp, NVM_CFG1_PORT_FEC_FORCE_MODE)) {
		case NVM_CFG1_PORT_FEC_FORCE_MODE_NONE:
			p_caps->fec_default |= QED_MCP_FEC_NONE;
			break;
		case NVM_CFG1_PORT_FEC_FORCE_MODE_FIRECODE:
			p_caps->fec_default |= QED_MCP_FEC_FIRECODE;
			break;
		case NVM_CFG1_PORT_FEC_FORCE_MODE_RS:
			p_caps->fec_default |= QED_MCP_FEC_RS;
			break;
		case NVM_CFG1_PORT_FEC_FORCE_MODE_AUTO:
			p_caps->fec_default |= QED_MCP_FEC_AUTO;
			break;
		default:
			DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
				   "unknown FEC mode in 0x%08x\n", link_temp);
		}
	} else {
		p_caps->fec_default = QED_MCP_FEC_UNSUPPORTED;
	}

	link->fec = p_caps->fec_default;

	if (p_hwfn->mcp_info->capabilities & FW_MB_PARAM_FEATURE_SUPPORT_EEE) {
		link_temp = qed_rd(p_hwfn, p_ptt, port_cfg_addr +
				   offsetof(struct nvm_cfg1_port, ext_phy));
		link_temp = GET_MFW_FIELD(link_temp,
					  NVM_CFG1_PORT_EEE_POWER_SAVING_MODE);
		p_caps->default_eee = QED_MCP_EEE_ENABLED;
		link->eee.enable = true;
		switch (link_temp) {
		case NVM_CFG1_PORT_EEE_POWER_SAVING_MODE_DISABLED:
			p_caps->default_eee = QED_MCP_EEE_DISABLED;
			link->eee.enable = false;
			break;
		case NVM_CFG1_PORT_EEE_POWER_SAVING_MODE_BALANCED:
			p_caps->eee_lpi_timer = EEE_TX_TIMER_USEC_BALANCED_TIME;
			break;
		case NVM_CFG1_PORT_EEE_POWER_SAVING_MODE_AGGRESSIVE:
			p_caps->eee_lpi_timer =
			    EEE_TX_TIMER_USEC_AGGRESSIVE_TIME;
			break;
		case NVM_CFG1_PORT_EEE_POWER_SAVING_MODE_LOW_LATENCY:
			p_caps->eee_lpi_timer = EEE_TX_TIMER_USEC_LATENCY_TIME;
			break;
		}

		link->eee.tx_lpi_timer = p_caps->eee_lpi_timer;
		link->eee.tx_lpi_enable = link->eee.enable;
		link->eee.adv_caps = QED_EEE_1G_ADV | QED_EEE_10G_ADV;
	} else {
		p_caps->default_eee = QED_MCP_EEE_UNSUPPORTED;
	}

	DP_VERBOSE(p_hwfn,
		   NETIF_MSG_LINK,
		   "Read default link: Speed 0x%08x, Adv. Speed 0x%08x, AN: 0x%02x, PAUSE AN: 0x%02x EEE: %02x [%08x usec], FEC: 0x%2x\n",
		   link->speed.forced_speed,
		   link->speed.advertised_speeds,
		   link->speed.autoneg,
		   link->pause.autoneg,
		   p_caps->default_eee,
		   p_caps->eee_lpi_timer, p_caps->fec_default);

	if (IS_LEAD_HWFN(p_hwfn)) {
		struct qed_dev *cdev = p_hwfn->cdev;

		/* Read Multi-function information from shmem */
		addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
		    offsetof(struct nvm_cfg1, glob) +
		    offsetof(struct nvm_cfg1_glob, generic_cont0);
		generic_cont0 = qed_rd(p_hwfn, p_ptt, addr);
		mf_mode = GET_MFW_FIELD(generic_cont0, NVM_CFG1_GLOB_MF_MODE);

		switch (mf_mode) {
		case NVM_CFG1_GLOB_MF_MODE_MF_ALLOWED:
			cdev->mf_bits = 1 << QED_MF_OVLAN_CLSS |
			    1 << QED_MF_VF_RDMA | 1 << QED_MF_LARGE_ILT;
			break;
		case NVM_CFG1_GLOB_MF_MODE_UFP:
			cdev->mf_bits = 1 << QED_MF_OVLAN_CLSS |
			    1 << QED_MF_LLH_PROTO_CLSS |
			    1 << QED_MF_UFP_SPECIFIC |
			    1 << QED_MF_8021Q_TAGGING |
			    1 << QED_MF_DONT_ADD_VLAN0_TAG |
			    1 << QED_MF_VF_RDMA | 1 << QED_MF_LARGE_ILT;
			break;
		case NVM_CFG1_GLOB_MF_MODE_BD:
			cdev->mf_bits = 1 << QED_MF_OVLAN_CLSS |
			    1 << QED_MF_LLH_PROTO_CLSS |
			    1 << QED_MF_8021AD_TAGGING |
			    1 << QED_MF_FIP_SPECIAL |
			    1 << QED_MF_DONT_ADD_VLAN0_TAG |
			    1 << QED_MF_VF_RDMA | 1 << QED_MF_LARGE_ILT;
			break;
		case NVM_CFG1_GLOB_MF_MODE_NPAR1_0:
			cdev->mf_bits = 1 << QED_MF_LLH_MAC_CLSS |
			    1 << QED_MF_LLH_PROTO_CLSS |
			    1 << QED_MF_LL2_NON_UNICAST |
			    1 << QED_MF_INTER_PF_SWITCH |
			    1 << QED_MF_DISABLE_ARFS | 1 << QED_MF_LARGE_ILT;
			break;
		case NVM_CFG1_GLOB_MF_MODE_DEFAULT:
			cdev->mf_bits = 1 << QED_MF_LLH_MAC_CLSS |
			    1 << QED_MF_LLH_PROTO_CLSS |
			    1 << QED_MF_LL2_NON_UNICAST |
			    1 << QED_MF_VF_RDMA | 1 << QED_MF_ROCE_LAG;
			if (QED_IS_BB(cdev))
				cdev->mf_bits |= 1 << QED_MF_NEED_DEF_PF;
			break;
		case NVM_CFG1_GLOB_MF_MODE_QINQ:
			cdev->mf_bits = 1 << QED_MF_OVLAN_CLSS |
			    1 << QED_MF_LLH_PROTO_CLSS |
			    1 << QED_MF_QINQ_SPECIFIC |
			    1 << QED_MF_8021Q_TAGGING |
			    1 << QED_MF_DONT_ADD_VLAN0_TAG |
			    1 << QED_MF_LARGE_ILT;
			break;
		}

		DP_INFO(cdev, "Multi function mode is 0x%lx\n", cdev->mf_bits);

		/* In CMT the PF is unknown when the GFS block processes the
		 * packet. Therefore cannot use searcher as it has a per PF
		 * database, and thus ARFS must be disabled.
		 *
		 */
		if (QED_IS_CMT(cdev))
			cdev->mf_bits |= 1 << QED_MF_DISABLE_ARFS;

		if (qed_is_dscp_mapping_allowed(p_hwfn, mf_mode))
			cdev->mf_bits |= 1 << QED_MF_DSCP_TO_TC_MAP;
	}

	/* Read device capabilities information from shmem */
	addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
	    offsetof(struct nvm_cfg1, glob) +
	    offsetof(struct nvm_cfg1_glob, device_capabilities);

	device_capabilities = qed_rd(p_hwfn, p_ptt, addr);
	if (device_capabilities & NVM_CFG1_GLOB_DEVICE_CAPABILITIES_ETHERNET)
		__set_bit(QED_DEV_CAP_ETH,
			  &p_hwfn->hw_info.device_capabilities);
	if (device_capabilities & NVM_CFG1_GLOB_DEVICE_CAPABILITIES_FCOE)
		__set_bit(QED_DEV_CAP_FCOE,
			  &p_hwfn->hw_info.device_capabilities);
	if (device_capabilities & NVM_CFG1_GLOB_DEVICE_CAPABILITIES_ISCSI)
		__set_bit(QED_DEV_CAP_ISCSI,
			  &p_hwfn->hw_info.device_capabilities);
	if (device_capabilities & NVM_CFG1_GLOB_DEVICE_CAPABILITIES_ROCE)
		__set_bit(QED_DEV_CAP_ROCE,
			  &p_hwfn->hw_info.device_capabilities);
	if (device_capabilities & NVM_CFG1_GLOB_DEVICE_CAPABILITIES_IWARP)
		__set_bit(QED_DEV_CAP_IWARP,
			  &p_hwfn->hw_info.device_capabilities);

	/* Read device serial number information from shmem */
	addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
	    offsetof(struct nvm_cfg1, glob) +
	    offsetof(struct nvm_cfg1_glob, serial_number);

	for (i = 0; i < ARRAY_SIZE(p_hwfn->hw_info.part_num); i++)
		p_hwfn->hw_info.part_num[i] =
		    qed_rd(p_hwfn, p_ptt, addr + i * sizeof(u32));

	rc = qed_mcp_fill_shmem_func_info(p_hwfn, p_ptt);
	if (rc != 0 && p_params->b_relaxed_probe) {
		rc = 0;
		p_params->p_relaxed_res = QED_HW_PREPARE_BAD_MCP;
	}

	return rc;
}

static void qed_get_num_funcs(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u8 num_funcs, enabled_func_idx, num_funcs_on_port;
	struct qed_dev *cdev = p_hwfn->cdev;
	u32 reg_function_hide;

	/* Default "worst-case" values */
	num_funcs = QED_IS_AH(cdev) ? MAX_NUM_PFS_K2 : MAX_NUM_PFS_BB;
	enabled_func_idx = p_hwfn->rel_pf_id;
	num_funcs_on_port = MAX_PF_PER_PORT;

	/* Bit 0 of MISCS_REG_FUNCTION_HIDE indicates whether the bypass values
	 * in the other bits are selected.
	 * Bits 1-15 are for functions 1-15, respectively, and their value is
	 * '0' only for enabled functions (function 0 always exists and
	 * enabled).
	 * In case of CMT in BB, only the "even" functions are enabled, and thus
	 * the number of functions for both hwfns is learnt from the same bits.
	 */
	reg_function_hide = qed_rd(p_hwfn, p_ptt, MISCS_REG_FUNCTION_HIDE);

	if (reg_function_hide & 0x1) {
		u32 enabled_funcs, eng_mask, low_pfs_mask, port_mask, tmp;

		/* Get the number of the enabled functions on the device */
		enabled_funcs = (reg_function_hide & 0xffff) ^ 0xfffe;

		/* Get the number of the enabled functions on the engine */
		if (QED_IS_BB(cdev)) {
			if (QED_PATH_ID(p_hwfn) && !QED_IS_CMT(cdev))
				eng_mask = 0xaaaa;
			else
				eng_mask = 0x5555;
		} else {
			eng_mask = 0xffff;
		}

		num_funcs = 0;
		tmp = enabled_funcs & eng_mask;
		while (tmp) {
			if (tmp & 0x1)
				num_funcs++;
			tmp >>= 0x1;
		}

		/* Get the PF index within the enabled functions */
		low_pfs_mask = BIT(QED_LEADING_HWFN(cdev)->abs_pf_id) - 1;
		enabled_func_idx = 0;
		tmp = enabled_funcs & eng_mask & low_pfs_mask;
		while (tmp) {
			if (tmp & 0x1)
				enabled_func_idx++;
			tmp >>= 0x1;
		}

		/* Get the number of functions on the port */
		if (qed_device_num_ports(p_hwfn->cdev) == 4)
			port_mask = 0x1111 << (p_hwfn->abs_pf_id % 4);
		else if (qed_device_num_ports(p_hwfn->cdev) == 2)
			port_mask = 0x5555 << (p_hwfn->abs_pf_id % 2);
		else		/* single port */
			port_mask = 0xffff;

		num_funcs_on_port = 0;
		tmp = enabled_funcs & port_mask;
		while (tmp) {
			if (tmp & 0x1)
				num_funcs_on_port++;
			tmp >>= 0x1;
		}
	}

	p_hwfn->num_funcs_on_engine = num_funcs;
	p_hwfn->enabled_func_idx = enabled_func_idx;
	p_hwfn->num_funcs_on_port = num_funcs_on_port;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_FPGA(cdev)) {
		DP_NOTICE(p_hwfn,
			  "FPGA: Limit number of PFs to 4 [would affect resource allocation, needed for IOV]\n");
		p_hwfn->num_funcs_on_engine = 4;
	}
#endif

	DP_VERBOSE(p_hwfn,
		   NETIF_MSG_PROBE,
		   "PF {abs %d, rel %d}: enabled func %d, %d funcs on engine, %d funcs on port [reg_function_hide 0x%x]\n",
		   p_hwfn->abs_pf_id,
		   p_hwfn->rel_pf_id,
		   p_hwfn->enabled_func_idx,
		   p_hwfn->num_funcs_on_engine,
		   p_hwfn->num_funcs_on_port, reg_function_hide);
}

#ifndef ASIC_ONLY
static void qed_emul_hw_info_port_num(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	u32 eco_reserved;

	/* MISCS_REG_ECO_RESERVED[15:12]: num of ports in an engine */
	eco_reserved = qed_rd(p_hwfn, p_ptt, MISCS_REG_ECO_RESERVED);
	switch ((eco_reserved & 0xf000) >> 12) {
	case 1:
		cdev->num_ports_in_engine = 1;
		break;
	case 3:
		cdev->num_ports_in_engine = 2;
		break;
	case 0xf:
		cdev->num_ports_in_engine = 4;
		break;
	default:
		DP_NOTICE(p_hwfn,
			  "Emulation: Unknown port mode [ECO_RESERVED 0x%08x]\n",
			  eco_reserved);
		cdev->num_ports_in_engine = 1;	/* Default to something */
		break;
	}

	cdev->num_ports = cdev->num_ports_in_engine *
	    qed_device_num_engines(cdev);
}
#endif

/* Determine the number of ports of the device and per engine */
static void qed_hw_info_port_num(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 addr, global_offsize, global_addr, port_mode;
	struct qed_dev *cdev = p_hwfn->cdev;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_TEDIBEAR(cdev)) {
		cdev->num_ports_in_engine = 1;
		cdev->num_ports = 2;
		return;
	}

	if (CHIP_REV_IS_EMUL(cdev)) {
		qed_emul_hw_info_port_num(p_hwfn, p_ptt);
		return;
	}
#endif

	/* In CMT there is always only one port */
	if (QED_IS_CMT(cdev)) {
		cdev->num_ports_in_engine = 1;
		cdev->num_ports = 1;
		return;
	}

	/* Determine the number of ports per engine */
	port_mode = qed_rd(p_hwfn, p_ptt, MISC_REG_PORT_MODE);
	switch (port_mode) {
	case 0x0:
		cdev->num_ports_in_engine = 1;
		break;
	case 0x1:
		cdev->num_ports_in_engine = 2;
		break;
	case 0x2:
		cdev->num_ports_in_engine = 4;
		break;
	default:
		DP_NOTICE(p_hwfn, "Unknown port mode 0x%08x\n", port_mode);
		cdev->num_ports_in_engine = 1;	/* Default to something */
		break;
	}

	/* Get the total number of ports of the device */
	addr = SECTION_OFFSIZE_ADDR(p_hwfn->mcp_info->public_base,
				    PUBLIC_GLOBAL);
	global_offsize = qed_rd(p_hwfn, p_ptt, addr);
	global_addr = SECTION_ADDR(global_offsize, 0);
	addr = global_addr + offsetof(struct public_global, max_ports);
	cdev->num_ports = (u8) qed_rd(p_hwfn, p_ptt, addr);
}

static void qed_mcp_get_eee_caps(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mcp_link_capabilities *p_caps;
	u32 eee_status;

	p_caps = &p_hwfn->mcp_info->link_capabilities;
	if (p_caps->default_eee == QED_MCP_EEE_UNSUPPORTED)
		return;

	p_caps->eee_speed_caps = 0;
	eee_status = qed_rd(p_hwfn,
			    p_ptt,
			    p_hwfn->mcp_info->port_addr +
			    offsetof(struct public_port, eee_status));
	eee_status = GET_MFW_FIELD(eee_status, EEE_SUPPORTED_SPEED);
	if (eee_status & EEE_1G_SUPPORTED)
		p_caps->eee_speed_caps |= QED_EEE_1G_ADV;
	if (eee_status & EEE_10G_ADV)
		p_caps->eee_speed_caps |= QED_EEE_10G_ADV;
}

static int
qed_get_hw_info(struct qed_hwfn *p_hwfn,
		struct qed_ptt *p_ptt,
		enum qed_pci_personality personality,
		struct qed_hw_prepare_params *p_params)
{
	bool drv_resc_alloc = p_params->drv_resc_alloc;
	int rc;

	if (IS_QED_PACING(p_hwfn))
		DP_VERBOSE(p_hwfn->cdev, QED_MSG_IOV,
			   "Skipping IOV as packet pacing is requested\n");

	/* Since all information is common, only first hwfns should do this */
	if (IS_LEAD_HWFN(p_hwfn) && !IS_QED_PACING(p_hwfn) &&
	    !IS_QED_DCQCN(p_hwfn) && !p_params->b_sriov_disable) {
		rc = qed_iov_hw_info(p_hwfn);
		if (rc) {
			if (p_params->b_relaxed_probe)
				p_params->p_relaxed_res =
				    QED_HW_PREPARE_BAD_IOV;
			else
				return rc;
		}
	}

	/* The following order should be kept:
	 * (1) qed_hw_info_port_num(),
	 * (2) qed_get_num_funcs(),
	 * (3) qed_mcp_get_capabilities, and
	 * (4) qed_hw_get_nvm_info(),
	 * since (2) depends on (1) and (4) depends on both.
	 * In addition, can send the MFW a MB command only after (1) is called.
	 */
	if (IS_LEAD_HWFN(p_hwfn))
		qed_hw_info_port_num(p_hwfn, p_ptt);

	qed_get_num_funcs(p_hwfn, p_ptt);

	qed_mcp_get_capabilities(p_hwfn, p_ptt);

	rc = qed_hw_get_nvm_info(p_hwfn, p_ptt, p_params);
	if (rc)
		return rc;

	if (p_hwfn->mcp_info->recovery_mode)
		return 0;

	rc = qed_int_igu_read_cam(p_hwfn, p_ptt);
	if (rc) {
		if (p_params->b_relaxed_probe)
			p_params->p_relaxed_res = QED_HW_PREPARE_BAD_IGU;
		else
			return rc;
	}

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_ASIC(p_hwfn->cdev) && qed_mcp_is_init(p_hwfn)) {
#endif
		memcpy(p_hwfn->hw_info.hw_mac_addr,
		       p_hwfn->mcp_info->func_info.mac, ETH_ALEN);
#ifndef ASIC_ONLY
	} else {
		static u8 mcp_hw_mac[6] = { 0, 2, 3, 4, 5, 6 }
		;

		memcpy(p_hwfn->hw_info.hw_mac_addr, mcp_hw_mac, ETH_ALEN);
		p_hwfn->hw_info.hw_mac_addr[5] = p_hwfn->abs_pf_id;
	}
#endif

	if (qed_mcp_is_init(p_hwfn)) {
		if (p_hwfn->mcp_info->func_info.ovlan != QED_MCP_VLAN_UNSET)
			p_hwfn->hw_info.ovlan =
			    p_hwfn->mcp_info->func_info.ovlan;

		qed_mcp_cmd_port_init(p_hwfn, p_ptt);

		qed_mcp_get_eee_caps(p_hwfn, p_ptt);

		qed_mcp_read_ufp_config(p_hwfn, p_ptt);

		qed_mcp_read_qinq_config(p_hwfn, p_ptt);
	}

	if (personality != QED_PCI_DEFAULT) {
		p_hwfn->hw_info.personality = personality;
	} else if (qed_mcp_is_init(p_hwfn)) {
		enum qed_pci_personality protocol;

		protocol = p_hwfn->mcp_info->func_info.protocol;
		p_hwfn->hw_info.personality = protocol;
	}
#ifndef ASIC_ONLY
	else if (CHIP_REV_IS_EMUL(p_hwfn->cdev)) {
		/* AH emulation:
		 * Allow only PF0 to be RoCE to overcome a lack of ILT lines.
		 */
		if (QED_IS_AH(p_hwfn->cdev) && p_hwfn->rel_pf_id)
			p_hwfn->hw_info.personality = QED_PCI_ETH;
		else
			p_hwfn->hw_info.personality = QED_PCI_ETH_ROCE;
	}
#endif

	if (QED_IS_ROCE_PERSONALITY(p_hwfn))
		p_hwfn->hw_info.multi_tc_roce_en = 1;

	/* although in BB some constellations may support more than 4 tcs,
	 * that can result in performance penalty in some cases. 4
	 * represents a good tradeoff between performance and flexibility.
	 */
	if (IS_QED_PACING(p_hwfn))
		p_hwfn->hw_info.num_hw_tc = 1;
	else
		p_hwfn->hw_info.num_hw_tc = NUM_PHYS_TCS_4PORT_K2;

	/* start out with a single active tc. This can be increased either
	 * by dcbx negotiation or by upper layer driver
	 */
	p_hwfn->hw_info.num_active_tc = 1;

	if (qed_mcp_is_init(p_hwfn))
		p_hwfn->hw_info.mtu = p_hwfn->mcp_info->func_info.mtu;

	/* Due to a dependency, qed_hw_get_resc() should be called after
	 * getting the number of functions on an engine, and after initializing
	 * the personality.
	 */
	rc = qed_hw_get_resc(p_hwfn, p_ptt, drv_resc_alloc);
	if (rc != 0 && p_params->b_relaxed_probe) {
		rc = 0;
		p_params->p_relaxed_res = QED_HW_PREPARE_BAD_MCP;
	}
	return rc;
}

#define QED_MAX_DEVICE_NAME_LEN (8)

void qed_get_dev_name(struct qed_dev *cdev, u8 * name, u8 max_chars)
{
	u8 n;

	n = min_t(u8, max_chars, QED_MAX_DEVICE_NAME_LEN);
	scnprintf((char *)name, n, "%s %c%d",
		  QED_IS_BB(cdev) ? "BB" : "AH",
		  'A' + cdev->chip_rev, (int)cdev->chip_metal);
}

static int qed_get_dev_info(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	u16 device_id_mask;
	u32 tmp;

	/* Read Vendor Id / Device Id */
	pci_read_config_word(cdev->pdev, PCI_VENDOR_ID, &cdev->vendor_id);
	pci_read_config_word(cdev->pdev, PCI_DEVICE_ID, &cdev->device_id);

	/* Determine type */
	device_id_mask = cdev->device_id & QED_DEV_ID_MASK;
	switch (device_id_mask) {
	case QED_DEV_ID_MASK_BB:
		cdev->type = QED_DEV_TYPE_BB;
		break;
	case QED_DEV_ID_MASK_AH:
		cdev->type = QED_DEV_TYPE_AH;
		break;
	default:
		DP_NOTICE(p_hwfn, "Unknown device id 0x%x\n", cdev->device_id);
		return -EBUSY;
	}

	tmp = qed_rd(p_hwfn, p_ptt, MISCS_REG_CHIP_NUM);
	cdev->chip_num = (u16) GET_FIELD(tmp, CHIP_NUM);
	tmp = qed_rd(p_hwfn, p_ptt, MISCS_REG_CHIP_REV);
	cdev->chip_rev = (u8) GET_FIELD(tmp, CHIP_REV);

	/* Learn number of HW-functions */
	tmp = qed_rd(p_hwfn, p_ptt, MISCS_REG_CMT_ENABLED_FOR_PAIR);

	if (tmp & BIT(p_hwfn->rel_pf_id)) {
		DP_NOTICE(cdev->hwfns, "device in CMT mode\n");
		cdev->num_hwfns = 2;
	} else {
		cdev->num_hwfns = 1;
	}

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(cdev) && QED_IS_BB(cdev)) {
		/* For some reason we have problems with this register
		 * in BB B0 emulation; Simply assume no CMT
		 */
		DP_NOTICE(cdev->hwfns, "device on emul - assume no CMT\n");
		cdev->num_hwfns = 1;
	}
#endif

	tmp = qed_rd(p_hwfn, p_ptt, MISCS_REG_CHIP_TEST_REG);
	cdev->chip_bond_id = (u8) GET_FIELD(tmp, CHIP_BOND_ID);
	tmp = qed_rd(p_hwfn, p_ptt, MISCS_REG_CHIP_METAL);
	cdev->chip_metal = (u8) GET_FIELD(tmp, CHIP_METAL);

	DP_INFO(cdev->hwfns,
		"Chip details - %s %c%d, Num: %04x Rev: %02x Bond id: %02x Metal: %02x\n",
		QED_IS_BB(cdev) ? "BB" : "AH",
		'A' + cdev->chip_rev,
		(int)cdev->chip_metal,
		cdev->chip_num,
		cdev->chip_rev, cdev->chip_bond_id, cdev->chip_metal);

	if (QED_IS_BB_A0(cdev)) {
		DP_NOTICE(cdev->hwfns,
			  "The chip type/rev (BB A0) is not supported!\n");
		return -EBUSY;
	}
#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(cdev) && QED_IS_AH(cdev))
		qed_wr(p_hwfn, p_ptt, MISCS_REG_PLL_MAIN_CTRL_4, 0x1);

	if (CHIP_REV_IS_EMUL(cdev)) {
		tmp = qed_rd(p_hwfn, p_ptt, MISCS_REG_ECO_RESERVED);

		/* MISCS_REG_ECO_RESERVED[29]: full/reduced emulation build */
		cdev->b_is_emul_full = ! !(tmp & BIT(29));

		/* MISCS_REG_ECO_RESERVED[28]: emulation build w/ or w/o MAC */
		cdev->b_is_emul_mac = ! !(tmp & BIT(28));

		DP_NOTICE(p_hwfn,
			  "Emulation: Running on a %s build %s MAC\n",
			  cdev->b_is_emul_full ? "full" : "reduced",
			  cdev->b_is_emul_mac ? "with" : "without");
	}
#endif

	return 0;
}

static int
qed_hw_prepare_single(struct qed_hwfn *p_hwfn,
		      void __iomem * p_regview,
		      void __iomem * p_doorbells,
		      u64 db_phys_addr,
		      unsigned long db_size,
		      struct qed_hw_prepare_params *p_params)
{
	struct qed_mdump_retain_data mdump_retain;
	struct qed_dev *cdev = p_hwfn->cdev;
	struct qed_mdump_info mdump_info;
	int rc = 0;

	/* Split PCI bars evenly between hwfns */
	p_hwfn->regview = p_regview;
	p_hwfn->doorbells = p_doorbells;
	p_hwfn->db_phys_addr = db_phys_addr;
	p_hwfn->db_size = db_size;

	p_hwfn->mcp_resc_lock_retry_cnt = (p_params->mcp_resc_lock_retry_cnt) ?
	    p_params->mcp_resc_lock_retry_cnt :
	    QED_MCP_RESC_LOCK_RETRY_CNT_DFLT;

	if (IS_VF(cdev))
		return qed_vf_hw_prepare(p_hwfn, p_params);

	/* Validate that chip access is feasible */
	if (REG_RD(p_hwfn, PXP_PF_ME_OPAQUE_ADDR) == 0xffffffff) {
		DP_ERR(p_hwfn,
		       "Reading the ME register returns all Fs; Preventing further chip access\n");
		if (p_params->b_relaxed_probe)
			p_params->p_relaxed_res = QED_HW_PREPARE_FAILED_ME;
		return -EINVAL;
	}

	get_function_id(p_hwfn);

	/* Allocate PTT pool */
	rc = qed_ptt_pool_alloc(p_hwfn);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed to prepare hwfn's hw\n");
		if (p_params->b_relaxed_probe)
			p_params->p_relaxed_res = QED_HW_PREPARE_FAILED_MEM;
		goto err0;
	}

	/* Allocate the main PTT */
	p_hwfn->p_main_ptt = qed_get_reserved_ptt(p_hwfn, RESERVED_PTT_MAIN);

	/* First hwfn learns basic information, e.g., number of hwfns */
	if (IS_LEAD_HWFN(p_hwfn)) {
		rc = qed_get_dev_info(p_hwfn, p_hwfn->p_main_ptt);
		if (rc) {
			if (p_params->b_relaxed_probe)
				p_params->p_relaxed_res =
				    QED_HW_PREPARE_FAILED_DEV;
			goto err1;
		}
	}
#ifndef ASIC_ONLY
	if (CHIP_REV_IS_SLOW(p_hwfn->cdev) && !b_ptt_gtt_init) {
		struct qed_ptt *p_ptt = p_hwfn->p_main_ptt;
		u32 val;

		/* Initialize PTT/GTT (done by MFW on ASIC) */
		qed_wr(p_hwfn, p_ptt, PGLUE_B_REG_START_INIT_PTT_GTT, 1);
		usleep_range(10000, 20000);
		qed_ptt_invalidate(p_hwfn);
		val = qed_rd(p_hwfn, p_ptt, PGLUE_B_REG_INIT_DONE_PTT_GTT);
		if (val != 1) {
			DP_ERR(p_hwfn,
			       "PTT and GTT init in PGLUE_B didn't complete\n");
			goto err1;
		}

		/* Clear a possible PGLUE_B parity from a previous GRC access */
		qed_wr(p_hwfn, p_ptt, PGLUE_B_REG_PRTY_STS_WR_H_0, 0x380);

		b_ptt_gtt_init = true;
	}
#endif

	/* Store the precompiled init data ptrs */
	if (IS_LEAD_HWFN(p_hwfn))
		qed_init_iro_array(p_hwfn->cdev);

	qed_hw_hwfn_prepare(p_hwfn);

	/* Initialize MCP structure */
	rc = qed_mcp_cmd_init(p_hwfn, p_hwfn->p_main_ptt);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed initializing mcp command\n");
		if (p_params->b_relaxed_probe)
			p_params->p_relaxed_res = QED_HW_PREPARE_FAILED_MEM;
		goto err1;
	}

	/* Read the device configuration information from the HW and SHMEM */
	rc = qed_get_hw_info(p_hwfn, p_hwfn->p_main_ptt,
			     p_params->personality, p_params);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed to get HW information\n");
		goto err2;
	}

	if (p_params->initiate_pf_flr && IS_LEAD_HWFN(p_hwfn)) {
		rc = qed_mcp_initiate_pf_flr(p_hwfn, p_hwfn->p_main_ptt);
		if (rc)
			DP_NOTICE(p_hwfn, "Failed to initiate PF FLR\n");
	}

	/* NVRAM info initialization and population */
	rc = qed_mcp_nvm_info_populate(p_hwfn);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed to populate nvm info shadow\n");
		goto err2;
	}

	/* Check if mdump logs/data are present and update the epoch value */
	if (IS_LEAD_HWFN(p_hwfn)) {
		rc = qed_mcp_mdump_get_info(p_hwfn, p_hwfn->p_main_ptt,
					    &mdump_info);
		if (rc == 0 && mdump_info.num_of_logs)
			DP_NOTICE(p_hwfn, "mdump data available.\n");

		rc = qed_mcp_mdump_get_retain(p_hwfn, p_hwfn->p_main_ptt,
					      &mdump_retain);
		if (rc == 0 && mdump_retain.valid)
			DP_NOTICE(p_hwfn,
				  "mdump retained data: epoch 0x%08x, pf 0x%x, status 0x%08x\n",
				  mdump_retain.epoch,
				  mdump_retain.pf, mdump_retain.status);

		qed_mcp_mdump_set_values(p_hwfn, p_hwfn->p_main_ptt,
					 p_params->epoch);
	}

	/* Allocate the init RT array and initialize the init-ops engine */
	rc = qed_init_alloc(p_hwfn);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed to allocate the init array\n");
		if (p_params->b_relaxed_probe)
			p_params->p_relaxed_res = QED_HW_PREPARE_FAILED_MEM;
		goto err3;
	}
#ifndef ASIC_ONLY
	if (CHIP_REV_IS_FPGA(cdev)) {
		if (QED_IS_AH(cdev)) {
			DP_NOTICE(p_hwfn,
				  "FPGA: workaround; Prevent DMAE parities\n");
			qed_wr(p_hwfn, p_hwfn->p_main_ptt,
			       PCIE_REG_PRTY_MASK_K2, 7);
		}

		DP_NOTICE(p_hwfn, "FPGA: workaround: Set VF bar0 size\n");
		qed_wr(p_hwfn, p_hwfn->p_main_ptt,
		       PGLUE_B_REG_VF_BAR0_SIZE_K2, 4);
	}
#endif

	return rc;
err3:
	qed_mcp_nvm_info_free(p_hwfn);
err2:
	if (IS_LEAD_HWFN(p_hwfn))
		qed_iov_free_hw_info(cdev);
	qed_mcp_free(p_hwfn);
err1:
	qed_hw_hwfn_free(p_hwfn);
err0:
	return rc;
}

int qed_hw_prepare(struct qed_dev *cdev, struct qed_hw_prepare_params *p_params)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	int rc;

	cdev->chk_reg_fifo = p_params->chk_reg_fifo;
	cdev->monitored_hw_addr = p_params->monitored_hw_addr;
	cdev->allow_mdump = p_params->allow_mdump;
	p_hwfn->b_en_pacing = p_params->b_en_pacing;
	cdev->b_is_target = p_params->b_is_target;
	p_hwfn->roce_edpm_mode = p_params->roce_edpm_mode;
	p_hwfn->num_vf_cnqs = p_params->num_vf_cnqs;
	p_hwfn->b_en_dcqcn = p_params->b_en_dcqcn;

	if (p_params->b_relaxed_probe)
		p_params->p_relaxed_res = QED_HW_PREPARE_SUCCESS;

	/* Initialize the first hwfn - will learn number of hwfns */
	rc = qed_hw_prepare_single(p_hwfn, cdev->regview,
				   cdev->doorbells, cdev->db_phys_addr,
				   cdev->db_size, p_params);
	if (rc)
		return rc;

	p_params->personality = p_hwfn->hw_info.personality;

	/* Initialize 2nd hwfn if necessary */
	if (QED_IS_CMT(cdev)) {
		void __iomem *p_regview, *p_doorbell;
		u8 __iomem *addr;
		u64 db_phys_addr;
		u32 offset;

		/* adjust bar offset for second engine */
		offset = qed_hw_bar_size(p_hwfn, p_hwfn->p_main_ptt,
					 BAR_ID_0) / 2;
		addr = (u8 __iomem *) cdev->regview + offset;
		p_regview = (void __iomem *)addr;

		offset = qed_hw_bar_size(p_hwfn, p_hwfn->p_main_ptt,
					 BAR_ID_1) / 2;
		addr = (u8 __iomem *) cdev->doorbells + offset;
		p_doorbell = (void __iomem *)addr;
		db_phys_addr = cdev->db_phys_addr + offset;

		cdev->hwfns[1].b_en_pacing = p_params->b_en_pacing;
		cdev->hwfns[1].num_vf_cnqs = p_params->num_vf_cnqs;
		/* prepare second hw function */
		rc = qed_hw_prepare_single(&cdev->hwfns[1], p_regview,
					   p_doorbell, db_phys_addr,
					   cdev->db_size, p_params);

		/* in case of error, need to free the previously
		 * initialized hwfn 0.
		 */
		if (rc) {
			if (p_params->b_relaxed_probe)
				p_params->p_relaxed_res =
				    QED_HW_PREPARE_FAILED_ENG2;

			if (IS_PF(cdev)) {
				qed_init_free(p_hwfn);
				qed_mcp_nvm_info_free(p_hwfn);
				qed_mcp_free(p_hwfn);
				qed_hw_hwfn_free(p_hwfn);
				qed_iov_free_hw_info(cdev);
			} else {
				DP_NOTICE(cdev,
					  "What do we need to free when VF hwfn1 init fails\n");
			}
			return rc;
		}
	}

	return rc;
}

void qed_hw_remove(struct qed_dev *cdev)
{
	int i;

	if (IS_PF(cdev)) {
		struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
		qed_mcp_ov_update_driver_state(p_hwfn, p_hwfn->p_main_ptt,
					       QED_OV_DRIVER_STATE_NOT_LOADED);
	}

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];

		if (IS_VF(cdev)) {
			qed_vf_pf_release(p_hwfn);
			continue;
		}

		qed_init_free(p_hwfn);
		qed_mcp_nvm_info_free(p_hwfn);
		qed_hw_hwfn_free(p_hwfn);
		qed_mcp_free(p_hwfn);
	}

	qed_iov_free_hw_info(cdev);
}

static void qed_chain_free_next_ptr(struct qed_dev *cdev,
				    struct qed_chain *p_chain)
{
	void *p_virt = p_chain->p_virt_addr, *p_virt_next = NULL;
	dma_addr_t p_phys = p_chain->p_phys_addr, p_phys_next = 0;
	struct qed_chain_next *p_next;
	u32 size, i;

	if (!p_virt)
		return;

	size = p_chain->elem_size * p_chain->usable_per_page;

	for (i = 0; i < p_chain->page_cnt; i++) {
		if (!p_virt)
			break;

		p_next = (struct qed_chain_next *)((u8 *) p_virt + size);
		p_virt_next = p_next->next_virt;
		p_phys_next = HILO_DMA_REGPAIR(p_next->next_phys);

		dma_free_coherent(&cdev->pdev->dev,
				  p_chain->page_size, p_virt, p_phys);

		p_virt = p_virt_next;
		p_phys = p_phys_next;
	}
}

static void qed_chain_free_single(struct qed_dev *cdev,
				  struct qed_chain *p_chain)
{
	if (!p_chain->p_virt_addr)
		return;

	dma_free_coherent(&cdev->pdev->dev,
			  p_chain->page_size,
			  p_chain->p_virt_addr, p_chain->p_phys_addr);
}

static void qed_chain_free_pbl(struct qed_dev *cdev, struct qed_chain *p_chain)
{
	void **pp_virt_addr_tbl = p_chain->pbl.pp_virt_addr_tbl;
	u8 *p_pbl_virt = (u8 *) p_chain->pbl_sp.p_virt_table;
	u32 page_cnt = p_chain->page_cnt, i, pbl_size;

	if (!pp_virt_addr_tbl)
		return;

	if (!p_pbl_virt)
		goto out;

	for (i = 0; i < page_cnt; i++) {
		if (!pp_virt_addr_tbl[i])
			break;

		dma_free_coherent(&cdev->pdev->dev,
				  p_chain->page_size,
				  pp_virt_addr_tbl[i],
				  *(dma_addr_t *) p_pbl_virt);

		p_pbl_virt += QED_CHAIN_PBL_ENTRY_SIZE;
	}

	pbl_size = page_cnt * QED_CHAIN_PBL_ENTRY_SIZE;

	if (!p_chain->b_external_pbl)
		dma_free_coherent(&cdev->pdev->dev,
				  pbl_size,
				  p_chain->pbl_sp.p_virt_table,
				  p_chain->pbl_sp.p_phys_table);
out:
	vfree(p_chain->pbl.pp_virt_addr_tbl);
	p_chain->pbl.pp_virt_addr_tbl = NULL;
}

void qed_chain_free(struct qed_dev *cdev, struct qed_chain *p_chain)
{
	switch (p_chain->mode) {
	case QED_CHAIN_MODE_NEXT_PTR:
		qed_chain_free_next_ptr(cdev, p_chain);
		break;
	case QED_CHAIN_MODE_SINGLE:
		qed_chain_free_single(cdev, p_chain);
		break;
	case QED_CHAIN_MODE_PBL:
		qed_chain_free_pbl(cdev, p_chain);
		break;
	}

	/* reset chain addresses to avoid double free */
	qed_chain_init_mem(p_chain, NULL, 0);
}

static int
qed_chain_alloc_sanity_check(struct qed_dev *cdev,
			     enum qed_chain_cnt_type cnt_type,
			     size_t elem_size, u32 page_size, u32 page_cnt)
{
	u64 chain_size = ELEMS_PER_PAGE(page_size, elem_size) * page_cnt;

	/* The actual chain size can be larger than the maximal possible value
	 * after rounding up the requested elements number to pages, and after
	 * taking into acount the unusuable elements (next-ptr elements).
	 * The size of a "u16" chain can be (U16_MAX + 1) since the chain
	 * size/capacity fields are of a u32 type.
	 */
	if ((cnt_type == QED_CHAIN_CNT_TYPE_U16 &&
	     chain_size > ((u32) QED_U16_MAX + 1)) ||
	    (cnt_type == QED_CHAIN_CNT_TYPE_U32 && chain_size > QED_U32_MAX)) {
		DP_NOTICE(cdev,
			  "The actual chain size (0x%llx) is larger than the maximal possible value\n",
			  chain_size);
		return -EINVAL;
	}

	return 0;
}

static int
qed_chain_alloc_next_ptr(struct qed_dev *cdev, struct qed_chain *p_chain)
{
	void *p_virt = NULL, *p_virt_prev = NULL;
	dma_addr_t p_phys = 0;
	u32 i;

	if (!p_chain->page_cnt) {
		DP_NOTICE(cdev, "Invalid page count\n");
		return -EINVAL;
	}

	for (i = 0; i < p_chain->page_cnt; i++) {
		p_virt = dma_alloc_coherent(&cdev->pdev->dev,
					    p_chain->page_size,
					    &p_phys, GFP_KERNEL);
		if (!p_virt) {
			DP_NOTICE(cdev, "Failed to allocate chain memory\n");
			return -ENOMEM;
		}

		if (i == 0) {
			qed_chain_init_mem(p_chain, p_virt, p_phys);
			qed_chain_reset(p_chain);
		} else {
			qed_chain_init_next_ptr_elem(p_chain, p_virt_prev,
						     p_virt, p_phys);
		}

		p_virt_prev = p_virt;
	}
	/* Last page's next element should point to the beginning of the
	 * chain.
	 */
	qed_chain_init_next_ptr_elem(p_chain, p_virt_prev,
				     p_chain->p_virt_addr,
				     p_chain->p_phys_addr);

	return 0;
}

static int
qed_chain_alloc_single(struct qed_dev *cdev, struct qed_chain *p_chain)
{
	dma_addr_t p_phys = 0;
	void *p_virt = NULL;

	p_virt = dma_alloc_coherent(&cdev->pdev->dev,
				    p_chain->page_size, &p_phys, GFP_KERNEL);
	if (!p_virt) {
		DP_NOTICE(cdev, "Failed to allocate chain memory\n");
		return -ENOMEM;
	}

	qed_chain_init_mem(p_chain, p_virt, p_phys);
	qed_chain_reset(p_chain);

	return 0;
}

static int
qed_chain_alloc_pbl(struct qed_dev *cdev,
		    struct qed_chain *p_chain,
		    struct qed_chain_ext_pbl *ext_pbl)
{
	u32 page_cnt = p_chain->page_cnt, size, i;
	dma_addr_t p_phys = 0, p_pbl_phys = 0;
	void **pp_virt_addr_tbl = NULL;
	u8 *p_pbl_virt = NULL;
	void *p_virt = NULL;

	size = page_cnt * sizeof(*pp_virt_addr_tbl);
	pp_virt_addr_tbl = (void **)vzalloc(size);
	if (!pp_virt_addr_tbl) {
		DP_NOTICE(cdev,
			  "Failed to allocate memory for the chain virtual addresses table\n");
		return -ENOMEM;
	}

	/* The allocation of the PBL table is done with its full size, since it
	 * is expected to be successive.
	 * qed_chain_init_pbl_mem() is called even in a case of an allocation
	 * failure, since pp_virt_addr_tbl was previously allocated, and it
	 * should be saved to allow its freeing during the error flow.
	 */
	size = page_cnt * QED_CHAIN_PBL_ENTRY_SIZE;

	if (ext_pbl == NULL) {
		p_pbl_virt = dma_alloc_coherent(&cdev->pdev->dev,
						size, &p_pbl_phys, GFP_KERNEL);
	} else {
		p_pbl_virt = ext_pbl->p_pbl_virt;
		p_pbl_phys = ext_pbl->p_pbl_phys;
		p_chain->b_external_pbl = true;
	}

	qed_chain_init_pbl_mem(p_chain, p_pbl_virt, p_pbl_phys,
			       pp_virt_addr_tbl);
	if (!p_pbl_virt) {
		DP_NOTICE(cdev, "Failed to allocate chain pbl memory\n");
		return -ENOMEM;
	}

	for (i = 0; i < page_cnt; i++) {
		p_virt = dma_alloc_coherent(&cdev->pdev->dev,
					    p_chain->page_size,
					    &p_phys, GFP_KERNEL);
		if (!p_virt) {
			DP_NOTICE(cdev, "Failed to allocate chain memory\n");
			return -ENOMEM;
		}

		if (i == 0) {
			qed_chain_init_mem(p_chain, p_virt, p_phys);
			qed_chain_reset(p_chain);
		}

		/* Fill the PBL table with the physical address of the page */
		*(dma_addr_t *) p_pbl_virt = p_phys;
		/* Keep the virtual address of the page */
		p_chain->pbl.pp_virt_addr_tbl[i] = p_virt;

		p_pbl_virt += QED_CHAIN_PBL_ENTRY_SIZE;
	}

	return 0;
}

void qed_chain_params_init(struct qed_chain_params *p_params,
			   enum qed_chain_use_mode intended_use,
			   enum qed_chain_mode mode,
			   enum qed_chain_cnt_type cnt_type,
			   u32 num_elems, size_t elem_size)
{
	p_params->intended_use = intended_use;
	p_params->mode = mode;
	p_params->cnt_type = cnt_type;
	p_params->num_elems = num_elems;
	p_params->elem_size = elem_size;

	/* common values */
	p_params->page_size = QED_CHAIN_PAGE_SIZE;
	p_params->ext_pbl = NULL;
}

int qed_chain_alloc(struct qed_dev *cdev,
		    struct qed_chain *p_chain,
		    struct qed_chain_params *p_params)
{
	u32 page_cnt;
	int rc = 0;

	if (p_params->mode == QED_CHAIN_MODE_SINGLE) {
		page_cnt = 1;
	} else {
		page_cnt = QED_CHAIN_PAGE_CNT(p_params->num_elems,
					      p_params->elem_size,
					      p_params->page_size,
					      p_params->mode);
	}

	rc = qed_chain_alloc_sanity_check(cdev, p_params->cnt_type,
					  p_params->elem_size,
					  p_params->page_size, page_cnt);
	if (rc) {
		DP_NOTICE(cdev,
			  "Cannot allocate a chain with the given arguments:\n"
			  "[use_mode %d, mode %d, cnt_type %d, num_elems %d, elem_size %zu, page_size %u]\n",
			  p_params->intended_use,
			  p_params->mode,
			  p_params->cnt_type,
			  p_params->num_elems,
			  p_params->elem_size, p_params->page_size);
		return rc;
	}

	qed_chain_init(p_chain, page_cnt, (u8) p_params->elem_size,
		       p_params->page_size, p_params->intended_use,
		       p_params->mode, p_params->cnt_type, cdev->dp_ctx);

	switch (p_params->mode) {
	case QED_CHAIN_MODE_NEXT_PTR:
		rc = qed_chain_alloc_next_ptr(cdev, p_chain);
		break;
	case QED_CHAIN_MODE_SINGLE:
		rc = qed_chain_alloc_single(cdev, p_chain);
		break;
	case QED_CHAIN_MODE_PBL:
		rc = qed_chain_alloc_pbl(cdev, p_chain, p_params->ext_pbl);
		break;
	}
	if (rc)
		goto nomem;

	return 0;

nomem:
	qed_chain_free(cdev, p_chain);
	return rc;
}

int qed_fw_l2_queue(struct qed_hwfn *p_hwfn, u16 src_id, u16 * dst_id)
{
	if (!RESC_NUM(p_hwfn, QED_L2_QUEUE)) {
		DP_NOTICE(p_hwfn, "No L2 queue is available\n");
		return -EINVAL;
	}

	if (src_id >= RESC_NUM(p_hwfn, QED_L2_QUEUE)) {
		u16 min, max;

		min = (u16) RESC_START(p_hwfn, QED_L2_QUEUE);
		max = min + RESC_NUM(p_hwfn, QED_L2_QUEUE) - 1;
		DP_NOTICE(p_hwfn,
			  "l2_queue id [%d] is not valid, available indices [%d - %d]\n",
			  src_id, min, max);

		return -EINVAL;
	}

	*dst_id = RESC_START(p_hwfn, QED_L2_QUEUE) + src_id;

	return 0;
}

int qed_fw_vport(struct qed_hwfn *p_hwfn, u8 src_id, u8 * dst_id)
{
	if (!RESC_NUM(p_hwfn, QED_VPORT)) {
		DP_NOTICE(p_hwfn, "No vport is available\n");
		return -EINVAL;
	}

	if (src_id >= RESC_NUM(p_hwfn, QED_VPORT)) {
		u8 min, max;

		min = (u8) RESC_START(p_hwfn, QED_VPORT);
		max = min + RESC_NUM(p_hwfn, QED_VPORT) - 1;
		DP_NOTICE(p_hwfn,
			  "vport id [%d] is not valid, available indices [%d - %d]\n",
			  src_id, min, max);

		return -EINVAL;
	}

	*dst_id = RESC_START(p_hwfn, QED_VPORT) + src_id;

	return 0;
}

int qed_fw_rss_eng(struct qed_hwfn *p_hwfn, u8 src_id, u8 * dst_id)
{
	if (!RESC_NUM(p_hwfn, QED_RSS_ENG)) {
		DP_NOTICE(p_hwfn, "No RSS engine is available\n");
		return -EINVAL;
	}

	if (src_id >= RESC_NUM(p_hwfn, QED_RSS_ENG)) {
		u8 min, max;

		min = (u8) RESC_START(p_hwfn, QED_RSS_ENG);
		max = min + RESC_NUM(p_hwfn, QED_RSS_ENG) - 1;
		DP_NOTICE(p_hwfn,
			  "rss_eng id [%d] is not valid, available indices [%d - %d]\n",
			  src_id, min, max);

		return -EINVAL;
	}

	*dst_id = RESC_START(p_hwfn, QED_RSS_ENG) + src_id;

	return 0;
}

static int qed_set_coalesce(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt,
			    u32 hw_addr,
			    void *p_eth_qzone,
			    size_t eth_qzone_size, u8 timeset)
{
	struct coalescing_timeset *p_coal_timeset;

	if (p_hwfn->cdev->int_coalescing_mode != QED_COAL_MODE_ENABLE) {
		DP_NOTICE(p_hwfn, "Coalescing configuration not enabled\n");
		return -EINVAL;
	}

	p_coal_timeset = p_eth_qzone;
	memset(p_eth_qzone, 0, eth_qzone_size);
	SET_FIELD(p_coal_timeset->value, COALESCING_TIMESET_TIMESET, timeset);
	SET_FIELD(p_coal_timeset->value, COALESCING_TIMESET_VALID, 1);
	qed_memcpy_to(p_hwfn, p_ptt, hw_addr, p_eth_qzone, eth_qzone_size);

	return 0;
}

int qed_set_queue_coalesce(struct qed_hwfn *p_hwfn,
			   u16 rx_coal, u16 tx_coal, void *p_handle)
{
	struct qed_queue_cid *p_cid = (struct qed_queue_cid *)p_handle;
	int rc = 0;
	struct qed_ptt *p_ptt;

	/* TODO - Configuring a single queue's coalescing but
	 * claiming all queues are abiding same configuration
	 * for PF and VF both.
	 */

	if (IS_VF(p_hwfn->cdev))
		return qed_vf_pf_set_coalesce(p_hwfn, rx_coal, tx_coal, p_cid);

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return -EAGAIN;

	if (rx_coal) {
		rc = qed_set_rxq_coalesce(p_hwfn, p_ptt, rx_coal, p_cid);
		if (rc)
			goto out;
		p_hwfn->cdev->rx_coalesce_usecs = rx_coal;
	}

	if (tx_coal) {
		rc = qed_set_txq_coalesce(p_hwfn, p_ptt, tx_coal, p_cid);
		if (rc)
			goto out;
		p_hwfn->cdev->tx_coalesce_usecs = tx_coal;
	}
out:
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

int qed_set_rxq_coalesce(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 u16 coalesce, struct qed_queue_cid *p_cid)
{
	struct ustorm_eth_queue_zone eth_qzone;
	u8 timeset, timer_res;
	u32 address;
	int rc;

	if (!p_cid)
		return -EINVAL;

	/* Coalesce = (timeset << timer-resolution), timeset is 7bit wide */
	if (coalesce <= 0x7F) {
		timer_res = 0;
	} else if (coalesce <= 0xFF) {
		timer_res = 1;
	} else if (coalesce <= 0x1FF) {
		timer_res = 2;
	} else {
		DP_ERR(p_hwfn, "Invalid coalesce value - %d\n", coalesce);
		return -EINVAL;
	}
	timeset = (u8) (coalesce >> timer_res);

	rc = qed_int_set_timer_res(p_hwfn, p_ptt, timer_res,
				   p_cid->sb_igu_id, false);
	if (rc)
		goto out;

	address = BAR0_MAP_REG_USDM_RAM +
	    USTORM_ETH_QUEUE_ZONE_GTT_OFFSET(p_cid->abs.queue_id);

	rc = qed_set_coalesce(p_hwfn, p_ptt, address, &eth_qzone,
			      sizeof(struct ustorm_eth_queue_zone), timeset);
	if (rc)
		goto out;

out:
	return rc;
}

int qed_set_txq_coalesce(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 u16 coalesce, struct qed_queue_cid *p_cid)
{
	struct xstorm_eth_queue_zone eth_qzone;
	u8 timeset, timer_res;
	u32 address;
	int rc;

	/* Coalesce = (timeset << timer-resolution), timeset is 7bit wide */
	if (coalesce <= 0x7F) {
		timer_res = 0;
	} else if (coalesce <= 0xFF) {
		timer_res = 1;
	} else if (coalesce <= 0x1FF) {
		timer_res = 2;
	} else {
		DP_ERR(p_hwfn, "Invalid coalesce value - %d\n", coalesce);
		return -EINVAL;
	}
	timeset = (u8) (coalesce >> timer_res);

	rc = qed_int_set_timer_res(p_hwfn, p_ptt, timer_res,
				   p_cid->sb_igu_id, true);
	if (rc)
		goto out;

	address = BAR0_MAP_REG_XSDM_RAM +
	    XSTORM_ETH_QUEUE_ZONE_GTT_OFFSET(p_cid->abs.queue_id);

	rc = qed_set_coalesce(p_hwfn, p_ptt, address, &eth_qzone,
			      sizeof(struct xstorm_eth_queue_zone), timeset);
out:
	return rc;
}

/* Calculate final WFQ values for all vports and configure it.
 * After this configuration each vport must have
 * approx min rate =  vport_wfq * min_pf_rate / QED_WFQ_UNIT
 */
static void qed_configure_wfq_for_all_vports(struct qed_hwfn *p_hwfn,
					     struct qed_ptt *p_ptt,
					     u32 min_pf_rate)
{
	struct init_qm_vport_params *vport_params;
	int i;

	vport_params = p_hwfn->qm_info.qm_vport_params;

	for (i = 0; i < p_hwfn->qm_info.num_vports; i++) {
		u32 wfq_speed = p_hwfn->qm_info.wfq_data[i].min_speed;

		vport_params[i].wfq = (wfq_speed * QED_WFQ_UNIT) / min_pf_rate;
		if (vport_params[i].wfq == 0)
			vport_params[i].wfq = 1;
		qed_init_vport_wfq(p_hwfn, p_ptt,
				   vport_params[i].first_tx_pq_id,
				   vport_params[i].wfq);
	}
}

static void qed_init_wfq_default_param(struct qed_hwfn *p_hwfn)
{
	int i;

	for (i = 0; i < p_hwfn->qm_info.num_vports; i++)
		p_hwfn->qm_info.qm_vport_params[i].wfq = 1;
}

static void qed_disable_wfq_for_all_vports(struct qed_hwfn *p_hwfn,
					   struct qed_ptt *p_ptt)
{
	struct init_qm_vport_params *vport_params;
	int i;

	vport_params = p_hwfn->qm_info.qm_vport_params;

	for (i = 0; i < p_hwfn->qm_info.num_vports; i++) {
		qed_init_wfq_default_param(p_hwfn);
		qed_init_vport_wfq(p_hwfn, p_ptt,
				   vport_params[i].first_tx_pq_id,
				   vport_params[i].wfq);
	}
}

/* This function performs several validations for WFQ
 * configuration and required min rate for a given vport
 * 1. req_rate must be greater than one percent of min_pf_rate.
 * 2. req_rate should not cause other vports [not configured for WFQ explicitly]
 *    rates to get less than one percent of min_pf_rate.
 * 3. total_req_min_rate [all vports min rate sum] shouldn't exceed min_pf_rate.
 */
static int qed_init_wfq_param(struct qed_hwfn *p_hwfn,
			      u16 vport_id, u32 req_rate, u32 min_pf_rate)
{
	u32 total_req_min_rate = 0, total_left_rate = 0, left_rate_per_vp = 0;
	int non_requested_count = 0, req_count = 0, i, num_vports;
	u16 num_default_vports = p_hwfn->qm_info.num_vports;
	u32 pq_flags = qed_get_pq_flags(p_hwfn);
	u32 percentages;

	/* we don't want to count PQ SET vports which are not configured because
	 * they aren't associated with any pq and don't need any reservation.
	 */
	if (pq_flags & PQ_FLAGS_PQSET) {
		u16 num_pq_set_vports;

		num_pq_set_vports = qed_init_qm_get_num_pqset_vports(p_hwfn);
		num_default_vports -= num_pq_set_vports;
	}

	num_vports = p_hwfn->qm_info.num_vports;

	/* Accounting for the vports which are configured for WFQ explicitly */
	for (i = 0; i < num_vports; i++) {
		u32 tmp_speed;

		if ((i != vport_id) && p_hwfn->qm_info.wfq_data[i].configured) {
			req_count++;
			tmp_speed = p_hwfn->qm_info.wfq_data[i].min_speed;
			total_req_min_rate += tmp_speed;
		} else if (i != vport_id && i < num_default_vports) {
			non_requested_count++;
		}
	}

	/* Include current vport data as well */
	req_count++;
	total_req_min_rate += req_rate;

	/* validate error case - req_rate less than 1% of min_pf_rate */
	if (min_pf_rate != 0 && req_rate != 0) {
		percentages = req_rate * 100 / min_pf_rate;
		if (QED_WEIGHT_IN_WFQ_GRANULARITY(percentages) == 0) {
			DP_NOTICE(p_hwfn,
				  "Vport [%d] - Requested rate[%d Mbps] is less WFQ unit (1:%d) of configured PF min rate [%d Mbps]\n",
				  vport_id,
				  req_rate, QED_WFQ_UNIT, min_pf_rate);

			return -EINVAL;
		}
	}

	if (total_req_min_rate > min_pf_rate) {
		DP_NOTICE(p_hwfn,
			  "Total requested min rate for all vports[%d Mbps] is greater than configured PF min rate[%d Mbps]\n",
			  total_req_min_rate, min_pf_rate);

		return -EINVAL;
	}

	/* bw left for non requested vports */
	total_left_rate = min_pf_rate - total_req_min_rate;

	/* validate if non requested get < 1% of min bw */
	if (non_requested_count && min_pf_rate != 0) {
		left_rate_per_vp = total_left_rate / non_requested_count;
		percentages = left_rate_per_vp * 100 / min_pf_rate;
		if (QED_WEIGHT_IN_WFQ_GRANULARITY(percentages) == 0) {
			DP_NOTICE(p_hwfn,
				  "Non WFQ configured vports rate [%d Mbps] is less than WFQ unit (1:%d) of configured PF min rate[%d Mbps]\n",
				  left_rate_per_vp, QED_WFQ_UNIT, min_pf_rate);

			return -EINVAL;
		}
	}

	/* now req_rate for given vport passes all scenarios.
	 * assign final wfq rates to all vports.
	 */
	p_hwfn->qm_info.wfq_data[vport_id].min_speed = req_rate;
	p_hwfn->qm_info.wfq_data[vport_id].configured = true;

	for (i = 0; i < num_vports; i++) {
		struct qed_wfq_data *wfq_data = &p_hwfn->qm_info.wfq_data[i];

		if (wfq_data->configured)
			continue;

		if (i < num_default_vports)
			wfq_data->min_speed = left_rate_per_vp;
		else
			wfq_data->min_speed = 0;
	}

	return 0;
}

static int __qed_configure_vport_wfq(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     u16 vp_id, u32 rate, u32 min_pf_rate)
{
	int rc = 0;

	if (!min_pf_rate) {
		p_hwfn->qm_info.wfq_data[vp_id].min_speed = rate;
		p_hwfn->qm_info.wfq_data[vp_id].configured = true;
		return rc;
	}

	rc = qed_init_wfq_param(p_hwfn, vp_id, rate, min_pf_rate);

	if (!rc)
		qed_configure_wfq_for_all_vports(p_hwfn, p_ptt, min_pf_rate);
	else
		DP_NOTICE(p_hwfn,
			  "Validation failed while configuring min rate\n");

	return rc;
}

static int __qed_configure_vp_wfq_on_link_change(struct qed_hwfn *p_hwfn,
						 struct qed_ptt *p_ptt,
						 u32 min_pf_rate)
{
	bool use_wfq = false;
	int rc = 0;
	u16 i;

	/* Validate all pre configured vports for wfq */
	for (i = 0; i < p_hwfn->qm_info.num_vports; i++) {
		u32 rate;

		if (!p_hwfn->qm_info.wfq_data[i].configured)
			continue;

		rate = p_hwfn->qm_info.wfq_data[i].min_speed;
		use_wfq = true;

		rc = qed_init_wfq_param(p_hwfn, i, rate, min_pf_rate);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "WFQ validation failed while configuring min rate\n");
			break;
		}
	}

	if (rc == 0 && use_wfq)
		qed_configure_wfq_for_all_vports(p_hwfn, p_ptt, min_pf_rate);
	else
		qed_disable_wfq_for_all_vports(p_hwfn, p_ptt);

	return rc;
}

/* Main API for qed clients to configure vport min rate.
 * vp_id - vport id in PF Range[0 - (total_num_vports_per_pf - 1)]
 * rate - Speed in Mbps needs to be assigned to a given vport.
 */
int qed_configure_vport_wfq(struct qed_dev *cdev, u16 vp_id, u32 rate)
{
	struct qed_mcp_link_state *p_link;
	int i, rc = -EINVAL;

	p_link = &QED_LEADING_HWFN(cdev)->mcp_info->link_output;

	/* TBD - for multiple hardware functions - that is 100 gig */
	if (QED_IS_CMT(cdev)) {
		DP_NOTICE(cdev,
			  "WFQ configuration is not supported for this device\n");
		return rc;
	}

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];
		struct qed_ptt *p_ptt;

		p_ptt = qed_ptt_acquire(p_hwfn);
		if (!p_ptt)
			return -EBUSY;

		rc = __qed_configure_vport_wfq(p_hwfn, p_ptt, vp_id, rate,
					       p_link->min_pf_rate);

		if (rc) {
			qed_ptt_release(p_hwfn, p_ptt);
			return rc;
		}

		qed_ptt_release(p_hwfn, p_ptt);
	}

	return rc;
}

/* API to configure WFQ from mcp link change */
void qed_configure_vp_wfq_on_link_change(struct qed_dev *cdev,
					 struct qed_ptt *p_ptt, u32 min_pf_rate)
{
	int i;

	/* TBD - for multiple hardware functions - that is 100 gig */
	if (QED_IS_CMT(cdev)) {
		DP_VERBOSE(cdev,
			   NETIF_MSG_LINK,
			   "WFQ configuration is not supported for this device\n");
		return;
	}

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *lead_p_hwfn = QED_LEADING_HWFN(cdev);
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];

		if (p_hwfn->qm_info.relative_wfq_speed)
			min_pf_rate = lead_p_hwfn->mcp_info->link_output.speed;
		if (min_pf_rate == 0)
			break;
		__qed_configure_vp_wfq_on_link_change(p_hwfn, p_ptt,
						      min_pf_rate);
	}
}

int __qed_configure_pf_max_bandwidth(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     struct qed_mcp_link_state *p_link,
				     u8 max_bw)
{
	int rc = 0;

	p_hwfn->mcp_info->func_info.bandwidth_max = max_bw;

	if (!p_link->line_speed && (max_bw != 100))
		return rc;

	p_link->speed = (p_link->line_speed * max_bw) / 100;
	p_hwfn->qm_info.pf_rl = p_link->speed;

	/* Since the limiter also affects Tx-switched traffic, we don't want it
	 * to limit such traffic in case there's no actual limit.
	 * In that case, set limit to imaginary high boundary.
	 */
	if (max_bw == 100)
		p_hwfn->qm_info.pf_rl = 100000;

	rc = qed_init_pf_rl(p_hwfn, p_ptt, p_hwfn->rel_pf_id,
			    p_hwfn->qm_info.pf_rl);

	DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
		   "Configured MAX bandwidth to be %08x Mb/sec\n",
		   p_link->speed);

	return rc;
}

/* Main API to configure PF max bandwidth where bw range is [1 - 100] */
int qed_configure_pf_max_bandwidth(struct qed_dev *cdev, u8 max_bw)
{
	int i, rc = -EINVAL;

	if (max_bw < 1 || max_bw > 100) {
		DP_NOTICE(cdev, "PF max bw valid range is [1-100]\n");
		return rc;
	}

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];
		struct qed_hwfn *p_lead = QED_LEADING_HWFN(cdev);
		struct qed_mcp_link_state *p_link;
		struct qed_ptt *p_ptt;

		p_link = &p_lead->mcp_info->link_output;

		p_ptt = qed_ptt_acquire(p_hwfn);
		if (!p_ptt)
			return -EBUSY;

		rc = __qed_configure_pf_max_bandwidth(p_hwfn, p_ptt,
						      p_link, max_bw);

		qed_ptt_release(p_hwfn, p_ptt);

		if (rc)
			break;
	}

	return rc;
}

int __qed_configure_pf_min_bandwidth(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     struct qed_mcp_link_state *p_link,
				     u8 min_bw)
{
	int rc = 0;

	p_hwfn->mcp_info->func_info.bandwidth_min = min_bw;
	p_hwfn->qm_info.pf_wfq = min_bw;

	if (!p_link->line_speed)
		return rc;

	p_link->min_pf_rate = (p_link->line_speed * min_bw) / 100;

	rc = qed_init_pf_wfq(p_hwfn, p_ptt, p_hwfn->rel_pf_id, min_bw);

	DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
		   "Configured MIN bandwidth to be %d Mb/sec\n",
		   p_link->min_pf_rate);

	return rc;
}

/* Main API to configure PF min bandwidth where bw range is [1-100] */
int qed_configure_pf_min_bandwidth(struct qed_dev *cdev, u8 min_bw)
{
	int i, rc = -EINVAL;

	if (min_bw < 1 || min_bw > 100) {
		DP_NOTICE(cdev, "PF min bw valid range is [1-100]\n");
		return rc;
	}

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];
		struct qed_hwfn *p_lead = QED_LEADING_HWFN(cdev);
		struct qed_mcp_link_state *p_link;
		struct qed_ptt *p_ptt;

		p_link = &p_lead->mcp_info->link_output;

		p_ptt = qed_ptt_acquire(p_hwfn);
		if (!p_ptt)
			return -EBUSY;

		rc = __qed_configure_pf_min_bandwidth(p_hwfn, p_ptt,
						      p_link, min_bw);
		if (rc) {
			qed_ptt_release(p_hwfn, p_ptt);
			return rc;
		}

		if (p_link->min_pf_rate) {
			u32 min_rate = p_link->min_pf_rate;

			rc = __qed_configure_vp_wfq_on_link_change(p_hwfn,
								   p_ptt,
								   min_rate);
		}

		qed_ptt_release(p_hwfn, p_ptt);
	}

	return rc;
}

void qed_clean_wfq_db(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mcp_link_state *p_link;

	p_link = &p_hwfn->mcp_info->link_output;

	if (p_link->min_pf_rate)
		qed_disable_wfq_for_all_vports(p_hwfn, p_ptt);

	memset(p_hwfn->qm_info.wfq_data, 0,
	       sizeof(*p_hwfn->qm_info.wfq_data) * p_hwfn->qm_info.num_vports);
}

int qed_device_num_engines(struct qed_dev *cdev)
{
	return QED_IS_BB(cdev) ? 2 : 1;
}

int qed_device_num_ports(struct qed_dev *cdev)
{
	return cdev->num_ports;
}

void qed_set_fw_mac_addr(__le16 * fw_msb,
			 __le16 * fw_mid, __le16 * fw_lsb, u8 * mac)
{
	((u8 *) fw_msb)[0] = mac[1];
	((u8 *) fw_msb)[1] = mac[0];
	((u8 *) fw_mid)[0] = mac[3];
	((u8 *) fw_mid)[1] = mac[2];
	((u8 *) fw_lsb)[0] = mac[5];
	((u8 *) fw_lsb)[1] = mac[4];
}

void qed_set_dev_access_enable(struct qed_dev *cdev, bool b_enable)
{
	if (cdev->recov_in_prog != !b_enable) {
		DP_INFO(cdev, "%s access to the device\n",
			b_enable ? "Enable" : "Disable");
		cdev->recov_in_prog = !b_enable;
	}
}

void qed_set_platform_str(struct qed_hwfn *p_hwfn, char *buf_str, u32 buf_size)
{
	u32 len;

	scnprintf(buf_str, buf_size, "QED %d.%d.%d.%d. ",
		  QED_MAJOR_VERSION, QED_MINOR_VERSION,
		  QED_REVISION_VERSION, QED_ENGINEERING_VERSION);

	len = strlen(buf_str);
	qed_set_platform_str_linux(p_hwfn, &buf_str[len], buf_size - len);
}

bool qed_is_mf_fip_special(struct qed_dev *cdev)
{
	return ! !test_bit(QED_MF_FIP_SPECIAL, &cdev->mf_bits);
}

bool qed_is_dscp_to_tc_capable(struct qed_dev * cdev)
{
	return ! !test_bit(QED_MF_DSCP_TO_TC_MAP, &cdev->mf_bits);
}

u8 qed_get_num_funcs_on_engine(struct qed_hwfn * p_hwfn)
{
	return p_hwfn->num_funcs_on_engine;
}

struct qed_qm_pqset_params {
	u16 abs_pqset_base_id;
	u16 rel_pqset_base_id;
	u16 abs_qm_vport;
	u16 rel_qm_vport;
	u16 abs_rl_id;
	u16 rel_rl_id;
};

static int
qed_qm_pqset_get_params(struct qed_hwfn *p_hwfn,
			u16 pq_set_id,
			u8 tc, struct qed_qm_pqset_params *pqset_params)
{
	u16 abs_pq_id, abs_vp_id, abs_rl_id, rel_pq_id, rel_vp_id, rel_rl_id;
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 max_tc = qed_init_qm_get_num_tcs(p_hwfn);

	if (tc >= max_tc ||
	    pq_set_id >= (qm_info->num_pqset + qm_info->start_pqset_num)) {
		DP_NOTICE(p_hwfn->cdev,
			  "Invalid PQ set parameters: tc %d, pq_set_id %d\n",
			  tc, pq_set_id);

		return -EINVAL;
	}

	abs_pq_id = qed_get_pqset_base(p_hwfn, pq_set_id) + tc;
	rel_pq_id = abs_pq_id - qm_info->start_pq;

	abs_vp_id = qm_info->qm_pq_params[rel_pq_id].vport_id;
	rel_vp_id = abs_vp_id - qm_info->start_vport;

	abs_rl_id = qm_info->qm_pq_params[rel_pq_id].rl_id;
	rel_rl_id = abs_rl_id - qm_info->start_rl;

	pqset_params->abs_pqset_base_id = abs_pq_id;
	pqset_params->rel_pqset_base_id = rel_pq_id;
	pqset_params->abs_qm_vport = abs_vp_id;
	pqset_params->rel_qm_vport = rel_vp_id;
	pqset_params->abs_rl_id = abs_rl_id;
	pqset_params->rel_rl_id = rel_rl_id;

	return 0;
}

int
qed_qm_update_rt_wfq_of_pqset(struct qed_hwfn *p_hwfn,
			      u16 pq_set_id, u8 tc, u32 min_bw)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	struct qed_qm_pqset_params pqset_params;
	int rc;
	u16 rel_vp_id;

	rc = qed_qm_pqset_get_params(p_hwfn, pq_set_id, tc, &pqset_params);
	if (rc)
		return rc;

	rel_vp_id = pqset_params.rel_qm_vport;

	if (!min_bw) {
		min_bw = 1;
		qm_info->wfq_data[rel_vp_id].configured = false;
	} else {
		qm_info->wfq_data[rel_vp_id].min_speed = min_bw;
		qm_info->wfq_data[rel_vp_id].configured = true;
	}

	qm_info->qm_vport_params[rel_vp_id].wfq = (u16) min_bw;

	return 0;
}

int
qed_qm_update_rt_rl_of_pqset(struct qed_hwfn *p_hwfn, u16 pq_set_id, u32 max_bw)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	struct qed_qm_pqset_params pqset_params;
	int rc;
	u16 rel_rl_id;

	rc = qed_qm_pqset_get_params(p_hwfn, pq_set_id, 0, &pqset_params);
	if (rc)
		return rc;

	rel_rl_id = pqset_params.rel_rl_id;

	qm_info->qm_rl_params[rel_rl_id].vport_rl = max_bw;
	qm_info->qm_rl_params[rel_rl_id].vport_rl_type = QM_RL_TYPE_NORMAL;

	return 0;
}

int
qed_qm_update_wfq_of_pqset(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt,
			   u16 pq_set_id, u8 tc, u32 min_bw)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	struct qed_qm_pqset_params pqset_params;
	struct qed_mcp_link_state *p_link;
	int rc;
	u16 rel_vp_id;

	rc = qed_qm_pqset_get_params(p_hwfn, pq_set_id, tc, &pqset_params);
	if (rc)
		return rc;

	p_link = &QED_LEADING_HWFN(p_hwfn->cdev)->mcp_info->link_output;

	rel_vp_id = pqset_params.rel_qm_vport;

	rc = __qed_configure_vport_wfq(p_hwfn, p_ptt, rel_vp_id, min_bw,
				       p_link->speed);
	if (rc)
		return rc;

	DP_INFO(p_hwfn->cdev,
		"WFQ%d(0x%x) for PQS%d-(TC%d)-PQ%d(0x%x) is configured as %d\n",
		qm_info->qm_vport_params[rel_vp_id].first_tx_pq_id[tc],
		qm_info->qm_vport_params[rel_vp_id].first_tx_pq_id[tc],
		pq_set_id, tc, pqset_params.rel_pqset_base_id,
		pqset_params.rel_pqset_base_id, min_bw);

	return rc;
}

int
qed_qm_update_rl_of_pqset(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, u16 pq_set_id, u32 max_bw)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	struct qed_qm_pqset_params pqset_params;
	struct qed_mcp_link_state *p_link;
	int rc;
	u32 rate_limit;
	u16 abs_rl_id;

	rc = qed_qm_pqset_get_params(p_hwfn, pq_set_id, 0, &pqset_params);
	if (rc)
		return rc;

	if (pqset_params.rel_rl_id >= (u16) RESC_NUM(p_hwfn, QED_RL)) {
		DP_NOTICE(p_hwfn, "rl_id %d exceeds rls num %d\n",
			  pqset_params.rel_rl_id, RESC_NUM(p_hwfn, QED_RL));

		return -EINVAL;
	}

	p_link = &QED_LEADING_HWFN(p_hwfn->cdev)->mcp_info->link_output;
	rate_limit = max_bw ? max_bw : p_link->speed;

	abs_rl_id = pqset_params.abs_rl_id;

	rc = qed_init_global_rl(p_hwfn, p_ptt, abs_rl_id, rate_limit,
				QM_RL_TYPE_NORMAL);
	if (rc)
		return rc;

	qm_info->qm_rl_params[pqset_params.rel_rl_id].vport_rl = rate_limit;
	qm_info->qm_rl_params[pqset_params.rel_rl_id].vport_rl_type =
	    QM_RL_TYPE_NORMAL;

	DP_INFO(p_hwfn->cdev, "RL%d(0x%x) is set as %d\n",
		abs_rl_id, abs_rl_id, max_bw ? max_bw : p_link->speed);

	return rc;
}

int
qed_qm_connect_pqset_to_wfq(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt,
			    u16 pq_set_id, u8 tc, u16 wfq_id)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 max_tc = qed_init_qm_get_num_tcs(p_hwfn);
	u16 abs_pq_ids[QED_PQSET_NUM_OF_PQ_TYPES];
	int rc = 0;
	u16 pq_idx;

	if (tc >= max_tc ||
	    pq_set_id >= (qm_info->num_pqset + qm_info->start_pqset_num)) {
		DP_NOTICE(p_hwfn->cdev,
			  "Invalid PQ set parameters: tc %d, pq_set_id %d\n",
			  tc, pq_set_id);

		return -EINVAL;
	}

	DP_INFO(p_hwfn->cdev, "Connect TC%d of PQS%d to WFQ%d(0x%x)\n",
		tc, pq_set_id, wfq_id, wfq_id);

	rc = qed_pq_set_get_pqs_of_tc(p_hwfn, pq_set_id, tc, abs_pq_ids,
				      QED_PQSET_NUM_OF_PQ_TYPES);
	if (rc)
		return rc;

	for (pq_idx = 0; pq_idx < QED_PQSET_NUM_OF_PQ_TYPES; pq_idx++) {
		if (abs_pq_ids[pq_idx] == QM_INVALID_PQ_ID)
			continue;

		/* PQ configured in previous loop */
		if (pq_idx > 0 &&
		    (abs_pq_ids[pq_idx] == abs_pq_ids[pq_idx - 1]))
			continue;

		rc = qed_qm_config_pq_wfq(p_hwfn, p_ptt, abs_pq_ids[pq_idx],
					  wfq_id);
		if (rc)
			return rc;

		DP_INFO(p_hwfn->cdev, "PQ%d is connected to WFQ%d(0x%x)\n",
			abs_pq_ids[pq_idx], wfq_id, wfq_id);
	}

	return rc;
}

int
qed_qm_connect_pqset_to_rl(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt,
			   u16 pq_set_id, u8 tc, u8 rl_id)
{
	u16 abs_pq_ids[QED_PQSET_NUM_OF_PQ_TYPES], pq_idx;
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 max_tc = qed_init_qm_get_num_tcs(p_hwfn);
	int rc = 0;

	if (tc >= max_tc ||
	    pq_set_id >= (qm_info->num_pqset + qm_info->start_pqset_num)) {
		DP_NOTICE(p_hwfn->cdev,
			  "Invalid PQ set parameters: tc %d, pq_set_id %d\n",
			  tc, pq_set_id);

		return -EINVAL;
	}

	DP_INFO(p_hwfn->cdev, "Connect TC%d of PQS%d to RL%d(0x%x)\n",
		tc, pq_set_id, rl_id, rl_id);

	rc = qed_pq_set_get_pqs_of_tc(p_hwfn, pq_set_id, tc, abs_pq_ids,
				      QED_PQSET_NUM_OF_PQ_TYPES);
	if (rc)
		return rc;

	for (pq_idx = 0; pq_idx < QED_PQSET_NUM_OF_PQ_TYPES; pq_idx++) {
		if (abs_pq_ids[pq_idx] == QM_INVALID_PQ_ID)
			continue;

		/* PQ configured in previous loop */
		if (pq_idx > 0 &&
		    (abs_pq_ids[pq_idx] == abs_pq_ids[pq_idx - 1]))
			continue;

		rc = qed_qm_config_pq_rl(p_hwfn, p_ptt, abs_pq_ids[pq_idx],
					 rl_id);
		if (rc)
			return rc;

		DP_INFO(p_hwfn->cdev, "PQ%d is connected to RL%d(0x%x)\n",
			abs_pq_ids[pq_idx], rl_id, rl_id);
	}

	return rc;
}

int
qed_pq_set_get_pqs_of_tc(struct qed_hwfn *p_hwfn,
			 u16 pq_set, u8 tc, u16 * pqs, u8 num_pqs)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 max_tc = qed_init_qm_get_num_tcs(p_hwfn);
	u16 pq_idx = 0;
	u32 pq_flags;

	if (tc >= max_tc ||
	    pq_set >= (qm_info->num_pqset + qm_info->start_pqset_num)) {
		DP_NOTICE(p_hwfn->cdev,
			  "Invalid PQ set parameters: tc %d, pq_set_id %d\n",
			  tc, pq_set);

		return -EINVAL;
	}

	if (pqs == NULL || num_pqs == 0) {
		DP_NOTICE(p_hwfn->cdev, "Invalid PQs array\n");

		return -EINVAL;
	}

	pqs[pq_idx++] = qed_pqset_get_mcos_pq_idx(p_hwfn, pq_set, tc);

	if (pq_idx == num_pqs)
		return 0;

	pqs[pq_idx++] = qed_pqset_get_ofld_pq_idx(p_hwfn, pq_set, tc);

	if (pq_idx == num_pqs)
		return 0;

	pq_flags = qed_get_pq_flags(p_hwfn);
	pqs[pq_idx++] = qed_pqset_get_aux_pq_idx(p_hwfn, pq_flags, pq_set, tc);

	return 0;
}

int
qed_qm_config_pq_wfq(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt, u16 abs_pq_id, u16 wfq_id)
{
	int rc = 0;
	struct qm_rf_pq_map tx_pq_map;
	bool b_rc;

	if ((abs_pq_id < RESC_START(p_hwfn, QED_PQ)) ||
	    (abs_pq_id >= (RESC_START(p_hwfn, QED_PQ) +
			   RESC_NUM(p_hwfn, QED_PQ))) ||
	    (wfq_id < RESC_START(p_hwfn, QED_PQ)) ||
	    (wfq_id >= (RESC_START(p_hwfn, QED_PQ) +
			RESC_NUM(p_hwfn, QED_PQ)))) {
		DP_NOTICE(p_hwfn->cdev,
			  "Invalid parameters: PQ%d(0x%x), WFQ%d(0x%x)\n",
			  abs_pq_id, abs_pq_id, wfq_id, wfq_id);
		return -EINVAL;
	}

	/* stop TX PQ */
	b_rc = qed_send_qm_stop_cmd(p_hwfn, p_ptt, false, true, abs_pq_id, 1);
	if (!b_rc) {
		DP_NOTICE(p_hwfn->cdev,
			  "Stop TX PQ%d(0x%x) is failed\n",
			  abs_pq_id, abs_pq_id);
		return -EINVAL;
	}

	tx_pq_map.reg = qed_rd(p_hwfn, p_ptt, QM_REG_TXPQMAP + abs_pq_id * 4);
	SET_FIELD(tx_pq_map.reg, QM_RF_PQ_MAP_VP_PQ_ID, wfq_id);
	qed_wr(p_hwfn, p_ptt, QM_REG_TXPQMAP + abs_pq_id * 4, tx_pq_map.reg);

	/* start TXC PQ */
	b_rc = qed_send_qm_stop_cmd(p_hwfn, p_ptt, true, true, abs_pq_id, 1);
	if (!b_rc) {
		DP_NOTICE(p_hwfn->cdev,
			  "Restart TX PQ%d(0x%x) is failed\n",
			  abs_pq_id, abs_pq_id);
		rc = -EINVAL;
	}

	return rc;
}

int
qed_qm_config_pq_rl(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt, u16 abs_pq_id, u8 rl_id)
{
	int rc = 0;
	struct qm_rf_pq_map tx_pq_map;
	bool b_rc;

	if ((abs_pq_id < RESC_START(p_hwfn, QED_PQ)) ||
	    (abs_pq_id >= (RESC_START(p_hwfn, QED_PQ) +
			   RESC_NUM(p_hwfn, QED_PQ))) ||
	    (rl_id < RESC_START(p_hwfn, QED_RL)) ||
	    (rl_id >= (RESC_START(p_hwfn, QED_RL) +
		       RESC_NUM(p_hwfn, QED_RL)))) {
		DP_NOTICE(p_hwfn->cdev,
			  "Invalid parameters: PQ%d(0x%x), RL%d(0x%x)\n",
			  abs_pq_id, abs_pq_id, rl_id, rl_id);
		return -EINVAL;
	}

	/* stop TX PQ */
	b_rc = qed_send_qm_stop_cmd(p_hwfn, p_ptt, false, true, abs_pq_id, 1);
	if (!b_rc)
		DP_NOTICE(p_hwfn->cdev,
			  "Stop TX PQ%d(0x%x) is failed\n",
			  abs_pq_id, abs_pq_id);

	tx_pq_map.reg = qed_rd(p_hwfn, p_ptt, QM_REG_TXPQMAP + abs_pq_id * 4);
	SET_FIELD(tx_pq_map.reg, QM_RF_PQ_MAP_RL_ID, rl_id);
	qed_wr(p_hwfn, p_ptt, QM_REG_TXPQMAP + abs_pq_id * 4, tx_pq_map.reg);

	/* start TXC PQ */
	b_rc = qed_send_qm_stop_cmd(p_hwfn, p_ptt, true, true, abs_pq_id, 1);
	if (!b_rc)
		DP_NOTICE(p_hwfn->cdev,
			  "Restart TX PQ%d(0x%x) is failed\n",
			  abs_pq_id, abs_pq_id);

	return rc;
}

int
qed_qm_get_wfq_of_pqset(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt,
			u16 pq_set_id, u8 tc, u16 * p_wfq_id)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 max_tc = qed_init_qm_get_num_tcs(p_hwfn);
	struct qm_rf_pq_map tx_pq_map;
	int rc;
	u16 wfq_id, abs_pq_id;

	if (p_wfq_id == NULL) {
		DP_NOTICE(p_hwfn->cdev, "p_wfq_id is NULL\n");
		return -EINVAL;
	}

	*p_wfq_id = QM_INVALID_WFQ_ID;

	if (tc >= max_tc ||
	    pq_set_id >= (qm_info->num_pqset + qm_info->start_pqset_num)) {
		DP_NOTICE(p_hwfn->cdev,
			  "Invalid PQ set parameters: tc %d, pq_set_id %d\n",
			  tc, pq_set_id);

		return -EINVAL;
	}

	rc = qed_pq_set_get_pqs_of_tc(p_hwfn, pq_set_id, tc, &abs_pq_id, 1);
	if (rc)
		return rc;

	tx_pq_map.reg = qed_rd(p_hwfn, p_ptt, QM_REG_TXPQMAP + abs_pq_id * 4);
	wfq_id = GET_FIELD(tx_pq_map.reg, QM_RF_PQ_MAP_VP_PQ_ID);

	*p_wfq_id = wfq_id;

	return 0;
}

int
qed_qm_get_rl_of_pqset(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       u16 pq_set_id, u8 tc, u16 * p_rl_id)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 max_tc = qed_init_qm_get_num_tcs(p_hwfn);
	struct qm_rf_pq_map tx_pq_map;
	int rc;
	u16 abs_pq_id;
	u8 rl_id;

	if (p_rl_id == NULL) {
		DP_NOTICE(p_hwfn->cdev, "p_rl_id is NULL\n");
		return -EINVAL;
	}

	*p_rl_id = QM_INVALID_RL_ID;

	if (tc >= max_tc ||
	    pq_set_id >= (qm_info->num_pqset + qm_info->start_pqset_num)) {
		DP_NOTICE(p_hwfn->cdev,
			  "Invalid PQ set parameters: tc %d, pq_set_id %d\n",
			  tc, pq_set_id);

		return -EINVAL;
	}

	rc = qed_pq_set_get_pqs_of_tc(p_hwfn, pq_set_id, tc, &abs_pq_id, 1);
	if (rc)
		return rc;

	tx_pq_map.reg = qed_rd(p_hwfn, p_ptt, QM_REG_TXPQMAP + abs_pq_id * 4);
	rl_id = GET_FIELD(tx_pq_map.reg, QM_RF_PQ_MAP_RL_ID);

	*p_rl_id = rl_id;

	return 0;
}

void qed_qm_acquire_access(struct qed_hwfn *p_hwfn)
{
	spin_lock_bh(p_hwfn->qm_lock);
}

void qed_qm_release_access(struct qed_hwfn *p_hwfn)
{
	spin_unlock_bh(p_hwfn->qm_lock);
}

int qed_sp_fw_assert(struct qed_hwfn *p_hwfn)
{
	struct vport_stop_ramrod_data *p_ramrod;
	struct qed_sp_init_data init_data;
	struct qed_spq_entry *p_ent;
	u8 abs_vport_id = 0;
	int rc;

	if (IS_VF(p_hwfn->cdev))
		return -EOPNOTSUPP;

	rc = qed_fw_vport(p_hwfn, RESC_NUM(p_hwfn, QED_VPORT) - 1,
			  &abs_vport_id);
	if (rc)
		return rc;

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = qed_spq_get_cid(p_hwfn);
	init_data.opaque_fid = p_hwfn->hw_info.opaque_fid;
	init_data.comp_mode = QED_SPQ_MODE_CB;

	rc = qed_sp_init_request(p_hwfn, &p_ent,
				 ETH_RAMROD_VPORT_STOP,
				 PROTOCOLID_ETH, &init_data);
	if (rc)
		return rc;

	p_ramrod = &p_ent->ramrod.vport_stop;
	p_ramrod->vport_id = abs_vport_id;

	return qed_spq_post(p_hwfn, p_ent, NULL);
}

void qed_gen_system_kill(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	qed_wr(p_hwfn, p_ptt, 0x2b0204 /* ATC_REG_PRTY_MASK_H_0 */ , 0x0);
	qed_rd(p_hwfn, p_ptt,
	       0x2b4800 /* first DWORD of ATC_REG_ATC_GPA_ARRAY_ACCESS_STATE */
	       );
}

void qed_gen_process_kill(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, u8 is_common_block)
{
	if (is_common_block) {
		/* The DCBX nvm option should be set to CEE (2) to enable LLDP traffic */
		qed_wr(p_hwfn, p_ptt,
		       0x540420 /* BMB_REG_MEM001_RF_ECC_ERROR_CONNECT */ ,
		       0x20300);
	} else {
		/* Traffic should be run after this register write */
		qed_wr(p_hwfn, p_ptt,
		       0x340420 /* BRB_REG_MEM001_RF_ECC_ERROR_CONNECT */ ,
		       0x20300);
	}
}

void qed_dmae_err(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct dmae_params params;
	dma_addr_t addr;
	u64 victim;

	params.flags = 0;
	SET_FIELD(params.flags, DMAE_PARAMS_SRC_PF_VALID, 0x1);
	SET_FIELD(params.flags, DMAE_PARAMS_COMPLETION_DST, 0x1);
	params.src_pf_id = p_hwfn->rel_pf_id;

	/* write invalid address */
	addr = p_hwfn->dmae_info.intermediate_buffer_phys_addr;
	/* This will cause DMA to host address 0,
	 * recommended to use only when IOMMU, use at your own risk.
	 */
	p_hwfn->dmae_info.intermediate_buffer_phys_addr = 0x0;

	qed_dmae_grc2host(p_hwfn,
			  p_ptt,
			  0xcafecafe, (u64) (uintptr_t) & victim, 1, &params);

	/* restore addr */
	p_hwfn->dmae_info.intermediate_buffer_phys_addr = addr;
}
