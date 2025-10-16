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

#ifndef _QED_VF_H
#define _QED_VF_H
#include <linux/types.h>
#include <asm/byteorder.h>
#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/delay.h>
#include <linux/if_ether.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "qed_dev_api.h"
#include "qed_l2.h"
#include "qed_mcp.h"
#include "qed_rdma.h"
#include "qed_sp.h"
#include "qed_rdma_if.h"

#define T_ETH_INDIRECTION_TABLE_SIZE 128	/* @@@ TBD MichalK this should be HSI? */
#define T_ETH_RSS_KEY_SIZE 10	/* @@@ TBD this should be HSI? */

/***********************************************
 *
 * Common definitions for all HVs
 *
 **/
struct vf_pf_resc_request {
	u8 num_rxqs;
	u8 num_txqs;
	u8 num_sbs;
	u8 num_mac_filters;
	u8 num_vlan_filters;
	u8 num_mc_filters;	/* No limit  so superfluous */
	u8 num_cids;
	u8 padding;
};

struct hw_sb_info {
	u16 hw_sb_id;		/* aka absolute igu id, used to ack the sb */
	u8 sb_qid;		/* used to update DHC for sb */
	u8 padding[5];
};

/* QED GID can be used as IPv4/6 address in RoCE v2 */
union vfpf_roce_gid {
	u8 bytes[16];
	u16 words[8];
	u32 dwords[4];
	u64 qwords[2];
	u32 ipv4_addr;
};

/***********************************************
 *
 * HW VF-PF channel definitions
 *
 * A.K.A VF-PF mailbox
 *
 **/
#define TLV_BUFFER_SIZE                 1024

#define PFVF_MAX_QUEUES_PER_VF          16
#define PFVF_MAX_CNQS_PER_VF            16
#define PFVF_MAX_SBS_PER_VF \
	(PFVF_MAX_QUEUES_PER_VF + PFVF_MAX_CNQS_PER_VF)

/* vf pf channel tlvs */
/* general tlv header (used for both vf->pf request and pf->vf response) */
struct channel_tlv {
	u16 type;
	u16 length;
};

/* header of first vf->pf tlv carries the offset used to calculate reponse
 * buffer address
 */
struct vfpf_first_tlv {
	struct channel_tlv tl;
	u32 padding;
	u64 reply_address;
};

/* header of pf->vf tlvs, carries the status of handling the request */
struct pfvf_tlv {
	struct channel_tlv tl;
	u8 status;
	u8 padding[3];
};

/* response tlv used for most tlvs */
struct pfvf_def_resp_tlv {
	struct pfvf_tlv hdr;
};

/* used to terminate and pad a tlv list */
struct channel_list_end_tlv {
	struct channel_tlv tl;
	u8 padding[4];
};

/* Acquire */
struct vfpf_acquire_tlv {
	struct vfpf_first_tlv first_tlv;

	struct vf_pf_vfdev_info {
#define VFPF_ACQUIRE_CAP_PRE_FP_HSI     (1 << 0)	/* VF pre-FP hsi version */
#define VFPF_ACQUIRE_CAP_100G           (1 << 1)	/* VF can support 100g */

		/* A requirement for supporting multi-Tx queues on a single queue-zone,
		 * VF would pass qids as additional information whenever passing queue
		 * references.
		 * TODO - due to the CID limitations in Bar0, VFs currently don't pass
		 * this, and use the legacy CID scheme.
		 */
#define VFPF_ACQUIRE_CAP_QUEUE_QIDS     (1 << 2)

		/* The VF is using the physical bar. While this is mostly internal
		 * to the VF, might affect the number of CIDs supported assuming
		 * QUEUE_QIDS is set.
		 */
#define VFPF_ACQUIRE_CAP_PHYSICAL_BAR   (1 << 3)

		/* Let the PF know this VF needs DORQ flush in case of overflow */
#define VFPF_ACQUIRE_CAP_EDPM           (1 << 4)

		/* Below capabilities to let PF know that VF supports RDMA. */
#define VFPF_ACQUIRE_CAP_ROCE (1 << 5)
#define VFPF_ACQUIRE_CAP_IWARP (1 << 6)

		/* Requesting and receiving EQs from PF */
#define VFPF_ACQUIRE_CAP_EQ             (1 << 7)

		/* Let the PF know this VF supports XRC mode */
#define VFPF_ACQUIRE_CAP_XRC            (1 << 8)

		u64 capabilities;
		u8 fw_major;
		u8 fw_minor;
		u8 fw_revision;
		u8 fw_engineering;
		u32 driver_version;
		u16 opaque_fid;	/* ME register value */
		u8 os_type;	/* VFPF_ACQUIRE_OS_* value */
		u8 eth_fp_hsi_major;
		u8 eth_fp_hsi_minor;
		u8 padding[3];
	} vfdev_info;

	struct vf_pf_resc_request resc_request;

	u64 bulletin_addr;
	u32 bulletin_size;
	u32 padding;
};

/* receive side scaling tlv */
struct vfpf_vport_update_rss_tlv {
	struct channel_tlv tl;

	u8 update_rss_flags;
#define VFPF_UPDATE_RSS_CONFIG_FLAG       (1 << 0)
#define VFPF_UPDATE_RSS_CAPS_FLAG         (1 << 1)
#define VFPF_UPDATE_RSS_IND_TABLE_FLAG    (1 << 2)
#define VFPF_UPDATE_RSS_KEY_FLAG          (1 << 3)

	u8 rss_enable;
	u8 rss_caps;
	u8 rss_table_size_log;	/* The table size is 2 ^ rss_table_size_log */
	u16 rss_ind_table[T_ETH_INDIRECTION_TABLE_SIZE];
	u32 rss_key[T_ETH_RSS_KEY_SIZE];
};

struct pfvf_storm_stats {
	u32 address;
	u32 len;
};

struct pfvf_stats_info {
	struct pfvf_storm_stats mstats;
	struct pfvf_storm_stats pstats;
	struct pfvf_storm_stats tstats;
	struct pfvf_storm_stats ustats;
};

/* acquire response tlv - carries the allocated resources */
struct pfvf_acquire_resp_tlv {
	struct pfvf_tlv hdr;

	struct pf_vf_pfdev_info {
		u32 chip_num;
		u32 mfw_ver;

		u16 fw_major;
		u16 fw_minor;
		u16 fw_rev;
		u16 fw_eng;

		u64 capabilities;
#define PFVF_ACQUIRE_CAP_DEFAULT_UNTAGGED       (1 << 0)
#define PFVF_ACQUIRE_CAP_100G                   (1 << 1)	/* If set, 100g PF */
/* There are old PF versions where the PF might mistakenly override the sanity
 * mechanism [version-based] and allow a VF that can't be supported to pass
 * the acquisition phase.
 * To overcome this, PFs now indicate that they're past that point and the new
 * VFs would fail probe on the older PFs that fail to do so.
 */
#define PFVF_ACQUIRE_CAP_POST_FW_OVERRIDE       (1 << 2)

		/* PF expects queues to be received with additional qids */
#define PFVF_ACQUIRE_CAP_QUEUE_QIDS             (1 << 3)

		/* Below capabilities to let VF know what PF/chip supports. */
#define PFVF_ACQUIRE_CAP_ROCE (1 << 4)
#define PFVF_ACQUIRE_CAP_IWARP (1 << 5)

		u16 db_size;
		u8 indices_per_sb;
		u8 os_type;

		/* These should match the PF's qed_dev values */
		u8 chip_rev;
		u8 chip_metal;
		u8 dev_type;

		/* Doorbell bar size configured in HW: log(size) or 0 */
		u8 bar_size;

		struct pfvf_stats_info stats_info;

		u8 port_mac[ETH_ALEN];

		/* It's possible PF had to configure an older fastpath HSI
		 * [in case VF is newer than PF]. This is communicated back
		 * to the VF. It can also be used in case of error due to
		 * non-matching versions to shed light in VF about failure.
		 */
		u8 major_fp_hsi;
		u8 minor_fp_hsi;
	} pfdev_info;

	struct pf_vf_resc {
		/* in case of status NO_RESOURCE in message hdr, pf will fill
		 * this struct with suggested amount of resources for next
		 * acquire request
		 */
		struct hw_sb_info hw_sbs[PFVF_MAX_QUEUES_PER_VF];
		u8 hw_qid[PFVF_MAX_QUEUES_PER_VF];
		u8 cid[PFVF_MAX_QUEUES_PER_VF];

		u8 num_rxqs;
		u8 num_txqs;
		u8 num_sbs;
		u8 num_mac_filters;
		u8 num_vlan_filters;
		u8 num_mc_filters;
		u8 num_cids;
		u8 padding;
	} resc;

	u32 bulletin_size;
	u32 padding;
};

struct pfvf_start_queue_resp_tlv {
	struct pfvf_tlv hdr;
	u32 offset;		/* offset to consumer/producer of queue */
	u8 padding[4];
};

/* Extended queue information - additional index for reference inside qzone.
 * If commmunicated between VF/PF, each TLV relating to queues should be
 * extended by one such [or have a future base TLV that already contains info].
 */
struct vfpf_qid_tlv {
	struct channel_tlv tl;
	u8 qid;
	u8 padding[3];
};

/* Soft FLR req */
struct vfpf_soft_flr_tlv {
	struct vfpf_first_tlv first_tlv;
	u32 reserved1;
	u32 reserved2;
};

/* VFQ filter request */
struct vfpf_filter_cfg_tlv {
	/* from qed_ntuple_filter_params */
	struct vfpf_first_tlv first_tlv;
	u32 length;
	u32 qid;

	/* Enough packet_hdr size to ensure
	 * template pkt hdr for the rule can fit in.
	 */
#define MAX_PKT_HDR_LEN 256
	u8 packet_hdr_buf[MAX_PKT_HDR_LEN];
	u8 b_is_add;
	u8 b_is_drop;
	u8 mode;
	u8 padding[5];
};

/* Setup Queue */
struct vfpf_start_rxq_tlv {
	struct vfpf_first_tlv first_tlv;

	/* physical addresses */
	u64 rxq_addr;
	u64 deprecated_sge_addr;
	u64 cqe_pbl_addr;

	u16 cqe_pbl_size;
	u16 hw_sb;
	u16 rx_qid;
	u16 hc_rate;		/* desired interrupts per sec. */

	u16 bd_max_bytes;
	u16 stat_id;
	u8 sb_index;
	u8 padding[3];
};

struct vfpf_start_txq_tlv {
	struct vfpf_first_tlv first_tlv;

	/* physical addresses */
	u64 pbl_addr;
	u16 pbl_size;
	u16 stat_id;
	u16 tx_qid;
	u16 hw_sb;

	u32 flags;		/* VFPF_QUEUE_FLG_X flags */
	u16 hc_rate;		/* desired interrupts per sec. */
	u8 sb_index;
	u8 tc;
	u8 padding[2];
};

/* Stop RX Queue */
struct vfpf_stop_rxqs_tlv {
	struct vfpf_first_tlv first_tlv;

	u16 rx_qid;

	/* While the API supports multiple Rx-queues on a single TLV
	 * message, in practice older VFs always used it as one [qed].
	 * And there are PFs [starting with the CHANNEL_TLV_QID] which
	 * would start assuming this is always a '1'. So in practice this
	 * field should be considered deprecated and *Always* set to '1'.
	 */
	u8 num_rxqs;

	u8 cqe_completion;
	u8 padding[4];
};

/* Stop TX Queues */
struct vfpf_stop_txqs_tlv {
	struct vfpf_first_tlv first_tlv;

