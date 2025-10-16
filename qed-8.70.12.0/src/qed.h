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

#ifndef _QED_H
#define _QED_H
#include <linux/types.h>
#include <linux/stringify.h>
#include <linux/bitops.h>
#if (!defined (_DEFINE_CRC8) && !defined(_MISSING_CRC8_MODULE))	/* QED_UPSTREAM */
#include <linux/crc8.h>
#else
#include "qed_compat.h"
#endif
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/if_ether.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <linux/zlib.h>
#if defined(QED_UPSTREAM)
#include <linux/hashtable.h>
#endif
#if defined(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif
#if defined(CONFIG_DEBUG_FS)
#include <linux/fs.h>
#endif
#include "qed_dbg_hsi.h"
#include "qed_debug.h"
#include "qed_hsi.h"
#include "qed_if.h"
#include "qed_mfw_hsi.h"
#include "qed_compat.h"
#include "qed_eth_if.h"
#include "qed_if.h"
#ifndef _QED_H_
#define _QED_H_

#define QED_MAJOR_VERSION               8
#define QED_MINOR_VERSION               70
#define QED_REVISION_VERSION            12
#define QED_ENGINEERING_VERSION 0

#define QED_VERSION						 \
	((QED_MAJOR_VERSION << 24) | (QED_MINOR_VERSION << 16) | \
	 (QED_REVISION_VERSION << 8) | QED_ENGINEERING_VERSION)

#define STORM_FW_VERSION				       \
	((FW_MAJOR_VERSION << 24) | (FW_MINOR_VERSION << 16) | \
	 (FW_REVISION_VERSION << 8) | FW_ENGINEERING_VERSION)

#define IS_QED_PACING(p_hwfn)   (!!(p_hwfn->b_en_pacing))
#define IS_QED_DCQCN(p_hwfn)    (!!(p_hwfn->b_en_dcqcn))

#define MAX_HWFNS_PER_DEVICE    2
#define NAME_SIZE 16
#define ARRAY_DECL static const

#define QM_INVALID_PQ_ID                0xffff
#define QM_INVALID_WFQ_ID               0xffff
#define QM_INVALID_RL_ID                0xffff

#define QED_WFQ_RESOLUTION      10000
#define QED_WFQ_UNIT    (QED_WFQ_RESOLUTION + COMMON_MAX_QM_GLOBAL_RLS)
#define QED_WEIGHT_IN_WFQ_GRANULARITY(__percentage) \
	(QED_WFQ_UNIT * __percentage / 100)

/* Constants */
#define QED_WID_SIZE            (1024)
#define QED_MIN_WIDS            (4)

/* Configurable */
#define QED_PF_DEMS_SIZE        (4)
#define QED_VF_DEMS_SIZE        (32)
#define QED_MIN_DPIS            (4)	/* The minimal number of DPIs required to
					 * load the driver. The number was
					 * arbitrarily set.
					 */
/* Derived */
#define QED_MIN_PWM_REGION      (QED_WID_SIZE * QED_MIN_DPIS)

#define QED_CXT_PF_CID (0xff)

#define QED_HW_STOP_RETRY_LIMIT (10)

/* cau states */
enum qed_coalescing_mode {
	QED_COAL_MODE_DISABLE,
	QED_COAL_MODE_ENABLE
};

enum qed_nvm_cmd {
	QED_PUT_FILE_BEGIN = DRV_MSG_CODE_NVM_PUT_FILE_BEGIN,
	QED_PUT_FILE_DATA = DRV_MSG_CODE_NVM_PUT_FILE_DATA,
	QED_NVM_READ_NVRAM = DRV_MSG_CODE_NVM_READ_NVRAM,
	QED_NVM_WRITE_NVRAM = DRV_MSG_CODE_NVM_WRITE_NVRAM,
	QED_NVM_DEL_FILE = DRV_MSG_CODE_NVM_DEL_FILE,
	QED_EXT_PHY_FW_UPGRADE = DRV_MSG_CODE_EXT_PHY_FW_UPGRADE,
	QED_PHY_RAW_READ = DRV_MSG_CODE_PHY_RAW_READ,
	QED_PHY_RAW_WRITE = DRV_MSG_CODE_PHY_RAW_WRITE,
	QED_PHY_CORE_READ = DRV_MSG_CODE_PHY_CORE_READ,
	QED_PHY_CORE_WRITE = DRV_MSG_CODE_PHY_CORE_WRITE,
	QED_ENCRYPT_PASSWORD = DRV_MSG_CODE_ENCRYPT_PASSWORD,
	QED_GET_MCP_NVM_RESP = 0xFFFFFF00
};

/* helpers */

static inline u32 DB_ADDR(u32 cid, u32 DEMS)
{
	u32 db_addr = FIELD_VALUE(DB_LEGACY_ADDR_DEMS, DEMS) |
	    (cid * QED_PF_DEMS_SIZE);

	return db_addr;
}

static inline u32 DB_ADDR_VF(u32 cid, u32 DEMS)
{
	u32 db_addr = FIELD_VALUE(DB_LEGACY_ADDR_DEMS, DEMS) |
	    FIELD_VALUE(DB_LEGACY_ADDR_ICID, cid);

	return db_addr;
}

#define ALIGNED_TYPE_SIZE(type_name, p_hwfn)				     \
	((sizeof(type_name) + (u32)(1 << (p_hwfn->cdev->cache_shift)) - 1) & \
	 ~((1 << (p_hwfn->cdev->cache_shift)) - 1))

#define for_each_hwfn(cdev, i)  for (i = 0; i < cdev->num_hwfns; i++)

#define D_TRINE(val, cond1, cond2, true1, true2, def) \
	((val) == (cond1) ? (true1) :		      \
	 ((val) == (cond2) ? (true2) : (def)))

/* forward */
struct qed_ptt_pool;
struct qed_spq;
struct qed_sb_info;
struct qed_sb_attn_info;
struct qed_cxt_mngr;
struct qed_dma_mem;
struct qed_sb_sp_info;
struct qed_ll2_info;
struct qed_l2_info;
struct qed_igu_info;
struct qed_mcp_info;
struct qed_dcbx_info;
struct qed_llh_info;

struct qed_rt_data {
	u32 *init_val;
	bool *b_valid;
};

enum qed_tunn_mode {
	QED_MODE_L2GENEVE_TUNN,
	QED_MODE_IPGENEVE_TUNN,
	QED_MODE_L2GRE_TUNN,
	QED_MODE_IPGRE_TUNN,
	QED_MODE_VXLAN_TUNN,
};

enum qed_tunn_clss {
	QED_TUNN_CLSS_MAC_VLAN,
	QED_TUNN_CLSS_MAC_VNI,
	QED_TUNN_CLSS_INNER_MAC_VLAN,
	QED_TUNN_CLSS_INNER_MAC_VNI,
	QED_TUNN_CLSS_MAC_VLAN_DUAL_STAGE,
	MAX_QED_TUNN_CLSS,
};

struct qed_tunn_update_type {
	bool b_update_mode;
	bool b_mode_enabled;
	enum qed_tunn_clss tun_cls;
};

struct qed_tunn_update_udp_port {
	bool b_update_port;
	u16 port;
};

struct qed_tunnel_info {
	struct qed_tunn_update_type vxlan;
	struct qed_tunn_update_type l2_geneve;
	struct qed_tunn_update_type ip_geneve;
	struct qed_tunn_update_type l2_gre;
	struct qed_tunn_update_type ip_gre;

