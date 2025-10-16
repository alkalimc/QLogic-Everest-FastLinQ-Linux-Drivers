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
#include <linux/stat.h>
#include <asm/byteorder.h>
#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/if_ether.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#define __PREVENT_DUMP_MEM_ARR__
#define __PREVENT_PXP_GLOBAL_WIN__
#include "qed.h"
#include "qed_compat.h"
#include "qed_cxt.h"
#include "qed_dcbx.h"
#include "qed_hsi.h"
#include "qed_hw.h"
#include "qed_init_ops.h"
#include "qed_int.h"
#include "qed_iro_hsi.h"
#include "qed_l2.h"
#include "qed_ll2.h"
#include "qed_mcp.h"
#include "qed_ooo.h"
#include "qed_rdma.h"
#include "qed_rdma_if.h"
#include "qed_reg_addr.h"
#include "qed_sp.h"
#include "qed_sriov.h"
#include "qed_vf.h"
#include "qed_rdma_if.h"

int qed_rdma_bmap_alloc(struct qed_hwfn *p_hwfn,
			struct qed_bmap *bmap, u32 max_count, char *name)
{
	u32 size_in_bytes;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "max_count = %08x\n", max_count);

	bmap->max_count = max_count;

	if (!max_count) {
		bmap->bitmap = NULL;
		return 0;
	}

	size_in_bytes = sizeof(unsigned long) *
	    DIV_ROUND_UP(max_count, (sizeof(unsigned long) * 8));

	bmap->bitmap = kzalloc(size_in_bytes, GFP_KERNEL);
	if (!bmap->bitmap) {
		DP_NOTICE(p_hwfn,
			  "qed bmap alloc failed: cannot allocate memory (bitmap). rc = %d\n",
			  -ENOMEM);
		return -ENOMEM;
	}

	scnprintf(bmap->name, QEDR_MAX_BMAP_NAME, "%s", name);

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "0\n");
	return 0;
}

int qed_rdma_bmap_alloc_id(struct qed_hwfn *p_hwfn,
			   struct qed_bmap *bmap, u32 * id_num)
{
	*id_num = find_first_zero_bit(bmap->bitmap, bmap->max_count);
	if (*id_num >= bmap->max_count)
		return -EINVAL;

	__set_bit(*id_num, bmap->bitmap);

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "%s bitmap: allocated id %d\n",
		   bmap->name, *id_num);

	return 0;
}

void qed_bmap_set_id(struct qed_hwfn *p_hwfn, struct qed_bmap *bmap, u32 id_num)
{
	if (id_num >= bmap->max_count) {
		DP_NOTICE(p_hwfn,
			  "%s bitmap: cannot set id %d max is %d\n",
			  bmap->name, id_num, bmap->max_count);

		return;
	}

	__set_bit(id_num, bmap->bitmap);

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "%s bitmap: set id %d\n",
		   bmap->name, id_num);
}

void qed_bmap_release_id(struct qed_hwfn *p_hwfn,
			 struct qed_bmap *bmap, u32 id_num)
{
	bool b_acquired;

	if (!bmap->bitmap || id_num >= bmap->max_count)
		return;

	b_acquired = test_and_clear_bit(id_num, bmap->bitmap);
	if (!b_acquired) {
		DP_NOTICE(p_hwfn, "%s bitmap: id %d already released\n",
			  bmap->name, id_num);
		return;
	}

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "%s bitmap: released id %d\n",
		   bmap->name, id_num);
}

int qed_bmap_test_id(struct qed_hwfn *p_hwfn, struct qed_bmap *bmap, u32 id_num)
{
	if (!bmap->bitmap)
		return -1;

	if (id_num >= bmap->max_count) {
		DP_NOTICE(p_hwfn,
			  "%s bitmap: id %d too high. max is %d\n",
			  bmap->name, id_num, bmap->max_count);
		return -1;
	}

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "%s bitmap: tested id %d\n",
		   bmap->name, id_num);

	return test_bit(id_num, bmap->bitmap);
}

static bool qed_bmap_is_empty(struct qed_bmap *bmap)
{
	return bmap->max_count == find_first_bit(bmap->bitmap, bmap->max_count);
}

int qed_rdma_info_alloc(struct qed_hwfn *p_hwfn)
{
	struct qed_rdma_info *p_rdma_info;

	p_rdma_info = kzalloc(sizeof(*p_rdma_info), GFP_KERNEL);
	if (!p_rdma_info) {
		DP_NOTICE(p_hwfn,
			  "qed rdma alloc failed: cannot allocate memory (rdma info).\n");
		return -ENOMEM;
	}
	p_hwfn->p_rdma_info = p_rdma_info;

	spin_lock_init(&p_rdma_info->lock);

	return 0;
}

void qed_rdma_info_free(struct qed_hwfn *p_hwfn)
{
	kfree(p_hwfn->p_rdma_info);
	p_hwfn->p_rdma_info = NULL;
}

static int qed_rdma_inc_ref_cnt(struct qed_hwfn *p_hwfn)
{
	int rc = -EINVAL;

	spin_lock_bh(&p_hwfn->p_rdma_info->lock);
	if (p_hwfn->p_rdma_info->active) {
		p_hwfn->p_rdma_info->ref_cnt++;
		rc = 0;
	} else {
		DP_INFO(p_hwfn, "Ref cnt requested for inactive rdma\n");
	}
	spin_unlock_bh(&p_hwfn->p_rdma_info->lock);
	return rc;
}

static void qed_rdma_dec_ref_cnt(struct qed_hwfn *p_hwfn)
{
	spin_lock_bh(&p_hwfn->p_rdma_info->lock);
	p_hwfn->p_rdma_info->ref_cnt--;
	spin_unlock_bh(&p_hwfn->p_rdma_info->lock);
}

u8 qed_rdma_get_start_cnq(struct qed_hwfn *p_hwfn,
			  struct qed_rdma_info *p_rdma_info)
{
	u8 start_cnq;

	if (!p_rdma_info->iov_info.is_vf) {
		start_cnq = (u8) RESC_START(p_hwfn, QED_RDMA_CNQ_RAM);
	} else {
		u8 pf_cnqs_offset = NUM_OF_GLOBAL_QUEUES - p_hwfn->num_vf_cnqs;
		struct qed_vf_info *p_vf = p_rdma_info->iov_info.p_vf;

		start_cnq = (u8) RESC_START(p_hwfn, QED_VF_RDMA_CNQ_RAM) +
		    pf_cnqs_offset + p_vf->cnq_offset;
	}

	return start_cnq;
}

static void qed_rdma_activate(struct qed_rdma_info *rdma_info)
{
	spin_lock_bh(&rdma_info->lock);
	rdma_info->active = true;
	spin_unlock_bh(&rdma_info->lock);
}

/* Part of deactivating rdma is letting all the relevant flows complete before
 * we start shutting down: Currently query-stats which can be called from MCP
 * context.
 */
/* The longest time it can take a rdma flow to complete */
#define QED_RDMA_MAX_FLOW_TIME (100)
static int qed_rdma_deactivate(struct qed_hwfn *p_hwfn)
{
	int wait_count;

	if (IS_VF(p_hwfn->cdev)) {
		DP_ERR(p_hwfn, "VF aborting\n");
		return 0;
	}

	spin_lock_bh(&p_hwfn->p_rdma_info->lock);
	p_hwfn->p_rdma_info->active = false;
	spin_unlock_bh(&p_hwfn->p_rdma_info->lock);

	/* We'll give each flow it's time to complete... */
	wait_count = p_hwfn->p_rdma_info->ref_cnt;

	while (p_hwfn->p_rdma_info->ref_cnt) {
		msleep(QED_RDMA_MAX_FLOW_TIME);
		if (wait_count-- == 0) {
			DP_NOTICE(p_hwfn,
				  "Timeout on refcnt=%d\n",
				  p_hwfn->p_rdma_info->ref_cnt);
			return -EBUSY;
		}
	}
	return 0;
}

static int
qed_rdma_alloc(struct qed_hwfn *p_hwfn, struct qed_rdma_info *p_rdma_info)
{
	u32 num_cons, num_tasks, num_vf_cons;
	enum qed_iov_is_vf_or_pf is_vf;
	int rc;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Allocating RDMA\n");

	if (!p_rdma_info)
		return -EINVAL;

	is_vf = p_rdma_info->iov_info.is_vf;

	/* is_vf was set by qed_sriov when the VF was created.
	 * Set PF's vf_id to QED_CXT_PF_CID to make the code cleaner.
	 */
	if (!is_vf) {
		p_rdma_info->iov_info.abs_vf_id = QED_CXT_PF_CID;
		p_rdma_info->iov_info.rel_vf_id = QED_CXT_PF_CID;
		p_rdma_info->iov_info.opaque_fid = p_hwfn->hw_info.opaque_fid;
	}

	if (p_hwfn->hw_info.personality == QED_PCI_ETH_IWARP)
		p_rdma_info->proto = PROTOCOLID_IWARP;
	else
		p_rdma_info->proto = PROTOCOLID_ROCE;

	num_cons = qed_cxt_get_proto_cid_count(p_hwfn, p_rdma_info->proto,
					       &num_vf_cons);

	if (is_vf)
		num_cons = num_vf_cons;

	if (IS_IWARP(p_hwfn))
		p_rdma_info->num_qps = num_cons;
	else
		p_rdma_info->num_qps = num_cons / 2;

	/* INTERNAL: RoCE & iWARP use the same taskid */
	num_tasks = qed_cxt_get_proto_tid_count(p_hwfn, PROTOCOLID_ROCE,
						p_rdma_info->iov_info.
						abs_vf_id);

	/* Each MR uses a single task */
	p_rdma_info->num_mrs = num_tasks;

	/* Allocate a struct with device params and fill it */
	p_rdma_info->dev = kzalloc(sizeof(*p_rdma_info->dev), GFP_KERNEL);
	if (!p_rdma_info->dev) {
		rc = -ENOMEM;
		DP_NOTICE(p_hwfn,
			  "qed rdma alloc failed: cannot allocate memory (rdma info dev). rc = %d\n",
			  rc);
		return rc;
	}

	/* Allocate a struct with port params and fill it */
	p_rdma_info->port = kzalloc(sizeof(*p_rdma_info->port), GFP_KERNEL);
	if (!p_rdma_info->port) {
		DP_NOTICE(p_hwfn,
			  "qed rdma alloc failed: cannot allocate memory (rdma info port)\n");
		return -ENOMEM;
	}

	/* Allocate bit map for pd's - VF will allocate for itself
	 * since it doesn't require any work from the PF
	 */
	if (!is_vf) {
		rc = qed_rdma_bmap_alloc(p_hwfn, &p_rdma_info->pd_map,
					 RDMA_MAX_PDS, "PD");
		if (rc) {
			DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
				   "Failed to allocate pd_map, rc = %d\n", rc);
			return rc;
		}
	}

	/* Allocate bit map for XRC Domains */
	rc = qed_rdma_bmap_alloc(p_hwfn, &p_rdma_info->xrcd_map,
				 QED_RDMA_MAX_XRCDS, "XRCD");
	if (rc) {
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
			   "Failed to allocate xrcd_map,rc = %d\n", rc);
		return rc;
	}

	/* Allocate DPI bitmap for PF only - VF will allocate for itself
	 * since it manages its own BAR
	 */
	if (!is_vf) {
		rc = qed_rdma_bmap_alloc(p_hwfn, &p_rdma_info->dpi_map,
					 p_hwfn->dpi_info.dpi_count, "DPI");
		if (rc) {
			DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
				   "Failed to allocate DPI bitmap, rc = %d\n",
				   rc);
			return rc;
		}
	}

	/* Allocate bitmap for cq's. The maximum number of CQs is bounded to
	 * twice the number of QPs.
	 */
	rc = qed_rdma_bmap_alloc(p_hwfn, &p_rdma_info->cq_map, num_cons, "CQ");
	if (rc) {
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
			   "Failed to allocate cq bitmap, rc = %d\n", rc);
		return rc;
	}

	/* Allocate bitmap for toggle bit for cq icids
	 * We toggle the bit every time we create or resize cq for a given icid.
	 * The maximum number of CQs is bounded to the number of connections we
	 * support. (num_qps in iWARP or num_qps/2 in RoCE).
	 */
	rc = qed_rdma_bmap_alloc(p_hwfn, &p_rdma_info->toggle_bits,
				 num_cons, "Toggle");
	if (rc) {
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
			   "Failed to allocate toggle bits, rc = %d\n", rc);
		return rc;
	}

	/* Allocate bitmap for itids */
	rc = qed_rdma_bmap_alloc(p_hwfn, &p_rdma_info->tid_map,
				 p_rdma_info->num_mrs, "MR");
	if (rc) {
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
			   "Failed to allocate itids bitmaps, rc = %d\n", rc);
		return rc;
	}

	/* Allocate bitmap for qps. */
	rc = qed_rdma_bmap_alloc(p_hwfn, &p_rdma_info->qp_map,
				 p_rdma_info->num_qps, "QP");
	if (rc) {
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
			   "Failed to allocate qp bitmap, rc = %d\n", rc);
		return rc;
	}

	/* Allocate bitmap for cids used for responders/requesters. */
	rc = qed_rdma_bmap_alloc(p_hwfn, &p_rdma_info->cid_map, num_cons,
				 "REAL CID");
	if (rc) {
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
			   "Failed to allocate cid bitmap, rc = %d\n", rc);
		return rc;
	}

	/* Allocate bitmap for suspended cids used for responders/requesters. */
	rc = qed_rdma_bmap_alloc(p_hwfn, &p_rdma_info->sus_cid_map, num_cons,
				 "SUS CID");
	if (rc) {
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
			   "Failed to allocate suspened cid bmap, rc %d\n", rc);
		return rc;
	}

	/* The first SRQ follows the last XRC SRQ. This means that the
	 * SRQ IDs start from an offset equals to num_xrc_srqs.
	 */
	p_rdma_info->srq_id_offset = (u16) qed_cxt_get_xrc_srq_count(p_hwfn);
	rc = qed_rdma_bmap_alloc(p_hwfn,
				 &p_rdma_info->xrc_srq_map,
				 p_rdma_info->srq_id_offset, "XRC SRQ");
	if (rc) {
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
			   "Failed to allocate xrc srq bitmap, rc = %d\n", rc);
		return rc;
	}

	/* Allocate bitmap for srqs */
	p_rdma_info->num_srqs = qed_cxt_get_srq_count(p_hwfn, is_vf);
	if (is_vf) {
		u8 rel_vf_id = p_rdma_info->iov_info.rel_vf_id;

		/* The first VF's SRQ ID follows the last PF's SRQ ID. */
		p_rdma_info->srq_id_offset += (u16)
		    qed_cxt_get_srq_count(p_hwfn, IOV_PF);
		p_rdma_info->srq_id_offset += rel_vf_id * p_rdma_info->num_srqs;
	}
	rc = qed_rdma_bmap_alloc(p_hwfn, &p_rdma_info->srq_map,
				 p_rdma_info->num_srqs, "SRQ");

	if (rc) {
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
			   "Failed to allocate srq bitmap, rc = %d\n", rc);

		return rc;
	}

	if (IS_IWARP(p_hwfn))
		rc = qed_iwarp_alloc(p_hwfn, p_rdma_info);

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "rc = %d\n", rc);

	return rc;
}

void qed_rdma_bmap_free(struct qed_hwfn *p_hwfn,
			struct qed_bmap *bmap, bool check)
{
	int weight, line, item, last_line, last_item;
	u64 *pmap;

	if (!bmap || !bmap->bitmap)
		return;

	if (!check)
		goto end;

	weight = bitmap_weight(bmap->bitmap, bmap->max_count);
	if (!weight)
		goto end;

	DP_NOTICE(p_hwfn,
		  "%s bitmap not free - size=%d, weight=%d, 512 bits per line\n",
		  bmap->name, bmap->max_count, weight);

	pmap = (u64 *) bmap->bitmap;
	last_line = bmap->max_count / (64 * 8);
	last_item = last_line * 8 + (((bmap->max_count % (64 * 8)) + 63) / 64);

	/* print aligned non-zero lines, if any */
	for (item = 0, line = 0; line < last_line; line++, item += 8)
		if (bitmap_weight((unsigned long *)&pmap[item], 64 * 8))
			DP_NOTICE(p_hwfn,
				  "line 0x%04x: 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx\n",
				  line,
				  pmap[item],
				  pmap[item + 1],
				  pmap[item + 2],
				  pmap[item + 3],
				  pmap[item + 4],
				  pmap[item + 5],
				  pmap[item + 6], pmap[item + 7]);

	/* print last unaligned non-zero line, if any */
	if ((bmap->max_count % (64 * 8)) &&
	    (bitmap_weight((unsigned long *)&pmap[item],
			   bmap->max_count - item * 64))) {
		char str_last_line[200] = { 0 };
		int offset;

		offset = scnprintf(str_last_line, sizeof(str_last_line),
				   "line 0x%04x: ", line);
		for (; item < last_item; item++)
			offset += sprintf(str_last_line + offset,
					  "0x%016llx ", pmap[item]);
		DP_NOTICE(p_hwfn, "%s\n", str_last_line);
	}

end:
	kfree(bmap->bitmap);
	bmap->bitmap = NULL;
}

void qed_rdma_resc_free(struct qed_hwfn *p_hwfn,
			struct qed_rdma_info *rdma_info)
{
	bool check;

	if (IS_IWARP(p_hwfn))
		qed_iwarp_resc_free(p_hwfn);

	check = !rdma_info->no_bmap_check;
	qed_rdma_bmap_free(p_hwfn, &rdma_info->sus_cid_map, check);
	qed_rdma_bmap_free(p_hwfn, &rdma_info->cid_map, check);
	qed_rdma_bmap_free(p_hwfn, &rdma_info->qp_map, check);
	qed_rdma_bmap_free(p_hwfn, &rdma_info->pd_map, check);
	qed_rdma_bmap_free(p_hwfn, &rdma_info->xrcd_map, check);
	qed_rdma_bmap_free(p_hwfn, &rdma_info->dpi_map, check);
	qed_rdma_bmap_free(p_hwfn, &rdma_info->cq_map, check);
	qed_rdma_bmap_free(p_hwfn, &rdma_info->toggle_bits, 0);
	qed_rdma_bmap_free(p_hwfn, &rdma_info->tid_map, check);
	qed_rdma_bmap_free(p_hwfn, &rdma_info->srq_map, check);
	qed_rdma_bmap_free(p_hwfn, &rdma_info->xrc_srq_map, check);

	kfree(rdma_info->port);
	rdma_info->port = NULL;

	kfree(rdma_info->dev);
	rdma_info->dev = NULL;
}

static inline void
qed_rdma_free_reserved_lkey(struct qed_hwfn *p_hwfn,
			    struct qed_rdma_info *rdma_info)
{
	qed_rdma_free_tid_inner(p_hwfn, rdma_info->dev->reserved_lkey,
				rdma_info);
}

