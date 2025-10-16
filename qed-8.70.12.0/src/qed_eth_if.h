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

#ifndef _QED_ETH_IF_H
#define _QED_ETH_IF_H
#include <linux/types.h>
#include <linux/if_link.h>
#include <linux/list.h>
#include <linux/if_ether.h>
#include "qed_if.h"
#include "qed_iov_if.h"
enum qed_rss_caps {
	QED_RSS_IPV4 = 0x1,
	QED_RSS_IPV6 = 0x2,
	QED_RSS_IPV4_TCP = 0x4,
	QED_RSS_IPV6_TCP = 0x8,
	QED_RSS_IPV4_UDP = 0x10,
	QED_RSS_IPV6_UDP = 0x20,
};

/* Should be the same as ETH_RSS_IND_TABLE_ENTRIES_NUM */
#define QED_RSS_IND_TABLE_SIZE 128
#define QED_RSS_KEY_SIZE 10	/* size in 32b chunks */

#define QED_MAX_PHC_DRIFT_PPB   291666666
#define QED_VF_START_BIN_ID     16

enum qed_ptp_filter_type {
	QED_PTP_FILTER_NONE,
	QED_PTP_FILTER_ALL,
	QED_PTP_FILTER_V1_L4_EVENT,
	QED_PTP_FILTER_V1_L4_GEN,
	QED_PTP_FILTER_V2_L4_EVENT,
	QED_PTP_FILTER_V2_L4_GEN,
	QED_PTP_FILTER_V2_L2_EVENT,
	QED_PTP_FILTER_V2_L2_GEN,
	QED_PTP_FILTER_V2_EVENT,
	QED_PTP_FILTER_V2_GEN
};

enum qed_ptp_hwtstamp_tx_type {
	QED_PTP_HWTSTAMP_TX_OFF,
	QED_PTP_HWTSTAMP_TX_ON,
};
struct qed_queue_start_common_params {
	/* Should always be relative to entity sending this. */
	u8 vport_id;
	u16 queue_id;

	/* Relative, but relevant only for PFs */
	u8 stats_id;

	struct qed_sb_info *p_sb;
	u8 sb_idx;

	u8 tc;
	u16 pq_set_id;		/* Keep 0 for non pq_set/legacy mode mode */
};

struct qed_queue_update_common_params {
	u16 update_flag;
#define QED_UPDATE_TXQ_PQ       0x0001
#define QED_UPDATE_TXQ_PQ_SET   0x0002
	u16 pq_id;
	u16 pq_set_id;
	u8 tc;
};

struct qed_rxq_start_ret_params {
	void __iomem *p_prod;
	void *p_handle;
};

struct qed_txq_start_ret_params {
	void __iomem *p_doorbell;
	void *p_handle;
};
enum qed_filter_config_mode {
	QED_FILTER_CONFIG_MODE_DISABLE,
	QED_FILTER_CONFIG_MODE_5_TUPLE,
	QED_FILTER_CONFIG_MODE_L4_PORT,
	QED_FILTER_CONFIG_MODE_IP_DEST,
	QED_FILTER_CONFIG_MODE_TUNN_TYPE,
	QED_FILTER_CONFIG_MODE_IP_SRC,
	QED_FILTER_CONFIG_MODE_MAC_VLAN_L4_DST,
	QED_FILTER_CONFIG_MODE_VLAN_L4_DST,
	QED_FILTER_CONFIG_MODE_MAX
};
struct qed_ntuple_filter_params {
	/* Physically mapped address containing header of buffer to be used
	 * as filter.
	 */
	dma_addr_t addr;

	/* Virtual mapped address containing header of buffer to be used as
	 * filter.
	 */
	u8 *pkt_virt_addr;

	/* Length of header in bytes */
	u16 length;

	/* Relative queue-id to receive classified packet */
#define QED_RFS_NTUPLE_QID_RSS ((u16) - 1)
	u16 qid;

