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

#ifndef _QED_ROCE_PVRDMA_H
#define _QED_ROCE_PVRDMA_H
#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/slab.h>
#include "qed_rdma_if.h"

struct qed_roce_pvrdma_qp_info;
struct qed_ll2_comp_rx_data;
struct qed_rdma_start_in_params;
struct qed_rdma_create_qp_in_params;
struct roce_resp_qp_rdb_entry;
struct qed_rdma_create_qp_out_params;
struct qed_rdma_query_qp_out_params;
struct qed_rdma_qp;
struct qed_rdma_cnq_params;
struct qed_chain;

#if IS_ENABLED(CONFIG_QED_RDMA_PVRDMA)

struct qed_roce_pvrdma_create_suspended_qp_in_params {
	struct qed_rdma_create_qp_in_params *create_params;
	struct qed_rdma_modify_qp_in_params *modify_params;
	u32 send_msg_psn;
	u32 inflight_sends;
	u32 ssn;

	u32 receive_msg_psn;
	u32 inflight_receives;
	u32 rmsn;
	bool rdma_active;
	u32 rdma_key;
	struct regpair rdma_va;
	u32 rdma_length;
	u32 num_rdb_entries;
	void *rdb_entry_array;
};

struct qed_roce_pvrdma_create_suspended_qp_out_params {
	u32 qp_id;
	u16 icid;
	void *rq_pbl_virt;
	dma_addr_t rq_pbl_phys;
	void *sq_pbl_virt;
	dma_addr_t sq_pbl_phys;
};

struct qed_roce_pvrdma_query_suspended_qp_out_params {
	struct qed_rdma_query_qp_out_params *qp_params;
	u32 send_msg_psn;
	u32 inflight_sends;
	u32 ssn;
	u32 receive_msg_psn;
	u32 inflight_receives;
	u32 rmsn;
	bool rdma_active;
	u32 rdma_key;
	u64 rdma_va;
	u32 rdma_length;
	u32 num_rdb_entries;
	void *rdb_entry_array;
};

int
qed_roce_pvrdma_alloc_tid(void *rdma_cxt, u32 * itid, u8 find_new, u8 ns_id);

int
qed_roce_pvrdma_create_ud_qp(void *rdma_cxt,
			     struct qed_roce_pvrdma_qp_info *in_params,
			     struct qed_rdma_create_qp_out_params *out_params);

int
qed_roce_pvrdma_destroy_ud_qp(void *rdma_cxt,
			      struct qed_roce_pvrdma_qp_info *in_params,
			      u16 cid);

int qed_roce_pvrdma_suspend_qp(void *rdma_cxt, struct qed_rdma_qp *qp);

int qed_roce_pvrdma_query_suspended_qp(void *rdma_cxt, struct qed_rdma_qp
				       *qp, struct
				       qed_roce_pvrdma_query_suspended_qp_out_params
				       *out_params);

struct qed_rdma_qp *qed_roce_pvrdma_create_suspended_qp(void *rdma_cxt, struct
							qed_roce_pvrdma_create_suspended_qp_in_params
							*in_params, struct
							qed_roce_pvrdma_create_suspended_qp_out_params
							*out_params);

int qed_roce_pvrdma_resume_qp(void *rdma_cxt, struct qed_rdma_qp *qp);

int qed_roce_pvrdma_start_ns_tracking(void *rdma_cxt, u8 ns_id);

int qed_roce_pvrdma_stop_ns_tracking(void *rdma_cxt, u8 ns_id);

int qed_roce_pvrdma_flush_dpt(void *rdma_cxt, struct qed_rdma_qp *qp);

#endif

struct qed_roce_pvrdma_dptq_info {
	u8 enabled;
	void *drv_cookie;
	void (*drv_cb) (void *cxt);
	u8 sb_index;
	__le16 *p_fw_cons;
	u8 common_queue_offset;
	struct qed_chain *pbl;
};

struct qed_roce_pvrdma_info {
	u8 num_ns_log;
	struct qed_roce_pvrdma_dptq_info dptq;
};

#if IS_ENABLED(CONFIG_QED_RDMA_PVRDMA)

#define QED_ROCE_PVRDMA_RESP_SUSPENDED  0x1
#define QED_ROCE_PVRDMA_REQ_SUSPENDED           0x2

#define QED_ROCE_PVRDMA_ENABLED(_p_hwfn) \
	(_p_hwfn->p_rdma_info->roce.pvrdma_info.num_ns_log)

