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

#ifndef _QED_L2_H
#define _QED_L2_H
#include <linux/types.h>
#include <linux/if_ether.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include "qed.h"
#include "qed_hw.h"
#include "qed_int.h"
#include "qed_sp.h"
#include "qed_eth_if.h"
#include "qed_if.h"

struct qed_rss_params {
	u8 update_rss_config;
	u8 rss_enable;
	u8 rss_eng_id;
	u8 update_rss_capabilities;
	u8 update_rss_ind_table;
	u8 update_rss_key;
	u8 rss_caps;
	u8 rss_table_size_log;	/* The table size is 2 ^ rss_table_size_log */

	/* Indirection table consist of rx queue handles */
	void *rss_ind_table[QED_RSS_IND_TABLE_SIZE];
	u32 rss_key[QED_RSS_KEY_SIZE];
};

struct qed_sge_tpa_params {
	u8 max_buffers_per_cqe;

	u8 update_tpa_en_flg;
	u8 tpa_ipv4_en_flg;
	u8 tpa_ipv6_en_flg;
	u8 tpa_ipv4_tunn_en_flg;
	u8 tpa_ipv6_tunn_en_flg;

	u8 update_tpa_param_flg;
	u8 tpa_pkt_split_flg;
	u8 tpa_hdr_data_split_flg;
	u8 tpa_gro_consistent_flg;
	u8 tpa_max_aggs_num;
	u16 tpa_max_size;
	u16 tpa_min_size_to_start;
	u16 tpa_min_size_to_cont;
};

enum qed_filter_opcode {
	QED_FILTER_ADD,
	QED_FILTER_REMOVE,
	QED_FILTER_MOVE,
	QED_FILTER_REPLACE,	/* Delete all MACs and add new one instead */
	QED_FILTER_FLUSH,	/* Removes all filters */
};

enum qed_filter_ucast_type {
	QED_FILTER_MAC,
	QED_FILTER_VLAN,
	QED_FILTER_MAC_VLAN,
	QED_FILTER_INNER_MAC,
	QED_FILTER_INNER_VLAN,
	QED_FILTER_INNER_PAIR,
	QED_FILTER_INNER_MAC_VNI_PAIR,
	QED_FILTER_MAC_VNI_PAIR,
	QED_FILTER_VNI,
};

struct qed_filter_ucast {
	enum qed_filter_opcode opcode;
	enum qed_filter_ucast_type type;
	u8 is_rx_filter;
	u8 is_tx_filter;
	u8 vport_to_add_to;
	u8 vport_to_remove_from;
	unsigned char mac[ETH_ALEN];
	u8 assert_on_error;
	u16 vlan;
	u32 vni;
};

struct qed_filter_mcast {
	/* MOVE is not supported for multicast */
	enum qed_filter_opcode opcode;
	u8 vport_to_add_to;
	u8 vport_to_remove_from;
	u8 num_mc_addrs;
#define QED_MAX_MC_ADDRS        64
	unsigned char mac[QED_MAX_MC_ADDRS][ETH_ALEN];
};

struct qed_filter_accept_flags {
	u8 update_rx_mode_config;
	u8 update_tx_mode_config;
	u8 rx_accept_filter;
	u8 tx_accept_filter;
#define QED_ACCEPT_NONE         0x01
#define QED_ACCEPT_UCAST_MATCHED        0x02
#define QED_ACCEPT_UCAST_UNMATCHED      0x04
#define QED_ACCEPT_MCAST_MATCHED        0x08
#define QED_ACCEPT_MCAST_UNMATCHED      0x10
#define QED_ACCEPT_BCAST                0x20
#define QED_ACCEPT_ANY_VNI              0x40
};

struct qed_arfs_config_params {
	bool tcp;
	bool udp;
	bool ipv4;
	bool ipv6;
	enum qed_filter_config_mode mode;
};

/* Add / remove / move / remove-all unicast MAC-VLAN filters.
 * FW will assert in the following cases, so driver should take care...:
 * 1. Adding a filter to a full table.
 * 2. Adding a filter which already exists on that vport.
 * 3. Removing a filter which doesn't exist.
 */

int
qed_filter_ucast_cmd(struct qed_dev *cdev,
		     struct qed_filter_ucast *p_filter_cmd,
		     enum spq_mode comp_mode,
		     struct qed_spq_comp_cb *p_comp_data);

