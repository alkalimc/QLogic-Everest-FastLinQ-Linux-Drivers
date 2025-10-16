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

#ifndef _QED_RDMA_H
#define _QED_RDMA_H
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/log2.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include "qed.h"
#include "qed_dev_api.h"
#include "qed_hsi.h"
#include "qed_if.h"
#include "qed_iwarp.h"
#include "qed_roce.h"
#include "qed_roce_pvrdma.h"
#include "qed_rdma_if.h"

struct qed_rdma_resize_cq_in_params {
	/* input variables (given by miniport) */

	u16 icid;
	u32 cq_size;
	bool pbl_two_level;
	u64 pbl_ptr;
	u16 pbl_num_pages;
	u8 pbl_page_size_log;	/* for the pages that contain the
				 * pointers to the CQ pages
				 */
};

struct qed_rdma_resize_cq_out_params {
	/* output variables, provided to the upper layer */
	u32 prod;		/* CQ producer value on old PBL */
	u32 cons;		/* CQ consumer value on old PBL */
};

struct qed_rdma_resize_cnq_in_params {
	/* input variables (given by miniport) */
	u32 cnq_id;
	u32 pbl_page_size_log;	/* for the pages that contain the
				 * pointers to the cnq pages
				 */
	u64 pbl_ptr;
};

int
qed_rdma_add_user(void *rdma_cxt,
		  struct qed_rdma_add_user_out_params *out_params);

int qed_rdma_alloc_pd(void *rdma_cxt, u16 * pd);

int qed_rdma_alloc_tid(void *rdma_cxt, u32 * tid);

int
qed_rdma_create_cq(void *rdma_cxt,
		   struct qed_rdma_create_cq_in_params *params, u16 * icid);

/* Returns a pointer to the responders' CID, which is also a pointer to the
 * qed_qp_params struct. Returns NULL in case of failure.
 */
struct qed_rdma_qp *qed_rdma_create_qp(void *rdma_cxt,
				       struct qed_rdma_create_qp_in_params
				       *in_params,
				       struct qed_rdma_create_qp_out_params
				       *out_params);

int
qed_roce_create_ud_qp(void *rdma_cxt,
		      struct qed_rdma_create_qp_out_params *out_params);

int qed_rdma_deregister_tid(void *rdma_cxt, u32 tid);

int
qed_rdma_destroy_cq(void *rdma_cxt,
		    struct qed_rdma_destroy_cq_in_params *in_params,
		    struct qed_rdma_destroy_cq_out_params *out_params);

int
qed_rdma_destroy_qp(void *rdma_cxt,
		    struct qed_rdma_qp *qp,
		    struct qed_rdma_destroy_qp_out_params *out_params);

int qed_roce_destroy_ud_qp(void *rdma_cxt, u16 cid);

void qed_rdma_free_pd(void *rdma_cxt, u16 pd);

void qed_rdma_free_tid(void *rdma_cxt, u32 tid);

int
qed_rdma_modify_qp(void *rdma_cxt,
		   struct qed_rdma_qp *qp,
		   struct qed_rdma_modify_qp_in_params *params);

struct qed_rdma_device *qed_rdma_query_device(void *rdma_cxt);

struct qed_rdma_port *qed_rdma_query_port(void *rdma_cxt);

int
qed_rdma_query_qp(void *rdma_cxt,
		  struct qed_rdma_qp *qp,
		  struct qed_rdma_query_qp_out_params *out_params);

int
qed_rdma_register_tid(void *rdma_cxt,
		      struct qed_rdma_register_tid_in_params *params);

void qed_rdma_remove_user(void *rdma_cxt, u16 dpi);

int
qed_rdma_resize_cnq(void *rdma_cxt,
		    struct qed_rdma_resize_cnq_in_params *in_params);

/*Returns the CQ CID or zero in case of failure */
int
qed_rdma_resize_cq(void *rdma_cxt,
		   struct qed_rdma_resize_cq_in_params *in_params,
		   struct qed_rdma_resize_cq_out_params *out_params);

/* Before calling rdma_start upper layer (VBD/qed) should fill the
 * page-size and mtu in hwfn context
 */
int qed_rdma_start(void *p_hwfn, struct qed_rdma_start_in_params *params);

