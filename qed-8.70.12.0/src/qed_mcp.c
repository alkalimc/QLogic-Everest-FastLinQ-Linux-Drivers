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
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/if_ether.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#define __PREVENT_DUMP_MEM_ARR__
#define __PREVENT_PXP_GLOBAL_WIN__
#include "qed.h"
#include "qed_cxt.h"
#include "qed_dcbx.h"
#include "qed_fcoe.h"
#include "qed_hsi.h"
#include "qed_hw.h"
#include "qed_iro_hsi.h"
#include "qed_mcp.h"
#include "qed_mfw_hsi.h"
#include "qed_reg_addr.h"
#include "qed_sp.h"
#include "qed_sriov.h"
#include "qed_vf.h"
#include "qed_eth_if.h"

#define GRCBASE_MCP     0xe00000

#define QED_MCP_RESP_ITER_US            10
#define QED_DRV_MB_MAX_RETRIES  (500 * 1000)	/* Account for 5 sec */
#define QED_MCP_RESET_RETRIES           (50 * 1000)	/* Account for 500 msec */

#ifndef ASIC_ONLY
/* Non-ASIC:
 * The waiting interval is multiplied by 100 to reduce the impact of the
 * built-in delay of 100usec in each qed_rd().
 * In addition, a factor of 6 comparing to ASIC is applied.
 */
#define QED_EMUL_MCP_RESP_ITER_US       (QED_MCP_RESP_ITER_US * 100)
#define QED_EMUL_DRV_MB_MAX_RETRIES     ((QED_DRV_MB_MAX_RETRIES / 100) * 6)
#define QED_EMUL_MCP_RESET_RETRIES      ((QED_MCP_RESET_RETRIES / 100) * 6)
#endif

#define DRV_INNER_WR(_p_hwfn, _p_ptt, _ptr, _offset, _val)	     \
	qed_wr(_p_hwfn, _p_ptt, (_p_hwfn->mcp_info->_ptr + _offset), \
	       _val)

#define DRV_INNER_RD(_p_hwfn, _p_ptt, _ptr, _offset) \
	qed_rd(_p_hwfn, _p_ptt, (_p_hwfn->mcp_info->_ptr + _offset))

#define DRV_MB_WR(_p_hwfn, _p_ptt, _field, _val)  \
	DRV_INNER_WR(p_hwfn, _p_ptt, drv_mb_addr, \
		     offsetof(struct public_drv_mb, _field), _val)

#define DRV_MB_RD(_p_hwfn, _p_ptt, _field)	   \
	DRV_INNER_RD(_p_hwfn, _p_ptt, drv_mb_addr, \
		     offsetof(struct public_drv_mb, _field))

#define PDA_COMP ((FW_MAJOR_VERSION) + (FW_MINOR_VERSION << 8))

#define MCP_BYTES_PER_MBIT_OFFSET 17

#ifndef ASIC_ONLY
static int loaded;
static int loaded_port[MAX_NUM_PORTS] = { 0 };
#endif

bool qed_mcp_is_init(struct qed_hwfn *p_hwfn)
{
	if (!p_hwfn->mcp_info || !p_hwfn->mcp_info->b_mfw_running)
		return false;
	return true;
}

void qed_mcp_cmd_port_init(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 addr = SECTION_OFFSIZE_ADDR(p_hwfn->mcp_info->public_base,
					PUBLIC_PORT);
	u32 mfw_mb_offsize = qed_rd(p_hwfn, p_ptt, addr);

	p_hwfn->mcp_info->port_addr = SECTION_ADDR(mfw_mb_offsize,
						   MFW_PORT(p_hwfn));
	p_hwfn->mcp_info->port_size = QED_SECTION_SIZE(mfw_mb_offsize);
	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "port_addr = 0x%x, port_size 0x%x, port_id 0x%02x\n",
		   p_hwfn->mcp_info->port_addr,
		   p_hwfn->mcp_info->port_size, MFW_PORT(p_hwfn));
}

void qed_mcp_read_mb(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 length = MFW_DRV_MSG_MAX_DWORDS(p_hwfn->mcp_info->mfw_mb_length);
	__be32 tmp;
	u32 i;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_TEDIBEAR(p_hwfn->cdev))
		return;
#endif

	if (!p_hwfn->mcp_info->public_base)
		return;

	for (i = 0; i < length; i++) {
		tmp = qed_rd(p_hwfn, p_ptt,
			     p_hwfn->mcp_info->mfw_mb_addr +
			     (i << 2) + sizeof(u32));

		((u32 *) p_hwfn->mcp_info->mfw_mb_cur)[i] = be32_to_cpu(tmp);
	}
}

static const char *qed_mcp_print_cmd_name(u32 cmd)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(qed_mcp_cmd_name_table); i++)
		if (qed_mcp_cmd_name_table[i].value == cmd)
			return qed_mcp_cmd_name_table[i].name;

	return "unknown";
}

struct qed_mcp_cmd_elem {
	struct list_head list;
	struct qed_mcp_mb_params *p_mb_params;
	u16 expected_seq_num;
	bool b_is_completed;
};

/* Must be called while cmd_lock is acquired */
static struct qed_mcp_cmd_elem *qed_mcp_cmd_add_elem(struct qed_hwfn *p_hwfn,
						     struct qed_mcp_mb_params
						     *p_mb_params,
						     u16 expected_seq_num)
{
	struct qed_mcp_cmd_elem *p_cmd_elem = NULL;

	p_cmd_elem = kzalloc(sizeof(*p_cmd_elem), GFP_ATOMIC);
	if (!p_cmd_elem) {
		DP_NOTICE(p_hwfn,
			  "Failed to allocate `struct qed_mcp_cmd_elem'\n");
		goto out;
	}

	p_cmd_elem->p_mb_params = p_mb_params;
	p_cmd_elem->expected_seq_num = expected_seq_num;
	list_add(&p_cmd_elem->list, &p_hwfn->mcp_info->cmd_list);
out:
	return p_cmd_elem;
}

/* Must be called while cmd_lock is acquired */
static void qed_mcp_cmd_del_elem(struct qed_hwfn *p_hwfn,
				 struct qed_mcp_cmd_elem *p_cmd_elem)
{
	list_del(&p_cmd_elem->list);
	kfree(p_cmd_elem);
}

/* Must be called while cmd_lock is acquired */
static struct qed_mcp_cmd_elem *qed_mcp_cmd_get_elem(struct qed_hwfn *p_hwfn,
						     u16 seq_num)
{
	struct qed_mcp_cmd_elem *p_cmd_elem = NULL;

	list_for_each_entry(p_cmd_elem, &p_hwfn->mcp_info->cmd_list, list) {
		if (p_cmd_elem->expected_seq_num == seq_num)
			return p_cmd_elem;
	}

	return NULL;
}

int qed_mcp_free(struct qed_hwfn *p_hwfn)
{
	if (p_hwfn->mcp_info) {
		struct qed_mcp_cmd_elem *p_cmd_elem = NULL, *p_tmp;

		kfree(p_hwfn->mcp_info->mfw_mb_cur);
		kfree(p_hwfn->mcp_info->mfw_mb_shadow);

		spin_lock_bh(&p_hwfn->mcp_info->cmd_lock);
		list_for_each_entry_safe(p_cmd_elem,
					 p_tmp,
					 &p_hwfn->mcp_info->cmd_list, list) {
			qed_mcp_cmd_del_elem(p_hwfn, p_cmd_elem);
		}
		spin_unlock_bh(&p_hwfn->mcp_info->cmd_lock);
	}

	kfree(p_hwfn->mcp_info);
	p_hwfn->mcp_info = NULL;

	return 0;
}

/* Maximum of 1 sec to wait for the SHMEM ready indication */
#define QED_MCP_SHMEM_RDY_MAX_RETRIES   20
#define QED_MCP_SHMEM_RDY_ITER_MS       50

#define MCP_REG_CACHE_PAGING_ENABLE_ENABLE_MASK	\
	MCP_REG_CACHE_PAGING_ENABLE_ENABLE

int qed_load_mcp_offsets(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mcp_info *p_info = p_hwfn->mcp_info;
	u32 drv_mb_offsize, mfw_mb_offsize, val;
	u8 cnt = QED_MCP_SHMEM_RDY_MAX_RETRIES;
	u8 msec = QED_MCP_SHMEM_RDY_ITER_MS;
	u32 mcp_pf_id = MCP_PF_ID(p_hwfn);
	u32 recovery_mode;

	/* Disabled cache paging in the MCP implies that no MFW is running */
	val = qed_rd(p_hwfn, p_ptt, MCP_REG_CACHE_PAGING_ENABLE);
	/* MISCS_REG_GENERIC_HW_0[31] is indication that the MCP/ROM is in
	 * recovery mode, serving NVM upgrade mailbox commands.
	 */
	recovery_mode = qed_rd(p_hwfn, p_ptt, MISCS_REG_GENERIC_HW_0) &
	    0x80000000;
	p_info->recovery_mode = recovery_mode ? true : false;
	p_info->b_mfw_running =
	    GET_FIELD(val, MCP_REG_CACHE_PAGING_ENABLE_ENABLE) || recovery_mode;
	if (!p_info->b_mfw_running) {
#ifndef ASIC_ONLY
		if (CHIP_REV_IS_EMUL(p_hwfn->cdev))
			DP_INFO(p_hwfn,
				"Emulation: Assume that no MFW is running [MCP_REG_CACHE_PAGING_ENABLE 0x%x]\n",
				val);
		else
#endif
			DP_NOTICE(p_hwfn,
				  "Assume that no MFW is running [MCP_REG_CACHE_PAGING_ENABLE 0x%x]\n",
				  val);
		return -EINVAL;
	}

	p_info->public_base = qed_rd(p_hwfn, p_ptt, MISC_REG_SHARED_MEM_ADDR);
	if (!p_info->public_base) {
		DP_NOTICE(p_hwfn,
			  "The address of the MCP scratch-pad is not configured\n");
		return -EINVAL;
	}

	p_info->public_base |= GRCBASE_MCP;

	/* Get the MFW MB address and number of supported messages */
	mfw_mb_offsize = qed_rd(p_hwfn, p_ptt,
				SECTION_OFFSIZE_ADDR(p_info->public_base,
						     PUBLIC_MFW_MB));
	p_info->mfw_mb_addr = SECTION_ADDR(mfw_mb_offsize, mcp_pf_id);
	p_info->mfw_mb_length = (u16) qed_rd(p_hwfn, p_ptt,
					     p_info->mfw_mb_addr +
					     offsetof(struct public_mfw_mb,
						      sup_msgs));

	/* @@@TBD:
	 * The driver can notify that there was an MCP reset, and might read the
	 * SHMEM values before the MFW has completed initializing them.
	 * As a temporary solution, the "sup_msgs" field in the MFW mailbox is
	 * used as a data ready indication.
	 * This should be replaced with an actual indication when it is provided
	 * by the MFW.
	 */
	if (!p_info->recovery_mode) {
		while (!p_info->mfw_mb_length && --cnt) {
			msleep(msec);
			p_info->mfw_mb_length =
			    (u16) qed_rd(p_hwfn, p_ptt,
					 p_info->mfw_mb_addr +
					 offsetof(struct public_mfw_mb,
						  sup_msgs));
		}

		if (!cnt) {
			DP_NOTICE(p_hwfn,
				  "Failed to get the SHMEM ready notification after %d msec\n",
				  QED_MCP_SHMEM_RDY_MAX_RETRIES * msec);
			return -EBUSY;
		}
	}

	/* Calculate the driver and MFW mailbox address */
	drv_mb_offsize = qed_rd(p_hwfn, p_ptt,
				SECTION_OFFSIZE_ADDR(p_info->public_base,
						     PUBLIC_DRV_MB));
	p_info->drv_mb_addr = SECTION_ADDR(drv_mb_offsize, mcp_pf_id);
	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "drv_mb_offsiz = 0x%x, drv_mb_addr = 0x%x mcp_pf_id = 0x%x\n",
		   drv_mb_offsize, p_info->drv_mb_addr, mcp_pf_id);

	/* Get the current driver mailbox sequence before sending
	 * the first command
	 */
	p_info->drv_mb_seq = DRV_MB_RD(p_hwfn, p_ptt, drv_mb_header) &
	    DRV_MSG_SEQ_NUMBER_MASK;

	/* Get current FW pulse sequence */
	p_info->drv_pulse_seq = DRV_MB_RD(p_hwfn, p_ptt, drv_pulse_mb) &
	    DRV_PULSE_SEQ_MASK;

	p_info->mcp_hist = qed_rd(p_hwfn, p_ptt, MISCS_REG_GENERIC_POR_0);

	return 0;
}

int qed_mcp_cmd_init(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mcp_info *p_info;
	u32 size;

	/* Allocate mcp_info structure */
	p_hwfn->mcp_info = kzalloc(sizeof(*p_hwfn->mcp_info), GFP_KERNEL);
	if (!p_hwfn->mcp_info) {
		DP_NOTICE(p_hwfn, "Failed to allocate mcp_info\n");
		return -ENOMEM;
	}
	p_info = p_hwfn->mcp_info;

	/* Initialize the MFW spinlocks */
	spin_lock_init(&p_info->cmd_lock);
	spin_lock_init(&p_info->link_lock);
	spin_lock_init(&p_info->dbg_data_lock);
	spin_lock_init(&p_info->unload_lock);

	INIT_LIST_HEAD(&p_info->cmd_list);

	if (qed_load_mcp_offsets(p_hwfn, p_ptt) != 0) {
		DP_NOTICE(p_hwfn, "MCP is not initialized\n");
		/* Do not free mcp_info here, since "public_base" indicates that
		 * the MCP is not initialized
		 */
		return 0;
	}

	size = MFW_DRV_MSG_MAX_DWORDS(p_info->mfw_mb_length) * sizeof(u32);
	p_info->mfw_mb_cur = kzalloc(size, GFP_KERNEL);
	p_info->mfw_mb_shadow = kzalloc(size, GFP_KERNEL);
	if (p_info->mfw_mb_cur == NULL || p_info->mfw_mb_shadow == NULL)
		goto err;

	return 0;

err:
	DP_NOTICE(p_hwfn, "Failed to allocate mcp memory\n");
	qed_mcp_free(p_hwfn);
	return -ENOMEM;
}

static void qed_mcp_reread_offsets(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt)
{
	u32 generic_por_0 = qed_rd(p_hwfn, p_ptt, MISCS_REG_GENERIC_POR_0);

	/* Use MCP history register to check if MCP reset occurred between init
	 * time and now.
	 */
	if (p_hwfn->mcp_info->mcp_hist != generic_por_0) {
		DP_VERBOSE(p_hwfn,
			   QED_MSG_SP,
			   "Rereading MCP offsets [mcp_hist 0x%08x, generic_por_0 0x%08x]\n",
			   p_hwfn->mcp_info->mcp_hist, generic_por_0);

		qed_load_mcp_offsets(p_hwfn, p_ptt);
		qed_mcp_cmd_port_init(p_hwfn, p_ptt);
	}
}

int qed_mcp_reset(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 prev_generic_por_0, seq, delay = QED_MCP_RESP_ITER_US, cnt = 0;
	u32 retries = QED_MCP_RESET_RETRIES;
	int rc = 0;

#if (!defined ASIC_ONLY) && (!defined ROM_TEST)
	if (CHIP_REV_IS_SLOW(p_hwfn->cdev)) {
		delay = QED_EMUL_MCP_RESP_ITER_US;
		retries = QED_EMUL_MCP_RESET_RETRIES;
	}
#endif
	if (p_hwfn->mcp_info->b_block_cmd) {
		DP_NOTICE(p_hwfn,
			  "The MFW is not responsive. Avoid sending MCP_RESET mailbox command.\n");
		return -EBUSY;
	}

	/* Ensure that only a single thread is accessing the mailbox */
	spin_lock_bh(&p_hwfn->mcp_info->cmd_lock);

	prev_generic_por_0 = qed_rd(p_hwfn, p_ptt, MISCS_REG_GENERIC_POR_0);

	/* Set drv command along with the updated sequence */
	qed_mcp_reread_offsets(p_hwfn, p_ptt);
	seq = ++p_hwfn->mcp_info->drv_mb_seq;
	DRV_MB_WR(p_hwfn, p_ptt, drv_mb_header, (DRV_MSG_CODE_MCP_RESET | seq));

	/* Give the MFW up to 500 second (50*1000*10usec) to resume */
	do {
		udelay(delay);

		if (qed_rd(p_hwfn, p_ptt, MISCS_REG_GENERIC_POR_0) !=
		    prev_generic_por_0)
			break;
	} while (cnt++ < retries);

	if (qed_rd(p_hwfn, p_ptt, MISCS_REG_GENERIC_POR_0) !=
	    prev_generic_por_0) {
		DP_VERBOSE(p_hwfn, QED_MSG_SP,
			   "MCP was reset after %d usec\n", cnt * delay);
	} else {
		DP_ERR(p_hwfn, "Failed to reset MCP\n");
		rc = -EAGAIN;
	}

	spin_unlock_bh(&p_hwfn->mcp_info->cmd_lock);

	return rc;
}

#ifndef ASIC_ONLY
static void qed_emul_mcp_load_req(struct qed_hwfn *p_hwfn,
				  struct qed_mcp_mb_params *p_mb_params)
{
	if (GET_MFW_FIELD(p_mb_params->param, DRV_ID_MCP_HSI_VER) !=
	    1 /* QED_LOAD_REQ_HSI_VER_1 */ ) {
		p_mb_params->mcp_resp = FW_MSG_CODE_DRV_LOAD_REFUSED_HSI_1;
		return;
	}

	if (!loaded)
		p_mb_params->mcp_resp = FW_MSG_CODE_DRV_LOAD_ENGINE;
	else if (!loaded_port[p_hwfn->port_id])
		p_mb_params->mcp_resp = FW_MSG_CODE_DRV_LOAD_PORT;
	else
		p_mb_params->mcp_resp = FW_MSG_CODE_DRV_LOAD_FUNCTION;

	/* On CMT, always tell that it's engine */
	if (QED_IS_CMT(p_hwfn->cdev))
		p_mb_params->mcp_resp = FW_MSG_CODE_DRV_LOAD_ENGINE;

	loaded++;
	loaded_port[p_hwfn->port_id]++;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "Load phase: 0x%08x load cnt: 0x%x port id=%d port_load=%d\n",
		   p_mb_params->mcp_resp,
		   loaded, p_hwfn->port_id, loaded_port[p_hwfn->port_id]);
}

static void qed_emul_mcp_unload_req(struct qed_hwfn *p_hwfn)
{
	loaded--;
	loaded_port[p_hwfn->port_id]--;
	DP_VERBOSE(p_hwfn, QED_MSG_SP, "Unload cnt: 0x%x\n", loaded);
}

static int
qed_emul_mcp_cmd(struct qed_hwfn *p_hwfn, struct qed_mcp_mb_params *p_mb_params)
{
	if (!CHIP_REV_IS_EMUL(p_hwfn->cdev))
		return -EINVAL;

	switch (p_mb_params->cmd) {
	case DRV_MSG_CODE_LOAD_REQ:
		qed_emul_mcp_load_req(p_hwfn, p_mb_params);
		break;
	case DRV_MSG_CODE_UNLOAD_REQ:
		qed_emul_mcp_unload_req(p_hwfn);
		break;
	case DRV_MSG_CODE_GET_MFW_FEATURE_SUPPORT:
	case DRV_MSG_CODE_RESOURCE_CMD:
	case DRV_MSG_CODE_MDUMP_CMD:
	case DRV_MSG_CODE_GET_ENGINE_CONFIG:
	case DRV_MSG_CODE_GET_PPFID_BITMAP:
		return -EOPNOTSUPP;
	default:
		break;
	}

	return 0;
}
#endif

/* Must be called while cmd_lock is acquired */
static bool qed_mcp_has_pending_cmd(struct qed_hwfn *p_hwfn)
{
	struct qed_mcp_cmd_elem *p_cmd_elem = NULL;

	/* There is at most one pending command at a certain time, and if it
	 * exists - it is placed at the HEAD of the list.
	 */
	if (!list_empty(&p_hwfn->mcp_info->cmd_list)) {
		p_cmd_elem = list_first_entry(&p_hwfn->mcp_info->cmd_list,
					      struct qed_mcp_cmd_elem, list);
		return !p_cmd_elem->b_is_completed;
	}

	return false;
}

/* Must be called while cmd_lock is acquired */
static int
qed_mcp_update_pending_cmd(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mcp_mb_params *p_mb_params;
	struct qed_mcp_cmd_elem *p_cmd_elem;
	u32 mcp_resp;
	u16 seq_num;

	mcp_resp = DRV_MB_RD(p_hwfn, p_ptt, fw_mb_header);
	seq_num = (u16) (mcp_resp & FW_MSG_SEQ_NUMBER_MASK);

	/* Return if no new non-handled response has been received */
	if (seq_num != p_hwfn->mcp_info->drv_mb_seq)
		return -EAGAIN;

	p_cmd_elem = qed_mcp_cmd_get_elem(p_hwfn, seq_num);
	if (!p_cmd_elem) {
		DP_ERR(p_hwfn,
		       "Failed to find a pending mailbox cmd that expects sequence number %d\n",
		       seq_num);
		return -EINVAL;
	}

	p_mb_params = p_cmd_elem->p_mb_params;

	/* Get the MFW response along with the sequence number */
	p_mb_params->mcp_resp = mcp_resp;

	/* Get the MFW param */
	p_mb_params->mcp_param = DRV_MB_RD(p_hwfn, p_ptt, fw_mb_param);

	/* Get the union data */
	if (p_mb_params->p_data_dst != NULL && p_mb_params->data_dst_size) {
		u32 union_data_addr = p_hwfn->mcp_info->drv_mb_addr +
		    offsetof(struct public_drv_mb,
			     union_data);
		qed_memcpy_from(p_hwfn, p_ptt, p_mb_params->p_data_dst,
				union_data_addr, p_mb_params->data_dst_size);
	}

	p_cmd_elem->b_is_completed = true;

	return 0;
}

/* Skip these mbx cmds from printing in internal log */
static inline u32 qed_mcp_get_dp_module(u32 cmd)
{
	if ((cmd == DRV_MSG_CODE_NVM_READ_NVRAM) ||
	    (cmd == DRV_MSG_CODE_NVM_GET_FILE_ATT) ||
	    (cmd == DRV_MSG_CODE_BIST_TEST) || (cmd == DRV_MSG_CODE_MDUMP_CMD))
		return QED_MSG_EXTRA;
	else
		return QED_MSG_SP;
}

/* Must be called while cmd_lock is acquired */
static void __qed_mcp_cmd_and_union(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt,
				    struct qed_mcp_mb_params *p_mb_params,
				    u16 seq_num)
{
	union drv_union_data union_data;
	u32 union_data_addr;

	/* Set the union data */
	union_data_addr = p_hwfn->mcp_info->drv_mb_addr +
	    offsetof(struct public_drv_mb, union_data);
	memset(&union_data, 0, sizeof(union_data));
	if (p_mb_params->p_data_src != NULL && p_mb_params->data_src_size)
		memcpy(&union_data, p_mb_params->p_data_src,
		       p_mb_params->data_src_size);
	qed_memcpy_to(p_hwfn, p_ptt, union_data_addr, &union_data,
		      sizeof(union_data));

	/* Set the drv param */
	DRV_MB_WR(p_hwfn, p_ptt, drv_mb_param, p_mb_params->param);

	/* Set the drv command along with the sequence number */
	DRV_MB_WR(p_hwfn, p_ptt, drv_mb_header, (p_mb_params->cmd | seq_num));

	DP_VERBOSE(p_hwfn, qed_mcp_get_dp_module(p_mb_params->cmd),
		   "MFW mailbox: %s (0x%08x) param 0x%08x, flags 0x%x\n",
		   qed_mcp_print_cmd_name(p_mb_params->cmd),
		   (p_mb_params->cmd | seq_num), p_mb_params->param,
		   p_mb_params->flags);
}

static void qed_mcp_cmd_set_blocking(struct qed_hwfn *p_hwfn, bool block_cmd)
{
	p_hwfn->mcp_info->b_block_cmd = block_cmd;

	DP_INFO(p_hwfn, "%s sending of mailbox commands to the MFW\n",
		block_cmd ? "Block" : "Unblock");
}

static void qed_mcp_cmd_set_halt(struct qed_hwfn *p_hwfn, bool halt)
{
	p_hwfn->mcp_info->b_halted = halt;

	DP_INFO(p_hwfn, "%s sending of mailbox commands to the MFW\n",
		halt ? "Halt" : "Unhalt");
}

static void qed_mcp_print_cpu_info(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt)
{
	u32 cpu_mode, cpu_state, cpu_pc_0, cpu_pc_1, cpu_pc_2;
	u32 delay = QED_MCP_RESP_ITER_US;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->cdev))
		delay = QED_EMUL_MCP_RESP_ITER_US;
#endif
	cpu_mode = qed_rd(p_hwfn, p_ptt, MCP_REG_CPU_MODE);
	cpu_state = qed_rd(p_hwfn, p_ptt, MCP_REG_CPU_STATE);
	cpu_pc_0 = qed_rd(p_hwfn, p_ptt, MCP_REG_CPU_PROGRAM_COUNTER);
	udelay(delay);
	cpu_pc_1 = qed_rd(p_hwfn, p_ptt, MCP_REG_CPU_PROGRAM_COUNTER);
	udelay(delay);
	cpu_pc_2 = qed_rd(p_hwfn, p_ptt, MCP_REG_CPU_PROGRAM_COUNTER);

	DP_NOTICE(p_hwfn,
		  "MCP CPU info: mode 0x%08x, state 0x%08x, pc {0x%08x, 0x%08x, 0x%08x}\n",
		  cpu_mode, cpu_state, cpu_pc_0, cpu_pc_1, cpu_pc_2);
}

static int
_qed_mcp_cmd_and_union(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       struct qed_mcp_mb_params *p_mb_params,
		       u32 max_retries, u32 usecs)
{
	u32 cnt = 0, msecs = DIV_ROUND_UP(usecs, 1000);
	struct qed_mcp_cmd_elem *p_cmd_elem;
	u32 ud_time = 10, ud_iter = 100;
	u16 seq_num;
	int rc = 0;

	if (!p_mb_params)
		return -EINVAL;

	if (REG_RD(p_hwfn, PXP_PF_ME_OPAQUE_ADDR) == 0xffffffff) {
		DP_ERR(p_hwfn,
		       "Reading the ME register returns all Fs; Preventing further chip access\n");
		return -EAGAIN;
	}

	/* First 100 udelay wait iterations equates to 1ms wait time */
	if (QED_MB_FLAGS_IS_SET(p_mb_params, CAN_SLEEP))
		max_retries += ud_iter - 1;

	/* Wait until the mailbox is non-occupied */
	do {
		/* Exit the loop if there is no pending command, or if the
		 * pending command is completed during this iteration.
		 * The spinlock stays locked until the command is sent.
		 */

		spin_lock_bh(&p_hwfn->mcp_info->cmd_lock);

		if (!qed_mcp_has_pending_cmd(p_hwfn))
			break;

		rc = qed_mcp_update_pending_cmd(p_hwfn, p_ptt);
		if (!rc)
			break;
		else if (rc != -EAGAIN)
			goto err;

		spin_unlock_bh(&p_hwfn->mcp_info->cmd_lock);

		if (QED_MB_FLAGS_IS_SET(p_mb_params, CAN_SLEEP)) {
			if (cnt < ud_iter)
				udelay(ud_time);
			else
				msleep(msecs);
		} else {
			udelay(usecs);
		}
	} while (++cnt < max_retries);

	if (cnt >= max_retries) {
		DP_NOTICE(p_hwfn,
			  "The MFW mailbox is occupied by an uncompleted cmd. Failed to send %s (0x%08x) [param 0x%08x].\n",
			  qed_mcp_print_cmd_name(p_mb_params->cmd),
			  p_mb_params->cmd, p_mb_params->param);
		return -EAGAIN;
	}

	/* Send the mailbox command */
	qed_mcp_reread_offsets(p_hwfn, p_ptt);
	seq_num = ++p_hwfn->mcp_info->drv_mb_seq;
	p_cmd_elem = qed_mcp_cmd_add_elem(p_hwfn, p_mb_params, seq_num);
	if (!p_cmd_elem) {
		rc = -ENOMEM;
		goto err;
	}

	__qed_mcp_cmd_and_union(p_hwfn, p_ptt, p_mb_params, seq_num);
	spin_unlock_bh(&p_hwfn->mcp_info->cmd_lock);

	/* Wait for the MFW response */
	do {
		/* Exit the loop if the command is already completed, or if the
		 * command is completed during this iteration.
		 * The spinlock stays locked until the list element is removed.
		 */

		if (QED_MB_FLAGS_IS_SET(p_mb_params, CAN_SLEEP)) {
			if (cnt < ud_iter)
				udelay(ud_time);
			else
				msleep(msecs);
		} else {
			udelay(usecs);
		}

		spin_lock_bh(&p_hwfn->mcp_info->cmd_lock);

		if (p_cmd_elem->b_is_completed)
			break;

		rc = qed_mcp_update_pending_cmd(p_hwfn, p_ptt);
		if (!rc)
			break;
		else if (rc != -EAGAIN)
			goto err;

		spin_unlock_bh(&p_hwfn->mcp_info->cmd_lock);
	} while (++cnt < max_retries);

	if (cnt >= max_retries) {
		qed_mcp_print_cpu_info(p_hwfn, p_ptt);

		spin_lock_bh(&p_hwfn->mcp_info->cmd_lock);
		qed_mcp_cmd_del_elem(p_hwfn, p_cmd_elem);
		spin_unlock_bh(&p_hwfn->mcp_info->cmd_lock);

		if (!QED_MB_FLAGS_IS_SET(p_mb_params, AVOID_BLOCK))
			qed_mcp_cmd_set_blocking(p_hwfn, true);

		qed_hw_err_notify(p_hwfn,
				  p_ptt,
				  QED_HW_ERR_MFW_RESP_FAIL,
				  "The MFW failed to respond to command 0x%08x [param 0x%08x].\n",
				  p_mb_params->cmd, p_mb_params->param);

		return -EAGAIN;
	}

	qed_mcp_cmd_del_elem(p_hwfn, p_cmd_elem);
	spin_unlock_bh(&p_hwfn->mcp_info->cmd_lock);

	DP_VERBOSE(p_hwfn, qed_mcp_get_dp_module(p_mb_params->cmd),
		   "MFW mailbox: response 0x%08x param 0x%08x [after %d.%03d ms]\n",
		   p_mb_params->mcp_resp, p_mb_params->mcp_param,
		   (cnt * usecs) / 1000, (cnt * usecs) % 1000);

	/* Clear the sequence number from the MFW response */
	p_mb_params->mcp_resp &= FW_MSG_CODE_MASK;

	return 0;

err:
	spin_unlock_bh(&p_hwfn->mcp_info->cmd_lock);
	return rc;
}

