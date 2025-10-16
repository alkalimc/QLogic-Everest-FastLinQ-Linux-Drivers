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

#ifndef __FCOE_COMMON__
#define __FCOE_COMMON__
/*********************/
/* FCOE FW CONSTANTS */
/*********************/

#define FC_ABTS_REPLY_MAX_PAYLOAD_LEN   12

/* fields coppied from ABTSrsp pckt */
struct fcoe_abts_pkt {
	__le32 abts_rsp_fc_payload_lo;	/* HSI_COMMENT: Abts flow: last 32 bits of fcPayload, out of 96 */
	__le16 abts_rsp_rx_id;	/* HSI_COMMENT: Abts flow: rxId parameter of the abts packet */
	u8 abts_rsp_rctl;	/* HSI_COMMENT: Abts flow: rctl parameter of the abts packet */
	u8 reserved2;
};

/* FCoE additional WQE (Sq/ XferQ) information */
union fcoe_additional_info_union {
	__le32 previous_tid;	/* HSI_COMMENT: Previous tid. Used for Send XFER WQEs in Multiple continuation mode - Target only. */
	__le32 parent_tid;	/* HSI_COMMENT: Parent tid. Used for write tasks in a continuation mode - Target only */
	__le32 burst_length;	/* HSI_COMMENT: The desired burst length. */
	__le32 seq_rec_updated_offset;	/* HSI_COMMENT: The updated offset in SGL - Used in sequence recovery */
};

/* Cached data sges */
struct fcoe_exp_ro {
	__le32 data_offset;	/* HSI_COMMENT: data-offset */
	__le32 reserved;	/* HSI_COMMENT: High data-offset */
};

/* Union of Cleanup address / expected relative offsets */
union fcoe_cleanup_addr_exp_ro_union {
	struct regpair abts_rsp_fc_payload_hi;	/* HSI_COMMENT: Abts flow: first 64 bits of fcPayload, out of 96 */
	struct fcoe_exp_ro exp_ro;	/* HSI_COMMENT: Expected relative offsets */
};

/* FCoE Ramrod Command IDs  */
enum fcoe_completion_status {
	FCOE_COMPLETION_STATUS_SUCCESS,	/* HSI_COMMENT: FCoE ramrod completed successfully */
	FCOE_COMPLETION_STATUS_FCOE_VER_ERR,	/* HSI_COMMENT: Wrong FCoE version */
	FCOE_COMPLETION_STATUS_SRC_MAC_ADD_ARR_ERR,	/* HSI_COMMENT: src_mac_arr for the current physical port is full- allocation failed */
	MAX_FCOE_COMPLETION_STATUS
};

/* FC address (SID/DID) network presentation  */
struct fc_addr_nw {
	u8 addr_lo;		/* HSI_COMMENT: First byte of the SID/DID address that comes/goes from/to the NW (for example if SID is 11:22:33 - this is 0x11) */
	u8 addr_mid;
	u8 addr_hi;
};

/* FCoE connection offload */
struct fcoe_conn_offload_ramrod_data {
	struct regpair sq_pbl_addr;	/* HSI_COMMENT: SQ Pbl base address */
	struct regpair sq_curr_page_addr;	/* HSI_COMMENT: SQ current page address */
	struct regpair sq_next_page_addr;	/* HSI_COMMENT: SQ next page address */
	struct regpair xferq_pbl_addr;	/* HSI_COMMENT: XFERQ Pbl base address */
	struct regpair xferq_curr_page_addr;	/* HSI_COMMENT: XFERQ current page address */
	struct regpair xferq_next_page_addr;	/* HSI_COMMENT: XFERQ next page address */
	struct regpair respq_pbl_addr;	/* HSI_COMMENT: RESPQ Pbl base address */
	struct regpair respq_curr_page_addr;	/* HSI_COMMENT: RESPQ current page address */
	struct regpair respq_next_page_addr;	/* HSI_COMMENT: RESPQ next page address */
	__le16 dst_mac_addr_lo;	/* HSI_COMMENT: First word of the MAC address that comes/goes from/to the NW (for example if MAC is 11:22:33:44:55:66 - this is 0x2211) */
	__le16 dst_mac_addr_mid;
	__le16 dst_mac_addr_hi;
	__le16 src_mac_addr_lo;	/* HSI_COMMENT: Source MAC address in NW order - First word of the MAC address that comes/goes from/to the NW (for example if MAC is 11:22:33:44:55:66 - this is 0x2211) */
	__le16 src_mac_addr_mid;
	__le16 src_mac_addr_hi;
	__le16 tx_max_fc_pay_len;	/* HSI_COMMENT: The maximum acceptable FC payload size (Buffer-to-buffer Receive Data_Field size) supported by target, received during both FLOGI and PLOGI, minimum value should be taken */
	__le16 e_d_tov_timer_val;	/* HSI_COMMENT: E_D_TOV timeout value in resolution of 1 msec */
	__le16 rx_max_fc_pay_len;	/* HSI_COMMENT: Maximum acceptable FC payload size supported by us */
	__le16 vlan_tag;
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_VLAN_ID_MASK      0xFFF	/* HSI_COMMENT: Vlan id */
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_VLAN_ID_SHIFT     0
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_CFI_MASK  0x1	/* HSI_COMMENT: Canonical format indicator */
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_CFI_SHIFT 12
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_PRIORITY_MASK     0x7	/* HSI_COMMENT: Vlan priority */
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_PRIORITY_SHIFT    13
	__le16 physical_q0;	/* HSI_COMMENT: Physical QM queue to be linked to logical queue 0 (fastPath queue) */
	__le16 rec_rr_tov_timer_val;	/* HSI_COMMENT: REC_TOV timeout value in resolution of 1 msec  */
	struct fc_addr_nw s_id;	/* HSI_COMMENT: Source ID in NW order, received during FLOGI */
	u8 max_conc_seqs_c3;	/* HSI_COMMENT: Maximum concurrent Sequences for Class 3 supported by target, received during PLOGI */
	struct fc_addr_nw d_id;	/* HSI_COMMENT: Destination ID in NW order, received after inquiry of the fabric network */
	u8 flags;
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_B_CONT_INCR_SEQ_CNT_MASK  0x1	/* HSI_COMMENT: Continuously increasing SEQ_CNT indication, received during PLOGI */
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_B_CONT_INCR_SEQ_CNT_SHIFT 0
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_B_CONF_REQ_MASK   0x1	/* HSI_COMMENT: Confirmation request supported */
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_B_CONF_REQ_SHIFT  1
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_B_REC_VALID_MASK  0x1	/* HSI_COMMENT: REC allowed */
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_B_REC_VALID_SHIFT 2
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_B_VLAN_FLAG_MASK  0x1	/* HSI_COMMENT: Does inner vlan exist */
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_B_VLAN_FLAG_SHIFT 3
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_B_SINGLE_VLAN_MASK        0x1	/* HSI_COMMENT: Does a single vlan (inner/outer) should be used. - UFP mode */
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_B_SINGLE_VLAN_SHIFT       4
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_MODE_MASK 0x3	/* HSI_COMMENT: indication for conn mode: 0=Initiator, 1=Target, 2=Both Initiator and Traget */
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_MODE_SHIFT        5
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_RESERVED0_MASK    0x1
#define FCOE_CONN_OFFLOAD_RAMROD_DATA_RESERVED0_SHIFT   7
	__le16 conn_id;		/* HSI_COMMENT: Drivers connection ID. Should be sent in EQs to speed-up drivers access to connection data. */
	u8 def_q_idx;		/* HSI_COMMENT: Default queue number to be used for unsolicited traffic */
	u8 reserved[5];
};