static void qed_rdma_free_ilt(struct qed_hwfn *p_hwfn,
			      struct qed_rdma_iov_info *p_iov_info)
{
	u8 abs_vf_id = p_iov_info->abs_vf_id;
	u8 rel_vf_id = p_iov_info->rel_vf_id;
	u32 num_cids, num_vf_cids;

	num_cids = qed_cxt_get_proto_cid_count(p_hwfn,
					       p_hwfn->p_rdma_info->proto,
					       &num_vf_cids);

	/* Free Connection CXT */
	qed_cxt_free_ilt_range(p_hwfn, QED_ELEM_CXT,
			       qed_cxt_get_proto_cid_start(p_hwfn,
							   p_hwfn->p_rdma_info->
							   proto, abs_vf_id),
			       (rel_vf_id !=
				QED_CXT_PF_CID ? num_vf_cids : num_cids),
			       rel_vf_id);

	/* Free Task CXT ( Intentionally RoCE as task-id is shared between
	 * RoCE and iWARP
	 */
	qed_cxt_free_ilt_range(p_hwfn, QED_ELEM_TASK, 0,
			       qed_cxt_get_proto_tid_count(p_hwfn,
							   PROTOCOLID_ROCE,
							   abs_vf_id),
			       rel_vf_id);

	if (rel_vf_id == QED_CXT_PF_CID) {
		/* Free TSDM CXT - only PF */
		qed_cxt_free_ilt_range(p_hwfn, QED_ELEM_XRC_SRQ, 0,
				       qed_cxt_get_xrc_srq_count(p_hwfn),
				       rel_vf_id);
		qed_cxt_free_ilt_range(p_hwfn, QED_ELEM_SRQ,
				       qed_cxt_get_xrc_srq_count(p_hwfn),
				       qed_cxt_get_srq_count(p_hwfn, IOV_PF),
				       rel_vf_id);
	}
}

void qed_rdma_free(struct qed_hwfn *p_hwfn, struct qed_rdma_info *rdma_info)
{
	u32 i;

	if (!rdma_info || !rdma_info->dev)
		return;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "\n");

	qed_rdma_free_reserved_lkey(p_hwfn, rdma_info);

	if (IS_ROCE(p_hwfn))
		qed_roce_free_reserved_qp_idx(p_hwfn, rdma_info);

	qed_rdma_resc_free(p_hwfn, rdma_info);

	/* Free ILT - in case this is VF unloaded flow, free the VF's ILT lines.
	 * In case this is a PF unload flow, free any of its VF ILT lines
	 * (can be due to unloaded VFs, lost VFs, races with FLR, etc)
	 * In addition free the PF's ILT lines.
	 */

	if (!rdma_info->iov_info.is_vf) {
		qed_for_each_vf(p_hwfn, i) {
			struct qed_vf_info *p_vf;

			p_vf = qed_iov_get_vf_info(p_hwfn, (u16) i, true);
			if (!p_vf)
				continue;

			/* TODO: There's ILT error that should be fixed. */
			/* qed_rdma_free_ilt(p_hwfn, p_vf->abs_vf_id); */
		}
	}

	qed_rdma_free_ilt(p_hwfn, &rdma_info->iov_info);
}

void qed_rdma_get_guid(struct qed_hwfn *p_hwfn, u8 * guid)
{
	u8 *p_addr;
	u8 mac_addr[6];

	p_addr = IS_PF(p_hwfn->cdev) ? &p_hwfn->hw_info.hw_mac_addr[0] :
	    &p_hwfn->vf_iov_info->mac_addr[0];

	memcpy(&mac_addr[0], p_addr, ETH_ALEN);
	guid[0] = mac_addr[0] ^ 2;
	guid[1] = mac_addr[1];
	guid[2] = mac_addr[2];
	guid[3] = 0xff;
	guid[4] = 0xfe;
	guid[5] = mac_addr[3];
	guid[6] = mac_addr[4];
	guid[7] = mac_addr[5];
}

static void qed_rdma_init_events(struct qed_rdma_start_in_params *params,
				 struct qed_rdma_info *p_rdma_info)
{
	struct qed_rdma_events *events;

	events = &p_rdma_info->events;

	events->unaffiliated_event = params->events->unaffiliated_event;
	events->affiliated_event = params->events->affiliated_event;
	events->context = params->events->context;
}

static void qed_rdma_init_vf_devinfo(struct qed_hwfn *p_hwfn,
				     struct qed_rdma_start_in_params *params,
				     struct qed_rdma_info *rdma_info)
{
	/* ARIEL TODO : Check to see if we need to change anything for VF. */
	struct qed_rdma_device *dev = rdma_info->dev;

	/* Vendor specific information */
	dev->vendor_id = p_hwfn->cdev->vendor_id;
	dev->vendor_part_id = p_hwfn->cdev->device_id;
	dev->hw_ver = 0;
	dev->fw_ver = STORM_FW_VERSION;

	dev->max_sge = min_t(u32, RDMA_MAX_SGE_PER_SQ_WQE,
			     RDMA_MAX_SGE_PER_RQ_WQE);

	if (p_hwfn->cdev->rdma_max_sge) {
		dev->max_sge = min_t(u32,
				     p_hwfn->cdev->rdma_max_sge, dev->max_sge);
	}

	/* Set these values according to configuration
	 * MAX SGE for SRQ is not defined by FW for now
	 * define it in driver.
	 * TODO: Get this value from FW.
	 */
	dev->max_srq_sge = QED_RDMA_MAX_SGE_PER_SRQ_WQE;
	if (p_hwfn->cdev->rdma_max_srq_sge) {
		dev->max_srq_sge = min_t(u32,
					 p_hwfn->cdev->rdma_max_srq_sge,
					 dev->max_srq_sge);
	}

	dev->max_inline = ROCE_REQ_MAX_INLINE_DATA_SIZE;
	dev->max_inline = (p_hwfn->cdev->rdma_max_inline) ?
	    min_t(u32,
		  p_hwfn->cdev->rdma_max_inline,
		  dev->max_inline) : dev->max_inline;

	dev->max_wqe = QED_RDMA_MAX_WQE;
	dev->max_cnq = QED_RDMA_VF_MAX_CNQS;

	/* The number of QPs may be higher than QED_ROCE_MAX_QPS. because
	 * it is up-aligned to 16 and then to ILT page size within qed cxt.
	 * This is OK in terms of ILT but we don't want to configure the FW
	 * above its abilities
	 */
	dev->max_qp = min_t(u64, ROCE_MAX_QPS, rdma_info->num_qps);

	/* CQs uses the same icids that QPs use hence they are limited by the
	 * number of icids. There are two icids per QP.
	 */
	dev->max_cq = dev->max_qp * 2;

	/* The number of mrs is smaller by 1 since the first is reserved */
	dev->max_mr = rdma_info->num_mrs - 1;
	dev->max_mr_size = QED_RDMA_MAX_MR_SIZE;
	/* The maximum CQE capacity per CQ supported */
	/* max number of cqes will be in two layer pbl,
	 * 8 is the pointer size in bytes
	 * 32 is the size of cq element in bytes
	 */
	if (params->roce.cq_mode == QED_RDMA_CQ_MODE_32_BITS)
		dev->max_cqe = QED_RDMA_MAX_CQE_32_BIT;
	else
		dev->max_cqe = QED_RDMA_MAX_CQE_16_BIT;

	dev->max_mw = 0;
	dev->max_fmr = QED_RDMA_MAX_FMR;
	dev->max_mr_mw_fmr_pbl = (PAGE_SIZE / 8) * (PAGE_SIZE / 8);
	dev->max_mr_mw_fmr_size = dev->max_mr_mw_fmr_pbl * PAGE_SIZE;
	dev->max_pkey = QED_RDMA_MAX_P_KEY;
	/* Right now we dont take any parameters from user
	 * So assign predefined max_srq to num_srqs.
	 */
	dev->max_srq = rdma_info->num_srqs;

	/* SRQ WQE size */
	dev->max_srq_wr = QED_RDMA_MAX_SRQ_WQE_ELEM;

	dev->max_qp_resp_rd_atomic_resc = RDMA_RING_PAGE_SIZE /
	    (RDMA_RESP_RD_ATOMIC_ELM_SIZE * 2);
	dev->max_qp_req_rd_atomic_resc = RDMA_RING_PAGE_SIZE /
	    RDMA_REQ_RD_ATOMIC_ELM_SIZE;

	dev->max_dev_resp_rd_atomic_resc =
	    dev->max_qp_resp_rd_atomic_resc * rdma_info->num_qps;
	dev->page_size_caps = QED_RDMA_PAGE_SIZE_CAPS;
	dev->dev_ack_delay = QED_RDMA_ACK_DELAY;
	dev->max_pd = RDMA_MAX_PDS;
	dev->max_ah = dev->max_qp;
	dev->max_stats_queues = 1;

	if (IS_IWARP(p_hwfn))
		qed_iwarp_init_devinfo(rdma_info);
}

static void qed_rdma_init_devinfo(struct qed_hwfn *p_hwfn,
				  struct qed_rdma_start_in_params *params)
{
	struct qed_rdma_device *dev = p_hwfn->p_rdma_info->dev;

	/* Vendor specific information */
	dev->vendor_id = p_hwfn->cdev->vendor_id;
	dev->vendor_part_id = p_hwfn->cdev->device_id;
	dev->hw_ver = 0;
	dev->fw_ver = STORM_FW_VERSION;

	qed_rdma_get_guid(p_hwfn, (u8 *) (&dev->sys_image_guid));
	dev->node_guid = dev->sys_image_guid;

	dev->max_sge = min_t(u32, RDMA_MAX_SGE_PER_SQ_WQE,
			     RDMA_MAX_SGE_PER_RQ_WQE);

	if (p_hwfn->cdev->rdma_max_sge) {
		dev->max_sge = min_t(u32,
				     p_hwfn->cdev->rdma_max_sge, dev->max_sge);
	}

	/* Set these values according to configuration
	 * MAX SGE for SRQ is not defined by FW for now
	 * define it in driver.
	 * TODO: Get this value from FW.
	 */
	dev->max_srq_sge = QED_RDMA_MAX_SGE_PER_SRQ_WQE;
	if (p_hwfn->cdev->rdma_max_srq_sge) {
		dev->max_srq_sge = min_t(u32,
					 p_hwfn->cdev->rdma_max_srq_sge,
					 dev->max_srq_sge);
	}

	dev->max_inline = ROCE_REQ_MAX_INLINE_DATA_SIZE;
	dev->max_inline = (p_hwfn->cdev->rdma_max_inline) ?
	    min_t(u32,
		  p_hwfn->cdev->rdma_max_inline,
		  dev->max_inline) : dev->max_inline;

	dev->max_wqe = QED_RDMA_MAX_WQE;
	dev->max_cnq = (u8) FEAT_NUM(p_hwfn, QED_RDMA_CNQ);

	/* The number of QPs may be higher than QED_ROCE_MAX_QPS. because
	 * it is up-aligned to 16 and then to ILT page size within qed cxt.
	 * This is OK in terms of ILT but we don't want to configure the FW
	 * above its abilities
	 */
	dev->max_qp = min_t(u64, ROCE_MAX_QPS, p_hwfn->p_rdma_info->num_qps);

	/* CQs uses the same icids that QPs use hence they are limited by the
	 * number of icids. There are two icids per QP.
	 */
	dev->max_cq = dev->max_qp * 2;

	/* The number of mrs is smaller by 1 since the first is reserved */
	dev->max_mr = p_hwfn->p_rdma_info->num_mrs - 1;
	dev->max_mr_size = QED_RDMA_MAX_MR_SIZE;
	/* The maximum CQE capacity per CQ supported */
	/* max number of cqes will be in two layer pbl,
	 * 8 is the pointer size in bytes
	 * 32 is the size of cq element in bytes
	 */
	if (params->roce.cq_mode == QED_RDMA_CQ_MODE_32_BITS)
		dev->max_cqe = QED_RDMA_MAX_CQE_32_BIT;
	else
		dev->max_cqe = QED_RDMA_MAX_CQE_16_BIT;

	dev->max_mw = 0;
	dev->max_fmr = QED_RDMA_MAX_FMR;
	dev->max_mr_mw_fmr_pbl = (PAGE_SIZE / 8) * (PAGE_SIZE / 8);
	dev->max_mr_mw_fmr_size = dev->max_mr_mw_fmr_pbl * PAGE_SIZE;
	dev->max_pkey = QED_RDMA_MAX_P_KEY;
	/* Right now we dont take any parameters from user
	 * So assign predefined max_srq to num_srqs.
	 */
	dev->max_srq = p_hwfn->p_rdma_info->num_srqs;

	/* SRQ WQE size */
	dev->max_srq_wr = QED_RDMA_MAX_SRQ_WQE_ELEM;

	dev->max_qp_resp_rd_atomic_resc = RDMA_RING_PAGE_SIZE /
	    (RDMA_RESP_RD_ATOMIC_ELM_SIZE * 2);
	dev->max_qp_req_rd_atomic_resc = RDMA_RING_PAGE_SIZE /
	    RDMA_REQ_RD_ATOMIC_ELM_SIZE;

	dev->max_dev_resp_rd_atomic_resc =
	    dev->max_qp_resp_rd_atomic_resc * p_hwfn->p_rdma_info->num_qps;
	dev->page_size_caps = QED_RDMA_PAGE_SIZE_CAPS;
	dev->dev_ack_delay = QED_RDMA_ACK_DELAY;
	dev->max_pd = RDMA_MAX_PDS;
	dev->max_ah = dev->max_qp;
	dev->max_stats_queues =
	    (u8) NUM_OF_RDMA_STATISTIC_COUNTERS(p_hwfn->cdev);

	if (IS_IWARP(p_hwfn))
		qed_iwarp_init_devinfo(p_hwfn->p_rdma_info);
}

static void qed_rdma_init_port(struct qed_hwfn *p_hwfn,
			       struct qed_rdma_info *rdma_info)
{
	struct qed_rdma_port *port = rdma_info->port;
	struct qed_rdma_device *dev = rdma_info->dev;

	port->port_state = p_hwfn->mcp_info->link_output.link_up ?
	    QED_RDMA_PORT_UP : QED_RDMA_PORT_DOWN;

	port->max_msg_size = min_t(u64,
				   (dev->max_mr_mw_fmr_size *
				    p_hwfn->cdev->rdma_max_sge),
				   ((u64) 1 << 31));

	port->pkey_bad_counter = 0;
}

static int qed_rdma_init_hw(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 ll2_ethertype_en;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Initializing HW\n");
	p_hwfn->b_rdma_enabled_in_prs = false;

	if (IS_IWARP(p_hwfn)) {
		qed_iwarp_init_hw(p_hwfn, p_ptt);
		return 0;
	}

	qed_wr(p_hwfn, p_ptt, PRS_REG_ROCE_DEST_QP_MAX_PF, 0);

	qed_roce_pvrdma_init_hw(p_hwfn, p_ptt);

	/* Setting PRS_REG_ROCE_DEST_QP_MAX_VF to 0 can casue troubles when
	 * working with VMs, e.g loading PF RDMA driver while RDMA traffic is
	 * running on VMs will cause traffic termination
	 */

	p_hwfn->rdma_prs_search_reg = PRS_REG_SEARCH_ROCE;

	/* We delay writing to this reg until first cid is allocated.
	 * See the qed_rdma_configure_prs() function for more details.
	 */

	ll2_ethertype_en = qed_rd(p_hwfn, p_ptt, PRS_REG_LIGHT_L2_ETHERTYPE_EN);
	qed_wr(p_hwfn, p_ptt, PRS_REG_LIGHT_L2_ETHERTYPE_EN,
	       (ll2_ethertype_en | 0x01));

#ifndef REAL_ASIC_ONLY
	if (QED_IS_BB_A0(p_hwfn->cdev) && QED_IS_CMT(p_hwfn->cdev)) {
		qed_wr(p_hwfn, p_ptt, NIG_REG_LLH_ENG_CLS_ENG_ID_TBL, 0);
		qed_wr(p_hwfn, p_ptt, NIG_REG_LLH_ENG_CLS_ENG_ID_TBL + 4, 0);
	}
#endif

	if (qed_cxt_get_proto_cid_start(p_hwfn, PROTOCOLID_ROCE,
					p_hwfn->p_rdma_info->iov_info.abs_vf_id)
	    % 2) {
		DP_NOTICE(p_hwfn, "The first RoCE's cid should be even\n");
		return -EINVAL;
	}

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Initializing HW - Done\n");
	return 0;
}

static u16
qed_rdma_get_igu_sb_id(struct qed_hwfn *p_hwfn,
		       struct qed_rdma_info *p_rdma_info, u8 cnq_id)
{
	u16 igu_sb_id, sb_id;

	if (p_rdma_info->iov_info.is_vf) {
		struct qed_vf_info *p_vf = p_rdma_info->iov_info.p_vf;

		sb_id = p_vf->cnq_sb_start_id + cnq_id;
		igu_sb_id = p_vf->igu_sbs[sb_id];
	} else {
		sb_id = (u16) qed_rdma_get_sb_id(p_hwfn, cnq_id);
		igu_sb_id = qed_get_igu_sb_id(p_hwfn, sb_id);
	}

	return igu_sb_id;
}

u16
qed_rdma_get_queue_zone(struct qed_hwfn * p_hwfn,
			struct qed_rdma_info * p_rdma_info, u8 cnq_id)
{
	u16 queue_zone;

	if (p_rdma_info->iov_info.is_vf) {
		struct qed_vf_info *p_vf = p_rdma_info->iov_info.p_vf;

		/* we assume that #cnqs <= #l2_queues */
		queue_zone = (u16) RESC_START(p_hwfn, QED_L2_QUEUE) +
		    p_vf->vf_queues[cnq_id].fw_rx_qid;
	} else {
		/* we arbitrarily decide that cnq_id will be as qz_offset */
		queue_zone = p_rdma_info->queue_zone_base + cnq_id;
	}

	return queue_zone;
}

static int qed_rdma_start_fw(struct qed_hwfn *p_hwfn,
#ifdef CONFIG_DCQCN
			     struct qed_ptt *p_ptt,
#else
			     struct qed_ptt __maybe_unused * p_ptt,
#endif
			     struct qed_rdma_start_in_params *params,
			     struct qed_rdma_info *p_rdma_info)
{
	struct rdma_init_func_ramrod_data *p_ramrod;
	struct rdma_init_func_hdr *pheader;
	struct qed_sp_init_data init_data;
	struct qed_spq_entry *p_ent;
	u16 igu_sb_id, queue_zone_num;
	u8 ll2_queue_id, cnq_id;
	int rc;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Starting FW\n");

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));

	init_data.opaque_fid = p_rdma_info->iov_info.opaque_fid;

	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	rc = qed_sp_init_request(p_hwfn, &p_ent, RDMA_RAMROD_FUNC_INIT,
				 p_rdma_info->proto, &init_data);
	if (rc)
		return rc;

	if (IS_IWARP(p_hwfn)) {
		qed_iwarp_init_fw_ramrod(p_hwfn,
					 &p_ent->ramrod.iwarp_init_func);
		p_ramrod = &p_ent->ramrod.iwarp_init_func.rdma;
	} else {
#ifdef CONFIG_DCQCN
		rc = qed_roce_dcqcn_cfg(p_hwfn, &params->roce.dcqcn_params,
					&p_ent->ramrod.roce_init_func, p_ptt);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "Failed to configure DCQCN. rc = %d.\n", rc);
			qed_sp_destroy_request(p_hwfn, p_ent);
			return rc;
		}