	struct qed_tunn_update_udp_port vxlan_port;
	struct qed_tunn_update_udp_port geneve_port;

	bool b_update_rx_cls;
	bool b_update_tx_cls;

	bool update_non_l2_vxlan;
	bool non_l2_vxlan_enable;
};

/* The PCI personality is not quite synonymous to protocol ID:
 * 1. All personalities need CORE connections
 * 2. The Ethernet personality may support also the RoCE/iWARP protocol
 */
enum qed_pci_personality {
	QED_PCI_ETH,
	QED_PCI_FCOE,
	QED_PCI_ISCSI,
	QED_PCI_ETH_ROCE,
	QED_PCI_ETH_IWARP,
	QED_PCI_ETH_RDMA,
	QED_PCI_DEFAULT		/* default in shmem */
};

/* All VFs are symetric, all counters are PF + all VFs */
struct qed_qm_iids {
	u32 cids;
	u32 vf_cids;
	u32 tids;
};

/* The PCI relax ordering is either taken care by management FW or can be
 * enable/disable by qed client.
 */
enum qed_pci_rlx_odr {
	QED_DEFAULT_RLX_ODR,
	QED_ENABLE_RLX_ODR,
	QED_DISABLE_RLX_ODR
};

#define MAX_PF_PER_PORT 8

/* HW / FW resources, output of features supported below, most information
 * is received from MFW.
 */
enum qed_resources {
	QED_L2_QUEUE,
	QED_VPORT,
	QED_RSS_ENG,
	QED_PQ,
	QED_RL,
	QED_MAC,
	QED_VLAN,
	QED_VF_RDMA_CNQ_RAM,
	QED_RDMA_CNQ_RAM,
	QED_ILT,
	QED_LL2_RAM_QUEUE,
	QED_LL2_CTX_QUEUE,
	QED_CMDQS_CQS,
	QED_RDMA_STATS_QUEUE,
	QED_BDQ,
	QED_VF_MAC_ADDR,

	/* This is needed only internally for matching against the IGU.
	 * In case of legacy MFW, would be set to `0'.
	 */
	QED_SB,

	QED_MAX_RESC,
};

/* Features that require resources, given as input to the resource management
 * algorithm, the output are the resources above
 */
enum qed_feature {
	QED_PF_L2_QUE,
	QED_PF_TC,
	QED_VF,
	QED_EXTRA_VF_QUE,
	QED_VMQ,
	QED_RDMA_CNQ,
	QED_VF_RDMA_CNQ,
	QED_ISCSI_CQ,
	QED_FCOE_CQ,
	QED_VF_L2_QUE,
	QED_MAX_FEATURES,
};

enum qed_port_mode {
	QED_PORT_MODE_DE_2X40G,
	QED_PORT_MODE_DE_2X50G,
	QED_PORT_MODE_DE_1X100G,
	QED_PORT_MODE_DE_4X10G_F,
	QED_PORT_MODE_DE_4X10G_E,
	QED_PORT_MODE_DE_4X20G,
	QED_PORT_MODE_DE_1X40G,
	QED_PORT_MODE_DE_2X25G,
	QED_PORT_MODE_DE_1X25G,
	QED_PORT_MODE_DE_4X25G,
	QED_PORT_MODE_DE_2X10G,
};

enum qed_dev_cap {
	QED_DEV_CAP_ETH,
	QED_DEV_CAP_FCOE,
	QED_DEV_CAP_ISCSI,
	QED_DEV_CAP_ROCE,
	QED_DEV_CAP_IWARP
};

enum qed_wol_support {
	QED_WOL_SUPPORT_NONE,
	QED_WOL_SUPPORT_PME,
};

struct qed_hw_info {
	/* PCI personality */
	enum qed_pci_personality personality;
#define QED_IS_RDMA_PERSONALITY(dev)			    \
	((dev)->hw_info.personality == QED_PCI_ETH_ROCE ||  \
	 (dev)->hw_info.personality == QED_PCI_ETH_IWARP || \
	 (dev)->hw_info.personality == QED_PCI_ETH_RDMA)
#define QED_IS_ROCE_PERSONALITY(dev)			   \
	((dev)->hw_info.personality == QED_PCI_ETH_ROCE || \
	 (dev)->hw_info.personality == QED_PCI_ETH_RDMA)
#define QED_IS_IWARP_PERSONALITY(dev)			    \
	((dev)->hw_info.personality == QED_PCI_ETH_IWARP || \
	 (dev)->hw_info.personality == QED_PCI_ETH_RDMA)
#define QED_IS_L2_PERSONALITY(dev)		      \
	((dev)->hw_info.personality == QED_PCI_ETH || \
	 QED_IS_RDMA_PERSONALITY(dev))
#define QED_IS_FCOE_PERSONALITY(dev) \
	((dev)->hw_info.personality == QED_PCI_FCOE)
#define QED_IS_ISCSI_PERSONALITY(dev) \
	((dev)->hw_info.personality == QED_PCI_ISCSI)

	/* Resource Allocation scheme results */
	u32 resc_start[QED_MAX_RESC];
	u32 resc_num[QED_MAX_RESC];
	u32 feat_num[QED_MAX_FEATURES];

#define RESC_START(_p_hwfn, resc) ((_p_hwfn)->hw_info.resc_start[resc])
#define RESC_NUM(_p_hwfn, resc) ((_p_hwfn)->hw_info.resc_num[resc])
#define RESC_END(_p_hwfn, resc) (RESC_START(_p_hwfn, resc) + \
				 RESC_NUM(_p_hwfn, resc))
#define FEAT_NUM(_p_hwfn, resc) ((_p_hwfn)->hw_info.feat_num[resc])

	/* Amount of traffic classes HW supports */
	u8 num_hw_tc;

	/* Amount of TCs which should be active according to DCBx or upper layer driver configuration */
	u8 num_active_tc;

	/* The traffic class used by PF for it's offloaded protocol */
	u8 offload_tc;
	bool offload_tc_set;

	bool multi_tc_roce_en;
#define IS_QED_MULTI_TC_ROCE(p_hwfn) (!!(p_hwfn->hw_info.multi_tc_roce_en))

	u32 concrete_fid;
	u16 opaque_fid;
	u16 ovlan;
	u32 part_num[4];

	unsigned char hw_mac_addr[ETH_ALEN];

	u16 num_iscsi_conns;
	u16 num_fcoe_conns;

	struct qed_igu_info *p_igu_info;
	/* Sriov */
	u8 max_chains_per_vf;

	u32 port_mode;
	u32 hw_mode;
	unsigned long device_capabilities;

	u16 mtu;

	enum qed_wol_support b_wol_support;
};

/* maximun size of read/write commands (HW limit) */
#define DMAE_MAX_RW_SIZE        0x2000

struct qed_dmae_info {
	/* Spinlock for synchronizing access to functions */
	spinlock_t lock;

	bool b_mem_ready;

	u8 channel;

	dma_addr_t completion_word_phys_addr;

	/* The memory location where the DMAE writes the completion
	 * value when an operation is finished on this context.
	 */
	u32 *p_completion_word;

	dma_addr_t intermediate_buffer_phys_addr;