/* FCoE terminate connection request  */
struct fcoe_conn_terminate_ramrod_data {
	struct regpair terminate_params_addr;	/* HSI_COMMENT: Terminate params ptr */
};

/* FCoE device type */
enum fcoe_device_type {
	FCOE_TASK_DEV_TYPE_DISK,
	FCOE_TASK_DEV_TYPE_TAPE,
	MAX_FCOE_DEVICE_TYPE
};

/* Data sgl */
struct fcoe_slow_sgl_ctx {
	struct regpair base_sgl_addr;	/* HSI_COMMENT: Address of first SGE in SGL */
	__le16 curr_sge_off;	/* HSI_COMMENT: Offset in current BD (in bytes) */
	__le16 remainder_num_sges;	/* HSI_COMMENT: Number of BDs */
	__le16 curr_sgl_index;	/* HSI_COMMENT: Index of current SGE */
	__le16 reserved;
};

/* Union of DIX SGL / cached DIX sges */
union fcoe_dix_desc_ctx {
	struct fcoe_slow_sgl_ctx dix_sgl;	/* HSI_COMMENT: DIX slow-SGL data base */
	struct scsi_sge cached_dix_sge;	/* HSI_COMMENT: Cached DIX sge */
};

/* Data sgl */
struct fcoe_fast_sgl_ctx {
	struct regpair sgl_start_addr;	/* HSI_COMMENT: Current sge address */
	__le32 sgl_byte_offset;	/* HSI_COMMENT: Byte offset from the beginning of the first page in the SGL. In case SGL starts in the middle of page then driver should init this value with the start offset */
	__le16 task_reuse_cnt;	/* HSI_COMMENT: The reuse count for that task. Wrap ion 4K value. */
	__le16 init_offset_in_first_sge;	/* HSI_COMMENT: offset from the beginning of the first page in the SGL, never changed by FW */
};

/* FCP CMD payload */
struct fcoe_fcp_cmd_payload {
	__le32 opaque[8];	/* HSI_COMMENT: The FCP_CMD payload */
};

/* FCP RSP payload */
struct fcoe_fcp_rsp_payload {
	__le32 opaque[6];	/* HSI_COMMENT: The FCP_RSP payload */
};

/* FCP RSP payload */
struct fcoe_fcp_xfer_payload {
	__le32 opaque[3];	/* HSI_COMMENT: The FCP_XFER payload */
};

/* FCoE firmware function init */
struct fcoe_init_func_ramrod_data {
	struct scsi_init_func_params func_params;	/* HSI_COMMENT: Common SCSI init params passed by driver to FW in function init ramrod */
	struct scsi_init_func_queues q_params;	/* HSI_COMMENT: SCSI RQ/CQ/CMDQ firmware function init parameters */
	__le16 mtu;		/* HSI_COMMENT: Max transmission unit */
	__le16 sq_num_pages_in_pbl;	/* HSI_COMMENT: Number of pages at Send Queue */
	__le32 reserved[3];
};

/* FCoE: Mode of the connection: Target or Initiator or both */
enum fcoe_mode_type {
	FCOE_INITIATOR_MODE = 0x0,
	FCOE_TARGET_MODE = 0x1,
	FCOE_BOTH_OR_NOT_CHOSEN = 0x3,
	MAX_FCOE_MODE_TYPE
};

/* Per PF FCoE receive path statistics - tStorm RAM structure */
struct fcoe_rx_stat {
	struct regpair fcoe_rx_byte_cnt;	/* HSI_COMMENT: Number of FCoE bytes that were received */
	struct regpair fcoe_rx_data_pkt_cnt;	/* HSI_COMMENT: Number of FCoE FCP DATA packets that were received */
	struct regpair fcoe_rx_xfer_pkt_cnt;	/* HSI_COMMENT: Number of FCoE FCP XFER RDY packets that were received */
	struct regpair fcoe_rx_other_pkt_cnt;	/* HSI_COMMENT: Number of FCoE packets which are not DATA/XFER_RDY that were received */
	__le32 fcoe_silent_drop_pkt_cmdq_full_cnt;	/* HSI_COMMENT: Number of packets that were silently dropped since CMDQ was full */
	__le32 fcoe_silent_drop_pkt_rq_full_cnt;	/* HSI_COMMENT: Number of packets that were silently dropped since RQ (BDQ) was full */
	__le32 fcoe_silent_drop_pkt_crc_error_cnt;	/* HSI_COMMENT: Number of packets that were silently dropped due to FC CRC error */
	__le32 fcoe_silent_drop_pkt_task_invalid_cnt;	/* HSI_COMMENT: Number of packets that were silently dropped since task was not valid */
	__le32 fcoe_silent_drop_total_pkt_cnt;	/* HSI_COMMENT: Number of FCoE packets that were silently dropped */
	__le32 rsrv;
};

/* FCoE SQE request type */
enum fcoe_sqe_request_type {
	SEND_FCOE_CMD,
	SEND_FCOE_MIDPATH,
	SEND_FCOE_ABTS_REQUEST,
	FCOE_EXCHANGE_CLEANUP,
	FCOE_SEQUENCE_RECOVERY,
	SEND_FCOE_XFER_RDY,
	SEND_FCOE_RSP,
	SEND_FCOE_RSP_WITH_SENSE_DATA,
	SEND_FCOE_TARGET_DATA,
	SEND_FCOE_INITIATOR_DATA,
	SEND_FCOE_XFER_CONTINUATION_RDY,	/* HSI_COMMENT: Xfer Continuation (==1) ready to be sent. Previous XFERs data received successfully. */
	SEND_FCOE_TARGET_ABTS_RSP,
	MAX_FCOE_SQE_REQUEST_TYPE
};

/* FCoe statistics request  */
struct fcoe_stat_ramrod_data {
	struct regpair stat_params_addr;	/* HSI_COMMENT: Statistics host address */
};

/* The fcoe storm task context protection-information of Ystorm */
struct protection_info_ctx {
	__le16 flags;
#define PROTECTION_INFO_CTX_HOST_INTERFACE_MASK 0x3	/* HSI_COMMENT: 0=none, 1=DIF, 2=DIX */
#define PROTECTION_INFO_CTX_HOST_INTERFACE_SHIFT        0
#define PROTECTION_INFO_CTX_DIF_TO_PEER_MASK    0x1	/* HSI_COMMENT: 0=no, 1=yes */
#define PROTECTION_INFO_CTX_DIF_TO_PEER_SHIFT   2
#define PROTECTION_INFO_CTX_VALIDATE_DIX_APP_TAG_MASK   0x1	/* HSI_COMMENT: 0=no, 1=yes */
#define PROTECTION_INFO_CTX_VALIDATE_DIX_APP_TAG_SHIFT  3
#define PROTECTION_INFO_CTX_INTERVAL_SIZE_LOG_MASK      0xF	/* HSI_COMMENT: Protection log interval (9=512 10=1024  11=2048 12=4096 13=8192) */
#define PROTECTION_INFO_CTX_INTERVAL_SIZE_LOG_SHIFT     4
#define PROTECTION_INFO_CTX_VALIDATE_DIX_REF_TAG_MASK   0x1	/* HSI_COMMENT: 0=no, 1=yes */
#define PROTECTION_INFO_CTX_VALIDATE_DIX_REF_TAG_SHIFT  8
#define PROTECTION_INFO_CTX_RESERVED0_MASK      0x7F
#define PROTECTION_INFO_CTX_RESERVED0_SHIFT     9
	u8 dix_block_size;	/* HSI_COMMENT: Source protection data size */
	u8 dst_size;		/* HSI_COMMENT: Destination protection data size */
};