/* Add / remove / move multicast MAC filters. */
int
qed_filter_mcast_cmd(struct qed_dev *cdev,
		     struct qed_filter_mcast *p_filter_cmd,
		     enum spq_mode comp_mode,
		     struct qed_spq_comp_cb *p_comp_data);

/* Set "accept" filters */
int
qed_filter_accept_cmd(struct qed_dev *cdev,
		      u8 vport,
		      struct qed_filter_accept_flags accept_flags,
		      u8 update_accept_any_vlan,
		      u8 accept_any_vlan,
		      enum spq_mode comp_mode,
		      struct qed_spq_comp_cb *p_comp_data);

/**
 * @brief qed_eth_rx_queue_start - RX Queue Start Ramrod
 *
 * This ramrod initializes an RX Queue for a VPort. An Assert is generated if
 * the VPort ID is not currently initialized.
 *
 * @param p_hwfn
 * @param opaque_fid
 * @p_params			Inputs; Relative for PF [SB being an exception]
 * @param bd_max_bytes          Maximum bytes that can be placed on a BD
 * @param bd_chain_phys_addr	Physical address of BDs for receive.
 * @param cqe_pbl_addr		Physical address of the CQE PBL Table.
 * @param cqe_pbl_size          Size of the CQE PBL Table
 * @param p_ret_params		Pointed struct to be filled with outputs.
 *
 * @return int
 */
int
qed_eth_rx_queue_start(struct qed_hwfn *p_hwfn,
		       u16 opaque_fid,
		       struct qed_queue_start_common_params *p_params,
		       u16 bd_max_bytes,
		       dma_addr_t
		       bd_chain_phys_addr,
		       dma_addr_t cqe_pbl_addr,
		       u16 cqe_pbl_size,
		       struct qed_rxq_start_ret_params *p_ret_params);

/**
 * @brief qed_eth_rx_queue_stop - This ramrod closes an Rx queue
 *
 * @param p_hwfn
 * @param p_rxq			Handler of queue to close
 * @param eq_completion_only	If True completion will be on
 *				EQe, if False completion will be
 *				on EQe if p_hwfn opaque
 *				different from the RXQ opaque
 *				otherwise on CQe.
 * @param cqe_completion	If True completion will be
 *				recieve on CQe.
 * @return int
 */
int
qed_eth_rx_queue_stop(struct qed_hwfn *p_hwfn,
		      void *p_rxq,
		      bool eq_completion_only, bool cqe_completion);

/**
 * @brief - TX Queue Start Ramrod
 *
 * This ramrod initializes a TX Queue for a VPort. An Assert is generated if
 * the VPort is not currently initialized.
 *
 * @param p_hwfn
 * @param opaque_fid
 * @p_params
 * @param tc			traffic class to use with this L2 txq
 * @param pbl_addr		address of the pbl array
 * @param pbl_size              number of entries in pbl
 * @oaram p_ret_params		Pointer to fill the return parameters in.
 *
 * @return int
 */
int
qed_eth_tx_queue_start(struct qed_hwfn *p_hwfn,
		       u16 opaque_fid,
		       struct qed_queue_start_common_params *p_params,
		       u8 tc,
		       dma_addr_t pbl_addr,
		       u16 pbl_size,
		       struct qed_txq_start_ret_params *p_ret_params);

/**
 * @brief qed_eth_tx_queue_update - update a Tx queue
 *
 * @param p_hwfn
 * @param p_txq - handle to Tx queue needed to be closed
 *
 * @return int
 */
int
qed_eth_tx_queue_update(struct qed_hwfn *p_hwfn,
			void *p_txq,
			struct qed_queue_update_common_params *p_params,
			enum spq_mode comp_mode,
			struct qed_spq_comp_cb *p_comp_data);

/**
 * @brief qed_eth_tx_queue_stop - closes a Tx queue
 *
 * @param p_hwfn
 * @param p_txq - handle to Tx queue needed to be closed
 *
 * @return int
 */
int qed_eth_tx_queue_stop(struct qed_hwfn *p_hwfn, void *p_txq);

enum qed_tpa_mode {
	QED_TPA_MODE_NONE,
	QED_TPA_MODE_RSC,
	QED_TPA_MODE_GRO,
	QED_TPA_MODE_MAX
};