	/* Identifier can either be according to vport-id or vfid */
	bool b_is_vf;
	u8 vport_id;
	u8 vf_id;

	/* true iff this filter is to be added. Else to be removed */
	bool b_is_add;

	/* If packet need to be drop */
	bool b_is_drop;

	/* VF Queue selection */
	bool b_vfq_enabled;
	enum qed_filter_config_mode mode;
};

struct qed_dev_eth_info {
	struct qed_dev_info common;

	u16 num_queues;
	u8 num_tc;
	u8 num_vports;

	u8 port_mac[ETH_ALEN];
	u16 num_vlan_filters;
	u16 num_mac_filters;

	/* Legacy VF - this affects the datapath, so qede has to know */
	bool is_legacy;

	/* Might depend on available resources [in case of VF] */
	bool xdp_supported;
};

struct qed_update_vport_rss_params {
	void *rss_ind_table[128];
	u32 rss_key[10];
	u8 rss_caps;		/* Protocol fields to be used for rss hash calculation */
};

struct qed_update_vport_params {
	u8 vport_id;
	u8 update_vport_active_flg;
	u8 vport_active_flg;
	u8 update_inner_vlan_removal_flg;
	u8 inner_vlan_removal_flg;
	u8 update_tx_switching_flg;
	u8 tx_switching_flg;
	u8 update_accept_any_vlan_flg;
	u8 accept_any_vlan;
	u8 update_rss_flg;
	struct qed_update_vport_rss_params rss_params;
};

struct qed_start_vport_params {
	bool remove_inner_vlan;
	bool handle_ptp_pkts;
	bool gro_enable;
	bool drop_ttl0;
	u8 vport_id;
	u16 mtu;
	bool clear_stats;
	u8 zero_placement_offset;
};

enum qed_filter_rx_mode_type {
	QED_FILTER_RX_MODE_TYPE_REGULAR,
	QED_FILTER_RX_MODE_TYPE_MULTI_PROMISC,
	QED_FILTER_RX_MODE_TYPE_PROMISC,
};

enum qed_filter_xcast_params_type {
	QED_FILTER_XCAST_TYPE_ADD,
	QED_FILTER_XCAST_TYPE_DEL,
	QED_FILTER_XCAST_TYPE_REPLACE,
};

struct qed_filter_ucast_params {
	enum qed_filter_xcast_params_type type;
	u8 vlan_valid;
	u16 vlan;
	u8 mac_valid;
	unsigned char mac[ETH_ALEN];
};

struct qed_filter_mcast_params {
	enum qed_filter_xcast_params_type type;
	u8 num;
	unsigned char mac[64][ETH_ALEN];
};

union qed_filter_type_params {
	enum qed_filter_rx_mode_type accept_flags;
	struct qed_filter_ucast_params ucast;
	struct qed_filter_mcast_params mcast;
};

enum qed_filter_type {
	QED_FILTER_TYPE_UCAST,
	QED_FILTER_TYPE_MCAST,
	QED_FILTER_TYPE_RX_MODE,
	QED_MAX_FILTER_TYPES,
};

struct qed_filter_params {
	enum qed_filter_type type;
	union qed_filter_type_params filter;
	u8 vport_id;
};

struct qed_tunn_params {
	u16 vxlan_port;
	u8 update_vxlan_port;
	u16 geneve_port;
	u8 update_geneve_port;
};

struct qed_eth_cb_ops {
	struct qed_common_cb_ops common;
	void (*force_mac) (void *dev, u8 * mac, bool forced);
	void (*ports_update) (void *dev, u16 vxlan_port, u16 geneve_port);
};

struct qed_eth_ptp_ops {
	int (*cfg_filters) (struct qed_dev *,
			    enum qed_ptp_filter_type,
			    enum qed_ptp_hwtstamp_tx_type);
	int (*read_rx_ts) (struct qed_dev *, u64 *);
	int (*read_tx_ts) (struct qed_dev *, u64 *);
	int (*read_cc) (struct qed_dev *, u64 *);
	int (*disable) (struct qed_dev *);
	int (*adjfreq) (struct qed_dev *, s32);
	int (*enable) (struct qed_dev *);
};