#endif
		p_ramrod = &p_ent->ramrod.roce_init_func.rdma;

		if (!p_rdma_info->iov_info.is_vf) {
			/* The ll2_queue_id is used only for UD QPs */
			ll2_queue_id =
			    qed_ll2_handle_to_queue_id(p_hwfn,
						       params->roce.ll2_handle,
						       QED_LL2_RX_TYPE_LEGACY,
						       QED_CXT_PF_CID);

			if (ll2_queue_id == QED_LL2_INVALID_QID)
				return -EINVAL;

			p_ent->ramrod.roce_init_func.roce.ll2_queue_id =
			    ll2_queue_id;
		}
	}

	pheader = &p_ramrod->params_header;
	pheader->num_cnqs = params->desired_cnq;
	pheader->cnq_start_offset = qed_rdma_get_start_cnq(p_hwfn, p_rdma_info);

	/* The first SRQ ILT page is used for XRC SRQs and all the following
	 * pages contain regular SRQs. Hence the first regular SRQ ID is the
	 * maximum number XRC SRQs.
	 * FW uses the same ILT table for PF/VF - first PF's SRQ's, then
	 * VF_0 SRQ's, VF_1.. etc. Hence, the vf start ramrod needs to use the
	 * same srq base address as the PF. The entry to the correct context
	 * element is calculated by the FW according to the SRQ ID.
	 */
	pheader->first_reg_srq_id =
	    cpu_to_le16(p_hwfn->p_rdma_info->srq_id_offset);
	pheader->reg_srq_base_addr =
	    cpu_to_le32(qed_cxt_get_ilt_page_size(p_hwfn));

	if (params->roce.cq_mode == QED_RDMA_CQ_MODE_16_BITS)
		pheader->cq_ring_mode = 1;	/* 1=16 bits */
	else
		pheader->cq_ring_mode = 0;	/* 0=32 bits */

	for (cnq_id = 0; cnq_id < params->desired_cnq; cnq_id++) {
		igu_sb_id = qed_rdma_get_igu_sb_id(p_hwfn, p_rdma_info, cnq_id);

		p_ramrod->cnq_params[cnq_id].sb_num = cpu_to_le16(igu_sb_id);

		p_ramrod->cnq_params[cnq_id].sb_index =
		    p_hwfn->pf_params.rdma_pf_params.gl_pi;

		p_ramrod->cnq_params[cnq_id].num_pbl_pages =
		    params->cnq_pbl_list[cnq_id].num_pbl_pages;

		p_ramrod->cnq_params[cnq_id].pbl_base_addr.hi =
		    DMA_HI_LE(params->cnq_pbl_list[cnq_id].pbl_ptr);
		p_ramrod->cnq_params[cnq_id].pbl_base_addr.lo =
		    DMA_LO_LE(params->cnq_pbl_list[cnq_id].pbl_ptr);

		queue_zone_num = qed_rdma_get_queue_zone(p_hwfn, p_rdma_info,
							 cnq_id);

		p_ramrod->cnq_params[cnq_id].queue_zone_num =
		    cpu_to_le16(queue_zone_num);
	}

	if (p_rdma_info->iov_info.is_vf) {
		p_ramrod->params_header.vf_valid = 1;
		p_ramrod->params_header.vf_id = p_rdma_info->iov_info.abs_vf_id;
	}

	qed_roce_pvrdma_config_mode(p_hwfn, params, p_ramrod);

	rc = qed_spq_post(p_hwfn, p_ent, NULL);

	return rc;
}

int
qed_rdma_alloc_tid_inner(struct qed_hwfn *p_hwfn,
			 u32 * itid, struct qed_rdma_info *rdma_info)
{
	int rc;

	spin_lock_bh(&rdma_info->lock);
	rc = qed_rdma_bmap_alloc_id(p_hwfn, &rdma_info->tid_map, itid);
	spin_unlock_bh(&rdma_info->lock);

	if (rc) {
		DP_VERBOSE(p_hwfn, false, "Failed in allocating tid\n");
		goto out;
	}

	rc = qed_cxt_dynamic_ilt_alloc(p_hwfn, QED_ELEM_TASK, *itid,
				       rdma_info->iov_info.rel_vf_id);
out:
	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Allocate TID - done, rc = %d\n", rc);
	return rc;
}

int qed_rdma_alloc_tid(void *rdma_cxt, u32 * itid)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	int rc;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Allocate TID\n");

	if (IS_VF(p_hwfn->cdev))
		rc = qed_vf_pf_rdma_alloc_tid(p_hwfn, itid);
	else
		rc = qed_rdma_alloc_tid_inner(p_hwfn, itid,
					      p_hwfn->p_rdma_info);

	if (rc == -EINVAL)
		DP_ERR(p_hwfn, "Out of MR resources\n");

	return rc;
}

static inline int qed_rdma_reserve_lkey(struct qed_hwfn *p_hwfn,
					struct qed_rdma_info *rdma_info)
{
	struct qed_rdma_device *dev = rdma_info->dev;

	/* Tid 0 will be used as the key for "reserved MR".
	 * The driver should allocate memory for it so it can be loaded but no
	 * ramrod should be passed on it.
	 */
	qed_rdma_alloc_tid_inner(p_hwfn, &dev->reserved_lkey, rdma_info);

	if (!rdma_info->iov_info.is_vf &&
	    dev->reserved_lkey != RDMA_RESERVED_LKEY) {
		DP_NOTICE(p_hwfn,
			  "Reserved lkey should be equal to RDMA_RESERVED_LKEY\n");
		return -EINVAL;
	}

	return 0;
}

static int qed_rdma_setup(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt,
			  struct qed_rdma_start_in_params *params,
			  struct qed_rdma_info *rdma_info)
{
	/* TODO : Pass rdma_info to iwarp functions, if needed. */
	u8 num_cnqs = params->desired_cnq;
	int rc = 0;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "RDMA setup\n");

	if (!rdma_info->iov_info.is_vf)
		qed_rdma_init_devinfo(p_hwfn, params);
	else
		qed_rdma_init_vf_devinfo(p_hwfn, params, rdma_info);

	rdma_info->num_cnqs = num_cnqs;

	/* Queue zone lines are shared between RoCE and L2 in such a way that
	 * they can be used by each without obstructing the other.
	 */
	rdma_info->queue_zone_base = (u16) RESC_START(p_hwfn, QED_L2_QUEUE);
	rdma_info->max_queue_zones = (u16) RESC_NUM(p_hwfn, QED_L2_QUEUE);

	qed_roce_pvrdma_init(rdma_info, params);

	qed_rdma_init_port(p_hwfn, rdma_info);

	if (!rdma_info->iov_info.is_vf)	/* Call only for PF. */
		qed_rdma_init_events(params, rdma_info);

	rc = qed_rdma_reserve_lkey(p_hwfn, rdma_info);
	if (rc)
		return rc;

	if (IS_ROCE(p_hwfn)) {
		rc = qed_roce_reserve_qp_idx(p_hwfn, rdma_info);
		if (rc)
			return rc;
	}

	/* VF currently doesn't support XRC mode */
	if (!rdma_info->iov_info.is_vf)
		rdma_info->xrc_supported = true;

	if (!rdma_info->iov_info.is_vf) {
		rc = qed_rdma_init_hw(p_hwfn, p_ptt);
		if (rc)
			return rc;

		if (IS_IWARP(p_hwfn)) {
			/* Looks like most of the things in below function
			 * need not be done for VF.
			 * Will need to revisit when iwarp support is added.
			 */
			rc = qed_iwarp_setup(p_hwfn, params);
			if (rc)
				return rc;
		} else {
			/* No need to register event callback for VF. */
			rc = qed_roce_setup(p_hwfn);
			if (rc)
				return rc;
		}
	}

	rdma_info->drv_ver = params->drv_ver ? params->drv_ver : QED_VERSION;

	return qed_rdma_start_fw(p_hwfn, p_ptt, params, rdma_info);
}

int
qed_rdma_stop_inner(struct qed_hwfn *p_hwfn, struct qed_rdma_info *rdma_info)
{
	struct rdma_close_func_ramrod_data *p_ramrod;
	int rc = -EBUSY;
	struct qed_sp_init_data init_data;
	struct qed_spq_entry *p_ent;
	struct qed_ptt *p_ptt;
	u32 ll2_ethertype_en;

	/* TODO
	 * For whole function, please check if any changes are needed to
	 * be done which are VF specific
	 * (like CNQ/SRQ start offset), apart from the ones currently done.
	 */
	if (!rdma_info->iov_info.is_vf) {
		rc = qed_rdma_deactivate(p_hwfn);
		if (rc)
			return rc;
	}

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt) {
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Failed to acquire PTT\n");
		return rc;
	}
#ifdef CONFIG_DCQCN
	qed_roce_stop_rl(p_hwfn);
#endif

	if (!rdma_info->iov_info.is_vf) {
		struct qed_hw_sriov_info *p_iov = p_hwfn->cdev->p_iov_info;

		/* Disable RoCE search is case no VFs are enabled */
		if (!p_iov || !p_iov->num_vfs) {
			qed_wr(p_hwfn, p_ptt, p_hwfn->rdma_prs_search_reg, 0);
			p_hwfn->b_rdma_enabled_in_prs = false;
		}

		qed_wr(p_hwfn, p_ptt, PRS_REG_ROCE_DEST_QP_MAX_PF, 0);

		/* Setting PRS_REG_ROCE_DEST_QP_MAX_VF to 0 can casue troubles
		 * when working with VMs, e.g unloading PF RDMA driver while
		 * RDMA traffic is running on VMs will cause traffic termination
		 */

		qed_roce_pvrdma_stop(p_hwfn, p_ptt);

		ll2_ethertype_en = qed_rd(p_hwfn,
					  p_ptt, PRS_REG_LIGHT_L2_ETHERTYPE_EN);

		qed_wr(p_hwfn, p_ptt, PRS_REG_LIGHT_L2_ETHERTYPE_EN,
		       (ll2_ethertype_en & 0xFFFE));

#ifndef REAL_ASIC_ONLY
		/* INTERNAL: In CMT mode, re-initialize nig to direct packets
		 * to both engines for L2 performance, Roce requires all traffic
		 * to go just to engine 0.
		 */
		if (QED_IS_BB_A0(p_hwfn->cdev) && QED_IS_CMT(p_hwfn->cdev)) {
			DP_ERR(p_hwfn->cdev,
			       "On Everest 4 Big Bear Board revision A0 when RoCE driver is loaded L2 performance is sub-optimal (all traffic is routed to engine 0). For optimal L2 results either remove RoCE driver or use board revision B0\n");

			qed_wr(p_hwfn,
			       p_ptt,
			       NIG_REG_LLH_ENG_CLS_ENG_ID_TBL, 0x55555555);
			qed_wr(p_hwfn,
			       p_ptt,
			       NIG_REG_LLH_ENG_CLS_ENG_ID_TBL + 0x4,
			       0x55555555);
		}
#endif
	}

	if (IS_IWARP(p_hwfn)) {
		rc = qed_iwarp_stop(p_hwfn);
		if (rc) {
			qed_ptt_release(p_hwfn, p_ptt);
			return rc;
		}
	} else {
		rc = qed_roce_stop(p_hwfn, rdma_info);
		if (rc) {
			qed_ptt_release(p_hwfn, p_ptt);
			return rc;
		}
	}

	qed_ptt_release(p_hwfn, p_ptt);

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));

	init_data.opaque_fid = rdma_info->iov_info.opaque_fid;

	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	/* Stop RoCE */
	rc = qed_sp_init_request(p_hwfn, &p_ent, RDMA_RAMROD_FUNC_CLOSE,
				 rdma_info->proto, &init_data);
	if (rc)
		goto out;

	p_ramrod = &p_ent->ramrod.rdma_close_func;

	p_ramrod->num_cnqs = rdma_info->num_cnqs;
	p_ramrod->cnq_start_offset = qed_rdma_get_start_cnq(p_hwfn, rdma_info);

	if (rdma_info->iov_info.is_vf) {
		p_ramrod->vf_valid = 1;
		p_ramrod->vf_id = rdma_info->iov_info.abs_vf_id;
	}

	rc = qed_spq_post(p_hwfn, p_ent, NULL);

out:
	qed_rdma_free(p_hwfn, rdma_info);

	return rc;
}

int qed_rdma_stop(void *rdma_cxt)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	int rc = -EBUSY;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "RDMA stop\n");

	p_hwfn->p_rdma_info->drv_ver = 0;

	if (IS_VF(p_hwfn->cdev))
		rc = qed_vf_pf_rdma_stop(p_hwfn);
	else
		rc = qed_rdma_stop_inner(p_hwfn, p_hwfn->p_rdma_info);

	return rc;
}

int qed_rdma_add_user(void *rdma_cxt,
		      struct qed_rdma_add_user_out_params *out_params)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	struct qed_dpi_info *dpi_info = &p_hwfn->dpi_info;
	int rc = 0;
	u32 dpi_start_offset;
	u32 returned_id = 0;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Adding User\n");

	/* Allocate DPI - this is done for both PF and VF */
	spin_lock_bh(&p_hwfn->p_rdma_info->lock);
	rc = qed_rdma_bmap_alloc_id(p_hwfn, &p_hwfn->p_rdma_info->dpi_map,
				    &returned_id);
	spin_unlock_bh(&p_hwfn->p_rdma_info->lock);

	if (rc)
		DP_ERR(p_hwfn, "Failed in allocating dpi\n");

	out_params->dpi = (u16) returned_id;

	/* Calculate the corresponding DPI address */
	dpi_start_offset = dpi_info->dpi_start_offset;

	out_params->dpi_addr = (u64) (long unsigned int)
	    ((u8 __iomem *) p_hwfn->doorbells +
	     dpi_start_offset + ((out_params->dpi) * dpi_info->dpi_size));

	out_params->dpi_phys_addr = p_hwfn->db_phys_addr + dpi_start_offset +
	    out_params->dpi * dpi_info->dpi_size;

	out_params->dpi_size = dpi_info->dpi_size;
	out_params->wid_count = dpi_info->wid_count;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Adding user - done, rc = %d\n", rc);
	return rc;
}

struct qed_rdma_port *qed_rdma_query_port_inner(struct qed_hwfn *p_hwfn,
						struct qed_rdma_info *rdma_info)
{
	struct qed_rdma_port *p_port = rdma_info->port;

	/* Link may have changed... */
	if (rdma_info->iov_info.is_vf) {
		u8 rel_vf_id = rdma_info->iov_info.rel_vf_id;
		struct qed_bulletin_content *p_bulletin;
		struct qed_vf_info *p_vf;

		p_vf = qed_iov_get_vf_info(p_hwfn, rel_vf_id, true);
		if (!p_vf)
			return NULL;

		p_bulletin = p_vf->bulletin.p_virt;
		p_port->port_state =
		    p_bulletin->link_up ? QED_RDMA_PORT_UP : QED_RDMA_PORT_DOWN;
		p_port->link_speed = p_bulletin->speed;
	} else {
		struct qed_mcp_link_state *p_link_output;

		/* The link state is saved only for the leading hwfn */
		p_link_output =
		    &QED_LEADING_HWFN(p_hwfn->cdev)->mcp_info->link_output;

		p_port->port_state = p_link_output->link_up ? QED_RDMA_PORT_UP
		    : QED_RDMA_PORT_DOWN;
		p_port->link_speed = p_link_output->speed;
	}

	p_port->max_msg_size = RDMA_MAX_DATA_SIZE_IN_WQE;

	return p_port;
}

struct qed_rdma_port *qed_rdma_query_port(void *rdma_cxt)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "RDMA Query port\n");

	if (IS_VF(p_hwfn->cdev))
		return qed_vf_pf_rdma_query_port(p_hwfn);
	else
		return qed_rdma_query_port_inner(p_hwfn, p_hwfn->p_rdma_info);
}

struct qed_rdma_device *qed_rdma_query_device(void *rdma_cxt)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	int rc;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Query device\n");

	if (p_hwfn->p_rdma_info == NULL) {
		DP_NOTICE(p_hwfn,
			  "aborting qed_rdma_query_device since NULL ptr\n");
		return NULL;
	}

	if (IS_VF(p_hwfn->cdev)) {
		rc = qed_vf_pf_rdma_query_device(p_hwfn);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "Error querying device info from PF.\n");
			return NULL;
		}
	}

	/* Return struct with device parameters */
	return p_hwfn->p_rdma_info->dev;
}

void qed_rdma_free_tid_inner(struct qed_hwfn *p_hwfn,
			     u32 itid, struct qed_rdma_info *rdma_info)
{
	spin_lock_bh(&rdma_info->lock);
	qed_bmap_release_id(p_hwfn, &rdma_info->tid_map, itid);
	spin_unlock_bh(&rdma_info->lock);
}

void qed_rdma_free_tid(void *rdma_cxt, u32 itid)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "itid = %08x\n", itid);

	if (IS_VF(p_hwfn->cdev))
		qed_vf_pf_rdma_free_tid(p_hwfn, itid);
	else
		qed_rdma_free_tid_inner(p_hwfn, itid, p_hwfn->p_rdma_info);
}

#define USTORM_COMMON_QUEUE_CONS_GTT_OFFSET_VF(queue_zone_id) \
	(USTORM_QZONE_START(p_hwfn->cdev) +		      \
	 (queue_zone_id) * USTORM_QZONE_SIZE +		      \
	 offsetof(struct ustorm_queue_zone, common) +	      \
	 offsetof(struct common_queue_zone, ring_drv_data_consumer))

void qed_rdma_cnq_prod_update(void *rdma_cxt, u8 qz_offset, u16 prod)
{
	struct qed_hwfn *p_hwfn;
	u16 qz_num;
	u32 addr;

	p_hwfn = (struct qed_hwfn *)rdma_cxt;

	if (qz_offset > p_hwfn->p_rdma_info->max_queue_zones) {
		DP_NOTICE(p_hwfn,
			  "queue zone offset %d is too large (max is %d)\n",
			  qz_offset, p_hwfn->p_rdma_info->max_queue_zones);
		return;
	}

	if (IS_PF(p_hwfn->cdev)) {
		qz_num = p_hwfn->p_rdma_info->queue_zone_base + qz_offset;

		addr = GET_GTT_REG_ADDR(GTT_BAR0_MAP_REG_USDM_RAM,
					USTORM_COMMON_QUEUE_CONS, qz_num);
	} else {
		struct pfvf_rdma_acquire_resp_tlv *resp =
		    &p_hwfn->vf_iov_info->rdma_acquire_resp;

		qz_num = resp->hw_qid[qz_offset];

		addr = USTORM_COMMON_QUEUE_CONS_GTT_OFFSET_VF(qz_num);
	}

	REG_WR16(p_hwfn, addr, prod);

	/* keep prod updates ordered */
	wmb();
}

