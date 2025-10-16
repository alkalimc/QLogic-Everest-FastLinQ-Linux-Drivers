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

#ifndef _QED_LL2_H
#define _QED_LL2_H
#include <linux/types.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include "qed.h"
#include "qed_chain.h"
#include "qed_hsi.h"
#include "qed_roce_pvrdma.h"
#include "qed_sp.h"
#include "qed_ll2_if.h"

/* QED LL2 API: called by QED's upper level client  */
/* must be the asme as core_rx_conn_type */

#define QED_MAX_NUM_OF_LL2_CONNS_VF          (1)

/**
 * @brief qed_ll2_acquire_connection - allocate resources,
 *        starts rx & tx (if relevant) queues pair. Provides
 *        connecion handler as output parameter.
 *
 *
 * @param p_hwfn
 * @param data - describes connection parameters
 * @return int
 */
int qed_ll2_acquire_connection(void *cxt, struct qed_ll2_acquire_data *data);

/**
 * @brief qed_ll2_establish_connection - start previously
 *        allocated LL2 queues pair
 *
 * @param p_hwfn
 * @param p_ptt
 * @param connection_handle    LL2 connection's handle
 *                              obtained from
 *                              qed_ll2_require_connection
 *
 * @return int
 */
int qed_ll2_establish_connection(void *cxt, u8 connection_handle);

/**
 * @brief qed_ll2_post_rx_buffers - submit buffers to LL2 RxQ.
 *
 * @param p_hwfn
 * @param connection_handle    LL2 connection's handle
 *                              obtained from
 *                              qed_ll2_require_connection
 * @param addr                  rx (physical address) buffers to
 *                              submit
 * @param cookie
 * @param notify_fw             produce corresponding Rx BD
 *                              immediately
 *
 * @return int
 */
int qed_ll2_post_rx_buffer(void *cxt,
			   u8 connection_handle,
			   dma_addr_t addr,
			   u16 buf_len, void *cookie, u8 notify_fw);

/**
 * @brief qed_ll2_prepare_tx_packet - request for start Tx BD
 *        to prepare Tx packet submission to FW.
 *
 *
 * @param p_hwfn
 * @param pkt - info regarding the tx packet
 * @param notify_fw - issue doorbell to fw for this packet
 *
 * @return int
 */
int qed_ll2_prepare_tx_packet(void *cxt,
			      u8
			      connection_handle,
			      struct qed_ll2_tx_pkt_info *pkt, bool notify_fw);

/**
 * @brief qed_ll2_release_connection - releases resources
 *        allocated for LL2 connection
 *
 * @param p_hwfn
 * @param connection_handle    LL2 connection's handle
 *                              obtained from
 *                              qed_ll2_require_connection
 */
void qed_ll2_release_connection(void *cxt, u8 connection_handle);

/**
 * @brief qed_ll2_set_fragment_of_tx_packet - provides
 *        fragments to fill Tx BD of BDs requested by
 *        qed_ll2_prepare_tx_packet..
 *
 *
 * @param p_hwfn
 * @param connection_handle    LL2 connection's handle
 *                              obtained from
 *                              qed_ll2_require_connection
 * @param addr
 * @param nbytes
 *
 * @return int
 */
int
qed_ll2_set_fragment_of_tx_packet(void *cxt,
				  u8 connection_handle,
				  dma_addr_t addr, u16 nbytes);

/**
 * @brief qed_ll2_terminate_connection - stops Tx/Rx queues
 *
 *
 * @param p_hwfn
 * @param connection_handle    LL2 connection's handle
 *                              obtained from
 *                              qed_ll2_require_connection
 *
 * @return int
 */
int qed_ll2_terminate_connection(void *cxt, u8 connection_handle);

int __qed_ll2_get_stats(void *cxt,
			u8 connection_handle, struct qed_ll2_stats *p_stats);

/**
 * @brief qed_ll2_get_stats - get LL2 queue's statistics
 *
 *
 * @param p_hwfn
 * @param connection_handle    LL2 connection's handle
 *                              obtained from
 *                              qed_ll2_require_connection
 * @param p_stats
 *
 * @return int
 */