struct qed_eth_ops {
	const struct qed_common_ops *common;

	const struct qed_iov_hv_ops *iov;

	const struct qed_eth_ptp_ops *ptp;

	const struct qed_lag_ops *lag;

	int (*fill_dev_info) (struct qed_dev
			      * cdev, struct qed_dev_eth_info * info);

	void (*register_ops) (struct qed_dev
			      * cdev,
			      struct qed_eth_cb_ops * ops, void *cookie);

	 bool(*check_mac) (struct qed_dev * cdev, u8 * mac);

	void (*init_admin_mac) (struct qed_dev * cdev, u8 * mac);

	int (*vport_start) (struct qed_dev
			    * cdev, struct qed_start_vport_params * params);

	int (*vport_stop) (struct qed_dev * cdev, u8 vport_id);

	int (*vport_update) (struct qed_dev
			     * cdev, struct qed_update_vport_params * params);

	int (*q_rx_start) (struct qed_dev
			   * cdev,
			   u8
			   rss_num,
			   struct
			   qed_queue_start_common_params * params,
			   u16
			   bd_max_bytes,
			   dma_addr_t
			   bd_chain_phys_addr,
			   dma_addr_t
			   cqe_pbl_addr,
			   u16
			   cqe_pbl_size,
			   struct qed_rxq_start_ret_params * ret_params);

	int (*q_rx_stop) (struct qed_dev * cdev, u8 rss_id, void *handle);

	int (*q_tx_start) (struct qed_dev * cdev,
			   u8 rss_num,
			   struct qed_queue_start_common_params * params,
			   dma_addr_t
			   pbl_addr,
			   u16
			   pbl_size,
			   struct qed_txq_start_ret_params * ret_params);

	int (*q_tx_stop) (struct qed_dev * cdev, u8 rss_id, void *handle);

	int (*filter_config) (struct qed_dev * cdev,
			      struct qed_filter_params * params);

	int (*fastpath_stop) (struct qed_dev * cdev);

	int (*eth_cqe_completion) (struct qed_dev * cdev,
				   u8 rss_id,
				   struct eth_slow_path_rx_cqe * cqe);

	int (*tunn_config) (struct qed_dev * cdev,
			    struct qed_tunn_params * params);
/**
 * @brief ntuple_filter_config - Configure aRFS filter for a vport
 *
 * @param cdev
 * @param cookie - cookie which will be passed in SPQ completion callback.
 * @param params
 *
 **/
	int (*ntuple_filter_config) (struct qed_dev * cdev,
				     void *cookie,
				     struct qed_ntuple_filter_params * params);
/**
 * @brief configure_arfs_searcher - Enable/Disable searcher
 *
 * @param cdev
 * @param en_searcher - Enable in some mode/Disable searcher on hardware
 *
 **/
	int (*configure_arfs_searcher) (struct qed_dev * cdev,
					enum qed_filter_config_mode mode);

/**
 * @brief read hw coalesce per queue basis.
 *
 * @param cdev
 * @param coal - store coalesce value read from the hardware
 * @param handle
 *
 **/
	int (*get_coalesce) (struct qed_dev * cdev, u16 * coal, void *handle);

	int (*q_maxrate) (struct qed_dev * cdev,
			  u8 qid, void *handle, u32 rate);
	int (*req_bulletin_update_mac) (struct qed_dev * cdev, const u8 * mac);
};

#ifdef QED_UPSTREAM
const struct qed_eth_ops *qed_get_eth_ops(void);
#else
const struct qed_eth_ops *qed_get_eth_ops(u32 version);
#endif

void qed_put_eth_ops(void);
#endif
