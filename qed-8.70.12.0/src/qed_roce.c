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
#include <linux/compiler.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/log2.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#define __PREVENT_DUMP_MEM_ARR__
#define __PREVENT_PXP_GLOBAL_WIN__
#include "qed.h"
#include "qed_cxt.h"
#include "qed_dcbx.h"
#include "qed_hsi.h"
#include "qed_hw.h"
#include "qed_init_ops.h"
#include "qed_int.h"
#include "qed_iro_hsi.h"
#include "qed_mcp.h"
#include "qed_rdma.h"
#include "qed_reg_addr.h"
#include "qed_sp.h"
#include "qed_sriov.h"

static void qed_roce_free_icid(struct qed_hwfn *p_hwfn, u16 icid, u8 abs_vf_id);

static int
qed_roce_async_event(struct qed_hwfn *p_hwfn,
		     u8 fw_event_code,
		     u16 __maybe_unused echo,
		     union event_ring_data *data,
		     u8 __maybe_unused fw_return_code, u8 vf_id)
{
	if (fw_event_code == ROCE_ASYNC_EVENT_DESTROY_QP_DONE) {
		u16 icid =
		    (u16) le32_to_cpu(data->rdma_data.rdma_destroy_qp_data.cid);

		/* icid release in this async event can occur only if the icid
		 * was offloaded to the FW. In case it wasn't offloaded this is
		 * handled in qed_roce_sp_destroy_qp.
		 */
		qed_roce_free_icid(p_hwfn, icid, vf_id);
	} else if (fw_event_code == ROCE_ASYNC_EVENT_SUSPEND_QP_DONE) {
		qed_roce_pvrdma_async_event(p_hwfn, data);
	} else {
		if (fw_event_code == ROCE_ASYNC_EVENT_SRQ_EMPTY ||
		    fw_event_code == ROCE_ASYNC_EVENT_SRQ_LIMIT) {
			u16 srq_id = (u16) data->rdma_data.async_handle.lo;

			p_hwfn->p_rdma_info->events.affiliated_event(p_hwfn->
								     p_rdma_info->
								     events.
								     context,
								     fw_event_code,
								     &srq_id);
		} else {
			p_hwfn->p_rdma_info->events.affiliated_event(p_hwfn->
								     p_rdma_info->
								     events.
								     context,
								     fw_event_code,
								     (void *)
								     &data->
								     rdma_data.
								     async_handle);
		}
	}

	return 0;
}

#ifdef CONFIG_DCQCN
static int qed_roce_start_rl(struct qed_hwfn *p_hwfn,
			     struct qed_roce_dcqcn_params *dcqcn_params)
{
	struct qed_rl_update_params params;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "\n");
	memset(&params, 0, sizeof(params));

	params.rl_id_first = (u8) RESC_START(p_hwfn, QED_RL);
	params.rl_id_last = RESC_START(p_hwfn, QED_RL) +
	    qed_init_qm_get_num_pf_rls(p_hwfn);
	params.dcqcn_update_param_flg = 1;
	params.rl_init_flg = 1;
	params.rl_start_flg = 1;
	params.rl_stop_flg = 0;
	params.rl_dc_qcn_flg = 1;

	params.rl_bc_rate = dcqcn_params->rl_bc_rate;
	params.rl_max_rate = dcqcn_params->rl_max_rate;
	params.rl_r_ai = dcqcn_params->rl_r_ai;
	params.rl_r_hai = dcqcn_params->rl_r_hai;
	params.dcqcn_gd = dcqcn_params->dcqcn_gd;
	params.dcqcn_k_us = dcqcn_params->dcqcn_k_us;
	params.dcqcn_timeuot_us = dcqcn_params->dcqcn_timeout_us;

	return qed_sp_rl_update(p_hwfn, &params);
}

int qed_roce_stop_rl(struct qed_hwfn *p_hwfn)
{
	struct qed_rl_update_params params;

	if (!p_hwfn->p_rdma_info->roce.dcqcn_reaction_point)
		return 0;

	memset(&params, 0, sizeof(params));
	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "\n");

	params.rl_id_first = (u8) RESC_START(p_hwfn, QED_RL);
	params.rl_id_last = RESC_START(p_hwfn, QED_RL) +
	    qed_init_qm_get_num_pf_rls(p_hwfn);
	params.rl_stop_flg = 1;

	return qed_sp_rl_update(p_hwfn, &params);
}

#define NIG_REG_ROCE_DUPLICATE_TO_HOST_BTH 2

int qed_roce_dcqcn_cfg(struct qed_hwfn *p_hwfn,
		       struct qed_roce_dcqcn_params *params,
		       struct roce_init_func_ramrod_data *p_ramrod,
		       struct qed_ptt *p_ptt)
{
	int rc = 0;

	if (!IS_QED_DCQCN(p_hwfn) ||
	    p_hwfn->p_rdma_info->proto == PROTOCOLID_IWARP)
		return rc;

	p_hwfn->p_rdma_info->roce.dcqcn_enabled = 0;
	if (params->notification_point) {
		DP_VERBOSE(p_hwfn,
			   QED_MSG_RDMA,
			   "Configuring dcqcn notification point: timeout = 0x%x\n",
			   params->cnp_send_timeout);
		p_ramrod->roce.cnp_send_timeout = params->cnp_send_timeout;
		p_hwfn->p_rdma_info->roce.dcqcn_enabled = 1;
		SET_FIELD(p_ramrod->roce.flags,
			  ROCE_INIT_FUNC_PARAMS_DCQCN_NP_EN,
			  params->notification_point);
	}

	if (params->reaction_point) {
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
			   "Configuring dcqcn reaction point\n");
		p_hwfn->p_rdma_info->roce.dcqcn_enabled = 1;
		p_hwfn->p_rdma_info->roce.dcqcn_reaction_point = 1;
		SET_FIELD(p_ramrod->roce.flags,
			  ROCE_INIT_FUNC_PARAMS_DCQCN_RP_EN,
			  params->reaction_point);

		rc = qed_roce_start_rl(p_hwfn, params);
	}

	if (rc)
		return rc;

	p_ramrod->roce.cnp_dscp = params->cnp_dscp;
	p_ramrod->roce.cnp_vlan_priority = params->cnp_vlan_priority;

	/* set rl offset and amount for this PF */
	p_ramrod->roce.rl_offset = (u8) RESC_START(p_hwfn, QED_RL);
	p_ramrod->roce.rl_count_log =
	    (u8) ilog2(qed_init_qm_get_num_pf_rls(p_hwfn));

	return rc;
}
#endif

#define QED_ROCE_TEST_CID_BITMAP_ATTEMPTS       (200)
#define QED_ROCE_TEST_CID_BITMAP_MSLEEP (100)

int qed_roce_stop(struct qed_hwfn *p_hwfn, struct qed_rdma_info *rdma_info)
{
	struct qed_bmap *cid_map = &rdma_info->cid_map;
	int wait_count = QED_ROCE_TEST_CID_BITMAP_ATTEMPTS;

	/* when destroying a_RoCE QP the control is returned to the
	 * user after the synchronous part. The asynchronous part may
	 * take a little longer. We delay for a short while if an
	 * asyn destroy QP is still expected. Beyond the added delay
	 * we clear the bitmap anyway.
	 */
	while (bitmap_weight(cid_map->bitmap, cid_map->max_count)) {
		/* If the HW device is during recovery, all resources are
		 * immediately reset without receiving a per-cid indication
		 * from HW. In this case we don't expect the cid bitmap to be
		 * cleared.
		 */
		if (p_hwfn->cdev->recov_in_prog)
			return 0;

		msleep(QED_ROCE_TEST_CID_BITMAP_MSLEEP);
		if (!wait_count) {
			DP_NOTICE(p_hwfn, "cid bitmap wait timed out\n");
			break;
		}

		wait_count--;
	}

	return 0;
}