	u16 tx_qid;

	/* While the API supports multiple Tx-queues on a single TLV
	 * message, in practice older VFs always used it as one [qed].
	 * And there are PFs [starting with the CHANNEL_TLV_QID] which
	 * would start assuming this is always a '1'. So in practice this
	 * field should be considered deprecated and *Always* set to '1'.
	 */
	u8 num_txqs;
	u8 padding[5];
};

struct vfpf_update_rxq_tlv {
	struct vfpf_first_tlv first_tlv;

	u64 deprecated_sge_addr[PFVF_MAX_QUEUES_PER_VF];

	u16 rx_qid;
	u8 num_rxqs;
	u8 flags;
#define VFPF_RXQ_UPD_INIT_SGE_DEPRECATE_FLAG    (1 << 0)
#define VFPF_RXQ_UPD_COMPLETE_CQE_FLAG          (1 << 1)
#define VFPF_RXQ_UPD_COMPLETE_EVENT_FLAG        (1 << 2)

	u8 padding[4];
};

/* Set Queue Filters */
struct vfpf_q_mac_vlan_filter {
	u32 flags;
#define VFPF_Q_FILTER_DEST_MAC_VALID    0x01
#define VFPF_Q_FILTER_VLAN_TAG_VALID    0x02
#define VFPF_Q_FILTER_SET_MAC           0x100	/* set/clear */

	u8 mac[ETH_ALEN];
	u16 vlan_tag;

	u8 padding[4];
};

/* Start a vport */
struct vfpf_vport_start_tlv {
	struct vfpf_first_tlv first_tlv;

	u64 sb_addr[PFVF_MAX_QUEUES_PER_VF];

	u32 tpa_mode;
	u16 dep1;
	u16 mtu;

	u8 vport_id;
	u8 inner_vlan_removal;

	u8 only_untagged;
	u8 max_buffers_per_cqe;

	u8 zero_placement_offset;
	u8 padding[3];
};

/* Extended tlvs - need to add rss, mcast, accept mode tlvs */
struct vfpf_vport_update_activate_tlv {
	struct channel_tlv tl;
	u8 update_rx;
	u8 update_tx;
	u8 active_rx;
	u8 active_tx;
};

struct vfpf_vport_update_tx_switch_tlv {
	struct channel_tlv tl;
	u8 tx_switching;
	u8 padding[3];
};

struct vfpf_vport_update_vlan_strip_tlv {
	struct channel_tlv tl;
	u8 remove_vlan;
	u8 padding[3];
};

struct vfpf_vport_update_mcast_bin_tlv {
	struct channel_tlv tl;
	u8 padding[4];

	/* This was a mistake; There are only 256 approx bins,
	 * and in HSI they're divided into 32-bit values.
	 * As old VFs used to set-bit to the values on its side,
	 * the upper half of the array is never expected to contain any data.
	 */
	u64 bins[4];
	u64 obsolete_bins[4];
};

struct vfpf_vport_update_accept_param_tlv {
	struct channel_tlv tl;
	u8 update_rx_mode;
	u8 update_tx_mode;
	u8 rx_accept_filter;
	u8 tx_accept_filter;
};

struct vfpf_vport_update_accept_any_vlan_tlv {
	struct channel_tlv tl;
	u8 update_accept_any_vlan_flg;
	u8 accept_any_vlan;

	u8 padding[2];
};

struct vfpf_vport_update_sge_tpa_tlv {
	struct channel_tlv tl;

	u16 sge_tpa_flags;
#define VFPF_TPA_IPV4_EN_FLAG        (1 << 0)
#define VFPF_TPA_IPV6_EN_FLAG        (1 << 1)
#define VFPF_TPA_PKT_SPLIT_FLAG      (1 << 2)
#define VFPF_TPA_HDR_DATA_SPLIT_FLAG (1 << 3)
#define VFPF_TPA_GRO_CONSIST_FLAG    (1 << 4)
#define VFPF_TPA_TUNN_IPV4_EN_FLAG   (1 << 5)
#define VFPF_TPA_TUNN_IPV6_EN_FLAG   (1 << 6)

	u8 update_sge_tpa_flags;
#define VFPF_UPDATE_SGE_DEPRECATED_FLAG    (1 << 0)
#define VFPF_UPDATE_TPA_EN_FLAG    (1 << 1)
#define VFPF_UPDATE_TPA_PARAM_FLAG (1 << 2)

	u8 max_buffers_per_cqe;

	u16 deprecated_sge_buff_size;
	u16 tpa_max_size;
	u16 tpa_min_size_to_start;
	u16 tpa_min_size_to_cont;

	u8 tpa_max_aggs_num;
	u8 padding[7];
};

/* Primary tlv as a header for various extended tlvs for
 * various functionalities in vport update ramrod.
 */
struct vfpf_vport_update_tlv {
	struct vfpf_first_tlv first_tlv;
};

struct vfpf_ucast_filter_tlv {
	struct vfpf_first_tlv first_tlv;

	u8 opcode;
	u8 type;

	u8 mac[ETH_ALEN];

	u16 vlan;
	u16 padding[3];
};

/* tunnel update param tlv */
struct vfpf_update_tunn_param_tlv {
	struct vfpf_first_tlv first_tlv;

	u8 tun_mode_update_mask;
	u8 tunn_mode;
	u8 update_tun_cls;
	u8 vxlan_clss;
	u8 l2gre_clss;
	u8 ipgre_clss;
	u8 l2geneve_clss;
	u8 ipgeneve_clss;
	u8 update_geneve_port;
	u8 update_vxlan_port;
	u16 geneve_port;
	u16 vxlan_port;
	u8 update_non_l2_vxlan;
	u8 non_l2_vxlan_enable;
};

struct pfvf_update_tunn_param_tlv {
	struct pfvf_tlv hdr;

	u16 tunn_feature_mask;
	u8 vxlan_mode;
	u8 l2geneve_mode;
	u8 ipgeneve_mode;
	u8 l2gre_mode;
	u8 ipgre_mode;
	u8 vxlan_clss;
	u8 l2gre_clss;
	u8 ipgre_clss;
	u8 l2geneve_clss;
	u8 ipgeneve_clss;
	u16 vxlan_udp_port;
	u16 geneve_udp_port;
};

struct tlv_buffer_size {
	u8 tlv_buffer[TLV_BUFFER_SIZE];
};

struct vfpf_update_coalesce {
	struct vfpf_first_tlv first_tlv;
	u16 rx_coal;
	u16 tx_coal;
	u16 qid;
	u8 padding[2];
};

struct vfpf_read_coal_req_tlv {
	struct vfpf_first_tlv first_tlv;
	u16 qid;
	u8 is_rx;
	u8 padding[5];
};

struct pfvf_read_coal_resp_tlv {
	struct pfvf_tlv hdr;
	u16 coal;
	u8 padding[6];
};

struct vfpf_bulletin_update_mac_tlv {
	struct vfpf_first_tlv first_tlv;
	u8 mac[ETH_ALEN];
	u8 padding[2];
};

struct vfpf_update_mtu_tlv {
	struct vfpf_first_tlv first_tlv;
	u16 mtu;
	u8 padding[6];
};

struct vfpf_est_ll2_conn_tlv {
	struct vfpf_first_tlv first_tlv;
	u64 rx_bd_base_addr;
	u64 rx_cqe_pbl_addr;
	u64 tx_pbl_addr;
	u16 mtu;
	u16 rx_cq_num_pbl_pages;
	u16 tx_num_pbl_pages;
	u16 sb_id;
	u8 txq_sb_pi;
	u8 rxq_sb_pi;
	u8 conn_type;
	u8 drop_ttl0_flg;
	u8 inner_vlan_stripping_en;
	u8 gsi_enable;
	u8 rel_qid;
	u8 err_packet_too_big;
	u8 err_no_buf;
	u8 tx_stats_en;
	u8 rx_cb_registered;
	u8 tx_cb_registered;
	u8 padding[4];
};

struct pfvf_est_ll2_conn_resp_tlv {
	struct pfvf_tlv hdr;
	u64 rxq_db_offset;
	u64 txq_db_offset;
	u64 rxq_db_msg;
	u64 txq_db_msg;
};

struct vfpf_terminate_ll2_conn_tlv {
	struct vfpf_first_tlv first_tlv;
	u8 conn_handle;
	u8 rx_cb_registered;
	u8 tx_cb_registered;
	u8 padding[5];
};

/* RDMA Acquire */
struct vfpf_rdma_acquire_tlv {
	struct vfpf_first_tlv first_tlv;

#define VFPF_RDMA_ACQUIRE_CAP_CNQ_SB_START_ID   (1 << 0)
	u64 capabilities;
	u8 num_cnqs;
	u16 cnq_sb_start_id;
	u8 padding[5];
};

/* RDMA acquire response tlv - carries the allocated resources */
struct pfvf_rdma_acquire_resp_tlv {
	struct pfvf_tlv hdr;

	/* Add any capability flags like below, change XXX with actual name. */
#define PFVF_RDMA_ACQUIRE_CAP_XXX       (1 << 0)
	u64 capabilities;

	struct hw_sb_info hw_sbs[PFVF_MAX_QUEUES_PER_VF];
	u16 hw_qid[PFVF_MAX_QUEUES_PER_VF];

	u8 num_cnqs;
	u8 max_queue_zones;

	u16 wid_cound;
	u32 dpi_size;
	u32 dpi_count;
	u32 dpi_start_offset;

	u32 reserved1;
	u32 reserved2;
};

struct vfpf_rdma_start_tlv {
	struct vfpf_first_tlv first_tlv;

	/* Physical address which will be used by PF to copy from. */
	dma_addr_t cnq_pbl_list_phy_addr;

	u8 mac_addr[ETH_ALEN];
	u16 max_mtu;

	struct qed_roce_dcqcn_params_tlv {
		u8 notification_point;
		u8 reaction_point;

		/* fields for notification point */
		u8 cnp_dscp;
		u8 cnp_vlan_priority;
		u32 cnp_send_timeout;

		/* fields for reaction point */
		u32 rl_bc_rate;	/* Byte Counter Limit. */
		u32 rl_max_rate;	/* Maximum rate in Mbps resolution */
		u32 rl_r_ai;	/* Active increase rate */
		u32 rl_r_hai;	/* Hyper active increase rate */
		u32 dcqcn_gd;	/* Alpha denominator */
		u32 dcqcn_k_us;	/* Alpha update interval */
		u32 dcqcn_timeout_us;
	} dcqcn_params;

	enum qed_rdma_cq_mode cq_mode;

	u64 sb_addr[PFVF_MAX_CNQS_PER_VF];

	u8 desired_cnq;
	u8 ll2_handle;		/* required for UD QPs */
	u8 padding[6];
};

struct vfpf_rdma_stop_tlv {
	struct vfpf_first_tlv first_tlv;
	u32 reserved1;
	u32 reserved2;
};

struct vfpf_rdma_query_counters_tlv {
	struct vfpf_first_tlv first_tlv;
	u32 reserved1;
	u32 reserved2;
};

struct pfvf_rdma_query_counters_resp_tlv {
	struct pfvf_tlv hdr;
	u64 pd_count;
	u64 max_pd;
	u64 dpi_count;
	u64 max_dpi;
	u64 cq_count;
	u64 max_cq;
	u64 qp_count;
	u64 max_qp;
	u64 tid_count;
	u64 max_tid;
	u64 srq_count;
	u64 max_srq;
	u64 xrc_srq_count;
	u64 max_xrc_srq;
	u64 xrcd_count;
	u64 max_xrcd;

	u32 reserved1;
	u32 reserved2;
};

