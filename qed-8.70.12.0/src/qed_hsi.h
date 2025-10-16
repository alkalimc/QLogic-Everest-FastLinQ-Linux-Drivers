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

#ifndef _QED_HSI_H
#define _QED_HSI_H
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/crc32.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "common_hsi.h"
#include "storage_common.h"
#include "storage_overtcp_common.h"
#include "tcp_common.h"
#include "fcoe_common.h"
#include "eth_common.h"
#include "iscsi_common.h"
#include "iwarp_common.h"
#include "rdma_common.h"
#include "roce_common.h"
#define ARRAY_DECL    static const
struct qed_hwfn;
struct qed_ptt;
/********************************/
/* Add include to common target */
/********************************/

/* opcodes for the event ring */
enum common_event_opcode {
	COMMON_EVENT_PF_START,
	COMMON_EVENT_PF_STOP,
	COMMON_EVENT_VF_START,
	COMMON_EVENT_VF_STOP,
	COMMON_EVENT_VF_PF_CHANNEL,
	COMMON_EVENT_VF_FLR,
	COMMON_EVENT_PF_UPDATE,
	COMMON_EVENT_FW_ERROR,	/* HSI_COMMENT: FW detected an error (could be Malicious VF or PF FP Error) */
	COMMON_EVENT_RL_UPDATE,
	COMMON_EVENT_EMPTY,
	MAX_COMMON_EVENT_OPCODE
};

/* Common Ramrod Command IDs */
enum common_ramrod_cmd_id {
	COMMON_RAMROD_UNUSED,
	COMMON_RAMROD_PF_START,	/* HSI_COMMENT: PF Function Start Ramrod */
	COMMON_RAMROD_PF_STOP,	/* HSI_COMMENT: PF Function Stop Ramrod */
	COMMON_RAMROD_VF_START,	/* HSI_COMMENT: VF Function Start */
	COMMON_RAMROD_VF_STOP,	/* HSI_COMMENT: VF Function Stop Ramrod */
	COMMON_RAMROD_PF_UPDATE,	/* HSI_COMMENT: PF update Ramrod */
	COMMON_RAMROD_RL_UPDATE,	/* HSI_COMMENT: QCN/DCQCN RL update Ramrod */
	COMMON_RAMROD_EMPTY,	/* HSI_COMMENT: Empty Ramrod */
	MAX_COMMON_RAMROD_CMD_ID
};

/* The core storm context for the Ystorm */
struct ystorm_core_conn_st_ctx {
	__le32 reserved[4];
};

/* The core storm context for the Pstorm */
struct pstorm_core_conn_st_ctx {
	__le32 reserved[20];
};

/* Core Slowpath Connection storm context of Xstorm */
struct xstorm_core_conn_st_ctx {
	struct regpair spq_base_addr;	/* HSI_COMMENT: SPQ Ring Base Address */
	__le32 reserved0[2];	/* HSI_COMMENT: To align Offset for firmware RW Fields. */
	__le16 spq_cons;	/* HSI_COMMENT: SPQ Ring Consumer */
	__le16 reserved1[111];	/* HSI_COMMENT: Pad to 15 cycles */
};

struct xstorm_core_conn_ag_ctx {
	u8 reserved0;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define XSTORM_CORE_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define XSTORM_CORE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define XSTORM_CORE_CONN_AG_CTX_RESERVED1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define XSTORM_CORE_CONN_AG_CTX_RESERVED1_SHIFT			1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED2_MASK			0x1	/* HSI_COMMENT: exist_in_qm2 */
#define XSTORM_CORE_CONN_AG_CTX_RESERVED2_SHIFT			2
#define XSTORM_CORE_CONN_AG_CTX_EXIST_IN_QM3_MASK		0x1	/* HSI_COMMENT: exist_in_qm3 */
#define XSTORM_CORE_CONN_AG_CTX_EXIST_IN_QM3_SHIFT		3
#define XSTORM_CORE_CONN_AG_CTX_RESERVED3_MASK			0x1	/* HSI_COMMENT: bit4 */
#define XSTORM_CORE_CONN_AG_CTX_RESERVED3_SHIFT			4
#define XSTORM_CORE_CONN_AG_CTX_RESERVED4_MASK			0x1	/* HSI_COMMENT: cf_array_active */
#define XSTORM_CORE_CONN_AG_CTX_RESERVED4_SHIFT			5
#define XSTORM_CORE_CONN_AG_CTX_RESERVED5_MASK			0x1	/* HSI_COMMENT: bit6 */
#define XSTORM_CORE_CONN_AG_CTX_RESERVED5_SHIFT			6
#define XSTORM_CORE_CONN_AG_CTX_RESERVED6_MASK			0x1	/* HSI_COMMENT: bit7 */
#define XSTORM_CORE_CONN_AG_CTX_RESERVED6_SHIFT			7
	u8 flags1;
#define XSTORM_CORE_CONN_AG_CTX_RESERVED7_MASK			0x1	/* HSI_COMMENT: bit8 */
#define XSTORM_CORE_CONN_AG_CTX_RESERVED7_SHIFT			0
#define XSTORM_CORE_CONN_AG_CTX_RESERVED8_MASK			0x1	/* HSI_COMMENT: bit9 */
#define XSTORM_CORE_CONN_AG_CTX_RESERVED8_SHIFT			1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED9_MASK			0x1	/* HSI_COMMENT: bit10 */
#define XSTORM_CORE_CONN_AG_CTX_RESERVED9_SHIFT			2
#define XSTORM_CORE_CONN_AG_CTX_BIT11_MASK			0x1	/* HSI_COMMENT: bit11 */
#define XSTORM_CORE_CONN_AG_CTX_BIT11_SHIFT			3
#define XSTORM_CORE_CONN_AG_CTX_BIT12_MASK			0x1	/* HSI_COMMENT: bit12 */
#define XSTORM_CORE_CONN_AG_CTX_BIT12_SHIFT			4
#define XSTORM_CORE_CONN_AG_CTX_BIT13_MASK			0x1	/* HSI_COMMENT: bit13 */
#define XSTORM_CORE_CONN_AG_CTX_BIT13_SHIFT			5
#define XSTORM_CORE_CONN_AG_CTX_TX_RULE_ACTIVE_MASK		0x1	/* HSI_COMMENT: bit14 */
#define XSTORM_CORE_CONN_AG_CTX_TX_RULE_ACTIVE_SHIFT		6
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_ACTIVE_MASK		0x1	/* HSI_COMMENT: bit15 */
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_ACTIVE_SHIFT		7
	u8 flags2;
#define XSTORM_CORE_CONN_AG_CTX_CF0_MASK			0x3	/* HSI_COMMENT: timer0cf */
#define XSTORM_CORE_CONN_AG_CTX_CF0_SHIFT			0
#define XSTORM_CORE_CONN_AG_CTX_CF1_MASK			0x3	/* HSI_COMMENT: timer1cf */
#define XSTORM_CORE_CONN_AG_CTX_CF1_SHIFT			2
#define XSTORM_CORE_CONN_AG_CTX_CF2_MASK			0x3	/* HSI_COMMENT: timer2cf */
#define XSTORM_CORE_CONN_AG_CTX_CF2_SHIFT			4
#define XSTORM_CORE_CONN_AG_CTX_CF3_MASK			0x3	/* HSI_COMMENT: timer_stop_all */
#define XSTORM_CORE_CONN_AG_CTX_CF3_SHIFT			6
	u8 flags3;
#define XSTORM_CORE_CONN_AG_CTX_CF4_MASK			0x3	/* HSI_COMMENT: cf4 */
#define XSTORM_CORE_CONN_AG_CTX_CF4_SHIFT			0
#define XSTORM_CORE_CONN_AG_CTX_CF5_MASK			0x3	/* HSI_COMMENT: cf5 */
#define XSTORM_CORE_CONN_AG_CTX_CF5_SHIFT			2
#define XSTORM_CORE_CONN_AG_CTX_CF6_MASK			0x3	/* HSI_COMMENT: cf6 */
#define XSTORM_CORE_CONN_AG_CTX_CF6_SHIFT			4
#define XSTORM_CORE_CONN_AG_CTX_CF7_MASK			0x3	/* HSI_COMMENT: cf7 */
#define XSTORM_CORE_CONN_AG_CTX_CF7_SHIFT			6
	u8 flags4;
#define XSTORM_CORE_CONN_AG_CTX_CF8_MASK			0x3	/* HSI_COMMENT: cf8 */
#define XSTORM_CORE_CONN_AG_CTX_CF8_SHIFT			0
#define XSTORM_CORE_CONN_AG_CTX_CF9_MASK			0x3	/* HSI_COMMENT: cf9 */
#define XSTORM_CORE_CONN_AG_CTX_CF9_SHIFT			2
#define XSTORM_CORE_CONN_AG_CTX_CF10_MASK			0x3	/* HSI_COMMENT: cf10 */
#define XSTORM_CORE_CONN_AG_CTX_CF10_SHIFT			4
#define XSTORM_CORE_CONN_AG_CTX_CF11_MASK			0x3	/* HSI_COMMENT: cf11 */
#define XSTORM_CORE_CONN_AG_CTX_CF11_SHIFT			6
	u8 flags5;
#define XSTORM_CORE_CONN_AG_CTX_CF12_MASK			0x3	/* HSI_COMMENT: cf12 */
#define XSTORM_CORE_CONN_AG_CTX_CF12_SHIFT			0
#define XSTORM_CORE_CONN_AG_CTX_CF13_MASK			0x3	/* HSI_COMMENT: cf13 */
#define XSTORM_CORE_CONN_AG_CTX_CF13_SHIFT			2
#define XSTORM_CORE_CONN_AG_CTX_CF14_MASK			0x3	/* HSI_COMMENT: cf14 */
#define XSTORM_CORE_CONN_AG_CTX_CF14_SHIFT			4
#define XSTORM_CORE_CONN_AG_CTX_CF15_MASK			0x3	/* HSI_COMMENT: cf15 */
#define XSTORM_CORE_CONN_AG_CTX_CF15_SHIFT			6
	u8 flags6;
#define XSTORM_CORE_CONN_AG_CTX_CONSOLID_PROD_CF_MASK		0x3	/* HSI_COMMENT: cf16 */
#define XSTORM_CORE_CONN_AG_CTX_CONSOLID_PROD_CF_SHIFT		0
#define XSTORM_CORE_CONN_AG_CTX_CF17_MASK			0x3	/* HSI_COMMENT: cf_array_cf */
#define XSTORM_CORE_CONN_AG_CTX_CF17_SHIFT			2
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_MASK			0x3	/* HSI_COMMENT: cf18 */
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_SHIFT			4
#define XSTORM_CORE_CONN_AG_CTX_TERMINATE_CF_MASK		0x3	/* HSI_COMMENT: cf19 */
#define XSTORM_CORE_CONN_AG_CTX_TERMINATE_CF_SHIFT		6
	u8 flags7;
#define XSTORM_CORE_CONN_AG_CTX_FLUSH_Q0_MASK			0x3	/* HSI_COMMENT: cf20 */
#define XSTORM_CORE_CONN_AG_CTX_FLUSH_Q0_SHIFT			0
#define XSTORM_CORE_CONN_AG_CTX_RESERVED10_MASK			0x3	/* HSI_COMMENT: cf21 */
#define XSTORM_CORE_CONN_AG_CTX_RESERVED10_SHIFT		2
#define XSTORM_CORE_CONN_AG_CTX_SLOW_PATH_MASK			0x3	/* HSI_COMMENT: cf22 */
#define XSTORM_CORE_CONN_AG_CTX_SLOW_PATH_SHIFT			4
#define XSTORM_CORE_CONN_AG_CTX_CF0EN_MASK			0x1	/* HSI_COMMENT: cf0en */
#define XSTORM_CORE_CONN_AG_CTX_CF0EN_SHIFT			6
#define XSTORM_CORE_CONN_AG_CTX_CF1EN_MASK			0x1	/* HSI_COMMENT: cf1en */
#define XSTORM_CORE_CONN_AG_CTX_CF1EN_SHIFT			7
	u8 flags8;
#define XSTORM_CORE_CONN_AG_CTX_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define XSTORM_CORE_CONN_AG_CTX_CF2EN_SHIFT			0
#define XSTORM_CORE_CONN_AG_CTX_CF3EN_MASK			0x1	/* HSI_COMMENT: cf3en */
#define XSTORM_CORE_CONN_AG_CTX_CF3EN_SHIFT			1
#define XSTORM_CORE_CONN_AG_CTX_CF4EN_MASK			0x1	/* HSI_COMMENT: cf4en */
#define XSTORM_CORE_CONN_AG_CTX_CF4EN_SHIFT			2
#define XSTORM_CORE_CONN_AG_CTX_CF5EN_MASK			0x1	/* HSI_COMMENT: cf5en */
#define XSTORM_CORE_CONN_AG_CTX_CF5EN_SHIFT			3
#define XSTORM_CORE_CONN_AG_CTX_CF6EN_MASK			0x1	/* HSI_COMMENT: cf6en */
#define XSTORM_CORE_CONN_AG_CTX_CF6EN_SHIFT			4
#define XSTORM_CORE_CONN_AG_CTX_CF7EN_MASK			0x1	/* HSI_COMMENT: cf7en */
#define XSTORM_CORE_CONN_AG_CTX_CF7EN_SHIFT			5
#define XSTORM_CORE_CONN_AG_CTX_CF8EN_MASK			0x1	/* HSI_COMMENT: cf8en */
#define XSTORM_CORE_CONN_AG_CTX_CF8EN_SHIFT			6
#define XSTORM_CORE_CONN_AG_CTX_CF9EN_MASK			0x1	/* HSI_COMMENT: cf9en */
#define XSTORM_CORE_CONN_AG_CTX_CF9EN_SHIFT			7
	u8 flags9;
#define XSTORM_CORE_CONN_AG_CTX_CF10EN_MASK			0x1	/* HSI_COMMENT: cf10en */
#define XSTORM_CORE_CONN_AG_CTX_CF10EN_SHIFT			0
#define XSTORM_CORE_CONN_AG_CTX_CF11EN_MASK			0x1	/* HSI_COMMENT: cf11en */
#define XSTORM_CORE_CONN_AG_CTX_CF11EN_SHIFT			1
#define XSTORM_CORE_CONN_AG_CTX_CF12EN_MASK			0x1	/* HSI_COMMENT: cf12en */
#define XSTORM_CORE_CONN_AG_CTX_CF12EN_SHIFT			2
#define XSTORM_CORE_CONN_AG_CTX_CF13EN_MASK			0x1	/* HSI_COMMENT: cf13en */
#define XSTORM_CORE_CONN_AG_CTX_CF13EN_SHIFT			3
#define XSTORM_CORE_CONN_AG_CTX_CF14EN_MASK			0x1	/* HSI_COMMENT: cf14en */
#define XSTORM_CORE_CONN_AG_CTX_CF14EN_SHIFT			4
#define XSTORM_CORE_CONN_AG_CTX_CF15EN_MASK			0x1	/* HSI_COMMENT: cf15en */
#define XSTORM_CORE_CONN_AG_CTX_CF15EN_SHIFT			5
#define XSTORM_CORE_CONN_AG_CTX_CONSOLID_PROD_CF_EN_MASK	0x1	/* HSI_COMMENT: cf16en */
#define XSTORM_CORE_CONN_AG_CTX_CONSOLID_PROD_CF_EN_SHIFT	6
#define XSTORM_CORE_CONN_AG_CTX_CF17EN_MASK			0x1	/* HSI_COMMENT: cf_array_cf_en */
#define XSTORM_CORE_CONN_AG_CTX_CF17EN_SHIFT			7
	u8 flags10;
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_EN_MASK			0x1	/* HSI_COMMENT: cf18en */
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_EN_SHIFT			0
#define XSTORM_CORE_CONN_AG_CTX_TERMINATE_CF_EN_MASK		0x1	/* HSI_COMMENT: cf19en */
#define XSTORM_CORE_CONN_AG_CTX_TERMINATE_CF_EN_SHIFT		1
#define XSTORM_CORE_CONN_AG_CTX_FLUSH_Q0_EN_MASK		0x1	/* HSI_COMMENT: cf20en */
#define XSTORM_CORE_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT		2
#define XSTORM_CORE_CONN_AG_CTX_RESERVED11_MASK			0x1	/* HSI_COMMENT: cf21en */
#define XSTORM_CORE_CONN_AG_CTX_RESERVED11_SHIFT		3
#define XSTORM_CORE_CONN_AG_CTX_SLOW_PATH_EN_MASK		0x1	/* HSI_COMMENT: cf22en */
#define XSTORM_CORE_CONN_AG_CTX_SLOW_PATH_EN_SHIFT		4
#define XSTORM_CORE_CONN_AG_CTX_CF23EN_MASK			0x1	/* HSI_COMMENT: cf23en */
#define XSTORM_CORE_CONN_AG_CTX_CF23EN_SHIFT			5
#define XSTORM_CORE_CONN_AG_CTX_RESERVED12_MASK			0x1	/* HSI_COMMENT: rule0en */
#define XSTORM_CORE_CONN_AG_CTX_RESERVED12_SHIFT		6
#define XSTORM_CORE_CONN_AG_CTX_RESERVED13_MASK			0x1	/* HSI_COMMENT: rule1en */
#define XSTORM_CORE_CONN_AG_CTX_RESERVED13_SHIFT		7
	u8 flags11;
#define XSTORM_CORE_CONN_AG_CTX_RESERVED14_MASK			0x1	/* HSI_COMMENT: rule2en */
#define XSTORM_CORE_CONN_AG_CTX_RESERVED14_SHIFT		0
#define XSTORM_CORE_CONN_AG_CTX_RESERVED15_MASK			0x1	/* HSI_COMMENT: rule3en */
#define XSTORM_CORE_CONN_AG_CTX_RESERVED15_SHIFT		1
#define XSTORM_CORE_CONN_AG_CTX_TX_DEC_RULE_EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define XSTORM_CORE_CONN_AG_CTX_TX_DEC_RULE_EN_SHIFT		2
#define XSTORM_CORE_CONN_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define XSTORM_CORE_CONN_AG_CTX_RULE5EN_SHIFT			3
#define XSTORM_CORE_CONN_AG_CTX_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define XSTORM_CORE_CONN_AG_CTX_RULE6EN_SHIFT			4
#define XSTORM_CORE_CONN_AG_CTX_RULE7EN_MASK			0x1	/* HSI_COMMENT: rule7en */
#define XSTORM_CORE_CONN_AG_CTX_RULE7EN_SHIFT			5
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED1_MASK		0x1	/* HSI_COMMENT: rule8en */
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED1_SHIFT		6
#define XSTORM_CORE_CONN_AG_CTX_RULE9EN_MASK			0x1	/* HSI_COMMENT: rule9en */
#define XSTORM_CORE_CONN_AG_CTX_RULE9EN_SHIFT			7
	u8 flags12;
#define XSTORM_CORE_CONN_AG_CTX_RULE10EN_MASK			0x1	/* HSI_COMMENT: rule10en */
#define XSTORM_CORE_CONN_AG_CTX_RULE10EN_SHIFT			0
#define XSTORM_CORE_CONN_AG_CTX_RULE11EN_MASK			0x1	/* HSI_COMMENT: rule11en */
#define XSTORM_CORE_CONN_AG_CTX_RULE11EN_SHIFT			1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED2_MASK		0x1	/* HSI_COMMENT: rule12en */
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED2_SHIFT		2
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED3_MASK		0x1	/* HSI_COMMENT: rule13en */
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED3_SHIFT		3
#define XSTORM_CORE_CONN_AG_CTX_RULE14EN_MASK			0x1	/* HSI_COMMENT: rule14en */
#define XSTORM_CORE_CONN_AG_CTX_RULE14EN_SHIFT			4
#define XSTORM_CORE_CONN_AG_CTX_RULE15EN_MASK			0x1	/* HSI_COMMENT: rule15en */
#define XSTORM_CORE_CONN_AG_CTX_RULE15EN_SHIFT			5
#define XSTORM_CORE_CONN_AG_CTX_RULE16EN_MASK			0x1	/* HSI_COMMENT: rule16en */
#define XSTORM_CORE_CONN_AG_CTX_RULE16EN_SHIFT			6
#define XSTORM_CORE_CONN_AG_CTX_RULE17EN_MASK			0x1	/* HSI_COMMENT: rule17en */
#define XSTORM_CORE_CONN_AG_CTX_RULE17EN_SHIFT			7
	u8 flags13;
#define XSTORM_CORE_CONN_AG_CTX_RULE18EN_MASK			0x1	/* HSI_COMMENT: rule18en */
#define XSTORM_CORE_CONN_AG_CTX_RULE18EN_SHIFT			0
#define XSTORM_CORE_CONN_AG_CTX_RULE19EN_MASK			0x1	/* HSI_COMMENT: rule19en */
#define XSTORM_CORE_CONN_AG_CTX_RULE19EN_SHIFT			1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED4_MASK		0x1	/* HSI_COMMENT: rule20en */
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED4_SHIFT		2
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED5_MASK		0x1	/* HSI_COMMENT: rule21en */
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED5_SHIFT		3
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED6_MASK		0x1	/* HSI_COMMENT: rule22en */
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED6_SHIFT		4
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED7_MASK		0x1	/* HSI_COMMENT: rule23en */
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED7_SHIFT		5
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED8_MASK		0x1	/* HSI_COMMENT: rule24en */
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED8_SHIFT		6
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED9_MASK		0x1	/* HSI_COMMENT: rule25en */
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED9_SHIFT		7
	u8 flags14;
#define XSTORM_CORE_CONN_AG_CTX_BIT16_MASK			0x1	/* HSI_COMMENT: bit16 */
#define XSTORM_CORE_CONN_AG_CTX_BIT16_SHIFT			0
#define XSTORM_CORE_CONN_AG_CTX_BIT17_MASK			0x1	/* HSI_COMMENT: bit17 */
#define XSTORM_CORE_CONN_AG_CTX_BIT17_SHIFT			1
#define XSTORM_CORE_CONN_AG_CTX_BIT18_MASK			0x1	/* HSI_COMMENT: bit18 */
#define XSTORM_CORE_CONN_AG_CTX_BIT18_SHIFT			2
#define XSTORM_CORE_CONN_AG_CTX_BIT19_MASK			0x1	/* HSI_COMMENT: bit19 */
#define XSTORM_CORE_CONN_AG_CTX_BIT19_SHIFT			3
#define XSTORM_CORE_CONN_AG_CTX_BIT20_MASK			0x1	/* HSI_COMMENT: bit20 */
#define XSTORM_CORE_CONN_AG_CTX_BIT20_SHIFT			4
#define XSTORM_CORE_CONN_AG_CTX_BIT21_MASK			0x1	/* HSI_COMMENT: bit21 */
#define XSTORM_CORE_CONN_AG_CTX_BIT21_SHIFT			5
#define XSTORM_CORE_CONN_AG_CTX_CF23_MASK			0x3	/* HSI_COMMENT: cf23 */
#define XSTORM_CORE_CONN_AG_CTX_CF23_SHIFT			6
	u8 byte2;		/* HSI_COMMENT: byte2 */
	__le16 physical_q0;	/* HSI_COMMENT: physical_q0 */
	__le16 consolid_prod;	/* HSI_COMMENT: physical_q1 */
	__le16 reserved16;	/* HSI_COMMENT: physical_q2 */
	__le16 tx_bd_cons;	/* HSI_COMMENT: word3 */
	__le16 tx_bd_or_spq_prod;	/* HSI_COMMENT: word4 */
	__le16 updated_qm_pq_id;	/* HSI_COMMENT: word5 */
	__le16 conn_dpi;	/* HSI_COMMENT: conn_dpi */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	u8 byte5;		/* HSI_COMMENT: byte5 */
	u8 byte6;		/* HSI_COMMENT: byte6 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
	__le32 reg5;		/* HSI_COMMENT: cf_array0 */
	__le32 reg6;		/* HSI_COMMENT: cf_array1 */
	__le16 word7;		/* HSI_COMMENT: word7 */
	__le16 word8;		/* HSI_COMMENT: word8 */
	__le16 word9;		/* HSI_COMMENT: word9 */
	__le16 word10;		/* HSI_COMMENT: word10 */
	__le32 reg7;		/* HSI_COMMENT: reg7 */
	__le32 reg8;		/* HSI_COMMENT: reg8 */
	__le32 reg9;		/* HSI_COMMENT: reg9 */
	u8 byte7;		/* HSI_COMMENT: byte7 */
	u8 byte8;		/* HSI_COMMENT: byte8 */
	u8 byte9;		/* HSI_COMMENT: byte9 */
	u8 byte10;		/* HSI_COMMENT: byte10 */
	u8 byte11;		/* HSI_COMMENT: byte11 */
	u8 byte12;		/* HSI_COMMENT: byte12 */
	u8 byte13;		/* HSI_COMMENT: byte13 */
	u8 byte14;		/* HSI_COMMENT: byte14 */
	u8 byte15;		/* HSI_COMMENT: byte15 */
	u8 e5_reserved;		/* HSI_COMMENT: e5_reserved */
	__le16 word11;		/* HSI_COMMENT: word11 */
	__le32 reg10;		/* HSI_COMMENT: reg10 */
	__le32 reg11;		/* HSI_COMMENT: reg11 */
	__le32 reg12;		/* HSI_COMMENT: reg12 */
	__le32 reg13;		/* HSI_COMMENT: reg13 */
	__le32 reg14;		/* HSI_COMMENT: reg14 */
	__le32 reg15;		/* HSI_COMMENT: reg15 */
	__le32 reg16;		/* HSI_COMMENT: reg16 */
	__le32 reg17;		/* HSI_COMMENT: reg17 */
	__le32 reg18;		/* HSI_COMMENT: reg18 */
	__le32 reg19;		/* HSI_COMMENT: reg19 */
	__le16 word12;		/* HSI_COMMENT: word12 */
	__le16 word13;		/* HSI_COMMENT: word13 */
	__le16 word14;		/* HSI_COMMENT: word14 */
	__le16 word15;		/* HSI_COMMENT: word15 */
};

struct tstorm_core_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define TSTORM_CORE_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define TSTORM_CORE_CONN_AG_CTX_BIT0_SHIFT		0
#define TSTORM_CORE_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define TSTORM_CORE_CONN_AG_CTX_BIT1_SHIFT		1
#define TSTORM_CORE_CONN_AG_CTX_BIT2_MASK		0x1	/* HSI_COMMENT: bit2 */
#define TSTORM_CORE_CONN_AG_CTX_BIT2_SHIFT		2
#define TSTORM_CORE_CONN_AG_CTX_BIT3_MASK		0x1	/* HSI_COMMENT: bit3 */
#define TSTORM_CORE_CONN_AG_CTX_BIT3_SHIFT		3
#define TSTORM_CORE_CONN_AG_CTX_BIT4_MASK		0x1	/* HSI_COMMENT: bit4 */
#define TSTORM_CORE_CONN_AG_CTX_BIT4_SHIFT		4
#define TSTORM_CORE_CONN_AG_CTX_BIT5_MASK		0x1	/* HSI_COMMENT: bit5 */
#define TSTORM_CORE_CONN_AG_CTX_BIT5_SHIFT		5
#define TSTORM_CORE_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: timer0cf */
#define TSTORM_CORE_CONN_AG_CTX_CF0_SHIFT		6
	u8 flags1;
#define TSTORM_CORE_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: timer1cf */
#define TSTORM_CORE_CONN_AG_CTX_CF1_SHIFT		0
#define TSTORM_CORE_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: timer2cf */
#define TSTORM_CORE_CONN_AG_CTX_CF2_SHIFT		2
#define TSTORM_CORE_CONN_AG_CTX_CF3_MASK		0x3	/* HSI_COMMENT: timer_stop_all */
#define TSTORM_CORE_CONN_AG_CTX_CF3_SHIFT		4
#define TSTORM_CORE_CONN_AG_CTX_CF4_MASK		0x3	/* HSI_COMMENT: cf4 */
#define TSTORM_CORE_CONN_AG_CTX_CF4_SHIFT		6
	u8 flags2;
#define TSTORM_CORE_CONN_AG_CTX_CF5_MASK		0x3	/* HSI_COMMENT: cf5 */
#define TSTORM_CORE_CONN_AG_CTX_CF5_SHIFT		0
#define TSTORM_CORE_CONN_AG_CTX_CF6_MASK		0x3	/* HSI_COMMENT: cf6 */
#define TSTORM_CORE_CONN_AG_CTX_CF6_SHIFT		2
#define TSTORM_CORE_CONN_AG_CTX_CF7_MASK		0x3	/* HSI_COMMENT: cf7 */
#define TSTORM_CORE_CONN_AG_CTX_CF7_SHIFT		4
#define TSTORM_CORE_CONN_AG_CTX_CF8_MASK		0x3	/* HSI_COMMENT: cf8 */
#define TSTORM_CORE_CONN_AG_CTX_CF8_SHIFT		6
	u8 flags3;
#define TSTORM_CORE_CONN_AG_CTX_CF9_MASK		0x3	/* HSI_COMMENT: cf9 */
#define TSTORM_CORE_CONN_AG_CTX_CF9_SHIFT		0
#define TSTORM_CORE_CONN_AG_CTX_CF10_MASK		0x3	/* HSI_COMMENT: cf10 */
#define TSTORM_CORE_CONN_AG_CTX_CF10_SHIFT		2
#define TSTORM_CORE_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define TSTORM_CORE_CONN_AG_CTX_CF0EN_SHIFT		4
#define TSTORM_CORE_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define TSTORM_CORE_CONN_AG_CTX_CF1EN_SHIFT		5
#define TSTORM_CORE_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define TSTORM_CORE_CONN_AG_CTX_CF2EN_SHIFT		6
#define TSTORM_CORE_CONN_AG_CTX_CF3EN_MASK		0x1	/* HSI_COMMENT: cf3en */
#define TSTORM_CORE_CONN_AG_CTX_CF3EN_SHIFT		7
	u8 flags4;
#define TSTORM_CORE_CONN_AG_CTX_CF4EN_MASK		0x1	/* HSI_COMMENT: cf4en */
#define TSTORM_CORE_CONN_AG_CTX_CF4EN_SHIFT		0
#define TSTORM_CORE_CONN_AG_CTX_CF5EN_MASK		0x1	/* HSI_COMMENT: cf5en */
#define TSTORM_CORE_CONN_AG_CTX_CF5EN_SHIFT		1
#define TSTORM_CORE_CONN_AG_CTX_CF6EN_MASK		0x1	/* HSI_COMMENT: cf6en */
#define TSTORM_CORE_CONN_AG_CTX_CF6EN_SHIFT		2
#define TSTORM_CORE_CONN_AG_CTX_CF7EN_MASK		0x1	/* HSI_COMMENT: cf7en */
#define TSTORM_CORE_CONN_AG_CTX_CF7EN_SHIFT		3
#define TSTORM_CORE_CONN_AG_CTX_CF8EN_MASK		0x1	/* HSI_COMMENT: cf8en */
#define TSTORM_CORE_CONN_AG_CTX_CF8EN_SHIFT		4
#define TSTORM_CORE_CONN_AG_CTX_CF9EN_MASK		0x1	/* HSI_COMMENT: cf9en */
#define TSTORM_CORE_CONN_AG_CTX_CF9EN_SHIFT		5
#define TSTORM_CORE_CONN_AG_CTX_CF10EN_MASK		0x1	/* HSI_COMMENT: cf10en */
#define TSTORM_CORE_CONN_AG_CTX_CF10EN_SHIFT		6
#define TSTORM_CORE_CONN_AG_CTX_RULE0EN_MASK		0x1	/* HSI_COMMENT: rule0en */
#define TSTORM_CORE_CONN_AG_CTX_RULE0EN_SHIFT		7
	u8 flags5;
#define TSTORM_CORE_CONN_AG_CTX_RULE1EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define TSTORM_CORE_CONN_AG_CTX_RULE1EN_SHIFT		0
#define TSTORM_CORE_CONN_AG_CTX_RULE2EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define TSTORM_CORE_CONN_AG_CTX_RULE2EN_SHIFT		1
#define TSTORM_CORE_CONN_AG_CTX_RULE3EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define TSTORM_CORE_CONN_AG_CTX_RULE3EN_SHIFT		2
#define TSTORM_CORE_CONN_AG_CTX_RULE4EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define TSTORM_CORE_CONN_AG_CTX_RULE4EN_SHIFT		3
#define TSTORM_CORE_CONN_AG_CTX_RULE5EN_MASK		0x1	/* HSI_COMMENT: rule5en */
#define TSTORM_CORE_CONN_AG_CTX_RULE5EN_SHIFT		4
#define TSTORM_CORE_CONN_AG_CTX_RULE6EN_MASK		0x1	/* HSI_COMMENT: rule6en */
#define TSTORM_CORE_CONN_AG_CTX_RULE6EN_SHIFT		5
#define TSTORM_CORE_CONN_AG_CTX_RULE7EN_MASK		0x1	/* HSI_COMMENT: rule7en */
#define TSTORM_CORE_CONN_AG_CTX_RULE7EN_SHIFT		6
#define TSTORM_CORE_CONN_AG_CTX_RULE8EN_MASK		0x1	/* HSI_COMMENT: rule8en */
#define TSTORM_CORE_CONN_AG_CTX_RULE8EN_SHIFT		7
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
	__le32 reg5;		/* HSI_COMMENT: reg5 */
	__le32 reg6;		/* HSI_COMMENT: reg6 */
	__le32 reg7;		/* HSI_COMMENT: reg7 */
	__le32 reg8;		/* HSI_COMMENT: reg8 */
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: word0 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	u8 byte5;		/* HSI_COMMENT: byte5 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le16 word2;		/* HSI_COMMENT: conn_dpi */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le32 ll2_rx_prod;	/* HSI_COMMENT: reg9 */
	__le32 reg10;		/* HSI_COMMENT: reg10 */
};

struct ustorm_core_conn_ag_ctx {
	u8 reserved;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define USTORM_CORE_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define USTORM_CORE_CONN_AG_CTX_BIT0_SHIFT		0
#define USTORM_CORE_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define USTORM_CORE_CONN_AG_CTX_BIT1_SHIFT		1
#define USTORM_CORE_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: timer0cf */
#define USTORM_CORE_CONN_AG_CTX_CF0_SHIFT		2
#define USTORM_CORE_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: timer1cf */
#define USTORM_CORE_CONN_AG_CTX_CF1_SHIFT		4
#define USTORM_CORE_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: timer2cf */
#define USTORM_CORE_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define USTORM_CORE_CONN_AG_CTX_CF3_MASK		0x3	/* HSI_COMMENT: timer_stop_all */
#define USTORM_CORE_CONN_AG_CTX_CF3_SHIFT		0
#define USTORM_CORE_CONN_AG_CTX_CF4_MASK		0x3	/* HSI_COMMENT: cf4 */
#define USTORM_CORE_CONN_AG_CTX_CF4_SHIFT		2
#define USTORM_CORE_CONN_AG_CTX_CF5_MASK		0x3	/* HSI_COMMENT: cf5 */
#define USTORM_CORE_CONN_AG_CTX_CF5_SHIFT		4
#define USTORM_CORE_CONN_AG_CTX_CF6_MASK		0x3	/* HSI_COMMENT: cf6 */
#define USTORM_CORE_CONN_AG_CTX_CF6_SHIFT		6
	u8 flags2;
#define USTORM_CORE_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define USTORM_CORE_CONN_AG_CTX_CF0EN_SHIFT		0
#define USTORM_CORE_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define USTORM_CORE_CONN_AG_CTX_CF1EN_SHIFT		1
#define USTORM_CORE_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define USTORM_CORE_CONN_AG_CTX_CF2EN_SHIFT		2
#define USTORM_CORE_CONN_AG_CTX_CF3EN_MASK		0x1	/* HSI_COMMENT: cf3en */
#define USTORM_CORE_CONN_AG_CTX_CF3EN_SHIFT		3
#define USTORM_CORE_CONN_AG_CTX_CF4EN_MASK		0x1	/* HSI_COMMENT: cf4en */
#define USTORM_CORE_CONN_AG_CTX_CF4EN_SHIFT		4
#define USTORM_CORE_CONN_AG_CTX_CF5EN_MASK		0x1	/* HSI_COMMENT: cf5en */
#define USTORM_CORE_CONN_AG_CTX_CF5EN_SHIFT		5
#define USTORM_CORE_CONN_AG_CTX_CF6EN_MASK		0x1	/* HSI_COMMENT: cf6en */
#define USTORM_CORE_CONN_AG_CTX_CF6EN_SHIFT		6
#define USTORM_CORE_CONN_AG_CTX_RULE0EN_MASK		0x1	/* HSI_COMMENT: rule0en */
#define USTORM_CORE_CONN_AG_CTX_RULE0EN_SHIFT		7
	u8 flags3;
#define USTORM_CORE_CONN_AG_CTX_RULE1EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define USTORM_CORE_CONN_AG_CTX_RULE1EN_SHIFT		0
#define USTORM_CORE_CONN_AG_CTX_RULE2EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define USTORM_CORE_CONN_AG_CTX_RULE2EN_SHIFT		1
#define USTORM_CORE_CONN_AG_CTX_RULE3EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define USTORM_CORE_CONN_AG_CTX_RULE3EN_SHIFT		2
#define USTORM_CORE_CONN_AG_CTX_RULE4EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define USTORM_CORE_CONN_AG_CTX_RULE4EN_SHIFT		3
#define USTORM_CORE_CONN_AG_CTX_RULE5EN_MASK		0x1	/* HSI_COMMENT: rule5en */
#define USTORM_CORE_CONN_AG_CTX_RULE5EN_SHIFT		4
#define USTORM_CORE_CONN_AG_CTX_RULE6EN_MASK		0x1	/* HSI_COMMENT: rule6en */
#define USTORM_CORE_CONN_AG_CTX_RULE6EN_SHIFT		5
#define USTORM_CORE_CONN_AG_CTX_RULE7EN_MASK		0x1	/* HSI_COMMENT: rule7en */
#define USTORM_CORE_CONN_AG_CTX_RULE7EN_SHIFT		6
#define USTORM_CORE_CONN_AG_CTX_RULE8EN_MASK		0x1	/* HSI_COMMENT: rule8en */
#define USTORM_CORE_CONN_AG_CTX_RULE8EN_SHIFT		7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: conn_dpi */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le32 rx_producers;	/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
};

/* The core storm context for the Mstorm */
struct mstorm_core_conn_st_ctx {
	__le32 reserved[40];
};

/* The core storm context for the Ustorm */
struct ustorm_core_conn_st_ctx {
	__le32 reserved[20];
};

/* The core storm context for the Tstorm */
struct tstorm_core_conn_st_ctx {
	__le32 reserved[4];
};

/* core connection context */
struct core_conn_context {
	struct ystorm_core_conn_st_ctx ystorm_st_context;	/* HSI_COMMENT: ystorm storm context */
	struct regpair ystorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct pstorm_core_conn_st_ctx pstorm_st_context;	/* HSI_COMMENT: pstorm storm context */
	struct regpair pstorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct xstorm_core_conn_st_ctx xstorm_st_context;	/* HSI_COMMENT: xstorm storm context */
	struct xstorm_core_conn_ag_ctx xstorm_ag_context;	/* HSI_COMMENT: xstorm aggregative context */
	struct tstorm_core_conn_ag_ctx tstorm_ag_context;	/* HSI_COMMENT: tstorm aggregative context */
	struct ustorm_core_conn_ag_ctx ustorm_ag_context;	/* HSI_COMMENT: ustorm aggregative context */
	struct mstorm_core_conn_st_ctx mstorm_st_context;	/* HSI_COMMENT: mstorm storm context */
	struct ustorm_core_conn_st_ctx ustorm_st_context;	/* HSI_COMMENT: ustorm storm context */
	struct regpair ustorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct tstorm_core_conn_st_ctx tstorm_st_context;	/* HSI_COMMENT: tstorm storm context */
	struct regpair tstorm_st_padding[2];	/* HSI_COMMENT: padding */
};

/* How ll2 should deal with packet upon errors */
enum core_error_handle {
	LL2_DROP_PACKET,	/* HSI_COMMENT: If error occurs drop packet */
	LL2_DO_NOTHING,		/* HSI_COMMENT: If error occurs do nothing */
	LL2_ASSERT,		/* HSI_COMMENT: If error occurs assert */
	MAX_CORE_ERROR_HANDLE
};

/* opcodes for the event ring */
enum core_event_opcode {
	CORE_EVENT_TX_QUEUE_START,
	CORE_EVENT_TX_QUEUE_STOP,
	CORE_EVENT_RX_QUEUE_START,
	CORE_EVENT_RX_QUEUE_STOP,
	CORE_EVENT_RX_QUEUE_FLUSH,
	CORE_EVENT_TX_QUEUE_UPDATE,
	CORE_EVENT_QUEUE_STATS_QUERY,
	MAX_CORE_EVENT_OPCODE
};

/* The L4 pseudo checksum mode for Core */
enum core_l4_pseudo_checksum_mode {
	CORE_L4_PSEUDO_CSUM_CORRECT_LENGTH,	/* HSI_COMMENT: Pseudo Checksum on packet is calculated with the correct packet length. */
	CORE_L4_PSEUDO_CSUM_ZERO_LENGTH,	/* HSI_COMMENT: Pseudo Checksum on packet is calculated with zero length. */
	MAX_CORE_L4_PSEUDO_CHECKSUM_MODE
};

/* LL2 SP error code */
enum core_ll2_error_code {
	LL2_OK = 0,		/* HSI_COMMENT: Command succeeded */
	LL2_ERROR,		/* HSI_COMMENT: Command failed */
	MAX_CORE_LL2_ERROR_CODE
};

/* Light-L2 RX Producers in Tstorm RAM */
struct core_ll2_port_stats {
	struct regpair gsi_invalid_hdr;
	struct regpair gsi_invalid_pkt_length;
	struct regpair gsi_unsupported_pkt_typ;
	struct regpair gsi_crcchksm_error;
};

/* LL2 TX Per Queue Stats */
struct core_ll2_pstorm_per_queue_stat {
	struct regpair sent_ucast_bytes;	/* HSI_COMMENT: number of total bytes sent without errors */
	struct regpair sent_mcast_bytes;	/* HSI_COMMENT: number of total bytes sent without errors */
	struct regpair sent_bcast_bytes;	/* HSI_COMMENT: number of total bytes sent without errors */
	struct regpair sent_ucast_pkts;	/* HSI_COMMENT: number of total packets sent without errors */
	struct regpair sent_mcast_pkts;	/* HSI_COMMENT: number of total packets sent without errors */
	struct regpair sent_bcast_pkts;	/* HSI_COMMENT: number of total packets sent without errors */
	struct regpair error_drop_pkts;	/* HSI_COMMENT: number of total packets dropped due to errors */
};

struct core_ll2_tstorm_per_queue_stat {
	struct regpair packet_too_big_discard;	/* HSI_COMMENT: Number of packets discarded because they are bigger than MTU */
	struct regpair no_buff_discard;	/* HSI_COMMENT: Number of packets discarded due to lack of host buffers */
};

struct core_ll2_ustorm_per_queue_stat {
	struct regpair rcv_ucast_bytes;
	struct regpair rcv_mcast_bytes;
	struct regpair rcv_bcast_bytes;
	struct regpair rcv_ucast_pkts;
	struct regpair rcv_mcast_pkts;
	struct regpair rcv_bcast_pkts;
};

struct core_ll2_rx_per_queue_stat {
	struct core_ll2_tstorm_per_queue_stat tstorm_stat;	/* HSI_COMMENT: TSTORM per queue statistics */
	struct core_ll2_ustorm_per_queue_stat ustorm_stat;	/* HSI_COMMENT: USTORM per queue statistics */
};

/* Light-L2 RX Producers */
struct core_ll2_rx_prod {
	__le16 bd_prod;		/* HSI_COMMENT: BD Producer */
	__le16 cqe_prod;	/* HSI_COMMENT: CQE Producer */
};

struct core_ll2_tx_per_queue_stat {
	struct core_ll2_pstorm_per_queue_stat pstorm_stat;	/* HSI_COMMENT: PSTORM per queue statistics */
};

/* Structure for doorbell data, in PWM mode, for RX producers update. */
struct core_pwm_prod_update_data {
	__le16 icid;		/* HSI_COMMENT: internal CID */
	u8 reserved0;
	u8 params;
#define CORE_PWM_PROD_UPDATE_DATA_AGG_CMD_MASK		0x3	/* HSI_COMMENT: aggregative command. Set DB_AGG_CMD_SET for producer update (use enum db_agg_cmd_sel) */
#define CORE_PWM_PROD_UPDATE_DATA_AGG_CMD_SHIFT		0
#define CORE_PWM_PROD_UPDATE_DATA_RESERVED1_MASK	0x3F	/* HSI_COMMENT: Set 0. */
#define CORE_PWM_PROD_UPDATE_DATA_RESERVED1_SHIFT	2
	struct core_ll2_rx_prod prod;	/* HSI_COMMENT: Producers. */
};

/* Ramrod data for rx/tx queue statistics query ramrod */
struct core_queue_stats_query_ramrod_data {
	u8 rx_stat;		/* HSI_COMMENT: If set, collect RX queue statistics. */
	u8 tx_stat;		/* HSI_COMMENT: If set, collect TX queue statistics. */
	__le16 reserved[3];
	struct regpair rx_stat_addr;	/* HSI_COMMENT: Address of RX statistic buffer. core_ll2_rx_per_queue_stat struct will be write to this address. */
	struct regpair tx_stat_addr;	/* HSI_COMMENT: Address of TX statistic buffer. core_ll2_tx_per_queue_stat struct will be write to this address. */
};

/* Core Ramrod Command IDs (light L2) */
enum core_ramrod_cmd_id {
	CORE_RAMROD_UNUSED,
	CORE_RAMROD_RX_QUEUE_START,	/* HSI_COMMENT: RX Queue Start Ramrod */
	CORE_RAMROD_TX_QUEUE_START,	/* HSI_COMMENT: TX Queue Start Ramrod */
	CORE_RAMROD_RX_QUEUE_STOP,	/* HSI_COMMENT: RX Queue Stop Ramrod */
	CORE_RAMROD_TX_QUEUE_STOP,	/* HSI_COMMENT: TX Queue Stop Ramrod */
	CORE_RAMROD_RX_QUEUE_FLUSH,	/* HSI_COMMENT: RX Flush queue Ramrod */
	CORE_RAMROD_TX_QUEUE_UPDATE,	/* HSI_COMMENT: TX Queue Update Ramrod */
	CORE_RAMROD_QUEUE_STATS_QUERY,	/* HSI_COMMENT: Queue Statist Query Ramrod */
	MAX_CORE_RAMROD_CMD_ID
};

/* Core RX CQE Type for Light L2 */
enum core_roce_flavor_type {
	CORE_ROCE,
	CORE_RROCE,
	MAX_CORE_ROCE_FLAVOR_TYPE
};

/* Specifies how ll2 should deal with packets errors: packet_too_big and no_buff */
struct core_rx_action_on_error {
	u8 error_type;
#define CORE_RX_ACTION_ON_ERROR_PACKET_TOO_BIG_MASK	0x3	/* HSI_COMMENT: ll2 how to handle error packet_too_big (use enum core_error_handle) */
#define CORE_RX_ACTION_ON_ERROR_PACKET_TOO_BIG_SHIFT	0
#define CORE_RX_ACTION_ON_ERROR_NO_BUFF_MASK		0x3	/* HSI_COMMENT: ll2 how to handle error with no_buff  (use enum core_error_handle) */
#define CORE_RX_ACTION_ON_ERROR_NO_BUFF_SHIFT		2
#define CORE_RX_ACTION_ON_ERROR_RESERVED_MASK		0xF
#define CORE_RX_ACTION_ON_ERROR_RESERVED_SHIFT		4
};

/* Core RX BD for Light L2 */
struct core_rx_bd {
	struct regpair addr;
	__le16 reserved[4];
};

/* Core RX CM offload BD for Light L2 */
struct core_rx_bd_with_buff_len {
	struct regpair addr;
	__le16 buff_length;
	__le16 reserved[3];
};

/* Core RX CM offload BD for Light L2 */
union core_rx_bd_union {
	struct core_rx_bd rx_bd;	/* HSI_COMMENT: Core Rx Bd static buffer size */
	struct core_rx_bd_with_buff_len rx_bd_with_len;	/* HSI_COMMENT: Core Rx Bd with dynamic buffer length */
};

/* Opaque Data for Light L2 RX CQE .  */
struct core_rx_cqe_opaque_data {
	__le32 data[2];		/* HSI_COMMENT: Opaque CQE Data */
};

/* Core RX CQE Type for Light L2 */
enum core_rx_cqe_type {
	CORE_RX_CQE_ILLIGAL_TYPE,	/* HSI_COMMENT: Bad RX Cqe type */
	CORE_RX_CQE_TYPE_REGULAR,	/* HSI_COMMENT: Regular Core RX CQE */
	CORE_RX_CQE_TYPE_GSI_OFFLOAD,	/* HSI_COMMENT: Fp Gsi offload RX CQE */
	CORE_RX_CQE_TYPE_SLOW_PATH,	/* HSI_COMMENT: Slow path Core RX CQE */
	MAX_CORE_RX_CQE_TYPE
};

/* Core RX CQE for Light L2 .  */
struct core_rx_fast_path_cqe {
	u8 type;		/* HSI_COMMENT: CQE type (use enum core_rx_cqe_type) */
	u8 placement_offset;	/* HSI_COMMENT: Offset (in bytes) of the packet from start of the buffer */
	struct parsing_and_err_flags parse_flags;	/* HSI_COMMENT: Parsing and error flags from the parser */
	__le16 packet_length;	/* HSI_COMMENT: Total packet length (from the parser) */
	__le16 vlan;		/* HSI_COMMENT: 802.1q VLAN tag */
	struct core_rx_cqe_opaque_data opaque_data;	/* HSI_COMMENT: Opaque Data */
	struct parsing_err_flags err_flags;	/* HSI_COMMENT: bit- map: each bit represents a specific error. errors indications are provided by the FC. see spec for detailed description */
	u8 packet_source;	/* HSI_COMMENT: RX packet source. (use enum core_rx_pkt_source) */
	u8 reserved0;
	__le32 reserved1[3];
};

/* Core Rx CM offload CQE .  */
struct core_rx_gsi_offload_cqe {
	u8 type;		/* HSI_COMMENT: CQE type (use enum core_rx_cqe_type) */
	u8 data_length_error;	/* HSI_COMMENT: set if GSI data is bigger than the buffer */
	struct parsing_and_err_flags parse_flags;	/* HSI_COMMENT: Parsing and error flags from the parser */
	__le16 data_length;	/* HSI_COMMENT: Total packet length (from the parser) */
	__le16 vlan;		/* HSI_COMMENT: 802.1q VLAN tag */
	__le32 src_mac_addrhi;	/* HSI_COMMENT: hi 4 bytes source mac address */
	__le16 src_mac_addrlo;	/* HSI_COMMENT: lo 2 bytes of source mac address */
	__le16 qp_id;		/* HSI_COMMENT: These are the lower 16 bit of QP id in RoCE BTH header */
	__le32 src_qp;		/* HSI_COMMENT: Source QP from DETH header */
	struct core_rx_cqe_opaque_data opaque_data;	/* HSI_COMMENT: Opaque Data */
	u8 packet_source;	/* HSI_COMMENT: RX packet source. (use enum core_rx_pkt_source) */
	u8 reserved[3];
};

/* Core RX CQE for Light L2 .  */
struct core_rx_slow_path_cqe {
	u8 type;		/* HSI_COMMENT: CQE type (use enum core_rx_cqe_type) */
	u8 ramrod_cmd_id;	/* HSI_COMMENT:  (use enum core_ramrod_cmd_id) */
	__le16 echo;
	struct core_rx_cqe_opaque_data opaque_data;	/* HSI_COMMENT: Opaque Data */
	__le32 reserved1[5];
};

/* Core RX CM offload BD for Light L2 */
union core_rx_cqe_union {
	struct core_rx_fast_path_cqe rx_cqe_fp;	/* HSI_COMMENT: Fast path CQE */
	struct core_rx_gsi_offload_cqe rx_cqe_gsi;	/* HSI_COMMENT: GSI offload CQE */
	struct core_rx_slow_path_cqe rx_cqe_sp;	/* HSI_COMMENT: Slow path CQE */
};

/* RX packet source. */
enum core_rx_pkt_source {
	CORE_RX_PKT_SOURCE_NETWORK = 0,	/* HSI_COMMENT: Regular RX packet from network port */
	CORE_RX_PKT_SOURCE_LB,	/* HSI_COMMENT: Loop back packet */
	CORE_RX_PKT_SOURCE_TX,	/* HSI_COMMENT: TX packet duplication. Used for debug. */
	CORE_RX_PKT_SOURCE_LL2_TX,	/* HSI_COMMENT: Loopback packet from LL2 TX */
	MAX_CORE_RX_PKT_SOURCE
};

/* Ramrod data for rx queue start ramrod */
struct core_rx_start_ramrod_data {
	struct regpair bd_base;	/* HSI_COMMENT: Address of the first BD page */
	struct regpair cqe_pbl_addr;	/* HSI_COMMENT: Base address on host of CQE PBL */
	__le16 mtu;		/* HSI_COMMENT: MTU */
	__le16 sb_id;		/* HSI_COMMENT: Status block ID */
	u8 sb_index;		/* HSI_COMMENT: Status block index */
	u8 complete_cqe_flg;	/* HSI_COMMENT: if set - post completion to the CQE ring */
	u8 complete_event_flg;	/* HSI_COMMENT: if set - post completion to the event ring */
	u8 drop_ttl0_flg;	/* HSI_COMMENT: if set - drop packet with ttl=0 */
	__le16 num_of_pbl_pages;	/* HSI_COMMENT: Number of pages in CQE PBL */
	u8 inner_vlan_stripping_en;	/* HSI_COMMENT: if set - 802.1q tag will be removed and copied to CQE. Set only if vport_id_valid flag clear. If vport_id_valid flag set, VPORT configuration used instead. */
	u8 report_outer_vlan;	/* HSI_COMMENT: if set and inner vlan does not exist, the outer vlan will copied to CQE as inner vlan. should be used in MF_OVLAN mode only. */
	u8 queue_id;		/* HSI_COMMENT: Light L2 RX Queue ID */
	u8 main_func_queue;	/* HSI_COMMENT: Set if this is the main PFs LL2 queue */
	u8 mf_si_bcast_accept_all;	/* HSI_COMMENT: Duplicate broadcast packets to LL2 main queue in mf_si mode. Valid if main_func_queue is set. */
	u8 mf_si_mcast_accept_all;	/* HSI_COMMENT: Duplicate multicast packets to LL2 main queue in mf_si mode. Valid if main_func_queue is set. */
	struct core_rx_action_on_error action_on_error;	/* HSI_COMMENT: Specifies how ll2 should deal with RX packets errors */
	u8 gsi_offload_flag;	/* HSI_COMMENT: set for GSI offload mode */
	u8 vport_id_valid;	/* HSI_COMMENT: If set, queue is subject for RX VFC classification. */
	u8 vport_id;		/* HSI_COMMENT: Queue VPORT for RX VFC classification. */
	u8 zero_prod_flg;	/* HSI_COMMENT: If set, zero RX producers. */
	u8 wipe_inner_vlan_pri_en;	/* HSI_COMMENT: If set, the inner vlan (802.1q tag) priority that is written to cqe will be zero out, used for TenantDcb */
	u8 reserved[2];
};

/* Ramrod data for rx queue stop ramrod */
struct core_rx_stop_ramrod_data {
	u8 complete_cqe_flg;	/* HSI_COMMENT: if set - post completion to the CQE ring */
	u8 complete_event_flg;	/* HSI_COMMENT: if set - post completion to the event ring */
	u8 queue_id;		/* HSI_COMMENT: Light L2 RX Queue ID */
	u8 reserved1;
	__le16 reserved2[2];
};

/* Flags for Core TX BD */
struct core_tx_bd_data {
	__le16 as_bitfield;
#define CORE_TX_BD_DATA_FORCE_VLAN_MODE_MASK		0x1	/* HSI_COMMENT: Do not allow additional VLAN manipulations on this packet (DCB) */
#define CORE_TX_BD_DATA_FORCE_VLAN_MODE_SHIFT		0
#define CORE_TX_BD_DATA_VLAN_INSERTION_MASK		0x1	/* HSI_COMMENT: Insert VLAN into packet. Cannot be set for LB packets (tx_dst == CORE_TX_DEST_LB) */
#define CORE_TX_BD_DATA_VLAN_INSERTION_SHIFT		1
#define CORE_TX_BD_DATA_START_BD_MASK			0x1	/* HSI_COMMENT: This is the first BD of the packet (for debug) */
#define CORE_TX_BD_DATA_START_BD_SHIFT			2
#define CORE_TX_BD_DATA_IP_CSUM_MASK			0x1	/* HSI_COMMENT: Calculate the IP checksum for the packet */
#define CORE_TX_BD_DATA_IP_CSUM_SHIFT			3
#define CORE_TX_BD_DATA_L4_CSUM_MASK			0x1	/* HSI_COMMENT: Calculate the L4 checksum for the packet */
#define CORE_TX_BD_DATA_L4_CSUM_SHIFT			4
#define CORE_TX_BD_DATA_IPV6_EXT_MASK			0x1	/* HSI_COMMENT: Packet is IPv6 with extensions */
#define CORE_TX_BD_DATA_IPV6_EXT_SHIFT			5
#define CORE_TX_BD_DATA_L4_PROTOCOL_MASK		0x1	/* HSI_COMMENT: If IPv6+ext, and if l4_csum is 1, than this field indicates L4 protocol: 0-TCP, 1-UDP */
#define CORE_TX_BD_DATA_L4_PROTOCOL_SHIFT		6
#define CORE_TX_BD_DATA_L4_PSEUDO_CSUM_MODE_MASK	0x1	/* HSI_COMMENT: The pseudo checksum mode to place in the L4 checksum field. Required only when IPv6+ext and l4_csum is set. (use enum core_l4_pseudo_checksum_mode) */
#define CORE_TX_BD_DATA_L4_PSEUDO_CSUM_MODE_SHIFT	7
#define CORE_TX_BD_DATA_NBDS_MASK			0xF	/* HSI_COMMENT: Number of BDs that make up one packet - width wide enough to present CORE_LL2_TX_MAX_BDS_PER_PACKET */
#define CORE_TX_BD_DATA_NBDS_SHIFT			8
#define CORE_TX_BD_DATA_ROCE_FLAV_MASK			0x1	/* HSI_COMMENT: Use roce_flavor enum - Differentiate between Roce flavors is valid when connType is ROCE (use enum core_roce_flavor_type) */
#define CORE_TX_BD_DATA_ROCE_FLAV_SHIFT			12
#define CORE_TX_BD_DATA_IP_LEN_MASK			0x1	/* HSI_COMMENT: Calculate ip length */
#define CORE_TX_BD_DATA_IP_LEN_SHIFT			13
#define CORE_TX_BD_DATA_DISABLE_STAG_INSERTION_MASK	0x1	/* HSI_COMMENT: disables the STAG insertion, relevant only in MF OVLAN mode. */
#define CORE_TX_BD_DATA_DISABLE_STAG_INSERTION_SHIFT	14
#define CORE_TX_BD_DATA_RESERVED0_MASK			0x1
#define CORE_TX_BD_DATA_RESERVED0_SHIFT			15
};

/* Core TX BD for Light L2 */
struct core_tx_bd {
	struct regpair addr;	/* HSI_COMMENT: Buffer Address */
	__le16 nbytes;		/* HSI_COMMENT: Number of Bytes in Buffer */
	__le16 nw_vlan_or_lb_echo;	/* HSI_COMMENT: Network packets: VLAN to insert to packet (if insertion flag set) LoopBack packets: echo data to pass to Rx */
	struct core_tx_bd_data bd_data;	/* HSI_COMMENT: BD flags */
	__le16 bitfield1;
#define CORE_TX_BD_L4_HDR_OFFSET_W_MASK		0x3FFF	/* HSI_COMMENT: L4 Header Offset from start of packet (in Words). This is needed if both l4_csum and ipv6_ext are set */
#define CORE_TX_BD_L4_HDR_OFFSET_W_SHIFT	0
#define CORE_TX_BD_TX_DST_MASK			0x3	/* HSI_COMMENT: Packet destination - Network, Loopback or Drop (use enum core_tx_dest) */
#define CORE_TX_BD_TX_DST_SHIFT			14
};

/* Light L2 TX Destination */
enum core_tx_dest {
	CORE_TX_DEST_NW,	/* HSI_COMMENT: TX Destination to the Network */
	CORE_TX_DEST_LB,	/* HSI_COMMENT: TX Destination to the Loopback */
	CORE_TX_DEST_RESERVED,
	CORE_TX_DEST_DROP,	/* HSI_COMMENT: TX Drop */
	MAX_CORE_TX_DEST
};

/* Ramrod data for tx queue start ramrod */
struct core_tx_start_ramrod_data {
	struct regpair pbl_base_addr;	/* HSI_COMMENT: Address of the pbl page */
	__le16 mtu;		/* HSI_COMMENT: MTU */
	__le16 sb_id;		/* HSI_COMMENT: Status block ID */
	u8 sb_index;		/* HSI_COMMENT: Status block index */
	u8 stats_en;		/* HSI_COMMENT: Ram statistics enable */
	u8 stats_id;		/* HSI_COMMENT: Ram statistics counter ID */
	u8 conn_type;		/* HSI_COMMENT: Parent protocol type (use enum protocol_type) */
	__le16 pbl_size;	/* HSI_COMMENT: Number of BD pages pointed by PBL */
	__le16 qm_pq_id;	/* HSI_COMMENT: QM PQ ID */
	u8 gsi_offload_flag;	/* HSI_COMMENT: set for GSI offload mode */
	u8 ctx_stats_en;	/* HSI_COMMENT: Context statistics enable */
	u8 vport_id_valid;	/* HSI_COMMENT: If set, queue is part of VPORT and subject for TX switching. */
	u8 vport_id;		/* HSI_COMMENT: vport id of the current connection, used to access non_rdma_in_to_in_pri_map which is per vport */
	u8 enforce_security_flag;	/* HSI_COMMENT: if set, security checks will be made for this connection. If set, disable_stag_insertion flag must be clear in TX BD. If set and anti-spoofing enable for associated VPORT, only packets with self destination MAC can be forced to LB. */
	u8 reserved[7];
};

/* Ramrod data for tx queue stop ramrod */
struct core_tx_stop_ramrod_data {
	__le32 reserved0[2];
};

/* Ramrod data for tx queue update ramrod */
struct core_tx_update_ramrod_data {
	u8 update_qm_pq_id_flg;	/* HSI_COMMENT: Flag to Update QM PQ ID */
	u8 reserved0;
	__le16 qm_pq_id;	/* HSI_COMMENT: Updated QM PQ ID */
	__le32 reserved1[1];
};

/* Enum flag for what type of DCB data to update */
enum dcb_dscp_update_mode {
	DONT_UPDATE_DCB_DSCP,	/* HSI_COMMENT: Set when no change should be done to DCB data */
	UPDATE_DCB,		/* HSI_COMMENT: Set to update only L2 (vlan) priority */
	UPDATE_DSCP,		/* HSI_COMMENT: Set to update only IP DSCP */
	UPDATE_DCB_DSCP,	/* HSI_COMMENT: Set to update vlan pri and DSCP */
	MAX_DCB_DSCP_UPDATE_MODE
};

struct eth_mstorm_per_pf_stat {
	struct regpair gre_discard_pkts;	/* HSI_COMMENT: Dropped GRE RX packets */
	struct regpair vxlan_discard_pkts;	/* HSI_COMMENT: Dropped VXLAN RX packets */
	struct regpair geneve_discard_pkts;	/* HSI_COMMENT: Dropped GENEVE RX packets */
	struct regpair lb_discard_pkts;	/* HSI_COMMENT: Dropped Tx switched packets */
};

struct eth_mstorm_per_queue_stat {
	struct regpair ttl0_discard;	/* HSI_COMMENT: Number of packets discarded because TTL=0 (in IPv4) or hopLimit=0 (in IPv6) */
	struct regpair packet_too_big_discard;	/* HSI_COMMENT: Number of packets discarded because they are bigger than MTU */
	struct regpair no_buff_discard;	/* HSI_COMMENT: Number of packets discarded due to lack of host buffers (BDs/SGEs/CQEs) */
	struct regpair not_active_discard;	/* HSI_COMMENT: Number of packets discarded because of no active Rx connection */
	struct regpair tpa_coalesced_pkts;	/* HSI_COMMENT: number of coalesced packets in all TPA aggregations */
	struct regpair tpa_coalesced_events;	/* HSI_COMMENT: total number of TPA aggregations */
	struct regpair tpa_aborts_num;	/* HSI_COMMENT: number of aggregations, which abnormally ended */
	struct regpair tpa_coalesced_bytes;	/* HSI_COMMENT: total TCP payload length in all TPA aggregations */
};

/* Ethernet TX Per PF */
struct eth_pstorm_per_pf_stat {
	struct regpair sent_lb_ucast_bytes;	/* HSI_COMMENT: number of total ucast bytes sent on loopback port without errors */
	struct regpair sent_lb_mcast_bytes;	/* HSI_COMMENT: number of total mcast bytes sent on loopback port without errors */
	struct regpair sent_lb_bcast_bytes;	/* HSI_COMMENT: number of total bcast bytes sent on loopback port without errors */
	struct regpair sent_lb_ucast_pkts;	/* HSI_COMMENT: number of total ucast packets sent on loopback port without errors */
	struct regpair sent_lb_mcast_pkts;	/* HSI_COMMENT: number of total mcast packets sent on loopback port without errors */
	struct regpair sent_lb_bcast_pkts;	/* HSI_COMMENT: number of total bcast packets sent on loopback port without errors */
	struct regpair sent_gre_bytes;	/* HSI_COMMENT: Sent GRE bytes */
	struct regpair sent_vxlan_bytes;	/* HSI_COMMENT: Sent VXLAN bytes */
	struct regpair sent_geneve_bytes;	/* HSI_COMMENT: Sent GENEVE bytes */
	struct regpair sent_mpls_bytes;	/* HSI_COMMENT: Sent MPLS bytes */
	struct regpair sent_gre_mpls_bytes;	/* HSI_COMMENT: Sent GRE MPLS bytes (E5 Only) */
	struct regpair sent_udp_mpls_bytes;	/* HSI_COMMENT: Sent GRE MPLS bytes (E5 Only) */
	struct regpair sent_gre_pkts;	/* HSI_COMMENT: Sent GRE packets (E5 Only) */
	struct regpair sent_vxlan_pkts;	/* HSI_COMMENT: Sent VXLAN packets */
	struct regpair sent_geneve_pkts;	/* HSI_COMMENT: Sent GENEVE packets */
	struct regpair sent_mpls_pkts;	/* HSI_COMMENT: Sent MPLS packets (E5 Only) */
	struct regpair sent_gre_mpls_pkts;	/* HSI_COMMENT: Sent GRE MPLS packets (E5 Only) */
	struct regpair sent_udp_mpls_pkts;	/* HSI_COMMENT: Sent UDP MPLS packets (E5 Only) */
	struct regpair gre_drop_pkts;	/* HSI_COMMENT: Dropped GRE TX packets */
	struct regpair vxlan_drop_pkts;	/* HSI_COMMENT: Dropped VXLAN TX packets */
	struct regpair geneve_drop_pkts;	/* HSI_COMMENT: Dropped GENEVE TX packets */
	struct regpair mpls_drop_pkts;	/* HSI_COMMENT: Dropped MPLS TX packets (E5 Only) */
	struct regpair gre_mpls_drop_pkts;	/* HSI_COMMENT: Dropped GRE MPLS TX packets (E5 Only) */
	struct regpair udp_mpls_drop_pkts;	/* HSI_COMMENT: Dropped UDP MPLS TX packets (E5 Only) */
};

/* Ethernet TX Per Queue Stats */
struct eth_pstorm_per_queue_stat {
	struct regpair sent_ucast_bytes;	/* HSI_COMMENT: number of total bytes sent without errors */
	struct regpair sent_mcast_bytes;	/* HSI_COMMENT: number of total bytes sent without errors */
	struct regpair sent_bcast_bytes;	/* HSI_COMMENT: number of total bytes sent without errors */
	struct regpair sent_ucast_pkts;	/* HSI_COMMENT: number of total packets sent without errors */
	struct regpair sent_mcast_pkts;	/* HSI_COMMENT: number of total packets sent without errors */
	struct regpair sent_bcast_pkts;	/* HSI_COMMENT: number of total packets sent without errors */
	struct regpair error_drop_pkts;	/* HSI_COMMENT: number of total packets dropped due to errors */
};

/* return code from eth sp ramrods */
struct eth_return_code {
	u8 value;
#define ETH_RETURN_CODE_ERR_CODE_MASK		0x3F	/* HSI_COMMENT: error code (use enum eth_error_code) */
#define ETH_RETURN_CODE_ERR_CODE_SHIFT		0
#define ETH_RETURN_CODE_RESERVED_MASK		0x1
#define ETH_RETURN_CODE_RESERVED_SHIFT		6
#define ETH_RETURN_CODE_RX_TX_MASK		0x1	/* HSI_COMMENT: rx path - 0, tx path - 1 */
#define ETH_RETURN_CODE_RX_TX_SHIFT		7
};

/* ETH Rx producers data */
struct eth_rx_rate_limit {
	__le16 mult;		/* HSI_COMMENT: Rate Limit Multiplier - (Storm Clock (MHz) * 8 / Desired Bandwidth (MB/s)) */
	__le16 cnst;		/* HSI_COMMENT: Constant term to add (or subtract from number of cycles) */
	u8 add_sub_cnst;	/* HSI_COMMENT: Add (1) or subtract (0) constant term */
	u8 reserved0;
	__le16 reserved1;
};

/* Update RSS indirection table entry command. One outstanding command supported per PF. */
struct eth_tstorm_rss_update_data {
	u8 vport_id;		/* HSI_COMMENT: Global VPORT ID. If RSS is disable for VPORT, RSS update command will be ignored. */
	u8 ind_table_index;	/* HSI_COMMENT: RSS indirect table index that will be updated. */
	__le16 ind_table_value;	/* HSI_COMMENT: RSS indirect table new value. */
	__le16 reserved1;	/* HSI_COMMENT: reserved. */
	u8 reserved;
	u8 valid;		/* HSI_COMMENT: Valid flag. Driver must set this flag, FW clear valid flag when ready for new RSS update command. */
};

struct eth_ustorm_per_pf_stat {
	struct regpair rcv_lb_ucast_bytes;	/* HSI_COMMENT: number of total ucast bytes received on loopback port without errors */
	struct regpair rcv_lb_mcast_bytes;	/* HSI_COMMENT: number of total mcast bytes received on loopback port without errors */
	struct regpair rcv_lb_bcast_bytes;	/* HSI_COMMENT: number of total bcast bytes received on loopback port without errors */
	struct regpair rcv_lb_ucast_pkts;	/* HSI_COMMENT: number of total ucast packets received on loopback port without errors */
	struct regpair rcv_lb_mcast_pkts;	/* HSI_COMMENT: number of total mcast packets received on loopback port without errors */
	struct regpair rcv_lb_bcast_pkts;	/* HSI_COMMENT: number of total bcast packets received on loopback port without errors */
	struct regpair rcv_gre_bytes;	/* HSI_COMMENT: Received GRE bytes */
	struct regpair rcv_vxlan_bytes;	/* HSI_COMMENT: Received VXLAN bytes */
	struct regpair rcv_geneve_bytes;	/* HSI_COMMENT: Received GENEVE bytes */
	struct regpair rcv_gre_pkts;	/* HSI_COMMENT: Received GRE packets */
	struct regpair rcv_vxlan_pkts;	/* HSI_COMMENT: Received VXLAN packets */
	struct regpair rcv_geneve_pkts;	/* HSI_COMMENT: Received GENEVE packets */
};

struct eth_ustorm_per_queue_stat {
	struct regpair rcv_ucast_bytes;
	struct regpair rcv_mcast_bytes;
	struct regpair rcv_bcast_bytes;
	struct regpair rcv_ucast_pkts;
	struct regpair rcv_mcast_pkts;
	struct regpair rcv_bcast_pkts;
};

/* Event Ring VF-PF Channel data */
struct vf_pf_channel_eqe_data {
	struct regpair msg_addr;	/* HSI_COMMENT: VF-PF message address */
};

/* Event Ring initial cleanup data */
struct initial_cleanup_eqe_data {
	u8 vf_id;		/* HSI_COMMENT: VF ID */
	u8 reserved[7];
};

/* FW error data */
struct fw_err_data {
	u8 recovery_scope;	/* HSI_COMMENT: FW error recovery scope (use enum fw_err_recovery_scope) */
	u8 err_id;		/* HSI_COMMENT: Function error ID (use enum func_err_id) */
	__le16 entity_id;	/* HSI_COMMENT: ID of the entity to recover (queue, vport, function , etc.). The entity type is according to the recovery_scope */
	u8 reserved[4];
};

/* Event Data Union */
union event_ring_data {
	u8 bytes[8];		/* HSI_COMMENT: Byte Array */
	struct vf_pf_channel_eqe_data vf_pf_channel;	/* HSI_COMMENT: VF-PF Channel data */
	struct iscsi_eqe_data iscsi_info;	/* HSI_COMMENT: Dedicated fields to iscsi data */
	struct iscsi_connect_done_results iscsi_conn_done_info;	/* HSI_COMMENT: Dedicated fields to iscsi connect done results */
	union rdma_eqe_data rdma_data;	/* HSI_COMMENT: Dedicated field for RDMA data */
	struct nvmf_eqe_data nvmf_data;	/* HSI_COMMENT: Dedicated field for NVMf data */
	struct initial_cleanup_eqe_data vf_init_cleanup;	/* HSI_COMMENT: VF Initial Cleanup data */
	struct fw_err_data err_data;	/* HSI_COMMENT: FW error recovery data. Valid only if fw_return_code != 0 */
};

/* Event Opcode */
union event_ring_opcode {
	u8 raw;
	u8 common;		/* HSI_COMMENT:  (use enum common_event_opcode) */
	u8 core;		/* HSI_COMMENT:  (use enum core_event_opcode) */
	u8 eth;			/* HSI_COMMENT:  (use enum eth_event_opcode) */
	u8 iscsi;		/* HSI_COMMENT:  (use enum iscsi_eqe_opcode) */
	u8 toe;			/* HSI_COMMENT:  (use enum toe_rx_cmp_opcode) */
	u8 fcoe;		/* HSI_COMMENT:  (use enum fcoe_event_type) */
	u8 rdma;		/* HSI_COMMENT:  (use enum rdma_event_opcode) */
	u8 roce;		/* HSI_COMMENT:  (use enum roce_event_opcode) */
	u8 roce_async;		/* HSI_COMMENT:  (use enum roce_async_events_type) */
	u8 iwarp;		/* HSI_COMMENT:  (use enum iwarp_eqe_sync_opcode) */
	u8 iwarp_async;		/* HSI_COMMENT:  (use enum iwarp_eqe_async_opcode) */
	u8 nvmf;		/* HSI_COMMENT:  (use enum nvmf_event_opcode) */
	u8 nvmf_async;		/* HSI_COMMENT:  (use enum nvmf_async_event_opcode) */
};

/* FW return code for SP ramrods. */
union event_ring_fw_return_code {
	u8 raw;
	u8 common;		/* HSI_COMMENT:  (use enum protocol_common_error_code) */
	u8 core;		/* HSI_COMMENT:  (use enum core_ll2_error_code) */
	struct eth_return_code eth;
	u8 fcoe;		/* HSI_COMMENT:  (use enum fcoe_completion_status) */
	u8 rdma;		/* HSI_COMMENT:  (use enum rdma_fw_return_code) */
	u8 iwarp;		/* HSI_COMMENT:  (use enum iwarp_fw_return_code) */
	u8 nvmf;		/* HSI_COMMENT:  (use enum nvmf_status) */
};

/* Event Ring Entry */
struct event_ring_entry {
	u8 protocol_id;		/* HSI_COMMENT: Event Protocol ID (use enum protocol_type) */
	union event_ring_opcode opcode;	/* HSI_COMMENT: Per protocol_id Event Opcode */
	u8 flags;
#define EVENT_RING_ENTRY_ASYNC_MASK		0x1	/* HSI_COMMENT: 0: synchronous EQE - a completion of SP message. 1: asynchronous EQE */
#define EVENT_RING_ENTRY_ASYNC_SHIFT		0
#define EVENT_RING_ENTRY_RESERVED1_MASK		0x7F
#define EVENT_RING_ENTRY_RESERVED1_SHIFT	1
	union event_ring_fw_return_code fw_return_code;	/* HSI_COMMENT: FW return code for SP ramrods. Use (according to protocol) eth_return_code, rdma_fw_return_code, fcoe_completion_status, core_ll2_error_code, protocol_common_error_code. If fw_return_code != 0, data.err_data may contain the error data. */
	u8 reserved0;		/* HSI_COMMENT: Reserved */
	u8 vf_id;		/* HSI_COMMENT: VF ID for this event, 0xFF if this is a PF event */
	__le16 echo;		/* HSI_COMMENT: Echo value from ramrod data on the host */
	union event_ring_data data;
};

/* Event Ring Next Page Address */
struct event_ring_next_addr {
	struct regpair addr;	/* HSI_COMMENT: Next Page Address */
	__le32 reserved[2];	/* HSI_COMMENT: Reserved */
};

/* Event Ring Element */
union event_ring_element {
	struct event_ring_entry entry;	/* HSI_COMMENT: Event Ring Entry */
	struct event_ring_next_addr next_addr;	/* HSI_COMMENT: Event Ring Next Page Address */
};

/* Function error ID */
enum func_err_id {
	FUNC_NO_ERROR,		/* HSI_COMMENT: No Error */
	VF_PF_CHANNEL_NOT_READY,	/* HSI_COMMENT: Writing to VF/PF channel when it is not ready */
	VF_ZONE_MSG_NOT_VALID,	/* HSI_COMMENT: VF channel message is not valid */
	VF_ZONE_FUNC_NOT_ENABLED,	/* HSI_COMMENT: Parent PF of VF channel is not active */
	ETH_PACKET_TOO_SMALL,	/* HSI_COMMENT: TX packet is shorter then reported on BDs or from minimal size */
	ETH_ILLEGAL_VLAN_MODE,	/* HSI_COMMENT: Tx packet with marked as insert VLAN when its illegal */
	ETH_MTU_VIOLATION,	/* HSI_COMMENT: TX packet is greater then MTU */
	ETH_ILLEGAL_INBAND_TAGS,	/* HSI_COMMENT: TX packet has illegal inband tags marked */
	ETH_VLAN_INSERT_AND_INBAND_VLAN,	/* HSI_COMMENT: Vlan cant be added to inband tag */
	ETH_ILLEGAL_NBDS,	/* HSI_COMMENT: indicated number of BDs for the packet is illegal */
	ETH_FIRST_BD_WO_SOP,	/* HSI_COMMENT: 1st BD must have start_bd flag set */
	ETH_INSUFFICIENT_BDS,	/* HSI_COMMENT: There are not enough BDs for transmission of even one packet */
	ETH_ILLEGAL_LSO_HDR_NBDS,	/* HSI_COMMENT: Header NBDs value is illegal */
	ETH_ILLEGAL_LSO_MSS,	/* HSI_COMMENT: LSO MSS value is more than allowed */
	ETH_ZERO_SIZE_BD,	/* HSI_COMMENT: empty BD (which not contains control flags) is illegal  */
	ETH_ILLEGAL_LSO_HDR_LEN,	/* HSI_COMMENT: LSO header size is above the limit  */
	ETH_INSUFFICIENT_PAYLOAD,	/* HSI_COMMENT: In LSO its expected that on the local BD ring there will be at least MSS bytes of data */
	ETH_EDPM_OUT_OF_SYNC,	/* HSI_COMMENT: Valid BDs on local ring after EDPM L2 sync */
	ETH_TUNN_IPV6_EXT_NBD_ERR,	/* HSI_COMMENT: Tunneled packet with IPv6+Ext without a proper number of BDs */
	ETH_CONTROL_PACKET_VIOLATION,	/* HSI_COMMENT: VF sent control frame such as PFC */
	ETH_ANTI_SPOOFING_ERR,	/* HSI_COMMENT: Anti-Spoofing verification failure */
	ETH_PACKET_SIZE_TOO_LARGE,	/* HSI_COMMENT: packet scanned is too large (can be 9700 at most) */
	CORE_ILLEGAL_VLAN_MODE,	/* HSI_COMMENT: Tx packet with marked as insert VLAN when its illegal */
	CORE_ILLEGAL_NBDS,	/* HSI_COMMENT: indicated number of BDs for the packet is illegal */
	CORE_FIRST_BD_WO_SOP,	/* HSI_COMMENT: 1st BD must have start_bd flag set */
	CORE_INSUFFICIENT_BDS,	/* HSI_COMMENT: There are not enough BDs for transmission of even one packet */
	CORE_PACKET_TOO_SMALL,	/* HSI_COMMENT: TX packet is shorter then reported on BDs or from minimal size */
	CORE_ILLEGAL_INBAND_TAGS,	/* HSI_COMMENT: TX packet has illegal inband tags marked */
	CORE_VLAN_INSERT_AND_INBAND_VLAN,	/* HSI_COMMENT: Vlan cant be added to inband tag */
	CORE_MTU_VIOLATION,	/* HSI_COMMENT: TX packet is greater then MTU */
	CORE_CONTROL_PACKET_VIOLATION,	/* HSI_COMMENT: VF sent control frame such as PFC */
	CORE_ANTI_SPOOFING_ERR,	/* HSI_COMMENT: Anti-Spoofing verification failure */
	CORE_PACKET_SIZE_TOO_LARGE,	/* HSI_COMMENT: packet scanned is too large (can be 9700 at most) */
	CORE_ILLEGAL_BD_FLAGS,	/* HSI_COMMENT: TX packet has illegal BD flags. */
	CORE_GSI_PACKET_VIOLATION,	/* HSI_COMMENT: TX packet GSI validation fail */
	MAX_FUNC_ERR_ID
};

/* FW error handling mode */
enum fw_err_mode {
	FW_ERR_FATAL_ASSERT,	/* HSI_COMMENT: Fatal assertion */
	FW_ERR_DRV_REPORT,	/* HSI_COMMENT: Report to the driver + warning assertion */
	MAX_FW_ERR_MODE
};

/* FW error recovery scope */
enum fw_err_recovery_scope {
	ERR_SCOPE_INVALID,	/* HSI_COMMENT: Error recovery scope is not set */
	ERR_SCOPE_TX_Q,		/* HSI_COMMENT: Error recovery scope - L2 TX queue */
	ERR_SCOPE_RX_Q,		/* HSI_COMMENT: Error recovery scope - L2 RX queue */
	ERR_SCOPE_QP,		/* HSI_COMMENT: Error recovery scope - RDMA QP */
	ERR_SCOPE_VPORT,	/* HSI_COMMENT: Error recovery scope - Vport */
	ERR_SCOPE_FUNC,		/* HSI_COMMENT: Error recovery scope - Function */
	ERR_SCOPE_PORT,		/* HSI_COMMENT: Error recovery scope - Port */
	ERR_SCOPE_ENGINE,	/* HSI_COMMENT: Error recovery scope - Engine */
	MAX_FW_ERR_RECOVERY_SCOPE
};

/* Ports mode */
enum fw_flow_ctrl_mode {
	flow_ctrl_pause,
	flow_ctrl_pfc,
	MAX_FW_FLOW_CTRL_MODE
};

/* GFT profile type. */
enum gft_profile_type {
	GFT_PROFILE_TYPE_4_TUPLE,	/* HSI_COMMENT: tunnel type, inner 4 tuple, IP type and L4 type match. */
	GFT_PROFILE_TYPE_L4_DST_PORT,	/* HSI_COMMENT: tunnel type, inner L4 destination port, IP type and L4 type match. */
	GFT_PROFILE_TYPE_IP_DST_ADDR,	/* HSI_COMMENT: tunnel type, inner IP destination address and IP type match. */
	GFT_PROFILE_TYPE_IP_SRC_ADDR,	/* HSI_COMMENT: tunnel type, inner IP source address and IP type match. */
	GFT_PROFILE_TYPE_TUNNEL_TYPE,	/* HSI_COMMENT: tunnel type and outer IP type match. */
	GFT_PROFILE_TYPE_MAC_VLAN_L4_DST,	/* HSI_COMMENT: tunnel type, MAC, VLAN, inner L4 destination port and L4 type match. */
	GFT_PROFILE_TYPE_VLAN_L4_DST,	/* HSI_COMMENT: tunnel type, VLAN, inner L4 destination port and L4 type match. */
	MAX_GFT_PROFILE_TYPE
};

/* Major and Minor hsi Versions */
struct hsi_fp_ver_struct {
	u8 minor_ver_arr[2];	/* HSI_COMMENT: Minor Version of hsi loading pf */
	u8 major_ver_arr[2];	/* HSI_COMMENT: Major Version of driver loading pf */
};

/* Integration Phase */
enum integ_phase {
	INTEG_PHASE_BB_A0_LATEST = 3,	/* HSI_COMMENT: BB A0 latest integration phase */
	INTEG_PHASE_BB_B0_NO_MCP = 10,	/* HSI_COMMENT: BB B0 without MCP */
	INTEG_PHASE_BB_B0_WITH_MCP = 11,	/* HSI_COMMENT: BB B0 with MCP */
	MAX_INTEG_PHASE
};

/* Ports mode */
enum iwarp_ll2_tx_queues {
	IWARP_LL2_IN_ORDER_TX_QUEUE = 1,	/* HSI_COMMENT: LL2 queue for OOO packets sent in-order by the driver */
	IWARP_LL2_ALIGNED_TX_QUEUE,	/* HSI_COMMENT: LL2 queue for unaligned packets sent aligned by the driver */
	IWARP_LL2_ALIGNED_RIGHT_TRIMMED_TX_QUEUE,	/* HSI_COMMENT: LL2 queue for unaligned packets sent aligned and was right-trimmed by the driver */
	IWARP_LL2_ERROR,	/* HSI_COMMENT: Error indication */
	MAX_IWARP_LL2_TX_QUEUES
};

/* Mstorm non-triggering VF zone */
struct mstorm_non_trigger_vf_zone {
	struct eth_mstorm_per_queue_stat eth_queue_stat;	/* HSI_COMMENT: VF statistic bucket */
	struct eth_rx_prod_data eth_rx_queue_producers[ETH_MAX_NUM_RX_QUEUES_PER_VF_QUAD];	/* HSI_COMMENT: VF RX queues producers */
};

/* Mstorm VF zone */
struct mstorm_vf_zone {
	struct mstorm_non_trigger_vf_zone non_trigger;	/* HSI_COMMENT: non-interrupt-triggering zone */
};

/* vlan header including TPID and TCI fields */
struct vlan_header {
	__le16 tpid;		/* HSI_COMMENT: Tag Protocol Identifier */
	__le16 tci;		/* HSI_COMMENT: Tag Control Information */
};

/* outer tag configurations */
struct outer_tag_config_struct {
	u8 enable_stag_pri_change;	/* HSI_COMMENT: Enables updating S-tag priority from inner tag or DCB. Should be 1 for Bette Davis, UFP with Host Control mode, and UFP with DCB over base interface. Else - 0. */
	u8 pri_map_valid;	/* HSI_COMMENT: When set, inner_to_outer_pri_map will be used */
	u8 reserved[2];
	struct vlan_header outer_tag;	/* HSI_COMMENT: In case mf_mode is MF_OVLAN, this field specifies the outer Tag Protocol Identifier and outer Tag Control Information */
	u8 inner_to_outer_pri_map[8];	/* HSI_COMMENT: Map from inner to outer priority. Used if pri_map_valid is set */
};

/* personality per PF */
enum personality_type {
	BAD_PERSONALITY_TYP,
	PERSONALITY_ISCSI,	/* HSI_COMMENT: iSCSI and LL2 */
	PERSONALITY_FCOE,	/* HSI_COMMENT: FCoE and LL2 */
	PERSONALITY_RDMA_AND_ETH,	/* HSI_COMMENT: RoCE or IWARP, Ethernet and LL2 */
	PERSONALITY_RDMA,	/* HSI_COMMENT: RoCE and LL2 */
	PERSONALITY_CORE,	/* HSI_COMMENT: Core (LL2) */
	PERSONALITY_ETH,	/* HSI_COMMENT: Ethernet */
	PERSONALITY_TOE,	/* HSI_COMMENT: TOE and LL2 */
	MAX_PERSONALITY_TYPE
};

/* tunnel configuration */
struct pf_start_tunnel_config {
	u8 set_vxlan_udp_port_flg;	/* HSI_COMMENT: Set VXLAN tunnel UDP destination port to vxlan_udp_port. If not set - FW will use a default port */
	u8 set_geneve_udp_port_flg;	/* HSI_COMMENT: Set GENEVE tunnel UDP destination port to geneve_udp_port. If not set - FW will use a default port */
	u8 set_no_inner_l2_vxlan_udp_port_flg;	/* HSI_COMMENT: Set no-inner-L2 VXLAN tunnel UDP destination port to no_inner_l2_vxlan_udp_port. If not set - FW will use a default port */
	u8 tunnel_clss_vxlan;	/* HSI_COMMENT: Rx classification scheme for VXLAN tunnel. (use enum tunnel_clss) */
	u8 tunnel_clss_l2geneve;	/* HSI_COMMENT: Rx classification scheme for L2 GENEVE tunnel. (use enum tunnel_clss) */
	u8 tunnel_clss_ipgeneve;	/* HSI_COMMENT: Rx classification scheme for IP GENEVE tunnel. (use enum tunnel_clss) */
	u8 tunnel_clss_l2gre;	/* HSI_COMMENT: Rx classification scheme for L2 GRE tunnel. (use enum tunnel_clss) */
	u8 tunnel_clss_ipgre;	/* HSI_COMMENT: Rx classification scheme for IP GRE tunnel. (use enum tunnel_clss) */
	__le16 vxlan_udp_port;	/* HSI_COMMENT: VXLAN tunnel UDP destination port. Valid if set_vxlan_udp_port_flg=1 */
	__le16 geneve_udp_port;	/* HSI_COMMENT: GENEVE tunnel UDP destination port. Valid if set_geneve_udp_port_flg=1 */
	__le16 no_inner_l2_vxlan_udp_port;	/* HSI_COMMENT: no-inner-L2 VXLAN  tunnel UDP destination port. Valid if set_no_inner_l2_vxlan_udp_port_flg=1 */
	__le16 reserved[3];
};

/* Ramrod data for PF start ramrod */
struct pf_start_ramrod_data {
	struct regpair event_ring_pbl_addr;	/* HSI_COMMENT: Address of event ring PBL */
	struct regpair consolid_q_pbl_base_addr;	/* HSI_COMMENT: PBL address of consolidation queue */
	struct pf_start_tunnel_config tunnel_config;	/* HSI_COMMENT: tunnel configuration. */
	__le16 event_ring_sb_id;	/* HSI_COMMENT: Status block ID */
	u8 base_vf_id;		/* HSI_COMMENT: All Vf IDs owned by PF will start from baseVfId till baseVfId+numVfs */
	u8 num_vfs;		/* HSI_COMMENT: Number of VFs owned by PF */
	u8 event_ring_num_pages;	/* HSI_COMMENT: Number of PBL pages in event ring */
	u8 event_ring_sb_index;	/* HSI_COMMENT: Status block index */
	u8 path_id;		/* HSI_COMMENT: HW path ID (engine ID) */
	u8 warning_as_error;	/* HSI_COMMENT: In FW asserts, treat warning as error */
	u8 dont_log_ramrods;	/* HSI_COMMENT: If set, FW will not log ramrods */
	u8 personality;		/* HSI_COMMENT: PFs personality (use enum personality_type) */
	__le16 log_type_mask;	/* HSI_COMMENT: Log type mask. Each bit set enables a corresponding event type logging. Event types are defined as ASSERT_LOG_TYPE_xxx */
	u8 mf_mode;		/* HSI_COMMENT: Multi function mode (use enum mf_mode) */
	u8 integ_phase;		/* HSI_COMMENT: Integration phase (use enum integ_phase) */
	u8 allow_npar_tx_switching;	/* HSI_COMMENT: If set, inter-pf tx switching is allowed in NPAR mode */
	u8 reserved0;
	struct hsi_fp_ver_struct hsi_fp_ver;	/* HSI_COMMENT: FP HSI version to be used by FW */
	struct outer_tag_config_struct outer_tag_config;	/* HSI_COMMENT: Outer tag configurations */
	u8 pf_fp_err_mode;	/* HSI_COMMENT: PF fast-path error mode (use enum fw_err_mode) */
	u8 consolid_q_num_pages;	/* HSI_COMMENT: Number of PBL pages in event ring */
	u8 reserved[6];
};

/* Per protocol DCB data */
struct protocol_dcb_data {
	u8 dcb_enable_flag;	/* HSI_COMMENT: Enable DCB */
	u8 dscp_enable_flag;	/* HSI_COMMENT: Enable updating DSCP value */
	u8 dcb_priority;	/* HSI_COMMENT: DCB priority */
	u8 dcb_tc;		/* HSI_COMMENT: DCB TC */
	u8 dscp_val;		/* HSI_COMMENT: DSCP value to write if dscp_enable_flag is set */
	u8 dcb_dont_add_vlan0;	/* HSI_COMMENT: When DCB is enabled - if this flag is set, dont add VLAN 0 tag to untagged frames */
};

/* Update tunnel configuration */
struct pf_update_tunnel_config {
	u8 update_rx_pf_clss;	/* HSI_COMMENT: Update per-PF RX tunnel classification scheme. */
	u8 update_rx_def_ucast_clss;	/* HSI_COMMENT: Update per-PORT default tunnel RX classification scheme for traffic with unknown unicast outer MAC in NPAR mode. */
	u8 update_rx_def_non_ucast_clss;	/* HSI_COMMENT: Update per-PORT default tunnel RX classification scheme for traffic with non unicast outer MAC in NPAR mode. */
	u8 set_vxlan_udp_port_flg;	/* HSI_COMMENT: Update VXLAN tunnel UDP destination port. */
	u8 set_geneve_udp_port_flg;	/* HSI_COMMENT: Update GENEVE tunnel UDP destination port. */
	u8 set_no_inner_l2_vxlan_udp_port_flg;	/* HSI_COMMENT: Update no-inner-L2 VXLAN  tunnel UDP destination port. */
	u8 tunnel_clss_vxlan;	/* HSI_COMMENT: Classification scheme for VXLAN tunnel. (use enum tunnel_clss) */
	u8 tunnel_clss_l2geneve;	/* HSI_COMMENT: Classification scheme for L2 GENEVE tunnel. (use enum tunnel_clss) */
	u8 tunnel_clss_ipgeneve;	/* HSI_COMMENT: Classification scheme for IP GENEVE tunnel. (use enum tunnel_clss) */
	u8 tunnel_clss_l2gre;	/* HSI_COMMENT: Classification scheme for L2 GRE tunnel. (use enum tunnel_clss) */
	u8 tunnel_clss_ipgre;	/* HSI_COMMENT: Classification scheme for IP GRE tunnel. (use enum tunnel_clss) */
	u8 reserved;
	__le16 vxlan_udp_port;	/* HSI_COMMENT: VXLAN tunnel UDP destination port. */
	__le16 geneve_udp_port;	/* HSI_COMMENT: GENEVE tunnel UDP destination port. */
	__le16 no_inner_l2_vxlan_udp_port;	/* HSI_COMMENT: no-inner-L2 VXLAN  tunnel UDP destination port. */
	__le16 reserved1[3];
};

/* Data for port update ramrod */
struct pf_update_ramrod_data {
	u8 update_eth_dcb_data_mode;	/* HSI_COMMENT: If set - Update Eth DCB data (use enum dcb_dscp_update_mode) */
	u8 update_fcoe_dcb_data_mode;	/* HSI_COMMENT: If set - Update FCOE DCB data (use enum dcb_dscp_update_mode) */
	u8 update_iscsi_dcb_data_mode;	/* HSI_COMMENT: If set - Update iSCSI DCB data (use enum dcb_dscp_update_mode) */
	u8 update_roce_dcb_data_mode;	/* HSI_COMMENT: If set - Update ROCE DCB data (use enum dcb_dscp_update_mode) */
	u8 update_rroce_dcb_data_mode;	/* HSI_COMMENT: If set - Update RROCE (RoceV2) DCB data (use enum dcb_dscp_update_mode) */
	u8 update_iwarp_dcb_data_mode;	/* HSI_COMMENT: If set - Update IWARP DCB  data (use enum dcb_dscp_update_mode) */
	u8 update_mf_vlan_flag;	/* HSI_COMMENT: If set - Update MF Tag TCI */
	u8 update_enable_stag_pri_change;	/* HSI_COMMENT: If set - Update Enable STAG Priority Change */
	struct protocol_dcb_data eth_dcb_data;	/* HSI_COMMENT: eth  DCB data */
	struct protocol_dcb_data fcoe_dcb_data;	/* HSI_COMMENT: fcoe DCB data */
	struct protocol_dcb_data iscsi_dcb_data;	/* HSI_COMMENT: iscsi DCB data */
	struct protocol_dcb_data roce_dcb_data;	/* HSI_COMMENT: roce DCB data */
	struct protocol_dcb_data rroce_dcb_data;	/* HSI_COMMENT: roce DCB data */
	struct protocol_dcb_data iwarp_dcb_data;	/* HSI_COMMENT: iwarp DCB data */
	__le16 mf_vlan;		/* HSI_COMMENT: MF Tag TCI */
	u8 enable_stag_pri_change;	/* HSI_COMMENT: enables updating S-tag priority from inner tag or DCB. Should be 1 for Bette Davis, UFP with Host Control mode, and UFP with DCB over base interface. else - 0. */
	u8 reserved;
	struct pf_update_tunnel_config tunnel_config;	/* HSI_COMMENT: tunnel configuration. */
};

/* Ports mode */
enum ports_mode {
	ENGX2_PORTX1,		/* HSI_COMMENT: 2 engines x 1 port */
	ENGX2_PORTX2,		/* HSI_COMMENT: 2 engines x 2 ports */
	ENGX1_PORTX1,		/* HSI_COMMENT: 1 engine  x 1 port */
	ENGX1_PORTX2,		/* HSI_COMMENT: 1 engine  x 2 ports */
	ENGX1_PORTX4,		/* HSI_COMMENT: 1 engine  x 4 ports */
	MAX_PORTS_MODE
};

/* Protocol-common error code */
enum protocol_common_error_code {
	COMMON_ERR_CODE_OK = 0,	/* HSI_COMMENT: Command succeeded */
	COMMON_ERR_CODE_ERROR,	/* HSI_COMMENT: Command failed */
	MAX_PROTOCOL_COMMON_ERROR_CODE
};

/* use to index in hsi_fp_[major|minor]_ver_arr per protocol */
enum protocol_version_array_key {
	ETH_VER_KEY = 0,
	ROCE_VER_KEY,
	MAX_PROTOCOL_VERSION_ARRAY_KEY
};

/* RDMA TX Stats */
struct rdma_sent_stats {
	struct regpair sent_bytes;	/* HSI_COMMENT: number of total RDMA bytes sent */
	struct regpair sent_pkts;	/* HSI_COMMENT: number of total RDMA packets sent */
};

/* Pstorm non-triggering VF zone */
struct pstorm_non_trigger_vf_zone {
	struct eth_pstorm_per_queue_stat eth_queue_stat;	/* HSI_COMMENT: VF statistic bucket */
	struct rdma_sent_stats rdma_stats;	/* HSI_COMMENT: RoCE sent statistics */
};

/* Pstorm VF zone */
struct pstorm_vf_zone {
	struct pstorm_non_trigger_vf_zone non_trigger;	/* HSI_COMMENT: non-interrupt-triggering zone */
	struct regpair reserved[7];	/* HSI_COMMENT: vf_zone size mus be power of 2 */
};

/* Ramrod Command ID */
union ramrod_cmd_id {
	u8 raw;
	u8 common;		/* HSI_COMMENT:  (use enum common_ramrod_cmd_id) */
	u8 core;		/* HSI_COMMENT:  (use enum core_ramrod_cmd_id) */
	u8 eth;			/* HSI_COMMENT:  (use enum eth_ramrod_cmd_id) */
	u8 iscsi;		/* HSI_COMMENT:  (use enum iscsi_ramrod_cmd_id) */
	u8 toe;			/* HSI_COMMENT:  (use enum toe_ramrod_cmd_id) */
	u8 fcoe;		/* HSI_COMMENT:  (use enum fcoe_ramrod_cmd_id) */
	u8 rdma;		/* HSI_COMMENT:  (use enum rdma_ramrod_cmd_id) */
	u8 roce;		/* HSI_COMMENT:  (use enum roce_ramrod_cmd_id) */
	u8 iwarp;		/* HSI_COMMENT:  (use enum iwarp_ramrod_cmd_id) */
	u8 nvmf;		/* HSI_COMMENT:  (use enum nvmf_ramrod_cmd_id) */
};

/* Ramrod Header of SPQE */
struct ramrod_header {
	__le32 cid;		/* HSI_COMMENT: Slowpath Connection CID */
	union ramrod_cmd_id cmd_id;	/* HSI_COMMENT: Ramrod Cmd (Per Protocol Type) */
	u8 protocol_id;		/* HSI_COMMENT: Ramrod Protocol ID (use enum protocol_type) */
	__le16 echo;		/* HSI_COMMENT: Ramrod echo */
};

/* RDMA RX Stats */
struct rdma_rcv_stats {
	struct regpair rcv_bytes;	/* HSI_COMMENT: number of total RDMA bytes received */
	struct regpair rcv_pkts;	/* HSI_COMMENT: number of total RDMA packets received */
};

/* Data for update QCN/DCQCN RL ramrod */
struct rl_update_ramrod_data {
	u8 qcn_update_param_flg;	/* HSI_COMMENT: Update QCN global params: timeout. */
	u8 dcqcn_update_param_flg;	/* HSI_COMMENT: Update DCQCN global params: timeout, g, k. */
	u8 rl_init_flg;		/* HSI_COMMENT: Init RL parameters, when RL disabled. */
	u8 rl_start_flg;	/* HSI_COMMENT: Start RL in IDLE state. Set rate to maximum. */
	u8 rl_stop_flg;		/* HSI_COMMENT: Stop RL. */
	u8 rl_id_first;		/* HSI_COMMENT: ID of first or single RL, that will be updated. */
	u8 rl_id_last;		/* HSI_COMMENT: ID of last RL, that will be updated. If clear, single RL will updated. */
	u8 rl_dc_qcn_flg;	/* HSI_COMMENT: If set, RL will used for DCQCN. */
	u8 dcqcn_reset_alpha_on_idle;	/* HSI_COMMENT: If set, alpha will be reset to 1 when the state machine is idle. */
	u8 rl_bc_stage_th;	/* HSI_COMMENT: Byte counter threshold to change rate increase stage. */
	u8 rl_timer_stage_th;	/* HSI_COMMENT: Timer threshold to change rate increase stage. */
	u8 reserved1;
	__le32 rl_bc_rate;	/* HSI_COMMENT: Byte Counter Limit. */
	__le16 rl_max_rate;	/* HSI_COMMENT: Maximum rate in 1.6 Mbps resolution. */
	__le16 rl_r_ai;		/* HSI_COMMENT: Active increase rate. */
	__le16 rl_r_hai;	/* HSI_COMMENT: Hyper active increase rate. */
	__le16 dcqcn_g;		/* HSI_COMMENT: DCQCN Alpha update gain in 1/64K resolution . */
	__le32 dcqcn_k_us;	/* HSI_COMMENT: DCQCN Alpha update interval. */
	__le32 dcqcn_timeuot_us;	/* HSI_COMMENT: DCQCN timeout. */
	__le32 qcn_timeuot_us;	/* HSI_COMMENT: QCN timeout. */
	__le32 reserved2;
};

/* Slowpath Element (SPQE) */
struct slow_path_element {
	struct ramrod_header hdr;	/* HSI_COMMENT: Ramrod Header */
	struct regpair data_ptr;	/* HSI_COMMENT: Pointer to the Ramrod Data on the Host */
};

/* Tstorm non-triggering VF zone */
struct tstorm_non_trigger_vf_zone {
	struct rdma_rcv_stats rdma_stats;	/* HSI_COMMENT: RoCE received statistics */
};

struct tstorm_per_port_stat {
	struct regpair trunc_error_discard;	/* HSI_COMMENT: packet is dropped because it was truncated in NIG */
	struct regpair mac_error_discard;	/* HSI_COMMENT: packet is dropped because of Ethernet FCS error */
	struct regpair mftag_filter_discard;	/* HSI_COMMENT: packet is dropped because classification was unsuccessful */
	struct regpair eth_mac_filter_bcast_discard;	/* HSI_COMMENT: broadcast packet was passed to Ethernet and dropped because of no mac filter match */
	struct regpair eth_mac_filter_mcast_discard;	/* HSI_COMMENT: multicast packet was passed to Ethernet and dropped because of no mac filter match */
	struct regpair eth_mac_filter_ucast_discard;	/* HSI_COMMENT: unicast packet was passed to Ethernet and dropped because of no mac filter match */
	struct regpair ll2_mac_filter_discard;	/* HSI_COMMENT: packet passed to Light L2 and dropped because Light L2 is not configured for this PF */
	struct regpair ll2_conn_disabled_discard;	/* HSI_COMMENT: packet passed to Light L2 and dropped because Light L2 is not configured for this PF */
	struct regpair iscsi_irregular_pkt;	/* HSI_COMMENT: packet is an ISCSI irregular packet */
	struct regpair fcoe_irregular_pkt;	/* HSI_COMMENT: packet is an FCOE irregular packet */
	struct regpair roce_irregular_pkt;	/* HSI_COMMENT: packet is an ROCE irregular packet */
	struct regpair iwarp_irregular_pkt;	/* HSI_COMMENT: packet is an IWARP irregular packet */
	struct regpair eth_irregular_pkt;	/* HSI_COMMENT: packet is an ETH irregular packet */
	struct regpair toe_irregular_pkt;	/* HSI_COMMENT: packet is an TOE irregular packet */
	struct regpair preroce_irregular_pkt;	/* HSI_COMMENT: packet is an PREROCE irregular packet */
	struct regpair eth_gre_tunn_filter_discard;	/* HSI_COMMENT: GRE dropped packets */
	struct regpair eth_vxlan_tunn_filter_discard;	/* HSI_COMMENT: VXLAN dropped packets */
	struct regpair eth_geneve_tunn_filter_discard;	/* HSI_COMMENT: GENEVE dropped packets */
	struct regpair eth_gft_drop_pkt;	/* HSI_COMMENT: GFT dropped packets */
};

/* Tstorm VF zone */
struct tstorm_vf_zone {
	struct tstorm_non_trigger_vf_zone non_trigger;	/* HSI_COMMENT: non-interrupt-triggering zone */
};

/* Tunnel classification scheme */
enum tunnel_clss {
	TUNNEL_CLSS_MAC_VLAN = 0,	/* HSI_COMMENT: Use MAC and VLAN from outermost L2 header for vport classification. */
	TUNNEL_CLSS_MAC_VNI,	/* HSI_COMMENT: Use MAC from outermost L2 header and VNI from tunnel header for vport classification */
	TUNNEL_CLSS_INNER_MAC_VLAN,	/* HSI_COMMENT: Use MAC and VLAN from inner L2 header for vport classification */
	TUNNEL_CLSS_INNER_MAC_VNI,	/* HSI_COMMENT: Use MAC from inner L2 header and VNI from tunnel header for vport classification */
	TUNNEL_CLSS_MAC_VLAN_DUAL_STAGE,	/* HSI_COMMENT: Use MAC and VLAN from inner L2 header for vport classification. If no exact match, use MAC and VLAN from outermost L2 header for vport classification. */
	MAX_TUNNEL_CLSS
};

/* Ustorm non-triggering VF zone */
struct ustorm_non_trigger_vf_zone {
	struct eth_ustorm_per_queue_stat eth_queue_stat;	/* HSI_COMMENT: VF statistic bucket */
	struct regpair vf_pf_msg_addr;	/* HSI_COMMENT: VF-PF message address */
};

/* Ustorm triggering VF zone */
struct ustorm_trigger_vf_zone {
	u8 vf_pf_msg_valid;	/* HSI_COMMENT: VF-PF message valid flag */
	u8 reserved[7];
};

/* Ustorm VF zone */
struct ustorm_vf_zone {
	struct ustorm_non_trigger_vf_zone non_trigger;	/* HSI_COMMENT: non-interrupt-triggering zone */
	struct ustorm_trigger_vf_zone trigger;	/* HSI_COMMENT: interrupt triggering zone */
};

/* VF-PF channel data */
struct vf_pf_channel_data {
	__le32 ready;		/* HSI_COMMENT: 0: VF-PF Channel NOT ready. Waiting for ack from PF driver. 1: VF-PF Channel is ready for a new transaction. */
	u8 valid;		/* HSI_COMMENT: 0: VF-PF Channel is invalid because of malicious VF. 1: VF-PF Channel is valid. */
	u8 reserved0;
	__le16 reserved1;
};

/* Ramrod data for VF start ramrod */
struct vf_start_ramrod_data {
	u8 vf_id;		/* HSI_COMMENT: VF ID */
	u8 enable_flr_ack;	/* HSI_COMMENT: If set, initial cleanup ack will be sent to parent PF SP event queue */
	__le16 opaque_fid;	/* HSI_COMMENT: VF opaque FID */
	u8 personality;		/* HSI_COMMENT: VFs personality (use enum personality_type) */
	u8 reserved[7];
	struct hsi_fp_ver_struct hsi_fp_ver;	/* HSI_COMMENT: FP HSI version to be used by FW */
};

/* Ramrod data for VF start ramrod */
struct vf_stop_ramrod_data {
	u8 vf_id;		/* HSI_COMMENT: VF ID */
	u8 reserved0;
	__le16 reserved1;
	__le32 reserved2;
};

/* VF zone size mode. */
enum vf_zone_size_mode {
	VF_ZONE_SIZE_MODE_DEFAULT,	/* HSI_COMMENT: Default VF zone size. Up to 192 VF supported. */
	VF_ZONE_SIZE_MODE_DOUBLE,	/* HSI_COMMENT: Doubled VF zone size. Up to 96 VF supported. */
	VF_ZONE_SIZE_MODE_QUAD,	/* HSI_COMMENT: Quad VF zone size. Up to 48 VF supported. */
	MAX_VF_ZONE_SIZE_MODE
};

/* Xstorm non-triggering VF zone */
struct xstorm_non_trigger_vf_zone {
	struct regpair non_edpm_ack_pkts;	/* HSI_COMMENT: RoCE received statistics */
};

/* Tstorm VF zone */
struct xstorm_vf_zone {
	struct xstorm_non_trigger_vf_zone non_trigger;	/* HSI_COMMENT: non-interrupt-triggering zone */
};

/* Ystorm VF zone */
struct ystorm_vf_zone {
	struct regpair drv_reserved;	/* HSI_COMMENT: PF-VF communication  */
};

/* Attentions status block */
struct atten_status_block {
	__le32 atten_bits;
	__le32 atten_ack;
	__le16 reserved0;
	__le16 sb_index;	/* HSI_COMMENT: status block running index */
	__le32 reserved1;
};

/* DMAE command */
struct dmae_cmd {
	__le32 opcode;
#define DMAE_CMD_SRC_MASK			0x1	/* HSI_COMMENT: DMA Source. 0 - PCIe, 1 - GRC (use enum dmae_cmd_src_enum) */
#define DMAE_CMD_SRC_SHIFT			0
#define DMAE_CMD_DST_MASK			0x3	/* HSI_COMMENT: DMA destination. 0 - None, 1 - PCIe, 2 - GRC, 3 - None (use enum dmae_cmd_dst_enum) */
#define DMAE_CMD_DST_SHIFT			1
#define DMAE_CMD_C_DST_MASK			0x1	/* HSI_COMMENT: Completion destination. 0 - PCie, 1 - GRC (use enum dmae_cmd_c_dst_enum) */
#define DMAE_CMD_C_DST_SHIFT			3
#define DMAE_CMD_CRC_RESET_MASK			0x1	/* HSI_COMMENT: Reset the CRC result (do not use the previous result as the seed) */
#define DMAE_CMD_CRC_RESET_SHIFT		4
#define DMAE_CMD_SRC_ADDR_RESET_MASK		0x1	/* HSI_COMMENT: Reset the source address in the next go to the same source address of the previous go */
#define DMAE_CMD_SRC_ADDR_RESET_SHIFT		5
#define DMAE_CMD_DST_ADDR_RESET_MASK		0x1	/* HSI_COMMENT: Reset the destination address in the next go to the same destination address of the previous go */
#define DMAE_CMD_DST_ADDR_RESET_SHIFT		6
#define DMAE_CMD_COMP_FUNC_MASK			0x1	/* HSI_COMMENT: 0   completion function is the same as src function, 1 - 0   completion function is the same as dst function (use enum dmae_cmd_comp_func_enum) */
#define DMAE_CMD_COMP_FUNC_SHIFT		7
#define DMAE_CMD_COMP_WORD_EN_MASK		0x1	/* HSI_COMMENT: 0 - Do not write a completion word, 1 - Write a completion word (use enum dmae_cmd_comp_word_en_enum) */
#define DMAE_CMD_COMP_WORD_EN_SHIFT		8
#define DMAE_CMD_COMP_CRC_EN_MASK		0x1	/* HSI_COMMENT: 0 - Do not write a CRC word, 1 - Write a CRC word (use enum dmae_cmd_comp_crc_en_enum) */
#define DMAE_CMD_COMP_CRC_EN_SHIFT		9
#define DMAE_CMD_COMP_CRC_OFFSET_MASK		0x7	/* HSI_COMMENT: The CRC word should be taken from the DMAE address space from address 9+X, where X is the value in these bits. */
#define DMAE_CMD_COMP_CRC_OFFSET_SHIFT		10
#define DMAE_CMD_RESERVED1_MASK			0x1
#define DMAE_CMD_RESERVED1_SHIFT		13
#define DMAE_CMD_ENDIANITY_MODE_MASK		0x3
#define DMAE_CMD_ENDIANITY_MODE_SHIFT		14
#define DMAE_CMD_ERR_HANDLING_MASK		0x3	/* HSI_COMMENT: The field specifies how the completion word is affected by PCIe read error. 0   Send a regular completion, 1 - Send a completion with an error indication, 2   do not send a completion (use enum dmae_cmd_error_handling_enum) */
#define DMAE_CMD_ERR_HANDLING_SHIFT		16
#define DMAE_CMD_PORT_ID_MASK			0x3	/* HSI_COMMENT: The port ID to be placed on the  RF FID  field of the GRC bus. this field is used both when GRC is the destination and when it is the source of the DMAE transaction. */
#define DMAE_CMD_PORT_ID_SHIFT			18
#define DMAE_CMD_SRC_PF_ID_MASK			0xF	/* HSI_COMMENT: Source PCI function number [3:0] */
#define DMAE_CMD_SRC_PF_ID_SHIFT		20
#define DMAE_CMD_DST_PF_ID_MASK			0xF	/* HSI_COMMENT: Destination PCI function number [3:0] */
#define DMAE_CMD_DST_PF_ID_SHIFT		24
#define DMAE_CMD_SRC_VF_ID_VALID_MASK		0x1	/* HSI_COMMENT: Source VFID valid */
#define DMAE_CMD_SRC_VF_ID_VALID_SHIFT		28
#define DMAE_CMD_DST_VF_ID_VALID_MASK		0x1	/* HSI_COMMENT: Destination VFID valid */
#define DMAE_CMD_DST_VF_ID_VALID_SHIFT		29
#define DMAE_CMD_RESERVED2_MASK			0x3
#define DMAE_CMD_RESERVED2_SHIFT		30
	__le32 src_addr_lo;	/* HSI_COMMENT: PCIe source address low in bytes or GRC source address in DW */
	__le32 src_addr_hi;	/* HSI_COMMENT: PCIe source address high in bytes or reserved (if source is GRC) */
	__le32 dst_addr_lo;	/* HSI_COMMENT: PCIe destination address low in bytes or GRC destination address in DW */
	__le32 dst_addr_hi;	/* HSI_COMMENT: PCIe destination address high in bytes or reserved (if destination is GRC) */
	__le16 length_dw;	/* HSI_COMMENT: Length in DW */
	__le16 opcode_b;
#define DMAE_CMD_SRC_VF_ID_MASK		0xFF	/* HSI_COMMENT: Source VF id */
#define DMAE_CMD_SRC_VF_ID_SHIFT	0
#define DMAE_CMD_DST_VF_ID_MASK		0xFF	/* HSI_COMMENT: Destination VF id */
#define DMAE_CMD_DST_VF_ID_SHIFT	8
	__le32 comp_addr_lo;	/* HSI_COMMENT: PCIe completion address low in bytes or GRC completion address in DW */
	__le32 comp_addr_hi;	/* HSI_COMMENT: PCIe completion address high in bytes or reserved (if completion address is GRC) */
	__le32 comp_val;	/* HSI_COMMENT: Value to write to completion address */
	__le32 crc32;		/* HSI_COMMENT: crc16 result */
	__le32 crc_32_c;	/* HSI_COMMENT: crc32_c result */
	__le16 crc16;		/* HSI_COMMENT: crc16 result */
	__le16 crc16_c;		/* HSI_COMMENT: crc16_c result */
	__le16 crc10;		/* HSI_COMMENT: crc_t10 result */
	__le16 error_bit_reserved;
#define DMAE_CMD_ERROR_BIT_MASK		0x1	/* HSI_COMMENT: Error bit */
#define DMAE_CMD_ERROR_BIT_SHIFT	0
#define DMAE_CMD_RESERVED_MASK		0x7FFF
#define DMAE_CMD_RESERVED_SHIFT		1
	__le16 xsum16;		/* HSI_COMMENT: checksum16 result  */
	__le16 xsum8;		/* HSI_COMMENT: checksum8 result  */
};

enum dmae_cmd_comp_crc_en_enum {
	dmae_cmd_comp_crc_disabled,	/* HSI_COMMENT: Do not write a CRC word */
	dmae_cmd_comp_crc_enabled,	/* HSI_COMMENT: Write a CRC word */
	MAX_DMAE_CMD_COMP_CRC_EN_ENUM
};

enum dmae_cmd_comp_func_enum {
	dmae_cmd_comp_func_to_src,	/* HSI_COMMENT: completion word and/or CRC will be sent to SRC-PCI function/SRC VFID */
	dmae_cmd_comp_func_to_dst,	/* HSI_COMMENT: completion word and/or CRC will be sent to DST-PCI function/DST VFID */
	MAX_DMAE_CMD_COMP_FUNC_ENUM
};

enum dmae_cmd_comp_word_en_enum {
	dmae_cmd_comp_word_disabled,	/* HSI_COMMENT: Do not write a completion word */
	dmae_cmd_comp_word_enabled,	/* HSI_COMMENT: Write the completion word */
	MAX_DMAE_CMD_COMP_WORD_EN_ENUM
};

enum dmae_cmd_c_dst_enum {
	dmae_cmd_c_dst_pcie,
	dmae_cmd_c_dst_grc,
	MAX_DMAE_CMD_C_DST_ENUM
};

enum dmae_cmd_dst_enum {
	dmae_cmd_dst_none_0,
	dmae_cmd_dst_pcie,
	dmae_cmd_dst_grc,
	dmae_cmd_dst_none_3,
	MAX_DMAE_CMD_DST_ENUM
};

enum dmae_cmd_error_handling_enum {
	dmae_cmd_error_handling_send_regular_comp,	/* HSI_COMMENT: Send a regular completion (with no error indication) */
	dmae_cmd_error_handling_send_comp_with_err,	/* HSI_COMMENT: Send a completion with an error indication (i.e. set bit 31 of the completion word) */
	dmae_cmd_error_handling_dont_send_comp,	/* HSI_COMMENT: Do not send a completion */
	MAX_DMAE_CMD_ERROR_HANDLING_ENUM
};

enum dmae_cmd_src_enum {
	dmae_cmd_src_pcie,	/* HSI_COMMENT: The source is the PCIe */
	dmae_cmd_src_grc,	/* HSI_COMMENT: The source is the GRC */
	MAX_DMAE_CMD_SRC_ENUM
};

/* DMAE parameters */
struct dmae_params {
	__le32 flags;
#define DMAE_PARAMS_RW_REPL_SRC_MASK		0x1	/* HSI_COMMENT: If set and the source is a block of length DMAE_MAX_RW_SIZE and the destination is larger, the source block will be duplicated as many times as required to fill the destination block. This is used mostly to write a zeroed buffer to destination address using DMA */
#define DMAE_PARAMS_RW_REPL_SRC_SHIFT		0
#define DMAE_PARAMS_SRC_VF_VALID_MASK		0x1	/* HSI_COMMENT: If set, the source is a VF, and the source VF ID is taken from the src_vf_id parameter. */
#define DMAE_PARAMS_SRC_VF_VALID_SHIFT		1
#define DMAE_PARAMS_DST_VF_VALID_MASK		0x1	/* HSI_COMMENT: If set, the destination is a VF, and the destination VF ID is taken from the dst_vf_id parameter. */
#define DMAE_PARAMS_DST_VF_VALID_SHIFT		2
#define DMAE_PARAMS_COMPLETION_DST_MASK		0x1	/* HSI_COMMENT: If set, a completion is sent to the destination function. Otherwise its sent to the source function. */
#define DMAE_PARAMS_COMPLETION_DST_SHIFT	3
#define DMAE_PARAMS_PORT_VALID_MASK		0x1	/* HSI_COMMENT: If set, the port ID is taken from the port_id parameter. Otherwise, the current port ID is used. */
#define DMAE_PARAMS_PORT_VALID_SHIFT		4
#define DMAE_PARAMS_SRC_PF_VALID_MASK		0x1	/* HSI_COMMENT: If set, the source PF ID is taken from the src_pf_id parameter. Otherwise, the current PF ID is used. */
#define DMAE_PARAMS_SRC_PF_VALID_SHIFT		5
#define DMAE_PARAMS_DST_PF_VALID_MASK		0x1	/* HSI_COMMENT: If set, the destination PF ID is taken from the dst_pf_id parameter. Otherwise, the current PF ID is used. */
#define DMAE_PARAMS_DST_PF_VALID_SHIFT		6
#define DMAE_PARAMS_RESERVED_MASK		0x1FFFFFF
#define DMAE_PARAMS_RESERVED_SHIFT		7
	u8 src_vf_id;		/* HSI_COMMENT: Source VF ID, valid only if src_vf_valid is set */
	u8 dst_vf_id;		/* HSI_COMMENT: Destination VF ID, valid only if dst_vf_valid is set */
	u8 port_id;		/* HSI_COMMENT: Port ID, valid only if port_valid is set */
	u8 src_pf_id;		/* HSI_COMMENT: Source PF ID, valid only if src_pf_valid is set */
	u8 dst_pf_id;		/* HSI_COMMENT: Destination PF ID, valid only if dst_pf_valid is set */
	u8 reserved1;
	__le16 reserved2;
};

/* Info regarding the FW asserts section in the Storm RAM */
struct fw_asserts_ram_section {
	__le16 section_ram_line_offset;	/* HSI_COMMENT: The offset of the section in the RAM in RAM lines (64-bit units) */
	__le16 section_ram_line_size;	/* HSI_COMMENT: The size of the section in RAM lines (64-bit units) */
	u8 list_dword_offset;	/* HSI_COMMENT: The offset of the asserts list within the section in dwords */
	u8 list_element_dword_size;	/* HSI_COMMENT: The size of an assert list element in dwords */
	u8 list_num_elements;	/* HSI_COMMENT: The number of elements in the asserts list */
	u8 list_next_index_dword_offset;	/* HSI_COMMENT: The offset of the next list index field within the section in dwords */
};

/* FW version number - backward compatible structure! Dont change it! */
struct fw_ver_num {
	u8 major;		/* HSI_COMMENT: Firmware major version number */
	u8 minor;		/* HSI_COMMENT: Firmware minor version number */
	u8 rev;			/* HSI_COMMENT: Firmware revision version number */
	u8 eng;			/* HSI_COMMENT: Firmware engineering version number (for bootleg versions) */
};

/* FW version information - backward compatible structure! Dont change it (except for using reserved fields)! */
struct fw_ver_info {
	__le16 tools_ver;	/* HSI_COMMENT: Tools version number */
	u8 image_id;		/* HSI_COMMENT: FW image ID (e.g. main, l2b, kuku) */
	u8 reserved1;
	struct fw_ver_num num;	/* HSI_COMMENT: FW version number */
	__le32 timestamp;	/* HSI_COMMENT: FW Timestamp in unix time  (sec. since 1970) */
	__le32 reserved2;
};

/* FW information */
struct fw_info {
	struct fw_ver_info ver;	/* HSI_COMMENT: FW version information */
	struct fw_asserts_ram_section fw_asserts_section;	/* HSI_COMMENT: Info regarding the FW asserts section in the Storm RAM */
};

/* Information about the location of the fw_info structure. */
struct fw_info_location {
	__le32 grc_addr;	/* HSI_COMMENT: GRC address where the fw_info struct is located. */
	__le32 size;		/* HSI_COMMENT: Size of the fw_info structure (thats located at the grc_addr). */
};

/* IGU cleanup command */
struct igu_cleanup {
	__le32 sb_id_and_flags;
#define IGU_CLEANUP_RESERVED0_MASK		0x7FFFFFF
#define IGU_CLEANUP_RESERVED0_SHIFT		0
#define IGU_CLEANUP_CLEANUP_SET_MASK		0x1	/* HSI_COMMENT: cleanup clear - 0, set - 1 */
#define IGU_CLEANUP_CLEANUP_SET_SHIFT		27
#define IGU_CLEANUP_CLEANUP_TYPE_MASK		0x7
#define IGU_CLEANUP_CLEANUP_TYPE_SHIFT		28
#define IGU_CLEANUP_COMMAND_TYPE_MASK		0x1	/* HSI_COMMENT: must always be set (use enum command_type_bit) */
#define IGU_CLEANUP_COMMAND_TYPE_SHIFT		31
	__le32 reserved1;
};

/* IGU firmware driver command */
union igu_command {
	struct igu_prod_cons_update prod_cons_update;
	struct igu_cleanup cleanup;
};

/* IGU firmware driver command */
struct igu_command_reg_ctrl {
	__le16 opaque_fid;
	__le16 igu_command_reg_ctrl_fields;
#define IGU_COMMAND_REG_CTRL_PXP_BAR_ADDR_MASK		0xFFF
#define IGU_COMMAND_REG_CTRL_PXP_BAR_ADDR_SHIFT		0
#define IGU_COMMAND_REG_CTRL_RESERVED_MASK		0x7
#define IGU_COMMAND_REG_CTRL_RESERVED_SHIFT		12
#define IGU_COMMAND_REG_CTRL_COMMAND_TYPE_MASK		0x1	/* HSI_COMMENT: command typ: 0 - read, 1 - write */
#define IGU_COMMAND_REG_CTRL_COMMAND_TYPE_SHIFT		15
};

/* IGU mapping line structure */
struct igu_mapping_line {
	__le32 igu_mapping_line_fields;
#define IGU_MAPPING_LINE_VALID_MASK			0x1
#define IGU_MAPPING_LINE_VALID_SHIFT			0
#define IGU_MAPPING_LINE_VECTOR_NUMBER_MASK		0xFF
#define IGU_MAPPING_LINE_VECTOR_NUMBER_SHIFT		1
#define IGU_MAPPING_LINE_FUNCTION_NUMBER_MASK		0xFF	/* HSI_COMMENT: In BB: VF-0-120, PF-0-7; In K2: VF-0-191, PF-0-15 */
#define IGU_MAPPING_LINE_FUNCTION_NUMBER_SHIFT		9
#define IGU_MAPPING_LINE_PF_VALID_MASK			0x1	/* HSI_COMMENT: PF-1, VF-0 */
#define IGU_MAPPING_LINE_PF_VALID_SHIFT			17
#define IGU_MAPPING_LINE_IPS_GROUP_MASK			0x3F
#define IGU_MAPPING_LINE_IPS_GROUP_SHIFT		18
#define IGU_MAPPING_LINE_RESERVED_MASK			0xFF
#define IGU_MAPPING_LINE_RESERVED_SHIFT			24
};

/* IGU MSIX line structure */
struct igu_msix_vector {
	struct regpair address;
	__le32 data;
	__le32 msix_vector_fields;
#define IGU_MSIX_VECTOR_MASK_BIT_MASK		0x1
#define IGU_MSIX_VECTOR_MASK_BIT_SHIFT		0
#define IGU_MSIX_VECTOR_RESERVED0_MASK		0x7FFF
#define IGU_MSIX_VECTOR_RESERVED0_SHIFT		1
#define IGU_MSIX_VECTOR_STEERING_TAG_MASK	0xFF
#define IGU_MSIX_VECTOR_STEERING_TAG_SHIFT	16
#define IGU_MSIX_VECTOR_RESERVED1_MASK		0xFF
#define IGU_MSIX_VECTOR_RESERVED1_SHIFT		24
};

struct mstorm_core_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define MSTORM_CORE_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define MSTORM_CORE_CONN_AG_CTX_BIT0_SHIFT		0
#define MSTORM_CORE_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define MSTORM_CORE_CONN_AG_CTX_BIT1_SHIFT		1
#define MSTORM_CORE_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: cf0 */
#define MSTORM_CORE_CONN_AG_CTX_CF0_SHIFT		2
#define MSTORM_CORE_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: cf1 */
#define MSTORM_CORE_CONN_AG_CTX_CF1_SHIFT		4
#define MSTORM_CORE_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: cf2 */
#define MSTORM_CORE_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define MSTORM_CORE_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define MSTORM_CORE_CONN_AG_CTX_CF0EN_SHIFT		0
#define MSTORM_CORE_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define MSTORM_CORE_CONN_AG_CTX_CF1EN_SHIFT		1
#define MSTORM_CORE_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define MSTORM_CORE_CONN_AG_CTX_CF2EN_SHIFT		2
#define MSTORM_CORE_CONN_AG_CTX_RULE0EN_MASK		0x1	/* HSI_COMMENT: rule0en */
#define MSTORM_CORE_CONN_AG_CTX_RULE0EN_SHIFT		3
#define MSTORM_CORE_CONN_AG_CTX_RULE1EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define MSTORM_CORE_CONN_AG_CTX_RULE1EN_SHIFT		4
#define MSTORM_CORE_CONN_AG_CTX_RULE2EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define MSTORM_CORE_CONN_AG_CTX_RULE2EN_SHIFT		5
#define MSTORM_CORE_CONN_AG_CTX_RULE3EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define MSTORM_CORE_CONN_AG_CTX_RULE3EN_SHIFT		6
#define MSTORM_CORE_CONN_AG_CTX_RULE4EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define MSTORM_CORE_CONN_AG_CTX_RULE4EN_SHIFT		7
	__le16 word0;		/* HSI_COMMENT: word0 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
};

/* per encapsulation type enabling flags */
struct prs_encapsulation_type_en_flags {
	u8 flags;
#define PRS_ENCAPSULATION_TYPE_EN_FLAGS_ETH_OVER_GRE_ENABLE_MASK	0x1	/* HSI_COMMENT: Enable bit for Ethernet-over-GRE (L2 GRE) encapsulation. */
#define PRS_ENCAPSULATION_TYPE_EN_FLAGS_ETH_OVER_GRE_ENABLE_SHIFT	0
#define PRS_ENCAPSULATION_TYPE_EN_FLAGS_IP_OVER_GRE_ENABLE_MASK		0x1	/* HSI_COMMENT: Enable bit for IP-over-GRE (IP GRE) encapsulation. */
#define PRS_ENCAPSULATION_TYPE_EN_FLAGS_IP_OVER_GRE_ENABLE_SHIFT	1
#define PRS_ENCAPSULATION_TYPE_EN_FLAGS_VXLAN_ENABLE_MASK		0x1	/* HSI_COMMENT: Enable bit for VXLAN encapsulation. */
#define PRS_ENCAPSULATION_TYPE_EN_FLAGS_VXLAN_ENABLE_SHIFT		2
#define PRS_ENCAPSULATION_TYPE_EN_FLAGS_T_TAG_ENABLE_MASK		0x1	/* HSI_COMMENT: Enable bit for T-Tag encapsulation. */
#define PRS_ENCAPSULATION_TYPE_EN_FLAGS_T_TAG_ENABLE_SHIFT		3
#define PRS_ENCAPSULATION_TYPE_EN_FLAGS_ETH_OVER_GENEVE_ENABLE_MASK	0x1	/* HSI_COMMENT: Enable bit for Ethernet-over-GENEVE (L2 GENEVE) encapsulation. */
#define PRS_ENCAPSULATION_TYPE_EN_FLAGS_ETH_OVER_GENEVE_ENABLE_SHIFT	4
#define PRS_ENCAPSULATION_TYPE_EN_FLAGS_IP_OVER_GENEVE_ENABLE_MASK	0x1	/* HSI_COMMENT: Enable bit for IP-over-GENEVE (IP GENEVE) encapsulation. */
#define PRS_ENCAPSULATION_TYPE_EN_FLAGS_IP_OVER_GENEVE_ENABLE_SHIFT	5
#define PRS_ENCAPSULATION_TYPE_EN_FLAGS_RESERVED_MASK			0x3
#define PRS_ENCAPSULATION_TYPE_EN_FLAGS_RESERVED_SHIFT			6
};

/* TPH Steering Tag Hint */
enum pxp_tph_st_hint {
	TPH_ST_HINT_BIDIR,	/* HSI_COMMENT: Read/Write access by Host and Device */
	TPH_ST_HINT_REQUESTER,	/* HSI_COMMENT: Read/Write access by Device */
	TPH_ST_HINT_TARGET,	/* HSI_COMMENT: Device Write and Host Read, or Host Write and Device Read */
	TPH_ST_HINT_TARGET_PRIO,	/* HSI_COMMENT: Device Write and Host Read, or Host Write and Device Read - with temporal reuse */
	MAX_PXP_TPH_ST_HINT
};

/* QM hardware structure of enable bypass credit mask */
struct qm_rf_bypass_mask {
	u8 flags;
#define QM_RF_BYPASS_MASK_LINEVOQ_MASK		0x1
#define QM_RF_BYPASS_MASK_LINEVOQ_SHIFT		0
#define QM_RF_BYPASS_MASK_RESERVED0_MASK	0x1
#define QM_RF_BYPASS_MASK_RESERVED0_SHIFT	1
#define QM_RF_BYPASS_MASK_PFWFQ_MASK		0x1
#define QM_RF_BYPASS_MASK_PFWFQ_SHIFT		2
#define QM_RF_BYPASS_MASK_VPWFQ_MASK		0x1
#define QM_RF_BYPASS_MASK_VPWFQ_SHIFT		3
#define QM_RF_BYPASS_MASK_PFRL_MASK		0x1
#define QM_RF_BYPASS_MASK_PFRL_SHIFT		4
#define QM_RF_BYPASS_MASK_VPQCNRL_MASK		0x1
#define QM_RF_BYPASS_MASK_VPQCNRL_SHIFT		5
#define QM_RF_BYPASS_MASK_FWPAUSE_MASK		0x1
#define QM_RF_BYPASS_MASK_FWPAUSE_SHIFT		6
#define QM_RF_BYPASS_MASK_RESERVED1_MASK	0x1
#define QM_RF_BYPASS_MASK_RESERVED1_SHIFT	7
};

/* QM hardware structure of opportunistic credit mask */
struct qm_rf_opportunistic_mask {
	__le16 flags;
#define QM_RF_OPPORTUNISTIC_MASK_LINEVOQ_MASK		0x1
#define QM_RF_OPPORTUNISTIC_MASK_LINEVOQ_SHIFT		0
#define QM_RF_OPPORTUNISTIC_MASK_BYTEVOQ_MASK		0x1
#define QM_RF_OPPORTUNISTIC_MASK_BYTEVOQ_SHIFT		1
#define QM_RF_OPPORTUNISTIC_MASK_PFWFQ_MASK		0x1
#define QM_RF_OPPORTUNISTIC_MASK_PFWFQ_SHIFT		2
#define QM_RF_OPPORTUNISTIC_MASK_VPWFQ_MASK		0x1
#define QM_RF_OPPORTUNISTIC_MASK_VPWFQ_SHIFT		3
#define QM_RF_OPPORTUNISTIC_MASK_PFRL_MASK		0x1
#define QM_RF_OPPORTUNISTIC_MASK_PFRL_SHIFT		4
#define QM_RF_OPPORTUNISTIC_MASK_VPQCNRL_MASK		0x1
#define QM_RF_OPPORTUNISTIC_MASK_VPQCNRL_SHIFT		5
#define QM_RF_OPPORTUNISTIC_MASK_FWPAUSE_MASK		0x1
#define QM_RF_OPPORTUNISTIC_MASK_FWPAUSE_SHIFT		6
#define QM_RF_OPPORTUNISTIC_MASK_RESERVED0_MASK		0x1
#define QM_RF_OPPORTUNISTIC_MASK_RESERVED0_SHIFT	7
#define QM_RF_OPPORTUNISTIC_MASK_QUEUEEMPTY_MASK	0x1
#define QM_RF_OPPORTUNISTIC_MASK_QUEUEEMPTY_SHIFT	8
#define QM_RF_OPPORTUNISTIC_MASK_RESERVED1_MASK		0x7F
#define QM_RF_OPPORTUNISTIC_MASK_RESERVED1_SHIFT	9
};

/* QM hardware structure of QM map memory */
struct qm_rf_pq_map {
	__le32 reg;
#define QM_RF_PQ_MAP_PQ_VALID_MASK		0x1	/* HSI_COMMENT: PQ active */
#define QM_RF_PQ_MAP_PQ_VALID_SHIFT		0
#define QM_RF_PQ_MAP_RL_ID_MASK			0xFF	/* HSI_COMMENT: RL ID */
#define QM_RF_PQ_MAP_RL_ID_SHIFT		1
#define QM_RF_PQ_MAP_VP_PQ_ID_MASK		0x1FF	/* HSI_COMMENT: the first PQ associated with the VPORT and VOQ of this PQ */
#define QM_RF_PQ_MAP_VP_PQ_ID_SHIFT		9
#define QM_RF_PQ_MAP_VOQ_MASK			0x1F	/* HSI_COMMENT: VOQ */
#define QM_RF_PQ_MAP_VOQ_SHIFT			18
#define QM_RF_PQ_MAP_WRR_WEIGHT_GROUP_MASK	0x3	/* HSI_COMMENT: WRR weight */
#define QM_RF_PQ_MAP_WRR_WEIGHT_GROUP_SHIFT	23
#define QM_RF_PQ_MAP_RL_VALID_MASK		0x1	/* HSI_COMMENT: RL active */
#define QM_RF_PQ_MAP_RL_VALID_SHIFT		25
#define QM_RF_PQ_MAP_RESERVED_MASK		0x3F
#define QM_RF_PQ_MAP_RESERVED_SHIFT		26
};

/* Completion params for aggregated interrupt completion */
struct sdm_agg_int_comp_params {
	__le16 params;
#define SDM_AGG_INT_COMP_PARAMS_AGG_INT_INDEX_MASK		0x3F	/* HSI_COMMENT: the number of aggregated interrupt, 0-31 */
#define SDM_AGG_INT_COMP_PARAMS_AGG_INT_INDEX_SHIFT		0
#define SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_ENABLE_MASK		0x1	/* HSI_COMMENT: 1 - set a bit in aggregated vector, 0 - dont set */
#define SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_ENABLE_SHIFT		6
#define SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_BIT_MASK		0x1FF	/* HSI_COMMENT: Number of bit in the aggregated vector, 0-279 (TBD) */
#define SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_BIT_SHIFT		7
};

/* SDM operation gen command (generate aggregative interrupt) */
struct sdm_op_gen {
	__le32 command;
#define SDM_OP_GEN_COMP_PARAM_MASK	0xFFFF	/* HSI_COMMENT: completion parameters 0-15 */
#define SDM_OP_GEN_COMP_PARAM_SHIFT	0
#define SDM_OP_GEN_COMP_TYPE_MASK	0xF	/* HSI_COMMENT: completion type 16-19 */
#define SDM_OP_GEN_COMP_TYPE_SHIFT	16
#define SDM_OP_GEN_RESERVED_MASK	0xFFF	/* HSI_COMMENT: reserved 20-31 */
#define SDM_OP_GEN_RESERVED_SHIFT	20
};

struct ystorm_core_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define YSTORM_CORE_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define YSTORM_CORE_CONN_AG_CTX_BIT0_SHIFT		0
#define YSTORM_CORE_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define YSTORM_CORE_CONN_AG_CTX_BIT1_SHIFT		1
#define YSTORM_CORE_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: cf0 */
#define YSTORM_CORE_CONN_AG_CTX_CF0_SHIFT		2
#define YSTORM_CORE_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: cf1 */
#define YSTORM_CORE_CONN_AG_CTX_CF1_SHIFT		4
#define YSTORM_CORE_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: cf2 */
#define YSTORM_CORE_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define YSTORM_CORE_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define YSTORM_CORE_CONN_AG_CTX_CF0EN_SHIFT		0
#define YSTORM_CORE_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define YSTORM_CORE_CONN_AG_CTX_CF1EN_SHIFT		1
#define YSTORM_CORE_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define YSTORM_CORE_CONN_AG_CTX_CF2EN_SHIFT		2
#define YSTORM_CORE_CONN_AG_CTX_RULE0EN_MASK		0x1	/* HSI_COMMENT: rule0en */
#define YSTORM_CORE_CONN_AG_CTX_RULE0EN_SHIFT		3
#define YSTORM_CORE_CONN_AG_CTX_RULE1EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define YSTORM_CORE_CONN_AG_CTX_RULE1EN_SHIFT		4
#define YSTORM_CORE_CONN_AG_CTX_RULE2EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define YSTORM_CORE_CONN_AG_CTX_RULE2EN_SHIFT		5
#define YSTORM_CORE_CONN_AG_CTX_RULE3EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define YSTORM_CORE_CONN_AG_CTX_RULE3EN_SHIFT		6
#define YSTORM_CORE_CONN_AG_CTX_RULE4EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define YSTORM_CORE_CONN_AG_CTX_RULE4EN_SHIFT		7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: word0 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le16 word4;		/* HSI_COMMENT: word4 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
};

#ifndef _HSI_FUNC_COMMON_H
#define _HSI_FUNC_COMMON_H

/* Physical memory descriptor */
struct phys_mem_desc {
	dma_addr_t phys_addr;
	void *virt_addr;
	u32 size;		/* In bytes */
};

/* Virtual memory descriptor */
struct virt_mem_desc {
	void *ptr;
	u32 size;		/* In bytes */
};

#endif

/********************************/
/* HSI Init Functions constants */
/********************************/

/* Number of VLAN priorities */
#define NUM_OF_VLAN_PRIORITIES	  8

/* Size of CRC8 lookup table */

/* GFS Context Command 0 enum */
enum gfs_cntx_cmd0 {
	e_cntx_cmd0_do_always,	/* HSI_COMMENT: do always */
	e_cntx_cmd0_if_red_not_met,	/* HSI_COMMENT: if redirection condition is not met */
	MAX_GFS_CNTX_CMD0
};

/* GFS Context Command 1 enum */
enum gfs_cntx_cmd1 {
	e_cntx_cmd1_disable,	/* HSI_COMMENT: disabled */
	e_cntx_cmd1_do_always,	/* HSI_COMMENT: do always */
	e_cntx_cmd1_do_if_red_met,	/* HSI_COMMENT: do if redirection condition is met */
	e_cntx_cmd1_do_if_samp_met,	/* HSI_COMMENT: do if  sampling  condition is met */
	MAX_GFS_CNTX_CMD1
};

/* GFS Context Command 2 enum */
enum gfs_cntx_cmd2 {
	e_cntx_cmd2_disable,	/* HSI_COMMENT: disabled */
	e_cntx_cmd2_cpy_single_vport_always,	/* HSI_COMMENT: copy to a single Vport always */
	e_cntx_cmd2_cpy_single_vport_cpy_met,	/* HSI_COMMENT: copy to a single Vport if copy condition is met */
	e_cntx_cmd2_cpy_mul_vport_always,	/* HSI_COMMENT: copy to multiple Vports always */
	e_cntx_cmd2_cpy_mul_vport_cpy_met,	/* HSI_COMMENT: copy to multiple Vports if copy condition is met */
	e_cntx_cmd2_do_if_samp_met,	/* HSI_COMMENT: do if  sampling  condition is met */
	MAX_GFS_CNTX_CMD2
};

/* GFS context Descriptot QREG (0) */
struct gfs_context_qreg0 {
	u32 bitfields0;
#define GFS_CONTEXT_QREG0_RESERVED_VALID_MASK			0x1	/* HSI_COMMENT: Reserved for valid bit in searcher database */
#define GFS_CONTEXT_QREG0_RESERVED_VALID_SHIFT			0
#define GFS_CONTEXT_QREG0_RESERVED_LAST_MASK			0x1	/* HSI_COMMENT: Reserved for last bit in searcher database */
#define GFS_CONTEXT_QREG0_RESERVED_LAST_SHIFT			1
#define GFS_CONTEXT_QREG0_FW_RESERVED0_MASK			0x1F
#define GFS_CONTEXT_QREG0_FW_RESERVED0_SHIFT			2
#define GFS_CONTEXT_QREG0_SKIP_LAST_SEARCH_MASK			0x1	/* HSI_COMMENT: Skips the last search in the bundle according to the logic descried is GFS spec */
#define GFS_CONTEXT_QREG0_SKIP_LAST_SEARCH_SHIFT		7
#define GFS_CONTEXT_QREG0_CONTEXT_TYPE_MASK			0x3	/* HSI_COMMENT: Always use leading. (Leading, Accumulative, both leading and accumulative) (use enum gfs_context_type) */
#define GFS_CONTEXT_QREG0_CONTEXT_TYPE_SHIFT			8
#define GFS_CONTEXT_QREG0_COMMAND0_COND_MASK			0x1	/* HSI_COMMENT: 0   do always , 1   if redirection condition is not met (use enum gfs_cntx_cmd0) */
#define GFS_CONTEXT_QREG0_COMMAND0_COND_SHIFT			10
#define GFS_CONTEXT_QREG0_COMMAND0_CHANGE_VPORT_MASK		0x1	/* HSI_COMMENT: if set, change the vport */
#define GFS_CONTEXT_QREG0_COMMAND0_CHANGE_VPORT_SHIFT		11
#define GFS_CONTEXT_QREG0_COMMAND0_FW_HINT_MASK			0x7	/* HSI_COMMENT: Substituted to  resolution  structure for FW use (use enum gfs_res_struct_dst_type) */
#define GFS_CONTEXT_QREG0_COMMAND0_FW_HINT_SHIFT		12
#define GFS_CONTEXT_QREG0_COMMAND0_LOCATION_MASK		0x7F	/* HSI_COMMENT: Command ID, 0   From the 3rd QREG of this context (QREG#2), 1-127   From canned Commands RAM */
#define GFS_CONTEXT_QREG0_COMMAND0_LOCATION_SHIFT		15
#define GFS_CONTEXT_QREG0_COMMAND0_VPORT_MASK			0xFF	/* HSI_COMMENT: Used if  ChangeVport  is set */
#define GFS_CONTEXT_QREG0_COMMAND0_VPORT_SHIFT			22
#define GFS_CONTEXT_QREG0_COMMAND1_COND_MASK			0x3	/* HSI_COMMENT: 0   disabled, 1   do always, 2   do if redirection condition is met, 3   do if  sampling  condition is met (use enum gfs_cntx_cmd1) */
#define GFS_CONTEXT_QREG0_COMMAND1_COND_SHIFT			30
	u32 priority;		/* HSI_COMMENT: Priority of the leading context */
	u32 bitfields1;
#define GFS_CONTEXT_QREG0_COMMAND2_COND_MASK			0x7	/* HSI_COMMENT: 0   disabled, 1   copy to a single Vport always ,2   copy to a single Vport if copy condition is met ,3   copy to multiple Vports always, 4   copy to multiple Vports if copy condition is met, 5   do if  sampling  condition is met (use enum gfs_cntx_cmd2) */
#define GFS_CONTEXT_QREG0_COMMAND2_COND_SHIFT			0
#define GFS_CONTEXT_QREG0_COMMAND1_CHANGE_VPORT_MASK		0x1	/* HSI_COMMENT: is set, change the vport */
#define GFS_CONTEXT_QREG0_COMMAND1_CHANGE_VPORT_SHIFT		3
#define GFS_CONTEXT_QREG0_COMMAND1_FW_HINT_MASK			0x7	/* HSI_COMMENT: Substituted to  resolution  structure for FW use (use enum gfs_res_struct_dst_type) */
#define GFS_CONTEXT_QREG0_COMMAND1_FW_HINT_SHIFT		4
#define GFS_CONTEXT_QREG0_COMMAND1_LOCATION_MASK		0x7F	/* HSI_COMMENT: 0   From the 3rd QREG of this context (QREG#2), 1-127   From canned Commands RAM */
#define GFS_CONTEXT_QREG0_COMMAND1_LOCATION_SHIFT		7
#define GFS_CONTEXT_QREG0_COMMAND1_VPORT_MASK			0xFF	/* HSI_COMMENT: Used if  ChangeVport  is set */
#define GFS_CONTEXT_QREG0_COMMAND1_VPORT_SHIFT			14
#define GFS_CONTEXT_QREG0_COPY_CONDITION_EN_MASK		0x3FF	/* HSI_COMMENT: Matched with 8 TCP flags and 2 additional bits supplied in the GFS header. */
#define GFS_CONTEXT_QREG0_COPY_CONDITION_EN_SHIFT		22
	u32 bitfields2;
#define GFS_CONTEXT_QREG0_COMMAND2_CHANGE_VPORT_MASK		0x1	/* HSI_COMMENT: is set, change the vport */
#define GFS_CONTEXT_QREG0_COMMAND2_CHANGE_VPORT_SHIFT		0
#define GFS_CONTEXT_QREG0_COMMAND2_FW_HINT_MASK			0x7	/* HSI_COMMENT: Substituted to  resolution  structure for FW use (use enum gfs_res_struct_dst_type) */
#define GFS_CONTEXT_QREG0_COMMAND2_FW_HINT_SHIFT		1
#define GFS_CONTEXT_QREG0_COMMAND2_LOCATION_MASK		0x7F	/* HSI_COMMENT: 0   From the 3rd QREG of this context (QREG#2), 1-127   From canned Commands RAM */
#define GFS_CONTEXT_QREG0_COMMAND2_LOCATION_SHIFT		4
#define GFS_CONTEXT_QREG0_COMMAND2_VPORT_MASK			0xFF	/* HSI_COMMENT: For copy to a single Vport   Vport number.For copy to multiple Vports: 0-31   index of RAM entry that contains the Vports bitmask, 0xFF   the bitmask is located in QREG#2 and QREG#3 */
#define GFS_CONTEXT_QREG0_COMMAND2_VPORT_SHIFT			11
#define GFS_CONTEXT_QREG0_REDIRECTION_CONDITION_EN_MASK		0xF	/* HSI_COMMENT: Matched with TTL==0 (bit 0), TTL==1 (bit 1) from eth_gfs_redirect_condition in action_data.redirect.condition, and 2 additional bits (2 and 3) supplied in the GFS header. */
#define GFS_CONTEXT_QREG0_REDIRECTION_CONDITION_EN_SHIFT	19
#define GFS_CONTEXT_QREG0_FW_RESERVED1_MASK			0x1FF
#define GFS_CONTEXT_QREG0_FW_RESERVED1_SHIFT			23
};

/* GFS context type */
enum gfs_context_type {
	e_gfs_cntx_lead,	/* HSI_COMMENT: leading context type */
	e_gfs_cntx_accum,	/* HSI_COMMENT: accumulative context type */
	e_gfs_cntx_lead_and_accum,	/* HSI_COMMENT: leading and accumulative context type */
	MAX_GFS_CONTEXT_TYPE
};

/* Modify tunnel GRE header data */
struct gfs_modify_tunnel_gre_header_data {
	u16 key_bits0to15;	/* HSI_COMMENT: Tunnel ID bits  0..15. */
	u16 key_bits16to31;	/* HSI_COMMENT: Tunnel ID bits 16..31. */
	u16 bitfields0;
#define GFS_MODIFY_TUNNEL_GRE_HEADER_DATA_UPDATE_CHECKSUM_MASK		0x1	/* HSI_COMMENT: Update GRE checksum if exists */
#define GFS_MODIFY_TUNNEL_GRE_HEADER_DATA_UPDATE_CHECKSUM_SHIFT		0
#define GFS_MODIFY_TUNNEL_GRE_HEADER_DATA_SET_VALID_MASK		0x1	/* HSI_COMMENT: Set Key */
#define GFS_MODIFY_TUNNEL_GRE_HEADER_DATA_SET_VALID_SHIFT		1
#define GFS_MODIFY_TUNNEL_GRE_HEADER_DATA_RESERVED_MASK			0x3FFF
#define GFS_MODIFY_TUNNEL_GRE_HEADER_DATA_RESERVED_SHIFT		2
};

/* Modify tunnel IPV4 header flags */
struct gfs_modify_tunnel_ipv4_header_flags {
	u16 bitfields0;
#define GFS_MODIFY_TUNNEL_IPV4_HEADER_FLAGS_FW_MODIFY_TUNN_IP_SRC_ADDR_MASK \
	        0x1		/* HSI_COMMENT: Modify tunnel source IP address. */
#define GFS_MODIFY_TUNNEL_IPV4_HEADER_FLAGS_FW_MODIFY_TUNN_IP_SRC_ADDR_SHIFT \
	        0
#define GFS_MODIFY_TUNNEL_IPV4_HEADER_FLAGS_FW_MODIFY_TUNN_IP_DST_ADDR_MASK \
	        0x1		/* HSI_COMMENT: Modify tunnel destination IP address. */
#define GFS_MODIFY_TUNNEL_IPV4_HEADER_FLAGS_FW_MODIFY_TUNN_IP_DST_ADDR_SHIFT \
	        1
#define GFS_MODIFY_TUNNEL_IPV4_HEADER_FLAGS_FW_SET_TUNN_IP_TTL_HOP_LIMIT_MASK \
	        0x1		/* HSI_COMMENT: Modify tunnel IP TTL or Hop Limit */
#define GFS_MODIFY_TUNNEL_IPV4_HEADER_FLAGS_FW_SET_TUNN_IP_TTL_HOP_LIMIT_SHIFT \
	        2
#define GFS_MODIFY_TUNNEL_IPV4_HEADER_FLAGS_FW_DEC_TUNN_IP_TTL_HOP_LIMIT_MASK \
	        0x1		/* HSI_COMMENT: Decrement tunnel IP TTL or Hop Limit */
#define GFS_MODIFY_TUNNEL_IPV4_HEADER_FLAGS_FW_DEC_TUNN_IP_TTL_HOP_LIMIT_SHIFT \
	        3
#define GFS_MODIFY_TUNNEL_IPV4_HEADER_FLAGS_FW_MODIFY_TUNN_IP_DSCP_MASK \
	        0x1		/* HSI_COMMENT: Modify tunnel IP DSCP */
#define GFS_MODIFY_TUNNEL_IPV4_HEADER_FLAGS_FW_MODIFY_TUNN_IP_DSCP_SHIFT \
	        4
#define GFS_MODIFY_TUNNEL_IPV4_HEADER_FLAGS_RESERVED_MASK \
	        0x7FF
#define GFS_MODIFY_TUNNEL_IPV4_HEADER_FLAGS_RESERVED_SHIFT \
	        5
};

/* Modify tunnel VXLAN header data */
struct gfs_modify_tunnel_vxlan_header_data {
	u16 vni_bits0to15;	/* HSI_COMMENT: Tunnel ID bits  0..15 */
	u8 vni_bits16to23;	/* HSI_COMMENT: Tunnel ID bits 16..23 */
	u8 bitfields0;
#define GFS_MODIFY_TUNNEL_VXLAN_HEADER_DATA_RESERVED0_MASK		0x7
#define GFS_MODIFY_TUNNEL_VXLAN_HEADER_DATA_RESERVED0_SHIFT		0
#define GFS_MODIFY_TUNNEL_VXLAN_HEADER_DATA_UPDATE_CHECKSUM_MASK	0x1	/* HSI_COMMENT: Update UDP checksum if exists */
#define GFS_MODIFY_TUNNEL_VXLAN_HEADER_DATA_UPDATE_CHECKSUM_SHIFT	3
#define GFS_MODIFY_TUNNEL_VXLAN_HEADER_DATA_SET_VNI_MASK		0x1	/* HSI_COMMENT: Set VNI */
#define GFS_MODIFY_TUNNEL_VXLAN_HEADER_DATA_SET_VNI_SHIFT		4
#define GFS_MODIFY_TUNNEL_VXLAN_HEADER_DATA_RESERVED1_MASK		0x1
#define GFS_MODIFY_TUNNEL_VXLAN_HEADER_DATA_RESERVED1_SHIFT		5
#define GFS_MODIFY_TUNNEL_VXLAN_HEADER_DATA_SET_ENTROPY_MASK		0x1	/* HSI_COMMENT: Set       UDP source port. */
#define GFS_MODIFY_TUNNEL_VXLAN_HEADER_DATA_SET_ENTROPY_SHIFT		6
#define GFS_MODIFY_TUNNEL_VXLAN_HEADER_DATA_CALC_ENTROPY_MASK		0x1	/* HSI_COMMENT: Calculate UDP source port from 4-tuple. */
#define GFS_MODIFY_TUNNEL_VXLAN_HEADER_DATA_CALC_ENTROPY_SHIFT		7
	u16 entropy;		/* HSI_COMMENT: VXLAN UDP source value. */
};

/* Push tunnel GRE header data */
struct gfs_push_tunnel_gre_header_data {
	u16 key_bits0to15;	/* HSI_COMMENT: Tunnel ID bits  0..15. */
	u16 key_bits16to31;	/* HSI_COMMENT: Tunnel ID bits 16..31. */
	u16 bitfields0;
#define GFS_PUSH_TUNNEL_GRE_HEADER_DATA_CHECKSUM_VALID_MASK	0x1	/* HSI_COMMENT: GRE checksum exists */
#define GFS_PUSH_TUNNEL_GRE_HEADER_DATA_CHECKSUM_VALID_SHIFT	0
#define GFS_PUSH_TUNNEL_GRE_HEADER_DATA_KEY_VALID_MASK		0x1	/* HSI_COMMENT: Key exists */
#define GFS_PUSH_TUNNEL_GRE_HEADER_DATA_KEY_VALID_SHIFT		1
#define GFS_PUSH_TUNNEL_GRE_HEADER_DATA_OOB_KEY_MASK		0x1	/* HSI_COMMENT: Use out of band KEY from RX path for hairpin traffic. */
#define GFS_PUSH_TUNNEL_GRE_HEADER_DATA_OOB_KEY_SHIFT		2
#define GFS_PUSH_TUNNEL_GRE_HEADER_DATA_RESERVED_MASK		0x1FFF
#define GFS_PUSH_TUNNEL_GRE_HEADER_DATA_RESERVED_SHIFT		3
};

/* Push tunnel IPV4 header flags */
struct gfs_push_tunnel_ipv4_header_flags {
	u16 bitfields0;
#define GFS_PUSH_TUNNEL_IPV4_HEADER_FLAGS_DONT_FRAG_FLAG_MASK \
	        0x1
#define GFS_PUSH_TUNNEL_IPV4_HEADER_FLAGS_DONT_FRAG_FLAG_SHIFT \
	        0
#define GFS_PUSH_TUNNEL_IPV4_HEADER_FLAGS_FW_TUNN_IPV4_ID_IND_INDEX_MASK \
	        0x1FF		/* HSI_COMMENT: Modify tunnel destination IP address. */
#define GFS_PUSH_TUNNEL_IPV4_HEADER_FLAGS_FW_TUNN_IPV4_ID_IND_INDEX_SHIFT \
	        1
#define GFS_PUSH_TUNNEL_IPV4_HEADER_FLAGS_RESERVED_MASK \
	        0x3F
#define GFS_PUSH_TUNNEL_IPV4_HEADER_FLAGS_RESERVED_SHIFT \
	        10
};

/* Push tunnel VXLAN header data */
struct gfs_push_tunnel_vxlan_header_data {
	u16 vni_bits0to15;	/* HSI_COMMENT: Tunnel ID bits  0..15 */
	u8 vni_bits16to23;	/* HSI_COMMENT: Tunnel ID bits 16..23 */
	u8 bitfields0;
#define GFS_PUSH_TUNNEL_VXLAN_HEADER_DATA_RESERVED0_MASK		0x7
#define GFS_PUSH_TUNNEL_VXLAN_HEADER_DATA_RESERVED0_SHIFT		0
#define GFS_PUSH_TUNNEL_VXLAN_HEADER_DATA_UDP_CHECKSUM_VALID_MASK	0x1	/* HSI_COMMENT: UDP checksum exists */
#define GFS_PUSH_TUNNEL_VXLAN_HEADER_DATA_UDP_CHECKSUM_VALID_SHIFT	3
#define GFS_PUSH_TUNNEL_VXLAN_HEADER_DATA_I_FRAG_MASK			0x1	/* HSI_COMMENT: VNI valid. */
#define GFS_PUSH_TUNNEL_VXLAN_HEADER_DATA_I_FRAG_SHIFT			4
#define GFS_PUSH_TUNNEL_VXLAN_HEADER_DATA_OOB_VNI_MASK			0x1	/* HSI_COMMENT: Use out of band VNI from RX path for hairpin traffic. */
#define GFS_PUSH_TUNNEL_VXLAN_HEADER_DATA_OOB_VNI_SHIFT			5
#define GFS_PUSH_TUNNEL_VXLAN_HEADER_DATA_RESERVED1_MASK		0x1
#define GFS_PUSH_TUNNEL_VXLAN_HEADER_DATA_RESERVED1_SHIFT		6
#define GFS_PUSH_TUNNEL_VXLAN_HEADER_DATA_CALC_ENTROPY_MASK		0x1	/* HSI_COMMENT: Calculate UDP source port from 4-tuple. */
#define GFS_PUSH_TUNNEL_VXLAN_HEADER_DATA_CALC_ENTROPY_SHIFT		7
	u16 entropy;		/* HSI_COMMENT: VXLAN UDP source value. */
};

enum gfs_res_struct_dst_type {
	e_gfs_dst_type_regular,	/* HSI_COMMENT: Perform regular TX-Switching classification  */
	e_gfs_dst_type_rx_vport,	/* HSI_COMMENT: Redirect to RX Vport */
	e_gfs_dst_type_tx_sw_bypass,	/* HSI_COMMENT: TX-Switching is bypassed, The TX destination is written in vport field in resolution struct */
	MAX_GFS_RES_STRUCT_DST_TYPE
};

/* GFS Tunnel Header Data union */
union gfs_tunnel_header_data {
	struct gfs_push_tunnel_vxlan_header_data
	 push_tunnel_vxlan_header_data;	/* HSI_COMMENT: Push   VXLAN tunnel header Data */
	struct gfs_modify_tunnel_vxlan_header_data
	 modify_tunnel_vxlan_header_data;	/* HSI_COMMENT: Modify VXLAN tunnel header Data */
	struct gfs_push_tunnel_gre_header_data
	 push_tunnel_gre_header_data;	/* HSI_COMMENT: Push   GRE   tunnel header Data */
	struct gfs_modify_tunnel_gre_header_data
	 modify_tunnel_gre_header_data;	/* HSI_COMMENT: Modify GRE   tunnel header Data */
};

/* GFS Tunnel Header Flags union */
union gfs_tunnel_header_flags {
	struct gfs_modify_tunnel_ipv4_header_flags
	 modify_tunnel_ipv4_header_flags;	/* HSI_COMMENT: Modify tunnel IPV4 header Flags */
	struct gfs_push_tunnel_ipv4_header_flags
	 push_tunnel_ipv4_header_flags;	/* HSI_COMMENT: Push tunnel IPV4 header Flags */
};

/* GFS Context VLAN mode enum */
enum gfs_vlan_mode {
	e_vlan_mode_nop,
	e_vlan_mode_pop,
	e_vlan_mode_push,
	e_vlan_mode_change_vid,
	e_vlan_mode_change_pri,
	e_vlan_mode_change_whole_tag,
	MAX_GFS_VLAN_MODE
};

/* RGFS context Data QREG 1 */
struct rgfs_context_data_qreg1 {
	u32 cid;		/* HSI_COMMENT: CID for replacement in CM header. Used for steering to TX queue. Only one copy can use CID. In TGFS used to change event ID   Select TGFS flow in PSTORM */
	u16 bitfields0;
#define RGFS_CONTEXT_DATA_QREG1_CONNECTION_TYPE_MASK			0xF	/* HSI_COMMENT: Connection Type for replacement in the CM header (Unused) */
#define RGFS_CONTEXT_DATA_QREG1_CONNECTION_TYPE_SHIFT			0
#define RGFS_CONTEXT_DATA_QREG1_CORE_AFFINITY_MASK			0x3	/* HSI_COMMENT: Core Affinity for replacement in the CM header (Unused) */
#define RGFS_CONTEXT_DATA_QREG1_CORE_AFFINITY_SHIFT			4
#define RGFS_CONTEXT_DATA_QREG1_CHANGE_DST_IP_ADDRESS_MASK		0x1	/* HSI_COMMENT: Change dst IP address that RSS header refers to. Set 1 to replace inner IPV4 address */
#define RGFS_CONTEXT_DATA_QREG1_CHANGE_DST_IP_ADDRESS_SHIFT		6
#define RGFS_CONTEXT_DATA_QREG1_CHANGE_SRC_IP_ADDRESS_MASK		0x1	/* HSI_COMMENT: Change src IP address that RSS header refers to. Set 1 to replace inner IPV4 address */
#define RGFS_CONTEXT_DATA_QREG1_CHANGE_SRC_IP_ADDRESS_SHIFT		7
#define RGFS_CONTEXT_DATA_QREG1_CHANGE_DST_PORT_MASK			0x1	/* HSI_COMMENT: Change dst port that RSS header refers to. Set 1 to replace inner UDP/TCP port */
#define RGFS_CONTEXT_DATA_QREG1_CHANGE_DST_PORT_SHIFT			8
#define RGFS_CONTEXT_DATA_QREG1_CHANGE_SRC_PORT_MASK			0x1	/* HSI_COMMENT: Change src port that RSS header refers to. Set 1 to replace inner UDP/TCP port */
#define RGFS_CONTEXT_DATA_QREG1_CHANGE_SRC_PORT_SHIFT			9
#define RGFS_CONTEXT_DATA_QREG1_CHANGE_RSS_IPVER_TYPE_MASK		0x1	/* HSI_COMMENT: Change IpVerType field in the RSS header. Set 0 */
#define RGFS_CONTEXT_DATA_QREG1_CHANGE_RSS_IPVER_TYPE_SHIFT		10
#define RGFS_CONTEXT_DATA_QREG1_CHANGE_RSS_L4_TYPE_MASK			0x1	/* HSI_COMMENT: Change L4Type field in the RSS header. Set 0 */
#define RGFS_CONTEXT_DATA_QREG1_CHANGE_RSS_L4_TYPE_SHIFT		11
#define RGFS_CONTEXT_DATA_QREG1_RSS_L4_TYPE_MASK			0x3	/* HSI_COMMENT: Unused */
#define RGFS_CONTEXT_DATA_QREG1_RSS_L4_TYPE_SHIFT			12
#define RGFS_CONTEXT_DATA_QREG1_CHANGE_RSS_OVER_IP_PROT_MASK		0x1	/* HSI_COMMENT: Change OVER_IP_PROTOCOL field in the RSS header. Set 0 */
#define RGFS_CONTEXT_DATA_QREG1_CHANGE_RSS_OVER_IP_PROT_SHIFT		14
#define RGFS_CONTEXT_DATA_QREG1_CHANGE_RSS_IPV4_FRAG_MASK		0x1	/* HSI_COMMENT: Change Ipv4Frag field in the RSS header. Set 0 */
#define RGFS_CONTEXT_DATA_QREG1_CHANGE_RSS_IPV4_FRAG_SHIFT		15
	u16 dst_port;		/* HSI_COMMENT: Dst port value for replacement. L4 port */
	u16 src_port;		/* HSI_COMMENT: Src port value for replacement. L4 port */
	u16 bitfields1;
#define RGFS_CONTEXT_DATA_QREG1_RSS_IPVER_TYPE_MASK			0x3	/* HSI_COMMENT: Unused */
#define RGFS_CONTEXT_DATA_QREG1_RSS_IPVER_TYPE_SHIFT			0
#define RGFS_CONTEXT_DATA_QREG1_RSS_IPV4_FRAG_MASK			0x1	/* HSI_COMMENT: Unused */
#define RGFS_CONTEXT_DATA_QREG1_RSS_IPV4_FRAG_SHIFT			2
#define RGFS_CONTEXT_DATA_QREG1_RSS_OVER_IP_PROT_MASK			0xFF	/* HSI_COMMENT: Unused */
#define RGFS_CONTEXT_DATA_QREG1_RSS_OVER_IP_PROT_SHIFT			3
#define RGFS_CONTEXT_DATA_QREG1_RESERVED_MASK				0x1F
#define RGFS_CONTEXT_DATA_QREG1_RESERVED_SHIFT				11
	u32 flow_id;		/* HSI_COMMENT: Flow Id. Used for TMLD aggregation. Always set. Replace last register in TM message. */
};

/* RGFS context Data QREG 2 */
struct rgfs_context_data_qreg2 {
	u32 src_ip;		/* HSI_COMMENT: HDR Modify   IPv4 source IP or IPv6 indirection index. 0 are invalid index. */
	u16 bitfields0;
#define RGFS_CONTEXT_DATA_QREG2_FW_POP_TYPE_MASK \
	        0x7
#define RGFS_CONTEXT_DATA_QREG2_FW_POP_TYPE_SHIFT \
	        0
#define RGFS_CONTEXT_DATA_QREG2_FW_TID0_VALID_MASK \
	        0x1		/* HSI_COMMENT: TID 0 Valid */
#define RGFS_CONTEXT_DATA_QREG2_FW_TID0_VALID_SHIFT \
	        3
#define RGFS_CONTEXT_DATA_QREG2_FW_TID1_VALID_MASK \
	        0x1		/* HSI_COMMENT: TID 1 Valid */
#define RGFS_CONTEXT_DATA_QREG2_FW_TID1_VALID_SHIFT \
	        4
#define RGFS_CONTEXT_DATA_QREG2_FW_FIRST_HDR_VLAN_MODE_MASK \
	        0x3		/* HSI_COMMENT: POP or PUSH CVLAN out of band (use enum gfs_vlan_mode) */
#define RGFS_CONTEXT_DATA_QREG2_FW_FIRST_HDR_VLAN_MODE_SHIFT \
	        5
#define                                                                                   \
	RGFS_CONTEXT_DATA_QREG2_RESERVED_DRV_HINT_TBD_WAS_FW_INNER_IPV4_ID_IND_INDEX_MASK \
	        0x1FF		/* HSI_COMMENT: Drv Hint (TBD). Was: Inner IPV4 ID indirection index. 0 are invalid index. */
#define                                                                                    \
	RGFS_CONTEXT_DATA_QREG2_RESERVED_DRV_HINT_TBD_WAS_FW_INNER_IPV4_ID_IND_INDEX_SHIFT \
	        7
	u16 fw_command1_sample_len;	/* HSI_COMMENT: 2nd Command sample length (1st command is command0) */
	u32 fw_tid0;		/* HSI_COMMENT: Counter 0 TID */
	u32 fw_tid1;		/* HSI_COMMENT: Counter 1 TID */
};

/* RGFS context Data QREG 3 */
struct rgfs_context_data_qreg3 {
	u32 dst_ip;		/* HSI_COMMENT: HDR Modify   inner IPv4 destination IP or IPv6 indirection index. 0 are invalid index. */
	u16 fw_first_hdr_vlan_val;	/* HSI_COMMENT: Tunnel VLAN value */
	u16 bitfields0;
#define RGFS_CONTEXT_DATA_QREG3_XXLOCK_CMD_MASK				0x7	/* HSI_COMMENT: CM HDR override - Clear for drop. No special allocation needed for drop fields. */
#define RGFS_CONTEXT_DATA_QREG3_XXLOCK_CMD_SHIFT			0
#define RGFS_CONTEXT_DATA_QREG3_FW_MODIFY_INNER_4TUPLE_MASK		0x1	/* HSI_COMMENT: Modify 4 tuple  */
#define RGFS_CONTEXT_DATA_QREG3_FW_MODIFY_INNER_4TUPLE_SHIFT		3
#define RGFS_CONTEXT_DATA_QREG3_FW_MODIFY_INNER_MAC_MASK		0x1	/* HSI_COMMENT: Modify inner/single ETH header */
#define RGFS_CONTEXT_DATA_QREG3_FW_MODIFY_INNER_MAC_SHIFT		4
#define RGFS_CONTEXT_DATA_QREG3_FW_MODIFY_INNER_IP_DSCP_MASK		0x1	/* HSI_COMMENT: Modify inner IP DSCP */
#define RGFS_CONTEXT_DATA_QREG3_FW_MODIFY_INNER_IP_DSCP_SHIFT		5
#define RGFS_CONTEXT_DATA_QREG3_FW_SET_INNER_IP_TTL_HOP_LIMIT_MASK	0x1	/* HSI_COMMENT: Modify inner IP TTL or Hop Limit */
#define RGFS_CONTEXT_DATA_QREG3_FW_SET_INNER_IP_TTL_HOP_LIMIT_SHIFT	6
#define RGFS_CONTEXT_DATA_QREG3_FW_DEC_INNER_IP_TTL_HOP_LIMIT_MASK	0x1	/* HSI_COMMENT: Decrement inner IP TTL or Hop Limit */
#define RGFS_CONTEXT_DATA_QREG3_FW_DEC_INNER_IP_TTL_HOP_LIMIT_SHIFT	7
#define RGFS_CONTEXT_DATA_QREG3_FW_INNER_VLAN_MODIFY_FLG_MASK		0x3	/* HSI_COMMENT: NOP, change VID, change PRI, change whole tag */
#define RGFS_CONTEXT_DATA_QREG3_FW_INNER_VLAN_MODIFY_FLG_SHIFT		8
#define RGFS_CONTEXT_DATA_QREG3_LOAD_ST_CTX_FLG_MASK			0x1	/* HSI_COMMENT: CM HDR override - Clear for drop. No special allocation needed for drop fields. */
#define RGFS_CONTEXT_DATA_QREG3_LOAD_ST_CTX_FLG_SHIFT			10
#define RGFS_CONTEXT_DATA_QREG3_FW_TUNNEL_VLAN_MODIFY_FLG_MASK		0x7	/* HSI_COMMENT: NOP, POP, PUSH, change VID, change PRI, change whole tag */
#define RGFS_CONTEXT_DATA_QREG3_FW_TUNNEL_VLAN_MODIFY_FLG_SHIFT		11
#define RGFS_CONTEXT_DATA_QREG3_RESERVED_MASK				0x3
#define RGFS_CONTEXT_DATA_QREG3_RESERVED_SHIFT				14
	u16 fw_command2_sample_len;	/* HSI_COMMENT: 3rd Command sample length (1st command is command0) */
	u16 fw_inner_vlan_val;	/* HSI_COMMENT: Inner VLAN value */
	u32 bitfields1;
#define RGFS_CONTEXT_DATA_QREG3_FW_INNER_IP_DSCP_MASK			0x3F	/* HSI_COMMENT: inner IP DSCP Set value */
#define RGFS_CONTEXT_DATA_QREG3_FW_INNER_IP_DSCP_SHIFT			0
#define RGFS_CONTEXT_DATA_QREG3_FW_INNER_SRC_MAC_IND_INDEX_MASK		0x1FF	/* HSI_COMMENT: Inner source      MAC indirection index. 0 are invalid index. */
#define RGFS_CONTEXT_DATA_QREG3_FW_INNER_SRC_MAC_IND_INDEX_SHIFT	6
#define RGFS_CONTEXT_DATA_QREG3_FW_INNER_DEST_MAC_IND_INDEX_MASK	0x1FF	/* HSI_COMMENT: Inner destination MAC indirection index. 0 are invalid index. */
#define RGFS_CONTEXT_DATA_QREG3_FW_INNER_DEST_MAC_IND_INDEX_SHIFT	15
#define RGFS_CONTEXT_DATA_QREG3_FW_INNER_IP_TTL_HOP_LIMIT_MASK		0xFF	/* HSI_COMMENT: inner IP TTL or Hop Limit Set value */
#define RGFS_CONTEXT_DATA_QREG3_FW_INNER_IP_TTL_HOP_LIMIT_SHIFT		24
};

/* RGFS context Data QREG 4 */
struct rgfs_context_data_qreg4 {
	u32 fw_tunn_src_ip;	/* HSI_COMMENT: HDR Modify   tunnel IPv4 source IP or IPv6 indirection index. Used by FW. */
	u32 flow_mark;		/* HSI_COMMENT: Filter marker. Always set. Replace per-pf config in basic block. Reported by CQE  */
	u32 fw_tunn_dest_ip;	/* HSI_COMMENT: HDR Modify   tunnel IPv4 destination IP or IPv6 indirection index. Used by FW. */
	u8 fw_tunn_ip_ttl_hop_limit;	/* HSI_COMMENT: Tunnel IP TTL or Hop Limit Set value */
	u8 reserved[3];
};

/* RGFS context Data QREG 5 */
struct rgfs_context_data_qreg5 {
	union gfs_tunnel_header_flags tunnel_ip_header_flags;	/* HSI_COMMENT: Tunnel IP tunnel header flag union. */
	u16 bitfields0;
#define RGFS_CONTEXT_DATA_QREG5_FW_TUNNEL_SRC_MAC_IND_INDEX_MASK	0x1FF	/* HSI_COMMENT: Tunnel source MAC indirection index. 0 are invalid index. */
#define RGFS_CONTEXT_DATA_QREG5_FW_TUNNEL_SRC_MAC_IND_INDEX_SHIFT	0
#define RGFS_CONTEXT_DATA_QREG5_FW_PUSH_TYPE_MASK			0xF	/* HSI_COMMENT: Push headers type. */
#define RGFS_CONTEXT_DATA_QREG5_FW_PUSH_TYPE_SHIFT			9
#define RGFS_CONTEXT_DATA_QREG5_CID_OVERWRITE_MASK			0x1	/* HSI_COMMENT: RSS HDR override - for CID steering. Disable CID override. */
#define RGFS_CONTEXT_DATA_QREG5_CID_OVERWRITE_SHIFT			13
#define RGFS_CONTEXT_DATA_QREG5_FW_TUNNEL_IP_ECN_MASK			0x3	/* HSI_COMMENT: Tunnel IP header flags for PUSH flow: 2 LSBits of tos field. */
#define RGFS_CONTEXT_DATA_QREG5_FW_TUNNEL_IP_ECN_SHIFT			14
	union gfs_tunnel_header_data tunnel_header_data;	/* HSI_COMMENT: VXLAN/GRE tunnel header union. */
	u16 bitfields1;
#define RGFS_CONTEXT_DATA_QREG5_RESERVED2_MASK				0x7F	/* HSI_COMMENT: Modify tunnel source IP address. */
#define RGFS_CONTEXT_DATA_QREG5_RESERVED2_SHIFT				0
#define RGFS_CONTEXT_DATA_QREG5_FW_TUNNEL_DEST_MAC_IND_INDEX_MASK	0x1FF	/* HSI_COMMENT: Tunnel destination MAC indirection index. 0 are invalid index. */
#define RGFS_CONTEXT_DATA_QREG5_FW_TUNNEL_DEST_MAC_IND_INDEX_SHIFT	7
	u32 bitfields2;
#define RGFS_CONTEXT_DATA_QREG5_KEY_ADDR_LDR_HEADER_REGION_MASK		0xFFF	/* HSI_COMMENT: RSS HDR override - for queue/CID steering. Only one copy can use queue ID. Limited by 256. Bits 9-11 can be used by context.   */
#define RGFS_CONTEXT_DATA_QREG5_KEY_ADDR_LDR_HEADER_REGION_SHIFT	0
#define RGFS_CONTEXT_DATA_QREG5_CALC_RSS_AND_IND_TBL_MASK		0xF	/* HSI_COMMENT: RSS HDR override - For steering. Only one copy can use queue ID. Limited by 1. Bits 1-3 can be used by context. * Cannot Move *  */
#define RGFS_CONTEXT_DATA_QREG5_CALC_RSS_AND_IND_TBL_SHIFT		12
#define RGFS_CONTEXT_DATA_QREG5_FW_TUNNEL_IP_DSCP_MASK			0x3F	/* HSI_COMMENT: Tunnel IP DSCP Set value */
#define RGFS_CONTEXT_DATA_QREG5_FW_TUNNEL_IP_DSCP_SHIFT			16
#define RGFS_CONTEXT_DATA_QREG5_DEFAULT_QUEUE_ID_MASK			0x3FF	/* HSI_COMMENT: RSS HDR override - For steering. Only one copy can use queue ID. Limited by 511. Bits 9 can be used by context. * Cannot Move *  */
#define RGFS_CONTEXT_DATA_QREG5_DEFAULT_QUEUE_ID_SHIFT			22
};

/* TGFS context Data QREG 1 */
struct tgfs_context_data_qreg1 {
	u16 fw_first_hdr_vlan_val;	/* HSI_COMMENT: Tunnel or single VLAN value */
	u16 bitfields0;
#define TGFS_CONTEXT_DATA_QREG1_FW_FIRST_HDR_VLAN_MODE_MASK	0x7	/* HSI_COMMENT: First ETH header VLAN modification: NOP, POP, PUSH, change VID, change PRI, change whole tag. Replace basic block register 8 (use enum gfs_vlan_mode) */
#define TGFS_CONTEXT_DATA_QREG1_FW_FIRST_HDR_VLAN_MODE_SHIFT	0
#define TGFS_CONTEXT_DATA_QREG1_FW_TID0_VALID_MASK		0x1	/* HSI_COMMENT: TID 0 Valid */
#define TGFS_CONTEXT_DATA_QREG1_FW_TID0_VALID_SHIFT		3
#define TGFS_CONTEXT_DATA_QREG1_FW_TID1_VALID_MASK		0x1	/* HSI_COMMENT: TID 1 Valid */
#define TGFS_CONTEXT_DATA_QREG1_FW_TID1_VALID_SHIFT		4
#define TGFS_CONTEXT_DATA_QREG1_RESERVED_MASK			0x7FF
#define TGFS_CONTEXT_DATA_QREG1_RESERVED_SHIFT			5
	u16 fw_command1_sample_len;	/* HSI_COMMENT: Command 2 sample length */
	u16 fw_command2_sample_len;	/* HSI_COMMENT: Command 3 sample length */
	u32 fw_tid0;		/* HSI_COMMENT: Counter TID. */
	u32 fw_tid1;		/* HSI_COMMENT: Counter TID. */
};

/* TGFS context Data QREG 2 */
struct tgfs_context_data_qreg2 {
	u32 src_ip;		/* HSI_COMMENT: Inner IPv4 source IP or IPv6 indirection index. */
	u32 dst_ip;		/* HSI_COMMENT: Inner IPv4 destination IP or IPv6 indirection index. */
	u16 dst_port;		/* HSI_COMMENT: L4 destination port. */
	u16 src_port;		/* HSI_COMMENT: L4 source port. */
	u32 bitfields;
#define TGFS_CONTEXT_DATA_QREG2_FW_INNER_SRC_MAC_IND_INDEX_MASK		0x1FF	/* HSI_COMMENT: Inner source MAC indirection index. 0 are invalid index. */
#define TGFS_CONTEXT_DATA_QREG2_FW_INNER_SRC_MAC_IND_INDEX_SHIFT	0
#define TGFS_CONTEXT_DATA_QREG2_FW_INNER_DEST_MAC_IND_INDEX_MASK	0x1FF	/* HSI_COMMENT: Inner destination MAC indirection index. 0 are invalid index. */
#define TGFS_CONTEXT_DATA_QREG2_FW_INNER_DEST_MAC_IND_INDEX_SHIFT	9
#define TGFS_CONTEXT_DATA_QREG2_FW_MODIFY_INNER_SRC_IP_ADDR_MASK	0x1	/* HSI_COMMENT: Modify inner/single source IP address. */
#define TGFS_CONTEXT_DATA_QREG2_FW_MODIFY_INNER_SRC_IP_ADDR_SHIFT	18
#define TGFS_CONTEXT_DATA_QREG2_FW_MODIFY_INNER_DEST_IP_ADDR_MASK	0x1	/* HSI_COMMENT: Modify inner/single destination IP address. */
#define TGFS_CONTEXT_DATA_QREG2_FW_MODIFY_INNER_DEST_IP_ADDR_SHIFT	19
#define TGFS_CONTEXT_DATA_QREG2_FW_MODIFY_INNER_L4_SRC_PORT_MASK	0x1	/* HSI_COMMENT: Modify inner/single L4 header source port */
#define TGFS_CONTEXT_DATA_QREG2_FW_MODIFY_INNER_L4_SRC_PORT_SHIFT	20
#define TGFS_CONTEXT_DATA_QREG2_FW_MODIFY_INNER_L4_DEST_PORT_MASK	0x1	/* HSI_COMMENT: Modify inner/single L4 header destination port */
#define TGFS_CONTEXT_DATA_QREG2_FW_MODIFY_INNER_L4_DEST_PORT_SHIFT	21
#define TGFS_CONTEXT_DATA_QREG2_FW_MODIFY_INNER_MAC_MASK		0x1	/* HSI_COMMENT: Modify MACs in inner ETH header */
#define TGFS_CONTEXT_DATA_QREG2_FW_MODIFY_INNER_MAC_SHIFT		22
#define TGFS_CONTEXT_DATA_QREG2_FW_MODIFY_INNER_IP_DSCP_MASK		0x1	/* HSI_COMMENT: Modify inner IP DSCP */
#define TGFS_CONTEXT_DATA_QREG2_FW_MODIFY_INNER_IP_DSCP_SHIFT		23
#define TGFS_CONTEXT_DATA_QREG2_FW_SET_INNER_IP_TTL_HOP_LIMIT_MASK	0x1	/* HSI_COMMENT: Modify inner IP TTL or Hop Limit */
#define TGFS_CONTEXT_DATA_QREG2_FW_SET_INNER_IP_TTL_HOP_LIMIT_SHIFT	24
#define TGFS_CONTEXT_DATA_QREG2_FW_DEC_INNER_IP_TTL_HOP_LIMIT_MASK	0x1	/* HSI_COMMENT: Decrement inner IP TTL or Hop Limit */
#define TGFS_CONTEXT_DATA_QREG2_FW_DEC_INNER_IP_TTL_HOP_LIMIT_SHIFT	25
#define TGFS_CONTEXT_DATA_QREG2_RESERVED_MASK				0x3
#define TGFS_CONTEXT_DATA_QREG2_RESERVED_SHIFT				26
#define TGFS_CONTEXT_DATA_QREG2_FW_SET_CHECKSUM_OFFLOAD_MASK		0xF	/* HSI_COMMENT: Set checksum offload due to modify fields. FP must verify if checksum exist. */
#define TGFS_CONTEXT_DATA_QREG2_FW_SET_CHECKSUM_OFFLOAD_SHIFT		28
};

/* TGFS context Data QREG 3 */
struct tgfs_context_data_qreg3 {
	u32 bitfields0;
#define TGFS_CONTEXT_DATA_QREG3_FW_FIRST_HDR_DEST_MAC_IND_INDEX_MASK \
	        0x1FF		/* HSI_COMMENT: First ETH header destination MAC indirection index. 0 for unchanged. Replace basic block register 8. */
#define TGFS_CONTEXT_DATA_QREG3_FW_FIRST_HDR_DEST_MAC_IND_INDEX_SHIFT \
	        0
#define TGFS_CONTEXT_DATA_QREG3_FW_FIRST_HDR_SRC_MAC_IND_INDEX_MASK \
	        0x1FF		/* HSI_COMMENT: First ETH header source MAC indirection index. 0 for unchanged. Replace basic block register 8 */
#define TGFS_CONTEXT_DATA_QREG3_FW_FIRST_HDR_SRC_MAC_IND_INDEX_SHIFT \
	        9
#define TGFS_CONTEXT_DATA_QREG3_FW_POP_TYPE_MASK \
	        0x7		/* HSI_COMMENT: Reserved for last bit in searcher database */
#define TGFS_CONTEXT_DATA_QREG3_FW_POP_TYPE_SHIFT \
	        18
#define TGFS_CONTEXT_DATA_QREG3_FW_INNER_HDR_VLAN_MODE_MASK \
	        0x7		/* HSI_COMMENT: Inner ETH header VLAN modification: NOP, POP, PUSH, change VID, change PRI, change whole tag. Replace basic block register 8 (use enum gfs_vlan_mode) */
#define TGFS_CONTEXT_DATA_QREG3_FW_INNER_HDR_VLAN_MODE_SHIFT \
	        21
#define TGFS_CONTEXT_DATA_QREG3_FW_INNER_IP_TTL_HOP_LIMIT_MASK \
	        0xFF		/* HSI_COMMENT: inner IP TTL or Hop Limit Set value. Replace basic block register 8. */
#define TGFS_CONTEXT_DATA_QREG3_FW_INNER_IP_TTL_HOP_LIMIT_SHIFT \
	        24
	u16 bitfields1;
#define TGFS_CONTEXT_DATA_QREG3_FW_PUSH_TYPE_MASK \
	        0xF		/* HSI_COMMENT: Push headers type. */
#define TGFS_CONTEXT_DATA_QREG3_FW_PUSH_TYPE_SHIFT \
	        0
#define TGFS_CONTEXT_DATA_QREG3_FW_INNER_IP_DSCP_MASK \
	        0x3F		/* HSI_COMMENT: inner IP DSCP Set value */
#define TGFS_CONTEXT_DATA_QREG3_FW_INNER_IP_DSCP_SHIFT \
	        4
#define TGFS_CONTEXT_DATA_QREG3_RESERVED0_MASK \
	        0x3F
#define TGFS_CONTEXT_DATA_QREG3_RESERVED0_SHIFT \
	        10
	u16 fw_inner_vlan_val;	/* HSI_COMMENT: Inner VLAN value */
	u32 flow_mark;		/* HSI_COMMENT: Filter marker. Pass to Rx if destination is RX_VPORT */
	u32 reserved1;
};

/* TGFS context Data QREG 4 */
struct tgfs_context_data_qreg4 {
	u32 flow_id;		/* HSI_COMMENT: Flow Id. Replace basic block register 8. */
	u32 reserved;
	u32 fw_tunn_dest_ip;	/* HSI_COMMENT: HDR Modify   tunnel IPv4 destination IP or IPv6 indirection index. */
	u32 fw_tunn_src_ip;	/* HSI_COMMENT: HDR Modify   tunnel IPv4 source IP or IPv6 indirection index. */
};

/* TGFS context Data QREG 5 */
struct tgfs_context_data_qreg5 {
	u8 bitfields0;
#define TGFS_CONTEXT_DATA_QREG5_FW_TUNNEL_IP_ECN_MASK		0x3	/* HSI_COMMENT: Tunnel IP header flags for PUSH flow: 2 LSBits of tos field. */
#define TGFS_CONTEXT_DATA_QREG5_FW_TUNNEL_IP_ECN_SHIFT		0
#define TGFS_CONTEXT_DATA_QREG5_FW_TUNNEL_IP_DSCP_MASK		0x3F	/* HSI_COMMENT: Tunnel IP DSCP Set value */
#define TGFS_CONTEXT_DATA_QREG5_FW_TUNNEL_IP_DSCP_SHIFT		2
	u8 fw_tunn_ip_ttl_hop_limit;	/* HSI_COMMENT: Tunnel IP TTL or Hop Limit Set value */
	u8 event_id;		/* HSI_COMMENT: This field is used to replace part of event ID in MCM header. */
	u8 bitfields1;
#define TGFS_CONTEXT_DATA_QREG5_FW_ORIGINAL_COPY_FLG_MASK	0x1	/* HSI_COMMENT: Original flag set for some copy. */
#define TGFS_CONTEXT_DATA_QREG5_FW_ORIGINAL_COPY_FLG_SHIFT	0
#define TGFS_CONTEXT_DATA_QREG5_RSERVED0_MASK			0x7F	/* HSI_COMMENT: Original flag set for some copy. */
#define TGFS_CONTEXT_DATA_QREG5_RSERVED0_SHIFT			1
	union gfs_tunnel_header_data tunnel_header_data;	/* HSI_COMMENT: VXLAN/GRE tunnel header union. */
	union gfs_tunnel_header_flags tunnel_ip_header_flags;	/* HSI_COMMENT: Tunnel IP tunnel header flag union. */
	u32 reserved2;
};

/* BRB RAM init requirements */
struct init_brb_ram_req {
	u32 guranteed_per_tc;	/* HSI_COMMENT: guaranteed size per TC, in bytes */
	u32 headroom_per_tc;	/* HSI_COMMENT: headroom size per TC, in bytes */
	u32 min_pkt_size;	/* HSI_COMMENT: min packet size, in bytes */
	u32 max_ports_per_engine;	/* HSI_COMMENT: min packet size, in bytes */
	u8 num_active_tcs[MAX_NUM_PORTS];	/* HSI_COMMENT: number of active TCs per port */
};

/* ETS per-TC init requirements */
struct init_ets_tc_req {
	u8 use_sp;		/* HSI_COMMENT: if set, this TC participates in the arbitration with a strict priority (the priority is equal to the TC ID) */
	u8 use_wfq;		/* HSI_COMMENT: if set, this TC participates in the arbitration with a WFQ weight (indicated by the weight field) */
	u16 weight;		/* HSI_COMMENT: An arbitration weight. Valid only if use_wfq is set. */
};

/* ETS init requirements */
struct init_ets_req {
	u32 mtu;		/* HSI_COMMENT: Max packet size (in bytes) */
	struct init_ets_tc_req tc_req[NUM_OF_TCS];	/* HSI_COMMENT: ETS initialization requirements per TC. */
};

/* NIG LB RL init requirements */
struct init_nig_lb_rl_req {
	u16 lb_mac_rate;	/* HSI_COMMENT: Global MAC+LB RL rate (in Mbps). If set to 0, the RL will be disabled. */
	u16 lb_rate;		/* HSI_COMMENT: Global LB RL rate (in Mbps). If set to 0, the RL will be disabled. */
	u32 mtu;		/* HSI_COMMENT: Max packet size (in bytes) */
	u16 tc_rate[NUM_OF_PHYS_TCS];	/* HSI_COMMENT: RL rate per physical TC (in Mbps). If set to 0, the RL will be disabled. */
};

/* NIG TC mapping for each priority */
struct init_nig_pri_tc_map_entry {
	u8 tc_id;		/* HSI_COMMENT: the mapped TC ID */
	u8 valid;		/* HSI_COMMENT: indicates if the mapping entry is valid */
};

/* NIG priority to TC map init requirements */
struct init_nig_pri_tc_map_req {
	struct init_nig_pri_tc_map_entry pri[NUM_OF_VLAN_PRIORITIES];
};

/* QM per global RL init parameters */
struct init_qm_global_rl_params {
	u8 type;		/* HSI_COMMENT: rate limiter type. (use enum init_qm_rl_type) */
	u8 reserved0;
	u16 reserved1;
	u32 rate_limit;		/* HSI_COMMENT: Rate limit in Mb/sec units. If set to zero, the link speed is uwsed instead. */
};

/* QM per port init parameters */
struct init_qm_port_params {
	u16 active_phys_tcs;	/* HSI_COMMENT: Vector of valid bits for active TCs used by this port */
	u16 num_pbf_cmd_lines;	/* HSI_COMMENT: Number of PBF command lines that can be used by this port. In E4 each line is 256b, and in E5 each line is 512b. */
	u16 num_btb_blocks;	/* HSI_COMMENT: Number of BTB blocks that can be used by this port */
	u8 active;		/* HSI_COMMENT: Indicates if this port is active */
	u8 reserved;
};

/* QM per PQ init parameters */
struct init_qm_pq_params {
	u16 vport_id;		/* HSI_COMMENT: VPORT ID */
	u16 rl_id;		/* HSI_COMMENT: RL ID, valid only if rl_valid is true */
	u8 rl_valid;		/* HSI_COMMENT: Indicates if a rate limiter should be allocated for the PQ (0/1) */
	u8 tc_id;		/* HSI_COMMENT: TC ID */
	u8 wrr_group;		/* HSI_COMMENT: WRR group */
	u8 port_id;		/* HSI_COMMENT: Port ID */
};

/* QM per RL init parameters */
struct init_qm_rl_params {
	u32 vport_rl;		/* HSI_COMMENT: rate limit in Mb/sec units. a value of 0 means dont configure. ignored if VPORT RL is globally disabled. */
	u8 vport_rl_type;	/* HSI_COMMENT: rate limiter type. (use enum init_qm_rl_type) */
	u8 reserved[3];
};

/* QM Rate Limiter types */
enum init_qm_rl_type {
	QM_RL_TYPE_NORMAL,	/* HSI_COMMENT: store data (fast debug) */
	QM_RL_TYPE_QCN,		/* HSI_COMMENT: pram address (fast debug) */
	MAX_INIT_QM_RL_TYPE
};

/* QM per VPORT init parameters */
struct init_qm_vport_params {
	u16 wfq;		/* HSI_COMMENT: WFQ weight. A value of 0 means dont configure. ignored if VPORT WFQ is globally disabled. */
	u16 reserved;
	u16 tc_wfq[NUM_OF_TCS];	/* HSI_COMMENT: Per-TC WFQ weight. A value of 0 means dont configure. Ignored if wfq field is non-zero or if the VPORT WFQ feature is disabled. */
	u16 first_tx_pq_id[NUM_OF_TCS];	/* HSI_COMMENT: the first Tx PQ ID associated with this VPORT for each TC. */
};

/**************************************/
/* Init Tool HSI constants and macros */
/**************************************/

/* Width of GRC address in bits (addresses are specified in dwords) */
#define GRC_ADDR_BITS		23
#define MAX_GRC_ADDR		(BIT(GRC_ADDR_BITS) - 1)

/* indicates an init that should be applied to any phase ID */
#define ANY_PHASE_ID		0xffff

/* Max size in dwords of a zipped array */
#define MAX_ZIPPED_SIZE		8192

/* Chip IDs */
enum chip_ids {
	CHIP_BB,
	CHIP_K2,
	MAX_CHIP_IDS
};

/* Debug Bus Clients */
enum dbg_bus_clients {
	DBG_BUS_CLIENT_RBCN,
	DBG_BUS_CLIENT_RBCP,
	DBG_BUS_CLIENT_RBCR,
	DBG_BUS_CLIENT_RBCT,
	DBG_BUS_CLIENT_RBCU,
	DBG_BUS_CLIENT_RBCF,
	DBG_BUS_CLIENT_RBCX,
	DBG_BUS_CLIENT_RBCS,
	DBG_BUS_CLIENT_RBCH,
	DBG_BUS_CLIENT_RBCZ,
	DBG_BUS_CLIENT_OTHER_ENGINE,
	DBG_BUS_CLIENT_TIMESTAMP,
	DBG_BUS_CLIENT_CPU,
	DBG_BUS_CLIENT_RBCY,
	DBG_BUS_CLIENT_RBCQ,
	DBG_BUS_CLIENT_RBCM,
	DBG_BUS_CLIENT_RBCB,
	DBG_BUS_CLIENT_RBCW,
	DBG_BUS_CLIENT_RBCV,
	MAX_DBG_BUS_CLIENTS
};

/* Init modes */
enum init_modes {
	MODE_BB_A0_DEPRECATED,
	MODE_BB,
	MODE_K2,
	MODE_ASIC,
	MODE_EMUL_REDUCED,
	MODE_EMUL_FULL,
	MODE_FPGA,
	MODE_CHIPSIM,
	MODE_SF,
	MODE_MF_SD,
	MODE_MF_SI,
	MODE_PORTS_PER_ENG_1,
	MODE_PORTS_PER_ENG_2,
	MODE_PORTS_PER_ENG_4,
	MODE_100G,
	MODE_SKIP_PRAM_INIT,
	MODE_EMUL_MAC,
	MAX_INIT_MODES
};

/* Init phases */
enum init_phases {
	PHASE_ENGINE,
	PHASE_PORT,
	PHASE_PF,
	PHASE_VF,
	PHASE_QM_PF,
	MAX_INIT_PHASES
};

/* Init split types */
enum init_split_types {
	SPLIT_TYPE_NONE,
	SPLIT_TYPE_PORT,
	SPLIT_TYPE_PF,
	SPLIT_TYPE_PORT_PF,
	SPLIT_TYPE_VF,
	MAX_INIT_SPLIT_TYPES
};

/* Binary buffer header */
struct bin_buffer_hdr {
	u32 offset;		/* HSI_COMMENT: buffer offset in bytes from the beginning of the binary file */
	u32 length;		/* HSI_COMMENT: buffer length in bytes */
};

/* binary init buffer types */
enum bin_init_buffer_type {
	BIN_BUF_INIT_FW_VER_INFO,	/* HSI_COMMENT: fw_ver_info struct */
	BIN_BUF_INIT_CMD,	/* HSI_COMMENT: init commands */
	BIN_BUF_INIT_VAL,	/* HSI_COMMENT: init data */
	BIN_BUF_INIT_MODE_TREE,	/* HSI_COMMENT: init modes tree */
	BIN_BUF_INIT_IRO,	/* HSI_COMMENT: internal RAM offsets */
	BIN_BUF_INIT_OVERLAYS,	/* HSI_COMMENT: FW overlays (except overlay 0) */
	MAX_BIN_INIT_BUFFER_TYPE
};

/* FW overlay buffer header */
struct fw_overlay_buf_hdr {
	u32 data;
#define FW_OVERLAY_BUF_HDR_STORM_ID_MASK	0xFF	/* HSI_COMMENT: Storm ID */
#define FW_OVERLAY_BUF_HDR_STORM_ID_SHIFT	0
#define FW_OVERLAY_BUF_HDR_BUF_SIZE_MASK	0xFFFFFF	/* HSI_COMMENT: Size of Storm FW overlay buffer in dwords */
#define FW_OVERLAY_BUF_HDR_BUF_SIZE_SHIFT	8
};

/* init array header: raw */
struct init_array_raw_hdr {
	u32 data;
#define INIT_ARRAY_RAW_HDR_TYPE_MASK		0xF	/* HSI_COMMENT: Init array type, from init_array_types enum */
#define INIT_ARRAY_RAW_HDR_TYPE_SHIFT		0
#define INIT_ARRAY_RAW_HDR_PARAMS_MASK		0xFFFFFFF	/* HSI_COMMENT: init array params */
#define INIT_ARRAY_RAW_HDR_PARAMS_SHIFT		4
};

/* init array header: standard */
struct init_array_standard_hdr {
	u32 data;
#define INIT_ARRAY_STANDARD_HDR_TYPE_MASK	0xF	/* HSI_COMMENT: Init array type, from init_array_types enum */
#define INIT_ARRAY_STANDARD_HDR_TYPE_SHIFT	0
#define INIT_ARRAY_STANDARD_HDR_SIZE_MASK	0xFFFFFFF	/* HSI_COMMENT: Init array size (in dwords) */
#define INIT_ARRAY_STANDARD_HDR_SIZE_SHIFT	4
};

/* init array header: zipped */
struct init_array_zipped_hdr {
	u32 data;
#define INIT_ARRAY_ZIPPED_HDR_TYPE_MASK			0xF	/* HSI_COMMENT: Init array type, from init_array_types enum */
#define INIT_ARRAY_ZIPPED_HDR_TYPE_SHIFT		0
#define INIT_ARRAY_ZIPPED_HDR_ZIPPED_SIZE_MASK		0xFFFFFFF	/* HSI_COMMENT: Init array zipped size (in bytes) */
#define INIT_ARRAY_ZIPPED_HDR_ZIPPED_SIZE_SHIFT		4
};

/* init array header: pattern */
struct init_array_pattern_hdr {
	u32 data;
#define INIT_ARRAY_PATTERN_HDR_TYPE_MASK		0xF	/* HSI_COMMENT: Init array type, from init_array_types enum */
#define INIT_ARRAY_PATTERN_HDR_TYPE_SHIFT		0
#define INIT_ARRAY_PATTERN_HDR_PATTERN_SIZE_MASK	0xF	/* HSI_COMMENT: pattern size in dword */
#define INIT_ARRAY_PATTERN_HDR_PATTERN_SIZE_SHIFT	4
#define INIT_ARRAY_PATTERN_HDR_REPETITIONS_MASK		0xFFFFFF	/* HSI_COMMENT: pattern repetitions */
#define INIT_ARRAY_PATTERN_HDR_REPETITIONS_SHIFT	8
};

/* init array header union */
union init_array_hdr {
	struct init_array_raw_hdr raw;	/* HSI_COMMENT: raw init array header */
	struct init_array_standard_hdr standard;	/* HSI_COMMENT: standard init array header */
	struct init_array_zipped_hdr zipped;	/* HSI_COMMENT: zipped init array header */
	struct init_array_pattern_hdr pattern;	/* HSI_COMMENT: pattern init array header */
};

/* init array types */
enum init_array_types {
	INIT_ARR_STANDARD,	/* HSI_COMMENT: standard init array */
	INIT_ARR_ZIPPED,	/* HSI_COMMENT: zipped init array */
	INIT_ARR_PATTERN,	/* HSI_COMMENT: a repeated pattern */
	MAX_INIT_ARRAY_TYPES
};

/* init operation: callback */
struct init_callback_op {
	u32 op_data;
#define INIT_CALLBACK_OP_OP_MASK		0xF	/* HSI_COMMENT: Init operation, from init_op_types enum */
#define INIT_CALLBACK_OP_OP_SHIFT		0
#define INIT_CALLBACK_OP_RESERVED_MASK		0xFFFFFFF
#define INIT_CALLBACK_OP_RESERVED_SHIFT		4
	u16 callback_id;	/* HSI_COMMENT: Callback ID */
	u16 block_id;		/* HSI_COMMENT: Blocks ID */
};

/* init operation: delay */
struct init_delay_op {
	u32 op_data;
#define INIT_DELAY_OP_OP_MASK		0xF	/* HSI_COMMENT: Init operation, from init_op_types enum */
#define INIT_DELAY_OP_OP_SHIFT		0
#define INIT_DELAY_OP_RESERVED_MASK	0xFFFFFFF
#define INIT_DELAY_OP_RESERVED_SHIFT	4
	u32 delay;		/* HSI_COMMENT: delay in us */
};

/* init operation: if_mode */
struct init_if_mode_op {
	u32 op_data;
#define INIT_IF_MODE_OP_OP_MASK			0xF	/* HSI_COMMENT: Init operation, from init_op_types enum */
#define INIT_IF_MODE_OP_OP_SHIFT		0
#define INIT_IF_MODE_OP_RESERVED1_MASK		0xFFF
#define INIT_IF_MODE_OP_RESERVED1_SHIFT		4
#define INIT_IF_MODE_OP_CMD_OFFSET_MASK		0xFFFF	/* HSI_COMMENT: Commands to skip if the modes dont match */
#define INIT_IF_MODE_OP_CMD_OFFSET_SHIFT	16
	u16 reserved2;
	u16 modes_buf_offset;	/* HSI_COMMENT: offset (in bytes) in modes expression buffer */
};

/* init operation: if_phase */
struct init_if_phase_op {
	u32 op_data;
#define INIT_IF_PHASE_OP_OP_MASK		0xF	/* HSI_COMMENT: Init operation, from init_op_types enum */
#define INIT_IF_PHASE_OP_OP_SHIFT		0
#define INIT_IF_PHASE_OP_RESERVED1_MASK		0xFFF
#define INIT_IF_PHASE_OP_RESERVED1_SHIFT	4
#define INIT_IF_PHASE_OP_CMD_OFFSET_MASK	0xFFFF	/* HSI_COMMENT: Commands to skip if the phases dont match */
#define INIT_IF_PHASE_OP_CMD_OFFSET_SHIFT	16
	u32 phase_data;
#define INIT_IF_PHASE_OP_PHASE_MASK		0xFF	/* HSI_COMMENT: Init phase */
#define INIT_IF_PHASE_OP_PHASE_SHIFT		0
#define INIT_IF_PHASE_OP_RESERVED2_MASK		0xFF
#define INIT_IF_PHASE_OP_RESERVED2_SHIFT	8
#define INIT_IF_PHASE_OP_PHASE_ID_MASK		0xFFFF	/* HSI_COMMENT: Init phase ID */
#define INIT_IF_PHASE_OP_PHASE_ID_SHIFT		16
};

/* init mode operators */
enum init_mode_ops {
	INIT_MODE_OP_NOT,	/* HSI_COMMENT: init mode not operator */
	INIT_MODE_OP_OR,	/* HSI_COMMENT: init mode or operator */
	INIT_MODE_OP_AND,	/* HSI_COMMENT: init mode and operator */
	MAX_INIT_MODE_OPS
};

/* init operation: raw */
struct init_raw_op {
	u32 op_data;
#define INIT_RAW_OP_OP_MASK		0xF	/* HSI_COMMENT: Init operation, from init_op_types enum */
#define INIT_RAW_OP_OP_SHIFT		0
#define INIT_RAW_OP_PARAM1_MASK		0xFFFFFFF	/* HSI_COMMENT: init param 1 */
#define INIT_RAW_OP_PARAM1_SHIFT	4
	u32 param2;		/* HSI_COMMENT: Init param 2 */
};

/* init array params */
struct init_op_array_params {
	u16 size;		/* HSI_COMMENT: array size in dwords */
	u16 offset;		/* HSI_COMMENT: array start offset in dwords */
};

/* Write init operation arguments */
union init_write_args {
	u32 inline_val;		/* HSI_COMMENT: value to write, used when init source is INIT_SRC_INLINE */
	u32 zeros_count;	/* HSI_COMMENT: number of zeros to write, used when init source is INIT_SRC_ZEROS */
	u32 array_offset;	/* HSI_COMMENT: array offset to write, used when init source is INIT_SRC_ARRAY */
	struct init_op_array_params runtime;	/* HSI_COMMENT: runtime array params to write, used when init source is INIT_SRC_RUNTIME */
};

/* init operation: write */
struct init_write_op {
	u32 data;
#define INIT_WRITE_OP_OP_MASK		0xF	/* HSI_COMMENT: init operation, from init_op_types enum */
#define INIT_WRITE_OP_OP_SHIFT		0
#define INIT_WRITE_OP_SOURCE_MASK	0x7	/* HSI_COMMENT: init source type, taken from init_source_types enum */
#define INIT_WRITE_OP_SOURCE_SHIFT	4
#define INIT_WRITE_OP_RESERVED_MASK	0x1
#define INIT_WRITE_OP_RESERVED_SHIFT	7
#define INIT_WRITE_OP_WIDE_BUS_MASK	0x1	/* HSI_COMMENT: indicates if the register is wide-bus */
#define INIT_WRITE_OP_WIDE_BUS_SHIFT	8
#define INIT_WRITE_OP_ADDRESS_MASK	0x7FFFFF	/* HSI_COMMENT: internal (absolute) GRC address, in dwords */
#define INIT_WRITE_OP_ADDRESS_SHIFT	9
	union init_write_args args;	/* HSI_COMMENT: Write init operation arguments */
};

/* init operation: read */
struct init_read_op {
	u32 op_data;
#define INIT_READ_OP_OP_MASK		0xF	/* HSI_COMMENT: init operation, from init_op_types enum */
#define INIT_READ_OP_OP_SHIFT		0
#define INIT_READ_OP_POLL_TYPE_MASK	0xF	/* HSI_COMMENT: polling type, from init_poll_types enum */
#define INIT_READ_OP_POLL_TYPE_SHIFT	4
#define INIT_READ_OP_RESERVED_MASK	0x1
#define INIT_READ_OP_RESERVED_SHIFT	8
#define INIT_READ_OP_ADDRESS_MASK	0x7FFFFF	/* HSI_COMMENT: internal (absolute) GRC address, in dwords */
#define INIT_READ_OP_ADDRESS_SHIFT	9
	u32 expected_val;	/* HSI_COMMENT: expected polling value, used only when polling is done */
};

/* Init operations union */
union init_op {
	struct init_raw_op raw;	/* HSI_COMMENT: raw init operation */
	struct init_write_op write;	/* HSI_COMMENT: write init operation */
	struct init_read_op read;	/* HSI_COMMENT: read init operation */
	struct init_if_mode_op if_mode;	/* HSI_COMMENT: if_mode init operation */
	struct init_if_phase_op if_phase;	/* HSI_COMMENT: if_phase init operation */
	struct init_callback_op callback;	/* HSI_COMMENT: callback init operation */
	struct init_delay_op delay;	/* HSI_COMMENT: delay init operation */
};

/* Init command operation types */
enum init_op_types {
	INIT_OP_READ,		/* HSI_COMMENT: GRC read init command */
	INIT_OP_WRITE,		/* HSI_COMMENT: GRC write init command */
	INIT_OP_IF_MODE,	/* HSI_COMMENT: Skip init commands if the init modes expression doesnt match */
	INIT_OP_IF_PHASE,	/* HSI_COMMENT: Skip init commands if the init phase doesnt match */
	INIT_OP_DELAY,		/* HSI_COMMENT: delay init command */
	INIT_OP_CALLBACK,	/* HSI_COMMENT: callback init command */
	MAX_INIT_OP_TYPES
};

/* init polling types */
enum init_poll_types {
	INIT_POLL_NONE,		/* HSI_COMMENT: No polling */
	INIT_POLL_EQ,		/* HSI_COMMENT: init value is included in the init command */
	INIT_POLL_OR,		/* HSI_COMMENT: init value is all zeros */
	INIT_POLL_AND,		/* HSI_COMMENT: init value is an array of values */
	MAX_INIT_POLL_TYPES
};

/* init source types */
enum init_source_types {
	INIT_SRC_INLINE,	/* HSI_COMMENT: init value is included in the init command */
	INIT_SRC_ZEROS,		/* HSI_COMMENT: init value is all zeros */
	INIT_SRC_ARRAY,		/* HSI_COMMENT: init value is an array of values */
	INIT_SRC_RUNTIME,	/* HSI_COMMENT: init value is provided during runtime */
	MAX_INIT_SOURCE_TYPES
};

/* Internal RAM Offsets macro data */
struct iro {
	u32 base;		/* HSI_COMMENT: RAM field offset */
	u16 m1;			/* HSI_COMMENT: multiplier 1 */
	u16 m2;			/* HSI_COMMENT: multiplier 2 */
	u16 m3;			/* HSI_COMMENT: multiplier 3 */
	u16 size;		/* HSI_COMMENT: RAM field size */
};

/* Win 2 */
#define GTT_BAR0_MAP_REG_IGU_CMD		0x00f000UL	// Access:RW   DataWidth:0x20

/* Win 3 */
#define GTT_BAR0_MAP_REG_TSDM_RAM		0x010000UL	// Access:RW   DataWidth:0x20

/* Win 4 */
#define GTT_BAR0_MAP_REG_MSDM_RAM		0x011000UL	// Access:RW   DataWidth:0x20

/* Win 5 */
#define GTT_BAR0_MAP_REG_MSDM_RAM_1024		0x012000UL	// Access:RW   DataWidth:0x20

/* Win 6 */
#define GTT_BAR0_MAP_REG_MSDM_RAM_2048		0x013000UL	// Access:RW   DataWidth:0x20

/* Win 7 */
#define GTT_BAR0_MAP_REG_USDM_RAM		0x014000UL	// Access:RW   DataWidth:0x20

/* Win 8 */
#define GTT_BAR0_MAP_REG_USDM_RAM_1024		0x015000UL	// Access:RW   DataWidth:0x20

/* Win 9 */
#define GTT_BAR0_MAP_REG_USDM_RAM_2048		0x016000UL	// Access:RW   DataWidth:0x20

/* Win 10 */
#define GTT_BAR0_MAP_REG_XSDM_RAM		0x017000UL	// Access:RW   DataWidth:0x20

/* Win 11 */
#define GTT_BAR0_MAP_REG_XSDM_RAM_1024		0x018000UL	// Access:RW   DataWidth:0x20

/* Win 12 */
#define GTT_BAR0_MAP_REG_YSDM_RAM		0x019000UL	// Access:RW   DataWidth:0x20

/* Win 13 */
#define GTT_BAR0_MAP_REG_PSDM_RAM		0x01a000UL	// Access:RW   DataWidth:0x20

/* Win 14 */

#ifndef __PREVENT_PXP_GLOBAL_WIN__

static u32 pxp_global_win[] = {
	0,
	0,
	0x1c02,			/* win 2: addr=0x1c02000, size=4096 bytes */
	0x1c80,			/* win 3: addr=0x1c80000, size=4096 bytes */
	0x1d00,			/* win 4: addr=0x1d00000, size=4096 bytes */
	0x1d01,			/* win 5: addr=0x1d01000, size=4096 bytes */
	0x1d02,			/* win 6: addr=0x1d02000, size=4096 bytes */
	0x1d80,			/* win 7: addr=0x1d80000, size=4096 bytes */
	0x1d81,			/* win 8: addr=0x1d81000, size=4096 bytes */
	0x1d82,			/* win 9: addr=0x1d82000, size=4096 bytes */
	0x1e00,			/* win 10: addr=0x1e00000, size=4096 bytes */
	0x1e01,			/* win 11: addr=0x1e01000, size=4096 bytes */
	0x1e80,			/* win 12: addr=0x1e80000, size=4096 bytes */
	0x1f00,			/* win 13: addr=0x1f00000, size=4096 bytes */
	0x1c08,			/* win 14: addr=0x1c08000, size=4096 bytes */
	0,
	0,
	0,
	0,
};

#endif /* __PREVENT_PXP_GLOBAL_WIN__ */

#ifndef __RT_DEFS_H__
#define __RT_DEFS_H__

/* Runtime array offsets */
#define DORQ_REG_PF_MAX_ICID_0_RT_OFFSET				0
#define DORQ_REG_PF_MAX_ICID_1_RT_OFFSET				1
#define DORQ_REG_PF_MAX_ICID_2_RT_OFFSET				2
#define DORQ_REG_PF_MAX_ICID_3_RT_OFFSET				3
#define DORQ_REG_PF_MAX_ICID_4_RT_OFFSET				4
#define DORQ_REG_PF_MAX_ICID_5_RT_OFFSET				5
#define DORQ_REG_PF_MAX_ICID_6_RT_OFFSET				6
#define DORQ_REG_PF_MAX_ICID_7_RT_OFFSET				7
#define DORQ_REG_VF_MAX_ICID_0_RT_OFFSET				8
#define DORQ_REG_VF_MAX_ICID_1_RT_OFFSET				9
#define DORQ_REG_VF_MAX_ICID_2_RT_OFFSET				10
#define DORQ_REG_VF_MAX_ICID_3_RT_OFFSET				11
#define DORQ_REG_VF_MAX_ICID_4_RT_OFFSET				12
#define DORQ_REG_VF_MAX_ICID_5_RT_OFFSET				13
#define DORQ_REG_VF_MAX_ICID_6_RT_OFFSET				14
#define DORQ_REG_VF_MAX_ICID_7_RT_OFFSET				15
#define DORQ_REG_VF_ICID_BIT_SHIFT_NORM_RT_OFFSET			16
#define DORQ_REG_PF_WAKE_ALL_RT_OFFSET					17
#define DORQ_REG_TAG1_ETHERTYPE_RT_OFFSET				18
#define IGU_REG_PF_CONFIGURATION_RT_OFFSET				19
#define IGU_REG_VF_CONFIGURATION_RT_OFFSET				20
#define IGU_REG_ATTN_MSG_ADDR_L_RT_OFFSET				21
#define IGU_REG_ATTN_MSG_ADDR_H_RT_OFFSET				22
#define IGU_REG_LEADING_EDGE_LATCH_RT_OFFSET				23
#define IGU_REG_TRAILING_EDGE_LATCH_RT_OFFSET				24
#define CAU_REG_CQE_AGG_UNIT_SIZE_RT_OFFSET				25
#define CAU_REG_SB_VAR_MEMORY_RT_OFFSET					26
#define CAU_REG_SB_VAR_MEMORY_RT_SIZE					736
#define CAU_REG_SB_ADDR_MEMORY_RT_OFFSET				762
#define CAU_REG_SB_ADDR_MEMORY_RT_SIZE					736
#define CAU_REG_PI_MEMORY_RT_OFFSET					1498
#define CAU_REG_PI_MEMORY_RT_SIZE					4416
#define PRS_REG_SEARCH_RESP_INITIATOR_TYPE_RT_OFFSET			5914
#define PRS_REG_TASK_ID_MAX_INITIATOR_PF_RT_OFFSET			5915
#define PRS_REG_TASK_ID_MAX_INITIATOR_VF_RT_OFFSET			5916
#define PRS_REG_TASK_ID_MAX_TARGET_PF_RT_OFFSET				5917
#define PRS_REG_TASK_ID_MAX_TARGET_VF_RT_OFFSET				5918
#define PRS_REG_SEARCH_TCP_RT_OFFSET					5919
#define PRS_REG_SEARCH_FCOE_RT_OFFSET					5920
#define PRS_REG_SEARCH_ROCE_RT_OFFSET					5921
#define PRS_REG_ROCE_DEST_QP_MAX_VF_RT_OFFSET				5922
#define PRS_REG_ROCE_DEST_QP_MAX_PF_RT_OFFSET				5923
#define PRS_REG_SEARCH_OPENFLOW_RT_OFFSET				5924
#define PRS_REG_SEARCH_NON_IP_AS_OPENFLOW_RT_OFFSET			5925
#define PRS_REG_OPENFLOW_SUPPORT_ONLY_KNOWN_OVER_IP_RT_OFFSET		5926
#define PRS_REG_OPENFLOW_SEARCH_KEY_MASK_RT_OFFSET			5927
#define PRS_REG_TAG_ETHERTYPE_0_RT_OFFSET				5928
#define PRS_REG_LIGHT_L2_ETHERTYPE_EN_RT_OFFSET				5929
#define SRC_REG_FIRSTFREE_RT_OFFSET					5930
#define SRC_REG_FIRSTFREE_RT_SIZE					2
#define SRC_REG_LASTFREE_RT_OFFSET					5932
#define SRC_REG_LASTFREE_RT_SIZE					2
#define SRC_REG_COUNTFREE_RT_OFFSET					5934
#define SRC_REG_NUMBER_HASH_BITS_RT_OFFSET				5935
#define PSWRQ2_REG_CDUT_P_SIZE_RT_OFFSET				5936
#define PSWRQ2_REG_CDUC_P_SIZE_RT_OFFSET				5937
#define PSWRQ2_REG_TM_P_SIZE_RT_OFFSET					5938
#define PSWRQ2_REG_QM_P_SIZE_RT_OFFSET					5939
#define PSWRQ2_REG_SRC_P_SIZE_RT_OFFSET					5940
#define PSWRQ2_REG_TSDM_P_SIZE_RT_OFFSET				5941
#define PSWRQ2_REG_TM_FIRST_ILT_RT_OFFSET				5942
#define PSWRQ2_REG_TM_LAST_ILT_RT_OFFSET				5943
#define PSWRQ2_REG_QM_FIRST_ILT_RT_OFFSET				5944
#define PSWRQ2_REG_QM_LAST_ILT_RT_OFFSET				5945
#define PSWRQ2_REG_SRC_FIRST_ILT_RT_OFFSET				5946
#define PSWRQ2_REG_SRC_LAST_ILT_RT_OFFSET				5947
#define PSWRQ2_REG_CDUC_FIRST_ILT_RT_OFFSET				5948
#define PSWRQ2_REG_CDUC_LAST_ILT_RT_OFFSET				5949
#define PSWRQ2_REG_CDUT_FIRST_ILT_RT_OFFSET				5950
#define PSWRQ2_REG_CDUT_LAST_ILT_RT_OFFSET				5951
#define PSWRQ2_REG_TSDM_FIRST_ILT_RT_OFFSET				5952
#define PSWRQ2_REG_TSDM_LAST_ILT_RT_OFFSET				5953
#define PSWRQ2_REG_TM_NUMBER_OF_PF_BLOCKS_RT_OFFSET			5954
#define PSWRQ2_REG_CDUT_NUMBER_OF_PF_BLOCKS_RT_OFFSET			5955
#define PSWRQ2_REG_CDUC_NUMBER_OF_PF_BLOCKS_RT_OFFSET			5956
#define PSWRQ2_REG_TM_VF_BLOCKS_RT_OFFSET				5957
#define PSWRQ2_REG_CDUT_VF_BLOCKS_RT_OFFSET				5958
#define PSWRQ2_REG_CDUC_VF_BLOCKS_RT_OFFSET				5959
#define PSWRQ2_REG_TM_BLOCKS_FACTOR_RT_OFFSET				5960
#define PSWRQ2_REG_CDUT_BLOCKS_FACTOR_RT_OFFSET				5961
#define PSWRQ2_REG_CDUC_BLOCKS_FACTOR_RT_OFFSET				5962
#define PSWRQ2_REG_VF_BASE_RT_OFFSET					5963
#define PSWRQ2_REG_VF_LAST_ILT_RT_OFFSET				5964
#define PSWRQ2_REG_DRAM_ALIGN_WR_RT_OFFSET				5965
#define PSWRQ2_REG_DRAM_ALIGN_RD_RT_OFFSET				5966
#define PSWRQ2_REG_ILT_MEMORY_RT_OFFSET					5967
#define PSWRQ2_REG_ILT_MEMORY_RT_SIZE					22000
#define PGLUE_REG_B_VF_BASE_RT_OFFSET					27967
#define PGLUE_REG_B_MSDM_OFFSET_MASK_B_RT_OFFSET			27968
#define PGLUE_REG_B_MSDM_VF_SHIFT_B_RT_OFFSET				27969
#define PGLUE_REG_B_CACHE_LINE_SIZE_RT_OFFSET				27970
#define PGLUE_REG_B_PF_BAR0_SIZE_RT_OFFSET				27971
#define PGLUE_REG_B_PF_BAR1_SIZE_RT_OFFSET				27972
#define PGLUE_REG_B_VF_BAR1_SIZE_RT_OFFSET				27973
#define TM_REG_VF_ENABLE_CONN_RT_OFFSET					27974
#define TM_REG_PF_ENABLE_CONN_RT_OFFSET					27975
#define TM_REG_PF_ENABLE_TASK_RT_OFFSET					27976
#define TM_REG_GROUP_SIZE_RESOLUTION_CONN_RT_OFFSET			27977
#define TM_REG_GROUP_SIZE_RESOLUTION_TASK_RT_OFFSET			27978
#define TM_REG_CONFIG_CONN_MEM_RT_OFFSET				27979
#define TM_REG_CONFIG_CONN_MEM_RT_SIZE					416
#define TM_REG_CONFIG_TASK_MEM_RT_OFFSET				28395
#define TM_REG_CONFIG_TASK_MEM_RT_SIZE					512
#define QM_REG_MAXPQSIZE_0_RT_OFFSET					28907
#define QM_REG_MAXPQSIZE_1_RT_OFFSET					28908
#define QM_REG_MAXPQSIZE_2_RT_OFFSET					28909
#define QM_REG_MAXPQSIZETXSEL_0_RT_OFFSET				28910
#define QM_REG_MAXPQSIZETXSEL_1_RT_OFFSET				28911
#define QM_REG_MAXPQSIZETXSEL_2_RT_OFFSET				28912
#define QM_REG_MAXPQSIZETXSEL_3_RT_OFFSET				28913
#define QM_REG_MAXPQSIZETXSEL_4_RT_OFFSET				28914
#define QM_REG_MAXPQSIZETXSEL_5_RT_OFFSET				28915
#define QM_REG_MAXPQSIZETXSEL_6_RT_OFFSET				28916
#define QM_REG_MAXPQSIZETXSEL_7_RT_OFFSET				28917
#define QM_REG_MAXPQSIZETXSEL_8_RT_OFFSET				28918
#define QM_REG_MAXPQSIZETXSEL_9_RT_OFFSET				28919
#define QM_REG_MAXPQSIZETXSEL_10_RT_OFFSET				28920
#define QM_REG_MAXPQSIZETXSEL_11_RT_OFFSET				28921
#define QM_REG_MAXPQSIZETXSEL_12_RT_OFFSET				28922
#define QM_REG_MAXPQSIZETXSEL_13_RT_OFFSET				28923
#define QM_REG_MAXPQSIZETXSEL_14_RT_OFFSET				28924
#define QM_REG_MAXPQSIZETXSEL_15_RT_OFFSET				28925
#define QM_REG_MAXPQSIZETXSEL_16_RT_OFFSET				28926
#define QM_REG_MAXPQSIZETXSEL_17_RT_OFFSET				28927
#define QM_REG_MAXPQSIZETXSEL_18_RT_OFFSET				28928
#define QM_REG_MAXPQSIZETXSEL_19_RT_OFFSET				28929
#define QM_REG_MAXPQSIZETXSEL_20_RT_OFFSET				28930
#define QM_REG_MAXPQSIZETXSEL_21_RT_OFFSET				28931
#define QM_REG_MAXPQSIZETXSEL_22_RT_OFFSET				28932
#define QM_REG_MAXPQSIZETXSEL_23_RT_OFFSET				28933
#define QM_REG_MAXPQSIZETXSEL_24_RT_OFFSET				28934
#define QM_REG_MAXPQSIZETXSEL_25_RT_OFFSET				28935
#define QM_REG_MAXPQSIZETXSEL_26_RT_OFFSET				28936
#define QM_REG_MAXPQSIZETXSEL_27_RT_OFFSET				28937
#define QM_REG_MAXPQSIZETXSEL_28_RT_OFFSET				28938
#define QM_REG_MAXPQSIZETXSEL_29_RT_OFFSET				28939
#define QM_REG_MAXPQSIZETXSEL_30_RT_OFFSET				28940
#define QM_REG_MAXPQSIZETXSEL_31_RT_OFFSET				28941
#define QM_REG_MAXPQSIZETXSEL_32_RT_OFFSET				28942
#define QM_REG_MAXPQSIZETXSEL_33_RT_OFFSET				28943
#define QM_REG_MAXPQSIZETXSEL_34_RT_OFFSET				28944
#define QM_REG_MAXPQSIZETXSEL_35_RT_OFFSET				28945
#define QM_REG_MAXPQSIZETXSEL_36_RT_OFFSET				28946
#define QM_REG_MAXPQSIZETXSEL_37_RT_OFFSET				28947
#define QM_REG_MAXPQSIZETXSEL_38_RT_OFFSET				28948
#define QM_REG_MAXPQSIZETXSEL_39_RT_OFFSET				28949
#define QM_REG_MAXPQSIZETXSEL_40_RT_OFFSET				28950
#define QM_REG_MAXPQSIZETXSEL_41_RT_OFFSET				28951
#define QM_REG_MAXPQSIZETXSEL_42_RT_OFFSET				28952
#define QM_REG_MAXPQSIZETXSEL_43_RT_OFFSET				28953
#define QM_REG_MAXPQSIZETXSEL_44_RT_OFFSET				28954
#define QM_REG_MAXPQSIZETXSEL_45_RT_OFFSET				28955
#define QM_REG_MAXPQSIZETXSEL_46_RT_OFFSET				28956
#define QM_REG_MAXPQSIZETXSEL_47_RT_OFFSET				28957
#define QM_REG_MAXPQSIZETXSEL_48_RT_OFFSET				28958
#define QM_REG_MAXPQSIZETXSEL_49_RT_OFFSET				28959
#define QM_REG_MAXPQSIZETXSEL_50_RT_OFFSET				28960
#define QM_REG_MAXPQSIZETXSEL_51_RT_OFFSET				28961
#define QM_REG_MAXPQSIZETXSEL_52_RT_OFFSET				28962
#define QM_REG_MAXPQSIZETXSEL_53_RT_OFFSET				28963
#define QM_REG_MAXPQSIZETXSEL_54_RT_OFFSET				28964
#define QM_REG_MAXPQSIZETXSEL_55_RT_OFFSET				28965
#define QM_REG_MAXPQSIZETXSEL_56_RT_OFFSET				28966
#define QM_REG_MAXPQSIZETXSEL_57_RT_OFFSET				28967
#define QM_REG_MAXPQSIZETXSEL_58_RT_OFFSET				28968
#define QM_REG_MAXPQSIZETXSEL_59_RT_OFFSET				28969
#define QM_REG_MAXPQSIZETXSEL_60_RT_OFFSET				28970
#define QM_REG_MAXPQSIZETXSEL_61_RT_OFFSET				28971
#define QM_REG_MAXPQSIZETXSEL_62_RT_OFFSET				28972
#define QM_REG_MAXPQSIZETXSEL_63_RT_OFFSET				28973
#define QM_REG_BASEADDROTHERPQ_RT_OFFSET				28974
#define QM_REG_BASEADDROTHERPQ_RT_SIZE					128
#define QM_REG_PTRTBLOTHER_RT_OFFSET					29102
#define QM_REG_PTRTBLOTHER_RT_SIZE					256
#define QM_REG_VOQCRDLINE_RT_OFFSET					29358
#define QM_REG_VOQCRDLINE_RT_SIZE					20
#define QM_REG_VOQINITCRDLINE_RT_OFFSET					29378
#define QM_REG_VOQINITCRDLINE_RT_SIZE					20
#define QM_REG_AFULLQMBYPTHRPFWFQ_RT_OFFSET				29398
#define QM_REG_AFULLQMBYPTHRVPWFQ_RT_OFFSET				29399
#define QM_REG_AFULLQMBYPTHRPFRL_RT_OFFSET				29400
#define QM_REG_AFULLQMBYPTHRGLBLRL_RT_OFFSET				29401
#define QM_REG_AFULLOPRTNSTCCRDMASK_RT_OFFSET				29402
#define QM_REG_WRROTHERPQGRP_0_RT_OFFSET				29403
#define QM_REG_WRROTHERPQGRP_1_RT_OFFSET				29404
#define QM_REG_WRROTHERPQGRP_2_RT_OFFSET				29405
#define QM_REG_WRROTHERPQGRP_3_RT_OFFSET				29406
#define QM_REG_WRROTHERPQGRP_4_RT_OFFSET				29407
#define QM_REG_WRROTHERPQGRP_5_RT_OFFSET				29408
#define QM_REG_WRROTHERPQGRP_6_RT_OFFSET				29409
#define QM_REG_WRROTHERPQGRP_7_RT_OFFSET				29410
#define QM_REG_WRROTHERPQGRP_8_RT_OFFSET				29411
#define QM_REG_WRROTHERPQGRP_9_RT_OFFSET				29412
#define QM_REG_WRROTHERPQGRP_10_RT_OFFSET				29413
#define QM_REG_WRROTHERPQGRP_11_RT_OFFSET				29414
#define QM_REG_WRROTHERPQGRP_12_RT_OFFSET				29415
#define QM_REG_WRROTHERPQGRP_13_RT_OFFSET				29416
#define QM_REG_WRROTHERPQGRP_14_RT_OFFSET				29417
#define QM_REG_WRROTHERPQGRP_15_RT_OFFSET				29418
#define QM_REG_WRROTHERGRPWEIGHT_0_RT_OFFSET				29419
#define QM_REG_WRROTHERGRPWEIGHT_1_RT_OFFSET				29420
#define QM_REG_WRROTHERGRPWEIGHT_2_RT_OFFSET				29421
#define QM_REG_WRROTHERGRPWEIGHT_3_RT_OFFSET				29422
#define QM_REG_WRRTXGRPWEIGHT_0_RT_OFFSET				29423
#define QM_REG_WRRTXGRPWEIGHT_1_RT_OFFSET				29424
#define QM_REG_PQTX2PF_0_RT_OFFSET					29425
#define QM_REG_PQTX2PF_1_RT_OFFSET					29426
#define QM_REG_PQTX2PF_2_RT_OFFSET					29427
#define QM_REG_PQTX2PF_3_RT_OFFSET					29428
#define QM_REG_PQTX2PF_4_RT_OFFSET					29429
#define QM_REG_PQTX2PF_5_RT_OFFSET					29430
#define QM_REG_PQTX2PF_6_RT_OFFSET					29431
#define QM_REG_PQTX2PF_7_RT_OFFSET					29432
#define QM_REG_PQTX2PF_8_RT_OFFSET					29433
#define QM_REG_PQTX2PF_9_RT_OFFSET					29434
#define QM_REG_PQTX2PF_10_RT_OFFSET					29435
#define QM_REG_PQTX2PF_11_RT_OFFSET					29436
#define QM_REG_PQTX2PF_12_RT_OFFSET					29437
#define QM_REG_PQTX2PF_13_RT_OFFSET					29438
#define QM_REG_PQTX2PF_14_RT_OFFSET					29439
#define QM_REG_PQTX2PF_15_RT_OFFSET					29440
#define QM_REG_PQTX2PF_16_RT_OFFSET					29441
#define QM_REG_PQTX2PF_17_RT_OFFSET					29442
#define QM_REG_PQTX2PF_18_RT_OFFSET					29443
#define QM_REG_PQTX2PF_19_RT_OFFSET					29444
#define QM_REG_PQTX2PF_20_RT_OFFSET					29445
#define QM_REG_PQTX2PF_21_RT_OFFSET					29446
#define QM_REG_PQTX2PF_22_RT_OFFSET					29447
#define QM_REG_PQTX2PF_23_RT_OFFSET					29448
#define QM_REG_PQTX2PF_24_RT_OFFSET					29449
#define QM_REG_PQTX2PF_25_RT_OFFSET					29450
#define QM_REG_PQTX2PF_26_RT_OFFSET					29451
#define QM_REG_PQTX2PF_27_RT_OFFSET					29452
#define QM_REG_PQTX2PF_28_RT_OFFSET					29453
#define QM_REG_PQTX2PF_29_RT_OFFSET					29454
#define QM_REG_PQTX2PF_30_RT_OFFSET					29455
#define QM_REG_PQTX2PF_31_RT_OFFSET					29456
#define QM_REG_PQTX2PF_32_RT_OFFSET					29457
#define QM_REG_PQTX2PF_33_RT_OFFSET					29458
#define QM_REG_PQTX2PF_34_RT_OFFSET					29459
#define QM_REG_PQTX2PF_35_RT_OFFSET					29460
#define QM_REG_PQTX2PF_36_RT_OFFSET					29461
#define QM_REG_PQTX2PF_37_RT_OFFSET					29462
#define QM_REG_PQTX2PF_38_RT_OFFSET					29463
#define QM_REG_PQTX2PF_39_RT_OFFSET					29464
#define QM_REG_PQTX2PF_40_RT_OFFSET					29465
#define QM_REG_PQTX2PF_41_RT_OFFSET					29466
#define QM_REG_PQTX2PF_42_RT_OFFSET					29467
#define QM_REG_PQTX2PF_43_RT_OFFSET					29468
#define QM_REG_PQTX2PF_44_RT_OFFSET					29469
#define QM_REG_PQTX2PF_45_RT_OFFSET					29470
#define QM_REG_PQTX2PF_46_RT_OFFSET					29471
#define QM_REG_PQTX2PF_47_RT_OFFSET					29472
#define QM_REG_PQTX2PF_48_RT_OFFSET					29473
#define QM_REG_PQTX2PF_49_RT_OFFSET					29474
#define QM_REG_PQTX2PF_50_RT_OFFSET					29475
#define QM_REG_PQTX2PF_51_RT_OFFSET					29476
#define QM_REG_PQTX2PF_52_RT_OFFSET					29477
#define QM_REG_PQTX2PF_53_RT_OFFSET					29478
#define QM_REG_PQTX2PF_54_RT_OFFSET					29479
#define QM_REG_PQTX2PF_55_RT_OFFSET					29480
#define QM_REG_PQTX2PF_56_RT_OFFSET					29481
#define QM_REG_PQTX2PF_57_RT_OFFSET					29482
#define QM_REG_PQTX2PF_58_RT_OFFSET					29483
#define QM_REG_PQTX2PF_59_RT_OFFSET					29484
#define QM_REG_PQTX2PF_60_RT_OFFSET					29485
#define QM_REG_PQTX2PF_61_RT_OFFSET					29486
#define QM_REG_PQTX2PF_62_RT_OFFSET					29487
#define QM_REG_PQTX2PF_63_RT_OFFSET					29488
#define QM_REG_PQOTHER2PF_0_RT_OFFSET					29489
#define QM_REG_PQOTHER2PF_1_RT_OFFSET					29490
#define QM_REG_PQOTHER2PF_2_RT_OFFSET					29491
#define QM_REG_PQOTHER2PF_3_RT_OFFSET					29492
#define QM_REG_PQOTHER2PF_4_RT_OFFSET					29493
#define QM_REG_PQOTHER2PF_5_RT_OFFSET					29494
#define QM_REG_PQOTHER2PF_6_RT_OFFSET					29495
#define QM_REG_PQOTHER2PF_7_RT_OFFSET					29496
#define QM_REG_PQOTHER2PF_8_RT_OFFSET					29497
#define QM_REG_PQOTHER2PF_9_RT_OFFSET					29498
#define QM_REG_PQOTHER2PF_10_RT_OFFSET					29499
#define QM_REG_PQOTHER2PF_11_RT_OFFSET					29500
#define QM_REG_PQOTHER2PF_12_RT_OFFSET					29501
#define QM_REG_PQOTHER2PF_13_RT_OFFSET					29502
#define QM_REG_PQOTHER2PF_14_RT_OFFSET					29503
#define QM_REG_PQOTHER2PF_15_RT_OFFSET					29504
#define QM_REG_RLGLBLPERIOD_0_RT_OFFSET					29505
#define QM_REG_RLGLBLPERIOD_1_RT_OFFSET					29506
#define QM_REG_RLGLBLPERIODTIMER_0_RT_OFFSET				29507
#define QM_REG_RLGLBLPERIODTIMER_1_RT_OFFSET				29508
#define QM_REG_RLGLBLPERIODSEL_0_RT_OFFSET				29509
#define QM_REG_RLGLBLPERIODSEL_1_RT_OFFSET				29510
#define QM_REG_RLGLBLPERIODSEL_2_RT_OFFSET				29511
#define QM_REG_RLGLBLPERIODSEL_3_RT_OFFSET				29512
#define QM_REG_RLGLBLPERIODSEL_4_RT_OFFSET				29513
#define QM_REG_RLGLBLPERIODSEL_5_RT_OFFSET				29514
#define QM_REG_RLGLBLPERIODSEL_6_RT_OFFSET				29515
#define QM_REG_RLGLBLPERIODSEL_7_RT_OFFSET				29516
#define QM_REG_RLGLBLINCVAL_RT_OFFSET					29517
#define QM_REG_RLGLBLINCVAL_RT_SIZE					256
#define QM_REG_RLGLBLUPPERBOUND_RT_OFFSET				29773
#define QM_REG_RLGLBLUPPERBOUND_RT_SIZE					256
#define QM_REG_RLGLBLCRD_RT_OFFSET					30029
#define QM_REG_RLGLBLCRD_RT_SIZE					256
#define QM_REG_RLGLBLENABLE_RT_OFFSET					30285
#define QM_REG_RLPFPERIOD_RT_OFFSET					30286
#define QM_REG_RLPFPERIODTIMER_RT_OFFSET				30287
#define QM_REG_RLPFINCVAL_RT_OFFSET					30288
#define QM_REG_RLPFINCVAL_RT_SIZE					16
#define QM_REG_RLPFUPPERBOUND_RT_OFFSET					30304
#define QM_REG_RLPFUPPERBOUND_RT_SIZE					16
#define QM_REG_RLPFCRD_RT_OFFSET					30320
#define QM_REG_RLPFCRD_RT_SIZE						16
#define QM_REG_RLPFENABLE_RT_OFFSET					30336
#define QM_REG_RLPFVOQENABLE_RT_OFFSET					30337
#define QM_REG_WFQPFWEIGHT_RT_OFFSET					30338
#define QM_REG_WFQPFWEIGHT_RT_SIZE					16
#define QM_REG_WFQPFUPPERBOUND_RT_OFFSET				30354
#define QM_REG_WFQPFUPPERBOUND_RT_SIZE					16
#define QM_REG_WFQPFCRD_RT_OFFSET					30370
#define QM_REG_WFQPFCRD_RT_SIZE						160
#define QM_REG_WFQPFENABLE_RT_OFFSET					30530
#define QM_REG_WFQVPENABLE_RT_OFFSET					30531
#define QM_REG_BASEADDRTXPQ_RT_OFFSET					30532
#define QM_REG_BASEADDRTXPQ_RT_SIZE					512
#define QM_REG_TXPQMAP_RT_OFFSET					31044
#define QM_REG_TXPQMAP_RT_SIZE						512
#define QM_REG_WFQVPWEIGHT_RT_OFFSET					31556
#define QM_REG_WFQVPWEIGHT_RT_SIZE					512
#define QM_REG_WFQVPUPPERBOUND_RT_OFFSET				32068
#define QM_REG_WFQVPUPPERBOUND_RT_SIZE					512
#define QM_REG_WFQVPCRD_RT_OFFSET					32580
#define QM_REG_WFQVPCRD_RT_SIZE						512
#define QM_REG_WFQVPMAP_RT_OFFSET					33092
#define QM_REG_WFQVPMAP_RT_SIZE						512
#define QM_REG_PTRTBLTX_RT_OFFSET					33604
#define QM_REG_PTRTBLTX_RT_SIZE						1024
#define QM_REG_WFQPFCRD_MSB_RT_OFFSET					34628
#define QM_REG_WFQPFCRD_MSB_RT_SIZE					160
#define NIG_REG_TAG_ETHERTYPE_0_RT_OFFSET				34788
#define NIG_REG_BRB_GATE_DNTFWD_PORT_RT_OFFSET				34789
#define NIG_REG_OUTER_TAG_VALUE_LIST0_RT_OFFSET				34790
#define NIG_REG_OUTER_TAG_VALUE_LIST1_RT_OFFSET				34791
#define NIG_REG_OUTER_TAG_VALUE_LIST2_RT_OFFSET				34792
#define NIG_REG_OUTER_TAG_VALUE_LIST3_RT_OFFSET				34793
#define NIG_REG_LLH_FUNC_TAGMAC_CLS_TYPE_RT_OFFSET			34794
#define NIG_REG_LLH_FUNC_TAG_EN_RT_OFFSET				34795
#define NIG_REG_LLH_FUNC_TAG_EN_RT_SIZE					4
#define NIG_REG_LLH_FUNC_TAG_VALUE_RT_OFFSET				34799
#define NIG_REG_LLH_FUNC_TAG_VALUE_RT_SIZE				4
#define NIG_REG_LLH_FUNC_FILTER_VALUE_RT_OFFSET				34803
#define NIG_REG_LLH_FUNC_FILTER_VALUE_RT_SIZE				32
#define NIG_REG_LLH_FUNC_FILTER_EN_RT_OFFSET				34835
#define NIG_REG_LLH_FUNC_FILTER_EN_RT_SIZE				16
#define NIG_REG_LLH_FUNC_FILTER_MODE_RT_OFFSET				34851
#define NIG_REG_LLH_FUNC_FILTER_MODE_RT_SIZE				16
#define NIG_REG_LLH_FUNC_FILTER_PROTOCOL_TYPE_RT_OFFSET			34867
#define NIG_REG_LLH_FUNC_FILTER_PROTOCOL_TYPE_RT_SIZE			16
#define NIG_REG_LLH_FUNC_FILTER_HDR_SEL_RT_OFFSET			34883
#define NIG_REG_LLH_FUNC_FILTER_HDR_SEL_RT_SIZE				16
#define NIG_REG_TX_EDPM_CTRL_RT_OFFSET					34899
#define NIG_REG_PPF_TO_ENGINE_SEL_RT_OFFSET				34900
#define NIG_REG_PPF_TO_ENGINE_SEL_RT_SIZE				8
#define CDU_REG_CID_ADDR_PARAMS_RT_OFFSET				34908
#define CDU_REG_SEGMENT0_PARAMS_RT_OFFSET				34909
#define CDU_REG_SEGMENT1_PARAMS_RT_OFFSET				34910
#define CDU_REG_PF_SEG0_TYPE_OFFSET_RT_OFFSET				34911
#define CDU_REG_PF_SEG1_TYPE_OFFSET_RT_OFFSET				34912
#define CDU_REG_PF_SEG2_TYPE_OFFSET_RT_OFFSET				34913
#define CDU_REG_PF_SEG3_TYPE_OFFSET_RT_OFFSET				34914
#define CDU_REG_PF_FL_SEG0_TYPE_OFFSET_RT_OFFSET			34915
#define CDU_REG_PF_FL_SEG1_TYPE_OFFSET_RT_OFFSET			34916
#define CDU_REG_PF_FL_SEG2_TYPE_OFFSET_RT_OFFSET			34917
#define CDU_REG_PF_FL_SEG3_TYPE_OFFSET_RT_OFFSET			34918
#define CDU_REG_VF_SEG_TYPE_OFFSET_RT_OFFSET				34919
#define CDU_REG_VF_FL_SEG_TYPE_OFFSET_RT_OFFSET				34920
#define PBF_REG_TAG_ETHERTYPE_0_RT_OFFSET				34921
#define PBF_REG_BTB_SHARED_AREA_SIZE_RT_OFFSET				34922
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ0_RT_OFFSET			34923
#define PBF_REG_BTB_GUARANTEED_VOQ0_RT_OFFSET				34924
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ0_RT_OFFSET			34925
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ1_RT_OFFSET			34926
#define PBF_REG_BTB_GUARANTEED_VOQ1_RT_OFFSET				34927
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ1_RT_OFFSET			34928
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ2_RT_OFFSET			34929
#define PBF_REG_BTB_GUARANTEED_VOQ2_RT_OFFSET				34930
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ2_RT_OFFSET			34931
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ3_RT_OFFSET			34932
#define PBF_REG_BTB_GUARANTEED_VOQ3_RT_OFFSET				34933
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ3_RT_OFFSET			34934
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ4_RT_OFFSET			34935
#define PBF_REG_BTB_GUARANTEED_VOQ4_RT_OFFSET				34936
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ4_RT_OFFSET			34937
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ5_RT_OFFSET			34938
#define PBF_REG_BTB_GUARANTEED_VOQ5_RT_OFFSET				34939
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ5_RT_OFFSET			34940
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ6_RT_OFFSET			34941
#define PBF_REG_BTB_GUARANTEED_VOQ6_RT_OFFSET				34942
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ6_RT_OFFSET			34943
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ7_RT_OFFSET			34944
#define PBF_REG_BTB_GUARANTEED_VOQ7_RT_OFFSET				34945
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ7_RT_OFFSET			34946
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ8_RT_OFFSET			34947
#define PBF_REG_BTB_GUARANTEED_VOQ8_RT_OFFSET				34948
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ8_RT_OFFSET			34949
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ9_RT_OFFSET			34950
#define PBF_REG_BTB_GUARANTEED_VOQ9_RT_OFFSET				34951
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ9_RT_OFFSET			34952
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ10_RT_OFFSET			34953
#define PBF_REG_BTB_GUARANTEED_VOQ10_RT_OFFSET				34954
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ10_RT_OFFSET			34955
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ11_RT_OFFSET			34956
#define PBF_REG_BTB_GUARANTEED_VOQ11_RT_OFFSET				34957
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ11_RT_OFFSET			34958
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ12_RT_OFFSET			34959
#define PBF_REG_BTB_GUARANTEED_VOQ12_RT_OFFSET				34960
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ12_RT_OFFSET			34961
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ13_RT_OFFSET			34962
#define PBF_REG_BTB_GUARANTEED_VOQ13_RT_OFFSET				34963
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ13_RT_OFFSET			34964
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ14_RT_OFFSET			34965
#define PBF_REG_BTB_GUARANTEED_VOQ14_RT_OFFSET				34966
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ14_RT_OFFSET			34967
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ15_RT_OFFSET			34968
#define PBF_REG_BTB_GUARANTEED_VOQ15_RT_OFFSET				34969
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ15_RT_OFFSET			34970
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ16_RT_OFFSET			34971
#define PBF_REG_BTB_GUARANTEED_VOQ16_RT_OFFSET				34972
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ16_RT_OFFSET			34973
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ17_RT_OFFSET			34974
#define PBF_REG_BTB_GUARANTEED_VOQ17_RT_OFFSET				34975
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ17_RT_OFFSET			34976
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ18_RT_OFFSET			34977
#define PBF_REG_BTB_GUARANTEED_VOQ18_RT_OFFSET				34978
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ18_RT_OFFSET			34979
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ19_RT_OFFSET			34980
#define PBF_REG_BTB_GUARANTEED_VOQ19_RT_OFFSET				34981
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ19_RT_OFFSET			34982
#define XCM_REG_CON_PHY_Q3_RT_OFFSET					34983

#define RUNTIME_ARRAY_SIZE						34984

/* Init Callbacks */
#define DMAE_READY_CB							0

#endif /* __RT_DEFS_H__ */

/************************************************************************/
/* Add include to common eth target for both eCore and protocol driver */
/************************************************************************/

/* The eth storm context for the Tstorm */
struct tstorm_eth_conn_st_ctx {
	__le32 reserved[4];
};

/* The eth storm context for the Pstorm */
struct pstorm_eth_conn_st_ctx {
	__le32 reserved[8];
};

/* The eth storm context for the Xstorm */
struct xstorm_eth_conn_st_ctx {
	__le32 reserved[60];
};

struct xstorm_eth_conn_ag_ctx {
	u8 reserved0;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define XSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define XSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define XSTORM_ETH_CONN_AG_CTX_RESERVED1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED1_SHIFT			1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED2_MASK			0x1	/* HSI_COMMENT: exist_in_qm2 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED2_SHIFT			2
#define XSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM3_MASK		0x1	/* HSI_COMMENT: exist_in_qm3 */
#define XSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM3_SHIFT		3
#define XSTORM_ETH_CONN_AG_CTX_RESERVED3_MASK			0x1	/* HSI_COMMENT: bit4 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED3_SHIFT			4
#define XSTORM_ETH_CONN_AG_CTX_RESERVED4_MASK			0x1	/* HSI_COMMENT: cf_array_active */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED4_SHIFT			5
#define XSTORM_ETH_CONN_AG_CTX_RESERVED5_MASK			0x1	/* HSI_COMMENT: bit6 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED5_SHIFT			6
#define XSTORM_ETH_CONN_AG_CTX_RESERVED6_MASK			0x1	/* HSI_COMMENT: bit7 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED6_SHIFT			7
	u8 flags1;
#define XSTORM_ETH_CONN_AG_CTX_RESERVED7_MASK			0x1	/* HSI_COMMENT: bit8 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED7_SHIFT			0
#define XSTORM_ETH_CONN_AG_CTX_RESERVED8_MASK			0x1	/* HSI_COMMENT: bit9 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED8_SHIFT			1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED9_MASK			0x1	/* HSI_COMMENT: bit10 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED9_SHIFT			2
#define XSTORM_ETH_CONN_AG_CTX_BIT11_MASK			0x1	/* HSI_COMMENT: bit11 */
#define XSTORM_ETH_CONN_AG_CTX_BIT11_SHIFT			3
#define XSTORM_ETH_CONN_AG_CTX_E5_RESERVED2_MASK		0x1	/* HSI_COMMENT: bit12 */
#define XSTORM_ETH_CONN_AG_CTX_E5_RESERVED2_SHIFT		4
#define XSTORM_ETH_CONN_AG_CTX_E5_RESERVED3_MASK		0x1	/* HSI_COMMENT: bit13 */
#define XSTORM_ETH_CONN_AG_CTX_E5_RESERVED3_SHIFT		5
#define XSTORM_ETH_CONN_AG_CTX_TX_RULE_ACTIVE_MASK		0x1	/* HSI_COMMENT: bit14 */
#define XSTORM_ETH_CONN_AG_CTX_TX_RULE_ACTIVE_SHIFT		6
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_ACTIVE_MASK		0x1	/* HSI_COMMENT: bit15 */
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_ACTIVE_SHIFT		7
	u8 flags2;
#define XSTORM_ETH_CONN_AG_CTX_CF0_MASK				0x3	/* HSI_COMMENT: timer0cf */
#define XSTORM_ETH_CONN_AG_CTX_CF0_SHIFT			0
#define XSTORM_ETH_CONN_AG_CTX_CF1_MASK				0x3	/* HSI_COMMENT: timer1cf */
#define XSTORM_ETH_CONN_AG_CTX_CF1_SHIFT			2
#define XSTORM_ETH_CONN_AG_CTX_CF2_MASK				0x3	/* HSI_COMMENT: timer2cf */
#define XSTORM_ETH_CONN_AG_CTX_CF2_SHIFT			4
#define XSTORM_ETH_CONN_AG_CTX_CF3_MASK				0x3	/* HSI_COMMENT: timer_stop_all */
#define XSTORM_ETH_CONN_AG_CTX_CF3_SHIFT			6
	u8 flags3;
#define XSTORM_ETH_CONN_AG_CTX_CF4_MASK				0x3	/* HSI_COMMENT: cf4 */
#define XSTORM_ETH_CONN_AG_CTX_CF4_SHIFT			0
#define XSTORM_ETH_CONN_AG_CTX_CF5_MASK				0x3	/* HSI_COMMENT: cf5 */
#define XSTORM_ETH_CONN_AG_CTX_CF5_SHIFT			2
#define XSTORM_ETH_CONN_AG_CTX_CF6_MASK				0x3	/* HSI_COMMENT: cf6 */
#define XSTORM_ETH_CONN_AG_CTX_CF6_SHIFT			4
#define XSTORM_ETH_CONN_AG_CTX_CF7_MASK				0x3	/* HSI_COMMENT: cf7 */
#define XSTORM_ETH_CONN_AG_CTX_CF7_SHIFT			6
	u8 flags4;
#define XSTORM_ETH_CONN_AG_CTX_CF8_MASK				0x3	/* HSI_COMMENT: cf8 */
#define XSTORM_ETH_CONN_AG_CTX_CF8_SHIFT			0
#define XSTORM_ETH_CONN_AG_CTX_CF9_MASK				0x3	/* HSI_COMMENT: cf9 */
#define XSTORM_ETH_CONN_AG_CTX_CF9_SHIFT			2
#define XSTORM_ETH_CONN_AG_CTX_CF10_MASK			0x3	/* HSI_COMMENT: cf10 */
#define XSTORM_ETH_CONN_AG_CTX_CF10_SHIFT			4
#define XSTORM_ETH_CONN_AG_CTX_CF11_MASK			0x3	/* HSI_COMMENT: cf11 */
#define XSTORM_ETH_CONN_AG_CTX_CF11_SHIFT			6
	u8 flags5;
#define XSTORM_ETH_CONN_AG_CTX_CF12_MASK			0x3	/* HSI_COMMENT: cf12 */
#define XSTORM_ETH_CONN_AG_CTX_CF12_SHIFT			0
#define XSTORM_ETH_CONN_AG_CTX_CF13_MASK			0x3	/* HSI_COMMENT: cf13 */
#define XSTORM_ETH_CONN_AG_CTX_CF13_SHIFT			2
#define XSTORM_ETH_CONN_AG_CTX_CF14_MASK			0x3	/* HSI_COMMENT: cf14 */
#define XSTORM_ETH_CONN_AG_CTX_CF14_SHIFT			4
#define XSTORM_ETH_CONN_AG_CTX_CF15_MASK			0x3	/* HSI_COMMENT: cf15 */
#define XSTORM_ETH_CONN_AG_CTX_CF15_SHIFT			6
	u8 flags6;
#define XSTORM_ETH_CONN_AG_CTX_GO_TO_BD_CONS_CF_MASK		0x3	/* HSI_COMMENT: cf16 */
#define XSTORM_ETH_CONN_AG_CTX_GO_TO_BD_CONS_CF_SHIFT		0
#define XSTORM_ETH_CONN_AG_CTX_MULTI_UNICAST_CF_MASK		0x3	/* HSI_COMMENT: cf_array_cf */
#define XSTORM_ETH_CONN_AG_CTX_MULTI_UNICAST_CF_SHIFT		2
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_MASK			0x3	/* HSI_COMMENT: cf18 */
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_SHIFT			4
#define XSTORM_ETH_CONN_AG_CTX_TERMINATE_CF_MASK		0x3	/* HSI_COMMENT: cf19 */
#define XSTORM_ETH_CONN_AG_CTX_TERMINATE_CF_SHIFT		6
	u8 flags7;
#define XSTORM_ETH_CONN_AG_CTX_FLUSH_Q0_MASK			0x3	/* HSI_COMMENT: cf20 */
#define XSTORM_ETH_CONN_AG_CTX_FLUSH_Q0_SHIFT			0
#define XSTORM_ETH_CONN_AG_CTX_RESERVED10_MASK			0x3	/* HSI_COMMENT: cf21 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED10_SHIFT			2
#define XSTORM_ETH_CONN_AG_CTX_SLOW_PATH_MASK			0x3	/* HSI_COMMENT: cf22 */
#define XSTORM_ETH_CONN_AG_CTX_SLOW_PATH_SHIFT			4
#define XSTORM_ETH_CONN_AG_CTX_CF0EN_MASK			0x1	/* HSI_COMMENT: cf0en */
#define XSTORM_ETH_CONN_AG_CTX_CF0EN_SHIFT			6
#define XSTORM_ETH_CONN_AG_CTX_CF1EN_MASK			0x1	/* HSI_COMMENT: cf1en */
#define XSTORM_ETH_CONN_AG_CTX_CF1EN_SHIFT			7
	u8 flags8;
#define XSTORM_ETH_CONN_AG_CTX_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define XSTORM_ETH_CONN_AG_CTX_CF2EN_SHIFT			0
#define XSTORM_ETH_CONN_AG_CTX_CF3EN_MASK			0x1	/* HSI_COMMENT: cf3en */
#define XSTORM_ETH_CONN_AG_CTX_CF3EN_SHIFT			1
#define XSTORM_ETH_CONN_AG_CTX_CF4EN_MASK			0x1	/* HSI_COMMENT: cf4en */
#define XSTORM_ETH_CONN_AG_CTX_CF4EN_SHIFT			2
#define XSTORM_ETH_CONN_AG_CTX_CF5EN_MASK			0x1	/* HSI_COMMENT: cf5en */
#define XSTORM_ETH_CONN_AG_CTX_CF5EN_SHIFT			3
#define XSTORM_ETH_CONN_AG_CTX_CF6EN_MASK			0x1	/* HSI_COMMENT: cf6en */
#define XSTORM_ETH_CONN_AG_CTX_CF6EN_SHIFT			4
#define XSTORM_ETH_CONN_AG_CTX_CF7EN_MASK			0x1	/* HSI_COMMENT: cf7en */
#define XSTORM_ETH_CONN_AG_CTX_CF7EN_SHIFT			5
#define XSTORM_ETH_CONN_AG_CTX_CF8EN_MASK			0x1	/* HSI_COMMENT: cf8en */
#define XSTORM_ETH_CONN_AG_CTX_CF8EN_SHIFT			6
#define XSTORM_ETH_CONN_AG_CTX_CF9EN_MASK			0x1	/* HSI_COMMENT: cf9en */
#define XSTORM_ETH_CONN_AG_CTX_CF9EN_SHIFT			7
	u8 flags9;
#define XSTORM_ETH_CONN_AG_CTX_CF10EN_MASK			0x1	/* HSI_COMMENT: cf10en */
#define XSTORM_ETH_CONN_AG_CTX_CF10EN_SHIFT			0
#define XSTORM_ETH_CONN_AG_CTX_CF11EN_MASK			0x1	/* HSI_COMMENT: cf11en */
#define XSTORM_ETH_CONN_AG_CTX_CF11EN_SHIFT			1
#define XSTORM_ETH_CONN_AG_CTX_CF12EN_MASK			0x1	/* HSI_COMMENT: cf12en */
#define XSTORM_ETH_CONN_AG_CTX_CF12EN_SHIFT			2
#define XSTORM_ETH_CONN_AG_CTX_CF13EN_MASK			0x1	/* HSI_COMMENT: cf13en */
#define XSTORM_ETH_CONN_AG_CTX_CF13EN_SHIFT			3
#define XSTORM_ETH_CONN_AG_CTX_CF14EN_MASK			0x1	/* HSI_COMMENT: cf14en */
#define XSTORM_ETH_CONN_AG_CTX_CF14EN_SHIFT			4
#define XSTORM_ETH_CONN_AG_CTX_CF15EN_MASK			0x1	/* HSI_COMMENT: cf15en */
#define XSTORM_ETH_CONN_AG_CTX_CF15EN_SHIFT			5
#define XSTORM_ETH_CONN_AG_CTX_GO_TO_BD_CONS_CF_EN_MASK		0x1	/* HSI_COMMENT: cf16en */
#define XSTORM_ETH_CONN_AG_CTX_GO_TO_BD_CONS_CF_EN_SHIFT	6
#define XSTORM_ETH_CONN_AG_CTX_MULTI_UNICAST_CF_EN_MASK		0x1	/* HSI_COMMENT: cf_array_cf_en */
#define XSTORM_ETH_CONN_AG_CTX_MULTI_UNICAST_CF_EN_SHIFT	7
	u8 flags10;
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_EN_MASK			0x1	/* HSI_COMMENT: cf18en */
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_EN_SHIFT			0
#define XSTORM_ETH_CONN_AG_CTX_TERMINATE_CF_EN_MASK		0x1	/* HSI_COMMENT: cf19en */
#define XSTORM_ETH_CONN_AG_CTX_TERMINATE_CF_EN_SHIFT		1
#define XSTORM_ETH_CONN_AG_CTX_FLUSH_Q0_EN_MASK			0x1	/* HSI_COMMENT: cf20en */
#define XSTORM_ETH_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT		2
#define XSTORM_ETH_CONN_AG_CTX_RESERVED11_MASK			0x1	/* HSI_COMMENT: cf21en */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED11_SHIFT			3
#define XSTORM_ETH_CONN_AG_CTX_SLOW_PATH_EN_MASK		0x1	/* HSI_COMMENT: cf22en */
#define XSTORM_ETH_CONN_AG_CTX_SLOW_PATH_EN_SHIFT		4
#define XSTORM_ETH_CONN_AG_CTX_TPH_ENABLE_EN_RESERVED_MASK	0x1	/* HSI_COMMENT: cf23en */
#define XSTORM_ETH_CONN_AG_CTX_TPH_ENABLE_EN_RESERVED_SHIFT	5
#define XSTORM_ETH_CONN_AG_CTX_RESERVED12_MASK			0x1	/* HSI_COMMENT: rule0en */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED12_SHIFT			6
#define XSTORM_ETH_CONN_AG_CTX_RESERVED13_MASK			0x1	/* HSI_COMMENT: rule1en */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED13_SHIFT			7
	u8 flags11;
#define XSTORM_ETH_CONN_AG_CTX_RESERVED14_MASK			0x1	/* HSI_COMMENT: rule2en */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED14_SHIFT			0
#define XSTORM_ETH_CONN_AG_CTX_RESERVED15_MASK			0x1	/* HSI_COMMENT: rule3en */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED15_SHIFT			1
#define XSTORM_ETH_CONN_AG_CTX_TX_DEC_RULE_EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define XSTORM_ETH_CONN_AG_CTX_TX_DEC_RULE_EN_SHIFT		2
#define XSTORM_ETH_CONN_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define XSTORM_ETH_CONN_AG_CTX_RULE5EN_SHIFT			3
#define XSTORM_ETH_CONN_AG_CTX_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define XSTORM_ETH_CONN_AG_CTX_RULE6EN_SHIFT			4
#define XSTORM_ETH_CONN_AG_CTX_RULE7EN_MASK			0x1	/* HSI_COMMENT: rule7en */
#define XSTORM_ETH_CONN_AG_CTX_RULE7EN_SHIFT			5
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED1_MASK		0x1	/* HSI_COMMENT: rule8en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED1_SHIFT		6
#define XSTORM_ETH_CONN_AG_CTX_RULE9EN_MASK			0x1	/* HSI_COMMENT: rule9en */
#define XSTORM_ETH_CONN_AG_CTX_RULE9EN_SHIFT			7
	u8 flags12;
#define XSTORM_ETH_CONN_AG_CTX_RULE10EN_MASK			0x1	/* HSI_COMMENT: rule10en */
#define XSTORM_ETH_CONN_AG_CTX_RULE10EN_SHIFT			0
#define XSTORM_ETH_CONN_AG_CTX_RULE11EN_MASK			0x1	/* HSI_COMMENT: rule11en */
#define XSTORM_ETH_CONN_AG_CTX_RULE11EN_SHIFT			1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED2_MASK		0x1	/* HSI_COMMENT: rule12en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED2_SHIFT		2
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED3_MASK		0x1	/* HSI_COMMENT: rule13en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED3_SHIFT		3
#define XSTORM_ETH_CONN_AG_CTX_RULE14EN_MASK			0x1	/* HSI_COMMENT: rule14en */
#define XSTORM_ETH_CONN_AG_CTX_RULE14EN_SHIFT			4
#define XSTORM_ETH_CONN_AG_CTX_RULE15EN_MASK			0x1	/* HSI_COMMENT: rule15en */
#define XSTORM_ETH_CONN_AG_CTX_RULE15EN_SHIFT			5
#define XSTORM_ETH_CONN_AG_CTX_RULE16EN_MASK			0x1	/* HSI_COMMENT: rule16en */
#define XSTORM_ETH_CONN_AG_CTX_RULE16EN_SHIFT			6
#define XSTORM_ETH_CONN_AG_CTX_RULE17EN_MASK			0x1	/* HSI_COMMENT: rule17en */
#define XSTORM_ETH_CONN_AG_CTX_RULE17EN_SHIFT			7
	u8 flags13;
#define XSTORM_ETH_CONN_AG_CTX_RULE18EN_MASK			0x1	/* HSI_COMMENT: rule18en */
#define XSTORM_ETH_CONN_AG_CTX_RULE18EN_SHIFT			0
#define XSTORM_ETH_CONN_AG_CTX_RULE19EN_MASK			0x1	/* HSI_COMMENT: rule19en */
#define XSTORM_ETH_CONN_AG_CTX_RULE19EN_SHIFT			1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED4_MASK		0x1	/* HSI_COMMENT: rule20en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED4_SHIFT		2
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED5_MASK		0x1	/* HSI_COMMENT: rule21en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED5_SHIFT		3
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED6_MASK		0x1	/* HSI_COMMENT: rule22en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED6_SHIFT		4
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED7_MASK		0x1	/* HSI_COMMENT: rule23en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED7_SHIFT		5
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED8_MASK		0x1	/* HSI_COMMENT: rule24en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED8_SHIFT		6
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED9_MASK		0x1	/* HSI_COMMENT: rule25en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED9_SHIFT		7
	u8 flags14;
#define XSTORM_ETH_CONN_AG_CTX_EDPM_USE_EXT_HDR_MASK		0x1	/* HSI_COMMENT: bit16 */
#define XSTORM_ETH_CONN_AG_CTX_EDPM_USE_EXT_HDR_SHIFT		0
#define XSTORM_ETH_CONN_AG_CTX_EDPM_SEND_RAW_L3L4_MASK		0x1	/* HSI_COMMENT: bit17 */
#define XSTORM_ETH_CONN_AG_CTX_EDPM_SEND_RAW_L3L4_SHIFT		1
#define XSTORM_ETH_CONN_AG_CTX_EDPM_INBAND_PROP_HDR_MASK	0x1	/* HSI_COMMENT: bit18 */
#define XSTORM_ETH_CONN_AG_CTX_EDPM_INBAND_PROP_HDR_SHIFT	2
#define XSTORM_ETH_CONN_AG_CTX_EDPM_SEND_EXT_TUNNEL_MASK	0x1	/* HSI_COMMENT: bit19 */
#define XSTORM_ETH_CONN_AG_CTX_EDPM_SEND_EXT_TUNNEL_SHIFT	3
#define XSTORM_ETH_CONN_AG_CTX_L2_EDPM_ENABLE_MASK		0x1	/* HSI_COMMENT: bit20 */
#define XSTORM_ETH_CONN_AG_CTX_L2_EDPM_ENABLE_SHIFT		4
#define XSTORM_ETH_CONN_AG_CTX_ROCE_EDPM_ENABLE_MASK		0x1	/* HSI_COMMENT: bit21 */
#define XSTORM_ETH_CONN_AG_CTX_ROCE_EDPM_ENABLE_SHIFT		5
#define XSTORM_ETH_CONN_AG_CTX_TPH_ENABLE_MASK			0x3	/* HSI_COMMENT: cf23 */
#define XSTORM_ETH_CONN_AG_CTX_TPH_ENABLE_SHIFT			6
	u8 edpm_event_id;	/* HSI_COMMENT: byte2 */
	__le16 physical_q0;	/* HSI_COMMENT: physical_q0 */
	__le16 e5_reserved1;	/* HSI_COMMENT: physical_q1 */
	__le16 edpm_num_bds;	/* HSI_COMMENT: physical_q2 */
	__le16 tx_bd_cons;	/* HSI_COMMENT: word3 */
	__le16 tx_bd_prod;	/* HSI_COMMENT: word4 */
	__le16 updated_qm_pq_id;	/* HSI_COMMENT: word5 */
	__le16 conn_dpi;	/* HSI_COMMENT: conn_dpi */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	u8 byte5;		/* HSI_COMMENT: byte5 */
	u8 byte6;		/* HSI_COMMENT: byte6 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
	__le32 reg5;		/* HSI_COMMENT: cf_array0 */
	__le32 reg6;		/* HSI_COMMENT: cf_array1 */
	__le16 word7;		/* HSI_COMMENT: word7 */
	__le16 word8;		/* HSI_COMMENT: word8 */
	__le16 word9;		/* HSI_COMMENT: word9 */
	__le16 word10;		/* HSI_COMMENT: word10 */
	__le32 reg7;		/* HSI_COMMENT: reg7 */
	__le32 reg8;		/* HSI_COMMENT: reg8 */
	__le32 reg9;		/* HSI_COMMENT: reg9 */
	u8 byte7;		/* HSI_COMMENT: byte7 */
	u8 byte8;		/* HSI_COMMENT: byte8 */
	u8 byte9;		/* HSI_COMMENT: byte9 */
	u8 byte10;		/* HSI_COMMENT: byte10 */
	u8 byte11;		/* HSI_COMMENT: byte11 */
	u8 byte12;		/* HSI_COMMENT: byte12 */
	u8 byte13;		/* HSI_COMMENT: byte13 */
	u8 byte14;		/* HSI_COMMENT: byte14 */
	u8 byte15;		/* HSI_COMMENT: byte15 */
	u8 e5_reserved;		/* HSI_COMMENT: e5_reserved */
	__le16 word11;		/* HSI_COMMENT: word11 */
	__le32 reg10;		/* HSI_COMMENT: reg10 */
	__le32 reg11;		/* HSI_COMMENT: reg11 */
	__le32 reg12;		/* HSI_COMMENT: reg12 */
	__le32 reg13;		/* HSI_COMMENT: reg13 */
	__le32 reg14;		/* HSI_COMMENT: reg14 */
	__le32 reg15;		/* HSI_COMMENT: reg15 */
	__le32 reg16;		/* HSI_COMMENT: reg16 */
	__le32 reg17;		/* HSI_COMMENT: reg17 */
	__le32 reg18;		/* HSI_COMMENT: reg18 */
	__le32 reg19;		/* HSI_COMMENT: reg19 */
	__le16 word12;		/* HSI_COMMENT: word12 */
	__le16 word13;		/* HSI_COMMENT: word13 */
	__le16 word14;		/* HSI_COMMENT: word14 */
	__le16 word15;		/* HSI_COMMENT: word15 */
};

struct tstorm_eth_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define TSTORM_ETH_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define TSTORM_ETH_CONN_AG_CTX_BIT0_SHIFT		0
#define TSTORM_ETH_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define TSTORM_ETH_CONN_AG_CTX_BIT1_SHIFT		1
#define TSTORM_ETH_CONN_AG_CTX_BIT2_MASK		0x1	/* HSI_COMMENT: bit2 */
#define TSTORM_ETH_CONN_AG_CTX_BIT2_SHIFT		2
#define TSTORM_ETH_CONN_AG_CTX_BIT3_MASK		0x1	/* HSI_COMMENT: bit3 */
#define TSTORM_ETH_CONN_AG_CTX_BIT3_SHIFT		3
#define TSTORM_ETH_CONN_AG_CTX_BIT4_MASK		0x1	/* HSI_COMMENT: bit4 */
#define TSTORM_ETH_CONN_AG_CTX_BIT4_SHIFT		4
#define TSTORM_ETH_CONN_AG_CTX_BIT5_MASK		0x1	/* HSI_COMMENT: bit5 */
#define TSTORM_ETH_CONN_AG_CTX_BIT5_SHIFT		5
#define TSTORM_ETH_CONN_AG_CTX_CF0_MASK			0x3	/* HSI_COMMENT: timer0cf */
#define TSTORM_ETH_CONN_AG_CTX_CF0_SHIFT		6
	u8 flags1;
#define TSTORM_ETH_CONN_AG_CTX_CF1_MASK			0x3	/* HSI_COMMENT: timer1cf */
#define TSTORM_ETH_CONN_AG_CTX_CF1_SHIFT		0
#define TSTORM_ETH_CONN_AG_CTX_CF2_MASK			0x3	/* HSI_COMMENT: timer2cf */
#define TSTORM_ETH_CONN_AG_CTX_CF2_SHIFT		2
#define TSTORM_ETH_CONN_AG_CTX_CF3_MASK			0x3	/* HSI_COMMENT: timer_stop_all */
#define TSTORM_ETH_CONN_AG_CTX_CF3_SHIFT		4
#define TSTORM_ETH_CONN_AG_CTX_CF4_MASK			0x3	/* HSI_COMMENT: cf4 */
#define TSTORM_ETH_CONN_AG_CTX_CF4_SHIFT		6
	u8 flags2;
#define TSTORM_ETH_CONN_AG_CTX_CF5_MASK			0x3	/* HSI_COMMENT: cf5 */
#define TSTORM_ETH_CONN_AG_CTX_CF5_SHIFT		0
#define TSTORM_ETH_CONN_AG_CTX_CF6_MASK			0x3	/* HSI_COMMENT: cf6 */
#define TSTORM_ETH_CONN_AG_CTX_CF6_SHIFT		2
#define TSTORM_ETH_CONN_AG_CTX_CF7_MASK			0x3	/* HSI_COMMENT: cf7 */
#define TSTORM_ETH_CONN_AG_CTX_CF7_SHIFT		4
#define TSTORM_ETH_CONN_AG_CTX_CF8_MASK			0x3	/* HSI_COMMENT: cf8 */
#define TSTORM_ETH_CONN_AG_CTX_CF8_SHIFT		6
	u8 flags3;
#define TSTORM_ETH_CONN_AG_CTX_CF9_MASK			0x3	/* HSI_COMMENT: cf9 */
#define TSTORM_ETH_CONN_AG_CTX_CF9_SHIFT		0
#define TSTORM_ETH_CONN_AG_CTX_CF10_MASK		0x3	/* HSI_COMMENT: cf10 */
#define TSTORM_ETH_CONN_AG_CTX_CF10_SHIFT		2
#define TSTORM_ETH_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define TSTORM_ETH_CONN_AG_CTX_CF0EN_SHIFT		4
#define TSTORM_ETH_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define TSTORM_ETH_CONN_AG_CTX_CF1EN_SHIFT		5
#define TSTORM_ETH_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define TSTORM_ETH_CONN_AG_CTX_CF2EN_SHIFT		6
#define TSTORM_ETH_CONN_AG_CTX_CF3EN_MASK		0x1	/* HSI_COMMENT: cf3en */
#define TSTORM_ETH_CONN_AG_CTX_CF3EN_SHIFT		7
	u8 flags4;
#define TSTORM_ETH_CONN_AG_CTX_CF4EN_MASK		0x1	/* HSI_COMMENT: cf4en */
#define TSTORM_ETH_CONN_AG_CTX_CF4EN_SHIFT		0
#define TSTORM_ETH_CONN_AG_CTX_CF5EN_MASK		0x1	/* HSI_COMMENT: cf5en */
#define TSTORM_ETH_CONN_AG_CTX_CF5EN_SHIFT		1
#define TSTORM_ETH_CONN_AG_CTX_CF6EN_MASK		0x1	/* HSI_COMMENT: cf6en */
#define TSTORM_ETH_CONN_AG_CTX_CF6EN_SHIFT		2
#define TSTORM_ETH_CONN_AG_CTX_CF7EN_MASK		0x1	/* HSI_COMMENT: cf7en */
#define TSTORM_ETH_CONN_AG_CTX_CF7EN_SHIFT		3
#define TSTORM_ETH_CONN_AG_CTX_CF8EN_MASK		0x1	/* HSI_COMMENT: cf8en */
#define TSTORM_ETH_CONN_AG_CTX_CF8EN_SHIFT		4
#define TSTORM_ETH_CONN_AG_CTX_CF9EN_MASK		0x1	/* HSI_COMMENT: cf9en */
#define TSTORM_ETH_CONN_AG_CTX_CF9EN_SHIFT		5
#define TSTORM_ETH_CONN_AG_CTX_CF10EN_MASK		0x1	/* HSI_COMMENT: cf10en */
#define TSTORM_ETH_CONN_AG_CTX_CF10EN_SHIFT		6
#define TSTORM_ETH_CONN_AG_CTX_RULE0EN_MASK		0x1	/* HSI_COMMENT: rule0en */
#define TSTORM_ETH_CONN_AG_CTX_RULE0EN_SHIFT		7
	u8 flags5;
#define TSTORM_ETH_CONN_AG_CTX_RULE1EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define TSTORM_ETH_CONN_AG_CTX_RULE1EN_SHIFT		0
#define TSTORM_ETH_CONN_AG_CTX_RULE2EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define TSTORM_ETH_CONN_AG_CTX_RULE2EN_SHIFT		1
#define TSTORM_ETH_CONN_AG_CTX_RULE3EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define TSTORM_ETH_CONN_AG_CTX_RULE3EN_SHIFT		2
#define TSTORM_ETH_CONN_AG_CTX_RULE4EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define TSTORM_ETH_CONN_AG_CTX_RULE4EN_SHIFT		3
#define TSTORM_ETH_CONN_AG_CTX_RULE5EN_MASK		0x1	/* HSI_COMMENT: rule5en */
#define TSTORM_ETH_CONN_AG_CTX_RULE5EN_SHIFT		4
#define TSTORM_ETH_CONN_AG_CTX_RX_BD_EN_MASK		0x1	/* HSI_COMMENT: rule6en */
#define TSTORM_ETH_CONN_AG_CTX_RX_BD_EN_SHIFT		5
#define TSTORM_ETH_CONN_AG_CTX_RULE7EN_MASK		0x1	/* HSI_COMMENT: rule7en */
#define TSTORM_ETH_CONN_AG_CTX_RULE7EN_SHIFT		6
#define TSTORM_ETH_CONN_AG_CTX_RULE8EN_MASK		0x1	/* HSI_COMMENT: rule8en */
#define TSTORM_ETH_CONN_AG_CTX_RULE8EN_SHIFT		7
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
	__le32 reg5;		/* HSI_COMMENT: reg5 */
	__le32 reg6;		/* HSI_COMMENT: reg6 */
	__le32 reg7;		/* HSI_COMMENT: reg7 */
	__le32 reg8;		/* HSI_COMMENT: reg8 */
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 rx_bd_cons;	/* HSI_COMMENT: word0 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	u8 byte5;		/* HSI_COMMENT: byte5 */
	__le16 rx_bd_prod;	/* HSI_COMMENT: word1 */
	__le16 word2;		/* HSI_COMMENT: conn_dpi */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le32 reg9;		/* HSI_COMMENT: reg9 */
	__le32 reg10;		/* HSI_COMMENT: reg10 */
};

/* The eth storm context for the Ystorm */
struct ystorm_eth_conn_st_ctx {
	__le32 reserved[8];
};

struct ystorm_eth_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define YSTORM_ETH_CONN_AG_CTX_BIT0_MASK			0x1	/* HSI_COMMENT: exist_in_qm0 */
#define YSTORM_ETH_CONN_AG_CTX_BIT0_SHIFT			0
#define YSTORM_ETH_CONN_AG_CTX_BIT1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define YSTORM_ETH_CONN_AG_CTX_BIT1_SHIFT			1
#define YSTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_MASK		0x3	/* HSI_COMMENT: cf0 */
#define YSTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_SHIFT		2
#define YSTORM_ETH_CONN_AG_CTX_PMD_TERMINATE_CF_MASK		0x3	/* HSI_COMMENT: cf1 */
#define YSTORM_ETH_CONN_AG_CTX_PMD_TERMINATE_CF_SHIFT		4
#define YSTORM_ETH_CONN_AG_CTX_CF2_MASK				0x3	/* HSI_COMMENT: cf2 */
#define YSTORM_ETH_CONN_AG_CTX_CF2_SHIFT			6
	u8 flags1;
#define YSTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_EN_MASK	0x1	/* HSI_COMMENT: cf0en */
#define YSTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_EN_SHIFT	0
#define YSTORM_ETH_CONN_AG_CTX_PMD_TERMINATE_CF_EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define YSTORM_ETH_CONN_AG_CTX_PMD_TERMINATE_CF_EN_SHIFT	1
#define YSTORM_ETH_CONN_AG_CTX_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define YSTORM_ETH_CONN_AG_CTX_CF2EN_SHIFT			2
#define YSTORM_ETH_CONN_AG_CTX_RULE0EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define YSTORM_ETH_CONN_AG_CTX_RULE0EN_SHIFT			3
#define YSTORM_ETH_CONN_AG_CTX_RULE1EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define YSTORM_ETH_CONN_AG_CTX_RULE1EN_SHIFT			4
#define YSTORM_ETH_CONN_AG_CTX_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define YSTORM_ETH_CONN_AG_CTX_RULE2EN_SHIFT			5
#define YSTORM_ETH_CONN_AG_CTX_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define YSTORM_ETH_CONN_AG_CTX_RULE3EN_SHIFT			6
#define YSTORM_ETH_CONN_AG_CTX_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define YSTORM_ETH_CONN_AG_CTX_RULE4EN_SHIFT			7
	u8 tx_q0_int_coallecing_timeset;	/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: word0 */
	__le32 terminate_spqe;	/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le16 tx_bd_cons_upd;	/* HSI_COMMENT: word1 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le16 word4;		/* HSI_COMMENT: word4 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
};

struct ustorm_eth_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define USTORM_ETH_CONN_AG_CTX_BIT0_MASK			0x1	/* HSI_COMMENT: exist_in_qm0 */
#define USTORM_ETH_CONN_AG_CTX_BIT0_SHIFT			0
#define USTORM_ETH_CONN_AG_CTX_BIT1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define USTORM_ETH_CONN_AG_CTX_BIT1_SHIFT			1
#define USTORM_ETH_CONN_AG_CTX_TX_PMD_TERMINATE_CF_MASK		0x3	/* HSI_COMMENT: timer0cf */
#define USTORM_ETH_CONN_AG_CTX_TX_PMD_TERMINATE_CF_SHIFT	2
#define USTORM_ETH_CONN_AG_CTX_RX_PMD_TERMINATE_CF_MASK		0x3	/* HSI_COMMENT: timer1cf */
#define USTORM_ETH_CONN_AG_CTX_RX_PMD_TERMINATE_CF_SHIFT	4
#define USTORM_ETH_CONN_AG_CTX_CF2_MASK				0x3	/* HSI_COMMENT: timer2cf */
#define USTORM_ETH_CONN_AG_CTX_CF2_SHIFT			6
	u8 flags1;
#define USTORM_ETH_CONN_AG_CTX_CF3_MASK				0x3	/* HSI_COMMENT: timer_stop_all */
#define USTORM_ETH_CONN_AG_CTX_CF3_SHIFT			0
#define USTORM_ETH_CONN_AG_CTX_TX_ARM_CF_MASK			0x3	/* HSI_COMMENT: cf4 */
#define USTORM_ETH_CONN_AG_CTX_TX_ARM_CF_SHIFT			2
#define USTORM_ETH_CONN_AG_CTX_RX_ARM_CF_MASK			0x3	/* HSI_COMMENT: cf5 */
#define USTORM_ETH_CONN_AG_CTX_RX_ARM_CF_SHIFT			4
#define USTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_MASK		0x3	/* HSI_COMMENT: cf6 */
#define USTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_SHIFT		6
	u8 flags2;
#define USTORM_ETH_CONN_AG_CTX_TX_PMD_TERMINATE_CF_EN_MASK	0x1	/* HSI_COMMENT: cf0en */
#define USTORM_ETH_CONN_AG_CTX_TX_PMD_TERMINATE_CF_EN_SHIFT	0
#define USTORM_ETH_CONN_AG_CTX_RX_PMD_TERMINATE_CF_EN_MASK	0x1	/* HSI_COMMENT: cf1en */
#define USTORM_ETH_CONN_AG_CTX_RX_PMD_TERMINATE_CF_EN_SHIFT	1
#define USTORM_ETH_CONN_AG_CTX_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define USTORM_ETH_CONN_AG_CTX_CF2EN_SHIFT			2
#define USTORM_ETH_CONN_AG_CTX_CF3EN_MASK			0x1	/* HSI_COMMENT: cf3en */
#define USTORM_ETH_CONN_AG_CTX_CF3EN_SHIFT			3
#define USTORM_ETH_CONN_AG_CTX_TX_ARM_CF_EN_MASK		0x1	/* HSI_COMMENT: cf4en */
#define USTORM_ETH_CONN_AG_CTX_TX_ARM_CF_EN_SHIFT		4
#define USTORM_ETH_CONN_AG_CTX_RX_ARM_CF_EN_MASK		0x1	/* HSI_COMMENT: cf5en */
#define USTORM_ETH_CONN_AG_CTX_RX_ARM_CF_EN_SHIFT		5
#define USTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_EN_MASK	0x1	/* HSI_COMMENT: cf6en */
#define USTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_EN_SHIFT	6
#define USTORM_ETH_CONN_AG_CTX_RULE0EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define USTORM_ETH_CONN_AG_CTX_RULE0EN_SHIFT			7
	u8 flags3;
#define USTORM_ETH_CONN_AG_CTX_RULE1EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define USTORM_ETH_CONN_AG_CTX_RULE1EN_SHIFT			0
#define USTORM_ETH_CONN_AG_CTX_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define USTORM_ETH_CONN_AG_CTX_RULE2EN_SHIFT			1
#define USTORM_ETH_CONN_AG_CTX_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define USTORM_ETH_CONN_AG_CTX_RULE3EN_SHIFT			2
#define USTORM_ETH_CONN_AG_CTX_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define USTORM_ETH_CONN_AG_CTX_RULE4EN_SHIFT			3
#define USTORM_ETH_CONN_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define USTORM_ETH_CONN_AG_CTX_RULE5EN_SHIFT			4
#define USTORM_ETH_CONN_AG_CTX_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define USTORM_ETH_CONN_AG_CTX_RULE6EN_SHIFT			5
#define USTORM_ETH_CONN_AG_CTX_RULE7EN_MASK			0x1	/* HSI_COMMENT: rule7en */
#define USTORM_ETH_CONN_AG_CTX_RULE7EN_SHIFT			6
#define USTORM_ETH_CONN_AG_CTX_RULE8EN_MASK			0x1	/* HSI_COMMENT: rule8en */
#define USTORM_ETH_CONN_AG_CTX_RULE8EN_SHIFT			7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: conn_dpi */
	__le16 tx_bd_cons;	/* HSI_COMMENT: word1 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 tx_int_coallecing_timeset;	/* HSI_COMMENT: reg3 */
	__le16 tx_drv_bd_cons;	/* HSI_COMMENT: word2 */
	__le16 rx_drv_cqe_cons;	/* HSI_COMMENT: word3 */
};

/* The eth storm context for the Ustorm */
struct ustorm_eth_conn_st_ctx {
	__le32 reserved[40];
};

/* The eth storm context for the Mstorm */
struct mstorm_eth_conn_st_ctx {
	__le32 reserved[8];
};

/* eth connection context */
struct eth_conn_context {
	struct tstorm_eth_conn_st_ctx tstorm_st_context;	/* HSI_COMMENT: tstorm storm context */
	struct regpair tstorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct pstorm_eth_conn_st_ctx pstorm_st_context;	/* HSI_COMMENT: pstorm storm context */
	struct xstorm_eth_conn_st_ctx xstorm_st_context;	/* HSI_COMMENT: xstorm storm context */
	struct xstorm_eth_conn_ag_ctx xstorm_ag_context;	/* HSI_COMMENT: xstorm aggregative context */
	struct tstorm_eth_conn_ag_ctx tstorm_ag_context;	/* HSI_COMMENT: tstorm aggregative context */
	struct ystorm_eth_conn_st_ctx ystorm_st_context;	/* HSI_COMMENT: ystorm storm context */
	struct ystorm_eth_conn_ag_ctx ystorm_ag_context;	/* HSI_COMMENT: ystorm aggregative context */
	struct ustorm_eth_conn_ag_ctx ustorm_ag_context;	/* HSI_COMMENT: ustorm aggregative context */
	struct ustorm_eth_conn_st_ctx ustorm_st_context;	/* HSI_COMMENT: ustorm storm context */
	struct mstorm_eth_conn_st_ctx mstorm_st_context;	/* HSI_COMMENT: mstorm storm context */
};

/* L2 SP error code */
enum eth_error_code {
	ETH_OK = 0x00,		/* HSI_COMMENT: command succeeded */
	ETH_FILTERS_MAC_ADD_FAIL_FULL,	/* HSI_COMMENT: mac add filters command failed due to cam full state */
	ETH_FILTERS_MAC_ADD_FAIL_FULL_MTT2,	/* HSI_COMMENT: mac add filters command failed due to mtt2 full state */
	ETH_FILTERS_MAC_ADD_FAIL_DUP_MTT2,	/* HSI_COMMENT: mac add filters command failed due to duplicate mac address */
	ETH_FILTERS_MAC_ADD_FAIL_DUP_STT2,	/* HSI_COMMENT: mac add filters command failed due to duplicate mac address */
	ETH_FILTERS_MAC_DEL_FAIL_NOF,	/* HSI_COMMENT: mac delete filters command failed due to not found state */
	ETH_FILTERS_MAC_DEL_FAIL_NOF_MTT2,	/* HSI_COMMENT: mac delete filters command failed due to not found state */
	ETH_FILTERS_MAC_DEL_FAIL_NOF_STT2,	/* HSI_COMMENT: mac delete filters command failed due to not found state */
	ETH_FILTERS_MAC_ADD_FAIL_ZERO_MAC,	/* HSI_COMMENT: mac add filters command failed due to MAC Address of 00:00:00:00:00:00 */
	ETH_FILTERS_VLAN_ADD_FAIL_FULL,	/* HSI_COMMENT: vlan add filters command failed due to cam full state */
	ETH_FILTERS_VLAN_ADD_FAIL_DUP,	/* HSI_COMMENT: vlan add filters command failed due to duplicate VLAN filter */
	ETH_FILTERS_VLAN_DEL_FAIL_NOF,	/* HSI_COMMENT: vlan delete filters command failed due to not found state */
	ETH_FILTERS_VLAN_DEL_FAIL_NOF_TT1,	/* HSI_COMMENT: vlan delete filters command failed due to not found state */
	ETH_FILTERS_PAIR_ADD_FAIL_DUP,	/* HSI_COMMENT: pair add filters command failed due to duplicate request */
	ETH_FILTERS_PAIR_ADD_FAIL_FULL,	/* HSI_COMMENT: pair add filters command failed due to full state */
	ETH_FILTERS_PAIR_ADD_FAIL_FULL_MAC,	/* HSI_COMMENT: pair add filters command failed due to full state */
	ETH_FILTERS_PAIR_DEL_FAIL_NOF,	/* HSI_COMMENT: pair add filters command failed due not found state */
	ETH_FILTERS_PAIR_DEL_FAIL_NOF_TT1,	/* HSI_COMMENT: pair add filters command failed due not found state */
	ETH_FILTERS_PAIR_ADD_FAIL_ZERO_MAC,	/* HSI_COMMENT: pair add filters command failed due to MAC Address of 00:00:00:00:00:00 */
	ETH_FILTERS_VNI_ADD_FAIL_FULL,	/* HSI_COMMENT: vni add filters command failed due to cam full state */
	ETH_FILTERS_VNI_ADD_FAIL_DUP,	/* HSI_COMMENT: vni add filters command failed due to duplicate VNI filter */
	ETH_FILTERS_GFT_UPDATE_FAIL,	/* HSI_COMMENT: Fail update GFT filter. */
	ETH_RX_QUEUE_FAIL_LOAD_VF_DATA,	/* HSI_COMMENT: Fail load VF data like BD or CQE ring next page address. */
	ETH_FILTERS_GFS_ADD_FILTER_FAIL_MAX_HOPS,	/* HSI_COMMENT: Fail to add a GFS filter - max hops reached. */
	ETH_FILTERS_GFS_ADD_FILTER_FAIL_NO_FREE_ENRTY,	/* HSI_COMMENT: Fail to add a GFS filter - no free entry to add, database full. */
	ETH_FILTERS_GFS_ADD_FILTER_FAIL_ALREADY_EXISTS,	/* HSI_COMMENT: Fail to add a GFS filter  - entry already added. */
	ETH_FILTERS_GFS_ADD_FILTER_FAIL_PCI_ERROR,	/* HSI_COMMENT: Fail to add a GFS filter  - PCI error. */
	ETH_FILTERS_GFS_ADD_FINLER_FAIL_MAGIC_NUM_ERROR,	/* HSI_COMMENT: Fail to add a GFS filter  - magic number error. */
	ETH_FILTERS_GFS_DEL_FILTER_FAIL_MAX_HOPS,	/* HSI_COMMENT: Fail to delete GFS filter - max hops reached. */
	ETH_FILTERS_GFS_DEL_FILTER_FAIL_NO_MATCH_ENRTY,	/* HSI_COMMENT: Fail to delete GFS filter - no matched entry to detele. */
	ETH_FILTERS_GFS_DEL_FILTER_FAIL_PCI_ERROR,	/* HSI_COMMENT: Fail to delete GFS filter - PCI error. */
	ETH_FILTERS_GFS_DEL_FILTER_FAIL_MAGIC_NUM_ERROR,	/* HSI_COMMENT: Fail to delete GFS filter - magic number error. */
	MAX_ETH_ERROR_CODE
};

/* opcodes for the event ring */
enum eth_event_opcode {
	ETH_EVENT_UNUSED,
	ETH_EVENT_VPORT_START,
	ETH_EVENT_VPORT_UPDATE,
	ETH_EVENT_VPORT_STOP,
	ETH_EVENT_TX_QUEUE_START,
	ETH_EVENT_TX_QUEUE_STOP,
	ETH_EVENT_RX_QUEUE_START,
	ETH_EVENT_RX_QUEUE_UPDATE,
	ETH_EVENT_RX_QUEUE_STOP,
	ETH_EVENT_FILTERS_UPDATE,
	ETH_EVENT_RX_ADD_OPENFLOW_FILTER,
	ETH_EVENT_RX_DELETE_OPENFLOW_FILTER,
	ETH_EVENT_RX_CREATE_OPENFLOW_ACTION,
	ETH_EVENT_RX_ADD_UDP_FILTER,
	ETH_EVENT_RX_DELETE_UDP_FILTER,
	ETH_EVENT_RX_CREATE_GFT_ACTION,
	ETH_EVENT_RX_GFT_UPDATE_FILTER,
	ETH_EVENT_TX_QUEUE_UPDATE,
	ETH_EVENT_RGFS_ADD_FILTER,
	ETH_EVENT_RGFS_DEL_FILTER,
	ETH_EVENT_TGFS_ADD_FILTER,
	ETH_EVENT_TGFS_DEL_FILTER,
	ETH_EVENT_GFS_COUNTERS_REPORT_REQUEST,
	MAX_ETH_EVENT_OPCODE
};

/* Classify rule types in E2/E3 */
enum eth_filter_action {
	ETH_FILTER_ACTION_UNUSED,
	ETH_FILTER_ACTION_REMOVE,
	ETH_FILTER_ACTION_ADD,
	ETH_FILTER_ACTION_REMOVE_ALL,	/* HSI_COMMENT: Remove all filters of given type and vport ID. */
	MAX_ETH_FILTER_ACTION
};

/* Command for adding/removing a classification rule $$KEEP_ENDIANNESS$$ */
struct eth_filter_cmd {
	u8 type;		/* HSI_COMMENT: Filter Type (MAC/VLAN/Pair/VNI) (use enum eth_filter_type) */
	u8 vport_id;		/* HSI_COMMENT: the vport id */
	u8 action;		/* HSI_COMMENT: filter command action: add/remove/replace (use enum eth_filter_action) */
	u8 reserved0;
	__le32 vni;
	__le16 mac_lsb;
	__le16 mac_mid;
	__le16 mac_msb;
	__le16 vlan_id;
};

/*  $$KEEP_ENDIANNESS$$ */
struct eth_filter_cmd_header {
	u8 rx;			/* HSI_COMMENT: If set, apply these commands to the RX path */
	u8 tx;			/* HSI_COMMENT: If set, apply these commands to the TX path */
	u8 cmd_cnt;		/* HSI_COMMENT: Number of filter commands */
	u8 assert_on_error;	/* HSI_COMMENT: 0 - dont assert in case of filter configuration error. Just return an error code. 1 - assert in case of filter configuration error. */
	u8 reserved1[4];
};

/* Ethernet filter types: mac/vlan/pair */
enum eth_filter_type {
	ETH_FILTER_TYPE_UNUSED,
	ETH_FILTER_TYPE_MAC,	/* HSI_COMMENT: Add/remove a MAC address */
	ETH_FILTER_TYPE_VLAN,	/* HSI_COMMENT: Add/remove a VLAN */
	ETH_FILTER_TYPE_PAIR,	/* HSI_COMMENT: Add/remove a MAC-VLAN pair */
	ETH_FILTER_TYPE_INNER_MAC,	/* HSI_COMMENT: Add/remove a inner MAC address */
	ETH_FILTER_TYPE_INNER_VLAN,	/* HSI_COMMENT: Add/remove a inner VLAN */
	ETH_FILTER_TYPE_INNER_PAIR,	/* HSI_COMMENT: Add/remove a inner MAC-VLAN pair */
	ETH_FILTER_TYPE_INNER_MAC_VNI_PAIR,	/* HSI_COMMENT: Add/remove a inner MAC-VNI pair */
	ETH_FILTER_TYPE_MAC_VNI_PAIR,	/* HSI_COMMENT: Add/remove a MAC-VNI pair */
	ETH_FILTER_TYPE_VNI,	/* HSI_COMMENT: Add/remove a VNI */
	MAX_ETH_FILTER_TYPE
};

/* GFS copy condition. */
enum eth_gfs_copy_condition {
	ETH_GFS_COPY_ALWAYS,	/* HSI_COMMENT: Copy always. */
	ETH_GFS_COPY_TTL0,	/* HSI_COMMENT: Copy if TTL=0. */
	ETH_GFS_COPY_TTL1,	/* HSI_COMMENT: Copy if TTL=1. */
	ETH_GFS_COPY_TTL0_TTL1,	/* HSI_COMMENT: Copy if TTL=0 or TTL=1. */
	ETH_GFS_COPY_TCP_FLAGS,	/* HSI_COMMENT: Copy if one of TCP flags from tcp_flags set in packet. */
	MAX_ETH_GFS_COPY_CONDITION
};

/* GFS Flow Counters Report Entry. Table header is an 8B value of numValidEntries */
struct eth_gfs_counters_report_entry {
	struct regpair packets;	/* HSI_COMMENT: Packets Counter */
	struct regpair bytes;	/* HSI_COMMENT: Bytes Counter */
	__le32 tid;		/* HSI_COMMENT: TID of GFS Counter */
	__le32 reserved_unused;
};

/* GFS packet destination type. */
enum eth_gfs_dest_type {
	ETH_GFS_DEST_DROP,	/* HSI_COMMENT: Drop TX or RX packet. */
	ETH_GFS_DEST_RX_VPORT,	/* HSI_COMMENT: Forward RX or TX packet to RX VPORT. */
	ETH_GFS_DEST_RX_QID,	/* HSI_COMMENT: Forward RX packet to RX queue. */
	ETH_GFS_DEST_TX_CID,	/* HSI_COMMENT: Forward RX packet to TX queue given by CID (hairpinning). */
	ETH_GFS_DEST_TXSW_BYPASS,	/* HSI_COMMENT: Bypass TX switching for TX packet. TX destination given by tx_dest value. */
	MAX_ETH_GFS_DEST_TYPE
};

/* GFS packet modified header position. */
enum eth_gfs_modified_header_position {
	ETH_GFS_MODIFY_HDR_SINGLE,	/* HSI_COMMENT: Modify single header in no tunnel packet. */
	ETH_GFS_MODIFY_HDR_TUNNEL,	/* HSI_COMMENT: Modify tunnel header. */
	ETH_GFS_MODIFY_HDR_INNER,	/* HSI_COMMENT: Modify inner header in tunnel packet. */
	MAX_ETH_GFS_MODIFIED_HEADER_POSITION
};

enum eth_gfs_pop_hdr_type {
	ETH_GFS_POP_UNUSED,	/* HSI_COMMENT: Reserved for initialized check */
	ETH_GFS_POP_ETH,	/* HSI_COMMENT: Pop outer ETH header. */
	ETH_GFS_POP_TUNN,	/* HSI_COMMENT: Pop tunnel header. */
	ETH_GFS_POP_TUNN_ETH,	/* HSI_COMMENT: Pop tunnel and inner ETH header. */
	MAX_ETH_GFS_POP_HDR_TYPE
};

enum eth_gfs_push_hdr_type {
	ETH_GFS_PUSH_UNUSED,	/* HSI_COMMENT: Reserved for initialized check */
	ETH_GFS_PUSH_ETH,	/* HSI_COMMENT: Push ETH header. */
	ETH_GFS_PUSH_GRE_V4,	/* HSI_COMMENT: Push GRE over IPV4 tunnel header (ETH-IPV4-GRE). */
	ETH_GFS_PUSH_GRE_V6,	/* HSI_COMMENT: Push GRE over IPV6 tunnel header (ETH-IPV6-GRE). */
	ETH_GFS_PUSH_VXLAN_V4,	/* HSI_COMMENT: Push VXLAN over IPV4 tunnel header (ETH-IPV4-UDP-VXLAN). */
	ETH_GFS_PUSH_VXLAN_V6,	/* HSI_COMMENT: Push VXLAN over IPV6 tunnel header (ETH-IPV6-UDP-VXLAN). */
	ETH_GFS_PUSH_VXLAN_V4_ETH,	/* HSI_COMMENT: Push VXLAN over IPV4 tunnel header and inner ETH header (ETH-IPV4-UDP-VXLAN-ETH). */
	ETH_GFS_PUSH_VXLAN_V6_ETH,	/* HSI_COMMENT: Push VXLAN over IPV6 tunnel header and inner ETH header (ETH-IPV6-UDP-VXLAN-ETH). */
	MAX_ETH_GFS_PUSH_HDR_TYPE
};

/* GFS redirect condition. */
enum eth_gfs_redirect_condition {
	ETH_GFS_REDIRECT_ALWAYS,	/* HSI_COMMENT: Redirect always. */
	ETH_GFS_REDIRECT_TTL0,	/* HSI_COMMENT: Redirect if TTL=0. */
	ETH_GFS_REDIRECT_TTL1,	/* HSI_COMMENT: Redirect if TTL=1. */
	ETH_GFS_REDIRECT_TTL0_TTL1,	/* HSI_COMMENT: Redirect if TTL=0 or TTL=1. */
	MAX_ETH_GFS_REDIRECT_CONDITION
};

/* inner to inner vlan priority translation configurations */
struct eth_in_to_in_pri_map_cfg {
	u8 inner_vlan_pri_remap_en;	/* HSI_COMMENT: If set, non_rdma_in_to_in_pri_map or rdma_in_to_in_pri_map will be used for inner to inner priority mapping depending on protocol type */
	u8 reserved[7];
	u8 non_rdma_in_to_in_pri_map[8];	/* HSI_COMMENT: Map for inner to inner vlan priority translation for Non RDMA protocols, used for TenantDcb. Set inner_vlan_pri_remap_en, when init the map. */
	u8 rdma_in_to_in_pri_map[8];	/* HSI_COMMENT: Map for inner to inner vlan priority translation for RDMA protocols, used for TenantDcb. Set inner_vlan_pri_remap_en, when init the map. */
};

/* eth IPv4 Fragment Type */
enum eth_ipv4_frag_type {
	ETH_IPV4_NOT_FRAG,	/* HSI_COMMENT: IPV4 Packet Not Fragmented */
	ETH_IPV4_FIRST_FRAG,	/* HSI_COMMENT: First Fragment of IPv4 Packet (contains headers) */
	ETH_IPV4_NON_FIRST_FRAG,	/* HSI_COMMENT: Non-First Fragment of IPv4 Packet (does not contain headers) */
	MAX_ETH_IPV4_FRAG_TYPE
};

/* eth IPv4 Fragment Type */
enum eth_ip_type {
	ETH_IPV4,		/* HSI_COMMENT: IPv4 */
	ETH_IPV6,		/* HSI_COMMENT: IPv6 */
	MAX_ETH_IP_TYPE
};

/* Ethernet Ramrod Command IDs */
enum eth_ramrod_cmd_id {
	ETH_RAMROD_UNUSED,
	ETH_RAMROD_VPORT_START,	/* HSI_COMMENT: VPort Start Ramrod */
	ETH_RAMROD_VPORT_UPDATE,	/* HSI_COMMENT: VPort Update Ramrod */
	ETH_RAMROD_VPORT_STOP,	/* HSI_COMMENT: VPort Stop Ramrod */
	ETH_RAMROD_RX_QUEUE_START,	/* HSI_COMMENT: RX Queue Start Ramrod */
	ETH_RAMROD_RX_QUEUE_STOP,	/* HSI_COMMENT: RX Queue Stop Ramrod */
	ETH_RAMROD_TX_QUEUE_START,	/* HSI_COMMENT: TX Queue Start Ramrod */
	ETH_RAMROD_TX_QUEUE_STOP,	/* HSI_COMMENT: TX Queue Stop Ramrod */
	ETH_RAMROD_FILTERS_UPDATE,	/* HSI_COMMENT: Add or Remove Mac/Vlan/Pair filters */
	ETH_RAMROD_RX_QUEUE_UPDATE,	/* HSI_COMMENT: RX Queue Update Ramrod */
	ETH_RAMROD_RX_CREATE_OPENFLOW_ACTION,	/* HSI_COMMENT: RX - Create an Openflow Action */
	ETH_RAMROD_RX_ADD_OPENFLOW_FILTER,	/* HSI_COMMENT: RX - Add an Openflow Filter to the Searcher */
	ETH_RAMROD_RX_DELETE_OPENFLOW_FILTER,	/* HSI_COMMENT: RX - Delete an Openflow Filter to the Searcher */
	ETH_RAMROD_RX_ADD_UDP_FILTER,	/* HSI_COMMENT: RX - Add a UDP Filter to the Searcher */
	ETH_RAMROD_RX_DELETE_UDP_FILTER,	/* HSI_COMMENT: RX - Delete a UDP Filter to the Searcher */
	ETH_RAMROD_RX_CREATE_GFT_ACTION,	/* HSI_COMMENT: RX - Create a Gft Action */
	ETH_RAMROD_RX_UPDATE_GFT_FILTER,	/* HSI_COMMENT: RX - Add/Delete a GFT Filter to the Searcher */
	ETH_RAMROD_TX_QUEUE_UPDATE,	/* HSI_COMMENT: TX Queue Update Ramrod */
	ETH_RAMROD_RGFS_FILTER_ADD,	/* HSI_COMMENT: add new RGFS filter */
	ETH_RAMROD_RGFS_FILTER_DEL,	/* HSI_COMMENT: delete RGFS filter */
	ETH_RAMROD_TGFS_FILTER_ADD,	/* HSI_COMMENT: add new TGFS filter */
	ETH_RAMROD_TGFS_FILTER_DEL,	/* HSI_COMMENT: delete TGFS filter */
	ETH_RAMROD_GFS_COUNTERS_REPORT_REQUEST,	/* HSI_COMMENT: GFS Flow Counters Report Request */
	MAX_ETH_RAMROD_CMD_ID
};

/* tx destination enum */
enum eth_tx_dst_mode_config_enum {
	ETH_TX_DST_MODE_CONFIG_DISABLE,	/* HSI_COMMENT: tx destination configuration override is disabled */
	ETH_TX_DST_MODE_CONFIG_FORWARD_DATA_IN_BD,	/* HSI_COMMENT: tx destination configuration override is enabled, vport and tx dst will be taken from from 4th bd */
	ETH_TX_DST_MODE_CONFIG_FORWARD_DATA_IN_VPORT,	/* HSI_COMMENT: tx destination configuration override is enabled, vport and tx dst will be taken from from vport data */
	MAX_ETH_TX_DST_MODE_CONFIG_ENUM
};

/* What to do in case an error occurs */
enum eth_tx_err {
	ETH_TX_ERR_DROP,	/* HSI_COMMENT: Drop erroneous packet. */
	ETH_TX_ERR_ASSERT_MALICIOUS,	/* HSI_COMMENT: Assert an interrupt for PF, declare as malicious for VF */
	MAX_ETH_TX_ERR
};

/* Array of the different error type behaviors */
struct eth_tx_err_vals {
	__le16 values;
#define ETH_TX_ERR_VALS_ILLEGAL_VLAN_MODE_MASK			0x1	/* HSI_COMMENT: Wrong VLAN insertion mode (use enum eth_tx_err) */
#define ETH_TX_ERR_VALS_ILLEGAL_VLAN_MODE_SHIFT			0
#define ETH_TX_ERR_VALS_PACKET_TOO_SMALL_MASK			0x1	/* HSI_COMMENT: Packet is below minimal size (use enum eth_tx_err) */
#define ETH_TX_ERR_VALS_PACKET_TOO_SMALL_SHIFT			1
#define ETH_TX_ERR_VALS_ANTI_SPOOFING_ERR_MASK			0x1	/* HSI_COMMENT: Vport has sent spoofed packet (use enum eth_tx_err) */
#define ETH_TX_ERR_VALS_ANTI_SPOOFING_ERR_SHIFT			2
#define ETH_TX_ERR_VALS_ILLEGAL_INBAND_TAGS_MASK		0x1	/* HSI_COMMENT: Packet with illegal type of inband tag (use enum eth_tx_err) */
#define ETH_TX_ERR_VALS_ILLEGAL_INBAND_TAGS_SHIFT		3
#define ETH_TX_ERR_VALS_VLAN_INSERTION_W_INBAND_TAG_MASK	0x1	/* HSI_COMMENT: Packet marked for VLAN insertion when inband tag is present (use enum eth_tx_err) */
#define ETH_TX_ERR_VALS_VLAN_INSERTION_W_INBAND_TAG_SHIFT	4
#define ETH_TX_ERR_VALS_MTU_VIOLATION_MASK			0x1	/* HSI_COMMENT: Non LSO packet larger than MTU (use enum eth_tx_err) */
#define ETH_TX_ERR_VALS_MTU_VIOLATION_SHIFT			5
#define ETH_TX_ERR_VALS_ILLEGAL_CONTROL_FRAME_MASK		0x1	/* HSI_COMMENT: VF/PF has sent LLDP/PFC or any other type of control packet which is not allowed to (use enum eth_tx_err) */
#define ETH_TX_ERR_VALS_ILLEGAL_CONTROL_FRAME_SHIFT		6
#define ETH_TX_ERR_VALS_ILLEGAL_BD_FLAGS_MASK			0x1	/* HSI_COMMENT: Wrong BD flags. (use enum eth_tx_err) */
#define ETH_TX_ERR_VALS_ILLEGAL_BD_FLAGS_SHIFT			7
#define ETH_TX_ERR_VALS_RESERVED_MASK				0xFF
#define ETH_TX_ERR_VALS_RESERVED_SHIFT				8
};

/* vport rss configuration data */
struct eth_vport_rss_config {
	__le16 capabilities;
#define ETH_VPORT_RSS_CONFIG_IPV4_CAPABILITY_MASK		0x1	/* HSI_COMMENT: configuration of the IpV4 2-tuple capability */
#define ETH_VPORT_RSS_CONFIG_IPV4_CAPABILITY_SHIFT		0
#define ETH_VPORT_RSS_CONFIG_IPV6_CAPABILITY_MASK		0x1	/* HSI_COMMENT: configuration of the IpV6 2-tuple capability */
#define ETH_VPORT_RSS_CONFIG_IPV6_CAPABILITY_SHIFT		1
#define ETH_VPORT_RSS_CONFIG_IPV4_TCP_CAPABILITY_MASK		0x1	/* HSI_COMMENT: configuration of the IpV4 4-tuple capability for TCP */
#define ETH_VPORT_RSS_CONFIG_IPV4_TCP_CAPABILITY_SHIFT		2
#define ETH_VPORT_RSS_CONFIG_IPV6_TCP_CAPABILITY_MASK		0x1	/* HSI_COMMENT: configuration of the IpV6 4-tuple capability for TCP */
#define ETH_VPORT_RSS_CONFIG_IPV6_TCP_CAPABILITY_SHIFT		3
#define ETH_VPORT_RSS_CONFIG_IPV4_UDP_CAPABILITY_MASK		0x1	/* HSI_COMMENT: configuration of the IpV4 4-tuple capability for UDP */
#define ETH_VPORT_RSS_CONFIG_IPV4_UDP_CAPABILITY_SHIFT		4
#define ETH_VPORT_RSS_CONFIG_IPV6_UDP_CAPABILITY_MASK		0x1	/* HSI_COMMENT: configuration of the IpV6 4-tuple capability for UDP */
#define ETH_VPORT_RSS_CONFIG_IPV6_UDP_CAPABILITY_SHIFT		5
#define ETH_VPORT_RSS_CONFIG_EN_5_TUPLE_CAPABILITY_MASK		0x1	/* HSI_COMMENT: configuration of the 5-tuple capability */
#define ETH_VPORT_RSS_CONFIG_EN_5_TUPLE_CAPABILITY_SHIFT	6
#define ETH_VPORT_RSS_CONFIG_RESERVED0_MASK			0x1FF	/* HSI_COMMENT: if set update the rss keys */
#define ETH_VPORT_RSS_CONFIG_RESERVED0_SHIFT			7
	u8 rss_id;		/* HSI_COMMENT: The RSS engine ID. Must be allocated to each vport with RSS enabled. Total number of RSS engines is ETH_RSS_ENGINE_NUM_ , according to chip type. */
	u8 rss_mode;		/* HSI_COMMENT: The RSS mode for this function (use enum eth_vport_rss_mode) */
	u8 update_rss_key;	/* HSI_COMMENT: if set - update the rss key. rss_id must be valid.  */
	u8 update_rss_ind_table;	/* HSI_COMMENT: if set - update the indirection table values. rss_id must be valid. */
	u8 update_rss_capabilities;	/* HSI_COMMENT: if set - update the capabilities and indirection table size. rss_id must be valid. */
	u8 tbl_size;		/* HSI_COMMENT: rss mask (Tbl size) */
	u8 ind_table_mask_valid;	/* HSI_COMMENT: If set and update_rss_ind_table set, update part of indirection table according to ind_table_mask. */
	u8 reserved2[3];
	__le16 indirection_table[ETH_RSS_IND_TABLE_ENTRIES_NUM];	/* HSI_COMMENT: RSS indirection table */
	__le32 ind_table_mask[ETH_RSS_IND_TABLE_MASK_SIZE_REGS];	/* HSI_COMMENT: RSS indirection table update mask. Used if update_rss_ind_table and ind_table_mask_valid set. */
	__le32 rss_key[ETH_RSS_KEY_SIZE_REGS];	/* HSI_COMMENT: RSS key supplied to us by OS */
	__le32 reserved3;
};

/* eth vport RSS mode */
enum eth_vport_rss_mode {
	ETH_VPORT_RSS_MODE_DISABLED,	/* HSI_COMMENT: RSS Disabled */
	ETH_VPORT_RSS_MODE_REGULAR,	/* HSI_COMMENT: Regular (ndis-like) RSS */
	MAX_ETH_VPORT_RSS_MODE
};

/* Command for setting classification flags for a vport $$KEEP_ENDIANNESS$$ */
struct eth_vport_rx_mode {
	__le16 state;
#define ETH_VPORT_RX_MODE_UCAST_DROP_ALL_MASK			0x1	/* HSI_COMMENT: drop all unicast packets */
#define ETH_VPORT_RX_MODE_UCAST_DROP_ALL_SHIFT			0
#define ETH_VPORT_RX_MODE_UCAST_ACCEPT_ALL_MASK			0x1	/* HSI_COMMENT: accept all unicast packets (subject to vlan) */
#define ETH_VPORT_RX_MODE_UCAST_ACCEPT_ALL_SHIFT		1
#define ETH_VPORT_RX_MODE_UCAST_ACCEPT_UNMATCHED_MASK		0x1	/* HSI_COMMENT: accept all unmatched unicast packets */
#define ETH_VPORT_RX_MODE_UCAST_ACCEPT_UNMATCHED_SHIFT		2
#define ETH_VPORT_RX_MODE_MCAST_DROP_ALL_MASK			0x1	/* HSI_COMMENT: drop all multicast packets */
#define ETH_VPORT_RX_MODE_MCAST_DROP_ALL_SHIFT			3
#define ETH_VPORT_RX_MODE_MCAST_ACCEPT_ALL_MASK			0x1	/* HSI_COMMENT: accept all multicast packets (subject to vlan) */
#define ETH_VPORT_RX_MODE_MCAST_ACCEPT_ALL_SHIFT		4
#define ETH_VPORT_RX_MODE_BCAST_ACCEPT_ALL_MASK			0x1	/* HSI_COMMENT: accept all broadcast packets (subject to vlan) */
#define ETH_VPORT_RX_MODE_BCAST_ACCEPT_ALL_SHIFT		5
#define ETH_VPORT_RX_MODE_ACCEPT_ANY_VNI_MASK			0x1	/* HSI_COMMENT: accept any VNI in tunnel VNI classification. Used for default queue. */
#define ETH_VPORT_RX_MODE_ACCEPT_ANY_VNI_SHIFT			6
#define ETH_VPORT_RX_MODE_RESERVED1_MASK			0x1FF
#define ETH_VPORT_RX_MODE_RESERVED1_SHIFT			7
};

/* Command for setting tpa parameters */
struct eth_vport_tpa_param {
	u8 tpa_ipv4_en_flg;	/* HSI_COMMENT: Enable TPA for IPv4 packets */
	u8 tpa_ipv6_en_flg;	/* HSI_COMMENT: Enable TPA for IPv6 packets */
	u8 tpa_ipv4_tunn_en_flg;	/* HSI_COMMENT: Enable TPA for IPv4 over tunnel */
	u8 tpa_ipv6_tunn_en_flg;	/* HSI_COMMENT: Enable TPA for IPv6 over tunnel */
	u8 tpa_pkt_split_flg;	/* HSI_COMMENT: If set, start each TPA segment on new BD (GRO mode). One BD per segment allowed. */
	u8 tpa_hdr_data_split_flg;	/* HSI_COMMENT: If set, put header of first TPA segment on first BD and data on second BD. */
	u8 tpa_gro_consistent_flg;	/* HSI_COMMENT: If set, GRO data consistent will checked for TPA continue */
	u8 tpa_max_aggs_num;	/* HSI_COMMENT: maximum number of opened aggregations per v-port  */
	__le16 tpa_max_size;	/* HSI_COMMENT: maximal size for the aggregated TPA packets */
	__le16 tpa_min_size_to_start;	/* HSI_COMMENT: minimum TCP payload size for a packet to start aggregation */
	__le16 tpa_min_size_to_cont;	/* HSI_COMMENT: minimum TCP payload size for a packet to continue aggregation */
	u8 max_buff_num;	/* HSI_COMMENT: maximal number of buffers that can be used for one aggregation */
	u8 reserved;
};

/* Command for setting classification flags for a vport $$KEEP_ENDIANNESS$$ */
struct eth_vport_tx_mode {
	__le16 state;
#define ETH_VPORT_TX_MODE_UCAST_DROP_ALL_MASK		0x1	/* HSI_COMMENT: drop all unicast packets */
#define ETH_VPORT_TX_MODE_UCAST_DROP_ALL_SHIFT		0
#define ETH_VPORT_TX_MODE_UCAST_ACCEPT_ALL_MASK		0x1	/* HSI_COMMENT: accept all unicast packets (subject to vlan) */
#define ETH_VPORT_TX_MODE_UCAST_ACCEPT_ALL_SHIFT	1
#define ETH_VPORT_TX_MODE_MCAST_DROP_ALL_MASK		0x1	/* HSI_COMMENT: drop all multicast packets */
#define ETH_VPORT_TX_MODE_MCAST_DROP_ALL_SHIFT		2
#define ETH_VPORT_TX_MODE_MCAST_ACCEPT_ALL_MASK		0x1	/* HSI_COMMENT: accept all multicast packets (subject to vlan) */
#define ETH_VPORT_TX_MODE_MCAST_ACCEPT_ALL_SHIFT	3
#define ETH_VPORT_TX_MODE_BCAST_ACCEPT_ALL_MASK		0x1	/* HSI_COMMENT: accept all broadcast packets (subject to vlan) */
#define ETH_VPORT_TX_MODE_BCAST_ACCEPT_ALL_SHIFT	4
#define ETH_VPORT_TX_MODE_RESERVED1_MASK		0x7FF
#define ETH_VPORT_TX_MODE_RESERVED1_SHIFT		5
};

/* GFS action data vlan insert */
struct gfs_action_data_vlan_insert {
	__le16 reserved1;
	__le16 vlan;
	__le32 reserved2[14];
};

/* GFS action data vlan remove */
struct gfs_action_data_vlan_remove {
	__le32 reserved2[15];
};

/* GFS destination. */
struct gfs_dest_data {
	u8 type;		/* HSI_COMMENT: Destination type (use enum eth_gfs_dest_type) */
	u8 rx_vport;		/* HSI_COMMENT: Destination vport id */
	__le16 rx_qid;		/* HSI_COMMENT: Destination queue ID. */
	__le32 tx_cid;		/* HSI_COMMENT: Destination CID. */
	u8 tx_dest;		/* HSI_COMMENT: TX destination for TX switch bypass. */
	u8 drv_hint;		/* HSI_COMMENT: GFS redirect or copy action hint value. Limited to 3 bit. Reported by RX CQE for RX packets or TX packets, forwarded to RX VPORT. */
	u8 reserved[2];
};

/* GFS redirect action data. */
struct gfs_action_data_redirect {
	struct gfs_dest_data destination;	/* HSI_COMMENT: GFS destination */
	u8 condition;		/* HSI_COMMENT: GFS redirect condition. (use enum eth_gfs_redirect_condition) */
	u8 reserved1[3];
	__le32 reserved2[11];
};

/* GFS copy action data. */
struct gfs_action_data_copy {
	struct gfs_dest_data destination;	/* HSI_COMMENT: GFS destination */
	__le16 sample_len;	/* HSI_COMMENT: Maximum packet length for copy. If packet exceed the length, packet will be truncated. */
	u8 original_flg;	/* HSI_COMMENT: If set, copy before header modification */
	u8 condition;		/* HSI_COMMENT: GFS redirect condition. (use enum eth_gfs_copy_condition) */
	u8 tcp_flags;		/* HSI_COMMENT: TCP flags for copy condition. */
	u8 reserved1[3];
	__le32 reserved2[10];
};

/* GFS count action data */
struct gfs_action_data_count {
	__le32 counter_tid;	/* HSI_COMMENT: Counter TID. */
	u8 do_not_reset;	/* HSI_COMMENT: If set, do not reset counter value. */
	u8 reserved1[3];
	__le32 reserved2[13];
};

/* GFS modify ETH header action data */
struct gfs_action_data_hdr_modify_eth {
	u8 header_position;	/* HSI_COMMENT: Modified header position. (use enum eth_gfs_modified_header_position) */
	u8 set_dst_mac_flg;	/* HSI_COMMENT: If set, modify destination MAC. */
	u8 set_src_mac_flg;	/* HSI_COMMENT: If set, modify source MAC. */
	u8 set_vlan_id_flg;	/* HSI_COMMENT: If set, modify VLAN ID. */
	u8 set_vlan_pri_flg;	/* HSI_COMMENT: If set, modify VLAN priority. */
	u8 vlan_pri;		/* HSI_COMMENT: VLAN priority */
	__le16 vlan_id;		/* HSI_COMMENT: VID */
	__le16 dst_mac_ind_index;	/* HSI_COMMENT: Destination MAC indirect data table index. */
	__le16 src_mac_ind_index;	/* HSI_COMMENT: Source MAC indirect data table index. */
	__le16 dst_mac_hi;	/* HSI_COMMENT: Destination Mac Bytes 0 to 1 */
	__le16 dst_mac_mid;	/* HSI_COMMENT: Destination Mac Bytes 2 to 3 */
	__le16 dst_mac_lo;	/* HSI_COMMENT: Destination Mac Bytes 4 to 5 */
	__le16 src_mac_hi;	/* HSI_COMMENT: Source Mac Bytes 0 to 1 */
	__le16 src_mac_mid;	/* HSI_COMMENT: Source Mac Bytes 2 to 3 */
	__le16 src_mac_lo;	/* HSI_COMMENT: Source Mac Bytes 4 to 5 */
	__le32 reserved2[9];
};

/* GFS modify IPV4 header action data */
struct gfs_action_data_hdr_modify_ipv4 {
	u8 header_position;	/* HSI_COMMENT: Modified header position. (use enum eth_gfs_modified_header_position) */
	u8 set_dst_addr_flg;	/* HSI_COMMENT: If set, modify destination IP address. */
	u8 set_src_addr_flg;	/* HSI_COMMENT: If set, modify source IP address. */
	u8 set_dscp_flg;	/* HSI_COMMENT: If set, modify DSCP. */
	u8 set_ttl_flg;		/* HSI_COMMENT: If set, set TTL value. */
	u8 dec_ttl_flg;		/* HSI_COMMENT: If set, decrement TTL by 1. */
	u8 dscp;		/* HSI_COMMENT: DSCP */
	u8 ttl;			/* HSI_COMMENT: TTL */
	__le32 dst_addr;	/* HSI_COMMENT: IP Destination Address */
	__le32 src_addr;	/* HSI_COMMENT: IP Source Address */
	__le32 reserved2[11];
};

/* GFS modify IPV6 header action data */
struct gfs_action_data_hdr_modify_ipv6 {
	u8 header_position;	/* HSI_COMMENT: Modified header position. (use enum eth_gfs_modified_header_position) */
	u8 set_dst_addr_flg;	/* HSI_COMMENT: If set, modify destination IP address. */
	u8 set_src_addr_flg;	/* HSI_COMMENT: If set, modify source IP address. */
	u8 set_dscp_flg;	/* HSI_COMMENT: If set, modify DSCP. */
	u8 set_hop_limit_flg;	/* HSI_COMMENT: If set, set hop limit value. */
	u8 dec_hop_limit_flg;	/* HSI_COMMENT: If set, decrement hop limit by 1. */
	u8 dscp;		/* HSI_COMMENT: DSCP */
	u8 hop_limit;		/* HSI_COMMENT: hop limit */
	__le16 dst_addr_ind_index;	/* HSI_COMMENT: Destination IP address indirect data table index. */
	__le16 src_addr_ind_index;	/* HSI_COMMENT: Source IP address indirect data table index. */
	__le32 dst_addr[4];	/* HSI_COMMENT: IP Destination Address */
	__le32 src_addr[4];	/* HSI_COMMENT: IP Source Address */
	__le32 reserved2[4];
};

/* GFS modify UDP header action data. */
struct gfs_action_data_hdr_modify_udp {
	u8 set_dst_port_flg;	/* HSI_COMMENT: If set, modify destination port. */
	u8 set_src_port_flg;	/* HSI_COMMENT: If set, modify source port. */
	__le16 reserved1;
	__le16 dst_port;	/* HSI_COMMENT: UDP Destination Port */
	__le16 src_port;	/* HSI_COMMENT: UDP Source Port */
	__le32 reserved2[13];
};

/* GFS modify TCP header action data. */
struct gfs_action_data_hdr_modify_tcp {
	u8 set_dst_port_flg;	/* HSI_COMMENT: If set, modify destination port. */
	u8 set_src_port_flg;	/* HSI_COMMENT: If set, modify source port. */
	__le16 reserved1;
	__le16 dst_port;	/* HSI_COMMENT: TCP Destination Port */
	__le16 src_port;	/* HSI_COMMENT: TCP Source Port */
	__le32 reserved2[13];
};

/* GFS modify VXLAN header action data. */
struct gfs_action_data_hdr_modify_vxlan {
	u8 set_vni_flg;		/* HSI_COMMENT: If set, modify VNI. */
	u8 set_entropy_flg;	/* HSI_COMMENT: If set, use constant value for tunnel UDP source port. */
	u8 set_entropy_from_inner;	/* HSI_COMMENT: If set, calculate tunnel UDP source port from inner headers. */
	u8 reserved1[3];
	__le16 entropy;
	__le32 vni;		/* HSI_COMMENT: VNI value. */
	__le32 reserved2[12];
};

/* GFS modify GRE header action data. */
struct gfs_action_data_hdr_modify_gre {
	u8 set_key_flg;		/* HSI_COMMENT: If set, modify key. */
	u8 reserved1[3];
	__le32 key;		/* HSI_COMMENT: KEY value */
	__le32 reserved2[13];
};

/* GFS pop header action data. */
struct gfs_action_data_pop_hdr {
	u8 hdr_pop_type;	/* HSI_COMMENT: Headers, that must be removed from packet. (use enum eth_gfs_pop_hdr_type) */
	u8 reserved1[3];
	__le32 reserved2[14];
};

/* GFS push header action data. */
struct gfs_action_data_push_hdr {
	u8 hdr_push_type;	/* HSI_COMMENT: Headers, that will be added. (use enum eth_gfs_push_hdr_type) */
	u8 reserved1[3];
	__le32 reserved2[14];
};

union gfs_action_data_union {
	struct gfs_action_data_vlan_insert vlan_insert;
	struct gfs_action_data_vlan_remove vlan_remove;
	struct gfs_action_data_redirect redirect;	/* HSI_COMMENT: Redirect action. */
	struct gfs_action_data_copy copy;	/* HSI_COMMENT: Copy action. */
	struct gfs_action_data_count count;	/* HSI_COMMENT: Count action. */
	struct gfs_action_data_hdr_modify_eth modify_eth;	/* HSI_COMMENT: Modify ETH header action. */
	struct gfs_action_data_hdr_modify_ipv4 modify_ipv4;	/* HSI_COMMENT: Modify IPV4 header action. */
	struct gfs_action_data_hdr_modify_ipv6 modify_ipv6;	/* HSI_COMMENT: Modify IPV6 header action. */
	struct gfs_action_data_hdr_modify_udp modify_udp;	/* HSI_COMMENT: Modify UDP header action. */
	struct gfs_action_data_hdr_modify_tcp modify_tcp;	/* HSI_COMMENT: Modify TCP header action. */
	struct gfs_action_data_hdr_modify_vxlan modify_vxlan;	/* HSI_COMMENT: Modify VXLAN header action. */
	struct gfs_action_data_hdr_modify_gre modify_gre;	/* HSI_COMMENT: Modify GRE header action. */
	struct gfs_action_data_pop_hdr pop_hdr;	/* HSI_COMMENT: Pop  headers action. */
	struct gfs_action_data_push_hdr push_hdr;	/* HSI_COMMENT: Push headers action. */
};

struct gfs_action {
	u8 action_type;		/* HSI_COMMENT: GFS action type (use enum gfs_action_type) */
	u8 reserved[3];
	union gfs_action_data_union action_data;	/* HSI_COMMENT: GFS action data */
};

/* GFS action type enum */
enum gfs_action_type {
	ETH_GFS_ACTION_UNUSED,
	ETH_GFS_ACTION_INSERT_VLAN,
	ETH_GFS_ACTION_REMOVE_VLAN,
	ETH_GFS_ACTION_REDIRECT,	/* HSI_COMMENT: Change packet destination */
	ETH_GFS_ACTION_COPY,	/* HSI_COMMENT: Copy or sample packet to VPORT */
	ETH_GFS_ACTION_COUNT,	/* HSI_COMMENT: Count flow traffic bytes and packets. */
	ETH_GFS_ACTION_HDR_MODIFY_ETH,	/* HSI_COMMENT: Modify ETH header */
	ETH_GFS_ACTION_HDR_MODIFY_IPV4,	/* HSI_COMMENT: Modify IPV4 header */
	ETH_GFS_ACTION_HDR_MODIFY_IPV6,	/* HSI_COMMENT: Modify IPV6 header */
	ETH_GFS_ACTION_HDR_MODIFY_UDP,	/* HSI_COMMENT: Modify UDP header */
	ETH_GFS_ACTION_HDR_MODIFY_TCP,	/* HSI_COMMENT: Modify TCP header */
	ETH_GFS_ACTION_HDR_MODIFY_VXLAN,	/* HSI_COMMENT: Modify VXLAN header */
	ETH_GFS_ACTION_HDR_MODIFY_GRE,	/* HSI_COMMENT: Modify GRE header */
	ETH_GFS_ACTION_POP_HDR,	/* HSI_COMMENT: Pop headers */
	ETH_GFS_ACTION_PUSH_HDR,	/* HSI_COMMENT: Push headers */
	ETH_GFS_ACTION_APPEND_CONTEXT,	/* HSI_COMMENT: For testing only.  */
	MAX_GFS_ACTION_TYPE
};

/* Add GFS filter command header */
struct gfs_add_filter_cmd_header {
	struct regpair pkt_hdr_addr;	/* HSI_COMMENT: Pointer to packet header that defines GFS filter */
	struct regpair return_hash_addr;	/* HSI_COMMENT: Wtite GFS hash to this address if return_hash_flg set. */
	__le32 flow_id;		/* HSI_COMMENT: flow id associated with this filter. Must be unique to allow performance optimization. */
	__le32 flow_mark;	/* HSI_COMMENT: filter flow mark. Reported by RX CQE. */
	__le16 pkt_hdr_length;	/* HSI_COMMENT: Packet header length */
	u8 profile_id;		/* HSI_COMMENT: Profile id. */
	u8 vport_id;		/* HSI_COMMENT: Vport id. */
	u8 filter_pri;		/* HSI_COMMENT: filter priority */
	u8 num_of_actions;	/* HSI_COMMENT: Number of valid actions in actions list. */
	u8 return_hash_flg;	/* HSI_COMMENT: If set, FW will write gfs_filter_hash_value to return_hash_addr. */
	u8 exception_context_flg;	/* HSI_COMMENT: If set, exception context will be updated. Packet header not needed in this case. Exception context allocated per PF.   */
	u8 assert_on_error;	/* HSI_COMMENT: 0 - dont assert in case of filter configuration error, return an error code. 1 - assert in case of filter configuration error */
	u8 reserved[15];
};

/* Ipv6 and MAC addresses to be stored in indirect table in storm ram */
struct gfs_indirect_data {
	__le32 src_ipv6_addr[4];	/* HSI_COMMENT: Inner or single source IPv6 address. */
	__le32 dst_ipv6_addr[4];	/* HSI_COMMENT: Inner or single destination IPv6 address. */
	__le32 tunn_src_ipv6_addr[4];	/* HSI_COMMENT: Tunnel source IPv6 address. */
	__le32 tunn_dst_ipv6_addr[4];	/* HSI_COMMENT: Tunnel destination IPv6 address. */
	__le16 src_mac_addr_hi;	/* HSI_COMMENT: Inner or single source MAC address. */
	__le16 src_mac_addr_mid;	/* HSI_COMMENT: Inner or single source MAC address. */
	__le16 src_mac_addr_lo;	/* HSI_COMMENT: Inner or single source MAC address. */
	__le16 dst_mac_addr_hi;	/* HSI_COMMENT: Inner or single destination MAC address. */
	__le16 dst_mac_addr_mid;	/* HSI_COMMENT: Inner or single destination MAC address. */
	__le16 dst_mac_addr_lo;	/* HSI_COMMENT: Inner or single destination MAC address. */
	__le16 tunn_src_mac_addr_hi;	/* HSI_COMMENT: Tunnel source MAC address. */
	__le16 tunn_src_mac_addr_mid;	/* HSI_COMMENT: Tunnel source MAC address. */
	__le16 tunn_src_mac_addr_lo;	/* HSI_COMMENT: Tunnel source MAC address. */
	__le16 tunn_dst_mac_addr_hi;	/* HSI_COMMENT: Tunnel destination MAC address. */
	__le16 tunn_dst_mac_addr_mid;	/* HSI_COMMENT: Tunnel destination MAC address. */
	__le16 tunn_dst_mac_addr_lo;	/* HSI_COMMENT: Tunnel destination MAC address. */
	__le16 ipid;		/* HSI_COMMENT: Identification field in IPv4 header */
	u8 ipid_valid_flg;	/* HSI_COMMENT: if set, use ipid field is valid */
	u8 update_ipv6_addr;	/* HSI_COMMENT: if set, fw will update inner or single ipv6 address according to ipv6 address above */
	u8 update_tunn_ipv6_addr;	/* HSI_COMMENT: if set, fw will update tunnel ipv6 address according to ipv6 address above */
	u8 reserved[3];
};

/* GFS filter context. */
struct gfs_filter_context_data {
	__le32 context[24];	/* HSI_COMMENT: GFS context. */
	struct gfs_indirect_data indirect_data;	/* HSI_COMMENT: Ipv6 and MAC addresses to be stored in indirect table in storm ram */
};

/* GFS push ETH header data */
struct gfs_filter_push_data_eth {
	u8 vlan_exist_flg;	/* HSI_COMMENT: If set, VLAN TAG exist in ETH header. */
	u8 reserved;
	__le16 vlan_id;		/* HSI_COMMENT: VID */
	__le16 dst_mac_ind_index;	/* HSI_COMMENT: Destination MAC indirect data table index. */
	__le16 src_mac_ind_index;	/* HSI_COMMENT: Source MAC indirect data table index. */
	__le16 dst_mac_hi;	/* HSI_COMMENT: Destination Mac Bytes 0 to 1 */
	__le16 dst_mac_mid;	/* HSI_COMMENT: Destination Mac Bytes 2 to 3 */
	__le16 dst_mac_lo;	/* HSI_COMMENT: Destination Mac Bytes 4 to 5 */
	__le16 src_mac_hi;	/* HSI_COMMENT: Source Mac Bytes 0 to 1 */
	__le16 src_mac_mid;	/* HSI_COMMENT: Source Mac Bytes 2 to 3 */
	__le16 src_mac_lo;	/* HSI_COMMENT: Source Mac Bytes 4 to 5 */
};

/* GFS push IPV4 header data */
struct gfs_filter_push_data_ipv4 {
	u8 tc;			/* HSI_COMMENT: TC value */
	u8 ttl;			/* HSI_COMMENT: TTL value */
	u8 dont_frag_flag;	/* HSI_COMMENT: dont frag flag value. */
	u8 set_ipid_in_ram;	/* HSI_COMMENT: If set, update IPID value in ram to ipid_val . */
	__le16 ipid_ind_index;	/* HSI_COMMENT: IPID counter indirect data table index. */
	__le16 ipid_val;	/* HSI_COMMENT: Initial IPID value. Used if set_ipid_in_ram flag set. */
	__le32 dst_addr;	/* HSI_COMMENT: IP destination Address */
	__le32 src_addr;	/* HSI_COMMENT: IP source Address */
	__le32 reserved[7];
};

/* GFS push IPV6 header data */
struct gfs_filter_push_data_ipv6 {
	u8 traffic_class;	/* HSI_COMMENT: traffic class */
	u8 hop_limit;		/* HSI_COMMENT: hop limit */
	__le16 dst_addr_ind_index;	/* HSI_COMMENT: destination IPv6 address indirect data table index. */
	__le16 src_addr_ind_index;	/* HSI_COMMENT: source IPv6 address indirect data table index. */
	__le16 reserved;
	__le32 flow_label;	/* HSI_COMMENT: flow label. */
	__le32 dst_addr[4];	/* HSI_COMMENT: IP Destination Address */
	__le32 src_addr[4];	/* HSI_COMMENT: IP Source Address */
};

/* GFS push IP header data */
union gfs_filter_push_data_ip {
	struct gfs_filter_push_data_ipv4 ip_v4;	/* HSI_COMMENT: IPV4 data */
	struct gfs_filter_push_data_ipv6 ip_v6;	/* HSI_COMMENT: IPV6 data */
};

/* GFS push VXLAN tunnel header data. */
struct gfs_filter_push_data_vxlan {
	u8 oob_vni;		/* HSI_COMMENT: If set, use out of band VNI from RX path for hairpin traffic. */
	u8 udp_checksum_exist_flg;	/* HSI_COMMENT: If set, calculate UDP checksum in tunnel header. */
	u8 i_bit;		/* HSI_COMMENT: VXLAN I bit value. Set if VNI valid. */
	u8 entropy_from_inner;	/* HSI_COMMENT: If set, calculate tunnel UDP source port from inner headers. */
	__le16 reserved1;
	__le16 entropy;
	__le32 vni;		/* HSI_COMMENT: VNI value. */
};

/* GFS push GRE tunnel header data. */
struct gfs_filter_push_data_gre {
	u8 oob_key;		/* HSI_COMMENT: If set, use out of band KEY from RX path for hairpin traffic. */
	u8 checksum_exist_flg;	/* HSI_COMMENT: If set, GRE checksum exist in tunnel header. */
	u8 key_exist_flg;	/* HSI_COMMENT: If set, GRE key exist in tunnel header. */
	u8 reserved1;
	__le32 reserved2;
	__le32 key;		/* HSI_COMMENT: KEY value */
};

/* GFS push tunnel header data */
union gfs_filter_push_data_tunnel_hdr {
	struct gfs_filter_push_data_vxlan vxlan;	/* HSI_COMMENT: VXLAN data */
	struct gfs_filter_push_data_gre gre;	/* HSI_COMMENT: GRE data */
};

/* GFS filter push data. */
struct gfs_filter_push_data {
	struct gfs_filter_push_data_eth tunn_eth_header;	/* HSI_COMMENT: Tunnel ETH header data */
	struct gfs_filter_push_data_eth inner_eth_header;	/* HSI_COMMENT: Inner ETH header data */
	union gfs_filter_push_data_ip tunn_ip_header;	/* HSI_COMMENT: Tunnel IP header data */
	union gfs_filter_push_data_tunnel_hdr tunn_header;	/* HSI_COMMENT: Tunnel header data */
};

/* add GFS filter - filter is packet header of type of packet wished to pass certain FW flow */
struct gfs_add_filter_ramrod_data {
	struct gfs_add_filter_cmd_header filter_cmd_hdr;	/* HSI_COMMENT: Add GFS filter command header */
	struct gfs_action actions[ETH_GFS_NUM_OF_ACTIONS];	/* HSI_COMMENT:  GFS actions */
	struct gfs_filter_context_data context_data;	/* HSI_COMMENT: GFS filter context. */
	struct gfs_filter_push_data push_data;	/* HSI_COMMENT: Push action data. */
};

/* GFS Flow Counters Report Request Ramrod */
struct gfs_counters_report_request_ramrod_data {
	u8 tx_table_valid;	/* HSI_COMMENT: 1: Valid Tx Reporting Table, 0: No Tx Reporting Required */
	u8 rx_table_valid;	/* HSI_COMMENT: 1: Valid Rx Reporting Table, 0: No Rx Reporting Required */
	u8 reserved[6];
	struct regpair tx_counters_data_table_address;	/* HSI_COMMENT: Valid Tx Counter Values reporting Table */
	struct regpair rx_counters_data_table_address;	/* HSI_COMMENT: Valid Rx Counter Values reporting Table */
};

/* GFS filter hash value. */
struct gfs_filter_hash_value {
	__le32 hash[4];		/* HSI_COMMENT: GFS hash. */
};

/* del GFS filter - filter is packet header of type of packet wished to pass certain FW flow */
struct gfs_del_filter_ramrod_data {
	struct regpair pkt_hdr_addr;	/* HSI_COMMENT: Pointer to Packet Header That Defines GFS Filter */
	__le16 pkt_hdr_length;	/* HSI_COMMENT: Packet Header Length */
	u8 profile_id;		/* HSI_COMMENT: profile id. */
	u8 vport_id;		/* HSI_COMMENT: vport id. */
	u8 assert_on_error;	/* HSI_COMMENT: 0 - dont assert in case of filter configuration error, return an error code. 1 - assert in case of filter configuration error */
	u8 use_hash_flg;	/* HSI_COMMENT: If set, hash value used for delete filter instead packet header. */
	__le16 reserved;
	struct gfs_filter_hash_value hash;	/* HSI_COMMENT: GFS filter hash value. */
};

enum gfs_module_type {
	e_rgfs,
	e_tgfs,
	MAX_GFS_MODULE_TYPE
};

/* GFT filter update action type. */
enum gft_filter_update_action {
	GFT_ADD_FILTER,
	GFT_DELETE_FILTER,
	MAX_GFT_FILTER_UPDATE_ACTION
};

/* Ramrod data for rx create gft action */
struct rx_create_gft_action_ramrod_data {
	u8 vport_id;		/* HSI_COMMENT: Vport Id of GFT Action  */
	u8 reserved[7];
};

/* Ramrod data for rx create openflow action */
struct rx_create_openflow_action_ramrod_data {
	u8 vport_id;		/* HSI_COMMENT: ID of RX queue */
	u8 reserved[7];
};

/* Ramrod data for rx add openflow filter */
struct rx_openflow_filter_ramrod_data {
	__le16 action_icid;	/* HSI_COMMENT: CID of Action to run for this filter */
	u8 priority;		/* HSI_COMMENT: Searcher String - Packet priority */
	u8 reserved0;
	__le32 tenant_id;	/* HSI_COMMENT: Searcher String - Tenant ID */
	__le16 dst_mac_hi;	/* HSI_COMMENT: Searcher String - Destination Mac Bytes 0 to 1 */
	__le16 dst_mac_mid;	/* HSI_COMMENT: Searcher String - Destination Mac Bytes 2 to 3 */
	__le16 dst_mac_lo;	/* HSI_COMMENT: Searcher String - Destination Mac Bytes 4 to 5 */
	__le16 src_mac_hi;	/* HSI_COMMENT: Searcher String - Source Mac 0 to 1 */
	__le16 src_mac_mid;	/* HSI_COMMENT: Searcher String - Source Mac 2 to 3 */
	__le16 src_mac_lo;	/* HSI_COMMENT: Searcher String - Source Mac 4 to 5 */
	__le16 vlan_id;		/* HSI_COMMENT: Searcher String - Vlan ID */
	__le16 l2_eth_type;	/* HSI_COMMENT: Searcher String - Last L2 Ethertype */
	u8 ipv4_dscp;		/* HSI_COMMENT: Searcher String - IPv4 6 MSBs of the TOS Field */
	u8 ipv4_frag_type;	/* HSI_COMMENT: Searcher String - IPv4 Fragmentation Type (use enum eth_ipv4_frag_type) */
	u8 ipv4_over_ip;	/* HSI_COMMENT: Searcher String - IPv4 Over IP Type */
	u8 tenant_id_exists;	/* HSI_COMMENT: Searcher String - Tenant ID Exists */
	__le32 ipv4_dst_addr;	/* HSI_COMMENT: Searcher String - IPv4 Destination Address */
	__le32 ipv4_src_addr;	/* HSI_COMMENT: Searcher String - IPv4 Source Address */
	__le16 l4_dst_port;	/* HSI_COMMENT: Searcher String - TCP/UDP Destination Port */
	__le16 l4_src_port;	/* HSI_COMMENT: Searcher String - TCP/UDP Source Port */
};

/* Ramrod data for rx queue start ramrod */
struct rx_queue_start_ramrod_data {
	__le16 rx_queue_id;	/* HSI_COMMENT: RX queue ID */
	__le16 num_of_pbl_pages;	/* HSI_COMMENT: Number of pages in CQE PBL */
	__le16 bd_max_bytes;	/* HSI_COMMENT: maximal number of bytes that can be places on the bd */
	__le16 sb_id;		/* HSI_COMMENT: Status block ID */
	u8 sb_index;		/* HSI_COMMENT: Status block index */
	u8 vport_id;		/* HSI_COMMENT: Vport ID */
	u8 default_rss_queue_flg;	/* HSI_COMMENT: if set - this queue will be the default RSS queue */
	u8 complete_cqe_flg;	/* HSI_COMMENT: if set - post completion to the CQE ring */
	u8 complete_event_flg;	/* HSI_COMMENT: if set - post completion to the event ring */
	u8 stats_counter_id;	/* HSI_COMMENT: Statistics counter ID */
	u8 pin_context;		/* HSI_COMMENT: if set - Pin CID context. Total number of pinned connections cannot exceed ETH_PINNED_CONN_MAX_NUM */
	u8 pxp_tph_valid_bd;	/* HSI_COMMENT: PXP command TPH Valid - for BD/SGE fetch */
	u8 pxp_tph_valid_pkt;	/* HSI_COMMENT: PXP command TPH Valid - for packet placement */
	u8 pxp_st_hint;		/* HSI_COMMENT: PXP command Steering tag hint. Use enum pxp_tph_st_hint */
	__le16 pxp_st_index;	/* HSI_COMMENT: PXP command Steering tag index */
	u8 pmd_mode;		/* HSI_COMMENT: Indicates that current queue belongs to poll-mode driver */
	u8 notify_en;		/* HSI_COMMENT: Indicates that the current queue is using the TX notification queue mechanism - should be set only for PMD queue */
	u8 toggle_val;		/* HSI_COMMENT: Initial value for the toggle valid bit - used in PMD mode */
	u8 vf_rx_prod_index;	/* HSI_COMMENT: Index of RX producers in VF zone. Used for VF only. */
	u8 vf_rx_prod_use_zone_a;	/* HSI_COMMENT: Backward compatibility mode. If set, unprotected mStorm queue zone will used for VF RX producers instead of VF zone. */
	u8 reserved[5];
	__le16 reserved1;	/* HSI_COMMENT: FW reserved. */
	struct regpair cqe_pbl_addr;	/* HSI_COMMENT: Base address on host of CQE PBL */
	struct regpair bd_base;	/* HSI_COMMENT: bd address of the first bd page */
	struct regpair reserved2;	/* HSI_COMMENT: FW reserved. */
};

/* Ramrod data for rx queue stop ramrod */
struct rx_queue_stop_ramrod_data {
	__le16 rx_queue_id;	/* HSI_COMMENT: RX queue ID */
	u8 complete_cqe_flg;	/* HSI_COMMENT: if set - post completion to the CQE ring  */
	u8 complete_event_flg;	/* HSI_COMMENT: if set - post completion to the event ring */
	u8 vport_id;		/* HSI_COMMENT: ID of virtual port */
	u8 reserved[3];
};

/* Ramrod data for rx queue update ramrod */
struct rx_queue_update_ramrod_data {
	__le16 rx_queue_id;	/* HSI_COMMENT: RX queue ID */
	u8 complete_cqe_flg;	/* HSI_COMMENT: if set - post completion to the CQE ring */
	u8 complete_event_flg;	/* HSI_COMMENT: if set - post completion to the event ring */
	u8 vport_id;		/* HSI_COMMENT: Vport ID */
	u8 set_default_rss_queue;	/* HSI_COMMENT: If set, update default RSS queue to this queue. */
	u8 reserved[3];
	u8 reserved1;		/* HSI_COMMENT: FW reserved. */
	u8 reserved2;		/* HSI_COMMENT: FW reserved. */
	u8 reserved3;		/* HSI_COMMENT: FW reserved. */
	__le16 reserved4;	/* HSI_COMMENT: FW reserved. */
	__le16 reserved5;	/* HSI_COMMENT: FW reserved. */
	struct regpair reserved6;	/* HSI_COMMENT: FW reserved. */
};

/* Ramrod data for rx Add UDP Filter */
struct rx_udp_filter_ramrod_data {
	__le16 action_icid;	/* HSI_COMMENT: CID of Action to run for this filter */
	__le16 vlan_id;		/* HSI_COMMENT: Searcher String - Vlan ID */
	u8 ip_type;		/* HSI_COMMENT: Searcher String - IP Type (use enum eth_ip_type) */
	u8 tenant_id_exists;	/* HSI_COMMENT: Searcher String - Tenant ID Exists */
	__le16 reserved1;
	__le32 ip_dst_addr[4];	/* HSI_COMMENT: Searcher String - IP Destination Address, for IPv4 use ip_dst_addr[0] only */
	__le32 ip_src_addr[4];	/* HSI_COMMENT: Searcher String - IP Source Address, for IPv4 use ip_dst_addr[0] only */
	__le16 udp_dst_port;	/* HSI_COMMENT: Searcher String - UDP Destination Port */
	__le16 udp_src_port;	/* HSI_COMMENT: Searcher String - UDP Source Port */
	__le32 tenant_id;	/* HSI_COMMENT: Searcher String - Tenant ID */
};

/* add or delete GFT filter - filter is packet header of type of packet wished to pass certain FW flow */
struct rx_update_gft_filter_ramrod_data {
	struct regpair pkt_hdr_addr;	/* HSI_COMMENT: Pointer to Packet Header That Defines GFT Filter */
	__le16 pkt_hdr_length;	/* HSI_COMMENT: Packet Header Length */
	__le16 action_icid;	/* HSI_COMMENT: Action icid. Valid if action_icid_valid flag set. */
	__le16 rx_qid;		/* HSI_COMMENT: RX queue ID. Valid if rx_qid_valid set. */
	__le16 flow_id;		/* HSI_COMMENT: RX flow ID. Valid if flow_id_valid set. */
	__le16 vport_id;	/* HSI_COMMENT: RX vport Id. For drop flow, set to ETH_GFT_TRASHCAN_VPORT. */
	u8 action_icid_valid;	/* HSI_COMMENT: If set, action_icid will used for GFT filter update. */
	u8 rx_qid_valid;	/* HSI_COMMENT: If set, rx_qid will used for traffic steering, in additional to vport_id. flow_id_valid must be cleared. If cleared, queue ID will selected by RSS. */
	u8 flow_id_valid;	/* HSI_COMMENT: If set, flow_id will reported by CQE, rx_qid_valid must be cleared. If cleared, flow_id 0 will reported by CQE. */
	u8 filter_action;	/* HSI_COMMENT: Use to set type of action on filter (use enum gft_filter_update_action) */
	u8 assert_on_error;	/* HSI_COMMENT: 0 - dont assert in case of error. Just return an error code. 1 - assert in case of error. */
	u8 inner_vlan_removal_en;	/* HSI_COMMENT: If set, inner VLAN will be removed regardless to VPORT configuration. Supported by E4 only. */
};

/* Ramrod data for tx queue start ramrod */
struct tx_queue_start_ramrod_data {
	__le16 sb_id;		/* HSI_COMMENT: Status block ID */
	u8 sb_index;		/* HSI_COMMENT: Status block index */
	u8 vport_id;		/* HSI_COMMENT: VPort ID */
	u8 reserved0;		/* HSI_COMMENT: FW reserved. (qcn_rl_en) */
	u8 stats_counter_id;	/* HSI_COMMENT: Statistics counter ID to use */
	__le16 qm_pq_id;	/* HSI_COMMENT: QM PQ ID */
	u8 flags;
#define TX_QUEUE_START_RAMROD_DATA_DISABLE_OPPORTUNISTIC_MASK		0x1	/* HSI_COMMENT: 0: Enable QM opportunistic flow. 1: Disable QM opportunistic flow */
#define TX_QUEUE_START_RAMROD_DATA_DISABLE_OPPORTUNISTIC_SHIFT		0
#define TX_QUEUE_START_RAMROD_DATA_TEST_MODE_PKT_DUP_MASK		0x1	/* HSI_COMMENT: If set, Test Mode - packets will be duplicated by Xstorm handler */
#define TX_QUEUE_START_RAMROD_DATA_TEST_MODE_PKT_DUP_SHIFT		1
#define TX_QUEUE_START_RAMROD_DATA_PMD_MODE_MASK			0x1	/* HSI_COMMENT: Indicates that current queue belongs to poll-mode driver */
#define TX_QUEUE_START_RAMROD_DATA_PMD_MODE_SHIFT			2
#define TX_QUEUE_START_RAMROD_DATA_NOTIFY_EN_MASK			0x1	/* HSI_COMMENT: Indicates that the current queue is using the TX notification queue mechanism - should be set only for PMD queue */
#define TX_QUEUE_START_RAMROD_DATA_NOTIFY_EN_SHIFT			3
#define TX_QUEUE_START_RAMROD_DATA_PIN_CONTEXT_MASK			0x1	/* HSI_COMMENT: If set - Pin CID context. Total number of pinned connections cannot exceed ETH_PINNED_CONN_MAX_NUM */
#define TX_QUEUE_START_RAMROD_DATA_PIN_CONTEXT_SHIFT			4
#define TX_QUEUE_START_RAMROD_DATA_RESERVED1_MASK			0x7
#define TX_QUEUE_START_RAMROD_DATA_RESERVED1_SHIFT			5
	u8 pxp_st_hint;		/* HSI_COMMENT: PXP command Steering tag hint (use enum pxp_tph_st_hint) */
	u8 pxp_tph_valid_bd;	/* HSI_COMMENT: PXP command TPH Valid - for BD fetch */
	u8 pxp_tph_valid_pkt;	/* HSI_COMMENT: PXP command TPH Valid - for packet fetch */
	__le16 pxp_st_index;	/* HSI_COMMENT: PXP command Steering tag index */
	u8 comp_agg_size;	/* HSI_COMMENT: TX completion min agg size - for PMD queues */
	u8 reserved3;
	__le16 queue_zone_id;	/* HSI_COMMENT: queue zone ID to use */
	__le16 reserved2;	/* HSI_COMMENT: FW reserved. (test_dup_count) */
	__le16 pbl_size;	/* HSI_COMMENT: Number of BD pages pointed by PBL */
	__le16 tx_queue_id;	/* HSI_COMMENT: unique Queue ID - currently used only by PMD flow */
	__le16 same_as_last_id;	/* HSI_COMMENT: For E4: Unique Same-As-Last Resource ID - improves performance for same-as-last packets per connection (range 0..ETH_TX_NUM_SAME_AS_LAST_ENTRIES-1 IDs available). Switch off SAL for this tx queue by setting value to ETH_TX_INACTIVE_SAME_AS_LAST (HSI constant). */
	__le16 reserved[3];
	struct regpair pbl_base_addr;	/* HSI_COMMENT: address of the pbl page */
	struct regpair bd_cons_address;	/* HSI_COMMENT: BD consumer address in host - for PMD queues */
};

/* Ramrod data for tx queue stop ramrod */
struct tx_queue_stop_ramrod_data {
	__le16 reserved[4];
};

/* Ramrod data for tx queue update ramrod */
struct tx_queue_update_ramrod_data {
	__le16 update_qm_pq_id_flg;	/* HSI_COMMENT: Flag to Update QM PQ ID */
	__le16 qm_pq_id;	/* HSI_COMMENT: Updated QM PQ ID */
	__le32 reserved0;
	struct regpair reserved1[5];
};

/* Inner to Inner VLAN priority map update mode */
enum update_in_to_in_pri_map_mode_enum {
	ETH_IN_TO_IN_PRI_MAP_UPDATE_DISABLED,	/* HSI_COMMENT: Inner to Inner VLAN priority map update Disabled */
	ETH_IN_TO_IN_PRI_MAP_UPDATE_NON_RDMA_TBL,	/* HSI_COMMENT: Update Inner to Inner VLAN priority map for non RDMA protocols */
	ETH_IN_TO_IN_PRI_MAP_UPDATE_RDMA_TBL,	/* HSI_COMMENT: Update Inner to Inner VLAN priority map for RDMA protocols */
	MAX_UPDATE_IN_TO_IN_PRI_MAP_MODE_ENUM
};

/* Ramrod data for vport update ramrod */
struct vport_filter_update_ramrod_data {
	struct eth_filter_cmd_header filter_cmd_hdr;	/* HSI_COMMENT: Header for Filter Commands (RX/TX, Add/Remove/Replace, etc) */
	struct eth_filter_cmd filter_cmds[ETH_FILTER_RULES_COUNT];	/* HSI_COMMENT: Filter Commands */
};

/* Ramrod data for vport start ramrod */
struct vport_start_ramrod_data {
	u8 vport_id;
	u8 sw_fid;
	__le16 mtu;
	u8 drop_ttl0_en;	/* HSI_COMMENT: if set, drop packet with ttl=0 */
	u8 inner_vlan_removal_en;
	struct eth_vport_rx_mode rx_mode;	/* HSI_COMMENT: Rx filter data */
	struct eth_vport_tx_mode tx_mode;	/* HSI_COMMENT: Tx filter data */
	struct eth_vport_tpa_param tpa_param;	/* HSI_COMMENT: TPA configuration parameters */
	__le16 default_vlan;	/* HSI_COMMENT: Default Vlan value to be forced by FW */
	u8 tx_switching_en;	/* HSI_COMMENT: Tx switching is enabled for current Vport */
	u8 anti_spoofing_en;	/* HSI_COMMENT: Anti-spoofing verification is set for current Vport */
	u8 default_vlan_en;	/* HSI_COMMENT: If set, the default Vlan value is forced by the FW */
	u8 handle_ptp_pkts;	/* HSI_COMMENT: If set, the vport handles PTP Timesync Packets */
	u8 silent_vlan_removal_en;	/* HSI_COMMENT: If enable then innerVlan will be striped and not written to cqe */
	u8 untagged;		/* HSI_COMMENT: If set untagged filter (vlan0) is added to current Vport, otherwise port is marked as any-vlan */
	struct eth_tx_err_vals tx_err_behav;	/* HSI_COMMENT: Desired behavior per TX error type */
	u8 zero_placement_offset;	/* HSI_COMMENT: If set, ETH header padding will not inserted. placement_offset will be zero. */
	u8 ctl_frame_mac_check_en;	/* HSI_COMMENT: If set, control frames will be filtered according to MAC check. */
	u8 ctl_frame_ethtype_check_en;	/* HSI_COMMENT: If set, control frames will be filtered according to ethtype check. */
	u8 reserved0;
	u8 reserved1;
	u8 tx_dst_port_mode_config;	/* HSI_COMMENT: Configurations for tx forwarding, used in VF Representor mode (use enum eth_tx_dst_mode_config_enum) */
	u8 dst_vport_id;	/* HSI_COMMENT: destination Vport ID to forward the packet, applicable only if dst_vport_id_valid is set and when tx_dst_port_mode_config == ETH_TX_DST_MODE_CONFIG_FORWARD_DATA_IN_VPORT and (tx_dst_port_mode == DST_PORT_LOOPBACK or tx_dst_port_mode == DST_PORT_PHY_LOOPBACK) */
	u8 tx_dst_port_mode;	/* HSI_COMMENT: destination tx to forward the packet, applicable only when tx_dst_port_mode_config == ETH_TX_DST_MODE_CONFIG_FORWARD_DATA_IN_VPORT (use enum dst_port_mode) */
	u8 dst_vport_id_valid;	/* HSI_COMMENT: if set, dst_vport_id has valid value */
	u8 wipe_inner_vlan_pri_en;	/* HSI_COMMENT: If set, the inner vlan (802.1q tag) priority that is written to cqe will be zero out, used for TenantDcb */
	u8 reserved2[2];
	struct eth_in_to_in_pri_map_cfg in_to_in_vlan_pri_map_cfg;	/* HSI_COMMENT: inner to inner vlan priority translation configurations */
};

/* Ramrod data for vport stop ramrod */
struct vport_stop_ramrod_data {
	u8 vport_id;
	u8 reserved[7];
};

/* Ramrod data for vport update ramrod */
struct vport_update_ramrod_data_cmn {
	u8 vport_id;
	u8 update_rx_active_flg;	/* HSI_COMMENT: set if rx active flag should be handled */
	u8 rx_active_flg;	/* HSI_COMMENT: rx active flag value */
	u8 update_tx_active_flg;	/* HSI_COMMENT: set if tx active flag should be handled */
	u8 tx_active_flg;	/* HSI_COMMENT: tx active flag value */
	u8 update_rx_mode_flg;	/* HSI_COMMENT: set if rx state data should be handled */
	u8 update_tx_mode_flg;	/* HSI_COMMENT: set if tx state data should be handled */
	u8 update_approx_mcast_flg;	/* HSI_COMMENT: set if approx. mcast data should be handled */
	u8 update_rss_flg;	/* HSI_COMMENT: set if rss data should be handled  */
	u8 update_inner_vlan_removal_en_flg;	/* HSI_COMMENT: set if inner_vlan_removal_en should be handled */
	u8 inner_vlan_removal_en;
	u8 update_tpa_param_flg;	/* HSI_COMMENT: set if tpa parameters should be handled, TPA must be disable before */
	u8 update_tpa_en_flg;	/* HSI_COMMENT: set if tpa enable changes */
	u8 update_tx_switching_en_flg;	/* HSI_COMMENT: set if tx switching en flag should be handled */
	u8 tx_switching_en;	/* HSI_COMMENT: tx switching en value */
	u8 update_anti_spoofing_en_flg;	/* HSI_COMMENT: set if anti spoofing flag should be handled */
	u8 anti_spoofing_en;	/* HSI_COMMENT: Anti-spoofing verification en value */
	u8 update_handle_ptp_pkts;	/* HSI_COMMENT: set if handle_ptp_pkts should be handled. */
	u8 handle_ptp_pkts;	/* HSI_COMMENT: If set, the vport handles PTP Timesync Packets */
	u8 update_default_vlan_en_flg;	/* HSI_COMMENT: If set, the default Vlan enable flag is updated */
	u8 default_vlan_en;	/* HSI_COMMENT: If set, the default Vlan value is forced by the FW */
	u8 update_default_vlan_flg;	/* HSI_COMMENT: If set, the default Vlan value is updated */
	__le16 default_vlan;	/* HSI_COMMENT: Default Vlan value to be forced by FW */
	u8 update_accept_any_vlan_flg;	/* HSI_COMMENT: set if accept_any_vlan should be handled */
	u8 accept_any_vlan;	/* HSI_COMMENT: accept_any_vlan updated value */
	u8 silent_vlan_removal_en;	/* HSI_COMMENT: Set to remove vlan silently, update_inner_vlan_removal_en_flg must be enabled as well. If Rx is in noSgl mode send rx_queue_update_ramrod_data */
	u8 update_mtu_flg;	/* HSI_COMMENT: If set, MTU will be updated. Vport must be not active. */
	__le16 mtu;		/* HSI_COMMENT: New MTU value. Used if update_mtu_flg are set */
	u8 update_ctl_frame_checks_en_flg;	/* HSI_COMMENT: If set, ctl_frame_mac_check_en and ctl_frame_ethtype_check_en will be updated */
	u8 ctl_frame_mac_check_en;	/* HSI_COMMENT: If set, control frames will be filtered according to MAC check. */
	u8 ctl_frame_ethtype_check_en;	/* HSI_COMMENT: If set, control frames will be filtered according to ethtype check. */
	u8 update_in_to_in_pri_map_mode;	/* HSI_COMMENT: Indicates to update RDMA or NON-RDMA vlan remapping priority table according to update_in_to_in_pri_map_mode_enum, used for TenantDcb (use enum update_in_to_in_pri_map_mode_enum) */
	u8 in_to_in_pri_map[8];	/* HSI_COMMENT: Map for inner to inner vlan priority translation, used for TenantDcb. */
	u8 update_tx_dst_port_mode_flg;	/* HSI_COMMENT: If set, tx_dst_port_mode_config, tx_dst_port_mode and dst_vport_id will be updated */
	u8 tx_dst_port_mode_config;	/* HSI_COMMENT: Configurations for tx forwarding, used in VF Representor mode (use enum eth_tx_dst_mode_config_enum) */
	u8 dst_vport_id;	/* HSI_COMMENT: destination Vport ID to forward the packet, applicable only if dst_vport_id_valid is set and when tx_dst_port_mode_config == ETH_TX_DST_MODE_CONFIG_FORWARD_DATA_IN_VPORT and (tx_dst_port_mode == DST_PORT_LOOPBACK or tx_dst_port_mode == DST_PORT_PHY_LOOPBACK) */
	u8 tx_dst_port_mode;	/* HSI_COMMENT: destination tx to forward the packet, applicable only when tx_dst_port_mode_config == ETH_TX_DST_MODE_CONFIG_FORWARD_DATA_IN_VPORT (use enum dst_port_mode) */
	u8 dst_vport_id_valid;	/* HSI_COMMENT: if set, dst_vport_id has valid value. If clear, RX classification will performed. */
	u8 reserved[1];
};

struct vport_update_ramrod_mcast {
	__le32 bins[ETH_MULTICAST_MAC_BINS_IN_REGS];	/* HSI_COMMENT: multicast bins */
};

/* Ramrod data for vport update ramrod */
struct vport_update_ramrod_data {
	struct vport_update_ramrod_data_cmn common;	/* HSI_COMMENT: Common data for all vport update ramrods */
	struct eth_vport_rx_mode rx_mode;	/* HSI_COMMENT: vport rx mode bitmap */
	struct eth_vport_tx_mode tx_mode;	/* HSI_COMMENT: vport tx mode bitmap */
	__le32 reserved[3];
	struct eth_vport_tpa_param tpa_param;	/* HSI_COMMENT: TPA configuration parameters */
	struct vport_update_ramrod_mcast approx_mcast;
	struct eth_vport_rss_config rss_config;	/* HSI_COMMENT: rss config data */
};

struct E4XstormEthConnAgCtxDqExtLdPart {
	u8 reserved0;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EXIST_IN_QM0_SHIFT		0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED1_SHIFT			1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED2_MASK			0x1	/* HSI_COMMENT: exist_in_qm2 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED2_SHIFT			2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EXIST_IN_QM3_MASK		0x1	/* HSI_COMMENT: exist_in_qm3 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EXIST_IN_QM3_SHIFT		3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED3_MASK			0x1	/* HSI_COMMENT: bit4 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED3_SHIFT			4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED4_MASK			0x1	/* HSI_COMMENT: cf_array_active */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED4_SHIFT			5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED5_MASK			0x1	/* HSI_COMMENT: bit6 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED5_SHIFT			6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED6_MASK			0x1	/* HSI_COMMENT: bit7 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED6_SHIFT			7
	u8 flags1;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED7_MASK			0x1	/* HSI_COMMENT: bit8 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED7_SHIFT			0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED8_MASK			0x1	/* HSI_COMMENT: bit9 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED8_SHIFT			1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED9_MASK			0x1	/* HSI_COMMENT: bit10 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED9_SHIFT			2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_BIT11_MASK			0x1	/* HSI_COMMENT: bit11 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_BIT11_SHIFT			3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_E5_RESERVED2_MASK		0x1	/* HSI_COMMENT: bit12 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_E5_RESERVED2_SHIFT		4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_E5_RESERVED3_MASK		0x1	/* HSI_COMMENT: bit13 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_E5_RESERVED3_SHIFT		5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TX_RULE_ACTIVE_MASK		0x1	/* HSI_COMMENT: bit14 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TX_RULE_ACTIVE_SHIFT		6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_ACTIVE_MASK		0x1	/* HSI_COMMENT: bit15 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_ACTIVE_SHIFT		7
	u8 flags2;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF0_MASK			0x3	/* HSI_COMMENT: timer0cf */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF0_SHIFT			0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF1_MASK			0x3	/* HSI_COMMENT: timer1cf */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF1_SHIFT			2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF2_MASK			0x3	/* HSI_COMMENT: timer2cf */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF2_SHIFT			4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF3_MASK			0x3	/* HSI_COMMENT: timer_stop_all */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF3_SHIFT			6
	u8 flags3;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF4_MASK			0x3	/* HSI_COMMENT: cf4 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF4_SHIFT			0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF5_MASK			0x3	/* HSI_COMMENT: cf5 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF5_SHIFT			2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF6_MASK			0x3	/* HSI_COMMENT: cf6 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF6_SHIFT			4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF7_MASK			0x3	/* HSI_COMMENT: cf7 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF7_SHIFT			6
	u8 flags4;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF8_MASK			0x3	/* HSI_COMMENT: cf8 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF8_SHIFT			0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF9_MASK			0x3	/* HSI_COMMENT: cf9 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF9_SHIFT			2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF10_MASK			0x3	/* HSI_COMMENT: cf10 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF10_SHIFT			4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF11_MASK			0x3	/* HSI_COMMENT: cf11 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF11_SHIFT			6
	u8 flags5;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF12_MASK			0x3	/* HSI_COMMENT: cf12 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF12_SHIFT			0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF13_MASK			0x3	/* HSI_COMMENT: cf13 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF13_SHIFT			2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF14_MASK			0x3	/* HSI_COMMENT: cf14 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF14_SHIFT			4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF15_MASK			0x3	/* HSI_COMMENT: cf15 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF15_SHIFT			6
	u8 flags6;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_GO_TO_BD_CONS_CF_MASK		0x3	/* HSI_COMMENT: cf16 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_GO_TO_BD_CONS_CF_SHIFT		0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_MULTI_UNICAST_CF_MASK		0x3	/* HSI_COMMENT: cf_array_cf */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_MULTI_UNICAST_CF_SHIFT		2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_MASK			0x3	/* HSI_COMMENT: cf18 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_SHIFT			4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TERMINATE_CF_MASK		0x3	/* HSI_COMMENT: cf19 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TERMINATE_CF_SHIFT		6
	u8 flags7;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_FLUSH_Q0_MASK			0x3	/* HSI_COMMENT: cf20 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_FLUSH_Q0_SHIFT			0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED10_MASK			0x3	/* HSI_COMMENT: cf21 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED10_SHIFT		2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_SLOW_PATH_MASK			0x3	/* HSI_COMMENT: cf22 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_SLOW_PATH_SHIFT			4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF0EN_MASK			0x1	/* HSI_COMMENT: cf0en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF0EN_SHIFT			6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF1EN_MASK			0x1	/* HSI_COMMENT: cf1en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF1EN_SHIFT			7
	u8 flags8;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF2EN_SHIFT			0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF3EN_MASK			0x1	/* HSI_COMMENT: cf3en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF3EN_SHIFT			1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF4EN_MASK			0x1	/* HSI_COMMENT: cf4en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF4EN_SHIFT			2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF5EN_MASK			0x1	/* HSI_COMMENT: cf5en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF5EN_SHIFT			3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF6EN_MASK			0x1	/* HSI_COMMENT: cf6en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF6EN_SHIFT			4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF7EN_MASK			0x1	/* HSI_COMMENT: cf7en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF7EN_SHIFT			5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF8EN_MASK			0x1	/* HSI_COMMENT: cf8en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF8EN_SHIFT			6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF9EN_MASK			0x1	/* HSI_COMMENT: cf9en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF9EN_SHIFT			7
	u8 flags9;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF10EN_MASK			0x1	/* HSI_COMMENT: cf10en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF10EN_SHIFT			0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF11EN_MASK			0x1	/* HSI_COMMENT: cf11en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF11EN_SHIFT			1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF12EN_MASK			0x1	/* HSI_COMMENT: cf12en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF12EN_SHIFT			2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF13EN_MASK			0x1	/* HSI_COMMENT: cf13en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF13EN_SHIFT			3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF14EN_MASK			0x1	/* HSI_COMMENT: cf14en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF14EN_SHIFT			4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF15EN_MASK			0x1	/* HSI_COMMENT: cf15en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF15EN_SHIFT			5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_GO_TO_BD_CONS_CF_EN_MASK	0x1	/* HSI_COMMENT: cf16en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_GO_TO_BD_CONS_CF_EN_SHIFT	6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_MULTI_UNICAST_CF_EN_MASK	0x1	/* HSI_COMMENT: cf_array_cf_en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_MULTI_UNICAST_CF_EN_SHIFT	7
	u8 flags10;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_EN_MASK			0x1	/* HSI_COMMENT: cf18en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_EN_SHIFT			0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TERMINATE_CF_EN_MASK		0x1	/* HSI_COMMENT: cf19en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TERMINATE_CF_EN_SHIFT		1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_FLUSH_Q0_EN_MASK		0x1	/* HSI_COMMENT: cf20en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_FLUSH_Q0_EN_SHIFT		2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED11_MASK			0x1	/* HSI_COMMENT: cf21en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED11_SHIFT		3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_SLOW_PATH_EN_MASK		0x1	/* HSI_COMMENT: cf22en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_SLOW_PATH_EN_SHIFT		4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TPH_ENABLE_EN_RESERVED_MASK	0x1	/* HSI_COMMENT: cf23en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TPH_ENABLE_EN_RESERVED_SHIFT	5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED12_MASK			0x1	/* HSI_COMMENT: rule0en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED12_SHIFT		6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED13_MASK			0x1	/* HSI_COMMENT: rule1en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED13_SHIFT		7
	u8 flags11;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED14_MASK			0x1	/* HSI_COMMENT: rule2en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED14_SHIFT		0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED15_MASK			0x1	/* HSI_COMMENT: rule3en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED15_SHIFT		1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TX_DEC_RULE_EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TX_DEC_RULE_EN_SHIFT		2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE5EN_SHIFT			3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE6EN_SHIFT			4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE7EN_MASK			0x1	/* HSI_COMMENT: rule7en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE7EN_SHIFT			5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED1_MASK		0x1	/* HSI_COMMENT: rule8en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED1_SHIFT		6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE9EN_MASK			0x1	/* HSI_COMMENT: rule9en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE9EN_SHIFT			7
	u8 flags12;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE10EN_MASK			0x1	/* HSI_COMMENT: rule10en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE10EN_SHIFT			0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE11EN_MASK			0x1	/* HSI_COMMENT: rule11en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE11EN_SHIFT			1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED2_MASK		0x1	/* HSI_COMMENT: rule12en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED2_SHIFT		2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED3_MASK		0x1	/* HSI_COMMENT: rule13en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED3_SHIFT		3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE14EN_MASK			0x1	/* HSI_COMMENT: rule14en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE14EN_SHIFT			4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE15EN_MASK			0x1	/* HSI_COMMENT: rule15en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE15EN_SHIFT			5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE16EN_MASK			0x1	/* HSI_COMMENT: rule16en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE16EN_SHIFT			6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE17EN_MASK			0x1	/* HSI_COMMENT: rule17en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE17EN_SHIFT			7
	u8 flags13;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE18EN_MASK			0x1	/* HSI_COMMENT: rule18en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE18EN_SHIFT			0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE19EN_MASK			0x1	/* HSI_COMMENT: rule19en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE19EN_SHIFT			1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED4_MASK		0x1	/* HSI_COMMENT: rule20en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED4_SHIFT		2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED5_MASK		0x1	/* HSI_COMMENT: rule21en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED5_SHIFT		3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED6_MASK		0x1	/* HSI_COMMENT: rule22en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED6_SHIFT		4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED7_MASK		0x1	/* HSI_COMMENT: rule23en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED7_SHIFT		5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED8_MASK		0x1	/* HSI_COMMENT: rule24en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED8_SHIFT		6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED9_MASK		0x1	/* HSI_COMMENT: rule25en */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED9_SHIFT		7
	u8 flags14;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_USE_EXT_HDR_MASK		0x1	/* HSI_COMMENT: bit16 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_USE_EXT_HDR_SHIFT		0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_SEND_RAW_L3L4_MASK		0x1	/* HSI_COMMENT: bit17 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_SEND_RAW_L3L4_SHIFT	1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_INBAND_PROP_HDR_MASK	0x1	/* HSI_COMMENT: bit18 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_INBAND_PROP_HDR_SHIFT	2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_SEND_EXT_TUNNEL_MASK	0x1	/* HSI_COMMENT: bit19 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_SEND_EXT_TUNNEL_SHIFT	3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_L2_EDPM_ENABLE_MASK		0x1	/* HSI_COMMENT: bit20 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_L2_EDPM_ENABLE_SHIFT		4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_ROCE_EDPM_ENABLE_MASK		0x1	/* HSI_COMMENT: bit21 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_ROCE_EDPM_ENABLE_SHIFT		5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TPH_ENABLE_MASK			0x3	/* HSI_COMMENT: cf23 */
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TPH_ENABLE_SHIFT		6
	u8 edpm_event_id;	/* HSI_COMMENT: byte2 */
	__le16 physical_q0;	/* HSI_COMMENT: physical_q0 */
	__le16 e5_reserved1;	/* HSI_COMMENT: physical_q1 */
	__le16 edpm_num_bds;	/* HSI_COMMENT: physical_q2 */
	__le16 tx_bd_cons;	/* HSI_COMMENT: word3 */
	__le16 tx_bd_prod;	/* HSI_COMMENT: word4 */
	__le16 updated_qm_pq_id;	/* HSI_COMMENT: word5 */
	__le16 conn_dpi;	/* HSI_COMMENT: conn_dpi */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	u8 byte5;		/* HSI_COMMENT: byte5 */
	u8 byte6;		/* HSI_COMMENT: byte6 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
};

/* GFS RAM line struct with fields breakout */
struct gfs_profile_ram_line {
	__le32 reg0;
#define GFS_PROFILE_RAM_LINE_IN_SRC_PORT_MASK			0x1F	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_IN_SRC_PORT_SHIFT			0
#define GFS_PROFILE_RAM_LINE_IN_SRC_IP_MASK			0xFF	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_IN_SRC_IP_SHIFT			5
#define GFS_PROFILE_RAM_LINE_IN_DST_MAC_MASK			0x3F	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_IN_DST_MAC_SHIFT			13
#define GFS_PROFILE_RAM_LINE_IN_SRC_MAC_MASK			0x3F	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_IN_SRC_MAC_SHIFT			19
#define GFS_PROFILE_RAM_LINE_IN_ETHTYPE_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_IN_ETHTYPE_SHIFT			25
#define GFS_PROFILE_RAM_LINE_IN_CVLAN_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_IN_CVLAN_SHIFT			26
#define GFS_PROFILE_RAM_LINE_IN_CVLAN_DEI_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_IN_CVLAN_DEI_SHIFT			27
#define GFS_PROFILE_RAM_LINE_IN_CVLAN_PRI_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_IN_CVLAN_PRI_SHIFT			28
#define GFS_PROFILE_RAM_LINE_IN_IS_UNICAST_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_IN_IS_UNICAST_SHIFT		29
#define GFS_PROFILE_RAM_LINE_IN_CVLAN_EXISTS_MASK		0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_IN_CVLAN_EXISTS_SHIFT		30
#define GFS_PROFILE_RAM_LINE_IN_SVLAN_EXISTS_MASK		0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_IN_SVLAN_EXISTS_SHIFT		31
	__le32 reg1;
#define GFS_PROFILE_RAM_LINE_IN_IS_IP_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_IN_IS_IP_SHIFT			0
#define GFS_PROFILE_RAM_LINE_IN_IS_TCP_UDP_SCTP_MASK		0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_IN_IS_TCP_UDP_SCTP_SHIFT		1
#define GFS_PROFILE_RAM_LINE_IN_DSCP_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_IN_DSCP_SHIFT			2
#define GFS_PROFILE_RAM_LINE_IN_ECN_MASK			0x3	/* HSI_COMMENT: mask of type: bitwise */
#define GFS_PROFILE_RAM_LINE_IN_ECN_SHIFT			3
#define GFS_PROFILE_RAM_LINE_IN_DST_IP_MASK			0xFF	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_IN_DST_IP_SHIFT			5
#define GFS_PROFILE_RAM_LINE_IN_DST_PORT_MASK			0x1F	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_IN_DST_PORT_SHIFT			13
#define GFS_PROFILE_RAM_LINE_IN_TTL_EQUALS_ZERO_MASK		0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_IN_TTL_EQUALS_ZERO_SHIFT		18
#define GFS_PROFILE_RAM_LINE_IN_TTL_EQUALS_ONE_MASK		0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_IN_TTL_EQUALS_ONE_SHIFT		19
#define GFS_PROFILE_RAM_LINE_IN_IP_PROTOCOL_MASK		0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_IN_IP_PROTOCOL_SHIFT		20
#define GFS_PROFILE_RAM_LINE_IN_IP_VERSION_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_IN_IP_VERSION_SHIFT		21
#define GFS_PROFILE_RAM_LINE_IN_IPV6_FLOW_LABEL_MASK		0x1F	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_IN_IPV6_FLOW_LABEL_SHIFT		22
#define GFS_PROFILE_RAM_LINE_TUN_SRC_PORT_MASK			0x1F	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_TUN_SRC_PORT_SHIFT			27
	__le32 reg2;
#define GFS_PROFILE_RAM_LINE_TUN_SRC_IP_MASK			0xFF	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_TUN_SRC_IP_SHIFT			0
#define GFS_PROFILE_RAM_LINE_TUN_DST_MAC_MASK			0x3F	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_TUN_DST_MAC_SHIFT			8
#define GFS_PROFILE_RAM_LINE_TUN_SRC_MAC_MASK			0x3F	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_TUN_SRC_MAC_SHIFT			14
#define GFS_PROFILE_RAM_LINE_TUN_ETHTYPE_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_TUN_ETHTYPE_SHIFT			20
#define GFS_PROFILE_RAM_LINE_TUN_CVLAN_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_TUN_CVLAN_SHIFT			21
#define GFS_PROFILE_RAM_LINE_TUN_CVLAN_DEI_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_TUN_CVLAN_DEI_SHIFT		22
#define GFS_PROFILE_RAM_LINE_TUN_CVLAN_PRI_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_TUN_CVLAN_PRI_SHIFT		23
#define GFS_PROFILE_RAM_LINE_TUN_IS_UNICAST_MASK		0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_TUN_IS_UNICAST_SHIFT		24
#define GFS_PROFILE_RAM_LINE_TUN_CVLAN_EXISTS_MASK		0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_TUN_CVLAN_EXISTS_SHIFT		25
#define GFS_PROFILE_RAM_LINE_TUN_SVLAN_EXISTS_MASK		0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_TUN_SVLAN_EXISTS_SHIFT		26
#define GFS_PROFILE_RAM_LINE_TUN_IS_IP_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_TUN_IS_IP_SHIFT			27
#define GFS_PROFILE_RAM_LINE_TUN_IS_TCP_UDP_SCTP_MASK		0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_TUN_IS_TCP_UDP_SCTP_SHIFT		28
#define GFS_PROFILE_RAM_LINE_TUN_DSCP_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_TUN_DSCP_SHIFT			29
#define GFS_PROFILE_RAM_LINE_TUN_ECN_MASK			0x3	/* HSI_COMMENT: mask of type: bitwise */
#define GFS_PROFILE_RAM_LINE_TUN_ECN_SHIFT			30
	__le32 reg3;
#define GFS_PROFILE_RAM_LINE_TUN_DST_IP_MASK			0xFF	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_TUN_DST_IP_SHIFT			0
#define GFS_PROFILE_RAM_LINE_TUN_DST_PORT_MASK			0x1F	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_TUN_DST_PORT_SHIFT			8
#define GFS_PROFILE_RAM_LINE_TUN_TTL_EQUALS_ZERO_MASK		0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_TUN_TTL_EQUALS_ZERO_SHIFT		13
#define GFS_PROFILE_RAM_LINE_TUN_TTL_EQUALS_ONE_MASK		0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_TUN_TTL_EQUALS_ONE_SHIFT		14
#define GFS_PROFILE_RAM_LINE_TUN_IP_PROTOCOL_MASK		0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_TUN_IP_PROTOCOL_SHIFT		15
#define GFS_PROFILE_RAM_LINE_TUN_IP_VERSION_MASK		0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_TUN_IP_VERSION_SHIFT		16
#define GFS_PROFILE_RAM_LINE_TUN_IPV6_FLOW_LABEL_MASK		0x1F	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_TUN_IPV6_FLOW_LABEL_SHIFT		17
#define GFS_PROFILE_RAM_LINE_PF_MASK				0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_PF_SHIFT				22
#define GFS_PROFILE_RAM_LINE_TUNNEL_EXISTS_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_TUNNEL_EXISTS_SHIFT		23
#define GFS_PROFILE_RAM_LINE_TUNNEL_TYPE_MASK			0xF	/* HSI_COMMENT: mask of type: bitwise (use enum gfs_tunnel_type_enum) */
#define GFS_PROFILE_RAM_LINE_TUNNEL_TYPE_SHIFT			24
#define GFS_PROFILE_RAM_LINE_TENANT_ID_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_TENANT_ID_SHIFT			28
#define GFS_PROFILE_RAM_LINE_ENTROPY_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_ENTROPY_SHIFT			29
#define GFS_PROFILE_RAM_LINE_L2_HEADER_EXISTS_MASK		0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_L2_HEADER_EXISTS_SHIFT		30
#define GFS_PROFILE_RAM_LINE_IP_FRAGMENT_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_IP_FRAGMENT_SHIFT			31
	__le32 reg4;
#define GFS_PROFILE_RAM_LINE_TCP_FLAGS_MASK			0x3FF	/* HSI_COMMENT: mask of type: bitwise */
#define GFS_PROFILE_RAM_LINE_TCP_FLAGS_SHIFT			0
#define GFS_PROFILE_RAM_LINE_CALC_TCP_FLAGS_MASK		0x3F	/* HSI_COMMENT: mask of type: bitwise */
#define GFS_PROFILE_RAM_LINE_CALC_TCP_FLAGS_SHIFT		10
#define GFS_PROFILE_RAM_LINE_STAG_MASK				0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_STAG_SHIFT				16
#define GFS_PROFILE_RAM_LINE_STAG_DEI_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_STAG_DEI_SHIFT			17
#define GFS_PROFILE_RAM_LINE_STAG_PRI_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_STAG_PRI_SHIFT			18
#define GFS_PROFILE_RAM_LINE_MPLS_EXISTS_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_MPLS_EXISTS_SHIFT			19
#define GFS_PROFILE_RAM_LINE_MPLS_LABEL_MASK			0x1F	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_MPLS_LABEL_SHIFT			20
#define GFS_PROFILE_RAM_LINE_MPLS_TC_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_MPLS_TC_SHIFT			25
#define GFS_PROFILE_RAM_LINE_MPLS_BOS_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_MPLS_BOS_SHIFT			26
#define GFS_PROFILE_RAM_LINE_MPLS_TTL_EQUALS_ZERO_MASK		0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_MPLS_TTL_EQUALS_ZERO_SHIFT		27
#define GFS_PROFILE_RAM_LINE_MPLS_TTL_EQUALS_ONE_MASK		0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_MPLS_TTL_EQUALS_ONE_SHIFT		28
#define GFS_PROFILE_RAM_LINE_MPLS2_EXISTS_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_MPLS2_EXISTS_SHIFT			29
#define GFS_PROFILE_RAM_LINE_MPLS3_EXISTS_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_MPLS3_EXISTS_SHIFT			30
#define GFS_PROFILE_RAM_LINE_MPLS4_EXISTS_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_MPLS4_EXISTS_SHIFT			31
	__le32 reg5;
#define GFS_PROFILE_RAM_LINE_FLEX_BYTE0_MASK			0xFF	/* HSI_COMMENT: mask of type: bitwise */
#define GFS_PROFILE_RAM_LINE_FLEX_BYTE0_SHIFT			0
#define GFS_PROFILE_RAM_LINE_FLEX_BYTE1_MASK			0xFF	/* HSI_COMMENT: mask of type: bitwise */
#define GFS_PROFILE_RAM_LINE_FLEX_BYTE1_SHIFT			8
#define GFS_PROFILE_RAM_LINE_FLEX_BYTE2_MASK			0xFF	/* HSI_COMMENT: mask of type: bitwise */
#define GFS_PROFILE_RAM_LINE_FLEX_BYTE2_SHIFT			16
#define GFS_PROFILE_RAM_LINE_FLEX_BYTE3_MASK			0xFF	/* HSI_COMMENT: mask of type: bitwise */
#define GFS_PROFILE_RAM_LINE_FLEX_BYTE3_SHIFT			24
	__le32 reg6;
#define GFS_PROFILE_RAM_LINE_FLEX_BYTE4_MASK			0xFF	/* HSI_COMMENT: mask of type: bitwise */
#define GFS_PROFILE_RAM_LINE_FLEX_BYTE4_SHIFT			0
#define GFS_PROFILE_RAM_LINE_FLEX_BYTE5_MASK			0xFF	/* HSI_COMMENT: mask of type: bitwise */
#define GFS_PROFILE_RAM_LINE_FLEX_BYTE5_SHIFT			8
#define GFS_PROFILE_RAM_LINE_FLEX_WORD0_MASK			0xFFFF	/* HSI_COMMENT: mask of type: bitwise */
#define GFS_PROFILE_RAM_LINE_FLEX_WORD0_SHIFT			16
	__le32 reg7;
#define GFS_PROFILE_RAM_LINE_FLEX_WORD1_MASK			0xFFFF	/* HSI_COMMENT: mask of type: bitwise */
#define GFS_PROFILE_RAM_LINE_FLEX_WORD1_SHIFT			0
#define GFS_PROFILE_RAM_LINE_FLEX_WORD2_MASK			0xFFFF	/* HSI_COMMENT: mask of type: bitwise */
#define GFS_PROFILE_RAM_LINE_FLEX_WORD2_SHIFT			16
	__le32 reg8;
#define GFS_PROFILE_RAM_LINE_FLEX_WORD3_MASK			0xFFFF	/* HSI_COMMENT: mask of type: bitwise */
#define GFS_PROFILE_RAM_LINE_FLEX_WORD3_SHIFT			0
#define GFS_PROFILE_RAM_LINE_FLEX_WORD4_MASK			0xFFFF	/* HSI_COMMENT: mask of type: bitwise */
#define GFS_PROFILE_RAM_LINE_FLEX_WORD4_SHIFT			16
	__le32 reg9;
#define GFS_PROFILE_RAM_LINE_FLEX_WORD5_MASK			0xFFFF	/* HSI_COMMENT: mask of type: bitwise */
#define GFS_PROFILE_RAM_LINE_FLEX_WORD5_SHIFT			0
#define GFS_PROFILE_RAM_LINE_PROFILE_ID_MASK			0x3FF	/* HSI_COMMENT: mask of type: bitwise, profile id associated with this context */
#define GFS_PROFILE_RAM_LINE_PROFILE_ID_SHIFT			16
#define GFS_PROFILE_RAM_LINE_FLEX_REG0_MASK			0x3F	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_FLEX_REG0_SHIFT			26
	__le32 reg10;
#define GFS_PROFILE_RAM_LINE_FLEX_REG1_MASK			0x3F	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_FLEX_REG1_SHIFT			0
#define GFS_PROFILE_RAM_LINE_FLEX_REG2_MASK			0x3F	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_FLEX_REG2_SHIFT			6
#define GFS_PROFILE_RAM_LINE_FLEX_REG3_MASK			0x3F	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_FLEX_REG3_SHIFT			12
#define GFS_PROFILE_RAM_LINE_FLEX_REG4_MASK			0x3F	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_FLEX_REG4_SHIFT			18
#define GFS_PROFILE_RAM_LINE_FLEX_REG5_MASK			0x3F	/* HSI_COMMENT: mask of type: prefix */
#define GFS_PROFILE_RAM_LINE_FLEX_REG5_SHIFT			24
#define GFS_PROFILE_RAM_LINE_VPORT_ID_MASK			0x1	/* HSI_COMMENT: mask of type: bool */
#define GFS_PROFILE_RAM_LINE_VPORT_ID_SHIFT			30
#define GFS_PROFILE_RAM_LINE_PRIORITY_MASK			0x1	/* HSI_COMMENT: When set - if a lookup from this profile matches, all the lookups from subsequent profiles will be discarded */
#define GFS_PROFILE_RAM_LINE_PRIORITY_SHIFT			31
	__le32 reg11;
#define GFS_PROFILE_RAM_LINE_SWAP_I2O_MASK			0x3	/* HSI_COMMENT:  (use enum gfs_swap_i2o_enum) */
#define GFS_PROFILE_RAM_LINE_SWAP_I2O_SHIFT			0
#define GFS_PROFILE_RAM_LINE_RESERVED_MASK			0x3FFFFFFF
#define GFS_PROFILE_RAM_LINE_RESERVED_SHIFT			2
	__le32 reservedRegs[4];
};

/* GFT CAM line struct with fields breakout */
struct gft_cam_line_mapped {
	__le32 camline;
#define GFT_CAM_LINE_MAPPED_VALID_MASK				0x1	/* HSI_COMMENT: Indication if the line is valid. */
#define GFT_CAM_LINE_MAPPED_VALID_SHIFT				0
#define GFT_CAM_LINE_MAPPED_IP_VERSION_MASK			0x1	/* HSI_COMMENT:  (use enum gft_profile_ip_version) */
#define GFT_CAM_LINE_MAPPED_IP_VERSION_SHIFT			1
#define GFT_CAM_LINE_MAPPED_TUNNEL_IP_VERSION_MASK		0x1	/* HSI_COMMENT:  (use enum gft_profile_ip_version) */
#define GFT_CAM_LINE_MAPPED_TUNNEL_IP_VERSION_SHIFT		2
#define GFT_CAM_LINE_MAPPED_UPPER_PROTOCOL_TYPE_MASK		0xF	/* HSI_COMMENT:  (use enum gft_profile_upper_protocol_type) */
#define GFT_CAM_LINE_MAPPED_UPPER_PROTOCOL_TYPE_SHIFT		3
#define GFT_CAM_LINE_MAPPED_TUNNEL_TYPE_MASK			0xF	/* HSI_COMMENT:  (use enum gft_profile_tunnel_type) */
#define GFT_CAM_LINE_MAPPED_TUNNEL_TYPE_SHIFT			7
#define GFT_CAM_LINE_MAPPED_PF_ID_MASK				0xF
#define GFT_CAM_LINE_MAPPED_PF_ID_SHIFT				11
#define GFT_CAM_LINE_MAPPED_IP_VERSION_MASK_MASK		0x1
#define GFT_CAM_LINE_MAPPED_IP_VERSION_MASK_SHIFT		15
#define GFT_CAM_LINE_MAPPED_TUNNEL_IP_VERSION_MASK_MASK		0x1
#define GFT_CAM_LINE_MAPPED_TUNNEL_IP_VERSION_MASK_SHIFT	16
#define GFT_CAM_LINE_MAPPED_UPPER_PROTOCOL_TYPE_MASK_MASK	0xF
#define GFT_CAM_LINE_MAPPED_UPPER_PROTOCOL_TYPE_MASK_SHIFT	17
#define GFT_CAM_LINE_MAPPED_TUNNEL_TYPE_MASK_MASK		0xF
#define GFT_CAM_LINE_MAPPED_TUNNEL_TYPE_MASK_SHIFT		21
#define GFT_CAM_LINE_MAPPED_PF_ID_MASK_MASK			0xF
#define GFT_CAM_LINE_MAPPED_PF_ID_MASK_SHIFT			25
#define GFT_CAM_LINE_MAPPED_RESERVED1_MASK			0x7
#define GFT_CAM_LINE_MAPPED_RESERVED1_SHIFT			29
};

/* Used in gft_profile_key: Indication for ip version */
enum gft_profile_ip_version {
	GFT_PROFILE_IPV4 = 0,
	GFT_PROFILE_IPV6 = 1,
	MAX_GFT_PROFILE_IP_VERSION
};

/* Profile key stucr fot GFT logic in Prs */
struct gft_profile_key {
	__le16 profile_key;
#define GFT_PROFILE_KEY_IP_VERSION_MASK			0x1	/* HSI_COMMENT: use enum gft_profile_ip_version (use enum gft_profile_ip_version) */
#define GFT_PROFILE_KEY_IP_VERSION_SHIFT		0
#define GFT_PROFILE_KEY_TUNNEL_IP_VERSION_MASK		0x1	/* HSI_COMMENT: use enum gft_profile_ip_version (use enum gft_profile_ip_version) */
#define GFT_PROFILE_KEY_TUNNEL_IP_VERSION_SHIFT		1
#define GFT_PROFILE_KEY_UPPER_PROTOCOL_TYPE_MASK	0xF	/* HSI_COMMENT: use enum gft_profile_upper_protocol_type (use enum gft_profile_upper_protocol_type) */
#define GFT_PROFILE_KEY_UPPER_PROTOCOL_TYPE_SHIFT	2
#define GFT_PROFILE_KEY_TUNNEL_TYPE_MASK		0xF	/* HSI_COMMENT: use enum gft_profile_tunnel_type (use enum gft_profile_tunnel_type) */
#define GFT_PROFILE_KEY_TUNNEL_TYPE_SHIFT		6
#define GFT_PROFILE_KEY_PF_ID_MASK			0xF
#define GFT_PROFILE_KEY_PF_ID_SHIFT			10
#define GFT_PROFILE_KEY_RESERVED0_MASK			0x3
#define GFT_PROFILE_KEY_RESERVED0_SHIFT			14
};

/* Used in gft_profile_key: Indication for tunnel type */
enum gft_profile_tunnel_type {
	GFT_PROFILE_NO_TUNNEL = 0,
	GFT_PROFILE_VXLAN_TUNNEL = 1,
	GFT_PROFILE_GRE_MAC_OR_NVGRE_TUNNEL = 2,
	GFT_PROFILE_GRE_IP_TUNNEL = 3,
	GFT_PROFILE_GENEVE_MAC_TUNNEL = 4,
	GFT_PROFILE_GENEVE_IP_TUNNEL = 5,
	MAX_GFT_PROFILE_TUNNEL_TYPE
};

/* Used in gft_profile_key: Indication for protocol type */
enum gft_profile_upper_protocol_type {
	GFT_PROFILE_ROCE_PROTOCOL = 0,
	GFT_PROFILE_RROCE_PROTOCOL = 1,
	GFT_PROFILE_FCOE_PROTOCOL = 2,
	GFT_PROFILE_ICMP_PROTOCOL = 3,
	GFT_PROFILE_ARP_PROTOCOL = 4,
	GFT_PROFILE_USER_TCP_SRC_PORT_1_INNER = 5,
	GFT_PROFILE_USER_TCP_DST_PORT_1_INNER = 6,
	GFT_PROFILE_TCP_PROTOCOL = 7,
	GFT_PROFILE_USER_UDP_DST_PORT_1_INNER = 8,
	GFT_PROFILE_USER_UDP_DST_PORT_2_OUTER = 9,
	GFT_PROFILE_UDP_PROTOCOL = 10,
	GFT_PROFILE_USER_IP_1_INNER = 11,
	GFT_PROFILE_USER_IP_2_OUTER = 12,
	GFT_PROFILE_USER_ETH_1_INNER = 13,
	GFT_PROFILE_USER_ETH_2_OUTER = 14,
	GFT_PROFILE_RAW = 15,
	MAX_GFT_PROFILE_UPPER_PROTOCOL_TYPE
};

/* GFT RAM line struct */
struct gft_ram_line {
	__le32 lo;
#define GFT_RAM_LINE_VLAN_SELECT_MASK			0x3	/* HSI_COMMENT:  (use enum gft_vlan_select) */
#define GFT_RAM_LINE_VLAN_SELECT_SHIFT			0
#define GFT_RAM_LINE_TUNNEL_ENTROPHY_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_ENTROPHY_SHIFT		2
#define GFT_RAM_LINE_TUNNEL_TTL_EQUAL_ONE_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_TTL_EQUAL_ONE_SHIFT		3
#define GFT_RAM_LINE_TUNNEL_TTL_MASK			0x1
#define GFT_RAM_LINE_TUNNEL_TTL_SHIFT			4
#define GFT_RAM_LINE_TUNNEL_ETHERTYPE_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_ETHERTYPE_SHIFT		5
#define GFT_RAM_LINE_TUNNEL_DST_PORT_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_DST_PORT_SHIFT		6
#define GFT_RAM_LINE_TUNNEL_SRC_PORT_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_SRC_PORT_SHIFT		7
#define GFT_RAM_LINE_TUNNEL_DSCP_MASK			0x1
#define GFT_RAM_LINE_TUNNEL_DSCP_SHIFT			8
#define GFT_RAM_LINE_TUNNEL_OVER_IP_PROTOCOL_MASK	0x1
#define GFT_RAM_LINE_TUNNEL_OVER_IP_PROTOCOL_SHIFT	9
#define GFT_RAM_LINE_TUNNEL_DST_IP_MASK			0x1
#define GFT_RAM_LINE_TUNNEL_DST_IP_SHIFT		10
#define GFT_RAM_LINE_TUNNEL_SRC_IP_MASK			0x1
#define GFT_RAM_LINE_TUNNEL_SRC_IP_SHIFT		11
#define GFT_RAM_LINE_TUNNEL_PRIORITY_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_PRIORITY_SHIFT		12
#define GFT_RAM_LINE_TUNNEL_PROVIDER_VLAN_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_PROVIDER_VLAN_SHIFT		13
#define GFT_RAM_LINE_TUNNEL_VLAN_MASK			0x1
#define GFT_RAM_LINE_TUNNEL_VLAN_SHIFT			14
#define GFT_RAM_LINE_TUNNEL_DST_MAC_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_DST_MAC_SHIFT		15
#define GFT_RAM_LINE_TUNNEL_SRC_MAC_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_SRC_MAC_SHIFT		16
#define GFT_RAM_LINE_TTL_EQUAL_ONE_MASK			0x1
#define GFT_RAM_LINE_TTL_EQUAL_ONE_SHIFT		17
#define GFT_RAM_LINE_TTL_MASK				0x1
#define GFT_RAM_LINE_TTL_SHIFT				18
#define GFT_RAM_LINE_ETHERTYPE_MASK			0x1
#define GFT_RAM_LINE_ETHERTYPE_SHIFT			19
#define GFT_RAM_LINE_RESERVED0_MASK			0x1
#define GFT_RAM_LINE_RESERVED0_SHIFT			20
#define GFT_RAM_LINE_TCP_FLAG_FIN_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_FIN_SHIFT			21
#define GFT_RAM_LINE_TCP_FLAG_SYN_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_SYN_SHIFT			22
#define GFT_RAM_LINE_TCP_FLAG_RST_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_RST_SHIFT			23
#define GFT_RAM_LINE_TCP_FLAG_PSH_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_PSH_SHIFT			24
#define GFT_RAM_LINE_TCP_FLAG_ACK_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_ACK_SHIFT			25
#define GFT_RAM_LINE_TCP_FLAG_URG_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_URG_SHIFT			26
#define GFT_RAM_LINE_TCP_FLAG_ECE_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_ECE_SHIFT			27
#define GFT_RAM_LINE_TCP_FLAG_CWR_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_CWR_SHIFT			28
#define GFT_RAM_LINE_TCP_FLAG_NS_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_NS_SHIFT			29
#define GFT_RAM_LINE_DST_PORT_MASK			0x1
#define GFT_RAM_LINE_DST_PORT_SHIFT			30
#define GFT_RAM_LINE_SRC_PORT_MASK			0x1
#define GFT_RAM_LINE_SRC_PORT_SHIFT			31
	__le32 hi;
#define GFT_RAM_LINE_DSCP_MASK				0x1
#define GFT_RAM_LINE_DSCP_SHIFT				0
#define GFT_RAM_LINE_OVER_IP_PROTOCOL_MASK		0x1
#define GFT_RAM_LINE_OVER_IP_PROTOCOL_SHIFT		1
#define GFT_RAM_LINE_DST_IP_MASK			0x1
#define GFT_RAM_LINE_DST_IP_SHIFT			2
#define GFT_RAM_LINE_SRC_IP_MASK			0x1
#define GFT_RAM_LINE_SRC_IP_SHIFT			3
#define GFT_RAM_LINE_PRIORITY_MASK			0x1
#define GFT_RAM_LINE_PRIORITY_SHIFT			4
#define GFT_RAM_LINE_PROVIDER_VLAN_MASK			0x1
#define GFT_RAM_LINE_PROVIDER_VLAN_SHIFT		5
#define GFT_RAM_LINE_VLAN_MASK				0x1
#define GFT_RAM_LINE_VLAN_SHIFT				6
#define GFT_RAM_LINE_DST_MAC_MASK			0x1
#define GFT_RAM_LINE_DST_MAC_SHIFT			7
#define GFT_RAM_LINE_SRC_MAC_MASK			0x1
#define GFT_RAM_LINE_SRC_MAC_SHIFT			8
#define GFT_RAM_LINE_TENANT_ID_MASK			0x1
#define GFT_RAM_LINE_TENANT_ID_SHIFT			9
#define GFT_RAM_LINE_RESERVED1_MASK			0x3FFFFF
#define GFT_RAM_LINE_RESERVED1_SHIFT			10
};

/* Used in the first 2 bits for gft_ram_line: Indication for vlan mask */
enum gft_vlan_select {
	INNER_PROVIDER_VLAN = 0,
	INNER_VLAN = 1,
	OUTER_PROVIDER_VLAN = 2,
	OUTER_VLAN = 3,
	MAX_GFT_VLAN_SELECT
};

struct mstorm_eth_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define MSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM0_MASK	0x1	/* HSI_COMMENT: exist_in_qm0 */
#define MSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM0_SHIFT	0
#define MSTORM_ETH_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define MSTORM_ETH_CONN_AG_CTX_BIT1_SHIFT		1
#define MSTORM_ETH_CONN_AG_CTX_CF0_MASK			0x3	/* HSI_COMMENT: cf0 */
#define MSTORM_ETH_CONN_AG_CTX_CF0_SHIFT		2
#define MSTORM_ETH_CONN_AG_CTX_CF1_MASK			0x3	/* HSI_COMMENT: cf1 */
#define MSTORM_ETH_CONN_AG_CTX_CF1_SHIFT		4
#define MSTORM_ETH_CONN_AG_CTX_CF2_MASK			0x3	/* HSI_COMMENT: cf2 */
#define MSTORM_ETH_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define MSTORM_ETH_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define MSTORM_ETH_CONN_AG_CTX_CF0EN_SHIFT		0
#define MSTORM_ETH_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define MSTORM_ETH_CONN_AG_CTX_CF1EN_SHIFT		1
#define MSTORM_ETH_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define MSTORM_ETH_CONN_AG_CTX_CF2EN_SHIFT		2
#define MSTORM_ETH_CONN_AG_CTX_RULE0EN_MASK		0x1	/* HSI_COMMENT: rule0en */
#define MSTORM_ETH_CONN_AG_CTX_RULE0EN_SHIFT		3
#define MSTORM_ETH_CONN_AG_CTX_RULE1EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define MSTORM_ETH_CONN_AG_CTX_RULE1EN_SHIFT		4
#define MSTORM_ETH_CONN_AG_CTX_RULE2EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define MSTORM_ETH_CONN_AG_CTX_RULE2EN_SHIFT		5
#define MSTORM_ETH_CONN_AG_CTX_RULE3EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define MSTORM_ETH_CONN_AG_CTX_RULE3EN_SHIFT		6
#define MSTORM_ETH_CONN_AG_CTX_RULE4EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define MSTORM_ETH_CONN_AG_CTX_RULE4EN_SHIFT		7
	__le16 word0;		/* HSI_COMMENT: word0 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
};

struct xstorm_eth_hw_conn_ag_ctx {
	u8 reserved0;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define XSTORM_ETH_HW_CONN_AG_CTX_EXIST_IN_QM0_MASK			0x1	/* HSI_COMMENT: exist_in_qm0 */
#define XSTORM_ETH_HW_CONN_AG_CTX_EXIST_IN_QM0_SHIFT			0
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED1_SHIFT			1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED2_MASK			0x1	/* HSI_COMMENT: exist_in_qm2 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED2_SHIFT			2
#define XSTORM_ETH_HW_CONN_AG_CTX_EXIST_IN_QM3_MASK			0x1	/* HSI_COMMENT: exist_in_qm3 */
#define XSTORM_ETH_HW_CONN_AG_CTX_EXIST_IN_QM3_SHIFT			3
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED3_MASK			0x1	/* HSI_COMMENT: bit4 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED3_SHIFT			4
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED4_MASK			0x1	/* HSI_COMMENT: cf_array_active */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED4_SHIFT			5
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED5_MASK			0x1	/* HSI_COMMENT: bit6 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED5_SHIFT			6
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED6_MASK			0x1	/* HSI_COMMENT: bit7 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED6_SHIFT			7
	u8 flags1;
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED7_MASK			0x1	/* HSI_COMMENT: bit8 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED7_SHIFT			0
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED8_MASK			0x1	/* HSI_COMMENT: bit9 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED8_SHIFT			1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED9_MASK			0x1	/* HSI_COMMENT: bit10 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED9_SHIFT			2
#define XSTORM_ETH_HW_CONN_AG_CTX_BIT11_MASK				0x1	/* HSI_COMMENT: bit11 */
#define XSTORM_ETH_HW_CONN_AG_CTX_BIT11_SHIFT				3
#define XSTORM_ETH_HW_CONN_AG_CTX_E5_RESERVED2_MASK			0x1	/* HSI_COMMENT: bit12 */
#define XSTORM_ETH_HW_CONN_AG_CTX_E5_RESERVED2_SHIFT			4
#define XSTORM_ETH_HW_CONN_AG_CTX_E5_RESERVED3_MASK			0x1	/* HSI_COMMENT: bit13 */
#define XSTORM_ETH_HW_CONN_AG_CTX_E5_RESERVED3_SHIFT			5
#define XSTORM_ETH_HW_CONN_AG_CTX_TX_RULE_ACTIVE_MASK			0x1	/* HSI_COMMENT: bit14 */
#define XSTORM_ETH_HW_CONN_AG_CTX_TX_RULE_ACTIVE_SHIFT			6
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_ACTIVE_MASK			0x1	/* HSI_COMMENT: bit15 */
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_ACTIVE_SHIFT			7
	u8 flags2;
#define XSTORM_ETH_HW_CONN_AG_CTX_CF0_MASK				0x3	/* HSI_COMMENT: timer0cf */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF0_SHIFT				0
#define XSTORM_ETH_HW_CONN_AG_CTX_CF1_MASK				0x3	/* HSI_COMMENT: timer1cf */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF1_SHIFT				2
#define XSTORM_ETH_HW_CONN_AG_CTX_CF2_MASK				0x3	/* HSI_COMMENT: timer2cf */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF2_SHIFT				4
#define XSTORM_ETH_HW_CONN_AG_CTX_CF3_MASK				0x3	/* HSI_COMMENT: timer_stop_all */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF3_SHIFT				6
	u8 flags3;
#define XSTORM_ETH_HW_CONN_AG_CTX_CF4_MASK				0x3	/* HSI_COMMENT: cf4 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF4_SHIFT				0
#define XSTORM_ETH_HW_CONN_AG_CTX_CF5_MASK				0x3	/* HSI_COMMENT: cf5 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF5_SHIFT				2
#define XSTORM_ETH_HW_CONN_AG_CTX_CF6_MASK				0x3	/* HSI_COMMENT: cf6 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF6_SHIFT				4
#define XSTORM_ETH_HW_CONN_AG_CTX_CF7_MASK				0x3	/* HSI_COMMENT: cf7 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF7_SHIFT				6
	u8 flags4;
#define XSTORM_ETH_HW_CONN_AG_CTX_CF8_MASK				0x3	/* HSI_COMMENT: cf8 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF8_SHIFT				0
#define XSTORM_ETH_HW_CONN_AG_CTX_CF9_MASK				0x3	/* HSI_COMMENT: cf9 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF9_SHIFT				2
#define XSTORM_ETH_HW_CONN_AG_CTX_CF10_MASK				0x3	/* HSI_COMMENT: cf10 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF10_SHIFT				4
#define XSTORM_ETH_HW_CONN_AG_CTX_CF11_MASK				0x3	/* HSI_COMMENT: cf11 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF11_SHIFT				6
	u8 flags5;
#define XSTORM_ETH_HW_CONN_AG_CTX_CF12_MASK				0x3	/* HSI_COMMENT: cf12 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF12_SHIFT				0
#define XSTORM_ETH_HW_CONN_AG_CTX_CF13_MASK				0x3	/* HSI_COMMENT: cf13 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF13_SHIFT				2
#define XSTORM_ETH_HW_CONN_AG_CTX_CF14_MASK				0x3	/* HSI_COMMENT: cf14 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF14_SHIFT				4
#define XSTORM_ETH_HW_CONN_AG_CTX_CF15_MASK				0x3	/* HSI_COMMENT: cf15 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF15_SHIFT				6
	u8 flags6;
#define XSTORM_ETH_HW_CONN_AG_CTX_GO_TO_BD_CONS_CF_MASK			0x3	/* HSI_COMMENT: cf16 */
#define XSTORM_ETH_HW_CONN_AG_CTX_GO_TO_BD_CONS_CF_SHIFT		0
#define XSTORM_ETH_HW_CONN_AG_CTX_MULTI_UNICAST_CF_MASK			0x3	/* HSI_COMMENT: cf_array_cf */
#define XSTORM_ETH_HW_CONN_AG_CTX_MULTI_UNICAST_CF_SHIFT		2
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_MASK				0x3	/* HSI_COMMENT: cf18 */
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_SHIFT				4
#define XSTORM_ETH_HW_CONN_AG_CTX_TERMINATE_CF_MASK			0x3	/* HSI_COMMENT: cf19 */
#define XSTORM_ETH_HW_CONN_AG_CTX_TERMINATE_CF_SHIFT			6
	u8 flags7;
#define XSTORM_ETH_HW_CONN_AG_CTX_FLUSH_Q0_MASK				0x3	/* HSI_COMMENT: cf20 */
#define XSTORM_ETH_HW_CONN_AG_CTX_FLUSH_Q0_SHIFT			0
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED10_MASK			0x3	/* HSI_COMMENT: cf21 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED10_SHIFT			2
#define XSTORM_ETH_HW_CONN_AG_CTX_SLOW_PATH_MASK			0x3	/* HSI_COMMENT: cf22 */
#define XSTORM_ETH_HW_CONN_AG_CTX_SLOW_PATH_SHIFT			4
#define XSTORM_ETH_HW_CONN_AG_CTX_CF0EN_MASK				0x1	/* HSI_COMMENT: cf0en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF0EN_SHIFT				6
#define XSTORM_ETH_HW_CONN_AG_CTX_CF1EN_MASK				0x1	/* HSI_COMMENT: cf1en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF1EN_SHIFT				7
	u8 flags8;
#define XSTORM_ETH_HW_CONN_AG_CTX_CF2EN_MASK				0x1	/* HSI_COMMENT: cf2en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF2EN_SHIFT				0
#define XSTORM_ETH_HW_CONN_AG_CTX_CF3EN_MASK				0x1	/* HSI_COMMENT: cf3en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF3EN_SHIFT				1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF4EN_MASK				0x1	/* HSI_COMMENT: cf4en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF4EN_SHIFT				2
#define XSTORM_ETH_HW_CONN_AG_CTX_CF5EN_MASK				0x1	/* HSI_COMMENT: cf5en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF5EN_SHIFT				3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF6EN_MASK				0x1	/* HSI_COMMENT: cf6en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF6EN_SHIFT				4
#define XSTORM_ETH_HW_CONN_AG_CTX_CF7EN_MASK				0x1	/* HSI_COMMENT: cf7en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF7EN_SHIFT				5
#define XSTORM_ETH_HW_CONN_AG_CTX_CF8EN_MASK				0x1	/* HSI_COMMENT: cf8en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF8EN_SHIFT				6
#define XSTORM_ETH_HW_CONN_AG_CTX_CF9EN_MASK				0x1	/* HSI_COMMENT: cf9en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF9EN_SHIFT				7
	u8 flags9;
#define XSTORM_ETH_HW_CONN_AG_CTX_CF10EN_MASK				0x1	/* HSI_COMMENT: cf10en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF10EN_SHIFT				0
#define XSTORM_ETH_HW_CONN_AG_CTX_CF11EN_MASK				0x1	/* HSI_COMMENT: cf11en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF11EN_SHIFT				1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF12EN_MASK				0x1	/* HSI_COMMENT: cf12en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF12EN_SHIFT				2
#define XSTORM_ETH_HW_CONN_AG_CTX_CF13EN_MASK				0x1	/* HSI_COMMENT: cf13en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF13EN_SHIFT				3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF14EN_MASK				0x1	/* HSI_COMMENT: cf14en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF14EN_SHIFT				4
#define XSTORM_ETH_HW_CONN_AG_CTX_CF15EN_MASK				0x1	/* HSI_COMMENT: cf15en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF15EN_SHIFT				5
#define XSTORM_ETH_HW_CONN_AG_CTX_GO_TO_BD_CONS_CF_EN_MASK		0x1	/* HSI_COMMENT: cf16en */
#define XSTORM_ETH_HW_CONN_AG_CTX_GO_TO_BD_CONS_CF_EN_SHIFT		6
#define XSTORM_ETH_HW_CONN_AG_CTX_MULTI_UNICAST_CF_EN_MASK		0x1	/* HSI_COMMENT: cf_array_cf_en */
#define XSTORM_ETH_HW_CONN_AG_CTX_MULTI_UNICAST_CF_EN_SHIFT		7
	u8 flags10;
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_EN_MASK				0x1	/* HSI_COMMENT: cf18en */
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_EN_SHIFT			0
#define XSTORM_ETH_HW_CONN_AG_CTX_TERMINATE_CF_EN_MASK			0x1	/* HSI_COMMENT: cf19en */
#define XSTORM_ETH_HW_CONN_AG_CTX_TERMINATE_CF_EN_SHIFT			1
#define XSTORM_ETH_HW_CONN_AG_CTX_FLUSH_Q0_EN_MASK			0x1	/* HSI_COMMENT: cf20en */
#define XSTORM_ETH_HW_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT			2
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED11_MASK			0x1	/* HSI_COMMENT: cf21en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED11_SHIFT			3
#define XSTORM_ETH_HW_CONN_AG_CTX_SLOW_PATH_EN_MASK			0x1	/* HSI_COMMENT: cf22en */
#define XSTORM_ETH_HW_CONN_AG_CTX_SLOW_PATH_EN_SHIFT			4
#define XSTORM_ETH_HW_CONN_AG_CTX_TPH_ENABLE_EN_RESERVED_MASK		0x1	/* HSI_COMMENT: cf23en */
#define XSTORM_ETH_HW_CONN_AG_CTX_TPH_ENABLE_EN_RESERVED_SHIFT		5
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED12_MASK			0x1	/* HSI_COMMENT: rule0en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED12_SHIFT			6
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED13_MASK			0x1	/* HSI_COMMENT: rule1en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED13_SHIFT			7
	u8 flags11;
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED14_MASK			0x1	/* HSI_COMMENT: rule2en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED14_SHIFT			0
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED15_MASK			0x1	/* HSI_COMMENT: rule3en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED15_SHIFT			1
#define XSTORM_ETH_HW_CONN_AG_CTX_TX_DEC_RULE_EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define XSTORM_ETH_HW_CONN_AG_CTX_TX_DEC_RULE_EN_SHIFT			2
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE5EN_MASK				0x1	/* HSI_COMMENT: rule5en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE5EN_SHIFT				3
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE6EN_MASK				0x1	/* HSI_COMMENT: rule6en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE6EN_SHIFT				4
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE7EN_MASK				0x1	/* HSI_COMMENT: rule7en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE7EN_SHIFT				5
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED1_MASK			0x1	/* HSI_COMMENT: rule8en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED1_SHIFT			6
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE9EN_MASK				0x1	/* HSI_COMMENT: rule9en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE9EN_SHIFT				7
	u8 flags12;
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE10EN_MASK				0x1	/* HSI_COMMENT: rule10en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE10EN_SHIFT			0
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE11EN_MASK				0x1	/* HSI_COMMENT: rule11en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE11EN_SHIFT			1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED2_MASK			0x1	/* HSI_COMMENT: rule12en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED2_SHIFT			2
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED3_MASK			0x1	/* HSI_COMMENT: rule13en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED3_SHIFT			3
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE14EN_MASK				0x1	/* HSI_COMMENT: rule14en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE14EN_SHIFT			4
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE15EN_MASK				0x1	/* HSI_COMMENT: rule15en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE15EN_SHIFT			5
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE16EN_MASK				0x1	/* HSI_COMMENT: rule16en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE16EN_SHIFT			6
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE17EN_MASK				0x1	/* HSI_COMMENT: rule17en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE17EN_SHIFT			7
	u8 flags13;
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE18EN_MASK				0x1	/* HSI_COMMENT: rule18en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE18EN_SHIFT			0
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE19EN_MASK				0x1	/* HSI_COMMENT: rule19en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE19EN_SHIFT			1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED4_MASK			0x1	/* HSI_COMMENT: rule20en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED4_SHIFT			2
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED5_MASK			0x1	/* HSI_COMMENT: rule21en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED5_SHIFT			3
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED6_MASK			0x1	/* HSI_COMMENT: rule22en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED6_SHIFT			4
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED7_MASK			0x1	/* HSI_COMMENT: rule23en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED7_SHIFT			5
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED8_MASK			0x1	/* HSI_COMMENT: rule24en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED8_SHIFT			6
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED9_MASK			0x1	/* HSI_COMMENT: rule25en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED9_SHIFT			7
	u8 flags14;
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_USE_EXT_HDR_MASK			0x1	/* HSI_COMMENT: bit16 */
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_USE_EXT_HDR_SHIFT		0
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_SEND_RAW_L3L4_MASK		0x1	/* HSI_COMMENT: bit17 */
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_SEND_RAW_L3L4_SHIFT		1
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_INBAND_PROP_HDR_MASK		0x1	/* HSI_COMMENT: bit18 */
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_INBAND_PROP_HDR_SHIFT		2
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_SEND_EXT_TUNNEL_MASK		0x1	/* HSI_COMMENT: bit19 */
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_SEND_EXT_TUNNEL_SHIFT		3
#define XSTORM_ETH_HW_CONN_AG_CTX_L2_EDPM_ENABLE_MASK			0x1	/* HSI_COMMENT: bit20 */
#define XSTORM_ETH_HW_CONN_AG_CTX_L2_EDPM_ENABLE_SHIFT			4
#define XSTORM_ETH_HW_CONN_AG_CTX_ROCE_EDPM_ENABLE_MASK			0x1	/* HSI_COMMENT: bit21 */
#define XSTORM_ETH_HW_CONN_AG_CTX_ROCE_EDPM_ENABLE_SHIFT		5
#define XSTORM_ETH_HW_CONN_AG_CTX_TPH_ENABLE_MASK			0x3	/* HSI_COMMENT: cf23 */
#define XSTORM_ETH_HW_CONN_AG_CTX_TPH_ENABLE_SHIFT			6
	u8 edpm_event_id;	/* HSI_COMMENT: byte2 */
	__le16 physical_q0;	/* HSI_COMMENT: physical_q0 */
	__le16 e5_reserved1;	/* HSI_COMMENT: physical_q1 */
	__le16 edpm_num_bds;	/* HSI_COMMENT: physical_q2 */
	__le16 tx_bd_cons;	/* HSI_COMMENT: word3 */
	__le16 tx_bd_prod;	/* HSI_COMMENT: word4 */
	__le16 updated_qm_pq_id;	/* HSI_COMMENT: word5 */
	__le16 conn_dpi;	/* HSI_COMMENT: conn_dpi */
};

/************************************************************************/
/* Add include to common TCP target */
/************************************************************************/

/********************/
/* TOE FW CONSTANTS */
/********************/

#define TOE_MAX_RAMROD_PER_PF			8
#define TOE_TX_PAGE_SIZE_BYTES			4096
#define TOE_GRQ_PAGE_SIZE_BYTES			4096
#define TOE_RX_CQ_PAGE_SIZE_BYTES		4096

#define TOE_RX_MAX_RSS_CHAINS			64
#define TOE_TX_MAX_TSS_CHAINS			64
#define TOE_RSS_INDIRECTION_TABLE_SIZE		128

/* The toe storm context of Mstorm */
struct mstorm_toe_conn_st_ctx {
	__le32 reserved[24];
};

/* The toe storm context of Pstorm */
struct pstorm_toe_conn_st_ctx {
	__le32 reserved[36];
};

/* The toe storm context of Ystorm */
struct ystorm_toe_conn_st_ctx {
	__le32 reserved[8];
};

/* The toe storm context of Xstorm */
struct xstorm_toe_conn_st_ctx {
	__le32 reserved[44];
};

struct ystorm_toe_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define YSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define YSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define YSTORM_TOE_CONN_AG_CTX_BIT1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define YSTORM_TOE_CONN_AG_CTX_BIT1_SHIFT			1
#define YSTORM_TOE_CONN_AG_CTX_SLOW_PATH_CF_MASK		0x3	/* HSI_COMMENT: cf0 */
#define YSTORM_TOE_CONN_AG_CTX_SLOW_PATH_CF_SHIFT		2
#define YSTORM_TOE_CONN_AG_CTX_RESET_RECEIVED_CF_MASK		0x3	/* HSI_COMMENT: cf1 */
#define YSTORM_TOE_CONN_AG_CTX_RESET_RECEIVED_CF_SHIFT		4
#define YSTORM_TOE_CONN_AG_CTX_CF2_MASK				0x3	/* HSI_COMMENT: cf2 */
#define YSTORM_TOE_CONN_AG_CTX_CF2_SHIFT			6
	u8 flags1;
#define YSTORM_TOE_CONN_AG_CTX_SLOW_PATH_CF_EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define YSTORM_TOE_CONN_AG_CTX_SLOW_PATH_CF_EN_SHIFT		0
#define YSTORM_TOE_CONN_AG_CTX_RESET_RECEIVED_CF_EN_MASK	0x1	/* HSI_COMMENT: cf1en */
#define YSTORM_TOE_CONN_AG_CTX_RESET_RECEIVED_CF_EN_SHIFT	1
#define YSTORM_TOE_CONN_AG_CTX_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define YSTORM_TOE_CONN_AG_CTX_CF2EN_SHIFT			2
#define YSTORM_TOE_CONN_AG_CTX_REL_SEQ_EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define YSTORM_TOE_CONN_AG_CTX_REL_SEQ_EN_SHIFT			3
#define YSTORM_TOE_CONN_AG_CTX_RULE1EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define YSTORM_TOE_CONN_AG_CTX_RULE1EN_SHIFT			4
#define YSTORM_TOE_CONN_AG_CTX_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define YSTORM_TOE_CONN_AG_CTX_RULE2EN_SHIFT			5
#define YSTORM_TOE_CONN_AG_CTX_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define YSTORM_TOE_CONN_AG_CTX_RULE3EN_SHIFT			6
#define YSTORM_TOE_CONN_AG_CTX_CONS_PROD_EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define YSTORM_TOE_CONN_AG_CTX_CONS_PROD_EN_SHIFT		7
	u8 completion_opcode;	/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: word0 */
	__le32 rel_seq;		/* HSI_COMMENT: reg0 */
	__le32 rel_seq_threshold;	/* HSI_COMMENT: reg1 */
	__le16 app_prod;	/* HSI_COMMENT: word1 */
	__le16 app_cons;	/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le16 word4;		/* HSI_COMMENT: word4 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
};

struct xstorm_toe_conn_ag_ctx {
	u8 reserved0;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define XSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define XSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define XSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define XSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM1_SHIFT		1
#define XSTORM_TOE_CONN_AG_CTX_RESERVED1_MASK			0x1	/* HSI_COMMENT: exist_in_qm2 */
#define XSTORM_TOE_CONN_AG_CTX_RESERVED1_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM3_MASK		0x1	/* HSI_COMMENT: exist_in_qm3 */
#define XSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM3_SHIFT		3
#define XSTORM_TOE_CONN_AG_CTX_TX_DEC_RULE_RES_MASK		0x1	/* HSI_COMMENT: bit4 */
#define XSTORM_TOE_CONN_AG_CTX_TX_DEC_RULE_RES_SHIFT		4
#define XSTORM_TOE_CONN_AG_CTX_RESERVED2_MASK			0x1	/* HSI_COMMENT: cf_array_active */
#define XSTORM_TOE_CONN_AG_CTX_RESERVED2_SHIFT			5
#define XSTORM_TOE_CONN_AG_CTX_BIT6_MASK			0x1	/* HSI_COMMENT: bit6 */
#define XSTORM_TOE_CONN_AG_CTX_BIT6_SHIFT			6
#define XSTORM_TOE_CONN_AG_CTX_BIT7_MASK			0x1	/* HSI_COMMENT: bit7 */
#define XSTORM_TOE_CONN_AG_CTX_BIT7_SHIFT			7
	u8 flags1;
#define XSTORM_TOE_CONN_AG_CTX_BIT8_MASK			0x1	/* HSI_COMMENT: bit8 */
#define XSTORM_TOE_CONN_AG_CTX_BIT8_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_BIT9_MASK			0x1	/* HSI_COMMENT: bit9 */
#define XSTORM_TOE_CONN_AG_CTX_BIT9_SHIFT			1
#define XSTORM_TOE_CONN_AG_CTX_BIT10_MASK			0x1	/* HSI_COMMENT: bit10 */
#define XSTORM_TOE_CONN_AG_CTX_BIT10_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_BIT11_MASK			0x1	/* HSI_COMMENT: bit11 */
#define XSTORM_TOE_CONN_AG_CTX_BIT11_SHIFT			3
#define XSTORM_TOE_CONN_AG_CTX_BIT12_MASK			0x1	/* HSI_COMMENT: bit12 */
#define XSTORM_TOE_CONN_AG_CTX_BIT12_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_BIT13_MASK			0x1	/* HSI_COMMENT: bit13 */
#define XSTORM_TOE_CONN_AG_CTX_BIT13_SHIFT			5
#define XSTORM_TOE_CONN_AG_CTX_BIT14_MASK			0x1	/* HSI_COMMENT: bit14 */
#define XSTORM_TOE_CONN_AG_CTX_BIT14_SHIFT			6
#define XSTORM_TOE_CONN_AG_CTX_BIT15_MASK			0x1	/* HSI_COMMENT: bit15 */
#define XSTORM_TOE_CONN_AG_CTX_BIT15_SHIFT			7
	u8 flags2;
#define XSTORM_TOE_CONN_AG_CTX_CF0_MASK				0x3	/* HSI_COMMENT: timer0cf */
#define XSTORM_TOE_CONN_AG_CTX_CF0_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_CF1_MASK				0x3	/* HSI_COMMENT: timer1cf */
#define XSTORM_TOE_CONN_AG_CTX_CF1_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_CF2_MASK				0x3	/* HSI_COMMENT: timer2cf */
#define XSTORM_TOE_CONN_AG_CTX_CF2_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_MASK		0x3	/* HSI_COMMENT: timer_stop_all */
#define XSTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_SHIFT		6
	u8 flags3;
#define XSTORM_TOE_CONN_AG_CTX_CF4_MASK				0x3	/* HSI_COMMENT: cf4 */
#define XSTORM_TOE_CONN_AG_CTX_CF4_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_CF5_MASK				0x3	/* HSI_COMMENT: cf5 */
#define XSTORM_TOE_CONN_AG_CTX_CF5_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_CF6_MASK				0x3	/* HSI_COMMENT: cf6 */
#define XSTORM_TOE_CONN_AG_CTX_CF6_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_CF7_MASK				0x3	/* HSI_COMMENT: cf7 */
#define XSTORM_TOE_CONN_AG_CTX_CF7_SHIFT			6
	u8 flags4;
#define XSTORM_TOE_CONN_AG_CTX_CF8_MASK				0x3	/* HSI_COMMENT: cf8 */
#define XSTORM_TOE_CONN_AG_CTX_CF8_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_CF9_MASK				0x3	/* HSI_COMMENT: cf9 */
#define XSTORM_TOE_CONN_AG_CTX_CF9_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_CF10_MASK			0x3	/* HSI_COMMENT: cf10 */
#define XSTORM_TOE_CONN_AG_CTX_CF10_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_CF11_MASK			0x3	/* HSI_COMMENT: cf11 */
#define XSTORM_TOE_CONN_AG_CTX_CF11_SHIFT			6
	u8 flags5;
#define XSTORM_TOE_CONN_AG_CTX_CF12_MASK			0x3	/* HSI_COMMENT: cf12 */
#define XSTORM_TOE_CONN_AG_CTX_CF12_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_CF13_MASK			0x3	/* HSI_COMMENT: cf13 */
#define XSTORM_TOE_CONN_AG_CTX_CF13_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_CF14_MASK			0x3	/* HSI_COMMENT: cf14 */
#define XSTORM_TOE_CONN_AG_CTX_CF14_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_CF15_MASK			0x3	/* HSI_COMMENT: cf15 */
#define XSTORM_TOE_CONN_AG_CTX_CF15_SHIFT			6
	u8 flags6;
#define XSTORM_TOE_CONN_AG_CTX_CF16_MASK			0x3	/* HSI_COMMENT: cf16 */
#define XSTORM_TOE_CONN_AG_CTX_CF16_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_CF17_MASK			0x3	/* HSI_COMMENT: cf_array_cf */
#define XSTORM_TOE_CONN_AG_CTX_CF17_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_CF18_MASK			0x3	/* HSI_COMMENT: cf18 */
#define XSTORM_TOE_CONN_AG_CTX_CF18_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_DQ_FLUSH_MASK			0x3	/* HSI_COMMENT: cf19 */
#define XSTORM_TOE_CONN_AG_CTX_DQ_FLUSH_SHIFT			6
	u8 flags7;
#define XSTORM_TOE_CONN_AG_CTX_FLUSH_Q0_MASK			0x3	/* HSI_COMMENT: cf20 */
#define XSTORM_TOE_CONN_AG_CTX_FLUSH_Q0_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_FLUSH_Q1_MASK			0x3	/* HSI_COMMENT: cf21 */
#define XSTORM_TOE_CONN_AG_CTX_FLUSH_Q1_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_SLOW_PATH_MASK			0x3	/* HSI_COMMENT: cf22 */
#define XSTORM_TOE_CONN_AG_CTX_SLOW_PATH_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_CF0EN_MASK			0x1	/* HSI_COMMENT: cf0en */
#define XSTORM_TOE_CONN_AG_CTX_CF0EN_SHIFT			6
#define XSTORM_TOE_CONN_AG_CTX_CF1EN_MASK			0x1	/* HSI_COMMENT: cf1en */
#define XSTORM_TOE_CONN_AG_CTX_CF1EN_SHIFT			7
	u8 flags8;
#define XSTORM_TOE_CONN_AG_CTX_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define XSTORM_TOE_CONN_AG_CTX_CF2EN_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_EN_MASK		0x1	/* HSI_COMMENT: cf3en */
#define XSTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_EN_SHIFT		1
#define XSTORM_TOE_CONN_AG_CTX_CF4EN_MASK			0x1	/* HSI_COMMENT: cf4en */
#define XSTORM_TOE_CONN_AG_CTX_CF4EN_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_CF5EN_MASK			0x1	/* HSI_COMMENT: cf5en */
#define XSTORM_TOE_CONN_AG_CTX_CF5EN_SHIFT			3
#define XSTORM_TOE_CONN_AG_CTX_CF6EN_MASK			0x1	/* HSI_COMMENT: cf6en */
#define XSTORM_TOE_CONN_AG_CTX_CF6EN_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_CF7EN_MASK			0x1	/* HSI_COMMENT: cf7en */
#define XSTORM_TOE_CONN_AG_CTX_CF7EN_SHIFT			5
#define XSTORM_TOE_CONN_AG_CTX_CF8EN_MASK			0x1	/* HSI_COMMENT: cf8en */
#define XSTORM_TOE_CONN_AG_CTX_CF8EN_SHIFT			6
#define XSTORM_TOE_CONN_AG_CTX_CF9EN_MASK			0x1	/* HSI_COMMENT: cf9en */
#define XSTORM_TOE_CONN_AG_CTX_CF9EN_SHIFT			7
	u8 flags9;
#define XSTORM_TOE_CONN_AG_CTX_CF10EN_MASK			0x1	/* HSI_COMMENT: cf10en */
#define XSTORM_TOE_CONN_AG_CTX_CF10EN_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_CF11EN_MASK			0x1	/* HSI_COMMENT: cf11en */
#define XSTORM_TOE_CONN_AG_CTX_CF11EN_SHIFT			1
#define XSTORM_TOE_CONN_AG_CTX_CF12EN_MASK			0x1	/* HSI_COMMENT: cf12en */
#define XSTORM_TOE_CONN_AG_CTX_CF12EN_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_CF13EN_MASK			0x1	/* HSI_COMMENT: cf13en */
#define XSTORM_TOE_CONN_AG_CTX_CF13EN_SHIFT			3
#define XSTORM_TOE_CONN_AG_CTX_CF14EN_MASK			0x1	/* HSI_COMMENT: cf14en */
#define XSTORM_TOE_CONN_AG_CTX_CF14EN_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_CF15EN_MASK			0x1	/* HSI_COMMENT: cf15en */
#define XSTORM_TOE_CONN_AG_CTX_CF15EN_SHIFT			5
#define XSTORM_TOE_CONN_AG_CTX_CF16EN_MASK			0x1	/* HSI_COMMENT: cf16en */
#define XSTORM_TOE_CONN_AG_CTX_CF16EN_SHIFT			6
#define XSTORM_TOE_CONN_AG_CTX_CF17EN_MASK			0x1	/* HSI_COMMENT: cf_array_cf_en */
#define XSTORM_TOE_CONN_AG_CTX_CF17EN_SHIFT			7
	u8 flags10;
#define XSTORM_TOE_CONN_AG_CTX_CF18EN_MASK			0x1	/* HSI_COMMENT: cf18en */
#define XSTORM_TOE_CONN_AG_CTX_CF18EN_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_DQ_FLUSH_EN_MASK			0x1	/* HSI_COMMENT: cf19en */
#define XSTORM_TOE_CONN_AG_CTX_DQ_FLUSH_EN_SHIFT		1
#define XSTORM_TOE_CONN_AG_CTX_FLUSH_Q0_EN_MASK			0x1	/* HSI_COMMENT: cf20en */
#define XSTORM_TOE_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT		2
#define XSTORM_TOE_CONN_AG_CTX_FLUSH_Q1_EN_MASK			0x1	/* HSI_COMMENT: cf21en */
#define XSTORM_TOE_CONN_AG_CTX_FLUSH_Q1_EN_SHIFT		3
#define XSTORM_TOE_CONN_AG_CTX_SLOW_PATH_EN_MASK		0x1	/* HSI_COMMENT: cf22en */
#define XSTORM_TOE_CONN_AG_CTX_SLOW_PATH_EN_SHIFT		4
#define XSTORM_TOE_CONN_AG_CTX_CF23EN_MASK			0x1	/* HSI_COMMENT: cf23en */
#define XSTORM_TOE_CONN_AG_CTX_CF23EN_SHIFT			5
#define XSTORM_TOE_CONN_AG_CTX_RULE0EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define XSTORM_TOE_CONN_AG_CTX_RULE0EN_SHIFT			6
#define XSTORM_TOE_CONN_AG_CTX_MORE_TO_SEND_RULE_EN_MASK	0x1	/* HSI_COMMENT: rule1en */
#define XSTORM_TOE_CONN_AG_CTX_MORE_TO_SEND_RULE_EN_SHIFT	7
	u8 flags11;
#define XSTORM_TOE_CONN_AG_CTX_TX_BLOCKED_EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define XSTORM_TOE_CONN_AG_CTX_TX_BLOCKED_EN_SHIFT		0
#define XSTORM_TOE_CONN_AG_CTX_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define XSTORM_TOE_CONN_AG_CTX_RULE3EN_SHIFT			1
#define XSTORM_TOE_CONN_AG_CTX_RESERVED3_MASK			0x1	/* HSI_COMMENT: rule4en */
#define XSTORM_TOE_CONN_AG_CTX_RESERVED3_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define XSTORM_TOE_CONN_AG_CTX_RULE5EN_SHIFT			3
#define XSTORM_TOE_CONN_AG_CTX_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define XSTORM_TOE_CONN_AG_CTX_RULE6EN_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_RULE7EN_MASK			0x1	/* HSI_COMMENT: rule7en */
#define XSTORM_TOE_CONN_AG_CTX_RULE7EN_SHIFT			5
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED1_MASK		0x1	/* HSI_COMMENT: rule8en */
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED1_SHIFT		6
#define XSTORM_TOE_CONN_AG_CTX_RULE9EN_MASK			0x1	/* HSI_COMMENT: rule9en */
#define XSTORM_TOE_CONN_AG_CTX_RULE9EN_SHIFT			7
	u8 flags12;
#define XSTORM_TOE_CONN_AG_CTX_RULE10EN_MASK			0x1	/* HSI_COMMENT: rule10en */
#define XSTORM_TOE_CONN_AG_CTX_RULE10EN_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_RULE11EN_MASK			0x1	/* HSI_COMMENT: rule11en */
#define XSTORM_TOE_CONN_AG_CTX_RULE11EN_SHIFT			1
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED2_MASK		0x1	/* HSI_COMMENT: rule12en */
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED2_SHIFT		2
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED3_MASK		0x1	/* HSI_COMMENT: rule13en */
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED3_SHIFT		3
#define XSTORM_TOE_CONN_AG_CTX_RULE14EN_MASK			0x1	/* HSI_COMMENT: rule14en */
#define XSTORM_TOE_CONN_AG_CTX_RULE14EN_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_RULE15EN_MASK			0x1	/* HSI_COMMENT: rule15en */
#define XSTORM_TOE_CONN_AG_CTX_RULE15EN_SHIFT			5
#define XSTORM_TOE_CONN_AG_CTX_RULE16EN_MASK			0x1	/* HSI_COMMENT: rule16en */
#define XSTORM_TOE_CONN_AG_CTX_RULE16EN_SHIFT			6
#define XSTORM_TOE_CONN_AG_CTX_RULE17EN_MASK			0x1	/* HSI_COMMENT: rule17en */
#define XSTORM_TOE_CONN_AG_CTX_RULE17EN_SHIFT			7
	u8 flags13;
#define XSTORM_TOE_CONN_AG_CTX_RULE18EN_MASK			0x1	/* HSI_COMMENT: rule18en */
#define XSTORM_TOE_CONN_AG_CTX_RULE18EN_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_RULE19EN_MASK			0x1	/* HSI_COMMENT: rule19en */
#define XSTORM_TOE_CONN_AG_CTX_RULE19EN_SHIFT			1
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED4_MASK		0x1	/* HSI_COMMENT: rule20en */
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED4_SHIFT		2
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED5_MASK		0x1	/* HSI_COMMENT: rule21en */
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED5_SHIFT		3
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED6_MASK		0x1	/* HSI_COMMENT: rule22en */
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED6_SHIFT		4
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED7_MASK		0x1	/* HSI_COMMENT: rule23en */
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED7_SHIFT		5
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED8_MASK		0x1	/* HSI_COMMENT: rule24en */
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED8_SHIFT		6
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED9_MASK		0x1	/* HSI_COMMENT: rule25en */
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED9_SHIFT		7
	u8 flags14;
#define XSTORM_TOE_CONN_AG_CTX_BIT16_MASK			0x1	/* HSI_COMMENT: bit16 */
#define XSTORM_TOE_CONN_AG_CTX_BIT16_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_BIT17_MASK			0x1	/* HSI_COMMENT: bit17 */
#define XSTORM_TOE_CONN_AG_CTX_BIT17_SHIFT			1
#define XSTORM_TOE_CONN_AG_CTX_BIT18_MASK			0x1	/* HSI_COMMENT: bit18 */
#define XSTORM_TOE_CONN_AG_CTX_BIT18_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_BIT19_MASK			0x1	/* HSI_COMMENT: bit19 */
#define XSTORM_TOE_CONN_AG_CTX_BIT19_SHIFT			3
#define XSTORM_TOE_CONN_AG_CTX_BIT20_MASK			0x1	/* HSI_COMMENT: bit20 */
#define XSTORM_TOE_CONN_AG_CTX_BIT20_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_BIT21_MASK			0x1	/* HSI_COMMENT: bit21 */
#define XSTORM_TOE_CONN_AG_CTX_BIT21_SHIFT			5
#define XSTORM_TOE_CONN_AG_CTX_CF23_MASK			0x3	/* HSI_COMMENT: cf23 */
#define XSTORM_TOE_CONN_AG_CTX_CF23_SHIFT			6
	u8 byte2;		/* HSI_COMMENT: byte2 */
	__le16 physical_q0;	/* HSI_COMMENT: physical_q0 */
	__le16 physical_q1;	/* HSI_COMMENT: physical_q1 */
	__le16 word2;		/* HSI_COMMENT: physical_q2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le16 bd_prod;		/* HSI_COMMENT: word4 */
	__le16 word5;		/* HSI_COMMENT: word5 */
	__le16 word6;		/* HSI_COMMENT: conn_dpi */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	u8 byte5;		/* HSI_COMMENT: byte5 */
	u8 byte6;		/* HSI_COMMENT: byte6 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 more_to_send_seq;	/* HSI_COMMENT: reg3 */
	__le32 local_adv_wnd_seq;	/* HSI_COMMENT: reg4 */
	__le32 reg5;		/* HSI_COMMENT: cf_array0 */
	__le32 reg6;		/* HSI_COMMENT: cf_array1 */
	__le16 word7;		/* HSI_COMMENT: word7 */
	__le16 word8;		/* HSI_COMMENT: word8 */
	__le16 word9;		/* HSI_COMMENT: word9 */
	__le16 word10;		/* HSI_COMMENT: word10 */
	__le32 reg7;		/* HSI_COMMENT: reg7 */
	__le32 reg8;		/* HSI_COMMENT: reg8 */
	__le32 reg9;		/* HSI_COMMENT: reg9 */
	u8 byte7;		/* HSI_COMMENT: byte7 */
	u8 byte8;		/* HSI_COMMENT: byte8 */
	u8 byte9;		/* HSI_COMMENT: byte9 */
	u8 byte10;		/* HSI_COMMENT: byte10 */
	u8 byte11;		/* HSI_COMMENT: byte11 */
	u8 byte12;		/* HSI_COMMENT: byte12 */
	u8 byte13;		/* HSI_COMMENT: byte13 */
	u8 byte14;		/* HSI_COMMENT: byte14 */
	u8 byte15;		/* HSI_COMMENT: byte15 */
	u8 e5_reserved;		/* HSI_COMMENT: e5_reserved */
	__le16 word11;		/* HSI_COMMENT: word11 */
	__le32 reg10;		/* HSI_COMMENT: reg10 */
	__le32 reg11;		/* HSI_COMMENT: reg11 */
	__le32 reg12;		/* HSI_COMMENT: reg12 */
	__le32 reg13;		/* HSI_COMMENT: reg13 */
	__le32 reg14;		/* HSI_COMMENT: reg14 */
	__le32 reg15;		/* HSI_COMMENT: reg15 */
	__le32 reg16;		/* HSI_COMMENT: reg16 */
	__le32 reg17;		/* HSI_COMMENT: reg17 */
};

struct tstorm_toe_conn_ag_ctx {
	u8 reserved0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define TSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define TSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define TSTORM_TOE_CONN_AG_CTX_BIT1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define TSTORM_TOE_CONN_AG_CTX_BIT1_SHIFT			1
#define TSTORM_TOE_CONN_AG_CTX_BIT2_MASK			0x1	/* HSI_COMMENT: bit2 */
#define TSTORM_TOE_CONN_AG_CTX_BIT2_SHIFT			2
#define TSTORM_TOE_CONN_AG_CTX_BIT3_MASK			0x1	/* HSI_COMMENT: bit3 */
#define TSTORM_TOE_CONN_AG_CTX_BIT3_SHIFT			3
#define TSTORM_TOE_CONN_AG_CTX_BIT4_MASK			0x1	/* HSI_COMMENT: bit4 */
#define TSTORM_TOE_CONN_AG_CTX_BIT4_SHIFT			4
#define TSTORM_TOE_CONN_AG_CTX_BIT5_MASK			0x1	/* HSI_COMMENT: bit5 */
#define TSTORM_TOE_CONN_AG_CTX_BIT5_SHIFT			5
#define TSTORM_TOE_CONN_AG_CTX_TIMEOUT_CF_MASK			0x3	/* HSI_COMMENT: timer0cf */
#define TSTORM_TOE_CONN_AG_CTX_TIMEOUT_CF_SHIFT			6
	u8 flags1;
#define TSTORM_TOE_CONN_AG_CTX_CF1_MASK				0x3	/* HSI_COMMENT: timer1cf */
#define TSTORM_TOE_CONN_AG_CTX_CF1_SHIFT			0
#define TSTORM_TOE_CONN_AG_CTX_CF2_MASK				0x3	/* HSI_COMMENT: timer2cf */
#define TSTORM_TOE_CONN_AG_CTX_CF2_SHIFT			2
#define TSTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_MASK		0x3	/* HSI_COMMENT: timer_stop_all */
#define TSTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_SHIFT		4
#define TSTORM_TOE_CONN_AG_CTX_CF4_MASK				0x3	/* HSI_COMMENT: cf4 */
#define TSTORM_TOE_CONN_AG_CTX_CF4_SHIFT			6
	u8 flags2;
#define TSTORM_TOE_CONN_AG_CTX_CF5_MASK				0x3	/* HSI_COMMENT: cf5 */
#define TSTORM_TOE_CONN_AG_CTX_CF5_SHIFT			0
#define TSTORM_TOE_CONN_AG_CTX_CF6_MASK				0x3	/* HSI_COMMENT: cf6 */
#define TSTORM_TOE_CONN_AG_CTX_CF6_SHIFT			2
#define TSTORM_TOE_CONN_AG_CTX_CF7_MASK				0x3	/* HSI_COMMENT: cf7 */
#define TSTORM_TOE_CONN_AG_CTX_CF7_SHIFT			4
#define TSTORM_TOE_CONN_AG_CTX_CF8_MASK				0x3	/* HSI_COMMENT: cf8 */
#define TSTORM_TOE_CONN_AG_CTX_CF8_SHIFT			6
	u8 flags3;
#define TSTORM_TOE_CONN_AG_CTX_FLUSH_Q0_MASK			0x3	/* HSI_COMMENT: cf9 */
#define TSTORM_TOE_CONN_AG_CTX_FLUSH_Q0_SHIFT			0
#define TSTORM_TOE_CONN_AG_CTX_CF10_MASK			0x3	/* HSI_COMMENT: cf10 */
#define TSTORM_TOE_CONN_AG_CTX_CF10_SHIFT			2
#define TSTORM_TOE_CONN_AG_CTX_TIMEOUT_CF_EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define TSTORM_TOE_CONN_AG_CTX_TIMEOUT_CF_EN_SHIFT		4
#define TSTORM_TOE_CONN_AG_CTX_CF1EN_MASK			0x1	/* HSI_COMMENT: cf1en */
#define TSTORM_TOE_CONN_AG_CTX_CF1EN_SHIFT			5
#define TSTORM_TOE_CONN_AG_CTX_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define TSTORM_TOE_CONN_AG_CTX_CF2EN_SHIFT			6
#define TSTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_EN_MASK		0x1	/* HSI_COMMENT: cf3en */
#define TSTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_EN_SHIFT		7
	u8 flags4;
#define TSTORM_TOE_CONN_AG_CTX_CF4EN_MASK			0x1	/* HSI_COMMENT: cf4en */
#define TSTORM_TOE_CONN_AG_CTX_CF4EN_SHIFT			0
#define TSTORM_TOE_CONN_AG_CTX_CF5EN_MASK			0x1	/* HSI_COMMENT: cf5en */
#define TSTORM_TOE_CONN_AG_CTX_CF5EN_SHIFT			1
#define TSTORM_TOE_CONN_AG_CTX_CF6EN_MASK			0x1	/* HSI_COMMENT: cf6en */
#define TSTORM_TOE_CONN_AG_CTX_CF6EN_SHIFT			2
#define TSTORM_TOE_CONN_AG_CTX_CF7EN_MASK			0x1	/* HSI_COMMENT: cf7en */
#define TSTORM_TOE_CONN_AG_CTX_CF7EN_SHIFT			3
#define TSTORM_TOE_CONN_AG_CTX_CF8EN_MASK			0x1	/* HSI_COMMENT: cf8en */
#define TSTORM_TOE_CONN_AG_CTX_CF8EN_SHIFT			4
#define TSTORM_TOE_CONN_AG_CTX_FLUSH_Q0_EN_MASK			0x1	/* HSI_COMMENT: cf9en */
#define TSTORM_TOE_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT		5
#define TSTORM_TOE_CONN_AG_CTX_CF10EN_MASK			0x1	/* HSI_COMMENT: cf10en */
#define TSTORM_TOE_CONN_AG_CTX_CF10EN_SHIFT			6
#define TSTORM_TOE_CONN_AG_CTX_RULE0EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define TSTORM_TOE_CONN_AG_CTX_RULE0EN_SHIFT			7
	u8 flags5;
#define TSTORM_TOE_CONN_AG_CTX_RULE1EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define TSTORM_TOE_CONN_AG_CTX_RULE1EN_SHIFT			0
#define TSTORM_TOE_CONN_AG_CTX_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define TSTORM_TOE_CONN_AG_CTX_RULE2EN_SHIFT			1
#define TSTORM_TOE_CONN_AG_CTX_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define TSTORM_TOE_CONN_AG_CTX_RULE3EN_SHIFT			2
#define TSTORM_TOE_CONN_AG_CTX_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define TSTORM_TOE_CONN_AG_CTX_RULE4EN_SHIFT			3
#define TSTORM_TOE_CONN_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define TSTORM_TOE_CONN_AG_CTX_RULE5EN_SHIFT			4
#define TSTORM_TOE_CONN_AG_CTX_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define TSTORM_TOE_CONN_AG_CTX_RULE6EN_SHIFT			5
#define TSTORM_TOE_CONN_AG_CTX_RULE7EN_MASK			0x1	/* HSI_COMMENT: rule7en */
#define TSTORM_TOE_CONN_AG_CTX_RULE7EN_SHIFT			6
#define TSTORM_TOE_CONN_AG_CTX_RULE8EN_MASK			0x1	/* HSI_COMMENT: rule8en */
#define TSTORM_TOE_CONN_AG_CTX_RULE8EN_SHIFT			7
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
	__le32 reg5;		/* HSI_COMMENT: reg5 */
	__le32 reg6;		/* HSI_COMMENT: reg6 */
	__le32 reg7;		/* HSI_COMMENT: reg7 */
	__le32 reg8;		/* HSI_COMMENT: reg8 */
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: word0 */
};

struct ustorm_toe_conn_ag_ctx {
	u8 reserved;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define USTORM_TOE_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define USTORM_TOE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define USTORM_TOE_CONN_AG_CTX_BIT1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define USTORM_TOE_CONN_AG_CTX_BIT1_SHIFT			1
#define USTORM_TOE_CONN_AG_CTX_CF0_MASK				0x3	/* HSI_COMMENT: timer0cf */
#define USTORM_TOE_CONN_AG_CTX_CF0_SHIFT			2
#define USTORM_TOE_CONN_AG_CTX_CF1_MASK				0x3	/* HSI_COMMENT: timer1cf */
#define USTORM_TOE_CONN_AG_CTX_CF1_SHIFT			4
#define USTORM_TOE_CONN_AG_CTX_PUSH_TIMER_CF_MASK		0x3	/* HSI_COMMENT: timer2cf */
#define USTORM_TOE_CONN_AG_CTX_PUSH_TIMER_CF_SHIFT		6
	u8 flags1;
#define USTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_MASK		0x3	/* HSI_COMMENT: timer_stop_all */
#define USTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_SHIFT		0
#define USTORM_TOE_CONN_AG_CTX_SLOW_PATH_CF_MASK		0x3	/* HSI_COMMENT: cf4 */
#define USTORM_TOE_CONN_AG_CTX_SLOW_PATH_CF_SHIFT		2
#define USTORM_TOE_CONN_AG_CTX_DQ_CF_MASK			0x3	/* HSI_COMMENT: cf5 */
#define USTORM_TOE_CONN_AG_CTX_DQ_CF_SHIFT			4
#define USTORM_TOE_CONN_AG_CTX_CF6_MASK				0x3	/* HSI_COMMENT: cf6 */
#define USTORM_TOE_CONN_AG_CTX_CF6_SHIFT			6
	u8 flags2;
#define USTORM_TOE_CONN_AG_CTX_CF0EN_MASK			0x1	/* HSI_COMMENT: cf0en */
#define USTORM_TOE_CONN_AG_CTX_CF0EN_SHIFT			0
#define USTORM_TOE_CONN_AG_CTX_CF1EN_MASK			0x1	/* HSI_COMMENT: cf1en */
#define USTORM_TOE_CONN_AG_CTX_CF1EN_SHIFT			1
#define USTORM_TOE_CONN_AG_CTX_PUSH_TIMER_CF_EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define USTORM_TOE_CONN_AG_CTX_PUSH_TIMER_CF_EN_SHIFT		2
#define USTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_EN_MASK		0x1	/* HSI_COMMENT: cf3en */
#define USTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_EN_SHIFT		3
#define USTORM_TOE_CONN_AG_CTX_SLOW_PATH_CF_EN_MASK		0x1	/* HSI_COMMENT: cf4en */
#define USTORM_TOE_CONN_AG_CTX_SLOW_PATH_CF_EN_SHIFT		4
#define USTORM_TOE_CONN_AG_CTX_DQ_CF_EN_MASK			0x1	/* HSI_COMMENT: cf5en */
#define USTORM_TOE_CONN_AG_CTX_DQ_CF_EN_SHIFT			5
#define USTORM_TOE_CONN_AG_CTX_CF6EN_MASK			0x1	/* HSI_COMMENT: cf6en */
#define USTORM_TOE_CONN_AG_CTX_CF6EN_SHIFT			6
#define USTORM_TOE_CONN_AG_CTX_RULE0EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define USTORM_TOE_CONN_AG_CTX_RULE0EN_SHIFT			7
	u8 flags3;
#define USTORM_TOE_CONN_AG_CTX_RULE1EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define USTORM_TOE_CONN_AG_CTX_RULE1EN_SHIFT			0
#define USTORM_TOE_CONN_AG_CTX_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define USTORM_TOE_CONN_AG_CTX_RULE2EN_SHIFT			1
#define USTORM_TOE_CONN_AG_CTX_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define USTORM_TOE_CONN_AG_CTX_RULE3EN_SHIFT			2
#define USTORM_TOE_CONN_AG_CTX_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define USTORM_TOE_CONN_AG_CTX_RULE4EN_SHIFT			3
#define USTORM_TOE_CONN_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define USTORM_TOE_CONN_AG_CTX_RULE5EN_SHIFT			4
#define USTORM_TOE_CONN_AG_CTX_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define USTORM_TOE_CONN_AG_CTX_RULE6EN_SHIFT			5
#define USTORM_TOE_CONN_AG_CTX_RULE7EN_MASK			0x1	/* HSI_COMMENT: rule7en */
#define USTORM_TOE_CONN_AG_CTX_RULE7EN_SHIFT			6
#define USTORM_TOE_CONN_AG_CTX_RULE8EN_MASK			0x1	/* HSI_COMMENT: rule8en */
#define USTORM_TOE_CONN_AG_CTX_RULE8EN_SHIFT			7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: conn_dpi */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
};

/* The toe storm context of Tstorm */
struct tstorm_toe_conn_st_ctx {
	__le32 reserved[16];
};

/* The toe storm context of Ustorm */
struct ustorm_toe_conn_st_ctx {
	__le32 reserved[52];
};

/* toe connection context */
struct toe_conn_context {
	struct ystorm_toe_conn_st_ctx ystorm_st_context;	/* HSI_COMMENT: ystorm storm context */
	struct pstorm_toe_conn_st_ctx pstorm_st_context;	/* HSI_COMMENT: pstorm storm context */
	struct regpair pstorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct xstorm_toe_conn_st_ctx xstorm_st_context;	/* HSI_COMMENT: xstorm storm context */
	struct regpair xstorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct ystorm_toe_conn_ag_ctx ystorm_ag_context;	/* HSI_COMMENT: ystorm aggregative context */
	struct xstorm_toe_conn_ag_ctx xstorm_ag_context;	/* HSI_COMMENT: xstorm aggregative context */
	struct tstorm_toe_conn_ag_ctx tstorm_ag_context;	/* HSI_COMMENT: tstorm aggregative context */
	struct regpair tstorm_ag_padding[2];	/* HSI_COMMENT: padding */
	struct timers_context timer_context;	/* HSI_COMMENT: timer context */
	struct ustorm_toe_conn_ag_ctx ustorm_ag_context;	/* HSI_COMMENT: ustorm aggregative context */
	struct tstorm_toe_conn_st_ctx tstorm_st_context;	/* HSI_COMMENT: tstorm storm context */
	struct mstorm_toe_conn_st_ctx mstorm_st_context;	/* HSI_COMMENT: mstorm storm context */
	struct ustorm_toe_conn_st_ctx ustorm_st_context;	/* HSI_COMMENT: ustorm storm context */
};

/* toe init ramrod header */
struct toe_init_ramrod_header {
	u8 first_rss;		/* HSI_COMMENT: First rss in PF */
	u8 num_rss;		/* HSI_COMMENT: Num of rss ids in PF */
	u8 reserved[6];
};

/* toe pf init parameters */
struct toe_pf_init_params {
	__le32 push_timeout;	/* HSI_COMMENT: push timer timeout in miliseconds */
	__le16 grq_buffer_size;	/* HSI_COMMENT: GRQ buffer size in bytes */
	__le16 grq_sb_id;	/* HSI_COMMENT: GRQ status block id */
	u8 grq_sb_index;	/* HSI_COMMENT: GRQ status block index */
	u8 max_seg_retransmit;	/* HSI_COMMENT: Maximum number of retransmits for one segment */
	u8 doubt_reachability;	/* HSI_COMMENT: Doubt reachability threshold */
	u8 ll2_rx_queue_id;	/* HSI_COMMENT: Queue ID of the Light-L2 Rx Queue */
	__le16 grq_fetch_threshold;	/* HSI_COMMENT: when passing this threshold, firmware will sync the driver with grq consumer */
	u8 reserved1[2];
	struct regpair grq_page_addr;	/* HSI_COMMENT: Address of the first page in the grq ring */
};

/* toe tss parameters */
struct toe_tss_params {
	struct regpair curr_page_addr;	/* HSI_COMMENT: Address of the current page in the tx cq ring */
	struct regpair next_page_addr;	/* HSI_COMMENT: Address of the next page in the tx cq ring */
	u8 reserved0;		/* HSI_COMMENT: Status block id */
	u8 status_block_index;	/* HSI_COMMENT: Status block index */
	__le16 status_block_id;	/* HSI_COMMENT: Status block id */
	__le16 reserved1[2];
};

/* toe rss parameters */
struct toe_rss_params {
	struct regpair curr_page_addr;	/* HSI_COMMENT: Address of the current page in the rx cq ring */
	struct regpair next_page_addr;	/* HSI_COMMENT: Address of the next page in the rx cq ring */
	u8 reserved0;		/* HSI_COMMENT: Status block id */
	u8 status_block_index;	/* HSI_COMMENT: Status block index */
	__le16 status_block_id;	/* HSI_COMMENT: Status block id */
	__le16 reserved1[2];
};

/* toe init ramrod data */
struct toe_init_ramrod_data {
	struct toe_init_ramrod_header hdr;
	struct tcp_init_params tcp_params;
	struct toe_pf_init_params pf_params;
	struct toe_tss_params tss_params[TOE_TX_MAX_TSS_CHAINS];
	struct toe_rss_params rss_params[TOE_RX_MAX_RSS_CHAINS];
};

/* toe offload parameters */
struct toe_offload_params {
	struct regpair tx_bd_page_addr;	/* HSI_COMMENT: Tx Bd page address */
	struct regpair tx_app_page_addr;	/* HSI_COMMENT: Tx App page address */
	__le32 more_to_send_seq;	/* HSI_COMMENT: Last byte in bd prod (not including fin) */
	__le16 rcv_indication_size;	/* HSI_COMMENT: Recieve indication threshold */
	u8 rss_tss_id;		/* HSI_COMMENT: RSS/TSS absolute id */
	u8 ignore_grq_push;
	struct regpair rx_db_data_ptr;
};

/* TOE offload ramrod data - DMAed by firmware */
struct toe_offload_ramrod_data {
	struct tcp_offload_params tcp_ofld_params;
	struct toe_offload_params toe_ofld_params;
};

/* TOE ramrod command IDs */
enum toe_ramrod_cmd_id {
	TOE_RAMROD_UNUSED,
	TOE_RAMROD_FUNC_INIT,
	TOE_RAMROD_INITATE_OFFLOAD,
	TOE_RAMROD_FUNC_CLOSE,
	TOE_RAMROD_SEARCHER_DELETE,
	TOE_RAMROD_TERMINATE,
	TOE_RAMROD_QUERY,
	TOE_RAMROD_UPDATE,
	TOE_RAMROD_EMPTY,
	TOE_RAMROD_RESET_SEND,
	TOE_RAMROD_INVALIDATE,
	MAX_TOE_RAMROD_CMD_ID
};

/* Toe RQ buffer descriptor */
struct toe_rx_bd {
	struct regpair addr;	/* HSI_COMMENT: Address of buffer */
	__le16 size;		/* HSI_COMMENT: Size of buffer */
	__le16 flags;
#define TOE_RX_BD_START_MASK		0x1	/* HSI_COMMENT: this bd is the beginning of an application buffer */
#define TOE_RX_BD_START_SHIFT		0
#define TOE_RX_BD_END_MASK		0x1	/* HSI_COMMENT: this bd is the end of an application buffer */
#define TOE_RX_BD_END_SHIFT		1
#define TOE_RX_BD_NO_PUSH_MASK		0x1	/* HSI_COMMENT: this application buffer must not be partially completed */
#define TOE_RX_BD_NO_PUSH_SHIFT		2
#define TOE_RX_BD_SPLIT_MASK		0x1
#define TOE_RX_BD_SPLIT_SHIFT		3
#define TOE_RX_BD_RESERVED0_MASK	0xFFF
#define TOE_RX_BD_RESERVED0_SHIFT	4
	__le32 reserved1;
};

/* TOE RX completion queue opcodes (opcode 0 is illegal) */
enum toe_rx_cmp_opcode {
	TOE_RX_CMP_OPCODE_GA = 1,
	TOE_RX_CMP_OPCODE_GR = 2,
	TOE_RX_CMP_OPCODE_GNI = 3,
	TOE_RX_CMP_OPCODE_GAIR = 4,
	TOE_RX_CMP_OPCODE_GAIL = 5,
	TOE_RX_CMP_OPCODE_GRI = 6,
	TOE_RX_CMP_OPCODE_GJ = 7,
	TOE_RX_CMP_OPCODE_DGI = 8,
	TOE_RX_CMP_OPCODE_CMP = 9,
	TOE_RX_CMP_OPCODE_REL = 10,
	TOE_RX_CMP_OPCODE_SKP = 11,
	TOE_RX_CMP_OPCODE_URG = 12,
	TOE_RX_CMP_OPCODE_RT_TO = 13,
	TOE_RX_CMP_OPCODE_KA_TO = 14,
	TOE_RX_CMP_OPCODE_MAX_RT = 15,
	TOE_RX_CMP_OPCODE_DBT_RE = 16,
	TOE_RX_CMP_OPCODE_SYN = 17,
	TOE_RX_CMP_OPCODE_OPT_ERR = 18,
	TOE_RX_CMP_OPCODE_FW2_TO = 19,
	TOE_RX_CMP_OPCODE_2WY_CLS = 20,
	TOE_RX_CMP_OPCODE_RST_RCV = 21,
	TOE_RX_CMP_OPCODE_FIN_RCV = 22,
	TOE_RX_CMP_OPCODE_FIN_UPL = 23,
	TOE_RX_CMP_OPCODE_INIT = 32,
	TOE_RX_CMP_OPCODE_RSS_UPDATE = 33,
	TOE_RX_CMP_OPCODE_CLOSE = 34,
	TOE_RX_CMP_OPCODE_INITIATE_OFFLOAD = 80,
	TOE_RX_CMP_OPCODE_SEARCHER_DELETE = 81,
	TOE_RX_CMP_OPCODE_TERMINATE = 82,
	TOE_RX_CMP_OPCODE_QUERY = 83,
	TOE_RX_CMP_OPCODE_RESET_SEND = 84,
	TOE_RX_CMP_OPCODE_INVALIDATE = 85,
	TOE_RX_CMP_OPCODE_EMPTY = 86,
	TOE_RX_CMP_OPCODE_UPDATE = 87,
	MAX_TOE_RX_CMP_OPCODE
};

/* TOE rx ooo completion data */
struct toe_rx_cqe_ooo_params {
	__le32 nbytes;
	__le16 grq_buff_id;	/* HSI_COMMENT: grq buffer identifier */
	u8 isle_num;
	u8 reserved0;
};

/* TOE rx in order completion data */
struct toe_rx_cqe_in_order_params {
	__le32 nbytes;
	__le16 grq_buff_id;	/* HSI_COMMENT: grq buffer identifier - applicable only for GA,GR opcodes */
	__le16 reserved1;
};

/* Union for TOE rx completion data */
union toe_rx_cqe_data_union {
	struct toe_rx_cqe_ooo_params ooo_params;
	struct toe_rx_cqe_in_order_params in_order_params;
	struct regpair raw_data;
};

/* TOE rx completion element */
struct toe_rx_cqe {
	__le16 icid;
	u8 completion_opcode;
	u8 reserved0;
	__le32 reserved1;
	union toe_rx_cqe_data_union data;
};

/* toe RX doorbel data */
struct toe_rx_db_data {
	__le32 local_adv_wnd_seq;	/* HSI_COMMENT: Sequence of the right edge of the local advertised window (receive window) */
	__le32 reserved[3];
};

/* Toe GRQ buffer descriptor */
struct toe_rx_grq_bd {
	struct regpair addr;	/* HSI_COMMENT: Address of buffer */
	__le16 buff_id;		/* HSI_COMMENT: buffer indentifier */
	__le16 reserved0;
	__le32 reserved1;
};

/* Toe transmission application buffer descriptor */
struct toe_tx_app_buff_desc {
	__le32 next_buffer_start_seq;	/* HSI_COMMENT: Tcp sequence of the first byte in the next application buffer */
	__le32 reserved;
};

/* Toe transmission application buffer descriptor page pointer */
struct toe_tx_app_buff_page_pointer {
	struct regpair next_page_addr;	/* HSI_COMMENT: Address of next page */
};

/* Toe transmission buffer descriptor */
struct toe_tx_bd {
	struct regpair addr;	/* HSI_COMMENT: Address of buffer */
	__le16 size;		/* HSI_COMMENT: Size of buffer */
	__le16 flags;
#define TOE_TX_BD_PUSH_MASK		0x1	/* HSI_COMMENT: Push flag */
#define TOE_TX_BD_PUSH_SHIFT		0
#define TOE_TX_BD_NOTIFY_MASK		0x1	/* HSI_COMMENT: Notify flag */
#define TOE_TX_BD_NOTIFY_SHIFT		1
#define TOE_TX_BD_LARGE_IO_MASK		0x1	/* HSI_COMMENT: Large IO flag */
#define TOE_TX_BD_LARGE_IO_SHIFT	2
#define TOE_TX_BD_BD_CONS_MASK		0x1FFF	/* HSI_COMMENT: 13 LSbits of the consumer of this bd for debugging */
#define TOE_TX_BD_BD_CONS_SHIFT		3
	__le32 next_bd_start_seq;	/* HSI_COMMENT: Tcp sequence of the first byte in the next buffer */
};

/* TOE completion opcodes */
enum toe_tx_cmp_opcode {
	TOE_TX_CMP_OPCODE_DATA,
	TOE_TX_CMP_OPCODE_TERMINATE,
	TOE_TX_CMP_OPCODE_EMPTY,
	TOE_TX_CMP_OPCODE_RESET_SEND,
	TOE_TX_CMP_OPCODE_INVALIDATE,
	TOE_TX_CMP_OPCODE_RST_RCV,
	MAX_TOE_TX_CMP_OPCODE
};

/* Toe transmission completion element */
struct toe_tx_cqe {
	__le16 icid;		/* HSI_COMMENT: Connection ID */
	u8 opcode;		/* HSI_COMMENT: Completion opcode */
	u8 reserved;
	__le32 size;		/* HSI_COMMENT: Size of completed data */
};

/* Toe transmission page pointer bd */
struct toe_tx_page_pointer_bd {
	struct regpair next_page_addr;	/* HSI_COMMENT: Address of next page */
	struct regpair prev_page_addr;	/* HSI_COMMENT: Address of previous page */
};

/* Toe transmission completion element page pointer */
struct toe_tx_page_pointer_cqe {
	struct regpair next_page_addr;	/* HSI_COMMENT: Address of next page */
};

/* toe update parameters */
struct toe_update_params {
	__le16 flags;
#define TOE_UPDATE_PARAMS_RCV_INDICATION_SIZE_CHANGED_MASK	0x1
#define TOE_UPDATE_PARAMS_RCV_INDICATION_SIZE_CHANGED_SHIFT	0
#define TOE_UPDATE_PARAMS_RESERVED_MASK				0x7FFF
#define TOE_UPDATE_PARAMS_RESERVED_SHIFT			1
	__le16 rcv_indication_size;
	__le16 reserved1[2];
};

/* TOE update ramrod data - DMAed by firmware */
struct toe_update_ramrod_data {
	struct tcp_update_params tcp_upd_params;
	struct toe_update_params toe_upd_params;
};

struct mstorm_toe_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define MSTORM_TOE_CONN_AG_CTX_BIT0_MASK	0x1	/* HSI_COMMENT: exist_in_qm0 */
#define MSTORM_TOE_CONN_AG_CTX_BIT0_SHIFT	0
#define MSTORM_TOE_CONN_AG_CTX_BIT1_MASK	0x1	/* HSI_COMMENT: exist_in_qm1 */
#define MSTORM_TOE_CONN_AG_CTX_BIT1_SHIFT	1
#define MSTORM_TOE_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: cf0 */
#define MSTORM_TOE_CONN_AG_CTX_CF0_SHIFT	2
#define MSTORM_TOE_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: cf1 */
#define MSTORM_TOE_CONN_AG_CTX_CF1_SHIFT	4
#define MSTORM_TOE_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: cf2 */
#define MSTORM_TOE_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define MSTORM_TOE_CONN_AG_CTX_CF0EN_MASK	0x1	/* HSI_COMMENT: cf0en */
#define MSTORM_TOE_CONN_AG_CTX_CF0EN_SHIFT	0
#define MSTORM_TOE_CONN_AG_CTX_CF1EN_MASK	0x1	/* HSI_COMMENT: cf1en */
#define MSTORM_TOE_CONN_AG_CTX_CF1EN_SHIFT	1
#define MSTORM_TOE_CONN_AG_CTX_CF2EN_MASK	0x1	/* HSI_COMMENT: cf2en */
#define MSTORM_TOE_CONN_AG_CTX_CF2EN_SHIFT	2
#define MSTORM_TOE_CONN_AG_CTX_RULE0EN_MASK	0x1	/* HSI_COMMENT: rule0en */
#define MSTORM_TOE_CONN_AG_CTX_RULE0EN_SHIFT	3
#define MSTORM_TOE_CONN_AG_CTX_RULE1EN_MASK	0x1	/* HSI_COMMENT: rule1en */
#define MSTORM_TOE_CONN_AG_CTX_RULE1EN_SHIFT	4
#define MSTORM_TOE_CONN_AG_CTX_RULE2EN_MASK	0x1	/* HSI_COMMENT: rule2en */
#define MSTORM_TOE_CONN_AG_CTX_RULE2EN_SHIFT	5
#define MSTORM_TOE_CONN_AG_CTX_RULE3EN_MASK	0x1	/* HSI_COMMENT: rule3en */
#define MSTORM_TOE_CONN_AG_CTX_RULE3EN_SHIFT	6
#define MSTORM_TOE_CONN_AG_CTX_RULE4EN_MASK	0x1	/* HSI_COMMENT: rule4en */
#define MSTORM_TOE_CONN_AG_CTX_RULE4EN_SHIFT	7
	__le16 word0;		/* HSI_COMMENT: word0 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
};

/* TOE doorbell data */
struct toe_db_data {
	u8 params;
#define TOE_DB_DATA_DEST_MASK			0x3	/* HSI_COMMENT: destination of doorbell (use enum db_dest) */
#define TOE_DB_DATA_DEST_SHIFT			0
#define TOE_DB_DATA_AGG_CMD_MASK		0x3	/* HSI_COMMENT: aggregative command to CM (use enum db_agg_cmd_sel) */
#define TOE_DB_DATA_AGG_CMD_SHIFT		2
#define TOE_DB_DATA_BYPASS_EN_MASK		0x1	/* HSI_COMMENT: enable QM bypass */
#define TOE_DB_DATA_BYPASS_EN_SHIFT		4
#define TOE_DB_DATA_RESERVED_MASK		0x1
#define TOE_DB_DATA_RESERVED_SHIFT		5
#define TOE_DB_DATA_AGG_VAL_SEL_MASK		0x3	/* HSI_COMMENT: aggregative value selection */
#define TOE_DB_DATA_AGG_VAL_SEL_SHIFT		6
	u8 agg_flags;		/* HSI_COMMENT: bit for every DQ counter flags in CM context that DQ can increment */
	__le16 bd_prod;
};

/************************************************************************/
/* Add include to common rdma target for both eCore and protocol rdma driver */
/************************************************************************/

/* X Iwarp Assert Codes */
struct xiwarp_assert_codes {
	__le32 flags;
#define XIWARP_ASSERT_CODES_MISC_ERR_MASK	0x1
#define XIWARP_ASSERT_CODES_MISC_ERR_SHIFT	0
#define XIWARP_ASSERT_CODES_UNUSED_MASK		0x7FFFFFFF
#define XIWARP_ASSERT_CODES_UNUSED_SHIFT	1
};

union xstorm_iwarp_asserts {
	struct xiwarp_assert_codes assert_type;
	__le32 all_access_bits;
};

/* Y Iwarp Assert Codes */
struct yiwarp_assert_codes {
	__le32 flags;
#define YIWARP_ASSERT_CODES_INVALID_STATE_ERR_MASK			0x1
#define YIWARP_ASSERT_CODES_INVALID_STATE_ERR_SHIFT			0
#define YIWARP_ASSERT_CODES_INVALID_PD_ERR_MASK				0x1
#define YIWARP_ASSERT_CODES_INVALID_PD_ERR_SHIFT			1
#define YIWARP_ASSERT_CODES_NON_PRIVLG_QP_ERR_MASK			0x1
#define YIWARP_ASSERT_CODES_NON_PRIVLG_QP_ERR_SHIFT			2
#define YIWARP_ASSERT_CODES_INVALID_PERM_ERR_MASK			0x1
#define YIWARP_ASSERT_CODES_INVALID_PERM_ERR_SHIFT			3
#define YIWARP_ASSERT_CODES_LOCAL_INV_NOT_PHYS_ERR_MASK			0x1
#define YIWARP_ASSERT_CODES_LOCAL_INV_NOT_PHYS_ERR_SHIFT		4
#define YIWARP_ASSERT_CODES_LOCAL_INV_PD_ERR_MASK			0x1
#define YIWARP_ASSERT_CODES_LOCAL_INV_PD_ERR_SHIFT			5
#define YIWARP_ASSERT_CODES_NON_ZERO_MEMORY_WINDOW_COUNT_ERR_MASK	0x1
#define YIWARP_ASSERT_CODES_NON_ZERO_MEMORY_WINDOW_COUNT_ERR_SHIFT	6
#define YIWARP_ASSERT_CODES_INVALID_QP_ID_ERR_MASK			0x1
#define YIWARP_ASSERT_CODES_INVALID_QP_ID_ERR_SHIFT			7
#define YIWARP_ASSERT_CODES_LOCAL_INV_MW_PD_ERR_MASK			0x1
#define YIWARP_ASSERT_CODES_LOCAL_INV_MW_PD_ERR_SHIFT			8
#define YIWARP_ASSERT_CODES_LOCAL_INV_STATE_ERR_MASK			0x1
#define YIWARP_ASSERT_CODES_LOCAL_INV_STATE_ERR_SHIFT			9
#define YIWARP_ASSERT_CODES_MW_PRNT_STATE_ERR_MASK			0x1
#define YIWARP_ASSERT_CODES_MW_PRNT_STATE_ERR_SHIFT			10
#define YIWARP_ASSERT_CODES_MW_INV_PD_ERR_MASK				0x1
#define YIWARP_ASSERT_CODES_MW_INV_PD_ERR_SHIFT				11
#define YIWARP_ASSERT_CODES_MW_PRNT_NON_EN_ERR_MASK			0x1
#define YIWARP_ASSERT_CODES_MW_PRNT_NON_EN_ERR_SHIFT			12
#define YIWARP_ASSERT_CODES_MW_PRNT_INV_PERM_ERR_MASK			0x1
#define YIWARP_ASSERT_CODES_MW_PRNT_INV_PERM_ERR_SHIFT			13
#define YIWARP_ASSERT_CODES_MW_PRNT_VA_MISMATCH_ERR_MASK		0x1
#define YIWARP_ASSERT_CODES_MW_PRNT_VA_MISMATCH_ERR_SHIFT		14
#define YIWARP_ASSERT_CODES_MW_INV_LEN_ERR_MASK				0x1
#define YIWARP_ASSERT_CODES_MW_INV_LEN_ERR_SHIFT			15
#define YIWARP_ASSERT_CODES_MW_INV_START_ERR_MASK			0x1
#define YIWARP_ASSERT_CODES_MW_INV_START_ERR_SHIFT			16
#define YIWARP_ASSERT_CODES_MW_INV_END_ERR_MASK				0x1
#define YIWARP_ASSERT_CODES_MW_INV_END_ERR_SHIFT			17
#define YIWARP_ASSERT_CODES_CHILD_INV_TYPE_ERR_MASK			0x1
#define YIWARP_ASSERT_CODES_CHILD_INV_TYPE_ERR_SHIFT			18
#define YIWARP_ASSERT_CODES_CHILD_INV_STATE_ERR_MASK			0x1
#define YIWARP_ASSERT_CODES_CHILD_INV_STATE_ERR_SHIFT			19
#define YIWARP_ASSERT_CODES_RKEY_INV_PD_ERR_MASK			0x1
#define YIWARP_ASSERT_CODES_RKEY_INV_PD_ERR_SHIFT			20
#define YIWARP_ASSERT_CODES_MISC_ERR_MASK				0x1
#define YIWARP_ASSERT_CODES_MISC_ERR_SHIFT				21
#define YIWARP_ASSERT_CODES_UNUSED_MASK					0x3FF
#define YIWARP_ASSERT_CODES_UNUSED_SHIFT				22
};

union ystorm_iwarp_asserts {
	struct yiwarp_assert_codes assert_type;
	__le32 all_access_bits;
};

/* P Iwarp Assert Codes */
struct piwarp_assert_codes {
	__le32 flags;
#define PIWARP_ASSERT_CODES_UNUSED_MASK		0xFFFFFFFF
#define PIWARP_ASSERT_CODES_UNUSED_SHIFT	0
};

union pstorm_iwarp_asserts {
	struct piwarp_assert_codes assert_type;
	__le32 all_access_bits;
};

/* T Iwarp Assert Codes */
struct tiwarp_assert_codes {
	__le32 flags;
#define TIWARP_ASSERT_CODES_RST_GRADE_ERR_MASK		0x1
#define TIWARP_ASSERT_CODES_RST_GRADE_ERR_SHIFT		0
#define TIWARP_ASSERT_CODES_MISC_ERR_MASK		0x1
#define TIWARP_ASSERT_CODES_MISC_ERR_SHIFT		1
#define TIWARP_ASSERT_CODES_UNUSED_MASK			0x3FFFFFFF
#define TIWARP_ASSERT_CODES_UNUSED_SHIFT		2
};

union tstorm_iwarp_asserts {
	struct tiwarp_assert_codes assert_type;
	__le32 all_access_bits;
};

/* M Iwarp Assert Codes */
struct miwarp_assert_codes {
	__le32 flags;
#define MIWARP_ASSERT_CODES_LTID_VAL_ERR_MASK				0x1
#define MIWARP_ASSERT_CODES_LTID_VAL_ERR_SHIFT				0
#define MIWARP_ASSERT_CODES_REF_CNT_ERR_MASK				0x1
#define MIWARP_ASSERT_CODES_REF_CNT_ERR_SHIFT				1
#define MIWARP_ASSERT_CODES_MEM_VAL_ERR_MASK				0x1
#define MIWARP_ASSERT_CODES_MEM_VAL_ERR_SHIFT				2
#define MIWARP_ASSERT_CODES_PERM_VAL_ERR_MASK				0x1
#define MIWARP_ASSERT_CODES_PERM_VAL_ERR_SHIFT				3
#define MIWARP_ASSERT_CODES_RESV_LKEY_USED_QP_ERR_MASK			0x1
#define MIWARP_ASSERT_CODES_RESV_LKEY_USED_QP_ERR_SHIFT			4
#define MIWARP_ASSERT_CODES_SGE_VAL_ERR_MASK				0x1
#define MIWARP_ASSERT_CODES_SGE_VAL_ERR_SHIFT				5
#define MIWARP_ASSERT_CODES_ATOMIC_REQ_VAL_FAILED_ERR_MASK		0x1
#define MIWARP_ASSERT_CODES_ATOMIC_REQ_VAL_FAILED_ERR_SHIFT		6
#define MIWARP_ASSERT_CODES_ATOMIC_RECV_UNSUPP_MASK_ERR_MASK		0x1
#define MIWARP_ASSERT_CODES_ATOMIC_RECV_UNSUPP_MASK_ERR_SHIFT		7
#define MIWARP_ASSERT_CODES_CALLER_SEND_FOC_ERR_MASK			0x1
#define MIWARP_ASSERT_CODES_CALLER_SEND_FOC_ERR_SHIFT			8
#define MIWARP_ASSERT_CODES_UNUSED1_MASK				0x7F
#define MIWARP_ASSERT_CODES_UNUSED1_SHIFT				9
#define MIWARP_ASSERT_CODES_UNUSED2_MASK				0xFFFF
#define MIWARP_ASSERT_CODES_UNUSED2_SHIFT				16
};

union mstorm_iwarp_asserts {
	struct miwarp_assert_codes assert_type;
	__le32 all_access_bits;
};

/* U Iwarp Assert Codes */
struct uiwarp_assert_codes {
	__le32 flags;
#define UIWARP_ASSERT_CODES_QP_CTX_ERR_MASK	0x1
#define UIWARP_ASSERT_CODES_QP_CTX_ERR_SHIFT	0
#define UIWARP_ASSERT_CODES_UNUSED_MASK		0x7FFFFFFF
#define UIWARP_ASSERT_CODES_UNUSED_SHIFT	1
};

union ustorm_iwarp_asserts {
	struct uiwarp_assert_codes assert_type;
	__le32 all_access_bits;
};

/* iWARP Assert Codes for each Storm */
struct iwarp_asserts_types {
	union xstorm_iwarp_asserts xstorm_asserts;	/* HSI_COMMENT: X Storm Iwarp assert codes */
	union ystorm_iwarp_asserts ystorm_asserts;	/* HSI_COMMENT: Y Storm Iwarp assert codes */
	union pstorm_iwarp_asserts pstorm_asserts;	/* HSI_COMMENT: P Storm Iwarp assert codes */
	union tstorm_iwarp_asserts tstorm_asserts;	/* HSI_COMMENT: T Storm Iwarp assert codes */
	union mstorm_iwarp_asserts mstorm_asserts;	/* HSI_COMMENT: M Storm Iwarp assert codes */
	union ustorm_iwarp_asserts ustorm_asserts;	/* HSI_COMMENT: U Storm Iwarp assert codes */
};

/* The roce task context of Mstorm */
struct mstorm_rdma_task_st_ctx {
	struct regpair temp[4];
};

/* rdma function init ramrod data */
struct rdma_close_func_ramrod_data {
	u8 cnq_start_offset;
	u8 num_cnqs;
	u8 vf_id;		/* HSI_COMMENT: This field should be assigned to Virtual Function ID if vf_valid == 1. Otherwise its dont care */
	u8 vf_valid;
	u8 reserved[4];
};

/* rdma function init Common Queue parameters */
struct rdma_common_queue_params {
	__le16 sb_num;		/* HSI_COMMENT: Status block number used by the queue */
	u8 sb_index;		/* HSI_COMMENT: Status block index used by the queue */
	u8 num_pbl_pages;	/* HSI_COMMENT: Number of pages in the PBL allocated for this queue */
	__le32 reserved;
	struct regpair pbl_base_addr;	/* HSI_COMMENT: Address to the first entry of the queue PBL */
	__le16 queue_zone_num;	/* HSI_COMMENT: Queue Zone ID used for CNQ consumer update */
	u8 reserved1[6];
};

/* rdma create cq ramrod data */
struct rdma_create_cq_ramrod_data {
	struct regpair cq_handle;
	struct regpair pbl_addr;
	__le32 max_cqes;
	__le16 pbl_num_pages;
	__le16 dpi;
	u8 is_two_level_pbl;
	u8 cnq_id;
	u8 pbl_log_page_size;
	u8 toggle_bit;
	__le16 int_timeout;	/* HSI_COMMENT: Timeout used for interrupt moderation */
	u8 vf_id;		/* HSI_COMMENT: Only used in E5 */
	u8 flags;
#define RDMA_CREATE_CQ_RAMROD_DATA_VF_ID_VALID_MASK	0x1	/* HSI_COMMENT: Bit indicating that this VF */
#define RDMA_CREATE_CQ_RAMROD_DATA_VF_ID_VALID_SHIFT	0
#define RDMA_CREATE_CQ_RAMROD_DATA_RESERVED1_MASK	0x7F
#define RDMA_CREATE_CQ_RAMROD_DATA_RESERVED1_SHIFT	1
};

/* rdma deregister tid ramrod data */
struct rdma_deregister_tid_ramrod_data {
	__le32 itid;
	__le32 reserved;
};

/* rdma destroy cq output params */
struct rdma_destroy_cq_output_params {
	__le16 cnq_num;		/* HSI_COMMENT: Sequence number of completion notification sent for the cq on the associated CNQ */
	__le16 reserved0;
	__le32 reserved1;
};

/* rdma destroy cq ramrod data */
struct rdma_destroy_cq_ramrod_data {
	struct regpair output_params_addr;
};

/* RDMA slow path EQ cmd IDs */
enum rdma_event_opcode {
	RDMA_EVENT_UNUSED,
	RDMA_EVENT_FUNC_INIT,
	RDMA_EVENT_FUNC_CLOSE,
	RDMA_EVENT_REGISTER_MR,
	RDMA_EVENT_DEREGISTER_MR,
	RDMA_EVENT_CREATE_CQ,
	RDMA_EVENT_RESIZE_CQ,
	RDMA_EVENT_DESTROY_CQ,
	RDMA_EVENT_CREATE_SRQ,
	RDMA_EVENT_MODIFY_SRQ,
	RDMA_EVENT_DESTROY_SRQ,
	RDMA_EVENT_START_NAMESPACE_TRACKING,
	RDMA_EVENT_STOP_NAMESPACE_TRACKING,
	MAX_RDMA_EVENT_OPCODE
};

/* RDMA FW return code for slow path ramrods */
enum rdma_fw_return_code {
	RDMA_RETURN_OK = 0,
	RDMA_RETURN_REGISTER_MR_BAD_STATE_ERR,
	RDMA_RETURN_DEREGISTER_MR_BAD_STATE_ERR,
	RDMA_RETURN_RESIZE_CQ_ERR,
	RDMA_RETURN_NIG_DRAIN_REQ,
	RDMA_RETURN_GENERAL_ERR,
	MAX_RDMA_FW_RETURN_CODE
};

/* rdma function init header */
struct rdma_init_func_hdr {
	u8 cnq_start_offset;	/* HSI_COMMENT: First RDMA CNQ */
	u8 num_cnqs;		/* HSI_COMMENT: Number of CNQs */
	u8 cq_ring_mode;	/* HSI_COMMENT: 0 for 32 bit cq producer and consumer counters and 1 for 16 bit */
	u8 vf_id;		/* HSI_COMMENT: This field should be assigned to Virtual Function ID if vf_valid == 1. Otherwise its dont care */
	u8 vf_valid;
	u8 relaxed_ordering;	/* HSI_COMMENT: 1 for using relaxed ordering PCI writes */
	__le16 first_reg_srq_id;	/* HSI_COMMENT: The SRQ ID of the first regular (non XRC) SRQ */
	__le32 reg_srq_base_addr;	/* HSI_COMMENT: Logical base address of first regular (non XRC) SRQ */
	u8 flags;
#define RDMA_INIT_FUNC_HDR_SEARCHER_MODE_MASK		0x1	/* HSI_COMMENT: RoCE PF Init only: 1 for using the Searcher, 0 otherwise. */
#define RDMA_INIT_FUNC_HDR_SEARCHER_MODE_SHIFT		0
#define RDMA_INIT_FUNC_HDR_PVRDMA_MODE_MASK		0x1	/* HSI_COMMENT: RoCE PF Init only: 1 for enabling PVRDMA, 0 otherwise. */
#define RDMA_INIT_FUNC_HDR_PVRDMA_MODE_SHIFT		1
#define RDMA_INIT_FUNC_HDR_DPT_MODE_MASK		0x1	/* HSI_COMMENT: RoCE PF Init only: 1 for enabling PVRDMA Dirty Pages Tracking (DPT), 0 otherwise. */
#define RDMA_INIT_FUNC_HDR_DPT_MODE_SHIFT		2
#define RDMA_INIT_FUNC_HDR_RESERVED0_MASK		0x1F
#define RDMA_INIT_FUNC_HDR_RESERVED0_SHIFT		3
	u8 dpt_byte_threshold_log;	/* HSI_COMMENT: ROCE PVRDMA DPT only: Log2 of the number of written bytes before end of IO for which DPTQE should be written (range: 12-16). */
	u8 dpt_common_queue_id;	/* HSI_COMMENT: ROCE PVRDMA DPT only: Index of DPTQ in common queues zone into which DPTQ elements should be written when needed */
	u8 max_num_ns_log;	/* HSI_COMMENT: ROCE PVRDMA mode only: log2 of maximum number of namespaces */
};

/* rdma function init ramrod data */
struct rdma_init_func_ramrod_data {
	struct rdma_init_func_hdr params_header;
	struct rdma_common_queue_params dptq_params;	/* HSI_COMMENT: ROCE PVRDMA DPT only: Parameters for initializing DPTQ common queue data in RAM */
	struct iwarp_asserts_types iwarp_asserts;
	struct rdma_common_queue_params cnq_params[NUM_OF_GLOBAL_QUEUES];
};

/* rdma namespace tracking ramrod data */
struct rdma_namespace_tracking_ramrod_data {
	u8 name_space;		/* HSI_COMMENT: The namespace ID on which dirty pages tracking should be started/stopped */
	u8 reserved[7];
};

/* RDMA ramrod command IDs */
enum rdma_ramrod_cmd_id {
	RDMA_RAMROD_UNUSED,
	RDMA_RAMROD_FUNC_INIT,
	RDMA_RAMROD_FUNC_CLOSE,
	RDMA_RAMROD_REGISTER_MR,
	RDMA_RAMROD_DEREGISTER_MR,
	RDMA_RAMROD_CREATE_CQ,
	RDMA_RAMROD_RESIZE_CQ,
	RDMA_RAMROD_DESTROY_CQ,
	RDMA_RAMROD_CREATE_SRQ,
	RDMA_RAMROD_MODIFY_SRQ,
	RDMA_RAMROD_DESTROY_SRQ,
	RDMA_RAMROD_START_NS_TRACKING,
	RDMA_RAMROD_STOP_NS_TRACKING,
	MAX_RDMA_RAMROD_CMD_ID
};

/* rdma register tid ramrod data */
struct rdma_register_tid_ramrod_data {
	__le16 flags;
#define RDMA_REGISTER_TID_RAMROD_DATA_PAGE_SIZE_LOG_MASK		0x1F
#define RDMA_REGISTER_TID_RAMROD_DATA_PAGE_SIZE_LOG_SHIFT		0
#define RDMA_REGISTER_TID_RAMROD_DATA_TWO_LEVEL_PBL_MASK		0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_TWO_LEVEL_PBL_SHIFT		5
#define RDMA_REGISTER_TID_RAMROD_DATA_ZERO_BASED_MASK			0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_ZERO_BASED_SHIFT			6
#define RDMA_REGISTER_TID_RAMROD_DATA_PHY_MR_MASK			0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_PHY_MR_SHIFT			7
#define RDMA_REGISTER_TID_RAMROD_DATA_REMOTE_READ_MASK			0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_REMOTE_READ_SHIFT			8
#define RDMA_REGISTER_TID_RAMROD_DATA_REMOTE_WRITE_MASK			0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_REMOTE_WRITE_SHIFT		9
#define RDMA_REGISTER_TID_RAMROD_DATA_REMOTE_ATOMIC_MASK		0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_REMOTE_ATOMIC_SHIFT		10
#define RDMA_REGISTER_TID_RAMROD_DATA_LOCAL_WRITE_MASK			0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_LOCAL_WRITE_SHIFT			11
#define RDMA_REGISTER_TID_RAMROD_DATA_LOCAL_READ_MASK			0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_LOCAL_READ_SHIFT			12
#define RDMA_REGISTER_TID_RAMROD_DATA_ENABLE_MW_BIND_MASK		0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_ENABLE_MW_BIND_SHIFT		13
#define RDMA_REGISTER_TID_RAMROD_DATA_RESERVED_MASK			0x3
#define RDMA_REGISTER_TID_RAMROD_DATA_RESERVED_SHIFT			14
	u8 flags1;
#define RDMA_REGISTER_TID_RAMROD_DATA_PBL_PAGE_SIZE_LOG_MASK		0x1F
#define RDMA_REGISTER_TID_RAMROD_DATA_PBL_PAGE_SIZE_LOG_SHIFT		0
#define RDMA_REGISTER_TID_RAMROD_DATA_TID_TYPE_MASK			0x7
#define RDMA_REGISTER_TID_RAMROD_DATA_TID_TYPE_SHIFT			5
	u8 flags2;
#define RDMA_REGISTER_TID_RAMROD_DATA_DMA_MR_MASK			0x1	/* HSI_COMMENT: Bit indicating that this MR is DMA_MR meaning SGEs that use it have the physical address on them */
#define RDMA_REGISTER_TID_RAMROD_DATA_DMA_MR_SHIFT			0
#define RDMA_REGISTER_TID_RAMROD_DATA_DIF_ON_HOST_FLG_MASK		0x1	/* HSI_COMMENT: Bit indicating that this MR has DIF protection enabled. */
#define RDMA_REGISTER_TID_RAMROD_DATA_DIF_ON_HOST_FLG_SHIFT		1
#define RDMA_REGISTER_TID_RAMROD_DATA_RESERVED1_MASK			0x3F
#define RDMA_REGISTER_TID_RAMROD_DATA_RESERVED1_SHIFT			2
	u8 key;
	u8 length_hi;
	u8 vf_id;		/* HSI_COMMENT: This field should be assigned to Virtual Function ID if vf_valid == 1. Otherwise its dont care */
	u8 vf_valid;
	__le16 pd;
	__le16 reserved2;
	__le32 length_lo;	/* HSI_COMMENT: lower 32 bits of the registered MR length. */
	__le32 itid;
	__le32 reserved3;
	struct regpair va;
	struct regpair pbl_base;
	struct regpair dif_error_addr;	/* HSI_COMMENT: DIF TX IO writes error information to this location when memory region is invalidated. */
	__le32 reserved4[4];
};

/* rdma resize cq output params */
struct rdma_resize_cq_output_params {
	__le32 old_cq_cons;	/* HSI_COMMENT: cq consumer value on old PBL */
	__le32 old_cq_prod;	/* HSI_COMMENT: cq producer value on old PBL */
};

/* rdma resize cq ramrod data */
struct rdma_resize_cq_ramrod_data {
	u8 flags;
#define RDMA_RESIZE_CQ_RAMROD_DATA_TOGGLE_BIT_MASK		0x1
#define RDMA_RESIZE_CQ_RAMROD_DATA_TOGGLE_BIT_SHIFT		0
#define RDMA_RESIZE_CQ_RAMROD_DATA_IS_TWO_LEVEL_PBL_MASK	0x1
#define RDMA_RESIZE_CQ_RAMROD_DATA_IS_TWO_LEVEL_PBL_SHIFT	1
#define RDMA_RESIZE_CQ_RAMROD_DATA_VF_ID_VALID_MASK		0x1
#define RDMA_RESIZE_CQ_RAMROD_DATA_VF_ID_VALID_SHIFT		2
#define RDMA_RESIZE_CQ_RAMROD_DATA_RESERVED_MASK		0x1F
#define RDMA_RESIZE_CQ_RAMROD_DATA_RESERVED_SHIFT		3
	u8 pbl_log_page_size;
	__le16 pbl_num_pages;
	__le32 max_cqes;
	struct regpair pbl_addr;
	struct regpair output_params_addr;
	u8 vf_id;
	u8 reserved1[7];
};

/* The rdma SRQ context */
struct rdma_srq_context {
	struct regpair temp[8];
};

/* rdma create qp requester ramrod data */
struct rdma_srq_create_ramrod_data {
	u8 flags;
#define RDMA_SRQ_CREATE_RAMROD_DATA_XRC_FLAG_MASK		0x1
#define RDMA_SRQ_CREATE_RAMROD_DATA_XRC_FLAG_SHIFT		0
#define RDMA_SRQ_CREATE_RAMROD_DATA_RESERVED_KEY_EN_MASK	0x1	/* HSI_COMMENT: Only applicable when xrc_flag is set */
#define RDMA_SRQ_CREATE_RAMROD_DATA_RESERVED_KEY_EN_SHIFT	1
#define RDMA_SRQ_CREATE_RAMROD_DATA_RESERVED1_MASK		0x3F
#define RDMA_SRQ_CREATE_RAMROD_DATA_RESERVED1_SHIFT		2
	u8 reserved2;
	__le16 xrc_domain;	/* HSI_COMMENT: Only applicable when xrc_flag is set */
	__le32 xrc_srq_cq_cid;	/* HSI_COMMENT: Only applicable when xrc_flag is set */
	struct regpair pbl_base_addr;	/* HSI_COMMENT: SRQ PBL base address */
	__le16 pages_in_srq_pbl;	/* HSI_COMMENT: Number of pages in PBL */
	__le16 pd_id;
	struct rdma_srq_id srq_id;	/* HSI_COMMENT: SRQ Index */
	__le16 page_size;	/* HSI_COMMENT: Page size in SGEs(16 bytes) elements. Supports up to 2M bytes page size */
	__le16 reserved3;
	__le32 reserved4;
	struct regpair producers_addr;	/* HSI_COMMENT: SRQ PBL base address */
};

/* rdma create qp requester ramrod data */
struct rdma_srq_destroy_ramrod_data {
	struct rdma_srq_id srq_id;	/* HSI_COMMENT: SRQ Index */
	__le32 reserved;
};

/* rdma create qp requester ramrod data */
struct rdma_srq_modify_ramrod_data {
	struct rdma_srq_id srq_id;	/* HSI_COMMENT: SRQ Index */
	__le32 wqe_limit;
};

/* The rdma task context of Mstorm */
struct ystorm_rdma_task_st_ctx {
	struct regpair temp[4];
};

struct ystorm_rdma_task_ag_ctx {
	u8 reserved;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	__le16 msem_ctx_upd_seq;	/* HSI_COMMENT: icid */
	u8 flags0;
#define YSTORM_RDMA_TASK_AG_CTX_CONNECTION_TYPE_MASK		0xF	/* HSI_COMMENT: connection_type */
#define YSTORM_RDMA_TASK_AG_CTX_CONNECTION_TYPE_SHIFT		0
#define YSTORM_RDMA_TASK_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define YSTORM_RDMA_TASK_AG_CTX_EXIST_IN_QM0_SHIFT		4
#define YSTORM_RDMA_TASK_AG_CTX_BIT1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define YSTORM_RDMA_TASK_AG_CTX_BIT1_SHIFT			5
#define YSTORM_RDMA_TASK_AG_CTX_VALID_MASK			0x1	/* HSI_COMMENT: bit2 */
#define YSTORM_RDMA_TASK_AG_CTX_VALID_SHIFT			6
#define YSTORM_RDMA_TASK_AG_CTX_DIF_FIRST_IO_MASK		0x1	/* HSI_COMMENT: bit3 */
#define YSTORM_RDMA_TASK_AG_CTX_DIF_FIRST_IO_SHIFT		7
	u8 flags1;
#define YSTORM_RDMA_TASK_AG_CTX_CF0_MASK			0x3	/* HSI_COMMENT: cf0 */
#define YSTORM_RDMA_TASK_AG_CTX_CF0_SHIFT			0
#define YSTORM_RDMA_TASK_AG_CTX_CF1_MASK			0x3	/* HSI_COMMENT: cf1 */
#define YSTORM_RDMA_TASK_AG_CTX_CF1_SHIFT			2
#define YSTORM_RDMA_TASK_AG_CTX_CF2SPECIAL_MASK			0x3	/* HSI_COMMENT: cf2special */
#define YSTORM_RDMA_TASK_AG_CTX_CF2SPECIAL_SHIFT		4
#define YSTORM_RDMA_TASK_AG_CTX_CF0EN_MASK			0x1	/* HSI_COMMENT: cf0en */
#define YSTORM_RDMA_TASK_AG_CTX_CF0EN_SHIFT			6
#define YSTORM_RDMA_TASK_AG_CTX_CF1EN_MASK			0x1	/* HSI_COMMENT: cf1en */
#define YSTORM_RDMA_TASK_AG_CTX_CF1EN_SHIFT			7
	u8 flags2;
#define YSTORM_RDMA_TASK_AG_CTX_BIT4_MASK			0x1	/* HSI_COMMENT: bit4 */
#define YSTORM_RDMA_TASK_AG_CTX_BIT4_SHIFT			0
#define YSTORM_RDMA_TASK_AG_CTX_RULE0EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define YSTORM_RDMA_TASK_AG_CTX_RULE0EN_SHIFT			1
#define YSTORM_RDMA_TASK_AG_CTX_RULE1EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define YSTORM_RDMA_TASK_AG_CTX_RULE1EN_SHIFT			2
#define YSTORM_RDMA_TASK_AG_CTX_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define YSTORM_RDMA_TASK_AG_CTX_RULE2EN_SHIFT			3
#define YSTORM_RDMA_TASK_AG_CTX_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define YSTORM_RDMA_TASK_AG_CTX_RULE3EN_SHIFT			4
#define YSTORM_RDMA_TASK_AG_CTX_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define YSTORM_RDMA_TASK_AG_CTX_RULE4EN_SHIFT			5
#define YSTORM_RDMA_TASK_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define YSTORM_RDMA_TASK_AG_CTX_RULE5EN_SHIFT			6
#define YSTORM_RDMA_TASK_AG_CTX_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define YSTORM_RDMA_TASK_AG_CTX_RULE6EN_SHIFT			7
	u8 key;			/* HSI_COMMENT: byte2 */
	__le32 mw_cnt_or_qp_id;	/* HSI_COMMENT: reg0 */
	u8 ref_cnt_seq;		/* HSI_COMMENT: byte3 */
	u8 ctx_upd_seq;		/* HSI_COMMENT: byte4 */
	__le16 dif_flags;	/* HSI_COMMENT: word1 */
	__le16 tx_ref_count;	/* HSI_COMMENT: word2 */
	__le16 last_used_ltid;	/* HSI_COMMENT: word3 */
	__le16 parent_mr_lo;	/* HSI_COMMENT: word4 */
	__le16 parent_mr_hi;	/* HSI_COMMENT: word5 */
	__le32 fbo_lo;		/* HSI_COMMENT: reg1 */
	__le32 fbo_hi;		/* HSI_COMMENT: reg2 */
};

struct mstorm_rdma_task_ag_ctx {
	u8 reserved;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	__le16 icid;		/* HSI_COMMENT: icid */
	u8 flags0;
#define MSTORM_RDMA_TASK_AG_CTX_CONNECTION_TYPE_MASK		0xF	/* HSI_COMMENT: connection_type */
#define MSTORM_RDMA_TASK_AG_CTX_CONNECTION_TYPE_SHIFT		0
#define MSTORM_RDMA_TASK_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define MSTORM_RDMA_TASK_AG_CTX_EXIST_IN_QM0_SHIFT		4
#define MSTORM_RDMA_TASK_AG_CTX_BIT1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define MSTORM_RDMA_TASK_AG_CTX_BIT1_SHIFT			5
#define MSTORM_RDMA_TASK_AG_CTX_BIT2_MASK			0x1	/* HSI_COMMENT: bit2 */
#define MSTORM_RDMA_TASK_AG_CTX_BIT2_SHIFT			6
#define MSTORM_RDMA_TASK_AG_CTX_DIF_FIRST_IO_MASK		0x1	/* HSI_COMMENT: bit3 */
#define MSTORM_RDMA_TASK_AG_CTX_DIF_FIRST_IO_SHIFT		7
	u8 flags1;
#define MSTORM_RDMA_TASK_AG_CTX_CF0_MASK			0x3	/* HSI_COMMENT: cf0 */
#define MSTORM_RDMA_TASK_AG_CTX_CF0_SHIFT			0
#define MSTORM_RDMA_TASK_AG_CTX_CF1_MASK			0x3	/* HSI_COMMENT: cf1 */
#define MSTORM_RDMA_TASK_AG_CTX_CF1_SHIFT			2
#define MSTORM_RDMA_TASK_AG_CTX_CF2_MASK			0x3	/* HSI_COMMENT: cf2 */
#define MSTORM_RDMA_TASK_AG_CTX_CF2_SHIFT			4
#define MSTORM_RDMA_TASK_AG_CTX_CF0EN_MASK			0x1	/* HSI_COMMENT: cf0en */
#define MSTORM_RDMA_TASK_AG_CTX_CF0EN_SHIFT			6
#define MSTORM_RDMA_TASK_AG_CTX_CF1EN_MASK			0x1	/* HSI_COMMENT: cf1en */
#define MSTORM_RDMA_TASK_AG_CTX_CF1EN_SHIFT			7
	u8 flags2;
#define MSTORM_RDMA_TASK_AG_CTX_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define MSTORM_RDMA_TASK_AG_CTX_CF2EN_SHIFT			0
#define MSTORM_RDMA_TASK_AG_CTX_RULE0EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define MSTORM_RDMA_TASK_AG_CTX_RULE0EN_SHIFT			1
#define MSTORM_RDMA_TASK_AG_CTX_RULE1EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define MSTORM_RDMA_TASK_AG_CTX_RULE1EN_SHIFT			2
#define MSTORM_RDMA_TASK_AG_CTX_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define MSTORM_RDMA_TASK_AG_CTX_RULE2EN_SHIFT			3
#define MSTORM_RDMA_TASK_AG_CTX_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define MSTORM_RDMA_TASK_AG_CTX_RULE3EN_SHIFT			4
#define MSTORM_RDMA_TASK_AG_CTX_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define MSTORM_RDMA_TASK_AG_CTX_RULE4EN_SHIFT			5
#define MSTORM_RDMA_TASK_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define MSTORM_RDMA_TASK_AG_CTX_RULE5EN_SHIFT			6
#define MSTORM_RDMA_TASK_AG_CTX_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define MSTORM_RDMA_TASK_AG_CTX_RULE6EN_SHIFT			7
	u8 key;			/* HSI_COMMENT: byte2 */
	__le32 mw_cnt_or_qp_id;	/* HSI_COMMENT: reg0 */
	u8 ref_cnt_seq;		/* HSI_COMMENT: byte3 */
	u8 ctx_upd_seq;		/* HSI_COMMENT: byte4 */
	__le16 dif_flags;	/* HSI_COMMENT: word1 */
	__le16 tx_ref_count;	/* HSI_COMMENT: word2 */
	__le16 last_used_ltid;	/* HSI_COMMENT: word3 */
	__le16 parent_mr_lo;	/* HSI_COMMENT: word4 */
	__le16 parent_mr_hi;	/* HSI_COMMENT: word5 */
	__le32 fbo_lo;		/* HSI_COMMENT: reg1 */
	__le32 fbo_hi;		/* HSI_COMMENT: reg2 */
};

/* The roce task context of Ustorm */
struct ustorm_rdma_task_st_ctx {
	struct regpair temp[6];
};

struct ustorm_rdma_task_ag_ctx {
	u8 reserved;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	__le16 icid;		/* HSI_COMMENT: icid */
	u8 flags0;
#define USTORM_RDMA_TASK_AG_CTX_CONNECTION_TYPE_MASK		0xF	/* HSI_COMMENT: connection_type */
#define USTORM_RDMA_TASK_AG_CTX_CONNECTION_TYPE_SHIFT		0
#define USTORM_RDMA_TASK_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define USTORM_RDMA_TASK_AG_CTX_EXIST_IN_QM0_SHIFT		4
#define USTORM_RDMA_TASK_AG_CTX_BIT1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define USTORM_RDMA_TASK_AG_CTX_BIT1_SHIFT			5
#define USTORM_RDMA_TASK_AG_CTX_DIF_WRITE_RESULT_CF_MASK	0x3	/* HSI_COMMENT: timer0cf */
#define USTORM_RDMA_TASK_AG_CTX_DIF_WRITE_RESULT_CF_SHIFT	6
	u8 flags1;
#define USTORM_RDMA_TASK_AG_CTX_DIF_RESULT_TOGGLE_BIT_MASK	0x3	/* HSI_COMMENT: timer1cf */
#define USTORM_RDMA_TASK_AG_CTX_DIF_RESULT_TOGGLE_BIT_SHIFT	0
#define USTORM_RDMA_TASK_AG_CTX_DIF_TX_IO_FLG_MASK		0x3	/* HSI_COMMENT: timer2cf */
#define USTORM_RDMA_TASK_AG_CTX_DIF_TX_IO_FLG_SHIFT		2
#define USTORM_RDMA_TASK_AG_CTX_DIF_BLOCK_SIZE_MASK		0x3	/* HSI_COMMENT: timer_stop_all */
#define USTORM_RDMA_TASK_AG_CTX_DIF_BLOCK_SIZE_SHIFT		4
#define USTORM_RDMA_TASK_AG_CTX_DIF_ERROR_CF_MASK		0x3	/* HSI_COMMENT: cf4 */
#define USTORM_RDMA_TASK_AG_CTX_DIF_ERROR_CF_SHIFT		6
	u8 flags2;
#define USTORM_RDMA_TASK_AG_CTX_DIF_WRITE_RESULT_CF_EN_MASK	0x1	/* HSI_COMMENT: cf0en */
#define USTORM_RDMA_TASK_AG_CTX_DIF_WRITE_RESULT_CF_EN_SHIFT	0
#define USTORM_RDMA_TASK_AG_CTX_RESERVED2_MASK			0x1	/* HSI_COMMENT: cf1en */
#define USTORM_RDMA_TASK_AG_CTX_RESERVED2_SHIFT			1
#define USTORM_RDMA_TASK_AG_CTX_RESERVED3_MASK			0x1	/* HSI_COMMENT: cf2en */
#define USTORM_RDMA_TASK_AG_CTX_RESERVED3_SHIFT			2
#define USTORM_RDMA_TASK_AG_CTX_RESERVED4_MASK			0x1	/* HSI_COMMENT: cf3en */
#define USTORM_RDMA_TASK_AG_CTX_RESERVED4_SHIFT			3
#define USTORM_RDMA_TASK_AG_CTX_DIF_ERROR_CF_EN_MASK		0x1	/* HSI_COMMENT: cf4en */
#define USTORM_RDMA_TASK_AG_CTX_DIF_ERROR_CF_EN_SHIFT		4
#define USTORM_RDMA_TASK_AG_CTX_RULE0EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define USTORM_RDMA_TASK_AG_CTX_RULE0EN_SHIFT			5
#define USTORM_RDMA_TASK_AG_CTX_RULE1EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define USTORM_RDMA_TASK_AG_CTX_RULE1EN_SHIFT			6
#define USTORM_RDMA_TASK_AG_CTX_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define USTORM_RDMA_TASK_AG_CTX_RULE2EN_SHIFT			7
	u8 flags3;
#define USTORM_RDMA_TASK_AG_CTX_DIF_RXMIT_PROD_CONS_EN_MASK	0x1	/* HSI_COMMENT: rule3en */
#define USTORM_RDMA_TASK_AG_CTX_DIF_RXMIT_PROD_CONS_EN_SHIFT	0
#define USTORM_RDMA_TASK_AG_CTX_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define USTORM_RDMA_TASK_AG_CTX_RULE4EN_SHIFT			1
#define USTORM_RDMA_TASK_AG_CTX_DIF_WRITE_PROD_CONS_EN_MASK	0x1	/* HSI_COMMENT: rule5en */
#define USTORM_RDMA_TASK_AG_CTX_DIF_WRITE_PROD_CONS_EN_SHIFT	2
#define USTORM_RDMA_TASK_AG_CTX_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define USTORM_RDMA_TASK_AG_CTX_RULE6EN_SHIFT			3
#define USTORM_RDMA_TASK_AG_CTX_DIF_ERROR_TYPE_MASK		0xF	/* HSI_COMMENT: nibble1 */
#define USTORM_RDMA_TASK_AG_CTX_DIF_ERROR_TYPE_SHIFT		4
	__le32 dif_err_intervals;	/* HSI_COMMENT: reg0 */
	__le32 dif_error_1st_interval;	/* HSI_COMMENT: reg1 */
	__le32 dif_rxmit_cons;	/* HSI_COMMENT: reg2 */
	__le32 dif_rxmit_prod;	/* HSI_COMMENT: reg3 */
	__le32 sge_index;	/* HSI_COMMENT: reg4 */
	__le32 sq_cons;		/* HSI_COMMENT: reg5 */
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 dif_write_cons;	/* HSI_COMMENT: word1 */
	__le16 dif_write_prod;	/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le32 dif_error_buffer_address_lo;	/* HSI_COMMENT: reg6 */
	__le32 dif_error_buffer_address_hi;	/* HSI_COMMENT: reg7 */
};

/* RDMA task context */
struct rdma_task_context {
	struct ystorm_rdma_task_st_ctx ystorm_st_context;	/* HSI_COMMENT: ystorm storm context */
	struct ystorm_rdma_task_ag_ctx ystorm_ag_context;	/* HSI_COMMENT: ystorm aggregative context */
	struct tdif_task_context tdif_context;	/* HSI_COMMENT: tdif context */
	struct mstorm_rdma_task_ag_ctx mstorm_ag_context;	/* HSI_COMMENT: mstorm aggregative context */
	struct mstorm_rdma_task_st_ctx mstorm_st_context;	/* HSI_COMMENT: mstorm storm context */
	struct rdif_task_context rdif_context;	/* HSI_COMMENT: rdif context */
	struct ustorm_rdma_task_st_ctx ustorm_st_context;	/* HSI_COMMENT: ustorm storm context */
	struct regpair ustorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct ustorm_rdma_task_ag_ctx ustorm_ag_context;	/* HSI_COMMENT: ustorm aggregative context */
};

/* RDMA Tid type enumeration (for register_tid ramrod) */
enum rdma_tid_type {
	RDMA_TID_REGISTERED_MR,
	RDMA_TID_FMR,
	RDMA_TID_MW,
	MAX_RDMA_TID_TYPE
};

/* The rdma XRC SRQ context */
struct rdma_xrc_srq_context {
	struct regpair temp[9];
};

struct tstorm_rdma_task_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	__le16 word0;		/* HSI_COMMENT: icid */
	u8 flags0;
#define TSTORM_RDMA_TASK_AG_CTX_NIBBLE0_MASK		0xF	/* HSI_COMMENT: connection_type */
#define TSTORM_RDMA_TASK_AG_CTX_NIBBLE0_SHIFT		0
#define TSTORM_RDMA_TASK_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define TSTORM_RDMA_TASK_AG_CTX_BIT0_SHIFT		4
#define TSTORM_RDMA_TASK_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define TSTORM_RDMA_TASK_AG_CTX_BIT1_SHIFT		5
#define TSTORM_RDMA_TASK_AG_CTX_BIT2_MASK		0x1	/* HSI_COMMENT: bit2 */
#define TSTORM_RDMA_TASK_AG_CTX_BIT2_SHIFT		6
#define TSTORM_RDMA_TASK_AG_CTX_BIT3_MASK		0x1	/* HSI_COMMENT: bit3 */
#define TSTORM_RDMA_TASK_AG_CTX_BIT3_SHIFT		7
	u8 flags1;
#define TSTORM_RDMA_TASK_AG_CTX_BIT4_MASK		0x1	/* HSI_COMMENT: bit4 */
#define TSTORM_RDMA_TASK_AG_CTX_BIT4_SHIFT		0
#define TSTORM_RDMA_TASK_AG_CTX_BIT5_MASK		0x1	/* HSI_COMMENT: bit5 */
#define TSTORM_RDMA_TASK_AG_CTX_BIT5_SHIFT		1
#define TSTORM_RDMA_TASK_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: timer0cf */
#define TSTORM_RDMA_TASK_AG_CTX_CF0_SHIFT		2
#define TSTORM_RDMA_TASK_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: timer1cf */
#define TSTORM_RDMA_TASK_AG_CTX_CF1_SHIFT		4
#define TSTORM_RDMA_TASK_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: timer2cf */
#define TSTORM_RDMA_TASK_AG_CTX_CF2_SHIFT		6
	u8 flags2;
#define TSTORM_RDMA_TASK_AG_CTX_CF3_MASK		0x3	/* HSI_COMMENT: timer_stop_all */
#define TSTORM_RDMA_TASK_AG_CTX_CF3_SHIFT		0
#define TSTORM_RDMA_TASK_AG_CTX_CF4_MASK		0x3	/* HSI_COMMENT: cf4 */
#define TSTORM_RDMA_TASK_AG_CTX_CF4_SHIFT		2
#define TSTORM_RDMA_TASK_AG_CTX_CF5_MASK		0x3	/* HSI_COMMENT: cf5 */
#define TSTORM_RDMA_TASK_AG_CTX_CF5_SHIFT		4
#define TSTORM_RDMA_TASK_AG_CTX_CF6_MASK		0x3	/* HSI_COMMENT: cf6 */
#define TSTORM_RDMA_TASK_AG_CTX_CF6_SHIFT		6
	u8 flags3;
#define TSTORM_RDMA_TASK_AG_CTX_CF7_MASK		0x3	/* HSI_COMMENT: cf7 */
#define TSTORM_RDMA_TASK_AG_CTX_CF7_SHIFT		0
#define TSTORM_RDMA_TASK_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define TSTORM_RDMA_TASK_AG_CTX_CF0EN_SHIFT		2
#define TSTORM_RDMA_TASK_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define TSTORM_RDMA_TASK_AG_CTX_CF1EN_SHIFT		3
#define TSTORM_RDMA_TASK_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define TSTORM_RDMA_TASK_AG_CTX_CF2EN_SHIFT		4
#define TSTORM_RDMA_TASK_AG_CTX_CF3EN_MASK		0x1	/* HSI_COMMENT: cf3en */
#define TSTORM_RDMA_TASK_AG_CTX_CF3EN_SHIFT		5
#define TSTORM_RDMA_TASK_AG_CTX_CF4EN_MASK		0x1	/* HSI_COMMENT: cf4en */
#define TSTORM_RDMA_TASK_AG_CTX_CF4EN_SHIFT		6
#define TSTORM_RDMA_TASK_AG_CTX_CF5EN_MASK		0x1	/* HSI_COMMENT: cf5en */
#define TSTORM_RDMA_TASK_AG_CTX_CF5EN_SHIFT		7
	u8 flags4;
#define TSTORM_RDMA_TASK_AG_CTX_CF6EN_MASK		0x1	/* HSI_COMMENT: cf6en */
#define TSTORM_RDMA_TASK_AG_CTX_CF6EN_SHIFT		0
#define TSTORM_RDMA_TASK_AG_CTX_CF7EN_MASK		0x1	/* HSI_COMMENT: cf7en */
#define TSTORM_RDMA_TASK_AG_CTX_CF7EN_SHIFT		1
#define TSTORM_RDMA_TASK_AG_CTX_RULE0EN_MASK		0x1	/* HSI_COMMENT: rule0en */
#define TSTORM_RDMA_TASK_AG_CTX_RULE0EN_SHIFT		2
#define TSTORM_RDMA_TASK_AG_CTX_RULE1EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define TSTORM_RDMA_TASK_AG_CTX_RULE1EN_SHIFT		3
#define TSTORM_RDMA_TASK_AG_CTX_RULE2EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define TSTORM_RDMA_TASK_AG_CTX_RULE2EN_SHIFT		4
#define TSTORM_RDMA_TASK_AG_CTX_RULE3EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define TSTORM_RDMA_TASK_AG_CTX_RULE3EN_SHIFT		5
#define TSTORM_RDMA_TASK_AG_CTX_RULE4EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define TSTORM_RDMA_TASK_AG_CTX_RULE4EN_SHIFT		6
#define TSTORM_RDMA_TASK_AG_CTX_RULE5EN_MASK		0x1	/* HSI_COMMENT: rule5en */
#define TSTORM_RDMA_TASK_AG_CTX_RULE5EN_SHIFT		7
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

struct ustorm_rdma_conn_ag_ctx {
	u8 reserved;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define USTORM_RDMA_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define USTORM_RDMA_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define USTORM_RDMA_CONN_AG_CTX_DIF_ERROR_REPORTED_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define USTORM_RDMA_CONN_AG_CTX_DIF_ERROR_REPORTED_SHIFT	1
#define USTORM_RDMA_CONN_AG_CTX_FLUSH_Q0_CF_MASK		0x3	/* HSI_COMMENT: timer0cf */
#define USTORM_RDMA_CONN_AG_CTX_FLUSH_Q0_CF_SHIFT		2
#define USTORM_RDMA_CONN_AG_CTX_CF1_MASK			0x3	/* HSI_COMMENT: timer1cf */
#define USTORM_RDMA_CONN_AG_CTX_CF1_SHIFT			4
#define USTORM_RDMA_CONN_AG_CTX_CF2_MASK			0x3	/* HSI_COMMENT: timer2cf */
#define USTORM_RDMA_CONN_AG_CTX_CF2_SHIFT			6
	u8 flags1;
#define USTORM_RDMA_CONN_AG_CTX_CF3_MASK			0x3	/* HSI_COMMENT: timer_stop_all */
#define USTORM_RDMA_CONN_AG_CTX_CF3_SHIFT			0
#define USTORM_RDMA_CONN_AG_CTX_CQ_ARM_SE_CF_MASK		0x3	/* HSI_COMMENT: cf4 */
#define USTORM_RDMA_CONN_AG_CTX_CQ_ARM_SE_CF_SHIFT		2
#define USTORM_RDMA_CONN_AG_CTX_CQ_ARM_CF_MASK			0x3	/* HSI_COMMENT: cf5 */
#define USTORM_RDMA_CONN_AG_CTX_CQ_ARM_CF_SHIFT			4
#define USTORM_RDMA_CONN_AG_CTX_CF6_MASK			0x3	/* HSI_COMMENT: cf6 */
#define USTORM_RDMA_CONN_AG_CTX_CF6_SHIFT			6
	u8 flags2;
#define USTORM_RDMA_CONN_AG_CTX_FLUSH_Q0_CF_EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define USTORM_RDMA_CONN_AG_CTX_FLUSH_Q0_CF_EN_SHIFT		0
#define USTORM_RDMA_CONN_AG_CTX_CF1EN_MASK			0x1	/* HSI_COMMENT: cf1en */
#define USTORM_RDMA_CONN_AG_CTX_CF1EN_SHIFT			1
#define USTORM_RDMA_CONN_AG_CTX_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define USTORM_RDMA_CONN_AG_CTX_CF2EN_SHIFT			2
#define USTORM_RDMA_CONN_AG_CTX_CF3EN_MASK			0x1	/* HSI_COMMENT: cf3en */
#define USTORM_RDMA_CONN_AG_CTX_CF3EN_SHIFT			3
#define USTORM_RDMA_CONN_AG_CTX_CQ_ARM_SE_CF_EN_MASK		0x1	/* HSI_COMMENT: cf4en */
#define USTORM_RDMA_CONN_AG_CTX_CQ_ARM_SE_CF_EN_SHIFT		4
#define USTORM_RDMA_CONN_AG_CTX_CQ_ARM_CF_EN_MASK		0x1	/* HSI_COMMENT: cf5en */
#define USTORM_RDMA_CONN_AG_CTX_CQ_ARM_CF_EN_SHIFT		5
#define USTORM_RDMA_CONN_AG_CTX_CF6EN_MASK			0x1	/* HSI_COMMENT: cf6en */
#define USTORM_RDMA_CONN_AG_CTX_CF6EN_SHIFT			6
#define USTORM_RDMA_CONN_AG_CTX_CQ_SE_EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define USTORM_RDMA_CONN_AG_CTX_CQ_SE_EN_SHIFT			7
	u8 flags3;
#define USTORM_RDMA_CONN_AG_CTX_CQ_EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define USTORM_RDMA_CONN_AG_CTX_CQ_EN_SHIFT			0
#define USTORM_RDMA_CONN_AG_CTX_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define USTORM_RDMA_CONN_AG_CTX_RULE2EN_SHIFT			1
#define USTORM_RDMA_CONN_AG_CTX_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define USTORM_RDMA_CONN_AG_CTX_RULE3EN_SHIFT			2
#define USTORM_RDMA_CONN_AG_CTX_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define USTORM_RDMA_CONN_AG_CTX_RULE4EN_SHIFT			3
#define USTORM_RDMA_CONN_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define USTORM_RDMA_CONN_AG_CTX_RULE5EN_SHIFT			4
#define USTORM_RDMA_CONN_AG_CTX_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define USTORM_RDMA_CONN_AG_CTX_RULE6EN_SHIFT			5
#define USTORM_RDMA_CONN_AG_CTX_RULE7EN_MASK			0x1	/* HSI_COMMENT: rule7en */
#define USTORM_RDMA_CONN_AG_CTX_RULE7EN_SHIFT			6
#define USTORM_RDMA_CONN_AG_CTX_RULE8EN_MASK			0x1	/* HSI_COMMENT: rule8en */
#define USTORM_RDMA_CONN_AG_CTX_RULE8EN_SHIFT			7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 nvmf_only;		/* HSI_COMMENT: byte3 */
	__le16 conn_dpi;	/* HSI_COMMENT: conn_dpi */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le32 cq_cons;		/* HSI_COMMENT: reg0 */
	__le32 cq_se_prod;	/* HSI_COMMENT: reg1 */
	__le32 cq_prod;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le16 int_timeout;	/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
};

/************************************************************************/
/* Add include to qed hsi rdma target for both roce and iwarp qed driver */
/************************************************************************/
/************************************************************************/
/* Add include to common roce target for both eCore and protocol roce driver */
/************************************************************************/

/* The roce storm context of Mstorm */
struct mstorm_roce_conn_st_ctx {
	struct regpair temp[6];
};

/* The roce storm context of Mstorm */
struct pstorm_roce_conn_st_ctx {
	struct regpair temp[16];
};

/* The roce storm context of Ystorm */
struct ystorm_roce_conn_st_ctx {
	struct regpair temp[2];
};

/* The roce storm context of Xstorm */
struct xstorm_roce_conn_st_ctx {
	struct regpair temp[24];
};

struct xstorm_roce_conn_ag_ctx {
	u8 reserved0;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define XSTORM_ROCE_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define XSTORM_ROCE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define XSTORM_ROCE_CONN_AG_CTX_BIT1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define XSTORM_ROCE_CONN_AG_CTX_BIT1_SHIFT			1
#define XSTORM_ROCE_CONN_AG_CTX_BIT2_MASK			0x1	/* HSI_COMMENT: exist_in_qm2 */
#define XSTORM_ROCE_CONN_AG_CTX_BIT2_SHIFT			2
#define XSTORM_ROCE_CONN_AG_CTX_EXIST_IN_QM3_MASK		0x1	/* HSI_COMMENT: exist_in_qm3 */
#define XSTORM_ROCE_CONN_AG_CTX_EXIST_IN_QM3_SHIFT		3
#define XSTORM_ROCE_CONN_AG_CTX_BIT4_MASK			0x1	/* HSI_COMMENT: bit4 */
#define XSTORM_ROCE_CONN_AG_CTX_BIT4_SHIFT			4
#define XSTORM_ROCE_CONN_AG_CTX_BIT5_MASK			0x1	/* HSI_COMMENT: cf_array_active */
#define XSTORM_ROCE_CONN_AG_CTX_BIT5_SHIFT			5
#define XSTORM_ROCE_CONN_AG_CTX_BIT6_MASK			0x1	/* HSI_COMMENT: bit6 */
#define XSTORM_ROCE_CONN_AG_CTX_BIT6_SHIFT			6
#define XSTORM_ROCE_CONN_AG_CTX_BIT7_MASK			0x1	/* HSI_COMMENT: bit7 */
#define XSTORM_ROCE_CONN_AG_CTX_BIT7_SHIFT			7
	u8 flags1;
#define XSTORM_ROCE_CONN_AG_CTX_BIT8_MASK			0x1	/* HSI_COMMENT: bit8 */
#define XSTORM_ROCE_CONN_AG_CTX_BIT8_SHIFT			0
#define XSTORM_ROCE_CONN_AG_CTX_BIT9_MASK			0x1	/* HSI_COMMENT: bit9 */
#define XSTORM_ROCE_CONN_AG_CTX_BIT9_SHIFT			1
#define XSTORM_ROCE_CONN_AG_CTX_BIT10_MASK			0x1	/* HSI_COMMENT: bit10 */
#define XSTORM_ROCE_CONN_AG_CTX_BIT10_SHIFT			2
#define XSTORM_ROCE_CONN_AG_CTX_BIT11_MASK			0x1	/* HSI_COMMENT: bit11 */
#define XSTORM_ROCE_CONN_AG_CTX_BIT11_SHIFT			3
#define XSTORM_ROCE_CONN_AG_CTX_MSDM_FLUSH_MASK			0x1	/* HSI_COMMENT: bit12 */
#define XSTORM_ROCE_CONN_AG_CTX_MSDM_FLUSH_SHIFT		4
#define XSTORM_ROCE_CONN_AG_CTX_MSEM_FLUSH_MASK			0x1	/* HSI_COMMENT: bit13 */
#define XSTORM_ROCE_CONN_AG_CTX_MSEM_FLUSH_SHIFT		5
#define XSTORM_ROCE_CONN_AG_CTX_BIT14_MASK			0x1	/* HSI_COMMENT: bit14 */
#define XSTORM_ROCE_CONN_AG_CTX_BIT14_SHIFT			6
#define XSTORM_ROCE_CONN_AG_CTX_YSTORM_FLUSH_MASK		0x1	/* HSI_COMMENT: bit15 */
#define XSTORM_ROCE_CONN_AG_CTX_YSTORM_FLUSH_SHIFT		7
	u8 flags2;
#define XSTORM_ROCE_CONN_AG_CTX_CF0_MASK			0x3	/* HSI_COMMENT: timer0cf */
#define XSTORM_ROCE_CONN_AG_CTX_CF0_SHIFT			0
#define XSTORM_ROCE_CONN_AG_CTX_CF1_MASK			0x3	/* HSI_COMMENT: timer1cf */
#define XSTORM_ROCE_CONN_AG_CTX_CF1_SHIFT			2
#define XSTORM_ROCE_CONN_AG_CTX_CF2_MASK			0x3	/* HSI_COMMENT: timer2cf */
#define XSTORM_ROCE_CONN_AG_CTX_CF2_SHIFT			4
#define XSTORM_ROCE_CONN_AG_CTX_CF3_MASK			0x3	/* HSI_COMMENT: timer_stop_all */
#define XSTORM_ROCE_CONN_AG_CTX_CF3_SHIFT			6
	u8 flags3;
#define XSTORM_ROCE_CONN_AG_CTX_CF4_MASK			0x3	/* HSI_COMMENT: cf4 */
#define XSTORM_ROCE_CONN_AG_CTX_CF4_SHIFT			0
#define XSTORM_ROCE_CONN_AG_CTX_CF5_MASK			0x3	/* HSI_COMMENT: cf5 */
#define XSTORM_ROCE_CONN_AG_CTX_CF5_SHIFT			2
#define XSTORM_ROCE_CONN_AG_CTX_CF6_MASK			0x3	/* HSI_COMMENT: cf6 */
#define XSTORM_ROCE_CONN_AG_CTX_CF6_SHIFT			4
#define XSTORM_ROCE_CONN_AG_CTX_FLUSH_Q0_CF_MASK		0x3	/* HSI_COMMENT: cf7 */
#define XSTORM_ROCE_CONN_AG_CTX_FLUSH_Q0_CF_SHIFT		6
	u8 flags4;
#define XSTORM_ROCE_CONN_AG_CTX_CF8_MASK			0x3	/* HSI_COMMENT: cf8 */
#define XSTORM_ROCE_CONN_AG_CTX_CF8_SHIFT			0
#define XSTORM_ROCE_CONN_AG_CTX_CF9_MASK			0x3	/* HSI_COMMENT: cf9 */
#define XSTORM_ROCE_CONN_AG_CTX_CF9_SHIFT			2
#define XSTORM_ROCE_CONN_AG_CTX_CF10_MASK			0x3	/* HSI_COMMENT: cf10 */
#define XSTORM_ROCE_CONN_AG_CTX_CF10_SHIFT			4
#define XSTORM_ROCE_CONN_AG_CTX_CF11_MASK			0x3	/* HSI_COMMENT: cf11 */
#define XSTORM_ROCE_CONN_AG_CTX_CF11_SHIFT			6
	u8 flags5;
#define XSTORM_ROCE_CONN_AG_CTX_CF12_MASK			0x3	/* HSI_COMMENT: cf12 */
#define XSTORM_ROCE_CONN_AG_CTX_CF12_SHIFT			0
#define XSTORM_ROCE_CONN_AG_CTX_CF13_MASK			0x3	/* HSI_COMMENT: cf13 */
#define XSTORM_ROCE_CONN_AG_CTX_CF13_SHIFT			2
#define XSTORM_ROCE_CONN_AG_CTX_CF14_MASK			0x3	/* HSI_COMMENT: cf14 */
#define XSTORM_ROCE_CONN_AG_CTX_CF14_SHIFT			4
#define XSTORM_ROCE_CONN_AG_CTX_CF15_MASK			0x3	/* HSI_COMMENT: cf15 */
#define XSTORM_ROCE_CONN_AG_CTX_CF15_SHIFT			6
	u8 flags6;
#define XSTORM_ROCE_CONN_AG_CTX_CF16_MASK			0x3	/* HSI_COMMENT: cf16 */
#define XSTORM_ROCE_CONN_AG_CTX_CF16_SHIFT			0
#define XSTORM_ROCE_CONN_AG_CTX_CF17_MASK			0x3	/* HSI_COMMENT: cf_array_cf */
#define XSTORM_ROCE_CONN_AG_CTX_CF17_SHIFT			2
#define XSTORM_ROCE_CONN_AG_CTX_CF18_MASK			0x3	/* HSI_COMMENT: cf18 */
#define XSTORM_ROCE_CONN_AG_CTX_CF18_SHIFT			4
#define XSTORM_ROCE_CONN_AG_CTX_CF19_MASK			0x3	/* HSI_COMMENT: cf19 */
#define XSTORM_ROCE_CONN_AG_CTX_CF19_SHIFT			6
	u8 flags7;
#define XSTORM_ROCE_CONN_AG_CTX_CF20_MASK			0x3	/* HSI_COMMENT: cf20 */
#define XSTORM_ROCE_CONN_AG_CTX_CF20_SHIFT			0
#define XSTORM_ROCE_CONN_AG_CTX_CF21_MASK			0x3	/* HSI_COMMENT: cf21 */
#define XSTORM_ROCE_CONN_AG_CTX_CF21_SHIFT			2
#define XSTORM_ROCE_CONN_AG_CTX_SLOW_PATH_MASK			0x3	/* HSI_COMMENT: cf22 */
#define XSTORM_ROCE_CONN_AG_CTX_SLOW_PATH_SHIFT			4
#define XSTORM_ROCE_CONN_AG_CTX_CF0EN_MASK			0x1	/* HSI_COMMENT: cf0en */
#define XSTORM_ROCE_CONN_AG_CTX_CF0EN_SHIFT			6
#define XSTORM_ROCE_CONN_AG_CTX_CF1EN_MASK			0x1	/* HSI_COMMENT: cf1en */
#define XSTORM_ROCE_CONN_AG_CTX_CF1EN_SHIFT			7
	u8 flags8;
#define XSTORM_ROCE_CONN_AG_CTX_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define XSTORM_ROCE_CONN_AG_CTX_CF2EN_SHIFT			0
#define XSTORM_ROCE_CONN_AG_CTX_CF3EN_MASK			0x1	/* HSI_COMMENT: cf3en */
#define XSTORM_ROCE_CONN_AG_CTX_CF3EN_SHIFT			1
#define XSTORM_ROCE_CONN_AG_CTX_CF4EN_MASK			0x1	/* HSI_COMMENT: cf4en */
#define XSTORM_ROCE_CONN_AG_CTX_CF4EN_SHIFT			2
#define XSTORM_ROCE_CONN_AG_CTX_CF5EN_MASK			0x1	/* HSI_COMMENT: cf5en */
#define XSTORM_ROCE_CONN_AG_CTX_CF5EN_SHIFT			3
#define XSTORM_ROCE_CONN_AG_CTX_CF6EN_MASK			0x1	/* HSI_COMMENT: cf6en */
#define XSTORM_ROCE_CONN_AG_CTX_CF6EN_SHIFT			4
#define XSTORM_ROCE_CONN_AG_CTX_FLUSH_Q0_CF_EN_MASK		0x1	/* HSI_COMMENT: cf7en */
#define XSTORM_ROCE_CONN_AG_CTX_FLUSH_Q0_CF_EN_SHIFT		5
#define XSTORM_ROCE_CONN_AG_CTX_CF8EN_MASK			0x1	/* HSI_COMMENT: cf8en */
#define XSTORM_ROCE_CONN_AG_CTX_CF8EN_SHIFT			6
#define XSTORM_ROCE_CONN_AG_CTX_CF9EN_MASK			0x1	/* HSI_COMMENT: cf9en */
#define XSTORM_ROCE_CONN_AG_CTX_CF9EN_SHIFT			7
	u8 flags9;
#define XSTORM_ROCE_CONN_AG_CTX_CF10EN_MASK			0x1	/* HSI_COMMENT: cf10en */
#define XSTORM_ROCE_CONN_AG_CTX_CF10EN_SHIFT			0
#define XSTORM_ROCE_CONN_AG_CTX_CF11EN_MASK			0x1	/* HSI_COMMENT: cf11en */
#define XSTORM_ROCE_CONN_AG_CTX_CF11EN_SHIFT			1
#define XSTORM_ROCE_CONN_AG_CTX_CF12EN_MASK			0x1	/* HSI_COMMENT: cf12en */
#define XSTORM_ROCE_CONN_AG_CTX_CF12EN_SHIFT			2
#define XSTORM_ROCE_CONN_AG_CTX_CF13EN_MASK			0x1	/* HSI_COMMENT: cf13en */
#define XSTORM_ROCE_CONN_AG_CTX_CF13EN_SHIFT			3
#define XSTORM_ROCE_CONN_AG_CTX_CF14EN_MASK			0x1	/* HSI_COMMENT: cf14en */
#define XSTORM_ROCE_CONN_AG_CTX_CF14EN_SHIFT			4
#define XSTORM_ROCE_CONN_AG_CTX_CF15EN_MASK			0x1	/* HSI_COMMENT: cf15en */
#define XSTORM_ROCE_CONN_AG_CTX_CF15EN_SHIFT			5
#define XSTORM_ROCE_CONN_AG_CTX_CF16EN_MASK			0x1	/* HSI_COMMENT: cf16en */
#define XSTORM_ROCE_CONN_AG_CTX_CF16EN_SHIFT			6
#define XSTORM_ROCE_CONN_AG_CTX_CF17EN_MASK			0x1	/* HSI_COMMENT: cf_array_cf_en */
#define XSTORM_ROCE_CONN_AG_CTX_CF17EN_SHIFT			7
	u8 flags10;
#define XSTORM_ROCE_CONN_AG_CTX_CF18EN_MASK			0x1	/* HSI_COMMENT: cf18en */
#define XSTORM_ROCE_CONN_AG_CTX_CF18EN_SHIFT			0
#define XSTORM_ROCE_CONN_AG_CTX_CF19EN_MASK			0x1	/* HSI_COMMENT: cf19en */
#define XSTORM_ROCE_CONN_AG_CTX_CF19EN_SHIFT			1
#define XSTORM_ROCE_CONN_AG_CTX_CF20EN_MASK			0x1	/* HSI_COMMENT: cf20en */
#define XSTORM_ROCE_CONN_AG_CTX_CF20EN_SHIFT			2
#define XSTORM_ROCE_CONN_AG_CTX_CF21EN_MASK			0x1	/* HSI_COMMENT: cf21en */
#define XSTORM_ROCE_CONN_AG_CTX_CF21EN_SHIFT			3
#define XSTORM_ROCE_CONN_AG_CTX_SLOW_PATH_EN_MASK		0x1	/* HSI_COMMENT: cf22en */
#define XSTORM_ROCE_CONN_AG_CTX_SLOW_PATH_EN_SHIFT		4
#define XSTORM_ROCE_CONN_AG_CTX_CF23EN_MASK			0x1	/* HSI_COMMENT: cf23en */
#define XSTORM_ROCE_CONN_AG_CTX_CF23EN_SHIFT			5
#define XSTORM_ROCE_CONN_AG_CTX_RULE0EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define XSTORM_ROCE_CONN_AG_CTX_RULE0EN_SHIFT			6
#define XSTORM_ROCE_CONN_AG_CTX_RULE1EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define XSTORM_ROCE_CONN_AG_CTX_RULE1EN_SHIFT			7
	u8 flags11;
#define XSTORM_ROCE_CONN_AG_CTX_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define XSTORM_ROCE_CONN_AG_CTX_RULE2EN_SHIFT			0
#define XSTORM_ROCE_CONN_AG_CTX_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define XSTORM_ROCE_CONN_AG_CTX_RULE3EN_SHIFT			1
#define XSTORM_ROCE_CONN_AG_CTX_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define XSTORM_ROCE_CONN_AG_CTX_RULE4EN_SHIFT			2
#define XSTORM_ROCE_CONN_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define XSTORM_ROCE_CONN_AG_CTX_RULE5EN_SHIFT			3
#define XSTORM_ROCE_CONN_AG_CTX_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define XSTORM_ROCE_CONN_AG_CTX_RULE6EN_SHIFT			4
#define XSTORM_ROCE_CONN_AG_CTX_RULE7EN_MASK			0x1	/* HSI_COMMENT: rule7en */
#define XSTORM_ROCE_CONN_AG_CTX_RULE7EN_SHIFT			5
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED1_MASK		0x1	/* HSI_COMMENT: rule8en */
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED1_SHIFT		6
#define XSTORM_ROCE_CONN_AG_CTX_RULE9EN_MASK			0x1	/* HSI_COMMENT: rule9en */
#define XSTORM_ROCE_CONN_AG_CTX_RULE9EN_SHIFT			7
	u8 flags12;
#define XSTORM_ROCE_CONN_AG_CTX_RULE10EN_MASK			0x1	/* HSI_COMMENT: rule10en */
#define XSTORM_ROCE_CONN_AG_CTX_RULE10EN_SHIFT			0
#define XSTORM_ROCE_CONN_AG_CTX_RULE11EN_MASK			0x1	/* HSI_COMMENT: rule11en */
#define XSTORM_ROCE_CONN_AG_CTX_RULE11EN_SHIFT			1
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED2_MASK		0x1	/* HSI_COMMENT: rule12en */
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED2_SHIFT		2
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED3_MASK		0x1	/* HSI_COMMENT: rule13en */
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED3_SHIFT		3
#define XSTORM_ROCE_CONN_AG_CTX_RULE14EN_MASK			0x1	/* HSI_COMMENT: rule14en */
#define XSTORM_ROCE_CONN_AG_CTX_RULE14EN_SHIFT			4
#define XSTORM_ROCE_CONN_AG_CTX_RULE15EN_MASK			0x1	/* HSI_COMMENT: rule15en */
#define XSTORM_ROCE_CONN_AG_CTX_RULE15EN_SHIFT			5
#define XSTORM_ROCE_CONN_AG_CTX_RULE16EN_MASK			0x1	/* HSI_COMMENT: rule16en */
#define XSTORM_ROCE_CONN_AG_CTX_RULE16EN_SHIFT			6
#define XSTORM_ROCE_CONN_AG_CTX_RULE17EN_MASK			0x1	/* HSI_COMMENT: rule17en */
#define XSTORM_ROCE_CONN_AG_CTX_RULE17EN_SHIFT			7
	u8 flags13;
#define XSTORM_ROCE_CONN_AG_CTX_RULE18EN_MASK			0x1	/* HSI_COMMENT: rule18en */
#define XSTORM_ROCE_CONN_AG_CTX_RULE18EN_SHIFT			0
#define XSTORM_ROCE_CONN_AG_CTX_RULE19EN_MASK			0x1	/* HSI_COMMENT: rule19en */
#define XSTORM_ROCE_CONN_AG_CTX_RULE19EN_SHIFT			1
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED4_MASK		0x1	/* HSI_COMMENT: rule20en */
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED4_SHIFT		2
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED5_MASK		0x1	/* HSI_COMMENT: rule21en */
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED5_SHIFT		3
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED6_MASK		0x1	/* HSI_COMMENT: rule22en */
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED6_SHIFT		4
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED7_MASK		0x1	/* HSI_COMMENT: rule23en */
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED7_SHIFT		5
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED8_MASK		0x1	/* HSI_COMMENT: rule24en */
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED8_SHIFT		6
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED9_MASK		0x1	/* HSI_COMMENT: rule25en */
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED9_SHIFT		7
	u8 flags14;
#define XSTORM_ROCE_CONN_AG_CTX_MIGRATION_MASK			0x1	/* HSI_COMMENT: bit16 */
#define XSTORM_ROCE_CONN_AG_CTX_MIGRATION_SHIFT			0
#define XSTORM_ROCE_CONN_AG_CTX_BIT17_MASK			0x1	/* HSI_COMMENT: bit17 */
#define XSTORM_ROCE_CONN_AG_CTX_BIT17_SHIFT			1
#define XSTORM_ROCE_CONN_AG_CTX_DPM_PORT_NUM_MASK		0x3	/* HSI_COMMENT: bit18 */
#define XSTORM_ROCE_CONN_AG_CTX_DPM_PORT_NUM_SHIFT		2
#define XSTORM_ROCE_CONN_AG_CTX_RESERVED_MASK			0x1	/* HSI_COMMENT: bit20 */
#define XSTORM_ROCE_CONN_AG_CTX_RESERVED_SHIFT			4
#define XSTORM_ROCE_CONN_AG_CTX_ROCE_EDPM_ENABLE_MASK		0x1	/* HSI_COMMENT: bit21 */
#define XSTORM_ROCE_CONN_AG_CTX_ROCE_EDPM_ENABLE_SHIFT		5
#define XSTORM_ROCE_CONN_AG_CTX_CF23_MASK			0x3	/* HSI_COMMENT: cf23 */
#define XSTORM_ROCE_CONN_AG_CTX_CF23_SHIFT			6
	u8 byte2;		/* HSI_COMMENT: byte2 */
	__le16 physical_q0;	/* HSI_COMMENT: physical_q0 */
	__le16 word1;		/* HSI_COMMENT: physical_q1 */
	__le16 word2;		/* HSI_COMMENT: physical_q2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le16 word4;		/* HSI_COMMENT: word4 */
	__le16 word5;		/* HSI_COMMENT: word5 */
	__le16 conn_dpi;	/* HSI_COMMENT: conn_dpi */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	u8 byte5;		/* HSI_COMMENT: byte5 */
	u8 byte6;		/* HSI_COMMENT: byte6 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 snd_nxt_psn;	/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
	__le32 reg5;		/* HSI_COMMENT: cf_array0 */
	__le32 reg6;		/* HSI_COMMENT: cf_array1 */
};

struct tstorm_roce_conn_ag_ctx {
	u8 reserved0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define TSTORM_ROCE_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define TSTORM_ROCE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define TSTORM_ROCE_CONN_AG_CTX_BIT1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define TSTORM_ROCE_CONN_AG_CTX_BIT1_SHIFT			1
#define TSTORM_ROCE_CONN_AG_CTX_BIT2_MASK			0x1	/* HSI_COMMENT: bit2 */
#define TSTORM_ROCE_CONN_AG_CTX_BIT2_SHIFT			2
#define TSTORM_ROCE_CONN_AG_CTX_BIT3_MASK			0x1	/* HSI_COMMENT: bit3 */
#define TSTORM_ROCE_CONN_AG_CTX_BIT3_SHIFT			3
#define TSTORM_ROCE_CONN_AG_CTX_BIT4_MASK			0x1	/* HSI_COMMENT: bit4 */
#define TSTORM_ROCE_CONN_AG_CTX_BIT4_SHIFT			4
#define TSTORM_ROCE_CONN_AG_CTX_BIT5_MASK			0x1	/* HSI_COMMENT: bit5 */
#define TSTORM_ROCE_CONN_AG_CTX_BIT5_SHIFT			5
#define TSTORM_ROCE_CONN_AG_CTX_CF0_MASK			0x3	/* HSI_COMMENT: timer0cf */
#define TSTORM_ROCE_CONN_AG_CTX_CF0_SHIFT			6
	u8 flags1;
#define TSTORM_ROCE_CONN_AG_CTX_MSTORM_FLUSH_CF_MASK		0x3	/* HSI_COMMENT: timer1cf */
#define TSTORM_ROCE_CONN_AG_CTX_MSTORM_FLUSH_CF_SHIFT		0
#define TSTORM_ROCE_CONN_AG_CTX_CF2_MASK			0x3	/* HSI_COMMENT: timer2cf */
#define TSTORM_ROCE_CONN_AG_CTX_CF2_SHIFT			2
#define TSTORM_ROCE_CONN_AG_CTX_TIMER_STOP_ALL_CF_MASK		0x3	/* HSI_COMMENT: timer_stop_all */
#define TSTORM_ROCE_CONN_AG_CTX_TIMER_STOP_ALL_CF_SHIFT		4
#define TSTORM_ROCE_CONN_AG_CTX_FLUSH_Q0_CF_MASK		0x3	/* HSI_COMMENT: cf4 */
#define TSTORM_ROCE_CONN_AG_CTX_FLUSH_Q0_CF_SHIFT		6
	u8 flags2;
#define TSTORM_ROCE_CONN_AG_CTX_CF5_MASK			0x3	/* HSI_COMMENT: cf5 */
#define TSTORM_ROCE_CONN_AG_CTX_CF5_SHIFT			0
#define TSTORM_ROCE_CONN_AG_CTX_CF6_MASK			0x3	/* HSI_COMMENT: cf6 */
#define TSTORM_ROCE_CONN_AG_CTX_CF6_SHIFT			2
#define TSTORM_ROCE_CONN_AG_CTX_CF7_MASK			0x3	/* HSI_COMMENT: cf7 */
#define TSTORM_ROCE_CONN_AG_CTX_CF7_SHIFT			4
#define TSTORM_ROCE_CONN_AG_CTX_CF8_MASK			0x3	/* HSI_COMMENT: cf8 */
#define TSTORM_ROCE_CONN_AG_CTX_CF8_SHIFT			6
	u8 flags3;
#define TSTORM_ROCE_CONN_AG_CTX_CF9_MASK			0x3	/* HSI_COMMENT: cf9 */
#define TSTORM_ROCE_CONN_AG_CTX_CF9_SHIFT			0
#define TSTORM_ROCE_CONN_AG_CTX_CF10_MASK			0x3	/* HSI_COMMENT: cf10 */
#define TSTORM_ROCE_CONN_AG_CTX_CF10_SHIFT			2
#define TSTORM_ROCE_CONN_AG_CTX_CF0EN_MASK			0x1	/* HSI_COMMENT: cf0en */
#define TSTORM_ROCE_CONN_AG_CTX_CF0EN_SHIFT			4
#define TSTORM_ROCE_CONN_AG_CTX_MSTORM_FLUSH_CF_EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define TSTORM_ROCE_CONN_AG_CTX_MSTORM_FLUSH_CF_EN_SHIFT	5
#define TSTORM_ROCE_CONN_AG_CTX_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define TSTORM_ROCE_CONN_AG_CTX_CF2EN_SHIFT			6
#define TSTORM_ROCE_CONN_AG_CTX_TIMER_STOP_ALL_CF_EN_MASK	0x1	/* HSI_COMMENT: cf3en */
#define TSTORM_ROCE_CONN_AG_CTX_TIMER_STOP_ALL_CF_EN_SHIFT	7
	u8 flags4;
#define TSTORM_ROCE_CONN_AG_CTX_FLUSH_Q0_CF_EN_MASK		0x1	/* HSI_COMMENT: cf4en */
#define TSTORM_ROCE_CONN_AG_CTX_FLUSH_Q0_CF_EN_SHIFT		0
#define TSTORM_ROCE_CONN_AG_CTX_CF5EN_MASK			0x1	/* HSI_COMMENT: cf5en */
#define TSTORM_ROCE_CONN_AG_CTX_CF5EN_SHIFT			1
#define TSTORM_ROCE_CONN_AG_CTX_CF6EN_MASK			0x1	/* HSI_COMMENT: cf6en */
#define TSTORM_ROCE_CONN_AG_CTX_CF6EN_SHIFT			2
#define TSTORM_ROCE_CONN_AG_CTX_CF7EN_MASK			0x1	/* HSI_COMMENT: cf7en */
#define TSTORM_ROCE_CONN_AG_CTX_CF7EN_SHIFT			3
#define TSTORM_ROCE_CONN_AG_CTX_CF8EN_MASK			0x1	/* HSI_COMMENT: cf8en */
#define TSTORM_ROCE_CONN_AG_CTX_CF8EN_SHIFT			4
#define TSTORM_ROCE_CONN_AG_CTX_CF9EN_MASK			0x1	/* HSI_COMMENT: cf9en */
#define TSTORM_ROCE_CONN_AG_CTX_CF9EN_SHIFT			5
#define TSTORM_ROCE_CONN_AG_CTX_CF10EN_MASK			0x1	/* HSI_COMMENT: cf10en */
#define TSTORM_ROCE_CONN_AG_CTX_CF10EN_SHIFT			6
#define TSTORM_ROCE_CONN_AG_CTX_RULE0EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define TSTORM_ROCE_CONN_AG_CTX_RULE0EN_SHIFT			7
	u8 flags5;
#define TSTORM_ROCE_CONN_AG_CTX_RULE1EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define TSTORM_ROCE_CONN_AG_CTX_RULE1EN_SHIFT			0
#define TSTORM_ROCE_CONN_AG_CTX_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define TSTORM_ROCE_CONN_AG_CTX_RULE2EN_SHIFT			1
#define TSTORM_ROCE_CONN_AG_CTX_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define TSTORM_ROCE_CONN_AG_CTX_RULE3EN_SHIFT			2
#define TSTORM_ROCE_CONN_AG_CTX_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define TSTORM_ROCE_CONN_AG_CTX_RULE4EN_SHIFT			3
#define TSTORM_ROCE_CONN_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define TSTORM_ROCE_CONN_AG_CTX_RULE5EN_SHIFT			4
#define TSTORM_ROCE_CONN_AG_CTX_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define TSTORM_ROCE_CONN_AG_CTX_RULE6EN_SHIFT			5
#define TSTORM_ROCE_CONN_AG_CTX_RULE7EN_MASK			0x1	/* HSI_COMMENT: rule7en */
#define TSTORM_ROCE_CONN_AG_CTX_RULE7EN_SHIFT			6
#define TSTORM_ROCE_CONN_AG_CTX_RULE8EN_MASK			0x1	/* HSI_COMMENT: rule8en */
#define TSTORM_ROCE_CONN_AG_CTX_RULE8EN_SHIFT			7
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
	__le32 reg5;		/* HSI_COMMENT: reg5 */
	__le32 reg6;		/* HSI_COMMENT: reg6 */
	__le32 reg7;		/* HSI_COMMENT: reg7 */
	__le32 reg8;		/* HSI_COMMENT: reg8 */
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: word0 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	u8 byte5;		/* HSI_COMMENT: byte5 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le16 word2;		/* HSI_COMMENT: conn_dpi */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le32 reg9;		/* HSI_COMMENT: reg9 */
	__le32 reg10;		/* HSI_COMMENT: reg10 */
};

/* The roce storm context of Tstorm */
struct tstorm_roce_conn_st_ctx {
	struct regpair temp[30];
};

/* The roce storm context of Ystorm */
struct ustorm_roce_conn_st_ctx {
	struct regpair temp[14];
};

/* roce connection context */
struct roce_conn_context {
	struct ystorm_roce_conn_st_ctx ystorm_st_context;	/* HSI_COMMENT: ystorm storm context */
	struct regpair ystorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct pstorm_roce_conn_st_ctx pstorm_st_context;	/* HSI_COMMENT: pstorm storm context */
	struct xstorm_roce_conn_st_ctx xstorm_st_context;	/* HSI_COMMENT: xstorm storm context */
	struct xstorm_roce_conn_ag_ctx xstorm_ag_context;	/* HSI_COMMENT: xstorm aggregative context */
	struct tstorm_roce_conn_ag_ctx tstorm_ag_context;	/* HSI_COMMENT: tstorm aggregative context */
	struct timers_context timer_context;	/* HSI_COMMENT: timer context */
	struct ustorm_rdma_conn_ag_ctx ustorm_ag_context;	/* HSI_COMMENT: ustorm aggregative context */
	struct tstorm_roce_conn_st_ctx tstorm_st_context;	/* HSI_COMMENT: tstorm storm context */
	struct regpair tstorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct mstorm_roce_conn_st_ctx mstorm_st_context;	/* HSI_COMMENT: mstorm storm context */
	struct regpair mstorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct ustorm_roce_conn_st_ctx ustorm_st_context;	/* HSI_COMMENT: ustorm storm context */
	struct regpair ustorm_st_padding[2];	/* HSI_COMMENT: padding */
};

/* roce CQEs statistics */
struct roce_cqe_stats {
	__le32 req_cqe_error;
	__le32 req_remote_access_errors;
	__le32 req_remote_invalid_request;
	__le32 resp_cqe_error;
	__le32 resp_local_length_error;
	__le32 reserved;
};

/* roce create qp requester ramrod data */
struct roce_create_qp_req_ramrod_data {
	__le16 flags;
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_ROCE_FLAVOR_MASK			0x3	/* HSI_COMMENT: Use roce_flavor enum */
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_ROCE_FLAVOR_SHIFT		0
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_FMR_AND_RESERVED_EN_MASK		0x1
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_FMR_AND_RESERVED_EN_SHIFT	2
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_SIGNALED_COMP_MASK		0x1
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_SIGNALED_COMP_SHIFT		3
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_PRI_MASK				0x7
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_PRI_SHIFT			4
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_XRC_FLAG_MASK			0x1
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_XRC_FLAG_SHIFT			7
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_ERR_RETRY_CNT_MASK		0xF
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_ERR_RETRY_CNT_SHIFT		8
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_RNR_NAK_CNT_MASK			0xF
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_RNR_NAK_CNT_SHIFT		12
	u8 max_ord;
	u8 traffic_class;	/* HSI_COMMENT: In case of RRoCE on IPv4 will be used as TOS */
	u8 hop_limit;		/* HSI_COMMENT: In case of RRoCE on IPv4 will be used as TTL */
	u8 orq_num_pages;
	__le16 p_key;
	__le32 flow_label;
	__le32 dst_qp_id;
	__le32 ack_timeout_val;
	__le32 initial_psn;
	__le16 mtu;
	__le16 pd;
	__le16 sq_num_pages;
	__le16 low_latency_phy_queue;
	struct regpair sq_pbl_addr;
	struct regpair orq_pbl_addr;
	__le16 local_mac_addr[3];	/* HSI_COMMENT: BE order */
	__le16 remote_mac_addr[3];	/* HSI_COMMENT: BE order */
	__le16 vlan_id;
	__le16 udp_src_port;	/* HSI_COMMENT: Only relevant in RRoCE */
	__le32 src_gid[4];	/* HSI_COMMENT: BE order. In case of RRoCE on IPv4 the high register will hold the address. Low registers must be zero! */
	__le32 dst_gid[4];	/* HSI_COMMENT: BE order. In case of RRoCE on IPv4 the high register will hold the address. Low registers must be zero! */
	__le32 cq_cid;
	struct regpair qp_handle_for_cqe;
	struct regpair qp_handle_for_async;
	u8 stats_counter_id;	/* HSI_COMMENT: Statistics counter ID to use */
	u8 vf_id;
	u8 vport_id;
	u8 flags2;
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_EDPM_MODE_MASK		0x1
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_EDPM_MODE_SHIFT		0
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_VF_ID_VALID_MASK		0x1
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_VF_ID_VALID_SHIFT	1
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_FORCE_LB_MASK		0x1	/* HSI_COMMENT: Used to indicate loopback */
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_FORCE_LB_SHIFT		2
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_RESERVED_MASK		0x1F
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_RESERVED_SHIFT		3
	u8 name_space;
	u8 reserved3[3];
	__le16 regular_latency_phy_queue;
	__le16 dpi;
};

/* roce create qp responder ramrod data */
struct roce_create_qp_resp_ramrod_data {
	__le32 flags;
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_ROCE_FLAVOR_MASK		0x3	/* HSI_COMMENT: Use roce_flavor enum */
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_ROCE_FLAVOR_SHIFT		0
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_RDMA_RD_EN_MASK			0x1
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_RDMA_RD_EN_SHIFT		2
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_RDMA_WR_EN_MASK			0x1
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_RDMA_WR_EN_SHIFT		3
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_ATOMIC_EN_MASK			0x1
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_ATOMIC_EN_SHIFT			4
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_SRQ_FLG_MASK			0x1
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_SRQ_FLG_SHIFT			5
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_E2E_FLOW_CONTROL_EN_MASK	0x1
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_E2E_FLOW_CONTROL_EN_SHIFT	6
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_RESERVED_KEY_EN_MASK		0x1
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_RESERVED_KEY_EN_SHIFT		7
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_PRI_MASK			0x7
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_PRI_SHIFT			8
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_MIN_RNR_NAK_TIMER_MASK		0x1F
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_MIN_RNR_NAK_TIMER_SHIFT		11
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_XRC_FLAG_MASK			0x1
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_XRC_FLAG_SHIFT			16
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_VF_ID_VALID_MASK		0x1
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_VF_ID_VALID_SHIFT		17
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_FORCE_LB_MASK			0x1	/* HSI_COMMENT: Used to indicate loopback */
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_FORCE_LB_SHIFT			18
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_RESERVED_MASK			0x1FFF
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_RESERVED_SHIFT			19
	__le16 xrc_domain;	/* HSI_COMMENT: SRC domain. Only applicable when xrc_flag is set */
	u8 max_ird;
	u8 traffic_class;	/* HSI_COMMENT: In case of RRoCE on IPv4 will be used as TOS */
	u8 hop_limit;		/* HSI_COMMENT: In case of RRoCE on IPv4 will be used as TTL */
	u8 irq_num_pages;
	__le16 p_key;
	__le32 flow_label;
	__le32 dst_qp_id;
	u8 stats_counter_id;	/* HSI_COMMENT: Statistics counter ID to use */
	u8 reserved1;
	__le16 mtu;
	__le32 initial_psn;
	__le16 pd;
	__le16 rq_num_pages;
	struct rdma_srq_id srq_id;
	struct regpair rq_pbl_addr;
	struct regpair irq_pbl_addr;
	__le16 local_mac_addr[3];	/* HSI_COMMENT: BE order */
	__le16 remote_mac_addr[3];	/* HSI_COMMENT: BE order */
	__le16 vlan_id;
	__le16 udp_src_port;	/* HSI_COMMENT: Only relevant in RRoCE */
	__le32 src_gid[4];	/* HSI_COMMENT: BE order. In case of RRoCE on IPv4 the lower register will hold the address. High registers must be zero! */
	__le32 dst_gid[4];	/* HSI_COMMENT: BE order. In case of RRoCE on IPv4 the lower register will hold the address. High registers must be zero! */
	struct regpair qp_handle_for_cqe;
	struct regpair qp_handle_for_async;
	__le16 low_latency_phy_queue;
	u8 vf_id;
	u8 vport_id;
	__le32 cq_cid;
	__le16 regular_latency_phy_queue;
	__le16 dpi;
	__le32 src_qp_id;
	u8 name_space;
	u8 reserved3[3];
};

/* RoCE Create Suspended qp requester runtime ramrod data */
struct roce_create_suspended_qp_req_runtime_ramrod_data {
	__le32 flags;
#define ROCE_CREATE_SUSPENDED_QP_REQ_RUNTIME_RAMROD_DATA_ERR_FLG_MASK \
	        0x1
#define ROCE_CREATE_SUSPENDED_QP_REQ_RUNTIME_RAMROD_DATA_ERR_FLG_SHIFT \
	        0
#define ROCE_CREATE_SUSPENDED_QP_REQ_RUNTIME_RAMROD_DATA_RESERVED0_MASK \
	        0x7FFFFFFF
#define ROCE_CREATE_SUSPENDED_QP_REQ_RUNTIME_RAMROD_DATA_RESERVED0_SHIFT \
	        1
	__le32 send_msg_psn;	/* HSI_COMMENT: PSN of the beginning of current executed SQ WQE */
	__le32 inflight_sends;	/* HSI_COMMENT: Number of inflight sends (uncompleted already sent WQEs) */
	__le32 ssn;		/* HSI_COMMENT: current send sequence number */
};

/* RoCE Create Suspended QP requester ramrod data */
struct roce_create_suspended_qp_req_ramrod_data {
	struct roce_create_qp_req_ramrod_data qp_params;
	struct roce_create_suspended_qp_req_runtime_ramrod_data
	 qp_runtime_params;
};

/* RoCE Create Suspended QP responder runtime params */
struct roce_create_suspended_qp_resp_runtime_params {
	__le32 flags;
#define ROCE_CREATE_SUSPENDED_QP_RESP_RUNTIME_PARAMS_ERR_FLG_MASK \
	        0x1
#define ROCE_CREATE_SUSPENDED_QP_RESP_RUNTIME_PARAMS_ERR_FLG_SHIFT \
	        0
#define ROCE_CREATE_SUSPENDED_QP_RESP_RUNTIME_PARAMS_RDMA_ACTIVE_MASK \
	        0x1
#define ROCE_CREATE_SUSPENDED_QP_RESP_RUNTIME_PARAMS_RDMA_ACTIVE_SHIFT \
	        1
#define ROCE_CREATE_SUSPENDED_QP_RESP_RUNTIME_PARAMS_RESERVED0_MASK \
	        0x3FFFFFFF
#define ROCE_CREATE_SUSPENDED_QP_RESP_RUNTIME_PARAMS_RESERVED0_SHIFT \
	        2
	__le32 receive_msg_psn;	/* HSI_COMMENT: PSN of the beginning of current executed RQ WQE */
	__le32 inflight_receives;	/* HSI_COMMENT: Number of RQ WQEs */
	__le32 rmsn;		/* HSI_COMMENT: next expected MSN */
	__le32 rdma_key;	/* HSI_COMMENT: key of current RDMA WRITE. Valid when rdma_active is set */
	struct regpair rdma_va;	/* HSI_COMMENT: VA of current RDMA WRITE. Valid when rdma_active is set */
	__le32 rdma_length;	/* HSI_COMMENT: Total length of current RDMA WRITE. Valid when rdma_active is set */
	__le32 num_rdb_entries;	/* HSI_COMMENT: Number of read requests on the IRQ */
	__le32 resreved;
};

/* RoCE RDB array entry */
struct roce_resp_qp_rdb_entry {
	struct regpair atomic_data;	/* HSI_COMMENT: Atomic response data */
	struct regpair va;	/* HSI_COMMENT: VA for read response */
	__le32 psn;		/* HSI_COMMENT: PSN of original read request */
	__le32 rkey;		/* HSI_COMMENT: Rkey of the original request */
	__le32 byte_count;	/* HSI_COMMENT: Read response size */
	u8 op_type;		/* HSI_COMMENT: RDB entry type (use enum roce_resp_qp_rdb_entry_type) */
	u8 reserved[3];
};

/* RoCE Create Suspended QP responder runtime ramrod data */
struct roce_create_suspended_qp_resp_runtime_ramrod_data {
	struct roce_create_suspended_qp_resp_runtime_params params;
	struct roce_resp_qp_rdb_entry
	 rdb_array_entries[RDMA_MAX_IRQ_ELEMS_IN_PAGE];	/* HSI_COMMENT: RDB array */
};

/* RoCE Create Suspended QP responder ramrod data */
struct roce_create_suspended_qp_resp_ramrod_data {
	struct roce_create_qp_resp_ramrod_data
	 qp_params;
	struct roce_create_suspended_qp_resp_runtime_ramrod_data
	 qp_runtime_params;
};

/* RoCE create ud qp ramrod data */
struct roce_create_ud_qp_ramrod_data {
	__le16 local_mac_addr[3];	/* HSI_COMMENT: BE order */
	__le16 vlan_id;
	__le32 src_qp_id;
	u8 name_space;
	u8 reserved[3];
};

/* roce DCQCN received statistics */
struct roce_dcqcn_received_stats {
	struct regpair ecn_pkt_rcv;	/* HSI_COMMENT: The number of total packets with ECN indication received */
	struct regpair cnp_pkt_rcv;	/* HSI_COMMENT: The number of total RoCE packets with CNP opcode received */
	struct regpair cnp_pkt_reject;	/* HSI_COMMENT: The number of total RoCE packets with CNP opcode reject since RP doesnt support ECN.  */
};

/* roce DCQCN sent statistics */
struct roce_dcqcn_sent_stats {
	struct regpair cnp_pkt_sent;	/* HSI_COMMENT: The number of total RoCE packets with CNP opcode sent */
};

/* RoCE destroy qp requester output params */
struct roce_destroy_qp_req_output_params {
	__le32 cq_prod;		/* HSI_COMMENT: Completion producer value at destroy QP */
	__le32 resrved;
};

/* RoCE destroy qp requester ramrod data */
struct roce_destroy_qp_req_ramrod_data {
	struct regpair output_params_addr;
};

/* RoCE destroy qp responder output params */
struct roce_destroy_qp_resp_output_params {
	__le32 cq_prod;		/* HSI_COMMENT: Completion producer value at destroy QP */
	__le32 reserved;
};

/* RoCE destroy qp responder ramrod data */
struct roce_destroy_qp_resp_ramrod_data {
	struct regpair output_params_addr;
	__le32 src_qp_id;
	__le32 reserved;
};

/* RoCE destroy ud qp ramrod data */
struct roce_destroy_ud_qp_ramrod_data {
	__le32 src_qp_id;
	__le32 reserved;
};

/* roce error statistics */
struct roce_error_stats {
	__le32 resp_remote_access_errors;
	__le32 reserved;
};

/* roce special events statistics */
struct roce_events_stats {
	__le32 silent_drops;
	__le32 rnr_naks_sent;
	__le32 retransmit_count;
	__le32 icrc_error_count;
	__le32 implied_nak_seq_err;
	__le32 duplicate_request;
	__le32 local_ack_timeout_err;
	__le32 out_of_sequence;
	__le32 packet_seq_err;
	__le32 rnr_nak_retry_err;
};

/* ROCE slow path EQ cmd IDs */
enum roce_event_opcode {
	ROCE_EVENT_CREATE_QP = 16,
	ROCE_EVENT_MODIFY_QP,
	ROCE_EVENT_QUERY_QP,
	ROCE_EVENT_DESTROY_QP,
	ROCE_EVENT_CREATE_UD_QP,
	ROCE_EVENT_DESTROY_UD_QP,
	ROCE_EVENT_FUNC_UPDATE,
	ROCE_EVENT_SUSPEND_QP,
	ROCE_EVENT_QUERY_SUSPENDED_QP,
	ROCE_EVENT_CREATE_SUSPENDED_QP,
	ROCE_EVENT_RESUME_QP,
	ROCE_EVENT_SUSPEND_UD_QP,
	ROCE_EVENT_RESUME_UD_QP,
	ROCE_EVENT_CREATE_SUSPENDED_UD_QP,
	ROCE_EVENT_FLUSH_DPT_QP,
	MAX_ROCE_EVENT_OPCODE
};

/* roce func init ramrod data */
struct roce_init_func_params {
	u8 ll2_queue_id;	/* HSI_COMMENT: This ll2 queue ID is used for Unreliable Datagram QP */
	u8 cnp_vlan_priority;	/* HSI_COMMENT: VLAN priority of DCQCN CNP packet */
	u8 cnp_dscp;		/* HSI_COMMENT: The value of DSCP field in IP header for CNP packets */
	u8 flags;
#define ROCE_INIT_FUNC_PARAMS_DCQCN_NP_EN_MASK		0x1
#define ROCE_INIT_FUNC_PARAMS_DCQCN_NP_EN_SHIFT		0
#define ROCE_INIT_FUNC_PARAMS_DCQCN_RP_EN_MASK		0x1
#define ROCE_INIT_FUNC_PARAMS_DCQCN_RP_EN_SHIFT		1
#define ROCE_INIT_FUNC_PARAMS_RESERVED0_MASK		0x3F
#define ROCE_INIT_FUNC_PARAMS_RESERVED0_SHIFT		2
	__le32 cnp_send_timeout;	/* HSI_COMMENT: The minimal difference of send time between CNP packets for specific QP. Units are in microseconds */
	__le16 rl_offset;	/* HSI_COMMENT: this pf rate limiters starting offset */
	u8 rl_count_log;	/* HSI_COMMENT: log2 of the number of rate limiters allocated to this pf */
	u8 reserved1[5];
};

/* roce func init ramrod data */
struct roce_init_func_ramrod_data {
	struct rdma_init_func_ramrod_data rdma;
	struct roce_init_func_params roce;
};

/* roce_ll2_cqe_data */
struct roce_ll2_cqe_data {
	u8 name_space;
	u8 flags;
#define ROCE_LL2_CQE_DATA_QP_SUSPENDED_MASK	0x1
#define ROCE_LL2_CQE_DATA_QP_SUSPENDED_SHIFT	0
#define ROCE_LL2_CQE_DATA_RESERVED0_MASK	0x7F
#define ROCE_LL2_CQE_DATA_RESERVED0_SHIFT	1
	u8 reserved1[2];
	__le32 cid;
};

/* roce modify qp requester ramrod data */
struct roce_modify_qp_req_ramrod_data {
	__le16 flags;
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_MOVE_TO_ERR_FLG_MASK		0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_MOVE_TO_ERR_FLG_SHIFT		0
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_MOVE_TO_SQD_FLG_MASK		0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_MOVE_TO_SQD_FLG_SHIFT		1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_EN_SQD_ASYNC_NOTIFY_MASK		0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_EN_SQD_ASYNC_NOTIFY_SHIFT	2
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_P_KEY_FLG_MASK			0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_P_KEY_FLG_SHIFT			3
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_ADDRESS_VECTOR_FLG_MASK		0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_ADDRESS_VECTOR_FLG_SHIFT		4
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_MAX_ORD_FLG_MASK			0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_MAX_ORD_FLG_SHIFT		5
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_RNR_NAK_CNT_FLG_MASK		0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_RNR_NAK_CNT_FLG_SHIFT		6
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_ERR_RETRY_CNT_FLG_MASK		0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_ERR_RETRY_CNT_FLG_SHIFT		7
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_ACK_TIMEOUT_FLG_MASK		0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_ACK_TIMEOUT_FLG_SHIFT		8
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_PRI_FLG_MASK			0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_PRI_FLG_SHIFT			9
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_PRI_MASK				0x7
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_PRI_SHIFT			10
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_PHYSICAL_QUEUE_FLG_MASK		0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_PHYSICAL_QUEUE_FLG_SHIFT		13
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_FORCE_LB_MASK			0x1	/* HSI_COMMENT: Used to indicate loopback */
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_FORCE_LB_SHIFT			14
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_RESERVED1_MASK			0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_RESERVED1_SHIFT			15
	u8 fields;
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_ERR_RETRY_CNT_MASK		0xF
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_ERR_RETRY_CNT_SHIFT		0
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_RNR_NAK_CNT_MASK			0xF
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_RNR_NAK_CNT_SHIFT		4
	u8 max_ord;
	u8 traffic_class;
	u8 hop_limit;
	__le16 p_key;
	__le32 flow_label;
	__le32 ack_timeout_val;
	__le16 mtu;
	__le16 reserved2;
	__le32 reserved3[2];
	__le16 low_latency_phy_queue;
	__le16 regular_latency_phy_queue;
	__le32 src_gid[4];	/* HSI_COMMENT: BE order. In case of IPv4 the higher register will hold the address. Low registers must be zero! */
	__le32 dst_gid[4];	/* HSI_COMMENT: BE order. In case of IPv4 the higher register will hold the address. Low registers must be zero! */
};

/* roce modify qp responder ramrod data */
struct roce_modify_qp_resp_ramrod_data {
	__le16 flags;
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_MOVE_TO_ERR_FLG_MASK		0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_MOVE_TO_ERR_FLG_SHIFT		0
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_RDMA_RD_EN_MASK			0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_RDMA_RD_EN_SHIFT		1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_RDMA_WR_EN_MASK			0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_RDMA_WR_EN_SHIFT		2
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_ATOMIC_EN_MASK			0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_ATOMIC_EN_SHIFT			3
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_P_KEY_FLG_MASK			0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_P_KEY_FLG_SHIFT			4
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_ADDRESS_VECTOR_FLG_MASK		0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_ADDRESS_VECTOR_FLG_SHIFT	5
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_MAX_IRD_FLG_MASK		0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_MAX_IRD_FLG_SHIFT		6
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_PRI_FLG_MASK			0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_PRI_FLG_SHIFT			7
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_MIN_RNR_NAK_TIMER_FLG_MASK	0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_MIN_RNR_NAK_TIMER_FLG_SHIFT	8
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_RDMA_OPS_EN_FLG_MASK		0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_RDMA_OPS_EN_FLG_SHIFT		9
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_PHYSICAL_QUEUE_FLG_MASK		0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_PHYSICAL_QUEUE_FLG_SHIFT	10
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_FORCE_LB_MASK			0x1	/* HSI_COMMENT: Used to indicate loopback */
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_FORCE_LB_SHIFT			11
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_RESERVED1_MASK			0xF
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_RESERVED1_SHIFT			12
	u8 fields;
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_PRI_MASK			0x7
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_PRI_SHIFT			0
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_MIN_RNR_NAK_TIMER_MASK		0x1F
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_MIN_RNR_NAK_TIMER_SHIFT		3
	u8 max_ird;
	u8 traffic_class;
	u8 hop_limit;
	__le16 p_key;
	__le32 flow_label;
	__le16 mtu;
	__le16 low_latency_phy_queue;
	__le16 regular_latency_phy_queue;
	u8 reserved2[6];
	__le32 src_gid[4];	/* HSI_COMMENT: BE order. In case of IPv4 the higher register will hold the address. Low registers must be zero! */
	__le32 dst_gid[4];	/* HSI_COMMENT: BE order. In case of IPv4 the higher register will hold the address. Low registers must be zero! */
};

/* RoCE query qp requester output params */
struct roce_query_qp_req_output_params {
	__le32 psn;		/* HSI_COMMENT: send next psn */
	__le32 flags;
#define ROCE_QUERY_QP_REQ_OUTPUT_PARAMS_ERR_FLG_MASK			0x1
#define ROCE_QUERY_QP_REQ_OUTPUT_PARAMS_ERR_FLG_SHIFT			0
#define ROCE_QUERY_QP_REQ_OUTPUT_PARAMS_SQ_DRAINING_FLG_MASK		0x1
#define ROCE_QUERY_QP_REQ_OUTPUT_PARAMS_SQ_DRAINING_FLG_SHIFT		1
#define ROCE_QUERY_QP_REQ_OUTPUT_PARAMS_RESERVED0_MASK \
	                                                                0x3FFFFFFF
#define ROCE_QUERY_QP_REQ_OUTPUT_PARAMS_RESERVED0_SHIFT			2
};

/* RoCE query qp requester ramrod data */
struct roce_query_qp_req_ramrod_data {
	struct regpair output_params_addr;
};

/* RoCE query qp responder output params */
struct roce_query_qp_resp_output_params {
	__le32 psn;		/* HSI_COMMENT: send next psn */
	__le32 flags;
#define ROCE_QUERY_QP_RESP_OUTPUT_PARAMS_ERROR_FLG_MASK		0x1
#define ROCE_QUERY_QP_RESP_OUTPUT_PARAMS_ERROR_FLG_SHIFT	0
#define ROCE_QUERY_QP_RESP_OUTPUT_PARAMS_RESERVED0_MASK		0x7FFFFFFF
#define ROCE_QUERY_QP_RESP_OUTPUT_PARAMS_RESERVED0_SHIFT	1
};

/* RoCE query qp responder ramrod data */
struct roce_query_qp_resp_ramrod_data {
	struct regpair output_params_addr;
};

/* RoCE Query Suspended QP requester output params */
struct roce_query_suspended_qp_req_output_params {
	__le32 psn;		/* HSI_COMMENT: send next psn */
	__le32 flags;
#define ROCE_QUERY_SUSPENDED_QP_REQ_OUTPUT_PARAMS_ERR_FLG_MASK		0x1
#define ROCE_QUERY_SUSPENDED_QP_REQ_OUTPUT_PARAMS_ERR_FLG_SHIFT		0
#define ROCE_QUERY_SUSPENDED_QP_REQ_OUTPUT_PARAMS_RESERVED0_MASK \
	                                                                0x7FFFFFFF
#define ROCE_QUERY_SUSPENDED_QP_REQ_OUTPUT_PARAMS_RESERVED0_SHIFT	1
	__le32 send_msg_psn;	/* HSI_COMMENT: PSN of the beginning of current executed SQ WQE */
	__le32 inflight_sends;	/* HSI_COMMENT: Number of inflight sends (uncompleted already sent WQEs) */
	__le32 ssn;		/* HSI_COMMENT: current send sequence number */
	__le32 reserved;
};

/* RoCE Query Suspended QP requester ramrod data */
struct roce_query_suspended_qp_req_ramrod_data {
	struct regpair output_params_addr;
};

/* RoCE Query Suspended QP responder runtime params */
struct roce_query_suspended_qp_resp_runtime_params {
	__le32 psn;		/* HSI_COMMENT: send next psn */
	__le32 flags;
#define ROCE_QUERY_SUSPENDED_QP_RESP_RUNTIME_PARAMS_ERR_FLG_MASK \
	        0x1
#define ROCE_QUERY_SUSPENDED_QP_RESP_RUNTIME_PARAMS_ERR_FLG_SHIFT \
	        0
#define ROCE_QUERY_SUSPENDED_QP_RESP_RUNTIME_PARAMS_RDMA_ACTIVE_MASK \
	        0x1
#define ROCE_QUERY_SUSPENDED_QP_RESP_RUNTIME_PARAMS_RDMA_ACTIVE_SHIFT \
	        1
#define ROCE_QUERY_SUSPENDED_QP_RESP_RUNTIME_PARAMS_RESERVED0_MASK \
	        0x3FFFFFFF
#define ROCE_QUERY_SUSPENDED_QP_RESP_RUNTIME_PARAMS_RESERVED0_SHIFT \
	        2
	__le32 receive_msg_psn;	/* HSI_COMMENT: PSN of the beginning of current executed RQ WQE */
	__le32 inflight_receives;	/* HSI_COMMENT: Number of RQ WQEs */
	__le32 rmsn;		/* HSI_COMMENT: next expected MSN */
	__le32 rdma_key;	/* HSI_COMMENT: key of current RDMA WRITE. Valid when rdma_active is set */
	struct regpair rdma_va;	/* HSI_COMMENT: VA of current RDMA WRITE. Valid when rdma_active is set */
	__le32 rdma_length;	/* HSI_COMMENT: Total length of current RDMA WRITE. Valid when rdma_active is set */
	__le32 num_rdb_entries;	/* HSI_COMMENT: Number of read requests on the IRQ */
};

/* RoCE Query Suspended QP responder output params */
struct roce_query_suspended_qp_resp_output_params {
	struct roce_query_suspended_qp_resp_runtime_params runtime_params;	/* HSI_COMMENT: Query suspended responder runtime params */
	struct roce_resp_qp_rdb_entry
	 rdb_array_entries[RDMA_MAX_IRQ_ELEMS_IN_PAGE];	/* HSI_COMMENT:  RDB array. TODO: need to replace 32 with hsi def. */
};

/* RoCE Query Suspended QP responder ramrod data */
struct roce_query_suspended_qp_resp_ramrod_data {
	struct regpair output_params_addr;
};

/* ROCE ramrod command IDs */
enum roce_ramrod_cmd_id {
	ROCE_RAMROD_CREATE_QP = 16,
	ROCE_RAMROD_MODIFY_QP,
	ROCE_RAMROD_QUERY_QP,
	ROCE_RAMROD_DESTROY_QP,
	ROCE_RAMROD_CREATE_UD_QP,
	ROCE_RAMROD_DESTROY_UD_QP,
	ROCE_RAMROD_FUNC_UPDATE,
	ROCE_RAMROD_SUSPEND_QP,
	ROCE_RAMROD_QUERY_SUSPENDED_QP,
	ROCE_RAMROD_CREATE_SUSPENDED_QP,
	ROCE_RAMROD_RESUME_QP,
	ROCE_RAMROD_SUSPEND_UD_QP,
	ROCE_RAMROD_RESUME_UD_QP,
	ROCE_RAMROD_CREATE_SUSPENDED_UD_QP,
	ROCE_RAMROD_FLUSH_DPT_QP,
	MAX_ROCE_RAMROD_CMD_ID
};

/* ROCE RDB array entry type */
enum roce_resp_qp_rdb_entry_type {
	ROCE_QP_RDB_ENTRY_RDMA_RESPONSE = 0,
	ROCE_QP_RDB_ENTRY_ATOMIC_RESPONSE = 1,
	ROCE_QP_RDB_ENTRY_INVALID = 2,
	MAX_ROCE_RESP_QP_RDB_ENTRY_TYPE
};

/* roce func init ramrod data */
struct roce_update_func_params {
	u8 cnp_vlan_priority;	/* HSI_COMMENT: VLAN priority of DCQCN CNP packet */
	u8 cnp_dscp;		/* HSI_COMMENT: The value of DSCP field in IP header for CNP packets */
	__le16 flags;
#define ROCE_UPDATE_FUNC_PARAMS_DCQCN_NP_EN_MASK	0x1
#define ROCE_UPDATE_FUNC_PARAMS_DCQCN_NP_EN_SHIFT	0
#define ROCE_UPDATE_FUNC_PARAMS_DCQCN_RP_EN_MASK	0x1
#define ROCE_UPDATE_FUNC_PARAMS_DCQCN_RP_EN_SHIFT	1
#define ROCE_UPDATE_FUNC_PARAMS_RESERVED0_MASK		0x3FFF
#define ROCE_UPDATE_FUNC_PARAMS_RESERVED0_SHIFT		2
	__le32 cnp_send_timeout;	/* HSI_COMMENT: The minimal difference of send time between CNP packets for specific QP. Units are in microseconds */
};

struct E4XstormRoceConnAgCtxDqExtLdPart {
	u8 reserved0;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_EXIST_IN_QM0_SHIFT		0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT1_SHIFT			1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT2_MASK			0x1	/* HSI_COMMENT: exist_in_qm2 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT2_SHIFT			2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_EXIST_IN_QM3_MASK		0x1	/* HSI_COMMENT: exist_in_qm3 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_EXIST_IN_QM3_SHIFT		3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT4_MASK			0x1	/* HSI_COMMENT: bit4 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT4_SHIFT			4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT5_MASK			0x1	/* HSI_COMMENT: cf_array_active */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT5_SHIFT			5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT6_MASK			0x1	/* HSI_COMMENT: bit6 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT6_SHIFT			6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT7_MASK			0x1	/* HSI_COMMENT: bit7 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT7_SHIFT			7
	u8 flags1;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT8_MASK			0x1	/* HSI_COMMENT: bit8 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT8_SHIFT			0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT9_MASK			0x1	/* HSI_COMMENT: bit9 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT9_SHIFT			1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT10_MASK			0x1	/* HSI_COMMENT: bit10 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT10_SHIFT			2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT11_MASK			0x1	/* HSI_COMMENT: bit11 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT11_SHIFT			3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_MSDM_FLUSH_MASK		0x1	/* HSI_COMMENT: bit12 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_MSDM_FLUSH_SHIFT		4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_MSEM_FLUSH_MASK		0x1	/* HSI_COMMENT: bit13 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_MSEM_FLUSH_SHIFT		5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT14_MASK			0x1	/* HSI_COMMENT: bit14 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT14_SHIFT			6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_YSTORM_FLUSH_MASK		0x1	/* HSI_COMMENT: bit15 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_YSTORM_FLUSH_SHIFT		7
	u8 flags2;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF0_MASK			0x3	/* HSI_COMMENT: timer0cf */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF0_SHIFT			0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF1_MASK			0x3	/* HSI_COMMENT: timer1cf */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF1_SHIFT			2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF2_MASK			0x3	/* HSI_COMMENT: timer2cf */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF2_SHIFT			4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF3_MASK			0x3	/* HSI_COMMENT: timer_stop_all */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF3_SHIFT			6
	u8 flags3;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF4_MASK			0x3	/* HSI_COMMENT: cf4 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF4_SHIFT			0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF5_MASK			0x3	/* HSI_COMMENT: cf5 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF5_SHIFT			2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF6_MASK			0x3	/* HSI_COMMENT: cf6 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF6_SHIFT			4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_FLUSH_Q0_CF_MASK		0x3	/* HSI_COMMENT: cf7 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_FLUSH_Q0_CF_SHIFT		6
	u8 flags4;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF8_MASK			0x3	/* HSI_COMMENT: cf8 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF8_SHIFT			0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF9_MASK			0x3	/* HSI_COMMENT: cf9 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF9_SHIFT			2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF10_MASK			0x3	/* HSI_COMMENT: cf10 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF10_SHIFT			4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF11_MASK			0x3	/* HSI_COMMENT: cf11 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF11_SHIFT			6
	u8 flags5;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF12_MASK			0x3	/* HSI_COMMENT: cf12 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF12_SHIFT			0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF13_MASK			0x3	/* HSI_COMMENT: cf13 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF13_SHIFT			2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF14_MASK			0x3	/* HSI_COMMENT: cf14 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF14_SHIFT			4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF15_MASK			0x3	/* HSI_COMMENT: cf15 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF15_SHIFT			6
	u8 flags6;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF16_MASK			0x3	/* HSI_COMMENT: cf16 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF16_SHIFT			0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF17_MASK			0x3	/* HSI_COMMENT: cf_array_cf */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF17_SHIFT			2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF18_MASK			0x3	/* HSI_COMMENT: cf18 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF18_SHIFT			4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF19_MASK			0x3	/* HSI_COMMENT: cf19 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF19_SHIFT			6
	u8 flags7;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF20_MASK			0x3	/* HSI_COMMENT: cf20 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF20_SHIFT			0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF21_MASK			0x3	/* HSI_COMMENT: cf21 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF21_SHIFT			2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_SLOW_PATH_MASK			0x3	/* HSI_COMMENT: cf22 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_SLOW_PATH_SHIFT		4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF0EN_MASK			0x1	/* HSI_COMMENT: cf0en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF0EN_SHIFT			6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF1EN_MASK			0x1	/* HSI_COMMENT: cf1en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF1EN_SHIFT			7
	u8 flags8;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF2EN_SHIFT			0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF3EN_MASK			0x1	/* HSI_COMMENT: cf3en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF3EN_SHIFT			1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF4EN_MASK			0x1	/* HSI_COMMENT: cf4en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF4EN_SHIFT			2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF5EN_MASK			0x1	/* HSI_COMMENT: cf5en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF5EN_SHIFT			3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF6EN_MASK			0x1	/* HSI_COMMENT: cf6en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF6EN_SHIFT			4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_FLUSH_Q0_CF_EN_MASK		0x1	/* HSI_COMMENT: cf7en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_FLUSH_Q0_CF_EN_SHIFT		5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF8EN_MASK			0x1	/* HSI_COMMENT: cf8en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF8EN_SHIFT			6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF9EN_MASK			0x1	/* HSI_COMMENT: cf9en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF9EN_SHIFT			7
	u8 flags9;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF10EN_MASK			0x1	/* HSI_COMMENT: cf10en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF10EN_SHIFT			0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF11EN_MASK			0x1	/* HSI_COMMENT: cf11en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF11EN_SHIFT			1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF12EN_MASK			0x1	/* HSI_COMMENT: cf12en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF12EN_SHIFT			2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF13EN_MASK			0x1	/* HSI_COMMENT: cf13en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF13EN_SHIFT			3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF14EN_MASK			0x1	/* HSI_COMMENT: cf14en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF14EN_SHIFT			4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF15EN_MASK			0x1	/* HSI_COMMENT: cf15en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF15EN_SHIFT			5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF16EN_MASK			0x1	/* HSI_COMMENT: cf16en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF16EN_SHIFT			6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF17EN_MASK			0x1	/* HSI_COMMENT: cf_array_cf_en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF17EN_SHIFT			7
	u8 flags10;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF18EN_MASK			0x1	/* HSI_COMMENT: cf18en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF18EN_SHIFT			0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF19EN_MASK			0x1	/* HSI_COMMENT: cf19en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF19EN_SHIFT			1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF20EN_MASK			0x1	/* HSI_COMMENT: cf20en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF20EN_SHIFT			2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF21EN_MASK			0x1	/* HSI_COMMENT: cf21en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF21EN_SHIFT			3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_SLOW_PATH_EN_MASK		0x1	/* HSI_COMMENT: cf22en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_SLOW_PATH_EN_SHIFT		4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF23EN_MASK			0x1	/* HSI_COMMENT: cf23en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF23EN_SHIFT			5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE0EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE0EN_SHIFT			6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE1EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE1EN_SHIFT			7
	u8 flags11;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE2EN_SHIFT			0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE3EN_SHIFT			1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE4EN_SHIFT			2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE5EN_SHIFT			3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE6EN_SHIFT			4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE7EN_MASK			0x1	/* HSI_COMMENT: rule7en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE7EN_SHIFT			5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED1_MASK		0x1	/* HSI_COMMENT: rule8en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED1_SHIFT		6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE9EN_MASK			0x1	/* HSI_COMMENT: rule9en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE9EN_SHIFT			7
	u8 flags12;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE10EN_MASK			0x1	/* HSI_COMMENT: rule10en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE10EN_SHIFT			0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE11EN_MASK			0x1	/* HSI_COMMENT: rule11en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE11EN_SHIFT			1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED2_MASK		0x1	/* HSI_COMMENT: rule12en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED2_SHIFT		2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED3_MASK		0x1	/* HSI_COMMENT: rule13en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED3_SHIFT		3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE14EN_MASK			0x1	/* HSI_COMMENT: rule14en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE14EN_SHIFT			4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE15EN_MASK			0x1	/* HSI_COMMENT: rule15en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE15EN_SHIFT			5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE16EN_MASK			0x1	/* HSI_COMMENT: rule16en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE16EN_SHIFT			6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE17EN_MASK			0x1	/* HSI_COMMENT: rule17en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE17EN_SHIFT			7
	u8 flags13;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE18EN_MASK			0x1	/* HSI_COMMENT: rule18en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE18EN_SHIFT			0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE19EN_MASK			0x1	/* HSI_COMMENT: rule19en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE19EN_SHIFT			1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED4_MASK		0x1	/* HSI_COMMENT: rule20en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED4_SHIFT		2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED5_MASK		0x1	/* HSI_COMMENT: rule21en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED5_SHIFT		3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED6_MASK		0x1	/* HSI_COMMENT: rule22en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED6_SHIFT		4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED7_MASK		0x1	/* HSI_COMMENT: rule23en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED7_SHIFT		5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED8_MASK		0x1	/* HSI_COMMENT: rule24en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED8_SHIFT		6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED9_MASK		0x1	/* HSI_COMMENT: rule25en */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED9_SHIFT		7
	u8 flags14;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_MIGRATION_MASK			0x1	/* HSI_COMMENT: bit16 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_MIGRATION_SHIFT		0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT17_MASK			0x1	/* HSI_COMMENT: bit17 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT17_SHIFT			1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_DPM_PORT_NUM_MASK		0x3	/* HSI_COMMENT: bit18 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_DPM_PORT_NUM_SHIFT		2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RESERVED_MASK			0x1	/* HSI_COMMENT: bit20 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RESERVED_SHIFT			4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_ROCE_EDPM_ENABLE_MASK		0x1	/* HSI_COMMENT: bit21 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_ROCE_EDPM_ENABLE_SHIFT		5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF23_MASK			0x3	/* HSI_COMMENT: cf23 */
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF23_SHIFT			6
	u8 byte2;		/* HSI_COMMENT: byte2 */
	__le16 physical_q0;	/* HSI_COMMENT: physical_q0 */
	__le16 word1;		/* HSI_COMMENT: physical_q1 */
	__le16 word2;		/* HSI_COMMENT: physical_q2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le16 word4;		/* HSI_COMMENT: word4 */
	__le16 word5;		/* HSI_COMMENT: word5 */
	__le16 conn_dpi;	/* HSI_COMMENT: conn_dpi */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	u8 byte5;		/* HSI_COMMENT: byte5 */
	u8 byte6;		/* HSI_COMMENT: byte6 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 snd_nxt_psn;	/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
};

struct mstorm_roce_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define MSTORM_ROCE_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define MSTORM_ROCE_CONN_AG_CTX_BIT0_SHIFT		0
#define MSTORM_ROCE_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define MSTORM_ROCE_CONN_AG_CTX_BIT1_SHIFT		1
#define MSTORM_ROCE_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: cf0 */
#define MSTORM_ROCE_CONN_AG_CTX_CF0_SHIFT		2
#define MSTORM_ROCE_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: cf1 */
#define MSTORM_ROCE_CONN_AG_CTX_CF1_SHIFT		4
#define MSTORM_ROCE_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: cf2 */
#define MSTORM_ROCE_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define MSTORM_ROCE_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define MSTORM_ROCE_CONN_AG_CTX_CF0EN_SHIFT		0
#define MSTORM_ROCE_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define MSTORM_ROCE_CONN_AG_CTX_CF1EN_SHIFT		1
#define MSTORM_ROCE_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define MSTORM_ROCE_CONN_AG_CTX_CF2EN_SHIFT		2
#define MSTORM_ROCE_CONN_AG_CTX_RULE0EN_MASK		0x1	/* HSI_COMMENT: rule0en */
#define MSTORM_ROCE_CONN_AG_CTX_RULE0EN_SHIFT		3
#define MSTORM_ROCE_CONN_AG_CTX_RULE1EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define MSTORM_ROCE_CONN_AG_CTX_RULE1EN_SHIFT		4
#define MSTORM_ROCE_CONN_AG_CTX_RULE2EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define MSTORM_ROCE_CONN_AG_CTX_RULE2EN_SHIFT		5
#define MSTORM_ROCE_CONN_AG_CTX_RULE3EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define MSTORM_ROCE_CONN_AG_CTX_RULE3EN_SHIFT		6
#define MSTORM_ROCE_CONN_AG_CTX_RULE4EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define MSTORM_ROCE_CONN_AG_CTX_RULE4EN_SHIFT		7
	__le16 word0;		/* HSI_COMMENT: word0 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
};

struct mstorm_roce_req_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define MSTORM_ROCE_REQ_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define MSTORM_ROCE_REQ_CONN_AG_CTX_BIT0_SHIFT		0
#define MSTORM_ROCE_REQ_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define MSTORM_ROCE_REQ_CONN_AG_CTX_BIT1_SHIFT		1
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: cf0 */
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF0_SHIFT		2
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: cf1 */
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF1_SHIFT		4
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: cf2 */
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF0EN_SHIFT		0
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF1EN_SHIFT		1
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF2EN_SHIFT		2
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_MASK	0x1	/* HSI_COMMENT: rule0en */
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_SHIFT	3
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_MASK	0x1	/* HSI_COMMENT: rule1en */
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_SHIFT	4
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE2EN_MASK	0x1	/* HSI_COMMENT: rule2en */
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE2EN_SHIFT	5
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_MASK	0x1	/* HSI_COMMENT: rule3en */
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_SHIFT	6
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_MASK	0x1	/* HSI_COMMENT: rule4en */
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_SHIFT	7
	__le16 word0;		/* HSI_COMMENT: word0 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
};

struct mstorm_roce_resp_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define MSTORM_ROCE_RESP_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define MSTORM_ROCE_RESP_CONN_AG_CTX_BIT0_SHIFT		0
#define MSTORM_ROCE_RESP_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define MSTORM_ROCE_RESP_CONN_AG_CTX_BIT1_SHIFT		1
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: cf0 */
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF0_SHIFT		2
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: cf1 */
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF1_SHIFT		4
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: cf2 */
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_SHIFT	0
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF1EN_SHIFT	1
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF2EN_SHIFT	2
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_MASK	0x1	/* HSI_COMMENT: rule0en */
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_SHIFT	3
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_MASK	0x1	/* HSI_COMMENT: rule1en */
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_SHIFT	4
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_MASK	0x1	/* HSI_COMMENT: rule2en */
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_SHIFT	5
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_MASK	0x1	/* HSI_COMMENT: rule3en */
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_SHIFT	6
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_MASK	0x1	/* HSI_COMMENT: rule4en */
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_SHIFT	7
	__le16 word0;		/* HSI_COMMENT: word0 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
};

/* Roce doorbell data */
enum roce_flavor {
	PLAIN_ROCE,		/* HSI_COMMENT: RoCE v1 */
	RROCE_IPV4,		/* HSI_COMMENT: RoCE v2 (Routable RoCE) over ipv4 */
	RROCE_IPV6,		/* HSI_COMMENT: RoCE v2 (Routable RoCE) over ipv6 */
	MAX_ROCE_FLAVOR
};

struct tstorm_roce_req_conn_ag_ctx {
	u8 reserved0;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define TSTORM_ROCE_REQ_CONN_AG_CTX_EXIST_IN_QM0_MASK			0x1	/* HSI_COMMENT: exist_in_qm0 */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_EXIST_IN_QM0_SHIFT			0
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RX_ERROR_OCCURED_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RX_ERROR_OCCURED_SHIFT		1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TX_CQE_ERROR_OCCURED_MASK		0x1	/* HSI_COMMENT: bit2 */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TX_CQE_ERROR_OCCURED_SHIFT		2
#define TSTORM_ROCE_REQ_CONN_AG_CTX_BIT3_MASK				0x1	/* HSI_COMMENT: bit3 */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_BIT3_SHIFT				3
#define TSTORM_ROCE_REQ_CONN_AG_CTX_MSTORM_FLUSH_MASK			0x1	/* HSI_COMMENT: bit4 */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_MSTORM_FLUSH_SHIFT			4
#define TSTORM_ROCE_REQ_CONN_AG_CTX_CACHED_ORQ_MASK			0x1	/* HSI_COMMENT: bit5 */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_CACHED_ORQ_SHIFT			5
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TIMER_CF_MASK			0x3	/* HSI_COMMENT: timer0cf */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TIMER_CF_SHIFT			6
	u8 flags1;
#define TSTORM_ROCE_REQ_CONN_AG_CTX_MSTORM_FLUSH_CF_MASK		0x3	/* HSI_COMMENT: timer1cf */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_MSTORM_FLUSH_CF_SHIFT		0
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_SQ_CF_MASK			0x3	/* HSI_COMMENT: timer2cf */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_SQ_CF_SHIFT			2
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TIMER_STOP_ALL_CF_MASK		0x3	/* HSI_COMMENT: timer_stop_all */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TIMER_STOP_ALL_CF_SHIFT		4
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_Q0_CF_MASK			0x3	/* HSI_COMMENT: cf4 */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_Q0_CF_SHIFT			6
	u8 flags2;
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FORCE_COMP_CF_MASK			0x3	/* HSI_COMMENT: cf5 */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FORCE_COMP_CF_SHIFT			0
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SET_TIMER_CF_MASK			0x3	/* HSI_COMMENT: cf6 */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SET_TIMER_CF_SHIFT			2
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TX_ASYNC_ERROR_CF_MASK		0x3	/* HSI_COMMENT: cf7 */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TX_ASYNC_ERROR_CF_SHIFT		4
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RXMIT_DONE_CF_MASK			0x3	/* HSI_COMMENT: cf8 */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RXMIT_DONE_CF_SHIFT			6
	u8 flags3;
#define TSTORM_ROCE_REQ_CONN_AG_CTX_ERROR_SCAN_COMPLETED_CF_MASK	0x3	/* HSI_COMMENT: cf9 */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_ERROR_SCAN_COMPLETED_CF_SHIFT	0
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SQ_DRAIN_COMPLETED_CF_MASK		0x3	/* HSI_COMMENT: cf10 */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SQ_DRAIN_COMPLETED_CF_SHIFT		2
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TIMER_CF_EN_MASK			0x1	/* HSI_COMMENT: cf0en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TIMER_CF_EN_SHIFT			4
#define TSTORM_ROCE_REQ_CONN_AG_CTX_MSTORM_FLUSH_CF_EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_MSTORM_FLUSH_CF_EN_SHIFT		5
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_SQ_CF_EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_SQ_CF_EN_SHIFT		6
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TIMER_STOP_ALL_CF_EN_MASK		0x1	/* HSI_COMMENT: cf3en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TIMER_STOP_ALL_CF_EN_SHIFT		7
	u8 flags4;
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_Q0_CF_EN_MASK			0x1	/* HSI_COMMENT: cf4en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_Q0_CF_EN_SHIFT		0
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FORCE_COMP_CF_EN_MASK		0x1	/* HSI_COMMENT: cf5en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FORCE_COMP_CF_EN_SHIFT		1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SET_TIMER_CF_EN_MASK		0x1	/* HSI_COMMENT: cf6en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SET_TIMER_CF_EN_SHIFT		2
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TX_ASYNC_ERROR_CF_EN_MASK		0x1	/* HSI_COMMENT: cf7en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TX_ASYNC_ERROR_CF_EN_SHIFT		3
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RXMIT_DONE_CF_EN_MASK		0x1	/* HSI_COMMENT: cf8en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RXMIT_DONE_CF_EN_SHIFT		4
#define TSTORM_ROCE_REQ_CONN_AG_CTX_ERROR_SCAN_COMPLETED_CF_EN_MASK	0x1	/* HSI_COMMENT: cf9en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_ERROR_SCAN_COMPLETED_CF_EN_SHIFT	5
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SQ_DRAIN_COMPLETED_CF_EN_MASK	0x1	/* HSI_COMMENT: cf10en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SQ_DRAIN_COMPLETED_CF_EN_SHIFT	6
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_SHIFT			7
	u8 flags5;
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_SHIFT			0
#define TSTORM_ROCE_REQ_CONN_AG_CTX_DIF_CNT_EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_DIF_CNT_EN_SHIFT			1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_SHIFT			2
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_SHIFT			3
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE5EN_SHIFT			4
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SND_SQ_CONS_EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SND_SQ_CONS_EN_SHIFT		5
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE7EN_MASK			0x1	/* HSI_COMMENT: rule7en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE7EN_SHIFT			6
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE8EN_MASK			0x1	/* HSI_COMMENT: rule8en */
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE8EN_SHIFT			7
	__le32 dif_rxmit_cnt;	/* HSI_COMMENT: reg0 */
	__le32 snd_nxt_psn;	/* HSI_COMMENT: reg1 */
	__le32 snd_max_psn;	/* HSI_COMMENT: reg2 */
	__le32 orq_prod;	/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
	__le32 dif_acked_cnt;	/* HSI_COMMENT: reg5 */
	__le32 dif_cnt;		/* HSI_COMMENT: reg6 */
	__le32 reg7;		/* HSI_COMMENT: reg7 */
	__le32 reg8;		/* HSI_COMMENT: reg8 */
	u8 tx_cqe_error_type;	/* HSI_COMMENT: byte2 */
	u8 orq_cache_idx;	/* HSI_COMMENT: byte3 */
	__le16 snd_sq_cons_th;	/* HSI_COMMENT: word0 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	u8 byte5;		/* HSI_COMMENT: byte5 */
	__le16 snd_sq_cons;	/* HSI_COMMENT: word1 */
	__le16 conn_dpi;	/* HSI_COMMENT: conn_dpi */
	__le16 force_comp_cons;	/* HSI_COMMENT: word3 */
	__le32 dif_rxmit_acked_cnt;	/* HSI_COMMENT: reg9 */
	__le32 reg10;		/* HSI_COMMENT: reg10 */
};

struct tstorm_roce_resp_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define TSTORM_ROCE_RESP_CONN_AG_CTX_EXIST_IN_QM0_MASK			0x1	/* HSI_COMMENT: exist_in_qm0 */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_EXIST_IN_QM0_SHIFT			0
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_NOTIFY_REQUESTER_MASK	0x1	/* HSI_COMMENT: exist_in_qm1 */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_NOTIFY_REQUESTER_SHIFT	1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_BIT2_MASK				0x1	/* HSI_COMMENT: bit2 */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_BIT2_SHIFT				2
#define TSTORM_ROCE_RESP_CONN_AG_CTX_BIT3_MASK				0x1	/* HSI_COMMENT: bit3 */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_BIT3_SHIFT				3
#define TSTORM_ROCE_RESP_CONN_AG_CTX_MSTORM_FLUSH_MASK			0x1	/* HSI_COMMENT: bit4 */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_MSTORM_FLUSH_SHIFT			4
#define TSTORM_ROCE_RESP_CONN_AG_CTX_BIT5_MASK				0x1	/* HSI_COMMENT: bit5 */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_BIT5_SHIFT				5
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF0_MASK				0x3	/* HSI_COMMENT: timer0cf */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF0_SHIFT				6
	u8 flags1;
#define TSTORM_ROCE_RESP_CONN_AG_CTX_MSTORM_FLUSH_CF_MASK		0x3	/* HSI_COMMENT: timer1cf */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_MSTORM_FLUSH_CF_SHIFT		0
#define TSTORM_ROCE_RESP_CONN_AG_CTX_TX_ERROR_CF_MASK			0x3	/* HSI_COMMENT: timer2cf */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_TX_ERROR_CF_SHIFT			2
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF3_MASK				0x3	/* HSI_COMMENT: timer_stop_all */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF3_SHIFT				4
#define TSTORM_ROCE_RESP_CONN_AG_CTX_FLUSH_Q0_CF_MASK			0x3	/* HSI_COMMENT: cf4 */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_FLUSH_Q0_CF_SHIFT			6
	u8 flags2;
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_CF_MASK			0x3	/* HSI_COMMENT: cf5 */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_CF_SHIFT			0
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF6_MASK				0x3	/* HSI_COMMENT: cf6 */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF6_SHIFT				2
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF7_MASK				0x3	/* HSI_COMMENT: cf7 */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF7_SHIFT				4
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF8_MASK				0x3	/* HSI_COMMENT: cf8 */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF8_SHIFT				6
	u8 flags3;
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF9_MASK				0x3	/* HSI_COMMENT: cf9 */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF9_SHIFT				0
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF10_MASK				0x3	/* HSI_COMMENT: cf10 */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF10_SHIFT				2
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_MASK				0x1	/* HSI_COMMENT: cf0en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_SHIFT			4
#define TSTORM_ROCE_RESP_CONN_AG_CTX_MSTORM_FLUSH_CF_EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_MSTORM_FLUSH_CF_EN_SHIFT		5
#define TSTORM_ROCE_RESP_CONN_AG_CTX_TX_ERROR_CF_EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_TX_ERROR_CF_EN_SHIFT		6
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF3EN_MASK				0x1	/* HSI_COMMENT: cf3en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF3EN_SHIFT			7
	u8 flags4;
#define TSTORM_ROCE_RESP_CONN_AG_CTX_FLUSH_Q0_CF_EN_MASK		0x1	/* HSI_COMMENT: cf4en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_FLUSH_Q0_CF_EN_SHIFT		0
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_CF_EN_MASK		0x1	/* HSI_COMMENT: cf5en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_CF_EN_SHIFT		1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF6EN_MASK				0x1	/* HSI_COMMENT: cf6en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF6EN_SHIFT			2
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF7EN_MASK				0x1	/* HSI_COMMENT: cf7en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF7EN_SHIFT			3
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF8EN_MASK				0x1	/* HSI_COMMENT: cf8en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF8EN_SHIFT			4
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF9EN_MASK				0x1	/* HSI_COMMENT: cf9en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF9EN_SHIFT			5
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF10EN_MASK			0x1	/* HSI_COMMENT: cf10en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF10EN_SHIFT			6
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_SHIFT			7
	u8 flags5;
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_SHIFT			0
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_SHIFT			1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_SHIFT			2
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_SHIFT			3
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE5EN_SHIFT			4
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RQ_RULE_EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RQ_RULE_EN_SHIFT			5
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE7EN_MASK			0x1	/* HSI_COMMENT: rule7en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE7EN_SHIFT			6
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE8EN_MASK			0x1	/* HSI_COMMENT: rule8en */
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE8EN_SHIFT			7
	__le32 psn_and_rxmit_id_echo;	/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
	__le32 reg5;		/* HSI_COMMENT: reg5 */
	__le32 reg6;		/* HSI_COMMENT: reg6 */
	__le32 reg7;		/* HSI_COMMENT: reg7 */
	__le32 reg8;		/* HSI_COMMENT: reg8 */
	u8 tx_async_error_type;	/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 rq_cons;		/* HSI_COMMENT: word0 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	u8 byte5;		/* HSI_COMMENT: byte5 */
	__le16 rq_prod;		/* HSI_COMMENT: word1 */
	__le16 conn_dpi;	/* HSI_COMMENT: conn_dpi */
	__le16 irq_cons;	/* HSI_COMMENT: word3 */
	__le32 reg9;		/* HSI_COMMENT: reg9 */
	__le32 reg10;		/* HSI_COMMENT: reg10 */
};

struct ustorm_roce_req_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define USTORM_ROCE_REQ_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define USTORM_ROCE_REQ_CONN_AG_CTX_BIT0_SHIFT		0
#define USTORM_ROCE_REQ_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define USTORM_ROCE_REQ_CONN_AG_CTX_BIT1_SHIFT		1
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: timer0cf */
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF0_SHIFT		2
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: timer1cf */
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF1_SHIFT		4
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: timer2cf */
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF3_MASK		0x3	/* HSI_COMMENT: timer_stop_all */
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF3_SHIFT		0
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF4_MASK		0x3	/* HSI_COMMENT: cf4 */
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF4_SHIFT		2
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF5_MASK		0x3	/* HSI_COMMENT: cf5 */
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF5_SHIFT		4
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF6_MASK		0x3	/* HSI_COMMENT: cf6 */
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF6_SHIFT		6
	u8 flags2;
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF0EN_SHIFT		0
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF1EN_SHIFT		1
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF2EN_SHIFT		2
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF3EN_MASK		0x1	/* HSI_COMMENT: cf3en */
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF3EN_SHIFT		3
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF4EN_MASK		0x1	/* HSI_COMMENT: cf4en */
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF4EN_SHIFT		4
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF5EN_MASK		0x1	/* HSI_COMMENT: cf5en */
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF5EN_SHIFT		5
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF6EN_MASK		0x1	/* HSI_COMMENT: cf6en */
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF6EN_SHIFT		6
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_MASK	0x1	/* HSI_COMMENT: rule0en */
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_SHIFT	7
	u8 flags3;
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_MASK	0x1	/* HSI_COMMENT: rule1en */
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_SHIFT	0
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE2EN_MASK	0x1	/* HSI_COMMENT: rule2en */
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE2EN_SHIFT	1
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_MASK	0x1	/* HSI_COMMENT: rule3en */
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_SHIFT	2
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_MASK	0x1	/* HSI_COMMENT: rule4en */
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_SHIFT	3
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE5EN_MASK	0x1	/* HSI_COMMENT: rule5en */
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE5EN_SHIFT	4
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE6EN_MASK	0x1	/* HSI_COMMENT: rule6en */
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE6EN_SHIFT	5
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE7EN_MASK	0x1	/* HSI_COMMENT: rule7en */
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE7EN_SHIFT	6
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE8EN_MASK	0x1	/* HSI_COMMENT: rule8en */
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE8EN_SHIFT	7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: conn_dpi */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
};

struct ustorm_roce_resp_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define USTORM_ROCE_RESP_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define USTORM_ROCE_RESP_CONN_AG_CTX_BIT0_SHIFT		0
#define USTORM_ROCE_RESP_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define USTORM_ROCE_RESP_CONN_AG_CTX_BIT1_SHIFT		1
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: timer0cf */
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF0_SHIFT		2
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: timer1cf */
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF1_SHIFT		4
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: timer2cf */
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF3_MASK		0x3	/* HSI_COMMENT: timer_stop_all */
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF3_SHIFT		0
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF4_MASK		0x3	/* HSI_COMMENT: cf4 */
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF4_SHIFT		2
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF5_MASK		0x3	/* HSI_COMMENT: cf5 */
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF5_SHIFT		4
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF6_MASK		0x3	/* HSI_COMMENT: cf6 */
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF6_SHIFT		6
	u8 flags2;
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_SHIFT	0
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF1EN_SHIFT	1
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF2EN_SHIFT	2
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF3EN_MASK		0x1	/* HSI_COMMENT: cf3en */
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF3EN_SHIFT	3
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF4EN_MASK		0x1	/* HSI_COMMENT: cf4en */
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF4EN_SHIFT	4
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF5EN_MASK		0x1	/* HSI_COMMENT: cf5en */
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF5EN_SHIFT	5
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF6EN_MASK		0x1	/* HSI_COMMENT: cf6en */
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF6EN_SHIFT	6
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_MASK	0x1	/* HSI_COMMENT: rule0en */
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_SHIFT	7
	u8 flags3;
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_MASK	0x1	/* HSI_COMMENT: rule1en */
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_SHIFT	0
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_MASK	0x1	/* HSI_COMMENT: rule2en */
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_SHIFT	1
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_MASK	0x1	/* HSI_COMMENT: rule3en */
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_SHIFT	2
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_MASK	0x1	/* HSI_COMMENT: rule4en */
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_SHIFT	3
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE5EN_MASK	0x1	/* HSI_COMMENT: rule5en */
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE5EN_SHIFT	4
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE6EN_MASK	0x1	/* HSI_COMMENT: rule6en */
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE6EN_SHIFT	5
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE7EN_MASK	0x1	/* HSI_COMMENT: rule7en */
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE7EN_SHIFT	6
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE8EN_MASK	0x1	/* HSI_COMMENT: rule8en */
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE8EN_SHIFT	7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: conn_dpi */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
};

struct xstorm_roce_req_conn_ag_ctx {
	u8 reserved0;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_EXIST_IN_QM0_MASK			0x1	/* HSI_COMMENT: exist_in_qm0 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_EXIST_IN_QM0_SHIFT			0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED1_SHIFT			1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED2_MASK			0x1	/* HSI_COMMENT: exist_in_qm2 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED2_SHIFT			2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_EXIST_IN_QM3_MASK			0x1	/* HSI_COMMENT: exist_in_qm3 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_EXIST_IN_QM3_SHIFT			3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED3_MASK			0x1	/* HSI_COMMENT: bit4 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED3_SHIFT			4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED4_MASK			0x1	/* HSI_COMMENT: cf_array_active */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED4_SHIFT			5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED5_MASK			0x1	/* HSI_COMMENT: bit6 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED5_SHIFT			6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED6_MASK			0x1	/* HSI_COMMENT: bit7 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED6_SHIFT			7
	u8 flags1;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED7_MASK			0x1	/* HSI_COMMENT: bit8 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED7_SHIFT			0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED8_MASK			0x1	/* HSI_COMMENT: bit9 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED8_SHIFT			1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_BIT10_MASK				0x1	/* HSI_COMMENT: bit10 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_BIT10_SHIFT				2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_BIT11_MASK				0x1	/* HSI_COMMENT: bit11 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_BIT11_SHIFT				3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_MSDM_FLUSH_MASK			0x1	/* HSI_COMMENT: bit12 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_MSDM_FLUSH_SHIFT			4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_MSEM_FLUSH_MASK			0x1	/* HSI_COMMENT: bit13 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_MSEM_FLUSH_SHIFT			5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_ERROR_STATE_MASK			0x1	/* HSI_COMMENT: bit14 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_ERROR_STATE_SHIFT			6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_YSTORM_FLUSH_MASK			0x1	/* HSI_COMMENT: bit15 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_YSTORM_FLUSH_SHIFT			7
	u8 flags2;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF0_MASK				0x3	/* HSI_COMMENT: timer0cf */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF0_SHIFT				0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF1_MASK				0x3	/* HSI_COMMENT: timer1cf */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF1_SHIFT				2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF2_MASK				0x3	/* HSI_COMMENT: timer2cf */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF2_SHIFT				4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF3_MASK				0x3	/* HSI_COMMENT: timer_stop_all */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF3_SHIFT				6
	u8 flags3;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SQ_FLUSH_CF_MASK			0x3	/* HSI_COMMENT: cf4 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SQ_FLUSH_CF_SHIFT			0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RX_ERROR_CF_MASK			0x3	/* HSI_COMMENT: cf5 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RX_ERROR_CF_SHIFT			2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SND_RXMIT_CF_MASK			0x3	/* HSI_COMMENT: cf6 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SND_RXMIT_CF_SHIFT			4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_Q0_CF_MASK			0x3	/* HSI_COMMENT: cf7 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_Q0_CF_SHIFT			6
	u8 flags4;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_DIF_ERROR_CF_MASK			0x3	/* HSI_COMMENT: cf8 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_DIF_ERROR_CF_SHIFT			0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SCAN_SQ_FOR_COMP_CF_MASK		0x3	/* HSI_COMMENT: cf9 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SCAN_SQ_FOR_COMP_CF_SHIFT		2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF10_MASK				0x3	/* HSI_COMMENT: cf10 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF10_SHIFT				4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF11_MASK				0x3	/* HSI_COMMENT: cf11 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF11_SHIFT				6
	u8 flags5;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF12_MASK				0x3	/* HSI_COMMENT: cf12 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF12_SHIFT				0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF13_MASK				0x3	/* HSI_COMMENT: cf13 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF13_SHIFT				2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_FMR_ENDED_CF_MASK			0x3	/* HSI_COMMENT: cf14 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_FMR_ENDED_CF_SHIFT			4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF15_MASK				0x3	/* HSI_COMMENT: cf15 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF15_SHIFT				6
	u8 flags6;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF16_MASK				0x3	/* HSI_COMMENT: cf16 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF16_SHIFT				0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF17_MASK				0x3	/* HSI_COMMENT: cf_array_cf */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF17_SHIFT				2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF18_MASK				0x3	/* HSI_COMMENT: cf18 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF18_SHIFT				4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF19_MASK				0x3	/* HSI_COMMENT: cf19 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF19_SHIFT				6
	u8 flags7;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF20_MASK				0x3	/* HSI_COMMENT: cf20 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF20_SHIFT				0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF21_MASK				0x3	/* HSI_COMMENT: cf21 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF21_SHIFT				2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SLOW_PATH_MASK			0x3	/* HSI_COMMENT: cf22 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SLOW_PATH_SHIFT			4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF0EN_MASK				0x1	/* HSI_COMMENT: cf0en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF0EN_SHIFT				6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF1EN_MASK				0x1	/* HSI_COMMENT: cf1en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF1EN_SHIFT				7
	u8 flags8;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF2EN_MASK				0x1	/* HSI_COMMENT: cf2en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF2EN_SHIFT				0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF3EN_MASK				0x1	/* HSI_COMMENT: cf3en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF3EN_SHIFT				1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SQ_FLUSH_CF_EN_MASK			0x1	/* HSI_COMMENT: cf4en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SQ_FLUSH_CF_EN_SHIFT		2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RX_ERROR_CF_EN_MASK			0x1	/* HSI_COMMENT: cf5en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RX_ERROR_CF_EN_SHIFT		3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SND_RXMIT_CF_EN_MASK		0x1	/* HSI_COMMENT: cf6en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SND_RXMIT_CF_EN_SHIFT		4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_Q0_CF_EN_MASK			0x1	/* HSI_COMMENT: cf7en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_Q0_CF_EN_SHIFT		5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_DIF_ERROR_CF_EN_MASK		0x1	/* HSI_COMMENT: cf8en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_DIF_ERROR_CF_EN_SHIFT		6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SCAN_SQ_FOR_COMP_CF_EN_MASK		0x1	/* HSI_COMMENT: cf9en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SCAN_SQ_FOR_COMP_CF_EN_SHIFT	7
	u8 flags9;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF10EN_MASK				0x1	/* HSI_COMMENT: cf10en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF10EN_SHIFT			0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF11EN_MASK				0x1	/* HSI_COMMENT: cf11en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF11EN_SHIFT			1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF12EN_MASK				0x1	/* HSI_COMMENT: cf12en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF12EN_SHIFT			2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF13EN_MASK				0x1	/* HSI_COMMENT: cf13en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF13EN_SHIFT			3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_FME_ENDED_CF_EN_MASK		0x1	/* HSI_COMMENT: cf14en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_FME_ENDED_CF_EN_SHIFT		4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF15EN_MASK				0x1	/* HSI_COMMENT: cf15en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF15EN_SHIFT			5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF16EN_MASK				0x1	/* HSI_COMMENT: cf16en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF16EN_SHIFT			6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF17EN_MASK				0x1	/* HSI_COMMENT: cf_array_cf_en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF17EN_SHIFT			7
	u8 flags10;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF18EN_MASK				0x1	/* HSI_COMMENT: cf18en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF18EN_SHIFT			0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF19EN_MASK				0x1	/* HSI_COMMENT: cf19en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF19EN_SHIFT			1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF20EN_MASK				0x1	/* HSI_COMMENT: cf20en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF20EN_SHIFT			2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF21EN_MASK				0x1	/* HSI_COMMENT: cf21en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF21EN_SHIFT			3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SLOW_PATH_EN_MASK			0x1	/* HSI_COMMENT: cf22en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SLOW_PATH_EN_SHIFT			4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF23EN_MASK				0x1	/* HSI_COMMENT: cf23en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF23EN_SHIFT			5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_SHIFT			6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_SHIFT			7
	u8 flags11;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE2EN_SHIFT			0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_SHIFT			1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_SHIFT			2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE5EN_SHIFT			3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE6EN_SHIFT			4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_E2E_CREDIT_RULE_EN_MASK		0x1	/* HSI_COMMENT: rule7en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_E2E_CREDIT_RULE_EN_SHIFT		5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED1_MASK			0x1	/* HSI_COMMENT: rule8en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED1_SHIFT			6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE9EN_MASK			0x1	/* HSI_COMMENT: rule9en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE9EN_SHIFT			7
	u8 flags12;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SQ_PROD_EN_MASK			0x1	/* HSI_COMMENT: rule10en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SQ_PROD_EN_SHIFT			0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE11EN_MASK			0x1	/* HSI_COMMENT: rule11en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE11EN_SHIFT			1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED2_MASK			0x1	/* HSI_COMMENT: rule12en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED2_SHIFT			2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED3_MASK			0x1	/* HSI_COMMENT: rule13en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED3_SHIFT			3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_INV_FENCE_RULE_EN_MASK		0x1	/* HSI_COMMENT: rule14en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_INV_FENCE_RULE_EN_SHIFT		4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE15EN_MASK			0x1	/* HSI_COMMENT: rule15en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE15EN_SHIFT			5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_ORQ_FENCE_RULE_EN_MASK		0x1	/* HSI_COMMENT: rule16en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_ORQ_FENCE_RULE_EN_SHIFT		6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_MAX_ORD_RULE_EN_MASK		0x1	/* HSI_COMMENT: rule17en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_MAX_ORD_RULE_EN_SHIFT		7
	u8 flags13;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE18EN_MASK			0x1	/* HSI_COMMENT: rule18en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE18EN_SHIFT			0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE19EN_MASK			0x1	/* HSI_COMMENT: rule19en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE19EN_SHIFT			1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED4_MASK			0x1	/* HSI_COMMENT: rule20en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED4_SHIFT			2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED5_MASK			0x1	/* HSI_COMMENT: rule21en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED5_SHIFT			3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED6_MASK			0x1	/* HSI_COMMENT: rule22en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED6_SHIFT			4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED7_MASK			0x1	/* HSI_COMMENT: rule23en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED7_SHIFT			5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED8_MASK			0x1	/* HSI_COMMENT: rule24en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED8_SHIFT			6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED9_MASK			0x1	/* HSI_COMMENT: rule25en */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED9_SHIFT			7
	u8 flags14;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_MIGRATION_FLAG_MASK			0x1	/* HSI_COMMENT: bit16 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_MIGRATION_FLAG_SHIFT		0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_BIT17_MASK				0x1	/* HSI_COMMENT: bit17 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_BIT17_SHIFT				1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_DPM_PORT_NUM_MASK			0x3	/* HSI_COMMENT: bit18 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_DPM_PORT_NUM_SHIFT			2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED_MASK			0x1	/* HSI_COMMENT: bit20 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED_SHIFT			4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_ROCE_EDPM_ENABLE_MASK		0x1	/* HSI_COMMENT: bit21 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_ROCE_EDPM_ENABLE_SHIFT		5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF23_MASK				0x3	/* HSI_COMMENT: cf23 */
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF23_SHIFT				6
	u8 byte2;		/* HSI_COMMENT: byte2 */
	__le16 physical_q0;	/* HSI_COMMENT: physical_q0 */
	__le16 word1;		/* HSI_COMMENT: physical_q1 */
	__le16 sq_cmp_cons;	/* HSI_COMMENT: physical_q2 */
	__le16 sq_cons;		/* HSI_COMMENT: word3 */
	__le16 sq_prod;		/* HSI_COMMENT: word4 */
	__le16 dif_error_first_sq_cons;	/* HSI_COMMENT: word5 */
	__le16 conn_dpi;	/* HSI_COMMENT: conn_dpi */
	u8 dif_error_sge_index;	/* HSI_COMMENT: byte3 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	u8 byte5;		/* HSI_COMMENT: byte5 */
	u8 byte6;		/* HSI_COMMENT: byte6 */
	__le32 lsn;		/* HSI_COMMENT: reg0 */
	__le32 ssn;		/* HSI_COMMENT: reg1 */
	__le32 snd_una_psn;	/* HSI_COMMENT: reg2 */
	__le32 snd_nxt_psn;	/* HSI_COMMENT: reg3 */
	__le32 dif_error_offset;	/* HSI_COMMENT: reg4 */
	__le32 orq_cons_th;	/* HSI_COMMENT: cf_array0 */
	__le32 orq_cons;	/* HSI_COMMENT: cf_array1 */
};

struct xstorm_roce_resp_conn_ag_ctx {
	u8 reserved0;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED1_SHIFT		1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED2_MASK		0x1	/* HSI_COMMENT: exist_in_qm2 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED2_SHIFT		2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_EXIST_IN_QM3_MASK		0x1	/* HSI_COMMENT: exist_in_qm3 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_EXIST_IN_QM3_SHIFT		3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED3_MASK		0x1	/* HSI_COMMENT: bit4 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED3_SHIFT		4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED4_MASK		0x1	/* HSI_COMMENT: cf_array_active */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED4_SHIFT		5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED5_MASK		0x1	/* HSI_COMMENT: bit6 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED5_SHIFT		6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED6_MASK		0x1	/* HSI_COMMENT: bit7 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED6_SHIFT		7
	u8 flags1;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED7_MASK		0x1	/* HSI_COMMENT: bit8 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED7_SHIFT		0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED8_MASK		0x1	/* HSI_COMMENT: bit9 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED8_SHIFT		1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT10_MASK			0x1	/* HSI_COMMENT: bit10 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT10_SHIFT		2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT11_MASK			0x1	/* HSI_COMMENT: bit11 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT11_SHIFT		3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_MSDM_FLUSH_MASK		0x1	/* HSI_COMMENT: bit12 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_MSDM_FLUSH_SHIFT		4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_MSEM_FLUSH_MASK		0x1	/* HSI_COMMENT: bit13 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_MSEM_FLUSH_SHIFT		5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_ERROR_STATE_MASK		0x1	/* HSI_COMMENT: bit14 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_ERROR_STATE_SHIFT		6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_YSTORM_FLUSH_MASK		0x1	/* HSI_COMMENT: bit15 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_YSTORM_FLUSH_SHIFT		7
	u8 flags2;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF0_MASK			0x3	/* HSI_COMMENT: timer0cf */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF0_SHIFT			0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF1_MASK			0x3	/* HSI_COMMENT: timer1cf */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF1_SHIFT			2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF2_MASK			0x3	/* HSI_COMMENT: timer2cf */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF2_SHIFT			4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF3_MASK			0x3	/* HSI_COMMENT: timer_stop_all */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF3_SHIFT			6
	u8 flags3;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RXMIT_CF_MASK		0x3	/* HSI_COMMENT: cf4 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RXMIT_CF_SHIFT		0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_CF_MASK		0x3	/* HSI_COMMENT: cf5 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_CF_SHIFT		2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_FORCE_ACK_CF_MASK		0x3	/* HSI_COMMENT: cf6 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_FORCE_ACK_CF_SHIFT		4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_FLUSH_Q0_CF_MASK		0x3	/* HSI_COMMENT: cf7 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_FLUSH_Q0_CF_SHIFT		6
	u8 flags4;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF8_MASK			0x3	/* HSI_COMMENT: cf8 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF8_SHIFT			0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF9_MASK			0x3	/* HSI_COMMENT: cf9 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF9_SHIFT			2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF10_MASK			0x3	/* HSI_COMMENT: cf10 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF10_SHIFT			4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF11_MASK			0x3	/* HSI_COMMENT: cf11 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF11_SHIFT			6
	u8 flags5;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF12_MASK			0x3	/* HSI_COMMENT: cf12 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF12_SHIFT			0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF13_MASK			0x3	/* HSI_COMMENT: cf13 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF13_SHIFT			2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF14_MASK			0x3	/* HSI_COMMENT: cf14 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF14_SHIFT			4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF15_MASK			0x3	/* HSI_COMMENT: cf15 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF15_SHIFT			6
	u8 flags6;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF16_MASK			0x3	/* HSI_COMMENT: cf16 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF16_SHIFT			0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF17_MASK			0x3	/* HSI_COMMENT: cf_array_cf */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF17_SHIFT			2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF18_MASK			0x3	/* HSI_COMMENT: cf18 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF18_SHIFT			4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF19_MASK			0x3	/* HSI_COMMENT: cf19 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF19_SHIFT			6
	u8 flags7;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF20_MASK			0x3	/* HSI_COMMENT: cf20 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF20_SHIFT			0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF21_MASK			0x3	/* HSI_COMMENT: cf21 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF21_SHIFT			2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_SLOW_PATH_MASK		0x3	/* HSI_COMMENT: cf22 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_SLOW_PATH_SHIFT		4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_MASK			0x1	/* HSI_COMMENT: cf0en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_SHIFT		6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF1EN_MASK			0x1	/* HSI_COMMENT: cf1en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF1EN_SHIFT		7
	u8 flags8;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF2EN_SHIFT		0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF3EN_MASK			0x1	/* HSI_COMMENT: cf3en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF3EN_SHIFT		1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RXMIT_CF_EN_MASK		0x1	/* HSI_COMMENT: cf4en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RXMIT_CF_EN_SHIFT		2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_CF_EN_MASK	0x1	/* HSI_COMMENT: cf5en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_CF_EN_SHIFT	3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_FORCE_ACK_CF_EN_MASK	0x1	/* HSI_COMMENT: cf6en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_FORCE_ACK_CF_EN_SHIFT	4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_FLUSH_Q0_CF_EN_MASK	0x1	/* HSI_COMMENT: cf7en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_FLUSH_Q0_CF_EN_SHIFT	5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF8EN_MASK			0x1	/* HSI_COMMENT: cf8en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF8EN_SHIFT		6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF9EN_MASK			0x1	/* HSI_COMMENT: cf9en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF9EN_SHIFT		7
	u8 flags9;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF10EN_MASK		0x1	/* HSI_COMMENT: cf10en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF10EN_SHIFT		0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF11EN_MASK		0x1	/* HSI_COMMENT: cf11en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF11EN_SHIFT		1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF12EN_MASK		0x1	/* HSI_COMMENT: cf12en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF12EN_SHIFT		2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF13EN_MASK		0x1	/* HSI_COMMENT: cf13en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF13EN_SHIFT		3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF14EN_MASK		0x1	/* HSI_COMMENT: cf14en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF14EN_SHIFT		4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF15EN_MASK		0x1	/* HSI_COMMENT: cf15en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF15EN_SHIFT		5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF16EN_MASK		0x1	/* HSI_COMMENT: cf16en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF16EN_SHIFT		6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF17EN_MASK		0x1	/* HSI_COMMENT: cf_array_cf_en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF17EN_SHIFT		7
	u8 flags10;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF18EN_MASK		0x1	/* HSI_COMMENT: cf18en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF18EN_SHIFT		0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF19EN_MASK		0x1	/* HSI_COMMENT: cf19en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF19EN_SHIFT		1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF20EN_MASK		0x1	/* HSI_COMMENT: cf20en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF20EN_SHIFT		2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF21EN_MASK		0x1	/* HSI_COMMENT: cf21en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF21EN_SHIFT		3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_SLOW_PATH_EN_MASK		0x1	/* HSI_COMMENT: cf22en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_SLOW_PATH_EN_SHIFT		4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF23EN_MASK		0x1	/* HSI_COMMENT: cf23en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF23EN_SHIFT		5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_MASK		0x1	/* HSI_COMMENT: rule0en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_SHIFT		6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_SHIFT		7
	u8 flags11;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_SHIFT		0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_SHIFT		1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_SHIFT		2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE5EN_MASK		0x1	/* HSI_COMMENT: rule5en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE5EN_SHIFT		3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE6EN_MASK		0x1	/* HSI_COMMENT: rule6en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE6EN_SHIFT		4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE7EN_MASK		0x1	/* HSI_COMMENT: rule7en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE7EN_SHIFT		5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED1_MASK		0x1	/* HSI_COMMENT: rule8en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED1_SHIFT		6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE9EN_MASK		0x1	/* HSI_COMMENT: rule9en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE9EN_SHIFT		7
	u8 flags12;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_IRQ_PROD_RULE_EN_MASK	0x1	/* HSI_COMMENT: rule10en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_IRQ_PROD_RULE_EN_SHIFT	0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE11EN_MASK		0x1	/* HSI_COMMENT: rule11en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE11EN_SHIFT		1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED2_MASK		0x1	/* HSI_COMMENT: rule12en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED2_SHIFT		2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED3_MASK		0x1	/* HSI_COMMENT: rule13en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED3_SHIFT		3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE14EN_MASK		0x1	/* HSI_COMMENT: rule14en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE14EN_SHIFT		4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE15EN_MASK		0x1	/* HSI_COMMENT: rule15en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE15EN_SHIFT		5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE16EN_MASK		0x1	/* HSI_COMMENT: rule16en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE16EN_SHIFT		6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE17EN_MASK		0x1	/* HSI_COMMENT: rule17en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE17EN_SHIFT		7
	u8 flags13;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE18EN_MASK		0x1	/* HSI_COMMENT: rule18en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE18EN_SHIFT		0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE19EN_MASK		0x1	/* HSI_COMMENT: rule19en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE19EN_SHIFT		1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED4_MASK		0x1	/* HSI_COMMENT: rule20en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED4_SHIFT		2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED5_MASK		0x1	/* HSI_COMMENT: rule21en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED5_SHIFT		3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED6_MASK		0x1	/* HSI_COMMENT: rule22en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED6_SHIFT		4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED7_MASK		0x1	/* HSI_COMMENT: rule23en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED7_SHIFT		5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED8_MASK		0x1	/* HSI_COMMENT: rule24en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED8_SHIFT		6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED9_MASK		0x1	/* HSI_COMMENT: rule25en */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED9_SHIFT		7
	u8 flags14;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT16_MASK			0x1	/* HSI_COMMENT: bit16 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT16_SHIFT		0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT17_MASK			0x1	/* HSI_COMMENT: bit17 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT17_SHIFT		1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT18_MASK			0x1	/* HSI_COMMENT: bit18 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT18_SHIFT		2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT19_MASK			0x1	/* HSI_COMMENT: bit19 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT19_SHIFT		3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT20_MASK			0x1	/* HSI_COMMENT: bit20 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT20_SHIFT		4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT21_MASK			0x1	/* HSI_COMMENT: bit21 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT21_SHIFT		5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF23_MASK			0x3	/* HSI_COMMENT: cf23 */
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF23_SHIFT			6
	u8 byte2;		/* HSI_COMMENT: byte2 */
	__le16 physical_q0;	/* HSI_COMMENT: physical_q0 */
	__le16 irq_prod_shadow;	/* HSI_COMMENT: physical_q1 */
	__le16 word2;		/* HSI_COMMENT: physical_q2 */
	__le16 irq_cons;	/* HSI_COMMENT: word3 */
	__le16 irq_prod;	/* HSI_COMMENT: word4 */
	__le16 e5_reserved1;	/* HSI_COMMENT: word5 */
	__le16 conn_dpi;	/* HSI_COMMENT: conn_dpi */
	u8 rxmit_opcode;	/* HSI_COMMENT: byte3 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	u8 byte5;		/* HSI_COMMENT: byte5 */
	u8 byte6;		/* HSI_COMMENT: byte6 */
	__le32 rxmit_psn_and_id;	/* HSI_COMMENT: reg0 */
	__le32 rxmit_bytes_length;	/* HSI_COMMENT: reg1 */
	__le32 psn;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
	__le32 reg5;		/* HSI_COMMENT: cf_array0 */
	__le32 msn_and_syndrome;	/* HSI_COMMENT: cf_array1 */
};

struct ystorm_roce_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define YSTORM_ROCE_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define YSTORM_ROCE_CONN_AG_CTX_BIT0_SHIFT		0
#define YSTORM_ROCE_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define YSTORM_ROCE_CONN_AG_CTX_BIT1_SHIFT		1
#define YSTORM_ROCE_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: cf0 */
#define YSTORM_ROCE_CONN_AG_CTX_CF0_SHIFT		2
#define YSTORM_ROCE_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: cf1 */
#define YSTORM_ROCE_CONN_AG_CTX_CF1_SHIFT		4
#define YSTORM_ROCE_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: cf2 */
#define YSTORM_ROCE_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define YSTORM_ROCE_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define YSTORM_ROCE_CONN_AG_CTX_CF0EN_SHIFT		0
#define YSTORM_ROCE_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define YSTORM_ROCE_CONN_AG_CTX_CF1EN_SHIFT		1
#define YSTORM_ROCE_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define YSTORM_ROCE_CONN_AG_CTX_CF2EN_SHIFT		2
#define YSTORM_ROCE_CONN_AG_CTX_RULE0EN_MASK		0x1	/* HSI_COMMENT: rule0en */
#define YSTORM_ROCE_CONN_AG_CTX_RULE0EN_SHIFT		3
#define YSTORM_ROCE_CONN_AG_CTX_RULE1EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define YSTORM_ROCE_CONN_AG_CTX_RULE1EN_SHIFT		4
#define YSTORM_ROCE_CONN_AG_CTX_RULE2EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define YSTORM_ROCE_CONN_AG_CTX_RULE2EN_SHIFT		5
#define YSTORM_ROCE_CONN_AG_CTX_RULE3EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define YSTORM_ROCE_CONN_AG_CTX_RULE3EN_SHIFT		6
#define YSTORM_ROCE_CONN_AG_CTX_RULE4EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define YSTORM_ROCE_CONN_AG_CTX_RULE4EN_SHIFT		7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: word0 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le16 word4;		/* HSI_COMMENT: word4 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
};

struct ystorm_roce_req_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define YSTORM_ROCE_REQ_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define YSTORM_ROCE_REQ_CONN_AG_CTX_BIT0_SHIFT		0
#define YSTORM_ROCE_REQ_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define YSTORM_ROCE_REQ_CONN_AG_CTX_BIT1_SHIFT		1
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: cf0 */
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF0_SHIFT		2
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: cf1 */
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF1_SHIFT		4
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: cf2 */
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF0EN_SHIFT		0
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF1EN_SHIFT		1
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF2EN_SHIFT		2
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_MASK	0x1	/* HSI_COMMENT: rule0en */
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_SHIFT	3
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_MASK	0x1	/* HSI_COMMENT: rule1en */
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_SHIFT	4
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE2EN_MASK	0x1	/* HSI_COMMENT: rule2en */
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE2EN_SHIFT	5
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_MASK	0x1	/* HSI_COMMENT: rule3en */
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_SHIFT	6
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_MASK	0x1	/* HSI_COMMENT: rule4en */
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_SHIFT	7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: word0 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le16 word4;		/* HSI_COMMENT: word4 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
};

struct ystorm_roce_resp_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define YSTORM_ROCE_RESP_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define YSTORM_ROCE_RESP_CONN_AG_CTX_BIT0_SHIFT		0
#define YSTORM_ROCE_RESP_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define YSTORM_ROCE_RESP_CONN_AG_CTX_BIT1_SHIFT		1
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: cf0 */
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF0_SHIFT		2
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: cf1 */
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF1_SHIFT		4
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: cf2 */
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_SHIFT	0
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF1EN_SHIFT	1
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF2EN_SHIFT	2
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_MASK	0x1	/* HSI_COMMENT: rule0en */
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_SHIFT	3
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_MASK	0x1	/* HSI_COMMENT: rule1en */
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_SHIFT	4
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_MASK	0x1	/* HSI_COMMENT: rule2en */
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_SHIFT	5
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_MASK	0x1	/* HSI_COMMENT: rule3en */
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_SHIFT	6
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_MASK	0x1	/* HSI_COMMENT: rule4en */
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_SHIFT	7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: word0 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le16 word4;		/* HSI_COMMENT: word4 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
};

/************************************************************************/
/* Add include to qed hsi rdma target for both roce and iwarp qed driver */
/************************************************************************/
/************************************************************************/
/* Add include to common TCP target */
/************************************************************************/

/************************************************************************/
/* Add include to common iwarp target for both eCore and protocol iwarp driver */
/************************************************************************/

/* The iwarp storm context of Ystorm */
struct ystorm_iwarp_conn_st_ctx {
	__le32 reserved[4];
};

/* The iwarp storm context of Pstorm */
struct pstorm_iwarp_conn_st_ctx {
	__le32 reserved[36];
};

/* The iwarp storm context of Xstorm */
struct xstorm_iwarp_conn_st_ctx {
	__le32 reserved[48];
};

struct xstorm_iwarp_conn_ag_ctx {
	u8 reserved0;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define XSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM0_MASK \
	        0x1		/* HSI_COMMENT: exist_in_qm0 */
#define XSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM0_SHIFT \
	        0
#define XSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM1_MASK \
	        0x1		/* HSI_COMMENT: exist_in_qm1 */
#define XSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM1_SHIFT \
	        1
#define XSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM2_MASK \
	        0x1		/* HSI_COMMENT: exist_in_qm2 */
#define XSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM2_SHIFT \
	        2
#define XSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM3_MASK \
	        0x1		/* HSI_COMMENT: exist_in_qm3 */
#define XSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM3_SHIFT \
	        3
#define XSTORM_IWARP_CONN_AG_CTX_BIT4_MASK \
	        0x1		/* HSI_COMMENT: bit4 */
#define XSTORM_IWARP_CONN_AG_CTX_BIT4_SHIFT \
	        4
#define XSTORM_IWARP_CONN_AG_CTX_RESERVED2_MASK \
	        0x1		/* HSI_COMMENT: cf_array_active */
#define XSTORM_IWARP_CONN_AG_CTX_RESERVED2_SHIFT \
	        5
#define XSTORM_IWARP_CONN_AG_CTX_BIT6_MASK \
	        0x1		/* HSI_COMMENT: bit6 */
#define XSTORM_IWARP_CONN_AG_CTX_BIT6_SHIFT \
	        6
#define XSTORM_IWARP_CONN_AG_CTX_BIT7_MASK \
	        0x1		/* HSI_COMMENT: bit7 */
#define XSTORM_IWARP_CONN_AG_CTX_BIT7_SHIFT \
	        7
	u8 flags1;
#define XSTORM_IWARP_CONN_AG_CTX_BIT8_MASK \
	        0x1		/* HSI_COMMENT: bit8 */
#define XSTORM_IWARP_CONN_AG_CTX_BIT8_SHIFT \
	        0
#define XSTORM_IWARP_CONN_AG_CTX_BIT9_MASK \
	        0x1		/* HSI_COMMENT: bit9 */
#define XSTORM_IWARP_CONN_AG_CTX_BIT9_SHIFT \
	        1
#define XSTORM_IWARP_CONN_AG_CTX_BIT10_MASK \
	        0x1		/* HSI_COMMENT: bit10 */
#define XSTORM_IWARP_CONN_AG_CTX_BIT10_SHIFT \
	        2
#define XSTORM_IWARP_CONN_AG_CTX_BIT11_MASK \
	        0x1		/* HSI_COMMENT: bit11 */
#define XSTORM_IWARP_CONN_AG_CTX_BIT11_SHIFT \
	        3
#define XSTORM_IWARP_CONN_AG_CTX_BIT12_MASK \
	        0x1		/* HSI_COMMENT: bit12 */
#define XSTORM_IWARP_CONN_AG_CTX_BIT12_SHIFT \
	        4
#define XSTORM_IWARP_CONN_AG_CTX_BIT13_MASK \
	        0x1		/* HSI_COMMENT: bit13 */
#define XSTORM_IWARP_CONN_AG_CTX_BIT13_SHIFT \
	        5
#define XSTORM_IWARP_CONN_AG_CTX_BIT14_MASK \
	        0x1		/* HSI_COMMENT: bit14 */
#define XSTORM_IWARP_CONN_AG_CTX_BIT14_SHIFT \
	        6
#define XSTORM_IWARP_CONN_AG_CTX_YSTORM_FLUSH_OR_REWIND_SND_MAX_MASK \
	        0x1		/* HSI_COMMENT: bit15 */
#define XSTORM_IWARP_CONN_AG_CTX_YSTORM_FLUSH_OR_REWIND_SND_MAX_SHIFT \
	        7
	u8 flags2;
#define XSTORM_IWARP_CONN_AG_CTX_CF0_MASK \
	        0x3		/* HSI_COMMENT: timer0cf */
#define XSTORM_IWARP_CONN_AG_CTX_CF0_SHIFT \
	        0
#define XSTORM_IWARP_CONN_AG_CTX_CF1_MASK \
	        0x3		/* HSI_COMMENT: timer1cf */
#define XSTORM_IWARP_CONN_AG_CTX_CF1_SHIFT \
	        2
#define XSTORM_IWARP_CONN_AG_CTX_CF2_MASK \
	        0x3		/* HSI_COMMENT: timer2cf */
#define XSTORM_IWARP_CONN_AG_CTX_CF2_SHIFT \
	        4
#define XSTORM_IWARP_CONN_AG_CTX_TIMER_STOP_ALL_MASK \
	        0x3		/* HSI_COMMENT: timer_stop_all */
#define XSTORM_IWARP_CONN_AG_CTX_TIMER_STOP_ALL_SHIFT \
	        6
	u8 flags3;
#define XSTORM_IWARP_CONN_AG_CTX_CF4_MASK \
	        0x3		/* HSI_COMMENT: cf4 */
#define XSTORM_IWARP_CONN_AG_CTX_CF4_SHIFT \
	        0
#define XSTORM_IWARP_CONN_AG_CTX_CF5_MASK \
	        0x3		/* HSI_COMMENT: cf5 */
#define XSTORM_IWARP_CONN_AG_CTX_CF5_SHIFT \
	        2
#define XSTORM_IWARP_CONN_AG_CTX_CF6_MASK \
	        0x3		/* HSI_COMMENT: cf6 */
#define XSTORM_IWARP_CONN_AG_CTX_CF6_SHIFT \
	        4
#define XSTORM_IWARP_CONN_AG_CTX_CF7_MASK \
	        0x3		/* HSI_COMMENT: cf7 */
#define XSTORM_IWARP_CONN_AG_CTX_CF7_SHIFT \
	        6
	u8 flags4;
#define XSTORM_IWARP_CONN_AG_CTX_CF8_MASK \
	        0x3		/* HSI_COMMENT: cf8 */
#define XSTORM_IWARP_CONN_AG_CTX_CF8_SHIFT \
	        0
#define XSTORM_IWARP_CONN_AG_CTX_CF9_MASK \
	        0x3		/* HSI_COMMENT: cf9 */
#define XSTORM_IWARP_CONN_AG_CTX_CF9_SHIFT \
	        2
#define XSTORM_IWARP_CONN_AG_CTX_CF10_MASK \
	        0x3		/* HSI_COMMENT: cf10 */
#define XSTORM_IWARP_CONN_AG_CTX_CF10_SHIFT \
	        4
#define XSTORM_IWARP_CONN_AG_CTX_CF11_MASK \
	        0x3		/* HSI_COMMENT: cf11 */
#define XSTORM_IWARP_CONN_AG_CTX_CF11_SHIFT \
	        6
	u8 flags5;
#define XSTORM_IWARP_CONN_AG_CTX_CF12_MASK \
	        0x3		/* HSI_COMMENT: cf12 */
#define XSTORM_IWARP_CONN_AG_CTX_CF12_SHIFT \
	        0
#define XSTORM_IWARP_CONN_AG_CTX_CF13_MASK \
	        0x3		/* HSI_COMMENT: cf13 */
#define XSTORM_IWARP_CONN_AG_CTX_CF13_SHIFT \
	        2
#define XSTORM_IWARP_CONN_AG_CTX_SQ_FLUSH_CF_MASK \
	        0x3		/* HSI_COMMENT: cf14 */
#define XSTORM_IWARP_CONN_AG_CTX_SQ_FLUSH_CF_SHIFT \
	        4
#define XSTORM_IWARP_CONN_AG_CTX_CF15_MASK \
	        0x3		/* HSI_COMMENT: cf15 */
#define XSTORM_IWARP_CONN_AG_CTX_CF15_SHIFT \
	        6
	u8 flags6;
#define XSTORM_IWARP_CONN_AG_CTX_MPA_OR_ERROR_WAKEUP_TRIGGER_CF_MASK \
	        0x3		/* HSI_COMMENT: cf16 */
#define XSTORM_IWARP_CONN_AG_CTX_MPA_OR_ERROR_WAKEUP_TRIGGER_CF_SHIFT \
	        0
#define XSTORM_IWARP_CONN_AG_CTX_CF17_MASK \
	        0x3		/* HSI_COMMENT: cf_array_cf */
#define XSTORM_IWARP_CONN_AG_CTX_CF17_SHIFT \
	        2
#define XSTORM_IWARP_CONN_AG_CTX_CF18_MASK \
	        0x3		/* HSI_COMMENT: cf18 */
#define XSTORM_IWARP_CONN_AG_CTX_CF18_SHIFT \
	        4
#define XSTORM_IWARP_CONN_AG_CTX_DQ_FLUSH_MASK \
	        0x3		/* HSI_COMMENT: cf19 */
#define XSTORM_IWARP_CONN_AG_CTX_DQ_FLUSH_SHIFT \
	        6
	u8 flags7;
#define XSTORM_IWARP_CONN_AG_CTX_FLUSH_Q0_MASK \
	        0x3		/* HSI_COMMENT: cf20 */
#define XSTORM_IWARP_CONN_AG_CTX_FLUSH_Q0_SHIFT \
	        0
#define XSTORM_IWARP_CONN_AG_CTX_FLUSH_Q1_MASK \
	        0x3		/* HSI_COMMENT: cf21 */
#define XSTORM_IWARP_CONN_AG_CTX_FLUSH_Q1_SHIFT \
	        2
#define XSTORM_IWARP_CONN_AG_CTX_SLOW_PATH_MASK \
	        0x3		/* HSI_COMMENT: cf22 */
#define XSTORM_IWARP_CONN_AG_CTX_SLOW_PATH_SHIFT \
	        4
#define XSTORM_IWARP_CONN_AG_CTX_CF0EN_MASK \
	        0x1		/* HSI_COMMENT: cf0en */
#define XSTORM_IWARP_CONN_AG_CTX_CF0EN_SHIFT \
	        6
#define XSTORM_IWARP_CONN_AG_CTX_CF1EN_MASK \
	        0x1		/* HSI_COMMENT: cf1en */
#define XSTORM_IWARP_CONN_AG_CTX_CF1EN_SHIFT \
	        7
	u8 flags8;
#define XSTORM_IWARP_CONN_AG_CTX_CF2EN_MASK \
	        0x1		/* HSI_COMMENT: cf2en */
#define XSTORM_IWARP_CONN_AG_CTX_CF2EN_SHIFT \
	        0
#define XSTORM_IWARP_CONN_AG_CTX_TIMER_STOP_ALL_EN_MASK \
	        0x1		/* HSI_COMMENT: cf3en */
#define XSTORM_IWARP_CONN_AG_CTX_TIMER_STOP_ALL_EN_SHIFT \
	        1
#define XSTORM_IWARP_CONN_AG_CTX_CF4EN_MASK \
	        0x1		/* HSI_COMMENT: cf4en */
#define XSTORM_IWARP_CONN_AG_CTX_CF4EN_SHIFT \
	        2
#define XSTORM_IWARP_CONN_AG_CTX_CF5EN_MASK \
	        0x1		/* HSI_COMMENT: cf5en */
#define XSTORM_IWARP_CONN_AG_CTX_CF5EN_SHIFT \
	        3
#define XSTORM_IWARP_CONN_AG_CTX_CF6EN_MASK \
	        0x1		/* HSI_COMMENT: cf6en */
#define XSTORM_IWARP_CONN_AG_CTX_CF6EN_SHIFT \
	        4
#define XSTORM_IWARP_CONN_AG_CTX_CF7EN_MASK \
	        0x1		/* HSI_COMMENT: cf7en */
#define XSTORM_IWARP_CONN_AG_CTX_CF7EN_SHIFT \
	        5
#define XSTORM_IWARP_CONN_AG_CTX_CF8EN_MASK \
	        0x1		/* HSI_COMMENT: cf8en */
#define XSTORM_IWARP_CONN_AG_CTX_CF8EN_SHIFT \
	        6
#define XSTORM_IWARP_CONN_AG_CTX_CF9EN_MASK \
	        0x1		/* HSI_COMMENT: cf9en */
#define XSTORM_IWARP_CONN_AG_CTX_CF9EN_SHIFT \
	        7
	u8 flags9;
#define XSTORM_IWARP_CONN_AG_CTX_CF10EN_MASK \
	        0x1		/* HSI_COMMENT: cf10en */
#define XSTORM_IWARP_CONN_AG_CTX_CF10EN_SHIFT \
	        0
#define XSTORM_IWARP_CONN_AG_CTX_CF11EN_MASK \
	        0x1		/* HSI_COMMENT: cf11en */
#define XSTORM_IWARP_CONN_AG_CTX_CF11EN_SHIFT \
	        1
#define XSTORM_IWARP_CONN_AG_CTX_CF12EN_MASK \
	        0x1		/* HSI_COMMENT: cf12en */
#define XSTORM_IWARP_CONN_AG_CTX_CF12EN_SHIFT \
	        2
#define XSTORM_IWARP_CONN_AG_CTX_CF13EN_MASK \
	        0x1		/* HSI_COMMENT: cf13en */
#define XSTORM_IWARP_CONN_AG_CTX_CF13EN_SHIFT \
	        3
#define XSTORM_IWARP_CONN_AG_CTX_SQ_FLUSH_CF_EN_MASK \
	        0x1		/* HSI_COMMENT: cf14en */
#define XSTORM_IWARP_CONN_AG_CTX_SQ_FLUSH_CF_EN_SHIFT \
	        4
#define XSTORM_IWARP_CONN_AG_CTX_CF15EN_MASK \
	        0x1		/* HSI_COMMENT: cf15en */
#define XSTORM_IWARP_CONN_AG_CTX_CF15EN_SHIFT \
	        5
#define XSTORM_IWARP_CONN_AG_CTX_MPA_OR_ERROR_WAKEUP_TRIGGER_CF_EN_MASK \
	        0x1		/* HSI_COMMENT: cf16en */
#define XSTORM_IWARP_CONN_AG_CTX_MPA_OR_ERROR_WAKEUP_TRIGGER_CF_EN_SHIFT \
	        6
#define XSTORM_IWARP_CONN_AG_CTX_CF17EN_MASK \
	        0x1		/* HSI_COMMENT: cf_array_cf_en */
#define XSTORM_IWARP_CONN_AG_CTX_CF17EN_SHIFT \
	        7
	u8 flags10;
#define XSTORM_IWARP_CONN_AG_CTX_CF18EN_MASK \
	        0x1		/* HSI_COMMENT: cf18en */
#define XSTORM_IWARP_CONN_AG_CTX_CF18EN_SHIFT \
	        0
#define XSTORM_IWARP_CONN_AG_CTX_DQ_FLUSH_EN_MASK \
	        0x1		/* HSI_COMMENT: cf19en */
#define XSTORM_IWARP_CONN_AG_CTX_DQ_FLUSH_EN_SHIFT \
	        1
#define XSTORM_IWARP_CONN_AG_CTX_FLUSH_Q0_EN_MASK \
	        0x1		/* HSI_COMMENT: cf20en */
#define XSTORM_IWARP_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT \
	        2
#define XSTORM_IWARP_CONN_AG_CTX_FLUSH_Q1_EN_MASK \
	        0x1		/* HSI_COMMENT: cf21en */
#define XSTORM_IWARP_CONN_AG_CTX_FLUSH_Q1_EN_SHIFT \
	        3
#define XSTORM_IWARP_CONN_AG_CTX_SLOW_PATH_EN_MASK \
	        0x1		/* HSI_COMMENT: cf22en */
#define XSTORM_IWARP_CONN_AG_CTX_SLOW_PATH_EN_SHIFT \
	        4
#define XSTORM_IWARP_CONN_AG_CTX_SEND_TERMINATE_CF_EN_MASK \
	        0x1		/* HSI_COMMENT: cf23en */
#define XSTORM_IWARP_CONN_AG_CTX_SEND_TERMINATE_CF_EN_SHIFT \
	        5
#define XSTORM_IWARP_CONN_AG_CTX_RULE0EN_MASK \
	        0x1		/* HSI_COMMENT: rule0en */
#define XSTORM_IWARP_CONN_AG_CTX_RULE0EN_SHIFT \
	        6
#define XSTORM_IWARP_CONN_AG_CTX_MORE_TO_SEND_RULE_EN_MASK \
	        0x1		/* HSI_COMMENT: rule1en */
#define XSTORM_IWARP_CONN_AG_CTX_MORE_TO_SEND_RULE_EN_SHIFT \
	        7
	u8 flags11;
#define XSTORM_IWARP_CONN_AG_CTX_TX_BLOCKED_EN_MASK \
	        0x1		/* HSI_COMMENT: rule2en */
#define XSTORM_IWARP_CONN_AG_CTX_TX_BLOCKED_EN_SHIFT \
	        0
#define XSTORM_IWARP_CONN_AG_CTX_RULE3EN_MASK \
	        0x1		/* HSI_COMMENT: rule3en */
#define XSTORM_IWARP_CONN_AG_CTX_RULE3EN_SHIFT \
	        1
#define XSTORM_IWARP_CONN_AG_CTX_RESERVED3_MASK \
	        0x1		/* HSI_COMMENT: rule4en */
#define XSTORM_IWARP_CONN_AG_CTX_RESERVED3_SHIFT \
	        2
#define XSTORM_IWARP_CONN_AG_CTX_RULE5EN_MASK \
	        0x1		/* HSI_COMMENT: rule5en */
#define XSTORM_IWARP_CONN_AG_CTX_RULE5EN_SHIFT \
	        3
#define XSTORM_IWARP_CONN_AG_CTX_RULE6EN_MASK \
	        0x1		/* HSI_COMMENT: rule6en */
#define XSTORM_IWARP_CONN_AG_CTX_RULE6EN_SHIFT \
	        4
#define XSTORM_IWARP_CONN_AG_CTX_RULE7EN_MASK \
	        0x1		/* HSI_COMMENT: rule7en */
#define XSTORM_IWARP_CONN_AG_CTX_RULE7EN_SHIFT \
	        5
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED1_MASK \
	        0x1		/* HSI_COMMENT: rule8en */
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED1_SHIFT \
	        6
#define XSTORM_IWARP_CONN_AG_CTX_RULE9EN_MASK \
	        0x1		/* HSI_COMMENT: rule9en */
#define XSTORM_IWARP_CONN_AG_CTX_RULE9EN_SHIFT \
	        7
	u8 flags12;
#define XSTORM_IWARP_CONN_AG_CTX_SQ_NOT_EMPTY_RULE_EN_MASK \
	        0x1		/* HSI_COMMENT: rule10en */
#define XSTORM_IWARP_CONN_AG_CTX_SQ_NOT_EMPTY_RULE_EN_SHIFT \
	        0
#define XSTORM_IWARP_CONN_AG_CTX_RULE11EN_MASK \
	        0x1		/* HSI_COMMENT: rule11en */
#define XSTORM_IWARP_CONN_AG_CTX_RULE11EN_SHIFT \
	        1
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED2_MASK \
	        0x1		/* HSI_COMMENT: rule12en */
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED2_SHIFT \
	        2
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED3_MASK \
	        0x1		/* HSI_COMMENT: rule13en */
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED3_SHIFT \
	        3
#define XSTORM_IWARP_CONN_AG_CTX_SQ_FENCE_RULE_EN_MASK \
	        0x1		/* HSI_COMMENT: rule14en */
#define XSTORM_IWARP_CONN_AG_CTX_SQ_FENCE_RULE_EN_SHIFT \
	        4
#define XSTORM_IWARP_CONN_AG_CTX_RULE15EN_MASK \
	        0x1		/* HSI_COMMENT: rule15en */
#define XSTORM_IWARP_CONN_AG_CTX_RULE15EN_SHIFT \
	        5
#define XSTORM_IWARP_CONN_AG_CTX_RULE16EN_MASK \
	        0x1		/* HSI_COMMENT: rule16en */
#define XSTORM_IWARP_CONN_AG_CTX_RULE16EN_SHIFT \
	        6
#define XSTORM_IWARP_CONN_AG_CTX_RULE17EN_MASK \
	        0x1		/* HSI_COMMENT: rule17en */
#define XSTORM_IWARP_CONN_AG_CTX_RULE17EN_SHIFT \
	        7
	u8 flags13;
#define XSTORM_IWARP_CONN_AG_CTX_IRQ_NOT_EMPTY_RULE_EN_MASK \
	        0x1		/* HSI_COMMENT: rule18en */
#define XSTORM_IWARP_CONN_AG_CTX_IRQ_NOT_EMPTY_RULE_EN_SHIFT \
	        0
#define XSTORM_IWARP_CONN_AG_CTX_HQ_NOT_FULL_RULE_EN_MASK \
	        0x1		/* HSI_COMMENT: rule19en */
#define XSTORM_IWARP_CONN_AG_CTX_HQ_NOT_FULL_RULE_EN_SHIFT \
	        1
#define XSTORM_IWARP_CONN_AG_CTX_ORQ_RD_FENCE_RULE_EN_MASK \
	        0x1		/* HSI_COMMENT: rule20en */
#define XSTORM_IWARP_CONN_AG_CTX_ORQ_RD_FENCE_RULE_EN_SHIFT \
	        2
#define XSTORM_IWARP_CONN_AG_CTX_RULE21EN_MASK \
	        0x1		/* HSI_COMMENT: rule21en */
#define XSTORM_IWARP_CONN_AG_CTX_RULE21EN_SHIFT \
	        3
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED6_MASK \
	        0x1		/* HSI_COMMENT: rule22en */
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED6_SHIFT \
	        4
#define XSTORM_IWARP_CONN_AG_CTX_ORQ_NOT_FULL_RULE_EN_MASK \
	        0x1		/* HSI_COMMENT: rule23en */
#define XSTORM_IWARP_CONN_AG_CTX_ORQ_NOT_FULL_RULE_EN_SHIFT \
	        5
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED8_MASK \
	        0x1		/* HSI_COMMENT: rule24en */
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED8_SHIFT \
	        6
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED9_MASK \
	        0x1		/* HSI_COMMENT: rule25en */
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED9_SHIFT \
	        7
	u8 flags14;
#define XSTORM_IWARP_CONN_AG_CTX_BIT16_MASK \
	        0x1		/* HSI_COMMENT: bit16 */
#define XSTORM_IWARP_CONN_AG_CTX_BIT16_SHIFT \
	        0
#define XSTORM_IWARP_CONN_AG_CTX_BIT17_MASK \
	        0x1		/* HSI_COMMENT: bit17 */
#define XSTORM_IWARP_CONN_AG_CTX_BIT17_SHIFT \
	        1
#define XSTORM_IWARP_CONN_AG_CTX_BIT18_MASK \
	        0x1		/* HSI_COMMENT: bit18 */
#define XSTORM_IWARP_CONN_AG_CTX_BIT18_SHIFT \
	        2
#define XSTORM_IWARP_CONN_AG_CTX_E5_RESERVED1_MASK \
	        0x1		/* HSI_COMMENT: bit19 */
#define XSTORM_IWARP_CONN_AG_CTX_E5_RESERVED1_SHIFT \
	        3
#define XSTORM_IWARP_CONN_AG_CTX_E5_RESERVED2_MASK \
	        0x1		/* HSI_COMMENT: bit20 */
#define XSTORM_IWARP_CONN_AG_CTX_E5_RESERVED2_SHIFT \
	        4
#define XSTORM_IWARP_CONN_AG_CTX_E5_RESERVED3_MASK \
	        0x1		/* HSI_COMMENT: bit21 */
#define XSTORM_IWARP_CONN_AG_CTX_E5_RESERVED3_SHIFT \
	        5
#define XSTORM_IWARP_CONN_AG_CTX_SEND_TERMINATE_CF_MASK \
	        0x3		/* HSI_COMMENT: cf23 */
#define XSTORM_IWARP_CONN_AG_CTX_SEND_TERMINATE_CF_SHIFT \
	        6
	u8 byte2;		/* HSI_COMMENT: byte2 */
	__le16 physical_q0;	/* HSI_COMMENT: physical_q0 */
	__le16 physical_q1;	/* HSI_COMMENT: physical_q1 */
	__le16 sq_comp_cons;	/* HSI_COMMENT: physical_q2 */
	__le16 sq_tx_cons;	/* HSI_COMMENT: word3 */
	__le16 sq_prod;		/* HSI_COMMENT: word4 */
	__le16 word5;		/* HSI_COMMENT: word5 */
	__le16 conn_dpi;	/* HSI_COMMENT: conn_dpi */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	u8 byte5;		/* HSI_COMMENT: byte5 */
	u8 byte6;		/* HSI_COMMENT: byte6 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 more_to_send_seq;	/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
	__le32 rewinded_snd_max_or_term_opcode;	/* HSI_COMMENT: cf_array0 */
	__le32 rd_msn;		/* HSI_COMMENT: cf_array1 */
	__le16 irq_prod_via_msdm;	/* HSI_COMMENT: word7 */
	__le16 irq_cons;	/* HSI_COMMENT: word8 */
	__le16 hq_cons_th_or_mpa_data;	/* HSI_COMMENT: word9 */
	__le16 hq_cons;		/* HSI_COMMENT: word10 */
	__le32 atom_msn;	/* HSI_COMMENT: reg7 */
	__le32 orq_cons;	/* HSI_COMMENT: reg8 */
	__le32 orq_cons_th;	/* HSI_COMMENT: reg9 */
	u8 byte7;		/* HSI_COMMENT: byte7 */
	u8 wqe_data_pad_bytes;	/* HSI_COMMENT: byte8 */
	u8 max_ord;		/* HSI_COMMENT: byte9 */
	u8 former_hq_prod;	/* HSI_COMMENT: byte10 */
	u8 irq_prod_via_msem;	/* HSI_COMMENT: byte11 */
	u8 byte12;		/* HSI_COMMENT: byte12 */
	u8 max_pkt_pdu_size_lo;	/* HSI_COMMENT: byte13 */
	u8 max_pkt_pdu_size_hi;	/* HSI_COMMENT: byte14 */
	u8 byte15;		/* HSI_COMMENT: byte15 */
	u8 e5_reserved;		/* HSI_COMMENT: e5_reserved */
	__le16 e5_reserved4;	/* HSI_COMMENT: word11 */
	__le32 reg10;		/* HSI_COMMENT: reg10 */
	__le32 reg11;		/* HSI_COMMENT: reg11 */
	__le32 shared_queue_page_addr_lo;	/* HSI_COMMENT: reg12 */
	__le32 shared_queue_page_addr_hi;	/* HSI_COMMENT: reg13 */
	__le32 reg14;		/* HSI_COMMENT: reg14 */
	__le32 reg15;		/* HSI_COMMENT: reg15 */
	__le32 reg16;		/* HSI_COMMENT: reg16 */
	__le32 reg17;		/* HSI_COMMENT: reg17 */
};

struct tstorm_iwarp_conn_ag_ctx {
	u8 reserved0;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define TSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM0_MASK \
	        0x1		/* HSI_COMMENT: exist_in_qm0 */
#define TSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM0_SHIFT \
	        0
#define TSTORM_IWARP_CONN_AG_CTX_BIT1_MASK \
	        0x1		/* HSI_COMMENT: exist_in_qm1 */
#define TSTORM_IWARP_CONN_AG_CTX_BIT1_SHIFT \
	        1
#define TSTORM_IWARP_CONN_AG_CTX_BIT2_MASK \
	        0x1		/* HSI_COMMENT: bit2 */
#define TSTORM_IWARP_CONN_AG_CTX_BIT2_SHIFT \
	        2
#define TSTORM_IWARP_CONN_AG_CTX_MSTORM_FLUSH_OR_TERMINATE_SENT_MASK \
	        0x1		/* HSI_COMMENT: bit3 */
#define TSTORM_IWARP_CONN_AG_CTX_MSTORM_FLUSH_OR_TERMINATE_SENT_SHIFT \
	        3
#define TSTORM_IWARP_CONN_AG_CTX_BIT4_MASK \
	        0x1		/* HSI_COMMENT: bit4 */
#define TSTORM_IWARP_CONN_AG_CTX_BIT4_SHIFT \
	        4
#define TSTORM_IWARP_CONN_AG_CTX_CACHED_ORQ_MASK \
	        0x1		/* HSI_COMMENT: bit5 */
#define TSTORM_IWARP_CONN_AG_CTX_CACHED_ORQ_SHIFT \
	        5
#define TSTORM_IWARP_CONN_AG_CTX_CF0_MASK \
	        0x3		/* HSI_COMMENT: timer0cf */
#define TSTORM_IWARP_CONN_AG_CTX_CF0_SHIFT \
	        6
	u8 flags1;
#define TSTORM_IWARP_CONN_AG_CTX_RQ_POST_CF_MASK \
	        0x3		/* HSI_COMMENT: timer1cf */
#define TSTORM_IWARP_CONN_AG_CTX_RQ_POST_CF_SHIFT \
	        0
#define TSTORM_IWARP_CONN_AG_CTX_MPA_TIMEOUT_CF_MASK \
	        0x3		/* HSI_COMMENT: timer2cf */
#define TSTORM_IWARP_CONN_AG_CTX_MPA_TIMEOUT_CF_SHIFT \
	        2
#define TSTORM_IWARP_CONN_AG_CTX_TIMER_STOP_ALL_MASK \
	        0x3		/* HSI_COMMENT: timer_stop_all */
#define TSTORM_IWARP_CONN_AG_CTX_TIMER_STOP_ALL_SHIFT \
	        4
#define TSTORM_IWARP_CONN_AG_CTX_CF4_MASK \
	        0x3		/* HSI_COMMENT: cf4 */
#define TSTORM_IWARP_CONN_AG_CTX_CF4_SHIFT \
	        6
	u8 flags2;
#define TSTORM_IWARP_CONN_AG_CTX_CF5_MASK \
	        0x3		/* HSI_COMMENT: cf5 */
#define TSTORM_IWARP_CONN_AG_CTX_CF5_SHIFT \
	        0
#define TSTORM_IWARP_CONN_AG_CTX_CF6_MASK \
	        0x3		/* HSI_COMMENT: cf6 */
#define TSTORM_IWARP_CONN_AG_CTX_CF6_SHIFT \
	        2
#define TSTORM_IWARP_CONN_AG_CTX_CF7_MASK \
	        0x3		/* HSI_COMMENT: cf7 */
#define TSTORM_IWARP_CONN_AG_CTX_CF7_SHIFT \
	        4
#define TSTORM_IWARP_CONN_AG_CTX_CF8_MASK \
	        0x3		/* HSI_COMMENT: cf8 */
#define TSTORM_IWARP_CONN_AG_CTX_CF8_SHIFT \
	        6
	u8 flags3;
#define TSTORM_IWARP_CONN_AG_CTX_FLUSH_Q0_AND_TCP_HANDSHAKE_COMPLETE_MASK \
	        0x3		/* HSI_COMMENT: cf9 */
#define TSTORM_IWARP_CONN_AG_CTX_FLUSH_Q0_AND_TCP_HANDSHAKE_COMPLETE_SHIFT \
	        0
#define TSTORM_IWARP_CONN_AG_CTX_FLUSH_OR_ERROR_DETECTED_MASK \
	        0x3		/* HSI_COMMENT: cf10 */
#define TSTORM_IWARP_CONN_AG_CTX_FLUSH_OR_ERROR_DETECTED_SHIFT \
	        2
#define TSTORM_IWARP_CONN_AG_CTX_CF0EN_MASK \
	        0x1		/* HSI_COMMENT: cf0en */
#define TSTORM_IWARP_CONN_AG_CTX_CF0EN_SHIFT \
	        4
#define TSTORM_IWARP_CONN_AG_CTX_RQ_POST_CF_EN_MASK \
	        0x1		/* HSI_COMMENT: cf1en */
#define TSTORM_IWARP_CONN_AG_CTX_RQ_POST_CF_EN_SHIFT \
	        5
#define TSTORM_IWARP_CONN_AG_CTX_MPA_TIMEOUT_CF_EN_MASK \
	        0x1		/* HSI_COMMENT: cf2en */
#define TSTORM_IWARP_CONN_AG_CTX_MPA_TIMEOUT_CF_EN_SHIFT \
	        6
#define TSTORM_IWARP_CONN_AG_CTX_TIMER_STOP_ALL_EN_MASK \
	        0x1		/* HSI_COMMENT: cf3en */
#define TSTORM_IWARP_CONN_AG_CTX_TIMER_STOP_ALL_EN_SHIFT \
	        7
	u8 flags4;
#define TSTORM_IWARP_CONN_AG_CTX_CF4EN_MASK \
	        0x1		/* HSI_COMMENT: cf4en */
#define TSTORM_IWARP_CONN_AG_CTX_CF4EN_SHIFT \
	        0
#define TSTORM_IWARP_CONN_AG_CTX_CF5EN_MASK \
	        0x1		/* HSI_COMMENT: cf5en */
#define TSTORM_IWARP_CONN_AG_CTX_CF5EN_SHIFT \
	        1
#define TSTORM_IWARP_CONN_AG_CTX_CF6EN_MASK \
	        0x1		/* HSI_COMMENT: cf6en */
#define TSTORM_IWARP_CONN_AG_CTX_CF6EN_SHIFT \
	        2
#define TSTORM_IWARP_CONN_AG_CTX_CF7EN_MASK \
	        0x1		/* HSI_COMMENT: cf7en */
#define TSTORM_IWARP_CONN_AG_CTX_CF7EN_SHIFT \
	        3
#define TSTORM_IWARP_CONN_AG_CTX_CF8EN_MASK \
	        0x1		/* HSI_COMMENT: cf8en */
#define TSTORM_IWARP_CONN_AG_CTX_CF8EN_SHIFT \
	        4
#define TSTORM_IWARP_CONN_AG_CTX_FLUSH_Q0_AND_TCP_HANDSHAKE_COMPLETE_EN_MASK \
	        0x1		/* HSI_COMMENT: cf9en */
#define TSTORM_IWARP_CONN_AG_CTX_FLUSH_Q0_AND_TCP_HANDSHAKE_COMPLETE_EN_SHIFT \
	        5
#define TSTORM_IWARP_CONN_AG_CTX_FLUSH_OR_ERROR_DETECTED_EN_MASK \
	        0x1		/* HSI_COMMENT: cf10en */
#define TSTORM_IWARP_CONN_AG_CTX_FLUSH_OR_ERROR_DETECTED_EN_SHIFT \
	        6
#define TSTORM_IWARP_CONN_AG_CTX_RULE0EN_MASK \
	        0x1		/* HSI_COMMENT: rule0en */
#define TSTORM_IWARP_CONN_AG_CTX_RULE0EN_SHIFT \
	        7
	u8 flags5;
#define TSTORM_IWARP_CONN_AG_CTX_RULE1EN_MASK \
	        0x1		/* HSI_COMMENT: rule1en */
#define TSTORM_IWARP_CONN_AG_CTX_RULE1EN_SHIFT \
	        0
#define TSTORM_IWARP_CONN_AG_CTX_RULE2EN_MASK \
	        0x1		/* HSI_COMMENT: rule2en */
#define TSTORM_IWARP_CONN_AG_CTX_RULE2EN_SHIFT \
	        1
#define TSTORM_IWARP_CONN_AG_CTX_RULE3EN_MASK \
	        0x1		/* HSI_COMMENT: rule3en */
#define TSTORM_IWARP_CONN_AG_CTX_RULE3EN_SHIFT \
	        2
#define TSTORM_IWARP_CONN_AG_CTX_RULE4EN_MASK \
	        0x1		/* HSI_COMMENT: rule4en */
#define TSTORM_IWARP_CONN_AG_CTX_RULE4EN_SHIFT \
	        3
#define TSTORM_IWARP_CONN_AG_CTX_RULE5EN_MASK \
	        0x1		/* HSI_COMMENT: rule5en */
#define TSTORM_IWARP_CONN_AG_CTX_RULE5EN_SHIFT \
	        4
#define TSTORM_IWARP_CONN_AG_CTX_SND_SQ_CONS_RULE_MASK \
	        0x1		/* HSI_COMMENT: rule6en */
#define TSTORM_IWARP_CONN_AG_CTX_SND_SQ_CONS_RULE_SHIFT \
	        5
#define TSTORM_IWARP_CONN_AG_CTX_RULE7EN_MASK \
	        0x1		/* HSI_COMMENT: rule7en */
#define TSTORM_IWARP_CONN_AG_CTX_RULE7EN_SHIFT \
	        6
#define TSTORM_IWARP_CONN_AG_CTX_RULE8EN_MASK \
	        0x1		/* HSI_COMMENT: rule8en */
#define TSTORM_IWARP_CONN_AG_CTX_RULE8EN_SHIFT \
	        7
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 unaligned_nxt_seq;	/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
	__le32 reg5;		/* HSI_COMMENT: reg5 */
	__le32 reg6;		/* HSI_COMMENT: reg6 */
	__le32 reg7;		/* HSI_COMMENT: reg7 */
	__le32 reg8;		/* HSI_COMMENT: reg8 */
	u8 orq_cache_idx;	/* HSI_COMMENT: byte2 */
	u8 hq_prod;		/* HSI_COMMENT: byte3 */
	__le16 sq_tx_cons_th;	/* HSI_COMMENT: word0 */
	u8 orq_prod;		/* HSI_COMMENT: byte4 */
	u8 irq_cons;		/* HSI_COMMENT: byte5 */
	__le16 sq_tx_cons;	/* HSI_COMMENT: word1 */
	__le16 conn_dpi;	/* HSI_COMMENT: conn_dpi */
	__le16 rq_prod;		/* HSI_COMMENT: word3 */
	__le32 snd_seq;		/* HSI_COMMENT: reg9 */
	__le32 last_hq_sequence;	/* HSI_COMMENT: reg10 */
};

/* The iwarp storm context of Tstorm */
struct tstorm_iwarp_conn_st_ctx {
	__le32 reserved[60];
};

/* The iwarp storm context of Mstorm */
struct mstorm_iwarp_conn_st_ctx {
	__le32 reserved[32];
};

/* The iwarp storm context of Ustorm */
struct ustorm_iwarp_conn_st_ctx {
	struct regpair reserved[14];
};

/* iwarp connection context */
struct iwarp_conn_context {
	struct ystorm_iwarp_conn_st_ctx ystorm_st_context;	/* HSI_COMMENT: ystorm storm context */
	struct regpair ystorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct pstorm_iwarp_conn_st_ctx pstorm_st_context;	/* HSI_COMMENT: pstorm storm context */
	struct regpair pstorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct xstorm_iwarp_conn_st_ctx xstorm_st_context;	/* HSI_COMMENT: xstorm storm context */
	struct xstorm_iwarp_conn_ag_ctx xstorm_ag_context;	/* HSI_COMMENT: xstorm aggregative context */
	struct tstorm_iwarp_conn_ag_ctx tstorm_ag_context;	/* HSI_COMMENT: tstorm aggregative context */
	struct timers_context timer_context;	/* HSI_COMMENT: timer context */
	struct ustorm_rdma_conn_ag_ctx ustorm_ag_context;	/* HSI_COMMENT: ustorm aggregative context */
	struct tstorm_iwarp_conn_st_ctx tstorm_st_context;	/* HSI_COMMENT: tstorm storm context */
	struct regpair tstorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct mstorm_iwarp_conn_st_ctx mstorm_st_context;	/* HSI_COMMENT: mstorm storm context */
	struct ustorm_iwarp_conn_st_ctx ustorm_st_context;	/* HSI_COMMENT: ustorm storm context */
	struct regpair ustorm_st_padding[2];	/* HSI_COMMENT: padding */
};

/* iWARP create QP params passed by driver to FW in CreateQP Request Ramrod  */
struct iwarp_create_qp_ramrod_data {
	u8 flags;
#define IWARP_CREATE_QP_RAMROD_DATA_FMR_AND_RESERVED_EN_MASK		0x1
#define IWARP_CREATE_QP_RAMROD_DATA_FMR_AND_RESERVED_EN_SHIFT		0
#define IWARP_CREATE_QP_RAMROD_DATA_SIGNALED_COMP_MASK			0x1
#define IWARP_CREATE_QP_RAMROD_DATA_SIGNALED_COMP_SHIFT			1
#define IWARP_CREATE_QP_RAMROD_DATA_RDMA_RD_EN_MASK			0x1
#define IWARP_CREATE_QP_RAMROD_DATA_RDMA_RD_EN_SHIFT			2
#define IWARP_CREATE_QP_RAMROD_DATA_RDMA_WR_EN_MASK			0x1
#define IWARP_CREATE_QP_RAMROD_DATA_RDMA_WR_EN_SHIFT			3
#define IWARP_CREATE_QP_RAMROD_DATA_ATOMIC_EN_MASK			0x1
#define IWARP_CREATE_QP_RAMROD_DATA_ATOMIC_EN_SHIFT			4
#define IWARP_CREATE_QP_RAMROD_DATA_SRQ_FLG_MASK			0x1
#define IWARP_CREATE_QP_RAMROD_DATA_SRQ_FLG_SHIFT			5
#define IWARP_CREATE_QP_RAMROD_DATA_LOW_LATENCY_QUEUE_EN_MASK		0x1
#define IWARP_CREATE_QP_RAMROD_DATA_LOW_LATENCY_QUEUE_EN_SHIFT		6
#define IWARP_CREATE_QP_RAMROD_DATA_RESERVED0_MASK			0x1
#define IWARP_CREATE_QP_RAMROD_DATA_RESERVED0_SHIFT			7
	u8 reserved1;		/* HSI_COMMENT: Basic/Enhanced (use enum mpa_negotiation_mode) */
	__le16 pd;
	__le16 sq_num_pages;
	__le16 rq_num_pages;
	__le32 reserved3[2];
	struct regpair qp_handle_for_cqe;	/* HSI_COMMENT: For use in CQEs */
	struct rdma_srq_id srq_id;
	__le32 cq_cid_for_sq;	/* HSI_COMMENT: Cid of the CQ that will be posted from SQ */
	__le32 cq_cid_for_rq;	/* HSI_COMMENT: Cid of the CQ that will be posted from RQ */
	__le16 dpi;
	__le16 physical_q0;	/* HSI_COMMENT: Physical QM queue to be tied to logical Q0 */
	__le16 physical_q1;	/* HSI_COMMENT: Physical QM queue to be tied to logical Q1 */
	u8 reserved2[6];
};

/* iWARP completion queue types */
enum iwarp_eqe_async_opcode {
	IWARP_EVENT_TYPE_ASYNC_CONNECT_COMPLETE,	/* HSI_COMMENT: Async completion after TCP 3-way handshake */
	IWARP_EVENT_TYPE_ASYNC_ENHANCED_MPA_REPLY_ARRIVED,	/* HSI_COMMENT: Enhanced MPA reply arrived. Driver should either send RTR or reject */
	IWARP_EVENT_TYPE_ASYNC_MPA_HANDSHAKE_COMPLETE,	/* HSI_COMMENT: MPA Negotiations completed */
	IWARP_EVENT_TYPE_ASYNC_CID_CLEANED,	/* HSI_COMMENT: Async completion that indicates to the driver that the CID can be re-used. */
	IWARP_EVENT_TYPE_ASYNC_EXCEPTION_DETECTED,	/* HSI_COMMENT: Async EQE indicating detection of an error/exception on a QP at Firmware */
	IWARP_EVENT_TYPE_ASYNC_QP_IN_ERROR_STATE,	/* HSI_COMMENT: Async EQE indicating QP is in Error state. */
	IWARP_EVENT_TYPE_ASYNC_CQ_OVERFLOW,	/* HSI_COMMENT: Async EQE indicating CQ, whose handle is sent with this event, has overflowed */
	IWARP_EVENT_TYPE_ASYNC_SRQ_LIMIT,	/* HSI_COMMENT: Async EQE indicating CQ, whose handle is sent with this event, has overflowed */
	IWARP_EVENT_TYPE_ASYNC_SRQ_EMPTY,	/* HSI_COMMENT: Async EQE indicating CQ, whose handle is sent with this event, has overflowed */
	MAX_IWARP_EQE_ASYNC_OPCODE
};

struct iwarp_eqe_data_mpa_async_completion {
	__le16 ulp_data_len;	/* HSI_COMMENT: On active side, length of ULP Data, from peers MPA Connect Response */
	u8 rtr_type_sent;	/* HSI_COMMENT: The type of RTR that was sent to the passive side (use enum mpa_rtr_type) */
	u8 reserved[5];
};

struct iwarp_eqe_data_tcp_async_completion {
	__le16 ulp_data_len;	/* HSI_COMMENT: On passive side, length of ULP Data, from peers active MPA Connect Request */
	u8 mpa_handshake_mode;	/* HSI_COMMENT: Negotiation type Basic/Enhanced */
	u8 reserved[5];
};

/* iWARP completion queue types */
enum iwarp_eqe_sync_opcode {
	IWARP_EVENT_TYPE_TCP_OFFLOAD = 16,	/* HSI_COMMENT: iWARP event queue response after option 2 offload Ramrod */
	IWARP_EVENT_TYPE_MPA_OFFLOAD,	/* HSI_COMMENT: Synchronous completion for MPA offload Request */
	IWARP_EVENT_TYPE_MPA_OFFLOAD_SEND_RTR,
	IWARP_EVENT_TYPE_CREATE_QP,
	IWARP_EVENT_TYPE_QUERY_QP,
	IWARP_EVENT_TYPE_MODIFY_QP,
	IWARP_EVENT_TYPE_DESTROY_QP,
	IWARP_EVENT_TYPE_ABORT_TCP_OFFLOAD,
	MAX_IWARP_EQE_SYNC_OPCODE
};

/* iWARP EQE completion status  */
enum iwarp_fw_return_code {
	IWARP_CONN_ERROR_TCP_CONNECT_INVALID_PACKET = 16,	/* HSI_COMMENT: Got invalid packet SYN/SYN-ACK */
	IWARP_CONN_ERROR_TCP_CONNECTION_RST,	/* HSI_COMMENT: Got RST during offload TCP connection  */
	IWARP_CONN_ERROR_TCP_CONNECT_TIMEOUT,	/* HSI_COMMENT: TCP connection setup timed out */
	IWARP_CONN_ERROR_MPA_ERROR_REJECT,	/* HSI_COMMENT: Got Reject in MPA reply. */
	IWARP_CONN_ERROR_MPA_NOT_SUPPORTED_VER,	/* HSI_COMMENT: Got MPA request with higher version that we support. */
	IWARP_CONN_ERROR_MPA_RST,	/* HSI_COMMENT: Got RST during MPA negotiation */
	IWARP_CONN_ERROR_MPA_FIN,	/* HSI_COMMENT: Got FIN during MPA negotiation */
	IWARP_CONN_ERROR_MPA_RTR_MISMATCH,	/* HSI_COMMENT: RTR mismatch detected when MPA reply arrived. */
	IWARP_CONN_ERROR_MPA_INSUF_IRD,	/* HSI_COMMENT: Insufficient IRD on the MPA reply that arrived. */
	IWARP_CONN_ERROR_MPA_INVALID_PACKET,	/* HSI_COMMENT: Incoming MPA packet failed on FW verifications */
	IWARP_CONN_ERROR_MPA_LOCAL_ERROR,	/* HSI_COMMENT: Detected an internal error during MPA negotiation. */
	IWARP_CONN_ERROR_MPA_TIMEOUT,	/* HSI_COMMENT: MPA negotiation timed out. */
	IWARP_CONN_ERROR_MPA_TERMINATE,	/* HSI_COMMENT: Got Terminate during MPA negotiation. */
	IWARP_QP_IN_ERROR_GOOD_CLOSE,	/* HSI_COMMENT: LLP connection was closed gracefully - Used for async IWARP_EVENT_TYPE_ASYNC_QP_IN_ERROR_STATE */
	IWARP_QP_IN_ERROR_BAD_CLOSE,	/* HSI_COMMENT: LLP Connection was closed abortively - Used for async IWARP_EVENT_TYPE_ASYNC_QP_IN_ERROR_STATE */
	IWARP_EXCEPTION_DETECTED_LLP_CLOSED,	/* HSI_COMMENT: LLP has been dissociated from the QP, although the TCP connection may not be closed yet - Used for async IWARP_EVENT_TYPE_ASYNC_EXCEPTION_DETECTED */
	IWARP_EXCEPTION_DETECTED_LLP_RESET,	/* HSI_COMMENT: LLP has Reset (either because of an RST, or a bad-close condition) - Used for async IWARP_EVENT_TYPE_ASYNC_EXCEPTION_DETECTED */
	IWARP_EXCEPTION_DETECTED_IRQ_FULL,	/* HSI_COMMENT: Peer sent more outstanding Read Requests than IRD - Used for async IWARP_EVENT_TYPE_ASYNC_EXCEPTION_DETECTED */
	IWARP_EXCEPTION_DETECTED_RQ_EMPTY,	/* HSI_COMMENT: SEND request received with RQ empty - Used for async IWARP_EVENT_TYPE_ASYNC_EXCEPTION_DETECTED */
	IWARP_EXCEPTION_DETECTED_LLP_TIMEOUT,	/* HSI_COMMENT: TCP Retransmissions timed out - Used for async IWARP_EVENT_TYPE_ASYNC_EXCEPTION_DETECTED */
	IWARP_EXCEPTION_DETECTED_REMOTE_PROTECTION_ERROR,	/* HSI_COMMENT: Peers Remote Access caused error */
	IWARP_EXCEPTION_DETECTED_CQ_OVERFLOW,	/* HSI_COMMENT: CQ overflow detected */
	IWARP_EXCEPTION_DETECTED_LOCAL_CATASTROPHIC,	/* HSI_COMMENT: Local catastrophic error detected - Used for async IWARP_EVENT_TYPE_ASYNC_EXCEPTION_DETECTED */
	IWARP_EXCEPTION_DETECTED_LOCAL_ACCESS_ERROR,	/* HSI_COMMENT: Local Access error detected while responding - Used for async IWARP_EVENT_TYPE_ASYNC_EXCEPTION_DETECTED */
	IWARP_EXCEPTION_DETECTED_REMOTE_OPERATION_ERROR,	/* HSI_COMMENT: An operation/protocol error caused by Remote Consumer */
	IWARP_EXCEPTION_DETECTED_TERMINATE_RECEIVED,	/* HSI_COMMENT: Peer sent a TERMINATE message */
	MAX_IWARP_FW_RETURN_CODE
};

/* unaligned opaque data received from LL2 */
struct iwarp_init_func_params {
	u8 ll2_ooo_q_index;	/* HSI_COMMENT: LL2 OOO queue id. The unaligned queue id will be + 1 */
	u8 reserved1[7];
};

/* iwarp func init ramrod data */
struct iwarp_init_func_ramrod_data {
	struct rdma_init_func_ramrod_data rdma;
	struct tcp_init_params tcp;
	struct iwarp_init_func_params iwarp;
};

/* iWARP QP - possible states to transition to */
enum iwarp_modify_qp_new_state_type {
	IWARP_MODIFY_QP_STATE_CLOSING = 1,	/* HSI_COMMENT: graceful close */
	IWARP_MODIFY_QP_STATE_ERROR = 2,	/* HSI_COMMENT: abortive close, if LLP connection still exists */
	MAX_IWARP_MODIFY_QP_NEW_STATE_TYPE
};

/* iwarp modify qp responder ramrod data */
struct iwarp_modify_qp_ramrod_data {
	__le16 transition_to_state;	/* HSI_COMMENT:  (use enum iwarp_modify_qp_new_state_type) */
	__le16 flags;
#define IWARP_MODIFY_QP_RAMROD_DATA_RDMA_RD_EN_MASK		0x1
#define IWARP_MODIFY_QP_RAMROD_DATA_RDMA_RD_EN_SHIFT		0
#define IWARP_MODIFY_QP_RAMROD_DATA_RDMA_WR_EN_MASK		0x1
#define IWARP_MODIFY_QP_RAMROD_DATA_RDMA_WR_EN_SHIFT		1
#define IWARP_MODIFY_QP_RAMROD_DATA_ATOMIC_EN_MASK		0x1
#define IWARP_MODIFY_QP_RAMROD_DATA_ATOMIC_EN_SHIFT		2
#define IWARP_MODIFY_QP_RAMROD_DATA_STATE_TRANS_EN_MASK		0x1	/* HSI_COMMENT: change QP state as per transition_to_state field */
#define IWARP_MODIFY_QP_RAMROD_DATA_STATE_TRANS_EN_SHIFT	3
#define IWARP_MODIFY_QP_RAMROD_DATA_RDMA_OPS_EN_FLG_MASK	0x1	/* HSI_COMMENT: If set, the rdma_rd/wr/atomic_en should be updated */
#define IWARP_MODIFY_QP_RAMROD_DATA_RDMA_OPS_EN_FLG_SHIFT	4
#define IWARP_MODIFY_QP_RAMROD_DATA_PHYSICAL_QUEUE_FLG_MASK	0x1	/* HSI_COMMENT: If set, the  physicalQ1Val/physicalQ0Val/regularLatencyPhyQueue should be updated */
#define IWARP_MODIFY_QP_RAMROD_DATA_PHYSICAL_QUEUE_FLG_SHIFT	5
#define IWARP_MODIFY_QP_RAMROD_DATA_RESERVED_MASK		0x3FF
#define IWARP_MODIFY_QP_RAMROD_DATA_RESERVED_SHIFT		6
	__le16 physical_q0;	/* HSI_COMMENT: Updated physicalQ0Val */
	__le16 physical_q1;	/* HSI_COMMENT: Updated physicalQ1Val */
	__le32 reserved1[10];
};

/* MPA params for Enhanced mode */
struct mpa_rq_params {
	__le32 ird;
	__le32 ord;
};

/* MPA host Address-Len for private data */
struct mpa_ulp_buffer {
	struct regpair addr;
	__le16 len;
	__le16 reserved[3];
};

/* iWARP MPA offload params common to Basic and Enhanced modes */
struct mpa_outgoing_params {
	u8 crc_needed;
	u8 reject;		/* HSI_COMMENT: Valid only for passive side. */
	u8 reserved[6];
	struct mpa_rq_params out_rq;
	struct mpa_ulp_buffer outgoing_ulp_buffer;	/* HSI_COMMENT: ULP buffer populated by the host */
};

/* iWARP MPA offload params passed by driver to FW in MPA Offload Request Ramrod  */
struct iwarp_mpa_offload_ramrod_data {
	struct mpa_outgoing_params common;
	__le32 tcp_cid;
	u8 mode;		/* HSI_COMMENT: Basic/Enhanced (use enum mpa_negotiation_mode) */
	u8 tcp_connect_side;	/* HSI_COMMENT: Passive/Active. use enum tcp_connect_mode */
	u8 rtr_pref;
#define IWARP_MPA_OFFLOAD_RAMROD_DATA_RTR_SUPPORTED_MASK	0x7	/* HSI_COMMENT:  (use enum mpa_rtr_type) */
#define IWARP_MPA_OFFLOAD_RAMROD_DATA_RTR_SUPPORTED_SHIFT	0
#define IWARP_MPA_OFFLOAD_RAMROD_DATA_RESERVED1_MASK		0x1F
#define IWARP_MPA_OFFLOAD_RAMROD_DATA_RESERVED1_SHIFT		3
	u8 reserved2;
	struct mpa_ulp_buffer incoming_ulp_buffer;	/* HSI_COMMENT: host buffer for placing the incoming MPA reply */
	struct regpair async_eqe_output_buf;	/* HSI_COMMENT: host buffer for async tcp/mpa completion information - must have space for at least 8 bytes */
	struct regpair handle_for_async;	/* HSI_COMMENT: a host cookie that will be echoed back with in every qp-specific async EQE */
	struct regpair shared_queue_addr;	/* HSI_COMMENT: Address of shared queue address that consist of SQ/RQ and FW internal queues (IRQ/ORQ/HQ) */
	__le32 additional_setup_time;	/* HSI_COMMENT: Additional time (m)s, over and above firmware default, to allow for MPA handshake. Capped at 1800s. */
	__le16 rcv_wnd;		/* HSI_COMMENT: TCP window after scaling */
	u8 stats_counter_id;	/* HSI_COMMENT: Statistics counter ID to use */
	u8 reserved3[9];
};

/* iWARP TCP connection offload params passed by driver to FW  */
struct iwarp_offload_params {
	struct mpa_ulp_buffer incoming_ulp_buffer;	/* HSI_COMMENT: host buffer for placing the incoming MPA request */
	struct regpair async_eqe_output_buf;	/* HSI_COMMENT: host buffer for async tcp/mpa completion information - must have space for at least 8 bytes */
	struct regpair handle_for_async;	/* HSI_COMMENT: host handle that will be echoed back with in every qp-specific async EQE */
	__le32 additional_setup_time;	/* HSI_COMMENT: Additional time (ms), over and above firmware default, to allow for MPA handshake. Capped at 1800s. */
	__le16 physical_q0;	/* HSI_COMMENT: Physical QM queue to be tied to logical Q0 */
	__le16 physical_q1;	/* HSI_COMMENT: Physical QM queue to be tied to logical Q1 */
	u8 stats_counter_id;	/* HSI_COMMENT: Statistics counter ID to use */
	u8 mpa_mode;		/* HSI_COMMENT: Basic/Enhanced. Used for a verification for incoming MPA request (use enum mpa_negotiation_mode) */
	u8 src_vport_id;	/* HSI_COMMENT: Src vport id, for switching via loopback, if needed */
	u8 reserved[5];
};

/* iWARP query QP output params */
struct iwarp_query_qp_output_params {
	__le32 flags;
#define IWARP_QUERY_QP_OUTPUT_PARAMS_ERROR_FLG_MASK	0x1
#define IWARP_QUERY_QP_OUTPUT_PARAMS_ERROR_FLG_SHIFT	0
#define IWARP_QUERY_QP_OUTPUT_PARAMS_RESERVED0_MASK	0x7FFFFFFF
#define IWARP_QUERY_QP_OUTPUT_PARAMS_RESERVED0_SHIFT	1
	u8 reserved1[4];	/* HSI_COMMENT: 64 bit alignment */
};

/* iWARP query QP ramrod data */
struct iwarp_query_qp_ramrod_data {
	struct regpair output_params_addr;
};

/* iWARP Ramrod Command IDs  */
enum iwarp_ramrod_cmd_id {
	IWARP_RAMROD_CMD_ID_TCP_OFFLOAD = 16,	/* HSI_COMMENT: iWARP TCP connection offload ramrod */
	IWARP_RAMROD_CMD_ID_MPA_OFFLOAD,	/* HSI_COMMENT: iWARP MPA offload ramrod */
	IWARP_RAMROD_CMD_ID_MPA_OFFLOAD_SEND_RTR,
	IWARP_RAMROD_CMD_ID_CREATE_QP,
	IWARP_RAMROD_CMD_ID_QUERY_QP,
	IWARP_RAMROD_CMD_ID_MODIFY_QP,
	IWARP_RAMROD_CMD_ID_DESTROY_QP,
	IWARP_RAMROD_CMD_ID_ABORT_TCP_OFFLOAD,
	MAX_IWARP_RAMROD_CMD_ID
};

/* Per PF iWARP retransmit path statistics */
struct iwarp_rxmit_stats_drv {
	struct regpair tx_go_to_slow_start_event_cnt;	/* HSI_COMMENT: Number of times slow start event occurred */
	struct regpair tx_fast_retransmit_event_cnt;	/* HSI_COMMENT: Number of times fast retransmit event occurred */
};

/* iWARP and TCP connection offload params passed by driver to FW in iWARP offload ramrod  */
struct iwarp_tcp_offload_ramrod_data {
	struct tcp_offload_params_opt2 tcp;	/* HSI_COMMENT: tcp offload params */
	struct iwarp_offload_params iwarp;	/* HSI_COMMENT: iWARP connection offload params */
};

/* iWARP MPA negotiation types */
enum mpa_negotiation_mode {
	MPA_NEGOTIATION_TYPE_BASIC = 1,
	MPA_NEGOTIATION_TYPE_ENHANCED = 2,
	MAX_MPA_NEGOTIATION_MODE
};

/* iWARP MPA Enhanced mode RTR types */
enum mpa_rtr_type {
	MPA_RTR_TYPE_NONE = 0,	/* HSI_COMMENT: No RTR type */
	MPA_RTR_TYPE_ZERO_SEND = 1,
	MPA_RTR_TYPE_ZERO_WRITE = 2,
	MPA_RTR_TYPE_ZERO_SEND_AND_WRITE = 3,
	MPA_RTR_TYPE_ZERO_READ = 4,
	MPA_RTR_TYPE_ZERO_SEND_AND_READ = 5,
	MPA_RTR_TYPE_ZERO_WRITE_AND_READ = 6,
	MPA_RTR_TYPE_ZERO_SEND_AND_WRITE_AND_READ = 7,
	MAX_MPA_RTR_TYPE
};

/* unaligned opaque data received from LL2 */
struct unaligned_opaque_data {
	__le16 first_mpa_offset;	/* HSI_COMMENT: offset of first MPA byte that should be processed */
	u8 tcp_payload_offset;	/* HSI_COMMENT: offset of first the byte that comes after the last byte of the TCP Hdr */
	u8 flags;
#define UNALIGNED_OPAQUE_DATA_PKT_REACHED_WIN_RIGHT_EDGE_MASK		0x1	/* HSI_COMMENT: packet reached window right edge */
#define UNALIGNED_OPAQUE_DATA_PKT_REACHED_WIN_RIGHT_EDGE_SHIFT		0
#define UNALIGNED_OPAQUE_DATA_CONNECTION_CLOSED_MASK			0x1	/* HSI_COMMENT: Indication that the connection is closed. Clean all connections database. */
#define UNALIGNED_OPAQUE_DATA_CONNECTION_CLOSED_SHIFT			1
#define UNALIGNED_OPAQUE_DATA_RESERVED_MASK				0x3F
#define UNALIGNED_OPAQUE_DATA_RESERVED_SHIFT				2
	__le32 cid;
};

struct mstorm_iwarp_conn_ag_ctx {
	u8 reserved;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define MSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define MSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define MSTORM_IWARP_CONN_AG_CTX_BIT1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define MSTORM_IWARP_CONN_AG_CTX_BIT1_SHIFT			1
#define MSTORM_IWARP_CONN_AG_CTX_INV_STAG_DONE_CF_MASK		0x3	/* HSI_COMMENT: cf0 */
#define MSTORM_IWARP_CONN_AG_CTX_INV_STAG_DONE_CF_SHIFT		2
#define MSTORM_IWARP_CONN_AG_CTX_CF1_MASK			0x3	/* HSI_COMMENT: cf1 */
#define MSTORM_IWARP_CONN_AG_CTX_CF1_SHIFT			4
#define MSTORM_IWARP_CONN_AG_CTX_CF2_MASK			0x3	/* HSI_COMMENT: cf2 */
#define MSTORM_IWARP_CONN_AG_CTX_CF2_SHIFT			6
	u8 flags1;
#define MSTORM_IWARP_CONN_AG_CTX_INV_STAG_DONE_CF_EN_MASK	0x1	/* HSI_COMMENT: cf0en */
#define MSTORM_IWARP_CONN_AG_CTX_INV_STAG_DONE_CF_EN_SHIFT	0
#define MSTORM_IWARP_CONN_AG_CTX_CF1EN_MASK			0x1	/* HSI_COMMENT: cf1en */
#define MSTORM_IWARP_CONN_AG_CTX_CF1EN_SHIFT			1
#define MSTORM_IWARP_CONN_AG_CTX_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define MSTORM_IWARP_CONN_AG_CTX_CF2EN_SHIFT			2
#define MSTORM_IWARP_CONN_AG_CTX_RULE0EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define MSTORM_IWARP_CONN_AG_CTX_RULE0EN_SHIFT			3
#define MSTORM_IWARP_CONN_AG_CTX_RULE1EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define MSTORM_IWARP_CONN_AG_CTX_RULE1EN_SHIFT			4
#define MSTORM_IWARP_CONN_AG_CTX_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define MSTORM_IWARP_CONN_AG_CTX_RULE2EN_SHIFT			5
#define MSTORM_IWARP_CONN_AG_CTX_RCQ_CONS_EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define MSTORM_IWARP_CONN_AG_CTX_RCQ_CONS_EN_SHIFT		6
#define MSTORM_IWARP_CONN_AG_CTX_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define MSTORM_IWARP_CONN_AG_CTX_RULE4EN_SHIFT			7
	__le16 rcq_cons;	/* HSI_COMMENT: word0 */
	__le16 rcq_cons_th;	/* HSI_COMMENT: word1 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
};

struct ustorm_iwarp_conn_ag_ctx {
	u8 reserved;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define USTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define USTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define USTORM_IWARP_CONN_AG_CTX_BIT1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define USTORM_IWARP_CONN_AG_CTX_BIT1_SHIFT			1
#define USTORM_IWARP_CONN_AG_CTX_CF0_MASK			0x3	/* HSI_COMMENT: timer0cf */
#define USTORM_IWARP_CONN_AG_CTX_CF0_SHIFT			2
#define USTORM_IWARP_CONN_AG_CTX_CF1_MASK			0x3	/* HSI_COMMENT: timer1cf */
#define USTORM_IWARP_CONN_AG_CTX_CF1_SHIFT			4
#define USTORM_IWARP_CONN_AG_CTX_CF2_MASK			0x3	/* HSI_COMMENT: timer2cf */
#define USTORM_IWARP_CONN_AG_CTX_CF2_SHIFT			6
	u8 flags1;
#define USTORM_IWARP_CONN_AG_CTX_CF3_MASK			0x3	/* HSI_COMMENT: timer_stop_all */
#define USTORM_IWARP_CONN_AG_CTX_CF3_SHIFT			0
#define USTORM_IWARP_CONN_AG_CTX_CQ_ARM_SE_CF_MASK		0x3	/* HSI_COMMENT: cf4 */
#define USTORM_IWARP_CONN_AG_CTX_CQ_ARM_SE_CF_SHIFT		2
#define USTORM_IWARP_CONN_AG_CTX_CQ_ARM_CF_MASK			0x3	/* HSI_COMMENT: cf5 */
#define USTORM_IWARP_CONN_AG_CTX_CQ_ARM_CF_SHIFT		4
#define USTORM_IWARP_CONN_AG_CTX_CF6_MASK			0x3	/* HSI_COMMENT: cf6 */
#define USTORM_IWARP_CONN_AG_CTX_CF6_SHIFT			6
	u8 flags2;
#define USTORM_IWARP_CONN_AG_CTX_CF0EN_MASK			0x1	/* HSI_COMMENT: cf0en */
#define USTORM_IWARP_CONN_AG_CTX_CF0EN_SHIFT			0
#define USTORM_IWARP_CONN_AG_CTX_CF1EN_MASK			0x1	/* HSI_COMMENT: cf1en */
#define USTORM_IWARP_CONN_AG_CTX_CF1EN_SHIFT			1
#define USTORM_IWARP_CONN_AG_CTX_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define USTORM_IWARP_CONN_AG_CTX_CF2EN_SHIFT			2
#define USTORM_IWARP_CONN_AG_CTX_CF3EN_MASK			0x1	/* HSI_COMMENT: cf3en */
#define USTORM_IWARP_CONN_AG_CTX_CF3EN_SHIFT			3
#define USTORM_IWARP_CONN_AG_CTX_CQ_ARM_SE_CF_EN_MASK		0x1	/* HSI_COMMENT: cf4en */
#define USTORM_IWARP_CONN_AG_CTX_CQ_ARM_SE_CF_EN_SHIFT		4
#define USTORM_IWARP_CONN_AG_CTX_CQ_ARM_CF_EN_MASK		0x1	/* HSI_COMMENT: cf5en */
#define USTORM_IWARP_CONN_AG_CTX_CQ_ARM_CF_EN_SHIFT		5
#define USTORM_IWARP_CONN_AG_CTX_CF6EN_MASK			0x1	/* HSI_COMMENT: cf6en */
#define USTORM_IWARP_CONN_AG_CTX_CF6EN_SHIFT			6
#define USTORM_IWARP_CONN_AG_CTX_CQ_SE_EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define USTORM_IWARP_CONN_AG_CTX_CQ_SE_EN_SHIFT			7
	u8 flags3;
#define USTORM_IWARP_CONN_AG_CTX_CQ_EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define USTORM_IWARP_CONN_AG_CTX_CQ_EN_SHIFT			0
#define USTORM_IWARP_CONN_AG_CTX_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define USTORM_IWARP_CONN_AG_CTX_RULE2EN_SHIFT			1
#define USTORM_IWARP_CONN_AG_CTX_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define USTORM_IWARP_CONN_AG_CTX_RULE3EN_SHIFT			2
#define USTORM_IWARP_CONN_AG_CTX_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define USTORM_IWARP_CONN_AG_CTX_RULE4EN_SHIFT			3
#define USTORM_IWARP_CONN_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define USTORM_IWARP_CONN_AG_CTX_RULE5EN_SHIFT			4
#define USTORM_IWARP_CONN_AG_CTX_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define USTORM_IWARP_CONN_AG_CTX_RULE6EN_SHIFT			5
#define USTORM_IWARP_CONN_AG_CTX_RULE7EN_MASK			0x1	/* HSI_COMMENT: rule7en */
#define USTORM_IWARP_CONN_AG_CTX_RULE7EN_SHIFT			6
#define USTORM_IWARP_CONN_AG_CTX_RULE8EN_MASK			0x1	/* HSI_COMMENT: rule8en */
#define USTORM_IWARP_CONN_AG_CTX_RULE8EN_SHIFT			7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: conn_dpi */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le32 cq_cons;		/* HSI_COMMENT: reg0 */
	__le32 cq_se_prod;	/* HSI_COMMENT: reg1 */
	__le32 cq_prod;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
};

struct ystorm_iwarp_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define YSTORM_IWARP_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define YSTORM_IWARP_CONN_AG_CTX_BIT0_SHIFT		0
#define YSTORM_IWARP_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define YSTORM_IWARP_CONN_AG_CTX_BIT1_SHIFT		1
#define YSTORM_IWARP_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: cf0 */
#define YSTORM_IWARP_CONN_AG_CTX_CF0_SHIFT		2
#define YSTORM_IWARP_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: cf1 */
#define YSTORM_IWARP_CONN_AG_CTX_CF1_SHIFT		4
#define YSTORM_IWARP_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: cf2 */
#define YSTORM_IWARP_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define YSTORM_IWARP_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define YSTORM_IWARP_CONN_AG_CTX_CF0EN_SHIFT		0
#define YSTORM_IWARP_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define YSTORM_IWARP_CONN_AG_CTX_CF1EN_SHIFT		1
#define YSTORM_IWARP_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define YSTORM_IWARP_CONN_AG_CTX_CF2EN_SHIFT		2
#define YSTORM_IWARP_CONN_AG_CTX_RULE0EN_MASK		0x1	/* HSI_COMMENT: rule0en */
#define YSTORM_IWARP_CONN_AG_CTX_RULE0EN_SHIFT		3
#define YSTORM_IWARP_CONN_AG_CTX_RULE1EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define YSTORM_IWARP_CONN_AG_CTX_RULE1EN_SHIFT		4
#define YSTORM_IWARP_CONN_AG_CTX_RULE2EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define YSTORM_IWARP_CONN_AG_CTX_RULE2EN_SHIFT		5
#define YSTORM_IWARP_CONN_AG_CTX_RULE3EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define YSTORM_IWARP_CONN_AG_CTX_RULE3EN_SHIFT		6
#define YSTORM_IWARP_CONN_AG_CTX_RULE4EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define YSTORM_IWARP_CONN_AG_CTX_RULE4EN_SHIFT		7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: word0 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le16 word4;		/* HSI_COMMENT: word4 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
};

/****************************************/
/* Add include to common storage target */
/****************************************/

/************************************************************************/
/* Add include to common fcoe target for both eCore and protocol driver */
/************************************************************************/

/* The fcoe storm context of Ystorm */
struct ystorm_fcoe_conn_st_ctx {
	u8 func_mode;		/* HSI_COMMENT: Function mode */
	u8 cos;			/* HSI_COMMENT: Transmission cos */
	u8 conf_version;	/* HSI_COMMENT: Is dcb_version or vntag_version changed */
	u8 eth_hdr_size;	/* HSI_COMMENT: Ethernet header size */
	__le16 stat_ram_addr;	/* HSI_COMMENT: Statistics ram adderss */
	__le16 mtu;		/* HSI_COMMENT: MTU limitation */
	__le16 max_fc_payload_len;	/* HSI_COMMENT: Max payload length according to target limitation and mtu. 8 bytes aligned (required for protection fast-path) */
	__le16 tx_max_fc_pay_len;	/* HSI_COMMENT: Max payload length according to target limitation */
	u8 fcp_cmd_size;	/* HSI_COMMENT: FCP cmd size. for performance reasons */
	u8 fcp_rsp_size;	/* HSI_COMMENT: FCP RSP size. for performance reasons */
	__le16 mss;		/* HSI_COMMENT: MSS for PBF (MSS we negotiate with target - protection data per segment. If we are not in perf mode it will be according to worse case) */
	struct regpair reserved;
	__le16 min_frame_size;	/* HSI_COMMENT: The minimum ETH frame size required for transmission (including ETH header) */
	u8 protection_info_flags;
#define YSTORM_FCOE_CONN_ST_CTX_SUPPORT_PROTECTION_MASK		0x1	/* HSI_COMMENT: Does this connection support protection (if couple of GOS share this connection its enough that one of them support protection) */
#define YSTORM_FCOE_CONN_ST_CTX_SUPPORT_PROTECTION_SHIFT	0
#define YSTORM_FCOE_CONN_ST_CTX_VALID_MASK			0x1	/* HSI_COMMENT: Are we in protection perf mode (there is only one protection mode for this connection and we manage to create mss that contain fixed amount of protection segment and we are only restrict by the target limitation and not line mss this is critical since if line mss restrict us we cant rely on this size as it depends on vlan num) */
#define YSTORM_FCOE_CONN_ST_CTX_VALID_SHIFT			1
#define YSTORM_FCOE_CONN_ST_CTX_RESERVED1_MASK			0x3F
#define YSTORM_FCOE_CONN_ST_CTX_RESERVED1_SHIFT			2
	u8 dst_protection_per_mss;	/* HSI_COMMENT: Destination Protection data per mss (if we are not in perf mode it will be worse case). Destination is the data add/remove from the transmitted packet (as opposed to src which is data validate by the nic they might not be identical) */
	u8 src_protection_per_mss;	/* HSI_COMMENT: Source Protection data per mss (if we are not in perf mode it will be worse case). Source  is the data validated by the nic  (as opposed to destination which is data add/remove from the transmitted packet they might not be identical) */
	u8 ptu_log_page_size;	/* HSI_COMMENT: 0-4K, 1-8K, 2-16K, 3-32K... */
	u8 flags;
#define YSTORM_FCOE_CONN_ST_CTX_INNER_VLAN_FLAG_MASK		0x1	/* HSI_COMMENT: Inner Vlan flag */
#define YSTORM_FCOE_CONN_ST_CTX_INNER_VLAN_FLAG_SHIFT		0
#define YSTORM_FCOE_CONN_ST_CTX_OUTER_VLAN_FLAG_MASK		0x1	/* HSI_COMMENT: Outer Vlan flag */
#define YSTORM_FCOE_CONN_ST_CTX_OUTER_VLAN_FLAG_SHIFT		1
#define YSTORM_FCOE_CONN_ST_CTX_RSRV_MASK			0x3F
#define YSTORM_FCOE_CONN_ST_CTX_RSRV_SHIFT			2
	u8 fcp_xfer_size;	/* HSI_COMMENT: FCP xfer size. for performance reasons */
};

/* FCoE 16-bits vlan structure */
struct fcoe_vlan_fields {
	__le16 fields;
#define FCOE_VLAN_FIELDS_VID_MASK	0xFFF
#define FCOE_VLAN_FIELDS_VID_SHIFT	0
#define FCOE_VLAN_FIELDS_CLI_MASK	0x1
#define FCOE_VLAN_FIELDS_CLI_SHIFT	12
#define FCOE_VLAN_FIELDS_PRI_MASK	0x7
#define FCOE_VLAN_FIELDS_PRI_SHIFT	13
};

/* FCoE 16-bits vlan union */
union fcoe_vlan_field_union {
	struct fcoe_vlan_fields fields;	/* HSI_COMMENT: Parameters field */
	__le16 val;		/* HSI_COMMENT: Global value */
};

/* FCoE 16-bits vlan, vif union */
union fcoe_vlan_vif_field_union {
	union fcoe_vlan_field_union vlan;	/* HSI_COMMENT: Vlan */
	__le16 vif;		/* HSI_COMMENT: VIF */
};

/* Ethernet context section */
struct pstorm_fcoe_eth_context_section {
	u8 remote_addr_3;	/* HSI_COMMENT: Remote Mac Address, used in PBF Header Builder Command */
	u8 remote_addr_2;	/* HSI_COMMENT: Remote Mac Address, used in PBF Header Builder Command */
	u8 remote_addr_1;	/* HSI_COMMENT: Remote Mac Address, used in PBF Header Builder Command */
	u8 remote_addr_0;	/* HSI_COMMENT: Remote Mac Address, used in PBF Header Builder Command */
	u8 local_addr_1;	/* HSI_COMMENT: Local Mac Address, used in PBF Header Builder Command */
	u8 local_addr_0;	/* HSI_COMMENT: Local Mac Address, used in PBF Header Builder Command */
	u8 remote_addr_5;	/* HSI_COMMENT: Remote Mac Address, used in PBF Header Builder Command */
	u8 remote_addr_4;	/* HSI_COMMENT: Remote Mac Address, used in PBF Header Builder Command */
	u8 local_addr_5;	/* HSI_COMMENT: Local Mac Address, used in PBF Header Builder Command */
	u8 local_addr_4;	/* HSI_COMMENT: Loca lMac Address, used in PBF Header Builder Command */
	u8 local_addr_3;	/* HSI_COMMENT: Local Mac Address, used in PBF Header Builder Command */
	u8 local_addr_2;	/* HSI_COMMENT: Local Mac Address, used in PBF Header Builder Command */
	union fcoe_vlan_vif_field_union vif_outer_vlan;	/* HSI_COMMENT: Union of VIF and outer vlan */
	__le16 vif_outer_eth_type;	/* HSI_COMMENT: reserved place for Ethernet type */
	union fcoe_vlan_vif_field_union inner_vlan;	/* HSI_COMMENT: inner vlan tag */
	__le16 inner_eth_type;	/* HSI_COMMENT: reserved place for Ethernet type */
};

/* The fcoe storm context of Pstorm */
struct pstorm_fcoe_conn_st_ctx {
	u8 func_mode;		/* HSI_COMMENT: Function mode */
	u8 cos;			/* HSI_COMMENT: Transmission cos */
	u8 conf_version;	/* HSI_COMMENT: Is dcb_version or vntag_version changed */
	u8 rsrv;
	__le16 stat_ram_addr;	/* HSI_COMMENT: Statistics ram adderss */
	__le16 mss;		/* HSI_COMMENT: MSS for PBF (MSS we negotiate with target - protection data per segment. If we are not in perf mode it will be according to worse case) */
	struct regpair abts_cleanup_addr;	/* HSI_COMMENT: Host addr of ABTS /Cleanup info. since we pass it  through session context, we pass only the addr to save space */
	struct pstorm_fcoe_eth_context_section eth;	/* HSI_COMMENT: Source mac */
	u8 sid_2;		/* HSI_COMMENT: SID FC address - Third byte that is sent to NW via PBF For example is SID is 01:02:03 then sid_2 is 0x03 */
	u8 sid_1;		/* HSI_COMMENT: SID FC address - Second byte that is sent to NW via PBF */
	u8 sid_0;		/* HSI_COMMENT: SID FC address - First byte that is sent to NW via PBF */
	u8 flags;
#define PSTORM_FCOE_CONN_ST_CTX_VNTAG_VLAN_MASK			0x1	/* HSI_COMMENT: Is inner vlan taken from vntag default vlan (in this case I have to update inner vlan each time the default change) */
#define PSTORM_FCOE_CONN_ST_CTX_VNTAG_VLAN_SHIFT		0
#define PSTORM_FCOE_CONN_ST_CTX_SUPPORT_REC_RR_TOV_MASK		0x1	/* HSI_COMMENT: AreSupport rec_tov timer */
#define PSTORM_FCOE_CONN_ST_CTX_SUPPORT_REC_RR_TOV_SHIFT	1
#define PSTORM_FCOE_CONN_ST_CTX_INNER_VLAN_FLAG_MASK		0x1	/* HSI_COMMENT: Inner Vlan flag */
#define PSTORM_FCOE_CONN_ST_CTX_INNER_VLAN_FLAG_SHIFT		2
#define PSTORM_FCOE_CONN_ST_CTX_OUTER_VLAN_FLAG_MASK		0x1	/* HSI_COMMENT: Outer Vlan flag */
#define PSTORM_FCOE_CONN_ST_CTX_OUTER_VLAN_FLAG_SHIFT		3
#define PSTORM_FCOE_CONN_ST_CTX_SINGLE_VLAN_FLAG_MASK		0x1	/* HSI_COMMENT: Indicaiton that there should be a single vlan (for UFP mode) */
#define PSTORM_FCOE_CONN_ST_CTX_SINGLE_VLAN_FLAG_SHIFT		4
#define PSTORM_FCOE_CONN_ST_CTX_RESERVED_MASK			0x7
#define PSTORM_FCOE_CONN_ST_CTX_RESERVED_SHIFT			5
	u8 did_2;		/* HSI_COMMENT: DID FC address - Third byte that is sent to NW via PBF */
	u8 did_1;		/* HSI_COMMENT: DID FC address - Second byte that is sent to NW via PBF */
	u8 did_0;		/* HSI_COMMENT: DID FC address - First byte that is sent to NW via PBF */
	u8 src_mac_index;
	__le16 rec_rr_tov_val;	/* HSI_COMMENT: REC_TOV value negotiated during PLOGI (in msec) */
	u8 q_relative_offset;	/* HSI_COMMENT: CQ, RQ (and CMDQ) relative offset for connection */
	u8 reserved1;
};

/* The fcoe storm context of Xstorm */
struct xstorm_fcoe_conn_st_ctx {
	u8 func_mode;		/* HSI_COMMENT: Function mode */
	u8 src_mac_index;	/* HSI_COMMENT: Index to the src_mac arr held in the xStorm RAM. Provided at the xStorm offload connection handler */
	u8 conf_version;	/* HSI_COMMENT: Advance if vntag/dcb version advance */
	u8 cached_wqes_avail;	/* HSI_COMMENT: Number of cached wqes available */
	__le16 stat_ram_addr;	/* HSI_COMMENT: Statistics ram adderss */
	u8 flags;
#define XSTORM_FCOE_CONN_ST_CTX_SQ_DEFERRED_MASK		0x1	/* HSI_COMMENT: SQ deferred (happens when we wait for xfer wqe to complete cleanup/abts */
#define XSTORM_FCOE_CONN_ST_CTX_SQ_DEFERRED_SHIFT		0
#define XSTORM_FCOE_CONN_ST_CTX_INNER_VLAN_FLAG_MASK		0x1	/* HSI_COMMENT: Inner vlan flag for calculating the header size */
#define XSTORM_FCOE_CONN_ST_CTX_INNER_VLAN_FLAG_SHIFT		1
#define XSTORM_FCOE_CONN_ST_CTX_INNER_VLAN_FLAG_ORIG_MASK	0x1	/* HSI_COMMENT: Original vlan configuration. used when we switch from dcb enable to dcb disabled */
#define XSTORM_FCOE_CONN_ST_CTX_INNER_VLAN_FLAG_ORIG_SHIFT	2
#define XSTORM_FCOE_CONN_ST_CTX_LAST_QUEUE_HANDLED_MASK		0x3
#define XSTORM_FCOE_CONN_ST_CTX_LAST_QUEUE_HANDLED_SHIFT	3
#define XSTORM_FCOE_CONN_ST_CTX_RSRV_MASK			0x7
#define XSTORM_FCOE_CONN_ST_CTX_RSRV_SHIFT			5
	u8 cached_wqes_offset;	/* HSI_COMMENT: Offset of first valid cached wqe */
	u8 reserved2;
	u8 eth_hdr_size;	/* HSI_COMMENT: Ethernet header size */
	u8 seq_id;		/* HSI_COMMENT: Sequence id */
	u8 max_conc_seqs;	/* HSI_COMMENT: Max concurrent sequence id */
	__le16 num_pages_in_pbl;	/* HSI_COMMENT: Num of pages in SQ/RESPQ/XFERQ Pbl */
	__le16 reserved;
	struct regpair sq_pbl_addr;	/* HSI_COMMENT: SQ address */
	struct regpair sq_curr_page_addr;	/* HSI_COMMENT: SQ current page address */
	struct regpair sq_next_page_addr;	/* HSI_COMMENT: SQ next page address */
	struct regpair xferq_pbl_addr;	/* HSI_COMMENT: XFERQ address */
	struct regpair xferq_curr_page_addr;	/* HSI_COMMENT: XFERQ current page address */
	struct regpair xferq_next_page_addr;	/* HSI_COMMENT: XFERQ next page address */
	struct regpair respq_pbl_addr;	/* HSI_COMMENT: RESPQ address */
	struct regpair respq_curr_page_addr;	/* HSI_COMMENT: RESPQ current page address */
	struct regpair respq_next_page_addr;	/* HSI_COMMENT: RESPQ next page address */
	__le16 mtu;		/* HSI_COMMENT: MTU limitation */
	__le16 tx_max_fc_pay_len;	/* HSI_COMMENT: Max payload length according to target limitation */
	__le16 max_fc_payload_len;	/* HSI_COMMENT: Max payload length according to target limitation and mtu. Aligned to 4 bytes. */
	__le16 min_frame_size;	/* HSI_COMMENT: The minimum ETH frame size required for transmission (including ETH header, excluding ETH CRC */
	__le16 sq_pbl_next_index;	/* HSI_COMMENT: Next index of SQ Pbl */
	__le16 respq_pbl_next_index;	/* HSI_COMMENT: Next index of RESPQ Pbl */
	u8 fcp_cmd_byte_credit;	/* HSI_COMMENT: Pre-calculated byte credit that single FCP command can consume */
	u8 fcp_rsp_byte_credit;	/* HSI_COMMENT: Pre-calculated byte credit that single FCP RSP can consume. */
	__le16 protection_info;
#define XSTORM_FCOE_CONN_ST_CTX_PROTECTION_PERF_MASK		0x1	/* HSI_COMMENT: Intend to accelerate the protection flows */
#define XSTORM_FCOE_CONN_ST_CTX_PROTECTION_PERF_SHIFT		0
#define XSTORM_FCOE_CONN_ST_CTX_SUPPORT_PROTECTION_MASK		0x1	/* HSI_COMMENT: Does this connection support protection (if couple of GOS share this connection is enough that one of them support protection) */
#define XSTORM_FCOE_CONN_ST_CTX_SUPPORT_PROTECTION_SHIFT	1
#define XSTORM_FCOE_CONN_ST_CTX_VALID_MASK			0x1	/* HSI_COMMENT: Are we in protection perf mode (there is only one protection mode for this connection and we manage to create mss that contain fixed amount of protection segment and we are only restrict by the target limitation and not line mss this is critical since if line mss restrict us we cant rely on this size as it depends on vlan num) */
#define XSTORM_FCOE_CONN_ST_CTX_VALID_SHIFT			2
#define XSTORM_FCOE_CONN_ST_CTX_FRAME_PROT_ALIGNED_MASK		0x1	/* HSI_COMMENT: Is size of tx_max_pay_len_prot can be aligned to protection intervals. This means that pure data in each frame is 2k exactly, and protection intervals are no bigger than 2k */
#define XSTORM_FCOE_CONN_ST_CTX_FRAME_PROT_ALIGNED_SHIFT	3
#define XSTORM_FCOE_CONN_ST_CTX_RESERVED3_MASK			0xF
#define XSTORM_FCOE_CONN_ST_CTX_RESERVED3_SHIFT			4
#define XSTORM_FCOE_CONN_ST_CTX_DST_PROTECTION_PER_MSS_MASK	0xFF	/* HSI_COMMENT: Destination Pro tection data per mss (if we are not in perf mode it will be worse case). Destination is the data add/remove from the transmitted packet (as opposed to src which is data validate by the nic they might not be identical) */
#define XSTORM_FCOE_CONN_ST_CTX_DST_PROTECTION_PER_MSS_SHIFT	8
	__le16 xferq_pbl_next_index;	/* HSI_COMMENT: Next index of XFERQ Pbl */
	__le16 page_size;	/* HSI_COMMENT: Page size (in bytes) */
	u8 mid_seq;		/* HSI_COMMENT: Equals 1 for Middle sequence indication, otherwise 0 */
	u8 fcp_xfer_byte_credit;	/* HSI_COMMENT: Pre-calculated byte credit that single FCP command can consume */
	u8 reserved1[2];
	struct fcoe_wqe cached_wqes[16];	/* HSI_COMMENT: cached wqe (8) = 8*8*8Bytes */
};

struct xstorm_fcoe_conn_ag_ctx {
	u8 reserved0;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define XSTORM_FCOE_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define XSTORM_FCOE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED1_SHIFT			1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED2_MASK			0x1	/* HSI_COMMENT: exist_in_qm2 */
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED2_SHIFT			2
#define XSTORM_FCOE_CONN_AG_CTX_EXIST_IN_QM3_MASK		0x1	/* HSI_COMMENT: exist_in_qm3 */
#define XSTORM_FCOE_CONN_AG_CTX_EXIST_IN_QM3_SHIFT		3
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED3_MASK			0x1	/* HSI_COMMENT: bit4 */
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED3_SHIFT			4
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED4_MASK			0x1	/* HSI_COMMENT: cf_array_active */
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED4_SHIFT			5
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED5_MASK			0x1	/* HSI_COMMENT: bit6 */
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED5_SHIFT			6
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED6_MASK			0x1	/* HSI_COMMENT: bit7 */
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED6_SHIFT			7
	u8 flags1;
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED7_MASK			0x1	/* HSI_COMMENT: bit8 */
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED7_SHIFT			0
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED8_MASK			0x1	/* HSI_COMMENT: bit9 */
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED8_SHIFT			1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED9_MASK			0x1	/* HSI_COMMENT: bit10 */
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED9_SHIFT			2
#define XSTORM_FCOE_CONN_AG_CTX_BIT11_MASK			0x1	/* HSI_COMMENT: bit11 */
#define XSTORM_FCOE_CONN_AG_CTX_BIT11_SHIFT			3
#define XSTORM_FCOE_CONN_AG_CTX_BIT12_MASK			0x1	/* HSI_COMMENT: bit12 */
#define XSTORM_FCOE_CONN_AG_CTX_BIT12_SHIFT			4
#define XSTORM_FCOE_CONN_AG_CTX_BIT13_MASK			0x1	/* HSI_COMMENT: bit13 */
#define XSTORM_FCOE_CONN_AG_CTX_BIT13_SHIFT			5
#define XSTORM_FCOE_CONN_AG_CTX_BIT14_MASK			0x1	/* HSI_COMMENT: bit14 */
#define XSTORM_FCOE_CONN_AG_CTX_BIT14_SHIFT			6
#define XSTORM_FCOE_CONN_AG_CTX_BIT15_MASK			0x1	/* HSI_COMMENT: bit15 */
#define XSTORM_FCOE_CONN_AG_CTX_BIT15_SHIFT			7
	u8 flags2;
#define XSTORM_FCOE_CONN_AG_CTX_CF0_MASK			0x3	/* HSI_COMMENT: timer0cf */
#define XSTORM_FCOE_CONN_AG_CTX_CF0_SHIFT			0
#define XSTORM_FCOE_CONN_AG_CTX_CF1_MASK			0x3	/* HSI_COMMENT: timer1cf */
#define XSTORM_FCOE_CONN_AG_CTX_CF1_SHIFT			2
#define XSTORM_FCOE_CONN_AG_CTX_CF2_MASK			0x3	/* HSI_COMMENT: timer2cf */
#define XSTORM_FCOE_CONN_AG_CTX_CF2_SHIFT			4
#define XSTORM_FCOE_CONN_AG_CTX_CF3_MASK			0x3	/* HSI_COMMENT: timer_stop_all */
#define XSTORM_FCOE_CONN_AG_CTX_CF3_SHIFT			6
	u8 flags3;
#define XSTORM_FCOE_CONN_AG_CTX_CF4_MASK			0x3	/* HSI_COMMENT: cf4 */
#define XSTORM_FCOE_CONN_AG_CTX_CF4_SHIFT			0
#define XSTORM_FCOE_CONN_AG_CTX_CF5_MASK			0x3	/* HSI_COMMENT: cf5 */
#define XSTORM_FCOE_CONN_AG_CTX_CF5_SHIFT			2
#define XSTORM_FCOE_CONN_AG_CTX_CF6_MASK			0x3	/* HSI_COMMENT: cf6 */
#define XSTORM_FCOE_CONN_AG_CTX_CF6_SHIFT			4
#define XSTORM_FCOE_CONN_AG_CTX_CF7_MASK			0x3	/* HSI_COMMENT: cf7 */
#define XSTORM_FCOE_CONN_AG_CTX_CF7_SHIFT			6
	u8 flags4;
#define XSTORM_FCOE_CONN_AG_CTX_CF8_MASK			0x3	/* HSI_COMMENT: cf8 */
#define XSTORM_FCOE_CONN_AG_CTX_CF8_SHIFT			0
#define XSTORM_FCOE_CONN_AG_CTX_CF9_MASK			0x3	/* HSI_COMMENT: cf9 */
#define XSTORM_FCOE_CONN_AG_CTX_CF9_SHIFT			2
#define XSTORM_FCOE_CONN_AG_CTX_CF10_MASK			0x3	/* HSI_COMMENT: cf10 */
#define XSTORM_FCOE_CONN_AG_CTX_CF10_SHIFT			4
#define XSTORM_FCOE_CONN_AG_CTX_CF11_MASK			0x3	/* HSI_COMMENT: cf11 */
#define XSTORM_FCOE_CONN_AG_CTX_CF11_SHIFT			6
	u8 flags5;
#define XSTORM_FCOE_CONN_AG_CTX_CF12_MASK			0x3	/* HSI_COMMENT: cf12 */
#define XSTORM_FCOE_CONN_AG_CTX_CF12_SHIFT			0
#define XSTORM_FCOE_CONN_AG_CTX_CF13_MASK			0x3	/* HSI_COMMENT: cf13 */
#define XSTORM_FCOE_CONN_AG_CTX_CF13_SHIFT			2
#define XSTORM_FCOE_CONN_AG_CTX_CF14_MASK			0x3	/* HSI_COMMENT: cf14 */
#define XSTORM_FCOE_CONN_AG_CTX_CF14_SHIFT			4
#define XSTORM_FCOE_CONN_AG_CTX_CF15_MASK			0x3	/* HSI_COMMENT: cf15 */
#define XSTORM_FCOE_CONN_AG_CTX_CF15_SHIFT			6
	u8 flags6;
#define XSTORM_FCOE_CONN_AG_CTX_CF16_MASK			0x3	/* HSI_COMMENT: cf16 */
#define XSTORM_FCOE_CONN_AG_CTX_CF16_SHIFT			0
#define XSTORM_FCOE_CONN_AG_CTX_CF17_MASK			0x3	/* HSI_COMMENT: cf_array_cf */
#define XSTORM_FCOE_CONN_AG_CTX_CF17_SHIFT			2
#define XSTORM_FCOE_CONN_AG_CTX_CF18_MASK			0x3	/* HSI_COMMENT: cf18 */
#define XSTORM_FCOE_CONN_AG_CTX_CF18_SHIFT			4
#define XSTORM_FCOE_CONN_AG_CTX_DQ_CF_MASK			0x3	/* HSI_COMMENT: cf19 */
#define XSTORM_FCOE_CONN_AG_CTX_DQ_CF_SHIFT			6
	u8 flags7;
#define XSTORM_FCOE_CONN_AG_CTX_FLUSH_Q0_MASK			0x3	/* HSI_COMMENT: cf20 */
#define XSTORM_FCOE_CONN_AG_CTX_FLUSH_Q0_SHIFT			0
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED10_MASK			0x3	/* HSI_COMMENT: cf21 */
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED10_SHIFT		2
#define XSTORM_FCOE_CONN_AG_CTX_SLOW_PATH_MASK			0x3	/* HSI_COMMENT: cf22 */
#define XSTORM_FCOE_CONN_AG_CTX_SLOW_PATH_SHIFT			4
#define XSTORM_FCOE_CONN_AG_CTX_CF0EN_MASK			0x1	/* HSI_COMMENT: cf0en */
#define XSTORM_FCOE_CONN_AG_CTX_CF0EN_SHIFT			6
#define XSTORM_FCOE_CONN_AG_CTX_CF1EN_MASK			0x1	/* HSI_COMMENT: cf1en */
#define XSTORM_FCOE_CONN_AG_CTX_CF1EN_SHIFT			7
	u8 flags8;
#define XSTORM_FCOE_CONN_AG_CTX_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define XSTORM_FCOE_CONN_AG_CTX_CF2EN_SHIFT			0
#define XSTORM_FCOE_CONN_AG_CTX_CF3EN_MASK			0x1	/* HSI_COMMENT: cf3en */
#define XSTORM_FCOE_CONN_AG_CTX_CF3EN_SHIFT			1
#define XSTORM_FCOE_CONN_AG_CTX_CF4EN_MASK			0x1	/* HSI_COMMENT: cf4en */
#define XSTORM_FCOE_CONN_AG_CTX_CF4EN_SHIFT			2
#define XSTORM_FCOE_CONN_AG_CTX_CF5EN_MASK			0x1	/* HSI_COMMENT: cf5en */
#define XSTORM_FCOE_CONN_AG_CTX_CF5EN_SHIFT			3
#define XSTORM_FCOE_CONN_AG_CTX_CF6EN_MASK			0x1	/* HSI_COMMENT: cf6en */
#define XSTORM_FCOE_CONN_AG_CTX_CF6EN_SHIFT			4
#define XSTORM_FCOE_CONN_AG_CTX_CF7EN_MASK			0x1	/* HSI_COMMENT: cf7en */
#define XSTORM_FCOE_CONN_AG_CTX_CF7EN_SHIFT			5
#define XSTORM_FCOE_CONN_AG_CTX_CF8EN_MASK			0x1	/* HSI_COMMENT: cf8en */
#define XSTORM_FCOE_CONN_AG_CTX_CF8EN_SHIFT			6
#define XSTORM_FCOE_CONN_AG_CTX_CF9EN_MASK			0x1	/* HSI_COMMENT: cf9en */
#define XSTORM_FCOE_CONN_AG_CTX_CF9EN_SHIFT			7
	u8 flags9;
#define XSTORM_FCOE_CONN_AG_CTX_CF10EN_MASK			0x1	/* HSI_COMMENT: cf10en */
#define XSTORM_FCOE_CONN_AG_CTX_CF10EN_SHIFT			0
#define XSTORM_FCOE_CONN_AG_CTX_CF11EN_MASK			0x1	/* HSI_COMMENT: cf11en */
#define XSTORM_FCOE_CONN_AG_CTX_CF11EN_SHIFT			1
#define XSTORM_FCOE_CONN_AG_CTX_CF12EN_MASK			0x1	/* HSI_COMMENT: cf12en */
#define XSTORM_FCOE_CONN_AG_CTX_CF12EN_SHIFT			2
#define XSTORM_FCOE_CONN_AG_CTX_CF13EN_MASK			0x1	/* HSI_COMMENT: cf13en */
#define XSTORM_FCOE_CONN_AG_CTX_CF13EN_SHIFT			3
#define XSTORM_FCOE_CONN_AG_CTX_CF14EN_MASK			0x1	/* HSI_COMMENT: cf14en */
#define XSTORM_FCOE_CONN_AG_CTX_CF14EN_SHIFT			4
#define XSTORM_FCOE_CONN_AG_CTX_CF15EN_MASK			0x1	/* HSI_COMMENT: cf15en */
#define XSTORM_FCOE_CONN_AG_CTX_CF15EN_SHIFT			5
#define XSTORM_FCOE_CONN_AG_CTX_CF16EN_MASK			0x1	/* HSI_COMMENT: cf16en */
#define XSTORM_FCOE_CONN_AG_CTX_CF16EN_SHIFT			6
#define XSTORM_FCOE_CONN_AG_CTX_CF17EN_MASK			0x1	/* HSI_COMMENT: cf_array_cf_en */
#define XSTORM_FCOE_CONN_AG_CTX_CF17EN_SHIFT			7
	u8 flags10;
#define XSTORM_FCOE_CONN_AG_CTX_CF18EN_MASK			0x1	/* HSI_COMMENT: cf18en */
#define XSTORM_FCOE_CONN_AG_CTX_CF18EN_SHIFT			0
#define XSTORM_FCOE_CONN_AG_CTX_DQ_CF_EN_MASK			0x1	/* HSI_COMMENT: cf19en */
#define XSTORM_FCOE_CONN_AG_CTX_DQ_CF_EN_SHIFT			1
#define XSTORM_FCOE_CONN_AG_CTX_FLUSH_Q0_EN_MASK		0x1	/* HSI_COMMENT: cf20en */
#define XSTORM_FCOE_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT		2
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED11_MASK			0x1	/* HSI_COMMENT: cf21en */
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED11_SHIFT		3
#define XSTORM_FCOE_CONN_AG_CTX_SLOW_PATH_EN_MASK		0x1	/* HSI_COMMENT: cf22en */
#define XSTORM_FCOE_CONN_AG_CTX_SLOW_PATH_EN_SHIFT		4
#define XSTORM_FCOE_CONN_AG_CTX_CF23EN_MASK			0x1	/* HSI_COMMENT: cf23en */
#define XSTORM_FCOE_CONN_AG_CTX_CF23EN_SHIFT			5
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED12_MASK			0x1	/* HSI_COMMENT: rule0en */
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED12_SHIFT		6
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED13_MASK			0x1	/* HSI_COMMENT: rule1en */
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED13_SHIFT		7
	u8 flags11;
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED14_MASK			0x1	/* HSI_COMMENT: rule2en */
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED14_SHIFT		0
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED15_MASK			0x1	/* HSI_COMMENT: rule3en */
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED15_SHIFT		1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED16_MASK			0x1	/* HSI_COMMENT: rule4en */
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED16_SHIFT		2
#define XSTORM_FCOE_CONN_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define XSTORM_FCOE_CONN_AG_CTX_RULE5EN_SHIFT			3
#define XSTORM_FCOE_CONN_AG_CTX_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define XSTORM_FCOE_CONN_AG_CTX_RULE6EN_SHIFT			4
#define XSTORM_FCOE_CONN_AG_CTX_RULE7EN_MASK			0x1	/* HSI_COMMENT: rule7en */
#define XSTORM_FCOE_CONN_AG_CTX_RULE7EN_SHIFT			5
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED1_MASK		0x1	/* HSI_COMMENT: rule8en */
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED1_SHIFT		6
#define XSTORM_FCOE_CONN_AG_CTX_XFERQ_DECISION_EN_MASK		0x1	/* HSI_COMMENT: rule9en */
#define XSTORM_FCOE_CONN_AG_CTX_XFERQ_DECISION_EN_SHIFT		7
	u8 flags12;
#define XSTORM_FCOE_CONN_AG_CTX_SQ_DECISION_EN_MASK		0x1	/* HSI_COMMENT: rule10en */
#define XSTORM_FCOE_CONN_AG_CTX_SQ_DECISION_EN_SHIFT		0
#define XSTORM_FCOE_CONN_AG_CTX_RULE11EN_MASK			0x1	/* HSI_COMMENT: rule11en */
#define XSTORM_FCOE_CONN_AG_CTX_RULE11EN_SHIFT			1
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED2_MASK		0x1	/* HSI_COMMENT: rule12en */
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED2_SHIFT		2
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED3_MASK		0x1	/* HSI_COMMENT: rule13en */
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED3_SHIFT		3
#define XSTORM_FCOE_CONN_AG_CTX_RULE14EN_MASK			0x1	/* HSI_COMMENT: rule14en */
#define XSTORM_FCOE_CONN_AG_CTX_RULE14EN_SHIFT			4
#define XSTORM_FCOE_CONN_AG_CTX_RULE15EN_MASK			0x1	/* HSI_COMMENT: rule15en */
#define XSTORM_FCOE_CONN_AG_CTX_RULE15EN_SHIFT			5
#define XSTORM_FCOE_CONN_AG_CTX_RULE16EN_MASK			0x1	/* HSI_COMMENT: rule16en */
#define XSTORM_FCOE_CONN_AG_CTX_RULE16EN_SHIFT			6
#define XSTORM_FCOE_CONN_AG_CTX_RULE17EN_MASK			0x1	/* HSI_COMMENT: rule17en */
#define XSTORM_FCOE_CONN_AG_CTX_RULE17EN_SHIFT			7
	u8 flags13;
#define XSTORM_FCOE_CONN_AG_CTX_RESPQ_DECISION_EN_MASK		0x1	/* HSI_COMMENT: rule18en */
#define XSTORM_FCOE_CONN_AG_CTX_RESPQ_DECISION_EN_SHIFT		0
#define XSTORM_FCOE_CONN_AG_CTX_RULE19EN_MASK			0x1	/* HSI_COMMENT: rule19en */
#define XSTORM_FCOE_CONN_AG_CTX_RULE19EN_SHIFT			1
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED4_MASK		0x1	/* HSI_COMMENT: rule20en */
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED4_SHIFT		2
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED5_MASK		0x1	/* HSI_COMMENT: rule21en */
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED5_SHIFT		3
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED6_MASK		0x1	/* HSI_COMMENT: rule22en */
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED6_SHIFT		4
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED7_MASK		0x1	/* HSI_COMMENT: rule23en */
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED7_SHIFT		5
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED8_MASK		0x1	/* HSI_COMMENT: rule24en */
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED8_SHIFT		6
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED9_MASK		0x1	/* HSI_COMMENT: rule25en */
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED9_SHIFT		7
	u8 flags14;
#define XSTORM_FCOE_CONN_AG_CTX_BIT16_MASK			0x1	/* HSI_COMMENT: bit16 */
#define XSTORM_FCOE_CONN_AG_CTX_BIT16_SHIFT			0
#define XSTORM_FCOE_CONN_AG_CTX_BIT17_MASK			0x1	/* HSI_COMMENT: bit17 */
#define XSTORM_FCOE_CONN_AG_CTX_BIT17_SHIFT			1
#define XSTORM_FCOE_CONN_AG_CTX_BIT18_MASK			0x1	/* HSI_COMMENT: bit18 */
#define XSTORM_FCOE_CONN_AG_CTX_BIT18_SHIFT			2
#define XSTORM_FCOE_CONN_AG_CTX_BIT19_MASK			0x1	/* HSI_COMMENT: bit19 */
#define XSTORM_FCOE_CONN_AG_CTX_BIT19_SHIFT			3
#define XSTORM_FCOE_CONN_AG_CTX_BIT20_MASK			0x1	/* HSI_COMMENT: bit20 */
#define XSTORM_FCOE_CONN_AG_CTX_BIT20_SHIFT			4
#define XSTORM_FCOE_CONN_AG_CTX_BIT21_MASK			0x1	/* HSI_COMMENT: bit21 */
#define XSTORM_FCOE_CONN_AG_CTX_BIT21_SHIFT			5
#define XSTORM_FCOE_CONN_AG_CTX_CF23_MASK			0x3	/* HSI_COMMENT: cf23 */
#define XSTORM_FCOE_CONN_AG_CTX_CF23_SHIFT			6
	u8 byte2;		/* HSI_COMMENT: byte2 */
	__le16 physical_q0;	/* HSI_COMMENT: physical_q0 */
	__le16 word1;		/* HSI_COMMENT: physical_q1 */
	__le16 word2;		/* HSI_COMMENT: physical_q2 */
	__le16 sq_cons;		/* HSI_COMMENT: word3 */
	__le16 sq_prod;		/* HSI_COMMENT: word4 */
	__le16 xferq_prod;	/* HSI_COMMENT: word5 */
	__le16 xferq_cons;	/* HSI_COMMENT: conn_dpi */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	u8 byte5;		/* HSI_COMMENT: byte5 */
	u8 byte6;		/* HSI_COMMENT: byte6 */
	__le32 remain_io;	/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
	__le32 reg5;		/* HSI_COMMENT: cf_array0 */
	__le32 reg6;		/* HSI_COMMENT: cf_array1 */
	__le16 respq_prod;	/* HSI_COMMENT: word7 */
	__le16 respq_cons;	/* HSI_COMMENT: word8 */
	__le16 word9;		/* HSI_COMMENT: word9 */
	__le16 word10;		/* HSI_COMMENT: word10 */
	__le32 reg7;		/* HSI_COMMENT: reg7 */
	__le32 reg8;		/* HSI_COMMENT: reg8 */
};

/* The fcoe storm context of Ustorm */
struct ustorm_fcoe_conn_st_ctx {
	struct regpair respq_pbl_addr;	/* HSI_COMMENT: RespQ Pbl base address */
	__le16 num_pages_in_pbl;	/* HSI_COMMENT: Number of RespQ pbl pages (both have same wqe size) */
	u8 ptu_log_page_size;	/* HSI_COMMENT: 0-4K, 1-8K, 2-16K, 3-32K... */
	u8 log_page_size;
	__le16 respq_prod;	/* HSI_COMMENT: RespQ producer */
	u8 reserved[2];
};

struct tstorm_fcoe_conn_ag_ctx {
	u8 reserved0;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define TSTORM_FCOE_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define TSTORM_FCOE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define TSTORM_FCOE_CONN_AG_CTX_BIT1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define TSTORM_FCOE_CONN_AG_CTX_BIT1_SHIFT			1
#define TSTORM_FCOE_CONN_AG_CTX_BIT2_MASK			0x1	/* HSI_COMMENT: bit2 */
#define TSTORM_FCOE_CONN_AG_CTX_BIT2_SHIFT			2
#define TSTORM_FCOE_CONN_AG_CTX_BIT3_MASK			0x1	/* HSI_COMMENT: bit3 */
#define TSTORM_FCOE_CONN_AG_CTX_BIT3_SHIFT			3
#define TSTORM_FCOE_CONN_AG_CTX_BIT4_MASK			0x1	/* HSI_COMMENT: bit4 */
#define TSTORM_FCOE_CONN_AG_CTX_BIT4_SHIFT			4
#define TSTORM_FCOE_CONN_AG_CTX_BIT5_MASK			0x1	/* HSI_COMMENT: bit5 */
#define TSTORM_FCOE_CONN_AG_CTX_BIT5_SHIFT			5
#define TSTORM_FCOE_CONN_AG_CTX_DUMMY_TIMER_CF_MASK		0x3	/* HSI_COMMENT: timer0cf */
#define TSTORM_FCOE_CONN_AG_CTX_DUMMY_TIMER_CF_SHIFT		6
	u8 flags1;
#define TSTORM_FCOE_CONN_AG_CTX_FLUSH_Q0_CF_MASK		0x3	/* HSI_COMMENT: timer1cf */
#define TSTORM_FCOE_CONN_AG_CTX_FLUSH_Q0_CF_SHIFT		0
#define TSTORM_FCOE_CONN_AG_CTX_CF2_MASK			0x3	/* HSI_COMMENT: timer2cf */
#define TSTORM_FCOE_CONN_AG_CTX_CF2_SHIFT			2
#define TSTORM_FCOE_CONN_AG_CTX_TIMER_STOP_ALL_CF_MASK		0x3	/* HSI_COMMENT: timer_stop_all */
#define TSTORM_FCOE_CONN_AG_CTX_TIMER_STOP_ALL_CF_SHIFT		4
#define TSTORM_FCOE_CONN_AG_CTX_CF4_MASK			0x3	/* HSI_COMMENT: cf4 */
#define TSTORM_FCOE_CONN_AG_CTX_CF4_SHIFT			6
	u8 flags2;
#define TSTORM_FCOE_CONN_AG_CTX_CF5_MASK			0x3	/* HSI_COMMENT: cf5 */
#define TSTORM_FCOE_CONN_AG_CTX_CF5_SHIFT			0
#define TSTORM_FCOE_CONN_AG_CTX_CF6_MASK			0x3	/* HSI_COMMENT: cf6 */
#define TSTORM_FCOE_CONN_AG_CTX_CF6_SHIFT			2
#define TSTORM_FCOE_CONN_AG_CTX_CF7_MASK			0x3	/* HSI_COMMENT: cf7 */
#define TSTORM_FCOE_CONN_AG_CTX_CF7_SHIFT			4
#define TSTORM_FCOE_CONN_AG_CTX_CF8_MASK			0x3	/* HSI_COMMENT: cf8 */
#define TSTORM_FCOE_CONN_AG_CTX_CF8_SHIFT			6
	u8 flags3;
#define TSTORM_FCOE_CONN_AG_CTX_CF9_MASK			0x3	/* HSI_COMMENT: cf9 */
#define TSTORM_FCOE_CONN_AG_CTX_CF9_SHIFT			0
#define TSTORM_FCOE_CONN_AG_CTX_CF10_MASK			0x3	/* HSI_COMMENT: cf10 */
#define TSTORM_FCOE_CONN_AG_CTX_CF10_SHIFT			2
#define TSTORM_FCOE_CONN_AG_CTX_DUMMY_TIMER_CF_EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define TSTORM_FCOE_CONN_AG_CTX_DUMMY_TIMER_CF_EN_SHIFT		4
#define TSTORM_FCOE_CONN_AG_CTX_FLUSH_Q0_CF_EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define TSTORM_FCOE_CONN_AG_CTX_FLUSH_Q0_CF_EN_SHIFT		5
#define TSTORM_FCOE_CONN_AG_CTX_CF2EN_MASK			0x1	/* HSI_COMMENT: cf2en */
#define TSTORM_FCOE_CONN_AG_CTX_CF2EN_SHIFT			6
#define TSTORM_FCOE_CONN_AG_CTX_TIMER_STOP_ALL_CF_EN_MASK	0x1	/* HSI_COMMENT: cf3en */
#define TSTORM_FCOE_CONN_AG_CTX_TIMER_STOP_ALL_CF_EN_SHIFT	7
	u8 flags4;
#define TSTORM_FCOE_CONN_AG_CTX_CF4EN_MASK			0x1	/* HSI_COMMENT: cf4en */
#define TSTORM_FCOE_CONN_AG_CTX_CF4EN_SHIFT			0
#define TSTORM_FCOE_CONN_AG_CTX_CF5EN_MASK			0x1	/* HSI_COMMENT: cf5en */
#define TSTORM_FCOE_CONN_AG_CTX_CF5EN_SHIFT			1
#define TSTORM_FCOE_CONN_AG_CTX_CF6EN_MASK			0x1	/* HSI_COMMENT: cf6en */
#define TSTORM_FCOE_CONN_AG_CTX_CF6EN_SHIFT			2
#define TSTORM_FCOE_CONN_AG_CTX_CF7EN_MASK			0x1	/* HSI_COMMENT: cf7en */
#define TSTORM_FCOE_CONN_AG_CTX_CF7EN_SHIFT			3
#define TSTORM_FCOE_CONN_AG_CTX_CF8EN_MASK			0x1	/* HSI_COMMENT: cf8en */
#define TSTORM_FCOE_CONN_AG_CTX_CF8EN_SHIFT			4
#define TSTORM_FCOE_CONN_AG_CTX_CF9EN_MASK			0x1	/* HSI_COMMENT: cf9en */
#define TSTORM_FCOE_CONN_AG_CTX_CF9EN_SHIFT			5
#define TSTORM_FCOE_CONN_AG_CTX_CF10EN_MASK			0x1	/* HSI_COMMENT: cf10en */
#define TSTORM_FCOE_CONN_AG_CTX_CF10EN_SHIFT			6
#define TSTORM_FCOE_CONN_AG_CTX_RULE0EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define TSTORM_FCOE_CONN_AG_CTX_RULE0EN_SHIFT			7
	u8 flags5;
#define TSTORM_FCOE_CONN_AG_CTX_RULE1EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define TSTORM_FCOE_CONN_AG_CTX_RULE1EN_SHIFT			0
#define TSTORM_FCOE_CONN_AG_CTX_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define TSTORM_FCOE_CONN_AG_CTX_RULE2EN_SHIFT			1
#define TSTORM_FCOE_CONN_AG_CTX_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define TSTORM_FCOE_CONN_AG_CTX_RULE3EN_SHIFT			2
#define TSTORM_FCOE_CONN_AG_CTX_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define TSTORM_FCOE_CONN_AG_CTX_RULE4EN_SHIFT			3
#define TSTORM_FCOE_CONN_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define TSTORM_FCOE_CONN_AG_CTX_RULE5EN_SHIFT			4
#define TSTORM_FCOE_CONN_AG_CTX_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define TSTORM_FCOE_CONN_AG_CTX_RULE6EN_SHIFT			5
#define TSTORM_FCOE_CONN_AG_CTX_RULE7EN_MASK			0x1	/* HSI_COMMENT: rule7en */
#define TSTORM_FCOE_CONN_AG_CTX_RULE7EN_SHIFT			6
#define TSTORM_FCOE_CONN_AG_CTX_RULE8EN_MASK			0x1	/* HSI_COMMENT: rule8en */
#define TSTORM_FCOE_CONN_AG_CTX_RULE8EN_SHIFT			7
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
};

struct ustorm_fcoe_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define USTORM_FCOE_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define USTORM_FCOE_CONN_AG_CTX_BIT0_SHIFT		0
#define USTORM_FCOE_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define USTORM_FCOE_CONN_AG_CTX_BIT1_SHIFT		1
#define USTORM_FCOE_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: timer0cf */
#define USTORM_FCOE_CONN_AG_CTX_CF0_SHIFT		2
#define USTORM_FCOE_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: timer1cf */
#define USTORM_FCOE_CONN_AG_CTX_CF1_SHIFT		4
#define USTORM_FCOE_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: timer2cf */
#define USTORM_FCOE_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define USTORM_FCOE_CONN_AG_CTX_CF3_MASK		0x3	/* HSI_COMMENT: timer_stop_all */
#define USTORM_FCOE_CONN_AG_CTX_CF3_SHIFT		0
#define USTORM_FCOE_CONN_AG_CTX_CF4_MASK		0x3	/* HSI_COMMENT: cf4 */
#define USTORM_FCOE_CONN_AG_CTX_CF4_SHIFT		2
#define USTORM_FCOE_CONN_AG_CTX_CF5_MASK		0x3	/* HSI_COMMENT: cf5 */
#define USTORM_FCOE_CONN_AG_CTX_CF5_SHIFT		4
#define USTORM_FCOE_CONN_AG_CTX_CF6_MASK		0x3	/* HSI_COMMENT: cf6 */
#define USTORM_FCOE_CONN_AG_CTX_CF6_SHIFT		6
	u8 flags2;
#define USTORM_FCOE_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define USTORM_FCOE_CONN_AG_CTX_CF0EN_SHIFT		0
#define USTORM_FCOE_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define USTORM_FCOE_CONN_AG_CTX_CF1EN_SHIFT		1
#define USTORM_FCOE_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define USTORM_FCOE_CONN_AG_CTX_CF2EN_SHIFT		2
#define USTORM_FCOE_CONN_AG_CTX_CF3EN_MASK		0x1	/* HSI_COMMENT: cf3en */
#define USTORM_FCOE_CONN_AG_CTX_CF3EN_SHIFT		3
#define USTORM_FCOE_CONN_AG_CTX_CF4EN_MASK		0x1	/* HSI_COMMENT: cf4en */
#define USTORM_FCOE_CONN_AG_CTX_CF4EN_SHIFT		4
#define USTORM_FCOE_CONN_AG_CTX_CF5EN_MASK		0x1	/* HSI_COMMENT: cf5en */
#define USTORM_FCOE_CONN_AG_CTX_CF5EN_SHIFT		5
#define USTORM_FCOE_CONN_AG_CTX_CF6EN_MASK		0x1	/* HSI_COMMENT: cf6en */
#define USTORM_FCOE_CONN_AG_CTX_CF6EN_SHIFT		6
#define USTORM_FCOE_CONN_AG_CTX_RULE0EN_MASK		0x1	/* HSI_COMMENT: rule0en */
#define USTORM_FCOE_CONN_AG_CTX_RULE0EN_SHIFT		7
	u8 flags3;
#define USTORM_FCOE_CONN_AG_CTX_RULE1EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define USTORM_FCOE_CONN_AG_CTX_RULE1EN_SHIFT		0
#define USTORM_FCOE_CONN_AG_CTX_RULE2EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define USTORM_FCOE_CONN_AG_CTX_RULE2EN_SHIFT		1
#define USTORM_FCOE_CONN_AG_CTX_RULE3EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define USTORM_FCOE_CONN_AG_CTX_RULE3EN_SHIFT		2
#define USTORM_FCOE_CONN_AG_CTX_RULE4EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define USTORM_FCOE_CONN_AG_CTX_RULE4EN_SHIFT		3
#define USTORM_FCOE_CONN_AG_CTX_RULE5EN_MASK		0x1	/* HSI_COMMENT: rule5en */
#define USTORM_FCOE_CONN_AG_CTX_RULE5EN_SHIFT		4
#define USTORM_FCOE_CONN_AG_CTX_RULE6EN_MASK		0x1	/* HSI_COMMENT: rule6en */
#define USTORM_FCOE_CONN_AG_CTX_RULE6EN_SHIFT		5
#define USTORM_FCOE_CONN_AG_CTX_RULE7EN_MASK		0x1	/* HSI_COMMENT: rule7en */
#define USTORM_FCOE_CONN_AG_CTX_RULE7EN_SHIFT		6
#define USTORM_FCOE_CONN_AG_CTX_RULE8EN_MASK		0x1	/* HSI_COMMENT: rule8en */
#define USTORM_FCOE_CONN_AG_CTX_RULE8EN_SHIFT		7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: conn_dpi */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
};

/* The fcoe storm context of Tstorm */
struct tstorm_fcoe_conn_st_ctx {
	__le16 stat_ram_addr;	/* HSI_COMMENT: Statistics ram adderss */
	__le16 rx_max_fc_payload_len;	/* HSI_COMMENT: Max rx fc payload length. provided in ramrod */
	__le16 e_d_tov_val;	/* HSI_COMMENT: E_D_TOV value negotiated during PLOGI (in msec) */
	u8 flags;
#define TSTORM_FCOE_CONN_ST_CTX_INC_SEQ_CNT_MASK	0x1	/* HSI_COMMENT: Does the target support increment sequence counter */
#define TSTORM_FCOE_CONN_ST_CTX_INC_SEQ_CNT_SHIFT	0
#define TSTORM_FCOE_CONN_ST_CTX_SUPPORT_CONF_MASK	0x1	/* HSI_COMMENT: Does the connection support CONF REQ transmission */
#define TSTORM_FCOE_CONN_ST_CTX_SUPPORT_CONF_SHIFT	1
#define TSTORM_FCOE_CONN_ST_CTX_DEF_Q_IDX_MASK		0x3F	/* HSI_COMMENT: Default queue index the connection associated to */
#define TSTORM_FCOE_CONN_ST_CTX_DEF_Q_IDX_SHIFT		2
	u8 timers_cleanup_invocation_cnt;	/* HSI_COMMENT: This variable is incremented each time the tStorm handler for timers cleanup is invoked within the same timers cleanup flow */
	__le32 reserved1[2];
	__le32 dstMacAddressBytes0To3;	/* HSI_COMMENT: destination MAC address: Bytes 0-3. */
	__le16 dstMacAddressBytes4To5;	/* HSI_COMMENT: destination MAC address: Bytes 4-5. */
	__le16 ramrodEcho;	/* HSI_COMMENT: Saved ramrod echo - needed for 2nd round of terminate_conn (flush Q0) */
	u8 flags1;
#define TSTORM_FCOE_CONN_ST_CTX_MODE_MASK		0x3	/* HSI_COMMENT: Indicate the mode of the connection: Target or Initiator, use enum fcoe_mode_type */
#define TSTORM_FCOE_CONN_ST_CTX_MODE_SHIFT		0
#define TSTORM_FCOE_CONN_ST_CTX_RESERVED_MASK		0x3F
#define TSTORM_FCOE_CONN_ST_CTX_RESERVED_SHIFT		2
	u8 cq_relative_offset;	/* HSI_COMMENT: CQ relative offset for connection */
	u8 cmdq_relative_offset;	/* HSI_COMMENT: CmdQ relative offset for connection */
	u8 bdq_resource_id;	/* HSI_COMMENT: The BDQ resource ID to which this function is mapped */
	u8 reserved0[4];	/* HSI_COMMENT: Alignment to 128b */
};

struct mstorm_fcoe_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define MSTORM_FCOE_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define MSTORM_FCOE_CONN_AG_CTX_BIT0_SHIFT		0
#define MSTORM_FCOE_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define MSTORM_FCOE_CONN_AG_CTX_BIT1_SHIFT		1
#define MSTORM_FCOE_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: cf0 */
#define MSTORM_FCOE_CONN_AG_CTX_CF0_SHIFT		2
#define MSTORM_FCOE_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: cf1 */
#define MSTORM_FCOE_CONN_AG_CTX_CF1_SHIFT		4
#define MSTORM_FCOE_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: cf2 */
#define MSTORM_FCOE_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define MSTORM_FCOE_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define MSTORM_FCOE_CONN_AG_CTX_CF0EN_SHIFT		0
#define MSTORM_FCOE_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define MSTORM_FCOE_CONN_AG_CTX_CF1EN_SHIFT		1
#define MSTORM_FCOE_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define MSTORM_FCOE_CONN_AG_CTX_CF2EN_SHIFT		2
#define MSTORM_FCOE_CONN_AG_CTX_RULE0EN_MASK		0x1	/* HSI_COMMENT: rule0en */
#define MSTORM_FCOE_CONN_AG_CTX_RULE0EN_SHIFT		3
#define MSTORM_FCOE_CONN_AG_CTX_RULE1EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define MSTORM_FCOE_CONN_AG_CTX_RULE1EN_SHIFT		4
#define MSTORM_FCOE_CONN_AG_CTX_RULE2EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define MSTORM_FCOE_CONN_AG_CTX_RULE2EN_SHIFT		5
#define MSTORM_FCOE_CONN_AG_CTX_RULE3EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define MSTORM_FCOE_CONN_AG_CTX_RULE3EN_SHIFT		6
#define MSTORM_FCOE_CONN_AG_CTX_RULE4EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define MSTORM_FCOE_CONN_AG_CTX_RULE4EN_SHIFT		7
	__le16 word0;		/* HSI_COMMENT: word0 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
};

/* Fast path part of the fcoe storm context of Mstorm */
struct fcoe_mstorm_fcoe_conn_st_ctx_fp {
	__le16 xfer_prod;	/* HSI_COMMENT: XferQ producer */
	u8 num_cqs;		/* HSI_COMMENT: Number of CQs per function (internal to FW) */
	u8 reserved1;
	u8 protection_info;
#define FCOE_MSTORM_FCOE_CONN_ST_CTX_FP_SUPPORT_PROTECTION_MASK		0x1	/* HSI_COMMENT: Does this connection support protection (if couple of GOS share this connection it is enough that one of them support protection) */
#define FCOE_MSTORM_FCOE_CONN_ST_CTX_FP_SUPPORT_PROTECTION_SHIFT	0
#define FCOE_MSTORM_FCOE_CONN_ST_CTX_FP_VALID_MASK			0x1	/* HSI_COMMENT: Are we in protection perf mode (there is only one protection mode for this connection and we manage to create mss that contain fixed amount of protection segment and we are only restrict by the target limitation and not line mss this is critical since if line mss restrict us we cant rely on this size as it depends on vlan num) */
#define FCOE_MSTORM_FCOE_CONN_ST_CTX_FP_VALID_SHIFT			1
#define FCOE_MSTORM_FCOE_CONN_ST_CTX_FP_RESERVED0_MASK			0x3F
#define FCOE_MSTORM_FCOE_CONN_ST_CTX_FP_RESERVED0_SHIFT			2
	u8 q_relative_offset;	/* HSI_COMMENT: CQ, RQ and CMDQ relative offset for connection */
	u8 reserved2[2];
};

/* Non fast path part of the fcoe storm context of Mstorm */
struct fcoe_mstorm_fcoe_conn_st_ctx_non_fp {
	__le16 conn_id;		/* HSI_COMMENT: Driver connection ID. To be used by slowpaths to fill EQ placement params */
	__le16 stat_ram_addr;	/* HSI_COMMENT: Statistics ram adderss */
	__le16 num_pages_in_pbl;	/* HSI_COMMENT: Number of XferQ/RespQ pbl pages (both have same wqe size) */
	u8 ptu_log_page_size;	/* HSI_COMMENT: 0-4K, 1-8K, 2-16K, 3-32K... */
	u8 log_page_size;
	__le16 unsolicited_cq_count;	/* HSI_COMMENT: Counts number of CQs done due to unsolicited packets on this connection */
	__le16 cmdq_count;	/* HSI_COMMENT: Counts number of CMDQs done on this connection */
	u8 bdq_resource_id;	/* HSI_COMMENT: BDQ Resource ID */
	u8 reserved0[3];	/* HSI_COMMENT: Padding bytes for 2nd RegPair */
	struct regpair xferq_pbl_addr;	/* HSI_COMMENT: XferQ Pbl base address */
	struct regpair reserved1;
	struct regpair reserved2[3];
};

/* The fcoe storm context of Mstorm */
struct mstorm_fcoe_conn_st_ctx {
	struct fcoe_mstorm_fcoe_conn_st_ctx_fp fp;	/* HSI_COMMENT: Fast path part of the fcoe storm context of Mstorm */
	struct fcoe_mstorm_fcoe_conn_st_ctx_non_fp non_fp;	/* HSI_COMMENT: Non fast path part of the fcoe storm context of Mstorm */
};

/* fcoe connection context */
struct fcoe_conn_context {
	struct ystorm_fcoe_conn_st_ctx ystorm_st_context;	/* HSI_COMMENT: ystorm storm context */
	struct pstorm_fcoe_conn_st_ctx pstorm_st_context;	/* HSI_COMMENT: pstorm storm context */
	struct regpair pstorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct xstorm_fcoe_conn_st_ctx xstorm_st_context;	/* HSI_COMMENT: xstorm storm context */
	struct xstorm_fcoe_conn_ag_ctx xstorm_ag_context;	/* HSI_COMMENT: xstorm aggregative context */
	struct regpair xstorm_ag_padding[6];	/* HSI_COMMENT: padding */
	struct ustorm_fcoe_conn_st_ctx ustorm_st_context;	/* HSI_COMMENT: ustorm storm context */
	struct regpair ustorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct tstorm_fcoe_conn_ag_ctx tstorm_ag_context;	/* HSI_COMMENT: tstorm aggregative context */
	struct regpair tstorm_ag_padding[2];	/* HSI_COMMENT: padding */
	struct timers_context timer_context;	/* HSI_COMMENT: timer context */
	struct ustorm_fcoe_conn_ag_ctx ustorm_ag_context;	/* HSI_COMMENT: ustorm aggregative context */
	struct tstorm_fcoe_conn_st_ctx tstorm_st_context;	/* HSI_COMMENT: tstorm storm context */
	struct mstorm_fcoe_conn_ag_ctx mstorm_ag_context;	/* HSI_COMMENT: mstorm aggregative context */
	struct mstorm_fcoe_conn_st_ctx mstorm_st_context;	/* HSI_COMMENT: mstorm storm context */
};

/* FCoE connection offload params passed by driver to FW in FCoE offload ramrod  */
struct fcoe_conn_offload_ramrod_params {
	struct fcoe_conn_offload_ramrod_data offload_ramrod_data;
};

/* FCoE connection terminate params passed by driver to FW in FCoE terminate conn ramrod  */
struct fcoe_conn_terminate_ramrod_params {
	struct fcoe_conn_terminate_ramrod_data terminate_ramrod_data;
};

/* FCoE event type */
enum fcoe_event_type {
	FCOE_EVENT_INIT_FUNC,	/* HSI_COMMENT: Slowpath completion on INIT_FUNC ramrod */
	FCOE_EVENT_DESTROY_FUNC,	/* HSI_COMMENT: Slowpath completion on DESTROY_FUNC ramrod */
	FCOE_EVENT_STAT_FUNC,	/* HSI_COMMENT: Slowpath completion on STAT_FUNC ramrod */
	FCOE_EVENT_OFFLOAD_CONN,	/* HSI_COMMENT: Slowpath completion on OFFLOAD_CONN ramrod */
	FCOE_EVENT_TERMINATE_CONN,	/* HSI_COMMENT: Slowpath completion on TERMINATE_CONN ramrod */
	FCOE_EVENT_ERROR,	/* HSI_COMMENT: Error event */
	MAX_FCOE_EVENT_TYPE
};

/* FCoE init params passed by driver to FW in FCoE init ramrod  */
struct fcoe_init_ramrod_params {
	struct fcoe_init_func_ramrod_data init_ramrod_data;
};

/* FCoE ramrod Command IDs  */
enum fcoe_ramrod_cmd_id {
	FCOE_RAMROD_CMD_ID_INIT_FUNC,	/* HSI_COMMENT: FCoE function init ramrod */
	FCOE_RAMROD_CMD_ID_DESTROY_FUNC,	/* HSI_COMMENT: FCoE function destroy ramrod */
	FCOE_RAMROD_CMD_ID_STAT_FUNC,	/* HSI_COMMENT: FCoE statistics ramrod */
	FCOE_RAMROD_CMD_ID_OFFLOAD_CONN,	/* HSI_COMMENT: FCoE connection offload ramrod */
	FCOE_RAMROD_CMD_ID_TERMINATE_CONN,	/* HSI_COMMENT: FCoE connection offload ramrod. Command ID known only to FW and VBD */
	MAX_FCOE_RAMROD_CMD_ID
};

/* FCoE statistics params buffer passed by driver to FW in FCoE statistics ramrod  */
struct fcoe_stat_ramrod_params {
	struct fcoe_stat_ramrod_data stat_ramrod_data;
};

struct ystorm_fcoe_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define YSTORM_FCOE_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define YSTORM_FCOE_CONN_AG_CTX_BIT0_SHIFT		0
#define YSTORM_FCOE_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define YSTORM_FCOE_CONN_AG_CTX_BIT1_SHIFT		1
#define YSTORM_FCOE_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: cf0 */
#define YSTORM_FCOE_CONN_AG_CTX_CF0_SHIFT		2
#define YSTORM_FCOE_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: cf1 */
#define YSTORM_FCOE_CONN_AG_CTX_CF1_SHIFT		4
#define YSTORM_FCOE_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: cf2 */
#define YSTORM_FCOE_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define YSTORM_FCOE_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define YSTORM_FCOE_CONN_AG_CTX_CF0EN_SHIFT		0
#define YSTORM_FCOE_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define YSTORM_FCOE_CONN_AG_CTX_CF1EN_SHIFT		1
#define YSTORM_FCOE_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define YSTORM_FCOE_CONN_AG_CTX_CF2EN_SHIFT		2
#define YSTORM_FCOE_CONN_AG_CTX_RULE0EN_MASK		0x1	/* HSI_COMMENT: rule0en */
#define YSTORM_FCOE_CONN_AG_CTX_RULE0EN_SHIFT		3
#define YSTORM_FCOE_CONN_AG_CTX_RULE1EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define YSTORM_FCOE_CONN_AG_CTX_RULE1EN_SHIFT		4
#define YSTORM_FCOE_CONN_AG_CTX_RULE2EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define YSTORM_FCOE_CONN_AG_CTX_RULE2EN_SHIFT		5
#define YSTORM_FCOE_CONN_AG_CTX_RULE3EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define YSTORM_FCOE_CONN_AG_CTX_RULE3EN_SHIFT		6
#define YSTORM_FCOE_CONN_AG_CTX_RULE4EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define YSTORM_FCOE_CONN_AG_CTX_RULE4EN_SHIFT		7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: word0 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le16 word4;		/* HSI_COMMENT: word4 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
};

/****************************************/
/* Add include to common storage target */
/****************************************/

/****************************************/
/* Add include to common storage over TCP target */
/****************************************/

/*************************************************************************/
/* Add include to common iSCSI target for both eCore and protocol driver */
/************************************************************************/

/* The iscsi storm connection context of Ystorm */
struct ystorm_iscsi_conn_st_ctx {
	__le32 reserved[8];
};

/* Combined iSCSI and TCP storm connection of Pstorm */
struct pstorm_iscsi_tcp_conn_st_ctx {
	__le32 tcp[32];
	__le32 iscsi[4];
};

/* The combined tcp and iscsi storm context of Xstorm */
struct xstorm_iscsi_tcp_conn_st_ctx {
	__le32 reserved_tcp[4];
	__le32 reserved_iscsi[44];
};

struct xstorm_iscsi_conn_ag_ctx {
	u8 cdu_validation;	/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define XSTORM_ISCSI_CONN_AG_CTX_EXIST_IN_QM0_MASK			0x1	/* HSI_COMMENT: exist_in_qm0 */
#define XSTORM_ISCSI_CONN_AG_CTX_EXIST_IN_QM0_SHIFT			0
#define XSTORM_ISCSI_CONN_AG_CTX_EXIST_IN_QM1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define XSTORM_ISCSI_CONN_AG_CTX_EXIST_IN_QM1_SHIFT			1
#define XSTORM_ISCSI_CONN_AG_CTX_RESERVED1_MASK				0x1	/* HSI_COMMENT: exist_in_qm2 */
#define XSTORM_ISCSI_CONN_AG_CTX_RESERVED1_SHIFT			2
#define XSTORM_ISCSI_CONN_AG_CTX_EXIST_IN_QM3_MASK			0x1	/* HSI_COMMENT: exist_in_qm3 */
#define XSTORM_ISCSI_CONN_AG_CTX_EXIST_IN_QM3_SHIFT			3
#define XSTORM_ISCSI_CONN_AG_CTX_BIT4_MASK				0x1	/* HSI_COMMENT: bit4 */
#define XSTORM_ISCSI_CONN_AG_CTX_BIT4_SHIFT				4
#define XSTORM_ISCSI_CONN_AG_CTX_RESERVED2_MASK				0x1	/* HSI_COMMENT: cf_array_active */
#define XSTORM_ISCSI_CONN_AG_CTX_RESERVED2_SHIFT			5
#define XSTORM_ISCSI_CONN_AG_CTX_BIT6_MASK				0x1	/* HSI_COMMENT: bit6 */
#define XSTORM_ISCSI_CONN_AG_CTX_BIT6_SHIFT				6
#define XSTORM_ISCSI_CONN_AG_CTX_BIT7_MASK				0x1	/* HSI_COMMENT: bit7 */
#define XSTORM_ISCSI_CONN_AG_CTX_BIT7_SHIFT				7
	u8 flags1;
#define XSTORM_ISCSI_CONN_AG_CTX_BIT8_MASK				0x1	/* HSI_COMMENT: bit8 */
#define XSTORM_ISCSI_CONN_AG_CTX_BIT8_SHIFT				0
#define XSTORM_ISCSI_CONN_AG_CTX_BIT9_MASK				0x1	/* HSI_COMMENT: bit9 */
#define XSTORM_ISCSI_CONN_AG_CTX_BIT9_SHIFT				1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT10_MASK				0x1	/* HSI_COMMENT: bit10 */
#define XSTORM_ISCSI_CONN_AG_CTX_BIT10_SHIFT				2
#define XSTORM_ISCSI_CONN_AG_CTX_BIT11_MASK				0x1	/* HSI_COMMENT: bit11 */
#define XSTORM_ISCSI_CONN_AG_CTX_BIT11_SHIFT				3
#define XSTORM_ISCSI_CONN_AG_CTX_BIT12_MASK				0x1	/* HSI_COMMENT: bit12 */
#define XSTORM_ISCSI_CONN_AG_CTX_BIT12_SHIFT				4
#define XSTORM_ISCSI_CONN_AG_CTX_BIT13_MASK				0x1	/* HSI_COMMENT: bit13 */
#define XSTORM_ISCSI_CONN_AG_CTX_BIT13_SHIFT				5
#define XSTORM_ISCSI_CONN_AG_CTX_BIT14_MASK				0x1	/* HSI_COMMENT: bit14 */
#define XSTORM_ISCSI_CONN_AG_CTX_BIT14_SHIFT				6
#define XSTORM_ISCSI_CONN_AG_CTX_TX_TRUNCATE_MASK			0x1	/* HSI_COMMENT: bit15 */
#define XSTORM_ISCSI_CONN_AG_CTX_TX_TRUNCATE_SHIFT			7
	u8 flags2;
#define XSTORM_ISCSI_CONN_AG_CTX_CF0_MASK				0x3	/* HSI_COMMENT: timer0cf */
#define XSTORM_ISCSI_CONN_AG_CTX_CF0_SHIFT				0
#define XSTORM_ISCSI_CONN_AG_CTX_CF1_MASK				0x3	/* HSI_COMMENT: timer1cf */
#define XSTORM_ISCSI_CONN_AG_CTX_CF1_SHIFT				2
#define XSTORM_ISCSI_CONN_AG_CTX_CF2_MASK				0x3	/* HSI_COMMENT: timer2cf */
#define XSTORM_ISCSI_CONN_AG_CTX_CF2_SHIFT				4
#define XSTORM_ISCSI_CONN_AG_CTX_TIMER_STOP_ALL_MASK			0x3	/* HSI_COMMENT: timer_stop_all */
#define XSTORM_ISCSI_CONN_AG_CTX_TIMER_STOP_ALL_SHIFT			6
	u8 flags3;
#define XSTORM_ISCSI_CONN_AG_CTX_CF4_MASK				0x3	/* HSI_COMMENT: cf4 */
#define XSTORM_ISCSI_CONN_AG_CTX_CF4_SHIFT				0
#define XSTORM_ISCSI_CONN_AG_CTX_CF5_MASK				0x3	/* HSI_COMMENT: cf5 */
#define XSTORM_ISCSI_CONN_AG_CTX_CF5_SHIFT				2
#define XSTORM_ISCSI_CONN_AG_CTX_CF6_MASK				0x3	/* HSI_COMMENT: cf6 */
#define XSTORM_ISCSI_CONN_AG_CTX_CF6_SHIFT				4
#define XSTORM_ISCSI_CONN_AG_CTX_CF7_MASK				0x3	/* HSI_COMMENT: cf7 */
#define XSTORM_ISCSI_CONN_AG_CTX_CF7_SHIFT				6
	u8 flags4;
#define XSTORM_ISCSI_CONN_AG_CTX_CF8_MASK				0x3	/* HSI_COMMENT: cf8 */
#define XSTORM_ISCSI_CONN_AG_CTX_CF8_SHIFT				0
#define XSTORM_ISCSI_CONN_AG_CTX_CF9_MASK				0x3	/* HSI_COMMENT: cf9 */
#define XSTORM_ISCSI_CONN_AG_CTX_CF9_SHIFT				2
#define XSTORM_ISCSI_CONN_AG_CTX_CF10_MASK				0x3	/* HSI_COMMENT: cf10 */
#define XSTORM_ISCSI_CONN_AG_CTX_CF10_SHIFT				4
#define XSTORM_ISCSI_CONN_AG_CTX_CF11_MASK				0x3	/* HSI_COMMENT: cf11 */
#define XSTORM_ISCSI_CONN_AG_CTX_CF11_SHIFT				6
	u8 flags5;
#define XSTORM_ISCSI_CONN_AG_CTX_CF12_MASK				0x3	/* HSI_COMMENT: cf12 */
#define XSTORM_ISCSI_CONN_AG_CTX_CF12_SHIFT				0
#define XSTORM_ISCSI_CONN_AG_CTX_CF13_MASK				0x3	/* HSI_COMMENT: cf13 */
#define XSTORM_ISCSI_CONN_AG_CTX_CF13_SHIFT				2
#define XSTORM_ISCSI_CONN_AG_CTX_CF14_MASK				0x3	/* HSI_COMMENT: cf14 */
#define XSTORM_ISCSI_CONN_AG_CTX_CF14_SHIFT				4
#define XSTORM_ISCSI_CONN_AG_CTX_UPDATE_STATE_TO_BASE_CF_MASK		0x3	/* HSI_COMMENT: cf15 */
#define XSTORM_ISCSI_CONN_AG_CTX_UPDATE_STATE_TO_BASE_CF_SHIFT		6
	u8 flags6;
#define XSTORM_ISCSI_CONN_AG_CTX_CF16_MASK				0x3	/* HSI_COMMENT: cf16 */
#define XSTORM_ISCSI_CONN_AG_CTX_CF16_SHIFT				0
#define XSTORM_ISCSI_CONN_AG_CTX_CF17_MASK				0x3	/* HSI_COMMENT: cf_array_cf */
#define XSTORM_ISCSI_CONN_AG_CTX_CF17_SHIFT				2
#define XSTORM_ISCSI_CONN_AG_CTX_CF18_MASK				0x3	/* HSI_COMMENT: cf18 */
#define XSTORM_ISCSI_CONN_AG_CTX_CF18_SHIFT				4
#define XSTORM_ISCSI_CONN_AG_CTX_DQ_FLUSH_MASK				0x3	/* HSI_COMMENT: cf19 */
#define XSTORM_ISCSI_CONN_AG_CTX_DQ_FLUSH_SHIFT				6
	u8 flags7;
#define XSTORM_ISCSI_CONN_AG_CTX_MST_XCM_Q0_FLUSH_CF_MASK		0x3	/* HSI_COMMENT: cf20 */
#define XSTORM_ISCSI_CONN_AG_CTX_MST_XCM_Q0_FLUSH_CF_SHIFT		0
#define XSTORM_ISCSI_CONN_AG_CTX_UST_XCM_Q1_FLUSH_CF_MASK		0x3	/* HSI_COMMENT: cf21 */
#define XSTORM_ISCSI_CONN_AG_CTX_UST_XCM_Q1_FLUSH_CF_SHIFT		2
#define XSTORM_ISCSI_CONN_AG_CTX_SLOW_PATH_MASK				0x3	/* HSI_COMMENT: cf22 */
#define XSTORM_ISCSI_CONN_AG_CTX_SLOW_PATH_SHIFT			4
#define XSTORM_ISCSI_CONN_AG_CTX_CF0EN_MASK				0x1	/* HSI_COMMENT: cf0en */
#define XSTORM_ISCSI_CONN_AG_CTX_CF0EN_SHIFT				6
#define XSTORM_ISCSI_CONN_AG_CTX_CF1EN_MASK				0x1	/* HSI_COMMENT: cf1en */
#define XSTORM_ISCSI_CONN_AG_CTX_CF1EN_SHIFT				7
	u8 flags8;
#define XSTORM_ISCSI_CONN_AG_CTX_CF2EN_MASK				0x1	/* HSI_COMMENT: cf2en */
#define XSTORM_ISCSI_CONN_AG_CTX_CF2EN_SHIFT				0
#define XSTORM_ISCSI_CONN_AG_CTX_TIMER_STOP_ALL_EN_MASK			0x1	/* HSI_COMMENT: cf3en */
#define XSTORM_ISCSI_CONN_AG_CTX_TIMER_STOP_ALL_EN_SHIFT		1
#define XSTORM_ISCSI_CONN_AG_CTX_CF4EN_MASK				0x1	/* HSI_COMMENT: cf4en */
#define XSTORM_ISCSI_CONN_AG_CTX_CF4EN_SHIFT				2
#define XSTORM_ISCSI_CONN_AG_CTX_CF5EN_MASK				0x1	/* HSI_COMMENT: cf5en */
#define XSTORM_ISCSI_CONN_AG_CTX_CF5EN_SHIFT				3
#define XSTORM_ISCSI_CONN_AG_CTX_CF6EN_MASK				0x1	/* HSI_COMMENT: cf6en */
#define XSTORM_ISCSI_CONN_AG_CTX_CF6EN_SHIFT				4
#define XSTORM_ISCSI_CONN_AG_CTX_CF7EN_MASK				0x1	/* HSI_COMMENT: cf7en */
#define XSTORM_ISCSI_CONN_AG_CTX_CF7EN_SHIFT				5
#define XSTORM_ISCSI_CONN_AG_CTX_CF8EN_MASK				0x1	/* HSI_COMMENT: cf8en */
#define XSTORM_ISCSI_CONN_AG_CTX_CF8EN_SHIFT				6
#define XSTORM_ISCSI_CONN_AG_CTX_CF9EN_MASK				0x1	/* HSI_COMMENT: cf9en */
#define XSTORM_ISCSI_CONN_AG_CTX_CF9EN_SHIFT				7
	u8 flags9;
#define XSTORM_ISCSI_CONN_AG_CTX_CF10EN_MASK				0x1	/* HSI_COMMENT: cf10en */
#define XSTORM_ISCSI_CONN_AG_CTX_CF10EN_SHIFT				0
#define XSTORM_ISCSI_CONN_AG_CTX_CF11EN_MASK				0x1	/* HSI_COMMENT: cf11en */
#define XSTORM_ISCSI_CONN_AG_CTX_CF11EN_SHIFT				1
#define XSTORM_ISCSI_CONN_AG_CTX_CF12EN_MASK				0x1	/* HSI_COMMENT: cf12en */
#define XSTORM_ISCSI_CONN_AG_CTX_CF12EN_SHIFT				2
#define XSTORM_ISCSI_CONN_AG_CTX_CF13EN_MASK				0x1	/* HSI_COMMENT: cf13en */
#define XSTORM_ISCSI_CONN_AG_CTX_CF13EN_SHIFT				3
#define XSTORM_ISCSI_CONN_AG_CTX_CF14EN_MASK				0x1	/* HSI_COMMENT: cf14en */
#define XSTORM_ISCSI_CONN_AG_CTX_CF14EN_SHIFT				4
#define XSTORM_ISCSI_CONN_AG_CTX_UPDATE_STATE_TO_BASE_CF_EN_MASK	0x1	/* HSI_COMMENT: cf15en */
#define XSTORM_ISCSI_CONN_AG_CTX_UPDATE_STATE_TO_BASE_CF_EN_SHIFT	5
#define XSTORM_ISCSI_CONN_AG_CTX_CF16EN_MASK				0x1	/* HSI_COMMENT: cf16en */
#define XSTORM_ISCSI_CONN_AG_CTX_CF16EN_SHIFT				6
#define XSTORM_ISCSI_CONN_AG_CTX_CF17EN_MASK				0x1	/* HSI_COMMENT: cf_array_cf_en */
#define XSTORM_ISCSI_CONN_AG_CTX_CF17EN_SHIFT				7
	u8 flags10;
#define XSTORM_ISCSI_CONN_AG_CTX_CF18EN_MASK				0x1	/* HSI_COMMENT: cf18en */
#define XSTORM_ISCSI_CONN_AG_CTX_CF18EN_SHIFT				0
#define XSTORM_ISCSI_CONN_AG_CTX_DQ_FLUSH_EN_MASK			0x1	/* HSI_COMMENT: cf19en */
#define XSTORM_ISCSI_CONN_AG_CTX_DQ_FLUSH_EN_SHIFT			1
#define XSTORM_ISCSI_CONN_AG_CTX_MST_XCM_Q0_FLUSH_CF_EN_MASK		0x1	/* HSI_COMMENT: cf20en */
#define XSTORM_ISCSI_CONN_AG_CTX_MST_XCM_Q0_FLUSH_CF_EN_SHIFT		2
#define XSTORM_ISCSI_CONN_AG_CTX_UST_XCM_Q1_FLUSH_CF_EN_MASK		0x1	/* HSI_COMMENT: cf21en */
#define XSTORM_ISCSI_CONN_AG_CTX_UST_XCM_Q1_FLUSH_CF_EN_SHIFT		3
#define XSTORM_ISCSI_CONN_AG_CTX_SLOW_PATH_EN_MASK			0x1	/* HSI_COMMENT: cf22en */
#define XSTORM_ISCSI_CONN_AG_CTX_SLOW_PATH_EN_SHIFT			4
#define XSTORM_ISCSI_CONN_AG_CTX_PROC_ONLY_CLEANUP_EN_MASK		0x1	/* HSI_COMMENT: cf23en */
#define XSTORM_ISCSI_CONN_AG_CTX_PROC_ONLY_CLEANUP_EN_SHIFT		5
#define XSTORM_ISCSI_CONN_AG_CTX_RULE0EN_MASK				0x1	/* HSI_COMMENT: rule0en */
#define XSTORM_ISCSI_CONN_AG_CTX_RULE0EN_SHIFT				6
#define XSTORM_ISCSI_CONN_AG_CTX_MORE_TO_SEND_DEC_RULE_EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define XSTORM_ISCSI_CONN_AG_CTX_MORE_TO_SEND_DEC_RULE_EN_SHIFT		7
	u8 flags11;
#define XSTORM_ISCSI_CONN_AG_CTX_TX_BLOCKED_EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define XSTORM_ISCSI_CONN_AG_CTX_TX_BLOCKED_EN_SHIFT			0
#define XSTORM_ISCSI_CONN_AG_CTX_RULE3EN_MASK				0x1	/* HSI_COMMENT: rule3en */
#define XSTORM_ISCSI_CONN_AG_CTX_RULE3EN_SHIFT				1
#define XSTORM_ISCSI_CONN_AG_CTX_RESERVED3_MASK				0x1	/* HSI_COMMENT: rule4en */
#define XSTORM_ISCSI_CONN_AG_CTX_RESERVED3_SHIFT			2
#define XSTORM_ISCSI_CONN_AG_CTX_RULE5EN_MASK				0x1	/* HSI_COMMENT: rule5en */
#define XSTORM_ISCSI_CONN_AG_CTX_RULE5EN_SHIFT				3
#define XSTORM_ISCSI_CONN_AG_CTX_RULE6EN_MASK				0x1	/* HSI_COMMENT: rule6en */
#define XSTORM_ISCSI_CONN_AG_CTX_RULE6EN_SHIFT				4
#define XSTORM_ISCSI_CONN_AG_CTX_RULE7EN_MASK				0x1	/* HSI_COMMENT: rule7en */
#define XSTORM_ISCSI_CONN_AG_CTX_RULE7EN_SHIFT				5
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED1_MASK			0x1	/* HSI_COMMENT: rule8en */
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED1_SHIFT			6
#define XSTORM_ISCSI_CONN_AG_CTX_RULE9EN_MASK				0x1	/* HSI_COMMENT: rule9en */
#define XSTORM_ISCSI_CONN_AG_CTX_RULE9EN_SHIFT				7
	u8 flags12;
#define XSTORM_ISCSI_CONN_AG_CTX_SQ_DEC_RULE_EN_MASK			0x1	/* HSI_COMMENT: rule10en */
#define XSTORM_ISCSI_CONN_AG_CTX_SQ_DEC_RULE_EN_SHIFT			0
#define XSTORM_ISCSI_CONN_AG_CTX_RULE11EN_MASK				0x1	/* HSI_COMMENT: rule11en */
#define XSTORM_ISCSI_CONN_AG_CTX_RULE11EN_SHIFT				1
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED2_MASK			0x1	/* HSI_COMMENT: rule12en */
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED2_SHIFT			2
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED3_MASK			0x1	/* HSI_COMMENT: rule13en */
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED3_SHIFT			3
#define XSTORM_ISCSI_CONN_AG_CTX_RULE14EN_MASK				0x1	/* HSI_COMMENT: rule14en */
#define XSTORM_ISCSI_CONN_AG_CTX_RULE14EN_SHIFT				4
#define XSTORM_ISCSI_CONN_AG_CTX_RULE15EN_MASK				0x1	/* HSI_COMMENT: rule15en */
#define XSTORM_ISCSI_CONN_AG_CTX_RULE15EN_SHIFT				5
#define XSTORM_ISCSI_CONN_AG_CTX_RULE16EN_MASK				0x1	/* HSI_COMMENT: rule16en */
#define XSTORM_ISCSI_CONN_AG_CTX_RULE16EN_SHIFT				6
#define XSTORM_ISCSI_CONN_AG_CTX_RULE17EN_MASK				0x1	/* HSI_COMMENT: rule17en */
#define XSTORM_ISCSI_CONN_AG_CTX_RULE17EN_SHIFT				7
	u8 flags13;
#define XSTORM_ISCSI_CONN_AG_CTX_R2TQ_DEC_RULE_EN_MASK			0x1	/* HSI_COMMENT: rule18en */
#define XSTORM_ISCSI_CONN_AG_CTX_R2TQ_DEC_RULE_EN_SHIFT			0
#define XSTORM_ISCSI_CONN_AG_CTX_HQ_DEC_RULE_EN_MASK			0x1	/* HSI_COMMENT: rule19en */
#define XSTORM_ISCSI_CONN_AG_CTX_HQ_DEC_RULE_EN_SHIFT			1
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED4_MASK			0x1	/* HSI_COMMENT: rule20en */
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED4_SHIFT			2
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED5_MASK			0x1	/* HSI_COMMENT: rule21en */
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED5_SHIFT			3
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED6_MASK			0x1	/* HSI_COMMENT: rule22en */
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED6_SHIFT			4
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED7_MASK			0x1	/* HSI_COMMENT: rule23en */
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED7_SHIFT			5
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED8_MASK			0x1	/* HSI_COMMENT: rule24en */
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED8_SHIFT			6
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED9_MASK			0x1	/* HSI_COMMENT: rule25en */
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED9_SHIFT			7
	u8 flags14;
#define XSTORM_ISCSI_CONN_AG_CTX_BIT16_MASK				0x1	/* HSI_COMMENT: bit16 */
#define XSTORM_ISCSI_CONN_AG_CTX_BIT16_SHIFT				0
#define XSTORM_ISCSI_CONN_AG_CTX_BIT17_MASK				0x1	/* HSI_COMMENT: bit17 */
#define XSTORM_ISCSI_CONN_AG_CTX_BIT17_SHIFT				1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT18_MASK				0x1	/* HSI_COMMENT: bit18 */
#define XSTORM_ISCSI_CONN_AG_CTX_BIT18_SHIFT				2
#define XSTORM_ISCSI_CONN_AG_CTX_BIT19_MASK				0x1	/* HSI_COMMENT: bit19 */
#define XSTORM_ISCSI_CONN_AG_CTX_BIT19_SHIFT				3
#define XSTORM_ISCSI_CONN_AG_CTX_BIT20_MASK				0x1	/* HSI_COMMENT: bit20 */
#define XSTORM_ISCSI_CONN_AG_CTX_BIT20_SHIFT				4
#define XSTORM_ISCSI_CONN_AG_CTX_DUMMY_READ_DONE_MASK			0x1	/* HSI_COMMENT: bit21 */
#define XSTORM_ISCSI_CONN_AG_CTX_DUMMY_READ_DONE_SHIFT			5
#define XSTORM_ISCSI_CONN_AG_CTX_PROC_ONLY_CLEANUP_MASK			0x3	/* HSI_COMMENT: cf23 */
#define XSTORM_ISCSI_CONN_AG_CTX_PROC_ONLY_CLEANUP_SHIFT		6
	u8 byte2;		/* HSI_COMMENT: byte2 */
	__le16 physical_q0;	/* HSI_COMMENT: physical_q0 */
	__le16 physical_q1;	/* HSI_COMMENT: physical_q1 */
	__le16 dummy_dorq_var;	/* HSI_COMMENT: physical_q2 */
	__le16 sq_cons;		/* HSI_COMMENT: word3 */
	__le16 sq_prod;		/* HSI_COMMENT: word4 */
	__le16 word5;		/* HSI_COMMENT: word5 */
	__le16 slow_io_total_data_tx_update;	/* HSI_COMMENT: conn_dpi */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	u8 byte4;		/* HSI_COMMENT: byte4 */
	u8 byte5;		/* HSI_COMMENT: byte5 */
	u8 byte6;		/* HSI_COMMENT: byte6 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 more_to_send_seq;	/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
	__le32 reg5;		/* HSI_COMMENT: cf_array0 */
	__le32 hq_scan_next_relevant_ack;	/* HSI_COMMENT: cf_array1 */
	__le16 r2tq_prod;	/* HSI_COMMENT: word7 */
	__le16 r2tq_cons;	/* HSI_COMMENT: word8 */
	__le16 hq_prod;		/* HSI_COMMENT: word9 */
	__le16 hq_cons;		/* HSI_COMMENT: word10 */
	__le32 remain_seq;	/* HSI_COMMENT: reg7 */
	__le32 bytes_to_next_pdu;	/* HSI_COMMENT: reg8 */
	__le32 hq_tcp_seq;	/* HSI_COMMENT: reg9 */
	u8 byte7;		/* HSI_COMMENT: byte7 */
	u8 byte8;		/* HSI_COMMENT: byte8 */
	u8 byte9;		/* HSI_COMMENT: byte9 */
	u8 byte10;		/* HSI_COMMENT: byte10 */
	u8 byte11;		/* HSI_COMMENT: byte11 */
	u8 byte12;		/* HSI_COMMENT: byte12 */
	u8 byte13;		/* HSI_COMMENT: byte13 */
	u8 byte14;		/* HSI_COMMENT: byte14 */
	u8 byte15;		/* HSI_COMMENT: byte15 */
	u8 e5_reserved;		/* HSI_COMMENT: e5_reserved */
	__le16 word11;		/* HSI_COMMENT: word11 */
	__le32 reg10;		/* HSI_COMMENT: reg10 */
	__le32 reg11;		/* HSI_COMMENT: reg11 */
	__le32 exp_stat_sn;	/* HSI_COMMENT: reg12 */
	__le32 ongoing_fast_rxmit_seq;	/* HSI_COMMENT: reg13 */
	__le32 reg14;		/* HSI_COMMENT: reg14 */
	__le32 reg15;		/* HSI_COMMENT: reg15 */
	__le32 reg16;		/* HSI_COMMENT: reg16 */
	__le32 reg17;		/* HSI_COMMENT: reg17 */
};

struct tstorm_iscsi_conn_ag_ctx {
	u8 reserved0;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define TSTORM_ISCSI_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define TSTORM_ISCSI_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define TSTORM_ISCSI_CONN_AG_CTX_BIT1_MASK			0x1	/* HSI_COMMENT: exist_in_qm1 */
#define TSTORM_ISCSI_CONN_AG_CTX_BIT1_SHIFT			1
#define TSTORM_ISCSI_CONN_AG_CTX_BIT2_MASK			0x1	/* HSI_COMMENT: bit2 */
#define TSTORM_ISCSI_CONN_AG_CTX_BIT2_SHIFT			2
#define TSTORM_ISCSI_CONN_AG_CTX_BIT3_MASK			0x1	/* HSI_COMMENT: bit3 */
#define TSTORM_ISCSI_CONN_AG_CTX_BIT3_SHIFT			3
#define TSTORM_ISCSI_CONN_AG_CTX_BIT4_MASK			0x1	/* HSI_COMMENT: bit4 */
#define TSTORM_ISCSI_CONN_AG_CTX_BIT4_SHIFT			4
#define TSTORM_ISCSI_CONN_AG_CTX_BIT5_MASK			0x1	/* HSI_COMMENT: bit5 */
#define TSTORM_ISCSI_CONN_AG_CTX_BIT5_SHIFT			5
#define TSTORM_ISCSI_CONN_AG_CTX_CF0_MASK			0x3	/* HSI_COMMENT: timer0cf */
#define TSTORM_ISCSI_CONN_AG_CTX_CF0_SHIFT			6
	u8 flags1;
#define TSTORM_ISCSI_CONN_AG_CTX_P2T_FLUSH_CF_MASK		0x3	/* HSI_COMMENT: timer1cf */
#define TSTORM_ISCSI_CONN_AG_CTX_P2T_FLUSH_CF_SHIFT		0
#define TSTORM_ISCSI_CONN_AG_CTX_M2T_FLUSH_CF_MASK		0x3	/* HSI_COMMENT: timer2cf */
#define TSTORM_ISCSI_CONN_AG_CTX_M2T_FLUSH_CF_SHIFT		2
#define TSTORM_ISCSI_CONN_AG_CTX_TIMER_STOP_ALL_MASK		0x3	/* HSI_COMMENT: timer_stop_all */
#define TSTORM_ISCSI_CONN_AG_CTX_TIMER_STOP_ALL_SHIFT		4
#define TSTORM_ISCSI_CONN_AG_CTX_CF4_MASK			0x3	/* HSI_COMMENT: cf4 */
#define TSTORM_ISCSI_CONN_AG_CTX_CF4_SHIFT			6
	u8 flags2;
#define TSTORM_ISCSI_CONN_AG_CTX_CF5_MASK			0x3	/* HSI_COMMENT: cf5 */
#define TSTORM_ISCSI_CONN_AG_CTX_CF5_SHIFT			0
#define TSTORM_ISCSI_CONN_AG_CTX_CF6_MASK			0x3	/* HSI_COMMENT: cf6 */
#define TSTORM_ISCSI_CONN_AG_CTX_CF6_SHIFT			2
#define TSTORM_ISCSI_CONN_AG_CTX_CF7_MASK			0x3	/* HSI_COMMENT: cf7 */
#define TSTORM_ISCSI_CONN_AG_CTX_CF7_SHIFT			4
#define TSTORM_ISCSI_CONN_AG_CTX_CF8_MASK			0x3	/* HSI_COMMENT: cf8 */
#define TSTORM_ISCSI_CONN_AG_CTX_CF8_SHIFT			6
	u8 flags3;
#define TSTORM_ISCSI_CONN_AG_CTX_FLUSH_Q0_MASK			0x3	/* HSI_COMMENT: cf9 */
#define TSTORM_ISCSI_CONN_AG_CTX_FLUSH_Q0_SHIFT			0
#define TSTORM_ISCSI_CONN_AG_CTX_FLUSH_OOO_ISLES_CF_MASK	0x3	/* HSI_COMMENT: cf10 */
#define TSTORM_ISCSI_CONN_AG_CTX_FLUSH_OOO_ISLES_CF_SHIFT	2
#define TSTORM_ISCSI_CONN_AG_CTX_CF0EN_MASK			0x1	/* HSI_COMMENT: cf0en */
#define TSTORM_ISCSI_CONN_AG_CTX_CF0EN_SHIFT			4
#define TSTORM_ISCSI_CONN_AG_CTX_P2T_FLUSH_CF_EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define TSTORM_ISCSI_CONN_AG_CTX_P2T_FLUSH_CF_EN_SHIFT		5
#define TSTORM_ISCSI_CONN_AG_CTX_M2T_FLUSH_CF_EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define TSTORM_ISCSI_CONN_AG_CTX_M2T_FLUSH_CF_EN_SHIFT		6
#define TSTORM_ISCSI_CONN_AG_CTX_TIMER_STOP_ALL_EN_MASK		0x1	/* HSI_COMMENT: cf3en */
#define TSTORM_ISCSI_CONN_AG_CTX_TIMER_STOP_ALL_EN_SHIFT	7
	u8 flags4;
#define TSTORM_ISCSI_CONN_AG_CTX_CF4EN_MASK			0x1	/* HSI_COMMENT: cf4en */
#define TSTORM_ISCSI_CONN_AG_CTX_CF4EN_SHIFT			0
#define TSTORM_ISCSI_CONN_AG_CTX_CF5EN_MASK			0x1	/* HSI_COMMENT: cf5en */
#define TSTORM_ISCSI_CONN_AG_CTX_CF5EN_SHIFT			1
#define TSTORM_ISCSI_CONN_AG_CTX_CF6EN_MASK			0x1	/* HSI_COMMENT: cf6en */
#define TSTORM_ISCSI_CONN_AG_CTX_CF6EN_SHIFT			2
#define TSTORM_ISCSI_CONN_AG_CTX_CF7EN_MASK			0x1	/* HSI_COMMENT: cf7en */
#define TSTORM_ISCSI_CONN_AG_CTX_CF7EN_SHIFT			3
#define TSTORM_ISCSI_CONN_AG_CTX_CF8EN_MASK			0x1	/* HSI_COMMENT: cf8en */
#define TSTORM_ISCSI_CONN_AG_CTX_CF8EN_SHIFT			4
#define TSTORM_ISCSI_CONN_AG_CTX_FLUSH_Q0_EN_MASK		0x1	/* HSI_COMMENT: cf9en */
#define TSTORM_ISCSI_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT		5
#define TSTORM_ISCSI_CONN_AG_CTX_FLUSH_OOO_ISLES_CF_EN_MASK	0x1	/* HSI_COMMENT: cf10en */
#define TSTORM_ISCSI_CONN_AG_CTX_FLUSH_OOO_ISLES_CF_EN_SHIFT	6
#define TSTORM_ISCSI_CONN_AG_CTX_RULE0EN_MASK			0x1	/* HSI_COMMENT: rule0en */
#define TSTORM_ISCSI_CONN_AG_CTX_RULE0EN_SHIFT			7
	u8 flags5;
#define TSTORM_ISCSI_CONN_AG_CTX_RULE1EN_MASK			0x1	/* HSI_COMMENT: rule1en */
#define TSTORM_ISCSI_CONN_AG_CTX_RULE1EN_SHIFT			0
#define TSTORM_ISCSI_CONN_AG_CTX_RULE2EN_MASK			0x1	/* HSI_COMMENT: rule2en */
#define TSTORM_ISCSI_CONN_AG_CTX_RULE2EN_SHIFT			1
#define TSTORM_ISCSI_CONN_AG_CTX_RULE3EN_MASK			0x1	/* HSI_COMMENT: rule3en */
#define TSTORM_ISCSI_CONN_AG_CTX_RULE3EN_SHIFT			2
#define TSTORM_ISCSI_CONN_AG_CTX_RULE4EN_MASK			0x1	/* HSI_COMMENT: rule4en */
#define TSTORM_ISCSI_CONN_AG_CTX_RULE4EN_SHIFT			3
#define TSTORM_ISCSI_CONN_AG_CTX_RULE5EN_MASK			0x1	/* HSI_COMMENT: rule5en */
#define TSTORM_ISCSI_CONN_AG_CTX_RULE5EN_SHIFT			4
#define TSTORM_ISCSI_CONN_AG_CTX_RULE6EN_MASK			0x1	/* HSI_COMMENT: rule6en */
#define TSTORM_ISCSI_CONN_AG_CTX_RULE6EN_SHIFT			5
#define TSTORM_ISCSI_CONN_AG_CTX_RULE7EN_MASK			0x1	/* HSI_COMMENT: rule7en */
#define TSTORM_ISCSI_CONN_AG_CTX_RULE7EN_SHIFT			6
#define TSTORM_ISCSI_CONN_AG_CTX_RULE8EN_MASK			0x1	/* HSI_COMMENT: rule8en */
#define TSTORM_ISCSI_CONN_AG_CTX_RULE8EN_SHIFT			7
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 rx_tcp_checksum_err_cnt;	/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le32 reg4;		/* HSI_COMMENT: reg4 */
	__le32 reg5;		/* HSI_COMMENT: reg5 */
	__le32 reg6;		/* HSI_COMMENT: reg6 */
	__le32 reg7;		/* HSI_COMMENT: reg7 */
	__le32 reg8;		/* HSI_COMMENT: reg8 */
	u8 cid_offload_cnt;	/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: word0 */
};

struct ustorm_iscsi_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define USTORM_ISCSI_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define USTORM_ISCSI_CONN_AG_CTX_BIT0_SHIFT		0
#define USTORM_ISCSI_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define USTORM_ISCSI_CONN_AG_CTX_BIT1_SHIFT		1
#define USTORM_ISCSI_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: timer0cf */
#define USTORM_ISCSI_CONN_AG_CTX_CF0_SHIFT		2
#define USTORM_ISCSI_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: timer1cf */
#define USTORM_ISCSI_CONN_AG_CTX_CF1_SHIFT		4
#define USTORM_ISCSI_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: timer2cf */
#define USTORM_ISCSI_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define USTORM_ISCSI_CONN_AG_CTX_CF3_MASK		0x3	/* HSI_COMMENT: timer_stop_all */
#define USTORM_ISCSI_CONN_AG_CTX_CF3_SHIFT		0
#define USTORM_ISCSI_CONN_AG_CTX_CF4_MASK		0x3	/* HSI_COMMENT: cf4 */
#define USTORM_ISCSI_CONN_AG_CTX_CF4_SHIFT		2
#define USTORM_ISCSI_CONN_AG_CTX_CF5_MASK		0x3	/* HSI_COMMENT: cf5 */
#define USTORM_ISCSI_CONN_AG_CTX_CF5_SHIFT		4
#define USTORM_ISCSI_CONN_AG_CTX_CF6_MASK		0x3	/* HSI_COMMENT: cf6 */
#define USTORM_ISCSI_CONN_AG_CTX_CF6_SHIFT		6
	u8 flags2;
#define USTORM_ISCSI_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define USTORM_ISCSI_CONN_AG_CTX_CF0EN_SHIFT		0
#define USTORM_ISCSI_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define USTORM_ISCSI_CONN_AG_CTX_CF1EN_SHIFT		1
#define USTORM_ISCSI_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define USTORM_ISCSI_CONN_AG_CTX_CF2EN_SHIFT		2
#define USTORM_ISCSI_CONN_AG_CTX_CF3EN_MASK		0x1	/* HSI_COMMENT: cf3en */
#define USTORM_ISCSI_CONN_AG_CTX_CF3EN_SHIFT		3
#define USTORM_ISCSI_CONN_AG_CTX_CF4EN_MASK		0x1	/* HSI_COMMENT: cf4en */
#define USTORM_ISCSI_CONN_AG_CTX_CF4EN_SHIFT		4
#define USTORM_ISCSI_CONN_AG_CTX_CF5EN_MASK		0x1	/* HSI_COMMENT: cf5en */
#define USTORM_ISCSI_CONN_AG_CTX_CF5EN_SHIFT		5
#define USTORM_ISCSI_CONN_AG_CTX_CF6EN_MASK		0x1	/* HSI_COMMENT: cf6en */
#define USTORM_ISCSI_CONN_AG_CTX_CF6EN_SHIFT		6
#define USTORM_ISCSI_CONN_AG_CTX_RULE0EN_MASK		0x1	/* HSI_COMMENT: rule0en */
#define USTORM_ISCSI_CONN_AG_CTX_RULE0EN_SHIFT		7
	u8 flags3;
#define USTORM_ISCSI_CONN_AG_CTX_RULE1EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define USTORM_ISCSI_CONN_AG_CTX_RULE1EN_SHIFT		0
#define USTORM_ISCSI_CONN_AG_CTX_RULE2EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define USTORM_ISCSI_CONN_AG_CTX_RULE2EN_SHIFT		1
#define USTORM_ISCSI_CONN_AG_CTX_RULE3EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define USTORM_ISCSI_CONN_AG_CTX_RULE3EN_SHIFT		2
#define USTORM_ISCSI_CONN_AG_CTX_RULE4EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define USTORM_ISCSI_CONN_AG_CTX_RULE4EN_SHIFT		3
#define USTORM_ISCSI_CONN_AG_CTX_RULE5EN_MASK		0x1	/* HSI_COMMENT: rule5en */
#define USTORM_ISCSI_CONN_AG_CTX_RULE5EN_SHIFT		4
#define USTORM_ISCSI_CONN_AG_CTX_RULE6EN_MASK		0x1	/* HSI_COMMENT: rule6en */
#define USTORM_ISCSI_CONN_AG_CTX_RULE6EN_SHIFT		5
#define USTORM_ISCSI_CONN_AG_CTX_RULE7EN_MASK		0x1	/* HSI_COMMENT: rule7en */
#define USTORM_ISCSI_CONN_AG_CTX_RULE7EN_SHIFT		6
#define USTORM_ISCSI_CONN_AG_CTX_RULE8EN_MASK		0x1	/* HSI_COMMENT: rule8en */
#define USTORM_ISCSI_CONN_AG_CTX_RULE8EN_SHIFT		7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: conn_dpi */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
};

/* The iscsi storm connection context of Tstorm */
struct tstorm_iscsi_conn_st_ctx {
	__le32 reserved[44];
};

struct mstorm_iscsi_conn_ag_ctx {
	u8 reserved;		/* HSI_COMMENT: cdu_validation */
	u8 state;		/* HSI_COMMENT: state */
	u8 flags0;
#define MSTORM_ISCSI_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define MSTORM_ISCSI_CONN_AG_CTX_BIT0_SHIFT		0
#define MSTORM_ISCSI_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define MSTORM_ISCSI_CONN_AG_CTX_BIT1_SHIFT		1
#define MSTORM_ISCSI_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: cf0 */
#define MSTORM_ISCSI_CONN_AG_CTX_CF0_SHIFT		2
#define MSTORM_ISCSI_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: cf1 */
#define MSTORM_ISCSI_CONN_AG_CTX_CF1_SHIFT		4
#define MSTORM_ISCSI_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: cf2 */
#define MSTORM_ISCSI_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define MSTORM_ISCSI_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define MSTORM_ISCSI_CONN_AG_CTX_CF0EN_SHIFT		0
#define MSTORM_ISCSI_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define MSTORM_ISCSI_CONN_AG_CTX_CF1EN_SHIFT		1
#define MSTORM_ISCSI_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define MSTORM_ISCSI_CONN_AG_CTX_CF2EN_SHIFT		2
#define MSTORM_ISCSI_CONN_AG_CTX_RULE0EN_MASK		0x1	/* HSI_COMMENT: rule0en */
#define MSTORM_ISCSI_CONN_AG_CTX_RULE0EN_SHIFT		3
#define MSTORM_ISCSI_CONN_AG_CTX_RULE1EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define MSTORM_ISCSI_CONN_AG_CTX_RULE1EN_SHIFT		4
#define MSTORM_ISCSI_CONN_AG_CTX_RULE2EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define MSTORM_ISCSI_CONN_AG_CTX_RULE2EN_SHIFT		5
#define MSTORM_ISCSI_CONN_AG_CTX_RULE3EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define MSTORM_ISCSI_CONN_AG_CTX_RULE3EN_SHIFT		6
#define MSTORM_ISCSI_CONN_AG_CTX_RULE4EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define MSTORM_ISCSI_CONN_AG_CTX_RULE4EN_SHIFT		7
	__le16 word0;		/* HSI_COMMENT: word0 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
};

/* Combined iSCSI and TCP storm connection of Mstorm */
struct mstorm_iscsi_tcp_conn_st_ctx {
	__le32 reserved_tcp[20];
	__le32 reserved_iscsi[12];
};

/* The iscsi storm context of Ustorm */
struct ustorm_iscsi_conn_st_ctx {
	__le32 reserved[52];
};

/* iscsi connection context */
struct iscsi_conn_context {
	struct ystorm_iscsi_conn_st_ctx ystorm_st_context;	/* HSI_COMMENT: ystorm storm context */
	struct pstorm_iscsi_tcp_conn_st_ctx pstorm_st_context;	/* HSI_COMMENT: pstorm storm context */
	struct regpair pstorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct pb_context xpb2_context;	/* HSI_COMMENT: xpb2 context */
	struct xstorm_iscsi_tcp_conn_st_ctx xstorm_st_context;	/* HSI_COMMENT: xstorm storm context */
	struct regpair xstorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct xstorm_iscsi_conn_ag_ctx xstorm_ag_context;	/* HSI_COMMENT: xstorm aggregative context */
	struct tstorm_iscsi_conn_ag_ctx tstorm_ag_context;	/* HSI_COMMENT: tstorm aggregative context */
	struct regpair tstorm_ag_padding[2];	/* HSI_COMMENT: padding */
	struct timers_context timer_context;	/* HSI_COMMENT: timer context */
	struct ustorm_iscsi_conn_ag_ctx ustorm_ag_context;	/* HSI_COMMENT: ustorm aggregative context */
	struct pb_context upb_context;	/* HSI_COMMENT: upb context */
	struct tstorm_iscsi_conn_st_ctx tstorm_st_context;	/* HSI_COMMENT: tstorm storm context */
	struct regpair tstorm_st_padding[2];	/* HSI_COMMENT: padding */
	struct mstorm_iscsi_conn_ag_ctx mstorm_ag_context;	/* HSI_COMMENT: mstorm aggregative context */
	struct mstorm_iscsi_tcp_conn_st_ctx mstorm_st_context;	/* HSI_COMMENT: mstorm storm context */
	struct ustorm_iscsi_conn_st_ctx ustorm_st_context;	/* HSI_COMMENT: ustorm storm context */
};

/* iSCSI init params passed by driver to FW in iSCSI init ramrod  */
struct iscsi_init_ramrod_params {
	struct iscsi_spe_func_init iscsi_init_spe;	/* HSI_COMMENT: parameters initialized by the miniport and handed to bus-driver */
	struct tcp_init_params tcp_init;	/* HSI_COMMENT: TCP parameters initialized by the bus-driver */
};

struct ystorm_iscsi_conn_ag_ctx {
	u8 byte0;		/* HSI_COMMENT: cdu_validation */
	u8 byte1;		/* HSI_COMMENT: state */
	u8 flags0;
#define YSTORM_ISCSI_CONN_AG_CTX_BIT0_MASK		0x1	/* HSI_COMMENT: exist_in_qm0 */
#define YSTORM_ISCSI_CONN_AG_CTX_BIT0_SHIFT		0
#define YSTORM_ISCSI_CONN_AG_CTX_BIT1_MASK		0x1	/* HSI_COMMENT: exist_in_qm1 */
#define YSTORM_ISCSI_CONN_AG_CTX_BIT1_SHIFT		1
#define YSTORM_ISCSI_CONN_AG_CTX_CF0_MASK		0x3	/* HSI_COMMENT: cf0 */
#define YSTORM_ISCSI_CONN_AG_CTX_CF0_SHIFT		2
#define YSTORM_ISCSI_CONN_AG_CTX_CF1_MASK		0x3	/* HSI_COMMENT: cf1 */
#define YSTORM_ISCSI_CONN_AG_CTX_CF1_SHIFT		4
#define YSTORM_ISCSI_CONN_AG_CTX_CF2_MASK		0x3	/* HSI_COMMENT: cf2 */
#define YSTORM_ISCSI_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define YSTORM_ISCSI_CONN_AG_CTX_CF0EN_MASK		0x1	/* HSI_COMMENT: cf0en */
#define YSTORM_ISCSI_CONN_AG_CTX_CF0EN_SHIFT		0
#define YSTORM_ISCSI_CONN_AG_CTX_CF1EN_MASK		0x1	/* HSI_COMMENT: cf1en */
#define YSTORM_ISCSI_CONN_AG_CTX_CF1EN_SHIFT		1
#define YSTORM_ISCSI_CONN_AG_CTX_CF2EN_MASK		0x1	/* HSI_COMMENT: cf2en */
#define YSTORM_ISCSI_CONN_AG_CTX_CF2EN_SHIFT		2
#define YSTORM_ISCSI_CONN_AG_CTX_RULE0EN_MASK		0x1	/* HSI_COMMENT: rule0en */
#define YSTORM_ISCSI_CONN_AG_CTX_RULE0EN_SHIFT		3
#define YSTORM_ISCSI_CONN_AG_CTX_RULE1EN_MASK		0x1	/* HSI_COMMENT: rule1en */
#define YSTORM_ISCSI_CONN_AG_CTX_RULE1EN_SHIFT		4
#define YSTORM_ISCSI_CONN_AG_CTX_RULE2EN_MASK		0x1	/* HSI_COMMENT: rule2en */
#define YSTORM_ISCSI_CONN_AG_CTX_RULE2EN_SHIFT		5
#define YSTORM_ISCSI_CONN_AG_CTX_RULE3EN_MASK		0x1	/* HSI_COMMENT: rule3en */
#define YSTORM_ISCSI_CONN_AG_CTX_RULE3EN_SHIFT		6
#define YSTORM_ISCSI_CONN_AG_CTX_RULE4EN_MASK		0x1	/* HSI_COMMENT: rule4en */
#define YSTORM_ISCSI_CONN_AG_CTX_RULE4EN_SHIFT		7
	u8 byte2;		/* HSI_COMMENT: byte2 */
	u8 byte3;		/* HSI_COMMENT: byte3 */
	__le16 word0;		/* HSI_COMMENT: word0 */
	__le32 reg0;		/* HSI_COMMENT: reg0 */
	__le32 reg1;		/* HSI_COMMENT: reg1 */
	__le16 word1;		/* HSI_COMMENT: word1 */
	__le16 word2;		/* HSI_COMMENT: word2 */
	__le16 word3;		/* HSI_COMMENT: word3 */
	__le16 word4;		/* HSI_COMMENT: word4 */
	__le32 reg2;		/* HSI_COMMENT: reg2 */
	__le32 reg3;		/* HSI_COMMENT: reg3 */
};

#define NUM_STORMS    6

/* Returns the VOQ based on port and TC */
#define VOQ(port, tc, max_phys_tcs_per_port)    (tc ==                          \
                                                 PURE_LB_TC ? NUM_OF_PHYS_TCS * \
                                                 MAX_NUM_PORTS_BB +             \
                                                 port : port *                  \
                                                 max_phys_tcs_per_port + tc)

struct init_qm_pq_params;

/**
 * @brief qed_qm_pf_mem_size - Prepare QM ILT sizes
 *
 * Returns the required host memory size in 4KB units.
 * Must be called before all QM init HSI functions.
 *
 * @param num_pf_cids -		no. of connections used by this PF
 * @param num_vf_cids -		no. of connections used by VFs of this PF
 * @param num_tids -		no. of tasks used by this PF
 * @param num_pf_pqs -		no. of Tx PQs associated with this PF
 * @param num_vf_pqs -		no. of Tx PQs associated with a VF
 *
 * @return The required host memory size in 4KB units.
 */
u32 qed_qm_pf_mem_size(u32 num_pf_cids,
		       u32 num_vf_cids,
		       u32 num_tids, u16 num_pf_pqs, u16 num_vf_pqs);

/**
 * @brief qed_qm_common_rt_init - Prepare QM runtime init values for the
 * engine phase.
 *
 * @param p_hwfn -			  HW device data
 * @param max_ports_per_engine -  max no. of ports per engine in HW
 * @param max_phys_tcs_per_port	- max no. of physical TCs per port in HW
 * @param pf_rl_en -		  enable per-PF rate limiters
 * @param pf_wfq_en -		  enable per-PF WFQ
 * @param global_rl_en -	  enable global rate limiters
 * @param vport_wfq_en -	  enable per-VPORT WFQ
 * @param port_params -		  array with parameters for each port.
 * @param global_rl_params -	  array with parameters for each global RL.
 *				  If NULL, global RLs are not configured.
 *
 * @return 0 on success, -1 on error.
 */
int qed_qm_common_rt_init(struct qed_hwfn *p_hwfn,
			  u8
			  max_ports_per_engine,
			  u8
			  max_phys_tcs_per_port,
			  bool pf_rl_en,
			  bool pf_wfq_en,
			  bool global_rl_en,
			  bool vport_wfq_en,
			  struct init_qm_port_params port_params[MAX_NUM_PORTS],
			  struct init_qm_global_rl_params
			  global_rl_params[COMMON_MAX_QM_GLOBAL_RLS]);

/**
 * @brief qed_qm_pf_rt_init - Prepare QM runtime init values for the PF phase
 *
 * @param p_hwfn -			  HW device data
 * @param p_ptt -			  ptt window used for writing the registers
 * @param pf_id -		  PF ID
 * @param max_phys_tcs_per_port	- max no. of physical TCs per port in HW
 * @param is_pf_loading -	  indicates if the PF is currently loading,
 *				  i.e. it has no allocated QM resources.
 * @param num_pf_cids -		  no. of connections used by this PF
 * @param num_vf_cids -		  no. of connections used by VFs of this PF
 * @param num_tids -		  no. of tasks used by this PF
 * @param start_pq -		  first Tx PQ ID associated with this PF
 * @param num_pf_pqs -		  no. of VF Tx PQs associated with this PF
 * @param num_vf_pqs -		  no. of PF Tx PQs associated with a VF
 * @param start_vport -		  first VPORT ID associated with this PF
 * @param num_vports -		  number of VPORTs associated with this PF
 * @param pf_wfq_weight -	  PF WFQ weight. if the PF WFQ feature is
 *				  disabled, the weight must be 0. otherwise,
 *				  the weight must be non-zero.
 * @param pf_rl -		  rate limit in Mbps units. a value of 0
 *				  means don't configure. ignored if PF RL is
 *				  globally disabled.
 * @param pq_params -		  array of size (num_pf_only_pqs+num_pf_vf_pqs+
 *				  num_vf_only_pqs) with parameters for each Tx
 *				  PQ associated with the specified PF.
 * @param vport_params -	  array of size num_vports with parameters for
 *				  each associated VPORT.
 *
 * @return 0 on success, -1 on error.
 */
int qed_qm_pf_rt_init(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt,
		      u8 pf_id,
		      u8 max_phys_tcs_per_port,
		      bool is_pf_loading,
		      u32 num_pf_cids,
		      u32 num_vf_cids,
		      u32 num_tids,
		      u16 start_pq,
		      u16 num_pf_pqs,
		      u16 num_vf_pqs,
		      u16 start_vport,
		      u16 num_vports,
		      u16 start_rl,
		      u16 num_rls,
		      u16 pf_wfq_weight,
		      u32 pf_rl,
		      u32 link_speed,
		      struct init_qm_pq_params *pq_params,
		      struct init_qm_vport_params *vport_params,
		      struct init_qm_rl_params *rl_params);

/**
 * @brief qed_init_pf_wfq - Initializes the WFQ weight of the specified PF
 *
 * @param p_hwfn -	   HW device data
 * @param p_ptt -	   ptt window used for writing the registers
 * @param pf_id	-  PF ID
 * @param weight - PF WFQ weight. Must be non-zero.
 *
 * @return 0 on success, -1 on error.
 */
int qed_init_pf_wfq(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt, u8 pf_id, u16 weight);

/**
 * @brief qed_init_pf_rl - Initializes the rate limit of the specified PF
 *
 * @param p_hwfn
 * @param p_ptt -   ptt window used for writing the registers
 * @param pf_id	- PF ID
 * @param pf_rl	- rate limit in Mbps units
 *
 * @return 0 on success, -1 on error.
 */
int qed_init_pf_rl(struct qed_hwfn *p_hwfn,
		   struct qed_ptt *p_ptt, u8 pf_id, u32 pf_rl);

/**
 * @brief qed_init_vport_wfq - Initializes the WFQ weight of the specified VPORT
 *
 * @param p_hwfn -		   HW device data
 * @param p_ptt -		   ptt window used for writing the registers
 * @param first_tx_pq_id - An array containing the first Tx PQ ID associated
 *                         with the VPORT for each TC. This array is filled by
 *                         qed_qm_pf_rt_init
 * @param weight -		   VPORT WFQ weight.
 *
 * @return 0 on success, -1 on error.
 */
int qed_init_vport_wfq(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       u16 first_tx_pq_id[NUM_OF_TCS], u16 weight);

/**
 * @brief qed_init_vport_tc_wfq - Initializes the WFQ weight of the specified
 * VPORT and TC.
 *
 * @param p_hwfn -		   HW device data
 * @param p_ptt -		   ptt window used for writing the registers
 * @param first_tx_pq_id -  The first Tx PQ ID associated with the VPORT and TC.
 *                          (filled by qed_qm_pf_rt_init).
 * @param weight -	   VPORT+TC WFQ weight.
 *
 * @return 0 on success, -1 on error.
 */
int qed_init_vport_tc_wfq(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt,
			  u16 first_tx_pq_id, u16 weight);

/**
 * @brief qed_init_global_rl - Initializes the rate limit of the specified
 * rate limiter.
 *
 * @param p_hwfn -		HW device data
 * @param p_ptt -		ptt window used for writing the registers
 * @param rate_limit -	rate limit in Mbps
 * @param rl_type -	rate limit type
 *
 * @return 0 on success, -1 on error.
 */
int qed_init_global_rl(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       u16 rl_id,
		       u32 rate_limit, enum init_qm_rl_type vport_rl_type);

/**
 * @brief qed_send_qm_stop_cmd - Sends a stop command to the QM
 *
 * @param p_hwfn -		   HW device data
 * @param p_ptt -		   ptt window used for writing the registers
 * @param is_release_cmd - true for release, false for stop.
 * @param is_tx_pq -	   true for Tx PQs, false for Other PQs.
 * @param start_pq -	   first PQ ID to stop
 * @param num_pqs -	   Number of PQs to stop, starting from start_pq.
 *
 * @return bool, true if successful, false if timeout occured while waiting for
 * QM command done.
 */
bool qed_send_qm_stop_cmd(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt,
			  bool is_release_cmd,
			  bool is_tx_pq, u16 start_pq, u16 num_pqs);

#ifndef UNUSED_HSI_FUNC

/**
 * @brief qed_init_nig_ets - Initializes the NIG ETS arbiter
 *
 * Based on weight/priority requirements per-TC.
 *
 * @param p_hwfn -   HW device data
 * @param p_ptt -   ptt window used for writing the registers.
 * @param req -   the NIG ETS initialization requirements.
 * @param is_lb	- if set, the loopback port arbiter is initialized, otherwise
 *		  the physical port arbiter is initialized. The pure-LB TC
 *		  requirements are ignored when is_lb is cleared.
 */
void qed_init_nig_ets(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt,
		      struct init_ets_req *req, bool is_lb);

/**
 * @brief qed_init_nig_lb_rl - Initializes the NIG LB RLs
 *
 * Based on global and per-TC rate requirements
 *
 * @param p_hwfn -	HW device data
 * @param p_ptt - ptt window used for writing the registers.
 * @param req -	the NIG LB RLs initialization requirements.
 */
void qed_init_nig_lb_rl(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt, struct init_nig_lb_rl_req *req);

#endif /* UNUSED_HSI_FUNC */

/**
 * @brief qed_init_nig_pri_tc_map - Initializes the NIG priority to TC map.
 *
 * Assumes valid arguments.
 *
 * @param p_hwfn -	HW device data
 * @param p_ptt - ptt window used for writing the registers.
 * @param req - required mapping from prioirties to TCs.
 */
void qed_init_nig_pri_tc_map(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt,
			     struct init_nig_pri_tc_map_req *req);

#ifndef UNUSED_HSI_FUNC

/**
 * @brief qed_init_prs_ets - Initializes the PRS Rx ETS arbiter
 *
 * Based on weight/priority requirements per-TC.
 *
 * @param p_hwfn -	HW device data
 * @param p_ptt - ptt window used for writing the registers.
 * @param req -	the PRS ETS initialization requirements.
 */
void qed_init_prs_ets(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, struct init_ets_req *req);

#endif /* UNUSED_HSI_FUNC */
#ifndef UNUSED_HSI_FUNC

/**
 * @brief qed_init_brb_ram - Initializes BRB RAM sizes per TC.
 *
 * Based on weight/priority requirements per-TC.
 *
 * @param p_hwfn -   HW device data
 * @param p_ptt	- ptt window used for writing the registers.
 * @param req -   the BRB RAM initialization requirements.
 */
void qed_init_brb_ram(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, struct init_brb_ram_req *req);

#endif /* UNUSED_HSI_FUNC */
#ifndef UNUSED_HSI_FUNC

/**
 * @brief qed_set_port_mf_ovlan_eth_type - initializes DORQ ethType Regs to
 * input ethType. should Be called once per port.
 *
 * @param p_hwfn -     HW device data
 * @param ethType - etherType to configure
 */
void qed_set_port_mf_ovlan_eth_type(struct qed_hwfn *p_hwfn, u32 ethType);

#endif /* UNUSED_HSI_FUNC */

/**
 * @brief qed_set_vxlan_dest_port - Initializes vxlan tunnel destination udp
 * port.
 *
 * @param p_hwfn -       HW device data
 * @param p_ptt -       ptt window used for writing the registers.
 * @param dest_port - vxlan destination udp port.
 */
void qed_set_vxlan_dest_port(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, u16 dest_port);

/**
 * @brief qed_set_vxlan_enable - Enable or disable VXLAN tunnel in HW
 *
 * @param p_hwfn -      HW device data
 * @param p_ptt -      ptt window used for writing the registers.
 * @param vxlan_enable - vxlan enable flag.
 */
void qed_set_vxlan_enable(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, bool vxlan_enable);

/**
 * @brief qed_set_gre_enable - Enable or disable GRE tunnel in HW
 *
 * @param p_hwfn -        HW device data
 * @param p_ptt -        ptt window used for writing the registers.
 * @param eth_gre_enable - eth GRE enable enable flag.
 * @param ip_gre_enable -  IP GRE enable enable flag.
 */
void qed_set_gre_enable(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt,
			bool eth_gre_enable, bool ip_gre_enable);

/**
 * @brief qed_set_geneve_dest_port - Initializes geneve tunnel destination
 * udp port.
 *
 * @param p_hwfn -       HW device data
 * @param p_ptt -       ptt window used for writing the registers.
 * @param dest_port - geneve destination udp port.
 */
void qed_set_geneve_dest_port(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, u16 dest_port);

/**
 * @brief qed_set_geneve_enable - Enable or disable GRE tunnel in HW
 *
 * @param p_hwfn -         HW device data
 * @param p_ptt -         ptt window used for writing the registers.
 * @param eth_geneve_enable -   eth GENEVE enable enable flag.
 * @param ip_geneve_enable -    IP GENEVE enable enable flag.
 */
void qed_set_geneve_enable(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt,
			   bool eth_geneve_enable, bool ip_geneve_enable);

/**
 * @brief qed_set_vxlan_no_l2_enable - enable or disable VXLAN no L2 parsing
 *
 * @param p_ptt             - ptt window used for writing the registers.
 * @param enable            - VXLAN no L2 enable flag.
 */
void qed_set_vxlan_no_l2_enable(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt, bool enable);

#ifndef UNUSED_HSI_FUNC

/**
 * @brief qed_set_gft_event_id_cm_hdr - Configure GFT event id and cm header
 *
 * @param p_hwfn - HW device data
 * @param p_ptt - ptt window used for writing the registers.
 */
void qed_set_gft_event_id_cm_hdr(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt);

/**
 * @brief qed_gft_disable - Disable GFT
 *
 * @param p_hwfn -   HW device data
 * @param p_ptt -   ptt window used for writing the registers.
 * @param pf_id - pf on which to disable GFT.
 */
void qed_gft_disable(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u16 pf_id);

/**
 * @brief qed_gft_config - Enable and configure HW for GFT
 *
 * @param p_hwfn -   HW device data
 * @param p_ptt -   ptt window used for writing the registers.
 * @param pf_id - pf on which to enable GFT.
 * @param tcp -   set profile tcp packets.
 * @param udp -   set profile udp  packet.
 * @param ipv4 -  set profile ipv4 packet.
 * @param ipv6 -  set profile ipv6 packet.
 * @param profile_type -  define packet same fields. Use enum gft_profile_type.
 */
void qed_gft_config(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    u16 pf_id,
		    bool tcp,
		    bool udp,
		    bool ipv4, bool ipv6, enum gft_profile_type profile_type);

struct gft_ram_line;
struct gft_cam_line_mapped;

/**
 * @brief qed_gft_set_profile - Enable and configure HW for GFT.
 *
 * @param p_hwfn -   HW device data
 * @param p_ptt -   ptt window used for writing the registers.
 * @param pf_id - pf on which to enable GFT.
 * @param profile_cam_line -  profile cam line.
 * @param profile_ram_line -  profile ram line.
 */

void qed_gft_set_profile(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 u16 pf_id,
			 struct gft_cam_line_mapped cam_line,
			 struct gft_ram_line ram_line);

#endif /* UNUSED_HSI_FUNC */

/**
 * @brief qed_config_vf_zone_size_mode - Configure VF zone size mode. Must be
 * used before first ETH queue started.
 *
 * @param p_hwfn -      HW device data
 * @param p_ptt -      ptt window used for writing the registers. Don't care
 *           if runtime_init used.
 * @param mode -     VF zone size mode. Use enum vf_zone_size_mode.
 * @param runtime_init - Set 1 to init runtime registers in engine phase.
 *           Set 0 if VF zone size mode configured after engine
 *           phase.
 */
void qed_config_vf_zone_size_mode(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt,
				  u16 mode, bool runtime_init);

/**
 * @brief qed_get_mstorm_queue_stat_offset - Get mstorm statistics offset by
 * VF zone size mode.
 *
 * @param p_hwfn -         HW device data
 * @param stat_cnt_id -     statistic counter id
 * @param vf_zone_size_mode -   VF zone size mode. Use enum vf_zone_size_mode.
 */
u32 qed_get_mstorm_queue_stat_offset(struct qed_hwfn *p_hwfn,
				     u16 stat_cnt_id, u16 vf_zone_size_mode);

/**
 * @brief qed_get_mstorm_eth_vf_prods_offset - VF producer offset by VF zone
 * size mode.
 *
 * @param p_hwfn -           HW device data
 * @param vf_id -         vf id.
 * @param vf_queue_id -       per VF rx queue id.
 * @param vf_zone_size_mode - vf zone size mode. Use enum vf_zone_size_mode.
 */
u32 qed_get_mstorm_eth_vf_prods_offset(struct qed_hwfn *p_hwfn,
				       u8 vf_id,
				       u8 vf_queue_id, u16 vf_zone_size_mode);

/**
 * @brief qed_enable_context_validation - Enable and configure context
 * validation.
 *
 * @param p_hwfn -   HW device data
 */
void qed_enable_context_validation(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt);

/**
 * @brief qed_calc_session_ctx_validation - Calcualte validation byte for
 * session context.
 *
 * @param p_ctx_mem -	pointer to context memory.
 * @param ctx_size -	context size.
 * @param ctx_type -	context type.
 * @param cid -		context cid.
 */
void qed_calc_session_ctx_validation(void *p_ctx_mem,
				     u16 ctx_size, u8 ctx_type, u32 cid);

/**
 * @brief qed_calc_task_ctx_validation - Calcualte validation byte for task
 * context.
 *
 * @param p_ctx_mem -	pointer to context memory.
 * @param ctx_size -	context size.
 * @param ctx_type -	context type.
 * @param tid -		context tid.
 */
void qed_calc_task_ctx_validation(void *p_ctx_mem,
				  u16 ctx_size, u8 ctx_type, u32 tid);

/**
 * @brief qed_memset_session_ctx - Memset session context to 0 while
 * preserving validation bytes.
 *
 * @param p_ctx_mem -	pointer to context memory.
 * @param ctx_size -	size to initialzie.
 * @param ctx_type -	context type.
 */
void qed_memset_session_ctx(void *p_ctx_mem, u32 ctx_size, u8 ctx_type);

/**
 * @brief qed_memset_task_ctx - Memset task context to 0 while preserving
 * validation bytes.
 *
 * @param p_ctx_mem -	pointer to context memory.
 * @param ctx_size -	size to initialzie.
 * @param ctx_type -	context type.
 */
void qed_memset_task_ctx(void *p_ctx_mem, u32 ctx_size, u8 ctx_type);

/**
 * @brief qed_push_eth_rss_ind_table_update - Proccess RSS indirection table
 * entry update. Use if RSS indirection table update delayed.
 * The function must run in exclusive mode to prevent race on HW interface.
 *
 * @param p_hwfn    - HW device data
 * @param p_ptt  - ptt window used for writing the registers.
 */
void qed_push_eth_rss_ind_table_update(struct qed_hwfn *p_hwfn,
				       struct qed_ptt *p_ptt);

/**
 * @brief qed_get_protocol_type_str - Get a string for Protocol type
 *
 * @param protocol_type     - Protocol type (using enum protocol_type)
 *
 * @return String, representing the Protocol type
 */
const char *qed_get_protocol_type_str(u32 protocol_type);

/**
 * @brief qed_get_ramrod_cmd_id_str - Get a string for Ramrod command ID
 *
 * @param protocol_type     - Protocol type (using enum protocol_type)
 * @param ramrod_cmd_id     - Ramrod command ID (using per-protocol enum <protocol>_ramrod_cmd_id)
 *
 * @return String, representing the Ramrod command ID
 */
const char *qed_get_ramrod_cmd_id_str(u32 protocol_type, u32 ramrod_cmd_id);

/**
 * @brief qed_get_event_ring_entry_opcode_str - Get a string for EQE opcode
 *
 * @param eqe  - EQE pointer
 *
 * @return String, representing the EQE opcode
 */
struct event_ring_entry;

const char *qed_get_event_ring_entry_opcode_str(const struct event_ring_entry
						*eqe);

/**
 * @brief qed_set_rdma_error_level - Sets the RDMA assert level.
 *                                     If the severity of the error will be
 *									   above the level, the FW will assert.
 * @param p_hwfn -		   HW device data
 * @param p_ptt -		   ptt window used for writing the registers
 * @param assert_level - An array of assert levels for each storm.
 */
void qed_set_rdma_error_level(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt,
			      u8 assert_level[NUM_STORMS]);

/**
 * @brief qed_fw_overlay_mem_alloc - Allocates and fills the FW overlay memory.
 *
 * @param p_hwfn -		      HW device data
 * @param fw_overlay_in_buf -  the input FW overlay buffer.
 * @param buf_size -	      the size of the input FW overlay buffer in bytes.
 *			      must be aligned to dwords.
 * @param fw_overlay_out_mem - OUT: a pointer to the allocated overlays memory.
 *
 * @return a pointer to the allocated overlays memory, or NULL in case of failures.
 */
struct phys_mem_desc *qed_fw_overlay_mem_alloc(struct qed_hwfn *p_hwfn,
					       const u32 * const
					       fw_overlay_in_buf,
					       u32 buf_size_in_bytes);

/**
 * @brief qed_fw_overlay_init_ram - Initializes the FW overlay RAM.
 *
 * @param p_hwfn -			HW device data.
 * @param p_ptt -			ptt window used for writing the registers.
 * @param fw_overlay_mem -	the allocated FW overlay memory.
 */
void qed_fw_overlay_init_ram(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt,
			     struct phys_mem_desc *fw_overlay_mem);

/**
 * @brief qed_fw_overlay_mem_free - Frees the FW overlay memory.
 *
 * @param p_hwfn -				HW device data.
 * @param fw_overlay_mem -	pointer to the allocated FW overlay memory to free.
 */
void qed_fw_overlay_mem_free(struct qed_hwfn *p_hwfn,
			     struct phys_mem_desc **fw_overlay_mem);

#define PCICFG_OFFSET					0x2000
#define GRC_CONFIG_REG_PF_INIT_VF			0x624

/* First VF_NUM for PF is encoded in this register.
 * The number of VFs assigned to a PF is assumed to be a multiple of 8.
 * Software should program these bits based on Total Number of VFs programmed
 * for each PF.
 * Since registers from 0x000-0x7ff are spilt across functions, each PF will
 * have the same location for the same 4 bits
 */
#define GRC_CR_PF_INIT_VF_PF_FIRST_VF_NUM_MASK		0xff
/****************************************************************************
* Name:        aeu_inputs.h
*
* Description: This file contains the AEU inputs bits definitions which
*              should be used to configure the MISC_REG_AEU_ENABLE
*              registers.
*              The file was based upon the AEU specification.
*
* Created:     5/17/2015 yrosner
*
****************************************************************************/
#ifndef AEU_INPUTS_H
#define AEU_INPUTS_H

/* AEU INPUT REGISTER 1 */
#define AEU_INPUT1_BITS_GPIO0					(1 << 0)
#define AEU_INPUT1_BITS_GPIO1					(1 << 1)
#define AEU_INPUT1_BITS_GPIO2					(1 << 2)
#define AEU_INPUT1_BITS_GPIO3					(1 << 3)
#define AEU_INPUT1_BITS_GPIO4					(1 << 4)
#define AEU_INPUT1_BITS_GPIO5					(1 << 5)
#define AEU_INPUT1_BITS_GPIO6					(1 << 6)
#define AEU_INPUT1_BITS_GPIO7					(1 << 7)
#define AEU_INPUT1_BITS_GPIO8					(1 << 8)
#define AEU_INPUT1_BITS_GPIO9					(1 << 9)
#define AEU_INPUT1_BITS_GPIO10					(1 << 10)
#define AEU_INPUT1_BITS_GPIO11					(1 << 11)
#define AEU_INPUT1_BITS_GPIO12					(1 << 12)
#define AEU_INPUT1_BITS_GPIO13					(1 << 13)
#define AEU_INPUT1_BITS_GPIO14					(1 << 14)
#define AEU_INPUT1_BITS_GPIO15					(1 << 15)
#define AEU_INPUT1_BITS_GPIO16					(1 << 16)
#define AEU_INPUT1_BITS_GPIO17					(1 << 17)
#define AEU_INPUT1_BITS_GPIO18					(1 << 18)
#define AEU_INPUT1_BITS_GPIO19					(1 << 19)
#define AEU_INPUT1_BITS_GPIO20					(1 << 20)
#define AEU_INPUT1_BITS_GPIO21					(1 << 21)
#define AEU_INPUT1_BITS_GPIO22					(1 << 22)
#define AEU_INPUT1_BITS_GPIO23					(1 << 23)
#define AEU_INPUT1_BITS_GPIO24					(1 << 24)
#define AEU_INPUT1_BITS_GPIO25					(1 << 25)
#define AEU_INPUT1_BITS_GPIO26					(1 << 26)
#define AEU_INPUT1_BITS_GPIO27					(1 << 27)
#define AEU_INPUT1_BITS_GPIO28					(1 << 28)
#define AEU_INPUT1_BITS_GPIO29					(1 << 29)
#define AEU_INPUT1_BITS_GPIO30					(1 << 30)
#define AEU_INPUT1_BITS_GPIO31					(1 << 31)

#define AEU_INPUT1_BITS_PARITY_ERROR				(0)
#define AEU_INPUT1_BITS_PARITY_COMMON_BLOCKS			(0)

/* AEU INPUT REGISTER 2 */
#define AEU_INPUT2_BITS_PGLUE_CONFIG_SPACE			(1 << 0)
#define AEU_INPUT2_BITS_PGLUE_MISC_FLR				(1 << 1)
#define AEU_INPUT2_BITS_PGLUE_B_RBC_PARITY_ERROR		(1 << 2)
#define AEU_INPUT2_BITS_PGLUE_B_RBC_HW_INTERRUPT		(1 << 3)
#define AEU_INPUT2_BITS_PGLUE_MISC_MCTP_ATTN			(1 << 4)
#define AEU_INPUT2_BITS_FLASH_EVENT				(1 << 5)
#define AEU_INPUT2_BITS_SMB_EVENT				(1 << 6)
#define AEU_INPUT2_BITS_MAIN_POWER_INTERRUPT			(1 << 7)
#define AEU_INPUT2_BITS_SW_TIMERS_1				(1 << 8)
#define AEU_INPUT2_BITS_SW_TIMERS_2				(1 << 9)
#define AEU_INPUT2_BITS_SW_TIMERS_3				(1 << 10)
#define AEU_INPUT2_BITS_SW_TIMERS_4				(1 << 11)
#define AEU_INPUT2_BITS_SW_TIMERS_5				(1 << 12)
#define AEU_INPUT2_BITS_SW_TIMERS_6				(1 << 13)
#define AEU_INPUT2_BITS_SW_TIMERS_7				(1 << 14)
#define AEU_INPUT2_BITS_SW_TIMERS_8				(1 << 15)
#define AEU_INPUT2_BITS_PCIE_GLUE_OR_PXP_VPD_EVENT_0		(1 << 16)
#define AEU_INPUT2_BITS_PCIE_GLUE_OR_PXP_VPD_EVENT_1		(1 << 17)
#define AEU_INPUT2_BITS_PCIE_GLUE_OR_PXP_VPD_EVENT_2		(1 << 18)
#define AEU_INPUT2_BITS_PCIE_GLUE_OR_PXP_VPD_EVENT_3		(1 << 19)
#define AEU_INPUT2_BITS_PCIE_GLUE_OR_PXP_VPD_EVENT_4		(1 << 20)
#define AEU_INPUT2_BITS_PCIE_GLUE_OR_PXP_VPD_EVENT_5		(1 << 21)
#define AEU_INPUT2_BITS_PCIE_GLUE_OR_PXP_VPD_EVENT_6		(1 << 22)
#define AEU_INPUT2_BITS_PCIE_GLUE_OR_PXP_VPD_EVENT_7		(1 << 23)
#define AEU_INPUT2_BITS_PCIE_GLUE_OR_PXP_VPD_EVENT_8		(1 << 24)
#define AEU_INPUT2_BITS_PCIE_GLUE_OR_PXP_VPD_EVENT_9		(1 << 25)
#define AEU_INPUT2_BITS_PCIE_GLUE_OR_PXP_VPD_EVENT_10		(1 << 26)
#define AEU_INPUT2_BITS_PCIE_GLUE_OR_PXP_VPD_EVENT_11		(1 << 27)
#define AEU_INPUT2_BITS_PCIE_GLUE_OR_PXP_VPD_EVENT_12		(1 << 28)
#define AEU_INPUT2_BITS_PCIE_GLUE_OR_PXP_VPD_EVENT_13		(1 << 29)
#define AEU_INPUT2_BITS_PCIE_GLUE_OR_PXP_VPD_EVENT_14		(1 << 30)
#define AEU_INPUT2_BITS_PCIE_GLUE_OR_PXP_VPD_EVENT_15		(1 << 31)

#define AEU_INPUT2_BITS_GENERATE_SYSTEM_KILL \
        (AEU_INPUT2_BITS_PGLUE_B_RBC_PARITY_ERROR)

#define AEU_INPUT2_BITS_PARITY_ERROR \
        (AEU_INPUT2_BITS_PGLUE_B_RBC_PARITY_ERROR)

#define AEU_INPUT2_BITS_PARITY_COMMON_BLOCKS \
        (AEU_INPUT2_BITS_PGLUE_B_RBC_PARITY_ERROR)

/* AEU INPUT REGISTER 3 */
#define AEU_INPUT3_BITS_GENERAL_ATTN0			(1 << 0)
#define AEU_INPUT3_BITS_GENERAL_ATTN1			(1 << 1)
#define AEU_INPUT3_BITS_GENERAL_ATTN2			(1 << 2)
#define AEU_INPUT3_BITS_GENERAL_ATTN3			(1 << 3)
#define AEU_INPUT3_BITS_GENERAL_ATTN4			(1 << 4)
#define AEU_INPUT3_BITS_GENERAL_ATTN5			(1 << 5)
#define AEU_INPUT3_BITS_GENERAL_ATTN6			(1 << 6)
#define AEU_INPUT3_BITS_GENERAL_ATTN7			(1 << 7)
#define AEU_INPUT3_BITS_GENERAL_ATTN8			(1 << 8)
#define AEU_INPUT3_BITS_GENERAL_ATTN9			(1 << 9)
#define AEU_INPUT3_BITS_GENERAL_ATTN10			(1 << 10)
#define AEU_INPUT3_BITS_GENERAL_ATTN11			(1 << 11)
#define AEU_INPUT3_BITS_GENERAL_ATTN12			(1 << 12)
#define AEU_INPUT3_BITS_GENERAL_ATTN13			(1 << 13)
#define AEU_INPUT3_BITS_GENERAL_ATTN14			(1 << 14)
#define AEU_INPUT3_BITS_GENERAL_ATTN15			(1 << 15)
#define AEU_INPUT3_BITS_GENERAL_ATTN16			(1 << 16)
#define AEU_INPUT3_BITS_GENERAL_ATTN17			(1 << 17)
#define AEU_INPUT3_BITS_GENERAL_ATTN18			(1 << 18)
#define AEU_INPUT3_BITS_GENERAL_ATTN19			(1 << 19)
#define AEU_INPUT3_BITS_GENERAL_ATTN20			(1 << 20)
#define AEU_INPUT3_BITS_GENERAL_ATTN21			(1 << 21)
#define AEU_INPUT3_BITS_GENERAL_ATTN22			(1 << 22)
#define AEU_INPUT3_BITS_GENERAL_ATTN23			(1 << 23)
#define AEU_INPUT3_BITS_GENERAL_ATTN24			(1 << 24)
#define AEU_INPUT3_BITS_GENERAL_ATTN25			(1 << 25)
#define AEU_INPUT3_BITS_GENERAL_ATTN26			(1 << 26)
#define AEU_INPUT3_BITS_GENERAL_ATTN27			(1 << 27)
#define AEU_INPUT3_BITS_GENERAL_ATTN28			(1 << 28)
#define AEU_INPUT3_BITS_GENERAL_ATTN29			(1 << 29)
#define AEU_INPUT3_BITS_GENERAL_ATTN30			(1 << 30)
#define AEU_INPUT3_BITS_GENERAL_ATTN31			(1 << 31)

#define AEU_INPUT3_BITS_PARITY_ERROR			(0)
#define AEU_INPUT3_BITS_PARITY_COMMON_BLOCKS		(0)

/* AEU INPUT REGISTER 4 */
#define AEU_INPUT4_BITS_GENERAL_ATTN32			(1 << 0)
#define AEU_INPUT4_BITS_GENERAL_ATTN33			(1 << 1)
#define AEU_INPUT4_BITS_GENERAL_ATTN34			(1 << 2)
#define AEU_INPUT4_BITS_GENERAL_ATTN35			(1 << 3)	/* Driver initiate recovery flow */
#define AEU_INPUT4_BITS_CNIG_ATTN_PORT_0_BB		(1 << 4)
#define AEU_INPUT4_BITS_NWS_PARITY_ERROR_K2		(1 << 4)
#define AEU_INPUT4_BITS_CNIG_ATTN_PORT_1_BB		(1 << 5)
#define AEU_INPUT4_BITS_NWS_HW_INTERRUPT_K2		(1 << 5)
#define AEU_INPUT4_BITS_CNIG_ATTN_PORT_2_BB		(1 << 6)
#define AEU_INPUT4_BITS_NWM_PARITY_ERROR_K2		(1 << 6)
#define AEU_INPUT4_BITS_CNIG_ATTN_PORT_3_BB		(1 << 7)
#define AEU_INPUT4_BITS_NWM_HW_INTERRUPT_K2		(1 << 7)
#define AEU_INPUT4_BITS_MCP_CPU_EVENT			(1 << 8)
#define AEU_INPUT4_BITS_MCP_WATCHDOG_TIMER		(1 << 9)
#define AEU_INPUT4_BITS_MCP_M2P_ATTN			(1 << 10)
#define AEU_INPUT4_BITS_AVS_STOP_STATUS_READY		(1 << 11)
#define AEU_INPUT4_BITS_MSTAT_PARITY_ERROR		(1 << 12)
#define AEU_INPUT4_BITS_MSTAT_HW_INTERRUPT		(1 << 13)
#define AEU_INPUT4_BITS_MSTAT_PER_PATH_PARITY_ERROR	(1 << 14)
#define AEU_INPUT4_BITS_MSTAT_PER_PATH_HW_INTERRUPT	(1 << 15)
#define AEU_INPUT4_BITS_OPTE_PARITY_ERROR		(1 << 16)
#define AEU_INPUT4_BITS_MCP_PARITY_ERROR		(1 << 17)
#define AEU_INPUT4_BITS_RSRV18_BB			(1 << 18)
#define AEU_INPUT4_BITS_MS_HW_INTERRUPT_K2		(1 << 18)
#define AEU_INPUT4_BITS_RSRV19_BB			(1 << 19)
#define AEU_INPUT4_BITS_UMAC_HW_INTERRUPT_K2		(1 << 19)
#define AEU_INPUT4_BITS_RSRV20_BB			(1 << 20)
#define AEU_INPUT4_BITS_LED_HW_INTERRUPT_K2		(1 << 20)
#define AEU_INPUT4_BITS_BMBN_HW_INTERRUPT		(1 << 21)
#define AEU_INPUT4_BITS_NIG_PARITY_ERROR		(1 << 22)
#define AEU_INPUT4_BITS_NIG_HW_INTERRUPT		(1 << 23)
#define AEU_INPUT4_BITS_BMB_PARITY_ERROR		(1 << 24)
#define AEU_INPUT4_BITS_BMB_HW_INTERRUPT		(1 << 25)
#define AEU_INPUT4_BITS_BTB_PARITY_ERROR		(1 << 26)
#define AEU_INPUT4_BITS_BTB_HW_INTERRUPT		(1 << 27)
#define AEU_INPUT4_BITS_BRB_PARITY_ERROR		(1 << 28)
#define AEU_INPUT4_BITS_BRB_HW_INTERRUPT		(1 << 29)
#define AEU_INPUT4_BITS_PRS_PARITY_ERROR		(1 << 30)
#define AEU_INPUT4_BITS_PRS_HW_INTERRUPT		(1 << 31)

#define AEU_INPUT4_BITS_GENERATE_SYSTEM_KILL_BB        \
        (AEU_INPUT4_BITS_OPTE_PARITY_ERROR |           \
         AEU_INPUT4_BITS_MSTAT_PARITY_ERROR |          \
         AEU_INPUT4_BITS_MSTAT_PER_PATH_PARITY_ERROR | \
         AEU_INPUT4_BITS_MCP_PARITY_ERROR |            \
         AEU_INPUT4_BITS_NIG_PARITY_ERROR |            \
         AEU_INPUT4_BITS_BMB_PARITY_ERROR)

#define AEU_INPUT4_BITS_GENERATE_SYSTEM_KILL_K2        \
        (AEU_INPUT4_BITS_NWS_PARITY_ERROR_K2 |         \
         AEU_INPUT4_BITS_NWM_PARITY_ERROR_K2 |         \
         AEU_INPUT4_BITS_OPTE_PARITY_ERROR |           \
         AEU_INPUT4_BITS_MSTAT_PARITY_ERROR |          \
         AEU_INPUT4_BITS_MSTAT_PER_PATH_PARITY_ERROR | \
         AEU_INPUT4_BITS_MCP_PARITY_ERROR |            \
         AEU_INPUT4_BITS_NIG_PARITY_ERROR |            \
         AEU_INPUT4_BITS_BMB_PARITY_ERROR)

#define AEU_INPUT4_BITS_GENERATE_PROCESS_KILL_BB \
        (AEU_INPUT4_BITS_GENERAL_ATTN35 |        \
         AEU_INPUT4_BITS_BTB_PARITY_ERROR |      \
         AEU_INPUT4_BITS_BRB_PARITY_ERROR |      \
         AEU_INPUT4_BITS_PRS_PARITY_ERROR)

#define AEU_INPUT4_BITS_GENERATE_PROCESS_KILL_K2 \
        (AEU_INPUT4_BITS_NWS_PARITY_ERROR_K2 |   \
         AEU_INPUT4_BITS_NWM_PARITY_ERROR_K2 |   \
         AEU_INPUT4_BITS_GENERAL_ATTN35 |        \
         AEU_INPUT4_BITS_BTB_PARITY_ERROR |      \
         AEU_INPUT4_BITS_BRB_PARITY_ERROR |      \
         AEU_INPUT4_BITS_PRS_PARITY_ERROR)

/* General ATTN35 is for the driver to trigger recovery flow */
#define AEU_INPUT4_BITS_PARITY_ERROR_BB            \
        (AEU_INPUT4_BITS_GENERATE_SYSTEM_KILL_BB | \
         AEU_INPUT4_BITS_GENERATE_PROCESS_KILL_BB)

#define AEU_INPUT4_BITS_PARITY_ERROR_K2            \
        (AEU_INPUT4_BITS_GENERATE_SYSTEM_KILL_K2 | \
         AEU_INPUT4_BITS_GENERATE_PROCESS_KILL_K2)

#define AEU_INPUT4_BITS_PARITY_COMMON_BLOCKS           \
        (AEU_INPUT4_BITS_OPTE_PARITY_ERROR |           \
         AEU_INPUT4_BITS_MSTAT_PARITY_ERROR |          \
         AEU_INPUT4_BITS_MSTAT_PER_PATH_PARITY_ERROR | \
         AEU_INPUT4_BITS_MCP_PARITY_ERROR)

/* AEU INPUT REGISTER 5 */
#define AEU_INPUT5_BITS_SRC_PARITY_ERROR		(1 << 0)
#define AEU_INPUT5_BITS_SRC_HW_INTERRUPT		(1 << 1)
#define AEU_INPUT5_BITS_PB_CLIENT1_PARITY_ERROR		(1 << 2)
#define AEU_INPUT5_BITS_PB_CLIENT1_HW_INTERRUPT		(1 << 3)
#define AEU_INPUT5_BITS_PB_CLIENT2_PARITY_ERROR		(1 << 4)
#define AEU_INPUT5_BITS_PB_CLIENT2_HW_INTERRUPT		(1 << 5)
#define AEU_INPUT5_BITS_RPB_PARITY_ERROR		(1 << 6)
#define AEU_INPUT5_BITS_RPB_HW_INTERRUPT		(1 << 7)
#define AEU_INPUT5_BITS_PBF_PARITY_ERROR		(1 << 8)
#define AEU_INPUT5_BITS_PBF_HW_INTERRUPT		(1 << 9)
#define AEU_INPUT5_BITS_QM_PARITY_ERROR			(1 << 10)
#define AEU_INPUT5_BITS_QM_HW_INTERRUPT			(1 << 11)
#define AEU_INPUT5_BITS_TM_PARITY_ERROR			(1 << 12)
#define AEU_INPUT5_BITS_TM_HW_INTERRUPT			(1 << 13)
#define AEU_INPUT5_BITS_MCM_PARITY_ERROR		(1 << 14)
#define AEU_INPUT5_BITS_MCM_HW_INTERRUPT		(1 << 15)
#define AEU_INPUT5_BITS_MSDM_PARITY_ERROR		(1 << 16)
#define AEU_INPUT5_BITS_MSDM_HW_INTERRUPT		(1 << 17)
#define AEU_INPUT5_BITS_MSEM_PARITY_ERROR		(1 << 18)
#define AEU_INPUT5_BITS_MSEM_HW_INTERRUPT		(1 << 19)
#define AEU_INPUT5_BITS_PCM_PARITY_ERROR		(1 << 20)
#define AEU_INPUT5_BITS_PCM_HW_INTERRUPT		(1 << 21)
#define AEU_INPUT5_BITS_PSDM_PARITY_ERROR		(1 << 22)
#define AEU_INPUT5_BITS_PSDM_HW_INTERRUPT		(1 << 23)
#define AEU_INPUT5_BITS_PSEM_PARITY_ERROR		(1 << 24)
#define AEU_INPUT5_BITS_PSEM_HW_INTERRUPT		(1 << 25)
#define AEU_INPUT5_BITS_TCM_PARITY_ERROR		(1 << 26)
#define AEU_INPUT5_BITS_TCM_HW_INTERRUPT		(1 << 27)
#define AEU_INPUT5_BITS_TSDM_PARITY_ERROR		(1 << 28)
#define AEU_INPUT5_BITS_TSDM_HW_INTERRUPT		(1 << 29)
#define AEU_INPUT5_BITS_TSEM_PARITY_ERROR		(1 << 30)
#define AEU_INPUT5_BITS_TSEM_HW_INTERRUPT		(1 << 31)

#define AEU_INPUT5_BITS_GENERATE_SYSTEM_KILL		(0x0)

#define AEU_INPUT5_BITS_GENERATE_PROCESS_KILL      \
        (AEU_INPUT5_BITS_SRC_PARITY_ERROR |        \
         AEU_INPUT5_BITS_PB_CLIENT1_PARITY_ERROR | \
         AEU_INPUT5_BITS_PB_CLIENT2_PARITY_ERROR | \
         AEU_INPUT5_BITS_RPB_PARITY_ERROR |        \
         AEU_INPUT5_BITS_PBF_PARITY_ERROR |        \
         AEU_INPUT5_BITS_QM_PARITY_ERROR |         \
         AEU_INPUT5_BITS_TM_PARITY_ERROR |         \
         AEU_INPUT5_BITS_MCM_PARITY_ERROR |        \
         AEU_INPUT5_BITS_MSDM_PARITY_ERROR |       \
         AEU_INPUT5_BITS_MSEM_PARITY_ERROR |       \
         AEU_INPUT5_BITS_PCM_PARITY_ERROR |        \
         AEU_INPUT5_BITS_PSDM_PARITY_ERROR |       \
         AEU_INPUT5_BITS_PSEM_PARITY_ERROR |       \
         AEU_INPUT5_BITS_TCM_PARITY_ERROR |        \
         AEU_INPUT5_BITS_TSDM_PARITY_ERROR |       \
         AEU_INPUT5_BITS_TSEM_PARITY_ERROR)

#define AEU_INPUT5_BITS_PARITY_ERROR            \
        (AEU_INPUT5_BITS_GENERATE_SYSTEM_KILL | \
         AEU_INPUT5_BITS_GENERATE_PROCESS_KILL)

#define AEU_INPUT5_BITS_PARITY_COMMON_BLOCKS	(0)

/* AEU INPUT REGISTER 6 */
#define AEU_INPUT6_BITS_UCM_PARITY_ERROR	(1 << 0)
#define AEU_INPUT6_BITS_UCM_HW_INTERRUPT	(1 << 1)
#define AEU_INPUT6_BITS_USDM_PARITY_ERROR	(1 << 2)
#define AEU_INPUT6_BITS_USDM_HW_INTERRUPT	(1 << 3)
#define AEU_INPUT6_BITS_USEM_PARITY_ERROR	(1 << 4)
#define AEU_INPUT6_BITS_USEM_HW_INTERRUPT	(1 << 5)
#define AEU_INPUT6_BITS_XCM_PARITY_ERROR	(1 << 6)
#define AEU_INPUT6_BITS_XCM_HW_INTERRUPT	(1 << 7)
#define AEU_INPUT6_BITS_XSDM_PARITY_ERROR	(1 << 8)
#define AEU_INPUT6_BITS_XSDM_HW_INTERRUPT	(1 << 9)
#define AEU_INPUT6_BITS_XSEM_PARITY_ERROR	(1 << 10)
#define AEU_INPUT6_BITS_XSEM_HW_INTERRUPT	(1 << 11)
#define AEU_INPUT6_BITS_YCM_PARITY_ERROR	(1 << 12)
#define AEU_INPUT6_BITS_YCM_HW_INTERRUPT	(1 << 13)
#define AEU_INPUT6_BITS_YSDM_PARITY_ERROR	(1 << 14)
#define AEU_INPUT6_BITS_YSDM_HW_INTERRUPT	(1 << 15)
#define AEU_INPUT6_BITS_YSEM_PARITY_ERROR	(1 << 16)
#define AEU_INPUT6_BITS_YSEM_HW_INTERRUPT	(1 << 17)
#define AEU_INPUT6_BITS_XYLD_PARITY_ERROR	(1 << 18)
#define AEU_INPUT6_BITS_XYLD_HW_INTERRUPT	(1 << 19)
#define AEU_INPUT6_BITS_TMLD_PARITY_ERROR	(1 << 20)
#define AEU_INPUT6_BITS_TMLD_HW_INTERRUPT	(1 << 21)
#define AEU_INPUT6_BITS_MULD_PARITY_ERROR	(1 << 22)
#define AEU_INPUT6_BITS_MULD_HW_INTERRUPT	(1 << 23)
#define AEU_INPUT6_BITS_YULD_PARITY_ERROR	(1 << 24)
#define AEU_INPUT6_BITS_YULD_HW_INTERRUPT	(1 << 25)
#define AEU_INPUT6_BITS_DORQ_PARITY_ERROR	(1 << 26)
#define AEU_INPUT6_BITS_DORQ_HW_INTERRUPT	(1 << 27)
#define AEU_INPUT6_BITS_DBG_PARITY_ERROR	(1 << 28)
#define AEU_INPUT6_BITS_DBG_HW_INTERRUPT	(1 << 29)
#define AEU_INPUT6_BITS_IPC_PARITY_ERROR	(1 << 30)
#define AEU_INPUT6_BITS_IPC_HW_INTERRUPT	(1 << 31)

#define AEU_INPUT6_BITS_GENERATE_SYSTEM_KILL \
        (AEU_INPUT6_BITS_IPC_PARITY_ERROR)

#define AEU_INPUT6_BITS_GENERATE_PROCESS_KILL \
        (AEU_INPUT6_BITS_UCM_PARITY_ERROR |   \
         AEU_INPUT6_BITS_USDM_PARITY_ERROR |  \
         AEU_INPUT6_BITS_USEM_PARITY_ERROR |  \
         AEU_INPUT6_BITS_XCM_PARITY_ERROR |   \
         AEU_INPUT6_BITS_XSDM_PARITY_ERROR |  \
         AEU_INPUT6_BITS_XSEM_PARITY_ERROR |  \
         AEU_INPUT6_BITS_YCM_PARITY_ERROR |   \
         AEU_INPUT6_BITS_YSDM_PARITY_ERROR |  \
         AEU_INPUT6_BITS_YSEM_PARITY_ERROR |  \
         AEU_INPUT6_BITS_XYLD_PARITY_ERROR |  \
         AEU_INPUT6_BITS_TMLD_PARITY_ERROR |  \
         AEU_INPUT6_BITS_MULD_PARITY_ERROR |  \
         AEU_INPUT6_BITS_YULD_PARITY_ERROR |  \
         AEU_INPUT6_BITS_DORQ_PARITY_ERROR)

#define AEU_INPUT6_BITS_PARITY_ERROR            \
        (AEU_INPUT6_BITS_GENERATE_SYSTEM_KILL | \
         AEU_INPUT6_BITS_GENERATE_PROCESS_KILL)

#define AEU_INPUT6_BITS_PARITY_COMMON_BLOCKS \
        (AEU_INPUT6_BITS_IPC_PARITY_ERROR)

/* AEU INPUT REGISTER 7 */
#define AEU_INPUT7_BITS_CCFC_PARITY_ERROR			(1 << 0)
#define AEU_INPUT7_BITS_CCFC_HW_INTERRUPT			(1 << 1)
#define AEU_INPUT7_BITS_CDU_PARITY_ERROR			(1 << 2)
#define AEU_INPUT7_BITS_CDU_HW_INTERRUPT			(1 << 3)
#define AEU_INPUT7_BITS_DMAE_PARITY_ERROR			(1 << 4)
#define AEU_INPUT7_BITS_DMAE_HW_INTERRUPT			(1 << 5)
#define AEU_INPUT7_BITS_IGU_PARITY_ERROR			(1 << 6)
#define AEU_INPUT7_BITS_IGU_HW_INTERRUPT			(1 << 7)
#define AEU_INPUT7_BITS_ATC_PARITY_ERROR			(1 << 8)
#define AEU_INPUT7_BITS_ATC_HW_INTERRUPT			(1 << 9)
#define AEU_INPUT7_BITS_CAU_PARITY_ERROR			(1 << 10)
#define AEU_INPUT7_BITS_CAU_HW_INTERRUPT			(1 << 11)
#define AEU_INPUT7_BITS_PTU_PARITY_ERROR			(1 << 12)
#define AEU_INPUT7_BITS_PTU_HW_INTERRUPT			(1 << 13)
#define AEU_INPUT7_BITS_PRM_PARITY_ERROR			(1 << 14)
#define AEU_INPUT7_BITS_PRM_HW_INTERRUPT			(1 << 15)
#define AEU_INPUT7_BITS_TCFC_PARITY_ERROR			(1 << 16)
#define AEU_INPUT7_BITS_TCFC_HW_INTERRUPT			(1 << 17)
#define AEU_INPUT7_BITS_RDIF_PARITY_ERROR			(1 << 18)
#define AEU_INPUT7_BITS_RDIF_HW_INTERRUPT			(1 << 19)
#define AEU_INPUT7_BITS_TDIF_PARITY_ERROR			(1 << 20)
#define AEU_INPUT7_BITS_TDIF_HW_INTERRUPT			(1 << 21)
#define AEU_INPUT7_BITS_RSS_PARITY_ERROR			(1 << 22)
#define AEU_INPUT7_BITS_RSS_HW_INTERRUPT			(1 << 23)
#define AEU_INPUT7_BITS_MISC_PARITY_ERROR			(1 << 24)
#define AEU_INPUT7_BITS_MISC_HW_INTERRUPT			(1 << 25)
#define AEU_INPUT7_BITS_MISCS_PARITY_ERROR			(1 << 26)
#define AEU_INPUT7_BITS_MISCS_HW_INTERRUPT			(1 << 27)
#define AEU_INPUT7_BITS_VAUX_PCI_CORE_OR_PGLUE_PARITY_ERROR	(1 << 28)
#define AEU_INPUT7_BITS_VAUX_PCI_CORE_HW_INTERRUPT		(1 << 29)
#define AEU_INPUT7_BITS_PSWRQ_PARITY_ERROR			(1 << 30)
#define AEU_INPUT7_BITS_PSWRQ_HW_INTERRUPT			(1 << 31)

#define AEU_INPUT7_BITS_GENERATE_SYSTEM_KILL                   \
        (AEU_INPUT7_BITS_IGU_PARITY_ERROR |                    \
         AEU_INPUT7_BITS_ATC_PARITY_ERROR |                    \
         AEU_INPUT7_BITS_CAU_PARITY_ERROR |                    \
         AEU_INPUT7_BITS_MISC_PARITY_ERROR |                   \
         AEU_INPUT7_BITS_MISCS_PARITY_ERROR |                  \
         AEU_INPUT7_BITS_VAUX_PCI_CORE_OR_PGLUE_PARITY_ERROR | \
         AEU_INPUT7_BITS_PSWRQ_PARITY_ERROR)

#define AEU_INPUT7_BITS_GENERATE_PROCESS_KILL \
        (AEU_INPUT7_BITS_CCFC_PARITY_ERROR |  \
         AEU_INPUT7_BITS_CDU_PARITY_ERROR |   \
         AEU_INPUT7_BITS_DMAE_PARITY_ERROR |  \
         AEU_INPUT7_BITS_PTU_PARITY_ERROR |   \
         AEU_INPUT7_BITS_PRM_PARITY_ERROR |   \
         AEU_INPUT7_BITS_TCFC_PARITY_ERROR |  \
         AEU_INPUT7_BITS_RDIF_PARITY_ERROR |  \
         AEU_INPUT7_BITS_TDIF_PARITY_ERROR |  \
         AEU_INPUT7_BITS_RSS_PARITY_ERROR)

#define AEU_INPUT7_BITS_PARITY_ERROR            \
        (AEU_INPUT7_BITS_GENERATE_SYSTEM_KILL | \
         AEU_INPUT7_BITS_GENERATE_PROCESS_KILL)

#define AEU_INPUT7_BITS_PARITY_COMMON_BLOCKS  \
        (AEU_INPUT7_BITS_MISCS_PARITY_ERROR | \
         AEU_INPUT7_BITS_VAUX_PCI_CORE_OR_PGLUE_PARITY_ERROR)

/* AEU INPUT REGISTER 8 */
#define AEU_INPUT8_BITS_PSWRQ_PCI_CLK_PARITY_ERROR		(1 << 0)
#define AEU_INPUT8_BITS_PSWRQ_PCI_CLK_HW_INTERRUPT		(1 << 1)
#define AEU_INPUT8_BITS_PSWWR_PARITY_ERROR			(1 << 2)
#define AEU_INPUT8_BITS_PSWWR_HW_INTERRUPT			(1 << 3)
#define AEU_INPUT8_BITS_PSWWR_PCI_CLK_PARITY_ERROR		(1 << 4)
#define AEU_INPUT8_BITS_PSWWR_PCI_CLK_HW_INTERRUPT		(1 << 5)
#define AEU_INPUT8_BITS_PSWRD_PARITY_ERROR			(1 << 6)
#define AEU_INPUT8_BITS_PSWRD_HW_INTERRUPT			(1 << 7)
#define AEU_INPUT8_BITS_PSWRD_PCI_CLK_PARITY_ERROR		(1 << 8)
#define AEU_INPUT8_BITS_PSWRD_PCI_CLK_HW_INTERRUPT		(1 << 9)
#define AEU_INPUT8_BITS_PSWHST_PARITY_ERROR			(1 << 10)
#define AEU_INPUT8_BITS_PSWHST_HW_INTERRUPT			(1 << 11)
#define AEU_INPUT8_BITS_PSWHST_PCI_CLK_PARITY_ERROR		(1 << 12)
#define AEU_INPUT8_BITS_PSWHST_PCI_CLK_HW_INTERRUPT		(1 << 13)
#define AEU_INPUT8_BITS_GRC_PARITY_ERROR			(1 << 14)
#define AEU_INPUT8_BITS_GRC_HW_INTERRUPT			(1 << 15)
#define AEU_INPUT8_BITS_CPMU_PARITY_ERROR			(1 << 16)
#define AEU_INPUT8_BITS_CPMU_HW_INTERRUPT			(1 << 17)
#define AEU_INPUT8_BITS_NCSI_PARITY_ERROR			(1 << 18)
#define AEU_INPUT8_BITS_NCSI_HW_INTERRUPT			(1 << 19)
#define AEU_INPUT8_BITS_YSEM_PRAM_PARITY_ERROR			(1 << 20)
#define AEU_INPUT8_BITS_XSEM_PRAM_PARITY_ERROR			(1 << 21)
#define AEU_INPUT8_BITS_USEM_PRAM_PARITY_ERROR			(1 << 22)
#define AEU_INPUT8_BITS_TSEM_PRAM_PARITY_ERROR			(1 << 23)
#define AEU_INPUT8_BITS_PSEM_PRAM_PARITY_ERROR			(1 << 24)
#define AEU_INPUT8_BITS_MSEM_PRAM_PARITY_ERROR			(1 << 25)
#define AEU_INPUT8_BITS_PXP_MISC_MPS_ATTN			(1 << 26)
#define AEU_INPUT8_BITS_PCIE_GLUE_OR_PXP_EXPANSION_ROM_EVENT	(1 << 27)
#define AEU_INPUT8_BITS_PERST_B_ASSERTION			(1 << 28)
#define AEU_INPUT8_BITS_PERST_B_DE_ASSERTION			(1 << 29)
#define AEU_INPUT8_BITS_RSRV30_BB				(1 << 30)
#define AEU_INPUT8_BITS_WOL_PARITY_ERROR_K2			(1 << 30)
#define AEU_INPUT8_BITS_RSRV31_BB				(1 << 31)
#define AEU_INPUT8_BITS_WOL_HW_INTERRUPT_K2			(1 << 31)

#define AEU_INPUT8_BITS_GENERATE_SYSTEM_KILL_K2        \
        (AEU_INPUT8_BITS_PSWRQ_PCI_CLK_PARITY_ERROR |  \
         AEU_INPUT8_BITS_PSWWR_PARITY_ERROR |          \
         AEU_INPUT8_BITS_PSWWR_PCI_CLK_PARITY_ERROR |  \
         AEU_INPUT8_BITS_PSWRD_PARITY_ERROR |          \
         AEU_INPUT8_BITS_PSWRD_PCI_CLK_PARITY_ERROR |  \
         AEU_INPUT8_BITS_PSWHST_PARITY_ERROR |         \
         AEU_INPUT8_BITS_PSWHST_PCI_CLK_PARITY_ERROR | \
         AEU_INPUT8_BITS_CPMU_PARITY_ERROR |           \
         AEU_INPUT8_BITS_NCSI_PARITY_ERROR |           \
         AEU_INPUT8_BITS_WOL_PARITY_ERROR_K2)

#define AEU_INPUT8_BITS_GENERATE_SYSTEM_KILL_BB        \
        (AEU_INPUT8_BITS_PSWRQ_PCI_CLK_PARITY_ERROR |  \
         AEU_INPUT8_BITS_PSWWR_PARITY_ERROR |          \
         AEU_INPUT8_BITS_PSWWR_PCI_CLK_PARITY_ERROR |  \
         AEU_INPUT8_BITS_PSWRD_PARITY_ERROR |          \
         AEU_INPUT8_BITS_PSWRD_PCI_CLK_PARITY_ERROR |  \
         AEU_INPUT8_BITS_PSWHST_PARITY_ERROR |         \
         AEU_INPUT8_BITS_PSWHST_PCI_CLK_PARITY_ERROR | \
         AEU_INPUT8_BITS_CPMU_PARITY_ERROR |           \
         AEU_INPUT8_BITS_NCSI_PARITY_ERROR)

#define AEU_INPUT8_BITS_GENERATE_PROCESS_KILL     \
        (AEU_INPUT8_BITS_GRC_PARITY_ERROR |       \
         AEU_INPUT8_BITS_XSEM_PRAM_PARITY_ERROR | \
         AEU_INPUT8_BITS_USEM_PRAM_PARITY_ERROR | \
         AEU_INPUT8_BITS_TSEM_PRAM_PARITY_ERROR | \
         AEU_INPUT8_BITS_PSEM_PRAM_PARITY_ERROR | \
         AEU_INPUT8_BITS_MSEM_PRAM_PARITY_ERROR)

#define AEU_INPUT8_BITS_PARITY_ERROR_BB            \
        (AEU_INPUT8_BITS_GENERATE_SYSTEM_KILL_BB | \
         AEU_INPUT8_BITS_GENERATE_PROCESS_KILL)

#define AEU_INPUT8_BITS_PARITY_ERROR_K2            \
        (AEU_INPUT8_BITS_GENERATE_SYSTEM_KILL_K2 | \
         AEU_INPUT8_BITS_GENERATE_PROCESS_KILL)

#define AEU_INPUT8_BITS_PARITY_COMMON_BLOCKS_K2 \
        (AEU_INPUT8_BITS_NCSI_PARITY_ERROR |    \
         AEU_INPUT8_BITS_GRC_PARITY_ERROR |     \
         AEU_INPUT8_BITS_WOL_PARITY_ERROR_K2)

#define AEU_INPUT8_BITS_PARITY_COMMON_BLOCKS_BB \
        (AEU_INPUT8_BITS_NCSI_PARITY_ERROR |    \
         AEU_INPUT8_BITS_GRC_PARITY_ERROR)

/* AEU INPUT REGISTER 9 */
#define AEU_INPUT9_BITS_MCP_LATCHED_MEMORY_PARITY	(1 << 0)
#define AEU_INPUT9_BITS_MCP_LATCHED_SCRATCHPAD_CACHE	(1 << 1)
#define AEU_INPUT9_BITS_MCP_LATCHED_UMP_TX_PARITY_BB	(1 << 2)
#define AEU_INPUT9_BITS_AVS_PARITY_K2			(1 << 2)
#define AEU_INPUT9_BITS_MCP_LATCHED_SPAD_PARITY_BB	(1 << 3)
#define AEU_INPUT9_BITS_AVS_HW_INTERRUPT_K2		(1 << 3)
#define AEU_INPUT9_BITS_PCIE_RSRV_4_BB			(1 << 4)
#define AEU_INPUT9_BITS_PCIE_CORE_HW_INTERRUPT_K2	(1 << 4)
#define AEU_INPUT9_BITS_PCIE_LINK_UP_K2			(1 << 5)
#define AEU_INPUT9_BITS_PCIE_HOT_RESET_K2		(1 << 6)
#define AEU_INPUT9_BITS_RSRV_7				(1 << 7)
#define AEU_INPUT9_BITS_RSRV_8				(1 << 8)
#define AEU_INPUT9_BITS_YPLD_HW_INTERRUPT		(1 << 8)
#define AEU_INPUT9_BITS_RSRV_9				(1 << 9)
#define AEU_INPUT9_BITS_PTLD_PARITY_ERROR		(1 << 9)
#define AEU_INPUT9_BITS_RSRV_10				(1 << 10)
#define AEU_INPUT9_BITS_RSRV_11				(1 << 11)
#define AEU_INPUT9_BITS_RSRV_12				(1 << 12)
#define AEU_INPUT9_BITS_RSRV_13				(1 << 13)
#define AEU_INPUT9_BITS_RSRV_14				(1 << 14)
#define AEU_INPUT9_BITS_RSRV_15				(1 << 15)
#define AEU_INPUT9_BITS_RSRV_16				(1 << 16)
#define AEU_INPUT9_BITS_RSRV_17				(1 << 17)
#define AEU_INPUT9_BITS_RSRV_18				(1 << 18)
#define AEU_INPUT9_BITS_RSRV_19				(1 << 19)
#define AEU_INPUT9_BITS_RSRV_20				(1 << 20)
#define AEU_INPUT9_BITS_RSRV_21				(1 << 21)
#define AEU_INPUT9_BITS_RSRV_22				(1 << 22)
#define AEU_INPUT9_BITS_RSRV_23				(1 << 23)
#define AEU_INPUT9_BITS_RSRV_24				(1 << 24)
#define AEU_INPUT9_BITS_RSRV_25				(1 << 25)
#define AEU_INPUT9_BITS_RSRV_26				(1 << 26)
#define AEU_INPUT9_BITS_RSRV_27				(1 << 27)
#define AEU_INPUT9_BITS_RSRV_28				(1 << 28)
#define AEU_INPUT9_BITS_RSRV_29				(1 << 29)
#define AEU_INPUT9_BITS_RSRV_30				(1 << 30)
#define AEU_INPUT9_BITS_RSRV_31				(1 << 31)

#define AEU_INPUT9_BITS_GENERATE_SYSTEM_KILL \
        (AEU_INPUT9_BITS_MCP_LATCHED_MEMORY_PARITY)

#define AEU_INPUT9_BITS_GENERATE_PROCESS_KILL		(0)

#define AEU_INPUT9_BITS_PARITY_ERROR            \
        (AEU_INPUT9_BITS_GENERATE_SYSTEM_KILL | \
         AEU_INPUT9_BITS_GENERATE_PROCESS_KILL)

#define AEU_INPUT9_BITS_PARITY_COMMON_BLOCKS \
        (AEU_INPUT9_BITS_MCP_LATCHED_MEMORY_PARITY)

#endif /* AEU_INPUTS_H */
#endif
