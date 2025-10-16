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

#ifndef _QED_IF_H
#define _QED_IF_H
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <net/dcbnl.h>
#include <asm/byteorder.h>
#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/if_ether.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/version.h>
#include "qed_chain.h"
enum dcbx_protocol_type {
	DCBX_PROTOCOL_ISCSI,
	DCBX_PROTOCOL_FCOE,
	DCBX_PROTOCOL_ROCE,
	DCBX_PROTOCOL_ROCE_V2,
	DCBX_PROTOCOL_ETH,
	DCBX_PROTOCOL_IWARP,
	DCBX_MAX_PROTOCOL_TYPE
};

#define QED_LLDP_CHASSIS_ID_STAT_LEN 4
#define QED_LLDP_PORT_ID_STAT_LEN 4
#define QED_DCBX_MAX_APP_PROTOCOL 32
#define QED_MAX_PFC_PRIORITIES 8
#define QED_DCBX_DSCP_SIZE 64

struct qed_dcbx_lldp_remote {
	u32 peer_chassis_id[QED_LLDP_CHASSIS_ID_STAT_LEN];
	u32 peer_port_id[QED_LLDP_PORT_ID_STAT_LEN];
	bool enable_rx;
	bool enable_tx;
	u32 tx_interval;
	u32 max_credit;
};

struct qed_dcbx_lldp_local {
	u32 local_chassis_id[QED_LLDP_CHASSIS_ID_STAT_LEN];
	u32 local_port_id[QED_LLDP_PORT_ID_STAT_LEN];
};

struct qed_dcbx_app_prio {
	u8 roce;
	u8 roce_v2;
	u8 fcoe;
	u8 iscsi;
	u8 eth;
};

struct qed_dbcx_pfc_params {
	bool willing;
	bool enabled;
	bool mbc;
	u8 prio[QED_MAX_PFC_PRIORITIES];
	u8 max_tc;
};

enum qed_dcbx_sf_ieee_type {
	QED_DCBX_SF_IEEE_ETHTYPE,
	QED_DCBX_SF_IEEE_TCP_PORT,
	QED_DCBX_SF_IEEE_UDP_PORT,
	QED_DCBX_SF_IEEE_TCP_UDP_PORT
};

struct qed_app_entry {
	bool ethtype;
	enum qed_dcbx_sf_ieee_type sf_ieee;
	u8 prio;
	u16 proto_id;
	enum dcbx_protocol_type proto_type;
};

struct qed_dcbx_params {
	struct qed_app_entry app_entry[QED_DCBX_MAX_APP_PROTOCOL];
	u16 num_app_entries;
	bool app_willing;
	bool app_valid;
	bool app_error;
	bool ets_willing;
	bool ets_enabled;
	bool ets_cbs;
	u8 ets_pri_tc_tbl[QED_MAX_PFC_PRIORITIES];
	u8 ets_tc_bw_tbl[QED_MAX_PFC_PRIORITIES];
	u8 ets_tc_tsa_tbl[QED_MAX_PFC_PRIORITIES];
	struct qed_dbcx_pfc_params pfc;
	u8 max_ets_tc;
};

struct qed_dcbx_admin_params {
	struct qed_dcbx_params params;
	bool valid;		/* Indicate validity of params */
};

struct qed_dcbx_remote_params {
	struct qed_dcbx_params params;
	bool valid;		/* Indicate validity of params */
};

struct qed_dcbx_operational_params {
	struct qed_dcbx_app_prio app_prio;
	struct qed_dcbx_params params;
	bool valid;		/* Indicate validity of params */
	bool enabled;
	bool ieee;
	bool cee;
	bool local;
	u32 err;
};

struct qed_dcbx_dscp_params {
	bool enabled;
	u8 dscp_pri_map[QED_DCBX_DSCP_SIZE];
};

struct qed_dcbx_get {
	struct qed_dcbx_operational_params operational;
	struct qed_dcbx_lldp_remote lldp_remote;
	struct qed_dcbx_lldp_local lldp_local;
	struct qed_dcbx_remote_params remote;
	struct qed_dcbx_admin_params local;
	struct qed_dcbx_dscp_params dscp;
};
#define QED_MCP_FEC_NONE                (1 << 0)
#define QED_MCP_FEC_FIRECODE            (1 << 1)
#define QED_MCP_FEC_RS          (1 << 2)
#define QED_MCP_FEC_AUTO                (1 << 3)
#define QED_MCP_FEC_UNSUPPORTED (1 << 4)

struct qed_link_eee_params {
	u32 tx_lpi_timer;
#define QED_EEE_1G_ADV  (1 << 0)
#define QED_EEE_10G_ADV (1 << 1)
	/* Capabilities are represented using QED_EEE_*_ADV values */
	u8 adv_caps;
	u8 lp_adv_caps;
	bool enable;
	bool tx_lpi_enable;
};
enum qed_nvm_images {
	QED_NVM_IMAGE_ISCSI_CFG,
	QED_NVM_IMAGE_FCOE_CFG,
	QED_NVM_IMAGE_MDUMP,
	QED_NVM_IMAGE_NVM_CFG1,
	QED_NVM_IMAGE_DEFAULT_CFG,
	QED_NVM_IMAGE_NVM_META,
};
#define QED_MAX_NPIV_ENTRIES 128
#define QED_WWN_SIZE 8
struct qed_fc_npiv_tbl {
	u16 num_wwpn;
	u16 num_wwnn;
	u8 wwpn[QED_MAX_NPIV_ENTRIES][QED_WWN_SIZE];
	u8 wwnn[QED_MAX_NPIV_ENTRIES][QED_WWN_SIZE];
};

enum qed_led_mode {
	QED_LED_MODE_OFF,
	QED_LED_MODE_ON,
	QED_LED_MODE_RESTORE
};
struct qed_mfw_tlv_eth {
	u16 lso_maxoff_size;
	bool lso_maxoff_size_set;
	u16 lso_minseg_size;
	bool lso_minseg_size_set;
	u8 prom_mode;
	bool prom_mode_set;
	u16 tx_descr_size;
	bool tx_descr_size_set;
	u16 rx_descr_size;
	bool rx_descr_size_set;
	u16 netq_count;
	bool netq_count_set;
	u32 tcp4_offloads;
	bool tcp4_offloads_set;
	u32 tcp6_offloads;
	bool tcp6_offloads_set;
	u16 tx_descr_qdepth;
	bool tx_descr_qdepth_set;
	u16 rx_descr_qdepth;
	bool rx_descr_qdepth_set;
	u8 iov_offload;
#define QED_MFW_TLV_IOV_OFFLOAD_NONE            (0)
#define QED_MFW_TLV_IOV_OFFLOAD_MULTIQUEUE      (1)
#define QED_MFW_TLV_IOV_OFFLOAD_VEB             (2)
#define QED_MFW_TLV_IOV_OFFLOAD_VEPA            (3)
	bool iov_offload_set;
	u8 txqs_empty;
	bool txqs_empty_set;
	u8 rxqs_empty;
	bool rxqs_empty_set;
	u8 num_txqs_full;
	bool num_txqs_full_set;
	u8 num_rxqs_full;
	bool num_rxqs_full_set;
};

struct qed_mfw_tlv_time {
	bool b_set;
	u8 month;
	u8 day;
	u8 hour;
	u8 min;
	u16 msec;
	u16 usec;
};

