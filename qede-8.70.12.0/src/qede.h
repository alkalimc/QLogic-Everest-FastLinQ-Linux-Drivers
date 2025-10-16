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

#ifndef _QEDE_H_
#define _QEDE_H_
#include <linux/compiler.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)) /* QEDE_UPSTREAM */
#include <linux/workqueue.h>
#endif
#include <linux/netdevice.h>
#include <linux/interrupt.h>
#include <linux/bitmap.h>
#include <linux/kernel.h>
#include <linux/mutex.h>

#if defined(_HAS_TC_FLOWER) || defined(_HAS_FLOW_BLOCK_OFFLOAD) /* QEDE_UPSTREAM */
#include <net/pkt_cls.h>
#include <linux/tc_act/tc_mirred.h>
#include <net/tc_act/tc_mirred.h>
#include <net/tc_act/tc_gact.h>
#endif

#ifdef _HAS_NDO_XDP /* QEDE_UPSTREAM */
#include <linux/bpf.h>
#endif

#include "qede_compat.h"
#include "qede_hsi.h"
#include "qede_rdma.h"

#include <linux/io.h>
#ifdef CONFIG_RFS_ACCEL
#include <linux/cpu_rmap.h>
#endif

#include "qed_if.h"
#include "qed_chain.h"
#include "qed_eth_if.h"

#ifdef _HAS_KERNEL_ETHTOOL_COALESCE
int qede_set_coalesce(struct net_device *dev,
		      struct ethtool_coalesce *coal,
		      struct kernel_ethtool_coalesce *kernel_coal,
		      struct netlink_ext_ack *extack);
#else
int qede_set_coalesce(struct net_device *dev, struct ethtool_coalesce *coal);
#endif
void _qede_rdma_dev_remove(struct qede_dev *edev);
struct qedr_dev *_qede_rdma_dev_add(struct qede_dev *edev, bool lag_enable);
void _qede_rdma_dev_open(struct qede_dev *edev);

extern bool numa_native;

#define QEDE_MAJOR_VERSION		8
#define QEDE_MINOR_VERSION		70
#define QEDE_REVISION_VERSION		12
#define QEDE_ENGINEERING_VERSION	0
#define DRV_MODULE_VERSION __stringify(QEDE_MAJOR_VERSION) "."	\
		__stringify(QEDE_MINOR_VERSION) "."		\
		__stringify(QEDE_REVISION_VERSION) "." 		\
		__stringify(QEDE_ENGINEERING_VERSION)

#ifndef QEDE_UPSTREAM
#define QEDE_ETH_INTERFACE_VERSION	7012
#endif

#define DRV_MODULE_SYM		qede

/* defines the amount of bytes allocated for recording the length of
 * debugfs feature buffer
 */
#define REGDUMP_HEADER_SIZE sizeof(u32)
#define REGDUMP_HEADER_FEATURE_SHIFT 24
#define REGDUMP_HEADER_ENGINE_SHIFT 31
#define REGDUMP_HEADER_OMIT_ENGINE_SHIFT 30
enum debug_print_features {
	OLD_MODE = 0,
	IDLE_CHK = 1,
	GRC_DUMP = 2,
	MCP_TRACE = 3,
	REG_FIFO = 4,
	PROTECTION_OVERRIDE = 5,
	IGU_FIFO = 6,
	PHY = 7
};

struct qede_stats_common {
	u64 no_buff_discards;
	u64 packet_too_big_discard;
	u64 ttl0_discard;
	u64 rx_ucast_bytes;
	u64 rx_mcast_bytes;
	u64 rx_bcast_bytes;
	u64 rx_ucast_pkts;
	u64 rx_mcast_pkts;
	u64 rx_bcast_pkts;
	u64 mftag_filter_discards;
	u64 mac_filter_ucast_discards;
	u64 mac_filter_mcast_discards;
	u64 mac_filter_bcast_discards;
	u64 gft_filter_drop;
	u64 tx_ucast_bytes;
	u64 tx_mcast_bytes;
	u64 tx_bcast_bytes;
	u64 tx_ucast_pkts;
	u64 tx_mcast_pkts;
	u64 tx_bcast_pkts;
	u64 tx_err_drop_pkts;
	u64 coalesced_pkts;
	u64 coalesced_events;
	u64 coalesced_aborts_num;
	u64 non_coalesced_pkts;
	u64 coalesced_bytes;
	u64 link_change_count;
	u64 ptp_skip_txts;

