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

#ifndef __ISCSI_COMMON__
#define __ISCSI_COMMON__
/**********************/
/* ISCSI FW CONSTANTS */
/**********************/

/* iSCSI HSI constants */
#define ISCSI_DEFAULT_MTU       (1500)

/* KWQ (kernel work queue) layer codes */
#define ISCSI_SLOW_PATH_LAYER_CODE   (6)

/* iSCSI parameter defaults */
#define ISCSI_DEFAULT_HEADER_DIGEST         (0)
#define ISCSI_DEFAULT_DATA_DIGEST           (0)
#define ISCSI_DEFAULT_INITIAL_R2T           (1)
#define ISCSI_DEFAULT_IMMEDIATE_DATA        (1)
#define ISCSI_DEFAULT_MAX_PDU_LENGTH        (0x2000)
#define ISCSI_DEFAULT_FIRST_BURST_LENGTH    (0x10000)
#define ISCSI_DEFAULT_MAX_BURST_LENGTH      (0x40000)
#define ISCSI_DEFAULT_MAX_OUTSTANDING_R2T   (1)

/* iSCSI parameter limits */
#define ISCSI_MIN_VAL_MAX_PDU_LENGTH        (0x200)
#define ISCSI_MAX_VAL_MAX_PDU_LENGTH        (0xffffff)
#define ISCSI_MIN_VAL_BURST_LENGTH          (0x200)
#define ISCSI_MAX_VAL_BURST_LENGTH          (0xffffff)
#define ISCSI_MIN_VAL_MAX_OUTSTANDING_R2T   (1)
#define ISCSI_MAX_VAL_MAX_OUTSTANDING_R2T   (0xff)	// 0x10000 according to RFC

#define ISCSI_AHS_CNTL_SIZE 4

#define ISCSI_WQE_NUM_SGES_SLOWIO           (0xf)

/* iSCSI reserved params */
#define ISCSI_ITT_ALL_ONES                                      (0xffffffff)
#define ISCSI_TTT_ALL_ONES                                      (0xffffffff)

#define ISCSI_OPTION_1_OFF_CHIP_TCP 1
#define ISCSI_OPTION_2_ON_CHIP_TCP 2

#define ISCSI_INITIATOR_MODE 0
#define ISCSI_TARGET_MODE 1

/* iSCSI request op codes */
#define ISCSI_OPCODE_NOP_OUT                            (0)
#define ISCSI_OPCODE_SCSI_CMD                   (1)
#define ISCSI_OPCODE_TMF_REQUEST                        (2)
#define ISCSI_OPCODE_LOGIN_REQUEST                      (3)
#define ISCSI_OPCODE_TEXT_REQUEST               (4)
#define ISCSI_OPCODE_DATA_OUT                           (5)
#define ISCSI_OPCODE_LOGOUT_REQUEST             (6)

/* iSCSI response/messages op codes */
#define ISCSI_OPCODE_NOP_IN             (0x20)
#define ISCSI_OPCODE_SCSI_RESPONSE      (0x21)
#define ISCSI_OPCODE_TMF_RESPONSE       (0x22)
#define ISCSI_OPCODE_LOGIN_RESPONSE     (0x23)
#define ISCSI_OPCODE_TEXT_RESPONSE      (0x24)
#define ISCSI_OPCODE_DATA_IN            (0x25)
#define ISCSI_OPCODE_LOGOUT_RESPONSE    (0x26)
#define ISCSI_OPCODE_R2T                (0x31)
#define ISCSI_OPCODE_ASYNC_MSG          (0x32)
#define ISCSI_OPCODE_REJECT             (0x3f)

/* iSCSI stages */
#define ISCSI_STAGE_SECURITY_NEGOTIATION            (0)
#define ISCSI_STAGE_LOGIN_OPERATIONAL_NEGOTIATION   (1)
#define ISCSI_STAGE_FULL_FEATURE_PHASE              (3)

/* iSCSI CQE errors */
#define CQE_ERROR_BITMAP_DATA_DIGEST          (0x08)
#define CQE_ERROR_BITMAP_RCV_ON_INVALID_CONN  (0x10)
#define CQE_ERROR_BITMAP_DATA_TRUNCATED       (0x20)

/* Union of data bd_opaque/ tq_tid */
union bd_opaque_tq_union {
	__le16 bd_opaque;	/* HSI_COMMENT: BDs opaque data */
	__le16 tq_tid;		/* HSI_COMMENT: Immediate Data with DIF TQe TID */
};

/* ISCSI SGL entry */
struct cqe_error_bitmap {
	u8 cqe_error_status_bits;
#define CQE_ERROR_BITMAP_DIF_ERR_BITS_MASK      0x7	/* HSI_COMMENT: Mark task with DIF error (3 bit): [0]-CRC/checksum, [1]-app tag, [2]-reference tag */
#define CQE_ERROR_BITMAP_DIF_ERR_BITS_SHIFT     0
#define CQE_ERROR_BITMAP_DATA_DIGEST_ERR_MASK   0x1	/* HSI_COMMENT: Mark task with data digest error (1 bit) */
#define CQE_ERROR_BITMAP_DATA_DIGEST_ERR_SHIFT  3
#define CQE_ERROR_BITMAP_RCV_ON_INVALID_CONN_MASK       0x1	/* HSI_COMMENT: Mark receive on invalid connection */
#define CQE_ERROR_BITMAP_RCV_ON_INVALID_CONN_SHIFT      4
#define CQE_ERROR_BITMAP_DATA_TRUNCATED_ERR_MASK        0x1	/* HSI_COMMENT: Target Mode - Mark middle task error, data truncated */
#define CQE_ERROR_BITMAP_DATA_TRUNCATED_ERR_SHIFT       5
#define CQE_ERROR_BITMAP_UNDER_RUN_ERR_MASK     0x1
#define CQE_ERROR_BITMAP_UNDER_RUN_ERR_SHIFT    6
#define CQE_ERROR_BITMAP_RESERVED2_MASK 0x1
#define CQE_ERROR_BITMAP_RESERVED2_SHIFT        7
};

union cqe_error_status {
	u8 error_status;	/* HSI_COMMENT: all error bits as uint8 */
	struct cqe_error_bitmap error_bits;	/* HSI_COMMENT: cqe errors bitmap */
};

/* iSCSI Login Response PDU header */
struct data_hdr {
	__le32 data[12];	/* HSI_COMMENT: iscsi header data */
};

struct lun_mapper_addr_reserved {
	struct regpair lun_mapper_addr;	/* HSI_COMMENT: Lun mapper address */
	u8 reserved0[8];
};

/* rdif conetxt for dif on immediate */
struct dif_on_immediate_params {
	__le32 initial_ref_tag;
	__le16 application_tag;
	__le16 application_tag_mask;
	__le16 flags1;
#define DIF_ON_IMMEDIATE_PARAMS_VALIDATE_GUARD_MASK     0x1
#define DIF_ON_IMMEDIATE_PARAMS_VALIDATE_GUARD_SHIFT    0
#define DIF_ON_IMMEDIATE_PARAMS_VALIDATE_APP_TAG_MASK   0x1
#define DIF_ON_IMMEDIATE_PARAMS_VALIDATE_APP_TAG_SHIFT  1
#define DIF_ON_IMMEDIATE_PARAMS_VALIDATE_REF_TAG_MASK   0x1
#define DIF_ON_IMMEDIATE_PARAMS_VALIDATE_REF_TAG_SHIFT  2
#define DIF_ON_IMMEDIATE_PARAMS_FORWARD_GUARD_MASK      0x1
#define DIF_ON_IMMEDIATE_PARAMS_FORWARD_GUARD_SHIFT     3
#define DIF_ON_IMMEDIATE_PARAMS_FORWARD_APP_TAG_MASK    0x1
#define DIF_ON_IMMEDIATE_PARAMS_FORWARD_APP_TAG_SHIFT   4
#define DIF_ON_IMMEDIATE_PARAMS_FORWARD_REF_TAG_MASK    0x1
#define DIF_ON_IMMEDIATE_PARAMS_FORWARD_REF_TAG_SHIFT   5
#define DIF_ON_IMMEDIATE_PARAMS_INTERVAL_SIZE_MASK      0x1	/* HSI_COMMENT: 0=512B, 1=4KB */
#define DIF_ON_IMMEDIATE_PARAMS_INTERVAL_SIZE_SHIFT     6
#define DIF_ON_IMMEDIATE_PARAMS_NETWORK_INTERFACE_MASK  0x1	/* HSI_COMMENT: 0=None, 1=DIF */
#define DIF_ON_IMMEDIATE_PARAMS_NETWORK_INTERFACE_SHIFT 7
#define DIF_ON_IMMEDIATE_PARAMS_HOST_INTERFACE_MASK     0x3	/* HSI_COMMENT: 0=None, 1=DIF, 2=DIX */
#define DIF_ON_IMMEDIATE_PARAMS_HOST_INTERFACE_SHIFT    8
#define DIF_ON_IMMEDIATE_PARAMS_REF_TAG_MASK_MASK       0xF	/* HSI_COMMENT: mask for refernce tag handling */
#define DIF_ON_IMMEDIATE_PARAMS_REF_TAG_MASK_SHIFT      10
#define DIF_ON_IMMEDIATE_PARAMS_FORWARD_APP_TAG_WITH_MASK_MASK  0x1	/* HSI_COMMENT: Forward application tag with mask */
#define DIF_ON_IMMEDIATE_PARAMS_FORWARD_APP_TAG_WITH_MASK_SHIFT 14
#define DIF_ON_IMMEDIATE_PARAMS_FORWARD_REF_TAG_WITH_MASK_MASK  0x1	/* HSI_COMMENT: Forward reference tag with mask */
#define DIF_ON_IMMEDIATE_PARAMS_FORWARD_REF_TAG_WITH_MASK_SHIFT 15
	u8 flags0;
#define DIF_ON_IMMEDIATE_PARAMS_RESERVED_MASK   0x1
#define DIF_ON_IMMEDIATE_PARAMS_RESERVED_SHIFT  0
#define DIF_ON_IMMEDIATE_PARAMS_IGNORE_APP_TAG_MASK     0x1
#define DIF_ON_IMMEDIATE_PARAMS_IGNORE_APP_TAG_SHIFT    1
#define DIF_ON_IMMEDIATE_PARAMS_INITIAL_REF_TAG_IS_VALID_MASK   0x1
#define DIF_ON_IMMEDIATE_PARAMS_INITIAL_REF_TAG_IS_VALID_SHIFT  2
#define DIF_ON_IMMEDIATE_PARAMS_HOST_GUARD_TYPE_MASK    0x1	/* HSI_COMMENT: 0 = IP checksum, 1 = CRC */
#define DIF_ON_IMMEDIATE_PARAMS_HOST_GUARD_TYPE_SHIFT   3
#define DIF_ON_IMMEDIATE_PARAMS_PROTECTION_TYPE_MASK    0x3	/* HSI_COMMENT: 1/2/3 - Protection Type */
#define DIF_ON_IMMEDIATE_PARAMS_PROTECTION_TYPE_SHIFT   4
#define DIF_ON_IMMEDIATE_PARAMS_CRC_SEED_MASK   0x1	/* HSI_COMMENT: 0=0x0000, 1=0xffff */
#define DIF_ON_IMMEDIATE_PARAMS_CRC_SEED_SHIFT  6
#define DIF_ON_IMMEDIATE_PARAMS_KEEP_REF_TAG_CONST_MASK 0x1	/* HSI_COMMENT: Keep reference tag constant */
#define DIF_ON_IMMEDIATE_PARAMS_KEEP_REF_TAG_CONST_SHIFT        7
	u8 reserved_zero[5];
};

/* iSCSI dif on immediate mode attributes union */
union dif_configuration_params {
	struct lun_mapper_addr_reserved lun_mapper_address;	/* HSI_COMMENT: lun mapper address */
	struct dif_on_immediate_params def_dif_conf;	/* HSI_COMMENT: default dif on immediate rdif configuration */
};

union xstorm_iscsi_asserts {
	__le16 all_access_bits;
};

union ystorm_iscsi_asserts {
	__le16 all_access_bits;
};

union pstorm_iscsi_asserts {
	__le16 all_access_bits;
};

/* T iSCSI Assert Codes */
struct tiscsi_assert_codes {
	__le16 flags;
#define TISCSI_ASSERT_CODES_OOO_TRIM_ERR_MASK   0x1
#define TISCSI_ASSERT_CODES_OOO_TRIM_ERR_SHIFT  0
#define TISCSI_ASSERT_CODES_FIN_LOOPBACK_ERR_MASK       0x1
#define TISCSI_ASSERT_CODES_FIN_LOOPBACK_ERR_SHIFT      1
#define TISCSI_ASSERT_CODES_RST_LOOPBACK_ERR_MASK       0x1
#define TISCSI_ASSERT_CODES_RST_LOOPBACK_ERR_SHIFT      2
#define TISCSI_ASSERT_CODES_FLUSH_OP_ERR_MASK   0x1
#define TISCSI_ASSERT_CODES_FLUSH_OP_ERR_SHIFT  3
#define TISCSI_ASSERT_CODES_UNUSED_MASK 0xFFF
#define TISCSI_ASSERT_CODES_UNUSED_SHIFT        4
};

union tstorm_iscsi_asserts {
	struct tiscsi_assert_codes assert_type;
	__le16 all_access_bits;
};

/* M iSCSI Assert Codes */
struct miscsi_assert_codes {
	__le16 flags;
#define MISCSI_ASSERT_CODES_EVENTID_MISMATCH_ERR_MASK   0x1
#define MISCSI_ASSERT_CODES_EVENTID_MISMATCH_ERR_SHIFT  0
#define MISCSI_ASSERT_CODES_UNUSED_MASK 0x7FFF
#define MISCSI_ASSERT_CODES_UNUSED_SHIFT        1
};

union mstorm_iscsi_asserts {
	struct miscsi_assert_codes assert_type;
	__le16 all_access_bits;
};

union ustorm_iscsi_asserts {
	__le16 all_access_bits;
};

/* iSCSI Assert Codes for each Storm */
struct iscsi_asserts_types {
	union xstorm_iscsi_asserts xstorm_asserts;	/* HSI_COMMENT: X Storm iSCSI assert codes */
	union ystorm_iscsi_asserts ystorm_asserts;	/* HSI_COMMENT: Y Storm iSCSI assert codes */
	union pstorm_iscsi_asserts pstorm_asserts;	/* HSI_COMMENT: P Storm iSCSI assert codes */
	union tstorm_iscsi_asserts tstorm_asserts;	/* HSI_COMMENT: T Storm iSCSI assert codes */
	union mstorm_iscsi_asserts mstorm_asserts;	/* HSI_COMMENT: M Storm iSCSI assert codes */
	union ustorm_iscsi_asserts ustorm_asserts;	/* HSI_COMMENT: U Storm iSCSI assert codes */
};

/* iSCSI Asynchronous Message PDU header */
struct iscsi_async_msg_hdr {
	__le16 reserved0;	/* HSI_COMMENT: reserved */
	u8 flags_attr;
#define ISCSI_ASYNC_MSG_HDR_RSRV_MASK   0x7F	/* HSI_COMMENT: reserved */
#define ISCSI_ASYNC_MSG_HDR_RSRV_SHIFT  0
#define ISCSI_ASYNC_MSG_HDR_CONST1_MASK 0x1	/* HSI_COMMENT: const1 */
#define ISCSI_ASYNC_MSG_HDR_CONST1_SHIFT        7
	u8 opcode;		/* HSI_COMMENT: opcode */
	__le32 hdr_second_dword;
#define ISCSI_ASYNC_MSG_HDR_DATA_SEG_LEN_MASK   0xFFFFFF	/* HSI_COMMENT: DataSegmentLength */
#define ISCSI_ASYNC_MSG_HDR_DATA_SEG_LEN_SHIFT  0
#define ISCSI_ASYNC_MSG_HDR_TOTAL_AHS_LEN_MASK  0xFF	/* HSI_COMMENT: TotalAHSLength */
#define ISCSI_ASYNC_MSG_HDR_TOTAL_AHS_LEN_SHIFT 24
	struct regpair lun;	/* HSI_COMMENT: Logical Unit Number */
	__le32 all_ones;	/* HSI_COMMENT: should be 0xffffffff */
	__le32 reserved1;	/* HSI_COMMENT: reserved */
	__le32 stat_sn;		/* HSI_COMMENT: stat_sn */
	__le32 exp_cmd_sn;	/* HSI_COMMENT: exp_cmd_sn */
	__le32 max_cmd_sn;	/* HSI_COMMENT: max_cmd_sn */
	__le16 param1_rsrv;	/* HSI_COMMENT: Parameter1 or Reserved */
	u8 async_vcode;		/* HSI_COMMENT: AsuncVCode */
	u8 async_event;		/* HSI_COMMENT: AsyncEvent */
	__le16 param3_rsrv;	/* HSI_COMMENT: Parameter3 or Reserved */
	__le16 param2_rsrv;	/* HSI_COMMENT: Parameter2 or Reserved */
	__le32 reserved7;	/* HSI_COMMENT: reserved */
};