struct qed_sp_vport_start_params {
	enum qed_tpa_mode tpa_mode;
	bool remove_inner_vlan;	/* Inner VLAN removal is enabled */
	bool tx_switching;	/* Vport supports tx-switching */
	bool handle_ptp_pkts;	/* Handle PTP packets */
	bool only_untagged;	/* Untagged pkt control */
	bool drop_ttl0;		/* Drop packets with TTL = 0 */
	u8 max_buffers_per_cqe;
	u32 concrete_fid;
	u16 opaque_fid;
	u8 vport_id;		/* VPORT ID */
	u16 mtu;		/* VPORT MTU */
	bool zero_placement_offset;
	bool check_mac;
	bool check_ethtype;

	/* Strict behavior on transmission errors */
	bool b_err_illegal_vlan_mode;
	bool b_err_illegal_inband_mode;
	bool b_err_vlan_insert_with_inband;
	bool b_err_small_pkt;
	bool b_err_big_pkt;
	bool b_err_anti_spoof;
	bool b_err_ctrl_frame;
	bool b_en_rgfs;
	bool b_en_tgfs;
};

/**
 * @brief qed_sp_vport_start -
 *
 * This ramrod initializes a VPort. An Assert if generated if the Function ID
 * of the VPort is not enabled.
 *
 * @param p_hwfn
 * @param p_params		VPORT start params
 *
 * @return int
 */
int
qed_sp_vport_start(struct qed_hwfn *p_hwfn,
		   struct qed_sp_vport_start_params *p_params);

struct qed_sp_vport_update_params {
	u16 opaque_fid;
	u8 vport_id;
	u8 update_vport_active_rx_flg;
	u8 vport_active_rx_flg;
	u8 update_vport_active_tx_flg;
	u8 vport_active_tx_flg;
	u8 update_inner_vlan_removal_flg;
	u8 inner_vlan_removal_flg;
	u8 silent_vlan_removal_flg;
	u8 update_default_vlan_enable_flg;
	u8 default_vlan_enable_flg;
	u8 update_default_vlan_flg;
	u16 default_vlan;
	u8 update_tx_switching_flg;
	u8 tx_switching_flg;
	u8 update_approx_mcast_flg;
	u8 update_anti_spoofing_en_flg;
	u8 anti_spoofing_en;
	u8 update_accept_any_vlan_flg;
	u8 accept_any_vlan;
	u32 bins[8];
	struct qed_rss_params *rss_params;
	struct qed_filter_accept_flags accept_flags;
	struct qed_sge_tpa_params *sge_tpa_params;

	/* MTU change - notice this requires the vport to be disabled.
	 * If non-zero, value would be used.
	 */
	u16 mtu;
	u8 update_ctl_frame_check;
	u8 mac_chk_en;
	u8 ethtype_chk_en;
};

/**
 * @brief qed_sp_vport_update -
 *
 * This ramrod updates the parameters of the VPort. Every field can be updated
 * independently, according to flags.
 *
 * This ramrod is also used to set the VPort state to active after creation.
 * An Assert is generated if the VPort does not contain an RX queue.
 *
 * @param p_hwfn
 * @param p_params
 *
 * @return int
 */
int
qed_sp_vport_update(struct qed_hwfn *p_hwfn,
		    struct qed_sp_vport_update_params *p_params,
		    enum spq_mode comp_mode,
		    struct qed_spq_comp_cb *p_comp_data);
/**
 * @brief qed_sp_vport_stop -
 *
 * This ramrod closes a VPort after all its RX and TX queues are terminated.
 * An Assert is generated if any queues are left open.
 *
 * @param p_hwfn
 * @param opaque_fid
 * @param vport_id VPort ID
 *
 * @return int
 */
int qed_sp_vport_stop(struct qed_hwfn *p_hwfn, u16 opaque_fid, u8 vport_id);

int
qed_sp_eth_filter_ucast(struct qed_hwfn *p_hwfn,
			u16 opaque_fid,
			struct qed_filter_ucast *p_filter_cmd,
			enum spq_mode comp_mode,
			struct qed_spq_comp_cb *p_comp_data);

