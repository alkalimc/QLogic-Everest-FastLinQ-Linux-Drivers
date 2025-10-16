/*
 *  QLogic iSCSI Offload Driver
 *  Copyright (c) 2015-2018 Cavium Inc.
 *
 *  See LICENSE.qedi for copyright and licensing details.
 */

#ifndef __QEDI_HSI__
#define __QEDI_HSI__ 
/********************************/
/* Add include to common target */
/********************************/
#include "common_hsi.h"

/********************************************************/
/* Add include to common storage target for upper driver*/
/*******************************************************/
#include "qeds_hsi.h"

/****************************************/
/* Add include to common storage target */
/****************************************/
#include "storage_common.h"
#include "storage_overtcp_common.h"

/************************************************************************/
/* Add include to common TCP target */
/************************************************************************/
#include "tcp_common.h"

/*************************************************************************/
/* Add include to common iSCSI target for both eCore and protocol driver */
/************************************************************************/
#include "iscsi_common.h"


/*
 * iSCSI CMDQ element
 */
struct iscsi_cmdqe
{
	__le16 conn_id;
	u8 cmdqe_opcode /* indicates the iscsi cmdqe type */;
	u8 error_bit_map;
#define ISCSI_CMDQE_DIF_ERR_BITS_MASK         0x7 /* dif error bit map: [0]-CRC/checksum, [1]-app tag, [2]-reference tag */
#define ISCSI_CMDQE_DIF_ERR_BITS_SHIFT        0
#define ISCSI_CMDQE_DATA_DIGEST_ERR_MASK      0x1 /* Signal Immediate Data Digest Error */
#define ISCSI_CMDQE_DATA_DIGEST_ERR_SHIFT     3
#define ISCSI_CMDQE_RCV_ON_INVALID_CONN_MASK  0x1 /* Signal Connection Error */
#define ISCSI_CMDQE_RCV_ON_INVALID_CONN_SHIFT 4
#define ISCSI_CMDQE_RESERVED_MASK             0x7 /* reserved */
#define ISCSI_CMDQE_RESERVED_SHIFT            5
	struct regpair imm_bd_opaque /* Immediate Data BDs opaque data */;
	__le32 cmd_payload[13] /* iSCSI Basic/Additional Header Segment */;
};


/*
 * iSCSI CMDQE Opcode  
 */
enum iscsi_cmdqe_opcode
{
	ISCSI_CMDQE_OPCODE_NONE /* Used by FW only to indicate that no CMDQE should be consumed */,
	ISCSI_CMDQE_OPCODE_BHS_ONLY /* iSCSI BHS without AHS and without immediate data BD */,
	ISCSI_CMDQE_OPCODE_BHS_W_IMM /* iSCSI BHS with immediate data BD */,
	ISCSI_CMDQE_OPCODE_BHS_W_IMM_NO_BD /* iSCSI BHS arrived with immediate data but BD wasnt consumed */,
	ISCSI_CMDQE_OPCODE_BHS_W_AHS /* iSCSI BHS with expected AHS, without immediate data BD */,
	ISCSI_CMDQE_OPCODE_BHS_W_AHS_W_IMM /* iSCSI BHS with expected AHS, with immediate data BD */,
	ISCSI_CMDQE_OPCODE_AHS /* iSCSI AHS without immediate data BD */,
	ISCSI_CMDQE_OPCODE_AHS_W_IMM /* iSCSI AHS, with immediate data BD */,
	ISCSI_CMDQE_OPCODE_AHS_W_IMM_NO_BD /* iSCSI AHS where the Command arrived with immediate data but BD wasnt consumed */,
	ISCSI_CMDQE_OPCODE_TMF /* iSCSI TMF */,
	MAX_ISCSI_CMDQE_OPCODE
};

#endif /* __QEDI_HSI__ */