	/* port */
	u64 rx_64_byte_packets;
	u64 rx_65_to_127_byte_packets;
	u64 rx_128_to_255_byte_packets;
	u64 rx_256_to_511_byte_packets;
	u64 rx_512_to_1023_byte_packets;
	u64 rx_1024_to_1518_byte_packets;
	u64 rx_crc_errors;
	u64 rx_mac_crtl_frames;
	u64 rx_pause_frames;
	u64 rx_pfc_frames;
	u64 rx_align_errors;
	u64 rx_carrier_errors;
	u64 rx_oversize_packets;
	u64 rx_jabbers;
	u64 rx_undersize_packets;
	u64 rx_fragments;
	u64 tx_64_byte_packets;
	u64 tx_65_to_127_byte_packets;
	u64 tx_128_to_255_byte_packets;
	u64 tx_256_to_511_byte_packets;
	u64 tx_512_to_1023_byte_packets;
	u64 tx_1024_to_1518_byte_packets;
	u64 tx_pause_frames;
	u64 tx_pfc_frames;
	u64 brb_truncates;
	u64 brb_discards;
	u64 tx_mac_ctrl_frames;
	u32 pfm_state_changes;
	u32 nig_drain_cnt;
};

struct qede_stats_bb {
	/* port */
	u64 rx_1519_to_1522_byte_packets;
	u64 rx_1519_to_2047_byte_packets;
	u64 rx_2048_to_4095_byte_packets;
	u64 rx_4096_to_9216_byte_packets;
	u64 rx_9217_to_16383_byte_packets;
	u64 tx_1519_to_2047_byte_packets;
	u64 tx_2048_to_4095_byte_packets;
	u64 tx_4096_to_9216_byte_packets;
	u64 tx_9217_to_16383_byte_packets;
	u64 tx_lpi_entry_count;
	u64 tx_total_collisions;
};

struct qede_stats_ah {
	/* port */
	u64 rx_1519_to_max_byte_packets;
	u64 tx_1519_to_max_byte_packets;
};

struct qede_stats {
	struct qede_stats_common common;
	union {
		struct qede_stats_bb bb;
		struct qede_stats_ah ah;
	};
};

struct qede_vlan {
	struct list_head list;
	u16 vid;
	bool configured;
};

struct qede_rdma_dev {
	struct qedr_dev *qedr_dev;
	struct list_head entry;
	struct list_head rdma_event_list;
	struct workqueue_struct *rdma_wq;
	struct kref refcnt;
	struct completion event_comp;
	bool lag_enabled;
};

struct qede_ptp;

/* info of chain for printing */
struct chain_print_struct {
	bool b_key_entered;
	char *buffer;
	u32 current_index;
	u32 final_index;
	bool print_metadata;
	struct qed_chain *chain;
};

struct qede_arfs;
#define QEDE_RFS_MAX_FLTR	256
#define QEDE_MAX_TC		4

enum qede_flags_bit {
	QEDE_FLAGS_IS_VF,
	QEDE_FLAGS_LINK_REQUESTED,
	QEDE_FLAGS_PTP_TX_IN_PRORGESS,
	QEDE_FLAGS_TX_TIMESTAMPING_EN
};

struct qede_fwd_dev {
	struct list_head list;
	struct qede_dev *edev;
	struct net_device *upper_ndev;
	u8 vport_id;
	u8 num_queues;
	u8 base_queue_id;
};

struct qede_lag {
	struct qed_dev *lag_cdev;
	u8 port_active;

	/* TODO move lag status here */
};

#define QEDE_DUMP_MAX_ARGS 4
enum qede_dump_cmd {
	QEDE_DUMP_CMD_NONE = 0,
	QEDE_DUMP_CMD_NVM_CFG,
	QEDE_DUMP_CMD_GRCDUMP,
	QEDE_DUMP_CMD_MAX
};

struct qede_dump_info {
	enum qede_dump_cmd cmd;
	u8 num_args;
	u32 args[QEDE_DUMP_MAX_ARGS];
};

struct qede_dev {
	struct qed_dev			*cdev;
	struct net_device		*ndev;
	struct pci_dev			*pdev;
	struct dentry			*bdf_dentry;
	struct devlink			*devlink;

	int				num_vfs;
	struct chain_print_struct chain_info;

	u32				dp_module;
	u8				dp_level;
	void				*dp_ctx;
	u32				dp_int_module;
	u8				dp_int_level;
	bool				set_int_msglevel;

	unsigned long flags;
#define IS_VF(edev)	(test_bit(QEDE_FLAGS_IS_VF, &edev->flags))

	const struct qed_eth_ops	*ops;
	struct qede_ptp			*ptp;
	u64				ptp_skip_txts;

	struct qed_dev_eth_info		dev_info;
#define QEDE_MAX_RSS_CNT(edev)	((edev)->dev_info.num_queues - \
				 (edev)->fwd_dev_queues)
#define QEDE_MAX_TSS_CNT(edev)	((edev)->dev_info.num_queues - \
				 (edev)->fwd_dev_queues)
