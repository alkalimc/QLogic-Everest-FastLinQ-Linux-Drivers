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

#ifndef __STORAGE_COMMON__
#define __STORAGE_COMMON__
/*********************/
/* SCSI CONSTANTS */
/*********************/

#define SCSI_MAX_NUM_OF_CMDQS (NUM_OF_GLOBAL_QUEUES / 2)
// Each Resource ID is one-one-valued mapped by the driver to a BDQ Resource ID (for instance per port)
#define SCSI_MAX_NUM_STORAGE_FUNCS (4)

// ID 0 : RQ, ID 1 : IMMEDIATE_DATA, ID 2 : TQ
#define BDQ_ID_RQ                        (0)
#define BDQ_ID_IMM_DATA          (1)
#define BDQ_ID_TQ            (2)
#define BDQ_NUM_IDS          (3)

#define SCSI_MAX_BDQS_NUM    (SCSI_MAX_NUM_STORAGE_FUNCS * BDQ_NUM_IDS)

#define SCSI_NUM_SGES_SLOW_SGL_THR      8

#define BDQ_MAX_EXTERNAL_RING_SIZE (1 << 15)

/* SCSI op codes */
#define SCSI_OPCODE_COMPARE_AND_WRITE    (0x89)
#define SCSI_OPCODE_READ_10              (0x28)
#define SCSI_OPCODE_WRITE_6              (0x0A)
#define SCSI_OPCODE_WRITE_10             (0x2A)
#define SCSI_OPCODE_WRITE_12             (0xAA)
#define SCSI_OPCODE_WRITE_16             (0x8A)
#define SCSI_OPCODE_WRITE_AND_VERIFY_10  (0x2E)
#define SCSI_OPCODE_WRITE_AND_VERIFY_12  (0xAE)
#define SCSI_OPCODE_WRITE_AND_VERIFY_16  (0x8E)

/* iSCSI Drv opaque */
struct iscsi_drv_opaque {
	__le16 reserved_zero[3];
	__le16 opaque;
};

/* Scsi 2B/8B opaque union */
union scsi_opaque {
	struct regpair fcoe_opaque;	/* HSI_COMMENT: 8 Bytes opaque */
	struct iscsi_drv_opaque iscsi_opaque;	/* HSI_COMMENT: 2 Bytes opaque */
};

/* SCSI buffer descriptor */
struct scsi_bd {
	struct regpair address;	/* HSI_COMMENT: Physical Address of buffer */
	union scsi_opaque opaque;	/* HSI_COMMENT: Driver Metadata (preferably Virtual Address of buffer) */
};

/* Scsi Drv BDQ struct */
struct scsi_bdq_ram_drv_data {
	__le16 external_producer;	/* HSI_COMMENT: BDQ External Producer; updated by driver when it loads BDs to External Ring */
	__le16 reserved0[3];
};

/* SCSI SGE entry */
struct scsi_sge {
	struct regpair sge_addr;	/* HSI_COMMENT: SGE address */
	__le32 sge_len;		/* HSI_COMMENT: SGE length */
	__le32 reserved;
};

/* Cached SGEs section */
struct scsi_cached_sges {
	struct scsi_sge sge[4];	/* HSI_COMMENT: Cached SGEs section */
};

/* Scsi Drv CMDQ struct */
struct scsi_drv_cmdq {
	__le16 cmdq_cons;	/* HSI_COMMENT: CMDQ consumer - updated by driver when CMDQ is consumed */
	__le16 reserved0;
	__le32 reserved1;
};

/* Common SCSI init params passed by driver to FW in function init ramrod  */
struct scsi_init_func_params {
	__le16 num_tasks;	/* HSI_COMMENT: Number of tasks in global task list */
	u8 log_page_size;	/* HSI_COMMENT: log of page size value */
	u8 log_page_size_conn;	/* HSI_COMMENT: log of page size value for connection queues SQ, R2TQ and HQs */
	u8 debug_mode;		/* HSI_COMMENT: Use iscsi_debug_mode enum */
	u8 reserved2[11];
};