/**
 * @brief qed_sp_rx_eth_queues_update -
 *
 * This ramrod updates an RX queue. It is used for setting the active state
 * of the queue and updating the TPA and SGE parameters.
 *
 * @note Final phase API.
 *
 * @param p_hwfn
 * @param pp_rxq_handlers	An array of queue handlers to be updated.
 * @param num_rxqs              number of queues to update.
 * @param complete_cqe_flg	Post completion to the CQE Ring if set
 * @param complete_event_flg	Post completion to the Event Ring if set
 * @param comp_mode
 * @param p_comp_data
 *
 * @return int
 */

int
qed_sp_eth_rx_queues_update(struct qed_hwfn *p_hwfn,
			    void **pp_rxq_handlers,
			    u8 num_rxqs,
			    u8 complete_cqe_flg,
			    u8 complete_event_flg,
			    enum spq_mode comp_mode,
			    struct qed_spq_comp_cb *p_comp_data);

/**
 * @brief qed_sp_eth_rx_queues_set_default -
 *
 * This ramrod sets RSS RX queue as default one.
 *
 * @note Final phase API.
 *
 * @param p_hwfn
 * @param p_rxq_handlers	queue handlers to be updated.
 * @param comp_mode
 * @param p_comp_data
 *
 * @return int
 */

int
qed_sp_eth_rx_queues_set_default(struct qed_hwfn *p_hwfn,
				 void *p_rxq_handler,
				 enum spq_mode comp_mode,
				 struct qed_spq_comp_cb *p_comp_data);

void __qed_get_vport_stats(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt,
			   struct qed_eth_stats *stats,
			   u16 statistics_bin, bool b_get_port_stats);

void qed_get_vport_stats(struct qed_dev *cdev, struct qed_eth_stats *stats);

void qed_reset_vport_stats(struct qed_dev *cdev);

/**
 * *@brief qed_arfs_mode_configure -
 *
 **Enable or disable rfs mode. It must accept atleast one of tcp or udp true
 **and atleast one of ipv4 or ipv6 true to enable rfs mode.
 *
 **@param p_hwfn
 **@param p_ptt
 **@param p_cfg_params		arfs mode configuration parameters.
 *
 */
void qed_arfs_mode_configure(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt,
			     struct qed_arfs_config_params *p_cfg_params);

/**
 * @brief - qed_configure_rfs_ntuple_filter
 *
 * This ramrod should be used to add or remove arfs hw filter
 *
 * @params p_hwfn
 * @params p_cb		Used for QED_SPQ_MODE_CB,where client would initialize
 *			it with cookie and callback function address, if not
 *			using this mode then client must pass NULL.
 * @params p_params
 */
int
qed_configure_rfs_ntuple_filter(struct qed_hwfn *p_hwfn,
				struct qed_spq_comp_cb *p_cb,
				struct qed_ntuple_filter_params *p_params);

/**
 * @brief - qed_update_eth_rss_ind_table_entry
 *
 * This function being used to update RSS indirection table entry to FW RAM
 * instead of using the SP vport update ramrod with rss params.
 *
 * Notice:
 * This function supports only one outstanding command per engine. QED
 * clients which use this function should call qed_mcp_ind_table_lock() prior
 * to it and qed_mcp_ind_table_unlock() after it.
 *
 * @params p_hwfn
 * @params vport_id
 * @params ind_table_index
 * @params ind_table_value
 *
 * @return int
 */
int
qed_update_eth_rss_ind_table_entry(struct qed_hwfn *p_hwfn,
				   u8 vport_id,
				   u8 ind_table_index, u16 ind_table_value);

/**
 * @brief - qed_set_vf_stats_bin_id
 *
 * Set stats bin id of a given vf
 *
 * @params cdev
 * @params vf_id	VF number
 *
 * @return int
 */

int qed_set_vf_stats_bin_id(struct qed_dev *cdev, u16 vf_id);

#define MAX_QUEUES_PER_QZONE    (sizeof(unsigned long) * 8)
#define QED_QUEUE_CID_PF        (0xff)

/* Almost identical to the qed_queue_start_common_params,
 * but here we maintain the SB index in IGU CAM.
 */
struct qed_queue_cid_params {
	u8 vport_id;
	u16 queue_id;
	u8 stats_id;
};

/* Additional parameters required for initialization of the queue_cid
 * and are relevant only for a PF initializing one for its VFs.
 */
struct qed_queue_cid_vf_params {
	/* Should match the VF's relative index */
	u8 vfid;

	/* 0-based queue index. Should reflect the relative qzone the
	 * VF thinks is associated with it [in its range].
	 */
	u8 vf_qid;