	/* An intermediate buffer for DMAE operations that use virtual
	 * addresses - data is DMA'd to/from this buffer and then
	 * memcpy'd to/from the virtual address
	 */
	u32 *p_intermediate_buffer;

	dma_addr_t dmae_cmd_phys_addr;
	struct dmae_cmd *p_dmae_cmd;
};

struct qed_wfq_data {
	u32 default_min_speed;	/* When wfq feature is not configured */
	u32 min_speed;		/* when feature is configured for any 1 vport */
	bool configured;
};

#define OFLD_GRP_SIZE 4

struct qed_offload_pq {
	u8 port;
	u8 tc;
};

enum qed_pqset_aux_offld_type {
	QED_AUX_OFFLOAD_SHARED_PQ_SET,
	QED_AUX_OFFLOAD_DEDICATED_PQ_SET,
	QED_AUX_OFFLOAD_SHARED_DEFAULT,

	QED_AUX_OFFLOAD_PQ_SET_MAX
};

enum qed_pqset_offld_type {
	QED_OFFLOAD_SHARED_PQ_SET,
	QED_OFFLOAD_DEDICATED_PQ_SET,

	QED_OFFLOAD_PQ_SET_MAX
};

struct qed_qm_info {
	struct init_qm_pq_params *qm_pq_params;
	struct init_qm_vport_params *qm_vport_params;
	struct init_qm_rl_params *qm_rl_params;
	struct init_qm_port_params *qm_port_params;
	u16 start_pq;
	u16 start_vport;
	u16 start_rl;
	u16 pure_lb_pq;
	u16 first_ofld_pq;
	u16 first_llt_pq;
	u16 pure_ack_pq;
	u16 ooo_pq;
	u16 single_vf_rdma_pq;
	u16 first_vf_pq;
	u16 first_mcos_pq;
	u16 first_rl_pq;
	u16 first_ofld_grp_pq;
	u16 num_pqs;
	u16 num_vf_pqs;
	u16 ilt_pf_pqs;
	u16 num_vports;
	u16 num_rls;
	u8 max_phys_tcs_per_port;
	u8 ooo_tc;
	bool pq_overflow;
	bool pf_rl_en;
	bool pf_wfq_en;
	bool vport_rl_en;
	bool vport_wfq_en;
	bool vf_rdma_en;
#define IS_QED_QM_VF_RDMA(_p_hwfn) ((_p_hwfn)->qm_info.vf_rdma_en)
	u16 pf_wfq;
	u32 pf_rl;
	struct qed_wfq_data *wfq_data;
	u8 num_pf_rls;
	struct qed_offload_pq offload_group[OFLD_GRP_SIZE];
	u8 offload_group_count;
#define IS_QED_OFLD_GRP(p_hwfn) (p_hwfn->qm_info.offload_group_count > 0)
	u16 num_pqset;
#define QED_PQSET_SUPPORTED(p_hwfn)     (!!(p_hwfn)->qm_info.num_pqset)
#define QED_PQSET_NUM_OF_PQ_TYPES       3
	u16 start_pqset_num;
#define QED_IS_SHARED_DEFAULT_PQSET(_p_hwfn, _pqs_id) \
	(((_pqs_id) == 0) && ((_p_hwfn)->qm_info.start_pqset_num != 0))
	u16 first_pqset_pq;
	u8 pqset_num_offload_pqs;
	u8 pqset_num_aux_pqs;
	enum qed_pqset_offld_type offload_type;
	enum qed_pqset_aux_offld_type aux_offload_type;
	u16 num_pqs_per_pqset;
	u8 relative_wfq_speed;

	/* Locks PQ getters against QM info initialization */
	spinlock_t qm_info_lock;
};

#define QED_OVERFLOW_BIT        1

struct qed_db_recovery_info {
	struct list_head list;
	spinlock_t lock;
	u32 count;

	/* PF doorbell overflow sticky indicator was cleared in the DORQ
	 * attention callback, but still needs to execute doorbell recovery.
	 * Full (REAL_DEAL) dorbell recovery is executed in the periodic
	 * handler.
	 * This value doesn't require a lock but must use atomic operations.
	 */
	unsigned long overflow;

	/* Indicates that DORQ attention was handled in qed_int_deassertion */
	bool dorq_attn;

	/* Indicates that doorbell recovery mechanism setup is done */
	bool setup_done;
};

struct storm_stats {
	u32 address;
	u32 len;
};

struct qed_fw_data {
#ifdef CONFIG_QED_BINARY_FW
	struct fw_ver_info *fw_ver_info;
#endif
	const u8 *modes_tree_buf;
	union init_op *init_ops;
	const u32 *arr_data;
	const u32 *fw_overlays;
	u32 fw_overlays_len;
	u32 init_ops_size;
};

enum qed_mf_mode_bit {
	/* Supports PF-classification based on tag */
	QED_MF_OVLAN_CLSS,

	/* Supports PF-classification based on MAC */
	QED_MF_LLH_MAC_CLSS,

	/* Supports PF-classification based on protocol type */
	QED_MF_LLH_PROTO_CLSS,

	/* Requires a default PF to be set */
	QED_MF_NEED_DEF_PF,

	/* Allow LL2 to multicast/broadcast */
	QED_MF_LL2_NON_UNICAST,

	/* Allow Cross-PF [& child VFs] Tx-switching */
	QED_MF_INTER_PF_SWITCH,

	/* TODO - if we ever re-utilize any of this logic, we can rename */
	QED_MF_UFP_SPECIFIC,

	QED_MF_DISABLE_ARFS,

	/* Use vlan for steering */
	QED_MF_8021Q_TAGGING,

	/* Use stag for steering */
	QED_MF_8021AD_TAGGING,

	/* Allow DSCP to TC mapping */
	QED_MF_DSCP_TO_TC_MAP,

	/* Allow FIP discovery fallback */
	QED_MF_FIP_SPECIAL,

	/* Do not insert a vlan tag with id 0 */
	QED_MF_DONT_ADD_VLAN0_TAG,

	/* Allow VF RDMA */
	QED_MF_VF_RDMA,

	/* Allow RoCE LAG */
	QED_MF_ROCE_LAG,

	/* QinQ support */
	QED_MF_QINQ_SPECIFIC,

	/* Large ILT size */
	QED_MF_LARGE_ILT,
};

enum qed_ufp_mode {
	QED_UFP_MODE_ETS,
	QED_UFP_MODE_VNIC_BW,
	QED_UFP_MODE_UNKNOWN
};

enum qed_ufp_pri_type {
	QED_UFP_PRI_OS,
	QED_UFP_PRI_VNIC,
	QED_UFP_PRI_UNKNOWN
};

struct qed_ufp_info {
	enum qed_ufp_pri_type pri_type;
	enum qed_ufp_mode mode;
	u8 tc;
};

enum BAR_ID {
	BAR_ID_0,		/* used for GRC */
	BAR_ID_1		/* Used for doorbells */
};

struct qed_nvm_image_info {
	u32 num_images;
	struct bist_nvm_image_att *image_att;
	bool valid;
};

#define LAG_MAX_PORT_NUM        2

struct qed_lag_info {
	enum qed_lag_type lag_type;
	void (*link_change_cb) (void *cxt);
	void *cxt;
	u8 port_num;
	unsigned long active_ports;
	u8 first_port;
	u8 second_port;
	bool is_master;
	u8 master_pf;
};