#define QEDE_MAX_QUEUE_CNT(edev) ((edev)->dev_info.num_queues)
#define QEDE_IS_BB(edev) \
	((edev)->dev_info.common.dev_type == QED_DEV_TYPE_BB)
#define QEDE_IS_AH(edev) \
	((edev)->dev_info.common.dev_type == QED_DEV_TYPE_AH)
#define QEDE_IS_CMT(edev)	((edev)->dev_info.common.num_hwfns > 1)

	struct qede_fastpath		*fp_array;
	u8				req_num_tx;
	u8				fp_num_tx;
	u8				req_num_rx;
	u8				fp_num_rx;

	/* requested for base dev */
	u16				req_queues;
	u16				num_queues;
	u16				base_num_queues;
#define QEDE_QUEUE_CNT(edev)		((edev)->num_queues)
#define QEDE_BASE_QUEUE_CNT(edev)	((edev)->base_num_queues)
#define QEDE_BASE_RSS_COUNT(edev)	(QEDE_BASE_QUEUE_CNT(edev) - \
					 (edev)->fp_num_tx)
#define QEDE_RX_QUEUE_IDX(edev, i)	(i)
#define QEDE_BASE_TSS_COUNT(edev)	(QEDE_BASE_QUEUE_CNT(edev) - \
					 (edev)->fp_num_rx)

	/* Linked list to maintain L2 forwarding offload device info */
	struct list_head		fwd_dev_list;
	u8				num_fwd_devs;
#define QEDE_BASE_DEV_VPORT_ID	0
	/* used to allocate vport ids */
	u8				vport_id_used;

	/* total queues used by L2 fwd devs */
	u16				fwd_dev_queues;

	/* requested for L2 fwd dev */
	u16				req_fwd_dev_queues;

	struct qed_int_info		int_info;

	/* Smaller private variant of the RTNL lock */
	struct mutex			qede_lock;
	u32 				state; /* Protected by __qede_lock */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37)) /* ! QEDE_UPSTREAM */
	struct vlan_group	*vlan_group;
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39)) /* ! QEDE_UPSTREAM */
	u32			rx_csum;
#endif

	u16				rx_buf_size;
	u32				rx_copybreak;

	/* L2 header size + 2*VLANs (8 bytes) + LLC SNAP (8 bytes) */
#define ETH_OVERHEAD			(ETH_HLEN + 8 + 8)
	/* Max supported alignment is 256 (8 shift)
	 * minimal alignment shift 6 is optimal for 57xxx HW performance
	 */
#define QEDE_RX_ALIGN_SHIFT		max(6, min(8, L1_CACHE_SHIFT))
	/* We assume skb_build() uses sizeof(struct skb_shared_info) bytes
	 * at the end of skb->data, to avoid wasting a full cache line.
	 * This reduces memory use (skb->truesize).
	 */
#define QEDE_FW_RX_ALIGN_END					\
	max_t(u64, 1UL << QEDE_RX_ALIGN_SHIFT,			\
	      SKB_DATA_ALIGN(sizeof(struct skb_shared_info)))

	struct qede_stats		stats;

#define QEDE_RSS_INDIR_INITED	BIT(0)
#define QEDE_RSS_KEY_INITED	BIT(1)
#define QEDE_RSS_CAPS_INITED	BIT(2)
	u32 rss_params_inited; /* bit-field to track initialized rss params */
	u16 rss_ind_table[128];
	u32 rss_key[10];
	u8 rss_caps;

	u16			q_num_rx_buffers; /* Must be a power of two */
	u16			q_num_tx_buffers; /* Must be a power of two */
	bool gro_disable;

	/* Vlans */
	struct list_head vlan_list;
	u16 configured_vlans;
	u16 non_configured_vlans;
	bool accept_any_vlan; /* true: enabled, false: disabled */

	struct delayed_work		sp_task;
	unsigned long			sp_flags;
	u16				vxlan_dst_port;
	u16				geneve_dst_port;
	struct qede_arfs *arfs;
	bool wol_enabled;

	struct qede_rdma_dev rdma_info;

	struct bpf_prog *xdp_prog;
	struct qede_lag lag;

	enum qed_hw_err_type last_err_type;
	unsigned long err_flags;
#define QEDE_ERR_IS_HANDLED	31
#define QEDE_ERR_ATTN_CLR_EN	0
#define QEDE_ERR_GET_DBG_INFO	1
#define QEDE_ERR_IS_RECOVERABLE	2
#define QEDE_ERR_WARN		3
	unsigned long err_flags_override;
#define QEDE_ERR_OVERRIDE_EN	31

#ifdef CONFIG_DEBUG_FS
	bool gen_tx_timeout;