int qed_rdma_stop(void *rdma_cxt);

int qed_rdma_get_stats_queue(void *rdma_cxt, u8 * stats_queue);

int
qed_rdma_query_stats(void *rdma_cxt,
		     u8 stats_queue,
		     struct qed_rdma_stats_out_params *out_parms);

int qed_rdma_reset_stats(void *rdma_cxt, u8 stats_queue);

int
qed_rdma_query_counters(void *rdma_cxt,
			struct qed_rdma_counters_out_params *out_parms);

u32 qed_rdma_get_sb_id(struct qed_hwfn *p_hwfn, u32 rel_sb_id);

void qed_rdma_cnq_prod_update(void *rdma_cxt, u8 cnq_index, u16 prod);

void qed_rdma_resc_free(struct qed_hwfn *p_hwfn,
			struct qed_rdma_info *rdma_info);

int
qed_rdma_create_srq(void *rdma_cxt,
		    struct qed_rdma_create_srq_in_params *in_params,
		    struct qed_rdma_create_srq_out_params *out_params);

int
qed_rdma_destroy_srq(void *rdma_cxt,
		     struct qed_rdma_destroy_srq_in_params *in_params);

int
qed_rdma_modify_srq(void *rdma_cxt,
		    struct qed_rdma_modify_srq_in_params *in_params);

#ifdef CONFIG_IWARP

/* iWARP API */

int
qed_iwarp_connect(void *rdma_cxt,
		  struct qed_iwarp_connect_in *iparams,
		  struct qed_iwarp_connect_out *oparams);

int
qed_iwarp_create_listen(void *rdma_cxt,
			struct qed_iwarp_listen_in *iparams,
			struct qed_iwarp_listen_out *oparams);

int qed_iwarp_accept(void *rdma_cxt, struct qed_iwarp_accept_in *iparams);

int qed_iwarp_reject(void *rdma_cxt, struct qed_iwarp_reject_in *iparams);

int qed_iwarp_destroy_listen(void *rdma_cxt, void *handle);

int qed_iwarp_send_rtr(void *rdma_cxt, struct qed_iwarp_send_rtr_in *iparams);

int qed_iwarp_pause_listen(void *rdma_cxt, void *handle, bool pause, bool comp);

#endif /* CONFIG_IWARP */

/* Constants */

/* HW/FW RoCE Limitations (internal. For external see qed_rdma_api.h) */
#define QED_RDMA_MAX_FMR                    (RDMA_MAX_TIDS)	/* 2^17 - 1 */
#define QED_RDMA_MAX_P_KEY                  (1)
#define QED_RDMA_MAX_WQE                    (0x7FFF)	/* 2^15 -1 */
#define QED_RDMA_MAX_SRQ_WQE_ELEM           (0x7FFF)	/* 2^15 -1 */
#define QED_RDMA_PAGE_SIZE_CAPS             (0x3FFFFF000ULL)	/* 4KB - 8GB */
#define QED_RDMA_ACK_DELAY                  (15)	/* 131 milliseconds */
#define QED_RDMA_MAX_MR_SIZE                (0x10000000000ULL)	/* 2^40 */
#define QED_RDMA_MAX_CQS                    (RDMA_MAX_CQS)	/* 64k */
#define QED_RDMA_MAX_MRS                    (RDMA_MAX_TIDS)	/* 2^17 - 1 */
/* Add 1 for header element */
#define QED_RDMA_MAX_SRQ_ELEM_PER_WQE         (RDMA_MAX_SGE_PER_RQ_WQE + 1)
#define QED_RDMA_MAX_SGE_PER_SRQ_WQE          (RDMA_MAX_SGE_PER_RQ_WQE)
#define QED_RDMA_SRQ_WQE_ELEM_SIZE          (16)
#define QED_RDMA_MAX_SRQS                     (32 * 1024)	/* 32k */
#define QED_RDMA_VF_MAX_CNQS                  (16)

/* Configurable */
/* Max CQE is derived from u16/32 size, halved and decremented by 1 to handle
 * wrap properly and then decremented by 1 again. The latter decrement comes
 * from a requirement to create a chain that is bigger than what the user
 * requested by one:
 * The CQE size is 32 bytes but the FW writes in chunks of 64
 * bytes, for performance purposes. Allocating an extra entry and telling the
 * FW we have less prevents overwriting the first entry in case of a wrap i.e.
 * when the FW writes the last entry and the application hasn't read the first
 * one.
 */