void qed_rdma_copy_gids(struct qed_rdma_qp *qp,
			__le32 * src_gid, __le32 * dst_gid)
{
	u32 i;

	if (qp->roce_mode == ROCE_V2_IPV4) {
		/* The IPv4 addresses shall be aligned to the highest word.
		 * The lower words must be zero.
		 */
		memset(src_gid, 0, sizeof(union qed_gid));
		memset(dst_gid, 0, sizeof(union qed_gid));
		src_gid[3] = cpu_to_le32(qp->sgid.ipv4_addr);
		dst_gid[3] = cpu_to_le32(qp->dgid.ipv4_addr);
	} else {
		/* RoCE, and RoCE v2 - IPv6: GIDs and IPv6 addresses coincide in
		 * location and size
		 */
		for (i = 0; i < ARRAY_SIZE(qp->sgid.dwords); i++) {
			src_gid[i] = cpu_to_le32(qp->sgid.dwords[i]);
			dst_gid[i] = cpu_to_le32(qp->dgid.dwords[i]);
		}
	}
}

enum roce_flavor qed_roce_mode_to_flavor(enum roce_mode roce_mode)
{
	switch (roce_mode) {
	case ROCE_V1:
		return PLAIN_ROCE;
	case ROCE_V2_IPV4:
		return RROCE_IPV4;
	case ROCE_V2_IPV6:
		return RROCE_IPV6;
	default:
		return MAX_ROCE_FLAVOR;
	}
}

void qed_roce_free_qp(struct qed_hwfn *p_hwfn,
		      u16 qp_idx, struct qed_rdma_info *rdma_info)
{
	spin_lock_bh(&rdma_info->lock);
	qed_bmap_release_id(p_hwfn, &rdma_info->qp_map, qp_idx);
	spin_unlock_bh(&rdma_info->lock);
}

#define QED_ROCE_CREATE_QP_ATTEMPTS             (200)
#define QED_ROCE_CREATE_QP_MSLEEP               (10)

static int
qed_roce_wait_free_cids(struct qed_hwfn *p_hwfn,
			u32 qp_idx, struct qed_rdma_info *p_rdma_info)
{
	bool cids_free = false;
	u32 icid, iter = 0;
	int req, resp;

	icid = qed_roce_qp_idx_to_icid(p_hwfn, (u16) qp_idx,
				       p_rdma_info->iov_info.abs_vf_id);

	/* Make sure that the cids that were used by the QP index are free.
	 * This is necessary because the destroy flow returns to the user before
	 * the device finishes clean up.
	 * It can happen in the following flows:
	 * (1) ib_destroy_qp followed by an ib_create_qp
	 * (2) ib_modify_qp to RESET followed (not immediately), by an
	 *     ib_modify_qp to RTR
	 */

	do {
		spin_lock_bh(&p_rdma_info->lock);
		resp = qed_bmap_test_id(p_hwfn, &p_rdma_info->cid_map, icid);
		req = qed_bmap_test_id(p_hwfn, &p_rdma_info->cid_map, icid + 1);
		if (!resp && !req)
			cids_free = true;

		spin_unlock_bh(&p_rdma_info->lock);

		if (!cids_free) {
			msleep(QED_ROCE_CREATE_QP_MSLEEP);
			iter++;
		}
	} while (!cids_free && iter < QED_ROCE_CREATE_QP_ATTEMPTS);

	if (!cids_free) {
		DP_ERR(p_hwfn->cdev,
		       "responder and/or requester CIDs are still in use. resp=%d, req=%d\n",
		       resp, req);
		return -EAGAIN;
	}

	return 0;
}

int qed_roce_alloc_qp_idx(struct qed_hwfn *p_hwfn,
			  u16 * qp_idx16, struct qed_rdma_info *rdma_info)
{
	int rc;
	u32 icid, qp_idx;

	spin_lock_bh(&rdma_info->lock);
	rc = qed_rdma_bmap_alloc_id(p_hwfn, &rdma_info->qp_map, &qp_idx);
	if (rc) {
		DP_NOTICE(p_hwfn, "failed to allocate qp\n");
		spin_unlock_bh(&rdma_info->lock);
		return rc;
	}

	spin_unlock_bh(&rdma_info->lock);

	/* Verify the cid bits that of this qp index are clear */
	rc = qed_roce_wait_free_cids(p_hwfn, qp_idx, rdma_info);
	if (rc) {
		rc = -EINVAL;
		goto err;
	}

	/* Allocate a DMA-able context for an ILT page, if not existing, for the
	 * associated iids.
	 * Note: If second allocation fails there's no need to free the first as
	 *       it will be used in the future.
	 */

	icid = qed_roce_qp_idx_to_icid(p_hwfn, (u16) qp_idx,
				       rdma_info->iov_info.abs_vf_id);

	rc = qed_cxt_dynamic_ilt_alloc(p_hwfn, QED_ELEM_CXT, icid,
				       rdma_info->iov_info.rel_vf_id);

	if (rc)
		goto err;

	rc = qed_cxt_dynamic_ilt_alloc(p_hwfn, QED_ELEM_CXT, icid + 1,
				       rdma_info->iov_info.rel_vf_id);
	if (rc)
		goto err;

	rc = qed_rdma_configure_prs(p_hwfn, rdma_info, icid + 1);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed to configure prs\n");
		goto err;
	}

	/* qp index is under 2^16 */
	*qp_idx16 = (u16) qp_idx;

	return 0;

err:
	qed_roce_free_qp(p_hwfn, (u16) qp_idx, rdma_info);

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "rc = %d\n", rc);

	return rc;
}

int qed_roce_reserve_qp_idx(struct qed_hwfn *p_hwfn,
			    struct qed_rdma_info *rdma_info)
{
	/* reserve qp 0 for VF0 */
	if (rdma_info->iov_info.abs_vf_id == 0) {
		int rc;
		u16 qp_idx;

		rc = qed_roce_alloc_qp_idx(p_hwfn, &qp_idx, rdma_info);
		if (rc) {
			DP_NOTICE(p_hwfn, "Failed to reserve QP 0 for VF0\n");
			return rc;
		}

		DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
			   "QP 0 has been reserved for VF0\n");
	}

	return 0;
}

void qed_roce_free_reserved_qp_idx(struct qed_hwfn *p_hwfn,
				   struct qed_rdma_info *rdma_info)
{
	/* free the reserved qp 0 for VF0 over RoCE */
	if (rdma_info->iov_info.abs_vf_id == 0)
		qed_roce_free_qp(p_hwfn, 0, rdma_info);
}

void qed_roce_set_cid(struct qed_hwfn *p_hwfn,
		      struct qed_rdma_info *p_rdma_info, u32 cid)
{
	spin_lock_bh(&p_rdma_info->lock);
	qed_bmap_set_id(p_hwfn, &p_rdma_info->cid_map, cid);
	spin_unlock_bh(&p_rdma_info->lock);
}

u8 qed_roce_get_qp_tc(struct qed_hwfn *p_hwfn, struct qed_rdma_qp *qp)
{
	struct qed_rdma_glob_cfg *p_glob_cfg = &p_hwfn->p_rdma_info->glob_cfg;
	int rc;
	u8 dscp, pri;

	pri = GET_FIELD(qp->vlan_id, QED_VLAN_PRIO);

	if (p_glob_cfg->vlan_pri_en)
		return qed_dcbx_get_priority_tc(p_hwfn, pri);

	if (qed_dcbx_get_dscp_state(p_hwfn)) {
		dscp = GET_FIELD(qp->traffic_class_tos, QED_TOS_DSCP);

		rc = qed_dcbx_get_dscp_priority(p_hwfn, dscp, &pri);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "qp %u: qed_dcbx_get_dscp_priority failed\n",
				  qp->qp_idx);
			return 0;
		}

		/* Override VLAN priority with DSCP's priority */
		if (! !qp->vlan_id)
			SET_FIELD(qp->vlan_id, QED_VLAN_PRIO, pri);

		return qed_dcbx_get_priority_tc(p_hwfn, pri);
	}

	if (! !qp->vlan_id)
		return qed_dcbx_get_priority_tc(p_hwfn, pri);

	return 0;
}