#endif
	struct qede_dump_info		dump_info;

	/* Driver initiated AER recovery in progress */
	bool aer_recov_prog;

	/* flag to keep track of slot_reset call during AER recovery */
	bool is_slot_reset_done;

	/* edev's state before AER error was injected */
	u32 pre_aer_state;
};

enum QEDE_STATE {
	QEDE_STATE_CLOSED,
	QEDE_STATE_OPEN,
	QEDE_STATE_RECOVERY,
};

#define HILO_U64(hi, lo)		((((u64)(hi)) << 32) + (lo))

#define	MAX_NUM_TC	8
#define	MAX_NUM_PRI	8

/* The driver supports the new build_skb() API:
 * RX ring buffer contains pointer to kmalloc() data only,
 * skb are built only after the frame was DMA-ed.
 */
struct sw_rx_data {
	struct page *data;
	dma_addr_t mapping;
	unsigned int page_offset;
};

enum qede_agg_state {
	QEDE_AGG_STATE_NONE  = 0,
	QEDE_AGG_STATE_START = 1,
	QEDE_AGG_STATE_ERROR = 2
};

struct qede_agg_info {
	/* rx_buf is a data buffer that can be placed / consumed from rx bd
	 * chain. It has two purposes: We will preallocate the data buffer
	 * for each aggregation when we open the interface and will place this
	 * buffer on the rx-bd-ring when we receive TPA_START. We don't want
	 * to be in a state where allocation fails, as we can't reuse the
	 * consumer buffer in the rx-chain since FW may still be writing to it
	 * (since header needs to be modified for TPA).
	 * The second purpose is to keep a pointer to the bd buffer during
	 * aggregation.
	 */
	struct sw_rx_data	buffer;

	struct sk_buff		*skb;

	/* We need some structs from the start cookie until termination */
	u16 vlan_tag;

#ifndef _HAS_BUILD_SKB_V2 /* ! QEDE_UPSTREAM */
	u16			start_cqe_bd_len;
	u8			start_cqe_placement_offset;
	dma_addr_t		buffer_mapping;
#endif

	u8 state;
	u8 frag_id;

#ifdef DEBUG_GRO
	int num_aggregations;
	int num_of_bds;
	int total_packet_len;
#endif
#ifdef ENC_SUPPORTED
	u8			tunnel_type;
	u8			inner_vlan_exist;
#endif
};

struct qede_pool_dma_info {
	struct page	*page;
	dma_addr_t	mapping;
};

struct qede_page_pool {
	/* size would always be a power of two */
	u16 size;
	u16 cons;
	u16 prod;
	struct qede_pool_dma_info *page_pool;
};

/* MK TODO: re-arrange according to cache-line access */
struct qede_rx_queue {
	__le16			*hw_cons_ptr;
	void __iomem		*hw_rxq_prod_addr;

	/* Required for the allocation of replacement buffers */
	struct device		*dev;

	/* The queue's program may differ from the edev during the process
	 * of replacing the program, as we're not stopping all queues.
	 */
	struct bpf_prog		*xdp_prog;

	u16			sw_rx_cons;
	u16			sw_rx_prod;

	u16			filled_buffers;
	u8			data_direction;
	u8			rxq_id;

	/* Used once per each NAPI run */
	u16			num_rx_buffers;

	/* Reserved headroom on buffer for XDP OR build_skb() */
	u16			rx_headroom;

	/* Buffer size for FW from the segment of the page */
	u32			rx_buf_size;

	/* Segment size of data segments on the page */
	u32			rx_buf_seg_size;

	struct sw_rx_data	*sw_rx_ring;
	struct qed_chain	rx_bd_ring;
	struct qed_chain	rx_comp_ring;

	/* GRO */
	struct qede_agg_info	tpa_info[ETH_TPA_MAX_AGGS_NUM];

	/* Used once per each NAPI run */
	u64			rcv_pkts;

	u64			rx_csum_errors;
	u64			rx_alloc_errors;
	u64			rx_ip_frags;

	u64			xdp_no_pass;
	u64			xdp_pass;
	u64			pool_full;
	u64			pool_unready;

	struct qede_page_pool	page_pool;

	struct qede_fastpath *fp;
	/* Slowpath in nature; Should be left at the end */
	void *handle;
};


union db_prod {
	struct eth_db_data data;
	u32		raw;
};

struct sw_tx_bd {
	struct sk_buff *skb;
	u8 flags;
/* Set on the first BD descriptor when there is a split BD */
#define QEDE_TSO_SPLIT_BD		BIT(0)
};

/* Due to padding, it's possible the physical address we uese for forwarding
 * would differ from actual base physical address, so we'll save original for
 * the eventual unmapping.
 */