/* PWM region specific data */
struct qed_dpi_info {
	u16 wid_count;
	u32 dpi_size;
	u32 dpi_count;
	u32 dpi_start_offset;	/* this is used to calculate
				 * the doorbell address
				 */
	u32 dpi_bit_shift_addr;
};

enum qed_hsi_def_type {
	QED_HSI_DEF_MAX_NUM_VFS,
	QED_HSI_DEF_MAX_NUM_L2_QUEUES,
	QED_HSI_DEF_MAX_NUM_PORTS,
	QED_HSI_DEF_MAX_SB_PER_PATH,
	QED_HSI_DEF_MAX_NUM_PFS,
	QED_HSI_DEF_MAX_NUM_VPORTS,
	QED_HSI_DEF_NUM_ETH_RSS_ENGINE,
	QED_HSI_DEF_MAX_QM_TX_QUEUES,
	QED_HSI_DEF_NUM_PXP_ILT_RECORDS,
	QED_HSI_DEF_NUM_RDMA_STATISTIC_COUNTERS,
	QED_HSI_DEF_MAX_QM_GLOBAL_RLS,
	QED_HSI_DEF_MAX_PBF_CMD_LINES,
	QED_HSI_DEF_MAX_BTB_BLOCKS,
	QED_NUM_HSI_DEFS
};

#define DRV_MODULE_VERSION		      \
	__stringify(QED_MAJOR_VERSION) "."    \
	__stringify(QED_MINOR_VERSION) "."    \
	__stringify(QED_REVISION_VERSION) "." \
	__stringify(QED_ENGINEERING_VERSION)

#ifndef QED_UPSTREAM
#define QED_ETH_INTERFACE_VERSION       7012
#define QED_ISCSI_INTERFACE_VERSION     7012
#define QED_FCOE_INTERFACE_VERSION      7012
#define QED_ROCE_INTERFACE_VERSION      7012
#endif

struct qed_simd_fp_handler {
	void *token;
	void (*func) (void *);
};

enum qed_slowpath_wq_flag {
	QED_SLOWPATH_MFW_TLV_REQ,
	QED_SLOWPATH_PERIODIC_DB_REC,
	QED_SLOWPATH_SFP_UPDATE,
	QED_SLOWPATH_SFP_TX_FLT,
	QED_SLOWPATH_SFP_RX_LOS,
	QED_SLOWPATH_ACTIVE,
	QED_SLOWPATH_RESCHEDULE,
};

struct qed_hwfn {
	struct qed_dev *cdev;
	u8 my_id;		/* ID inside the PF */
#define IS_LEAD_HWFN(_p_hwfn)           (!((_p_hwfn)->my_id))
#define IS_AFFIN_HWFN(_p_hwfn) \
	((_p_hwfn) == QED_AFFIN_HWFN((_p_hwfn)->cdev))
	u8 rel_pf_id;		/* Relative to engine */
	u8 abs_pf_id;
#define QED_PATH_ID(_p_hwfn) \
	(QED_IS_BB((_p_hwfn)->cdev) ? ((_p_hwfn)->abs_pf_id & 1) : 0)
	u8 port_id;
	bool b_active;

	u32 dp_module;
	u8 dp_level;
	u32 dp_int_module;
	u8 dp_int_level;
	char name[NAME_SIZE];
	void *dp_ctx;

	bool hw_init_done;

	u8 num_funcs_on_engine;
	u8 enabled_func_idx;
	u8 num_funcs_on_port;

	/* BAR access */
	void __iomem *regview;
	void __iomem *doorbells;
	u64 db_phys_addr;
	unsigned long db_size;

	/* PTT pool */
	struct qed_ptt_pool *p_ptt_pool;

	/* HW info */
	struct qed_hw_info hw_info;

	/* rt_array (for init-tool) */
	struct qed_rt_data rt_data;

	/* SPQ */
	struct qed_spq *p_spq;

	/* EQ */
	struct qed_eq *p_eq;

	/* Consolidate Q */
	struct qed_consq *p_consq;

	/* Slow-Path definitions */
	struct tasklet_struct *sp_dpc;
	bool b_sp_dpc_enabled;

	struct qed_ptt *p_main_ptt;
	struct qed_ptt *p_dpc_ptt;

	/* PTP will be used only by the leading funtion.
	 * Usage of all PTP-apis should be synchronized as result.
	 */
	struct qed_ptt *p_ptp_ptt;

	struct qed_sb_sp_info *p_sp_sb;
	struct qed_sb_attn_info *p_sb_attn;

	/* Protocol related */
	bool using_ll2;
	struct qed_ll2_info *p_ll2_info;
	struct qed_ooo_info *p_ooo_info;
	struct qed_iscsi_info *p_iscsi_info;
	struct qed_fcoe_info *p_fcoe_info;
	struct qed_rdma_info *p_rdma_info;
	struct qed_pf_params pf_params;

	bool b_rdma_enabled_in_prs;
	u32 rdma_prs_search_reg;

	struct qed_cxt_mngr *p_cxt_mngr;

	/* Flag indicating whether interrupts are enabled or not */
	bool b_int_enabled;
	bool b_int_requested;

	/* True if the driver requests for the link */
	bool b_drv_link_init;

	struct qed_vf_iov *vf_iov_info;
	struct qed_pf_iov *pf_iov_info;
	struct qed_mcp_info *mcp_info;
	struct qed_dcbx_info *p_dcbx_info;
	struct qed_ufp_info ufp_info;

	struct qed_dmae_info dmae_info;

	/* QM init */
	struct qed_qm_info qm_info;

	/* Buffer for unzipping firmware data */
#ifdef CONFIG_QED_ZIPPED_FW
	void *unzip_buf;
#endif

	struct dbg_tools_data dbg_info;
	void *dbg_user_info;
	struct virt_mem_desc dbg_arrays[MAX_BIN_DBG_BUFFER_TYPE];

	struct qed_dpi_info dpi_info;

	u8 roce_edpm_mode;
	/* If one of the following is set then EDPM shouldn't be used */
	u8 dcbx_no_edpm;
	u8 db_bar_no_edpm;
	u8 num_vf_cnqs;

	/* L2-related */
	struct qed_l2_info *p_l2_info;

	/* Mechanism for recovering from doorbell drop */
	struct qed_db_recovery_info db_recovery_info;

	/* Enable/disable pacing, if request to enable then
	 * IOV and mcos configuration will be skipped.
	 * this actually reflects the value requested in
	 * struct qed_hw_prepare_params by qed client.
	 */
	bool b_en_pacing;

	/* Enable DCQCN and skip IOV configuration. Reflects the value
	 * requested in hw_prepare_params by the QED client.
	 */
	bool b_en_dcqcn;

	struct qed_lag_info lag_info;

	/* Nvm images number and attributes */
	struct qed_nvm_image_info nvm_info;

	struct phys_mem_desc *fw_overlay_mem;
	int (*p_dummy_cb)
	 (struct qed_hwfn *, void *);

	/* TC value to be used in QinQ header */
	u8 qinq_tc;
	spinlock_t *qm_lock;

	/* retry count to acquire MCP resource lock */
	u8 mcp_resc_lock_retry_cnt;