	/* Indicates a VF is legacy, making it differ in several things:
	 *  - Producers would be placed in a different place.
	 *  - Makes assumptions regarding the CIDs.
	 */
	u8 vf_legacy;

	/* For VFs, this index arrives via TLV to diffrentiate between
	 * different queues opened on the same qzone, and is passed
	 * [where the PF would have allocated it internally for its own].
	 */
	u8 qid_usage_idx;
};

struct qed_queue_cid {
	/* For stats-id, the `rel' is actually absolute as well */
	struct qed_queue_cid_params rel;
	struct qed_queue_cid_params abs;

	/* These have no 'relative' meaning */
	u16 sb_igu_id;
	u8 sb_idx;

	u32 cid;
	u16 opaque_fid;

	bool b_is_rx;

	/* VFs queues are mapped differently, so we need to know the
	 * relative queue associated with them [0-based].
	 * Notice this is relevant on the *PF* queue-cid of its VF's queues,
	 * and not on the VF itself.
	 */
	u8 vfid;
	u8 vf_qid;

	/* We need an additional index to diffrentiate between queues opened
	 * for same queue-zone, as VFs would have to communicate the info
	 * to the PF [otherwise PF has no way to diffrentiate].
	 */
	u8 qid_usage_idx;

	/* Legacy VFs might have Rx producer located elsewhere */
	u8 vf_legacy;
#define QED_QCID_LEGACY_VF_RX_PROD      (1 << 0)
#define QED_QCID_LEGACY_VF_CID  (1 << 1)

	struct qed_hwfn *p_owner;
};

int qed_l2_alloc(struct qed_hwfn *p_hwfn);
void qed_l2_setup(struct qed_hwfn *p_hwfn);
void qed_l2_free(struct qed_hwfn *p_hwfn);

void qed_eth_queue_cid_release(struct qed_hwfn *p_hwfn,
			       struct qed_queue_cid *p_cid);

struct qed_queue_cid *qed_eth_queue_to_cid(struct qed_hwfn *p_hwfn,
					   u16 opaque_fid,
					   struct qed_queue_start_common_params
					   *p_params, bool b_is_rx,
					   struct qed_queue_cid_vf_params
					   *p_vf_params);

int
qed_sp_eth_vport_start(struct qed_hwfn *p_hwfn,
		       struct qed_sp_vport_start_params *p_params);

/**
 * @brief - Starts an Rx queue, when queue_cid is already prepared
 *
 * @param p_hwfn
 * @param p_cid
 * @param bd_max_bytes
 * @param bd_chain_phys_addr
 * @param cqe_pbl_addr
 * @param cqe_pbl_size
 *
 * @return int
 */
int
qed_eth_rxq_start_ramrod(struct qed_hwfn *p_hwfn,
			 struct qed_queue_cid *p_cid,
			 u16 bd_max_bytes,
			 dma_addr_t bd_chain_phys_addr,
			 dma_addr_t cqe_pbl_addr, u16 cqe_pbl_size);

/**
 * @brief - Starts a Tx queue, where queue_cid is already prepared
 *
 * @param p_hwfn
 * @param p_cid
 * @param pbl_addr
 * @param pbl_size
 * @param p_pq_params - parameters for choosing the PQ for this Tx queue
 *
 * @return int
 */
int
qed_eth_txq_start_ramrod(struct qed_hwfn *p_hwfn,
			 struct qed_queue_cid *p_cid,
			 dma_addr_t pbl_addr, u16 pbl_size, u16 pq_id);

u8 qed_mcast_bin_from_mac(u8 * mac);

int qed_set_rxq_coalesce(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 u16 coalesce, struct qed_queue_cid *p_cid);

int qed_set_txq_coalesce(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 u16 coalesce, struct qed_queue_cid *p_cid);

int qed_get_rxq_coalesce(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 struct qed_queue_cid *p_cid, u16 * p_hw_coal);

int qed_get_txq_coalesce(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 struct qed_queue_cid *p_cid, u16 * p_hw_coal);

/**
 * @brief - Sets a Tx rate limit on a specific queue
 *
 * @params p_hwfn
 * @params p_ptt
 * @params p_cid
 * @params rate
 *
 * @return int
 */
int
qed_eth_tx_queue_maxrate(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 struct qed_queue_cid *p_cid, u32 rate);
#endif