struct sw_tx_xdp {
	struct page *page;
	dma_addr_t mapping;
};

struct qede_tx_queue {
	u8			is_xdp;
	u8			is_legacy;

	u16			sw_tx_cons;
	u16			sw_tx_prod;

	__le16			*hw_cons_ptr;

	/* Needed for the mapping of packets */
	struct device		*dev;

	/* Regular Tx requires skb + metadata for release purpose,
	 * while XDP requires the pages and the mapped address.
	 */
	union {
		struct sw_tx_bd *skbs;
		struct sw_tx_xdp *xdp;
	} sw_tx_ring;

	struct qed_chain	tx_pbl;
	void __iomem		*doorbell_addr;
	union db_prod		tx_db;

	/* TBD - while purely debug, it's used by logger.
	 * so it's better to have this in 3 3rd cacheline rather than
	 * `stopped_cnt' which wouldn't usually adavnce.
	 */
	struct qede_fastpath *fp;

	u64			xmit_pkts;
	u64			stopped_cnt;
	u64			tx_mem_alloc_err;

	/* Slowpath in nature; Should be left at the end */
	void			*handle;
	u16			num_tx_buffers;
	u16			index;
	u16			cos;
	u16			ndev_txq_id;
#define QEDE_TXQ_XDP_TO_IDX(edev, txq)	((txq)->index - \
					 QEDE_MAX_TSS_CNT(edev))
#define QEDE_TXQ_IDX_TO_XDP(edev, idx)	((idx) + QEDE_MAX_TSS_CNT(edev))
#define QEDE_NDEV_TXQ_ID_TO_FP_ID(edev, idx)	\
			((edev)->fp_num_rx + \
			 ((idx) % (QEDE_BASE_TSS_COUNT(edev) + \
				   (edev)->fwd_dev_queues)))
#define QEDE_NDEV_TXQ_ID_TO_TXQ_COS(edev, idx)	\
			((idx) / (QEDE_BASE_TSS_COUNT(edev) + \
				  (edev)->fwd_dev_queues))
#define QEDE_TXQ_TO_NDEV_TXQ_ID(edev, txq)  (((QEDE_BASE_TSS_COUNT(edev) + \
					       (edev)->fwd_dev_queues) * \
						(txq)->cos) + (txq)->index)
#define QEDE_NDEV_TXQ_ID_TO_TXQ(edev, idx)	\
	(&((edev)->fp_array[QEDE_NDEV_TXQ_ID_TO_FP_ID(edev, idx)].txq \
	 [QEDE_NDEV_TXQ_ID_TO_TXQ_COS(edev, idx)]))
#define QEDE_FP_TC0_TXQ(fp)	(&((fp)->txq[0]))
};

enum qede_fp_logger_type {
	QEDE_FP_LOGGER_INT,
	QEDE_FP_LOGGER_NAPI,
};

struct qede_fp_logger_gen {
	u8 type;
	long unsigned int ts;
};

struct qede_fp_logger_db {
	struct qede_fp_logger_gen common;

	__le16 db;
};

struct qede_fp_logger_int {
	struct qede_fp_logger_gen common;

	u32 ack;
	u8 command;
};

struct qede_fp_logger_napi {
	struct qede_fp_logger_gen common;

	int rc;
	u16 rx_cons;
	u16 tx_prod[QEDE_MAX_TC];
	u16 tx_cons[QEDE_MAX_TC];
};

struct qede_fp_logger {
	bool spilled;

	/* We'll be using this from both interrupt context & napi,
	 * but this is actually [usually] safe, as the two should be exclusive
	 * [not counting polling stuff].
	 */
	u8 idx;

	union {
		struct qede_fp_logger_int intr;
		struct qede_fp_logger_napi napi;
		struct qede_fp_logger_gen gen;
	} data[256];

	struct qede_fp_logger_db last_db;
};

#define BD_UNMAP_ADDR(bd)		HILO_U64(le32_to_cpu((bd)->addr.hi), \
						 le32_to_cpu((bd)->addr.lo))
#define BD_SET_UNMAP_ADDR_LEN(bd, maddr, len)				\
	do {								\
		(bd)->addr.hi = cpu_to_le32(upper_32_bits(maddr));	\
		(bd)->addr.lo = cpu_to_le32(lower_32_bits(maddr));	\
		(bd)->nbytes = cpu_to_le16(len);			\
	} while (0)
#define BD_UNMAP_LEN(bd)		(le16_to_cpu((bd)->nbytes))

#ifdef TIME_FP_DEBUG /* ! QEDE_UPSTREAM */
enum qede_fp_time_event {
	QEDE_FP_TIME_START,
	QEDE_FP_TIME_END,
	QEDE_FP_TIME_END_RESCHEDULE,
};