struct qed_mfw_tlv_fcoe {
	u8 scsi_timeout;
	bool scsi_timeout_set;
	u32 rt_tov;
	bool rt_tov_set;
	u32 ra_tov;
	bool ra_tov_set;
	u32 ed_tov;
	bool ed_tov_set;
	u32 cr_tov;
	bool cr_tov_set;
	u8 boot_type;
	bool boot_type_set;
	u8 npiv_state;
	bool npiv_state_set;
	u32 num_npiv_ids;
	bool num_npiv_ids_set;
	u8 switch_name[8];
	bool switch_name_set;
	u16 switch_portnum;
	bool switch_portnum_set;
	u8 switch_portid[3];
	bool switch_portid_set;
	u8 vendor_name[8];
	bool vendor_name_set;
	u8 switch_model[8];
	bool switch_model_set;
	u8 switch_fw_version[8];
	bool switch_fw_version_set;
	u8 qos_pri;
	bool qos_pri_set;
	u8 port_alias[3];
	bool port_alias_set;
	u8 port_state;
#define QED_MFW_TLV_PORT_STATE_OFFLINE  (0)
#define QED_MFW_TLV_PORT_STATE_LOOP             (1)
#define QED_MFW_TLV_PORT_STATE_P2P              (2)
#define QED_MFW_TLV_PORT_STATE_FABRIC           (3)
	bool port_state_set;
	u16 fip_tx_descr_size;
	bool fip_tx_descr_size_set;
	u16 fip_rx_descr_size;
	bool fip_rx_descr_size_set;
	u16 link_failures;
	bool link_failures_set;
	u8 fcoe_boot_progress;
	bool fcoe_boot_progress_set;
	u64 rx_bcast;
	bool rx_bcast_set;
	u64 tx_bcast;
	bool tx_bcast_set;
	u16 fcoe_txq_depth;
	bool fcoe_txq_depth_set;
	u16 fcoe_rxq_depth;
	bool fcoe_rxq_depth_set;
	u64 fcoe_rx_frames;
	bool fcoe_rx_frames_set;
	u64 fcoe_rx_bytes;
	bool fcoe_rx_bytes_set;
	u64 fcoe_tx_frames;
	bool fcoe_tx_frames_set;
	u64 fcoe_tx_bytes;
	bool fcoe_tx_bytes_set;
	u16 crc_count;
	bool crc_count_set;
	u32 crc_err_src_fcid[5];
	bool crc_err_src_fcid_set[5];
	struct qed_mfw_tlv_time crc_err[5];
	u16 losync_err;
	bool losync_err_set;
	u16 losig_err;
	bool losig_err_set;
	u16 primtive_err;
	bool primtive_err_set;
	u16 disparity_err;
	bool disparity_err_set;
	u16 code_violation_err;
	bool code_violation_err_set;
	u32 flogi_param[4];
	bool flogi_param_set[4];
	struct qed_mfw_tlv_time flogi_tstamp;
	u32 flogi_acc_param[4];
	bool flogi_acc_param_set[4];
	struct qed_mfw_tlv_time flogi_acc_tstamp;
	u32 flogi_rjt;
	bool flogi_rjt_set;
	struct qed_mfw_tlv_time flogi_rjt_tstamp;
	u32 fdiscs;
	bool fdiscs_set;
	u8 fdisc_acc;
	bool fdisc_acc_set;
	u8 fdisc_rjt;
	bool fdisc_rjt_set;
	u8 plogi;
	bool plogi_set;
	u8 plogi_acc;
	bool plogi_acc_set;
	u8 plogi_rjt;
	bool plogi_rjt_set;
	u32 plogi_dst_fcid[5];
	bool plogi_dst_fcid_set[5];
	struct qed_mfw_tlv_time plogi_tstamp[5];
	u32 plogi_acc_src_fcid[5];
	bool plogi_acc_src_fcid_set[5];
	struct qed_mfw_tlv_time plogi_acc_tstamp[5];
	u8 tx_plogos;
	bool tx_plogos_set;
	u8 plogo_acc;
	bool plogo_acc_set;
	u8 plogo_rjt;
	bool plogo_rjt_set;
	u32 plogo_src_fcid[5];
	bool plogo_src_fcid_set[5];
	struct qed_mfw_tlv_time plogo_tstamp[5];
	u8 rx_logos;
	bool rx_logos_set;
	u8 tx_accs;
	bool tx_accs_set;
	u8 tx_prlis;
	bool tx_prlis_set;
	u8 rx_accs;
	bool rx_accs_set;
	u8 tx_abts;
	bool tx_abts_set;
	u8 rx_abts_acc;
	bool rx_abts_acc_set;
	u8 rx_abts_rjt;
	bool rx_abts_rjt_set;
	u32 abts_dst_fcid[5];
	bool abts_dst_fcid_set[5];
	struct qed_mfw_tlv_time abts_tstamp[5];
	u8 rx_rscn;
	bool rx_rscn_set;
	u32 rx_rscn_nport[4];
	bool rx_rscn_nport_set[4];
	u8 tx_lun_rst;
	bool tx_lun_rst_set;
	u8 abort_task_sets;
	bool abort_task_sets_set;
	u8 tx_tprlos;
	bool tx_tprlos_set;
	u8 tx_nos;
	bool tx_nos_set;
	u8 rx_nos;
	bool rx_nos_set;
	u8 ols;
	bool ols_set;
	u8 lr;
	bool lr_set;
	u8 lrr;
	bool lrr_set;
	u8 tx_lip;
	bool tx_lip_set;
	u8 rx_lip;
	bool rx_lip_set;
	u8 eofa;
	bool eofa_set;
	u8 eofni;
	bool eofni_set;
	u8 scsi_chks;
	bool scsi_chks_set;
	u8 scsi_cond_met;
	bool scsi_cond_met_set;
	u8 scsi_busy;
	bool scsi_busy_set;
	u8 scsi_inter;
	bool scsi_inter_set;
	u8 scsi_inter_cond_met;
	bool scsi_inter_cond_met_set;
	u8 scsi_rsv_conflicts;
	bool scsi_rsv_conflicts_set;
	u8 scsi_tsk_full;
	bool scsi_tsk_full_set;
	u8 scsi_aca_active;
	bool scsi_aca_active_set;
	u8 scsi_tsk_abort;
	bool scsi_tsk_abort_set;
	u32 scsi_rx_chk[5];
	bool scsi_rx_chk_set[5];
	struct qed_mfw_tlv_time scsi_chk_tstamp[5];
};