	/* Skip MCP Drain if it is already in progress */
	bool mcp_drain_inprogress;
	struct qed_ptt *p_arfs_ptt;
	struct qed_simd_fp_handler simd_proto_handler[64];

#ifdef CONFIG_QED_SRIOV
	struct workqueue_struct *iov_wq;
	struct delayed_work iov_task;
	unsigned long iov_task_flags;
#endif
	struct z_stream_s *stream;
	struct workqueue_struct *slowpath_wq;
	struct delayed_work slowpath_task;
	unsigned long slowpath_task_flags;
	u32 periodic_db_rec_count;
};

struct qed_filter_ucast;
struct qed_vf_info;
struct qed_eth_cb_ops;
struct qed_dev_info;
struct qed_cb_ll2_info;
struct qed_public_vf_info;
struct qed_vf_acquire_sw_info;
union qed_mcp_protocol_stats;
enum qed_mcp_protocol_type;
enum qed_mfw_tlv_type;
union qed_mfw_tlv_data;
enum qed_hw_info_change;

struct pci_params {
	int pm_cap;

	unsigned long mem_start;
	unsigned long mem_end;
	unsigned int irq;
	u8 pf_num;
};

struct qed_int_param {
	u32 int_mode;
	u16 num_vectors;
	u16 min_msix_cnt;	/* for minimal functionality */
};

struct qed_int_params {
	struct qed_int_param in;
	struct qed_int_param out;
	struct msix_entry *msix_table;
	bool fp_initialized;
	u16 fp_msix_base;
	u16 fp_msix_cnt;
	u16 rdma_msix_base;
	u16 rdma_msix_cnt;
};

#if defined(CONFIG_DEBUG_FS)
struct qed_dbg_feature {
	struct dentry *dentry;
	u8 *dump_buf;
	u32 buf_size;
	u32 dumped_dwords;
};

/* info of chain for printing */
struct chain_print_struct {
	bool b_key_entered;
	char *buffer;
	u32 current_index;
	u32 final_index;
	bool print_metadata;
	struct qed_chain *chain;
};

enum qed_dbg_mdump_cmd {
	QED_DBG_MDUMP_CMD_NONE,
	QED_DBG_MDUMP_CMD_STATUS,
	QED_DBG_MDUMP_CMD_TRIGGER,
	QED_DBG_MDUMP_CMD_DUMP,
	QED_DBG_MDUMP_CMD_CLEAR,
	QED_DBG_MDUMP_CMD_GET_RETAIN,
	QED_DBG_MDUMP_CMD_CLR_RETAIN,
};

struct qed_dbg_mdump {
	enum qed_dbg_mdump_cmd cmd;
	u8 *buf;
	u32 buf_size;
};
#endif

enum qed_vf_mac_origin {
	QED_VF_MAC_NVRAM_OR_ZERO,
	QED_VF_MAC_NVRAM_OR_RANDOM,
	QED_VF_MAC_ZERO,
	QED_VF_MAC_RANDOM,
};

struct qed_dev {
	u32 dp_module;
	u8 dp_level;
	char name[NAME_SIZE];
	void *dp_ctx;
	struct qed_internal_trace internal_trace;
	u8 dp_int_level;
	u32 dp_int_module;

/* for work DP_* macros with cdev, hwfn, etc */
	struct qed_dev *cdev;
	enum qed_dev_type type;
/* Translate type/revision combo into the proper conditions */
#define QED_IS_BB(dev)  ((dev)->type == QED_DEV_TYPE_BB)
#define QED_IS_BB_A0(dev)       (QED_IS_BB(dev) && CHIP_REV_IS_A0(dev))
#ifndef ASIC_ONLY
#define QED_IS_BB_B0(dev)       ((QED_IS_BB(dev) && CHIP_REV_IS_B0(dev)) || \
				 (CHIP_REV_IS_TEDIBEAR(dev)))
#else
#define QED_IS_BB_B0(dev)       (QED_IS_BB(dev) && CHIP_REV_IS_B0(dev))
#endif
#define QED_IS_AH(dev)  ((dev)->type == QED_DEV_TYPE_AH)
#define QED_IS_K2(dev)  QED_IS_AH(dev)

	u16 vendor_id;
	u16 device_id;
#define QED_DEV_ID_MASK 0xff00
#define QED_DEV_ID_MASK_BB      0x1600
#define QED_DEV_ID_MASK_AH      0x8000

	u16 chip_num;
#define CHIP_NUM_MASK                   0xffff
#define CHIP_NUM_SHIFT                  0

	u8 chip_rev;
#define CHIP_REV_MASK                   0xf
#define CHIP_REV_SHIFT                  0
#ifndef ASIC_ONLY
#define CHIP_REV_IS_TEDIBEAR(_cdev)     ((_cdev)->chip_rev == 0x5)
#define CHIP_REV_IS_EMUL_A0(_cdev)      ((_cdev)->chip_rev == 0xe)
#define CHIP_REV_IS_EMUL_B0(_cdev)      ((_cdev)->chip_rev == 0xc)
#define CHIP_REV_IS_EMUL(_cdev)	\
	(CHIP_REV_IS_EMUL_A0(_cdev) || CHIP_REV_IS_EMUL_B0(_cdev))
#define CHIP_REV_IS_FPGA_A0(_cdev)      ((_cdev)->chip_rev == 0xf)
#define CHIP_REV_IS_FPGA_B0(_cdev)      ((_cdev)->chip_rev == 0xd)
#define CHIP_REV_IS_FPGA(_cdev)	\
	(CHIP_REV_IS_FPGA_A0(_cdev) || CHIP_REV_IS_FPGA_B0(_cdev))
#define CHIP_REV_IS_SLOW(_cdev)	\
	(CHIP_REV_IS_EMUL(_cdev) || CHIP_REV_IS_FPGA(_cdev))
#define CHIP_REV_IS_A0(_cdev)					     \
	(CHIP_REV_IS_EMUL_A0(_cdev) || CHIP_REV_IS_FPGA_A0(_cdev) || \
	 (!(_cdev)->chip_rev && !(_cdev)->chip_metal))
#define CHIP_REV_IS_B0(_cdev)					     \
	(CHIP_REV_IS_EMUL_B0(_cdev) || CHIP_REV_IS_FPGA_B0(_cdev) || \
	 ((_cdev)->chip_rev == 1 && !(_cdev)->chip_metal))
#define CHIP_REV_IS_ASIC(_cdev) !CHIP_REV_IS_SLOW(_cdev)
#else
#define CHIP_REV_IS_A0(_cdev) \
	(!(_cdev)->chip_rev && !(_cdev)->chip_metal)
#define CHIP_REV_IS_B0(_cdev) \
	((_cdev)->chip_rev == 1 && !(_cdev)->chip_metal)
#endif

	u8 chip_metal;
#define CHIP_METAL_MASK                 0xff
#define CHIP_METAL_SHIFT                0

	u8 chip_bond_id;
#define CHIP_BOND_ID_MASK               0xff
#define CHIP_BOND_ID_SHIFT              0

	u8 num_ports;
	u8 num_ports_in_engine;

	u8 path_id;

	unsigned long mf_bits;

	int pcie_width;
	int pcie_speed;

	/* Add MF related configuration */
	u8 mcp_rev;
	u8 boot_mode;

	/* WoL related configurations */
	u8 wol_config;
	u8 wol_mac[ETH_ALEN];

	u32 int_mode;
	enum qed_coalescing_mode int_coalescing_mode;
	u16 rx_coalesce_usecs;
	u16 tx_coalesce_usecs;

	/* Start Bar offset of first hwfn */
	void __iomem *regview;
	void __iomem *doorbells;
	u64 db_phys_addr;
	unsigned long db_size;

	/* PCI */
	u8 cache_shift;

	/* Init */
	const u32 *iro_arr;