int qed_ll2_get_stats(void *cxt,
		      u8 connection_handle, struct qed_ll2_stats *p_stats);

/**
 * @brief qed_ll2_completion - handles ll2 txq/rxq completions (if any)
 *
 *
 * @param p_hwfn
 * @param connection_handle    LL2 connection's handle
 *                              obtained from
 *                              qed_ll2_require_connection
 *
 * @return int
 */
int qed_ll2_completion(void *cxt, u8 connection_handle);

/* QED LL2: internal structures and functions*/

/* LL2 queues handles will be split as follows:
 * first will be legacy queues, and then the ctx based queues.
 * for VF it will always start from 0 and will have its own allowed connections
 */
#define QED_MAX_NUM_OF_LL2_CONNS_PF            (4)
#define QED_MAX_NUM_OF_LEGACY_LL2_CONNS_PF   (3)

#define QED_MAX_NUM_OF_CTX_LL2_CONNS_PF	\
	(QED_MAX_NUM_OF_LL2_CONNS_PF - QED_MAX_NUM_OF_LEGACY_LL2_CONNS_PF)

#define QED_LL2_LEGACY_CONN_BASE_PF     0
#define QED_LL2_CTX_CONN_BASE_PF        QED_MAX_NUM_OF_LEGACY_LL2_CONNS_PF
#define QED_LL2_CONN_BASE_VF            QED_MAX_NUM_OF_CTX_LL2_CONNS_PF

#define QED_MAX_NUM_OF_LL2_CONNS(p_hwfn)		     \
	(IS_PF(p_hwfn->cdev) ? QED_MAX_NUM_OF_LL2_CONNS_PF : \
	 QED_MAX_NUM_OF_LL2_CONNS_VF)

#define QED_LL2_RX_REGISTERED(ll2)      ((ll2)->rx_queue.b_cb_registered)
#define QED_LL2_TX_REGISTERED(ll2)      ((ll2)->tx_queue.b_cb_registered)
#define QED_LL2_INVALID_QID             0xFF

static inline u8 qed_ll2_handle_to_queue_id(struct qed_hwfn *p_hwfn,
					    u8 handle,
					    u8 ll2_queue_type, u8 vf_id)
{
	u8 qid, start_range, end_range;

	if (ll2_queue_type == QED_LL2_RX_TYPE_LEGACY) {
		qid = RESC_START(p_hwfn, QED_LL2_RAM_QUEUE) + handle;
		start_range = qid;
		end_range = RESC_END(p_hwfn, QED_LL2_RAM_QUEUE);
	} else {
		/* QED_LL2_RX_TYPE_CTX
		 * FW distinguish between the legacy queus (ram based) and the
		 * ctx based queues by the queue_id.
		 * The first MAX_NUM_LL2_RX_RAM_QUEUES queues are legacy
		 * and the queue ids above that are ctx base.
		 */
		qid = RESC_START(p_hwfn, QED_LL2_CTX_QUEUE) +
		    MAX_NUM_LL2_RX_RAM_QUEUES;
		start_range = qid;
		/* See comment on th acquire conneciton for how the ll2
		 * queues handles are divided.
		 */
		if (vf_id == QED_CXT_PF_CID)
			qid += (handle - QED_MAX_NUM_OF_LEGACY_LL2_CONNS_PF);
		else
			qid += QED_LL2_CONN_BASE_VF + vf_id;

		end_range = RESC_END(p_hwfn, QED_LL2_CTX_QUEUE) +
		    MAX_NUM_LL2_RX_RAM_QUEUES;
	}

	if (qid >= end_range) {
		DP_ERR(p_hwfn, "ll2 qid %u is out of range [%u - %u]\n",
		       qid, start_range, end_range - 1);
		return QED_LL2_INVALID_QID;
	}

	return qid;
}

#define QED_LL2_INVALID_STATS_ID        0xff