/* iSCSI Command PDU header */
struct iscsi_cmd_hdr {
	__le16 reserved1;	/* HSI_COMMENT: reserved */
	u8 flags_attr;
#define ISCSI_CMD_HDR_ATTR_MASK 0x7	/* HSI_COMMENT: attributes */
#define ISCSI_CMD_HDR_ATTR_SHIFT        0
#define ISCSI_CMD_HDR_RSRV_MASK 0x3	/* HSI_COMMENT: reserved */
#define ISCSI_CMD_HDR_RSRV_SHIFT        3
#define ISCSI_CMD_HDR_WRITE_MASK        0x1	/* HSI_COMMENT: write */
#define ISCSI_CMD_HDR_WRITE_SHIFT       5
#define ISCSI_CMD_HDR_READ_MASK 0x1	/* HSI_COMMENT: read */
#define ISCSI_CMD_HDR_READ_SHIFT        6
#define ISCSI_CMD_HDR_FINAL_MASK        0x1	/* HSI_COMMENT: final */
#define ISCSI_CMD_HDR_FINAL_SHIFT       7
	u8 hdr_first_byte;
#define ISCSI_CMD_HDR_OPCODE_MASK       0x3F	/* HSI_COMMENT: Opcode */
#define ISCSI_CMD_HDR_OPCODE_SHIFT      0
#define ISCSI_CMD_HDR_IMM_MASK  0x1	/* HSI_COMMENT: Immediate delivery */
#define ISCSI_CMD_HDR_IMM_SHIFT 6
#define ISCSI_CMD_HDR_RSRV1_MASK        0x1	/* HSI_COMMENT: first bit of iSCSI PDU header */
#define ISCSI_CMD_HDR_RSRV1_SHIFT       7
	__le32 hdr_second_dword;
#define ISCSI_CMD_HDR_DATA_SEG_LEN_MASK 0xFFFFFF	/* HSI_COMMENT: DataSegmentLength */
#define ISCSI_CMD_HDR_DATA_SEG_LEN_SHIFT        0
#define ISCSI_CMD_HDR_TOTAL_AHS_LEN_MASK        0xFF	/* HSI_COMMENT: TotalAHSLength */
#define ISCSI_CMD_HDR_TOTAL_AHS_LEN_SHIFT       24
	struct regpair lun;	/* HSI_COMMENT: Logical Unit Number. [constant, initialized] */
	__le32 itt;		/* HSI_COMMENT: Initiator Task Tag (only 2 bytes are significant). [constant, initialized] */
	__le32 expected_transfer_length;	/* HSI_COMMENT: Expected Data Transfer Length (only 3 bytes are significant) */
	__le32 cmd_sn;		/* HSI_COMMENT: CmdSn. [constant, initialized] */
	__le32 exp_stat_sn;	/* HSI_COMMENT: various fields for middle-path PDU. [constant, initialized] */
	__le32 cdb[4];		/* HSI_COMMENT: CDB. [constant, initialized] */
};

/* iSCSI Common PDU header */
struct iscsi_common_hdr {
	u8 hdr_status;		/* HSI_COMMENT: Status field of ISCSI header */
	u8 hdr_response;	/* HSI_COMMENT: Response field of ISCSI header for Responses / Reserved for Data-In */
	u8 hdr_flags;		/* HSI_COMMENT: Flags field of ISCSI header */
	u8 hdr_first_byte;
#define ISCSI_COMMON_HDR_OPCODE_MASK    0x3F	/* HSI_COMMENT: Opcode */
#define ISCSI_COMMON_HDR_OPCODE_SHIFT   0
#define ISCSI_COMMON_HDR_IMM_MASK       0x1	/* HSI_COMMENT: Immediate */
#define ISCSI_COMMON_HDR_IMM_SHIFT      6
#define ISCSI_COMMON_HDR_RSRV_MASK      0x1	/* HSI_COMMENT: first bit of iSCSI PDU header */
#define ISCSI_COMMON_HDR_RSRV_SHIFT     7
	__le32 hdr_second_dword;
#define ISCSI_COMMON_HDR_DATA_SEG_LEN_MASK      0xFFFFFF	/* HSI_COMMENT: DataSegmentLength */
#define ISCSI_COMMON_HDR_DATA_SEG_LEN_SHIFT     0
#define ISCSI_COMMON_HDR_TOTAL_AHS_LEN_MASK     0xFF	/* HSI_COMMENT: TotalAHSLength */
#define ISCSI_COMMON_HDR_TOTAL_AHS_LEN_SHIFT    24
	struct regpair lun_reserved;	/* HSI_COMMENT: Bytes 8..15 : LUN (if PDU contains a LUN field) or reserved */
	__le32 itt;		/* HSI_COMMENT: ITT - common to all headers */
	__le32 ttt;		/* HSI_COMMENT: bytes 20 to 23 - common ttt to various PDU headers */
	__le32 cmdstat_sn;	/* HSI_COMMENT: bytes 24 to 27 - common cmd_sn (initiator) or stat_sn (target) to various PDU headers */
	__le32 exp_statcmd_sn;	/* HSI_COMMENT: bytes 28 to 31 - common expected stat_sn (initiator) or cmd_sn (target) to various PDU headers */
	__le32 max_cmd_sn;	/* HSI_COMMENT: bytes 32 to 35 - common max cmd_sn to various PDU headers */
	__le32 data[3];		/* HSI_COMMENT: bytes 36 to 47 */
};

/* ISCSI connection offload params passed by driver to FW in ISCSI offload ramrod  */
struct iscsi_conn_offload_params {
	struct regpair sq_pbl_addr;	/* HSI_COMMENT: PBL SQ pointer */
	struct regpair r2tq_pbl_addr;	/* HSI_COMMENT: PBL R2TQ pointer */
	struct regpair xhq_pbl_addr;	/* HSI_COMMENT: PBL XHQ pointer */
	struct regpair uhq_pbl_addr;	/* HSI_COMMENT: PBL UHQ pointer */
	__le16 physical_q0;	/* HSI_COMMENT: Physical QM queue to be tied to logical Q0 */
	__le16 physical_q1;	/* HSI_COMMENT: Physical QM queue to be tied to logical Q1 */
	u8 flags;
#define ISCSI_CONN_OFFLOAD_PARAMS_TCP_ON_CHIP_1B_MASK   0x1	/* HSI_COMMENT: TCP connect/terminate option. 0 - TCP on host (option-1); 1 - TCP on chip (option-2). */
#define ISCSI_CONN_OFFLOAD_PARAMS_TCP_ON_CHIP_1B_SHIFT  0
#define ISCSI_CONN_OFFLOAD_PARAMS_TARGET_MODE_MASK      0x1	/* HSI_COMMENT: iSCSI connect mode: 0-iSCSI Initiator, 1-iSCSI Target */
#define ISCSI_CONN_OFFLOAD_PARAMS_TARGET_MODE_SHIFT     1
#define ISCSI_CONN_OFFLOAD_PARAMS_RESTRICTED_MODE_MASK  0x1	/* HSI_COMMENT: Restricted mode: 0 - un-restricted (deviating from the RFC), 1 - restricted (according to the RFC) */
#define ISCSI_CONN_OFFLOAD_PARAMS_RESTRICTED_MODE_SHIFT 2
#define ISCSI_CONN_OFFLOAD_PARAMS_NVMETCP_MODE_MASK     0x1	/* HSI_COMMENT: NVMe TCP mode: 0 - iSCSI, 1 - NVMe TCP */
#define ISCSI_CONN_OFFLOAD_PARAMS_NVMETCP_MODE_SHIFT    3
#define ISCSI_CONN_OFFLOAD_PARAMS_RESERVED1_MASK        0xF	/* HSI_COMMENT: reserved */
#define ISCSI_CONN_OFFLOAD_PARAMS_RESERVED1_SHIFT       4
	u8 default_cq;		/* HSI_COMMENT: Default CQ used to write unsolicited data */
	__le16 reserved0;	/* HSI_COMMENT: reserved */
	__le32 stat_sn;		/* HSI_COMMENT: StatSn for Target Mode only: the first Login Response StatSn value for Target mode */
	__le32 initial_ack;	/* HSI_COMMENT: Initial ack, received from TCP (option 1) */
};

/* iSCSI connection statistics */
struct iscsi_conn_stats_params {
	struct regpair iscsi_tcp_tx_packets_cnt;	/* HSI_COMMENT: Counts number of transmitted TCP packets for this iSCSI connection */
	struct regpair iscsi_tcp_tx_bytes_cnt;	/* HSI_COMMENT: Counts number of transmitted TCP bytes for this iSCSI connection */
	struct regpair iscsi_tcp_tx_rxmit_cnt;	/* HSI_COMMENT: Counts number of TCP retransmission events for this iSCSI connection */
	struct regpair iscsi_tcp_rx_packets_cnt;	/* HSI_COMMENT: Counts number of received TCP packets for this iSCSI connection */
	struct regpair iscsi_tcp_rx_bytes_cnt;	/* HSI_COMMENT: Counts number of received TCP bytes for this iSCSI connection */
	struct regpair iscsi_tcp_rx_dup_ack_cnt;	/* HSI_COMMENT: Counts number of received TCP duplicate acks for this iSCSI connection */
	__le32 iscsi_tcp_rx_chksum_err_cnt;	/* HSI_COMMENT: Counts number of received TCP packets with checksum err for this iSCSI connection */
	__le32 reserved;
};

/* ISCSI connection update params passed by driver to FW in ISCSI update ramrod  */
struct iscsi_conn_update_ramrod_params {
	__le16 reserved0;	/* HSI_COMMENT: reserved. */
	__le16 conn_id;		/* HSI_COMMENT: ISCSI Connection ID. (MOTI_COHEN : draft for DrvSim sake) */
	__le32 reserved1;	/* HSI_COMMENT: reserved. */
	u8 flags;
#define ISCSI_CONN_UPDATE_RAMROD_PARAMS_HD_EN_MASK      0x1	/* HSI_COMMENT: Is header digest enabled */
#define ISCSI_CONN_UPDATE_RAMROD_PARAMS_HD_EN_SHIFT     0
#define ISCSI_CONN_UPDATE_RAMROD_PARAMS_DD_EN_MASK      0x1	/* HSI_COMMENT: Is data digest enabled */
#define ISCSI_CONN_UPDATE_RAMROD_PARAMS_DD_EN_SHIFT     1
#define ISCSI_CONN_UPDATE_RAMROD_PARAMS_INITIAL_R2T_MASK        0x1	/* HSI_COMMENT: Initial R2T */
#define ISCSI_CONN_UPDATE_RAMROD_PARAMS_INITIAL_R2T_SHIFT       2
#define ISCSI_CONN_UPDATE_RAMROD_PARAMS_IMMEDIATE_DATA_MASK     0x1	/* HSI_COMMENT: Immediate data */
#define ISCSI_CONN_UPDATE_RAMROD_PARAMS_IMMEDIATE_DATA_SHIFT    3
#define ISCSI_CONN_UPDATE_RAMROD_PARAMS_DIF_BLOCK_SIZE_MASK     0x1	/* HSI_COMMENT: 0 - 512B, 1 - 4K */
#define ISCSI_CONN_UPDATE_RAMROD_PARAMS_DIF_BLOCK_SIZE_SHIFT    4
#define ISCSI_CONN_UPDATE_RAMROD_PARAMS_DIF_ON_HOST_EN_MASK     0x1	/* HSI_COMMENT: 0 - no DIF, 1 - could be enabled per task */
#define ISCSI_CONN_UPDATE_RAMROD_PARAMS_DIF_ON_HOST_EN_SHIFT    5
#define ISCSI_CONN_UPDATE_RAMROD_PARAMS_DIF_ON_IMM_EN_MASK      0x1	/* HSI_COMMENT: Support DIF on immediate, 1-Yes, 0-No */
#define ISCSI_CONN_UPDATE_RAMROD_PARAMS_DIF_ON_IMM_EN_SHIFT     6
#define ISCSI_CONN_UPDATE_RAMROD_PARAMS_LUN_MAPPER_EN_MASK      0x1	/* HSI_COMMENT: valid only if dif_on_imm_en=1 Does this connection has dif configuration per Lun or Default dif configuration */
#define ISCSI_CONN_UPDATE_RAMROD_PARAMS_LUN_MAPPER_EN_SHIFT     7
	u8 reserved3[3];
	__le32 max_seq_size;	/* HSI_COMMENT: Maximum sequence size. Valid for TX and RX */
	__le32 max_send_pdu_length;	/* HSI_COMMENT: Maximum PDU size. Valid for the TX */
	__le32 max_recv_pdu_length;	/* HSI_COMMENT: Maximum PDU size. Valid for the RX */
	__le32 first_seq_length;	/* HSI_COMMENT: Initial sequence length */
	__le32 exp_stat_sn;	/* HSI_COMMENT: ExpStatSn - Option1 Only */
	union dif_configuration_params dif_on_imme_params;	/* HSI_COMMENT: dif on immmediate params - Target mode Only */
};

/* iSCSI Command PDU header with Extended CDB (Initiator Mode) */
struct iscsi_ext_cdb_cmd_hdr {
	__le16 reserved1;	/* HSI_COMMENT: reserved */
	u8 flags_attr;
#define ISCSI_EXT_CDB_CMD_HDR_ATTR_MASK 0x7	/* HSI_COMMENT: attributes */
#define ISCSI_EXT_CDB_CMD_HDR_ATTR_SHIFT        0
#define ISCSI_EXT_CDB_CMD_HDR_RSRV_MASK 0x3	/* HSI_COMMENT: reserved */
#define ISCSI_EXT_CDB_CMD_HDR_RSRV_SHIFT        3
#define ISCSI_EXT_CDB_CMD_HDR_WRITE_MASK        0x1	/* HSI_COMMENT: write */
#define ISCSI_EXT_CDB_CMD_HDR_WRITE_SHIFT       5
#define ISCSI_EXT_CDB_CMD_HDR_READ_MASK 0x1	/* HSI_COMMENT: read */
#define ISCSI_EXT_CDB_CMD_HDR_READ_SHIFT        6
#define ISCSI_EXT_CDB_CMD_HDR_FINAL_MASK        0x1	/* HSI_COMMENT: final */
#define ISCSI_EXT_CDB_CMD_HDR_FINAL_SHIFT       7
	u8 opcode;		/* HSI_COMMENT: opcode. [constant, initialized] */
	__le32 hdr_second_dword;
#define ISCSI_EXT_CDB_CMD_HDR_DATA_SEG_LEN_MASK 0xFFFFFF	/* HSI_COMMENT: DataSegmentLength */
#define ISCSI_EXT_CDB_CMD_HDR_DATA_SEG_LEN_SHIFT        0
#define ISCSI_EXT_CDB_CMD_HDR_CDB_SIZE_MASK     0xFF	/* HSI_COMMENT: The Extended CDB size in bytes. Maximum Extended CDB size supported is CDB 64B. */
#define ISCSI_EXT_CDB_CMD_HDR_CDB_SIZE_SHIFT    24
	struct regpair lun;	/* HSI_COMMENT: Logical Unit Number. [constant, initialized] */
	__le32 itt;		/* HSI_COMMENT: Initiator Task Tag (only 2 bytes are significant). [constant, initialized] */
	__le32 expected_transfer_length;	/* HSI_COMMENT: Expected Data Transfer Length (only 3 bytes are significant) */
	__le32 cmd_sn;		/* HSI_COMMENT: CmdSn. [constant, initialized] */
	__le32 exp_stat_sn;	/* HSI_COMMENT: various fields for middle-path PDU. [constant, initialized] */
	struct scsi_sge cdb_sge;	/* HSI_COMMENT: Extended CDBs dedicated SGE */
};