static void qed_roce_set_pqs(struct qed_hwfn *p_hwfn,
			     struct qed_rdma_qp *qp,
			     enum qed_iov_is_vf_or_pf is_vf_or_pf,
			     u8 rel_vf_id,
			     u16 * regular_latency_queue,
			     u16 * low_latency_queue)
{
	u8 tc;

	qed_qm_acquire_access(p_hwfn);
	if (!QED_PQSET_SUPPORTED(p_hwfn))
		tc = qed_roce_get_qp_tc(p_hwfn, qp);
	else
		tc = qp->tc;

	if (is_vf_or_pf == IOV_PF) {
		*regular_latency_queue =
		    qed_get_cm_pq_idx_ofld_mtc(p_hwfn, qp->qp_idx, tc,
					       qp->pq_set_id);
		*low_latency_queue =
		    qed_get_cm_pq_idx_llt_mtc(p_hwfn, qp->qp_idx, tc,
					      qp->pq_set_id);
	} else {
		*regular_latency_queue =
		    qed_get_cm_pq_idx_vf_rdma(p_hwfn, rel_vf_id, tc,
					      qp->pq_set_id);
		*low_latency_queue =
		    qed_get_cm_pq_idx_vf_rdma_llt(p_hwfn, rel_vf_id, tc,
						  qp->pq_set_id);
	}

	qed_qm_release_access(p_hwfn);

	DP_VERBOSE(p_hwfn, QED_MSG_SP,
		   "qp_idx %u pqs: regular_latency %u low_latency %u (tc %u)\n",
		   qp->qp_idx, *regular_latency_queue - CM_TX_PQ_BASE,
		   *low_latency_queue - CM_TX_PQ_BASE, tc);
}

static int qed_roce_sp_create_responder(struct qed_hwfn *p_hwfn,
					struct qed_rdma_qp *qp,
					struct qed_rdma_info *rdma_info)
{
	struct roce_create_qp_resp_ramrod_data *p_ramrod;
	u16 regular_latency_queue, low_latency_queue;
	struct qed_rdma_iov_info *p_iov_info;
	struct qed_sp_init_data init_data;
	enum roce_flavor roce_flavor;
	struct qed_spq_entry *p_ent;
	int rc;
	u32 cid_start;

	if (!qp->has_resp)
		return 0;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "qp_idx = %08x\n", qp->qp_idx);

	p_iov_info = &rdma_info->iov_info;

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = qp->icid;

	init_data.opaque_fid = p_iov_info->opaque_fid;

	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	rc = qed_sp_init_request(p_hwfn, &p_ent, ROCE_RAMROD_CREATE_QP,
				 PROTOCOLID_ROCE, &init_data);
	if (rc)
		goto err;

	p_ramrod = &p_ent->ramrod.roce_create_qp_resp;

	p_ramrod->flags = 0;

	roce_flavor = qed_roce_mode_to_flavor(qp->roce_mode);
	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_RESP_RAMROD_DATA_ROCE_FLAVOR, roce_flavor);

	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_RESP_RAMROD_DATA_RDMA_RD_EN,
		  qp->incoming_rdma_read_en);

	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_RESP_RAMROD_DATA_RDMA_WR_EN,
		  qp->incoming_rdma_write_en);

	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_RESP_RAMROD_DATA_ATOMIC_EN,
		  qp->incoming_atomic_en);

	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_RESP_RAMROD_DATA_E2E_FLOW_CONTROL_EN,
		  qp->e2e_flow_control_en);

	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_RESP_RAMROD_DATA_SRQ_FLG, qp->use_srq);

	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_RESP_RAMROD_DATA_RESERVED_KEY_EN,
		  qp->fmr_and_reserved_lkey);

	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_RESP_RAMROD_DATA_XRC_FLAG,
		  qed_rdma_is_xrc_qp(qp));

	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_RESP_RAMROD_DATA_VF_ID_VALID,
		  rdma_info->iov_info.is_vf);

	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_RESP_RAMROD_DATA_FORCE_LB, qp->force_lb);

	/* TBD: future use only
	 * #define ROCE_CREATE_QP_RESP_RAMROD_DATA_PRI_MASK
	 * #define ROCE_CREATE_QP_RESP_RAMROD_DATA_PRI_SHIFT
	 */
	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_RESP_RAMROD_DATA_MIN_RNR_NAK_TIMER,
		  qp->min_rnr_nak_timer);

	p_ramrod->max_ird = qp->max_rd_atomic_resp;
	p_ramrod->traffic_class = qp->traffic_class_tos;
	p_ramrod->hop_limit = qp->hop_limit_ttl;
	p_ramrod->irq_num_pages = qp->irq_num_pages;
	p_ramrod->p_key = cpu_to_le16(qp->pkey);
	p_ramrod->flow_label = cpu_to_le32(qp->flow_label);
	p_ramrod->dst_qp_id = cpu_to_le32(qp->dest_qp);
	p_ramrod->mtu = cpu_to_le16(qp->mtu);
	p_ramrod->initial_psn = cpu_to_le32(qp->rq_psn);
	p_ramrod->pd = cpu_to_le16(qp->pd);
	p_ramrod->rq_num_pages = cpu_to_le16(qp->rq_num_pages);
	DMA_REGPAIR_LE(p_ramrod->rq_pbl_addr, qp->rq_pbl_ptr);
	DMA_REGPAIR_LE(p_ramrod->irq_pbl_addr, qp->irq_phys_addr);
	qed_rdma_copy_gids(qp, p_ramrod->src_gid, p_ramrod->dst_gid);
	p_ramrod->qp_handle_for_async.hi = cpu_to_le32(qp->qp_handle_async.hi);
	p_ramrod->qp_handle_for_async.lo = cpu_to_le32(qp->qp_handle_async.lo);
	p_ramrod->qp_handle_for_cqe.hi = cpu_to_le32(qp->qp_handle.hi);
	p_ramrod->qp_handle_for_cqe.lo = cpu_to_le32(qp->qp_handle.lo);
	p_ramrod->cq_cid = cpu_to_le32((p_iov_info->opaque_fid << 16) | qp->
				       rq_cq_id);
	p_ramrod->xrc_domain = cpu_to_le16(qp->xrcd_id);

	qed_roce_set_pqs(p_hwfn, qp, p_iov_info->is_vf,
			 p_iov_info->rel_vf_id,
			 &regular_latency_queue, &low_latency_queue);

	if (p_iov_info->is_vf == IOV_PF) {
		p_ramrod->vport_id = (u8) RESC_START(p_hwfn, QED_VPORT);
	} else {
		rc = qed_fw_vport(p_hwfn, p_iov_info->p_vf->vport_id,
				  &p_ramrod->vport_id);
		if (rc)
			goto err;
	}

	p_ramrod->regular_latency_phy_queue =
	    cpu_to_le16(regular_latency_queue);
	p_ramrod->low_latency_phy_queue = cpu_to_le16(low_latency_queue);

	p_ramrod->dpi = cpu_to_le16(qp->dpi);

	qed_rdma_set_fw_mac(p_ramrod->remote_mac_addr, qp->remote_mac_addr);
	qed_rdma_set_fw_mac(p_ramrod->local_mac_addr, qp->local_mac_addr);

	p_ramrod->udp_src_port = qp->udp_src_port;
	p_ramrod->vlan_id = cpu_to_le16(qp->vlan_id);
	p_ramrod->srq_id.srq_idx = cpu_to_le16(qp->srq_id);
	p_ramrod->srq_id.opaque_fid = cpu_to_le16(p_iov_info->opaque_fid);

	p_ramrod->stats_counter_id = qp->stats_queue;

	qed_roce_pvrdma_create_responder(p_hwfn, qp, p_ramrod);

	rc = qed_spq_post(p_hwfn, p_ent, NULL);
	if (rc)
		goto err;

	qp->resp_offloaded = true;
	qp->cq_prod.resp = 0;

	cid_start = qed_cxt_get_proto_cid_start(p_hwfn,
						rdma_info->proto,
						p_iov_info->abs_vf_id);

	qed_roce_set_cid(p_hwfn, rdma_info, qp->icid - cid_start);

	return rc;

err:
	DP_NOTICE(p_hwfn, "create responder - failed, rc = %d\n", rc);

	return rc;
}

static int qed_roce_sp_create_requester(struct qed_hwfn *p_hwfn,
					struct qed_rdma_qp *qp,
					struct qed_rdma_info *rdma_info)
{
	struct roce_create_qp_req_ramrod_data *p_ramrod;
	u16 regular_latency_queue, low_latency_queue;
	struct qed_rdma_iov_info *p_iov_info;
	struct qed_sp_init_data init_data;
	enum roce_flavor roce_flavor;
	struct qed_spq_entry *p_ent;
	int rc;
	u32 cid_start;