int qed_rdma_alloc_pd(void *rdma_cxt, u16 * pd)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	u32 returned_id;
	int rc;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Alloc PD\n");

	/* Allocates an unused protection domain */
	spin_lock_bh(&p_hwfn->p_rdma_info->lock);
	rc = qed_rdma_bmap_alloc_id(p_hwfn,
				    &p_hwfn->p_rdma_info->pd_map, &returned_id);
	spin_unlock_bh(&p_hwfn->p_rdma_info->lock);
	if (rc)
		DP_NOTICE(p_hwfn, "Failed in allocating pd id\n");

	*pd = (u16) returned_id;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Alloc PD - done, rc = %d\n", rc);
	return rc;
}

void qed_rdma_free_pd(void *rdma_cxt, u16 pd)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "pd = %08x\n", pd);

	/* Returns a previously allocated protection domain for reuse */
	spin_lock_bh(&p_hwfn->p_rdma_info->lock);
	qed_bmap_release_id(p_hwfn, &p_hwfn->p_rdma_info->pd_map, pd);
	spin_unlock_bh(&p_hwfn->p_rdma_info->lock);
}

static int qed_rdma_alloc_xrcd(void *rdma_cxt, u16 * xrcd_id)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	u32 returned_id;
	int rc;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Alloc XRCD\n");

	/* Allocates an unused XRC domain */
	spin_lock_bh(&p_hwfn->p_rdma_info->lock);
	rc = qed_rdma_bmap_alloc_id(p_hwfn,
				    &p_hwfn->p_rdma_info->xrcd_map,
				    &returned_id);
	spin_unlock_bh(&p_hwfn->p_rdma_info->lock);
	if (rc)
		DP_NOTICE(p_hwfn, "Failed in allocating xrcd id\n");

	*xrcd_id = (u16) returned_id;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Alloc XRCD - done, rc = %d\n", rc);
	return rc;
}

static void qed_rdma_free_xrcd(void *rdma_cxt, u16 xrcd_id)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "xrcd_id = %08x\n", xrcd_id);

	/* Returns a previously allocated protection domain for reuse */
	spin_lock_bh(&p_hwfn->p_rdma_info->lock);
	qed_bmap_release_id(p_hwfn, &p_hwfn->p_rdma_info->xrcd_map, xrcd_id);
	spin_unlock_bh(&p_hwfn->p_rdma_info->lock);
}

static enum qed_rdma_toggle_bit
qed_rdma_toggle_bit_create_resize_cq(struct qed_hwfn *p_hwfn,
				     u16 icid, struct qed_rdma_info *p_info)
{
	enum qed_rdma_toggle_bit toggle_bit;
	u8 vf_id = p_info->iov_info.abs_vf_id;
	u32 bmap_id;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "icid = %08x\n", icid);

	/* the function toggle the bit that is related to a given icid
	 * and returns the new toggle bit's value
	 */
	bmap_id = icid - qed_cxt_get_proto_cid_start(p_hwfn, p_info->proto,
						     vf_id);

	spin_lock_bh(&p_info->lock);
	toggle_bit = !test_and_change_bit(bmap_id, p_info->toggle_bits.bitmap);
	spin_unlock_bh(&p_info->lock);

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "QED_RDMA_TOGGLE_BIT_= %d\n",
		   toggle_bit);

	return toggle_bit;
}

int
qed_rdma_create_cq_inner(struct qed_hwfn *p_hwfn,
			 struct qed_rdma_create_cq_in_params *params,
			 u16 * icid, struct qed_rdma_info *p_info)
{
	struct rdma_create_cq_ramrod_data *p_ramrod;
	enum qed_rdma_toggle_bit toggle_bit;
	struct qed_sp_init_data init_data;
	struct qed_spq_entry *p_ent;
	int rc;
	u32 returned_id;

	/* Allocate icid */
	spin_lock_bh(&p_info->lock);
	rc = qed_rdma_bmap_alloc_id(p_hwfn, &p_info->cq_map, &returned_id);
	spin_unlock_bh(&p_info->lock);

	if (rc) {
		DP_NOTICE(p_hwfn, "Can't create CQ, rc = %d\n", rc);
		return rc;
	}

	*icid = (u16) (returned_id +
		       qed_cxt_get_proto_cid_start(p_hwfn,
						   p_info->proto,
						   p_info->iov_info.abs_vf_id));

	/* Check if icid requires a page allocation */
	rc = qed_cxt_dynamic_ilt_alloc(p_hwfn, QED_ELEM_CXT, *icid,
				       p_info->iov_info.rel_vf_id);

	if (rc)
		goto err;

	rc = qed_rdma_configure_prs(p_hwfn, p_info, *icid);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed to configure prs\n");
		goto err;
	}

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = *icid;

	init_data.opaque_fid = p_info->iov_info.opaque_fid;

	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	/* Send create CQ ramrod */
	rc = qed_sp_init_request(p_hwfn, &p_ent,
				 RDMA_RAMROD_CREATE_CQ,
				 p_info->proto, &init_data);
	if (rc)
		goto err;

	p_ramrod = &p_ent->ramrod.rdma_create_cq;

	p_ramrod->cq_handle.hi = cpu_to_le32(params->cq_handle_hi);
	p_ramrod->cq_handle.lo = cpu_to_le32(params->cq_handle_lo);
	p_ramrod->dpi = cpu_to_le16(params->dpi);
	p_ramrod->is_two_level_pbl = params->pbl_two_level;
	p_ramrod->max_cqes = cpu_to_le32(params->cq_size);
	DMA_REGPAIR_LE(p_ramrod->pbl_addr, params->pbl_ptr);
	p_ramrod->pbl_num_pages = cpu_to_le16(params->pbl_num_pages);
	p_ramrod->cnq_id = qed_rdma_get_start_cnq(p_hwfn, p_info) +
	    params->cnq_id;
	p_ramrod->int_timeout = params->int_timeout;
	/* INTERNAL: Two layer PBL is currently not supported, ignoring next line */
	/* INTERNAL: p_ramrod->pbl_log_page_size = params->pbl_page_size_log - 12; */

	/* toggle the bit for every resize or create cq for a given icid */
	toggle_bit = qed_rdma_toggle_bit_create_resize_cq(p_hwfn, *icid,
							  p_info);

	p_ramrod->toggle_bit = toggle_bit;

	rc = qed_spq_post(p_hwfn, p_ent, NULL);
	if (rc) {
		/* restore toggle bit */
		qed_rdma_toggle_bit_create_resize_cq(p_hwfn, *icid, p_info);
		goto err;
	}

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Created CQ, cid = %d rc = %d\n",
		   init_data.cid, rc);

	return rc;

err:
	/* release allocated icid */
	spin_lock_bh(&p_info->lock);
	qed_bmap_release_id(p_hwfn, &p_info->cq_map, returned_id);
	spin_unlock_bh(&p_info->lock);

	DP_NOTICE(p_hwfn, "Create CQ failed, rc = %d\n", rc);

	return rc;
}

int
qed_rdma_create_cq(void *rdma_cxt,
		   struct qed_rdma_create_cq_in_params *params, u16 * icid)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	int rc;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "cq_handle = %08x%08x\n",
		   params->cq_handle_hi, params->cq_handle_lo);

	if (IS_VF(p_hwfn->cdev))
		rc = qed_vf_pf_rdma_create_cq(p_hwfn, params, icid);
	else
		rc = qed_rdma_create_cq_inner(p_hwfn, params,
					      icid, p_hwfn->p_rdma_info);

	return rc;
}

int qed_rdma_destroy_cq_inner(struct qed_hwfn *p_hwfn,
			      struct qed_rdma_destroy_cq_in_params *in_params,
			      struct qed_rdma_destroy_cq_out_params *out_params,
			      struct qed_rdma_info *rdma_info)
{
	struct rdma_destroy_cq_output_params *p_ramrod_res;
	int rc = -ENOMEM;
	struct rdma_destroy_cq_ramrod_data *p_ramrod;
	struct qed_sp_init_data init_data;
	struct qed_spq_entry *p_ent;
	dma_addr_t ramrod_res_phys;

	p_ramrod_res =
	    (struct rdma_destroy_cq_output_params *)dma_alloc_coherent(&p_hwfn->
								       cdev->
								       pdev->
								       dev,
								       sizeof
								       (struct
									rdma_destroy_cq_output_params),
								       &ramrod_res_phys,
								       GFP_KERNEL);
	if (!p_ramrod_res) {
		DP_NOTICE(p_hwfn,
			  "qed destroy cq failed: cannot allocate memory (ramrod)\n");
		return rc;
	}

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = in_params->icid;

	init_data.opaque_fid = rdma_info->iov_info.opaque_fid;

	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	/* Send destroy CQ ramrod */
	rc = qed_sp_init_request(p_hwfn, &p_ent,
				 RDMA_RAMROD_DESTROY_CQ,
				 rdma_info->proto, &init_data);
	if (rc)
		goto err;

	p_ramrod = &p_ent->ramrod.rdma_destroy_cq;
	DMA_REGPAIR_LE(p_ramrod->output_params_addr, ramrod_res_phys);

	rc = qed_spq_post(p_hwfn, p_ent, NULL);
	if (rc)
		goto err;

	out_params->num_cq_notif = le16_to_cpu(p_ramrod_res->cnq_num);

	dma_free_coherent(&p_hwfn->cdev->pdev->dev,
			  sizeof(struct rdma_destroy_cq_output_params),
			  p_ramrod_res, ramrod_res_phys);

	/* Free icid */
	spin_lock_bh(&rdma_info->lock);

	qed_bmap_release_id(p_hwfn,
			    &rdma_info->cq_map,
			    (in_params->icid -
			     qed_cxt_get_proto_cid_start(p_hwfn,
							 rdma_info->proto,
							 rdma_info->iov_info.
							 abs_vf_id)));

	spin_unlock_bh(&rdma_info->lock);

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Destroyed CQ, rc = %d\n", rc);
	return rc;

err:	dma_free_coherent(&p_hwfn->cdev->pdev->dev,
			  sizeof(struct rdma_destroy_cq_output_params),
			  p_ramrod_res, ramrod_res_phys);

	return rc;
}

int qed_rdma_destroy_cq(void *rdma_cxt,
			struct qed_rdma_destroy_cq_in_params *in_params,
			struct qed_rdma_destroy_cq_out_params *out_params)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "icid = %08x\n", in_params->icid);

	if (IS_VF(p_hwfn->cdev))
		return qed_vf_pf_rdma_destroy_cq(p_hwfn, in_params, out_params);

	return qed_rdma_destroy_cq_inner(p_hwfn, in_params,
					 out_params, p_hwfn->p_rdma_info);
}

void qed_rdma_set_fw_mac(u16 * p_fw_mac, u8 * p_qed_mac)
{
	p_fw_mac[0] = cpu_to_le16((p_qed_mac[0] << 8) + p_qed_mac[1]);
	p_fw_mac[1] = cpu_to_le16((p_qed_mac[2] << 8) + p_qed_mac[3]);
	p_fw_mac[2] = cpu_to_le16((p_qed_mac[4] << 8) + p_qed_mac[5]);
}

int qed_rdma_query_qp_inner(struct qed_hwfn *p_hwfn,
			    struct qed_rdma_qp *qp,
			    struct qed_rdma_query_qp_out_params *out_params,
			    struct qed_rdma_info *rdma_info)
{
	int rc = 0;

	/* The following fields are filled in from qp and not FW as they can't
	 * be modified by FW
	 */
	out_params->mtu = qp->mtu;
	out_params->dest_qp = qp->dest_qp;
	out_params->incoming_atomic_en = qp->incoming_atomic_en;
	out_params->e2e_flow_control_en = qp->e2e_flow_control_en;
	out_params->incoming_rdma_read_en = qp->incoming_rdma_read_en;
	out_params->incoming_rdma_write_en = qp->incoming_rdma_write_en;
	out_params->dgid = qp->dgid;
	out_params->flow_label = qp->flow_label;
	out_params->hop_limit_ttl = qp->hop_limit_ttl;
	out_params->traffic_class_tos = qp->traffic_class_tos;
	out_params->timeout = qp->ack_timeout;
	out_params->rnr_retry = qp->rnr_retry_cnt;
	out_params->retry_cnt = qp->retry_cnt;
	out_params->min_rnr_nak_timer = qp->min_rnr_nak_timer;
	out_params->pkey_index = 0;
	out_params->max_rd_atomic = qp->max_rd_atomic_req;
	out_params->max_dest_rd_atomic = qp->max_rd_atomic_resp;
	out_params->sqd_async = qp->sqd_async;

	if (IS_IWARP(p_hwfn))
		qed_iwarp_query_qp(qp, out_params);
	else
		rc = qed_roce_query_qp(p_hwfn, qp, out_params, rdma_info);

	return rc;
}

int
qed_rdma_query_qp(void *rdma_cxt,
		  struct qed_rdma_qp *qp,
		  struct qed_rdma_query_qp_out_params *out_params)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	int rc = 0;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "icid = %08x\n", qp->icid);

	if (IS_VF(p_hwfn->cdev))
		rc = qed_vf_pf_rdma_query_qp(p_hwfn, qp, out_params);
	else
		rc = qed_rdma_query_qp_inner(p_hwfn, qp, out_params,
					     p_hwfn->p_rdma_info);

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Query QP, rc = %d\n", rc);

	return rc;
}

int qed_rdma_destroy_qp_inner(struct qed_hwfn *p_hwfn,
			      struct qed_rdma_qp *qp,
			      struct qed_rdma_destroy_qp_out_params *out_params,
			      struct qed_rdma_info *rdma_info)
{
	int rc = 0;

	if (IS_IWARP(p_hwfn))
		/* TODO : Handle iwarp later. */
		rc = qed_iwarp_destroy_qp(p_hwfn, qp, rdma_info);
	else
		rc = qed_roce_destroy_qp(p_hwfn, qp, out_params, rdma_info);

	return rc;
}

int qed_rdma_destroy_qp(void *rdma_cxt,
			struct qed_rdma_qp *qp,
			struct qed_rdma_destroy_qp_out_params *out_params)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	int rc = 0;

	if (!rdma_cxt || !qp) {
		if (p_hwfn)
			DP_ERR(p_hwfn,
			       "qed rdma destroy qp failed: invalid NULL input. rdma_cxt=%p, qp=%p\n",
			       rdma_cxt, qp);
		return -EINVAL;
	}

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "QP(0x%x)\n", qp->icid);

	if (IS_VF(p_hwfn->cdev))
		rc = qed_vf_pf_rdma_destroy_qp(p_hwfn, qp, out_params);
	else
		rc = qed_rdma_destroy_qp_inner(p_hwfn, qp, out_params,
					       p_hwfn->p_rdma_info);

	/* In case of RoCE QP, free IRQ/ORQ */
	if (IS_ROCE(p_hwfn)) {
		qed_roce_free_irq(p_hwfn, qp);
		qed_roce_free_orq(p_hwfn, qp);
	}

	/* Free the QP */
	kfree(qp);

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "qed_rdma_destroy_qp - rc = %d\n", rc);

	return rc;
}

struct qed_rdma_qp *qed_rdma_create_qp_inner(struct qed_hwfn *p_hwfn,
					     struct qed_rdma_create_qp_in_params
					     *in_params,
					     struct
					     qed_rdma_create_qp_out_params
					     *out_params,
					     struct qed_rdma_info *rdma_info)
{
	int rc = 0;
	struct qed_rdma_qp *qp;

	if (!in_params || !out_params || !rdma_info) {
		DP_ERR(p_hwfn->cdev,
		       "qed roce create qp failed due to NULL entry (in=%p, out=%p, roce_info=?\n",
		       in_params, out_params);
		return NULL;
	}

	if (!rdma_info->iov_info.is_vf) {
		/* Some sanity checks... */
		u8 max_stats_queues = rdma_info->dev->max_stats_queues;

		if (in_params->stats_queue >= max_stats_queues) {
			DP_ERR(p_hwfn->cdev,
			       "qed rdma create qp failed due to invalid statistics queue %d. maximum is %d\n",
			       in_params->stats_queue, max_stats_queues);
			return NULL;
		}
	}

	if (IS_IWARP(p_hwfn)) {
		if (in_params->sq_num_pages * sizeof(struct regpair) >
		    IWARP_SHARED_QUEUE_PAGE_SQ_PBL_MAX_SIZE) {
			DP_NOTICE(p_hwfn->cdev,
				  "Sq num pages: %d exceeds maximum\n",
				  in_params->sq_num_pages);
			return NULL;
		}

		if (in_params->rq_num_pages * sizeof(struct regpair) >
		    IWARP_SHARED_QUEUE_PAGE_RQ_PBL_MAX_SIZE) {
			DP_NOTICE(p_hwfn->cdev,
				  "Rq num pages: %d exceeds maximum\n",
				  in_params->rq_num_pages);
			return NULL;
		}
	}

	qp = kzalloc(sizeof(struct qed_rdma_qp), GFP_KERNEL);
	if (!qp) {
		DP_NOTICE(p_hwfn, "Failed to allocate qed_rdma_qp\n");
		return NULL;
	}

	qp->cur_state = QED_ROCE_QP_STATE_RESET;
#ifdef CONFIG_IWARP
	qp->iwarp_state = QED_IWARP_QP_STATE_IDLE;
#endif
	qp->qp_handle.hi = cpu_to_le32(in_params->qp_handle_hi);
	qp->qp_handle.lo = cpu_to_le32(in_params->qp_handle_lo);
	qp->qp_handle_async.hi = cpu_to_le32(in_params->qp_handle_async_hi);
	qp->qp_handle_async.lo = cpu_to_le32(in_params->qp_handle_async_lo);

	/* XRC TGT QP is associated SRQs alebit not specific ones. Hence we
	 * don't expect the user to configure it and we do it oureselves.
	 */
	if (in_params->qp_type == QED_RDMA_QP_TYPE_XRC_TGT)
		qp->use_srq = 1;
	else
		qp->use_srq = in_params->use_srq;