/* iSCSI login request PDU header */
struct iscsi_login_req_hdr {
	u8 version_min;		/* HSI_COMMENT: Version-min */
	u8 version_max;		/* HSI_COMMENT: Version-max */
	u8 flags_attr;
#define ISCSI_LOGIN_REQ_HDR_NSG_MASK    0x3	/* HSI_COMMENT: Next Stage (NSG) */
#define ISCSI_LOGIN_REQ_HDR_NSG_SHIFT   0
#define ISCSI_LOGIN_REQ_HDR_CSG_MASK    0x3	/* HSI_COMMENT: Current stage (CSG) */
#define ISCSI_LOGIN_REQ_HDR_CSG_SHIFT   2
#define ISCSI_LOGIN_REQ_HDR_RSRV_MASK   0x3	/* HSI_COMMENT: reserved */
#define ISCSI_LOGIN_REQ_HDR_RSRV_SHIFT  4
#define ISCSI_LOGIN_REQ_HDR_C_MASK      0x1	/* HSI_COMMENT: C (Continue) bit */
#define ISCSI_LOGIN_REQ_HDR_C_SHIFT     6
#define ISCSI_LOGIN_REQ_HDR_T_MASK      0x1	/* HSI_COMMENT: T (Transit) bit */
#define ISCSI_LOGIN_REQ_HDR_T_SHIFT     7
	u8 opcode;		/* HSI_COMMENT: opcode. [constant, initialized] */
	__le32 hdr_second_dword;
#define ISCSI_LOGIN_REQ_HDR_DATA_SEG_LEN_MASK   0xFFFFFF	/* HSI_COMMENT: DataSegmentLength */
#define ISCSI_LOGIN_REQ_HDR_DATA_SEG_LEN_SHIFT  0
#define ISCSI_LOGIN_REQ_HDR_TOTAL_AHS_LEN_MASK  0xFF	/* HSI_COMMENT: TotalAHSLength */
#define ISCSI_LOGIN_REQ_HDR_TOTAL_AHS_LEN_SHIFT 24
	__le32 isid_tabc;	/* HSI_COMMENT: Session identifier high double word [constant, initialized] */
	__le16 tsih;		/* HSI_COMMENT: TSIH */
	__le16 isid_d;		/* HSI_COMMENT: Session identifier low word [constant, initialized] */
	__le32 itt;		/* HSI_COMMENT: Initiator Task Tag (only 2 bytes are significant). [constant, initialized] */
	__le16 reserved1;
	__le16 cid;		/* HSI_COMMENT: Unique Connection ID within the session [constant, initialized] */
	__le32 cmd_sn;		/* HSI_COMMENT: CmdSn. [constant, initialized] */
	__le32 exp_stat_sn;	/* HSI_COMMENT: various fields for middle-path PDU. [constant, initialized] */
	__le32 reserved2[4];
};

/* iSCSI logout request PDU header */
struct iscsi_logout_req_hdr {
	__le16 reserved0;	/* HSI_COMMENT: reserved */
	u8 reason_code;		/* HSI_COMMENT: Reason Code */
	u8 opcode;		/* HSI_COMMENT: opcode. [constant, initialized] */
	__le32 reserved1;
	__le32 reserved2[2];	/* HSI_COMMENT: Reserved */
	__le32 itt;		/* HSI_COMMENT: Initiator Task Tag (only 2 bytes are significant). [constant, initialized] */
	__le16 reserved3;	/* HSI_COMMENT: Reserved */
	__le16 cid;		/* HSI_COMMENT: Unique Connection ID within the session [constant, initialized] */
	__le32 cmd_sn;		/* HSI_COMMENT: CmdSn. [constant, initialized] */
	__le32 exp_stat_sn;	/* HSI_COMMENT: various fields for middle-path PDU. [constant, initialized] */
	__le32 reserved4[4];	/* HSI_COMMENT: Reserved */
};

/* iSCSI Data-out PDU header */
struct iscsi_data_out_hdr {
	__le16 reserved1;	/* HSI_COMMENT: reserved */
	u8 flags_attr;
#define ISCSI_DATA_OUT_HDR_RSRV_MASK    0x7F	/* HSI_COMMENT: reserved */
#define ISCSI_DATA_OUT_HDR_RSRV_SHIFT   0
#define ISCSI_DATA_OUT_HDR_FINAL_MASK   0x1	/* HSI_COMMENT: final */
#define ISCSI_DATA_OUT_HDR_FINAL_SHIFT  7
	u8 opcode;		/* HSI_COMMENT: opcode */
	__le32 reserved2;	/* HSI_COMMENT: reserved */
	struct regpair lun;	/* HSI_COMMENT: Logical Unit Number */
	__le32 itt;		/* HSI_COMMENT: Initiator Task Tag (only 2 bytes are significant) */
	__le32 ttt;		/* HSI_COMMENT: Target Transfer Tag (from R2T) */
	__le32 reserved3;	/* HSI_COMMENT: resrved */
	__le32 exp_stat_sn;	/* HSI_COMMENT: Expected StatSn */
	__le32 reserved4;	/* HSI_COMMENT: resrved */
	__le32 data_sn;		/* HSI_COMMENT: DataSN - PDU index in sequnece */
	__le32 buffer_offset;	/* HSI_COMMENT: Buffer Offset - offset in task */
	__le32 reserved5;	/* HSI_COMMENT: resrved */
};

/* iSCSI Data-in PDU header */
struct iscsi_data_in_hdr {
	u8 status_rsvd;		/* HSI_COMMENT: Status or reserved */
	u8 reserved1;		/* HSI_COMMENT: reserved */
	u8 flags;
#define ISCSI_DATA_IN_HDR_STATUS_MASK   0x1	/* HSI_COMMENT: Status */
#define ISCSI_DATA_IN_HDR_STATUS_SHIFT  0
#define ISCSI_DATA_IN_HDR_UNDERFLOW_MASK        0x1	/* HSI_COMMENT: Residual Underflow */
#define ISCSI_DATA_IN_HDR_UNDERFLOW_SHIFT       1
#define ISCSI_DATA_IN_HDR_OVERFLOW_MASK 0x1	/* HSI_COMMENT: Residual Overflow */
#define ISCSI_DATA_IN_HDR_OVERFLOW_SHIFT        2
#define ISCSI_DATA_IN_HDR_RSRV_MASK     0x7	/* HSI_COMMENT: reserved - 0 */
#define ISCSI_DATA_IN_HDR_RSRV_SHIFT    3
#define ISCSI_DATA_IN_HDR_ACK_MASK      0x1	/* HSI_COMMENT: Acknowledge */
#define ISCSI_DATA_IN_HDR_ACK_SHIFT     6
#define ISCSI_DATA_IN_HDR_FINAL_MASK    0x1	/* HSI_COMMENT: final */
#define ISCSI_DATA_IN_HDR_FINAL_SHIFT   7
	u8 opcode;		/* HSI_COMMENT: opcode */
	__le32 reserved2;	/* HSI_COMMENT: reserved */
	struct regpair lun;	/* HSI_COMMENT: Logical Unit Number */
	__le32 itt;		/* HSI_COMMENT: Initiator Task Tag (only 2 bytes are significant) */
	__le32 ttt;		/* HSI_COMMENT: Target Transfer Tag (from R2T) */
	__le32 stat_sn;		/* HSI_COMMENT: StatSN or reserved */
	__le32 exp_cmd_sn;	/* HSI_COMMENT: Expected CmdSn */
	__le32 max_cmd_sn;	/* HSI_COMMENT: MaxCmdSn */
	__le32 data_sn;		/* HSI_COMMENT: DataSN - PDU index in sequnece */
	__le32 buffer_offset;	/* HSI_COMMENT: Buffer Offset - offset in task */
	__le32 residual_count;	/* HSI_COMMENT: Residual Count */
};

/* iSCSI R2T PDU header */
struct iscsi_r2t_hdr {
	u8 reserved0[3];	/* HSI_COMMENT: reserved */
	u8 opcode;		/* HSI_COMMENT: opcode */
	__le32 reserved2;	/* HSI_COMMENT: reserved */
	struct regpair lun;	/* HSI_COMMENT: Logical Unit Number */
	__le32 itt;		/* HSI_COMMENT: Initiator Task Tag */
	__le32 ttt;		/* HSI_COMMENT: Target Transfer Tag */
	__le32 stat_sn;		/* HSI_COMMENT: stat sn */
	__le32 exp_cmd_sn;	/* HSI_COMMENT: Expected CmdSn */
	__le32 max_cmd_sn;	/* HSI_COMMENT: Max CmdSn */
	__le32 r2t_sn;		/* HSI_COMMENT: DataSN - PDU index in sequnece */
	__le32 buffer_offset;	/* HSI_COMMENT: Buffer Offset - offset in task */
	__le32 desired_data_trns_len;	/* HSI_COMMENT: Desired data trnsfer len */
};

/* iSCSI NOP-out PDU header */
struct iscsi_nop_out_hdr {
	__le16 reserved1;	/* HSI_COMMENT: reserved */
	u8 flags_attr;
#define ISCSI_NOP_OUT_HDR_RSRV_MASK     0x7F	/* HSI_COMMENT: reserved */
#define ISCSI_NOP_OUT_HDR_RSRV_SHIFT    0
#define ISCSI_NOP_OUT_HDR_CONST1_MASK   0x1	/* HSI_COMMENT: const1 */
#define ISCSI_NOP_OUT_HDR_CONST1_SHIFT  7
	u8 opcode;		/* HSI_COMMENT: opcode */
	__le32 reserved2;	/* HSI_COMMENT: reserved */
	struct regpair lun;	/* HSI_COMMENT: Logical Unit Number */
	__le32 itt;		/* HSI_COMMENT: Initiator Task Tag (only 2 bytes are significant) */
	__le32 ttt;		/* HSI_COMMENT: Target Transfer Tag (from R2T) */
	__le32 cmd_sn;		/* HSI_COMMENT: CmdSN */
	__le32 exp_stat_sn;	/* HSI_COMMENT: Expected StatSn */
	__le32 reserved3;	/* HSI_COMMENT: reserved */
	__le32 reserved4;	/* HSI_COMMENT: reserved */
	__le32 reserved5;	/* HSI_COMMENT: reserved */
	__le32 reserved6;	/* HSI_COMMENT: reserved */
};

/* iSCSI NOP-in PDU header */
struct iscsi_nop_in_hdr {
	__le16 reserved0;	/* HSI_COMMENT: reserved */
	u8 flags_attr;
#define ISCSI_NOP_IN_HDR_RSRV_MASK      0x7F	/* HSI_COMMENT: reserved */
#define ISCSI_NOP_IN_HDR_RSRV_SHIFT     0
#define ISCSI_NOP_IN_HDR_CONST1_MASK    0x1	/* HSI_COMMENT: const1 */
#define ISCSI_NOP_IN_HDR_CONST1_SHIFT   7
	u8 opcode;		/* HSI_COMMENT: opcode */
	__le32 hdr_second_dword;
#define ISCSI_NOP_IN_HDR_DATA_SEG_LEN_MASK      0xFFFFFF	/* HSI_COMMENT: DataSegmentLength */
#define ISCSI_NOP_IN_HDR_DATA_SEG_LEN_SHIFT     0
#define ISCSI_NOP_IN_HDR_TOTAL_AHS_LEN_MASK     0xFF	/* HSI_COMMENT: TotalAHSLength */
#define ISCSI_NOP_IN_HDR_TOTAL_AHS_LEN_SHIFT    24
	struct regpair lun;	/* HSI_COMMENT: Logical Unit Number */
	__le32 itt;		/* HSI_COMMENT: Initiator Task Tag (only 2 bytes are significant) */
	__le32 ttt;		/* HSI_COMMENT: Target Transfer Tag */
	__le32 stat_sn;		/* HSI_COMMENT: stat_sn */
	__le32 exp_cmd_sn;	/* HSI_COMMENT: exp_cmd_sn */
	__le32 max_cmd_sn;	/* HSI_COMMENT: max_cmd_sn */
	__le32 reserved5;	/* HSI_COMMENT: reserved */
	__le32 reserved6;	/* HSI_COMMENT: reserved */
	__le32 reserved7;	/* HSI_COMMENT: reserved */
};

/* iSCSI Login Response PDU header */
struct iscsi_login_response_hdr {
	u8 version_active;	/* HSI_COMMENT: Version-active */
	u8 version_max;		/* HSI_COMMENT: Version-max */
	u8 flags_attr;
#define ISCSI_LOGIN_RESPONSE_HDR_NSG_MASK       0x3	/* HSI_COMMENT: Next Stage (NSG) */
#define ISCSI_LOGIN_RESPONSE_HDR_NSG_SHIFT      0
#define ISCSI_LOGIN_RESPONSE_HDR_CSG_MASK       0x3	/* HSI_COMMENT: Current stage (CSG) */
#define ISCSI_LOGIN_RESPONSE_HDR_CSG_SHIFT      2
#define ISCSI_LOGIN_RESPONSE_HDR_RSRV_MASK      0x3	/* HSI_COMMENT: reserved */
#define ISCSI_LOGIN_RESPONSE_HDR_RSRV_SHIFT     4
#define ISCSI_LOGIN_RESPONSE_HDR_C_MASK 0x1	/* HSI_COMMENT: C (Continue) bit */
#define ISCSI_LOGIN_RESPONSE_HDR_C_SHIFT        6
#define ISCSI_LOGIN_RESPONSE_HDR_T_MASK 0x1	/* HSI_COMMENT: T (Transit) bit */
#define ISCSI_LOGIN_RESPONSE_HDR_T_SHIFT        7
	u8 opcode;		/* HSI_COMMENT: opcode. [constant, initialized] */
	__le32 hdr_second_dword;
#define ISCSI_LOGIN_RESPONSE_HDR_DATA_SEG_LEN_MASK      0xFFFFFF	/* HSI_COMMENT: DataSegmentLength */
#define ISCSI_LOGIN_RESPONSE_HDR_DATA_SEG_LEN_SHIFT     0
#define ISCSI_LOGIN_RESPONSE_HDR_TOTAL_AHS_LEN_MASK     0xFF	/* HSI_COMMENT: TotalAHSLength */
#define ISCSI_LOGIN_RESPONSE_HDR_TOTAL_AHS_LEN_SHIFT    24
	__le32 isid_tabc;	/* HSI_COMMENT: Session identifier high double word [constant, initialized] */
	__le16 tsih;		/* HSI_COMMENT: TSIH */
	__le16 isid_d;		/* HSI_COMMENT: Session identifier low word [constant, initialized] */
	__le32 itt;		/* HSI_COMMENT: Initiator Task Tag (only 2 bytes are significant). [constant, initialized] */
	__le32 reserved1;
	__le32 stat_sn;		/* HSI_COMMENT: CmdSn. [constant, initialized] */
	__le32 exp_cmd_sn;	/* HSI_COMMENT: various fields for middle-path PDU. [constant, initialized] */
	__le32 max_cmd_sn;	/* HSI_COMMENT: max_cmd_sn */
	__le16 reserved2;
	u8 status_detail;	/* HSI_COMMENT: status_detail */
	u8 status_class;	/* HSI_COMMENT: status_class */
	__le32 reserved4[2];
};

/* iSCSI Logout Response PDU header */
struct iscsi_logout_response_hdr {
	u8 reserved1;		/* HSI_COMMENT: reserved */
	u8 response;		/* HSI_COMMENT: response */
	u8 flags;		/* HSI_COMMENT: flags and attributes */
	u8 opcode;		/* HSI_COMMENT: opcode. [constant, initialized] */
	__le32 hdr_second_dword;
#define ISCSI_LOGOUT_RESPONSE_HDR_DATA_SEG_LEN_MASK     0xFFFFFF	/* HSI_COMMENT: DataSegmentLength */
#define ISCSI_LOGOUT_RESPONSE_HDR_DATA_SEG_LEN_SHIFT    0
#define ISCSI_LOGOUT_RESPONSE_HDR_TOTAL_AHS_LEN_MASK    0xFF	/* HSI_COMMENT: TotalAHSLength */
#define ISCSI_LOGOUT_RESPONSE_HDR_TOTAL_AHS_LEN_SHIFT   24
	__le32 reserved2[2];	/* HSI_COMMENT: Reserved */
	__le32 itt;		/* HSI_COMMENT: Initiator Task Tag (only 2 bytes are significant). [constant, initialized] */
	__le32 reserved3;	/* HSI_COMMENT: Reserved */
	__le32 stat_sn;		/* HSI_COMMENT: CmdSN */
	__le32 exp_cmd_sn;	/* HSI_COMMENT: Expected StatSn */
	__le32 max_cmd_sn;	/* HSI_COMMENT: CmdSN */
	__le32 reserved4;	/* HSI_COMMENT: Reserved */
	__le16 time_2_retain;	/* HSI_COMMENT: Time to Retain  */
	__le16 time_2_wait;	/* HSI_COMMENT: Time to wait */
	__le32 reserved5[1];	/* HSI_COMMENT: Reserved */
};