struct qede_fp_time_stats {
	u64 runs;
	u64 rx;
	u64 tx[QEDE_MAX_TC];
	u64 usecs;
	u64 longest;
};

struct qede_fp_time {
	struct qede_fp_time_stats softirq;
	struct qede_fp_time_stats ksoftirqd;

	struct timeval start_tv;
	u32 start_rx;
	u32 start_tx[QEDE_MAX_TC];
	u32 consecutive_napi;
};
#endif

struct qede_fastpath {
#define QEDE_FASTPATH_TX	BIT(0)
#define QEDE_FASTPATH_RX	BIT(1)
#define QEDE_FASTPATH_XDP	BIT(2)
#define QEDE_FASTPATH_COMBINED	(QEDE_FASTPATH_TX | QEDE_FASTPATH_RX)
	u8			type;
	u8			id;
	u8			xdp_xmit;
	u8			vport_id;
	bool			fwd_fp;
	struct qede_dev		*edev;
	struct napi_struct	napi;
	struct qed_sb_info	*sb_info;
	struct qede_rx_queue	*rxq;
	struct qede_tx_queue	*txq;
	struct qede_tx_queue	*xdp_tx;
	struct qede_fwd_dev	*fwd_dev;
	struct net_device	*ndev;

	struct qede_fp_logger	logger;
#ifdef TIME_FP_DEBUG /* ! QEDE_UPSTREAM */
	struct qede_fp_time	time_log;
#endif

#define VEC_NAME_SIZE	(sizeof(((struct net_device *)0)->name) + 8)
	char	name[VEC_NAME_SIZE];
};

/* Debug print definitions */
#define DP_NAME(edev) ((edev)->ndev->name)

#define XMIT_PLAIN		0
#define XMIT_L4_CSUM		BIT(0)
#define XMIT_LSO		BIT(1)
#define XMIT_ENC		BIT(2)
#define XMIT_ENC_GSO_L4_CSUM	BIT(3)

#define QEDE_CSUM_ERROR			BIT(0)
#define QEDE_CSUM_UNNECESSARY		BIT(1)
#define QEDE_TUNN_CSUM_UNNECESSARY	BIT(2)

#define QEDE_SP_RECOVERY		0
#define QEDE_SP_RX_MODE			1
#define QEDE_SP_VXLAN_PORT_CONFIG	2
#define QEDE_SP_GENEVE_PORT_CONFIG	3
#define QEDE_SP_HW_ERR			4
#define QEDE_SP_ARFS_CONFIG		5
#define QEDE_SP_LINK_UP			6
#define QEDE_SP_AER			7

#define QEDE_SP_DISABLE			9

#define QEDE_SP_TASK_POLL_DELAY		(5 * HZ)

enum qede_update_flags {
	/* Complete reload */
	QEDE_UPDATE_COMPLETE = BIT(0),

	/* Reload due to the device recovery */
	QEDE_UPDATE_RECOVERY = BIT(1),

	/* Purge/rebuild the tx/rx buffers */
	QEDE_UPDATE_FP_BUFFERS = BIT(2),

	/* Purge/rebuild the tx/rx channels */
	QEDE_UPDATE_FP_ELEM = BIT(3),

	/* Reload due to MTU, ringparam etc. changes */
	QEDE_RELOAD = BIT(4),
};

struct qede_reload_args {
	void (*func)(struct qede_dev *edev,
		     struct qede_reload_args *args);
	union {
		netdev_features_t features;
		struct bpf_prog *new_prog;
		struct qede_fwd_dev *fwd_dev;
		u16 mtu;
	} u;

	enum qede_update_flags flags;

	bool is_locked;
};

/* Datapath functions definition */
netdev_tx_t qede_start_xmit(struct sk_buff *skb, struct net_device *ndev);
#ifdef CONFIG_DEBUG_FS
netdev_tx_t qede_start_xmit_tx_timeout(struct sk_buff *skb,
				       struct net_device *ndev);
#endif

#ifdef QEDE_SELECTQUEUE_HAS_SBDEV_PARAM  /* QEDE_UPSTREAM */
u16 qede_select_queue(struct net_device *dev, struct sk_buff *skb,
		      struct net_device *sb_dev);
#elif defined(QEDE_SELECTQUEUE_HAS_FALLBACK_SBDEV_PARAM)
u16 qede_select_queue(struct net_device *dev, struct sk_buff *skb,
		      struct net_device *sb_dev,
		      select_queue_fallback_t fallback);
#elif defined(QEDE_SELECTQUEUE_HAS_FALLBACK_PARAM)
u16 qede_select_queue(struct net_device *dev, struct sk_buff *skb,
		      void *accel_priv, select_queue_fallback_t fallback);