/* The fcoe storm task context protection-information of Ystorm */
union protection_info_union_ctx {
	struct protection_info_ctx info;
	__le32 value;		/* HSI_COMMENT: If and only if this field is not 0 then protection is set */
};

/* FCP RSP payload */
struct fcp_rsp_payload_padded {
	struct fcoe_fcp_rsp_payload rsp_payload;	/* HSI_COMMENT: The FCP_RSP payload */
	__le32 reserved[2];
};

/* FCP RSP payload */
struct fcp_xfer_payload_padded {
	struct fcoe_fcp_xfer_payload xfer_payload;	/* HSI_COMMENT: The FCP_XFER payload */
	__le32 reserved[5];
};

/* Task params */
struct fcoe_tx_data_params {
	__le32 data_offset;	/* HSI_COMMENT: Data offset */
	__le32 offset_in_io;	/* HSI_COMMENT: For sequence cleanup */
	u8 flags;
#define FCOE_TX_DATA_PARAMS_OFFSET_IN_IO_VALID_MASK     0x1	/* HSI_COMMENT: Should we send offset in IO */
#define FCOE_TX_DATA_PARAMS_OFFSET_IN_IO_VALID_SHIFT    0
#define FCOE_TX_DATA_PARAMS_DROP_DATA_MASK      0x1	/* HSI_COMMENT: Should the PBF drop this data */
#define FCOE_TX_DATA_PARAMS_DROP_DATA_SHIFT     1
#define FCOE_TX_DATA_PARAMS_AFTER_SEQ_REC_MASK  0x1	/* HSI_COMMENT: Indication if the task after seqqence recovery flow */
#define FCOE_TX_DATA_PARAMS_AFTER_SEQ_REC_SHIFT 2
#define FCOE_TX_DATA_PARAMS_RESERVED0_MASK      0x1F
#define FCOE_TX_DATA_PARAMS_RESERVED0_SHIFT     3
	u8 dif_residual;	/* HSI_COMMENT: Residual from protection interval */
	__le16 seq_cnt;		/* HSI_COMMENT: Sequence counter */
	__le16 single_sge_saved_offset;	/* HSI_COMMENT: Saved SGE length for single SGE case */
	__le16 next_dif_offset;	/* HSI_COMMENT: Tracking next DIF offset in FC payload */
	__le16 seq_id;		/* HSI_COMMENT: Sequence ID (Set [saved] upon seq_cnt==0 (start of sequence) and used throughout sequence) */
	__le16 reserved3;
};

/* Middle path parameters: FC header fields provided by the driver */
struct fcoe_tx_mid_path_params {
	__le32 parameter;
	u8 r_ctl;
	u8 type;
	u8 cs_ctl;
	u8 df_ctl;
	__le16 rx_id;
	__le16 ox_id;
};

/* Task params */
struct fcoe_tx_params {
	struct fcoe_tx_data_params data;	/* HSI_COMMENT: Data offset */
	struct fcoe_tx_mid_path_params mid_path;
};

/* Union of FCP CMD payload / TX params / ABTS / Cleanup */
union fcoe_tx_info_union_ctx {
	struct fcoe_fcp_cmd_payload fcp_cmd_payload;	/* HSI_COMMENT: FCP CMD payload */
	struct fcp_rsp_payload_padded fcp_rsp_payload;	/* HSI_COMMENT: FCP RSP payload */
	struct fcp_xfer_payload_padded fcp_xfer_payload;	/* HSI_COMMENT: FCP XFER payload */
	struct fcoe_tx_params tx_params;	/* HSI_COMMENT: Task TX params */
};

/* The fcoe storm task context of Ystorm */
struct ystorm_fcoe_task_st_ctx {
	u8 task_type;		/* HSI_COMMENT: Task type. use enum fcoe_task_type  (use enum fcoe_task_type) */
	u8 sgl_mode;
#define YSTORM_FCOE_TASK_ST_CTX_TX_SGL_MODE_MASK        0x1	/* HSI_COMMENT: use enum scsi_sgl_mode (use enum scsi_sgl_mode) */
#define YSTORM_FCOE_TASK_ST_CTX_TX_SGL_MODE_SHIFT       0
#define YSTORM_FCOE_TASK_ST_CTX_RSRV_MASK       0x7F
#define YSTORM_FCOE_TASK_ST_CTX_RSRV_SHIFT      1
	u8 cached_dix_sge;	/* HSI_COMMENT: Dix sge is cached on task context */
	u8 expect_first_xfer;	/* HSI_COMMENT: Will let Ystorm know when it should initialize fcp_cmd_payload_params_union.params */
	__le32 num_pbf_zero_write;	/* HSI_COMMENT: The amount of bytes that PBF should dummy write - Relevant for protection only. */
	union protection_info_union_ctx protection_info_union;	/* HSI_COMMENT: Protection information */
	__le32 data_2_trns_rem;	/* HSI_COMMENT: Entire SGL-buffer remainder */
	struct scsi_sgl_params sgl_params;
	u8 reserved1[12];
	union fcoe_tx_info_union_ctx tx_info_union;	/* HSI_COMMENT: Union of FCP CMD payload / TX params / ABTS / Cleanup */
	union fcoe_dix_desc_ctx dix_desc;	/* HSI_COMMENT: Union of DIX SGL / cached DIX sges */
	struct scsi_cached_sges data_desc;	/* HSI_COMMENT: Data cached SGEs */
	__le16 ox_id;		/* HSI_COMMENT: OX-ID. Used in Target mode only */
	__le16 rx_id;		/* HSI_COMMENT: RX-ID. Used in Target mode only */
	__le32 task_rety_identifier;	/* HSI_COMMENT: Parameter field of the FCP CMDs FC header */
	u8 reserved2[8];
};

struct ystorm_fcoe_task_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	__le16 word0;		/* HSI_COMMENT: icid */
	u8 flags0;
#define YSTORM_FCOE_TASK_AG_CTX_NIBBLE0_MASK    0xF	/* HSI_COMMENT: connection_type */
#define YSTORM_FCOE_TASK_AG_CTX_NIBBLE0_SHIFT   0
#define YSTORM_FCOE_TASK_AG_CTX_BIT0_MASK       0x1	/* HSI_COMMENT: exist_in_qm0 */
#define YSTORM_FCOE_TASK_AG_CTX_BIT0_SHIFT      4
#define YSTORM_FCOE_TASK_AG_CTX_BIT1_MASK       0x1	/* HSI_COMMENT: exist_in_qm1 */
#define YSTORM_FCOE_TASK_AG_CTX_BIT1_SHIFT      5
#define YSTORM_FCOE_TASK_AG_CTX_BIT2_MASK       0x1	/* HSI_COMMENT: bit2 */
#define YSTORM_FCOE_TASK_AG_CTX_BIT2_SHIFT      6
#define YSTORM_FCOE_TASK_AG_CTX_BIT3_MASK       0x1	/* HSI_COMMENT: bit3 */
#define YSTORM_FCOE_TASK_AG_CTX_BIT3_SHIFT      7
	u8 flags1;
