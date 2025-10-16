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
#include <linux/compiler.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#define __PREVENT_DUMP_MEM_ARR__
#define __PREVENT_PXP_GLOBAL_WIN__
#include "qed.h"
#include "qed_cxt.h"
#include "qed_dev_api.h"
#include "qed_hsi.h"
#include "qed_hw.h"
#include "qed_int.h"
#include "qed_iro_hsi.h"
#include "qed_iscsi.h"
#include "qed_mcp.h"
#include "qed_ooo.h"
#include "qed_rdma.h"
#include "qed_reg_addr.h"
#include "qed_sp.h"
#include "qed_sriov.h"
#ifdef CONFIG_QED_RDMA
#endif
#if IS_ENABLED(CONFIG_QEDI)
#endif

/***************************************************************************
* Structures & Definitions
***************************************************************************/

#define SPQ_HIGH_PRI_RESERVE_DEFAULT    (1)

#define SPQ_BLOCK_DELAY_MAX_ITER        (10)
#define SPQ_BLOCK_DELAY_US              (10)
#define SPQ_BLOCK_SLEEP_MAX_ITER        (200)
#define SPQ_BLOCK_SLEEP_MS              (5)

/***************************************************************************
* Blocking Imp. (BLOCK/EBLOCK mode)
***************************************************************************/
static void qed_spq_blocking_cb(struct qed_hwfn *p_hwfn,
				void *cookie,
				union event_ring_data __maybe_unused * data,
				u8 fw_return_code)
{
	struct qed_spq_comp_done *comp_done;

	comp_done = (struct qed_spq_comp_done *)cookie;

	comp_done->done = 0x1;
	comp_done->fw_return_code = fw_return_code;

	/* make update visible to waiting thread */
	smp_wmb();
}

static int __qed_spq_block(struct qed_hwfn *p_hwfn,
			   struct qed_spq_entry *p_ent,
			   u8 * p_fw_ret, bool sleep_between_iter)
{
	struct qed_spq_comp_done *comp_done;
	u32 iter_cnt;

	comp_done = (struct qed_spq_comp_done *)p_ent->comp_cb.cookie;
	iter_cnt =
	    sleep_between_iter ? p_hwfn->p_spq->block_sleep_max_iter
	    : SPQ_BLOCK_DELAY_MAX_ITER;
#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->cdev) && sleep_between_iter)
		iter_cnt *= 5;
#endif

	while (iter_cnt--) {
		if (QED_RECOV_IN_PROG(p_hwfn->cdev)) {
			DP_VERBOSE(p_hwfn,
				   QED_MSG_SPQ,
				   "Recovery is in progress. Skip completion. iter_cnt %d\n",
				   iter_cnt);
			/* return success to skip comp time-out handling */
			return 0;
		}

		smp_rmb();
		if (comp_done->done == 1) {
			if (p_fw_ret)
				*p_fw_ret = comp_done->fw_return_code;
			return 0;
		}

		if (p_hwfn->pf_params.eth_pf_params.is_ens_mode_enabled) {
			udelay(SPQ_BLOCK_SLEEP_MS * 1000);
		} else {
			if (sleep_between_iter)
				msleep(SPQ_BLOCK_SLEEP_MS);
			else
				udelay(SPQ_BLOCK_DELAY_US);
		}
	}

	return -EBUSY;
}