struct vfpf_rdma_alloc_tid_tlv {
	struct vfpf_first_tlv first_tlv;
	u32 reserved1;
	u32 reserved2;
};

struct pfvf_rdma_alloc_tid_resp_tlv {
	struct pfvf_tlv hdr;
	u32 tid;
	u8 padding[4];
};

struct vfpf_rdma_register_tid_tlv {
	struct vfpf_first_tlv first_tlv;

	/* input variables (given by miniport) */
	u32 itid;		/* index only, 18 bit long, lkey = itid << 8 | key */
	enum qed_rdma_tid_type tid_type;

	u8 key;
	u8 local_read;
	u8 local_write;
	u8 remote_read;
	u8 remote_write;
	u8 remote_atomic;
	u8 mw_bind;
	u8 pbl_two_level;
	u8 pbl_page_size_log;	/* for the pages that contain the pointers
				 * to the MR pages
				 */
	u8 page_size_log;	/* for the MR pages */
	u16 pd;
	u32 fbo;
	u64 pbl_ptr;
	u64 length;		/* only lower 40 bits are valid */
	u64 vaddr;
	/* DIF related fields */
	u64 dif_error_addr;
	u8 dif_enabled;

	u8 zbva;
	u8 phy_mr;
	u8 dma_mr;
	u8 padding[4];
};

struct vfpf_rdma_deregister_tid_tlv {
	struct vfpf_first_tlv first_tlv;
	u32 tid;
	u8 padding[4];
};

struct vfpf_rdma_free_tid_tlv {
	struct vfpf_first_tlv first_tlv;
	u32 tid;
	u8 padding[4];
};

struct vfpf_rdma_create_cq_tlv {
	struct vfpf_first_tlv first_tlv;

	/* input variables (given by miniport) */
	u32 cq_handle_lo;	/* CQ handle to be written in CNQ */
	u32 cq_handle_hi;
	u32 cq_size;
	u16 pbl_num_pages;
	u8 pbl_two_level;
	u8 pbl_page_size_log;	/* for the pages that contain the
				 * pointers to the CQ pages
				 */
	u64 pbl_ptr;
	u16 dpi;
	u16 int_timeout;
	u8 cnq_id;
	u8 padding[3];
};

struct pfvf_rdma_create_cq_resp_tlv {
	struct pfvf_tlv hdr;
	u32 cq_icid;
	u8 padding[4];
};

struct vfpf_rdma_resize_cq_tlv {
	struct vfpf_first_tlv first_tlv;

	u32 cq_size;
	u16 icid;
	u16 pbl_num_pages;
	u64 pbl_ptr;
	u8 pbl_two_level;
	u8 pbl_page_size_log;	/* for the pages that contain the
				 * pointers to the CQ pages
				 */
	u8 padding[6];
};

struct pfvf_rdma_resize_cq_resp_tlv {
	struct pfvf_tlv hdr;

	/* output variables, provided to the upper layer */
	u32 prod;		/* CQ producer value on old PBL */
	u32 cons;		/* CQ consumer value on old PBL */

	u32 reserved1;
	u32 reserved2;
};

struct vfpf_rdma_destroy_cq_tlv {
	struct vfpf_first_tlv first_tlv;
	u16 icid;
	u8 padding[6];
};

struct pfvf_rdma_destroy_cq_resp_tlv {
	struct pfvf_tlv hdr;

	/* Sequence number of completion notification sent for the CQ on
	 * the associated CNQ
	 */
	u16 num_cq_notif;
	u8 padding[6];
};

struct vfpf_rdma_channel_qp {
	u32 qp_handle_lo;
	u32 qp_handle_hi;
	u32 qp_handle_async_lo;
	u32 qp_handle_async_hi;

	enum qed_rdma_qp_type qp_type;
	enum roce_mode roce_mode;
	enum qed_roce_qp_state cur_state;

	u32 dest_qp;
	u16 icid;
	u16 qp_idx;
	u32 flow_label;
	u32 ack_timeout;
	u16 pd;
	u16 pkey;
	u16 mtu;
	u16 srq_id;
	u16 vlan_id;
	u16 dpi;
	u16 udp_src_port;
	u8 use_srq;
	u8 signal_all;
	u8 fmr_and_reserved_lkey;
	u8 incoming_rdma_read_en;
	u8 incoming_rdma_write_en;
	u8 incoming_atomic_en;
	u8 e2e_flow_control_en;
	u8 sqd_async;
	u8 traffic_class_tos;
	u8 hop_limit_ttl;
	u8 retry_cnt;
	u8 rnr_retry_cnt;
	u8 min_rnr_nak_timer;
	u8 stats_queue;

	union vfpf_roce_gid sgid;
	union vfpf_roce_gid dgid;

	/* requeseter */
	u32 sq_psn;
	u32 cq_prod_req;
	u16 sq_cq_id;
	u16 sq_num_pages;
	u8 max_rd_atomic_req;
	u8 orq_num_pages;
	u8 req_offloaded;
	u8 has_req;

	dma_addr_t sq_pbl_ptr;
	dma_addr_t orq_phys_addr;

	/* responder */
	u32 rq_psn;
	u32 cq_prod_resp;
	u16 rq_cq_id;
	u16 rq_num_pages;
	u8 max_rd_atomic_resp;
	u8 irq_num_pages;
	u8 resp_offloaded;
	u8 has_resp;

	dma_addr_t rq_pbl_ptr;
	dma_addr_t irq_phys_addr;

	u8 remote_mac_addr[ETH_ALEN];
	u8 local_mac_addr[ETH_ALEN];
	u16 xrcd_id;
	u8 edpm_mode;
	u8 force_lb;
};

struct vfpf_rdma_create_qp_tlv {
	struct vfpf_first_tlv first_tlv;

	u32 qp_handle_lo;	/* QP handle to be written in CQE */
	u32 qp_handle_hi;
	u32 qp_handle_async_lo;	/* QP handle to be written in async event */
	u32 qp_handle_async_hi;

	enum qed_rdma_qp_type qp_type;

	u16 pd;
	u16 dpi;
	u32 create_flags;
	u8 use_srq;
	u8 signal_all;
	u8 fmr_and_reserved_lkey;
	u8 max_sq_sges;
	u16 sq_cq_id;
	u16 sq_num_pages;
	u16 rq_cq_id;
	u16 rq_num_pages;
	u64 sq_pbl_ptr;
	u64 rq_pbl_ptr;
	u16 srq_id;
	u16 xrcd_id;
	u8 stats_queue;
	u8 padding[3];
};

struct pfvf_rdma_create_qp_resp_tlv {
	struct pfvf_tlv hdr;

	struct vfpf_rdma_channel_qp channel_qp;

	u32 qp_id;
	u16 icid;
	u8 padding[2];
};

struct vfpf_rdma_modify_qp_tlv {
	struct vfpf_first_tlv first_tlv;

	struct vfpf_rdma_channel_qp channel_qp;

	u32 modify_flags;
	enum qed_roce_qp_state new_state;
	enum roce_mode roce_mode;
	u8 incoming_rdma_read_en;
	u8 incoming_rdma_write_en;
	u8 incoming_atomic_en;
	u8 e2e_flow_control_en;
	u32 dest_qp;
	u16 mtu;
	u8 traffic_class_tos;	/* IPv6/GRH tc; IPv4 TOS */
	u8 hop_limit_ttl;	/* IPv6/GRH hop limit; IPv4 TTL */
	u32 flow_label;		/* ignored in IPv4 */
	u16 udp_src_port;	/* RoCEv2 only */
	u16 vlan_id;

	union vfpf_roce_gid sgid;	/* GRH SGID; IPv4/6 Source IP */
	union vfpf_roce_gid dgid;	/* GRH DGID; IPv4/6 Destination IP */

	u32 rq_psn;
	u32 sq_psn;
	u32 ack_timeout;
	u16 pkey;
	u8 max_rd_atomic_resp;
	u8 max_rd_atomic_req;
	u8 remote_mac_addr[ETH_ALEN];
	u8 local_mac_addr[ETH_ALEN];
	u8 retry_cnt;
	u8 rnr_retry_cnt;
	u8 min_rnr_nak_timer;
	u8 sqd_async;
	u8 use_local_mac;
	u8 padding[1];
};

struct pfvf_rdma_modify_qp_resp_tlv {
	struct pfvf_tlv hdr;

	struct vfpf_rdma_channel_qp channel_qp;
	u32 reserved1;
	u32 reserved2;
};

struct vfpf_rdma_query_qp_tlv {
	struct vfpf_first_tlv first_tlv;

	struct vfpf_rdma_channel_qp channel_qp;
	u32 reserved1;
	u32 reserved2;
};

struct pfvf_rdma_query_qp_resp_tlv {
	struct pfvf_tlv hdr;

	enum qed_roce_qp_state state;
	u32 rq_psn;		/* responder */
	u32 sq_psn;		/* requester */
	u32 dest_qp;
	u32 flow_label;		/* ignored in IPv4 */
	u8 incoming_rdma_read_en;
	u8 incoming_rdma_write_en;
	u8 incoming_atomic_en;
	u8 e2e_flow_control_en;

	union vfpf_roce_gid sgid;	/* GRH SGID; IPv4/6 Source IP */
	union vfpf_roce_gid dgid;	/* GRH DGID; IPv4/6 Destination IP */

	u16 mtu;
	u8 draining;		/* send queue is draining */
	u8 hop_limit_ttl;	/* IPv6/GRH hop limit; IPv4 TTL */
	u8 traffic_class_tos;	/* IPv6/GRH tc; IPv4 TOS */
	u8 rnr_retry;
	u8 retry_cnt;
	u8 min_rnr_nak_timer;
	u32 timeout;
	u16 pkey_index;
	u8 max_rd_atomic;
	u8 max_dest_rd_atomic;
	u8 sqd_async;
	u8 padding[7];
};

struct vfpf_rdma_destroy_qp_tlv {
	struct vfpf_first_tlv first_tlv;

	struct vfpf_rdma_channel_qp channel_qp;
	u32 reserved1;
	u32 reserved2;
};

struct pfvf_rdma_destroy_qp_resp_tlv {
	struct pfvf_tlv hdr;

	u32 sq_cq_prod;
	u32 rq_cq_prod;

	u32 reserved1;
	u32 reserved2;
};

struct vfpf_rdma_create_srq_tlv {
	struct vfpf_first_tlv first_tlv;

	u64 pbl_base_addr;
	u64 prod_pair_addr;
	u16 num_pages;
	u16 pd_id;
	u16 page_size;

	/* XRC related only */
	u16 xrcd_id;
	u32 cq_cid;
	u8 is_xrc;
	u8 reserved_key_en;
	u8 padding[2];
};

struct pfvf_rdma_create_srq_resp_tlv {
	struct pfvf_tlv hdr;

	u16 srq_id;
	u8 padding[6];
};

struct vfpf_rdma_modify_srq_tlv {
	struct vfpf_first_tlv first_tlv;

	u32 wqe_limit;
	u16 srq_id;
	u8 is_xrc;
	u8 padding[1];
};

struct pfvf_rdma_modify_srq_resp_tlv {
	struct pfvf_tlv hdr;
};

struct vfpf_rdma_destroy_srq_tlv {
	struct vfpf_first_tlv first_tlv;

	u16 srq_id;
	u8 is_xrc;
	u8 padding[5];
};

struct pfvf_rdma_destroy_srq_resp_tlv {
	struct pfvf_tlv hdr;
};

struct vfpf_rdma_query_port_tlv {
	struct vfpf_first_tlv first_tlv;
	u32 reserved1;
	u32 reserved2;
};