static inline u8 qed_ll2_handle_to_stats_id(struct qed_hwfn *p_hwfn,
					    u8 ll2_queue_type, u8 qid)
{
	u8 stats_id;

	/* For legacy (RAM based) queues, the stats_id will be set as the
	 * queue_id. Otherwise (context based queue), it will be set to
	 * the "abs_pf_id" offset from the end of the RAM based queue IDs.
	 * If the final value exceeds the total counters amount, return
	 * INVALID value to indicate that the stats for this connection should
	 * be disabled.
	 */
	if (ll2_queue_type == QED_LL2_RX_TYPE_LEGACY)
		stats_id = qid;
	else
		stats_id = MAX_NUM_LL2_RX_RAM_QUEUES + p_hwfn->abs_pf_id;

	if (stats_id < MAX_NUM_LL2_TX_STATS_COUNTERS)
		return stats_id;
	else
		return QED_LL2_INVALID_STATS_ID;
}

struct qed_ll2_rx_packet {
	struct list_head list_entry;
	struct core_rx_bd_with_buff_len *rxq_bd;
	dma_addr_t rx_buf_addr;
	u16 buf_length;
	void *cookie;
	u8 placement_offset;
	u16 parse_flags;
	u16 packet_length;
	u16 vlan;
	u32 opaque_data[2];
};

struct qed_ll2_tx_packet {
	struct list_head list_entry;
	u16 bd_used;
	bool notify_fw;
	void *cookie;
	struct {
		struct core_tx_bd *txq_bd;
		dma_addr_t tx_frag;
		u16 frag_len;
	} bds_set[1];
	/* Flexible Array of bds_set determined by max_bds_per_packet */
};

struct qed_ll2_rx_queue {
	spinlock_t lock;
	struct qed_chain rxq_chain;
	struct qed_chain rcq_chain;
	u8 rx_sb_index;
	u8 ctx_based;
	bool b_cb_registered;
	bool b_started;
	__le16 *p_fw_cons;
	struct list_head active_descq;
	struct list_head free_descq;
	struct list_head posting_descq;
	struct qed_ll2_rx_packet *descq_array;

	/* Either RAM addr or doorbell (depends on qid) */
	void __iomem *set_prod_addr;
	struct core_pwm_prod_update_data db_data;
};

struct qed_ll2_tx_queue {
	spinlock_t lock;
	struct qed_chain txq_chain;
	u8 tx_sb_index;
	bool b_cb_registered;
	bool b_started;
	__le16 *p_fw_cons;
	struct list_head active_descq;
	struct list_head free_descq;
	struct list_head sending_descq;
	struct qed_ll2_tx_packet *descq_array;
	struct qed_ll2_tx_packet *cur_send_packet;
	struct qed_ll2_tx_packet cur_completing_packet;
	u16 cur_completing_bd_idx;
	void __iomem *doorbell_addr;
	struct core_db_data db_msg;
	u16 bds_idx;
	u16 cur_send_frag_num;
	u16 cur_completing_frag_num;
	bool b_completing_packet;
};

struct qed_ll2_info {
	struct mutex mutex;

	struct qed_ll2_acquire_data_inputs input;
	u32 cid;
	u8 my_id;
	u8 queue_id;
	u8 tx_stats_id;
	bool b_active;
	enum core_tx_dest tx_dest;
	u8 tx_stats_en;
	u8 main_func_queue;
	struct qed_ll2_rx_queue rx_queue;
	struct qed_ll2_tx_queue tx_queue;
	struct qed_ll2_cbs cbs;
};

struct qed_sp_ll2_rx_queue_start_params {
	u64 bd_base_addr;
	u64 cqe_pbl_addr;
	u32 cid;
	u16 opaque_fid;
	u16 sb_id;
	u16 mtu;
	u16 num_of_pbl_pages;
	u8 sb_index;
	u8 drop_ttl0_flg;
	u8 inner_vlan_stripping_en;
	u8 report_outer_vlan;
	u8 gsi_enable;
	u8 queue_id;
	u8 main_func_queue;
	u8 mf_si_bcast_accept_all;
	u8 mf_si_mcast_accept_all;
	u8 vport_id_valid;
	u8 vport_id;
	enum qed_ll2_error_handle err_packet_too_big;
	enum qed_ll2_error_handle err_no_buf;
};