#elif defined(QEDE_SELECTQUEUE_HAS_ACCEL_PARAM)
u16 qede_select_queue(struct net_device *dev, struct sk_buff *skb,
		      void *accel_priv);
#else
u16 qede_select_queue(struct net_device *dev, struct sk_buff *skb);
#endif
#ifdef _HAS_NDO_FEATURES_CHECK /* QEDE_UPSTREAM */
netdev_features_t qede_features_check(struct sk_buff *skb,
				      struct net_device *dev,
				      netdev_features_t features);
#endif
void qede_txq_fp_log_metadata(struct qede_dev *edev, struct qede_fastpath *fp,
			      struct qede_tx_queue *txq);
void qede_tx_log_print(struct qede_dev *edev,
		       struct qede_fastpath *fp, struct qede_tx_queue *txq);
int qede_alloc_rx_buffer(struct qede_rx_queue *rxq, bool allow_lazy);
int qede_free_tx_pkt(struct qede_dev *edev,
		     struct qede_tx_queue *txq, int *len);
int qede_poll(struct napi_struct *napi, int budget);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)) /* QEDE_UPSTREAM */
irqreturn_t qede_msix_fp_int(int irq, void *fp_cookie);
#else
irqreturn_t qede_msix_fp_int(int irq, void *fp_cookie,
			     struct pt_regs *regs);
#endif

/* Filtering function definitions */
void qede_force_mac(void *dev, u8 *mac, bool forced);
void qede_udp_ports_update(void *dev, u16 vxlan_port, u16 geneve_port);
int qede_set_mac_addr(struct net_device *ndev, void *p);

#if !defined(_VLAN_RX_ADD_VID_RETURNS_VOID) && !defined(_VLAN_RX_ADD_VID_NO_PROTO) /* QEDE_UPSTREAM */
int qede_vlan_rx_add_vid(struct net_device *dev, __be16 proto, u16 vid);
int qede_vlan_rx_kill_vid(struct net_device *dev, __be16 proto, u16 vid);
#elif defined(_VLAN_RX_ADD_VID_RETURNS_VOID)
void qede_vlan_rx_add_vid_void(struct net_device *dev, u16 vid);
void qede_vlan_rx_kill_vid_void(struct net_device *dev, u16 vid);
#else
int qede_vlan_rx_add_vid_no_proto(struct net_device *dev, u16 vid);
int qede_vlan_rx_kill_vid_no_proto(struct net_device *dev, u16 vid);
#endif
void qede_vlan_mark_nonconfigured(struct qede_dev *edev);
int qede_configure_vlan_filters(struct qede_dev *edev);
#ifdef BCM_VLAN /* ! QEDE_UPSTREAM */
void qede_vlan_rx_register(struct net_device *ndev, struct vlan_group *vlgrp);
#endif

netdev_features_t qede_fix_features(struct net_device *dev,
				    netdev_features_t features);
int qede_set_features(struct net_device *dev, netdev_features_t features);
void qede_set_rx_mode(struct net_device *ndev);
void qede_config_rx_mode(struct net_device *ndev);
void qede_config_rx_mode_for_all(struct qede_dev *edev);
void qede_fill_rss_params(struct qede_dev *edev,
			  struct qed_update_vport_rss_params *rss,
			  u8 *update);

#ifdef _HAS_ADD_VXLAN_PORT /* ! QEDE_UPSTREAM */
void qede_add_vxlan_port(struct net_device *dev,
			 sa_family_t sa_family, __be16 port);
void qede_del_vxlan_port(struct net_device *dev,
			 sa_family_t sa_family, __be16 port);
#endif
#ifdef _HAS_ADD_GENEVE_PORT /* ! QEDE_UPSTREAM */
void qede_add_geneve_port(struct net_device *dev,
			  sa_family_t sa_family, __be16 port);
void qede_del_geneve_port(struct net_device *dev,
			  sa_family_t sa_family, __be16 port);
#endif
#if HAS_NDO(UDP_TUNNEL_CONFIG) /* QEDE_UPSTREAM */
void qede_udp_tunnel_add(struct net_device *dev,
			 struct udp_tunnel_info *ti);
void qede_udp_tunnel_del(struct net_device *dev,
			 struct udp_tunnel_info *ti);
#endif

#ifdef CONFIG_RFS_ACCEL
int qede_rx_flow_steer(struct net_device *dev, const struct sk_buff *skb,
		       u16 rxq_index, u32 flow_id);