struct pfvf_rdma_query_port_resp_tlv {
	struct pfvf_tlv hdr;

	enum qed_port_state port_state;
	int link_speed;
	u64 max_msg_size;

	u32 reserved1;
	u32 reserved2;
};

struct vfpf_rdma_query_device_tlv {
	struct vfpf_first_tlv first_tlv;
	u32 reserved1;
	u32 reserved2;
};

struct pfvf_rdma_query_device_resp_tlv {
	struct pfvf_tlv hdr;

	/* Vendor specific information */
	u32 vendor_id;
	u32 vendor_part_id;
	u64 fw_ver;
	u32 hw_ver;

	u16 max_inline;
	u8 max_cnq;
	u8 max_sge;
	u32 max_wqe;
	u32 max_srq_wqe;
	u64 max_dev_resp_rd_atomic_resc;
	u8 max_qp_resp_rd_atomic_resc;
	u8 max_qp_req_rd_atomic_resc;
	u8 max_pkey;
	u8 max_srq_sge;
	u32 max_cq;
	u32 max_qp;
	u32 max_srq;
	u64 max_mr_size;
	u32 max_mr;
	u32 max_cqe;
	u32 max_mw;
	u32 max_fmr;
	u64 max_mr_mw_fmr_size;
	u32 max_mr_mw_fmr_pbl;
	u32 max_pd;
	u32 max_ah;
	u32 srq_limit;
	u16 max_srq_wr;
	u8 dev_ack_delay;
	u8 max_stats_queues;
	u32 bad_pkey_counter;
	u64 page_size_caps;
	u32 reserved_lkey;
	u8 padding[4];
};

struct vfpf_async_event_req_tlv {
	struct vfpf_first_tlv first_tlv;
	u32 reserved1;
	u32 reserved2;
};

#define QED_PFVF_EQ_COUNT       32

struct pfvf_async_event_resp_tlv {
	struct pfvf_tlv hdr;

	struct event_ring_entry eqs[QED_PFVF_EQ_COUNT];
	u8 eq_count;
	u8 padding[7];
};

union vfpf_tlvs {
	struct vfpf_first_tlv first_tlv;
	struct vfpf_acquire_tlv acquire;
	struct vfpf_start_rxq_tlv start_rxq;
	struct vfpf_start_txq_tlv start_txq;
	struct vfpf_stop_rxqs_tlv stop_rxqs;
	struct vfpf_stop_txqs_tlv stop_txqs;
	struct vfpf_update_rxq_tlv update_rxq;
	struct vfpf_vport_start_tlv start_vport;
	struct vfpf_vport_update_tlv vport_update;
	struct vfpf_ucast_filter_tlv ucast_filter;
	struct vfpf_update_tunn_param_tlv tunn_param_update;
	struct vfpf_update_coalesce update_coalesce;
	struct vfpf_read_coal_req_tlv read_coal_req;
	struct vfpf_bulletin_update_mac_tlv bulletin_update_mac;
	struct vfpf_update_mtu_tlv update_mtu;
	struct vfpf_est_ll2_conn_tlv establish_ll2;
	struct vfpf_terminate_ll2_conn_tlv terminate_ll2;
	struct vfpf_rdma_acquire_tlv rdma_acquire;
	struct vfpf_rdma_start_tlv rdma_start;
	struct vfpf_rdma_stop_tlv rdma_stop;
	struct vfpf_rdma_query_counters_tlv rdma_query_counters;
	struct vfpf_rdma_alloc_tid_tlv rdma_alloc_tid;
	struct vfpf_rdma_register_tid_tlv rdma_register_tid;
	struct vfpf_rdma_deregister_tid_tlv rdma_deregister_tid;
	struct vfpf_rdma_free_tid_tlv rdma_free_tid;
	struct vfpf_rdma_create_cq_tlv rdma_create_cq;
	struct vfpf_rdma_resize_cq_tlv rdma_resize_cq;
	struct vfpf_rdma_destroy_cq_tlv rdma_destroy_cq;
	struct vfpf_rdma_create_qp_tlv rdma_create_qp;
	struct vfpf_rdma_modify_qp_tlv rdma_modify_qp;
	struct vfpf_rdma_query_qp_tlv rdma_query_qp;
	struct vfpf_rdma_destroy_qp_tlv rdma_destroy_qp;
	struct vfpf_rdma_create_srq_tlv rdma_create_srq;
	struct vfpf_rdma_modify_srq_tlv rdma_modify_srq;
	struct vfpf_rdma_destroy_srq_tlv rdma_destroy_srq;
	struct vfpf_rdma_query_port_tlv rdma_query_port;
	struct vfpf_rdma_query_port_tlv rdma_query_device;
	struct vfpf_async_event_req_tlv async_event_req;
	struct vfpf_soft_flr_tlv vf_soft_flr;
	struct vfpf_filter_cfg_tlv filter_cfg;
	struct tlv_buffer_size tlv_buf_size;
};

union pfvf_tlvs {
	struct pfvf_def_resp_tlv default_resp;
	struct pfvf_acquire_resp_tlv acquire_resp;
	struct tlv_buffer_size tlv_buf_size;
	struct pfvf_start_queue_resp_tlv queue_start;
	struct pfvf_update_tunn_param_tlv tunn_param_resp;
	struct pfvf_read_coal_resp_tlv read_coal_resp;
	struct pfvf_est_ll2_conn_resp_tlv establish_ll2_resp;
	struct pfvf_rdma_acquire_resp_tlv rdma_acquire_resp;
	struct pfvf_rdma_query_counters_resp_tlv
	 rdma_query_counters_resp;
	struct pfvf_rdma_alloc_tid_resp_tlv rdma_alloc_tid_resp;
	struct pfvf_rdma_create_cq_resp_tlv rdma_create_cq_resp;
	struct pfvf_rdma_resize_cq_resp_tlv rdma_resize_cq_resp;
	struct pfvf_rdma_destroy_cq_resp_tlv rdma_destroy_cq_resp;
	struct pfvf_rdma_create_qp_resp_tlv rdma_create_qp_resp;
	struct pfvf_rdma_modify_qp_resp_tlv rdma_modify_qp_resp;
	struct pfvf_rdma_query_qp_resp_tlv rdma_query_qp_resp;
	struct pfvf_rdma_destroy_qp_resp_tlv rdma_destroy_qp_resp;
	struct pfvf_rdma_create_srq_resp_tlv rdma_create_srq_resp;
	struct pfvf_rdma_modify_srq_resp_tlv rdma_modify_srq_resp;
	struct pfvf_rdma_destroy_srq_resp_tlv rdma_destroy_srq_resp;
	struct pfvf_rdma_query_port_resp_tlv rdma_query_port_resp;
	struct pfvf_rdma_query_device_resp_tlv rdma_query_device_resp;
	struct pfvf_async_event_resp_tlv async_event_resp;
};

/* This is a structure which is allocated in the VF, which the PF may update
 * when it deems it necessary to do so. The bulletin board is sampled
 * periodically by the VF. A copy per VF is maintained in the PF (to prevent
 * loss of data upon multiple updates (or the need for read modify write)).
 */
enum qed_bulletin_bit {
	/* Alert the VF that a forced MAC was set by the PF */
	MAC_ADDR_FORCED = 0,

	/* The VF should not access the vfpf channel */
	VFPF_CHANNEL_INVALID = 1,

	/* Alert the VF that a forced VLAN was set by the PF */
	VLAN_ADDR_FORCED = 2,

	/* Indicate that `default_only_untagged' contains actual data */
	VFPF_BULLETIN_UNTAGGED_DEFAULT = 3,
	VFPF_BULLETIN_UNTAGGED_DEFAULT_FORCED = 4,

	/* Alert the VF that suggested mac was sent by the PF.
	 * MAC_ADDR will be disabled in case MAC_ADDR_FORCED is set
	 */
	VFPF_BULLETIN_MAC_ADDR = 5,
};

struct qed_bulletin_content {
	/* crc of structure to ensure is not in mid-update */
	u32 crc;

	u32 version;

	/* bitmap indicating which fields hold valid values */
	u64 valid_bitmap;

	/* used for MAC_ADDR or MAC_ADDR_FORCED */
	u8 mac[ETH_ALEN];

	/* If valid, 1 => only untagged Rx if no vlan is configured */
	u8 default_only_untagged;
	u8 padding;

	/* The following is a 'copy' of qed_mcp_link_state,
	 * qed_mcp_link_params and qed_mcp_link_capabilities. Since it's
	 * possible the structs will increase further along the road we cannot
	 * have it here; Instead we need to have all of its fields.
	 */
	u8 req_autoneg;
	u8 req_autoneg_pause;
	u8 req_forced_rx;
	u8 req_forced_tx;
	u8 padding2[4];

	u32 req_adv_speed;
	u32 req_forced_speed;
	u32 req_loopback;
	u32 padding3;

	u8 link_up;
	u8 full_duplex;
	u8 autoneg;
	u8 autoneg_complete;
	u8 parallel_detection;
	u8 pfc_enabled;
	u8 partner_tx_flow_ctrl_en;
	u8 partner_rx_flow_ctrl_en;

	u8 partner_adv_pause;
	u8 sfp_tx_fault;
	u16 vxlan_udp_port;
	u16 geneve_udp_port;

	/* Pending async events completions */
	u16 eq_completion;

	u32 speed;
	u32 partner_adv_speed;

	u32 capability_speed;

	/* Forced vlan */
	u16 pvid;

	u8 db_recovery_execute;
	u8 padding4;
};

struct qed_bulletin {
	dma_addr_t phys;
	struct qed_bulletin_content *p_virt;
	u32 size;
};

enum {
/*!!!!! Make sure to update STRINGS structure accordingly !!!!!*/
/*!!!!! Also make sure that new TLVs will be inserted last (before CHANNEL_TLV_MAX) !!!!!*/