#define QED_RDMA_MAX_CQE_32_BIT             (0x7FFFFFFF - 1)
#define QED_RDMA_MAX_CQE_16_BIT             (0x7FFF - 1)

#define QED_RDMA_MAX_XRC_SRQS           (RDMA_MAX_XRC_SRQS)

/* Up to 2^16 XRC Domains are supported, but the actual number of supported XRC
 * SRQs is much smaller so there's no need to have that many domains.
 */
#define QED_RDMA_MAX_XRCDS      (roundup_pow_of_two(RDMA_MAX_XRC_SRQS))

#define IS_IWARP(_p_hwfn) (_p_hwfn->p_rdma_info->proto == PROTOCOLID_IWARP)
#define IS_ROCE(_p_hwfn) (_p_hwfn->p_rdma_info->proto == PROTOCOLID_ROCE)

enum qed_rdma_toggle_bit {
	QED_RDMA_TOGGLE_BIT_CLEAR = 0,
	QED_RDMA_TOGGLE_BIT_SET = 1
};

/* @@@TBD Currently we support only affilited events
 * enum qed_rdma_unaffiliated_event_code {
 * QED_RDMA_PORT_ACTIVE, // Link Up
 * QED_RDMA_PORT_CHANGED, // SGID table has changed
 * QED_RDMA_LOCAL_CATASTROPHIC_ERR, // Fatal device error
 * QED_RDMA_PORT_ERR, // Link down
 * };
 */

#define QEDR_MAX_BMAP_NAME      (10)
struct qed_bmap {
	u32 max_count;
	unsigned long *bitmap;
	char name[QEDR_MAX_BMAP_NAME];
};

enum qed_iov_is_vf_or_pf {
	IOV_PF = 0,		/* This is a PF instance. */
	IOV_VF = 1		/* This is a VF instance. */
};

struct qed_rdma_iov_info {
	struct qed_vf_info *p_vf;
	u8 abs_vf_id;
	u8 rel_vf_id;
	u16 opaque_fid;
	enum qed_iov_is_vf_or_pf is_vf;
};

struct qed_rdma_info {
	spinlock_t lock;

	struct qed_bmap cq_map;
	struct qed_bmap pd_map;
	struct qed_bmap xrcd_map;
	struct qed_bmap tid_map;
	struct qed_bmap srq_map;
	struct qed_bmap xrc_srq_map;
	struct qed_bmap qp_map;
	struct qed_bmap tcp_cid_map;
	struct qed_bmap cid_map;
	struct qed_bmap dpi_map;
	struct qed_bmap toggle_bits;
	struct qed_bmap sus_cid_map;
	struct qed_rdma_events events;
	struct qed_rdma_device *dev;
	struct qed_rdma_port *port;
	u32 last_tid;
	u8 num_cnqs;
	struct rdma_sent_stats rdma_sent_pstats;
	struct rdma_rcv_stats rdma_rcv_tstats;
	u32 num_qps;
	u32 num_mrs;
	u32 num_srqs;
	u16 srq_id_offset;
	u16 xrc_id_offset;
	bool xrc_supported;
	u16 queue_zone_base;
	u16 max_queue_zones;

	struct qed_rdma_glob_cfg glob_cfg;

	enum protocol_type proto;
	struct qed_roce_info roce;
#ifdef CONFIG_IWARP
	struct qed_iwarp_info iwarp;
#endif
	bool active;
	bool no_bmap_check;
	int ref_cnt;
	struct qed_rdma_iov_info iov_info;
	u32 drv_ver;
};

struct cq_prod {
	u32 req;
	u32 resp;
};

struct qed_rdma_qp {
	struct regpair qp_handle;
	struct regpair qp_handle_async;
	u32 qpid;		/* iwarp: may differ from icid */
	u16 icid;
	u16 qp_idx;
	enum qed_roce_qp_state cur_state;
	enum qed_rdma_qp_type qp_type;
#ifdef CONFIG_IWARP
	enum qed_iwarp_qp_state iwarp_state;
#endif
	bool use_srq;
	bool signal_all;
	bool fmr_and_reserved_lkey;