static int qed_mcp_cmd_and_union(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt,
				 struct qed_mcp_mb_params *p_mb_params)
{
	size_t union_data_size = sizeof(union drv_union_data);
	u32 max_retries = QED_DRV_MB_MAX_RETRIES;
	u32 usecs = QED_MCP_RESP_ITER_US;

	if (!p_mb_params)
		return -EINVAL;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->cdev) && !qed_mcp_is_init(p_hwfn))
		return qed_emul_mcp_cmd(p_hwfn, p_mb_params);

	if (CHIP_REV_IS_SLOW(p_hwfn->cdev)) {
		max_retries = QED_EMUL_DRV_MB_MAX_RETRIES;
		usecs = QED_EMUL_MCP_RESP_ITER_US;
	}
#endif
	/* MCP not initialized */
	if (!qed_mcp_is_init(p_hwfn)) {
		DP_NOTICE(p_hwfn, "MFW is not initialized!\n");
		return -EBUSY;
	}

	/* If MFW was halted by another process, let the mailbox sender know
	 * they can retry sending the mailbox later.
	 */
	if (p_hwfn->mcp_info->b_halted) {
		DP_VERBOSE(p_hwfn,
			   QED_MSG_SP,
			   "The MFW is halted. Avoid sending %s (0x%08x) [param 0x%08x].\n",
			   qed_mcp_print_cmd_name(p_mb_params->cmd),
			   p_mb_params->cmd, p_mb_params->param);
		return -EAGAIN;
	}

	if (p_hwfn->mcp_info->b_block_cmd) {
		DP_NOTICE(p_hwfn,
			  "The MFW is not responsive. Avoid sending %s (0x%08x) [param 0x%08x].\n",
			  qed_mcp_print_cmd_name(p_mb_params->cmd),
			  p_mb_params->cmd, p_mb_params->param);
		return -EBUSY;
	}

	if (p_mb_params->data_src_size > union_data_size ||
	    p_mb_params->data_dst_size > union_data_size) {
		DP_ERR(p_hwfn,
		       "The provided size is larger than the union data size [src_size %u, dst_size %u, union_data_size %zu]\n",
		       p_mb_params->data_src_size,
		       p_mb_params->data_dst_size, union_data_size);
		return -EINVAL;
	}

	if (QED_MB_FLAGS_IS_SET(p_mb_params, CAN_SLEEP)) {
		max_retries = DIV_ROUND_UP(max_retries, 1000);
		usecs *= 1000;
	}

	return _qed_mcp_cmd_and_union(p_hwfn, p_ptt, p_mb_params, max_retries,
				      usecs);
}

static int _qed_mcp_cmd(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt,
			u32 cmd,
			u32 param,
			u32 * o_mcp_resp, u32 * o_mcp_param, bool can_sleep)
{
	struct qed_mcp_mb_params mb_params;
	int rc;

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = cmd;
	mb_params.param = param;
	mb_params.flags = can_sleep ? QED_MB_FLAG_CAN_SLEEP : 0;
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		return rc;

	*o_mcp_resp = mb_params.mcp_resp;
	*o_mcp_param = mb_params.mcp_param;

	return 0;
}

int qed_mcp_cmd(struct qed_hwfn *p_hwfn,
		struct qed_ptt *p_ptt,
		u32 cmd, u32 param, u32 * o_mcp_resp, u32 * o_mcp_param)
{
	return _qed_mcp_cmd(p_hwfn, p_ptt, cmd, param, o_mcp_resp,
			    o_mcp_param, true);
}

int qed_mcp_cmd_nosleep(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt,
			u32 cmd, u32 param, u32 * o_mcp_resp, u32 * o_mcp_param)
{
	return _qed_mcp_cmd(p_hwfn, p_ptt, cmd, param, o_mcp_resp,
			    o_mcp_param, false);
}

int qed_mcp_nvm_wr_cmd(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       u32 cmd,
		       u32 param,
		       u32 * o_mcp_resp,
		       u32 * o_mcp_param,
		       u32 i_txn_size, u32 * i_buf, bool b_can_sleep)
{
	struct qed_mcp_mb_params mb_params;
	int rc;

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = cmd;
	mb_params.param = param;
	mb_params.p_data_src = i_buf;
	mb_params.data_src_size = (u8) i_txn_size;
	if (b_can_sleep)
		mb_params.flags = QED_MB_FLAG_CAN_SLEEP;
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		return rc;

	*o_mcp_resp = mb_params.mcp_resp;
	*o_mcp_param = mb_params.mcp_param;

	/* nvm_info needs to be updated */
	p_hwfn->nvm_info.valid = false;

	return 0;
}

int qed_mcp_nvm_rd_cmd(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       u32 cmd,
		       u32 param,
		       u32 * o_mcp_resp,
		       u32 * o_mcp_param,
		       u32 * o_txn_size, u32 * o_buf, bool b_can_sleep)
{
	struct qed_mcp_mb_params mb_params;
	u8 raw_data[MCP_DRV_NVM_BUF_LEN];
	int rc;

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = cmd;
	mb_params.param = param;
	mb_params.p_data_dst = raw_data;

	/* Use the maximal value since the actual one is part of the response */
	mb_params.data_dst_size = MCP_DRV_NVM_BUF_LEN;
	if (b_can_sleep)
		mb_params.flags = QED_MB_FLAG_CAN_SLEEP;

	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		return rc;

	*o_mcp_resp = mb_params.mcp_resp;
	*o_mcp_param = mb_params.mcp_param;

	*o_txn_size = *o_mcp_param;
	memcpy(o_buf, raw_data, *o_txn_size);

	return 0;
}

static bool
qed_mcp_can_force_load(u8 drv_role,
		       u8 exist_drv_role,
		       enum qed_override_force_load override_force_load)
{
	bool can_force_load = false;

	switch (override_force_load) {
	case QED_OVERRIDE_FORCE_LOAD_ALWAYS:
		can_force_load = true;
		break;
	case QED_OVERRIDE_FORCE_LOAD_NEVER:
		can_force_load = false;
		break;
	default:
		can_force_load = (drv_role == DRV_ROLE_OS &&
				  exist_drv_role == DRV_ROLE_PREBOOT) ||
		    (drv_role == DRV_ROLE_KDUMP &&
		     exist_drv_role == DRV_ROLE_OS);
		break;
	}

	return can_force_load;
}

int qed_mcp_cancel_load_req(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 resp = 0, param = 0;
	int rc;

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_CANCEL_LOAD_REQ, 0,
			 &resp, &param);
	if (rc) {
		DP_NOTICE(p_hwfn,
			  "Failed to send cancel load request, rc = %d\n", rc);
		return rc;
	}

	if (resp == FW_MSG_CODE_UNSUPPORTED) {
		DP_INFO(p_hwfn,
			"The cancel load command is unsupported by the MFW\n");
		return -EOPNOTSUPP;
	}

	return 0;
}

#define CONFIG_QEDE_BITMAP_IDX  (0x1 << 0)
#define CONFIG_QED_SRIOV_BITMAP_IDX     (0x1 << 1)
#define CONFIG_QED_RDMA_BITMAP_IDX      (0x1 << 2)
#define CONFIG_IWARP_BITMAP_IDX (0x1 << 3)
#define CONFIG_QED_FCOE_BITMAP_IDX      (0x1 << 4)
#define CONFIG_QEDI_BITMAP_IDX  (0x1 << 5)
#define CONFIG_QED_LL2_BITMAP_IDX       (0x1 << 6)

static u32 qed_get_config_bitmap(void)
{
	u32 config_bitmap = 0x0;

#ifdef CONFIG_QEDE
	config_bitmap |= CONFIG_QEDE_BITMAP_IDX;
#endif
#ifdef CONFIG_QED_SRIOV
	config_bitmap |= CONFIG_QED_SRIOV_BITMAP_IDX;
#endif
#if IS_ENABLED(CONFIG_QED_RDMA)
	config_bitmap |= CONFIG_QED_RDMA_BITMAP_IDX;
#endif
#ifdef CONFIG_IWARP
	config_bitmap |= CONFIG_IWARP_BITMAP_IDX;
#endif
#if IS_ENABLED(CONFIG_QED_FCOE)
	config_bitmap |= CONFIG_QED_FCOE_BITMAP_IDX;
#endif
#if IS_ENABLED(CONFIG_QEDI)
	config_bitmap |= CONFIG_QEDI_BITMAP_IDX;
#endif
#ifdef CONFIG_QED_LL2
	config_bitmap |= CONFIG_QED_LL2_BITMAP_IDX;
#endif

	return config_bitmap;
}

struct qed_load_req_in_params {
	u8 hsi_ver;
#define QED_LOAD_REQ_HSI_VER_DEFAULT    0
#define QED_LOAD_REQ_HSI_VER_1  1
	u32 drv_ver_0;
	u32 drv_ver_1;
	u32 fw_ver;
	u8 drv_role;
	u8 timeout_val;
	u8 force_cmd;
	bool avoid_eng_reset;
};

struct qed_load_req_out_params {
	u32 load_code;
	u32 exist_drv_ver_0;
	u32 exist_drv_ver_1;
	u32 exist_fw_ver;
	u8 exist_drv_role;
	u8 mfw_hsi_ver;
	bool drv_exists;
};

static int
__qed_mcp_load_req(struct qed_hwfn *p_hwfn,
		   struct qed_ptt *p_ptt,
		   struct qed_load_req_in_params *p_in_params,
		   struct qed_load_req_out_params *p_out_params)
{
	struct qed_mcp_mb_params mb_params;
	struct load_req_stc load_req;
	struct load_rsp_stc load_rsp;
	int rc;

	memset(&load_req, 0, sizeof(load_req));
	load_req.drv_ver_0 = p_in_params->drv_ver_0;
	load_req.drv_ver_1 = p_in_params->drv_ver_1;
	load_req.fw_ver = p_in_params->fw_ver;
	SET_MFW_FIELD(load_req.misc0, LOAD_REQ_ROLE, p_in_params->drv_role);
	SET_MFW_FIELD(load_req.misc0, LOAD_REQ_LOCK_TO,
		      p_in_params->timeout_val);
	SET_MFW_FIELD(load_req.misc0, LOAD_REQ_FORCE, p_in_params->force_cmd);
	SET_MFW_FIELD(load_req.misc0, LOAD_REQ_FLAGS0,
		      p_in_params->avoid_eng_reset);

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_LOAD_REQ;
	mb_params.param = p_hwfn->cdev->drv_type;
	SET_MFW_FIELD(mb_params.param, DRV_ID_PDA_COMP_VER, PDA_COMP);
	SET_MFW_FIELD(mb_params.param, DRV_ID_MCP_HSI_VER,
		      p_in_params->hsi_ver == QED_LOAD_REQ_HSI_VER_DEFAULT ?
		      LOAD_REQ_HSI_VERSION : p_in_params->hsi_ver);
	mb_params.p_data_src = &load_req;
	mb_params.data_src_size = sizeof(load_req);
	mb_params.p_data_dst = &load_rsp;
	mb_params.data_dst_size = sizeof(load_rsp);
	mb_params.flags = QED_MB_FLAG_CAN_SLEEP | QED_MB_FLAG_AVOID_BLOCK;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "Load Request: param 0x%08x [init_hw %d, drv_type %d, hsi_ver %d, pda 0x%04x]\n",
		   mb_params.param,
		   GET_MFW_FIELD(mb_params.param, DRV_ID_DRV_INIT_HW),
		   GET_MFW_FIELD(mb_params.param, DRV_ID_DRV_TYPE),
		   GET_MFW_FIELD(mb_params.param, DRV_ID_MCP_HSI_VER),
		   GET_MFW_FIELD(mb_params.param, DRV_ID_PDA_COMP_VER));

	if (p_in_params->hsi_ver != QED_LOAD_REQ_HSI_VER_1) {
		DP_VERBOSE(p_hwfn,
			   QED_MSG_SP,
			   "Load Request: drv_ver 0x%08x_0x%08x, fw_ver 0x%08x, misc0 0x%08x [role %d, timeout %d, force %d, flags0 0x%x]\n",
			   load_req.drv_ver_0,
			   load_req.drv_ver_1,
			   load_req.fw_ver,
			   load_req.misc0,
			   GET_MFW_FIELD(load_req.misc0, LOAD_REQ_ROLE),
			   GET_MFW_FIELD(load_req.misc0, LOAD_REQ_LOCK_TO),
			   GET_MFW_FIELD(load_req.misc0, LOAD_REQ_FORCE),
			   GET_MFW_FIELD(load_req.misc0, LOAD_REQ_FLAGS0));
	}

	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed to send load request, rc = %d\n", rc);
		return rc;
	}

	DP_VERBOSE(p_hwfn, QED_MSG_SP,
		   "Load Response: resp 0x%08x\n", mb_params.mcp_resp);
	p_out_params->load_code = mb_params.mcp_resp;

	if (p_in_params->hsi_ver != QED_LOAD_REQ_HSI_VER_1 &&
	    p_out_params->load_code != FW_MSG_CODE_DRV_LOAD_REFUSED_HSI_1) {
		DP_VERBOSE(p_hwfn,
			   QED_MSG_SP,
			   "Load Response: exist_drv_ver 0x%08x_0x%08x, exist_fw_ver 0x%08x, misc0 0x%08x [exist_role %d, mfw_hsi %d, flags0 0x%x]\n",
			   load_rsp.drv_ver_0,
			   load_rsp.drv_ver_1,
			   load_rsp.fw_ver,
			   load_rsp.misc0,
			   GET_MFW_FIELD(load_rsp.misc0, LOAD_RSP_ROLE),
			   GET_MFW_FIELD(load_rsp.misc0, LOAD_RSP_HSI),
			   GET_MFW_FIELD(load_rsp.misc0, LOAD_RSP_FLAGS0));

		p_out_params->exist_drv_ver_0 = load_rsp.drv_ver_0;
		p_out_params->exist_drv_ver_1 = load_rsp.drv_ver_1;
		p_out_params->exist_fw_ver = load_rsp.fw_ver;
		p_out_params->exist_drv_role =
		    GET_MFW_FIELD(load_rsp.misc0, LOAD_RSP_ROLE);
		p_out_params->mfw_hsi_ver =
		    GET_MFW_FIELD(load_rsp.misc0, LOAD_RSP_HSI);
		p_out_params->drv_exists =
		    GET_MFW_FIELD(load_rsp.misc0, LOAD_RSP_FLAGS0) &
		    LOAD_RSP_FLAGS0_DRV_EXISTS;
	}

	return 0;
}

static void qed_get_mfw_drv_role(enum qed_drv_role drv_role,
				 u8 * p_mfw_drv_role)
{
	switch (drv_role) {
	case QED_DRV_ROLE_OS:
		*p_mfw_drv_role = DRV_ROLE_OS;
		break;
	case QED_DRV_ROLE_KDUMP:
		*p_mfw_drv_role = DRV_ROLE_KDUMP;
		break;
	}
}

enum qed_load_req_force {
	QED_LOAD_REQ_FORCE_NONE,
	QED_LOAD_REQ_FORCE_PF,
	QED_LOAD_REQ_FORCE_ALL,
};

static void qed_get_mfw_force_cmd(enum qed_load_req_force force_cmd,
				  u8 * p_mfw_force_cmd)
{
	switch (force_cmd) {
	case QED_LOAD_REQ_FORCE_NONE:
		*p_mfw_force_cmd = LOAD_REQ_FORCE_NONE;
		break;
	case QED_LOAD_REQ_FORCE_PF:
		*p_mfw_force_cmd = LOAD_REQ_FORCE_PF;
		break;
	case QED_LOAD_REQ_FORCE_ALL:
		*p_mfw_force_cmd = LOAD_REQ_FORCE_ALL;
		break;
	}
}

int qed_mcp_load_req(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt,
		     struct qed_load_req_params *p_params)
{
	struct qed_load_req_out_params out_params;
	struct qed_load_req_in_params in_params;
	u8 mfw_drv_role = 0, mfw_force_cmd;
	int rc;

	memset(&in_params, 0, sizeof(in_params));
	in_params.hsi_ver = QED_LOAD_REQ_HSI_VER_DEFAULT;
	in_params.drv_ver_0 = QED_VERSION;
	in_params.drv_ver_1 = qed_get_config_bitmap();
	in_params.fw_ver = STORM_FW_VERSION;
	qed_get_mfw_drv_role(p_params->drv_role, &mfw_drv_role);
	in_params.drv_role = mfw_drv_role;
	in_params.timeout_val = p_params->timeout_val;
	qed_get_mfw_force_cmd(QED_LOAD_REQ_FORCE_NONE, &mfw_force_cmd);
	in_params.force_cmd = mfw_force_cmd;
	in_params.avoid_eng_reset = p_params->avoid_eng_reset;

	memset(&out_params, 0, sizeof(out_params));
	rc = __qed_mcp_load_req(p_hwfn, p_ptt, &in_params, &out_params);
	if (rc)
		return rc;

	/* First handle cases where another load request should/might be sent:
	 * - MFW expects the old interface [HSI version = 1]
	 * - MFW responds that a force load request is required
	 */
	if (out_params.load_code == FW_MSG_CODE_DRV_LOAD_REFUSED_HSI_1) {
		DP_INFO(p_hwfn,
			"MFW refused a load request due to HSI > 1. Resending with HSI = 1.\n");

		in_params.hsi_ver = QED_LOAD_REQ_HSI_VER_1;
		memset(&out_params, 0, sizeof(out_params));
		rc = __qed_mcp_load_req(p_hwfn, p_ptt, &in_params, &out_params);
		if (rc)
			return rc;
	} else if (out_params.load_code ==
		   FW_MSG_CODE_DRV_LOAD_REFUSED_REQUIRES_FORCE) {
		if (qed_mcp_can_force_load(in_params.drv_role,
					   out_params.exist_drv_role,
					   p_params->override_force_load)) {
			DP_INFO(p_hwfn,
				"A force load is required [{role, fw_ver, drv_ver}: loading={%d, 0x%08x, 0x%08x_%08x}, existing={%d, 0x%08x, 0x%08x_%08x}]\n",
				in_params.drv_role,
				in_params.fw_ver,
				in_params.drv_ver_0,
				in_params.drv_ver_1,
				out_params.exist_drv_role,
				out_params.exist_fw_ver,
				out_params.exist_drv_ver_0,
				out_params.exist_drv_ver_1);

			qed_get_mfw_force_cmd(QED_LOAD_REQ_FORCE_ALL,
					      &mfw_force_cmd);

			in_params.force_cmd = mfw_force_cmd;
			memset(&out_params, 0, sizeof(out_params));
			rc = __qed_mcp_load_req(p_hwfn, p_ptt, &in_params,
						&out_params);
			if (rc)
				return rc;
		} else {
			DP_NOTICE(p_hwfn,
				  "A force load is required [{role, fw_ver, drv_ver}: loading={%d, 0x%08x, 0x%08x_%08x}, existing={%d, 0x%08x, 0x%08x_%08x}] - Avoid\n",
				  in_params.drv_role,
				  in_params.fw_ver,
				  in_params.drv_ver_0,
				  in_params.drv_ver_1,
				  out_params.exist_drv_role,
				  out_params.exist_fw_ver,
				  out_params.exist_drv_ver_0,
				  out_params.exist_drv_ver_1);

			qed_mcp_cancel_load_req(p_hwfn, p_ptt);
			return -EBUSY;
		}
	}

	/* Now handle the other types of responses.
	 * The "REFUSED_HSI_1" and "REFUSED_REQUIRES_FORCE" responses are not
	 * expected here after the additional revised load requests were sent.
	 */
	switch (out_params.load_code) {
	case FW_MSG_CODE_DRV_LOAD_ENGINE:
	case FW_MSG_CODE_DRV_LOAD_PORT:
	case FW_MSG_CODE_DRV_LOAD_FUNCTION:
		if (out_params.mfw_hsi_ver != QED_LOAD_REQ_HSI_VER_1 &&
		    out_params.drv_exists && !QED_RECOV_IN_PROG(p_hwfn->cdev)) {
			/* The role and fw/driver version match, but the PF is
			 * already loaded and has not been unloaded gracefully.
			 * This is unexpected since a quasi-FLR request was
			 * previously sent as part of qed_hw_prepare().
			 */
			DP_NOTICE(p_hwfn,
				  "PF is already loaded - shouldn't have got here since a quasi-FLR request was previously sent!\n");
			return -EINVAL;
		}
		break;
	default:
		DP_NOTICE(p_hwfn,
			  "Unexpected refusal to load request [resp 0x%08x]. Aborting.\n",
			  out_params.load_code);
		return -EBUSY;
	}

	p_params->load_code = out_params.load_code;

	return 0;
}

int qed_mcp_load_done(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 resp = 0, param = 0;
	int rc;

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_LOAD_DONE, 0, &resp,
			 &param);
	if (rc) {
		DP_NOTICE(p_hwfn,
			  "Failed to send a LOAD_DONE command, rc = %d\n", rc);
		return rc;
	}

	if (resp == FW_MSG_CODE_DRV_LOAD_REFUSED_REJECT) {
		DP_NOTICE(p_hwfn,
			  "Received a LOAD_REFUSED_REJECT response from the mfw\n");
		return -EBUSY;
	}

	/* Check if there is a DID mismatch between nvm-cfg/efuse */
	if (param & FW_MB_PARAM_LOAD_DONE_DID_EFUSE_ERROR)
		DP_NOTICE(p_hwfn,
			  "warning: device configuration is not supported on this board type. The device may not function as expected.\n");

	return 0;
}

#define MFW_COMPLETION_MAX_ITER 5000
#define MFW_COMPLETION_INTERVAL_MS 1

int qed_mcp_unload_req(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mcp_mb_params mb_params;
	u32 cnt = MFW_COMPLETION_MAX_ITER;
	u32 wol_param;
	int rc;

	switch (p_hwfn->cdev->wol_config) {
	case QED_OV_WOL_DISABLED:
		wol_param = DRV_MB_PARAM_UNLOAD_WOL_DISABLED;
		break;
	case QED_OV_WOL_ENABLED:
		wol_param = DRV_MB_PARAM_UNLOAD_WOL_ENABLED;
		break;
	default:
		DP_NOTICE(p_hwfn,
			  "Unknown WoL configuration %02x\n",
			  p_hwfn->cdev->wol_config);
		COMPAT_FALLTHROUGH;
		/* fallthrough */
	case QED_OV_WOL_DEFAULT:
		wol_param = DRV_MB_PARAM_UNLOAD_WOL_MCP;
	}

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_UNLOAD_REQ;
	mb_params.param = wol_param;
	mb_params.flags = QED_MB_FLAG_CAN_SLEEP | QED_MB_FLAG_AVOID_BLOCK;

	spin_lock_bh(&p_hwfn->mcp_info->unload_lock);
	set_bit(QED_MCP_BYPASS_PROC_BIT,
		&p_hwfn->mcp_info->mcp_handling_status);
	spin_unlock_bh(&p_hwfn->mcp_info->unload_lock);

	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);

	while (test_bit(QED_MCP_IN_PROCESSING_BIT,
			&p_hwfn->mcp_info->mcp_handling_status) && --cnt)
		msleep(MFW_COMPLETION_INTERVAL_MS);

	if (!cnt)
		DP_NOTICE(p_hwfn,
			  "Failed to wait MFW event completion after %d msec\n",
			  MFW_COMPLETION_MAX_ITER * MFW_COMPLETION_INTERVAL_MS);

	return rc;
}

int qed_mcp_unload_done(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mcp_mb_params mb_params;
	struct mcp_mac wol_mac;

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_UNLOAD_DONE;

	/* Set the primary MAC if WoL is enabled */
	if (p_hwfn->cdev->wol_config == QED_OV_WOL_ENABLED) {
		u8 *p_mac = p_hwfn->cdev->wol_mac;

		memset(&wol_mac, 0, sizeof(wol_mac));
		wol_mac.mac_upper = p_mac[0] << 8 | p_mac[1];
		wol_mac.mac_lower = p_mac[2] << 24 | p_mac[3] << 16 |
		    p_mac[4] << 8 | p_mac[5];

		DP_VERBOSE(p_hwfn,
			   (QED_MSG_SP | NETIF_MSG_IFDOWN),
			   "Setting WoL MAC: %02x:%02x:%02x:%02x:%02x:%02x --> [%08x,%08x]\n",
			   p_mac[0],
			   p_mac[1],
			   p_mac[2],
			   p_mac[3],
			   p_mac[4],
			   p_mac[5], wol_mac.mac_upper, wol_mac.mac_lower);

		mb_params.p_data_src = &wol_mac;
		mb_params.data_src_size = sizeof(wol_mac);
	}

	return qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
}

static void qed_mcp_handle_vf_flr(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt)
{
	u32 addr = SECTION_OFFSIZE_ADDR(p_hwfn->mcp_info->public_base,
					PUBLIC_PATH);
	u32 mfw_path_offsize = qed_rd(p_hwfn, p_ptt, addr);
	u32 path_addr = SECTION_ADDR(mfw_path_offsize,
				     QED_PATH_ID(p_hwfn));
	u32 disabled_vfs[EXT_VF_BITMAP_SIZE_IN_DWORDS];
	int i;

	memset(disabled_vfs, 0, EXT_VF_BITMAP_SIZE_IN_BYTES);

	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "Reading Disabled VF information from [offset %08x], path_addr %08x\n",
		   mfw_path_offsize, path_addr);

	for (i = 0; i < VF_BITMAP_SIZE_IN_DWORDS; i++) {
		disabled_vfs[i] = qed_rd(p_hwfn, p_ptt,
					 path_addr +
					 offsetof(struct public_path,
						  mcp_vf_disabled) +
					 sizeof(u32) * i);
		DP_VERBOSE(p_hwfn, (QED_MSG_SP | QED_MSG_IOV),
			   "FLR-ed VFs [%08x,...,%08x] - %08x\n",
			   i * 32, (i + 1) * 32 - 1, disabled_vfs[i]);
	}

	if (qed_iov_mark_vf_flr(p_hwfn, disabled_vfs))
		qed_schedule_iov(p_hwfn, QED_IOV_WQ_FLR_FLAG);
}