#define IRO     ((const struct iro *)p_hwfn->cdev->iro_arr)

	/* HW functions */
	u8 num_hwfns;
	struct qed_hwfn hwfns[MAX_HWFNS_PER_DEVICE];
#define QED_LEADING_HWFN(dev)           (&dev->hwfns[0])
#define QED_IS_CMT(dev)         ((dev)->num_hwfns > 1)

	/* Engine affinity */
	u8 l2_affin_hint;
	u8 fir_affin;
	u8 iwarp_affin;
	/* Macro for getting the engine-affinitized hwfn for FCoE/iSCSI/RoCE */
#define QED_FIR_AFFIN_HWFN(dev) (&dev->hwfns[dev->fir_affin])
	/* Macro for getting the engine-affinitized hwfn for iWARP */
#define QED_IWARP_AFFIN_HWFN(dev)       (&dev->hwfns[dev->iwarp_affin])
	/* Generic macro for getting the engine-affinitized hwfn */
#define QED_AFFIN_HWFN(dev)				   \
	(QED_IS_IWARP_PERSONALITY(QED_LEADING_HWFN(dev)) ? \
	 QED_IWARP_AFFIN_HWFN(dev) :			   \
	 QED_FIR_AFFIN_HWFN(dev))
	/* Macro for getting the index (0/1) of the engine-affinitized hwfn */
#define QED_AFFIN_HWFN_IDX(dev)	\
	(IS_LEAD_HWFN(QED_AFFIN_HWFN(dev)) ? 0 : 1)

	/* SRIOV */
	struct qed_hw_sriov_info *p_iov_info;
#define IS_QED_SRIOV(cdev)              (!!(cdev)->p_iov_info)
	struct qed_tunnel_info tunnel;
	bool b_is_vf;
	bool b_dont_override_vf_msix;

	u32 drv_type;

	u32 rdma_max_sge;
	u32 rdma_max_inline;
	u32 rdma_max_srq_sge;
	u8 ilt_page_size;

	struct qed_eth_stats *reset_stats;
	struct qed_fw_data *fw_data;

	u32 mcp_nvm_resp;

	/* Recovery */
	bool recov_in_prog;

	/* Indicates whether should prevent attentions from being reasserted */
	bool attn_clr_en;

	/* Indicates whether allowing the MFW to collect a crash dump */
	bool allow_mdump;

	/* Indicates if the reg_fifo is checked after any register access */
	bool chk_reg_fifo;

	/* Indicates the monitored address by qed_rd()/qed_wr() */
	u32 monitored_hw_addr;

#ifndef ASIC_ONLY
	bool b_is_emul_full;
	bool b_is_emul_mac;
#endif
	/* LLH info */
	u8 ppfid_bitmap;
	struct qed_llh_info *p_llh_info;

	/* Indicates whether this PF serves a storage target */
	bool b_is_target;

	/* AER in progress */
	bool aer_in_prog;

	/* PCI Host reset assertions received by mfw */
	u16 hot_reset_count;

	/* Instruct driver to read statistics from the specified bin id */
	u8 stats_bin_id;

	/* Linux specific here */
	struct qed_dev_info common_dev_info;
	struct pci_dev *pdev;
	u32 flags;
#define QED_FLAG_STORAGE_STARTED (1 << 0)	/* TODO - very likely this isn't the
						 * correct place, but didn't want to
						 * start protocol-specific structs.
						 */

	int msg_enable;

	struct pci_params pci_params;

	struct qed_int_params int_params;

	u8 protocol;
#define IS_QED_ETH_IF(cdev)     ((cdev)->protocol == QED_PROTOCOL_ETH)
#define IS_QED_FCOE_IF(cdev)    ((cdev)->protocol == QED_PROTOCOL_FCOE)
#define IS_QED_ISCSI_IF(cdev)   ((cdev)->protocol == QED_PROTOCOL_ISCSI)

	/* Callbacks to protocol driver */
	union {
		struct qed_common_cb_ops *common;
		struct qed_eth_cb_ops *eth;
		struct qed_fcoe_cb_ops *fcoe;
		struct qed_iscsi_cb_ops *iscsi;
	} protocol_ops;
	void *ops_cookie;

#ifdef CONFIG_QED_LL2
	struct qed_cb_ll2_info *ll2;
	u8 ll2_mac_address[ETH_ALEN];
#endif

#ifdef CONFIG_DEBUG_FS
	struct qed_dbg_feature dbg_features[DBG_FEATURE_NUM];
	struct dentry *bdf_dentry;
	bool test_result_available;
	struct kobject *bdf_kobj;
#define QED_TEST_RESULT_LENGTH  11	/* 10 chars for max int in decimal + '\0' */
	char test_result[QED_TEST_RESULT_LENGTH + 1];	/* +1 for '\n' */
	struct chain_print_struct chain_info;
	u8 engine_for_debug;
	bool disable_ilt_dump;
	bool recording_active;
	bool disable_internal_trace_dump;
	struct qed_dbg_mdump dbg_mdump;
	u8 dbg_bin_dump;
#endif

	 DECLARE_HASHTABLE(connections, 10);
#ifdef CONFIG_QED_BINARY_FW
	const struct firmware *firmware;
#ifndef QED_UPSTREAM
	u8 *fw_buf;
#endif
#endif

	u8 *p_dbg_data_buf;
	u32 dbg_data_buf_size;
	bool print_dbg_data;
#ifndef QED_UPSTREAM
	bool b_dump_dbg_data;
	u8 dbg_data_path[PATH_MAX];
#define QED_DBG_DATA_FILE_NAME_SIZE     60
#define QED_DBG_DATA_PATH_MAX_SIZE      (PATH_MAX - QED_DBG_DATA_FILE_NAME_SIZE)
#endif

	u16 tunn_feature_mask;

	u16 num_l2_queues;

#ifndef QED_UPSTREAM		/* ! QED_UPSTREAM */
	/* Controls whether to commit dcbx config from debugfs to MFW or not */
	bool b_dcbx_cfg_commit;
#endif
	bool tx_switching;
	enum qed_vf_mac_origin vf_mac_origin;

#ifdef _HAS_DEVLINK		/* QED_UPSTREAM */
	bool iwarp_cmt;
#endif
	/* Indiactes whether cdev needs to be retained during recovery or not */
	bool b_reuse_dev;
};

u16 qed_get_hsi_def_val(struct qed_dev *cdev, enum qed_hsi_def_type type);

#define NUM_OF_VFS(dev)	\
	qed_get_hsi_def_val(dev, QED_HSI_DEF_MAX_NUM_VFS)
#define NUM_OF_L2_QUEUES(dev) \
	qed_get_hsi_def_val(dev, QED_HSI_DEF_MAX_NUM_L2_QUEUES)
#define NUM_OF_PORTS(dev) \
	qed_get_hsi_def_val(dev, QED_HSI_DEF_MAX_NUM_PORTS)
#define NUM_OF_SBS(dev)	\
	qed_get_hsi_def_val(dev, QED_HSI_DEF_MAX_SB_PER_PATH)