/* iSCSI Text Request PDU header */
struct iscsi_text_request_hdr {
	__le16 reserved0;	/* HSI_COMMENT: reserved */
	u8 flags_attr;
#define ISCSI_TEXT_REQUEST_HDR_RSRV_MASK        0x3F	/* HSI_COMMENT: reserved */
#define ISCSI_TEXT_REQUEST_HDR_RSRV_SHIFT       0
#define ISCSI_TEXT_REQUEST_HDR_C_MASK   0x1	/* HSI_COMMENT: C (Continue) bit */
#define ISCSI_TEXT_REQUEST_HDR_C_SHIFT  6
#define ISCSI_TEXT_REQUEST_HDR_F_MASK   0x1	/* HSI_COMMENT: F (Final) bit */
#define ISCSI_TEXT_REQUEST_HDR_F_SHIFT  7
	u8 opcode;		/* HSI_COMMENT: opcode. [constant, initialized] */
	__le32 hdr_second_dword;
#define ISCSI_TEXT_REQUEST_HDR_DATA_SEG_LEN_MASK        0xFFFFFF	/* HSI_COMMENT: DataSegmentLength */
#define ISCSI_TEXT_REQUEST_HDR_DATA_SEG_LEN_SHIFT       0
#define ISCSI_TEXT_REQUEST_HDR_TOTAL_AHS_LEN_MASK       0xFF	/* HSI_COMMENT: TotalAHSLength */
#define ISCSI_TEXT_REQUEST_HDR_TOTAL_AHS_LEN_SHIFT      24
	struct regpair lun;	/* HSI_COMMENT: Logical Unit Number */
	__le32 itt;		/* HSI_COMMENT: Initiator Task Tag (only 2 bytes are significant). [constant, initialized] */
	__le32 ttt;		/* HSI_COMMENT: Referenced Task Tag or 0xffffffff */
	__le32 cmd_sn;		/* HSI_COMMENT: cmd_sn */
	__le32 exp_stat_sn;	/* HSI_COMMENT: exp_stat_sn */
	__le32 reserved4[4];	/* HSI_COMMENT: Reserved */
};

/* iSCSI Text Response PDU header */
struct iscsi_text_response_hdr {
	__le16 reserved1;	/* HSI_COMMENT: reserved */
	u8 flags;
#define ISCSI_TEXT_RESPONSE_HDR_RSRV_MASK       0x3F	/* HSI_COMMENT: reserved */
#define ISCSI_TEXT_RESPONSE_HDR_RSRV_SHIFT      0
#define ISCSI_TEXT_RESPONSE_HDR_C_MASK  0x1	/* HSI_COMMENT: C (Continue) bit */
#define ISCSI_TEXT_RESPONSE_HDR_C_SHIFT 6
#define ISCSI_TEXT_RESPONSE_HDR_F_MASK  0x1	/* HSI_COMMENT: F (Final) bit */
#define ISCSI_TEXT_RESPONSE_HDR_F_SHIFT 7
	u8 opcode;		/* HSI_COMMENT: opcode. [constant, initialized] */
	__le32 hdr_second_dword;
#define ISCSI_TEXT_RESPONSE_HDR_DATA_SEG_LEN_MASK       0xFFFFFF	/* HSI_COMMENT: DataSegmentLength */
#define ISCSI_TEXT_RESPONSE_HDR_DATA_SEG_LEN_SHIFT      0
#define ISCSI_TEXT_RESPONSE_HDR_TOTAL_AHS_LEN_MASK      0xFF	/* HSI_COMMENT: TotalAHSLength */
#define ISCSI_TEXT_RESPONSE_HDR_TOTAL_AHS_LEN_SHIFT     24
	struct regpair lun;	/* HSI_COMMENT: Logical Unit Number */
	__le32 itt;		/* HSI_COMMENT: Initiator Task Tag (only 2 bytes are significant). [constant, initialized] */
	__le32 ttt;		/* HSI_COMMENT: Target Task Tag */
	__le32 stat_sn;		/* HSI_COMMENT: CmdSN */
	__le32 exp_cmd_sn;	/* HSI_COMMENT: Expected StatSn */
	__le32 max_cmd_sn;	/* HSI_COMMENT: CmdSN */
	__le32 reserved4[3];	/* HSI_COMMENT: Reserved */
};

/* iSCSI TMF Request PDU header */
struct iscsi_tmf_request_hdr {
	__le16 reserved0;	/* HSI_COMMENT: reserved */
	u8 function;		/* HSI_COMMENT: function */
	u8 opcode;		/* HSI_COMMENT: opcode. [constant, initialized] */
	__le32 hdr_second_dword;
#define ISCSI_TMF_REQUEST_HDR_DATA_SEG_LEN_MASK 0xFFFFFF	/* HSI_COMMENT: DataSegmentLength */
#define ISCSI_TMF_REQUEST_HDR_DATA_SEG_LEN_SHIFT        0
#define ISCSI_TMF_REQUEST_HDR_TOTAL_AHS_LEN_MASK        0xFF	/* HSI_COMMENT: TotalAHSLength */
#define ISCSI_TMF_REQUEST_HDR_TOTAL_AHS_LEN_SHIFT       24
	struct regpair lun;	/* HSI_COMMENT: Logical Unit Number */
	__le32 itt;		/* HSI_COMMENT: Initiator Task Tag (only 2 bytes are significant). [constant, initialized] */
	__le32 rtt;		/* HSI_COMMENT: Referenced Task Tag or 0xffffffff */
	__le32 cmd_sn;		/* HSI_COMMENT: cmd_sn */
	__le32 exp_stat_sn;	/* HSI_COMMENT: exp_stat_sn */
	__le32 ref_cmd_sn;	/* HSI_COMMENT: ref_cmd_sn */
	__le32 exp_data_sn;	/* HSI_COMMENT: exp_data_sn */
	__le32 reserved4[2];	/* HSI_COMMENT: Reserved */
};

/* iSCSI TMF Response PDU header */
struct iscsi_tmf_response_hdr {
	u8 reserved2;		/* HSI_COMMENT: reserved2 */
	u8 hdr_response;	/* HSI_COMMENT: Response field of ISCSI header for Responses / Reserved for Data-In */
	u8 hdr_flags;		/* HSI_COMMENT: Flags field of ISCSI header */
	u8 opcode;		/* HSI_COMMENT: opcode. [constant, initialized] */
	__le32 hdr_second_dword;
#define ISCSI_TMF_RESPONSE_HDR_DATA_SEG_LEN_MASK        0xFFFFFF	/* HSI_COMMENT: DataSegmentLength */
#define ISCSI_TMF_RESPONSE_HDR_DATA_SEG_LEN_SHIFT       0
#define ISCSI_TMF_RESPONSE_HDR_TOTAL_AHS_LEN_MASK       0xFF	/* HSI_COMMENT: TotalAHSLength */
#define ISCSI_TMF_RESPONSE_HDR_TOTAL_AHS_LEN_SHIFT      24
	struct regpair reserved0;
	__le32 itt;		/* HSI_COMMENT: Initiator Task Tag (only 2 bytes are significant). [constant, initialized] */
	__le32 reserved1;	/* HSI_COMMENT: Reserved */
	__le32 stat_sn;		/* HSI_COMMENT: stat_sn */
	__le32 exp_cmd_sn;	/* HSI_COMMENT: exp_cmd_sn */
	__le32 max_cmd_sn;	/* HSI_COMMENT: max_cmd_sn */
	__le32 reserved4[3];	/* HSI_COMMENT: Reserved */
};

/* iSCSI Response PDU header */
struct iscsi_response_hdr {
	u8 hdr_status;		/* HSI_COMMENT: Status field of ISCSI header */
	u8 hdr_response;	/* HSI_COMMENT: Response field of ISCSI header for Responses / Reserved for Data-In */
	u8 hdr_flags;		/* HSI_COMMENT: Flags field of ISCSI header */
	u8 opcode;		/* HSI_COMMENT: opcode. [constant, initialized] */
	__le32 hdr_second_dword;
#define ISCSI_RESPONSE_HDR_DATA_SEG_LEN_MASK    0xFFFFFF	/* HSI_COMMENT: DataSegmentLength */
#define ISCSI_RESPONSE_HDR_DATA_SEG_LEN_SHIFT   0
#define ISCSI_RESPONSE_HDR_TOTAL_AHS_LEN_MASK   0xFF	/* HSI_COMMENT: TotalAHSLength */
#define ISCSI_RESPONSE_HDR_TOTAL_AHS_LEN_SHIFT  24
	struct regpair lun;	/* HSI_COMMENT: Logical Unit Number */
	__le32 itt;		/* HSI_COMMENT: Initiator Task Tag (only 2 bytes are significant). [constant, initialized] */
	__le32 snack_tag;	/* HSI_COMMENT: Currently ERL>0 is not supported */
	__le32 stat_sn;		/* HSI_COMMENT: CmdSN */
	__le32 exp_cmd_sn;	/* HSI_COMMENT: Expected StatSn */
	__le32 max_cmd_sn;	/* HSI_COMMENT: CmdSN */
	__le32 exp_data_sn;	/* HSI_COMMENT: exp_data_sn */
	__le32 bi_residual_count;	/* HSI_COMMENT: bi residual count */
	__le32 residual_count;	/* HSI_COMMENT: residual count */
};

/* iSCSI Reject PDU header */
struct iscsi_reject_hdr {
	u8 reserved4;		/* HSI_COMMENT: Reserved */
	u8 hdr_reason;		/* HSI_COMMENT: The reject reason */
	u8 hdr_flags;		/* HSI_COMMENT: Flags field of ISCSI header */
	u8 opcode;		/* HSI_COMMENT: opcode. [constant, initialized] */
	__le32 hdr_second_dword;
#define ISCSI_REJECT_HDR_DATA_SEG_LEN_MASK      0xFFFFFF	/* HSI_COMMENT: DataSegmentLength */
#define ISCSI_REJECT_HDR_DATA_SEG_LEN_SHIFT     0
#define ISCSI_REJECT_HDR_TOTAL_AHS_LEN_MASK     0xFF	/* HSI_COMMENT: TotalAHSLength */
#define ISCSI_REJECT_HDR_TOTAL_AHS_LEN_SHIFT    24
	struct regpair reserved0;
	__le32 all_ones;
	__le32 reserved2;
	__le32 stat_sn;		/* HSI_COMMENT: stat_sn */
	__le32 exp_cmd_sn;	/* HSI_COMMENT: exp_cmd_sn */
	__le32 max_cmd_sn;	/* HSI_COMMENT: max_cmd_sn */
	__le32 data_sn;		/* HSI_COMMENT: data_sn */
	__le32 reserved3[2];	/* HSI_COMMENT: reserved3 */
};

/* PDU header part of Ystorm task context */
union iscsi_task_hdr {
	struct iscsi_common_hdr common;	/* HSI_COMMENT: Command PDU header */
	struct data_hdr data;	/* HSI_COMMENT: Command PDU header */
	struct iscsi_cmd_hdr cmd;	/* HSI_COMMENT: Command PDU header */
	struct iscsi_ext_cdb_cmd_hdr ext_cdb_cmd;	/* HSI_COMMENT: Command PDU header with extended CDB - Initiator Mode */
	struct iscsi_login_req_hdr login_req;	/* HSI_COMMENT: Login request PDU header */
	struct iscsi_logout_req_hdr logout_req;	/* HSI_COMMENT: Logout request PDU header */
	struct iscsi_data_out_hdr data_out;	/* HSI_COMMENT: Data-out PDU header */
	struct iscsi_data_in_hdr data_in;	/* HSI_COMMENT: Data-in PDU header */
	struct iscsi_r2t_hdr r2t;	/* HSI_COMMENT: R2T PDU header */
	struct iscsi_nop_out_hdr nop_out;	/* HSI_COMMENT: NOP-out PDU header */
	struct iscsi_nop_in_hdr nop_in;	/* HSI_COMMENT: NOP-in PDU header */
	struct iscsi_login_response_hdr login_response;	/* HSI_COMMENT: Login response PDU header */
	struct iscsi_logout_response_hdr logout_response;	/* HSI_COMMENT: Logout response PDU header */
	struct iscsi_text_request_hdr text_request;	/* HSI_COMMENT: Text request PDU header */
	struct iscsi_text_response_hdr text_response;	/* HSI_COMMENT: Text response PDU header */
	struct iscsi_tmf_request_hdr tmf_request;	/* HSI_COMMENT: TMF request PDU header */
	struct iscsi_tmf_response_hdr tmf_response;	/* HSI_COMMENT: TMF response PDU header */
	struct iscsi_response_hdr response;	/* HSI_COMMENT: Text response PDU header */
	struct iscsi_reject_hdr reject;	/* HSI_COMMENT: Reject PDU header */
	struct iscsi_async_msg_hdr async_msg;	/* HSI_COMMENT: Asynchronous Message PDU header */
};

/* iSCSI CQ element */
struct iscsi_cqe_common {
	__le16 conn_id;		/* HSI_COMMENT: Drivers connection Id */
	u8 cqe_type;		/* HSI_COMMENT: Indicates CQE type (use enum iscsi_cqes_type) */
	union cqe_error_status error_bitmap;	/* HSI_COMMENT: CQE error status */
	__le32 reserved[3];
	union iscsi_task_hdr iscsi_hdr;	/* HSI_COMMENT: iscsi header union */
};

/* iSCSI CQ element */
struct iscsi_cqe_solicited {
	__le16 conn_id;		/* HSI_COMMENT: Drivers connection Id */
	u8 cqe_type;		/* HSI_COMMENT: Indicates CQE type (use enum iscsi_cqes_type) */
	union cqe_error_status error_bitmap;	/* HSI_COMMENT: CQE error status */
	__le16 itid;		/* HSI_COMMENT: initiator itt (Initiator mode) or target ttt (Target mode) */
	u8 task_type;		/* HSI_COMMENT: Task type */
	u8 fw_dbg_field;	/* HSI_COMMENT: FW debug params */
	u8 caused_conn_err;	/* HSI_COMMENT: Equals 1 if this TID caused the connection error, otherwise equals 0. */
	u8 reserved0[3];
	__le32 data_truncated_bytes;	/* HSI_COMMENT: Target Mode only: Valid only if data_truncated_err equals 1: The remaining bytes till the end of the IO. */
	union iscsi_task_hdr iscsi_hdr;	/* HSI_COMMENT: iscsi header union */
};

/* iSCSI CQ element */
struct iscsi_cqe_unsolicited {
	__le16 conn_id;		/* HSI_COMMENT: Drivers connection Id */
	u8 cqe_type;		/* HSI_COMMENT: Indicates CQE type (use enum iscsi_cqes_type) */
	union cqe_error_status error_bitmap;	/* HSI_COMMENT: CQE error status */
	__le16 reserved0;	/* HSI_COMMENT: Reserved */
	u8 reserved1;		/* HSI_COMMENT: Reserved */
	u8 unsol_cqe_type;	/* HSI_COMMENT: Represent this unsolicited CQE position in a sequence of packets belonging to the same unsolicited PDU (use enum iscsi_cqe_unsolicited_type) */
	__le16 rqe_opaque;	/* HSI_COMMENT: Relevant for Unsolicited CQE only: The opaque data of RQ BDQ */
	__le16 cmd_counter;	/* HSI_COMMENT: Counter per CQ/CMDQ pair - used to keep order */
	__le16 reserved2[2];	/* HSI_COMMENT: Reserved */
	union iscsi_task_hdr iscsi_hdr;	/* HSI_COMMENT: iscsi header union */
};

/* iSCSI CQ element */
union iscsi_cqe {
	struct iscsi_cqe_common cqe_common;	/* HSI_COMMENT: Common CQE */
	struct iscsi_cqe_solicited cqe_solicited;	/* HSI_COMMENT: Solicited CQE */
	struct iscsi_cqe_unsolicited cqe_unsolicited;	/* HSI_COMMENT: Unsolicited CQE. relevant only when cqe_opcode == ISCSI_CQE_TYPE_UNSOLICITED */
};

/* iSCSI CQE type  */
enum iscsi_cqes_type {
	ISCSI_CQE_TYPE_SOLICITED = 1,	/* HSI_COMMENT: iSCSI CQE with solicited data */
	ISCSI_CQE_TYPE_UNSOLICITED,	/* HSI_COMMENT: iSCSI CQE with unsolicited data */
	ISCSI_CQE_TYPE_SOLICITED_WITH_SENSE,	/* HSI_COMMENT: iSCSI CQE with solicited with sense data */
	ISCSI_CQE_TYPE_TASK_CLEANUP,	/* HSI_COMMENT: iSCSI CQE task cleanup */
	ISCSI_CQE_TYPE_DUMMY,	/* HSI_COMMENT: iSCSI Dummy CQE */
	MAX_ISCSI_CQES_TYPE
};