int qed_mcp_ack_vf_flr(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt, u32 * vfs_to_ack)
{
	struct qed_mcp_mb_params mb_params;
	int rc;
	u16 i;

	for (i = 0; i < VF_BITMAP_SIZE_IN_DWORDS; i++)
		DP_VERBOSE(p_hwfn, (QED_MSG_SP | QED_MSG_IOV),
			   "Acking VFs [%08x,...,%08x] - %08x\n",
			   i * 32, (i + 1) * 32 - 1, vfs_to_ack[i]);

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_VF_DISABLED_DONE;
	mb_params.p_data_src = vfs_to_ack;
	mb_params.data_src_size = (u8) VF_BITMAP_SIZE_IN_BYTES;
	mb_params.flags = QED_MB_FLAG_CAN_SLEEP;
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed to pass ACK for VF flr to MFW\n");
		return -EBUSY;
	}

	return rc;
}

static void qed_mcp_handle_transceiver_change(struct qed_hwfn *p_hwfn,
					      struct qed_ptt *p_ptt)
{
	u32 transceiver_state;

	transceiver_state = qed_rd(p_hwfn, p_ptt,
				   p_hwfn->mcp_info->port_addr +
				   offsetof(struct public_port,
					    transceiver_data));

	DP_VERBOSE(p_hwfn,
		   (NETIF_MSG_HW | QED_MSG_SP),
		   "Received transceiver state update [0x%08x] from mfw [Addr 0x%x]\n",
		   transceiver_state,
		   (u32) (p_hwfn->mcp_info->port_addr +
			  offsetof(struct public_port, transceiver_data)));

	transceiver_state = GET_MFW_FIELD(transceiver_state,
					  ETH_TRANSCEIVER_STATE);

	if (transceiver_state == ETH_TRANSCEIVER_STATE_PRESENT)
		DP_NOTICE(p_hwfn, "Transceiver is present.\n");
	else
		DP_NOTICE(p_hwfn, "Transceiver is unplugged.\n");

	qed_transceiver_update(p_hwfn);
}

static void qed_mcp_handle_transceiver_tx_fault(struct qed_hwfn *p_hwfn)
{
	DP_VERBOSE(p_hwfn, (NETIF_MSG_HW | QED_MSG_SP),
		   "Received TX_FAULT event from mfw\n");

	qed_transceiver_tx_fault(p_hwfn);
}

static void qed_mcp_handle_transceiver_rx_los(struct qed_hwfn *p_hwfn)
{
	DP_VERBOSE(p_hwfn, (NETIF_MSG_HW | QED_MSG_SP),
		   "Received RX_LOS event from mfw\n");

	qed_transceiver_rx_los(p_hwfn);
}

static void qed_mcp_read_eee_config(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt,
				    struct qed_mcp_link_state *p_link)
{
	u32 eee_status, val;

	p_link->eee_adv_caps = 0;
	p_link->eee_lp_adv_caps = 0;
	eee_status = qed_rd(p_hwfn,
			    p_ptt,
			    p_hwfn->mcp_info->port_addr +
			    offsetof(struct public_port, eee_status));
	p_link->eee_active = ! !(eee_status & EEE_ACTIVE_BIT);
	val = GET_MFW_FIELD(eee_status, EEE_LD_ADV_STATUS);
	if (val & EEE_1G_ADV)
		p_link->eee_adv_caps |= QED_EEE_1G_ADV;
	if (val & EEE_10G_ADV)
		p_link->eee_adv_caps |= QED_EEE_10G_ADV;
	val = GET_MFW_FIELD(eee_status, EEE_LP_ADV_STATUS);
	if (val & EEE_1G_ADV)
		p_link->eee_lp_adv_caps |= QED_EEE_1G_ADV;
	if (val & EEE_10G_ADV)
		p_link->eee_lp_adv_caps |= QED_EEE_10G_ADV;
}

static u32 qed_mcp_get_shmem_func(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt,
				  struct public_func *p_data, int pfid)
{
	u32 addr = SECTION_OFFSIZE_ADDR(p_hwfn->mcp_info->public_base,
					PUBLIC_FUNC);
	u32 mfw_path_offsize = qed_rd(p_hwfn, p_ptt, addr);
	u32 func_addr = SECTION_ADDR(mfw_path_offsize, pfid);
	u32 i, size;

	memset(p_data, 0, sizeof(*p_data));

	size = min_t(u32, sizeof(*p_data), QED_SECTION_SIZE(mfw_path_offsize));
	for (i = 0; i < size / sizeof(u32); i++)
		((u32 *) p_data)[i] = qed_rd(p_hwfn, p_ptt,
					     func_addr + (i << 2));

	return size;
}

static void qed_read_pf_bandwidth(struct qed_hwfn *p_hwfn,
				  struct public_func *p_shmem_info)
{
	struct qed_mcp_function_info *p_info;

	p_info = &p_hwfn->mcp_info->func_info;

	/* TODO - bandwidth min/max should have valid values of 1-100,
	 * as well as some indication that the feature is disabled.
	 * Until MFW/qlediag enforce those limitations, Assume THERE IS ALWAYS
	 * limit and correct value to min `1' and max `100' if limit isn't in
	 * range.
	 */
	p_info->bandwidth_min = GET_MFW_FIELD(p_shmem_info->config,
					      FUNC_MF_CFG_MIN_BW);
	if (p_info->bandwidth_min < 1 || p_info->bandwidth_min > 100) {
		DP_INFO(p_hwfn,
			"bandwidth minimum out of bounds [%02x]. Set to 1\n",
			p_info->bandwidth_min);
		p_info->bandwidth_min = 1;
	}

	p_info->bandwidth_max = GET_MFW_FIELD(p_shmem_info->config,
					      FUNC_MF_CFG_MAX_BW);
	if (p_info->bandwidth_max < 1 || p_info->bandwidth_max > 100) {
		DP_INFO(p_hwfn,
			"bandwidth maximum out of bounds [%02x]. Set to 100\n",
			p_info->bandwidth_max);
		p_info->bandwidth_max = 100;
	}
}

static void qed_mcp_handle_link_change(struct qed_hwfn *p_hwfn,
				       struct qed_ptt *p_ptt, bool b_reset)
{
	struct qed_mcp_link_state *p_link;
	u8 max_bw, min_bw;
	u32 status = 0;

	/* Prevent SW/attentions from doing this at the same time */
	spin_lock_bh(&p_hwfn->mcp_info->link_lock);

	p_link = &p_hwfn->mcp_info->link_output;
	memset(p_link, 0, sizeof(*p_link));
	if (!b_reset) {
		status = qed_rd(p_hwfn, p_ptt,
				p_hwfn->mcp_info->port_addr +
				offsetof(struct public_port, link_status));
		DP_VERBOSE(p_hwfn,
			   (NETIF_MSG_LINK | QED_MSG_SP),
			   "Received link update [0x%08x] from mfw [Addr 0x%x]\n",
			   status,
			   (u32) (p_hwfn->mcp_info->port_addr +
				  offsetof(struct public_port, link_status)));
	} else {
		DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
			   "Resetting link indications\n");
		goto out;
	}

	if (p_hwfn->b_drv_link_init) {
		/* Link indication with modern MFW arrives as per-PF
		 * indication.
		 */
		if (p_hwfn->mcp_info->capabilities &
		    FW_MB_PARAM_FEATURE_SUPPORT_VLINK) {
			struct public_func shmem_info;

			qed_mcp_get_shmem_func(p_hwfn, p_ptt, &shmem_info,
					       MCP_PF_ID(p_hwfn));
			p_link->link_up = ! !(shmem_info.status &
					      FUNC_STATUS_VIRTUAL_LINK_UP);
			qed_read_pf_bandwidth(p_hwfn, &shmem_info);
			DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
				   "Virtual link_up = %d\n", p_link->link_up);
		} else {
			p_link->link_up = ! !(status & LINK_STATUS_LINK_UP);
			DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
				   "Physical link_up = %d\n", p_link->link_up);
		}
	} else {
		p_link->link_up = false;

		DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
			   "Link is down - disbale dcbx\n");
		p_hwfn->p_dcbx_info->get.operational.enabled = false;
	}

	p_link->full_duplex = true;
	switch ((status & LINK_STATUS_SPEED_AND_DUPLEX_MASK)) {
	case LINK_STATUS_SPEED_AND_DUPLEX_100G:
		p_link->speed = 100000;
		break;
	case LINK_STATUS_SPEED_AND_DUPLEX_50G:
		p_link->speed = 50000;
		break;
	case LINK_STATUS_SPEED_AND_DUPLEX_40G:
		p_link->speed = 40000;
		break;
	case LINK_STATUS_SPEED_AND_DUPLEX_25G:
		p_link->speed = 25000;
		break;
	case LINK_STATUS_SPEED_AND_DUPLEX_20G:
		p_link->speed = 20000;
		break;
	case LINK_STATUS_SPEED_AND_DUPLEX_10G:
		p_link->speed = 10000;
		break;
	case LINK_STATUS_SPEED_AND_DUPLEX_1000THD:
		p_link->full_duplex = false;
		COMPAT_FALLTHROUGH;
		/* fallthrough */
	case LINK_STATUS_SPEED_AND_DUPLEX_1000TFD:
		p_link->speed = 1000;
		break;
	default:
		p_link->speed = 0;
		p_link->link_up = 0;
	}

	/* We never store total line speed as p_link->speed is
	 * again changes according to bandwidth allocation.
	 */
	if (p_link->link_up && p_link->speed)
		p_link->line_speed = p_link->speed;
	else
		p_link->line_speed = 0;

	max_bw = p_hwfn->mcp_info->func_info.bandwidth_max;
	min_bw = p_hwfn->mcp_info->func_info.bandwidth_min;

	/* Max bandwidth configuration */
	__qed_configure_pf_max_bandwidth(p_hwfn, p_ptt, p_link, max_bw);

	/* Min bandwidth configuration */
	__qed_configure_pf_min_bandwidth(p_hwfn, p_ptt, p_link, min_bw);
	qed_configure_vp_wfq_on_link_change(p_hwfn->cdev, p_ptt,
					    p_link->min_pf_rate);

	p_link->an = ! !(status & LINK_STATUS_AUTO_NEGOTIATE_ENABLED);
	p_link->an_complete = ! !(status & LINK_STATUS_AUTO_NEGOTIATE_COMPLETE);
	p_link->parallel_detection = ! !(status &
					 LINK_STATUS_PARALLEL_DETECTION_USED);
	p_link->pfc_enabled = ! !(status & LINK_STATUS_PFC_ENABLED);

	p_link->partner_adv_speed |=
	    (status & LINK_STATUS_LINK_PARTNER_1000TFD_CAPABLE) ?
	    QED_LINK_PARTNER_SPEED_1G_FD : 0;
	p_link->partner_adv_speed |=
	    (status & LINK_STATUS_LINK_PARTNER_1000THD_CAPABLE) ?
	    QED_LINK_PARTNER_SPEED_1G_HD : 0;
	p_link->partner_adv_speed |=
	    (status & LINK_STATUS_LINK_PARTNER_10G_CAPABLE) ?
	    QED_LINK_PARTNER_SPEED_10G : 0;
	p_link->partner_adv_speed |=
	    (status & LINK_STATUS_LINK_PARTNER_20G_CAPABLE) ?
	    QED_LINK_PARTNER_SPEED_20G : 0;
	p_link->partner_adv_speed |=
	    (status & LINK_STATUS_LINK_PARTNER_25G_CAPABLE) ?
	    QED_LINK_PARTNER_SPEED_25G : 0;
	p_link->partner_adv_speed |=
	    (status & LINK_STATUS_LINK_PARTNER_40G_CAPABLE) ?
	    QED_LINK_PARTNER_SPEED_40G : 0;
	p_link->partner_adv_speed |=
	    (status & LINK_STATUS_LINK_PARTNER_50G_CAPABLE) ?
	    QED_LINK_PARTNER_SPEED_50G : 0;
	p_link->partner_adv_speed |=
	    (status & LINK_STATUS_LINK_PARTNER_100G_CAPABLE) ?
	    QED_LINK_PARTNER_SPEED_100G : 0;

	p_link->partner_tx_flow_ctrl_en =
	    ! !(status & LINK_STATUS_TX_FLOW_CONTROL_ENABLED);
	p_link->partner_rx_flow_ctrl_en =
	    ! !(status & LINK_STATUS_RX_FLOW_CONTROL_ENABLED);

	switch (status & LINK_STATUS_LINK_PARTNER_FLOW_CONTROL_MASK) {
	case LINK_STATUS_LINK_PARTNER_SYMMETRIC_PAUSE:
		p_link->partner_adv_pause = QED_LINK_PARTNER_SYMMETRIC_PAUSE;
		break;
	case LINK_STATUS_LINK_PARTNER_ASYMMETRIC_PAUSE:
		p_link->partner_adv_pause = QED_LINK_PARTNER_ASYMMETRIC_PAUSE;
		break;
	case LINK_STATUS_LINK_PARTNER_BOTH_PAUSE:
		p_link->partner_adv_pause = QED_LINK_PARTNER_BOTH_PAUSE;
		break;
	default:
		p_link->partner_adv_pause = 0;
	}

	p_link->sfp_tx_fault = ! !(status & LINK_STATUS_SFP_TX_FAULT);

	if (p_hwfn->mcp_info->capabilities & FW_MB_PARAM_FEATURE_SUPPORT_EEE)
		qed_mcp_read_eee_config(p_hwfn, p_ptt, p_link);

	if (p_hwfn->mcp_info->capabilities &
	    FW_MB_PARAM_FEATURE_SUPPORT_FEC_CONTROL) {
		switch (status & LINK_STATUS_FEC_MODE_MASK) {
		case LINK_STATUS_FEC_MODE_NONE:
			p_link->fec_active = QED_MCP_FEC_NONE;
			break;
		case LINK_STATUS_FEC_MODE_FIRECODE_CL74:
			p_link->fec_active = QED_MCP_FEC_FIRECODE;
			break;
		case LINK_STATUS_FEC_MODE_RS_CL91:
			p_link->fec_active = QED_MCP_FEC_RS;
			break;
		default:
			p_link->fec_active = QED_MCP_FEC_AUTO;
		}
	} else {
		p_link->fec_active = QED_MCP_FEC_UNSUPPORTED;
	}

	qed_link_update(p_hwfn, p_ptt);
out:
	spin_unlock_bh(&p_hwfn->mcp_info->link_lock);
}

int qed_mcp_set_link(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, bool b_up)
{
	struct qed_mcp_link_params *params = &p_hwfn->mcp_info->link_input;
	struct qed_mcp_mb_params mb_params;
	struct eth_phy_cfg phy_cfg;
	int rc = 0;
	u32 cmd, fec_bit = 0;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->cdev)) {
		if (b_up)
			qed_link_update(p_hwfn, p_ptt);
		return 0;
	}
#endif

	/* Set the shmem configuration according to params */
	memset(&phy_cfg, 0, sizeof(phy_cfg));
	cmd = b_up ? DRV_MSG_CODE_INIT_PHY : DRV_MSG_CODE_LINK_RESET;
	if (!params->speed.autoneg)
		phy_cfg.speed = params->speed.forced_speed;
	phy_cfg.pause |= (params->pause.autoneg) ? ETH_PAUSE_AUTONEG : 0;
	phy_cfg.pause |= (params->pause.forced_rx) ? ETH_PAUSE_RX : 0;
	phy_cfg.pause |= (params->pause.forced_tx) ? ETH_PAUSE_TX : 0;
	phy_cfg.adv_speed = params->speed.advertised_speeds;
	phy_cfg.loopback_mode = params->loopback_mode;

	/* There are MFWs that share this capability regardless of whether
	 * this is feasible or not. And given that at the very least adv_caps
	 * would be set internally by qed, we want to make sure LFA would
	 * still work.
	 */
	if ((p_hwfn->mcp_info->capabilities &
	     FW_MB_PARAM_FEATURE_SUPPORT_EEE) && params->eee.enable) {
		phy_cfg.eee_cfg |= EEE_CFG_EEE_ENABLED;
		if (params->eee.tx_lpi_enable)
			phy_cfg.eee_cfg |= EEE_CFG_TX_LPI;
		if (params->eee.adv_caps & QED_EEE_1G_ADV)
			phy_cfg.eee_cfg |= EEE_CFG_ADV_SPEED_1G;
		if (params->eee.adv_caps & QED_EEE_10G_ADV)
			phy_cfg.eee_cfg |= EEE_CFG_ADV_SPEED_10G;
		SET_MFW_FIELD(phy_cfg.eee_cfg, EEE_TX_TIMER_USEC,
			      params->eee.tx_lpi_timer);
	}

	if (p_hwfn->mcp_info->capabilities &
	    FW_MB_PARAM_FEATURE_SUPPORT_FEC_CONTROL) {
		if (params->fec & QED_MCP_FEC_NONE)
			fec_bit |= FEC_FORCE_MODE_NONE;
		else if (params->fec & QED_MCP_FEC_FIRECODE)
			fec_bit |= FEC_FORCE_MODE_FIRECODE;
		else if (params->fec & QED_MCP_FEC_RS)
			fec_bit |= FEC_FORCE_MODE_RS;
		else if (params->fec & QED_MCP_FEC_AUTO)
			fec_bit |= FEC_FORCE_MODE_AUTO;
		SET_MFW_FIELD(phy_cfg.fec_mode, FEC_FORCE_MODE, fec_bit);
	}

	p_hwfn->b_drv_link_init = b_up;

	if (b_up) {
		DP_VERBOSE(p_hwfn,
			   NETIF_MSG_LINK,
			   "Configuring Link: Speed 0x%08x, Pause 0x%08x, adv_speed 0x%08x, loopback 0x%08x FEC 0x%08x\n",
			   phy_cfg.speed,
			   phy_cfg.pause,
			   phy_cfg.adv_speed,
			   phy_cfg.loopback_mode, phy_cfg.fec_mode);
	} else {
		DP_VERBOSE(p_hwfn, NETIF_MSG_LINK, "Resetting link\n");
	}

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = cmd;
	mb_params.p_data_src = &phy_cfg;
	mb_params.data_src_size = sizeof(phy_cfg);
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);

	/* if mcp fails to respond we must abort */
	if (rc) {
		DP_ERR(p_hwfn, "MCP response failure, aborting\n");
		return rc;
	}

	/* Mimic link-change attention, done for several reasons:
	 *  - On reset, there's no guarantee MFW would trigger
	 *    an attention.
	 *  - On initialization, older MFWs might not indicate link change
	 *    during LFA, so we'll never get an UP indication.
	 */
	qed_mcp_handle_link_change(p_hwfn, p_ptt, !b_up);

	return 0;
}

u32 qed_get_process_kill_counter(struct qed_hwfn * p_hwfn,
				 struct qed_ptt * p_ptt)
{
	u32 path_offsize_addr, path_offsize, path_addr, proc_kill_cnt;

	/* TODO - Add support for VFs */
	if (IS_VF(p_hwfn->cdev))
		return -EINVAL;

	path_offsize_addr = SECTION_OFFSIZE_ADDR(p_hwfn->mcp_info->public_base,
						 PUBLIC_PATH);
	path_offsize = qed_rd(p_hwfn, p_ptt, path_offsize_addr);
	path_addr = SECTION_ADDR(path_offsize, QED_PATH_ID(p_hwfn));

	proc_kill_cnt = qed_rd(p_hwfn, p_ptt,
			       path_addr +
			       offsetof(struct public_path, process_kill)) &
	    PROCESS_KILL_COUNTER_MASK;

	return proc_kill_cnt;
}

static void qed_mcp_handle_process_kill(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	u32 proc_kill_cnt;

	/* Prevent possible attentions/interrupts during the recovery handling
	 * and till its load phase, during which they will be re-enabled.
	 */
	qed_int_igu_disable_int(p_hwfn, p_ptt);

	DP_NOTICE(p_hwfn, "Received a process kill indication\n");

	/* The following operations should be done once, and thus in CMT mode
	 * are carried out by only the first HW function.
	 */
	if (p_hwfn != QED_LEADING_HWFN(cdev))
		return;

	if (QED_RECOV_IN_PROG(p_hwfn->cdev)) {
		DP_NOTICE(p_hwfn,
			  "Ignoring the indication since a recovery process is already in progress\n");
		return;
	}

	cdev->recov_in_prog = true;

	proc_kill_cnt = qed_get_process_kill_counter(p_hwfn, p_ptt);
	DP_NOTICE(p_hwfn, "Process kill counter: %d\n", proc_kill_cnt);

	qed_schedule_recovery_handler(p_hwfn);
}

static void qed_mcp_send_protocol_stats(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt,
					enum MFW_DRV_MSG_TYPE type)
{
	enum qed_mcp_protocol_type stats_type;
	union qed_mcp_protocol_stats stats;
	struct qed_mcp_mb_params mb_params;
	u32 hsi_param;
	int rc;

	switch (type) {
	case MFW_DRV_MSG_GET_LAN_STATS:
		stats_type = QED_MCP_LAN_STATS;
		hsi_param = DRV_MSG_CODE_STATS_TYPE_LAN;
		break;
	case MFW_DRV_MSG_GET_FCOE_STATS:
		stats_type = QED_MCP_FCOE_STATS;
		hsi_param = DRV_MSG_CODE_STATS_TYPE_FCOE;
		break;
	case MFW_DRV_MSG_GET_ISCSI_STATS:
		stats_type = QED_MCP_ISCSI_STATS;
		hsi_param = DRV_MSG_CODE_STATS_TYPE_ISCSI;
		break;
	case MFW_DRV_MSG_GET_RDMA_STATS:
		stats_type = QED_MCP_RDMA_STATS;
		hsi_param = DRV_MSG_CODE_STATS_TYPE_RDMA;
		break;
	default:
		DP_VERBOSE(p_hwfn, QED_MSG_SP,
			   "Invalid protocol type %d\n", type);
		return;
	}

	qed_get_protocol_stats(p_hwfn->cdev, stats_type, &stats);

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_GET_STATS;
	mb_params.param = hsi_param;
	mb_params.p_data_src = &stats;
	mb_params.data_src_size = sizeof(stats);
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		DP_ERR(p_hwfn, "Failed to send protocol stats, rc = %d\n", rc);
}

static void qed_mcp_update_bw(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mcp_function_info *p_info;
	struct public_func shmem_info;
	u32 resp = 0, param = 0;

	spin_lock_bh(&p_hwfn->mcp_info->link_lock);

	qed_mcp_get_shmem_func(p_hwfn, p_ptt, &shmem_info, MCP_PF_ID(p_hwfn));

	qed_read_pf_bandwidth(p_hwfn, &shmem_info);

	p_info = &p_hwfn->mcp_info->func_info;

	qed_configure_pf_min_bandwidth(p_hwfn->cdev, p_info->bandwidth_min);

	qed_configure_pf_max_bandwidth(p_hwfn->cdev, p_info->bandwidth_max);

	qed_bw_update(p_hwfn, p_ptt);

	spin_unlock_bh(&p_hwfn->mcp_info->link_lock);

	/* Acknowledge the MFW */
	qed_mcp_cmd_nosleep(p_hwfn, p_ptt, DRV_MSG_CODE_BW_UPDATE_ACK, 0,
			    &resp, &param);
}

static void qed_mcp_update_stag(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct public_func shmem_info;
	u32 resp = 0, param = 0;

	qed_mcp_get_shmem_func(p_hwfn, p_ptt, &shmem_info, MCP_PF_ID(p_hwfn));

	p_hwfn->mcp_info->func_info.ovlan = (u16) shmem_info.ovlan_stag &
	    FUNC_MF_CFG_OV_STAG_MASK;
	p_hwfn->hw_info.ovlan = p_hwfn->mcp_info->func_info.ovlan;
	if (test_bit(QED_MF_OVLAN_CLSS, &p_hwfn->cdev->mf_bits)) {
		if (p_hwfn->hw_info.ovlan != QED_MCP_VLAN_UNSET) {
			qed_wr(p_hwfn, p_ptt, NIG_REG_LLH_FUNC_TAG_VALUE,
			       p_hwfn->hw_info.ovlan);
			qed_wr(p_hwfn, p_ptt, NIG_REG_LLH_FUNC_TAG_EN, 1);

			/* Configure DB to add external vlan to EDPM packets */
			qed_wr(p_hwfn, p_ptt, DORQ_REG_TAG1_OVRD_MODE, 1);
			qed_wr(p_hwfn, p_ptt, DORQ_REG_PF_EXT_VID,
			       p_hwfn->hw_info.ovlan);
		} else {
			qed_wr(p_hwfn, p_ptt, NIG_REG_LLH_FUNC_TAG_EN, 0);
			qed_wr(p_hwfn, p_ptt, NIG_REG_LLH_FUNC_TAG_VALUE, 0);

			/* Configure DB to add external vlan to EDPM packets */
			qed_wr(p_hwfn, p_ptt, DORQ_REG_TAG1_OVRD_MODE, 0);
			qed_wr(p_hwfn, p_ptt, DORQ_REG_PF_EXT_VID, 0);
		}

		qed_sp_pf_update_stag(p_hwfn);
	}

	DP_VERBOSE(p_hwfn, QED_MSG_SP, "ovlan  = %d hw_mode = 0x%x\n",
		   p_hwfn->mcp_info->func_info.ovlan, p_hwfn->hw_info.hw_mode);
	qed_hw_attr_update(p_hwfn, QED_HW_INFO_CHANGE_OVLAN);

	/* Acknowledge the MFW */
	qed_mcp_cmd_nosleep(p_hwfn, p_ptt, DRV_MSG_CODE_S_TAG_UPDATE_ACK, 0,
			    &resp, &param);
}

static void qed_mcp_handle_fan_failure(struct qed_hwfn *p_hwfn,
				       struct qed_ptt *p_ptt)
{
	u32 addr, global_offsize, global_addr;
	u32 nvm_cfg_addr, nvm_cfg1_offset;
	u32 phy_mod_temp, gpio_value;
	u32 fan_fail_enforcement;
	u32 int_temp, ext_temp;
	u32 gpio0;

	/* A single notification should be sent to upper driver in CMT mode */
	if (p_hwfn != QED_LEADING_HWFN(p_hwfn->cdev))
		return;

	addr = SECTION_OFFSIZE_ADDR(p_hwfn->mcp_info->public_base,
				    PUBLIC_GLOBAL);
	global_offsize = qed_rd(p_hwfn, p_ptt, addr);
	global_addr = SECTION_ADDR(global_offsize, 0);
	int_temp = qed_rd(p_hwfn, p_ptt, global_addr +
			  offsetof(struct public_global, internal_temperature));
	addr = global_addr + offsetof(struct public_global,
				      external_temperature);
	ext_temp = qed_rd(p_hwfn, p_ptt, addr);
	addr = p_hwfn->mcp_info->port_addr + offsetof(struct public_port,
						      phy_module_temperature);
	phy_mod_temp = qed_rd(p_hwfn, p_ptt, addr);
	nvm_cfg_addr = qed_rd(p_hwfn, p_ptt, MISC_REG_GEN_PURP_CR0);
	nvm_cfg1_offset = qed_rd(p_hwfn, p_ptt, nvm_cfg_addr + 4);
	addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
	    offsetof(struct nvm_cfg1, glob) +
	    offsetof(struct nvm_cfg1_glob, generic_cont0);
	fan_fail_enforcement = qed_rd(p_hwfn, p_ptt, addr);
	fan_fail_enforcement &= NVM_CFG1_GLOB_FAN_FAILURE_ENFORCEMENT_MASK;

	if (NVM_CFG1_GLOB_FAN_FAILURE_ENFORCEMENT_ENABLED ==
	    fan_fail_enforcement) {
		qed_mcp_gpio_read(p_hwfn, p_ptt, MISCS_REG_GPIO_VAL,
				  &gpio_value);
		gpio0 = gpio_value & 1;
		DP_NOTICE(p_hwfn, "Fan Failure(Status = %d)\n", gpio0);
	}

	DP_NOTICE(p_hwfn,
		  "Fan failure: Internal Temp=%dC External Temp=%dC Phy Module Temp=%dC\n",
		  int_temp, ext_temp, phy_mod_temp);

	qed_hw_err_notify(p_hwfn,
			  p_ptt,
			  QED_HW_ERR_FAN_FAIL,
			  "Fan failure was detected on the network interface card and it's going to be shut down.\n");
}

struct qed_mdump_cmd_params {
	u32 cmd;
	void *p_data_src;
	u8 data_src_size;
	void *p_data_dst;
	u8 data_dst_size;
	u32 mcp_resp;
	u32 mcp_param;
	bool can_sleep;
};