	CHANNEL_TLV_NONE,	/* ends tlv sequence */
	CHANNEL_TLV_ACQUIRE,
	CHANNEL_TLV_VPORT_START,
	CHANNEL_TLV_VPORT_UPDATE,
	CHANNEL_TLV_VPORT_TEARDOWN,
	CHANNEL_TLV_START_RXQ,
	CHANNEL_TLV_START_TXQ,
	CHANNEL_TLV_STOP_RXQS,
	CHANNEL_TLV_STOP_TXQS,
	CHANNEL_TLV_UPDATE_RXQ,
	CHANNEL_TLV_INT_CLEANUP,
	CHANNEL_TLV_CLOSE,
	CHANNEL_TLV_RELEASE,
	CHANNEL_TLV_LIST_END,
	CHANNEL_TLV_UCAST_FILTER,
	CHANNEL_TLV_VPORT_UPDATE_ACTIVATE,
	CHANNEL_TLV_VPORT_UPDATE_TX_SWITCH,
	CHANNEL_TLV_VPORT_UPDATE_VLAN_STRIP,
	CHANNEL_TLV_VPORT_UPDATE_MCAST,
	CHANNEL_TLV_VPORT_UPDATE_ACCEPT_PARAM,
	CHANNEL_TLV_VPORT_UPDATE_RSS,
	CHANNEL_TLV_VPORT_UPDATE_ACCEPT_ANY_VLAN,
	CHANNEL_TLV_VPORT_UPDATE_SGE_TPA,
	CHANNEL_TLV_UPDATE_TUNN_PARAM,
	CHANNEL_TLV_COALESCE_UPDATE,
	CHANNEL_TLV_QID,
	CHANNEL_TLV_COALESCE_READ,
	CHANNEL_TLV_BULLETIN_UPDATE_MAC,
	CHANNEL_TLV_UPDATE_MTU,
	CHANNEL_TLV_RDMA_ACQUIRE,
	CHANNEL_TLV_RDMA_START,
	CHANNEL_TLV_RDMA_STOP,
	CHANNEL_TLV_RDMA_ADD_USER,
	CHANNEL_TLV_RDMA_REMOVE_USER,
	CHANNEL_TLV_RDMA_QUERY_COUNTERS,
	CHANNEL_TLV_RDMA_ALLOC_TID,
	CHANNEL_TLV_RDMA_REGISTER_TID,
	CHANNEL_TLV_RDMA_DEREGISTER_TID,
	CHANNEL_TLV_RDMA_FREE_TID,
	CHANNEL_TLV_RDMA_CREATE_CQ,
	CHANNEL_TLV_RDMA_RESIZE_CQ,
	CHANNEL_TLV_RDMA_DESTROY_CQ,
	CHANNEL_TLV_RDMA_CREATE_QP,
	CHANNEL_TLV_RDMA_MODIFY_QP,
	CHANNEL_TLV_RDMA_QUERY_QP,
	CHANNEL_TLV_RDMA_DESTROY_QP,
	CHANNEL_TLV_RDMA_QUERY_PORT,
	CHANNEL_TLV_RDMA_QUERY_DEVICE,
	CHANNEL_TLV_RDMA_IWARP_CONNECT,
	CHANNEL_TLV_RDMA_IWARP_ACCEPT,
	CHANNEL_TLV_RDMA_IWARP_CREATE_LISTEN,
	CHANNEL_TLV_RDMA_IWARP_DESTROY_LISTEN,
	CHANNEL_TLV_RDMA_IWARP_PAUSE_LISTEN,
	CHANNEL_TLV_RDMA_IWARP_REJECT,
	CHANNEL_TLV_RDMA_IWARP_SEND_RTR,
	CHANNEL_TLV_ESTABLISH_LL2_CONN,
	CHANNEL_TLV_TERMINATE_LL2_CONN,
	CHANNEL_TLV_ASYNC_EVENT,
	CHANNEL_TLV_RDMA_CREATE_SRQ,
	CHANNEL_TLV_RDMA_MODIFY_SRQ,
	CHANNEL_TLV_RDMA_DESTROY_SRQ,
	CHANNEL_TLV_SOFT_FLR,
	CHANNEL_TLV_FILTER_CFG,
	CHANNEL_TLV_MAX,

	/* Required for iterating over vport-update tlvs.
	 * Will break in case non-sequential vport-update tlvs.
	 */
	CHANNEL_TLV_VPORT_UPDATE_MAX = CHANNEL_TLV_VPORT_UPDATE_SGE_TPA + 1,

/*!!!!! Make sure to update STRINGS structure accordingly !!!!!*/
};
extern const char *qed_channel_tlvs_string[];

static inline void
qed_vfpf_qp_to_channel_qp(struct qed_rdma_qp *p_qp,
			  struct vfpf_rdma_channel_qp *p_channel_qp)
{
	p_channel_qp->qp_handle_lo = le32_to_cpu(p_qp->qp_handle.lo);
	p_channel_qp->qp_handle_hi = le32_to_cpu(p_qp->qp_handle.hi);
	p_channel_qp->qp_handle_async_lo =
	    le32_to_cpu(p_qp->qp_handle_async.lo);
	p_channel_qp->qp_handle_async_hi =
	    le32_to_cpu(p_qp->qp_handle_async.hi);
	p_channel_qp->icid = p_qp->icid;
	p_channel_qp->qp_idx = p_qp->qp_idx;
	p_channel_qp->cur_state = p_qp->cur_state;
	p_channel_qp->qp_type = p_qp->qp_type;
	p_channel_qp->use_srq = p_qp->use_srq;
	p_channel_qp->signal_all = p_qp->signal_all;
	p_channel_qp->fmr_and_reserved_lkey = p_qp->fmr_and_reserved_lkey;

	p_channel_qp->incoming_rdma_read_en = p_qp->incoming_rdma_read_en;
	p_channel_qp->incoming_rdma_write_en = p_qp->incoming_rdma_write_en;
	p_channel_qp->incoming_atomic_en = p_qp->incoming_atomic_en;
	p_channel_qp->e2e_flow_control_en = p_qp->e2e_flow_control_en;

	p_channel_qp->pd = p_qp->pd;
	p_channel_qp->pkey = p_qp->pkey;
	p_channel_qp->dest_qp = p_qp->dest_qp;
	p_channel_qp->mtu = p_qp->mtu;
	p_channel_qp->srq_id = p_qp->srq_id;
	p_channel_qp->traffic_class_tos = p_qp->traffic_class_tos;
	p_channel_qp->hop_limit_ttl = p_qp->hop_limit_ttl;
	p_channel_qp->dpi = p_qp->dpi;
	p_channel_qp->flow_label = p_qp->flow_label;
	p_channel_qp->vlan_id = p_qp->vlan_id;
	p_channel_qp->ack_timeout = p_qp->ack_timeout;
	p_channel_qp->retry_cnt = p_qp->retry_cnt;
	p_channel_qp->rnr_retry_cnt = p_qp->rnr_retry_cnt;
	p_channel_qp->min_rnr_nak_timer = p_qp->min_rnr_nak_timer;
	p_channel_qp->sqd_async = p_qp->sqd_async;
	memcpy(p_channel_qp->sgid.bytes, p_qp->sgid.bytes,
	       sizeof(p_channel_qp->sgid));
	memcpy(p_channel_qp->dgid.bytes, p_qp->dgid.bytes,
	       sizeof(p_channel_qp->dgid));
	p_channel_qp->roce_mode = p_qp->roce_mode;
	p_channel_qp->udp_src_port = p_qp->udp_src_port;
	p_channel_qp->stats_queue = p_qp->stats_queue;

	p_channel_qp->max_rd_atomic_req = p_qp->max_rd_atomic_req;
	p_channel_qp->sq_psn = p_qp->sq_psn;
	p_channel_qp->sq_cq_id = p_qp->sq_cq_id;
	p_channel_qp->sq_num_pages = p_qp->sq_num_pages;
	p_channel_qp->sq_pbl_ptr = p_qp->sq_pbl_ptr;
	p_channel_qp->orq_phys_addr = p_qp->orq_phys_addr;
	p_channel_qp->orq_num_pages = p_qp->orq_num_pages;
	p_channel_qp->req_offloaded = p_qp->req_offloaded;
	p_channel_qp->has_req = p_qp->has_req;

	p_channel_qp->max_rd_atomic_resp = p_qp->max_rd_atomic_resp;
	p_channel_qp->rq_psn = p_qp->rq_psn;
	p_channel_qp->rq_cq_id = p_qp->rq_cq_id;
	p_channel_qp->rq_num_pages = p_qp->rq_num_pages;
	p_channel_qp->rq_pbl_ptr = p_qp->rq_pbl_ptr;
	p_channel_qp->irq_phys_addr = p_qp->irq_phys_addr;
	p_channel_qp->irq_num_pages = p_qp->irq_num_pages;
	p_channel_qp->resp_offloaded = p_qp->resp_offloaded;
	p_channel_qp->has_resp = p_qp->has_resp;
	p_channel_qp->cq_prod_req = p_qp->cq_prod.req;
	p_channel_qp->cq_prod_resp = p_qp->cq_prod.resp;

	memcpy(p_channel_qp->remote_mac_addr, p_qp->remote_mac_addr, ETH_ALEN);
	memcpy(p_channel_qp->local_mac_addr, p_qp->local_mac_addr, ETH_ALEN);

	p_channel_qp->xrcd_id = p_qp->xrcd_id;
	p_channel_qp->edpm_mode = p_qp->edpm_mode;
	p_channel_qp->force_lb = p_qp->force_lb;
}

static inline void
qed_vfpf_channel_qp_to_qp(struct qed_rdma_qp *p_qp,
			  struct vfpf_rdma_channel_qp *p_channel_qp)
{
	p_qp->qp_handle.lo = cpu_to_le32(p_channel_qp->qp_handle_lo);
	p_qp->qp_handle.hi = cpu_to_le32(p_channel_qp->qp_handle_hi);
	p_qp->qp_handle_async.lo =
	    cpu_to_le32(p_channel_qp->qp_handle_async_lo);
	p_qp->qp_handle_async.hi =
	    cpu_to_le32(p_channel_qp->qp_handle_async_hi);
	p_qp->icid = p_channel_qp->icid;
	p_qp->qp_idx = p_channel_qp->qp_idx;
	p_qp->cur_state = p_channel_qp->cur_state;
	p_qp->qp_type = p_channel_qp->qp_type;
	p_qp->use_srq = p_channel_qp->use_srq;
	p_qp->signal_all = p_channel_qp->signal_all;
	p_qp->fmr_and_reserved_lkey = p_channel_qp->fmr_and_reserved_lkey;

	p_qp->incoming_rdma_read_en = p_channel_qp->incoming_rdma_read_en;
	p_qp->incoming_rdma_write_en = p_channel_qp->incoming_rdma_write_en;
	p_qp->incoming_atomic_en = p_channel_qp->incoming_atomic_en;
	p_qp->e2e_flow_control_en = p_channel_qp->e2e_flow_control_en;

	p_qp->pd = p_channel_qp->pd;
	p_qp->pkey = p_channel_qp->pkey;
	p_qp->dest_qp = p_channel_qp->dest_qp;
	p_qp->mtu = p_channel_qp->mtu;
	p_qp->srq_id = p_channel_qp->srq_id;
	p_qp->traffic_class_tos = p_channel_qp->traffic_class_tos;
	p_qp->hop_limit_ttl = p_channel_qp->hop_limit_ttl;
	p_qp->dpi = p_channel_qp->dpi;
	p_qp->flow_label = p_channel_qp->flow_label;
	p_qp->vlan_id = p_channel_qp->vlan_id;
	p_qp->ack_timeout = p_channel_qp->ack_timeout;
	p_qp->retry_cnt = p_channel_qp->retry_cnt;
	p_qp->rnr_retry_cnt = p_channel_qp->rnr_retry_cnt;
	p_qp->min_rnr_nak_timer = p_channel_qp->min_rnr_nak_timer;
	p_qp->sqd_async = p_channel_qp->sqd_async;
	memcpy(p_qp->sgid.bytes, p_channel_qp->sgid.bytes,
	       sizeof(p_channel_qp->sgid));
	memcpy(p_qp->dgid.bytes, p_channel_qp->dgid.bytes,
	       sizeof(p_channel_qp->dgid));
	p_qp->roce_mode = p_channel_qp->roce_mode;
	p_qp->udp_src_port = p_channel_qp->udp_src_port;
	p_qp->stats_queue = p_channel_qp->stats_queue;

	p_qp->max_rd_atomic_req = p_channel_qp->max_rd_atomic_req;
	p_qp->sq_psn = p_channel_qp->sq_psn;
	p_qp->sq_cq_id = p_channel_qp->sq_cq_id;
	p_qp->sq_num_pages = p_channel_qp->sq_num_pages;
	p_qp->sq_pbl_ptr = p_channel_qp->sq_pbl_ptr;
	p_qp->orq_phys_addr = p_channel_qp->orq_phys_addr;
	p_qp->orq_num_pages = p_channel_qp->orq_num_pages;
	p_qp->req_offloaded = p_channel_qp->req_offloaded;
	p_qp->has_req = p_channel_qp->has_req;