	qp->signal_all = in_params->signal_all;
	qp->fmr_and_reserved_lkey = in_params->fmr_and_reserved_lkey;
	qp->pd = in_params->pd;
	qp->dpi = in_params->dpi;
	qp->sq_cq_id = in_params->sq_cq_id;
	qp->sq_num_pages = in_params->sq_num_pages;
	qp->sq_pbl_ptr = in_params->sq_pbl_ptr;
	qp->rq_cq_id = in_params->rq_cq_id;
	qp->rq_num_pages = in_params->rq_num_pages;
	qp->rq_pbl_ptr = in_params->rq_pbl_ptr;
	qp->srq_id = in_params->srq_id;
	qp->req_offloaded = false;
	qp->resp_offloaded = false;
	/* e2e_flow_control cannot be done in case of S-RQ.
	 * Refer to 9.7.7.2 End-to-End Flow Control section of IB spec
	 */
	qp->e2e_flow_control_en = qp->use_srq ? false : true;
	qp->stats_queue = !rdma_info->iov_info.is_vf ?
	    in_params->stats_queue :
	    rdma_info->iov_info.abs_vf_id + MAX_NUM_PFS;
	qp->qp_type = in_params->qp_type;
	qp->xrcd_id = in_params->xrcd_id;

	if (GET_FIELD(in_params->create_flags, QED_QP_PQSET)) {
		qp->pq_set_id = in_params->pq_set_id;
		qp->tc = in_params->tc;
	}

	if (IS_IWARP(p_hwfn)) {
		rc = qed_iwarp_create_qp(p_hwfn, qp, out_params, rdma_info);
		qp->qpid = qp->icid;
	} else {
		if (GET_FIELD(in_params->create_flags, QED_ROCE_EDPM_MODE_V1))
			qp->edpm_mode = 1;
		rc = qed_roce_alloc_qp_idx(p_hwfn, &qp->qp_idx, rdma_info);
		qp->icid = qed_roce_qp_idx_to_icid(p_hwfn,
						   qp->qp_idx,
						   rdma_info->
						   iov_info.abs_vf_id);
		qp->qpid = ((rdma_info->iov_info.abs_vf_id << 16) | qp->icid);
	}

	if (rc) {
		kfree(qp);
		return NULL;
	}

	out_params->icid = qp->icid;
	out_params->qp_id = qp->qpid;

	qed_roce_pvrdma_create_qp(p_hwfn, in_params, qp);

	/* INTERNAL: max_sq_sges future use only */

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Create QP, rc = %d\n", rc);
	return qp;
}

struct qed_rdma_qp *qed_rdma_create_qp(void *rdma_cxt,
				       struct qed_rdma_create_qp_in_params
				       *in_params,
				       struct qed_rdma_create_qp_out_params
				       *out_params)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;

	if (IS_VF(p_hwfn->cdev))
		return qed_vf_pf_rdma_create_qp(p_hwfn, in_params, out_params);
	else
		return qed_rdma_create_qp_inner(p_hwfn, in_params,
						out_params,
						p_hwfn->p_rdma_info);
}

int qed_rdma_modify_qp_inner(struct qed_hwfn *p_hwfn,
			     struct qed_rdma_qp *qp,
			     struct qed_rdma_modify_qp_in_params *params,
			     struct qed_rdma_info *rdma_info)
{
	int rc = 0;
	enum qed_roce_qp_state prev_state;

	if (GET_FIELD(params->modify_flags,
		      QED_RDMA_MODIFY_QP_VALID_RDMA_OPS_EN)) {
		qp->incoming_rdma_read_en = params->incoming_rdma_read_en;
		qp->incoming_rdma_write_en = params->incoming_rdma_write_en;
		qp->incoming_atomic_en = params->incoming_atomic_en;
	}

	/* Update QP structure with the updated values */
	if (GET_FIELD(params->modify_flags, QED_ROCE_MODIFY_QP_VALID_ROCE_MODE))
		qp->roce_mode = params->roce_mode;

	if (GET_FIELD(params->modify_flags, QED_ROCE_MODIFY_QP_VALID_PKEY))
		qp->pkey = params->pkey;

	if (GET_FIELD(params->modify_flags,
		      QED_ROCE_MODIFY_QP_VALID_E2E_FLOW_CONTROL_EN))
		qp->e2e_flow_control_en = params->e2e_flow_control_en;

	if (GET_FIELD(params->modify_flags, QED_ROCE_MODIFY_QP_VALID_DEST_QP))
		qp->dest_qp = params->dest_qp;

	if (GET_FIELD(params->modify_flags,
		      QED_ROCE_MODIFY_QP_VALID_ADDRESS_VECTOR)) {
		/* Indicates that the following parameters have changed:
		 * Traffic class, flow label, hop limit, source GID,
		 * destination GID, loopback indicator
		 */
		qp->flow_label = params->flow_label;
		qp->hop_limit_ttl = params->hop_limit_ttl;

		qp->sgid = params->sgid;
		qp->dgid = params->dgid;
		qp->udp_src_port = params->udp_src_port;
		qp->vlan_id = params->vlan_id;
		qp->traffic_class_tos = params->traffic_class_tos;
		qp->force_lb = params->force_lb;

		/* apply global override values */
		if (p_hwfn->p_rdma_info->glob_cfg.vlan_pri_en)
			SET_FIELD(qp->vlan_id, QED_VLAN_PRIO,
				  p_hwfn->p_rdma_info->glob_cfg.vlan_pri);

		if (p_hwfn->p_rdma_info->glob_cfg.ecn_en)
			SET_FIELD(qp->traffic_class_tos, QED_TOS_ECN,
				  p_hwfn->p_rdma_info->glob_cfg.ecn);

		if (p_hwfn->p_rdma_info->glob_cfg.dscp_en)
			SET_FIELD(qp->traffic_class_tos, QED_TOS_DSCP,
				  p_hwfn->p_rdma_info->glob_cfg.dscp);

		qp->mtu = params->mtu;

		memcpy((u8 *) & qp->remote_mac_addr[0],
		       (u8 *) & params->remote_mac_addr[0], ETH_ALEN);
		if (params->use_local_mac) {
			memcpy(&qp->local_mac_addr[0],
			       &params->local_mac_addr[0], ETH_ALEN);
		} else {
			u8 *mac_addr, zero_mac[ETH_ALEN] = { 0 };

			if (rdma_info->iov_info.is_vf) {
				struct qed_vf_info *p_vf =
				    rdma_info->iov_info.p_vf;

				mac_addr = (u8 *) & p_vf->bulletin.p_virt->mac;
			} else {
				mac_addr = (u8 *) & p_hwfn->hw_info.hw_mac_addr;
			}

			if (!memcmp(mac_addr, &zero_mac[0], ETH_ALEN))
				DP_NOTICE(p_hwfn->cdev,
					  "qp %u has a zero local_mac_addr\n",
					  qp->qp_idx);

			memcpy(&qp->local_mac_addr[0], mac_addr, ETH_ALEN);
		}
	}

	if (GET_FIELD(params->modify_flags, QED_ROCE_MODIFY_QP_VALID_RQ_PSN))
		qp->rq_psn = params->rq_psn;

	if (GET_FIELD(params->modify_flags, QED_ROCE_MODIFY_QP_VALID_SQ_PSN))
		qp->sq_psn = params->sq_psn;

	if (GET_FIELD(params->modify_flags,
		      QED_RDMA_MODIFY_QP_VALID_MAX_RD_ATOMIC_REQ))
		qp->max_rd_atomic_req = params->max_rd_atomic_req;

	if (GET_FIELD(params->modify_flags,
		      QED_RDMA_MODIFY_QP_VALID_MAX_RD_ATOMIC_RESP))
		qp->max_rd_atomic_resp = params->max_rd_atomic_resp;

	if (GET_FIELD(params->modify_flags,
		      QED_ROCE_MODIFY_QP_VALID_ACK_TIMEOUT))
		qp->ack_timeout = params->ack_timeout;

	if (GET_FIELD(params->modify_flags, QED_ROCE_MODIFY_QP_VALID_RETRY_CNT))
		qp->retry_cnt = params->retry_cnt;

	if (GET_FIELD(params->modify_flags,
		      QED_ROCE_MODIFY_QP_VALID_RNR_RETRY_CNT))
		qp->rnr_retry_cnt = params->rnr_retry_cnt;

	if (GET_FIELD(params->modify_flags,
		      QED_ROCE_MODIFY_QP_VALID_MIN_RNR_NAK_TIMER))
		qp->min_rnr_nak_timer = params->min_rnr_nak_timer;

	qp->sqd_async = params->sqd_async;

	prev_state = qp->cur_state;
	if (GET_FIELD(params->modify_flags, QED_RDMA_MODIFY_QP_VALID_NEW_STATE)) {
		qp->cur_state = params->new_state;
		DP_VERBOSE(p_hwfn,
			   QED_MSG_RDMA, "qp->cur_state=%d\n", qp->cur_state);
	}

	if (qp->qp_type == QED_RDMA_QP_TYPE_XRC_INI) {
		qp->has_req = 1;
	} else if (qp->qp_type == QED_RDMA_QP_TYPE_XRC_TGT) {
		qp->has_resp = 1;
	} else {
		qp->has_req = 1;
		qp->has_resp = 1;
	}

	if (IS_IWARP(p_hwfn)) {
		enum qed_iwarp_qp_state new_state =
		    qed_roce2iwarp_state(qp->cur_state);
		bool pq_update = false;

		if (GET_FIELD(params->modify_flags,
			      QED_RDMA_MODIFY_QP_VALID_PHYSICAL_QUEUES_FLG))
			pq_update = true;

		rc = qed_iwarp_modify_qp(p_hwfn, qp, new_state, 0,
					 pq_update, rdma_info);
	} else {
		rc = qed_roce_modify_qp(p_hwfn, qp, prev_state,
					params, rdma_info);
	}

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Modify QP, rc = %d\n", rc);

	return rc;
}

int qed_rdma_modify_qp(void *rdma_cxt,
		       struct qed_rdma_qp *qp,
		       struct qed_rdma_modify_qp_in_params *params)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	int rc;

	if (IS_ROCE(p_hwfn)) {
		rc = qed_roce_alloc_irq(p_hwfn, qp);
		if (rc)
			goto exit;

		rc = qed_roce_alloc_orq(p_hwfn, qp);
		if (rc)
			goto err2;
	}

	if (IS_VF(p_hwfn->cdev))
		rc = qed_vf_pf_rdma_modify_qp(p_hwfn, qp, params);
	else
		rc = qed_rdma_modify_qp_inner(p_hwfn, qp, params,
					      p_hwfn->p_rdma_info);

	if (IS_ROCE(p_hwfn) && rc != 0)
		goto err1;

	goto exit;
err1:
	qed_roce_free_orq(p_hwfn, qp);
err2:
	qed_roce_free_irq(p_hwfn, qp);
exit:
	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "qed_rdma_modify_qp - rc = %d\n", rc);

	return rc;
}

int qed_rdma_register_tid_inner(struct qed_hwfn *p_hwfn,
				struct qed_rdma_register_tid_in_params *params,
				struct qed_rdma_info *rdma_info)
{
	u8 fw_return_code;
	struct qed_sp_init_data init_data;
	enum rdma_tid_type tid_type;
	struct rdma_register_tid_ramrod_data *p_ramrod;
	struct qed_spq_entry *p_ent;
	int rc;

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));

	init_data.opaque_fid = rdma_info->iov_info.opaque_fid;

	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	rc = qed_sp_init_request(p_hwfn, &p_ent, RDMA_RAMROD_REGISTER_MR,
				 p_hwfn->p_rdma_info->proto, &init_data);
	if (rc) {
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "rc = %d\n", rc);
		return rc;
	}

	if (rdma_info->last_tid < params->itid)
		rdma_info->last_tid = params->itid;

	p_ramrod = &p_ent->ramrod.rdma_register_tid;

	p_ramrod->flags = 0;
	SET_FIELD(p_ramrod->flags,
		  RDMA_REGISTER_TID_RAMROD_DATA_TWO_LEVEL_PBL,
		  params->pbl_two_level);

	SET_FIELD(p_ramrod->flags,
		  RDMA_REGISTER_TID_RAMROD_DATA_ZERO_BASED, params->zbva);

	SET_FIELD(p_ramrod->flags,
		  RDMA_REGISTER_TID_RAMROD_DATA_PHY_MR, params->phy_mr);

	/* Don't initialize D/C field, as it may override other bits. */
	if (!(params->tid_type == QED_RDMA_TID_FMR) && !(params->dma_mr))
		SET_FIELD(p_ramrod->flags,
			  RDMA_REGISTER_TID_RAMROD_DATA_PAGE_SIZE_LOG,
			  params->page_size_log - 12);

	SET_FIELD(p_ramrod->flags,
		  RDMA_REGISTER_TID_RAMROD_DATA_REMOTE_READ,
		  params->remote_read);

	SET_FIELD(p_ramrod->flags,
		  RDMA_REGISTER_TID_RAMROD_DATA_REMOTE_WRITE,
		  params->remote_write);

	SET_FIELD(p_ramrod->flags,
		  RDMA_REGISTER_TID_RAMROD_DATA_REMOTE_ATOMIC,
		  params->remote_atomic);

	SET_FIELD(p_ramrod->flags,
		  RDMA_REGISTER_TID_RAMROD_DATA_LOCAL_WRITE,
		  params->local_write);

	SET_FIELD(p_ramrod->flags,
		  RDMA_REGISTER_TID_RAMROD_DATA_LOCAL_READ, params->local_read);

	SET_FIELD(p_ramrod->flags,
		  RDMA_REGISTER_TID_RAMROD_DATA_ENABLE_MW_BIND,
		  params->mw_bind);

	SET_FIELD(p_ramrod->flags1,
		  RDMA_REGISTER_TID_RAMROD_DATA_PBL_PAGE_SIZE_LOG,
		  params->pbl_page_size_log - 12);

	SET_FIELD(p_ramrod->flags2,
		  RDMA_REGISTER_TID_RAMROD_DATA_DMA_MR, params->dma_mr);

	switch (params->tid_type) {
	case QED_RDMA_TID_REGISTERED_MR:
		tid_type = RDMA_TID_REGISTERED_MR;
		break;
	case QED_RDMA_TID_FMR:
		tid_type = RDMA_TID_FMR;
		break;
	case QED_RDMA_TID_MW:
		tid_type = RDMA_TID_MW;
		break;
	default:
		rc = -EINVAL;
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "rc = %d\n", rc);
		qed_sp_destroy_request(p_hwfn, p_ent);
		return rc;
	}
	SET_FIELD(p_ramrod->flags1,
		  RDMA_REGISTER_TID_RAMROD_DATA_TID_TYPE, tid_type);

	p_ramrod->itid = cpu_to_le32(params->itid);
	p_ramrod->key = params->key;
	p_ramrod->pd = cpu_to_le16(params->pd);
	p_ramrod->length_hi = (u8) (params->length >> 32);
	p_ramrod->length_lo = DMA_LO_LE(params->length);
	if (params->zbva) {
		/* Lower 32 bits of the registered MR address.
		 * In case of zero based MR, will hold FBO
		 */
		p_ramrod->va.hi = 0;
		p_ramrod->va.lo = cpu_to_le32(params->fbo);
	} else {
		DMA_REGPAIR_LE(p_ramrod->va, params->vaddr);
	}

	DMA_REGPAIR_LE(p_ramrod->pbl_base, params->pbl_ptr);

	/* DIF */
	if (params->dif_enabled) {
		SET_FIELD(p_ramrod->flags2,
			  RDMA_REGISTER_TID_RAMROD_DATA_DIF_ON_HOST_FLG, 1);
		DMA_REGPAIR_LE(p_ramrod->dif_error_addr,
			       params->dif_error_addr);
	}

	if (rdma_info->iov_info.is_vf) {
		p_ramrod->vf_valid = 1;
		p_ramrod->vf_id = rdma_info->iov_info.abs_vf_id;
	}

	rc = qed_spq_post(p_hwfn, p_ent, &fw_return_code);
	if (rc)
		return rc;

	if (fw_return_code != RDMA_RETURN_OK) {
		DP_NOTICE(p_hwfn, "fw_return_code = %d\n", fw_return_code);
		return -EINVAL;
	}

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Register TID, rc = %d\n", rc);
	return rc;
}

int qed_rdma_register_tid(void *rdma_cxt,
			  struct qed_rdma_register_tid_in_params *params)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "itid = %08x\n", params->itid);

	if (IS_VF(p_hwfn->cdev))
		return qed_vf_pf_rdma_register_tid(p_hwfn, params);
	else
		return qed_rdma_register_tid_inner(p_hwfn, params,
						   p_hwfn->p_rdma_info);
}

static inline int qed_rdma_send_deregister_tid_ramrod(struct qed_hwfn *p_hwfn,
						      u32 itid,
						      u8 * fw_return_code,
						      struct qed_rdma_info
						      *rdma_info)
{
	struct qed_sp_init_data init_data;
	struct rdma_deregister_tid_ramrod_data *p_ramrod;
	struct qed_spq_entry *p_ent;
	int rc;

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));

	init_data.opaque_fid = rdma_info->iov_info.opaque_fid;

	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	rc = qed_sp_init_request(p_hwfn, &p_ent,
				 RDMA_RAMROD_DEREGISTER_MR,
				 p_hwfn->p_rdma_info->proto, &init_data);
	if (rc) {
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "rc = %d\n", rc);
		return rc;
	}

	p_ramrod = &p_ent->ramrod.rdma_deregister_tid;
	p_ramrod->itid = cpu_to_le32(itid);

	rc = qed_spq_post(p_hwfn, p_ent, fw_return_code);
	if (rc) {
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "rc = %d\n", rc);
		return rc;
	}

	return rc;
}

#define QED_RDMA_DEREGISTER_TIMEOUT_MSEC        (1)

int
qed_rdma_deregister_tid_inner(struct qed_hwfn *p_hwfn,
			      u32 itid, struct qed_rdma_info *rdma_info)
{
	u8 fw_ret_code;
	struct qed_ptt *p_ptt;
	int rc;

	/* First attempt */
	rc = qed_rdma_send_deregister_tid_ramrod(p_hwfn, itid,
						 &fw_ret_code, rdma_info);

	if (rc)
		return rc;

	if (fw_ret_code != RDMA_RETURN_NIG_DRAIN_REQ)
		goto done;

	/* Second attempt, after 1msec, if device still holds data.
	 * This can occur since 'destroy QP' returns to the caller rather fast.
	 * The synchronous part of it returns after freeing a few of the
	 * resources but not all of them, allowing the consumer to continue its
	 * flow. All of the resources will be freed after the asynchronous part
	 * of the destroy QP is complete.
	 */
	msleep(QED_RDMA_DEREGISTER_TIMEOUT_MSEC);
	rc = qed_rdma_send_deregister_tid_ramrod(p_hwfn, itid,
						 &fw_ret_code, rdma_info);
	if (rc)
		return rc;

	if (fw_ret_code != RDMA_RETURN_NIG_DRAIN_REQ)
		goto done;

	/* Third and last attempt, perform NIG drain and resend the ramrod */
	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return -EBUSY;

	rc = qed_mcp_drain(p_hwfn, p_ptt);
	if (rc) {
		qed_ptt_release(p_hwfn, p_ptt);
		return rc;
	}

	qed_ptt_release(p_hwfn, p_ptt);

	rc = qed_rdma_send_deregister_tid_ramrod(p_hwfn, itid,
						 &fw_ret_code, rdma_info);
	if (rc)
		return rc;

done:
	if (fw_ret_code == RDMA_RETURN_OK) {
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "De-registered itid=%d\n",
			   itid);
		return 0;
	} else if (fw_ret_code == RDMA_RETURN_DEREGISTER_MR_BAD_STATE_ERR) {
		/* INTERNAL: This error is returned in case trying to deregister
		 * a MR that is not allocated. We define "allocated" as either:
		 * 1. Registered.
		 * 2. This is an FMR MR type, which is not currently registered
		 *    but can accept FMR WQEs on SQ.
		 */
		DP_NOTICE(p_hwfn, "itid=%d, fw_ret_code=%d\n", itid,
			  fw_ret_code);
		return -EINVAL;
	} else {		/* fw_ret_code == RDMA_RETURN_NIG_DRAIN_REQ */
		DP_NOTICE(p_hwfn,
			  "deregister failed after three attempts. itid=%d, fw_ret_code=%d\n",
			  itid, fw_ret_code);
		return -EINVAL;
	}

	return rc;
}

