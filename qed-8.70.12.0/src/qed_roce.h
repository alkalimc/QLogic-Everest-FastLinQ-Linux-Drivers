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

#ifndef _QED_ROCE_H
#define _QED_ROCE_H
#include <linux/types.h>
#include <linux/slab.h>
#include "qed_roce_pvrdma.h"

/* functions for enabling/disabling edpm in rdma PFs according to existence of
 * qps during DCBx update or bar size
 */
void qed_roce_dpm_dcbx(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

void qed_rdma_dpm_bar(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int
qed_roce_dcqcn_cfg(struct qed_hwfn *p_hwfn,
		   struct qed_roce_dcqcn_params *params,
		   struct roce_init_func_ramrod_data *p_ramrod,
		   struct qed_ptt *p_ptt);

int qed_roce_setup(struct qed_hwfn *p_hwfn);

int qed_roce_stop_rl(struct qed_hwfn *p_hwfn);

int qed_roce_stop(struct qed_hwfn *p_hwfn, struct qed_rdma_info *rdma_info);

int
qed_roce_query_qp(struct qed_hwfn *p_hwfn,
		  struct qed_rdma_qp *qp,
		  struct qed_rdma_query_qp_out_params *out_params,
		  struct qed_rdma_info *rdma_info);

int
qed_roce_destroy_qp(struct qed_hwfn *p_hwfn,
		    struct qed_rdma_qp *qp,
		    struct qed_rdma_destroy_qp_out_params *out_params,
		    struct qed_rdma_info *rdma_info);

int
qed_roce_alloc_qp_idx(struct qed_hwfn *p_hwfn,
		      u16 * qp_idx16, struct qed_rdma_info *rdma_info);

int
qed_roce_reserve_qp_idx(struct qed_hwfn *p_hwfn,
			struct qed_rdma_info *rdma_info);

void
qed_roce_free_reserved_qp_idx(struct qed_hwfn *p_hwfn,
			      struct qed_rdma_info *rdma_info);

struct qed_roce_info {
	struct roce_events_stats event_stats;
	struct roce_dcqcn_received_stats dcqcn_rx_stats;
	struct roce_dcqcn_sent_stats dcqcn_tx_stats;
	struct roce_cqe_stats cqe_stats;
	struct roce_error_stats error_stats;
	u8 dcqcn_enabled;
	u8 dcqcn_reaction_point;
	struct qed_roce_pvrdma_info pvrdma_info;
};

int qed_roce_alloc_irq(struct qed_hwfn *p_hwfn, struct qed_rdma_qp *qp);
void qed_roce_free_irq(struct qed_hwfn *p_hwfn, struct qed_rdma_qp *qp);
int qed_roce_alloc_orq(struct qed_hwfn *p_hwfn, struct qed_rdma_qp *qp);
void qed_roce_free_orq(struct qed_hwfn *p_hwfn, struct qed_rdma_qp *qp);

int
qed_roce_modify_qp(struct qed_hwfn *p_hwfn,
		   struct qed_rdma_qp *qp,
		   enum qed_roce_qp_state prev_state,
		   struct qed_rdma_modify_qp_in_params *params,
		   struct qed_rdma_info *rdma_info);

u16 qed_roce_qp_idx_to_icid(struct qed_hwfn *p_hwfn, u16 qp_idx, u8 vf_id);
u16 qed_roce_icid_to_qp_idx(struct qed_hwfn *p_hwfn, u16 icid);

void qed_roce_free_qp(struct qed_hwfn *p_hwfn,
		      u16 qp_idx, struct qed_rdma_info *rdma_info);

enum roce_flavor qed_roce_mode_to_flavor(enum roce_mode roce_mode);
void qed_rdma_copy_gids(struct qed_rdma_qp *qp,
			__le32 * src_gid, __le32 * dst_gid);
u8 qed_roce_get_qp_tc(struct qed_hwfn *p_hwfn, struct qed_rdma_qp *qp);
void qed_roce_set_cid(struct qed_hwfn *p_hwfn,
		      struct qed_rdma_info *p_rdma_info, u32 cid);
#endif