/* iSCSI CQE type  */
enum iscsi_cqe_unsolicited_type {
	ISCSI_CQE_UNSOLICITED_NONE,	/* HSI_COMMENT: iSCSI CQE with unsolicited data */
	ISCSI_CQE_UNSOLICITED_SINGLE,	/* HSI_COMMENT: iSCSI CQE with unsolicited data */
	ISCSI_CQE_UNSOLICITED_FIRST,	/* HSI_COMMENT: iSCSI CQE with unsolicited data */
	ISCSI_CQE_UNSOLICITED_MIDDLE,	/* HSI_COMMENT: iSCSI CQE with unsolicited data */
	ISCSI_CQE_UNSOLICITED_LAST,	/* HSI_COMMENT: iSCSI CQE with unsolicited data */
	MAX_ISCSI_CQE_UNSOLICITED_TYPE
};

/* iSCSI DIF flags  */
struct iscsi_dif_flags {
	u8 flags;
#define ISCSI_DIF_FLAGS_PROT_INTERVAL_SIZE_LOG_MASK     0xF	/* HSI_COMMENT: Protection log interval (9=512 10=1024  11=2048 12=4096 13=8192) */
#define ISCSI_DIF_FLAGS_PROT_INTERVAL_SIZE_LOG_SHIFT    0
#define ISCSI_DIF_FLAGS_DIF_TO_PEER_MASK        0x1	/* HSI_COMMENT: If DIF protection is configured against target (0=no, 1=yes) */
#define ISCSI_DIF_FLAGS_DIF_TO_PEER_SHIFT       4
#define ISCSI_DIF_FLAGS_HOST_INTERFACE_MASK     0x7	/* HSI_COMMENT: If DIF/DIX protection is configured against the host (0=none, 1=DIF, 2=DIX 2 bytes, 3=DIX 4 bytes, 4=DIX 8 bytes) */
#define ISCSI_DIF_FLAGS_HOST_INTERFACE_SHIFT    5
};

/* iSCSI kernel completion queue IDs  */
enum iscsi_eqe_opcode {
	ISCSI_EVENT_TYPE_INIT_FUNC = 0,	/* HSI_COMMENT: iSCSI response after init Ramrod */
	ISCSI_EVENT_TYPE_DESTROY_FUNC,	/* HSI_COMMENT: iSCSI response after destroy Ramrod */
	ISCSI_EVENT_TYPE_OFFLOAD_CONN,	/* HSI_COMMENT: iSCSI response after option 2 offload Ramrod */
	ISCSI_EVENT_TYPE_UPDATE_CONN,	/* HSI_COMMENT: iSCSI response after update Ramrod */
	ISCSI_EVENT_TYPE_CLEAR_SQ,	/* HSI_COMMENT: iSCSI response after clear sq Ramrod */
	ISCSI_EVENT_TYPE_TERMINATE_CONN,	/* HSI_COMMENT: iSCSI response after termination Ramrod */
	ISCSI_EVENT_TYPE_MAC_UPDATE_CONN,	/* HSI_COMMENT: iSCSI response after MAC address update Ramrod */
	ISCSI_EVENT_TYPE_COLLECT_STATS_CONN,	/* HSI_COMMENT: iSCSI response after collecting connection statistics Ramrod */
	ISCSI_EVENT_TYPE_ASYN_CONNECT_COMPLETE,	/* HSI_COMMENT: iSCSI response after option 2 connect completed (A-syn EQE) */
	ISCSI_EVENT_TYPE_ASYN_TERMINATE_DONE,	/* HSI_COMMENT: iSCSI response after option 2 termination completed (A-syn EQE) */
	ISCSI_EVENT_TYPE_START_OF_ERROR_TYPES = 10,	/* HSI_COMMENT: Never returned in EQE, used to separate Regular event types from Error event types */
	ISCSI_EVENT_TYPE_ASYN_ABORT_RCVD,	/* HSI_COMMENT: iSCSI abort response after TCP RST packet recieve (A-syn EQE) */
	ISCSI_EVENT_TYPE_ASYN_CLOSE_RCVD,	/* HSI_COMMENT: iSCSI response after close receive (A-syn EQE) */
	ISCSI_EVENT_TYPE_ASYN_SYN_RCVD,	/* HSI_COMMENT: iSCSI response after TCP SYN+ACK packet receive (A-syn EQE) */
	ISCSI_EVENT_TYPE_ASYN_MAX_RT_TIME,	/* HSI_COMMENT: iSCSI error - tcp max retransmit time (A-syn EQE) */
	ISCSI_EVENT_TYPE_ASYN_MAX_RT_CNT,	/* HSI_COMMENT: iSCSI error - tcp max retransmit count (A-syn EQE) */
	ISCSI_EVENT_TYPE_ASYN_MAX_KA_PROBES_CNT,	/* HSI_COMMENT: iSCSI error - tcp ka probes count (A-syn EQE) */
	ISCSI_EVENT_TYPE_ASYN_FIN_WAIT2,	/* HSI_COMMENT: iSCSI error - tcp fin wait 2 (A-syn EQE) */
	ISCSI_EVENT_TYPE_ISCSI_CONN_ERROR,	/* HSI_COMMENT: iSCSI error response (A-syn EQE) */
	ISCSI_EVENT_TYPE_TCP_CONN_ERROR,	/* HSI_COMMENT: iSCSI error - tcp error (A-syn EQE) */
	MAX_ISCSI_EQE_OPCODE
};

/* iSCSI EQE and CQE completion status  */
enum iscsi_error_types {
	ISCSI_STATUS_NONE = 0,
	ISCSI_CQE_ERROR_UNSOLICITED_RCV_ON_INVALID_CONN = 1,
	ISCSI_CONN_ERROR_TASK_CID_MISMATCH,	/* HSI_COMMENT: iSCSI connection error - Corrupted Task context  */
	ISCSI_CONN_ERROR_TASK_NOT_VALID,	/* HSI_COMMENT: iSCSI connection error - The task is not valid  */
	ISCSI_CONN_ERROR_RQ_RING_IS_FULL,	/* HSI_COMMENT: iSCSI connection error - RQ full  */
	ISCSI_CONN_ERROR_CMDQ_RING_IS_FULL,	/* HSI_COMMENT: iSCSI connection error - CMDQ full (Target only)  */
	ISCSI_CONN_ERROR_HQE_CACHING_FAILED,	/* HSI_COMMENT: iSCSI connection error - HQ error  */
	ISCSI_CONN_ERROR_HEADER_DIGEST_ERROR,	/* HSI_COMMENT: iSCSI connection error - Header digest error */
	ISCSI_CONN_ERROR_LOCAL_COMPLETION_ERROR,	/* HSI_COMMENT: iSCSI connection error - Local completion bit is not correct   (A-syn EQE) */
	ISCSI_CONN_ERROR_DATA_OVERRUN,	/* HSI_COMMENT: iSCSI connection error - data overrun */
	ISCSI_CONN_ERROR_OUT_OF_SGES_ERROR,	/* HSI_COMMENT: iSCSI connection error - out of sges in task context */
	ISCSI_CONN_ERROR_IP_OPTIONS_ERROR,	/* HSI_COMMENT: TCP connection error - IP option error  */
	ISCSI_CONN_ERROR_PRS_ERRORS,	/* HSI_COMMENT: TCP connection error - error indication form parser */
	ISCSI_CONN_ERROR_CONNECT_INVALID_TCP_OPTION,	/* HSI_COMMENT: TCP connection error - tcp options error(option 2 only)  */
	ISCSI_CONN_ERROR_TCP_IP_FRAGMENT_ERROR,	/* HSI_COMMENT: TCP connection error - IP fragmentation error  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_AHS_LEN,	/* HSI_COMMENT: iSCSI connection error - invalid AHS length (Target only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_AHS_TYPE,	/* HSI_COMMENT: iSCSI connection error - invalid AHS type (Target only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_ITT_OUT_OF_RANGE,	/* HSI_COMMENT: iSCSI connection error - invalid ITT  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_TTT_OUT_OF_RANGE,	/* HSI_COMMENT: iSCSI connection error - invalid TTT (Target only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_DATA_SEG_LEN_EXCEEDS_PDU_SIZE,	/* HSI_COMMENT: iSCSI connection error - PDU data_seg_len > max receive pdu size */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_INVALID_OPCODE,	/* HSI_COMMENT: iSCSI connection error - invalid PDU opcode */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_OOO_TRIM,	/* HSI_COMMENT: iSCSI connection error - ooo trim error */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_FIN_LOOPBACK,	/* HSI_COMMENT: iSCSI connection error - packet with FIN goes to LoopBack */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_RST_LOOPBACK,	/* HSI_COMMENT: iSCSI connection error - packet with RST goes to LoopBack */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_EVENTID_MISMATCH,	/* HSI_COMMENT: iSCSI connection error - packet with RST goes to LoopBack */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_FLUSH,	/* HSI_COMMENT: iSCSI connection error - result code from the searcher for the searcher flush operation is not eSrchResult_Add_Del_Chg_SUCCESS */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_INVALID_OPCODE_BEFORE_UPDATE,	/* HSI_COMMENT: iSCSI connection error - invalid PDU opcode before update ramrod (Option 2 only)  */
	ISCSI_CONN_ERROR_UNVALID_NOPIN_DSL,	/* HSI_COMMENT: iSCSI connection error - NOPIN dsl > 0 and ITT = 0xffffffff (Initiator only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_R2T_CARRIES_NO_DATA,	/* HSI_COMMENT: iSCSI connection error - R2T dsl > 0 (Initiator only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_DATA_SN,	/* HSI_COMMENT: iSCSI connection error - DATA-SN error  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_DATA_IN_TTT,	/* HSI_COMMENT: iSCSI connection error - DATA-IN TTT error (Initiator only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_DATA_OUT_ITT,	/* HSI_COMMENT: iSCSI connection error - DATA-OUT ITT error (Target only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_R2T_TTT,	/* HSI_COMMENT: iSCSI connection error - R2T TTT error (Initiator only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_R2T_BUFFER_OFFSET,	/* HSI_COMMENT: iSCSI connection error - R2T buffer offset error (Initiator only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_BUFFER_OFFSET_OOO,	/* HSI_COMMENT: iSCSI connection error - DATA PDU buffer offset error  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_R2T_SN,	/* HSI_COMMENT: iSCSI connection error - R2T SN error (Initiator only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_DESIRED_DATA_TRNS_LEN_0,	/* HSI_COMMENT: iSCSI connection error - R2T desired data transfer length = 0 (Initiator only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_DESIRED_DATA_TRNS_LEN_1,	/* HSI_COMMENT: iSCSI connection error - R2T desired data transfer length less then max burst size (Initiator only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_DESIRED_DATA_TRNS_LEN_2,	/* HSI_COMMENT: iSCSI connection error - R2T desired data transfer length + buffer offset > task size (Initiator only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_LUN,	/* HSI_COMMENT: iSCSI connection error - R2T unvalid LUN (Initiator only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_F_BIT_ZERO,	/* HSI_COMMENT: iSCSI connection error - All data has been already received, however it is not the end of sequence (Initiator only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_F_BIT_ZERO_S_BIT_ONE,	/* HSI_COMMENT: iSCSI connection error - S-bit and final bit = 1 (Initiator only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_EXP_STAT_SN,	/* HSI_COMMENT: iSCSI connection error - STAT SN error (Initiator only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_DSL_NOT_ZERO,	/* HSI_COMMENT: iSCSI connection error - TMF or LOGOUT PDUs dsl > 0 (Initiator only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_INVALID_DSL,	/* HSI_COMMENT: iSCSI connection error - CMD PDU dsl>0 while immediate data is disabled (Target only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_DATA_SEG_LEN_TOO_BIG,	/* HSI_COMMENT: iSCSI connection error - Data In overrun (Initiator only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_OUTSTANDING_R2T_COUNT,	/* HSI_COMMENT: iSCSI connection error - >1 outstanding R2T (Initiator only)  */
	ISCSI_CONN_ERROR_PROTOCOL_ERR_DIF_TX,	/* HSI_COMMENT: iSCSI connection error - DIF Tx error + DIF error drop is enabled (Target only)  */
	ISCSI_CONN_ERROR_SENSE_DATA_LENGTH,	/* HSI_COMMENT: iSCSI connection error - Sense data length > 256 (Initiator only)  */
	ISCSI_CONN_ERROR_DATA_PLACEMENT_ERROR,	/* HSI_COMMENT: iSCSI connection error - Data placement error  */
	ISCSI_CONN_ERROR_INVALID_ITT,	/* HSI_COMMENT: iSCSI connection error - Invalid ITT (Target Only) */
	ISCSI_ERROR_UNKNOWN,	/* HSI_COMMENT: iSCSI connection error  */
	MAX_ISCSI_ERROR_TYPES
};

/* iSCSI Ramrod Command IDs  */
enum iscsi_ramrod_cmd_id {
	ISCSI_RAMROD_CMD_ID_UNUSED = 0,
	ISCSI_RAMROD_CMD_ID_INIT_FUNC = 1,	/* HSI_COMMENT: iSCSI function init Ramrod */
	ISCSI_RAMROD_CMD_ID_DESTROY_FUNC = 2,	/* HSI_COMMENT: iSCSI function destroy Ramrod */
	ISCSI_RAMROD_CMD_ID_OFFLOAD_CONN = 3,	/* HSI_COMMENT: iSCSI connection offload Ramrod */
	ISCSI_RAMROD_CMD_ID_UPDATE_CONN = 4,	/* HSI_COMMENT: iSCSI connection update Ramrod  */
	ISCSI_RAMROD_CMD_ID_TERMINATION_CONN = 5,	/* HSI_COMMENT: iSCSI connection offload Ramrod. Command ID known only to FW and VBD */
	ISCSI_RAMROD_CMD_ID_CLEAR_SQ = 6,	/* HSI_COMMENT: iSCSI connection clear-sq ramrod.  */
	ISCSI_RAMROD_CMD_ID_MAC_UPDATE = 7,	/* HSI_COMMENT: iSCSI connection update MAC address ramrod.  */
	ISCSI_RAMROD_CMD_ID_CONN_STATS = 8,	/* HSI_COMMENT: iSCSI collect connection statistics ramrod.  */
	MAX_ISCSI_RAMROD_CMD_ID
};

struct iscsi_reg1 {
	__le32 reg1_map;
#define ISCSI_REG1_NUM_SGES_MASK        0xF	/* HSI_COMMENT: Written to R2tQE */
#define ISCSI_REG1_NUM_SGES_SHIFT       0
#define ISCSI_REG1_RESERVED1_MASK       0xFFFFFFF	/* HSI_COMMENT: reserved */
#define ISCSI_REG1_RESERVED1_SHIFT      4
};

/* Union of data/r2t sequence number */
union iscsi_seq_num {
	__le16 data_sn;		/* HSI_COMMENT: data-in sequence number */
	__le16 r2t_sn;		/* HSI_COMMENT: r2t pdu sequence number */
};

/* ISCSI connection termination request */
struct iscsi_spe_conn_mac_update {
	__le16 reserved0;	/* HSI_COMMENT: reserved. */
	__le16 conn_id;		/* HSI_COMMENT: ISCSI Connection ID. */
	__le32 reserved1;	/* HSI_COMMENT: reserved. */
	__le16 remote_mac_addr_lo;	/* HSI_COMMENT: new remote mac address lo */
	__le16 remote_mac_addr_mid;	/* HSI_COMMENT: new remote mac address mid */
	__le16 remote_mac_addr_hi;	/* HSI_COMMENT: new remote mac address hi */
	u8 reserved2[2];
};

/* ISCSI and TCP connection(Option 1) offload params passed by driver to FW in ISCSI offload ramrod  */
struct iscsi_spe_conn_offload {
	__le16 reserved0;	/* HSI_COMMENT: reserved. */
	__le16 conn_id;		/* HSI_COMMENT: ISCSI Connection ID. */
	__le32 reserved1;	/* HSI_COMMENT: reserved. */
	struct iscsi_conn_offload_params iscsi;	/* HSI_COMMENT: iSCSI session offload params */
	struct tcp_offload_params tcp;	/* HSI_COMMENT: iSCSI session offload params */
};