	p_qp->max_rd_atomic_resp = p_channel_qp->max_rd_atomic_resp;
	p_qp->rq_psn = p_channel_qp->rq_psn;
	p_qp->rq_cq_id = p_channel_qp->rq_cq_id;
	p_qp->rq_num_pages = p_channel_qp->rq_num_pages;
	p_qp->rq_pbl_ptr = p_channel_qp->rq_pbl_ptr;
	p_qp->irq_phys_addr = p_channel_qp->irq_phys_addr;
	p_qp->irq_num_pages = p_channel_qp->irq_num_pages;
	p_qp->resp_offloaded = p_channel_qp->resp_offloaded;
	p_qp->has_resp = p_channel_qp->has_resp;
	p_qp->cq_prod.req = p_channel_qp->cq_prod_req;
	p_qp->cq_prod.resp = p_channel_qp->cq_prod_resp;

	memcpy(p_qp->remote_mac_addr, p_channel_qp->remote_mac_addr, ETH_ALEN);
	memcpy(p_qp->local_mac_addr, p_channel_qp->local_mac_addr, ETH_ALEN);

	p_qp->xrcd_id = p_channel_qp->xrcd_id;
	p_qp->edpm_mode = p_channel_qp->edpm_mode;
	p_qp->force_lb = p_channel_qp->force_lb;
}

#ifdef CONFIG_QED_SRIOV

#define QED_VF_ACQUIRE_THRESH 3

/**
 * @brief Read the VF bulletin and act on it if needed
 *
 * @param p_hwfn
 * @param p_change - qed fills 1 iff bulletin board has changed, 0 otherwise.
 *
 * @return enum _qed_status
 */
int qed_vf_read_bulletin(struct qed_hwfn *p_hwfn, u8 * p_change);

/**
 * @brief Get link paramters for VF from qed
 *
 * @param p_hwfn
 * @param params - the link params structure to be filled for the VF
 */
void qed_vf_get_link_params(struct qed_hwfn *p_hwfn,
			    struct qed_mcp_link_params *params);

/**
 * @brief Get link state for VF from qed
 *
 * @param p_hwfn
 * @param link - the link state structure to be filled for the VF
 */
void qed_vf_get_link_state(struct qed_hwfn *p_hwfn,
			   struct qed_mcp_link_state *link);

/**
 * @brief Get link capabilities for VF from qed
 *
 * @param p_hwfn
 * @param p_link_caps - the link capabilities structure to be filled for the VF
 */
void qed_vf_get_link_caps(struct qed_hwfn *p_hwfn,
			  struct qed_mcp_link_capabilities *p_link_caps);

/**
 * @brief Get number of status blocks allocated for VF by qed
 *
 *  @param p_hwfn
 *  @param num_sbs - allocated status blocks
 */
void qed_vf_get_num_sbs(struct qed_hwfn *p_hwfn, u8 * num_sbs);

/**
 * @brief Get number of Rx queues allocated for VF by qed
 *
 *  @param p_hwfn
 *  @param num_rxqs - allocated RX queues
 */
void qed_vf_get_num_rxqs(struct qed_hwfn *p_hwfn, u8 * num_rxqs);

/**
 * @brief Get number of Rx queues allocated for VF by qed
 *
 *  @param p_hwfn
 *  @param num_txqs - allocated RX queues
 */
void qed_vf_get_num_txqs(struct qed_hwfn *p_hwfn, u8 * num_txqs);

/**
 * @brief Get number of available connections [both Rx and Tx] for VF
 *
 * @param p_hwfn
 * @param num_cids - allocated number of connections
 */
void qed_vf_get_num_cids(struct qed_hwfn *p_hwfn, u8 * num_cids);

/**
 * @brief Get number of cnqs allocated for VF by qed
 *
 * @param p_hwfn
 * @param num_cnqs - allocated cnqs
 */
void qed_vf_get_num_cnqs(struct qed_hwfn *p_hwfn, u8 * num_cnqs);

/**
 * @brief Returns the relative sb_id for the first CNQ
 *
 * @param p_hwfn
 */
u8 qed_vf_rdma_cnq_sb_start_id(struct qed_hwfn *p_hwfn);

/**
 * @brief Get port mac address for VF
 *
 * @param p_hwfn
 * @param port_mac - destination location for port mac
 */
void qed_vf_get_port_mac(struct qed_hwfn *p_hwfn, u8 * port_mac);

/**
 * @brief Get number of VLAN filters allocated for VF by qed
 *
 *  @param p_hwfn
 *  @param num_rxqs - allocated VLAN filters
 */
void qed_vf_get_num_vlan_filters(struct qed_hwfn *p_hwfn,
				 u8 * num_vlan_filters);

/**
 * @brief Get number of MAC filters allocated for VF by qed
 *
 *  @param p_hwfn
 *  @param num_rxqs - allocated MAC filters
 */
void qed_vf_get_num_mac_filters(struct qed_hwfn *p_hwfn, u8 * num_mac_filters);

/**
 * @brief Check if VF can set a MAC address
 *
 * @param p_hwfn
 * @param mac
 *
 * @return bool
 */
bool qed_vf_check_mac(struct qed_hwfn *p_hwfn, u8 * mac);

/**
 * @brief Init the admin mac for the VF from the bulletin
 *        without updating VF shadow copy of bulletin
 *
 * @param p_hwfn
 * @param p_mac
 *
 * @return void
 */
void qed_vf_init_admin_mac(struct qed_hwfn *p_hwfn, u8 * p_mac);

/**
 * @brief Set firmware version information in dev_info from VFs acquire response tlv
 *
 * @param p_hwfn
 * @param fw_major
 * @param fw_minor
 * @param fw_rev
 * @param fw_eng
 */
void qed_vf_get_fw_version(struct qed_hwfn *p_hwfn,
			   u16 * fw_major,
			   u16 * fw_minor, u16 * fw_rev, u16 * fw_eng);
void qed_vf_bulletin_get_udp_ports(struct qed_hwfn *p_hwfn,
				   u16 * p_vxlan_port, u16 * p_geneve_port);
void qed_vf_update_mac(struct qed_hwfn *p_hwfn, u8 * mac);

#else
static inline int qed_vf_read_bulletin(struct qed_hwfn __maybe_unused * p_hwfn,
				       u8 __maybe_unused * p_change)
{
	return -EINVAL;
}

static inline void qed_vf_get_link_params(struct qed_hwfn __maybe_unused *
					  p_hwfn,
					  struct qed_mcp_link_params
					  __maybe_unused * params)
{
}

static inline void qed_vf_get_link_state(struct qed_hwfn __maybe_unused *
					 p_hwfn,
					 struct qed_mcp_link_state
					 __maybe_unused * link)
{
}

static inline void qed_vf_get_link_caps(struct qed_hwfn __maybe_unused * p_hwfn,
					struct qed_mcp_link_capabilities
					__maybe_unused * p_link_caps)
{
}

static inline void qed_vf_get_num_sbs(struct qed_hwfn __maybe_unused * p_hwfn,
				      u8 __maybe_unused * num_sbs)
{
}

static inline void qed_vf_get_num_rxqs(struct qed_hwfn __maybe_unused * p_hwfn,
				       u8 __maybe_unused * num_rxqs)
{
}

static inline void qed_vf_get_num_txqs(struct qed_hwfn __maybe_unused * p_hwfn,
				       u8 __maybe_unused * num_txqs)
{
}

static inline void qed_vf_get_num_cids(struct qed_hwfn __maybe_unused * p_hwfn,
				       u8 __maybe_unused * num_cids)
{
}

static inline void qed_vf_get_num_cnqs(struct qed_hwfn __maybe_unused * p_hwfn,
				       u8 __maybe_unused * num_cnqs)
{
}

static inline u8 qed_vf_rdma_cnq_sb_start_id(struct qed_hwfn __maybe_unused *
					     p_hwfn)
{
	return 0;
}

static inline void qed_vf_get_port_mac(struct qed_hwfn __maybe_unused * p_hwfn,
				       u8 __maybe_unused * port_mac)
{
}

static inline void qed_vf_get_num_vlan_filters(struct qed_hwfn __maybe_unused *
					       p_hwfn,
					       u8 __maybe_unused *
					       num_vlan_filters)
{
}

static inline void qed_vf_get_num_mac_filters(struct qed_hwfn __maybe_unused *
					      p_hwfn,
					      u8 __maybe_unused *
					      num_mac_filters)
{
}

static inline bool qed_vf_check_mac(struct qed_hwfn __maybe_unused * p_hwfn,
				    u8 __maybe_unused * mac)
{
	return false;
}

static inline void qed_vf_init_admin_mac(struct qed_hwfn __maybe_unused *
					 p_hwfn, u8 __maybe_unused * mac)
{
}

static inline void qed_vf_get_fw_version(struct qed_hwfn __maybe_unused *
					 p_hwfn, u16 __maybe_unused * fw_major,
					 u16 __maybe_unused * fw_minor,
					 u16 __maybe_unused * fw_rev,
					 u16 __maybe_unused * fw_eng)
{
}

static inline void qed_vf_bulletin_get_udp_ports(struct qed_hwfn __maybe_unused
						 * p_hwfn,
						 u16 __maybe_unused *
						 p_vxlan_port,
						 u16 __maybe_unused *
						 p_geneve_port)
{
	return;
}

static inline void qed_vf_update_mac(struct qed_hwfn __maybe_unused * p_hwfn,
				     u8 __maybe_unused * mac)
{
}

#endif

/* Default number of CIDs [total of both Rx and Tx] to be requested
 * by default, and maximum possible number.
 */
#define QED_ETH_VF_DEFAULT_NUM_CIDS     (32)
#define QED_ETH_VF_MAX_NUM_CIDS (255)
#define QED_VF_RDMA_DEFAULT_CNQS  4

/* This data is held in the qed_hwfn structure for VFs only. */
struct qed_vf_iov {
	union vfpf_tlvs *vf2pf_request;
	dma_addr_t vf2pf_request_phys;
	union pfvf_tlvs *pf2vf_reply;
	dma_addr_t pf2vf_reply_phys;

	/* Should be taken whenever the mailbox buffers are accessed */
	struct mutex mutex;
	u8 *offset;

	/* Bulletin Board */
	struct qed_bulletin bulletin;
	struct qed_bulletin_content bulletin_shadow;

	/* we set aside a copy of the acquire response */
	struct pfvf_acquire_resp_tlv acquire_resp;
	struct pfvf_rdma_acquire_resp_tlv rdma_acquire_resp;

	/* In case PF originates prior to the fp-hsi version comparison,
	 * this has to be propagated as it affects the fastpath.
	 */
	bool b_pre_fp_hsi;

	/* Current day VFs are passing the SBs physical address on vport
	 * start, and as they lack an IGU mapping they need to store the
	 * addresses of previously registered SBs.
	 * Even if we were to change configuration flow, due to backward
	 * compatability [with older PFs] we'd still need to store these.
	 */
	struct qed_sb_info *sbs_info[PFVF_MAX_SBS_PER_VF];

	/* Determines whether VF utilizes doorbells via limited register
	 * bar or via the doorbell bar.
	 */
	bool b_doorbell_bar;

	/* retry count for VF acquire on channel timeout */
	u8 acquire_retry_cnt;

	/* Compare this value with bulletin board's db_recovery_execute to know
	 * whether doorbell overflow recovery is needed.
	 */
	u8 db_recovery_execute_prev;

	/* Compare this value with bulletin board's eq_completion to know
	 * whether new async events completions are pending.
	 */
	u16 eq_completion_prev;

	u8 mac_addr[ETH_ALEN];
};

