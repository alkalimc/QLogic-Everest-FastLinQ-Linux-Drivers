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

#ifndef _QED_SP_H
#define _QED_SP_H
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include "tcp_common.h"
#include "qed.h"
#include "qed_chain.h"
#include "qed_hsi.h"

enum spq_mode {
	QED_SPQ_MODE_BLOCK,	/* Client will poll a designated mem. address */
	QED_SPQ_MODE_CB,	/* Client supplies a callback */
	QED_SPQ_MODE_EBLOCK,	/* QED should block until completion */
};

struct qed_hwfn;
union event_ring_data;
struct eth_slow_path_rx_cqe;

struct qed_spq_comp_cb {
	void (*function) (struct qed_hwfn *,
			  void *, union event_ring_data *, u8 fw_return_code);
	void *cookie;
};

/**
 * @brief qed_eth_cqe_completion - handles the completion of a
 *        ramrod on the cqe ring
 *
 * @param p_hwfn
 * @param cqe
 *
 * @return int
 */
int qed_eth_cqe_completion(struct qed_hwfn *p_hwfn,
			   struct eth_slow_path_rx_cqe *cqe);
/**
 * @brief qed_sp_pf_update_tunn_cfg - PF Function Tunnel configuration
 *					update  Ramrod
 *
 * This ramrod is sent to update a tunneling configuration
 * for a physical function (PF).
 *
 * @param p_hwfn
 * @param p_ptt
 * @param p_tunn - pf update tunneling parameters
 * @param comp_mode - completion mode
 * @param p_comp_data - callback function
 *
 * @return int
 */

int
qed_sp_pf_update_tunn_cfg(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt,
			  struct qed_tunnel_info *p_tunn,
			  enum spq_mode comp_mode,
			  struct qed_spq_comp_cb *p_comp_data);

union ramrod_data {
	struct pf_start_ramrod_data pf_start;
	struct pf_update_ramrod_data pf_update;
	struct rl_update_ramrod_data rl_update;
	struct rx_queue_start_ramrod_data rx_queue_start;
	struct rx_queue_update_ramrod_data rx_queue_update;
	struct rx_queue_stop_ramrod_data rx_queue_stop;
	struct tx_queue_start_ramrod_data tx_queue_start;
	struct tx_queue_update_ramrod_data tx_queue_update;
	struct tx_queue_stop_ramrod_data tx_queue_stop;
	struct vport_start_ramrod_data vport_start;
	struct vport_stop_ramrod_data vport_stop;
	struct rx_update_gft_filter_ramrod_data rx_update_gft;
	struct gfs_add_filter_ramrod_data gfs_add_filter;
	struct gfs_del_filter_ramrod_data gfs_del_filter;
	struct vport_update_ramrod_data vport_update;
	struct core_rx_start_ramrod_data
	 core_rx_queue_start;
	struct core_rx_stop_ramrod_data
	 core_rx_queue_stop;
	struct core_tx_start_ramrod_data
	 core_tx_queue_start;
	struct core_tx_update_ramrod_data
	 core_tx_queue_update;
	struct core_tx_stop_ramrod_data
	 core_tx_queue_stop;
	struct vport_filter_update_ramrod_data
	 vport_filter_update;