static int
qed_mcp_mdump_cmd(struct qed_hwfn *p_hwfn,
		  struct qed_ptt *p_ptt,
		  struct qed_mdump_cmd_params *p_mdump_cmd_params)
{
	struct qed_mcp_mb_params mb_params;
	int rc;

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_MDUMP_CMD;
	mb_params.param = p_mdump_cmd_params->cmd;
	mb_params.p_data_src = p_mdump_cmd_params->p_data_src;
	mb_params.data_src_size = p_mdump_cmd_params->data_src_size;
	mb_params.p_data_dst = p_mdump_cmd_params->p_data_dst;
	mb_params.data_dst_size = p_mdump_cmd_params->data_dst_size;
	mb_params.flags = p_mdump_cmd_params->can_sleep ?
	    QED_MB_FLAG_CAN_SLEEP : 0;
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		return rc;

	p_mdump_cmd_params->mcp_resp = mb_params.mcp_resp;
	p_mdump_cmd_params->mcp_param = mb_params.mcp_param;

	if (p_mdump_cmd_params->mcp_resp == FW_MSG_CODE_MDUMP_INVALID_CMD) {
		DP_INFO(p_hwfn,
			"The mdump sub command is unsupported by the MFW [mdump_cmd 0x%x]\n",
			p_mdump_cmd_params->cmd);
		rc = -EOPNOTSUPP;
	} else if (p_mdump_cmd_params->mcp_resp == FW_MSG_CODE_UNSUPPORTED) {
		DP_INFO(p_hwfn,
			"The mdump command is not supported by the MFW\n");
		rc = -EOPNOTSUPP;
	}

	return rc;
}

static int qed_mcp_mdump_ack(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mdump_cmd_params mdump_cmd_params;

	memset(&mdump_cmd_params, 0, sizeof(mdump_cmd_params));
	mdump_cmd_params.cmd = DRV_MSG_CODE_MDUMP_ACK;

	return qed_mcp_mdump_cmd(p_hwfn, p_ptt, &mdump_cmd_params);
}

int qed_mcp_mdump_set_values(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, u32 epoch)
{
	struct qed_mdump_cmd_params mdump_cmd_params;

	memset(&mdump_cmd_params, 0, sizeof(mdump_cmd_params));
	mdump_cmd_params.cmd = DRV_MSG_CODE_MDUMP_SET_VALUES;
	mdump_cmd_params.p_data_src = &epoch;
	mdump_cmd_params.data_src_size = sizeof(epoch);

	return qed_mcp_mdump_cmd(p_hwfn, p_ptt, &mdump_cmd_params);
}

int qed_mcp_mdump_trigger(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mdump_cmd_params mdump_cmd_params;
	int rc;

	memset(&mdump_cmd_params, 0, sizeof(mdump_cmd_params));
	mdump_cmd_params.cmd = DRV_MSG_CODE_MDUMP_TRIGGER;

	rc = qed_mcp_mdump_cmd(p_hwfn, p_ptt, &mdump_cmd_params);
	if (mdump_cmd_params.mcp_resp == FW_MSG_CODE_MDUMP_NO_IMAGE_FOUND) {
		DP_INFO(p_hwfn,
			"The mdump command failed cause there is no HW_DUMP image\n");
		rc = -EINVAL;
	} else if (mdump_cmd_params.mcp_resp != FW_MSG_CODE_OK) {
		DP_INFO(p_hwfn,
			"The mdump command failed cause of MFW error\n");
		rc = -EINVAL;
	}
	return rc;
}

static int
qed_mcp_mdump_get_config(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 struct mdump_config_stc *p_mdump_config)
{
	struct qed_mdump_cmd_params mdump_cmd_params;
	int rc;

	memset(&mdump_cmd_params, 0, sizeof(mdump_cmd_params));
	mdump_cmd_params.cmd = DRV_MSG_CODE_MDUMP_GET_CONFIG;
	mdump_cmd_params.p_data_dst = p_mdump_config;
	mdump_cmd_params.data_dst_size = sizeof(*p_mdump_config);

	rc = qed_mcp_mdump_cmd(p_hwfn, p_ptt, &mdump_cmd_params);
	if (rc)
		return rc;

	if (mdump_cmd_params.mcp_resp != FW_MSG_CODE_OK) {
		DP_INFO(p_hwfn,
			"Failed to get the mdump configuration and logs info [mcp_resp 0x%x]\n",
			mdump_cmd_params.mcp_resp);
		rc = -EINVAL;
	}

	return rc;
}

int
qed_mcp_mdump_get_info(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       struct qed_mdump_info *p_mdump_info)
{
	u32 addr, global_offsize, global_addr;
	struct mdump_config_stc mdump_config;
	int rc;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->cdev) && !qed_mcp_is_init(p_hwfn)) {
		DP_INFO(p_hwfn, "Emulation: Can't get mdump info\n");
		return -EOPNOTSUPP;
	}
#endif

	memset(p_mdump_info, 0, sizeof(*p_mdump_info));

	addr = SECTION_OFFSIZE_ADDR(p_hwfn->mcp_info->public_base,
				    PUBLIC_GLOBAL);
	global_offsize = qed_rd(p_hwfn, p_ptt, addr);
	global_addr = SECTION_ADDR(global_offsize, 0);
	p_mdump_info->reason = qed_rd(p_hwfn, p_ptt,
				      global_addr +
				      offsetof(struct public_global,
					       mdump_reason));

	if (p_mdump_info->reason) {
		rc = qed_mcp_mdump_get_config(p_hwfn, p_ptt, &mdump_config);
		if (rc)
			return rc;

		p_mdump_info->version = mdump_config.version;
		p_mdump_info->config = mdump_config.config;
		p_mdump_info->epoch = mdump_config.epoc;
		p_mdump_info->num_of_logs = mdump_config.num_of_logs;
		p_mdump_info->valid_logs = mdump_config.valid_logs;

		DP_VERBOSE(p_hwfn,
			   QED_MSG_SP,
			   "MFW mdump info: reason %d, version 0x%x, config 0x%x, epoch 0x%x, num_of_logs 0x%x, valid_logs 0x%x\n",
			   p_mdump_info->reason,
			   p_mdump_info->version,
			   p_mdump_info->config,
			   p_mdump_info->epoch,
			   p_mdump_info->num_of_logs, p_mdump_info->valid_logs);
	} else {
		DP_VERBOSE(p_hwfn, QED_MSG_SP,
			   "MFW mdump info: reason %d\n", p_mdump_info->reason);
	}

	return 0;
}

int qed_mcp_mdump_clear_logs(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mdump_cmd_params mdump_cmd_params;

	memset(&mdump_cmd_params, 0, sizeof(mdump_cmd_params));
	mdump_cmd_params.cmd = DRV_MSG_CODE_MDUMP_CLEAR_LOGS;

	return qed_mcp_mdump_cmd(p_hwfn, p_ptt, &mdump_cmd_params);
}

int
qed_mcp_mdump_get_retain(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 struct qed_mdump_retain_data *p_mdump_retain)
{
	struct qed_mdump_cmd_params mdump_cmd_params;
	struct mdump_retain_data_stc mfw_mdump_retain;
	int rc;

	memset(&mdump_cmd_params, 0, sizeof(mdump_cmd_params));
	mdump_cmd_params.cmd = DRV_MSG_CODE_MDUMP_GET_RETAIN;
	mdump_cmd_params.p_data_dst = &mfw_mdump_retain;
	mdump_cmd_params.data_dst_size = sizeof(mfw_mdump_retain);

	rc = qed_mcp_mdump_cmd(p_hwfn, p_ptt, &mdump_cmd_params);
	if (rc)
		return rc;

	if (mdump_cmd_params.mcp_resp != FW_MSG_CODE_OK) {
		DP_INFO(p_hwfn,
			"Failed to get the mdump retained data [mcp_resp 0x%x]\n",
			mdump_cmd_params.mcp_resp);
		return -EINVAL;
	}

	p_mdump_retain->valid = mfw_mdump_retain.valid;
	p_mdump_retain->epoch = mfw_mdump_retain.epoch;
	p_mdump_retain->pf = mfw_mdump_retain.pf;
	p_mdump_retain->status = mfw_mdump_retain.status;

	return 0;
}

int qed_mcp_mdump_clr_retain(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mdump_cmd_params mdump_cmd_params;

	memset(&mdump_cmd_params, 0, sizeof(mdump_cmd_params));
	mdump_cmd_params.cmd = DRV_MSG_CODE_MDUMP_CLR_RETAIN;

	return qed_mcp_mdump_cmd(p_hwfn, p_ptt, &mdump_cmd_params);
}

int qed_mcp_hw_dump_trigger(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mdump_cmd_params mdump_cmd_params;
	int rc;

	memset(&mdump_cmd_params, 0, sizeof(mdump_cmd_params));
	mdump_cmd_params.cmd = DRV_MSG_CODE_HW_DUMP_TRIGGER;

	rc = qed_mcp_mdump_cmd(p_hwfn, p_ptt, &mdump_cmd_params);
	if (mdump_cmd_params.mcp_resp == FW_MSG_CODE_MDUMP_NO_IMAGE_FOUND) {
		DP_INFO(p_hwfn,
			"The hw_dump command failed cause there is no HW_DUMP image\n");
		rc = -EINVAL;
	} else if (mdump_cmd_params.mcp_resp != FW_MSG_CODE_OK) {
		DP_INFO(p_hwfn,
			"The hw_dump command failed cause of MFW error\n");
		rc = -EINVAL;
	}
	return rc;
}

int qed_mdump2_req_offsize(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt,
			   u32 * buff_byte_size, u32 * buff_byte_addr)
{
	struct qed_mdump_cmd_params mdump_cmd_params;
	int rc;

	memset(&mdump_cmd_params, 0, sizeof(mdump_cmd_params));
	mdump_cmd_params.cmd = DRV_MSG_CODE_MDUMP_GEN_LINK_DUMP;
	mdump_cmd_params.can_sleep = true;

	SET_MFW_FIELD(mdump_cmd_params.cmd,
		      DRV_MSG_CODE_MDUMP_USE_DRIVER_BUF, 1);

	rc = qed_mcp_mdump_cmd(p_hwfn, p_ptt, &mdump_cmd_params);
	if (mdump_cmd_params.mcp_resp != FW_MSG_CODE_OK) {
		DP_INFO(p_hwfn,
			"The mdump2 command failed because of MFW error\n");
		rc = -EINVAL;
	}

	*buff_byte_size = QED_SECTION_SIZE(mdump_cmd_params.mcp_param);
	*buff_byte_addr = SECTION_ADDR(mdump_cmd_params.mcp_param, 0);

	return rc;
}

int qed_mdump2_req_free(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mdump_cmd_params mdump_cmd_params;
	int rc;

	memset(&mdump_cmd_params, 0, sizeof(mdump_cmd_params));
	mdump_cmd_params.cmd = DRV_MSG_CODE_MDUMP_FREE_DRIVER_BUF;
	mdump_cmd_params.can_sleep = true;

	rc = qed_mcp_mdump_cmd(p_hwfn, p_ptt, &mdump_cmd_params);
	if (mdump_cmd_params.mcp_resp != FW_MSG_CODE_OK) {
		DP_INFO(p_hwfn,
			"The mdump2 command failed because of MFW error\n");
		rc = -EINVAL;
	}

	return rc;
}

static int
qed_mcp_get_disabled_attns(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, u32 * disabled_attns)
{
	struct qed_mcp_mb_params mb_params;
	struct get_att_ctrl_stc get_att_ctrl;
	int rc;

	memset(&get_att_ctrl, 0, sizeof(get_att_ctrl));
	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_GET_ATTN_CONTROL;
	mb_params.p_data_dst = &get_att_ctrl;
	mb_params.data_dst_size = sizeof(get_att_ctrl);
	mb_params.flags = QED_MB_FLAG_CAN_SLEEP;

	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc) {
		DP_NOTICE(p_hwfn, "GET_ATTN_CONTROL failed, rc = %d\n", rc);
		return rc;
	}
	if (mb_params.mcp_resp == FW_MSG_CODE_UNSUPPORTED) {
		DP_INFO(p_hwfn, "GET_ATTN_CONTROL command is not supported\n");
		return -EOPNOTSUPP;
	}

	*disabled_attns = get_att_ctrl.disabled_attns;

	return 0;
}

static int
qed_mcp_get_attn_control(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 enum MFW_DRV_MSG_TYPE type, u8 * enabled)
{
	u32 disabled_attns = 0;
	int rc;

	rc = qed_mcp_get_disabled_attns(p_hwfn, p_ptt, &disabled_attns);
	if (rc)
		return rc;

	*enabled = !(disabled_attns & BIT(type));

	return 0;
}

static int
qed_mcp_set_attn_control(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 enum MFW_DRV_MSG_TYPE type, u8 enable)
{
	struct qed_mcp_mb_params mb_params;
	u32 disabled_attns = 0;
	u8 is_enabled;
	int rc;

	rc = qed_mcp_get_disabled_attns(p_hwfn, p_ptt, &disabled_attns);
	if (rc)
		return rc;

	is_enabled = (~disabled_attns >> type) & 0x1;
	if (is_enabled == enable)
		return 0;

	if (enable)
		disabled_attns &= ~BIT(type);
	else
		disabled_attns |= BIT(type);

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_SET_ATTN_CONTROL;
	mb_params.p_data_src = &disabled_attns;
	mb_params.data_src_size = sizeof(disabled_attns);
	mb_params.flags = QED_MB_FLAG_CAN_SLEEP;

	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc) {
		DP_NOTICE(p_hwfn, "SET_ATTN_CONTROL failed, rc = %d\n", rc);
		return rc;
	}

	return rc;
}

int
qed_mcp_is_tx_flt_attn_enabled(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, u8 * enabled)
{
	return qed_mcp_get_attn_control(p_hwfn, p_ptt,
					MFW_DRV_MSG_XCVR_TX_FAULT, enabled);
}

int
qed_mcp_is_rx_los_attn_enabled(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, u8 * enabled)
{
	return qed_mcp_get_attn_control(p_hwfn, p_ptt,
					MFW_DRV_MSG_XCVR_RX_LOS, enabled);
}

int
qed_mcp_enable_tx_flt_attn(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, u8 enable)
{
	return qed_mcp_set_attn_control(p_hwfn, p_ptt,
					MFW_DRV_MSG_XCVR_TX_FAULT, enable);
}

int
qed_mcp_enable_rx_los_attn(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, u8 enable)
{
	return qed_mcp_set_attn_control(p_hwfn, p_ptt,
					MFW_DRV_MSG_XCVR_RX_LOS, enable);
}

int
qed_mcp_set_trace_filter(struct qed_hwfn *p_hwfn,
			 u32 * dbg_level, u32 * dbg_modules)
{
	struct trace_filter_stc trace_filter;
	struct qed_ptt *p_ptt;
	u32 resp = 0, param;
	int rc;

	trace_filter.level = *dbg_level;
	trace_filter.modules = *dbg_modules;

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return -EBUSY;

	rc = qed_mcp_nvm_wr_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_SET_TRACE_FILTER,
				sizeof(trace_filter), &resp,
				&param, sizeof(trace_filter),
				(u32 *) & trace_filter, true);
	if (rc)
		DP_NOTICE(p_hwfn, "MCP command rc = %d\n", rc);
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

int qed_mcp_restore_trace_filter(struct qed_hwfn *p_hwfn)
{
	struct qed_ptt *p_ptt;
	u32 resp = 0, param;
	int rc;

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return -EBUSY;

	rc = qed_mcp_nvm_wr_cmd(p_hwfn, p_ptt,
				DRV_MSG_CODE_RESTORE_TRACE_FILTER,
				0, &resp, &param, 0, NULL, true);
	if (rc)
		DP_NOTICE(p_hwfn, "MCP command rc = %d\n", rc);
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

static void qed_mcp_handle_critical_error(struct qed_hwfn *p_hwfn,
					  struct qed_ptt *p_ptt)
{
	struct qed_mdump_retain_data mdump_retain;
	int rc;

	/* In CMT mode - no need for more than a single acknowledgement to the
	 * MFW, and no more than a single notification to the upper driver.
	 */
	if (p_hwfn != QED_LEADING_HWFN(p_hwfn->cdev))
		return;

	rc = qed_mcp_mdump_get_retain(p_hwfn, p_ptt, &mdump_retain);
	if (rc == 0 && mdump_retain.valid)
		DP_NOTICE(p_hwfn,
			  "The MFW notified that a critical error occurred in the device [epoch 0x%08x, pf 0x%x, status 0x%08x]\n",
			  mdump_retain.epoch,
			  mdump_retain.pf, mdump_retain.status);
	else
		DP_NOTICE(p_hwfn,
			  "The MFW notified that a critical error occurred in the device\n");

	if (p_hwfn->cdev->allow_mdump) {
		DP_NOTICE(p_hwfn,
			  "Not acknowledging the notification to allow the MFW crash dump\n");
		return;
	}

	DP_NOTICE(p_hwfn,
		  "Acknowledging the notification to not allow the MFW crash dump [driver debug data collection is preferable]\n");
	qed_mcp_mdump_ack(p_hwfn, p_ptt);
	qed_hw_err_notify(p_hwfn, p_ptt, QED_HW_ERR_HW_ATTN, NULL);
}

void qed_mcp_read_qinq_config(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct public_func shmem_info;

	if (!test_bit(QED_MF_QINQ_SPECIFIC, &p_hwfn->cdev->mf_bits))
		return;

	qed_mcp_get_shmem_func(p_hwfn, p_ptt, &shmem_info, MCP_PF_ID(p_hwfn));
	p_hwfn->qinq_tc = GET_MFW_FIELD(shmem_info.oem_cfg_func,
					OEM_CFG_FUNC_TC);

	DP_NOTICE(p_hwfn, "QINQ shmem config: tc = %d, port_id 0x%02x\n",
		  p_hwfn->qinq_tc, MFW_PORT(p_hwfn));
}

void qed_mcp_read_ufp_config(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct public_func shmem_info;
	u32 port_cfg, val;

	if (!test_bit(QED_MF_UFP_SPECIFIC, &p_hwfn->cdev->mf_bits))
		return;

	memset(&p_hwfn->ufp_info, 0, sizeof(p_hwfn->ufp_info));
	port_cfg = qed_rd(p_hwfn, p_ptt, p_hwfn->mcp_info->port_addr +
			  offsetof(struct public_port, oem_cfg_port));
	val = GET_MFW_FIELD(port_cfg, OEM_CFG_CHANNEL_TYPE);
	if (val != OEM_CFG_CHANNEL_TYPE_STAGGED)
		DP_NOTICE(p_hwfn,
			  "Incorrect UFP Channel type  %d port_id 0x%02x\n",
			  val, MFW_PORT(p_hwfn));

	val = GET_MFW_FIELD(port_cfg, OEM_CFG_SCHED_TYPE);
	if (val == OEM_CFG_SCHED_TYPE_ETS) {
		p_hwfn->ufp_info.mode = QED_UFP_MODE_ETS;
	} else if (val == OEM_CFG_SCHED_TYPE_VNIC_BW) {
		p_hwfn->ufp_info.mode = QED_UFP_MODE_VNIC_BW;
	} else {
		p_hwfn->ufp_info.mode = QED_UFP_MODE_UNKNOWN;
		DP_NOTICE(p_hwfn,
			  "Unknown UFP scheduling mode %d port_id 0x%02x\n",
			  val, MFW_PORT(p_hwfn));
	}

	qed_mcp_get_shmem_func(p_hwfn, p_ptt, &shmem_info, MCP_PF_ID(p_hwfn));
	val = GET_MFW_FIELD(shmem_info.oem_cfg_func, OEM_CFG_FUNC_TC);
	p_hwfn->ufp_info.tc = (u8) val;
	val = GET_MFW_FIELD(shmem_info.oem_cfg_func,
			    OEM_CFG_FUNC_HOST_PRI_CTRL);
	if (val == OEM_CFG_FUNC_HOST_PRI_CTRL_VNIC) {
		p_hwfn->ufp_info.pri_type = QED_UFP_PRI_VNIC;
	} else if (val == OEM_CFG_FUNC_HOST_PRI_CTRL_OS) {
		p_hwfn->ufp_info.pri_type = QED_UFP_PRI_OS;
	} else {
		p_hwfn->ufp_info.pri_type = QED_UFP_PRI_UNKNOWN;
		DP_NOTICE(p_hwfn,
			  "Unknown Host priority control %d port_id 0x%02x\n",
			  val, MFW_PORT(p_hwfn));
	}

	DP_NOTICE(p_hwfn,
		  "UFP shmem config: mode = %d tc = %d pri_type = %d port_id 0x%02x\n",
		  p_hwfn->ufp_info.mode,
		  p_hwfn->ufp_info.tc,
		  p_hwfn->ufp_info.pri_type, MFW_PORT(p_hwfn));
}

static int
qed_mcp_handle_ufp_event(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	qed_mcp_read_ufp_config(p_hwfn, p_ptt);

	if (p_hwfn->ufp_info.mode == QED_UFP_MODE_VNIC_BW) {
		p_hwfn->qm_info.ooo_tc = p_hwfn->ufp_info.tc;
		qed_hw_info_set_offload_tc(&p_hwfn->hw_info,
					   p_hwfn->ufp_info.tc);
		qed_qm_reconf_intr(p_hwfn, p_ptt);
	} else if (p_hwfn->ufp_info.mode == QED_UFP_MODE_ETS) {
		/* Merge UFP TC with the dcbx TC data */
		qed_dcbx_mib_update_event(p_hwfn, p_ptt,
					  QED_DCBX_OPERATIONAL_MIB);
	} else {
		DP_ERR(p_hwfn, "Invalid sched type, discard the UFP config\n");
		return -EINVAL;
	}

	/* update storm FW with negotiation results */
	qed_sp_pf_update_ufp(p_hwfn);

	/* update stag pcp value */
	qed_sp_pf_update_stag(p_hwfn);

	return 0;
}

#define QED_FCOE_CAP(val) (b_undefined ? FCOE_CAP_UNDEFINED_VALUE : (val))

static void qed_mcp_send_fcoe_capabilities(struct qed_hwfn *p_hwfn,
					   struct qed_ptt *p_ptt)
{
	struct qed_mcp_mb_params mb_params;
	struct fcoe_cap_stc mfw_fcoe_caps;
	struct qed_fcoe_caps fcoe_caps;
	bool b_undefined = false;
	int rc;

	memset(&fcoe_caps, 0, sizeof(fcoe_caps));
	if (p_hwfn->hw_info.personality != QED_PCI_FCOE) {
		DP_INFO(p_hwfn,
			"MFW requested fcoe capabilities on non fcoe PF. Setting as undefined.\n");
		b_undefined = true;
	} else {
		rc = qed_get_fcoe_capabilities(p_hwfn->cdev, &fcoe_caps);
		if (rc) {
			DP_INFO(p_hwfn,
				"Failed to get FCoE capabilities from upper driver [rc %d]. Setting as undefined.\n",
				rc);
			b_undefined = true;
		}
	}

	memset(&mfw_fcoe_caps, 0, sizeof(mfw_fcoe_caps));
	SET_MFW_FIELD(mfw_fcoe_caps.max_ios, FCOE_CAP_IOS,
		      QED_FCOE_CAP(fcoe_caps.max_ios));
	SET_MFW_FIELD(mfw_fcoe_caps.max_log, FCOE_CAP_LOG,
		      QED_FCOE_CAP(fcoe_caps.max_log));
	SET_MFW_FIELD(mfw_fcoe_caps.max_exch, FCOE_CAP_EXCH,
		      QED_FCOE_CAP(fcoe_caps.max_exch));
	SET_MFW_FIELD(mfw_fcoe_caps.max_npiv, FCOE_CAP_NPIV,
		      QED_FCOE_CAP(fcoe_caps.max_npiv));
	SET_MFW_FIELD(mfw_fcoe_caps.max_tgt, FCOE_CAP_TGT,
		      QED_FCOE_CAP(fcoe_caps.max_tgt));
	SET_MFW_FIELD(mfw_fcoe_caps.max_outstnd, FCOE_CAP_OUTSTND,
		      QED_FCOE_CAP(fcoe_caps.max_outstnd));

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_GET_FCOE_CAP;
	mb_params.p_data_src = &mfw_fcoe_caps;
	mb_params.data_src_size = sizeof(mfw_fcoe_caps);
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		DP_NOTICE(p_hwfn,
			  "Failed to send FCoE capabilities to the MFW [rc %d]\n",
			  rc);
}

int qed_mcp_handle_events(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mcp_info *info = p_hwfn->mcp_info;
	int rc = 0;
	bool found = false;
	u16 i;

	DP_VERBOSE(p_hwfn, QED_MSG_SP, "Received message from MFW\n");

	/* Read Messages from MFW */
	qed_mcp_read_mb(p_hwfn, p_ptt);

	/* Compare current messages to old ones */
	for (i = 0; i < info->mfw_mb_length; i++) {
		if (info->mfw_mb_cur[i] == info->mfw_mb_shadow[i])
			continue;

		found = true;

		DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
			   "Msg [%d] - old CMD 0x%02x, new CMD 0x%02x\n",
			   i, info->mfw_mb_shadow[i], info->mfw_mb_cur[i]);

		spin_lock_bh(&p_hwfn->mcp_info->unload_lock);
		if (test_bit(QED_MCP_BYPASS_PROC_BIT,
			     &p_hwfn->mcp_info->mcp_handling_status)) {
			spin_unlock_bh(&p_hwfn->mcp_info->unload_lock);
			DP_INFO(p_hwfn,
				"Msg [%d] is bypassed on unload flow\n", i);
			continue;
		}

		set_bit(QED_MCP_IN_PROCESSING_BIT,
			&p_hwfn->mcp_info->mcp_handling_status);
		spin_unlock_bh(&p_hwfn->mcp_info->unload_lock);

		switch (i) {
		case MFW_DRV_MSG_LINK_CHANGE:
			qed_mcp_handle_link_change(p_hwfn, p_ptt, false);
			break;
		case MFW_DRV_MSG_VF_DISABLED:
			qed_mcp_handle_vf_flr(p_hwfn, p_ptt);
			break;
		case MFW_DRV_MSG_LLDP_DATA_UPDATED:
			qed_dcbx_mib_update_event(p_hwfn, p_ptt,
						  QED_DCBX_REMOTE_LLDP_MIB);
			break;
		case MFW_DRV_MSG_DCBX_REMOTE_MIB_UPDATED:
			qed_dcbx_mib_update_event(p_hwfn, p_ptt,
						  QED_DCBX_REMOTE_MIB);
			break;
		case MFW_DRV_MSG_DCBX_OPERATIONAL_MIB_UPDATED:
			qed_dcbx_mib_update_event(p_hwfn, p_ptt,
						  QED_DCBX_OPERATIONAL_MIB);
			/* clear the user-config cache */
			memset(&p_hwfn->p_dcbx_info->set, 0,
			       sizeof(struct qed_dcbx_set));
			break;
		case MFW_DRV_MSG_DCBX_ADMIN_CFG_APPLIED:
			p_hwfn->p_dcbx_info->is_admin_cfg_applied = true;
			qed_dcbx_mib_update_event(p_hwfn, p_ptt,
						  QED_DCBX_OPERATIONAL_MIB);
			/* clear the user-config cache */
			memset(&p_hwfn->p_dcbx_info->set, 0,
			       sizeof(struct qed_dcbx_set));
			break;
		case MFW_DRV_MSG_LLDP_RECEIVED_TLVS_UPDATED:
			qed_lldp_mib_update_event(p_hwfn, p_ptt);
			break;
		case MFW_DRV_MSG_OEM_CFG_UPDATE:
			qed_mcp_handle_ufp_event(p_hwfn, p_ptt);
			break;
		case MFW_DRV_MSG_TRANSCEIVER_STATE_CHANGE:
			qed_mcp_handle_transceiver_change(p_hwfn, p_ptt);
			break;
		case MFW_DRV_MSG_XCVR_TX_FAULT:
			qed_mcp_handle_transceiver_tx_fault(p_hwfn);
			break;
		case MFW_DRV_MSG_XCVR_RX_LOS:
			qed_mcp_handle_transceiver_rx_los(p_hwfn);
			break;
		case MFW_DRV_MSG_ERROR_RECOVERY:
			qed_mcp_handle_process_kill(p_hwfn, p_ptt);
			break;
		case MFW_DRV_MSG_GET_LAN_STATS:
		case MFW_DRV_MSG_GET_FCOE_STATS:
		case MFW_DRV_MSG_GET_ISCSI_STATS:
		case MFW_DRV_MSG_GET_RDMA_STATS:
			qed_mcp_send_protocol_stats(p_hwfn, p_ptt, i);
			break;
		case MFW_DRV_MSG_BW_UPDATE:
			qed_mcp_update_bw(p_hwfn, p_ptt);
			break;
		case MFW_DRV_MSG_S_TAG_UPDATE:
			qed_mcp_update_stag(p_hwfn, p_ptt);
			break;
		case MFW_DRV_MSG_FAILURE_DETECTED:
			qed_mcp_handle_fan_failure(p_hwfn, p_ptt);
			break;
		case MFW_DRV_MSG_CRITICAL_ERROR_OCCURRED:
			qed_mcp_handle_critical_error(p_hwfn, p_ptt);
			break;
		case MFW_DRV_MSG_GET_TLV_REQ:
			qed_mfw_tlv_req(p_hwfn);
			break;
		case MFW_DRV_MSG_GET_FCOE_CAP:
			qed_mcp_send_fcoe_capabilities(p_hwfn, p_ptt);
			break;
		default:
			DP_INFO(p_hwfn, "Unimplemented MFW message %d\n", i);
			rc = -EINVAL;
		}

		clear_bit(QED_MCP_IN_PROCESSING_BIT,
			  &p_hwfn->mcp_info->mcp_handling_status);
	}

	/* ACK everything */
	for (i = 0; i < MFW_DRV_MSG_MAX_DWORDS(info->mfw_mb_length); i++) {
		__be32 val = cpu_to_be32(((u32 *) info->mfw_mb_cur)[i]);

		/* MFW expect answer in BE, so we force write in that format */
		qed_wr(p_hwfn, p_ptt,
		       info->mfw_mb_addr + sizeof(u32) +
		       MFW_DRV_MSG_MAX_DWORDS(info->mfw_mb_length) *
		       sizeof(u32) + i * sizeof(u32), val);
	}

	if (!found) {
		DP_INFO(p_hwfn,
			"Received an MFW message indication but no new message!\n");
		rc = -EINVAL;
	}

	/* Copy the new mfw messages into the shadow */
	memcpy(info->mfw_mb_shadow, info->mfw_mb_cur, info->mfw_mb_length);

	return rc;
}