void qede_delete_non_user_arfs_flows(struct qede_dev *edev);
void qede_free_cpu_rmap(struct qede_dev *edev);
int qede_alloc_cpu_rmap(struct qede_dev *edev);
#endif
void qede_process_arfs_filters(struct qede_dev *edev, bool free_fltr);
void qede_poll_for_freeing_arfs_filters(struct qede_dev *edev);
void qede_arfs_filter_op(void *dev, void *filter, u8 fw_rc);
void qede_free_arfs(struct qede_dev *edev);
int qede_alloc_arfs(struct qede_dev *edev);
int qede_add_cls_rule(struct qede_dev *edev, struct ethtool_rxnfc *info);
int qede_delete_flow_filter(struct qede_dev *edev, u64 cookie);
int qede_get_cls_rule_entry(struct qede_dev *edev, struct ethtool_rxnfc *cmd);
int qede_get_cls_rule_all(struct qede_dev *edev, struct ethtool_rxnfc *info,
			  u32 *rule_locs);
int qede_get_arfs_filter_count(struct qede_dev *edev);

#ifdef _HAS_NDO_XDP /* QEDE_UPSTREAM */
#ifdef _HAS_NDO_BPF /* QEDE_UPSTREAM */
int qede_xdp(struct net_device *dev, struct netdev_bpf *xdp);
#else
int qede_xdp(struct net_device *dev, struct netdev_xdp *xdp);
#endif
#endif

#ifdef CONFIG_DCB
void qede_set_dcbnl_ops(struct net_device *ndev);
#endif
void qede_config_debug(uint debug, u32 *p_dp_module, u8 *p_dp_level);
void qede_set_ethtool_ops(struct net_device *netdev);
void qede_reload(struct qede_dev *edev, struct qede_reload_args *args);
int qede_change_mtu(struct net_device *dev, int new_mtu);
void qede_fill_by_demand_stats(struct qede_dev *edev);
void qede_print_rcq(struct qede_dev *edev,
			   struct qede_rx_queue *rxq);
void __qede_lock(struct qede_dev *edev);
void __qede_unlock(struct qede_dev *edev);
void qede_lock(struct qede_dev *edev);
void qede_unlock(struct qede_dev *edev);
bool qede_has_rx_work(struct qede_rx_queue *rxq);
int qede_txq_has_work(struct qede_tx_queue *txq);
void qede_recycle_rx_bd_ring(struct qede_rx_queue *rxq, u8 count);
void qede_update_rx_prod(struct qede_dev *edev, struct qede_rx_queue *rxq);
int qede_get_vf_config(struct net_device *dev, int vfidx,
		       struct ifla_vf_info *ivi);

#if defined(_HAS_TC_FLOWER) || defined(_HAS_FLOW_BLOCK_OFFLOAD) /* QEDE_UPSTREAM */
#if defined(_HAS_FLOW_BLOCK_OFFLOAD) /* QEDE_UPSTREAM */
int qede_add_tc_flower_fltr(struct qede_dev *edev, __be16 proto,
			    struct flow_cls_offload *f);
#else
int qede_add_tc_flower_fltr(struct qede_dev *edev, __be16 proto,
			    struct tc_cls_flower_offload *f);
#endif
#endif

#ifdef CONFIG_NET_POLL_CONTROLLER
void qede_poll_controller(struct net_device *dev);
#endif

/* @@@TBD Temporary defines - to be updated - must be a power of two.
 * Also - find real min/max values.
 */

#define RX_RING_SIZE_POW	13
#define RX_RING_SIZE		((u16)BIT(RX_RING_SIZE_POW))
#define NUM_RX_BDS_MAX		(RX_RING_SIZE - 1)
#define NUM_RX_BDS_MIN		128
#define NUM_RX_BDS_KDUMP_MIN    32
#define NUM_RX_BDS_DEF		((u16)BIT(10) - 1)

#define TX_RING_SIZE_POW	13
#define TX_RING_SIZE		((u16)BIT(TX_RING_SIZE_POW))
#define NUM_TX_BDS_MAX		(TX_RING_SIZE - 1)
#define NUM_TX_BDS_MIN		128
#define NUM_TX_BDS_KDUMP_MIN    32
#define NUM_TX_BDS_DEF		NUM_TX_BDS_MAX

#define QEDE_MIN_PKT_LEN		64
#define QEDE_RX_HDR_SIZE		256
#define QEDE_MAX_JUMBO_PACKET_SIZE	9600
#define	for_each_queue(i) for (i = 0; i < edev->num_queues; i++)
#define for_each_cos_in_txq(edev, var) \
	for ((var) = 0; (var) < (edev)->dev_info.num_tc; (var)++)

#define QED_INT_DBG_STORE(P_DEV, ...)					\
	do {								\
		if ((P_DEV) && (P_DEV)->ops)				\
			(P_DEV)->ops->common->internal_trace((P_DEV)->cdev, \
							     __VA_ARGS__); \
	} while (0)

#endif /* _QEDE_H_ */