	bool incoming_rdma_read_en;
	bool incoming_rdma_write_en;
	bool incoming_atomic_en;
	bool e2e_flow_control_en;

	u16 pd;			/* Protection domain */
	u16 pkey;		/* Primary P_key index */
	u32 dest_qp;
	u16 mtu;
	u16 srq_id;
	u8 traffic_class_tos;	/* IPv6/GRH traffic class; IPv4 TOS */
	u8 hop_limit_ttl;	/* IPv6/GRH hop limit; IPv4 TTL */
	u16 dpi;
	u32 flow_label;		/* ignored in IPv4 */
	u16 vlan_id;
	u32 ack_timeout;
	u8 retry_cnt;
	u8 rnr_retry_cnt;
	u8 min_rnr_nak_timer;
	bool sqd_async;
	union qed_gid sgid;	/* GRH SGID; IPv4/6 Source IP */
	union qed_gid dgid;	/* GRH DGID; IPv4/6 Destination IP */
	enum roce_mode roce_mode;
	u16 udp_src_port;	/* RoCEv2 only */
	u8 stats_queue;

	/* requeseter */
	u8 max_rd_atomic_req;
	u32 sq_psn;
	u16 sq_cq_id;		/* The cq to be associated with the send queue */
	u16 sq_num_pages;
	dma_addr_t sq_pbl_ptr;
	void *orq;
	dma_addr_t orq_phys_addr;
	u8 orq_num_pages;
	bool req_offloaded;
	bool has_req;

	/* responder */
	u8 max_rd_atomic_resp;
	u32 rq_psn;
	u16 rq_cq_id;		/* The cq to be associated with the receive queue */
	u16 rq_num_pages;
	dma_addr_t rq_pbl_ptr;
	void *irq;
	dma_addr_t irq_phys_addr;
	u8 irq_num_pages;
	bool resp_offloaded;
	bool has_resp;
	struct cq_prod cq_prod;

	u8 remote_mac_addr[6];
	u8 local_mac_addr[6];

	void *shared_queue;
	dma_addr_t shared_queue_phys_addr;
#ifdef CONFIG_IWARP
	struct qed_iwarp_ep *ep;
#endif

	u16 xrcd_id;

	u16 pq_set_id;
	u8 tc;

	u8 edpm_mode;
	struct qed_roce_pvrdma_qp_info qp_ns_info;
	bool force_lb;
};

static inline bool qed_rdma_is_xrc_qp(struct qed_rdma_qp *qp)
{
	if ((qp->qp_type == QED_RDMA_QP_TYPE_XRC_TGT) ||
	    (qp->qp_type == QED_RDMA_QP_TYPE_XRC_INI))
		return 1;

	return 0;
}

int qed_rdma_info_alloc(struct qed_hwfn *p_hwfn);
void qed_rdma_info_free(struct qed_hwfn *p_hwfn);

void qed_rdma_free(struct qed_hwfn *p_hwfn, struct qed_rdma_info *rdma_info);

int
qed_rdma_bmap_alloc(struct qed_hwfn *p_hwfn,
		    struct qed_bmap *bmap, u32 max_count, char *name);

void
qed_rdma_bmap_free(struct qed_hwfn *p_hwfn, struct qed_bmap *bmap, bool check);

int
qed_rdma_bmap_alloc_id(struct qed_hwfn *p_hwfn,
		       struct qed_bmap *bmap, u32 * id_num);

void
qed_bmap_set_id(struct qed_hwfn *p_hwfn, struct qed_bmap *bmap, u32 id_num);

void
qed_bmap_release_id(struct qed_hwfn *p_hwfn, struct qed_bmap *bmap, u32 id_num);

int
qed_bmap_test_id(struct qed_hwfn *p_hwfn, struct qed_bmap *bmap, u32 id_num);

void qed_rdma_set_fw_mac(u16 * p_fw_mac, u8 * p_qed_mac);

bool qed_rdma_allocated_qps(struct qed_hwfn *p_hwfn);

u16 qed_rdma_get_fw_srq_id(struct qed_hwfn *p_hwfn, u16 id, bool is_xrc);

int qed_rdma_configure_prs(struct qed_hwfn *p_hwfn,
			   struct qed_rdma_info *rdma_info, u32 icid);