#ifdef CONFIG_QED_SRIOV
/**
 * @brief VF - Get coalesce per VF's relative queue.
 *
 * @param p_hwfn
 * @param p_coal - coalesce value in micro second for VF queues.
 * @param p_cid  - queue cid
 *
 **/
int qed_vf_pf_get_coalesce(struct qed_hwfn *p_hwfn,
			   u16 * p_coal, struct qed_queue_cid *p_cid);

/**
 * @brief VF - Set Rx/Tx coalesce per VF's relative queue.
 *             Coalesce value '0' will omit the configuration.
 *
 * @param p_hwfn
 * @param rx_coal - coalesce value in micro second for rx queue
 * @param tx_coal - coalesce value in micro second for tx queue
 * @param p_cid   - queue cid
 *
 **/
int qed_vf_pf_set_coalesce(struct qed_hwfn *p_hwfn,
			   u16 rx_coal,
			   u16 tx_coal, struct qed_queue_cid *p_cid);

/**
 * @brief hw preparation for VF
 *	sends ACQUIRE message
 *
 * @param p_hwfn
 * @param p_params
 *
 * @return int
 */
int
qed_vf_hw_prepare(struct qed_hwfn *p_hwfn,
		  struct qed_hw_prepare_params *p_params);

/**
 * @brief VF - start the RX Queue by sending a message to the PF
 *
 * @param p_hwfn
 * @param p_cid			- Only relative fields are relevant
 * @param bd_max_bytes          - maximum number of bytes per bd
 * @param bd_chain_phys_addr    - physical address of bd chain
 * @param cqe_pbl_addr          - physical address of pbl
 * @param cqe_pbl_size          - pbl size
 * @param pp_prod               - pointer to the producer to be
 *				  used in fasthpath
 *
 * @return int
 */
int qed_vf_pf_rxq_start(struct qed_hwfn *p_hwfn,
			struct qed_queue_cid *p_cid,
			u16 bd_max_bytes,
			dma_addr_t bd_chain_phys_addr,
			dma_addr_t cqe_pbl_addr,
			u16 cqe_pbl_size, void __iomem ** pp_prod);

/**
 * @brief VF - start the TX queue by sending a message to the
 *        PF.
 *
 * @param p_hwfn
 * @param p_cid
 * @param bd_chain_phys_addr    - physical address of tx chain
 * @param pp_doorbell           - pointer to address to which to
 *                      write the doorbell too..
 *
 * @return int
 */
int
qed_vf_pf_txq_start(struct qed_hwfn *p_hwfn,
		    struct qed_queue_cid *p_cid,
		    u8 tc,
		    dma_addr_t pbl_addr,
		    u16 pbl_size, void __iomem ** pp_doorbell);

/**
 * @brief VF - stop the RX queue by sending a message to the PF
 *
 * @param p_hwfn
 * @param p_cid
 * @param cqe_completion
 *
 * @return int
 */
int qed_vf_pf_rxq_stop(struct qed_hwfn *p_hwfn,
		       struct qed_queue_cid *p_cid, bool cqe_completion);

/**
 * @brief VF - stop the TX queue by sending a message to the PF
 *
 * @param p_hwfn
 * @param p_cid
 *
 * @return int
 */
int qed_vf_pf_txq_stop(struct qed_hwfn *p_hwfn, struct qed_queue_cid *p_cid);

/* TODO - fix all the !SRIOV prototypes */

/**
 * @brief VF - send a vport update command
 *
 * @param p_hwfn
 * @param params
 *
 * @return int
 */
int qed_vf_pf_vport_update(struct qed_hwfn *p_hwfn,
			   struct qed_sp_vport_update_params *p_params);

/**
 * @brief VF - send a close message to PF
 *
 * @param p_hwfn
 *
 * @return enum _qed_status
 */
int qed_vf_pf_reset(struct qed_hwfn *p_hwfn);

/**
 * @brief VF - free vf`s memories
 *
 * @param p_hwfn
 *
 * @return enum _qed_status
 */