#define YSTORM_FCOE_TASK_AG_CTX_CF0_MASK        0x3	/* HSI_COMMENT: cf0 */
#define YSTORM_FCOE_TASK_AG_CTX_CF0_SHIFT       0
#define YSTORM_FCOE_TASK_AG_CTX_CF1_MASK        0x3	/* HSI_COMMENT: cf1 */
#define YSTORM_FCOE_TASK_AG_CTX_CF1_SHIFT       2
#define YSTORM_FCOE_TASK_AG_CTX_CF2SPECIAL_MASK 0x3	/* HSI_COMMENT: cf2special */
#define YSTORM_FCOE_TASK_AG_CTX_CF2SPECIAL_SHIFT        4
#define YSTORM_FCOE_TASK_AG_CTX_CF0EN_MASK      0x1	/* HSI_COMMENT: cf0en */
#define YSTORM_FCOE_TASK_AG_CTX_CF0EN_SHIFT     6
#define YSTORM_FCOE_TASK_AG_CTX_CF1EN_MASK      0x1	/* HSI_COMMENT: cf1en */
#define YSTORM_FCOE_TASK_AG_CTX_CF1EN_SHIFT     7
	u8 flags2;
#define YSTORM_FCOE_TASK_AG_CTX_BIT4_MASK       0x1	/* HSI_COMMENT: bit4 */
#define YSTORM_FCOE_TASK_AG_CTX_BIT4_SHIFT      0
#define YSTORM_FCOE_TASK_AG_CTX_RULE0EN_MASK    0x1	/* HSI_COMMENT: rule0en */
#define YSTORM_FCOE_TASK_AG_CTX_RULE0EN_SHIFT   1
#define YSTORM_FCOE_TASK_AG_CTX_RULE1EN_MASK    0x1	/* HSI_COMMENT: rule1en */
#define YSTORM_FCOE_TASK_AG_CTX_RULE1EN_SHIFT   2
#define YSTORM_FCOE_TASK_AG_CTX_RULE2EN_MASK    0x1	/* HSI_COMMENT: rule2en */
#define YSTORM_FCOE_TASK_AG_CTX_RULE2EN_SHIFT   3
#define YSTORM_FCOE_TASK_AG_CTX_RULE3EN_MASK    0x1	/* HSI_COMMENT: rule3en */
#define YSTORM_FCOE_TASK_AG_CTX_RULE3EN_SHIFT   4
#define YSTORM_FCOE_TASK_AG_CTX_RULE4EN_MASK    0x1	/* HSI_COMMENT: rule4en */
#define YSTORM_FCOE_TASK_AG_CTX_RULE4EN_SHIFT   5
#define YSTORM_FCOE_TASK_AG_CTX_RULE5EN_MASK    0x1	/* HSI_COMMENT: rule5en */
#define YSTORM_FCOE_TASK_AG_CTX_RULE5EN_SHIFT   6
#define YSTORM_FCOE_TASK_AG_CTX_RULE6EN_MASK    0x1	/* HSI_COMMENT: rule6en */
#define YSTORM_FCOE_TASK_AG_CTX_RULE6EN_SHIFT   7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	__le16 rx_id;		/* HSI_COMMENT: word1 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le16 word4;		/* HSI_COMMENT: word4 */
	__le16 word5;		/* HSI_COMMENT: word5 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
};

struct tstorm_fcoe_task_ag_ctx {
	u8 reserved;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	__le16 icid;		/* HSI_COMMENT: icid */
	u8 flags0;
#define TSTORM_FCOE_TASK_AG_CTX_CONNECTION_TYPE_MASK    0xF	/* HSI_COMMENT: connection_type */
#define TSTORM_FCOE_TASK_AG_CTX_CONNECTION_TYPE_SHIFT   0
#define TSTORM_FCOE_TASK_AG_CTX_EXIST_IN_QM0_MASK       0x1	/* HSI_COMMENT: exist_in_qm0 */
#define TSTORM_FCOE_TASK_AG_CTX_EXIST_IN_QM0_SHIFT      4
#define TSTORM_FCOE_TASK_AG_CTX_BIT1_MASK       0x1	/* HSI_COMMENT: exist_in_qm1 */
#define TSTORM_FCOE_TASK_AG_CTX_BIT1_SHIFT      5
#define TSTORM_FCOE_TASK_AG_CTX_WAIT_ABTS_RSP_F_MASK    0x1	/* HSI_COMMENT: bit2 */
#define TSTORM_FCOE_TASK_AG_CTX_WAIT_ABTS_RSP_F_SHIFT   6
#define TSTORM_FCOE_TASK_AG_CTX_VALID_MASK      0x1	/* HSI_COMMENT: bit3 */
#define TSTORM_FCOE_TASK_AG_CTX_VALID_SHIFT     7
	u8 flags1;
#define TSTORM_FCOE_TASK_AG_CTX_FALSE_RR_TOV_MASK       0x1	/* HSI_COMMENT: bit4 */
#define TSTORM_FCOE_TASK_AG_CTX_FALSE_RR_TOV_SHIFT      0
#define TSTORM_FCOE_TASK_AG_CTX_BIT5_MASK       0x1	/* HSI_COMMENT: bit5 */
#define TSTORM_FCOE_TASK_AG_CTX_BIT5_SHIFT      1
#define TSTORM_FCOE_TASK_AG_CTX_REC_RR_TOV_CF_MASK      0x3	/* HSI_COMMENT: timer0cf */
#define TSTORM_FCOE_TASK_AG_CTX_REC_RR_TOV_CF_SHIFT     2
#define TSTORM_FCOE_TASK_AG_CTX_ED_TOV_CF_MASK  0x3	/* HSI_COMMENT: timer1cf */
#define TSTORM_FCOE_TASK_AG_CTX_ED_TOV_CF_SHIFT 4
#define TSTORM_FCOE_TASK_AG_CTX_CF2_MASK        0x3	/* HSI_COMMENT: timer2cf */
#define TSTORM_FCOE_TASK_AG_CTX_CF2_SHIFT       6
	u8 flags2;
#define TSTORM_FCOE_TASK_AG_CTX_TIMER_STOP_ALL_MASK     0x3	/* HSI_COMMENT: timer_stop_all */
#define TSTORM_FCOE_TASK_AG_CTX_TIMER_STOP_ALL_SHIFT    0
#define TSTORM_FCOE_TASK_AG_CTX_EX_CLEANUP_CF_MASK      0x3	/* HSI_COMMENT: cf4 */
#define TSTORM_FCOE_TASK_AG_CTX_EX_CLEANUP_CF_SHIFT     2
#define TSTORM_FCOE_TASK_AG_CTX_SEQ_INIT_CF_MASK        0x3	/* HSI_COMMENT: cf5 */
#define TSTORM_FCOE_TASK_AG_CTX_SEQ_INIT_CF_SHIFT       4
#define TSTORM_FCOE_TASK_AG_CTX_SEQ_RECOVERY_CF_MASK    0x3	/* HSI_COMMENT: cf6 */
#define TSTORM_FCOE_TASK_AG_CTX_SEQ_RECOVERY_CF_SHIFT   6
	u8 flags3;