	if (!qp->has_req)
		return 0;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "icid = %08x\n", qp->icid);

	p_iov_info = &rdma_info->iov_info;

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = qp->icid + 1;

	init_data.opaque_fid = p_iov_info->opaque_fid;

	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	rc = qed_sp_init_request(p_hwfn, &p_ent,
				 ROCE_RAMROD_CREATE_QP,
				 PROTOCOLID_ROCE, &init_data);
	if (rc)
		goto err;

	p_ramrod = &p_ent->ramrod.roce_create_qp_req;

	p_ramrod->flags = 0;
	p_ramrod->flags2 = 0;

	roce_flavor = qed_roce_mode_to_flavor(qp->roce_mode);
	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_REQ_RAMROD_DATA_ROCE_FLAVOR, roce_flavor);

	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_REQ_RAMROD_DATA_FMR_AND_RESERVED_EN,
		  qp->fmr_and_reserved_lkey);

	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_REQ_RAMROD_DATA_SIGNALED_COMP, qp->signal_all);

	/* TBD:
	 * future use only
	 * #define ROCE_CREATE_QP_REQ_RAMROD_DATA_PRI_MASK
	 * #define ROCE_CREATE_QP_REQ_RAMROD_DATA_PRI_SHIFT
	 */
	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_REQ_RAMROD_DATA_ERR_RETRY_CNT, qp->retry_cnt);

	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_REQ_RAMROD_DATA_RNR_NAK_CNT,
		  qp->rnr_retry_cnt);

	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_REQ_RAMROD_DATA_XRC_FLAG,
		  qed_rdma_is_xrc_qp(qp));

	SET_FIELD(p_ramrod->flags2,
		  ROCE_CREATE_QP_REQ_RAMROD_DATA_EDPM_MODE, qp->edpm_mode);

	SET_FIELD(p_ramrod->flags2,
		  ROCE_CREATE_QP_REQ_RAMROD_DATA_VF_ID_VALID,
		  rdma_info->iov_info.is_vf);

	SET_FIELD(p_ramrod->flags2,
		  ROCE_CREATE_QP_REQ_RAMROD_DATA_FORCE_LB, qp->force_lb);

	p_ramrod->max_ord = qp->max_rd_atomic_req;
	p_ramrod->traffic_class = qp->traffic_class_tos;
	p_ramrod->hop_limit = qp->hop_limit_ttl;
	p_ramrod->orq_num_pages = qp->orq_num_pages;
	p_ramrod->p_key = cpu_to_le16(qp->pkey);
	p_ramrod->flow_label = cpu_to_le32(qp->flow_label);
	p_ramrod->dst_qp_id = cpu_to_le32(qp->dest_qp);
	p_ramrod->ack_timeout_val = cpu_to_le32(qp->ack_timeout);
	p_ramrod->mtu = cpu_to_le16(qp->mtu);
	p_ramrod->initial_psn = cpu_to_le32(qp->sq_psn);
	p_ramrod->pd = cpu_to_le16(qp->pd);
	p_ramrod->sq_num_pages = cpu_to_le16(qp->sq_num_pages);
	DMA_REGPAIR_LE(p_ramrod->sq_pbl_addr, qp->sq_pbl_ptr);
	DMA_REGPAIR_LE(p_ramrod->orq_pbl_addr, qp->orq_phys_addr);
	qed_rdma_copy_gids(qp, p_ramrod->src_gid, p_ramrod->dst_gid);
	p_ramrod->qp_handle_for_async.hi = cpu_to_le32(qp->qp_handle_async.hi);
	p_ramrod->qp_handle_for_async.lo = cpu_to_le32(qp->qp_handle_async.lo);
	p_ramrod->qp_handle_for_cqe.hi = cpu_to_le32(qp->qp_handle.hi);
	p_ramrod->qp_handle_for_cqe.lo = cpu_to_le32(qp->qp_handle.lo);
	p_ramrod->cq_cid = cpu_to_le32((p_iov_info->opaque_fid << 16) | qp->
				       sq_cq_id);

	qed_roce_set_pqs(p_hwfn, qp, p_iov_info->is_vf,
			 p_iov_info->rel_vf_id,
			 &regular_latency_queue, &low_latency_queue);

	if (p_iov_info->is_vf == IOV_PF) {
		p_ramrod->vport_id = (u8) RESC_START(p_hwfn, QED_VPORT);
	} else {
		rc = qed_fw_vport(p_hwfn, p_iov_info->p_vf->vport_id,
				  &p_ramrod->vport_id);
		if (rc)
			goto err;
	}

	p_ramrod->regular_latency_phy_queue =
	    cpu_to_le16(regular_latency_queue);
	p_ramrod->low_latency_phy_queue = cpu_to_le16(low_latency_queue);

	p_ramrod->dpi = cpu_to_le16(qp->dpi);

	qed_rdma_set_fw_mac(p_ramrod->remote_mac_addr, qp->remote_mac_addr);
	qed_rdma_set_fw_mac(p_ramrod->local_mac_addr, qp->local_mac_addr);

	p_ramrod->udp_src_port = qp->udp_src_port;
	p_ramrod->vlan_id = cpu_to_le16(qp->vlan_id);
	p_ramrod->stats_counter_id = qp->stats_queue;

	qed_roce_pvrdma_create_requester(p_hwfn, qp, p_ramrod);

	rc = qed_spq_post(p_hwfn, p_ent, NULL);
	if (rc)
		goto err;

	qp->req_offloaded = true;
	qp->cq_prod.req = 0;

	cid_start = qed_cxt_get_proto_cid_start(p_hwfn,
						rdma_info->proto,
						p_iov_info->abs_vf_id);

	qed_roce_set_cid(p_hwfn, rdma_info, qp->icid + 1 - cid_start);

	return rc;

err:
	DP_NOTICE(p_hwfn, "Create requested - failed, rc = %d\n", rc);
	return rc;
}

static int qed_roce_sp_modify_responder(struct qed_hwfn *p_hwfn,
					struct qed_rdma_qp *qp,
					bool move_to_err,
					u32 modify_flags,
					struct qed_rdma_info *rdma_info)
{
	u16 regular_latency_phy_queue, low_latency_phy_queue;
	struct roce_modify_qp_resp_ramrod_data *p_ramrod;
	struct qed_sp_init_data init_data;
	struct qed_spq_entry *p_ent;
	int rc;