#define QED_ROCE_PVRDMA_MAX_MR_PER_NS(_p_hwfn) \
	BIT((RDMA_TID_LENGTH -		       \
	     _p_hwfn->p_rdma_info->roce.pvrdma_info.num_ns_log))

void qed_roce_pvrdma_async_event(struct qed_hwfn *p_hwfn,
				 union event_ring_data *data);
void qed_roce_pvrdma_stop(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
void qed_roce_pvrdma_init_hw(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
void qed_roce_pvrdma_init(struct qed_rdma_info *rdma_info,
			  struct qed_rdma_start_in_params *params);
void qed_roce_pvrdma_create_qp(struct qed_hwfn *p_hwfn,
			       struct qed_rdma_create_qp_in_params *in_params,
			       struct qed_rdma_qp *qp);
void qed_roce_pvrdma_config_mode(struct qed_hwfn *p_hwfn,
				 struct qed_rdma_start_in_params *in_params,
				 struct rdma_init_func_ramrod_data *p_ramrod);
void qed_roce_pvrdma_parse_gsi(union core_rx_cqe_union *p_cqe,
			       struct qed_ll2_comp_rx_data *data);
void
qed_roce_pvrdma_create_responder(struct qed_hwfn *p_hwfn,
				 struct qed_rdma_qp *qp,
				 struct roce_create_qp_resp_ramrod_data
				 *p_ramrod);
void qed_roce_pvrdma_destroy_responder(struct qed_hwfn *p_hwfn, struct qed_rdma_qp
				       *qp, struct roce_destroy_qp_resp_ramrod_data
				       *p_ramrod);
void
qed_roce_pvrdma_create_requester(struct qed_hwfn *p_hwfn,
				 struct qed_rdma_qp *qp,
				 struct roce_create_qp_req_ramrod_data
				 *p_ramrod);
int qed_roce_pvrdma_dptq_handler(struct qed_hwfn *p_hwfn, void *cookie);

#else

static inline void qed_roce_pvrdma_async_event(struct qed_hwfn __maybe_unused *
					       p_hwfn,
					       union event_ring_data
					       __maybe_unused * data)
{
}

static inline void qed_roce_pvrdma_stop(struct qed_hwfn __maybe_unused * p_hwfn,
					struct qed_ptt __maybe_unused * p_ptt)
{
}

static inline void qed_roce_pvrdma_init_hw(struct qed_hwfn __maybe_unused *
					   p_hwfn,
					   struct qed_ptt __maybe_unused *
					   p_ptt)
{
}

static inline void qed_roce_pvrdma_create_qp(struct qed_hwfn __maybe_unused *
					     p_hwfn, struct
					     qed_rdma_create_qp_in_params
					     __maybe_unused * in_params,
					     struct qed_rdma_qp __maybe_unused *
					     qp)
{
}

static inline void qed_roce_pvrdma_config_mode(struct qed_hwfn __maybe_unused *
					       p_hwfn,
					       struct qed_rdma_start_in_params
					       __maybe_unused * in_params,
					       struct
					       rdma_init_func_ramrod_data
					       __maybe_unused * p_ramrod)
{
}

static inline void qed_roce_pvrdma_parse_gsi(union core_rx_cqe_union
					     __maybe_unused * p_cqe,
					     struct qed_ll2_comp_rx_data
					     __maybe_unused * data)
{
}

static inline void qed_roce_pvrdma_create_responder(struct qed_hwfn
						    __maybe_unused * p_hwfn,
						    struct qed_rdma_qp
						    __maybe_unused * qp, struct
						    roce_create_qp_resp_ramrod_data
						    __maybe_unused * p_ramrod)
{
}

static inline void qed_roce_pvrdma_destroy_responder(struct qed_hwfn
						     __maybe_unused * p_hwfn,
						     struct qed_rdma_qp
						     __maybe_unused * qp, struct
						     roce_destroy_qp_resp_ramrod_data
						     __maybe_unused * p_ramrod)
{
}

static inline void qed_roce_pvrdma_create_requester(struct qed_hwfn
						    __maybe_unused * p_hwfn,
						    struct qed_rdma_qp
						    __maybe_unused * qp, struct
						    roce_create_qp_req_ramrod_data
						    __maybe_unused * p_ramrod)
{
}

static inline void qed_roce_pvrdma_init(struct qed_rdma_info __maybe_unused *
					rdma_info,
					struct qed_rdma_start_in_params
					__maybe_unused * params)
{
}
#endif /* CONFIG_QED_RDMA_PVRDMA */
#endif