int qed_mcp_get_mfw_ver(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt,
			u32 * p_mfw_ver, u32 * p_running_bundle_id)
{
	u32 global_offsize;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->cdev) && !qed_mcp_is_init(p_hwfn)) {
		DP_INFO(p_hwfn, "Emulation: Can't get MFW version\n");
		return -EOPNOTSUPP;
	}
#endif

	if (!p_ptt)
		return -EINVAL;

	if (IS_VF(p_hwfn->cdev)) {
		if (p_hwfn->vf_iov_info) {
			struct pfvf_acquire_resp_tlv *p_resp;

			p_resp = &p_hwfn->vf_iov_info->acquire_resp;
			*p_mfw_ver = p_resp->pfdev_info.mfw_ver;
			return 0;
		} else {
			DP_VERBOSE(p_hwfn,
				   QED_MSG_IOV,
				   "VF requested MFW version prior to ACQUIRE\n");
			return -EINVAL;
		}
	}

	global_offsize = qed_rd(p_hwfn, p_ptt,
				SECTION_OFFSIZE_ADDR(p_hwfn->
						     mcp_info->public_base,
						     PUBLIC_GLOBAL));
	*p_mfw_ver =
	    qed_rd(p_hwfn, p_ptt,
		   SECTION_ADDR(global_offsize,
				0) + offsetof(struct public_global, mfw_ver));

	if (p_running_bundle_id != NULL) {
		*p_running_bundle_id = qed_rd(p_hwfn, p_ptt,
					      SECTION_ADDR(global_offsize, 0) +
					      offsetof(struct public_global,
						       running_bundle_id));
	}

	return 0;
}

int qed_mcp_get_mbi_ver(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt, u32 * p_mbi_ver)
{
	u32 nvm_cfg_addr, nvm_cfg1_offset, mbi_ver_addr;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->cdev) && !qed_mcp_is_init(p_hwfn)) {
		DP_INFO(p_hwfn, "Emulation: Can't get MBI version\n");
		return -EOPNOTSUPP;
	}
#endif

	if (IS_VF(p_hwfn->cdev))
		return -EINVAL;

	/* Read the address of the nvm_cfg */
	nvm_cfg_addr = qed_rd(p_hwfn, p_ptt, MISC_REG_GEN_PURP_CR0);
	if (!nvm_cfg_addr) {
		DP_NOTICE(p_hwfn, "Shared memory not initialized\n");
		return -EINVAL;
	}

	/* Read the offset of nvm_cfg1 */
	nvm_cfg1_offset = qed_rd(p_hwfn, p_ptt, nvm_cfg_addr + 4);

	mbi_ver_addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
	    offsetof(struct nvm_cfg1, glob)+offsetof(struct nvm_cfg1_glob,
						     mbi_version);
	*p_mbi_ver =
	    qed_rd(p_hwfn, p_ptt,
		   mbi_ver_addr) & (NVM_CFG1_GLOB_MBI_VERSION_0_MASK |
				    NVM_CFG1_GLOB_MBI_VERSION_1_MASK |
				    NVM_CFG1_GLOB_MBI_VERSION_2_MASK);

	return 0;
}

int qed_mcp_get_media_type(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, u32 * p_media_type)
{
	*p_media_type = MEDIA_UNSPECIFIED;

	/* TODO - Add support for VFs */
	if (IS_VF(p_hwfn->cdev))
		return -EINVAL;

	if (!qed_mcp_is_init(p_hwfn)) {
#ifndef ASIC_ONLY
		if (CHIP_REV_IS_EMUL(p_hwfn->cdev)) {
			DP_INFO(p_hwfn, "Emulation: Can't get media type\n");
			return -EOPNOTSUPP;
		}
#endif
		DP_NOTICE(p_hwfn, "MFW is not initialized!\n");
		return -EBUSY;
	}

	if (!p_ptt)
		return -EINVAL;

	*p_media_type = qed_rd(p_hwfn, p_ptt,
			       p_hwfn->mcp_info->port_addr +
			       offsetof(struct public_port, media_type));

	return 0;
}

int qed_mcp_get_transceiver_data(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt,
				 u32 * p_transceiver_state,
				 u32 * p_transceiver_type)
{
	u32 transceiver_info;

	if (QED_RECOV_IN_PROG(p_hwfn->cdev)) {
		DP_ERR(p_hwfn, "Error recovery in progress\n");
		return -EAGAIN;
	}

	if (!p_ptt)
		return -EINVAL;

	*p_transceiver_type = ETH_TRANSCEIVER_TYPE_NONE;
	*p_transceiver_state = ETH_TRANSCEIVER_STATE_UPDATING;

	/* TODO - Add support for VFs */
	if (IS_VF(p_hwfn->cdev))
		return -EINVAL;

	if (!qed_mcp_is_init(p_hwfn)) {
#ifndef ASIC_ONLY
		if (CHIP_REV_IS_EMUL(p_hwfn->cdev)) {
			DP_INFO(p_hwfn,
				"Emulation: Can't get transceiver data\n");
			return -EOPNOTSUPP;
		}
#endif
		DP_NOTICE(p_hwfn, "MFW is not initialized!\n");
		return -EBUSY;
	}

	transceiver_info = qed_rd(p_hwfn, p_ptt,
				  p_hwfn->mcp_info->port_addr +
				  offsetof(struct public_port,
					   transceiver_data));
	*p_transceiver_state = GET_MFW_FIELD(transceiver_info,
					     ETH_TRANSCEIVER_STATE);

	if (*p_transceiver_state == ETH_TRANSCEIVER_STATE_PRESENT)
		*p_transceiver_type = GET_MFW_FIELD(transceiver_info,
						    ETH_TRANSCEIVER_TYPE);
	else
		*p_transceiver_type = ETH_TRANSCEIVER_TYPE_UNKNOWN;

	return 0;
}

static int is_transceiver_ready(u32 transceiver_state, u32 transceiver_type)
{
	if ((transceiver_state & ETH_TRANSCEIVER_STATE_PRESENT) &&
	    ((transceiver_state & ETH_TRANSCEIVER_STATE_UPDATING) == 0x0) &&
	    (transceiver_type != ETH_TRANSCEIVER_TYPE_NONE))
		return 1;

	return 0;
}

int qed_mcp_trans_speed_mask(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, u32 * p_speed_mask)
{
	u32 transceiver_type, transceiver_state;
	int ret;

	ret = qed_mcp_get_transceiver_data(p_hwfn, p_ptt, &transceiver_state,
					   &transceiver_type);
	if (ret)
		return ret;

	if (is_transceiver_ready(transceiver_state, transceiver_type) == 0)
		return -EINVAL;

	switch (transceiver_type) {
	case ETH_TRANSCEIVER_TYPE_1G_LX:
	case ETH_TRANSCEIVER_TYPE_1G_SX:
	case ETH_TRANSCEIVER_TYPE_1G_PCC:
	case ETH_TRANSCEIVER_TYPE_1G_ACC:
	case ETH_TRANSCEIVER_TYPE_1000BASET:
		*p_speed_mask = NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_1G;
		break;

	case ETH_TRANSCEIVER_TYPE_10G_SR:
	case ETH_TRANSCEIVER_TYPE_10G_LR:
	case ETH_TRANSCEIVER_TYPE_10G_LRM:
	case ETH_TRANSCEIVER_TYPE_10G_ER:
	case ETH_TRANSCEIVER_TYPE_10G_PCC:
	case ETH_TRANSCEIVER_TYPE_10G_ACC:
	case ETH_TRANSCEIVER_TYPE_4x10G:
		*p_speed_mask = NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_10G;
		break;

	case ETH_TRANSCEIVER_TYPE_40G_LR4:
	case ETH_TRANSCEIVER_TYPE_40G_SR4:
	case ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_SR:
	case ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_LR:
		*p_speed_mask = NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_40G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_10G;
		break;

	case ETH_TRANSCEIVER_TYPE_100G_AOC:
	case ETH_TRANSCEIVER_TYPE_100G_SR4:
	case ETH_TRANSCEIVER_TYPE_100G_LR4:
	case ETH_TRANSCEIVER_TYPE_100G_ER4:
	case ETH_TRANSCEIVER_TYPE_100G_ACC:
		*p_speed_mask =
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_BB_AHP_100G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_25G;
		break;

	case ETH_TRANSCEIVER_TYPE_25G_SR:
	case ETH_TRANSCEIVER_TYPE_25G_LR:
	case ETH_TRANSCEIVER_TYPE_25G_AOC:
	case ETH_TRANSCEIVER_TYPE_25G_ACC_S:
	case ETH_TRANSCEIVER_TYPE_25G_ACC_M:
	case ETH_TRANSCEIVER_TYPE_25G_ACC_L:
		*p_speed_mask = NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_25G;
		break;

	case ETH_TRANSCEIVER_TYPE_25G_CA_N:
	case ETH_TRANSCEIVER_TYPE_25G_CA_S:
	case ETH_TRANSCEIVER_TYPE_25G_CA_L:
	case ETH_TRANSCEIVER_TYPE_4x25G_CR:
		*p_speed_mask = NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_25G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_10G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_1G;
		break;
	case ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_25G_SR:
	case ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_25G_LR:
		*p_speed_mask = NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_25G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_10G;
		break;

	case ETH_TRANSCEIVER_TYPE_40G_CR4:
	case ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_CR:
		*p_speed_mask = NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_40G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_10G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_1G;
		break;

	case ETH_TRANSCEIVER_TYPE_100G_CR4:
	case ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_CR:
		*p_speed_mask =
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_BB_AHP_100G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_50G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_40G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_25G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_20G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_10G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_1G;
		break;

	case ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_SR:
	case ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_LR:
	case ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_AOC:
		*p_speed_mask =
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_BB_AHP_100G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_40G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_25G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_10G;
		break;

	case ETH_TRANSCEIVER_TYPE_XLPPI:
		*p_speed_mask = NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_40G;
		break;

	case ETH_TRANSCEIVER_TYPE_10G_BASET:
	case ETH_TRANSCEIVER_TYPE_MULTI_RATE_1G_10G_SR:
	case ETH_TRANSCEIVER_TYPE_MULTI_RATE_1G_10G_LR:
		*p_speed_mask = NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_10G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_1G;
		break;

	case ETH_TRANSCEIVER_TYPE_50G_CR:
		*p_speed_mask = NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_50G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_25G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_10G |
		    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_1G;
		break;

	default:
		DP_INFO(p_hwfn, "Unknown transceiver type 0x%x\n",
			transceiver_type);
		*p_speed_mask = 0xff;
		break;
	}

	return 0;
}

int qed_mcp_get_board_config(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, u32 * p_board_config)
{
	u32 nvm_cfg_addr, nvm_cfg1_offset, port_cfg_addr;

	/* TODO - Add support for VFs */
	if (IS_VF(p_hwfn->cdev))
		return -EINVAL;

	if (!qed_mcp_is_init(p_hwfn)) {
#ifndef ASIC_ONLY
		if (CHIP_REV_IS_EMUL(p_hwfn->cdev)) {
			DP_INFO(p_hwfn, "Emulation: Can't get board config\n");
			return -EOPNOTSUPP;
		}
#endif
		DP_NOTICE(p_hwfn, "MFW is not initialized!\n");
		return -EBUSY;
	}
	if (!p_ptt) {
		*p_board_config = NVM_CFG1_PORT_PORT_TYPE_UNDEFINED;
		return -EINVAL;
	} else {
		nvm_cfg_addr = qed_rd(p_hwfn, p_ptt, MISC_REG_GEN_PURP_CR0);
		nvm_cfg1_offset = qed_rd(p_hwfn, p_ptt, nvm_cfg_addr + 4);
		port_cfg_addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
		    offsetof(struct nvm_cfg1, port[MFW_PORT(p_hwfn)]);
		*p_board_config = qed_rd(p_hwfn, p_ptt,
					 port_cfg_addr +
					 offsetof(struct nvm_cfg1_port,
						  board_cfg));
	}

	return 0;
}

/* Old MFW has a global configuration for all PFs regarding RDMA support */
static void
qed_mcp_get_shmem_proto_legacy(struct qed_hwfn *p_hwfn,
			       enum qed_pci_personality *p_proto)
{
	/* There wasn't ever a legacy MFW that published iwarp.
	 * So at this point, this is either plain l2 or RoCE.
	 */
	if (test_bit(QED_DEV_CAP_ROCE, &p_hwfn->hw_info.device_capabilities))
		*p_proto = QED_PCI_ETH_ROCE;
	else
		*p_proto = QED_PCI_ETH;

	DP_VERBOSE(p_hwfn, NETIF_MSG_IFUP,
		   "According to Legacy capabilities, L2 personality is %08x\n",
		   (u32) * p_proto);
}

static int
qed_mcp_get_shmem_proto_mfw(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt,
			    enum qed_pci_personality *p_proto)
{
	u32 resp = 0, param = 0;
	int rc;

	rc = qed_mcp_cmd(p_hwfn, p_ptt,
			 DRV_MSG_CODE_GET_PF_RDMA_PROTOCOL, 0, &resp, &param);
	if (rc)
		return rc;
	if (resp != FW_MSG_CODE_OK) {
		DP_VERBOSE(p_hwfn, NETIF_MSG_IFUP,
			   "MFW lacks support for command; Returns %08x\n",
			   resp);
		return -EINVAL;
	}

	switch (param) {
	case FW_MB_PARAM_GET_PF_RDMA_NONE:
		*p_proto = QED_PCI_ETH;
		break;
	case FW_MB_PARAM_GET_PF_RDMA_ROCE:
		*p_proto = QED_PCI_ETH_ROCE;
		break;
	case FW_MB_PARAM_GET_PF_RDMA_IWARP:
		*p_proto = QED_PCI_ETH_IWARP;
		break;
	case FW_MB_PARAM_GET_PF_RDMA_BOTH:
		*p_proto = QED_PCI_ETH_RDMA;
		break;
	default:
		DP_NOTICE(p_hwfn,
			  "MFW answers GET_PF_RDMA_PROTOCOL but param is %08x\n",
			  param);
		return -EINVAL;
	}

	DP_VERBOSE(p_hwfn,
		   NETIF_MSG_IFUP,
		   "According to capabilities, L2 personality is %08x [resp %08x param %08x]\n",
		   (u32) * p_proto, resp, param);
	return 0;
}

static int
qed_mcp_get_shmem_proto(struct qed_hwfn *p_hwfn,
			struct public_func *p_info,
			struct qed_ptt *p_ptt,
			enum qed_pci_personality *p_proto)
{
	int rc = 0;

	switch (p_info->config & FUNC_MF_CFG_PROTOCOL_MASK) {
	case FUNC_MF_CFG_PROTOCOL_ETHERNET:
		if (qed_mcp_get_shmem_proto_mfw(p_hwfn, p_ptt, p_proto) != 0)
			qed_mcp_get_shmem_proto_legacy(p_hwfn, p_proto);
		break;
	case FUNC_MF_CFG_PROTOCOL_ISCSI:
		*p_proto = QED_PCI_ISCSI;
		break;
	case FUNC_MF_CFG_PROTOCOL_FCOE:
		*p_proto = QED_PCI_FCOE;
		break;
	case FUNC_MF_CFG_PROTOCOL_ROCE:
		DP_NOTICE(p_hwfn, "RoCE personality is not a valid value!\n");
		COMPAT_FALLTHROUGH;
		/* fallthrough */
	default:
		rc = -EINVAL;
	}

	return rc;
}

int qed_mcp_fill_shmem_func_info(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mcp_function_info *info;
	struct public_func shmem_info;

	qed_mcp_get_shmem_func(p_hwfn, p_ptt, &shmem_info, MCP_PF_ID(p_hwfn));
	info = &p_hwfn->mcp_info->func_info;

	info->pause_on_host = (shmem_info.config &
			       FUNC_MF_CFG_PAUSE_ON_HOST_RING) ? 1 : 0;

	if (qed_mcp_get_shmem_proto(p_hwfn, &shmem_info, p_ptt,
				    &info->protocol)) {
		DP_ERR(p_hwfn, "Unknown personality %08x\n",
		       (u32) (shmem_info.config & FUNC_MF_CFG_PROTOCOL_MASK));
		return -EINVAL;
	}

	qed_read_pf_bandwidth(p_hwfn, &shmem_info);

	if (shmem_info.mac_upper || shmem_info.mac_lower) {
		info->mac[0] = (u8) (shmem_info.mac_upper >> 8);
		info->mac[1] = (u8) (shmem_info.mac_upper);
		info->mac[2] = (u8) (shmem_info.mac_lower >> 24);
		info->mac[3] = (u8) (shmem_info.mac_lower >> 16);
		info->mac[4] = (u8) (shmem_info.mac_lower >> 8);
		info->mac[5] = (u8) (shmem_info.mac_lower);

		/* Store primary MAC for later possible WoL */
		memcpy(p_hwfn->cdev->wol_mac, info->mac, ETH_ALEN);
	} else {
		/* TODO - are there protocols for which there's no MAC? */
		DP_NOTICE(p_hwfn, "MAC is 0 in shmem\n");
	}

	/* TODO - are these calculations true for BE machine? */
	info->wwn_port = (u64) shmem_info.fcoe_wwn_port_name_lower |
	    (((u64) shmem_info.fcoe_wwn_port_name_upper) << 32);
	info->wwn_node = (u64) shmem_info.fcoe_wwn_node_name_lower |
	    (((u64) shmem_info.fcoe_wwn_node_name_upper) << 32);

	info->ovlan = (u16) (shmem_info.ovlan_stag & FUNC_MF_CFG_OV_STAG_MASK);

	info->mtu = (u16) shmem_info.mtu_size;

	p_hwfn->hw_info.b_wol_support = QED_WOL_SUPPORT_NONE;
	p_hwfn->cdev->wol_config = (u8) QED_OV_WOL_DEFAULT;
	if (qed_mcp_is_init(p_hwfn)) {
		u32 resp = 0, param = 0;
		int rc;

		rc = qed_mcp_cmd(p_hwfn, p_ptt,
				 DRV_MSG_CODE_OS_WOL, 0, &resp, &param);
		if (rc)
			return rc;
		if (resp == FW_MSG_CODE_OS_WOL_SUPPORTED)
			p_hwfn->hw_info.b_wol_support = QED_WOL_SUPPORT_PME;
	}

	DP_VERBOSE(p_hwfn,
		   (QED_MSG_SP | NETIF_MSG_IFUP),
		   "Read configuration from shmem: pause_on_host %02x protocol %02x BW [%02x - %02x] MAC %02x:%02x:%02x:%02x:%02x:%02x wwn port %llx node %llx ovlan %04x wol %02x\n",
		   info->pause_on_host,
		   info->protocol,
		   info->bandwidth_min,
		   info->bandwidth_max,
		   info->mac[0],
		   info->mac[1],
		   info->mac[2],
		   info->mac[3],
		   info->mac[4],
		   info->mac[5],
		   info->wwn_port,
		   info->wwn_node,
		   info->ovlan, (u8) p_hwfn->hw_info.b_wol_support);

	return 0;
}

struct qed_mcp_link_params
*qed_mcp_get_link_params(struct qed_hwfn *p_hwfn)
{
	if (!p_hwfn || !p_hwfn->mcp_info)
		return NULL;
	return &p_hwfn->mcp_info->link_input;
}

struct qed_mcp_link_state
*qed_mcp_get_link_state(struct qed_hwfn *p_hwfn)
{
	if (!p_hwfn || !p_hwfn->mcp_info)
		return NULL;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_SLOW(p_hwfn->cdev)) {
		DP_INFO(p_hwfn, "Non-ASIC - always notify that link is up\n");
		p_hwfn->mcp_info->link_output.link_up = true;
	}
#endif

	return &p_hwfn->mcp_info->link_output;
}

struct qed_mcp_link_capabilities
*qed_mcp_get_link_capabilities(struct qed_hwfn *p_hwfn)
{
	if (!p_hwfn || !p_hwfn->mcp_info)
		return NULL;
	return &p_hwfn->mcp_info->link_capabilities;
}

static int _qed_mcp_drain(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 resp = 0, param = 0;
	int rc;
	int i;

	rc = qed_mcp_cmd(p_hwfn, p_ptt,
			 DRV_MSG_CODE_NIG_DRAIN, 1000, &resp, &param);

	/* Wait for the drain to complete before returning */
	if (p_hwfn->pf_params.eth_pf_params.is_ens_mode_enabled)
		for (i = 0; i < 1000; i++)
			udelay(1020);
	else
		msleep(1020);

	return rc;
}

int qed_mcp_drain(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	int rc = 0;

	if (!p_hwfn->mcp_drain_inprogress) {
		p_hwfn->mcp_drain_inprogress = true;
		rc = _qed_mcp_drain(p_hwfn, p_ptt);
		p_hwfn->mcp_drain_inprogress = false;
	} else {
		DP_INFO(p_hwfn, "MCP drain already in progress\n");
	}
	return rc;
}

int qed_mcp_get_flash_size(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, u32 * p_flash_size)
{
	u32 flash_size;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->cdev) && !qed_mcp_is_init(p_hwfn)) {
		DP_INFO(p_hwfn, "Emulation: Can't get flash size\n");
		return -EOPNOTSUPP;
	}
#endif

	if (IS_VF(p_hwfn->cdev))
		return -EINVAL;

	flash_size = qed_rd(p_hwfn, p_ptt, MCP_REG_NVM_CFG4);
	flash_size = (flash_size & MCP_REG_NVM_CFG4_FLASH_SIZE) >>
	    MCP_REG_NVM_CFG4_FLASH_SIZE_SHIFT;
	flash_size = BIT((flash_size + MCP_BYTES_PER_MBIT_OFFSET));

	*p_flash_size = flash_size;

	return 0;
}

int qed_start_recovery_process(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_dev *cdev = p_hwfn->cdev;

	if (QED_RECOV_IN_PROG(cdev)) {
		DP_NOTICE(p_hwfn,
			  "Avoid triggering a recovery since such a process is already in progress\n");
		return -EAGAIN;
	}

	DP_NOTICE(p_hwfn, "Triggering a recovery process\n");
	qed_wr(p_hwfn, p_ptt, MISC_REG_AEU_GENERAL_ATTN_35, 0x1);

	/* mark all vf-pf channel as dead */
	qed_disable_channel_for_all_vfs(p_hwfn, p_ptt);

	return 0;
}

#define QED_RECOVERY_PROLOG_SLEEP_MS    100

int qed_recovery_prolog(struct qed_dev *cdev)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt = p_hwfn->p_main_ptt;
	int rc;

	/* When a specific PF encounter recovery flow (such as AER) which
	 * does not involve entire chip reset, other PFs get timer attention.
	 * It results into another error recovery flow. Stopping the timers
	 * early during recovery flow will prevent other ports getting
	 * timer attention.
	 */
	qed_hw_timers_stop_all(cdev);

	/* Allow ongoing PCIe transactions to complete */
	msleep(QED_RECOVERY_PROLOG_SLEEP_MS);

	/* Clear the PF's internal FID_enable in the PXP */
	rc = qed_pglueb_set_pfid_enable(p_hwfn, p_ptt, false);
	if (rc)
		DP_NOTICE(p_hwfn,
			  "qed_pglueb_set_pfid_enable() failed. rc = %d.\n",
			  rc);

	return rc;
}

static int
qed_mcp_config_vf_msix_bb(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, u8 vf_id, u8 num)
{
	u32 resp = 0, param = 0, rc_param = 0;
	int rc;

	/* Only Leader can configure MSIX, and need to take CMT into account */
	if (!IS_LEAD_HWFN(p_hwfn))
		return 0;
	num *= p_hwfn->cdev->num_hwfns;

	SET_MFW_FIELD(param, DRV_MB_PARAM_CFG_VF_MSIX_VF_ID, vf_id);
	SET_MFW_FIELD(param, DRV_MB_PARAM_CFG_VF_MSIX_SB_NUM, num);

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_CFG_VF_MSIX, param,
			 &resp, &rc_param);

	if (resp != (u32) FW_MSG_CODE_DRV_CFG_VF_MSIX_DONE) {
		DP_NOTICE(p_hwfn, "VF[%d]: MFW failed to set MSI-X\n", vf_id);
		rc = -EINVAL;
	} else {
		DP_VERBOSE(p_hwfn, QED_MSG_IOV,
			   "Requested 0x%02x MSI-x interrupts from VF 0x%02x\n",
			   num, vf_id);
	}

	return rc;
}

static int
qed_mcp_config_vf_msix_ah(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, u8 num)
{
	u32 resp = 0, param = num, rc_param = 0;
	int rc;

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_CFG_PF_VFS_MSIX,
			 param, &resp, &rc_param);

	if (resp != FW_MSG_CODE_DRV_CFG_PF_VFS_MSIX_DONE) {
		DP_NOTICE(p_hwfn, "MFW failed to set MSI-X for VFs\n");
		rc = -EINVAL;
	} else {
		DP_VERBOSE(p_hwfn, QED_MSG_IOV,
			   "Requested 0x%02x MSI-x interrupts for VFs\n", num);
	}

	return rc;
}

int qed_mcp_config_vf_msix(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, u8 vf_id, u8 num)
{
#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->cdev) && !qed_mcp_is_init(p_hwfn)) {
		DP_INFO(p_hwfn,
			"Emulation: Avoid sending the %s mailbox command\n",
			QED_IS_BB(p_hwfn->cdev) ? "CFG_VF_MSIX" :
			"CFG_PF_VFS_MSIX");
		return 0;
	}
#endif

	if (QED_IS_BB(p_hwfn->cdev))
		return qed_mcp_config_vf_msix_bb(p_hwfn, p_ptt, vf_id, num);
	else
		return qed_mcp_config_vf_msix_ah(p_hwfn, p_ptt, num);
}

int
qed_mcp_send_drv_version(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 struct qed_mcp_drv_version *p_ver)
{
	struct qed_mcp_mb_params mb_params;
	struct drv_version_stc drv_version;
	u32 num_words, i;
	void *p_name;
	__be32 val;
	int rc;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_SLOW(p_hwfn->cdev))
		return 0;
#endif

	memset(&drv_version, 0, sizeof(drv_version));
	drv_version.version = p_ver->version;
	num_words = (MCP_DRV_VER_STR_SIZE - 4) / 4;
	for (i = 0; i < num_words; i++) {
		/* The driver name is expected to be in a big-endian format */
		p_name = &p_ver->name[i * sizeof(u32)];
		val = cpu_to_be32(*(u32 *) p_name);
		*(u32 *) & drv_version.name[i * sizeof(u32)] = val;
	}

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_SET_VERSION;
	mb_params.p_data_src = &drv_version;
	mb_params.data_src_size = sizeof(drv_version);
	mb_params.flags = QED_MB_FLAG_CAN_SLEEP;
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		DP_ERR(p_hwfn, "MCP response failure, aborting\n");

	return rc;
}