#define NUM_OF_ENG_PFS(dev) \
	qed_get_hsi_def_val(dev, QED_HSI_DEF_MAX_NUM_PFS)
#define NUM_OF_VPORTS(dev) \
	qed_get_hsi_def_val(dev, QED_HSI_DEF_MAX_NUM_VPORTS)
#define NUM_OF_RSS_ENGINES(dev)	\
	qed_get_hsi_def_val(dev, QED_HSI_DEF_NUM_ETH_RSS_ENGINE)
#define NUM_OF_QM_TX_QUEUES(dev) \
	qed_get_hsi_def_val(dev, QED_HSI_DEF_MAX_QM_TX_QUEUES)
#define NUM_OF_PXP_ILT_RECORDS(dev) \
	qed_get_hsi_def_val(dev, QED_HSI_DEF_NUM_PXP_ILT_RECORDS)
#define NUM_OF_RDMA_STATISTIC_COUNTERS(dev) \
	qed_get_hsi_def_val(dev, QED_HSI_DEF_NUM_RDMA_STATISTIC_COUNTERS)
#define NUM_OF_QM_GLOBAL_RLS(dev) \
	qed_get_hsi_def_val(dev, QED_HSI_DEF_MAX_QM_GLOBAL_RLS)
#define NUM_OF_PBF_CMD_LINES(dev) \
	qed_get_hsi_def_val(dev, QED_HSI_DEF_MAX_PBF_CMD_LINES)
#define NUM_OF_BTB_BLOCKS(dev) \
	qed_get_hsi_def_val(dev, QED_HSI_DEF_MAX_BTB_BLOCKS)

#define QED_RECOV_IN_PROG(_cdev) \
	((_cdev)->recov_in_prog || (_cdev)->aer_in_prog)

/**
 * @brief qed_concrete_to_sw_fid - get the sw function id from
 *        the concrete value.
 *
 * @param concrete_fid
 *
 * @return inline u8
 */
static inline u8 qed_concrete_to_sw_fid(u32 concrete_fid)
{
	u8 vfid = GET_FIELD(concrete_fid, PXP_CONCRETE_FID_VFID);
	u8 pfid = GET_FIELD(concrete_fid, PXP_CONCRETE_FID_PFID);
	u8 vf_valid = GET_FIELD(concrete_fid,
				PXP_CONCRETE_FID_VFVALID);
	u8 sw_fid;

	if (vf_valid)
		sw_fid = vfid + MAX_NUM_PFS;
	else
		sw_fid = pfid;

	return sw_fid;
}

#define PKT_LB_TC 9

int qed_configure_vport_wfq(struct qed_dev *cdev, u16 vp_id, u32 rate);
void qed_configure_vp_wfq_on_link_change(struct qed_dev *cdev,
					 struct qed_ptt *p_ptt,
					 u32 min_pf_rate);

int qed_configure_pf_max_bandwidth(struct qed_dev *cdev, u8 max_bw);
int qed_configure_pf_min_bandwidth(struct qed_dev *cdev, u8 min_bw);
void qed_clean_wfq_db(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
int qed_device_num_engines(struct qed_dev *cdev);
int qed_device_num_ports(struct qed_dev *cdev);
void qed_set_fw_mac_addr(__le16 * fw_msb,
			 __le16 * fw_mid, __le16 * fw_lsb, u8 * mac);

#define QED_TOS_ECN_SHIFT       0
#define QED_TOS_ECN_MASK        0x3
#define QED_TOS_DSCP_SHIFT      2
#define QED_TOS_DSCP_MASK       0x3f
#define QED_VLAN_PRIO_SHIFT     13
#define QED_VLAN_PRIO_MASK      0x7

/* Flags for indication of required queues */
#define PQ_FLAGS_RLS    (1 << 0)
#define PQ_FLAGS_MCOS   (1 << 1)
#define PQ_FLAGS_LB     (1 << 2)
#define PQ_FLAGS_OOO    (1 << 3)
#define PQ_FLAGS_ACK    (1 << 4)
#define PQ_FLAGS_OFLD   (1 << 5)
#define PQ_FLAGS_GRP    (1 << 6)
#define PQ_FLAGS_VFS    (1 << 7)
#define PQ_FLAGS_LLT    (1 << 8)
#define PQ_FLAGS_MTC    (1 << 9)
#define PQ_FLAGS_VFR    BIT(10)
#define PQ_FLAGS_VSR    BIT(11)
#define PQ_FLAGS_PQSET  BIT(12)

#define QED_DEFAULT_PQ_SET      0
u16 qed_get_pq_idx(struct qed_hwfn *p_hwfn, u32 pq_flags, u16 pq_set, u8 tc);
/* physical queue index for cm context intialization */
u16 qed_get_cm_pq_idx(struct qed_hwfn *p_hwfn, u32 pq_flags);

u16 qed_get_cm_pq_idx_tc(struct qed_hwfn *p_hwfn,
			 u32 pq_flags, u16 pq_set, u8 tc);

u16 qed_get_cm_pq_idx_vf(struct qed_hwfn *p_hwfn, u16 vf, u16 pq_set_id, u8 tc);

u16 qed_get_cm_pq_idx_vf_ll2(struct qed_hwfn *p_hwfn,
			     u16 vf, u16 pq_set_id, u8 tc);

u16 qed_get_cm_pq_idx_vf_rdma(struct qed_hwfn *p_hwfn,
			      u16 vf, u8 tc, u16 pq_set_id);

u16 qed_get_cm_pq_idx_vf_rdma_llt(struct qed_hwfn *p_hwfn,
				  u16 vf, u8 tc, u16 pq_set_id);

u16 qed_get_cm_pq_idx_rl(struct qed_hwfn *p_hwfn, u16 rl);

u16 qed_get_cm_pq_idx_grp(struct qed_hwfn *p_hwfn, u8 idx);

u16 qed_get_cm_pq_idx_ofld_mtc(struct qed_hwfn *p_hwfn,
			       u16 idx, u8 tc, u16 pq_set_id);

u16 qed_get_cm_pq_idx_llt_mtc(struct qed_hwfn *p_hwfn,
			      u16 idx, u8 tc, u16 pq_set_id);

u16 qed_get_cm_pq_idx_ll2(struct qed_hwfn *p_hwfn, u8 tc);

/* qm vport/rl for rate limit configuration */
u16 qed_get_pq_vport_id_from_rl(struct qed_hwfn *p_hwfn, u16 rl);
u16 qed_get_pq_vport_id_from_vf(struct qed_hwfn *p_hwfn, u16 vf);
u16 qed_get_pq_rl_id_from_rl(struct qed_hwfn *p_hwfn, u16 rl);
u16 qed_get_pq_rl_id_from_vf(struct qed_hwfn *p_hwfn, u16 vf);

const char *qed_hw_get_resc_name(enum qed_resources res_id);

/* doorbell recovery mechanism */
void qed_db_recovery_dp(struct qed_hwfn *p_hwfn);
void qed_db_recovery_execute(struct qed_hwfn *p_hwfn);
int
qed_db_rec_flush_queue(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt, u32 usage_cnt_reg, u32 * count);
#define QED_DB_REC_COUNT        1000
#define QED_DB_REC_INTERVAL     100

bool qed_edpm_enabled(struct qed_hwfn *p_hwfn);

int qed_hw_init_dpi_size(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 struct qed_dpi_info *dpi_info,
			 u32 pwm_region_size, u32 n_cpus);

/* amount of resources used in qm init */
u8 qed_init_qm_get_num_tcs(struct qed_hwfn *p_hwfn);
u16 qed_init_qm_get_num_vfs(struct qed_hwfn *p_hwfn);
u16 qed_init_qm_get_num_pf_rls(struct qed_hwfn *p_hwfn);
u16 qed_init_qm_get_num_vports(struct qed_hwfn *p_hwfn);
u16 qed_init_qm_get_num_pqs(struct qed_hwfn *p_hwfn);

void qed_hw_info_set_offload_tc(struct qed_hw_info *p_info, u8 tc);
u8 qed_get_offload_tc(struct qed_hwfn *p_hwfn);
int
qed_pq_set_get_pqs_of_tc(struct qed_hwfn *p_hwfn,
			 u16 pq_set, u8 tc, u16 * pqs, u8 num_pqs);

#define MFW_PORT(_p_hwfn)       ((_p_hwfn)->abs_pf_id %	\
				 qed_device_num_ports((_p_hwfn)->cdev))