#define TSTORM_FCOE_TASK_AG_CTX_UNSOL_COMP_CF_MASK      0x3	/* HSI_COMMENT: cf7 */
#define TSTORM_FCOE_TASK_AG_CTX_UNSOL_COMP_CF_SHIFT     0
#define TSTORM_FCOE_TASK_AG_CTX_REC_RR_TOV_CF_EN_MASK   0x1	/* HSI_COMMENT: cf0en */
#define TSTORM_FCOE_TASK_AG_CTX_REC_RR_TOV_CF_EN_SHIFT  2
#define TSTORM_FCOE_TASK_AG_CTX_ED_TOV_CF_EN_MASK       0x1	/* HSI_COMMENT: cf1en */
#define TSTORM_FCOE_TASK_AG_CTX_ED_TOV_CF_EN_SHIFT      3
#define TSTORM_FCOE_TASK_AG_CTX_CF2EN_MASK      0x1	/* HSI_COMMENT: cf2en */
#define TSTORM_FCOE_TASK_AG_CTX_CF2EN_SHIFT     4
#define TSTORM_FCOE_TASK_AG_CTX_TIMER_STOP_ALL_EN_MASK  0x1	/* HSI_COMMENT: cf3en */
#define TSTORM_FCOE_TASK_AG_CTX_TIMER_STOP_ALL_EN_SHIFT 5
#define TSTORM_FCOE_TASK_AG_CTX_EX_CLEANUP_CF_EN_MASK   0x1	/* HSI_COMMENT: cf4en */
#define TSTORM_FCOE_TASK_AG_CTX_EX_CLEANUP_CF_EN_SHIFT  6
#define TSTORM_FCOE_TASK_AG_CTX_SEQ_INIT_CF_EN_MASK     0x1	/* HSI_COMMENT: cf5en */
#define TSTORM_FCOE_TASK_AG_CTX_SEQ_INIT_CF_EN_SHIFT    7
	u8 flags4;
#define TSTORM_FCOE_TASK_AG_CTX_SEQ_RECOVERY_CF_EN_MASK 0x1	/* HSI_COMMENT: cf6en */
#define TSTORM_FCOE_TASK_AG_CTX_SEQ_RECOVERY_CF_EN_SHIFT        0
#define TSTORM_FCOE_TASK_AG_CTX_UNSOL_COMP_CF_EN_MASK   0x1	/* HSI_COMMENT: cf7en */
#define TSTORM_FCOE_TASK_AG_CTX_UNSOL_COMP_CF_EN_SHIFT  1
#define TSTORM_FCOE_TASK_AG_CTX_RULE0EN_MASK    0x1	/* HSI_COMMENT: rule0en */
#define TSTORM_FCOE_TASK_AG_CTX_RULE0EN_SHIFT   2
#define TSTORM_FCOE_TASK_AG_CTX_RULE1EN_MASK    0x1	/* HSI_COMMENT: rule1en */
#define TSTORM_FCOE_TASK_AG_CTX_RULE1EN_SHIFT   3
#define TSTORM_FCOE_TASK_AG_CTX_RULE2EN_MASK    0x1	/* HSI_COMMENT: rule2en */
#define TSTORM_FCOE_TASK_AG_CTX_RULE2EN_SHIFT   4
#define TSTORM_FCOE_TASK_AG_CTX_RULE3EN_MASK    0x1	/* HSI_COMMENT: rule3en */
#define TSTORM_FCOE_TASK_AG_CTX_RULE3EN_SHIFT   5
#define TSTORM_FCOE_TASK_AG_CTX_RULE4EN_MASK    0x1	/* HSI_COMMENT: rule4en */
#define TSTORM_FCOE_TASK_AG_CTX_RULE4EN_SHIFT   6
#define TSTORM_FCOE_TASK_AG_CTX_RULE5EN_MASK    0x1	/* HSI_COMMENT: rule5en */
#define TSTORM_FCOE_TASK_AG_CTX_RULE5EN_SHIFT   7
	u8 cleanup_state;	/* HSI_COMMENT: byte2 */
	__le16 last_sent_tid;	/* HSI_COMMENT: word1 */
	__le32 rec_rr_tov_exp_timeout;	/* HSI_COMMENT: reg0 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le16 word4;		/* HSI_COMMENT: word4 */
	__le32 data_offset_end_of_seq;	/* HSI_COMMENT: reg1 */
	__le32 data_offset_next;	/* HSI_COMMENT: reg2 */
};

/* FW read- write (modifyable) part The fcoe task storm context of Tstorm */
struct fcoe_tstorm_fcoe_task_st_ctx_read_write {
	union fcoe_cleanup_addr_exp_ro_union cleanup_addr_exp_ro_union;	/* HSI_COMMENT: Union of Cleanup address / expected relative offsets */
	__le16 flags;
#define FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_RX_SGL_MODE_MASK        0x1	/* HSI_COMMENT: Rx SGL type. use enum scsi_sgl_mode  (use enum scsi_sgl_mode) */
#define FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_RX_SGL_MODE_SHIFT       0
#define FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_EXP_FIRST_FRAME_MASK    0x1	/* HSI_COMMENT: Expected first frame flag */
#define FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_EXP_FIRST_FRAME_SHIFT   1
#define FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_SEQ_ACTIVE_MASK 0x1	/* HSI_COMMENT: Sequence active */
#define FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_SEQ_ACTIVE_SHIFT        2
#define FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_SEQ_TIMEOUT_MASK        0x1	/* HSI_COMMENT: Sequence timeout for an active Sequence */
#define FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_SEQ_TIMEOUT_SHIFT       3
#define FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_SINGLE_PKT_IN_EX_MASK   0x1	/* HSI_COMMENT: Set by Data-in flow. Indicate that this exchange contains a single FCP DATA packet */
#define FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_SINGLE_PKT_IN_EX_SHIFT  4
#define FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_OOO_RX_SEQ_STAT_MASK    0x1	/* HSI_COMMENT: The status of the current out of order received Sequence */
#define FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_OOO_RX_SEQ_STAT_SHIFT   5
#define FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_CQ_ADD_ADV_MASK 0x3	/* HSI_COMMENT: number of additional CQE that will be produced for this task completion */
#define FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_CQ_ADD_ADV_SHIFT        6
#define FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_RSRV1_MASK      0xFF
#define FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_RSRV1_SHIFT     8
	__le16 seq_cnt;		/* HSI_COMMENT: Sequence counter */
	u8 seq_id;		/* HSI_COMMENT: Sequence id */
	u8 ooo_rx_seq_id;	/* HSI_COMMENT: The last out of order received SEQ_ID */
	__le16 rx_id;		/* HSI_COMMENT: RX_ID of the exchange - should match each packet expect for the first */
	struct fcoe_abts_pkt abts_data;	/* HSI_COMMENT: The last out of order received SEQ_CNT */
	__le32 e_d_tov_exp_timeout_val;	/* HSI_COMMENT: E_D_TOV timer val (in msec) */
	__le16 ooo_rx_seq_cnt;	/* HSI_COMMENT: The last out of order received SEQ_CNT */
	__le16 reserved1;
};

/* FW read only part The fcoe task storm context of Tstorm */
struct fcoe_tstorm_fcoe_task_st_ctx_read_only {
	u8 task_type;		/* HSI_COMMENT: Task type. use enum fcoe_task_type (use enum fcoe_task_type) */
	u8 dev_type;		/* HSI_COMMENT: Device type (disk or tape). use enum fcoe_device_type (use enum fcoe_device_type) */
	u8 conf_supported;	/* HSI_COMMENT: Confirmation supported indication */
	u8 glbl_q_num;		/* HSI_COMMENT: Global RQ/CQ num to be used for sense data placement/completion */
	__le32 cid;		/* HSI_COMMENT: CID which that tasks associated to */
	__le32 fcp_cmd_trns_size;	/* HSI_COMMENT: IO size as reflected in FCP CMD */
	__le32 rsrv;
};