	if (!qp->has_resp)
		return 0;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "icid = %08x\n", qp->icid);

	if (move_to_err && !qp->resp_offloaded)
		return 0;

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = qp->icid;

	init_data.opaque_fid = rdma_info->iov_info.opaque_fid;

	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	rc = qed_sp_init_request(p_hwfn, &p_ent,
				 ROCE_EVENT_MODIFY_QP,
				 PROTOCOLID_ROCE, &init_data);
	if (rc) {
		DP_NOTICE(p_hwfn, "rc = %d\n", rc);
		return rc;
	}

	p_ramrod = &p_ent->ramrod.roce_modify_qp_resp;

	p_ramrod->flags = 0;

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_RESP_RAMROD_DATA_MOVE_TO_ERR_FLG, move_to_err);

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_RESP_RAMROD_DATA_RDMA_RD_EN,
		  qp->incoming_rdma_read_en);

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_RESP_RAMROD_DATA_RDMA_WR_EN,
		  qp->incoming_rdma_write_en);

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_RESP_RAMROD_DATA_ATOMIC_EN,
		  qp->incoming_atomic_en);

	SET_FIELD(p_ramrod->flags,
		  ROCE_CREATE_QP_RESP_RAMROD_DATA_E2E_FLOW_CONTROL_EN,
		  qp->e2e_flow_control_en);

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_RESP_RAMROD_DATA_RDMA_OPS_EN_FLG,
		  GET_FIELD(modify_flags,
			    QED_RDMA_MODIFY_QP_VALID_RDMA_OPS_EN));

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_RESP_RAMROD_DATA_P_KEY_FLG,
		  GET_FIELD(modify_flags, QED_ROCE_MODIFY_QP_VALID_PKEY));

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_RESP_RAMROD_DATA_ADDRESS_VECTOR_FLG,
		  GET_FIELD(modify_flags,
			    QED_ROCE_MODIFY_QP_VALID_ADDRESS_VECTOR));

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_RESP_RAMROD_DATA_MAX_IRD_FLG,
		  GET_FIELD(modify_flags,
			    QED_RDMA_MODIFY_QP_VALID_MAX_RD_ATOMIC_RESP));

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_RESP_RAMROD_DATA_FORCE_LB, qp->force_lb);

	/* TBD: future use only
	 * #define ROCE_MODIFY_QP_RESP_RAMROD_DATA_PRI_FLG_MASK
	 * #define ROCE_MODIFY_QP_RESP_RAMROD_DATA_PRI_FLG_SHIFT
	 */

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_RESP_RAMROD_DATA_MIN_RNR_NAK_TIMER_FLG,
		  GET_FIELD(modify_flags,
			    QED_ROCE_MODIFY_QP_VALID_MIN_RNR_NAK_TIMER));

	p_ramrod->fields = 0;
	SET_FIELD(p_ramrod->fields,
		  ROCE_MODIFY_QP_RESP_RAMROD_DATA_MIN_RNR_NAK_TIMER,
		  qp->min_rnr_nak_timer);

	if (GET_FIELD(modify_flags,
		      QED_RDMA_MODIFY_QP_VALID_PHYSICAL_QUEUES_FLG)) {
		qed_qm_acquire_access(p_hwfn);
		low_latency_phy_queue =
		    qed_get_cm_pq_idx_tc(p_hwfn, PQ_FLAGS_LLT,
					 qp->pq_set_id, qp->tc);
		p_ramrod->low_latency_phy_queue =
		    cpu_to_le16(low_latency_phy_queue);
		regular_latency_phy_queue =
		    qed_get_cm_pq_idx_tc(p_hwfn, PQ_FLAGS_OFLD,
					 qp->pq_set_id, qp->tc);
		p_ramrod->regular_latency_phy_queue =
		    cpu_to_le16(regular_latency_phy_queue);
		qed_qm_release_access(p_hwfn);

		SET_FIELD(p_ramrod->flags,
			  ROCE_MODIFY_QP_RESP_RAMROD_DATA_PHYSICAL_QUEUE_FLG,
			  1);
	}

	p_ramrod->max_ird = qp->max_rd_atomic_resp;
	p_ramrod->traffic_class = qp->traffic_class_tos;
	p_ramrod->hop_limit = qp->hop_limit_ttl;
	p_ramrod->p_key = cpu_to_le16(qp->pkey);
	p_ramrod->flow_label = cpu_to_le32(qp->flow_label);
	p_ramrod->mtu = cpu_to_le16(qp->mtu);
	qed_rdma_copy_gids(qp, p_ramrod->src_gid, p_ramrod->dst_gid);
	rc = qed_spq_post(p_hwfn, p_ent, NULL);

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Modify responder, rc = %d\n", rc);

	return rc;
}

static int qed_roce_sp_modify_requester(struct qed_hwfn *p_hwfn,
					struct qed_rdma_qp *qp,
					bool move_to_sqd,
					bool move_to_err,
					u32 modify_flags,
					struct qed_rdma_info *rdma_info)
{
	u16 regular_latency_phy_queue, low_latency_phy_queue;
	struct roce_modify_qp_req_ramrod_data *p_ramrod;
	struct qed_sp_init_data init_data;
	struct qed_spq_entry *p_ent;
	int rc;

	if (!qp->has_req)
		return 0;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "icid = %08x\n", qp->icid);

	if (move_to_err && !(qp->req_offloaded))
		return 0;

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = qp->icid + 1;

	init_data.opaque_fid = rdma_info->iov_info.opaque_fid;

	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	rc = qed_sp_init_request(p_hwfn, &p_ent,
				 ROCE_EVENT_MODIFY_QP,
				 PROTOCOLID_ROCE, &init_data);
	if (rc) {
		DP_NOTICE(p_hwfn, "rc = %d\n", rc);
		return rc;
	}

	p_ramrod = &p_ent->ramrod.roce_modify_qp_req;

	p_ramrod->flags = 0;

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_REQ_RAMROD_DATA_MOVE_TO_ERR_FLG, move_to_err);

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_REQ_RAMROD_DATA_MOVE_TO_SQD_FLG, move_to_sqd);

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_REQ_RAMROD_DATA_EN_SQD_ASYNC_NOTIFY,
		  qp->sqd_async);

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_REQ_RAMROD_DATA_P_KEY_FLG,
		  GET_FIELD(modify_flags, QED_ROCE_MODIFY_QP_VALID_PKEY));

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_REQ_RAMROD_DATA_ADDRESS_VECTOR_FLG,
		  GET_FIELD(modify_flags,
			    QED_ROCE_MODIFY_QP_VALID_ADDRESS_VECTOR));

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_REQ_RAMROD_DATA_MAX_ORD_FLG,
		  GET_FIELD(modify_flags,
			    QED_RDMA_MODIFY_QP_VALID_MAX_RD_ATOMIC_REQ));

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_REQ_RAMROD_DATA_RNR_NAK_CNT_FLG,
		  GET_FIELD(modify_flags,
			    QED_ROCE_MODIFY_QP_VALID_RNR_RETRY_CNT));

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_REQ_RAMROD_DATA_ERR_RETRY_CNT_FLG,
		  GET_FIELD(modify_flags, QED_ROCE_MODIFY_QP_VALID_RETRY_CNT));

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_REQ_RAMROD_DATA_ACK_TIMEOUT_FLG,
		  GET_FIELD(modify_flags,
			    QED_ROCE_MODIFY_QP_VALID_ACK_TIMEOUT));

	SET_FIELD(p_ramrod->flags,
		  ROCE_MODIFY_QP_REQ_RAMROD_DATA_FORCE_LB, qp->force_lb);

	if (GET_FIELD(modify_flags,
		      QED_RDMA_MODIFY_QP_VALID_PHYSICAL_QUEUES_FLG)) {
		qed_qm_acquire_access(p_hwfn);
		low_latency_phy_queue =
		    qed_get_cm_pq_idx_tc(p_hwfn, PQ_FLAGS_LLT,
					 qp->pq_set_id, qp->tc);
		p_ramrod->low_latency_phy_queue =
		    cpu_to_le16(low_latency_phy_queue);
		regular_latency_phy_queue =
		    qed_get_cm_pq_idx_tc(p_hwfn, PQ_FLAGS_OFLD,
					 qp->pq_set_id, qp->tc);
		p_ramrod->regular_latency_phy_queue =
		    cpu_to_le16(regular_latency_phy_queue);
		qed_qm_release_access(p_hwfn);

		SET_FIELD(p_ramrod->flags,
			  ROCE_MODIFY_QP_REQ_RAMROD_DATA_PHYSICAL_QUEUE_FLG, 1);
	}

	/* TBD: future use only
	 * #define ROCE_MODIFY_QP_REQ_RAMROD_DATA_PRI_FLG_MASK
	 * #define ROCE_MODIFY_QP_REQ_RAMROD_DATA_PRI_FLG_SHIFT
	 */

	p_ramrod->fields = 0;
	SET_FIELD(p_ramrod->fields,
		  ROCE_MODIFY_QP_REQ_RAMROD_DATA_ERR_RETRY_CNT, qp->retry_cnt);

	SET_FIELD(p_ramrod->fields,
		  ROCE_MODIFY_QP_REQ_RAMROD_DATA_RNR_NAK_CNT,
		  qp->rnr_retry_cnt);

	p_ramrod->max_ord = qp->max_rd_atomic_req;
	p_ramrod->traffic_class = qp->traffic_class_tos;
	p_ramrod->hop_limit = qp->hop_limit_ttl;
	p_ramrod->p_key = cpu_to_le16(qp->pkey);
	p_ramrod->flow_label = cpu_to_le32(qp->flow_label);
	p_ramrod->ack_timeout_val = cpu_to_le32(qp->ack_timeout);
	p_ramrod->mtu = cpu_to_le16(qp->mtu);
	qed_rdma_copy_gids(qp, p_ramrod->src_gid, p_ramrod->dst_gid);
	rc = qed_spq_post(p_hwfn, p_ent, NULL);

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Modify requester, rc = %d\n", rc);

	return rc;
}