int qed_rdma_deregister_tid(void *rdma_cxt, u32 itid)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;

	if (IS_VF(p_hwfn->cdev))
		return qed_vf_pf_rdma_deregister_tid(p_hwfn, itid);
	else
		return qed_rdma_deregister_tid_inner(p_hwfn, itid,
						     p_hwfn->p_rdma_info);
}

static struct qed_bmap *qed_rdma_get_srq_bmap(struct qed_rdma_info *rdma_info,
					      bool is_xrc)
{
	if (is_xrc)
		return &rdma_info->xrc_srq_map;

	return &rdma_info->srq_map;
}

static u16 qed_rdma_get_srq_offset(struct qed_rdma_info *rdma_info, bool is_xrc)
{
	if (is_xrc)
		return rdma_info->xrc_id_offset;

	return rdma_info->srq_id_offset;
}

int
qed_rdma_modify_srq(void *rdma_cxt,
		    struct qed_rdma_modify_srq_in_params *in_params)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	int rc;

	if (IS_VF(p_hwfn->cdev))
		rc = qed_vf_pf_rdma_modify_srq(p_hwfn, in_params);
	else
		rc = qed_rdma_modify_srq_inner(p_hwfn, in_params,
					       p_hwfn->p_rdma_info);
	return rc;
}

int
qed_rdma_modify_srq_inner(struct qed_hwfn *p_hwfn,
			  struct qed_rdma_modify_srq_in_params *in_params,
			  struct qed_rdma_info *rdma_info)
{
	struct rdma_srq_modify_ramrod_data *p_ramrod;
	struct qed_sp_init_data init_data;
	struct qed_spq_entry *p_ent;
	u16 opaque_fid;
	int rc;

	memset(&init_data, 0, sizeof(init_data));
	opaque_fid = rdma_info->iov_info.opaque_fid;
	init_data.opaque_fid = opaque_fid;
	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;
	/* Send modify SRQ ramrod */
	rc = qed_sp_init_request(p_hwfn, &p_ent,
				 RDMA_RAMROD_MODIFY_SRQ,
				 rdma_info->proto, &init_data);
	if (rc)
		return rc;

	p_ramrod = &p_ent->ramrod.rdma_modify_srq;

	p_ramrod->srq_id.srq_idx = cpu_to_le16(in_params->srq_id);
	p_ramrod->srq_id.opaque_fid = cpu_to_le16(opaque_fid);
	p_ramrod->wqe_limit = cpu_to_le16(in_params->wqe_limit);

	rc = qed_spq_post(p_hwfn, p_ent, NULL);
	if (rc)
		return rc;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "modified SRQ id = %x, is_xrc=%u\n",
		   in_params->srq_id, in_params->is_xrc);

	return rc;
}

int
qed_rdma_destroy_srq(void *rdma_cxt,
		     struct qed_rdma_destroy_srq_in_params *in_params)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	int rc;

	if (IS_VF(p_hwfn->cdev))
		rc = qed_vf_pf_rdma_destroy_srq(p_hwfn, in_params);
	else
		rc = qed_rdma_destroy_srq_inner(p_hwfn, in_params,
						p_hwfn->p_rdma_info);
	return rc;
}

int
qed_rdma_destroy_srq_inner(struct qed_hwfn *p_hwfn,
			   struct qed_rdma_destroy_srq_in_params *in_params,
			   struct qed_rdma_info *rdma_info)
{
	struct rdma_srq_destroy_ramrod_data *p_ramrod;
	struct qed_sp_init_data init_data;
	struct qed_spq_entry *p_ent;
	struct qed_bmap *bmap;
	u16 opaque_fid;
	u16 offset;
	int rc;

	if (in_params->is_xrc && !rdma_info->xrc_supported) {
		DP_NOTICE(p_hwfn, "VF XRC-SRQ is not supported\n");
		return -EOPNOTSUPP;
	}

	opaque_fid = rdma_info->iov_info.opaque_fid;

	memset(&init_data, 0, sizeof(init_data));
	init_data.opaque_fid = opaque_fid;
	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	/* Send destroy SRQ ramrod */
	rc = qed_sp_init_request(p_hwfn, &p_ent,
				 RDMA_RAMROD_DESTROY_SRQ,
				 rdma_info->proto, &init_data);
	if (rc)
		return rc;

	p_ramrod = &p_ent->ramrod.rdma_destroy_srq;
	p_ramrod->srq_id.srq_idx = cpu_to_le16(in_params->srq_id);
	p_ramrod->srq_id.opaque_fid = cpu_to_le16(opaque_fid);

	rc = qed_spq_post(p_hwfn, p_ent, NULL);

	if (rc)
		return rc;

	bmap = qed_rdma_get_srq_bmap(rdma_info, in_params->is_xrc);
	offset = qed_rdma_get_srq_offset(rdma_info, in_params->is_xrc);

	spin_lock_bh(&rdma_info->lock);
	qed_bmap_release_id(p_hwfn, bmap, in_params->srq_id - offset);
	spin_unlock_bh(&rdma_info->lock);

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
		   "XRC/SRQ destroyed Id = %x, is_xrc=%u\n",
		   in_params->srq_id, in_params->is_xrc);

	return rc;
}

int qed_rdma_create_srq(void *rdma_cxt,
			struct qed_rdma_create_srq_in_params *in_params,
			struct qed_rdma_create_srq_out_params *out_params)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	int rc;

	if (IS_VF(p_hwfn->cdev))
		rc = qed_vf_pf_rdma_create_srq(p_hwfn, in_params, out_params);
	else
		rc = qed_rdma_create_srq_inner(p_hwfn, in_params,
					       out_params, p_hwfn->p_rdma_info);
	return rc;
}

int
qed_rdma_create_srq_inner(struct qed_hwfn *p_hwfn,
			  struct qed_rdma_create_srq_in_params *in_params,
			  struct qed_rdma_create_srq_out_params *out_params,
			  struct qed_rdma_info *rdma_info)
{
	struct rdma_srq_create_ramrod_data *p_ramrod;
	struct qed_sp_init_data init_data;
	enum qed_cxt_elem_type elem_type;
	struct qed_spq_entry *p_ent;
	u16 opaque_fid, srq_id;
	struct qed_bmap *bmap;
	u32 returned_id;
	u16 offset;
	int rc;

	if (in_params->is_xrc && !rdma_info->xrc_supported) {
		DP_NOTICE(p_hwfn, "VF XRC-SRQ is not supported\n");
		return -EOPNOTSUPP;
	}

	/* Allocate XRC/SRQ ID */
	bmap = qed_rdma_get_srq_bmap(rdma_info, in_params->is_xrc);
	spin_lock_bh(&rdma_info->lock);
	rc = qed_rdma_bmap_alloc_id(p_hwfn, bmap, &returned_id);
	spin_unlock_bh(&rdma_info->lock);

	if (rc) {
		DP_NOTICE(p_hwfn,
			  "failed to allocate xrc/srq id (is_xrc=%u)\n",
			  in_params->is_xrc);
		return rc;
	}

	/* Allocate XRC/SRQ ILT page */
	offset = qed_rdma_get_srq_offset(rdma_info, in_params->is_xrc);
	srq_id = returned_id + offset;
	elem_type = (in_params->is_xrc) ? (QED_ELEM_XRC_SRQ) : (QED_ELEM_SRQ);
	rc = qed_cxt_dynamic_ilt_alloc(p_hwfn,
				       elem_type,
				       srq_id, rdma_info->iov_info.rel_vf_id);

	if (rc)
		goto err;

	memset(&init_data, 0, sizeof(init_data));
	opaque_fid = rdma_info->iov_info.opaque_fid;
	init_data.opaque_fid = opaque_fid;
	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	/* Create XRC/SRQ ramrod */
	rc = qed_sp_init_request(p_hwfn, &p_ent,
				 RDMA_RAMROD_CREATE_SRQ,
				 rdma_info->proto, &init_data);
	if (rc)
		goto err;

	p_ramrod = &p_ent->ramrod.rdma_create_srq;
	p_ramrod->pbl_base_addr.hi = DMA_HI_LE(in_params->pbl_base_addr);
	p_ramrod->pbl_base_addr.lo = DMA_LO_LE(in_params->pbl_base_addr);
	p_ramrod->pages_in_srq_pbl = cpu_to_le16(in_params->num_pages);
	p_ramrod->pd_id = cpu_to_le16(in_params->pd_id);
	p_ramrod->srq_id.opaque_fid = cpu_to_le16(opaque_fid);
	p_ramrod->page_size = cpu_to_le16(in_params->page_size);
	p_ramrod->producers_addr.hi = DMA_HI_LE(in_params->prod_pair_addr);
	p_ramrod->producers_addr.lo = DMA_LO_LE(in_params->prod_pair_addr);
	p_ramrod->srq_id.srq_idx = cpu_to_le16(srq_id);

	if (in_params->is_xrc) {
		SET_FIELD(p_ramrod->flags,
			  RDMA_SRQ_CREATE_RAMROD_DATA_XRC_FLAG, 1);
		SET_FIELD(p_ramrod->flags,
			  RDMA_SRQ_CREATE_RAMROD_DATA_RESERVED_KEY_EN,
			  in_params->reserved_key_en);
		p_ramrod->xrc_srq_cq_cid =
		    cpu_to_le32((opaque_fid << 16) | in_params->cq_cid);
		p_ramrod->xrc_domain = cpu_to_le16(in_params->xrcd_id);
	}

	rc = qed_spq_post(p_hwfn, p_ent, NULL);

	if (rc)
		goto err;

	out_params->srq_id = srq_id;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_RDMA,
		   "XRC/SRQ created Id = %x (is_xrc=%u)\n",
		   out_params->srq_id, in_params->is_xrc);
	return rc;

err:
	spin_lock_bh(&rdma_info->lock);
	qed_bmap_release_id(p_hwfn, bmap, returned_id);
	spin_unlock_bh(&rdma_info->lock);

	return rc;
}

bool qed_rdma_allocated_qps(struct qed_hwfn * p_hwfn)
{
	bool result;

	/* if rdma info has not been allocated, naturally there are no qps */
	if (!p_hwfn->p_rdma_info)
		return false;

	spin_lock_bh(&p_hwfn->p_rdma_info->lock);
	if (!p_hwfn->p_rdma_info->qp_map.bitmap)
		result = false;
	else
		result = !qed_bmap_is_empty(&p_hwfn->p_rdma_info->qp_map);
	spin_unlock_bh(&p_hwfn->p_rdma_info->lock);
	return result;
}

int qed_rdma_resize_cq_inner(struct qed_hwfn *p_hwfn,
			     struct qed_rdma_resize_cq_in_params *in_params,
			     struct qed_rdma_resize_cq_out_params *out_params,
			     struct qed_rdma_info *rdma_info)
{
	u8 fw_return_code;
	dma_addr_t ramrod_res_phys;
	struct rdma_resize_cq_output_params *p_ramrod_res;
	struct rdma_resize_cq_ramrod_data *p_ramrod;
	enum qed_rdma_toggle_bit toggle_bit;
	struct qed_sp_init_data init_data;
	struct qed_spq_entry *p_ent;
	int rc;

	/* Send resize CQ ramrod */
	p_ramrod_res =
	    (struct rdma_resize_cq_output_params *)dma_alloc_coherent(&p_hwfn->
								      cdev->
								      pdev->dev,
								      sizeof
								      (*p_ramrod_res),
								      &ramrod_res_phys,
								      GFP_KERNEL);
	if (!p_ramrod_res) {
		rc = -ENOMEM;
		DP_NOTICE(p_hwfn,
			  "qed resize cq failed: cannot allocate memory (ramrod). rc = %d\n",
			  rc);
		return rc;
	}

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = in_params->icid;

	init_data.opaque_fid = rdma_info->iov_info.opaque_fid;

	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	rc = qed_sp_init_request(p_hwfn, &p_ent,
				 RDMA_RAMROD_RESIZE_CQ,
				 p_hwfn->p_rdma_info->proto, &init_data);
	if (rc)
		goto err;

	p_ramrod = &p_ent->ramrod.rdma_resize_cq;

	p_ramrod->flags = 0;

	/* toggle the bit for every resize or create cq for a given icid */
	toggle_bit = qed_rdma_toggle_bit_create_resize_cq(p_hwfn,
							  in_params->icid,
							  rdma_info);

	SET_FIELD(p_ramrod->flags,
		  RDMA_RESIZE_CQ_RAMROD_DATA_TOGGLE_BIT, toggle_bit);

	SET_FIELD(p_ramrod->flags,
		  RDMA_RESIZE_CQ_RAMROD_DATA_IS_TWO_LEVEL_PBL,
		  in_params->pbl_two_level);

	p_ramrod->pbl_log_page_size = in_params->pbl_page_size_log - 12;
	p_ramrod->pbl_num_pages = cpu_to_le16(in_params->pbl_num_pages);
	p_ramrod->max_cqes = cpu_to_le32(in_params->cq_size);
	p_ramrod->pbl_addr.hi = DMA_HI_LE(in_params->pbl_ptr);
	p_ramrod->pbl_addr.lo = DMA_LO_LE(in_params->pbl_ptr);

	p_ramrod->output_params_addr.hi = DMA_HI_LE(ramrod_res_phys);
	p_ramrod->output_params_addr.lo = DMA_LO_LE(ramrod_res_phys);

	rc = qed_spq_post(p_hwfn, p_ent, &fw_return_code);
	if (rc)
		goto err;

	if (fw_return_code != RDMA_RETURN_OK) {
		DP_NOTICE(p_hwfn, "fw_return_code = %d\n", fw_return_code);
		rc = -EINVAL;
		goto err;
	}

	out_params->prod = le32_to_cpu(p_ramrod_res->old_cq_prod);
	out_params->cons = le32_to_cpu(p_ramrod_res->old_cq_cons);

	dma_free_coherent(&p_hwfn->cdev->pdev->dev,
			  sizeof(*p_ramrod_res), p_ramrod_res, ramrod_res_phys);

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "rc = %d\n", rc);

	return rc;

err:	dma_free_coherent(&p_hwfn->cdev->pdev->dev,
			  sizeof(*p_ramrod_res),
			  p_ramrod_res, ramrod_res_phys);
	DP_NOTICE(p_hwfn, "rc = %d\n", rc);

	return rc;
}

int qed_rdma_resize_cq(void *rdma_cxt,
		       struct qed_rdma_resize_cq_in_params *in_params,
		       struct qed_rdma_resize_cq_out_params *out_params)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "icid = %08x\n", in_params->icid);

	if (IS_VF(p_hwfn->cdev))
		return qed_vf_pf_rdma_resize_cq(p_hwfn, in_params, out_params);
	else
		return qed_rdma_resize_cq_inner(p_hwfn, in_params,
						out_params,
						p_hwfn->p_rdma_info);
}

int qed_rdma_start_inner(struct qed_hwfn *p_hwfn,
			 struct qed_rdma_start_in_params *params,
			 struct qed_rdma_info *rdma_info)
{
	int rc = -EBUSY;
	struct qed_ptt *p_ptt;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
		   "desired_cnq = %08x\n", params->desired_cnq);

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		goto err;

	rc = qed_rdma_alloc(p_hwfn, rdma_info);
	if (rc)
		goto err1;

	rc = qed_rdma_setup(p_hwfn, p_ptt, params, rdma_info);
	if (rc)
		goto err1;

	qed_ptt_release(p_hwfn, p_ptt);

	if (!rdma_info->iov_info.is_vf)
		qed_rdma_activate(rdma_info);

	return rc;

err1:
	qed_ptt_release(p_hwfn, p_ptt);
	qed_rdma_free(p_hwfn, rdma_info);
err:
	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "RDMA start - error, rc = %d\n", rc);
	return rc;
}

int qed_rdma_start(void *rdma_cxt, struct qed_rdma_start_in_params *params)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	int rc = -EBUSY;

	if (IS_VF(p_hwfn->cdev))
		rc = qed_vf_pf_rdma_start(p_hwfn, params, p_hwfn->p_rdma_info);
	else
		rc = qed_rdma_start_inner(p_hwfn, params, p_hwfn->p_rdma_info);

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "RDMA start - rc = %d\n", rc);
	return rc;
}

int qed_rdma_get_stats_queue(void *rdma_cxt, u8 * stats_queue)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;

	if (!p_hwfn)
		return -EINVAL;

	/* stats_queue is relevant only for PF */
	if (IS_VF(p_hwfn->cdev))
		return 0;

	*stats_queue = p_hwfn->abs_pf_id;

	return 0;
}

static void qed_rdma_vf_reset_stats(struct qed_hwfn *p_hwfn)
{
	struct qed_rdma_info *info;
	u32 addr;

	info = p_hwfn->p_rdma_info;

	addr = PXP_VF_BAR0_START_PSDM_ZONE_B +
	    offsetof(struct pstorm_vf_zone, non_trigger.rdma_stats);
	qed_memzero_hw(p_hwfn, NULL, addr, sizeof(info->rdma_sent_pstats));

	addr = PXP_VF_BAR0_START_TSDM_ZONE_B +
	    offsetof(struct tstorm_vf_zone, non_trigger.rdma_stats);
	qed_memzero_hw(p_hwfn, NULL, addr, sizeof(info->rdma_rcv_tstats));
}