/* The fcoe task storm context of Tstorm */
struct tstorm_fcoe_task_st_ctx {
	struct fcoe_tstorm_fcoe_task_st_ctx_read_write read_write;	/* HSI_COMMENT: FW read- write (modifyable) part The fcoe task storm context of Tstorm */
	struct fcoe_tstorm_fcoe_task_st_ctx_read_only read_only;	/* HSI_COMMENT: FW read only part The fcoe task storm context of Tstorm */
};

struct mstorm_fcoe_task_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	__le16 icid;		/* HSI_COMMENT: icid */
	u8 flags0;
#define MSTORM_FCOE_TASK_AG_CTX_CONNECTION_TYPE_MASK    0xF	/* HSI_COMMENT: connection_type */
#define MSTORM_FCOE_TASK_AG_CTX_CONNECTION_TYPE_SHIFT   0
#define MSTORM_FCOE_TASK_AG_CTX_EXIST_IN_QM0_MASK       0x1	/* HSI_COMMENT: exist_in_qm0 */
#define MSTORM_FCOE_TASK_AG_CTX_EXIST_IN_QM0_SHIFT      4
#define MSTORM_FCOE_TASK_AG_CTX_CQE_PLACED_MASK 0x1	/* HSI_COMMENT: exist_in_qm1 */
#define MSTORM_FCOE_TASK_AG_CTX_CQE_PLACED_SHIFT        5
#define MSTORM_FCOE_TASK_AG_CTX_BIT2_MASK       0x1	/* HSI_COMMENT: bit2 */
#define MSTORM_FCOE_TASK_AG_CTX_BIT2_SHIFT      6
#define MSTORM_FCOE_TASK_AG_CTX_BIT3_MASK       0x1	/* HSI_COMMENT: bit3 */
#define MSTORM_FCOE_TASK_AG_CTX_BIT3_SHIFT      7
	u8 flags1;
#define MSTORM_FCOE_TASK_AG_CTX_EX_CLEANUP_CF_MASK      0x3	/* HSI_COMMENT: cf0 */
#define MSTORM_FCOE_TASK_AG_CTX_EX_CLEANUP_CF_SHIFT     0
#define MSTORM_FCOE_TASK_AG_CTX_CF1_MASK        0x3	/* HSI_COMMENT: cf1 */
#define MSTORM_FCOE_TASK_AG_CTX_CF1_SHIFT       2
#define MSTORM_FCOE_TASK_AG_CTX_CF2_MASK        0x3	/* HSI_COMMENT: cf2 */
#define MSTORM_FCOE_TASK_AG_CTX_CF2_SHIFT       4
#define MSTORM_FCOE_TASK_AG_CTX_EX_CLEANUP_CF_EN_MASK   0x1	/* HSI_COMMENT: cf0en */
#define MSTORM_FCOE_TASK_AG_CTX_EX_CLEANUP_CF_EN_SHIFT  6
#define MSTORM_FCOE_TASK_AG_CTX_CF1EN_MASK      0x1	/* HSI_COMMENT: cf1en */
#define MSTORM_FCOE_TASK_AG_CTX_CF1EN_SHIFT     7
	u8 flags2;
#define MSTORM_FCOE_TASK_AG_CTX_CF2EN_MASK      0x1	/* HSI_COMMENT: cf2en */
#define MSTORM_FCOE_TASK_AG_CTX_CF2EN_SHIFT     0
#define MSTORM_FCOE_TASK_AG_CTX_RULE0EN_MASK    0x1	/* HSI_COMMENT: rule0en */
#define MSTORM_FCOE_TASK_AG_CTX_RULE0EN_SHIFT   1
#define MSTORM_FCOE_TASK_AG_CTX_RULE1EN_MASK    0x1	/* HSI_COMMENT: rule1en */
#define MSTORM_FCOE_TASK_AG_CTX_RULE1EN_SHIFT   2
#define MSTORM_FCOE_TASK_AG_CTX_RULE2EN_MASK    0x1	/* HSI_COMMENT: rule2en */
#define MSTORM_FCOE_TASK_AG_CTX_RULE2EN_SHIFT   3
#define MSTORM_FCOE_TASK_AG_CTX_RULE3EN_MASK    0x1	/* HSI_COMMENT: rule3en */
#define MSTORM_FCOE_TASK_AG_CTX_RULE3EN_SHIFT   4
#define MSTORM_FCOE_TASK_AG_CTX_RULE4EN_MASK    0x1	/* HSI_COMMENT: rule4en */
#define MSTORM_FCOE_TASK_AG_CTX_RULE4EN_SHIFT   5
#define MSTORM_FCOE_TASK_AG_CTX_XFER_PLACEMENT_EN_MASK  0x1	/* HSI_COMMENT: rule5en */
#define MSTORM_FCOE_TASK_AG_CTX_XFER_PLACEMENT_EN_SHIFT 6
#define MSTORM_FCOE_TASK_AG_CTX_RULE6EN_MASK    0x1	/* HSI_COMMENT: rule6en */
#define MSTORM_FCOE_TASK_AG_CTX_RULE6EN_SHIFT   7
	u8 cleanup_state;	/* HSI_COMMENT: byte2 */
	__le32 received_bytes;	/* HSI_COMMENT: reg0 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	u8 glbl_q_num;		/* HSI_COMMENT: byte4 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le16 tid_to_xfer;	/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le16 word4;		/* HSI_COMMENT: word4 */
	__le16 word5;		/* HSI_COMMENT: word5 */
	__le32 expected_bytes;	/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
};