static int qed_roce_sp_destroy_qp_responder(struct qed_hwfn *p_hwfn,
					    struct qed_rdma_qp *qp,
					    u32 * cq_prod,
					    struct qed_rdma_info *rdma_info)
{
	struct roce_destroy_qp_resp_output_params *p_ramrod_res;
	struct roce_destroy_qp_resp_ramrod_data *p_ramrod;
	struct qed_sp_init_data init_data;
	struct qed_spq_entry *p_ent;
	dma_addr_t ramrod_res_phys;
	int rc;

	if (!qp->has_resp) {
		*cq_prod = 0;
		return 0;
	}

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "icid = %08x\n", qp->icid);

	if (!qp->resp_offloaded) {
		*cq_prod = qp->cq_prod.resp;
		return 0;
	}

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = qp->icid;

	init_data.opaque_fid = rdma_info->iov_info.opaque_fid;

	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	rc = qed_sp_init_request(p_hwfn, &p_ent,
				 ROCE_RAMROD_DESTROY_QP,
				 PROTOCOLID_ROCE, &init_data);
	if (rc)
		return rc;

	p_ramrod = &p_ent->ramrod.roce_destroy_qp_resp;

	qed_roce_pvrdma_destroy_responder(p_hwfn, qp, p_ramrod);

	p_ramrod_res =
	    (struct roce_destroy_qp_resp_output_params *)
	    dma_alloc_coherent(&p_hwfn->cdev->pdev->dev, sizeof(*p_ramrod_res),
			       &ramrod_res_phys, GFP_KERNEL);

	if (!p_ramrod_res) {
		rc = -ENOMEM;
		DP_NOTICE(p_hwfn,
			  "qed destroy responder failed: cannot allocate memory (ramrod). rc = %d\n",
			  rc);
		qed_sp_destroy_request(p_hwfn, p_ent);
		return rc;
	}

	DMA_REGPAIR_LE(p_ramrod->output_params_addr, ramrod_res_phys);

	rc = qed_spq_post(p_hwfn, p_ent, NULL);
	if (rc)
		goto err;

	*cq_prod = le32_to_cpu(p_ramrod_res->cq_prod);
	qp->cq_prod.resp = *cq_prod;

	qp->resp_offloaded = false;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Destroy responder, rc = %d\n", rc);

	/* "fall through" */

err:	dma_free_coherent(&p_hwfn->cdev->pdev->dev,
			  sizeof(*p_ramrod_res),
			  p_ramrod_res, ramrod_res_phys);

	return rc;
}

static int qed_roce_sp_destroy_qp_requester(struct qed_hwfn *p_hwfn,
					    struct qed_rdma_qp *qp,
					    u32 * cq_prod,
					    struct qed_rdma_info *rdma_info)
{
	struct roce_destroy_qp_req_output_params *p_ramrod_res;
	struct roce_destroy_qp_req_ramrod_data *p_ramrod;
	struct qed_sp_init_data init_data;
	struct qed_spq_entry *p_ent;
	dma_addr_t ramrod_res_phys;
	int rc;

	if (!qp->has_req) {
		*cq_prod = 0;
		return 0;
	}

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "icid = %08x\n", qp->icid);

	if (!qp->req_offloaded) {
		*cq_prod = qp->cq_prod.req;
		return 0;
	}

	p_ramrod_res =
	    (struct roce_destroy_qp_req_output_params *)
	    dma_alloc_coherent(&p_hwfn->cdev->pdev->dev, sizeof(*p_ramrod_res),
			       &ramrod_res_phys, GFP_KERNEL);
	if (!p_ramrod_res) {
		DP_NOTICE(p_hwfn,
			  "qed destroy requester failed: cannot allocate memory (ramrod)\n");
		return -ENOMEM;
	}

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = qp->icid + 1;

	init_data.opaque_fid = rdma_info->iov_info.opaque_fid;

	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	rc = qed_sp_init_request(p_hwfn, &p_ent, ROCE_RAMROD_DESTROY_QP,
				 PROTOCOLID_ROCE, &init_data);
	if (rc)
		goto err;

	p_ramrod = &p_ent->ramrod.roce_destroy_qp_req;
	DMA_REGPAIR_LE(p_ramrod->output_params_addr, ramrod_res_phys);

	rc = qed_spq_post(p_hwfn, p_ent, NULL);
	if (rc)
		goto err;

	*cq_prod = le32_to_cpu(p_ramrod_res->cq_prod);
	qp->cq_prod.req = *cq_prod;

	qp->req_offloaded = false;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Destroy requester, rc = %d\n", rc);

	/* "fall through" */

err:	dma_free_coherent(&p_hwfn->cdev->pdev->dev,
			  sizeof(*p_ramrod_res),
			  p_ramrod_res, ramrod_res_phys);

	return rc;
}

static inline int qed_roce_sp_query_responder(struct qed_hwfn *p_hwfn,
					      struct qed_rdma_qp *qp,
					      struct
					      qed_rdma_query_qp_out_params
					      *out_params,
					      struct qed_rdma_info *rdma_info)
{
	struct roce_query_qp_resp_output_params *p_resp_ramrod_res;
	struct roce_query_qp_resp_ramrod_data *p_resp_ramrod;
	struct qed_sp_init_data init_data;
	dma_addr_t resp_ramrod_res_phys;
	struct qed_spq_entry *p_ent;
	int rc = 0;
	bool error_flag;

	if (!qp->resp_offloaded) {
		/* Don't send query qp for the responder */
		out_params->rq_psn = qp->rq_psn;

		return 0;
	}

	/* Send a query responder ramrod to the FW */
	p_resp_ramrod_res =
	    (struct roce_query_qp_resp_output_params *)
	    dma_alloc_coherent(&p_hwfn->cdev->pdev->dev,
			       sizeof(*p_resp_ramrod_res),
			       &resp_ramrod_res_phys, GFP_KERNEL);
	if (!p_resp_ramrod_res) {
		DP_NOTICE(p_hwfn,
			  "qed query qp failed: cannot allocate memory (ramrod)\n");
		return -ENOMEM;
	}

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = qp->icid;

	init_data.opaque_fid = rdma_info->iov_info.opaque_fid;

	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;
	rc = qed_sp_init_request(p_hwfn,
				 &p_ent,
				 ROCE_RAMROD_QUERY_QP,
				 PROTOCOLID_ROCE, &init_data);
	if (rc)
		goto err;

	p_resp_ramrod = &p_ent->ramrod.roce_query_qp_resp;
	DMA_REGPAIR_LE(p_resp_ramrod->output_params_addr, resp_ramrod_res_phys);

	rc = qed_spq_post(p_hwfn, p_ent, NULL);
	if (rc)
		goto err;

	out_params->rq_psn = le32_to_cpu(p_resp_ramrod_res->psn);
	error_flag = GET_FIELD(le32_to_cpu(p_resp_ramrod_res->flags),
			       ROCE_QUERY_QP_RESP_OUTPUT_PARAMS_ERROR_FLG);
	if (error_flag)
		qp->cur_state = QED_ROCE_QP_STATE_ERR;

err:	dma_free_coherent(&p_hwfn->cdev->pdev->dev,
			  sizeof(*p_resp_ramrod_res),
			  p_resp_ramrod_res, resp_ramrod_res_phys);

	return rc;
}

static inline int qed_roce_sp_query_requester(struct qed_hwfn *p_hwfn,
					      struct qed_rdma_qp *qp,
					      struct
					      qed_rdma_query_qp_out_params
					      *out_params, bool * sq_draining,
					      struct qed_rdma_info *rdma_info)
{
	struct roce_query_qp_req_output_params *p_req_ramrod_res;
	struct roce_query_qp_req_ramrod_data *p_req_ramrod;
	struct qed_sp_init_data init_data;
	dma_addr_t req_ramrod_res_phys;
	struct qed_spq_entry *p_ent;
	int rc = 0;
	bool error_flag;

	if (!qp->req_offloaded) {
		/* Don't send query qp for the requester */
		out_params->sq_psn = qp->sq_psn;
		out_params->draining = false;

		*sq_draining = 0;

		return 0;
	}

	/* Send a query requester ramrod to the FW */
	p_req_ramrod_res =
	    (struct roce_query_qp_req_output_params *)
	    dma_alloc_coherent(&p_hwfn->cdev->pdev->dev,
			       sizeof(*p_req_ramrod_res), &req_ramrod_res_phys,
			       GFP_KERNEL);
	if (!p_req_ramrod_res) {
		DP_NOTICE(p_hwfn,
			  "qed query qp failed: cannot allocate memory (ramrod). rc = %d\n",
			  rc);
		return -ENOMEM;
	}

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = qp->icid + 1;

