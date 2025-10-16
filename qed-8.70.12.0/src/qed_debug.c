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

#include <linux/types.h>
#include <asm/byteorder.h>
#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/crc32.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/log2.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#define __PREVENT_PXP_GLOBAL_WIN__
#include "qed.h"
#include "qed_cxt.h"
#include "qed_dbg_hsi.h"
#include "qed_hsi.h"
#include "qed_hw.h"
#include "qed_mcp.h"
#include "qed_mfw_hsi.h"
#include "qed_reg_addr.h"

#define __PREVENT_DUMP_MEM_ARR__
#ifdef CONFIG_DEBUG_FS
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/kobject.h>
#include <linux/binfmts.h>
#include <linux/sysfs.h>
#include <linux/efi.h>
#include "qed_phy_api.h"
#include "qed_compat.h"
#include "qed_dev_api.h"
#include "qed_dcbx.h"
#include "qed_chain.h"
#include "qed_sp.h"
#ifndef QED_UPSTREAM		/* ! QED_UPSTREAM */
#include "qed_tests.h"
#endif
#endif

/* Memory groups enum */
enum mem_groups {
	MEM_GROUP_PXP_MEM,
	MEM_GROUP_DMAE_MEM,
	MEM_GROUP_CM_MEM,
	MEM_GROUP_QM_MEM,
	MEM_GROUP_DORQ_MEM,
	MEM_GROUP_BRB_RAM,
	MEM_GROUP_BRB_MEM,
	MEM_GROUP_PRS_MEM,
	MEM_GROUP_SDM_MEM,
	MEM_GROUP_PBUF,
	MEM_GROUP_IOR,
	MEM_GROUP_SEM_MEM,
	MEM_GROUP_RAM,
	MEM_GROUP_BTB_RAM,
	MEM_GROUP_RDIF_CTX,
	MEM_GROUP_TDIF_CTX,
	MEM_GROUP_CFC_MEM,
	MEM_GROUP_CONN_CFC_MEM,
	MEM_GROUP_CAU_PI,
	MEM_GROUP_CAU_MEM,
	MEM_GROUP_CAU_MEM_EXT,
	MEM_GROUP_PXP_ILT,
	MEM_GROUP_MULD_MEM,
	MEM_GROUP_BTB_MEM,
	MEM_GROUP_IGU_MEM,
	MEM_GROUP_IGU_MSIX,
	MEM_GROUP_CAU_SB,
	MEM_GROUP_BMB_RAM,
	MEM_GROUP_BMB_MEM,
	MEM_GROUP_TM_MEM,
	MEM_GROUP_TASK_CFC_MEM,
	MEM_GROUPS_NUM
};

/* Memory groups names */
static const char *s_mem_group_names[] = {
	"PXP_MEM",
	"DMAE_MEM",
	"CM_MEM",
	"QM_MEM",
	"DORQ_MEM",
	"BRB_RAM",
	"BRB_MEM",
	"PRS_MEM",
	"SDM_MEM",
	"PBUF",
	"IOR",
	"SEM_MEM",
	"RAM",
	"BTB_RAM",
	"RDIF_CTX",
	"TDIF_CTX",
	"CFC_MEM",
	"CONN_CFC_MEM",
	"CAU_PI",
	"CAU_MEM",
	"CAU_MEM_EXT",
	"PXP_ILT",
	"MULD_MEM",
	"BTB_MEM",
	"IGU_MEM",
	"IGU_MSIX",
	"CAU_SB",
	"BMB_RAM",
	"BMB_MEM",
	"TM_MEM",
	"TASK_CFC_MEM",
};

/* Idle check conditions */

#ifndef __PREVENT_COND_ARR__

static u32 cond5(const u32 * r, const u32 * imm)
{
	return ((r[0] & imm[0]) != imm[1]) && ((r[1] & imm[2]) != imm[3]);
}

static u32 cond7(const u32 * r, const u32 * imm)
{
	return ((r[0] >> imm[0]) & imm[1]) != imm[2];
}

static u32 cond6(const u32 * r, const u32 * imm)
{
	return (r[0] & imm[0]) != imm[1];
}

static u32 cond9(const u32 * r, const u32 * imm)
{
	return ((r[0] & imm[0]) >> imm[1]) !=
	    (((r[0] & imm[2]) >> imm[3]) | ((r[1] & imm[4]) << imm[5]));
}

static u32 cond10(const u32 * r, const u32 * imm)
{
	return ((r[0] & imm[0]) >> imm[1]) != (r[0] & imm[2]);
}

static u32 cond4(const u32 * r, const u32 * imm)
{
	return (r[0] & ~imm[0]) != imm[1];
}

static u32 cond0(const u32 * r, const u32 * imm)
{
	return (r[0] & ~r[1]) != imm[0];
}

static u32 cond14(const u32 * r, const u32 * imm)
{
	return (r[0] | imm[0]) != imm[1];
}

static u32 cond1(const u32 * r, const u32 * imm)
{
	return r[0] != imm[0];
}

static u32 cond11(const u32 * r, const u32 * imm)
{
	return r[0] != r[1] && r[2] == imm[0];
}

static u32 cond12(const u32 * r, const u32 * imm)
{
	return r[0] != r[1] && r[2] > imm[0];
}

static u32 cond3(const u32 * r, const u32 __maybe_unused * imm)
{
	return r[0] != r[1];
}

static u32 cond13(const u32 * r, const u32 * imm)
{
	return r[0] & imm[0];
}

static u32 cond8(const u32 * r, const u32 * imm)
{
	return r[0] < (r[1] - imm[0]);
}

static u32 cond2(const u32 * r, const u32 * imm)
{
	return r[0] > imm[0];
}

/* Array of Idle Check conditions */
static u32(*cond_arr[]) (const u32 * r, const u32 * imm) = {
cond0,
	    cond1,
	    cond2,
	    cond3,
	    cond4,
	    cond5,
	    cond6,
	    cond7, cond8, cond9, cond10, cond11, cond12, cond13, cond14,};

#endif /* __PREVENT_COND_ARR__ */

#define NUM_PHYS_BLOCKS 84

#define NUM_DBG_RESET_REGS 8

/**
 * **************************** Data Types *********************************
 */

enum hw_types {
	HW_TYPE_ASIC,
	HW_TYPE_EMUL_FULL,
	HW_TYPE_EMUL_REDUCED,
	HW_TYPE_FPGA,
	PLATFORM_CHIPSIM,
	MAX_HW_TYPES
};

/**
 * CM context types
 */
enum cm_ctx_types {
	CM_CTX_CONN_AG,
	CM_CTX_CONN_ST,
	CM_CTX_TASK_AG,
	CM_CTX_TASK_ST,
	NUM_CM_CTX_TYPES
};

/**
 * HW ID allocation modes
 */
enum hw_id_alloc_mode {
	HW_ID_MODE_SINGLE,	/* Single HW ID to all dwords */
	HW_ID_MODE_PER_BLOCK,	/* HW ID per block */
	HW_ID_MODE_PER_DWORD,	/* HW ID per dword */
	MAX_HW_ID_MODES
};

/**
 * Debug bus E4 frame modes
 */
enum dbg_bus_frame_modes {
	DBG_BUS_FRAME_MODE_4ST = 0,	/* 4 Storm dwords (no HW) */
	DBG_BUS_FRAME_MODE_2ST_2HW = 1,	/* 2 Storm dwords, 2 HW dwords */
	DBG_BUS_FRAME_MODE_1ST_3HW = 2,	/* 1 Storm dwords, 3 HW dwords */
	DBG_BUS_FRAME_MODE_4HW = 3,	/* 4 HW dwords (no Storms) */
	DBG_BUS_FRAME_MODE_8HW = 4,	/* 8 HW dwords (no Storms) */
	DBG_BUS_NUM_FRAME_MODES
};

/**
 * Debug bus SEMI frame modes
 */
enum dbg_bus_semi_frame_modes {
	DBG_BUS_SEMI_FRAME_MODE_4FAST = 0,	/* 4 fast dwords */
	DBG_BUS_SEMI_FRAME_MODE_2FAST_2SLOW = 1,	/* 2 fast dwords, 2 slow dwords */
	DBG_BUS_SEMI_FRAME_MODE_1FAST_3SLOW = 2,	/* 1 fast dwords, 3 slow dwords */
	DBG_BUS_SEMI_FRAME_MODE_4SLOW = 3,	/* 4 slow dwords */
	DBG_BUS_SEMI_NUM_FRAME_MODES
};

/**
 * Debug bus filter types
 */
enum dbg_bus_filter_types {
	DBG_BUS_FILTER_TYPE_OFF,	/* Filter always off */
	DBG_BUS_FILTER_TYPE_PRE,	/* Filter before trigger only */
	DBG_BUS_FILTER_TYPE_POST,	/* Filter after trigger only */
	DBG_BUS_FILTER_TYPE_ON	/* Filter always on */
};

/**
 * Debug bus pre-trigger recording types
 */
enum dbg_bus_pre_trigger_types {
	DBG_BUS_PRE_TRIGGER_FROM_ZERO,	/* Record from time 0 */
	DBG_BUS_PRE_TRIGGER_NUM_CHUNKS,	/* Record some chunks before trigger */
	DBG_BUS_PRE_TRIGGER_DROP	/* Drop data before trigger */
};

/**
 * Debug bus post-trigger recording types
 */
enum dbg_bus_post_trigger_types {
	DBG_BUS_POST_TRIGGER_RECORD,	/* Start recording after trigger */
	DBG_BUS_POST_TRIGGER_DROP	/* Drop data after trigger */
};

/**
 * Debug bus other engine mode
 */
enum dbg_bus_other_engine_modes {
	DBG_BUS_OTHER_ENGINE_MODE_NONE,
	DBG_BUS_OTHER_ENGINE_MODE_DOUBLE_BW_TX,
	DBG_BUS_OTHER_ENGINE_MODE_DOUBLE_BW_RX,
	DBG_BUS_OTHER_ENGINE_MODE_CROSS_ENGINE_TX,
	DBG_BUS_OTHER_ENGINE_MODE_CROSS_ENGINE_RX
};

/**
 * DBG block Framing mode definitions
 */
struct framing_mode_defs {
	u8 id;
	u8 blocks_dword_mask;
	u8 storms_dword_mask;
	u8 semi_framing_mode_id;
	u8 full_buf_thr;
};

/**
 * Chip constant definitions
 */
struct chip_defs {
	const char *name;
	u8 dwords_per_cycle;
	u8 num_framing_modes;
	u32 num_ilt_pages;
	struct framing_mode_defs *framing_modes;
};

/**
 * HW type constant definitions
 */
struct hw_type_defs {
	const char *name;
	u32 delay_factor;
	u32 dmae_thresh;
	u32 log_thresh;
};

/**
 * RBC reset definitions
 */
struct rbc_reset_defs {
	u32 reset_reg_addr;
	u32 reset_val[MAX_CHIP_IDS];
};

/**
 * Storm constant definitions.
 * Addresses are in bytes, sizes are in quad-regs.
 */
struct storm_defs {
	char letter;
	enum block_id sem_block_id;
	enum dbg_bus_clients dbg_client_id[MAX_CHIP_IDS];
	bool has_vfc;
	u32 sem_fast_mem_addr;
	u32 sem_frame_mode_addr;
	u32 sem_slow_enable_addr;
	u32 sem_slow_mode_addr;
	u32 sem_slow_mode1_conf_addr;
	u32 sem_sync_dbg_empty_addr;
	u32 sem_gpre_vect_addr;
	u32 cm_ctx_wr_addr;
	u32 cm_ctx_rd_addr[NUM_CM_CTX_TYPES];
	u32 cm_ctx_lid_sizes[MAX_CHIP_IDS][NUM_CM_CTX_TYPES];
};

/**
 * Debug Bus Constraint operation constant definitions
 */
struct dbg_bus_constraint_op_defs {
	u8 hw_op_val;
	bool is_cyclic;
};

/**
 * Storm Mode definitions
 */
struct storm_mode_defs {
	const char *name;
	bool is_fast_dbg;
	u8 id_in_hw;
	u32 src_disable_reg_addr;
	u32 src_enable_val;
	bool exists[MAX_CHIP_IDS];
};

struct grc_param_defs {
	u32 default_val[MAX_CHIP_IDS];	/* default value for this parameter. Will be initialized with this value. */
	u32 min;		/* minimum valid value for the parameter */
	u32 max;		/* maximum valid value for the parameter */
	bool is_preset;		/* if true - this parameter has pre-configured values for other (non-preset) parameters */
	bool is_persistent;	/* if true - this parameter won't be initialize to default */
	u32 exclude_all_preset_val;	/* pre-configured value for DBG_GRC_PARAM_EXCLUDE_ALL parameter */
	u32 crash_preset_val[MAX_CHIP_IDS];	/* pre-configured value for DBG_GRC_PARAM_CRASH parameter */
};

/**
 * address is in 128b units. Width is in bits.
 */
struct rss_mem_defs {
	const char *mem_name;
	const char *type_name;
	u32 addr;
	u32 entry_width;
	u32 num_entries[MAX_CHIP_IDS];
};

struct vfc_ram_defs {
	const char *mem_name;
	const char *type_name;
	u32 base_row;
	u32 num_rows;
};

struct big_ram_defs {
	const char *instance_name;
	enum mem_groups mem_group_id;
	enum mem_groups ram_mem_group_id;
	enum dbg_grc_params grc_param;
	u32 addr_reg_addr;
	u32 data_reg_addr;
	u32 is_256b_reg_addr;
	u32 is_256b_bit_offset[MAX_CHIP_IDS];
	u32 ram_size[MAX_CHIP_IDS];	/* In dwords */
};

struct phy_defs {
	const char *phy_name;

	/**
	 * PHY base GRC address
	 */
	u32 base_addr;

	/**
	 * Relative address of indirect TBUS address register (bits 0..7)
	 */
	u32 tbus_addr_lo_addr;

	/**
	 * Relative address of indirect TBUS address register (bits 8..10)
	 */
	u32 tbus_addr_hi_addr;

	/**
	 * Relative address of indirect TBUS data register (bits 0..7)
	 */
	u32 tbus_data_lo_addr;

	/**
	 * Relative address of indirect TBUS data register (bits 8..11)
	 */
	u32 tbus_data_hi_addr;
};

/**
 * Split type definitions
 */
struct split_type_defs {
	const char *name;
};

/**
 * ***************************** Constants *********************************
 */

#define BYTES_IN_DWORD                  sizeof(u32)

/**
 * In the macros below, size and offset are specified in bits
 */
#define CEIL_DWORDS(size)               DIV_ROUND_UP(size, 32)
#define FIELD_BIT_OFFSET(type, field)   type ## _ ## field ## _ ## OFFSET
#define FIELD_BIT_SIZE(type, field)     type ## _ ## field ## _ ## SIZE
#define FIELD_DWORD_OFFSET(type,				       \
			   field)         (int)(FIELD_BIT_OFFSET(type, \
								 field) / 32)
#define FIELD_DWORD_SHIFT(type, field)  (FIELD_BIT_OFFSET(type, field) % 32)
#define FIELD_BIT_MASK(type,					      \
		       field)             (((1 <<		      \
					     FIELD_BIT_SIZE(type,     \
							    field)) - \
					    1) << FIELD_DWORD_SHIFT(type, field))

#define SET_VAR_FIELD(var, type, field,					      \
		      val)    do { var[FIELD_DWORD_OFFSET(type,		      \
							  field)] &=	      \
					   (~FIELD_BIT_MASK(type, field));    \
				   var[FIELD_DWORD_OFFSET(type,		      \
							  field)] |=	      \
					   (val) << FIELD_DWORD_SHIFT(type,   \
								      field); \
}  while (0)

#define ARR_REG_WR(dev, ptt, addr, arr, arr_size)       for (i = 0;		\
							     i < (arr_size);	\
							     i++) qed_wr(dev,	\
									 ptt,	\
									 addr,	\
									 (arr)[	\
										 i])

#ifndef DWORDS_TO_BYTES
#define DWORDS_TO_BYTES(dwords)         ((dwords) * BYTES_IN_DWORD)
#endif
#ifndef BYTES_TO_DWORDS
#define BYTES_TO_DWORDS(bytes)          ((bytes) / BYTES_IN_DWORD)
#endif

/**
 * extra lines include a signature line + optional latency events line
 */
#define NUM_EXTRA_DBG_LINES(block)              (GET_FIELD(block->flags,		      \
							   DBG_BLOCK_CHIP_HAS_LATENCY_EVENTS) \
						 ? 2 : 1)
#define NUM_DBG_LINES(block)            (block->num_of_dbg_bus_lines + \
					 NUM_EXTRA_DBG_LINES(block))

#define USE_DMAE                        true
#define PROTECT_WIDE_BUS                true

#define RAM_LINES_TO_DWORDS(lines)      ((lines) * 2)
#define RAM_LINES_TO_BYTES(lines)               DWORDS_TO_BYTES( \
		RAM_LINES_TO_DWORDS(lines))

#define REG_DUMP_LEN_SHIFT              24
#define MEM_DUMP_ENTRY_SIZE_DWORDS              BYTES_TO_DWORDS(sizeof(struct \
								       dbg_dump_mem))

#define IDLE_CHK_RULE_SIZE_DWORDS               BYTES_TO_DWORDS(sizeof(struct \
								       dbg_idle_chk_rule))

#define IDLE_CHK_RESULT_HDR_DWORDS              BYTES_TO_DWORDS(sizeof(struct \
								       dbg_idle_chk_result_hdr))

#define IDLE_CHK_RESULT_REG_HDR_DWORDS          BYTES_TO_DWORDS(sizeof(struct \
								       dbg_idle_chk_result_reg_hdr))

#define PAGE_MEM_DESC_SIZE_DWORDS               BYTES_TO_DWORDS(sizeof(struct \
								       phys_mem_desc))

#define IDLE_CHK_MAX_ENTRIES_SIZE       32

/**
 * The sizes and offsets below are specified in bits
 */
#define VFC_CAM_CMD_STRUCT_SIZE         64
#define VFC_CAM_CMD_ROW_OFFSET          48
#define VFC_CAM_CMD_ROW_SIZE            9
#define VFC_CAM_ADDR_STRUCT_SIZE        16
#define VFC_CAM_ADDR_OP_OFFSET          0
#define VFC_CAM_ADDR_OP_SIZE            4
#define VFC_CAM_RESP_STRUCT_SIZE        256
#define VFC_RAM_ADDR_STRUCT_SIZE        16
#define VFC_RAM_ADDR_OP_OFFSET          0
#define VFC_RAM_ADDR_OP_SIZE            2
#define VFC_RAM_ADDR_ROW_OFFSET         2
#define VFC_RAM_ADDR_ROW_SIZE           10
#define VFC_RAM_RESP_STRUCT_SIZE        256

#define VFC_CAM_CMD_DWORDS              CEIL_DWORDS(VFC_CAM_CMD_STRUCT_SIZE)
#define VFC_CAM_ADDR_DWORDS             CEIL_DWORDS(VFC_CAM_ADDR_STRUCT_SIZE)
#define VFC_CAM_RESP_DWORDS             CEIL_DWORDS(VFC_CAM_RESP_STRUCT_SIZE)
#define VFC_RAM_CMD_DWORDS              VFC_CAM_CMD_DWORDS
#define VFC_RAM_ADDR_DWORDS             CEIL_DWORDS(VFC_RAM_ADDR_STRUCT_SIZE)
#define VFC_RAM_RESP_DWORDS             CEIL_DWORDS(VFC_RAM_RESP_STRUCT_SIZE)

#define NUM_VFC_RAM_TYPES               4

#define VFC_CAM_NUM_ROWS                512

#define VFC_OPCODE_CAM_RD               14
#define VFC_OPCODE_RAM_RD               0

#define NUM_RSS_MEM_TYPES               5

#define NUM_BIG_RAM_TYPES               3
#define BIG_RAM_NAME_LEN                3

#define NUM_PHY_TBUS_ADDRESSES          2048
#define PHY_DUMP_SIZE_DWORDS            (NUM_PHY_TBUS_ADDRESSES / 2)

#define SEM_FAST_MODE23_SRC_DISABLE_VAL 0x7
#define SEM_FAST_MODE4_SRC_DISABLE_VAL  0x3
#define SEM_FAST_MODE6_SRC_DISABLE_VAL  0x3f

#define SEM_SLOW_MODE1_DATA_ENABLE      0x1

#define DEBUG_BUS_CYCLE_DWORDS          8

#define VALUES_PER_BLOCK                4
#define MAX_BLOCK_VALUES_MASK           (BIT(VALUES_PER_BLOCK) - 1)

#define HW_ID_BITS                      3

#define NUM_CALENDAR_SLOTS              16

#define MAX_TRIGGER_STATES              3
#define TRIGGER_SETS_PER_STATE          2
#define MAX_CONSTRAINTS                 4
#define MAX_FILTER_CYCLE_OFFSET         4

#define SEM_FILTER_CID_EN_MASK          0x00b
#define SEM_FILTER_EID_MASK_EN_MASK     0x013
#define SEM_FILTER_EID_RANGE_EN_MASK    0x113

#define CHUNK_SIZE_IN_DWORDS            64
#define CHUNK_SIZE_IN_BYTES             DWORDS_TO_BYTES(CHUNK_SIZE_IN_DWORDS)

#define INT_BUF_NUM_OF_LINES            192
#define INT_BUF_LINE_SIZE_IN_DWORDS     16
#define INT_BUF_SIZE_IN_DWORDS                  (INT_BUF_NUM_OF_LINES *	\
						 INT_BUF_LINE_SIZE_IN_DWORDS)
#define INT_BUF_SIZE_IN_CHUNKS                  (INT_BUF_SIZE_IN_DWORDS / \
						 CHUNK_SIZE_IN_DWORDS)

#define PCI_BUF_LINE_SIZE_IN_DWORDS     8
#define PCI_BUF_LINE_SIZE_IN_BYTES              DWORDS_TO_BYTES( \
		PCI_BUF_LINE_SIZE_IN_DWORDS)

#define TARGET_EN_MASK_PCI              0x3
#define TARGET_EN_MASK_NIG              0x4

#define PCI_REQ_CREDIT                  1
#define PCI_PHYS_ADDR_TYPE              0

#define OPAQUE_FID(pci_func)            ((pci_func << 4) | 0xff00)

#define RESET_REG_UNRESET_OFFSET        4

#define PCI_PKT_SIZE_IN_CHUNKS          1
#define PCI_PKT_SIZE_IN_BYTES                   (PCI_PKT_SIZE_IN_CHUNKS * \
						 CHUNK_SIZE_IN_BYTES)

#define NIG_PKT_SIZE_IN_CHUNKS          4

#define FLUSH_DELAY_MS                  500
#define STALL_DELAY_MS                  500

#define SRC_MAC_ADDR_LO16               0x0a0b
#define SRC_MAC_ADDR_HI32               0x0c0d0e0f
#define ETH_TYPE                        0x1000

#define STATIC_DEBUG_LINE_DWORDS        9

#define NUM_COMMON_GLOBAL_PARAMS        11

#define MAX_RECURSION_DEPTH             10

#define FW_IMG_KUKU                     0
#define FW_IMG_MAIN                     1
#define FW_IMG_L2B                      2

#ifndef REG_FIFO_ELEMENT_DWORDS
#define REG_FIFO_ELEMENT_DWORDS         2
#endif
#define REG_FIFO_DEPTH_ELEMENTS         32
#define REG_FIFO_DEPTH_DWORDS                   (REG_FIFO_ELEMENT_DWORDS * \
						 REG_FIFO_DEPTH_ELEMENTS)

#ifndef IGU_FIFO_ELEMENT_DWORDS
#define IGU_FIFO_ELEMENT_DWORDS         4
#endif
#define IGU_FIFO_DEPTH_ELEMENTS         64
#define IGU_FIFO_DEPTH_DWORDS                   (IGU_FIFO_ELEMENT_DWORDS * \
						 IGU_FIFO_DEPTH_ELEMENTS)

#define SEMI_SYNC_FIFO_POLLING_DELAY_MS 5
#define SEMI_SYNC_FIFO_POLLING_COUNT    20

#ifndef PROTECTION_OVERRIDE_ELEMENT_DWORDS
#define PROTECTION_OVERRIDE_ELEMENT_DWORDS 2
#endif
#define PROTECTION_OVERRIDE_DEPTH_ELEMENTS 20
#define PROTECTION_OVERRIDE_DEPTH_DWORDS        (    \
		PROTECTION_OVERRIDE_DEPTH_ELEMENTS * \
		PROTECTION_OVERRIDE_ELEMENT_DWORDS)

#define MCP_SPAD_TRACE_OFFSIZE_ADDR             (MCP_REG_SCRATCH +		     \
						 offsetof(struct static_init,	     \
							  sections[		     \
								  SPAD_SECTION_TRACE \
							  ]))

#define MAX_SW_PLTAFORM_STR_SIZE        64

#define EMPTY_FW_VERSION_STR            "???_???_???_???"
#define EMPTY_FW_IMAGE_STR              "???????????????"

/**
 * **************************** Constant Arrays ******************************
 */

/**
 * E4 DBG block framing mode definitions, in descending preference order
 */
static struct framing_mode_defs s_framing_mode_defs[4] = {
	{DBG_BUS_FRAME_MODE_4ST, 0x0, 0xf,
	 DBG_BUS_SEMI_FRAME_MODE_4FAST,
	 10},
	{DBG_BUS_FRAME_MODE_4HW, 0xf, 0x0, DBG_BUS_SEMI_FRAME_MODE_4SLOW,
	 10},
	{DBG_BUS_FRAME_MODE_2ST_2HW, 0x3, 0xc,
	 DBG_BUS_SEMI_FRAME_MODE_2FAST_2SLOW, 10},
	{DBG_BUS_FRAME_MODE_1ST_3HW, 0x7, 0x8,
	 DBG_BUS_SEMI_FRAME_MODE_1FAST_3SLOW, 10}
};

/**
 * Chip constant definitions array
 */
static struct chip_defs s_chip_defs[MAX_CHIP_IDS] = {
	{"bb", 4, DBG_BUS_NUM_FRAME_MODES, PSWRQ2_REG_ILT_MEMORY_SIZE_BB / 2,
	 s_framing_mode_defs},
	{"ah", 4, DBG_BUS_NUM_FRAME_MODES, PSWRQ2_REG_ILT_MEMORY_SIZE_K2 / 2,
	 s_framing_mode_defs}
};

/**
 * 256b framing mode definitions
 */
static struct framing_mode_defs s_256b_framing_mode =
    { DBG_BUS_FRAME_MODE_8HW, 0xf, 0x0, 0, 10 };

/**
 * Storm constant definitions array
 */
static struct storm_defs s_storm_defs[] = {
	/**
	 * Tstorm
	 */
	{'T', BLOCK_TSEM,
	 {
	  DBG_BUS_CLIENT_RBCT,
	  DBG_BUS_CLIENT_RBCT},
	 true,
	 TSEM_REG_FAST_MEMORY,
	 TSEM_REG_DBG_FRAME_MODE,
	 TSEM_REG_SLOW_DBG_ACTIVE,
	 TSEM_REG_SLOW_DBG_MODE,
	 TSEM_REG_DBG_MODE1_CFG,
	 TSEM_REG_SYNC_DBG_EMPTY,
	 TSEM_REG_DBG_GPRE_VECT,
	 TCM_REG_CTX_RBC_ACCS,
	 {
	  TCM_REG_AGG_CON_CTX,
	  TCM_REG_SM_CON_CTX,
	  TCM_REG_AGG_TASK_CTX,
	  TCM_REG_SM_TASK_CTX},
	 {			/* bb */
	  {
	   4,
	   16,
	   2,
	   4},
	  /* k2 */
	  {
	   4,
	   16,
	   2,
	   4}}},

	/**
	 * Mstorm
	 */
	{'M', BLOCK_MSEM,
	 {DBG_BUS_CLIENT_RBCT,
	  DBG_BUS_CLIENT_RBCM}, false,
	 MSEM_REG_FAST_MEMORY,
	 MSEM_REG_DBG_FRAME_MODE,
	 MSEM_REG_SLOW_DBG_ACTIVE,
	 MSEM_REG_SLOW_DBG_MODE,
	 MSEM_REG_DBG_MODE1_CFG,
	 MSEM_REG_SYNC_DBG_EMPTY,
	 MSEM_REG_DBG_GPRE_VECT,
	 MCM_REG_CTX_RBC_ACCS,
	 {MCM_REG_AGG_CON_CTX, MCM_REG_SM_CON_CTX,
	  MCM_REG_AGG_TASK_CTX,
	  MCM_REG_SM_TASK_CTX},
	 { /* bb */ {1, 10,
		     2, 7},
	  /* k2 */ {1,
		    10,
		    2, 7}}},

	/**
	 * Ustorm
	 */
	{'U', BLOCK_USEM,
	 {DBG_BUS_CLIENT_RBCU,
	  DBG_BUS_CLIENT_RBCU}, false,
	 USEM_REG_FAST_MEMORY,
	 USEM_REG_DBG_FRAME_MODE,
	 USEM_REG_SLOW_DBG_ACTIVE,
	 USEM_REG_SLOW_DBG_MODE,
	 USEM_REG_DBG_MODE1_CFG,
	 USEM_REG_SYNC_DBG_EMPTY,
	 USEM_REG_DBG_GPRE_VECT,
	 UCM_REG_CTX_RBC_ACCS,
	 {UCM_REG_AGG_CON_CTX, UCM_REG_SM_CON_CTX,
	  UCM_REG_AGG_TASK_CTX,
	  UCM_REG_SM_TASK_CTX},
	 { /* bb */ {2, 13,
		     3, 3},
	  /* k2 */ {2,
		    13,
		    3, 3}}},

	/**
	 * Xstorm
	 */
	{'X', BLOCK_XSEM,
	 {DBG_BUS_CLIENT_RBCX,
	  DBG_BUS_CLIENT_RBCX}, false,
	 XSEM_REG_FAST_MEMORY,
	 XSEM_REG_DBG_FRAME_MODE,
	 XSEM_REG_SLOW_DBG_ACTIVE,
	 XSEM_REG_SLOW_DBG_MODE,
	 XSEM_REG_DBG_MODE1_CFG,
	 XSEM_REG_SYNC_DBG_EMPTY,
	 XSEM_REG_DBG_GPRE_VECT,
	 XCM_REG_CTX_RBC_ACCS,
	 {XCM_REG_AGG_CON_CTX, XCM_REG_SM_CON_CTX,
	  0, 0},
	 { /* bb */ {9, 15,
		     0, 0},
	  /* k2 */ {9,
		    15,
		    0, 0}}},

	/**
	 * Ystorm
	 */
	{'Y', BLOCK_YSEM,
	 {DBG_BUS_CLIENT_RBCX,
	  DBG_BUS_CLIENT_RBCY}, false,
	 YSEM_REG_FAST_MEMORY,
	 YSEM_REG_DBG_FRAME_MODE,
	 YSEM_REG_SLOW_DBG_ACTIVE,
	 YSEM_REG_SLOW_DBG_MODE,
	 YSEM_REG_DBG_MODE1_CFG,
	 YSEM_REG_SYNC_DBG_EMPTY,
	 YSEM_REG_DBG_GPRE_VECT,
	 YCM_REG_CTX_RBC_ACCS,
	 {YCM_REG_AGG_CON_CTX, YCM_REG_SM_CON_CTX,
	  YCM_REG_AGG_TASK_CTX,
	  YCM_REG_SM_TASK_CTX},
	 { /* bb */ {2, 3,
		     2, 12},
	  /* k2 */ {2,
		    3,
		    2, 12}}},

	/**
	 * Pstorm
	 */
	{'P', BLOCK_PSEM,
	 {DBG_BUS_CLIENT_RBCS,
	  DBG_BUS_CLIENT_RBCS}, true,
	 PSEM_REG_FAST_MEMORY,
	 PSEM_REG_DBG_FRAME_MODE,
	 PSEM_REG_SLOW_DBG_ACTIVE,
	 PSEM_REG_SLOW_DBG_MODE,
	 PSEM_REG_DBG_MODE1_CFG,
	 PSEM_REG_SYNC_DBG_EMPTY,
	 PSEM_REG_DBG_GPRE_VECT,
	 PCM_REG_CTX_RBC_ACCS,
	 {0, PCM_REG_SM_CON_CTX,
	  0, 0},
	 { /* bb */ {0, 10,
		     0, 0},
	  /* k2 */ {0,
		    10,
		    0, 0}}},
};

/**
 * Constraint operation types
 */
static struct dbg_bus_constraint_op_defs s_constraint_op_defs[] = {
	/**
	 * DBG_BUS_CONSTRAINT_OP_EQ
	 */
	{0, false},

	/**
	 * DBG_BUS_CONSTRAINT_OP_NE
	 */
	{5, false},

	/**
	 * DBG_BUS_CONSTRAINT_OP_LT
	 */
	{1, false},

	/**
	 * DBG_BUS_CONSTRAINT_OP_LTC
	 */
	{1, true},

	/**
	 * DBG_BUS_CONSTRAINT_OP_LE
	 */
	{2, false},

	/**
	 * DBG_BUS_CONSTRAINT_OP_LEC
	 */
	{2, true},

	/**
	 * DBG_BUS_CONSTRAINT_OP_GT
	 */
	{4, false},

	/**
	 * DBG_BUS_CONSTRAINT_OP_GTC
	 */
	{4, true},

	/**
	 * DBG_BUS_CONSTRAINT_OP_GE
	 */
	{3, false},

	/**
	 * DBG_BUS_CONSTRAINT_OP_GEC
	 */
	{3, true}
};

static const char *const s_dbg_target_names[] = {
	/**
	 * DBG_BUS_TARGET_ID_INT_BUF
	 */
	"int-buf",

	/**
	 * DBG_BUS_TARGET_ID_NIG
	 */
	"nw",

	/**
	 * DBG_BUS_TARGET_ID_PCI
	 */
	"pci-buf"
};

static struct storm_mode_defs s_storm_mode_defs[] = {
	/**
	 * DBG_BUS_STORM_MODE_PRINTF
	 */
	{"printf", true, 0, 0,
	 0,
	 {true, true}},

	/**
	 * DBG_BUS_STORM_MODE_PRAM_ADDR
	 */
	{"pram_addr", true, 1, 0, 0,
	 {true, true}},

	/**
	 * DBG_BUS_STORM_MODE_DRA_RW
	 */
	{"dra_rw", true, 2, SEM_FAST_REG_DBG_MODE23_SRC_DISABLE, 0x0,
	 {true, true}},

	/**
	 * DBG_BUS_STORM_MODE_DRA_W
	 */
	{"dra_w", true, 3, SEM_FAST_REG_DBG_MODE23_SRC_DISABLE, 0x0,
	 {true, true}},

	/**
	 * DBG_BUS_STORM_MODE_LD_ST_ADDR
	 */
	{"ld_st_addr", true, 4, SEM_FAST_REG_DBG_MODE4_SRC_DISABLE, 0x0,
	 {true, true}},

	/**
	 * DBG_BUS_STORM_MODE_DRA_FSM
	 */
	{"dra_fsm", true, 5, 0, 0,
	 {true, true}},

	/**
	 * DBG_BUS_STORM_MODE_FAST_DBGMUX
	 */
	{"fast_dbgmux", true, 8, 0, 0,
	 {false, false}},

	/**
	 * DBG_BUS_STORM_MODE_RH
	 */
	{"rh", true, 6, SEM_FAST_REG_DBG_MODE6_SRC_DISABLE, 0x10,
	 {true, true}},

	/**
	 * DBG_BUS_STORM_MODE_RH_WITH_STORE
	 */
	{"rh_with_store", true, 6, SEM_FAST_REG_DBG_MODE6_SRC_DISABLE, 0x0,
	 {true, true}},

	/**
	 * DBG_BUS_STORM_MODE_FOC
	 */
	{"foc", false, 1, 0, 0,
	 {true, true}},

	/**
	 * DBG_BUS_STORM_MODE_EXT_STORE
	 */
	{"ext_store", false, 3, 0, 0,
	 {true, true}}
};

static struct hw_type_defs s_hw_type_defs[] = {
	/**
	 * HW_TYPE_ASIC
	 */
	{"asic", 1, 256, 32768},

	/**
	 * HW_TYPE_EMUL_FULL
	 */
	{"emul_full", 2000, 8, 4096},

	/**
	 * HW_TYPE_EMUL_REDUCED
	 */
	{"emul_reduced", 2000, 8, 4096},

	/**
	 * HW_TYPE_FPGA
	 */
	{"fpga", 200, 32, 8192},

	/**
	 * PLATFORM_CHIPSIM
	 */
	{"chipsim", 0, 0, 32768}
};

static struct grc_param_defs s_grc_param_defs[] = {
	/**
	 * DBG_GRC_PARAM_DUMP_TSTORM
	 */
	{{1,
	  1}, 0, 1, false, false, 1, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_MSTORM
	 */
	{{1,
	  1}, 0, 1, false, false, 1, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_USTORM
	 */
	{{1,
	  1}, 0, 1, false, false, 1, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_XSTORM
	 */
	{{1,
	  1}, 0, 1, false, false, 1, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_YSTORM
	 */
	{{1,
	  1}, 0, 1, false, false, 1, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_PSTORM
	 */
	{{1,
	  1}, 0, 1, false, false, 1, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_REGS
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_RAM
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_PBUF
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_IOR
	 */
	{{0,
	  0}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_VFC
	 */
	{{0,
	  0}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_CM_CTX
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/* DBG_GRC_PARAM_DUMP_PXP */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_RSS
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_CAU
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_QM
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_MCP
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_DORQ
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_CFC
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_IGU
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_BRB
	 */
	{{0,
	  0}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_BTB
	 */
	{{0,
	  0}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_BMB
	 */
	{{0,
	  0}, 0, 1, false, false, 0, {0, 0}},

	/* DBG_GRC_PARAM_RESERVED1 */
	{{0,
	  0}, 0, 1, false, false, 0, {0, 0}},

	/**
	 * DBG_GRC_PARAM_DUMP_MULD
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_PRS
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_DMAE
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_TM
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_SDM
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_DIF
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_STATIC
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_UNSTALL
	 */
	{{0,
	  0}, 0, 1, false, false, 0, {0, 0}},

	/* DBG_GRC_PARAM_DUMP_SEM */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_MCP_TRACE_META_SIZE
	 */
	{{0,
	  0}, 1, 0xffffffff, false, true, 0, {0, 0}},

	/* DBG_GRC_PARAM_EXCLUDE_ALL */
	{{0,
	  0}, 0, 1, true, false, 0, {0, 0}},

	/**
	 * DBG_GRC_PARAM_CRASH
	 */
	{{0,
	  0}, 0, 1, true, false, 0, {0, 0}},

	/**
	 * DBG_GRC_PARAM_PARITY_SAFE
	 */
	{{0,
	  0}, 0, 1, false, false, 0, {0, 0}},

	/**
	 * DBG_GRC_PARAM_DUMP_CM
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_PHY
	 */
	{{0,
	  0}, 0, 1, false, false, 0, {0, 0}},

	/**
	 * DBG_GRC_PARAM_NO_MCP
	 */
	{{0,
	  0}, 0, 1, false, false, 0, {0, 0}},

	/**
	 * DBG_GRC_PARAM_NO_FW_VER
	 */
	{{0,
	  0}, 0, 1, false, false, 0, {0, 0}},

	/* DBG_GRC_PARAM_RESERVED3 */
	{{0,
	  0}, 0, 1, false, false, 0, {0, 0}},

	/**
	 * DBG_GRC_PARAM_DUMP_MCP_HW_DUMP
	 */
	{{0,
	  1}, 0, 1, false, false, 0, {0, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_ILT_CDUC
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/**
	 * DBG_GRC_PARAM_DUMP_ILT_CDUT
	 */
	{{1,
	  1}, 0, 1, false, false, 0, {1, 1}},

	/* DBG_GRC_PARAM_DUMP_CAU_EXT */
	{{0,
	  0}, 0, 1, false, false, 0, {1, 1}}
};

static struct rss_mem_defs s_rss_mem_defs[] = {
	{"rss_mem_cid", "rss_cid", 0, 32,
	 {256, 320}},

	{"rss_mem_key_msb", "rss_key", 1024, 256,
	 {128, 208}},

	{"rss_mem_key_lsb", "rss_key", 2048, 64,
	 {128, 208}},

	{"rss_mem_info", "rss_info", 3072, 16,
	 {128, 208}},

	{"rss_mem_ind", "rss_ind", 4096, 16,
	 {16384, 26624}}
};

static struct vfc_ram_defs s_vfc_ram_defs[] = {
	{"vfc_ram_tt1", "vfc_ram", 0, 512},
	{"vfc_ram_mtt2", "vfc_ram", 512, 128},
	{"vfc_ram_stt2", "vfc_ram", 640, 32},
	{"vfc_ram_ro_vect", "vfc_ram", 672, 32}
};

static struct big_ram_defs s_big_ram_defs[] = {
	{"BRB", MEM_GROUP_BRB_MEM, MEM_GROUP_BRB_RAM,
	 DBG_GRC_PARAM_DUMP_BRB,
	 BRB_REG_BIG_RAM_ADDRESS, BRB_REG_BIG_RAM_DATA,
	 MISC_REG_BLOCK_256B_EN, {0, 0},
	 {153600, 180224}},

	{"BTB", MEM_GROUP_BTB_MEM, MEM_GROUP_BTB_RAM, DBG_GRC_PARAM_DUMP_BTB,
	 BTB_REG_BIG_RAM_ADDRESS, BTB_REG_BIG_RAM_DATA,
	 MISC_REG_BLOCK_256B_EN, {0, 1},
	 {92160, 117760}},

	{"BMB", MEM_GROUP_BMB_MEM, MEM_GROUP_BMB_RAM, DBG_GRC_PARAM_DUMP_BMB,
	 BMB_REG_BIG_RAM_ADDRESS, BMB_REG_BIG_RAM_DATA,
	 MISCS_REG_BLOCK_256B_EN, {0, 0},
	 {36864, 36864}}
};

static struct rbc_reset_defs s_rbc_reset_defs[] = {
	{MISCS_REG_RESET_PL_HV,
	 {0x0, 0x400}},
	{MISC_REG_RESET_PL_PDA_VMAIN_1,
	 {0x4404040, 0x4404040}},
	{MISC_REG_RESET_PL_PDA_VMAIN_2,
	 {0x7, 0x7c00007}},
	{MISC_REG_RESET_PL_PDA_VAUX,
	 {0x2, 0x2}},
};

static struct phy_defs s_phy_defs[] = {
	{"nw_phy", NWS_REG_NWS_CMU_K2,
	 PHY_NW_IP_REG_PHY0_TOP_TBUS_ADDR_7_0_K2,
	 PHY_NW_IP_REG_PHY0_TOP_TBUS_ADDR_15_8_K2,
	 PHY_NW_IP_REG_PHY0_TOP_TBUS_DATA_7_0_K2,
	 PHY_NW_IP_REG_PHY0_TOP_TBUS_DATA_11_8_K2},
	{"sgmii_phy", MS_REG_MS_CMU_K2,
	 PHY_SGMII_IP_REG_AHB_CMU_CSR_0_X132_K2,
	 PHY_SGMII_IP_REG_AHB_CMU_CSR_0_X133_K2,
	 PHY_SGMII_IP_REG_AHB_CMU_CSR_0_X130_K2,
	 PHY_SGMII_IP_REG_AHB_CMU_CSR_0_X131_K2},
	{"pcie_phy0", PHY_PCIE_REG_PHY0_K2,
	 PHY_PCIE_IP_REG_AHB_CMU_CSR_0_X132_K2,
	 PHY_PCIE_IP_REG_AHB_CMU_CSR_0_X133_K2,
	 PHY_PCIE_IP_REG_AHB_CMU_CSR_0_X130_K2,
	 PHY_PCIE_IP_REG_AHB_CMU_CSR_0_X131_K2},
	{"pcie_phy1", PHY_PCIE_REG_PHY1_K2,
	 PHY_PCIE_IP_REG_AHB_CMU_CSR_0_X132_K2,
	 PHY_PCIE_IP_REG_AHB_CMU_CSR_0_X133_K2,
	 PHY_PCIE_IP_REG_AHB_CMU_CSR_0_X130_K2,
	 PHY_PCIE_IP_REG_AHB_CMU_CSR_0_X131_K2},
};

static struct split_type_defs s_split_type_defs[] = {
	/**
	 * SPLIT_TYPE_NONE
	 */
	{"eng"},

	/**
	 * SPLIT_TYPE_PORT
	 */
	{"port"},

	/**
	 * SPLIT_TYPE_PF
	 */
	{"pf"},

	/**
	 * SPLIT_TYPE_PORT_PF
	 */
	{"port"},

	/**
	 * SPLIT_TYPE_VF
	 */
	{"vf"}
};

/**
 * The order of indexes that should be applied to a PCI buffer line
 */
static const u8 s_pci_buf_line_ind[PCI_BUF_LINE_SIZE_IN_DWORDS] =
    { 1, 0, 3, 2, 5, 4, 7, 6 };

/**
 * ******************************* Variables *********************************
 */

/**
 * The version of the calling app
 */
static u32 s_app_ver;

/**
 * *************************** Private Functions *****************************
 */
static void qed_static_asserts(void)
{
}

/**
 * Reads and returns a single dword from the specified unaligned buffer. */
static u32 qed_read_unaligned_dword(u8 * buf)
{
	u32 dword;

	memcpy((u8 *) & dword, buf, sizeof(dword));
	return dword;
}

/**
 * Returns the difference in bytes between the specified physical addresses.
 * Assumes that the first address is bigger then the second, and that the
 * difference is a 32-bit value.
 */
static u32 qed_phys_addr_diff(struct dbg_bus_mem_addr *a,
			      struct dbg_bus_mem_addr *b)
{
	return a->hi == b->hi ? a->lo - b->lo : b->lo - a->lo;
}

/**
 * Returns the number of bits in the specified mask.
 */
static u8 qed_count_mask_bits(u32 mask, u8 mask_bit_width)
{
	u8 i, count = 0;

	for (i = 0; i < mask_bit_width; i++)
		if (mask & BIT(i))
			count++;

	return count;
}

/**
 * Sets the value of the specified GRC param
 */
static void qed_grc_set_param(struct qed_hwfn *p_hwfn,
			      enum dbg_grc_params grc_param, u32 val)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;

	dev_data->grc.param_val[grc_param] = val;
}

/**
 * Returns the value of the specified GRC param
 */
static u32 qed_grc_get_param(struct qed_hwfn *p_hwfn,
			     enum dbg_grc_params grc_param)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;

	return dev_data->grc.param_val[grc_param];
}

/**
 * Initializes the GRC parameters
 */
static void qed_dbg_grc_init_params(struct qed_hwfn *p_hwfn)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;

	if (!dev_data->grc.params_initialized) {
		qed_dbg_grc_set_params_default(p_hwfn);
		dev_data->grc.params_initialized = 1;
	}
}

/**
 * Sets pointer and size for the specified binary buffer type
 */
static void qed_set_dbg_bin_buf(struct qed_hwfn *p_hwfn,
				enum bin_dbg_buffer_type bufType,
				const u32 * ptr, u32 size)
{
	struct virt_mem_desc *buf = &p_hwfn->dbg_arrays[bufType];

	buf->ptr = (void *)ptr;
	buf->size = size;
}

/**
 * Initializes debug data for the specified device
 */
enum dbg_status qed_dbg_dev_init(struct qed_hwfn *p_hwfn)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u8 num_pfs = 0, max_pfs_per_port = 0;

	if (dev_data->initialized)
		return DBG_STATUS_OK;

	if (!s_app_ver)
		return DBG_STATUS_APP_VERSION_NOT_SET;

	/**
	 * Set chip
	 */
	if (QED_IS_K2(p_hwfn->cdev)) {
		dev_data->chip_id = CHIP_K2;
		dev_data->mode_enable[MODE_K2] = 1;
		dev_data->num_vfs = MAX_NUM_VFS_K2;
		num_pfs = MAX_NUM_PFS_K2;
		max_pfs_per_port = MAX_NUM_PFS_K2 / 2;
	} else if (QED_IS_BB_B0(p_hwfn->cdev)) {
		dev_data->chip_id = CHIP_BB;
		dev_data->mode_enable[MODE_BB] = 1;
		dev_data->num_vfs = MAX_NUM_VFS_BB;
		num_pfs = MAX_NUM_PFS_BB;
		max_pfs_per_port = MAX_NUM_PFS_BB;
	} else {
		return DBG_STATUS_UNKNOWN_CHIP;
	}
	/**
	 * Set HW type
	 */
#ifdef ASIC_ONLY
	dev_data->hw_type = HW_TYPE_ASIC;
	dev_data->mode_enable[MODE_ASIC] = 1;
#else
	if (CHIP_REV_IS_ASIC(p_hwfn->cdev)) {
		dev_data->hw_type = HW_TYPE_ASIC;
		dev_data->mode_enable[MODE_ASIC] = 1;
	} else if (CHIP_REV_IS_EMUL(p_hwfn->cdev)) {
		if (p_hwfn->cdev->b_is_emul_full) {
			dev_data->hw_type = HW_TYPE_EMUL_FULL;
			dev_data->mode_enable[MODE_EMUL_FULL] = 1;
		} else {
			dev_data->hw_type = HW_TYPE_EMUL_REDUCED;
			dev_data->mode_enable[MODE_EMUL_REDUCED] = 1;
		}
	} else if (CHIP_REV_IS_FPGA(p_hwfn->cdev)) {
		dev_data->hw_type = HW_TYPE_FPGA;
		dev_data->mode_enable[MODE_FPGA] = 1;
	}
#ifdef CHIPSIM_SUPPORT
	else if (HWAL_IS_CHIPSIM(p_hwfn)) {
		dev_data->hw_type = PLATFORM_CHIPSIM;
		dev_data->mode_enable[MODE_ASIC] = 1;
	}
#endif /* CHIPSIM_SUPPORT */
	else {
		return DBG_STATUS_UNKNOWN_CHIP;
	}
#endif /* ASIC_ONLY */

	/**
	 * Set port mode
	 */
	switch (p_hwfn->cdev->num_ports_in_engine) {
	case 1:
		dev_data->mode_enable[MODE_PORTS_PER_ENG_1] = 1;
		break;
	case 2:
		dev_data->mode_enable[MODE_PORTS_PER_ENG_2] = 1;
		break;
	case 4:
		dev_data->mode_enable[MODE_PORTS_PER_ENG_4] = 1;
		break;
	}

	/**
	 * Set 100G mode
	 */
	if (QED_IS_CMT(p_hwfn->cdev))
		dev_data->mode_enable[MODE_100G] = 1;

	/**
	 * Set number of ports
	 */
	if (dev_data->mode_enable[MODE_PORTS_PER_ENG_1] ||
	    dev_data->mode_enable[MODE_100G])
		dev_data->num_ports = 1;
	else if (dev_data->mode_enable[MODE_PORTS_PER_ENG_2])
		dev_data->num_ports = 2;
	else if (dev_data->mode_enable[MODE_PORTS_PER_ENG_4])
		dev_data->num_ports = 4;

	/**
	 * Set number of PFs per port
	 */
	dev_data->num_pfs_per_port = min_t(u32,
					   num_pfs / dev_data->num_ports,
					   max_pfs_per_port);

	/**
	 * Initializes the GRC parameters
	 */
	qed_dbg_grc_init_params(p_hwfn);

	dev_data->use_dmae = USE_DMAE;
	dev_data->initialized = 1;

	/**
	 * Set debug binary buffers
	 */

	return DBG_STATUS_OK;
}

static const struct dbg_block *get_dbg_block(struct qed_hwfn *p_hwfn,
					     enum block_id block_id)
{
	return (const struct dbg_block *)p_hwfn->dbg_arrays[BIN_BUF_DBG_BLOCKS]
	    .ptr + block_id;
}

static const struct dbg_block_chip *qed_get_dbg_block_per_chip(struct qed_hwfn
							       *p_hwfn,
							       enum block_id
							       block_id)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;

	return (const struct dbg_block_chip *)p_hwfn->
	    dbg_arrays[BIN_BUF_DBG_BLOCKS_CHIP_DATA].ptr +
	    block_id * MAX_CHIP_IDS + dev_data->chip_id;
}

/**
 * Returns NULL for signature line, latency line and non-existing lines
 */
static const struct dbg_bus_line *get_dbg_bus_line(struct qed_hwfn *p_hwfn,
						   enum block_id
						   block_id, u8 line_num)
{
	const struct dbg_block_chip *block;
	bool has_latency_events;

	block = qed_get_dbg_block_per_chip(p_hwfn, block_id);
	has_latency_events = GET_FIELD(block->flags,
				       DBG_BLOCK_CHIP_HAS_LATENCY_EVENTS);

	if (!line_num ||
	    (line_num == 1 &&
	     has_latency_events) || line_num >= NUM_DBG_LINES(block))
		return NULL;

	return (const struct dbg_bus_line *)p_hwfn->
	    dbg_arrays[BIN_BUF_DBG_BUS_LINES].ptr +
	    block->dbg_bus_lines_offset + line_num - NUM_EXTRA_DBG_LINES(block);
}

static const struct dbg_reset_reg *qed_get_dbg_reset_reg(struct qed_hwfn
							 *p_hwfn,
							 u8 reset_reg_id)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;

	return (const struct dbg_reset_reg *)p_hwfn->
	    dbg_arrays[BIN_BUF_DBG_RESET_REGS].ptr +
	    reset_reg_id * MAX_CHIP_IDS + dev_data->chip_id;
}

/**
 * Reads the FW info structure for the specified Storm from the chip,
 * and writes it to the specified fw_info pointer.
 */
static void qed_read_storm_fw_info(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt,
				   u8 storm_id, struct fw_info *fw_info)
{
	struct storm_defs *storm = &s_storm_defs[storm_id];
	struct fw_info_location fw_info_location;
	u32 addr, i, *dest;

	memset(&fw_info_location, 0, sizeof(fw_info_location));
	memset(fw_info, 0, sizeof(*fw_info));

	/**
	 * Read first the address that points to fw_info location.
	 * The address is located in the last line of the Storm RAM.
	 */
	addr = storm->sem_fast_mem_addr + SEM_FAST_REG_INT_RAM +
	    DWORDS_TO_BYTES(SEM_FAST_REG_INT_RAM_SIZE) -
	    sizeof(fw_info_location);

	dest = (u32 *) & fw_info_location;

	for (i = 0; i < BYTES_TO_DWORDS(sizeof(fw_info_location));
	     i++, addr += BYTES_IN_DWORD)
		dest[i] = qed_rd(p_hwfn, p_ptt, addr);

	/**
	 * Read FW version info from Storm RAM
	 */
	if (fw_info_location.size > 0 && fw_info_location.size <=
	    sizeof(*fw_info)) {
		addr = fw_info_location.grc_addr;
		dest = (u32 *) fw_info;
		for (i = 0; i < BYTES_TO_DWORDS(fw_info_location.size);
		     i++, addr += BYTES_IN_DWORD)
			dest[i] = qed_rd(p_hwfn, p_ptt, addr);
	}
}

/**
 * Dumps the specified string to the specified buffer.
 * Returns the dumped size in bytes.
 */
static u32 qed_dump_str(char *dump_buf, bool dump, const char *str)
{
	if (dump)
		strcpy(dump_buf, str);

	return (u32) strlen(str) + 1;
}

/**
 * Dumps zeros to align the specified buffer to dwords.
 * Returns the dumped size in bytes.
 */
static u32 qed_dump_align(char *dump_buf, bool dump, u32 byte_offset)
{
	u8 offset_in_dword, align_size;

	offset_in_dword = (u8) (byte_offset & 0x3);
	align_size = offset_in_dword ? BYTES_IN_DWORD - offset_in_dword : 0;

	if (dump && align_size)
		memset(dump_buf, 0, align_size);

	return align_size;
}

/**
 * Writes the specified string param to the specified buffer.
 * Returns the dumped size in dwords.
 */
static u32 qed_dump_str_param(u32 * dump_buf,
			      bool dump,
			      const char *param_name, const char *param_val)
{
	char *char_buf = (char *)dump_buf;
	u32 offset = 0;

	/**
	 * Dump param name
	 */
	offset += qed_dump_str(char_buf + offset, dump, param_name);

	/**
	 * Indicate a string param value
	 */
	if (dump)
		*(char_buf + offset) = 1;
	offset++;

	/**
	 * Dump param value
	 */
	offset += qed_dump_str(char_buf + offset, dump, param_val);

	/**
	 * Align buffer to next dword
	 */
	offset += qed_dump_align(char_buf + offset, dump, offset);

	return BYTES_TO_DWORDS(offset);
}

/**
 * Writes the specified numeric param to the specified buffer.
 * Returns the dumped size in dwords.
 */
static u32 qed_dump_num_param(u32 * dump_buf,
			      bool dump, const char *param_name, u32 param_val)
{
	char *char_buf = (char *)dump_buf;
	u32 offset = 0;

	/**
	 * Dump param name
	 */
	offset += qed_dump_str(char_buf + offset, dump, param_name);

	/**
	 * Indicate a numeric param value
	 */
	if (dump)
		*(char_buf + offset) = 0;
	offset++;

	/**
	 * Align buffer to next dword
	 */
	offset += qed_dump_align(char_buf + offset, dump, offset);

	/**
	 * Dump param value (and change offset from bytes to dwords)
	 */
	offset = BYTES_TO_DWORDS(offset);
	if (dump)
		*(dump_buf + offset) = param_val;
	offset++;

	return offset;
}

/**
 * Reads the FW version and writes it as a param to the specified buffer.
 * Returns the dumped size in dwords.
 */
static u32 qed_dump_fw_ver_param(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt,
				 u32 * dump_buf, bool dump)
{
	char fw_ver_str[16] = EMPTY_FW_VERSION_STR;
	char fw_img_str[16] = EMPTY_FW_IMAGE_STR;
	struct fw_info fw_info;
	u32 offset = 0;

	memset(&fw_info, 0, sizeof(struct fw_info));

	if (dump && !qed_grc_get_param(p_hwfn, DBG_GRC_PARAM_NO_FW_VER)) {
		/**
		 * Read FW info from chip
		 */
		qed_read_fw_info(p_hwfn, p_ptt, &fw_info);

		/**
		 * Create FW version/image strings
		 */
		if (scnprintf(fw_ver_str, sizeof(fw_ver_str), "%d_%d_%d_%d",
			      fw_info.ver.num.major, fw_info.ver.num.minor,
			      fw_info.ver.num.rev, fw_info.ver.num.eng) < 0)
			DP_NOTICE(p_hwfn,
				  "Unexpected debug error: invalid FW version string\n");
		switch (fw_info.ver.image_id) {
		case FW_IMG_KUKU:
			strcpy(fw_img_str, "kuku");
			break;
		case FW_IMG_MAIN:
			strcpy(fw_img_str, "main");
			break;
		case FW_IMG_L2B:
			strcpy(fw_img_str, "l2b");
			break;
		default:
			strcpy(fw_img_str, "unknown");
			break;
		}
	}

	/**
	 * Dump FW version, image and timestamp
	 */
	offset += qed_dump_str_param(dump_buf + offset,
				     dump, "fw-version", fw_ver_str);
	offset += qed_dump_str_param(dump_buf + offset,
				     dump, "fw-image", fw_img_str);
	offset += qed_dump_num_param(dump_buf + offset,
				     dump,
				     "fw-timestamp", fw_info.ver.timestamp);

	return offset;
}

/**
 * Reads the MFW version and writes it as a param to the specified buffer.
 * Returns the dumped size in dwords.
 */
static u32 qed_dump_mfw_ver_param(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt,
				  u32 * dump_buf, bool dump)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	char mfw_ver_str[16] = EMPTY_FW_VERSION_STR;

	if (dump && dev_data->hw_type == HW_TYPE_ASIC &&
	    !qed_grc_get_param(p_hwfn, DBG_GRC_PARAM_NO_FW_VER)) {
		u32 public_data_addr, global_section_offsize_addr,
		    global_section_offsize, global_section_addr, mfw_ver;

		/**
		 * Find MCP public data GRC address. Needs to be ORed with
		 * MCP_REG_SCRATCH due to a HW bug.
		 */
		public_data_addr = qed_rd(p_hwfn,
					  p_ptt,
					  MISC_REG_SHARED_MEM_ADDR) |
		    MCP_REG_SCRATCH;

		/**
		 * Find MCP public global section offset
		 */
		global_section_offsize_addr =
		    public_data_addr + offsetof(struct mcp_public_data,
						sections) +
		    sizeof(offsize_t) * PUBLIC_GLOBAL;
		global_section_offsize =
		    qed_rd(p_hwfn, p_ptt, global_section_offsize_addr);
		global_section_addr =
		    MCP_REG_SCRATCH +
		    (global_section_offsize & OFFSIZE_OFFSET_MASK) * 4;

		/**
		 * Read MFW version from MCP public global section
		 */
		mfw_ver =
		    qed_rd(p_hwfn, p_ptt, global_section_addr +
			   offsetof(struct public_global, mfw_ver));

		/**
		 * Dump MFW version param
		 */
		if (scnprintf(mfw_ver_str, sizeof(mfw_ver_str), "%d_%d_%d_%d",
			      (u8) (mfw_ver >> 24), (u8) (mfw_ver >> 16),
			      (u8) (mfw_ver >> 8), (u8) mfw_ver) < 0)
			DP_NOTICE(p_hwfn,
				  "Unexpected debug error: invalid MFW version string\n");
	}

	return qed_dump_str_param(dump_buf, dump, "mfw-version", mfw_ver_str);
}

/**
 * Reads the chip revision from the chip and writes it as a param to the
 * specified buffer. Returns the dumped size in dwords.
 */
static u32 qed_dump_chip_revision_param(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt,
					u32 * dump_buf, bool dump)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	char param_str[3] = "??";

	if (dev_data->hw_type == HW_TYPE_ASIC) {
		u32 chip_rev, chip_metal;

		chip_rev = qed_rd(p_hwfn, p_ptt, MISCS_REG_CHIP_REV);
		chip_metal = qed_rd(p_hwfn, p_ptt, MISCS_REG_CHIP_METAL);

		param_str[0] = 'a' + (u8) chip_rev;
		param_str[1] = '0' + (u8) chip_metal;
	}

	return qed_dump_str_param(dump_buf, dump, "chip-revision", param_str);
}

/**
 * Writes a section header to the specified buffer.
 * Returns the dumped size in dwords.
 */
static u32 qed_dump_section_hdr(u32 * dump_buf,
				bool dump, const char *name, u32 num_params)
{
	return qed_dump_num_param(dump_buf, dump, name, num_params);
}

/**
 * Writes the common global params to the specified buffer.
 * Returns the dumped size in dwords.
 */
static u32 qed_dump_common_global_params(struct qed_hwfn *p_hwfn,
					 struct qed_ptt *p_ptt,
					 u32 * dump_buf,
					 bool dump,
					 u8 num_specific_global_params)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	char sw_platform_str[MAX_SW_PLTAFORM_STR_SIZE];
	u32 offset = 0;
	u8 num_params;

	/**
	 * Fill platform string
	 */
	qed_set_platform_str(p_hwfn, sw_platform_str, MAX_SW_PLTAFORM_STR_SIZE);

	/**
	 * Dump global params section header
	 */
	num_params = NUM_COMMON_GLOBAL_PARAMS +
	    num_specific_global_params + (dev_data->chip_id == CHIP_BB ? 1 : 0);
	offset += qed_dump_section_hdr(dump_buf + offset,
				       dump, "global_params", num_params);

	/**
	 * Store params
	 */
	offset += qed_dump_fw_ver_param(p_hwfn, p_ptt, dump_buf + offset, dump);
	offset += qed_dump_mfw_ver_param(p_hwfn,
					 p_ptt, dump_buf + offset, dump);
	offset += qed_dump_chip_revision_param(p_hwfn,
					       p_ptt, dump_buf + offset, dump);
	offset += qed_dump_num_param(dump_buf + offset,
				     dump, "tools-version", TOOLS_VERSION);
	offset += qed_dump_str_param(dump_buf + offset,
				     dump,
				     "chip",
				     s_chip_defs[dev_data->chip_id].name);
	offset += qed_dump_str_param(dump_buf + offset,
				     dump,
				     "platform",
				     s_hw_type_defs[dev_data->hw_type].name);
	offset += qed_dump_str_param(dump_buf + offset,
				     dump, "sw-platform", sw_platform_str);
	offset += qed_dump_num_param(dump_buf + offset,
				     dump, "pci-func", p_hwfn->abs_pf_id);
	offset += qed_dump_num_param(dump_buf + offset,
				     dump, "epoch", qed_get_epoch_time());
	if (dev_data->chip_id == CHIP_BB)
		offset += qed_dump_num_param(dump_buf + offset,
					     dump, "path", QED_PATH_ID(p_hwfn));

	return offset;
}

/**
 * Writes the "last" section (including CRC) to the specified buffer at the
 * given offset. Returns the dumped size in dwords.
 */
static u32 qed_dump_last_section(u32 * dump_buf, u32 offset, bool dump)
{
	u32 start_offset = offset;

	/**
	 * Dump CRC section header
	 */
	offset += qed_dump_section_hdr(dump_buf + offset, dump, "last", 0);

	/**
	 * Calculate CRC32 and add it to the dword after the "last" section
	 */
	if (dump)
		*(dump_buf + offset) = ~crc32(0xffffffff,
					      (u8 *) dump_buf,
					      DWORDS_TO_BYTES(offset));

	offset++;

	return offset - start_offset;
}

/**
 * Update blocks reset state
 */
static void qed_update_blocks_reset_state(struct qed_hwfn *p_hwfn,
					  struct qed_ptt *p_ptt)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u32 reg_val[NUM_DBG_RESET_REGS] = { 0 };
	u8 reset_reg_id;
	u32 block_id;

	if (dev_data->hw_type == PLATFORM_CHIPSIM)
		return;

	/**
	 * Read reset registers
	 */
	for (reset_reg_id = 0; reset_reg_id < NUM_DBG_RESET_REGS;
	     reset_reg_id++) {
		const struct dbg_reset_reg *reset_reg;
		bool reset_reg_removed;
		u32 reset_reg_addr;

		reset_reg = qed_get_dbg_reset_reg(p_hwfn, reset_reg_id);
		reset_reg_removed = GET_FIELD(reset_reg->data,
					      DBG_RESET_REG_IS_REMOVED);
		reset_reg_addr =
		    DWORDS_TO_BYTES(GET_FIELD(reset_reg->data,
					      DBG_RESET_REG_ADDR));

		if (!reset_reg_removed)
			reg_val[reset_reg_id] = qed_rd(p_hwfn,
						       p_ptt, reset_reg_addr);
	}

	/**
	 * Check if blocks are in reset
	 */
	for (block_id = 0; block_id < NUM_PHYS_BLOCKS; block_id++) {
		const struct dbg_block_chip *block;
		bool has_reset_reg;
		bool is_removed;

		block =
		    qed_get_dbg_block_per_chip(p_hwfn, (enum block_id)block_id);
		is_removed = GET_FIELD(block->flags, DBG_BLOCK_CHIP_IS_REMOVED);
		has_reset_reg = GET_FIELD(block->flags,
					  DBG_BLOCK_CHIP_HAS_RESET_REG);

		if (!is_removed && has_reset_reg)
			dev_data->block_in_reset[block_id] =
			    !(reg_val[block->reset_reg_id] &
			      BIT(block->reset_reg_bit_offset));
	}
}

/**
 * is_mode_match recursive function
 */
static bool qed_is_mode_match_rec(struct qed_hwfn *p_hwfn,
				  u16 * modes_buf_offset, u8 rec_depth)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	bool arg1, arg2;
	u8 tree_val;

	if (rec_depth > MAX_RECURSION_DEPTH) {
		DP_NOTICE(p_hwfn,
			  "Unexpected error: is_mode_match_rec exceeded the max recursion depth. This is probably due to a corrupt init/debug buffer.\n");
		return false;
	}

	/**
	 * Get next element from modes tree buffer
	 */
	tree_val =
	    ((u8 *) p_hwfn->dbg_arrays[BIN_BUF_DBG_MODE_TREE].
	     ptr)[(*modes_buf_offset)
		  ++];

	switch (tree_val) {
	case INIT_MODE_OP_NOT:
		return !qed_is_mode_match_rec(p_hwfn,
					      modes_buf_offset, rec_depth + 1);
	case INIT_MODE_OP_OR:
	case INIT_MODE_OP_AND:
		arg1 = qed_is_mode_match_rec(p_hwfn,
					     modes_buf_offset, rec_depth + 1);
		arg2 = qed_is_mode_match_rec(p_hwfn,
					     modes_buf_offset, rec_depth + 1);
		return (tree_val == INIT_MODE_OP_OR) ? (arg1 ||
							arg2) : (arg1 && arg2);
	default:
		return dev_data->mode_enable[tree_val - MAX_INIT_MODE_OPS] > 0;
	}
}

/**
 * Returns true if the mode (specified using modes_buf_offset) is enabled
 */
static bool qed_is_mode_match(struct qed_hwfn *p_hwfn, u16 * modes_buf_offset)
{
	return qed_is_mode_match_rec(p_hwfn, modes_buf_offset, 0);
}

/**
 * Try to empty the SEMI sync fifo. Must be done after messages output
 * were disabled in all Storms.
 * Returns true if the SEMI sync FIFO was emptied in all Storms, false otherwise.
 */
static bool qed_empty_sem_sync_fifo(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u8 storm_id, num_fifos_to_empty = MAX_DBG_STORMS;
	bool is_fifo_empty[MAX_DBG_STORMS] = { false };
	u32 polling_ms, polling_count = 0;

	polling_ms = SEMI_SYNC_FIFO_POLLING_DELAY_MS *
	    s_hw_type_defs[dev_data->hw_type].delay_factor;

	while (num_fifos_to_empty && polling_count <
	       SEMI_SYNC_FIFO_POLLING_COUNT) {
		for (storm_id = 0; storm_id < MAX_DBG_STORMS; storm_id++) {
			struct storm_defs *storm = &s_storm_defs[storm_id];

			if (is_fifo_empty[storm_id])
				continue;

			/**
			 * Check if sync fifo got empty
			 */
			if (dev_data->block_in_reset[storm->sem_block_id] ||
			    qed_rd(p_hwfn, p_ptt,
				   storm->sem_sync_dbg_empty_addr)) {
				is_fifo_empty[storm_id] = true;
				num_fifos_to_empty--;
			}
		}

		/**
		 * Check if need to continue polling
		 */
		if (!num_fifos_to_empty)
			break;

		msleep(polling_ms);
		polling_count++;
	}

	return !num_fifos_to_empty;
}

/**
 * Enable / disable the Debug block
 */
static void qed_bus_enable_dbg_block(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt, bool enable)
{
	qed_wr(p_hwfn, p_ptt, DBG_REG_DBG_BLOCK_ON, enable ? 1 : 0);
}

/**
 * Resets the Debug block
 */
static void qed_bus_reset_dbg_block(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt)
{
	u32 reset_reg_addr, old_reset_reg_val, new_reset_reg_val;
	const struct dbg_reset_reg *reset_reg;
	const struct dbg_block_chip *block;

	block = qed_get_dbg_block_per_chip(p_hwfn, BLOCK_DBG);
	reset_reg = qed_get_dbg_reset_reg(p_hwfn, block->reset_reg_id);
	reset_reg_addr =
	    DWORDS_TO_BYTES(GET_FIELD(reset_reg->data, DBG_RESET_REG_ADDR));

	old_reset_reg_val = qed_rd(p_hwfn, p_ptt, reset_reg_addr);
	new_reset_reg_val =
	    old_reset_reg_val & ~BIT(block->reset_reg_bit_offset);

	qed_wr(p_hwfn, p_ptt, reset_reg_addr, new_reset_reg_val);
	qed_wr(p_hwfn, p_ptt, reset_reg_addr, old_reset_reg_val);
}

/**
 * Returns the dword mask for the specified block parameters. A dword mask
 * is an 8-bit value containing a bit for each dword in the debug bus cycle,
 * indicating if this dword is recorded (1) or not (0).
 */
static u8 qed_bus_get_dword_mask(u8 enable_mask, u8 right_shift)
{
	return ((enable_mask |
		 (enable_mask <<
		  VALUES_PER_BLOCK)) >> right_shift) & MAX_BLOCK_VALUES_MASK;
}

/**
 * Enable / disable Debug Bus clients according to the specified mask
 * (1 = enable, 0 = disable).
 */
static void qed_bus_enable_clients(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt, u32 client_mask)
{
	qed_wr(p_hwfn, p_ptt, DBG_REG_CLIENT_ENABLE, client_mask);
}

/**
 * Enables the specified Storm for Debug Bus. Assumes a valid Storm ID.
 */
static void qed_bus_add_storm_input(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt,
				    enum dbg_storms storm_id,
				    u8 semi_framing_mode_id)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u32 base_addr, sem_filter_params = 0;
	struct dbg_bus_storm_data *storm_bus;
	struct storm_mode_defs *storm_mode;
	struct storm_defs *storm;

	storm = &s_storm_defs[storm_id];
	storm_bus = &dev_data->bus.storms[storm_id];
	storm_mode = &s_storm_mode_defs[storm_bus->mode];
	base_addr = storm->sem_fast_mem_addr;

	qed_wr(p_hwfn, p_ptt, storm->sem_frame_mode_addr, semi_framing_mode_id);

	/**
	 * Config SEM
	 */
	if (storm_mode->is_fast_dbg) {
		/**
		 * Enable fast debug
		 */
		qed_wr(p_hwfn,
		       p_ptt,
		       base_addr + SEM_FAST_REG_DEBUG_MODE,
		       storm_mode->id_in_hw);
		qed_wr(p_hwfn, p_ptt, base_addr + SEM_FAST_REG_DEBUG_ACTIVE, 1);
	} else {
		/**
		 * Enable slow debug
		 */
		qed_wr(p_hwfn, p_ptt, storm->sem_slow_enable_addr, 1);
		qed_wr(p_hwfn,
		       p_ptt, storm->sem_slow_mode_addr, storm_mode->id_in_hw);
		qed_wr(p_hwfn,
		       p_ptt,
		       storm->sem_slow_mode1_conf_addr,
		       SEM_SLOW_MODE1_DATA_ENABLE);
	}

	/**
	 * Enable messages. Must be done after enabling SEM_FAST_REG_DEBUG_ACTIVE
	 * otherwise messages will be dropped after the SEMI sync fifo is filled.
	 */
	if (storm_mode->src_disable_reg_addr)
		qed_wr(p_hwfn,
		       p_ptt,
		       base_addr + storm_mode->src_disable_reg_addr,
		       storm_mode->src_enable_val);

	/**
	 * Config SEM cid filter
	 */
	if (storm_bus->cid_filter_en) {
		qed_wr(p_hwfn,
		       p_ptt,
		       base_addr + SEM_FAST_REG_FILTER_CID, storm_bus->cid);
		sem_filter_params |= SEM_FILTER_CID_EN_MASK;
	}

	/**
	 * Config SEM eid filter
	 */
	if (storm_bus->eid_filter_en) {
		const union dbg_bus_storm_eid_params *eid_filter =
		    &storm_bus->eid_filter_params;

		if (storm_bus->eid_range_not_mask) {
			qed_wr(p_hwfn,
			       p_ptt,
			       base_addr + SEM_FAST_REG_EVENT_ID_RANGE_STRT,
			       eid_filter->range.min);
			qed_wr(p_hwfn,
			       p_ptt,
			       base_addr + SEM_FAST_REG_EVENT_ID_RANGE_END,
			       eid_filter->range.max);
			sem_filter_params |= SEM_FILTER_EID_RANGE_EN_MASK;
		} else {
			qed_wr(p_hwfn,
			       p_ptt,
			       base_addr + SEM_FAST_REG_FILTER_EVENT_ID,
			       eid_filter->mask.val);
			qed_wr(p_hwfn,
			       p_ptt,
			       base_addr + SEM_FAST_REG_EVENT_ID_MASK,
			       ~eid_filter->mask.mask);
			sem_filter_params |= SEM_FILTER_EID_MASK_EN_MASK;
		}
	}

	/**
	 * Config accumulaed SEM filter parameters (if any)
	 */
	if (sem_filter_params)
		qed_wr(p_hwfn,
		       p_ptt,
		       base_addr + SEM_FAST_REG_RECORD_FILTER_ENABLE,
		       sem_filter_params);
}

/**
 * Configure the DBG block client mask
 */
static void qed_bus_config_client_mask(struct qed_hwfn *p_hwfn,
				       struct qed_ptt *p_ptt)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_data *bus = &dev_data->bus;
	u32 block_id, client_mask = 0;
	u8 storm_id;

	/**
	 * Update client mask for Storm inputs
	 */
	if (bus->num_enabled_storms) {
		for (storm_id = 0; storm_id < MAX_DBG_STORMS; storm_id++) {
			struct storm_defs *storm = &s_storm_defs[storm_id];

			if (bus->storms[storm_id].enabled)
				client_mask |=
				    BIT(storm->
					dbg_client_id[dev_data->chip_id]);
		}
	}

	/**
	 * Update client mask for block inputs
	 */
	if (bus->num_enabled_blocks) {
		for (block_id = 0; block_id < MAX_BLOCK_ID; block_id++) {
			struct dbg_bus_block_data *block_bus;
			const struct dbg_block_chip *block;

			block =
			    qed_get_dbg_block_per_chip(p_hwfn, (enum block_id)
						       block_id);
			block_bus = &bus->blocks[block_id];

			if (block_bus->enable_mask && block_id != BLOCK_DBG)
				client_mask |= BIT(block->dbg_client_id);
		}
	}

	/**
	 * Update client mask for GRC input
	 */
	if (bus->grc_input_en)
		client_mask |= BIT(DBG_BUS_CLIENT_CPU);

	/**
	 * Update client mask for timestamp input
	 */
	if (bus->timestamp_input_en)
		client_mask |= BIT(DBG_BUS_CLIENT_TIMESTAMP);

	qed_bus_enable_clients(p_hwfn, p_ptt, client_mask);
}

/**
 * Chooses and returns a matching DBG block framing mode.
 * If no matching framing mode found, return null.
 */
static struct framing_mode_defs *qed_bus_get_framing_mode(struct qed_hwfn
							  *p_hwfn)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_data *bus = &dev_data->bus;
	struct framing_mode_defs *framing_modes;
	u8 num_framing_modes, i;

	if (bus->mode_256b_en)
		return &s_256b_framing_mode;

	num_framing_modes = s_chip_defs[dev_data->chip_id].num_framing_modes;
	framing_modes = s_chip_defs[dev_data->chip_id].framing_modes;

	for (i = 0; i < num_framing_modes; i++) {
		struct framing_mode_defs *framing_mode = &framing_modes[i];

		/**
		 * Check if the requested block dwords fit into the framing mode
		 * block dwords. If Storm dwords are requested, check that the
		 * framing mode contains Storm dwords as well.
		 */
		if (((bus->blocks_dword_mask &
		      framing_mode->blocks_dword_mask) ==
		     bus->blocks_dword_mask) &&
		    (!bus->num_enabled_storms ||
		     framing_mode->storms_dword_mask))
			return framing_mode;
	}

	return NULL;
}

/**
 * Configure the DBG block Storm data
 */
static enum dbg_status qed_bus_config_storm_inputs(struct qed_hwfn *p_hwfn,
						   struct qed_ptt *p_ptt)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_data *bus = &dev_data->bus;
	u8 storm_id, i, next_storm_id = 0;
	u32 storm_id_mask = 0;

	/**
	 * Check if SEMI sync FIFO is empty
	 */
	for (storm_id = 0; storm_id < MAX_DBG_STORMS; storm_id++) {
		struct dbg_bus_storm_data *storm_bus = &bus->storms[storm_id];
		struct storm_defs *storm = &s_storm_defs[storm_id];

		if (storm_bus->enabled &&
		    !qed_rd(p_hwfn, p_ptt, storm->sem_sync_dbg_empty_addr))
			return DBG_STATUS_SEMI_FIFO_NOT_EMPTY;
	}

	for (storm_id = 0; storm_id < MAX_DBG_STORMS; storm_id++) {
		struct dbg_bus_storm_data *storm_bus = &bus->storms[storm_id];

		if (storm_bus->enabled)
			storm_id_mask |=
			    (storm_bus->hw_id << (storm_id * HW_ID_BITS));
	}

	qed_wr(p_hwfn, p_ptt, DBG_REG_STORM_ID_NUM, storm_id_mask);

	/**
	 * Disable storm stall if recording to internal buffer in one-shot
	 */
	qed_wr(p_hwfn,
	       p_ptt,
	       DBG_REG_NO_GRANT_ON_FULL,
	       (dev_data->bus.target == DBG_BUS_TARGET_ID_INT_BUF &&
		bus->one_shot_en) ? 0 : 1);

	/**
	 * Configure calendar
	 */
	for (i = 0; i < NUM_CALENDAR_SLOTS;
	     i++, next_storm_id = (next_storm_id + 1) % MAX_DBG_STORMS) {
		/**
		 * Find next enabled Storm
		 */
		for (; !dev_data->bus.storms[next_storm_id].enabled;
		     next_storm_id = (next_storm_id + 1) % MAX_DBG_STORMS) ;

		/**
		 * Configure calendar slot
		 */
		qed_wr(p_hwfn, p_ptt, DBG_REG_CALENDAR_SLOT0 +
		       DWORDS_TO_BYTES(i), next_storm_id);
	}

	return DBG_STATUS_OK;
}

/* Assigns block HW IDs to each dword in the debug bus cycles. The HW IDs are
 * written to the block_hw_ids array (expected to be of size
 * DEBUG_BUS_CYCLE_DWORDS).
 */
static enum dbg_status qed_bus_assign_block_hw_ids(struct qed_hwfn *p_hwfn,
						   u8 * block_hw_ids)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_data *bus = &dev_data->bus;
	u8 num_block_dwords, max_blocks_hw_ids;
	u8 dword_id, block_id, next_hw_id;
	enum hw_id_alloc_mode hw_id_mode;

	/**
	 * Default mode is HW ID per dword
	 */
	hw_id_mode = HW_ID_MODE_PER_DWORD;

	if (bus->blocks_dword_overlap) {
		/**
		 * Some of the dwords contain data from multiple blocks -
		 * use a single HW ID.
		 */
		hw_id_mode = HW_ID_MODE_SINGLE;
	} else if (bus->filter_en || bus->trigger_en) {
		if (bus->filter_en) {
			/**
			 * Filtering on a single HW ID - use a single HW ID
			 */
			hw_id_mode = HW_ID_MODE_SINGLE;
		} else if (bus->trigger_en) {
			bool multi_constraint_dwords = false,
			    multi_constraint_blocks = false;
			u8 state_id;

			/**
			 * In single HW ID filter mode, we must use a single
			 * HW ID for all constraints. Therefore, for each
			 * trigger state, check if there are constraints on
			 * multiple dwords. If there are, do the following:
			 * - If the constraints cover a single block, use HW ID
			 *   per block.
			 * - If the constraints cover multiple blocks - use
			 *   a single HW ID.
			 */
			for (state_id = 0;
			     state_id < bus->next_trigger_state &&
			     !multi_constraint_blocks; state_id++) {
				struct dbg_bus_trigger_state_data *trigger_state
				    = &bus->trigger_states[state_id];
				u8 num_constraint_dwords =
				    0, num_constraint_blocks = 0;

				num_constraint_dwords =
				    qed_count_mask_bits
				    (trigger_state->constraint_dword_mask,
				     DEBUG_BUS_CYCLE_DWORDS);
				if (num_constraint_dwords <= 1)
					continue;

				multi_constraint_dwords = true;

				/**
				 * This state has multiple dwords per constraint -
				 * check if these dwords contain multiple blocks
				 */
				for (block_id =
				     0;
				     block_id < MAX_BLOCK_ID &&
				     !multi_constraint_blocks; block_id++) {
					struct dbg_bus_block_data *block_bus =
					    &bus->blocks[block_id];

					if (trigger_state->constraint_dword_mask
					    & block_bus->dword_mask)
						multi_constraint_blocks =
						    (++num_constraint_blocks >
						     1);
				}
			}

			if (multi_constraint_dwords)
				hw_id_mode =
				    multi_constraint_blocks ?
				    HW_ID_MODE_SINGLE : HW_ID_MODE_PER_BLOCK;
		}
	}

	/**
	 * Find number of remaining HW IDs for blocks, and update the HW ID
	 * allocation mode if the remaining IDs are not sufficient.
	 */
	num_block_dwords = qed_count_mask_bits(bus->blocks_dword_mask,
					       DEBUG_BUS_CYCLE_DWORDS);
	max_blocks_hw_ids = DEBUG_BUS_CYCLE_DWORDS - bus->num_enabled_storms;

	if (hw_id_mode == HW_ID_MODE_PER_DWORD && num_block_dwords >
	    max_blocks_hw_ids)
		hw_id_mode = HW_ID_MODE_PER_BLOCK;
	if (hw_id_mode == HW_ID_MODE_PER_BLOCK && bus->num_enabled_blocks >
	    max_blocks_hw_ids)
		hw_id_mode = HW_ID_MODE_SINGLE;
	if (hw_id_mode == HW_ID_MODE_SINGLE && !max_blocks_hw_ids)
		return DBG_STATUS_INSUFFICIENT_HW_IDS;

	/**
	 * Assign block HW IDs
	 */
	memset(block_hw_ids, 0, DEBUG_BUS_CYCLE_DWORDS);
	next_hw_id = bus->num_enabled_storms;
	if (hw_id_mode == HW_ID_MODE_PER_DWORD) {
		for (dword_id = 0; dword_id < DEBUG_BUS_CYCLE_DWORDS;
		     dword_id++)
			if (bus->blocks_dword_mask & BIT(dword_id))
				block_hw_ids[dword_id] = next_hw_id++;
	} else if (hw_id_mode == HW_ID_MODE_PER_BLOCK) {
		for (block_id = 0; block_id < MAX_BLOCK_ID; block_id++) {
			struct dbg_bus_block_data *block_bus =
			    &bus->blocks[block_id];

			if (!block_bus->dword_mask)
				continue;
			for (dword_id = 0; dword_id < DEBUG_BUS_CYCLE_DWORDS;
			     dword_id++)
				if (block_bus->dword_mask & BIT(dword_id))
					block_hw_ids[dword_id] = next_hw_id;
			next_hw_id++;
		}
	} else {
		memset(block_hw_ids, next_hw_id, DEBUG_BUS_CYCLE_DWORDS);
	}

	return DBG_STATUS_OK;
}

/**
 * Configure the DBG block HW blocks data.
 * The assignment of block HW IDs to each dword in the debug bus cycle is
 * written to the block_hw_ids array (expected to be of size
 * DEBUG_BUS_CYCLE_DWORDS).
 */
static enum dbg_status qed_bus_config_block_inputs(struct qed_hwfn *p_hwfn,
						   struct qed_ptt *p_ptt,
						   u8 * block_hw_ids)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_data *bus = &dev_data->bus;
	u8 dword_id;

	/**
	 * Configure HW ID mask
	 */
	bus->hw_id_mask = 0;
	for (dword_id = 0; dword_id < DEBUG_BUS_CYCLE_DWORDS; dword_id++)
		bus->hw_id_mask |=
		    (block_hw_ids[dword_id] << (dword_id * HW_ID_BITS));
	qed_wr(p_hwfn, p_ptt, DBG_REG_HW_ID_NUM, bus->hw_id_mask);

	/**
	 * Configure additional K2 PCIE registers
	 */
	if (dev_data->chip_id == CHIP_K2 &&
	    (bus->blocks[BLOCK_PCIE].enable_mask ||
	     bus->blocks[BLOCK_PHY_PCIE].enable_mask)) {
		qed_wr(p_hwfn, p_ptt, PCIE_REG_DBG_REPEAT_THRESHOLD_COUNT_K2,
		       1);
		qed_wr(p_hwfn, p_ptt, PCIE_REG_DBG_FW_TRIGGER_ENABLE_K2, 1);
	}

	return DBG_STATUS_OK;
}

/**
 * Configure the DBG block filters and triggers.
 * The assignment of block HW IDs to each dword in the debug bus cycle is
 * written to the block_hw_ids array (expected to be of size
 * DEBUG_BUS_CYCLE_DWORDS).
 */
static enum dbg_status qed_bus_config_filters_and_triggers(struct qed_hwfn
							   *p_hwfn,
							   struct qed_ptt
							   *p_ptt, struct
							   framing_mode_defs
							   *framing_mode,
							   u8 * block_hw_ids)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_data *bus = &dev_data->bus;
	enum dbg_bus_filter_types filter_type;

	/**
	 * Configure filter type
	 */
	if (bus->filter_en) {
		if (bus->trigger_en) {
			if (bus->filter_pre_trigger)
				filter_type =
				    bus->filter_post_trigger ?
				    DBG_BUS_FILTER_TYPE_ON
				    : DBG_BUS_FILTER_TYPE_PRE;
			else
				filter_type =
				    bus->filter_post_trigger ?
				    DBG_BUS_FILTER_TYPE_POST
				    : DBG_BUS_FILTER_TYPE_OFF;
		} else {
			filter_type = DBG_BUS_FILTER_TYPE_ON;
		}
	} else {
		filter_type = DBG_BUS_FILTER_TYPE_OFF;
	}

	qed_wr(p_hwfn, p_ptt, DBG_REG_FILTER_ENABLE, filter_type);

	/**
	 * Configure the filter HW ID - in Single HW ID mode, there can be
	 * either (1) a single Storm input with no block inputs, or
	 * (2) multiple block inputs with no Storm inputs.
	 */
	if (bus->filter_en) {
		if (!(bus->filter_constraint_dword_mask &
		      bus->blocks_dword_mask) &&
		    !(bus->filter_constraint_dword_mask &
		      framing_mode->storms_dword_mask))
			return DBG_STATUS_INVALID_FILTER_TRIGGER_DWORDS;
		if (bus->num_enabled_storms > 1 ||
		    (bus->num_enabled_storms == 1 && bus->num_enabled_blocks))
			return DBG_STATUS_FILTER_SINGLE_HW_ID;
		qed_wr(p_hwfn, p_ptt, DBG_REG_FILTER_ID_NUM, 0);
	}

	/**
	 * Configure the HW ID for each trigger state
	 */
	if (bus->trigger_en) {
		u8 state_id;

		for (state_id = 0; state_id < bus->next_trigger_state;
		     state_id++) {
			struct dbg_bus_trigger_state_data *state =
			    &bus->trigger_states[state_id];
			u8 dword_id, hw_id = 0;

			if (state->constraint_dword_mask &
			    framing_mode->storms_dword_mask) {
				/**
				 * Triggering on Storm data - use HW ID of
				 * specified Storm.
				 */
				if (bus->num_enabled_storms > 1 ||
				    (state->constraint_dword_mask &
				     bus->blocks_dword_mask))
					return DBG_STATUS_TRIGGER_SINGLE_HW_ID;
				if (state->storm_id == MAX_DBG_STORMS)
					return
					    DBG_STATUS_MISSING_TRIGGER_STATE_STORM;
				hw_id = bus->storms[state->storm_id].hw_id;
			} else if (state->constraint_dword_mask &
				   bus->blocks_dword_mask) {
				/**
				 * Triggering on block data - all state's
				 * constraints have same HW ID. Use the first.
				 */
				for (dword_id = 0;
				     dword_id < DEBUG_BUS_CYCLE_DWORDS;
				     dword_id++) {
					if (!(state->constraint_dword_mask &
					      BIT(dword_id)))
						continue;
					hw_id = block_hw_ids[dword_id];
					break;
				}
			} else {
				return DBG_STATUS_INVALID_FILTER_TRIGGER_DWORDS;
			}

			qed_wr(p_hwfn,
			       p_ptt,
			       DBG_REG_TRIGGER_STATE_ID_0 + state_id *
			       BYTES_IN_DWORD, hw_id);
		}
	}

	return DBG_STATUS_OK;
}

static void qed_bus_config_dbg_line(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt,
				    enum block_id block_id,
				    u8 line_id,
				    u8 enable_mask,
				    u8 right_shift,
				    u8 force_valid_mask, u8 force_frame_mask)
{
	const struct dbg_block_chip *block = qed_get_dbg_block_per_chip(p_hwfn,
									block_id);

	qed_wr(p_hwfn, p_ptt, DWORDS_TO_BYTES(block->dbg_select_reg_addr),
	       line_id);
	qed_wr(p_hwfn, p_ptt, DWORDS_TO_BYTES(block->dbg_dword_enable_reg_addr),
	       enable_mask);
	qed_wr(p_hwfn, p_ptt, DWORDS_TO_BYTES(block->dbg_shift_reg_addr),
	       right_shift);
	qed_wr(p_hwfn, p_ptt, DWORDS_TO_BYTES(block->dbg_force_valid_reg_addr),
	       force_valid_mask);
	qed_wr(p_hwfn, p_ptt, DWORDS_TO_BYTES(block->dbg_force_frame_reg_addr),
	       force_frame_mask);
}

/**
 * Disable debug bus in all blocks
 */
static void qed_bus_disable_blocks(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u32 block_id;

	/**
	 * Disable all blocks
	 */
	for (block_id = 0; block_id < MAX_BLOCK_ID; block_id++) {
		const struct dbg_block_chip *block_per_chip =
		    qed_get_dbg_block_per_chip(p_hwfn,
					       (enum block_id)block_id);

		if (GET_FIELD(block_per_chip->flags,
			      DBG_BLOCK_CHIP_IS_REMOVED) ||
		    dev_data->block_in_reset[block_id])
			continue;
		/**
		 * Disable debug bus
		 */
		if (GET_FIELD(block_per_chip->flags,
			      DBG_BLOCK_CHIP_HAS_DBG_BUS)) {
			u16 modes_buf_offset =
			    GET_FIELD(block_per_chip->dbg_bus_mode.data,
				      DBG_MODE_HDR_MODES_BUF_OFFSET);
			bool eval_mode =
			    GET_FIELD(block_per_chip->dbg_bus_mode.data,
				      DBG_MODE_HDR_EVAL_MODE) > 0;

			if (!eval_mode ||
			    qed_is_mode_match(p_hwfn, &modes_buf_offset))
				qed_wr(p_hwfn, p_ptt,
				       DWORDS_TO_BYTES
				       (block_per_chip->dbg_dword_enable_reg_addr),
				       0);
		}
	}
}

/**
 * Disables Debug Bus block inputs
 */
static enum dbg_status qed_bus_disable_inputs(struct qed_hwfn *p_hwfn,
					      struct qed_ptt *p_ptt,
					      bool empty_semi_fifos)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u8 storm_id;

	/**
	 * Disable messages output in all Storms
	 */
	for (storm_id = 0; storm_id < MAX_DBG_STORMS; storm_id++) {
		struct storm_defs *storm = &s_storm_defs[storm_id];

		if (dev_data->block_in_reset[storm->sem_block_id])
			continue;

		qed_wr(p_hwfn,
		       p_ptt,
		       storm->sem_fast_mem_addr +
		       SEM_FAST_REG_DBG_MODE23_SRC_DISABLE,
		       SEM_FAST_MODE23_SRC_DISABLE_VAL);
		qed_wr(p_hwfn,
		       p_ptt,
		       storm->sem_fast_mem_addr +
		       SEM_FAST_REG_DBG_MODE4_SRC_DISABLE,
		       SEM_FAST_MODE4_SRC_DISABLE_VAL);
		qed_wr(p_hwfn,
		       p_ptt,
		       storm->sem_fast_mem_addr +
		       SEM_FAST_REG_DBG_MODE6_SRC_DISABLE,
		       SEM_FAST_MODE6_SRC_DISABLE_VAL);
	}

	/**
	 * Empty SEMI sync FIFO. If we fail to empty the FIFO when recording to
	 * PCI/NW, change recording target to DBG block internal buffer and try
	 * emptying the FIFO again.
	 */
	if (empty_semi_fifos) {
		bool emptied = qed_empty_sem_sync_fifo(p_hwfn, p_ptt);

		if (!emptied && dev_data->bus.state ==
		    DBG_BUS_STATE_RECORDING && dev_data->bus.target !=
		    DBG_BUS_TARGET_ID_INT_BUF) {
			qed_wr(p_hwfn,
			       p_ptt,
			       DBG_REG_DEBUG_TARGET, DBG_BUS_TARGET_ID_INT_BUF);
			emptied = qed_empty_sem_sync_fifo(p_hwfn, p_ptt);
			if (!emptied)
				DP_NOTICE(p_hwfn,
					  "Warning: failed to empty the SEMI sync FIFO. It means that the last few messages from the SEMI could not be sent to the DBG block. This can happen when the DBG block is blocked (e.g. due to a PCI problem).\n");
		}
	}

	/**
	 * Disable debug in all Storms
	 */
	for (storm_id = 0; storm_id < MAX_DBG_STORMS; storm_id++) {
		struct storm_defs *storm = &s_storm_defs[storm_id];
		u32 base_addr = storm->sem_fast_mem_addr;

		if (dev_data->block_in_reset[storm->sem_block_id])
			continue;

		qed_wr(p_hwfn, p_ptt, base_addr + SEM_FAST_REG_DEBUG_ACTIVE, 0);
		qed_wr(p_hwfn,
		       p_ptt,
		       base_addr + SEM_FAST_REG_RECORD_FILTER_ENABLE,
		       DBG_BUS_FILTER_TYPE_OFF);

		qed_wr(p_hwfn,
		       p_ptt,
		       storm->sem_frame_mode_addr,
		       DBG_BUS_SEMI_FRAME_MODE_4SLOW);
		qed_wr(p_hwfn, p_ptt, storm->sem_slow_enable_addr, 0);
	}

	/**
	 * Disable all clients
	 */
	qed_bus_enable_clients(p_hwfn, p_ptt, 0);

	/**
	 * Disable debug bus in all blocks
	 */
	qed_bus_disable_blocks(p_hwfn, p_ptt);

	/**
	 * Disable timestamp
	 */
	qed_wr(p_hwfn, p_ptt, DBG_REG_TIMESTAMP_VALID_EN, 0);

	/**
	 * Disable filters and triggers
	 */
	qed_wr(p_hwfn, p_ptt, DBG_REG_FILTER_ENABLE, DBG_BUS_FILTER_TYPE_OFF);
	qed_wr(p_hwfn, p_ptt, DBG_REG_TRIGGER_ENABLE, 0);

	return DBG_STATUS_OK;
}

/**
 * Sets a Debug Bus trigger/filter constraint
 */
static void qed_bus_set_constraint(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt,
				   bool is_filter,
				   u8 constraint_id,
				   u8 hw_op_val,
				   u32 data_val,
				   u32 data_mask,
				   u8 frame_bit,
				   u8 frame_mask,
				   u16 dword_offset,
				   u16 range, u8 cyclic_bit, u8 must_bit)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u32 reg_offset = constraint_id * BYTES_IN_DWORD;
	u8 curr_trigger_state;

	/**
	 * For trigger only - set register offset according to state
	 */
	if (!is_filter) {
		curr_trigger_state = dev_data->bus.next_trigger_state - 1;
		reg_offset += curr_trigger_state *
		    TRIGGER_SETS_PER_STATE * MAX_CONSTRAINTS * BYTES_IN_DWORD;
	}

	qed_wr(p_hwfn,
	       p_ptt,
	       (is_filter ? DBG_REG_FILTER_CNSTR_OPRTN_0 :
		DBG_REG_TRIGGER_STATE_SET_CNSTR_OPRTN_0)
	       + reg_offset, hw_op_val);
	qed_wr(p_hwfn,
	       p_ptt,
	       (is_filter ? DBG_REG_FILTER_CNSTR_DATA_0 :
		DBG_REG_TRIGGER_STATE_SET_CNSTR_DATA_0)
	       + reg_offset, data_val);
	qed_wr(p_hwfn,
	       p_ptt,
	       (is_filter ? DBG_REG_FILTER_CNSTR_DATA_MASK_0 :
		DBG_REG_TRIGGER_STATE_SET_CNSTR_DATA_MASK_0)
	       + reg_offset, data_mask);
	qed_wr(p_hwfn,
	       p_ptt,
	       (is_filter ? DBG_REG_FILTER_CNSTR_FRAME_0 :
		DBG_REG_TRIGGER_STATE_SET_CNSTR_FRAME_0)
	       + reg_offset, frame_bit);
	qed_wr(p_hwfn,
	       p_ptt,
	       (is_filter ? DBG_REG_FILTER_CNSTR_FRAME_MASK_0 :
		DBG_REG_TRIGGER_STATE_SET_CNSTR_FRAME_MASK_0)
	       + reg_offset, frame_mask);
	qed_wr(p_hwfn,
	       p_ptt,
	       (is_filter ? DBG_REG_FILTER_CNSTR_OFFSET_0 :
		DBG_REG_TRIGGER_STATE_SET_CNSTR_OFFSET_0)
	       + reg_offset, dword_offset);
	qed_wr(p_hwfn,
	       p_ptt,
	       (is_filter ? DBG_REG_FILTER_CNSTR_RANGE_0 :
		DBG_REG_TRIGGER_STATE_SET_CNSTR_RANGE_0)
	       + reg_offset, range);
	qed_wr(p_hwfn,
	       p_ptt,
	       (is_filter ? DBG_REG_FILTER_CNSTR_CYCLIC_0 :
		DBG_REG_TRIGGER_STATE_SET_CNSTR_CYCLIC_0)
	       + reg_offset, cyclic_bit);
	qed_wr(p_hwfn,
	       p_ptt,
	       (is_filter ? DBG_REG_FILTER_CNSTR_MUST_0 :
		DBG_REG_TRIGGER_STATE_SET_CNSTR_MUST_0)
	       + reg_offset, must_bit);
}

static enum dbg_status qed_bus_add_block_input(struct qed_hwfn *p_hwfn,
					       enum block_id block_id,
					       u8 line_num,
					       u8 enable_mask,
					       u8 right_shift,
					       u8
					       force_valid_mask,
					       u8
					       force_frame_mask,
					       u8 is_256b_line)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_data *bus = &dev_data->bus;
	struct dbg_bus_block_data *block_bus;

	block_bus = &bus->blocks[block_id];

	/**
	 * Set block data
	 */
	block_bus->line_num = line_num;
	block_bus->enable_mask = enable_mask;
	block_bus->right_shift = right_shift;
	block_bus->force_valid_mask = force_valid_mask;
	block_bus->force_frame_mask = force_frame_mask;
	SET_FIELD(block_bus->flags,
		  DBG_BUS_BLOCK_DATA_IS_256B_LINE, is_256b_line);

	/**
	 * Set dword mask
	 */
	block_bus->dword_mask =
	    qed_bus_get_dword_mask(enable_mask, right_shift);
	if ((bus->blocks_dword_mask & block_bus->dword_mask))
		bus->blocks_dword_overlap = 1;
	bus->blocks_dword_mask |= block_bus->dword_mask;

	/* Set 256b mode */
	if (is_256b_line)
		bus->mode_256b_en = 1;

	bus->num_enabled_blocks++;

	return DBG_STATUS_OK;
}

/**
 * Reads the specified DBG Bus internal buffer range and copy it to the
 * specified buffer. Returns the dumped size in dwords.
 */
static u32 qed_bus_dump_int_buf_range(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      u32 * dump_buf,
				      bool dump, u32 start_line, u32 end_line)
{
	u32 line, reg_addr, i, offset = 0;

	if (!dump)
		return (end_line - start_line +
			1) * INT_BUF_LINE_SIZE_IN_DWORDS;

	for (line = start_line,
	     reg_addr = DBG_REG_INTR_BUFFER +
	     DWORDS_TO_BYTES(start_line * INT_BUF_LINE_SIZE_IN_DWORDS);
	     line <= end_line; line++, offset += INT_BUF_LINE_SIZE_IN_DWORDS)
		for (i = 0; i < INT_BUF_LINE_SIZE_IN_DWORDS;
		     i++, reg_addr += BYTES_IN_DWORD)
			dump_buf[offset + INT_BUF_LINE_SIZE_IN_DWORDS - 1 -
				 i] = qed_rd(p_hwfn, p_ptt, reg_addr);

	return offset;
}

/**
 * Reads the DBG Bus internal buffer and copy its contents to a buffer.
 * Returns the dumped size in dwords.
 */
static u32 qed_bus_dump_int_buf(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt,
				u32 * dump_buf, bool dump)
{
	u32 last_written_line, offset = 0;

	last_written_line = qed_rd(p_hwfn, p_ptt, DBG_REG_INTR_BUFFER_WR_PTR);

	if (qed_rd(p_hwfn, p_ptt, DBG_REG_WRAP_ON_INT_BUFFER)) {
		/**
		 * Internal buffer was wrapped: first dump from write pointer
		 * to buffer end, then dump from buffer start to write pointer.
		 */
		if (last_written_line < INT_BUF_NUM_OF_LINES - 1)
			offset += qed_bus_dump_int_buf_range(p_hwfn,
							     p_ptt,
							     dump_buf + offset,
							     dump,
							     last_written_line
							     + 1,
							     INT_BUF_NUM_OF_LINES
							     - 1);
		offset += qed_bus_dump_int_buf_range(p_hwfn,
						     p_ptt,
						     dump_buf + offset,
						     dump,
						     0, last_written_line);
	} else if (last_written_line) {
		/**
		 * Internal buffer wasn't wrapped: dump from buffer start until
		 *  write pointer.
		 */
		if (!qed_rd(p_hwfn, p_ptt, DBG_REG_INTR_BUFFER_RD_PTR))
			offset += qed_bus_dump_int_buf_range(p_hwfn,
							     p_ptt,
							     dump_buf + offset,
							     dump,
							     0,
							     last_written_line);
		else
			DP_NOTICE(p_hwfn,
				  "Unexpected Debug Bus error: internal buffer read pointer is not zero\n");
	}

	return offset;
}

/**
 * Reads the specified DBG Bus PCI buffer range and copy it to the specified
 * buffer. Returns the dumped size in dwords.
 */
static u32 qed_bus_dump_pci_buf_range(struct qed_hwfn *p_hwfn,
				      u32 * dump_buf,
				      bool dump, u32 start_line, u32 end_line)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u32 offset = 0;

	/**
	 * Extract PCI buffer pointer from virtual address
	 */
	void *virt_addr_lo = &dev_data->bus.pci_buf.virt_addr.lo;
	u32 *pci_buf_start = (u32 *) (uintptr_t) * ((u64 *) virt_addr_lo);
	u32 *pci_buf, line, i;

	if (!dump)
		return (end_line - start_line +
			1) * PCI_BUF_LINE_SIZE_IN_DWORDS;

	for (line = start_line,
	     pci_buf = pci_buf_start + start_line * PCI_BUF_LINE_SIZE_IN_DWORDS;
	     line <= end_line; line++, offset += PCI_BUF_LINE_SIZE_IN_DWORDS)
		for (i = 0; i < PCI_BUF_LINE_SIZE_IN_DWORDS; i++, pci_buf++)
			dump_buf[offset + s_pci_buf_line_ind[i]] = *pci_buf;

	return offset;
}

/**
 * Copies the DBG Bus PCI buffer to the specified buffer.
 * Returns the dumped size in dwords.
 */
static u32 qed_bus_dump_pci_buf(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt,
				u32 * dump_buf, bool dump)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u32 next_wr_byte_offset, next_wr_line_offset;
	struct dbg_bus_mem_addr next_wr_phys_addr;
	u32 pci_buf_size_in_lines, offset = 0;

	pci_buf_size_in_lines = dev_data->bus.pci_buf.size /
	    PCI_BUF_LINE_SIZE_IN_BYTES;

	/**
	 * Extract write pointer (physical address)
	 */
	next_wr_phys_addr.lo = qed_rd(p_hwfn, p_ptt, DBG_REG_EXT_BUFFER_WR_PTR);
	next_wr_phys_addr.hi = qed_rd(p_hwfn,
				      p_ptt,
				      DBG_REG_EXT_BUFFER_WR_PTR +
				      BYTES_IN_DWORD);

	/**
	 * Convert write pointer to offset
	 */
	next_wr_byte_offset = qed_phys_addr_diff(&next_wr_phys_addr,
						 &dev_data->bus.
						 pci_buf.phys_addr);
	if ((next_wr_byte_offset % PCI_BUF_LINE_SIZE_IN_BYTES)
	    || next_wr_byte_offset > dev_data->bus.pci_buf.size)
		return 0;
	next_wr_line_offset = next_wr_byte_offset / PCI_BUF_LINE_SIZE_IN_BYTES;

	/**
	 * PCI buffer wrapped: first dump from write pointer to buffer end.
	 */
	if (qed_rd(p_hwfn, p_ptt, DBG_REG_WRAP_ON_EXT_BUFFER))
		offset += qed_bus_dump_pci_buf_range(p_hwfn,
						     dump_buf + offset,
						     dump,
						     next_wr_line_offset,
						     pci_buf_size_in_lines - 1);

	/**
	 * Dump from buffer start until write pointer
	 */
	if (next_wr_line_offset)
		offset += qed_bus_dump_pci_buf_range(p_hwfn,
						     dump_buf + offset,
						     dump,
						     0,
						     next_wr_line_offset - 1);

	return offset;
}

/**
 * Copies the DBG Bus recorded data to the specified buffer.
 * Returns the dumped size in dwords.
 */
static u32 qed_bus_dump_data(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, u32 * dump_buf, bool dump)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;

	switch (dev_data->bus.target) {
	case DBG_BUS_TARGET_ID_INT_BUF:
		return qed_bus_dump_int_buf(p_hwfn, p_ptt, dump_buf, dump);
	case DBG_BUS_TARGET_ID_PCI:
		return qed_bus_dump_pci_buf(p_hwfn, p_ptt, dump_buf, dump);
	default:
		break;
	}

	return 0;
}

/**
 * Frees the Debug Bus PCI buffer
 */
static void qed_bus_free_pci_buf(struct qed_hwfn *p_hwfn)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	dma_addr_t pci_buf_phys_addr;
	void *virt_addr_lo;
	u32 *pci_buf;

	/**
	 * Extract PCI buffer pointer from virtual address
	 */
	virt_addr_lo = &dev_data->bus.pci_buf.virt_addr.lo;
	pci_buf = (u32 *) (uintptr_t) * ((u64 *) virt_addr_lo);

	if (!dev_data->bus.pci_buf.size)
		return;

	memcpy(&pci_buf_phys_addr, &dev_data->bus.pci_buf.phys_addr,
	       sizeof(pci_buf_phys_addr));

	dma_free_coherent(&p_hwfn->cdev->pdev->dev,
			  dev_data->bus.pci_buf.size,
			  pci_buf, pci_buf_phys_addr);

	dev_data->bus.pci_buf.size = 0;
}

/**
 * Dumps the list of DBG Bus inputs (blocks/Storms) to the specified buffer.
 * Returns the dumped size in dwords.
 */
static u32 qed_bus_dump_inputs(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, u32 * dump_buf, bool dump)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	char storm_name[8] = "?storm";
	u32 block_id, offset = 0;
	u8 storm_id;

	/**
	 * Store storms
	 */
	for (storm_id = 0; storm_id < MAX_DBG_STORMS; storm_id++) {
		struct dbg_bus_storm_data *storm_bus =
		    &dev_data->bus.storms[storm_id];
		struct storm_defs *storm = &s_storm_defs[storm_id];
		u8 num_input_params = 3;
		u32 gpre_vect = 0;

		if (!dev_data->bus.storms[storm_id].enabled)
			continue;

		if (dump) {
			/**
			 * Read GPRE vector
			 */
			gpre_vect = qed_rd(p_hwfn,
					   p_ptt, storm->sem_gpre_vect_addr);
			if (gpre_vect)
				num_input_params++;
		}

		/**
		 * Dump section header
		 */
		storm_name[0] = storm->letter;
		offset += qed_dump_section_hdr(dump_buf + offset,
					       dump,
					       "bus_input", num_input_params);
		offset += qed_dump_str_param(dump_buf + offset,
					     dump, "name", storm_name);
		offset += qed_dump_num_param(dump_buf + offset,
					     dump, "id", storm_bus->hw_id);
		offset += qed_dump_str_param(dump_buf + offset,
					     dump,
					     "mode",
					     s_storm_mode_defs[storm_bus->
							       mode].name);
		if (gpre_vect)
			offset += qed_dump_num_param(dump_buf + offset,
						     dump,
						     "gpre_vect", gpre_vect);
	}

	/**
	 * Store blocks
	 */
	for (block_id = 0; block_id < MAX_BLOCK_ID; block_id++) {
		struct dbg_bus_block_data *block_bus =
		    &dev_data->bus.blocks[block_id];
		const struct dbg_block *block = get_dbg_block(p_hwfn,
							      (enum block_id)
							      block_id);
		u8 is_256b_line;

		if (!block_bus->enable_mask)
			continue;

		is_256b_line = GET_FIELD(block_bus->flags,
					 DBG_BUS_BLOCK_DATA_IS_256B_LINE);

		/* Dump section header */
		offset += qed_dump_section_hdr(dump_buf + offset,
					       dump, "bus_input", 5);
		offset +=
		    qed_dump_str_param(dump_buf + offset, dump, "name",
				       (const char *)block->name);
		offset += qed_dump_num_param(dump_buf + offset,
					     dump, "line", block_bus->line_num);
		offset += qed_dump_num_param(dump_buf + offset,
					     dump,
					     "en", block_bus->enable_mask);
		offset += qed_dump_num_param(dump_buf + offset,
					     dump,
					     "shr", block_bus->right_shift);
		offset += qed_dump_num_param(dump_buf + offset,
					     dump, "256b", is_256b_line);
	}

	return offset;
}

/**
 * Dumps the Debug Bus header (params, inputs, data header) to the specified
 * buffer. Returns the dumped size in dwords.
 */
static u32 qed_bus_dump_hdr(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, u32 * dump_buf, bool dump)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	char hw_id_mask_str[16];
	u8 num_specific_params;
	u32 offset = 0;

	if (scnprintf(hw_id_mask_str, sizeof(hw_id_mask_str), "0x%x",
		      dev_data->bus.hw_id_mask) < 0)
		DP_NOTICE(p_hwfn,
			  "Unexpected debug error : invalid HW ID mask\n");

	num_specific_params = 4 +	/* 4 must match below amount of params */
	    (dev_data->bus.mode_256b_en ? 1 : 0) +
	    (dev_data->bus.target != DBG_BUS_TARGET_ID_NIG ? 1 : 0);

	/**
	 * Dump global params
	 */
	offset += qed_dump_common_global_params(p_hwfn,
						p_ptt,
						dump_buf + offset,
						dump, num_specific_params);
	offset += /* 1 */ qed_dump_str_param(dump_buf + offset,
					     dump, "dump-type", "debug-bus");
	offset += /* 2 */ qed_dump_str_param(
						    dump_buf + offset,
						    dump,
						    "wrap-mode",
						    dev_data->bus.one_shot_en ?
						    "one-shot" : "wrap-around");
	offset += /* 3 */ qed_dump_str_param(dump_buf + offset,
					     dump,
					     "hw-id-mask", hw_id_mask_str);
	offset += /* 4 */ qed_dump_str_param(dump_buf + offset,
					     dump,
					     "target",
					     s_dbg_target_names[dev_data->
								bus.target]);
	/* Additional/Less parameters require matching of number in call to dump_common_global_params() */

	if (dev_data->bus.target != DBG_BUS_TARGET_ID_NIG) {
		u32 buf_size_bytes =
		    (dev_data->bus.target ==
		     DBG_BUS_TARGET_ID_PCI ? dev_data->bus.pci_buf.size :
		     DWORDS_TO_BYTES(INT_BUF_SIZE_IN_DWORDS));

		offset += qed_dump_num_param(dump_buf + offset,
					     dump,
					     "target-size-kb",
					     buf_size_bytes / 1024);
	}
	if (dev_data->bus.mode_256b_en)
		offset += qed_dump_num_param(dump_buf + offset,
					     dump, "hw-dwords", 8);

	offset += qed_bus_dump_inputs(p_hwfn, p_ptt, dump_buf + offset, dump);

	if (dev_data->bus.target != DBG_BUS_TARGET_ID_NIG) {
		/**
		 * Start data section
		 */
		u32 recorded_dwords = 0;

		if (dump)
			recorded_dwords = qed_bus_dump_data(p_hwfn,
							    p_ptt, NULL, false);

		offset += qed_dump_section_hdr(dump_buf + offset,
					       dump, "bus_data", 1);
		offset += qed_dump_num_param(dump_buf + offset,
					     dump, "size", recorded_dwords);
	}

	return offset;
}

/**
 * Returns true if the specified entity (indicated by GRC param) should be
 * included in the dump, false otherwise.
 */
static bool qed_grc_is_included(struct qed_hwfn *p_hwfn,
				enum dbg_grc_params grc_param)
{
	return qed_grc_get_param(p_hwfn, grc_param) > 0;
}

/**
 * Returns the storm_id that matches the specified Storm letter,
 * or MAX_DBG_STORMS if invalid storm letter.
 */
static enum dbg_storms qed_get_storm_id_from_letter(char storm_letter)
{
	u8 storm_id;

	for (storm_id = 0; storm_id < MAX_DBG_STORMS; storm_id++)
		if (s_storm_defs[storm_id].letter == storm_letter)
			return (enum dbg_storms)storm_id;

	return MAX_DBG_STORMS;
}

/**
 * Returns true of the specified Storm should be included in the dump, false
 * otherwise.
 */
static bool qed_grc_is_storm_included(struct qed_hwfn *p_hwfn,
				      enum dbg_storms storm)
{
	return qed_grc_get_param(p_hwfn, (enum dbg_grc_params)storm) > 0;
}

/**
 * Returns true if the specified memory should be included in the dump, false
 * otherwise.
 */
static bool qed_grc_is_mem_included(struct qed_hwfn *p_hwfn,
				    enum block_id block_id, u8 mem_group_id)
{
	const struct dbg_block *block;
	u8 i;

	block = get_dbg_block(p_hwfn, block_id);

	/**
	 * If the block is associated with a Storm, check Storm match
	 */
	if (block->associated_storm_letter) {
		enum dbg_storms associated_storm_id =
		    qed_get_storm_id_from_letter(block->
						 associated_storm_letter);

		if (associated_storm_id == MAX_DBG_STORMS ||
		    !qed_grc_is_storm_included(p_hwfn, associated_storm_id))
			return false;
	}

	for (i = 0; i < NUM_BIG_RAM_TYPES; i++) {
		struct big_ram_defs *big_ram = &s_big_ram_defs[i];

		if (mem_group_id == big_ram->mem_group_id || mem_group_id ==
		    big_ram->ram_mem_group_id)
			return qed_grc_is_included(p_hwfn, big_ram->grc_param);
	}

	switch (mem_group_id) {
	case MEM_GROUP_PXP_ILT:
	case MEM_GROUP_PXP_MEM:
		return qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_PXP);
	case MEM_GROUP_RAM:
		return qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_RAM);
	case MEM_GROUP_PBUF:
		return qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_PBUF);
	case MEM_GROUP_CAU_MEM:
	case MEM_GROUP_CAU_SB:
	case MEM_GROUP_CAU_PI:
		return qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_CAU);
	case MEM_GROUP_CAU_MEM_EXT:
		return qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_CAU_EXT);
	case MEM_GROUP_QM_MEM:
		return qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_QM);
	case MEM_GROUP_CFC_MEM:
	case MEM_GROUP_CONN_CFC_MEM:
	case MEM_GROUP_TASK_CFC_MEM:
		return qed_grc_is_included(p_hwfn,
					   DBG_GRC_PARAM_DUMP_CFC) ||
		    qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_CM_CTX);
	case MEM_GROUP_DORQ_MEM:
		return qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_DORQ);
	case MEM_GROUP_IGU_MEM:
	case MEM_GROUP_IGU_MSIX:
		return qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_IGU);
	case MEM_GROUP_MULD_MEM:
		return qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_MULD);
	case MEM_GROUP_PRS_MEM:
		return qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_PRS);
	case MEM_GROUP_DMAE_MEM:
		return qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_DMAE);
	case MEM_GROUP_TM_MEM:
		return qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_TM);
	case MEM_GROUP_SDM_MEM:
		return qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_SDM);
	case MEM_GROUP_TDIF_CTX:
	case MEM_GROUP_RDIF_CTX:
		return qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_DIF);
	case MEM_GROUP_CM_MEM:
		return qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_CM);
	case MEM_GROUP_SEM_MEM:
		return qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_SEM);
	case MEM_GROUP_IOR:
		return qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_IOR);
	default:
		return true;
	}
}

/**
 * Stalls all Storms
 */
static void qed_grc_stall_storms(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt, bool stall)
{
	u32 reg_addr;
	u8 storm_id;

	for (storm_id = 0; storm_id < MAX_DBG_STORMS; storm_id++) {
		if (!qed_grc_is_storm_included(p_hwfn,
					       (enum dbg_storms)storm_id))
			continue;

		reg_addr = s_storm_defs[storm_id].sem_fast_mem_addr +
		    SEM_FAST_REG_STALL_0;
		qed_wr(p_hwfn, p_ptt, reg_addr, stall ? 1 : 0);
	}

	msleep(STALL_DELAY_MS);
}

/**
 * Takes all blocks out of reset. If rbc_only is true, only RBC clients are
 * taken out of reset.
 */
static void qed_grc_unreset_blocks(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt, bool rbc_only)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u32 i;

	/**
	 * Take RBCs out of reset
	 */
	for (i = 0; i < ARRAY_SIZE(s_rbc_reset_defs); i++)
		if (s_rbc_reset_defs[i].reset_val[dev_data->chip_id])
			qed_wr(p_hwfn,
			       p_ptt,
			       s_rbc_reset_defs[i].reset_reg_addr +
			       RESET_REG_UNRESET_OFFSET,
			       s_rbc_reset_defs[i].reset_val[dev_data->
							     chip_id]);

	if (!rbc_only) {
		u32 reg_val[NUM_DBG_RESET_REGS] = { 0 };
		u8 reset_reg_id;
		u32 block_id;

		/**
		 * Fill reset regs values
		 */
		for (block_id = 0; block_id < NUM_PHYS_BLOCKS; block_id++) {
			bool is_removed, has_reset_reg, unreset_before_dump;
			const struct dbg_block_chip *block;

			block = qed_get_dbg_block_per_chip(p_hwfn,
							   (enum block_id)
							   block_id);
			is_removed =
			    GET_FIELD(block->flags, DBG_BLOCK_CHIP_IS_REMOVED);
			has_reset_reg =
			    GET_FIELD(block->flags,
				      DBG_BLOCK_CHIP_HAS_RESET_REG);
			unreset_before_dump =
			    GET_FIELD(block->flags,
				      DBG_BLOCK_CHIP_UNRESET_BEFORE_DUMP);

			if (!is_removed && has_reset_reg && unreset_before_dump)
				reg_val[block->reset_reg_id] |=
				    BIT(block->reset_reg_bit_offset);
		}

		/**
		 * Write reset registers
		 */
		for (reset_reg_id = 0; reset_reg_id < NUM_DBG_RESET_REGS;
		     reset_reg_id++) {
			const struct dbg_reset_reg *reset_reg;
			u32 reset_reg_addr;

			reset_reg = qed_get_dbg_reset_reg(p_hwfn, reset_reg_id);

			if (GET_FIELD
			    (reset_reg->data, DBG_RESET_REG_IS_REMOVED))
				continue;

			if (reg_val[reset_reg_id]) {
				reset_reg_addr =
				    DWORDS_TO_BYTES(GET_FIELD(reset_reg->data,
							      DBG_RESET_REG_ADDR));
				qed_wr(p_hwfn,
				       p_ptt,
				       reset_reg_addr +
				       RESET_REG_UNRESET_OFFSET,
				       reg_val[reset_reg_id]);
			}
		}
	}
}

/**
 * Returns the attention block data of the specified block
 */
static const struct dbg_attn_block_type_data *qed_get_block_attn_data(struct
								      qed_hwfn
								      *p_hwfn,
								      enum
								      block_id
								      block_id,
								      enum
								      dbg_attn_type
								      attn_type)
{
	const struct dbg_attn_block *base_attn_block_arr =
	    (const struct dbg_attn_block *)p_hwfn->
	    dbg_arrays[BIN_BUF_DBG_ATTN_BLOCKS].ptr;

	return &base_attn_block_arr[block_id].per_type_data[attn_type];
}

/**
 * Returns the attention registers of the specified block
 */
static const struct dbg_attn_reg *qed_get_block_attn_regs(struct qed_hwfn
							  *p_hwfn,
							  enum block_id
							  block_id,
							  enum dbg_attn_type
							  attn_type,
							  u8 * num_attn_regs)
{
	const struct dbg_attn_block_type_data *block_type_data =
	    qed_get_block_attn_data(p_hwfn, block_id, attn_type);

	*num_attn_regs = block_type_data->num_regs;

	return (const struct dbg_attn_reg *)p_hwfn->
	    dbg_arrays[BIN_BUF_DBG_ATTN_REGS].ptr +
	    block_type_data->regs_offset;
}

/**
 * For each block, clear the status of all parities
 */
static void qed_grc_clear_all_prty(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	const struct dbg_attn_reg *attn_reg_arr;
	u8 reg_idx, num_attn_regs;
	u32 block_id;

	for (block_id = 0; block_id < NUM_PHYS_BLOCKS; block_id++) {
		if (dev_data->block_in_reset[block_id])
			continue;

		attn_reg_arr = qed_get_block_attn_regs(p_hwfn,
						       (enum block_id)block_id,
						       ATTN_TYPE_PARITY,
						       &num_attn_regs);

		for (reg_idx = 0; reg_idx < num_attn_regs; reg_idx++) {
			const struct dbg_attn_reg *reg_data =
			    &attn_reg_arr[reg_idx];
			u16 modes_buf_offset;
			bool eval_mode;

			/**
			 * Check mode
			 */
			eval_mode = GET_FIELD(reg_data->mode.data,
					      DBG_MODE_HDR_EVAL_MODE)
			    > 0;
			modes_buf_offset = GET_FIELD(reg_data->mode.data,
						     DBG_MODE_HDR_MODES_BUF_OFFSET);

			/**
			 * If Mode match: clear parity status
			 */
			if (!eval_mode ||
			    qed_is_mode_match(p_hwfn, &modes_buf_offset))
				qed_rd(p_hwfn, p_ptt,
				       DWORDS_TO_BYTES
				       (reg_data->sts_clr_address));
		}
	}
}

/**
 * Finds the meta data image in NVRAM
 */
static enum dbg_status qed_find_nvram_image(struct qed_hwfn *p_hwfn,
					    struct qed_ptt *p_ptt,
					    u32 image_type,
					    u32 *
					    nvram_offset_bytes,
					    u32 * nvram_size_bytes)
{
	u32 ret_mcp_resp, ret_mcp_param, ret_txn_size;
	struct mcp_file_att file_att;
	int nvm_result;

	/**
	 * Call NVRAM get file command
	 */
	nvm_result = qed_mcp_nvm_rd_cmd(p_hwfn,
					p_ptt,
					DRV_MSG_CODE_NVM_GET_FILE_ATT,
					image_type,
					&ret_mcp_resp,
					&ret_mcp_param,
					&ret_txn_size,
					(u32 *) & file_att, true);

	/**
	 * Check response
	 */
	if (nvm_result || (ret_mcp_resp & FW_MSG_CODE_MASK) !=
	    FW_MSG_CODE_NVM_OK)
		return DBG_STATUS_NVRAM_GET_IMAGE_FAILED;

	/**
	 * Update return values
	 */
	*nvram_offset_bytes = file_att.nvm_start_addr;
	*nvram_size_bytes = file_att.len;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DEBUG,
		   "find_nvram_image: found NVRAM image of type %d in NVRAM offset %d bytes with size %d bytes\n",
		   image_type, *nvram_offset_bytes, *nvram_size_bytes);

	/**
	 * Check alignment
	 */
	if (*nvram_size_bytes & 0x3)
		return DBG_STATUS_NON_ALIGNED_NVRAM_IMAGE;

	return DBG_STATUS_OK;
}

/**
 * Reads data from NVRAM
 */
static enum dbg_status qed_nvram_read(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      u32 nvram_offset_bytes,
				      u32 nvram_size_bytes, u32 * ret_buf)
{
	u32 ret_mcp_resp, ret_mcp_param, ret_read_size, bytes_to_copy;
	s32 bytes_left = nvram_size_bytes;
	u32 read_offset = 0, param = 0;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DEBUG,
		   "nvram_read: reading image of size %d bytes from NVRAM\n",
		   nvram_size_bytes);

	do {
		bytes_to_copy =
		    (bytes_left >
		     MCP_DRV_NVM_BUF_LEN) ? MCP_DRV_NVM_BUF_LEN : bytes_left;

		/**
		 * Call NVRAM read command
		 */
		SET_MFW_FIELD(param,
			      DRV_MB_PARAM_NVM_OFFSET,
			      nvram_offset_bytes + read_offset);
		SET_MFW_FIELD(param, DRV_MB_PARAM_NVM_LEN, bytes_to_copy);
		if (qed_mcp_nvm_rd_cmd(p_hwfn, p_ptt,
				       DRV_MSG_CODE_NVM_READ_NVRAM, param,
				       &ret_mcp_resp,
				       &ret_mcp_param, &ret_read_size,
				       (u32 *) ((u8 *) ret_buf + read_offset),
				       true))
			return DBG_STATUS_NVRAM_READ_FAILED;

		/**
		 * Check response
		 */
		if ((ret_mcp_resp & FW_MSG_CODE_MASK) != FW_MSG_CODE_NVM_OK)
			return DBG_STATUS_NVRAM_READ_FAILED;

		/**
		 * Update read offset
		 */
		read_offset += ret_read_size;
		bytes_left -= ret_read_size;
	} while (bytes_left > 0);

	return DBG_STATUS_OK;
}

/**
 * Dumps GRC registers section header. Returns the dumped size in dwords.
 * the following parameters are dumped:
 * - count: no. of dumped entries
 * - split_type: split type
 * - split_id: split ID (dumped only if split_id != SPLIT_TYPE_NONE)
 * - reg_type_name: register type name (dumped only if reg_type_name != NULL)
 */
static u32 qed_grc_dump_regs_hdr(u32 * dump_buf,
				 bool dump,
				 u32 num_reg_entries,
				 enum init_split_types split_type,
				 u8 split_id, const char *reg_type_name)
{
	u8 num_params = 2 +
	    (split_type != SPLIT_TYPE_NONE ? 1 : 0) + (reg_type_name ? 1 : 0);
	u32 offset = 0;

	offset += qed_dump_section_hdr(dump_buf + offset,
				       dump, "grc_regs", num_params);
	offset += qed_dump_num_param(dump_buf + offset,
				     dump, "count", num_reg_entries);
	offset += qed_dump_str_param(dump_buf + offset,
				     dump,
				     "split",
				     s_split_type_defs[split_type].name);
	if (split_type != SPLIT_TYPE_NONE)
		offset += qed_dump_num_param(dump_buf + offset,
					     dump, "id", split_id);
	if (reg_type_name)
		offset += qed_dump_str_param(dump_buf + offset,
					     dump, "type", reg_type_name);

	return offset;
}

/**
 * Dumps the GRC registers in the specified address range.
 * Returns the dumped size in dwords.
 * The addr and len arguments are specified in dwords.
 */
u32 qed_grc_dump_addr_range(struct qed_hwfn * p_hwfn,
			    struct qed_ptt * p_ptt,
			    u32 * dump_buf,
			    bool dump,
			    u32 addr,
			    u32 len,
			    bool wide_bus,
			    enum init_split_types split_type, u8 split_id)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u8 port_id = 0, pf_id = 0, vf_id = 0;
	bool read_using_dmae = false;
	u32 thresh;

	if (!dump)
		return len;

	switch (split_type) {
	case SPLIT_TYPE_PORT:
		port_id = split_id;
		break;
	case SPLIT_TYPE_PF:
		pf_id = split_id;
		break;
	case SPLIT_TYPE_PORT_PF:
		port_id = split_id / dev_data->num_pfs_per_port;
		pf_id = port_id + dev_data->num_ports *
		    (split_id % dev_data->num_pfs_per_port);
		break;
	case SPLIT_TYPE_VF:
		vf_id = split_id;
		break;
	default:
		break;
	}

	/**
	 * Try reading using DMAE
	 */
	if (dev_data->use_dmae && split_type != SPLIT_TYPE_VF &&
	    (len >= s_hw_type_defs[dev_data->hw_type].dmae_thresh ||
	     (PROTECT_WIDE_BUS && wide_bus))) {
		struct dmae_params dmae_params;

		/**
		 * Set DMAE params
		 */
		memset(&dmae_params, 0, sizeof(dmae_params));
		SET_FIELD(dmae_params.flags, DMAE_PARAMS_COMPLETION_DST, 1);
		switch (split_type) {
		case SPLIT_TYPE_PORT:
			SET_FIELD(dmae_params.flags, DMAE_PARAMS_PORT_VALID, 1);
			dmae_params.port_id = port_id;
			break;
		case SPLIT_TYPE_PF:
			SET_FIELD(dmae_params.flags,
				  DMAE_PARAMS_SRC_PF_VALID, 1);
			dmae_params.src_pf_id = pf_id;
			break;
		case SPLIT_TYPE_PORT_PF:
			SET_FIELD(dmae_params.flags, DMAE_PARAMS_PORT_VALID, 1);
			SET_FIELD(dmae_params.flags,
				  DMAE_PARAMS_SRC_PF_VALID, 1);
			dmae_params.port_id = port_id;
			dmae_params.src_pf_id = pf_id;
			break;
		default:
			break;
		}

		/**
		 * Execute DMAE command
		 */
		read_using_dmae = !qed_dmae_grc2host(p_hwfn,
						     p_ptt,
						     DWORDS_TO_BYTES(addr),
						     (u64) (uintptr_t)
						     (dump_buf), len,
						     &dmae_params);
		if (!read_using_dmae) {
			dev_data->use_dmae = 0;
			DP_VERBOSE(p_hwfn,
				   QED_MSG_DEBUG,
				   "Failed reading from chip using DMAE, using GRC instead\n");
		}
	}

	/**
	 * If not read using DMAE, read using GRC
	 */
	if (!read_using_dmae) {
		/**
		 * Set pretend
		 */
		if (split_type != dev_data->pretend.split_type || split_id !=
		    dev_data->pretend.split_id) {
			switch (split_type) {
			case SPLIT_TYPE_PORT:
				qed_port_pretend(p_hwfn, p_ptt, port_id);
				break;
			case SPLIT_TYPE_PF:
				qed_fid_pretend(p_hwfn, p_ptt,
						FIELD_VALUE
						(PXP_PRETEND_CONCRETE_FID_PFID,
						 pf_id));
				break;
			case SPLIT_TYPE_PORT_PF:
				qed_port_fid_pretend(p_hwfn,
						     p_ptt,
						     port_id,
						     FIELD_VALUE
						     (PXP_PRETEND_CONCRETE_FID_PFID,
						      pf_id));
				break;
			case SPLIT_TYPE_VF:
				qed_fid_pretend(p_hwfn, p_ptt,
						FIELD_VALUE
						(PXP_PRETEND_CONCRETE_FID_VFVALID,
						 1) |
						FIELD_VALUE
						(PXP_PRETEND_CONCRETE_FID_VFID,
						 vf_id));
				break;
			default:
				break;
			}

			dev_data->pretend.split_type = (u8) split_type;
			dev_data->pretend.split_id = split_id;
		}

		/**
		 * Read registers using GRC
		 */
		qed_read_regs(p_hwfn, p_ptt, dump_buf, addr, len);
	}

	/**
	 * Print log
	 */
	dev_data->num_regs_read += len;
	thresh = s_hw_type_defs[dev_data->hw_type].log_thresh;
	if ((dev_data->num_regs_read / thresh) >
	    ((dev_data->num_regs_read - len) / thresh))
		DP_VERBOSE(p_hwfn,
			   QED_MSG_DEBUG,
			   "Dumped %d registers...\n", dev_data->num_regs_read);

	return len;
}

/**
 * Dumps GRC registers sequence header. Returns the dumped size in dwords.
 * The addr and len arguments are specified in dwords.
 */
static u32 qed_grc_dump_reg_entry_hdr(u32 * dump_buf,
				      bool dump, u32 addr, u32 len)
{
	if (dump)
		*dump_buf = addr | (len << REG_DUMP_LEN_SHIFT);

	return 1;
}

/**
 * Dumps GRC registers sequence. Returns the dumped size in dwords.
 * The addr and len arguments are specified in dwords.
 */
static u32 qed_grc_dump_reg_entry(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt,
				  u32 * dump_buf,
				  bool dump,
				  u32 addr,
				  u32 len,
				  bool wide_bus,
				  enum init_split_types split_type, u8 split_id)
{
	u32 offset = 0;

	offset += qed_grc_dump_reg_entry_hdr(dump_buf, dump, addr, len);
	offset += qed_grc_dump_addr_range(p_hwfn,
					  p_ptt,
					  dump_buf + offset,
					  dump,
					  addr,
					  len, wide_bus, split_type, split_id);

	return offset;
}

/**
 * Dumps GRC registers sequence with skip cycle.
 * Returns the dumped size in dwords.
 * - addr:	start GRC address in dwords
 * - total_len:	total no. of dwords to dump
 * - read_len:	no. consecutive dwords to read
 * - skip_len:	no. of dwords to skip (and fill with zeros)
 */
static u32 qed_grc_dump_reg_entry_skip(struct qed_hwfn *p_hwfn,
				       struct qed_ptt *p_ptt,
				       u32 * dump_buf,
				       bool dump,
				       u32 addr,
				       u32 total_len,
				       u32 read_len, u32 skip_len)
{
	u32 offset = 0, reg_offset = 0;

	offset += qed_grc_dump_reg_entry_hdr(dump_buf, dump, addr, total_len);

	if (!dump)
		return offset + total_len;

	while (reg_offset < total_len) {
		u32 curr_len = min_t(u32, read_len, total_len - reg_offset);

		offset += qed_grc_dump_addr_range(p_hwfn,
						  p_ptt,
						  dump_buf + offset,
						  dump,
						  addr,
						  curr_len,
						  false, SPLIT_TYPE_NONE, 0);
		reg_offset += curr_len;
		addr += curr_len;

		if (reg_offset < total_len) {
			curr_len = min_t(u32, skip_len, total_len - skip_len);
			memset(dump_buf + offset, 0, DWORDS_TO_BYTES(curr_len));
			offset += curr_len;
			reg_offset += curr_len;
			addr += curr_len;
		}
	}

	return offset;
}

/**
 * Dumps GRC registers entries. Returns the dumped size in dwords.
 */
static u32 qed_grc_dump_regs_entries(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     struct virt_mem_desc input_regs_arr,
				     u32 * dump_buf,
				     bool dump,
				     enum init_split_types split_type,
				     u8 split_id,
				     bool block_enable[MAX_BLOCK_ID],
				     u32 * num_dumped_reg_entries)
{
	u32 i, offset = 0, input_offset = 0;
	bool mode_match = true;

	*num_dumped_reg_entries = 0;

	while (input_offset < BYTES_TO_DWORDS(input_regs_arr.size)) {
		const struct dbg_dump_cond_hdr *cond_hdr =
		    (const struct dbg_dump_cond_hdr *)input_regs_arr.ptr +
		    input_offset++;
		u16 modes_buf_offset;
		bool eval_mode;

		/**
		 * Check mode/block
		 */
		eval_mode = GET_FIELD(cond_hdr->mode.data,
				      DBG_MODE_HDR_EVAL_MODE) > 0;
		if (eval_mode) {
			modes_buf_offset = GET_FIELD(cond_hdr->mode.data,
						     DBG_MODE_HDR_MODES_BUF_OFFSET);
			mode_match = qed_is_mode_match(p_hwfn,
						       &modes_buf_offset);
		}

		if (!mode_match || !block_enable[cond_hdr->block_id]) {
			input_offset += cond_hdr->data_size;
			continue;
		}

		for (i = 0; i < cond_hdr->data_size; i++, input_offset++) {
			const struct dbg_dump_reg *reg =
			    (const struct dbg_dump_reg *)input_regs_arr.ptr
			    + input_offset;

			offset += qed_grc_dump_reg_entry(p_hwfn,
							 p_ptt,
							 dump_buf + offset,
							 dump,
							 GET_FIELD(reg->data,
								   DBG_DUMP_REG_ADDRESS),
							 GET_FIELD(reg->data,
								   DBG_DUMP_REG_LENGTH),
							 GET_FIELD(reg->data,
								   DBG_DUMP_REG_WIDE_BUS),
							 split_type, split_id);
			(*num_dumped_reg_entries)++;
		}
	}

	return offset;
}

/**
 * Dumps GRC registers entries. Returns the dumped size in dwords.
 */
static u32 qed_grc_dump_split_data(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt,
				   struct virt_mem_desc input_regs_arr,
				   u32 * dump_buf,
				   bool dump,
				   bool block_enable[MAX_BLOCK_ID],
				   enum init_split_types split_type,
				   u8 split_id, const char *reg_type_name)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	enum init_split_types hdr_split_type = split_type;
	u32 num_dumped_reg_entries, offset;
	u8 hdr_split_id = split_id;

	/**
	 * In PORT_PF split type, print a port split header
	 */
	if (split_type == SPLIT_TYPE_PORT_PF) {
		hdr_split_type = SPLIT_TYPE_PORT;
		hdr_split_id = split_id / dev_data->num_pfs_per_port;
	}

	/**
	 * Calculate register dump header size (and skip it for now)
	 */
	offset = qed_grc_dump_regs_hdr(dump_buf,
				       false,
				       0,
				       hdr_split_type,
				       hdr_split_id, reg_type_name);

	/**
	 * Dump registers
	 */
	offset += qed_grc_dump_regs_entries(p_hwfn,
					    p_ptt,
					    input_regs_arr,
					    dump_buf + offset,
					    dump,
					    split_type,
					    split_id,
					    block_enable,
					    &num_dumped_reg_entries);

	/**
	 * Write register dump header
	 */
	if (dump && num_dumped_reg_entries > 0)
		qed_grc_dump_regs_hdr(dump_buf,
				      dump,
				      num_dumped_reg_entries,
				      hdr_split_type,
				      hdr_split_id, reg_type_name);

	return num_dumped_reg_entries > 0 ? offset : 0;
}

/**
 * Dumps registers according to the input registers array. Returns the dumped
 * size in dwords.
 */
static u32 qed_grc_dump_registers(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt,
				  u32 * dump_buf,
				  bool dump,
				  bool block_enable[MAX_BLOCK_ID],
				  const char *reg_type_name)
{
	struct virt_mem_desc *dbg_buf =
	    &p_hwfn->dbg_arrays[BIN_BUF_DBG_DUMP_REG];
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u32 offset = 0, input_offset = 0;

	while (input_offset < BYTES_TO_DWORDS(dbg_buf->size)) {
		const struct dbg_dump_split_hdr *split_hdr;
		struct virt_mem_desc curr_input_regs_arr;
		enum init_split_types split_type;
		u16 split_count = 0;
		u32 split_data_size;
		u16 split_id;

		split_hdr =
		    (const struct dbg_dump_split_hdr *)dbg_buf->ptr +
		    input_offset++;
		split_type =
		    (enum init_split_types)GET_FIELD(split_hdr->hdr,
						     DBG_DUMP_SPLIT_HDR_SPLIT_TYPE_ID);
		split_data_size = GET_FIELD(split_hdr->hdr,
					    DBG_DUMP_SPLIT_HDR_DATA_SIZE);
		curr_input_regs_arr.ptr =
		    (u32 *) p_hwfn->dbg_arrays[BIN_BUF_DBG_DUMP_REG].ptr +
		    input_offset;
		curr_input_regs_arr.size = DWORDS_TO_BYTES(split_data_size);

		switch (split_type) {
		case SPLIT_TYPE_NONE:
			split_count = 1;
			break;
		case SPLIT_TYPE_PORT:
			split_count = dev_data->num_ports;
			break;
		case SPLIT_TYPE_PF:
		case SPLIT_TYPE_PORT_PF:
			split_count = dev_data->num_ports *
			    dev_data->num_pfs_per_port;
			break;
		case SPLIT_TYPE_VF:
			split_count = dev_data->num_vfs;
			break;
		default:
			return 0;
		}
		;

		for (split_id = 0; split_id < split_count; split_id++)
			/* Split id is u8 in all function call hirarchy. So cast it here itself */
			offset += qed_grc_dump_split_data(p_hwfn,
							  p_ptt,
							  curr_input_regs_arr,
							  dump_buf + offset,
							  dump,
							  block_enable,
							  split_type,
							  (u8) split_id,
							  reg_type_name);

		input_offset += split_data_size;
	}

	/**
	 * Cancel pretends (pretend to original PF)
	 */
	if (dump) {
		qed_fid_pretend(p_hwfn, p_ptt,
				FIELD_VALUE(PXP_PRETEND_CONCRETE_FID_PFID,
					    p_hwfn->rel_pf_id));
		dev_data->pretend.split_type = SPLIT_TYPE_NONE;
		dev_data->pretend.split_id = 0;
	}

	return offset;
}

/**
 * Dump reset registers. Returns the dumped size in dwords.
 */
static u32 qed_grc_dump_reset_regs(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt,
				   u32 * dump_buf, bool dump)
{
	u32 offset = 0, num_regs = 0;
	u8 reset_reg_id;

	/**
	 * Calculate header size
	 */
	offset += qed_grc_dump_regs_hdr(dump_buf,
					false,
					0, SPLIT_TYPE_NONE, 0, "RESET_REGS");

	/**
	 * Write reset registers
	 */
	for (reset_reg_id = 0; reset_reg_id < NUM_DBG_RESET_REGS;
	     reset_reg_id++) {
		const struct dbg_reset_reg *reset_reg;
		u32 reset_reg_addr;

		reset_reg = qed_get_dbg_reset_reg(p_hwfn, reset_reg_id);

		if (GET_FIELD(reset_reg->data, DBG_RESET_REG_IS_REMOVED))
			continue;

		reset_reg_addr = GET_FIELD(reset_reg->data, DBG_RESET_REG_ADDR);
		offset += qed_grc_dump_reg_entry(p_hwfn,
						 p_ptt,
						 dump_buf + offset,
						 dump,
						 reset_reg_addr,
						 1, false, SPLIT_TYPE_NONE, 0);
		num_regs++;
	}

	/**
	 * Write header
	 */
	if (dump)
		qed_grc_dump_regs_hdr(dump_buf,
				      true,
				      num_regs,
				      SPLIT_TYPE_NONE, 0, "RESET_REGS");

	return offset;
}

/**
 * Dump registers that are modified during GRC Dump and therefore must be
 * dumped first. Returns the dumped size in dwords.
 */
static u32 qed_grc_dump_modified_regs(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      u32 * dump_buf, bool dump)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u32 block_id, offset = 0, stall_regs_offset;
	const struct dbg_attn_reg *attn_reg_arr;
	u8 storm_id, reg_idx, num_attn_regs;
	u32 num_reg_entries = 0;

	/**
	 * Write empty header for attention registers
	 */
	offset += qed_grc_dump_regs_hdr(dump_buf,
					false,
					0, SPLIT_TYPE_NONE, 0, "ATTN_REGS");

	/**
	 * Write parity registers
	 */
	for (block_id = 0; block_id < NUM_PHYS_BLOCKS; block_id++) {
		if (dev_data->block_in_reset[block_id] && dump)
			continue;

		attn_reg_arr = qed_get_block_attn_regs(p_hwfn,
						       (enum block_id)block_id,
						       ATTN_TYPE_PARITY,
						       &num_attn_regs);

		for (reg_idx = 0; reg_idx < num_attn_regs; reg_idx++) {
			const struct dbg_attn_reg *reg_data =
			    &attn_reg_arr[reg_idx];
			u16 modes_buf_offset;
			bool eval_mode;

			/**
			 * Check mode
			 */
			eval_mode = GET_FIELD(reg_data->mode.data,
					      DBG_MODE_HDR_EVAL_MODE)
			    > 0;
			modes_buf_offset = GET_FIELD(reg_data->mode.data,
						     DBG_MODE_HDR_MODES_BUF_OFFSET);
			if (eval_mode &&
			    !qed_is_mode_match(p_hwfn, &modes_buf_offset))
				continue;

			/**
			 * Mode match: read & dump registers
			 */
			offset += qed_grc_dump_reg_entry(p_hwfn,
							 p_ptt,
							 dump_buf +
							 offset,
							 dump,
							 reg_data->mask_address,
							 1,
							 false,
							 SPLIT_TYPE_NONE, 0);
			offset += qed_grc_dump_reg_entry(p_hwfn,
							 p_ptt,
							 dump_buf +
							 offset,
							 dump,
							 GET_FIELD(reg_data->
								   data,
								   DBG_ATTN_REG_STS_ADDRESS),
							 1, false,
							 SPLIT_TYPE_NONE, 0);
			num_reg_entries += 2;
		}
	}

	/**
	 * Overwrite header for attention registers
	 */
	if (dump)
		qed_grc_dump_regs_hdr(dump_buf,
				      true,
				      num_reg_entries,
				      SPLIT_TYPE_NONE, 0, "ATTN_REGS");

	/**
	 * Write empty header for stall registers
	 */
	stall_regs_offset = offset;
	offset += qed_grc_dump_regs_hdr(dump_buf,
					false, 0, SPLIT_TYPE_NONE, 0, "REGS");

	/**
	 * Write Storm stall status registers
	 */
	for (storm_id = 0, num_reg_entries = 0; storm_id < MAX_DBG_STORMS;
	     storm_id++) {
		struct storm_defs *storm = &s_storm_defs[storm_id];

		if (dev_data->block_in_reset[storm->sem_block_id] && dump)
			continue;

		offset += qed_grc_dump_reg_entry(p_hwfn,
						 p_ptt,
						 dump_buf + offset,
						 dump,
						 BYTES_TO_DWORDS
						 (storm->sem_fast_mem_addr +
						  SEM_FAST_REG_STALLED), 1,
						 false, SPLIT_TYPE_NONE, 0);
		num_reg_entries++;
	}

	/**
	 * Overwrite header for stall registers
	 */
	if (dump)
		qed_grc_dump_regs_hdr(dump_buf + stall_regs_offset,
				      true,
				      num_reg_entries,
				      SPLIT_TYPE_NONE, 0, "REGS");

	return offset;
}

/**
 * Dumps registers that can't be represented in the debug arrays
 */
static u32 qed_grc_dump_special_regs(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     u32 * dump_buf, bool dump)
{
	u32 offset = 0;

	offset += qed_grc_dump_regs_hdr(dump_buf,
					dump, 2, SPLIT_TYPE_NONE, 0, "REGS");

	/**
	 * Dump R/TDIF_REG_DEBUG_ERROR_INFO_SIZE (every 8'th register should be
	 * skipped).
	 */
	offset += qed_grc_dump_reg_entry_skip(p_hwfn,
					      p_ptt,
					      dump_buf + offset,
					      dump,
					      BYTES_TO_DWORDS
					      (RDIF_REG_DEBUG_ERROR_INFO),
					      RDIF_REG_DEBUG_ERROR_INFO_SIZE, 7,
					      1);
	offset +=
	    qed_grc_dump_reg_entry_skip(p_hwfn, p_ptt, dump_buf + offset, dump,
					BYTES_TO_DWORDS
					(TDIF_REG_DEBUG_ERROR_INFO),
					TDIF_REG_DEBUG_ERROR_INFO_SIZE, 7, 1);

	return offset;
}

/**
 * Dumps a GRC memory header (section and params). Returns the dumped size in
 * dwords. The following parameters are dumped:
 * - name:	   dumped only if it's not NULL.
 * - addr:	   in dwords, dumped only if name is NULL.
 * - len:	   in dwords, always dumped.
 * - width:	   dumped if it's not zero.
 * - packed:	   dumped only if it's not false.
 * - mem_group:	   always dumped.
 * - is_storm:	   true only if the memory is related to a Storm.
 * - storm_letter: valid only if is_storm is true.
 */
static u32 qed_grc_dump_mem_hdr(struct qed_hwfn *p_hwfn,
				u32 * dump_buf,
				bool dump,
				const char *name,
				u32 addr,
				u32 len,
				u32 bit_width,
				bool packed,
				const char *mem_group, char storm_letter)
{
	u8 num_params = 3;
	u32 offset = 0;
	char buf[64] = { '\0' };

	if (!len)
		DP_NOTICE(p_hwfn,
			  "Unexpected GRC Dump error: dumped memory size must be non-zero\n");

	if (bit_width)
		num_params++;
	if (packed)
		num_params++;

	/**
	 * Dump section header
	 */
	offset += qed_dump_section_hdr(dump_buf + offset,
				       dump, "grc_mem", num_params);

	if (name) {
		/**
		 * Dump name
		 */
		if (storm_letter) {
			strcpy(buf, "?STORM_");
			buf[0] = storm_letter;
			strncpy(buf + strlen(buf), name, 63 - strlen(buf));
		} else {
			strncpy(buf, name, 63);
		}

		offset += qed_dump_str_param(dump_buf + offset,
					     dump, "name", buf);
	} else { /**
		  * Dump address
		  */
		u32 addr_in_bytes = DWORDS_TO_BYTES(addr);

		offset += qed_dump_num_param(dump_buf + offset,
					     dump, "addr", addr_in_bytes);
	}

	/**
	 * Dump len
	 */
	offset += qed_dump_num_param(dump_buf + offset, dump, "len", len);

	/**
	 * Dump bit width
	 */
	if (bit_width)
		offset += qed_dump_num_param(dump_buf + offset,
					     dump, "width", bit_width);

	/**
	 * Dump packed
	 */
	if (packed)
		offset += qed_dump_num_param(dump_buf + offset,
					     dump, "packed", 1);

	/**
	 * Dump reg type
	 */
	if (storm_letter) {
		strcpy(buf, "?STORM_");
		buf[0] = storm_letter;
		strcpy(buf + strlen(buf), mem_group);
	} else {
		strcpy(buf, mem_group);
	}

	offset += qed_dump_str_param(dump_buf + offset, dump, "type", buf);

	return offset;
}

/**
 * Dumps a single GRC memory. If name is NULL, the memory is stored by address.
 * Returns the dumped size in dwords.
 * The addr and len arguments are specified in dwords.
 */
static u32 qed_grc_dump_mem(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt,
			    u32 * dump_buf,
			    bool dump,
			    const char *name,
			    u32 addr,
			    u32 len,
			    bool wide_bus,
			    u32 bit_width,
			    bool packed,
			    const char *mem_group, char storm_letter)
{
	u32 offset = 0;

	offset += qed_grc_dump_mem_hdr(p_hwfn,
				       dump_buf + offset,
				       dump,
				       name,
				       addr,
				       len,
				       bit_width,
				       packed, mem_group, storm_letter);
	offset += qed_grc_dump_addr_range(p_hwfn,
					  p_ptt,
					  dump_buf + offset,
					  dump,
					  addr,
					  len, wide_bus, SPLIT_TYPE_NONE, 0);

	return offset;
}

/**
 * Dumps GRC memories entries. Returns the dumped size in dwords. */
static u32 qed_grc_dump_mem_entries(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt,
				    struct virt_mem_desc input_mems_arr,
				    u32 * dump_buf, bool dump)
{
	u32 i, offset = 0, input_offset = 0;
	bool mode_match = true;

	while (input_offset < BYTES_TO_DWORDS(input_mems_arr.size)) {
		const struct dbg_dump_cond_hdr *cond_hdr;
		u16 modes_buf_offset;
		u32 num_entries;
		bool eval_mode;

		cond_hdr =
		    (const struct dbg_dump_cond_hdr *)input_mems_arr.ptr +
		    input_offset++;
		num_entries = cond_hdr->data_size / MEM_DUMP_ENTRY_SIZE_DWORDS;

		/**
		 * Check required mode
		 */
		eval_mode = GET_FIELD(cond_hdr->mode.data,
				      DBG_MODE_HDR_EVAL_MODE) > 0;
		if (eval_mode) {
			modes_buf_offset = GET_FIELD(cond_hdr->mode.data,
						     DBG_MODE_HDR_MODES_BUF_OFFSET);
			mode_match = qed_is_mode_match(p_hwfn,
						       &modes_buf_offset);
		}

		if (!mode_match) {
			input_offset += cond_hdr->data_size;
			continue;
		}

		for (i = 0; i < num_entries;
		     i++, input_offset += MEM_DUMP_ENTRY_SIZE_DWORDS) {
			const struct dbg_dump_mem *mem =
			    (const struct dbg_dump_mem *)((u32 *)
							  input_mems_arr.ptr
							  + input_offset);
			const struct dbg_block *block;
			char storm_letter = 0;
			u32 mem_addr, mem_len;
			bool mem_wide_bus;
			u8 mem_group_id;

			mem_group_id = GET_FIELD(mem->dword0,
						 DBG_DUMP_MEM_MEM_GROUP_ID);
			if (mem_group_id >= MEM_GROUPS_NUM) {
				DP_NOTICE(p_hwfn, "Invalid mem_group_id\n");
				return 0;
			}

			if (!qed_grc_is_mem_included(p_hwfn,
						     (enum block_id)
						     cond_hdr->block_id,
						     mem_group_id))
				continue;

			mem_addr = GET_FIELD(mem->dword0, DBG_DUMP_MEM_ADDRESS);
			mem_len = GET_FIELD(mem->dword1, DBG_DUMP_MEM_LENGTH);
			mem_wide_bus = GET_FIELD(mem->dword1,
						 DBG_DUMP_MEM_WIDE_BUS);

			block = get_dbg_block(p_hwfn,
					      (enum block_id)cond_hdr->
					      block_id);

			/**
			 * If memory is associated with Storm, udpate Storm details
			 */
			if (block->associated_storm_letter)
				storm_letter = block->associated_storm_letter;

			/**
			 * Dump memory
			 */
			offset += qed_grc_dump_mem(p_hwfn,
						   p_ptt,
						   dump_buf + offset,
						   dump,
						   NULL,
						   mem_addr,
						   mem_len,
						   mem_wide_bus,
						   0,
						   false,
						   s_mem_group_names
						   [mem_group_id],
						   storm_letter);
		}
	}

	return offset;
}

/**
 * Dumps GRC memories according to the input array dump_mem.
 * Returns the dumped size in dwords.
 */
static u32 qed_grc_dump_memories(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt,
				 u32 * dump_buf, bool dump)
{
	struct virt_mem_desc *dbg_buf =
	    &p_hwfn->dbg_arrays[BIN_BUF_DBG_DUMP_MEM];
	u32 offset = 0, input_offset = 0;

	while (input_offset < BYTES_TO_DWORDS(dbg_buf->size)) {
		const struct dbg_dump_split_hdr *split_hdr;
		struct virt_mem_desc curr_input_mems_arr;
		enum init_split_types split_type;
		u32 split_data_size;

		split_hdr =
		    (const struct dbg_dump_split_hdr *)dbg_buf->ptr +
		    input_offset++;
		split_type =
		    (enum init_split_types)GET_FIELD(split_hdr->hdr,
						     DBG_DUMP_SPLIT_HDR_SPLIT_TYPE_ID);
		split_data_size = GET_FIELD(split_hdr->hdr,
					    DBG_DUMP_SPLIT_HDR_DATA_SIZE);
		curr_input_mems_arr.ptr = (u32 *) dbg_buf->ptr + input_offset;
		curr_input_mems_arr.size = DWORDS_TO_BYTES(split_data_size);

		if (split_type == SPLIT_TYPE_NONE)
			offset += qed_grc_dump_mem_entries(p_hwfn,
							   p_ptt,
							   curr_input_mems_arr,
							   dump_buf + offset,
							   dump);
		else
			DP_NOTICE(p_hwfn,
				  "Dumping split memories is currently not supported\n");

		input_offset += split_data_size;
	}

	return offset;
}

/**
 * Dumps GRC context data for the specified Storm.
 * Returns the dumped size in dwords.
 * The lid_size argument is specified in quad-regs.
 */
static u32 qed_grc_dump_ctx_data(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt,
				 u32 * dump_buf,
				 bool dump,
				 const char *name,
				 u32 num_lids,
				 enum cm_ctx_types ctx_type, u8 storm_id)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct storm_defs *storm = &s_storm_defs[storm_id];
	u32 i, lid, lid_size, total_size;
	u32 rd_reg_addr, offset = 0;

	/**
	 * Convert quad-regs to dwords
	 */
	lid_size = storm->cm_ctx_lid_sizes[dev_data->chip_id][ctx_type] * 4;

	if (!lid_size)
		return 0;

	total_size = num_lids * lid_size;

	offset += qed_grc_dump_mem_hdr(p_hwfn,
				       dump_buf + offset,
				       dump,
				       name,
				       0,
				       total_size,
				       lid_size * 32,
				       false, name, storm->letter);

	if (!dump)
		return offset + total_size;

	rd_reg_addr = BYTES_TO_DWORDS(storm->cm_ctx_rd_addr[ctx_type]);

	/**
	 * Dump context data
	 */
	for (lid = 0; lid < num_lids; lid++) {
		for (i = 0; i < lid_size; i++) {
			qed_wr(p_hwfn,
			       p_ptt, storm->cm_ctx_wr_addr, (i << 9) | lid);
			offset += qed_grc_dump_addr_range(p_hwfn,
							  p_ptt,
							  dump_buf + offset,
							  dump,
							  rd_reg_addr,
							  1,
							  false,
							  SPLIT_TYPE_NONE, 0);
		}
	}

	return offset;
}

/**
 * Dumps GRC contexts. Returns the dumped size in dwords.
 */
static u32 qed_grc_dump_ctx(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, u32 * dump_buf, bool dump)
{
	u32 offset = 0, num_ltids;
	u8 storm_id;

	num_ltids = NUM_OF_LTIDS;

	for (storm_id = 0; storm_id < MAX_DBG_STORMS; storm_id++) {
		if (!qed_grc_is_storm_included(p_hwfn,
					       (enum dbg_storms)storm_id))
			continue;

		/**
		 * Dump Conn AG context size
		 */
		offset += qed_grc_dump_ctx_data(p_hwfn,
						p_ptt,
						dump_buf + offset,
						dump,
						"CONN_AG_CTX",
						NUM_OF_LCIDS,
						CM_CTX_CONN_AG, storm_id);

		/**
		 * Dump Conn ST context size
		 */
		offset += qed_grc_dump_ctx_data(p_hwfn,
						p_ptt,
						dump_buf + offset,
						dump,
						"CONN_ST_CTX",
						NUM_OF_LCIDS,
						CM_CTX_CONN_ST, storm_id);

		/**
		 * Dump Task AG context size
		 */
		offset += qed_grc_dump_ctx_data(p_hwfn,
						p_ptt,
						dump_buf + offset,
						dump,
						"TASK_AG_CTX",
						num_ltids,
						CM_CTX_TASK_AG, storm_id);

		/**
		 * Dump Task ST context size
		 */
		offset += qed_grc_dump_ctx_data(p_hwfn,
						p_ptt,
						dump_buf + offset,
						dump,
						"TASK_ST_CTX",
						num_ltids,
						CM_CTX_TASK_ST, storm_id);
	}

	return offset;
}

#define VFC_STATUS_RESP_READY_BIT       0
#define VFC_STATUS_BUSY_BIT             1
#define VFC_STATUS_SENDING_CMD_BIT      2

#define VFC_POLLING_DELAY_MS    1
#define VFC_POLLING_COUNT               20

/**
 * Reads data from VFC. Returns the number of dwords read (0 on error).
 * Sizes are specified in dwords.
 */
static u32 qed_grc_dump_read_from_vfc(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      struct storm_defs *storm,
				      u32 * cmd_data,
				      u32 cmd_size,
				      u32 * addr_data,
				      u32 addr_size,
				      u32 resp_size, u32 * dump_buf)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u32 vfc_status, polling_ms, polling_count = 0, i;
	bool is_ready = false;

	polling_ms = VFC_POLLING_DELAY_MS *
	    s_hw_type_defs[dev_data->hw_type].delay_factor;

	/**
	 * Write VFC command
	 */
	ARR_REG_WR(p_hwfn,
		   p_ptt,
		   storm->sem_fast_mem_addr + SEM_FAST_REG_VFC_DATA_WR,
		   cmd_data, cmd_size);

	/**
	 * Write VFC address
	 */
	ARR_REG_WR(p_hwfn,
		   p_ptt,
		   storm->sem_fast_mem_addr + SEM_FAST_REG_VFC_ADDR,
		   addr_data, addr_size);

	/**
	 * Read response
	 */
	for (i = 0; i < resp_size; i++) {
		/**
		 * Poll until ready
		 */
		do {
			qed_grc_dump_addr_range(p_hwfn,
						p_ptt,
						&vfc_status,
						true,
						BYTES_TO_DWORDS
						(storm->sem_fast_mem_addr +
						 SEM_FAST_REG_VFC_STATUS), 1,
						false, SPLIT_TYPE_NONE, 0);
			is_ready = vfc_status & BIT(VFC_STATUS_RESP_READY_BIT);

			if (!is_ready) {
				if (polling_count++ == VFC_POLLING_COUNT)
					return 0;

				msleep(polling_ms);
			}
		} while (!is_ready);

		qed_grc_dump_addr_range(p_hwfn,
					p_ptt,
					dump_buf + i,
					true,
					BYTES_TO_DWORDS(storm->sem_fast_mem_addr
							+
							SEM_FAST_REG_VFC_DATA_RD),
					1, false, SPLIT_TYPE_NONE, 0);
	}

	return resp_size;
}

/**
 * Dump VFC CAM. Returns the dumped size in dwords.
 */
static u32 qed_grc_dump_vfc_cam(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt,
				u32 * dump_buf, bool dump, u8 storm_id)
{
	u32 total_size = VFC_CAM_NUM_ROWS * VFC_CAM_RESP_DWORDS;
	struct storm_defs *storm = &s_storm_defs[storm_id];
	u32 cam_addr[VFC_CAM_ADDR_DWORDS] = { 0 };
	u32 cam_cmd[VFC_CAM_CMD_DWORDS] = { 0 };
	u32 row, offset = 0;

	offset += qed_grc_dump_mem_hdr(p_hwfn,
				       dump_buf + offset,
				       dump,
				       "vfc_cam",
				       0,
				       total_size,
				       256, false, "vfc_cam", storm->letter);

	if (!dump)
		return offset + total_size;

	/**
	 * Prepare CAM address
	 */
	SET_VAR_FIELD(cam_addr, VFC_CAM_ADDR, OP, VFC_OPCODE_CAM_RD);

	/**
	 * Read VFC CAM data
	 */
	for (row = 0; row < VFC_CAM_NUM_ROWS; row++) {
		SET_VAR_FIELD(cam_cmd, VFC_CAM_CMD, ROW, row);
		offset += qed_grc_dump_read_from_vfc(p_hwfn,
						     p_ptt,
						     storm,
						     cam_cmd,
						     VFC_CAM_CMD_DWORDS,
						     cam_addr,
						     VFC_CAM_ADDR_DWORDS,
						     VFC_CAM_RESP_DWORDS,
						     dump_buf + offset);
	}

	return offset;
}

/**
 * Dump VFC RAM. Returns the dumped size in dwords.
 */
static u32 qed_grc_dump_vfc_ram(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt,
				u32 * dump_buf,
				bool dump,
				u8 storm_id, struct vfc_ram_defs *ram_defs)
{
	u32 total_size = ram_defs->num_rows * VFC_RAM_RESP_DWORDS;
	struct storm_defs *storm = &s_storm_defs[storm_id];
	u32 ram_addr[VFC_RAM_ADDR_DWORDS] = { 0 };
	u32 ram_cmd[VFC_RAM_CMD_DWORDS] = { 0 };
	u32 row, offset = 0;

	offset += qed_grc_dump_mem_hdr(p_hwfn,
				       dump_buf + offset,
				       dump,
				       ram_defs->mem_name,
				       0,
				       total_size,
				       256,
				       false,
				       ram_defs->type_name, storm->letter);

	if (!dump)
		return offset + total_size;

	/**
	 * Prepare RAM address
	 */
	SET_VAR_FIELD(ram_addr, VFC_RAM_ADDR, OP, VFC_OPCODE_RAM_RD);

	/**
	 * Read VFC RAM data
	 */
	for (row = ram_defs->base_row;
	     row < ram_defs->base_row + ram_defs->num_rows; row++) {
		SET_VAR_FIELD(ram_addr, VFC_RAM_ADDR, ROW, row);
		offset += qed_grc_dump_read_from_vfc(p_hwfn,
						     p_ptt,
						     storm,
						     ram_cmd,
						     VFC_RAM_CMD_DWORDS,
						     ram_addr,
						     VFC_RAM_ADDR_DWORDS,
						     VFC_RAM_RESP_DWORDS,
						     dump_buf + offset);
	}

	return offset;
}

/**
 * Dumps GRC VFC data. Returns the dumped size in dwords.
 */
static u32 qed_grc_dump_vfc(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, u32 * dump_buf, bool dump)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u8 storm_id, i;
	u32 offset = 0;

	for (storm_id = 0; storm_id < MAX_DBG_STORMS; storm_id++) {
		if (!qed_grc_is_storm_included(p_hwfn,
					       (enum dbg_storms)storm_id) ||
		    !s_storm_defs[storm_id].has_vfc ||
		    (storm_id == DBG_PSTORM_ID &&
		     (dev_data->hw_type == HW_TYPE_EMUL_REDUCED ||
		      dev_data->hw_type == HW_TYPE_FPGA)))
			continue;

		/**
		 * Read CAM
		 */
		offset += qed_grc_dump_vfc_cam(p_hwfn,
					       p_ptt,
					       dump_buf + offset,
					       dump, storm_id);

		/**
		 * Read RAM
		 */
		for (i = 0; i < NUM_VFC_RAM_TYPES; i++)
			offset += qed_grc_dump_vfc_ram(p_hwfn,
						       p_ptt,
						       dump_buf + offset,
						       dump,
						       storm_id,
						       &s_vfc_ram_defs[i]);
	}

	return offset;
}

/**
 * Dumps GRC RSS data. Returns the dumped size in dwords.
 */
static u32 qed_grc_dump_rss(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, u32 * dump_buf, bool dump)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u32 offset = 0;
	u8 rss_mem_id;

	for (rss_mem_id = 0; rss_mem_id < NUM_RSS_MEM_TYPES; rss_mem_id++) {
		u32 rss_addr, num_entries, total_dwords;
		struct rss_mem_defs *rss_defs;
		bool packed;

		rss_defs = &s_rss_mem_defs[rss_mem_id];
		rss_addr = rss_defs->addr;
		num_entries = rss_defs->num_entries[dev_data->chip_id];
		total_dwords = (num_entries * rss_defs->entry_width) / 32;
		packed = (rss_defs->entry_width == 16);

		offset += qed_grc_dump_mem_hdr(p_hwfn,
					       dump_buf + offset,
					       dump,
					       rss_defs->mem_name,
					       0,
					       total_dwords,
					       rss_defs->entry_width,
					       packed, rss_defs->type_name, 0);

		/**
		 * Dump RSS data
		 */
		if (!dump) {
			offset += total_dwords;
			continue;
		}

		while (total_dwords) {
			u32 num_dwords_to_read = min_t(u32,
						       RSS_REG_RSS_RAM_DATA_SIZE,
						       total_dwords);

			qed_wr(p_hwfn, p_ptt, RSS_REG_RSS_RAM_ADDR, rss_addr);
			offset += qed_grc_dump_addr_range(p_hwfn,
							  p_ptt,
							  dump_buf +
							  offset,
							  dump,
							  BYTES_TO_DWORDS
							  (RSS_REG_RSS_RAM_DATA),
							  num_dwords_to_read,
							  false,
							  SPLIT_TYPE_NONE, 0);
			total_dwords -= num_dwords_to_read;
			rss_addr++;
		}
	}

	return offset;
}

/**
 * Dumps GRC Big RAM. Returns the dumped size in dwords.
 */
static u32 qed_grc_dump_big_ram(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt,
				u32 * dump_buf, bool dump, u8 big_ram_id)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u32 block_size, ram_size, offset = 0, reg_val, i;
	char mem_name[12] = "???_BIG_RAM";
	char type_name[8] = "???_RAM";
	struct big_ram_defs *big_ram;

	big_ram = &s_big_ram_defs[big_ram_id];
	ram_size = big_ram->ram_size[dev_data->chip_id];

	reg_val = qed_rd(p_hwfn, p_ptt, big_ram->is_256b_reg_addr);
	block_size = reg_val &
	    BIT(big_ram->is_256b_bit_offset[dev_data->chip_id]) ? 256 : 128;

	strncpy(type_name, big_ram->instance_name, BIG_RAM_NAME_LEN);
	strncpy(mem_name, big_ram->instance_name, BIG_RAM_NAME_LEN);

	/**
	 * Dump memory header
	 */
	offset += qed_grc_dump_mem_hdr(p_hwfn,
				       dump_buf + offset,
				       dump,
				       mem_name,
				       0,
				       ram_size,
				       block_size * 8, false, type_name, 0);

	/**
	 * Read and dump Big RAM data
	 */
	if (!dump)
		return offset + ram_size;

	/**
	 * Dump Big RAM
	 */
	for (i = 0; i < DIV_ROUND_UP(ram_size, BRB_REG_BIG_RAM_DATA_SIZE); i++) {
		qed_wr(p_hwfn, p_ptt, big_ram->addr_reg_addr, i);
		offset += qed_grc_dump_addr_range(p_hwfn,
						  p_ptt,
						  dump_buf + offset,
						  dump,
						  BYTES_TO_DWORDS
						  (big_ram->data_reg_addr),
						  BRB_REG_BIG_RAM_DATA_SIZE,
						  false, SPLIT_TYPE_NONE, 0);
	}

	return offset;
}

/**
 * Dumps MCP scratchpad. Returns the dumped size in dwords.
 */
static u32 qed_grc_dump_mcp(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, u32 * dump_buf, bool dump)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	bool block_enable[MAX_BLOCK_ID] = { 0 };
	bool halted = false;
	u32 offset = 0;

	/**
	 * Halt MCP
	 */
	if (dump && dev_data->hw_type == HW_TYPE_ASIC &&
	    !qed_grc_get_param(p_hwfn, DBG_GRC_PARAM_NO_MCP)) {
		halted = !qed_mcp_halt(p_hwfn, p_ptt);
		if (!halted)
			DP_NOTICE(p_hwfn, "MCP halt failed!\n");
	}

	/**
	 * Dump MCP scratchpad
	 */
	offset += qed_grc_dump_mem(p_hwfn,
				   p_ptt,
				   dump_buf + offset,
				   dump,
				   NULL,
				   BYTES_TO_DWORDS(MCP_REG_SCRATCH),
				   MCP_REG_SCRATCH_SIZE,
				   false, 0, false, "MCP", 0);

	/**
	 * Dump MCP cpu_reg_file
	 */
	offset += qed_grc_dump_mem(p_hwfn,
				   p_ptt,
				   dump_buf + offset,
				   dump,
				   NULL,
				   BYTES_TO_DWORDS(MCP_REG_CPU_REG_FILE),
				   MCP_REG_CPU_REG_FILE_SIZE,
				   false, 0, false, "MCP", 0);

	/**
	 * Dump MCP registers
	 */
	block_enable[BLOCK_MCP] = true;
	offset += qed_grc_dump_registers(p_hwfn,
					 p_ptt,
					 dump_buf + offset,
					 dump, block_enable, "MCP");

	/**
	 * Dump required non-MCP registers
	 */
	offset += qed_grc_dump_regs_hdr(dump_buf + offset,
					dump, 1, SPLIT_TYPE_NONE, 0, "MCP");
	offset += qed_grc_dump_reg_entry(p_hwfn,
					 p_ptt,
					 dump_buf + offset,
					 dump,
					 BYTES_TO_DWORDS
					 (MISC_REG_SHARED_MEM_ADDR), 1, false,
					 SPLIT_TYPE_NONE, 0);

	/**
	 * Release MCP
	 */
	if (halted && qed_mcp_resume(p_hwfn, p_ptt))
		DP_NOTICE(p_hwfn, "Failed to resume MCP after halt!\n");

	return offset;
}

/**
 * Dumps the tbus indirect memory for all PHYs.
 * Returns the dumped size in dwords.
 */
static u32 qed_grc_dump_phy(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, u32 * dump_buf, bool dump)
{
	u32 offset = 0, tbus_lo_offset, tbus_hi_offset;
	char mem_name[32];
	u8 phy_id;

	for (phy_id = 0; phy_id < ARRAY_SIZE(s_phy_defs); phy_id++) {
		u32 addr_lo_addr, addr_hi_addr, data_lo_addr, data_hi_addr;
		struct phy_defs *phy_defs;
		u8 *bytes_buf;

		phy_defs = &s_phy_defs[phy_id];
		addr_lo_addr = phy_defs->base_addr +
		    phy_defs->tbus_addr_lo_addr;
		addr_hi_addr = phy_defs->base_addr +
		    phy_defs->tbus_addr_hi_addr;
		data_lo_addr = phy_defs->base_addr +
		    phy_defs->tbus_data_lo_addr;
		data_hi_addr = phy_defs->base_addr +
		    phy_defs->tbus_data_hi_addr;

		if (scnprintf(mem_name, sizeof(mem_name), "tbus_%s",
			      phy_defs->phy_name) < 0)
			DP_NOTICE(p_hwfn,
				  "Unexpected debug error: invalid PHY memory name\n");

		offset += qed_grc_dump_mem_hdr(p_hwfn,
					       dump_buf + offset,
					       dump,
					       mem_name,
					       0,
					       PHY_DUMP_SIZE_DWORDS,
					       16, true, mem_name, 0);

		if (!dump) {
			offset += PHY_DUMP_SIZE_DWORDS;
			continue;
		}

		bytes_buf = (u8 *) (dump_buf + offset);
		for (tbus_hi_offset = 0;
		     tbus_hi_offset < (NUM_PHY_TBUS_ADDRESSES >> 8);
		     tbus_hi_offset++) {
			qed_wr(p_hwfn, p_ptt, addr_hi_addr, tbus_hi_offset);
			for (tbus_lo_offset = 0; tbus_lo_offset < 256;
			     tbus_lo_offset++) {
				qed_wr(p_hwfn,
				       p_ptt, addr_lo_addr, tbus_lo_offset);
				*(bytes_buf++) = (u8) qed_rd(p_hwfn,
							     p_ptt,
							     data_lo_addr);
				*(bytes_buf++) = (u8) qed_rd(p_hwfn,
							     p_ptt,
							     data_hi_addr);
			}
		}

		offset += PHY_DUMP_SIZE_DWORDS;
	}

	return offset;
}

/**
 * Dumps the MCP HW dump from NVRAM. Returns the dumped size in dwords.
 */
static u32 qed_grc_dump_mcp_hw_dump(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt,
				    u32 * dump_buf, bool dump)
{
	u32 hw_dump_offset_bytes = 0, hw_dump_size_bytes = 0;
	u32 hw_dump_size_dwords = 0, offset = 0;
	enum dbg_status status;

	/**
	 * Read HW dump image from NVRAM
	 */
	status = qed_find_nvram_image(p_hwfn,
				      p_ptt,
				      NVM_TYPE_HW_DUMP_OUT,
				      &hw_dump_offset_bytes,
				      &hw_dump_size_bytes);
	if (status != DBG_STATUS_OK)
		return 0;

	hw_dump_size_dwords = BYTES_TO_DWORDS(hw_dump_size_bytes);

	/**
	 * Dump HW dump image section
	 */
	offset += qed_dump_section_hdr(dump_buf + offset,
				       dump, "mcp_hw_dump", 1);
	offset += qed_dump_num_param(dump_buf + offset,
				     dump, "size", hw_dump_size_dwords);

	/**
	 * Read MCP HW dump image into dump buffer
	 */
	if (dump && hw_dump_size_dwords) {
		status = qed_nvram_read(p_hwfn,
					p_ptt,
					hw_dump_offset_bytes,
					hw_dump_size_bytes, dump_buf + offset);
		if (status != DBG_STATUS_OK) {
			DP_NOTICE(p_hwfn,
				  "Failed to read MCP HW Dump image from NVRAM\n");
			return 0;
		}
	}
	offset += hw_dump_size_dwords;

	return offset;
}

/**
 * Dumps Static Debug data. Returns the dumped size in dwords.
 */
static u32 qed_grc_dump_static_debug(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     u32 * dump_buf, bool dump)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u32 block_id, line_id, offset = 0;

	/**
	 * don't dump static debug if a debug bus recording is in progress
	 */
	if (dump && qed_rd(p_hwfn, p_ptt, DBG_REG_DBG_BLOCK_ON))
		return 0;

	if (dump) {
		/**
		 * Disable debug bus in all blocks
		 */
		qed_bus_disable_blocks(p_hwfn, p_ptt);

		qed_bus_reset_dbg_block(p_hwfn, p_ptt);
		qed_wr(p_hwfn,
		       p_ptt, DBG_REG_FRAMING_MODE, DBG_BUS_FRAME_MODE_8HW);
		qed_wr(p_hwfn,
		       p_ptt, DBG_REG_DEBUG_TARGET, DBG_BUS_TARGET_ID_INT_BUF);
		qed_wr(p_hwfn, p_ptt, DBG_REG_FULL_MODE, 1);
		qed_bus_enable_dbg_block(p_hwfn, p_ptt, true);
	}

	/**
	 * Dump all static debug lines for each relevant block
	 */
	for (block_id = 0; block_id < MAX_BLOCK_ID; block_id++) {
		const struct dbg_block_chip *block_per_chip;
		const struct dbg_block *block;
		bool is_removed, has_dbg_bus;
		u16 modes_buf_offset;
		u32 block_dwords;

		block_per_chip =
		    qed_get_dbg_block_per_chip(p_hwfn, (enum block_id)block_id);
		is_removed = GET_FIELD(block_per_chip->flags,
				       DBG_BLOCK_CHIP_IS_REMOVED);
		has_dbg_bus = GET_FIELD(block_per_chip->flags,
					DBG_BLOCK_CHIP_HAS_DBG_BUS);

		if (!is_removed && has_dbg_bus &&
		    GET_FIELD(block_per_chip->dbg_bus_mode.data,
			      DBG_MODE_HDR_EVAL_MODE) > 0) {
			modes_buf_offset =
			    GET_FIELD(block_per_chip->dbg_bus_mode.data,
				      DBG_MODE_HDR_MODES_BUF_OFFSET);
			if (!qed_is_mode_match(p_hwfn, &modes_buf_offset))
				has_dbg_bus = false;
		}

		if (is_removed || !has_dbg_bus)
			continue;

		block_dwords = NUM_DBG_LINES(block_per_chip) *
		    STATIC_DEBUG_LINE_DWORDS;

		/**
		 * Dump static section params
		 */
		block = get_dbg_block(p_hwfn, (enum block_id)block_id);
		offset +=
		    qed_grc_dump_mem_hdr(p_hwfn,
					 dump_buf + offset,
					 dump,
					 (const char *)block->name,
					 0,
					 block_dwords, 32, false, "STATIC", 0);

		if (!dump) {
			offset += block_dwords;
			continue;
		}

		/**
		 * If all lines are invalid - dump zeros
		 */
		if (dev_data->block_in_reset[block_id]) {
			memset(dump_buf + offset, 0,
			       DWORDS_TO_BYTES(block_dwords));
			offset += block_dwords;
			continue;
		}

		/**
		 * Enable block's client
		 */
		qed_bus_enable_clients(p_hwfn,
				       p_ptt,
				       1 << block_per_chip->dbg_client_id);
		for (line_id = 0; line_id < (u32) NUM_DBG_LINES(block_per_chip);
		     line_id++) {
			/**
			 * Configure debug line ID
			 */
			qed_bus_config_dbg_line(p_hwfn,
						p_ptt,
						(enum block_id)block_id,
						(u8) line_id, 0xf, 0, 0, 0);

			/**
			 * Read debug line info
			 */
			offset += qed_grc_dump_addr_range(p_hwfn,
							  p_ptt,
							  dump_buf + offset,
							  dump,
							  BYTES_TO_DWORDS
							  (DBG_REG_CALENDAR_OUT_DATA),
							  STATIC_DEBUG_LINE_DWORDS,
							  true, SPLIT_TYPE_NONE,
							  0);
		}

		/**
		 * Disable block's client and debug output
		 */
		qed_bus_enable_clients(p_hwfn, p_ptt, 0);
		qed_bus_config_dbg_line(p_hwfn,
					p_ptt,
					(enum block_id)block_id, 0, 0, 0, 0, 0);
	}

	if (dump) {
		qed_bus_enable_dbg_block(p_hwfn, p_ptt, false);
		qed_bus_enable_clients(p_hwfn, p_ptt, 0);
	}

	return offset;
}

/* Performs GRC Dump to the specified buffer.
 * Returns the dumped size in dwords.
 */
static enum dbg_status qed_grc_dump(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt,
				    u32 * dump_buf,
				    bool dump, u32 * num_dumped_dwords)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u32 dwords_read, offset = 0, num_ltids;
	bool is_asic, parities_masked = false;
	u8 i;

	is_asic = dev_data->hw_type == HW_TYPE_ASIC;
	num_ltids = NUM_OF_LTIDS;
	*num_dumped_dwords = 0;
	dev_data->num_regs_read = 0;

	/**
	 * Update reset state
	 */
	if (dump)
		qed_update_blocks_reset_state(p_hwfn, p_ptt);

	/**
	 * Dump global params
	 */
	offset += qed_dump_common_global_params(p_hwfn, p_ptt, dump_buf + offset, dump, 4);	/* 4 must match below amount of params */
	offset += /* 1 */ qed_dump_str_param(dump_buf + offset,
					     dump, "dump-type", "grc-dump");
	offset += /* 2 */ qed_dump_num_param(dump_buf + offset,
					     dump, "num-lcids", NUM_OF_LCIDS);
	offset += /* 3 */ qed_dump_num_param(dump_buf + offset,
					     dump, "num-ltids", num_ltids);
	offset += /* 4 */ qed_dump_num_param(dump_buf + offset,
					     dump,
					     "num-ports", dev_data->num_ports);
	/* Additional/Less parameters require matching of number in call to dump_common_global_params() */

	/**
	 * Dump reset registers (dumped before taking blocks out of reset )
	 */
	if (qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_REGS))
		offset += qed_grc_dump_reset_regs(p_hwfn,
						  p_ptt,
						  dump_buf + offset, dump);

	/**
	 * Take all blocks out of reset (using reset registers)
	 */
	if (dump) {
		qed_grc_unreset_blocks(p_hwfn, p_ptt, false);
		qed_update_blocks_reset_state(p_hwfn, p_ptt);
	}

	/**
	 * Disable all parities
	 */
	if (dump) {
		if (!is_asic) {
			/**
			 * Disable parities using GRC (no MCP)
			 */
			qed_mask_parities_no_mcp(p_hwfn, p_ptt, true);
			parities_masked = true;
		} else if (!qed_grc_get_param(p_hwfn, DBG_GRC_PARAM_NO_MCP)) {
			/**
			 * Disable parities using MFW command
			 */
			parities_masked = !qed_mcp_mask_parities(p_hwfn,
								 p_ptt, 1);
			if (!parities_masked) {
				DP_NOTICE(p_hwfn,
					  "Failed to mask parities using MFW\n");
				if (qed_grc_get_param(p_hwfn,
						      DBG_GRC_PARAM_PARITY_SAFE))
					return
					    DBG_STATUS_MCP_COULD_NOT_MASK_PRTY;
				DP_NOTICE(p_hwfn, "mask parities using GRC\n");
				qed_mask_parities_no_mcp(p_hwfn, p_ptt, true);
				parities_masked = true;
			}
		}
	}

	/**
	 * Dump modified registers (dumped before modifying them)
	 */
	if (qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_REGS))
		offset += qed_grc_dump_modified_regs(p_hwfn,
						     p_ptt,
						     dump_buf + offset, dump);

	/**
	 * Stall storms
	 */
	if (dump &&
	    (qed_grc_is_included(p_hwfn,
				 DBG_GRC_PARAM_DUMP_IOR) ||
	     qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_VFC)))
		qed_grc_stall_storms(p_hwfn, p_ptt, true);

	/**
	 * Dump all regs
	 */
	if (qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_REGS)) {
		bool block_enable[MAX_BLOCK_ID];

		/**
		 * Dump all blocks except MCP
		 */
		for (i = 0; i < MAX_BLOCK_ID; i++)
			block_enable[i] = true;
		block_enable[BLOCK_MCP] = false;
		offset += qed_grc_dump_registers(p_hwfn,
						 p_ptt,
						 dump_buf +
						 offset,
						 dump, block_enable, NULL);

		/**
		 * Dump special registers
		 */
		offset += qed_grc_dump_special_regs(p_hwfn,
						    p_ptt,
						    dump_buf + offset, dump);
	}

	/**
	 * Dump memories
	 */
	offset += qed_grc_dump_memories(p_hwfn, p_ptt, dump_buf + offset, dump);

	/**
	 * Dump MCP
	 */
	if (qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_MCP))
		offset += qed_grc_dump_mcp(p_hwfn,
					   p_ptt, dump_buf + offset, dump);

	/**
	 * Dump context
	 */
	if (qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_CM_CTX))
		offset += qed_grc_dump_ctx(p_hwfn,
					   p_ptt, dump_buf + offset, dump);

	/**
	 * Dump RSS memories
	 */
	if (qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_RSS))
		offset += qed_grc_dump_rss(p_hwfn,
					   p_ptt, dump_buf + offset, dump);

	/**
	 * Dump Big RAM
	 */
	for (i = 0; i < NUM_BIG_RAM_TYPES; i++)
		if (qed_grc_is_included(p_hwfn, s_big_ram_defs[i].grc_param))
			offset += qed_grc_dump_big_ram(p_hwfn,
						       p_ptt,
						       dump_buf + offset,
						       dump, i);

	/**
	 * Dump VFC
	 */
	if (qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_VFC)) {
		dwords_read = qed_grc_dump_vfc(p_hwfn,
					       p_ptt, dump_buf + offset, dump);
		offset += dwords_read;
		if (!dwords_read)
			return DBG_STATUS_VFC_READ_ERROR;
	}

	/**
	 * Dump PHY tbus
	 */
	if (qed_grc_is_included(p_hwfn,
				DBG_GRC_PARAM_DUMP_PHY) && dev_data->chip_id ==
	    CHIP_K2 && dev_data->hw_type == HW_TYPE_ASIC)
		offset += qed_grc_dump_phy(p_hwfn,
					   p_ptt, dump_buf + offset, dump);

	/**
	 * Dump MCP HW Dump
	 */
	if (qed_grc_is_included(p_hwfn, DBG_GRC_PARAM_DUMP_MCP_HW_DUMP) &&
	    dev_data->hw_type == HW_TYPE_ASIC &&
	    !qed_grc_get_param(p_hwfn, DBG_GRC_PARAM_NO_MCP) && 1)
		offset += qed_grc_dump_mcp_hw_dump(p_hwfn,
						   p_ptt,
						   dump_buf + offset, dump);

	/**
	 * Dump static debug data (only if not during debug bus recording)
	 */
	if (qed_grc_is_included(p_hwfn,
				DBG_GRC_PARAM_DUMP_STATIC) &&
	    (!dump || dev_data->bus.state == DBG_BUS_STATE_IDLE))
		offset += qed_grc_dump_static_debug(p_hwfn,
						    p_ptt,
						    dump_buf + offset, dump);

	/**
	 * Dump last section
	 */
	offset += qed_dump_last_section(dump_buf, offset, dump);

	if (dump) {
		/**
		 * Unstall storms
		 */
		if (qed_grc_get_param(p_hwfn, DBG_GRC_PARAM_UNSTALL))
			qed_grc_stall_storms(p_hwfn, p_ptt, false);

		/**
		 * Clear parity status
		 */
		qed_grc_clear_all_prty(p_hwfn, p_ptt);

		/**
		 * Enable parities
		 */
		if (parities_masked) {
			if (!is_asic)
				qed_mask_parities_no_mcp(p_hwfn, p_ptt, false);
			else
				qed_mcp_mask_parities(p_hwfn, p_ptt, 0);
		}
	}

	*num_dumped_dwords = offset;

	return DBG_STATUS_OK;
}

/**
 * Writes the specified failing Idle Check rule to the specified buffer.
 * Returns the dumped size in dwords.
 */
static u32 qed_idle_chk_dump_failure(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     u32 *
				     dump_buf,
				     bool dump,
				     u16 rule_id,
				     const struct dbg_idle_chk_rule *rule,
				     u16 fail_entry_id, u32 * cond_reg_values)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	const struct dbg_idle_chk_cond_reg *cond_regs;
	const struct dbg_idle_chk_info_reg *info_regs;
	u32 i, next_reg_offset = 0, offset = 0;
	struct dbg_idle_chk_result_hdr *hdr;
	const union dbg_idle_chk_reg *regs;
	u8 reg_id;

	hdr = (struct dbg_idle_chk_result_hdr *)dump_buf;
	regs =
	    (const union dbg_idle_chk_reg *)p_hwfn->
	    dbg_arrays[BIN_BUF_DBG_IDLE_CHK_REGS].ptr + rule->reg_offset;
	cond_regs = &regs[0].cond_reg;
	info_regs = &regs[rule->num_cond_regs].info_reg;

	/**
	 * Dump rule data
	 */
	if (dump) {
		memset(hdr, 0, sizeof(*hdr));
		hdr->rule_id = rule_id;
		hdr->mem_entry_id = fail_entry_id;
		hdr->severity = rule->severity;
		hdr->num_dumped_cond_regs = rule->num_cond_regs;
	}

	offset += IDLE_CHK_RESULT_HDR_DWORDS;

	/**
	 * Dump condition register values
	 */
	for (reg_id = 0; reg_id < rule->num_cond_regs; reg_id++) {
		const struct dbg_idle_chk_cond_reg *reg = &cond_regs[reg_id];
		struct dbg_idle_chk_result_reg_hdr *reg_hdr;

		reg_hdr =
		    (struct dbg_idle_chk_result_reg_hdr *)(dump_buf + offset);

		/**
		 * Write register header
		 */
		if (!dump) {
			offset += IDLE_CHK_RESULT_REG_HDR_DWORDS +
			    reg->entry_size;
			continue;
		}

		offset += IDLE_CHK_RESULT_REG_HDR_DWORDS;
		memset(reg_hdr, 0, sizeof(*reg_hdr));
		reg_hdr->start_entry = reg->start_entry;
		reg_hdr->size = reg->entry_size;
		SET_FIELD(reg_hdr->data,
			  DBG_IDLE_CHK_RESULT_REG_HDR_IS_MEM,
			  reg->num_entries > 1 || reg->start_entry > 0 ? 1 : 0);
		SET_FIELD(reg_hdr->data,
			  DBG_IDLE_CHK_RESULT_REG_HDR_REG_ID, reg_id);

		/**
		 * Write register values
		 */
		for (i = 0; i < reg_hdr->size; i++, next_reg_offset++, offset++)
			dump_buf[offset] = cond_reg_values[next_reg_offset];
	}

	/**
	 * Dump info register values
	 */
	for (reg_id = 0; reg_id < rule->num_info_regs; reg_id++) {
		const struct dbg_idle_chk_info_reg *reg = &info_regs[reg_id];
		u32 block_id;

		/**
		 * Check if register's block is in reset
		 */
		if (!dump) {
			offset += IDLE_CHK_RESULT_REG_HDR_DWORDS + reg->size;
			continue;
		}

		block_id = GET_FIELD(reg->data, DBG_IDLE_CHK_INFO_REG_BLOCK_ID);
		if (block_id >= MAX_BLOCK_ID) {
			DP_NOTICE(p_hwfn, "Invalid block_id\n");
			return 0;
		}

		if (!dev_data->block_in_reset[block_id]) {
			struct dbg_idle_chk_result_reg_hdr *reg_hdr;
			bool wide_bus, eval_mode, mode_match = true;
			u16 modes_buf_offset;
			u32 addr;

			reg_hdr =
			    (struct dbg_idle_chk_result_reg_hdr *)(dump_buf
								   + offset);

			/**
			 * Check mode
			 */
			eval_mode = GET_FIELD(reg->mode.data,
					      DBG_MODE_HDR_EVAL_MODE) > 0;
			if (eval_mode) {
				modes_buf_offset = GET_FIELD(reg->mode.data,
							     DBG_MODE_HDR_MODES_BUF_OFFSET);
				mode_match = qed_is_mode_match(p_hwfn,
							       &modes_buf_offset);
			}

			if (!mode_match)
				continue;

			addr = GET_FIELD(reg->data,
					 DBG_IDLE_CHK_INFO_REG_ADDRESS);
			wide_bus = GET_FIELD(reg->data,
					     DBG_IDLE_CHK_INFO_REG_WIDE_BUS);

			/**
			 * Write register header
			 */
			offset += IDLE_CHK_RESULT_REG_HDR_DWORDS;
			hdr->num_dumped_info_regs++;
			memset(reg_hdr, 0, sizeof(*reg_hdr));
			reg_hdr->size = reg->size;
			SET_FIELD(reg_hdr->data,
				  DBG_IDLE_CHK_RESULT_REG_HDR_REG_ID,
				  rule->num_cond_regs + reg_id);

			/**
			 * Write register values
			 */
			offset += qed_grc_dump_addr_range(p_hwfn,
							  p_ptt,
							  dump_buf + offset,
							  dump,
							  addr,
							  reg->size,
							  wide_bus,
							  SPLIT_TYPE_NONE, 0);
		}
	}

	return offset;
}

/**
 * Dumps idle check rule entries. Returns the dumped size in dwords. */
static u32 qed_idle_chk_dump_rule_entries(struct qed_hwfn *p_hwfn, struct qed_ptt
					  *p_ptt, u32 * dump_buf, bool dump, const struct dbg_idle_chk_rule
					  *input_rules,
					  u32
					  num_input_rules,
					  u32 * num_failing_rules)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u32 cond_reg_values[IDLE_CHK_MAX_ENTRIES_SIZE];
	u32 i, offset = 0;
	u16 entry_id;
	u8 reg_id;

	*num_failing_rules = 0;

	for (i = 0; i < num_input_rules; i++) {
		const struct dbg_idle_chk_cond_reg *cond_regs;
		const struct dbg_idle_chk_rule *rule;
		const union dbg_idle_chk_reg *regs;
		u16 num_reg_entries = 1;
		bool check_rule = true;
		const u32 *imm_values;

		rule = &input_rules[i];
		regs =
		    (const union dbg_idle_chk_reg *)p_hwfn->
		    dbg_arrays[BIN_BUF_DBG_IDLE_CHK_REGS].ptr +
		    rule->reg_offset;
		cond_regs = &regs[0].cond_reg;
		imm_values =
		    (u32 *) p_hwfn->dbg_arrays[BIN_BUF_DBG_IDLE_CHK_IMMS].ptr +
		    rule->imm_offset;

		/**
		 * Check if all condition register blocks are out of reset, and
		 * find maximal number of entries (all condition registers that
		 * are memories must have the same size, which is > 1).
		 */
		for (reg_id = 0; reg_id < rule->num_cond_regs && check_rule;
		     reg_id++) {
			u32 block_id = GET_FIELD(cond_regs[reg_id].data,
						 DBG_IDLE_CHK_COND_REG_BLOCK_ID);

			if (block_id >= MAX_BLOCK_ID) {
				DP_NOTICE(p_hwfn, "Invalid block_id\n");
				return 0;
			}

			check_rule = !dev_data->block_in_reset[block_id];
			if (cond_regs[reg_id].num_entries > num_reg_entries)
				num_reg_entries = cond_regs[reg_id].num_entries;
		}

		if (!check_rule && dump)
			continue;

		if (!dump) {
			u32 entry_dump_size = qed_idle_chk_dump_failure(p_hwfn,
									p_ptt,
									dump_buf
									+
									offset,
									false,
									rule->rule_id,
									rule,
									0,
									NULL);

			offset += num_reg_entries * entry_dump_size;
			(*num_failing_rules) += num_reg_entries;
			continue;
		}

		/**
		 * Go over all register entries (number of entries is the same for all
		 * condition registers).
		 */
		for (entry_id = 0; entry_id < num_reg_entries; entry_id++) {
			u32 next_reg_offset = 0;

			/**
			 * Read current entry of all condition registers
			 */
			for (reg_id = 0; reg_id < rule->num_cond_regs; reg_id++) {
				const struct dbg_idle_chk_cond_reg *reg =
				    &cond_regs[reg_id];
				u32 padded_entry_size, addr;
				bool wide_bus;

				/**
				 * Find GRC address (if it's a memory, the address of the
				 * specific entry is calculated).
				 */
				addr = GET_FIELD(reg->data,
						 DBG_IDLE_CHK_COND_REG_ADDRESS);
				wide_bus = GET_FIELD(reg->data,
						     DBG_IDLE_CHK_COND_REG_WIDE_BUS);
				if (reg->num_entries > 1 || reg->start_entry >
				    0) {
					padded_entry_size =
					    reg->entry_size >
					    1 ? roundup_pow_of_two(reg->
								   entry_size) :
					    1;
					addr +=
					    (reg->start_entry +
					     entry_id) * padded_entry_size;
				}

				/**
				 * Read registers
				 */
				if (next_reg_offset + reg->entry_size >=
				    IDLE_CHK_MAX_ENTRIES_SIZE) {
					DP_NOTICE(p_hwfn,
						  "idle check registers entry is too large\n");
					return 0;
				}

				next_reg_offset +=
				    qed_grc_dump_addr_range(p_hwfn, p_ptt,
							    cond_reg_values +
							    next_reg_offset,
							    dump, addr,
							    reg->entry_size,
							    wide_bus,
							    SPLIT_TYPE_NONE, 0);
			}

			/**
			 * Call rule condition function. if returns true, it's a failure
			 */
			if ((*cond_arr[rule->cond_id]) (cond_reg_values,
							imm_values)) {
				offset += qed_idle_chk_dump_failure(p_hwfn,
								    p_ptt,
								    dump_buf +
								    offset,
								    dump,
								    rule->rule_id,
								    rule,
								    entry_id,
								    cond_reg_values);
				(*num_failing_rules)++;
			}
		}
	}

	return offset;
}

/**
 * Performs Idle Check Dump to the specified buffer.
 * Returns the dumped size in dwords.
 */
static u32 qed_idle_chk_dump(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, u32 * dump_buf, bool dump)
{
	struct virt_mem_desc *dbg_buf =
	    &p_hwfn->dbg_arrays[BIN_BUF_DBG_IDLE_CHK_RULES];
	u32 num_failing_rules_offset, offset = 0,
	    input_offset = 0, num_failing_rules = 0;

	/**
	 * Dump global params
	 */
	offset += qed_dump_common_global_params(p_hwfn, p_ptt, dump_buf + offset, dump, 1);	/* 1 must match below amount of params */
	offset += /* 1 */ qed_dump_str_param(dump_buf + offset,
					     dump, "dump-type", "idle-chk");
	/* Additional/Less parameters require matching of number in call to dump_common_global_params() */

	/**
	 * Dump idle check section header with a single parameter
	 */
	offset += qed_dump_section_hdr(dump_buf + offset, dump, "idle_chk", 1);
	num_failing_rules_offset = offset;
	offset += qed_dump_num_param(dump_buf + offset, dump, "num_rules", 0);

	while (input_offset < BYTES_TO_DWORDS(dbg_buf->size)) {
		const struct dbg_idle_chk_cond_hdr *cond_hdr =
		    (const struct dbg_idle_chk_cond_hdr *)dbg_buf->ptr +
		    input_offset++;
		bool eval_mode, mode_match = true;
		u32 curr_failing_rules;
		u16 modes_buf_offset;

		/**
		 * Check mode
		 */
		eval_mode = GET_FIELD(cond_hdr->mode.data,
				      DBG_MODE_HDR_EVAL_MODE) > 0;
		if (eval_mode) {
			modes_buf_offset = GET_FIELD(cond_hdr->mode.data,
						     DBG_MODE_HDR_MODES_BUF_OFFSET);
			mode_match = qed_is_mode_match(p_hwfn,
						       &modes_buf_offset);
		}

		if (mode_match) {
			const struct dbg_idle_chk_rule *rule =
			    (const struct dbg_idle_chk_rule *)((u32 *)
							       dbg_buf->ptr
							       + input_offset);

			offset +=
			    qed_idle_chk_dump_rule_entries(p_hwfn,
							   p_ptt,
							   dump_buf +
							   offset,
							   dump,
							   rule,
							   cond_hdr->data_size /
							   IDLE_CHK_RULE_SIZE_DWORDS,
							   &curr_failing_rules);
			num_failing_rules += curr_failing_rules;
		}

		input_offset += cond_hdr->data_size;
	}

	/**
	 * Overwrite num_rules parameter
	 */
	if (dump)
		qed_dump_num_param(dump_buf + num_failing_rules_offset,
				   dump, "num_rules", num_failing_rules);

	/**
	 * Dump last section
	 */
	offset += qed_dump_last_section(dump_buf, offset, dump);

	return offset;
}

/**
 * Get info on the MCP Trace data in the scratchpad:
 * - trace_data_grc_addr (OUT): trace data GRC address in bytes
 * - trace_data_size (OUT): trace data size in bytes (without the header)
 */
static enum dbg_status qed_mcp_trace_get_data_info(struct qed_hwfn *p_hwfn,
						   struct qed_ptt *p_ptt,
						   u32 *
						   trace_data_grc_addr,
						   u32 * trace_data_size)
{
	u32 spad_trace_offsize, signature;

	/**
	 * Read trace section offsize structure from MCP scratchpad
	 */
	spad_trace_offsize = qed_rd(p_hwfn, p_ptt, MCP_SPAD_TRACE_OFFSIZE_ADDR);

	/**
	 * Extract trace section address from offsize (in scratchpad)
	 */
	*trace_data_grc_addr =
	    MCP_REG_SCRATCH + SECTION_OFFSET(spad_trace_offsize);

	/**
	 * Read signature from MCP trace section
	 */
	signature =
	    qed_rd(p_hwfn, p_ptt, *trace_data_grc_addr +
		   offsetof(struct mcp_trace, signature));

	if (signature != MFW_TRACE_SIGNATURE)
		return DBG_STATUS_INVALID_TRACE_SIGNATURE;

	/**
	 * Read trace size from MCP trace section
	 */
	*trace_data_size = qed_rd(p_hwfn,
				  p_ptt,
				  *trace_data_grc_addr +
				  offsetof(struct mcp_trace, size));

	return DBG_STATUS_OK;
}

/**
 * Reads MCP trace meta data image from NVRAM
 * - running_bundle_id (OUT): running bundle ID (invalid when loaded from file)
 * - trace_meta_offset (OUT): trace meta offset in NVRAM in bytes (invalid when
 *			      loaded from file).
 * - trace_meta_size (OUT):   size in bytes of the trace meta data.
 */
static enum dbg_status qed_mcp_trace_get_meta_info(struct qed_hwfn *p_hwfn,
						   struct qed_ptt *p_ptt,
						   u32
						   trace_data_size_bytes,
						   u32 *
						   running_bundle_id,
						   u32 *
						   trace_meta_offset,
						   u32 * trace_meta_size)
{
	u32 spad_trace_offsize, nvram_image_type, running_mfw_addr;

	/**
	 * Read MCP trace section offsize structure from MCP scratchpad
	 */
	spad_trace_offsize = qed_rd(p_hwfn, p_ptt, MCP_SPAD_TRACE_OFFSIZE_ADDR);

	/**
	 * Find running bundle ID
	 */
	running_mfw_addr =
	    MCP_REG_SCRATCH + SECTION_OFFSET(spad_trace_offsize) +
	    QED_SECTION_SIZE(spad_trace_offsize) + trace_data_size_bytes;
	*running_bundle_id = qed_rd(p_hwfn, p_ptt, running_mfw_addr);
	if (*running_bundle_id > 1)
		return DBG_STATUS_INVALID_NVRAM_BUNDLE;

	/**
	 * Find image in NVRAM
	 */
	nvram_image_type =
	    (*running_bundle_id ==
	     DIR_ID_1) ? NVM_TYPE_MFW_TRACE1 : NVM_TYPE_MFW_TRACE2;
	return qed_find_nvram_image(p_hwfn,
				    p_ptt,
				    nvram_image_type,
				    trace_meta_offset, trace_meta_size);
}

/**
 * Reads the MCP Trace meta data from NVRAM into the specified buffer
 */
static enum dbg_status qed_mcp_trace_read_meta(struct qed_hwfn *p_hwfn,
					       struct qed_ptt *p_ptt,
					       u32
					       nvram_offset_in_bytes,
					       u32 size_in_bytes, u32 * buf)
{
	u8 modules_num, module_len, i, *byte_buf = (u8 *) buf;
	enum dbg_status status;
	u32 signature;

	/**
	 * Read meta data from NVRAM
	 */
	status = qed_nvram_read(p_hwfn,
				p_ptt,
				nvram_offset_in_bytes, size_in_bytes, buf);
	if (status != DBG_STATUS_OK)
		return status;

	/**
	 * Extract and check first signature
	 */
	signature = qed_read_unaligned_dword(byte_buf);
	byte_buf += sizeof(signature);
	if (signature != NVM_MAGIC_VALUE)
		return DBG_STATUS_INVALID_TRACE_SIGNATURE;

	/**
	 * Extract number of modules
	 */
	modules_num = *(byte_buf++);

	/**
	 * Skip all modules
	 */
	for (i = 0; i < modules_num; i++) {
		module_len = *(byte_buf++);
		byte_buf += module_len;
	}

	/**
	 * Extract and check second signature
	 */
	signature = qed_read_unaligned_dword(byte_buf);
	byte_buf += sizeof(signature);
	if (signature != NVM_MAGIC_VALUE)
		return DBG_STATUS_INVALID_TRACE_SIGNATURE;

	return DBG_STATUS_OK;
}

/**
 * Dump MCP Trace
 */
static enum dbg_status qed_mcp_trace_dump(struct qed_hwfn *p_hwfn,
					  struct qed_ptt *p_ptt,
					  u32 * dump_buf,
					  bool dump, u32 * num_dumped_dwords)
{
	u32 trace_meta_offset_bytes = 0, trace_meta_size_bytes = 0,
	    trace_meta_size_dwords = 0;
	u32 trace_data_grc_addr, trace_data_size_bytes, trace_data_size_dwords;
	u32 running_bundle_id, offset = 0;
	enum dbg_status status;
	int halted = 0;
	bool use_mfw;

	*num_dumped_dwords = 0;

	use_mfw = !qed_grc_get_param(p_hwfn, DBG_GRC_PARAM_NO_MCP);

	/**
	 * Get trace data info
	 */
	status = qed_mcp_trace_get_data_info(p_hwfn,
					     p_ptt,
					     &trace_data_grc_addr,
					     &trace_data_size_bytes);
	if (status != DBG_STATUS_OK)
		return status;

	/**
	 * Dump global params
	 */
	offset += qed_dump_common_global_params(p_hwfn, p_ptt, dump_buf + offset, dump, 1);	/* 1 must match below amount of params */
	offset += /* 1 */ qed_dump_str_param(dump_buf + offset,
					     dump, "dump-type", "mcp-trace");
	/* Additional/Less parameters require matching of number in call to dump_common_global_params() */

	/**
	 * Halt MCP while reading from scratchpad so the read data will be
	 * consistent. if halt fails, MCP trace is taken anyway, with a small
	 * risk that it may be corrupt.
	 */
	if (dump && use_mfw) {
		halted = !qed_mcp_halt(p_hwfn, p_ptt);
		if (!halted)
			DP_NOTICE(p_hwfn, "MCP halt failed!\n");
	}

	/**
	 * Find trace data size
	 */
	trace_data_size_dwords =
	    DIV_ROUND_UP(trace_data_size_bytes + sizeof(struct mcp_trace),
			 BYTES_IN_DWORD);

	/**
	 * Dump trace data section header and param
	 */
	offset += qed_dump_section_hdr(dump_buf + offset,
				       dump, "mcp_trace_data", 1);
	offset += qed_dump_num_param(dump_buf + offset,
				     dump, "size", trace_data_size_dwords);

	/**
	 * Read trace data from scratchpad into dump buffer
	 */
	offset += qed_grc_dump_addr_range(p_hwfn,
					  p_ptt,
					  dump_buf + offset,
					  dump,
					  BYTES_TO_DWORDS(trace_data_grc_addr),
					  trace_data_size_dwords,
					  false, SPLIT_TYPE_NONE, 0);

	/**
	 * Resume MCP (only if halt succeeded)
	 */
	if (halted && qed_mcp_resume(p_hwfn, p_ptt))
		DP_NOTICE(p_hwfn, "Failed to resume MCP after halt!\n");

	/**
	 * Dump trace meta section header
	 */
	offset += qed_dump_section_hdr(dump_buf + offset,
				       dump, "mcp_trace_meta", 1);

	/**
	 * If MCP Trace meta size parameter was set, use it.
	 * Otherwise, read trace meta (if NVRAM access is enabled).
	 * trace_meta_size_bytes is dword-aligned.
	 */
	trace_meta_size_bytes = qed_grc_get_param(p_hwfn,
						  DBG_GRC_PARAM_MCP_TRACE_META_SIZE);
	if ((!trace_meta_size_bytes || dump) && 1 && use_mfw)
		status = qed_mcp_trace_get_meta_info(p_hwfn,
						     p_ptt,
						     trace_data_size_bytes,
						     &running_bundle_id,
						     &trace_meta_offset_bytes,
						     &trace_meta_size_bytes);
	if (status == DBG_STATUS_OK)
		trace_meta_size_dwords = BYTES_TO_DWORDS(trace_meta_size_bytes);

	/**
	 * Dump trace meta size param
	 */
	offset += qed_dump_num_param(dump_buf + offset,
				     dump, "size", trace_meta_size_dwords);

	/**
	 * Read trace meta image into dump buffer
	 */
	if (dump && trace_meta_size_dwords)
		status = qed_mcp_trace_read_meta(p_hwfn,
						 p_ptt,
						 trace_meta_offset_bytes,
						 trace_meta_size_bytes,
						 dump_buf + offset);
	if (status == DBG_STATUS_OK)
		offset += trace_meta_size_dwords;

	/**
	 * Dump last section
	 */
	offset += qed_dump_last_section(dump_buf, offset, dump);

	*num_dumped_dwords = offset;

	/**
	 * If MFW can't be used, indicate that the dump doesn't contain the
	 * meta data from NVRAM.
	 */
	return use_mfw ? status : DBG_STATUS_NVRAM_GET_IMAGE_FAILED;
}

/**
 * Dump GRC FIFO
 */
static enum dbg_status qed_reg_fifo_dump(struct qed_hwfn *p_hwfn,
					 struct qed_ptt *p_ptt,
					 u32 * dump_buf,
					 bool dump, u32 * num_dumped_dwords)
{
	u32 dwords_read, size_param_offset, offset = 0;
	bool fifo_has_data;

	*num_dumped_dwords = 0;

	/**
	 * Dump global params
	 */
	offset += qed_dump_common_global_params(p_hwfn, p_ptt, dump_buf + offset, dump, 1);	/* 1 must match below amount of params */
	offset += /* 1 */ qed_dump_str_param(dump_buf + offset,
					     dump, "dump-type", "reg-fifo");
	/* Additional/Less parameters require matching of number in call to dump_common_global_params() */

	/**
	 * Dump fifo data section header and param. The size param is 0 for
	 * now, and is overwritten after reading the FIFO.
	 */
	offset += qed_dump_section_hdr(dump_buf + offset,
				       dump, "reg_fifo_data", 1);
	size_param_offset = offset;
	offset += qed_dump_num_param(dump_buf + offset, dump, "size", 0);

	if (dump) {
		fifo_has_data = qed_rd(p_hwfn,
				       p_ptt,
				       GRC_REG_TRACE_FIFO_VALID_DATA) == 1;

		/**
		 * Pull available data from fifo. Use DMAE since this is
		 * widebus memory and must be accessed atomically. Test for
		 * dwords_read not passing buffer size since more entries could
		 * be added to the buffer as we
		 * are emptying it.
		 */
		for (dwords_read = 0;
		     fifo_has_data && dwords_read < REG_FIFO_DEPTH_DWORDS;
		     dwords_read += REG_FIFO_ELEMENT_DWORDS) {
			offset += qed_grc_dump_addr_range(p_hwfn,
							  p_ptt,
							  dump_buf +
							  offset,
							  true,
							  BYTES_TO_DWORDS
							  (GRC_REG_TRACE_FIFO),
							  REG_FIFO_ELEMENT_DWORDS,
							  true, SPLIT_TYPE_NONE,
							  0);
			fifo_has_data =
			    qed_rd(p_hwfn, p_ptt, GRC_REG_TRACE_FIFO_VALID_DATA)
			    == 1;
		}

		qed_dump_num_param(dump_buf + size_param_offset,
				   dump, "size", dwords_read);
	} else {
		/* FIFO max size is REG_FIFO_DEPTH_DWORDS. There is no way to
		 * test how much data is available, except for reading it.
		 */
		offset += REG_FIFO_DEPTH_DWORDS;
	}

	/**
	 * Dump last section
	 */
	offset += qed_dump_last_section(dump_buf, offset, dump);

	*num_dumped_dwords = offset;

	return DBG_STATUS_OK;
}

/**
 * Dump IGU FIFO
 */
static enum dbg_status qed_igu_fifo_dump(struct qed_hwfn *p_hwfn,
					 struct qed_ptt *p_ptt,
					 u32 * dump_buf,
					 bool dump, u32 * num_dumped_dwords)
{
	u32 dwords_read, size_param_offset, offset = 0;
	bool fifo_has_data;

	*num_dumped_dwords = 0;

	/**
	 * Dump global params
	 */
	offset += qed_dump_common_global_params(p_hwfn, p_ptt, dump_buf + offset, dump, 1);	/* 1 must match below amount of params */
	offset += /* 1 */ qed_dump_str_param(dump_buf + offset,
					     dump, "dump-type", "igu-fifo");
	/* Additional/Less parameters require matching of number in call to dump_common_global_params() */

	/**
	 * Dump fifo data section header and param. The size param is 0 for
	 * now, and is overwritten after reading the FIFO.
	 */
	offset += qed_dump_section_hdr(dump_buf + offset,
				       dump, "igu_fifo_data", 1);
	size_param_offset = offset;
	offset += qed_dump_num_param(dump_buf + offset, dump, "size", 0);

	if (dump) {
		fifo_has_data = qed_rd(p_hwfn,
				       p_ptt,
				       IGU_REG_ERROR_HANDLING_DATA_VALID) == 1;

		/**
		 * Pull available data from fifo. Use DMAE since this is
		 * widebus memory and must be accessed atomically. Test for
		 * dwords_read not passing buffer size since more entries could
		 * be added to the buffer as we are emptying it.
		 */
		for (dwords_read = 0;
		     fifo_has_data && dwords_read < IGU_FIFO_DEPTH_DWORDS;
		     dwords_read += IGU_FIFO_ELEMENT_DWORDS) {
			offset += qed_grc_dump_addr_range(p_hwfn,
							  p_ptt,
							  dump_buf +
							  offset,
							  true,
							  BYTES_TO_DWORDS
							  (IGU_REG_ERROR_HANDLING_MEMORY),
							  IGU_FIFO_ELEMENT_DWORDS,
							  true, SPLIT_TYPE_NONE,
							  0);
			fifo_has_data =
			    qed_rd(p_hwfn, p_ptt,
				   IGU_REG_ERROR_HANDLING_DATA_VALID)
			    == 1;
		}

		qed_dump_num_param(dump_buf + size_param_offset,
				   dump, "size", dwords_read);
	} else {
		/* FIFO max size is IGU_FIFO_DEPTH_DWORDS. There is no way to
		 * test how much data is available, except for reading it.
		 */
		offset += IGU_FIFO_DEPTH_DWORDS;
	}

	/**
	 * Dump last section
	 */
	offset += qed_dump_last_section(dump_buf, offset, dump);

	*num_dumped_dwords = offset;

	return DBG_STATUS_OK;
}

/**
 * Protection Override dump
 */
static enum dbg_status qed_protection_override_dump(struct qed_hwfn *p_hwfn,
						    struct qed_ptt *p_ptt,
						    u32 *
						    dump_buf,
						    bool dump,
						    u32 * num_dumped_dwords)
{
	u32 size_param_offset, override_window_dwords, offset = 0;

	*num_dumped_dwords = 0;

	/**
	 * Dump global params
	 */
	offset += qed_dump_common_global_params(p_hwfn, p_ptt, dump_buf + offset, dump, 1);	/* 1 must match below amount of params */
	offset += /* 1 */ qed_dump_str_param(dump_buf + offset,
					     dump,
					     "dump-type",
					     "protection-override");
	/* Additional/Less parameters require matching of number in call to dump_common_global_params() */

	/**
	 * Dump data section header and param. The size param is 0 for now,
	 * and is overwritten after reading the data.
	 */
	offset += qed_dump_section_hdr(dump_buf + offset,
				       dump, "protection_override_data", 1);
	size_param_offset = offset;
	offset += qed_dump_num_param(dump_buf + offset, dump, "size", 0);

	if (dump) {
		/**
		 * Add override window info to buffer
		 */
		override_window_dwords = qed_rd(p_hwfn,
						p_ptt,
						GRC_REG_NUMBER_VALID_OVERRIDE_WINDOW)
		    * PROTECTION_OVERRIDE_ELEMENT_DWORDS;
		if (((override_window_dwords) &&
		     (override_window_dwords <
		      PROTECTION_OVERRIDE_DEPTH_DWORDS))) {
			offset += qed_grc_dump_addr_range(p_hwfn,
							  p_ptt,
							  dump_buf + offset,
							  true,
							  BYTES_TO_DWORDS
							  (GRC_REG_PROTECTION_OVERRIDE_WINDOW),
							  override_window_dwords,
							  true, SPLIT_TYPE_NONE,
							  0);
			qed_dump_num_param(dump_buf + size_param_offset, dump,
					   "size", override_window_dwords);
		} else if (override_window_dwords) {
			offset += PROTECTION_OVERRIDE_DEPTH_DWORDS;
		}
	} else {
		offset += PROTECTION_OVERRIDE_DEPTH_DWORDS;
	}

	/**
	 * Dump last section
	 */
	offset += qed_dump_last_section(dump_buf, offset, dump);

	*num_dumped_dwords = offset;

	return DBG_STATUS_OK;
}

/**
 * Performs FW Asserts Dump to the specified buffer.
 * Returns the dumped size in dwords.
 */
static u32 qed_fw_asserts_dump(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, u32 * dump_buf, bool dump)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct fw_asserts_ram_section *asserts;
	char storm_letter_str[2] = "?";
	struct fw_info fw_info;
	u32 offset = 0;
	u8 storm_id;

	/**
	 * Dump global params
	 */
	offset += qed_dump_common_global_params(p_hwfn, p_ptt, dump_buf + offset, dump, 1);	/* 1 must match below amount of params */
	offset += /* 1 */ qed_dump_str_param(dump_buf + offset,
					     dump, "dump-type", "fw-asserts");
	/* Additional/Less parameters require matching of number in call to dump_common_global_params() */

	/**
	 * Find Storm dump size
	 */
	for (storm_id = 0; storm_id < MAX_DBG_STORMS; storm_id++) {
		u32 fw_asserts_section_addr,
		    next_list_idx_addr, next_list_idx, last_list_idx, addr;
		struct storm_defs *storm = &s_storm_defs[storm_id];

		if (dev_data->block_in_reset[storm->sem_block_id])
			continue;

		/**
		 * Read FW info for the current Storm
		 */
		qed_read_storm_fw_info(p_hwfn, p_ptt, storm_id, &fw_info);

		asserts = &fw_info.fw_asserts_section;

		/**
		 * Dump FW Asserts section header and params
		 */
		storm_letter_str[0] = storm->letter;
		offset += qed_dump_section_hdr(dump_buf + offset,
					       dump, "fw_asserts", 2);
		offset += qed_dump_str_param(dump_buf + offset,
					     dump, "storm", storm_letter_str);
		offset += qed_dump_num_param(dump_buf + offset,
					     dump,
					     "size",
					     asserts->list_element_dword_size);

		/**
		 * Read and dump FW Asserts data
		 */
		if (!dump) {
			offset += asserts->list_element_dword_size;
			continue;
		}

		fw_asserts_section_addr = storm->sem_fast_mem_addr +
		    SEM_FAST_REG_INT_RAM +
		    RAM_LINES_TO_BYTES(asserts->section_ram_line_offset);
		next_list_idx_addr = fw_asserts_section_addr +
		    DWORDS_TO_BYTES(asserts->list_next_index_dword_offset);
		next_list_idx = qed_rd(p_hwfn, p_ptt, next_list_idx_addr);
		last_list_idx =
		    (next_list_idx >
		     0 ? next_list_idx : asserts->list_num_elements) - 1;
		addr = BYTES_TO_DWORDS(fw_asserts_section_addr) +
		    asserts->list_dword_offset +
		    last_list_idx * asserts->list_element_dword_size;
		offset += qed_grc_dump_addr_range(p_hwfn,
						  p_ptt,
						  dump_buf + offset,
						  dump,
						  addr,
						  asserts->list_element_dword_size,
						  false, SPLIT_TYPE_NONE, 0);
	}

	/**
	 * Dump last section
	 */
	offset += qed_dump_last_section(dump_buf, offset, dump);

	return offset;
}

/**
 * Dumps the specified ILT pages to the specified buffer.
 * Returns the dumped size in dwords.
 * OUT params: dump - can change in function from true to false if buf_size_in_dwords is smaller then dumped data
 *             given_offset - offset of dumpewd data is updated in function
 *             given_actual_dump_size_in_dwords - size of data that is actual dumped to buffer - is changed in function
 */
static u32 qed_ilt_dump_pages_range(u32 * dump_buf,
				    u32 * given_offset,
				    bool * dump,
				    u32 start_page_id,
				    u32 num_pages,
				    struct phys_mem_desc *ilt_pages,
				    bool dump_page_ids,
				    u32
				    buf_size_in_dwords,
				    u32 * given_actual_dump_size_in_dwords)
{
	u32 actual_dump_size_in_dwords = *given_actual_dump_size_in_dwords;
	u32 page_id, end_page_id, offset = *given_offset;
	struct phys_mem_desc *mem_desc = NULL;
	bool continue_dump = *dump;
	u32 partial_page_size = 0;

	if (num_pages == 0)
		return offset;

	end_page_id = start_page_id + num_pages - 1;
	for (page_id = start_page_id; page_id <= end_page_id; page_id++) {
		mem_desc = &ilt_pages[page_id];
		if (!ilt_pages[page_id].virt_addr)
			continue;

		if (dump_page_ids) {
			/*
			 * Copy page ID to dump buffer
			 * (if dump is needed and buffer is not full)
			 */
			if ((continue_dump) &&
			    (offset + 1 > buf_size_in_dwords)) {
				continue_dump = false;
				actual_dump_size_in_dwords = offset;
			}
			if (continue_dump)
				*(dump_buf + offset) = page_id;
			offset++;
		} else {
			/* Copy page memory to dump buffer */
			if ((continue_dump) &&
			    (offset + BYTES_TO_DWORDS(mem_desc->size) >
			     buf_size_in_dwords)) {
				if (offset + BYTES_TO_DWORDS(mem_desc->size) >
				    buf_size_in_dwords) {
					partial_page_size =
					    buf_size_in_dwords - offset;
					memcpy(dump_buf + offset,
					       mem_desc->virt_addr,
					       partial_page_size);
					continue_dump = false;
					actual_dump_size_in_dwords =
					    offset + partial_page_size;
				}
			}

			if (continue_dump)
				memcpy(dump_buf + offset,
				       mem_desc->virt_addr, mem_desc->size);
			offset += BYTES_TO_DWORDS(mem_desc->size);
		}
	}

	*dump = continue_dump;
	*given_offset = offset;
	*given_actual_dump_size_in_dwords = actual_dump_size_in_dwords;
	return offset;
}

/**
 * Dumps a section containing the dumped ILT pages.
 * Returns the dumped size in dwords.
 * OUT params: dump - can change in function from true to false if buf_size_in_dwords is smaller then dumped data
 *		      given_offset - offset of dumpewd data is updated in function
 *		      given_actual_dump_size_in_dwords - size of data that is actual dumped to buffer - is changed in function
 */
static u32 qed_ilt_dump_pages_section(struct qed_hwfn *p_hwfn,
				      u32 * dump_buf,
				      u32 * given_offset,
				      bool * dump,
				      u32
				      valid_conn_pf_pages,
				      u32
				      valid_conn_vf_pages,
				      struct phys_mem_desc *ilt_pages,
				      bool dump_page_ids,
				      u32
				      buf_size_in_dwords,
				      u32 * given_actual_dump_size_in_dwords)
{
#if ((!defined VMWARE) && (!defined UEFI))
	struct qed_ilt_client_cfg *clients = p_hwfn->p_cxt_mngr->clients;
#endif
	u32 pf_start_line, start_page_id, offset = *given_offset;
	u32 cdut_pf_init_pages, cdut_vf_init_pages;
	u32 cdut_pf_work_pages, cdut_vf_work_pages;
	u32 base_data_offset, size_param_offset;
	u32 src_pages;
	u32 section_header_and_param_size;
	u32 cdut_pf_pages, cdut_vf_pages;
	u32 actual_dump_size_in_dwords;
	bool continue_dump = *dump;
	bool update_size = *dump;
	const char *section_name;
	u32 i;

	actual_dump_size_in_dwords = *given_actual_dump_size_in_dwords;
	section_name = dump_page_ids ? "ilt_page_ids" : "ilt_page_mem";
	cdut_pf_init_pages = qed_get_cdut_num_pf_init_pages(p_hwfn);
	cdut_vf_init_pages = qed_get_cdut_num_vf_init_pages(p_hwfn);
	cdut_pf_work_pages = qed_get_cdut_num_pf_work_pages(p_hwfn);
	cdut_vf_work_pages = qed_get_cdut_num_vf_work_pages(p_hwfn);
	cdut_pf_pages = cdut_pf_init_pages + cdut_pf_work_pages;
	cdut_vf_pages = cdut_vf_init_pages + cdut_vf_work_pages;
	pf_start_line = p_hwfn->p_cxt_mngr->pf_start_line;
	section_header_and_param_size = qed_dump_section_hdr(NULL,
							     false,
							     section_name,
							     1) +
	    qed_dump_num_param(NULL, false, "size", 0);

	if ((continue_dump) &&
	    (offset + section_header_and_param_size > buf_size_in_dwords)) {
		continue_dump = false;
		update_size = false;
		actual_dump_size_in_dwords = offset;
	}

	offset += qed_dump_section_hdr(dump_buf + offset,
				       continue_dump, section_name, 1);

	/* Dump size parameter (0 for now, overwritten with real size later) */
	size_param_offset = offset;
	offset += qed_dump_num_param(dump_buf + offset,
				     continue_dump, "size", 0);
	base_data_offset = offset;

	/* CDUC pages are ordered as follows:
	 * - PF pages - valid section (included in PF connection type mapping)
	 * - PF pages - invalid section (not dumped)
	 * - For each VF in the PF:
	 * - VF pages - valid section (included in VF connection type mapping)
	 * - VF pages - invalid section (not dumped)
	 */
	if (qed_grc_get_param(p_hwfn, DBG_GRC_PARAM_DUMP_ILT_CDUC)) {
		/* Dump connection PF pages */
		start_page_id = clients[ILT_CLI_CDUC].first.val - pf_start_line;
		qed_ilt_dump_pages_range(dump_buf,
					 &offset,
					 &continue_dump,
					 start_page_id,
					 valid_conn_pf_pages,
					 ilt_pages,
					 dump_page_ids,
					 buf_size_in_dwords,
					 &actual_dump_size_in_dwords);

		/* Dump connection VF pages */
		start_page_id += clients[ILT_CLI_CDUC].pf_total_lines;
		for (i = 0; i < p_hwfn->p_cxt_mngr->vf_count;
		     i++, start_page_id += clients[ILT_CLI_CDUC].vf_total_lines)
			qed_ilt_dump_pages_range(dump_buf,
						 &offset,
						 &continue_dump,
						 start_page_id,
						 valid_conn_vf_pages,
						 ilt_pages,
						 dump_page_ids,
						 buf_size_in_dwords,
						 &actual_dump_size_in_dwords);
	}

	/* CDUT pages are ordered as follos:
	 * - PF init pages (not dumped)
	 * - PF work pages
	 * - For each VF in the PF:
	 *   - VF init pages (not dumped)
	 *   - VF work pages
	 */
	if (qed_grc_get_param(p_hwfn, DBG_GRC_PARAM_DUMP_ILT_CDUT)) {
		/* Dump task PF pages */
		start_page_id = clients[ILT_CLI_CDUT].first.val +
		    cdut_pf_init_pages - pf_start_line;
		qed_ilt_dump_pages_range(dump_buf,
					 &offset,
					 &continue_dump,
					 start_page_id,
					 cdut_pf_work_pages,
					 ilt_pages,
					 dump_page_ids,
					 buf_size_in_dwords,
					 &actual_dump_size_in_dwords);

		/* Dump task VF pages */
		start_page_id = clients[ILT_CLI_CDUT].first.val +
		    cdut_pf_pages + cdut_vf_init_pages - pf_start_line;
		for (i = 0; i < p_hwfn->p_cxt_mngr->vf_count;
		     i++, start_page_id += cdut_vf_pages)
			qed_ilt_dump_pages_range(dump_buf,
						 &offset,
						 &continue_dump,
						 start_page_id,
						 cdut_vf_work_pages,
						 ilt_pages,
						 dump_page_ids,
						 buf_size_in_dwords,
						 &actual_dump_size_in_dwords);
	}

	/*Dump Searcher pages */
	if (clients[ILT_CLI_SRC].active) {
		start_page_id = clients[ILT_CLI_SRC].first.val - pf_start_line;
		src_pages = clients[ILT_CLI_SRC].last.val -
		    clients[ILT_CLI_SRC].first.val + 1;
		qed_ilt_dump_pages_range(dump_buf, &offset, &continue_dump,
					 start_page_id, src_pages, ilt_pages,
					 dump_page_ids, buf_size_in_dwords,
					 &actual_dump_size_in_dwords);
	}

	/* Overwrite size param */
	if (update_size) {
		u32 section_size = (*dump == continue_dump) ?
		    offset - base_data_offset :
		    actual_dump_size_in_dwords - base_data_offset;
		if (section_size > 0)
			qed_dump_num_param(dump_buf + size_param_offset,
					   *dump, "size", section_size);
		else if ((section_size == 0) && (*dump != continue_dump))
			actual_dump_size_in_dwords -=
			    section_header_and_param_size;
	}

	*dump = continue_dump;
	*given_offset = offset;
	*given_actual_dump_size_in_dwords = actual_dump_size_in_dwords;

	return offset;
}

/**
 * Dumps a section containing the global parameters.
 * Part of ilt dump process
 * Returns the dumped size in dwords.
 * Out params: full_dump_size_param_offset - offset of full dump size parameter,
 *             This parameter is initiated here with 0 and should be updated at the end of ilt dump process
 *             actual_dump_size_param_offset - offset of full dump size parameter,
 *             This parameter is initiated here with 0 and should be updated at the end of ilt dump process
 */
static u32 qed_ilt_dump_dump_common_global_params(struct qed_hwfn *p_hwfn,
						  struct qed_ptt *p_ptt,
						  u32 *
						  dump_buf,
						  bool dump,
						  u32
						  cduc_page_size,
						  u32
						  conn_ctx_size,
						  u32
						  cdut_page_size,
						  u32 *
						  full_dump_size_param_offset,
						  u32 *
						  actual_dump_size_param_offset)
{
#if ((!defined VMWARE) && (!defined UEFI))
	struct qed_ilt_client_cfg *clients = p_hwfn->p_cxt_mngr->clients;
#endif
	u32 offset = 0;

	offset += qed_dump_common_global_params(p_hwfn, p_ptt, dump_buf + offset, dump, 30);	/* 30 must match below amount of params */
	offset += /*  1 */ qed_dump_str_param(
						     dump_buf + offset,
						     dump,
						     "dump-type", "ilt-dump");
	offset += /*  2 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "cduc-page-size",
						     cduc_page_size);
	offset += /*  3 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "cduc-first-page-id",
						     clients[ILT_CLI_CDUC].
						     first.val);
	offset += /*  4 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "cduc-last-page-id",
						     clients[ILT_CLI_CDUC].last.
						     val);
	offset += /*  5 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "cduc-num-pf-pages",
						     clients[ILT_CLI_CDUC].
						     pf_total_lines);
	offset += /*  6 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "cduc-num-vf-pages",
						     clients[ILT_CLI_CDUC].
						     vf_total_lines);
	offset += /*  7 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "max-conn-ctx-size",
						     conn_ctx_size);
	offset += /*  8 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "cdut-page-size",
						     cdut_page_size);
	offset += /*  9 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "cdut-first-page-id",
						     clients[ILT_CLI_CDUT].
						     first.val);
	offset += /* 10 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "cdut-last-page-id",
						     clients[ILT_CLI_CDUT].last.
						     val);
	offset += /* 11 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "cdut-num-pf-init-pages",
						     qed_get_cdut_num_pf_init_pages
						     (p_hwfn));
	offset += /* 12 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "cdut-num-vf-init-pages",
						     qed_get_cdut_num_vf_init_pages
						     (p_hwfn));
	offset += /* 13 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "cdut-num-pf-work-pages",
						     qed_get_cdut_num_pf_work_pages
						     (p_hwfn));
	offset += /* 14 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "cdut-num-vf-work-pages",
						     qed_get_cdut_num_vf_work_pages
						     (p_hwfn));
	offset += /* 15 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "max-task-ctx-size",
						     p_hwfn->p_cxt_mngr->
						     task_ctx_size);
	offset += /* 16 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "first-vf-id-in-pf",
						     p_hwfn->p_cxt_mngr->
						     first_vf_in_pf);
	offset += /* 17 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "num-vfs-in-pf",
						     p_hwfn->p_cxt_mngr->
						     vf_count);
	offset += /* 18 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "ptr-size-bytes",
						     sizeof(void *));
	offset += /* 19 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "pf-start-line",
						     p_hwfn->p_cxt_mngr->
						     pf_start_line);
	offset += /* 20 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "page-mem-desc-size-dwords",
						     PAGE_MEM_DESC_SIZE_DWORDS);
	offset += /* 21 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "ilt-shadow-size",
						     p_hwfn->p_cxt_mngr->
						     ilt_shadow_size);
	*full_dump_size_param_offset = offset;
	offset += /* 22 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump, "dump-size-full", 0);
	*actual_dump_size_param_offset = offset;
	offset += /* 23 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "dump-size-actual", 0);
	offset += /* 24 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "iscsi_task_pages",
						     p_hwfn->p_cxt_mngr->
						     iscsi_task_pages);
	offset += /* 25 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "fcoe_task_pages",
						     p_hwfn->p_cxt_mngr->
						     fcoe_task_pages);
	offset += /* 26 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "roce_task_pages",
						     p_hwfn->p_cxt_mngr->
						     roce_task_pages);
	offset += /* 27 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "eth_task_pages",
						     p_hwfn->p_cxt_mngr->
						     eth_task_pages);
	offset += /* 28 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "src-first-page-id",
						     clients[ILT_CLI_SRC].first.
						     val);
	offset += /* 29 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "src-last-page-id",
						     clients[ILT_CLI_SRC].last.
						     val);
	offset += /* 30 */ qed_dump_num_param(
						     dump_buf + offset,
						     dump,
						     "src-is-active",
						     clients[ILT_CLI_SRC].
						     active);
	/* Additional/Less parameters require matching of number in call to dump_common_global_params() */
	return offset;
}

/**
 * Dump section containing number of PF CIDs per connection type.
 * Part of ilt dump process.
 * Returns the dumped size in dwords.
 */
static u32 qed_ilt_dump_dump_num_pf_cids(struct qed_hwfn *p_hwfn,
					 u32 * dump_buf,
					 bool dump, u32 * valid_conn_pf_cids)
{
	u32 num_pf_cids = 0;
	u32 offset = 0;
	u8 conn_type;

	offset += qed_dump_section_hdr(dump_buf + offset,
				       dump, "num_pf_cids_per_conn_type", 1);
	offset += qed_dump_num_param(dump_buf + offset,
				     dump, "size", NUM_OF_CONNECTION_TYPES);
	for (conn_type = 0, *valid_conn_pf_cids = 0;
	     conn_type < NUM_OF_CONNECTION_TYPES; conn_type++, offset++) {
		num_pf_cids = p_hwfn->p_cxt_mngr->conn_cfg[conn_type].cid_count;
		if (dump)
			*(dump_buf + offset) = num_pf_cids;
		*valid_conn_pf_cids += num_pf_cids;
	}

	return offset;
}

/**
 * Dump section containing number of VF CIDs per connection type
 * Part of ilt dump process.
 * Returns the dumped size in dwords.
 */
static u32 qed_ilt_dump_dump_num_vf_cids(struct qed_hwfn *p_hwfn,
					 u32 * dump_buf,
					 bool dump, u32 * valid_conn_vf_cids)
{
	u32 num_vf_cids = 0;
	u32 offset = 0;
	u8 conn_type;

	offset += qed_dump_section_hdr(dump_buf + offset, dump,
				       "num_vf_cids_per_conn_type", 1);
	offset += qed_dump_num_param(dump_buf + offset,
				     dump, "size", NUM_OF_CONNECTION_TYPES);
	for (conn_type = 0, *valid_conn_vf_cids = 0;
	     conn_type < NUM_OF_CONNECTION_TYPES; conn_type++, offset++) {
		num_vf_cids =
		    p_hwfn->p_cxt_mngr->conn_cfg[conn_type].cids_per_vf;
		if (dump)
			*(dump_buf + offset) = num_vf_cids;
		*valid_conn_vf_cids += num_vf_cids;
	}

	return offset;
}

/**
 * Performs ILT Dump to the specified buffer.
 * buf_size_in_dwords - The dumped buffer size. function stops the dumping when data exceeding  this size
 * Returns the dumped size in dwords.
 */
static u32 qed_ilt_dump(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt,
			u32 * dump_buf, u32 buf_size_in_dwords, bool dump)
{
#if ((!defined VMWARE) && (!defined UEFI))
	struct qed_ilt_client_cfg *clients = p_hwfn->p_cxt_mngr->clients;
#endif
	u32 valid_conn_vf_cids = 0,
	    valid_conn_vf_pages, offset = 0, real_dumped_size = 0;
	u32 valid_conn_pf_cids = 0, valid_conn_pf_pages, num_pages;
	u32 actual_dump_size_param_offset = 0;
	u32 num_cids_per_page, conn_ctx_size;
	u32 full_dump_size_param_offset = 0;
	u32 cduc_page_size, cdut_page_size;
	u32 actual_dump_size_in_dwords = 0;
	struct phys_mem_desc *ilt_pages;
	u32 last_section_size;
	u32 section_size = 0;
	bool continue_dump;
	u32 page_id;

	last_section_size = qed_dump_last_section(NULL, 0, false);
	cduc_page_size = 1 <<
	    (clients[ILT_CLI_CDUC].p_size.val + PXP_ILT_PAGE_SIZE_NUM_BITS_MIN);
	cdut_page_size = 1 <<
	    (clients[ILT_CLI_CDUT].p_size.val + PXP_ILT_PAGE_SIZE_NUM_BITS_MIN);
	conn_ctx_size = p_hwfn->p_cxt_mngr->conn_ctx_size;
	num_cids_per_page = (int)(cduc_page_size / conn_ctx_size);
	ilt_pages = p_hwfn->p_cxt_mngr->ilt_shadow;
	continue_dump = dump;

	/* if need to dump then save memory for the last section
	 * (last section calculates CRC of dumped data)
	 */
	if (dump) {
		if (buf_size_in_dwords >= last_section_size) {
			buf_size_in_dwords -= last_section_size;
		} else {
			continue_dump = false;
			actual_dump_size_in_dwords = offset;
		}
	}

	/* Dump global params */

	/* if need to dump then first check that there is enough memory in dumped buffer for this section
	 * calculate the size of this section without dumping.
	 * if there is not enough memory - then stop the dumping
	 */
	if (continue_dump) {
		section_size = qed_ilt_dump_dump_common_global_params(p_hwfn,
								      p_ptt,
								      NULL,
								      false,
								      cduc_page_size,
								      conn_ctx_size,
								      cdut_page_size,
								      &full_dump_size_param_offset,
								      &actual_dump_size_param_offset);
		if (offset + section_size > buf_size_in_dwords) {
			continue_dump = false;
			actual_dump_size_in_dwords = offset;
		}
	}

	offset += qed_ilt_dump_dump_common_global_params(p_hwfn,
							 p_ptt,
							 dump_buf + offset,
							 continue_dump,
							 cduc_page_size,
							 conn_ctx_size,
							 cdut_page_size,
							 &full_dump_size_param_offset,
							 &actual_dump_size_param_offset);

	/* Dump section containing number of PF CIDs per connection type
	 * If need to dump then first check that there is enough memory in dumped buffer for this section
	 */
	if (continue_dump) {
		section_size = qed_ilt_dump_dump_num_pf_cids(p_hwfn,
							     NULL,
							     false,
							     &valid_conn_pf_cids);
		if (offset + section_size > buf_size_in_dwords) {
			continue_dump = false;
			actual_dump_size_in_dwords = offset;
		}
	}

	offset += qed_ilt_dump_dump_num_pf_cids(p_hwfn,
						dump_buf + offset,
						continue_dump,
						&valid_conn_pf_cids);

	/* Dump section containing number of VF CIDs per connection type
	 * If need to dump then first check that there is enough memory in dumped buffer for this section
	 */
	if (continue_dump) {
		section_size = qed_ilt_dump_dump_num_vf_cids(p_hwfn,
							     NULL,
							     false,
							     &valid_conn_vf_cids);
		if (offset + section_size > buf_size_in_dwords) {
			continue_dump = false;
			actual_dump_size_in_dwords = offset;
		}
	}

	offset += qed_ilt_dump_dump_num_vf_cids(p_hwfn,
						dump_buf + offset,
						continue_dump,
						&valid_conn_vf_cids);

	/* Dump section containing physical memory descriptors for each ILT page */
	num_pages = p_hwfn->p_cxt_mngr->ilt_shadow_size;	/* struct dbg_tools_data *dev_data = &p_hwfn->dbg_info; s_chip_defs[dev_data->chip_id].num_ilt_pages; */

	/* If need to dump then first check that there is enough memory in dumped buffer for the section header */
	if (continue_dump) {
		section_size = qed_dump_section_hdr(NULL,
						    false,
						    "ilt_page_desc",
						    1) +
		    qed_dump_num_param(NULL,
				       false,
				       "size",
				       num_pages * PAGE_MEM_DESC_SIZE_DWORDS);
		if (offset + section_size > buf_size_in_dwords) {
			continue_dump = false;
			actual_dump_size_in_dwords = offset;
		}
	}

	offset += qed_dump_section_hdr(dump_buf + offset,
				       continue_dump, "ilt_page_desc", 1);
	offset += qed_dump_num_param(dump_buf + offset,
				     continue_dump,
				     "size",
				     num_pages * PAGE_MEM_DESC_SIZE_DWORDS);

	/* Copy memory descriptors to dump buffer
	 * If need to dump then dump till the dump buffer size
	 */
	if (continue_dump) {
		for (page_id = 0; page_id < num_pages;
		     page_id++, offset += PAGE_MEM_DESC_SIZE_DWORDS) {
			if (continue_dump &&
			    (offset + PAGE_MEM_DESC_SIZE_DWORDS <=
			     buf_size_in_dwords)) {
				memcpy(dump_buf + offset,
				       &ilt_pages[page_id],
				       DWORDS_TO_BYTES
				       (PAGE_MEM_DESC_SIZE_DWORDS));
			} else {
				if (continue_dump) {
					continue_dump = false;
					actual_dump_size_in_dwords = offset;
				}
			}
		}
	} else {
		offset += num_pages * PAGE_MEM_DESC_SIZE_DWORDS;
	}

	valid_conn_pf_pages = DIV_ROUND_UP(valid_conn_pf_cids,
					   num_cids_per_page);
	valid_conn_vf_pages = DIV_ROUND_UP(valid_conn_vf_cids,
					   num_cids_per_page);

	/* Dump ILT pages IDs */
	qed_ilt_dump_pages_section(p_hwfn, dump_buf, &offset, &continue_dump,
				   valid_conn_pf_pages, valid_conn_vf_pages,
				   ilt_pages, true, buf_size_in_dwords,
				   &actual_dump_size_in_dwords);

	/* Dump ILT pages memory */
	qed_ilt_dump_pages_section(p_hwfn, dump_buf, &offset, &continue_dump,
				   valid_conn_pf_pages, valid_conn_vf_pages,
				   ilt_pages, false, buf_size_in_dwords,
				   &actual_dump_size_in_dwords);

	real_dumped_size =
	    (continue_dump == dump) ? offset : actual_dump_size_in_dwords;
	qed_dump_num_param(dump_buf + full_dump_size_param_offset, dump,
			   "full-dump-size", offset + last_section_size);
	qed_dump_num_param(dump_buf + actual_dump_size_param_offset,
			   dump,
			   "actual-dump-size",
			   real_dumped_size + last_section_size);

	/* Dump last section */
	real_dumped_size += qed_dump_last_section(dump_buf,
						  real_dumped_size, dump);

	return real_dumped_size;
}

/* Set/clear bits in the specified register */
static void qed_update_reg_bits(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt,
				u32 reg_addr, bool set, u32 mask)
{
	u32 val;

	val = qed_rd(p_hwfn, p_ptt, reg_addr);
	qed_wr(p_hwfn, p_ptt, reg_addr, set ? (val | mask) : (val & ~mask));
}

/* Set/clear parity AEU bits */
static void qed_update_parity_aeu_bits(struct qed_hwfn *p_hwfn,
				       struct qed_ptt *p_ptt,
				       int aeu_id,
				       bool set,
				       u32 parity_mask, u32 sys_kill_mask)
{
	u32 aeu_reg_offset;

	if (aeu_id < 1 || aeu_id > 9)
		return;

	aeu_reg_offset = (aeu_id - 1) * 4;

	/* Set all parity bits to trigger Close-The-Gate NIG */
	qed_update_reg_bits(p_hwfn,
			    p_ptt,
			    MISC_REG_AEU_ENABLE1_IGU_OUT_0 + aeu_reg_offset,
			    set, parity_mask);
	qed_update_reg_bits(p_hwfn,
			    p_ptt,
			    MISC_REG_AEU_ENABLE1_IGU_OUT_1 + aeu_reg_offset,
			    set, parity_mask);
	qed_update_reg_bits(p_hwfn,
			    p_ptt,
			    MISC_REG_AEU_ENABLE1_IGU_OUT_2 + aeu_reg_offset,
			    set, parity_mask);
	qed_update_reg_bits(p_hwfn,
			    p_ptt,
			    MISC_REG_AEU_ENABLE1_IGU_OUT_3 + aeu_reg_offset,
			    set, parity_mask);
	qed_update_reg_bits(p_hwfn,
			    p_ptt,
			    MISC_REG_AEU_ENABLE1_IGU_OUT_4 + aeu_reg_offset,
			    set, parity_mask);
	qed_update_reg_bits(p_hwfn,
			    p_ptt,
			    MISC_REG_AEU_ENABLE1_IGU_OUT_5 + aeu_reg_offset,
			    set, parity_mask);
	qed_update_reg_bits(p_hwfn,
			    p_ptt,
			    MISC_REG_AEU_ENABLE1_IGU_OUT_6 + aeu_reg_offset,
			    set, parity_mask);
	qed_update_reg_bits(p_hwfn,
			    p_ptt,
			    MISC_REG_AEU_ENABLE1_IGU_OUT_7 + aeu_reg_offset,
			    set, parity_mask);

	/* Set all parity bits to trigger Close-The-Gate NIG */
	qed_update_reg_bits(p_hwfn,
			    p_ptt,
			    MISC_REG_AEU_ENABLE1_NIG + aeu_reg_offset,
			    set, parity_mask);

	/* Set all parity bits to trigger Close-The-Gate PXP */
	qed_update_reg_bits(p_hwfn,
			    p_ptt,
			    MISC_REG_AEU_ENABLE1_PXP + aeu_reg_offset,
			    set, parity_mask);

	/* Route the parity bits to MCP output 0 in the AEU. When triggered,
	 * the MCPF_ATTENTIONS_BITS_PARITY_ERROR bit will be set.
	 */
	qed_update_reg_bits(p_hwfn,
			    p_ptt,
			    MISC_REG_AEU_ENABLE1_MCP_OUT_0 + aeu_reg_offset,
			    set, parity_mask);

	/* Route parity to all loaded driver.
	 * Configure specific blocks to uncorrectable errors per definition.
	 */
	qed_update_reg_bits(p_hwfn,
			    p_ptt,
			    MISC_REG_AEU_ENABLE1_GLB_UNC_ERR + aeu_reg_offset,
			    set, sys_kill_mask);

	/* Configure specific blocks to generate system kill per definition */
	qed_update_reg_bits(p_hwfn,
			    p_ptt,
			    MISC_REG_AEU_ENABLE1_SYS_KILL + aeu_reg_offset,
			    set, sys_kill_mask);
}

/***************************** Public Functions *******************************/

enum dbg_status qed_dbg_set_bin_ptr(struct qed_hwfn *p_hwfn,
				    const u8 * const bin_ptr)
{
	struct bin_buffer_hdr *buf_hdrs = (struct bin_buffer_hdr *)bin_ptr;
	u8 buf_id;

	/* Convert binary data to debug arrays */
	for (buf_id = 0; buf_id < MAX_BIN_DBG_BUFFER_TYPE; buf_id++)
		qed_set_dbg_bin_buf(p_hwfn,
				    (enum bin_dbg_buffer_type)buf_id,
				    (u32 *) (bin_ptr + buf_hdrs[buf_id].offset),
				    buf_hdrs[buf_id].length);

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_set_app_ver(u32 ver)
{
	if (ver < TOOLS_VERSION)
		return DBG_STATUS_UNSUPPORTED_APP_VERSION;

	s_app_ver = ver;

	return DBG_STATUS_OK;
}

u32 qed_dbg_get_fw_func_ver(void)
{
	return TOOLS_VERSION;
}

enum chip_ids qed_dbg_get_chip_id(struct qed_hwfn *p_hwfn)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;

	return (enum chip_ids)dev_data->chip_id;
}

void qed_read_regs(struct qed_hwfn *p_hwfn,
		   struct qed_ptt *p_ptt, u32 * buf, u32 addr, u32 len)
{
	u32 i;

	for (i = 0; i < len; i++)
		buf[i] = qed_rd(p_hwfn, p_ptt, DWORDS_TO_BYTES(addr + i));
}

bool qed_read_fw_info(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, struct fw_info *fw_info)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u8 storm_id;

	for (storm_id = 0; storm_id < MAX_DBG_STORMS; storm_id++) {
		struct storm_defs *storm = &s_storm_defs[storm_id];

		/* Skip Storm if it's in reset */
		if (dev_data->block_in_reset[storm->sem_block_id])
			continue;

		/* Read FW info for the current Storm */
		qed_read_storm_fw_info(p_hwfn, p_ptt, storm_id, fw_info);

		return true;
	}

	return false;
}

enum dbg_status qed_dbg_bus_reset(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt,
				  bool one_shot_en,
				  __maybe_unused u8 hw_dwords,
				  __maybe_unused bool unify_inputs,
				  bool grc_input_en)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	enum dbg_status status;

	status = qed_dbg_dev_init(p_hwfn);
	if (status != DBG_STATUS_OK)
		return status;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DEBUG,
		   "dbg_bus_reset: one_shot_en = %d, grc_input_en = %d\n",
		   one_shot_en, grc_input_en);

	if (qed_rd(p_hwfn, p_ptt, DBG_REG_DBG_BLOCK_ON))
		return DBG_STATUS_DBG_BUS_IN_USE;

	/* Update reset state of all blocks */
	qed_update_blocks_reset_state(p_hwfn, p_ptt);

	/* Disable all debug inputs */
	status = qed_bus_disable_inputs(p_hwfn, p_ptt, false);
	if (status != DBG_STATUS_OK)
		return status;

	/* Reset DBG block */
	qed_bus_reset_dbg_block(p_hwfn, p_ptt);

	/* Set one-shot / wrap-around */
	qed_wr(p_hwfn, p_ptt, DBG_REG_FULL_MODE, one_shot_en ? 0 : 1);

	/* Init state params */
	memset(&dev_data->bus, 0, sizeof(dev_data->bus));
	dev_data->bus.target = DBG_BUS_TARGET_ID_INT_BUF;
	dev_data->bus.state = DBG_BUS_STATE_READY;
	dev_data->bus.one_shot_en = one_shot_en;
	dev_data->bus.grc_input_en = grc_input_en;

	/* Init special DBG block */
	if (grc_input_en)
		qed_bus_add_block_input(p_hwfn, BLOCK_DBG, 0, 0x1, 0, 0, 0, 0);

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_bus_set_pci_output(struct qed_hwfn *p_hwfn,
					   struct qed_ptt *p_ptt,
					   u16 buf_size_kb)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	dma_addr_t pci_buf_phys_addr;
	void *pci_buf;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DEBUG,
		   "dbg_bus_set_pci_output: buf_size_kb = %d\n", buf_size_kb);

	if (dev_data->bus.target != DBG_BUS_TARGET_ID_INT_BUF)
		return DBG_STATUS_OUTPUT_ALREADY_SET;
	if (dev_data->bus.state != DBG_BUS_STATE_READY ||
	    dev_data->bus.pci_buf.size > 0)
		return DBG_STATUS_DBG_BLOCK_NOT_RESET;

	dev_data->bus.target = DBG_BUS_TARGET_ID_PCI;
	dev_data->bus.pci_buf.size = buf_size_kb * 1024;
	if (dev_data->bus.pci_buf.size % PCI_PKT_SIZE_IN_BYTES)
		return DBG_STATUS_INVALID_ARGS;

	pci_buf = dma_alloc_coherent(&p_hwfn->cdev->pdev->dev,
				     dev_data->bus.pci_buf.size,
				     &pci_buf_phys_addr, GFP_KERNEL);
	if (!pci_buf)
		return DBG_STATUS_PCI_BUF_ALLOC_FAILED;

	memcpy(&dev_data->bus.pci_buf.phys_addr, &pci_buf_phys_addr,
	       sizeof(pci_buf_phys_addr));

	dev_data->bus.pci_buf.virt_addr.lo = (u32) ((u64) (uintptr_t) pci_buf);
	dev_data->bus.pci_buf.virt_addr.hi =
	    (u32) ((u64) (uintptr_t) pci_buf >> 32);

	qed_wr(p_hwfn,
	       p_ptt,
	       DBG_REG_PCI_EXT_BUFFER_STRT_ADDR_LSB,
	       dev_data->bus.pci_buf.phys_addr.lo);
	qed_wr(p_hwfn,
	       p_ptt,
	       DBG_REG_PCI_EXT_BUFFER_STRT_ADDR_MSB,
	       dev_data->bus.pci_buf.phys_addr.hi);
	qed_wr(p_hwfn,
	       p_ptt, DBG_REG_TARGET_PACKET_SIZE, PCI_PKT_SIZE_IN_CHUNKS);
	qed_wr(p_hwfn,
	       p_ptt,
	       DBG_REG_PCI_EXT_BUFFER_SIZE,
	       dev_data->bus.pci_buf.size / PCI_PKT_SIZE_IN_BYTES);
	qed_wr(p_hwfn, p_ptt, DBG_REG_PCI_FUNC_NUM,
	       OPAQUE_FID(p_hwfn->rel_pf_id));
	qed_wr(p_hwfn, p_ptt, DBG_REG_PCI_LOGIC_ADDR, PCI_PHYS_ADDR_TYPE);
	qed_wr(p_hwfn, p_ptt, DBG_REG_PCI_REQ_CREDIT, PCI_REQ_CREDIT);
	qed_wr(p_hwfn, p_ptt, DBG_REG_DEBUG_TARGET, DBG_BUS_TARGET_ID_PCI);
	qed_wr(p_hwfn, p_ptt, DBG_REG_OUTPUT_ENABLE, TARGET_EN_MASK_PCI);

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_bus_set_nw_output(struct qed_hwfn *p_hwfn,
					  struct qed_ptt *p_ptt,
					  u8 port_id,
					  u32 dest_addr_lo32,
					  u16 dest_addr_hi16,
					  u16
					  data_limit_size_kb,
					  bool
					  send_to_other_engine,
					  bool rcv_from_other_engine)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DEBUG,
		   "dbg_bus_set_nw_output: port_id = %d, dest_addr_lo32 = 0x%x, dest_addr_hi16 = 0x%x, data_limit_size_kb = %d, send_to_other_engine = %d, rcv_from_other_engine = %d\n",
		   port_id,
		   dest_addr_lo32,
		   dest_addr_hi16,
		   data_limit_size_kb,
		   send_to_other_engine, rcv_from_other_engine);

	if (dev_data->bus.target != DBG_BUS_TARGET_ID_INT_BUF)
		return DBG_STATUS_OUTPUT_ALREADY_SET;
	if (dev_data->bus.state != DBG_BUS_STATE_READY)
		return DBG_STATUS_DBG_BLOCK_NOT_RESET;
	if ((send_to_other_engine ||
	     rcv_from_other_engine) && dev_data->chip_id != CHIP_BB)
		return DBG_STATUS_OTHER_ENGINE_BB_ONLY;
	if (port_id >= dev_data->num_ports ||
	    (send_to_other_engine && rcv_from_other_engine))
		return DBG_STATUS_INVALID_ARGS;

	dev_data->bus.target = DBG_BUS_TARGET_ID_NIG;
	dev_data->bus.rcv_from_other_engine = rcv_from_other_engine;

	qed_wr(p_hwfn, p_ptt, DBG_REG_OUTPUT_ENABLE, TARGET_EN_MASK_NIG);
	qed_wr(p_hwfn, p_ptt, DBG_REG_DEBUG_TARGET, DBG_BUS_TARGET_ID_NIG);

	if (send_to_other_engine)
		qed_wr(p_hwfn,
		       p_ptt,
		       DBG_REG_OTHER_ENGINE_MODE,
		       DBG_BUS_OTHER_ENGINE_MODE_CROSS_ENGINE_TX);
	else
		qed_wr(p_hwfn, p_ptt, NIG_REG_DEBUG_PORT, port_id);

	if (rcv_from_other_engine) {
		qed_wr(p_hwfn,
		       p_ptt,
		       DBG_REG_OTHER_ENGINE_MODE,
		       DBG_BUS_OTHER_ENGINE_MODE_CROSS_ENGINE_RX);
	} else {
		/* Configure ethernet header of 14 bytes */
		qed_wr(p_hwfn, p_ptt, DBG_REG_ETHERNET_HDR_WIDTH, 0);
		qed_wr(p_hwfn, p_ptt, DBG_REG_ETHERNET_HDR_7, dest_addr_lo32);
		qed_wr(p_hwfn,
		       p_ptt,
		       DBG_REG_ETHERNET_HDR_6,
		       (u32) SRC_MAC_ADDR_LO16 | ((u32) dest_addr_hi16 << 16));
		qed_wr(p_hwfn, p_ptt, DBG_REG_ETHERNET_HDR_5,
		       SRC_MAC_ADDR_HI32);
		qed_wr(p_hwfn, p_ptt, DBG_REG_ETHERNET_HDR_4,
		       (u32) ETH_TYPE << 16);
		qed_wr(p_hwfn, p_ptt, DBG_REG_TARGET_PACKET_SIZE,
		       NIG_PKT_SIZE_IN_CHUNKS);
		if (data_limit_size_kb)
			qed_wr(p_hwfn,
			       p_ptt,
			       DBG_REG_NIG_DATA_LIMIT_SIZE,
			       (data_limit_size_kb *
				1024) / CHUNK_SIZE_IN_BYTES);
	}

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_bus_enable_block(struct qed_hwfn *p_hwfn,
					 enum block_id block_id,
					 u8 line_num,
					 u8 enable_mask,
					 u8 right_shift,
					 u8
					 force_valid_mask, u8 force_frame_mask)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_data *bus = &dev_data->bus;
	struct dbg_bus_block_data *block_bus;
	const struct dbg_bus_line *dbg_line;
	const struct dbg_block_chip *block;
	bool is_removed, has_dbg_bus;
	u16 modes_buf_offset;
	u8 is_256b_line;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DEBUG,
		   "dbg_bus_enable_block: block = %d, line_num = %d, enable_mask = 0x%x, right_shift = %d, force_valid_mask = 0x%x, force_frame_mask = 0x%x\n",
		   block_id,
		   line_num,
		   enable_mask,
		   right_shift, force_valid_mask, force_frame_mask);

	if (block_id < 0 || block_id >= MAX_BLOCK_ID)
		return DBG_STATUS_INVALID_ARGS;

	block = qed_get_dbg_block_per_chip(p_hwfn, block_id);
	is_removed = GET_FIELD(block->flags, DBG_BLOCK_CHIP_IS_REMOVED);
	has_dbg_bus = GET_FIELD(block->flags, DBG_BLOCK_CHIP_HAS_DBG_BUS);

	/* Check debug bus mode */
	if (!is_removed && has_dbg_bus &&
	    GET_FIELD(block->dbg_bus_mode.data, DBG_MODE_HDR_EVAL_MODE) > 0) {
		modes_buf_offset = GET_FIELD(block->dbg_bus_mode.data,
					     DBG_MODE_HDR_MODES_BUF_OFFSET);
		if (!qed_is_mode_match(p_hwfn, &modes_buf_offset))
			has_dbg_bus = false;
	}

	block_bus = &bus->blocks[block_id];
	dbg_line = get_dbg_bus_line(p_hwfn, block_id, line_num);
	is_256b_line = dbg_line && GET_FIELD(dbg_line->data,
					     DBG_BUS_LINE_IS_256B) ? 1 : 0;

	if (bus->state != DBG_BUS_STATE_READY)
		return DBG_STATUS_DBG_BLOCK_NOT_RESET;
	if (block_bus->enable_mask)
		return DBG_STATUS_BLOCK_ALREADY_ENABLED;
	if (is_removed ||
	    !has_dbg_bus ||
	    line_num >= NUM_DBG_LINES(block) ||
	    !enable_mask ||
	    enable_mask > MAX_BLOCK_VALUES_MASK ||
	    force_valid_mask > MAX_BLOCK_VALUES_MASK ||
	    force_frame_mask > MAX_BLOCK_VALUES_MASK)
		return DBG_STATUS_INVALID_ARGS;
	if (dev_data->block_in_reset[block_id])
		return DBG_STATUS_BLOCK_IN_RESET;
	if (is_256b_line && (bus->trigger_en || bus->filter_en))
		return DBG_STATUS_NO_FILTER_TRIGGER_256B;

	/* Verify the following in all enabled blocks:
	 * - configured debug lines have the same width (128/256 bit).
	 */
	if (bus->num_enabled_blocks) {
		u8 curr_block_id;

		for (curr_block_id = 0; curr_block_id < MAX_BLOCK_ID;
		     curr_block_id++) {
			struct dbg_bus_block_data *curr_block_bus =
			    &bus->blocks[curr_block_id];

			if (curr_block_id == block_id)
				continue;

			if (curr_block_bus->enable_mask) {
				u8 curr_is_256b_line =
				    GET_FIELD(block_bus->flags,
					      DBG_BUS_BLOCK_DATA_IS_256B_LINE);

				if (is_256b_line != curr_is_256b_line)
					return DBG_STATUS_NON_MATCHING_LINES;
			}
		}
	}

	qed_bus_add_block_input(p_hwfn,
				block_id,
				line_num,
				enable_mask,
				right_shift,
				force_valid_mask,
				force_frame_mask, is_256b_line);

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_bus_enable_storm(struct qed_hwfn *p_hwfn,
					 enum dbg_storms
					 storm_id,
					 enum dbg_bus_storm_modes storm_mode)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_data *bus = &dev_data->bus;
	struct dbg_bus_storm_data *storm_bus;
	struct storm_defs *storm;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DEBUG,
		   "dbg_bus_enable_storm: storm = %d, storm_mode = %d\n",
		   storm_id, storm_mode);

	if (bus->state != DBG_BUS_STATE_READY)
		return DBG_STATUS_DBG_BLOCK_NOT_RESET;
	if (storm_id < 0 || storm_id >= MAX_DBG_STORMS)
		return DBG_STATUS_INVALID_ARGS;
	if (storm_mode >= MAX_DBG_BUS_STORM_MODES || storm_mode < 0)
		return DBG_STATUS_INVALID_ARGS;
	if (!s_storm_mode_defs[storm_mode].exists[dev_data->chip_id])
		return DBG_STATUS_INVALID_STORM_DBG_MODE;
	if (bus->storms[storm_id].enabled)
		return DBG_STATUS_STORM_ALREADY_ENABLED;

	storm = &s_storm_defs[storm_id];
	storm_bus = &bus->storms[storm_id];

	if (dev_data->block_in_reset[storm->sem_block_id])
		return DBG_STATUS_BLOCK_IN_RESET;

	storm_bus->enabled = true;
	storm_bus->mode = (u8) storm_mode;
	storm_bus->hw_id = bus->num_enabled_storms++;

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_bus_enable_timestamp(struct qed_hwfn *p_hwfn,
					     struct qed_ptt *p_ptt,
					     u8 valid_mask,
					     u8 frame_mask, u32 tick_len)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_data *bus = &dev_data->bus;

	u8 max_en_mask =
	    BIT((s_chip_defs[dev_data->chip_id].dwords_per_cycle - 1)) - 1;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DEBUG,
		   "dbg_bus_enable_timestamp: valid_mask = 0x%x, frame_mask = 0x%x, tick_len = %d\n",
		   valid_mask, frame_mask, tick_len);

	if (bus->state != DBG_BUS_STATE_READY)
		return DBG_STATUS_DBG_BLOCK_NOT_RESET;
	if (valid_mask > max_en_mask || frame_mask > max_en_mask)
		return DBG_STATUS_INVALID_ARGS;

	bus->timestamp_input_en = true;

	qed_wr(p_hwfn, p_ptt, DBG_REG_TIMESTAMP_VALID_EN, valid_mask);
	qed_wr(p_hwfn, p_ptt, DBG_REG_TIMESTAMP_FRAME_EN, frame_mask);
	qed_wr(p_hwfn, p_ptt, DBG_REG_TIMESTAMP_TICK, tick_len);

	qed_bus_add_block_input(p_hwfn,
				BLOCK_DBG, 0, 0x1, 0, false, false, false);

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_bus_add_eid_range_sem_filter(struct qed_hwfn *p_hwfn,
						     enum dbg_storms
						     storm_id,
						     u8 min_eid, u8 max_eid)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_storm_data *storm_bus;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DEBUG,
		   "dbg_bus_add_eid_range_sem_filter: storm = %d, min_eid = 0x%x, max_eid = 0x%x\n",
		   storm_id, min_eid, max_eid);

	if (storm_id < 0 || storm_id >= MAX_DBG_STORMS)
		return DBG_STATUS_INVALID_ARGS;
	if (min_eid > max_eid)
		return DBG_STATUS_INVALID_ARGS;
	if (!dev_data->bus.storms[storm_id].enabled)
		return DBG_STATUS_STORM_NOT_ENABLED;

	storm_bus = &dev_data->bus.storms[storm_id];
	storm_bus->eid_filter_en = 1;
	storm_bus->eid_range_not_mask = 1;
	storm_bus->eid_filter_params.range.min = min_eid;
	storm_bus->eid_filter_params.range.max = max_eid;

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_bus_add_eid_mask_sem_filter(struct qed_hwfn *p_hwfn,
						    enum dbg_storms
						    storm_id,
						    u8 eid_val, u8 eid_mask)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_storm_data *storm_bus;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DEBUG,
		   "dbg_bus_add_eid_mask_sem_filter: storm = %d, eid_val = 0x%x, eid_mask = 0x%x\n",
		   storm_id, eid_val, eid_mask);

	if (storm_id < 0 || storm_id >= MAX_DBG_STORMS)
		return DBG_STATUS_INVALID_ARGS;
	if (!dev_data->bus.storms[storm_id].enabled)
		return DBG_STATUS_STORM_NOT_ENABLED;

	storm_bus = &dev_data->bus.storms[storm_id];
	storm_bus->eid_filter_en = 1;
	storm_bus->eid_range_not_mask = 0;
	storm_bus->eid_filter_params.mask.val = eid_val;
	storm_bus->eid_filter_params.mask.mask = eid_mask;

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_bus_add_cid_sem_filter(struct qed_hwfn *p_hwfn,
					       enum dbg_storms storm_id,
					       u32 cid)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_storm_data *storm_bus;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DEBUG,
		   "dbg_bus_add_cid_sem_filter: storm = %d, cid = 0x%x\n",
		   storm_id, cid);

	if (storm_id < 0 || storm_id >= MAX_DBG_STORMS)
		return DBG_STATUS_INVALID_ARGS;
	if (!dev_data->bus.storms[storm_id].enabled)
		return DBG_STATUS_STORM_NOT_ENABLED;

	storm_bus = &dev_data->bus.storms[storm_id];
	storm_bus->cid_filter_en = 1;
	storm_bus->cid = cid;

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_bus_enable_filter(struct qed_hwfn *p_hwfn,
					  struct qed_ptt *p_ptt,
					  __maybe_unused enum block_id
					  block_id, u8 msg_len)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_data *bus = &dev_data->bus;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DEBUG,
		   "dbg_bus_enable_filter: msg_len = %d\n", msg_len);

	if (bus->state != DBG_BUS_STATE_READY)
		return DBG_STATUS_DBG_BLOCK_NOT_RESET;
	if (bus->filter_en)
		return DBG_STATUS_FILTER_ALREADY_ENABLED;
	if (bus->mode_256b_en)
		return DBG_STATUS_NO_FILTER_TRIGGER_256B;

	qed_wr(p_hwfn,
	       p_ptt, DBG_REG_FILTER_MSG_LENGTH_ENABLE, msg_len > 0 ? 1 : 0);
	if (msg_len > 0)
		qed_wr(p_hwfn, p_ptt, DBG_REG_FILTER_MSG_LENGTH, msg_len - 1);

	bus->filter_msg_len = msg_len;
	bus->filter_en = true;
	bus->next_constraint_id = 0;
	bus->adding_filter = true;

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_bus_enable_trigger(struct qed_hwfn *p_hwfn,
					   struct qed_ptt *p_ptt,
					   bool rec_pre_trigger,
					   u8 pre_chunks,
					   bool
					   rec_post_trigger,
					   u32 post_cycles,
					   bool
					   filter_pre_trigger,
					   bool filter_post_trigger)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	enum dbg_bus_post_trigger_types post_trigger_type;
	enum dbg_bus_pre_trigger_types pre_trigger_type;
	struct dbg_bus_data *bus = &dev_data->bus;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DEBUG,
		   "dbg_bus_enable_trigger: rec_pre_trigger = %d, pre_chunks = %d, rec_post_trigger = %d, post_cycles = %d, filter_pre_trigger = %d, filter_post_trigger = %d\n",
		   rec_pre_trigger,
		   pre_chunks,
		   rec_post_trigger,
		   post_cycles, filter_pre_trigger, filter_post_trigger);

	if (bus->state != DBG_BUS_STATE_READY)
		return DBG_STATUS_DBG_BLOCK_NOT_RESET;
	if (bus->trigger_en)
		return DBG_STATUS_TRIGGER_ALREADY_ENABLED;
	if (rec_pre_trigger && pre_chunks >= INT_BUF_SIZE_IN_CHUNKS)
		return DBG_STATUS_INVALID_ARGS;
	if (bus->mode_256b_en)
		return DBG_STATUS_NO_FILTER_TRIGGER_256B;

	bus->trigger_en = true;
	bus->filter_pre_trigger = filter_pre_trigger;
	bus->filter_post_trigger = filter_post_trigger;

	if (rec_pre_trigger) {
		pre_trigger_type =
		    pre_chunks ? DBG_BUS_PRE_TRIGGER_NUM_CHUNKS :
		    DBG_BUS_PRE_TRIGGER_FROM_ZERO;
		qed_wr(p_hwfn,
		       p_ptt,
		       DBG_REG_RCRD_ON_WINDOW_PRE_NUM_CHUNKS, pre_chunks);
	} else {
		pre_trigger_type = DBG_BUS_PRE_TRIGGER_DROP;
	}

	if (rec_post_trigger) {
		post_trigger_type = DBG_BUS_POST_TRIGGER_RECORD;
		qed_wr(p_hwfn,
		       p_ptt,
		       DBG_REG_RCRD_ON_WINDOW_POST_NUM_CYCLES,
		       post_cycles ? post_cycles : 0xffffffff);
	} else {
		post_trigger_type = DBG_BUS_POST_TRIGGER_DROP;
	}

	qed_wr(p_hwfn,
	       p_ptt,
	       DBG_REG_RCRD_ON_WINDOW_PRE_TRGR_EVNT_MODE, pre_trigger_type);
	qed_wr(p_hwfn,
	       p_ptt,
	       DBG_REG_RCRD_ON_WINDOW_POST_TRGR_EVNT_MODE, post_trigger_type);
	qed_wr(p_hwfn, p_ptt, DBG_REG_TRIGGER_ENABLE, 1);

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_bus_add_trigger_state(struct qed_hwfn *p_hwfn,
					      struct qed_ptt *p_ptt,
					      enum block_id block_id,
					      u8 msg_len, u16 count_to_next)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_trigger_state_data *trigger_state;
	struct dbg_bus_data *bus = &dev_data->bus;
	u8 reg_offset;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DEBUG,
		   "dbg_bus_add_trigger_state: block_id = %d, msg_len = %d, count_to_next = %d\n",
		   block_id, msg_len, count_to_next);

	if (!bus->trigger_en)
		return DBG_STATUS_TRIGGER_NOT_ENABLED;
	if (bus->next_trigger_state >= MAX_TRIGGER_STATES)
		return DBG_STATUS_TOO_MANY_TRIGGER_STATES;
	if (!count_to_next)
		return DBG_STATUS_INVALID_ARGS;

	trigger_state = &bus->trigger_states[bus->next_trigger_state];
	trigger_state->msg_len = msg_len;

	reg_offset = bus->next_trigger_state * BYTES_IN_DWORD;

	if (block_id != MAX_BLOCK_ID) {
		const struct dbg_block *block;
		enum dbg_storms associated_storm_id;

		block = get_dbg_block(p_hwfn, block_id);

		if (!block->associated_storm_letter)
			return DBG_STATUS_INVALID_ARGS;

		associated_storm_id =
		    qed_get_storm_id_from_letter(block->
						 associated_storm_letter);
		if (associated_storm_id == MAX_DBG_STORMS
		    || !bus->storms[associated_storm_id].enabled)
			return DBG_STATUS_STORM_NOT_ENABLED;

		trigger_state->storm_id = (u8) associated_storm_id;
	} else {
		trigger_state->storm_id = MAX_DBG_STORMS;
	}

	bus->next_constraint_id = 0;
	bus->adding_filter = false;

	/* Set trigger state registers */
	qed_wr(p_hwfn,
	       p_ptt,
	       DBG_REG_TRIGGER_STATE_MSG_LENGTH_ENABLE_0 + reg_offset,
	       msg_len > 0 ? 1 : 0);
	if (msg_len > 0)
		qed_wr(p_hwfn,
		       p_ptt,
		       DBG_REG_TRIGGER_STATE_MSG_LENGTH_0 + reg_offset,
		       msg_len - 1);

	/* Set trigger set registers */
	reg_offset = bus->next_trigger_state * TRIGGER_SETS_PER_STATE *
	    BYTES_IN_DWORD;
	qed_wr(p_hwfn,
	       p_ptt,
	       DBG_REG_TRIGGER_STATE_SET_COUNT_0 + reg_offset, count_to_next);

	/* Set next state to final state, and overwrite previous next state
	 * (if any).
	 */
	qed_wr(p_hwfn,
	       p_ptt,
	       DBG_REG_TRIGGER_STATE_SET_NXT_STATE_0 + reg_offset,
	       MAX_TRIGGER_STATES);
	if (bus->next_trigger_state > 0) {
		reg_offset =
		    (bus->next_trigger_state -
		     1) * TRIGGER_SETS_PER_STATE * BYTES_IN_DWORD;
		qed_wr(p_hwfn,
		       p_ptt,
		       DBG_REG_TRIGGER_STATE_SET_NXT_STATE_0 + reg_offset,
		       bus->next_trigger_state);
	}

	bus->next_trigger_state++;

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_bus_add_constraint(struct qed_hwfn *p_hwfn,
					   struct qed_ptt *p_ptt,
					   enum dbg_bus_constraint_ops
					   constraint_op,
					   u32
					   data_val,
					   u32
					   data_mask,
					   bool
					   compare_frame,
					   u8
					   frame_bit,
					   u8
					   cycle_offset,
					   u8
					   dword_offset_in_cycle,
					   bool is_mandatory)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_data *bus = &dev_data->bus;
	u16 dword_offset, range = 0;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DEBUG,
		   "dbg_bus_add_constraint: op = %d, data_val = 0x%x, data_mask = 0x%x, compare_frame = %d, frame_bit = %d, cycle_offset = %d, dword_offset_in_cycle = %d, is_mandatory = %d\n",
		   constraint_op,
		   data_val,
		   data_mask,
		   compare_frame,
		   frame_bit,
		   cycle_offset, dword_offset_in_cycle, is_mandatory);

	if (!bus->filter_en && !dev_data->bus.trigger_en)
		return DBG_STATUS_CANT_ADD_CONSTRAINT;
	if (bus->trigger_en && !bus->adding_filter && !bus->next_trigger_state)
		return DBG_STATUS_CANT_ADD_CONSTRAINT;
	if (bus->next_constraint_id >= MAX_CONSTRAINTS)
		return DBG_STATUS_TOO_MANY_CONSTRAINTS;
	if (constraint_op >= MAX_DBG_BUS_CONSTRAINT_OPS ||
	    constraint_op < 0 ||
	    frame_bit > 1 ||
	    dword_offset_in_cycle >=
	    s_chip_defs[dev_data->chip_id].dwords_per_cycle ||
	    (bus->adding_filter && cycle_offset >= MAX_FILTER_CYCLE_OFFSET))
		return DBG_STATUS_INVALID_ARGS;
	if (compare_frame &&
	    constraint_op != DBG_BUS_CONSTRAINT_OP_EQ &&
	    constraint_op != DBG_BUS_CONSTRAINT_OP_NE)
		return DBG_STATUS_INVALID_ARGS;

	dword_offset = cycle_offset *
	    s_chip_defs[dev_data->chip_id].dwords_per_cycle +
	    dword_offset_in_cycle;

	/* Add selected dword to constraints dword mask */
	if (bus->adding_filter) {
		bus->filter_constraint_dword_mask |=
		    (u8) BIT(dword_offset_in_cycle);
	} else {
		u8 curr_trigger_state_id = bus->next_trigger_state - 1;
		struct dbg_bus_trigger_state_data *trigger_state;

		trigger_state = &bus->trigger_states[curr_trigger_state_id];
		trigger_state->constraint_dword_mask |=
		    (u8) BIT(dword_offset_in_cycle);
	}

	/* Prepare data mask and range */
	if (constraint_op == DBG_BUS_CONSTRAINT_OP_EQ ||
	    constraint_op == DBG_BUS_CONSTRAINT_OP_NE) {
		data_mask = ~data_mask;
	} else {
		u8 lsb, width;

		/* Extract lsb and width from mask */
		if (!data_mask)
			return DBG_STATUS_INVALID_ARGS;

		for (lsb = 0; lsb < 32 && !(data_mask & 1);
		     lsb++, data_mask >>= 1) ;
		for (width = 0; width < 32 - lsb && (data_mask & 1);
		     width++, data_mask >>= 1) ;
		if (data_mask)
			return DBG_STATUS_INVALID_ARGS;
		range = (lsb << 5) | (width - 1);
	}

	/* Add constraint */
	qed_bus_set_constraint(p_hwfn,
			       p_ptt,
			       dev_data->bus.adding_filter ? 1 : 0,
			       dev_data->bus.next_constraint_id,
			       s_constraint_op_defs[constraint_op].hw_op_val,
			       data_val,
			       data_mask,
			       frame_bit,
			       compare_frame ? 0 : 1,
			       dword_offset,
			       range,
			       s_constraint_op_defs[constraint_op].is_cyclic ?
			       1 : 0, is_mandatory ? 1 : 0);

	/* If first constraint, fill other 3 constraints with dummy constraints
	 * that always match (using the same offset).
	 */
	if (!dev_data->bus.next_constraint_id) {
		u8 i;

		for (i = 1; i < MAX_CONSTRAINTS; i++)
			qed_bus_set_constraint(p_hwfn,
					       p_ptt,
					       bus->adding_filter ? 1 : 0,
					       i,
					       DBG_BUS_CONSTRAINT_OP_EQ,
					       0,
					       0xffffffff,
					       0, 1, dword_offset, 0, 0, 1);
	}

	bus->next_constraint_id++;

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_bus_start(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_data *bus = &dev_data->bus;
	u8 block_hw_ids[DEBUG_BUS_CYCLE_DWORDS];
	struct framing_mode_defs *framing_mode;
	enum dbg_status status;
	u32 block_id;
	u8 storm_id;

	DP_VERBOSE(p_hwfn, QED_MSG_DEBUG, "dbg_bus_start\n");

	if (bus->state != DBG_BUS_STATE_READY)
		return DBG_STATUS_DBG_BLOCK_NOT_RESET;

	/* Check if any input was enabled */
	if (!bus->num_enabled_storms &&
	    !bus->num_enabled_blocks && !bus->rcv_from_other_engine)
		return DBG_STATUS_NO_INPUT_ENABLED;

	/* Configure framing mode */
	framing_mode = qed_bus_get_framing_mode(p_hwfn);
	if (!framing_mode)
		return DBG_STATUS_NO_MATCHING_FRAMING_MODE;

	qed_wr(p_hwfn, p_ptt, DBG_REG_FRAMING_MODE, framing_mode->id);
	qed_wr(p_hwfn,
	       p_ptt, DBG_REG_FULL_BUFFER_THR, framing_mode->full_buf_thr);

	/* Configure DBG block for Storm inputs */
	if (bus->num_enabled_storms) {
		status = qed_bus_config_storm_inputs(p_hwfn, p_ptt);
		if (status != DBG_STATUS_OK)
			return status;
	}

	/* Assigns block HW IDs */
	status = qed_bus_assign_block_hw_ids(p_hwfn, block_hw_ids);
	if (status != DBG_STATUS_OK)
		return status;

	/* Configure DBG block for block inputs */
	if (bus->num_enabled_blocks) {
		status = qed_bus_config_block_inputs(p_hwfn,
						     p_ptt, block_hw_ids);
		if (status != DBG_STATUS_OK)
			return status;
	}

	/* Configure DBG block filters and triggers */
	if (bus->filter_en || bus->trigger_en) {
		status = qed_bus_config_filters_and_triggers(p_hwfn,
							     p_ptt,
							     framing_mode,
							     block_hw_ids);
		if (status != DBG_STATUS_OK)
			return status;
	}

	/* Restart timestamp */
	qed_wr(p_hwfn, p_ptt, DBG_REG_TIMESTAMP, 0);

	/* Enable debug block */
	qed_bus_enable_dbg_block(p_hwfn, p_ptt, 1);

	/* Configure enabled blocks - must be done before the DBG block is
	 * enabled.
	 */
	if (dev_data->bus.num_enabled_blocks) {
		/* Config blocks */
		for (block_id = 0; block_id < MAX_BLOCK_ID; block_id++) {
			struct dbg_bus_block_data *block_bus =
			    &bus->blocks[block_id];

			if (!block_bus->enable_mask || block_id == BLOCK_DBG)
				continue;

			qed_bus_config_dbg_line(p_hwfn,
						p_ptt,
						(enum block_id)block_id,
						block_bus->line_num,
						block_bus->enable_mask,
						block_bus->right_shift,
						block_bus->force_valid_mask,
						block_bus->force_frame_mask);
		}
	}

	/* Configure client mask */
	qed_bus_config_client_mask(p_hwfn, p_ptt);

	/* Configure enabled Storms - must be done after the DBG block is
	 * enabled.
	 */
	if (dev_data->bus.num_enabled_storms) {
		for (storm_id = 0; storm_id < MAX_DBG_STORMS; storm_id++) {
			if (!dev_data->bus.storms[storm_id].enabled)
				continue;
			qed_bus_add_storm_input(p_hwfn,
						p_ptt,
						(enum dbg_storms)storm_id,
						framing_mode->semi_framing_mode_id);
		}
	}

	dev_data->bus.state = DBG_BUS_STATE_RECORDING;

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_bus_stop(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_data *bus = &dev_data->bus;
	enum dbg_status status = DBG_STATUS_OK;

	DP_VERBOSE(p_hwfn, QED_MSG_DEBUG, "dbg_bus_stop\n");

	if (bus->state != DBG_BUS_STATE_RECORDING)
		return DBG_STATUS_RECORDING_NOT_STARTED;

	status = qed_bus_disable_inputs(p_hwfn, p_ptt, true);
	if (status != DBG_STATUS_OK)
		return status;

	qed_wr(p_hwfn, p_ptt, DBG_REG_CPU_TIMEOUT, 1);

	msleep(FLUSH_DELAY_MS);

	qed_bus_enable_dbg_block(p_hwfn, p_ptt, false);

	/* Check if trigger worked */
	if (bus->trigger_en) {
		u32 trigger_state = qed_rd(p_hwfn,
					   p_ptt,
					   DBG_REG_TRIGGER_STATUS_CUR_STATE);

		if (trigger_state != MAX_TRIGGER_STATES)
			return DBG_STATUS_DATA_DIDNT_TRIGGER;
	}

	bus->state = DBG_BUS_STATE_STOPPED;

	return status;
}

enum dbg_status qed_dbg_bus_get_dump_buf_size(struct qed_hwfn *p_hwfn,
					      struct qed_ptt *p_ptt,
					      u32 * buf_size)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct dbg_bus_data *bus = &dev_data->bus;
	enum dbg_status status;

	status = qed_dbg_dev_init(p_hwfn);

	*buf_size = 0;

	if (status != DBG_STATUS_OK)
		return status;

	/* Add dump header */
	*buf_size = (u32) qed_bus_dump_hdr(p_hwfn, p_ptt, NULL, false);

	switch (bus->target) {
	case DBG_BUS_TARGET_ID_INT_BUF:
		*buf_size += INT_BUF_SIZE_IN_DWORDS;
		break;
	case DBG_BUS_TARGET_ID_PCI:
		*buf_size += BYTES_TO_DWORDS(bus->pci_buf.size);
		break;
	default:
		break;
	}

	/* Dump last section */
	*buf_size += qed_dump_last_section(NULL, 0, false);

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_bus_dump(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt,
				 u32 * dump_buf,
				 u32 buf_size_in_dwords,
				 u32 * num_dumped_dwords)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u32 min_buf_size_in_dwords, block_id, offset = 0;
	struct dbg_bus_data *bus = &dev_data->bus;
	enum dbg_status status;
	u8 storm_id;

	*num_dumped_dwords = 0;

	status = qed_dbg_bus_get_dump_buf_size(p_hwfn,
					       p_ptt, &min_buf_size_in_dwords);
	if (status != DBG_STATUS_OK)
		return status;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DEBUG,
		   "dbg_bus_dump: dump_buf = 0x%p, buf_size_in_dwords = %d\n",
		   dump_buf, buf_size_in_dwords);

	if (bus->state != DBG_BUS_STATE_RECORDING && bus->state !=
	    DBG_BUS_STATE_STOPPED)
		return DBG_STATUS_RECORDING_NOT_STARTED;

	if (bus->state == DBG_BUS_STATE_RECORDING) {
		enum dbg_status stop_state = qed_dbg_bus_stop(p_hwfn, p_ptt);
		if (stop_state != DBG_STATUS_OK)
			return stop_state;
	}

	if (buf_size_in_dwords < min_buf_size_in_dwords)
		return DBG_STATUS_DUMP_BUF_TOO_SMALL;

	if (bus->target == DBG_BUS_TARGET_ID_PCI && !bus->pci_buf.size)
		return DBG_STATUS_PCI_BUF_NOT_ALLOCATED;

	/* Dump header */
	offset += qed_bus_dump_hdr(p_hwfn, p_ptt, dump_buf + offset, true);

	/* Dump recorded data */
	if (bus->target != DBG_BUS_TARGET_ID_NIG) {
		u32 recorded_dwords = qed_bus_dump_data(p_hwfn,
							p_ptt,
							dump_buf + offset,
							true);

		if (!recorded_dwords)
			return DBG_STATUS_NO_DATA_RECORDED;
		if (recorded_dwords % CHUNK_SIZE_IN_DWORDS)
			return DBG_STATUS_DUMP_NOT_CHUNK_ALIGNED;
		offset += recorded_dwords;
	}

	/**
	 * Dump last section
	 */
	offset += qed_dump_last_section(dump_buf, offset, true);

	/**
	 * If recorded to PCI buffer - free the buffer
	 */
	qed_bus_free_pci_buf(p_hwfn);

	/**
	 * Clear debug bus parameters
	 */
	bus->state = DBG_BUS_STATE_IDLE;
	bus->num_enabled_blocks = 0;
	bus->num_enabled_storms = 0;
	bus->filter_en = bus->trigger_en = 0;

	/**
	 * Clear block data
	 */
	for (block_id = 0; block_id < MAX_BLOCK_ID; block_id++)
		bus->blocks[block_id].enable_mask = 0;

	/**
	 * Clear Storm data
	 */
	for (storm_id = 0; storm_id < MAX_DBG_STORMS; storm_id++) {
		struct dbg_bus_storm_data *storm_bus = &bus->storms[storm_id];

		storm_bus->enabled = false;
		storm_bus->eid_filter_en = storm_bus->cid_filter_en = 0;
	}

	*num_dumped_dwords = offset;

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_grc_config(struct qed_hwfn *p_hwfn,
				   enum dbg_grc_params grc_param, u32 val)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	enum dbg_status status;
	int i;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DEBUG,
		   "dbg_grc_config: paramId = %d, val = %d\n", grc_param, val);

	status = qed_dbg_dev_init(p_hwfn);
	if (status != DBG_STATUS_OK)
		return status;

	/**
	 * Initializes the GRC parameters (if not initialized). Needed in order
	 * to set the default parameter values for the first time.
	 */
	qed_dbg_grc_init_params(p_hwfn);

	if (grc_param >= MAX_DBG_GRC_PARAMS || grc_param < 0)
		return DBG_STATUS_INVALID_ARGS;
	if (val < s_grc_param_defs[grc_param].min ||
	    val > s_grc_param_defs[grc_param].max)
		return DBG_STATUS_INVALID_ARGS;

	if (s_grc_param_defs[grc_param].is_preset) {
		/**
		 * Preset param
		 */

		/**
		 * Disabling a preset is not allowed. Call
		 * dbg_grc_set_params_default instead.
		 */
		if (!val)
			return DBG_STATUS_INVALID_ARGS;

		/**
		 * Update all params with the preset values
		 */
		for (i = 0; i < MAX_DBG_GRC_PARAMS; i++) {
			u32 preset_val;

			/**
			 * Skip persistent params
			 */
			if (s_grc_param_defs[i].is_persistent)
				continue;

			/**
			 * Find preset value
			 */
			if (grc_param == DBG_GRC_PARAM_EXCLUDE_ALL)
				preset_val =
				    s_grc_param_defs[i].exclude_all_preset_val;
			else if (grc_param == DBG_GRC_PARAM_CRASH)
				preset_val =
				    s_grc_param_defs[i].
				    crash_preset_val[dev_data->chip_id];
			else
				return DBG_STATUS_INVALID_ARGS;

			qed_grc_set_param(p_hwfn,
					  (enum dbg_grc_params)i, preset_val);
		}
	} else {
		/* Regular param - set its value */
		qed_grc_set_param(p_hwfn, grc_param, val);
	}

	return DBG_STATUS_OK;
}

/**
 * Assign default GRC param values
 */
void qed_dbg_grc_set_params_default(struct qed_hwfn *p_hwfn)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	u32 i;

	for (i = 0; i < MAX_DBG_GRC_PARAMS; i++)
		if (!s_grc_param_defs[i].is_persistent)
			dev_data->grc.param_val[i] =
			    s_grc_param_defs[i].default_val[dev_data->chip_id];
}

enum dbg_status qed_dbg_grc_get_dump_buf_size(struct qed_hwfn *p_hwfn,
					      struct qed_ptt *p_ptt,
					      u32 * buf_size)
{
	enum dbg_status status = qed_dbg_dev_init(p_hwfn);

	*buf_size = 0;

	if (status != DBG_STATUS_OK)
		return status;

	if (!p_hwfn->dbg_arrays[BIN_BUF_DBG_MODE_TREE].ptr ||
	    !p_hwfn->dbg_arrays[BIN_BUF_DBG_DUMP_REG].ptr ||
	    !p_hwfn->dbg_arrays[BIN_BUF_DBG_DUMP_MEM].ptr ||
	    !p_hwfn->dbg_arrays[BIN_BUF_DBG_ATTN_BLOCKS].ptr ||
	    !p_hwfn->dbg_arrays[BIN_BUF_DBG_ATTN_REGS].ptr)
		return DBG_STATUS_DBG_ARRAY_NOT_SET;

	return qed_grc_dump(p_hwfn, p_ptt, NULL, false, buf_size);
}

enum dbg_status qed_dbg_grc_dump(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt,
				 u32 * dump_buf,
				 u32 buf_size_in_dwords,
				 u32 * num_dumped_dwords)
{
	u32 needed_buf_size_in_dwords;
	enum dbg_status status;

	*num_dumped_dwords = 0;

	status = qed_dbg_grc_get_dump_buf_size(p_hwfn,
					       p_ptt,
					       &needed_buf_size_in_dwords);
	if (status != DBG_STATUS_OK)
		return status;

	if (buf_size_in_dwords < needed_buf_size_in_dwords)
		return DBG_STATUS_DUMP_BUF_TOO_SMALL;

	/**
	 * Doesn't do anything, needed for compile time asserts
	 */
	qed_static_asserts();

	/**
	 * GRC Dump
	 */
	status = qed_grc_dump(p_hwfn, p_ptt, dump_buf, true, num_dumped_dwords);

	/**
	 * Revert GRC params to their default
	 */
	qed_dbg_grc_set_params_default(p_hwfn);

	return status;
}

enum dbg_status qed_dbg_idle_chk_get_dump_buf_size(struct qed_hwfn *p_hwfn,
						   struct qed_ptt *p_ptt,
						   u32 * buf_size)
{
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	struct idle_chk_data *idle_chk = &dev_data->idle_chk;
	enum dbg_status status;

	*buf_size = 0;

	status = qed_dbg_dev_init(p_hwfn);
	if (status != DBG_STATUS_OK)
		return status;

	if (!p_hwfn->dbg_arrays[BIN_BUF_DBG_MODE_TREE].ptr ||
	    !p_hwfn->dbg_arrays[BIN_BUF_DBG_IDLE_CHK_REGS].ptr ||
	    !p_hwfn->dbg_arrays[BIN_BUF_DBG_IDLE_CHK_IMMS].ptr ||
	    !p_hwfn->dbg_arrays[BIN_BUF_DBG_IDLE_CHK_RULES].ptr)
		return DBG_STATUS_DBG_ARRAY_NOT_SET;

	if (!idle_chk->buf_size_set) {
		idle_chk->buf_size = qed_idle_chk_dump(p_hwfn,
						       p_ptt, NULL, false);
		idle_chk->buf_size_set = true;
	}

	*buf_size = idle_chk->buf_size;

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_idle_chk_dump(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      u32 * dump_buf,
				      u32 buf_size_in_dwords,
				      u32 * num_dumped_dwords)
{
	u32 needed_buf_size_in_dwords;
	enum dbg_status status;

	*num_dumped_dwords = 0;

	status = qed_dbg_idle_chk_get_dump_buf_size(p_hwfn,
						    p_ptt,
						    &needed_buf_size_in_dwords);
	if (status != DBG_STATUS_OK)
		return status;

	if (buf_size_in_dwords < needed_buf_size_in_dwords)
		return DBG_STATUS_DUMP_BUF_TOO_SMALL;

	/**
	 * Update reset state
	 */
	qed_grc_unreset_blocks(p_hwfn, p_ptt, true);
	qed_update_blocks_reset_state(p_hwfn, p_ptt);

	/**
	 * Idle Check Dump
	 */
	*num_dumped_dwords = qed_idle_chk_dump(p_hwfn, p_ptt, dump_buf, true);

	/**
	 * Revert GRC params to their default
	 */
	qed_dbg_grc_set_params_default(p_hwfn);

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_mcp_trace_get_dump_buf_size(struct qed_hwfn *p_hwfn,
						    struct qed_ptt *p_ptt,
						    u32 * buf_size)
{
	enum dbg_status status = qed_dbg_dev_init(p_hwfn);

	*buf_size = 0;

	if (status != DBG_STATUS_OK)
		return status;

	return qed_mcp_trace_dump(p_hwfn, p_ptt, NULL, false, buf_size);
}

enum dbg_status qed_dbg_mcp_trace_dump(struct qed_hwfn *p_hwfn,
				       struct qed_ptt *p_ptt,
				       u32 * dump_buf,
				       u32 buf_size_in_dwords,
				       u32 * num_dumped_dwords)
{
	u32 needed_buf_size_in_dwords;
	enum dbg_status status;

	status = qed_dbg_mcp_trace_get_dump_buf_size(p_hwfn,
						     p_ptt,
						     &needed_buf_size_in_dwords);
	if (status != DBG_STATUS_OK && status !=
	    DBG_STATUS_NVRAM_GET_IMAGE_FAILED)
		return status;

	if (buf_size_in_dwords < needed_buf_size_in_dwords)
		return DBG_STATUS_DUMP_BUF_TOO_SMALL;

	/**
	 * Update reset state
	 */
	qed_update_blocks_reset_state(p_hwfn, p_ptt);

	/**
	 * Perform dump
	 */
	status = qed_mcp_trace_dump(p_hwfn,
				    p_ptt, dump_buf, true, num_dumped_dwords);

	/**
	 * Revert GRC params to their default
	 */
	qed_dbg_grc_set_params_default(p_hwfn);

	return status;
}

enum dbg_status qed_dbg_reg_fifo_get_dump_buf_size(struct qed_hwfn *p_hwfn,
						   struct qed_ptt *p_ptt,
						   u32 * buf_size)
{
	enum dbg_status status = qed_dbg_dev_init(p_hwfn);

	*buf_size = 0;

	if (status != DBG_STATUS_OK)
		return status;

	return qed_reg_fifo_dump(p_hwfn, p_ptt, NULL, false, buf_size);
}

enum dbg_status qed_dbg_reg_fifo_dump(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      u32 * dump_buf,
				      u32 buf_size_in_dwords,
				      u32 * num_dumped_dwords)
{
	u32 needed_buf_size_in_dwords;
	enum dbg_status status;

	*num_dumped_dwords = 0;

	status = qed_dbg_reg_fifo_get_dump_buf_size(p_hwfn,
						    p_ptt,
						    &needed_buf_size_in_dwords);
	if (status != DBG_STATUS_OK)
		return status;

	if (buf_size_in_dwords < needed_buf_size_in_dwords)
		return DBG_STATUS_DUMP_BUF_TOO_SMALL;

	/**
	 * Update reset state
	 */
	qed_update_blocks_reset_state(p_hwfn, p_ptt);

	status = qed_reg_fifo_dump(p_hwfn,
				   p_ptt, dump_buf, true, num_dumped_dwords);

	/**
	 * Revert GRC params to their default
	 */
	qed_dbg_grc_set_params_default(p_hwfn);

	return status;
}

enum dbg_status qed_dbg_igu_fifo_get_dump_buf_size(struct qed_hwfn *p_hwfn,
						   struct qed_ptt *p_ptt,
						   u32 * buf_size)
{
	enum dbg_status status = qed_dbg_dev_init(p_hwfn);

	*buf_size = 0;

	if (status != DBG_STATUS_OK)
		return status;

	return qed_igu_fifo_dump(p_hwfn, p_ptt, NULL, false, buf_size);
}

enum dbg_status qed_dbg_igu_fifo_dump(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      u32 * dump_buf,
				      u32 buf_size_in_dwords,
				      u32 * num_dumped_dwords)
{
	u32 needed_buf_size_in_dwords;
	enum dbg_status status;

	*num_dumped_dwords = 0;

	status = qed_dbg_igu_fifo_get_dump_buf_size(p_hwfn,
						    p_ptt,
						    &needed_buf_size_in_dwords);
	if (status != DBG_STATUS_OK)
		return status;

	if (buf_size_in_dwords < needed_buf_size_in_dwords)
		return DBG_STATUS_DUMP_BUF_TOO_SMALL;

	/**
	 * Update reset state
	 */
	qed_update_blocks_reset_state(p_hwfn, p_ptt);

	status = qed_igu_fifo_dump(p_hwfn,
				   p_ptt, dump_buf, true, num_dumped_dwords);

	/**
	 * Revert GRC params to their default
	 */
	qed_dbg_grc_set_params_default(p_hwfn);

	return status;
}

enum dbg_status qed_dbg_protection_override_get_dump_buf_size(struct qed_hwfn
							      *p_hwfn,
							      struct qed_ptt
							      *p_ptt,
							      u32 * buf_size)
{
	enum dbg_status status = qed_dbg_dev_init(p_hwfn);

	*buf_size = 0;

	if (status != DBG_STATUS_OK)
		return status;

	return qed_protection_override_dump(p_hwfn,
					    p_ptt, NULL, false, buf_size);
}

enum dbg_status qed_dbg_protection_override_dump(struct qed_hwfn *p_hwfn,
						 struct qed_ptt *p_ptt,
						 u32 *
						 dump_buf,
						 u32
						 buf_size_in_dwords,
						 u32 * num_dumped_dwords)
{
	u32 needed_buf_size_in_dwords;
	enum dbg_status status;

	*num_dumped_dwords = 0;

	status = qed_dbg_protection_override_get_dump_buf_size(p_hwfn,
							       p_ptt,
							       &needed_buf_size_in_dwords);
	if (status != DBG_STATUS_OK)
		return status;

	if (buf_size_in_dwords < needed_buf_size_in_dwords)
		return DBG_STATUS_DUMP_BUF_TOO_SMALL;

	/**
	 * Update reset state
	 */
	qed_update_blocks_reset_state(p_hwfn, p_ptt);

	status = qed_protection_override_dump(p_hwfn,
					      p_ptt,
					      dump_buf,
					      true, num_dumped_dwords);

	/**
	 * Revert GRC params to their default
	 */
	qed_dbg_grc_set_params_default(p_hwfn);

	return status;
}

enum dbg_status qed_dbg_fw_asserts_get_dump_buf_size(struct qed_hwfn *p_hwfn,
						     struct qed_ptt *p_ptt,
						     u32 * buf_size)
{
	enum dbg_status status = qed_dbg_dev_init(p_hwfn);

	*buf_size = 0;

	if (status != DBG_STATUS_OK)
		return status;

	/**
	 * Update reset state
	 */
	qed_update_blocks_reset_state(p_hwfn, p_ptt);

	*buf_size = qed_fw_asserts_dump(p_hwfn, p_ptt, NULL, false);

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_fw_asserts_dump(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt,
					u32 * dump_buf,
					u32 buf_size_in_dwords,
					u32 * num_dumped_dwords)
{
	u32 needed_buf_size_in_dwords;
	enum dbg_status status;

	*num_dumped_dwords = 0;

	status = qed_dbg_fw_asserts_get_dump_buf_size(p_hwfn,
						      p_ptt,
						      &needed_buf_size_in_dwords);
	if (status != DBG_STATUS_OK)
		return status;

	if (buf_size_in_dwords < needed_buf_size_in_dwords)
		return DBG_STATUS_DUMP_BUF_TOO_SMALL;

	*num_dumped_dwords = qed_fw_asserts_dump(p_hwfn, p_ptt, dump_buf, true);

	/**
	 * Revert GRC params to their default
	 */
	qed_dbg_grc_set_params_default(p_hwfn);

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_ilt_get_dump_buf_size(struct qed_hwfn *p_hwfn,
					      struct qed_ptt *p_ptt,
					      u32 * buf_size)
{
	enum dbg_status status = qed_dbg_dev_init(p_hwfn);

	*buf_size = 0;

	if (status != DBG_STATUS_OK)
		return status;

	*buf_size = qed_ilt_dump(p_hwfn, p_ptt, NULL, 0, false);

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_ilt_dump(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt,
				 u32 * dump_buf,
				 u32 buf_size_in_dwords,
				 u32 * num_dumped_dwords)
{
	*num_dumped_dwords = qed_ilt_dump(p_hwfn,
					  p_ptt,
					  dump_buf, buf_size_in_dwords, true);

	/* Reveret GRC params to their default */
	qed_dbg_grc_set_params_default(p_hwfn);

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_read_attn(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt,
				  enum block_id block_id,
				  enum dbg_attn_type attn_type,
				  bool clear_status,
				  struct dbg_attn_block_result *results)
{
	enum dbg_status status = qed_dbg_dev_init(p_hwfn);
	u8 reg_idx, num_attn_regs, num_result_regs = 0;
	const struct dbg_attn_reg *attn_reg_arr;

	if (status != DBG_STATUS_OK)
		return status;

	if (!p_hwfn->dbg_arrays[BIN_BUF_DBG_MODE_TREE].ptr ||
	    !p_hwfn->dbg_arrays[BIN_BUF_DBG_ATTN_BLOCKS].ptr ||
	    !p_hwfn->dbg_arrays[BIN_BUF_DBG_ATTN_REGS].ptr)
		return DBG_STATUS_DBG_ARRAY_NOT_SET;

	attn_reg_arr = qed_get_block_attn_regs(p_hwfn,
					       block_id,
					       attn_type, &num_attn_regs);

	for (reg_idx = 0; reg_idx < num_attn_regs; reg_idx++) {
		const struct dbg_attn_reg *reg_data = &attn_reg_arr[reg_idx];
		struct dbg_attn_reg_result *reg_result;
		u32 sts_addr, sts_val;
		u16 modes_buf_offset;
		bool eval_mode;

		/**
		 * Check mode
		 */
		eval_mode = GET_FIELD(reg_data->mode.data,
				      DBG_MODE_HDR_EVAL_MODE) > 0;
		modes_buf_offset = GET_FIELD(reg_data->mode.data,
					     DBG_MODE_HDR_MODES_BUF_OFFSET);
		if (eval_mode && !qed_is_mode_match(p_hwfn, &modes_buf_offset))
			continue;

		/**
		 * Mode match - read attention status register
		 */
		sts_addr =
		    clear_status ? reg_data->
		    sts_clr_address : GET_FIELD(reg_data->data,
						DBG_ATTN_REG_STS_ADDRESS);
		sts_val = qed_rd(p_hwfn, p_ptt, DWORDS_TO_BYTES(sts_addr));
		if (!sts_val)
			continue;

		/**
		 * Non-zero attention status - add to results
		 */
		reg_result = &results->reg_results[num_result_regs];
		SET_FIELD(reg_result->data,
			  DBG_ATTN_REG_RESULT_STS_ADDRESS, sts_addr);
		SET_FIELD(reg_result->data,
			  DBG_ATTN_REG_RESULT_NUM_REG_ATTN,
			  GET_FIELD(reg_data->data, DBG_ATTN_REG_NUM_REG_ATTN));
		reg_result->block_attn_offset = reg_data->block_attn_offset;
		reg_result->sts_val = sts_val;
		reg_result->mask_val = qed_rd(p_hwfn,
					      p_ptt,
					      DWORDS_TO_BYTES
					      (reg_data->mask_address));
		num_result_regs++;
	}

	results->block_id = (u8) block_id;
	results->names_offset = qed_get_block_attn_data(p_hwfn,
							block_id,
							attn_type)->names_offset;
	SET_FIELD(results->data, DBG_ATTN_BLOCK_RESULT_ATTN_TYPE, attn_type);
	SET_FIELD(results->data,
		  DBG_ATTN_BLOCK_RESULT_NUM_REGS, num_result_regs);

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_print_attn(struct qed_hwfn *p_hwfn,
				   struct dbg_attn_block_result *results)
{
	enum dbg_attn_type attn_type;
	u8 num_regs, i;

	num_regs = GET_FIELD(results->data, DBG_ATTN_BLOCK_RESULT_NUM_REGS);
	attn_type = (enum dbg_attn_type)GET_FIELD(results->data,
						  DBG_ATTN_BLOCK_RESULT_ATTN_TYPE);

	for (i = 0; i < num_regs; i++) {
		struct dbg_attn_reg_result *reg_result;
		const char *attn_type_str;
		u32 sts_addr;

		reg_result = &results->reg_results[i];
		attn_type_str =
		    (attn_type == ATTN_TYPE_INTERRUPT ? "interrupt" : "parity");
		sts_addr = GET_FIELD(reg_result->data,
				     DBG_ATTN_REG_RESULT_STS_ADDRESS);
		DP_NOTICE(p_hwfn,
			  "%s: address 0x%08x, status 0x%08x, mask 0x%08x\n",
			  attn_type_str,
			  (u32) DWORDS_TO_BYTES(sts_addr),
			  reg_result->sts_val, reg_result->mask_val);
	}

	return DBG_STATUS_OK;
}

bool qed_is_block_in_reset(struct qed_hwfn * p_hwfn,
			   struct qed_ptt * p_ptt, enum block_id block_id)
{
	const struct dbg_reset_reg *reset_reg;
	const struct dbg_block_chip *block;
	u32 reset_reg_addr;

	block = qed_get_dbg_block_per_chip(p_hwfn, block_id);

	if (!GET_FIELD(block->flags, DBG_BLOCK_CHIP_HAS_RESET_REG))
		return false;

	reset_reg = qed_get_dbg_reset_reg(p_hwfn, block->reset_reg_id);

	if (GET_FIELD(reset_reg->data, DBG_RESET_REG_IS_REMOVED))
		return false;

	reset_reg_addr = GET_FIELD(reset_reg->data, DBG_RESET_REG_ADDR);
	return (qed_rd(p_hwfn, p_ptt,
		       reset_reg_addr) & BIT(block->reset_reg_bit_offset)) > 0;
}

void qed_mask_parities_no_mcp(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, bool mask)
{
	/* Clear General Attn 35 which is used for driver initiated parity */
	qed_wr(p_hwfn, p_ptt, MISC_REG_AEU_GENERAL_ATTN_35, 0);

	qed_update_parity_aeu_bits(p_hwfn,
				   p_ptt,
				   2,
				   !mask,
				   AEU_INPUT2_BITS_PARITY_ERROR,
				   AEU_INPUT2_BITS_GENERATE_SYSTEM_KILL);
	qed_update_parity_aeu_bits(p_hwfn, p_ptt, 4, !mask,
				   QED_IS_BB_B0(p_hwfn->cdev) ?
				   AEU_INPUT4_BITS_PARITY_ERROR_BB :
				   AEU_INPUT4_BITS_PARITY_ERROR_K2,
				   QED_IS_BB_B0(p_hwfn->cdev) ?
				   AEU_INPUT4_BITS_GENERATE_SYSTEM_KILL_BB :
				   AEU_INPUT4_BITS_GENERATE_SYSTEM_KILL_K2);
	qed_update_parity_aeu_bits(p_hwfn,
				   p_ptt,
				   5,
				   !mask,
				   AEU_INPUT5_BITS_PARITY_ERROR,
				   AEU_INPUT5_BITS_GENERATE_SYSTEM_KILL);
	qed_update_parity_aeu_bits(p_hwfn,
				   p_ptt,
				   6,
				   !mask,
				   AEU_INPUT6_BITS_PARITY_ERROR,
				   AEU_INPUT6_BITS_GENERATE_SYSTEM_KILL);
	qed_update_parity_aeu_bits(p_hwfn,
				   p_ptt,
				   7,
				   !mask,
				   AEU_INPUT7_BITS_PARITY_ERROR,
				   AEU_INPUT7_BITS_GENERATE_SYSTEM_KILL);
	qed_update_parity_aeu_bits(p_hwfn, p_ptt, 8, !mask,
				   QED_IS_BB_B0(p_hwfn->cdev) ?
				   AEU_INPUT8_BITS_PARITY_ERROR_BB :
				   AEU_INPUT8_BITS_PARITY_ERROR_K2,
				   QED_IS_BB_B0(p_hwfn->cdev) ?
				   AEU_INPUT8_BITS_GENERATE_SYSTEM_KILL_BB :
				   AEU_INPUT8_BITS_GENERATE_SYSTEM_KILL_K2);
	qed_update_parity_aeu_bits(p_hwfn,
				   p_ptt,
				   9,
				   !mask,
				   AEU_INPUT9_BITS_PARITY_ERROR,
				   AEU_INPUT9_BITS_GENERATE_SYSTEM_KILL);

	qed_update_reg_bits(p_hwfn, p_ptt, MISC_REG_AEU_MASK_ATTN_MCP, !mask,
			    1);

	qed_wr(p_hwfn, p_ptt, MISC_REG_AEU_SYS_KILL_BEHAVIOR, 0);
	qed_wr(p_hwfn, p_ptt, MISC_REG_AEU_GENERAL_MASK, 0);
}

bool qed_is_reg_read_valid(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 reg_val;

	reg_val = qed_rd(p_hwfn, p_ptt, PGLCS_REG_PGL_CS);

	return reg_val != 0xFFFFFFFF;
}

/**
 * ***************************** Data Types *********************************
 */

/* REG fifo element */
struct reg_fifo_element {
	u64 data;
#define REG_FIFO_ELEMENT_ADDRESS_SHIFT          0
#define REG_FIFO_ELEMENT_ADDRESS_MASK           0x7fffff
#define REG_FIFO_ELEMENT_ACCESS_SHIFT           23
#define REG_FIFO_ELEMENT_ACCESS_MASK            0x1
#define REG_FIFO_ELEMENT_PF_SHIFT               24
#define REG_FIFO_ELEMENT_PF_MASK                0xf
#define REG_FIFO_ELEMENT_VF_SHIFT               28
#define REG_FIFO_ELEMENT_VF_MASK                0xff
#define REG_FIFO_ELEMENT_PORT_SHIFT             36
#define REG_FIFO_ELEMENT_PORT_MASK              0x3
#define REG_FIFO_ELEMENT_PRIVILEGE_SHIFT        38
#define REG_FIFO_ELEMENT_PRIVILEGE_MASK         0x3
#define REG_FIFO_ELEMENT_PROTECTION_SHIFT       40
#define REG_FIFO_ELEMENT_PROTECTION_MASK        0x7
#define REG_FIFO_ELEMENT_MASTER_SHIFT           43
#define REG_FIFO_ELEMENT_MASTER_MASK            0xf
#define REG_FIFO_ELEMENT_ERROR_SHIFT            47
#define REG_FIFO_ELEMENT_ERROR_MASK             0x1f
};

/**
 * REG fifo error element
 */
struct reg_fifo_err {
	u32 err_code;
	const char *err_msg;
};

/**
 * IGU fifo element
 */
struct igu_fifo_element {
	u32 dword0;
#define IGU_FIFO_ELEMENT_DWORD0_FID_SHIFT       0
#define IGU_FIFO_ELEMENT_DWORD0_FID_MASK        0xff
#define IGU_FIFO_ELEMENT_DWORD0_IS_PF_SHIFT     8
#define IGU_FIFO_ELEMENT_DWORD0_IS_PF_MASK      0x1
#define IGU_FIFO_ELEMENT_DWORD0_SOURCE_SHIFT    9
#define IGU_FIFO_ELEMENT_DWORD0_SOURCE_MASK     0xf
#define IGU_FIFO_ELEMENT_DWORD0_ERR_TYPE_SHIFT  13
#define IGU_FIFO_ELEMENT_DWORD0_ERR_TYPE_MASK   0xf
#define IGU_FIFO_ELEMENT_DWORD0_CMD_ADDR_SHIFT  17
#define IGU_FIFO_ELEMENT_DWORD0_CMD_ADDR_MASK   0x7fff
	u32 dword1;
	u32 dword2;
#define IGU_FIFO_ELEMENT_DWORD12_IS_WR_CMD_SHIFT 0
#define IGU_FIFO_ELEMENT_DWORD12_IS_WR_CMD_MASK  0x1
#define IGU_FIFO_ELEMENT_DWORD12_WR_DATA_SHIFT   1
#define IGU_FIFO_ELEMENT_DWORD12_WR_DATA_MASK    0xffffffff
	u32 reserved;
};

struct igu_fifo_wr_data {
	u32 data;
#define IGU_FIFO_WR_DATA_PROD_CONS_SHIFT                0
#define IGU_FIFO_WR_DATA_PROD_CONS_MASK                 0xffffff
#define IGU_FIFO_WR_DATA_UPDATE_FLAG_SHIFT              24
#define IGU_FIFO_WR_DATA_UPDATE_FLAG_MASK               0x1
#define IGU_FIFO_WR_DATA_EN_DIS_INT_FOR_SB_SHIFT        25
#define IGU_FIFO_WR_DATA_EN_DIS_INT_FOR_SB_MASK         0x3
#define IGU_FIFO_WR_DATA_SEGMENT_SHIFT                  27
#define IGU_FIFO_WR_DATA_SEGMENT_MASK                   0x1
#define IGU_FIFO_WR_DATA_TIMER_MASK_SHIFT               28
#define IGU_FIFO_WR_DATA_TIMER_MASK_MASK                0x1
#define IGU_FIFO_WR_DATA_CMD_TYPE_SHIFT                 31
#define IGU_FIFO_WR_DATA_CMD_TYPE_MASK                  0x1
};

struct igu_fifo_cleanup_wr_data {
	u32 data;
#define IGU_FIFO_CLEANUP_WR_DATA_RESERVED_SHIFT         0
#define IGU_FIFO_CLEANUP_WR_DATA_RESERVED_MASK          0x7ffffff
#define IGU_FIFO_CLEANUP_WR_DATA_CLEANUP_VAL_SHIFT      27
#define IGU_FIFO_CLEANUP_WR_DATA_CLEANUP_VAL_MASK       0x1
#define IGU_FIFO_CLEANUP_WR_DATA_CLEANUP_TYPE_SHIFT     28
#define IGU_FIFO_CLEANUP_WR_DATA_CLEANUP_TYPE_MASK      0x7
#define IGU_FIFO_CLEANUP_WR_DATA_CMD_TYPE_SHIFT         31
#define IGU_FIFO_CLEANUP_WR_DATA_CMD_TYPE_MASK          0x1
};

/**
 * Protection override element
 */
struct protection_override_element {
	u64 data;
#define PROTECTION_OVERRIDE_ELEMENT_ADDRESS_SHIFT               0
#define PROTECTION_OVERRIDE_ELEMENT_ADDRESS_MASK                0x7fffff
#define PROTECTION_OVERRIDE_ELEMENT_WINDOW_SIZE_SHIFT           23
#define PROTECTION_OVERRIDE_ELEMENT_WINDOW_SIZE_MASK            0xffffff
#define PROTECTION_OVERRIDE_ELEMENT_READ_SHIFT                  47
#define PROTECTION_OVERRIDE_ELEMENT_READ_MASK                   0x1
#define PROTECTION_OVERRIDE_ELEMENT_WRITE_SHIFT                 48
#define PROTECTION_OVERRIDE_ELEMENT_WRITE_MASK                  0x1
#define PROTECTION_OVERRIDE_ELEMENT_READ_PROTECTION_SHIFT       49
#define PROTECTION_OVERRIDE_ELEMENT_READ_PROTECTION_MASK        0x7
#define PROTECTION_OVERRIDE_ELEMENT_WRITE_PROTECTION_SHIFT      52
#define PROTECTION_OVERRIDE_ELEMENT_WRITE_PROTECTION_MASK       0x7
};

enum igu_fifo_sources {
	IGU_SRC_PXP0,
	IGU_SRC_PXP1,
	IGU_SRC_PXP2,
	IGU_SRC_PXP3,
	IGU_SRC_PXP4,
	IGU_SRC_PXP5,
	IGU_SRC_PXP6,
	IGU_SRC_PXP7,
	IGU_SRC_CAU,
	IGU_SRC_ATTN,
	IGU_SRC_GRC
};

enum igu_fifo_addr_types {
	IGU_ADDR_TYPE_MSIX_MEM,
	IGU_ADDR_TYPE_WRITE_PBA,
	IGU_ADDR_TYPE_WRITE_INT_ACK,
	IGU_ADDR_TYPE_WRITE_ATTN_BITS,
	IGU_ADDR_TYPE_READ_INT,
	IGU_ADDR_TYPE_WRITE_PROD_UPDATE,
	IGU_ADDR_TYPE_RESERVED
};

struct igu_fifo_addr_data {
	u16 start_addr;
	u16 end_addr;
	char *desc;
	char *vf_desc;
	enum igu_fifo_addr_types type;
};

/**
 * ****************************** Constants *********************************
 */

#define MAX_MSG_LEN                             1024

#define MCP_TRACE_MAX_MODULE_LEN                8
#define MCP_TRACE_FORMAT_MAX_PARAMS             3
#define MCP_TRACE_FORMAT_PARAM_WIDTH                    ( \
		MCP_TRACE_FORMAT_P2_SIZE_OFFSET -	  \
		MCP_TRACE_FORMAT_P1_SIZE_OFFSET)

#ifndef REG_FIFO_ELEMENT_DWORDS
#define REG_FIFO_ELEMENT_DWORDS                 2
#endif
#define REG_FIFO_ELEMENT_ADDR_FACTOR            4
#define REG_FIFO_ELEMENT_IS_PF_VF_VAL           127

#ifndef IGU_FIFO_ELEMENT_DWORDS
#define IGU_FIFO_ELEMENT_DWORDS                 4
#endif

#ifndef PROTECTION_OVERRIDE_ELEMENT_DWORDS
#define PROTECTION_OVERRIDE_ELEMENT_DWORDS      2
#endif
#define PROTECTION_OVERRIDE_ELEMENT_ADDR_FACTOR 4

#define DBG_BUS_SIGNATURE_LINE_NAME             "signature"
#define DBG_BUS_SIGNATURE_LINE_NUM              0

#define DBG_BUS_LATENCY_LINE_NAME               "latency"
#define DBG_BUS_LATENCY_LINE_NUM                1

/**
 * Extra lines include a signature line + optional latency events line
 */
#define NUM_EXTRA_DBG_LINES_USER(block)                 (1 +		       \
							 (block->	       \
							  has_latency_events ? \
							  1 : 0))
#define NUM_DBG_LINES_USER(block)                       (block->		   \
							 num_of_dbg_bus_lines +	   \
							 NUM_EXTRA_DBG_LINES_USER( \
								 block))

/**
 * ******************************* Macros ***********************************
 */

#ifndef BYTES_TO_DWORDS
#define BYTES_TO_DWORDS(bytes)                  ((bytes) / sizeof(u32))
#endif

/**
 * *************************** Constant Arrays ******************************
 */

/**
 * Storm names array
 */
static const char *const s_storm_str[] = { "t", "m", "u", "x", "y", "p" };

/**
 * Storm debug mode names array
 */
static const char *const s_storm_mode_str[] = {
	/**
	 * DBG_BUS_STORM_MODE_PRINTF
	 */
	"printf",

	/**
	 * DBG_BUS_STORM_MODE_PRAM_ADDR
	 */
	"pram_addr",

	/**
	 * DBG_BUS_STORM_MODE_DRA_RW
	 */
	"dra_rw",

	/**
	 * DBG_BUS_STORM_MODE_DRA_W
	 */
	"dra_w",

	/**
	 * DBG_BUS_STORM_MODE_LD_ST_ADDR
	 */
	"ld_st_addr",

	/**
	 * DBG_BUS_STORM_MODE_DRA_FSM
	 */
	"dra_fsm",

	/**
	 * DBG_BUS_STORM_MODE_FAST_DBGMUX
	 */
	"fast_dbgmux",

	/**
	 * DBG_BUS_STORM_MODE_RH
	 */
	"rh",

	/* DBG_BUS_STORM_MODE_RH_WITH_STORE */
	"rh_with_store",

	/**
	 * DBG_BUS_STORM_MODE_FOC
	 */
	"foc",

	/**
	 * DBG_BUS_STORM_MODE_EXT_STOR
	 */
	"ext_store"
};

/**
 * Constraint operation names array
 */
static const char *const s_constraint_op_str[MAX_DBG_BUS_CONSTRAINT_OPS] = {
	/**
	 * DBG_BUS_CONSTRAINT_OP_EQ
	 */
	"eq",

	/**
	 * DBG_BUS_CONSTRAINT_OP_NE
	 */
	"ne",

	/**
	 * DBG_BUS_CONSTRAINT_OP_LT
	 */
	"lt",

	/**
	 * DBG_BUS_CONSTRAINT_OP_LTC
	 */
	"ltc",

	/**
	 * DBG_BUS_CONSTRAINT_OP_LE
	 */
	"le",

	/**
	 * DBG_BUS_CONSTRAINT_OP_LEC
	 */
	"lec",

	/**
	 * DBG_BUS_CONSTRAINT_OP_GT
	 */
	"gt",

	/**
	 * DBG_BUS_CONSTRAINT_OP_GTC
	 */
	"gtc",

	/**
	 * DBG_BUS_CONSTRAINT_OP_GE
	 */
	"ge",

	/**
	 * DBG_BUS_CONSTRAINT_OP_GEC
	 */
	"gec"
};

/**
 * Status string array
 */
static const char *const s_status_str[] = {
	/**
	 * DBG_STATUS_OK
	 */
	"Operation completed successfully",

	/**
	 * DBG_STATUS_APP_VERSION_NOT_SET
	 */
	"Debug application version wasn't set",

	/**
	 * DBG_STATUS_UNSUPPORTED_APP_VERSION
	 */
	"Unsupported debug application version",

	/**
	 * DBG_STATUS_DBG_BLOCK_NOT_RESET
	 */
	"The debug block wasn't reset since the last recording",

	/**
	 * DBG_STATUS_INVALID_ARGS
	 */
	"Invalid arguments",

	/**
	 * DBG_STATUS_OUTPUT_ALREADY_SET
	 */
	"The debug output was already set",

	/**
	 * DBG_STATUS_INVALID_PCI_BUF_SIZE
	 */
	"Invalid PCI buffer size",

	/**
	 * DBG_STATUS_PCI_BUF_ALLOC_FAILED
	 */
	"PCI buffer allocation failed",

	/**
	 * DBG_STATUS_PCI_BUF_NOT_ALLOCATED
	 */
	"A PCI buffer wasn't allocated",

	/**
	 * DBG_STATUS_INVALID_FILTER_TRIGGER_DWORDS
	 */
	"The filter/trigger constraint dword offsets are not enabled for recording",

	/**
	 * DBG_STATUS_NO_MATCHING_FRAMING_MODE
	 */
	"No matching framing mode found for the enabled blocks/Storms - use less dwords for blocks data",

	/**
	 * DBG_STATUS_VFC_READ_ERROR
	 */
	"Error reading from VFC",

	/**
	 * DBG_STATUS_STORM_ALREADY_ENABLED
	 */
	"The Storm was already enabled",

	/**
	 * DBG_STATUS_STORM_NOT_ENABLED
	 */
	"The specified Storm wasn't enabled",

	/**
	 * DBG_STATUS_BLOCK_ALREADY_ENABLED
	 */
	"The block was already enabled",

	/**
	 * DBG_STATUS_BLOCK_NOT_ENABLED
	 */
	"The specified block wasn't enabled",

	/**
	 * DBG_STATUS_NO_INPUT_ENABLED
	 */
	"No input was enabled for recording",

	/**
	 * DBG_STATUS_NO_FILTER_TRIGGER_256B
	 */
	"Filters and triggers are not allowed in E4 256-bit mode",

	/**
	 * DBG_STATUS_FILTER_ALREADY_ENABLED
	 */
	"The filter was already enabled",

	/**
	 * DBG_STATUS_TRIGGER_ALREADY_ENABLED
	 */
	"The trigger was already enabled",

	/**
	 * DBG_STATUS_TRIGGER_NOT_ENABLED
	 */
	"The trigger wasn't enabled",

	/**
	 * DBG_STATUS_CANT_ADD_CONSTRAINT
	 */
	"A constraint can be added only after a filter was enabled or a trigger state was added",

	/**
	 * DBG_STATUS_TOO_MANY_TRIGGER_STATES
	 */
	"Cannot add more than 3 trigger states",

	/**
	 * DBG_STATUS_TOO_MANY_CONSTRAINTS
	 */
	"Cannot add more than 4 constraints per filter or trigger state",

	/**
	 * DBG_STATUS_RECORDING_NOT_STARTED
	 */
	"The recording wasn't started",

	/**
	 * DBG_STATUS_DATA_DIDNT_TRIGGER
	 */
	"A trigger was configured, but it didn't trigger",

	/**
	 * DBG_STATUS_NO_DATA_RECORDED
	 */
	"No data was recorded",

	/**
	 * DBG_STATUS_DUMP_BUF_TOO_SMALL
	 */
	"Dump buffer is too small",

	/**
	 * DBG_STATUS_DUMP_NOT_CHUNK_ALIGNED
	 */
	"Dumped data is not aligned to chunks",

	/**
	 * DBG_STATUS_UNKNOWN_CHIP
	 */
	"Unknown chip",

	/**
	 * DBG_STATUS_VIRT_MEM_ALLOC_FAILED
	 */
	"Failed allocating virtual memory",

	/**
	 * DBG_STATUS_BLOCK_IN_RESET
	 */
	"The input block is in reset",

	/**
	 * DBG_STATUS_INVALID_TRACE_SIGNATURE
	 */
	"Invalid MCP trace signature found in NVRAM",

	/**
	 * DBG_STATUS_INVALID_NVRAM_BUNDLE
	 */
	"Invalid bundle ID found in NVRAM",

	/**
	 * DBG_STATUS_NVRAM_GET_IMAGE_FAILED
	 */
	"Failed getting NVRAM image",

	/**
	 * DBG_STATUS_NON_ALIGNED_NVRAM_IMAGE
	 */
	"NVRAM image is not dword-aligned",

	/**
	 * DBG_STATUS_NVRAM_READ_FAILED
	 */
	"Failed reading from NVRAM",

	/**
	 * DBG_STATUS_IDLE_CHK_PARSE_FAILED
	 */
	"Idle check parsing failed",

	/**
	 * DBG_STATUS_MCP_TRACE_BAD_DATA
	 */
	"MCP Trace data is corrupt",

	/**
	 * DBG_STATUS_MCP_TRACE_NO_META
	 */
	"Dump doesn't contain meta data - it must be provided in image file",

	/**
	 * DBG_STATUS_MCP_COULD_NOT_HALT
	 */
	"Failed to halt MCP",

	/**
	 * DBG_STATUS_MCP_COULD_NOT_RESUME
	 */
	"Failed to resume MCP after halt",

	/**
	 * DBG_STATUS_RESERVED0
	 */
	"",

	/**
	 * DBG_STATUS_SEMI_FIFO_NOT_EMPTY
	 */
	"Failed to empty SEMI sync FIFO",

	/**
	 * DBG_STATUS_IGU_FIFO_BAD_DATA
	 */
	"IGU FIFO data is corrupt",

	/**
	 * DBG_STATUS_MCP_COULD_NOT_MASK_PRTY
	 */
	"MCP failed to mask parities",

	/**
	 * DBG_STATUS_FW_ASSERTS_PARSE_FAILED
	 */
	"FW Asserts parsing failed",

	/**
	 * DBG_STATUS_REG_FIFO_BAD_DATA
	 */
	"GRC FIFO data is corrupt",

	/**
	 * DBG_STATUS_PROTECTION_OVERRIDE_BAD_DATA
	 */
	"Protection Override data is corrupt",

	/**
	 * DBG_STATUS_DBG_ARRAY_NOT_SET
	 */
	"Debug arrays were not set (when using binary files, dbg_set_bin_ptr must be called)",

	/**
	 * DBG_STATUS_RESERVED1
	 */
	"",

	/**
	 * DBG_STATUS_NON_MATCHING_LINES
	 */
	"Non-matching debug lines - in E4, all lines must be of the same type (either 128b or 256b)",

	/**
	 * DBG_STATUS_INSUFFICIENT_HW_IDS
	 */
	"Insufficient HW IDs. Try to record less Storms/blocks",

	/**
	 * DBG_STATUS_DBG_BUS_IN_USE
	 */
	"The debug bus is in use",

	/**
	 * DBG_STATUS_INVALID_STORM_DBG_MODE
	 */
	"The storm debug mode is not supported in the current chip",

	/**
	 * DBG_STATUS_OTHER_ENGINE_BB_ONLY
	 */
	"Other engine is supported only in BB",

	/**
	 * DBG_STATUS_FILTER_SINGLE_HW_ID
	 */
	"The configured filter mode requires a single Storm/block input",

	/**
	 * DBG_STATUS_TRIGGER_SINGLE_HW_ID
	 */
	"The configured filter mode requires that all the constraints of a single trigger state will be defined on a single Storm/block input",

	/**
	 * DBG_STATUS_MISSING_TRIGGER_STATE_STORM
	 */
	"When triggering on Storm data, the Storm to trigger on must be specified",

	/* DBG_STATUS_MDUMP2_FAILED_TO_REQUEST_OFFSIZE */
	"Failed to request MDUMP2 Offsize",

	/* DBG_STATUS_MDUMP2_FAILED_VALIDATION_OF_DATA_CRC */
	"Expected CRC (part of the MDUMP2 data) is different than the calculated CRC over that data",

	/* DBG_STATUS_MDUMP2_INVALID_SIGNATURE */
	"Invalid Signature found at start of MDUMP2",

	/* DBG_STATUS_MDUMP2_INVALID_LOG_SIZE */
	"Invalid Log Size of MDUMP2",

	/* DBG_STATUS_MDUMP2_INVALID_LOG_HDR */
	"Invalid Log Header of MDUMP2",

	/* DBG_STATUS_MDUMP2_INVALID_LOG_DATA */
	"Invalid Log Data of MDUMP2",

	/* DBG_STATUS_MDUMP2_ERROR_EXTRACTING_NUM_PORTS */
	"Could not extract number of ports from regval buf of MDUMP2",

	/* DBG_STATUS_MDUMP2_ERROR_EXTRACTING_MFW_STATUS */
	"Could not extract MFW (link) status from regval buf of MDUMP2",

	/* DBG_STATUS_MDUMP2_ERROR_DISPLAYING_LINKDUMP */
	"Could not display linkdump of MDUMP2",

	/* DBG_STATUS_MDUMP2_ERROR_READING_PHY_CFG */
	"Could not read PHY CFG of MDUMP2",

	/* DBG_STATUS_MDUMP2_ERROR_READING_PLL_MODE */
	"Could not read PLL Mode of MDUMP2",

	/* DBG_STATUS_MDUMP2_ERROR_READING_LANE_REGS */
	"Could not read TSCF/TSCE Lane Regs of MDUMP2",

	/* DBG_STATUS_MDUMP2_ERROR_ALLOCATING_BUF */
	"Could not allocate MDUMP2 reg-val internal buffer"
};

/**
 * GRC Dump flag names array
 */
static const char *const s_grc_param_str[] = {
	/**
	 * DBG_GRC_PARAM_DUMP_TSTORM
	 */
	"tstorm",

	/**
	 * DBG_GRC_PARAM_DUMP_MSTORM
	 */
	"mstorm",

	/**
	 * DBG_GRC_PARAM_DUMP_USTORM
	 */
	"ustorm",

	/**
	 * DBG_GRC_PARAM_DUMP_XSTORM
	 */
	"xstorm",

	/**
	 * DBG_GRC_PARAM_DUMP_YSTORM
	 */
	"ystorm",

	/**
	 * DBG_GRC_PARAM_DUMP_PSTORM
	 */
	"pstorm",

	/**
	 * DBG_GRC_PARAM_DUMP_REGS
	 */
	"regs",

	/**
	 * DBG_GRC_PARAM_DUMP_RAM
	 */
	"ram",

	/**
	 * DBG_GRC_PARAM_DUMP_PBUF
	 */
	"pbuf",

	/**
	 * DBG_GRC_PARAM_DUMP_IOR
	 */
	"ior",

	/**
	 * DBG_GRC_PARAM_DUMP_VFC
	 */
	"vfc",

	/**
	 * DBG_GRC_PARAM_DUMP_CM_CTX
	 */
	"ctx",

	/**
	 * DBG_GRC_PARAM_DUMP_PXP
	 */
	"pxp",

	/**
	 * DBG_GRC_PARAM_DUMP_RSS
	 */
	"rss",

	/**
	 * DBG_GRC_PARAM_DUMP_CAU
	 */
	"cau",

	/**
	 * DBG_GRC_PARAM_DUMP_QM
	 */
	"qm",

	/**
	 * DBG_GRC_PARAM_DUMP_MCP
	 */
	"mcp",

	/**
	 * DBG_GRC_PARAM_DORQ
	 */
	"dorq",

	/**
	 * DBG_GRC_PARAM_DUMP_CFC
	 */
	"cfc",

	/**
	 * DBG_GRC_PARAM_DUMP_IGU
	 */
	"igu",

	/**
	 * DBG_GRC_PARAM_DUMP_BRB
	 */
	"brb",

	/**
	 * DBG_GRC_PARAM_DUMP_BTB
	 */
	"btb",

	/**
	 * DBG_GRC_PARAM_DUMP_BMB
	 */
	"bmb",

	/* DBG_GRC_PARAM_RESERVED1 */
	"reserved1",

	/**
	 * DBG_GRC_PARAM_DUMP_MULD
	 */
	"muld",

	/**
	 * DBG_GRC_PARAM_DUMP_PRS
	 */
	"prs",

	/**
	 * DBG_GRC_PARAM_DUMP_DMAE
	 */
	"dmae",

	/**
	 * DBG_GRC_PARAM_DUMP_TM
	 */
	"tm",

	/**
	 * DBG_GRC_PARAM_DUMP_SDM
	 */
	"sdm",

	/**
	 * DBG_GRC_PARAM_DUMP_DIF
	 */
	"dif",

	/**
	 * DBG_GRC_PARAM_DUMP_STATIC
	 */
	"static",

	/**
	 * DBG_GRC_PARAM_UNSTALL
	 */
	"unstall",

	/* DBG_GRC_PARAM_DUMP_SEM */
	"sem",

	/**
	 * DBG_GRC_PARAM_MCP_TRACE_META_SIZE
	 */
	"trace_meta_size",

	/**
	 * DBG_GRC_PARAM_EXCLUDE_ALL
	 */
	"exclude_all",

	/**
	 * DBG_GRC_PARAM_CRASH
	 */
	"crash",

	/**
	 * DBG_GRC_PARAM_PARITY_SAFE
	 */
	"parity_safe",

	/**
	 * DBG_GRC_PARAM_DUMP_CM
	 */
	"cm",

	/**
	 * DBG_GRC_PARAM_DUMP_PHY
	 */
	"phy",

	/**
	 * DBG_GRC_PARAM_NO_MCP
	 */
	"nomcp",

	/**
	 * DBG_GRC_PARAM_NO_FW_VER
	 */
	"nofwver",

	/* DBG_GRC_PARAM_RESERVED3 */
	"reserved3",

	/**
	 * DBG_GRC_PARAM_DUMP_MCP_HW_DUMP
	 */
	"mcp_hw_dump",

	/**
	 * DBG_GRC_PARAM_DUMP_ILT_CDUC
	 */
	"cduc",

	/**
	 * DBG_GRC_PARAM_DUMP_ILT_CDUT
	 */
	"cdut",

	/* DBG_GRC_PARAM_DUMP_CAU_EXT */
	"cau_ext"
};

/**
 * Idle check severity names array
 */
static const char *const s_idle_chk_severity_str[] = {
	"Error",
	"Error if no traffic",
	"Warning"
};

/**
 * MCP Trace level names array
 */
static const char *const s_mcp_trace_level_str[] = {
	"ERROR",
	"TRACE",
	"DEBUG"
};

/**
 * Access type names array
 */
static const char *const s_access_strs[] = {
	"read",
	"write"
};

/**
 * Privilege type names array
 */
static const char *const s_privilege_strs[] = {
	"VF",
	"PDA",
	"HV",
	"UA"
};

/**
 * Protection type names array
 */
static const char *const s_protection_strs[] = {
	"(default)",
	"(default)",
	"(default)",
	"(default)",
	"override VF",
	"override PDA",
	"override HV",
	"override UA"
};

/**
 * Master type names array
 */
static const char *const s_master_strs[] = {
	"???",
	"pxp",
	"mcp",
	"msdm",
	"psdm",
	"ysdm",
	"usdm",
	"tsdm",
	"xsdm",
	"dbu",
	"dmae",
	"jdap",
	"???",
	"???",
	"???",
	"???"
};

/**
 * REG FIFO error messages array
 */
static struct reg_fifo_err s_reg_fifo_errors[] = {
	{1, "grc timeout"},
	{2, "address doesn't belong to any block"},
	{4, "reserved address in block or write to read-only address"},
	{8, "privilege/protection mismatch"},
	{16, "path isolation error"},
	{17, "RSL error"}
};

/**
 * IGU FIFO sources array
 */
static const char *const s_igu_fifo_source_strs[] = {
	"TSTORM",
	"MSTORM",
	"USTORM",
	"XSTORM",
	"YSTORM",
	"PSTORM",
	"PCIE",
	"NIG_QM_PBF",
	"CAU",
	"ATTN",
	"GRC",
};

/* IGU FIFO error messages */
static const char *s_igu_fifo_error_strs[] = {
	"no error",
	"length error",
	"function disabled",
	"VF sent command to attention address",
	"host sent prod update command",
	"read of during interrupt register while in MIMD mode",
	"access to PXP BAR reserved address",
	"producer update command to attention index",
	"unknown error",
	"SB index not valid",
	"SB relative index and FID not found",
	"FID not match",
	"command with error flag asserted (PCI error or CAU discard)",
	"VF sent cleanup and RF cleanup is disabled",
	"cleanup command on type bigger than 4"
};

/**
 * IGU FIFO address data
 */
static const struct igu_fifo_addr_data s_igu_fifo_addr_data[] = {
	{0x0, 0x101, "MSI-X Memory", NULL,
	 IGU_ADDR_TYPE_MSIX_MEM},
	{0x102, 0x1ff, "reserved", NULL,
	 IGU_ADDR_TYPE_RESERVED},
	{0x200, 0x200, "Write PBA[0:63]", NULL,
	 IGU_ADDR_TYPE_WRITE_PBA},
	{0x201, 0x201, "Write PBA[64:127]", "reserved",
	 IGU_ADDR_TYPE_WRITE_PBA},
	{0x202, 0x202, "Write PBA[128]", "reserved",
	 IGU_ADDR_TYPE_WRITE_PBA},
	{0x203, 0x3ff, "reserved", NULL,
	 IGU_ADDR_TYPE_RESERVED},
	{0x400, 0x5ef, "Write interrupt acknowledgment", NULL,
	 IGU_ADDR_TYPE_WRITE_INT_ACK},
	{0x5f0, 0x5f0, "Attention bits update", NULL,
	 IGU_ADDR_TYPE_WRITE_ATTN_BITS},
	{0x5f1, 0x5f1, "Attention bits set", NULL,
	 IGU_ADDR_TYPE_WRITE_ATTN_BITS},
	{0x5f2, 0x5f2, "Attention bits clear", NULL,
	 IGU_ADDR_TYPE_WRITE_ATTN_BITS},
	{0x5f3, 0x5f3, "Read interrupt 0:63 with mask", NULL,
	 IGU_ADDR_TYPE_READ_INT},
	{0x5f4, 0x5f4, "Read interrupt 0:31 with mask", NULL,
	 IGU_ADDR_TYPE_READ_INT},
	{0x5f5, 0x5f5, "Read interrupt 32:63 with mask", NULL,
	 IGU_ADDR_TYPE_READ_INT},
	{0x5f6, 0x5f6, "Read interrupt 0:63 without mask", NULL,
	 IGU_ADDR_TYPE_READ_INT},
	{0x5f7, 0x5ff, "reserved", NULL,
	 IGU_ADDR_TYPE_RESERVED},
	{0x600, 0x7ff, "Producer update", NULL,
	 IGU_ADDR_TYPE_WRITE_PROD_UPDATE}
};

/*
 * ******************************* Variables *********************************
 */

/**
 * Temporary buffer, used for print size calculations
 */
static char s_temp_buf[MAX_MSG_LEN];

/*
 * ************************** Private Functions *****************************
 */
static void qed_user_static_asserts(void)
{
}

/**
 * Returns true if the specified string is a number, false otherwise.
 */
static bool qed_is_number(const char *str)
{
	size_t i = 0, len = strlen(str);

	for (i = 0; i < len; i++)
		if (str[i] < '0' || str[i] > '9')
			return false;

	return true;
}

static u32 qed_cyclic_add(u32 a, u32 b, u32 size)
{
	return (a + b) % size;
}

static u32 qed_cyclic_sub(u32 a, u32 b, u32 size)
{
	return (size + a - b) % size;
}

/**
 * Reads the specified number of bytes from the specified cyclic buffer (up to
 * 4 bytes) and returns them as a dword value. the specified buffer offset is
 * updated.
 */
static u32 qed_read_from_cyclic_buf(void *buf,
				    u32 * offset,
				    u32 buf_size, u8 num_bytes_to_read)
{
	u8 i, *val_ptr, *bytes_buf = (u8 *) buf;
	u32 val = 0;

	val_ptr = (u8 *) & val;

	/**
	 * Assume running on a LITTLE ENDIAN and the buffer is network order
	 * (BIG ENDIAN), as high order bytes are placed in lower memory address.
	 */
	for (i = 0; i < num_bytes_to_read; i++) {
		val_ptr[i] = bytes_buf[*offset];
		*offset = qed_cyclic_add(*offset, 1, buf_size);
	}

	return val;
}

/**
 * Reads and returns the next byte from the specified buffer. the specified
 * buffer offset is updated.
 */
static u8 qed_read_byte_from_buf(void *buf, u32 * offset)
{
	return ((u8 *) buf)[(*offset)++];
}

/**
 * Reads and returns the next dword from the specified buffer. the specified
 * buffer offset is updated.
 */
static u32 qed_read_dword_from_buf(void *buf, u32 * offset)
{
	u32 dword_val = *(u32 *) & ((u8 *) buf)[*offset];

	*offset += 4;

	return dword_val;
}

/**
 * Reads the next string from the specified buffer, and copies it to the
 * specified pointer the specified buffer offset is updated.
 */
static void qed_read_str_from_buf(void *buf, u32 * offset, u32 size, char *dest)
{
	const char *source_str = &((const char *)buf)[*offset];

	strncpy(dest, source_str, size);
	dest[size - 1] = '\0';
	*offset += size;
}

/**
 * Returns a pointer to the specified offset (in bytes) of the specified
 * buffer. if the specified buffer in NULL, a temporary buffer pointer is
 * returned.
 */
static char *qed_get_buf_ptr(void *buf, u32 offset)
{
	return buf ? (char *)buf + offset : s_temp_buf;
}

/**
 * Returns a remind size of the specified buffer.
 * if the specified buffer in NULL, a temporary buffer size returned.
 */
static u32 qed_get_buf_size(struct qed_hwfn *p_hwfn,
			    void *buf, u32 buf_size, u32 offset)
{
	if (buf != NULL) {
		if (buf_size < offset) {
			DP_NOTICE(p_hwfn,
				  "Unexpected debug error : dump buffer offset overrun\n");
			return 0;
		} else {
			return buf_size - offset;
		}
	} else {
		return MAX_MSG_LEN;
	}
}

/**
 * Returns updated buffer offset.
 */
static u32 qed_buf_offset_update(struct qed_hwfn *p_hwfn,
				 int print_size, u32 offset)
{
	if (print_size < 0) {
		DP_NOTICE(p_hwfn,
			  "Unexpected debug error : dump buffer overrun\n");
		return offset;
	}

	return offset + print_size;
}

/**
 * Reads a param from the specified buffer. returns the number of dwords read.
 * if the returned str_param is NULL, the param is numeric and its value is
 * returned in num_param. otherwise, the param is a string and its pointer is
 * returned in str_param.
 */
static u32 qed_read_param(u32 * dump_buf,
			  const char **param_name,
			  const char **param_str_val, u32 * param_num_val)
{
	char *char_buf = (char *)dump_buf;
	size_t offset = 0;

	/**
	 * Extract param name
	 */
	*param_name = char_buf;
	offset += strlen(*param_name) + 1;

	/**
	 * Check param type
	 */
	if (*(char_buf + offset++)) {
		/**
		 * String param
		 */
		*param_str_val = char_buf + offset;
		*param_num_val = 0;
		offset += strlen(*param_str_val) + 1;
		if (offset & 0x3)
			offset += (4 - (offset & 0x3));
	} else {
		/**
		 * Numeric param
		 */
		*param_str_val = NULL;
		if (offset & 0x3)
			offset += (4 - (offset & 0x3));
		*param_num_val = *(u32 *) (char_buf + offset);
		offset += 4;
	}

	return (u32) offset / 4;
}

/**
 * Reads a section header from the specified buffer. Returns the number of
 * dwords read.
 */
static u32 qed_read_section_hdr(u32 * dump_buf,
				const char **section_name,
				u32 * num_section_params)
{
	const char *param_str_val;

	return qed_read_param(dump_buf,
			      section_name, &param_str_val, num_section_params);
}

/**
 * Reads section params from the specified buffer and prints them to the
 * results buffer. Returns the number of dwords read.
 */
static u32 qed_print_section_params(struct qed_hwfn *p_hwfn,
				    u32 * dump_buf,
				    u32 num_section_params,
				    char *results_buf,
				    u32 results_buf_size,
				    u32 * num_chars_printed)
{
	u32 i, dump_offset = 0, results_offset = 0;
	int print_size;

	for (i = 0; i < num_section_params; i++) {
		const char *param_name, *param_str_val;
		u32 param_num_val = 0;

		dump_offset += qed_read_param(dump_buf + dump_offset,
					      &param_name,
					      &param_str_val, &param_num_val);

		if (param_str_val)
			print_size =
			    scnprintf(qed_get_buf_ptr(results_buf,
						      results_offset),
				      qed_get_buf_size(p_hwfn,
						       results_buf,
						       results_buf_size,
						       results_offset),
				      "%s: %s\n", param_name, param_str_val);
		else if (strcmp(param_name, "fw-timestamp"))
			print_size =
			    scnprintf(qed_get_buf_ptr(results_buf,
						      results_offset),
				      qed_get_buf_size(p_hwfn,
						       results_buf,
						       results_buf_size,
						       results_offset),
				      "%s: %d\n", param_name, param_num_val);
		else
			print_size = 0;

		results_offset = qed_buf_offset_update(p_hwfn,
						       print_size,
						       results_offset);
	}

	print_size = scnprintf(qed_get_buf_ptr(results_buf,
					       results_offset),
			       qed_get_buf_size(p_hwfn,
						results_buf,
						results_buf_size,
						results_offset), "\n");
	results_offset = qed_buf_offset_update(p_hwfn,
					       print_size, results_offset);

	*num_chars_printed = results_offset;

	return dump_offset;
}

/**
 * Returns the block name that matches the specified block ID,
 * or NULL if not found.
 */
static const char *qed_dbg_get_block_name(struct qed_hwfn *p_hwfn,
					  enum block_id block_id)
{
	const struct dbg_block_user *block =
	    (const struct dbg_block_user *)p_hwfn->
	    dbg_arrays[BIN_BUF_DBG_BLOCKS_USER_DATA].ptr + block_id;

	return (const char *)block->name;
}

static struct dbg_tools_user_data *qed_dbg_get_user_data(struct qed_hwfn
							 *p_hwfn)
{
	return (struct dbg_tools_user_data *)p_hwfn->dbg_user_info;
}

/**
 * Parses the idle check rules and returns the number of characters printed.
 * in case of parsing error, returns 0.
 */
static u32 qed_parse_idle_chk_dump_rules(struct qed_hwfn *p_hwfn,
					 u32 * dump_buf,
					 u32 * dump_buf_end,
					 u32 num_rules,
					 bool
					 print_fw_idle_chk,
					 char *results_buf,
					 u32
					 results_buf_size,
					 u32 * num_errors, u32 * num_warnings)
{
	/**
	 * Offset in results_buf in bytes
	 */
	u32 results_offset = 0;

	u32 rule_idx;
	u16 i, j;

	*num_errors = 0;
	*num_warnings = 0;

	/**
	 * Go over dumped results
	 */
	for (rule_idx = 0; rule_idx < num_rules && dump_buf < dump_buf_end;
	     rule_idx++) {
		const struct dbg_idle_chk_rule_parsing_data *rule_parsing_data;
		struct dbg_idle_chk_result_hdr *hdr;
		const char *parsing_str, *lsi_msg;
		u32 parsing_str_offset;
		bool has_fw_msg;
		u8 curr_reg_id;
		int print_size;

		hdr = (struct dbg_idle_chk_result_hdr *)dump_buf;
		rule_parsing_data =
		    (const struct dbg_idle_chk_rule_parsing_data *)
		    p_hwfn->dbg_arrays[BIN_BUF_DBG_IDLE_CHK_PARSING_DATA].ptr +
		    hdr->rule_id;
		parsing_str_offset =
		    GET_FIELD(rule_parsing_data->data,
			      DBG_IDLE_CHK_RULE_PARSING_DATA_STR_OFFSET);
		has_fw_msg =
		    GET_FIELD(rule_parsing_data->data,
			      DBG_IDLE_CHK_RULE_PARSING_DATA_HAS_FW_MSG)
		    > 0;
		parsing_str =
		    (const char *)p_hwfn->
		    dbg_arrays[BIN_BUF_DBG_PARSING_STRINGS].ptr +
		    parsing_str_offset;
		lsi_msg = parsing_str;
		curr_reg_id = 0;

		if (hdr->severity >= MAX_DBG_IDLE_CHK_SEVERITY_TYPES)
			return 0;

		/**
		 * Skip rule header
		 */
		dump_buf += BYTES_TO_DWORDS(sizeof(*hdr));

		/**
		 * Update errors/warnings count
		 */
		if (hdr->severity == IDLE_CHK_SEVERITY_ERROR ||
		    hdr->severity == IDLE_CHK_SEVERITY_ERROR_NO_TRAFFIC)
			(*num_errors)++;
		else
			(*num_warnings)++;

		/**
		 * Print rule severity:
		 */
		print_size =
		    scnprintf(qed_get_buf_ptr(results_buf,
					      results_offset),
			      qed_get_buf_size(p_hwfn,
					       results_buf,
					       results_buf_size,
					       results_offset), "%s: ",
			      s_idle_chk_severity_str[hdr->severity]);
		results_offset = qed_buf_offset_update(p_hwfn,
						       print_size,
						       results_offset);

		/**
		 * Print rule message
		 */
		if (has_fw_msg)
			parsing_str += strlen(parsing_str) + 1;
		print_size =
		    scnprintf(qed_get_buf_ptr(results_buf,
					      results_offset),
			      qed_get_buf_size(p_hwfn,
					       results_buf,
					       results_buf_size,
					       results_offset), "%s.",
			      has_fw_msg &&
			      print_fw_idle_chk ? parsing_str : lsi_msg);
		results_offset = qed_buf_offset_update(p_hwfn,
						       print_size,
						       results_offset);
		parsing_str += strlen(parsing_str) + 1;

		/**
		 * Print register values
		 */
		print_size =
		    scnprintf(qed_get_buf_ptr(results_buf,
					      results_offset),
			      qed_get_buf_size(p_hwfn,
					       results_buf,
					       results_buf_size,
					       results_offset), " Registers:");
		results_offset = qed_buf_offset_update(p_hwfn,
						       print_size,
						       results_offset);
		for (i = 0;
		     i < hdr->num_dumped_cond_regs + hdr->num_dumped_info_regs;
		     i++) {
			struct dbg_idle_chk_result_reg_hdr *reg_hdr;
			bool is_mem;
			u8 reg_id;

			reg_hdr =
			    (struct dbg_idle_chk_result_reg_hdr *)dump_buf;
			is_mem = GET_FIELD(reg_hdr->data,
					   DBG_IDLE_CHK_RESULT_REG_HDR_IS_MEM);
			reg_id = GET_FIELD(reg_hdr->data,
					   DBG_IDLE_CHK_RESULT_REG_HDR_REG_ID);

			/**
			 * Skip reg header
			 */
			dump_buf += BYTES_TO_DWORDS(sizeof(*reg_hdr));

			/**
			 * Skip register names until the required reg_id is
			 * reached.
			 */
			for (; reg_id > curr_reg_id;
			     curr_reg_id++, parsing_str +=
			     strlen(parsing_str) + 1) ;

			print_size =
			    scnprintf(qed_get_buf_ptr(results_buf,
						      results_offset),
				      qed_get_buf_size(p_hwfn,
						       results_buf,
						       results_buf_size,
						       results_offset),
				      " %s", parsing_str);
			results_offset = qed_buf_offset_update(p_hwfn,
							       print_size,
							       results_offset);
			if (i < hdr->num_dumped_cond_regs && is_mem) {
				print_size =
				    scnprintf(qed_get_buf_ptr(results_buf,
							      results_offset),
					      qed_get_buf_size(p_hwfn,
							       results_buf,
							       results_buf_size,
							       results_offset),
					      "[%d]", hdr->mem_entry_id +
					      reg_hdr->start_entry);
				results_offset = qed_buf_offset_update(p_hwfn,
								       print_size,
								       results_offset);
			}
			print_size =
			    scnprintf(qed_get_buf_ptr(results_buf,
						      results_offset),
				      qed_get_buf_size(p_hwfn, results_buf,
						       results_buf_size,
						       results_offset), "=");
			results_offset = qed_buf_offset_update(p_hwfn,
							       print_size,
							       results_offset);
			for (j = 0; j < reg_hdr->size; j++, dump_buf++) {
				print_size =
				    scnprintf(qed_get_buf_ptr(results_buf,
							      results_offset),
					      qed_get_buf_size(p_hwfn,
							       results_buf,
							       results_buf_size,
							       results_offset),
					      "0x%x", *dump_buf);
				results_offset = qed_buf_offset_update(p_hwfn,
								       print_size,
								       results_offset);
				if (j < reg_hdr->size - 1) {
					print_size =
					    scnprintf(qed_get_buf_ptr
						      (results_buf,
						       results_offset),
						      qed_get_buf_size(p_hwfn,
								       results_buf,
								       results_buf_size,
								       results_offset),
						      ",");
					results_offset =
					    qed_buf_offset_update(p_hwfn,
								  print_size,
								  results_offset);
				}
			}
		}

		print_size =
		    scnprintf(qed_get_buf_ptr(results_buf,
					      results_offset),
			      qed_get_buf_size(p_hwfn, results_buf,
					       results_buf_size,
					       results_offset), "\n");
		results_offset = qed_buf_offset_update(p_hwfn,
						       print_size,
						       results_offset);
	}

	/**
	 * Check if end of dump buffer was exceeded
	 */
	if (dump_buf > dump_buf_end)
		return 0;

	return results_offset;
}

/**
 * Parses an idle check dump buffer. If result_buf is not NULL, the idle check
 * results are printed to it. In any case, the required results buffer size is
 * assigned to parsed_results_bytes. The parsing status is returned.
 */
static enum dbg_status qed_parse_idle_chk_dump(struct qed_hwfn *p_hwfn,
					       u32 * dump_buf,
					       u32
					       num_dumped_dwords,
					       char *results_buf,
					       u32
					       results_buf_size,
					       u32 *
					       parsed_results_bytes,
					       u32 * num_errors,
					       u32 * num_warnings)
{
	const char *section_name, *param_name, *param_str_val;
	u32 *dump_buf_end = dump_buf + num_dumped_dwords;
	u32 num_section_params = 0, num_rules, num_rules_not_dumped;
	int print_size;

	/**
	 * Offset in results_buf in bytes
	 */
	u32 results_offset = 0;

	*parsed_results_bytes = 0;
	*num_errors = 0;
	*num_warnings = 0;

	if (!p_hwfn->dbg_arrays[BIN_BUF_DBG_PARSING_STRINGS].ptr ||
	    !p_hwfn->dbg_arrays[BIN_BUF_DBG_IDLE_CHK_PARSING_DATA].ptr)
		return DBG_STATUS_DBG_ARRAY_NOT_SET;

	/**
	 * Read global_params section
	 */
	dump_buf += qed_read_section_hdr(dump_buf,
					 &section_name, &num_section_params);
	if (strcmp(section_name, "global_params"))
		return DBG_STATUS_IDLE_CHK_PARSE_FAILED;

	/**
	 * Print global params
	 */
	dump_buf += qed_print_section_params(p_hwfn,
					     dump_buf,
					     num_section_params,
					     results_buf,
					     results_buf_size, &results_offset);

	/**
	 * Read idle_chk section
	 * There may be 1 or 2 idle_chk section parameters:
	 * - 1st is "num_rules"
	 * - 2nd is "num_rules_not_dumped" (optional)
	 */
	dump_buf += qed_read_section_hdr(dump_buf,
					 &section_name, &num_section_params);
	if (strcmp(section_name, "idle_chk") ||
	    (num_section_params != 2 && num_section_params != 1))
		return DBG_STATUS_IDLE_CHK_PARSE_FAILED;
	dump_buf += qed_read_param(dump_buf,
				   &param_name, &param_str_val, &num_rules);
	if (strcmp(param_name, "num_rules"))
		return DBG_STATUS_IDLE_CHK_PARSE_FAILED;
	if (num_section_params > 1) {
		dump_buf += qed_read_param(dump_buf,
					   &param_name,
					   &param_str_val,
					   &num_rules_not_dumped);
		if (strcmp(param_name, "num_rules_not_dumped"))
			return DBG_STATUS_IDLE_CHK_PARSE_FAILED;
	} else {
		num_rules_not_dumped = 0;
	}

	if (num_rules) {
		u32 rules_print_size;

		/**
		 * Print FW output:
		 */
		print_size =
		    scnprintf(qed_get_buf_ptr(results_buf,
					      results_offset),
			      qed_get_buf_size(p_hwfn,
					       results_buf,
					       results_buf_size,
					       results_offset),
			      "FW_IDLE_CHECK:\n");
		results_offset = qed_buf_offset_update(p_hwfn,
						       print_size,
						       results_offset);

		rules_print_size = qed_parse_idle_chk_dump_rules(p_hwfn,
								 dump_buf,
								 dump_buf_end,
								 num_rules,
								 true,
								 results_buf
								 ? results_buf +
								 results_offset
								 : NULL,
								 results_buf ?
								 results_buf_size
								 -
								 results_offset
								 : 0,
								 num_errors,
								 num_warnings);
		results_offset += rules_print_size;
		if (!rules_print_size)
			return DBG_STATUS_IDLE_CHK_PARSE_FAILED;
	}

	/**
	 * Print errors/warnings count
	 */
	if (*num_errors)
		print_size =
		    scnprintf(qed_get_buf_ptr(results_buf,
					      results_offset),
			      qed_get_buf_size(p_hwfn,
					       results_buf,
					       results_buf_size,
					       results_offset),
			      "\nIdle Check failed!!! (with %d errors and %d warnings)\n",
			      *num_errors, *num_warnings);
	else if (*num_warnings)
		print_size =
		    scnprintf(qed_get_buf_ptr(results_buf,
					      results_offset),
			      qed_get_buf_size(p_hwfn,
					       results_buf,
					       results_buf_size,
					       results_offset),
			      "\nIdle Check completed successfully (with %d warnings)\n",
			      *num_warnings);
	else
		print_size =
		    scnprintf(qed_get_buf_ptr(results_buf,
					      results_offset),
			      qed_get_buf_size(p_hwfn,
					       results_buf,
					       results_buf_size,
					       results_offset),
			      "\nIdle Check completed successfully\n");

	results_offset = qed_buf_offset_update(p_hwfn,
					       print_size, results_offset);

	if (num_rules_not_dumped) {
		print_size =
		    scnprintf(qed_get_buf_ptr(results_buf,
					      results_offset),
			      qed_get_buf_size(p_hwfn,
					       results_buf,
					       results_buf_size,
					       results_offset),
			      "\nIdle Check Partially dumped : num_rules_not_dumped = %d\n",
			      num_rules_not_dumped);
		results_offset = qed_buf_offset_update(p_hwfn,
						       print_size,
						       results_offset);
	}

	/* Add 1 for string NULL termination */
	*parsed_results_bytes = results_offset + 1;

	return DBG_STATUS_OK;
}

/**
 * Allocates and fills MCP Trace meta data based on the specified meta data
 * dump buffer. returns debug status code.
 */
static enum dbg_status qed_mcp_trace_alloc_meta_data(struct qed_hwfn *p_hwfn,
						     const u32 * meta_buf)
{
	struct dbg_tools_user_data *dev_user_data =
	    qed_dbg_get_user_data(p_hwfn);
	struct mcp_trace_meta *meta = &dev_user_data->mcp_trace_meta;
	u8 *meta_buf_bytes = (u8 *) meta_buf;
	u32 offset = 0, signature, i;

	/**
	 * Free the previous meta before loading a new one.
	 */
	if (meta->is_allocated)
		qed_mcp_trace_free_meta_data(p_hwfn);

	memset(meta, 0, sizeof(*meta));

	/**
	 * Read first signature
	 */
	signature = qed_read_dword_from_buf(meta_buf_bytes, &offset);
	if (signature != NVM_MAGIC_VALUE)
		return DBG_STATUS_INVALID_TRACE_SIGNATURE;

	/**
	 * Read no. of modules and allocate memory for their pointers
	 */
	meta->modules_num = qed_read_byte_from_buf(meta_buf_bytes, &offset);
	meta->modules =
	    (char **)kzalloc(meta->modules_num * sizeof(char *), GFP_KERNEL);
	if (!meta->modules)
		return DBG_STATUS_VIRT_MEM_ALLOC_FAILED;

	/**
	 * Allocate and read all module strings
	 */
	for (i = 0; i < meta->modules_num; i++) {
		u8 module_len = qed_read_byte_from_buf(meta_buf_bytes, &offset);

		*(meta->modules + i) = (char *)kzalloc(module_len, GFP_KERNEL);
		if (!(*(meta->modules + i))) {
			/**
			 * Update number of modules to be released
			 */
			meta->modules_num = i ? i - 1 : 0;
			return DBG_STATUS_VIRT_MEM_ALLOC_FAILED;
		}

		qed_read_str_from_buf(meta_buf_bytes, &offset, module_len,
				      *(meta->modules + i));
		if (module_len > MCP_TRACE_MAX_MODULE_LEN)
			(*(meta->modules + i))[MCP_TRACE_MAX_MODULE_LEN] = '\0';
	}

	/**
	 * Read second signature
	 */
	signature = qed_read_dword_from_buf(meta_buf_bytes, &offset);
	if (signature != NVM_MAGIC_VALUE)
		return DBG_STATUS_INVALID_TRACE_SIGNATURE;

	/**
	 * Read number of formats and allocate memory for all formats
	 */
	meta->formats_num = qed_read_dword_from_buf(meta_buf_bytes, &offset);
	meta->formats =
	    (struct mcp_trace_format *)kzalloc(meta->formats_num *
					       sizeof(struct mcp_trace_format),
					       GFP_KERNEL);
	if (!meta->formats)
		return DBG_STATUS_VIRT_MEM_ALLOC_FAILED;

	/**
	 * Allocate and read all Format strings
	 */
	for (i = 0; i < meta->formats_num; i++) {
		struct mcp_trace_format *format_ptr = &meta->formats[i];
		u8 format_len;

		format_ptr->data = qed_read_dword_from_buf(meta_buf_bytes,
							   &offset);
		format_len = GET_MFW_FIELD(format_ptr->data,
					   MCP_TRACE_FORMAT_LEN);
		format_ptr->format_str = (char *)kzalloc(format_len,
							 GFP_KERNEL);
		if (!format_ptr->format_str) {
			/**
			 * Update number of modules to be released
			 */
			meta->formats_num = i ? i - 1 : 0;
			return DBG_STATUS_VIRT_MEM_ALLOC_FAILED;
		}

		qed_read_str_from_buf(meta_buf_bytes,
				      &offset,
				      format_len, format_ptr->format_str);
	}

	meta->is_allocated = true;

	return DBG_STATUS_OK;
}

/**
 * Parses an MCP trace buffer. If result_buf is not NULL, the MCP Trace results
 * are printed to it. The parsing status is returned.
 * Arguments:
 * trace_buf -	          MCP trace cyclic buffer
 * trace_buf_size -       MCP trace cyclic buffer size in bytes
 * data_offset -          offset in bytes of the data to parse in the MCP trace cyclic
 *		          buffer.
 * data_size -	          size in bytes of data to parse.
 * parsed_buf -	          destination buffer for parsed data.
 * parsed_results_bytes - size of parsed data in bytes.
 */
static enum dbg_status qed_parse_mcp_trace_buf(struct qed_hwfn *p_hwfn,
					       u8 * trace_buf,
					       u32 trace_buf_size,
					       u32 data_offset,
					       u32 data_size,
					       char *parsed_buf,
					       u32 parsed_buf_size,
					       u32 * parsed_results_bytes)
{
	struct dbg_tools_user_data *dev_user_data =
	    qed_dbg_get_user_data(p_hwfn);
	struct mcp_trace_meta *meta = &dev_user_data->mcp_trace_meta;
	u32 param_mask, param_shift;
	enum dbg_status status;
	int print_size;

	*parsed_results_bytes = 0;

	if (!meta->is_allocated)
		return DBG_STATUS_MCP_TRACE_BAD_DATA;

	status = DBG_STATUS_OK;

	while (data_size) {
		u32 params[3] = { 0, 0, 0 };
		struct mcp_trace_format *format_ptr;
		u32 header, format_idx, i;
		u8 format_level, format_module;

		if (data_size < MFW_TRACE_ENTRY_SIZE)
			return DBG_STATUS_MCP_TRACE_BAD_DATA;

		header = qed_read_from_cyclic_buf(trace_buf,
						  &data_offset,
						  trace_buf_size,
						  MFW_TRACE_ENTRY_SIZE);
		data_size -= MFW_TRACE_ENTRY_SIZE;
		format_idx = header & MFW_TRACE_EVENTID_MASK;

		/**
		 * Skip message if its Format index doesn't exist in the meta
		 * data.
		 */
		if (format_idx >= meta->formats_num) {
			u8 format_size = (u8) GET_MFW_FIELD(header,
							    MFW_TRACE_PRM_SIZE);

			if (data_size < format_size)
				return DBG_STATUS_MCP_TRACE_BAD_DATA;

			data_offset = qed_cyclic_add(data_offset,
						     format_size,
						     trace_buf_size);
			data_size -= format_size;
			continue;
		}

		format_ptr = &meta->formats[format_idx];

		for (i = 0,
		     param_mask = MCP_TRACE_FORMAT_P1_SIZE_MASK, param_shift =
		     MCP_TRACE_FORMAT_P1_SIZE_OFFSET;
		     i < MCP_TRACE_FORMAT_MAX_PARAMS;
		     i++, param_mask <<= MCP_TRACE_FORMAT_PARAM_WIDTH,
		     param_shift += MCP_TRACE_FORMAT_PARAM_WIDTH) {
			/* Extract param size (0..3) */
			u8 param_size =
			    (u8) ((format_ptr->data &
				   param_mask) >> param_shift);

			/**
			 * If the param size is zero, there are no other parameters.
			 */
			if (!param_size)
				break;

			/**
			 * Size is encoded using 2 bits, where 3 is used to encode 4.
			 */
			if (param_size == 3)
				param_size = 4;

			if (data_size < param_size)
				return DBG_STATUS_MCP_TRACE_BAD_DATA;

			params[i] = qed_read_from_cyclic_buf(trace_buf,
							     &data_offset,
							     trace_buf_size,
							     param_size);
			data_size -= param_size;
		}

		format_level = (u8) GET_MFW_FIELD(format_ptr->data,
						  MCP_TRACE_FORMAT_LEVEL);
		format_module = (u8) GET_MFW_FIELD(format_ptr->data,
						   MCP_TRACE_FORMAT_MODULE);
		if (format_level >= ARRAY_SIZE(s_mcp_trace_level_str))
			return DBG_STATUS_MCP_TRACE_BAD_DATA;

		/**
		 * Print current message to results buffer
		 */
		print_size =
		    scnprintf(qed_get_buf_ptr(parsed_buf,
					      *parsed_results_bytes),
			      qed_get_buf_size(p_hwfn,
					       parsed_buf,
					       parsed_buf_size,
					       *parsed_results_bytes),
			      "%s %-8s: ",
			      s_mcp_trace_level_str[format_level],
			      meta->modules[format_module]);
		*parsed_results_bytes = qed_buf_offset_update(p_hwfn,
							      print_size,
							      *parsed_results_bytes);
		print_size =
		    scnprintf(qed_get_buf_ptr(parsed_buf,
					      *parsed_results_bytes),
			      qed_get_buf_size(p_hwfn,
					       parsed_buf,
					       parsed_buf_size,
					       *parsed_results_bytes),
			      format_ptr->format_str, params[0],
			      params[1], params[2]);
		*parsed_results_bytes = qed_buf_offset_update(p_hwfn,
							      print_size,
							      *parsed_results_bytes);
	}

	/**
	 * Add string NULL terminator
	 */
	(*parsed_results_bytes)++;

	return status;
}

/**
 * Parses an MCP Trace dump buffer. If result_buf is not NULL, the MCP Trace
 * results are printed to it. In any case, the required results buffer size is
 * assigned to parsed_results_bytes. The parsing status is returned.
 */
static enum dbg_status qed_parse_mcp_trace_dump(struct qed_hwfn *p_hwfn,
						u32 * dump_buf,
						char *results_buf,
						u32
						results_buf_size,
						u32 *
						parsed_results_bytes,
						bool free_meta_data)
{
	const char *section_name, *param_name, *param_str_val;
	u32 data_size, trace_data_dwords, trace_meta_dwords;
	u32 offset, results_offset, results_buf_bytes;
	u32 param_num_val, num_section_params;
	struct mcp_trace *trace;
	enum dbg_status status;
	const u32 *meta_buf;
	u8 *trace_buf;

	*parsed_results_bytes = 0;

	/**
	 * Read global_params section
	 */
	dump_buf += qed_read_section_hdr(dump_buf,
					 &section_name, &num_section_params);
	if (strcmp(section_name, "global_params"))
		return DBG_STATUS_MCP_TRACE_BAD_DATA;

	/**
	 * Print global params
	 */
	dump_buf += qed_print_section_params(p_hwfn,
					     dump_buf,
					     num_section_params,
					     results_buf,
					     results_buf_size, &results_offset);

	/**
	 * Read trace_data section
	 */
	dump_buf += qed_read_section_hdr(dump_buf,
					 &section_name, &num_section_params);
	if (strcmp(section_name, "mcp_trace_data") || num_section_params != 1)
		return DBG_STATUS_MCP_TRACE_BAD_DATA;
	dump_buf += qed_read_param(dump_buf,
				   &param_name, &param_str_val, &param_num_val);
	if (strcmp(param_name, "size"))
		return DBG_STATUS_MCP_TRACE_BAD_DATA;
	trace_data_dwords = param_num_val;

	/**
	 * Prepare trace info
	 */
	trace = (struct mcp_trace *)dump_buf;
	if (trace->signature != MFW_TRACE_SIGNATURE || !trace->size)
		return DBG_STATUS_MCP_TRACE_BAD_DATA;

	trace_buf = (u8 *) dump_buf + sizeof(*trace);
	offset = trace->trace_oldest;
	data_size = qed_cyclic_sub(trace->trace_prod, offset, trace->size);
	dump_buf += trace_data_dwords;

	/**
	 * Read meta_data section
	 */
	dump_buf += qed_read_section_hdr(dump_buf,
					 &section_name, &num_section_params);
	if (strcmp(section_name, "mcp_trace_meta"))
		return DBG_STATUS_MCP_TRACE_BAD_DATA;
	dump_buf += qed_read_param(dump_buf,
				   &param_name, &param_str_val, &param_num_val);
	if (strcmp(param_name, "size"))
		return DBG_STATUS_MCP_TRACE_BAD_DATA;
	trace_meta_dwords = param_num_val;

	/**
	 * Choose meta data buffer
	 */
	if (!trace_meta_dwords) {
		/**
		 * Dump doesn't include meta data
		 */
		struct dbg_tools_user_data *dev_user_data =
		    qed_dbg_get_user_data(p_hwfn);

		if (!dev_user_data->mcp_trace_user_meta_buf)
			return DBG_STATUS_MCP_TRACE_NO_META;

		meta_buf = dev_user_data->mcp_trace_user_meta_buf;
	} else {
		/* Dump includes meta data */
		meta_buf = dump_buf;
	}
	/**
	 * Allocate meta data memory
	 */
	status = qed_mcp_trace_alloc_meta_data(p_hwfn, meta_buf);
	if (status != DBG_STATUS_OK)
		return status;

	status = qed_parse_mcp_trace_buf(p_hwfn,
					 trace_buf,
					 trace->size,
					 offset,
					 data_size,
					 results_buf ? results_buf +
					 results_offset : NULL,
					 results_buf_size, &results_buf_bytes);
	if (status != DBG_STATUS_OK)
		return status;

	if (free_meta_data)
		qed_mcp_trace_free_meta_data(p_hwfn);

	*parsed_results_bytes = results_offset + results_buf_bytes;

	return DBG_STATUS_OK;
}

/**
 * Parses a Reg FIFO dump buffer. If result_buf is not NULL, the Reg FIFO
 * results are printed to it. In any case, the required results buffer size is
 * assigned to parsed_results_bytes. The parsing status is returned.
 */
static enum dbg_status qed_parse_reg_fifo_dump(struct qed_hwfn *p_hwfn,
					       u32 * dump_buf,
					       char *results_buf,
					       u32
					       results_buf_size,
					       u32 * parsed_results_bytes)
{
	const char *section_name, *param_name, *param_str_val;
	u32 param_num_val, num_section_params, num_elements;
	struct reg_fifo_element *elements;
	u8 i, j, err_code, vf_val;
	u32 results_offset = 0;
	char vf_str[4];
	int print_size;

	/**
	 * Read global_params section
	 */
	dump_buf += qed_read_section_hdr(dump_buf,
					 &section_name, &num_section_params);
	if (strcmp(section_name, "global_params"))
		return DBG_STATUS_REG_FIFO_BAD_DATA;

	/**
	 * Print global params
	 */
	dump_buf += qed_print_section_params(p_hwfn,
					     dump_buf,
					     num_section_params,
					     results_buf,
					     results_buf_size, &results_offset);

	/**
	 * Read reg_fifo_data section
	 */
	dump_buf += qed_read_section_hdr(dump_buf,
					 &section_name, &num_section_params);
	if (strcmp(section_name, "reg_fifo_data"))
		return DBG_STATUS_REG_FIFO_BAD_DATA;
	dump_buf += qed_read_param(dump_buf,
				   &param_name, &param_str_val, &param_num_val);
	if (strcmp(param_name, "size"))
		return DBG_STATUS_REG_FIFO_BAD_DATA;
	if (param_num_val % REG_FIFO_ELEMENT_DWORDS)
		return DBG_STATUS_REG_FIFO_BAD_DATA;
	num_elements = param_num_val / REG_FIFO_ELEMENT_DWORDS;
	elements = (struct reg_fifo_element *)dump_buf;

	/**
	 * Decode elements
	 */
	for (i = 0; i < num_elements; i++) {
		const char *err_msg = NULL;

		/**
		 * Discover if element belongs to a VF or a PF
		 */
		vf_val = GET_FIELD(elements[i].data, REG_FIFO_ELEMENT_VF);
		if (vf_val == REG_FIFO_ELEMENT_IS_PF_VF_VAL)
			scnprintf(vf_str, sizeof(vf_str), "%s", "N/A");
		else
			scnprintf(vf_str, sizeof(vf_str), "%d", vf_val);

		/**
		 * Find error message
		 */
		err_code = GET_FIELD(elements[i].data, REG_FIFO_ELEMENT_ERROR);
		for (j = 0; j < ARRAY_SIZE(s_reg_fifo_errors) && !err_msg; j++)
			if (err_code == s_reg_fifo_errors[j].err_code)
				err_msg = s_reg_fifo_errors[j].err_msg;

		/**
		 * Add parsed element to parsed buffer
		 */
		print_size =
		    scnprintf(qed_get_buf_ptr(results_buf,
					      results_offset),
			      qed_get_buf_size(p_hwfn,
					       results_buf,
					       results_buf_size,
					       results_offset),
			      "raw: 0x%016llx, address: 0x%07x, access: %-5s, pf: %2d, vf: %s, port: %d, privilege: %-3s, protection: %-12s, initiator: %-4s, error: %s\n",
			      elements[i].data,
			      (u32) GET_FIELD(elements[i].data,
					      REG_FIFO_ELEMENT_ADDRESS) *
			      REG_FIFO_ELEMENT_ADDR_FACTOR,
			      s_access_strs[GET_FIELD(elements[i].data,
						      REG_FIFO_ELEMENT_ACCESS)
			      ],
			      (u32) GET_FIELD(elements[i].data,
					      REG_FIFO_ELEMENT_PF),
			      vf_str,
			      (u32) GET_FIELD(elements[i].data,
					      REG_FIFO_ELEMENT_PORT),
			      s_privilege_strs[GET_FIELD(elements[i].data,
							 REG_FIFO_ELEMENT_PRIVILEGE)
			      ],
			      s_protection_strs[GET_FIELD(elements[i].data,
							  REG_FIFO_ELEMENT_PROTECTION)
			      ],
			      s_master_strs[GET_FIELD(elements[i].data,
						      REG_FIFO_ELEMENT_MASTER)
			      ], err_msg ? err_msg : "unknown error code");
		results_offset = qed_buf_offset_update(p_hwfn,
						       print_size,
						       results_offset);
	}

	print_size = scnprintf(qed_get_buf_ptr(results_buf,
					       results_offset),
			       qed_get_buf_size(p_hwfn,
						results_buf,
						results_buf_size,
						results_offset),
			       "fifo contained %d elements", num_elements);
	results_offset = qed_buf_offset_update(p_hwfn,
					       print_size, results_offset);

	/**
	 * Add 1 for string NULL termination
	 */
	*parsed_results_bytes = results_offset + 1;

	return DBG_STATUS_OK;
}

static enum dbg_status qed_parse_igu_fifo_element(struct qed_hwfn *p_hwfn, struct igu_fifo_element
						  *element, char
						  *results_buf,
						  u32
						  results_buf_size,
						  u32 * results_offset)
{
	const struct igu_fifo_addr_data *found_addr = NULL;
	char parsed_addr_data[32];
	char parsed_wr_data[256];
	u8 source, err_type, i;
	bool is_wr_cmd, is_pf;
	u16 cmd_addr;
	u64 dword12;
	int print_size;

	/* Dword12 (dword index 1 and 2) contains bits 32..95 of the
	 * FIFO element.
	 */
	dword12 = ((u64) element->dword2 << 32) | element->dword1;
	is_wr_cmd = GET_FIELD(dword12, IGU_FIFO_ELEMENT_DWORD12_IS_WR_CMD);
	is_pf = GET_FIELD(element->dword0, IGU_FIFO_ELEMENT_DWORD0_IS_PF);
	cmd_addr = GET_FIELD(element->dword0, IGU_FIFO_ELEMENT_DWORD0_CMD_ADDR);
	source = GET_FIELD(element->dword0, IGU_FIFO_ELEMENT_DWORD0_SOURCE);
	err_type = GET_FIELD(element->dword0, IGU_FIFO_ELEMENT_DWORD0_ERR_TYPE);

	if (source >= ARRAY_SIZE(s_igu_fifo_source_strs))
		return DBG_STATUS_IGU_FIFO_BAD_DATA;
	if (err_type >= ARRAY_SIZE(s_igu_fifo_error_strs))
		return DBG_STATUS_IGU_FIFO_BAD_DATA;

	/* Find address data */
	for (i = 0; i < ARRAY_SIZE(s_igu_fifo_addr_data) && !found_addr; i++) {
		const struct igu_fifo_addr_data *curr_addr =
		    &s_igu_fifo_addr_data[i];

		if (cmd_addr >= curr_addr->start_addr && cmd_addr <=
		    curr_addr->end_addr)
			found_addr = curr_addr;
	}

	if (!found_addr)
		return DBG_STATUS_IGU_FIFO_BAD_DATA;

	/* Prepare parsed address data */
	switch (found_addr->type) {
	case IGU_ADDR_TYPE_MSIX_MEM:
		scnprintf(parsed_addr_data,
			  sizeof(parsed_addr_data),
			  " vector_num = 0x%x", cmd_addr / 2);
		break;
	case IGU_ADDR_TYPE_WRITE_INT_ACK:
	case IGU_ADDR_TYPE_WRITE_PROD_UPDATE:
		scnprintf(parsed_addr_data,
			  sizeof(parsed_addr_data),
			  " SB = 0x%x", cmd_addr - found_addr->start_addr);
		break;
	default:
		parsed_addr_data[0] = '\0';
	}

	/* Prepare parsed write data */
	if (is_wr_cmd) {
		u32 wr_data, prod_cons;
		u8 is_cleanup;

		wr_data = GET_FIELD(dword12, IGU_FIFO_ELEMENT_DWORD12_WR_DATA);
		prod_cons = GET_FIELD(wr_data, IGU_FIFO_WR_DATA_PROD_CONS);
		is_cleanup = GET_FIELD(wr_data, IGU_FIFO_WR_DATA_CMD_TYPE);

		if (source == IGU_SRC_ATTN) {
			scnprintf(parsed_wr_data,
				  sizeof(parsed_wr_data),
				  "prod: 0x%x, ", prod_cons);
		} else {
			if (is_cleanup) {
				u8 cleanup_val, cleanup_type;

				cleanup_val = GET_FIELD(wr_data,
							IGU_FIFO_CLEANUP_WR_DATA_CLEANUP_VAL);
				cleanup_type = GET_FIELD(wr_data,
							 IGU_FIFO_CLEANUP_WR_DATA_CLEANUP_TYPE);

				scnprintf(parsed_wr_data,
					  sizeof(parsed_wr_data),
					  "cmd_type: cleanup, cleanup_val: %s, cleanup_type : %d, ",
					  cleanup_val ? "set" : "clear",
					  cleanup_type);
			} else {
				u8 update_flag, en_dis_int_for_sb, segment,
				    timer_mask;

				update_flag = GET_FIELD(wr_data,
							IGU_FIFO_WR_DATA_UPDATE_FLAG);
				en_dis_int_for_sb = GET_FIELD(wr_data,
							      IGU_FIFO_WR_DATA_EN_DIS_INT_FOR_SB);
				segment = GET_FIELD(wr_data,
						    IGU_FIFO_WR_DATA_SEGMENT);
				timer_mask = GET_FIELD(wr_data,
						       IGU_FIFO_WR_DATA_TIMER_MASK);

				scnprintf(parsed_wr_data,
					  sizeof(parsed_wr_data),
					  "cmd_type: prod/cons update, prod/cons: 0x%x, update_flag: %s, en_dis_int_for_sb : %s, segment : %s, timer_mask = %d, ",
					  prod_cons,
					  update_flag ? "update" : "nop",
					  en_dis_int_for_sb ? (en_dis_int_for_sb
							       ==
							       1 ? "disable" :
							       "nop") :
					  "enable",
					  segment ? "attn" : "regular",
					  timer_mask);
			}
		}
	} else {
		parsed_wr_data[0] = '\0';
	}

	/* Add parsed element to parsed buffer */
	print_size = scnprintf(qed_get_buf_ptr(results_buf,
					       *results_offset),
			       qed_get_buf_size(p_hwfn,
						results_buf,
						results_buf_size,
						*results_offset),
			       "raw: 0x%01x%08x%08x, %s: %d, source : %s, type : %s, cmd_addr : 0x%x(%s%s), %serror: %s\n",
			       element->dword2, element->dword1,
			       element->dword0,
			       is_pf ? "pf" : "vf",
			       GET_FIELD(element->dword0,
					 IGU_FIFO_ELEMENT_DWORD0_FID),
			       s_igu_fifo_source_strs[source],
			       is_wr_cmd ? "wr" : "rd",
			       cmd_addr,
			       (!is_pf &&
				found_addr->vf_desc) ? found_addr->vf_desc :
			       found_addr->desc,
			       parsed_addr_data,
			       parsed_wr_data, s_igu_fifo_error_strs[err_type]);
	*results_offset = qed_buf_offset_update(p_hwfn,
						print_size, *results_offset);

	return DBG_STATUS_OK;
}

/* Parses an IGU FIFO dump buffer. If result_buf is not NULL, the IGU FIFO
 * results are printed to it. In any case, the required results buffer size is
 * assigned to parsed_results_bytes. The parsing status is returned.
 */
static enum dbg_status qed_parse_igu_fifo_dump(struct qed_hwfn *p_hwfn,
					       u32 * dump_buf,
					       char *results_buf,
					       u32
					       results_buf_size,
					       u32 * parsed_results_bytes)
{
	const char *section_name, *param_name, *param_str_val;
	u32 param_num_val, num_section_params, num_elements;
	struct igu_fifo_element *elements;
	enum dbg_status status;
	u32 results_offset = 0;
	u8 i;
	int print_size;

	/* Read global_params section */
	dump_buf += qed_read_section_hdr(dump_buf,
					 &section_name, &num_section_params);
	if (strcmp(section_name, "global_params"))
		return DBG_STATUS_IGU_FIFO_BAD_DATA;

	/* Print global params */
	dump_buf += qed_print_section_params(p_hwfn,
					     dump_buf,
					     num_section_params,
					     results_buf,
					     results_buf_size, &results_offset);

	/* Read igu_fifo_data section */
	dump_buf += qed_read_section_hdr(dump_buf,
					 &section_name, &num_section_params);
	if (strcmp(section_name, "igu_fifo_data"))
		return DBG_STATUS_IGU_FIFO_BAD_DATA;
	dump_buf += qed_read_param(dump_buf,
				   &param_name, &param_str_val, &param_num_val);
	if (strcmp(param_name, "size"))
		return DBG_STATUS_IGU_FIFO_BAD_DATA;
	if (param_num_val % IGU_FIFO_ELEMENT_DWORDS)
		return DBG_STATUS_IGU_FIFO_BAD_DATA;
	num_elements = param_num_val / IGU_FIFO_ELEMENT_DWORDS;
	elements = (struct igu_fifo_element *)dump_buf;

	/* Decode elements */
	for (i = 0; i < num_elements; i++)
		if ((status =
		     qed_parse_igu_fifo_element(p_hwfn, &elements[i],
						results_buf,
						results_buf_size,
						&results_offset)) !=
		    DBG_STATUS_OK)
			return status;

	print_size = scnprintf(qed_get_buf_ptr(results_buf,
					       results_offset),
			       qed_get_buf_size(p_hwfn,
						results_buf,
						results_buf_size,
						results_offset),
			       "fifo contained %d elements", num_elements);
	results_offset = qed_buf_offset_update(p_hwfn,
					       print_size, results_offset);

	/* Add 1 for string NULL termination */
	*parsed_results_bytes = results_offset + 1;

	return DBG_STATUS_OK;
}

static enum dbg_status qed_parse_protection_override_dump(struct qed_hwfn
							  *p_hwfn,
							  u32 * dump_buf, char
							  *results_buf,
							  u32 results_buf_size,
							  u32 *
							  parsed_results_bytes)
{
	const char *section_name, *param_name, *param_str_val;
	u32 param_num_val, num_section_params, num_elements;
	struct protection_override_element *elements;
	u32 results_offset = 0;
	u8 i;
	int print_size;

	/* Read global_params section */
	dump_buf += qed_read_section_hdr(dump_buf,
					 &section_name, &num_section_params);
	if (strcmp(section_name, "global_params"))
		return DBG_STATUS_PROTECTION_OVERRIDE_BAD_DATA;

	/* Print global params */
	dump_buf += qed_print_section_params(p_hwfn,
					     dump_buf,
					     num_section_params,
					     results_buf,
					     results_buf_size, &results_offset);

	/* Read protection_override_data section */
	dump_buf += qed_read_section_hdr(dump_buf,
					 &section_name, &num_section_params);
	if (strcmp(section_name, "protection_override_data"))
		return DBG_STATUS_PROTECTION_OVERRIDE_BAD_DATA;
	dump_buf += qed_read_param(dump_buf,
				   &param_name, &param_str_val, &param_num_val);
	if (strcmp(param_name, "size"))
		return DBG_STATUS_PROTECTION_OVERRIDE_BAD_DATA;
	if (param_num_val % PROTECTION_OVERRIDE_ELEMENT_DWORDS)
		return DBG_STATUS_PROTECTION_OVERRIDE_BAD_DATA;
	num_elements = param_num_val / PROTECTION_OVERRIDE_ELEMENT_DWORDS;
	elements = (struct protection_override_element *)dump_buf;

	/* Decode elements */
	for (i = 0; i < num_elements; i++) {
		u32 address = GET_FIELD(elements[i].data,
					PROTECTION_OVERRIDE_ELEMENT_ADDRESS) *
		    PROTECTION_OVERRIDE_ELEMENT_ADDR_FACTOR;

		print_size =
		    scnprintf(qed_get_buf_ptr(results_buf,
					      results_offset),
			      qed_get_buf_size(p_hwfn,
					       results_buf,
					       results_buf_size,
					       results_offset),
			      "window %2d, address: 0x%07x, size: %7d regs, read: %d, write: %d, read protection: %-12s, write protection: %-12s\n",
			      i, address,
			      (u32) GET_FIELD(elements[i].data,
					      PROTECTION_OVERRIDE_ELEMENT_WINDOW_SIZE),
			      (u32) GET_FIELD(elements[i].data,
					      PROTECTION_OVERRIDE_ELEMENT_READ),
			      (u32) GET_FIELD(elements[i].data,
					      PROTECTION_OVERRIDE_ELEMENT_WRITE),
			      s_protection_strs[GET_FIELD(elements[i].data,
							  PROTECTION_OVERRIDE_ELEMENT_READ_PROTECTION)
			      ],
			      s_protection_strs[GET_FIELD(elements[i].data,
							  PROTECTION_OVERRIDE_ELEMENT_WRITE_PROTECTION)
			      ]);
		results_offset = qed_buf_offset_update(p_hwfn,
						       print_size,
						       results_offset);
	}

	print_size = scnprintf(qed_get_buf_ptr(results_buf,
					       results_offset),
			       qed_get_buf_size(p_hwfn,
						results_buf,
						results_buf_size,
						results_offset),
			       "protection override contained %d elements",
			       num_elements);
	results_offset = qed_buf_offset_update(p_hwfn,
					       print_size, results_offset);

	/* Add 1 for string NULL termination */
	*parsed_results_bytes = results_offset + 1;

	return DBG_STATUS_OK;
}

/* Parses a FW Asserts dump buffer. if result_buf is not NULL, the FW Asserts
 * results are printed to it. in any case, the required results buffer size is
 * assigned to parsed_results_bytes. the parsing status is returned.
 */
static enum dbg_status qed_parse_fw_asserts_dump(struct qed_hwfn *p_hwfn,
						 u32 *
						 dump_buf,
						 char *results_buf,
						 u32
						 results_buf_size,
						 u32 * parsed_results_bytes)
{
	u32 num_section_params, param_num_val, i, results_offset = 0;
	const char *param_name, *param_str_val, *section_name;
	bool last_section_found = false;
	int print_size;

	*parsed_results_bytes = 0;

	/* Read global_params section */
	dump_buf += qed_read_section_hdr(dump_buf,
					 &section_name, &num_section_params);
	if (strcmp(section_name, "global_params"))
		return DBG_STATUS_FW_ASSERTS_PARSE_FAILED;

	/* Print global params */
	dump_buf += qed_print_section_params(p_hwfn,
					     dump_buf,
					     num_section_params,
					     results_buf,
					     results_buf_size, &results_offset);

	while (!last_section_found) {
		dump_buf += qed_read_section_hdr(dump_buf,
						 &section_name,
						 &num_section_params);
		if (!strcmp(section_name, "fw_asserts")) {
			/* Extract params */
			const char *storm_letter = NULL;
			u32 storm_dump_size = 0;

			for (i = 0; i < num_section_params; i++) {
				dump_buf += qed_read_param(dump_buf,
							   &param_name,
							   &param_str_val,
							   &param_num_val);
				if (!strcmp(param_name, "storm"))
					storm_letter = param_str_val;
				else if (!strcmp(param_name, "size"))
					storm_dump_size = param_num_val;
				else
					return
					    DBG_STATUS_FW_ASSERTS_PARSE_FAILED;
			}

			if (!storm_letter || !storm_dump_size)
				return DBG_STATUS_FW_ASSERTS_PARSE_FAILED;

			/* Print data */
			print_size =
			    scnprintf(qed_get_buf_ptr(results_buf,
						      results_offset),
				      qed_get_buf_size(p_hwfn,
						       results_buf,
						       results_buf_size,
						       results_offset),
				      "\n%sSTORM_ASSERT: size=%d\n",
				      storm_letter, storm_dump_size);
			results_offset = qed_buf_offset_update(p_hwfn,
							       print_size,
							       results_offset);
			for (i = 0; i < storm_dump_size; i++, dump_buf++) {
				print_size =
				    scnprintf(qed_get_buf_ptr(results_buf,
							      results_offset),
					      qed_get_buf_size(p_hwfn,
							       results_buf,
							       results_buf_size,
							       results_offset),
					      "%08x\n", *dump_buf);
				results_offset = qed_buf_offset_update(p_hwfn,
								       print_size,
								       results_offset);
			}
		} else if (!strcmp(section_name, "last")) {
			last_section_found = true;
		} else {
			return DBG_STATUS_FW_ASSERTS_PARSE_FAILED;
		}
	}
	;

	/* Add 1 for string NULL termination */
	*parsed_results_bytes = results_offset + 1;

	return DBG_STATUS_OK;
}

/***************************** Public Functions *******************************/

enum dbg_status qed_dbg_user_set_bin_ptr(struct qed_hwfn *p_hwfn,
					 const u8 * const bin_ptr)
{
	struct bin_buffer_hdr *buf_hdrs = (struct bin_buffer_hdr *)bin_ptr;
	u8 buf_id;

	/* Convert binary data to debug arrays */
	for (buf_id = 0; buf_id < MAX_BIN_DBG_BUFFER_TYPE; buf_id++)
		qed_set_dbg_bin_buf(p_hwfn,
				    (enum bin_dbg_buffer_type)buf_id,
				    (u32 *) (bin_ptr + buf_hdrs[buf_id].offset),
				    buf_hdrs[buf_id].length);

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_alloc_user_data(struct qed_hwfn *p_hwfn,
					void **user_data_ptr)
{
	*user_data_ptr =
	    kzalloc(sizeof(struct dbg_tools_user_data), GFP_KERNEL);
	if (!(*user_data_ptr))
		return DBG_STATUS_VIRT_MEM_ALLOC_FAILED;

	memset(*user_data_ptr, 0, sizeof(struct dbg_tools_user_data));

	/* Set debug binary buffers */

	return DBG_STATUS_OK;
}

enum dbg_storms qed_dbg_get_storm_id(const char *storm_name)
{
	u8 i;

	for (i = 0; i < MAX_DBG_STORMS; i++)
		if (!strcmp(s_storm_str[i], storm_name))
			return (enum dbg_storms)i;

	return MAX_DBG_STORMS;
}

enum block_id qed_dbg_get_block_id(struct qed_hwfn *p_hwfn,
				   const char *block_name)
{
	const struct dbg_block_user *block;
	u32 block_id;

	for (block_id = 0; block_id < MAX_BLOCK_ID; block_id++) {
		block =
		    (const struct dbg_block_user *)p_hwfn->
		    dbg_arrays[BIN_BUF_DBG_BLOCKS_USER_DATA].ptr + block_id;
		if (!strcmp((const char *)block->name, block_name))
			return (enum block_id)block_id;
	}

	return MAX_BLOCK_ID;
}

enum dbg_bus_storm_modes qed_dbg_get_storm_mode_id(const char *storm_mode_name)
{
	u8 i;

	for (i = 0; i < MAX_DBG_BUS_STORM_MODES; i++)
		if (!strcmp(s_storm_mode_str[i], storm_mode_name))
			return (enum dbg_bus_storm_modes)i;

	return MAX_DBG_BUS_STORM_MODES;
}

enum dbg_bus_constraint_ops qed_dbg_get_constraint_op_id(const char *op_name)
{
	u8 i;

	for (i = 0; i < MAX_DBG_BUS_CONSTRAINT_OPS; i++)
		if (!strcmp(s_constraint_op_str[i], op_name))
			return (enum dbg_bus_constraint_ops)i;

	return MAX_DBG_BUS_CONSTRAINT_OPS;
}

const char *qed_dbg_get_status_str(enum dbg_status status)
{
	return (status <
		MAX_DBG_STATUS) ? s_status_str[status] : "Invalid debug status";
}

enum dbg_grc_params qed_dbg_get_grc_param_id(const char *param_name)
{
	u8 i;

	for (i = 0; i < MAX_DBG_GRC_PARAMS; i++)
		if (!strcmp(s_grc_param_str[i], param_name))
			return (enum dbg_grc_params)i;

	return MAX_DBG_GRC_PARAMS;
}

int qed_dbg_get_dbg_bus_line(struct qed_hwfn *p_hwfn,
			     enum block_id block_id,
			     enum chip_ids chip_id, const char *line)
{
	const struct dbg_block_chip_user *block =
	    (const struct dbg_block_chip_user *)p_hwfn->
	    dbg_arrays[BIN_BUF_DBG_BLOCKS_CHIP_USER_DATA].ptr +
	    block_id * MAX_CHIP_IDS + chip_id;

	/* Check if line is a number */
	if (qed_is_number(line)) {
		int rc;
		unsigned long line_num = 0;

		rc = kstrtoul(line, 10, &line_num);
		if (rc)
			return -1;
		return ((int)line_num <
			NUM_DBG_LINES_USER(block)) ? (int)line_num : -1;
	} else if (!strcmp(line, DBG_BUS_SIGNATURE_LINE_NAME)) {
		return DBG_BUS_SIGNATURE_LINE_NUM;
	} else if (!strcmp(line, DBG_BUS_LATENCY_LINE_NAME)) {
		return block->
		    has_latency_events ? DBG_BUS_LATENCY_LINE_NUM : -1;
	} else {
		/* Block-specific debug line string */
		const u32 *block_line_names_buf;
		const char *parsing_str_buf;
		u8 line_num;

		block_line_names_buf =
		    (u32 *) p_hwfn->
		    dbg_arrays[BIN_BUF_DBG_BUS_LINE_NAME_OFFSETS].ptr +
		    block->names_offset;
		parsing_str_buf =
		    (const char *)p_hwfn->
		    dbg_arrays[BIN_BUF_DBG_PARSING_STRINGS].ptr;

		/* Search name in debug lines array */
		for (line_num = 0; line_num < block->num_of_dbg_bus_lines;
		     line_num++)
			if (!strcmp(line,
				    &parsing_str_buf[block_line_names_buf
						     [line_num]]))
				return line_num +
				    NUM_EXTRA_DBG_LINES_USER(block);

		return -1;
	}
}

enum dbg_status qed_get_idle_chk_results_buf_size(struct qed_hwfn *p_hwfn,
						  u32 *
						  dump_buf,
						  u32
						  num_dumped_dwords,
						  u32 * results_buf_size)
{
	u32 num_errors, num_warnings;

	return qed_parse_idle_chk_dump(p_hwfn,
				       dump_buf,
				       num_dumped_dwords,
				       NULL,
				       0,
				       results_buf_size,
				       &num_errors, &num_warnings);
}

enum dbg_status qed_print_idle_chk_results(struct qed_hwfn *p_hwfn,
					   u32 * dump_buf,
					   u32
					   num_dumped_dwords,
					   char *results_buf,
					   u32
					   results_buf_size,
					   u32 * num_errors, u32 * num_warnings)
{
	u32 parsed_buf_size;

	return qed_parse_idle_chk_dump(p_hwfn,
				       dump_buf,
				       num_dumped_dwords,
				       results_buf,
				       results_buf_size,
				       &parsed_buf_size,
				       num_errors, num_warnings);
}

void qed_dbg_mcp_trace_set_meta_data(struct qed_hwfn *p_hwfn,
				     const u32 * meta_buf)
{
	struct dbg_tools_user_data *dev_user_data =
	    qed_dbg_get_user_data(p_hwfn);

	dev_user_data->mcp_trace_user_meta_buf = meta_buf;
}

enum dbg_status qed_get_mcp_trace_results_buf_size(struct qed_hwfn *p_hwfn,
						   u32 *
						   dump_buf,
						   u32 __maybe_unused
						   num_dumped_dwords,
						   u32 * results_buf_size)
{
	return qed_parse_mcp_trace_dump(p_hwfn,
					dump_buf,
					NULL, 0, results_buf_size, true);
}

enum dbg_status qed_print_mcp_trace_results(struct qed_hwfn *p_hwfn,
					    u32 * dump_buf,
					    u32 __maybe_unused
					    num_dumped_dwords,
					    char *results_buf,
					    u32 results_buf_size)
{
	u32 parsed_buf_size;

	/* Doesn't do anything, needed for compile time asserts */
	qed_user_static_asserts();

	return qed_parse_mcp_trace_dump(p_hwfn,
					dump_buf,
					results_buf,
					results_buf_size,
					&parsed_buf_size, true);
}

enum dbg_status qed_print_mcp_trace_results_cont(struct qed_hwfn *p_hwfn,
						 u32 *
						 dump_buf,
						 u32 __maybe_unused
						 num_dumped_dwords,
						 char *results_buf,
						 u32 results_buf_size)
{
	u32 parsed_buf_size;

	return qed_parse_mcp_trace_dump(p_hwfn,
					dump_buf,
					results_buf,
					results_buf_size,
					&parsed_buf_size, false);
}

enum dbg_status qed_print_mcp_trace_line(struct qed_hwfn *p_hwfn,
					 u8 * dump_buf,
					 u32
					 num_dumped_bytes,
					 char *results_buf,
					 u32 results_buf_size)
{
	u32 parsed_results_bytes;

	return qed_parse_mcp_trace_buf(p_hwfn,
				       dump_buf,
				       num_dumped_bytes,
				       0,
				       num_dumped_bytes,
				       results_buf,
				       results_buf_size, &parsed_results_bytes);
}

/* Frees the specified MCP Trace meta data */
void qed_mcp_trace_free_meta_data(struct qed_hwfn *p_hwfn)
{
	struct dbg_tools_user_data *dev_user_data =
	    qed_dbg_get_user_data(p_hwfn);
	struct mcp_trace_meta *meta = &dev_user_data->mcp_trace_meta;
	u32 i;

	if (!meta->is_allocated)
		return;

	/* Release modules */
	if (meta->modules) {
		for (i = 0; i < meta->modules_num; i++)
			kfree(meta->modules[i]);
		kfree(meta->modules);
	}

	/* Release formats */
	if (meta->formats) {
		for (i = 0; i < meta->formats_num; i++)
			kfree(meta->formats[i].format_str);
		kfree(meta->formats);
	}

	meta->is_allocated = false;
}

enum dbg_status qed_get_reg_fifo_results_buf_size(struct qed_hwfn __maybe_unused
						  * p_hwfn, u32 * dump_buf,
						  u32 __maybe_unused
						  num_dumped_dwords,
						  u32 * results_buf_size)
{
	return qed_parse_reg_fifo_dump(p_hwfn,
				       dump_buf, NULL, 0, results_buf_size);
}

enum dbg_status qed_print_reg_fifo_results(struct qed_hwfn __maybe_unused *
					   p_hwfn, u32 * dump_buf,
					   u32 __maybe_unused num_dumped_dwords,
					   char
					   *results_buf, u32 results_buf_size)
{
	u32 parsed_buf_size;

	return qed_parse_reg_fifo_dump(p_hwfn,
				       dump_buf,
				       results_buf,
				       results_buf_size, &parsed_buf_size);
}

enum dbg_status qed_get_igu_fifo_results_buf_size(struct qed_hwfn __maybe_unused
						  * p_hwfn, u32 * dump_buf,
						  u32 __maybe_unused
						  num_dumped_dwords,
						  u32 * results_buf_size)
{
	return qed_parse_igu_fifo_dump(p_hwfn,
				       dump_buf, NULL, 0, results_buf_size);
}

enum dbg_status qed_print_igu_fifo_results(struct qed_hwfn __maybe_unused *
					   p_hwfn, u32 * dump_buf,
					   u32 __maybe_unused num_dumped_dwords,
					   char
					   *results_buf, u32 results_buf_size)
{
	u32 parsed_buf_size;

	return qed_parse_igu_fifo_dump(p_hwfn,
				       dump_buf,
				       results_buf,
				       results_buf_size, &parsed_buf_size);
}

enum dbg_status qed_get_protection_override_results_buf_size(struct qed_hwfn
							     __maybe_unused *
							     p_hwfn,
							     u32 * dump_buf,
							     u32 __maybe_unused
							     num_dumped_dwords,
							     u32 *
							     results_buf_size)
{
	return qed_parse_protection_override_dump(p_hwfn,
						  dump_buf,
						  NULL, 0, results_buf_size);
}

enum dbg_status qed_print_protection_override_results(struct qed_hwfn
						      __maybe_unused * p_hwfn,
						      u32 * dump_buf,
						      u32 __maybe_unused
						      num_dumped_dwords, char
						      *results_buf,
						      u32 results_buf_size)
{
	u32 parsed_buf_size;

	return qed_parse_protection_override_dump(p_hwfn,
						  dump_buf,
						  results_buf,
						  results_buf_size,
						  &parsed_buf_size);
}

enum dbg_status qed_get_fw_asserts_results_buf_size(struct qed_hwfn
						    __maybe_unused * p_hwfn,
						    u32 * dump_buf,
						    u32 __maybe_unused
						    num_dumped_dwords,
						    u32 * results_buf_size)
{
	return qed_parse_fw_asserts_dump(p_hwfn,
					 dump_buf, NULL, 0, results_buf_size);
}

enum dbg_status qed_print_fw_asserts_results(struct qed_hwfn __maybe_unused *
					     p_hwfn, u32 * dump_buf,
					     u32 __maybe_unused
					     num_dumped_dwords, char
					     *results_buf, u32 results_buf_size)
{
	u32 parsed_buf_size;

	return qed_parse_fw_asserts_dump(p_hwfn,
					 dump_buf,
					 results_buf,
					 results_buf_size, &parsed_buf_size);
}

enum dbg_status qed_dbg_parse_attn(struct qed_hwfn *p_hwfn,
				   struct dbg_attn_block_result *results)
{
	const u32 *block_attn_name_offsets;
	enum dbg_attn_type attn_type;
	const char *block_name;
	u8 num_regs, i, j;

	num_regs = GET_FIELD(results->data, DBG_ATTN_BLOCK_RESULT_NUM_REGS);
	attn_type = (enum dbg_attn_type)GET_FIELD(results->data,
						  DBG_ATTN_BLOCK_RESULT_ATTN_TYPE);
	block_name = qed_dbg_get_block_name(p_hwfn,
					    (enum block_id)results->block_id);
	if (!block_name)
		return DBG_STATUS_INVALID_ARGS;

	if (!p_hwfn->dbg_arrays[BIN_BUF_DBG_ATTN_INDEXES].ptr ||
	    !p_hwfn->dbg_arrays[BIN_BUF_DBG_ATTN_NAME_OFFSETS].ptr ||
	    !p_hwfn->dbg_arrays[BIN_BUF_DBG_PARSING_STRINGS].ptr)
		return DBG_STATUS_DBG_ARRAY_NOT_SET;

	block_attn_name_offsets =
	    (u32 *) p_hwfn->dbg_arrays[BIN_BUF_DBG_ATTN_NAME_OFFSETS].ptr +
	    results->names_offset;

	/* Go over registers with a non-zero attention status */
	for (i = 0; i < num_regs; i++) {
		struct dbg_attn_bit_mapping *bit_mapping;
		struct dbg_attn_reg_result *reg_result;
		u8 num_reg_attn, bit_idx = 0;

		reg_result = &results->reg_results[i];
		num_reg_attn = GET_FIELD(reg_result->data,
					 DBG_ATTN_REG_RESULT_NUM_REG_ATTN);
		bit_mapping =
		    (struct dbg_attn_bit_mapping *)p_hwfn->
		    dbg_arrays[BIN_BUF_DBG_ATTN_INDEXES].ptr +
		    reg_result->block_attn_offset;

		/* Go over attention status bits */
		for (j = 0; j < num_reg_attn; j++) {
			u16 attn_idx_val = GET_FIELD(bit_mapping[j].data,
						     DBG_ATTN_BIT_MAPPING_VAL);

			/* Check if bit mask should be advanced (due to unused
			 * bits).
			 */
			if (GET_FIELD(bit_mapping[j].data,
				      DBG_ATTN_BIT_MAPPING_IS_UNUSED_BIT_CNT)) {
				bit_idx += (u8) attn_idx_val;
				continue;
			}

			/* Check current bit index */
			if (reg_result->sts_val & BIT(bit_idx)) {
				/* An attention bit with value=1 was found */
				const char *attn_name, *attn_type_str,
				    *masked_str;
				u32 attn_name_offset;
				u32 sts_addr;

				/* Find attention name */
				attn_name_offset =
				    block_attn_name_offsets[attn_idx_val];
				attn_name =
				    (const char *)p_hwfn->
				    dbg_arrays[BIN_BUF_DBG_PARSING_STRINGS].ptr
				    + attn_name_offset;
				attn_type_str =
				    (attn_type ==
				     ATTN_TYPE_INTERRUPT ? "Interrupt" :
				     "Parity");
				masked_str =
				    reg_result->
				    mask_val & BIT(bit_idx) ? " [masked]" : "";
				sts_addr =
				    GET_FIELD(reg_result->data,
					      DBG_ATTN_REG_RESULT_STS_ADDRESS);
				DP_NOTICE(p_hwfn,
					  "%s (%s) : %s [address 0x%08x, bit %d]%s\n",
					  block_name, attn_type_str, attn_name,
					  sts_addr * 4, bit_idx, masked_str);
			}

			bit_idx++;
		}
	}

	return DBG_STATUS_OK;
}

enum dbg_status qed_dbg_internal_trace_get_dump_buf_size(struct qed_hwfn
							 *p_hwfn,
							 struct qed_ptt
							 __maybe_unused * p_ptt,
							 u32 * buf_dword_size)
{
	struct qed_internal_trace *log = &p_hwfn->cdev->internal_trace;

	*buf_dword_size = BYTES_TO_DWORDS(log->size + 3);

	if (log->buf)
		return DBG_STATUS_OK;
	else
		return DBG_STATUS_RECORDING_NOT_STARTED;
}

enum dbg_status qed_dbg_internal_trace_dump(struct qed_hwfn *p_hwfn,
					    struct qed_ptt *p_ptt,
					    u32 * dump_buf,
					    u32 buf_dword_size,
					    u32 * num_dumped_dwords)
{
	struct qed_internal_trace *log = &p_hwfn->cdev->internal_trace;
	char *char_buf = (char *)dump_buf;
	u32 needed_buf_size_in_dwords;
	enum dbg_status status;
	u32 prod_to_size_len;

	*num_dumped_dwords = 0;

	status = qed_dbg_internal_trace_get_dump_buf_size(p_hwfn,
							  p_ptt,
							  &needed_buf_size_in_dwords);
	if (status != DBG_STATUS_OK)
		return status;

	if (buf_dword_size < needed_buf_size_in_dwords)
		return DBG_STATUS_DUMP_BUF_TOO_SMALL;

	spin_lock_bh(&log->lock);

	/* copy buf from prod to size then from 0 to prod */

	prod_to_size_len = (u32) min_t(u64, log->prod, log->size) -
	    (log->prod % log->size);

	memcpy(char_buf, log->buf + (log->prod % log->size), prod_to_size_len);
	memcpy(char_buf + prod_to_size_len, log->buf, log->prod % log->size);
	if (log->prod >= log->size) {
		*num_dumped_dwords = BYTES_TO_DWORDS(log->size);
	} else {
		memset(char_buf + log->prod, 0, (log->size - log->prod));
		*num_dumped_dwords = (u32) BYTES_TO_DWORDS(log->prod + 3);
	}

	spin_unlock_bh(&log->lock);

	return DBG_STATUS_OK;
}

#define MAX_PORT_PER_DEVICE     4
/* max 128 char per line in the dump output */
#define MAX_CHAR_PER_LINE       128

struct mdump_phy_cfg {
	u32 core_cfg;
	u32 f_lane_cfg1;
	u32 e_lane_cfg1;
	u32 generic_cont1;
	u32 link_change_count[MAX_PORT_PER_DEVICE];
	u32 lfa_status[MAX_PORT_PER_DEVICE];
	u32 transceiver_data[MAX_PORT_PER_DEVICE];
};

struct link_dump_port_lane_map {
	u8 port0;
	u8 port1;
	u8 port2;
	u8 port3;
};

union link_dump_port_lane_map_u {
	struct link_dump_port_lane_map lane_map;
	u8 port[MAX_PORT_PER_DEVICE];
};

enum display_mode_t {
	ITEM = 0,
	PORT0,
	PORT1,
	PORT2,
	PORT3
};

enum select_display_param_t {
	LINK_STATE,
	PCS_LINK_STATE,
	MAC_FAULT,
	MAC_FAULT_HW,
	RX_SIGNAL,
	LINK_OWNER,
	LINK_SPEED,
	MODULE_P,
	FLOW_CTRL,
	FEC_MODE,
	FEC_COR_ERR_CNT,
	FEC_UNCOR_ERR_CNT,
	AUTONEG,
	AN_LP_ADV_SPEEDS,
	AN_LP_ADV_FLOW_CTRL,
	TX_PRE_FIR,
	TX_MAIN_FIR,
	TX_POST_FIR,
	RX_DFE,
	DATA_MODE,
	TRANSCEIVER_TYPE,
	PORT_SWAP,
	LINK_CHANGE_CNT,
	LFA_CNT,
	LFA_STATE,
	MFW_RAW_LINK_STATUS
};

enum chip_mode_t {
	CHIP_MODE_2X40,
	CHIP_MODE_2X50,
	CHIP_MODE_1X100,
	CHIP_MODE_4X10GF,
	CHIP_MODE_4X25GF,
	CHIP_MODE_4X10GE,
	CHIP_MODE_4X20,
	CHIP_MODE_1X40_2X10,
	CHIP_MODE_1X40_2X20,
	CHIP_MODE_4X25G,
	CHIP_MODE_2X25G,
	CHIP_MODE_2X10G,
	CHIP_MODE_1X40,
	CHIP_MODE_1X25G,
	CHIP_MODE_UNKNOWN,
};

struct link_dump_port {
	u32 pcs_status;
	u32 mac_hw_fault_status;
	u32 mfw_link_status;
	u32 transceiver_data;
	u32 link_change_count;
	u32 lfa_status;
};

struct link_dump {
	enum chip_mode_t mode;
	struct link_dump_port port[MAX_PORT_PER_DEVICE];
	u8 is_falcon[MAX_PORT_PER_DEVICE];
};

struct mdump2_cfg_params {
	struct mdump_phy_cfg phy_cfg;
	union link_dump_port_lane_map_u linkdump_port_lane_map;
	struct link_dump link_dump_results;
};

struct log_hdr_stc {
	u32 log;
	u32 log_size;
	u32 storm_fw_ver;
	u32 storm_timestamp;
	u32 storm_tools_ver;
	u32 mfw_ver;
	u32 mba_ver;
	u32 chip;
	u32 num_of_func;
	u32 num_of_ports;
	u32 link_status[MAX_PORT_PER_DEVICE];
	u32 power_mode;
	u32 log_epoch;
	u32 reason;
};

struct tlv_stc {
	u32 type;
	u32 len;
};

#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_2X40G        0x0
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X50G           0x1
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_1X100G       0x2
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_4X10G_F         0x3
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_4X10G_E      0x4
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_4X20G        0x5
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_1X40G           0xB
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X25G           0xC
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_1X25G           0xD
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_4X25G           0xE
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X10G           0xF
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X25G_LIO2      0x10

#define NWM_REG_FC_FEC_CERR_COUNT_0     0x8000a8UL
#define NWM_REG_FC_FEC_CERR_COUNT_1     0x8000acUL
#define NWM_REG_FC_FEC_CERR_COUNT_2     0x8000b0UL
#define NWM_REG_FC_FEC_CERR_COUNT_3     0x8000b4UL
#define NWM_REG_FC_FEC_CERR_COUNT_4     0x8000b8UL
#define NWM_REG_FC_FEC_CERR_COUNT_5     0x8000bcUL
#define NWM_REG_FC_FEC_CERR_COUNT_6     0x8000c0UL
#define NWM_REG_FC_FEC_CERR_COUNT_7     0x8000c4UL
#define NWM_REG_FC_FEC_NCERR_COUNT_0    0x8000c8UL
#define NWM_REG_FC_FEC_NCERR_COUNT_1    0x8000ccUL
#define NWM_REG_FC_FEC_NCERR_COUNT_2    0x8000d0UL
#define NWM_REG_FC_FEC_NCERR_COUNT_3    0x8000d4UL
#define NWM_REG_FC_FEC_NCERR_COUNT_4    0x8000d8UL
#define NWM_REG_FC_FEC_NCERR_COUNT_5    0x8000dcUL
#define NWM_REG_FC_FEC_NCERR_COUNT_6    0x8000e0UL
#define NWM_REG_FC_FEC_NCERR_COUNT_7    0x8000e4UL

#define isleap(y)       ((!(y % 4) && (y % 100)) || (!(y % 400)))
#define SEC_PER_HOUR    (60 * 60)
#define SEC_PER_DAY     (24 * SEC_PER_HOUR)
#define SEC_PER_YEAR    (365 * SEC_PER_DAY)

#define MDUMP_SIGNATURE         (('m') | ('d' << 8) | ('m' << 16) | ('p' << 24))
#define LOG_SIGNATURE           (('l') | ('d' << 8) | ('m' << 16) | ('p' << 24))
#define MAX_OFFSET              32

#define ASCII2TYPE(a, b, c, d)  ((d) | ((c) << 8) | ((b) << 16) | ((a) << 24))
#define TLV_UNUSED_TYPE0        ASCII2TYPE('a', 's', 'r', 't')
#define TLV_UNUSED_TYPE1        ASCII2TYPE('r', 'e', 'g', 'f')
#define TLV_UNUSED_TYPE2        ASCII2TYPE('i', 'g', 'u', 'f')
#define STATIC_INIT_TYPE        ASCII2TYPE('s', 't', 'i', 'c')
#define FUNC_PUBLIC_TYPE        ASCII2TYPE('f', 'u', 'n', 'c')
#define FUNC_PRIVATE_TYPE       ASCII2TYPE('p', 'r', 'i', 'v')
#define MCP_TRACE_TYPE          ASCII2TYPE('m', 'c', 'p', 't')
#define GRC_REGS_TYPE           ASCII2TYPE('g', 'r', 'c', 'r')
#define PORT_REGS_TYPE          ASCII2TYPE('p', 'o', 'r', 't')
#define PF_REGS_TYPE            ASCII2TYPE('p', 'r', 'e', 'g')
#define EAGLE_LANE_TYPE         ASCII2TYPE('e', 'a', 'g', 'l')
#define FALCON_LANE_TYPE        ASCII2TYPE('f', 'l', 'c', 'n')
#define TSCE_LANE_TYPE          ASCII2TYPE('t', 's', 'c', 'e')
#define TSCF_LANE_TYPE          ASCII2TYPE('t', 's', 'c', 'f')
#define CXLPORT_TYPE            ASCII2TYPE('c', 'x', 'l', 'p')
#define PHY_CFG_TYPE            ASCII2TYPE('p', 'h', 'y', 'c')
#define EOD_OF_LOG              ASCII2TYPE('e', 'n', 'd', 'l')

#define LANE_NONE_MASK          0x00
#define LANE_0_MASK             0x01
#define LANE_1_MASK             0x02
#define LANE_2_MASK             0x04
#define LANE_3_MASK             0x08
#define EAGLE_LANE_0_MASK       0x10
#define EAGLE_LANE_1_MASK       0x20
#define EAGLE_LANE_2_MASK       0x40
#define EAGLE_LANE_3_MASK       0x80

#define PCS_STATUS_REG_LINK     0x0
#define PCS_STATUS_LINK_UP      0x1
#define PCS_STATUS_LINK_DOWN    0x2

#define PCS_STATUS_REG_FAULT            0x1
#define PCS_STATUS_LOCAL_FAULT          0x1
#define PCS_STATUS_REMOTE_FAULT         0x2
#define PCS_STATUS_INTERRUPT_FAULT      0x4

#define LINE_SIZE                       8
#define STR_APPEND(RES_BUF, RES_OFF, ...) \
	sprintf(qed_get_buf_ptr(RES_BUF, RES_OFF), __VA_ARGS__)

enum e_mdump2_regval_strs_ids {
	NUMBER_OF_PORTS,
	PORT_LINK_STATUS,
	PHY_CFG,
	REGS_FOR_FALCON_LANE_INDEX,
	REGS_FOR_TSCF_LANE_INDEX,
	REGS_FOR_EAGLE_LANE_INDEX,
	REGS_FOR_TSCE_LANE_INDEX,
	ASIC_TYPE,
	REGS_FOR_PHY_MODE_CORE_PORT,
	GRC_REGS
};

static const char *const s_mdump2_asic_name[] = { "Unknown", "BB", "AH" };

static const char *const s_mdump2_regval_strs[] = {
	"Number of ports =" /* NUMBER_OF_PORTS */ ,
	"Port[%u] link status =" /* PORT_LINK_STATUS */ ,
	"Registers for PHY CFG:" /* PHY_CFG */ ,
	"Registers for Falcon lane #%u:" /* REGS_FOR_FALCON_LANE_INDEX */ ,
	"Registers for TSCF lane #%u:" /* REGS_FOR_TSCF_LANE_INDEX */ ,
	"Registers for Eagle lane #%u:" /* REGS_FOR_EAGLE_LANE_INDEX */ ,
	"Registers for TSCE lane #%u:" /* REGS_FOR_TSCE_LANE_INDEX */ ,
	"ASIC type =" /* ASIC_TYPE */ ,
	"Registers for PHY mode core port #%u:"
	    /* REGS_FOR_PHY_MODE_CORE_PORT */ ,
	"GRC registers:" /* GRC_REGS */ ,
};

static const char *const months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char *const days[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static u8 days_per_month[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static void qed_epoch_to_date(u32 epoch, char tz, char *buf)
{
	int hr, min, sec;
	int yr = 1970;
	int mnt = 0;
	int day = 1;
	int d = 0;

	epoch += (tz * SEC_PER_HOUR);

	for (;;) {
		if (!(epoch / SEC_PER_YEAR))
			break;
		epoch -= SEC_PER_YEAR;
		d += 365;
		if (isleap(yr)) {
			epoch -= SEC_PER_DAY;
			d++;
		}
		yr++;
	}

	if (isleap(yr))
		days_per_month[1] = 29;

	for (;;) {
		if (!(epoch / (SEC_PER_DAY * days_per_month[mnt])))
			break;
		d += days_per_month[mnt];
		epoch -= SEC_PER_DAY * days_per_month[mnt];
		mnt++;
	}

	for (;;) {
		if (!(epoch / SEC_PER_DAY))
			break;
		epoch -= SEC_PER_DAY;
		day++;
	}

	hr = epoch / SEC_PER_HOUR;
	min = (epoch % SEC_PER_HOUR) / 60;
	sec = (epoch % SEC_PER_HOUR) % 60;

	d = (d + 3 + day) % 7;

	scnprintf(buf,
		  MAX_CHAR_PER_LINE,
		  "%s %s %u %u:%.2u:%.2u %s %u",
		  days[d],
		  months[mnt],
		  day,
		  (hr >= 12) ? (hr - 12) : hr,
		  min, sec, (hr >= 12) ? "PM" : "AM", yr);
};

/**
 * Print mdump2 main header. Returns Bytes printed
 */
static bool qed_parse_mdump2_log_hdr(struct log_hdr_stc *hdr,
				     char *res_buf, u32 * res_off)
{
	static char const *reason[] =
	    { "Internal", "External", "Aged resource" };
	char str[64];
	u32 i;

	*res_off += STR_APPEND(res_buf, *res_off,
			       "Log actual size = %u\n", hdr->log_size);
	*res_off += STR_APPEND(res_buf, *res_off,
			       "Storm fw version = %u.%u.%u.%u\n",
			       (((hdr->storm_fw_ver) >> 24) & 0xff),
			       (((hdr->storm_fw_ver) >> 16) & 0xff),
			       (((hdr->storm_fw_ver) >> 8) & 0xff),
			       ((hdr->storm_fw_ver) & 0xff));
	*res_off += STR_APPEND(res_buf, *res_off,
			       "Storm tools version = %u.%u.%u.%u\n",
			       (((hdr->storm_tools_ver) >> 24) & 0xff),
			       (((hdr->storm_tools_ver) >> 16) & 0xff),
			       (((hdr->storm_tools_ver) >> 8) & 0xff),
			       ((hdr->storm_tools_ver) & 0xff));
	qed_epoch_to_date(hdr->storm_timestamp, 2, str);
	*res_off += STR_APPEND(res_buf, *res_off,
			       "Storm timestamps = %x(%s)\n",
			       hdr->storm_timestamp, str);
	*res_off += STR_APPEND(res_buf, *res_off,
			       "MFW version = %u.%u.%u.%u\n",
			       (((hdr->mfw_ver) >> 24) & 0xff),
			       (((hdr->mfw_ver) >> 16) & 0xff),
			       (((hdr->mfw_ver) >> 8) & 0xff),
			       ((hdr->mfw_ver) & 0xff));
	*res_off += STR_APPEND(res_buf, *res_off,
			       "MBA version = %u.%u.%u.%u\n",
			       (((hdr->mba_ver) >> 24) & 0xff),
			       (((hdr->mba_ver) >> 16) & 0xff),
			       (((hdr->mba_ver) >> 8) & 0xff),
			       ((hdr->mba_ver) & 0xff));
	*res_off += STR_APPEND(res_buf, *res_off,
			       "%s %s\n", s_mdump2_regval_strs[ASIC_TYPE],
			       s_mdump2_asic_name[hdr->chip]);
	*res_off += STR_APPEND(res_buf, *res_off,
			       "Number of active functions = %u\n",
			       hdr->num_of_func);
	*res_off += STR_APPEND(res_buf, *res_off,
			       "%s %u\n",
			       s_mdump2_regval_strs[NUMBER_OF_PORTS],
			       hdr->num_of_ports);

	for (i = 0; i < hdr->num_of_ports; i++) {
		char port_link_status_str[64];

		scnprintf(port_link_status_str, sizeof(port_link_status_str),
			  s_mdump2_regval_strs[PORT_LINK_STATUS], i);
		*res_off += STR_APPEND(res_buf, *res_off,
				       "%s %x\n", port_link_status_str,
				       hdr->link_status[i]);
	}

	*res_off += STR_APPEND(res_buf, *res_off,
			       "Power mode = %x\n", hdr->power_mode);
	qed_epoch_to_date(hdr->log_epoch, 2, str);
	*res_off += STR_APPEND(res_buf, *res_off,
			       "Log capture date = %s\n", str);
	*res_off += STR_APPEND(res_buf, *res_off,
			       "Reason = %x [", (hdr->reason & 0xffff));
	for (i = 0; i < 3; i++)
		if (hdr->reason & BIT(i))
			*res_off += STR_APPEND(res_buf, *res_off, "%s",
					       reason[i]);

	*res_off += STR_APPEND(res_buf, *res_off, "]\n\n");

	return true;
}

static void qed_print_data(u32 * buf, u32 len, char *res_buf, u32 * res_off)
{
	u32 i;

	for (i = 0; i < (len >> 2); i++) {
		*res_off += STR_APPEND(res_buf, *res_off,
				       "%08x%s", buf[i],
				       (!((i + 1) % LINE_SIZE)) ? "\n" : " ");
	}

	if (i % LINE_SIZE)
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
}

static void qed_print_tlv_unused_type0(struct tlv_stc *tlv,
				       char *res_buf, u32 * res_off)
{
	u32 *value;

	if (tlv->len > 4) {
		value = (u32 *) (((char *)tlv) + sizeof(struct tlv_stc));
		*res_off += STR_APPEND(res_buf, *res_off,
				       "UNUSED Type0 storm #%u:\n", value[0]);
		qed_print_data(&value[1], tlv->len - 4, res_buf, res_off);
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
	}
}

static void qed_print_tlv_unused_type1(struct tlv_stc *tlv,
				       char *res_buf, u32 * res_off)
{
	u32 *value;

	if (tlv->len) {
		value = (u32 *) (((char *)tlv) + sizeof(struct tlv_stc));
		*res_off += STR_APPEND(res_buf, *res_off,
				       "UNUSED Type1 data :\n");
		qed_print_data(&value[0], tlv->len, res_buf, res_off);
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
	}
}

static void qed_print_tlv_unused_type2(struct tlv_stc *tlv,
				       char *res_buf, u32 * res_off)
{
	u32 *value;

	if (tlv->len) {
		value = (u32 *) (((char *)tlv) + sizeof(struct tlv_stc));
		*res_off += STR_APPEND(res_buf, *res_off,
				       "UNUSED Type2 data:\n");
		qed_print_data(&value[0], tlv->len, res_buf, res_off);
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
	}
}

static void qed_print_static_init(struct tlv_stc *tlv,
				  char *res_buf, u32 * res_off)
{
	u32 *value;

	if (tlv->len) {
		value = (u32 *) (((char *)tlv) + sizeof(struct tlv_stc));
		*res_off += STR_APPEND(res_buf, *res_off, "MFW Static init:\n");
		qed_print_data(&value[0], tlv->len, res_buf, res_off);
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
	}
}

static void qed_print_phy_cfg(struct tlv_stc *tlv, char *res_buf, u32 * res_off)
{
	u32 *value;

	if (tlv->len) {
		value = (u32 *) (((char *)tlv) + sizeof(struct tlv_stc));
		*res_off += STR_APPEND(res_buf, *res_off,
				       "%s\n", s_mdump2_regval_strs[PHY_CFG]);
		qed_print_data(&value[0], tlv->len, res_buf, res_off);
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
	}
}

static void qed_print_public(struct tlv_stc *tlv, char *res_buf, u32 * res_off)
{
	u32 *value;

	if (tlv->len) {
		value = (u32 *) (((char *)tlv) + sizeof(struct tlv_stc));
		*res_off += STR_APPEND(res_buf, *res_off,
				       "MFW function public:\n");
		qed_print_data(&value[0], tlv->len, res_buf, res_off);
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
	}
}

static void qed_print_private(struct tlv_stc *tlv, char *res_buf, u32 * res_off)
{
	u32 *value;

	if (tlv->len) {
		value = (u32 *) (((char *)tlv) + sizeof(struct tlv_stc));
		*res_off += STR_APPEND(res_buf, *res_off, "MFW private:\n");
		qed_print_data(&value[0], tlv->len, res_buf, res_off);
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
	}
}

static void qed_print_grc_regs(struct tlv_stc *tlv,
			       char *res_buf, u32 * res_off)
{
	u32 *value;

	if (tlv->len) {
		value = (u32 *) (((char *)tlv) + sizeof(struct tlv_stc));
		*res_off += STR_APPEND(res_buf, *res_off, "%s\n",
				       s_mdump2_regval_strs[GRC_REGS]);
		qed_print_data(&value[0], tlv->len, res_buf, res_off);
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
	}
}

static void qed_print_pf_regs(struct tlv_stc *tlv, char *res_buf, u32 * res_off)
{
	u32 *value;

	if (tlv->len > 4) {
		value = (u32 *) (((char *)tlv) + sizeof(struct tlv_stc));
		*res_off += STR_APPEND(res_buf, *res_off,
				       "Registers for function #%u:\n",
				       value[0]);
		qed_print_data(&value[1], tlv->len - 4, res_buf, res_off);
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
	}
}

static void qed_print_port_regs(struct tlv_stc *tlv,
				char *res_buf, u32 * res_off)
{
	u32 *value;

	if (tlv->len > 4) {
		value = (u32 *) (((char *)tlv) + sizeof(struct tlv_stc));
		*res_off += STR_APPEND(res_buf, *res_off,
				       "Registers for port #%u:\n", value[0]);
		qed_print_data(&value[1], tlv->len - 4, res_buf, res_off);
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
	}
}

static void qed_print_eagle_regs(struct tlv_stc *tlv,
				 char *res_buf, u32 * res_off)
{
	u32 *value;

	if (tlv->len > 4) {
		char regs_for_eagle_lane_index[MAX_CHAR_PER_LINE];

		value = (u32 *) (((char *)tlv) + sizeof(struct tlv_stc));
		scnprintf(regs_for_eagle_lane_index,
			  sizeof(regs_for_eagle_lane_index),
			  s_mdump2_regval_strs[REGS_FOR_EAGLE_LANE_INDEX],
			  value[0]);
		*res_off += STR_APPEND(res_buf, *res_off,
				       "%s\n", regs_for_eagle_lane_index);
		qed_print_data(&value[1], tlv->len - 4, res_buf, res_off);
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
	}
}

static void qed_print_tsce_regs(struct tlv_stc *tlv,
				char *res_buf, u32 * res_off)
{
	u32 *value;

	if (tlv->len > 4) {
		char regs_for_tsce_lane_index[MAX_CHAR_PER_LINE];

		value = (u32 *) (((char *)tlv) + sizeof(struct tlv_stc));
		scnprintf(regs_for_tsce_lane_index,
			  sizeof(regs_for_tsce_lane_index),
			  s_mdump2_regval_strs[REGS_FOR_TSCE_LANE_INDEX],
			  value[0]);
		*res_off += STR_APPEND(res_buf, *res_off,
				       "%s\n", regs_for_tsce_lane_index);
		qed_print_data(&value[1], tlv->len - 4, res_buf, res_off);
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
	}
}

static void qed_print_falcon_regs(struct tlv_stc *tlv,
				  char *res_buf, u32 * res_off)
{
	u32 *value;

	if (tlv->len > 4) {
		char regs_for_falcon_lane_index[MAX_CHAR_PER_LINE];

		value = (u32 *) (((char *)tlv) + sizeof(struct tlv_stc));
		scnprintf(regs_for_falcon_lane_index,
			  sizeof(regs_for_falcon_lane_index),
			  s_mdump2_regval_strs[REGS_FOR_FALCON_LANE_INDEX],
			  value[0]);
		*res_off += STR_APPEND(res_buf, *res_off,
				       "%s\n", regs_for_falcon_lane_index);
		qed_print_data(&value[1], tlv->len - 4, res_buf, res_off);
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
	}
}

static void qed_print_tscf_regs(struct tlv_stc *tlv,
				char *res_buf, u32 * res_off)
{
	u32 *value;

	if (tlv->len > 4) {
		value = (u32 *) (((char *)tlv) + sizeof(struct tlv_stc));

		*res_off += STR_APPEND(res_buf, *res_off,
				       s_mdump2_regval_strs
				       [REGS_FOR_TSCF_LANE_INDEX], value[0]);
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
		qed_print_data(&value[1], tlv->len - 4, res_buf, res_off);
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
	}
}

static void qed_print_clxport_regs(struct tlv_stc *tlv,
				   char *res_buf, u32 * res_off)
{
	u32 *value;

	if (tlv->len > 4) {
		value = (u32 *) (((char *)tlv) + sizeof(struct tlv_stc));

		*res_off += STR_APPEND(res_buf, *res_off,
				       s_mdump2_regval_strs
				       [REGS_FOR_PHY_MODE_CORE_PORT], value[0]);
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
		qed_print_data(&value[1], tlv->len - 4, res_buf, res_off);
		*res_off += STR_APPEND(res_buf, *res_off, "\n");
	}
}

struct dump_type_handler_stc {
	u32 type;
	char *type_str;
	void (*handler) (struct tlv_stc *, char *res_buf, u32 * res_off);
};

static struct dump_type_handler_stc dump_handlers[] = {
	{TLV_UNUSED_TYPE0, "tlv_unused_type0",
	 &qed_print_tlv_unused_type0},
	{TLV_UNUSED_TYPE1, "tlv_unused_type1",
	 &qed_print_tlv_unused_type1},
	{TLV_UNUSED_TYPE2, "tlv_unused_type2",
	 &qed_print_tlv_unused_type2},
	{STATIC_INIT_TYPE, "static_init", &qed_print_static_init},
	{FUNC_PUBLIC_TYPE, "func_public", &qed_print_public},
	{FUNC_PRIVATE_TYPE, "func_private", &qed_print_private},
	{GRC_REGS_TYPE, "grc_dump", &qed_print_grc_regs},
	{PF_REGS_TYPE, "pf_regs", &qed_print_pf_regs},
	{PORT_REGS_TYPE, "port_dump", &qed_print_port_regs},
	{EAGLE_LANE_TYPE, "eagle_dump", &qed_print_eagle_regs},
	{FALCON_LANE_TYPE, "falcon_dump", &qed_print_falcon_regs},
	{TSCE_LANE_TYPE, "tsce_dump", &qed_print_tsce_regs},
	{TSCF_LANE_TYPE, "tscf_dump", &qed_print_tscf_regs},
	{CXLPORT_TYPE, "phymod_core_dump", &qed_print_clxport_regs},
	{PHY_CFG_TYPE, "phy_cfg_dump", &qed_print_phy_cfg},
	{EOD_OF_LOG, NULL, NULL},
};

/**
 * Print mdump2 main data. Returns Bytes printed
 */
static bool qed_parse_mdump2_log_data(u32 * dump_buf,
				      char *res_buf, u32 * res_off)
{
	struct tlv_stc *tlv = (struct tlv_stc *)dump_buf;
	struct dump_type_handler_stc *p;

	*res_off += STR_APPEND(res_buf, *res_off, "Log 0 data:\n");

	while (tlv->type != EOD_OF_LOG) {
		for (p = dump_handlers; p->handler != NULL; p++)
			if (p->type == tlv->type) {
				p->handler(tlv, res_buf, res_off);
				break;
			}
		if (!p->handler) {
			*res_off += STR_APPEND(res_buf,
					       *res_off,
					       "tlv type %08x unknown - aborting\n",
					       tlv->type);
			return false;
		}
		tlv = (struct tlv_stc *)((char *)(tlv + 1) + tlv->len);
	}
	*res_off += STR_APPEND(res_buf, *res_off, "\n");

	return true;
}

static bool qed_mdump2_regval_buf_get_num_ports(char *regval_buf,
						u32 * num_ports)
{
	char *num_ports_pos = strstr(regval_buf,
				     s_mdump2_regval_strs[NUMBER_OF_PORTS]);

	if (!num_ports_pos)
		return false;

	num_ports_pos += strlen(s_mdump2_regval_strs[NUMBER_OF_PORTS]);
	if (sscanf(num_ports_pos, "%u", num_ports) != 1)
		return false;

	return true;
}

static bool qed_is_bb_chip(char *regval_buf)
{
	char *asic_type_pos = strstr(regval_buf,
				     s_mdump2_regval_strs[ASIC_TYPE]);
	char asic_type[64];

	if (!asic_type_pos)
		return false;

	asic_type_pos += strlen(s_mdump2_regval_strs[ASIC_TYPE]);
	if (sscanf(asic_type_pos, "%64s", asic_type) != 1)
		return false;

	if (!strcmp(asic_type, s_mdump2_asic_name[1]))
		return true;

	return false;
}

static bool qed_is_k2_chip(char *regval_buf)
{
	char *asic_type_pos = strstr(regval_buf,
				     s_mdump2_regval_strs[ASIC_TYPE]);
	char asic_type[64];

	if (!asic_type_pos)
		return false;

	asic_type_pos += strlen(s_mdump2_regval_strs[ASIC_TYPE]);
	if (sscanf(asic_type_pos, "%64s", asic_type) != 1)
		return false;

	if (!strcmp(asic_type, s_mdump2_asic_name[2]))
		return true;

	return false;
}

/**
 * Search the register address in the GRC_REGS groups within the regval_buf:
 */
static bool qed_mdump2_reg_read(char *regval_buf,
				const char *reg_group_name,
				u32 reg_addr, u32 * val)
{
	bool search_new_grc_regs_group = true;
	char *grc_regs_pos = regval_buf;
	u32 num_rvs_read;
	u8 rv_index;
	u32 num_rvs;
	char *nl_pos;
	char *sp_pos;
	u32 rv[8];

	do {
		if (search_new_grc_regs_group) {
			grc_regs_pos = strstr(grc_regs_pos, reg_group_name);

			if (!grc_regs_pos)
				return false;

			/* Advance the pointer to the start of the data: +1 is
			 * for newline
			 */
			grc_regs_pos += (strlen(reg_group_name) + 1);

			search_new_grc_regs_group = false;
		}

		/* Determine if this line is empty (end of GRC_REGS group * by
		 * measuring the distance between a ' ' (space) and '\n'
		 * (newline). The position of '\n' must be larger than the
		 * position of ' '.
		 *
		 * GRC registers:
		 * 00700000 00003841 00700004 00011869 00700008 00000000 0070000c 000001fe
		 */
		nl_pos = strstr(grc_regs_pos, "\n");
		if (!nl_pos)
			break;
		sp_pos = strstr(grc_regs_pos, " ");
		if (nl_pos <= sp_pos)
			break;
		/* Read reg-val pairs:
		 * 8 chars per value, 2 form a reg-val
		 */
		num_rvs = (u32) ((nl_pos - grc_regs_pos) / 8);

		/* Must be reg and vals (pairs) */
		if (num_rvs % 2)
			return false;
		num_rvs /= 2;

		switch (num_rvs) {
		case 4:
			num_rvs_read = sscanf(grc_regs_pos,
					      "%x %x %x %x %x %x %x %x",
					      &rv[0], &rv[1], &rv[2], &rv[3],
					      &rv[4], &rv[5], &rv[6], &rv[7]);
			if (num_rvs_read != 8)
				return false;
			break;
		case 3:
			num_rvs_read = sscanf(grc_regs_pos, "%x %x %x %x %x %x",
					      &rv[0], &rv[1], &rv[2],
					      &rv[3], &rv[4], &rv[5]);
			if (num_rvs_read != 6)
				return false;
			break;
		case 2:
			num_rvs_read = sscanf(grc_regs_pos, "%x %x %x %x",
					      &rv[0], &rv[1], &rv[2], &rv[3]);
			if (num_rvs_read != 4)
				return false;
			rv[7] = rv[6] = rv[5] = rv[4] = 0;
			break;
		case 1:
			num_rvs_read = sscanf(grc_regs_pos, "%x %x",
					      &rv[0], &rv[1]);
			if (num_rvs_read != 2)
				return false;
			rv[7] = rv[6] = rv[5] = rv[4] = rv[3] = rv[2] = 0;
			break;
		default:
			return false;
		}

		/* Line read, now search for the address required.
		 * If it is in this line, set value and return true.
		 */
		for (rv_index = 0; rv_index < num_rvs_read; rv_index += 2)
			if (rv[rv_index] == reg_addr) {
				*val = rv[rv_index + 1];
				return true;
			}

		/* continue to next line. If it's the end of group (another \n),
		 * identify it.
		 */
		grc_regs_pos = nl_pos + 1;

		if (grc_regs_pos[0] == '\n')
			search_new_grc_regs_group = true;
	} while (nl_pos > sp_pos);

	/* If reached here - not found */
	return false;
}

/**
 * Note: These are 64-bit regs, first value after address is the HI,
 * and the second is the LO, only the LO part is returned
 */
static bool qed_read_phy_mode_core_port_regs(char *regval_buf,
					     u32 port,
					     u32 reg_addr,
					     u32 reg_index, u32 * val)
{
	u32 num_rvs_read = 0, expected_num_rvs = 1;
	char regs_phy_mode_core_port[MAX_CHAR_PER_LINE];
	char *phy_mode_core_port_pos;
	u32 rv[1 * 3];		/* regvals (1 triplet) */

	scnprintf(regs_phy_mode_core_port, sizeof(regs_phy_mode_core_port),
		  s_mdump2_regval_strs[REGS_FOR_PHY_MODE_CORE_PORT], port);

	phy_mode_core_port_pos = strstr(regval_buf, regs_phy_mode_core_port);

	if (!phy_mode_core_port_pos)
		return false;

	/* There is exactly 1 reg-val triplet, and index 0
	 * (first and last, 1st triplet) is the required one
	 */
	phy_mode_core_port_pos += strlen(regs_phy_mode_core_port);
	num_rvs_read = sscanf(phy_mode_core_port_pos,
			      "%x %x %x", &rv[0], &rv[1], &rv[2]);
	expected_num_rvs = 3;

	if (num_rvs_read != expected_num_rvs)
		return false;

	/* Verify address of 1st triplet: */
	if (rv[3 * reg_index] != reg_addr)
		return false;
	*val = rv[3 * reg_index + 2];

	return true;
}

static u32 qed_read_pcs_status_reg(struct mdump2_cfg_params *mdump2_cfg,
				   char *regval_buf, u32 port, u32 mode)
{
	struct link_dump_port *ld_port;
	u8 port_lane_map;
	u32 reg_addr;
	u32 lane = 0;
	u32 val;

	port_lane_map = mdump2_cfg->linkdump_port_lane_map.port[port];
	ld_port = &mdump2_cfg->link_dump_results.port[port];

	switch (port_lane_map) {
		/* FALCON LANE MAPPINGS */
	case 0x1:
		lane = 0;
		mdump2_cfg->link_dump_results.is_falcon[port] = true;
		break;
	case 0x2:
		lane = 1;
		mdump2_cfg->link_dump_results.is_falcon[port] = true;
		break;
	case 0x4:
		lane = 2;
		mdump2_cfg->link_dump_results.is_falcon[port] = true;
		break;
	case 0x8:
		lane = 3;
		mdump2_cfg->link_dump_results.is_falcon[port] = true;
		break;
	case 0x3:
		lane = 0;
		mdump2_cfg->link_dump_results.is_falcon[port] = true;
		break;
	case 0xc:
		lane = 2;
		mdump2_cfg->link_dump_results.is_falcon[port] = true;
		break;
	case 0xf:
		lane = 0;
		mdump2_cfg->link_dump_results.is_falcon[port] = true;
		break;

		/* EAGLE LANE MAPPINGS */
	case 0x10:
		lane = 0;
		mdump2_cfg->link_dump_results.is_falcon[port] = false;
		break;
	case 0x20:
		lane = 1;
		mdump2_cfg->link_dump_results.is_falcon[port] = false;
		break;
	case 0x40:
		lane = 2;
		mdump2_cfg->link_dump_results.is_falcon[port] = false;
		break;
	case 0x80:
		lane = 3;
		mdump2_cfg->link_dump_results.is_falcon[port] = false;
		break;
	case 0x30:
		lane = 0;
		mdump2_cfg->link_dump_results.is_falcon[port] = false;
		break;
	case 0xc0:
		lane = 2;
		mdump2_cfg->link_dump_results.is_falcon[port] = false;
		break;
	case 0xf0:
		lane = 0;
		mdump2_cfg->link_dump_results.is_falcon[port] = false;
		break;
	default:
		break;
	}

	if (mode == PCS_STATUS_REG_LINK) {
		if (qed_is_bb_chip(regval_buf)) {
			u32 tcsf_or_tcse_marker;
			char group_name[MAX_CHAR_PER_LINE];

			if (mdump2_cfg->link_dump_results.is_falcon[port]) {
				reg_addr = 0xc161;
				tcsf_or_tcse_marker = REGS_FOR_TSCF_LANE_INDEX;
			} else {
				reg_addr = 0xc154;	/* Eagle */
				tcsf_or_tcse_marker = REGS_FOR_TSCE_LANE_INDEX;
			}
			scnprintf(group_name, sizeof(group_name),
				  s_mdump2_regval_strs[tcsf_or_tcse_marker],
				  lane);

			if (!qed_mdump2_reg_read(regval_buf, group_name,
						 reg_addr | (lane << 16), &val))
				return
				    DBG_STATUS_MDUMP2_ERROR_READING_LANE_REGS;

			/* Check for link_status bit - link up in pcs */
			if (val & 0x2)
				ld_port->pcs_status = PCS_STATUS_LINK_UP;
			else
				ld_port->pcs_status = PCS_STATUS_LINK_DOWN;
		} else if (qed_is_k2_chip(regval_buf)) {
			u32 ls_bar = (NWM_REG_PCS_LS0_K2 +
				      (port * NWM_REG_PCS_LS0_SIZE * 4));
			u32 hs_bar = (NWM_REG_PCS_HS0_K2 +
				      (port * NWM_REG_PCS_HS0_SIZE * 4));

			ld_port->pcs_status = PCS_STATUS_LINK_DOWN;

			if (!qed_mdump2_reg_read(regval_buf,
						 s_mdump2_regval_strs[GRC_REGS],
						 (hs_bar +
						  ETH_PCS10_50G_REG_STATUS1_K2),
						 &val))
				return
				    DBG_STATUS_MDUMP2_ERROR_READING_LANE_REGS;
			if (val & ETH_PCS10_50G_REG_STATUS1_PCS_RECEIVE_LINK_K2)
				ld_port->pcs_status = PCS_STATUS_LINK_UP;

			if (!qed_mdump2_reg_read(regval_buf,
						 s_mdump2_regval_strs[GRC_REGS],
						 (ls_bar +
						  ETH_PCS1G_REG_STATUS_K2),
						 &val))
				return
				    DBG_STATUS_MDUMP2_ERROR_READING_LANE_REGS;
			if (val & ETH_PCS1G_REG_STATUS_LINKSTATUS_K2)
				ld_port->pcs_status = PCS_STATUS_LINK_UP;
		} else {
			ld_port->pcs_status = PCS_STATUS_LINK_DOWN;
		}
	}

	if (mode == PCS_STATUS_REG_FAULT) {
		if (qed_is_bb_chip(regval_buf)) {
			/* Read CLMAC/XLMACs status reg for link
			 * CLMAC/XLMAC_RX_LSS_STATUS 0x60b
			 * Register defination and location is same for XLMAC
			 * and CLMAC
			 */
			qed_read_phy_mode_core_port_regs(regval_buf, port,
							 0x60b, 0, &val);

			if (val & 0x1)
				ld_port->pcs_status |= PCS_STATUS_LOCAL_FAULT;
			if (val & 0x2)
				ld_port->pcs_status |= PCS_STATUS_REMOTE_FAULT;
			if (val & 0x4)
				ld_port->pcs_status |=
				    PCS_STATUS_INTERRUPT_FAULT;
		} else {
			ld_port->pcs_status |= (PCS_STATUS_LOCAL_FAULT |
						PCS_STATUS_REMOTE_FAULT |
						PCS_STATUS_INTERRUPT_FAULT);
		}
	}
	return DBG_STATUS_OK;
}

/**
 * Save link status reported by mfw
 */
static bool qed_retrieve_status_from_mfw(struct mdump2_cfg_params *mdump2_cfg,
					 char *regval_buf, u32 port)
{
	struct link_dump_port *ld_port;
	char port_link_status_str[64];
	char *link_status_pos;

	ld_port = &mdump2_cfg->link_dump_results.port[port];

	scnprintf(port_link_status_str, sizeof(port_link_status_str),
		  s_mdump2_regval_strs[PORT_LINK_STATUS], port);
	link_status_pos = strstr(regval_buf, port_link_status_str);

	if (!link_status_pos)
		return false;

	link_status_pos += strlen(port_link_status_str);
	if (sscanf(link_status_pos, "%x", &ld_port->mfw_link_status) != 1)
		return false;

	return true;
}

/**
 * Verify consecutive registers are 0, from (including) reg_addr
 */
static bool qed_all_falcon_or_eagle_regs_0(char *regval_buf,
					   u32 lane,
					   u32 falcon_or_eagle_marker,
					   u32 reg_addr, u32 num_regs)
{
	char regs_for_falcon_or_eagle_lane_index[MAX_CHAR_PER_LINE];
	u32 rindex, val;

	scnprintf(regs_for_falcon_or_eagle_lane_index,
		  sizeof(regs_for_falcon_or_eagle_lane_index),
		  s_mdump2_regval_strs[falcon_or_eagle_marker], lane);

	/* Verify consecutive registers are 0: */
	for (rindex = 0; rindex < num_regs; rindex++) {
		if (!qed_mdump2_reg_read(regval_buf,
					 regs_for_falcon_or_eagle_lane_index,
					 (reg_addr | (lane << 16)) + rindex,
					 &val))
			return false;
		if (val)
			return false;
	}

	return true;
}

static bool qed_read_phy_cfg(struct mdump2_cfg_params *mdump2_cfg,
			     char *regval_buf)
{
	char *phy_cfg_pos = strstr(regval_buf, s_mdump2_regval_strs[PHY_CFG]);
	int num_read_items;
	int port;

	if (!phy_cfg_pos)
		return false;

	phy_cfg_pos += strlen(s_mdump2_regval_strs[PHY_CFG]);
	num_read_items = sscanf(phy_cfg_pos,
				"%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
				&mdump2_cfg->phy_cfg.core_cfg,
				&mdump2_cfg->phy_cfg.f_lane_cfg1,
				&mdump2_cfg->phy_cfg.e_lane_cfg1,
				&mdump2_cfg->phy_cfg.generic_cont1,
				&mdump2_cfg->phy_cfg.link_change_count[0],
				&mdump2_cfg->phy_cfg.link_change_count[1],
				&mdump2_cfg->phy_cfg.link_change_count[2],
				&mdump2_cfg->phy_cfg.link_change_count[3],
				&mdump2_cfg->phy_cfg.lfa_status[0],
				&mdump2_cfg->phy_cfg.lfa_status[1],
				&mdump2_cfg->phy_cfg.lfa_status[2],
				&mdump2_cfg->phy_cfg.lfa_status[3],
				&mdump2_cfg->phy_cfg.transceiver_data[0],
				&mdump2_cfg->phy_cfg.transceiver_data[1],
				&mdump2_cfg->phy_cfg.transceiver_data[2],
				&mdump2_cfg->phy_cfg.transceiver_data[3]);
	if (num_read_items != sizeof(struct mdump_phy_cfg) / sizeof(u32))
		return false;

	/* Copy to link_dump_results (used in linkdump): */
	for (port = 0; port < 4; port++) {
		mdump2_cfg->link_dump_results.port[port].link_change_count =
		    mdump2_cfg->phy_cfg.link_change_count[port];
		mdump2_cfg->link_dump_results.port[port].lfa_status =
		    mdump2_cfg->phy_cfg.lfa_status[port];
		mdump2_cfg->link_dump_results.port[port].transceiver_data =
		    mdump2_cfg->phy_cfg.transceiver_data[port];
	}

	return true;
}

static enum dbg_status qed_display_chip_code(struct mdump2_cfg_params
					     *mdump2_cfg, char *regval_buf,
					     char *res_buf, u32 * res_off)
{
	union link_dump_port_lane_map_u *linkdump_port_lane_map;
	u32 core_cfg, pll_mode;

	*res_off += STR_APPEND(res_buf, *res_off, "\n\nChip mode: ");

	/* Read core_cfg: f[alcon]_lane_cfg1 and e[agle]_lane_cfg */
	if (!qed_read_phy_cfg(mdump2_cfg, regval_buf))
		return DBG_STATUS_MDUMP2_ERROR_READING_PHY_CFG;

	core_cfg = mdump2_cfg->phy_cfg.core_cfg;
	linkdump_port_lane_map = &mdump2_cfg->linkdump_port_lane_map;

	switch (core_cfg & NVM_CFG1_GLOB_NETWORK_PORT_MODE_MASK) {
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_2X40G:
		linkdump_port_lane_map->lane_map.port0 =
		    LANE_0_MASK | LANE_1_MASK | LANE_2_MASK | LANE_3_MASK;
		linkdump_port_lane_map->lane_map.port1 =
		    EAGLE_LANE_0_MASK | EAGLE_LANE_1_MASK |
		    EAGLE_LANE_2_MASK | EAGLE_LANE_3_MASK;
		linkdump_port_lane_map->lane_map.port2 = LANE_NONE_MASK;
		linkdump_port_lane_map->lane_map.port3 = LANE_NONE_MASK;
		*res_off += STR_APPEND(res_buf, *res_off, "2x40 ");
		mdump2_cfg->link_dump_results.mode = CHIP_MODE_2X40;
		break;

	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X50G:
		linkdump_port_lane_map->lane_map.port0 = LANE_0_MASK |
		    LANE_1_MASK;
		linkdump_port_lane_map->lane_map.port1 = LANE_2_MASK |
		    LANE_3_MASK;
		linkdump_port_lane_map->lane_map.port2 = LANE_NONE_MASK;
		linkdump_port_lane_map->lane_map.port3 = LANE_NONE_MASK;
		*res_off += STR_APPEND(res_buf, *res_off, "2x50 ");
		mdump2_cfg->link_dump_results.mode = CHIP_MODE_2X50;
		break;

	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_1X100G:
		linkdump_port_lane_map->lane_map.port0 =
		    LANE_0_MASK | LANE_1_MASK | LANE_2_MASK | LANE_3_MASK;
		linkdump_port_lane_map->lane_map.port1 = LANE_NONE_MASK;
		linkdump_port_lane_map->lane_map.port2 = LANE_NONE_MASK;
		linkdump_port_lane_map->lane_map.port3 = LANE_NONE_MASK;
		*res_off += STR_APPEND(res_buf, *res_off, "1x100 ");
		mdump2_cfg->link_dump_results.mode = CHIP_MODE_1X100;
		break;

	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_4X10G_F:
		linkdump_port_lane_map->lane_map.port0 = LANE_0_MASK;
		linkdump_port_lane_map->lane_map.port1 = LANE_1_MASK;
		linkdump_port_lane_map->lane_map.port2 = LANE_2_MASK;
		linkdump_port_lane_map->lane_map.port3 = LANE_3_MASK;

		if (qed_is_bb_chip(regval_buf)) {
			/* Check for 4x10 vs 4x25 there is only 1 PLL so
			 * don't need to check for each port
			 * Read pll_mode (port 0):
			 */
			char group_name[MAX_CHAR_PER_LINE];

			scnprintf(group_name, sizeof(group_name),
				  s_mdump2_regval_strs
				  [REGS_FOR_FALCON_LANE_INDEX], 0);

			if (!qed_mdump2_reg_read(regval_buf, group_name,
						 0x0800d147, &pll_mode))
				return DBG_STATUS_MDUMP2_ERROR_READING_PLL_MODE;

			switch (pll_mode) {
			case 0x4:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "4x10GF ");
				mdump2_cfg->link_dump_results.mode =
				    CHIP_MODE_4X10GF;
				break;
			case 0x7:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "4x25GF ");
				mdump2_cfg->link_dump_results.mode =
				    CHIP_MODE_4X25GF;
				break;
			default:
				break;
			}
		} else {
			*res_off += STR_APPEND(res_buf, *res_off, "4x10G ");
			mdump2_cfg->link_dump_results.mode = CHIP_MODE_4X10GF;
		}
		break;

	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_4X25G:
		linkdump_port_lane_map->lane_map.port0 = LANE_0_MASK;
		linkdump_port_lane_map->lane_map.port1 = LANE_1_MASK;
		linkdump_port_lane_map->lane_map.port2 = LANE_2_MASK;
		linkdump_port_lane_map->lane_map.port3 = LANE_3_MASK;
		*res_off += STR_APPEND(res_buf, *res_off, "4x25G ");
		mdump2_cfg->link_dump_results.mode = CHIP_MODE_4X25G;
		break;

	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_4X10G_E:
		linkdump_port_lane_map->lane_map.port0 = EAGLE_LANE_0_MASK;
		linkdump_port_lane_map->lane_map.port1 = EAGLE_LANE_1_MASK;
		linkdump_port_lane_map->lane_map.port2 = EAGLE_LANE_2_MASK;
		linkdump_port_lane_map->lane_map.port3 = EAGLE_LANE_3_MASK;
		*res_off += STR_APPEND(res_buf, *res_off, "4x10GE ");
		mdump2_cfg->link_dump_results.mode = CHIP_MODE_4X10GE;
		break;

	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_4X20G:
		linkdump_port_lane_map->lane_map.port0 = LANE_0_MASK;
		linkdump_port_lane_map->lane_map.port1 = LANE_1_MASK;
		linkdump_port_lane_map->lane_map.port2 = LANE_2_MASK;
		linkdump_port_lane_map->lane_map.port3 = LANE_3_MASK;
		*res_off += STR_APPEND(res_buf, *res_off, "4x20 ");
		mdump2_cfg->link_dump_results.mode = CHIP_MODE_4X20;
		break;

	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X25G:
		linkdump_port_lane_map->lane_map.port0 = LANE_0_MASK;
		linkdump_port_lane_map->lane_map.port1 = LANE_1_MASK;
		linkdump_port_lane_map->lane_map.port2 = LANE_NONE_MASK;
		linkdump_port_lane_map->lane_map.port3 = LANE_NONE_MASK;
		*res_off += STR_APPEND(res_buf, *res_off, "2x25G ");
		mdump2_cfg->link_dump_results.mode = CHIP_MODE_2X25G;
		break;

	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X10G:
		linkdump_port_lane_map->lane_map.port0 = LANE_0_MASK;
		linkdump_port_lane_map->lane_map.port1 = LANE_1_MASK;
		linkdump_port_lane_map->lane_map.port2 = LANE_NONE_MASK;
		linkdump_port_lane_map->lane_map.port3 = LANE_NONE_MASK;
		*res_off += STR_APPEND(res_buf, *res_off, "2x10G ");
		mdump2_cfg->link_dump_results.mode = CHIP_MODE_2X10G;
		break;

	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_1X40G:
		linkdump_port_lane_map->lane_map.port0 =
		    LANE_0_MASK | LANE_1_MASK | LANE_2_MASK | LANE_3_MASK;
		linkdump_port_lane_map->lane_map.port1 = LANE_NONE_MASK;
		linkdump_port_lane_map->lane_map.port2 = LANE_NONE_MASK;
		linkdump_port_lane_map->lane_map.port3 = LANE_NONE_MASK;
		*res_off += STR_APPEND(res_buf, *res_off, "1x40 ");
		mdump2_cfg->link_dump_results.mode = CHIP_MODE_1X40;
		break;

	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_1X25G:
		linkdump_port_lane_map->lane_map.port0 = LANE_0_MASK;
		linkdump_port_lane_map->lane_map.port1 = LANE_NONE_MASK;
		linkdump_port_lane_map->lane_map.port2 = LANE_NONE_MASK;
		linkdump_port_lane_map->lane_map.port3 = LANE_NONE_MASK;
		*res_off += STR_APPEND(res_buf, *res_off, "1x25G ");
		mdump2_cfg->link_dump_results.mode = CHIP_MODE_1X25G;
		break;

	default:
		linkdump_port_lane_map->lane_map.port0 = LANE_NONE_MASK;
		linkdump_port_lane_map->lane_map.port1 = LANE_NONE_MASK;
		linkdump_port_lane_map->lane_map.port2 = LANE_NONE_MASK;
		linkdump_port_lane_map->lane_map.port3 = LANE_NONE_MASK;
		*res_off += STR_APPEND(res_buf, *res_off, "Unknown ");
		mdump2_cfg->link_dump_results.mode = CHIP_MODE_UNKNOWN;
		break;
	}
	return DBG_STATUS_OK;
}

/**
 * Get master serdes lane associated with the port
 */
static bool qed_get_master_lane_for_port(struct mdump2_cfg_params *mdump2_cfg,
					 char *regval_buf, u32 port, u32 * lane)
{
	u32 lane_map;

	/* Lane map NVM configuation setting is different for BB & AH
	 * BB is different for low speed and high speed lanes.
	 */
	if (qed_is_bb_chip(regval_buf)) {
		if (mdump2_cfg->link_dump_results.is_falcon[port] == true)
			/* Read BB port high speed lane map from NVM (nvm cfg
			 * option 43/44).
			 * NVM has Rx and Tx map independently. In practice
			 * these should be the same.
			 */
			lane_map = mdump2_cfg->phy_cfg.f_lane_cfg1;
		else
			/* Read BB port low speed lane map from NVM (nvm cfg
			 * option 39/40).
			 * NVM has Rx and Tx map independently  In practice
			 * these should be the same.
			 * Use Rx lane mask to determine lane.
			 */
			lane_map = mdump2_cfg->phy_cfg.e_lane_cfg1;

		switch (port) {
		case 0:
			*lane = GET_MFW_FIELD(lane_map,
					      NVM_CFG1_GLOB_RX_LANE0_SWAP);
			break;
		case 1:
			*lane = GET_MFW_FIELD(lane_map,
					      NVM_CFG1_GLOB_RX_LANE1_SWAP);
			break;
		case 2:
			*lane = GET_MFW_FIELD(lane_map,
					      NVM_CFG1_GLOB_RX_LANE2_SWAP);
			break;
		case 3:
			*lane = GET_MFW_FIELD(lane_map,
					      NVM_CFG1_GLOB_RX_LANE3_SWAP);
			break;
		default:
			return false;
		}
	} else {
		u32 lanes[4];
		int i;

		/* Read AH port lane map from NVM (nvm cfg option 39/40)
		 * AH NVM setting has combined Rx & Tx map.
		 */
		lane_map = mdump2_cfg->phy_cfg.generic_cont1;
		lanes[0] = GET_MFW_FIELD(lane_map, NVM_CFG1_GLOB_LANE0_SWAP);
		lanes[1] = GET_MFW_FIELD(lane_map, NVM_CFG1_GLOB_LANE1_SWAP);
		lanes[2] = GET_MFW_FIELD(lane_map, NVM_CFG1_GLOB_LANE2_SWAP);
		lanes[3] = GET_MFW_FIELD(lane_map, NVM_CFG1_GLOB_LANE3_SWAP);
		for (i = 0; i < 4; i++) {
			if (lanes[i] == port) {
				*lane = i;
				return true;
			}
		}

		return false;
	}

	return true;
}

struct phy_dump_reg {
	u32 addr;
	const char *name;
	u32 jump;
};

static const struct phy_dump_reg common_phy_dump_regs_list[] = {
	{MISCS_REG_RESET_PL_HV_2_K2, "MISCS_REG_RESET_PL_HV_2",
	 0},
	{MISCS_REG_RESET_PL_HV, "MISCS_REG_RESET_PL_HV",
	 0},
	{NWS_REG_COMMON_CONTROL_K2, "NWS_REG_COMMON_CONTROL",
	 0},
	{NWS_REG_PHY_CTRL_K2, "NWS_REG_PHY_CTRL",
	 0},
	{NWS_REG_ANEG_CFG_K2, "NWS_REG_ANEG_CFG",
	 0},
	{NWS_REG_COMMON_STATUS_K2, "NWS_REG_COMMON_STATUS",
	 0},
	{NWS_REG_EXTERNAL_SIGNAL_DETECT_K2,
	 "NWS_REG_EXTERNAL_SIGNAL_DETECT", 0},
	{NWS_REG_EXTERNAL_LINK_ALARM_STATUS_K2,
	 "NWS_REG_EXTERNAL_LINK_ALARM_STATUS", 0},
	{NWM_REG_PCS_SELECT_K2, "NWM_REG_PCS_SELECT:L3L2L1L0",
	 0},
	{0, "",
	 0}
};

static const struct phy_dump_reg phy_dump_port_regs_list[] = {
	{0x840000, "ETH_PCS10_50G_REG_CONTROL1", 0x40000},
	{0x840004, "ETH_PCS10_50G_REG_STATUS1", 0x40000},
	{0x840010, "ETH_PCS10_50G_REG_SPEED_ABILITY", 0x40000},
	{0x84001c, "ETH_PCS10_50G_REG_CONTROL2", 0x40000},
	{0x840020, "ETH_PCS10_50G_REG_STATUS2", 0x40000},
	{0x860010, "ETH_PCS10_50G_REG_VENDOR_RXLAUI_CFG", 0x40000},
	{0x860040, "ETH_PCS10_50G_REG_VENDOR_PCS_MODE", 0x40000},
	{0x802400, "ETH_PCS1G_REG_CONTROL", 0x80},

	{0x801600, "ETH_RSFEC_REG_RS_FEC_VENDOR_CONTROL", 0x400},
	{0x800480, "ETH_MAC_REG_XIF_MODE", 0x400},
	{0x800414, "ETH_MAC_REG_FRM_LENGTH", 0x400},
	{0x800444, "ETH_MAC_REG_TX_IPG_LENGTH", 0x400},
	{0x800408, "ETH_MAC_REG_COMMAND_CONFIG", 0x400},
	{0, "", 0}
};

static const struct phy_dump_reg phy_dump_lane_regs_list[] = {
	{0x700010, "NWS_REG_LN_CTRL", 0x10},
	{0x700014, "NWS_REG_LN_STATUS", 0x10},
	{0x700018, "NWS_REG_LN_AN_LINK_INPUTS", 0x10},
	{0x70001c, "NWS_REG_LN_AN_LINK_OUTPUTS", 0x10},
	{0, "", 0}
};

static bool qed_phy_dump_display(struct mdump2_cfg_params *mdump2_cfg,
				 char *regval_buf, char *res_buf, u32 * res_off)
{
	char addr_array_str[MAX_PORT_PER_DEVICE][MAX_CHAR_PER_LINE];
	bool stec_rx_valid[MAX_PORT_PER_DEVICE];
	bool stec_link_up[MAX_PORT_PER_DEVICE];
	bool mtip_1g_link[MAX_PORT_PER_DEVICE];
	bool mtip_link[MAX_PORT_PER_DEVICE];
	char addr_str[MAX_CHAR_PER_LINE];
	u32 ln_map[MAX_PORT_PER_DEVICE];
	u32 index, val, port;

	memset(stec_rx_valid, 0, sizeof(stec_rx_valid));
	memset(stec_link_up, 0, sizeof(stec_link_up));
	memset(mtip_1g_link, 0, sizeof(mtip_1g_link));
	memset(mtip_link, 0, sizeof(mtip_link));

	*res_off += STR_APPEND(res_buf, *res_off,
			       "\n%-35s %-10s %-16s\n",
			       "Common Register Name", "Addr", "Value");
	*res_off += STR_APPEND(res_buf, *res_off,
			       "%-35s %-10s %-16s\n",
			       "--------------------", "----", "-----");

	for (index = 0; common_phy_dump_regs_list[index].addr; index++) {
		if (!qed_mdump2_reg_read(regval_buf,
					 s_mdump2_regval_strs[GRC_REGS],
					 common_phy_dump_regs_list[index].addr,
					 &val))
			continue;
		scnprintf(addr_str, sizeof(addr_str), "0x%x",
			  common_phy_dump_regs_list[index].addr);
		*res_off += STR_APPEND(res_buf, *res_off,
				       "%-35s %-10s 0x%x\n",
				       common_phy_dump_regs_list[index].name,
				       addr_str, val);
	}
	*res_off += STR_APPEND(res_buf, *res_off, "\n");

	*res_off += STR_APPEND(res_buf, *res_off,
			       "%-35s %-10s %-10s %-10s %-10s %-10s\n",
			       "Register Name",
			       "Addr", "Port0", "Port1", "Port2", "Port3");
	*res_off += STR_APPEND(res_buf, *res_off,
			       "%-35s %-10s %-10s %-10s %-10s %-10s\n",
			       "--------------------",
			       "------",
			       "------", "------", "------", "------");

	memset(addr_array_str, 0, sizeof(addr_array_str));

	for (index = 0; phy_dump_port_regs_list[index].addr; index++) {
		u32 port, val_array[MAX_PORT_PER_DEVICE];

		for (port = 0; port < 4; port++) {
			u32 addr = phy_dump_port_regs_list[index].addr +
			    port * phy_dump_port_regs_list[index].jump;

			if (!qed_mdump2_reg_read(regval_buf,
						 s_mdump2_regval_strs[GRC_REGS],
						 addr, &val_array[port]))
				continue;
			scnprintf(addr_array_str[port],
				  sizeof(addr_array_str[port]), "0x%x",
				  val_array[port]);
		}

		scnprintf(addr_str, sizeof(addr_str), "0x%x",
			  phy_dump_port_regs_list[index].addr);
		*res_off += STR_APPEND(res_buf, *res_off,
				       "%-35s (%-8s) %-10s %-10s %-10s %-10s\n",
				       phy_dump_port_regs_list[index].name,
				       addr_str, addr_array_str[0],
				       addr_array_str[1], addr_array_str[2],
				       addr_array_str[3]);
	}

	/* Print Lane mapping */
	for (port = 0; port < 4; port++)
		qed_get_master_lane_for_port(mdump2_cfg, regval_buf, port,
					     &ln_map[port]);

	/* dispaly lane registers */
	*res_off += STR_APPEND(res_buf, *res_off,
			       "\n%-34s %-10s L%-9d L%-9d L%-9d L%-9d\n",
			       "Port to Lane swap", "Base Addr",
			       ln_map[0], ln_map[1], ln_map[2], ln_map[3]);
	*res_off += STR_APPEND(res_buf, *res_off,
			       "%-34s %-10s %-10s %-10s %-10s %-10s\n",
			       "--------------------",
			       "------",
			       "------", "------", "------", "------");

	memset(addr_array_str, 0, sizeof(addr_array_str));

	for (index = 0; phy_dump_lane_regs_list[index].addr; index++) {
		u32 port, val_array[MAX_PORT_PER_DEVICE], abs_addr;

		for (port = 0; port < 4; port++) {
			abs_addr = phy_dump_lane_regs_list[index].addr +
			    ln_map[port] * phy_dump_lane_regs_list[index].jump;
			if (!qed_mdump2_reg_read(regval_buf,
						 s_mdump2_regval_strs[GRC_REGS],
						 abs_addr, &val_array[port]))
				continue;
			scnprintf(addr_array_str[port],
				  sizeof(addr_array_str[port]), "0x%x",
				  val_array[port]);
		}

		scnprintf(addr_str, sizeof(addr_str), "0x%x",
			  phy_dump_lane_regs_list[index].addr);
		*res_off += STR_APPEND(res_buf, *res_off,
				       "%-34s (%-8s) %-10s %-10s %-10s %-10s\n",
				       phy_dump_lane_regs_list[index].name,
				       addr_str,
				       addr_array_str[0], addr_array_str[1],
				       addr_array_str[2], addr_array_str[3]);
	}

	*res_off += STR_APPEND(res_buf, *res_off, "LinkDump\n");

	for (port = 0; port < 4; port++) {
		u32 lane_status[MAX_PORT_PER_DEVICE];

		if (qed_mdump2_reg_read(regval_buf,
					s_mdump2_regval_strs[GRC_REGS],
					0x700014 + ln_map[port] * 0x10,
					&lane_status[port])) {
			stec_link_up[port] = (lane_status[port] & 1) ?
			    true : false;
			stec_rx_valid[port] = (lane_status[port] & 2) ?
			    true : false;
		}
		if (qed_mdump2_reg_read(regval_buf,
					s_mdump2_regval_strs[GRC_REGS],
					0x840004 + port * 0x40000, &val))
			mtip_link[port] = (val & BIT(2)) ? true : false;
		if (qed_mdump2_reg_read(regval_buf,
					s_mdump2_regval_strs[GRC_REGS],
					0x802404 + port * 0x80, &val))
			mtip_1g_link[port] = (val & BIT(2)) ? true : false;
	}
	*res_off += STR_APPEND(res_buf, *res_off,
			       "%-45s %-10d %-10d %-10d %-10d\n",
			       "Serdes Link Up",
			       stec_link_up[0], stec_link_up[1],
			       stec_link_up[2], stec_link_up[3]);
	*res_off += STR_APPEND(res_buf, *res_off,
			       "%-45s %-10d %-10d %-10d %-10d\n",
			       "Serdes RX Valid",
			       stec_rx_valid[0], stec_rx_valid[1],
			       stec_rx_valid[2], stec_rx_valid[3]);
	*res_off += STR_APPEND(res_buf, *res_off,
			       "%-45s %-10d %-10d %-10d %-10d\n",
			       "10G-50G PCS link",
			       mtip_link[0], mtip_link[1], mtip_link[2],
			       mtip_link[3]);
	*res_off += STR_APPEND(res_buf, *res_off,
			       "%-45s %-10d %-10d %-10d %-10d\n",
			       "1G PCS link",
			       mtip_1g_link[0], mtip_1g_link[1],
			       mtip_1g_link[2], mtip_1g_link[3]);

	return true;
}

static bool qed_lnk_dump_fec_cor_err_bb(struct mdump2_cfg_params *mdump2_cfg,
					char *regval_buf,
					char *res_buf,
					u32 * res_off, u32 port, u32 lane)
{
	char group_name[MAX_CHAR_PER_LINE];
	u32 result_L16;
	u32 result_U16;
	u32 val;

	val = mdump2_cfg->link_dump_results.port[port].mfw_link_status;
	switch (val & LINK_STATUS_FEC_MODE_MASK) {
	case LINK_STATUS_FEC_MODE_NONE:
		*res_off += STR_APPEND(res_buf, *res_off, "N.A.          ");
		break;
	case LINK_STATUS_FEC_MODE_FIRECODE_CL74:
	case LINK_STATUS_FEC_MODE_RS_CL91:
		scnprintf(group_name, sizeof(group_name),
			  s_mdump2_regval_strs[REGS_FOR_TSCF_LANE_INDEX], lane);
		if (qed_mdump2_reg_read(regval_buf, group_name,
					(0x000092b2 | (lane << 16)),
					&result_L16) &&
		    qed_mdump2_reg_read(regval_buf, group_name,
					(0x000092b3 | (lane << 16)),
					&result_U16)) {
			val = (result_U16 << 16) | result_L16;
			*res_off += STR_APPEND(res_buf, *res_off,
					       "%-14x ", val);
		}
		break;
	default:
		*res_off += STR_APPEND(res_buf, *res_off, "N.A.          ");
		break;
	}

	return true;
}

static bool qed_lnk_dump_fec_cor_err_k2(struct mdump2_cfg_params *mdump2_cfg,
					char *regval_buf,
					char *res_buf, u32 * res_off, u32 port)
{
	u32 val;

	/* If AH this is FC FEC or RS FEC                        */
	val = mdump2_cfg->link_dump_results.port[port].mfw_link_status;
	switch (val & LINK_STATUS_FEC_MODE_MASK) {
	case LINK_STATUS_FEC_MODE_NONE:
		*res_off += STR_APPEND(res_buf, *res_off, "N.A.          ");
		return true;
	case LINK_STATUS_FEC_MODE_FIRECODE_CL74:
		switch (port) {
		case 0:
			if (!qed_mdump2_reg_read(regval_buf,
						 s_mdump2_regval_strs[GRC_REGS],
						 NWM_REG_FC_FEC_CERR_COUNT_0,
						 &val))
				return true;
			break;
		case 1:
			if (!qed_mdump2_reg_read(regval_buf,
						 s_mdump2_regval_strs[GRC_REGS],
						 NWM_REG_FC_FEC_CERR_COUNT_4,
						 &val))
				return true;
			break;
		case 2:
			if (!qed_mdump2_reg_read(regval_buf,
						 s_mdump2_regval_strs[GRC_REGS],
						 NWM_REG_FC_FEC_CERR_COUNT_6,
						 &val))
				return true;
			break;
		case 3:
			qed_mdump2_reg_read(regval_buf,
					    s_mdump2_regval_strs[GRC_REGS],
					    NWM_REG_FC_FEC_CERR_COUNT_7, &val);
			break;
		default:
			*res_off += STR_APPEND(res_buf, *res_off,
					       "N.A.          ");
			return true;
		}
		break;
	case LINK_STATUS_FEC_MODE_RS_CL91:
		switch (port) {
		case 0:
			qed_mdump2_reg_read(regval_buf,
					    s_mdump2_regval_strs[GRC_REGS],
					    0x801408, &val);
			break;
		case 1:
			qed_mdump2_reg_read(regval_buf,
					    s_mdump2_regval_strs[GRC_REGS],
					    0x801808, &val);
			break;
		case 2:
			qed_mdump2_reg_read(regval_buf,
					    s_mdump2_regval_strs[GRC_REGS],
					    0x801c08, &val);
			break;
		case 3:
			qed_mdump2_reg_read(regval_buf,
					    s_mdump2_regval_strs[GRC_REGS],
					    0x802008, &val);
			break;
		default:
			*res_off += STR_APPEND(res_buf, *res_off,
					       "N.A.          ");
			return true;
		}
		break;
	default:
		*res_off += STR_APPEND(res_buf, *res_off, "              ");
		return true;
	}
	*res_off += STR_APPEND(res_buf, *res_off, "%-14x ", val);

	return true;
}

static bool qed_lnk_dump_fec_cor_err(struct mdump2_cfg_params *mdump2_cfg,
				     char *regval_buf,
				     char *res_buf,
				     u32 * res_off, u32 port, u32 lane)
{
	/* Check if this is BB or AH */
	if (qed_is_bb_chip(regval_buf))
		return qed_lnk_dump_fec_cor_err_bb(mdump2_cfg, regval_buf,
						   res_buf, res_off,
						   port, lane);
	else if (qed_is_k2_chip(regval_buf))
		return qed_lnk_dump_fec_cor_err_k2(mdump2_cfg, regval_buf,
						   res_buf, res_off, port);

	return true;
}

static bool qed_lnk_dump_fec_uncor_err(struct mdump2_cfg_params *mdump2_cfg,
				       char *regval_buf,
				       char *res_buf,
				       u32 * res_off, u32 port, u32 lane)
{
	u32 val = 0;

	/* Check if this is BB or AH */
	if (qed_is_bb_chip(regval_buf)) {
		char group_name[MAX_CHAR_PER_LINE];
		u32 result_L16;
		u32 result_U16;

		val = mdump2_cfg->link_dump_results.port[port].mfw_link_status;
		switch (val & LINK_STATUS_FEC_MODE_MASK) {
		case LINK_STATUS_FEC_MODE_NONE:
			*res_off += STR_APPEND(res_buf, *res_off,
					       "N.A.          ");
			return true;
		case LINK_STATUS_FEC_MODE_FIRECODE_CL74:
		case LINK_STATUS_FEC_MODE_RS_CL91:
			scnprintf(group_name,
				  sizeof(group_name),
				  s_mdump2_regval_strs
				  [REGS_FOR_TSCF_LANE_INDEX], lane);
			qed_mdump2_reg_read(regval_buf, group_name,
					    (0x000092b4 | (lane << 16)),
					    &result_L16);
			qed_mdump2_reg_read(regval_buf, group_name,
					    (0x000092b5 | (lane << 16)),
					    &result_U16);
			val = (result_U16 << 16) | result_L16;
			break;
		default:
			*res_off += STR_APPEND(res_buf, *res_off,
					       "N.A.          ");
			return true;
		}
	} else if (qed_is_k2_chip(regval_buf)) {
		/* If AH this is FC FEC or RS FEC */
		val = mdump2_cfg->link_dump_results.port[port].mfw_link_status;
		switch (val & LINK_STATUS_FEC_MODE_MASK) {
		case LINK_STATUS_FEC_MODE_NONE:
			*res_off += STR_APPEND(res_buf, *res_off,
					       "N.A.          ");
			return true;
		case LINK_STATUS_FEC_MODE_FIRECODE_CL74:
			switch (port) {
			case 0:
				qed_mdump2_reg_read(regval_buf,
						    s_mdump2_regval_strs
						    [GRC_REGS],
						    NWM_REG_FC_FEC_NCERR_COUNT_0,
						    &val);
				break;
			case 1:
				qed_mdump2_reg_read(regval_buf,
						    s_mdump2_regval_strs
						    [GRC_REGS],
						    NWM_REG_FC_FEC_NCERR_COUNT_4,
						    &val);
				break;
			case 2:
				qed_mdump2_reg_read(regval_buf,
						    s_mdump2_regval_strs
						    [GRC_REGS],
						    NWM_REG_FC_FEC_NCERR_COUNT_6,
						    &val);
				break;
			case 3:
				qed_mdump2_reg_read(regval_buf,
						    s_mdump2_regval_strs
						    [GRC_REGS],
						    NWM_REG_FC_FEC_NCERR_COUNT_7,
						    &val);
				break;
			default:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "N.A.          ");
				return true;
			}
			break;
		case LINK_STATUS_FEC_MODE_RS_CL91:
			switch (port) {
			case 0:
				qed_mdump2_reg_read(regval_buf,
						    s_mdump2_regval_strs
						    [GRC_REGS], 0x801410, &val);
				break;
			case 1:
				qed_mdump2_reg_read(regval_buf,
						    s_mdump2_regval_strs
						    [GRC_REGS], 0x801810, &val);
				break;
			case 2:
				qed_mdump2_reg_read(regval_buf,
						    s_mdump2_regval_strs
						    [GRC_REGS], 0x801c10, &val);
				break;
			case 3:
				qed_mdump2_reg_read(regval_buf,
						    s_mdump2_regval_strs
						    [GRC_REGS], 0x802010, &val);
				break;
			default:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "N.A.          ");
				return true;
			}
			break;
		default:
			*res_off += STR_APPEND(res_buf, *res_off,
					       "              ");
			return true;
		}
	}

	*res_off += STR_APPEND(res_buf, *res_off, "%-14x ", val);
	return true;
}

static bool qed_link_dump_display(struct mdump2_cfg_params *mdump2_cfg,
				  char *regval_buf,
				  char *res_buf,
				  u32 * res_off,
				  u32 display_mode, u32 display_param)
{
	struct link_dump_port *ld_port;
	u32 val, reg_addr;
	u32 port = 0;
	u32 lane = 0;

	if (display_mode != ITEM) {
		port = display_mode - 1;
		if (!qed_get_master_lane_for_port(mdump2_cfg, regval_buf,
						  port, &lane)) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nERROR RETRIEVING LANE      ");
			return false;
		}
	}

	ld_port = &mdump2_cfg->link_dump_results.port[port];
	switch (display_param) {
	case LINK_STATE:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nMFW LINK STATE      ");
		} else {
			val = ld_port->mfw_link_status;
			if (val & LINK_STATUS_LINK_UP)
				*res_off += STR_APPEND(res_buf, *res_off,
						       "UP            ");
			else
				*res_off += STR_APPEND(res_buf, *res_off,
						       "DOWN          ");
		}
		break;
	case PCS_LINK_STATE:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nPCS LINK            ");
		} else {
			val = ld_port->pcs_status;
			if (val == PCS_STATUS_LINK_UP)
				*res_off += STR_APPEND(res_buf, *res_off,
						       "Yes           ");
			else
				*res_off += STR_APPEND(res_buf, *res_off,
						       "NO            ");
		}
		break;
	case MAC_FAULT:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nMAC FAULT           ");
		} else {
			val = ld_port->mfw_link_status;
			if ((val & LINK_STATUS_MAC_LOCAL_FAULT) &&
			    (val & LINK_STATUS_MAC_REMOTE_FAULT))
				*res_off += STR_APPEND(res_buf, *res_off,
						       "LOC / REM     ");
			else if (val & LINK_STATUS_MAC_LOCAL_FAULT)
				*res_off += STR_APPEND(res_buf, *res_off,
						       "LOCAL         ");
			else if (val & LINK_STATUS_MAC_REMOTE_FAULT)
				*res_off += STR_APPEND(res_buf, *res_off,
						       "REMOTE        ");
			else
				*res_off += STR_APPEND(res_buf, *res_off,
						       "None          ");
		}
		break;
	case MAC_FAULT_HW:
		break;
	case RX_SIGNAL:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nRX SIGNAL DETECT    ");
		} else {
			val = ld_port->mfw_link_status;
			if (val & LINK_STATUS_RX_SIGNAL_PRESENT)
				*res_off += STR_APPEND(res_buf, *res_off,
						       "Yes           ");
			else
				*res_off += STR_APPEND(res_buf, *res_off,
						       "No            ");
		}
		break;
	case LINK_OWNER:
		break;
	case LINK_SPEED:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nLINK SPEED          ");
		} else {
			val = ld_port->mfw_link_status;
			if ((val & LINK_STATUS_LINK_UP) == 0) {
				*res_off += STR_APPEND(res_buf, *res_off,
						       "              ");
			} else {
				switch (val & LINK_STATUS_SPEED_AND_DUPLEX_MASK) {
				case LINK_STATUS_SPEED_AND_DUPLEX_1000THD:
				case LINK_STATUS_SPEED_AND_DUPLEX_1000TFD:
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "1000          ");
					break;
				case LINK_STATUS_SPEED_AND_DUPLEX_10G:
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "10G           ");
					break;
				case LINK_STATUS_SPEED_AND_DUPLEX_20G:
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "20G           ");
					break;
				case LINK_STATUS_SPEED_AND_DUPLEX_25G:
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "25G           ");
					break;
				case LINK_STATUS_SPEED_AND_DUPLEX_40G:
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "40G           ");
					break;
				case LINK_STATUS_SPEED_AND_DUPLEX_50G:
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "50G           ");
					break;
				case LINK_STATUS_SPEED_AND_DUPLEX_100G:
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "100G          ");
					break;
				default:
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "              ");
					break;
				}
			}
		}
		break;
	case MODULE_P:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nMODULE              ");
		} else {
			val = ld_port->transceiver_data;

			switch (GET_MFW_FIELD(val, ETH_TRANSCEIVER_TYPE)) {
			case ETH_TRANSCEIVER_TYPE_UNKNOWN:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "UNKNOWN      ");
				break;
			case ETH_TRANSCEIVER_TYPE_1G_PCC:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "1G Passive Cu");
				break;
			case ETH_TRANSCEIVER_TYPE_1G_ACC:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "1G Active Cu ");
				break;
			case ETH_TRANSCEIVER_TYPE_1G_LX:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "1G LX        ");
				break;
			case ETH_TRANSCEIVER_TYPE_1G_SX:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "1G SX        ");
				break;
			case ETH_TRANSCEIVER_TYPE_10G_SR:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "10G SR       ");
				break;
			case ETH_TRANSCEIVER_TYPE_10G_LR:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "10G LR       ");
				break;
			case ETH_TRANSCEIVER_TYPE_10G_LRM:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "10G LRM      ");
				break;
			case ETH_TRANSCEIVER_TYPE_10G_ER:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "10G ER       ");
				break;
			case ETH_TRANSCEIVER_TYPE_10G_PCC:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "10G Passive Cu");
				break;
			case ETH_TRANSCEIVER_TYPE_10G_ACC:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "10G Active Cu ");
				break;
			case ETH_TRANSCEIVER_TYPE_XLPPI:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "XLPPI         ");
				break;
			case ETH_TRANSCEIVER_TYPE_40G_LR4:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "40G LR-4      ");
				break;
			case ETH_TRANSCEIVER_TYPE_40G_SR4:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "40G SR-4      ");
				break;
			case ETH_TRANSCEIVER_TYPE_40G_CR4:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "40G CR-4      ");
				break;
			case ETH_TRANSCEIVER_TYPE_100G_AOC:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "100G AOC      ");
				break;
			case ETH_TRANSCEIVER_TYPE_100G_SR4:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "100G SR-4     ");
				break;
			case ETH_TRANSCEIVER_TYPE_100G_LR4:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "100G LR-4     ");
				break;
			case ETH_TRANSCEIVER_TYPE_100G_ER4:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "100G ER-4     ");
				break;
			case ETH_TRANSCEIVER_TYPE_100G_ACC:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "100G Active Cu");
				break;
			case ETH_TRANSCEIVER_TYPE_100G_CR4:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "100G CR-4     ");
				break;
			case ETH_TRANSCEIVER_TYPE_4x10G_SR:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "4x10G SR      ");
				break;
			case ETH_TRANSCEIVER_TYPE_25G_CA_N:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "25G Cu Short  ");
				break;
			case ETH_TRANSCEIVER_TYPE_25G_ACC_S:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "25G Cu Short  ");
				break;
			case ETH_TRANSCEIVER_TYPE_25G_CA_S:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "25G Cu Medium ");
				break;
			case ETH_TRANSCEIVER_TYPE_25G_ACC_M:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "25G Cu Medium ");
				break;
			case ETH_TRANSCEIVER_TYPE_25G_CA_L:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "25G Cu Long   ");
				break;
			case ETH_TRANSCEIVER_TYPE_25G_ACC_L:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "25G Cu Long   ");
				break;
			case ETH_TRANSCEIVER_TYPE_25G_SR:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "25G SR        ");
				break;
			case ETH_TRANSCEIVER_TYPE_25G_LR:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "25G LR        ");
				break;
			case ETH_TRANSCEIVER_TYPE_25G_AOC:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "25G AOC       ");
				break;
			case ETH_TRANSCEIVER_TYPE_4x10G:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "4x10G         ");
				break;
			case ETH_TRANSCEIVER_TYPE_4x25G_CR:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "4x25G CR      ");
				break;
			case ETH_TRANSCEIVER_TYPE_1000BASET:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "1000BaseT     ");
				break;
			case ETH_TRANSCEIVER_TYPE_10G_BASET:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "10G_BaseT     ");
				break;
			case ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_SR:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "MULT 10/40 SR ");
				break;
			case ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_CR:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "MULT 10/40 CR ");
				break;
			case ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_LR:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "MULT 10/40 LR ");
				break;
			case ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_SR:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "MULT 40/100SR ");
				break;
			case ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_CR:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "MULT 40/100CR ");
				break;
			case ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_LR:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "MULT 40/100LR ");
				break;
			case ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_AOC:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "MULT 40/100AOC");
				break;
			case ETH_TRANSCEIVER_TYPE_MULTI_RATE_1G_10G_SR:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "MULT 1/10G SR ");
				break;
			case ETH_TRANSCEIVER_TYPE_MULTI_RATE_1G_10G_LR:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "MULT 1/10G LR ");
				break;
			case ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_25G_SR:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "MULT 10/25 SR ");
				break;
			case ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_25G_LR:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "MULT 10/25 LR ");
				break;
			default:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "Undefined     ");
				break;
			}
		}
		break;
	case FLOW_CTRL:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nFLOW CTRL           ");
		} else {
			val = ld_port->mfw_link_status;
			if ((val & LINK_STATUS_LINK_UP) == 0) {
				*res_off += STR_APPEND(res_buf, *res_off,
						       "              ");
			} else {
				/* First check if PFC is enabled */
				if (val & LINK_STATUS_PFC_ENABLED) {
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "PFC           ");
				} else {
					/* PFC not enabled */
					if ((val & 0x600000) == 0x600000)
						*res_off += STR_APPEND(res_buf,
								       *res_off,
								       "RX/TX PAUSE   ");
					else if (val & 0x400000)
						*res_off += STR_APPEND(res_buf,
								       *res_off,
								       "RX PAUSE      ");
					else if (val & 0x200000)
						*res_off += STR_APPEND(res_buf,
								       *res_off,
								       "TX PAUSE      ");
					else
						*res_off += STR_APPEND(res_buf,
								       *res_off,
								       "N/A           ");
				}
			}
		}
		break;
	case FEC_MODE:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nFEC MODE            ");
		} else {
			val = ld_port->mfw_link_status;

			switch (val & LINK_STATUS_FEC_MODE_MASK) {
			case LINK_STATUS_FEC_MODE_NONE:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "NONE          ");
				break;
			case LINK_STATUS_FEC_MODE_FIRECODE_CL74:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "FIRECODE      ");
				break;
			case LINK_STATUS_FEC_MODE_RS_CL91:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "RS_FEC        ");
				break;
			default:
				*res_off += STR_APPEND(res_buf, *res_off,
						       "              ");
				break;
			}
		}
		break;
	case FEC_COR_ERR_CNT:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nFEC COR_ERR_CNT     ");
		} else {
			qed_lnk_dump_fec_cor_err(mdump2_cfg,
						 regval_buf,
						 res_buf, res_off, port, lane);
		}
		break;
	case FEC_UNCOR_ERR_CNT:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nFEC UNCOR_ERR_CNT   ");
		} else {
			qed_lnk_dump_fec_uncor_err(mdump2_cfg,
						   regval_buf,
						   res_buf,
						   res_off, port, lane);
		}
		break;
	case AUTONEG:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nAUTONEG             ");
		} else {
			val = ld_port->mfw_link_status;

			if (val & LINK_STATUS_AUTO_NEGOTIATE_COMPLETE)
				*res_off += STR_APPEND(res_buf, *res_off,
						       "COMPLETE      ");
			else if (val & LINK_STATUS_PARALLEL_DETECTION_USED)
				*res_off += STR_APPEND(res_buf, *res_off,
						       "PARALLEL_DET  ");
			else if (val & LINK_STATUS_AUTO_NEGOTIATE_ENABLED)
				*res_off += STR_APPEND(res_buf, *res_off,
						       "ENABLED       ");
			else
				*res_off += STR_APPEND(res_buf, *res_off,
						       "DISABLED      ");
		}
		break;
	case AN_LP_ADV_SPEEDS:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nAN LP ADV SPEEDS    ");
		} else {
			val = ld_port->mfw_link_status;
			if ((val & LINK_STATUS_AUTO_NEGOTIATE_ENABLED) == 0) {
				*res_off += STR_APPEND(res_buf, *res_off,
						       "              ");
			} else {
				u32 speed_map = 0;

				if (val &
				    (LINK_STATUS_LINK_PARTNER_1000TFD_CAPABLE |
				     LINK_STATUS_LINK_PARTNER_1000THD_CAPABLE))
					speed_map |= 0x1;
				if (val & LINK_STATUS_LINK_PARTNER_10G_CAPABLE)
					speed_map |= 0x2;
				if (val & LINK_STATUS_LINK_PARTNER_20G_CAPABLE)
					speed_map |= 0x4;
				if (val & LINK_STATUS_LINK_PARTNER_25G_CAPABLE)
					speed_map |= 0x8;
				if (val & LINK_STATUS_LINK_PARTNER_40G_CAPABLE)
					speed_map |= 0x10;
				if (val & LINK_STATUS_LINK_PARTNER_50G_CAPABLE)
					speed_map |= 0x20;
				if (val & LINK_STATUS_LINK_PARTNER_100G_CAPABLE)
					speed_map |= 0x40;
				*res_off += STR_APPEND(res_buf, *res_off,
						       "0x%02x          ",
						       speed_map);
			}
		}
		break;
	case AN_LP_ADV_FLOW_CTRL:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nAN LP ADV FLOW CTRL ");
		} else {
			val = ld_port->mfw_link_status;
			if ((val & LINK_STATUS_AUTO_NEGOTIATE_ENABLED) == 0) {
				*res_off += STR_APPEND(res_buf, *res_off,
						       "              ");
			} else {
				val &=
				    LINK_STATUS_LINK_PARTNER_FLOW_CONTROL_MASK;
				if (val == LINK_STATUS_LINK_PARTNER_BOTH_PAUSE) {
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "SYM/ASYM      ");
				} else if (val ==
					   LINK_STATUS_LINK_PARTNER_SYMMETRIC_PAUSE)
				{
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "SYM PAUSE     ");
				} else if (val ==
					   LINK_STATUS_LINK_PARTNER_ASYMMETRIC_PAUSE)
				{
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "ASYM PAUSE    ");
				} else {
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "No pause adv  ");
				}
			}
		}
		break;
	case TX_PRE_FIR:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nTX PRE FIR          ");
		} else {
			if (qed_is_bb_chip(regval_buf)) {
				/* Check if we are using Fixed mode or AN
				 * For now assume fixed
				 * Check Falcon
				 */
				if (mdump2_cfg->link_dump_results.
				    is_falcon[port] == true) {
					/* For now assume fixed */
					char group_name[MAX_CHAR_PER_LINE];

					scnprintf(group_name,
						  sizeof(group_name),
						  s_mdump2_regval_strs
						  [REGS_FOR_FALCON_LANE_INDEX],
						  lane);

					qed_mdump2_reg_read(regval_buf,
							    group_name,
							    0x0800d094, &val);
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "%-14x ", val);
				}
			} else if (qed_is_k2_chip(regval_buf)) {
				/* Chip is AH */
				val = ld_port->mfw_link_status;
				if ((val &
				     LINK_STATUS_AUTO_NEGOTIATE_ENABLED) == 0) {
					/* Report fixed tx FIR settings Tx
					 * pre-cursor when not in lt 0x394 [3:0]
					 */
					reg_addr = 0x726e50;
					qed_mdump2_reg_read(regval_buf,
							    s_mdump2_regval_strs
							    [GRC_REGS],
							    (reg_addr +
							     (0x2000 * lane)),
							    &val);
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "%-14x ", val);
				} else {
					/* Report LT tx FIR settings Spare_cfg3
					 * is the TX pre-cursor 0x75B [3:0]
					 */
					reg_addr = 0x727D6C;
					qed_mdump2_reg_read(regval_buf,
							    s_mdump2_regval_strs
							    [GRC_REGS],
							    (reg_addr +
							     (0x2000 * lane)),
							    &val);
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "%-14x ", val);
				}
			}
		}
		break;
	case TX_MAIN_FIR:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nTX MAIN FIR         ");
		} else {
			if (qed_is_bb_chip(regval_buf)) {
				/* Check if we are using Fixed mode or AN
				 * For now assume fixed
				 * Check Falcon
				 */
				if (mdump2_cfg->link_dump_results.
				    is_falcon[port] == true) {
					/* For now assume fixed */
					char group_name[MAX_CHAR_PER_LINE];

					scnprintf(group_name,
						  sizeof(group_name),
						  s_mdump2_regval_strs
						  [REGS_FOR_FALCON_LANE_INDEX],
						  lane);

					qed_mdump2_reg_read(regval_buf,
							    group_name,
							    0x0800d095, &val);
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "%-14x ", val);
				}
			} else if (qed_is_k2_chip(regval_buf)) {
				/* Chip is AH */
				val = ld_port->mfw_link_status;
				if ((val &
				     LINK_STATUS_AUTO_NEGOTIATE_ENABLED) == 0) {
					/* Report fixed tx FIR settings tx drv_swing
					 * transmit amplitude 0x396 [3:0]
					 */
					reg_addr = 0x726e58;
					qed_mdump2_reg_read(regval_buf,
							    s_mdump2_regval_strs
							    [GRC_REGS],
							    (reg_addr +
							     (0x2000 * lane)),
							    &val);
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "%-14x ", val);
				} else {
					/* Report LT tx FIR settings  Spare_cfg4
					 * is the TX main cursor
					 * when link training 0x75C [7:0]
					 */
					reg_addr = 0x727D70;
					qed_mdump2_reg_read(regval_buf,
							    s_mdump2_regval_strs
							    [GRC_REGS],
							    (reg_addr +
							     (0x2000 * lane)),
							    &val);
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "%-14x ", val);
				}
			}
		}
		break;
	case TX_POST_FIR:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nTX POST FIR         ");
		} else {
			if (qed_is_bb_chip(regval_buf)) {
				/* Check if we are using Fixed mode or AN
				 * Check Falcon
				 */
				if (mdump2_cfg->link_dump_results.
				    is_falcon[port] == true) {
					/* For now assume fixed */
					char group_name[MAX_CHAR_PER_LINE];

					scnprintf(group_name,
						  sizeof(group_name),
						  s_mdump2_regval_strs
						  [REGS_FOR_FALCON_LANE_INDEX],
						  lane);

					qed_mdump2_reg_read(regval_buf,
							    group_name,
							    0x0800d094, &val);
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "%-14x ", val);
				} else {
					/* Check Eagle */
				}
			} else if (qed_is_k2_chip(regval_buf)) {
				/* Chip is AH */
				val = ld_port->mfw_link_status;
				if ((val &
				     LINK_STATUS_AUTO_NEGOTIATE_ENABLED) == 0) {
					/* Report fixed tx FIR settings Tx post-cursor when
					 * not in lt 0x392 [4:0]
					 */
					reg_addr = 0x726e48;
					qed_mdump2_reg_read(regval_buf,
							    s_mdump2_regval_strs
							    [GRC_REGS],
							    (reg_addr +
							     (0x2000 * lane)),
							    &val);
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "%-14x ", val);
				} else {
					/* Report LT tx FIR settings. Spare_cfg2 is the TX post-cursor
					 * when link training 0x75A [4:0]
					 */
					reg_addr = 0x727D68;
					qed_mdump2_reg_read(regval_buf,
							    s_mdump2_regval_strs
							    [GRC_REGS],
							    (reg_addr +
							     (0x2000 * lane)),
							    &val);
					*res_off += STR_APPEND(res_buf,
							       *res_off,
							       "%-14x ", val);
				}
			}
		}
		break;
	case RX_DFE:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nRX DFE              ");
		} else {
			if (qed_is_bb_chip(regval_buf)) {
				if (mdump2_cfg->link_dump_results.
				    is_falcon[port] == true) {
					/* Run through all DFE tap registers.
					 * If they are all zero, DFE is Disabled
					 */
					if (!qed_all_falcon_or_eagle_regs_0
					    (regval_buf, lane,
					     REGS_FOR_FALCON_LANE_INDEX,
					     0x0800d010, 14)) {
						*res_off +=
						    STR_APPEND(res_buf,
							       *res_off,
							       "ON            ");
					} else {
						*res_off += STR_APPEND(res_buf,
								       *res_off,
								       "OFF           ");
					}
				} else {
					/* Check Eagle:
					 * Run through all DFE tap registers.
					 * If they are all zero, DFE is Disabled
					 */
					if (!qed_all_falcon_or_eagle_regs_0
					    (regval_buf, lane,
					     REGS_FOR_EAGLE_LANE_INDEX,
					     0x0800d020, 11)) {
						*res_off +=
						    STR_APPEND(res_buf,
							       *res_off,
							       "ON            ");
					} else {
						*res_off += STR_APPEND(res_buf,
								       *res_off,
								       "OFF           ");
					}
				}
			} else if (qed_is_k2_chip(regval_buf)) {
				*res_off += STR_APPEND(res_buf, *res_off,
						       "N.A.          ");
			}
		}
		break;
	case DATA_MODE:
		break;
	case TRANSCEIVER_TYPE:
		break;
	case PORT_SWAP:
		break;
	case LINK_CHANGE_CNT:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nLINK CHANGE COUNT   ");
		} else {
			val = ld_port->link_change_count;
			*res_off += STR_APPEND(res_buf, *res_off,
					       "%-14d ", val);
		}
		break;
	case LFA_CNT:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nLFA COUNT           ");
		} else {
			val = ld_port->lfa_status;
			val = GET_MFW_FIELD(val, LINK_FLAP_AVOIDANCE_COUNT);
			*res_off += STR_APPEND(res_buf, *res_off,
					       "%-14x ", val);
		}
		break;
	case LFA_STATE:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nLFA STATE           ");
		} else {
			val = ld_port->lfa_status;
			val = GET_MFW_FIELD(val, LFA_LINK_FLAP_REASON);
			*res_off += STR_APPEND(res_buf, *res_off,
					       "%-14x ", val);
		}
		break;
	case MFW_RAW_LINK_STATUS:
		if (display_mode == ITEM) {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "\nMFW RAW LINK STATUS ");
		} else {
			*res_off += STR_APPEND(res_buf, *res_off,
					       "0x%08x    ",
					       ld_port->mfw_link_status);
		}
		break;
	}

	return true;
}

/**
 * Parses an Mdump2 dump buffer. If res_buf is not NULL, the Mdump2 results
 * are printed to it. In any case, the required results buffer size is assigned
 * to parsed_results_bytes. The parsing status is returned.
 * MDUMP2 1st Stage: Convert the binary dump buffer to register-value textual
 * output
 */
static enum dbg_status qed_parse_mdump2_dump_to_regval(struct qed_hwfn
						       __maybe_unused * p_hwfn,
						       u32 * dump_buf,
						       char *res_buf,
						       u32 *
						       parsed_results_bytes)
{
	struct log_hdr_stc *log_hdr = (struct log_hdr_stc *)dump_buf;
	enum dbg_status status = DBG_STATUS_OK;
	u32 res_off = 0;

	/* 1st Stage: Verify signature: log_hdr_stc contains the
	 * signature under the log fie which must be ldump.
	 */
	if (log_hdr->log != LOG_SIGNATURE)
		return DBG_STATUS_MDUMP2_INVALID_SIGNATURE;

	if (!log_hdr->log_size)
		return DBG_STATUS_MDUMP2_INVALID_LOG_SIZE;

	if (!qed_parse_mdump2_log_hdr(log_hdr, res_buf, &res_off))
		status = DBG_STATUS_MDUMP2_INVALID_LOG_HDR;
	if (status == DBG_STATUS_OK &&
	    !qed_parse_mdump2_log_data(dump_buf +
				       sizeof(struct log_hdr_stc) / sizeof(u32),
				       res_buf, &res_off))
		status = DBG_STATUS_MDUMP2_INVALID_LOG_DATA;
	/* Add 1 for string NULL termination */
	*parsed_results_bytes = res_off + 1;

	return status;
}

static enum dbg_status qed_get_mdump2_regval_results_buf_size(struct qed_hwfn *p_hwfn, u32 * dump_buf,	/* Binary Input */
							      u32 __maybe_unused
							      num_dumped_dwords,
							      u32 *
							      results_buf_size)
{
	return qed_parse_mdump2_dump_to_regval(p_hwfn, dump_buf, NULL,
					       results_buf_size);
}

static enum dbg_status qed_print_mdump2_regval_results(struct qed_hwfn *p_hwfn,
						       u32 * dump_buf,
						       u32 __maybe_unused
						       num_dumped_dwords,
						       char *res_buf)
{
	u32 parsed_buf_size;
	enum dbg_status rc;

	rc = qed_parse_mdump2_dump_to_regval(p_hwfn, dump_buf,
					     res_buf, &parsed_buf_size);
	return rc;
}

/* MDUMP2 2nd Stage: Convert the binary mdump2 data to register-value textual
 * output and from that, to linkdump textual output
 * Params : dump_buf - mdump2 binary Input
 *          dumped_size_in_dwords - mdump2 binary size in dwords
 * Outputs : res_buf - Textual linkdump output
 *           parsed_results_bytes - Textual linkdump output size
 */
static enum dbg_status qed_parse_mdump2_dump_to_linkdump(struct qed_hwfn
							 *p_hwfn,
							 u32 * dump_buf,
							 u32
							 dumped_size_in_dwords,
							 char *res_buf,
							 u32 *
							 parsed_results_bytes)
{
	u32 res_off = 0, parsed_regval_results_buf_size, num_ports;
	struct mdump2_cfg_params mdump2_cfg;
	enum dbg_status status;
	char *regval_buf;

	/* Step 1: convert binary input to reg-val textual output: */
	status = qed_get_mdump2_regval_results_buf_size(p_hwfn,
							dump_buf,
							dumped_size_in_dwords,
							&parsed_regval_results_buf_size);
	if (status != DBG_STATUS_OK)
		return status;

	/* parse and print results */
	regval_buf =
	    (char *)kzalloc(parsed_regval_results_buf_size, GFP_KERNEL);
	if (!regval_buf)
		return DBG_STATUS_MDUMP2_ERROR_ALLOCATING_BUF;

	status = qed_print_mdump2_regval_results(p_hwfn, dump_buf,
						 dumped_size_in_dwords,
						 regval_buf);

	/* -1 for the NULL included
	 * Get the number of ports on the card
	 */
	if (strlen(regval_buf) != parsed_regval_results_buf_size - 1 ||
	    status != DBG_STATUS_OK) {
		kfree(regval_buf);
		*parsed_results_bytes = res_off + 1;

		return status;
	}

	if (!qed_mdump2_regval_buf_get_num_ports(regval_buf, &num_ports)) {
		kfree(regval_buf);
		status = DBG_STATUS_MDUMP2_ERROR_EXTRACTING_NUM_PORTS;
		*parsed_results_bytes = res_off + 1;

		return status;
	}

	status = qed_display_chip_code(&mdump2_cfg, regval_buf,
				       res_buf, &res_off);
	if (status == DBG_STATUS_OK) {
		u32 display_mode, display_param;

		res_off += STR_APPEND(res_buf,
				      res_off,
				      "\n------------------------------------------------------------------ - ");
		for (display_mode = ITEM; display_mode <= num_ports;
		     display_mode++) {
			if (display_mode > 0) {
				if (!qed_retrieve_status_from_mfw(&mdump2_cfg,
								  regval_buf,
								  display_mode
								  - 1)) {
					status =
					    DBG_STATUS_MDUMP2_ERROR_EXTRACTING_MFW_STATUS;
					break;
				}
				qed_read_pcs_status_reg(&mdump2_cfg,
							regval_buf,
							display_mode - 1,
							PCS_STATUS_REG_LINK);
			}

			switch (display_mode) {
			case 0:
				res_off += STR_APPEND(res_buf, res_off,
						      "\nITEM                ");
				break;
			case 1:
				res_off += STR_APPEND(res_buf, res_off,
						      "PORT0         ");
				break;
			case 2:
				res_off += STR_APPEND(res_buf, res_off,
						      "PORT1         ");
				break;
			case 3:
				res_off += STR_APPEND(res_buf, res_off,
						      "PORT2         ");
				break;
			case 4:
				res_off += STR_APPEND(res_buf, res_off,
						      "PORT3         ");
				break;
			default:
				break;
			}
		}
		res_off += STR_APPEND(res_buf,
				      res_off,
				      "\n------------------------------------------------------------------ - ");

		for (display_param = LINK_STATE;
		     display_param <= MFW_RAW_LINK_STATUS &&
		     status == DBG_STATUS_OK; display_param++)
			for (display_mode = ITEM;
			     display_mode <= num_ports &&
			     status == DBG_STATUS_OK; display_mode++)
				if (!qed_link_dump_display(&mdump2_cfg,
							   regval_buf, res_buf,
							   &res_off,
							   display_mode,
							   display_param))
					status =
					    DBG_STATUS_MDUMP2_ERROR_DISPLAYING_LINKDUMP;
	}
	res_off += STR_APPEND(res_buf, res_off, "\n");

	/* For AH, display phy_dump: */
	if (status == DBG_STATUS_OK && qed_is_k2_chip(regval_buf))
		if (!qed_phy_dump_display(&mdump2_cfg, regval_buf,
					  res_buf, &res_off))
			status = DBG_STATUS_MDUMP2_ERROR_DISPLAYING_LINKDUMP;

	kfree(regval_buf);

	*parsed_results_bytes = res_off + 1;

	return status;
}

/**
 * Get the parsed linkdump/phydump results size
 *   Params: dump_buf - Binary input
 *           num_dumped_dwords -  mdump2 binary input dwords length
 *           results_buf_size - Textual output result size
 */
enum dbg_status qed_get_linkdump_phydump_results_buf_size(struct qed_hwfn
							  *p_hwfn,
							  u32 * dump_buf,
							  u32 num_dumped_dwords,
							  u32 *
							  results_buf_size)
{
	/* set results_buf = NULL, for getting the result buff size
	 * (No actual dump in this stage)
	 */
	return qed_parse_mdump2_dump_to_linkdump(p_hwfn,
						 dump_buf,
						 num_dumped_dwords,
						 NULL, results_buf_size);
}

/**
 * Parse the linkdump/phydump results and print
 *  Params: dump_buf - Binary input
 *           num_dumped_dwords -  mdump2 binary input dwords length (expected)
 *           results_buf - Textual result output
 */
enum dbg_status qed_print_linkdump_phydump_results(struct qed_hwfn *p_hwfn,
						   u32 * dump_buf,
						   u32 num_dumped_dwords,
						   char *results_buf,
						   u32 results_buf_size)
{
	return qed_parse_mdump2_dump_to_linkdump(p_hwfn,
						 dump_buf,
						 num_dumped_dwords,
						 results_buf,
						 &results_buf_size);
}

static void qed_dword_le2be_buf_inplace(u32 * buf, u32 dword_len)
{
	if (buf) {
		while (dword_len--) {
			*buf = be32_to_cpu(*buf);
			buf++;
		}
	}
}

#define MDUMP2_CRC_SIZE 4

static enum dbg_status qed_linkdump_phydump(struct qed_hwfn *p_hwfn,
					    struct qed_ptt *p_ptt,
					    u32 * dump_buf,
					    bool dump, u32 * num_dumped_dwords)
{
	u32 buff_byte_size, buff_byte_addr = 0;
	u32 word_offset = 0;

	/* Issues an mcp Read within it: ECore will have to implement this */
	if (qed_mdump2_req_offsize(p_hwfn, p_ptt, &buff_byte_size,
				   &buff_byte_addr))
		return DBG_STATUS_MDUMP2_FAILED_TO_REQUEST_OFFSIZE;

	if (buff_byte_size == 0 || buff_byte_addr < MCP_REG_SCRATCH ||
	    buff_byte_addr >= MCP_REG_SCRATCH +
	    DWORDS_TO_BYTES(MCP_REG_SCRATCH_SIZE)) {
		DP_INFO(p_hwfn,
			"Received addr 0x%x size 0x%x, Scratchpad address from 0x%lx to 0x%lx\n",
			buff_byte_addr,
			buff_byte_size,
			MCP_REG_SCRATCH,
			(MCP_REG_SCRATCH +
			 DWORDS_TO_BYTES(MCP_REG_SCRATCH_SIZE)));
		return DBG_STATUS_MDUMP2_FAILED_TO_REQUEST_OFFSIZE;
	}

	/* Read data from scratchpad into dump buffer. It includes a trailing
	 * CRC 32bit value, which shall be verified (and included in the dumped
	 * buf)
	 */
	buff_byte_size -= MDUMP2_CRC_SIZE;
	word_offset += qed_grc_dump_addr_range(p_hwfn,
					       p_ptt,
					       dump_buf,
					       dump,
					       BYTES_TO_DWORDS(buff_byte_addr),
					       BYTES_TO_DWORDS(buff_byte_size),
					       false, SPLIT_TYPE_NONE, 0);

	qed_mdump2_req_free(p_hwfn, p_ptt);

	if (dump) {
		u32 exp_crc = qed_rd(p_hwfn, p_ptt,
				     buff_byte_addr + buff_byte_size);
		u32 calc_crc;
		u32 *p_tmp_buf;

		/* Calculate the CRC of read data */
		p_tmp_buf = vzalloc(buff_byte_size);
		if (!p_tmp_buf) {
			DP_ERR(p_hwfn, "Buffer allocation failed\n");
			return DBG_STATUS_MDUMP2_ERROR_ALLOCATING_BUF;
		}
		memcpy(p_tmp_buf, dump_buf, buff_byte_size);
		qed_dword_le2be_buf_inplace(p_tmp_buf,
					    BYTES_TO_DWORDS(buff_byte_size));
		calc_crc = ~crc32(0xffffffff, (u8 *) p_tmp_buf, buff_byte_size);

		vfree(p_tmp_buf);

		/* Include the CRC in the dumped data (for sanity) */
		dump_buf[word_offset] = calc_crc;

		if (calc_crc != exp_crc) {
			DP_ERR(p_hwfn,
			       "mdump2 crc mismatch, expected=%x calculated=%x\n",
			       exp_crc, calc_crc);
			*num_dumped_dwords = 0;
			return DBG_STATUS_MDUMP2_FAILED_VALIDATION_OF_DATA_CRC;
		}
	}

	/* Additional word for CRC */
	*num_dumped_dwords = word_offset++;

	return DBG_STATUS_OK;
}

/**
 * @brief qed_dbg_linkdump_phydump_get_dump_buf_size - Calculates size in
 * dwords of required buffer for linkdump/phydump data.
 *
 * @param p_ptt -		  Ptt window used for writing the registers.
 * @param buf_dword_size - a pointer to the binary data with debug arrays.
 */
enum dbg_status qed_dbg_linkdump_phydump_get_dump_buf_size(struct qed_hwfn
							   *p_hwfn,
							   struct qed_ptt
							   *p_ptt,
							   u32 * buf_dword_size)
{
	enum dbg_status status = qed_dbg_dev_init(p_hwfn);

	*buf_dword_size = 0;

	if (status != DBG_STATUS_OK)
		return status;

	return qed_linkdump_phydump(p_hwfn, p_ptt, NULL, false, buf_dword_size);
}

/**
 * @brief qed_dbg_linkdump_phydump_dump - Effectively performs linkdump/phydump
 *
 * @param p_ptt -	      Ptt window used for writing the registers.
 * @param dump_buf -	      Pointer to copy the data into.
 * @param buf_size_in_dwords -     Size of the specified buffer in dwords.
 * @param num_dumped_dwords  - OUT: number of dumped dwords.
 *
 * @return error if the specified dump buffer is too small
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_linkdump_phydump_dump(struct qed_hwfn *p_hwfn,
					      struct qed_ptt *p_ptt,
					      u32 * dump_buf,
					      u32
					      buf_size_in_dwords,
					      u32 * num_dumped_dwords)
{
	u32 needed_buf_size_in_dwords;
	enum dbg_status status;

	*num_dumped_dwords = 0;

	status = qed_dbg_linkdump_phydump_get_dump_buf_size(p_hwfn,
							    p_ptt,
							    &needed_buf_size_in_dwords);
	if (status != DBG_STATUS_OK)
		return status;

	if (buf_size_in_dwords < needed_buf_size_in_dwords)
		return DBG_STATUS_DUMP_BUF_TOO_SMALL;

	return qed_linkdump_phydump(p_hwfn,
				    p_ptt, dump_buf, true, num_dumped_dwords);
}

#ifdef CONFIG_DEBUG_FS

#ifndef QED_UPSTREAM
static uint dbg_send_uevent = 1;
module_param(dbg_send_uevent, uint, S_IRUGO);
MODULE_PARM_DESC(dbg_send_uevent,
		 " Send a uevent upon saving all debug data to internal buffer (0 do-not-send; 1 send (default))");

static char dbg_data_path[QED_DBG_DATA_PATH_MAX_SIZE];
module_param_string(dbg_data_path, dbg_data_path, sizeof(dbg_data_path),
		    S_IRUGO);
MODULE_PARM_DESC(dbg_data_path, " Path for debug data files.\n"
		 "\t\tA non-empty path enables saving auto-collected debug data to file. No uevent will be sent in this case.");
#endif

static DEFINE_MUTEX(qed_dbg_lock);

static struct dentry *qed_dbg_root;

#ifdef _HAS_SYSFS_BIN_ATTR_INIT
static struct kobject *qed_sysfs_root;
struct qed_sysfs_obj {
	struct bin_attribute bin_attr;
	struct kobject *bdf_kobj;
	struct list_head list;
};

static struct list_head qed_sysfs_tests_head;
static struct list_head qed_sysfs_phy_head;
#define __init_sysfs(feature) \
{ \
	sysfs_bin_attr_init(&feature##_info->bin_attr);\
	feature##_info->bin_attr.attr.mode = 0600;\
	feature##_info->bin_attr.size = 4096;\
	feature##_info->bin_attr.read = sysfs_show;\
	feature##_info->bin_attr.write = sysfs_store;\
	feature##_info->bin_attr.private = cdev;\
	feature##_info->bdf_kobj = NULL;\
}
#endif

#define MAX_PHY_RESULT_BUFFER 9000

/******************************** Feature Meta data section ************************************/
#define BUS_NUM_STR_FUNCS 16
#define GRC_NUM_STR_FUNCS 2
#define IDLE_CHK_NUM_STR_FUNCS 1
#define MCP_TRACE_NUM_STR_FUNCS 1
#define REG_FIFO_NUM_STR_FUNCS 1
#define IGU_FIFO_NUM_STR_FUNCS 1
#define PROTECTION_OVERRIDE_NUM_STR_FUNCS 1
#define FW_ASSERTS_NUM_STR_FUNCS 1
#define ILT_NUM_STR_FUNCS 1
#define INTERNAL_TRACE_NUM_STR_FUNCS 1
#define LINKDUMP_PHYDUMP_NUM_STR_FUNCS 1
#ifndef QED_UPSTREAM		/* ! QED_UPSTREAM */
#define TESTS_NUM_STR_FUNCS 115
#endif
#define PHY_NUM_STR_FUNCS 20

struct qed_func_lookup {
	const char *key;
	int (*str_func) (struct qed_hwfn * p_hwfn, struct qed_ptt * p_ptt,
			 char *params_string);
};

/* lookup tables for finding the correct hsi function from user command */
const static struct qed_func_lookup qed_bus_hsi_func_lookup[] = {
	{"reset", qed_str_bus_reset},
	{"set_pci_output", qed_str_bus_set_pci_output},
	{"set_nw_output", qed_str_bus_set_nw_output},
	{"enable_block", qed_str_bus_enable_block},
	{"enable_storm", qed_str_bus_enable_storm},
	{"enable_timestamp", qed_str_bus_enable_timestamp},
	{"add_eid_range_sem_filter", qed_str_bus_add_eid_range_sem_filter},
	{"add_eid_mask_sem_filter", qed_str_bus_add_eid_mask_sem_filter},
	{"add_cid_sem_filter", qed_str_bus_add_cid_sem_filter},
	{"enable_filter", qed_str_bus_enable_filter},
	{"enable_trigger", qed_str_bus_enable_trigger},
	{"add_trigger_state", qed_str_bus_add_trigger_state},
	{"add_constraint", qed_str_bus_add_constraint},
	{"start", qed_str_bus_start},
	{"stop", qed_str_bus_stop},
	{"dump", qed_str_bus_dump},
};

const static struct qed_func_lookup qed_grc_hsi_func_lookup[] = {
	{"config", qed_str_grc_config},
	{"dump", qed_str_grc_dump},
};

const static struct qed_func_lookup qed_idle_chk_hsi_func_lookup[] = {
	{"dump", qed_str_idle_chk_dump},
};

const static struct qed_func_lookup qed_mcp_trace_hsi_func_lookup[] = {
	{"dump", qed_str_mcp_trace_dump},
};

const static struct qed_func_lookup qed_reg_fifo_hsi_func_lookup[] = {
	{"dump", qed_str_reg_fifo_dump},
};

const static struct qed_func_lookup qed_igu_fifo_hsi_func_lookup[] = {
	{"dump", qed_str_igu_fifo_dump},
};

const static struct qed_func_lookup qed_protection_override_hsi_func_lookup[] = {
	{"dump", qed_str_protection_override_dump},
};

const static struct qed_func_lookup qed_fw_asserts_hsi_func_lookup[] = {
	{"dump", qed_str_fw_asserts_dump},
};

const static struct qed_func_lookup qed_ilt_hsi_func_lookup[] = {
	{"dump", qed_str_ilt_dump},
};

const static struct qed_func_lookup qed_internal_trace_hsi_func_lookup[] = {
	{"dump", qed_str_internal_trace_dump},
};

const static struct qed_func_lookup qed_linkdump_phydump_hsi_func_lookup[] = {
	{"dump", qed_str_linkdump_phydump_dump},
};

#ifndef QED_UPSTREAM		/* ! QED_UPSTREAM */
const static struct qed_func_lookup qed_tests_func_lookup[] = {
	{"qm_reconf", qed_str_qm_reconf_test},
	{"ets", qed_str_ets_test},
	{"phony_dcbx", qed_str_phony_dcbx_test},
	{"mcp_halt", qed_str_mcp_halt_test},
	{"mcp_resume", qed_str_mcp_resume_test},
	{"mcp_mask_parities", qed_str_mcp_mask_parities_test},
	{"mcp_unmask_parities", qed_str_mcp_unmask_parities_test},
	{"test", qed_str_test_test},
	{"coal_vf", qed_str_coal_vf_test},
	{"gen_process_kill", qed_str_gen_process_kill_test},
	{"gen_system_kill", qed_str_gen_system_kill_test},
	{"trigger_recovery", qed_str_trigger_recovery_test},
	{"fw_assert", qed_str_fw_assert_test},
	{"dmae_err", qed_str_dmae_err_test},
	{"msix_vector_mask", qed_str_msix_vector_mask_test},
	{"msix_mask", qed_str_msix_mask_test},
	{"msix_disable", qed_str_msix_disable_test},
	{"config_obff_fsm", qed_str_config_obff_fsm_test},
	{"dump_obff_stats", qed_str_dump_obff_stats_test},
	{"set_obff_state", qed_str_set_obff_state_test},
	{"ramrod_flood", qed_str_ramrod_flood_test},
	{"gen_ramrod_stuck", qed_str_gen_ramrod_stuck_test},
	{"gen_fan_failure", qed_str_gen_fan_failure_test},
	{"bist_register", qed_str_bist_register_test},
	{"bist_clock", qed_str_bist_clock_test},
	{"bist_nvm", qed_str_bist_nvm_test},
	{"get_temperature", qed_str_get_temperature_test},
	{"get_mba_versions", qed_str_get_mba_versions_test},
	{"mcp_resc_lock", qed_str_mcp_resc_lock_test},
	{"mcp_resc_unlock", qed_str_mcp_resc_unlock_test},
	{"read_dpm_register", qed_str_read_dpm_register_test},
	{"iwarp_tcp_cids_weight", qed_str_iwarp_tcp_cids_weight_test},
	{"iwarp_ep_free_list", qed_str_iwarp_ep_free_list_test},
	{"iwarp_ep_active_list", qed_str_iwarp_ep_active_list_test},
	{"iwarp_create_listen", qed_str_iwarp_create_listen_test},
	{"iwarp_remove_listen", qed_str_iwarp_remove_listen_test},
	{"iwarp_listeners", qed_str_iwarp_listeners_test},
	{"rdma_query_stats", qed_str_rdma_query_stats_test},
	{"db_recovery_dp", qed_str_db_recovery_dp_test},
	{"db_recovery_execute", qed_str_db_recovery_execute_test},
	{"dscp_pfc_get_enable", qed_str_dscp_pfc_get_enable_test},
	{"dscp_pfc_enable", qed_str_dscp_pfc_enable_test},
	{"dscp_pfc_get", qed_str_dscp_pfc_get_test},
	{"dscp_pfc_set", qed_str_dscp_pfc_set_test},
	{"dscp_pfc_batch_get", qed_str_dscp_pfc_batch_get_test},
	{"dscp_pfc_batch_set", qed_str_dscp_pfc_batch_set_test},
	{"dcbx_set_mode", qed_str_dcbx_set_mode_test},
	{"dcbx_get_mode", qed_str_dcbx_get_mode_test},
	{"dcbx_get_pfc", qed_str_dcbx_get_pfc_test},
	{"dcbx_set_pfc", qed_str_dcbx_set_pfc_test},
	{"dcbx_get_pri_to_tc", qed_str_dcbx_get_pri_to_tc_test},
	{"dcbx_set_pri_to_tc", qed_str_dcbx_set_pri_to_tc_test},
	{"dcbx_get_tc_bw", qed_str_dcbx_get_tc_bw_test},
	{"dcbx_get_tc_tsa", qed_str_dcbx_get_tc_tsa_test},
	{"dcbx_set_tc_bw_tsa", qed_str_dcbx_set_tc_bw_tsa_test},
	{"dcbx_get_num_tcs", qed_str_dcbx_get_num_tcs_test},
	{"dcbx_get_ets_tcs", qed_str_dcbx_get_ets_tcs_test},
	{"dcbx_app_tlv_get_count", qed_str_dcbx_app_tlv_get_count_test},
	{"dcbx_app_tlv_get_value_by_idx",
	 qed_str_dcbx_app_tlv_get_value_by_idx_test},
	{"dcbx_app_tlv_get_type_by_idx",
	 qed_str_dcbx_app_tlv_get_type_by_idx_test},
	{"dcbx_app_tlv_get_pri_by_idx",
	 qed_str_dcbx_app_tlv_get_pri_by_idx_test},
	{"dcbx_app_tlv_set_app", qed_str_dcbx_app_tlv_set_app_test},
	{"dcbx_get_willing", qed_str_dcbx_get_willing_test},
	{"dcbx_set_willing", qed_str_dcbx_set_willing_test},
	{"dcbx_hw_commit", qed_str_dcbx_hw_commit_test},
	{"dcbx_set_cfg_commit", qed_str_dcbx_set_cfg_commit_test},
	{"dcbx_app_tlv_del_all", qed_str_dcbx_app_tlv_del_all_test},
	{"rdma_glob_vlan_pri_en", qed_str_rdma_glob_vlan_pri_en_test},
	{"rdma_glob_get_vlan_pri_en", qed_str_rdma_glob_get_vlan_pri_en_test},
	{"rdma_glob_vlan_pri", qed_str_rdma_glob_vlan_pri_test},
	{"rdma_glob_get_vlan_pri", qed_str_rdma_glob_get_vlan_pri_test},
	{"rdma_glob_ecn_en", qed_str_rdma_glob_ecn_en_test},
	{"rdma_glob_get_ecn_en", qed_str_rdma_glob_get_ecn_en_test},
	{"rdma_glob_ecn", qed_str_rdma_glob_ecn_test},
	{"rdma_glob_get_ecn", qed_str_rdma_glob_get_ecn_test},
	{"rdma_glob_dscp_en", qed_str_rdma_glob_dscp_en_test},
	{"rdma_glob_get_dscp_en", qed_str_rdma_glob_get_dscp_en_test},
	{"rdma_glob_dscp", qed_str_rdma_glob_dscp_test},
	{"rdma_glob_get_dscp", qed_str_rdma_glob_get_dscp_test},
	{"gen_hw_err", qed_str_gen_hw_err_test},
	{"set_dev_access", qed_str_set_dev_access_test},
	{"reg_read", qed_str_reg_read_test},
	{"reg_write", qed_str_reg_write_test},
	{"dump_llh", qed_str_dump_llh_test},
	{"pq_group_count", qed_str_pq_group_count_test},
	{"pq_group_set_pq_port", qed_str_pq_group_set_pq_port_test},
	{"get_multi_tc_roce_en", qed_str_get_multi_tc_roce_en_test},
	{"get_offload_tc", qed_str_get_offload_tc_test},
	{"set_offload_tc", qed_str_set_offload_tc_test},
	{"unset_offload_tc", qed_str_unset_offload_tc_test},
	{"link_down", qed_str_link_down_test},
	{"lag_create", qed_str_lag_create_test},
	{"lag_modify", qed_str_lag_modify_test},
	{"lag_destroy", qed_str_lag_destroy_test},
	{"set_fec", qed_str_set_fec_test},
	{"get_fec", qed_str_get_fec_test},
	{"monitored_hw_addr", qed_str_monitored_hw_addr_test},
	{"get_phys_port", qed_str_get_phys_port_test},
	{"set_led", qed_str_set_led_test},
	{"nvm_get_cfg_len", qed_str_nvm_get_cfg_len_test},
	{"nvm_get_cfg", qed_str_nvm_get_cfg_test},
	{"nvm_set_cfg", qed_str_nvm_set_cfg_test},
	{"mcp_get_tx_flt_attn_en", qed_str_mcp_get_tx_flt_attn_en_test},
	{"mcp_get_rx_los_attn_en", qed_str_mcp_get_rx_los_attn_en_test},
	{"mcp_enable_tx_flt_attn", qed_str_mcp_enable_tx_flt_attn_test},
	{"mcp_enable_rx_los_attn", qed_str_mcp_enable_rx_los_attn_test},
	{"set_bw", qed_str_set_bw_test},
	{"set_trace_filter", qed_str_set_trace_filter_test},
	{"restore_trace_filter", qed_str_restore_trace_filter_test},
	{"get_print_dbg_data", qed_str_get_print_dbg_data_test},
	{"set_print_dbg_data", qed_str_set_print_dbg_data_test},
	{"esl_supported", qed_str_esl_supported_test},
	{"esl_active", qed_str_esl_active_test},
	{"gen_mdump_idlechk", qed_str_gen_mdump_idlechk_test},
	{"set_vf_stats_bin_id", qed_str_set_vf_stats_bin_id_test},
};
#endif

const static struct qed_func_lookup qed_phy_func_lookup[] = {
	{"core_write", qed_str_phy_core_write},
	{"core_read", qed_str_phy_core_read},
	{"raw_write", qed_str_phy_raw_write},
	{"raw_read", qed_str_phy_raw_read},
	{"mac_stat", qed_str_phy_mac_stat},
	{"info", qed_str_phy_info},
	{"sfp_write", qed_str_phy_sfp_write},
	{"sfp_read", qed_str_phy_sfp_read},
	{"sfp_decode", qed_str_phy_sfp_decode},
	{"sfp_get_inserted", qed_str_phy_sfp_get_inserted},
	{"sfp_get_txdisable", qed_str_phy_sfp_get_txdisable},
	{"sfp_set_txdisable", qed_str_phy_sfp_set_txdisable},
	{"sfp_get_txreset", qed_str_phy_sfp_get_txreset},
	{"sfp_get_rxlos", qed_str_phy_sfp_get_rxlos},
	{"sfp_get_eeprom", qed_str_phy_sfp_get_eeprom},
	{"gpio_write", qed_str_phy_gpio_write},
	{"gpio_read", qed_str_phy_gpio_read},
	{"gpio_info", qed_str_phy_gpio_info},
	{"extphy_read", qed_str_phy_extphy_read},
	{"extphy_write", qed_str_phy_extphy_write},
};

/* wrapper for unifying the idle_chk and mcp_trace api */
static enum dbg_status qed_print_idle_chk_results_wrapper(struct qed_hwfn
							  *p_hwfn,
							  u32 * dump_buf,
							  u32 num_dumped_dwords,
							  char *results_buf,
							  u32 results_buf_size)
{
	u32 num_errors, num_warnnings;

	return qed_print_idle_chk_results(p_hwfn, dump_buf, num_dumped_dwords,
					  results_buf, results_buf_size,
					  &num_errors, &num_warnnings);
}

/* feature meta data lookup table */
static struct {
	char *name;
	u32 num_funcs;
	enum dbg_status (*get_size) (struct qed_hwfn * p_hwfn,
				     struct qed_ptt * p_ptt, u32 * size);
	enum dbg_status (*perform_dump) (struct qed_hwfn * p_hwfn,
					 struct qed_ptt * p_ptt, u32 * dump_buf,
					 u32 buf_size, u32 * dumped_dwords);
	enum dbg_status (*print_results) (struct qed_hwfn * p_hwfn,
					  u32 * dump_buf, u32 num_dumped_dwords,
					  char *results_buf,
					  u32 results_buf_size);
	enum dbg_status (*results_buf_size) (struct qed_hwfn * p_hwfn,
					     u32 * dump_buf,
					     u32 num_dumped_dwords,
					     u32 * results_buf_size);
	const struct qed_func_lookup *hsi_func_lookup;
} qed_features_lookup[] = {
	{
	"bus", BUS_NUM_STR_FUNCS, qed_dbg_bus_get_dump_buf_size,
		    qed_dbg_bus_dump, NULL, NULL, qed_bus_hsi_func_lookup}, {
	"grc", GRC_NUM_STR_FUNCS, qed_dbg_grc_get_dump_buf_size,
		    qed_dbg_grc_dump, NULL, NULL, qed_grc_hsi_func_lookup}, {
	"idle_chk", IDLE_CHK_NUM_STR_FUNCS,
		    qed_dbg_idle_chk_get_dump_buf_size,
		    qed_dbg_idle_chk_dump,
		    qed_print_idle_chk_results_wrapper,
		    qed_get_idle_chk_results_buf_size,
		    qed_idle_chk_hsi_func_lookup}, {
	"mcp_trace", MCP_TRACE_NUM_STR_FUNCS,
		    qed_dbg_mcp_trace_get_dump_buf_size,
		    qed_dbg_mcp_trace_dump, qed_print_mcp_trace_results,
		    qed_get_mcp_trace_results_buf_size,
		    qed_mcp_trace_hsi_func_lookup}, {
	"reg_fifo", REG_FIFO_NUM_STR_FUNCS,
		    qed_dbg_reg_fifo_get_dump_buf_size,
		    qed_dbg_reg_fifo_dump, qed_print_reg_fifo_results,
		    qed_get_reg_fifo_results_buf_size,
		    qed_reg_fifo_hsi_func_lookup}, {
	"igu_fifo", IGU_FIFO_NUM_STR_FUNCS,
		    qed_dbg_igu_fifo_get_dump_buf_size,
		    qed_dbg_igu_fifo_dump, qed_print_igu_fifo_results,
		    qed_get_igu_fifo_results_buf_size,
		    qed_igu_fifo_hsi_func_lookup}, {
	"protection_override", PROTECTION_OVERRIDE_NUM_STR_FUNCS,
		    qed_dbg_protection_override_get_dump_buf_size,
		    qed_dbg_protection_override_dump,
		    qed_print_protection_override_results,
		    qed_get_protection_override_results_buf_size,
		    qed_protection_override_hsi_func_lookup}, {
	"fw_asserts", FW_ASSERTS_NUM_STR_FUNCS,
		    qed_dbg_fw_asserts_get_dump_buf_size,
		    qed_dbg_fw_asserts_dump,
		    qed_print_fw_asserts_results,
		    qed_get_fw_asserts_results_buf_size,
		    qed_fw_asserts_hsi_func_lookup}, {
	"ilt", ILT_NUM_STR_FUNCS, qed_dbg_ilt_get_dump_buf_size,
		    qed_dbg_ilt_dump, NULL, NULL, qed_ilt_hsi_func_lookup}, {
	"internal_trace", INTERNAL_TRACE_NUM_STR_FUNCS,
		    qed_dbg_internal_trace_get_dump_buf_size,
		    qed_dbg_internal_trace_dump, NULL, NULL,
		    qed_internal_trace_hsi_func_lookup}, {
"linkdump_phydump", LINKDUMP_PHYDUMP_NUM_STR_FUNCS,
		    qed_dbg_linkdump_phydump_get_dump_buf_size,
		    qed_dbg_linkdump_phydump_dump,
		    qed_print_linkdump_phydump_results,
		    qed_get_linkdump_phydump_results_buf_size,
		    qed_linkdump_phydump_hsi_func_lookup},};

#ifndef QED_UPSTREAM		/* ! QED_UPSTREAM */
static const char *tests_list =
    "qm_reconf\n"
    "ets\n"
    "phony_dcbx\n"
    "mcp_halt\n"
    "mcp_resume\n"
    "mcp_mask_parities\n"
    "mcp_unmask_parities\n"
    "test\n"
    "coal_vf\n"
    "gen_process_kill\n"
    "gen_system_kill\n"
    "trigger_recovery\n"
    "fw_assert\n"
    "dmae_err\n"
    "msix_vector_mask\n"
    "msix_mask\n"
    "msix_disable\n"
    "config_obff_fsm\n"
    "dump_obff_stats\n"
    "set_obff_state\n"
    "ramrod_flood\n"
    "gen_ramrod_stuck\n"
    "gen_fan_failure\n"
    "bist_register\n"
    "bist_clock\n"
    "bist_nvm\n"
    "get_temperature\n"
    "get_mba_versions\n"
    "mcp_resc_lock\n"
    "mcp_resc_unlock\n"
    "read_dpm_register\n"
    "iwarp_tcp_cids_weight\n"
    "iwarp_ep_free_list\n"
    "iwarp_ep_active_list\n"
    "iwarp_create_listen\n"
    "iwarp_remove_listen\n"
    "iwarp_listeners\n"
    "rdma_query_stats\n"
    "db_recovery_dp\n"
    "db_recovery_execute\n"
    "dscp_pfc_get_enable\n"
    "dscp_pfc_enable\n"
    "dscp_pfc_get\n"
    "dscp_pfc_set\n"
    "dscp_pfc_batch_get\n"
    "dscp_pfc_batch_set\n"
    "dcbx_set_mode\n"
    "dcbx_get_mode\n"
    "dcbx_get_pfc\n"
    "dcbx_set_pfc\n"
    "dcbx_get_pri_to_tc\n"
    "dcbx_set_pri_to_tc\n"
    "dcbx_get_tc_bw\n"
    "dcbx_get_tc_tsa\n"
    "dcbx_set_tc_bw_tsa\n"
    "dcbx_get_num_tcs\n"
    "dcbx_get_ets_tcs\n"
    "dcbx_app_tlv_get_count\n"
    "dcbx_app_tlv_get_value_by_idx\n"
    "dcbx_app_tlv_get_type_by_idx\n"
    "dcbx_app_tlv_get_pri_by_idx\n"
    "dcbx_app_tlv_set_app\n"
    "dcbx_get_willing\n"
    "dcbx_set_willing\n"
    "dcbx_hw_commit\n"
    "dcbx_set_cfg_commit\n"
    "dcbx_app_tlv_del_all\n"
    "rdma_glob_vlan_pri_en\n"
    "rdma_glob_get_vlan_pri_en\n"
    "rdma_glob_vlan_pri\n"
    "rdma_glob_get_vlan_pri\n"
    "rdma_glob_ecn_en\n"
    "rdma_glob_get_ecn_en\n"
    "rdma_glob_ecn\n"
    "rdma_glob_get_ecn\n"
    "rdma_glob_dscp_en\n"
    "rdma_glob_get_dscp_en\n"
    "rdma_glob_dscp\n"
    "rdma_glob_get_dscp\n"
    "gen_hw_err\n"
    "set_dev_access\n"
    "reg_read\n"
    "reg_write\n"
    "dump_llh\n"
    "pq_group_count\n"
    "pq_group_set_pq_port\n"
    "get_multi_tc_roce_en\n"
    "get_offload_tc\n"
    "set_offload_tc\n"
    "unset_offload_tc\n"
    "link_down\n"
    "lag_create\n"
    "lag_modify\n"
    "lag_destroy\n"
    "set_fec\n"
    "get_fec\n"
    "monitored_hw_addr\n"
    "get_phys_port\n"
    "set_led\n"
    "nvm_get_cfg_len\n"
    "nvm_get_cfg\n"
    "nvm_set_cfg\n"
    "mcp_get_tx_flt_attn_en\n"
    "mcp_get_rx_los_attn_en\n"
    "mcp_enable_tx_flt_attn\n"
    "mcp_enable_rx_los_attn\n"
    "set_bw\n"
    "set_trace_filter\n"
    "restore_trace_filter\n"
    "get_print_dbg_data\n"
    "set_print_dbg_data\n"
    "esl_supported\n"
    "esl_active\n" "gen_mdump_idlechk\n" "set_vf_stats_bin_id\n";
#endif

static const char *phy_list =
    "core_write\n"
    "core_read\n"
    "raw_write\n"
    "raw_read\n"
    "mac_stat\n"
    "info\n"
    "sfp_write\n"
    "sfp_read\n"
    "sfp_decode\n"
    "sfp_get_inserted\n"
    "sfp_get_txdisable\n"
    "sfp_set_txdisable\n"
    "sfp_get_txreset\n"
    "sfp_get_rxlos\n"
    "sfp_get_eeprom\n"
    "gpio_write\n" "gpio_read\n" "gpio_info\n" "extphy_read\n" "extphy_write\n";

#define ENGINE_NUM_STR_FUNCS 1

const static struct qed_func_lookup qed_engine_func_lookup[] = {
	{"engine", qed_str_engine},
};

/******************************** end of Feature meta data section *****************************/

static void qed_dbg_print_feature(u8 * p_text_buf, u32 text_size)
{
	u32 i, precision = 80;

	if (!p_text_buf)
		return;

	pr_notice("\n%.*s", precision, p_text_buf);
	for (i = precision; i < text_size; i += precision)
		pr_cont("%.*s", precision, p_text_buf + i);
	pr_cont("\n");
}

#define QED_RESULTS_BUF_MIN_SIZE 16
/* generic function for decoding debug feature info. */
static enum dbg_status format_feature(struct qed_hwfn *p_hwfn,
				      enum qed_dbg_features feature_idx)
{
	struct qed_dbg_feature *feature =
	    &p_hwfn->cdev->dbg_features[feature_idx];
	u32 text_size_bytes;
	enum dbg_status rc;
	char *text_buf;

	/* check if feature supports formatting capability */
	if (qed_features_lookup[feature_idx].results_buf_size == NULL)
		return DBG_STATUS_OK;

	/* obtain size of formatted output */
	rc = qed_features_lookup[feature_idx].results_buf_size(p_hwfn,
							       (u32 *) feature->
							       dump_buf,
							       feature->
							       dumped_dwords,
							       &text_size_bytes);
	if (rc != DBG_STATUS_OK)
		return rc;

	/* Make sure that the allocated size is a multiple of dword (4 bytes) */
	text_size_bytes = (text_size_bytes + 3) & ~0x3;

	if (text_size_bytes < QED_RESULTS_BUF_MIN_SIZE) {
		DP_NOTICE(p_hwfn->cdev,
			  "formatted size of feature was too small %d. Aborting\n",
			  text_size_bytes);
		return DBG_STATUS_INVALID_ARGS;
	}

	/* allocate temp text buf */
	text_buf = vzalloc(text_size_bytes);
	if (!text_buf) {
		DP_NOTICE(p_hwfn->cdev,
			  "failed to allocate text buffer. Aborting\n");
		return DBG_STATUS_VIRT_MEM_ALLOC_FAILED;
	}

	/* decode feature opcodes to string on temp buf */
	rc = qed_features_lookup[feature_idx].print_results(p_hwfn,
							    (u32 *) feature->
							    dump_buf,
							    feature->
							    dumped_dwords,
							    text_buf,
							    text_size_bytes);
	if (rc != DBG_STATUS_OK) {
		vfree(text_buf);
		return rc;
	}

	/* dump printable feature to log */
	if (p_hwfn->cdev->print_dbg_data)
		qed_dbg_print_feature(text_buf, text_size_bytes);

	/* dump binary data as is to the output file */
	if (p_hwfn->cdev->dbg_bin_dump) {
		vfree(text_buf);
		return rc;
	}

	/* free the old dump_buf and point the dump_buf to the newly allocagted and formatted text buffer */
	vfree(feature->dump_buf);
	feature->dump_buf = text_buf;
	feature->buf_size = text_size_bytes;
	feature->dumped_dwords = text_size_bytes / 4;

	return rc;
}

#define MAX_DBG_FEATURE_SIZE_DWORDS	0x3FFFFFFF

/* generic function for performing the dump of a debug feature. */
static enum dbg_status qed_dbg_dump(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt,
				    enum qed_dbg_features feature_idx)
{
	struct qed_dbg_feature *feature =
	    &p_hwfn->cdev->dbg_features[feature_idx];
	u32 buf_size_dwords;
	enum dbg_status rc;

	DP_NOTICE(p_hwfn->cdev, "Collecting a debug feature [\"%s\"]\n",
		  qed_features_lookup[feature_idx].name);

	/* dump_buf was already allocated need to free (this can happen if dump was called but file was never read).
	 * We can't use the buffer as is since size may have changed
	 */
	if (feature->dump_buf) {
		vfree(feature->dump_buf);
		feature->dump_buf = NULL;
	}

	/* get buffer size from hsi, allocate accordingly, and perform the dump */
	rc = qed_features_lookup[feature_idx].get_size(p_hwfn, p_ptt,
						       &buf_size_dwords);
	if (rc != DBG_STATUS_OK && rc != DBG_STATUS_NVRAM_GET_IMAGE_FAILED)
		return rc;

	if (buf_size_dwords > MAX_DBG_FEATURE_SIZE_DWORDS) {
		feature->buf_size = 0;
		DP_NOTICE(p_hwfn->cdev,
			  "Debug feature [\"%s\"] size (0x%x dwords) exceeds maximum size (0x%x dwords)\n",
			  qed_features_lookup[feature_idx].name,
			  buf_size_dwords, MAX_DBG_FEATURE_SIZE_DWORDS);

		return DBG_STATUS_OK;
	}

	feature->buf_size = buf_size_dwords * sizeof(u32);
	feature->dump_buf = vmalloc(feature->buf_size);
	if (!feature->dump_buf)
		return DBG_STATUS_VIRT_MEM_ALLOC_FAILED;

	rc = qed_features_lookup[feature_idx].perform_dump(p_hwfn, p_ptt,
							   (u32 *) feature->
							   dump_buf,
							   feature->buf_size /
							   sizeof(u32),
							   &feature->
							   dumped_dwords);

	/* if mcp is stuck we get DBG_STATUS_NVRAM_GET_IMAGE_FAILED error.
	 * In this case the buffer holds valid binary data, but we wont able
	 * to parse it (since parsing relies on data in NVRAM which is only
	 * accessible when MFW is responsive). skip the formatting but return
	 * success so that binary data is provided.
	 */
	if (rc == DBG_STATUS_NVRAM_GET_IMAGE_FAILED)
		return DBG_STATUS_OK;

	if (rc != DBG_STATUS_OK)
		return rc;

	/* format output */
	rc = format_feature(p_hwfn, feature_idx);
	return rc;
}

/* postconfig globals */
static void *postconfig_buf;
static int postconfig_bytes;

/* phy globals */
static char *p_phy_result_buf;

/******************************** wrappers ************************************/
/*** wrapper for invoking qed_dbg_bus_reset according to string parameter ***/
int qed_str_bus_reset(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		      char *params_string)
{
	u32 one_shot_en;
	u8 hw_dwords;
	u32 unify_inputs;
	u32 grc_input_en;
	char canary[4];
	int expected_args = 4, args;

	args =
	    sscanf(params_string, "%i %hhi %i %i %3s ", &one_shot_en,
		   &hw_dwords, &unify_inputs, &grc_input_en, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dbg_bus_reset(p_hwfn, p_ptt, one_shot_en, hw_dwords,
				 unify_inputs, grc_input_en);
}

/*** wrapper for invoking qed_dbg_bus_set_pci_output according to string parameter ***/
int qed_str_bus_set_pci_output(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			       char *params_string)
{
	u16 buf_size_kb;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hi %3s ", &buf_size_kb, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dbg_bus_set_pci_output(p_hwfn, p_ptt, buf_size_kb);
}

/*** wrapper for invoking qed_dbg_bus_set_nw_output according to string parameter ***/
int qed_str_bus_set_nw_output(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      char *params_string)
{
	u8 port_id;
	u32 dest_addr_lo32;
	u16 dest_addr_hi16;
	u16 data_limit_size_kb;
	u32 send_to_other_engine;
	u32 rcv_from_other_engine;
	char canary[4];
	int expected_args = 6, args;

	args =
	    sscanf(params_string, "%hhi %i %hi %hi %i %i %3s ", &port_id,
		   &dest_addr_lo32, &dest_addr_hi16, &data_limit_size_kb,
		   &send_to_other_engine, &rcv_from_other_engine, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dbg_bus_set_nw_output(p_hwfn, p_ptt, port_id, dest_addr_lo32,
					 dest_addr_hi16, data_limit_size_kb,
					 send_to_other_engine,
					 rcv_from_other_engine);
}

/*** wrapper for invoking qed_dbg_bus_enable_block according to string parameter ***/
int qed_str_bus_enable_block(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			     char *params_string)
{
	char block[MAX_NAME_LEN];
	u8 line_num;
	u8 cycle_en;
	u8 right_shift;
	u8 force_valid;
	u8 force_frame;
	char line[64];
	char canary[4];
	int expected_args = 6, args;

	args =
	    sscanf(params_string, "%s %s %hhi %hhi %hhi %hhi%3s ", block, line,
		   &cycle_en, &right_shift, &force_valid, &force_frame, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	line_num =
	    qed_dbg_get_dbg_bus_line(p_hwfn,
				     qed_dbg_get_block_id(p_hwfn, block),
				     qed_dbg_get_chip_id(p_hwfn), line);

	return qed_dbg_bus_enable_block(p_hwfn,
					qed_dbg_get_block_id(p_hwfn, block),
					line_num, cycle_en, right_shift,
					force_valid, force_frame);
}

/*** wrapper for invoking qed_dbg_bus_enable_storm according to string parameter ***/
int qed_str_bus_enable_storm(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			     char *params_string)
{
	char storm[MAX_NAME_LEN];
	char storm_mode[MAX_NAME_LEN];
	char canary[4];
	int expected_args = 2, args;

	args = sscanf(params_string, "%s %s %3s ", storm, storm_mode, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dbg_bus_enable_storm(p_hwfn, qed_dbg_get_storm_id(storm),
					qed_dbg_get_storm_mode_id(storm_mode));
}

/*** wrapper for invoking qed_dbg_bus_enable_timestamp according to string parameter ***/
int qed_str_bus_enable_timestamp(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 char *params_string)
{
	u8 valid_en;
	u8 frame_en;
	u32 tick_len;
	char canary[4];
	int expected_args = 3, args;

	args =
	    sscanf(params_string, "%hhi %hhi %i %3s ", &valid_en, &frame_en,
		   &tick_len, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dbg_bus_enable_timestamp(p_hwfn, p_ptt, valid_en, frame_en,
					    tick_len);
}

/*** wrapper for invoking qed_dbg_bus_add_eid_range_sem_filter according to string parameter ***/
int qed_str_bus_add_eid_range_sem_filter(struct qed_hwfn *p_hwfn,
					 struct qed_ptt *p_ptt,
					 char *params_string)
{
	char storm[MAX_NAME_LEN];
	u8 min_eid;
	u8 max_eid;
	char canary[4];
	int expected_args = 3, args;

	args =
	    sscanf(params_string, "%s %hhi %hhi %3s ", storm, &min_eid,
		   &max_eid, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dbg_bus_add_eid_range_sem_filter(p_hwfn,
						    qed_dbg_get_storm_id(storm),
						    min_eid, max_eid);
}

/*** wrapper for invoking qed_dbg_bus_add_eid_mask_sem_filter according to string parameter ***/
int qed_str_bus_add_eid_mask_sem_filter(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt,
					char *params_string)
{
	char storm[MAX_NAME_LEN];
	u8 eid_val;
	u8 eid_mask;
	char canary[4];
	int expected_args = 3, args;

	args =
	    sscanf(params_string, "%s %hhi %hhi %3s ", storm, &eid_val,
		   &eid_mask, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dbg_bus_add_eid_mask_sem_filter(p_hwfn,
						   qed_dbg_get_storm_id(storm),
						   eid_val, eid_mask);
}

/*** wrapper for invoking qed_dbg_bus_add_cid_sem_filter according to string parameter ***/
int qed_str_bus_add_cid_sem_filter(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt, char *params_string)
{
	char storm[MAX_NAME_LEN];
	u32 cid;
	char canary[4];
	int expected_args = 2, args;

	args = sscanf(params_string, "%s %i %3s ", storm, &cid, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dbg_bus_add_cid_sem_filter(p_hwfn,
					      qed_dbg_get_storm_id(storm), cid);
}

/*** wrapper for invoking qed_dbg_bus_enable_filter according to string parameter ***/
int qed_str_bus_enable_filter(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      char *params_string)
{
	char block[MAX_NAME_LEN];
	u8 msg_len;
	char canary[4];
	int expected_args = 2, args;

	args = sscanf(params_string, "%s %hhi %3s ", block, &msg_len, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dbg_bus_enable_filter(p_hwfn, p_ptt,
					 qed_dbg_get_block_id(p_hwfn, block),
					 msg_len);
}

/*** wrapper for invoking qed_dbg_bus_enable_trigger according to string parameter ***/
int qed_str_bus_enable_trigger(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			       char *params_string)
{
	u32 rec_pre_trigger;
	u8 pre_chunks;
	u32 rec_post_trigger;
	u32 post_cycles;
	u32 filter_pre_trigger;
	u32 filter_post_trigger;
	char canary[4];
	int expected_args = 6, args;

	args =
	    sscanf(params_string, "%i %hhi %i %i %i %i %3s ", &rec_pre_trigger,
		   &pre_chunks, &rec_post_trigger, &post_cycles,
		   &filter_pre_trigger, &filter_post_trigger, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dbg_bus_enable_trigger(p_hwfn, p_ptt, rec_pre_trigger,
					  pre_chunks, rec_post_trigger,
					  post_cycles, filter_pre_trigger,
					  filter_post_trigger);
}

/*** wrapper for invoking qed_dbg_bus_add_trigger_state according to string parameter ***/
int qed_str_bus_add_trigger_state(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string)
{
	char block[MAX_NAME_LEN];
	u8 const_msg_len;
	u16 count_to_next;
	char canary[4];
	int expected_args = 3, args;

	args =
	    sscanf(params_string, "%s %hhi %hi %3s ", block, &const_msg_len,
		   &count_to_next, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dbg_bus_add_trigger_state(p_hwfn, p_ptt,
					     qed_dbg_get_block_id(p_hwfn,
								  block),
					     const_msg_len, count_to_next);
}

/*** wrapper for invoking qed_dbg_bus_add_constraint according to string parameter ***/
int qed_str_bus_add_constraint(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			       char *params_string)
{
	char constraint_op[MAX_NAME_LEN];
	u32 data;
	u32 data_mask;
	u32 compare_frame;
	u8 frame_bit;
	u8 cycle_offset;
	u8 dword_offset_in_cycle;
	u32 is_mandatory;
	char canary[4];
	int expected_args = 8, args;

	args =
	    sscanf(params_string, "%s %i %i %i %hhi %hhi %hhi %i %3s ",
		   constraint_op, &data, &data_mask, &compare_frame, &frame_bit,
		   &cycle_offset, &dword_offset_in_cycle, &is_mandatory,
		   canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dbg_bus_add_constraint(p_hwfn, p_ptt,
					  qed_dbg_get_constraint_op_id
					  (constraint_op), data, data_mask,
					  compare_frame, frame_bit,
					  cycle_offset, dword_offset_in_cycle,
					  is_mandatory);
}

/*** wrapper for invoking qed_dbg_bus_start according to string parameter ***/
int qed_str_bus_start(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		      char *params_string)
{
	p_hwfn->cdev->recording_active = true;
	return qed_dbg_bus_start(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_dbg_bus_stop according to string parameter ***/
int qed_str_bus_stop(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		     char *params_string)
{
	return qed_dbg_bus_stop(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_dbg_bus_dump according to string parameter ***/
int qed_str_bus_dump(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		     char *params_string)
{
	p_hwfn->cdev->recording_active = false;
	return qed_dbg_dump(p_hwfn, p_ptt, DBG_FEATURE_BUS);
}

/*** wrapper for invoking qed_dbg_grc_config according to string parameter ***/
int qed_str_grc_config(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		       char *params_string)
{
	char grc_param[MAX_NAME_LEN];
	u32 val;
	char canary[4];
	int expected_args = 2, args;

	args = sscanf(params_string, "%s %i %3s ", grc_param, &val, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dbg_grc_config(p_hwfn, qed_dbg_get_grc_param_id(grc_param),
				  val);
}

/*** wrapper for invoking qed_dbg_grc_dump according to string parameter ***/
int qed_str_grc_dump(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		     char *params_string)
{
	return qed_dbg_dump(p_hwfn, p_ptt, DBG_FEATURE_GRC);
}

/*** wrapper for invoking qed_dbg_idle_chk_dump according to string parameter ***/
int qed_str_idle_chk_dump(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  char *params_string)
{
	return qed_dbg_dump(p_hwfn, p_ptt, DBG_FEATURE_IDLE_CHK);
}

/*** wrapper for invoking qed_dbg_mcp_trace_dump according to string parameter ***/
int qed_str_mcp_trace_dump(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			   char *params_string)
{
	return qed_dbg_dump(p_hwfn, p_ptt, DBG_FEATURE_MCP_TRACE);
}

/*** wrapper for invoking qed_dbg_reg_fifo_dump according to string parameter ***/
int qed_str_reg_fifo_dump(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  char *params_string)
{
	return qed_dbg_dump(p_hwfn, p_ptt, DBG_FEATURE_REG_FIFO);
}

/*** wrapper for invoking qed_dbg_igu_fifo_dump according to string parameter ***/
int qed_str_igu_fifo_dump(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  char *params_string)
{
	return qed_dbg_dump(p_hwfn, p_ptt, DBG_FEATURE_IGU_FIFO);
}

/*** wrapper for invoking qed_dbg_protection_override_dump according to string parameter ***/
int qed_str_protection_override_dump(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt, char *params_string)
{
	return qed_dbg_dump(p_hwfn, p_ptt, DBG_FEATURE_PROTECTION_OVERRIDE);
}

/*** wrapper for invoking qed_dbg_fw_asserts_dump according to string parameter ***/
int qed_str_fw_asserts_dump(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    char *params_string)
{
	return qed_dbg_dump(p_hwfn, p_ptt, DBG_FEATURE_FW_ASSERTS);
}

/*** wrapper for invoking qed_dbg_ilt_dump according to string parameter ***/
int qed_str_ilt_dump(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		     char *params_string)
{
	return qed_dbg_dump(p_hwfn, p_ptt, DBG_FEATURE_ILT);
}

/*** wrapper for invoking qed_dbg_internal_trace_dump according to string parameter ***/
int qed_str_internal_trace_dump(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				char *params_string)
{
	return qed_dbg_dump(p_hwfn, p_ptt, DBG_FEATURE_INTERNAL_TRACE);
}

/*** wrapper for invoking qed_dbg_linkdump_phydump_dump according to string parameter ***/
int qed_str_linkdump_phydump_dump(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string)
{
	return qed_dbg_dump(p_hwfn, p_ptt, DBG_FEATURE_LINKDUMP_PHYDUMP);
}

#ifndef QED_UPSTREAM		/* ! QED_UPSTREAM */
/*** wrapper for invoking qed_qm_reconf_test according to string parameter ***/
int qed_str_qm_reconf_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			   char *params_string)
{
	return qed_qm_reconf_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_ets_test according to string parameter ***/
int qed_str_ets_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		     char *params_string)
{
	return qed_ets_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_phony_dcbx_test according to string parameter ***/
int qed_str_phony_dcbx_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    char *params_string)
{
	return qed_phony_dcbx_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_mcp_halt_test according to string parameter ***/
int qed_str_mcp_halt_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  char *params_string)
{
	return qed_mcp_halt_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_mcp_resume_test according to string parameter ***/
int qed_str_mcp_resume_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    char *params_string)
{
	return qed_mcp_resume_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_mcp_mask_parities_test according to string parameter ***/
int qed_str_mcp_mask_parities_test(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt, char *params_string)
{
	return qed_mcp_mask_parities_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_mcp_unmask_parities_test according to string parameter ***/
int qed_str_mcp_unmask_parities_test(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt, char *params_string)
{
	return qed_mcp_unmask_parities_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_test_test according to string parameter ***/
int qed_str_test_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		      char *params_string)
{
	u32 rc;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%i %3s ", &rc, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_test_test(p_hwfn, p_ptt, rc);
}

/*** wrapper for invoking qed_coal_vf_test according to string parameter ***/
int qed_str_coal_vf_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			 char *params_string)
{
	u16 rx_coal;
	u16 tx_coal;
	u16 vf_id;
	char canary[4];
	int expected_args = 3, args;

	args =
	    sscanf(params_string, "%hi %hi %hi %3s ", &rx_coal, &tx_coal,
		   &vf_id, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_coal_vf_test(p_hwfn, p_ptt, rx_coal, tx_coal, vf_id);
}

/*** wrapper for invoking qed_gen_process_kill_test according to string parameter ***/
int qed_str_gen_process_kill_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string)
{
	u8 is_common_block;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &is_common_block, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_gen_process_kill_test(p_hwfn, p_ptt, is_common_block);
}

/*** wrapper for invoking qed_gen_system_kill_test according to string parameter ***/
int qed_str_gen_system_kill_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 char *params_string)
{
	return qed_gen_system_kill_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_trigger_recovery_test according to string parameter ***/
int qed_str_trigger_recovery_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string)
{
	return qed_trigger_recovery_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_fw_assert_test according to string parameter ***/
int qed_str_fw_assert_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			   char *params_string)
{
	return qed_fw_assert_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_dmae_err_test according to string parameter ***/
int qed_str_dmae_err_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  char *params_string)
{
	return qed_dmae_err_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_msix_vector_mask_test according to string parameter ***/
int qed_str_msix_vector_mask_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string)
{
	u8 vector;
	u8 b_mask;
	char canary[4];
	int expected_args = 2, args;

	args =
	    sscanf(params_string, "%hhi %hhi %3s ", &vector, &b_mask, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_msix_vector_mask_test(p_hwfn, p_ptt, vector, b_mask);
}

/*** wrapper for invoking qed_msix_mask_test according to string parameter ***/
int qed_str_msix_mask_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			   char *params_string)
{
	u8 b_mask;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &b_mask, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_msix_mask_test(p_hwfn, p_ptt, b_mask);
}

/*** wrapper for invoking qed_msix_disable_test according to string parameter ***/
int qed_str_msix_disable_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      char *params_string)
{
	u8 b_disable;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &b_disable, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_msix_disable_test(p_hwfn, p_ptt, b_disable);
}

/*** wrapper for invoking qed_config_obff_fsm_test according to string parameter ***/
int qed_str_config_obff_fsm_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 char *params_string)
{
	return qed_config_obff_fsm_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_dump_obff_stats_test according to string parameter ***/
int qed_str_dump_obff_stats_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 char *params_string)
{
	return qed_dump_obff_stats_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_set_obff_state_test according to string parameter ***/
int qed_str_set_obff_state_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				char *params_string)
{
	u8 state;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &state, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_set_obff_state_test(p_hwfn, p_ptt, state);
}

/*** wrapper for invoking qed_ramrod_flood_test according to string parameter ***/
int qed_str_ramrod_flood_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      char *params_string)
{
	u32 ramrod_amount;
	u8 blocking;
	char canary[4];
	int expected_args = 2, args;

	args =
	    sscanf(params_string, "%i %hhi %3s ", &ramrod_amount, &blocking,
		   canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_ramrod_flood_test(p_hwfn, p_ptt, ramrod_amount, blocking);
}

/*** wrapper for invoking qed_gen_ramrod_stuck_test according to string parameter ***/
int qed_str_gen_ramrod_stuck_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string)
{
	return qed_gen_ramrod_stuck_test(p_hwfn);
}

/*** wrapper for invoking qed_gen_fan_failure_test according to string parameter ***/
int qed_str_gen_fan_failure_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 char *params_string)
{
	u8 is_over_temp;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &is_over_temp, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_gen_fan_failure_test(p_hwfn, p_ptt, is_over_temp);
}

/*** wrapper for invoking qed_bist_register_test according to string parameter ***/
int qed_str_bist_register_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			       char *params_string)
{
	return qed_bist_register_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_bist_clock_test according to string parameter ***/
int qed_str_bist_clock_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    char *params_string)
{
	return qed_bist_clock_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_bist_nvm_test according to string parameter ***/
int qed_str_bist_nvm_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  char *params_string)
{
	return qed_bist_nvm_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_get_temperature_test according to string parameter ***/
int qed_str_get_temperature_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 char *params_string)
{
	return qed_get_temperature_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_get_mba_versions_test according to string parameter ***/
int qed_str_get_mba_versions_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string)
{
	return qed_get_mba_versions_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_mcp_resc_lock_test according to string parameter ***/
int qed_str_mcp_resc_lock_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			       char *params_string)
{
	u8 resource;
	u8 timeout;
	char canary[4];
	int expected_args = 2, args;

	args =
	    sscanf(params_string, "%hhi %hhi %3s ", &resource, &timeout,
		   canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_mcp_resc_lock_test(p_hwfn, p_ptt, resource, timeout);
}

/*** wrapper for invoking qed_mcp_resc_unlock_test according to string parameter ***/
int qed_str_mcp_resc_unlock_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 char *params_string)
{
	u8 resource;
	u8 force;
	char canary[4];
	int expected_args = 2, args;

	args =
	    sscanf(params_string, "%hhi %hhi %3s ", &resource, &force, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_mcp_resc_unlock_test(p_hwfn, p_ptt, resource, force);
}

/*** wrapper for invoking qed_read_dpm_register_test according to string parameter ***/
int qed_str_read_dpm_register_test(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt, char *params_string)
{
	u32 hw_addr;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%i %3s ", &hw_addr, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_read_dpm_register_test(p_hwfn, p_ptt, hw_addr);
}

/*** wrapper for invoking qed_iwarp_tcp_cids_weight_test according to string parameter ***/
int qed_str_iwarp_tcp_cids_weight_test(struct qed_hwfn *p_hwfn,
				       struct qed_ptt *p_ptt,
				       char *params_string)
{
	return qed_iwarp_tcp_cids_weight_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_iwarp_ep_free_list_test according to string parameter ***/
int qed_str_iwarp_ep_free_list_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string)
{
	return qed_iwarp_ep_free_list_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_iwarp_ep_active_list_test according to string parameter ***/
int qed_str_iwarp_ep_active_list_test(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      char *params_string)
{
	return qed_iwarp_ep_active_list_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_iwarp_create_listen_test according to string parameter ***/
int qed_str_iwarp_create_listen_test(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt, char *params_string)
{
	u32 ip_addr;
	u32 port;
	char canary[4];
	int expected_args = 2, args;

	args = sscanf(params_string, "%i %i %3s ", &ip_addr, &port, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_iwarp_create_listen_test(p_hwfn, p_ptt, ip_addr, port);
}

/*** wrapper for invoking qed_iwarp_remove_listen_test according to string parameter ***/
int qed_str_iwarp_remove_listen_test(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt, char *params_string)
{
	u32 handle_hi;
	u32 handle_lo;
	char canary[4];
	int expected_args = 2, args;

	args =
	    sscanf(params_string, "%i %i %3s ", &handle_hi, &handle_lo, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_iwarp_remove_listen_test(p_hwfn, p_ptt, handle_hi,
					    handle_lo);
}

/*** wrapper for invoking qed_iwarp_listeners_test according to string parameter ***/
int qed_str_iwarp_listeners_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 char *params_string)
{
	return qed_iwarp_listeners_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_rdma_query_stats_test according to string parameter ***/
int qed_str_rdma_query_stats_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string)
{
	return qed_rdma_query_stats_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_db_recovery_dp_test according to string parameter ***/
int qed_str_db_recovery_dp_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				char *params_string)
{
	return qed_db_recovery_dp_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_db_recovery_execute_test according to string parameter ***/
int qed_str_db_recovery_execute_test(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt, char *params_string)
{
	return qed_db_recovery_execute_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_dscp_pfc_get_enable_test according to string parameter ***/
int qed_str_dscp_pfc_get_enable_test(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt, char *params_string)
{
	return qed_dscp_pfc_get_enable_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_dscp_pfc_enable_test according to string parameter ***/
int qed_str_dscp_pfc_enable_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 char *params_string)
{
	u8 enable;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &enable, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dscp_pfc_enable_test(p_hwfn, p_ptt, enable);
}

/*** wrapper for invoking qed_dscp_pfc_get_test according to string parameter ***/
int qed_str_dscp_pfc_get_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      char *params_string)
{
	u8 index;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &index, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dscp_pfc_get_test(p_hwfn, p_ptt, index);
}

/*** wrapper for invoking qed_dscp_pfc_set_test according to string parameter ***/
int qed_str_dscp_pfc_set_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      char *params_string)
{
	u8 index;
	u8 pri_val;
	char canary[4];
	int expected_args = 2, args;

	args =
	    sscanf(params_string, "%hhi %hhi %3s ", &index, &pri_val, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dscp_pfc_set_test(p_hwfn, p_ptt, index, pri_val);
}

/*** wrapper for invoking qed_dscp_pfc_batch_get_test according to string parameter ***/
int qed_str_dscp_pfc_batch_get_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string)
{
	u8 index;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &index, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dscp_pfc_batch_get_test(p_hwfn, p_ptt, index);
}

/*** wrapper for invoking qed_dscp_pfc_batch_set_test according to string parameter ***/
int qed_str_dscp_pfc_batch_set_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string)
{
	u8 index;
	u32 pri_val;
	char canary[4];
	int expected_args = 2, args;

	args = sscanf(params_string, "%hhi %i %3s ", &index, &pri_val, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dscp_pfc_batch_set_test(p_hwfn, p_ptt, index, pri_val);
}

/*** wrapper for invoking qed_dcbx_set_mode_test according to string parameter ***/
int qed_str_dcbx_set_mode_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			       char *params_string)
{
	u8 mode;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &mode, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dcbx_set_mode_test(p_hwfn, p_ptt, mode);
}

/*** wrapper for invoking qed_dcbx_get_mode_test according to string parameter ***/
int qed_str_dcbx_get_mode_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			       char *params_string)
{
	return qed_dcbx_get_mode_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_dcbx_get_pfc_test according to string parameter ***/
int qed_str_dcbx_get_pfc_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      char *params_string)
{
	u8 priority;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &priority, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dcbx_get_pfc_test(p_hwfn, p_ptt, priority);
}

/*** wrapper for invoking qed_dcbx_set_pfc_test according to string parameter ***/
int qed_str_dcbx_set_pfc_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      char *params_string)
{
	u8 priority;
	u8 enable;
	char canary[4];
	int expected_args = 2, args;

	args =
	    sscanf(params_string, "%hhi %hhi %3s ", &priority, &enable, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dcbx_set_pfc_test(p_hwfn, p_ptt, priority, enable);
}

/*** wrapper for invoking qed_dcbx_get_pri_to_tc_test according to string parameter ***/
int qed_str_dcbx_get_pri_to_tc_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string)
{
	u8 pri;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &pri, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dcbx_get_pri_to_tc_test(p_hwfn, p_ptt, pri);
}

/*** wrapper for invoking qed_dcbx_set_pri_to_tc_test according to string parameter ***/
int qed_str_dcbx_set_pri_to_tc_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string)
{
	u8 pri;
	u8 tc;
	char canary[4];
	int expected_args = 2, args;

	args = sscanf(params_string, "%hhi %hhi %3s ", &pri, &tc, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dcbx_set_pri_to_tc_test(p_hwfn, p_ptt, pri, tc);
}

/*** wrapper for invoking qed_dcbx_get_tc_bw_test according to string parameter ***/
int qed_str_dcbx_get_tc_bw_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				char *params_string)
{
	u8 tc;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &tc, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dcbx_get_tc_bw_test(p_hwfn, p_ptt, tc);
}

/*** wrapper for invoking qed_dcbx_get_tc_tsa_test according to string parameter ***/
int qed_str_dcbx_get_tc_tsa_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 char *params_string)
{
	u8 tc;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &tc, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dcbx_get_tc_tsa_test(p_hwfn, p_ptt, tc);
}

/*** wrapper for invoking qed_dcbx_set_tc_bw_tsa_test according to string parameter ***/
int qed_str_dcbx_set_tc_bw_tsa_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string)
{
	u8 tc;
	u8 bw_pct;
	u8 tsa_type;
	char canary[4];
	int expected_args = 3, args;

	args =
	    sscanf(params_string, "%hhi %hhi %hhi %3s ", &tc, &bw_pct,
		   &tsa_type, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dcbx_set_tc_bw_tsa_test(p_hwfn, p_ptt, tc, bw_pct, tsa_type);
}

/*** wrapper for invoking qed_dcbx_get_num_tcs_test according to string parameter ***/
int qed_str_dcbx_get_num_tcs_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string)
{
	return qed_dcbx_get_num_tcs_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_dcbx_get_ets_tcs_test according to string parameter ***/
int qed_str_dcbx_get_ets_tcs_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string)
{
	return qed_dcbx_get_ets_tcs_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_dcbx_app_tlv_get_count_test according to string parameter ***/
int qed_str_dcbx_app_tlv_get_count_test(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt,
					char *params_string)
{
	return qed_dcbx_app_tlv_get_count_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_dcbx_app_tlv_get_value_by_idx_test according to string parameter ***/
int qed_str_dcbx_app_tlv_get_value_by_idx_test(struct qed_hwfn *p_hwfn,
					       struct qed_ptt *p_ptt,
					       char *params_string)
{
	u8 idx;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &idx, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dcbx_app_tlv_get_value_by_idx_test(p_hwfn, p_ptt, idx);
}

/*** wrapper for invoking qed_dcbx_app_tlv_get_type_by_idx_test according to string parameter ***/
int qed_str_dcbx_app_tlv_get_type_by_idx_test(struct qed_hwfn *p_hwfn,
					      struct qed_ptt *p_ptt,
					      char *params_string)
{
	u8 idx;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &idx, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dcbx_app_tlv_get_type_by_idx_test(p_hwfn, p_ptt, idx);
}

/*** wrapper for invoking qed_dcbx_app_tlv_get_pri_by_idx_test according to string parameter ***/
int qed_str_dcbx_app_tlv_get_pri_by_idx_test(struct qed_hwfn *p_hwfn,
					     struct qed_ptt *p_ptt,
					     char *params_string)
{
	u8 idx;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &idx, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dcbx_app_tlv_get_pri_by_idx_test(p_hwfn, p_ptt, idx);
}

/*** wrapper for invoking qed_dcbx_app_tlv_set_app_test according to string parameter ***/
int qed_str_dcbx_app_tlv_set_app_test(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      char *params_string)
{
	u8 idtype;
	u16 idval;
	u8 pri;
	char canary[4];
	int expected_args = 3, args;

	args =
	    sscanf(params_string, "%hhi %hi %hhi %3s ", &idtype, &idval, &pri,
		   canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dcbx_app_tlv_set_app_test(p_hwfn, p_ptt, idtype, idval, pri);
}

/*** wrapper for invoking qed_dcbx_get_willing_test according to string parameter ***/
int qed_str_dcbx_get_willing_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string)
{
	u8 featid;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &featid, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dcbx_get_willing_test(p_hwfn, p_ptt, featid);
}

/*** wrapper for invoking qed_dcbx_set_willing_test according to string parameter ***/
int qed_str_dcbx_set_willing_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string)
{
	u8 featid;
	u8 enable;
	char canary[4];
	int expected_args = 2, args;

	args =
	    sscanf(params_string, "%hhi %hhi %3s ", &featid, &enable, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dcbx_set_willing_test(p_hwfn, p_ptt, featid, enable);
}

/*** wrapper for invoking qed_dcbx_hw_commit_test according to string parameter ***/
int qed_str_dcbx_hw_commit_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				char *params_string)
{
	return qed_dcbx_hw_commit_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_dcbx_set_cfg_commit_test according to string parameter ***/
int qed_str_dcbx_set_cfg_commit_test(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt, char *params_string)
{
	u8 enable;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &enable, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_dcbx_set_cfg_commit_test(p_hwfn, p_ptt, enable);
}

/*** wrapper for invoking qed_dcbx_app_tlv_del_all_test according to string parameter ***/
int qed_str_dcbx_app_tlv_del_all_test(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      char *params_string)
{
	return qed_dcbx_app_tlv_del_all_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_rdma_glob_vlan_pri_en_test according to string parameter ***/
int qed_str_rdma_glob_vlan_pri_en_test(struct qed_hwfn *p_hwfn,
				       struct qed_ptt *p_ptt,
				       char *params_string)
{
	u8 pri_en_val;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &pri_en_val, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_rdma_glob_vlan_pri_en_test(p_hwfn, p_ptt, pri_en_val);
}

/*** wrapper for invoking qed_rdma_glob_get_vlan_pri_en_test according to string parameter ***/
int qed_str_rdma_glob_get_vlan_pri_en_test(struct qed_hwfn *p_hwfn,
					   struct qed_ptt *p_ptt,
					   char *params_string)
{
	return qed_rdma_glob_get_vlan_pri_en_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_rdma_glob_vlan_pri_test according to string parameter ***/
int qed_str_rdma_glob_vlan_pri_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string)
{
	u8 pri_val;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &pri_val, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_rdma_glob_vlan_pri_test(p_hwfn, p_ptt, pri_val);
}

/*** wrapper for invoking qed_rdma_glob_get_vlan_pri_test according to string parameter ***/
int qed_str_rdma_glob_get_vlan_pri_test(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt,
					char *params_string)
{
	return qed_rdma_glob_get_vlan_pri_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_rdma_glob_ecn_en_test according to string parameter ***/
int qed_str_rdma_glob_ecn_en_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string)
{
	u8 ecn_en_val;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &ecn_en_val, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_rdma_glob_ecn_en_test(p_hwfn, p_ptt, ecn_en_val);
}

/*** wrapper for invoking qed_rdma_glob_get_ecn_en_test according to string parameter ***/
int qed_str_rdma_glob_get_ecn_en_test(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      char *params_string)
{
	return qed_rdma_glob_get_ecn_en_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_rdma_glob_ecn_test according to string parameter ***/
int qed_str_rdma_glob_ecn_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			       char *params_string)
{
	u8 ecn_val;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &ecn_val, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_rdma_glob_ecn_test(p_hwfn, p_ptt, ecn_val);
}

/*** wrapper for invoking qed_rdma_glob_get_ecn_test according to string parameter ***/
int qed_str_rdma_glob_get_ecn_test(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt, char *params_string)
{
	return qed_rdma_glob_get_ecn_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_rdma_glob_dscp_en_test according to string parameter ***/
int qed_str_rdma_glob_dscp_en_test(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt, char *params_string)
{
	u8 dscp_en_val;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &dscp_en_val, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_rdma_glob_dscp_en_test(p_hwfn, p_ptt, dscp_en_val);
}

/*** wrapper for invoking qed_rdma_glob_get_dscp_en_test according to string parameter ***/
int qed_str_rdma_glob_get_dscp_en_test(struct qed_hwfn *p_hwfn,
				       struct qed_ptt *p_ptt,
				       char *params_string)
{
	return qed_rdma_glob_get_dscp_en_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_rdma_glob_dscp_test according to string parameter ***/
int qed_str_rdma_glob_dscp_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				char *params_string)
{
	u8 dscp_val;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &dscp_val, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_rdma_glob_dscp_test(p_hwfn, p_ptt, dscp_val);
}

/*** wrapper for invoking qed_rdma_glob_get_dscp_test according to string parameter ***/
int qed_str_rdma_glob_get_dscp_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string)
{
	return qed_rdma_glob_get_dscp_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_gen_hw_err_test according to string parameter ***/
int qed_str_gen_hw_err_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    char *params_string)
{
	u8 hw_err_type;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &hw_err_type, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_gen_hw_err_test(p_hwfn, p_ptt, hw_err_type);
}

/*** wrapper for invoking qed_set_dev_access_test according to string parameter ***/
int qed_str_set_dev_access_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				char *params_string)
{
	u8 enable;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &enable, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_set_dev_access_test(p_hwfn, p_ptt, enable);
}

/*** wrapper for invoking qed_reg_read_test according to string parameter ***/
int qed_str_reg_read_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  char *params_string)
{
	u32 addr;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%i %3s ", &addr, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_reg_read_test(p_hwfn, p_ptt, addr);
}

/*** wrapper for invoking qed_reg_write_test according to string parameter ***/
int qed_str_reg_write_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			   char *params_string)
{
	u32 addr;
	u32 value;
	char canary[4];
	int expected_args = 2, args;

	args = sscanf(params_string, "%i %i %3s ", &addr, &value, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_reg_write_test(p_hwfn, p_ptt, addr, value);
}

/*** wrapper for invoking qed_dump_llh_test according to string parameter ***/
int qed_str_dump_llh_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  char *params_string)
{
	return qed_dump_llh_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_pq_group_count_test according to string parameter ***/
int qed_str_pq_group_count_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				char *params_string)
{
	u8 count;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &count, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_pq_group_count_test(p_hwfn, p_ptt, count);
}

/*** wrapper for invoking qed_pq_group_set_pq_port_test according to string parameter ***/
int qed_str_pq_group_set_pq_port_test(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      char *params_string)
{
	u16 idx;
	u8 port;
	u8 tc;
	char canary[4];
	int expected_args = 3, args;

	args =
	    sscanf(params_string, "%hi %hhi %hhi %3s ", &idx, &port, &tc,
		   canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_pq_group_set_pq_port_test(p_hwfn, p_ptt, idx, port, tc);
}

/*** wrapper for invoking qed_get_multi_tc_roce_en_test according to string parameter ***/
int qed_str_get_multi_tc_roce_en_test(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      char *params_string)
{
	return qed_get_multi_tc_roce_en_test(p_hwfn);
}

/*** wrapper for invoking qed_get_offload_tc_test according to string parameter ***/
int qed_str_get_offload_tc_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				char *params_string)
{
	return qed_get_offload_tc_test(p_hwfn);
}

/*** wrapper for invoking qed_set_offload_tc_test according to string parameter ***/
int qed_str_set_offload_tc_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				char *params_string)
{
	u8 tc;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &tc, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_set_offload_tc_test(p_hwfn, p_ptt, tc);
}

/*** wrapper for invoking qed_unset_offload_tc_test according to string parameter ***/
int qed_str_unset_offload_tc_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string)
{
	return qed_unset_offload_tc_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_link_down_test according to string parameter ***/
int qed_str_link_down_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			   char *params_string)
{
	u8 link_up;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &link_up, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_link_down_test(p_hwfn, p_ptt, link_up);
}

/*** wrapper for invoking qed_lag_create_test according to string parameter ***/
int qed_str_lag_create_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    char *params_string)
{
	return qed_lag_create_test(p_hwfn);
}

/*** wrapper for invoking qed_lag_modify_test according to string parameter ***/
int qed_str_lag_modify_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    char *params_string)
{
	u8 port_id;
	u8 link_active;
	char canary[4];
	int expected_args = 2, args;

	args =
	    sscanf(params_string, "%hhi %hhi %3s ", &port_id, &link_active,
		   canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_lag_modify_test(p_hwfn, port_id, link_active);
}

/*** wrapper for invoking qed_lag_destroy_test according to string parameter ***/
int qed_str_lag_destroy_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			     char *params_string)
{
	return qed_lag_destroy_test(p_hwfn);
}

/*** wrapper for invoking qed_set_fec_test according to string parameter ***/
int qed_str_set_fec_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			 char *params_string)
{
	u16 fec_mode;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hi %3s ", &fec_mode, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_set_fec_test(p_hwfn, p_ptt, fec_mode);
}

/*** wrapper for invoking qed_get_fec_test according to string parameter ***/
int qed_str_get_fec_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			 char *params_string)
{
	return qed_get_fec_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_monitored_hw_addr_test according to string parameter ***/
int qed_str_monitored_hw_addr_test(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt, char *params_string)
{
	u32 hw_addr;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%i %3s ", &hw_addr, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_monitored_hw_addr_test(p_hwfn, p_ptt, hw_addr);
}

/*** wrapper for invoking qed_get_phys_port_test according to string parameter ***/
int qed_str_get_phys_port_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			       char *params_string)
{
	return qed_get_phys_port_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_set_led_test according to string parameter ***/
int qed_str_set_led_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			 char *params_string)
{
	u8 led_state;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &led_state, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_set_led_test(p_hwfn, p_ptt, led_state);
}

/*** wrapper for invoking qed_nvm_get_cfg_len_test according to string parameter ***/
int qed_str_nvm_get_cfg_len_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 char *params_string)
{
	u16 option_id;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hi %3s ", &option_id, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_nvm_get_cfg_len_test(p_hwfn, p_ptt, option_id);
}

/*** wrapper for invoking qed_nvm_get_cfg_test according to string parameter ***/
int qed_str_nvm_get_cfg_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			     char *params_string)
{
	u16 option_id;
	u8 entity_id;
	u16 flags;
	u8 offset;
	char canary[4];
	int expected_args = 4, args;

	args =
	    sscanf(params_string, "%hi %hhi %hi %hhi %3s ", &option_id,
		   &entity_id, &flags, &offset, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_nvm_get_cfg_test(p_hwfn, p_ptt, option_id, entity_id, flags,
				    offset);
}

/*** wrapper for invoking qed_nvm_set_cfg_test according to string parameter ***/
int qed_str_nvm_set_cfg_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			     char *params_string)
{
	u16 option_id;
	u8 entity_id;
	u16 flags;
	u32 p_buf;
	u8 offset;
	char canary[4];
	int expected_args = 5, args;

	args =
	    sscanf(params_string, "%hi %hhi %hi %i %hhi %3s ", &option_id,
		   &entity_id, &flags, &p_buf, &offset, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_nvm_set_cfg_test(p_hwfn, p_ptt, option_id, entity_id, flags,
				    p_buf, offset);
}

/*** wrapper for invoking qed_mcp_get_tx_flt_attn_en_test according to string parameter ***/
int qed_str_mcp_get_tx_flt_attn_en_test(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt,
					char *params_string)
{
	return qed_mcp_get_tx_flt_attn_en_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_mcp_get_rx_los_attn_en_test according to string parameter ***/
int qed_str_mcp_get_rx_los_attn_en_test(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt,
					char *params_string)
{
	return qed_mcp_get_rx_los_attn_en_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_mcp_enable_tx_flt_attn_test according to string parameter ***/
int qed_str_mcp_enable_tx_flt_attn_test(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt,
					char *params_string)
{
	u8 enable;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &enable, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_mcp_enable_tx_flt_attn_test(p_hwfn, p_ptt, enable);
}

/*** wrapper for invoking qed_mcp_enable_rx_los_attn_test according to string parameter ***/
int qed_str_mcp_enable_rx_los_attn_test(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt,
					char *params_string)
{
	u8 enable;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hhi %3s ", &enable, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_mcp_enable_rx_los_attn_test(p_hwfn, p_ptt, enable);
}

/*** wrapper for invoking qed_set_bw_test according to string parameter ***/
int qed_str_set_bw_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			char *params_string)
{
	u8 min_bw;
	u8 max_bw;
	char canary[4];
	int expected_args = 2, args;

	args =
	    sscanf(params_string, "%hhi %hhi %3s ", &min_bw, &max_bw, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_set_bw_test(p_hwfn, p_ptt, min_bw, max_bw);
}

/*** wrapper for invoking qed_set_trace_filter_test according to string parameter ***/
int qed_str_set_trace_filter_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string)
{
	u32 dbg_level;
	u32 dbg_modules;
	char canary[4];
	int expected_args = 2, args;

	args =
	    sscanf(params_string, "%i %i %3s ", &dbg_level, &dbg_modules,
		   canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_set_trace_filter_test(p_hwfn, dbg_level, dbg_modules);
}

/*** wrapper for invoking qed_restore_trace_filter_test according to string parameter ***/
int qed_str_restore_trace_filter_test(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      char *params_string)
{
	return qed_restore_trace_filter_test(p_hwfn);
}

/*** wrapper for invoking qed_get_print_dbg_data_test according to string parameter ***/
int qed_str_get_print_dbg_data_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string)
{
	return qed_get_print_dbg_data_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_set_print_dbg_data_test according to string parameter ***/
int qed_str_set_print_dbg_data_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string)
{
	u32 print_dbg_data;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%i %3s ", &print_dbg_data, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_set_print_dbg_data_test(p_hwfn, p_ptt, print_dbg_data);
}

/*** wrapper for invoking qed_esl_supported_test according to string parameter ***/
int qed_str_esl_supported_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			       char *params_string)
{
	return qed_esl_supported_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_esl_active_test according to string parameter ***/
int qed_str_esl_active_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    char *params_string)
{
	return qed_esl_active_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_gen_mdump_idlechk_test according to string parameter ***/
int qed_str_gen_mdump_idlechk_test(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt, char *params_string)
{
	return qed_gen_mdump_idlechk_test(p_hwfn, p_ptt);
}

/*** wrapper for invoking qed_set_vf_stats_bin_id_test according to string parameter ***/
int qed_str_set_vf_stats_bin_id_test(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt, char *params_string)
{
	u16 vf_id;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hi %3s ", &vf_id, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_set_vf_stats_bin_id_test(p_hwfn, p_ptt, vf_id);
}

#endif
/*** wrapper for invoking qed_phy_core_write according to string parameter ***/
int qed_str_phy_core_write(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			   char *params_string)
{
	u32 port;
	u32 addr;
	u32 data_lo;
	u32 data_hi;
	char canary[4];
	int expected_args = 4, args;

	args =
	    sscanf(params_string, "%i %i %i %i %3s ", &port, &addr, &data_lo,
		   &data_hi, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_core_write(p_hwfn, port, addr, data_lo, data_hi,
				  p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_core_read according to string parameter ***/
int qed_str_phy_core_read(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  char *params_string)
{
	u32 port;
	u32 addr;
	char canary[4];
	int expected_args = 2, args;

	args = sscanf(params_string, "%i %i %3s ", &port, &addr, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_core_read(p_hwfn, port, addr, p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_raw_write according to string parameter ***/
int qed_str_phy_raw_write(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  char *params_string)
{
	u32 port;
	u32 lane;
	u32 addr;
	u32 data_lo;
	u32 data_hi;
	char canary[4];
	int expected_args = 5, args;

	args =
	    sscanf(params_string, "%i %i %i %i %i %3s ", &port, &lane, &addr,
		   &data_lo, &data_hi, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_raw_write(p_hwfn, port, lane, addr, data_lo, data_hi,
				 p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_raw_read according to string parameter ***/
int qed_str_phy_raw_read(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			 char *params_string)
{
	u32 port;
	u32 lane;
	u32 addr;
	char canary[4];
	int expected_args = 3, args;

	args =
	    sscanf(params_string, "%i %i %i %3s ", &port, &lane, &addr, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_raw_read(p_hwfn, port, lane, addr, p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_mac_stat according to string parameter ***/
int qed_str_phy_mac_stat(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			 char *params_string)
{
	u32 port;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%i %3s ", &port, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_mac_stat(p_hwfn, p_ptt, port, p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_info according to string parameter ***/
int qed_str_phy_info(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		     char *params_string)
{
	return qed_phy_info(p_hwfn, p_ptt, p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_sfp_write according to string parameter ***/
int qed_str_phy_sfp_write(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  char *params_string)
{
	u32 port;
	u32 addr;
	u32 offset;
	u32 size;
	u32 val;
	char canary[4];
	int expected_args = 5, args;

	args =
	    sscanf(params_string, "%i %i %i %i %i %3s ", &port, &addr, &offset,
		   &size, &val, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_sfp_write(p_hwfn, p_ptt, port, addr, offset, size, val,
				 p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_sfp_read according to string parameter ***/
int qed_str_phy_sfp_read(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			 char *params_string)
{
	u32 port;
	u32 addr;
	u32 offset;
	u32 size;
	char canary[4];
	int expected_args = 4, args;

	args =
	    sscanf(params_string, "%i %i %i %i %3s ", &port, &addr, &offset,
		   &size, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_sfp_read(p_hwfn, p_ptt, port, addr, offset, size,
				p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_sfp_decode according to string parameter ***/
int qed_str_phy_sfp_decode(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			   char *params_string)
{
	u32 port;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%i %3s ", &port, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_sfp_decode(p_hwfn, p_ptt, port, p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_sfp_get_inserted according to string parameter ***/
int qed_str_phy_sfp_get_inserted(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 char *params_string)
{
	u32 port;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%i %3s ", &port, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_sfp_get_inserted(p_hwfn, p_ptt, port, p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_sfp_get_txdisable according to string parameter ***/
int qed_str_phy_sfp_get_txdisable(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string)
{
	u32 port;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%i %3s ", &port, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_sfp_get_txdisable(p_hwfn, p_ptt, port, p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_sfp_set_txdisable according to string parameter ***/
int qed_str_phy_sfp_set_txdisable(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string)
{
	u32 port;
	u8 txdisable;
	char canary[4];
	int expected_args = 2, args;

	args = sscanf(params_string, "%i %hhi %3s ", &port, &txdisable, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_sfp_set_txdisable(p_hwfn, p_ptt, port, txdisable,
					 p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_sfp_get_txreset according to string parameter ***/
int qed_str_phy_sfp_get_txreset(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				char *params_string)
{
	u32 port;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%i %3s ", &port, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_sfp_get_txreset(p_hwfn, p_ptt, port, p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_sfp_get_rxlos according to string parameter ***/
int qed_str_phy_sfp_get_rxlos(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      char *params_string)
{
	u32 port;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%i %3s ", &port, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_sfp_get_rxlos(p_hwfn, p_ptt, port, p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_sfp_get_eeprom according to string parameter ***/
int qed_str_phy_sfp_get_eeprom(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			       char *params_string)
{
	u32 port;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%i %3s ", &port, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_sfp_get_eeprom(p_hwfn, p_ptt, port, p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_gpio_write according to string parameter ***/
int qed_str_phy_gpio_write(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			   char *params_string)
{
	u16 gpio;
	u16 gpio_val;
	char canary[4];
	int expected_args = 2, args;

	args = sscanf(params_string, "%hi %hi %3s ", &gpio, &gpio_val, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_gpio_write(p_hwfn, p_ptt, gpio, gpio_val,
				  p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_gpio_read according to string parameter ***/
int qed_str_phy_gpio_read(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  char *params_string)
{
	u16 gpio;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hi %3s ", &gpio, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_gpio_read(p_hwfn, p_ptt, gpio, p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_gpio_info according to string parameter ***/
int qed_str_phy_gpio_info(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  char *params_string)
{
	u16 gpio;
	char canary[4];
	int expected_args = 1, args;

	args = sscanf(params_string, "%hi %3s ", &gpio, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_gpio_info(p_hwfn, p_ptt, gpio, p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_extphy_read according to string parameter ***/
int qed_str_phy_extphy_read(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    char *params_string)
{
	u16 port;
	u16 devad;
	u16 reg;
	char canary[4];
	int expected_args = 3, args;

	args =
	    sscanf(params_string, "%hi %hi %hi %3s ", &port, &devad, &reg,
		   canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_extphy_read(p_hwfn, p_ptt, port, devad, reg,
				   p_phy_result_buf);
}

/*** wrapper for invoking qed_phy_extphy_write according to string parameter ***/
int qed_str_phy_extphy_write(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			     char *params_string)
{
	u16 port;
	u16 devad;
	u16 reg;
	u16 val;
	char canary[4];
	int expected_args = 4, args;

	args =
	    sscanf(params_string, "%hi %hi %hi %hi %3s ", &port, &devad, &reg,
		   &val, canary);
	if (expected_args != args) {
		DP_NOTICE(p_hwfn->cdev, "Error: Expected %d arguments\n",
			  expected_args);
		return DBG_STATUS_INVALID_ARGS;
	}

	return qed_phy_extphy_write(p_hwfn, p_ptt, port, devad, reg, val,
				    p_phy_result_buf);
}

/*** wrapper for invoking qed_engine according to string parameter ***/
int qed_str_engine(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		   char *params_string)
{
	u8 engine_for_debug;
	int args_num = 1;

	/* convert string to int */
	if (args_num != sscanf(params_string, "%hhd", &engine_for_debug))
		return DBG_STATUS_INVALID_ARGS;

	/* check if the input is valid */
	if ((engine_for_debug != 0) && (engine_for_debug != 1)) {
		DP_NOTICE(p_hwfn->cdev, "error! input must be 0 or 1!\n");
		return DBG_STATUS_INVALID_ARGS;
	}

	/* check if the engine that trying to set exists */
	/* if trying to set engine 1 and there is one engine it's wrong */
	if (engine_for_debug == p_hwfn->cdev->num_hwfns) {
		DP_NOTICE(p_hwfn->cdev,
			  "error! you are trying to set engine that doesn't exist\n");
		return DBG_STATUS_INVALID_ARGS;
	}

	/* change the debug bus engine */
	p_hwfn->cdev->engine_for_debug = engine_for_debug;
	return DBG_STATUS_OK;
}

/************************ end of wrappers section *****************************/

int qed_dbg_grc(struct qed_dev *cdev, void *buffer, u32 * num_dumped_bytes)
{
	return qed_dbg_feature(cdev, buffer, DBG_FEATURE_GRC, num_dumped_bytes);
}

int qed_dbg_grc_size(struct qed_dev *cdev)
{
	return qed_dbg_feature_size(cdev, DBG_FEATURE_GRC);
}

int qed_dbg_idle_chk(struct qed_dev *cdev, void *buffer, u32 * num_dumped_bytes)
{
	return qed_dbg_feature(cdev, buffer, DBG_FEATURE_IDLE_CHK,
			       num_dumped_bytes);
}

int qed_dbg_idle_chk_size(struct qed_dev *cdev)
{
	return qed_dbg_feature_size(cdev, DBG_FEATURE_IDLE_CHK);
}

int qed_dbg_reg_fifo(struct qed_dev *cdev, void *buffer, u32 * num_dumped_bytes)
{
	return qed_dbg_feature(cdev, buffer, DBG_FEATURE_REG_FIFO,
			       num_dumped_bytes);
}

int qed_dbg_reg_fifo_size(struct qed_dev *cdev)
{
	return qed_dbg_feature_size(cdev, DBG_FEATURE_REG_FIFO);
}

int qed_dbg_igu_fifo(struct qed_dev *cdev, void *buffer, u32 * num_dumped_bytes)
{
	return qed_dbg_feature(cdev, buffer, DBG_FEATURE_IGU_FIFO,
			       num_dumped_bytes);
}

int qed_dbg_igu_fifo_size(struct qed_dev *cdev)
{
	return qed_dbg_feature_size(cdev, DBG_FEATURE_IGU_FIFO);
}

int qed_dbg_phy(struct qed_dev *cdev, void *buffer, u32 * num_dumped_bytes)
{
	struct qed_hwfn *p_hwfn = &cdev->hwfns[cdev->engine_for_debug];
	struct qed_ptt *p_ptt;
	int i, num_ports, rc;

	DP_NOTICE(p_hwfn->cdev, "Collecting a debug feature [\"phy_dump\"]\n");

	/* acquire ptt */
	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return -EINVAL;

	/* get phy info */
	rc = qed_phy_info(p_hwfn, p_ptt, buffer);
	if (!rc) {
		*num_dumped_bytes = strlen(buffer);

		/* run through all ports and get phy mac_stat */
		num_ports = qed_device_num_ports(cdev);
		for (i = 0; i < num_ports; i++) {
			rc = qed_phy_mac_stat(p_hwfn, p_ptt, i,
					      buffer + *num_dumped_bytes);
			if (rc)
				break;

			*num_dumped_bytes += strlen(buffer + *num_dumped_bytes);
		}
	}

	qed_ptt_release(p_hwfn, p_ptt);
	return rc;
}

int qed_dbg_phy_size(struct qed_dev *cdev)
{
	/* return max size of phy info and
	 * phy mac_stat multiplied by the number of ports
	 */
	return MAX_PHY_RESULT_BUFFER * (1 + qed_device_num_ports(cdev));
}

static int qed_dbg_nvm_image_length(struct qed_hwfn *p_hwfn,
				    enum qed_nvm_images image_id, u32 * length)
{
	struct qed_nvm_image_att image_att;
	int rc;

	*length = 0;

	rc = qed_mcp_get_nvm_image_att(p_hwfn, image_id, &image_att);
	if (rc)
		return rc;

	*length = image_att.length;

	return rc;
}

static int qed_dbg_nvm_image(struct qed_dev *cdev, void *buffer,
			     u32 * num_dumped_bytes,
			     enum qed_nvm_images image_id)
{
	struct qed_hwfn *p_hwfn = &cdev->hwfns[cdev->engine_for_debug];
	u32 len_rounded, i;
	__be32 val;
	int rc;

	*num_dumped_bytes = 0;

	rc = qed_dbg_nvm_image_length(p_hwfn, image_id, &len_rounded);
	if (rc)
		return rc;

	DP_NOTICE(p_hwfn->cdev,
		  "Collecting a debug feature [\"nvram image %d\"]\n",
		  image_id);

	len_rounded = roundup(len_rounded, sizeof(u32));
	rc = qed_mcp_get_nvm_image(p_hwfn, image_id, buffer, len_rounded);
	if (rc)
		return rc;

	/* QED_NVM_IMAGE_NVM_META image is not swapped like other images */
	if (image_id != QED_NVM_IMAGE_NVM_META)
		for (i = 0; i < len_rounded; i += 4) {
			val = cpu_to_be32(*(u32 *) (buffer + i));
			*(u32 *) (buffer + i) = val;
		}

	*num_dumped_bytes = len_rounded;

	return rc;
}

int qed_dbg_protection_override(struct qed_dev *cdev, void *buffer,
				u32 * num_dumped_bytes)
{
	return qed_dbg_feature(cdev, buffer, DBG_FEATURE_PROTECTION_OVERRIDE,
			       num_dumped_bytes);
}

int qed_dbg_protection_override_size(struct qed_dev *cdev)
{
	return qed_dbg_feature_size(cdev, DBG_FEATURE_PROTECTION_OVERRIDE);
}

int qed_dbg_fw_asserts(struct qed_dev *cdev, void *buffer,
		       u32 * num_dumped_bytes)
{
	return qed_dbg_feature(cdev, buffer, DBG_FEATURE_FW_ASSERTS,
			       num_dumped_bytes);
}

int qed_dbg_fw_asserts_size(struct qed_dev *cdev)
{
	return qed_dbg_feature_size(cdev, DBG_FEATURE_FW_ASSERTS);
}

int qed_dbg_ilt(struct qed_dev *cdev, void *buffer, u32 * num_dumped_bytes)
{
	return qed_dbg_feature(cdev, buffer, DBG_FEATURE_ILT, num_dumped_bytes);
}

int qed_dbg_ilt_size(struct qed_dev *cdev)
{
	return qed_dbg_feature_size(cdev, DBG_FEATURE_ILT);
}

int qed_dbg_mcp_trace(struct qed_dev *cdev, void *buffer,
		      u32 * num_dumped_bytes)
{
	return qed_dbg_feature(cdev, buffer, DBG_FEATURE_MCP_TRACE,
			       num_dumped_bytes);
}

int qed_dbg_mcp_trace_size(struct qed_dev *cdev)
{
	return qed_dbg_feature_size(cdev, DBG_FEATURE_MCP_TRACE);
}

int qed_dbg_internal_trace(struct qed_dev *cdev, void *buffer,
			   u32 * num_dumped_bytes)
{
	return qed_dbg_feature(cdev, buffer, DBG_FEATURE_INTERNAL_TRACE,
			       num_dumped_bytes);
}

int qed_dbg_internal_trace_size(struct qed_dev *cdev)
{
	return qed_dbg_feature_size(cdev, DBG_FEATURE_INTERNAL_TRACE);
}

int qed_dbg_linkdump_pyhdump(struct qed_dev *cdev, void *buffer,
			     u32 * num_dumped_bytes)
{
	return qed_dbg_feature(cdev, buffer, DBG_FEATURE_LINKDUMP_PHYDUMP,
			       num_dumped_bytes);
}

int qed_dbg_linkdump_phydump_size(struct qed_dev *cdev)
{
	return qed_dbg_feature_size(cdev, DBG_FEATURE_LINKDUMP_PHYDUMP);
}

/* defines the amount of bytes allocated for recording the length of
 * debugfs feature buffer @@@TBD duplicate of qede.h
 */
#define REGDUMP_HEADER_SIZE			sizeof(u32)
#define REGDUMP_HEADER_SIZE_SHIFT		0
#define REGDUMP_HEADER_SIZE_MASK		0xffffff
#define REGDUMP_HEADER_FEATURE_SHIFT		24
#define REGDUMP_HEADER_FEATURE_MASK		0x1f
#define REGDUMP_HEADER_BIN_DUMP_SHIFT		29
#define REGDUMP_HEADER_BIN_DUMP_MASK		0x1
#define REGDUMP_HEADER_OMIT_ENGINE_SHIFT	30
#define REGDUMP_HEADER_OMIT_ENGINE_MASK		0x1
#define REGDUMP_HEADER_ENGINE_SHIFT		31
#define REGDUMP_HEADER_ENGINE_MASK		0x1

#define ILT_DUMP_MAX_SIZE			(1024 * 1024 * 15)

enum debug_print_features {
	OLD_MODE = 0,
	IDLE_CHK = 1,
	GRC_DUMP = 2,
	MCP_TRACE = 3,
	REG_FIFO = 4,
	PROTECTION_OVERRIDE = 5,
	IGU_FIFO = 6,
	PHY = 7,
	FW_ASSERTS = 8,
	NVM_CFG1 = 9,
	DEFAULT_CFG = 10,
	NVM_META = 11,
	MDUMP = 12,
	ILT_DUMP = 13,
	INTERNAL_TRACE = 14,
	LINKDUMP_PHYDUMP = 15,
};

static u32 qed_calc_regdump_header(struct qed_dev *cdev,
				   enum debug_print_features feature,
				   int engine, u32 feature_size,
				   u8 omit_engine, u8 dbg_bin_dump)
{
	u32 res = 0;

	SET_FIELD(res, REGDUMP_HEADER_SIZE, feature_size);
	if (res != feature_size)
		DP_NOTICE(cdev,
			  "Feature %d is too large (size 0x%x) and will corrupt the dump\n",
			  feature, feature_size);

	SET_FIELD(res, REGDUMP_HEADER_FEATURE, feature);
	SET_FIELD(res, REGDUMP_HEADER_BIN_DUMP, dbg_bin_dump);
	SET_FIELD(res, REGDUMP_HEADER_OMIT_ENGINE, omit_engine);
	SET_FIELD(res, REGDUMP_HEADER_ENGINE, engine);

	return res;
}

static void qed_dbg_all_data_free_buf(struct qed_dev *cdev)
{
	vfree(cdev->p_dbg_data_buf);
	cdev->p_dbg_data_buf = NULL;
}

static bool qed_dbg_validate_reg_read(struct qed_hwfn *p_hwfn)
{
	struct qed_ptt *p_ptt;
	bool result;

	/* acquire ptt */
	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return false;

	result = qed_is_reg_read_valid(p_hwfn, p_ptt);

	qed_ptt_release(p_hwfn, p_ptt);
	return result;
}

int qed_dbg_all_data(struct qed_dev *cdev, void *buffer)
{
	u8 cur_engine, omit_engine = 0, org_engine;
	struct qed_hwfn *p_hwfn = &cdev->hwfns[cdev->engine_for_debug];
	struct dbg_tools_data *dev_data = &p_hwfn->dbg_info;
	int grc_params[MAX_DBG_GRC_PARAMS], rc, i;
	u32 offset = 0, feature_size;

	/* Use a previously saved buffer if exists */
	if (cdev->p_dbg_data_buf) {
		DP_NOTICE(cdev,
			  "Using a debug data buffer that was previously obtained and saved\n");
		memcpy(buffer, cdev->p_dbg_data_buf, cdev->dbg_data_buf_size);
		qed_dbg_all_data_free_buf(cdev);
		return 0;
	}

	if (!qed_dbg_validate_reg_read(p_hwfn)) {
		DP_ERR(cdev,
		       "Register reads are invalid, cannot collect debug data\n");
		return -EPERM;
	}

	for (i = 0; i < MAX_DBG_GRC_PARAMS; i++)
		grc_params[i] = dev_data->grc.param_val[i];

	if (!QED_IS_CMT(cdev))
		omit_engine = 1;

	cdev->dbg_bin_dump = 1;
	mutex_lock(&qed_dbg_lock);

	org_engine = qed_get_debug_engine(cdev);
	for (cur_engine = 0; cur_engine < cdev->num_hwfns; cur_engine++) {
		/* collect idle_chks and grcDump for each hw function */
		DP_VERBOSE(cdev, QED_MSG_DEBUG,
			   "obtaining idle_chk and grcdump for current engine\n");
		qed_set_debug_engine(cdev, cur_engine);

		/* first idle_chk */
		rc = qed_dbg_idle_chk(cdev, (u8 *) buffer + offset +
				      REGDUMP_HEADER_SIZE, &feature_size);
		if (!rc) {
			*(u32 *) ((u8 *) buffer + offset) =
			    qed_calc_regdump_header(cdev, IDLE_CHK,
						    cur_engine,
						    feature_size,
						    omit_engine,
						    cdev->dbg_bin_dump);
			offset += (feature_size + REGDUMP_HEADER_SIZE);
		} else {
			DP_ERR(cdev, "qed_dbg_idle_chk failed. rc = %d\n", rc);
		}

		/* second idle_chk */
		rc = qed_dbg_idle_chk(cdev, (u8 *) buffer + offset +
				      REGDUMP_HEADER_SIZE, &feature_size);
		if (!rc) {
			*(u32 *) ((u8 *) buffer + offset) =
			    qed_calc_regdump_header(cdev, IDLE_CHK,
						    cur_engine,
						    feature_size,
						    omit_engine,
						    cdev->dbg_bin_dump);
			offset += (feature_size + REGDUMP_HEADER_SIZE);
		} else {
			DP_ERR(cdev, "qed_dbg_idle_chk failed. rc = %d\n", rc);
		}

		/* reg_fifo dump */
		rc = qed_dbg_reg_fifo(cdev, (u8 *) buffer + offset +
				      REGDUMP_HEADER_SIZE, &feature_size);
		if (!rc) {
			*(u32 *) ((u8 *) buffer + offset) =
			    qed_calc_regdump_header(cdev, REG_FIFO,
						    cur_engine,
						    feature_size,
						    omit_engine,
						    cdev->dbg_bin_dump);
			offset += (feature_size + REGDUMP_HEADER_SIZE);
		} else {
			DP_ERR(cdev, "qed_dbg_reg_fifo failed. rc = %d\n", rc);
		}

		/* igu_fifo dump */
		rc = qed_dbg_igu_fifo(cdev, (u8 *) buffer + offset +
				      REGDUMP_HEADER_SIZE, &feature_size);
		if (!rc) {
			*(u32 *) ((u8 *) buffer + offset) =
			    qed_calc_regdump_header(cdev, IGU_FIFO,
						    cur_engine,
						    feature_size,
						    omit_engine,
						    cdev->dbg_bin_dump);
			offset += (feature_size + REGDUMP_HEADER_SIZE);
		} else {
			DP_ERR(cdev, "qed_dbg_igu_fifo failed. rc = %d", rc);
		}

		/* protection_override dump */
		rc = qed_dbg_protection_override(cdev, (u8 *) buffer + offset +
						 REGDUMP_HEADER_SIZE,
						 &feature_size);
		if (!rc) {
			*(u32 *) ((u8 *) buffer + offset) =
			    qed_calc_regdump_header(cdev,
						    PROTECTION_OVERRIDE,
						    cur_engine,
						    feature_size,
						    omit_engine,
						    cdev->dbg_bin_dump);
			offset += (feature_size + REGDUMP_HEADER_SIZE);
		} else {
			DP_ERR(cdev,
			       "qed_dbg_protection_override failed. rc = %d\n",
			       rc);
		}

		/* fw_asserts dump */
		rc = qed_dbg_fw_asserts(cdev, (u8 *) buffer + offset +
					REGDUMP_HEADER_SIZE, &feature_size);
		if (!rc) {
			*(u32 *) ((u8 *) buffer + offset) =
			    qed_calc_regdump_header(cdev, FW_ASSERTS,
						    cur_engine,
						    feature_size,
						    omit_engine,
						    cdev->dbg_bin_dump);
			offset += (feature_size + REGDUMP_HEADER_SIZE);
		} else {
			DP_ERR(cdev, "qed_dbg_fw_asserts failed. rc = %d\n",
			       rc);
		}

		feature_size = qed_dbg_ilt_size(cdev);
		if (!cdev->disable_ilt_dump && feature_size < ILT_DUMP_MAX_SIZE) {
			rc = qed_dbg_ilt(cdev, (u8 *) buffer + offset +
					 REGDUMP_HEADER_SIZE, &feature_size);
			if (!rc) {
				*(u32 *) ((u8 *) buffer + offset) =
				    qed_calc_regdump_header(cdev, ILT_DUMP,
							    cur_engine,
							    feature_size,
							    omit_engine,
							    cdev->dbg_bin_dump);
				offset += (feature_size + REGDUMP_HEADER_SIZE);
			} else {
				DP_ERR(cdev, "qed_dbg_ilt failed. rc = %d\n",
				       rc);
			}
		}

		/* grc dump - must be last because when mcp stuck it will
		 * clutter idle_chk, reg_fifo, ...
		 */
		for (i = 0; i < MAX_DBG_GRC_PARAMS; i++)
			dev_data->grc.param_val[i] = grc_params[i];

		rc = qed_dbg_grc(cdev, (u8 *) buffer + offset +
				 REGDUMP_HEADER_SIZE, &feature_size);
		if (!rc) {
			*(u32 *) ((u8 *) buffer + offset) =
			    qed_calc_regdump_header(cdev, GRC_DUMP,
						    cur_engine,
						    feature_size,
						    omit_engine,
						    cdev->dbg_bin_dump);
			offset += (feature_size + REGDUMP_HEADER_SIZE);
		} else {
			DP_ERR(cdev, "qed_dbg_grc failed. rc = %d", rc);
		}
	}

	qed_set_debug_engine(cdev, org_engine);

#ifndef ASIC_ONLY
	if (!CHIP_REV_IS_EMUL(cdev)) {
#endif
		/* phy dump */
		rc = qed_dbg_phy(cdev,
				 (u8 *) buffer + offset + REGDUMP_HEADER_SIZE,
				 &feature_size);
		if (!rc) {
			*(u32 *) ((u8 *) buffer + offset) =
			    qed_calc_regdump_header(cdev, PHY, cur_engine,
						    feature_size, omit_engine,
						    0);
			offset += (feature_size + REGDUMP_HEADER_SIZE);
		} else {
			DP_ERR(cdev, "qed_dbg_phy failed. rc = %d", rc);
		}

		/* mcp_trace */
		rc = qed_dbg_mcp_trace(cdev, (u8 *) buffer + offset +
				       REGDUMP_HEADER_SIZE, &feature_size);
		if (!rc) {
			*(u32 *) ((u8 *) buffer + offset) =
			    qed_calc_regdump_header(cdev, MCP_TRACE, cur_engine,
						    feature_size, omit_engine,
						    cdev->dbg_bin_dump);
			offset += (feature_size + REGDUMP_HEADER_SIZE);
		} else {
			DP_ERR(cdev, "qed_dbg_mcp_trace failed. rc = %d\n", rc);
		}

		/* Re-populate nvm attribute info */
		qed_mcp_nvm_info_free(p_hwfn);
		qed_mcp_nvm_info_populate(p_hwfn);

		/* nvm cfg1 */
		rc = qed_dbg_nvm_image(cdev,
				       (u8 *) buffer + offset +
				       REGDUMP_HEADER_SIZE, &feature_size,
				       QED_NVM_IMAGE_NVM_CFG1);
		if (!rc) {
			*(u32 *) ((u8 *) buffer + offset) =
			    qed_calc_regdump_header(cdev, NVM_CFG1, cur_engine,
						    feature_size, omit_engine,
						    cdev->dbg_bin_dump);
			offset += (feature_size + REGDUMP_HEADER_SIZE);
		} else if (rc != -ENOENT) {
			DP_ERR(cdev,
			       "qed_dbg_nvm_image failed for image  %d (%s), rc = %d\n",
			       QED_NVM_IMAGE_NVM_CFG1, "QED_NVM_IMAGE_NVM_CFG1",
			       rc);
		}

		/* nvm default */
		rc = qed_dbg_nvm_image(cdev,
				       (u8 *) buffer + offset +
				       REGDUMP_HEADER_SIZE, &feature_size,
				       QED_NVM_IMAGE_DEFAULT_CFG);
		if (!rc) {
			*(u32 *) ((u8 *) buffer + offset) =
			    qed_calc_regdump_header(cdev, DEFAULT_CFG,
						    cur_engine, feature_size,
						    omit_engine,
						    cdev->dbg_bin_dump);
			offset += (feature_size + REGDUMP_HEADER_SIZE);
		} else if (rc != -ENOENT) {
			DP_ERR(cdev,
			       "qed_dbg_nvm_image failed for image %d (%s), rc = %d\n",
			       QED_NVM_IMAGE_DEFAULT_CFG,
			       "QED_NVM_IMAGE_DEFAULT_CFG", rc);
		}

		/* nvm meta */
		rc = qed_dbg_nvm_image(cdev,
				       (u8 *) buffer + offset +
				       REGDUMP_HEADER_SIZE, &feature_size,
				       QED_NVM_IMAGE_NVM_META);
		if (!rc) {
			*(u32 *) ((u8 *) buffer + offset) =
			    qed_calc_regdump_header(cdev, NVM_META, cur_engine,
						    feature_size, omit_engine,
						    cdev->dbg_bin_dump);
			offset += (feature_size + REGDUMP_HEADER_SIZE);
		} else if (rc != -ENOENT) {
			DP_ERR(cdev,
			       "qed_dbg_nvm_image failed for image %d (%s), rc = %d\n",
			       QED_NVM_IMAGE_NVM_META, "QED_NVM_IMAGE_NVM_META",
			       rc);
		}

		/* nvm mdump */
		rc = qed_dbg_nvm_image(cdev,
				       (u8 *) buffer + offset +
				       REGDUMP_HEADER_SIZE, &feature_size,
				       QED_NVM_IMAGE_MDUMP);
		if (!rc) {
			*(u32 *) ((u8 *) buffer + offset) =
			    qed_calc_regdump_header(cdev, MDUMP, cur_engine,
						    feature_size, omit_engine,
						    cdev->dbg_bin_dump);
			offset += (feature_size + REGDUMP_HEADER_SIZE);
		} else if (rc != -ENOENT) {
			DP_ERR(cdev,
			       "qed_dbg_nvm_image failed for image %d (%s), rc = %d\n",
			       QED_NVM_IMAGE_MDUMP, "QED_NVM_IMAGE_MDUMP", rc);
		}

		/* linkdump/phydump */
		rc = qed_dbg_linkdump_pyhdump(cdev, (u8 *) buffer + offset +
					      REGDUMP_HEADER_SIZE,
					      &feature_size);
		if (!rc) {
			*(u32 *) ((u8 *) buffer + offset) =
			    qed_calc_regdump_header(cdev, LINKDUMP_PHYDUMP,
						    cur_engine, feature_size,
						    omit_engine,
						    cdev->dbg_bin_dump);
			offset += (feature_size + REGDUMP_HEADER_SIZE);
		} else {
			DP_ERR(cdev,
			       "qed_dbg_linkdump_pyhdump failed. rc = %d\n",
			       rc);
		}

#ifndef ASIC_ONLY
	}
#endif

	/* internal trace */
	if (!cdev->disable_internal_trace_dump) {
		rc = qed_dbg_internal_trace(cdev, (u8 *) buffer + offset +
					    REGDUMP_HEADER_SIZE, &feature_size);
		if (!rc) {
			*(u32 *) ((u8 *) buffer + offset) =
			    qed_calc_regdump_header(cdev,
						    INTERNAL_TRACE,
						    cur_engine,
						    feature_size,
						    omit_engine,
						    cdev->dbg_bin_dump);
			offset += (feature_size + REGDUMP_HEADER_SIZE);
		} else {
			DP_ERR(cdev,
			       "qed_dbg_internal_trace failed. rc = %d\n", rc);
		}
	}

	mutex_unlock(&qed_dbg_lock);
	cdev->dbg_bin_dump = 0;

	return 0;
}

int qed_dbg_all_data_size(struct qed_dev *cdev)
{
	u32 regs_len = 0, image_len = 0, ilt_len = 0, total_ilt_len = 0;
	struct qed_hwfn *p_hwfn = &cdev->hwfns[cdev->engine_for_debug];
	u8 cur_engine, org_engine;

	/* Use a previously saved buffer size if exists */
	if (cdev->p_dbg_data_buf)
		return cdev->dbg_data_buf_size;

	cdev->disable_ilt_dump = false;
	cdev->disable_internal_trace_dump = false;
	org_engine = qed_get_debug_engine(cdev);
	for (cur_engine = 0; cur_engine < cdev->num_hwfns; cur_engine++) {
		/* engine specific */
		DP_VERBOSE(cdev, QED_MSG_DEBUG,
			   "calculating idle_chk and grcdump register length for current engine\n");
		qed_set_debug_engine(cdev, cur_engine);
		regs_len += REGDUMP_HEADER_SIZE + qed_dbg_idle_chk_size(cdev) +
		    REGDUMP_HEADER_SIZE + qed_dbg_idle_chk_size(cdev) +
		    REGDUMP_HEADER_SIZE + qed_dbg_grc_size(cdev) +
		    REGDUMP_HEADER_SIZE + qed_dbg_reg_fifo_size(cdev) +
		    REGDUMP_HEADER_SIZE + qed_dbg_igu_fifo_size(cdev) +
		    REGDUMP_HEADER_SIZE +
		    qed_dbg_protection_override_size(cdev) +
		    REGDUMP_HEADER_SIZE + qed_dbg_fw_asserts_size(cdev);
		ilt_len = REGDUMP_HEADER_SIZE + qed_dbg_ilt_size(cdev);
		if (ilt_len < ILT_DUMP_MAX_SIZE) {
			total_ilt_len += ilt_len;
			regs_len += ilt_len;
		}
	}

	qed_set_debug_engine(cdev, org_engine);

#ifndef ASIC_ONLY
	if (!CHIP_REV_IS_EMUL(cdev)) {
#endif
		/* engine common */
		regs_len += REGDUMP_HEADER_SIZE + qed_dbg_mcp_trace_size(cdev) +
		    REGDUMP_HEADER_SIZE + qed_dbg_phy_size(cdev);
		qed_dbg_nvm_image_length(p_hwfn, QED_NVM_IMAGE_NVM_CFG1,
					 &image_len);
		if (image_len)
			regs_len += REGDUMP_HEADER_SIZE + image_len;
		qed_dbg_nvm_image_length(p_hwfn, QED_NVM_IMAGE_DEFAULT_CFG,
					 &image_len);
		if (image_len)
			regs_len += REGDUMP_HEADER_SIZE + image_len;
		qed_dbg_nvm_image_length(p_hwfn, QED_NVM_IMAGE_NVM_META,
					 &image_len);
		if (image_len)
			regs_len += REGDUMP_HEADER_SIZE + image_len;
		qed_dbg_nvm_image_length(p_hwfn, QED_NVM_IMAGE_MDUMP,
					 &image_len);
		if (image_len)
			regs_len += REGDUMP_HEADER_SIZE + image_len;
		regs_len +=
		    REGDUMP_HEADER_SIZE + qed_dbg_linkdump_phydump_size(cdev);
#ifndef ASIC_ONLY
	}
#endif
	regs_len += REGDUMP_HEADER_SIZE + qed_dbg_internal_trace_size(cdev);
	if (regs_len > REGDUMP_MAX_SIZE) {
		DP_VERBOSE(cdev, QED_MSG_DEBUG,
			   "Dump exceeds max size 0x%x, disable internal trace\n",
			   REGDUMP_MAX_SIZE);
		cdev->disable_internal_trace_dump = true;
		regs_len -= REGDUMP_HEADER_SIZE +
		    qed_dbg_internal_trace_size(cdev);
	}

	if (regs_len > REGDUMP_MAX_SIZE) {
		DP_VERBOSE(cdev, QED_MSG_DEBUG,
			   "Dump exceeds max size 0x%x, disable ILT dump\n",
			   REGDUMP_MAX_SIZE);
		cdev->disable_ilt_dump = true;
		regs_len -= total_ilt_len;
	}

	return regs_len;
}

#ifndef QED_UPSTREAM
static struct file *qed_file_open(const char *filename, int flags, umode_t mode,
				  long *err)
{
	struct file *filp = NULL;
#ifdef _HAS_GET_FS
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	filp = filp_open(filename, flags, mode);
	set_fs(old_fs);
#else
	filp = filp_open(filename, flags, mode);
#endif
	if (IS_ERR(filp)) {
		*err = PTR_ERR(filp);
		return NULL;
	}

	return filp;
}

static int qed_file_close(struct file *file)
{
	return filp_close(file, NULL);
}

static ssize_t qed_file_write(struct file *file, const char *buf, size_t count,
			      loff_t * pos)
{
#ifdef _HAS_KERNEL_WRITE_V2
	return kernel_write(file, buf, count, pos);
#else
	mm_segment_t old_fs;
	ssize_t res;

	old_fs = get_fs();
	set_fs(get_ds());
	/* The cast to a user pointer is valid due to the set_fs() */
	res = vfs_write(file, (__force const char __user *)buf, count, pos);
	set_fs(old_fs);

	return res;
#endif
}

static int qed_file_sync(struct file *file)
{
#ifdef _HAS_VFS_FSYNC_V2
	return vfs_fsync(file, 0);
#else
	return vfs_fsync(file, file->f_path.dentry, 0);
#endif
}

static int qed_dbg_save_to_file(struct qed_dev *cdev)
{
	size_t size = strlen(cdev->dbg_data_path);
	int rc = 0, fclose_rc = 0;
	struct file *filp = NULL;
	struct timespec64 now;
	struct tm tm_val;
	ssize_t ret = 0;
	loff_t pos = 0;
	long err = 0;

	ktime_get_real_ts64(&now);
	time_to_tm(now.tv_sec, 0, &tm_val);
	snprintf(cdev->dbg_data_path + size,
		 min_t(size_t,
		       QED_DBG_DATA_FILE_NAME_SIZE, PATH_MAX - size),
		 "/qed_dump_%02x-%02x-%x_%02d-%02d-%02ld_%02d-%02d-%02d.bin",
		 cdev->pdev->bus->number, PCI_SLOT(cdev->pdev->devfn),
		 PCI_FUNC(cdev->pdev->devfn), tm_val.tm_mon + 1,
		 (int)tm_val.tm_mday, 1900 + tm_val.tm_year,
		 tm_val.tm_hour, tm_val.tm_min, tm_val.tm_sec);

	filp = qed_file_open(cdev->dbg_data_path, O_RDWR | O_CREAT, 0600, &err);
	if (!filp) {
		DP_NOTICE(cdev,
			  "Failed to open \"%s\" for saving debug data [err %ld]\n",
			  cdev->dbg_data_path, err);
		return -EFAULT;
	}

	ret = qed_file_write(filp, cdev->p_dbg_data_buf,
			     cdev->dbg_data_buf_size, &pos);
	if (ret < 0) {
		DP_NOTICE(cdev, "Failed to write debug data to \"%s\"\n",
			  cdev->dbg_data_path);
		rc = (int)ret;
		goto out;
	}

	if (pos != cdev->dbg_data_buf_size) {
		DP_NOTICE(cdev,
			  "Failed to write all debug data to \"%s\" [written %lld, dbg_data_buf_size %d]\n",
			  cdev->dbg_data_path, pos, cdev->dbg_data_buf_size);
		rc = -EAGAIN;
		goto out;
	}

	rc = qed_file_sync(filp);
	if (rc)
		DP_NOTICE(cdev,
			  "Failed to write back data for \"%s\" to disk\n",
			  cdev->dbg_data_path);
out:
	fclose_rc = qed_file_close(filp);
	if (fclose_rc) {
		DP_NOTICE(cdev, "Failed to close \"%s\"\n",
			  cdev->dbg_data_path);

		/* The code of the first error should be returned */
		rc = !rc ? fclose_rc : rc;
	}

	if (!rc) {
		DP_NOTICE(cdev, "Saved qed debug data at \"%s\"\n",
			  cdev->dbg_data_path);
		qed_dbg_all_data_free_buf(cdev);
	}

	/* Remove the file name from the path */
	cdev->dbg_data_path[size] = '\0';

	return rc;
}
#endif

static void qed_dbg_send_uevent(struct qed_dev *cdev, char *uevent)
{
	struct device *dev = &cdev->pdev->dev;
	char bdf[64];
	char *envp_ext[] = { bdf, NULL };
	int rc;

	snprintf(bdf, sizeof(bdf), "QED_DEBUGFS_BDF_%s=%02x:%02x.%x",
		 uevent, cdev->pdev->bus->number, PCI_SLOT(cdev->pdev->devfn),
		 PCI_FUNC(cdev->pdev->devfn));

	rc = kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp_ext);
	if (rc)
		DP_NOTICE(cdev, "Failed to send uevent %s for %s\n",
			  uevent, bdf);
}

void qed_dbg_uevent_sfp(struct qed_dev *cdev, enum qed_dbg_uevent_sfp_type type)
{
	switch (type) {
	case QED_DBG_UEVENT_SFP_UPDATE:
		qed_dbg_send_uevent(cdev, "SFP");
		break;
	case QED_DBG_UEVENT_SFP_TX_FLT:
		qed_dbg_send_uevent(cdev, "TX_FLT");
		break;
	case QED_DBG_UEVENT_SFP_RX_LOS:
		qed_dbg_send_uevent(cdev, "RX_LOS");
		break;
	default:
		DP_NOTICE(cdev, "Unknown qed_dbg_uevent_sfp_type %d\n", type);
	}
}

static int __qed_dbg_save_all_data(struct qed_dev *cdev)
{
	struct qed_hwfn *p_hwfn = &cdev->hwfns[cdev->engine_for_debug];
	u32 dbg_data_buf_size;
	u8 *p_dbg_data_buf;
	int rc;

	if (!qed_dbg_validate_reg_read(p_hwfn)) {
		DP_ERR(cdev,
		       "Register reads are invalid, cannot collect debug data\n");

		return -EPERM;
	}

	dbg_data_buf_size = qed_dbg_all_data_size(cdev);
	p_dbg_data_buf = vzalloc(dbg_data_buf_size);
	if (!p_dbg_data_buf) {
		DP_NOTICE(cdev,
			  "Failed to allocate memory for a debug data buffer\n");
		return -ENOMEM;
	}

	rc = qed_dbg_all_data(cdev, p_dbg_data_buf);
	if (rc) {
		DP_NOTICE(cdev, "Failed to obtain debug data\n");
		vfree(p_dbg_data_buf);
		return rc;
	}

	cdev->p_dbg_data_buf = p_dbg_data_buf;
	cdev->dbg_data_buf_size = dbg_data_buf_size;

	return 0;
}

void qed_dbg_save_all_data(struct qed_dev *cdev, bool print_dbg_data)
{
	bool curr_print_flag;
#ifndef QED_UPSTREAM
	int rc;
#endif
	if (!cdev) {
		pr_err("cdev is NULL cannot collect debug data\n");
		return;
	}

	curr_print_flag = cdev->print_dbg_data;

	cdev->dbg_bin_dump = 1;
	qed_dbg_all_data_free_buf(cdev);

	cdev->print_dbg_data = print_dbg_data;
#ifndef QED_UPSTREAM
	rc =
#endif
	    __qed_dbg_save_all_data(cdev);
#ifndef QED_UPSTREAM
	if (rc)
		goto out;

	/* The udev mechanism is not needed if directly saving to file */
	if (cdev->b_dump_dbg_data)
		rc = qed_dbg_save_to_file(cdev);
	else if (dbg_send_uevent)
#endif
		qed_dbg_send_uevent(cdev, "DBG");
#ifndef QED_UPSTREAM
out:
#endif
	cdev->print_dbg_data = curr_print_flag;
	cdev->dbg_bin_dump = 0;
}

int qed_dbg_feature(struct qed_dev *cdev, void *buffer,
		    enum qed_dbg_features feature, u32 * num_dumped_bytes)
{
	struct qed_ptt *p_ptt;
	enum dbg_status dbg_rc;
	int rc = 0;
	struct qed_hwfn *p_hwfn = &cdev->hwfns[cdev->engine_for_debug];
	struct qed_dbg_feature *qed_feature = &cdev->dbg_features[feature];

	/* acquire ptt */
	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return -EINVAL;

	/* get dump */
	dbg_rc = qed_dbg_dump(p_hwfn, p_ptt, feature);
	if (dbg_rc != DBG_STATUS_OK) {
		DP_VERBOSE(cdev, QED_MSG_DEBUG, "%s\n",
			   qed_dbg_get_status_str(dbg_rc));
		*num_dumped_bytes = 0;
		rc = -EINVAL;
		goto out;
	}

	DP_VERBOSE(cdev, QED_MSG_DEBUG,
		   "copying debugfs feature to external buffer\n");
	memcpy(buffer, qed_feature->dump_buf, qed_feature->buf_size);
	*num_dumped_bytes = cdev->dbg_features[feature].dumped_dwords * 4;

out:
	qed_ptt_release(p_hwfn, p_ptt);
	return rc;
}

/* function for external module to obtain feature dump size */
int qed_dbg_feature_size(struct qed_dev *cdev, enum qed_dbg_features feature)
{
	struct qed_hwfn *p_hwfn = &cdev->hwfns[cdev->engine_for_debug];
	struct qed_dbg_feature *qed_feature = &cdev->dbg_features[feature];
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	u32 buf_size_dwords;
	enum dbg_status rc;

	if (!p_ptt)
		return 0;

	rc = qed_features_lookup[feature].get_size(p_hwfn, p_ptt,
						   &buf_size_dwords);
	if (rc != DBG_STATUS_OK)
		buf_size_dwords = 0;

	/* Feature will not be dumped if it exceeds maximum size */
	if (buf_size_dwords > MAX_DBG_FEATURE_SIZE_DWORDS)
		buf_size_dwords = 0;

	qed_ptt_release(p_hwfn, p_ptt);
	qed_feature->buf_size = buf_size_dwords * sizeof(u32);
	return qed_feature->buf_size;
}

static ssize_t qed_dbg_cmd_read_inner(struct file *filp, char __user * buffer,
				      size_t count, loff_t * ppos,
				      u8 * dump_buf, int dumped_bytes)
{
	int bytes_not_copied, len;

	/*pr_info("qed_dbg_cmd_read called. qed_dumped_bytes %d, *ppos %d\n",
	   dumped_bytes, (u32)*ppos); */

	/* if there is nothing further to dump, no point to continue */
	if (*ppos == dumped_bytes)
		return 0;

	/* data availability sanity */
	if (*ppos > dumped_bytes || !dumped_bytes || !dump_buf) {
		pr_err("no data in dump buffer\n");
		return 0;
	}

	len = min_t(int, count, dumped_bytes - *ppos);

	/* copy dump data to the user */
	bytes_not_copied = copy_to_user(buffer, &dump_buf[*ppos], len);

	if (bytes_not_copied < 0) {
		pr_err("failed to copy all bytes: bytes_not_copied %d\n",
		       bytes_not_copied);
		return bytes_not_copied;
	}

	*ppos += len;
	return len;
}

/* Generic function for supplying debug data of a feature to the user */
static ssize_t qed_dbg_cmd_read(struct file *filp, char __user * buffer,
				size_t count, loff_t * ppos,
				enum qed_dbg_features feature_idx)
{
	struct qed_dbg_feature *feature;
	struct qed_dev *cdev;
	int dumped_bytes;

	cdev = (struct qed_dev *)filp->private_data;
	feature = &cdev->dbg_features[feature_idx];
	dumped_bytes = feature->dumped_dwords * sizeof(u32);

	return qed_dbg_cmd_read_inner(filp, buffer, count, ppos,
				      (void *)feature->dump_buf, dumped_bytes);
}

/* qed_dbg_cmd_write - write into cmd datum */
static ssize_t qed_dbg_cmd_write(struct file *filp, const char __user * buffer,
				 size_t count, loff_t * ppos,
				 const struct qed_func_lookup *lookup,
				 int num_funcs, bool from_user, bool is_hsi,
				 bool is_tests)
{
	struct qed_dev *cdev = (struct qed_dev *)filp->private_data;
	struct qed_hwfn *p_hwfn = &cdev->hwfns[cdev->engine_for_debug];
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	int bytes_not_copied, func_idx;
	enum dbg_status rc = 100;
	long int result = 0;
	char *cmd_buf;
	long cmd_len;

	if (!p_ptt)
		return -1;

	DP_VERBOSE(cdev, QED_MSG_DEBUG, "qed_dbg_cmd_write called: count %zu\n",
		   count);

	/* don't allow partial writes */
	if (*ppos != 0) {
		qed_ptt_release(p_hwfn, p_ptt);
		return 0;
	}

	/* prep command buffer */
	if (!from_user) {
		char __user *nl_pos = (char __user *)strchr(buffer, '\n');
		/* +1 to include the \n */
		if (nl_pos)
			cmd_len = nl_pos - buffer + 1;
		else
			cmd_len = count;
	} else {
		/* strnlen_user() includes the null terminator */
		cmd_len = strnlen_user(buffer, MAX_ARG_STRLEN) - 1;
	}

	cmd_buf = kzalloc(cmd_len + 1, GFP_KERNEL);
	if (!cmd_buf) {
		qed_ptt_release(p_hwfn, p_ptt);
		return count;
	}

	if (!from_user) {
		memcpy(cmd_buf, buffer, cmd_len);
	} else {
		char *nl_pos;

		/* copy user data to command buffer and perform sanity */
		bytes_not_copied = copy_from_user(cmd_buf, buffer, cmd_len);
		if (bytes_not_copied < 0) {
			kfree(cmd_buf);
			qed_ptt_release(p_hwfn, p_ptt);
			return bytes_not_copied;
		}
		if (bytes_not_copied > 0)
			return count;

		/* Fix cmd_len to be the size of the string till the first
		 * occurrence of \n (inclusively), since a multiple-line input
		 * should be processed line by line.
		 */
		nl_pos = strchr(cmd_buf, '\n');
		if (nl_pos) {
			cmd_len = nl_pos - cmd_buf + 1;
			/* Replace the closing \n character with a null
			 *  terminator
			 */
			cmd_buf[cmd_len - 1] = '\0';
		}
	}

	/* scan lookup table keys for a match to command buffer first arg */
	for (func_idx = 0; func_idx < num_funcs; func_idx++) {
		int keylen = strlen(lookup[func_idx].key);

		/* use strncmp rather than strcmp since we only want to find a
		 * matching prefix, not the entire string.
		 */
		if (strncmp(lookup[func_idx].key, cmd_buf, keylen) == 0) {
			DP_VERBOSE(cdev, QED_MSG_DEBUG,
				   "debugfs cmd %s being executed\n", cmd_buf);
			rc = (lookup[func_idx].str_func) (p_hwfn, p_ptt,
							  cmd_buf + keylen);
			break;
		}
	}

	/* if command string was not found in the lookup table then log error and do nothing */
	if (func_idx == num_funcs)
		DP_NOTICE(cdev, "unknown command: %s\n", cmd_buf);
	/* for hsi function, test result code and print status if error */
	if (is_hsi && rc != DBG_STATUS_OK)
		DP_NOTICE(cdev, "hsi func returned status %s\n",
			  qed_dbg_get_status_str(rc));

	/* for test function, store result */
	if (!is_hsi) {
		memset(cdev->test_result, 0, sizeof(cdev->test_result));
		result = rc;
		snprintf(cdev->test_result,
			 QED_TEST_RESULT_LENGTH, "%ld\n", result);
		cdev->test_result_available = true;
	}

	kfree(cmd_buf);
	cmd_buf = NULL;
	qed_ptt_release(p_hwfn, p_ptt);
	return cmd_len;
}

#ifdef _HAS_SYSFS_BIN_ATTR_INIT
/* qed_sysfs_cmd_write - write into cmd datum */
static ssize_t qed_sysfs_cmd_write(const char __user * buffer,
				   size_t count, loff_t * ppos,
				   const struct qed_func_lookup *lookup,
				   int num_funcs, bool from_user, bool is_hsi,
				   bool is_tests,
				   struct bin_attribute *bin_attr)
{
	enum dbg_status rc = 100;
	struct qed_hwfn *p_hwfn;
	struct qed_ptt *p_ptt;
	struct qed_dev *cdev;
	long int result = 0;
	char *cmd_buf;
	int func_idx;
	long cmd_len;

	cdev = (struct qed_dev *)bin_attr->private;
	p_hwfn = &cdev->hwfns[cdev->engine_for_debug];
	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return 0;

	DP_VERBOSE(cdev, QED_MSG_DEBUG, "qed_sysfs_cmd_write count %zu\n",
		   count);
	cmd_len = strnlen(buffer, MAX_ARG_STRLEN);
	cmd_buf = kzalloc(cmd_len, GFP_KERNEL);
	if (!cmd_buf) {
		qed_ptt_release(p_hwfn, p_ptt);
		return count;
	}
	memcpy(cmd_buf, buffer, cmd_len);
	/* Replace the closing \n character with a null terminator */
	cmd_buf[cmd_len - 1] = '\0';
	/* scan lookup table keys for a match to command buffer first arg */
	for (func_idx = 0; func_idx < num_funcs; func_idx++) {
		int keylen = strlen(lookup[func_idx].key);

		/* use strncmp rather than strcmp since we only want to find a
		 * matching prefix, not the entire string.
		 */
		if (strncmp(lookup[func_idx].key, cmd_buf, keylen) == 0) {
			pr_debug("matched %s to cmd_buf %s\n",
				 lookup[func_idx].key, cmd_buf);
			rc = (lookup[func_idx].str_func) (p_hwfn, p_ptt,
							  cmd_buf + keylen);
			break;
		}
	}

	/* if command string was not found in the lookup table then log
	 * error and do nothing
	 */
	if (func_idx == num_funcs)
		DP_NOTICE(cdev, "unknown command: %s\n", cmd_buf);
	/* for hsi function, test result code and print status if error */
	if (is_hsi && rc != DBG_STATUS_OK)
		DP_NOTICE(cdev, "hsi func returned status %s\n",
			  qed_dbg_get_status_str(rc));
	/* for test function, store result */
	if (!is_hsi) {
		memset(cdev->test_result, 0, sizeof(cdev->test_result));
		result = rc;
		snprintf(cdev->test_result, QED_TEST_RESULT_LENGTH, "%ld\n",
			 result);
		cdev->test_result_available = true;
	}

	kfree(cmd_buf);
	cmd_buf = NULL;
	qed_ptt_release(p_hwfn, p_ptt);
	return cmd_len;
}
#endif

/********************** file operations section *******************************/
/* read file op for the bus feature */
static ssize_t qed_dbg_bus_cmd_read(struct file *filp, char __user * buffer,
				    size_t count, loff_t * ppos)
{
	return qed_dbg_cmd_read(filp, buffer, count, ppos, DBG_FEATURE_BUS);
}

/* write file op for the bus feature */
static ssize_t qed_dbg_bus_cmd_write(struct file *filp,
				     const char __user * buffer, size_t count,
				     loff_t * ppos)
{
	return qed_dbg_cmd_write(filp, buffer, count, ppos,
				 qed_features_lookup[DBG_FEATURE_BUS].
				 hsi_func_lookup,
				 qed_features_lookup[DBG_FEATURE_BUS].num_funcs,
				 true /* from user */ ,
				 true /* hsi function */ ,
				 false /* not tests function */ );
}

/* read file op for the grc feature */
static ssize_t qed_dbg_grc_cmd_read(struct file *filp, char __user * buffer,
				    size_t count, loff_t * ppos)
{
	return qed_dbg_cmd_read(filp, buffer, count, ppos, DBG_FEATURE_GRC);
}

/* write file op for the grc feature */
static ssize_t qed_dbg_grc_cmd_write(struct file *filp,
				     const char __user * buffer, size_t count,
				     loff_t * ppos)
{
	return qed_dbg_cmd_write(filp, buffer, count, ppos,
				 qed_features_lookup[DBG_FEATURE_GRC].
				 hsi_func_lookup,
				 qed_features_lookup[DBG_FEATURE_GRC].num_funcs,
				 true /* from user */ ,
				 true /* hsi function */ ,
				 false /* not tests function */ );
}

/* read file op for the idle_chk feature */
static ssize_t qed_dbg_idle_chk_cmd_read(struct file *filp,
					 char __user * buffer, size_t count,
					 loff_t * ppos)
{
	return qed_dbg_cmd_read(filp, buffer, count, ppos,
				DBG_FEATURE_IDLE_CHK);
}

/* write file op for the idle_chk feature */
static ssize_t qed_dbg_idle_chk_cmd_write(struct file *filp,
					  const char __user * buffer,
					  size_t count, loff_t * ppos)
{
	return qed_dbg_cmd_write(filp, buffer, count, ppos,
				 qed_features_lookup[DBG_FEATURE_IDLE_CHK].
				 hsi_func_lookup,
				 qed_features_lookup[DBG_FEATURE_IDLE_CHK].
				 num_funcs, true /* from user */ ,
				 true /* hsi function */ ,
				 false /* not tests function */ );
}

/* read file op for the mcp_trace feature */
static ssize_t qed_dbg_mcp_trace_cmd_read(struct file *filp,
					  char __user * buffer, size_t count,
					  loff_t * ppos)
{
	return qed_dbg_cmd_read(filp, buffer, count, ppos,
				DBG_FEATURE_MCP_TRACE);
}

/* write file op for the mcp_trace feature */
static ssize_t qed_dbg_mcp_trace_cmd_write(struct file *filp,
					   const char __user * buffer,
					   size_t count, loff_t * ppos)
{
	return qed_dbg_cmd_write(filp, buffer, count, ppos,
				 qed_features_lookup[DBG_FEATURE_MCP_TRACE].
				 hsi_func_lookup,
				 qed_features_lookup[DBG_FEATURE_MCP_TRACE].
				 num_funcs, true /* from user */ ,
				 true /* hsi function */ ,
				 false /* not tests function */ );
}

/* read file op for the reg_fifo feature */
static ssize_t qed_dbg_reg_fifo_cmd_read(struct file *filp,
					 char __user * buffer, size_t count,
					 loff_t * ppos)
{
	return qed_dbg_cmd_read(filp, buffer, count, ppos,
				DBG_FEATURE_REG_FIFO);
}

/* write file op for the reg_fifo feature */
static ssize_t qed_dbg_reg_fifo_cmd_write(struct file *filp,
					  const char __user * buffer,
					  size_t count, loff_t * ppos)
{
	return qed_dbg_cmd_write(filp, buffer, count, ppos,
				 qed_features_lookup[DBG_FEATURE_REG_FIFO].
				 hsi_func_lookup,
				 qed_features_lookup[DBG_FEATURE_REG_FIFO].
				 num_funcs, true /* from user */ ,
				 true /* hsi function */ ,
				 false /* not tests function */ );
}

/* read file op for the igu_fifo feature */
static ssize_t qed_dbg_igu_fifo_cmd_read(struct file *filp,
					 char __user * buffer, size_t count,
					 loff_t * ppos)
{
	return qed_dbg_cmd_read(filp, buffer, count, ppos,
				DBG_FEATURE_IGU_FIFO);
}

/* write file op for the igu_fifo feature */
static ssize_t qed_dbg_igu_fifo_cmd_write(struct file *filp,
					  const char __user * buffer,
					  size_t count, loff_t * ppos)
{
	return qed_dbg_cmd_write(filp, buffer, count, ppos,
				 qed_features_lookup[DBG_FEATURE_IGU_FIFO].
				 hsi_func_lookup,
				 qed_features_lookup[DBG_FEATURE_IGU_FIFO].
				 num_funcs, true /* from user */ ,
				 true /* hsi function */ ,
				 false /* not tests function */ );
}

/* read file op for the protection_override feature */
static ssize_t qed_dbg_protection_override_cmd_read(struct file *filp,
						    char __user * buffer,
						    size_t count, loff_t * ppos)
{
	return qed_dbg_cmd_read(filp, buffer, count, ppos,
				DBG_FEATURE_PROTECTION_OVERRIDE);
}

/* write file op for the protection_override feature */
static ssize_t qed_dbg_protection_override_cmd_write(struct file *filp,
						     const char __user * buffer,
						     size_t count,
						     loff_t * ppos)
{
	return qed_dbg_cmd_write(filp, buffer, count, ppos,
				 qed_features_lookup
				 [DBG_FEATURE_PROTECTION_OVERRIDE].
				 hsi_func_lookup,
				 qed_features_lookup
				 [DBG_FEATURE_PROTECTION_OVERRIDE].num_funcs,
				 true /* from user */ ,
				 true /* hsi function */ ,
				 false /* not tests function */ );
}

/* read file op for the fw_asserts feature */
static ssize_t qed_dbg_fw_asserts_cmd_read(struct file *filp,
					   char __user * buffer, size_t count,
					   loff_t * ppos)
{
	return qed_dbg_cmd_read(filp, buffer, count, ppos,
				DBG_FEATURE_FW_ASSERTS);
}

/* write file op for the fw_asserts feature */
static ssize_t qed_dbg_fw_asserts_cmd_write(struct file *filp,
					    const char __user * buffer,
					    size_t count, loff_t * ppos)
{
	return qed_dbg_cmd_write(filp, buffer, count, ppos,
				 qed_features_lookup[DBG_FEATURE_FW_ASSERTS].
				 hsi_func_lookup,
				 qed_features_lookup[DBG_FEATURE_FW_ASSERTS].
				 num_funcs, true /* from user */ ,
				 true /* hsi function */ ,
				 false /* not tests function */ );
}

/* read file op for the ilt feature */
static ssize_t qed_dbg_ilt_cmd_read(struct file *filp, char __user * buffer,
				    size_t count, loff_t * ppos)
{
	return qed_dbg_cmd_read(filp, buffer, count, ppos, DBG_FEATURE_ILT);
}

/* write file op for the ilt feature */
static ssize_t qed_dbg_ilt_cmd_write(struct file *filp,
				     const char __user * buffer, size_t count,
				     loff_t * ppos)
{
	return qed_dbg_cmd_write(filp, buffer, count, ppos,
				 qed_features_lookup[DBG_FEATURE_ILT].
				 hsi_func_lookup,
				 qed_features_lookup[DBG_FEATURE_ILT].num_funcs,
				 true /* from user */ ,
				 true /* hsi function */ ,
				 false /* not tests function */ );
}

/* read file op for the internal_trace feature */
static ssize_t qed_dbg_internal_trace_cmd_read(struct file *filp,
					       char __user * buffer,
					       size_t count, loff_t * ppos)
{
	return qed_dbg_cmd_read(filp, buffer, count, ppos,
				DBG_FEATURE_INTERNAL_TRACE);
}

/* write file op for the internal_trace feature */
static ssize_t qed_dbg_internal_trace_cmd_write(struct file *filp,
						const char __user * buffer,
						size_t count, loff_t * ppos)
{
	return qed_dbg_cmd_write(filp, buffer, count, ppos,
				 qed_features_lookup
				 [DBG_FEATURE_INTERNAL_TRACE].hsi_func_lookup,
				 qed_features_lookup
				 [DBG_FEATURE_INTERNAL_TRACE].num_funcs,
				 true /* from user */ ,
				 true /* hsi function */ ,
				 false /* not tests function */ );
}

/* read file op for the linkdump_phydump feature */
static ssize_t qed_dbg_linkdump_phydump_cmd_read(struct file *filp,
						 char __user * buffer,
						 size_t count, loff_t * ppos)
{
	return qed_dbg_cmd_read(filp, buffer, count, ppos,
				DBG_FEATURE_LINKDUMP_PHYDUMP);
}

/* write file op for the linkdump_phydump feature */
static ssize_t qed_dbg_linkdump_phydump_cmd_write(struct file *filp,
						  const char __user * buffer,
						  size_t count, loff_t * ppos)
{
	return qed_dbg_cmd_write(filp, buffer, count, ppos,
				 qed_features_lookup
				 [DBG_FEATURE_LINKDUMP_PHYDUMP].hsi_func_lookup,
				 qed_features_lookup
				 [DBG_FEATURE_LINKDUMP_PHYDUMP].num_funcs,
				 true /* from user */ ,
				 true /* hsi function */ ,
				 false /* not tests function */ );
}

/*** prepare file operations structures for debug features ***/
#define qed_debugfs_fileops(feature) \
	{ \
		.owner  = THIS_MODULE, \
		.open   = simple_open, \
		.read   = qed_dbg_##feature##_cmd_read, \
		.write  = qed_dbg_##feature##_cmd_write \
	}

static const struct file_operations qed_feature_fops[DBG_FEATURE_NUM] = {
	qed_debugfs_fileops(bus),
	qed_debugfs_fileops(grc),
	qed_debugfs_fileops(idle_chk),
	qed_debugfs_fileops(mcp_trace),
	qed_debugfs_fileops(reg_fifo),
	qed_debugfs_fileops(igu_fifo),
	qed_debugfs_fileops(protection_override),
	qed_debugfs_fileops(fw_asserts),
	qed_debugfs_fileops(ilt),
	qed_debugfs_fileops(internal_trace),
	qed_debugfs_fileops(linkdump_phydump),
};

/********************** end of file opreation section *************************/

/* list element that contains the preconfig commands for the debug bus */
struct preconfig_struct {
	struct list_head list_entry;
	char *buffer;
	size_t count;
	loff_t *ppos;
};

/* pool of the preconfig struct that holds the preconfig list */
static struct preconfig_struct_pool {
	struct list_head preconfig_list;
} preconfig_pool;

static u8 preconfig_engine;

u8 qed_get_debug_engine(struct qed_dev *cdev)
{
	return cdev->engine_for_debug;
}

void qed_set_debug_engine(struct qed_dev *cdev, int engine_number)
{
	DP_VERBOSE(cdev, QED_MSG_DEBUG, "set debug engine to %d\n",
		   engine_number);
	cdev->engine_for_debug = engine_number;
}

/* frees preconfig list */
static void free_preconfig(void)
{
	struct preconfig_struct tmp_preconfig;
	struct preconfig_struct *p_preconfig;
	struct preconfig_struct *tmp;

	p_preconfig = &tmp_preconfig;

	/* free all list elements in preconfig pool */
	list_for_each_entry_safe(p_preconfig, tmp,
				 &preconfig_pool.preconfig_list, list_entry) {
		kfree(p_preconfig->buffer);
		list_del(&p_preconfig->list_entry);
		kfree(p_preconfig);
	}
}

/* free post config info */
static void free_postconfig(void)
{
	kfree(postconfig_buf);
	postconfig_bytes = 0;
}

/* allocate new preconfig list element
 * copy __user buffer into the buffer in the new element
 */
static ssize_t qed_dbg_preconfig_cmd_write(struct file *filp,
					   const char __user * buffer,
					   size_t count, loff_t * ppos)
{
	/* prepare list element */
	struct preconfig_struct *p_preconfig = kmalloc(sizeof(*p_preconfig),
						       GFP_KERNEL);
	int bytes_not_copied;
	u8 preconfig_len;
	char *nl_pos;

	if (!p_preconfig) {
		pr_notice("allocating p_preconfig failed\n");
		goto err0;
	}

	/* strnlen_user() includes the null terminator */
	preconfig_len = strnlen_user(buffer, MAX_ARG_STRLEN) - 1;

	/* allocate buffer for command string */
	p_preconfig->buffer = kzalloc(preconfig_len + 1, GFP_KERNEL);
	if (!p_preconfig->buffer) {
		pr_notice("allocating p_preconfig->buffer failed\n");
		goto err1;
	}

	/* populate list element */
	bytes_not_copied = copy_from_user(p_preconfig->buffer, buffer,
					  preconfig_len);
	if (bytes_not_copied != 0) {
		pr_notice("copy from user failed\n");
		goto err2;
	}

	/* Fix preconfig_len to be the size of the string till the first
	 * occurrence of \n (inclusively), since a multiple-line input should be
	 * processed line by line.
	 */
	nl_pos = strchr(p_preconfig->buffer, '\n');
	if (nl_pos)
		preconfig_len = nl_pos - p_preconfig->buffer + 1;

	p_preconfig->count = preconfig_len;
	p_preconfig->ppos = ppos;

	/* add element to list */
	list_add_tail(&p_preconfig->list_entry, &preconfig_pool.preconfig_list);
	return preconfig_len;

err2:
	kfree(p_preconfig->buffer);
err1:
	kfree(p_preconfig);
err0:
	pr_notice
	    ("due to the error that occurred debugbus preconfig will not work\n");
	return count;
}

/* prints the buffer from preconfig list */
static ssize_t qed_dbg_preconfig_cmd_read(struct file *filp,
					  char __user * buffer, size_t count,
					  loff_t * ppos)
{
	struct preconfig_struct tmp_preconfig;
	struct preconfig_struct *p_preconfig;
	struct preconfig_struct *tmp;
	int traversed_bytes = 0;
	int bytes_not_copied;
	int bytes_copied;
	size_t len = 0;

	p_preconfig = &tmp_preconfig;

	/* traverses the preconfig list elements and prints the buffers */
	list_for_each_entry_safe(p_preconfig, tmp,
				 &(preconfig_pool.preconfig_list), list_entry) {

		/* does this element contain new bytes? */
		if (traversed_bytes + p_preconfig->count > *ppos) {
			int local_ppos = *ppos - traversed_bytes;

			len = p_preconfig->count - local_ppos;
			bytes_not_copied =
			    copy_to_user(buffer,
					 &p_preconfig->buffer[local_ppos], len);
			if (bytes_not_copied)
				pr_notice
				    ("failed to copy all bytes, bytes not copied: %d\n",
				     bytes_not_copied);
			bytes_copied = len - bytes_not_copied;
			*ppos += bytes_copied;
			return bytes_copied;
		}

		traversed_bytes += p_preconfig->count;
	}

	return 0;
}

static ssize_t qed_dbg_preconfig_engine_cmd_write(struct file *filp,
						  const char __user * buffer,
						  size_t count, loff_t * ppos)
{
	int bytes_not_copied;
	char *cmd_buf;
	u8 cmd_len, tmp;
	char *nl_pos;

	/* don't allow partial writes */
	if (*ppos != 0)
		return 0;

	/* strnlen_user() includes the null terminator */
	cmd_len = strnlen_user(buffer, MAX_ARG_STRLEN) - 1;

	/* allocate buffer for command string */
	cmd_buf = kzalloc(cmd_len + 1, GFP_KERNEL);
	if (!cmd_buf)
		return count;

	/* copy user data to command buffer and perform sanity */
	bytes_not_copied = copy_from_user(cmd_buf, buffer, cmd_len);
	if (bytes_not_copied < 0) {
		kfree(cmd_buf);
		return bytes_not_copied;
	}
	if (bytes_not_copied > 0) {
		kfree(cmd_buf);
		return count;
	}

	/* Fix cmd_len to be the size of the string till the first occurrence of
	 * \n (inclusively).
	 */
	nl_pos = strchr(cmd_buf, '\n');
	if (nl_pos)
		cmd_len = nl_pos - cmd_buf + 1;

	/* Replace the closing \n character with a null terminator */
	cmd_buf[cmd_len - 1] = '\0';

	/* scan user input into preconfig engine */
	sscanf(cmd_buf, "%hhi", &tmp);

	/* sanitize */
	if (tmp > 1)
		pr_err("illegal value for preconfig engine %d\n", tmp);
	else
		preconfig_engine = tmp;

	kfree(cmd_buf);
	cmd_buf = NULL;
	return cmd_len;
}

static ssize_t qed_dbg_char_cmd_read(char __user * buffer, size_t count,
				     loff_t * ppos, char *source)
{
	int bytes_not_copied, len;
	char data[3];

	snprintf(data, sizeof(char) + 2, "%hhd\n", *source);	/* +2 for /n and /0 */
	len = sizeof(data);

	/* avoid reading beyond available data */
	len = min_t(int, count, len - *ppos);

	/* copy data to the user */
	bytes_not_copied = copy_to_user(buffer, data + *ppos, len);

	/* notify user of problems */
	if (bytes_not_copied < 0) {
		pr_err("failed to copy all bytes: bytes_not_copied %d\n",
		       bytes_not_copied);
		return bytes_not_copied;
	}

	*ppos += len;
	return len;
}

static ssize_t qed_dbg_preconfig_engine_cmd_read(struct file *filp,
						 char __user * buffer,
						 size_t count, loff_t * ppos)
{
	return qed_dbg_char_cmd_read(buffer, count, ppos, &preconfig_engine);
}

/* curently no functionality for writing into the postconfig node */
static ssize_t qed_dbg_postconfig_cmd_write(struct file *filp,
					    const char __user * buffer,
					    size_t count, loff_t * ppos)
{
	return count;
}

static ssize_t qed_dbg_postconfig_cmd_read(struct file *filp,
					   char __user * buffer, size_t count,
					   loff_t * ppos)
{
	return qed_dbg_cmd_read_inner(filp, buffer, count, ppos, postconfig_buf,
				      postconfig_bytes);
}

static void qed_chain_info_init(struct qed_dev *cdev, int start_index,
				bool print_metadata)
{
	cdev->chain_info.current_index = start_index;
	cdev->chain_info.b_key_entered = true;
	cdev->chain_info.print_metadata = print_metadata;
	cdev->chain_info.final_index = cdev->chain_info.chain->capacity;
}

static ssize_t qed_dbg_chain_print_cmd_write(struct file *filp,
					     const char __user * buffer,
					     size_t count, loff_t * ppos)
{
	struct qed_dev *cdev = (struct qed_dev *)filp->private_data;
	struct qed_hwfn *p_hwfn = &cdev->hwfns[cdev->engine_for_debug];
	char *buf;
	int len;

	buf = kmalloc(count + 1, GFP_ATOMIC);	/* +1 for '\0' */
	if (!buf) {
		pr_notice("allocating buffer failed\n");
		goto error;
	}

	len = simple_write_to_buffer(buf, count, ppos, buffer, count);
	if (len < 0) {
		pr_notice("copy from user failed\n");
		goto error;
	}

	buf[len] = '\0';

	if (!strncmp(buf, "eq", 2)) {
		cdev->chain_info.chain = &p_hwfn->p_eq->chain;
		qed_chain_info_init(cdev, 0, true);
	}

	if (!strncmp(buf, "spq", 3)) {
		cdev->chain_info.chain = &p_hwfn->p_spq->chain;
		qed_chain_info_init(cdev, 0, true);
	}

	kfree(buf);
	return len;

error:
	kfree(buf);
	pr_notice("due to the error that occurred chain print will not work\n");
	return count;
}

/* EXAMPLE FOR USING YOUR OWN ELEMENT AND METADATA PRINT FUNCTIONS */
static int qed_chain_print_element(struct qed_chain *p_chain, void *p_element,
				   char *buffer)
{
	/* this will be a service function for the per chain print element function */
	int pos = 0, length, elem_size = p_chain->elem_size;

	/* print element byte by byte */
	while (elem_size > 0) {
		length = sprintf(buffer + pos, " %02x", *(u8 *) p_element);
		if (length < 0) {
			pr_notice("Failed to copy data to buffer\n");
			return length;
		}
		pos += length;
		elem_size--;
		p_element++;
	}

	length = sprintf(buffer + pos, "\n");
	if (length < 0) {
		pr_notice("Failed to copy data to buffer\n");
		return length;
	}

	pos += length;

	return pos;
}

static int qed_chain_print_metadata(struct qed_chain *p_chain, char *buffer)
{
	int pos = 0, length;

	length = sprintf(buffer, "prod 0x%x [%03d], cons 0x%x [%03d]\n",
			 qed_chain_get_prod_idx(p_chain),
			 qed_chain_get_prod_idx(p_chain) & 0xff,
			 qed_chain_get_cons_idx(p_chain),
			 qed_chain_get_cons_idx(p_chain) & 0xff);
	if (length < 0) {
		pr_notice("Failed to copy Metadata to buffer\n");
		return length;
	}

	pos += length;
	length = sprintf(buffer + pos, "Chain capacity: %d, Chain size: %d\n",
			 p_chain->capacity, p_chain->size);
	if (length < 0) {
		pr_notice("Failed to copy Metadata to buffer\n");
		return length;
	}

	pos += length;

	return pos;
}

/* END OF EXAMPLE */

#define CHAIN_PRINT_DONE 200

static ssize_t qed_dbg_chain_print_cmd_read(struct file *filp,
					    char __user * buffer,
					    size_t count, loff_t * ppos)
{
	struct qed_dev *cdev = (struct qed_dev *)filp->private_data;
	int len = 0, rc = 0;
	char *buf;
	char key_list[] = "spq\neq\n";

	/* check if the user entered a key, if not print the list of the keys */
	if (!cdev->chain_info.b_key_entered) {
		len = simple_read_from_buffer(buffer, count, ppos, key_list,
					      strlen(key_list));
	} else {
		buf = kmalloc(sizeof(char) * count, GFP_KERNEL);
		if (!buf) {
			pr_notice("allocating buffer failed\n");
			return 0;
		}

		/* example for calling chain print with function pointers */
		rc = qed_chain_print(cdev->chain_info.chain, buf, count,
				     &cdev->chain_info.current_index,
				     cdev->chain_info.final_index,
				     cdev->chain_info.print_metadata, false,
				     qed_chain_print_element,
				     qed_chain_print_metadata);

		/* example for calling chain print without function pointers */

		/*rc = qed_chain_print(cdev->chain_info.chain, buf, count,
		   &cdev->chain_info.current_index, cdev->chain_info.final_index,
		   cdev->chain_info.print_metadata, NULL, NULL);
		 */
		if (rc < 0) {
			pr_notice("printing chain to buffer failed\n");
			return 0;
		}

		len = simple_read_from_buffer(buffer, count, ppos, buf,
					      strlen(buf));
		cdev->chain_info.print_metadata = false;
		kfree(buf);
		if (rc == CHAIN_PRINT_DONE)
			*ppos -= len;
		else
			cdev->chain_info.b_key_entered = false;
	}

	return len;
}

/* copies the buffer from preconfig list to debug bus
 * occurs only if preconfig list exists
 * the function is called after device is brought out of reset
 */
void qed_copy_preconfig_to_bus(struct qed_dev *cdev, u8 init_engine)
{
	struct preconfig_struct tmp_preconfig;
	struct preconfig_struct *p_preconfig;
	struct preconfig_struct *tmp;
	struct file filp;
	loff_t temp = 0;

	/* TODO add support in secureboot */
	if (_efi_enabled)
		return;

	/* copy to debug bus only if the engine being initialized matches the one indicated
	 * in the preconfig command
	 */
	if (init_engine != preconfig_engine)
		return;
	else
		qed_set_debug_engine(cdev, preconfig_engine);

	filp.private_data = (void *)cdev;
	p_preconfig = &tmp_preconfig;
	if (!list_empty(&preconfig_pool.preconfig_list)) {
		/* traverses the preconfig list elements and the buffer to qed_dbg_cmd_write */
		list_for_each_entry_safe(p_preconfig, tmp,
					 &(preconfig_pool.preconfig_list),
					 list_entry) {
			qed_dbg_cmd_write(&filp, p_preconfig->buffer,
					  p_preconfig->count, &temp,
					  qed_bus_hsi_func_lookup,
					  BUS_NUM_STR_FUNCS,
					  false /*not from user */ ,
					  false /*not an dbg hsi function */ ,
					  false /*not an tests function */ );
		}
		free_preconfig();
	}
}

/* postconfig buf is used to copy the data from a running debug bus recording
 * when driver is being removed, or probe flow fails, since the data will be
 * unattainable otherwise.
 */
int qed_copy_bus_to_postconfig(struct qed_dev *cdev, u8 down_engine)
{
	int rc;

	/* TODO:add support in secureboot */
	if (_efi_enabled)
		return 0;

	/* check if there is an active recording in this PF */
	if (!cdev->recording_active)
		return 0;

	/* check that the recording is for this engine */
	if (down_engine != qed_get_debug_engine(cdev))
		return 0;

	/* allocate the postconfig buffer acording to active recording size */
	postconfig_bytes = qed_dbg_feature_size(cdev, DBG_FEATURE_BUS);
	if (postconfig_bytes <= 0)
		return postconfig_bytes;

	postconfig_buf = kzalloc(postconfig_bytes, GFP_KERNEL);
	if (!postconfig_buf) {
		DP_ERR(cdev, "postconfig buf allocation failed\n");
		postconfig_bytes = 0;
		return -ENOMEM;
	}

	/* copy recorded data to postconfig buffer */
	rc = qed_dbg_feature(cdev, postconfig_buf, DBG_FEATURE_BUS,
			     &postconfig_bytes);
	cdev->recording_active = false;

	return rc;
}

#ifndef QED_UPSTREAM		/* ! QED_UPSTREAM */
static ssize_t qed_dbg_tests_cmd_write(struct file *filp,
				       const char __user * buffer, size_t count,
				       loff_t * ppos)
{
	struct qed_dev *cdev = (struct qed_dev *)filp->private_data;

	if (cdev->b_reuse_dev) {
		DP_ERR(cdev, "cdev is being re-configured\n");
		return 0;
	}

	return qed_dbg_cmd_write(filp, buffer, count, ppos,
				 qed_tests_func_lookup, TESTS_NUM_STR_FUNCS,
				 true /* from user */ ,
				 false /* not an dbg hsi function */ ,
				 true /* tests function */ );
}
#endif

/* function services tests and phy read command */
static ssize_t qed_dbg_external_cmd_read(struct file *filp,
					 char __user * buffer, size_t count,
					 loff_t * ppos, char *data, int len)
{
	struct qed_dev *cdev = (struct qed_dev *)filp->private_data;
	int bytes_not_copied;

	/* avoid reading beyond available data */
	len = min_t(int, count, len - *ppos);

	/* copy data to the user */
	bytes_not_copied = copy_to_user(buffer, data + *ppos, len);

	/* notify user of problems */
	if (bytes_not_copied < 0) {
		DP_NOTICE(cdev,
			  "failed to copy all bytes: bytes_not_copied %d\n",
			  bytes_not_copied);
		return bytes_not_copied;
	}

	/* mark result as unavailable */
	if (len == 0)
		cdev->test_result_available = false;

	*ppos += len;
	return len;
}

#ifndef QED_UPSTREAM		/* ! QED_UPSTREAM */
static ssize_t qed_dbg_tests_cmd_read(struct file *filp, char __user * buffer,
				      size_t count, loff_t * ppos)
{
	struct qed_dev *cdev = (struct qed_dev *)filp->private_data;
	int len, return_len;
	char *data;

	/* if test result is available return it. Otherwise print out available tests */
	if (cdev->test_result_available) {
		data = (char *)&cdev->test_result;
		len = sizeof(cdev->test_result);
	} else {
		data = (char *)tests_list;
		len = strlen(tests_list);
	}

	return_len =
	    qed_dbg_external_cmd_read(filp, buffer, count, ppos, data, len);
	return return_len;
}
#endif

static ssize_t qed_dbg_phy_cmd_write(struct file *filp,
				     const char __user * buffer, size_t count,
				     loff_t * ppos)
{
	if (p_phy_result_buf == NULL)
		p_phy_result_buf =
		    kzalloc(sizeof(char) * MAX_PHY_RESULT_BUFFER, GFP_KERNEL);

	return qed_dbg_cmd_write(filp, buffer, count, ppos, qed_phy_func_lookup,
				 PHY_NUM_STR_FUNCS, true /* from user */ ,
				 false /* not a dbg hsi function */ ,
				 false /* not a tests function */ );
	return 0;
}

/* prints the buffer from phy list */
static ssize_t qed_dbg_phy_cmd_read(struct file *filp,
				    char __user * buffer, size_t count,
				    loff_t * ppos)
{
	int len, return_len;
	char *data;

	/* if test result is available return it. Otherwise print out available phy commands */
	if (p_phy_result_buf != NULL) {
		data = p_phy_result_buf;
		len = strlen(p_phy_result_buf);
	} else {
		data = (char *)phy_list;
		len = strlen(phy_list);
	}

	return_len =
	    qed_dbg_external_cmd_read(filp, buffer, count, ppos, data, len);

	if (return_len == 0) {
		kfree(p_phy_result_buf);
		p_phy_result_buf = NULL;
	}

	return return_len;
}

static ssize_t qed_dbg_engine_cmd_write(struct file *filp,
					const char __user * buffer,
					size_t count, loff_t * ppos)
{
	return qed_dbg_cmd_write(filp, buffer, count, ppos,
				 qed_engine_func_lookup, ENGINE_NUM_STR_FUNCS,
				 true /* from user */ ,
				 true /* dbg hsi function */ ,
				 false /* not a tests function */ );
}

static ssize_t qed_dbg_engine_cmd_read(struct file *filp, char __user * buffer,
				       size_t count, loff_t * ppos)
{
	struct qed_dev *cdev = (struct qed_dev *)filp->private_data;

	return qed_dbg_char_cmd_read(buffer, count, ppos,
				     &cdev->engine_for_debug);
}

static void qed_dbg_mdump_free_buf(struct qed_dev *cdev)
{
	kfree(cdev->dbg_mdump.buf);
	cdev->dbg_mdump.buf = NULL;
	cdev->dbg_mdump.buf_size = 0;
}

#define QED_DBG_MDUMP_STATUS_LENGTH	32

static int qed_dbg_mdump_get_status(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	struct qed_mdump_info mdump_info;
	int rc;

	qed_dbg_mdump_free_buf(cdev);

	cdev->dbg_mdump.buf = kzalloc(QED_DBG_MDUMP_STATUS_LENGTH, GFP_KERNEL);
	if (!cdev->dbg_mdump.buf) {
		DP_NOTICE(cdev, "Failed to allocate buffer\n");
		return -ENOMEM;
	}

	rc = qed_mcp_mdump_get_info(p_hwfn, p_ptt, &mdump_info);
	if (rc) {
		qed_dbg_mdump_free_buf(cdev);
		return rc;
	}

	cdev->dbg_mdump.buf_size = QED_DBG_MDUMP_STATUS_LENGTH;

	snprintf(cdev->dbg_mdump.buf, QED_DBG_MDUMP_STATUS_LENGTH,
		 "num_of_logs: %d\n", mdump_info.num_of_logs);

	return 0;
}

static int qed_dbg_mdump_get_image(struct qed_hwfn *p_hwfn)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	struct qed_nvm_image_att image_att;
	u32 len_rounded, i;
	__be32 val;
	int rc;

	qed_dbg_mdump_free_buf(cdev);

	rc = qed_mcp_get_nvm_image_att(p_hwfn, QED_NVM_IMAGE_MDUMP, &image_att);
	if (rc)
		return rc;

	/* Rounding the image length up to sizeof(u32) saves the need to handle
	 * residues when we call later to cpu_to_be32().
	 */
	len_rounded = roundup(image_att.length, sizeof(u32));

	cdev->dbg_mdump.buf = kzalloc(len_rounded, GFP_KERNEL);
	if (!cdev->dbg_mdump.buf) {
		DP_NOTICE(cdev, "Failed to allocate buffer\n");
		return -ENOMEM;
	}

	rc = qed_mcp_get_nvm_image(p_hwfn, QED_NVM_IMAGE_MDUMP,
				   cdev->dbg_mdump.buf, len_rounded);
	if (rc) {
		qed_dbg_mdump_free_buf(cdev);
		return rc;
	}

	/* The parsing tool expects the image to be in a big-endian foramt */
	for (i = 0; i < len_rounded; i += 4) {
		val = cpu_to_be32(*(u32 *) & cdev->dbg_mdump.buf[i]);
		*(u32 *) & cdev->dbg_mdump.buf[i] = val;
	}

	cdev->dbg_mdump.buf_size = len_rounded;

	return 0;
}

#define QED_DBG_MDUMP_RETAIN_LENGTH	64

static int qed_dbg_mdump_get_retain(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt)
{
	struct qed_mdump_retain_data mdump_retain;
	struct qed_dev *cdev = p_hwfn->cdev;
	int rc;

	qed_dbg_mdump_free_buf(cdev);

	cdev->dbg_mdump.buf = kzalloc(QED_DBG_MDUMP_RETAIN_LENGTH, GFP_KERNEL);
	if (!cdev->dbg_mdump.buf) {
		DP_NOTICE(cdev, "Failed to allocate buffer\n");
		return -ENOMEM;
	}

	rc = qed_mcp_mdump_get_retain(p_hwfn, p_ptt, &mdump_retain);
	if (rc) {
		qed_dbg_mdump_free_buf(cdev);
		return rc;
	}

	cdev->dbg_mdump.buf_size = QED_DBG_MDUMP_RETAIN_LENGTH;

	snprintf(cdev->dbg_mdump.buf, QED_DBG_MDUMP_RETAIN_LENGTH,
		 "valid 0x%x, epoch 0x%08x, pf 0x%x, status 0x%08x\n",
		 mdump_retain.valid, mdump_retain.epoch, mdump_retain.pf,
		 mdump_retain.status);

	return 0;
}

static int qed_str_mdump_status(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				char *params_string)
{
	int rc = qed_dbg_mdump_get_status(p_hwfn, p_ptt);

	if (!rc)
		p_hwfn->cdev->dbg_mdump.cmd = QED_DBG_MDUMP_CMD_STATUS;

	return rc;
}

static int qed_str_mdump_trigger(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 char *params_string)
{
	int rc = qed_mcp_mdump_trigger(p_hwfn, p_ptt);

	if (!rc)
		p_hwfn->cdev->dbg_mdump.cmd = QED_DBG_MDUMP_CMD_TRIGGER;

	return rc;
}

static int qed_str_mdump_dump(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      char *params_string)
{
	int rc = qed_dbg_mdump_get_image(p_hwfn);

	if (!rc)
		p_hwfn->cdev->dbg_mdump.cmd = QED_DBG_MDUMP_CMD_DUMP;

	return rc;
}

static int qed_str_mdump_clear(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			       char *params_string)
{
	int rc = qed_mcp_mdump_clear_logs(p_hwfn, p_ptt);

	if (!rc)
		p_hwfn->cdev->dbg_mdump.cmd = QED_DBG_MDUMP_CMD_CLEAR;

	return rc;
}

static int qed_str_mdump_get_retain(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string)
{
	int rc = qed_dbg_mdump_get_retain(p_hwfn, p_ptt);

	if (!rc)
		p_hwfn->cdev->dbg_mdump.cmd = QED_DBG_MDUMP_CMD_GET_RETAIN;

	return rc;
}

static int qed_str_mdump_clr_retain(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string)
{
	int rc = qed_mcp_mdump_clr_retain(p_hwfn, p_ptt);

	if (!rc)
		p_hwfn->cdev->dbg_mdump.cmd = QED_DBG_MDUMP_CMD_CLR_RETAIN;

	return rc;
}

const static struct qed_func_lookup qed_mdump_func_lookup[] = {
	{"status", qed_str_mdump_status},
	{"trigger", qed_str_mdump_trigger},
	{"dump", qed_str_mdump_dump},
	{"clear", qed_str_mdump_clear},
	{"get_retain", qed_str_mdump_get_retain},
	{"clr_retain", qed_str_mdump_clr_retain},
};

static ssize_t qed_dbg_mdump_cmd_write(struct file *filp,
				       const char __user * buffer, size_t count,
				       loff_t * ppos)
{
	struct qed_dev *cdev = (struct qed_dev *)filp->private_data;
	int num_funcs = sizeof(qed_mdump_func_lookup) /
	    sizeof(qed_mdump_func_lookup[0]);

	cdev->dbg_mdump.cmd = QED_DBG_MDUMP_CMD_NONE;

	return qed_dbg_cmd_write(filp, buffer, count, ppos, qed_mdump_func_lookup, num_funcs, true,	/* from user */
				 false,	/* not a dbg hsi function */
				 false /* not a tests function */ );
}

static ssize_t qed_dbg_mdump_cmd_read(struct file *filp, char __user * buffer,
				      size_t count, loff_t * ppos)
{
	struct qed_dev *cdev = (struct qed_dev *)filp->private_data;
	char key_list[] =
	    "status\ntrigger\ndump\nclear\nget_retain\nclr_retain\n";
	int len = 0;

	switch (cdev->dbg_mdump.cmd) {
	case QED_DBG_MDUMP_CMD_NONE:
		/* If no key was given - print the list of the valid keys */
		len = simple_read_from_buffer(buffer, count, ppos, key_list,
					      strlen(key_list));
		break;
	case QED_DBG_MDUMP_CMD_STATUS:
	case QED_DBG_MDUMP_CMD_DUMP:
	case QED_DBG_MDUMP_CMD_GET_RETAIN:
		len = simple_read_from_buffer(buffer, count, ppos,
					      cdev->dbg_mdump.buf,
					      cdev->dbg_mdump.buf_size);
		if (!len) {
			cdev->dbg_mdump.cmd = QED_DBG_MDUMP_CMD_NONE;
			qed_dbg_mdump_free_buf(cdev);
		}
		break;
	default:
		/* Do nothing for other mdump keys */
		cdev->dbg_mdump.cmd = QED_DBG_MDUMP_CMD_NONE;
		break;
	}

	return len;
}

static int qed_str_all_data_dump(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 char *params_string)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	int rc;

	/* Use a previously saved buffer if exists */
	if (cdev->p_dbg_data_buf)
		return 0;

	rc = __qed_dbg_save_all_data(cdev);

	return rc;
}

const static struct qed_func_lookup qed_all_data_func_lookup[] = {
	{"dump", qed_str_all_data_dump},
};

static ssize_t qed_dbg_all_data_cmd_write(struct file *filp,
					  const char __user * buffer,
					  size_t count, loff_t * ppos)
{
	int num_funcs = sizeof(qed_all_data_func_lookup) /
	    sizeof(qed_all_data_func_lookup[0]);

	return qed_dbg_cmd_write(filp, buffer, count, ppos, qed_all_data_func_lookup, num_funcs, true,	/* from user */
				 false,	/* not a dbg hsi function */
				 false /* not a tests function */ );
}

static ssize_t qed_dbg_all_data_cmd_read(struct file *filp,
					 char __user * buffer, size_t count,
					 loff_t * ppos)
{
	struct qed_dev *cdev = (struct qed_dev *)filp->private_data;
	int len = 0;

	if (!cdev->p_dbg_data_buf) {
		DP_NOTICE(cdev, "No data in the debug data buffer\n");
		return 0;
	}

	len = simple_read_from_buffer(buffer, count, ppos, cdev->p_dbg_data_buf,
				      cdev->dbg_data_buf_size);
	if (!len)
		qed_dbg_all_data_free_buf(cdev);

	return len;
}

static const struct file_operations preconfig_fops =
qed_debugfs_fileops(preconfig);
static const struct file_operations preconfig_engine_fops =
qed_debugfs_fileops(preconfig_engine);
static const struct file_operations chain_print_fops =
qed_debugfs_fileops(chain_print);
static const struct file_operations postconfig_fops =
qed_debugfs_fileops(postconfig);
#ifndef QED_UPSTREAM		/* ! QED_UPSTREAM */
static const struct file_operations tests_fops = qed_debugfs_fileops(tests);
#endif
static const struct file_operations engine_fops = qed_debugfs_fileops(engine);
static const struct file_operations phy_fops = qed_debugfs_fileops(phy);
static const struct file_operations mdump_fops = qed_debugfs_fileops(mdump);
static const struct file_operations all_data_fops =
qed_debugfs_fileops(all_data);

#ifdef _HAS_SYSFS_BIN_ATTR_INIT
#ifndef QED_UPSTREAM		/* ! QED_UPSTREAM */
static ssize_t sysfs_show(struct file *filp, struct kobject *kobp,
			  struct bin_attribute *bin_attr, char *buf,
			  loff_t pos, size_t count)
{
	struct qed_dev *cdev = (struct qed_dev *)bin_attr->private;
	char *data = NULL;
	int len = 0;

	if (strcmp(bin_attr->attr.name, "tests") == 0) {
		if (cdev->test_result_available) {
			data = (char *)&cdev->test_result;
			len = sizeof(cdev->test_result);
		} else {
			data = (char *)tests_list;
			len = strlen(tests_list);
		}
		len = memory_read_from_buffer(buf, count, &pos, data, len);
		if (len == 0)
			cdev->test_result_available = false;
	}

	if (strcmp(bin_attr->attr.name, "phy") == 0) {
		if (p_phy_result_buf != NULL) {
			data = p_phy_result_buf;
			len = strlen(p_phy_result_buf);
		} else {
			data = (char *)phy_list;
			len = strlen(phy_list);
		}
		len = memory_read_from_buffer(buf, count, &pos, data, len);
		if (len == 0) {
			kfree(p_phy_result_buf);
			p_phy_result_buf = NULL;
		}
	}
	return len;
}

static ssize_t sysfs_store(struct file *filp, struct kobject *kobj,
			   struct bin_attribute *bin_attr, char *buf,
			   loff_t pos, size_t count)
{
	if (strcmp(bin_attr->attr.name, "tests") == 0) {
		return qed_sysfs_cmd_write(buf, count, &pos,
					   qed_tests_func_lookup,
					   TESTS_NUM_STR_FUNCS,
					   true /* from user */ ,
					   false /* not an dbg hsi function */ ,
					   true /* tests function */ ,
					   bin_attr);
	}
	if (strcmp(bin_attr->attr.name, "phy") == 0) {
		if (p_phy_result_buf == NULL)
			p_phy_result_buf = kzalloc(sizeof(char) *
						   MAX_PHY_RESULT_BUFFER,
						   GFP_KERNEL);
		return qed_sysfs_cmd_write(buf, count, &pos,
					   qed_phy_func_lookup,
					   PHY_NUM_STR_FUNCS,
					   true /* from user */ ,
					   false /* not a dbg hsi function */ ,
					   false /* not a tests function */ ,
					   bin_attr);
	}
	return 0;
}
#endif
#endif

/**
 * qed_dbg_pf_init - setup the debugfs file for the pf
 * @pf: the pf that is starting up
 **/
void qed_dbg_pf_init(struct qed_dev *cdev)
{
	char pf_dirname[9];	/* e.g. 00:04:00 +1 null termination */
	enum qed_dbg_features feature_idx;
	struct dentry *file_dentry = NULL;
	const u8 *dbg_values = NULL;
#ifndef QED_UPSTREAM
	size_t size;
#endif
	int i;

	/* Sync ver with debugbus qed code */
	qed_dbg_set_app_ver(TOOLS_VERSION);

	/* Debug values are after init values.
	 * The offset is the first dword of the file.
	 */
#ifdef CONFIG_QED_BINARY_FW
#ifndef QED_UPSTREAM
	dbg_values = cdev->fw_buf + *(u32 *) cdev->fw_buf;
#else
	dbg_values = cdev->firmware->data + *(u32 *) cdev->firmware->data;
#endif
#endif

	for_each_hwfn(cdev, i) {
		qed_dbg_set_bin_ptr(&cdev->hwfns[i], dbg_values);
		qed_dbg_user_set_bin_ptr(&cdev->hwfns[i], dbg_values);
	}

	/* Set the hwfn to be 0 as default */
	cdev->engine_for_debug = 0;

	if (!qed_dbg_root)
		return;

	/* Create pf dir */
	sprintf(pf_dirname, "%02x:%02x.%x", cdev->pdev->bus->number,
		PCI_SLOT(cdev->pdev->devfn), PCI_FUNC(cdev->pdev->devfn));
	cdev->bdf_dentry = debugfs_create_dir(pf_dirname, qed_dbg_root);
	if (!cdev->bdf_dentry) {
		pr_notice("debugfs entry %s creation failed\n", pf_dirname);
		return;
	}

	/* Create debug features debugfs files */
	for (feature_idx = 0; feature_idx < DBG_FEATURE_NUM; feature_idx++) {
		const struct file_operations *fops;
		char *name;

		DP_VERBOSE(cdev, QED_MSG_DEBUG,
			   "Creating debugfs node for %s\n",
			   qed_features_lookup[feature_idx].name);

		name = qed_features_lookup[feature_idx].name;
		fops = &qed_feature_fops[feature_idx];
		file_dentry = debugfs_create_file(name, 0600, cdev->bdf_dentry,
						  cdev, fops);
		if (!file_dentry)
			DP_NOTICE(cdev, "debugfs entry %s creation failed\n",
				  name);
	}

#ifndef QED_UPSTREAM		/* ! QED_UPSTREAM */
	/* Create tests debugfs node */
	DP_VERBOSE(cdev, QED_MSG_DEBUG, "Creating debugfs tests node\n");
	file_dentry = debugfs_create_file("tests", 0600, cdev->bdf_dentry, cdev,
					  &tests_fops);
	if (!file_dentry)
		DP_NOTICE(cdev, "debugfs tests entry creation failed\n");
#endif

	/* Create engine debugfs node */
	DP_VERBOSE(cdev, QED_MSG_DEBUG, "creating debugfs engine node\n");
	file_dentry = debugfs_create_file("engine", 0600, cdev->bdf_dentry,
					  cdev, &engine_fops);
	if (!file_dentry)
		DP_NOTICE(cdev, "debugfs entry engine creation failed\n");

	/* Create phy debugfs node */
	DP_VERBOSE(cdev, QED_MSG_DEBUG, "creating debugfs PHY node\n");
	file_dentry = debugfs_create_file("phy", 0600, cdev->bdf_dentry, cdev,
					  &phy_fops);
	if (!file_dentry)
		DP_NOTICE(cdev, "debugfs entry PHY creation failed\n");

	/* Create chain_print file under qed */
	DP_VERBOSE(cdev, QED_MSG_DEBUG, "creating debugfs Chain Print node\n");
	file_dentry = debugfs_create_file("chain_print", 0600, cdev->bdf_dentry,
					  cdev, &chain_print_fops);
	if (!file_dentry)
		DP_NOTICE(cdev, "debugfs entry Chain Print creation failed\n");

	/* Create mdump file under qed */
	DP_VERBOSE(cdev, QED_MSG_DEBUG, "creating debugfs mdump node\n");
	file_dentry = debugfs_create_file("mdump", 0600, cdev->bdf_dentry, cdev,
					  &mdump_fops);
	if (!file_dentry)
		DP_NOTICE(cdev, "debugfs entry mdump creation failed\n");

	/* Create all_data file under qed */
	DP_VERBOSE(cdev, QED_MSG_DEBUG, "creating debugfs all_data node\n");
	file_dentry = debugfs_create_file("all_data", 0600, cdev->bdf_dentry,
					  cdev, &all_data_fops);
	if (!file_dentry)
		DP_NOTICE(cdev, "debugfs entry all_data creation failed\n");

#ifndef QED_UPSTREAM
	/* Enable/disable the saving of auto-collected debug data to file */
	size = strnlen(dbg_data_path, QED_DBG_DATA_PATH_MAX_SIZE);
	cdev->b_dump_dbg_data = ! !size;
	if (cdev->b_dump_dbg_data)
		strncpy(cdev->dbg_data_path, dbg_data_path,
			QED_DBG_DATA_PATH_MAX_SIZE);
#endif

	return;
}

/**
 * qed_dbg_pf_exit - clear out the pf's debugfs entries
 * @pf: the pf that is stopping
 **/
void qed_dbg_pf_exit(struct qed_dev *cdev)
{
	struct qed_dbg_feature *feature = NULL;
	enum qed_dbg_features feature_idx;

	/* remove debugfs entries of this PF */
	DP_VERBOSE(cdev, QED_MSG_DEBUG, "removing debugfs entry of PF %d\n",
		   cdev->hwfns[0].abs_pf_id);
	debugfs_remove_recursive(cdev->bdf_dentry);
	cdev->bdf_dentry = NULL;

	/* debug features' buffers may be allocated if debug feature was used but dump wasn't called */
	for (feature_idx = 0; feature_idx < DBG_FEATURE_NUM; feature_idx++) {
		feature = &cdev->dbg_features[feature_idx];
		if (feature->dump_buf) {
			vfree(feature->dump_buf);
			feature->dump_buf = NULL;
		}
	}

	/* The mdump buffer may be allocated if a command was writtetn w/o a
	 * following read.
	 */
	qed_dbg_mdump_free_buf(cdev);

	/* free a previously saved buffer if exists */
	vfree(cdev->p_dbg_data_buf);
	cdev->p_dbg_data_buf = NULL;
}

/**
 * qed_init - start up debugfs for the driver
 **/
void qed_dbg_init(void)
{
	struct dentry *file_dentry = NULL;

	pr_notice("creating debugfs root node\n");

	INIT_LIST_HEAD(&preconfig_pool.preconfig_list);

	/* Create qed dir in root of debugfs. NULL means debugfs root. */
	qed_dbg_root = debugfs_create_dir("qed", NULL);
	if (!qed_dbg_root) {
		pr_notice("init of debugfs failed\n");
		return;
	}

	/* Create preconfig file under qed */
	file_dentry = debugfs_create_file("preconfig", 0600, qed_dbg_root, NULL,
					  &preconfig_fops);
	if (!file_dentry) {
		pr_notice("debugfs entry preconfig creation failed\n");
		free_preconfig();
	}

	/* Create preconfig engine file under qed */
	file_dentry = debugfs_create_file("preconfig_engine", 0600,
					  qed_dbg_root, NULL,
					  &preconfig_engine_fops);
	if (!file_dentry)
		pr_notice("debugfs entry preconfig engine creation failed\n");

	/* Create postconfig file under qed */
	file_dentry =
	    debugfs_create_file("postconfig", 0600, qed_dbg_root, NULL,
				&postconfig_fops);
	if (!file_dentry)
		pr_notice("debugfs entry postconfig creation failed\n");
}

/**
 * qed_dbg_exit - clean out the driver's debugfs entries
 **/
void qed_dbg_exit(void)
{
	if (!qed_dbg_root)
		return;

	pr_notice("destroying debugfs root entry\n");

	/* remove preconfig list */
	free_preconfig();

	/* free postconfig info */
	free_postconfig();

	/* remove qed dir in root of debugfs */
	debugfs_remove_recursive(qed_dbg_root);
	qed_dbg_root = NULL;
}

#ifdef _HAS_SYSFS_BIN_ATTR_INIT
/* qed_sysfs_pf_init - setup the sysfs file for the pf
 * @pf: the pf that is starting up
 */
void qed_sysfs_pf_init(struct qed_dev *cdev)
{
	char pf_dirname[9];	/* e.g. 00:04:00 +1 null termination */
	struct qed_sysfs_obj *tests_info = NULL;
	struct qed_sysfs_obj *phy_info = NULL;
	int status = 0;

	cdev->bdf_kobj = NULL;
	/* Set the hwfn to be 0 as default */
	cdev->engine_for_debug = 0;
	/* Create pf dir */
	sprintf(pf_dirname, "%02x:%02x.%x", cdev->pdev->bus->number,
		PCI_SLOT(cdev->pdev->devfn), PCI_FUNC(cdev->pdev->devfn));
	/* sysfs entries creation */
	tests_info = kzalloc(sizeof(*tests_info), GFP_KERNEL);
	if (!tests_info) {
		pr_err("sysfs memory allocation failed\n");
		status = -1;
		goto err;
	}
	/* sysfs entries creation */
	phy_info = kzalloc(sizeof(*phy_info), GFP_KERNEL);
	if (!phy_info) {
		pr_err("sysfs memory allocation failed\n");
		status = -1;
		goto err;
	}
	__init_sysfs(tests);
	__init_sysfs(phy);
	/* create only once */
	if (!qed_sysfs_root) {
		qed_sysfs_root = kobject_create_and_add("qed", NULL);
		if (!qed_sysfs_root) {
			kfree(tests_info);
			kfree(phy_info);
			status = -1;
			goto err;
		}
	}
	/* Create B:D:F entry */
	cdev->bdf_kobj = kobject_create_and_add(pf_dirname, qed_sysfs_root);
	if (!cdev->bdf_kobj) {
		kfree(tests_info);
		kfree(phy_info);
		status = -1;
		goto err;
	}
#ifndef QED_UPSTREAM		/* ! QED_UPSTREAM */
	/* create /sys/qed/B:D:F/tests */
	tests_info->bin_attr.attr.name = "tests";
	if (sysfs_create_bin_file(cdev->bdf_kobj, &tests_info->bin_attr)) {
		kobject_del(cdev->bdf_kobj);
		kfree(tests_info);
		status = -1;
		goto err;
	}
#endif
	/* create /sys/qed/B:D:F/phy */
	phy_info->bin_attr.attr.name = "phy";
	if (sysfs_create_bin_file(cdev->bdf_kobj, &phy_info->bin_attr)) {
		kobject_del(cdev->bdf_kobj);
		kfree(phy_info);
		status = -1;
		goto err;
	}
	tests_info->bdf_kobj = cdev->bdf_kobj;
	phy_info->bdf_kobj = cdev->bdf_kobj;
	list_add_tail(&tests_info->list, &qed_sysfs_tests_head);
	list_add_tail(&phy_info->list, &qed_sysfs_phy_head);
err:
	if (status == -1)
		pr_err("Sysfs entries failed\n");
	return;
}

/* qed_sysfs_pf_exit - clear out the pf's sysfs entries
 * @pf: the pf that is stopping
 */
void qed_sysfs_pf_exit(struct qed_dev *cdev)
{
	struct qed_sysfs_obj *info = NULL;
	struct qed_sysfs_obj *temp = NULL;

	/* remove sysfs entries of this PF */
	DP_VERBOSE(cdev, QED_MSG_DEBUG, "removing sysfs entry of PF %d\n",
		   cdev->hwfns[0].abs_pf_id);
	/* remove bdf sysfs entries and memory allocated of this PF */
	kobject_del(cdev->bdf_kobj);
	list_for_each_entry_safe_reverse(info, temp, &qed_sysfs_tests_head,
					 list) {
		if (info->bdf_kobj == cdev->bdf_kobj) {
			list_del(&info->list);
			kfree(info);
			break;
		}
	}
	list_for_each_entry_safe_reverse(info, temp, &qed_sysfs_phy_head, list) {
		if (info->bdf_kobj == cdev->bdf_kobj) {
			list_del(&info->list);
			kfree(info);
			break;
		}
	}
	cdev->bdf_kobj = NULL;
}

void qed_sysfs_init(void)
{
	pr_notice("creating sysfs entries\n");
	INIT_LIST_HEAD(&qed_sysfs_tests_head);
	INIT_LIST_HEAD(&qed_sysfs_phy_head);
}

void qed_sysfs_exit(void)
{
	if (!qed_sysfs_root)
		return;
	pr_notice("destroying sysfs root entry\n");
	kobject_del(qed_sysfs_root);
	qed_sysfs_root = NULL;
}

#endif /* END _HAS_SYSFS_BIN_ATTR_INIT */

void qed_set_platform_str_linux(struct qed_hwfn *p_hwfn,
				char *buf_str, u32 buf_size)
{
	snprintf(buf_str, buf_size, "Kernel %d.%d.%d.",
		 (LINUX_VERSION_CODE & 0xf0000) >> 16,
		 (LINUX_VERSION_CODE & 0x0ff00) >> 8,
		 (LINUX_VERSION_CODE & 0x000ff));
}

#else /* CONFIG_DEBUG_FS */
void qed_dbg_pf_init(struct qed_dev *cdev)
{
}

void qed_dbg_pf_exit(struct qed_dev *cdev)
{
}

void qed_dbg_init(void)
{
}

void qed_dbg_exit(void)
{
}

void qed_copy_preconfig_to_bus(struct qed_dev *cdev, u8 init_engine)
{
}

int qed_copy_bus_to_postconfig(struct qed_dev *cdev, u8 down_engine)
{
	return 0;
}

u8 qed_get_debug_engine(struct qed_dev * cdev)
{
	return 0;
}
#endif /* CONFIG_DEBUG_FS */

#ifndef _HAS_SYSFS_BIN_ATTR_INIT
void qed_sysfs_pf_init(struct qed_dev *cdev)
{
}

void qed_sysfs_pf_exit(struct qed_dev *cdev)
{
}

void qed_sysfs_init(void)
{
}

void qed_sysfs_exit(void)
{
}
#endif