/* The fcoe task storm context of Mstorm */
struct mstorm_fcoe_task_st_ctx {
	struct regpair rsp_buf_addr;	/* HSI_COMMENT: Buffer to place the sense/response data attached to FCP_RSP frame */
	__le32 rsrv[2];
	struct scsi_sgl_params sgl_params;
	__le32 data_2_trns_rem;	/* HSI_COMMENT: Entire SGL buffer size remainder */
	__le32 data_buffer_offset;	/* HSI_COMMENT: Buffer offset */
	__le16 parent_id;	/* HSI_COMMENT: Used for multiple continuation in Target mode */
	__le16 flags;
#define MSTORM_FCOE_TASK_ST_CTX_INTERVAL_SIZE_LOG_MASK  0xF	/* HSI_COMMENT: Protection log interval (9=512 10=1024  11=2048 12=4096 13=8192) */
#define MSTORM_FCOE_TASK_ST_CTX_INTERVAL_SIZE_LOG_SHIFT 0
#define MSTORM_FCOE_TASK_ST_CTX_HOST_INTERFACE_MASK     0x3	/* HSI_COMMENT: 0=none, 1=DIF, 2=DIX */
#define MSTORM_FCOE_TASK_ST_CTX_HOST_INTERFACE_SHIFT    4
#define MSTORM_FCOE_TASK_ST_CTX_DIF_TO_PEER_MASK        0x1	/* HSI_COMMENT: 0=no, 1=yes */
#define MSTORM_FCOE_TASK_ST_CTX_DIF_TO_PEER_SHIFT       6
#define MSTORM_FCOE_TASK_ST_CTX_MP_INCLUDE_FC_HEADER_MASK       0x1	/* HSI_COMMENT: 0 = 24 Bytes FC Header not included in Middle-Path placement, 1 = 24 Bytes FC Header included in MP placement */
#define MSTORM_FCOE_TASK_ST_CTX_MP_INCLUDE_FC_HEADER_SHIFT      7
#define MSTORM_FCOE_TASK_ST_CTX_DIX_BLOCK_SIZE_MASK     0x3	/* HSI_COMMENT: DIX block size: can be 0:2B, 1:4B, 2:8B */
#define MSTORM_FCOE_TASK_ST_CTX_DIX_BLOCK_SIZE_SHIFT    8
#define MSTORM_FCOE_TASK_ST_CTX_VALIDATE_DIX_REF_TAG_MASK       0x1	/* HSI_COMMENT: 0=no, 1=yes */
#define MSTORM_FCOE_TASK_ST_CTX_VALIDATE_DIX_REF_TAG_SHIFT      10
#define MSTORM_FCOE_TASK_ST_CTX_DIX_CACHED_SGE_FLG_MASK 0x1	/* HSI_COMMENT: Indication to a single cached DIX SGE instead of SGL */
#define MSTORM_FCOE_TASK_ST_CTX_DIX_CACHED_SGE_FLG_SHIFT        11
#define MSTORM_FCOE_TASK_ST_CTX_DIF_SUPPORTED_MASK      0x1
#define MSTORM_FCOE_TASK_ST_CTX_DIF_SUPPORTED_SHIFT     12
#define MSTORM_FCOE_TASK_ST_CTX_TX_SGL_MODE_MASK        0x1	/* HSI_COMMENT: use_enum scsi_sgl_mode (use enum scsi_sgl_mode) */
#define MSTORM_FCOE_TASK_ST_CTX_TX_SGL_MODE_SHIFT       13
#define MSTORM_FCOE_TASK_ST_CTX_RESERVED_MASK   0x3
#define MSTORM_FCOE_TASK_ST_CTX_RESERVED_SHIFT  14
	struct scsi_cached_sges data_desc;	/* HSI_COMMENT: Union of Data SGL / cached sge */
};

struct ustorm_fcoe_task_ag_ctx {
	u8 reserved;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	__le16 icid;		/* HSI_COMMENT: icid */
	u8 flags0;
#define USTORM_FCOE_TASK_AG_CTX_CONNECTION_TYPE_MASK    0xF	/* HSI_COMMENT: connection_type */
#define USTORM_FCOE_TASK_AG_CTX_CONNECTION_TYPE_SHIFT   0
#define USTORM_FCOE_TASK_AG_CTX_EXIST_IN_QM0_MASK       0x1	/* HSI_COMMENT: exist_in_qm0 */
#define USTORM_FCOE_TASK_AG_CTX_EXIST_IN_QM0_SHIFT      4
#define USTORM_FCOE_TASK_AG_CTX_BIT1_MASK       0x1	/* HSI_COMMENT: exist_in_qm1 */
#define USTORM_FCOE_TASK_AG_CTX_BIT1_SHIFT      5
#define USTORM_FCOE_TASK_AG_CTX_CF0_MASK        0x3	/* HSI_COMMENT: timer0cf */
#define USTORM_FCOE_TASK_AG_CTX_CF0_SHIFT       6
	u8 flags1;
#define USTORM_FCOE_TASK_AG_CTX_CF1_MASK        0x3	/* HSI_COMMENT: timer1cf */
#define USTORM_FCOE_TASK_AG_CTX_CF1_SHIFT       0
#define USTORM_FCOE_TASK_AG_CTX_CF2_MASK        0x3	/* HSI_COMMENT: timer2cf */
#define USTORM_FCOE_TASK_AG_CTX_CF2_SHIFT       2
#define USTORM_FCOE_TASK_AG_CTX_CF3_MASK        0x3	/* HSI_COMMENT: timer_stop_all */
#define USTORM_FCOE_TASK_AG_CTX_CF3_SHIFT       4
#define USTORM_FCOE_TASK_AG_CTX_DIF_ERROR_CF_MASK       0x3	/* HSI_COMMENT: cf4 */
#define USTORM_FCOE_TASK_AG_CTX_DIF_ERROR_CF_SHIFT      6
	u8 flags2;
#define USTORM_FCOE_TASK_AG_CTX_CF0EN_MASK      0x1	/* HSI_COMMENT: cf0en */
#define USTORM_FCOE_TASK_AG_CTX_CF0EN_SHIFT     0
#define USTORM_FCOE_TASK_AG_CTX_CF1EN_MASK      0x1	/* HSI_COMMENT: cf1en */
#define USTORM_FCOE_TASK_AG_CTX_CF1EN_SHIFT     1
#define USTORM_FCOE_TASK_AG_CTX_CF2EN_MASK      0x1	/* HSI_COMMENT: cf2en */
#define USTORM_FCOE_TASK_AG_CTX_CF2EN_SHIFT     2
#define USTORM_FCOE_TASK_AG_CTX_CF3EN_MASK      0x1	/* HSI_COMMENT: cf3en */
#define USTORM_FCOE_TASK_AG_CTX_CF3EN_SHIFT     3
#define USTORM_FCOE_TASK_AG_CTX_DIF_ERROR_CF_EN_MASK    0x1	/* HSI_COMMENT: cf4en */
#define USTORM_FCOE_TASK_AG_CTX_DIF_ERROR_CF_EN_SHIFT   4
#define USTORM_FCOE_TASK_AG_CTX_RULE0EN_MASK    0x1	/* HSI_COMMENT: rule0en */
#define USTORM_FCOE_TASK_AG_CTX_RULE0EN_SHIFT   5
#define USTORM_FCOE_TASK_AG_CTX_RULE1EN_MASK    0x1	/* HSI_COMMENT: rule1en */
#define USTORM_FCOE_TASK_AG_CTX_RULE1EN_SHIFT   6
#define USTORM_FCOE_TASK_AG_CTX_RULE2EN_MASK    0x1	/* HSI_COMMENT: rule2en */
#define USTORM_FCOE_TASK_AG_CTX_RULE2EN_SHIFT   7
	u8 flags3;
#define USTORM_FCOE_TASK_AG_CTX_RULE3EN_MASK    0x1	/* HSI_COMMENT: rule3en */
#define USTORM_FCOE_TASK_AG_CTX_RULE3EN_SHIFT   0
#define USTORM_FCOE_TASK_AG_CTX_RULE4EN_MASK    0x1	/* HSI_COMMENT: rule4en */
#define USTORM_FCOE_TASK_AG_CTX_RULE4EN_SHIFT   1
#define USTORM_FCOE_TASK_AG_CTX_RULE5EN_MASK    0x1	/* HSI_COMMENT: rule5en */
#define USTORM_FCOE_TASK_AG_CTX_RULE5EN_SHIFT   2
#define USTORM_FCOE_TASK_AG_CTX_RULE6EN_MASK    0x1	/* HSI_COMMENT: rule6en */
#define USTORM_FCOE_TASK_AG_CTX_RULE6EN_SHIFT   3
#define USTORM_FCOE_TASK_AG_CTX_DIF_ERROR_TYPE_MASK     0xF	/* HSI_COMMENT: nibble1 */
#define USTORM_FCOE_TASK_AG_CTX_DIF_ERROR_TYPE_SHIFT    4
	__le32 dif_err_intervals;	/* HSI_COMMENT: reg0 */
	__le32 dif_error_1st_interval;	/* HSI_COMMENT: reg1 */
	__le32 global_cq_num;	/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
	__le32 reg5;		/* HSI_COMMENT: reg5 */
};