	struct rdma_init_func_ramrod_data rdma_init_func;
	struct rdma_close_func_ramrod_data rdma_close_func;
	struct rdma_register_tid_ramrod_data
	 rdma_register_tid;
	struct rdma_deregister_tid_ramrod_data
	 rdma_deregister_tid;
	struct roce_create_qp_resp_ramrod_data
	 roce_create_qp_resp;
	struct roce_create_qp_req_ramrod_data
	 roce_create_qp_req;
	struct roce_modify_qp_resp_ramrod_data
	 roce_modify_qp_resp;
	struct roce_modify_qp_req_ramrod_data
	 roce_modify_qp_req;
	struct roce_query_qp_resp_ramrod_data
	 roce_query_qp_resp;
	struct roce_query_qp_req_ramrod_data
	 roce_query_qp_req;
	struct roce_destroy_qp_resp_ramrod_data
	 roce_destroy_qp_resp;
	struct roce_destroy_qp_req_ramrod_data
	 roce_destroy_qp_req;
	struct roce_create_ud_qp_ramrod_data
	 roce_create_ud_qp;
	struct roce_destroy_ud_qp_ramrod_data
	 roce_destroy_ud_qp;
	struct roce_query_suspended_qp_resp_ramrod_data
	 roce_query_susp_qp_resp;
	struct roce_query_suspended_qp_req_ramrod_data
	 roce_query_susp_qp_req;
	struct roce_create_suspended_qp_resp_ramrod_data
	 roce_create_susp_qp_resp;
	struct roce_create_suspended_qp_req_ramrod_data
	 roce_create_susp_qp_req;
	struct roce_init_func_ramrod_data roce_init_func;

	struct rdma_create_cq_ramrod_data rdma_create_cq;
	struct rdma_resize_cq_ramrod_data rdma_resize_cq;
	struct rdma_destroy_cq_ramrod_data rdma_destroy_cq;
	struct rdma_srq_create_ramrod_data rdma_create_srq;
	struct rdma_srq_destroy_ramrod_data
	 rdma_destroy_srq;
	struct rdma_srq_modify_ramrod_data rdma_modify_srq;
	struct rdma_namespace_tracking_ramrod_data
	 roce_pvrdma_ns_track;

	struct iwarp_create_qp_ramrod_data iwarp_create_qp;
	struct iwarp_tcp_offload_ramrod_data
	 iwarp_tcp_offload;
	struct iwarp_mpa_offload_ramrod_data
	 iwarp_mpa_offload;
	struct iwarp_modify_qp_ramrod_data iwarp_modify_qp;
	struct iwarp_query_qp_ramrod_data iwarp_query_qp;
	struct iwarp_init_func_ramrod_data iwarp_init_func;

	struct fcoe_init_ramrod_params fcoe_init;
	struct fcoe_conn_offload_ramrod_params fcoe_conn_ofld;
	struct fcoe_conn_terminate_ramrod_params
	 fcoe_conn_terminate;
	struct fcoe_stat_ramrod_params fcoe_stat;

	struct iscsi_init_ramrod_params iscsi_init;
	struct iscsi_spe_conn_offload
	 iscsi_conn_offload;
	struct iscsi_conn_update_ramrod_params
	 iscsi_conn_update;
	struct iscsi_spe_conn_mac_update
	 iscsi_conn_mac_update;
	struct iscsi_spe_conn_termination
	 iscsi_conn_terminate;
	struct iscsi_spe_conn_statistics
	 iscsi_conn_statistics;

	struct vf_start_ramrod_data vf_start;
	struct vf_stop_ramrod_data vf_stop;
};

#define EQ_MAX_CREDIT   0xffffffff

enum spq_priority {
	QED_SPQ_PRIORITY_NORMAL,
	QED_SPQ_PRIORITY_HIGH,
};

union qed_spq_req_comp {
	struct qed_spq_comp_cb cb;
	u64 *done_addr;
};

/* SPQ_MODE_EBLOCK */
struct qed_spq_comp_done {
	u64 done;
	u8 fw_return_code;
};

struct qed_spq_entry {
	struct list_head list;

	u8 flags;

	/* HSI slow path element */
	struct slow_path_element elem;

	union ramrod_data ramrod;

	enum spq_priority priority;

	/* pending queue for this entry */
	struct list_head *queue;

	enum spq_mode comp_mode;
	struct qed_spq_comp_cb comp_cb;
	struct qed_spq_comp_done comp_done;	/* SPQ_MODE_EBLOCK */

	/* Posted entry for unlimited list entry in EBLOCK mode */
	struct qed_spq_entry *post_ent;
};

#define QED_EQ_MAX_ELEMENTS     0xFF00

struct qed_eq {
	struct qed_chain chain;
	u8 eq_sb_index;		/* index within the SB */
	__le16 *p_fw_cons;	/* ptr to index value */
};