/* SCSI RQ/CQ/CMDQ firmware function init parameters */
struct scsi_init_func_queues {
	struct regpair glbl_q_params_addr;	/* HSI_COMMENT: Global Qs (CQ/RQ/CMDQ) params host address */
	__le16 rq_buffer_size;	/* HSI_COMMENT: The buffer size of RQ BDQ */
	__le16 cq_num_entries;	/* HSI_COMMENT: CQ num entries */
	__le16 cmdq_num_entries;	/* HSI_COMMENT: CMDQ num entries */
	u8 storage_func_id;	/* HSI_COMMENT: Each function-init Ramrod maps its funciton ID to a Storage function ID, each Storage function ID contains per-BDQ-ID BDQs */
	u8 q_validity;
#define SCSI_INIT_FUNC_QUEUES_RQ_VALID_MASK     0x1
#define SCSI_INIT_FUNC_QUEUES_RQ_VALID_SHIFT    0
#define SCSI_INIT_FUNC_QUEUES_IMM_DATA_VALID_MASK       0x1
#define SCSI_INIT_FUNC_QUEUES_IMM_DATA_VALID_SHIFT      1
#define SCSI_INIT_FUNC_QUEUES_CMD_VALID_MASK    0x1
#define SCSI_INIT_FUNC_QUEUES_CMD_VALID_SHIFT   2
#define SCSI_INIT_FUNC_QUEUES_TQ_VALID_MASK     0x1
#define SCSI_INIT_FUNC_QUEUES_TQ_VALID_SHIFT    3
#define SCSI_INIT_FUNC_QUEUES_SOC_EN_MASK       0x1	/* HSI_COMMENT: This bit is valid if TQ is enabled for this function, SOC option enabled/disabled */
#define SCSI_INIT_FUNC_QUEUES_SOC_EN_SHIFT      4
#define SCSI_INIT_FUNC_QUEUES_SOC_NUM_OF_BLOCKS_LOG_MASK        0x7	/* HSI_COMMENT: Relevant for TQe SOC option - num of blocks in SGE - log */
#define SCSI_INIT_FUNC_QUEUES_SOC_NUM_OF_BLOCKS_LOG_SHIFT       5
	__le16 cq_cmdq_sb_num_arr[SCSI_MAX_NUM_OF_CMDQS];	/* HSI_COMMENT: CQ/CMDQ status block number array */
	u8 num_queues;		/* HSI_COMMENT: Number of continuous global queues used */
	u8 queue_relative_offset;	/* HSI_COMMENT: offset of continuous global queues used */
	u8 cq_sb_pi;		/* HSI_COMMENT: Protocol Index of CQ in status block (CQ consumer) */
	u8 cmdq_sb_pi;		/* HSI_COMMENT: Protocol Index of CMDQ in status block (CMDQ consumer) */
	u8 bdq_pbl_num_entries[BDQ_NUM_IDS];	/* HSI_COMMENT: Per BDQ ID, the PBL page size (number of entries in PBL) */
	u8 reserved1;		/* HSI_COMMENT: reserved */
	struct regpair bdq_pbl_base_address[BDQ_NUM_IDS];	/* HSI_COMMENT: Per BDQ ID, the PBL page Base Address */
	__le16 bdq_xoff_threshold[BDQ_NUM_IDS];	/* HSI_COMMENT: BDQ XOFF threshold - when number of entries will be below that TH, it will send XOFF */
	__le16 cmdq_xoff_threshold;	/* HSI_COMMENT: CMDQ XOFF threshold - when number of entries will be below that TH, it will send XOFF */
	__le16 bdq_xon_threshold[BDQ_NUM_IDS];	/* HSI_COMMENT: BDQ XON threshold - when number of entries will be above that TH, it will send XON */
	__le16 cmdq_xon_threshold;	/* HSI_COMMENT: CMDQ XON threshold - when number of entries will be above that TH, it will send XON */
};

/* Scsi Drv BDQ Data struct (2 BDQ IDs: 0 - RQ, 1 - Immediate Data) */
struct scsi_ram_per_bdq_resource_drv_data {
	struct scsi_bdq_ram_drv_data drv_data_per_bdq_id[BDQ_NUM_IDS];	/* HSI_COMMENT: External ring data */
};

/* SCSI SGL types */
enum scsi_sgl_mode {
	SCSI_TX_SLOW_SGL,	/* HSI_COMMENT: Slow-SGL: More than SCSI_NUM_SGES_SLOW_SGL_THR SGEs and there is at least 1 middle SGE than is smaller than a page size. May be only at TX  */
	SCSI_FAST_SGL,		/* HSI_COMMENT: Fast SGL: Less than SCSI_NUM_SGES_SLOW_SGL_THR SGEs or all middle SGEs are at least a page size */
	MAX_SCSI_SGL_MODE
};

/* SCSI SGL parameters */
struct scsi_sgl_params {
	struct regpair sgl_addr;	/* HSI_COMMENT: SGL base address */
	__le32 sgl_total_length;	/* HSI_COMMENT: SGL total legnth (bytes)  */
	__le32 sge_offset;	/* HSI_COMMENT: Offset in SGE (bytes) */
	__le16 sgl_num_sges;	/* HSI_COMMENT: Number of SGLs sges */
	u8 sgl_index;		/* HSI_COMMENT: SGL index */
	u8 reserved;
};

/* SCSI terminate connection params */
struct scsi_terminate_extra_params {
	__le16 unsolicited_cq_count;	/* HSI_COMMENT: Counts number of CQ placements done due to arrival of unsolicited packets on this connection */
	__le16 cmdq_count;	/* HSI_COMMENT: Counts number of CMDQ placements on this connection */
	u8 reserved[4];
};

/* SCSI Task Queue Element */
struct scsi_tqe {
	__le16 itid;		/* HSI_COMMENT: Physical Address of buffer */
};

#endif /* __STORAGE_COMMON__ */