/* fcoe task context */
struct fcoe_task_context {
	struct ystorm_fcoe_task_st_ctx ystorm_st_context;	/* HSI_COMMENT: ystorm storm context */
	struct regpair ystorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct tdif_task_context tdif_context;	/* HSI_COMMENT: tdif context */
	struct ystorm_fcoe_task_ag_ctx ystorm_ag_context;	/* HSI_COMMENT: ystorm aggregative context */
	struct tstorm_fcoe_task_ag_ctx tstorm_ag_context;	/* HSI_COMMENT: tstorm aggregative context */
	struct timers_context timer_context;	/* HSI_COMMENT: timer context */
	struct tstorm_fcoe_task_st_ctx tstorm_st_context;	/* HSI_COMMENT: tstorm storm context */
	struct regpair tstorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct mstorm_fcoe_task_ag_ctx mstorm_ag_context;	/* HSI_COMMENT: mstorm aggregative context */
	struct mstorm_fcoe_task_st_ctx mstorm_st_context;	/* HSI_COMMENT: mstorm storm context */
	struct ustorm_fcoe_task_ag_ctx ustorm_ag_context;	/* HSI_COMMENT: ustorm aggregative context */
	struct rdif_task_context rdif_context;	/* HSI_COMMENT: rdif context */
};

/* FCoE task type */
enum fcoe_task_type {
	FCOE_TASK_TYPE_WRITE_INITIATOR,
	FCOE_TASK_TYPE_READ_INITIATOR,
	FCOE_TASK_TYPE_MIDPATH,
	FCOE_TASK_TYPE_UNSOLICITED,
	FCOE_TASK_TYPE_ABTS,
	FCOE_TASK_TYPE_EXCHANGE_CLEANUP,
	FCOE_TASK_TYPE_SEQUENCE_CLEANUP,
	FCOE_TASK_TYPE_WRITE_TARGET,
	FCOE_TASK_TYPE_READ_TARGET,
	FCOE_TASK_TYPE_RSP,
	FCOE_TASK_TYPE_RSP_SENSE_DATA,
	FCOE_TASK_TYPE_ABTS_TARGET,
	FCOE_TASK_TYPE_ENUM_SIZE,
	MAX_FCOE_TASK_TYPE
};

/* Per PF FCoE transmit path statistics - pStorm RAM structure */
struct fcoe_tx_stat {
	struct regpair fcoe_tx_byte_cnt;	/* HSI_COMMENT: Transmitted FCoE bytes count */
	struct regpair fcoe_tx_data_pkt_cnt;	/* HSI_COMMENT: Transmitted FCoE FCP DATA packets count */
	struct regpair fcoe_tx_xfer_pkt_cnt;	/* HSI_COMMENT: Transmitted FCoE XFER_RDY packets count */
	struct regpair fcoe_tx_other_pkt_cnt;	/* HSI_COMMENT: Transmitted FCoE packets which are not DATA/XFER_RDY count */
};

/* FCoE SQ/XferQ element  */
struct fcoe_wqe {
	__le16 task_id;		/* HSI_COMMENT: Initiator - The task identifier (OX_ID). Target - Continuation tid or RX_ID in non-continuation mode */
	__le16 flags;
#define FCOE_WQE_REQ_TYPE_MASK  0xF	/* HSI_COMMENT: Type of the wqe request. use enum fcoe_sqe_request_type  (use enum fcoe_sqe_request_type) */
#define FCOE_WQE_REQ_TYPE_SHIFT 0
#define FCOE_WQE_SGL_MODE_MASK  0x1	/* HSI_COMMENT: The driver will give a hint about sizes of SGEs for better credits evaluation at Xstorm. use enum scsi_sgl_mode (use enum scsi_sgl_mode) */
#define FCOE_WQE_SGL_MODE_SHIFT 4
#define FCOE_WQE_CONTINUATION_MASK      0x1	/* HSI_COMMENT: Indication if this wqe is a continuation to an existing task (Target only) */
#define FCOE_WQE_CONTINUATION_SHIFT     5
#define FCOE_WQE_SEND_AUTO_RSP_MASK     0x1	/* HSI_COMMENT: Indication to FW to send FCP_RSP after all data was sent - Target only */
#define FCOE_WQE_SEND_AUTO_RSP_SHIFT    6
#define FCOE_WQE_RESERVED_MASK  0x1
#define FCOE_WQE_RESERVED_SHIFT 7
#define FCOE_WQE_NUM_SGES_MASK  0xF	/* HSI_COMMENT: Number of SGEs. 8 = at least 8 sges */
#define FCOE_WQE_NUM_SGES_SHIFT 8
#define FCOE_WQE_RESERVED1_MASK 0xF
#define FCOE_WQE_RESERVED1_SHIFT        12
	union fcoe_additional_info_union additional_info_union;	/* HSI_COMMENT: Additional wqe information (if needed) */
};

/* FCoE XFRQ element  */
struct xfrqe_prot_flags {
	u8 flags;
#define XFRQE_PROT_FLAGS_PROT_INTERVAL_SIZE_LOG_MASK    0xF	/* HSI_COMMENT: Protection log interval (9=512 10=1024  11=2048 12=4096 13=8192) */
#define XFRQE_PROT_FLAGS_PROT_INTERVAL_SIZE_LOG_SHIFT   0
#define XFRQE_PROT_FLAGS_DIF_TO_PEER_MASK       0x1	/* HSI_COMMENT: If DIF protection is configured against target (0=no, 1=yes) */
#define XFRQE_PROT_FLAGS_DIF_TO_PEER_SHIFT      4
#define XFRQE_PROT_FLAGS_HOST_INTERFACE_MASK    0x3	/* HSI_COMMENT: If DIF/DIX protection is configured against the host (0=none, 1=DIF, 2=DIX) */
#define XFRQE_PROT_FLAGS_HOST_INTERFACE_SHIFT   5
#define XFRQE_PROT_FLAGS_RESERVED_MASK  0x1	/* HSI_COMMENT: Must set to 0 */
#define XFRQE_PROT_FLAGS_RESERVED_SHIFT 7
};

/* FCoE doorbell data */
struct fcoe_db_data {
	u8 params;
#define FCOE_DB_DATA_DEST_MASK  0x3	/* HSI_COMMENT: destination of doorbell (use enum db_dest) */
#define FCOE_DB_DATA_DEST_SHIFT 0
#define FCOE_DB_DATA_AGG_CMD_MASK       0x3	/* HSI_COMMENT: aggregative command to CM (use enum db_agg_cmd_sel) */
#define FCOE_DB_DATA_AGG_CMD_SHIFT      2
#define FCOE_DB_DATA_BYPASS_EN_MASK     0x1	/* HSI_COMMENT: enable QM bypass */
#define FCOE_DB_DATA_BYPASS_EN_SHIFT    4
#define FCOE_DB_DATA_RESERVED_MASK      0x1
#define FCOE_DB_DATA_RESERVED_SHIFT     5
#define FCOE_DB_DATA_AGG_VAL_SEL_MASK   0x3	/* HSI_COMMENT: aggregative value selection */
#define FCOE_DB_DATA_AGG_VAL_SEL_SHIFT  6
	u8 agg_flags;		/* HSI_COMMENT: bit for every DQ counter flags in CM context that DQ can increment */
	__le16 sq_prod;
};

#endif /* __FCOE_COMMON__ */