static int qed_spq_block(struct qed_hwfn *p_hwfn,
			 struct qed_spq_entry *p_ent,
			 u8 * p_fw_ret, bool skip_quick_poll)
{
	struct qed_spq_comp_done *comp_done;
	u8 str[QED_HW_ERR_MAX_STR_SIZE];
	struct qed_ptt *p_ptt;
	int rc;

	/* A relatively short polling period w/o sleeping, to allow the FW to
	 * complete the ramrod and thus possibly to avoid the following sleeps.
	 */
	if (!skip_quick_poll) {
		rc = __qed_spq_block(p_hwfn, p_ent, p_fw_ret, false);
		if (!rc)
			return rc;
	}

	/* Move to polling with a sleeping period between iterations */
	rc = __qed_spq_block(p_hwfn, p_ent, p_fw_ret, true);
	if (!rc)
		return rc;

	DP_INFO(p_hwfn, "Ramrod is stuck, requesting MCP drain\n");

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		goto err;

	rc = qed_mcp_drain(p_hwfn, p_ptt);
	qed_ptt_release(p_hwfn, p_ptt);
	if (rc) {
		DP_NOTICE(p_hwfn, "MCP drain failed\n");
		goto err;
	}

	/* Retry after drain */
	rc = __qed_spq_block(p_hwfn, p_ent, p_fw_ret, true);
	if (!rc)
		return rc;

	comp_done = (struct qed_spq_comp_done *)p_ent->comp_cb.cookie;
	if (comp_done->done == 1) {
		if (p_fw_ret)
			*p_fw_ret = comp_done->fw_return_code;

		return 0;
	}

	rc = -EBUSY;
err:
	scnprintf((char *)str, QED_HW_ERR_MAX_STR_SIZE,
		  "Ramrod is stuck [CID %08x %s:0x%02x %s:0x%02x echo %04x]\n",
		  le32_to_cpu(p_ent->elem.hdr.cid),
		  qed_get_ramrod_cmd_id_str(p_ent->elem.hdr.protocol_id,
					    p_ent->elem.hdr.cmd_id.raw),
		  p_ent->elem.hdr.cmd_id.raw,
		  qed_get_protocol_type_str(p_ent->elem.hdr.protocol_id),
		  p_ent->elem.hdr.protocol_id,
		  le16_to_cpu(p_ent->elem.hdr.echo));
	DP_NOTICE(p_hwfn, "%s", str);

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return rc;

	qed_hw_err_notify(p_hwfn, p_ptt, QED_HW_ERR_RAMROD_FAIL, (char *)str);

	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

void qed_set_spq_block_timeout(struct qed_hwfn *p_hwfn, u32 spq_timeout_ms)
{
	p_hwfn->p_spq->block_sleep_max_iter = spq_timeout_ms ?
	    spq_timeout_ms / SPQ_BLOCK_SLEEP_MS : SPQ_BLOCK_SLEEP_MAX_ITER;
}

/***************************************************************************
* SPQ entries inner API
***************************************************************************/
static int qed_spq_fill_entry(struct qed_hwfn *p_hwfn,
			      struct qed_spq_entry *p_ent)
{
	p_ent->flags = 0;

	switch (p_ent->comp_mode) {
	case QED_SPQ_MODE_EBLOCK:
	case QED_SPQ_MODE_BLOCK:
		p_ent->comp_cb.function = qed_spq_blocking_cb;
		break;
	case QED_SPQ_MODE_CB:
		break;
	default:
		DP_NOTICE(p_hwfn, "Unknown SPQE completion mode %d\n",
			  p_ent->comp_mode);
		return -EINVAL;
	}

	DP_VERBOSE(p_hwfn,
		   QED_MSG_SPQ,
		   "Ramrod header: [CID 0x%08x %s:0x%02x %s:0x%02x] Data pointer: [%08x:%08x] Completion Mode: %s\n",
		   p_ent->elem.hdr.cid,
		   qed_get_ramrod_cmd_id_str(p_ent->elem.hdr.protocol_id,
					     p_ent->elem.hdr.cmd_id.raw),
		   p_ent->elem.hdr.cmd_id.raw,
		   qed_get_protocol_type_str(p_ent->elem.hdr.protocol_id),
		   p_ent->elem.hdr.protocol_id,
		   p_ent->elem.data_ptr.hi,
		   p_ent->elem.data_ptr.lo,
		   D_TRINE(p_ent->comp_mode, QED_SPQ_MODE_EBLOCK,
			   QED_SPQ_MODE_BLOCK, "MODE_EBLOCK", "MODE_BLOCK",
			   "MODE_CB"));

	return 0;
}

/***************************************************************************
* HSI access
***************************************************************************/

#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_EN_MASK                   0x1
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_EN_SHIFT                  0
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_ACTIVE_MASK               0x1
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_ACTIVE_SHIFT              7
#define XSTORM_CORE_CONN_AG_CTX_SLOW_PATH_EN_MASK               0x1
#define XSTORM_CORE_CONN_AG_CTX_SLOW_PATH_EN_SHIFT              4
#define XSTORM_CORE_CONN_AG_CTX_CONSOLID_PROD_CF_EN_MASK        0x1
#define XSTORM_CORE_CONN_AG_CTX_CONSOLID_PROD_CF_EN_SHIFT       6

static void qed_spq_hw_initialize(struct qed_hwfn *p_hwfn,
				  struct qed_spq *p_spq)
{
	__le32 *p_spq_base_lo, *p_spq_base_hi;
	u8 *p_flags1, *p_flags9, *p_flags10;
	struct core_conn_context *p_cxt;
	struct qed_cxt_info cxt_info;
	u32 core_conn_context_size;
	__le16 *p_physical_q0;
	u16 physical_q;
	int rc;

	cxt_info.iid = p_spq->cid;

	rc = qed_cxt_get_cid_info(p_hwfn, &cxt_info);
	if (rc) {
		DP_NOTICE(p_hwfn, "Cannot find context info for cid=%d\n",
			  p_spq->cid);
		return;
	}

	p_cxt = cxt_info.p_cxt;
	core_conn_context_size = sizeof(*p_cxt);
	p_flags1 = &p_cxt->xstorm_ag_context.flags1;
	p_flags9 = &p_cxt->xstorm_ag_context.flags9;
	p_flags10 = &p_cxt->xstorm_ag_context.flags10;
	p_physical_q0 = &p_cxt->xstorm_ag_context.physical_q0;
	p_spq_base_lo = &p_cxt->xstorm_st_context.spq_base_addr.lo;
	p_spq_base_hi = &p_cxt->xstorm_st_context.spq_base_addr.hi;

	/* @@@TBD we zero the context until we have ilt_reset implemented. */
	memset(p_cxt, 0, core_conn_context_size);

	SET_FIELD(*p_flags10, XSTORM_CORE_CONN_AG_CTX_DQ_CF_EN, 1);
	SET_FIELD(*p_flags1, XSTORM_CORE_CONN_AG_CTX_DQ_CF_ACTIVE, 1);
	SET_FIELD(*p_flags9, XSTORM_CORE_CONN_AG_CTX_CONSOLID_PROD_CF_EN, 1);

	/* CDU validation - FIXME currently disabled */

	/* QM physical queue */
	physical_q = qed_get_cm_pq_idx(p_hwfn, PQ_FLAGS_LB);
	*p_physical_q0 = cpu_to_le16(physical_q);

	*p_spq_base_lo = DMA_LO_LE(p_spq->chain.p_phys_addr);
	*p_spq_base_hi = DMA_HI_LE(p_spq->chain.p_phys_addr);
}

static int qed_spq_hw_post(struct qed_hwfn *p_hwfn,
			   struct qed_spq *p_spq, struct qed_spq_entry *p_ent)
{
	struct qed_chain *p_chain = &p_hwfn->p_spq->chain;
	struct core_db_data *p_db_data = &p_spq->db_data;
	u16 echo = qed_chain_get_prod_idx(p_chain);
	struct slow_path_element *elem;

	p_ent->elem.hdr.echo = cpu_to_le16(echo);
	elem = qed_chain_produce(p_chain);
	if (!elem) {
		DP_NOTICE(p_hwfn, "Failed to produce from SPQ chain\n");
		return -EINVAL;
	}

	*elem = p_ent->elem;	/* Struct assignment */

	p_db_data->spq_prod = cpu_to_le16(qed_chain_get_prod_idx(p_chain));

	/* Make sure the SPQE is updated before the doorbell */
	wmb();

	writel((u32) * (u32 *) p_db_data,
	       (void __iomem *)((u8 __iomem *) (p_hwfn->doorbells) +
				(p_spq->db_addr_offset)));

	/* Make sure doorbell was rung */
	wmb();

	DP_VERBOSE(p_hwfn,
		   QED_MSG_SPQ,
		   "Doorbelled [0x%08x, CID 0x%08x] with Flags: %02x agg_params: %02x, prod: %04x\n",
		   p_spq->db_addr_offset,
		   p_spq->cid,
		   p_db_data->params,
		   p_db_data->agg_flags, qed_chain_get_prod_idx(p_chain));

	return 0;
}

/***************************************************************************
* Asynchronous events
***************************************************************************/
int
qed_async_event_completion(struct qed_hwfn *p_hwfn,
			   struct event_ring_entry *p_eqe)
{
	qed_spq_async_comp_cb cb;
	int rc;

	if (p_eqe->protocol_id >= MAX_PROTOCOL_TYPE) {
		DP_ERR(p_hwfn, "Wrong protocol: %s:%d\n",
		       qed_get_protocol_type_str(p_eqe->protocol_id),
		       p_eqe->protocol_id);
		return -EINVAL;
	}

	/* This function is called by PF only, and only for VF completions.
	 * The exceptions which are handled by the PF are PROTOCOLID_COMMON
	 * and ROCE_ASYNC_EVENT_DESTROY_QP_DONE events.
	 */
	if (IS_PF(p_hwfn->cdev) && p_eqe->vf_id != QED_CXT_PF_CID &&
	    p_eqe->protocol_id != PROTOCOLID_COMMON &&
	    !(p_eqe->protocol_id == PROTOCOLID_ROCE &&
	      p_eqe->opcode.roce == ROCE_ASYNC_EVENT_DESTROY_QP_DONE))
		return qed_iov_async_event_completion(p_hwfn, p_eqe);

	cb = p_hwfn->p_spq->async_comp_cb[p_eqe->protocol_id];
	if (!cb) {
		DP_NOTICE(p_hwfn,
			  "Unknown Async completion for %s:%d\n",
			  qed_get_protocol_type_str(p_eqe->protocol_id),
			  p_eqe->protocol_id);
		return -EINVAL;
	}

	rc = cb(p_hwfn, p_eqe->opcode.raw, p_eqe->echo,
		&p_eqe->data, p_eqe->fw_return_code.raw, p_eqe->vf_id);
	if (rc)
		DP_NOTICE(p_hwfn,
			  "Async completion callback failed, rc = %d [%s:0x%02x, echo %x, fw_return_code %x]",
			  rc,
			  qed_get_event_ring_entry_opcode_str(p_eqe),
			  p_eqe->opcode.raw,
			  p_eqe->echo, p_eqe->fw_return_code.raw);

	return rc;
}

int
qed_spq_register_async_cb(struct qed_hwfn *p_hwfn,
			  enum protocol_type protocol_id,
			  qed_spq_async_comp_cb cb)
{
	if (!p_hwfn->p_spq || (protocol_id >= MAX_PROTOCOL_TYPE) ||
	    (protocol_id < 0))
		return -EINVAL;

	p_hwfn->p_spq->async_comp_cb[protocol_id] = cb;
	return 0;
}

void
qed_spq_unregister_async_cb(struct qed_hwfn *p_hwfn,
			    enum protocol_type protocol_id)
{
	if (!p_hwfn->p_spq || (protocol_id >= MAX_PROTOCOL_TYPE) ||
	    (protocol_id < 0))
		return;

	p_hwfn->p_spq->async_comp_cb[protocol_id] = NULL;
}

/***************************************************************************
* EQ API
***************************************************************************/
void qed_eq_prod_update(struct qed_hwfn *p_hwfn, u16 prod)
{
	u32 addr = GET_GTT_REG_ADDR(GTT_BAR0_MAP_REG_USDM_RAM,
				    USTORM_EQE_CONS, p_hwfn->rel_pf_id);

	REG_WR16(p_hwfn, addr, prod);
}

void qed_event_ring_entry_dp(struct qed_hwfn *p_hwfn,
			     struct event_ring_entry *p_eqe)
{
	DP_VERBOSE(p_hwfn,
		   QED_MSG_SPQ,
		   "%s:0x%02x %s:0x%02x res0 %x vfid %x echo %x fwret %x flags %x\n",
		   qed_get_event_ring_entry_opcode_str(p_eqe),
		   p_eqe->opcode.raw,
		   qed_get_protocol_type_str(p_eqe->protocol_id),
		   p_eqe->protocol_id,
		   p_eqe->reserved0,
		   p_eqe->vf_id,
		   le16_to_cpu(p_eqe->echo),
		   p_eqe->fw_return_code.raw, p_eqe->flags);
}

int qed_eq_completion(struct qed_hwfn *p_hwfn, void *cookie)
{
	struct qed_eq *p_eq = cookie;
	struct qed_chain *p_chain = &p_eq->chain;
	u16 fw_cons_idx = 0;
	int rc;

	if (!p_hwfn->p_spq) {
		DP_ERR(p_hwfn, "Unexpected NULL p_spq\n");
		return -EINVAL;
	}

	/* take a snapshot of the FW consumer */
	fw_cons_idx = le16_to_cpu(*p_eq->p_fw_cons);

	DP_VERBOSE(p_hwfn, QED_MSG_SPQ, "fw_cons_idx %x\n", fw_cons_idx);

	/* Need to guarantee the fw_cons index we use points to a usuable
	 * element (to comply with our chain), so our macros would comply
	 */
	if ((fw_cons_idx & qed_chain_get_usable_per_page(p_chain)) ==
	    qed_chain_get_usable_per_page(p_chain))
		fw_cons_idx += qed_chain_get_unusable_per_page(p_chain);

	/* Complete current segment of eq entries */
	while (fw_cons_idx != qed_chain_get_cons_idx(p_chain)) {
		struct event_ring_entry *p_eqe = qed_chain_consume(p_chain);
		if (!p_eqe) {
			DP_ERR(p_hwfn,
			       "Unexpected NULL chain consumer entry\n");
			break;
		}

		qed_event_ring_entry_dp(p_hwfn, p_eqe);

		if (GET_FIELD(p_eqe->flags, EVENT_RING_ENTRY_ASYNC)) {
			qed_async_event_completion(p_hwfn, p_eqe);
		} else {
			qed_spq_completion(p_hwfn,
					   p_eqe->echo,
					   p_eqe->fw_return_code.raw,
					   &p_eqe->data);
		}

		qed_chain_recycle_consumed(p_chain);
	}

	qed_eq_prod_update(p_hwfn, qed_chain_get_prod_idx(p_chain));

	/* Attempt to post pending requests */
	spin_lock_bh(&p_hwfn->p_spq->lock);
	rc = qed_spq_pend_post(p_hwfn);
	spin_unlock_bh(&p_hwfn->p_spq->lock);

	return rc;
}

int qed_eq_alloc(struct qed_hwfn *p_hwfn, u16 num_elem)
{
	struct qed_chain_params chain_params;
	struct qed_eq *p_eq;

	/* Allocate EQ struct */
	p_eq = kzalloc(sizeof(*p_eq), GFP_KERNEL);
	if (!p_eq) {
		DP_NOTICE(p_hwfn, "Failed to allocate `struct qed_eq'\n");
		return -ENOMEM;
	}

	/* Allocate and initialize EQ chain */
	qed_chain_params_init(&chain_params,
			      QED_CHAIN_USE_TO_PRODUCE,
			      QED_CHAIN_MODE_PBL,
			      QED_CHAIN_CNT_TYPE_U16,
			      num_elem, sizeof(union event_ring_element));
	if (qed_chain_alloc(p_hwfn->cdev, &p_eq->chain, &chain_params)) {
		DP_NOTICE(p_hwfn, "Failed to allocate eq chain\n");
		goto eq_allocate_fail;
	}

	/* register EQ completion on the SP SB */
	qed_int_register_cb(p_hwfn, qed_eq_completion,
			    p_eq, &p_eq->eq_sb_index, &p_eq->p_fw_cons);

	p_hwfn->p_eq = p_eq;
	return 0;

eq_allocate_fail:
	kfree(p_eq);
	return -ENOMEM;
}

void qed_eq_setup(struct qed_hwfn *p_hwfn)
{
	qed_chain_reset(&p_hwfn->p_eq->chain);
}

void qed_eq_free(struct qed_hwfn *p_hwfn)
{
	if (!p_hwfn->p_eq)
		return;

	qed_chain_free(p_hwfn->cdev, &p_hwfn->p_eq->chain);

	kfree(p_hwfn->p_eq);
	p_hwfn->p_eq = NULL;
}

/***************************************************************************
* CQE API - manipulate EQ functionallity
***************************************************************************/
static int qed_cqe_completion(struct qed_hwfn *p_hwfn,
			      struct eth_slow_path_rx_cqe *cqe,
			      enum protocol_type protocol)
{
	if (IS_VF(p_hwfn->cdev))
		return 0;

	/* @@@tmp - it's possible we'll eventually want to handle some
	 * actual commands that can arrive here, but for now this is only
	 * used to complete the ramrod using the echo value on the cqe
	 */
	return qed_spq_completion(p_hwfn, cqe->echo, 0, NULL);
}

int qed_eth_cqe_completion(struct qed_hwfn *p_hwfn,
			   struct eth_slow_path_rx_cqe *cqe)
{
	int rc;

	rc = qed_cqe_completion(p_hwfn, cqe, PROTOCOLID_ETH);
	if (rc)
		DP_NOTICE(p_hwfn,
			  "Failed to handle RXQ CQE [cmd 0x%02x]\n",
			  cqe->ramrod_cmd_id);

	return rc;
}

/***************************************************************************
* Slow hwfn Queue (spq)
***************************************************************************/
void qed_spq_setup(struct qed_hwfn *p_hwfn)
{
	struct qed_spq *p_spq = p_hwfn->p_spq;
	struct qed_spq_entry *p_virt = NULL;
	struct core_db_data *p_db_data;
	void __iomem *db_addr;
	dma_addr_t p_phys = 0;
	u32 i, capacity;
	int rc;

	INIT_LIST_HEAD(&p_spq->pending);
	INIT_LIST_HEAD(&p_spq->completion_pending);
	INIT_LIST_HEAD(&p_spq->free_pool);
	INIT_LIST_HEAD(&p_spq->unlimited_pending);
	spin_lock_init(&p_spq->lock);

	/* SPQ empty pool */
	p_phys = p_spq->p_phys + offsetof(struct qed_spq_entry, ramrod);
	p_virt = p_spq->p_virt;

	capacity = qed_chain_get_capacity(&p_spq->chain);
	for (i = 0; i < capacity; i++) {
		DMA_REGPAIR_LE(p_virt->elem.data_ptr, p_phys);

		list_add_tail(&p_virt->list, &p_spq->free_pool);

		p_virt++;
		p_phys += sizeof(struct qed_spq_entry);
	}

	/* Statistics */
	p_spq->normal_count = 0;
	p_spq->comp_count = 0;
	p_spq->comp_sent_count = 0;
	p_spq->unlimited_pending_count = 0;

	memset(p_spq->p_comp_bitmap, 0, SPQ_COMP_BMAP_SIZE *
	       sizeof(unsigned long));
	p_spq->comp_bitmap_idx = 0;

	/* SPQ cid, cannot fail */
	qed_cxt_acquire_cid(p_hwfn, PROTOCOLID_CORE, &p_spq->cid);
	qed_spq_hw_initialize(p_hwfn, p_spq);

	/* reset the chain itself */
	qed_chain_reset(&p_spq->chain);

	/* Initialize the address/data of the SPQ doorbell */
	p_spq->db_addr_offset = DB_ADDR(p_spq->cid, DQ_DEMS_LEGACY);
	p_db_data = &p_spq->db_data;
	memset(p_db_data, 0, sizeof(*p_db_data));
	SET_FIELD(p_db_data->params, CORE_DB_DATA_DEST, DB_DEST_XCM);
	SET_FIELD(p_db_data->params, CORE_DB_DATA_AGG_CMD, DB_AGG_CMD_MAX);
	SET_FIELD(p_db_data->params, CORE_DB_DATA_AGG_VAL_SEL,
		  DQ_XCM_CORE_SPQ_PROD_CMD);
	p_db_data->agg_flags = DQ_XCM_CORE_DQ_CF_CMD;

	/* Register the SPQ doorbell with the doorbell recovery mechanism */
	db_addr = (void __iomem *)((u8 __iomem *) p_hwfn->doorbells +
				   p_spq->db_addr_offset);
	rc = qed_db_recovery_add(p_hwfn->cdev, db_addr, &p_spq->db_data,
				 DB_REC_WIDTH_32B, DB_REC_KERNEL);
	if (rc)
		DP_INFO(p_hwfn,
			"Failed to register the SPQ doorbell with the doorbell recovery mechanism\n");
}

int qed_spq_alloc(struct qed_hwfn *p_hwfn)
{
	struct qed_spq_entry *p_virt = NULL;
	struct qed_chain_params chain_params;
	struct qed_spq *p_spq = NULL;
	dma_addr_t p_phys = 0;
	u32 capacity;

	/* SPQ struct */
	p_spq = kzalloc(sizeof(struct qed_spq), GFP_KERNEL);
	if (!p_spq) {
		DP_NOTICE(p_hwfn, "Failed to allocate `struct qed_spq'\n");
		return -ENOMEM;
	}

	/* SPQ for VF is needed only for async_comp callbacks */
	if (IS_VF(p_hwfn->cdev)) {
		p_hwfn->p_spq = p_spq;
		return 0;
	}

	/* SPQ ring  */
	qed_chain_params_init(&chain_params, QED_CHAIN_USE_TO_PRODUCE, QED_CHAIN_MODE_SINGLE, QED_CHAIN_CNT_TYPE_U16, 0,	/* N/A when the mode is SINGLE */
			      sizeof(struct slow_path_element));
	if (qed_chain_alloc(p_hwfn->cdev, &p_spq->chain, &chain_params)) {
		DP_NOTICE(p_hwfn, "Failed to allocate spq chain\n");
		goto spq_allocate_fail;
	}

	/* allocate and fill the SPQ elements (incl. ramrod data list) */
	capacity = qed_chain_get_capacity(&p_spq->chain);
	p_virt = dma_alloc_coherent(&p_hwfn->cdev->pdev->dev,
				    capacity *
				    sizeof(struct qed_spq_entry),
				    &p_phys, GFP_KERNEL);
	if (!p_virt)
		goto spq_allocate_fail;

	p_spq->p_virt = p_virt;
	p_spq->p_phys = p_phys;

	p_hwfn->p_spq = p_spq;
	return 0;

spq_allocate_fail:
	qed_chain_free(p_hwfn->cdev, &p_spq->chain);
	kfree(p_spq);
	return -ENOMEM;
}

void qed_spq_free(struct qed_hwfn *p_hwfn)
{
	struct qed_spq *p_spq = p_hwfn->p_spq;
	void __iomem *db_addr;
	u32 capacity;

	if (!p_spq)
		return;

	if (IS_VF(p_hwfn->cdev))
		goto out;

	/* Delete the SPQ doorbell from the doorbell recovery mechanism */
	db_addr = (void __iomem *)((u8 __iomem *) p_hwfn->doorbells +
				   p_spq->db_addr_offset);
	qed_db_recovery_del(p_hwfn->cdev, db_addr, &p_spq->db_data);

	if (p_spq->p_virt) {
		capacity = qed_chain_get_capacity(&p_spq->chain);
		dma_free_coherent(&p_hwfn->cdev->pdev->dev,
				  capacity *
				  sizeof(struct qed_spq_entry),
				  p_spq->p_virt, p_spq->p_phys);
	}

	qed_chain_free(p_hwfn->cdev, &p_spq->chain);
out:
	kfree(p_spq);
	p_hwfn->p_spq = NULL;
}

int qed_spq_get_entry(struct qed_hwfn *p_hwfn, struct qed_spq_entry **pp_ent)
{
	struct qed_spq *p_spq = p_hwfn->p_spq;
	struct qed_spq_entry *p_ent = NULL;
	int rc = 0;

	spin_lock_bh(&p_spq->lock);

	if (list_empty(&p_spq->free_pool)) {
		p_ent = kzalloc(sizeof(*p_ent), GFP_ATOMIC);
		if (!p_ent) {
			DP_NOTICE(p_hwfn,
				  "Failed to allocate an SPQ entry for a pending ramrod\n");
			rc = -ENOMEM;
			goto out_unlock;
		}
		p_ent->queue = &p_spq->unlimited_pending;
	} else {
		p_ent = list_first_entry(&p_spq->free_pool,
					 struct qed_spq_entry, list);
		list_del(&p_ent->list);
		p_ent->queue = &p_spq->pending;
	}

	*pp_ent = p_ent;

out_unlock:
	spin_unlock_bh(&p_spq->lock);
	return rc;
}

/* Locked variant; Should be called while the SPQ lock is taken */
static void __qed_spq_return_entry(struct qed_hwfn *p_hwfn,
				   struct qed_spq_entry *p_ent)
{
	list_add_tail(&p_ent->list, &p_hwfn->p_spq->free_pool);
}

void qed_spq_return_entry(struct qed_hwfn *p_hwfn, struct qed_spq_entry *p_ent)
{
	spin_lock_bh(&p_hwfn->p_spq->lock);
	__qed_spq_return_entry(p_hwfn, p_ent);
	spin_unlock_bh(&p_hwfn->p_spq->lock);
}

/**
 * @brief qed_spq_add_entry - adds a new entry to the pending
 *        list. Should be used while lock is being held.
 *
 * Addes an entry to the pending list is there is room (en empty
 * element is avaliable in the free_pool), or else places the
 * entry in the unlimited_pending pool.
 *
 * @param p_hwfn
 * @param p_ent
 * @param priority
 *
 * @return int
 */
static int qed_spq_add_entry(struct qed_hwfn *p_hwfn,
			     struct qed_spq_entry *p_ent,
			     enum spq_priority priority)
{
	struct qed_spq *p_spq = p_hwfn->p_spq;

	if (p_ent->queue == &p_spq->unlimited_pending) {
		if (list_empty(&p_spq->free_pool)) {
			list_add_tail(&p_ent->list, &p_spq->unlimited_pending);
			p_spq->unlimited_pending_count++;

			return 0;
		} else {
			struct qed_spq_entry *p_en2;

			p_en2 = list_first_entry(&p_spq->free_pool,
						 struct qed_spq_entry, list);
			list_del(&p_en2->list);

			/* Copy the ring element physical pointer to the new
			 * entry, since we are about to override the entire ring
			 * entry and don't want to lose the pointer.
			 */
			p_ent->elem.data_ptr = p_en2->elem.data_ptr;

			*p_en2 = *p_ent;

			/* EBLOCK responsible to free the allocated p_ent */
			if (p_ent->comp_mode != QED_SPQ_MODE_EBLOCK)
				kfree(p_ent);
			else
				p_ent->post_ent = p_en2;

			p_ent = p_en2;
		}
	}

	/* entry is to be placed in 'pending' queue */
	switch (priority) {
	case QED_SPQ_PRIORITY_NORMAL:
		list_add_tail(&p_ent->list, &p_spq->pending);
		p_spq->normal_count++;
		break;
	case QED_SPQ_PRIORITY_HIGH:
		list_add(&p_ent->list, &p_spq->pending);
		p_spq->high_count++;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/***************************************************************************
* Accessor
***************************************************************************/
u32 qed_spq_get_cid(struct qed_hwfn * p_hwfn)
{
	if (!p_hwfn->p_spq)
		return 0xffffffff;	/* illegal */
	return p_hwfn->p_spq->cid;
}

/***************************************************************************
* Posting new Ramrods
***************************************************************************/
static int qed_spq_post_list(struct qed_hwfn *p_hwfn,
			     struct list_head *head, u32 keep_reserve)
{
	struct qed_spq *p_spq = p_hwfn->p_spq;
	int rc;

	/* TODO - implementation might be wasteful; will always keep room
	 * for an additional high priority ramrod (even if one is already
	 * pending FW)
	 */
	while (qed_chain_get_elem_left(&p_spq->chain) > keep_reserve &&
	       !list_empty(head)) {
		struct qed_spq_entry *p_ent =
		    list_first_entry(head, struct qed_spq_entry, list);
		if (p_ent != NULL) {
			list_del(&p_ent->list);
			list_add_tail(&p_ent->list, &p_spq->completion_pending);
			p_spq->comp_sent_count++;

			rc = qed_spq_hw_post(p_hwfn, p_spq, p_ent);
			if (rc) {
				list_del(&p_ent->list);
				__qed_spq_return_entry(p_hwfn, p_ent);
				return rc;
			}
		}
	}

	return 0;
}

int qed_spq_pend_post(struct qed_hwfn *p_hwfn)
{
	struct qed_spq *p_spq = p_hwfn->p_spq;
	struct qed_spq_entry *p_ent = NULL;

	while (!list_empty(&p_spq->free_pool)) {
		if (list_empty(&p_spq->unlimited_pending))
			break;

		p_ent = list_first_entry(&p_spq->unlimited_pending,
					 struct qed_spq_entry, list);
		if (!p_ent)
			return -EINVAL;

		list_del(&p_ent->list);

		qed_spq_add_entry(p_hwfn, p_ent, p_ent->priority);
	}

	return qed_spq_post_list(p_hwfn, &p_spq->pending,
				 SPQ_HIGH_PRI_RESERVE_DEFAULT);
}

static void qed_spq_recov_set_ret_code(struct qed_spq_entry *p_ent,
				       u8 * fw_return_code)
{
	if (fw_return_code == NULL)
		return;

	if (p_ent->elem.hdr.protocol_id == PROTOCOLID_ROCE ||
	    p_ent->elem.hdr.protocol_id == PROTOCOLID_IWARP)
		*fw_return_code = RDMA_RETURN_OK;
}

/* Avoid overriding of SPQ entries when getting out-of-order completions, by
 * marking the completions in a bitmap and increasing the chain consumer only
 * for the first successive completed entries.
 */
static void qed_spq_comp_bmap_update(struct qed_hwfn *p_hwfn, __le16 echo)
{
	struct qed_spq *p_spq = p_hwfn->p_spq;

	SPQ_COMP_BMAP_SET_BIT(p_spq, echo);
	while (SPQ_COMP_BMAP_TEST_BIT(p_spq, p_spq->comp_bitmap_idx)) {
		SPQ_COMP_BMAP_CLEAR_BIT(p_spq, p_spq->comp_bitmap_idx);
		p_spq->comp_bitmap_idx++;
		qed_chain_return_produced(&p_spq->chain);
	}
}

int qed_spq_post(struct qed_hwfn *p_hwfn,
		 struct qed_spq_entry *p_ent, u8 * fw_return_code)
{
	int rc = 0;
	struct qed_spq *p_spq = p_hwfn ? p_hwfn->p_spq : NULL;
	struct ramrod_header hdr;
	bool b_ret_ent = true;
	bool eblock;

	if (!p_hwfn)
		return -EINVAL;

	if (!p_ent) {
		DP_NOTICE(p_hwfn, "Got a NULL pointer\n");
		return -EINVAL;
	}

	hdr = p_ent->elem.hdr;

	if (QED_RECOV_IN_PROG(p_hwfn->cdev)) {
		DP_VERBOSE(p_hwfn,
			   QED_MSG_SPQ,
			   "Recovery is in progress. Skip spq post [%s:0x%02x %s:0x%02x]\n",
			   qed_get_ramrod_cmd_id_str(hdr.protocol_id,
						     hdr.cmd_id.raw),
			   hdr.cmd_id.raw,
			   qed_get_protocol_type_str(hdr.protocol_id),
			   hdr.protocol_id);

		/* Let the flow complete w/o any error handling */
		qed_spq_recov_set_ret_code(p_ent, fw_return_code);
		return 0;
	}

	spin_lock_bh(&p_spq->lock);

	/* Complete the entry */
	rc = qed_spq_fill_entry(p_hwfn, p_ent);

	/* Check return value after LOCK is taken for cleaner error flow */
	if (rc)
		goto spq_post_fail;

	/* Check if entry is in block mode before qed_spq_add_entry,
	 * which might kfree p_ent.
	 */
	eblock = (p_ent->comp_mode == QED_SPQ_MODE_EBLOCK);

	/* Add the request to the pending queue */
	rc = qed_spq_add_entry(p_hwfn, p_ent, p_ent->priority);
	if (rc)
		goto spq_post_fail;

	rc = qed_spq_pend_post(p_hwfn);
	if (rc) {
		/* Since it's possible that pending failed for a different
		 * entry [although unlikely], the failed entry was already
		 * dealt with; No need to return it here.
		 */
		b_ret_ent = false;
		goto spq_post_fail;
	}

	spin_unlock_bh(&p_spq->lock);

	if (eblock) {
		/* For entries in QED BLOCK mode, the completion code cannot
		 * perform the necessary cleanup - if it did, we couldn't
		 * access p_ent here to see whether it's successful or not.
		 * Thus, after gaining the answer perform the cleanup here.
		 */
		rc = qed_spq_block(p_hwfn, p_ent, fw_return_code,
				   p_ent->queue == &p_spq->unlimited_pending);

		if (p_ent->queue == &p_spq->unlimited_pending) {
			struct qed_spq_entry *p_post_ent = p_ent->post_ent;
			kfree(p_ent);

			/* Return the entry which was actually posted */
			p_ent = p_post_ent;
		}

		if (rc)
			goto spq_post_fail2;

		/* return to pool */
		qed_spq_return_entry(p_hwfn, p_ent);
	}
	return rc;

spq_post_fail2:
	spin_lock_bh(&p_spq->lock);
	list_del(&p_ent->list);
	if (SPQ_COMP_BMAP_TEST_BIT(p_spq, p_ent->elem.hdr.echo))
		DP_ERR(p_hwfn, "SPQ Entry %p (echo %x) already completed\n",
		       p_ent, p_ent->elem.hdr.echo);
	qed_spq_comp_bmap_update(p_hwfn, p_ent->elem.hdr.echo);

spq_post_fail:
	/* return to the free pool */
	if (b_ret_ent)
		__qed_spq_return_entry(p_hwfn, p_ent);
	spin_unlock_bh(&p_spq->lock);

	return rc;
}

int qed_spq_completion(struct qed_hwfn *p_hwfn,
		       __le16 echo,
		       u8 fw_return_code, union event_ring_data *p_data)
{
	struct qed_spq *p_spq;
	struct qed_spq_entry *p_ent = NULL;
	struct qed_spq_entry *tmp;
	struct qed_spq_entry *found = NULL;

	p_spq = p_hwfn->p_spq;
	if (!p_spq) {
		DP_ERR(p_hwfn, "Unexpected NULL p_spq\n");
		return -EINVAL;
	}

	spin_lock_bh(&p_spq->lock);
	list_for_each_entry_safe(p_ent, tmp, &p_spq->completion_pending, list) {
		if (p_ent->elem.hdr.echo == echo) {
#ifndef QED_UPSTREAM
			/* Ignoring completions is only supported for EBLOCK */
			if (p_spq->drop_next_completion &&
			    p_ent->comp_mode == QED_SPQ_MODE_EBLOCK) {
				DP_VERBOSE(p_hwfn,
					   QED_MSG_SPQ,
					   "Got completion for echo %04x - ignoring\n",
					   echo);
				p_spq->drop_next_completion = false;
				spin_unlock_bh(&p_spq->lock);
				return 0;
			}
#endif

			list_del(&p_ent->list);

			qed_spq_comp_bmap_update(p_hwfn, echo);
			p_spq->comp_count++;
			found = p_ent;
			break;
		}

		/* This is debug and should be relatively uncommon - depends
		 * on scenarios which have mutliple per-PF sent ramrods.
		 */
		DP_VERBOSE(p_hwfn,
			   QED_MSG_SPQ,
			   "Got completion for echo %04x - doesn't match echo %04x in completion pending list\n",
			   le16_to_cpu(echo),
			   le16_to_cpu(p_ent->elem.hdr.echo));
	}

	/* Release lock before callback, as callback may post
	 * an additional ramrod.
	 */
	spin_unlock_bh(&p_spq->lock);

	if (!found) {
		DP_NOTICE(p_hwfn,
			  "Failed to find an entry this EQE [echo %04x] completes\n",
			  le16_to_cpu(echo));
		return -EEXIST;
	}

	DP_VERBOSE(p_hwfn, QED_MSG_SPQ,
		   "Complete EQE [echo %04x]: func %p cookie %p)\n",
		   le16_to_cpu(echo),
		   p_ent->comp_cb.function, p_ent->comp_cb.cookie);
	if (found->comp_cb.function)
		found->comp_cb.function(p_hwfn, found->comp_cb.cookie, p_data,
					fw_return_code);
	else
		DP_VERBOSE(p_hwfn,
			   QED_MSG_SPQ,
			   "Got a completion without a callback function\n");

	/* EBLOCK is responsible for returning its own entry */
	if (found->comp_mode != QED_SPQ_MODE_EBLOCK)
		qed_spq_return_entry(p_hwfn, found);

	return 0;
}

#ifndef QED_UPSTREAM
void qed_spq_drop_next_completion(struct qed_hwfn *p_hwfn)
{
	p_hwfn->p_spq->drop_next_completion = true;
}
#endif

int qed_consq_alloc(struct qed_hwfn *p_hwfn)
{
	struct qed_chain_params chain_params;
	struct qed_consq *p_consq;

	/* Allocate ConsQ struct */
	p_consq = kzalloc(sizeof(*p_consq), GFP_KERNEL);
	if (!p_consq) {
		DP_NOTICE(p_hwfn, "Failed to allocate `struct qed_consq'\n");
		return -ENOMEM;
	}

	/* Allocate and initialize EQ chain */
	qed_chain_params_init(&chain_params,
			      QED_CHAIN_USE_TO_PRODUCE,
			      QED_CHAIN_MODE_PBL,
			      QED_CHAIN_CNT_TYPE_U16,
			      QED_CHAIN_PAGE_SIZE / 0x80, 0x80);
	if (qed_chain_alloc(p_hwfn->cdev, &p_consq->chain, &chain_params)) {
		DP_NOTICE(p_hwfn, "Failed to allocate consq chain");
		goto consq_allocate_fail;
	}

	p_hwfn->p_consq = p_consq;
	return 0;

consq_allocate_fail:
	kfree(p_consq);
	return -ENOMEM;
}

void qed_consq_setup(struct qed_hwfn *p_hwfn)
{
	qed_chain_reset(&p_hwfn->p_consq->chain);
}

void qed_consq_free(struct qed_hwfn *p_hwfn)
{
	if (!p_hwfn->p_consq)
		return;

	qed_chain_free(p_hwfn->cdev, &p_hwfn->p_consq->chain);

	kfree(p_hwfn->p_consq);
	p_hwfn->p_consq = NULL;
}