int qed_rdma_reset_stats(void *rdma_cxt, u8 stats_queue)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	struct qed_rdma_info *info;
	struct qed_ptt *p_ptt;
	u8 max_stats_queues;
	u32 addr;

	int rc = 0;

	if (!p_hwfn)
		return -EINVAL;

	if (!p_hwfn->p_rdma_info) {
		DP_INFO(p_hwfn->cdev,
			"qed rdma query stats failed due to NULL rdma_info\n");
		return -EINVAL;
	}

	if (IS_VF(p_hwfn->cdev)) {
		qed_rdma_vf_reset_stats(p_hwfn);
		return 0;
	}

	info = p_hwfn->p_rdma_info;

	rc = qed_rdma_inc_ref_cnt(p_hwfn);
	if (rc)
		return rc;

	max_stats_queues = p_hwfn->p_rdma_info->dev->max_stats_queues;
	if (stats_queue >= max_stats_queues) {
		DP_ERR(p_hwfn->cdev,
		       "qed rdma query stats failed due to invalid statistics queue %d. maximum is %d\n",
		       stats_queue, max_stats_queues);
		rc = -EINVAL;
		goto err;
	}

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt) {
		rc = -EBUSY;
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "rc = %d\n", rc);
		goto err;
	}

	/* Reset Stats in HW */
	addr = BAR0_MAP_REG_PSDM_RAM +
	    PSTORM_RDMA_QUEUE_STAT_OFFSET(stats_queue);
	qed_memzero_hw(p_hwfn, p_ptt, addr, sizeof(info->rdma_sent_pstats));

	addr = BAR0_MAP_REG_TSDM_RAM +
	    TSTORM_RDMA_QUEUE_STAT_OFFSET(stats_queue);
	qed_memzero_hw(p_hwfn, p_ptt, addr, sizeof(info->rdma_rcv_tstats));

#ifdef CONFIG_IWARP
	addr = BAR0_MAP_REG_XSDM_RAM +
	    XSTORM_IWARP_RXMIT_STATS_OFFSET(stats_queue);
	qed_memzero_hw(p_hwfn, p_ptt, addr, sizeof(info->iwarp.stats));
#endif
	addr = BAR0_MAP_REG_TSDM_RAM +
	    TSTORM_ROCE_EVENTS_STAT_OFFSET(p_hwfn->rel_pf_id);
	qed_memzero_hw(p_hwfn, p_ptt, addr, sizeof(info->roce.event_stats));

	addr = BAR0_MAP_REG_YSDM_RAM +
	    YSTORM_ROCE_DCQCN_RECEIVED_STATS_OFFSET(p_hwfn->rel_pf_id);
	qed_memzero_hw(p_hwfn, p_ptt, addr, sizeof(info->roce.dcqcn_rx_stats));

	addr = BAR0_MAP_REG_PSDM_RAM +
	    PSTORM_ROCE_DCQCN_SENT_STATS_OFFSET(p_hwfn->rel_pf_id);
	qed_memzero_hw(p_hwfn, p_ptt, addr, sizeof(info->roce.dcqcn_tx_stats));

	addr = BAR0_MAP_REG_USDM_RAM +
	    USTORM_ROCE_CQE_STATS_OFFSET(p_hwfn->rel_pf_id);
	qed_memzero_hw(p_hwfn, p_ptt, addr, sizeof(info->roce.cqe_stats));

	addr = BAR0_MAP_REG_YSDM_RAM +
	    YSTORM_ROCE_ERROR_STATS_OFFSET(p_hwfn->rel_pf_id);
	qed_memzero_hw(p_hwfn, p_ptt, addr, sizeof(info->roce.error_stats));

	qed_ptt_release(p_hwfn, p_ptt);
err:
	qed_rdma_dec_ref_cnt(p_hwfn);

	return rc;
}

static void
qed_rdma_vf_query_stats(struct qed_hwfn *p_hwfn,
			struct qed_rdma_stats_out_params *out_params)
{
	struct qed_rdma_info *info;
	u32 pstats_addr, tstats_addr;

	info = p_hwfn->p_rdma_info;

	pstats_addr = PXP_VF_BAR0_START_PSDM_ZONE_B +
	    offsetof(struct pstorm_vf_zone, non_trigger.rdma_stats);
	tstats_addr = PXP_VF_BAR0_START_TSDM_ZONE_B +
	    offsetof(struct tstorm_vf_zone, non_trigger.rdma_stats);

	qed_memcpy_from(p_hwfn, NULL, &info->rdma_sent_pstats,
			pstats_addr, sizeof(info->rdma_sent_pstats));

	qed_memcpy_from(p_hwfn, NULL, &info->rdma_rcv_tstats,
			tstats_addr, sizeof(info->rdma_rcv_tstats));

	out_params->sent_bytes =
	    HILO_64_REGPAIR(info->rdma_sent_pstats.sent_bytes);
	out_params->sent_pkts =
	    HILO_64_REGPAIR(info->rdma_sent_pstats.sent_pkts);
	out_params->rcv_bytes =
	    HILO_64_REGPAIR(info->rdma_rcv_tstats.rcv_bytes);
	out_params->rcv_pkts = HILO_64_REGPAIR(info->rdma_rcv_tstats.rcv_pkts);
}

int
qed_rdma_query_stats(void *rdma_cxt,
		     u8 stats_queue,
		     struct qed_rdma_stats_out_params *out_params)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	u32 pstats_addr, tstats_addr, addr;
	struct qed_rdma_info *info;
	struct qed_ptt *p_ptt;
	u8 max_stats_queues;

#ifdef CONFIG_IWARP
	u32 xstats_addr;
#endif
	int rc = 0;

	if (!p_hwfn)
		return -EINVAL;

	if (!p_hwfn->p_rdma_info) {
		DP_INFO(p_hwfn->cdev,
			"qed rdma query stats failed due to NULL rdma_info\n");
		return -EINVAL;
	}

	memset(out_params, 0, sizeof(*out_params));

	if (IS_VF(p_hwfn->cdev)) {
		qed_rdma_vf_query_stats(p_hwfn, out_params);
		return 0;
	}

	info = p_hwfn->p_rdma_info;

	rc = qed_rdma_inc_ref_cnt(p_hwfn);
	if (rc)
		return rc;

	max_stats_queues = p_hwfn->p_rdma_info->dev->max_stats_queues;
	if (stats_queue >= max_stats_queues) {
		DP_ERR(p_hwfn->cdev,
		       "qed rdma query stats failed due to invalid statistics queue %d. maximum is %d\n",
		       stats_queue, max_stats_queues);
		rc = -EINVAL;
		goto err;
	}

	pstats_addr = BAR0_MAP_REG_PSDM_RAM +
	    PSTORM_RDMA_QUEUE_STAT_OFFSET(stats_queue);
	tstats_addr = BAR0_MAP_REG_TSDM_RAM +
	    TSTORM_RDMA_QUEUE_STAT_OFFSET(stats_queue);

#ifdef CONFIG_IWARP
	/* Statistics per PF ID */
	xstats_addr = BAR0_MAP_REG_XSDM_RAM +
	    XSTORM_IWARP_RXMIT_STATS_OFFSET(stats_queue);
#endif

	memset(&info->rdma_sent_pstats, 0, sizeof(info->rdma_sent_pstats));
	memset(&info->rdma_rcv_tstats, 0, sizeof(info->rdma_rcv_tstats));
	memset(&info->roce.event_stats, 0, sizeof(info->roce.event_stats));
	memset(&info->roce.dcqcn_rx_stats, 0,
	       sizeof(info->roce.dcqcn_rx_stats));
	memset(&info->roce.dcqcn_tx_stats, 0,
	       sizeof(info->roce.dcqcn_tx_stats));
#ifdef CONFIG_IWARP
	memset(&info->iwarp.stats, 0, sizeof(info->iwarp.stats));
#endif

	p_ptt = qed_ptt_acquire(p_hwfn);

	if (!p_ptt) {
		rc = -EBUSY;
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "rc = %d\n", rc);
		goto err;
	}

	qed_memcpy_from(p_hwfn, p_ptt, &info->rdma_sent_pstats,
			pstats_addr, sizeof(struct rdma_sent_stats));

	qed_memcpy_from(p_hwfn, p_ptt, &info->rdma_rcv_tstats,
			tstats_addr, sizeof(struct rdma_rcv_stats));

	addr = BAR0_MAP_REG_TSDM_RAM +
	    TSTORM_ROCE_EVENTS_STAT_OFFSET(p_hwfn->rel_pf_id);
	qed_memcpy_from(p_hwfn, p_ptt, &info->roce.event_stats, addr,
			sizeof(struct roce_events_stats));

	addr = BAR0_MAP_REG_YSDM_RAM +
	    YSTORM_ROCE_DCQCN_RECEIVED_STATS_OFFSET(p_hwfn->rel_pf_id);
	qed_memcpy_from(p_hwfn, p_ptt, &info->roce.dcqcn_rx_stats, addr,
			sizeof(struct roce_dcqcn_received_stats));

	addr = BAR0_MAP_REG_PSDM_RAM +
	    PSTORM_ROCE_DCQCN_SENT_STATS_OFFSET(p_hwfn->rel_pf_id);
	qed_memcpy_from(p_hwfn, p_ptt, &info->roce.dcqcn_tx_stats, addr,
			sizeof(struct roce_dcqcn_sent_stats));

	addr = BAR0_MAP_REG_USDM_RAM +
	    USTORM_ROCE_CQE_STATS_OFFSET(p_hwfn->rel_pf_id);
	qed_memcpy_from(p_hwfn, p_ptt, &info->roce.cqe_stats, addr,
			sizeof(struct roce_cqe_stats));

	addr = BAR0_MAP_REG_YSDM_RAM +
	    YSTORM_ROCE_ERROR_STATS_OFFSET(p_hwfn->rel_pf_id);
	qed_memcpy_from(p_hwfn, p_ptt, &info->roce.error_stats, addr,
			sizeof(struct roce_error_stats));

#ifdef CONFIG_IWARP
	qed_memcpy_from(p_hwfn, p_ptt, &info->iwarp.stats,
			xstats_addr, sizeof(struct iwarp_rxmit_stats_drv));
#endif

	qed_ptt_release(p_hwfn, p_ptt);

	memset(out_params, 0, sizeof(*out_params));

	out_params->sent_bytes =
	    HILO_64_REGPAIR(info->rdma_sent_pstats.sent_bytes);
	out_params->sent_pkts =
	    HILO_64_REGPAIR(info->rdma_sent_pstats.sent_pkts);
	out_params->rcv_bytes =
	    HILO_64_REGPAIR(info->rdma_rcv_tstats.rcv_bytes);
	out_params->rcv_pkts = HILO_64_REGPAIR(info->rdma_rcv_tstats.rcv_pkts);

	out_params->silent_drops =
	    le16_to_cpu(info->roce.event_stats.silent_drops);
	out_params->rnr_nacks_sent =
	    le16_to_cpu(info->roce.event_stats.rnr_naks_sent);
	out_params->icrc_errors =
	    le32_to_cpu(info->roce.event_stats.icrc_error_count);
	out_params->retransmit_events =
	    le32_to_cpu(info->roce.event_stats.retransmit_count);
	out_params->implied_nak_seq_err =
	    le32_to_cpu(info->roce.event_stats.implied_nak_seq_err);
	out_params->duplicate_request =
	    le32_to_cpu(info->roce.event_stats.duplicate_request);
	out_params->local_ack_timeout_err =
	    le32_to_cpu(info->roce.event_stats.local_ack_timeout_err);
	out_params->out_of_sequence =
	    le32_to_cpu(info->roce.event_stats.out_of_sequence);
	out_params->packet_seq_err =
	    le32_to_cpu(info->roce.event_stats.packet_seq_err);
	out_params->rnr_nak_retry_err =
	    le32_to_cpu(info->roce.event_stats.rnr_nak_retry_err);

	out_params->req_cqe_error =
	    le32_to_cpu(info->roce.cqe_stats.req_cqe_error);
	out_params->req_remote_access_errors =
	    le32_to_cpu(info->roce.cqe_stats.req_remote_access_errors);
	out_params->req_remote_invalid_request =
	    le32_to_cpu(info->roce.cqe_stats.req_remote_invalid_request);
	out_params->resp_cqe_error =
	    le32_to_cpu(info->roce.cqe_stats.resp_cqe_error);
	out_params->resp_local_length_error =
	    le32_to_cpu(info->roce.cqe_stats.resp_local_length_error);

	out_params->resp_remote_access_errors =
	    le32_to_cpu(info->roce.error_stats.resp_remote_access_errors);

	out_params->ecn_pkt_rcv =
	    HILO_64_REGPAIR(info->roce.dcqcn_rx_stats.ecn_pkt_rcv);
	out_params->cnp_pkt_rcv =
	    HILO_64_REGPAIR(info->roce.dcqcn_rx_stats.cnp_pkt_rcv);
	out_params->cnp_pkt_sent =
	    HILO_64_REGPAIR(info->roce.dcqcn_tx_stats.cnp_pkt_sent);
	out_params->cnp_pkt_reject =
	    HILO_64_REGPAIR(info->roce.dcqcn_rx_stats.cnp_pkt_reject);

#ifdef CONFIG_IWARP
	out_params->iwarp_tx_fast_rxmit_cnt =
	    HILO_64_REGPAIR(info->iwarp.stats.tx_fast_retransmit_event_cnt);
	out_params->iwarp_tx_slow_start_cnt =
	    HILO_64_REGPAIR(info->iwarp.stats.tx_go_to_slow_start_event_cnt);
	out_params->unalign_rx_comp = info->iwarp.unalign_rx_comp;
#endif

err:
	qed_rdma_dec_ref_cnt(p_hwfn);

	return rc;
}

int qed_rdma_query_counters_inner(struct qed_hwfn *p_hwfn, struct qed_rdma_counters_out_params
				  *out_params, struct qed_rdma_info *rdma_info)
{
	unsigned long *bitmap;
	unsigned int nbits;

	if (!rdma_info)
		return -EINVAL;

	memset(out_params, 0, sizeof(*out_params));

	bitmap = rdma_info->pd_map.bitmap;
	nbits = rdma_info->pd_map.max_count;
	out_params->pd_count = bitmap_weight(bitmap, nbits);
	out_params->max_pd = nbits;

	bitmap = rdma_info->dpi_map.bitmap;
	nbits = rdma_info->dpi_map.max_count;
	out_params->dpi_count = bitmap_weight(bitmap, nbits);
	out_params->max_dpi = nbits;

	bitmap = rdma_info->cq_map.bitmap;
	nbits = rdma_info->cq_map.max_count;
	out_params->cq_count = bitmap_weight(bitmap, nbits);
	out_params->max_cq = nbits;

	if (IS_IWARP(p_hwfn)) {
		bitmap = rdma_info->cid_map.bitmap;
		nbits = rdma_info->cid_map.max_count;
		out_params->qp_count = bitmap_weight(bitmap, nbits) -
		    QED_IWARP_PREALLOC_CNT;
	} else {
		bitmap = rdma_info->qp_map.bitmap;
		nbits = rdma_info->qp_map.max_count;
		out_params->qp_count = bitmap_weight(bitmap, nbits);
	}

	out_params->max_qp = nbits;

	bitmap = rdma_info->tid_map.bitmap;
	nbits = rdma_info->tid_map.max_count;
	out_params->tid_count = bitmap_weight(bitmap, nbits);
	out_params->max_tid = nbits;

	bitmap = rdma_info->srq_map.bitmap;
	nbits = rdma_info->srq_map.max_count;
	out_params->srq_count = bitmap_weight(bitmap, nbits);
	out_params->max_srq = nbits;

	bitmap = rdma_info->xrc_srq_map.bitmap;
	nbits = rdma_info->xrc_srq_map.max_count;
	out_params->xrc_srq_count = bitmap_weight(bitmap, nbits);
	out_params->max_xrc_srq = nbits;

	bitmap = rdma_info->xrcd_map.bitmap;
	nbits = rdma_info->xrcd_map.max_count;
	out_params->xrcd_count = bitmap_weight(bitmap, nbits);
	out_params->max_xrcd = nbits;

	return 0;
}

int
qed_rdma_query_counters(void *rdma_cxt,
			struct qed_rdma_counters_out_params *out_params)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	int rc = 0;

	if (IS_VF(p_hwfn->cdev))
		rc = qed_vf_pf_rdma_query_counters(p_hwfn, out_params);
	else
		rc = qed_rdma_query_counters_inner(p_hwfn, out_params,
						   p_hwfn->p_rdma_info);

	return rc;
}

int qed_rdma_resize_cnq(void *rdma_cxt,
			struct qed_rdma_resize_cnq_in_params *params)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "cnq_id = %08x\n", params->cnq_id);

	/* @@@TBD: waiting for fw (there is no ramrod yet) */
	return -EOPNOTSUPP;
}

void qed_rdma_remove_user(void *rdma_cxt, u16 dpi)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "dpi = %08x\n", dpi);

	spin_lock_bh(&p_hwfn->p_rdma_info->lock);
	qed_bmap_release_id(p_hwfn, &p_hwfn->p_rdma_info->dpi_map, dpi);
	spin_unlock_bh(&p_hwfn->p_rdma_info->lock);
}

int qed_rdma_configure_prs(struct qed_hwfn *p_hwfn,
			   struct qed_rdma_info *rdma_info, u32 icid)
{
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	u32 prs_reg_addr, dest_qp_max;

	if (!p_ptt)
		return -EAGAIN;

	prs_reg_addr = rdma_info->iov_info.is_vf ?
	    PRS_REG_ROCE_DEST_QP_MAX_VF : PRS_REG_ROCE_DEST_QP_MAX_PF;

	dest_qp_max = qed_rd(p_hwfn, p_ptt, prs_reg_addr);

	/* w/a:
	 * A stray RoCE packet from switch, or a RoCE packet with crc-error, can
	 * be received with icid=0 even before its CDU context was initialized.
	 * Because the prs treats icid > dest_qp_max as a non-valid value, and
	 * since the reset value of dest_qp_max is 0 - 0 will always be a valid
	 * value. To avoid a load of uninitialized context - we write to the prs
	 * search reg only when the first one is allocated.
	 */

	if (icid > dest_qp_max)
		qed_wr(p_hwfn, p_ptt, prs_reg_addr, icid);

	if (p_hwfn->rdma_prs_search_reg && !p_hwfn->b_rdma_enabled_in_prs) {
		/* Enable Rdma search */
		qed_wr(p_hwfn, p_ptt, p_hwfn->rdma_prs_search_reg, 1);
		p_hwfn->b_rdma_enabled_in_prs = true;
	}

	qed_ptt_release(p_hwfn, p_ptt);

	return 0;
}

extern const struct qed_common_ops qed_common_ops_pass;

#ifdef CONFIG_DCQCN

/* According to 802-1au */
#define DCQCN_RPG_AI_RATE       5
#define DCQCN_RPG_HAI_RATE      50
#define DCQCN_RPG_GD            256

/* According to sigcomm */
#define DCQCN_CNP_SEND_TIMEOUT  50
#define DCQCN_BYTECOUNT_RATE    (10 * 1024 * 1024)
#define DCQCN_ALPHA_INTERVAL    55
#define DCQCN_TIMEOUT           55