	init_data.opaque_fid = rdma_info->iov_info.opaque_fid;

	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;
	rc = qed_sp_init_request(p_hwfn,
				 &p_ent,
				 ROCE_RAMROD_QUERY_QP,
				 PROTOCOLID_ROCE, &init_data);
	if (rc)
		goto err;

	p_req_ramrod = &p_ent->ramrod.roce_query_qp_req;
	DMA_REGPAIR_LE(p_req_ramrod->output_params_addr, req_ramrod_res_phys);

	rc = qed_spq_post(p_hwfn, p_ent, NULL);
	if (rc)
		goto err;

	out_params->sq_psn = le32_to_cpu(p_req_ramrod_res->psn);
	error_flag = GET_FIELD(le32_to_cpu(p_req_ramrod_res->flags),
			       ROCE_QUERY_QP_REQ_OUTPUT_PARAMS_ERR_FLG);
	if (error_flag)
		qp->cur_state = QED_ROCE_QP_STATE_ERR;
	else
		*sq_draining = GET_FIELD(le32_to_cpu(p_req_ramrod_res->flags),
					 ROCE_QUERY_QP_REQ_OUTPUT_PARAMS_SQ_DRAINING_FLG);

err:	dma_free_coherent(&p_hwfn->cdev->pdev->dev,
			  sizeof(*p_req_ramrod_res),
			  p_req_ramrod_res, req_ramrod_res_phys);

	return rc;
}

int qed_roce_query_qp(struct qed_hwfn *p_hwfn,
		      struct qed_rdma_qp *qp,
		      struct qed_rdma_query_qp_out_params *out_params,
		      struct qed_rdma_info *rdma_info)
{
	int rc;

	rc = qed_roce_sp_query_responder(p_hwfn, qp, out_params, rdma_info);
	if (rc)
		return rc;

	rc = qed_roce_sp_query_requester(p_hwfn, qp, out_params,
					 &out_params->draining, rdma_info);
	if (rc)
		return rc;

	out_params->state = qp->cur_state;

	return 0;
}

int
qed_roce_destroy_qp(struct qed_hwfn *p_hwfn,
		    struct qed_rdma_qp *qp,
		    struct qed_rdma_destroy_qp_out_params *out_params,
		    struct qed_rdma_info *rdma_info)
{
	u32 cq_prod_resp = qp->cq_prod.resp, cq_prod_req = qp->cq_prod.req;
	int rc;

	/* Destroys the specified QP
	 * Note: if qp state != RESET/ERR/INIT then upper driver first need to
	 * call modify qp to move the qp to ERR state
	 */
	if ((qp->cur_state != QED_ROCE_QP_STATE_RESET) &&
	    (qp->cur_state != QED_ROCE_QP_STATE_ERR) &&
	    (qp->cur_state != QED_ROCE_QP_STATE_INIT)) {
		DP_NOTICE(p_hwfn,
			  "QP must be in error, reset or init state before destroying it\n");
		return -EINVAL;
	}

	if (qp->cur_state != QED_ROCE_QP_STATE_RESET) {
		rc = qed_roce_sp_destroy_qp_responder(p_hwfn,
						      qp,
						      &cq_prod_resp, rdma_info);
		if (rc)
			return rc;

		/* Send destroy requester ramrod */
		rc = qed_roce_sp_destroy_qp_requester(p_hwfn, qp,
						      &cq_prod_req, rdma_info);
		if (rc)
			return rc;
	}

	qed_roce_free_qp(p_hwfn, qp->qp_idx, rdma_info);

	out_params->rq_cq_prod = cq_prod_resp;
	out_params->sq_cq_prod = cq_prod_req;

	return 0;
}

int qed_roce_destroy_ud_qp(void *rdma_cxt, u16 cid)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	struct qed_sp_init_data init_data;
	struct qed_spq_entry *p_ent;
	int rc;

	if (!rdma_cxt)
		return -EINVAL;

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = cid;
	init_data.opaque_fid = p_hwfn->hw_info.opaque_fid;
	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;
	rc = qed_sp_init_request(p_hwfn,
				 &p_ent,
				 ROCE_RAMROD_DESTROY_UD_QP,
				 PROTOCOLID_ROCE, &init_data);
	if (rc)
		goto err;

	rc = qed_spq_post(p_hwfn, p_ent, NULL);
	if (rc)
		goto err;

	qed_roce_free_qp(p_hwfn,
			 qed_roce_icid_to_qp_idx(p_hwfn, cid),
			 p_hwfn->p_rdma_info);

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "freed a ud qp with cid=%d\n", cid);

	return 0;

err:
	DP_ERR(p_hwfn, "failed destroying a ud qp with cid=%d\n", cid);

	return rc;
}

int qed_roce_create_ud_qp(void *rdma_cxt,
			  struct qed_rdma_create_qp_out_params *out_params)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)rdma_cxt;
	struct qed_sp_init_data init_data;
	struct qed_spq_entry *p_ent;
	int rc;
	u16 icid, qp_idx;

	if (!p_hwfn)
		return -EINVAL;

	if (!out_params) {
		DP_ERR(p_hwfn->cdev,
		       "qed roce create ud qp failed due to NULL entry (rdma_cxt=%p, out=%p)\n",
		       rdma_cxt, out_params);
		return -EINVAL;
	}

	rc = qed_roce_alloc_qp_idx(p_hwfn, &qp_idx, p_hwfn->p_rdma_info);
	if (rc)
		goto err;

	icid = qed_roce_qp_idx_to_icid(p_hwfn, qp_idx,
				       p_hwfn->p_rdma_info->iov_info.abs_vf_id);

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = icid;
	init_data.opaque_fid = p_hwfn->hw_info.opaque_fid;
	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;
	rc = qed_sp_init_request(p_hwfn,
				 &p_ent,
				 ROCE_RAMROD_CREATE_UD_QP,
				 PROTOCOLID_ROCE, &init_data);
	if (rc)
		goto err1;

	rc = qed_spq_post(p_hwfn, p_ent, NULL);
	if (rc)
		goto err1;

	out_params->icid = icid;
	out_params->qp_id = ((0xFF << 16) | icid);

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "created a ud qp with icid=%d\n",
		   icid);

	return 0;

err1:
	qed_roce_free_qp(p_hwfn, qp_idx, p_hwfn->p_rdma_info);

err:
	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "failed creating a ud qp\n");

	return rc;
}

int qed_roce_alloc_irq(struct qed_hwfn *p_hwfn, struct qed_rdma_qp *qp)
{
	/* return if IRQ has allocated already */
	if (qp->irq)
		return 0;

	/* Allocate DMA-able memory for IRQ */
	qp->irq_num_pages = 1;
	qp->irq = dma_alloc_coherent(&p_hwfn->cdev->pdev->dev,
				     RDMA_RING_PAGE_SIZE,
				     &qp->irq_phys_addr, GFP_KERNEL);
	if (!qp->irq) {
		DP_NOTICE(p_hwfn,
			  "qed create responder failed: cannot allocate memory (irq)\n");
		return -ENOMEM;
	}

	return 0;
}

void qed_roce_free_irq(struct qed_hwfn *p_hwfn, struct qed_rdma_qp *qp)
{
	/* return if IRQ in NULL */
	if (!qp->irq)
		return;

	dma_free_coherent(&p_hwfn->cdev->pdev->dev,
			  qp->irq_num_pages * RDMA_RING_PAGE_SIZE,
			  qp->irq, qp->irq_phys_addr);

	qp->irq = NULL;
}

int qed_roce_alloc_orq(struct qed_hwfn *p_hwfn, struct qed_rdma_qp *qp)
{
	/* return if ORQ has allocated already */
	if (qp->orq)
		return 0;

	/* Allocate DMA-able memory for ORQ */
	qp->orq_num_pages = 1;
	qp->orq = dma_alloc_coherent(&p_hwfn->cdev->pdev->dev,
				     RDMA_RING_PAGE_SIZE,
				     &qp->orq_phys_addr, GFP_KERNEL);
	if (!qp->orq) {
		DP_NOTICE(p_hwfn,
			  "qed create responder failed: cannot allocate memory (orq)\n");
		return -ENOMEM;
	}

	return 0;
}