struct qed_consq {
	struct qed_chain chain;
};

typedef int
 (*qed_spq_async_comp_cb) (struct qed_hwfn * p_hwfn,
			   u8 opcode,
			   u16 echo,
			   union event_ring_data * data,
			   u8 fw_return_code, u8 vf_id);

int
qed_spq_register_async_cb(struct qed_hwfn *p_hwfn,
			  enum protocol_type protocol_id,
			  qed_spq_async_comp_cb cb);

void
qed_spq_unregister_async_cb(struct qed_hwfn *p_hwfn,
			    enum protocol_type protocol_id);

struct qed_spq {
	spinlock_t lock;

	struct list_head unlimited_pending;
	struct list_head pending;
	struct list_head completion_pending;
	struct list_head free_pool;

	struct qed_chain chain;

	/* allocated dma-able memory for spq entries (+ramrod data) */
	dma_addr_t p_phys;
	struct qed_spq_entry *p_virt;

	/* SPQ max sleep iterations used in __qed_spq_block() */
	u32 block_sleep_max_iter;

	/* Bitmap for handling out-of-order completions */
#define SPQ_RING_SIZE \
	(CORE_SPQE_PAGE_SIZE_BYTES / sizeof(struct slow_path_element))
#define SPQ_COMP_BMAP_SIZE      (SPQ_RING_SIZE / BITS_PER_LONG)
	unsigned long p_comp_bitmap[SPQ_COMP_BMAP_SIZE];
	u8 comp_bitmap_idx;
#define SPQ_COMP_BMAP_SET_BIT(p_spq, idx)	   \
	do {					   \
		__set_bit(((idx) % SPQ_RING_SIZE), \
			  (p_spq)->p_comp_bitmap); \
	} while (0)
#define SPQ_COMP_BMAP_CLEAR_BIT(p_spq, idx)				    \
	do {								    \
		clear_bit(((idx) % SPQ_RING_SIZE), (p_spq)->p_comp_bitmap); \
	} while (0)
#define SPQ_COMP_BMAP_TEST_BIT(p_spq, idx) \
	(test_bit(((idx) % SPQ_RING_SIZE), (p_spq)->p_comp_bitmap))

	/* Statistics */
	u32 unlimited_pending_count;
	u32 normal_count;
	u32 high_count;
	u32 comp_sent_count;
	u32 comp_count;

	u32 cid;

	u32 db_addr_offset;
	struct core_db_data db_data;
	qed_spq_async_comp_cb async_comp_cb[MAX_PROTOCOL_TYPE];

#ifndef QED_UPSTREAM
	bool drop_next_completion;
#endif
};

struct qed_port;
struct qed_hwfn;

/**
 * @brief qed_set_spq_block_timeout - calculates the maximum sleep
 *        iterations used in __qed_spq_block();
 *
 * @param p_hwfn
 * @param spq_timeout_ms
 */
void qed_set_spq_block_timeout(struct qed_hwfn *p_hwfn, u32 spq_timeout_ms);

/**
 * @brief qed_spq_post - Posts a Slow hwfn request to FW, or lacking that
 *        Pends it to the future list.
 *
 * @param p_hwfn
 * @param p_req
 *
 * @return int
 */
int qed_spq_post(struct qed_hwfn *p_hwfn,
		 struct qed_spq_entry *p_ent, u8 * fw_return_code);

/**
 * @brief qed_spq_allocate - Alloocates & initializes the SPQ and EQ.
 *
 * @param p_hwfn
 *
 * @return int
 */