/* 100G speed */
#define DCQCN_RL_MAX_RATE       100000

static uint dcqcn_cnp_send_timeout = DCQCN_CNP_SEND_TIMEOUT;
module_param(dcqcn_cnp_send_timeout, uint, S_IRUGO);
MODULE_PARM_DESC(dcqcn_cnp_send_timeout,
		 "Minimal difference of send time between CNP packets. Units are in microseconds. Values between 50..500000");

static uint dcqcn_cnp_dscp;
module_param(dcqcn_cnp_dscp, uint, S_IRUGO);
MODULE_PARM_DESC(dcqcn_cnp_dscp,
		 "DSCP value to be used on CNP packets. Values between 0..63\n");

static uint dcqcn_cnp_vlan_priority;
module_param(dcqcn_cnp_vlan_priority, uint, S_IRUGO);
MODULE_PARM_DESC(dcqcn_cnp_vlan_priority,
		 "vlan priority to be used on CNP packets. Values between 0..7\n");

static uint dcqcn_notification_point = 1;
module_param(dcqcn_notification_point, uint, S_IRUGO);
MODULE_PARM_DESC(dcqcn_notification_point,
		 "0 - Disable dcqcn notification point. 1 - Enable dcqcn notification point");

static uint dcqcn_reaction_point = 1;
module_param(dcqcn_reaction_point, uint, S_IRUGO);
MODULE_PARM_DESC(dcqcn_reaction_point,
		 "0 - Disable dcqcn reaction point. 1 - Enable dcqcn reaction point");

static uint dcqcn_rl_bc_rate = DCQCN_BYTECOUNT_RATE;
module_param(dcqcn_rl_bc_rate, uint, S_IRUGO);
MODULE_PARM_DESC(dcqcn_rl_bc_rate, "Byte counter limit");

static uint dcqcn_rl_max_rate = DCQCN_RL_MAX_RATE;
module_param(dcqcn_rl_max_rate, uint, S_IRUGO);
MODULE_PARM_DESC(dcqcn_rl_max_rate, "Maximum rate in Mbps");

static uint dcqcn_rl_r_ai = DCQCN_RPG_AI_RATE;
module_param(dcqcn_rl_r_ai, uint, S_IRUGO);
MODULE_PARM_DESC(dcqcn_rl_r_ai, "Active increase rate in Mbps");

static uint dcqcn_rl_r_hai = DCQCN_RPG_HAI_RATE;
module_param(dcqcn_rl_r_hai, uint, S_IRUGO);
MODULE_PARM_DESC(dcqcn_rl_r_hai, "Hyper active increase rate in Mbps");

static uint dcqcn_gd = DCQCN_RPG_GD;
module_param(dcqcn_gd, uint, S_IRUGO);
MODULE_PARM_DESC(dcqcn_gd,
		 "Alpha update gain denominator. Set to 32 for 1/32, etc");

static uint dcqcn_k_us = DCQCN_ALPHA_INTERVAL;
module_param(dcqcn_k_us, uint, S_IRUGO);
MODULE_PARM_DESC(dcqcn_k_us, "Alpha update interval");

static uint dcqcn_timeout_us = DCQCN_TIMEOUT;
module_param(dcqcn_timeout_us, uint, S_IRUGO);
MODULE_PARM_DESC(dcqcn_timeout_us, "Dcqcn timeout");

static void qed_init_dcqcn(struct qed_dev *cdev,
			   struct qed_rdma_start_in_params *in_params)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_roce_dcqcn_params *dcqcn_params =
	    &(in_params->roce.dcqcn_params);

	if (!IS_QED_DCQCN(p_hwfn))
		return;

	dcqcn_params->cnp_send_timeout = dcqcn_cnp_send_timeout;
	dcqcn_params->cnp_dscp = dcqcn_cnp_dscp;
	dcqcn_params->cnp_vlan_priority = dcqcn_cnp_vlan_priority;
	dcqcn_params->notification_point = dcqcn_notification_point;
	dcqcn_params->reaction_point = dcqcn_reaction_point;
	dcqcn_params->rl_bc_rate = dcqcn_rl_bc_rate;
	dcqcn_params->rl_max_rate = dcqcn_rl_max_rate;
	dcqcn_params->rl_r_ai = dcqcn_rl_r_ai;
	dcqcn_params->rl_r_hai = dcqcn_rl_r_hai;
	dcqcn_params->dcqcn_gd = dcqcn_gd;
	dcqcn_params->dcqcn_k_us = dcqcn_k_us;
	dcqcn_params->dcqcn_timeout_us = dcqcn_timeout_us;

	DP_ERR(cdev,
	       "DCQCN TO:%d\ndscp:%d\nvlan_pri:%d\nnotif:%d\nreact:%d\nrl_bc:%d\nrl_max:%d\nrl_r:%d\nrl-r_hai:%d\ng:%d\nk_us:%d\nto:%d\n",
	       dcqcn_params->cnp_send_timeout,
	       dcqcn_params->cnp_dscp,
	       dcqcn_params->cnp_vlan_priority,
	       dcqcn_params->notification_point,
	       dcqcn_params->reaction_point,
	       dcqcn_params->rl_bc_rate,
	       dcqcn_params->rl_max_rate,
	       dcqcn_params->rl_r_ai,
	       dcqcn_params->rl_r_hai,
	       dcqcn_params->dcqcn_gd,
	       dcqcn_params->dcqcn_k_us, dcqcn_params->dcqcn_timeout_us);
}

#endif

static int qed_fill_rdma_dev_info(struct qed_dev *cdev,
				  struct qed_dev_rdma_info *info)
{
	struct qed_hwfn *p_hwfn = QED_AFFIN_HWFN(cdev);

	memset(info, 0, sizeof(*info));

	info->rdma_type =
	    (p_hwfn->hw_info.personality == QED_PCI_ETH_ROCE ||
	     IS_VF(cdev)) ? QED_RDMA_TYPE_ROCE : QED_RDMA_TYPE_IWARP;

	/* The DPM status is determined by the DB BAR and disregards the DCBx
	 * influence. This means that there may be a case where DPM will be
	 * disabled in the HW by DCBx but DPMs will be attempted anyway. There
	 * is no harm in that. This mechanism was chosen because there's
	 * (currently) no way to notify the user space that the state of the
	 * DPM changed and we want to have a unified approach for all users.
	 * Note that kernel space, that currently doesn't implement DPM, can
	 * easily check the actual DPM state.
	 */
	if (!p_hwfn->db_bar_no_edpm) {
		if (QED_IS_IWARP_PERSONALITY(p_hwfn))
			info->dpm_enabled = QED_IWARP_DPM_TYPE_LEGACY;
		else
			info->dpm_enabled = QED_ROCE_DPM_TYPE_ENHANCED |
			    QED_ROCE_DPM_TYPE_LEGACY |
			    QED_ROCE_DPM_TYPE_ENHANCED_MODE;
	} else {
		info->dpm_enabled = 0;
	}

	qed_fill_dev_info(cdev, &info->common);

	return 0;
}

static int qed_rdma_get_sb_start(struct qed_dev *cdev)
{
	if (IS_PF(cdev))
		return cdev->num_l2_queues / cdev->num_hwfns;
	else
		/* SAGIV TODO - need to take care of CMT */
		return (int)qed_vf_rdma_cnq_sb_start_id(QED_AFFIN_HWFN(cdev));
}

u32 qed_rdma_get_sb_id(struct qed_hwfn *p_hwfn, u32 rel_sb_id)
{
	/* First SB ID for RDMA is after all the L2 SBs */
	return qed_rdma_get_sb_start(p_hwfn->cdev) + rel_sb_id;
}

static int qed_rdma_init(struct qed_dev *cdev,
			 struct qed_rdma_start_in_params *params)
{
	int rc;

#ifdef CONFIG_DCQCN
	qed_init_dcqcn(cdev, params);
#endif

	rc = qed_rdma_start(QED_AFFIN_HWFN(cdev), params);

	return rc;
}

static int qed_rdma_get_min_cnq_msix(struct qed_dev *cdev)
{
	u16 n_msix = 0;
	u8 n_cnq = 0;
	int min;

	if (IS_PF(cdev))
		n_cnq = FEAT_NUM(QED_AFFIN_HWFN(cdev), QED_RDMA_CNQ);
	else
		qed_vf_get_num_cnqs(QED_AFFIN_HWFN(cdev), &n_cnq);

	n_msix = cdev->int_params.rdma_msix_cnt;
	min = min_t(int, n_cnq, n_msix);

	if (min)
		DP_VERBOSE(cdev, QED_MSG_RDMA, "n_msix = %d, n_cnq=%d\n",
			   n_msix, n_cnq);
	else
		DP_ERR(cdev, "n_msix = %d, n_cnq=%d\n", n_msix, n_cnq);

	return min;
}

static int qed_rdma_set_int(struct qed_dev *cdev, u16 cnt)
{
	int limit = 0;

	/* Mark the fastpath as free/used */
	cdev->int_params.fp_initialized = cnt ? true : false;

	if (cdev->int_params.out.int_mode != QED_INT_MODE_MSIX) {
		DP_ERR(cdev,
		       "qed roce supports only MSI-X interrupts (detected %d).\n",
		       cdev->int_params.out.int_mode);
		return -EINVAL;
	} else if (cdev->int_params.fp_msix_cnt) {
		limit = cdev->int_params.rdma_msix_cnt;
	}

	if (!limit)
		return -ENOMEM;

	return min_t(int, cnt, limit);
}

static int qed_rdma_get_int(struct qed_dev *cdev, struct qed_int_info *info)
{
	memset(info, 0, sizeof(struct qed_int_info));

	if (!cdev->int_params.fp_initialized) {
		DP_INFO(cdev,
			"Protocol driver requested interrupt information, but its support is not yet configured\n");
		return -EINVAL;
	}

	/* Need to expose only MSI-X information; Single IRQ is handled solely
	 * by qed.
	 */
	if (cdev->int_params.out.int_mode == QED_INT_MODE_MSIX) {
		int msix_base = cdev->int_params.rdma_msix_base;

		info->msix_cnt = cdev->int_params.rdma_msix_cnt;
		info->msix = &cdev->int_params.msix_table[msix_base];

		DP_VERBOSE(cdev, QED_MSG_RDMA, "msix_cnt = %d msix_base=%d\n",
			   info->msix_cnt, msix_base);
	}

	return 0;
}

static void *qed_rdma_get_rdma_ctx(struct qed_dev *cdev)
{
	return QED_AFFIN_HWFN(cdev);
}

static int qed_rdma_get_dscp_priority(void *rdma_cxt, u8 dscp_index, u8 * pri)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	int rc = -EINVAL;

	if (!IS_VF(p_hwfn->cdev) && qed_dcbx_get_dscp_state(p_hwfn)) {
		rc = qed_dcbx_get_dscp_priority(p_hwfn, dscp_index, pri);
		if (rc)
			DP_INFO(p_hwfn,
				"Failed to get priority val for dscp idx %d\n",
				dscp_index);
	}

	return rc;
}

static int qed_roce_ll2_set_mac_filter(struct qed_dev *cdev,
				       u8 * old_mac_address,
				       const u8 * new_mac_address)
{
	int rc = 0;

	/* TODO: do we really need a mutex here?? */
	if (old_mac_address)
		qed_llh_remove_mac_filter(cdev, 0, old_mac_address);
	if (new_mac_address)
		rc = qed_llh_add_mac_filter(cdev, 0, new_mac_address);

	if (rc)
		DP_ERR(cdev,
		       "qed roce ll2 mac filter set: failed to add MAC filter\n");

	return rc;
}

static int qed_iwarp_set_engine_affin(struct qed_dev *cdev, bool b_reset)
{
	enum qed_eng eng;
	u8 ppfid = 0;
	int rc;

#ifdef _HAS_DEVLINK		/* QED_UPSTREAM */
	/* Make sure iwarp cmt mode is enabled before setting affinity */
	if (!cdev->iwarp_cmt)
		return -EINVAL;
#endif

	if (b_reset)
		eng = QED_BOTH_ENG;
	else
		eng = cdev->l2_affin_hint ? QED_ENG1 : QED_ENG0;

	rc = qed_llh_set_ppfid_affinity(cdev, ppfid, eng);
	if (rc) {
		DP_NOTICE(cdev,
			  "Failed to set the engine affinity of ppfid %d\n",
			  ppfid);
		return rc;
	}

	DP_VERBOSE(cdev, (QED_MSG_RDMA | QED_MSG_SP),
		   "LLH: Set the engine affinity of non-RoCE packets as %d\n",
		   eng);

	return 0;
}

static const struct qed_rdma_ops qed_rdma_ops_pass = {
	INIT_STRUCT_FIELD(common,
			  &qed_common_ops_pass),
	INIT_STRUCT_FIELD(fill_dev_info,
			  &qed_fill_rdma_dev_info),
	INIT_STRUCT_FIELD(rdma_get_rdma_ctx,
			  &qed_rdma_get_rdma_ctx),
	INIT_STRUCT_FIELD(rdma_init, &qed_rdma_init),
	INIT_STRUCT_FIELD(rdma_add_user,
			  &qed_rdma_add_user),
	INIT_STRUCT_FIELD(rdma_remove_user,
			  &qed_rdma_remove_user),
	INIT_STRUCT_FIELD(rdma_stop, &qed_rdma_stop),
	INIT_STRUCT_FIELD(rdma_query_device,
			  &qed_rdma_query_device),
	INIT_STRUCT_FIELD(rdma_query_port,
			  &qed_rdma_query_port),
	INIT_STRUCT_FIELD(rdma_alloc_pd,
			  &qed_rdma_alloc_pd),
	INIT_STRUCT_FIELD(rdma_dealloc_pd,
			  &qed_rdma_free_pd),
	INIT_STRUCT_FIELD(rdma_alloc_xrcd,
			  &qed_rdma_alloc_xrcd),
	INIT_STRUCT_FIELD(rdma_dealloc_xrcd,
			  &qed_rdma_free_xrcd),
	INIT_STRUCT_FIELD(rdma_get_start_sb,
			  &qed_rdma_get_sb_start),
	INIT_STRUCT_FIELD(rdma_create_cq,
			  &qed_rdma_create_cq),
	INIT_STRUCT_FIELD(rdma_destroy_cq,
			  &qed_rdma_destroy_cq),
	INIT_STRUCT_FIELD(rdma_create_qp,
			  &qed_rdma_create_qp),
	INIT_STRUCT_FIELD(rdma_modify_qp,
			  &qed_rdma_modify_qp),
	INIT_STRUCT_FIELD(rdma_query_qp,
			  &qed_rdma_query_qp),
	INIT_STRUCT_FIELD(rdma_destroy_qp,
			  &qed_rdma_destroy_qp),
	INIT_STRUCT_FIELD(rdma_alloc_tid,
			  &qed_rdma_alloc_tid),
	INIT_STRUCT_FIELD(rdma_free_tid,
			  &qed_rdma_free_tid),
	INIT_STRUCT_FIELD(rdma_register_tid,
			  &qed_rdma_register_tid),
	INIT_STRUCT_FIELD(rdma_deregister_tid,
			  &qed_rdma_deregister_tid),
	INIT_STRUCT_FIELD(rdma_get_rdma_int,
			  &qed_rdma_get_int),
	INIT_STRUCT_FIELD(rdma_set_rdma_int,
			  &qed_rdma_set_int),
	INIT_STRUCT_FIELD(rdma_get_stats_queue,
			  &qed_rdma_get_stats_queue),
	INIT_STRUCT_FIELD(rdma_query_stats,
			  &qed_rdma_query_stats),
	INIT_STRUCT_FIELD(rdma_reset_stats,
			  &qed_rdma_reset_stats),
	INIT_STRUCT_FIELD(rdma_query_counters,
			  &qed_rdma_query_counters),
	INIT_STRUCT_FIELD(rdma_get_min_cnq_msix,
			  &qed_rdma_get_min_cnq_msix),
	INIT_STRUCT_FIELD(rdma_cnq_prod_update,
			  &qed_rdma_cnq_prod_update),

	INIT_STRUCT_FIELD(rdma_create_srq,
			  &qed_rdma_create_srq),
	INIT_STRUCT_FIELD(rdma_destroy_srq,
			  &qed_rdma_destroy_srq),
	INIT_STRUCT_FIELD(rdma_modify_srq,
			  &qed_rdma_modify_srq),
	INIT_STRUCT_FIELD(rdma_get_dscp_priority,
			  &qed_rdma_get_dscp_priority),

	/* RDMA CM / LL2 */
	INIT_STRUCT_FIELD(ll2_acquire_connection,
			  &qed_ll2_acquire_connection),
	INIT_STRUCT_FIELD(ll2_establish_connection,
			  &qed_ll2_establish_connection),
	INIT_STRUCT_FIELD(ll2_terminate_connection,
			  &qed_ll2_terminate_connection),
	INIT_STRUCT_FIELD(ll2_release_connection,
			  &qed_ll2_release_connection),
	INIT_STRUCT_FIELD(ll2_post_rx_buffer,
			  &qed_ll2_post_rx_buffer),
	INIT_STRUCT_FIELD(ll2_prepare_tx_packet,
			  &qed_ll2_prepare_tx_packet),
	INIT_STRUCT_FIELD(ll2_set_fragment_of_tx_packet,
			  &qed_ll2_set_fragment_of_tx_packet),

	INIT_STRUCT_FIELD(ll2_set_mac_filter,
			  &qed_roce_ll2_set_mac_filter),

	INIT_STRUCT_FIELD(ll2_get_stats,
			  &qed_ll2_get_stats),
	INIT_STRUCT_FIELD(ll2_completion,
			  &qed_ll2_completion),

	INIT_STRUCT_FIELD(iwarp_set_engine_affin,
			  &qed_iwarp_set_engine_affin),

#ifdef CONFIG_IWARP
	INIT_STRUCT_FIELD(iwarp_connect,
			  &qed_iwarp_connect),
	INIT_STRUCT_FIELD(iwarp_create_listen,
			  &qed_iwarp_create_listen),
	INIT_STRUCT_FIELD(iwarp_destroy_listen,
			  &qed_iwarp_destroy_listen),
	INIT_STRUCT_FIELD(iwarp_accept,
			  &qed_iwarp_accept),
	INIT_STRUCT_FIELD(iwarp_reject,
			  &qed_iwarp_reject),
	INIT_STRUCT_FIELD(iwarp_send_rtr,
			  &qed_iwarp_send_rtr),
#endif
};

const struct qed_rdma_ops *qed_get_rdma_ops(u32 version)
{
	if (version != QED_ROCE_INTERFACE_VERSION) {
		pr_notice("Cannot supply rdma operations [%08x != %08x]\n",
			  version, QED_ROCE_INTERFACE_VERSION);
		return NULL;
	}

	return &qed_rdma_ops_pass;
}

EXPORT_SYMBOL(qed_get_rdma_ops);