/* ISCSI and TCP connection(Option 2) offload params passed by driver to FW in ISCSI offload ramrod  */
struct iscsi_spe_conn_offload_option2 {
	__le16 reserved0;	/* HSI_COMMENT: reserved. */
	__le16 conn_id;		/* HSI_COMMENT: ISCSI Connection ID. */
	__le32 reserved1;	/* HSI_COMMENT: reserved. */
	struct iscsi_conn_offload_params iscsi;	/* HSI_COMMENT: iSCSI session offload params */
	struct tcp_offload_params_opt2 tcp;	/* HSI_COMMENT: iSCSI session offload params */
};

/* ISCSI collect connection statistics request */
struct iscsi_spe_conn_statistics {
	__le16 reserved0;	/* HSI_COMMENT: reserved. */
	__le16 conn_id;		/* HSI_COMMENT: ISCSI Connection ID. */
	__le32 reserved1;	/* HSI_COMMENT: reserved. */
	u8 reset_stats;		/* HSI_COMMENT: Indicates whether to reset the connection statistics. */
	u8 reserved2[7];
	struct regpair stats_cnts_addr;	/* HSI_COMMENT: cmdq and unsolicited counters termination params */
};

/* ISCSI connection termination request */
struct iscsi_spe_conn_termination {
	__le16 reserved0;	/* HSI_COMMENT: reserved. */
	__le16 conn_id;		/* HSI_COMMENT: ISCSI Connection ID. */
	__le32 reserved1;	/* HSI_COMMENT: reserved. */
	u8 abortive;		/* HSI_COMMENT: Mark termination as abort(reset) flow */
	u8 reserved2[7];
	struct regpair queue_cnts_addr;	/* HSI_COMMENT: cmdq and unsolicited counters termination params */
	struct regpair query_params_addr;	/* HSI_COMMENT: query_params_ptr */
};

/* iSCSI firmware function init parameters  */
struct iscsi_spe_func_init {
	__le16 half_way_close_timeout;	/* HSI_COMMENT: Half Way Close Timeout in Option 2 Close */
	u8 num_sq_pages_in_ring;	/* HSI_COMMENT: Number of entries in the SQ PBL. Provided by driver at function init spe */
	u8 num_r2tq_pages_in_ring;	/* HSI_COMMENT: Number of entries in the R2TQ PBL. Provided by driver at function init spe */
	u8 num_uhq_pages_in_ring;	/* HSI_COMMENT: Number of entries in the uHQ PBL (xHQ entries is X2). Provided by driver at function init spe */
	u8 ll2_rx_queue_id;	/* HSI_COMMENT: Queue ID of the Light-L2 Rx Queue */
	u8 flags;
#define ISCSI_SPE_FUNC_INIT_COUNTERS_EN_MASK    0x1	/* HSI_COMMENT: Enable counters - function and connection counters */
#define ISCSI_SPE_FUNC_INIT_COUNTERS_EN_SHIFT   0
#define ISCSI_SPE_FUNC_INIT_NVMETCP_MODE_MASK   0x1	/* HSI_COMMENT: NVMe TCP Mode */
#define ISCSI_SPE_FUNC_INIT_NVMETCP_MODE_SHIFT  1
#define ISCSI_SPE_FUNC_INIT_RESERVED0_MASK      0x3F	/* HSI_COMMENT: reserved */
#define ISCSI_SPE_FUNC_INIT_RESERVED0_SHIFT     2
	struct iscsi_debug_modes debug_mode;	/* HSI_COMMENT: Use iscsi_debug_mode flags */
	u8 params;
#define ISCSI_SPE_FUNC_INIT_MAX_SYN_RT_MASK     0xF	/* HSI_COMMENT: Maximum syn retransmissions */
#define ISCSI_SPE_FUNC_INIT_MAX_SYN_RT_SHIFT    0
#define ISCSI_SPE_FUNC_INIT_RESERVED1_MASK      0xF	/* HSI_COMMENT: reserved */
#define ISCSI_SPE_FUNC_INIT_RESERVED1_SHIFT     4
	u8 reserved2[7];
	struct scsi_init_func_params func_params;	/* HSI_COMMENT: Common SCSI init params passed by driver to FW in function init ramrod */
	struct scsi_init_func_queues q_params;	/* HSI_COMMENT: SCSI RQ/CQ firmware function init parameters */
	struct iscsi_asserts_types iscsi_asserts;
	u8 reserved3[4];	/* HSI_COMMENT: 128 Bits Alignment */
};

/* The iscsi storm task context of Ystorm */
struct ystorm_iscsi_task_state {
	struct scsi_cached_sges data_desc;
	struct scsi_sgl_params sgl_params;
	__le32 exp_r2t_sn;	/* HSI_COMMENT: Initiator mode - Expected R2T PDU index in sequence. [variable, initialized 0] */
	__le32 buffer_offset;	/* HSI_COMMENT: Payload data offset */
	union iscsi_seq_num seq_num;	/* HSI_COMMENT: PDU index in sequence */
	struct iscsi_dif_flags dif_flags;	/* HSI_COMMENT: Dif flags */
	u8 flags;
#define YSTORM_ISCSI_TASK_STATE_LOCAL_COMP_MASK 0x1	/* HSI_COMMENT: local_completion  */
#define YSTORM_ISCSI_TASK_STATE_LOCAL_COMP_SHIFT        0
#define YSTORM_ISCSI_TASK_STATE_SLOW_IO_MASK    0x1	/* HSI_COMMENT: Equals 1 if SGL is predicted and 0 otherwise. */
#define YSTORM_ISCSI_TASK_STATE_SLOW_IO_SHIFT   1
#define YSTORM_ISCSI_TASK_STATE_SET_DIF_OFFSET_MASK     0x1	/* HSI_COMMENT: Indication for Ystorm that TDIFs offsetInIo is not synced with buffer_offset */
#define YSTORM_ISCSI_TASK_STATE_SET_DIF_OFFSET_SHIFT    2
#define YSTORM_ISCSI_TASK_STATE_RESERVED0_MASK  0x1F
#define YSTORM_ISCSI_TASK_STATE_RESERVED0_SHIFT 3
};

/* The iscsi storm task context of Ystorm */
struct ystorm_iscsi_task_rxmit_opt {
	__le32 fast_rxmit_sge_offset;	/* HSI_COMMENT: SGE offset from which to continue dummy-read or start fast retransmit */
	__le32 scan_start_buffer_offset;	/* HSI_COMMENT: Starting buffer offset of next retransmit SGL scan */
	__le32 fast_rxmit_buffer_offset;	/* HSI_COMMENT: Buffer offset from which to continue dummy-read or start fast retransmit */
	u8 scan_start_sgl_index;	/* HSI_COMMENT: Starting SGL index of next retransmit SGL scan */
	u8 fast_rxmit_sgl_index;	/* HSI_COMMENT: SGL index from which to continue dummy-read or start fast retransmit */
	__le16 reserved;	/* HSI_COMMENT: reserved */
};

/* The iscsi storm task context of Ystorm */
struct ystorm_iscsi_task_st_ctx {
	struct ystorm_iscsi_task_state state;	/* HSI_COMMENT: iSCSI task parameters and state */
	struct ystorm_iscsi_task_rxmit_opt rxmit_opt;	/* HSI_COMMENT: iSCSI retransmit optimizations parameters */
	union iscsi_task_hdr pdu_hdr;	/* HSI_COMMENT: PDU header - [constant initialized] */
};

struct ystorm_iscsi_task_ag_ctx {
	u8 reserved;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	__le16 word0;		/* HSI_COMMENT: icid */
	u8 flags0;
#define YSTORM_ISCSI_TASK_AG_CTX_NIBBLE0_MASK   0xF	/* HSI_COMMENT: connection_type */
#define YSTORM_ISCSI_TASK_AG_CTX_NIBBLE0_SHIFT  0
#define YSTORM_ISCSI_TASK_AG_CTX_BIT0_MASK      0x1	/* HSI_COMMENT: exist_in_qm0 */
#define YSTORM_ISCSI_TASK_AG_CTX_BIT0_SHIFT     4
#define YSTORM_ISCSI_TASK_AG_CTX_BIT1_MASK      0x1	/* HSI_COMMENT: exist_in_qm1 */
#define YSTORM_ISCSI_TASK_AG_CTX_BIT1_SHIFT     5
#define YSTORM_ISCSI_TASK_AG_CTX_VALID_MASK     0x1	/* HSI_COMMENT: bit2 */
#define YSTORM_ISCSI_TASK_AG_CTX_VALID_SHIFT    6
#define YSTORM_ISCSI_TASK_AG_CTX_TTT_VALID_MASK 0x1	/* HSI_COMMENT: bit3 */
#define YSTORM_ISCSI_TASK_AG_CTX_TTT_VALID_SHIFT        7
	u8 flags1;
#define YSTORM_ISCSI_TASK_AG_CTX_CF0_MASK       0x3	/* HSI_COMMENT: cf0 */
#define YSTORM_ISCSI_TASK_AG_CTX_CF0_SHIFT      0
#define YSTORM_ISCSI_TASK_AG_CTX_CF1_MASK       0x3	/* HSI_COMMENT: cf1 */
#define YSTORM_ISCSI_TASK_AG_CTX_CF1_SHIFT      2
#define YSTORM_ISCSI_TASK_AG_CTX_CF2SPECIAL_MASK        0x3	/* HSI_COMMENT: cf2special */
#define YSTORM_ISCSI_TASK_AG_CTX_CF2SPECIAL_SHIFT       4
#define YSTORM_ISCSI_TASK_AG_CTX_CF0EN_MASK     0x1	/* HSI_COMMENT: cf0en */
#define YSTORM_ISCSI_TASK_AG_CTX_CF0EN_SHIFT    6
#define YSTORM_ISCSI_TASK_AG_CTX_CF1EN_MASK     0x1	/* HSI_COMMENT: cf1en */
#define YSTORM_ISCSI_TASK_AG_CTX_CF1EN_SHIFT    7
	u8 flags2;
#define YSTORM_ISCSI_TASK_AG_CTX_BIT4_MASK      0x1	/* HSI_COMMENT: bit4 */
#define YSTORM_ISCSI_TASK_AG_CTX_BIT4_SHIFT     0
#define YSTORM_ISCSI_TASK_AG_CTX_RULE0EN_MASK   0x1	/* HSI_COMMENT: rule0en */
#define YSTORM_ISCSI_TASK_AG_CTX_RULE0EN_SHIFT  1
#define YSTORM_ISCSI_TASK_AG_CTX_RULE1EN_MASK   0x1	/* HSI_COMMENT: rule1en */
#define YSTORM_ISCSI_TASK_AG_CTX_RULE1EN_SHIFT  2
#define YSTORM_ISCSI_TASK_AG_CTX_RULE2EN_MASK   0x1	/* HSI_COMMENT: rule2en */
#define YSTORM_ISCSI_TASK_AG_CTX_RULE2EN_SHIFT  3
#define YSTORM_ISCSI_TASK_AG_CTX_RULE3EN_MASK   0x1	/* HSI_COMMENT: rule3en */
#define YSTORM_ISCSI_TASK_AG_CTX_RULE3EN_SHIFT  4
#define YSTORM_ISCSI_TASK_AG_CTX_RULE4EN_MASK   0x1	/* HSI_COMMENT: rule4en */
#define YSTORM_ISCSI_TASK_AG_CTX_RULE4EN_SHIFT  5
#define YSTORM_ISCSI_TASK_AG_CTX_RULE5EN_MASK   0x1	/* HSI_COMMENT: rule5en */
#define YSTORM_ISCSI_TASK_AG_CTX_RULE5EN_SHIFT  6
#define YSTORM_ISCSI_TASK_AG_CTX_RULE6EN_MASK   0x1	/* HSI_COMMENT: rule6en */
#define YSTORM_ISCSI_TASK_AG_CTX_RULE6EN_SHIFT  7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	__le32 TTT;		/* HSI_COMMENT: reg0 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	__le16 word1;		/* HSI_COMMENT: word1 */
};

struct mstorm_iscsi_task_ag_ctx {
	u8 cdu_validation;	/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	__le16 task_cid;	/* HSI_COMMENT: icid */
	u8 flags0;
#define MSTORM_ISCSI_TASK_AG_CTX_CONNECTION_TYPE_MASK   0xF	/* HSI_COMMENT: connection_type */
#define MSTORM_ISCSI_TASK_AG_CTX_CONNECTION_TYPE_SHIFT  0
#define MSTORM_ISCSI_TASK_AG_CTX_EXIST_IN_QM0_MASK      0x1	/* HSI_COMMENT: exist_in_qm0 */
#define MSTORM_ISCSI_TASK_AG_CTX_EXIST_IN_QM0_SHIFT     4
#define MSTORM_ISCSI_TASK_AG_CTX_CONN_CLEAR_SQ_FLAG_MASK        0x1	/* HSI_COMMENT: exist_in_qm1 */
#define MSTORM_ISCSI_TASK_AG_CTX_CONN_CLEAR_SQ_FLAG_SHIFT       5
#define MSTORM_ISCSI_TASK_AG_CTX_VALID_MASK     0x1	/* HSI_COMMENT: bit2 */
#define MSTORM_ISCSI_TASK_AG_CTX_VALID_SHIFT    6
#define MSTORM_ISCSI_TASK_AG_CTX_TASK_CLEANUP_FLAG_MASK 0x1	/* HSI_COMMENT: bit3 */
#define MSTORM_ISCSI_TASK_AG_CTX_TASK_CLEANUP_FLAG_SHIFT        7
	u8 flags1;
#define MSTORM_ISCSI_TASK_AG_CTX_TASK_CLEANUP_CF_MASK   0x3	/* HSI_COMMENT: cf0 */
#define MSTORM_ISCSI_TASK_AG_CTX_TASK_CLEANUP_CF_SHIFT  0
#define MSTORM_ISCSI_TASK_AG_CTX_CF1_MASK       0x3	/* HSI_COMMENT: cf1 */
#define MSTORM_ISCSI_TASK_AG_CTX_CF1_SHIFT      2
#define MSTORM_ISCSI_TASK_AG_CTX_CF2_MASK       0x3	/* HSI_COMMENT: cf2 */
#define MSTORM_ISCSI_TASK_AG_CTX_CF2_SHIFT      4
#define MSTORM_ISCSI_TASK_AG_CTX_TASK_CLEANUP_CF_EN_MASK        0x1	/* HSI_COMMENT: cf0en */
#define MSTORM_ISCSI_TASK_AG_CTX_TASK_CLEANUP_CF_EN_SHIFT       6
#define MSTORM_ISCSI_TASK_AG_CTX_CF1EN_MASK     0x1	/* HSI_COMMENT: cf1en */
#define MSTORM_ISCSI_TASK_AG_CTX_CF1EN_SHIFT    7
	u8 flags2;
#define MSTORM_ISCSI_TASK_AG_CTX_CF2EN_MASK     0x1	/* HSI_COMMENT: cf2en */
#define MSTORM_ISCSI_TASK_AG_CTX_CF2EN_SHIFT    0
#define MSTORM_ISCSI_TASK_AG_CTX_RULE0EN_MASK   0x1	/* HSI_COMMENT: rule0en */
#define MSTORM_ISCSI_TASK_AG_CTX_RULE0EN_SHIFT  1
#define MSTORM_ISCSI_TASK_AG_CTX_RULE1EN_MASK   0x1	/* HSI_COMMENT: rule1en */
#define MSTORM_ISCSI_TASK_AG_CTX_RULE1EN_SHIFT  2
#define MSTORM_ISCSI_TASK_AG_CTX_RULE2EN_MASK   0x1	/* HSI_COMMENT: rule2en */
#define MSTORM_ISCSI_TASK_AG_CTX_RULE2EN_SHIFT  3
#define MSTORM_ISCSI_TASK_AG_CTX_RULE3EN_MASK   0x1	/* HSI_COMMENT: rule3en */
#define MSTORM_ISCSI_TASK_AG_CTX_RULE3EN_SHIFT  4
#define MSTORM_ISCSI_TASK_AG_CTX_RULE4EN_MASK   0x1	/* HSI_COMMENT: rule4en */
#define MSTORM_ISCSI_TASK_AG_CTX_RULE4EN_SHIFT  5
#define MSTORM_ISCSI_TASK_AG_CTX_RULE5EN_MASK   0x1	/* HSI_COMMENT: rule5en */
#define MSTORM_ISCSI_TASK_AG_CTX_RULE5EN_SHIFT  6
#define MSTORM_ISCSI_TASK_AG_CTX_RULE6EN_MASK   0x1	/* HSI_COMMENT: rule6en */
#define MSTORM_ISCSI_TASK_AG_CTX_RULE6EN_SHIFT  7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	__le16 word1;		/* HSI_COMMENT: word1 */
};