int qed_abs_ppfid(struct qed_dev *cdev, u8 rel_ppfid, u8 * p_abs_ppfid);
int qed_llh_map_ppfid_to_pfid(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, u8 ppfid, u8 pfid);

/* The PFID<->PPFID calculation is based on the relative index of a PF on its
 * port. In BB there is a bug in the LLH in which the PPFID is actually engine
 * based, and thus it equals the PFID.
 */
#define QED_PFID_BY_PPFID(_p_hwfn, abs_ppfid)		      \
	(QED_IS_BB((_p_hwfn)->cdev) ?			      \
	 (abs_ppfid) :					      \
	 (abs_ppfid) * (_p_hwfn)->cdev->num_ports_in_engine + \
	 MFW_PORT(_p_hwfn))
#define QED_PPFID_BY_PFID(_p_hwfn)    \
	(QED_IS_BB((_p_hwfn)->cdev) ? \
	 (_p_hwfn)->rel_pf_id :	      \
	 (_p_hwfn)->rel_pf_id / (_p_hwfn)->cdev->num_ports_in_engine)

int qed_all_ppfids_wr(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, u32 addr, u32 val);

/* Utility functions for dumping the content of the NIG LLH filters */
int qed_llh_dump_ppfid(struct qed_dev *cdev, u8 ppfid);
int qed_llh_dump_all(struct qed_dev *cdev);

/**
 * @brief qed_set_platform_str - Set the debug dump platform string.
 * Write the qed version and device's string to the given buffer.
 *
 * @param p_hwfn
 * @param buf_str
 * @param buf_size
 */
void qed_set_platform_str(struct qed_hwfn *p_hwfn, char *buf_str, u32 buf_size);

#define TSTORM_QZONE_START      PXP_VF_BAR0_START_SDM_ZONE_A

#define MSTORM_QZONE_START(dev)	\
	(TSTORM_QZONE_START + (TSTORM_QZONE_SIZE * NUM_OF_L2_QUEUES(dev)))

#define USTORM_QZONE_START(dev) (MSTORM_QZONE_START(dev) + \
				 (MSTORM_QZONE_SIZE * NUM_OF_L2_QUEUES(dev)))

#define GET_GTT_REG_ADDR(__base, __offset, __idx) \
	((__base) + __offset ## _GTT_OFFSET((__idx)))

#define GET_GTT_BDQ_REG_ADDR(__base, __offset, __idx, __bdq_idx) \
	((__base) + __offset ## _GTT_OFFSET((__idx), (__bdq_idx)))

/* Other Linux specific common definitions */
#define DP_NAME(cdev) ((cdev)->name)
#define DP_TRACK(cdev) (0)

#define REG_ADDR(cdev, offset)          (void __iomem *)((u8 __iomem *)	   \
							 (cdev->regview) + \
							 (offset))

#define REG_RD(cdev, offset)            ({ rmb(); readl(REG_ADDR(cdev, offset)); })
#define REG_RD8(cdev, offset)           ({ rmb(); readb(REG_ADDR(cdev, offset)); })
#define REG_RD16(cdev, offset)          ({ rmb(); readw(REG_ADDR(cdev, offset)); })

#define REG_WR(cdev, offset, \
	       val)       ({ writel((u32)val, REG_ADDR(cdev, offset)); wmb(); })
#define REG_WR8(cdev, offset, \
		val)      ({ writeb((u8)val, REG_ADDR(cdev, offset)); wmb(); })
#define REG_WR16(cdev, offset, \
		 val)     ({ writew((u16)val, REG_ADDR(cdev, offset)); wmb(); })

#define QED_DB_SHIFT    5	/* 32 bytes - check if correct for E4 */

/* Prototypes */
int qed_fill_dev_info(struct qed_dev *cdev, struct qed_dev_info *dev_info);
void qed_link_update(struct qed_hwfn *hwfn, struct qed_ptt *ptt);
void qed_bw_update(struct qed_hwfn *hwfn, struct qed_ptt *ptt);
void qed_transceiver_update(struct qed_hwfn *p_hwfn);
void qed_transceiver_tx_fault(struct qed_hwfn *p_hwfn);
void qed_transceiver_rx_los(struct qed_hwfn *p_hwfn);
void qed_dcbx_aen(struct qed_hwfn *hwfn, u32 mib_type);
u32 qed_unzip_data(struct qed_hwfn *p_hwfn,
		   u32 input_len, u8 * input_buf, u32 max_size, u8 * unzip_buf);
int qed_recovery_process(struct qed_dev *cdev);
void qed_schedule_recovery_handler(struct qed_hwfn *p_hwfn);
void qed_hw_error_occurred(struct qed_hwfn *p_hwfn,
			   enum qed_hw_err_type err_type);
void qed_get_protocol_stats(struct qed_dev *cdev,
			    enum qed_mcp_protocol_type type,
			    union qed_mcp_protocol_stats *stats);
int qed_slowpath_irq_req(struct qed_hwfn *hwfn);
void qed_slowpath_irq_sync(struct qed_hwfn *p_hwfn);

int qed_mfw_tlv_req(struct qed_hwfn *hwfn);

int qed_mfw_fill_tlv_data(struct qed_hwfn *hwfn,
			  enum qed_mfw_tlv_type type,
			  union qed_mfw_tlv_data *tlv_data);

int qed_hw_attr_update(struct qed_hwfn *hwfn, enum qed_hw_info_change attr);

int qed_set_link(struct qed_dev *cdev, struct qed_link_params *params);

void qed_get_current_link(struct qed_dev *cdev,
			  struct qed_link_output *if_link);

void qed_periodic_db_rec_start(struct qed_hwfn *p_hwfn);

#ifdef _MISSING_CRC8_MODULE	/* ! QED_UPSTREAM */
void qed_crc8_populate_msb(u8 table[CRC8_TABLE_SIZE], u8 polynomial);
u8 qed_crc8(const u8 table[CRC8_TABLE_SIZE], u8 * pdata, size_t nbytes, u8 crc);
#endif

unsigned long qed_get_epoch_time(void);

extern const struct qed_common_ops qed_common_ops_pass;
#define QED_INT_DBG_STORE(P_DEV, ...) \
	qed_common_ops_pass.internal_trace(P_DEV->cdev, __VA_ARGS__)

#endif /* _QED_H_ */
#endif