int qed_spq_alloc(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_spq_setup - Reset the SPQ to its start state.
 *
 * @param p_hwfn
 */
void qed_spq_setup(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_spq_deallocate - Deallocates the given SPQ struct.
 *
 * @param p_hwfn
 */
void qed_spq_free(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_spq_get_entry - Obtain an entrry from the spq
 *        free pool list.
 *
 *
 *
 * @param p_hwfn
 * @param pp_ent
 *
 * @return int
 */
int qed_spq_get_entry(struct qed_hwfn *p_hwfn, struct qed_spq_entry **pp_ent);

/**
 * @brief qed_spq_return_entry - Return an entry to spq free
 *                                 pool list
 *
 * @param p_hwfn
 * @param p_ent
 */
void qed_spq_return_entry(struct qed_hwfn *p_hwfn, struct qed_spq_entry *p_ent);
/**
 * @brief qed_eq_allocate - Allocates & initializes an EQ struct
 *
 * @param p_hwfn
 * @param num_elem number of elements in the eq
 *
 * @return int
 */
int qed_eq_alloc(struct qed_hwfn *p_hwfn, u16 num_elem);

/**
 * @brief qed_eq_setup - Reset the EQ to its start state.
 *
 * @param p_hwfn
 */
void qed_eq_setup(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_eq_free - deallocates the given EQ struct.
 *
 * @param p_hwfn
 */
void qed_eq_free(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_eq_prod_update - update the FW with default EQ producer
 *
 * @param p_hwfn
 * @param prod
 */
void qed_eq_prod_update(struct qed_hwfn *p_hwfn, u16 prod);

/**
 * @brief qed_eq_completion - Completes currently pending EQ elements
 *
 * @param p_hwfn
 * @param cookie
 *
 * @return int
 */
int qed_eq_completion(struct qed_hwfn *p_hwfn, void *cookie);

/**
 * @brief qed_spq_completion - Completes a single event
 *
 * @param p_hwfn
 * @param echo - echo value from cookie (used for determining completion)
 * @param p_data - data from cookie (used in callback function if applicable)
 *
 * @return int
 */
int qed_spq_completion(struct qed_hwfn *p_hwfn,
		       __le16 echo,
		       u8 fw_return_code, union event_ring_data *p_data);

#ifndef QED_UPSTREAM
/**
 * @brief qed_spq_drop_next_completion - Next SPQ completion will be ignored
 *
 * @param p_hwfn
 */
void qed_spq_drop_next_completion(struct qed_hwfn *p_hwfn);
#endif

/**
 * @brief qed_spq_get_cid - Given p_hwfn, return cid for the hwfn's SPQ
 *
 * @param p_hwfn
 *
 * @return u32 - SPQ CID
 */
u32 qed_spq_get_cid(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_consq_alloc - Allocates & initializes an ConsQ struct
 *
 * @param p_hwfn
 *
 * @return int
 */
int qed_consq_alloc(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_consq_setup - Reset the ConsQ to its start state.
 *
 * @param p_hwfn
 */
void qed_consq_setup(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_consq_free - deallocates the given ConsQ struct.
 *
 * @param p_hwfn
 */
void qed_consq_free(struct qed_hwfn *p_hwfn);
int qed_spq_pend_post(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_event_ring_entry_dp - print ring entry info
 *
 * @param p_hwfn
 * @param p_eqe
 */
void qed_event_ring_entry_dp(struct qed_hwfn *p_hwfn,
			     struct event_ring_entry *p_eqe);

/**
 * @brief qed_async_event_completion - handle EQ completions
 *
 * @param p_hwfn
 * @param p_eqe
 */
int
qed_async_event_completion(struct qed_hwfn *p_hwfn,
			   struct event_ring_entry *p_eqe);

#define QED_SP_EQ_COMPLETION  0x01
#define QED_SP_CQE_COMPLETION 0x02

struct qed_sp_init_data {
	/* The CID and FID aren't necessarily derived from hwfn,
	 * e.g., in IOV scenarios. CID might defer between SPQ and
	 * other elements.
	 */
	u32 cid;
	u16 opaque_fid;

	/* Information regarding operation upon sending & completion */
	enum spq_mode comp_mode;
	struct qed_spq_comp_cb *p_comp_data;
};

/**
 * @brief Returns a SPQ entry to the pool / frees the entry if allocated
 *        Should be called on in error flows after initializing the SPQ entry
 *        and before posting it.
 *
 * @param p_hwfn
 * @param p_ent
 */
void qed_sp_destroy_request(struct qed_hwfn *p_hwfn,
			    struct qed_spq_entry *p_ent);

/**
 * @brief Acquire and initialize and SPQ entry for a given ramrod.
 *
 * @param p_hwfn
 * @param pp_ent - will be filled with a pointer to an entry upon success
 * @param cmd - dependent upon protocol
 * @param protocol
 * @param p_data - various configuration required for ramrod
 *
 * @return 0 upon success, otherwise failure.
 */
int qed_sp_init_request(struct qed_hwfn *p_hwfn,
			struct qed_spq_entry **pp_ent,
			u8 cmd, u8 protocol, struct qed_sp_init_data *p_data);

/**
 * @brief qed_sp_pf_start - PF Function Start Ramrod
 *
 * This ramrod is sent to initialize a physical function (PF). It will
 * configure the function related parameters and write its completion to the
 * event ring specified in the parameters.
 *
 * Ramrods complete on the common event ring for the PF. This ring is
 * allocated by the driver on host memory and its parameters are written
 * to the internal RAM of the UStorm by the Function Start Ramrod.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param p_tunn - pf start tunneling configuration
 * @param allow_npar_tx_switch - npar tx switching to be used
 *	  for vports configured for tx-switching.
 *
 * @return int
 */

int qed_sp_pf_start(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    struct qed_tunnel_info *p_tunn, bool allow_npar_tx_switch);

/**
 * @brief qed_sp_pf_update - PF Function Update Ramrod
 *
 * This ramrod updates function-related parameters. Every parameter can be
 * updated independently, according to configuration flags.
 *
 * @note Final phase API.
 *
 * @param p_hwfn
 *
 * @return int
 */

int qed_sp_pf_update_dcbx(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_sp_pf_stop - PF Function Stop Ramrod
 *
 * This ramrod is sent to close a Physical Function (PF). It is the last ramrod
 * sent and the last completion written to the PFs Event Ring. This ramrod also
 * deletes the context for the Slowhwfn connection on this PF.
 *
 * @note Not required for first packet.
 *
 * @param p_hwfn
 *
 * @return int
 */

int qed_sp_pf_stop(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_sp_heartbeat_ramrod - Send empty Ramrod
 *
 * @param p_hwfn
 *
 * @return int
 */

int qed_sp_heartbeat_ramrod(struct qed_hwfn *p_hwfn);

struct qed_rl_update_params {
	u8 qcn_update_param_flg;
	u8 dcqcn_update_param_flg;
	u8 rl_init_flg;
	u8 rl_start_flg;
	u8 rl_stop_flg;
	u8 rl_id_first;
	u8 rl_id_last;
	u8 rl_dc_qcn_flg;	/* If set, RL will used for DCQCN */
	u32 rl_bc_rate;		/* Byte Counter Limit */
	u32 rl_max_rate;	/* Maximum rate in Mbps resolution */
	u32 rl_r_ai;		/* Active increase rate */
	u32 rl_r_hai;		/* Hyper active increase rate */
	u32 dcqcn_gd;		/* DCQCN Alpha update gain */
	u32 dcqcn_k_us;		/* DCQCN Alpha update interval */
	u32 dcqcn_timeuot_us;
	u32 qcn_timeuot_us;
};

/**
 * @brief qed_sp_rl_update - Update rate limiters
 *
 * @param p_hwfn
 * @param params
 *
 * @return int
 */
int qed_sp_rl_update(struct qed_hwfn *p_hwfn,
		     struct qed_rl_update_params *params);

/**
 * @brief qed_sp_pf_update_stag - PF STAG value update Ramrod
 *
 * @param p_hwfn
 *
 * @return int
 */

int qed_sp_pf_update_stag(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_sp_pf_update_ufp - PF ufp update Ramrod
 *
 * @param p_hwfn
 *
 * @return int
 */
int qed_sp_pf_update_ufp(struct qed_hwfn *p_hwfn);

#endif