int qed_vf_pf_release(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_vf_get_igu_sb_id - Get the IGU SB ID for a given
 *        sb_id. For VFs igu sbs don't have to be contiguous
 *
 * @param p_hwfn
 * @param sb_id
 *
 * @return INLINE u16
 */
u16 qed_vf_get_igu_sb_id(struct qed_hwfn *p_hwfn, u16 sb_id);

/**
 * @brief Stores [or removes] a configured sb_info.
 *
 * @param p_hwfn
 * @param sb_id - zero-based SB index [for fastpath]
 * @param sb_info - may be NULL [during removal].
 */
void qed_vf_set_sb_info(struct qed_hwfn *p_hwfn,
			u16 sb_id, struct qed_sb_info *p_sb);

/**
 * @brief qed_vf_pf_vport_start - perform vport start for VF.
 *
 * @param p_hwfn
 * @param vport_id
 * @param mtu
 * @param inner_vlan_removal
 * @param tpa_mode
 * @param max_buffers_per_cqe,
 * @param only_untagged - default behavior regarding vlan acceptance
 * @param zero_placement_offset - if set, zero padding will be inserted
 *
 * @return enum _qed_status
 */
int qed_vf_pf_vport_start(struct qed_hwfn *p_hwfn,
			  u8 vport_id,
			  u16 mtu,
			  u8 inner_vlan_removal,
			  enum qed_tpa_mode tpa_mode,
			  u8 max_buffers_per_cqe,
			  u8 only_untagged, u8 zero_placement_offset);

/**
 * @brief qed_vf_pf_vport_stop - stop the VF's vport
 *
 * @param p_hwfn
 *
 * @return enum _qed_status
 */
int qed_vf_pf_vport_stop(struct qed_hwfn *p_hwfn);

int qed_vf_pf_filter_ucast(struct qed_hwfn *p_hwfn,
			   struct qed_filter_ucast *p_param);

void qed_vf_pf_filter_mcast(struct qed_hwfn *p_hwfn,
			    struct qed_filter_mcast *p_filter_cmd);

int
qed_vf_pf_filter_cfg(struct qed_hwfn *p_hwfn,
		     struct qed_ntuple_filter_params *params);

/**
 * @brief qed_vf_pf_int_cleanup - clean the SB of the VF
 *
 * @param p_hwfn
 *
 * @return enum _qed_status
 */
int qed_vf_pf_int_cleanup(struct qed_hwfn *p_hwfn);

/**
 * @brief - return the link params in a given bulletin board
 *
 * @param p_params - pointer to a struct to fill with link params
 * @param p_bulletin
 */
void __qed_vf_get_link_params(struct qed_mcp_link_params *p_params,
			      struct qed_bulletin_content *p_bulletin);

/**
 * @brief - return the link state in a given bulletin board
 *
 * @param p_link - pointer to a struct to fill with link state
 * @param p_bulletin
 */
void __qed_vf_get_link_state(struct qed_mcp_link_state *p_link,
			     struct qed_bulletin_content *p_bulletin);

/**
 * @brief - return the link capabilities in a given bulletin board
 *
 * @param p_link - pointer to a struct to fill with link capabilities
 * @param p_bulletin
 */
void __qed_vf_get_link_caps(struct qed_mcp_link_capabilities *p_link_caps,
			    struct qed_bulletin_content *p_bulletin);
int
qed_vf_pf_tunnel_param_update(struct qed_hwfn *p_hwfn,
			      struct qed_tunnel_info *p_tunn);
void qed_vf_set_vf_start_tunn_update_param(struct qed_tunnel_info *p_tun);

u32 qed_vf_hw_bar_size(struct qed_hwfn *p_hwfn, enum BAR_ID bar_id);
/**
 * @brief - Ask PF to update the MAC address in it's bulletin board
 *
 * @param p_mac - mac address to be updated in bulletin board
 */
int qed_vf_pf_bulletin_update_mac(struct qed_hwfn *p_hwfn, const u8 * p_mac);
/**
 * @brief - qed_vf_pf_update_mtu Update MTU for VF.
 *          vport should not be active before issuing this function.
 *
 * @param - mtu
 */
int qed_vf_pf_update_mtu(struct qed_hwfn *p_hwfn, u16 mtu);

/**
 * @brief - qed_vf_pf_establish_ll2_connection establishes ll2 connection
 *	    (opens tx and rx ll2 queues) for the VF
 *
 * @param - connection_handle - LL2 connection handle (that was obtained from
 *				ll2_acquire_connection)
 */
int qed_vf_pf_establish_ll2_conn(struct qed_hwfn *p_hwfn, u8 connection_handle);

/**
 * @brief - qed_vf_pf_terminate_ll2 terminates the ll2 connection
 *	    (stops tx and rx queues) for the VF
 *
 * @param - conn_handle	 - LL2 connection handle (that was obtained from
 *			   ll2_acquire_connection)
 */
int qed_vf_pf_terminate_ll2_conn(struct qed_hwfn *p_hwfn, u8 conn_handle);

int
qed_vf_pf_rdma_start(struct qed_hwfn *p_hwfn,
		     struct qed_rdma_start_in_params *params,
		     struct qed_rdma_info *p_rdma_info);

int qed_vf_pf_rdma_stop(struct qed_hwfn *p_hwfn);

int qed_vf_pf_rdma_add_user(struct qed_hwfn *p_hwfn);

int qed_vf_pf_rdma_remove_user(struct qed_hwfn *p_hwfn);

int qed_vf_pf_rdma_query_counters(struct qed_hwfn *p_hwfn, struct qed_rdma_counters_out_params
				  *out_params);

int qed_vf_pf_rdma_alloc_tid(struct qed_hwfn *p_hwfn, u32 * p_tid);

int qed_vf_pf_rdma_register_tid(struct qed_hwfn *p_hwfn, struct qed_rdma_register_tid_in_params
				*params);

int qed_vf_pf_rdma_deregister_tid(struct qed_hwfn *p_hwfn, u32 tid);

int qed_vf_pf_rdma_free_tid(struct qed_hwfn *p_hwfn, u32 tid);

int
qed_vf_pf_rdma_create_cq(struct qed_hwfn *p_hwfn,
			 struct qed_rdma_create_cq_in_params *in_params,
			 u16 * icid);

int
qed_vf_pf_rdma_resize_cq(struct qed_hwfn *p_hwfn,
			 struct qed_rdma_resize_cq_in_params *in_params,
			 struct qed_rdma_resize_cq_out_params *out_params);

int
qed_vf_pf_rdma_destroy_cq(struct qed_hwfn *p_hwfn,
			  struct qed_rdma_destroy_cq_in_params *in_params,
			  struct qed_rdma_destroy_cq_out_params
			  *out_params);

struct qed_rdma_qp *qed_vf_pf_rdma_create_qp(struct qed_hwfn *p_hwfn,
					     struct qed_rdma_create_qp_in_params
					     *in_params,
					     struct
					     qed_rdma_create_qp_out_params
					     *out_params);

int
qed_vf_pf_rdma_modify_qp(struct qed_hwfn *p_hwfn,
			 struct qed_rdma_qp *qp,
			 struct qed_rdma_modify_qp_in_params *in_params);

int
qed_vf_pf_rdma_query_qp(struct qed_hwfn *p_hwfn,
			struct qed_rdma_qp *qp,
			struct qed_rdma_query_qp_out_params *out_params);

int qed_vf_pf_rdma_destroy_qp(struct qed_hwfn *p_hwfn, struct qed_rdma_qp *qp, struct qed_rdma_destroy_qp_out_params
			      *out_params);

int
qed_vf_pf_rdma_create_srq(struct qed_hwfn *p_hwfn,
			  struct qed_rdma_create_srq_in_params *in_params,
			  struct qed_rdma_create_srq_out_params *out_params);

int
qed_vf_pf_rdma_modify_srq(struct qed_hwfn *p_hwfn,
			  struct qed_rdma_modify_srq_in_params *in_params);

int
qed_vf_pf_rdma_destroy_srq(struct qed_hwfn *p_hwfn,
			   struct qed_rdma_destroy_srq_in_params *in_params);

struct qed_rdma_port *qed_vf_pf_rdma_query_port(struct qed_hwfn *p_hwfn);

int qed_vf_pf_rdma_query_device(struct qed_hwfn *p_hwfn);

int qed_vf_pf_acquire(struct qed_hwfn *p_hwfn);
#else
static inline int qed_vf_pf_get_coalesce(struct qed_hwfn __maybe_unused *
					 p_hwfn, u16 __maybe_unused * p_coal,
					 struct qed_queue_cid __maybe_unused *
					 p_cid)
{
	return -EINVAL;
}

static inline int qed_vf_pf_set_coalesce(struct qed_hwfn __maybe_unused *
					 p_hwfn, u16 __maybe_unused rx_coal,
					 u16 __maybe_unused tx_coal,
					 struct qed_queue_cid __maybe_unused *
					 p_cid)
{
	return -EINVAL;
}

static inline int qed_vf_hw_prepare(struct qed_hwfn __maybe_unused * p_hwfn,
				    struct qed_hw_prepare_params __maybe_unused
				    * p_params)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rxq_start(struct qed_hwfn __maybe_unused * p_hwfn,
				      struct qed_queue_cid __maybe_unused
				      * p_cid,
				      u16 __maybe_unused
				      bd_max_bytes,
				      dma_addr_t __maybe_unused
				      bd_chain_phys_addr,
				      dma_addr_t __maybe_unused
				      cqe_pbl_addr,
				      u16 __maybe_unused
				      cqe_pbl_size,
				      void __iomem __maybe_unused ** pp_prod)
{
	return -EINVAL;
}

static inline int qed_vf_pf_txq_start(struct qed_hwfn __maybe_unused * p_hwfn,
				      struct qed_queue_cid __maybe_unused
				      * p_cid,
				      u8 __maybe_unused
				      tc,
				      dma_addr_t __maybe_unused
				      pbl_addr,
				      u16 __maybe_unused
				      pbl_size,
				      void __iomem __maybe_unused
				      ** pp_doorbell)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rxq_stop(struct qed_hwfn __maybe_unused * p_hwfn,
				     struct qed_queue_cid __maybe_unused
				     * p_cid,
				     bool __maybe_unused cqe_completion)
{
	return -EINVAL;
}

static inline int qed_vf_pf_txq_stop(struct qed_hwfn __maybe_unused * p_hwfn,
				     struct qed_queue_cid __maybe_unused
				     * p_cid)
{
	return -EINVAL;
}

static inline int qed_vf_pf_vport_update(struct qed_hwfn __maybe_unused *
					 p_hwfn,
					 struct qed_sp_vport_update_params
					 __maybe_unused * p_params)
{
	return -EINVAL;
}

static inline int qed_vf_pf_reset(struct qed_hwfn __maybe_unused * p_hwfn)
{
	return -EINVAL;
}

static inline int qed_vf_pf_release(struct qed_hwfn __maybe_unused * p_hwfn)
{
	return -EINVAL;
}

static inline u16 qed_vf_get_igu_sb_id(struct qed_hwfn __maybe_unused * p_hwfn,
				       u16 __maybe_unused sb_id)
{
	return 0;
}

static inline void qed_vf_set_sb_info(struct qed_hwfn __maybe_unused * p_hwfn,
				      u16 __maybe_unused sb_id,
				      struct qed_sb_info __maybe_unused * p_sb)
{
}

static inline int qed_vf_pf_vport_start(struct qed_hwfn __maybe_unused * p_hwfn,
					u8 __maybe_unused
					vport_id,
					u16 __maybe_unused
					mtu,
					u8 __maybe_unused
					inner_vlan_removal,
					enum qed_tpa_mode __maybe_unused
					tpa_mode,
					u8 __maybe_unused
					max_buffers_per_cqe,
					u8 __maybe_unused
					only_untagged,
					u8 __maybe_unused zero_placement_offset)
{
	return -EINVAL;
}

static inline int qed_vf_pf_vport_stop(struct qed_hwfn __maybe_unused * p_hwfn)
{
	return -EINVAL;
}

static inline int qed_vf_pf_filter_ucast(struct qed_hwfn __maybe_unused *
					 p_hwfn,
					 struct qed_filter_ucast __maybe_unused
					 * p_param)
{
	return -EINVAL;
}

static inline void qed_vf_pf_filter_mcast(struct qed_hwfn __maybe_unused *
					  p_hwfn,
					  struct qed_filter_mcast __maybe_unused
					  * p_filter_cmd)
{
}

static inline int qed_vf_pf_int_cleanup(struct qed_hwfn __maybe_unused * p_hwfn)
{
	return -EINVAL;
}

static inline void __qed_vf_get_link_params(struct qed_mcp_link_params
					    __maybe_unused * p_params,
					    struct qed_bulletin_content
					    __maybe_unused * p_bulletin)
{
}

static inline void __qed_vf_get_link_state(struct qed_mcp_link_state
					   __maybe_unused * p_link,
					   struct qed_bulletin_content
					   __maybe_unused * p_bulletin)
{
}

static inline void __qed_vf_get_link_caps(struct qed_mcp_link_capabilities
					  __maybe_unused * p_link_caps,
					  struct qed_bulletin_content
					  __maybe_unused * p_bulletin)
{
}

static inline int qed_vf_pf_tunnel_param_update(struct qed_hwfn __maybe_unused *
						p_hwfn,
						struct qed_tunnel_info
						__maybe_unused * p_tunn)
{
	return -EINVAL;
}

static inline void qed_vf_set_vf_start_tunn_update_param(struct qed_tunnel_info
							 __maybe_unused * p_tun)
{
	return;
}

static inline int qed_vf_pf_bulletin_update_mac(struct qed_hwfn __maybe_unused *
						p_hwfn,
						const u8 __maybe_unused * p_mac)
{
	return -EINVAL;
}

static inline int qed_vf_pf_update_mtu(struct qed_hwfn __maybe_unused * p_hwfn,
				       u16 __maybe_unused mtu)
{
	return -EINVAL;
}

static inline int qed_vf_pf_filter_cfg(struct qed_hwfn __maybe_unused * p_hwfn,
				       struct qed_ntuple_filter_params
				       __maybe_unused * params)
{
	return -EINVAL;
}

static inline int qed_vf_pf_establish_ll2_conn(struct qed_hwfn __maybe_unused *
					       p_hwfn,
					       u8 __maybe_unused
					       connection_handle)
{
	return -EINVAL;
}

static inline int qed_vf_pf_terminate_ll2_conn(struct qed_hwfn __maybe_unused *
					       p_hwfn,
					       u8 __maybe_unused
					       connection_handle)
{
	return -EINVAL;
}

/* RDMA */
static inline int qed_vf_pf_rdma_acquire(struct qed_hwfn __maybe_unused *
					 p_hwfn)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rdma_start(struct qed_hwfn __maybe_unused * p_hwfn,
				       struct qed_rdma_start_in_params
				       __maybe_unused * params,
				       struct qed_rdma_info __maybe_unused
				       * p_rdma_info)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rdma_stop(struct qed_hwfn __maybe_unused * p_hwfn)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rdma_add_user(struct qed_hwfn __maybe_unused *
					  p_hwfn)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rdma_remove_user(struct qed_hwfn __maybe_unused *
					     p_hwfn)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rdma_query_counters(struct qed_hwfn __maybe_unused *
						p_hwfn, struct
						qed_rdma_counters_out_params
						__maybe_unused * out_params)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rdma_alloc_tid(struct qed_hwfn __maybe_unused *
					   p_hwfn, u32 __maybe_unused * p_tid)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rdma_register_tid(struct qed_hwfn __maybe_unused *
					      p_hwfn, struct
					      qed_rdma_register_tid_in_params
					      __maybe_unused * params)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rdma_deregister_tid(struct qed_hwfn __maybe_unused *
						p_hwfn, u32 __maybe_unused tid)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rdma_free_tid(struct qed_hwfn __maybe_unused *
					  p_hwfn, u32 __maybe_unused tid)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rdma_create_cq(struct qed_hwfn __maybe_unused *
					   p_hwfn,
					   struct qed_rdma_create_cq_in_params
					   __maybe_unused * in_params,
					   u16 __maybe_unused * icid)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rdma_resize_cq(struct qed_hwfn __maybe_unused *
					   p_hwfn,
					   struct qed_rdma_resize_cq_in_params
					   __maybe_unused * in_params,
					   struct qed_rdma_resize_cq_out_params
					   __maybe_unused * out_params)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rdma_destroy_cq(struct qed_hwfn __maybe_unused *
					    p_hwfn, struct
					    qed_rdma_destroy_cq_in_params
					    __maybe_unused * in_params, struct
					    qed_rdma_destroy_cq_out_params
					    __maybe_unused * out_params)
{
	return -EINVAL;
}

static inline struct qed_rdma_qp *qed_vf_pf_rdma_create_qp(struct qed_hwfn
							   __maybe_unused *
							   p_hwfn, struct
							   qed_rdma_create_qp_in_params
							   __maybe_unused *
							   in_params, struct
							   qed_rdma_create_qp_out_params
							   __maybe_unused *
							   out_params)
{
	return NULL;
}

static inline int qed_vf_pf_rdma_modify_qp(struct qed_hwfn __maybe_unused *
					   p_hwfn,
					   struct qed_rdma_qp __maybe_unused *
					   qp,
					   struct qed_rdma_modify_qp_in_params
					   __maybe_unused * in_params)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rdma_query_qp(struct qed_hwfn __maybe_unused *
					  p_hwfn,
					  struct qed_rdma_qp __maybe_unused *
					  qp,
					  struct qed_rdma_query_qp_out_params
					  __maybe_unused * out_params)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rdma_destroy_qp(struct qed_hwfn __maybe_unused *
					    p_hwfn,
					    struct qed_rdma_qp __maybe_unused *
					    qp, struct
					    qed_rdma_destroy_qp_out_params
					    __maybe_unused * out_params)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rdma_create_srq(struct qed_hwfn __maybe_unused *
					    p_hwfn, struct
					    qed_rdma_create_srq_in_params
					    __maybe_unused * in_params, struct
					    qed_rdma_create_srq_out_params
					    __maybe_unused * out_params)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rdma_modify_srq(struct qed_hwfn __maybe_unused *
					    p_hwfn, struct
					    qed_rdma_modify_srq_in_params
					    __maybe_unused * in_params)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rdma_destroy_srq(struct qed_hwfn __maybe_unused *
					     p_hwfn, struct
					     qed_rdma_destroy_srq_in_params
					     __maybe_unused * in_params)
{
	return -EINVAL;
}

static inline struct qed_rdma_port *qed_vf_pf_rdma_query_port(struct qed_hwfn
							      __maybe_unused *
							      p_hwfn)
{
	return NULL;
}

static inline int qed_vf_pf_rdma_query_device(struct qed_hwfn __maybe_unused *
					      p_hwfn)
{
	return -EINVAL;
}

static inline u32 qed_vf_hw_bar_size(struct qed_hwfn __maybe_unused * p_hwfn,
				     enum BAR_ID __maybe_unused bar_id)
{
	return 0;
}

#endif

void qed_iov_vf_task(struct work_struct *work);

#endif