struct ustorm_iscsi_task_ag_ctx {
	u8 reserved;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	__le16 icid;		/* HSI_COMMENT: icid */
	u8 flags0;
#define USTORM_ISCSI_TASK_AG_CTX_CONNECTION_TYPE_MASK   0xF	/* HSI_COMMENT: connection_type */
#define USTORM_ISCSI_TASK_AG_CTX_CONNECTION_TYPE_SHIFT  0
#define USTORM_ISCSI_TASK_AG_CTX_EXIST_IN_QM0_MASK      0x1	/* HSI_COMMENT: exist_in_qm0 */
#define USTORM_ISCSI_TASK_AG_CTX_EXIST_IN_QM0_SHIFT     4
#define USTORM_ISCSI_TASK_AG_CTX_CONN_CLEAR_SQ_FLAG_MASK        0x1	/* HSI_COMMENT: exist_in_qm1 */
#define USTORM_ISCSI_TASK_AG_CTX_CONN_CLEAR_SQ_FLAG_SHIFT       5
#define USTORM_ISCSI_TASK_AG_CTX_HQ_SCANNED_CF_MASK     0x3	/* HSI_COMMENT: timer0cf */
#define USTORM_ISCSI_TASK_AG_CTX_HQ_SCANNED_CF_SHIFT    6
	u8 flags1;
#define USTORM_ISCSI_TASK_AG_CTX_RESERVED1_MASK 0x3	/* HSI_COMMENT: timer1cf */
#define USTORM_ISCSI_TASK_AG_CTX_RESERVED1_SHIFT        0
#define USTORM_ISCSI_TASK_AG_CTX_R2T2RECV_MASK  0x3	/* HSI_COMMENT: timer2cf */
#define USTORM_ISCSI_TASK_AG_CTX_R2T2RECV_SHIFT 2
#define USTORM_ISCSI_TASK_AG_CTX_CF3_MASK       0x3	/* HSI_COMMENT: timer_stop_all */
#define USTORM_ISCSI_TASK_AG_CTX_CF3_SHIFT      4
#define USTORM_ISCSI_TASK_AG_CTX_DIF_ERROR_CF_MASK      0x3	/* HSI_COMMENT: cf4 */
#define USTORM_ISCSI_TASK_AG_CTX_DIF_ERROR_CF_SHIFT     6
	u8 flags2;
#define USTORM_ISCSI_TASK_AG_CTX_HQ_SCANNED_CF_EN_MASK  0x1	/* HSI_COMMENT: cf0en */
#define USTORM_ISCSI_TASK_AG_CTX_HQ_SCANNED_CF_EN_SHIFT 0
#define USTORM_ISCSI_TASK_AG_CTX_DISABLE_DATA_ACKED_MASK        0x1	/* HSI_COMMENT: cf1en */
#define USTORM_ISCSI_TASK_AG_CTX_DISABLE_DATA_ACKED_SHIFT       1
#define USTORM_ISCSI_TASK_AG_CTX_R2T2RECV_EN_MASK       0x1	/* HSI_COMMENT: cf2en */
#define USTORM_ISCSI_TASK_AG_CTX_R2T2RECV_EN_SHIFT      2
#define USTORM_ISCSI_TASK_AG_CTX_CF3EN_MASK     0x1	/* HSI_COMMENT: cf3en */
#define USTORM_ISCSI_TASK_AG_CTX_CF3EN_SHIFT    3
#define USTORM_ISCSI_TASK_AG_CTX_DIF_ERROR_CF_EN_MASK   0x1	/* HSI_COMMENT: cf4en */
#define USTORM_ISCSI_TASK_AG_CTX_DIF_ERROR_CF_EN_SHIFT  4
#define USTORM_ISCSI_TASK_AG_CTX_CMP_DATA_TOTAL_EXP_EN_MASK     0x1	/* HSI_COMMENT: rule0en */
#define USTORM_ISCSI_TASK_AG_CTX_CMP_DATA_TOTAL_EXP_EN_SHIFT    5
#define USTORM_ISCSI_TASK_AG_CTX_RULE1EN_MASK   0x1	/* HSI_COMMENT: rule1en */
#define USTORM_ISCSI_TASK_AG_CTX_RULE1EN_SHIFT  6
#define USTORM_ISCSI_TASK_AG_CTX_CMP_CONT_RCV_EXP_EN_MASK       0x1	/* HSI_COMMENT: rule2en */
#define USTORM_ISCSI_TASK_AG_CTX_CMP_CONT_RCV_EXP_EN_SHIFT      7
	u8 flags3;
#define USTORM_ISCSI_TASK_AG_CTX_RULE3EN_MASK   0x1	/* HSI_COMMENT: rule3en */
#define USTORM_ISCSI_TASK_AG_CTX_RULE3EN_SHIFT  0
#define USTORM_ISCSI_TASK_AG_CTX_RULE4EN_MASK   0x1	/* HSI_COMMENT: rule4en */
#define USTORM_ISCSI_TASK_AG_CTX_RULE4EN_SHIFT  1
#define USTORM_ISCSI_TASK_AG_CTX_RULE5EN_MASK   0x1	/* HSI_COMMENT: rule5en */
#define USTORM_ISCSI_TASK_AG_CTX_RULE5EN_SHIFT  2
#define USTORM_ISCSI_TASK_AG_CTX_RULE6EN_MASK   0x1	/* HSI_COMMENT: rule6en */
#define USTORM_ISCSI_TASK_AG_CTX_RULE6EN_SHIFT  3
#define USTORM_ISCSI_TASK_AG_CTX_DIF_ERROR_TYPE_MASK    0xF	/* HSI_COMMENT: nibble1 */
#define USTORM_ISCSI_TASK_AG_CTX_DIF_ERROR_TYPE_SHIFT   4
	__le32 dif_err_intervals;	/* HSI_COMMENT: reg0 */
	__le32 dif_error_1st_interval;	/* HSI_COMMENT: reg1 */
	__le32 rcv_cont_len;	/* HSI_COMMENT: reg2 */
	__le32 exp_cont_len;	/* HSI_COMMENT: reg3 */
	__le32 total_data_acked;	/* HSI_COMMENT: reg4 */
	__le32 exp_data_acked;	/* HSI_COMMENT: reg5 */
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le16 next_tid;	/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le32 hdr_residual_count;	/* HSI_COMMENT: reg6 */
	__le32 exp_r2t_sn;	/* HSI_COMMENT: reg7 */
};

/* The iscsi storm task context of Mstorm */
struct mstorm_iscsi_task_st_ctx {
	struct scsi_cached_sges data_desc;	/* HSI_COMMENT: Union of Data SGL / cached sge */
	struct scsi_sgl_params sgl_params;
	__le32 rem_task_size;	/* HSI_COMMENT: Remaining task size, used for placement verification */
	__le32 data_buffer_offset;	/* HSI_COMMENT: Buffer offset */
	u8 task_type;		/* HSI_COMMENT: Task type, (use: iscsi_task_type enum) */
	struct iscsi_dif_flags dif_flags;	/* HSI_COMMENT: sizes of host/peer protection intervals + protection log interval */
	__le16 dif_task_icid;	/* HSI_COMMENT: save tasks CID for validation - dif on immediate flow */
	struct regpair sense_db;	/* HSI_COMMENT: Pointer to sense data buffer */
	__le32 expected_itt;	/* HSI_COMMENT: ITT - for target mode validations */
	__le32 reserved1;	/* HSI_COMMENT: reserved1 */
};

struct tqe_opaque {
	__le16 opaque[2];	/* HSI_COMMENT: TQe opaque */
};

/* The iscsi storm task context of Ustorm */
struct ustorm_iscsi_task_st_ctx {
	__le32 rem_rcv_len;	/* HSI_COMMENT: Remaining data to be received in bytes. Used in validations */
	__le32 exp_data_transfer_len;	/* HSI_COMMENT: iSCSI Initiator - The size of the transmitted task, iSCSI Target - the size of the Rx continuation */
	__le32 exp_data_sn;	/* HSI_COMMENT: Expected data SN */
	struct regpair lun;	/* HSI_COMMENT: LUN */
	struct iscsi_reg1 reg1;
	u8 flags2;
#define USTORM_ISCSI_TASK_ST_CTX_AHS_EXIST_MASK 0x1	/* HSI_COMMENT: Initiator Mode - Mark AHS exist */
#define USTORM_ISCSI_TASK_ST_CTX_AHS_EXIST_SHIFT        0
#define USTORM_ISCSI_TASK_ST_CTX_RESERVED1_MASK 0x7F
#define USTORM_ISCSI_TASK_ST_CTX_RESERVED1_SHIFT        1
	struct iscsi_dif_flags dif_flags;	/* HSI_COMMENT: Dif flags (written to R2T WQE) */
	__le16 reserved3;
	struct tqe_opaque tqe_opaque_list;
	__le32 reserved5;
	__le32 reserved6;
	__le32 reserved7;
	u8 task_type;		/* HSI_COMMENT: Task Type */
	u8 error_flags;
#define USTORM_ISCSI_TASK_ST_CTX_DATA_DIGEST_ERROR_MASK 0x1	/* HSI_COMMENT: Mark task with data digest error (1 bit) */
#define USTORM_ISCSI_TASK_ST_CTX_DATA_DIGEST_ERROR_SHIFT        0
#define USTORM_ISCSI_TASK_ST_CTX_DATA_TRUNCATED_ERROR_MASK      0x1	/* HSI_COMMENT: Target Mode - Mark middle task error, data truncated */
#define USTORM_ISCSI_TASK_ST_CTX_DATA_TRUNCATED_ERROR_SHIFT     1
#define USTORM_ISCSI_TASK_ST_CTX_UNDER_RUN_ERROR_MASK   0x1
#define USTORM_ISCSI_TASK_ST_CTX_UNDER_RUN_ERROR_SHIFT  2
#define USTORM_ISCSI_TASK_ST_CTX_NVME_TCP_MASK  0x1	/* HSI_COMMENT: NVMe/TCP context */
#define USTORM_ISCSI_TASK_ST_CTX_NVME_TCP_SHIFT 3
#define USTORM_ISCSI_TASK_ST_CTX_RESERVED8_MASK 0xF
#define USTORM_ISCSI_TASK_ST_CTX_RESERVED8_SHIFT        4
	u8 flags;
#define USTORM_ISCSI_TASK_ST_CTX_CQE_WRITE_MASK 0x3	/* HSI_COMMENT: mark task cqe write (for cleanup flow) */
#define USTORM_ISCSI_TASK_ST_CTX_CQE_WRITE_SHIFT        0
#define USTORM_ISCSI_TASK_ST_CTX_LOCAL_COMP_MASK        0x1	/* HSI_COMMENT: local completion bit */
#define USTORM_ISCSI_TASK_ST_CTX_LOCAL_COMP_SHIFT       2
#define USTORM_ISCSI_TASK_ST_CTX_Q0_R2TQE_WRITE_MASK    0x1	/* HSI_COMMENT: write R2TQE from Q0 flow */
#define USTORM_ISCSI_TASK_ST_CTX_Q0_R2TQE_WRITE_SHIFT   3
#define USTORM_ISCSI_TASK_ST_CTX_TOTAL_DATA_ACKED_DONE_MASK     0x1	/* HSI_COMMENT: Mark total data acked or disabled */
#define USTORM_ISCSI_TASK_ST_CTX_TOTAL_DATA_ACKED_DONE_SHIFT    4
#define USTORM_ISCSI_TASK_ST_CTX_HQ_SCANNED_DONE_MASK   0x1	/* HSI_COMMENT: Mark HQ scanned or disabled */
#define USTORM_ISCSI_TASK_ST_CTX_HQ_SCANNED_DONE_SHIFT  5
#define USTORM_ISCSI_TASK_ST_CTX_R2T2RECV_DONE_MASK     0x1	/* HSI_COMMENT: Mark HQ scanned or disabled */
#define USTORM_ISCSI_TASK_ST_CTX_R2T2RECV_DONE_SHIFT    6
#define USTORM_ISCSI_TASK_ST_CTX_RESERVED0_MASK 0x1
#define USTORM_ISCSI_TASK_ST_CTX_RESERVED0_SHIFT        7
	u8 cq_rss_number;	/* HSI_COMMENT: Task CQ_RSS number 0.63 */
};

/* iscsi task context */
struct iscsi_task_context {
	struct ystorm_iscsi_task_st_ctx ystorm_st_context;	/* HSI_COMMENT: ystorm storm context */
	struct ystorm_iscsi_task_ag_ctx ystorm_ag_context;	/* HSI_COMMENT: ystorm aggregative context */
	struct regpair ystorm_ag_padding[2];	/* HSI_COMMENT: padding */
	struct tdif_task_context tdif_context;	/* HSI_COMMENT: tdif context */
	struct mstorm_iscsi_task_ag_ctx mstorm_ag_context;	/* HSI_COMMENT: mstorm aggregative context */
	struct regpair mstorm_ag_padding[2];	/* HSI_COMMENT: padding */
	struct ustorm_iscsi_task_ag_ctx ustorm_ag_context;	/* HSI_COMMENT: ustorm aggregative context */
	struct mstorm_iscsi_task_st_ctx mstorm_st_context;	/* HSI_COMMENT: mstorm storm context */
	struct ustorm_iscsi_task_st_ctx ustorm_st_context;	/* HSI_COMMENT: ustorm storm context */
	struct rdif_task_context rdif_context;	/* HSI_COMMENT: rdif context */
};

/* iSCSI task type */
enum iscsi_task_type {
	ISCSI_TASK_TYPE_INITIATOR_WRITE,
	ISCSI_TASK_TYPE_INITIATOR_READ,
	ISCSI_TASK_TYPE_MIDPATH,
	ISCSI_TASK_TYPE_UNSOLIC,
	ISCSI_TASK_TYPE_EXCHCLEANUP,
	ISCSI_TASK_TYPE_IRRELEVANT,
	ISCSI_TASK_TYPE_TARGET_WRITE,
	ISCSI_TASK_TYPE_TARGET_READ,
	ISCSI_TASK_TYPE_TARGET_RESPONSE,
	ISCSI_TASK_TYPE_LOGIN_RESPONSE,
	ISCSI_TASK_TYPE_TARGET_IMM_W_DIF,
	MAX_ISCSI_TASK_TYPE
};

/* iSCSI DesiredDataTransferLength/ttt union */
union iscsi_ttt_txlen_union {
	__le32 desired_tx_len;	/* HSI_COMMENT: desired data transfer length */
	__le32 ttt;		/* HSI_COMMENT: target transfer tag */
};

/* iSCSI uHQ element */
struct iscsi_uhqe {
	__le32 reg1;
#define ISCSI_UHQE_PDU_PAYLOAD_LEN_MASK 0xFFFFF	/* HSI_COMMENT: iSCSI payload (doesnt include padding or digest) or AHS length */
#define ISCSI_UHQE_PDU_PAYLOAD_LEN_SHIFT        0
#define ISCSI_UHQE_LOCAL_COMP_MASK      0x1	/* HSI_COMMENT: local compleiton flag */
#define ISCSI_UHQE_LOCAL_COMP_SHIFT     20
#define ISCSI_UHQE_TOGGLE_BIT_MASK      0x1	/* HSI_COMMENT: toggle bit to protect from uHQ full */
#define ISCSI_UHQE_TOGGLE_BIT_SHIFT     21
#define ISCSI_UHQE_PURE_PAYLOAD_MASK    0x1	/* HSI_COMMENT: indicates whether pdu_payload_len contains pure payload length. if not, pdu_payload_len is AHS length */
#define ISCSI_UHQE_PURE_PAYLOAD_SHIFT   22
#define ISCSI_UHQE_LOGIN_RESPONSE_PDU_MASK      0x1	/* HSI_COMMENT: indicates login pdu */
#define ISCSI_UHQE_LOGIN_RESPONSE_PDU_SHIFT     23
#define ISCSI_UHQE_TASK_ID_HI_MASK      0xFF	/* HSI_COMMENT: most significant byte of task_id */
#define ISCSI_UHQE_TASK_ID_HI_SHIFT     24
	__le32 reg2;
#define ISCSI_UHQE_BUFFER_OFFSET_MASK   0xFFFFFF	/* HSI_COMMENT: absolute offset in task */
#define ISCSI_UHQE_BUFFER_OFFSET_SHIFT  0
#define ISCSI_UHQE_TASK_ID_LO_MASK      0xFF	/* HSI_COMMENT: least significant byte of task_id */
#define ISCSI_UHQE_TASK_ID_LO_SHIFT     24
};