struct qed_sp_ll2_tx_queue_start_params {
	u64 pbl_addr;
	u32 cid;
	u16 pbl_size;
	u16 pq_id;
	u16 opaque_fid;
	u16 sb_id;
	u16 mtu;
	u8 sb_index;
	u8 stats_en;
	u8 stats_id;
	u8 conn_type;
	u8 gsi_enable;
	u8 vport_id_valid;
	u8 vport_id;
};

struct qed_sp_ll2_tx_queue_update_params {
	u32 cid;
	u16 opaque_fid;
	u16 pq_id;
	u16 pq_set_id;
	u8 update_flag;
#define QED_UPDATE_LL2_TXQ_PQ           0x0001
#define QED_UPDATE_LL2_TXQ_PQ_SET       0x0002
	u8 tc;
};

struct qed_sp_ll2_rx_queue_stop_params {
	u32 cid;
	u16 opaque_fid;
	u8 queue_id;
};

struct qed_sp_ll2_tx_queue_stop_params {
	u32 cid;
	u16 opaque_fid;
};

/**
 * @brief qed_ll2_alloc - Allocates LL2 connections set
 *
 * @param p_hwfn
 *
 * @return int
 */
int qed_ll2_alloc(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_ll2_setup - Inits LL2 connections set
 *
 * @param p_hwfn
 *
 */
void qed_ll2_setup(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_ll2_free - Releases LL2 connections set
 *
 * @param p_hwfn
 *
 */
void qed_ll2_free(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_ll2_handle_sanity_lock - performs sanity on the connection
 *					 handle and return the corresponding
 *					 ll2_info
 *
 * @param p_hwfn
 * @param connection_handle    LL2 connection's handle
 *                              obtained from
 *                              qed_ll2_require_connection
 *
 * @return struct qed_ll2_info*
 */
struct qed_ll2_info *qed_ll2_handle_sanity_lock(struct qed_hwfn *p_hwfn,
						u8 connection_handle);

/**
 * @brief qed_sp_ll2_rx_queue_start - Sends RX_QUEUE_START ramrod to the FW
 *
 * @param p_hwfn
 * @param params    ll2_rx_queue_start_params
 *
 * @return int
 */
int
qed_sp_ll2_rx_queue_start(struct qed_hwfn *p_hwfn,
			  struct qed_sp_ll2_rx_queue_start_params *params);

/**
 * @brief qed_sp_ll2_tx_queue_start - Sends TX_QUEUE_START ramrod to the FW
 *
 * @param p_hwfn
 * @param params    ll2_tx_queue_start_params
 *
 * @return int
 */
int
qed_sp_ll2_tx_queue_start(struct qed_hwfn *p_hwfn,
			  struct qed_sp_ll2_tx_queue_start_params *params);

/**
 * @brief qed_sp_ll2_tx_queue_UPDATE - Sends TX_QUEUE_update ramrod to the FW
 *
 * @param p_hwfn
 * @param params    ll2_tx_queue_UPDATE_params
 *
 * @return int
 */
int
qed_sp_ll2_tx_queue_update(struct qed_hwfn *p_hwfn,
			   struct qed_sp_ll2_tx_queue_update_params *params);

/**
 * @brief qed_sp_ll2_tx_queue_stop - Sends TX_QUEUE_STOP ramrod to the FW
 *
 * @param p_hwfn
 * @param params    ll2_tx_queue_stop_params
 *
 * @return int
 */
int
qed_sp_ll2_tx_queue_stop(struct qed_hwfn *p_hwfn,
			 struct qed_sp_ll2_tx_queue_stop_params *params);

/**
 * @brief qed_sp_ll2_rx_queue_stop - Sends RX_QUEUE_STOP ramrod to the FW
 *
 * @param p_hwfn
 * @param params    ll2_rx_queue_stop_params
 *
 * @return int
 */
int
qed_sp_ll2_rx_queue_stop(struct qed_hwfn *p_hwfn,
			 struct qed_sp_ll2_rx_queue_stop_params *params);

#endif