void qed_roce_free_orq(struct qed_hwfn *p_hwfn, struct qed_rdma_qp *qp)
{
	/* return if ORQ in NULL */
	if (!qp->orq)
		return;

	dma_free_coherent(&p_hwfn->cdev->pdev->dev,
			  qp->orq_num_pages * RDMA_RING_PAGE_SIZE,
			  qp->orq, qp->orq_phys_addr);

	qp->orq = NULL;
}

int
qed_roce_modify_qp(struct qed_hwfn *p_hwfn,
		   struct qed_rdma_qp *qp,
		   enum qed_roce_qp_state prev_state,
		   struct qed_rdma_modify_qp_in_params *params,
		   struct qed_rdma_info *rdma_info)
{
	int rc = 0;

	/* Perform additional operations according to the current state and the
	 * next state
	 */
	if (((prev_state == QED_ROCE_QP_STATE_INIT) ||
	     (prev_state == QED_ROCE_QP_STATE_RESET)) &&
	    (qp->cur_state == QED_ROCE_QP_STATE_RTR)) {
		/* Init->RTR or Reset->RTR */

		/* Verify the cid bits that of this qp index are clear */
		rc = qed_roce_wait_free_cids(p_hwfn, qp->qp_idx, rdma_info);
		if (rc)
			return rc;

		rc = qed_roce_sp_create_responder(p_hwfn, qp, rdma_info);
		return rc;
	} else if ((prev_state == QED_ROCE_QP_STATE_RTR) &&
		   (qp->cur_state == QED_ROCE_QP_STATE_RTS)) {
		/* RTR-> RTS */
		rc = qed_roce_sp_create_requester(p_hwfn, qp, rdma_info);
		if (rc)
			return rc;

		/* Send modify responder ramrod */
		rc = qed_roce_sp_modify_responder(p_hwfn, qp, false,
						  params->modify_flags,
						  rdma_info);
		return rc;
	} else if ((prev_state == QED_ROCE_QP_STATE_RTS) &&
		   (qp->cur_state == QED_ROCE_QP_STATE_RTS)) {
		/* RTS->RTS */
		rc = qed_roce_sp_modify_responder(p_hwfn, qp, false,
						  params->modify_flags,
						  rdma_info);
		if (rc)
			return rc;

		rc = qed_roce_sp_modify_requester(p_hwfn, qp, false, false,
						  params->modify_flags,
						  rdma_info);
		return rc;
	} else if ((prev_state == QED_ROCE_QP_STATE_RTS) &&
		   (qp->cur_state == QED_ROCE_QP_STATE_SQD)) {
		/* RTS->SQD */
		rc = qed_roce_sp_modify_requester(p_hwfn, qp, true, false,
						  params->modify_flags,
						  rdma_info);
		return rc;
	} else if ((prev_state == QED_ROCE_QP_STATE_SQD) &&
		   (qp->cur_state == QED_ROCE_QP_STATE_SQD)) {
		/* SQD->SQD */
		rc = qed_roce_sp_modify_responder(p_hwfn, qp, false,
						  params->modify_flags,
						  rdma_info);
		if (rc)
			return rc;

		rc = qed_roce_sp_modify_requester(p_hwfn, qp, false, false,
						  params->modify_flags,
						  rdma_info);
		return rc;
	} else if ((prev_state == QED_ROCE_QP_STATE_SQD) &&
		   (qp->cur_state == QED_ROCE_QP_STATE_RTS)) {
		/* SQD->RTS */
		rc = qed_roce_sp_modify_responder(p_hwfn, qp, false,
						  params->modify_flags,
						  rdma_info);
		if (rc)
			return rc;

		rc = qed_roce_sp_modify_requester(p_hwfn, qp, false, false,
						  params->modify_flags,
						  rdma_info);

		return rc;
	} else if (qp->cur_state == QED_ROCE_QP_STATE_ERR) {
		/* ->ERR */
		rc = qed_roce_sp_modify_responder(p_hwfn, qp, true,
						  params->modify_flags,
						  rdma_info);
		if (rc)
			return rc;

		rc = qed_roce_sp_modify_requester(p_hwfn, qp, false, true,
						  params->modify_flags,
						  rdma_info);
		return rc;
	} else if (qp->cur_state == QED_ROCE_QP_STATE_RESET) {
		/* Any state -> RESET */

		/* Send destroy responder ramrod */
		rc = qed_roce_sp_destroy_qp_responder(p_hwfn, qp,
						      &qp->cq_prod.resp,
						      rdma_info);

		if (rc)
			return rc;

		rc = qed_roce_sp_destroy_qp_requester(p_hwfn, qp,
						      &qp->cq_prod.req,
						      rdma_info);

		if (rc)
			return rc;
	} else {
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "0\n");
	}

	return rc;
}

static void qed_roce_free_icid(struct qed_hwfn *p_hwfn, u16 icid, u8 abs_vf_id)
{
	struct qed_rdma_info *p_rdma_info;
	u8 rel_vf_id = QED_CXT_PF_CID;
	u32 start_cid, cid;

	if (QED_CXT_PF_CID == abs_vf_id) {
		p_rdma_info = p_hwfn->p_rdma_info;
	} else {
		rel_vf_id = qed_iov_abs_to_rel_id(p_hwfn, abs_vf_id);

		p_rdma_info =
		    p_hwfn->pf_iov_info->vfs_array[rel_vf_id].rdma_info;
	}

	if (!p_rdma_info) {
		DP_ERR(p_hwfn, "p_rdma_info is null. rel_vf_id = [%u]\n",
		       rel_vf_id);
		return;
	}

	start_cid = qed_cxt_get_proto_cid_start(p_hwfn, p_rdma_info->proto,
						abs_vf_id);
	cid = icid - start_cid;

	spin_lock_bh(&p_rdma_info->lock);

	qed_bmap_release_id(p_hwfn, &p_rdma_info->cid_map, cid);

	spin_unlock_bh(&p_rdma_info->lock);
}

static void qed_rdma_dpm_conf(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 val;

	val = qed_edpm_enabled(p_hwfn) ? 1 : 0;

	qed_wr(p_hwfn, p_ptt, DORQ_REG_PF_DPM_ENABLE, val);
	DP_VERBOSE(p_hwfn, (QED_MSG_DCB | QED_MSG_RDMA),
		   "Changing DPM_EN state to %d (DCBX=%d, DB_BAR=%d)\n",
		   val, p_hwfn->dcbx_no_edpm, p_hwfn->db_bar_no_edpm);
}

/* This function disables EDPM due to DCBx considerations */
void qed_roce_dpm_dcbx(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u8 val;

	/* if any QPs are already active, we want to disable DPM, since their
	 * context information contains information from before the latest DCBx
	 * update. Otherwise enable it.
	 */
	val = (qed_rdma_allocated_qps(p_hwfn)) ? true : false;
	p_hwfn->dcbx_no_edpm = (u8) val;

	qed_rdma_dpm_conf(p_hwfn, p_ptt);
}

/* This function disables EDPM due to doorbell bar considerations */
void qed_rdma_dpm_bar(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	p_hwfn->db_bar_no_edpm = true;

	qed_rdma_dpm_conf(p_hwfn, p_ptt);
}

int qed_roce_setup(struct qed_hwfn *p_hwfn)
{
	return qed_spq_register_async_cb(p_hwfn, PROTOCOLID_ROCE,
					 qed_roce_async_event);
}

u16 qed_roce_qp_idx_to_icid(struct qed_hwfn * p_hwfn, u16 qp_idx, u8 vf_id)
{
	enum protocol_type prot_type = p_hwfn->p_rdma_info->proto;

	return qed_cxt_get_proto_cid_start(p_hwfn, prot_type, vf_id) +
	    qp_idx * 2;
}

u16 qed_roce_icid_to_qp_idx(struct qed_hwfn * p_hwfn, u16 icid)
{
	enum protocol_type prot_type = p_hwfn->p_rdma_info->proto;

	return (icid - qed_cxt_get_proto_cid_start(p_hwfn, prot_type,
						   p_hwfn->
						   p_rdma_info->iov_info.
						   abs_vf_id)) / 2;
}