/* iSCSI WQ element  */
struct iscsi_wqe {
	__le16 task_id;		/* HSI_COMMENT: The task identifier (itt) includes all the relevant information required for the task processing */
	u8 flags;
#define ISCSI_WQE_WQE_TYPE_MASK 0x7	/* HSI_COMMENT: Wqe type [use iscsi_wqe_type] */
#define ISCSI_WQE_WQE_TYPE_SHIFT        0
#define ISCSI_WQE_NUM_SGES_MASK 0xF	/* HSI_COMMENT: The driver will give a hint about sizes of SGEs for better credits evaluation at Xstorm */
#define ISCSI_WQE_NUM_SGES_SHIFT        3
#define ISCSI_WQE_RESPONSE_MASK 0x1	/* HSI_COMMENT: 1 if this Wqe triggers a response and advances stat_sn, 0 otherwise */
#define ISCSI_WQE_RESPONSE_SHIFT        7
	struct iscsi_dif_flags prot_flags;	/* HSI_COMMENT: Task data-integrity flags (protection) */
	__le32 contlen_cdbsize;
#define ISCSI_WQE_CONT_LEN_MASK 0xFFFFFF	/* HSI_COMMENT: expected/desired data transfer length */
#define ISCSI_WQE_CONT_LEN_SHIFT        0
#define ISCSI_WQE_CDB_SIZE_MASK 0xFF	/* HSI_COMMENT: Initiator mode only: equals SCSI command CDB size if extended CDB is used, otherwise equals zero.  */
#define ISCSI_WQE_CDB_SIZE_SHIFT        24
};

/* iSCSI wqe type  */
enum iscsi_wqe_type {
	ISCSI_WQE_TYPE_NORMAL,	/* HSI_COMMENT: iSCSI WQE type normal. excluding status bit in target mode. */
	ISCSI_WQE_TYPE_TASK_CLEANUP,	/* HSI_COMMENT: iSCSI WQE type task cleanup */
	ISCSI_WQE_TYPE_MIDDLE_PATH,	/* HSI_COMMENT: iSCSI WQE type middle path */
	ISCSI_WQE_TYPE_LOGIN,	/* HSI_COMMENT: iSCSI WQE type login */
	ISCSI_WQE_TYPE_FIRST_R2T_CONT,	/* HSI_COMMENT: iSCSI WQE type First Write Continuation (Target) */
	ISCSI_WQE_TYPE_NONFIRST_R2T_CONT,	/* HSI_COMMENT: iSCSI WQE type Non-First Write Continuation (Target) */
	ISCSI_WQE_TYPE_RESPONSE,	/* HSI_COMMENT: iSCSI WQE type SCSI response */
	MAX_ISCSI_WQE_TYPE
};

/* iSCSI xHQ element */
struct iscsi_xhqe {
	union iscsi_ttt_txlen_union ttt_or_txlen;	/* HSI_COMMENT: iSCSI DesiredDataTransferLength/ttt union */
	__le32 exp_stat_sn;	/* HSI_COMMENT: expected StatSn */
	struct iscsi_dif_flags prot_flags;	/* HSI_COMMENT: Task data-integrity flags (protection) */
	u8 total_ahs_length;	/* HSI_COMMENT: Initiator mode only: Total AHS Length. greater than zero if and only if PDU is SCSI command and CDB > 16 */
	u8 opcode;		/* HSI_COMMENT: Type opcode for command PDU */
	u8 flags;
#define ISCSI_XHQE_FINAL_MASK   0x1	/* HSI_COMMENT: The Final(F) for this PDU */
#define ISCSI_XHQE_FINAL_SHIFT  0
#define ISCSI_XHQE_STATUS_BIT_MASK      0x1	/* HSI_COMMENT: Whether this PDU is Data-In PDU with status_bit = 1 */
#define ISCSI_XHQE_STATUS_BIT_SHIFT     1
#define ISCSI_XHQE_NUM_SGES_MASK        0xF	/* HSI_COMMENT: If Predicted IO equals Min(8, number of SGEs in SGL), otherwise equals 0 */
#define ISCSI_XHQE_NUM_SGES_SHIFT       2
#define ISCSI_XHQE_RESERVED0_MASK       0x3	/* HSI_COMMENT: reserved */
#define ISCSI_XHQE_RESERVED0_SHIFT      6
	union iscsi_seq_num seq_num;	/* HSI_COMMENT: R2T/DataSN sequence number */
	__le16 reserved1;
};

/* Per PF iSCSI receive path statistics - mStorm RAM structure */
struct mstorm_iscsi_stats_drv {
	struct regpair iscsi_rx_dropped_PDUs_task_not_valid;	/* HSI_COMMENT: Number of Rx silently dropped PDUs due to task not valid */
	struct regpair iscsi_rx_dup_ack_cnt;	/* HSI_COMMENT: Received Dup-ACKs - after 3 dup ack, the counter doesnt count the same dup ack */
};

/* Per PF iSCSI transmit path statistics - pStorm RAM structure */
struct pstorm_iscsi_stats_drv {
	struct regpair iscsi_tx_bytes_cnt;	/* HSI_COMMENT: Counts the number of tx bytes that were transmitted */
	struct regpair iscsi_tx_packet_cnt;	/* HSI_COMMENT: Counts the number of tx packets that were transmitted */
};

/* Per PF iSCSI receive path statistics - tStorm RAM structure */
struct tstorm_iscsi_stats_drv {
	struct regpair iscsi_rx_bytes_cnt;	/* HSI_COMMENT: Counts the number of rx bytes that were received */
	struct regpair iscsi_rx_packet_cnt;	/* HSI_COMMENT: Counts the number of rx packets that were received */
	struct regpair iscsi_rx_new_ooo_isle_events_cnt;	/* HSI_COMMENT: Counts the number of new out-of-order isle event */
	struct regpair iscsi_rx_tcp_payload_bytes_cnt;	/* HSI_COMMENT: Received In-Order TCP Payload Bytes */
	struct regpair iscsi_rx_tcp_pkt_cnt;	/* HSI_COMMENT: Received In-Order TCP Packets */
	struct regpair iscsi_rx_pure_ack_cnt;	/* HSI_COMMENT: Received Pure-ACKs */
	__le32 iscsi_cmdq_threshold_cnt;	/* HSI_COMMENT: Counts the number of times elements in cmdQ reached threshold */
	__le32 iscsi_rq_threshold_cnt;	/* HSI_COMMENT: Counts the number of times elements in RQQ reached threshold */
	__le32 iscsi_immq_threshold_cnt;	/* HSI_COMMENT: Counts the number of times elements in immQ reached threshold */
};

/* Per PF iSCSI receive path statistics - uStorm RAM structure */
struct ustorm_iscsi_stats_drv {
	struct regpair iscsi_rx_data_pdu_cnt;	/* HSI_COMMENT: Number of data PDUs that were received */
	struct regpair iscsi_rx_r2t_pdu_cnt;	/* HSI_COMMENT: Number of R2T PDUs that were received */
	struct regpair iscsi_rx_total_pdu_cnt;	/* HSI_COMMENT: Number of total PDUs that were received */
};

/* Per PF iSCSI transmit path statistics - xStorm RAM structure */
struct xstorm_iscsi_stats_drv {
	struct regpair iscsi_tx_go_to_slow_start_event_cnt;	/* HSI_COMMENT: Number of times slow start event occurred */
	struct regpair iscsi_tx_fast_retransmit_event_cnt;	/* HSI_COMMENT: Number of times fast retransmit event occurred */
	struct regpair iscsi_tx_pure_ack_cnt;	/* HSI_COMMENT: Transmitted Pure-ACKs */
	struct regpair iscsi_tx_delayed_ack_cnt;	/* HSI_COMMENT: Transmitted Delayed ACKs */
};

/* Per PF iSCSI transmit path statistics - yStorm RAM structure */
struct ystorm_iscsi_stats_drv {
	struct regpair iscsi_tx_data_pdu_cnt;	/* HSI_COMMENT: Number of data PDUs that were transmitted */
	struct regpair iscsi_tx_r2t_pdu_cnt;	/* HSI_COMMENT: Number of R2T PDUs that were transmitted */
	struct regpair iscsi_tx_total_pdu_cnt;	/* HSI_COMMENT: Number of total PDUs that were transmitted */
	struct regpair iscsi_tx_tcp_payload_bytes_cnt;	/* HSI_COMMENT: Transmitted In-Order TCP Payload Bytes */
	struct regpair iscsi_tx_tcp_pkt_cnt;	/* HSI_COMMENT: Transmitted In-Order TCP Packets */
};

/* iSCSI doorbell data */
struct iscsi_db_data {
	u8 params;
#define ISCSI_DB_DATA_DEST_MASK 0x3	/* HSI_COMMENT: destination of doorbell (use enum db_dest) */
#define ISCSI_DB_DATA_DEST_SHIFT        0
#define ISCSI_DB_DATA_AGG_CMD_MASK      0x3	/* HSI_COMMENT: aggregative command to CM (use enum db_agg_cmd_sel) */
#define ISCSI_DB_DATA_AGG_CMD_SHIFT     2
#define ISCSI_DB_DATA_BYPASS_EN_MASK    0x1	/* HSI_COMMENT: enable QM bypass */
#define ISCSI_DB_DATA_BYPASS_EN_SHIFT   4
#define ISCSI_DB_DATA_RESERVED_MASK     0x1
#define ISCSI_DB_DATA_RESERVED_SHIFT    5
#define ISCSI_DB_DATA_AGG_VAL_SEL_MASK  0x3	/* HSI_COMMENT: aggregative value selection */
#define ISCSI_DB_DATA_AGG_VAL_SEL_SHIFT 6
	u8 agg_flags;		/* HSI_COMMENT: bit for every DQ counter flags in CM context that DQ can increment */
	__le16 sq_prod;
};

struct tstorm_iscsi_task_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	__le16 word0;		/* HSI_COMMENT: icid */
	u8 flags0;
#define TSTORM_ISCSI_TASK_AG_CTX_NIBBLE0_MASK   0xF	/* HSI_COMMENT: connection_type */
#define TSTORM_ISCSI_TASK_AG_CTX_NIBBLE0_SHIFT  0
#define TSTORM_ISCSI_TASK_AG_CTX_BIT0_MASK      0x1	/* HSI_COMMENT: exist_in_qm0 */
#define TSTORM_ISCSI_TASK_AG_CTX_BIT0_SHIFT     4
#define TSTORM_ISCSI_TASK_AG_CTX_BIT1_MASK      0x1	/* HSI_COMMENT: exist_in_qm1 */
#define TSTORM_ISCSI_TASK_AG_CTX_BIT1_SHIFT     5
#define TSTORM_ISCSI_TASK_AG_CTX_BIT2_MASK      0x1	/* HSI_COMMENT: bit2 */
#define TSTORM_ISCSI_TASK_AG_CTX_BIT2_SHIFT     6
#define TSTORM_ISCSI_TASK_AG_CTX_BIT3_MASK      0x1	/* HSI_COMMENT: bit3 */
#define TSTORM_ISCSI_TASK_AG_CTX_BIT3_SHIFT     7
	u8 flags1;
#define TSTORM_ISCSI_TASK_AG_CTX_BIT4_MASK      0x1	/* HSI_COMMENT: bit4 */
#define TSTORM_ISCSI_TASK_AG_CTX_BIT4_SHIFT     0
#define TSTORM_ISCSI_TASK_AG_CTX_BIT5_MASK      0x1	/* HSI_COMMENT: bit5 */
#define TSTORM_ISCSI_TASK_AG_CTX_BIT5_SHIFT     1
#define TSTORM_ISCSI_TASK_AG_CTX_CF0_MASK       0x3	/* HSI_COMMENT: timer0cf */
#define TSTORM_ISCSI_TASK_AG_CTX_CF0_SHIFT      2
#define TSTORM_ISCSI_TASK_AG_CTX_CF1_MASK       0x3	/* HSI_COMMENT: timer1cf */
#define TSTORM_ISCSI_TASK_AG_CTX_CF1_SHIFT      4
#define TSTORM_ISCSI_TASK_AG_CTX_CF2_MASK       0x3	/* HSI_COMMENT: timer2cf */
#define TSTORM_ISCSI_TASK_AG_CTX_CF2_SHIFT      6
	u8 flags2;
#define TSTORM_ISCSI_TASK_AG_CTX_CF3_MASK       0x3	/* HSI_COMMENT: timer_stop_all */
#define TSTORM_ISCSI_TASK_AG_CTX_CF3_SHIFT      0
#define TSTORM_ISCSI_TASK_AG_CTX_CF4_MASK       0x3	/* HSI_COMMENT: cf4 */
#define TSTORM_ISCSI_TASK_AG_CTX_CF4_SHIFT      2
#define TSTORM_ISCSI_TASK_AG_CTX_CF5_MASK       0x3	/* HSI_COMMENT: cf5 */
#define TSTORM_ISCSI_TASK_AG_CTX_CF5_SHIFT      4
#define TSTORM_ISCSI_TASK_AG_CTX_CF6_MASK       0x3	/* HSI_COMMENT: cf6 */
#define TSTORM_ISCSI_TASK_AG_CTX_CF6_SHIFT      6
	u8 flags3;
#define TSTORM_ISCSI_TASK_AG_CTX_CF7_MASK       0x3	/* HSI_COMMENT: cf7 */
#define TSTORM_ISCSI_TASK_AG_CTX_CF7_SHIFT      0
#define TSTORM_ISCSI_TASK_AG_CTX_CF0EN_MASK     0x1	/* HSI_COMMENT: cf0en */
#define TSTORM_ISCSI_TASK_AG_CTX_CF0EN_SHIFT    2
#define TSTORM_ISCSI_TASK_AG_CTX_CF1EN_MASK     0x1	/* HSI_COMMENT: cf1en */
#define TSTORM_ISCSI_TASK_AG_CTX_CF1EN_SHIFT    3
#define TSTORM_ISCSI_TASK_AG_CTX_CF2EN_MASK     0x1	/* HSI_COMMENT: cf2en */
#define TSTORM_ISCSI_TASK_AG_CTX_CF2EN_SHIFT    4
#define TSTORM_ISCSI_TASK_AG_CTX_CF3EN_MASK     0x1	/* HSI_COMMENT: cf3en */
#define TSTORM_ISCSI_TASK_AG_CTX_CF3EN_SHIFT    5
#define TSTORM_ISCSI_TASK_AG_CTX_CF4EN_MASK     0x1	/* HSI_COMMENT: cf4en */
#define TSTORM_ISCSI_TASK_AG_CTX_CF4EN_SHIFT    6
#define TSTORM_ISCSI_TASK_AG_CTX_CF5EN_MASK     0x1	/* HSI_COMMENT: cf5en */
#define TSTORM_ISCSI_TASK_AG_CTX_CF5EN_SHIFT    7
	u8 flags4;
#define TSTORM_ISCSI_TASK_AG_CTX_CF6EN_MASK     0x1	/* HSI_COMMENT: cf6en */
#define TSTORM_ISCSI_TASK_AG_CTX_CF6EN_SHIFT    0
#define TSTORM_ISCSI_TASK_AG_CTX_CF7EN_MASK     0x1	/* HSI_COMMENT: cf7en */
#define TSTORM_ISCSI_TASK_AG_CTX_CF7EN_SHIFT    1
#define TSTORM_ISCSI_TASK_AG_CTX_RULE0EN_MASK   0x1	/* HSI_COMMENT: rule0en */
#define TSTORM_ISCSI_TASK_AG_CTX_RULE0EN_SHIFT  2
#define TSTORM_ISCSI_TASK_AG_CTX_RULE1EN_MASK   0x1	/* HSI_COMMENT: rule1en */
#define TSTORM_ISCSI_TASK_AG_CTX_RULE1EN_SHIFT  3
#define TSTORM_ISCSI_TASK_AG_CTX_RULE2EN_MASK   0x1	/* HSI_COMMENT: rule2en */
#define TSTORM_ISCSI_TASK_AG_CTX_RULE2EN_SHIFT  4
#define TSTORM_ISCSI_TASK_AG_CTX_RULE3EN_MASK   0x1	/* HSI_COMMENT: rule3en */
#define TSTORM_ISCSI_TASK_AG_CTX_RULE3EN_SHIFT  5
#define TSTORM_ISCSI_TASK_AG_CTX_RULE4EN_MASK   0x1	/* HSI_COMMENT: rule4en */
#define TSTORM_ISCSI_TASK_AG_CTX_RULE4EN_SHIFT  6
#define TSTORM_ISCSI_TASK_AG_CTX_RULE5EN_MASK   0x1	/* HSI_COMMENT: rule5en */
#define TSTORM_ISCSI_TASK_AG_CTX_RULE5EN_SHIFT  7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le16 word4;		/* HSI_COMMENT: word4 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
};

#endif /* __ISCSI_COMMON__ */