int qed_rdma_start_inner(struct qed_hwfn *p_hwfn,
			 struct qed_rdma_start_in_params *params,
			 struct qed_rdma_info *rdma_info);

int qed_rdma_stop_inner(struct qed_hwfn *p_hwfn,
			struct qed_rdma_info *rdma_info);

int qed_rdma_query_counters_inner(struct qed_hwfn *p_hwfn,
				  struct qed_rdma_counters_out_params
				  *out_params, struct qed_rdma_info *rdma_info);

int qed_rdma_alloc_tid_inner(struct qed_hwfn *p_hwfn,
			     u32 * itid, struct qed_rdma_info *rdma_info);

int qed_rdma_register_tid_inner(struct qed_hwfn *p_hwfn,
				struct qed_rdma_register_tid_in_params *params,
				struct qed_rdma_info *rdma_info);

int qed_rdma_deregister_tid_inner(struct qed_hwfn *p_hwfn,
				  u32 itid, struct qed_rdma_info *rdma_info);

void qed_rdma_free_tid_inner(struct qed_hwfn *p_hwfn,
			     u32 itid, struct qed_rdma_info *rdma_info);

int qed_rdma_create_cq_inner(struct qed_hwfn *p_hwfn,
			     struct qed_rdma_create_cq_in_params *params,
			     u16 * icid, struct qed_rdma_info *p_info);

int qed_rdma_resize_cq_inner(struct qed_hwfn *p_hwfn,
			     struct qed_rdma_resize_cq_in_params *in_params,
			     struct qed_rdma_resize_cq_out_params *out_paramsf,
			     struct qed_rdma_info *rdma_info);

int qed_rdma_destroy_cq_inner(struct qed_hwfn *p_hwfn,
			      struct qed_rdma_destroy_cq_in_params *in_params,
			      struct qed_rdma_destroy_cq_out_params *out_params,
			      struct qed_rdma_info *rdma_info);

struct qed_rdma_qp *qed_rdma_create_qp_inner(struct qed_hwfn *p_hwfn, struct
					     qed_rdma_create_qp_in_params
					     *in_params, struct
					     qed_rdma_create_qp_out_params
					     *out_params, struct qed_rdma_info
					     *rdma_info);

int qed_rdma_modify_qp_inner(struct qed_hwfn *p_hwfn,
			     struct qed_rdma_qp *qp,
			     struct qed_rdma_modify_qp_in_params *params,
			     struct qed_rdma_info *rdma_info);

int qed_rdma_query_qp_inner(struct qed_hwfn *p_hwfn,
			    struct qed_rdma_qp *qp,
			    struct qed_rdma_query_qp_out_params *out_params,
			    struct qed_rdma_info *rdma_info);

int qed_rdma_destroy_qp_inner(struct qed_hwfn *p_hwfn,
			      struct qed_rdma_qp *qp,
			      struct qed_rdma_destroy_qp_out_params *out_params,
			      struct qed_rdma_info *rdma_info);

int qed_rdma_create_srq_inner(struct qed_hwfn *p_hwfn,
			      struct qed_rdma_create_srq_in_params *in_params,
			      struct qed_rdma_create_srq_out_params *out_params,
			      struct qed_rdma_info *rdma_info);

int qed_rdma_modify_srq_inner(struct qed_hwfn *p_hwfn,
			      struct qed_rdma_modify_srq_in_params *in_params,
			      struct qed_rdma_info *rdma_info);

int qed_rdma_destroy_srq_inner(struct qed_hwfn *p_hwfn,
			       struct qed_rdma_destroy_srq_in_params *in_params,
			       struct qed_rdma_info *rdma_info);

struct qed_rdma_port *qed_rdma_query_port_inner(struct qed_hwfn *p_hwfn,
						struct qed_rdma_info
						*rdma_info);

void qed_rdma_get_guid(struct qed_hwfn *p_hwfn, u8 * guid);

u8 qed_rdma_get_start_cnq(struct qed_hwfn *p_hwfn,
			  struct qed_rdma_info *p_rdma_info);

u16 qed_rdma_get_queue_zone(struct qed_hwfn *p_hwfn,
			    struct qed_rdma_info *p_rdma_info, u8 cnq_id);

#endif