/* A maximal 100 msec waiting time for the MCP to halt */
#define QED_MCP_HALT_SLEEP_MS           10
#define QED_MCP_HALT_MAX_RETRIES        10

int qed_mcp_halt(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 resp = 0, param = 0, cpu_state, cnt = 0;
	int rc;

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_MCP_HALT, 0, &resp,
			 &param);
	if (rc) {
		DP_ERR(p_hwfn, "MCP response failure, aborting\n");
		return rc;
	}

	do {
		msleep(QED_MCP_HALT_SLEEP_MS);
		cpu_state = qed_rd(p_hwfn, p_ptt, MCP_REG_CPU_STATE);
		if (cpu_state & MCP_REG_CPU_STATE_SOFT_HALTED)
			break;
	} while (++cnt < QED_MCP_HALT_MAX_RETRIES);

	if (cnt == QED_MCP_HALT_MAX_RETRIES) {
		DP_NOTICE(p_hwfn,
			  "Failed to halt the MCP [CPU_MODE = 0x%08x, CPU_STATE = 0x%08x]\n",
			  qed_rd(p_hwfn, p_ptt, MCP_REG_CPU_MODE), cpu_state);
		return -EBUSY;
	}

	qed_mcp_cmd_set_halt(p_hwfn, true);

	return 0;
}

#define QED_MCP_RESUME_SLEEP_MS 10

int qed_mcp_resume(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 cpu_mode, cpu_state;

	qed_wr(p_hwfn, p_ptt, MCP_REG_CPU_STATE, 0xffffffff);

	cpu_mode = qed_rd(p_hwfn, p_ptt, MCP_REG_CPU_MODE);
	cpu_mode &= ~MCP_REG_CPU_MODE_SOFT_HALT;
	qed_wr(p_hwfn, p_ptt, MCP_REG_CPU_MODE, cpu_mode);
	msleep(QED_MCP_RESUME_SLEEP_MS);
	cpu_state = qed_rd(p_hwfn, p_ptt, MCP_REG_CPU_STATE);

	if (cpu_state & MCP_REG_CPU_STATE_SOFT_HALTED) {
		DP_NOTICE(p_hwfn,
			  "Failed to resume the MCP [CPU_MODE = 0x%08x, CPU_STATE = 0x%08x]\n",
			  cpu_mode, cpu_state);
		return -EBUSY;
	}

	qed_mcp_cmd_set_halt(p_hwfn, false);

	return 0;
}

int
qed_mcp_ov_update_current_config(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt,
				 enum qed_ov_client client)
{
	u32 resp = 0, param = 0;
	u32 drv_mb_param;
	int rc;

	switch (client) {
	case QED_OV_CLIENT_DRV:
		drv_mb_param = DRV_MB_PARAM_OV_CURR_CFG_OS;
		break;
	case QED_OV_CLIENT_USER:
		drv_mb_param = DRV_MB_PARAM_OV_CURR_CFG_OTHER;
		break;
	case QED_OV_CLIENT_VENDOR_SPEC:
		drv_mb_param = DRV_MB_PARAM_OV_CURR_CFG_VENDOR_SPEC;
		break;
	default:
		DP_NOTICE(p_hwfn, "Invalid client type %d\n", client);
		return -EINVAL;
	}

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_OV_UPDATE_CURR_CFG,
			 drv_mb_param, &resp, &param);
	if (rc)
		DP_ERR(p_hwfn, "MCP response failure, aborting\n");

	return rc;
}

int
qed_mcp_ov_update_driver_state(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt,
			       enum qed_ov_driver_state drv_state)
{
	u32 resp = 0, param = 0;
	u32 drv_mb_param;
	int rc;

	switch (drv_state) {
	case QED_OV_DRIVER_STATE_NOT_LOADED:
		drv_mb_param = DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_NOT_LOADED;
		break;
	case QED_OV_DRIVER_STATE_DISABLED:
		drv_mb_param = DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_DISABLED;
		break;
	case QED_OV_DRIVER_STATE_ACTIVE:
		drv_mb_param = DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_ACTIVE;
		break;
	default:
		DP_NOTICE(p_hwfn, "Invalid driver state %d\n", drv_state);
		return -EINVAL;
	}

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE,
			 drv_mb_param, &resp, &param);
	if (rc)
		DP_ERR(p_hwfn, "Failed to send driver state\n");

	return rc;
}

int
qed_mcp_ov_get_fc_npiv(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt, struct qed_fc_npiv_tbl *p_table)
{
	struct dci_fc_npiv_tbl *p_npiv_table;
	u8 *p_buf = NULL;
	u32 addr, size, i;
	int rc = 0;
	struct dci_fc_npiv_cfg npiv_cfg;

	p_table->num_wwpn = 0;
	p_table->num_wwnn = 0;
	addr = qed_rd(p_hwfn,
		      p_ptt,
		      p_hwfn->mcp_info->port_addr +
		      offsetof(struct public_port, fc_npiv_nvram_tbl_addr));
	if (addr == NPIV_TBL_INVALID_ADDR) {
		DP_VERBOSE(p_hwfn, QED_MSG_SP, "NPIV table doesn't exist\n");
		return -EINVAL;
	}

	/* First read table header. Then use number of entries field to
	 * know actual number of npiv entries.
	 */
	size = sizeof(struct dci_fc_npiv_cfg);
	memset(&npiv_cfg, 0, size);

	rc = qed_mcp_nvm_read(p_hwfn->cdev, addr, (u8 *) & npiv_cfg, size);
	if (rc) {
		DP_ERR(p_hwfn, "NPIV table configuration read failed.\n");
		return rc;
	}

	if (npiv_cfg.num_of_npiv < 1) {
		DP_ERR(p_hwfn, "No entries in NPIV table\n");
		return -EINVAL;
	}

	size += sizeof(struct dci_npiv_settings) * npiv_cfg.num_of_npiv;

	p_buf = vzalloc(size);
	if (!p_buf) {
		DP_ERR(p_hwfn, "Buffer allocation failed\n");
		return -ENOMEM;
	}

	rc = qed_mcp_nvm_read(p_hwfn->cdev, addr, p_buf, size);
	if (rc) {
		vfree(p_buf);
		return rc;
	}

	p_npiv_table = (struct dci_fc_npiv_tbl *)p_buf;
	p_table->num_wwpn = (u16) p_npiv_table->fc_npiv_cfg.num_of_npiv;
	p_table->num_wwnn = (u16) p_npiv_table->fc_npiv_cfg.num_of_npiv;
	for (i = 0; i < p_npiv_table->fc_npiv_cfg.num_of_npiv; i++) {
		p_table->wwpn[i][0] = p_npiv_table->settings[i].npiv_wwpn[3];
		p_table->wwpn[i][1] = p_npiv_table->settings[i].npiv_wwpn[2];
		p_table->wwpn[i][2] = p_npiv_table->settings[i].npiv_wwpn[1];
		p_table->wwpn[i][3] = p_npiv_table->settings[i].npiv_wwpn[0];
		p_table->wwpn[i][4] = p_npiv_table->settings[i].npiv_wwpn[7];
		p_table->wwpn[i][5] = p_npiv_table->settings[i].npiv_wwpn[6];
		p_table->wwpn[i][6] = p_npiv_table->settings[i].npiv_wwpn[5];
		p_table->wwpn[i][7] = p_npiv_table->settings[i].npiv_wwpn[4];

		p_table->wwnn[i][0] = p_npiv_table->settings[i].npiv_wwnn[3];
		p_table->wwnn[i][1] = p_npiv_table->settings[i].npiv_wwnn[2];
		p_table->wwnn[i][2] = p_npiv_table->settings[i].npiv_wwnn[1];
		p_table->wwnn[i][3] = p_npiv_table->settings[i].npiv_wwnn[0];
		p_table->wwnn[i][4] = p_npiv_table->settings[i].npiv_wwnn[7];
		p_table->wwnn[i][5] = p_npiv_table->settings[i].npiv_wwnn[6];
		p_table->wwnn[i][6] = p_npiv_table->settings[i].npiv_wwnn[5];
		p_table->wwnn[i][7] = p_npiv_table->settings[i].npiv_wwnn[4];
	}

	vfree(p_buf);

	return 0;
}

int
qed_mcp_ov_update_mtu(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u16 mtu)
{
	u32 resp = 0, param = 0, drv_mb_param = 0;
	int rc;

	SET_MFW_FIELD(drv_mb_param, DRV_MB_PARAM_OV_MTU_SIZE, (u32) mtu);
	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_OV_UPDATE_MTU,
			 drv_mb_param, &resp, &param);
	if (rc)
		DP_ERR(p_hwfn, "Failed to send mtu value, rc = %d\n", rc);

	return rc;
}

int
qed_mcp_ov_update_mac(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, const u8 * mac)
{
	struct qed_mcp_mb_params mb_params;
	u32 mfw_mac[2];
	int rc;

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_SET_VMAC;
	SET_MFW_FIELD(mb_params.param, DRV_MSG_CODE_VMAC_TYPE,
		      DRV_MSG_CODE_VMAC_TYPE_MAC);
	mb_params.param |= (u32) p_hwfn->abs_pf_id;

	/* MCP is BE, and on LE platforms PCI would swap access to SHMEM
	 * in 32-bit granularity.
	 * So the MAC has to be set in native order [and not byte order],
	 * otherwise it would be read incorrectly by MFW after swap.
	 */
	mfw_mac[0] = mac[0] << 24 | mac[1] << 16 | mac[2] << 8 | mac[3];
	mfw_mac[1] = mac[4] << 24 | mac[5] << 16;

	mb_params.p_data_src = (u8 *) mfw_mac;
	mb_params.data_src_size = 8;
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		DP_ERR(p_hwfn, "Failed to send mac address, rc = %d\n", rc);

	/* Store primary MAC for later possible WoL */
	memcpy(p_hwfn->cdev->wol_mac, mac, ETH_ALEN);

	return rc;
}

int
qed_mcp_ov_update_wol(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, enum qed_ov_wol wol)
{
	u32 resp = 0, param = 0;
	u32 drv_mb_param;
	int rc;

	if (p_hwfn->hw_info.b_wol_support == QED_WOL_SUPPORT_NONE) {
		DP_VERBOSE(p_hwfn,
			   QED_MSG_SP,
			   "Can't change WoL configuration when WoL isn't supported\n");
		return -EINVAL;
	}

	switch (wol) {
	case QED_OV_WOL_DEFAULT:
		drv_mb_param = DRV_MB_PARAM_WOL_DEFAULT;
		break;
	case QED_OV_WOL_DISABLED:
		drv_mb_param = DRV_MB_PARAM_WOL_DISABLED;
		break;
	case QED_OV_WOL_ENABLED:
		drv_mb_param = DRV_MB_PARAM_WOL_ENABLED;
		break;
	default:
		DP_ERR(p_hwfn, "Invalid wol state %d\n", wol);
		return -EINVAL;
	}

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_OV_UPDATE_WOL,
			 drv_mb_param, &resp, &param);
	if (rc)
		DP_ERR(p_hwfn, "Failed to send wol mode, rc = %d\n", rc);

	/* Store the WoL update for a future unload */
	p_hwfn->cdev->wol_config = (u8) wol;

	return rc;
}

int
qed_mcp_ov_update_eswitch(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, enum qed_ov_eswitch eswitch)
{
	u32 resp = 0, param = 0;
	u32 drv_mb_param;
	int rc = 0;

	switch (eswitch) {
	case QED_OV_ESWITCH_NONE:
		drv_mb_param = DRV_MB_PARAM_ESWITCH_MODE_NONE;
		break;
	case QED_OV_ESWITCH_VEB:
		drv_mb_param = DRV_MB_PARAM_ESWITCH_MODE_VEB;
		break;
	case QED_OV_ESWITCH_VEPA:
		drv_mb_param = DRV_MB_PARAM_ESWITCH_MODE_VEPA;
		break;
	default:
		DP_ERR(p_hwfn, "Invalid eswitch mode %d\n", eswitch);
		return -EINVAL;
	}

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_OV_UPDATE_ESWITCH_MODE,
			 drv_mb_param, &resp, &param);
	if (rc)
		DP_ERR(p_hwfn, "Failed to send eswitch mode, rc = %d\n", rc);

	return rc;
}

int qed_mcp_set_led(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt, enum qed_led_mode mode)
{
	u32 resp = 0, param = 0, drv_mb_param;
	int rc;

	switch (mode) {
	case QED_LED_MODE_ON:
		drv_mb_param = DRV_MB_PARAM_SET_LED_MODE_ON;
		break;
	case QED_LED_MODE_OFF:
		drv_mb_param = DRV_MB_PARAM_SET_LED_MODE_OFF;
		break;
	case QED_LED_MODE_RESTORE:
		drv_mb_param = DRV_MB_PARAM_SET_LED_MODE_OPER;
		break;
	default:
		DP_NOTICE(p_hwfn, "Invalid LED mode %d\n", mode);
		return -EINVAL;
	}

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_SET_LED_MODE,
			 drv_mb_param, &resp, &param);
	if (rc)
		DP_ERR(p_hwfn, "MCP response failure, aborting\n");

	return rc;
}

int qed_mcp_mask_parities(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, u32 mask_parities)
{
	u32 resp = 0, param = 0;
	int rc;

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_MASK_PARITIES,
			 mask_parities, &resp, &param);

	if (rc) {
		DP_ERR(p_hwfn,
		       "MCP response failure for mask parities, aborting\n");
	} else if (resp != FW_MSG_CODE_OK) {
		DP_ERR(p_hwfn,
		       "MCP did not acknowledge mask parity request. Old MFW?\n");
		rc = -EINVAL;
	}

	return rc;
}

int qed_mcp_nvm_read(struct qed_dev *cdev, u32 addr, u8 * p_buf, u32 len)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	u32 bytes_left, offset, bytes_to_copy, buf_size;
	u32 nvm_offset = 0, resp = 0, param;
	struct qed_ptt *p_ptt;
	int rc = 0;

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return -EBUSY;

	bytes_left = len;
	offset = 0;
	while (bytes_left > 0) {
		bytes_to_copy = min_t(u32, bytes_left, MCP_DRV_NVM_BUF_LEN);
		SET_MFW_FIELD(nvm_offset, DRV_MB_PARAM_NVM_OFFSET,
			      addr + offset);
		SET_MFW_FIELD(nvm_offset, DRV_MB_PARAM_NVM_LEN, bytes_to_copy);
		rc = qed_mcp_nvm_rd_cmd(p_hwfn, p_ptt,
					DRV_MSG_CODE_NVM_READ_NVRAM,
					nvm_offset, &resp, &param, &buf_size,
					(u32 *) (p_buf + offset), true);
		if (rc) {
			DP_NOTICE(cdev,
				  "qed_mcp_nvm_rd_cmd() failed, rc = %d\n", rc);
			resp = FW_MSG_CODE_ERROR;
			break;
		}

		if (resp != FW_MSG_CODE_NVM_OK) {
			DP_NOTICE(cdev,
				  "nvm read failed, resp = 0x%08x\n", resp);
			rc = -EINVAL;
			break;
		}

		/* This can be a lengthy process, and it's possible scheduler
		 * isn't preemptable. Sleep a bit to prevent CPU hogging.
		 */
		if (bytes_left % FLASH_PAGE_SIZE <
		    (bytes_left - buf_size) % FLASH_PAGE_SIZE)
			usleep_range(1000, 2000);

		offset += buf_size;
		bytes_left -= buf_size;
	}

	cdev->mcp_nvm_resp = resp;
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

int qed_mcp_phy_read(struct qed_dev *cdev,
		     u32 cmd, u32 addr, u8 * p_buf, u32 * p_len)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt;
	u32 resp = 0, param;
	int rc;

	if (QED_RECOV_IN_PROG(p_hwfn->cdev)) {
		DP_ERR(p_hwfn, "Error recovery in progress\n");
		return -EAGAIN;
	}

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return -EBUSY;

	rc = qed_mcp_nvm_rd_cmd(p_hwfn, p_ptt,
				(cmd == QED_PHY_CORE_READ) ?
				DRV_MSG_CODE_PHY_CORE_READ :
				DRV_MSG_CODE_PHY_RAW_READ, addr, &resp,
				&param, p_len, (u32 *) p_buf, true);
	if (rc)
		DP_NOTICE(cdev, "MCP command rc = %d\n", rc);

	cdev->mcp_nvm_resp = resp;
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

int qed_mcp_nvm_del_file(struct qed_dev *cdev, u32 addr)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt;
	u32 resp = 0, param;
	int rc;

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return -EBUSY;
	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_NVM_DEL_FILE, addr,
			 &resp, &param);
	cdev->mcp_nvm_resp = resp;
	qed_ptt_release(p_hwfn, p_ptt);

	/* nvm_info needs to be updated */
	p_hwfn->nvm_info.valid = false;

	return rc;
}

/* rc recieves -EINVAL as default parameter because
 * it might not enter the while loop if the len is 0
 */
int qed_mcp_nvm_write(struct qed_dev *cdev,
		      u32 cmd, u32 addr, u8 * p_buf, u32 len)
{
	u32 buf_idx, buf_size, nvm_cmd, nvm_offset, resp = 0, param;
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	int rc = -EINVAL;
	struct qed_ptt *p_ptt;

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return -EBUSY;

	switch (cmd) {
	case QED_PUT_FILE_BEGIN:
		nvm_cmd = DRV_MSG_CODE_NVM_PUT_FILE_BEGIN;
		break;
	case QED_PUT_FILE_DATA:
		nvm_cmd = DRV_MSG_CODE_NVM_PUT_FILE_DATA;
		break;
	case QED_NVM_WRITE_NVRAM:
		nvm_cmd = DRV_MSG_CODE_NVM_WRITE_NVRAM;
		break;
	case QED_EXT_PHY_FW_UPGRADE:
		nvm_cmd = DRV_MSG_CODE_EXT_PHY_FW_UPGRADE;
		break;
	case QED_ENCRYPT_PASSWORD:
		nvm_cmd = DRV_MSG_CODE_ENCRYPT_PASSWORD;
		break;
	default:
		DP_NOTICE(p_hwfn, "Invalid nvm write command 0x%x\n", cmd);
		rc = -EINVAL;
		goto out;
	}

	buf_idx = 0;
	buf_size = min_t(u32, (len - buf_idx), MCP_DRV_NVM_BUF_LEN);
	while (buf_idx < len) {
		if (cmd == QED_PUT_FILE_BEGIN)
			nvm_offset = addr;
		else
			nvm_offset =
			    ((buf_size << DRV_MB_PARAM_NVM_LEN_OFFSET) |
			     addr) + buf_idx;
		rc = qed_mcp_nvm_wr_cmd(p_hwfn, p_ptt, nvm_cmd, nvm_offset,
					&resp, &param, buf_size,
					(u32 *) & p_buf[buf_idx], true);
		if (rc) {
			DP_NOTICE(cdev,
				  "qed_mcp_nvm_write() failed, rc = %d\n", rc);
			resp = FW_MSG_CODE_ERROR;
			break;
		}

		if (resp != FW_MSG_CODE_OK &&
		    resp != FW_MSG_CODE_NVM_OK &&
		    resp != FW_MSG_CODE_NVM_PUT_FILE_FINISH_OK) {
			DP_NOTICE(cdev,
				  "nvm write failed, resp = 0x%08x\n", resp);
			rc = -EINVAL;
			break;
		}

		/* This can be a lengthy process, and it's possible scheduler
		 * isn't preemptable. Sleep a bit to prevent CPU hogging.
		 */
		if (buf_idx % 0x1000 > (buf_idx + buf_size) % 0x1000)
			usleep_range(1000, 2000);

		/* For MBI upgrade, MFW response includes the next buffer offset
		 * to be delivered to MFW.
		 */
		if (param && cmd == QED_PUT_FILE_DATA) {
			buf_idx = GET_MFW_FIELD(param,
						FW_MB_PARAM_NVM_PUT_FILE_REQ_OFFSET);
			buf_size = GET_MFW_FIELD(param,
						 FW_MB_PARAM_NVM_PUT_FILE_REQ_SIZE);
		} else {
			buf_idx += buf_size;
			buf_size = min_t(u32, (len - buf_idx),
					 MCP_DRV_NVM_BUF_LEN);
		}
	}

	cdev->mcp_nvm_resp = resp;
out:
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

int qed_mcp_phy_write(struct qed_dev *cdev,
		      u32 cmd, u32 addr, u8 * p_buf, u32 len)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt;
	u32 resp = 0, param, nvm_cmd;
	int rc;

	if (QED_RECOV_IN_PROG(p_hwfn->cdev)) {
		DP_ERR(p_hwfn, "Error recovery in progress\n");
		return -EAGAIN;
	}

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return -EBUSY;

	nvm_cmd = (cmd == QED_PHY_CORE_WRITE) ? DRV_MSG_CODE_PHY_CORE_WRITE :
	    DRV_MSG_CODE_PHY_RAW_WRITE;
	rc = qed_mcp_nvm_wr_cmd(p_hwfn, p_ptt, nvm_cmd, addr,
				&resp, &param, len, (u32 *) p_buf, true);
	if (rc)
		DP_NOTICE(cdev, "MCP command rc = %d\n", rc);
	cdev->mcp_nvm_resp = resp;
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

int qed_mcp_phy_sfp_read(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 u32 port, u32 addr, u32 offset, u32 len, u8 * p_buf)
{
	u32 bytes_left, bytes_to_copy, buf_size, nvm_offset = 0;
	u32 resp, param;
	int rc;

	if (QED_RECOV_IN_PROG(p_hwfn->cdev)) {
		DP_ERR(p_hwfn, "Error recovery in progress\n");
		return -EAGAIN;
	}

	SET_MFW_FIELD(nvm_offset, DRV_MB_PARAM_TRANSCEIVER_PORT, port);
	SET_MFW_FIELD(nvm_offset, DRV_MB_PARAM_TRANSCEIVER_I2C_ADDRESS, addr);
	addr = offset;
	offset = 0;
	bytes_left = len;
	while (bytes_left > 0) {
		bytes_to_copy = min_t(u32, bytes_left,
				      MAX_I2C_TRANSACTION_SIZE);
		nvm_offset &= (DRV_MB_PARAM_TRANSCEIVER_I2C_ADDRESS_MASK |
			       DRV_MB_PARAM_TRANSCEIVER_PORT_MASK);
		SET_MFW_FIELD(nvm_offset, DRV_MB_PARAM_TRANSCEIVER_OFFSET,
			      (addr + offset));
		SET_MFW_FIELD(nvm_offset, DRV_MB_PARAM_TRANSCEIVER_SIZE,
			      bytes_to_copy);
		rc = qed_mcp_nvm_rd_cmd(p_hwfn, p_ptt,
					DRV_MSG_CODE_TRANSCEIVER_READ,
					nvm_offset, &resp, &param, &buf_size,
					(u32 *) (p_buf + offset), true);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "Failed to send a transceiver read command to the MFW. rc = %d.\n",
				  rc);
			return rc;
		}

		if (resp == FW_MSG_CODE_TRANSCEIVER_NOT_PRESENT)
			return -ENODEV;
		else if (resp != FW_MSG_CODE_TRANSCEIVER_DIAG_OK)
			return -EINVAL;

		offset += buf_size;
		bytes_left -= buf_size;

		/* The MFW reads from the SFP using the slow I2C bus, so a short
		 * sleep is needed to prevent CPU hogging.
		 */
		usleep_range(1000, 2000);
	}

	return 0;
}

int qed_mcp_phy_sfp_write(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt,
			  u32 port, u32 addr, u32 offset, u32 len, u8 * p_buf)
{
	u32 buf_idx, buf_size, nvm_offset = 0, resp, param;
	int rc;

	if (QED_RECOV_IN_PROG(p_hwfn->cdev)) {
		DP_ERR(p_hwfn, "Error recovery in progress\n");
		return -EAGAIN;
	}

	SET_MFW_FIELD(nvm_offset, DRV_MB_PARAM_TRANSCEIVER_PORT, port);
	SET_MFW_FIELD(nvm_offset, DRV_MB_PARAM_TRANSCEIVER_I2C_ADDRESS, addr);
	buf_idx = 0;
	while (buf_idx < len) {
		buf_size = min_t(u32, (len - buf_idx),
				 MAX_I2C_TRANSACTION_SIZE);
		nvm_offset &= (DRV_MB_PARAM_TRANSCEIVER_I2C_ADDRESS_MASK |
			       DRV_MB_PARAM_TRANSCEIVER_PORT_MASK);
		SET_MFW_FIELD(nvm_offset, DRV_MB_PARAM_TRANSCEIVER_OFFSET,
			      (offset + buf_idx));
		SET_MFW_FIELD(nvm_offset, DRV_MB_PARAM_TRANSCEIVER_SIZE,
			      buf_size);
		rc = qed_mcp_nvm_wr_cmd(p_hwfn, p_ptt,
					DRV_MSG_CODE_TRANSCEIVER_WRITE,
					nvm_offset, &resp, &param, buf_size,
					(u32 *) & p_buf[buf_idx], true);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "Failed to send a transceiver write command to the MFW. rc = %d.\n",
				  rc);
			return rc;
		}

		if (resp == FW_MSG_CODE_TRANSCEIVER_NOT_PRESENT)
			return -ENODEV;
		else if (resp != FW_MSG_CODE_TRANSCEIVER_DIAG_OK)
			return -EINVAL;

		buf_idx += buf_size;

		/* The MFW writes to the SFP using the slow I2C bus, so a short
		 * sleep is needed to prevent CPU hogging.
		 */
		usleep_range(1000, 2000);
	}

	return 0;
}

int qed_mcp_gpio_read(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, u16 gpio, u32 * gpio_val)
{
	int rc = 0;
	u32 drv_mb_param = 0, rsp;

	if (QED_RECOV_IN_PROG(p_hwfn->cdev)) {
		DP_ERR(p_hwfn, "Error recovery in progress\n");
		return -EAGAIN;
	}

	SET_MFW_FIELD(drv_mb_param, DRV_MB_PARAM_GPIO_NUMBER, gpio);

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_GPIO_READ,
			 drv_mb_param, &rsp, gpio_val);

	if (rc)
		return rc;

	if ((rsp & FW_MSG_CODE_MASK) != FW_MSG_CODE_GPIO_OK)
		return -EINVAL;

	return 0;
}

int qed_mcp_gpio_write(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt, u16 gpio, u16 gpio_val)
{
	int rc = 0;
	u32 drv_mb_param = 0, param, rsp;

	if (QED_RECOV_IN_PROG(p_hwfn->cdev)) {
		DP_ERR(p_hwfn, "Error recovery in progress\n");
		return -EAGAIN;
	}

	SET_MFW_FIELD(drv_mb_param, DRV_MB_PARAM_GPIO_NUMBER, gpio);
	SET_MFW_FIELD(drv_mb_param, DRV_MB_PARAM_GPIO_VALUE, gpio_val);

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_GPIO_WRITE,
			 drv_mb_param, &rsp, &param);

	if (rc)
		return rc;

	if ((rsp & FW_MSG_CODE_MASK) != FW_MSG_CODE_GPIO_OK)
		return -EINVAL;

	return 0;
}

int qed_mcp_gpio_info(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt,
		      u16 gpio, u32 * gpio_direction, u32 * gpio_ctrl)
{
	u32 drv_mb_param = 0, rsp, val = 0;
	int rc = 0;

	if (QED_RECOV_IN_PROG(p_hwfn->cdev)) {
		DP_ERR(p_hwfn, "Error recovery in progress\n");
		return -EAGAIN;
	}

	SET_MFW_FIELD(drv_mb_param, DRV_MB_PARAM_GPIO_NUMBER, gpio);

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_GPIO_INFO,
			 drv_mb_param, &rsp, &val);
	if (rc)
		return rc;

	*gpio_direction = GET_MFW_FIELD(val, DRV_MB_PARAM_GPIO_DIRECTION);
	*gpio_ctrl = GET_MFW_FIELD(val, DRV_MB_PARAM_GPIO_CTRL);

	if ((rsp & FW_MSG_CODE_MASK) != FW_MSG_CODE_GPIO_OK)
		return -EINVAL;

	return 0;
}

int qed_mcp_bist_register_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 drv_mb_param = 0, rsp, param;
	int rc = 0;

	SET_MFW_FIELD(drv_mb_param, DRV_MB_PARAM_BIST_TEST_INDEX,
		      DRV_MB_PARAM_BIST_REGISTER_TEST);

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_BIST_TEST,
			 drv_mb_param, &rsp, &param);

	if (rc)
		return rc;

	if (((rsp & FW_MSG_CODE_MASK) != FW_MSG_CODE_OK) ||
	    (param != DRV_MB_PARAM_BIST_RC_PASSED))
		rc = -EINVAL;

	return rc;
}

