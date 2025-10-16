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

#ifndef _QED_MFW_HSI_H
#define _QED_MFW_HSI_H
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/slab.h>
#include "qed_fcoe_if.h"
#include "qed_if.h"
#include "qed_iscsi_if.h"
/****************************************************************************
*
* Name:        mfw_hsi.h
*
* Description: Global definitions
*
****************************************************************************/

#define MFW_HSI_H

#define MFW_TRACE_SIGNATURE		0x25071946

/* The trace in the buffer */
#define MFW_TRACE_EVENTID_MASK		0x00ffff
#define MFW_TRACE_PRM_SIZE_MASK		0x0f0000
#define MFW_TRACE_PRM_SIZE_OFFSET	16
#define MFW_TRACE_ENTRY_SIZE		3

struct mcp_trace {
	u32 signature;		/* Help to identify that the trace is valid */
	u32 size;		/* the size of the trace buffer in bytes */
	u32 curr_level;		/* 2 - all will be written to the buffer
				 * 1 - debug trace will not be written
				 * 0 - just errors will be written to the buffer
				 */
	u32 modules_mask[2];	/* a bit per module, 1 means mask it off, 0 means add it to the trace buffer */

	/* Warning: the following pointers are assumed to be 32bits as they are used only in the MFW */
	u32 trace_prod;		/* The next trace will be written to this offset */
	u32 trace_oldest;	/* The oldest valid trace starts at this offset (usually very close after the current producer) */
};

/****************************************************************************
*
* Name:		mcp_public.h
*
* Description: MCP public data
*
* Created:      13/01/2013 yanivr
*
****************************************************************************/

#define VF_MAX_STATIC			192	/* In case of AH */
#define VF_BITMAP_SIZE_IN_DWORDS	(VF_MAX_STATIC / 32)
#define VF_BITMAP_SIZE_IN_BYTES		(VF_BITMAP_SIZE_IN_DWORDS * sizeof(u32))

/* Extended array size to support for 240 VFs 8 dwords */
#define EXT_VF_MAX_STATIC		240
#define EXT_VF_BITMAP_SIZE_IN_DWORDS	(((EXT_VF_MAX_STATIC - 1) / 32) + 1)
#define EXT_VF_BITMAP_SIZE_IN_BYTES	(EXT_VF_BITMAP_SIZE_IN_DWORDS * \
                                         sizeof(u32))
#define ADDED_VF_BITMAP_SIZE		2

#define MCP_GLOB_PATH_MAX		2
#define MCP_PORT_MAX			2	/* Global */
#define MCP_GLOB_PORT_MAX		4	/* Global */
#define MCP_GLOB_FUNC_MAX		16	/* Global */

typedef u32 offsize_t;		/* In DWORDS !!! */
/* Offset from the beginning of the MCP scratchpad */
#define OFFSIZE_OFFSET_OFFSET		0
#define OFFSIZE_OFFSET_MASK		0x0000ffff
/* Size of specific element (not the whole array if any) */
#define OFFSIZE_SIZE_OFFSET		16
#define OFFSIZE_SIZE_MASK		0xffff0000

/* SECTION_OFFSET is calculating the offset in bytes out of offsize */
#define SECTION_OFFSET(_offsize)			((((_offsize &            \
                                                            OFFSIZE_OFFSET_MASK)  \
                                                           >>                     \
                                                           OFFSIZE_OFFSET_OFFSET) \
                                                          << 2))

/* QED_SECTION_SIZE is calculating the size in bytes out of offsize */
#define QED_SECTION_SIZE(_offsize)			(((_offsize &          \
                                                           OFFSIZE_SIZE_MASK)  \
                                                          >>                   \
                                                          OFFSIZE_SIZE_OFFSET) \
                                                         << 2)

/* SECTION_ADDR returns the GRC addr of a section, given offsize and index within section */
#define SECTION_ADDR(_offsize, idx)			(MCP_REG_SCRATCH +   \
                                                         SECTION_OFFSET(     \
                                                                 _offsize) + \
                                                         (QED_SECTION_SIZE(  \
                                                                  _offsize) * idx))

/* SECTION_OFFSIZE_ADDR returns the GRC addr to the offsize address. Use offsetof, since the OFFSETUP collide with the firmware definition */
#define SECTION_OFFSIZE_ADDR(_pub_base, _section)	(_pub_base +               \
                                                         offsetof(struct           \
                                                                  mcp_public_data, \
                                                                  sections[        \
                                                                          _section]))
/* PHY configuration */
struct eth_phy_cfg {
	u32 speed;		/* 0 = autoneg, 1000/10000/20000/25000/40000/50000/100000 */
#define ETH_SPEED_AUTONEG			0
#define ETH_SPEED_SMARTLINQ			0x8	/* deprecated - use link_modes field instead */

	u32 pause;		/* bitmask */
#define ETH_PAUSE_NONE				0x0
#define ETH_PAUSE_AUTONEG			0x1
#define ETH_PAUSE_RX				0x2
#define ETH_PAUSE_TX				0x4

	u32 adv_speed;		/* Default should be the speed_cap_mask */
	u32 loopback_mode;
#define ETH_LOOPBACK_NONE			(0)
#define ETH_LOOPBACK_INT_PHY			(1)	/* Serdes loopback. In AH, it refers to Near End */
#define ETH_LOOPBACK_EXT_PHY			(2)	/* External PHY Loopback */
#define ETH_LOOPBACK_EXT			(3)	/* External Loopback (Require loopback plug) */
#define ETH_LOOPBACK_MAC			(4)	/* MAC Loopback - not supported */
#define ETH_LOOPBACK_CNIG_AH_ONLY_0123		(5)	/* Port to itself */
#define ETH_LOOPBACK_CNIG_AH_ONLY_2301		(6)	/* Port to Port */
#define ETH_LOOPBACK_PCS_AH_ONLY		(7)	/* PCS loopback (TX to RX) */
#define ETH_LOOPBACK_REVERSE_MAC_AH_ONLY	(8)	/* Loop RX packet from PCS to TX */
#define ETH_LOOPBACK_INT_PHY_FEA_AH_ONLY	(9)	/* Remote Serdes Loopback (RX to TX) */

	u32 eee_cfg;
#define EEE_CFG_EEE_ENABLED			(1 << 0)	/* EEE is enabled (configuration). Refer to eee_status->active for negotiated status */
#define EEE_CFG_TX_LPI				(1 << 1)
#define EEE_CFG_ADV_SPEED_1G			(1 << 2)
#define EEE_CFG_ADV_SPEED_10G			(1 << 3)
#define EEE_TX_TIMER_USEC_MASK			(0xfffffff0)
#define EEE_TX_TIMER_USEC_OFFSET		4
#define EEE_TX_TIMER_USEC_BALANCED_TIME		(0xa00)
#define EEE_TX_TIMER_USEC_AGGRESSIVE_TIME	(0x100)
#define EEE_TX_TIMER_USEC_LATENCY_TIME		(0x6000)

	u32 link_modes;		/* Additional link modes */
#define LINK_MODE_SMARTLINQ_ENABLE		0x1	/* XXX Deprecate */

	u32 fec_mode;		/* Values similar to nvm cfg */
#define FEC_FORCE_MODE_MASK			0x000000FF
#define FEC_FORCE_MODE_OFFSET			0
#define FEC_FORCE_MODE_NONE			0x00
#define FEC_FORCE_MODE_FIRECODE			0x01
#define FEC_FORCE_MODE_RS			0x02
#define FEC_FORCE_MODE_AUTO			0x07
#define FEC_EXTENDED_MODE_MASK			0xFFFFFF00
#define FEC_EXTENDED_MODE_OFFSET		8
#define ETH_EXT_FEC_NONE			0x00000000
#define ETH_EXT_FEC_10G_NONE			0x00000100
#define ETH_EXT_FEC_10G_BASE_R			0x00000200
#define ETH_EXT_FEC_25G_NONE			0x00000400
#define ETH_EXT_FEC_25G_BASE_R			0x00000800
#define ETH_EXT_FEC_25G_RS528			0x00001000
#define ETH_EXT_FEC_40G_NONE			0x00002000
#define ETH_EXT_FEC_40G_BASE_R			0x00004000
#define ETH_EXT_FEC_50G_NONE			0x00008000
#define ETH_EXT_FEC_50G_BASE_R			0x00010000
#define ETH_EXT_FEC_50G_RS528			0x00020000
#define ETH_EXT_FEC_50G_RS544			0x00040000
#define ETH_EXT_FEC_100G_NONE			0x00080000
#define ETH_EXT_FEC_100G_BASE_R			0x00100000
#define ETH_EXT_FEC_100G_RS528			0x00200000
#define ETH_EXT_FEC_100G_RS544			0x00400000
	u32 extended_speed;	/* Values similar to nvm cfg */
#define ETH_EXT_SPEED_MASK			0x0000FFFF
#define ETH_EXT_SPEED_OFFSET			0
#define ETH_EXT_SPEED_NONE			0x00000001
#define ETH_EXT_SPEED_1G			0x00000002
#define ETH_EXT_SPEED_10G			0x00000004
#define ETH_EXT_SPEED_25G			0x00000008
#define ETH_EXT_SPEED_40G			0x00000010
#define ETH_EXT_SPEED_50G_BASE_R		0x00000020
#define ETH_EXT_SPEED_50G_BASE_R2		0x00000040
#define ETH_EXT_SPEED_100G_BASE_R2		0x00000080
#define ETH_EXT_SPEED_100G_BASE_R4		0x00000100
#define ETH_EXT_SPEED_100G_BASE_P4		0x00000200
#define ETH_EXT_ADV_SPEED_MASK			0xFFFF0000
#define ETH_EXT_ADV_SPEED_OFFSET		16
#define ETH_EXT_ADV_SPEED_1G			0x00010000
#define ETH_EXT_ADV_SPEED_10G			0x00020000
#define ETH_EXT_ADV_SPEED_25G			0x00040000
#define ETH_EXT_ADV_SPEED_40G			0x00080000
#define ETH_EXT_ADV_SPEED_50G_BASE_R		0x00100000
#define ETH_EXT_ADV_SPEED_50G_BASE_R2		0x00200000
#define ETH_EXT_ADV_SPEED_100G_BASE_R2		0x00400000
#define ETH_EXT_ADV_SPEED_100G_BASE_R4		0x00800000
#define ETH_EXT_ADV_SPEED_100G_BASE_P4		0x01000000
};

struct port_mf_cfg {
	u32 dynamic_cfg;	/* device control channel */
#define PORT_MF_CFG_OV_TAG_MASK		0x0000ffff
#define PORT_MF_CFG_OV_TAG_OFFSET	0
#define PORT_MF_CFG_OV_TAG_DEFAULT	PORT_MF_CFG_OV_TAG_MASK

	u32 reserved[2];	/* This is to make sure next field "stats" is alignment to 64-bit.
				 *                      It doesn't change existing alignment in MFW */
};

/* DO NOT add new fields in the middle
 * MUST be synced with struct pmm_stats_map
 */
struct eth_stats {
	u64 r64;		/* 0x00 (Offset 0x00 ) RX 64-byte frame counter */
	u64 r127;		/* 0x01 (Offset 0x08 ) RX 65 to 127 byte frame counter */
	u64 r255;		/* 0x02 (Offset 0x10 ) RX 128 to 255 byte frame counter */
	u64 r511;		/* 0x03 (Offset 0x18 ) RX 256 to 511 byte frame counter */
	u64 r1023;		/* 0x04 (Offset 0x20 ) RX 512 to 1023 byte frame counter */
	u64 r1518;		/* 0x05 (Offset 0x28 ) RX 1024 to 1518 byte frame counter */
	union {
		struct {	/* bb */
			u64 r1522;	/* 0x06 (Offset 0x30 ) RX 1519 to 1522 byte VLAN-tagged frame counter */
			u64 r2047;	/* 0x07 (Offset 0x38 ) RX 1519 to 2047 byte frame counter */
			u64 r4095;	/* 0x08 (Offset 0x40 ) RX 2048 to 4095 byte frame counter */
			u64 r9216;	/* 0x09 (Offset 0x48 ) RX 4096 to 9216 byte frame counter */
			u64 r16383;	/* 0x0A (Offset 0x50 ) RX 9217 to 16383 byte frame counter */
		} bb0;
		struct {	/* ah */
			u64 unused1;
			u64 r1519_to_max;	/* 0x07 (Offset 0x38 ) RX 1519 to max byte frame counter */
			u64 unused2;
			u64 unused3;
			u64 unused4;
		} ah0;
	} u0;
	u64 rfcs;		/* 0x0F (Offset 0x58 ) RX FCS error frame counter */
	u64 rxcf;		/* 0x10 (Offset 0x60 ) RX control frame counter */
	u64 rxpf;		/* 0x11 (Offset 0x68 ) RX pause frame counter */
	u64 rxpp;		/* 0x12 (Offset 0x70 ) RX PFC frame counter */
	u64 raln;		/* 0x16 (Offset 0x78 ) RX alignment error counter */
	u64 rfcr;		/* 0x19 (Offset 0x80 ) RX false carrier counter */
	u64 rovr;		/* 0x1A (Offset 0x88 ) RX oversized frame counter */
	u64 rjbr;		/* 0x1B (Offset 0x90 ) RX jabber frame counter */
	u64 rund;		/* 0x34 (Offset 0x98 ) RX undersized frame counter */
	u64 rfrg;		/* 0x35 (Offset 0xa0 ) RX fragment counter */
	u64 t64;		/* 0x40 (Offset 0xa8 ) TX 64-byte frame counter */
	u64 t127;		/* 0x41 (Offset 0xb0 ) TX 65 to 127 byte frame counter */
	u64 t255;		/* 0x42 (Offset 0xb8 ) TX 128 to 255 byte frame counter */
	u64 t511;		/* 0x43 (Offset 0xc0 ) TX 256 to 511 byte frame counter */
	u64 t1023;		/* 0x44 (Offset 0xc8 ) TX 512 to 1023 byte frame counter */
	u64 t1518;		/* 0x45 (Offset 0xd0 ) TX 1024 to 1518 byte frame counter */
	union {
		struct {	/* bb */
			u64 t2047;	/* 0x47 (Offset 0xd8 ) TX 1519 to 2047 byte frame counter */
			u64 t4095;	/* 0x48 (Offset 0xe0 ) TX 2048 to 4095 byte frame counter */
			u64 t9216;	/* 0x49 (Offset 0xe8 ) TX 4096 to 9216 byte frame counter */
			u64 t16383;	/* 0x4A (Offset 0xf0 ) TX 9217 to 16383 byte frame counter */
		} bb1;
		struct {	/* ah */
			u64 t1519_to_max;	/* 0x47 (Offset 0xd8 ) TX 1519 to max byte frame counter */
			u64 unused6;
			u64 unused7;
			u64 unused8;
		} ah1;
	} u1;
	u64 txpf;		/* 0x50 (Offset 0xf8 ) TX pause frame counter */
	u64 txpp;		/* 0x51 (Offset 0x100) TX PFC frame counter */
	union {
		struct {	/* bb */
			u64 tlpiec;	/* 0x6C (Offset 0x108) Transmit Logical Type LLFC message counter */
			u64 tncl;	/* 0x6E (Offset 0x110) Transmit Total Collision Counter */
		} bb2;
		struct {	/* ah */
			u64 unused9;
			u64 unused10;
		} ah2;
	} u2;
	u64 rbyte;		/* 0x3d (Offset 0x118) RX byte counter */
	u64 rxuca;		/* 0x0c (Offset 0x120) RX UC frame counter */
	u64 rxmca;		/* 0x0d (Offset 0x128) RX MC frame counter */
	u64 rxbca;		/* 0x0e (Offset 0x130) RX BC frame counter */
	u64 rxpok;		/* 0x22 (Offset 0x138) RX good frame (good CRC, not oversized, no ERROR) */
	u64 tbyte;		/* 0x6f (Offset 0x140) TX byte counter */
	u64 txuca;		/* 0x4d (Offset 0x148) TX UC frame counter */
	u64 txmca;		/* 0x4e (Offset 0x150) TX MC frame counter */
	u64 txbca;		/* 0x4f (Offset 0x158) TX BC frame counter */
	u64 txcf;		/* 0x54 (Offset 0x160) TX control frame counter */
	/* HSI - Cannot add more stats to this struct. If needed, then need to open new struct */
};

struct pkt_type_cnt {
	u64 tc_tx_pkt_cnt[8];
	u64 tc_tx_oct_cnt[8];
	u64 priority_rx_pkt_cnt[8];
	u64 priority_rx_oct_cnt[8];
};

struct brb_stats {
	u64 brb_truncate[8];
	u64 brb_discard[8];
};

struct port_stats {
	struct brb_stats brb;
	struct eth_stats eth;
};

/*-----+-----------------------------------------------------------------------------
 * Chip | Number and	   | Ports in| Ports in|2 PHY-s |# of ports|# of engines
 *      | rate of physical | team #1 | team #2 |are used|per path  | (paths) enabled
 *      | ports                    |		 |         |		|          |
 **======+==================+=========+=========+========+==========+=================
 * BB   | 1x100G		   | This is special mode, where there are actually 2 HW func
 * BB   | 2x10/20Gbps      | 0,1	 | NA      |  No	| 1        | 1
 * BB   | 2x40 Gbps        | 0,1	 | NA      |  Yes   | 1            | 1
 * BB   | 2x50Gbps         | 0,1	 | NA      |  No	| 1        | 1
 * BB   | 4x10Gbps         | 0,2	 | 1,3     |  No	| 1/2      | 1,2 (2 is optional)
 * BB   | 4x10Gbps         | 0,1	 | 2,3     |  No	| 1/2      | 1,2 (2 is optional)
 * BB   | 4x10Gbps         | 0,3	 | 1,2     |  No	| 1/2      | 1,2 (2 is optional)
 * BB   | 4x10Gbps         | 0,1,2,3 | NA      |  No	| 1        | 1
 * AH   | 2x10/20Gbps      | 0,1	 | NA      |  NA	| 1        | NA
 * AH   | 4x10Gbps         | 0,1	 | 2,3     |  NA	| 2        | NA
 * AH   | 4x10Gbps         | 0,2	 | 1,3     |  NA	| 2        | NA
 * AH   | 4x10Gbps         | 0,3	 | 1,2     |  NA	| 2        | NA
 * AH   | 4x10Gbps         | 0,1,2,3 | NA      |  NA	| 1        | NA
 **======+==================+=========+=========+========+==========+===================
 */

#define CMT_TEAM0			0
#define CMT_TEAM1			1
#define CMT_TEAM_MAX			2

struct couple_mode_teaming {
	u8 port_cmt[MCP_GLOB_PORT_MAX];
#define PORT_CMT_IN_TEAM		(1 << 0)

#define PORT_CMT_PORT_ROLE		(1 << 1)
#define PORT_CMT_PORT_INACTIVE		(0 << 1)
#define PORT_CMT_PORT_ACTIVE		(1 << 1)

#define PORT_CMT_TEAM_MASK		(1 << 2)
#define PORT_CMT_TEAM0			(0 << 2)
#define PORT_CMT_TEAM1			(1 << 2)
};

/**************************************
*     LLDP and DCBX HSI structures
**************************************/
#define LLDP_CHASSIS_ID_STAT_LEN	4
#define LLDP_PORT_ID_STAT_LEN		4
#define DCBX_MAX_APP_PROTOCOL		32
#define MAX_SYSTEM_LLDP_TLV_DATA	32	/* In dwords. 128 in bytes */
#define MAX_TLV_BUFFER			128	/* In dwords. 512 in bytes */
typedef enum _lldp_agent_e {
	LLDP_NEAREST_BRIDGE = 0,
	LLDP_NEAREST_NON_TPMR_BRIDGE,
	LLDP_NEAREST_CUSTOMER_BRIDGE,
	LLDP_MAX_LLDP_AGENTS
} lldp_agent_e;

struct lldp_config_params_s {
	u32 config;
#define LLDP_CONFIG_TX_INTERVAL_MASK		0x000000ff
#define LLDP_CONFIG_TX_INTERVAL_OFFSET		0
#define LLDP_CONFIG_HOLD_MASK			0x00000f00
#define LLDP_CONFIG_HOLD_OFFSET			8
#define LLDP_CONFIG_MAX_CREDIT_MASK		0x0000f000
#define LLDP_CONFIG_MAX_CREDIT_OFFSET		12
#define LLDP_CONFIG_ENABLE_RX_MASK		0x40000000
#define LLDP_CONFIG_ENABLE_RX_OFFSET		30
#define LLDP_CONFIG_ENABLE_TX_MASK		0x80000000
#define LLDP_CONFIG_ENABLE_TX_OFFSET		31
	/* Holds local Chassis ID TLV header, subtype and 9B of payload.
	 * If firtst byte is 0, then we will use default chassis ID */
	u32 local_chassis_id[LLDP_CHASSIS_ID_STAT_LEN];
	/* Holds local Port ID TLV header, subtype and 9B of payload.
	 * If firtst byte is 0, then we will use default port ID */
	u32 local_port_id[LLDP_PORT_ID_STAT_LEN];
};

struct lldp_status_params_s {
	u32 prefix_seq_num;
	u32 status;		/* TBD */
	/* Holds remote Chassis ID TLV header, subtype and 9B of payload. */
	u32 peer_chassis_id[LLDP_CHASSIS_ID_STAT_LEN];
	/* Holds remote Port ID TLV header, subtype and 9B of payload. */
	u32 peer_port_id[LLDP_PORT_ID_STAT_LEN];
	u32 suffix_seq_num;
};

struct dcbx_ets_feature {
	u32 flags;
#define DCBX_ETS_ENABLED_MASK			0x00000001
#define DCBX_ETS_ENABLED_OFFSET			0
#define DCBX_ETS_WILLING_MASK			0x00000002
#define DCBX_ETS_WILLING_OFFSET			1
#define DCBX_ETS_ERROR_MASK			0x00000004
#define DCBX_ETS_ERROR_OFFSET			2
#define DCBX_ETS_CBS_MASK			0x00000008
#define DCBX_ETS_CBS_OFFSET			3
#define DCBX_ETS_MAX_TCS_MASK			0x000000f0
#define DCBX_ETS_MAX_TCS_OFFSET			4
#define DCBX_OOO_TC_MASK			0x00000f00
#define DCBX_OOO_TC_OFFSET			8
	/* Entries in tc table are orginized that the left most is pri 0, right most is prio 7 */
	u32 pri_tc_tbl[1];
/* Fixed TCP OOO TC usage is deprecated and used only for driver backward compatibility */
#define DCBX_TCP_OOO_TC				(4)
#define DCBX_TCP_OOO_K2_4PORT_TC		(3)

#define NIG_ETS_ISCSI_OOO_CLIENT_OFFSET		(DCBX_TCP_OOO_TC + 1)
#define DCBX_CEE_STRICT_PRIORITY		0xf
	/* Entries in tc table are orginized that the left most is pri 0, right most is prio 7 */
	u32 tc_bw_tbl[2];
	/* Entries in tc table are orginized that the left most is pri 0, right most is prio 7 */
	u32 tc_tsa_tbl[2];
#define DCBX_ETS_TSA_STRICT	0
#define DCBX_ETS_TSA_CBS	1
#define DCBX_ETS_TSA_ETS	2
};

struct dcbx_app_priority_entry {
	u32 entry;
#define DCBX_APP_PRI_MAP_MASK			0x000000ff
#define DCBX_APP_PRI_MAP_OFFSET			0
#define DCBX_APP_PRI_0				0x01
#define DCBX_APP_PRI_1				0x02
#define DCBX_APP_PRI_2				0x04
#define DCBX_APP_PRI_3				0x08
#define DCBX_APP_PRI_4				0x10
#define DCBX_APP_PRI_5				0x20
#define DCBX_APP_PRI_6				0x40
#define DCBX_APP_PRI_7				0x80
#define DCBX_APP_SF_MASK			0x00000300
#define DCBX_APP_SF_OFFSET			8
#define DCBX_APP_SF_ETHTYPE			0
#define DCBX_APP_SF_PORT			1
#define DCBX_APP_SF_IEEE_MASK			0x0000f000
#define DCBX_APP_SF_IEEE_OFFSET			12
#define DCBX_APP_SF_IEEE_RESERVED		0
#define DCBX_APP_SF_IEEE_ETHTYPE		1
#define DCBX_APP_SF_IEEE_TCP_PORT		2
#define DCBX_APP_SF_IEEE_UDP_PORT		3
#define DCBX_APP_SF_IEEE_TCP_UDP_PORT		4

#define DCBX_APP_PROTOCOL_ID_MASK		0xffff0000
#define DCBX_APP_PROTOCOL_ID_OFFSET		16
};

/* FW structure in BE */
struct dcbx_app_priority_feature {
	u32 flags;
#define DCBX_APP_ENABLED_MASK		0x00000001
#define DCBX_APP_ENABLED_OFFSET		0
#define DCBX_APP_WILLING_MASK		0x00000002
#define DCBX_APP_WILLING_OFFSET		1
#define DCBX_APP_ERROR_MASK		0x00000004
#define DCBX_APP_ERROR_OFFSET		2
	/* Not in use
	 * #define DCBX_APP_DEFAULT_PRI_MASK    0x00000f00
	 * #define DCBX_APP_DEFAULT_PRI_OFFSET   8
	 */
#define DCBX_APP_MAX_TCS_MASK		0x0000f000
#define DCBX_APP_MAX_TCS_OFFSET		12
#define DCBX_APP_NUM_ENTRIES_MASK	0x00ff0000
#define DCBX_APP_NUM_ENTRIES_OFFSET	16
	struct dcbx_app_priority_entry app_pri_tbl[DCBX_MAX_APP_PROTOCOL];
};

/* FW structure in BE */
struct dcbx_features {
	/* PG feature */
	struct dcbx_ets_feature ets;
	/* PFC feature */
	u32 pfc;
#define DCBX_PFC_PRI_EN_BITMAP_MASK		0x000000ff
#define DCBX_PFC_PRI_EN_BITMAP_OFFSET		0
#define DCBX_PFC_PRI_EN_BITMAP_PRI_0		0x01
#define DCBX_PFC_PRI_EN_BITMAP_PRI_1		0x02
#define DCBX_PFC_PRI_EN_BITMAP_PRI_2		0x04
#define DCBX_PFC_PRI_EN_BITMAP_PRI_3		0x08
#define DCBX_PFC_PRI_EN_BITMAP_PRI_4		0x10
#define DCBX_PFC_PRI_EN_BITMAP_PRI_5		0x20
#define DCBX_PFC_PRI_EN_BITMAP_PRI_6		0x40
#define DCBX_PFC_PRI_EN_BITMAP_PRI_7		0x80

#define DCBX_PFC_FLAGS_MASK			0x0000ff00
#define DCBX_PFC_FLAGS_OFFSET			8
#define DCBX_PFC_CAPS_MASK			0x00000f00
#define DCBX_PFC_CAPS_OFFSET			8
#define DCBX_PFC_MBC_MASK			0x00004000
#define DCBX_PFC_MBC_OFFSET			14
#define DCBX_PFC_WILLING_MASK			0x00008000
#define DCBX_PFC_WILLING_OFFSET			15
#define DCBX_PFC_ENABLED_MASK			0x00010000
#define DCBX_PFC_ENABLED_OFFSET			16
#define DCBX_PFC_ERROR_MASK			0x00020000
#define DCBX_PFC_ERROR_OFFSET			17

	/* APP feature */
	struct dcbx_app_priority_feature app;
};

struct dcbx_local_params {
	u32 config;
#define DCBX_CONFIG_VERSION_MASK	0x00000007
#define DCBX_CONFIG_VERSION_OFFSET	0
#define DCBX_CONFIG_VERSION_DISABLED	0
#define DCBX_CONFIG_VERSION_IEEE	1
#define DCBX_CONFIG_VERSION_CEE		2
#define DCBX_CONFIG_VERSION_DYNAMIC	(DCBX_CONFIG_VERSION_IEEE | \
	                                 DCBX_CONFIG_VERSION_CEE)
#define DCBX_CONFIG_VERSION_STATIC	4

	u32 flags;
	struct dcbx_features features;
};

struct dcbx_mib {
	u32 prefix_seq_num;
	u32 flags;
	/*
	 * #define DCBX_CONFIG_VERSION_MASK                     0x00000007
	 * #define DCBX_CONFIG_VERSION_OFFSET            0
	 * #define DCBX_CONFIG_VERSION_DISABLED         0
	 * #define DCBX_CONFIG_VERSION_IEEE                     1
	 * #define DCBX_CONFIG_VERSION_CEE                      2
	 * #define DCBX_CONFIG_VERSION_STATIC           4
	 */
	struct dcbx_features features;
	u32 suffix_seq_num;
};

struct lldp_system_tlvs_buffer_s {
	u32 flags;
#define LLDP_SYSTEM_TLV_VALID_MASK		0x1
#define LLDP_SYSTEM_TLV_VALID_OFFSET		0
/* This bit defines if system TLVs are instead of mandatory TLVS or in
 * addition to them. Set 1 for replacing mandatory TLVs
 */
#define LLDP_SYSTEM_TLV_MANDATORY_MASK		0x2
#define LLDP_SYSTEM_TLV_MANDATORY_OFFSET	1
#define LLDP_SYSTEM_TLV_LENGTH_MASK		0xffff0000
#define LLDP_SYSTEM_TLV_LENGTH_OFFSET		16
	u32 data[MAX_SYSTEM_LLDP_TLV_DATA];
};

/* Since this struct is written by MFW and read by driver need to add
 * sequence guards (as in case of DCBX MIB)
 */
struct lldp_received_tlvs_s {
	u32 prefix_seq_num;
	u32 length;
	u32 tlvs_buffer[MAX_TLV_BUFFER];
	u32 suffix_seq_num;
};

struct dcb_dscp_map {
	u32 flags;
#define DCB_DSCP_ENABLE_MASK		0x1
#define DCB_DSCP_ENABLE_OFFSET		0
#define DCB_DSCP_ENABLE			1
	u32 dscp_pri_map[8];
	/* the map structure is the following:
	 * each u32 is split into 4 bits chunks, each chunk holds priority for respective dscp
	 * Lowest dscp is at lsb
	 *                      31              28              24              20              16              12              8               4               0
	 * dscp_pri_map[0]: | dscp7 pri | dscp6 pri | dscp5 pri | dscp4 pri | dscp3 pri | dscp2 pri | dscp1 pri | dscp0 pri |
	 * dscp_pri_map[1]: | dscp15 pri| dscp14 pri| dscp13 pri| dscp12 pri| dscp11 pri| dscp10 pri| dscp9 pri | dscp8 pri |
	 * etc.*/
};

struct mcp_val64 {
	u32 lo;
	u32 hi;
};

/* struct generic_idc_msg to be used for inter driver communication.
 * source_pf specifies the originating PF that sent messages to all target PFs
 * msg contains 64 bit value of the message - opaque to the MFW
 */
struct generic_idc_msg_s {
	u32 source_pf;
	struct mcp_val64 msg;
};

/**************************************
*     PCIE Statistics structures
**************************************/
struct pcie_stats_stc {
	u32 sr_cnt_wr_byte_msb;
	u32 sr_cnt_wr_byte_lsb;
	u32 sr_cnt_wr_cnt;
	u32 sr_cnt_rd_byte_msb;
	u32 sr_cnt_rd_byte_lsb;
	u32 sr_cnt_rd_cnt;
};

/**************************************
*     Attributes commands
**************************************/

enum _attribute_commands_e {
	ATTRIBUTE_CMD_READ = 0,
	ATTRIBUTE_CMD_WRITE,
	ATTRIBUTE_CMD_READ_CLEAR,
	ATTRIBUTE_CMD_CLEAR,
	ATTRIBUTE_NUM_OF_COMMANDS
};

/**************************************/
/*                                                                */
/*     P U B L I C      G L O B A L   */
/*                                                                */
/**************************************/
struct public_global {
	u32 max_path;		/* 0x0 32bit is wasty, but this will be used often */
	u32 max_ports;		/* 0x4 (Global) 32bit is wasty, but this will be used often */
#define MODE_1P		1	/* TBD - NEED TO THINK OF A BETTER NAME */
#define MODE_2P		2
#define MODE_3P		3
#define MODE_4P		4
	u32 debug_mb_offset;	/* 0x8 */
	u32 phymod_dbg_mb_offset;	/* 0xc */
	struct couple_mode_teaming cmt;	/* 0x10 */
	s32 internal_temperature;	/* 0x14 Temperature in Celcius (-255C / +255C), measured every second. */
	u32 mfw_ver;		/* 0x18 */
	u32 running_bundle_id;	/* 0x1c */
	s32 external_temperature;	/* 0x20 */
	u32 mdump_reason;	/* 0x24 */
#define MDUMP_REASON_INTERNAL_ERROR			(1 << 0)
#define MDUMP_REASON_EXTERNAL_TRIGGER			(1 << 1)
#define MDUMP_REASON_DUMP_AGED				(1 << 2)
	u32 ext_phy_upgrade_fw;	/* 0x28 */
#define EXT_PHY_FW_UPGRADE_STATUS_MASK			(0x0000ffff)
#define EXT_PHY_FW_UPGRADE_STATUS_OFFSET		(0)
#define EXT_PHY_FW_UPGRADE_STATUS_IN_PROGRESS		(1)
#define EXT_PHY_FW_UPGRADE_STATUS_FAILED		(2)
#define EXT_PHY_FW_UPGRADE_STATUS_SUCCESS		(3)
#define EXT_PHY_FW_UPGRADE_TYPE_MASK			(0xffff0000)
#define EXT_PHY_FW_UPGRADE_TYPE_OFFSET			(16)

	u8 runtime_port_swap_map[MODE_4P];	/* 0x2c */
	u32 data_ptr;		/* 0x30 */
	u32 data_size;		/* 0x34 */
	u32 bmb_error_status_cnt;	/* 0x38 */
	u32 bmb_jumbo_frame_cnt;	/* 0x3c */
	u32 sent_to_bmc_cnt;	/* 0x40 */
	u32 handled_by_mfw;	/* 0x44 */
	u32 sent_to_nw_cnt;	/* 0x48 BMC BC/MC are handled by MFW */
#define LEAKY_BUCKET_SAMPLES    10
	u32 to_bmc_kb_per_second;	/* 0x4c Used to be sent_to_nw_cnt */
	u32 bcast_dropped_to_bmc_cnt;	/* 0x50 */
	u32 mcast_dropped_to_bmc_cnt;	/* 0x54 */
	u32 ucast_dropped_to_bmc_cnt;	/* 0x58 */
	u32 ncsi_response_failure_cnt;	/* 0x5c */
	u32 device_attr;	/* 0x60 */
	u32 vpd_warning;	/* 0x64 */
#define VPD_WARNING_ID_MASK				0x000000ff
#define VPD_WARNING_ID_OFFSET				0
#define VPD_WARNING_NONE				0
#define VPD_WARNING_INVALID_PRODUCT_NAME_LENGTH		1
#define VPD_WARNING_INVALID_LR_STRING			2
#define VPD_WARNING_INVALID_LR_VPD_R			3
#define VPD_WARNING_INVALID_VPD_R_SIZE			4
#define VPD_WARNING_TOO_MANY_TAGS			5
#define VPD_WARNING_INVALID_TAG_NAME			6
#define VPD_WARNING_DUP_TAG_DETECTED			7
#define VPD_INVALID_CRC					8
#define VPD_WARNING_NO_ROOM_FOR_TAG			9
#define VPD_WARNING_FILE_TOO_BIG			10
#define VPD_WARNING_NO_MEMORY				11
#define VPD_WARNING_NVM_RD_ERROR			12
#define VPD_WARNING_TABLE_TOO_SMALL			13
#define VPD_WARNING_INVALID_LEN				14
#define VPD_WARNING_INVALID_TAG_LEN			15
#define VPD_WARNING_INVALID_IMAGE			16
#define VPD_WARNING_NO_ROOM_FOR_NEW_TAG_DATA		17
#define VPD_WARNING_NO_PLACE_FOR_NEW_TAG		18
#define VPD_WARNING_SKIPPING_VPD_V0_UPDATE		19
#define VPD_WARNING_VAL_MASK				0xffffff00
#define VPD_WARNING_VAL_OFFSET				8
};

/**************************************/
/*                                                                */
/*     P U B L I C      P A T H           */
/*                                                                */
/**************************************/

/****************************************************************************
* Shared Memory 2 Region                                                                                                *
****************************************************************************/
/* The fw_flr_ack is actually built in the following way:                               */
/* 8 bit:  PF ack                                                                                                               */
/* 128 bit: VF ack                                                                                                               */
/* 8 bit:  ios_dis_ack                                                                                                          */
/* In order to maintain endianity in the mailbox hsi, we want to keep using */
/* u32. The fw must have the VF right after the PF since this is how it         */
/* access arrays(it expects always the VF to reside after the PF, and that  */
/* makes the calculation much easier for it. )                                                          */
/* In order to answer both limitations, and keep the struct small, the code */
/* will abuse the structure defined here to achieve the actual partition	*/
/* above																	*/
/****************************************************************************/
struct fw_flr_mb {
	u32 aggint;
	u32 opgen_addr;
	u32 accum_ack;		/* 0..15:PF, 16..207:VF, 256..271:IOV_DIS */
#define ACCUM_ACK_PF_BASE		0
#define ACCUM_ACK_PF_SHIFT		0

#define ACCUM_ACK_VF_BASE		8
#define ACCUM_ACK_VF_SHIFT		3

#define ACCUM_ACK_IOV_DIS_BASE		256
#define ACCUM_ACK_IOV_DIS_SHIFT		8
};

struct public_path {
	struct fw_flr_mb flr_mb;
	/*
	 * mcp_vf_disabled is set by the MCP to indicate the driver about VFs
	 * which were disabled/flred
	 */
	u32 mcp_vf_disabled[VF_MAX_STATIC / 32];	/* 0x003c */

	u32 process_kill;	/* Reset on mcp reset, and incremented for eveny process kill event. */
#define PROCESS_KILL_COUNTER_MASK		0x0000ffff
#define PROCESS_KILL_COUNTER_OFFSET		0
#define PROCESS_KILL_GLOB_AEU_BIT_MASK		0xffff0000
#define PROCESS_KILL_GLOB_AEU_BIT_OFFSET	16
#define GLOBAL_AEU_BIT(aeu_reg_id, aeu_bit)    (aeu_reg_id * 32 + aeu_bit)
};

/**************************************/
/*                                                                */
/*     P U B L I C      P O R T           */
/*                                                                */
/**************************************/
#define FC_NPIV_WWPN_SIZE	8
#define FC_NPIV_WWNN_SIZE	8
struct dci_npiv_settings {
	u8 npiv_wwpn[FC_NPIV_WWPN_SIZE];
	u8 npiv_wwnn[FC_NPIV_WWNN_SIZE];
};

struct dci_fc_npiv_cfg {
	/* hdr used internally by the MFW */
	u32 hdr;
	u32 num_of_npiv;
};

#define MAX_NUMBER_NPIV    64
struct dci_fc_npiv_tbl {
	struct dci_fc_npiv_cfg fc_npiv_cfg;
	struct dci_npiv_settings settings[MAX_NUMBER_NPIV];
};

struct pause_flood_monitor {
/* Arbitrary number greater than 0, and lower than the BRB threshold */
#define PAUSE_FLOOD_BRB_BLOCK_FULL_THRESHOLD	100
	u8 period_cnt;
#define PAUSE_FLOOD_MONITOR_TIMEOUT		2
/* Stores bit per previous sample (up to 8 samples, currently set to 2) */
#define ANY_BRB_PRS_PACKET_MASK			0x3
	u8 any_brb_prs_packet_hist;

/* Stores bit per previous sample (up to 8 samples, currently set to 3) */
#define ANY_BRB_BLOCK_FULL_MASK			0x7
	u8 any_brb_block_is_full_hist;

#define PAUSE_FLOOD_DETECTED			(1 << 0)
	u8 flags;

/* Stores cumulative number of Pause Flood state changes. */
	u32 num_of_state_changes;
};

/****************************************************************************
* Driver <-> FW Mailbox													*
****************************************************************************/

struct public_port {
	u32 validity_map;	/* 0x0 (0x4) */

	/* validity bits */
#define MCP_VALIDITY_PCI_CFG				0x00100000
#define MCP_VALIDITY_MB					0x00200000
#define MCP_VALIDITY_DEV_INFO				0x00400000
#define MCP_VALIDITY_RESERVED				0x00000007

	/* One licensing bit should be set */
#define MCP_VALIDITY_LIC_KEY_IN_EFFECT_MASK		0x00000038	/* yaniv - tbd ? license */
#define MCP_VALIDITY_LIC_MANUF_KEY_IN_EFFECT		0x00000008
#define MCP_VALIDITY_LIC_UPGRADE_KEY_IN_EFFECT		0x00000010
#define MCP_VALIDITY_LIC_NO_KEY_IN_EFFECT		0x00000020

	/* Active MFW */
#define MCP_VALIDITY_ACTIVE_MFW_UNKNOWN			0x00000000
#define MCP_VALIDITY_ACTIVE_MFW_MASK			0x000001c0
#define MCP_VALIDITY_ACTIVE_MFW_NCSI			0x00000040
#define MCP_VALIDITY_ACTIVE_MFW_NONE			0x000001c0

	u32 link_status;	/* 0x4 (0x4) */
#define LINK_STATUS_LINK_UP				0x00000001
#define LINK_STATUS_SPEED_AND_DUPLEX_MASK		0x0000001e
#define LINK_STATUS_SPEED_AND_DUPLEX_1000THD		(1 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_1000TFD		(2 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_10G		(3 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_20G		(4 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_40G		(5 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_50G		(6 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_100G		(7 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_25G		(8 << 1)
#define LINK_STATUS_AUTO_NEGOTIATE_ENABLED		0x00000020
#define LINK_STATUS_AUTO_NEGOTIATE_COMPLETE		0x00000040
#define LINK_STATUS_PARALLEL_DETECTION_USED		0x00000080
#define LINK_STATUS_PFC_ENABLED				0x00000100
#define LINK_STATUS_LINK_PARTNER_1000TFD_CAPABLE	0x00000200
#define LINK_STATUS_LINK_PARTNER_1000THD_CAPABLE	0x00000400
#define LINK_STATUS_LINK_PARTNER_10G_CAPABLE		0x00000800
#define LINK_STATUS_LINK_PARTNER_20G_CAPABLE		0x00001000
#define LINK_STATUS_LINK_PARTNER_40G_CAPABLE		0x00002000
#define LINK_STATUS_LINK_PARTNER_50G_CAPABLE		0x00004000
#define LINK_STATUS_LINK_PARTNER_100G_CAPABLE		0x00008000
#define LINK_STATUS_LINK_PARTNER_25G_CAPABLE		0x00010000
#define LINK_STATUS_LINK_PARTNER_FLOW_CONTROL_MASK	0x000C0000
#define LINK_STATUS_LINK_PARTNER_NOT_PAUSE_CAPABLE	(0 << 18)
#define LINK_STATUS_LINK_PARTNER_SYMMETRIC_PAUSE	(1 << 18)
#define LINK_STATUS_LINK_PARTNER_ASYMMETRIC_PAUSE	(2 << 18)
#define LINK_STATUS_LINK_PARTNER_BOTH_PAUSE		(3 << 18)
#define LINK_STATUS_SFP_TX_FAULT			0x00100000
#define LINK_STATUS_TX_FLOW_CONTROL_ENABLED		0x00200000
#define LINK_STATUS_RX_FLOW_CONTROL_ENABLED		0x00400000
#define LINK_STATUS_RX_SIGNAL_PRESENT			0x00800000
#define LINK_STATUS_MAC_LOCAL_FAULT			0x01000000
#define LINK_STATUS_MAC_REMOTE_FAULT			0x02000000
#define LINK_STATUS_UNSUPPORTED_SPD_REQ			0x04000000
#define LINK_STATUS_FEC_MODE_MASK			0x38000000
#define LINK_STATUS_FEC_MODE_NONE			(0 << 27)
#define LINK_STATUS_FEC_MODE_FIRECODE_CL74		(1 << 27)
#define LINK_STATUS_FEC_MODE_RS_CL91			(2 << 27)
#define LINK_STATUS_EXT_PHY_LINK_UP			0x40000000

	u32 link_status1;	/* 0x8 (0x4) */
#define LP_PRESENCE_STATUS_OFFSET			0
#define LP_PRESENCE_STATUS_MASK				0x3
#define LP_PRESENCE_UNKNOWN				0x0
#define LP_PRESENCE_PROBING				0x1
#define	LP_PRESENT					0x2
#define	LP_NOT_PRESENT					0x3
#define TXFAULT_STATUS_OFFSET				2
#define TXFAULT_STATUS_MASK				(0x1 << \
	                                                 TXFAULT_STATUS_OFFSET)
#define TXFAULT_NOT_PRESENT				(0x0 << \
	                                                 TXFAULT_STATUS_OFFSET)
#define TXFAULT_PRESENT					(0x1 << \
	                                                 TXFAULT_STATUS_OFFSET)
#define RXLOS_STATUS_OFFSET				3
#define RXLOS_STATUS_MASK				(0x1 << \
	                                                 RXLOS_STATUS_OFFSET)
#define RXLOS_NOT_PRESENT				(0x0 << \
	                                                 RXLOS_STATUS_OFFSET)
#define RXLOS_PRESENT					(0x1 << \
	                                                 RXLOS_STATUS_OFFSET)
#define PORT_NOT_READY					(1 << 4)	/* Port init is not completed or failed */
	u32 ext_phy_fw_version;	/* 0xc (0x4) */
	u32 drv_phy_cfg_addr;	/* Points to struct eth_phy_cfg (For READ-ONLY) *//* 0x10 (0x4) */

	u32 port_stx;		/* 0x14 (0x4) */

	u32 stat_nig_timer;	/* 0x18 (0x4) */

	struct port_mf_cfg port_mf_config;	/* 0x1c (0xc) */
	struct port_stats stats;	/* 0x28 (0x1e8) 64-bit aligned */

	u32 media_type;		/* 0x210 (0x4) */
#define	MEDIA_UNSPECIFIED			0x0
#define	MEDIA_SFPP_10G_FIBER			0x1	/* Use MEDIA_MODULE_FIBER instead */
#define	MEDIA_XFP_FIBER				0x2	/* Use MEDIA_MODULE_FIBER instead */
#define	MEDIA_DA_TWINAX				0x3
#define	MEDIA_BASE_T				0x4
#define MEDIA_SFP_1G_FIBER			0x5	/* Use MEDIA_MODULE_FIBER instead */
#define MEDIA_MODULE_FIBER			0x6
#define	MEDIA_KR				0xf0
#define	MEDIA_NOT_PRESENT			0xff

	u32 lfa_status;		/* 0x214 (0x4) */
#define LFA_LINK_FLAP_REASON_OFFSET		0
#define LFA_LINK_FLAP_REASON_MASK		0x000000ff
#define LFA_NO_REASON				(0 << 0)
#define LFA_LINK_DOWN				(1 << 0)
#define LFA_FORCE_INIT				(1 << 1)
#define LFA_LOOPBACK_MISMATCH			(1 << 2)
#define LFA_SPEED_MISMATCH			(1 << 3)
#define LFA_FLOW_CTRL_MISMATCH			(1 << 4)
#define LFA_ADV_SPEED_MISMATCH			(1 << 5)
#define LFA_EEE_MISMATCH			(1 << 6)
#define LFA_LINK_MODES_MISMATCH			(1 << 7)
#define LINK_FLAP_AVOIDANCE_COUNT_OFFSET	8
#define LINK_FLAP_AVOIDANCE_COUNT_MASK		0x0000ff00
#define LINK_FLAP_COUNT_OFFSET			16
#define LINK_FLAP_COUNT_MASK			0x00ff0000
#define LFA_LINK_FLAP_EXT_REASON_OFFSET		24
#define LFA_LINK_FLAP_EXT_REASON_MASK		0xff000000
#define LFA_EXT_SPEED_MISMATCH			(1 << 24)
#define LFA_EXT_ADV_SPEED_MISMATCH		(1 << 25)
#define LFA_EXT_FEC_MISMATCH			(1 << 26)
	u32 link_change_count;	/* 0x218 (0x4) */

	/* LLDP params */
	struct lldp_config_params_s lldp_config_params[LLDP_MAX_LLDP_AGENTS];	// offset: 536 bytes?
	struct lldp_status_params_s lldp_status_params[LLDP_MAX_LLDP_AGENTS];
	struct lldp_system_tlvs_buffer_s system_lldp_tlvs_buf;

	/* DCBX related MIB */
	struct dcbx_local_params local_admin_dcbx_mib;
	struct dcbx_mib remote_dcbx_mib;
	struct dcbx_mib operational_dcbx_mib;

	/* FC_NPIV table offset & size in NVRAM value of 0 means not present */
	u32 fc_npiv_nvram_tbl_addr;
#define NPIV_TBL_INVALID_ADDR				0xFFFFFFFF

	u32 fc_npiv_nvram_tbl_size;
	u32 transceiver_data;
#define ETH_TRANSCEIVER_STATE_MASK			0x000000FF
#define ETH_TRANSCEIVER_STATE_OFFSET			0x0
#define ETH_TRANSCEIVER_STATE_UNPLUGGED			0x00
#define ETH_TRANSCEIVER_STATE_PRESENT			0x01
#define ETH_TRANSCEIVER_STATE_VALID			0x03
#define ETH_TRANSCEIVER_STATE_UPDATING			0x08
#define ETH_TRANSCEIVER_STATE_IN_SETUP			0x10
#define ETH_TRANSCEIVER_TYPE_MASK			0x0000FF00
#define ETH_TRANSCEIVER_TYPE_OFFSET			0x8
#define ETH_TRANSCEIVER_TYPE_NONE			0x00
#define ETH_TRANSCEIVER_TYPE_UNKNOWN			0xFF
#define ETH_TRANSCEIVER_TYPE_1G_PCC			0x01	/* 1G Passive copper cable */
#define ETH_TRANSCEIVER_TYPE_1G_ACC			0x02	/* 1G Active copper cable  */
#define ETH_TRANSCEIVER_TYPE_1G_LX			0x03
#define ETH_TRANSCEIVER_TYPE_1G_SX			0x04
#define ETH_TRANSCEIVER_TYPE_10G_SR			0x05
#define ETH_TRANSCEIVER_TYPE_10G_LR			0x06
#define ETH_TRANSCEIVER_TYPE_10G_LRM			0x07
#define ETH_TRANSCEIVER_TYPE_10G_ER			0x08
#define ETH_TRANSCEIVER_TYPE_10G_PCC			0x09	/* 10G Passive copper cable */
#define ETH_TRANSCEIVER_TYPE_10G_ACC			0x0a	/* 10G Active copper cable  */
#define ETH_TRANSCEIVER_TYPE_XLPPI			0x0b
#define ETH_TRANSCEIVER_TYPE_40G_LR4			0x0c
#define ETH_TRANSCEIVER_TYPE_40G_SR4			0x0d
#define ETH_TRANSCEIVER_TYPE_40G_CR4			0x0e
#define ETH_TRANSCEIVER_TYPE_100G_AOC			0x0f	/* Active optical cable */
#define ETH_TRANSCEIVER_TYPE_100G_SR4			0x10
#define ETH_TRANSCEIVER_TYPE_100G_LR4			0x11
#define ETH_TRANSCEIVER_TYPE_100G_ER4			0x12
#define ETH_TRANSCEIVER_TYPE_100G_ACC			0x13	/* Active copper cable */
#define ETH_TRANSCEIVER_TYPE_100G_CR4			0x14
#define ETH_TRANSCEIVER_TYPE_4x10G_SR			0x15
#define ETH_TRANSCEIVER_TYPE_25G_CA_N			0x16	/* 25G Passive copper cable - short */
#define ETH_TRANSCEIVER_TYPE_25G_ACC_S			0x17	/* 25G Active copper cable  - short */
#define ETH_TRANSCEIVER_TYPE_25G_CA_S			0x18	/* 25G Passive copper cable - medium */
#define ETH_TRANSCEIVER_TYPE_25G_ACC_M			0x19	/* 25G Active copper cable  - medium */
#define ETH_TRANSCEIVER_TYPE_25G_CA_L			0x1a	/* 25G Passive copper cable - long */
#define ETH_TRANSCEIVER_TYPE_25G_ACC_L			0x1b	/* 25G Active copper cable  - long */
#define ETH_TRANSCEIVER_TYPE_25G_SR			0x1c
#define ETH_TRANSCEIVER_TYPE_25G_LR			0x1d
#define ETH_TRANSCEIVER_TYPE_25G_AOC			0x1e
#define ETH_TRANSCEIVER_TYPE_4x10G			0x1f
#define ETH_TRANSCEIVER_TYPE_4x25G_CR			0x20
#define ETH_TRANSCEIVER_TYPE_1000BASET			0x21
#define ETH_TRANSCEIVER_TYPE_10G_BASET			0x22
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_SR	0x30
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_CR	0x31
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_LR	0x32
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_SR	0x33
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_CR	0x34
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_LR	0x35
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_AOC	0x36
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_25G_SR	0x37
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_25G_LR	0x38
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_1G_10G_SR	0x39
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_1G_10G_LR	0x3a
#define ETH_TRANSCEIVER_TYPE_25G_ER			0x3b
#define ETH_TRANSCEIVER_TYPE_50G_CR			0x3c
#define ETH_TRANSCEIVER_TYPE_50G_SR			0x3d
#define ETH_TRANSCEIVER_TYPE_50G_LR			0x3e
#define ETH_TRANSCEIVER_TYPE_50G_FR			0x3f
#define ETH_TRANSCEIVER_TYPE_50G_R1_AOC			0x40
#define ETH_TRANSCEIVER_TYPE_50G_R1_ACC_BER_1E6		0x41
#define ETH_TRANSCEIVER_TYPE_50G_R1_ACC_BER_1E4		0x42
#define ETH_TRANSCEIVER_TYPE_50G_R1_AOC_BER_1E6		0x43
#define ETH_TRANSCEIVER_TYPE_50G_R1_AOC_BER_1E4		0x44
#define ETH_TRANSCEIVER_TYPE_100G_CR2			0x45
#define ETH_TRANSCEIVER_TYPE_100G_SR2			0x46
#define ETH_TRANSCEIVER_TYPE_100G_R2_AOC		0x47
#define ETH_TRANSCEIVER_TYPE_100G_R2_ACC_BER_1E6	0x48
#define ETH_TRANSCEIVER_TYPE_100G_R2_ACC_BER_1E4	0x49
#define ETH_TRANSCEIVER_TYPE_100G_R2_AOC_BER_1E6	0x4a
#define ETH_TRANSCEIVER_TYPE_100G_R2_AOC_BER_1E4	0x4b
#define ETH_TRANSCEIVER_TYPE_200G_CR4			0x4c
#define ETH_TRANSCEIVER_TYPE_200G_SR4			0x4d
#define ETH_TRANSCEIVER_TYPE_200G_LR4			0x4e
#define ETH_TRANSCEIVER_TYPE_200G_FR4			0x4f
#define ETH_TRANSCEIVER_TYPE_200G_DR4			0x50
#define ETH_TRANSCEIVER_TYPE_200G_R4_AOC		0x51
#define ETH_TRANSCEIVER_TYPE_200G_R4_ACC_BER_1E6	0x52
#define ETH_TRANSCEIVER_TYPE_200G_R4_ACC_BER_1E4	0x53
#define ETH_TRANSCEIVER_TYPE_200G_R4_AOC_BER_1E6	0x54
#define ETH_TRANSCEIVER_TYPE_200G_R4_AOC_BER_1E4	0x55
/* COULD ALSO ADD...  BUT FOR NOW UNSUPPORTED */
#define ETH_TRANSCEIVER_TYPE_5G_BASET			0x56
#define ETH_TRANSCEIVER_TYPE_2p5G_BASET			0x57
#define ETH_TRANSCEIVER_TYPE_10G_BASET_30M		0x58
#define ETH_TRANSCEIVER_TYPE_25G_BASET			0x59
#define ETH_TRANSCEIVER_TYPE_40G_BASET			0x5a
#define ETH_TRANSCEIVER_TYPE_40G_ER4			0x5b
#define ETH_TRANSCEIVER_TYPE_40G_PSM4			0x5c
#define ETH_TRANSCEIVER_TYPE_40G_SWDM4			0x5d
#define ETH_TRANSCEIVER_TYPE_50G_BASET			0x5e
#define ETH_TRANSCEIVER_TYPE_100G_SR10			0x5f
#define ETH_TRANSCEIVER_TYPE_100G_CDWM4			0x60
#define ETH_TRANSCEIVER_TYPE_100G_DWDM2			0x61
#define ETH_TRANSCEIVER_TYPE_100G_PSM4			0x62
#define ETH_TRANSCEIVER_TYPE_100G_CLR4			0x63
#define ETH_TRANSCEIVER_TYPE_100G_WDM			0x64
#define ETH_TRANSCEIVER_TYPE_100G_SWDM4			0x65	/* Supported */
#define ETH_TRANSCEIVER_TYPE_100G_4WDM_10		0x66
#define ETH_TRANSCEIVER_TYPE_100G_4WDM_20		0x67
#define ETH_TRANSCEIVER_TYPE_100G_4WDM_40		0x68
#define ETH_TRANSCEIVER_TYPE_100G_DR_CAUI4		0x69
#define ETH_TRANSCEIVER_TYPE_100G_FR_CAUI4		0x6a
#define ETH_TRANSCEIVER_TYPE_100G_LR_CAUI4		0x6b
#define ETH_TRANSCEIVER_TYPE_200G_PSM4			0x6c

	u32 wol_info;
	u32 wol_pkt_len;
	u32 wol_pkt_details;
	struct dcb_dscp_map dcb_dscp_map;

	u32 eee_status;
#define EEE_ACTIVE_BIT					(1 << 0)	/* Set when EEE negotiation is complete. */

#define EEE_LD_ADV_STATUS_MASK				0x000000f0	/* Shows the Local Device EEE capabilities */
#define EEE_LD_ADV_STATUS_OFFSET			4
#define EEE_1G_ADV					(1 << 1)
#define EEE_10G_ADV					(1 << 2)
#define	EEE_LP_ADV_STATUS_MASK				0x00000f00	/* Same values as in EEE_LD_ADV, but for Link Parter */
#define EEE_LP_ADV_STATUS_OFFSET			8

#define EEE_SUPPORTED_SPEED_MASK			0x0000f000	/* Supported speeds for EEE */
#define EEE_SUPPORTED_SPEED_OFFSET			12
#define EEE_1G_SUPPORTED				(1 << 1)
#define EEE_10G_SUPPORTED				(1 << 2)

	u32 eee_remote;		/* Used for EEE in LLDP */
#define EEE_REMOTE_TW_TX_MASK				0x0000ffff
#define EEE_REMOTE_TW_TX_OFFSET				0
#define EEE_REMOTE_TW_RX_MASK				0xffff0000
#define EEE_REMOTE_TW_RX_OFFSET				16

	u32 module_info;
#define ETH_TRANSCEIVER_MONITORING_TYPE_MASK		0x000000FF
#define ETH_TRANSCEIVER_MONITORING_TYPE_OFFSET		0
#define ETH_TRANSCEIVER_ADDR_CHNG_REQUIRED		(1 << 2)
#define ETH_TRANSCEIVER_RCV_PWR_MEASURE_TYPE		(1 << 3)
#define ETH_TRANSCEIVER_EXTERNALLY_CALIBRATED		(1 << 4)
#define ETH_TRANSCEIVER_INTERNALLY_CALIBRATED		(1 << 5)
#define ETH_TRANSCEIVER_HAS_DIAGNOSTIC			(1 << 6)
#define ETH_TRANSCEIVER_IDENT_MASK			0x0000ff00
#define ETH_TRANSCEIVER_IDENT_OFFSET			8
#define ETH_TRANSCEIVER_ENHANCED_OPTIONS_MASK		0x00ff0000
#define ETH_TRANSCEIVER_ENHANCED_OPTIONS_OFFSET		16
#define ETH_TRANSCEIVER_HAS_TXFAULT			(1 << 5)
#define ETH_TRANSCEIVER_HAS_RXLOS			(1 << 4)

	u32 oem_cfg_port;
#define OEM_CFG_CHANNEL_TYPE_MASK			0x00000003
#define OEM_CFG_CHANNEL_TYPE_OFFSET			0
#define OEM_CFG_CHANNEL_TYPE_VLAN_PARTITION		0x1
#define OEM_CFG_CHANNEL_TYPE_STAGGED			0x2

#define OEM_CFG_SCHED_TYPE_MASK				0x0000000C
#define OEM_CFG_SCHED_TYPE_OFFSET			2
#define OEM_CFG_SCHED_TYPE_ETS				0x1
#define OEM_CFG_SCHED_TYPE_VNIC_BW			0x2

	struct lldp_received_tlvs_s lldp_received_tlvs[LLDP_MAX_LLDP_AGENTS];
	u32 system_lldp_tlvs_buf2[MAX_SYSTEM_LLDP_TLV_DATA];
	u32 phy_module_temperature;	/* Temperature of transceiver / external PHY - 0xc80 (0x4) */
	u32 nig_reg_stat_rx_bmb_packet;
	u32 nig_reg_rx_llh_ncsi_mcp_mask;
	u32 nig_reg_rx_llh_ncsi_mcp_mask_2;
	struct pause_flood_monitor pause_flood_monitor;
	u32 nig_drain_cnt;
	struct pkt_type_cnt pkt_tc_priority_cnt;
};
#define MCP_DRV_VER_STR_SIZE		16
#define MCP_DRV_VER_STR_SIZE_DWORD	(MCP_DRV_VER_STR_SIZE / sizeof(u32))
#define MCP_DRV_NVM_BUF_LEN		32
struct drv_version_stc {
	u32 version;
	u8 name[MCP_DRV_VER_STR_SIZE - 4];
};
/**************************************/
/*                                                                */
/*     P U B L I C      F U N C           */
/*                                                                */
/**************************************/

struct public_func {
	u32 iscsi_boot_signature;
	u32 iscsi_boot_block_offset;

	/* MTU size per funciton is needed for the OV feature */
	u32 mtu_size;
	/* 9 entires for the C2S PCP map for each inner VLAN PCP + 1 default */
	/* For PCP values 0-3 use the map lower */
	/* 0xFF000000 - PCP 0, 0x00FF0000 - PCP 1,
	 * 0x0000FF00 - PCP 2, 0x000000FF PCP 3
	 */
	u32 c2s_pcp_map_lower;
	/* For PCP values 4-7 use the map upper */
	/* 0xFF000000 - PCP 4, 0x00FF0000 - PCP 5,
	 * 0x0000FF00 - PCP 6, 0x000000FF PCP 7
	 */
	u32 c2s_pcp_map_upper;

	/* For PCP default value get the MSB byte of the map default */
	u32 c2s_pcp_map_default;

	/* For generic inter driver communication channel messages between PFs via MFW */
	struct generic_idc_msg_s generic_idc_msg;

	u32 num_of_msix;

	// replace old mf_cfg
	u32 config;
	/* E/R/I/D */
	/* function 0 of each port cannot be hidden */
#define FUNC_MF_CFG_FUNC_HIDE				0x00000001
#define FUNC_MF_CFG_PAUSE_ON_HOST_RING			0x00000002
#define FUNC_MF_CFG_PAUSE_ON_HOST_RING_OFFSET		0x00000001

#define FUNC_MF_CFG_PROTOCOL_MASK			0x000000f0
#define FUNC_MF_CFG_PROTOCOL_OFFSET			4
#define FUNC_MF_CFG_PROTOCOL_ETHERNET			0x00000000
#define FUNC_MF_CFG_PROTOCOL_ISCSI			0x00000010
#define FUNC_MF_CFG_PROTOCOL_FCOE			0x00000020
#define FUNC_MF_CFG_PROTOCOL_ROCE			0x00000030
#define FUNC_MF_CFG_PROTOCOL_MAX			0x00000030

	/* MINBW, MAXBW */
	/* value range - 0..100, increments in 1 %  */
#define FUNC_MF_CFG_MIN_BW_MASK				0x0000ff00
#define FUNC_MF_CFG_MIN_BW_OFFSET			8
#define FUNC_MF_CFG_MIN_BW_DEFAULT			0x00000000
#define FUNC_MF_CFG_MAX_BW_MASK				0x00ff0000
#define FUNC_MF_CFG_MAX_BW_OFFSET			16
#define FUNC_MF_CFG_MAX_BW_DEFAULT			0x00640000

	/*RDMA PROTOCL */
#define FUNC_MF_CFG_RDMA_PROTOCOL_MASK			0x03000000
#define FUNC_MF_CFG_RDMA_PROTOCOL_OFFSET		24
#define FUNC_MF_CFG_RDMA_PROTOCOL_NONE			0x00000000
#define FUNC_MF_CFG_RDMA_PROTOCOL_ROCE			0x01000000
#define FUNC_MF_CFG_RDMA_PROTOCOL_IWARP			0x02000000
	/*for future support */
#define FUNC_MF_CFG_RDMA_PROTOCOL_BOTH			0x03000000

#define FUNC_MF_CFG_BOOT_MODE_MASK			0x0C000000
#define FUNC_MF_CFG_BOOT_MODE_OFFSET			26
#define FUNC_MF_CFG_BOOT_MODE_BIOS_CTRL			0x00000000
#define FUNC_MF_CFG_BOOT_MODE_DISABLED			0x04000000
#define FUNC_MF_CFG_BOOT_MODE_ENABLED			0x08000000

	u32 status;
#define FUNC_STATUS_VIRTUAL_LINK_UP			0x00000001
#define FUNC_STATUS_LOGICAL_LINK_UP			0x00000002
#define FUNC_STATUS_FORCED_LINK				0x00000004

	u32 mac_upper;		/* MAC */
#define FUNC_MF_CFG_UPPERMAC_MASK			0x0000ffff
#define FUNC_MF_CFG_UPPERMAC_OFFSET			0
#define FUNC_MF_CFG_UPPERMAC_DEFAULT \
	                                                FUNC_MF_CFG_UPPERMAC_MASK
	u32 mac_lower;
#define FUNC_MF_CFG_LOWERMAC_DEFAULT			0xffffffff

	u32 fcoe_wwn_port_name_upper;
	u32 fcoe_wwn_port_name_lower;

	u32 fcoe_wwn_node_name_upper;
	u32 fcoe_wwn_node_name_lower;

	u32 ovlan_stag;		/* tags */
#define FUNC_MF_CFG_OV_STAG_MASK	0x0000ffff
#define FUNC_MF_CFG_OV_STAG_OFFSET	0
#define FUNC_MF_CFG_OV_STAG_DEFAULT	FUNC_MF_CFG_OV_STAG_MASK

	u32 pf_allocation;	/* vf per pf */

	u32 preserve_data;	/* Will be used bt CCM */

	u32 driver_last_activity_ts;

	/*
	 * drv_ack_vf_disabled is set by the PF driver to ack handled disabled
	 * VFs
	 */
	/*Not in use anymore */
	u32 drv_ack_vf_disabled[VF_MAX_STATIC / 32];	/* 0x0044 */

	u32 drv_id;
#define DRV_ID_PDA_COMP_VER_MASK		0x0000ffff
#define DRV_ID_PDA_COMP_VER_OFFSET		0

#define LOAD_REQ_HSI_VERSION			2
#define DRV_ID_MCP_HSI_VER_MASK			0x00ff0000
#define DRV_ID_MCP_HSI_VER_OFFSET		16
#define DRV_ID_MCP_HSI_VER_CURRENT		(LOAD_REQ_HSI_VERSION << \
	                                         DRV_ID_MCP_HSI_VER_OFFSET)

#define DRV_ID_DRV_TYPE_MASK			0x7f000000
#define DRV_ID_DRV_TYPE_OFFSET			24
#define DRV_ID_DRV_TYPE_UNKNOWN			(0 << DRV_ID_DRV_TYPE_OFFSET)
#define DRV_ID_DRV_TYPE_LINUX			BIT(DRV_ID_DRV_TYPE_OFFSET)
#define DRV_ID_DRV_TYPE_WINDOWS			(2 << DRV_ID_DRV_TYPE_OFFSET)
#define DRV_ID_DRV_TYPE_DIAG			(3 << DRV_ID_DRV_TYPE_OFFSET)
#define DRV_ID_DRV_TYPE_PREBOOT			(4 << DRV_ID_DRV_TYPE_OFFSET)
#define DRV_ID_DRV_TYPE_SOLARIS			(5 << DRV_ID_DRV_TYPE_OFFSET)
#define DRV_ID_DRV_TYPE_VMWARE			(6 << DRV_ID_DRV_TYPE_OFFSET)
#define DRV_ID_DRV_TYPE_FREEBSD			(7 << DRV_ID_DRV_TYPE_OFFSET)
#define DRV_ID_DRV_TYPE_AIX			(8 << DRV_ID_DRV_TYPE_OFFSET)

#define DRV_ID_DRV_TYPE_OS			(DRV_ID_DRV_TYPE_LINUX |   \
	                                         DRV_ID_DRV_TYPE_WINDOWS | \
	                                         DRV_ID_DRV_TYPE_SOLARIS | \
	                                         DRV_ID_DRV_TYPE_VMWARE |  \
	                                         DRV_ID_DRV_TYPE_FREEBSD | \
	                                         DRV_ID_DRV_TYPE_AIX)

#define DRV_ID_DRV_INIT_HW_MASK			0x80000000
#define DRV_ID_DRV_INIT_HW_OFFSET		31
#define DRV_ID_DRV_INIT_HW_FLAG			BIT(DRV_ID_DRV_INIT_HW_OFFSET)

	u32 oem_cfg_func;
#define OEM_CFG_FUNC_TC_MASK			0x0000000F
#define OEM_CFG_FUNC_TC_OFFSET			0
#define OEM_CFG_FUNC_TC_0			0x0
#define OEM_CFG_FUNC_TC_1			0x1
#define OEM_CFG_FUNC_TC_2			0x2
#define OEM_CFG_FUNC_TC_3			0x3
#define OEM_CFG_FUNC_TC_4			0x4
#define OEM_CFG_FUNC_TC_5			0x5
#define OEM_CFG_FUNC_TC_6			0x6
#define OEM_CFG_FUNC_TC_7			0x7

#define OEM_CFG_FUNC_HOST_PRI_CTRL_MASK		0x00000030
#define OEM_CFG_FUNC_HOST_PRI_CTRL_OFFSET	4
#define OEM_CFG_FUNC_HOST_PRI_CTRL_VNIC		0x1
#define OEM_CFG_FUNC_HOST_PRI_CTRL_OS		0x2
	struct drv_version_stc drv_ver;
};

/**************************************/
/*                                                                */
/*     P U B L I C       M B		  */
/*                                                                */
/**************************************/
/* This is the only section that the driver can write to, and each */
/* Basically each driver request to set feature parameters,
 * will be done using a different command, which will be linked
 * to a specific data structure from the union below.
 * For huge strucuture, the common blank structure should be used.
 */

struct mcp_mac {
	u32 mac_upper;		/* Upper 16 bits are always zeroes */
	u32 mac_lower;
};

struct mcp_file_att {
	u32 nvm_start_addr;
	u32 len;
};

struct bist_nvm_image_att {
	u32 return_code;
	u32 image_type;		/* Image type */
	u32 nvm_start_addr;	/* NVM address of the image */
	u32 len;		/* Include CRC */
};

/* statistics for ncsi */
struct lan_stats_stc {
	u64 ucast_rx_pkts;
	u64 ucast_tx_pkts;
	u32 fcs_err;
	u32 rserved;
};

struct fcoe_stats_stc {
	u64 rx_pkts;
	u64 tx_pkts;
	u32 fcs_err;
	u32 login_failure;
};

struct iscsi_stats_stc {
	u64 rx_pdus;
	u64 tx_pdus;
	u64 rx_bytes;
	u64 tx_bytes;
};

struct rdma_stats_stc {
	u64 rx_pkts;
	u64 tx_pkts;
	u64 rx_bytes;
	u64 tx_bytes;
};

struct ocbb_data_stc {
	u32 ocbb_host_addr;
	u32 ocsd_host_addr;
	u32 ocsd_req_update_interval;
};

struct fcoe_cap_stc {
#define FCOE_CAP_UNDEFINED_VALUE	0xffff
	u32 max_ios;		/*Maximum number of I/Os per connection */
#define FCOE_CAP_IOS_MASK		0x0000ffff
#define FCOE_CAP_IOS_OFFSET		0
	u32 max_log;		/*Maximum number of Logins per port */
#define FCOE_CAP_LOG_MASK		0x0000ffff
#define FCOE_CAP_LOG_OFFSET		0
	u32 max_exch;		/*Maximum number of exchanges */
#define FCOE_CAP_EXCH_MASK		0x0000ffff
#define FCOE_CAP_EXCH_OFFSET		0
	u32 max_npiv;		/*Maximum NPIV WWN per port */
#define FCOE_CAP_NPIV_MASK		0x0000ffff
#define FCOE_CAP_NPIV_OFFSET		0
	u32 max_tgt;		/*Maximum number of targets supported */
#define FCOE_CAP_TGT_MASK		0x0000ffff
#define FCOE_CAP_TGT_OFFSET		0
	u32 max_outstnd;	/*Maximum number of outstanding commands across all connections */
#define FCOE_CAP_OUTSTND_MASK		0x0000ffff
#define FCOE_CAP_OUTSTND_OFFSET		0
};
#define MAX_NUM_OF_SENSORS		7
#define MFW_SENSOR_LOCATION_INTERNAL	1
#define MFW_SENSOR_LOCATION_EXTERNAL	2
#define MFW_SENSOR_LOCATION_SFP		3

#define SENSOR_LOCATION_OFFSET		0
#define SENSOR_LOCATION_MASK		0x000000ff
#define THRESHOLD_HIGH_OFFSET		8
#define THRESHOLD_HIGH_MASK		0x0000ff00
#define CRITICAL_TEMPERATURE_OFFSET	16
#define CRITICAL_TEMPERATURE_MASK	0x00ff0000
#define CURRENT_TEMP_OFFSET		24
#define CURRENT_TEMP_MASK		0xff000000
struct temperature_status_stc {
	u32 num_of_sensors;
	u32 sensor[MAX_NUM_OF_SENSORS];
};

/* crash dump configuration header */
struct mdump_config_stc {
	u32 version;
	u32 config;
	u32 epoc;
	u32 num_of_logs;
	u32 valid_logs;
};

enum resource_id_enum {
	RESOURCE_NUM_SB_E = 0,
	RESOURCE_NUM_L2_QUEUE_E = 1,
	RESOURCE_NUM_VPORT_E = 2,
	RESOURCE_NUM_VMQ_E = 3,
	RESOURCE_FACTOR_NUM_RSS_PF_E = 4,	/* Not a real resource!! it's a factor used to calculate others */
	RESOURCE_FACTOR_RSS_PER_VF_E = 5,	/* Not a real resource!! it's a factor used to calculate others */
	RESOURCE_NUM_RL_E = 6,
	RESOURCE_NUM_PQ_E = 7,
	RESOURCE_NUM_VF_E = 8,
	RESOURCE_VFC_FILTER_E = 9,
	RESOURCE_ILT_E = 10,
	RESOURCE_CQS_E = 11,
	RESOURCE_GFT_PROFILES_E = 12,
	RESOURCE_NUM_TC_E = 13,
	RESOURCE_NUM_RSS_ENGINES_E = 14,
	RESOURCE_LL2_QUEUE_E = 15,
	RESOURCE_RDMA_STATS_QUEUE_E = 16,
	RESOURCE_BDQ_E = 17,
	RESOURCE_QCN_E = 18,
	RESOURCE_LLH_FILTER_E = 19,
	RESOURCE_VF_MAC_ADDR = 20,
	RESOURCE_LL2_CQS_E = 21,
	RESOURCE_VF_CNQS = 22,
	RESOURCE_MAX_NUM,
	RESOURCE_NUM_INVALID = 0xFFFFFFFF
};

/* Resource ID is to be filled by the driver in the MB request
 * Size, offset & flags to be filled by the MFW in the MB response
 */
struct resource_info {
	enum resource_id_enum res_id;
	u32 size;		/* number of allocated resources */
	u32 offset;		/* Offset of the 1st resource */
	u32 vf_size;
	u32 vf_offset;
	u32 flags;
#define RESOURCE_ELEMENT_STRICT    (1 << 0)
};

struct mcp_wwn {
	u32 wwn_upper;
	u32 wwn_lower;
};

#define DRV_ROLE_NONE		0
#define DRV_ROLE_PREBOOT	1
#define DRV_ROLE_OS		2
#define DRV_ROLE_KDUMP		3

struct load_req_stc {
	u32 drv_ver_0;
	u32 drv_ver_1;
	u32 fw_ver;
	u32 misc0;
#define LOAD_REQ_ROLE_MASK		0x000000FF
#define LOAD_REQ_ROLE_OFFSET		0
#define LOAD_REQ_LOCK_TO_MASK		0x0000FF00
#define LOAD_REQ_LOCK_TO_OFFSET		8
#define LOAD_REQ_LOCK_TO_DEFAULT	0
#define LOAD_REQ_LOCK_TO_NONE		255
#define LOAD_REQ_FORCE_MASK		0x000F0000
#define LOAD_REQ_FORCE_OFFSET		16
#define LOAD_REQ_FORCE_NONE		0
#define LOAD_REQ_FORCE_PF		1
#define LOAD_REQ_FORCE_ALL		2
#define LOAD_REQ_FLAGS0_MASK		0x00F00000
#define LOAD_REQ_FLAGS0_OFFSET		20
#define LOAD_REQ_FLAGS0_AVOID_RESET	(0x1 << 0)
};

struct load_rsp_stc {
	u32 drv_ver_0;
	u32 drv_ver_1;
	u32 fw_ver;
	u32 misc0;
#define LOAD_RSP_ROLE_MASK		0x000000FF
#define LOAD_RSP_ROLE_OFFSET		0
#define LOAD_RSP_HSI_MASK		0x0000FF00
#define LOAD_RSP_HSI_OFFSET		8
#define LOAD_RSP_FLAGS0_MASK		0x000F0000
#define LOAD_RSP_FLAGS0_OFFSET		16
#define LOAD_RSP_FLAGS0_DRV_EXISTS	(0x1 << 0)
};

struct mdump_retain_data_stc {
	u32 valid;
	u32 epoch;
	u32 pf;
	u32 status;
};

struct attribute_cmd_write_stc {
	u32 val;
	u32 mask;
	u32 offset;
};

struct lldp_stats_stc {
	u32 tx_frames_total;
	u32 rx_frames_total;
	u32 rx_frames_discarded;
	u32 rx_age_outs;
};

struct get_att_ctrl_stc {
	u32 disabled_attns;
	u32 controllable_attns;
};

struct trace_filter_stc {
	u32 level;
	u32 modules;
};
union drv_union_data {
	struct mcp_mac wol_mac;	/* UNLOAD_DONE */

	/* This configuration should be set by the driver for the LINK_SET command. */
	struct eth_phy_cfg drv_phy_cfg;

	struct mcp_val64 val64;	/* For PHY / AVS commands */

	u8 raw_data[MCP_DRV_NVM_BUF_LEN];

	struct mcp_file_att file_att;
	/*extend from (VF_MAX_STATIC / 32 6 dword) to support 240 VFs (8 dwords) */
	u32 ack_vf_disabled[EXT_VF_BITMAP_SIZE_IN_DWORDS];

	struct drv_version_stc drv_version;

	struct lan_stats_stc lan_stats;
	struct fcoe_stats_stc fcoe_stats;
	struct iscsi_stats_stc iscsi_stats;
	struct rdma_stats_stc rdma_stats;
	struct ocbb_data_stc ocbb_info;
	struct temperature_status_stc temp_info;
	struct resource_info resource;
	struct bist_nvm_image_att nvm_image_att;
	struct mdump_config_stc mdump_config;
	struct mcp_mac lldp_mac;
	struct mcp_wwn fcoe_fabric_name;
	u32 dword;

	struct load_req_stc load_req;
	struct load_rsp_stc load_rsp;
	struct mdump_retain_data_stc mdump_retain;
	struct attribute_cmd_write_stc attribute_cmd_write;
	struct lldp_stats_stc lldp_stats;
	struct pcie_stats_stc pcie_stats;

	struct get_att_ctrl_stc get_att_ctrl;
	struct fcoe_cap_stc fcoe_cap;
	struct trace_filter_stc trace_filter;
	/* ... */
};

struct public_drv_mb {
	u32 drv_mb_header;
#define DRV_MSG_SEQ_NUMBER_MASK			0x0000ffff
#define DRV_MSG_SEQ_NUMBER_OFFSET		0
#define DRV_MSG_CODE_MASK			0xffff0000
#define DRV_MSG_CODE_OFFSET			16
	u32 drv_mb_param;

	u32 fw_mb_header;
#define FW_MSG_SEQ_NUMBER_MASK			0x0000ffff
#define FW_MSG_SEQ_NUMBER_OFFSET		0
#define FW_MSG_CODE_MASK			0xffff0000
#define FW_MSG_CODE_OFFSET			16
	u32 fw_mb_param;

	u32 drv_pulse_mb;
#define DRV_PULSE_SEQ_MASK			0x00007fff
#define DRV_PULSE_SYSTEM_TIME_MASK		0xffff0000
	/*
	 * The system time is in the format of
	 * (year-2001)*12*32 + month*32 + day.
	 */
#define DRV_PULSE_ALWAYS_ALIVE			0x00008000
	/*
	 * Indicate to the firmware not to go into the
	 * OS-absent when it is not getting driver pulse.
	 * This is used for debugging as well for PXE(MBA).
	 */

	u32 mcp_pulse_mb;
#define MCP_PULSE_SEQ_MASK			0x00007fff
#define MCP_PULSE_ALWAYS_ALIVE			0x00008000
	/* Indicates to the driver not to assert due to lack
	 * of MCP response */
#define MCP_EVENT_MASK				0xffff0000
#define MCP_EVENT_OTHER_DRIVER_RESET_REQ	0x00010000

	/* The union data is used by the driver to pass parameters to the scratchpad. */
	union drv_union_data union_data;
};
/***************************************************************/
/*                                                                                                                 */
/*                              Driver Message Code (Request)                      */
/*                                                                                                                 */
/***************************************************************/
#define DRV_MSG_CODE(_code_)    (_code_ << DRV_MSG_CODE_OFFSET)
enum drv_msg_code_enum {
	DRV_MSG_CODE_NVM_PUT_FILE_BEGIN = DRV_MSG_CODE(0x0001),	/* Param is either DRV_MB_PARAM_NVM_PUT_FILE_BEGIN_MFW/IMAGE */
	DRV_MSG_CODE_NVM_PUT_FILE_DATA = DRV_MSG_CODE(0x0002),	/* Param should be set to the transaction size (up to 64 bytes) */
	DRV_MSG_CODE_NVM_GET_FILE_ATT = DRV_MSG_CODE(0x0003),	/* MFW will place the file offset and len in file_att struct */
	DRV_MSG_CODE_NVM_READ_NVRAM = DRV_MSG_CODE(0x0005),	/* Read 32bytes of nvram data. Param is [0:23] ??? Offset [24:31] ??? Len in Bytes */
	DRV_MSG_CODE_NVM_WRITE_NVRAM = DRV_MSG_CODE(0x0006),	/* Writes up to 32Bytes to nvram. Param is [0:23] ??? Offset [24:31] ??? Len in Bytes. In case this address is in the range of secured file in secured mode, the operation will fail */
	DRV_MSG_CODE_NVM_DEL_FILE = DRV_MSG_CODE(0x0008),	/* Delete a file from nvram. Param is image_type. */
	DRV_MSG_CODE_MCP_RESET = DRV_MSG_CODE(0x0009),	/* Reset MCP when no NVM operation is going on, and no drivers are loaded. In case operation succeed, MCP will not ack back. */
	DRV_MSG_CODE_PHY_RAW_READ = DRV_MSG_CODE(0x000b),	/* Param: [0:15] - Address, [16:18] - lane# (0/1/2/3 - for single lane, 4/5 - for dual lanes, 6 - for all lanes, [28] - PMD reg, [29] - select port, [30:31] - port */
	DRV_MSG_CODE_PHY_RAW_WRITE = DRV_MSG_CODE(0x000c),	/* Param: [0:15] - Address, [16:18] - lane# (0/1/2/3 - for single lane, 4/5 - for dual lanes, 6 - for all lanes, [28] - PMD reg, [29] - select port, [30:31] - port */
	DRV_MSG_CODE_PHY_CORE_READ = DRV_MSG_CODE(0x000d),	/* Param: [0:15] - Address, [30:31] - port */
	DRV_MSG_CODE_PHY_CORE_WRITE = DRV_MSG_CODE(0x000e),	/* Param: [0:15] - Address, [30:31] - port */
	DRV_MSG_CODE_SET_VERSION = DRV_MSG_CODE(0x000f),	/* Param: [0:3] - version, [4:15] - name (null terminated) */
	DRV_MSG_CODE_MCP_HALT = DRV_MSG_CODE(0x0010),	/* Halts the MCP. To resume MCP, user will need to use MCP_REG_CPU_STATE/MCP_REG_CPU_MODE registers. */
	DRV_MSG_CODE_SET_VMAC = DRV_MSG_CODE(0x0011),	/* Set virtual mac address, params [31:6] - reserved, [5:4] - type, [3:0] - func, drv_data[7:0] - MAC/WWNN/WWPN */
	DRV_MSG_CODE_GET_VMAC = DRV_MSG_CODE(0x0012),	/* Set virtual mac address, params [31:6] - reserved, [5:4] - type, [3:0] - func, drv_data[7:0] - MAC/WWNN/WWPN */
	DRV_MSG_CODE_GET_STATS = DRV_MSG_CODE(0x0013),	/* Get statistics from pf, params [31:4] - reserved, [3:0] - stats type */
	DRV_MSG_CODE_PMD_DIAG_DUMP = DRV_MSG_CODE(0x0014),	/* Host shall provide buffer and size for MFW  */
	DRV_MSG_CODE_PMD_DIAG_EYE = DRV_MSG_CODE(0x0015),	/* Host shall provide buffer and size for MFW  */
	DRV_MSG_CODE_TRANSCEIVER_READ = DRV_MSG_CODE(0x0016),	/* Param: [0:1] - Port, [2:7] - read size, [8:15] - I2C address, [16:31] - offset */
	DRV_MSG_CODE_TRANSCEIVER_WRITE = DRV_MSG_CODE(0x0017),	/* Param: [0:1] - Port, [2:7] - write size, [8:15] - I2C address, [16:31] - offset */
	DRV_MSG_CODE_OCBB_DATA = DRV_MSG_CODE(0x0018),	/* indicate OCBB related information */
	DRV_MSG_CODE_SET_BW = DRV_MSG_CODE(0x0019),	/* Set function BW, params[15:8] - min, params[7:0] - max */
	DRV_MSG_CODE_MASK_PARITIES = DRV_MSG_CODE(0x001a),	/* When param is set to 1, all parities will be masked(disabled). When params are set to 0, parities will be unmasked again. */
	DRV_MSG_CODE_INDUCE_FAILURE = DRV_MSG_CODE(0x001b),	/* param[0] - Simulate fan failure,  param[1] - simulate over temp. */
	DRV_MSG_CODE_GPIO_READ = DRV_MSG_CODE(0x001c),	/* Param: [0:15] - gpio number */
	DRV_MSG_CODE_GPIO_WRITE = DRV_MSG_CODE(0x001d),	/* Param: [0:15] - gpio number, [16:31] - gpio value */
	DRV_MSG_CODE_BIST_TEST = DRV_MSG_CODE(0x001e),	/* Param: [0:7] - test enum, [8:15] - image index, [16:31] - reserved */
	DRV_MSG_CODE_GET_TEMPERATURE = DRV_MSG_CODE(0x001f),
	DRV_MSG_CODE_SET_LED_MODE = DRV_MSG_CODE(0x0020),	/* Set LED mode  params :0 operational, 1 LED turn ON, 2 LED turn OFF */
	DRV_MSG_CODE_TIMESTAMP = DRV_MSG_CODE(0x0021),	/* drv_data[7:0] - EPOC in seconds, drv_data[15:8] - driver version (MAJ MIN BUILD SUB) */
	DRV_MSG_CODE_EMPTY_MB = DRV_MSG_CODE(0x0022),	/* This is an empty mailbox just return OK */
	DRV_MSG_CODE_RESOURCE_CMD = DRV_MSG_CODE(0x0023),	/* Param[0:4] - resource number (0-31), Param[5:7] - opcode, param[15:8] - age */
	DRV_MSG_CODE_GET_MBA_VERSION = DRV_MSG_CODE(0x0024),	/* Get MBA version */
	DRV_MSG_CODE_MDUMP_CMD = DRV_MSG_CODE(0x0025),	/* Send crash dump commands with param[3:0] - opcode */
	DRV_MSG_CODE_MEM_ECC_EVENTS = DRV_MSG_CODE(0x0026),	/* Param: None */
	DRV_MSG_CODE_GPIO_INFO = DRV_MSG_CODE(0x0027),	/* Param: [0:15] - gpio number */
	DRV_MSG_CODE_EXT_PHY_READ = DRV_MSG_CODE(0x0028),	/* Value will be placed in union */
	DRV_MSG_CODE_EXT_PHY_WRITE = DRV_MSG_CODE(0x0029),	/* Value shoud be placed in union */
	DRV_MSG_CODE_EXT_PHY_FW_UPGRADE = DRV_MSG_CODE(0x002a),
	DRV_MSG_CODE_GET_PF_RDMA_PROTOCOL = DRV_MSG_CODE(0x002b),
	DRV_MSG_CODE_SET_LLDP_MAC = DRV_MSG_CODE(0x002c),
	DRV_MSG_CODE_GET_LLDP_MAC = DRV_MSG_CODE(0x002d),
	DRV_MSG_CODE_OS_WOL = DRV_MSG_CODE(0x002e),
	DRV_MSG_CODE_GET_TLV_DONE = DRV_MSG_CODE(0x002f),	/* Param: None */
	DRV_MSG_CODE_FEATURE_SUPPORT = DRV_MSG_CODE(0x0030),	/* Param: Set DRV_MB_PARAM_FEATURE_SUPPORT_* */
	DRV_MSG_CODE_GET_MFW_FEATURE_SUPPORT = DRV_MSG_CODE(0x0031),	/* return FW_MB_PARAM_FEATURE_SUPPORT_*  */
	DRV_MSG_CODE_READ_WOL_REG = DRV_MSG_CODE(0x0032),
	DRV_MSG_CODE_WRITE_WOL_REG = DRV_MSG_CODE(0x0033),
	DRV_MSG_CODE_GET_WOL_BUFFER = DRV_MSG_CODE(0x0034),
	DRV_MSG_CODE_ATTRIBUTE = DRV_MSG_CODE(0x0035),	/* Param: [0:23] Attribute key, [24:31] Attribute sub command */
	DRV_MSG_CODE_ENCRYPT_PASSWORD = DRV_MSG_CODE(0x0036),	/* Param: Password len. Union: Plain Password */
	DRV_MSG_CODE_GET_ENGINE_CONFIG = DRV_MSG_CODE(0x0037),	/* Param: None */
	DRV_MSG_CODE_PMBUS_READ = DRV_MSG_CODE(0x0038),	/* Param: [0:7] - Cmd, [8:9] - len */
	DRV_MSG_CODE_PMBUS_WRITE = DRV_MSG_CODE(0x0039),	/* Param: [0:7] - Cmd, [8:9] - len, [16:31] -data */
	DRV_MSG_CODE_GENERIC_IDC = DRV_MSG_CODE(0x003a),
	DRV_MSG_CODE_RESET_CHIP = DRV_MSG_CODE(0x003b),	/* If param = 0, it triggers engine reset. If param = 1, it triggers soft_reset */
	DRV_MSG_CODE_SET_RETAIN_VMAC = DRV_MSG_CODE(0x003c),
	DRV_MSG_CODE_GET_RETAIN_VMAC = DRV_MSG_CODE(0x003d),
	DRV_MSG_CODE_GET_NVM_CFG_OPTION = DRV_MSG_CODE(0x003e),	/* Param: [0:15] Option ID, [16] - All, [17] - Init, [18] - Commit, [19] - Free, [20] - SelectEntity, [24:27] - EntityID */
	DRV_MSG_CODE_SET_NVM_CFG_OPTION = DRV_MSG_CODE(0x003f),	/* Param: [0:15] Option ID,             [17] - Init, [18]         , [19] - Free, [20] - SelectEntity, [24:27] - EntityID */
	DRV_MSG_CODE_PCIE_STATS_START = DRV_MSG_CODE(0x0040),	/* param determine the timeout in usec */
	DRV_MSG_CODE_PCIE_STATS_GET = DRV_MSG_CODE(0x0041),	/* MFW returns the stats (struct pcie_stats_stc pcie_stats) gathered in fw_param */
	DRV_MSG_CODE_GET_ATTN_CONTROL = DRV_MSG_CODE(0x0042),
	DRV_MSG_CODE_SET_ATTN_CONTROL = DRV_MSG_CODE(0x0043),
	DRV_MSG_CODE_SET_TRACE_FILTER = DRV_MSG_CODE(0x0044),	/* Param is the struct size. Arguments are in trace_filter_stc (raw data) */
	DRV_MSG_CODE_RESTORE_TRACE_FILTER = DRV_MSG_CODE(0x0045),	/* No params. Restores the trace level to the nvm cfg */
	DRV_MSG_CODE_INITIATE_FLR_DEPRECATED = DRV_MSG_CODE(0x0200),	/*deprecated don't use */
	DRV_MSG_CODE_INITIATE_PF_FLR = DRV_MSG_CODE(0x0201),
	DRV_MSG_CODE_INITIATE_VF_FLR = DRV_MSG_CODE(0x0202),
	DRV_MSG_CODE_LOAD_REQ = DRV_MSG_CODE(0x1000),
	DRV_MSG_CODE_LOAD_DONE = DRV_MSG_CODE(0x1100),
	DRV_MSG_CODE_INIT_HW = DRV_MSG_CODE(0x1200),
	DRV_MSG_CODE_CANCEL_LOAD_REQ = DRV_MSG_CODE(0x1300),
	DRV_MSG_CODE_UNLOAD_REQ = DRV_MSG_CODE(0x2000),
	DRV_MSG_CODE_UNLOAD_DONE = DRV_MSG_CODE(0x2100),
	DRV_MSG_CODE_INIT_PHY = DRV_MSG_CODE(0x2200),	/* Params - FORCE - Reinitialize the link regardless of LFA , - DONT_CARE - Don't flap the link if up */
	DRV_MSG_CODE_LINK_RESET = DRV_MSG_CODE(0x2300),
	DRV_MSG_CODE_SET_LLDP = DRV_MSG_CODE(0x2400),
	DRV_MSG_CODE_REGISTER_LLDP_TLVS_RX = DRV_MSG_CODE(0x2410),
	DRV_MSG_CODE_SET_DCBX = DRV_MSG_CODE(0x2500),	/* OneView feature driver HSI */
	DRV_MSG_CODE_OV_UPDATE_CURR_CFG = DRV_MSG_CODE(0x2600),
	DRV_MSG_CODE_OV_UPDATE_BUS_NUM = DRV_MSG_CODE(0x2700),
	DRV_MSG_CODE_OV_UPDATE_BOOT_PROGRESS = DRV_MSG_CODE(0x2800),
	DRV_MSG_CODE_OV_UPDATE_STORM_FW_VER = DRV_MSG_CODE(0x2900),
	DRV_MSG_CODE_NIG_DRAIN = DRV_MSG_CODE(0x3000),
	DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE = DRV_MSG_CODE(0x3100),
	DRV_MSG_CODE_BW_UPDATE_ACK = DRV_MSG_CODE(0x3200),
	DRV_MSG_CODE_OV_UPDATE_MTU = DRV_MSG_CODE(0x3300),
	DRV_MSG_GET_RESOURCE_ALLOC_MSG = DRV_MSG_CODE(0x3400),	/* DRV_MB Param: driver version supp, FW_MB param: MFW version supp, data: struct resource_info */
	DRV_MSG_SET_RESOURCE_VALUE_MSG = DRV_MSG_CODE(0x3500),
	DRV_MSG_CODE_OV_UPDATE_WOL = DRV_MSG_CODE(0x3800),
	DRV_MSG_CODE_OV_UPDATE_ESWITCH_MODE = DRV_MSG_CODE(0x3900),
	DRV_MSG_CODE_S_TAG_UPDATE_ACK = DRV_MSG_CODE(0x3b00),
	DRV_MSG_CODE_OEM_UPDATE_FCOE_CVID = DRV_MSG_CODE(0x3c00),
	DRV_MSG_CODE_OEM_UPDATE_FCOE_FABRIC_NAME = DRV_MSG_CODE(0x3d00),
	DRV_MSG_CODE_OEM_UPDATE_BOOT_CFG = DRV_MSG_CODE(0x3e00),
	DRV_MSG_CODE_OEM_RESET_TO_DEFAULT = DRV_MSG_CODE(0x3f00),
	DRV_MSG_CODE_OV_GET_CURR_CFG = DRV_MSG_CODE(0x4000),
	DRV_MSG_CODE_GET_OEM_UPDATES = DRV_MSG_CODE(0x4100),
	DRV_MSG_CODE_GET_LLDP_STATS = DRV_MSG_CODE(0x4200),
	DRV_MSG_CODE_GET_PPFID_BITMAP = DRV_MSG_CODE(0x4300),	/* params [31:8] - reserved, [7:0] - bitmap */
	DRV_MSG_CODE_VF_DISABLED_DONE = DRV_MSG_CODE(0xc000),
	DRV_MSG_CODE_CFG_VF_MSIX = DRV_MSG_CODE(0xc001),
	DRV_MSG_CODE_CFG_PF_VFS_MSIX = DRV_MSG_CODE(0xc002),
	DRV_MSG_CODE_GET_PERM_MAC = DRV_MSG_CODE(0xc003),	/* get permanent mac - params: [31:24] reserved ,[23:8] index, [7:4] reserved, [3:0] type,.- driver_data[7:0] mac */
	DRV_MSG_CODE_DEBUG_DATA_SEND = DRV_MSG_CODE(0xc004),	/* send driver debug data up to 32 bytes params [7:0] size < 32 data in the union part */
	DRV_MSG_CODE_GET_FCOE_CAP = DRV_MSG_CODE(0xc005),	/* get fcoe capablities from driver */
	DRV_MSG_CODE_VF_WITH_MORE_16SB = DRV_MSG_CODE(0xc006),	/* params [7:0] - VFID, [8] - set/clear */
	DRV_MSG_CODE_GET_MANAGEMENT_STATUS = DRV_MSG_CODE(0xc007),	/* return FW_MB_MANAGEMENT_STATUS */
};

/* DRV_MSG_CODE_VMAC_TYPE parameters */
#define DRV_MSG_CODE_VMAC_TYPE_OFFSET					4
#define DRV_MSG_CODE_VMAC_TYPE_MASK					0x30
#define DRV_MSG_CODE_VMAC_TYPE_MAC					1
#define DRV_MSG_CODE_VMAC_TYPE_WWNN					2
#define DRV_MSG_CODE_VMAC_TYPE_WWPN					3

/* DRV_MSG_CODE_RETAIN_VMAC parameters */
#define DRV_MSG_CODE_RETAIN_VMAC_FUNC_OFFSET				0
#define DRV_MSG_CODE_RETAIN_VMAC_FUNC_MASK				0xf

#define DRV_MSG_CODE_RETAIN_VMAC_TYPE_OFFSET				4
#define DRV_MSG_CODE_RETAIN_VMAC_TYPE_MASK				0x70
#define DRV_MSG_CODE_RETAIN_VMAC_TYPE_L2				0
#define DRV_MSG_CODE_RETAIN_VMAC_TYPE_ISCSI				1
#define DRV_MSG_CODE_RETAIN_VMAC_TYPE_FCOE				2
#define DRV_MSG_CODE_RETAIN_VMAC_TYPE_WWNN				3
#define DRV_MSG_CODE_RETAIN_VMAC_TYPE_WWPN				4

#define DRV_MSG_CODE_MCP_RESET_FORCE					0xf04ce
/* DRV_MSG_CODE_STAT_TYPE parameters */
#define DRV_MSG_CODE_STATS_TYPE_LAN					1
#define DRV_MSG_CODE_STATS_TYPE_FCOE					2
#define DRV_MSG_CODE_STATS_TYPE_ISCSI					3
#define DRV_MSG_CODE_STATS_TYPE_RDMA					4

/* DRV_MSG_CODE_SET_BW parametes */
#define BW_MAX_MASK \
                                                                        0x000000ff
#define BW_MAX_OFFSET							0
#define BW_MIN_MASK \
                                                                        0x0000ff00
#define BW_MIN_OFFSET							8

/* DRV_MSG_CODE_INDUCE_FAILURE parameters */
#define DRV_MSG_FAN_FAILURE_TYPE					(1 << 0)
#define DRV_MSG_TEMPERATURE_FAILURE_TYPE				(1 << 1)

/* DRV_MSG_CODE_RESOURCE_CMD parameters */
#define RESOURCE_CMD_REQ_RESC_MASK \
                                                                        0x0000001F
#define RESOURCE_CMD_REQ_RESC_OFFSET					0
#define RESOURCE_CMD_REQ_OPCODE_MASK \
                                                                        0x000000E0
#define RESOURCE_CMD_REQ_OPCODE_OFFSET					5
#define RESOURCE_OPCODE_REQ						1	/* request resource ownership with default aging */
#define RESOURCE_OPCODE_REQ_WO_AGING					2	/* request resource ownership without aging */
#define RESOURCE_OPCODE_REQ_W_AGING					3	/* request resource ownership with specific aging timer (in seconds) */
#define RESOURCE_OPCODE_RELEASE						4	/* release resource */
#define RESOURCE_OPCODE_FORCE_RELEASE					5	/* force resource release */
#define RESOURCE_CMD_REQ_AGE_MASK \
                                                                        0x0000FF00
#define RESOURCE_CMD_REQ_AGE_OFFSET					8
#define RESOURCE_CMD_RSP_OWNER_MASK \
                                                                        0x000000FF
#define RESOURCE_CMD_RSP_OWNER_OFFSET					0
#define RESOURCE_CMD_RSP_OPCODE_MASK \
                                                                        0x00000700
#define RESOURCE_CMD_RSP_OPCODE_OFFSET					8
#define RESOURCE_OPCODE_GNT						1	/* resource is free and granted to requester */
#define RESOURCE_OPCODE_BUSY						2	/* resource is busy, param[7:0] indicates owner as follow 0-15 = PF0-15, 16 = MFW, 17 = diag over serial */
#define RESOURCE_OPCODE_RELEASED					3	/* indicate release request was acknowledged */
#define RESOURCE_OPCODE_RELEASED_PREVIOUS				4	/* indicate release request was previously received by other owner */
#define RESOURCE_OPCODE_WRONG_OWNER					5	/* indicate wrong owner during release */
#define RESOURCE_OPCODE_UNKNOWN_CMD					255
#define RESOURCE_DUMP							0	/* dedicate resource 0 for dump */

/* DRV_MSG_CODE_MDUMP_CMD parameters */
#define MDUMP_DRV_PARAM_OPCODE_MASK \
                                                                        0x000000ff
#define DRV_MSG_CODE_MDUMP_ACK						0x01	/* acknowledge reception of error indication */
#define DRV_MSG_CODE_MDUMP_SET_VALUES					0x02	/* set epoc and personality as follow: drv_data[3:0] - epoch, drv_data[7:4] - personality */
#define DRV_MSG_CODE_MDUMP_TRIGGER					0x03	/* trigger crash dump procedure */
#define DRV_MSG_CODE_MDUMP_GET_CONFIG					0x04	/* Request valid logs and config words */
#define DRV_MSG_CODE_MDUMP_SET_ENABLE					0x05	/* Set triggers mask. drv_mb_param should indicate (bitwise) which trigger enabled */
#define DRV_MSG_CODE_MDUMP_CLEAR_LOGS					0x06	/* Clear all logs */
#define DRV_MSG_CODE_MDUMP_GET_RETAIN					0x07	/* Get retained data */
#define DRV_MSG_CODE_MDUMP_CLR_RETAIN					0x08	/* Clear retain data */
/* hw_dump parameters*/
#define DRV_MSG_CODE_HW_DUMP_TRIGGER					0x0a	/* Clear retain data */
#define DRV_MSG_CODE_MDUMP_FREE_DRIVER_BUF				0x0b	/* Free Link_Dump / Idle_check buffer after driver consumed it */
#define DRV_MSG_CODE_MDUMP_GEN_LINK_DUMP				0x0c	/* Generate debug data for scripts as wol_dump, link_dump and phy_dump. Data will be available at fw_param (offsize) in fw_param) for 6 seconds */
#define DRV_MSG_CODE_MDUMP_GEN_IDLE_CHK					0x0d	/* Generate debug data for scripts as idle_chk */

/* DRV_MSG_CODE_MDUMP_CMD options */
#define MDUMP_DRV_PARAM_OPTION_MASK \
                                                                        0x00000f00
#define DRV_MSG_CODE_MDUMP_USE_DRIVER_BUF_OFFSET			8	/* Driver will not use debug data (GEN_LINK_DUMP, GEN_IDLE_CHK), free Buffer imediately */
#define DRV_MSG_CODE_MDUMP_USE_DRIVER_BUF_MASK				0x100	/* Driver will not use debug data (GEN_LINK_DUMP, GEN_IDLE_CHK), free Buffer imediately */

/* DRV_MSG_CODE_EXT_PHY_READ/DRV_MSG_CODE_EXT_PHY_WRITE parameters */
#define DRV_MB_PARAM_ADDR_OFFSET					0
#define DRV_MB_PARAM_ADDR_MASK \
                                                                        0x0000FFFF
#define DRV_MB_PARAM_DEVAD_OFFSET					16
#define DRV_MB_PARAM_DEVAD_MASK \
                                                                        0x001F0000
#define DRV_MB_PARAM_PORT_OFFSET					21
#define DRV_MB_PARAM_PORT_MASK \
                                                                        0x00600000

/* DRV_MSG_CODE_PMBUS_READ/DRV_MSG_CODE_PMBUS_WRITE parameters */
#define DRV_MB_PARAM_PMBUS_CMD_OFFSET					0
#define DRV_MB_PARAM_PMBUS_CMD_MASK					0xFF
#define DRV_MB_PARAM_PMBUS_LEN_OFFSET					8
#define DRV_MB_PARAM_PMBUS_LEN_MASK					0x300
#define DRV_MB_PARAM_PMBUS_DATA_OFFSET					16
#define DRV_MB_PARAM_PMBUS_DATA_MASK \
                                                                        0xFFFF0000

/* UNLOAD_REQ params */
#define DRV_MB_PARAM_UNLOAD_WOL_UNKNOWN \
                                                                        0x00000000
#define DRV_MB_PARAM_UNLOAD_WOL_MCP \
                                                                        0x00000001
#define DRV_MB_PARAM_UNLOAD_WOL_DISABLED \
                                                                        0x00000002
#define DRV_MB_PARAM_UNLOAD_WOL_ENABLED \
                                                                        0x00000003

/* UNLOAD_DONE_params */
#define DRV_MB_PARAM_UNLOAD_NON_D3_POWER \
                                                                        0x00000001

/* INIT_PHY params */
#define DRV_MB_PARAM_INIT_PHY_FORCE \
                                                                        0x00000001
#define DRV_MB_PARAM_INIT_PHY_DONT_CARE \
                                                                        0x00000002

/* LLDP / DCBX params*/
/* To be used with SET_LLDP command */
#define DRV_MB_PARAM_LLDP_SEND_MASK \
                                                                        0x00000001
#define DRV_MB_PARAM_LLDP_SEND_OFFSET					0
/* To be used with SET_LLDP and REGISTER_LLDP_TLVS_RX commands */
#define DRV_MB_PARAM_LLDP_AGENT_MASK \
                                                                        0x00000006
#define DRV_MB_PARAM_LLDP_AGENT_OFFSET					1
/* To be used with REGISTER_LLDP_TLVS_RX command */
#define DRV_MB_PARAM_LLDP_TLV_RX_VALID_MASK \
                                                                        0x00000001
#define DRV_MB_PARAM_LLDP_TLV_RX_VALID_OFFSET				0
#define DRV_MB_PARAM_LLDP_TLV_RX_TYPE_MASK \
                                                                        0x000007f0
#define DRV_MB_PARAM_LLDP_TLV_RX_TYPE_OFFSET				4
/* To be used with SET_DCBX command */
#define DRV_MB_PARAM_DCBX_NOTIFY_MASK \
                                                                        0x00000008
#define DRV_MB_PARAM_DCBX_NOTIFY_OFFSET					3
#define DRV_MB_PARAM_DCBX_ADMIN_CFG_NOTIFY_MASK \
                                                                        0x00000010
#define DRV_MB_PARAM_DCBX_ADMIN_CFG_NOTIFY_OFFSET			4

#define DRV_MB_PARAM_NIG_DRAIN_PERIOD_MS_MASK \
                                                                        0x000000FF
#define DRV_MB_PARAM_NIG_DRAIN_PERIOD_MS_OFFSET				0

#define DRV_MB_PARAM_NVM_PUT_FILE_TYPE_MASK \
                                                                        0x000000ff
#define DRV_MB_PARAM_NVM_PUT_FILE_TYPE_OFFSET				0
#define DRV_MB_PARAM_NVM_PUT_FILE_BEGIN_MFW				0x1
#define DRV_MB_PARAM_NVM_PUT_FILE_BEGIN_IMAGE				0x2
#define DRV_MB_PARAM_NVM_PUT_FILE_BEGIN_MBI				0x3

#define DRV_MB_PARAM_NVM_OFFSET_OFFSET					0
#define DRV_MB_PARAM_NVM_OFFSET_MASK \
                                                                        0x00FFFFFF
#define DRV_MB_PARAM_NVM_LEN_OFFSET					24
#define DRV_MB_PARAM_NVM_LEN_MASK \
                                                                        0xFF000000

#define DRV_MB_PARAM_PHY_ADDR_OFFSET					0
#define DRV_MB_PARAM_PHY_ADDR_MASK \
                                                                        0x1FF0FFFF
#define DRV_MB_PARAM_PHY_LANE_OFFSET					16
#define DRV_MB_PARAM_PHY_LANE_MASK \
                                                                        0x000F0000
#define DRV_MB_PARAM_PHY_SELECT_PORT_OFFSET				29
#define DRV_MB_PARAM_PHY_SELECT_PORT_MASK \
                                                                        0x20000000
#define DRV_MB_PARAM_PHY_PORT_OFFSET					30
#define DRV_MB_PARAM_PHY_PORT_MASK \
                                                                        0xc0000000

#define DRV_MB_PARAM_PHYMOD_LANE_OFFSET					0
#define DRV_MB_PARAM_PHYMOD_LANE_MASK \
                                                                        0x000000FF
#define DRV_MB_PARAM_PHYMOD_SIZE_OFFSET					8
#define DRV_MB_PARAM_PHYMOD_SIZE_MASK \
                                                                        0x000FFF00
/* configure vf MSIX params BB */
#define DRV_MB_PARAM_CFG_VF_MSIX_VF_ID_OFFSET				0
#define DRV_MB_PARAM_CFG_VF_MSIX_VF_ID_MASK \
                                                                        0x000000FF
#define DRV_MB_PARAM_CFG_VF_MSIX_SB_NUM_OFFSET				8
#define DRV_MB_PARAM_CFG_VF_MSIX_SB_NUM_MASK \
                                                                        0x0000FF00
/* configure vf MSIX for PF params AH*/
#define DRV_MB_PARAM_CFG_PF_VFS_MSIX_SB_NUM_OFFSET			0
#define DRV_MB_PARAM_CFG_PF_VFS_MSIX_SB_NUM_MASK \
                                                                        0x000000FF

#define DRV_MSG_CODE_VF_WITH_MORE_16SB_VF_ID_OFFSET			0
#define DRV_MSG_CODE_VF_WITH_MORE_16SB_VF_ID_MASK \
                                                                        0x000000FF
#define DRV_MSG_CODE_VF_WITH_MORE_16SB_SET_OFFSET			8
#define DRV_MSG_CODE_VF_WITH_MORE_16SB_SET_MASK \
                                                                        0x00000100

/* OneView configuration parametres */
#define DRV_MB_PARAM_OV_CURR_CFG_OFFSET					0
#define DRV_MB_PARAM_OV_CURR_CFG_MASK \
                                                                        0x0000000F
#define DRV_MB_PARAM_OV_CURR_CFG_NONE					0
#define DRV_MB_PARAM_OV_CURR_CFG_OS					1
#define DRV_MB_PARAM_OV_CURR_CFG_VENDOR_SPEC				2
#define DRV_MB_PARAM_OV_CURR_CFG_OTHER					3
#define DRV_MB_PARAM_OV_CURR_CFG_VC_CLP					4
#define DRV_MB_PARAM_OV_CURR_CFG_CNU					5
#define DRV_MB_PARAM_OV_CURR_CFG_DCI					6
#define DRV_MB_PARAM_OV_CURR_CFG_HII					7

#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_OFFSET				0
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_MASK \
                                                                        0x000000FF
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_NONE				(1 << 0)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_ISCSI_IP_ACQUIRED		(1 << 1)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_FCOE_FABRIC_LOGIN_SUCCESS	(1 << 1)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_TRARGET_FOUND			(1 << 2)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_ISCSI_CHAP_SUCCESS		(1 << 3)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_FCOE_LUN_FOUND			(1 << 3)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_LOGGED_INTO_TGT		(1 << 4)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_IMG_DOWNLOADED			(1 << 5)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_OS_HANDOFF			(1 << 6)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_COMPLETED				0

#define DRV_MB_PARAM_OV_PCI_BUS_NUM_OFFSET				0
#define DRV_MB_PARAM_OV_PCI_BUS_NUM_MASK \
                                                                        0x000000FF

#define DRV_MB_PARAM_OV_STORM_FW_VER_OFFSET				0
#define DRV_MB_PARAM_OV_STORM_FW_VER_MASK \
                                                                        0xFFFFFFFF
#define DRV_MB_PARAM_OV_STORM_FW_VER_MAJOR_MASK \
                                                                        0xFF000000
#define DRV_MB_PARAM_OV_STORM_FW_VER_MINOR_MASK \
                                                                        0x00FF0000
#define DRV_MB_PARAM_OV_STORM_FW_VER_BUILD_MASK \
                                                                        0x0000FF00
#define DRV_MB_PARAM_OV_STORM_FW_VER_DROP_MASK \
                                                                        0x000000FF

#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_OFFSET			0
#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_MASK			0xF
#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_UNKNOWN			0x1
#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_NOT_LOADED			0x2	/* Not Installed */
#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_LOADING			0x3
#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_DISABLED			0x4	/* installed but disabled by user/admin/OS */
#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_ACTIVE			0x5	/* installed and active */

#define DRV_MB_PARAM_OV_MTU_SIZE_OFFSET					0
#define DRV_MB_PARAM_OV_MTU_SIZE_MASK \
                                                                        0xFFFFFFFF

#define DRV_MB_PARAM_WOL_MASK						( \
                DRV_MB_PARAM_WOL_DEFAULT |                                \
                DRV_MB_PARAM_WOL_DISABLED                                 \
                |                                                         \
                DRV_MB_PARAM_WOL_ENABLED)
#define DRV_MB_PARAM_WOL_DEFAULT \
                                                                        DRV_MB_PARAM_UNLOAD_WOL_MCP
#define DRV_MB_PARAM_WOL_DISABLED \
                                                                        DRV_MB_PARAM_UNLOAD_WOL_DISABLED
#define DRV_MB_PARAM_WOL_ENABLED \
                                                                        DRV_MB_PARAM_UNLOAD_WOL_ENABLED

#define DRV_MB_PARAM_ESWITCH_MODE_MASK					( \
                DRV_MB_PARAM_ESWITCH_MODE_NONE |                          \
                DRV_MB_PARAM_ESWITCH_MODE_VEB                             \
                |                                                         \
                DRV_MB_PARAM_ESWITCH_MODE_VEPA)
#define DRV_MB_PARAM_ESWITCH_MODE_NONE					0x0
#define DRV_MB_PARAM_ESWITCH_MODE_VEB					0x1
#define DRV_MB_PARAM_ESWITCH_MODE_VEPA					0x2

#define DRV_MB_PARAM_FCOE_CVID_MASK					0xFFF
#define DRV_MB_PARAM_FCOE_CVID_OFFSET					0

#define DRV_MB_PARAM_BOOT_CFG_UPDATE_MASK				0xFF
#define DRV_MB_PARAM_BOOT_CFG_UPDATE_OFFSET				0
#define DRV_MB_PARAM_BOOT_CFG_UPDATE_FCOE				0x01
#define DRV_MB_PARAM_BOOT_CFG_UPDATE_ISCSI				0x02

#define DRV_MB_PARAM_DUMMY_OEM_UPDATES_MASK				0x1
#define DRV_MB_PARAM_DUMMY_OEM_UPDATES_OFFSET				0

#define DRV_MB_PARAM_LLDP_STATS_AGENT_MASK				0xFF
#define DRV_MB_PARAM_LLDP_STATS_AGENT_OFFSET				0

#define DRV_MB_PARAM_SET_LED_MODE_OPER					0x0
#define DRV_MB_PARAM_SET_LED_MODE_ON					0x1
#define DRV_MB_PARAM_SET_LED_MODE_OFF					0x2
#define DRV_MB_PARAM_SET_LED1_MODE_ON					0x3
#define DRV_MB_PARAM_SET_LED2_MODE_ON					0x4
#define DRV_MB_PARAM_SET_ACT_LED_MODE_ON				0x6

#define DRV_MB_PARAM_TRANSCEIVER_PORT_OFFSET				0
#define DRV_MB_PARAM_TRANSCEIVER_PORT_MASK \
                                                                        0x00000003
#define DRV_MB_PARAM_TRANSCEIVER_SIZE_OFFSET				2
#define DRV_MB_PARAM_TRANSCEIVER_SIZE_MASK \
                                                                        0x000000FC
#define DRV_MB_PARAM_TRANSCEIVER_I2C_ADDRESS_OFFSET			8
#define DRV_MB_PARAM_TRANSCEIVER_I2C_ADDRESS_MASK \
                                                                        0x0000FF00
#define DRV_MB_PARAM_TRANSCEIVER_OFFSET_OFFSET				16
#define DRV_MB_PARAM_TRANSCEIVER_OFFSET_MASK \
                                                                        0xFFFF0000

#define DRV_MB_PARAM_GPIO_NUMBER_OFFSET					0
#define DRV_MB_PARAM_GPIO_NUMBER_MASK \
                                                                        0x0000FFFF
#define DRV_MB_PARAM_GPIO_VALUE_OFFSET					16
#define DRV_MB_PARAM_GPIO_VALUE_MASK \
                                                                        0xFFFF0000
#define DRV_MB_PARAM_GPIO_DIRECTION_OFFSET				16
#define DRV_MB_PARAM_GPIO_DIRECTION_MASK \
                                                                        0x00FF0000
#define DRV_MB_PARAM_GPIO_CTRL_OFFSET					24
#define DRV_MB_PARAM_GPIO_CTRL_MASK \
                                                                        0xFF000000

/* Resource Allocation params - Driver version support*/
#define DRV_MB_PARAM_RESOURCE_ALLOC_VERSION_MAJOR_MASK \
                                                                        0xFFFF0000
#define DRV_MB_PARAM_RESOURCE_ALLOC_VERSION_MAJOR_OFFSET		16
#define DRV_MB_PARAM_RESOURCE_ALLOC_VERSION_MINOR_MASK \
                                                                        0x0000FFFF
#define DRV_MB_PARAM_RESOURCE_ALLOC_VERSION_MINOR_OFFSET		0

#define DRV_MB_PARAM_BIST_UNKNOWN_TEST					0
#define DRV_MB_PARAM_BIST_REGISTER_TEST					1
#define DRV_MB_PARAM_BIST_CLOCK_TEST					2
#define DRV_MB_PARAM_BIST_NVM_TEST_NUM_IMAGES				3
#define DRV_MB_PARAM_BIST_NVM_TEST_IMAGE_BY_INDEX			4

#define DRV_MB_PARAM_BIST_RC_UNKNOWN					0
#define DRV_MB_PARAM_BIST_RC_PASSED					1
#define DRV_MB_PARAM_BIST_RC_FAILED					2
#define DRV_MB_PARAM_BIST_RC_INVALID_PARAMETER				3

#define DRV_MB_PARAM_BIST_TEST_INDEX_OFFSET				0
#define DRV_MB_PARAM_BIST_TEST_INDEX_MASK \
                                                                        0x000000FF
#define DRV_MB_PARAM_BIST_TEST_IMAGE_INDEX_OFFSET			8
#define DRV_MB_PARAM_BIST_TEST_IMAGE_INDEX_MASK \
                                                                        0x0000FF00

#define DRV_MB_PARAM_FEATURE_SUPPORT_PORT_MASK \
                                                                        0x0000FFFF
#define DRV_MB_PARAM_FEATURE_SUPPORT_PORT_OFFSET			0
#define DRV_MB_PARAM_FEATURE_SUPPORT_PORT_SMARTLINQ \
                                                                        0x00000001	/* driver supports SmartLinQ parameter */
#define DRV_MB_PARAM_FEATURE_SUPPORT_PORT_EEE \
                                                                        0x00000002	/* driver supports EEE parameter */
#define DRV_MB_PARAM_FEATURE_SUPPORT_PORT_FEC_CONTROL \
                                                                        0x00000004	/* driver supports FEC Control parameter */
#define DRV_MB_PARAM_FEATURE_SUPPORT_PORT_EXT_SPEED_FEC_CONTROL \
                                                                        0x00000008	/* driver supports extended speed and FEC Control parameters */
#define DRV_MB_PARAM_FEATURE_SUPPORT_FUNC_MASK \
                                                                        0xFFFF0000
#define DRV_MB_PARAM_FEATURE_SUPPORT_FUNC_OFFSET			16
#define DRV_MB_PARAM_FEATURE_SUPPORT_FUNC_VLINK \
                                                                        0x00010000	/* driver supports virtual link parameter */

/* DRV_MSG_CODE_DEBUG_DATA_SEND parameters */
#define DRV_MSG_CODE_DEBUG_DATA_SEND_SIZE_OFFSET			0
#define DRV_MSG_CODE_DEBUG_DATA_SEND_SIZE_MASK				0xFF

/* Driver attributes params */
#define DRV_MB_PARAM_ATTRIBUTE_KEY_OFFSET				0
#define DRV_MB_PARAM_ATTRIBUTE_KEY_MASK \
                                                                        0x00FFFFFF
#define DRV_MB_PARAM_ATTRIBUTE_CMD_OFFSET				24
#define DRV_MB_PARAM_ATTRIBUTE_CMD_MASK \
                                                                        0xFF000000

#define DRV_MB_PARAM_NVM_CFG_OPTION_ID_OFFSET				0
#define DRV_MB_PARAM_NVM_CFG_OPTION_ID_MASK \
                                                                        0x0000FFFF	/* Option# */
#define DRV_MB_PARAM_NVM_CFG_OPTION_ID_IGNORE \
                                                                        0x0000FFFF
#define DRV_MB_PARAM_NVM_CFG_OPTION_ALL_OFFSET				16
#define DRV_MB_PARAM_NVM_CFG_OPTION_ALL_MASK \
                                                                        0x00010000	/* (Only for Set) Applies options value to all entities (port/func) depending on the option type */
#define DRV_MB_PARAM_NVM_CFG_OPTION_INIT_OFFSET				17
#define DRV_MB_PARAM_NVM_CFG_OPTION_INIT_MASK \
                                                                        0x00020000	/* When set, and state is IDLE, MFW will allocate resources and load configuration from NVM */
#define DRV_MB_PARAM_NVM_CFG_OPTION_COMMIT_OFFSET			18
#define DRV_MB_PARAM_NVM_CFG_OPTION_COMMIT_MASK \
                                                                        0x00040000	/* (Only for Set) - When set submit changed nvm_cfg1 to flash */
#define DRV_MB_PARAM_NVM_CFG_OPTION_FREE_OFFSET				19
#define DRV_MB_PARAM_NVM_CFG_OPTION_FREE_MASK \
                                                                        0x00080000	/* Free - When set, free allocated resources, and return to IDLE state. */
#define DRV_MB_PARAM_NVM_CFG_OPTION_ENTITY_SEL_OFFSET			20
#define DRV_MB_PARAM_NVM_CFG_OPTION_ENTITY_SEL_MASK \
                                                                        0x00100000	/* When set, command applies to the entity (func/port) in the ENTITY_ID field */
#define DRV_MB_PARAM_NVM_CFG_OPTION_DEFAULT_RESTORE_ALL_OFFSET		21
#define DRV_MB_PARAM_NVM_CFG_OPTION_DEFAULT_RESTORE_ALL_MASK \
                                                                        0x00200000	/* When set, the nvram restore to default */
#define DRV_MB_PARAM_NVM_CFG_OPTION_ENTITY_ID_OFFSET			24
#define DRV_MB_PARAM_NVM_CFG_OPTION_ENTITY_ID_MASK \
                                                                        0x0f000000	/* When ENTITY_SET is set, the command applies to this entity ID (func/port) selection */

#define SINGLE_NVM_WR_OP(optionId)	(((optionId &                             \
                                           DRV_MB_PARAM_NVM_CFG_OPTION_ID_MASK)   \
                                          <<                                      \
                                          DRV_MB_PARAM_NVM_CFG_OPTION_ID_OFFSET)  \
                                         |                                        \
                                         (DRV_MB_PARAM_NVM_CFG_OPTION_INIT_MASK   \
                                          |                                       \
                                          DRV_MB_PARAM_NVM_CFG_OPTION_COMMIT_MASK \
                                          |                                       \
                                          DRV_MB_PARAM_NVM_CFG_OPTION_FREE_MASK))
#define BEGIN_NVM_WR_OP(optionId)	(((optionId &                            \
                                           DRV_MB_PARAM_NVM_CFG_OPTION_ID_MASK)  \
                                          <<                                     \
                                          DRV_MB_PARAM_NVM_CFG_OPTION_ID_OFFSET) \
                                         |                                       \
                                         (DRV_MB_PARAM_NVM_CFG_OPTION_INIT_MASK))
#define CONT_NVM_WR_OP(optionId)	(((optionId &                           \
                                           DRV_MB_PARAM_NVM_CFG_OPTION_ID_MASK) \
                                          <<                                    \
                                          DRV_MB_PARAM_NVM_CFG_OPTION_ID_OFFSET))
#define END_NVM_WR_OP(optionId)		(((optionId &                                    \
                                           DRV_MB_PARAM_NVM_CFG_OPTION_ID_MASK)          \
                                          <<                                             \
                                          DRV_MB_PARAM_NVM_CFG_OPTION_ID_OFFSET)         \
                                         |                                               \
                                         (                                               \
                                                 DRV_MB_PARAM_NVM_CFG_OPTION_COMMIT_MASK \
                                                 |                                       \
                                                 DRV_MB_PARAM_NVM_CFG_OPTION_FREE_MASK))
#define DEFAULT_RESTORE_NVM_OP()	(                              \
                DRV_MB_PARAM_NVM_CFG_OPTION_DEFAULT_RESTORE_ALL_MASK | \
                DRV_MB_PARAM_NVM_CFG_OPTION_INIT_MASK |                \
                DRV_MB_PARAM_NVM_CFG_OPTION_COMMIT_MASK |              \
                DRV_MB_PARAM_NVM_CFG_OPTION_FREE_MASK)

/*DRV_MSG_CODE_GET_PERM_MAC parametres*/
#define DRV_MSG_CODE_GET_PERM_MAC_TYPE_OFFSET		0
#define DRV_MSG_CODE_GET_PERM_MAC_TYPE_MASK		0xF
#define DRV_MSG_CODE_GET_PERM_MAC_TYPE_PF		0
#define DRV_MSG_CODE_GET_PERM_MAC_TYPE_BMC		1
#define DRV_MSG_CODE_GET_PERM_MAC_TYPE_VF		2
#define DRV_MSG_CODE_GET_PERM_MAC_TYPE_LLDP		3
#define DRV_MSG_CODE_GET_PERM_MAC_TYPE_MAX		4
#define DRV_MSG_CODE_GET_PERM_MAC_INDEX_OFFSET		8
#define DRV_MSG_CODE_GET_PERM_MAC_INDEX_MASK		0xFFFF00

/***************************************************************/
/*                                                                                                                 */
/*                      Firmware Message Code (Response)			   */
/*                                                                                                                 */
/***************************************************************/

#define FW_MSG_CODE(_code_)    (_code_ << FW_MSG_CODE_OFFSET)
enum fw_msg_code_enum {
	FW_MSG_CODE_UNSUPPORTED = FW_MSG_CODE(0x0000),
	FW_MSG_CODE_NVM_OK = FW_MSG_CODE(0x0001),
	FW_MSG_CODE_NVM_INVALID_MODE = FW_MSG_CODE(0x0002),
	FW_MSG_CODE_NVM_PREV_CMD_WAS_NOT_FINISHED = FW_MSG_CODE(0x0003),
	FW_MSG_CODE_ATTRIBUTE_INVALID_KEY = FW_MSG_CODE(0x0002),
	FW_MSG_CODE_ATTRIBUTE_INVALID_CMD = FW_MSG_CODE(0x0003),
	FW_MSG_CODE_NVM_FAILED_TO_ALLOCATE_PAGE = FW_MSG_CODE(0x0004),
	FW_MSG_CODE_NVM_INVALID_DIR_FOUND = FW_MSG_CODE(0x0005),
	FW_MSG_CODE_NVM_PAGE_NOT_FOUND = FW_MSG_CODE(0x0006),
	FW_MSG_CODE_NVM_FAILED_PARSING_BNDLE_HEADER = FW_MSG_CODE(0x0007),
	FW_MSG_CODE_NVM_FAILED_PARSING_IMAGE_HEADER = FW_MSG_CODE(0x0008),
	FW_MSG_CODE_NVM_PARSING_OUT_OF_SYNC = FW_MSG_CODE(0x0009),
	FW_MSG_CODE_NVM_FAILED_UPDATING_DIR = FW_MSG_CODE(0x000a),
	FW_MSG_CODE_NVM_FAILED_TO_FREE_PAGE = FW_MSG_CODE(0x000b),
	FW_MSG_CODE_NVM_FILE_NOT_FOUND = FW_MSG_CODE(0x000c),
	FW_MSG_CODE_NVM_OPERATION_FAILED = FW_MSG_CODE(0x000d),
	FW_MSG_CODE_NVM_FAILED_UNALIGNED = FW_MSG_CODE(0x000e),
	FW_MSG_CODE_NVM_BAD_OFFSET = FW_MSG_CODE(0x000f),
	FW_MSG_CODE_NVM_BAD_SIGNATURE = FW_MSG_CODE(0x0010),
	FW_MSG_CODE_NVM_FILE_READ_ONLY = FW_MSG_CODE(0x0020),
	FW_MSG_CODE_NVM_UNKNOWN_FILE = FW_MSG_CODE(0x0030),
	FW_MSG_CODE_NVM_FAILED_CALC_HASH = FW_MSG_CODE(0x0031),
	FW_MSG_CODE_NVM_PUBLIC_KEY_MISSING = FW_MSG_CODE(0x0032),
	FW_MSG_CODE_NVM_INVALID_PUBLIC_KEY = FW_MSG_CODE(0x0033),
	FW_MSG_CODE_NVM_PUT_FILE_FINISH_OK = FW_MSG_CODE(0x0040),
	FW_MSG_CODE_UNVERIFIED_KEY_CHAIN = FW_MSG_CODE(0x0050),
	FW_MSG_CODE_MCP_RESET_REJECT = FW_MSG_CODE(0x0060),	/* MFW reject "mcp reset" command if one of the drivers is up */
	FW_MSG_CODE_PHY_OK = FW_MSG_CODE(0x0011),
	FW_MSG_CODE_PHY_ERROR = FW_MSG_CODE(0x0012),
	FW_MSG_CODE_SET_SECURE_MODE_ERROR = FW_MSG_CODE(0x0013),
	FW_MSG_CODE_SET_SECURE_MODE_OK = FW_MSG_CODE(0x0014),
	FW_MSG_MODE_PHY_PRIVILEGE_ERROR = FW_MSG_CODE(0x0015),
	FW_MSG_CODE_OK = FW_MSG_CODE(0x0016),
	FW_MSG_CODE_ERROR = FW_MSG_CODE(0x0017),
	FW_MSG_CODE_LED_MODE_INVALID = FW_MSG_CODE(0x0017),
	FW_MSG_CODE_PHY_DIAG_OK = FW_MSG_CODE(0x0016),
	FW_MSG_CODE_PHY_DIAG_ERROR = FW_MSG_CODE(0x0017),
	FW_MSG_CODE_INIT_HW_FAILED_TO_ALLOCATE_PAGE = FW_MSG_CODE(0x0004),
	FW_MSG_CODE_INIT_HW_FAILED_BAD_STATE = FW_MSG_CODE(0x0017),
	FW_MSG_CODE_INIT_HW_FAILED_TO_SET_WINDOW = FW_MSG_CODE(0x000d),
	FW_MSG_CODE_INIT_HW_FAILED_NO_IMAGE = FW_MSG_CODE(0x000c),
	FW_MSG_CODE_INIT_HW_FAILED_VERSION_MISMATCH = FW_MSG_CODE(0x0010),
	FW_MSG_CODE_INIT_HW_FAILED_UNALLOWED_ADDR = FW_MSG_CODE(0x0092),
	FW_MSG_CODE_TRANSCEIVER_DIAG_OK = FW_MSG_CODE(0x0016),
	FW_MSG_CODE_TRANSCEIVER_DIAG_ERROR = FW_MSG_CODE(0x0017),
	FW_MSG_CODE_TRANSCEIVER_NOT_PRESENT = FW_MSG_CODE(0x0002),
	FW_MSG_CODE_TRANSCEIVER_BAD_BUFFER_SIZE = FW_MSG_CODE(0x000f),
	FW_MSG_CODE_GPIO_OK = FW_MSG_CODE(0x0016),
	FW_MSG_CODE_GPIO_DIRECTION_ERR = FW_MSG_CODE(0x0017),
	FW_MSG_CODE_GPIO_CTRL_ERR = FW_MSG_CODE(0x0002),
	FW_MSG_CODE_GPIO_INVALID = FW_MSG_CODE(0x000f),
	FW_MSG_CODE_GPIO_INVALID_VALUE = FW_MSG_CODE(0x0005),
	FW_MSG_CODE_BIST_TEST_INVALID = FW_MSG_CODE(0x000f),
	FW_MSG_CODE_EXTPHY_INVALID_IMAGE_HEADER = FW_MSG_CODE(0x0070),
	FW_MSG_CODE_EXTPHY_INVALID_PHY_TYPE = FW_MSG_CODE(0x0071),
	FW_MSG_CODE_EXTPHY_OPERATION_FAILED = FW_MSG_CODE(0x0072),
	FW_MSG_CODE_EXTPHY_NO_PHY_DETECTED = FW_MSG_CODE(0x0073),
	FW_MSG_CODE_RECOVERY_MODE = FW_MSG_CODE(0x0074),
	FW_MSG_CODE_MDUMP_NO_IMAGE_FOUND = FW_MSG_CODE(0x0001),
	FW_MSG_CODE_MDUMP_ALLOC_FAILED = FW_MSG_CODE(0x0002),
	FW_MSG_CODE_MDUMP_INVALID_CMD = FW_MSG_CODE(0x0003),
	FW_MSG_CODE_MDUMP_IN_PROGRESS = FW_MSG_CODE(0x0004),
	FW_MSG_CODE_MDUMP_WRITE_FAILED = FW_MSG_CODE(0x0005),
	FW_MSG_CODE_OS_WOL_SUPPORTED = FW_MSG_CODE(0x0080),
	FW_MSG_CODE_OS_WOL_NOT_SUPPORTED = FW_MSG_CODE(0x0081),
	FW_MSG_CODE_WOL_READ_WRITE_OK = FW_MSG_CODE(0x0082),
	FW_MSG_CODE_WOL_READ_WRITE_INVALID_VAL = FW_MSG_CODE(0x0083),
	FW_MSG_CODE_WOL_READ_WRITE_INVALID_ADDR = FW_MSG_CODE(0x0084),
	FW_MSG_CODE_WOL_READ_BUFFER_OK = FW_MSG_CODE(0x0085),
	FW_MSG_CODE_WOL_READ_BUFFER_INVALID_VAL = FW_MSG_CODE(0x0086),
	FW_MSG_CODE_DRV_CFG_PF_VFS_MSIX_DONE = FW_MSG_CODE(0x0087),
	FW_MSG_CODE_DRV_CFG_PF_VFS_MSIX_BAD_ASIC = FW_MSG_CODE(0x0088),
	FW_MSG_CODE_RETAIN_VMAC_SUCCESS = FW_MSG_CODE(0x0089),
	FW_MSG_CODE_RETAIN_VMAC_FAIL = FW_MSG_CODE(0x008a),
	FW_MSG_CODE_ERR_RESOURCE_TEMPORARY_UNAVAILABLE = FW_MSG_CODE(0x008b),
	FW_MSG_CODE_ERR_RESOURCE_ALREADY_ALLOCATED = FW_MSG_CODE(0x008c),
	FW_MSG_CODE_ERR_RESOURCE_NOT_ALLOCATED = FW_MSG_CODE(0x008d),
	FW_MSG_CODE_ERR_NON_USER_OPTION = FW_MSG_CODE(0x008e),
	FW_MSG_CODE_ERR_UNKNOWN_OPTION = FW_MSG_CODE(0x008f),
	FW_MSG_CODE_WAIT = FW_MSG_CODE(0x0090),
	FW_MSG_CODE_BUSY = FW_MSG_CODE(0x0091),
	FW_MSG_CODE_FLR_ACK = FW_MSG_CODE(0x0200),
	FW_MSG_CODE_FLR_NACK = FW_MSG_CODE(0x0210),
	FW_MSG_CODE_SET_DRIVER_DONE = FW_MSG_CODE(0x0220),
	FW_MSG_CODE_SET_VMAC_SUCCESS = FW_MSG_CODE(0x0230),
	FW_MSG_CODE_SET_VMAC_FAIL = FW_MSG_CODE(0x0240),
	FW_MSG_CODE_DRV_LOAD_ENGINE = FW_MSG_CODE(0x1010),
	FW_MSG_CODE_DRV_LOAD_PORT = FW_MSG_CODE(0x1011),
	FW_MSG_CODE_DRV_LOAD_FUNCTION = FW_MSG_CODE(0x1012),
	FW_MSG_CODE_DRV_LOAD_REFUSED_PDA = FW_MSG_CODE(0x1020),
	FW_MSG_CODE_DRV_LOAD_REFUSED_HSI_1 = FW_MSG_CODE(0x1021),
	FW_MSG_CODE_DRV_LOAD_REFUSED_DIAG = FW_MSG_CODE(0x1022),
	FW_MSG_CODE_DRV_LOAD_REFUSED_HSI = FW_MSG_CODE(0x1023),
	FW_MSG_CODE_DRV_LOAD_REFUSED_REQUIRES_FORCE = FW_MSG_CODE(0x1030),
	FW_MSG_CODE_DRV_LOAD_REFUSED_REJECT = FW_MSG_CODE(0x1031),
	FW_MSG_CODE_DRV_LOAD_DONE = FW_MSG_CODE(0x1110),
	FW_MSG_CODE_DRV_UNLOAD_ENGINE = FW_MSG_CODE(0x2011),
	FW_MSG_CODE_DRV_UNLOAD_PORT = FW_MSG_CODE(0x2012),
	FW_MSG_CODE_DRV_UNLOAD_FUNCTION = FW_MSG_CODE(0x2013),
	FW_MSG_CODE_DRV_UNLOAD_DONE = FW_MSG_CODE(0x2110),
	FW_MSG_CODE_INIT_PHY_DONE = FW_MSG_CODE(0x2120),
	FW_MSG_CODE_INIT_PHY_ERR_INVALID_ARGS = FW_MSG_CODE(0x2130),
	FW_MSG_CODE_LINK_RESET_DONE = FW_MSG_CODE(0x2300),
	FW_MSG_CODE_SET_LLDP_DONE = FW_MSG_CODE(0x2400),
	FW_MSG_CODE_SET_LLDP_UNSUPPORTED_AGENT = FW_MSG_CODE(0x2401),
	FW_MSG_CODE_ERROR_EMBEDDED_LLDP_IS_DISABLED = FW_MSG_CODE(0x2402),
	FW_MSG_CODE_REGISTER_LLDP_TLVS_RX_DONE = FW_MSG_CODE(0x2410),
	FW_MSG_CODE_SET_DCBX_DONE = FW_MSG_CODE(0x2500),
	FW_MSG_CODE_UPDATE_CURR_CFG_DONE = FW_MSG_CODE(0x2600),
	FW_MSG_CODE_UPDATE_BUS_NUM_DONE = FW_MSG_CODE(0x2700),
	FW_MSG_CODE_UPDATE_BOOT_PROGRESS_DONE = FW_MSG_CODE(0x2800),
	FW_MSG_CODE_UPDATE_STORM_FW_VER_DONE = FW_MSG_CODE(0x2900),
	FW_MSG_CODE_UPDATE_DRIVER_STATE_DONE = FW_MSG_CODE(0x3100),
	FW_MSG_CODE_DRV_MSG_CODE_BW_UPDATE_DONE = FW_MSG_CODE(0x3200),
	FW_MSG_CODE_DRV_MSG_CODE_MTU_SIZE_DONE = FW_MSG_CODE(0x3300),
	FW_MSG_CODE_RESOURCE_ALLOC_OK = FW_MSG_CODE(0x3400),
	FW_MSG_CODE_RESOURCE_ALLOC_UNKNOWN = FW_MSG_CODE(0x3500),
	FW_MSG_CODE_RESOURCE_ALLOC_DEPRECATED = FW_MSG_CODE(0x3600),
	FW_MSG_CODE_RESOURCE_ALLOC_GEN_ERR = FW_MSG_CODE(0x3700),
	FW_MSG_CODE_UPDATE_WOL_DONE = FW_MSG_CODE(0x3800),
	FW_MSG_CODE_UPDATE_ESWITCH_MODE_DONE = FW_MSG_CODE(0x3900),
	FW_MSG_CODE_UPDATE_ERR = FW_MSG_CODE(0x3a01),
	FW_MSG_CODE_UPDATE_PARAM_ERR = FW_MSG_CODE(0x3a02),
	FW_MSG_CODE_UPDATE_NOT_ALLOWED = FW_MSG_CODE(0x3a03),
	FW_MSG_CODE_S_TAG_UPDATE_ACK_DONE = FW_MSG_CODE(0x3b00),
	FW_MSG_CODE_UPDATE_FCOE_CVID_DONE = FW_MSG_CODE(0x3c00),
	FW_MSG_CODE_UPDATE_FCOE_FABRIC_NAME_DONE = FW_MSG_CODE(0x3d00),
	FW_MSG_CODE_UPDATE_BOOT_CFG_DONE = FW_MSG_CODE(0x3e00),
	FW_MSG_CODE_RESET_TO_DEFAULT_ACK = FW_MSG_CODE(0x3f00),
	FW_MSG_CODE_OV_GET_CURR_CFG_DONE = FW_MSG_CODE(0x4000),
	FW_MSG_CODE_GET_OEM_UPDATES_DONE = FW_MSG_CODE(0x4100),
	FW_MSG_CODE_GET_LLDP_STATS_DONE = FW_MSG_CODE(0x4200),
	FW_MSG_CODE_GET_LLDP_STATS_ERROR = FW_MSG_CODE(0x4201),
	FW_MSG_CODE_NIG_DRAIN_DONE = FW_MSG_CODE(0x3000),
	FW_MSG_CODE_VF_DISABLED_DONE = FW_MSG_CODE(0xb000),
	FW_MSG_CODE_DRV_CFG_VF_MSIX_DONE = FW_MSG_CODE(0xb001),
	FW_MSG_CODE_INITIATE_VF_FLR_BAD_PARAM = FW_MSG_CODE(0xb002),
	FW_MSG_CODE_INITIATE_VF_FLR_OK = FW_MSG_CODE(0xb003),
	FW_MSG_CODE_IDC_BUSY = FW_MSG_CODE(0x0001),
	FW_MSG_CODE_GET_PERM_MAC_WRONG_TYPE = FW_MSG_CODE(0xb004),
	FW_MSG_CODE_GET_PERM_MAC_WRONG_INDEX = FW_MSG_CODE(0xb005),
	FW_MSG_CODE_GET_PERM_MAC_OK = FW_MSG_CODE(0xb006),
	FW_MSG_CODE_DEBUG_DATA_SEND_INV_ARG = FW_MSG_CODE(0xb007),
	FW_MSG_CODE_DEBUG_DATA_SEND_BUF_FULL = FW_MSG_CODE(0xb008),
	FW_MSG_CODE_DEBUG_DATA_SEND_NO_BUF = FW_MSG_CODE(0xb009),
	FW_MSG_CODE_DEBUG_NOT_ENABLED = FW_MSG_CODE(0xb00a),
	FW_MSG_CODE_DEBUG_DATA_SEND_OK = FW_MSG_CODE(0xb00b),
};

/* Resource Allocation params - MFW version support */
#define FW_MB_PARAM_RESOURCE_ALLOC_VERSION_MAJOR_MASK		0xFFFF0000
#define FW_MB_PARAM_RESOURCE_ALLOC_VERSION_MAJOR_OFFSET		16
#define FW_MB_PARAM_RESOURCE_ALLOC_VERSION_MINOR_MASK		0x0000FFFF
#define FW_MB_PARAM_RESOURCE_ALLOC_VERSION_MINOR_OFFSET		0

/* get pf rdma protocol command response */
#define FW_MB_PARAM_GET_PF_RDMA_NONE				0x0
#define FW_MB_PARAM_GET_PF_RDMA_ROCE				0x1
#define FW_MB_PARAM_GET_PF_RDMA_IWARP				0x2
#define FW_MB_PARAM_GET_PF_RDMA_BOTH				0x3

/* get MFW feature support response */
#define FW_MB_PARAM_FEATURE_SUPPORT_SMARTLINQ			(1 << 0)	/* MFW supports SmartLinQ */
#define FW_MB_PARAM_FEATURE_SUPPORT_EEE				(1 << 1)	/* MFW supports EEE */
#define FW_MB_PARAM_FEATURE_SUPPORT_DRV_LOAD_TO			(1 << 2)	/* MFW supports DRV_LOAD Timeout */
#define FW_MB_PARAM_FEATURE_SUPPORT_LP_PRES_DET			(1 << 3)	/* MFW supports early detection of LP Presence */
#define FW_MB_PARAM_FEATURE_SUPPORT_RELAXED_ORD			(1 << 4)	/* MFW supports relaxed ordering setting */
#define FW_MB_PARAM_FEATURE_SUPPORT_FEC_CONTROL			(1 << 5)	/* MFW supports FEC control by driver */
#define FW_MB_PARAM_FEATURE_SUPPORT_EXT_SPEED_FEC_CONTROL	(1 << 6)	/* MFW supports extended speed and FEC Control parameters */
#define FW_MB_PARAM_FEATURE_SUPPORT_IGU_CLEANUP			(1 << 7)	/* MFW supports complete IGU cleanup upon FLR */
#define FW_MB_PARAM_FEATURE_SUPPORT_VF_DPM			(1 << 8)	/* MFW supports VF DPM (bug fixes) */
#define FW_MB_PARAM_FEATURE_SUPPORT_IDLE_CHK			(1 << 9)	/* MFW supports idleChk collection */
#define FW_MB_PARAM_FEATURE_SUPPORT_VLINK			(1 << 16)	/* MFW supports virtual link */
#define FW_MB_PARAM_FEATURE_SUPPORT_DISABLE_LLDP		(1 << 17)	/* MFW supports disabling embedded LLDP */
#define FW_MB_PARAM_FEATURE_SUPPORT_ENHANCED_SYS_LCK		(1 << 18)	/* MFW supports ESL : Enhanced System Lockdown */
#define FW_MB_PARAM_FEATURE_SUPPORT_RESTORE_DEFAULT_CFG		(1 << 19)	/* MFW supports restoring default values of NVM_CFG image */

/* get MFW management status response */
#define FW_MB_PARAM_MANAGEMENT_STATUS_LOCKDOWN_ENABLED		0x00000001	/* MFW ESL is active */

#define FW_MB_PARAM_LOAD_DONE_DID_EFUSE_ERROR			(1 << 0)

#define FW_MB_PARAM_OEM_UPDATE_MASK				0xFF
#define FW_MB_PARAM_OEM_UPDATE_OFFSET				0
#define FW_MB_PARAM_OEM_UPDATE_BW				0x01
#define FW_MB_PARAM_OEM_UPDATE_S_TAG				0x02
#define FW_MB_PARAM_OEM_UPDATE_CFG				0x04

#define FW_MB_PARAM_ENG_CFG_FIR_AFFIN_VALID_MASK		0x00000001
#define FW_MB_PARAM_ENG_CFG_FIR_AFFIN_VALID_OFFSET		0
#define FW_MB_PARAM_ENG_CFG_FIR_AFFIN_VALUE_MASK		0x00000002
#define FW_MB_PARAM_ENG_CFG_FIR_AFFIN_VALUE_OFFSET		1
#define FW_MB_PARAM_ENG_CFG_L2_AFFIN_VALID_MASK			0x00000004
#define FW_MB_PARAM_ENG_CFG_L2_AFFIN_VALID_OFFSET		2
#define FW_MB_PARAM_ENG_CFG_L2_AFFIN_VALUE_MASK			0x00000008
#define FW_MB_PARAM_ENG_CFG_L2_AFFIN_VALUE_OFFSET		3

#define FW_MB_PARAM_PPFID_BITMAP_MASK				0xFF
#define FW_MB_PARAM_PPFID_BITMAP_OFFSET				0

#define FW_MB_PARAM_NVM_PUT_FILE_REQ_OFFSET_MASK		0x00ffffff
#define FW_MB_PARAM_NVM_PUT_FILE_REQ_OFFSET_OFFSET		0
#define FW_MB_PARAM_NVM_PUT_FILE_REQ_SIZE_MASK			0xff000000
#define FW_MB_PARAM_NVM_PUT_FILE_REQ_SIZE_OFFSET		24

/* MFW - DRV MB */
/**********************************************************************
* Description
*   Incremental Aggregative
*   8-bit MFW counter per message
*   8-bit ack-counter per message
* Capabilities
*   Provides up to 256 aggregative message per type
*   Provides 4 message types in dword
*   Message type pointers to byte offset
*   Backward Compatibility by using sizeof for the counters.
*   No lock requires for 32bit messages
* Limitations:
* In case of messages greater than 32bit, a dedicated mechanism(e.g lock)
* is required to prevent data corruption.
**********************************************************************/
enum MFW_DRV_MSG_TYPE {
	MFW_DRV_MSG_LINK_CHANGE,
	MFW_DRV_MSG_FLR_FW_ACK_FAILED,
	MFW_DRV_MSG_VF_DISABLED,
	MFW_DRV_MSG_LLDP_DATA_UPDATED,
	MFW_DRV_MSG_DCBX_REMOTE_MIB_UPDATED,
	MFW_DRV_MSG_DCBX_OPERATIONAL_MIB_UPDATED,
	MFW_DRV_MSG_ERROR_RECOVERY,
	MFW_DRV_MSG_BW_UPDATE,
	MFW_DRV_MSG_S_TAG_UPDATE,
	MFW_DRV_MSG_GET_LAN_STATS,
	MFW_DRV_MSG_GET_FCOE_STATS,
	MFW_DRV_MSG_GET_ISCSI_STATS,
	MFW_DRV_MSG_GET_RDMA_STATS,
	MFW_DRV_MSG_FAILURE_DETECTED,
	MFW_DRV_MSG_TRANSCEIVER_STATE_CHANGE,
	MFW_DRV_MSG_CRITICAL_ERROR_OCCURRED,
	MFW_DRV_MSG_EEE_NEGOTIATION_COMPLETE,
	MFW_DRV_MSG_GET_TLV_REQ,
	MFW_DRV_MSG_OEM_CFG_UPDATE,
	MFW_DRV_MSG_LLDP_RECEIVED_TLVS_UPDATED,
	MFW_DRV_MSG_GENERIC_IDC,	/* Generic Inter Driver Communication message */
	MFW_DRV_MSG_XCVR_TX_FAULT,
	MFW_DRV_MSG_XCVR_RX_LOS,
	MFW_DRV_MSG_GET_FCOE_CAP,
	MFW_DRV_MSG_GEN_LINK_DUMP,
	MFW_DRV_MSG_GEN_IDLE_CHK,
	MFW_DRV_MSG_DCBX_ADMIN_CFG_APPLIED,
	MFW_DRV_MSG_MAX
};

#define MFW_DRV_MSG_MAX_DWORDS(msgs)	(((msgs - 1) >> 2) + 1)
#define MFW_DRV_MSG_DWORD(msg_id)	(msg_id >> 2)
#define MFW_DRV_MSG_OFFSET(msg_id)	((msg_id & 0x3) << 3)
#define MFW_DRV_MSG_MASK(msg_id)	(0xff << \
                                         MFW_DRV_MSG_OFFSET(msg_id))

#ifdef BIG_ENDIAN		/* Like MFW */
#define DRV_ACK_MSG(msg_p, msg_id)	(u8)((u8 *)msg_p)[msg_id]++;
#else
#define DRV_ACK_MSG(msg_p,                                           \
                    msg_id)		(u8)((u8 *)msg_p)[((msg_id & \
                                                            ~3) |    \
                                                           ((~msg_id) & 3))]++;
#endif

#define MFW_DRV_UPDATE(shmem_func,                                              \
                       msg_id)		(u8)((u8 *)(MFW_MB_P(shmem_func)->msg)) \
        [msg_id]++;

struct public_mfw_mb {
	u32 sup_msgs;		/* Assigend with MFW_DRV_MSG_MAX */
	u32 msg[MFW_DRV_MSG_MAX_DWORDS(MFW_DRV_MSG_MAX)];	/* Incremented by the MFW */
	u32 ack[MFW_DRV_MSG_MAX_DWORDS(MFW_DRV_MSG_MAX)];	/* Incremented by the driver */
};

/**************************************/
/*                                                                */
/*     P U B L I C       D A T A	  */
/*                                                                */
/**************************************/
enum public_sections {
	PUBLIC_DRV_MB,		/* Points to the first drv_mb of path0 */
	PUBLIC_MFW_MB,		/* Points to the first mfw_mb of path0 */
	PUBLIC_GLOBAL,
	PUBLIC_PATH,
	PUBLIC_PORT,
	PUBLIC_FUNC,
	PUBLIC_MAX_SECTIONS
};

struct drv_ver_info_stc {
	u32 ver;
	u8 name[32];
};

/* Runtime data needs about 1/2K. We use 2K to be on the safe side.
 * Please make sure data does not exceed this size.
 */
#define NUM_RUNTIME_DWORDS    16
struct drv_init_hw_stc {
	u32 init_hw_bitmask[NUM_RUNTIME_DWORDS];
	u32 init_hw_data[NUM_RUNTIME_DWORDS * 32];
};

struct mcp_public_data {
	/* The sections fields is an array */
	u32 num_sections;
	offsize_t sections[PUBLIC_MAX_SECTIONS];
	struct public_drv_mb drv_mb[MCP_GLOB_FUNC_MAX];
	struct public_mfw_mb mfw_mb[MCP_GLOB_FUNC_MAX];
	struct public_global global;
	struct public_path path[MCP_GLOB_PATH_MAX];
	struct public_port port[MCP_GLOB_PORT_MAX];
	struct public_func func[MCP_GLOB_FUNC_MAX];
};

#define I2C_TRANSCEIVER_ADDR			0xa0
#define MAX_I2C_TRANSACTION_SIZE		16
#define MAX_I2C_TRANSCEIVER_PAGE_SIZE		256

/* OCBB definitions */
enum tlvs {
	/* Category 1: Device Properties */
	DRV_TLV_CLP_STR,
	DRV_TLV_CLP_STR_CTD,
	/* Category 6: Device Configuration */
	DRV_TLV_SCSI_TO,
	DRV_TLV_R_T_TOV,
	DRV_TLV_R_A_TOV,
	DRV_TLV_E_D_TOV,
	DRV_TLV_CR_TOV,
	DRV_TLV_BOOT_TYPE,
	/* Category 8: Port Configuration */
	DRV_TLV_NPIV_ENABLED,
	/* Category 10: Function Configuration */
	DRV_TLV_FEATURE_FLAGS,
	DRV_TLV_LOCAL_ADMIN_ADDR,
	DRV_TLV_ADDITIONAL_MAC_ADDR_1,
	DRV_TLV_ADDITIONAL_MAC_ADDR_2,
	DRV_TLV_LSO_MAX_OFFLOAD_SIZE,
	DRV_TLV_LSO_MIN_SEGMENT_COUNT,
	DRV_TLV_PROMISCUOUS_MODE,
	DRV_TLV_TX_DESCRIPTORS_QUEUE_SIZE,
	DRV_TLV_RX_DESCRIPTORS_QUEUE_SIZE,
	DRV_TLV_NUM_OF_NET_QUEUE_VMQ_CFG,
	DRV_TLV_FLEX_NIC_OUTER_VLAN_ID,
	DRV_TLV_OS_DRIVER_STATES,
	DRV_TLV_PXE_BOOT_PROGRESS,
	/* Category 12: FC/FCoE Configuration */
	DRV_TLV_NPIV_STATE,
	DRV_TLV_NUM_OF_NPIV_IDS,
	DRV_TLV_SWITCH_NAME,
	DRV_TLV_SWITCH_PORT_NUM,
	DRV_TLV_SWITCH_PORT_ID,
	DRV_TLV_VENDOR_NAME,
	DRV_TLV_SWITCH_MODEL,
	DRV_TLV_SWITCH_FW_VER,
	DRV_TLV_QOS_PRIORITY_PER_802_1P,
	DRV_TLV_PORT_ALIAS,
	DRV_TLV_PORT_STATE,
	DRV_TLV_FIP_TX_DESCRIPTORS_QUEUE_SIZE,
	DRV_TLV_FCOE_RX_DESCRIPTORS_QUEUE_SIZE,
	DRV_TLV_LINK_FAILURE_COUNT,
	DRV_TLV_FCOE_BOOT_PROGRESS,
	/* Category 13: iSCSI Configuration */
	DRV_TLV_TARGET_LLMNR_ENABLED,
	DRV_TLV_HEADER_DIGEST_FLAG_ENABLED,
	DRV_TLV_DATA_DIGEST_FLAG_ENABLED,
	DRV_TLV_AUTHENTICATION_METHOD,
	DRV_TLV_ISCSI_BOOT_TARGET_PORTAL,
	DRV_TLV_MAX_FRAME_SIZE,
	DRV_TLV_PDU_TX_DESCRIPTORS_QUEUE_SIZE,
	DRV_TLV_PDU_RX_DESCRIPTORS_QUEUE_SIZE,
	DRV_TLV_ISCSI_BOOT_PROGRESS,
	/* Category 20: Device Data */
	DRV_TLV_PCIE_BUS_RX_UTILIZATION,
	DRV_TLV_PCIE_BUS_TX_UTILIZATION,
	DRV_TLV_DEVICE_CPU_CORES_UTILIZATION,
	DRV_TLV_LAST_VALID_DCC_TLV_RECEIVED,
	DRV_TLV_NCSI_RX_BYTES_RECEIVED,
	DRV_TLV_NCSI_TX_BYTES_SENT,
	/* Category 22: Base Port Data */
	DRV_TLV_RX_DISCARDS,
	DRV_TLV_RX_ERRORS,
	DRV_TLV_TX_ERRORS,
	DRV_TLV_TX_DISCARDS,
	DRV_TLV_RX_FRAMES_RECEIVED,
	DRV_TLV_TX_FRAMES_SENT,
	/* Category 23: FC/FCoE Port Data */
	DRV_TLV_RX_BROADCAST_PACKETS,
	DRV_TLV_TX_BROADCAST_PACKETS,
	/* Category 28: Base Function Data */
	DRV_TLV_NUM_OFFLOADED_CONNECTIONS_TCP_IPV4,
	DRV_TLV_NUM_OFFLOADED_CONNECTIONS_TCP_IPV6,
	DRV_TLV_TX_DESCRIPTOR_QUEUE_AVG_DEPTH,
	DRV_TLV_RX_DESCRIPTORS_QUEUE_AVG_DEPTH,
	DRV_TLV_PF_RX_FRAMES_RECEIVED,
	DRV_TLV_RX_BYTES_RECEIVED,
	DRV_TLV_PF_TX_FRAMES_SENT,
	DRV_TLV_TX_BYTES_SENT,
	DRV_TLV_IOV_OFFLOAD,
	DRV_TLV_PCI_ERRORS_CAP_ID,
	DRV_TLV_UNCORRECTABLE_ERROR_STATUS,
	DRV_TLV_UNCORRECTABLE_ERROR_MASK,
	DRV_TLV_CORRECTABLE_ERROR_STATUS,
	DRV_TLV_CORRECTABLE_ERROR_MASK,
	DRV_TLV_PCI_ERRORS_AECC_REGISTER,
	DRV_TLV_TX_QUEUES_EMPTY,
	DRV_TLV_RX_QUEUES_EMPTY,
	DRV_TLV_TX_QUEUES_FULL,
	DRV_TLV_RX_QUEUES_FULL,
	/* Category 29: FC/FCoE Function Data */
	DRV_TLV_FCOE_TX_DESCRIPTOR_QUEUE_AVG_DEPTH,
	DRV_TLV_FCOE_RX_DESCRIPTORS_QUEUE_AVG_DEPTH,
	DRV_TLV_FCOE_RX_FRAMES_RECEIVED,
	DRV_TLV_FCOE_RX_BYTES_RECEIVED,
	DRV_TLV_FCOE_TX_FRAMES_SENT,
	DRV_TLV_FCOE_TX_BYTES_SENT,
	DRV_TLV_CRC_ERROR_COUNT,
	DRV_TLV_CRC_ERROR_1_RECEIVED_SOURCE_FC_ID,
	DRV_TLV_CRC_ERROR_1_TIMESTAMP,
	DRV_TLV_CRC_ERROR_2_RECEIVED_SOURCE_FC_ID,
	DRV_TLV_CRC_ERROR_2_TIMESTAMP,
	DRV_TLV_CRC_ERROR_3_RECEIVED_SOURCE_FC_ID,
	DRV_TLV_CRC_ERROR_3_TIMESTAMP,
	DRV_TLV_CRC_ERROR_4_RECEIVED_SOURCE_FC_ID,
	DRV_TLV_CRC_ERROR_4_TIMESTAMP,
	DRV_TLV_CRC_ERROR_5_RECEIVED_SOURCE_FC_ID,
	DRV_TLV_CRC_ERROR_5_TIMESTAMP,
	DRV_TLV_LOSS_OF_SYNC_ERROR_COUNT,
	DRV_TLV_LOSS_OF_SIGNAL_ERRORS,
	DRV_TLV_PRIMITIVE_SEQUENCE_PROTOCOL_ERROR_COUNT,
	DRV_TLV_DISPARITY_ERROR_COUNT,
	DRV_TLV_CODE_VIOLATION_ERROR_COUNT,
	DRV_TLV_LAST_FLOGI_ISSUED_COMMON_PARAMETERS_WORD_1,
	DRV_TLV_LAST_FLOGI_ISSUED_COMMON_PARAMETERS_WORD_2,
	DRV_TLV_LAST_FLOGI_ISSUED_COMMON_PARAMETERS_WORD_3,
	DRV_TLV_LAST_FLOGI_ISSUED_COMMON_PARAMETERS_WORD_4,
	DRV_TLV_LAST_FLOGI_TIMESTAMP,
	DRV_TLV_LAST_FLOGI_ACC_COMMON_PARAMETERS_WORD_1,
	DRV_TLV_LAST_FLOGI_ACC_COMMON_PARAMETERS_WORD_2,
	DRV_TLV_LAST_FLOGI_ACC_COMMON_PARAMETERS_WORD_3,
	DRV_TLV_LAST_FLOGI_ACC_COMMON_PARAMETERS_WORD_4,
	DRV_TLV_LAST_FLOGI_ACC_TIMESTAMP,
	DRV_TLV_LAST_FLOGI_RJT,
	DRV_TLV_LAST_FLOGI_RJT_TIMESTAMP,
	DRV_TLV_FDISCS_SENT_COUNT,
	DRV_TLV_FDISC_ACCS_RECEIVED,
	DRV_TLV_FDISC_RJTS_RECEIVED,
	DRV_TLV_PLOGI_SENT_COUNT,
	DRV_TLV_PLOGI_ACCS_RECEIVED,
	DRV_TLV_PLOGI_RJTS_RECEIVED,
	DRV_TLV_PLOGI_1_SENT_DESTINATION_FC_ID,
	DRV_TLV_PLOGI_1_TIMESTAMP,
	DRV_TLV_PLOGI_2_SENT_DESTINATION_FC_ID,
	DRV_TLV_PLOGI_2_TIMESTAMP,
	DRV_TLV_PLOGI_3_SENT_DESTINATION_FC_ID,
	DRV_TLV_PLOGI_3_TIMESTAMP,
	DRV_TLV_PLOGI_4_SENT_DESTINATION_FC_ID,
	DRV_TLV_PLOGI_4_TIMESTAMP,
	DRV_TLV_PLOGI_5_SENT_DESTINATION_FC_ID,
	DRV_TLV_PLOGI_5_TIMESTAMP,
	DRV_TLV_PLOGI_1_ACC_RECEIVED_SOURCE_FC_ID,
	DRV_TLV_PLOGI_1_ACC_TIMESTAMP,
	DRV_TLV_PLOGI_2_ACC_RECEIVED_SOURCE_FC_ID,
	DRV_TLV_PLOGI_2_ACC_TIMESTAMP,
	DRV_TLV_PLOGI_3_ACC_RECEIVED_SOURCE_FC_ID,
	DRV_TLV_PLOGI_3_ACC_TIMESTAMP,
	DRV_TLV_PLOGI_4_ACC_RECEIVED_SOURCE_FC_ID,
	DRV_TLV_PLOGI_4_ACC_TIMESTAMP,
	DRV_TLV_PLOGI_5_ACC_RECEIVED_SOURCE_FC_ID,
	DRV_TLV_PLOGI_5_ACC_TIMESTAMP,
	DRV_TLV_LOGOS_ISSUED,
	DRV_TLV_LOGO_ACCS_RECEIVED,
	DRV_TLV_LOGO_RJTS_RECEIVED,
	DRV_TLV_LOGO_1_RECEIVED_SOURCE_FC_ID,
	DRV_TLV_LOGO_1_TIMESTAMP,
	DRV_TLV_LOGO_2_RECEIVED_SOURCE_FC_ID,
	DRV_TLV_LOGO_2_TIMESTAMP,
	DRV_TLV_LOGO_3_RECEIVED_SOURCE_FC_ID,
	DRV_TLV_LOGO_3_TIMESTAMP,
	DRV_TLV_LOGO_4_RECEIVED_SOURCE_FC_ID,
	DRV_TLV_LOGO_4_TIMESTAMP,
	DRV_TLV_LOGO_5_RECEIVED_SOURCE_FC_ID,
	DRV_TLV_LOGO_5_TIMESTAMP,
	DRV_TLV_LOGOS_RECEIVED,
	DRV_TLV_ACCS_ISSUED,
	DRV_TLV_PRLIS_ISSUED,
	DRV_TLV_ACCS_RECEIVED,
	DRV_TLV_ABTS_SENT_COUNT,
	DRV_TLV_ABTS_ACCS_RECEIVED,
	DRV_TLV_ABTS_RJTS_RECEIVED,
	DRV_TLV_ABTS_1_SENT_DESTINATION_FC_ID,
	DRV_TLV_ABTS_1_TIMESTAMP,
	DRV_TLV_ABTS_2_SENT_DESTINATION_FC_ID,
	DRV_TLV_ABTS_2_TIMESTAMP,
	DRV_TLV_ABTS_3_SENT_DESTINATION_FC_ID,
	DRV_TLV_ABTS_3_TIMESTAMP,
	DRV_TLV_ABTS_4_SENT_DESTINATION_FC_ID,
	DRV_TLV_ABTS_4_TIMESTAMP,
	DRV_TLV_ABTS_5_SENT_DESTINATION_FC_ID,
	DRV_TLV_ABTS_5_TIMESTAMP,
	DRV_TLV_RSCNS_RECEIVED,
	DRV_TLV_LAST_RSCN_RECEIVED_N_PORT_1,
	DRV_TLV_LAST_RSCN_RECEIVED_N_PORT_2,
	DRV_TLV_LAST_RSCN_RECEIVED_N_PORT_3,
	DRV_TLV_LAST_RSCN_RECEIVED_N_PORT_4,
	DRV_TLV_LUN_RESETS_ISSUED,
	DRV_TLV_ABORT_TASK_SETS_ISSUED,
	DRV_TLV_TPRLOS_SENT,
	DRV_TLV_NOS_SENT_COUNT,
	DRV_TLV_NOS_RECEIVED_COUNT,
	DRV_TLV_OLS_COUNT,
	DRV_TLV_LR_COUNT,
	DRV_TLV_LRR_COUNT,
	DRV_TLV_LIP_SENT_COUNT,
	DRV_TLV_LIP_RECEIVED_COUNT,
	DRV_TLV_EOFA_COUNT,
	DRV_TLV_EOFNI_COUNT,
	DRV_TLV_SCSI_STATUS_CHECK_CONDITION_COUNT,
	DRV_TLV_SCSI_STATUS_CONDITION_MET_COUNT,
	DRV_TLV_SCSI_STATUS_BUSY_COUNT,
	DRV_TLV_SCSI_STATUS_INTERMEDIATE_COUNT,
	DRV_TLV_SCSI_STATUS_INTERMEDIATE_CONDITION_MET_COUNT,
	DRV_TLV_SCSI_STATUS_RESERVATION_CONFLICT_COUNT,
	DRV_TLV_SCSI_STATUS_TASK_SET_FULL_COUNT,
	DRV_TLV_SCSI_STATUS_ACA_ACTIVE_COUNT,
	DRV_TLV_SCSI_STATUS_TASK_ABORTED_COUNT,
	DRV_TLV_SCSI_CHECK_CONDITION_1_RECEIVED_SK_ASC_ASCQ,
	DRV_TLV_SCSI_CHECK_1_TIMESTAMP,
	DRV_TLV_SCSI_CHECK_CONDITION_2_RECEIVED_SK_ASC_ASCQ,
	DRV_TLV_SCSI_CHECK_2_TIMESTAMP,
	DRV_TLV_SCSI_CHECK_CONDITION_3_RECEIVED_SK_ASC_ASCQ,
	DRV_TLV_SCSI_CHECK_3_TIMESTAMP,
	DRV_TLV_SCSI_CHECK_CONDITION_4_RECEIVED_SK_ASC_ASCQ,
	DRV_TLV_SCSI_CHECK_4_TIMESTAMP,
	DRV_TLV_SCSI_CHECK_CONDITION_5_RECEIVED_SK_ASC_ASCQ,
	DRV_TLV_SCSI_CHECK_5_TIMESTAMP,
	/* Category 30: iSCSI Function Data */
	DRV_TLV_PDU_TX_DESCRIPTOR_QUEUE_AVG_DEPTH,
	DRV_TLV_PDU_RX_DESCRIPTORS_QUEUE_AVG_DEPTH,
	DRV_TLV_ISCSI_PDU_RX_FRAMES_RECEIVED,
	DRV_TLV_ISCSI_PDU_RX_BYTES_RECEIVED,
	DRV_TLV_ISCSI_PDU_TX_FRAMES_SENT,
	DRV_TLV_ISCSI_PDU_TX_BYTES_SENT,
	DRV_TLV_RDMA_DRV_VERSION
};

#define I2C_DEV_ADDR_A2				0xa2
#define SFP_EEPROM_A2_TEMPERATURE_ADDR		0x60
#define SFP_EEPROM_A2_TEMPERATURE_SIZE		2
#define SFP_EEPROM_A2_VCC_ADDR			0x62
#define SFP_EEPROM_A2_VCC_SIZE			2
#define SFP_EEPROM_A2_TX_BIAS_ADDR		0x64
#define SFP_EEPROM_A2_TX_BIAS_SIZE		2
#define SFP_EEPROM_A2_TX_POWER_ADDR		0x66
#define SFP_EEPROM_A2_TX_POWER_SIZE		2
#define SFP_EEPROM_A2_RX_POWER_ADDR		0x68
#define SFP_EEPROM_A2_RX_POWER_SIZE		2

#define I2C_DEV_ADDR_A0				0xa0
#define QSFP_EEPROM_A0_TEMPERATURE_ADDR		0x16
#define QSFP_EEPROM_A0_TEMPERATURE_SIZE		2
#define QSFP_EEPROM_A0_VCC_ADDR			0x1a
#define QSFP_EEPROM_A0_VCC_SIZE			2
#define QSFP_EEPROM_A0_TX1_BIAS_ADDR		0x2a
#define QSFP_EEPROM_A0_TX1_BIAS_SIZE		2
#define QSFP_EEPROM_A0_TX1_POWER_ADDR		0x32
#define QSFP_EEPROM_A0_TX1_POWER_SIZE		2
#define QSFP_EEPROM_A0_RX1_POWER_ADDR		0x22
#define QSFP_EEPROM_A0_RX1_POWER_SIZE		2

/**************************************
*     eDiag NETWORK Mode (DON)
**************************************/

#define ETH_DON_TYPE				0x0911	/* NETWORK Mode for QeDiag */
#define ETH_DON_TRACE_TYPE			0x0912	/* NETWORK Mode Continous Trace */

#define	DON_RESP_UNKNOWN_CMD_ID			0x10	/* Response Error */

/* Op Codes, Response is Op Code+1 */

#define	DON_REG_READ_REQ_CMD_ID			0x11
#define	DON_REG_WRITE_REQ_CMD_ID		0x22
#define	DON_CHALLENGE_REQ_CMD_ID		0x33
#define	DON_NVM_READ_REQ_CMD_ID			0x44
#define	DON_BLOCK_READ_REQ_CMD_ID		0x55

#define	DON_MFW_MODE_TRACE_CONTINUOUS_ID	0x70

/****************************************************************************
*
* Name:        nvm_cfg.h
*
* Description: NVM config file - Generated file from nvm cfg excel.
*              DO NOT MODIFY !!!
*
* Created:     4/14/2021
*
****************************************************************************/

#define NVM_CFG_version				0x85542

#define NVM_CFG_new_option_seq			65

#define NVM_CFG_removed_option_seq		4

#define NVM_CFG_updated_value_seq		23

struct nvm_cfg_mac_address {
	u32 mac_addr_hi;
#define NVM_CFG_MAC_ADDRESS_HI_MASK		0x0000FFFF
#define NVM_CFG_MAC_ADDRESS_HI_OFFSET		0
	u32 mac_addr_lo;
};

/******************************************
* nvm_cfg1 structs
******************************************/
struct nvm_cfg1_glob {
	u32 generic_cont0;	/* 0x0 */
#define NVM_CFG1_GLOB_BOARD_SWAP_MASK \
	                                                                0x0000000F
#define NVM_CFG1_GLOB_BOARD_SWAP_OFFSET					0
#define NVM_CFG1_GLOB_BOARD_SWAP_NONE					0x0
#define NVM_CFG1_GLOB_BOARD_SWAP_PATH					0x1
#define NVM_CFG1_GLOB_BOARD_SWAP_PORT					0x2
#define NVM_CFG1_GLOB_BOARD_SWAP_BOTH					0x3
#define NVM_CFG1_GLOB_MF_MODE_MASK \
	                                                                0x00000FF0
#define NVM_CFG1_GLOB_MF_MODE_OFFSET					4
#define NVM_CFG1_GLOB_MF_MODE_MF_ALLOWED				0x0
#define NVM_CFG1_GLOB_MF_MODE_DEFAULT					0x1
#define NVM_CFG1_GLOB_MF_MODE_SPIO4					0x2
#define NVM_CFG1_GLOB_MF_MODE_NPAR1_0					0x3
#define NVM_CFG1_GLOB_MF_MODE_NPAR1_5					0x4
#define NVM_CFG1_GLOB_MF_MODE_NPAR2_0					0x5
#define NVM_CFG1_GLOB_MF_MODE_BD					0x6
#define NVM_CFG1_GLOB_MF_MODE_UFP					0x7
#define NVM_CFG1_GLOB_MF_MODE_DCI_NPAR					0x8
#define NVM_CFG1_GLOB_MF_MODE_QINQ					0x9
#define NVM_CFG1_GLOB_FAN_FAILURE_ENFORCEMENT_MASK \
	                                                                0x00001000
#define NVM_CFG1_GLOB_FAN_FAILURE_ENFORCEMENT_OFFSET			12
#define NVM_CFG1_GLOB_FAN_FAILURE_ENFORCEMENT_DISABLED			0x0
#define NVM_CFG1_GLOB_FAN_FAILURE_ENFORCEMENT_ENABLED			0x1
#define NVM_CFG1_GLOB_AVS_MARGIN_LOW_MASK \
	                                                                0x001FE000
#define NVM_CFG1_GLOB_AVS_MARGIN_LOW_OFFSET				13
#define NVM_CFG1_GLOB_AVS_MARGIN_HIGH_MASK \
	                                                                0x1FE00000
#define NVM_CFG1_GLOB_AVS_MARGIN_HIGH_OFFSET				21
#define NVM_CFG1_GLOB_ENABLE_SRIOV_MASK \
	                                                                0x20000000
#define NVM_CFG1_GLOB_ENABLE_SRIOV_OFFSET				29
#define NVM_CFG1_GLOB_ENABLE_SRIOV_DISABLED				0x0
#define NVM_CFG1_GLOB_ENABLE_SRIOV_ENABLED				0x1
#define NVM_CFG1_GLOB_ENABLE_ATC_MASK \
	                                                                0x40000000
#define NVM_CFG1_GLOB_ENABLE_ATC_OFFSET					30
#define NVM_CFG1_GLOB_ENABLE_ATC_DISABLED				0x0
#define NVM_CFG1_GLOB_ENABLE_ATC_ENABLED				0x1
#define NVM_CFG1_GLOB_RESERVED__M_WAS_CLOCK_SLOWDOWN_MASK \
	                                                                0x80000000
#define NVM_CFG1_GLOB_RESERVED__M_WAS_CLOCK_SLOWDOWN_OFFSET		31
#define NVM_CFG1_GLOB_RESERVED__M_WAS_CLOCK_SLOWDOWN_DISABLED		0x0
#define NVM_CFG1_GLOB_RESERVED__M_WAS_CLOCK_SLOWDOWN_ENABLED		0x1
	u32 engineering_change[3];	/* 0x4 */
	u32 manufacturing_id;	/* 0x10 */
	u32 serial_number[4];	/* 0x14 */
	u32 pcie_cfg;		/* 0x24 */
#define NVM_CFG1_GLOB_PCI_GEN_MASK \
	                                                                0x00000003
#define NVM_CFG1_GLOB_PCI_GEN_OFFSET					0
#define NVM_CFG1_GLOB_PCI_GEN_PCI_GEN1					0x0
#define NVM_CFG1_GLOB_PCI_GEN_PCI_GEN2					0x1
#define NVM_CFG1_GLOB_PCI_GEN_PCI_GEN3					0x2
#define NVM_CFG1_GLOB_PCI_GEN_AHP_PCI_GEN4				0x3
#define NVM_CFG1_GLOB_BEACON_WOL_ENABLED_MASK \
	                                                                0x00000004
#define NVM_CFG1_GLOB_BEACON_WOL_ENABLED_OFFSET				2
#define NVM_CFG1_GLOB_BEACON_WOL_ENABLED_DISABLED			0x0
#define NVM_CFG1_GLOB_BEACON_WOL_ENABLED_ENABLED			0x1
#define NVM_CFG1_GLOB_ASPM_SUPPORT_MASK \
	                                                                0x00000018
#define NVM_CFG1_GLOB_ASPM_SUPPORT_OFFSET				3
#define NVM_CFG1_GLOB_ASPM_SUPPORT_L0S_L1_ENABLED			0x0
#define NVM_CFG1_GLOB_ASPM_SUPPORT_L0S_DISABLED				0x1
#define NVM_CFG1_GLOB_ASPM_SUPPORT_L1_DISABLED				0x2
#define NVM_CFG1_GLOB_ASPM_SUPPORT_L0S_L1_DISABLED			0x3
#define NVM_CFG1_GLOB_RESERVED_MPREVENT_PCIE_L1_MENTRY_MASK \
	                                                                0x00000020
#define NVM_CFG1_GLOB_RESERVED_MPREVENT_PCIE_L1_MENTRY_OFFSET		5
#define NVM_CFG1_GLOB_PCIE_G2_TX_AMPLITUDE_MASK \
	                                                                0x000003C0
#define NVM_CFG1_GLOB_PCIE_G2_TX_AMPLITUDE_OFFSET			6
#define NVM_CFG1_GLOB_PCIE_PREEMPHASIS_MASK \
	                                                                0x00001C00
#define NVM_CFG1_GLOB_PCIE_PREEMPHASIS_OFFSET				10
#define NVM_CFG1_GLOB_PCIE_PREEMPHASIS_HW				0x0
#define NVM_CFG1_GLOB_PCIE_PREEMPHASIS_0DB				0x1
#define NVM_CFG1_GLOB_PCIE_PREEMPHASIS_3_5DB				0x2
#define NVM_CFG1_GLOB_PCIE_PREEMPHASIS_6_0DB				0x3
#define NVM_CFG1_GLOB_WWN_NODE_PREFIX0_MASK \
	                                                                0x001FE000
#define NVM_CFG1_GLOB_WWN_NODE_PREFIX0_OFFSET				13
#define NVM_CFG1_GLOB_WWN_NODE_PREFIX1_MASK \
	                                                                0x1FE00000
#define NVM_CFG1_GLOB_WWN_NODE_PREFIX1_OFFSET				21
#define NVM_CFG1_GLOB_NCSI_PACKAGE_ID_MASK \
	                                                                0x60000000
#define NVM_CFG1_GLOB_NCSI_PACKAGE_ID_OFFSET				29
	/*  Set the duration, in seconds, fan failure signal should be
	 * sampled */
#define NVM_CFG1_GLOB_RESERVED_FAN_FAILURE_DURATION_MASK \
	                                                                0x80000000
#define NVM_CFG1_GLOB_RESERVED_FAN_FAILURE_DURATION_OFFSET		31
	u32 mgmt_traffic;	/* 0x28 */
#define NVM_CFG1_GLOB_RESERVED60_MASK \
	                                                                0x00000001
#define NVM_CFG1_GLOB_RESERVED60_OFFSET					0
#define NVM_CFG1_GLOB_WWN_PORT_PREFIX0_MASK \
	                                                                0x000001FE
#define NVM_CFG1_GLOB_WWN_PORT_PREFIX0_OFFSET				1
#define NVM_CFG1_GLOB_WWN_PORT_PREFIX1_MASK \
	                                                                0x0001FE00
#define NVM_CFG1_GLOB_WWN_PORT_PREFIX1_OFFSET				9
#define NVM_CFG1_GLOB_SMBUS_ADDRESS_MASK \
	                                                                0x01FE0000
#define NVM_CFG1_GLOB_SMBUS_ADDRESS_OFFSET				17
#define NVM_CFG1_GLOB_SIDEBAND_MODE_MASK \
	                                                                0x06000000
#define NVM_CFG1_GLOB_SIDEBAND_MODE_OFFSET				25
#define NVM_CFG1_GLOB_SIDEBAND_MODE_DISABLED				0x0
#define NVM_CFG1_GLOB_SIDEBAND_MODE_RMII				0x1
#define NVM_CFG1_GLOB_SIDEBAND_MODE_SGMII				0x2
#define NVM_CFG1_GLOB_AUX_MODE_MASK \
	                                                                0x78000000
#define NVM_CFG1_GLOB_AUX_MODE_OFFSET					27
#define NVM_CFG1_GLOB_AUX_MODE_DEFAULT					0x0
#define NVM_CFG1_GLOB_AUX_MODE_SMBUS_ONLY				0x1
	/*  Indicates whether external thermal sonsor is available */
#define NVM_CFG1_GLOB_EXTERNAL_THERMAL_SENSOR_MASK \
	                                                                0x80000000
#define NVM_CFG1_GLOB_EXTERNAL_THERMAL_SENSOR_OFFSET			31
#define NVM_CFG1_GLOB_EXTERNAL_THERMAL_SENSOR_DISABLED			0x0
#define NVM_CFG1_GLOB_EXTERNAL_THERMAL_SENSOR_ENABLED			0x1
	u32 core_cfg;		/* 0x2C */
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_OFFSET				0
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_2X40G			0x0
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X50G				0x1
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_1X100G			0x2
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_4X10G_F				0x3
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_4X10G_E			0x4
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_4X20G			0x5
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_1X40G				0xB
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X25G				0xC
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_1X25G				0xD
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_4X25G				0xE
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X10G				0xF
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X25G_LIO2			0x10
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_AHP_2X50G_R1			0x11
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_AHP_4X50G_R1			0x12
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_AHP_1X100G_R2			0x13
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_AHP_2X100G_R2			0x14
#define NVM_CFG1_GLOB_NETWORK_PORT_MODE_AHP_1X100G_R4			0x15
#define NVM_CFG1_GLOB_MPS10_ENFORCE_TX_FIR_CFG_MASK \
	                                                                0x00000100
#define NVM_CFG1_GLOB_MPS10_ENFORCE_TX_FIR_CFG_OFFSET			8
#define NVM_CFG1_GLOB_MPS10_ENFORCE_TX_FIR_CFG_DISABLED			0x0
#define NVM_CFG1_GLOB_MPS10_ENFORCE_TX_FIR_CFG_ENABLED			0x1
#define NVM_CFG1_GLOB_MPS25_ENFORCE_TX_FIR_CFG_MASK \
	                                                                0x00000200
#define NVM_CFG1_GLOB_MPS25_ENFORCE_TX_FIR_CFG_OFFSET			9
#define NVM_CFG1_GLOB_MPS25_ENFORCE_TX_FIR_CFG_DISABLED			0x0
#define NVM_CFG1_GLOB_MPS25_ENFORCE_TX_FIR_CFG_ENABLED			0x1
#define NVM_CFG1_GLOB_MPS10_CORE_ADDR_MASK \
	                                                                0x0003FC00
#define NVM_CFG1_GLOB_MPS10_CORE_ADDR_OFFSET				10
#define NVM_CFG1_GLOB_MPS25_CORE_ADDR_MASK \
	                                                                0x03FC0000
#define NVM_CFG1_GLOB_MPS25_CORE_ADDR_OFFSET				18
#define NVM_CFG1_GLOB_AVS_MODE_MASK \
	                                                                0x1C000000
#define NVM_CFG1_GLOB_AVS_MODE_OFFSET					26
#define NVM_CFG1_GLOB_AVS_MODE_CLOSE_LOOP				0x0
#define NVM_CFG1_GLOB_AVS_MODE_OPEN_LOOP_CFG				0x1
#define NVM_CFG1_GLOB_AVS_MODE_OPEN_LOOP_OTP				0x2
#define NVM_CFG1_GLOB_AVS_MODE_DISABLED					0x3
#define NVM_CFG1_GLOB_OVERRIDE_SECURE_MODE_MASK \
	                                                                0x60000000
#define NVM_CFG1_GLOB_OVERRIDE_SECURE_MODE_OFFSET			29
#define NVM_CFG1_GLOB_OVERRIDE_SECURE_MODE_DISABLED			0x0
#define NVM_CFG1_GLOB_OVERRIDE_SECURE_MODE_ENABLED			0x1
#define NVM_CFG1_GLOB_DCI_SUPPORT_MASK \
	                                                                0x80000000
#define NVM_CFG1_GLOB_DCI_SUPPORT_OFFSET				31
#define NVM_CFG1_GLOB_DCI_SUPPORT_DISABLED				0x0
#define NVM_CFG1_GLOB_DCI_SUPPORT_ENABLED				0x1
	u32 e_lane_cfg1;	/* 0x30 */
#define NVM_CFG1_GLOB_RX_LANE0_SWAP_MASK \
	                                                                0x0000000F
#define NVM_CFG1_GLOB_RX_LANE0_SWAP_OFFSET				0
#define NVM_CFG1_GLOB_RX_LANE1_SWAP_MASK \
	                                                                0x000000F0
#define NVM_CFG1_GLOB_RX_LANE1_SWAP_OFFSET				4
#define NVM_CFG1_GLOB_RX_LANE2_SWAP_MASK \
	                                                                0x00000F00
#define NVM_CFG1_GLOB_RX_LANE2_SWAP_OFFSET				8
#define NVM_CFG1_GLOB_RX_LANE3_SWAP_MASK \
	                                                                0x0000F000
#define NVM_CFG1_GLOB_RX_LANE3_SWAP_OFFSET				12
#define NVM_CFG1_GLOB_TX_LANE0_SWAP_MASK \
	                                                                0x000F0000
#define NVM_CFG1_GLOB_TX_LANE0_SWAP_OFFSET				16
#define NVM_CFG1_GLOB_TX_LANE1_SWAP_MASK \
	                                                                0x00F00000
#define NVM_CFG1_GLOB_TX_LANE1_SWAP_OFFSET				20
#define NVM_CFG1_GLOB_TX_LANE2_SWAP_MASK \
	                                                                0x0F000000
#define NVM_CFG1_GLOB_TX_LANE2_SWAP_OFFSET				24
#define NVM_CFG1_GLOB_TX_LANE3_SWAP_MASK \
	                                                                0xF0000000
#define NVM_CFG1_GLOB_TX_LANE3_SWAP_OFFSET				28
	u32 e_lane_cfg2;	/* 0x34 */
#define NVM_CFG1_GLOB_RX_LANE0_POL_FLIP_MASK \
	                                                                0x00000001
#define NVM_CFG1_GLOB_RX_LANE0_POL_FLIP_OFFSET				0
#define NVM_CFG1_GLOB_RX_LANE1_POL_FLIP_MASK \
	                                                                0x00000002
#define NVM_CFG1_GLOB_RX_LANE1_POL_FLIP_OFFSET				1
#define NVM_CFG1_GLOB_RX_LANE2_POL_FLIP_MASK \
	                                                                0x00000004
#define NVM_CFG1_GLOB_RX_LANE2_POL_FLIP_OFFSET				2
#define NVM_CFG1_GLOB_RX_LANE3_POL_FLIP_MASK \
	                                                                0x00000008
#define NVM_CFG1_GLOB_RX_LANE3_POL_FLIP_OFFSET				3
#define NVM_CFG1_GLOB_TX_LANE0_POL_FLIP_MASK \
	                                                                0x00000010
#define NVM_CFG1_GLOB_TX_LANE0_POL_FLIP_OFFSET				4
#define NVM_CFG1_GLOB_TX_LANE1_POL_FLIP_MASK \
	                                                                0x00000020
#define NVM_CFG1_GLOB_TX_LANE1_POL_FLIP_OFFSET				5
#define NVM_CFG1_GLOB_TX_LANE2_POL_FLIP_MASK \
	                                                                0x00000040
#define NVM_CFG1_GLOB_TX_LANE2_POL_FLIP_OFFSET				6
#define NVM_CFG1_GLOB_TX_LANE3_POL_FLIP_MASK \
	                                                                0x00000080
#define NVM_CFG1_GLOB_TX_LANE3_POL_FLIP_OFFSET				7
#define NVM_CFG1_GLOB_SMBUS_MODE_MASK \
	                                                                0x00000F00
#define NVM_CFG1_GLOB_SMBUS_MODE_OFFSET					8
#define NVM_CFG1_GLOB_SMBUS_MODE_DISABLED				0x0
#define NVM_CFG1_GLOB_SMBUS_MODE_100KHZ					0x1
#define NVM_CFG1_GLOB_SMBUS_MODE_400KHZ					0x2
#define NVM_CFG1_GLOB_NCSI_MASK \
	                                                                0x0000F000
#define NVM_CFG1_GLOB_NCSI_OFFSET					12
#define NVM_CFG1_GLOB_NCSI_DISABLED					0x0
#define NVM_CFG1_GLOB_NCSI_ENABLED					0x1
	/*  Maximum advertised pcie link width */
#define NVM_CFG1_GLOB_MAX_LINK_WIDTH_MASK \
	                                                                0x000F0000
#define NVM_CFG1_GLOB_MAX_LINK_WIDTH_OFFSET				16
#define NVM_CFG1_GLOB_MAX_LINK_WIDTH_BB_16_AHP_16_LANES			0x0
#define NVM_CFG1_GLOB_MAX_LINK_WIDTH_1_LANE				0x1
#define NVM_CFG1_GLOB_MAX_LINK_WIDTH_2_LANES				0x2
#define NVM_CFG1_GLOB_MAX_LINK_WIDTH_4_LANES				0x3
#define NVM_CFG1_GLOB_MAX_LINK_WIDTH_8_LANES				0x4
	/*  ASPM L1 mode */
#define NVM_CFG1_GLOB_ASPM_L1_MODE_MASK \
	                                                                0x00300000
#define NVM_CFG1_GLOB_ASPM_L1_MODE_OFFSET				20
#define NVM_CFG1_GLOB_ASPM_L1_MODE_FORCED				0x0
#define NVM_CFG1_GLOB_ASPM_L1_MODE_DYNAMIC_LOW_LATENCY			0x1
#define NVM_CFG1_GLOB_ON_CHIP_SENSOR_MODE_MASK \
	                                                                0x01C00000
#define NVM_CFG1_GLOB_ON_CHIP_SENSOR_MODE_OFFSET			22
#define NVM_CFG1_GLOB_ON_CHIP_SENSOR_MODE_DISABLED			0x0
#define NVM_CFG1_GLOB_ON_CHIP_SENSOR_MODE_INT_EXT_I2C			0x1
#define NVM_CFG1_GLOB_ON_CHIP_SENSOR_MODE_INT_ONLY			0x2
#define NVM_CFG1_GLOB_ON_CHIP_SENSOR_MODE_INT_EXT_SMBUS			0x3
#define NVM_CFG1_GLOB_TEMPERATURE_MONITORING_MODE_MASK \
	                                                                0x06000000
#define NVM_CFG1_GLOB_TEMPERATURE_MONITORING_MODE_OFFSET		25
#define NVM_CFG1_GLOB_TEMPERATURE_MONITORING_MODE_DISABLE		0x0
#define NVM_CFG1_GLOB_TEMPERATURE_MONITORING_MODE_INTERNAL		0x1
#define NVM_CFG1_GLOB_TEMPERATURE_MONITORING_MODE_EXTERNAL		0x2
#define NVM_CFG1_GLOB_TEMPERATURE_MONITORING_MODE_BOTH			0x3
	/*  Set the PLDM sensor modes */
#define NVM_CFG1_GLOB_PLDM_SENSOR_MODE_MASK \
	                                                                0x38000000
#define NVM_CFG1_GLOB_PLDM_SENSOR_MODE_OFFSET				27
#define NVM_CFG1_GLOB_PLDM_SENSOR_MODE_INTERNAL				0x0
#define NVM_CFG1_GLOB_PLDM_SENSOR_MODE_EXTERNAL				0x1
#define NVM_CFG1_GLOB_PLDM_SENSOR_MODE_BOTH				0x2
	/*  Enable VDM interface */
#define NVM_CFG1_GLOB_PCIE_VDM_ENABLED_MASK \
	                                                                0x40000000
#define NVM_CFG1_GLOB_PCIE_VDM_ENABLED_OFFSET				30
#define NVM_CFG1_GLOB_PCIE_VDM_ENABLED_DISABLED				0x0
#define NVM_CFG1_GLOB_PCIE_VDM_ENABLED_ENABLED				0x1
	/*  ROL enable */
#define NVM_CFG1_GLOB_RESET_ON_LAN_MASK \
	                                                                0x80000000
#define NVM_CFG1_GLOB_RESET_ON_LAN_OFFSET				31
#define NVM_CFG1_GLOB_RESET_ON_LAN_DISABLED				0x0
#define NVM_CFG1_GLOB_RESET_ON_LAN_ENABLED				0x1
	u32 f_lane_cfg1;	/* 0x38 */
#define NVM_CFG1_GLOB_RX_LANE0_SWAP_MASK \
	                                                                0x0000000F
#define NVM_CFG1_GLOB_RX_LANE0_SWAP_OFFSET				0
#define NVM_CFG1_GLOB_RX_LANE1_SWAP_MASK \
	                                                                0x000000F0
#define NVM_CFG1_GLOB_RX_LANE1_SWAP_OFFSET				4
#define NVM_CFG1_GLOB_RX_LANE2_SWAP_MASK \
	                                                                0x00000F00
#define NVM_CFG1_GLOB_RX_LANE2_SWAP_OFFSET				8
#define NVM_CFG1_GLOB_RX_LANE3_SWAP_MASK \
	                                                                0x0000F000
#define NVM_CFG1_GLOB_RX_LANE3_SWAP_OFFSET				12
#define NVM_CFG1_GLOB_TX_LANE0_SWAP_MASK \
	                                                                0x000F0000
#define NVM_CFG1_GLOB_TX_LANE0_SWAP_OFFSET				16
#define NVM_CFG1_GLOB_TX_LANE1_SWAP_MASK \
	                                                                0x00F00000
#define NVM_CFG1_GLOB_TX_LANE1_SWAP_OFFSET				20
#define NVM_CFG1_GLOB_TX_LANE2_SWAP_MASK \
	                                                                0x0F000000
#define NVM_CFG1_GLOB_TX_LANE2_SWAP_OFFSET				24
#define NVM_CFG1_GLOB_TX_LANE3_SWAP_MASK \
	                                                                0xF0000000
#define NVM_CFG1_GLOB_TX_LANE3_SWAP_OFFSET				28
	u32 f_lane_cfg2;	/* 0x3C */
#define NVM_CFG1_GLOB_RX_LANE0_POL_FLIP_MASK \
	                                                                0x00000001
#define NVM_CFG1_GLOB_RX_LANE0_POL_FLIP_OFFSET				0
#define NVM_CFG1_GLOB_RX_LANE1_POL_FLIP_MASK \
	                                                                0x00000002
#define NVM_CFG1_GLOB_RX_LANE1_POL_FLIP_OFFSET				1
#define NVM_CFG1_GLOB_RX_LANE2_POL_FLIP_MASK \
	                                                                0x00000004
#define NVM_CFG1_GLOB_RX_LANE2_POL_FLIP_OFFSET				2
#define NVM_CFG1_GLOB_RX_LANE3_POL_FLIP_MASK \
	                                                                0x00000008
#define NVM_CFG1_GLOB_RX_LANE3_POL_FLIP_OFFSET				3
#define NVM_CFG1_GLOB_TX_LANE0_POL_FLIP_MASK \
	                                                                0x00000010
#define NVM_CFG1_GLOB_TX_LANE0_POL_FLIP_OFFSET				4
#define NVM_CFG1_GLOB_TX_LANE1_POL_FLIP_MASK \
	                                                                0x00000020
#define NVM_CFG1_GLOB_TX_LANE1_POL_FLIP_OFFSET				5
#define NVM_CFG1_GLOB_TX_LANE2_POL_FLIP_MASK \
	                                                                0x00000040
#define NVM_CFG1_GLOB_TX_LANE2_POL_FLIP_OFFSET				6
#define NVM_CFG1_GLOB_TX_LANE3_POL_FLIP_MASK \
	                                                                0x00000080
#define NVM_CFG1_GLOB_TX_LANE3_POL_FLIP_OFFSET				7
	/*  Control the period between two successive checks */
#define NVM_CFG1_GLOB_TEMPERATURE_PERIOD_BETWEEN_CHECKS_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_TEMPERATURE_PERIOD_BETWEEN_CHECKS_OFFSET		8
	/*  Set shutdown temperature */
#define NVM_CFG1_GLOB_SHUTDOWN_THRESHOLD_TEMPERATURE_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_SHUTDOWN_THRESHOLD_TEMPERATURE_OFFSET		16
	/*  Set max. count for over operational temperature */
#define NVM_CFG1_GLOB_MAX_COUNT_OPER_THRESHOLD_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_MAX_COUNT_OPER_THRESHOLD_OFFSET			24
	u32 mps10_preemphasis;	/* 0x40 */
#define NVM_CFG1_GLOB_LANE0_PREEMP_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_LANE0_PREEMP_OFFSET				0
#define NVM_CFG1_GLOB_LANE1_PREEMP_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_LANE1_PREEMP_OFFSET				8
#define NVM_CFG1_GLOB_LANE2_PREEMP_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_LANE2_PREEMP_OFFSET				16
#define NVM_CFG1_GLOB_LANE3_PREEMP_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_LANE3_PREEMP_OFFSET				24
	u32 mps10_driver_current;	/* 0x44 */
#define NVM_CFG1_GLOB_LANE0_AMP_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_LANE0_AMP_OFFSET					0
#define NVM_CFG1_GLOB_LANE1_AMP_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_LANE1_AMP_OFFSET					8
#define NVM_CFG1_GLOB_LANE2_AMP_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_LANE2_AMP_OFFSET					16
#define NVM_CFG1_GLOB_LANE3_AMP_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_LANE3_AMP_OFFSET					24
	u32 mps25_preemphasis;	/* 0x48 */
#define NVM_CFG1_GLOB_LANE0_PREEMP_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_LANE0_PREEMP_OFFSET				0
#define NVM_CFG1_GLOB_LANE1_PREEMP_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_LANE1_PREEMP_OFFSET				8
#define NVM_CFG1_GLOB_LANE2_PREEMP_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_LANE2_PREEMP_OFFSET				16
#define NVM_CFG1_GLOB_LANE3_PREEMP_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_LANE3_PREEMP_OFFSET				24
	u32 mps25_driver_current;	/* 0x4C */
#define NVM_CFG1_GLOB_LANE0_AMP_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_LANE0_AMP_OFFSET					0
#define NVM_CFG1_GLOB_LANE1_AMP_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_LANE1_AMP_OFFSET					8
#define NVM_CFG1_GLOB_LANE2_AMP_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_LANE2_AMP_OFFSET					16
#define NVM_CFG1_GLOB_LANE3_AMP_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_LANE3_AMP_OFFSET					24
	u32 pci_id;		/* 0x50 */
#define NVM_CFG1_GLOB_VENDOR_ID_MASK \
	                                                                0x0000FFFF
#define NVM_CFG1_GLOB_VENDOR_ID_OFFSET					0
	/*  Set caution temperature */
#define NVM_CFG1_GLOB_DEAD_TEMP_TH_TEMPERATURE_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_DEAD_TEMP_TH_TEMPERATURE_OFFSET			16
	/*  Set external thermal sensor I2C address */
#define NVM_CFG1_GLOB_EXTERNAL_THERMAL_SENSOR_ADDRESS_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_EXTERNAL_THERMAL_SENSOR_ADDRESS_OFFSET		24
	u32 pci_subsys_id;	/* 0x54 */
#define NVM_CFG1_GLOB_SUBSYSTEM_VENDOR_ID_MASK \
	                                                                0x0000FFFF
#define NVM_CFG1_GLOB_SUBSYSTEM_VENDOR_ID_OFFSET			0
#define NVM_CFG1_GLOB_SUBSYSTEM_DEVICE_ID_MASK \
	                                                                0xFFFF0000
#define NVM_CFG1_GLOB_SUBSYSTEM_DEVICE_ID_OFFSET			16
	u32 bar;		/* 0x58 */
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_MASK \
	                                                                0x0000000F
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_OFFSET				0
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_DISABLED			0x0
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_2K				0x1
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_4K				0x2
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_8K				0x3
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_16K				0x4
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_32K				0x5
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_64K				0x6
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_128K				0x7
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_256K				0x8
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_512K				0x9
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_1M				0xA
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_2M				0xB
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_4M				0xC
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_8M				0xD
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_16M				0xE
#define NVM_CFG1_GLOB_EXPANSION_ROM_SIZE_32M				0xF
	/*  BB VF BAR2 size */
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_MASK \
	                                                                0x000000F0
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_OFFSET				4
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_DISABLED				0x0
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_4K				0x1
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_8K				0x2
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_16K				0x3
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_32K				0x4
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_64K				0x5
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_128K				0x6
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_256K				0x7
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_512K				0x8
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_1M				0x9
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_2M				0xA
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_4M				0xB
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_8M				0xC
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_16M				0xD
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_32M				0xE
#define NVM_CFG1_GLOB_VF_PCI_BAR2_SIZE_64M				0xF
	/*  BB BAR2 size (global) */
#define NVM_CFG1_GLOB_BAR2_SIZE_MASK \
	                                                                0x00000F00
#define NVM_CFG1_GLOB_BAR2_SIZE_OFFSET					8
#define NVM_CFG1_GLOB_BAR2_SIZE_DISABLED				0x0
#define NVM_CFG1_GLOB_BAR2_SIZE_64K					0x1
#define NVM_CFG1_GLOB_BAR2_SIZE_128K					0x2
#define NVM_CFG1_GLOB_BAR2_SIZE_256K					0x3
#define NVM_CFG1_GLOB_BAR2_SIZE_512K					0x4
#define NVM_CFG1_GLOB_BAR2_SIZE_1M					0x5
#define NVM_CFG1_GLOB_BAR2_SIZE_2M					0x6
#define NVM_CFG1_GLOB_BAR2_SIZE_4M					0x7
#define NVM_CFG1_GLOB_BAR2_SIZE_8M					0x8
#define NVM_CFG1_GLOB_BAR2_SIZE_16M					0x9
#define NVM_CFG1_GLOB_BAR2_SIZE_32M					0xA
#define NVM_CFG1_GLOB_BAR2_SIZE_64M					0xB
#define NVM_CFG1_GLOB_BAR2_SIZE_128M					0xC
#define NVM_CFG1_GLOB_BAR2_SIZE_256M					0xD
#define NVM_CFG1_GLOB_BAR2_SIZE_512M					0xE
#define NVM_CFG1_GLOB_BAR2_SIZE_1G					0xF
	/*  Set the duration, in seconds, fan failure signal should be
	 * sampled */
#define NVM_CFG1_GLOB_FAN_FAILURE_DURATION_MASK \
	                                                                0x0000F000
#define NVM_CFG1_GLOB_FAN_FAILURE_DURATION_OFFSET			12
	/*  This field defines the board total budget  for bar2 when disabled
	 * the regular bar size is used. */
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_OFFSET				16
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_DISABLED			0x0
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_64K				0x1
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_128K				0x2
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_256K				0x3
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_512K				0x4
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_1M				0x5
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_2M				0x6
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_4M				0x7
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_8M				0x8
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_16M				0x9
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_32M				0xA
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_64M				0xB
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_128M				0xC
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_256M				0xD
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_512M				0xE
#define NVM_CFG1_GLOB_BAR2_TOTAL_BUDGET_1G				0xF
	/*  Enable/Disable Crash dump triggers */
#define NVM_CFG1_GLOB_CRASH_DUMP_TRIGGER_ENABLE_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_CRASH_DUMP_TRIGGER_ENABLE_OFFSET			24
	u32 mps10_txfir_main;	/* 0x5C */
#define NVM_CFG1_GLOB_LANE0_TXFIR_MAIN_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_LANE0_TXFIR_MAIN_OFFSET				0
#define NVM_CFG1_GLOB_LANE1_TXFIR_MAIN_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_LANE1_TXFIR_MAIN_OFFSET				8
#define NVM_CFG1_GLOB_LANE2_TXFIR_MAIN_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_LANE2_TXFIR_MAIN_OFFSET				16
#define NVM_CFG1_GLOB_LANE3_TXFIR_MAIN_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_LANE3_TXFIR_MAIN_OFFSET				24
	u32 mps10_txfir_post;	/* 0x60 */
#define NVM_CFG1_GLOB_LANE0_TXFIR_POST_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_LANE0_TXFIR_POST_OFFSET				0
#define NVM_CFG1_GLOB_LANE1_TXFIR_POST_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_LANE1_TXFIR_POST_OFFSET				8
#define NVM_CFG1_GLOB_LANE2_TXFIR_POST_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_LANE2_TXFIR_POST_OFFSET				16
#define NVM_CFG1_GLOB_LANE3_TXFIR_POST_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_LANE3_TXFIR_POST_OFFSET				24
	u32 mps25_txfir_main;	/* 0x64 */
#define NVM_CFG1_GLOB_LANE0_TXFIR_MAIN_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_LANE0_TXFIR_MAIN_OFFSET				0
#define NVM_CFG1_GLOB_LANE1_TXFIR_MAIN_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_LANE1_TXFIR_MAIN_OFFSET				8
#define NVM_CFG1_GLOB_LANE2_TXFIR_MAIN_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_LANE2_TXFIR_MAIN_OFFSET				16
#define NVM_CFG1_GLOB_LANE3_TXFIR_MAIN_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_LANE3_TXFIR_MAIN_OFFSET				24
	u32 mps25_txfir_post;	/* 0x68 */
#define NVM_CFG1_GLOB_LANE0_TXFIR_POST_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_LANE0_TXFIR_POST_OFFSET				0
#define NVM_CFG1_GLOB_LANE1_TXFIR_POST_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_LANE1_TXFIR_POST_OFFSET				8
#define NVM_CFG1_GLOB_LANE2_TXFIR_POST_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_LANE2_TXFIR_POST_OFFSET				16
#define NVM_CFG1_GLOB_LANE3_TXFIR_POST_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_LANE3_TXFIR_POST_OFFSET				24
	u32 manufacture_ver;	/* 0x6C */
#define NVM_CFG1_GLOB_MANUF0_VER_MASK \
	                                                                0x0000003F
#define NVM_CFG1_GLOB_MANUF0_VER_OFFSET					0
#define NVM_CFG1_GLOB_MANUF1_VER_MASK \
	                                                                0x00000FC0
#define NVM_CFG1_GLOB_MANUF1_VER_OFFSET					6
#define NVM_CFG1_GLOB_MANUF2_VER_MASK \
	                                                                0x0003F000
#define NVM_CFG1_GLOB_MANUF2_VER_OFFSET					12
#define NVM_CFG1_GLOB_MANUF3_VER_MASK \
	                                                                0x00FC0000
#define NVM_CFG1_GLOB_MANUF3_VER_OFFSET					18
#define NVM_CFG1_GLOB_MANUF4_VER_MASK \
	                                                                0x3F000000
#define NVM_CFG1_GLOB_MANUF4_VER_OFFSET					24
	/*  Select package id method */
#define NVM_CFG1_GLOB_NCSI_PACKAGE_ID_IO_MASK \
	                                                                0x40000000
#define NVM_CFG1_GLOB_NCSI_PACKAGE_ID_IO_OFFSET				30
#define NVM_CFG1_GLOB_NCSI_PACKAGE_ID_IO_NVRAM				0x0
#define NVM_CFG1_GLOB_NCSI_PACKAGE_ID_IO_IO_PINS			0x1
#define NVM_CFG1_GLOB_RECOVERY_MODE_MASK \
	                                                                0x80000000
#define NVM_CFG1_GLOB_RECOVERY_MODE_OFFSET				31
#define NVM_CFG1_GLOB_RECOVERY_MODE_DISABLED				0x0
#define NVM_CFG1_GLOB_RECOVERY_MODE_ENABLED				0x1
	u32 manufacture_time;	/* 0x70 */
#define NVM_CFG1_GLOB_MANUF0_TIME_MASK \
	                                                                0x0000003F
#define NVM_CFG1_GLOB_MANUF0_TIME_OFFSET				0
#define NVM_CFG1_GLOB_MANUF1_TIME_MASK \
	                                                                0x00000FC0
#define NVM_CFG1_GLOB_MANUF1_TIME_OFFSET				6
#define NVM_CFG1_GLOB_MANUF2_TIME_MASK \
	                                                                0x0003F000
#define NVM_CFG1_GLOB_MANUF2_TIME_OFFSET				12
	/*  Max MSIX for Ethernet in default mode */
#define NVM_CFG1_GLOB_MAX_MSIX_MASK \
	                                                                0x03FC0000
#define NVM_CFG1_GLOB_MAX_MSIX_OFFSET					18
	/*  PF Mapping */
#define NVM_CFG1_GLOB_PF_MAPPING_MASK \
	                                                                0x0C000000
#define NVM_CFG1_GLOB_PF_MAPPING_OFFSET					26
#define NVM_CFG1_GLOB_PF_MAPPING_CONTINUOUS				0x0
#define NVM_CFG1_GLOB_PF_MAPPING_FIXED					0x1
#define NVM_CFG1_GLOB_VOLTAGE_REGULATOR_TYPE_MASK \
	                                                                0x30000000
#define NVM_CFG1_GLOB_VOLTAGE_REGULATOR_TYPE_OFFSET			28
#define NVM_CFG1_GLOB_VOLTAGE_REGULATOR_TYPE_DISABLED			0x0
#define NVM_CFG1_GLOB_VOLTAGE_REGULATOR_TYPE_TI				0x1
	/*  Enable/Disable PCIE Relaxed Ordering */
#define NVM_CFG1_GLOB_PCIE_RELAXED_ORDERING_MASK \
	                                                                0x40000000
#define NVM_CFG1_GLOB_PCIE_RELAXED_ORDERING_OFFSET			30
#define NVM_CFG1_GLOB_PCIE_RELAXED_ORDERING_DISABLED			0x0
#define NVM_CFG1_GLOB_PCIE_RELAXED_ORDERING_ENABLED			0x1
	/*  Reset the chip using iPOR to release PCIe due to short PERST
	 * issues */
#define NVM_CFG1_GLOB_SHORT_PERST_PROTECTION_MASK \
	                                                                0x80000000
#define NVM_CFG1_GLOB_SHORT_PERST_PROTECTION_OFFSET			31
#define NVM_CFG1_GLOB_SHORT_PERST_PROTECTION_DISABLED			0x0
#define NVM_CFG1_GLOB_SHORT_PERST_PROTECTION_ENABLED			0x1
	u32 led_global_settings;	/* 0x74 */
#define NVM_CFG1_GLOB_LED_SWAP_0_MASK \
	                                                                0x0000000F
#define NVM_CFG1_GLOB_LED_SWAP_0_OFFSET					0
#define NVM_CFG1_GLOB_LED_SWAP_1_MASK \
	                                                                0x000000F0
#define NVM_CFG1_GLOB_LED_SWAP_1_OFFSET					4
#define NVM_CFG1_GLOB_LED_SWAP_2_MASK \
	                                                                0x00000F00
#define NVM_CFG1_GLOB_LED_SWAP_2_OFFSET					8
#define NVM_CFG1_GLOB_LED_SWAP_3_MASK \
	                                                                0x0000F000
#define NVM_CFG1_GLOB_LED_SWAP_3_OFFSET					12
	/*  Max. continues operating temperature */
#define NVM_CFG1_GLOB_MAX_CONT_OPERATING_TEMP_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_MAX_CONT_OPERATING_TEMP_OFFSET			16
	/*  GPIO which triggers run-time port swap according to the map
	 * specified in option 205 */
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_OFFSET			24
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_NA				0x0
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO0			0x1
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO1			0x2
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO2			0x3
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO3			0x4
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO4			0x5
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO5			0x6
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO6			0x7
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO7			0x8
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO8			0x9
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO9			0xA
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO10			0xB
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO11			0xC
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO12			0xD
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO13			0xE
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO14			0xF
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO15			0x10
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO16			0x11
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO17			0x12
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO18			0x13
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO19			0x14
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO20			0x15
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO21			0x16
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO22			0x17
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO23			0x18
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO24			0x19
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO25			0x1A
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO26			0x1B
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO27			0x1C
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO28			0x1D
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO29			0x1E
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO30			0x1F
#define NVM_CFG1_GLOB_RUNTIME_PORT_SWAP_GPIO_GPIO31			0x20
	u32 generic_cont1;	/* 0x78 */
#define NVM_CFG1_GLOB_AVS_DAC_CODE_MASK \
	                                                                0x000003FF
#define NVM_CFG1_GLOB_AVS_DAC_CODE_OFFSET				0
#define NVM_CFG1_GLOB_LANE0_SWAP_MASK \
	                                                                0x00000C00
#define NVM_CFG1_GLOB_LANE0_SWAP_OFFSET					10
#define NVM_CFG1_GLOB_LANE1_SWAP_MASK \
	                                                                0x00003000
#define NVM_CFG1_GLOB_LANE1_SWAP_OFFSET					12
#define NVM_CFG1_GLOB_LANE2_SWAP_MASK \
	                                                                0x0000C000
#define NVM_CFG1_GLOB_LANE2_SWAP_OFFSET					14
#define NVM_CFG1_GLOB_LANE3_SWAP_MASK \
	                                                                0x00030000
#define NVM_CFG1_GLOB_LANE3_SWAP_OFFSET					16
	/*  Enable option 195 - Overriding the PCIe Preset value */
#define NVM_CFG1_GLOB_OVERRIDE_PCIE_PRESET_EQUAL_MASK \
	                                                                0x00040000
#define NVM_CFG1_GLOB_OVERRIDE_PCIE_PRESET_EQUAL_OFFSET			18
#define NVM_CFG1_GLOB_OVERRIDE_PCIE_PRESET_EQUAL_DISABLED		0x0
#define NVM_CFG1_GLOB_OVERRIDE_PCIE_PRESET_EQUAL_ENABLED		0x1
	/*  PCIe Preset value - applies only if option 194 is enabled */
#define NVM_CFG1_GLOB_PCIE_PRESET_VALUE_MASK \
	                                                                0x00780000
#define NVM_CFG1_GLOB_PCIE_PRESET_VALUE_OFFSET				19
	/*  Port mapping to be used when the run-time GPIO for port-swap is
	 * defined and set. */
#define NVM_CFG1_GLOB_RUNTIME_PORT0_SWAP_MAP_MASK \
	                                                                0x01800000
#define NVM_CFG1_GLOB_RUNTIME_PORT0_SWAP_MAP_OFFSET			23
#define NVM_CFG1_GLOB_RUNTIME_PORT1_SWAP_MAP_MASK \
	                                                                0x06000000
#define NVM_CFG1_GLOB_RUNTIME_PORT1_SWAP_MAP_OFFSET			25
#define NVM_CFG1_GLOB_RUNTIME_PORT2_SWAP_MAP_MASK \
	                                                                0x18000000
#define NVM_CFG1_GLOB_RUNTIME_PORT2_SWAP_MAP_OFFSET			27
#define NVM_CFG1_GLOB_RUNTIME_PORT3_SWAP_MAP_MASK \
	                                                                0x60000000
#define NVM_CFG1_GLOB_RUNTIME_PORT3_SWAP_MAP_OFFSET			29
	/*  Option to Disable embedded LLDP, 0 - Off, 1 - On */
#define NVM_CFG1_GLOB_LLDP_DISABLE_MASK \
	                                                                0x80000000
#define NVM_CFG1_GLOB_LLDP_DISABLE_OFFSET				31
#define NVM_CFG1_GLOB_LLDP_DISABLE_OFF					0x0
#define NVM_CFG1_GLOB_LLDP_DISABLE_ON					0x1
	u32 mbi_version;	/* 0x7C */
#define NVM_CFG1_GLOB_MBI_VERSION_0_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_MBI_VERSION_0_OFFSET				0
#define NVM_CFG1_GLOB_MBI_VERSION_1_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_MBI_VERSION_1_OFFSET				8
#define NVM_CFG1_GLOB_MBI_VERSION_2_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_MBI_VERSION_2_OFFSET				16
	/*  If set to other than NA, 0 - Normal operation, 1 - Thermal event
	 * occurred */
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_OFFSET				24
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_NA				0x0
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO0				0x1
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO1				0x2
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO2				0x3
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO3				0x4
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO4				0x5
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO5				0x6
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO6				0x7
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO7				0x8
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO8				0x9
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO9				0xA
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO10				0xB
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO11				0xC
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO12				0xD
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO13				0xE
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO14				0xF
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO15				0x10
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO16				0x11
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO17				0x12
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO18				0x13
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO19				0x14
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO20				0x15
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO21				0x16
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO22				0x17
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO23				0x18
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO24				0x19
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO25				0x1A
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO26				0x1B
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO27				0x1C
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO28				0x1D
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO29				0x1E
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO30				0x1F
#define NVM_CFG1_GLOB_THERMAL_EVENT_GPIO_GPIO31				0x20
	u32 mbi_date;		/* 0x80 */
	u32 misc_sig;		/* 0x84 */
	/*  Define the GPIO mapping to switch i2c mux */
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO_0_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO_0_OFFSET				0
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO_1_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO_1_OFFSET				8
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__NA				0x0
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO0				0x1
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO1				0x2
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO2				0x3
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO3				0x4
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO4				0x5
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO5				0x6
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO6				0x7
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO7				0x8
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO8				0x9
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO9				0xA
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO10				0xB
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO11				0xC
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO12				0xD
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO13				0xE
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO14				0xF
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO15				0x10
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO16				0x11
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO17				0x12
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO18				0x13
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO19				0x14
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO20				0x15
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO21				0x16
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO22				0x17
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO23				0x18
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO24				0x19
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO25				0x1A
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO26				0x1B
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO27				0x1C
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO28				0x1D
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO29				0x1E
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO30				0x1F
#define NVM_CFG1_GLOB_I2C_MUX_SEL_GPIO__GPIO31				0x20
	/*  Interrupt signal used for SMBus/I2C management interface
	 *
	 * 0 = Interrupt event occurred
	 * 1 = Normal
	 */
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_OFFSET				16
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_NA				0x0
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO0				0x1
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO1				0x2
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO2				0x3
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO3				0x4
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO4				0x5
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO5				0x6
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO6				0x7
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO7				0x8
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO8				0x9
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO9				0xA
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO10				0xB
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO11				0xC
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO12				0xD
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO13				0xE
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO14				0xF
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO15				0x10
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO16				0x11
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO17				0x12
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO18				0x13
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO19				0x14
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO20				0x15
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO21				0x16
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO22				0x17
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO23				0x18
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO24				0x19
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO25				0x1A
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO26				0x1B
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO27				0x1C
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO28				0x1D
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO29				0x1E
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO30				0x1F
#define NVM_CFG1_GLOB_I2C_INTERRUPT_GPIO_GPIO31				0x20
	/*  Set aLOM FAN on GPIO */
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_OFFSET			24
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_NA				0x0
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO0			0x1
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO1			0x2
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO2			0x3
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO3			0x4
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO4			0x5
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO5			0x6
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO6			0x7
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO7			0x8
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO8			0x9
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO9			0xA
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO10			0xB
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO11			0xC
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO12			0xD
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO13			0xE
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO14			0xF
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO15			0x10
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO16			0x11
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO17			0x12
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO18			0x13
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO19			0x14
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO20			0x15
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO21			0x16
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO22			0x17
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO23			0x18
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO24			0x19
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO25			0x1A
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO26			0x1B
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO27			0x1C
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO28			0x1D
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO29			0x1E
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO30			0x1F
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_GPIO_GPIO31			0x20
	u32 device_capabilities;	/* 0x88 */
#define NVM_CFG1_GLOB_DEVICE_CAPABILITIES_ETHERNET			0x1
#define NVM_CFG1_GLOB_DEVICE_CAPABILITIES_FCOE				0x2
#define NVM_CFG1_GLOB_DEVICE_CAPABILITIES_ISCSI				0x4
#define NVM_CFG1_GLOB_DEVICE_CAPABILITIES_ROCE				0x8
#define NVM_CFG1_GLOB_DEVICE_CAPABILITIES_IWARP				0x10
#define NVM_CFG1_GLOB_DEVICE_CAPABILITIES_NVMETCP			0x20
	u32 power_dissipated;	/* 0x8C */
#define NVM_CFG1_GLOB_POWER_DIS_D0_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_POWER_DIS_D0_OFFSET				0
#define NVM_CFG1_GLOB_POWER_DIS_D1_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_POWER_DIS_D1_OFFSET				8
#define NVM_CFG1_GLOB_POWER_DIS_D2_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_POWER_DIS_D2_OFFSET				16
#define NVM_CFG1_GLOB_POWER_DIS_D3_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_POWER_DIS_D3_OFFSET				24
	u32 power_consumed;	/* 0x90 */
#define NVM_CFG1_GLOB_POWER_CONS_D0_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_POWER_CONS_D0_OFFSET				0
#define NVM_CFG1_GLOB_POWER_CONS_D1_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_POWER_CONS_D1_OFFSET				8
#define NVM_CFG1_GLOB_POWER_CONS_D2_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_POWER_CONS_D2_OFFSET				16
#define NVM_CFG1_GLOB_POWER_CONS_D3_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_POWER_CONS_D3_OFFSET				24
	u32 efi_version;	/* 0x94 */
	u32 multi_network_modes_capability;	/* 0x98 */
#define NVM_CFG1_GLOB_MULTI_NETWORK_MODES_CAPABILITY_4X10G		0x1
#define NVM_CFG1_GLOB_MULTI_NETWORK_MODES_CAPABILITY_1X25G		0x2
#define NVM_CFG1_GLOB_MULTI_NETWORK_MODES_CAPABILITY_2X25G		0x4
#define NVM_CFG1_GLOB_MULTI_NETWORK_MODES_CAPABILITY_4X25G		0x8
#define NVM_CFG1_GLOB_MULTI_NETWORK_MODES_CAPABILITY_1X40G		0x10
#define NVM_CFG1_GLOB_MULTI_NETWORK_MODES_CAPABILITY_2X40G		0x20
#define NVM_CFG1_GLOB_MULTI_NETWORK_MODES_CAPABILITY_2X50G		0x40
#define NVM_CFG1_GLOB_MULTI_NETWORK_MODES_CAPABILITY_BB_1X100G		0x80
#define NVM_CFG1_GLOB_MULTI_NETWORK_MODES_CAPABILITY_2X10G		0x100
	u32 nvm_cfg_version;	/* 0x9C */
	u32 nvm_cfg_new_option_seq;	/* 0xA0 */
	u32 nvm_cfg_removed_option_seq;	/* 0xA4 */
	u32 nvm_cfg_updated_value_seq;	/* 0xA8 */
	u32 extended_serial_number[8];	/* 0xAC */
	u32 option_kit_pn[8];	/* 0xCC */
	u32 spare_pn[8];	/* 0xEC */
	u32 mps25_active_txfir_pre;	/* 0x10C */
#define NVM_CFG1_GLOB_LANE0_ACT_TXFIR_PRE_MASK			0x000000FF
#define NVM_CFG1_GLOB_LANE0_ACT_TXFIR_PRE_OFFSET		0
#define NVM_CFG1_GLOB_LANE1_ACT_TXFIR_PRE_MASK			0x0000FF00
#define NVM_CFG1_GLOB_LANE1_ACT_TXFIR_PRE_OFFSET		8
#define NVM_CFG1_GLOB_LANE2_ACT_TXFIR_PRE_MASK			0x00FF0000
#define NVM_CFG1_GLOB_LANE2_ACT_TXFIR_PRE_OFFSET		16
#define NVM_CFG1_GLOB_LANE3_ACT_TXFIR_PRE_MASK			0xFF000000
#define NVM_CFG1_GLOB_LANE3_ACT_TXFIR_PRE_OFFSET		24
	u32 mps25_active_txfir_main;	/* 0x110 */
#define NVM_CFG1_GLOB_LANE0_ACT_TXFIR_MAIN_MASK			0x000000FF
#define NVM_CFG1_GLOB_LANE0_ACT_TXFIR_MAIN_OFFSET		0
#define NVM_CFG1_GLOB_LANE1_ACT_TXFIR_MAIN_MASK			0x0000FF00
#define NVM_CFG1_GLOB_LANE1_ACT_TXFIR_MAIN_OFFSET		8
#define NVM_CFG1_GLOB_LANE2_ACT_TXFIR_MAIN_MASK			0x00FF0000
#define NVM_CFG1_GLOB_LANE2_ACT_TXFIR_MAIN_OFFSET		16
#define NVM_CFG1_GLOB_LANE3_ACT_TXFIR_MAIN_MASK			0xFF000000
#define NVM_CFG1_GLOB_LANE3_ACT_TXFIR_MAIN_OFFSET		24
	u32 mps25_active_txfir_post;	/* 0x114 */
#define NVM_CFG1_GLOB_LANE0_ACT_TXFIR_POST_MASK			0x000000FF
#define NVM_CFG1_GLOB_LANE0_ACT_TXFIR_POST_OFFSET		0
#define NVM_CFG1_GLOB_LANE1_ACT_TXFIR_POST_MASK			0x0000FF00
#define NVM_CFG1_GLOB_LANE1_ACT_TXFIR_POST_OFFSET		8
#define NVM_CFG1_GLOB_LANE2_ACT_TXFIR_POST_MASK			0x00FF0000
#define NVM_CFG1_GLOB_LANE2_ACT_TXFIR_POST_OFFSET		16
#define NVM_CFG1_GLOB_LANE3_ACT_TXFIR_POST_MASK			0xFF000000
#define NVM_CFG1_GLOB_LANE3_ACT_TXFIR_POST_OFFSET		24
	u32 features;		/* 0x118 */
	/*  Set the Aux Fan on temperature  */
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_VALUE_MASK		0x000000FF
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_VALUE_OFFSET		0
	/*  Set NC-SI package ID */
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_MASK				0x0000FF00
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_OFFSET			8
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_NA				0x0
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO0			0x1
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO1			0x2
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO2			0x3
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO3			0x4
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO4			0x5
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO5			0x6
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO6			0x7
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO7			0x8
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO8			0x9
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO9			0xA
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO10			0xB
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO11			0xC
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO12			0xD
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO13			0xE
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO14			0xF
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO15			0x10
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO16			0x11
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO17			0x12
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO18			0x13
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO19			0x14
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO20			0x15
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO21			0x16
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO22			0x17
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO23			0x18
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO24			0x19
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO25			0x1A
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO26			0x1B
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO27			0x1C
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO28			0x1D
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO29			0x1E
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO30			0x1F
#define NVM_CFG1_GLOB_SLOT_ID_GPIO_GPIO31			0x20
	/*  PMBUS Clock GPIO */
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_MASK			0x00FF0000
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_OFFSET			16
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_NA				0x0
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO0			0x1
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO1			0x2
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO2			0x3
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO3			0x4
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO4			0x5
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO5			0x6
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO6			0x7
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO7			0x8
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO8			0x9
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO9			0xA
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO10			0xB
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO11			0xC
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO12			0xD
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO13			0xE
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO14			0xF
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO15			0x10
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO16			0x11
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO17			0x12
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO18			0x13
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO19			0x14
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO20			0x15
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO21			0x16
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO22			0x17
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO23			0x18
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO24			0x19
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO25			0x1A
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO26			0x1B
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO27			0x1C
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO28			0x1D
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO29			0x1E
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO30			0x1F
#define NVM_CFG1_GLOB_PMBUS_SCL_GPIO_GPIO31			0x20
	/*  PMBUS Data GPIO */
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_MASK			0xFF000000
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_OFFSET			24
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_NA				0x0
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO0			0x1
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO1			0x2
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO2			0x3
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO3			0x4
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO4			0x5
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO5			0x6
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO6			0x7
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO7			0x8
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO8			0x9
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO9			0xA
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO10			0xB
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO11			0xC
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO12			0xD
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO13			0xE
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO14			0xF
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO15			0x10
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO16			0x11
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO17			0x12
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO18			0x13
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO19			0x14
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO20			0x15
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO21			0x16
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO22			0x17
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO23			0x18
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO24			0x19
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO25			0x1A
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO26			0x1B
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO27			0x1C
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO28			0x1D
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO29			0x1E
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO30			0x1F
#define NVM_CFG1_GLOB_PMBUS_SDA_GPIO_GPIO31			0x20
	u32 tx_rx_eq_25g_hlpc;	/* 0x11C */
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_25G_HLPC_MASK		0x000000FF
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_25G_HLPC_OFFSET		0
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_25G_HLPC_MASK		0x0000FF00
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_25G_HLPC_OFFSET		8
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_25G_HLPC_MASK		0x00FF0000
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_25G_HLPC_OFFSET		16
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_25G_HLPC_MASK		0xFF000000
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_25G_HLPC_OFFSET		24
	u32 tx_rx_eq_25g_llpc;	/* 0x120 */
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_25G_LLPC_MASK		0x000000FF
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_25G_LLPC_OFFSET		0
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_25G_LLPC_MASK		0x0000FF00
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_25G_LLPC_OFFSET		8
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_25G_LLPC_MASK		0x00FF0000
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_25G_LLPC_OFFSET		16
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_25G_LLPC_MASK		0xFF000000
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_25G_LLPC_OFFSET		24
	u32 tx_rx_eq_25g_ac;	/* 0x124 */
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_25G_AC_MASK		0x000000FF
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_25G_AC_OFFSET		0
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_25G_AC_MASK		0x0000FF00
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_25G_AC_OFFSET		8
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_25G_AC_MASK		0x00FF0000
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_25G_AC_OFFSET		16
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_25G_AC_MASK		0xFF000000
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_25G_AC_OFFSET		24
	u32 tx_rx_eq_10g_pc;	/* 0x128 */
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_10G_PC_MASK		0x000000FF
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_10G_PC_OFFSET		0
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_10G_PC_MASK		0x0000FF00
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_10G_PC_OFFSET		8
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_10G_PC_MASK		0x00FF0000
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_10G_PC_OFFSET		16
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_10G_PC_MASK		0xFF000000
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_10G_PC_OFFSET		24
	u32 tx_rx_eq_10g_ac;	/* 0x12C */
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_10G_AC_MASK		0x000000FF
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_10G_AC_OFFSET		0
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_10G_AC_MASK		0x0000FF00
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_10G_AC_OFFSET		8
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_10G_AC_MASK		0x00FF0000
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_10G_AC_OFFSET		16
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_10G_AC_MASK		0xFF000000
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_10G_AC_OFFSET		24
	u32 tx_rx_eq_1g;	/* 0x130 */
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_1G_MASK			0x000000FF
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_1G_OFFSET			0
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_1G_MASK			0x0000FF00
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_1G_OFFSET			8
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_1G_MASK			0x00FF0000
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_1G_OFFSET			16
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_1G_MASK			0xFF000000
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_1G_OFFSET			24
	u32 tx_rx_eq_25g_bt;	/* 0x134 */
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_25G_BT_MASK		0x000000FF
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_25G_BT_OFFSET		0
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_25G_BT_MASK		0x0000FF00
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_25G_BT_OFFSET		8
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_25G_BT_MASK		0x00FF0000
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_25G_BT_OFFSET		16
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_25G_BT_MASK		0xFF000000
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_25G_BT_OFFSET		24
	u32 tx_rx_eq_10g_bt;	/* 0x138 */
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_10G_BT_MASK		0x000000FF
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_10G_BT_OFFSET		0
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_10G_BT_MASK		0x0000FF00
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_10G_BT_OFFSET		8
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_10G_BT_MASK		0x00FF0000
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_10G_BT_OFFSET		16
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_10G_BT_MASK		0xFF000000
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_10G_BT_OFFSET		24
	u32 generic_cont4;	/* 0x13C */
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_MASK			0x000000FF
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_OFFSET			0
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_NA			0x0
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO0			0x1
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO1			0x2
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO2			0x3
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO3			0x4
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO4			0x5
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO5			0x6
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO6			0x7
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO7			0x8
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO8			0x9
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO9			0xA
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO10			0xB
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO11			0xC
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO12			0xD
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO13			0xE
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO14			0xF
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO15			0x10
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO16			0x11
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO17			0x12
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO18			0x13
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO19			0x14
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO20			0x15
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO21			0x16
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO22			0x17
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO23			0x18
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO24			0x19
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO25			0x1A
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO26			0x1B
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO27			0x1C
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO28			0x1D
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO29			0x1E
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO30			0x1F
#define NVM_CFG1_GLOB_THERMAL_ALARM_GPIO_GPIO31			0x20
	/*  Select the number of allowed port link in aux power */
#define NVM_CFG1_GLOB_NCSI_AUX_LINK_MASK			0x00000300
#define NVM_CFG1_GLOB_NCSI_AUX_LINK_OFFSET			8
#define NVM_CFG1_GLOB_NCSI_AUX_LINK_DEFAULT			0x0
#define NVM_CFG1_GLOB_NCSI_AUX_LINK_1_PORT			0x1
#define NVM_CFG1_GLOB_NCSI_AUX_LINK_2_PORTS			0x2
#define NVM_CFG1_GLOB_NCSI_AUX_LINK_3_PORTS			0x3
	/*  Set Trace Filter Log Level */
#define NVM_CFG1_GLOB_TRACE_LEVEL_MASK				0x00000C00
#define NVM_CFG1_GLOB_TRACE_LEVEL_OFFSET			10
#define NVM_CFG1_GLOB_TRACE_LEVEL_DEFAULT			0x0
#define NVM_CFG1_GLOB_TRACE_LEVEL_DEBUG				0x1
#define NVM_CFG1_GLOB_TRACE_LEVEL_TRACE				0x2
#define NVM_CFG1_GLOB_TRACE_LEVEL_ERROR				0x3
	/*  For OCP2.0, MFW listens on SMBUS slave address 0x3e, and return
	 * temperature reading */
#define NVM_CFG1_GLOB_EMULATED_TMP421_MASK			0x00001000
#define NVM_CFG1_GLOB_EMULATED_TMP421_OFFSET			12
#define NVM_CFG1_GLOB_EMULATED_TMP421_DISABLED			0x0
#define NVM_CFG1_GLOB_EMULATED_TMP421_ENABLED			0x1
	/*  GPIO which triggers when ASIC temperature reaches nvm option 286
	 * value */
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_MASK		0x001FE000
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_OFFSET		13
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_NA		0x0
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO0		0x1
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO1		0x2
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO2		0x3
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO3		0x4
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO4		0x5
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO5		0x6
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO6		0x7
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO7		0x8
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO8		0x9
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO9		0xA
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO10		0xB
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO11		0xC
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO12		0xD
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO13		0xE
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO14		0xF
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO15		0x10
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO16		0x11
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO17		0x12
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO18		0x13
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO19		0x14
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO20		0x15
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO21		0x16
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO22		0x17
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO23		0x18
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO24		0x19
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO25		0x1A
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO26		0x1B
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO27		0x1C
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO28		0x1D
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO29		0x1E
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO30		0x1F
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_GPIO_GPIO31		0x20
	/*  Warning temperature threshold used with nvm option 286 */
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_THRESHOLD_MASK	0x1FE00000
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_THRESHOLD_OFFSET	21
	/*  Disable PLDM protocol */
#define NVM_CFG1_GLOB_DISABLE_PLDM_MASK				0x20000000
#define NVM_CFG1_GLOB_DISABLE_PLDM_OFFSET			29
#define NVM_CFG1_GLOB_DISABLE_PLDM_DISABLED			0x0
#define NVM_CFG1_GLOB_DISABLE_PLDM_ENABLED			0x1
	/*  Disable OCBB protocol */
#define NVM_CFG1_GLOB_DISABLE_MCTP_OEM_MASK			0x40000000
#define NVM_CFG1_GLOB_DISABLE_MCTP_OEM_OFFSET			30
#define NVM_CFG1_GLOB_DISABLE_MCTP_OEM_DISABLED			0x0
#define NVM_CFG1_GLOB_DISABLE_MCTP_OEM_ENABLED			0x1
	/*  MCTP Virtual Link OEM3 */
#define NVM_CFG1_GLOB_MCTP_VIRTUAL_LINK_OEM3_MASK		0x80000000
#define NVM_CFG1_GLOB_MCTP_VIRTUAL_LINK_OEM3_OFFSET		31
#define NVM_CFG1_GLOB_MCTP_VIRTUAL_LINK_OEM3_DISABLED		0x0
#define NVM_CFG1_GLOB_MCTP_VIRTUAL_LINK_OEM3_ENABLED		0x1
	u32 preboot_debug_mode_std;	/* 0x140 */
	u32 preboot_debug_mode_ext;	/* 0x144 */
	u32 ext_phy_cfg1;	/* 0x148 */
	/*  Ext PHY MDI pair swap value */
#define NVM_CFG1_GLOB_RESERVED_244_MASK \
	                                                                0x0000FFFF
#define NVM_CFG1_GLOB_RESERVED_244_OFFSET				0
	/*  Define for PGOOD signal Mapping  for EXT PHY */
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_OFFSET				16
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_NA					0x0
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO0				0x1
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO1				0x2
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO2				0x3
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO3				0x4
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO4				0x5
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO5				0x6
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO6				0x7
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO7				0x8
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO8				0x9
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO9				0xA
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO10				0xB
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO11				0xC
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO12				0xD
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO13				0xE
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO14				0xF
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO15				0x10
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO16				0x11
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO17				0x12
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO18				0x13
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO19				0x14
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO20				0x15
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO21				0x16
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO22				0x17
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO23				0x18
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO24				0x19
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO25				0x1A
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO26				0x1B
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO27				0x1C
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO28				0x1D
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO29				0x1E
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO30				0x1F
#define NVM_CFG1_GLOB_EXT_PHY_PGOOD_GPIO31				0x20
	/*  GPIO which trigger when PERST asserted  */
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_OFFSET			24
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_NA				0x0
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO0			0x1
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO1			0x2
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO2			0x3
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO3			0x4
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO4			0x5
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO5			0x6
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO6			0x7
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO7			0x8
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO8			0x9
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO9			0xA
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO10			0xB
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO11			0xC
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO12			0xD
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO13			0xE
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO14			0xF
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO15			0x10
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO16			0x11
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO17			0x12
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO18			0x13
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO19			0x14
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO20			0x15
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO21			0x16
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO22			0x17
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO23			0x18
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO24			0x19
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO25			0x1A
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO26			0x1B
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO27			0x1C
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO28			0x1D
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO29			0x1E
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO30			0x1F
#define NVM_CFG1_GLOB_PERST_INDICATION_GPIO_GPIO31			0x20
	u32 clocks;		/* 0x14C */
	/*  Sets core clock frequency */
#define NVM_CFG1_GLOB_MAIN_CLOCK_FREQUENCY_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_MAIN_CLOCK_FREQUENCY_OFFSET			0
#define NVM_CFG1_GLOB_MAIN_CLOCK_FREQUENCY_MAIN_CLK_DEFAULT		0x0
#define NVM_CFG1_GLOB_MAIN_CLOCK_FREQUENCY_MAIN_CLK_375			0x1
#define NVM_CFG1_GLOB_MAIN_CLOCK_FREQUENCY_MAIN_CLK_350			0x2
#define NVM_CFG1_GLOB_MAIN_CLOCK_FREQUENCY_MAIN_CLK_325			0x3
#define NVM_CFG1_GLOB_MAIN_CLOCK_FREQUENCY_MAIN_CLK_300			0x4
#define NVM_CFG1_GLOB_MAIN_CLOCK_FREQUENCY_MAIN_CLK_280			0x5
	/*  Sets MAC clock frequency */
#define NVM_CFG1_GLOB_MAC_CLOCK_FREQUENCY_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_MAC_CLOCK_FREQUENCY_OFFSET			8
#define NVM_CFG1_GLOB_MAC_CLOCK_FREQUENCY_MAC_CLK_DEFAULT		0x0
#define NVM_CFG1_GLOB_MAC_CLOCK_FREQUENCY_MAC_CLK_782			0x1
#define NVM_CFG1_GLOB_MAC_CLOCK_FREQUENCY_MAC_CLK_516			0x2
	/*  Sets storm clock frequency */
#define NVM_CFG1_GLOB_STORM_CLOCK_FREQUENCY_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_STORM_CLOCK_FREQUENCY_OFFSET			16
#define NVM_CFG1_GLOB_STORM_CLOCK_FREQUENCY_STORM_CLK_DEFAULT		0x0
#define NVM_CFG1_GLOB_STORM_CLOCK_FREQUENCY_STORM_CLK_1200		0x1
#define NVM_CFG1_GLOB_STORM_CLOCK_FREQUENCY_STORM_CLK_1000		0x2
#define NVM_CFG1_GLOB_STORM_CLOCK_FREQUENCY_STORM_CLK_900		0x3
#define NVM_CFG1_GLOB_STORM_CLOCK_FREQUENCY_STORM_CLK_1100		0x4
	/*  Non zero value will override PCIe AGC threshold to improve
	 * receiver */
#define NVM_CFG1_GLOB_OVERRIDE_AGC_THRESHOLD_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_OVERRIDE_AGC_THRESHOLD_OFFSET			24
	u32 pre2_generic_cont_1;	/* 0x150 */
#define NVM_CFG1_GLOB_50G_HLPC_PRE2_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_50G_HLPC_PRE2_OFFSET				0
#define NVM_CFG1_GLOB_50G_MLPC_PRE2_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_50G_MLPC_PRE2_OFFSET				8
#define NVM_CFG1_GLOB_50G_LLPC_PRE2_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_50G_LLPC_PRE2_OFFSET				16
#define NVM_CFG1_GLOB_25G_HLPC_PRE2_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_25G_HLPC_PRE2_OFFSET				24
	u32 pre2_generic_cont_2;	/* 0x154 */
#define NVM_CFG1_GLOB_25G_LLPC_PRE2_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_25G_LLPC_PRE2_OFFSET				0
#define NVM_CFG1_GLOB_25G_AC_PRE2_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_25G_AC_PRE2_OFFSET				8
#define NVM_CFG1_GLOB_10G_PC_PRE2_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_10G_PC_PRE2_OFFSET				16
#define NVM_CFG1_GLOB_PRE2_10G_AC_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_PRE2_10G_AC_OFFSET				24
	u32 pre2_generic_cont_3;	/* 0x158 */
#define NVM_CFG1_GLOB_1G_PRE2_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_1G_PRE2_OFFSET					0
#define NVM_CFG1_GLOB_5G_BT_PRE2_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_5G_BT_PRE2_OFFSET					8
#define NVM_CFG1_GLOB_10G_BT_PRE2_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_10G_BT_PRE2_OFFSET				16
	/*  When temperature goes below (warning temperature - delta) warning
	 * gpio is unset */
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_DELTA_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_WARNING_TEMPERATURE_DELTA_OFFSET			24
	u32 tx_rx_eq_50g_hlpc;	/* 0x15C */
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_50G_HLPC_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_50G_HLPC_OFFSET			0
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_50G_HLPC_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_50G_HLPC_OFFSET			8
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_50G_HLPC_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_50G_HLPC_OFFSET			16
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_50G_HLPC_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_50G_HLPC_OFFSET			24
	u32 tx_rx_eq_50g_mlpc;	/* 0x160 */
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_50G_MLPC_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_50G_MLPC_OFFSET			0
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_50G_MLPC_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_50G_MLPC_OFFSET			8
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_50G_MLPC_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_50G_MLPC_OFFSET			16
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_50G_MLPC_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_50G_MLPC_OFFSET			24
	u32 tx_rx_eq_50g_llpc;	/* 0x164 */
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_50G_LLPC_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_50G_LLPC_OFFSET			0
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_50G_LLPC_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_50G_LLPC_OFFSET			8
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_50G_LLPC_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_50G_LLPC_OFFSET			16
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_50G_LLPC_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_50G_LLPC_OFFSET			24
	u32 tx_rx_eq_50g_ac;	/* 0x168 */
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_50G_AC_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_INDEX0_RX_TX_EQ_50G_AC_OFFSET			0
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_50G_AC_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_INDEX1_RX_TX_EQ_50G_AC_OFFSET			8
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_50G_AC_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_INDEX2_RX_TX_EQ_50G_AC_OFFSET			16
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_50G_AC_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_INDEX3_RX_TX_EQ_50G_AC_OFFSET			24
	/*  Set Trace Filter Modules Log Bit Mask */
	u32 trace_modules;	/* 0x16C */
#define NVM_CFG1_GLOB_TRACE_MODULES_ERROR				0x1
#define NVM_CFG1_GLOB_TRACE_MODULES_DBG					0x2
#define NVM_CFG1_GLOB_TRACE_MODULES_DRV_HSI				0x4
#define NVM_CFG1_GLOB_TRACE_MODULES_INTERRUPT				0x8
#define NVM_CFG1_GLOB_TRACE_MODULES_TEMPERATURE				0x10
#define NVM_CFG1_GLOB_TRACE_MODULES_FLR					0x20
#define NVM_CFG1_GLOB_TRACE_MODULES_INIT				0x40
#define NVM_CFG1_GLOB_TRACE_MODULES_NVM					0x80
#define NVM_CFG1_GLOB_TRACE_MODULES_PIM					0x100
#define NVM_CFG1_GLOB_TRACE_MODULES_NET					0x200
#define NVM_CFG1_GLOB_TRACE_MODULES_POWER				0x400
#define NVM_CFG1_GLOB_TRACE_MODULES_UTILS				0x800
#define NVM_CFG1_GLOB_TRACE_MODULES_RESOURCES				0x1000
#define NVM_CFG1_GLOB_TRACE_MODULES_SCHEDULER				0x2000
#define NVM_CFG1_GLOB_TRACE_MODULES_PHYMOD				0x4000
#define NVM_CFG1_GLOB_TRACE_MODULES_EVENTS				0x8000
#define NVM_CFG1_GLOB_TRACE_MODULES_PMM					0x10000
#define NVM_CFG1_GLOB_TRACE_MODULES_DBG_DRV				0x20000
#define NVM_CFG1_GLOB_TRACE_MODULES_ETH					0x40000
#define NVM_CFG1_GLOB_TRACE_MODULES_SECURITY				0x80000
#define NVM_CFG1_GLOB_TRACE_MODULES_PCIE \
	                                                                0x100000
#define NVM_CFG1_GLOB_TRACE_MODULES_TRACE \
	                                                                0x200000
#define NVM_CFG1_GLOB_TRACE_MODULES_MANAGEMENT \
	                                                                0x400000
#define NVM_CFG1_GLOB_TRACE_MODULES_SIM \
	                                                                0x800000
#define NVM_CFG1_GLOB_TRACE_MODULES_BUF_MGR \
	                                                                0x1000000
	u32 pcie_class_code_fcoe;	/* 0x170 */
	/*  Set PCIe FCoE Class Code */
#define NVM_CFG1_GLOB_PCIE_CLASS_CODE_FCOE_MASK \
	                                                                0x00FFFFFF
#define NVM_CFG1_GLOB_PCIE_CLASS_CODE_FCOE_OFFSET			0
	/*  When temperature goes below (ALOM FAN ON AUX value - delta) ALOM
	 * FAN ON AUX gpio is unset */
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_DELTA_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_ALOM_FAN_ON_AUX_DELTA_OFFSET			24
	u32 pcie_class_code_iscsi;	/* 0x174 */
	/*  Set PCIe iSCSI Class Code */
#define NVM_CFG1_GLOB_PCIE_CLASS_CODE_ISCSI_MASK \
	                                                                0x00FFFFFF
#define NVM_CFG1_GLOB_PCIE_CLASS_CODE_ISCSI_OFFSET			0
	/*  When temperature goes below (Dead Temp TH  - delta)Thermal Event
	 * gpio is unset */
#define NVM_CFG1_GLOB_DEAD_TEMP_TH_DELTA_MASK \
	                                                                0xFF000000
#define NVM_CFG1_GLOB_DEAD_TEMP_TH_DELTA_OFFSET				24
	u32 no_provisioned_mac;	/* 0x178 */
	/*  Set number of provisioned MAC addresses */
#define NVM_CFG1_GLOB_NUMBER_OF_PROVISIONED_MAC_MASK \
	                                                                0x0000FFFF
#define NVM_CFG1_GLOB_NUMBER_OF_PROVISIONED_MAC_OFFSET			0
	/*  Set number of provisioned VF MAC addresses */
#define NVM_CFG1_GLOB_NUMBER_OF_PROVISIONED_VF_MAC_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_NUMBER_OF_PROVISIONED_VF_MAC_OFFSET		16
	/*  Enable/Disable BMC MAC */
#define NVM_CFG1_GLOB_PROVISIONED_BMC_MAC_MASK \
	                                                                0x01000000
#define NVM_CFG1_GLOB_PROVISIONED_BMC_MAC_OFFSET			24
#define NVM_CFG1_GLOB_PROVISIONED_BMC_MAC_DISABLED			0x0
#define NVM_CFG1_GLOB_PROVISIONED_BMC_MAC_ENABLED			0x1
	/*  Select the number of ports NCSI reports */
#define NVM_CFG1_GLOB_NCSI_GET_CAPAS_CH_COUNT_MASK \
	                                                                0x06000000
#define NVM_CFG1_GLOB_NCSI_GET_CAPAS_CH_COUNT_OFFSET			25
	/*  Apply nvm option 320 - Tx Preset Value */
#define NVM_CFG1_GLOB_PCIE_TX_PRESET_OVERRIDE_MASK \
	                                                                0x08000000
#define NVM_CFG1_GLOB_PCIE_TX_PRESET_OVERRIDE_OFFSET			27
#define NVM_CFG1_GLOB_PCIE_TX_PRESET_OVERRIDE_DISABLED			0x0
#define NVM_CFG1_GLOB_PCIE_TX_PRESET_OVERRIDE_ENABLED			0x1
	/*  Preset between 0 and 9 */
#define NVM_CFG1_GLOB_PCIE_TX_PRESET_NUMBER_MASK \
	                                                                0xF0000000
#define NVM_CFG1_GLOB_PCIE_TX_PRESET_NUMBER_OFFSET			28
	u32 lowest_mbi_version;	/* 0x17C */
#define NVM_CFG1_GLOB_LOWEST_MBI_VERSION_0_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_LOWEST_MBI_VERSION_0_OFFSET			0
#define NVM_CFG1_GLOB_LOWEST_MBI_VERSION_1_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_GLOB_LOWEST_MBI_VERSION_1_OFFSET			8
#define NVM_CFG1_GLOB_LOWEST_MBI_VERSION_2_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_GLOB_LOWEST_MBI_VERSION_2_OFFSET			16
	u32 generic_cont5;	/* 0x180 */
	/*  Activate options 305-306 */
#define NVM_CFG1_GLOB_OVERRIDE_TX_DRIVER_REGULATOR_MASK \
	                                                                0x00000001
#define NVM_CFG1_GLOB_OVERRIDE_TX_DRIVER_REGULATOR_OFFSET		0
#define NVM_CFG1_GLOB_OVERRIDE_TX_DRIVER_REGULATOR_DISABLED		0x0
#define NVM_CFG1_GLOB_OVERRIDE_TX_DRIVER_REGULATOR_ENABLED		0x1
	/*  A value of 0x00 gives 720mV, A setting of 0x05 gives 800mV; A
	 * setting of 0x15 give a swing of 1060mV */
#define NVM_CFG1_GLOB_TX_REGULATOR_VOLTAGE_GEN1_2_MASK \
	                                                                0x0000003E
#define NVM_CFG1_GLOB_TX_REGULATOR_VOLTAGE_GEN1_2_OFFSET		1
	/*  A value of 0x00 gives 720mV, A setting of 0x05 gives 800mV; A
	 * setting of 0x15 give a swing of 1060mV */
#define NVM_CFG1_GLOB_TX_REGULATOR_VOLTAGE_GEN3_MASK \
	                                                                0x000007C0
#define NVM_CFG1_GLOB_TX_REGULATOR_VOLTAGE_GEN3_OFFSET			6
	/*  QinQ Device Capability */
#define NVM_CFG1_GLOB_QINQ_SUPPORT_MASK \
	                                                                0x00000800
#define NVM_CFG1_GLOB_QINQ_SUPPORT_OFFSET				11
#define NVM_CFG1_GLOB_QINQ_SUPPORT_DISABLED				0x0
#define NVM_CFG1_GLOB_QINQ_SUPPORT_ENABLED				0x1
	/*  NPAR Capability */
#define NVM_CFG1_GLOB_NPAR_CAPABILITY_MASK \
	                                                                0x00001000
#define NVM_CFG1_GLOB_NPAR_CAPABILITY_OFFSET				12
#define NVM_CFG1_GLOB_NPAR_CAPABILITY_DISABLED				0x0
#define NVM_CFG1_GLOB_NPAR_CAPABILITY_ENABLED				0x1
	u32 pre2_generic_cont_4;	/* 0x184 */
#define NVM_CFG1_GLOB_50G_AC_PRE2_MASK \
	                                                                0x000000FF
#define NVM_CFG1_GLOB_50G_AC_PRE2_OFFSET				0
	/*  Set PCIe NVMeTCP Class Code */
#define NVM_CFG1_GLOB_PCIE_CLASS_CODE_NVMETCP_MASK \
	                                                                0xFFFFFF00
#define NVM_CFG1_GLOB_PCIE_CLASS_CODE_NVMETCP_OFFSET			8
	u32 reserved[40];	/* 0x188 */
};

struct nvm_cfg1_path {
	u32 reserved[1];	/* 0x0 */
};

struct nvm_cfg1_port {
	u32 reserved__m_relocated_to_option_123;	/* 0x0 */
	u32 reserved__m_relocated_to_option_124;	/* 0x4 */
	u32 generic_cont0;	/* 0x8 */
#define NVM_CFG1_PORT_LED_MODE_MASK \
	                                                                0x000000FF
#define NVM_CFG1_PORT_LED_MODE_OFFSET					0
#define NVM_CFG1_PORT_LED_MODE_MAC1					0x0
#define NVM_CFG1_PORT_LED_MODE_PHY1					0x1
#define NVM_CFG1_PORT_LED_MODE_PHY2					0x2
#define NVM_CFG1_PORT_LED_MODE_PHY3					0x3
#define NVM_CFG1_PORT_LED_MODE_MAC2					0x4
#define NVM_CFG1_PORT_LED_MODE_PHY4					0x5
#define NVM_CFG1_PORT_LED_MODE_PHY5					0x6
#define NVM_CFG1_PORT_LED_MODE_PHY6					0x7
#define NVM_CFG1_PORT_LED_MODE_MAC3					0x8
#define NVM_CFG1_PORT_LED_MODE_PHY7					0x9
#define NVM_CFG1_PORT_LED_MODE_PHY8					0xA
#define NVM_CFG1_PORT_LED_MODE_PHY9					0xB
#define NVM_CFG1_PORT_LED_MODE_MAC4					0xC
#define NVM_CFG1_PORT_LED_MODE_PHY10					0xD
#define NVM_CFG1_PORT_LED_MODE_PHY11					0xE
#define NVM_CFG1_PORT_LED_MODE_PHY12					0xF
#define NVM_CFG1_PORT_LED_MODE_BREAKOUT					0x10
#define NVM_CFG1_PORT_LED_MODE_OCP_3_0					0x11
#define NVM_CFG1_PORT_LED_MODE_OCP_3_0_MAC2				0x12
#define NVM_CFG1_PORT_LED_MODE_SW_DEF1					0x13
#define NVM_CFG1_PORT_LED_MODE_SW_DEF1_MAC2				0x14
#define NVM_CFG1_PORT_ROCE_PRIORITY_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_PORT_ROCE_PRIORITY_OFFSET				8
#define NVM_CFG1_PORT_DCBX_MODE_MASK \
	                                                                0x000F0000
#define NVM_CFG1_PORT_DCBX_MODE_OFFSET					16
#define NVM_CFG1_PORT_DCBX_MODE_DISABLED				0x0
#define NVM_CFG1_PORT_DCBX_MODE_IEEE					0x1
#define NVM_CFG1_PORT_DCBX_MODE_CEE					0x2
#define NVM_CFG1_PORT_DCBX_MODE_DYNAMIC					0x3
#define NVM_CFG1_PORT_DEFAULT_ENABLED_PROTOCOLS_MASK \
	                                                                0x00F00000
#define NVM_CFG1_PORT_DEFAULT_ENABLED_PROTOCOLS_OFFSET			20
#define NVM_CFG1_PORT_DEFAULT_ENABLED_PROTOCOLS_ETHERNET		0x1
#define NVM_CFG1_PORT_DEFAULT_ENABLED_PROTOCOLS_FCOE			0x2
#define NVM_CFG1_PORT_DEFAULT_ENABLED_PROTOCOLS_ISCSI			0x4
#define NVM_CFG1_PORT_DEFAULT_ENABLED_PROTOCOLS_NVMETCP			0x8
	/*  GPIO for HW reset the PHY. In case it is the same for all ports,
	 * need to set same value for all ports */
#define NVM_CFG1_PORT_EXT_PHY_RESET_MASK \
	                                                                0xFF000000
#define NVM_CFG1_PORT_EXT_PHY_RESET_OFFSET				24
#define NVM_CFG1_PORT_EXT_PHY_RESET_NA					0x0
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO0				0x1
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO1				0x2
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO2				0x3
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO3				0x4
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO4				0x5
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO5				0x6
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO6				0x7
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO7				0x8
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO8				0x9
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO9				0xA
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO10				0xB
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO11				0xC
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO12				0xD
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO13				0xE
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO14				0xF
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO15				0x10
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO16				0x11
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO17				0x12
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO18				0x13
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO19				0x14
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO20				0x15
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO21				0x16
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO22				0x17
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO23				0x18
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO24				0x19
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO25				0x1A
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO26				0x1B
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO27				0x1C
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO28				0x1D
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO29				0x1E
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO30				0x1F
#define NVM_CFG1_PORT_EXT_PHY_RESET_GPIO31				0x20
	u32 pcie_cfg;		/* 0xC */
#define NVM_CFG1_PORT_RESERVED15_MASK \
	                                                                0x00000007
#define NVM_CFG1_PORT_RESERVED15_OFFSET					0
#define NVM_CFG1_PORT_NVMETCP_DID_SUFFIX_MASK \
	                                                                0x000007F8
#define NVM_CFG1_PORT_NVMETCP_DID_SUFFIX_OFFSET				3
#define NVM_CFG1_PORT_NVMETCP_MODE_MASK \
	                                                                0x00007800
#define NVM_CFG1_PORT_NVMETCP_MODE_OFFSET				11
#define NVM_CFG1_PORT_NVMETCP_MODE_HOST_MODE_ONLY			0x0
#define NVM_CFG1_PORT_NVMETCP_MODE_TARGET_MODE_ONLY			0x1
#define NVM_CFG1_PORT_NVMETCP_MODE_DUAL_MODE				0x2
#define NVM_CFG1_PORT_ETH_NPAR_DID_SUFFIX_MASK \
	                                                                0x007F8000
#define NVM_CFG1_PORT_ETH_NPAR_DID_SUFFIX_OFFSET			15
	/*  Enabling comission static and dynamic NVMe TCP PFs */
#define NVM_CFG1_PORT_NVME_TCP_OFFLOAD_MODE_MASK \
	                                                                0x01800000
#define NVM_CFG1_PORT_NVME_TCP_OFFLOAD_MODE_OFFSET			23
#define NVM_CFG1_PORT_NVME_TCP_OFFLOAD_MODE_DISABLED			0x0
#define NVM_CFG1_PORT_NVME_TCP_OFFLOAD_MODE_ENABLED			0x1
	u32 features;		/* 0x10 */
#define NVM_CFG1_PORT_ENABLE_WOL_ON_ACPI_PATTERN_MASK \
	                                                                0x00000001
#define NVM_CFG1_PORT_ENABLE_WOL_ON_ACPI_PATTERN_OFFSET			0
#define NVM_CFG1_PORT_ENABLE_WOL_ON_ACPI_PATTERN_DISABLED		0x0
#define NVM_CFG1_PORT_ENABLE_WOL_ON_ACPI_PATTERN_ENABLED		0x1
#define NVM_CFG1_PORT_MAGIC_PACKET_WOL_MASK \
	                                                                0x00000002
#define NVM_CFG1_PORT_MAGIC_PACKET_WOL_OFFSET				1
#define NVM_CFG1_PORT_MAGIC_PACKET_WOL_DISABLED				0x0
#define NVM_CFG1_PORT_MAGIC_PACKET_WOL_ENABLED				0x1
	/*  Enable Permit port shutdown feature */
#define NVM_CFG1_PORT_PERMIT_TOTAL_PORT_SHUTDOWN_MASK \
	                                                                0x00000004
#define NVM_CFG1_PORT_PERMIT_TOTAL_PORT_SHUTDOWN_OFFSET			2
#define NVM_CFG1_PORT_PERMIT_TOTAL_PORT_SHUTDOWN_DISABLED		0x0
#define NVM_CFG1_PORT_PERMIT_TOTAL_PORT_SHUTDOWN_ENABLED		0x1
	u32 speed_cap_mask;	/* 0x14 */
#define NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_MASK \
	                                                                0x0000FFFF
#define NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_OFFSET			0
#define NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_1G			0x1
#define NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_10G			0x2
#define NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_20G			0x4
#define NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_25G			0x8
#define NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_40G			0x10
#define NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_50G			0x20
#define NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_BB_AHP_100G		0x40
#define NVM_CFG1_PORT_MFW_SPEED_CAPABILITY_MASK_MASK \
	                                                                0xFFFF0000
#define NVM_CFG1_PORT_MFW_SPEED_CAPABILITY_MASK_OFFSET			16
#define NVM_CFG1_PORT_MFW_SPEED_CAPABILITY_MASK_1G			0x1
#define NVM_CFG1_PORT_MFW_SPEED_CAPABILITY_MASK_10G			0x2
#define NVM_CFG1_PORT_MFW_SPEED_CAPABILITY_MASK_20G			0x4
#define NVM_CFG1_PORT_MFW_SPEED_CAPABILITY_MASK_25G			0x8
#define NVM_CFG1_PORT_MFW_SPEED_CAPABILITY_MASK_40G			0x10
#define NVM_CFG1_PORT_MFW_SPEED_CAPABILITY_MASK_50G			0x20
#define NVM_CFG1_PORT_MFW_SPEED_CAPABILITY_MASK_BB_AHP_100G		0x40
	u32 link_settings;	/* 0x18 */
#define NVM_CFG1_PORT_DRV_LINK_SPEED_MASK \
	                                                                0x0000000F
#define NVM_CFG1_PORT_DRV_LINK_SPEED_OFFSET				0
#define NVM_CFG1_PORT_DRV_LINK_SPEED_AUTONEG				0x0
#define NVM_CFG1_PORT_DRV_LINK_SPEED_1G					0x1
#define NVM_CFG1_PORT_DRV_LINK_SPEED_10G				0x2
#define NVM_CFG1_PORT_DRV_LINK_SPEED_20G				0x3
#define NVM_CFG1_PORT_DRV_LINK_SPEED_25G				0x4
#define NVM_CFG1_PORT_DRV_LINK_SPEED_40G				0x5
#define NVM_CFG1_PORT_DRV_LINK_SPEED_50G				0x6
#define NVM_CFG1_PORT_DRV_LINK_SPEED_BB_AHP_100G			0x7
#define NVM_CFG1_PORT_DRV_FLOW_CONTROL_MASK \
	                                                                0x00000070
#define NVM_CFG1_PORT_DRV_FLOW_CONTROL_OFFSET				4
#define NVM_CFG1_PORT_DRV_FLOW_CONTROL_AUTONEG				0x1
#define NVM_CFG1_PORT_DRV_FLOW_CONTROL_RX				0x2
#define NVM_CFG1_PORT_DRV_FLOW_CONTROL_TX				0x4
#define NVM_CFG1_PORT_MFW_LINK_SPEED_MASK \
	                                                                0x00000780
#define NVM_CFG1_PORT_MFW_LINK_SPEED_OFFSET				7
#define NVM_CFG1_PORT_MFW_LINK_SPEED_AUTONEG				0x0
#define NVM_CFG1_PORT_MFW_LINK_SPEED_1G					0x1
#define NVM_CFG1_PORT_MFW_LINK_SPEED_10G				0x2
#define NVM_CFG1_PORT_MFW_LINK_SPEED_20G				0x3
#define NVM_CFG1_PORT_MFW_LINK_SPEED_25G				0x4
#define NVM_CFG1_PORT_MFW_LINK_SPEED_40G				0x5
#define NVM_CFG1_PORT_MFW_LINK_SPEED_50G				0x6
#define NVM_CFG1_PORT_MFW_LINK_SPEED_BB_AHP_100G			0x7
#define NVM_CFG1_PORT_MFW_FLOW_CONTROL_MASK \
	                                                                0x00003800
#define NVM_CFG1_PORT_MFW_FLOW_CONTROL_OFFSET				11
#define NVM_CFG1_PORT_MFW_FLOW_CONTROL_AUTONEG				0x1
#define NVM_CFG1_PORT_MFW_FLOW_CONTROL_RX				0x2
#define NVM_CFG1_PORT_MFW_FLOW_CONTROL_TX				0x4
#define NVM_CFG1_PORT_OPTIC_MODULE_VENDOR_ENFORCEMENT_MASK \
	                                                                0x00004000
#define NVM_CFG1_PORT_OPTIC_MODULE_VENDOR_ENFORCEMENT_OFFSET		14
#define NVM_CFG1_PORT_OPTIC_MODULE_VENDOR_ENFORCEMENT_DISABLED		0x0
#define NVM_CFG1_PORT_OPTIC_MODULE_VENDOR_ENFORCEMENT_ENABLED		0x1
#define NVM_CFG1_PORT_AN_25G_50G_OUI_MASK \
	                                                                0x00018000
#define NVM_CFG1_PORT_AN_25G_50G_OUI_OFFSET				15
#define NVM_CFG1_PORT_AN_25G_50G_OUI_CONSORTIUM				0x0
#define NVM_CFG1_PORT_AN_25G_50G_OUI_BAM				0x1
#define NVM_CFG1_PORT_FEC_FORCE_MODE_MASK \
	                                                                0x000E0000
#define NVM_CFG1_PORT_FEC_FORCE_MODE_OFFSET				17
#define NVM_CFG1_PORT_FEC_FORCE_MODE_NONE				0x0
#define NVM_CFG1_PORT_FEC_FORCE_MODE_FIRECODE				0x1
#define NVM_CFG1_PORT_FEC_FORCE_MODE_RS					0x2
#define NVM_CFG1_PORT_FEC_FORCE_MODE_AUTO				0x7
#define NVM_CFG1_PORT_FEC_AN_MODE_MASK \
	                                                                0x00700000
#define NVM_CFG1_PORT_FEC_AN_MODE_OFFSET				20
#define NVM_CFG1_PORT_FEC_AN_MODE_NONE					0x0
#define NVM_CFG1_PORT_FEC_AN_MODE_10G_FIRECODE				0x1
#define NVM_CFG1_PORT_FEC_AN_MODE_25G_FIRECODE				0x2
#define NVM_CFG1_PORT_FEC_AN_MODE_10G_AND_25G_FIRECODE			0x3
#define NVM_CFG1_PORT_FEC_AN_MODE_25G_RS				0x4
#define NVM_CFG1_PORT_FEC_AN_MODE_25G_FIRECODE_AND_RS			0x5
#define NVM_CFG1_PORT_FEC_AN_MODE_ALL					0x6
#define NVM_CFG1_PORT_SMARTLINQ_MODE_MASK \
	                                                                0x00800000
#define NVM_CFG1_PORT_SMARTLINQ_MODE_OFFSET				23
#define NVM_CFG1_PORT_SMARTLINQ_MODE_DISABLED				0x0
#define NVM_CFG1_PORT_SMARTLINQ_MODE_ENABLED				0x1
#define NVM_CFG1_PORT_RESERVED_WAS_MFW_SMARTLINQ_MASK \
	                                                                0x01000000
#define NVM_CFG1_PORT_RESERVED_WAS_MFW_SMARTLINQ_OFFSET			24
#define NVM_CFG1_PORT_RESERVED_WAS_MFW_SMARTLINQ_DISABLED		0x0
#define NVM_CFG1_PORT_RESERVED_WAS_MFW_SMARTLINQ_ENABLED		0x1
	/*  Enable/Disable RX PAM-4 precoding */
#define NVM_CFG1_PORT_RX_PRECODE_MASK \
	                                                                0x02000000
#define NVM_CFG1_PORT_RX_PRECODE_OFFSET					25
#define NVM_CFG1_PORT_RX_PRECODE_DISABLED				0x0
#define NVM_CFG1_PORT_RX_PRECODE_ENABLED				0x1
	/*  Enable/Disable TX PAM-4 precoding */
#define NVM_CFG1_PORT_TX_PRECODE_MASK \
	                                                                0x04000000
#define NVM_CFG1_PORT_TX_PRECODE_OFFSET					26
#define NVM_CFG1_PORT_TX_PRECODE_DISABLED				0x0
#define NVM_CFG1_PORT_TX_PRECODE_ENABLED				0x1
	u32 phy_cfg;		/* 0x1C */
#define NVM_CFG1_PORT_OPTIONAL_LINK_MODES_MASK \
	                                                                0x0000FFFF
#define NVM_CFG1_PORT_OPTIONAL_LINK_MODES_OFFSET			0
#define NVM_CFG1_PORT_OPTIONAL_LINK_MODES_HIGIG				0x1
#define NVM_CFG1_PORT_OPTIONAL_LINK_MODES_SCRAMBLER			0x2
#define NVM_CFG1_PORT_OPTIONAL_LINK_MODES_FIBER				0x4
#define NVM_CFG1_PORT_OPTIONAL_LINK_MODES_DISABLE_CL72_AN		0x8
#define NVM_CFG1_PORT_OPTIONAL_LINK_MODES_DISABLE_FEC_AN		0x10
#define NVM_CFG1_PORT_SERDES_NET_INTERFACE_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_PORT_SERDES_NET_INTERFACE_OFFSET			16
#define NVM_CFG1_PORT_SERDES_NET_INTERFACE_BYPASS			0x0
#define NVM_CFG1_PORT_SERDES_NET_INTERFACE_KR				0x2
#define NVM_CFG1_PORT_SERDES_NET_INTERFACE_KR2				0x3
#define NVM_CFG1_PORT_SERDES_NET_INTERFACE_KR4				0x4
#define NVM_CFG1_PORT_SERDES_NET_INTERFACE_XFI				0x8
#define NVM_CFG1_PORT_SERDES_NET_INTERFACE_SFI				0x9
#define NVM_CFG1_PORT_SERDES_NET_INTERFACE_1000X			0xB
#define NVM_CFG1_PORT_SERDES_NET_INTERFACE_SGMII			0xC
#define NVM_CFG1_PORT_SERDES_NET_INTERFACE_XLAUI			0x11
#define NVM_CFG1_PORT_SERDES_NET_INTERFACE_XLPPI			0x12
#define NVM_CFG1_PORT_SERDES_NET_INTERFACE_CAUI				0x21
#define NVM_CFG1_PORT_SERDES_NET_INTERFACE_CPPI				0x22
#define NVM_CFG1_PORT_SERDES_NET_INTERFACE_25GAUI			0x31
#define NVM_CFG1_PORT_AN_MODE_MASK \
	                                                                0xFF000000
#define NVM_CFG1_PORT_AN_MODE_OFFSET					24
#define NVM_CFG1_PORT_AN_MODE_NONE					0x0
#define NVM_CFG1_PORT_AN_MODE_CL73					0x1
#define NVM_CFG1_PORT_AN_MODE_CL37					0x2
#define NVM_CFG1_PORT_AN_MODE_CL73_BAM					0x3
#define NVM_CFG1_PORT_AN_MODE_BB_CL37_BAM				0x4
#define NVM_CFG1_PORT_AN_MODE_BB_HPAM					0x5
#define NVM_CFG1_PORT_AN_MODE_BB_SGMII					0x6
	u32 mgmt_traffic;	/* 0x20 */
#define NVM_CFG1_PORT_RESERVED61_MASK \
	                                                                0x0000000F
#define NVM_CFG1_PORT_RESERVED61_OFFSET					0
	u32 ext_phy;		/* 0x24 */
#define NVM_CFG1_PORT_EXTERNAL_PHY_TYPE_MASK \
	                                                                0x000000FF
#define NVM_CFG1_PORT_EXTERNAL_PHY_TYPE_OFFSET				0
#define NVM_CFG1_PORT_EXTERNAL_PHY_TYPE_NONE				0x0
#define NVM_CFG1_PORT_EXTERNAL_PHY_TYPE_BCM8485X			0x1
#define NVM_CFG1_PORT_EXTERNAL_PHY_TYPE_BCM5422X			0x2
#define NVM_CFG1_PORT_EXTERNAL_PHY_TYPE_88X33X0				0x3
#define NVM_CFG1_PORT_EXTERNAL_PHY_TYPE_AQR11X				0x4
#define NVM_CFG1_PORT_EXTERNAL_PHY_ADDRESS_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_PORT_EXTERNAL_PHY_ADDRESS_OFFSET			8
	/*  EEE power saving mode */
#define NVM_CFG1_PORT_EEE_POWER_SAVING_MODE_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_PORT_EEE_POWER_SAVING_MODE_OFFSET			16
#define NVM_CFG1_PORT_EEE_POWER_SAVING_MODE_DISABLED			0x0
#define NVM_CFG1_PORT_EEE_POWER_SAVING_MODE_BALANCED			0x1
#define NVM_CFG1_PORT_EEE_POWER_SAVING_MODE_AGGRESSIVE			0x2
#define NVM_CFG1_PORT_EEE_POWER_SAVING_MODE_LOW_LATENCY			0x3
	/*  Ext PHY AVS Enable */
#define NVM_CFG1_PORT_EXT_PHY_AVS_ENABLE_MASK \
	                                                                0x01000000
#define NVM_CFG1_PORT_EXT_PHY_AVS_ENABLE_OFFSET				24
#define NVM_CFG1_PORT_EXT_PHY_AVS_ENABLE_DISABLED			0x0
#define NVM_CFG1_PORT_EXT_PHY_AVS_ENABLE_ENABLED			0x1
	u32 mba_cfg1;		/* 0x28 */
#define NVM_CFG1_PORT_PREBOOT_OPROM_MASK \
	                                                                0x00000001
#define NVM_CFG1_PORT_PREBOOT_OPROM_OFFSET				0
#define NVM_CFG1_PORT_PREBOOT_OPROM_DISABLED				0x0
#define NVM_CFG1_PORT_PREBOOT_OPROM_ENABLED				0x1
#define NVM_CFG1_PORT_RESERVED__M_MBA_BOOT_TYPE_MASK \
	                                                                0x00000006
#define NVM_CFG1_PORT_RESERVED__M_MBA_BOOT_TYPE_OFFSET			1
#define NVM_CFG1_PORT_MBA_DELAY_TIME_MASK \
	                                                                0x00000078
#define NVM_CFG1_PORT_MBA_DELAY_TIME_OFFSET				3
#define NVM_CFG1_PORT_MBA_SETUP_HOT_KEY_MASK \
	                                                                0x00000080
#define NVM_CFG1_PORT_MBA_SETUP_HOT_KEY_OFFSET				7
#define NVM_CFG1_PORT_MBA_SETUP_HOT_KEY_CTRL_S				0x0
#define NVM_CFG1_PORT_MBA_SETUP_HOT_KEY_CTRL_B				0x1
#define NVM_CFG1_PORT_MBA_HIDE_SETUP_PROMPT_MASK \
	                                                                0x00000100
#define NVM_CFG1_PORT_MBA_HIDE_SETUP_PROMPT_OFFSET			8
#define NVM_CFG1_PORT_MBA_HIDE_SETUP_PROMPT_DISABLED			0x0
#define NVM_CFG1_PORT_MBA_HIDE_SETUP_PROMPT_ENABLED			0x1
#define NVM_CFG1_PORT_RESERVED5_MASK \
	                                                                0x0001FE00
#define NVM_CFG1_PORT_RESERVED5_OFFSET					9
#define NVM_CFG1_PORT_PREBOOT_LINK_SPEED_MASK \
	                                                                0x001E0000
#define NVM_CFG1_PORT_PREBOOT_LINK_SPEED_OFFSET				17
#define NVM_CFG1_PORT_PREBOOT_LINK_SPEED_AUTONEG			0x0
#define NVM_CFG1_PORT_PREBOOT_LINK_SPEED_1G				0x1
#define NVM_CFG1_PORT_PREBOOT_LINK_SPEED_10G				0x2
#define NVM_CFG1_PORT_PREBOOT_LINK_SPEED_20G				0x3
#define NVM_CFG1_PORT_PREBOOT_LINK_SPEED_25G				0x4
#define NVM_CFG1_PORT_PREBOOT_LINK_SPEED_40G				0x5
#define NVM_CFG1_PORT_PREBOOT_LINK_SPEED_50G				0x6
#define NVM_CFG1_PORT_PREBOOT_LINK_SPEED_BB_AHP_100G			0x7
#define NVM_CFG1_PORT_RESERVED__M_MBA_BOOT_RETRY_COUNT_MASK \
	                                                                0x00E00000
#define NVM_CFG1_PORT_RESERVED__M_MBA_BOOT_RETRY_COUNT_OFFSET		21
#define NVM_CFG1_PORT_RESERVED_WAS_PREBOOT_SMARTLINQ_MASK \
	                                                                0x01000000
#define NVM_CFG1_PORT_RESERVED_WAS_PREBOOT_SMARTLINQ_OFFSET		24
#define NVM_CFG1_PORT_RESERVED_WAS_PREBOOT_SMARTLINQ_DISABLED		0x0
#define NVM_CFG1_PORT_RESERVED_WAS_PREBOOT_SMARTLINQ_ENABLED		0x1
	u32 mba_cfg2;		/* 0x2C */
#define NVM_CFG1_PORT_RESERVED65_MASK \
	                                                                0x0000FFFF
#define NVM_CFG1_PORT_RESERVED65_OFFSET					0
#define NVM_CFG1_PORT_RESERVED66_MASK \
	                                                                0x00010000
#define NVM_CFG1_PORT_RESERVED66_OFFSET					16
#define NVM_CFG1_PORT_PREBOOT_LINK_UP_DELAY_MASK \
	                                                                0x01FE0000
#define NVM_CFG1_PORT_PREBOOT_LINK_UP_DELAY_OFFSET			17
	u32 vf_cfg;		/* 0x30 */
#define NVM_CFG1_PORT_RESERVED8_MASK \
	                                                                0x0000FFFF
#define NVM_CFG1_PORT_RESERVED8_OFFSET					0
#define NVM_CFG1_PORT_RESERVED6_MASK \
	                                                                0x000F0000
#define NVM_CFG1_PORT_RESERVED6_OFFSET					16
	struct nvm_cfg_mac_address lldp_mac_address;	/* 0x34 */
	u32 led_port_settings;	/* 0x3C */
#define NVM_CFG1_PORT_LANE_LED_SPD_0_SEL_MASK \
	                                                                0x000000FF
#define NVM_CFG1_PORT_LANE_LED_SPD_0_SEL_OFFSET				0
#define NVM_CFG1_PORT_LANE_LED_SPD_1_SEL_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_PORT_LANE_LED_SPD_1_SEL_OFFSET				8
#define NVM_CFG1_PORT_LANE_LED_SPD_2_SEL_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_PORT_LANE_LED_SPD_2_SEL_OFFSET				16
#define NVM_CFG1_PORT_LANE_LED_SPD__SEL_1G				0x1
#define NVM_CFG1_PORT_LANE_LED_SPD__SEL_10G				0x2
#define NVM_CFG1_PORT_LANE_LED_SPD__SEL_AH_25G_AHP_25G			0x4
#define NVM_CFG1_PORT_LANE_LED_SPD__SEL_BB_25G_AH_40G_AHP_40G		0x8
#define NVM_CFG1_PORT_LANE_LED_SPD__SEL_BB_40G_AH_50G_AHP_50G		0x10
#define NVM_CFG1_PORT_LANE_LED_SPD__SEL_BB_50G_AHP_100G			0x20
#define NVM_CFG1_PORT_LANE_LED_SPD__SEL_BB_100G				0x40
	/*  UID LED Blink Mode Settings */
#define NVM_CFG1_PORT_UID_LED_MODE_MASK_MASK \
	                                                                0x0F000000
#define NVM_CFG1_PORT_UID_LED_MODE_MASK_OFFSET				24
#define NVM_CFG1_PORT_UID_LED_MODE_MASK_ACTIVITY_LED			0x1
#define NVM_CFG1_PORT_UID_LED_MODE_MASK_LINK_LED0			0x2
#define NVM_CFG1_PORT_UID_LED_MODE_MASK_LINK_LED1			0x4
#define NVM_CFG1_PORT_UID_LED_MODE_MASK_LINK_LED2			0x8
	/*  Activity LED Blink Rate in Hz */
#define NVM_CFG1_PORT_ACTIVITY_LED_BLINK_RATE_HZ_MASK \
	                                                                0xF0000000
#define NVM_CFG1_PORT_ACTIVITY_LED_BLINK_RATE_HZ_OFFSET			28
	u32 transceiver_00;	/* 0x40 */
	/*  Define for mapping of transceiver signal module absent */
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_MASK \
	                                                                0x000000FF
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_OFFSET				0
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_NA				0x0
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO0				0x1
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO1				0x2
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO2				0x3
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO3				0x4
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO4				0x5
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO5				0x6
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO6				0x7
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO7				0x8
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO8				0x9
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO9				0xA
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO10				0xB
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO11				0xC
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO12				0xD
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO13				0xE
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO14				0xF
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO15				0x10
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO16				0x11
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO17				0x12
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO18				0x13
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO19				0x14
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO20				0x15
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO21				0x16
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO22				0x17
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO23				0x18
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO24				0x19
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO25				0x1A
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO26				0x1B
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO27				0x1C
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO28				0x1D
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO29				0x1E
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO30				0x1F
#define NVM_CFG1_PORT_TRANS_MODULE_ABS_GPIO31				0x20
	/*  Define the GPIO mux settings  to switch i2c mux to this port */
#define NVM_CFG1_PORT_I2C_MUX_SEL_VALUE_0_MASK \
	                                                                0x00000F00
#define NVM_CFG1_PORT_I2C_MUX_SEL_VALUE_0_OFFSET			8
#define NVM_CFG1_PORT_I2C_MUX_SEL_VALUE_1_MASK \
	                                                                0x0000F000
#define NVM_CFG1_PORT_I2C_MUX_SEL_VALUE_1_OFFSET			12
	/*  Option to override SmartAN FEC requirements */
#define NVM_CFG1_PORT_SMARTAN_FEC_OVERRIDE_MASK \
	                                                                0x00010000
#define NVM_CFG1_PORT_SMARTAN_FEC_OVERRIDE_OFFSET			16
#define NVM_CFG1_PORT_SMARTAN_FEC_OVERRIDE_DISABLED			0x0
#define NVM_CFG1_PORT_SMARTAN_FEC_OVERRIDE_ENABLED			0x1
	u32 device_ids;		/* 0x44 */
#define NVM_CFG1_PORT_ETH_DID_SUFFIX_MASK \
	                                                                0x000000FF
#define NVM_CFG1_PORT_ETH_DID_SUFFIX_OFFSET				0
#define NVM_CFG1_PORT_FCOE_DID_SUFFIX_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_PORT_FCOE_DID_SUFFIX_OFFSET				8
#define NVM_CFG1_PORT_ISCSI_DID_SUFFIX_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_PORT_ISCSI_DID_SUFFIX_OFFSET				16
#define NVM_CFG1_PORT_RESERVED_DID_SUFFIX_MASK \
	                                                                0xFF000000
#define NVM_CFG1_PORT_RESERVED_DID_SUFFIX_OFFSET			24
	u32 board_cfg;		/* 0x48 */
	/*  This field defines the board technology
	 * (backpane,transceiver,external PHY) */
#define NVM_CFG1_PORT_PORT_TYPE_MASK \
	                                                                0x000000FF
#define NVM_CFG1_PORT_PORT_TYPE_OFFSET					0
#define NVM_CFG1_PORT_PORT_TYPE_UNDEFINED				0x0
#define NVM_CFG1_PORT_PORT_TYPE_MODULE					0x1
#define NVM_CFG1_PORT_PORT_TYPE_BACKPLANE				0x2
#define NVM_CFG1_PORT_PORT_TYPE_EXT_PHY					0x3
#define NVM_CFG1_PORT_PORT_TYPE_MODULE_SLAVE				0x4
	/*  This field defines the GPIO mapped to tx_disable signal in SFP */
#define NVM_CFG1_PORT_TX_DISABLE_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_PORT_TX_DISABLE_OFFSET					8
#define NVM_CFG1_PORT_TX_DISABLE_NA					0x0
#define NVM_CFG1_PORT_TX_DISABLE_GPIO0					0x1
#define NVM_CFG1_PORT_TX_DISABLE_GPIO1					0x2
#define NVM_CFG1_PORT_TX_DISABLE_GPIO2					0x3
#define NVM_CFG1_PORT_TX_DISABLE_GPIO3					0x4
#define NVM_CFG1_PORT_TX_DISABLE_GPIO4					0x5
#define NVM_CFG1_PORT_TX_DISABLE_GPIO5					0x6
#define NVM_CFG1_PORT_TX_DISABLE_GPIO6					0x7
#define NVM_CFG1_PORT_TX_DISABLE_GPIO7					0x8
#define NVM_CFG1_PORT_TX_DISABLE_GPIO8					0x9
#define NVM_CFG1_PORT_TX_DISABLE_GPIO9					0xA
#define NVM_CFG1_PORT_TX_DISABLE_GPIO10					0xB
#define NVM_CFG1_PORT_TX_DISABLE_GPIO11					0xC
#define NVM_CFG1_PORT_TX_DISABLE_GPIO12					0xD
#define NVM_CFG1_PORT_TX_DISABLE_GPIO13					0xE
#define NVM_CFG1_PORT_TX_DISABLE_GPIO14					0xF
#define NVM_CFG1_PORT_TX_DISABLE_GPIO15					0x10
#define NVM_CFG1_PORT_TX_DISABLE_GPIO16					0x11
#define NVM_CFG1_PORT_TX_DISABLE_GPIO17					0x12
#define NVM_CFG1_PORT_TX_DISABLE_GPIO18					0x13
#define NVM_CFG1_PORT_TX_DISABLE_GPIO19					0x14
#define NVM_CFG1_PORT_TX_DISABLE_GPIO20					0x15
#define NVM_CFG1_PORT_TX_DISABLE_GPIO21					0x16
#define NVM_CFG1_PORT_TX_DISABLE_GPIO22					0x17
#define NVM_CFG1_PORT_TX_DISABLE_GPIO23					0x18
#define NVM_CFG1_PORT_TX_DISABLE_GPIO24					0x19
#define NVM_CFG1_PORT_TX_DISABLE_GPIO25					0x1A
#define NVM_CFG1_PORT_TX_DISABLE_GPIO26					0x1B
#define NVM_CFG1_PORT_TX_DISABLE_GPIO27					0x1C
#define NVM_CFG1_PORT_TX_DISABLE_GPIO28					0x1D
#define NVM_CFG1_PORT_TX_DISABLE_GPIO29					0x1E
#define NVM_CFG1_PORT_TX_DISABLE_GPIO30					0x1F
#define NVM_CFG1_PORT_TX_DISABLE_GPIO31					0x20
	u32 mnm_10g_cap;	/* 0x4C */
#define NVM_CFG1_PORT_MNM_10G_DRV_SPEED_CAPABILITY_MASK_MASK \
	                                                                0x0000FFFF
#define NVM_CFG1_PORT_MNM_10G_DRV_SPEED_CAPABILITY_MASK_OFFSET		0
#define NVM_CFG1_PORT_MNM_10G_DRV_SPEED_CAPABILITY_MASK_1G		0x1
#define NVM_CFG1_PORT_MNM_10G_DRV_SPEED_CAPABILITY_MASK_10G		0x2
#define NVM_CFG1_PORT_MNM_10G_DRV_SPEED_CAPABILITY_MASK_20G		0x4
#define NVM_CFG1_PORT_MNM_10G_DRV_SPEED_CAPABILITY_MASK_25G		0x8
#define NVM_CFG1_PORT_MNM_10G_DRV_SPEED_CAPABILITY_MASK_40G		0x10
#define NVM_CFG1_PORT_MNM_10G_DRV_SPEED_CAPABILITY_MASK_50G		0x20
#define NVM_CFG1_PORT_MNM_10G_DRV_SPEED_CAPABILITY_MASK_BB_AHP_100G	0x40
#define NVM_CFG1_PORT_MNM_10G_MFW_SPEED_CAPABILITY_MASK_MASK \
	                                                                0xFFFF0000
#define NVM_CFG1_PORT_MNM_10G_MFW_SPEED_CAPABILITY_MASK_OFFSET		16
#define NVM_CFG1_PORT_MNM_10G_MFW_SPEED_CAPABILITY_MASK_1G		0x1
#define NVM_CFG1_PORT_MNM_10G_MFW_SPEED_CAPABILITY_MASK_10G		0x2
#define NVM_CFG1_PORT_MNM_10G_MFW_SPEED_CAPABILITY_MASK_20G		0x4
#define NVM_CFG1_PORT_MNM_10G_MFW_SPEED_CAPABILITY_MASK_25G		0x8
#define NVM_CFG1_PORT_MNM_10G_MFW_SPEED_CAPABILITY_MASK_40G		0x10
#define NVM_CFG1_PORT_MNM_10G_MFW_SPEED_CAPABILITY_MASK_50G		0x20
#define NVM_CFG1_PORT_MNM_10G_MFW_SPEED_CAPABILITY_MASK_BB_AHP_100G	0x40
	u32 mnm_10g_ctrl;	/* 0x50 */
#define NVM_CFG1_PORT_MNM_10G_DRV_LINK_SPEED_MASK \
	                                                                0x0000000F
#define NVM_CFG1_PORT_MNM_10G_DRV_LINK_SPEED_OFFSET			0
#define NVM_CFG1_PORT_MNM_10G_DRV_LINK_SPEED_AUTONEG			0x0
#define NVM_CFG1_PORT_MNM_10G_DRV_LINK_SPEED_1G				0x1
#define NVM_CFG1_PORT_MNM_10G_DRV_LINK_SPEED_10G			0x2
#define NVM_CFG1_PORT_MNM_10G_DRV_LINK_SPEED_20G			0x3
#define NVM_CFG1_PORT_MNM_10G_DRV_LINK_SPEED_25G			0x4
#define NVM_CFG1_PORT_MNM_10G_DRV_LINK_SPEED_40G			0x5
#define NVM_CFG1_PORT_MNM_10G_DRV_LINK_SPEED_50G			0x6
#define NVM_CFG1_PORT_MNM_10G_DRV_LINK_SPEED_BB_AHP_100G		0x7
#define NVM_CFG1_PORT_MNM_10G_MFW_LINK_SPEED_MASK \
	                                                                0x000000F0
#define NVM_CFG1_PORT_MNM_10G_MFW_LINK_SPEED_OFFSET			4
#define NVM_CFG1_PORT_MNM_10G_MFW_LINK_SPEED_AUTONEG			0x0
#define NVM_CFG1_PORT_MNM_10G_MFW_LINK_SPEED_1G				0x1
#define NVM_CFG1_PORT_MNM_10G_MFW_LINK_SPEED_10G			0x2
#define NVM_CFG1_PORT_MNM_10G_MFW_LINK_SPEED_20G			0x3
#define NVM_CFG1_PORT_MNM_10G_MFW_LINK_SPEED_25G			0x4
#define NVM_CFG1_PORT_MNM_10G_MFW_LINK_SPEED_40G			0x5
#define NVM_CFG1_PORT_MNM_10G_MFW_LINK_SPEED_50G			0x6
#define NVM_CFG1_PORT_MNM_10G_MFW_LINK_SPEED_BB_AHP_100G		0x7
	/*  This field defines the board technology
	 * (backpane,transceiver,external PHY) */
#define NVM_CFG1_PORT_MNM_10G_PORT_TYPE_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_PORT_MNM_10G_PORT_TYPE_OFFSET				8
#define NVM_CFG1_PORT_MNM_10G_PORT_TYPE_UNDEFINED			0x0
#define NVM_CFG1_PORT_MNM_10G_PORT_TYPE_MODULE				0x1
#define NVM_CFG1_PORT_MNM_10G_PORT_TYPE_BACKPLANE			0x2
#define NVM_CFG1_PORT_MNM_10G_PORT_TYPE_EXT_PHY				0x3
#define NVM_CFG1_PORT_MNM_10G_PORT_TYPE_MODULE_SLAVE			0x4
#define NVM_CFG1_PORT_MNM_10G_SERDES_NET_INTERFACE_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_PORT_MNM_10G_SERDES_NET_INTERFACE_OFFSET		16
#define NVM_CFG1_PORT_MNM_10G_SERDES_NET_INTERFACE_BYPASS		0x0
#define NVM_CFG1_PORT_MNM_10G_SERDES_NET_INTERFACE_KR			0x2
#define NVM_CFG1_PORT_MNM_10G_SERDES_NET_INTERFACE_KR2			0x3
#define NVM_CFG1_PORT_MNM_10G_SERDES_NET_INTERFACE_KR4			0x4
#define NVM_CFG1_PORT_MNM_10G_SERDES_NET_INTERFACE_XFI			0x8
#define NVM_CFG1_PORT_MNM_10G_SERDES_NET_INTERFACE_SFI			0x9
#define NVM_CFG1_PORT_MNM_10G_SERDES_NET_INTERFACE_1000X		0xB
#define NVM_CFG1_PORT_MNM_10G_SERDES_NET_INTERFACE_SGMII		0xC
#define NVM_CFG1_PORT_MNM_10G_SERDES_NET_INTERFACE_XLAUI		0x11
#define NVM_CFG1_PORT_MNM_10G_SERDES_NET_INTERFACE_XLPPI		0x12
#define NVM_CFG1_PORT_MNM_10G_SERDES_NET_INTERFACE_CAUI			0x21
#define NVM_CFG1_PORT_MNM_10G_SERDES_NET_INTERFACE_CPPI			0x22
#define NVM_CFG1_PORT_MNM_10G_SERDES_NET_INTERFACE_25GAUI		0x31
#define NVM_CFG1_PORT_MNM_10G_ETH_DID_SUFFIX_MASK \
	                                                                0xFF000000
#define NVM_CFG1_PORT_MNM_10G_ETH_DID_SUFFIX_OFFSET			24
	u32 mnm_10g_misc;	/* 0x54 */
#define NVM_CFG1_PORT_MNM_10G_FEC_FORCE_MODE_MASK \
	                                                                0x00000007
#define NVM_CFG1_PORT_MNM_10G_FEC_FORCE_MODE_OFFSET			0
#define NVM_CFG1_PORT_MNM_10G_FEC_FORCE_MODE_NONE			0x0
#define NVM_CFG1_PORT_MNM_10G_FEC_FORCE_MODE_FIRECODE			0x1
#define NVM_CFG1_PORT_MNM_10G_FEC_FORCE_MODE_RS				0x2
#define NVM_CFG1_PORT_MNM_10G_FEC_FORCE_MODE_AUTO			0x7
	u32 mnm_25g_cap;	/* 0x58 */
#define NVM_CFG1_PORT_MNM_25G_DRV_SPEED_CAPABILITY_MASK_MASK \
	                                                                0x0000FFFF
#define NVM_CFG1_PORT_MNM_25G_DRV_SPEED_CAPABILITY_MASK_OFFSET		0
#define NVM_CFG1_PORT_MNM_25G_DRV_SPEED_CAPABILITY_MASK_1G		0x1
#define NVM_CFG1_PORT_MNM_25G_DRV_SPEED_CAPABILITY_MASK_10G		0x2
#define NVM_CFG1_PORT_MNM_25G_DRV_SPEED_CAPABILITY_MASK_20G		0x4
#define NVM_CFG1_PORT_MNM_25G_DRV_SPEED_CAPABILITY_MASK_25G		0x8
#define NVM_CFG1_PORT_MNM_25G_DRV_SPEED_CAPABILITY_MASK_40G		0x10
#define NVM_CFG1_PORT_MNM_25G_DRV_SPEED_CAPABILITY_MASK_50G		0x20
#define NVM_CFG1_PORT_MNM_25G_DRV_SPEED_CAPABILITY_MASK_BB_AHP_100G	0x40
#define NVM_CFG1_PORT_MNM_25G_MFW_SPEED_CAPABILITY_MASK_MASK \
	                                                                0xFFFF0000
#define NVM_CFG1_PORT_MNM_25G_MFW_SPEED_CAPABILITY_MASK_OFFSET		16
#define NVM_CFG1_PORT_MNM_25G_MFW_SPEED_CAPABILITY_MASK_1G		0x1
#define NVM_CFG1_PORT_MNM_25G_MFW_SPEED_CAPABILITY_MASK_10G		0x2
#define NVM_CFG1_PORT_MNM_25G_MFW_SPEED_CAPABILITY_MASK_20G		0x4
#define NVM_CFG1_PORT_MNM_25G_MFW_SPEED_CAPABILITY_MASK_25G		0x8
#define NVM_CFG1_PORT_MNM_25G_MFW_SPEED_CAPABILITY_MASK_40G		0x10
#define NVM_CFG1_PORT_MNM_25G_MFW_SPEED_CAPABILITY_MASK_50G		0x20
#define NVM_CFG1_PORT_MNM_25G_MFW_SPEED_CAPABILITY_MASK_BB_AHP_100G	0x40
	u32 mnm_25g_ctrl;	/* 0x5C */
#define NVM_CFG1_PORT_MNM_25G_DRV_LINK_SPEED_MASK \
	                                                                0x0000000F
#define NVM_CFG1_PORT_MNM_25G_DRV_LINK_SPEED_OFFSET			0
#define NVM_CFG1_PORT_MNM_25G_DRV_LINK_SPEED_AUTONEG			0x0
#define NVM_CFG1_PORT_MNM_25G_DRV_LINK_SPEED_1G				0x1
#define NVM_CFG1_PORT_MNM_25G_DRV_LINK_SPEED_10G			0x2
#define NVM_CFG1_PORT_MNM_25G_DRV_LINK_SPEED_20G			0x3
#define NVM_CFG1_PORT_MNM_25G_DRV_LINK_SPEED_25G			0x4
#define NVM_CFG1_PORT_MNM_25G_DRV_LINK_SPEED_40G			0x5
#define NVM_CFG1_PORT_MNM_25G_DRV_LINK_SPEED_50G			0x6
#define NVM_CFG1_PORT_MNM_25G_DRV_LINK_SPEED_BB_AHP_100G		0x7
#define NVM_CFG1_PORT_MNM_25G_MFW_LINK_SPEED_MASK \
	                                                                0x000000F0
#define NVM_CFG1_PORT_MNM_25G_MFW_LINK_SPEED_OFFSET			4
#define NVM_CFG1_PORT_MNM_25G_MFW_LINK_SPEED_AUTONEG			0x0
#define NVM_CFG1_PORT_MNM_25G_MFW_LINK_SPEED_1G				0x1
#define NVM_CFG1_PORT_MNM_25G_MFW_LINK_SPEED_10G			0x2
#define NVM_CFG1_PORT_MNM_25G_MFW_LINK_SPEED_20G			0x3
#define NVM_CFG1_PORT_MNM_25G_MFW_LINK_SPEED_25G			0x4
#define NVM_CFG1_PORT_MNM_25G_MFW_LINK_SPEED_40G			0x5
#define NVM_CFG1_PORT_MNM_25G_MFW_LINK_SPEED_50G			0x6
#define NVM_CFG1_PORT_MNM_25G_MFW_LINK_SPEED_BB_AHP_100G		0x7
	/*  This field defines the board technology
	 * (backpane,transceiver,external PHY) */
#define NVM_CFG1_PORT_MNM_25G_PORT_TYPE_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_PORT_MNM_25G_PORT_TYPE_OFFSET				8
#define NVM_CFG1_PORT_MNM_25G_PORT_TYPE_UNDEFINED			0x0
#define NVM_CFG1_PORT_MNM_25G_PORT_TYPE_MODULE				0x1
#define NVM_CFG1_PORT_MNM_25G_PORT_TYPE_BACKPLANE			0x2
#define NVM_CFG1_PORT_MNM_25G_PORT_TYPE_EXT_PHY				0x3
#define NVM_CFG1_PORT_MNM_25G_PORT_TYPE_MODULE_SLAVE			0x4
#define NVM_CFG1_PORT_MNM_25G_SERDES_NET_INTERFACE_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_PORT_MNM_25G_SERDES_NET_INTERFACE_OFFSET		16
#define NVM_CFG1_PORT_MNM_25G_SERDES_NET_INTERFACE_BYPASS		0x0
#define NVM_CFG1_PORT_MNM_25G_SERDES_NET_INTERFACE_KR			0x2
#define NVM_CFG1_PORT_MNM_25G_SERDES_NET_INTERFACE_KR2			0x3
#define NVM_CFG1_PORT_MNM_25G_SERDES_NET_INTERFACE_KR4			0x4
#define NVM_CFG1_PORT_MNM_25G_SERDES_NET_INTERFACE_XFI			0x8
#define NVM_CFG1_PORT_MNM_25G_SERDES_NET_INTERFACE_SFI			0x9
#define NVM_CFG1_PORT_MNM_25G_SERDES_NET_INTERFACE_1000X		0xB
#define NVM_CFG1_PORT_MNM_25G_SERDES_NET_INTERFACE_SGMII		0xC
#define NVM_CFG1_PORT_MNM_25G_SERDES_NET_INTERFACE_XLAUI		0x11
#define NVM_CFG1_PORT_MNM_25G_SERDES_NET_INTERFACE_XLPPI		0x12
#define NVM_CFG1_PORT_MNM_25G_SERDES_NET_INTERFACE_CAUI			0x21
#define NVM_CFG1_PORT_MNM_25G_SERDES_NET_INTERFACE_CPPI			0x22
#define NVM_CFG1_PORT_MNM_25G_SERDES_NET_INTERFACE_25GAUI		0x31
#define NVM_CFG1_PORT_MNM_25G_ETH_DID_SUFFIX_MASK \
	                                                                0xFF000000
#define NVM_CFG1_PORT_MNM_25G_ETH_DID_SUFFIX_OFFSET			24
	u32 mnm_25g_misc;	/* 0x60 */
#define NVM_CFG1_PORT_MNM_25G_FEC_FORCE_MODE_MASK \
	                                                                0x00000007
#define NVM_CFG1_PORT_MNM_25G_FEC_FORCE_MODE_OFFSET			0
#define NVM_CFG1_PORT_MNM_25G_FEC_FORCE_MODE_NONE			0x0
#define NVM_CFG1_PORT_MNM_25G_FEC_FORCE_MODE_FIRECODE			0x1
#define NVM_CFG1_PORT_MNM_25G_FEC_FORCE_MODE_RS				0x2
#define NVM_CFG1_PORT_MNM_25G_FEC_FORCE_MODE_AUTO			0x7
	u32 mnm_40g_cap;	/* 0x64 */
#define NVM_CFG1_PORT_MNM_40G_DRV_SPEED_CAPABILITY_MASK_MASK \
	                                                                0x0000FFFF
#define NVM_CFG1_PORT_MNM_40G_DRV_SPEED_CAPABILITY_MASK_OFFSET		0
#define NVM_CFG1_PORT_MNM_40G_DRV_SPEED_CAPABILITY_MASK_1G		0x1
#define NVM_CFG1_PORT_MNM_40G_DRV_SPEED_CAPABILITY_MASK_10G		0x2
#define NVM_CFG1_PORT_MNM_40G_DRV_SPEED_CAPABILITY_MASK_20G		0x4
#define NVM_CFG1_PORT_MNM_40G_DRV_SPEED_CAPABILITY_MASK_25G		0x8
#define NVM_CFG1_PORT_MNM_40G_DRV_SPEED_CAPABILITY_MASK_40G		0x10
#define NVM_CFG1_PORT_MNM_40G_DRV_SPEED_CAPABILITY_MASK_50G		0x20
#define NVM_CFG1_PORT_MNM_40G_DRV_SPEED_CAPABILITY_MASK_BB_AHP_100G	0x40
#define NVM_CFG1_PORT_MNM_40G_MFW_SPEED_CAPABILITY_MASK_MASK \
	                                                                0xFFFF0000
#define NVM_CFG1_PORT_MNM_40G_MFW_SPEED_CAPABILITY_MASK_OFFSET		16
#define NVM_CFG1_PORT_MNM_40G_MFW_SPEED_CAPABILITY_MASK_1G		0x1
#define NVM_CFG1_PORT_MNM_40G_MFW_SPEED_CAPABILITY_MASK_10G		0x2
#define NVM_CFG1_PORT_MNM_40G_MFW_SPEED_CAPABILITY_MASK_20G		0x4
#define NVM_CFG1_PORT_MNM_40G_MFW_SPEED_CAPABILITY_MASK_25G		0x8
#define NVM_CFG1_PORT_MNM_40G_MFW_SPEED_CAPABILITY_MASK_40G		0x10
#define NVM_CFG1_PORT_MNM_40G_MFW_SPEED_CAPABILITY_MASK_50G		0x20
#define NVM_CFG1_PORT_MNM_40G_MFW_SPEED_CAPABILITY_MASK_BB_AHP_100G	0x40
	u32 mnm_40g_ctrl;	/* 0x68 */
#define NVM_CFG1_PORT_MNM_40G_DRV_LINK_SPEED_MASK \
	                                                                0x0000000F
#define NVM_CFG1_PORT_MNM_40G_DRV_LINK_SPEED_OFFSET			0
#define NVM_CFG1_PORT_MNM_40G_DRV_LINK_SPEED_AUTONEG			0x0
#define NVM_CFG1_PORT_MNM_40G_DRV_LINK_SPEED_1G				0x1
#define NVM_CFG1_PORT_MNM_40G_DRV_LINK_SPEED_10G			0x2
#define NVM_CFG1_PORT_MNM_40G_DRV_LINK_SPEED_20G			0x3
#define NVM_CFG1_PORT_MNM_40G_DRV_LINK_SPEED_25G			0x4
#define NVM_CFG1_PORT_MNM_40G_DRV_LINK_SPEED_40G			0x5
#define NVM_CFG1_PORT_MNM_40G_DRV_LINK_SPEED_50G			0x6
#define NVM_CFG1_PORT_MNM_40G_DRV_LINK_SPEED_BB_AHP_100G		0x7
#define NVM_CFG1_PORT_MNM_40G_MFW_LINK_SPEED_MASK \
	                                                                0x000000F0
#define NVM_CFG1_PORT_MNM_40G_MFW_LINK_SPEED_OFFSET			4
#define NVM_CFG1_PORT_MNM_40G_MFW_LINK_SPEED_AUTONEG			0x0
#define NVM_CFG1_PORT_MNM_40G_MFW_LINK_SPEED_1G				0x1
#define NVM_CFG1_PORT_MNM_40G_MFW_LINK_SPEED_10G			0x2
#define NVM_CFG1_PORT_MNM_40G_MFW_LINK_SPEED_20G			0x3
#define NVM_CFG1_PORT_MNM_40G_MFW_LINK_SPEED_25G			0x4
#define NVM_CFG1_PORT_MNM_40G_MFW_LINK_SPEED_40G			0x5
#define NVM_CFG1_PORT_MNM_40G_MFW_LINK_SPEED_50G			0x6
#define NVM_CFG1_PORT_MNM_40G_MFW_LINK_SPEED_BB_AHP_100G		0x7
	/*  This field defines the board technology
	 * (backpane,transceiver,external PHY) */
#define NVM_CFG1_PORT_MNM_40G_PORT_TYPE_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_PORT_MNM_40G_PORT_TYPE_OFFSET				8
#define NVM_CFG1_PORT_MNM_40G_PORT_TYPE_UNDEFINED			0x0
#define NVM_CFG1_PORT_MNM_40G_PORT_TYPE_MODULE				0x1
#define NVM_CFG1_PORT_MNM_40G_PORT_TYPE_BACKPLANE			0x2
#define NVM_CFG1_PORT_MNM_40G_PORT_TYPE_EXT_PHY				0x3
#define NVM_CFG1_PORT_MNM_40G_PORT_TYPE_MODULE_SLAVE			0x4
#define NVM_CFG1_PORT_MNM_40G_SERDES_NET_INTERFACE_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_PORT_MNM_40G_SERDES_NET_INTERFACE_OFFSET		16
#define NVM_CFG1_PORT_MNM_40G_SERDES_NET_INTERFACE_BYPASS		0x0
#define NVM_CFG1_PORT_MNM_40G_SERDES_NET_INTERFACE_KR			0x2
#define NVM_CFG1_PORT_MNM_40G_SERDES_NET_INTERFACE_KR2			0x3
#define NVM_CFG1_PORT_MNM_40G_SERDES_NET_INTERFACE_KR4			0x4
#define NVM_CFG1_PORT_MNM_40G_SERDES_NET_INTERFACE_XFI			0x8
#define NVM_CFG1_PORT_MNM_40G_SERDES_NET_INTERFACE_SFI			0x9
#define NVM_CFG1_PORT_MNM_40G_SERDES_NET_INTERFACE_1000X		0xB
#define NVM_CFG1_PORT_MNM_40G_SERDES_NET_INTERFACE_SGMII		0xC
#define NVM_CFG1_PORT_MNM_40G_SERDES_NET_INTERFACE_XLAUI		0x11
#define NVM_CFG1_PORT_MNM_40G_SERDES_NET_INTERFACE_XLPPI		0x12
#define NVM_CFG1_PORT_MNM_40G_SERDES_NET_INTERFACE_CAUI			0x21
#define NVM_CFG1_PORT_MNM_40G_SERDES_NET_INTERFACE_CPPI			0x22
#define NVM_CFG1_PORT_MNM_40G_SERDES_NET_INTERFACE_25GAUI		0x31
#define NVM_CFG1_PORT_MNM_40G_ETH_DID_SUFFIX_MASK \
	                                                                0xFF000000
#define NVM_CFG1_PORT_MNM_40G_ETH_DID_SUFFIX_OFFSET			24
	u32 mnm_40g_misc;	/* 0x6C */
#define NVM_CFG1_PORT_MNM_40G_FEC_FORCE_MODE_MASK \
	                                                                0x00000007
#define NVM_CFG1_PORT_MNM_40G_FEC_FORCE_MODE_OFFSET			0
#define NVM_CFG1_PORT_MNM_40G_FEC_FORCE_MODE_NONE			0x0
#define NVM_CFG1_PORT_MNM_40G_FEC_FORCE_MODE_FIRECODE			0x1
#define NVM_CFG1_PORT_MNM_40G_FEC_FORCE_MODE_RS				0x2
#define NVM_CFG1_PORT_MNM_40G_FEC_FORCE_MODE_AUTO			0x7
	u32 mnm_50g_cap;	/* 0x70 */
#define NVM_CFG1_PORT_MNM_50G_DRV_SPEED_CAPABILITY_MASK_MASK \
	                                                                0x0000FFFF
#define NVM_CFG1_PORT_MNM_50G_DRV_SPEED_CAPABILITY_MASK_OFFSET		0
#define NVM_CFG1_PORT_MNM_50G_DRV_SPEED_CAPABILITY_MASK_1G		0x1
#define NVM_CFG1_PORT_MNM_50G_DRV_SPEED_CAPABILITY_MASK_10G		0x2
#define NVM_CFG1_PORT_MNM_50G_DRV_SPEED_CAPABILITY_MASK_20G		0x4
#define NVM_CFG1_PORT_MNM_50G_DRV_SPEED_CAPABILITY_MASK_25G		0x8
#define NVM_CFG1_PORT_MNM_50G_DRV_SPEED_CAPABILITY_MASK_40G		0x10
#define NVM_CFG1_PORT_MNM_50G_DRV_SPEED_CAPABILITY_MASK_50G		0x20
#define NVM_CFG1_PORT_MNM_50G_DRV_SPEED_CAPABILITY_MASK_BB_AHP_100G	0x40
#define NVM_CFG1_PORT_MNM_50G_MFW_SPEED_CAPABILITY_MASK_MASK \
	                                                                0xFFFF0000
#define NVM_CFG1_PORT_MNM_50G_MFW_SPEED_CAPABILITY_MASK_OFFSET		16
#define NVM_CFG1_PORT_MNM_50G_MFW_SPEED_CAPABILITY_MASK_1G		0x1
#define NVM_CFG1_PORT_MNM_50G_MFW_SPEED_CAPABILITY_MASK_10G		0x2
#define NVM_CFG1_PORT_MNM_50G_MFW_SPEED_CAPABILITY_MASK_20G		0x4
#define NVM_CFG1_PORT_MNM_50G_MFW_SPEED_CAPABILITY_MASK_25G		0x8
#define NVM_CFG1_PORT_MNM_50G_MFW_SPEED_CAPABILITY_MASK_40G		0x10
#define NVM_CFG1_PORT_MNM_50G_MFW_SPEED_CAPABILITY_MASK_50G		0x20
#define NVM_CFG1_PORT_MNM_50G_MFW_SPEED_CAPABILITY_MASK_BB_AHP_100G	0x40
	u32 mnm_50g_ctrl;	/* 0x74 */
#define NVM_CFG1_PORT_MNM_50G_DRV_LINK_SPEED_MASK \
	                                                                0x0000000F
#define NVM_CFG1_PORT_MNM_50G_DRV_LINK_SPEED_OFFSET			0
#define NVM_CFG1_PORT_MNM_50G_DRV_LINK_SPEED_AUTONEG			0x0
#define NVM_CFG1_PORT_MNM_50G_DRV_LINK_SPEED_1G				0x1
#define NVM_CFG1_PORT_MNM_50G_DRV_LINK_SPEED_10G			0x2
#define NVM_CFG1_PORT_MNM_50G_DRV_LINK_SPEED_20G			0x3
#define NVM_CFG1_PORT_MNM_50G_DRV_LINK_SPEED_25G			0x4
#define NVM_CFG1_PORT_MNM_50G_DRV_LINK_SPEED_40G			0x5
#define NVM_CFG1_PORT_MNM_50G_DRV_LINK_SPEED_50G			0x6
#define NVM_CFG1_PORT_MNM_50G_DRV_LINK_SPEED_BB_AHP_100G		0x7
#define NVM_CFG1_PORT_MNM_50G_MFW_LINK_SPEED_MASK \
	                                                                0x000000F0
#define NVM_CFG1_PORT_MNM_50G_MFW_LINK_SPEED_OFFSET			4
#define NVM_CFG1_PORT_MNM_50G_MFW_LINK_SPEED_AUTONEG			0x0
#define NVM_CFG1_PORT_MNM_50G_MFW_LINK_SPEED_1G				0x1
#define NVM_CFG1_PORT_MNM_50G_MFW_LINK_SPEED_10G			0x2
#define NVM_CFG1_PORT_MNM_50G_MFW_LINK_SPEED_20G			0x3
#define NVM_CFG1_PORT_MNM_50G_MFW_LINK_SPEED_25G			0x4
#define NVM_CFG1_PORT_MNM_50G_MFW_LINK_SPEED_40G			0x5
#define NVM_CFG1_PORT_MNM_50G_MFW_LINK_SPEED_50G			0x6
#define NVM_CFG1_PORT_MNM_50G_MFW_LINK_SPEED_BB_AHP_100G		0x7
	/*  This field defines the board technology
	 * (backpane,transceiver,external PHY) */
#define NVM_CFG1_PORT_MNM_50G_PORT_TYPE_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_PORT_MNM_50G_PORT_TYPE_OFFSET				8
#define NVM_CFG1_PORT_MNM_50G_PORT_TYPE_UNDEFINED			0x0
#define NVM_CFG1_PORT_MNM_50G_PORT_TYPE_MODULE				0x1
#define NVM_CFG1_PORT_MNM_50G_PORT_TYPE_BACKPLANE			0x2
#define NVM_CFG1_PORT_MNM_50G_PORT_TYPE_EXT_PHY				0x3
#define NVM_CFG1_PORT_MNM_50G_PORT_TYPE_MODULE_SLAVE			0x4
#define NVM_CFG1_PORT_MNM_50G_SERDES_NET_INTERFACE_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_PORT_MNM_50G_SERDES_NET_INTERFACE_OFFSET		16
#define NVM_CFG1_PORT_MNM_50G_SERDES_NET_INTERFACE_BYPASS		0x0
#define NVM_CFG1_PORT_MNM_50G_SERDES_NET_INTERFACE_KR			0x2
#define NVM_CFG1_PORT_MNM_50G_SERDES_NET_INTERFACE_KR2			0x3
#define NVM_CFG1_PORT_MNM_50G_SERDES_NET_INTERFACE_KR4			0x4
#define NVM_CFG1_PORT_MNM_50G_SERDES_NET_INTERFACE_XFI			0x8
#define NVM_CFG1_PORT_MNM_50G_SERDES_NET_INTERFACE_SFI			0x9
#define NVM_CFG1_PORT_MNM_50G_SERDES_NET_INTERFACE_1000X		0xB
#define NVM_CFG1_PORT_MNM_50G_SERDES_NET_INTERFACE_SGMII		0xC
#define NVM_CFG1_PORT_MNM_50G_SERDES_NET_INTERFACE_XLAUI		0x11
#define NVM_CFG1_PORT_MNM_50G_SERDES_NET_INTERFACE_XLPPI		0x12
#define NVM_CFG1_PORT_MNM_50G_SERDES_NET_INTERFACE_CAUI			0x21
#define NVM_CFG1_PORT_MNM_50G_SERDES_NET_INTERFACE_CPPI			0x22
#define NVM_CFG1_PORT_MNM_50G_SERDES_NET_INTERFACE_25GAUI		0x31
#define NVM_CFG1_PORT_MNM_50G_ETH_DID_SUFFIX_MASK \
	                                                                0xFF000000
#define NVM_CFG1_PORT_MNM_50G_ETH_DID_SUFFIX_OFFSET			24
	u32 mnm_50g_misc;	/* 0x78 */
#define NVM_CFG1_PORT_MNM_50G_FEC_FORCE_MODE_MASK \
	                                                                0x00000007
#define NVM_CFG1_PORT_MNM_50G_FEC_FORCE_MODE_OFFSET			0
#define NVM_CFG1_PORT_MNM_50G_FEC_FORCE_MODE_NONE			0x0
#define NVM_CFG1_PORT_MNM_50G_FEC_FORCE_MODE_FIRECODE			0x1
#define NVM_CFG1_PORT_MNM_50G_FEC_FORCE_MODE_RS				0x2
#define NVM_CFG1_PORT_MNM_50G_FEC_FORCE_MODE_AUTO			0x7
	u32 mnm_100g_cap;	/* 0x7C */
#define NVM_CFG1_PORT_MNM_100G_DRV_SPEED_CAP_MASK_MASK \
	                                                                0x0000FFFF
#define NVM_CFG1_PORT_MNM_100G_DRV_SPEED_CAP_MASK_OFFSET		0
#define NVM_CFG1_PORT_MNM_100G_DRV_SPEED_CAP_MASK_1G			0x1
#define NVM_CFG1_PORT_MNM_100G_DRV_SPEED_CAP_MASK_10G			0x2
#define NVM_CFG1_PORT_MNM_100G_DRV_SPEED_CAP_MASK_20G			0x4
#define NVM_CFG1_PORT_MNM_100G_DRV_SPEED_CAP_MASK_25G			0x8
#define NVM_CFG1_PORT_MNM_100G_DRV_SPEED_CAP_MASK_40G			0x10
#define NVM_CFG1_PORT_MNM_100G_DRV_SPEED_CAP_MASK_50G			0x20
#define NVM_CFG1_PORT_MNM_100G_DRV_SPEED_CAP_MASK_BB_AHP_100G		0x40
#define NVM_CFG1_PORT_MNM_100G_MFW_SPEED_CAP_MASK_MASK \
	                                                                0xFFFF0000
#define NVM_CFG1_PORT_MNM_100G_MFW_SPEED_CAP_MASK_OFFSET		16
#define NVM_CFG1_PORT_MNM_100G_MFW_SPEED_CAP_MASK_1G			0x1
#define NVM_CFG1_PORT_MNM_100G_MFW_SPEED_CAP_MASK_10G			0x2
#define NVM_CFG1_PORT_MNM_100G_MFW_SPEED_CAP_MASK_20G			0x4
#define NVM_CFG1_PORT_MNM_100G_MFW_SPEED_CAP_MASK_25G			0x8
#define NVM_CFG1_PORT_MNM_100G_MFW_SPEED_CAP_MASK_40G			0x10
#define NVM_CFG1_PORT_MNM_100G_MFW_SPEED_CAP_MASK_50G			0x20
#define NVM_CFG1_PORT_MNM_100G_MFW_SPEED_CAP_MASK_BB_AHP_100G		0x40
	u32 mnm_100g_ctrl;	/* 0x80 */
#define NVM_CFG1_PORT_MNM_100G_DRV_LINK_SPEED_MASK \
	                                                                0x0000000F
#define NVM_CFG1_PORT_MNM_100G_DRV_LINK_SPEED_OFFSET			0
#define NVM_CFG1_PORT_MNM_100G_DRV_LINK_SPEED_AUTONEG			0x0
#define NVM_CFG1_PORT_MNM_100G_DRV_LINK_SPEED_1G			0x1
#define NVM_CFG1_PORT_MNM_100G_DRV_LINK_SPEED_10G			0x2
#define NVM_CFG1_PORT_MNM_100G_DRV_LINK_SPEED_20G			0x3
#define NVM_CFG1_PORT_MNM_100G_DRV_LINK_SPEED_25G			0x4
#define NVM_CFG1_PORT_MNM_100G_DRV_LINK_SPEED_40G			0x5
#define NVM_CFG1_PORT_MNM_100G_DRV_LINK_SPEED_50G			0x6
#define NVM_CFG1_PORT_MNM_100G_DRV_LINK_SPEED_BB_AHP_100G		0x7
#define NVM_CFG1_PORT_MNM_100G_MFW_LINK_SPEED_MASK \
	                                                                0x000000F0
#define NVM_CFG1_PORT_MNM_100G_MFW_LINK_SPEED_OFFSET			4
#define NVM_CFG1_PORT_MNM_100G_MFW_LINK_SPEED_AUTONEG			0x0
#define NVM_CFG1_PORT_MNM_100G_MFW_LINK_SPEED_1G			0x1
#define NVM_CFG1_PORT_MNM_100G_MFW_LINK_SPEED_10G			0x2
#define NVM_CFG1_PORT_MNM_100G_MFW_LINK_SPEED_20G			0x3
#define NVM_CFG1_PORT_MNM_100G_MFW_LINK_SPEED_25G			0x4
#define NVM_CFG1_PORT_MNM_100G_MFW_LINK_SPEED_40G			0x5
#define NVM_CFG1_PORT_MNM_100G_MFW_LINK_SPEED_50G			0x6
#define NVM_CFG1_PORT_MNM_100G_MFW_LINK_SPEED_BB_AHP_100G		0x7
	/*  This field defines the board technology
	 * (backpane,transceiver,external PHY) */
#define NVM_CFG1_PORT_MNM_100G_PORT_TYPE_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_PORT_MNM_100G_PORT_TYPE_OFFSET				8
#define NVM_CFG1_PORT_MNM_100G_PORT_TYPE_UNDEFINED			0x0
#define NVM_CFG1_PORT_MNM_100G_PORT_TYPE_MODULE				0x1
#define NVM_CFG1_PORT_MNM_100G_PORT_TYPE_BACKPLANE			0x2
#define NVM_CFG1_PORT_MNM_100G_PORT_TYPE_EXT_PHY			0x3
#define NVM_CFG1_PORT_MNM_100G_PORT_TYPE_MODULE_SLAVE			0x4
#define NVM_CFG1_PORT_MNM_100G_SERDES_NET_INTERFACE_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_PORT_MNM_100G_SERDES_NET_INTERFACE_OFFSET		16
#define NVM_CFG1_PORT_MNM_100G_SERDES_NET_INTERFACE_BYPASS		0x0
#define NVM_CFG1_PORT_MNM_100G_SERDES_NET_INTERFACE_KR			0x2
#define NVM_CFG1_PORT_MNM_100G_SERDES_NET_INTERFACE_KR2			0x3
#define NVM_CFG1_PORT_MNM_100G_SERDES_NET_INTERFACE_KR4			0x4
#define NVM_CFG1_PORT_MNM_100G_SERDES_NET_INTERFACE_XFI			0x8
#define NVM_CFG1_PORT_MNM_100G_SERDES_NET_INTERFACE_SFI			0x9
#define NVM_CFG1_PORT_MNM_100G_SERDES_NET_INTERFACE_1000X		0xB
#define NVM_CFG1_PORT_MNM_100G_SERDES_NET_INTERFACE_SGMII		0xC
#define NVM_CFG1_PORT_MNM_100G_SERDES_NET_INTERFACE_XLAUI		0x11
#define NVM_CFG1_PORT_MNM_100G_SERDES_NET_INTERFACE_XLPPI		0x12
#define NVM_CFG1_PORT_MNM_100G_SERDES_NET_INTERFACE_CAUI		0x21
#define NVM_CFG1_PORT_MNM_100G_SERDES_NET_INTERFACE_CPPI		0x22
#define NVM_CFG1_PORT_MNM_100G_SERDES_NET_INTERFACE_25GAUI		0x31
#define NVM_CFG1_PORT_MNM_100G_ETH_DID_SUFFIX_MASK \
	                                                                0xFF000000
#define NVM_CFG1_PORT_MNM_100G_ETH_DID_SUFFIX_OFFSET			24
	u32 mnm_100g_misc;	/* 0x84 */
#define NVM_CFG1_PORT_MNM_100G_FEC_FORCE_MODE_MASK \
	                                                                0x00000007
#define NVM_CFG1_PORT_MNM_100G_FEC_FORCE_MODE_OFFSET			0
#define NVM_CFG1_PORT_MNM_100G_FEC_FORCE_MODE_NONE			0x0
#define NVM_CFG1_PORT_MNM_100G_FEC_FORCE_MODE_FIRECODE			0x1
#define NVM_CFG1_PORT_MNM_100G_FEC_FORCE_MODE_RS			0x2
#define NVM_CFG1_PORT_MNM_100G_FEC_FORCE_MODE_AUTO			0x7
	u32 temperature;	/* 0x88 */
#define NVM_CFG1_PORT_PHY_MODULE_DEAD_TEMP_TH_MASK \
	                                                                0x000000FF
#define NVM_CFG1_PORT_PHY_MODULE_DEAD_TEMP_TH_OFFSET			0
#define NVM_CFG1_PORT_PHY_MODULE_ALOM_FAN_ON_TEMP_TH_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_PORT_PHY_MODULE_ALOM_FAN_ON_TEMP_TH_OFFSET		8
	/*  Warning temperature threshold used with nvm option 235 */
#define NVM_CFG1_PORT_PHY_MODULE_WARNING_TEMP_TH_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_PORT_PHY_MODULE_WARNING_TEMP_TH_OFFSET			16
	u32 ext_phy_cfg1;	/* 0x8C */
	/*  Ext PHY MDI pair swap value */
#define NVM_CFG1_PORT_EXT_PHY_MDI_PAIR_SWAP_MASK \
	                                                                0x0000FFFF
#define NVM_CFG1_PORT_EXT_PHY_MDI_PAIR_SWAP_OFFSET			0
	u32 extended_speed;	/* 0x90 */
	/*  Sets speed in conjunction with legacy speed field */
#define NVM_CFG1_PORT_EXTENDED_SPEED_MASK \
	                                                                0x0000FFFF
#define NVM_CFG1_PORT_EXTENDED_SPEED_OFFSET				0
#define NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_AN			0x1
#define NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_1G			0x2
#define NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_10G			0x4
#define NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_20G			0x8
#define NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_25G			0x10
#define NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_40G			0x20
#define NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_50G_R			0x40
#define NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_50G_R2			0x80
#define NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_100G_R2			0x100
#define NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_100G_R4			0x200
#define NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_100G_P4			0x400
	/*  Sets speed capabilities in conjunction with legacy capabilities
	 * field */
#define NVM_CFG1_PORT_EXTENDED_SPEED_CAP_MASK \
	                                                                0xFFFF0000
#define NVM_CFG1_PORT_EXTENDED_SPEED_CAP_OFFSET				16
#define NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_RESERVED		0x1
#define NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_1G			0x2
#define NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_10G			0x4
#define NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_20G			0x8
#define NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_25G			0x10
#define NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_40G			0x20
#define NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_50G_R		0x40
#define NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_50G_R2		0x80
#define NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_100G_R2		0x100
#define NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_100G_R4		0x200
#define NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_100G_P4		0x400
	/*  Set speed specific FEC setting in conjunction with legacy FEC
	 * mode */
	u32 extended_fec_mode;	/* 0x94 */
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_NONE			0x1
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_10G_NONE		0x2
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_10G_BASE_R		0x4
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_20G_NONE		0x8
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_20G_BASE_R		0x10
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_25G_NONE		0x20
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_25G_BASE_R		0x40
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_25G_RS528		0x80
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_40G_NONE		0x100
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_40G_BASE_R		0x200
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_50G_NONE		0x400
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_50G_BASE_R		0x800
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_50G_RS528		0x1000
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_50G_RS544		0x2000
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_100G_NONE		0x4000
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_100G_BASE_R		0x8000
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_100G_RS528		0x10000
#define NVM_CFG1_PORT_EXTENDED_FEC_MODE_EXTND_FEC_100G_RS544		0x20000
	u32 port_generic_cont_01;	/* 0x98 */
	/*  Define for GPIO mapping of SFP Rate Select 0 */
#define NVM_CFG1_PORT_MODULE_RS0_MASK \
	                                                                0x000000FF
#define NVM_CFG1_PORT_MODULE_RS0_OFFSET					0
#define NVM_CFG1_PORT_MODULE_RS0_NA					0x0
#define NVM_CFG1_PORT_MODULE_RS0_GPIO0					0x1
#define NVM_CFG1_PORT_MODULE_RS0_GPIO1					0x2
#define NVM_CFG1_PORT_MODULE_RS0_GPIO2					0x3
#define NVM_CFG1_PORT_MODULE_RS0_GPIO3					0x4
#define NVM_CFG1_PORT_MODULE_RS0_GPIO4					0x5
#define NVM_CFG1_PORT_MODULE_RS0_GPIO5					0x6
#define NVM_CFG1_PORT_MODULE_RS0_GPIO6					0x7
#define NVM_CFG1_PORT_MODULE_RS0_GPIO7					0x8
#define NVM_CFG1_PORT_MODULE_RS0_GPIO8					0x9
#define NVM_CFG1_PORT_MODULE_RS0_GPIO9					0xA
#define NVM_CFG1_PORT_MODULE_RS0_GPIO10					0xB
#define NVM_CFG1_PORT_MODULE_RS0_GPIO11					0xC
#define NVM_CFG1_PORT_MODULE_RS0_GPIO12					0xD
#define NVM_CFG1_PORT_MODULE_RS0_GPIO13					0xE
#define NVM_CFG1_PORT_MODULE_RS0_GPIO14					0xF
#define NVM_CFG1_PORT_MODULE_RS0_GPIO15					0x10
#define NVM_CFG1_PORT_MODULE_RS0_GPIO16					0x11
#define NVM_CFG1_PORT_MODULE_RS0_GPIO17					0x12
#define NVM_CFG1_PORT_MODULE_RS0_GPIO18					0x13
#define NVM_CFG1_PORT_MODULE_RS0_GPIO19					0x14
#define NVM_CFG1_PORT_MODULE_RS0_GPIO20					0x15
#define NVM_CFG1_PORT_MODULE_RS0_GPIO21					0x16
#define NVM_CFG1_PORT_MODULE_RS0_GPIO22					0x17
#define NVM_CFG1_PORT_MODULE_RS0_GPIO23					0x18
#define NVM_CFG1_PORT_MODULE_RS0_GPIO24					0x19
#define NVM_CFG1_PORT_MODULE_RS0_GPIO25					0x1A
#define NVM_CFG1_PORT_MODULE_RS0_GPIO26					0x1B
#define NVM_CFG1_PORT_MODULE_RS0_GPIO27					0x1C
#define NVM_CFG1_PORT_MODULE_RS0_GPIO28					0x1D
#define NVM_CFG1_PORT_MODULE_RS0_GPIO29					0x1E
#define NVM_CFG1_PORT_MODULE_RS0_GPIO30					0x1F
#define NVM_CFG1_PORT_MODULE_RS0_GPIO31					0x20
	/*  Define for GPIO mapping of SFP Rate Select 1 */
#define NVM_CFG1_PORT_MODULE_RS1_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_PORT_MODULE_RS1_OFFSET					8
#define NVM_CFG1_PORT_MODULE_RS1_NA					0x0
#define NVM_CFG1_PORT_MODULE_RS1_GPIO0					0x1
#define NVM_CFG1_PORT_MODULE_RS1_GPIO1					0x2
#define NVM_CFG1_PORT_MODULE_RS1_GPIO2					0x3
#define NVM_CFG1_PORT_MODULE_RS1_GPIO3					0x4
#define NVM_CFG1_PORT_MODULE_RS1_GPIO4					0x5
#define NVM_CFG1_PORT_MODULE_RS1_GPIO5					0x6
#define NVM_CFG1_PORT_MODULE_RS1_GPIO6					0x7
#define NVM_CFG1_PORT_MODULE_RS1_GPIO7					0x8
#define NVM_CFG1_PORT_MODULE_RS1_GPIO8					0x9
#define NVM_CFG1_PORT_MODULE_RS1_GPIO9					0xA
#define NVM_CFG1_PORT_MODULE_RS1_GPIO10					0xB
#define NVM_CFG1_PORT_MODULE_RS1_GPIO11					0xC
#define NVM_CFG1_PORT_MODULE_RS1_GPIO12					0xD
#define NVM_CFG1_PORT_MODULE_RS1_GPIO13					0xE
#define NVM_CFG1_PORT_MODULE_RS1_GPIO14					0xF
#define NVM_CFG1_PORT_MODULE_RS1_GPIO15					0x10
#define NVM_CFG1_PORT_MODULE_RS1_GPIO16					0x11
#define NVM_CFG1_PORT_MODULE_RS1_GPIO17					0x12
#define NVM_CFG1_PORT_MODULE_RS1_GPIO18					0x13
#define NVM_CFG1_PORT_MODULE_RS1_GPIO19					0x14
#define NVM_CFG1_PORT_MODULE_RS1_GPIO20					0x15
#define NVM_CFG1_PORT_MODULE_RS1_GPIO21					0x16
#define NVM_CFG1_PORT_MODULE_RS1_GPIO22					0x17
#define NVM_CFG1_PORT_MODULE_RS1_GPIO23					0x18
#define NVM_CFG1_PORT_MODULE_RS1_GPIO24					0x19
#define NVM_CFG1_PORT_MODULE_RS1_GPIO25					0x1A
#define NVM_CFG1_PORT_MODULE_RS1_GPIO26					0x1B
#define NVM_CFG1_PORT_MODULE_RS1_GPIO27					0x1C
#define NVM_CFG1_PORT_MODULE_RS1_GPIO28					0x1D
#define NVM_CFG1_PORT_MODULE_RS1_GPIO29					0x1E
#define NVM_CFG1_PORT_MODULE_RS1_GPIO30					0x1F
#define NVM_CFG1_PORT_MODULE_RS1_GPIO31					0x20
	/*  Define for GPIO mapping of SFP Module TX Fault */
#define NVM_CFG1_PORT_MODULE_TX_FAULT_MASK \
	                                                                0x00FF0000
#define NVM_CFG1_PORT_MODULE_TX_FAULT_OFFSET				16
#define NVM_CFG1_PORT_MODULE_TX_FAULT_NA				0x0
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO0				0x1
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO1				0x2
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO2				0x3
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO3				0x4
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO4				0x5
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO5				0x6
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO6				0x7
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO7				0x8
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO8				0x9
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO9				0xA
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO10				0xB
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO11				0xC
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO12				0xD
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO13				0xE
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO14				0xF
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO15				0x10
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO16				0x11
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO17				0x12
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO18				0x13
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO19				0x14
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO20				0x15
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO21				0x16
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO22				0x17
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO23				0x18
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO24				0x19
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO25				0x1A
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO26				0x1B
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO27				0x1C
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO28				0x1D
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO29				0x1E
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO30				0x1F
#define NVM_CFG1_PORT_MODULE_TX_FAULT_GPIO31				0x20
	/*  Define for GPIO mapping of QSFP Reset signal */
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_MASK \
	                                                                0xFF000000
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_OFFSET				24
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_NA				0x0
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO0				0x1
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO1				0x2
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO2				0x3
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO3				0x4
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO4				0x5
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO5				0x6
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO6				0x7
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO7				0x8
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO8				0x9
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO9				0xA
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO10				0xB
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO11				0xC
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO12				0xD
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO13				0xE
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO14				0xF
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO15				0x10
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO16				0x11
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO17				0x12
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO18				0x13
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO19				0x14
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO20				0x15
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO21				0x16
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO22				0x17
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO23				0x18
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO24				0x19
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO25				0x1A
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO26				0x1B
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO27				0x1C
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO28				0x1D
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO29				0x1E
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO30				0x1F
#define NVM_CFG1_PORT_QSFP_MODULE_RESET_GPIO31				0x20
	u32 port_generic_cont_02;	/* 0x9C */
	/*  Define for GPIO mapping of QSFP Transceiver LP mode */
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_MASK \
	                                                                0x000000FF
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_OFFSET			0
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_NA				0x0
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO0				0x1
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO1				0x2
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO2				0x3
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO3				0x4
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO4				0x5
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO5				0x6
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO6				0x7
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO7				0x8
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO8				0x9
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO9				0xA
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO10			0xB
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO11			0xC
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO12			0xD
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO13			0xE
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO14			0xF
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO15			0x10
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO16			0x11
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO17			0x12
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO18			0x13
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO19			0x14
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO20			0x15
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO21			0x16
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO22			0x17
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO23			0x18
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO24			0x19
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO25			0x1A
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO26			0x1B
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO27			0x1C
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO28			0x1D
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO29			0x1E
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO30			0x1F
#define NVM_CFG1_PORT_QSFP_MODULE_LP_MODE_GPIO31			0x20
	/*  Define for GPIO mapping of Transceiver Power Enable */
#define NVM_CFG1_PORT_MODULE_POWER_MASK \
	                                                                0x0000FF00
#define NVM_CFG1_PORT_MODULE_POWER_OFFSET				8
#define NVM_CFG1_PORT_MODULE_POWER_NA					0x0
#define NVM_CFG1_PORT_MODULE_POWER_GPIO0				0x1
#define NVM_CFG1_PORT_MODULE_POWER_GPIO1				0x2
#define NVM_CFG1_PORT_MODULE_POWER_GPIO2				0x3
#define NVM_CFG1_PORT_MODULE_POWER_GPIO3				0x4
#define NVM_CFG1_PORT_MODULE_POWER_GPIO4				0x5
#define NVM_CFG1_PORT_MODULE_POWER_GPIO5				0x6
#define NVM_CFG1_PORT_MODULE_POWER_GPIO6				0x7
#define NVM_CFG1_PORT_MODULE_POWER_GPIO7				0x8
#define NVM_CFG1_PORT_MODULE_POWER_GPIO8				0x9
#define NVM_CFG1_PORT_MODULE_POWER_GPIO9				0xA
#define NVM_CFG1_PORT_MODULE_POWER_GPIO10				0xB
#define NVM_CFG1_PORT_MODULE_POWER_GPIO11				0xC
#define NVM_CFG1_PORT_MODULE_POWER_GPIO12				0xD
#define NVM_CFG1_PORT_MODULE_POWER_GPIO13				0xE
#define NVM_CFG1_PORT_MODULE_POWER_GPIO14				0xF
#define NVM_CFG1_PORT_MODULE_POWER_GPIO15				0x10
#define NVM_CFG1_PORT_MODULE_POWER_GPIO16				0x11
#define NVM_CFG1_PORT_MODULE_POWER_GPIO17				0x12
#define NVM_CFG1_PORT_MODULE_POWER_GPIO18				0x13
#define NVM_CFG1_PORT_MODULE_POWER_GPIO19				0x14
#define NVM_CFG1_PORT_MODULE_POWER_GPIO20				0x15
#define NVM_CFG1_PORT_MODULE_POWER_GPIO21				0x16
#define NVM_CFG1_PORT_MODULE_POWER_GPIO22				0x17
#define NVM_CFG1_PORT_MODULE_POWER_GPIO23				0x18
#define NVM_CFG1_PORT_MODULE_POWER_GPIO24				0x19
#define NVM_CFG1_PORT_MODULE_POWER_GPIO25				0x1A
#define NVM_CFG1_PORT_MODULE_POWER_GPIO26				0x1B
#define NVM_CFG1_PORT_MODULE_POWER_GPIO27				0x1C
#define NVM_CFG1_PORT_MODULE_POWER_GPIO28				0x1D
#define NVM_CFG1_PORT_MODULE_POWER_GPIO29				0x1E
#define NVM_CFG1_PORT_MODULE_POWER_GPIO30				0x1F
#define NVM_CFG1_PORT_MODULE_POWER_GPIO31				0x20
	/*  Define for LASI Mapping of Interrupt from module or PHY */
#define NVM_CFG1_PORT_LASI_INTR_IN_MASK \
	                                                                0x000F0000
#define NVM_CFG1_PORT_LASI_INTR_IN_OFFSET				16
#define NVM_CFG1_PORT_LASI_INTR_IN_NA					0x0
#define NVM_CFG1_PORT_LASI_INTR_IN_LASI0				0x1
#define NVM_CFG1_PORT_LASI_INTR_IN_LASI1				0x2
#define NVM_CFG1_PORT_LASI_INTR_IN_LASI2				0x3
#define NVM_CFG1_PORT_LASI_INTR_IN_LASI3				0x4
	/*  Define for GPIO mapping of QSFP MODSEL */
#define NVM_CFG1_PORT_QSFP_MODSEL_MASK \
	                                                                0x0FF00000
#define NVM_CFG1_PORT_QSFP_MODSEL_OFFSET				20
#define NVM_CFG1_PORT_QSFP_MODSEL_NA					0x0
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO0					0x1
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO1					0x2
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO2					0x3
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO3					0x4
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO4					0x5
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO5					0x6
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO6					0x7
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO7					0x8
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO8					0x9
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO9					0xA
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO10				0xB
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO11				0xC
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO12				0xD
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO13				0xE
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO14				0xF
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO15				0x10
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO16				0x11
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO17				0x12
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO18				0x13
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO19				0x14
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO20				0x15
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO21				0x16
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO22				0x17
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO23				0x18
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO24				0x19
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO25				0x1A
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO26				0x1B
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO27				0x1C
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO28				0x1D
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO29				0x1E
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO30				0x1F
#define NVM_CFG1_PORT_QSFP_MODSEL_GPIO31				0x20
	u32 phy_temp_monitor;	/* 0xA0 */
	/*  Enables PHY temperature monitoring. In case PHY temperature rise
	 * above option 308 (Temperature monitoring PHY temp) for option 309
	 * (#samples) times, MFW will shutdown the chip. */
#define NVM_CFG1_PORT_SHUTDOWN__M_PHY_MONITOR_ENABLED_MASK \
	                                                                0x00000001
#define NVM_CFG1_PORT_SHUTDOWN__M_PHY_MONITOR_ENABLED_OFFSET		0
#define NVM_CFG1_PORT_SHUTDOWN__M_PHY_MONITOR_ENABLED_DISABLED		0x0
#define NVM_CFG1_PORT_SHUTDOWN__M_PHY_MONITOR_ENABLED_ENABLED		0x1
	/*  In case option 307 is enabled, this option determinges the
	 * temperature theshold to shutdown the chip if sampled more than
	 * [option 309 (#samples)] times in a row */
#define NVM_CFG1_PORT_SHUTDOWN__M_PHY_THRESHOLD_TEMP_MASK \
	                                                                0x000001FE
#define NVM_CFG1_PORT_SHUTDOWN__M_PHY_THRESHOLD_TEMP_OFFSET		1
	/*  Number of PHY temperature samples above option 308 required in a
	 * row for MFW to shutdown the chip. */
#define NVM_CFG1_PORT_SHUTDOWN__M_PHY_SAMPLES_MASK \
	                                                                0x0001FE00
#define NVM_CFG1_PORT_SHUTDOWN__M_PHY_SAMPLES_OFFSET			9
	u32 reserved[109];	/* 0xA4 */
};

struct nvm_cfg1_func {
	struct nvm_cfg_mac_address mac_address;	/* 0x0 */
	u32 rsrv1;		/* 0x8 */
#define NVM_CFG1_FUNC_RESERVED1_MASK				0x0000FFFF
#define NVM_CFG1_FUNC_RESERVED1_OFFSET				0
#define NVM_CFG1_FUNC_RESERVED2_MASK				0xFFFF0000
#define NVM_CFG1_FUNC_RESERVED2_OFFSET				16
	u32 rsrv2;		/* 0xC */
#define NVM_CFG1_FUNC_RESERVED3_MASK				0x0000FFFF
#define NVM_CFG1_FUNC_RESERVED3_OFFSET				0
#define NVM_CFG1_FUNC_RESERVED4_MASK				0xFFFF0000
#define NVM_CFG1_FUNC_RESERVED4_OFFSET				16
	u32 device_id;		/* 0x10 */
#define NVM_CFG1_FUNC_MF_VENDOR_DEVICE_ID_MASK			0x0000FFFF
#define NVM_CFG1_FUNC_MF_VENDOR_DEVICE_ID_OFFSET		0
#define NVM_CFG1_FUNC_RESERVED77_MASK				0xFFFF0000
#define NVM_CFG1_FUNC_RESERVED77_OFFSET				16
	u32 cmn_cfg;		/* 0x14 */
#define NVM_CFG1_FUNC_PREBOOT_BOOT_PROTOCOL_MASK		0x00000007
#define NVM_CFG1_FUNC_PREBOOT_BOOT_PROTOCOL_OFFSET		0
#define NVM_CFG1_FUNC_PREBOOT_BOOT_PROTOCOL_PXE			0x0
#define NVM_CFG1_FUNC_PREBOOT_BOOT_PROTOCOL_ISCSI_BOOT		0x3
#define NVM_CFG1_FUNC_PREBOOT_BOOT_PROTOCOL_FCOE_BOOT		0x4
#define NVM_CFG1_FUNC_PREBOOT_BOOT_PROTOCOL_NVMETCP		0x5
#define NVM_CFG1_FUNC_PREBOOT_BOOT_PROTOCOL_NONE		0x7
#define NVM_CFG1_FUNC_VF_PCI_DEVICE_ID_MASK			0x0007FFF8
#define NVM_CFG1_FUNC_VF_PCI_DEVICE_ID_OFFSET			3
#define NVM_CFG1_FUNC_PERSONALITY_MASK				0x00780000
#define NVM_CFG1_FUNC_PERSONALITY_OFFSET			19
#define NVM_CFG1_FUNC_PERSONALITY_ETHERNET			0x0
#define NVM_CFG1_FUNC_PERSONALITY_ISCSI				0x1
#define NVM_CFG1_FUNC_PERSONALITY_FCOE				0x2
#define NVM_CFG1_FUNC_PERSONALITY_NVMETCP			0x4
#define NVM_CFG1_FUNC_BANDWIDTH_WEIGHT_MASK			0x7F800000
#define NVM_CFG1_FUNC_BANDWIDTH_WEIGHT_OFFSET			23
#define NVM_CFG1_FUNC_PAUSE_ON_HOST_RING_MASK			0x80000000
#define NVM_CFG1_FUNC_PAUSE_ON_HOST_RING_OFFSET			31
#define NVM_CFG1_FUNC_PAUSE_ON_HOST_RING_DISABLED		0x0
#define NVM_CFG1_FUNC_PAUSE_ON_HOST_RING_ENABLED		0x1
	u32 pci_cfg;		/* 0x18 */
#define NVM_CFG1_FUNC_NUMBER_OF_VFS_PER_PF_MASK			0x0000007F
#define NVM_CFG1_FUNC_NUMBER_OF_VFS_PER_PF_OFFSET		0
	/*  AH VF BAR2 size */
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_MASK			0x00003F80
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_OFFSET			7
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_DISABLED			0x0
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_4K			0x1
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_8K			0x2
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_16K			0x3
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_32K			0x4
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_64K			0x5
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_128K			0x6
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_256K			0x7
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_512K			0x8
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_1M			0x9
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_2M			0xA
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_4M			0xB
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_8M			0xC
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_16M			0xD
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_32M			0xE
#define NVM_CFG1_FUNC_VF_PCI_BAR2_SIZE_64M			0xF
#define NVM_CFG1_FUNC_BAR1_SIZE_MASK				0x0003C000
#define NVM_CFG1_FUNC_BAR1_SIZE_OFFSET				14
#define NVM_CFG1_FUNC_BAR1_SIZE_DISABLED			0x0
#define NVM_CFG1_FUNC_BAR1_SIZE_64K				0x1
#define NVM_CFG1_FUNC_BAR1_SIZE_128K				0x2
#define NVM_CFG1_FUNC_BAR1_SIZE_256K				0x3
#define NVM_CFG1_FUNC_BAR1_SIZE_512K				0x4
#define NVM_CFG1_FUNC_BAR1_SIZE_1M				0x5
#define NVM_CFG1_FUNC_BAR1_SIZE_2M				0x6
#define NVM_CFG1_FUNC_BAR1_SIZE_4M				0x7
#define NVM_CFG1_FUNC_BAR1_SIZE_8M				0x8
#define NVM_CFG1_FUNC_BAR1_SIZE_16M				0x9
#define NVM_CFG1_FUNC_BAR1_SIZE_32M				0xA
#define NVM_CFG1_FUNC_BAR1_SIZE_64M				0xB
#define NVM_CFG1_FUNC_BAR1_SIZE_128M				0xC
#define NVM_CFG1_FUNC_BAR1_SIZE_256M				0xD
#define NVM_CFG1_FUNC_BAR1_SIZE_512M				0xE
#define NVM_CFG1_FUNC_BAR1_SIZE_1G				0xF
#define NVM_CFG1_FUNC_MAX_BANDWIDTH_MASK			0x03FC0000
#define NVM_CFG1_FUNC_MAX_BANDWIDTH_OFFSET			18
	/*  Hide function in npar mode */
#define NVM_CFG1_FUNC_FUNCTION_HIDE_MASK			0x04000000
#define NVM_CFG1_FUNC_FUNCTION_HIDE_OFFSET			26
#define NVM_CFG1_FUNC_FUNCTION_HIDE_DISABLED			0x0
#define NVM_CFG1_FUNC_FUNCTION_HIDE_ENABLED			0x1
	/*  AH BAR2 size (per function) */
#define NVM_CFG1_FUNC_BAR2_SIZE_MASK				0x78000000
#define NVM_CFG1_FUNC_BAR2_SIZE_OFFSET				27
#define NVM_CFG1_FUNC_BAR2_SIZE_DISABLED			0x0
#define NVM_CFG1_FUNC_BAR2_SIZE_1M				0x5
#define NVM_CFG1_FUNC_BAR2_SIZE_2M				0x6
#define NVM_CFG1_FUNC_BAR2_SIZE_4M				0x7
#define NVM_CFG1_FUNC_BAR2_SIZE_8M				0x8
#define NVM_CFG1_FUNC_BAR2_SIZE_16M				0x9
#define NVM_CFG1_FUNC_BAR2_SIZE_32M				0xA
#define NVM_CFG1_FUNC_BAR2_SIZE_64M				0xB
#define NVM_CFG1_FUNC_BAR2_SIZE_128M				0xC
#define NVM_CFG1_FUNC_BAR2_SIZE_256M				0xD
#define NVM_CFG1_FUNC_BAR2_SIZE_512M				0xE
#define NVM_CFG1_FUNC_BAR2_SIZE_1G				0xF
	struct nvm_cfg_mac_address fcoe_node_wwn_mac_addr;	/* 0x1C */
	struct nvm_cfg_mac_address fcoe_port_wwn_mac_addr;	/* 0x24 */
	u32 preboot_generic_cfg;	/* 0x2C */
#define NVM_CFG1_FUNC_PREBOOT_VLAN_VALUE_MASK			0x0000FFFF
#define NVM_CFG1_FUNC_PREBOOT_VLAN_VALUE_OFFSET			0
#define NVM_CFG1_FUNC_PREBOOT_VLAN_MASK				0x00010000
#define NVM_CFG1_FUNC_PREBOOT_VLAN_OFFSET			16
#define NVM_CFG1_FUNC_NPAR_ENABLED_PROTOCOL_MASK		0x01FE0000
#define NVM_CFG1_FUNC_NPAR_ENABLED_PROTOCOL_OFFSET		17
#define NVM_CFG1_FUNC_NPAR_ENABLED_PROTOCOL_ETHERNET		0x1
#define NVM_CFG1_FUNC_NPAR_ENABLED_PROTOCOL_FCOE		0x2
#define NVM_CFG1_FUNC_NPAR_ENABLED_PROTOCOL_ISCSI		0x4
#define NVM_CFG1_FUNC_NPAR_ENABLED_PROTOCOL_RDMA		0x8
#define NVM_CFG1_FUNC_NPAR_ENABLED_PROTOCOL_NVMETCP		0x10
	u32 features;		/* 0x30 */
	/*  RDMA protocol enablement  */
#define NVM_CFG1_FUNC_RDMA_ENABLEMENT_MASK			0x00000003
#define NVM_CFG1_FUNC_RDMA_ENABLEMENT_OFFSET			0
#define NVM_CFG1_FUNC_RDMA_ENABLEMENT_NONE			0x0
#define NVM_CFG1_FUNC_RDMA_ENABLEMENT_ROCE			0x1
#define NVM_CFG1_FUNC_RDMA_ENABLEMENT_IWARP			0x2
#define NVM_CFG1_FUNC_RDMA_ENABLEMENT_BOTH			0x3
	u32 mf_mode_feature;	/* 0x34 */
	/*  QinQ Outer VLAN */
#define NVM_CFG1_FUNC_QINQ_OUTER_VLAN_MASK			0x0000FFFF
#define NVM_CFG1_FUNC_QINQ_OUTER_VLAN_OFFSET			0
	u32 reserved[6];	/* 0x38 */
};

struct nvm_cfg1 {
	struct nvm_cfg1_glob glob;	/* 0x0 */
	struct nvm_cfg1_path path[MCP_GLOB_PATH_MAX];	/* 0x228 */
	struct nvm_cfg1_port port[MCP_GLOB_PORT_MAX];	/* 0x230 */
	struct nvm_cfg1_func func[MCP_GLOB_FUNC_MAX];	/* 0xB90 */
};

/******************************************
* nvm_cfg structs
******************************************/

struct board_info {
	u16 vendor_id;
	u16 eth_did_suffix;
	u16 sub_vendor_id;
	u16 sub_device_id;
	char *board_name;
	char *friendly_name;
};

struct trace_module_info {
	char *module_name;
};
#define NUM_TRACE_MODULES    25

enum nvm_cfg_sections {
	NVM_CFG_SECTION_NVM_CFG1,
	NVM_CFG_SECTION_MAX
};

struct nvm_cfg {
	u32 num_sections;
	u32 sections_offset[NVM_CFG_SECTION_MAX];
	struct nvm_cfg1 cfg1;
};

/******************************************
* nvm_cfg options
******************************************/

#define NVM_CFG_ID_MAC_ADDRESS				1
#define NVM_CFG_ID_BOARD_SWAP				8
#define NVM_CFG_ID_MF_MODE				9
#define NVM_CFG_ID_LED_MODE				10
#define NVM_CFG_ID_FAN_FAILURE_ENFORCEMENT		11
#define NVM_CFG_ID_ENGINEERING_CHANGE			12
#define NVM_CFG_ID_MANUFACTURING_ID			13
#define NVM_CFG_ID_SERIAL_NUMBER			14
#define NVM_CFG_ID_PCI_GEN				15
#define NVM_CFG_ID_BEACON_WOL_ENABLED			16
#define NVM_CFG_ID_ASPM_SUPPORT				17
#define NVM_CFG_ID_ROCE_PRIORITY			20
#define NVM_CFG_ID_ENABLE_WOL_ON_ACPI_PATTERN		22
#define NVM_CFG_ID_MAGIC_PACKET_WOL			23
#define NVM_CFG_ID_AVS_MARGIN_LOW_BB			24
#define NVM_CFG_ID_AVS_MARGIN_HIGH_BB			25
#define NVM_CFG_ID_DCBX_MODE				26
#define NVM_CFG_ID_DRV_SPEED_CAPABILITY_MASK		27
#define NVM_CFG_ID_MFW_SPEED_CAPABILITY_MASK		28
#define NVM_CFG_ID_DRV_LINK_SPEED			29
#define NVM_CFG_ID_DRV_FLOW_CONTROL			30
#define NVM_CFG_ID_MFW_LINK_SPEED			31
#define NVM_CFG_ID_MFW_FLOW_CONTROL			32
#define NVM_CFG_ID_OPTIC_MODULE_VENDOR_ENFORCEMENT	33
#define NVM_CFG_ID_OPTIONAL_LINK_MODES_BB		34
#define NVM_CFG_ID_NETWORK_PORT_MODE			38
#define NVM_CFG_ID_MPS10_RX_LANE_SWAP_BB		39
#define NVM_CFG_ID_MPS10_TX_LANE_SWAP_BB		40
#define NVM_CFG_ID_MPS10_RX_LANE_POLARITY_BB		41
#define NVM_CFG_ID_MPS10_TX_LANE_POLARITY_BB		42
#define NVM_CFG_ID_MPS25_RX_LANE_SWAP_BB		43
#define NVM_CFG_ID_MPS25_TX_LANE_SWAP_BB		44
#define NVM_CFG_ID_MPS25_RX_LANE_POLARITY		45
#define NVM_CFG_ID_MPS25_TX_LANE_POLARITY		46
#define NVM_CFG_ID_MPS10_PREEMPHASIS_BB			47
#define NVM_CFG_ID_MPS10_DRIVER_CURRENT_BB		48
#define NVM_CFG_ID_MPS10_ENFORCE_TX_FIR_CFG_BB		49
#define NVM_CFG_ID_MPS25_PREEMPHASIS_BB_K2		50
#define NVM_CFG_ID_MPS25_DRIVER_CURRENT_BB_K2		51
#define NVM_CFG_ID_MPS25_ENFORCE_TX_FIR_CFG		52
#define NVM_CFG_ID_MPS10_CORE_ADDR_BB			53
#define NVM_CFG_ID_MPS25_CORE_ADDR_BB			54
#define NVM_CFG_ID_EXTERNAL_PHY_TYPE			55
#define NVM_CFG_ID_EXTERNAL_PHY_ADDRESS			56
#define NVM_CFG_ID_SERDES_NET_INTERFACE_BB		57
#define NVM_CFG_ID_AN_MODE_BB				58
#define NVM_CFG_ID_PREBOOT_OPROM			59
#define NVM_CFG_ID_MBA_DELAY_TIME			61
#define NVM_CFG_ID_MBA_SETUP_HOT_KEY			62
#define NVM_CFG_ID_MBA_HIDE_SETUP_PROMPT		63
#define NVM_CFG_ID_PREBOOT_LINK_SPEED_BB_K2		67
#define NVM_CFG_ID_PREBOOT_BOOT_PROTOCOL		69
#define NVM_CFG_ID_ENABLE_SRIOV				70
#define NVM_CFG_ID_ENABLE_ATC				71
#define NVM_CFG_ID_NUMBER_OF_VFS_PER_PF			74
#define NVM_CFG_ID_VF_PCI_BAR2_SIZE_K2_E5		75
#define NVM_CFG_ID_VENDOR_ID				76
#define NVM_CFG_ID_SUBSYSTEM_VENDOR_ID			78
#define NVM_CFG_ID_SUBSYSTEM_DEVICE_ID			79
#define NVM_CFG_ID_EXPANSION_ROM_SIZE			80
#define NVM_CFG_ID_VF_PCI_BAR2_SIZE_BB			81
#define NVM_CFG_ID_BAR1_SIZE				82
#define NVM_CFG_ID_BAR2_SIZE_BB				83
#define NVM_CFG_ID_VF_PCI_DEVICE_ID			84
#define NVM_CFG_ID_MPS10_TXFIR_MAIN_BB			85
#define NVM_CFG_ID_MPS10_TXFIR_POST_BB			86
#define NVM_CFG_ID_MPS25_TXFIR_MAIN_BB_K2		87
#define NVM_CFG_ID_MPS25_TXFIR_POST_BB_K2		88
#define NVM_CFG_ID_MANUFACTURE_KIT_VERSION		89
#define NVM_CFG_ID_MANUFACTURE_TIMESTAMP		90
#define NVM_CFG_ID_PERSONALITY				92
#define NVM_CFG_ID_FCOE_NODE_WWN_MAC_ADDR		93
#define NVM_CFG_ID_FCOE_PORT_WWN_MAC_ADDR		94
#define NVM_CFG_ID_BANDWIDTH_WEIGHT			95
#define NVM_CFG_ID_MAX_BANDWIDTH			96
#define NVM_CFG_ID_PAUSE_ON_HOST_RING			97
#define NVM_CFG_ID_PCIE_PREEMPHASIS			98
#define NVM_CFG_ID_LLDP_MAC_ADDRESS			99
#define NVM_CFG_ID_FCOE_WWN_NODE_PREFIX			100
#define NVM_CFG_ID_FCOE_WWN_PORT_PREFIX			101
#define NVM_CFG_ID_LED_SPEED_SELECT			102
#define NVM_CFG_ID_LED_PORT_SWAP			103
#define NVM_CFG_ID_AVS_MODE_BB				104
#define NVM_CFG_ID_OVERRIDE_SECURE_MODE			105
#define NVM_CFG_ID_AVS_DAC_CODE_BB			106
#define NVM_CFG_ID_MBI_VERSION				107
#define NVM_CFG_ID_MBI_DATE				108
#define NVM_CFG_ID_SMBUS_ADDRESS			109
#define NVM_CFG_ID_NCSI_PACKAGE_ID			110
#define NVM_CFG_ID_SIDEBAND_MODE			111
#define NVM_CFG_ID_SMBUS_MODE				112
#define NVM_CFG_ID_NCSI					113
#define NVM_CFG_ID_TRANSCEIVER_MODULE_ABSENT		114
#define NVM_CFG_ID_I2C_MUX_SELECT_GPIO_BB		115
#define NVM_CFG_ID_I2C_MUX_SELECT_VALUE_BB		116
#define NVM_CFG_ID_DEVICE_CAPABILITIES			117
#define NVM_CFG_ID_ETH_DID_SUFFIX			118
#define NVM_CFG_ID_FCOE_DID_SUFFIX			119
#define NVM_CFG_ID_ISCSI_DID_SUFFIX			120
#define NVM_CFG_ID_DEFAULT_ENABLED_PROTOCOLS		122
#define NVM_CFG_ID_POWER_DISSIPATED_BB			123
#define NVM_CFG_ID_POWER_CONSUMED_BB			124
#define NVM_CFG_ID_AUX_MODE				125
#define NVM_CFG_ID_PORT_TYPE				126
#define NVM_CFG_ID_TX_DISABLE				127
#define NVM_CFG_ID_MAX_LINK_WIDTH			128
#define NVM_CFG_ID_ASPM_L1_MODE				130
#define NVM_CFG_ID_ON_CHIP_SENSOR_MODE			131
#define NVM_CFG_ID_PREBOOT_VLAN_VALUE			132
#define NVM_CFG_ID_PREBOOT_VLAN				133
#define NVM_CFG_ID_TEMPERATURE_PERIOD_BETWEEN_CHECKS	134
#define NVM_CFG_ID_SHUTDOWN_THRESHOLD_TEMPERATURE	135
#define NVM_CFG_ID_MAX_COUNT_OPER_THRESHOLD		136
#define NVM_CFG_ID_DEAD_TEMP_TH_TEMPERATURE		137
#define NVM_CFG_ID_TEMPERATURE_MONITORING_MODE		139
#define NVM_CFG_ID_AN_25G_50G_OUI			140
#define NVM_CFG_ID_PLDM_SENSOR_MODE			141
#define NVM_CFG_ID_EXTERNAL_THERMAL_SENSOR		142
#define NVM_CFG_ID_EXTERNAL_THERMAL_SENSOR_ADDRESS	143
#define NVM_CFG_ID_FAN_FAILURE_DURATION			144
#define NVM_CFG_ID_FEC_FORCE_MODE			145
#define NVM_CFG_ID_MULTI_NETWORK_MODES_CAPABILITY	146
#define NVM_CFG_ID_MNM_10G_DRV_SPEED_CAPABILITY_MASK	147
#define NVM_CFG_ID_MNM_10G_MFW_SPEED_CAPABILITY_MASK	148
#define NVM_CFG_ID_MNM_10G_DRV_LINK_SPEED		149
#define NVM_CFG_ID_MNM_10G_MFW_LINK_SPEED		150
#define NVM_CFG_ID_MNM_10G_PORT_TYPE			151
#define NVM_CFG_ID_MNM_10G_SERDES_NET_INTERFACE		152
#define NVM_CFG_ID_MNM_10G_FEC_FORCE_MODE		153
#define NVM_CFG_ID_MNM_10G_ETH_DID_SUFFIX		154
#define NVM_CFG_ID_MNM_25G_DRV_SPEED_CAPABILITY_MASK	155
#define NVM_CFG_ID_MNM_25G_MFW_SPEED_CAPABILITY_MASK	156
#define NVM_CFG_ID_MNM_25G_DRV_LINK_SPEED		157
#define NVM_CFG_ID_MNM_25G_MFW_LINK_SPEED		158
#define NVM_CFG_ID_MNM_25G_PORT_TYPE			159
#define NVM_CFG_ID_MNM_25G_SERDES_NET_INTERFACE		160
#define NVM_CFG_ID_MNM_25G_ETH_DID_SUFFIX		161
#define NVM_CFG_ID_MNM_25G_FEC_FORCE_MODE		162
#define NVM_CFG_ID_MNM_40G_DRV_SPEED_CAPABILITY_MASK	163
#define NVM_CFG_ID_MNM_40G_MFW_SPEED_CAPABILITY_MASK	164
#define NVM_CFG_ID_MNM_40G_DRV_LINK_SPEED		165
#define NVM_CFG_ID_MNM_40G_MFW_LINK_SPEED		166
#define NVM_CFG_ID_MNM_40G_PORT_TYPE			167
#define NVM_CFG_ID_MNM_40G_SERDES_NET_INTERFACE		168
#define NVM_CFG_ID_MNM_40G_ETH_DID_SUFFIX		169
#define NVM_CFG_ID_MNM_40G_FEC_FORCE_MODE		170
#define NVM_CFG_ID_MNM_50G_DRV_SPEED_CAPABILITY_MASK	171
#define NVM_CFG_ID_MNM_50G_MFW_SPEED_CAPABILITY_MASK	172
#define NVM_CFG_ID_MNM_50G_DRV_LINK_SPEED		173
#define NVM_CFG_ID_MNM_50G_MFW_LINK_SPEED		174
#define NVM_CFG_ID_MNM_50G_PORT_TYPE			175
#define NVM_CFG_ID_MNM_50G_SERDES_NET_INTERFACE		176
#define NVM_CFG_ID_MNM_50G_ETH_DID_SUFFIX		177
#define NVM_CFG_ID_MNM_50G_FEC_FORCE_MODE		178
#define NVM_CFG_ID_MNM_100G_DRV_SPEED_CAP_MASK_BB	179
#define NVM_CFG_ID_MNM_100G_MFW_SPEED_CAP_MASK_BB	180
#define NVM_CFG_ID_MNM_100G_DRV_LINK_SPEED_BB		181
#define NVM_CFG_ID_MNM_100G_MFW_LINK_SPEED_BB		182
#define NVM_CFG_ID_MNM_100G_PORT_TYPE_BB		183
#define NVM_CFG_ID_MNM_100G_SERDES_NET_INTERFACE_BB	184
#define NVM_CFG_ID_MNM_100G_ETH_DID_SUFFIX_BB		185
#define NVM_CFG_ID_MNM_100G_FEC_FORCE_MODE_BB		186
#define NVM_CFG_ID_FUNCTION_HIDE			187
#define NVM_CFG_ID_BAR2_TOTAL_BUDGET_BB			188
#define NVM_CFG_ID_CRASH_DUMP_TRIGGER_ENABLE		189
#define NVM_CFG_ID_MPS25_LANE_SWAP_K2_E5		190
#define NVM_CFG_ID_BAR2_SIZE_K2_E5			191
#define NVM_CFG_ID_EXT_PHY_RESET			192
#define NVM_CFG_ID_EEE_POWER_SAVING_MODE		193
#define NVM_CFG_ID_OVERRIDE_PCIE_PRESET_EQUAL_BB	194
#define NVM_CFG_ID_PCIE_PRESET_VALUE_BB			195
#define NVM_CFG_ID_MAX_MSIX				196
#define NVM_CFG_ID_NVM_CFG_VERSION			197
#define NVM_CFG_ID_NVM_CFG_NEW_OPTION_SEQ		198
#define NVM_CFG_ID_NVM_CFG_REMOVED_OPTION_SEQ		199
#define NVM_CFG_ID_NVM_CFG_UPDATED_VALUE_SEQ		200
#define NVM_CFG_ID_EXTENDED_SERIAL_NUMBER		201
#define NVM_CFG_ID_RDMA_ENABLEMENT			202
#define NVM_CFG_ID_RUNTIME_PORT_SWAP_GPIO		204
#define NVM_CFG_ID_RUNTIME_PORT_SWAP_MAP		205
#define NVM_CFG_ID_THERMAL_EVENT_GPIO			206
#define NVM_CFG_ID_I2C_INTERRUPT_GPIO			207
#define NVM_CFG_ID_DCI_SUPPORT				208
#define NVM_CFG_ID_PCIE_VDM_ENABLED			209
#define NVM_CFG_ID_OPTION_KIT_PN			210
#define NVM_CFG_ID_SPARE_PN				211
#define NVM_CFG_ID_FEC_AN_MODE_K2_E5			212
#define NVM_CFG_ID_NPAR_ENABLED_PROTOCOL		213
#define NVM_CFG_ID_MPS25_ACTIVE_TXFIR_PRE_BB_K2		214
#define NVM_CFG_ID_MPS25_ACTIVE_TXFIR_MAIN_BB_K2	215
#define NVM_CFG_ID_MPS25_ACTIVE_TXFIR_POST_BB_K2	216
#define NVM_CFG_ID_ALOM_FAN_ON_AUX_GPIO			217
#define NVM_CFG_ID_ALOM_FAN_ON_AUX_VALUE		218
#define NVM_CFG_ID_SLOT_ID_GPIO				219
#define NVM_CFG_ID_PMBUS_SCL_GPIO			220
#define NVM_CFG_ID_PMBUS_SDA_GPIO			221
#define NVM_CFG_ID_RESET_ON_LAN				222
#define NVM_CFG_ID_NCSI_PACKAGE_ID_IO			223
#define NVM_CFG_ID_TX_RX_EQ_25G_HLPC			224
#define NVM_CFG_ID_TX_RX_EQ_25G_LLPC			225
#define NVM_CFG_ID_TX_RX_EQ_25G_AC			226
#define NVM_CFG_ID_TX_RX_EQ_10G_PC			227
#define NVM_CFG_ID_TX_RX_EQ_10G_AC			228
#define NVM_CFG_ID_TX_RX_EQ_1G				229
#define NVM_CFG_ID_TX_RX_EQ_25G_BT			230
#define NVM_CFG_ID_TX_RX_EQ_10G_BT			231
#define NVM_CFG_ID_PF_MAPPING				232
#define NVM_CFG_ID_RECOVERY_MODE			234
#define NVM_CFG_ID_PHY_MODULE_DEAD_TEMP_TH		235
#define NVM_CFG_ID_PHY_MODULE_ALOM_FAN_ON_TEMP_TH	236
#define NVM_CFG_ID_PREBOOT_DEBUG_MODE_STD		237
#define NVM_CFG_ID_PREBOOT_DEBUG_MODE_EXT		238
#define NVM_CFG_ID_SMARTLINQ_MODE			239
#define NVM_CFG_ID_PREBOOT_LINK_UP_DELAY		242
#define NVM_CFG_ID_VOLTAGE_REGULATOR_TYPE		243
#define NVM_CFG_ID_MAIN_CLOCK_FREQUENCY			245
#define NVM_CFG_ID_MAC_CLOCK_FREQUENCY			246
#define NVM_CFG_ID_STORM_CLOCK_FREQUENCY		247
#define NVM_CFG_ID_PCIE_RELAXED_ORDERING		248
#define NVM_CFG_ID_EXT_PHY_MDI_PAIR_SWAP		249
#define NVM_CFG_ID_UID_LED_MODE_MASK			250
#define NVM_CFG_ID_NCSI_AUX_LINK			251
#define NVM_CFG_ID_SMARTAN_FEC_OVERRIDE			272
#define NVM_CFG_ID_LLDP_DISABLE				273
#define NVM_CFG_ID_SHORT_PERST_PROTECTION_K2		274
#define NVM_CFG_ID_TRANSCEIVER_RATE_SELECT_0		275
#define NVM_CFG_ID_TRANSCEIVER_RATE_SELECT_1		276
#define NVM_CFG_ID_TRANSCEIVER_MODULE_TX_FAULT		277
#define NVM_CFG_ID_TRANSCEIVER_QSFP_MODULE_RESET	278
#define NVM_CFG_ID_TRANSCEIVER_QSFP_LP_MODE		279
#define NVM_CFG_ID_TRANSCEIVER_POWER_ENABLE		280
#define NVM_CFG_ID_LASI_INTERRUPT_INPUT			281
#define NVM_CFG_ID_EXT_PHY_PGOOD_INPUT			282
#define NVM_CFG_ID_TRACE_LEVEL				283
#define NVM_CFG_ID_TRACE_MODULES			284
#define NVM_CFG_ID_EMULATED_TMP421			285
#define NVM_CFG_ID_WARNING_TEMPERATURE_GPIO		286
#define NVM_CFG_ID_WARNING_TEMPERATURE_THRESHOLD	287
#define NVM_CFG_ID_PERST_INDICATION_GPIO		288
#define NVM_CFG_ID_PCIE_CLASS_CODE_FCOE_K2_E5		289
#define NVM_CFG_ID_PCIE_CLASS_CODE_ISCSI_K2_E5		290
#define NVM_CFG_ID_NUMBER_OF_PROVISIONED_MAC		291
#define NVM_CFG_ID_NUMBER_OF_PROVISIONED_VF_MAC		292
#define NVM_CFG_ID_PROVISIONED_BMC_MAC			293
#define NVM_CFG_ID_OVERRIDE_AGC_THRESHOLD_K2		294
#define NVM_CFG_ID_WARNING_TEMPERATURE_DELTA		295
#define NVM_CFG_ID_ALOM_FAN_ON_AUX_DELTA		296
#define NVM_CFG_ID_DEAD_TEMP_TH_DELTA			297
#define NVM_CFG_ID_PHY_MODULE_WARNING_TEMP_TH		298
#define NVM_CFG_ID_DISABLE_PLDM				299
#define NVM_CFG_ID_DISABLE_MCTP_OEM			300
#define NVM_CFG_ID_LOWEST_MBI_VERSION			301
#define NVM_CFG_ID_ACTIVITY_LED_BLINK_RATE_HZ		302
#define NVM_CFG_ID_EXT_PHY_AVS_ENABLE			303
#define NVM_CFG_ID_OVERRIDE_TX_DRIVER_REGULATOR_K2	304
#define NVM_CFG_ID_TX_REGULATOR_VOLTAGE_GEN1_2_K2	305
#define NVM_CFG_ID_TX_REGULATOR_VOLTAGE_GEN3_K2		306
#define NVM_CFG_ID_SHUTDOWN___PHY_MONITOR_ENABLED	307
#define NVM_CFG_ID_SHUTDOWN___PHY_THRESHOLD_TEMP	308
#define NVM_CFG_ID_SHUTDOWN___PHY_SAMPLES		309
#define NVM_CFG_ID_NCSI_GET_CAPAS_CH_COUNT		310
#define NVM_CFG_ID_PERMIT_TOTAL_PORT_SHUTDOWN		311
#define NVM_CFG_ID_MCTP_VIRTUAL_LINK_OEM3		312
#define NVM_CFG_ID_QINQ_SUPPORT				314
#define NVM_CFG_ID_QINQ_OUTER_VLAN			315

/****************************************************************************
* Name:        spad_layout.h
*
* Description: Global definitions
*
* Created:     01/09/2013
*
****************************************************************************/
/*
 *          Spad Layout                                NVM CFG                         MCP public
 **==========================================================================================================
 *     MCP_REG_SCRATCH                         REG_RD(MISC_REG_GEN_PURP_CR0)       REG_RD(MISC_REG_SHARED_MEM_ADDR)
 *    +------------------+                      +-------------------------+        +-------------------+
 *    |  Num Sections(4B)|Currently 4           |   Num Sections(4B)      |        |   Num Sections(4B)|Currently 6
 *    +------------------+                      +-------------------------+        +-------------------+
 *    | Offsize(Trace)   |4B -+             +-- | Offset(NVM_CFG1)        |        | Offsize(drv_mb)   |
 *  +-| Offsize(NVM_CFG) |4B  |             |   | (Size is fixed)         |        | Offsize(mfw_mb)   |
 **+-|-| Offsize(Public)  |4B  |             +-> +-------------------------+        | Offsize(global)   |
 *| | | Offsize(Private) |4B  |                 |                         |        | Offsize(path)     |
 *| | +------------------+ <--+                 | nvm_cfg1_glob           |        | Offsize(port)     |
 *| | |                  |                      +-------------------------+        | Offsize(func)     |
 *| | |      Trace       |                      | nvm_cfg1_path 0         |        +-------------------+
 *| +>+------------------+                      | nvm_cfg1_path 1         |        | drv_mb   PF0/2/4..|8 Funcs of engine0
 *|   |                  |                      +-------------------------+        | drv_mb   PF1/3/5..|8 Funcs of engine1
 *|   |     NVM_CFG      |                      | nvm_cfg1_port 0         |        +-------------------+
 *|*+-> +------------------+                      |            ....         |        | mfw_mb   PF0/2/4..|8 Funcs of engine0
 *    |                  |                      | nvm_cfg1_port 3         |        | mfw_mb   PF1/3/5..|8 Funcs of engine1
 *    |   Public Data    |                      +-------------------------+        +-------------------+
 *    +------------------+   8 Funcs of Engine 0| nvm_cfg1_func PF0/2/4/..|        |                   |
 *    |                  |   8 Funcs of Engine 1| nvm_cfg1_func PF1/3/5/..|        | public_global     |
 *    |   Private Data   |                      +-------------------------+        +-------------------+
 *    +------------------+                                                         | public_path 0     |
 *    |       Code       |                                                         | public_path 1     |
 *    |   Static Area    |                                                         +-------------------+
 *    +---            ---+                                                         | public_port 0     |
 *    |       Code       |                                                         |        ....       |
 *    |      PIM Area    |                                                         | public_port 3     |
 *    +------------------+                                                         +-------------------+
 *                                                                                 | public_func 0/2/4.|8 Funcs of engine0
 *                                                                                 | public_func 1/3/5.|8 Funcs of engine1
 *                                                                                 +-------------------+
 */
#ifndef SPAD_LAYOUT_H
#define SPAD_LAYOUT_H

#ifndef MDUMP_PARSE_TOOL

#define PORT_0		0
#define PORT_1		1
#define PORT_2		2
#define PORT_3		3

extern struct spad_layout g_spad;
struct spad_layout {
	struct nvm_cfg nvm_cfg;
	struct mcp_public_data public_data;
};
#endif /* MDUMP_PARSE_TOOL */

/* TBD - Consider renaming to MCP_STATIC_SPAD_SIZE, since the real size includes another 64kb */
#define MCP_SPAD_SIZE    0x00028000	/* 160 KB */

#define SPAD_OFFSET(addr)    (((u32)addr - (u32)CPU_SPAD_BASE))

#define TO_OFFSIZE(_offset, _size)                               \
        (u32)((((u32)(_offset) >> 2) << OFFSIZE_OFFSET_OFFSET) | \
              (((u32)(_size) >> 2) << OFFSIZE_SIZE_OFFSET))

enum spad_sections {
	SPAD_SECTION_TRACE,
	SPAD_SECTION_NVM_CFG,
	SPAD_SECTION_PUBLIC,
	SPAD_SECTION_PRIVATE,
	SPAD_SECTION_MAX	/* Cannot be modified anymore since ROM relying on this size !! */
};

#define STRUCT_OFFSET(f)    (STATIC_INIT_BASE + \
                             __builtin_offsetof(struct static_init, f))

/* This section is located at a fixed location in the beginning of the scratchpad,
 * to ensure that the MCP trace is not run over during MFW upgrade.
 * All the rest of data has a floating location which differs from version to version,
 * and is pointed by the mcp_meta_data below.
 * Moreover, the spad_layout section is part of the MFW firmware, and is loaded with it
 * from nvram in order to clear this portion.
 */
struct static_init {
	u32 num_sections;	/* 0xe20000 */
	offsize_t sections[SPAD_SECTION_MAX];	/* 0xe20004 */
#define SECTION(_sec_)    *((offsize_t *)(STRUCT_OFFSET(sections[_sec_])))

#ifdef SECURE_BOOT
	u32 tim_hash[8];	/* Used by E5 ROM. Do not relocate */
#define PRESERVED_TIM_HASH	((u8 *)(STRUCT_OFFSET(tim_hash)))
	u32 tpu_hash[8];	/* Used by E5 ROM. Do not relocate */
#define PRESERVED_TPU_HASH	((u8 *)(STRUCT_OFFSET(tpu_hash)))
	u32 secure_pcie_fw_ver;	/* Used by E5 ROM. Do not relocate */
#define SECURE_PCIE_FW_VER	*((u32 *)(STRUCT_OFFSET(secure_pcie_fw_ver)))
	u32 secure_running_mfw;	/* Instead of the one after the trace_buffer *//* Used by E5 ROM. Do not relocate */
#define SECURE_RUNNING_MFW	*((u32 *)(STRUCT_OFFSET(secure_running_mfw)))
#endif
	struct mcp_trace trace;	/* 0xe20014 */
};

#ifndef MDUMP_PARSE_TOOL
#define NVM_CFG1(x)			g_spad.nvm_cfg.cfg1.x
#define NVM_GLOB_VAL(n, m, o)		((NVM_GLOB(n) & m) >> o)
#endif /* MDUMP_PARSE_TOOL */

#endif /* SPAD_LAYOUT_H */

/****************************************************************************
* Name:        nvm_map.h
*
* Description: Everest NVRAM map
*
****************************************************************************/

#ifndef NVM_MAP_H
#define NVM_MAP_H

#define CRC_MAGIC_VALUE		0xDEBB20E3
#define CRC32_POLYNOMIAL	0xEDB88320
#define _KB(x)		(x * 1024)
#define _MB(x)		(_KB(x) * 1024)
#define NVM_CRC_SIZE		(sizeof(u32))
enum nvm_sw_arbitrator {
	NVM_SW_ARB_HOST,
	NVM_SW_ARB_MCP,
	NVM_SW_ARB_UART,
	NVM_SW_ARB_RESERVED
};

/****************************************************************************
* Boot Strap Region                                                        *
****************************************************************************/
struct legacy_bootstrap_region {
	u32 magic_value;	/* a pattern not likely to occur randomly */
#define NVM_MAGIC_VALUE    0x669955aa
	u32 sram_start_addr;	/* where to locate LIM code (byte addr) */
	u32 code_len;		/* boot code length (in dwords) */
	u32 code_start_addr;	/* location of code on media (media byte addr) */
	u32 crc;		/* 32-bit CRC */
};

/****************************************************************************
* Directories Region                                                       *
****************************************************************************/
struct nvm_code_entry {
	u32 image_type;		/* Image type */
	u32 nvm_start_addr;	/* NVM address of the image */
	u32 len;		/* Include CRC */
	u32 sram_start_addr;	/* Where to load the image on the scratchpad */
	u32 sram_run_addr;	/* Relevant in case of MIM only */
};

enum nvm_image_type {
	NVM_TYPE_TIM1 = 0x01,
	NVM_TYPE_TIM2 = 0x02,
	NVM_TYPE_MIM1 = 0x03,
	NVM_TYPE_MIM2 = 0x04,
	NVM_TYPE_MBA = 0x05,
	NVM_TYPE_MODULES_PN = 0x06,
	NVM_TYPE_VPD = 0x07,
	NVM_TYPE_MFW_TRACE1 = 0x08,
	NVM_TYPE_MFW_TRACE2 = 0x09,
	NVM_TYPE_NVM_CFG1 = 0x0a,
	NVM_TYPE_L2B = 0x0b,
	NVM_TYPE_DIR1 = 0x0c,
	NVM_TYPE_EAGLE_FW1 = 0x0d,
	NVM_TYPE_FALCON_FW1 = 0x0e,
	NVM_TYPE_PCIE_FW1 = 0x0f,
	NVM_TYPE_HW_SET = 0x10,
	NVM_TYPE_LIM = 0x11,
	NVM_TYPE_AVS_FW1 = 0x12,
	NVM_TYPE_DIR2 = 0x13,
	NVM_TYPE_CCM = 0x14,
	NVM_TYPE_EAGLE_FW2 = 0x15,
	NVM_TYPE_FALCON_FW2 = 0x16,
	NVM_TYPE_PCIE_FW2 = 0x17,
	NVM_TYPE_AVS_FW2 = 0x18,
	NVM_TYPE_INIT_HW = 0x19,
	NVM_TYPE_DEFAULT_CFG = 0x1a,
	NVM_TYPE_MDUMP = 0x1b,
	NVM_TYPE_NVM_META = 0x1c,
	NVM_TYPE_ISCSI_CFG = 0x1d,
	NVM_TYPE_FCOE_CFG = 0x1f,
	NVM_TYPE_ETH_PHY_FW1 = 0x20,
	NVM_TYPE_ETH_PHY_FW2 = 0x21,
	NVM_TYPE_BDN = 0x22,
	NVM_TYPE_8485X_PHY_FW = 0x23,
	NVM_TYPE_PUB_KEY = 0x24,
	NVM_TYPE_RECOVERY = 0x25,
	NVM_TYPE_PLDM = 0x26,
	NVM_TYPE_UPK1 = 0x27,
	NVM_TYPE_UPK2 = 0x28,
	NVM_TYPE_MASTER_KC = 0x29,
	NVM_TYPE_BACKUP_KC = 0x2a,
	NVM_TYPE_HW_DUMP = 0x2b,
	NVM_TYPE_HW_DUMP_OUT = 0x2c,
	NVM_TYPE_BIN_NVM_META = 0x30,	/* Depreciated, never used in production */
	NVM_TYPE_ROM_TEST = 0xf0,
	NVM_TYPE_88X33X0_PHY_FW = 0x31,
	NVM_TYPE_88X33X0_PHY_SLAVE_FW = 0x32,
	NVM_TYPE_IDLE_CHK = 0x33,
	NVM_TYPE_MAX,
};

#ifdef DEFINE_IMAGE_TABLE
struct image_map {
	char name[32];
	char option[32];
	u32 image_type;
};

struct image_map g_image_table[] = {
	{"TIM1", "-tim1", NVM_TYPE_TIM1},
	{"TIM2", "-tim2", NVM_TYPE_TIM2},
	{"MIM1", "-mim1", NVM_TYPE_MIM1},
	{"MIM2", "-mim2", NVM_TYPE_MIM2},
	{"MBA", "-mba", NVM_TYPE_MBA},
	{"OPT_MODULES", "-optm", NVM_TYPE_MODULES_PN},
	{"VPD", "-vpd", NVM_TYPE_VPD},
	{"MFW_TRACE1", "-mfwt1", NVM_TYPE_MFW_TRACE1},
	{"MFW_TRACE2", "-mfwt2", NVM_TYPE_MFW_TRACE2},
	{"NVM_CFG1", "-cfg", NVM_TYPE_NVM_CFG1},
	{"L2B", "-l2b", NVM_TYPE_L2B},
	{"DIR1", "-dir1", NVM_TYPE_DIR1},
	{"EAGLE_FW1", "-eagle1", NVM_TYPE_EAGLE_FW1},
	{"FALCON_FW1", "-falcon1", NVM_TYPE_FALCON_FW1},
	{"PCIE_FW1", "-pcie1", NVM_TYPE_PCIE_FW1},
	{"HW_SET", "-hw_set", NVM_TYPE_HW_SET},
	{"LIM", "-lim", NVM_TYPE_LIM},
	{"AVS_FW1", "-avs1", NVM_TYPE_AVS_FW1},
	{"DIR2", "-dir2", NVM_TYPE_DIR2},
	{"CCM", "-ccm", NVM_TYPE_CCM},
	{"EAGLE_FW2", "-eagle2", NVM_TYPE_EAGLE_FW2},
	{"FALCON_FW2", "-falcon2", NVM_TYPE_FALCON_FW2},
	{"PCIE_FW2", "-pcie2", NVM_TYPE_PCIE_FW2},
	{"AVS_FW2", "-avs2", NVM_TYPE_AVS_FW2},
	{"INIT_HW", "-init_hw", NVM_TYPE_INIT_HW},
	{"DEFAULT_CFG", "-def_cfg", NVM_TYPE_DEFAULT_CFG},
	{"CRASH_DUMP", "-mdump", NVM_TYPE_MDUMP},
	{"META", "-meta", NVM_TYPE_NVM_META},
	{"ISCSI_CFG", "-iscsi_cfg", NVM_TYPE_ISCSI_CFG},
	{"FCOE_CFG", "-fcoe_cfg", NVM_TYPE_FCOE_CFG},
	{"ETH_PHY_FW1", "-ethphy1", NVM_TYPE_ETH_PHY_FW1},
	{"ETH_PHY_FW2", "-ethphy2", NVM_TYPE_ETH_PHY_FW2},
	{"BDN", "-bdn", NVM_TYPE_BDN},
	{"PK", "-pk", NVM_TYPE_PUB_KEY},
	{"RECOVERY", "-recovery", NVM_TYPE_RECOVERY},
	{"PLDM", "-pldm", NVM_TYPE_PLDM},
	{"UPK1", "-upk1", NVM_TYPE_UPK1},
	{"UPK2", "-upk2", NVM_TYPE_UPK2},
	{"ROMTEST", "-romtest", NVM_TYPE_ROM_TEST},
	{"MASTER_KC", "-kc", NVM_TYPE_MASTER_KC},
	{"HW_DUMP", "-hw_dump", NVM_TYPE_HW_DUMP},
	{"HW_DUMP_OUT", "-hw_dump_out", NVM_TYPE_HW_DUMP_OUT},
	{"BACKUP_KC", "", NVM_TYPE_BACKUP_KC},
	{"PHY_88X33X0", "-phy_88x33x0", NVM_TYPE_88X33X0_PHY_FW},
	{"PHY_SLAVE_88X33X0", "-slave_phy_88x33x0",
	 NVM_TYPE_88X33X0_PHY_SLAVE_FW},
	{"IDLE_CHK", "-idle_chk", NVM_TYPE_IDLE_CHK}
};

#define IMAGE_TABLE_SIZE	(sizeof(g_image_table) / \
                                 sizeof(struct image_map))

#endif /* #ifdef DEFINE_IMAGE_TABLE */
#define MAX_NVM_DIR_ENTRIES	100
/* Note: The has given 150 possible entries since anyway each file captures at least one page. */

struct nvm_dir_meta {
	u32 dir_id;
	u32 nvm_dir_addr;
	u32 num_images;
	u32 next_mfw_to_run;
};

struct nvm_dir {
	s32 seq;		/* This dword is used to indicate whether this dir is valid, and whether it is more updated than the other dir */
#define NVM_DIR_NEXT_MFW_MASK		0x00000001
#define NVM_DIR_SEQ_MASK		0xfffffffe
#define NVM_DIR_NEXT_MFW(seq)		((seq) & NVM_DIR_NEXT_MFW_MASK)
#define NVM_DIR_UPDATE_SEQ(_seq, swap_mfw)                                          \
	do {                                                                        \
		_seq =                                                              \
		        (((_seq +                                                   \
		           2) &                                                     \
		          NVM_DIR_SEQ_MASK) | (NVM_DIR_NEXT_MFW(_seq ^ swap_mfw))); \
	} while (0)
#define IS_DIR_SEQ_VALID(seq)		((seq & NVM_DIR_SEQ_MASK) != \
	                                 NVM_DIR_SEQ_MASK)

	u32 num_images;
	u32 rsrv;
	struct nvm_code_entry code[1];	/* Up to MAX_NVM_DIR_ENTRIES */
};
#define NVM_DIR_SIZE(_num_images)    (sizeof(struct nvm_dir) +              \
                                      (_num_images -                        \
                                       1) * sizeof(struct nvm_code_entry) + \
                                      NVM_CRC_SIZE)

struct nvm_vpd_image {
	u32 format_revision;
#define VPD_IMAGE_VERSION    1

	/* This array length depends on the number of VPD fields */
	u8 vpd_data[1];
};

/****************************************************************************
* NVRAM FULL MAP                                                           *
****************************************************************************/
#define DIR_ID_1			(0)
#define DIR_ID_2			(1)
#define MAX_DIR_IDS			(2)

#define MFW_BUNDLE_1			(0)
#define MFW_BUNDLE_2			(1)
#define MAX_MFW_BUNDLES			(2)

#define FLASH_PAGE_SIZE			0x1000
#define NVM_DIR_MAX_SIZE		(FLASH_PAGE_SIZE)	/* 4Kb */
#define LEGACY_ASIC_MIM_MAX_SIZE	(_KB(1200))	/* 1.2Mb - E4 */

/* Define applied to EMUL as well. */
#define FPGA_MIM_MAX_SIZE		(0x40000)	/* 192Kb */

/* Each image must start on its own page. Bootstrap and LIM are bound together, so they can share the same page.
 * The LIM itself should be very small, so limit it to 8Kb, but in order to open a new page, we decrement the bootstrap size out of it.
 */
#define LIM_MAX_SIZE			((2 *                                   \
                                          FLASH_PAGE_SIZE) -                    \
                                         sizeof(struct legacy_bootstrap_region) \
                                         - NVM_RSV_SIZE)
#define LIM_OFFSET			(NVM_OFFSET(lim_image))
#define NVM_RSV_SIZE			(44)
#define GET_MIM_MAX_SIZE(is_asic, \
                         is_e4)			(LEGACY_ASIC_MIM_MAX_SIZE)
#define GET_MIM_OFFSET(idx, is_asic,                                            \
                       is_e4)			(NVM_OFFSET(dir[MAX_MFW_BUNDLES \
                                                            ]) +                \
                                                 ((idx ==                       \
                                                   NVM_TYPE_MIM2) ?             \
                                                  GET_MIM_MAX_SIZE(is_asic,     \
                                                                   is_e4) : 0))
#define GET_NVM_FIXED_AREA_SIZE(is_asic,                                    \
                                is_e4)		(sizeof(struct nvm_image) + \
                                                 GET_MIM_MAX_SIZE(is_asic,  \
                                                                  is_e4) * 2)

#define E5_MASTER_KEY_CHAIN_ADDR	0x1000
#define E5_BACKUP_KEY_CHAIN_ADDR	((0x20000 <<                    \
                                          (REG_READ(0,                  \
                                                    MCP_REG_NVM_CFG4) & \
                                           0x7)) - 0x1000)
union nvm_dir_union {
	struct nvm_dir dir;
	u8 page[FLASH_PAGE_SIZE];
};

/*          E4            Address
 *  +-------------------+ 0x000000
 *  |    Bootstrap:     |
 *  | magic_number      |
 *  | sram_start_addr   |
 *  | code_len          |
 *  | code_start_addr   |
 *  | crc               |
 *  +-------------------+ 0x000014
 *  | rsrv              |
 *  +-------------------+ 0x000040
 *  | LIM               |
 *  +-------------------+ 0x002000
 *  | Dir1              |
 *  +-------------------+ 0x003000
 *  | Dir2              |
 *  +-------------------+ 0x004000
 *  | MIM1              |
 *  +-------------------+ 0x130000
 *  | MIM2              |
 *  +-------------------+ 0x25C000
 *  | Rest Images:      |
 *  | TIM1/2            |
 *  | MFW_TRACE1/2      |
 *  | Eagle/Falcon FW   |
 *  | PCIE/AVS FW       |
 *  | MBA/CCM/L2B       |
 *  | VPD               |
 *  | optic_modules     |
 *  |  ...              |
 *  +-------------------+ 0x400000
 */
struct nvm_image {
/*********** !!!  FIXED SECTIONS  !!! DO NOT MODIFY !!! **********************/
	/* NVM Offset  (size) */
	struct legacy_bootstrap_region bootstrap;	/* 0x000000 (0x000014) */
	u8 rsrv[NVM_RSV_SIZE];	/* 0x000014 (0x00002c) */
	u8 lim_image[LIM_MAX_SIZE];	/* 0x000040 (0x001fc0) */
	union nvm_dir_union dir[MAX_MFW_BUNDLES];	/* 0x002000 (0x001000)x2 */
	/* MIM1_IMAGE                              0x004000 (0x12c000) */
	/* MIM2_IMAGE                              0x130000 (0x12c000) */
/*********** !!!  FIXED SECTIONS  !!! DO NOT MODIFY !!! **********************/
};				/* 0x134 */

#define NVM_OFFSET(f)    ((u32_t)((int_ptr_t)(&(((struct nvm_image *)0)->f))))

struct hw_set_info {
	u32 reg_type;
#define GRC_REG_TYPE	1
#define PHY_REG_TYPE	2
#define PCI_REG_TYPE	4

	u32 bank_num;
	u32 pf_num;
	u32 operation;
#define READ_OP		1
#define WRITE_OP	2
#define RMW_SET_OP	3
#define RMW_CLR_OP	4

	u32 reg_addr;
	u32 reg_data;

	u32 reset_type;
#define POR_RESET_TYPE		(1 << 0)
#define HARD_RESET_TYPE		(1 << 1)
#define CORE_RESET_TYPE		(1 << 2)
#define MCP_RESET_TYPE		(1 << 3)
#define PERSET_ASSERT		(1 << 4)
#define PERSET_DEASSERT		(1 << 5)
};

struct hw_set_image {
	u32 format_version;
#define HW_SET_IMAGE_VERSION    1
	u32 no_hw_sets;
	/* This array length depends on the no_hw_sets */
	struct hw_set_info hw_sets[1];
};

#define MAX_SUPPORTED_NVM_OPTIONS			1000	/* Arbitrary for sanity check */

#define NVM_META_BIN_OPTION_OFFSET_MASK			0x0000ffff
#define NVM_META_BIN_OPTION_OFFSET_OFFSET		0
#define NVM_META_BIN_OPTION_LEN_MASK			0x00ff0000
#define NVM_META_BIN_OPTION_LEN_OFFSET			16
#define NVM_META_BIN_OPTION_ENTITY_MASK			0x03000000
#define NVM_META_BIN_OPTION_ENTITY_OFFSET		24
#define NVM_META_BIN_OPTION_ENTITY_GLOB			0
#define NVM_META_BIN_OPTION_ENTITY_PORT			1
#define NVM_META_BIN_OPTION_ENTITY_FUNC			2
#define NVM_META_BIN_OPTION_CONFIG_TYPE_MASK		0x0c000000
#define NVM_META_BIN_OPTION_CONFIG_TYPE_OFFSET		26
#define NVM_META_BIN_OPTION_CONFIG_TYPE_USER		0
#define NVM_META_BIN_OPTION_CONFIG_TYPE_FIXED		1
#define NVM_META_BIN_OPTION_CONFIG_TYPE_FORCED		2

struct nvm_meta_bin_t {
	u32 magic;
#define NVM_META_BIN_MAGIC				0x669955bb
	u32 version;
#define NVM_META_BIN_VERSION				1
	u32 num_options;
	u32 options[0];
};
#endif //NVM_MAP_H

#endif