struct qed_mfw_tlv_iscsi {
	u8 target_llmnr;
	bool target_llmnr_set;
	u8 header_digest;
	bool header_digest_set;
	u8 data_digest;
	bool data_digest_set;
	u8 auth_method;
#define QED_MFW_TLV_AUTH_METHOD_NONE            (1)
#define QED_MFW_TLV_AUTH_METHOD_CHAP            (2)
#define QED_MFW_TLV_AUTH_METHOD_MUTUAL_CHAP     (3)
	bool auth_method_set;
	u16 boot_taget_portal;
	bool boot_taget_portal_set;
	u16 frame_size;
	bool frame_size_set;
	u16 tx_desc_size;
	bool tx_desc_size_set;
	u16 rx_desc_size;
	bool rx_desc_size_set;
	u8 boot_progress;
	bool boot_progress_set;
	u16 tx_desc_qdepth;
	bool tx_desc_qdepth_set;
	u16 rx_desc_qdepth;
	bool rx_desc_qdepth_set;
	u64 rx_frames;
	bool rx_frames_set;
	u64 rx_bytes;
	bool rx_bytes_set;
	u64 tx_frames;
	bool tx_frames_set;
	u64 tx_bytes;
	bool tx_bytes_set;
};
enum qed_hw_info_change {
	QED_HW_INFO_CHANGE_OVLAN,
};
#define MASK_FIELD(_name, _value) \
	((_value) &= (_name ## _MASK))

#define FIELD_VALUE(_name, _value) \
	((_value & _name ## _MASK) << _name ## _SHIFT)

#define SET_FIELD(value, name, flag)						  \
	do {									  \
		(value) &= ~(name ## _MASK << name ## _SHIFT);			  \
		(value) |=							  \
			((((u64)flag) & (u64)name ## _MASK) << (name ## _SHIFT)); \
	} while (0)

#define GET_FIELD(value, name) \
	(((value) >> (name ## _SHIFT)) & name ## _MASK)

#define GET_MFW_FIELD(name, field) \
	(((name) & (field ## _MASK)) >> (field ## _OFFSET))

#define SET_MFW_FIELD(name, field, value)					 \
	do {									 \
		(name)	&= ~(field ## _MASK);					 \
		(name)	|= (((value) << (field ## _OFFSET)) & (field ## _MASK)); \
	} while (0)

#define DB_ADDR_SHIFT(addr)             ((addr) << DB_PWM_ADDR_OFFSET_SHIFT)
#define QED_INT_DEBUG_SIZE_DEF     _MB(2)
struct qed_internal_trace {
	char *buf;
	u32 size;
	u64 prod;
	spinlock_t lock;
};

struct qed_timestamp {
	u64 sec;
	unsigned long rem_usec;
};

#define QED_DP_INT_LOG_MAX_STR_SIZE 256
#define QED_DP_INT_LOG_DEFAULT_MASK (0xffff43ff)

/* Debug print definitions */
#define DP_INT_LOG(P_DEV, LEVEL, MODULE, fmt, ...)			    \
	do {								    \
		if (unlikely((P_DEV)->dp_int_level > LEVEL))		    \
			break;						    \
		if (unlikely((P_DEV)->dp_int_level == QED_LEVEL_VERBOSE) && \
		    (LEVEL == QED_LEVEL_VERBOSE) &&			    \
		    ((P_DEV)->dp_int_module & MODULE) == 0)		    \
			break;						    \
									    \
		QED_INT_DBG_STORE(P_DEV, fmt,				    \
				  __func__, __LINE__,			    \
				  DP_NAME(P_DEV) ? DP_NAME(P_DEV) : "",	    \
				  ## __VA_ARGS__);			    \
	} while (0)

#define DP_ERR(P_DEV, fmt, ...)					    \
	do {							    \
		DP_INT_LOG((P_DEV), QED_LEVEL_ERR, 0,		    \
			   "ERR: [%s:%d(%s)]" fmt, ## __VA_ARGS__); \
		pr_err("[%s:%d(%s)]" fmt,			    \
		       __func__, __LINE__,			    \
		       DP_NAME(P_DEV) ? DP_NAME(P_DEV) : "",	    \
		       ## __VA_ARGS__);				    \
	} while (0)

#define DP_NOTICE(P_DEV, fmt, ...)					\
	do {								\
		DP_INT_LOG((P_DEV), QED_LEVEL_NOTICE, 0,		\
			   "NOTICE: [%s:%d(%s)]" fmt, ## __VA_ARGS__);	\
		if (unlikely((P_DEV)->dp_level <= QED_LEVEL_NOTICE)) {	\
			pr_notice("[%s:%d(%s)]" fmt,			\
				  __func__, __LINE__,			\
				  DP_NAME(P_DEV) ? DP_NAME(P_DEV) : "",	\
				  ## __VA_ARGS__);			\
									\
		}							\
	} while (0)

#define DP_INFO(P_DEV, fmt, ...)					\
	do {								\
		DP_INT_LOG((P_DEV), QED_LEVEL_INFO, 0,			\
			   "INFO: [%s:%d(%s)]" fmt, ## __VA_ARGS__);	\
		if (unlikely((P_DEV)->dp_level <= QED_LEVEL_INFO)) {	\
			pr_notice("[%s:%d(%s)]" fmt,			\
				  __func__, __LINE__,			\
				  DP_NAME(P_DEV) ? DP_NAME(P_DEV) : "",	\
				  ## __VA_ARGS__);			\
		}							\
	} while (0)

#define DP_VERBOSE(P_DEV, module, fmt, ...)				 \
	do {								 \
		DP_INT_LOG((P_DEV), QED_LEVEL_VERBOSE, module,		 \
			   "VERBOSE: [%s:%d(%s)]" fmt, ## __VA_ARGS__);	 \
		if (unlikely(((P_DEV)->dp_level <= QED_LEVEL_VERBOSE) && \
			     ((P_DEV)->dp_module & module))) {		 \
			pr_notice("[%s:%d(%s)]" fmt,			 \
				  __func__, __LINE__,			 \
				  DP_NAME(P_DEV) ? DP_NAME(P_DEV) : "",	 \
				  ## __VA_ARGS__);			 \
		}							 \
	} while (0)

enum DP_LEVEL {
	QED_LEVEL_VERBOSE = 0x0,
	QED_LEVEL_INFO = 0x1,
	QED_LEVEL_NOTICE = 0x2,
	QED_LEVEL_ERR = 0x3,
};

#define QED_LOG_LEVEL_SHIFT     (30)
#define QED_LOG_VERBOSE_MASK    (0x3fffffff)
#define QED_LOG_INFO_MASK       (0x40000000)
#define QED_LOG_NOTICE_MASK     (0x80000000)

enum DP_MODULE {
	QED_MSG_EXTRA = 0x8000,
	QED_MSG_SPQ = 0x10000,
	QED_MSG_STATS = 0x20000,
	QED_MSG_DCB = 0x40000,
	QED_MSG_IOV = 0x80000,
	QED_MSG_SP = 0x100000,
	QED_MSG_STORAGE = 0x200000,
	QED_MSG_OOO = 0x200000,
	QED_MSG_CXT = 0x800000,
	QED_MSG_LL2 = 0x1000000,
	QED_MSG_ILT = 0x2000000,
	QED_MSG_RDMA = 0x4000000,
	QED_MSG_DEBUG = 0x8000000,
	/* to be added...up to 0x8000000 */
	QED_MSG_QM = 0x10000000,
};

/**
 * @brief Convert from 32b debug param to two params of level and module
 *
 * @param debug
 * @param p_dp_module
 * @param p_dp_level
 * @return void
 *
 * @note Input 32b decoding:
 *	 b31 - enable all NOTICE prints. NOTICE prints are for deviation from
 *	 the 'happy' flow, e.g. memory allocation failed.
 *	 b30 - enable all INFO prints. INFO prints are for major steps in the
 *	 flow and provide important parameters.
 *	 b29-b0 - per-module bitmap, where each bit enables VERBOSE prints of
 *	 that module. VERBOSE prints are for tracking the specific flow in low
 *	 level.
 *
 *	 Notice that the level should be that of the lowest required logs.
 */
static inline void qed_config_debug(u32 debug,
				    u32 * p_dp_module, u8 * p_dp_level)
{
	*p_dp_level = QED_LEVEL_NOTICE;
	*p_dp_module = 0;

	if (debug & QED_LOG_VERBOSE_MASK) {
		*p_dp_level = QED_LEVEL_VERBOSE;
		*p_dp_module = (debug & 0x3FFFFFFF);
	} else if (debug & QED_LOG_INFO_MASK) {
		*p_dp_level = QED_LEVEL_INFO;
	} else if (debug & QED_LOG_NOTICE_MASK) {
		*p_dp_level = QED_LEVEL_NOTICE;
	}
}

enum qed_hw_err_type {
	QED_HW_ERR_FAN_FAIL,
	QED_HW_ERR_MFW_RESP_FAIL,
	QED_HW_ERR_HW_ATTN,
	QED_HW_ERR_DMAE_FAIL,
	QED_HW_ERR_RAMROD_FAIL,
	QED_HW_ERR_FW_ASSERT,
	QED_HW_ERR_PARITY,
};
enum qed_lag_type {
	QED_LAG_TYPE_NONE,
	QED_LAG_TYPE_ACTIVEACTIVE,
	QED_LAG_TYPE_ACTIVEBACKUP
};
enum qed_dev_type {
	QED_DEV_TYPE_BB,
	QED_DEV_TYPE_AH,
};
struct qed_eth_stats_common {
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
	u64 tpa_coalesced_pkts;
	u64 tpa_coalesced_events;
	u64 tpa_aborts_num;
	u64 tpa_not_coalesced_pkts;
	u64 tpa_coalesced_bytes;

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
	u64 rx_mac_bytes;
	u64 rx_mac_uc_packets;
	u64 rx_mac_mc_packets;
	u64 rx_mac_bc_packets;
	u64 rx_mac_frames_ok;
	u64 tx_mac_bytes;
	u64 tx_mac_uc_packets;
	u64 tx_mac_mc_packets;
	u64 tx_mac_bc_packets;
	u64 tx_mac_ctrl_frames;
	u64 link_change_count;
	u64 pfm_state_changes;
	u64 nig_drain_cnt;
};

struct qed_eth_stats_bb {
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

struct qed_eth_stats_ah {
	u64 rx_1519_to_max_byte_packets;
	u64 tx_1519_to_max_byte_packets;
};

// TODO Temporary fix to address linux build issues

struct qed_eth_stats {
	struct qed_eth_stats_common common;
	union {
		struct qed_eth_stats_bb bb;
		struct qed_eth_stats_ah ah;
	};
};
enum qed_db_rec_width {
	DB_REC_WIDTH_32B,
	DB_REC_WIDTH_64B,
};

enum qed_db_rec_space {
	DB_REC_KERNEL,
	DB_REC_USER,
};

#ifdef _HAS_MMIOWB_SPIN_LOCK
#define mmiowb() do { } while (0)
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)) || \
	defined(RHEL_RELEASE_CODE)	/* QED_UPSTREAM */
#define DCB_CEE_SUPPORT 1
#endif

#define DIRECT_REG_WR(reg_addr, val) ({ writel((u32)val,		    \
					       (void __iomem *)(reg_addr)); \
					wmb(); })

#define DIRECT_REG_RD(reg_addr) ({ rmb(); readl((void __iomem *)(reg_addr)); })

#define DIRECT_REG_WR64(reg_addr, val) ({ writeq((u64)val,		      \
						 (void __iomem *)(reg_addr)); \
					  wmb(); })

#define DIRECT_REG_RD64(reg_addr) ({ rmb(); readq((void __iomem *)(reg_addr)); })

#define QED_COALESCE_MAX 0x1FF
#define QED_DEFAULT_RX_USECS 12
#define QED_DEFAULT_TX_USECS 48

/* forward */
struct qed_dev;
struct qed_sb_info;
struct qed_pf_params;
struct qed_tunn_params;
struct qed_sb_info_dbg;
enum qed_int_mode;
enum qed_hw_info_change;

struct qed_dev_info {
	unsigned long pci_mem_start;
	unsigned long pci_mem_end;
	unsigned int pci_irq;
	u8 num_hwfns;

	u8 hw_mac[ETH_ALEN];

	/* FW version */
	u16 fw_major;
	u16 fw_minor;
	u16 fw_rev;
	u16 fw_eng;

	/* MFW version */
	u32 mfw_rev;
#define QED_MFW_VERSION_0_MASK          0x000000FF
#define QED_MFW_VERSION_0_OFFSET        0
#define QED_MFW_VERSION_1_MASK          0x0000FF00
#define QED_MFW_VERSION_1_OFFSET        8
#define QED_MFW_VERSION_2_MASK          0x00FF0000
#define QED_MFW_VERSION_2_OFFSET        16
#define QED_MFW_VERSION_3_MASK          0xFF000000
#define QED_MFW_VERSION_3_OFFSET        24

	bool rdma_supported;

	u32 flash_size;
	bool b_arfs_capable;
	bool b_inter_pf_switch;
	bool tx_switching;
	u16 mtu;

	bool wol_support;
	bool smart_an;
	bool esl;

	/* MBI version */
	u32 mbi_version;
#define QED_MBI_VERSION_0_MASK          0x000000FF
#define QED_MBI_VERSION_0_OFFSET        0
#define QED_MBI_VERSION_1_MASK          0x0000FF00
#define QED_MBI_VERSION_1_OFFSET        8
#define QED_MBI_VERSION_2_MASK          0x00FF0000
#define QED_MBI_VERSION_2_OFFSET        16

	/* Out param for qede */
	bool vxlan_enable;
	bool gre_enable;
	bool geneve_enable;

	u8 abs_pf_id;

	enum qed_dev_type dev_type;
};

enum qed_sb_type {
	QED_SB_TYPE_L2_QUEUE,
	QED_SB_TYPE_STORAGE,
	QED_SB_TYPE_CNQ,
};

enum qed_protocol {
	QED_PROTOCOL_ETH,
	QED_PROTOCOL_ISCSI,
	QED_PROTOCOL_FCOE,
};

enum qed_link_mode_bits {
	QED_LM_FIBRE_BIT = BIT(0),
	QED_LM_Autoneg_BIT = BIT(1),
	QED_LM_Asym_Pause_BIT = BIT(2),
	QED_LM_Pause_BIT = BIT(3),
	QED_LM_1000baseT_Full_BIT = BIT(4),
	QED_LM_10000baseT_Full_BIT = BIT(5),
	QED_LM_10000baseKR_Full_BIT = BIT(6),
	QED_LM_20000baseKR2_Full_BIT = BIT(7),
	QED_LM_25000baseKR_Full_BIT = BIT(8),
	QED_LM_40000baseLR4_Full_BIT = BIT(9),
	QED_LM_50000baseKR2_Full_BIT = BIT(10),
	QED_LM_100000baseKR4_Full_BIT = BIT(11),
	QED_LM_TP_BIT = BIT(12),
	QED_LM_Backplane_BIT = BIT(13),
	QED_LM_1000baseKX_Full_BIT = BIT(14),
	QED_LM_10000baseKX4_Full_BIT = BIT(15),
	QED_LM_10000baseR_FEC_BIT = BIT(16),
	QED_LM_40000baseKR4_Full_BIT = BIT(17),
	QED_LM_40000baseCR4_Full_BIT = BIT(18),
	QED_LM_40000baseSR4_Full_BIT = BIT(19),
	QED_LM_25000baseCR_Full_BIT = BIT(20),
	QED_LM_25000baseSR_Full_BIT = BIT(21),
	QED_LM_50000baseCR2_Full_BIT = BIT(22),
	QED_LM_100000baseSR4_Full_BIT = BIT(23),
	QED_LM_100000baseCR4_Full_BIT = BIT(24),
	QED_LM_100000baseLR4_ER4_Full_BIT = BIT(25),
	QED_LM_50000baseSR2_Full_BIT = BIT(26),
	QED_LM_1000baseX_Full_BIT = BIT(27),
	QED_LM_10000baseCR_Full_BIT = BIT(28),
	QED_LM_10000baseSR_Full_BIT = BIT(29),
	QED_LM_10000baseLR_Full_BIT = BIT(30),
	QED_LM_10000baseLRM_Full_BIT = BIT(31),
	QED_LM_COUNT = 32
};

enum qed_fec_mode {
	QED_FEC_MODE_NONE = BIT(0),
	QED_FEC_MODE_FIRECODE = BIT(1),
	QED_FEC_MODE_RS = BIT(2),
	QED_FEC_MODE_AUTO = BIT(3),
	QED_FEC_MODE_UNSUPPORTED = BIT(4)
};

struct qed_link_params {
	bool link_up;

#define QED_LINK_OVERRIDE_SPEED_AUTONEG         (1 << 0)
#define QED_LINK_OVERRIDE_SPEED_ADV_SPEEDS      (1 << 1)
#define QED_LINK_OVERRIDE_SPEED_FORCED_SPEED    (1 << 2)
#define QED_LINK_OVERRIDE_PAUSE_CONFIG          (1 << 3)
#define QED_LINK_OVERRIDE_LOOPBACK_MODE         (1 << 4)
#define QED_LINK_OVERRIDE_EEE_CONFIG            (1 << 5)
#define QED_LINK_OVERRIDE_FEC_CONFIG            (1 << 6)
	u32 override_flags;
	bool autoneg;
	u32 adv_speeds;
	u32 forced_speed;
#define QED_LINK_PAUSE_AUTONEG_ENABLE           (1 << 0)
#define QED_LINK_PAUSE_RX_ENABLE                (1 << 1)
#define QED_LINK_PAUSE_TX_ENABLE                (1 << 2)
	u32 pause_config;
#define QED_LINK_LOOPBACK_NONE                  (1 << 0)
#define QED_LINK_LOOPBACK_INT_PHY               (1 << 1)
#define QED_LINK_LOOPBACK_EXT_PHY               (1 << 2)
#define QED_LINK_LOOPBACK_EXT                   (1 << 3)
#define QED_LINK_LOOPBACK_MAC                   (1 << 4)
#define QED_LINK_LOOPBACK_CNIG_AH_ONLY_0123     (1 << 5)
#define QED_LINK_LOOPBACK_CNIG_AH_ONLY_2301     (1 << 6)
#define QED_LINK_LOOPBACK_PCS_AH_ONLY           (1 << 7)
#define QED_LINK_LOOPBACK_REVERSE_MAC_AH_ONLY   (1 << 8)
#define QED_LINK_LOOPBACK_INT_PHY_FEA_AH_ONLY   (1 << 9)
	u32 loopback_mode;
	struct qed_link_eee_params eee;
	u32 fec;
};

struct qed_link_output {
	bool link_up;

	/* In QED_LM_* values set */
	u32 supported_caps;
	u32 advertised_caps;
	u32 lp_caps;

	u32 speed;		/* In Mb/s */
	u8 duplex;		/* In DUPLEX defs */
	u8 port;		/* In PORT defs */
	bool autoneg;
	u32 pause_config;

	/* EEE - capability & param */
	bool eee_supported;
	bool eee_active;
	u8 sup_caps;
	struct qed_link_eee_params eee;
	u32 sup_fec;
	u32 active_fec;
};

struct qed_probe_params {
	enum qed_protocol protocol;
	u32 dp_module;
	u8 dp_level;
	u8 mcp_resc_lock_retry_cnt;
	bool is_vf;
	bool recov_in_prog;
	struct qed_dev *cdev;	/* cdev retained during the recovery process */
};

#define QED_DRV_VER_STR_SIZE 12
struct qed_slowpath_params {
	u32 int_mode;
	u8 drv_major;
	u8 drv_minor;
	u8 drv_rev;
	u8 drv_eng;
	u8 name[QED_DRV_VER_STR_SIZE];
};

#define ILT_PAGE_SIZE_TCFC 0x8000	/* 32KB */

#if 0
/* It's possible we'll one day want at least some of this functions inside
 * the ops interface between qed and protocol driver, but currently
 * they're either not implemented or not in use
 */

/*
 * Initilaizes intenal link state structure
 */
void qed_prepare_link_state(struct qed_dev *cdev);

/*
 * Setup coalessing for queue (q_id)
 */
//int qed_setup_queue_coales(struct qed_dev *cdev, int eid, int q_id, int usec);

/*
 * Request to collect statistics for queue (q_id) to addr
 */
int qed_add_queue_stats(struct qed_dev *cdev,
			int eid, int q_id, dma_addr_t addr);
int qed_del_queue_stats(struct qed_dev *cdev, int eid, int q_id);

/*
 * Request to collect statistics for port
 */
int qed_add_port_stats(struct qed_dev *cdev,
		       int eid, int port_id, dma_addr_t addr);
int qed_del_port_stats(struct qed_dev *cdev, int eid, int port_id);

/*
 * Update MAC address
 */
int qed_set_mac(struct qed_dev *cdev, int eid, int vport_id, u8 * mac);

/*
 * Set RX mode
 */
int qed_set_rx_mode(struct qed_dev *cdev, int mode);

/*
 * Handles SP events
 */
struct sp_event;
int qed_sp_event(struct qed_dev *cdev, int eid, int q_id, struct sp_event *e);

int qed_start_stats(struct qed_dev *cdev);
void qed_stop_stats(struct qed_dev *cdev);

struct qed_mac_list_elem {
	struct list_head link;
	u8 *mac;
};

int qed_set_mc_list(struct qed_dev *cdev, struct qed_mac_list_elem *head);

int qed_set_uc_list(struct qed_dev *cdev, struct qed_mac_list_elem *head);

enum {
	QED_RX_MODE_NONE,
	QED_RX_MODE_NORMAL,
	QED_RX_MODE_ALLMULTI,
	QED_RX_MODE_PROMISC
};

int qed_nvram_read(struct qed_dev *cdev, u32 offset, u8 * buff, int size);
int qed_nvram_write(struct qed_dev *cdev, u32 offset, u8 * buff, int size);

int qed_set_coalesce(struct qed_dev *cdev,
		     u32 rx_usec, u32 tx_usec, void *handle);
int qed_get_coalesce(struct qed_dev *cdev, u32 * rx_usec, u32 * tx_usec);
#endif

struct qed_int_info {
	struct msix_entry *msix;
	u16 msix_cnt;
	u16 used_cnt;		/* This should be updated by the protocol driver */
};

struct qed_generic_tlvs {
#define QED_TLV_IP_CSUM         BIT(0)
#define QED_TLV_LSO             BIT(1)
	u16 feat_flags;
	u8 mac[3][ETH_ALEN];
};

#define QED_I2C_DEV_ADDR_A0 0xA0
#define QED_I2C_DEV_ADDR_A2 0xA2

struct qed_sfp_lane_info {
	u16 tx_bias;
	u16 tx_power;
	u16 rx_power;
};

struct qed_sfp_stats {
	u16 sfp_type;
	u16 temperature;
	u16 vcc;
	struct qed_sfp_lane_info lane[4];
};

/* Devlink Param IDs range for QED Module.
 * Storage Drivers should use Devlink PARAM IDs beyond this number.
 */
#define QED_DEVLINK_PARAM_ID_START      DEVLINK_PARAM_GENERIC_ID_MAX
#define QED_DEVLINK_PARAM_ID_END        (DEVLINK_PARAM_GENERIC_ID_MAX + 20)

struct qed_devlink {
	struct qed_dev *cdev;
	void *drv_ctx;
	struct devlink_health_reporter *fw_reporter;
};

struct qed_common_cb_ops {
	void (*arfs_filter_op) (void *dev, void *fltr, u8 fw_rc);
	void (*link_update) (void *dev, struct qed_link_output * link);
	void (*schedule_recovery_handler) (void *dev);
	void (*schedule_hw_err_handler) (void *dev,
					 enum qed_hw_err_type err_type);
	void (*dcbx_aen) (void *dev, struct qed_dcbx_get * get, u32 mib_type);
	void (*get_generic_tlv_data) (void *dev,
				      struct qed_generic_tlvs * data);
	void (*get_protocol_tlv_data) (void *dev, void *data);
	void (*hw_attr_update) (void *dev, enum qed_hw_info_change attr);
	void (*bw_update) (void *dev);
};

struct qed_selftest_ops {
/**
 * @brief selftest_interrupt - Perform interrupt test
 *
 * @param cdev
 *
 * @return 0 on success, error otherwise.
 */
	int (*selftest_interrupt) (struct qed_dev * cdev);

/**
 * @brief selftest_memory - Perform memory test
 *
 * @param cdev
 *
 * @return 0 on success, error otherwise.
 */
	int (*selftest_memory) (struct qed_dev * cdev);

/**
 * @brief selftest_register - Perform register test
 *
 * @param cdev
 *
 * @return 0 on success, error otherwise.
 */
	int (*selftest_register) (struct qed_dev * cdev);

/**
 * @brief selftest_clock - Perform clock test
 *
 * @param cdev
 *
 * @return 0 on success, error otherwise.
 */
	int (*selftest_clock) (struct qed_dev * cdev);

/**
 * @brief selftest_nvram - Perform nvram test
 *
 * @param cdev
 *
 * @return 0 on success, error otherwise.
 */
	int (*selftest_nvram) (struct qed_dev * cdev);
};

/* Prototype declaration of qed_dcbnl_ops should match with the declaration
 * of dcbnl_rtnl_ops structure.
 */
struct qed_dcbnl_ops {
	/* IEEE 802.1Qaz std */
	int (*ieee_getpfc) (struct qed_dev * cdev, struct ieee_pfc * pfc);
	int (*ieee_setpfc) (struct qed_dev * cdev, struct ieee_pfc * pfc);
	int (*ieee_getets) (struct qed_dev * cdev, struct ieee_ets * ets);
	int (*ieee_setets) (struct qed_dev * cdev, struct ieee_ets * ets);
	int (*ieee_peer_getets) (struct qed_dev * cdev, struct ieee_ets * ets);
	int (*ieee_peer_getpfc) (struct qed_dev * cdev, struct ieee_pfc * pfc);
	int (*ieee_getapp) (struct qed_dev * cdev, struct dcb_app * app);
	int (*ieee_setapp) (struct qed_dev * cdev, struct dcb_app * app);

	/* CEE std */
	 u8(*getstate) (struct qed_dev * cdev);
	 u8(*setstate) (struct qed_dev * cdev, u8 state);
	void (*getpgtccfgtx) (struct qed_dev * cdev,
			      int prio,
			      u8 * prio_type,
			      u8 * pgid, u8 * bw_pct, u8 * up_map);
	void (*getpgbwgcfgtx) (struct qed_dev * cdev, int pgid, u8 * bw_pct);
	void (*getpgtccfgrx) (struct qed_dev * cdev,
			      int prio,
			      u8 * prio_type,
			      u8 * pgid, u8 * bw_pct, u8 * up_map);
	void (*getpgbwgcfgrx) (struct qed_dev * cdev, int pgid, u8 * bw_pct);
	void (*getpfccfg) (struct qed_dev * cdev, int prio, u8 * setting);
	void (*setpfccfg) (struct qed_dev * cdev, int prio, u8 setting);
	 u8(*getcap) (struct qed_dev * cdev, int capid, u8 * cap);
	int (*getnumtcs) (struct qed_dev * cdev, int tcid, u8 * num);
	 u8(*getpfcstate) (struct qed_dev * cdev);
	int (*getapp) (struct qed_dev * cdev, u8 idtype, u16 id);
	 u8(*getfeatcfg) (struct qed_dev * cdev, int featid, u8 * flags);
	/* DCBX configuration */
	 u8(*getdcbx) (struct qed_dev * cdev);
	void (*setpgtccfgtx) (struct qed_dev * cdev,
			      int prio,
			      u8 pri_type, u8 pgid, u8 bw_pct, u8 up_map);
	void (*setpgtccfgrx) (struct qed_dev * cdev,
			      int prio,
			      u8 pri_type, u8 pgid, u8 bw_pct, u8 up_map);
	void (*setpgbwgcfgtx) (struct qed_dev * cdev, int pgid, u8 bw_pct);
	void (*setpgbwgcfgrx) (struct qed_dev * cdev, int pgid, u8 bw_pct);
	 u8(*setall) (struct qed_dev * cdev);
	int (*setnumtcs) (struct qed_dev * cdev, int tcid, u8 num);
	void (*setpfcstate) (struct qed_dev * cdev, u8 state);
	int (*setapp) (struct qed_dev * cdev, u8 idtype, u16 idval, u8 up);
	 u8(*setdcbx) (struct qed_dev * cdev, u8 state);
	 u8(*setfeatcfg) (struct qed_dev * cdev, int featid, u8 flags);
#ifdef DCB_CEE_SUPPORT		/* QED_UPSTREAM */
	/* Peer apps */
	int (*peer_getappinfo) (struct qed_dev * cdev,
				struct dcb_peer_app_info * info,
				u16 * app_count);
	int (*peer_getapptable) (struct qed_dev * cdev, struct dcb_app * table);
	/* CEE peer */
	int (*cee_peer_getpfc) (struct qed_dev * cdev, struct cee_pfc * pfc);
	int (*cee_peer_getpg) (struct qed_dev * cdev, struct cee_pg * pg);
#endif
};

struct qed_common_ops {
	struct qed_selftest_ops *selftest;

	const struct qed_dcbnl_ops *dcb;

	struct qed_dev *(*probe) (struct pci_dev * pdev,
				  struct qed_probe_params * params);

	void (*remove) (struct qed_dev * cdev);

	int (*set_power_state) (struct qed_dev * cdev, pci_power_t state);

	void (*set_name) (struct qed_dev * cdev, char name[]);

	void (*get_dev_name) (struct qed_dev * cdev, u8 * name, u8 max_chars);

	/* Client drivers need to make this call before slowpath_start.
	 * PF params required for the call before slowpath_start is
	 * documented within the qed_pf_params structure definition.
	 * Storage drivers need to make this call a second time after
	 * slowpath start.
	 */
	void (*update_pf_params) (struct qed_dev * cdev,
				  struct qed_pf_params * params);
	int (*update_tunn_config) (struct qed_dev * cdev,
				   struct qed_tunn_params * params);

	int (*slowpath_start) (struct qed_dev * cdev,
			       struct qed_slowpath_params * params);

	int (*slowpath_stop) (struct qed_dev * cdev);

	/* Requests to use `cnt' interrupts for fastpath.
	 * upon success, returns number of interrupts allocated for fastpath.
	 */
	int (*set_fp_int) (struct qed_dev * cdev, u16 cnt);

	/* Fills `info' with pointers required for utilizing interrupts */
	int (*get_fp_int) (struct qed_dev * cdev, struct qed_int_info * info);

	 u32(*sb_init) (struct qed_dev * cdev,
			struct qed_sb_info * sb_info,
			void *sb_virt_addr,
			dma_addr_t sb_phy_addr,
			u16 sb_id, enum qed_sb_type type);

	 u32(*sb_release) (struct qed_dev * cdev,
			   struct qed_sb_info * sb_info,
			   u16 sb_id, enum qed_sb_type type);

/**
 * @brief Return information regarding a SB.
 *
 * @param cdev
 * @param sb - status block for which to print the information.
 * @param qid - used to map SB to engine; Can be 0 for non 100g.
 * @param sb_dbg - Pointer struct to fill in with data regarding SB.
 *
 * @return 0 iff success.
 */
	int (*get_sb_info) (struct qed_dev * cdev,
			    struct qed_sb_info * sb,
			    u16 qid, struct qed_sb_info_dbg * sb_dbg);

	void (*simd_handler_config) (struct qed_dev * cdev,
				     void *token,
				     int index, void (*handler) (void *));

	void (*simd_handler_clean) (struct qed_dev * cdev, int index);

	int (*dbg_grc) (struct qed_dev * cdev,
			void *buffer, u32 * num_dumped_bytes);

	int (*dbg_grc_size) (struct qed_dev * cdev);

	int (*dbg_idle_chk) (struct qed_dev * cdev,
			     void *buffer, u32 * num_dumped_bytes);

	int (*dbg_idle_chk_size) (struct qed_dev * cdev);

	int (*dbg_reg_fifo) (struct qed_dev * cdev,
			     void *buffer, u32 * num_dumped_bytes);

	int (*dbg_reg_fifo_size) (struct qed_dev * cdev);

	int (*dbg_igu_fifo) (struct qed_dev * cdev,
			     void *buffer, u32 * num_dumped_bytes);

	int (*dbg_igu_fifo_size) (struct qed_dev * cdev);

	int (*dbg_protection_override) (struct qed_dev * cdev,
					void *buffer, u32 * num_dumped_bytes);

	int (*dbg_protection_override_size) (struct qed_dev * cdev);

	int (*dbg_mcp_trace) (struct qed_dev * cdev,
			      void *buffer, u32 * num_dumped_bytes);

	int (*dbg_mcp_trace_size) (struct qed_dev * cdev);

	int (*dbg_phy) (struct qed_dev * cdev,
			void *buffer, u32 * num_dumped_bytes);

	int (*dbg_phy_size) (struct qed_dev * cdev);

	int (*dbg_fw_asserts) (struct qed_dev * cdev,
			       void *buffer, u32 * num_dumped_bytes);

	int (*dbg_fw_asserts_size) (struct qed_dev * cdev);

	int (*dbg_ilt) (struct qed_dev * cdev,
			void *buffer, u32 * num_dumped_bytes);

	int (*dbg_ilt_size) (struct qed_dev * cdev);

	 u8(*dbg_get_debug_engine) (struct qed_dev * cdev);

	void (*dbg_set_debug_engine) (struct qed_dev * cdev, int engine_number);

	int (*dbg_all_data) (struct qed_dev * cdev, void *buffer);

	int (*dbg_all_data_size) (struct qed_dev * cdev);

	void (*dbg_save_all_data) (struct qed_dev * cdev, bool print_dbg_data);

/**
 * @brief can_link_change - can the instance change the link or not
 *
 * @param cdev
 *
 * @return true if link-change is allowed, false otherwise.
 */
	 bool(*can_link_change) (struct qed_dev * cdev);

/**
 * @brief set_link - set links according to params
 *
 * @param cdev
 * @param params - values used to override the default link configuration
 *
 * @return 0 on success, error otherwise.
 */
	int (*set_link) (struct qed_dev * cdev,
			 struct qed_link_params * params);

/**
 * @brief get_link - returns the current link state.
 *
 * @param cdev
 * @param if_link - structure to be filled with current link configuration.
 */
	void (*get_link) (struct qed_dev * cdev,
			  struct qed_link_output * if_link);

/**
 * @brief - drains chip in case Tx completions fail to arrive due to pause.
 *
 * @param cdev
 */
	int (*drain) (struct qed_dev * cdev);

/**
 * @brief update_msglvl - update module debug level
 *
 * @param cdev
 * @param dp_module
 * @param dp_level
 * @param context - currently unused, can be passed as NULL
 */
	void (*update_msglvl) (struct qed_dev * cdev,
			       u32 dp_module, u8 dp_level, void *ctx);

	void (*chain_params_init) (struct qed_chain_params * p_params,
				   enum qed_chain_use_mode intended_use,
				   enum qed_chain_mode mode,
				   enum qed_chain_cnt_type cnt_type,
				   u32 num_elems, size_t elem_size);

	int (*chain_alloc) (struct qed_dev * cdev,
			    struct qed_chain * p_chain,
			    struct qed_chain_params * p_params);

	void (*chain_free) (struct qed_dev * cdev, struct qed_chain * p_chain);

	/* The function need to receive:
	 * Chain for printing.
	 * Allocated buffer that has to be managed by the calling function.
	 * Index of the element that need to print from.
	 * Index of the last element to print.
	 * Boolian that indicates if to print chain metedata, usually true in the first call.
	 * Function pointer for printing the element and the metadata,
	 * If the pointers are NULL function will use default functions
	 */
	int (*chain_print) (struct qed_chain
			    * p_chain,
			    char
			    *buffer,
			    u32
			    buffer_size,
			    u32
			    * element_indx,
			    u32
			    stop_indx,
			    bool
			    print_metadata,
			    bool
			    print_to_kernel,
			    int (*func_ptr_print_element) (struct qed_chain *
							   p_chain,
							   void
							   *p_element,
							   char
							   *buf_to_print),
			    int (*func_ptr_print_metadata) (struct qed_chain *
							    p_chain,
							    char *buffer));

/**
 * @brief nvm_get_cmd - Invoke mcp nvm get command
 *
 * @param cdev
 * @param cmd - qed_mcp command
 * @param offset
 * @param buf - buffer
 * @param len - buffer length
 *
 * @return 0 on success, error otherwise.
 */
	int (*nvm_get_cmd) (struct qed_dev * cdev,
			    u32 cmd, u32 offset, u8 * buf, u32 len);
/**
 * @brief nvm_put_cmd - Invoke mcp nvm get command
 *
 * @param cdev
 * @param cmd - qed_mcp command
 * @param offset
 * @param buf - buffer
 * @param len - buffer length
 *
 * @return 0 on success, error otherwise.
 */
	int (*nvm_set_cmd) (struct qed_dev * cdev,
			    u32 cmd, u32 offset, u8 * buf, u32 len);

/**
 * @brief nvm_flash - Flash nvm data.
 *
 * @param cdev
 * @param name - file containing the data
 *
 * @return 0 on success, error otherwise.
 */
	int (*nvm_flash) (struct qed_dev * cdev, const char *name);

/**
 * @brief nvm_get_image - reads an entire image from nvram
 *
 * @param cdev
 * @param type - type of the request nvram image
 * @param buf - preallocated buffer to fill with the image
 * @param len - length of the allocated buffer
 *
 * @return 0 on success, error otherwise
 */
	int (*nvm_get_image) (struct qed_dev * cdev,
			      enum qed_nvm_images type, u8 * buf, u16 len);

/**
 * @brief get_coalesce - Get coalesce parameters in usec
 *
 * @param cdev
 * @param rx_coal - Rx coalesce value in usec
 * @param tx_coal - Tx coalesce value in usec
 *
 */
	void (*get_coalesce) (struct qed_dev * cdev,
			      u16 * rx_coal, u16 * tx_coal);

/**
 * @brief set_coalesce - Configure Rx coalesce value in usec
 *
 * @param cdev
 * @param rx_coal - Rx coalesce value in usec
 * @param tx_coal - Tx coalesce value in usec
 * @param qid - Queue index
 * @param sb_id - Status Block Id
 *
 * @return 0 on success, error otherwise.
 */
	int (*set_coalesce) (struct qed_dev * cdev,
			     u16 rx_coal, u16 tx_coal, void *handle);

/**
 * @brief set_led - Configure LED mode
 *
 * @param cdev
 * @param mode - LED mode
 *
 * @return 0 on success, error otherwise.
 */
	int (*set_led) (struct qed_dev * cdev, enum qed_led_mode mode);

/**
 * @brief recovery_process - Trigger a recovery process
 *
 * @param cdev
 *
 * @return 0 on success, error otherwise.
 */
	int (*recovery_process) (struct qed_dev * cdev);

/**
 * @brief recovery_prolog - Execute the prolog operations of a recovery process
 *
 * @param cdev
 *
 * @return 0 on success, error otherwise.
 */
	int (*recovery_prolog) (struct qed_dev * cdev);

/**
 * @brief attn_clr_enable - Prevent attentions from being reasserted
 *
 * @param cdev
 * @param clr_enable
 */
	void (*attn_clr_enable) (struct qed_dev * cdev, bool clr_enable);

/**
 * @brief db_recovery_add - add doorbell information to the doorbell
 * recovery mechanism.
 *
 * @param cdev
 * @param db_addr - doorbell address
 * @param db_data - address of where db_data is stored
 * @param db_is_32b - doorbell is 32b pr 64b
 * @param db_is_user - doorbell recovery addresses are user or kernel space
 */
	int (*db_recovery_add) (struct qed_dev * cdev,
				void __iomem * db_addr,
				void *db_data,
				enum qed_db_rec_width db_width,
				enum qed_db_rec_space db_space);

/**
 * @brief db_recovery_del - remove doorbell information from the doorbell
 * recovery mechanism. db_data serves as key (db_addr is not unique).
 *
 * @param cdev
 * @param db_addr - doorbell address
 * @param db_data - address where db_data is stored. Serves as key for the
 *		    entry to delete.
 */
	int (*db_recovery_del) (struct qed_dev * cdev,
				void __iomem * db_addr, void *db_data);

/**
 * @brief update_drv_state - API to inform the change in the driver state.
 *
 * @param cdev
 * @param active
 */
	int (*update_drv_state) (struct qed_dev * cdev, bool active);

/**
 * @brief update_mac - API to inform the change in the mac address
 *
 * @param cdev
 * @param mac
 */
	int (*update_mac) (struct qed_dev * cdev, const u8 * mac);

/**
 * @brief update_mtu - API to inform the change in the mtu
 *
 * @param cdev
 * @param mtu
 */
	int (*update_mtu) (struct qed_dev * cdev, u16 mtu);

/**
 * @brief update_wol - update of changes in the WoL configuration
 *
 * @param cdev
 * @param enabled - true iff WoL should be enabled.
 */
	int (*update_wol) (struct qed_dev * cdev, bool enabled);

/**
 * @brief read_module_eeprom
 *
 * @param cdev
 * @param buf - buffer
 * @param dev_addr - PHY device memory region
 * @param offset - offset into eeprom contents to be read
 * @param len - buffer length, i.e., max bytes to be read
 */
	int (*read_module_eeprom) (struct qed_dev * cdev,
				   char *buf, u8 dev_addr, u32 offset, u32 len);

	void (*get_vport_stats) (struct qed_dev * cdev,
				 struct qed_eth_stats * stats);

/**
 * @brief - get sfp stats.
 *
 * @param cdev
 * @param sfp - sfp stats
 *
 * @return 0 on success, error otherwise.
 */
	int (*get_sfp_stats) (struct qed_dev * cdev,
			      struct qed_sfp_stats * sfp);

/**
 * @brief get_affin_hwfn_idx
 *
 * @param cdev
 */
	 u8(*get_affin_hwfn_idx) (struct qed_dev * cdev);

/**
 * @brief - is_fip_special_mode
 *
 * @param cdev
 *
 * @return true if device is in FIP special mode, false otherwise.
 */
	 bool(*is_fip_special_mode) (struct qed_dev * cdev);

/**
 * @brief - AHS API to report fatal errors to MFW.
 * The received error message is also DP_NOTICE'd.
 *
 * @param cdev
 * @param fmt - error message format
 */
	void (*mfw_report) (struct qed_dev * cdev, char *fmt, ...);

/**
 * @brief read_nvm_cfg - Read NVM config attribute value.
 * @param cdev
 * @param buf - buffer
 * @param cmd - NVM CFG command id
 * @param entity_id - Entity id
 *
 */
	int (*read_nvm_cfg) (struct qed_dev * cdev,
			     u8 ** buf, u32 cmd, u32 entity_id);

/**
 * @brief read_nvm_cfg - Read NVM config attribute value.
 * @param cdev
 * @param cmd - NVM CFG command id
 *
 * @return config id length, 0 on error.
 */
	int (*read_nvm_cfg_len) (struct qed_dev * cdev, u32 cmd);

/**
 * @brief set_grc_config - Configure value for grc config id.
 * @param cdev
 * @param cfg_id - grc config id
 * @param val - grc config value
 *
 */
	int (*set_grc_config) (struct qed_dev * cdev, u32 cfg_id, u32 val);

/**
 * @brief set_recov_in_prog - Set/clear recov_in_prog flag.
 * @param cdev
 * @param enable - Flag vlaue to be set.
 *
 */
	void (*set_recov_in_prog) (struct qed_dev * cdev, bool enable);

/**
 * @brief get_recov_in_prog - Get state of recov_in_prog flag.
 * @param cdev
 *
 */
	 bool(*get_recov_in_prog) (struct qed_dev * cdev);

/**
 * @brief wait_for_dcbx_to_enable - wait until dcbx is enabled.
 * @param cdev
 *
 */
	void (*wait_for_dcbx_to_enable) (struct qed_dev * cdev);

/**
 * @brief get_esl_status - Get Enhanced System Lockdown status
 * @param cdev
 * @param esl_active - ESL active or not.
 *
 */
	int (*get_esl_status) (struct qed_dev * cdev, bool * esl_active);

/**
 * @brief internal_trace - store buffer into internal trace storage
 *
 * @param cdev
 * @param fmt
 * @param ...
 */
	void (*internal_trace) (struct qed_dev * cdev, char *fmt, ...);

	/**
	 * @brief update_int_msglvl - update module internal debug level
	 *
	 * @param cdev
	 * @param dp_module
	 * @param dp_level
	 */
	void (*update_int_msglvl) (struct qed_dev * cdev,
				   u32 dp_module, u8 dp_level);

/**
 * @brief devlink_register - register devlink instance
 * @param cdev
 *
 * @return 0 on success, error otherwise.
 */
	struct devlink *(*devlink_register) (struct qed_dev * cdev,
					     void *drv_ctx);

/**
 * @brief devlink_unregister - unregister devlink instance
 * @param devlink
 *
 * @return 0 on success, error otherwise.
 */
	void (*devlink_unregister) (struct devlink * devlink);

/**
 * @brief report_fatal_error - report error to devlink
 * @param devlink
 * @param err_type
 *
 * @return 0 on success, error otherwise.
 */
	int (*report_fatal_error) (struct devlink * devlink,
				   enum qed_hw_err_type err_type,
				   int recovery_enable);
/**
 * @brief set_dev_reuse - Reuse cdev during recovery (i.e., across remove/probe)
 * @param cdev
 * @param flag
 *
 */
	void (*set_dev_reuse) (struct qed_dev * cdev, bool flag);

/**
 * @brief set_aer_state - Set AER status
 *
 * @param cdev
 *
 * @return true if device reset is required, false otherwise.
 */
	void (*set_aer_state) (struct qed_dev * cdev, bool aer);

/**
 * @brief is_hot_reset_occured_or_in_prgs - Determines if hot reset has
 * occurred or is in progress
 *
 * @param cdev
 *
 * @return true if hot reset has occurred or in progress, false otherwise.
 */
	 bool(*is_hot_reset_occured_or_in_prgs) (struct qed_dev * cdev);

/**
 * @brief set_vf_stats_bin_id - Set vf bin_id from where stats need to read.
 * @param cdev
 * @param vf_id - VF id
 *
 * @return 0 on success, error otherwise.
 */
	int (*set_vf_stats_bin_id) (struct qed_dev * cdev, u16 vf_id);
};

struct qed_lag_ops {
	int (*lag_create) (struct qed_dev * cdev,
			   enum qed_lag_type lag_type,
			   void (*link_change_cb) (void *cxt),
			   void *cxt, u8 active_ports);

	int (*lag_modify) (struct qed_dev * cdev, u8 port_id, u8 link_active);

	int (*lag_destroy) (struct qed_dev * cdev);
};

/**
 * @brief qed_get_protocol_version
 *
 * @param protocol
 *
 * @return version supported by qed for given protocol driver
 */
u32 qed_get_protocol_version(enum qed_protocol protocol);

static inline void qed_get_current_timestamp(struct qed_timestamp *ts)
{
	ts->sec = local_clock();
	ts->rem_usec = do_div(ts->sec, 1000000000) / 1000;
}

/*
 * PF parameters (according to personality/protocol)
 */

#define QED_ROCE_PROTOCOL_INDEX (3)

struct qed_eth_pf_params {
	/* The following parameters are used during HW-init
	 * and these parameters need to be passed as arguments
	 * to update_pf_params routine invoked before slowpath start
	 */
	u16 num_cons;

	/* per-VF number of CIDs */
	u8 num_vf_cons;
#define ETH_PF_PARAMS_VF_CONS_DEFAULT   (32)

	/* To enable arfs, previous to HW-init a positive number needs to be
	 * set [as filters require allocated searcher ILT memory].
	 * This will set the maximal number of configured steering-filters.
	 */
	u32 num_arfs_filters;

	/* To allow VF to change its MAC despite of PF set forced MAC. */
	bool allow_vf_mac_change;

	/* pqset specific params */
	u16 num_desired_pqset;	/* keep 0 for legacy mode */
	u32 pqset_offload_type;	/* enum qed_pqset_offld_type */
	u32 pqset_aux_offload_type;	/* enum qed_pqset_aux_offld_type */

	/* number of PQs for offload protocol - rdma/storage. up to max tcs */
	u8 pqset_offload_pqs_num;

	/* number of PQs for aux - ACK or LLT. up to max tcs */
	u8 pqset_aux_offload_pqs_num;

	/* "default pqset" uses the default PQs which are configured for
	 * mcos, ofld and aux.
	 */
	u8 pqset_shared_default;

	/* The following parameter disables acquiring of l2 info lock.
	 * If set true, no need to acquire lock on l2 info.
	 */
	bool is_l2_lockless;

	/* To validate if ENS mode is enabled/disabled.
	 * If set, ENS mode is enabled.
	 */
	u8 is_ens_mode_enabled;

	/* set aysmmetric bw distribution to port */
	u8 rx_asymmetric_bw_mode;
};

/* Most of the the parameters below are described in the FW FCoE HSI */
struct qed_fcoe_pf_params {
	/* The following parameters are used during protocol-init */
	u64 glbl_q_params_addr;
	u64 bdq_pbl_base_addr[2];

	/* The following parameters are used during HW-init
	 * and these parameters need to be passed as arguments
	 * to update_pf_params routine invoked before slowpath start
	 */
	u16 num_cons;
	u16 num_tasks;

	/* The following parameters are used during protocol-init */
	u16 sq_num_pbl_pages;

	u16 cq_num_entries;
	u16 cmdq_num_entries;
	u16 rq_buffer_log_size;
	u16 mtu;
	u16 dummy_icid;
	u16 bdq_xoff_threshold[2];
	u16 bdq_xon_threshold[2];
	u16 rq_buffer_size;
	u8 num_cqs;		/* num of global CQs */
	u8 log_page_size;
	u8 gl_rq_pi;
	u8 gl_cmd_pi;
	u8 debug_mode;
	u8 is_target;
	u8 bdq_pbl_num_entries[2];
};

/* Most of the the parameters below are described in the FW iSCSI / TCP HSI */
struct qed_iscsi_pf_params {
	u64 glbl_q_params_addr;
	u64 bdq_pbl_base_addr[3];
	u16 cq_num_entries;
	u16 cmdq_num_entries;
	u32 two_msl_timer;
	u16 tx_sws_timer;
	/* The following parameters are used during HW-init
	 * and these parameters need to be passed as arguments
	 * to update_pf_params routine invoked before slowpath start
	 */
	u16 num_cons;
	u16 num_tasks;

	/* The following parameters are used during protocol-init */
	u16 half_way_close_timeout;
	u16 bdq_xoff_threshold[3];
	u16 bdq_xon_threshold[3];
	u16 cmdq_xoff_threshold;
	u16 cmdq_xon_threshold;
	u16 rq_buffer_size;

	u8 num_sq_pages_in_ring;
	u8 num_uhq_pages_in_ring;
	u8 num_queues;
	u8 log_page_size;
	u8 log_page_size_conn;
	u8 rqe_log_size;
	u8 max_syn_rt;
	u8 max_fin_rt;
	u8 gl_rq_pi;
	u8 gl_cmd_pi;
	u8 debug_mode;
	u8 ll2_ooo_queue_id;

	u8 is_target;
	u8 is_soc_en;
	u8 soc_num_of_blocks_log;
	u8 bdq_pbl_num_entries[3];
	u8 disable_stats_collection;
};

enum qed_rdma_protocol {
	QED_RDMA_PROTOCOL_DEFAULT,
	QED_RDMA_PROTOCOL_NONE,
	QED_RDMA_PROTOCOL_ROCE,
	QED_RDMA_PROTOCOL_IWARP,
};

struct qed_rdma_pf_params {
	/* Supplied to QED during resource allocation (may affect the ILT and
	 * the doorbell BAR).
	 */
	u32 min_dpis;		/* number of requested DPIs */
	u32 num_qps;		/* number of requested Queue Pairs */
	u32 num_vf_qps;		/* number of requested Queue Pairs for VF */
	u32 num_vf_tasks;	/* number of requested MRs for VF */
	u32 num_srqs;		/* number of requested SRQs */
	u32 num_vf_srqs;	/* number of requested SRQs for VF */
	u8 gl_pi;		/* protocol index */

	/* Max number of CNQs - limits number of QED_RDMA_CNQ feature,
	 * Allowing an incrementation in QED_PF_L2_QUE.
	 * To disable CNQs, use dedicated value instead of `0'.
	 */
#define QED_RDMA_PF_PARAMS_CNQS_NONE    (0xffff)
	u16 max_cnqs;

	/* TCP port number used for the iwarp traffic */
	u16 iwarp_port;
	enum qed_rdma_protocol rdma_protocol;
};

struct qed_pf_params {
	struct qed_eth_pf_params eth_pf_params;
	struct qed_fcoe_pf_params fcoe_pf_params;
	struct qed_iscsi_pf_params iscsi_pf_params;
	struct qed_rdma_pf_params rdma_pf_params;
};

#define QED_SB_IDX              0x0002

#define RX_PI           0
#define TX_PI(tc)       (RX_PI + 1 + tc)

#define LL2_VF_RX_PI    9
#define LL2_VF_TX_PI    10

#ifndef QED_INT_MODE
#define QED_INT_MODE
enum qed_int_mode {
	QED_INT_MODE_INTA,
	QED_INT_MODE_MSIX,
	QED_INT_MODE_MSI,
	QED_INT_MODE_POLL,
};
#endif

struct qed_sb_info {
	void *sb_virt;		/* ptr to "struct status_block_e{4,5}" */
	u32 sb_size;		/* size of "struct status_block_e{4,5}" */
	__le16 *sb_pi_array;	/* ptr to "sb_virt->pi_array" */
	__le32 *sb_prod_index;	/* ptr to "sb_virt->prod_index" */
#define STATUS_BLOCK_PROD_INDEX_MASK    0xFFFFFF

	dma_addr_t sb_phys;
	u32 sb_ack;		/* Last given ack */
	u16 igu_sb_id;
	void __iomem *igu_addr;
	u8 flags;
#define QED_SB_INFO_INIT        0x1
#define QED_SB_INFO_SETUP       0x2

	struct qed_dev *cdev;
};

struct qed_sb_info_dbg {
	u32 igu_prod;
	u32 igu_cons;
	u16 pi[PIS_PER_SB];
};

struct qed_sb_cnt_info {
	/* Original, current, and free SBs for PF */
	u32 orig;
	u32 cnt;
	u32 free_cnt;

	/* Original, current and free SBS for child VFs */
	u32 iov_orig;
	u32 iov_cnt;
	u32 free_cnt_iov;
};

static inline u16 qed_sb_update_sb_idx(struct qed_sb_info *sb_info)
{
	u32 prod = 0;
	u16 rc = 0;

	prod = le32_to_cpu(*sb_info->sb_prod_index) &
	    STATUS_BLOCK_PROD_INDEX_MASK;

	/* make sure actual value of prod is fetched */
	rmb();

	if (sb_info->sb_ack != prod) {
		sb_info->sb_ack = prod;
		rc |= QED_SB_IDX;
	}

	return rc;
}

/**
 * @brief This function creates an update command for interrupts that is
 *        written to the IGU.
 *
 * @param sb_info       - This is the structure allocated and
 *                 initialized per status block. Assumption is
 *                 that it was initialized using qed_sb_init
 * @param int_cmd       - Enable/Disable/Nop
 * @param upd_flg       - whether igu consumer should be
 *                 updated.
 *
 * @return inline void
 */
static inline void qed_sb_ack(struct qed_sb_info *sb_info,
			      enum igu_int_cmd int_cmd, u8 upd_flg)
{
	struct igu_prod_cons_update igu_ack;
	u32 val;

	memset(&igu_ack, 0, sizeof(struct igu_prod_cons_update));
	SET_FIELD(igu_ack.sb_id_and_flags, IGU_PROD_CONS_UPDATE_SB_INDEX,
		  sb_info->sb_ack);
	SET_FIELD(igu_ack.sb_id_and_flags, IGU_PROD_CONS_UPDATE_UPDATE_FLAG,
		  upd_flg);
	SET_FIELD(igu_ack.sb_id_and_flags, IGU_PROD_CONS_UPDATE_ENABLE_INT,
		  int_cmd);
	SET_FIELD(igu_ack.sb_id_and_flags, IGU_PROD_CONS_UPDATE_SEGMENT_ACCESS,
		  IGU_SEG_ACCESS_REG);
	igu_ack.sb_id_and_flags = cpu_to_le32(igu_ack.sb_id_and_flags);

	val = le32_to_cpu(igu_ack.sb_id_and_flags);
	DIRECT_REG_WR(sb_info->igu_addr, val);
	/* Both segments (interrupts & acks) are written to same place address;
	 * Need to guarantee all commands will be received (in-order) by HW
	 * DIRECT_REG_WR prevents this as it has wmb() inside it.
	 */
}

static inline void __internal_ram_wr(void *p_hwfn,
				     void __iomem * addr, int size, u32 * data)
{
	unsigned int i;

	for (i = 0; i < size / sizeof(*data); i++)
		DIRECT_REG_WR(&((u32 __iomem *) addr)[i], data[i]);
}

static inline void internal_ram_wr(void __iomem * addr, int size, u32 * data)
{
	__internal_ram_wr(NULL, addr, size, data);
}
#endif