int qed_mcp_bist_clock_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 drv_mb_param = 0, rsp, param;
	int rc = 0;

	SET_MFW_FIELD(drv_mb_param, DRV_MB_PARAM_BIST_TEST_INDEX,
		      DRV_MB_PARAM_BIST_CLOCK_TEST);

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_BIST_TEST,
			 drv_mb_param, &rsp, &param);

	if (rc)
		return rc;

	if (((rsp & FW_MSG_CODE_MASK) != FW_MSG_CODE_OK) ||
	    (param != DRV_MB_PARAM_BIST_RC_PASSED))
		rc = -EINVAL;

	return rc;
}

int qed_mcp_bist_nvm_get_num_images(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, u32 * num_images)
{
	u32 drv_mb_param = 0, rsp;
	int rc = 0;

	SET_MFW_FIELD(drv_mb_param, DRV_MB_PARAM_BIST_TEST_INDEX,
		      DRV_MB_PARAM_BIST_NVM_TEST_NUM_IMAGES);

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_BIST_TEST,
			 drv_mb_param, &rsp, num_images);
	if (rc)
		return rc;

	if (rsp == FW_MSG_CODE_UNSUPPORTED)
		rc = -EOPNOTSUPP;
	else if (rsp != FW_MSG_CODE_OK)
		rc = -EINVAL;

	return rc;
}

int qed_mcp_bist_nvm_get_image_att(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt,
				   struct bist_nvm_image_att *p_image_att,
				   u32 image_index)
{
	u32 buf_size, nvm_offset = 0, resp, param;
	int rc;

	SET_MFW_FIELD(nvm_offset, DRV_MB_PARAM_BIST_TEST_INDEX,
		      DRV_MB_PARAM_BIST_NVM_TEST_IMAGE_BY_INDEX);
	SET_MFW_FIELD(nvm_offset, DRV_MB_PARAM_BIST_TEST_IMAGE_INDEX,
		      image_index);
	rc = qed_mcp_nvm_rd_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_BIST_TEST,
				nvm_offset, &resp, &param, &buf_size,
				(u32 *) p_image_att, true);
	if (rc)
		return rc;

	if (resp == FW_MSG_CODE_UNSUPPORTED)
		rc = -EOPNOTSUPP;
	else if ((resp != FW_MSG_CODE_OK) || (p_image_att->return_code != 1))
		rc = -EINVAL;

	return rc;
}

int qed_mcp_nvm_info_populate(struct qed_hwfn *p_hwfn)
{
	struct qed_nvm_image_info nvm_info;
	struct qed_ptt *p_ptt;
	int rc;
	u32 i;

	if (!p_hwfn || !p_hwfn->cdev || !p_hwfn->mcp_info)
		return -ENODEV;

	if (p_hwfn->nvm_info.valid)
		return 0;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->cdev) ||
	    CHIP_REV_IS_TEDIBEAR(p_hwfn->cdev) ||
	    p_hwfn->mcp_info->recovery_mode)
		return 0;
#endif

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt) {
		DP_ERR(p_hwfn, "failed to acquire ptt\n");
		return -EBUSY;
	}

	/* Acquire from MFW the amount of available images */
	memset(&nvm_info, 0, sizeof(nvm_info));
	rc = qed_mcp_bist_nvm_get_num_images(p_hwfn, p_ptt,
					     &nvm_info.num_images);
	if (rc == -EOPNOTSUPP) {
		DP_INFO(p_hwfn, "DRV_MSG_CODE_BIST_TEST is not supported\n");
		goto out;
	} else if ((rc != 0) || (nvm_info.num_images == 0)) {
		DP_ERR(p_hwfn, "Failed getting number of images\n");
		goto err0;
	}

	nvm_info.image_att =
	    kmalloc(nvm_info.num_images * sizeof(struct bist_nvm_image_att),
		    GFP_KERNEL);
	if (!nvm_info.image_att) {
		rc = -ENOMEM;
		goto err0;
	}

	/* Iterate over images and get their attributes */
	for (i = 0; i < nvm_info.num_images; i++) {
		rc = qed_mcp_bist_nvm_get_image_att(p_hwfn, p_ptt,
						    &nvm_info.image_att[i], i);
		if (rc) {
			DP_ERR(p_hwfn,
			       "Failed getting image index %d attributes\n", i);
			goto err1;
		}

		DP_VERBOSE(p_hwfn, QED_MSG_EXTRA, "image index %d, size %x\n",
			   i, nvm_info.image_att[i].len);
	}
out:
	/* Update hwfn's nvm_info */
	if (nvm_info.num_images) {
		p_hwfn->nvm_info.num_images = nvm_info.num_images;
		if (p_hwfn->nvm_info.image_att)
			kfree(p_hwfn->nvm_info.image_att);
		p_hwfn->nvm_info.image_att = nvm_info.image_att;
		p_hwfn->nvm_info.valid = true;
	}

	qed_ptt_release(p_hwfn, p_ptt);
	return 0;

err1:
	kfree(nvm_info.image_att);
err0:
	qed_ptt_release(p_hwfn, p_ptt);
	return rc;
}

void qed_mcp_nvm_info_free(struct qed_hwfn *p_hwfn)
{
#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->cdev) ||
	    CHIP_REV_IS_TEDIBEAR(p_hwfn->cdev))
		return;
#endif
	kfree(p_hwfn->nvm_info.image_att);
	p_hwfn->nvm_info.image_att = NULL;
	p_hwfn->nvm_info.valid = false;
}

int
qed_mcp_get_nvm_image_att(struct qed_hwfn *p_hwfn,
			  enum qed_nvm_images image_id,
			  struct qed_nvm_image_att *p_image_att)
{
	enum nvm_image_type type;
	int rc;
	u32 i;

	/* Translate image_id into MFW definitions */
	switch (image_id) {
	case QED_NVM_IMAGE_ISCSI_CFG:
		type = NVM_TYPE_ISCSI_CFG;
		break;
	case QED_NVM_IMAGE_FCOE_CFG:
		type = NVM_TYPE_FCOE_CFG;
		break;
	case QED_NVM_IMAGE_MDUMP:
		type = NVM_TYPE_MDUMP;
		break;
	case QED_NVM_IMAGE_NVM_CFG1:
		type = NVM_TYPE_NVM_CFG1;
		break;
	case QED_NVM_IMAGE_DEFAULT_CFG:
		type = NVM_TYPE_DEFAULT_CFG;
		break;
	case QED_NVM_IMAGE_NVM_META:
		type = NVM_TYPE_NVM_META;
		break;
	default:
		DP_NOTICE(p_hwfn, "Unknown request of image_id %08x\n",
			  image_id);
		return -EINVAL;
	}

	rc = qed_mcp_nvm_info_populate(p_hwfn);
	if (rc)
		return rc;

	for (i = 0; i < p_hwfn->nvm_info.num_images; i++)
		if (type == p_hwfn->nvm_info.image_att[i].image_type)
			break;
	if (i == p_hwfn->nvm_info.num_images) {
		DP_VERBOSE(p_hwfn, QED_MSG_STORAGE,
			   "Failed to find nvram image of type %08x\n",
			   image_id);
		return -ENOENT;
	}

	p_image_att->start_addr = p_hwfn->nvm_info.image_att[i].nvm_start_addr;
	p_image_att->length = p_hwfn->nvm_info.image_att[i].len;

	return 0;
}

int qed_mcp_get_nvm_image(struct qed_hwfn *p_hwfn,
			  enum qed_nvm_images image_id,
			  u8 * p_buffer, u32 buffer_len)
{
	struct qed_nvm_image_att image_att;
	int rc;

	memset(p_buffer, 0, buffer_len);

	rc = qed_mcp_get_nvm_image_att(p_hwfn, image_id, &image_att);
	if (rc)
		return rc;

	/* Validate sizes - both the image's and the supplied buffer's */
	if (image_att.length <= 4) {
		DP_VERBOSE(p_hwfn, QED_MSG_STORAGE,
			   "Image [%d] is too small - only %d bytes\n",
			   image_id, image_att.length);
		return -EINVAL;
	}

	if (image_att.length > buffer_len) {
		DP_VERBOSE(p_hwfn,
			   QED_MSG_STORAGE,
			   "Image [%d] is too big - %08x bytes where only %08x are available\n",
			   image_id, image_att.length, buffer_len);
		return -ENOMEM;
	}

	return qed_mcp_nvm_read(p_hwfn->cdev, image_att.start_addr,
				p_buffer, image_att.length);
}

int
qed_mcp_get_temperature_info(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt,
			     struct qed_temperature_info *p_temp_info)
{
	struct qed_temperature_sensor *p_temp_sensor;
	struct temperature_status_stc mfw_temp_info;
	struct qed_mcp_mb_params mb_params;
	u32 val;
	int rc;
	u32 i;

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_GET_TEMPERATURE;
	mb_params.p_data_dst = &mfw_temp_info;
	mb_params.data_dst_size = sizeof(mfw_temp_info);
	mb_params.flags = QED_MB_FLAG_CAN_SLEEP;
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		return rc;

	BUILD_BUG_ON(QED_MAX_NUM_OF_SENSORS != MAX_NUM_OF_SENSORS);
	p_temp_info->num_sensors = min_t(u32, mfw_temp_info.num_of_sensors,
					 QED_MAX_NUM_OF_SENSORS);
	for (i = 0; i < p_temp_info->num_sensors; i++) {
		val = mfw_temp_info.sensor[i];
		p_temp_sensor = &p_temp_info->sensors[i];
		p_temp_sensor->sensor_location =
		    GET_MFW_FIELD(val, SENSOR_LOCATION);
		p_temp_sensor->threshold_high =
		    GET_MFW_FIELD(val, THRESHOLD_HIGH);
		p_temp_sensor->critical =
		    GET_MFW_FIELD(val, CRITICAL_TEMPERATURE);
		p_temp_sensor->current_temp = GET_MFW_FIELD(val, CURRENT_TEMP);
	}

	return 0;
}

int qed_mcp_get_mba_versions(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt,
			     struct qed_mba_vers *p_mba_vers)
{
	u32 buf_size, resp, param;
	int rc;

	rc = qed_mcp_nvm_rd_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_GET_MBA_VERSION,
				0, &resp, &param, &buf_size,
				&(p_mba_vers->mba_vers[0]), true);

	if (rc)
		return rc;

	if ((resp & FW_MSG_CODE_MASK) != FW_MSG_CODE_NVM_OK)
		rc = -EINVAL;

	if (buf_size != MCP_DRV_NVM_BUF_LEN)
		rc = -EINVAL;

	return rc;
}

int qed_mcp_mem_ecc_events(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, u64 * num_events)
{
	struct qed_mcp_mb_params mb_params;
	struct mcp_val64 val64;
	int rc;

	memset(&mb_params, 0, sizeof(struct qed_mcp_mb_params));
	mb_params.cmd = DRV_MSG_CODE_MEM_ECC_EVENTS;
	mb_params.p_data_dst = &val64;
	mb_params.data_dst_size = sizeof(val64);
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		return rc;

	*num_events = ((u64) val64.hi << 32) | val64.lo;

	DP_VERBOSE(p_hwfn, QED_MSG_SP,
		   "Number of memory ECC events: %lld\n", *num_events);

	return 0;
}

static enum resource_id_enum qed_mcp_get_mfw_res_id(enum qed_resources res_id)
{
	enum resource_id_enum mfw_res_id = RESOURCE_NUM_INVALID;

	switch (res_id) {
	case QED_SB:
		mfw_res_id = RESOURCE_NUM_SB_E;
		break;
	case QED_L2_QUEUE:
		mfw_res_id = RESOURCE_NUM_L2_QUEUE_E;
		break;
	case QED_VPORT:
		mfw_res_id = RESOURCE_NUM_VPORT_E;
		break;
	case QED_RSS_ENG:
		mfw_res_id = RESOURCE_NUM_RSS_ENGINES_E;
		break;
	case QED_PQ:
		mfw_res_id = RESOURCE_NUM_PQ_E;
		break;
	case QED_RL:
		mfw_res_id = RESOURCE_NUM_RL_E;
		break;
	case QED_MAC:
	case QED_VLAN:
		/* Each VFC resource can accommodate both a MAC and a VLAN */
		mfw_res_id = RESOURCE_VFC_FILTER_E;
		break;
	case QED_ILT:
		mfw_res_id = RESOURCE_ILT_E;
		break;
	case QED_LL2_RAM_QUEUE:
		mfw_res_id = RESOURCE_LL2_QUEUE_E;
		break;
	case QED_LL2_CTX_QUEUE:
		mfw_res_id = RESOURCE_LL2_CQS_E;
		break;
	case QED_VF_RDMA_CNQ_RAM:
		mfw_res_id = RESOURCE_VF_CNQS;
		break;
	case QED_RDMA_CNQ_RAM:
	case QED_CMDQS_CQS:
		/* CNQ/CMDQS are the same resource */
		mfw_res_id = RESOURCE_CQS_E;
		break;
	case QED_RDMA_STATS_QUEUE:
		mfw_res_id = RESOURCE_RDMA_STATS_QUEUE_E;
		break;
	case QED_BDQ:
		mfw_res_id = RESOURCE_BDQ_E;
		break;
	case QED_VF_MAC_ADDR:
		mfw_res_id = RESOURCE_VF_MAC_ADDR;
		break;
	default:
		break;
	}

	return mfw_res_id;
}

#define QED_RESC_ALLOC_VERSION_MAJOR    2
#define QED_RESC_ALLOC_VERSION_MINOR    0

struct qed_resc_alloc_in_params {
	u32 cmd;
	enum qed_resources res_id;
	u32 resc_max_val;
};

struct qed_resc_alloc_out_params {
	u32 mcp_resp;
	u32 mcp_param;
	u32 resc_num;
	u32 resc_start;
	u32 vf_resc_num;
	u32 vf_resc_start;
	u32 flags;
};

static int
qed_mcp_resc_allocation_msg(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt,
			    struct qed_resc_alloc_in_params *p_in_params,
			    struct qed_resc_alloc_out_params *p_out_params)
{
	struct qed_mcp_mb_params mb_params;
	struct resource_info mfw_resc_info;
	int rc;

	memset(&mfw_resc_info, 0, sizeof(mfw_resc_info));

	mfw_resc_info.res_id = qed_mcp_get_mfw_res_id(p_in_params->res_id);
	if (mfw_resc_info.res_id == RESOURCE_NUM_INVALID) {
		DP_ERR(p_hwfn,
		       "Failed to match resource %d [%s] with the MFW resources\n",
		       p_in_params->res_id,
		       qed_hw_get_resc_name(p_in_params->res_id));
		return -EINVAL;
	}

	switch (p_in_params->cmd) {
	case DRV_MSG_SET_RESOURCE_VALUE_MSG:
		mfw_resc_info.size = p_in_params->resc_max_val;
		COMPAT_FALLTHROUGH;
		/* fallthrough */
	case DRV_MSG_GET_RESOURCE_ALLOC_MSG:
		break;
	default:
		DP_ERR(p_hwfn, "Unexpected resource alloc command [0x%08x]\n",
		       p_in_params->cmd);
		return -EINVAL;
	}

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = p_in_params->cmd;
	SET_MFW_FIELD(mb_params.param,
		      DRV_MB_PARAM_RESOURCE_ALLOC_VERSION_MAJOR,
		      QED_RESC_ALLOC_VERSION_MAJOR);
	SET_MFW_FIELD(mb_params.param,
		      DRV_MB_PARAM_RESOURCE_ALLOC_VERSION_MINOR,
		      QED_RESC_ALLOC_VERSION_MINOR);
	mb_params.p_data_src = &mfw_resc_info;
	mb_params.data_src_size = sizeof(mfw_resc_info);
	mb_params.p_data_dst = mb_params.p_data_src;
	mb_params.data_dst_size = mb_params.data_src_size;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "Resource message request: cmd 0x%08x, res_id %d [%s], hsi_version %d.%d, val 0x%x\n",
		   p_in_params->cmd,
		   p_in_params->res_id,
		   qed_hw_get_resc_name(p_in_params->res_id),
		   GET_MFW_FIELD(mb_params.param,
				 DRV_MB_PARAM_RESOURCE_ALLOC_VERSION_MAJOR),
		   GET_MFW_FIELD(mb_params.param,
				 DRV_MB_PARAM_RESOURCE_ALLOC_VERSION_MINOR),
		   p_in_params->resc_max_val);

	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		return rc;

	p_out_params->mcp_resp = mb_params.mcp_resp;
	p_out_params->mcp_param = mb_params.mcp_param;
	p_out_params->resc_num = mfw_resc_info.size;
	p_out_params->resc_start = mfw_resc_info.offset;
	p_out_params->vf_resc_num = mfw_resc_info.vf_size;
	p_out_params->vf_resc_start = mfw_resc_info.vf_offset;
	p_out_params->flags = mfw_resc_info.flags;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "Resource message response: mfw_hsi_version %d.%d, num 0x%x, start 0x%x, vf_num 0x%x, vf_start 0x%x, flags 0x%08x\n",
		   GET_MFW_FIELD(p_out_params->mcp_param,
				 FW_MB_PARAM_RESOURCE_ALLOC_VERSION_MAJOR),
		   GET_MFW_FIELD(p_out_params->mcp_param,
				 FW_MB_PARAM_RESOURCE_ALLOC_VERSION_MINOR),
		   p_out_params->resc_num,
		   p_out_params->resc_start,
		   p_out_params->vf_resc_num,
		   p_out_params->vf_resc_start, p_out_params->flags);

	return 0;
}

int
qed_mcp_set_resc_max_val(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 enum qed_resources res_id,
			 u32 resc_max_val, u32 * p_mcp_resp)
{
	struct qed_resc_alloc_out_params out_params;
	struct qed_resc_alloc_in_params in_params;
	int rc;

	memset(&in_params, 0, sizeof(in_params));
	in_params.cmd = DRV_MSG_SET_RESOURCE_VALUE_MSG;
	in_params.res_id = res_id;
	in_params.resc_max_val = resc_max_val;
	memset(&out_params, 0, sizeof(out_params));
	rc = qed_mcp_resc_allocation_msg(p_hwfn, p_ptt, &in_params,
					 &out_params);
	if (rc)
		return rc;

	*p_mcp_resp = out_params.mcp_resp;

	return 0;
}

int
qed_mcp_get_resc_info(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt,
		      enum qed_resources res_id,
		      u32 * p_mcp_resp, u32 * p_resc_num, u32 * p_resc_start)
{
	struct qed_resc_alloc_out_params out_params;
	struct qed_resc_alloc_in_params in_params;
	int rc;

	memset(&in_params, 0, sizeof(in_params));
	in_params.cmd = DRV_MSG_GET_RESOURCE_ALLOC_MSG;
	in_params.res_id = res_id;
	memset(&out_params, 0, sizeof(out_params));
	rc = qed_mcp_resc_allocation_msg(p_hwfn, p_ptt, &in_params,
					 &out_params);
	if (rc)
		return rc;

	*p_mcp_resp = out_params.mcp_resp;

	if (*p_mcp_resp == FW_MSG_CODE_RESOURCE_ALLOC_OK) {
		*p_resc_num = out_params.resc_num;
		*p_resc_start = out_params.resc_start;
	}

	return 0;
}

int qed_mcp_initiate_pf_flr(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 mcp_resp, mcp_param;

	return qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_INITIATE_PF_FLR, 0,
			   &mcp_resp, &mcp_param);
}

static int
qed_mcp_initiate_vf_flr(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt, u32 * vfs_to_flr)
{
	u8 i, vf_bitmap_size, vf_bitmap_size_in_bytes;
	struct qed_mcp_mb_params mb_params;
	int rc;

	if (QED_IS_BB(p_hwfn->cdev) || QED_IS_AH(p_hwfn->cdev)) {
		vf_bitmap_size = VF_BITMAP_SIZE_IN_DWORDS;
		vf_bitmap_size_in_bytes = VF_BITMAP_SIZE_IN_BYTES;
	} else {
		vf_bitmap_size = EXT_VF_BITMAP_SIZE_IN_DWORDS;
		vf_bitmap_size_in_bytes = EXT_VF_BITMAP_SIZE_IN_BYTES;
	}

	for (i = 0; i < vf_bitmap_size; i++)
		DP_VERBOSE(p_hwfn, (QED_MSG_SP | QED_MSG_IOV),
			   "FLR-ing VFs [%08x,...,%08x] - %08x\n",
			   i * 32, (i + 1) * 32 - 1, vfs_to_flr[i]);

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_INITIATE_VF_FLR;
	mb_params.p_data_src = vfs_to_flr;
	mb_params.data_src_size = vf_bitmap_size_in_bytes;
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc) {
		DP_NOTICE(p_hwfn,
			  "Failed to send an initiate VF FLR request\n");
		return rc;
	}

	if (mb_params.mcp_resp == FW_MSG_CODE_UNSUPPORTED) {
		DP_INFO(p_hwfn,
			"The VF FLR command is unsupported by the MFW\n");
		rc = -EOPNOTSUPP;
	} else if (mb_params.mcp_resp != (u32) FW_MSG_CODE_INITIATE_VF_FLR_OK) {
		DP_NOTICE(p_hwfn,
			  "Failed to initiate VF FLR [resp 0x%08x]\n",
			  mb_params.mcp_resp);
		rc = -EINVAL;
	}

	return rc;
}

int qed_mcp_vf_flr(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u8 rel_vf_id)
{
	u32 vfs_to_flr[EXT_VF_BITMAP_SIZE_IN_DWORDS];
	struct qed_dev *cdev = p_hwfn->cdev;
	u8 abs_vf_id, index, bit;
	int rc;

	if (!IS_PF_SRIOV(p_hwfn)) {
		DP_ERR(p_hwfn,
		       "Failed to initiate VF FLR: SRIOV is disabled\n");
		return -EINVAL;
	}

	if (rel_vf_id >= cdev->p_iov_info->total_vfs) {
		DP_ERR(p_hwfn,
		       "Failed to initiate VF FLR: VF ID is too high [total_vfs %d]\n",
		       cdev->p_iov_info->total_vfs);
		return -EINVAL;
	}

	abs_vf_id = (u8) cdev->p_iov_info->first_vf_in_pf + rel_vf_id;
	index = abs_vf_id / 32;
	bit = abs_vf_id % 32;
	memset(vfs_to_flr, 0, EXT_VF_BITMAP_SIZE_IN_BYTES);
	vfs_to_flr[index] |= BIT(bit);

	rc = qed_mcp_initiate_vf_flr(p_hwfn, p_ptt, vfs_to_flr);
	return rc;
}

int qed_mcp_get_lldp_mac(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt, u8 lldp_mac_addr[ETH_ALEN])
{
	struct qed_mcp_mb_params mb_params;
	struct mcp_mac lldp_mac;
	int rc;

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_GET_LLDP_MAC;
	mb_params.p_data_dst = &lldp_mac;
	mb_params.data_dst_size = sizeof(lldp_mac);
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		return rc;

	if (mb_params.mcp_resp != FW_MSG_CODE_OK) {
		DP_NOTICE(p_hwfn,
			  "MFW lacks support for the GET_LLDP_MAC command [resp 0x%08x]\n",
			  mb_params.mcp_resp);
		return -EINVAL;
	}

	*(u16 *) lldp_mac_addr = be16_to_cpu(*(u16 *) & lldp_mac.mac_upper);
	*(u32 *) (lldp_mac_addr + sizeof(u16)) =
	    be32_to_cpu(lldp_mac.mac_lower);

	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "LLDP MAC address is %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
		   lldp_mac_addr[0],
		   lldp_mac_addr[1],
		   lldp_mac_addr[2],
		   lldp_mac_addr[3], lldp_mac_addr[4], lldp_mac_addr[5]);

	return 0;
}

int qed_mcp_set_lldp_mac(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt, u8 lldp_mac_addr[ETH_ALEN])
{
	struct qed_mcp_mb_params mb_params;
	struct mcp_mac lldp_mac;
	int rc;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "Configuring LLDP MAC address to %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
		   lldp_mac_addr[0],
		   lldp_mac_addr[1],
		   lldp_mac_addr[2],
		   lldp_mac_addr[3], lldp_mac_addr[4], lldp_mac_addr[5]);

	memset(&lldp_mac, 0, sizeof(lldp_mac));
	lldp_mac.mac_upper = cpu_to_be16(*(u16 *) lldp_mac_addr);
	lldp_mac.mac_lower =
	    cpu_to_be32(*(u32 *) (lldp_mac_addr + sizeof(u16)));

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_SET_LLDP_MAC;
	mb_params.p_data_src = &lldp_mac;
	mb_params.data_src_size = sizeof(lldp_mac);
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		return rc;

	if (mb_params.mcp_resp != FW_MSG_CODE_OK) {
		DP_NOTICE(p_hwfn,
			  "MFW lacks support for the SET_LLDP_MAC command [resp 0x%08x]\n",
			  mb_params.mcp_resp);
		return -EINVAL;
	}

	return 0;
}

static int qed_mcp_resource_cmd(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt,
				u32 param, u32 * p_mcp_resp, u32 * p_mcp_param)
{
	int rc;

	rc = qed_mcp_cmd_nosleep(p_hwfn, p_ptt, DRV_MSG_CODE_RESOURCE_CMD,
				 param, p_mcp_resp, p_mcp_param);
	if (rc)
		return rc;

	if (*p_mcp_resp == FW_MSG_CODE_UNSUPPORTED) {
		DP_INFO(p_hwfn,
			"The resource command is unsupported by the MFW\n");
		return -EOPNOTSUPP;
	}

	if (*p_mcp_param == RESOURCE_OPCODE_UNKNOWN_CMD) {
		u8 opcode = GET_MFW_FIELD(param, RESOURCE_CMD_REQ_OPCODE);

		DP_NOTICE(p_hwfn,
			  "The resource command is unknown to the MFW [param 0x%08x, opcode %d]\n",
			  param, opcode);
		return -EINVAL;
	}

	return rc;
}

static int
__qed_mcp_resc_lock(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    struct qed_resc_lock_params *p_params)
{
	u32 param = 0, mcp_resp, mcp_param;
	u8 opcode, timeout;
	int rc;

	switch (p_params->timeout) {
	case QED_MCP_RESC_LOCK_TO_DEFAULT:
		opcode = RESOURCE_OPCODE_REQ;
		timeout = 0;
		break;
	case QED_MCP_RESC_LOCK_TO_NONE:
		opcode = RESOURCE_OPCODE_REQ_WO_AGING;
		timeout = 0;
		break;
	default:
		opcode = RESOURCE_OPCODE_REQ_W_AGING;
		timeout = p_params->timeout;
		break;
	}

	SET_MFW_FIELD(param, RESOURCE_CMD_REQ_RESC, p_params->resource);
	SET_MFW_FIELD(param, RESOURCE_CMD_REQ_OPCODE, opcode);
	SET_MFW_FIELD(param, RESOURCE_CMD_REQ_AGE, timeout);

	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "Resource lock request: param 0x%08x [age %d, opcode %d, resource %d]\n",
		   param, timeout, opcode, p_params->resource);

	/* Attempt to acquire the resource */
	rc = qed_mcp_resource_cmd(p_hwfn, p_ptt, param, &mcp_resp, &mcp_param);
	if (rc)
		return rc;

	/* Analyze the response */
	p_params->owner = GET_MFW_FIELD(mcp_param, RESOURCE_CMD_RSP_OWNER);
	opcode = GET_MFW_FIELD(mcp_param, RESOURCE_CMD_RSP_OPCODE);

	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "Resource lock response: mcp_param 0x%08x [opcode %d, owner %d]\n",
		   mcp_param, opcode, p_params->owner);

	switch (opcode) {
	case RESOURCE_OPCODE_GNT:
		p_params->b_granted = true;
		break;
	case RESOURCE_OPCODE_BUSY:
		p_params->b_granted = false;
		break;
	default:
		DP_NOTICE(p_hwfn,
			  "Unexpected opcode in resource lock response [mcp_param 0x%08x, opcode %d]\n",
			  mcp_param, opcode);
		return -EINVAL;
	}

	return 0;
}

int
qed_mcp_resc_lock(struct qed_hwfn *p_hwfn,
		  struct qed_ptt *p_ptt, struct qed_resc_lock_params *p_params)
{
	u32 retry_cnt = 0;
	int rc;

	do {
		/* No need for an interval before the first iteration */
		if (retry_cnt) {
			if (p_params->sleep_b4_retry) {
				u32 retry_interval_in_ms =
				    DIV_ROUND_UP(p_params->retry_interval,
						 1000);

				msleep(retry_interval_in_ms);
			} else {
				udelay(p_params->retry_interval);
			}
		}

		rc = __qed_mcp_resc_lock(p_hwfn, p_ptt, p_params);
		if (rc == -EAGAIN)
			continue;
		if (rc)
			return rc;

		if (p_params->b_granted)
			return 0;
	} while (retry_cnt++ < p_params->retry_num);

	return -EBUSY;
}

int
qed_mcp_resc_unlock(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    struct qed_resc_unlock_params *p_params)
{
	u32 param = 0, mcp_resp, mcp_param;
	u8 opcode;
	int rc;

	opcode = p_params->b_force ? RESOURCE_OPCODE_FORCE_RELEASE
	    : RESOURCE_OPCODE_RELEASE;
	SET_MFW_FIELD(param, RESOURCE_CMD_REQ_RESC, p_params->resource);
	SET_MFW_FIELD(param, RESOURCE_CMD_REQ_OPCODE, opcode);

	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "Resource unlock request: param 0x%08x [opcode %d, resource %d]\n",
		   param, opcode, p_params->resource);

	/* Attempt to release the resource */
	rc = qed_mcp_resource_cmd(p_hwfn, p_ptt, param, &mcp_resp, &mcp_param);
	if (rc)
		return rc;

	/* Analyze the response */
	opcode = GET_MFW_FIELD(mcp_param, RESOURCE_CMD_RSP_OPCODE);

	DP_VERBOSE(p_hwfn, QED_MSG_SP,
		   "Resource unlock response: mcp_param 0x%08x [opcode %d]\n",
		   mcp_param, opcode);

	switch (opcode) {
	case RESOURCE_OPCODE_RELEASED_PREVIOUS:
		DP_INFO(p_hwfn,
			"Resource unlock request for an already released resource [%d]\n",
			p_params->resource);
		COMPAT_FALLTHROUGH;
		/* fallthrough */
	case RESOURCE_OPCODE_RELEASED:
		p_params->b_released = true;
		break;
	case RESOURCE_OPCODE_WRONG_OWNER:
		p_params->b_released = false;
		break;
	default:
		DP_NOTICE(p_hwfn,
			  "Unexpected opcode in resource unlock response [mcp_param 0x%08x, opcode %d]\n",
			  mcp_param, opcode);
		return -EINVAL;
	}

	return 0;
}

void qed_mcp_resc_lock_default_init(struct qed_hwfn *p_hwfn,
				    struct qed_resc_lock_params *p_lock,
				    struct qed_resc_unlock_params *p_unlock,
				    enum qed_resc_lock
				    resource, bool b_is_permanent)
{
	if (p_lock != NULL) {
		memset(p_lock, 0, sizeof(*p_lock));

		/* Permanent resources don't require aging, and there's no
		 * point in trying to acquire them more than once since it's
		 * unexpected another entity would release them.
		 */
		if (b_is_permanent) {
			p_lock->timeout = QED_MCP_RESC_LOCK_TO_NONE;
		} else {
			p_lock->retry_num = p_hwfn->mcp_resc_lock_retry_cnt;
			p_lock->retry_interval =
			    QED_MCP_RESC_LOCK_RETRY_VAL_DFLT;
			p_lock->sleep_b4_retry = true;
		}

		p_lock->resource = resource;
	}

	if (p_unlock != NULL) {
		memset(p_unlock, 0, sizeof(*p_unlock));
		p_unlock->resource = resource;
	}
}

int
qed_mcp_update_fcoe_cvid(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt, u16 vlan)
{
	u32 resp = 0, param = 0, mcp_param = 0;
	int rc;

	SET_MFW_FIELD(param, DRV_MB_PARAM_FCOE_CVID, vlan);
	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_OEM_UPDATE_FCOE_CVID,
			 param, &resp, &mcp_param);
	if (rc)
		DP_ERR(p_hwfn, "Failed to update fcoe vlan, rc = %d\n", rc);

	return rc;
}

int
qed_mcp_update_fcoe_fabric_name(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt, u8 * wwn)
{
	struct qed_mcp_mb_params mb_params;
	struct mcp_wwn fabric_name;
	int rc;

	memset(&fabric_name, 0, sizeof(fabric_name));
	fabric_name.wwn_upper = *(u32 *) wwn;
	fabric_name.wwn_lower = *(u32 *) (wwn + 4);

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_OEM_UPDATE_FCOE_FABRIC_NAME;
	mb_params.p_data_src = &fabric_name;
	mb_params.data_src_size = sizeof(fabric_name);
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		DP_ERR(p_hwfn, "Failed to update fcoe wwn, rc = %d\n", rc);

	return rc;
}

void qed_mcp_wol_wr(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt, u32 offset, u32 val)
{
	int rc = 0;
	u32 dword = val;
	struct qed_mcp_mb_params mb_params;

	memset(&mb_params, 0, sizeof(struct qed_mcp_mb_params));
	mb_params.cmd = DRV_MSG_CODE_WRITE_WOL_REG;
	mb_params.param = offset;
	mb_params.p_data_src = &dword;
	mb_params.data_src_size = sizeof(dword);

	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		DP_NOTICE(p_hwfn, "Failed to wol write request, rc = %d\n", rc);

	if (mb_params.mcp_resp != FW_MSG_CODE_WOL_READ_WRITE_OK) {
		DP_NOTICE(p_hwfn,
			  "Failed to write value 0x%x to offset 0x%x [mcp_resp 0x%x]\n",
			  val, offset, mb_params.mcp_resp);
		rc = -EINVAL;
	}
}

bool qed_mcp_is_smart_an_supported(struct qed_hwfn *p_hwfn)
{
	return ! !(p_hwfn->mcp_info->capabilities &
		   FW_MB_PARAM_FEATURE_SUPPORT_SMARTLINQ);
}

bool qed_mcp_rlx_odr_supported(struct qed_hwfn * p_hwfn)
{
	return ! !(p_hwfn->mcp_info->capabilities &
		   FW_MB_PARAM_FEATURE_SUPPORT_RELAXED_ORD);
}

int qed_mcp_get_capabilities(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 mcp_resp;
	int rc;

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_GET_MFW_FEATURE_SUPPORT,
			 0, &mcp_resp, &p_hwfn->mcp_info->capabilities);
	if (!rc)
		DP_VERBOSE(p_hwfn, (QED_MSG_SP | NETIF_MSG_PROBE),
			   "MFW supported features: %08x\n",
			   p_hwfn->mcp_info->capabilities);

	return rc;
}

int qed_mcp_set_capabilities(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 mcp_resp, mcp_param, features;

	features = DRV_MB_PARAM_FEATURE_SUPPORT_PORT_SMARTLINQ |
	    DRV_MB_PARAM_FEATURE_SUPPORT_PORT_EEE |
	    DRV_MB_PARAM_FEATURE_SUPPORT_FUNC_VLINK |
	    DRV_MB_PARAM_FEATURE_SUPPORT_PORT_FEC_CONTROL;

	return qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_FEATURE_SUPPORT,
			   features, &mcp_resp, &mcp_param);
}

int
qed_mcp_drv_attribute(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt,
		      struct qed_mcp_drv_attr *p_drv_attr)
{
	struct attribute_cmd_write_stc attr_cmd_write;
	enum _attribute_commands_e mfw_attr_cmd;
	struct qed_mcp_mb_params mb_params;
	int rc;

	switch (p_drv_attr->attr_cmd) {
	case QED_MCP_DRV_ATTR_CMD_READ:
		mfw_attr_cmd = ATTRIBUTE_CMD_READ;
		break;
	case QED_MCP_DRV_ATTR_CMD_WRITE:
		mfw_attr_cmd = ATTRIBUTE_CMD_WRITE;
		break;
	case QED_MCP_DRV_ATTR_CMD_READ_CLEAR:
		mfw_attr_cmd = ATTRIBUTE_CMD_READ_CLEAR;
		break;
	case QED_MCP_DRV_ATTR_CMD_CLEAR:
		mfw_attr_cmd = ATTRIBUTE_CMD_CLEAR;
		break;
	default:
		DP_NOTICE(p_hwfn, "Unknown attribute command %d\n",
			  p_drv_attr->attr_cmd);
		return -EINVAL;
	}

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_ATTRIBUTE;
	mb_params.flags = QED_MB_FLAG_CAN_SLEEP;
	SET_MFW_FIELD(mb_params.param, DRV_MB_PARAM_ATTRIBUTE_KEY,
		      p_drv_attr->attr_num);
	SET_MFW_FIELD(mb_params.param, DRV_MB_PARAM_ATTRIBUTE_CMD,
		      mfw_attr_cmd);
	if (p_drv_attr->attr_cmd == QED_MCP_DRV_ATTR_CMD_WRITE) {
		memset(&attr_cmd_write, 0, sizeof(attr_cmd_write));
		attr_cmd_write.val = p_drv_attr->val;
		attr_cmd_write.mask = p_drv_attr->mask;
		attr_cmd_write.offset = p_drv_attr->offset;

		mb_params.p_data_src = &attr_cmd_write;
		mb_params.data_src_size = sizeof(attr_cmd_write);
	}

	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		return rc;

	if (mb_params.mcp_resp == FW_MSG_CODE_UNSUPPORTED) {
		DP_INFO(p_hwfn,
			"The attribute command is not supported by the MFW\n");
		return -EOPNOTSUPP;
	} else if (mb_params.mcp_resp != FW_MSG_CODE_OK) {
		DP_INFO(p_hwfn,
			"Failed to send an attribute command [mcp_resp 0x%x, attr_cmd %d, attr_num %d]\n",
			mb_params.mcp_resp,
			p_drv_attr->attr_cmd, p_drv_attr->attr_num);
		return -EINVAL;
	}

	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "Attribute Command: cmd %d [mfw_cmd %d], num %d, in={val 0x%08x, mask 0x%08x, offset 0x%08x}, out={val 0x%08x}\n",
		   p_drv_attr->attr_cmd,
		   mfw_attr_cmd,
		   p_drv_attr->attr_num,
		   p_drv_attr->val,
		   p_drv_attr->mask, p_drv_attr->offset, mb_params.mcp_param);

	if (p_drv_attr->attr_cmd == QED_MCP_DRV_ATTR_CMD_READ ||
	    p_drv_attr->attr_cmd == QED_MCP_DRV_ATTR_CMD_READ_CLEAR)
		p_drv_attr->val = mb_params.mcp_param;

	return 0;
}

int qed_mcp_get_engine_config(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	struct qed_mcp_mb_params mb_params;
	u8 fir_valid, l2_valid;
	int rc;

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_GET_ENGINE_CONFIG;
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		return rc;

	if (mb_params.mcp_resp == FW_MSG_CODE_UNSUPPORTED) {
		DP_INFO(p_hwfn,
			"The get_engine_config command is unsupported by the MFW\n");
		return -EOPNOTSUPP;
	}

	fir_valid = GET_MFW_FIELD(mb_params.mcp_param,
				  FW_MB_PARAM_ENG_CFG_FIR_AFFIN_VALID);
	if (fir_valid)
		cdev->fir_affin =
		    GET_MFW_FIELD(mb_params.mcp_param,
				  FW_MB_PARAM_ENG_CFG_FIR_AFFIN_VALUE);

	l2_valid = GET_MFW_FIELD(mb_params.mcp_param,
				 FW_MB_PARAM_ENG_CFG_L2_AFFIN_VALID);
	if (l2_valid)
		cdev->l2_affin_hint =
		    GET_MFW_FIELD(mb_params.mcp_param,
				  FW_MB_PARAM_ENG_CFG_L2_AFFIN_VALUE);

	DP_INFO(p_hwfn,
		"Engine affinity config: FIR={valid %hhd, value %hhd}, L2_hint={valid %hhd, value %hhd}\n",
		fir_valid, cdev->fir_affin, l2_valid, cdev->l2_affin_hint);

	return 0;
}

int qed_mcp_get_ppfid_bitmap(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	struct qed_mcp_mb_params mb_params;
	int rc;

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_GET_PPFID_BITMAP;
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		return rc;

	if (mb_params.mcp_resp == FW_MSG_CODE_UNSUPPORTED) {
		DP_INFO(p_hwfn,
			"The get_ppfid_bitmap command is unsupported by the MFW\n");
		return -EOPNOTSUPP;
	}

	cdev->ppfid_bitmap = GET_MFW_FIELD(mb_params.mcp_param,
					   FW_MB_PARAM_PPFID_BITMAP);

	DP_VERBOSE(p_hwfn, QED_MSG_SP, "PPFID bitmap 0x%hhx\n",
		   cdev->ppfid_bitmap);

	return 0;
}

int
qed_mcp_ind_table_lock(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt, u8 retry_num, u32 retry_interval)
{
	struct qed_resc_lock_params resc_lock_params;
	int rc;

	memset(&resc_lock_params, 0, sizeof(struct qed_resc_lock_params));
	resc_lock_params.resource = QED_RESC_LOCK_IND_TABLE;
	if (!retry_num)
		retry_num = p_hwfn->mcp_resc_lock_retry_cnt;
	resc_lock_params.retry_num = retry_num;

	if (!retry_interval)
		retry_interval = QED_MCP_RESC_LOCK_RETRY_VAL_DFLT;
	resc_lock_params.retry_interval = retry_interval;

	rc = qed_mcp_resc_lock(p_hwfn, p_ptt, &resc_lock_params);
	if (rc == 0 && !resc_lock_params.b_granted) {
		DP_NOTICE(p_hwfn,
			  "Failed to acquire the resource lock for IDT access\n");
		return -EBUSY;
	}
	return rc;
}

int qed_mcp_ind_table_unlock(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_resc_unlock_params resc_unlock_params;
	int rc;

	memset(&resc_unlock_params, 0, sizeof(struct qed_resc_unlock_params));
	resc_unlock_params.resource = QED_RESC_LOCK_IND_TABLE;
	rc = qed_mcp_resc_unlock(p_hwfn, p_ptt, &resc_unlock_params);
	return rc;
}

int
qed_mcp_nvm_get_cfg(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    u16 option_id,
		    u8 entity_id, u16 flags, u8 * p_buf, u32 * p_len)
{
	u32 mb_param = 0, resp, param;
	int rc;

	SET_MFW_FIELD(mb_param, DRV_MB_PARAM_NVM_CFG_OPTION_ID, option_id);
	if (flags & QED_NVM_CFG_OPTION_INIT)
		SET_MFW_FIELD(mb_param, DRV_MB_PARAM_NVM_CFG_OPTION_INIT, 1);
	if (flags & QED_NVM_CFG_OPTION_FREE)
		SET_MFW_FIELD(mb_param, DRV_MB_PARAM_NVM_CFG_OPTION_FREE, 1);
	if (flags & QED_NVM_CFG_OPTION_ENTITY_SEL) {
		SET_MFW_FIELD(mb_param, DRV_MB_PARAM_NVM_CFG_OPTION_ENTITY_SEL,
			      1);
		SET_MFW_FIELD(mb_param, DRV_MB_PARAM_NVM_CFG_OPTION_ENTITY_ID,
			      entity_id);
	}

	rc = qed_mcp_nvm_rd_cmd(p_hwfn, p_ptt,
				DRV_MSG_CODE_GET_NVM_CFG_OPTION, mb_param,
				&resp, &param, p_len, (u32 *) p_buf, true);

	return rc;
}

int
qed_mcp_nvm_set_cfg(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    u16 option_id, u8 entity_id, u16 flags, u8 * p_buf, u32 len)
{
	u32 mb_param = 0, resp, param;
	int rc;

	SET_MFW_FIELD(mb_param, DRV_MB_PARAM_NVM_CFG_OPTION_ID, option_id);
	if (flags & QED_NVM_CFG_OPTION_ALL)
		SET_MFW_FIELD(mb_param, DRV_MB_PARAM_NVM_CFG_OPTION_ALL, 1);
	if (flags & QED_NVM_CFG_OPTION_INIT)
		SET_MFW_FIELD(mb_param, DRV_MB_PARAM_NVM_CFG_OPTION_INIT, 1);
	if (flags & QED_NVM_CFG_OPTION_COMMIT)
		SET_MFW_FIELD(mb_param, DRV_MB_PARAM_NVM_CFG_OPTION_COMMIT, 1);
	if (flags & QED_NVM_CFG_OPTION_FREE)
		SET_MFW_FIELD(mb_param, DRV_MB_PARAM_NVM_CFG_OPTION_FREE, 1);
	if (flags & QED_NVM_CFG_OPTION_ENTITY_SEL) {
		SET_MFW_FIELD(mb_param, DRV_MB_PARAM_NVM_CFG_OPTION_ENTITY_SEL,
			      1);
		SET_MFW_FIELD(mb_param, DRV_MB_PARAM_NVM_CFG_OPTION_ENTITY_ID,
			      entity_id);
	}

	rc = qed_mcp_nvm_wr_cmd(p_hwfn, p_ptt,
				DRV_MSG_CODE_SET_NVM_CFG_OPTION, mb_param,
				&resp, &param, len, (u32 *) p_buf, true);

	return rc;
}

enum qed_perm_mac_type {
	QED_PERM_MAC_TYPE_PF,
	QED_PERM_MAC_TYPE_BMC,
	QED_PERM_MAC_TYPE_VF,
	QED_PERM_MAC_TYPE_LLDP,
};

static int
qed_mcp_get_permanent_mac(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt,
			  enum qed_perm_mac_type type,
			  u32 index, u8 mac_addr[ETH_ALEN])
{
	struct qed_mcp_mb_params mb_params;
	struct mcp_mac perm_mac;
	u32 mfw_type;
	int rc;

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_GET_PERM_MAC;
	switch (type) {
	case QED_PERM_MAC_TYPE_PF:
		mfw_type = DRV_MSG_CODE_GET_PERM_MAC_TYPE_PF;
		break;
	case QED_PERM_MAC_TYPE_BMC:
		mfw_type = DRV_MSG_CODE_GET_PERM_MAC_TYPE_BMC;
		break;
	case QED_PERM_MAC_TYPE_VF:
		mfw_type = DRV_MSG_CODE_GET_PERM_MAC_TYPE_VF;
		break;
	case QED_PERM_MAC_TYPE_LLDP:
		mfw_type = DRV_MSG_CODE_GET_PERM_MAC_TYPE_LLDP;
		break;
	default:
		DP_ERR(p_hwfn, "Unexpected type of permanent MAC [%d]\n", type);
		return -EINVAL;
	}
	SET_MFW_FIELD(mb_params.param, DRV_MSG_CODE_GET_PERM_MAC_TYPE,
		      mfw_type);
	SET_MFW_FIELD(mb_params.param, DRV_MSG_CODE_GET_PERM_MAC_INDEX, index);
	mb_params.p_data_dst = &perm_mac;
	mb_params.data_dst_size = sizeof(perm_mac);
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		return rc;

	if (mb_params.mcp_resp == FW_MSG_CODE_UNSUPPORTED) {
		DP_INFO(p_hwfn,
			"The GET_PERM_MAC command is unsupported by the MFW\n");
		return -EOPNOTSUPP;
	} else if (mb_params.mcp_resp != (u32) FW_MSG_CODE_GET_PERM_MAC_OK) {
		DP_NOTICE(p_hwfn,
			  "Failed to get a permanent MAC address [type %d, index %d, resp 0x%08x]\n",
			  type, index, mb_params.mcp_resp);
		return -EINVAL;
	}

	*(u16 *) mac_addr = be16_to_cpu(*(u16 *) & perm_mac.mac_upper);
	*(u32 *) (mac_addr + sizeof(u16)) = be32_to_cpu(perm_mac.mac_lower);

	DP_VERBOSE(p_hwfn,
		   QED_MSG_SP,
		   "Permanent MAC address [type %d, index %d] is %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
		   type,
		   index,
		   mac_addr[0],
		   mac_addr[1],
		   mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

	return 0;
}

int qed_mcp_get_perm_vf_mac(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt,
			    u32 rel_vf_id, u8 mac_addr[ETH_ALEN])
{
	u32 index;

	if (!RESC_NUM(p_hwfn, QED_VF_MAC_ADDR)) {
		DP_INFO(p_hwfn, "There are no permanent VF MAC addresses\n");
		return -EINVAL;
	}

	if (rel_vf_id >= RESC_NUM(p_hwfn, QED_VF_MAC_ADDR)) {
		DP_INFO(p_hwfn,
			"There are permanent VF MAC addresses for only the first %d VFs\n",
			RESC_NUM(p_hwfn, QED_VF_MAC_ADDR));
		return -EINVAL;
	}

	index = RESC_START(p_hwfn, QED_VF_MAC_ADDR) + rel_vf_id;
	return qed_mcp_get_permanent_mac(p_hwfn, p_ptt,
					 QED_PERM_MAC_TYPE_VF, index, mac_addr);
}

#define QED_MCP_DBG_DATA_MAX_SIZE               MCP_DRV_NVM_BUF_LEN
#define QED_MCP_DBG_DATA_MAX_HEADER_SIZE        sizeof(u32)
#define QED_MCP_DBG_DATA_MAX_PAYLOAD_SIZE \
	(QED_MCP_DBG_DATA_MAX_SIZE - QED_MCP_DBG_DATA_MAX_HEADER_SIZE)

static int
__qed_mcp_send_debug_data(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, u8 * p_buf, u8 size)
{
	struct qed_mcp_mb_params mb_params;
	int rc;

	if (size > QED_MCP_DBG_DATA_MAX_SIZE) {
		DP_ERR(p_hwfn,
		       "Debug data size is %d while it should not exceed %d\n",
		       size, QED_MCP_DBG_DATA_MAX_SIZE);
		return -EINVAL;
	}

	memset(&mb_params, 0, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_DEBUG_DATA_SEND;
	SET_MFW_FIELD(mb_params.param, DRV_MSG_CODE_DEBUG_DATA_SEND_SIZE, size);
	mb_params.p_data_src = p_buf;
	mb_params.data_src_size = size;
	rc = qed_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc)
		return rc;

	if (mb_params.mcp_resp == FW_MSG_CODE_UNSUPPORTED) {
		DP_INFO(p_hwfn,
			"The DEBUG_DATA_SEND command is unsupported by the MFW\n");
		return -EOPNOTSUPP;
	} else if (mb_params.mcp_resp == (u32) FW_MSG_CODE_DEBUG_NOT_ENABLED) {
		DP_INFO(p_hwfn, "The DEBUG_DATA_SEND command is not enabled\n");
		return -EBUSY;
	} else if (mb_params.mcp_resp != (u32) FW_MSG_CODE_DEBUG_DATA_SEND_OK) {
		DP_NOTICE(p_hwfn,
			  "Failed to send debug data to the MFW [resp 0x%08x]\n",
			  mb_params.mcp_resp);
		return -EINVAL;
	}

	return 0;
}

enum qed_mcp_dbg_data_type {
	QED_MCP_DBG_DATA_TYPE_RAW,
};

/* Header format: [31:28] PFID, [27:20] flags, [19:12] type, [11:0] S/N */
#define QED_MCP_DBG_DATA_HDR_SN_OFFSET  0
#define QED_MCP_DBG_DATA_HDR_SN_MASK            0x00000fff
#define QED_MCP_DBG_DATA_HDR_TYPE_OFFSET        12
#define QED_MCP_DBG_DATA_HDR_TYPE_MASK  0x000ff000
#define QED_MCP_DBG_DATA_HDR_FLAGS_OFFSET       20
#define QED_MCP_DBG_DATA_HDR_FLAGS_MASK 0x0ff00000
#define QED_MCP_DBG_DATA_HDR_PF_OFFSET  28
#define QED_MCP_DBG_DATA_HDR_PF_MASK            0xf0000000

#define QED_MCP_DBG_DATA_HDR_FLAGS_FIRST        0x1
#define QED_MCP_DBG_DATA_HDR_FLAGS_LAST 0x2

static int
qed_mcp_send_debug_data(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt,
			enum qed_mcp_dbg_data_type type, u8 * p_buf, u32 size)
{
	u8 raw_data[QED_MCP_DBG_DATA_MAX_SIZE], *p_tmp_buf = p_buf;
	u32 tmp_size = size, *p_header, *p_payload;
	u8 flags = 0;
	u16 seq;
	int rc;

	p_header = (u32 *) raw_data;
	p_payload = (u32 *) (raw_data + QED_MCP_DBG_DATA_MAX_HEADER_SIZE);

	spin_lock_bh(&p_hwfn->mcp_info->dbg_data_lock);
	seq = p_hwfn->mcp_info->dbg_data_seq++;
	spin_unlock_bh(&p_hwfn->mcp_info->dbg_data_lock);

	/* First chunk is marked as 'first' */
	flags |= QED_MCP_DBG_DATA_HDR_FLAGS_FIRST;

	*p_header = 0;
	SET_MFW_FIELD(*p_header, QED_MCP_DBG_DATA_HDR_SN, seq);
	SET_MFW_FIELD(*p_header, QED_MCP_DBG_DATA_HDR_TYPE, type);
	SET_MFW_FIELD(*p_header, QED_MCP_DBG_DATA_HDR_FLAGS, flags);
	SET_MFW_FIELD(*p_header, QED_MCP_DBG_DATA_HDR_PF, p_hwfn->abs_pf_id);

	while (tmp_size > QED_MCP_DBG_DATA_MAX_PAYLOAD_SIZE) {
		memcpy(p_payload, p_tmp_buf, QED_MCP_DBG_DATA_MAX_PAYLOAD_SIZE);
		rc = __qed_mcp_send_debug_data(p_hwfn, p_ptt, raw_data,
					       QED_MCP_DBG_DATA_MAX_SIZE);
		if (rc)
			return rc;

		/* Clear the 'first' marking after sending the first chunk */
		if (p_tmp_buf == p_buf) {
			flags &= ~QED_MCP_DBG_DATA_HDR_FLAGS_FIRST;
			SET_MFW_FIELD(*p_header, QED_MCP_DBG_DATA_HDR_FLAGS,
				      flags);
		}

		p_tmp_buf += QED_MCP_DBG_DATA_MAX_PAYLOAD_SIZE;
		tmp_size -= QED_MCP_DBG_DATA_MAX_PAYLOAD_SIZE;
	}

	/* Last chunk is marked as 'last' */
	flags |= QED_MCP_DBG_DATA_HDR_FLAGS_LAST;
	SET_MFW_FIELD(*p_header, QED_MCP_DBG_DATA_HDR_FLAGS, flags);
	memcpy(p_payload, p_tmp_buf, tmp_size);

	/* Casting the left size to u8 is ok since at this point it is <= 32 */
	return __qed_mcp_send_debug_data(p_hwfn, p_ptt, raw_data,
					 (u8) (QED_MCP_DBG_DATA_MAX_HEADER_SIZE
					       + tmp_size));
}

int
qed_mcp_send_raw_debug_data(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, u8 * p_buf, u32 size)
{
	return qed_mcp_send_debug_data(p_hwfn, p_ptt,
				       QED_MCP_DBG_DATA_TYPE_RAW, p_buf, size);
}

int
qed_mcp_set_bandwidth(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, u8 bw_min, u8 bw_max)
{
	u32 resp = 0, param = 0, drv_mb_param = 0;
	int rc;

	SET_MFW_FIELD(drv_mb_param, BW_MIN, bw_min);
	SET_MFW_FIELD(drv_mb_param, BW_MAX, bw_max);
	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_SET_BW,
			 drv_mb_param, &resp, &param);
	if (rc)
		DP_ERR(p_hwfn, "Failed to send BW update, rc = %d\n", rc);

	return rc;
}

bool qed_mcp_is_esl_supported(struct qed_hwfn * p_hwfn)
{
	return ! !(p_hwfn->mcp_info->capabilities &
		   FW_MB_PARAM_FEATURE_SUPPORT_ENHANCED_SYS_LCK);
}

int
qed_mcp_get_esl_status(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt, bool * active)
{
	u32 resp = 0, param = 0;
	int rc;

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_GET_MANAGEMENT_STATUS, 0,
			 &resp, &param);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed to send ESL command, rc = %d\n", rc);
		return rc;
	}

	*active = ! !(param & FW_MB_PARAM_MANAGEMENT_STATUS_LOCKDOWN_ENABLED);

	return 0;
}

int qed_mcp_gen_mdump_idlechk(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mdump_cmd_params mdump_cmd_params;

	memset(&mdump_cmd_params, 0, sizeof(mdump_cmd_params));
	mdump_cmd_params.cmd = DRV_MSG_CODE_MDUMP_GEN_IDLE_CHK;

	return qed_mcp_mdump_cmd(p_hwfn, p_ptt, &mdump_cmd_params);
}
