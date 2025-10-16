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
#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/string.h>
#define __PREVENT_DUMP_MEM_ARR__
#define __PREVENT_PXP_GLOBAL_WIN__
#include "qed.h"
#include "qed_hsi.h"
#include "qed_hw.h"
#include "qed_mcp.h"
#include "qed_mfw_hsi.h"
#include "qed_phy_api.h"
#include "qed_reg_addr.h"

#define SERDESID 0x900e

/* max 128 char per line in the dump output */
#define MAX_CHAR_PER_LINE       128

static int
qed_phy_read(struct qed_hwfn *p_hwfn,
	     u32 port, u32 lane, u32 addr, u32 cmd, u8 * buf)
{
	u32 len;

	return qed_mcp_phy_read(p_hwfn->cdev, cmd,
				addr | (lane << 16) | (1 << 29) | (port << 30),
				buf, &len);
}

static int
qed_phy_write(struct qed_hwfn *p_hwfn,
	      u32 port, u32 lane, u32 addr, u32 data_lo, u32 data_hi, u32 cmd)
{
	u8 buf64[8] = { 0 };

	memcpy(buf64, &data_lo, 4);
	memcpy(buf64 + 4, &data_hi, 4);

	return qed_mcp_phy_write(p_hwfn->cdev, cmd,
				 addr | (lane << 16) | (1 << 29) | (port << 30),
				 buf64, 8);
}

/* phy core write */
int qed_phy_core_write(struct qed_hwfn *p_hwfn,
		       u32 port,
		       u32 addr,
		       u32 data_lo, u32 data_hi, char *p_phy_result_buf)
{
	int rc = -EINVAL;

	if (port > 3) {
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "ERROR! Port must be in range of 0..3\n");
		return rc;
	}

	/* write to address */
	rc = qed_phy_write(p_hwfn, port, 0 /* lane */ , addr, data_lo, data_hi,
			   QED_PHY_CORE_WRITE);
	if (!rc)
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE, "0\n");
	else
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Failed placing phy_core command\n");

	return rc;
}

/* phy core read */
int qed_phy_core_read(struct qed_hwfn *p_hwfn,
		      u32 port, u32 addr, char *p_phy_result_buf)
{
	int rc = -EINVAL;
	u8 buf64[8] = { 0 };
	u8 data_hi[4];
	u8 data_lo[4];

	if (port > 3) {
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "ERROR! Port must be in range of 0..3\n");
		return rc;
	}

	/* read from address */
	rc = qed_phy_read(p_hwfn, port, 0 /* lane */ , addr,
			  QED_PHY_CORE_READ, buf64);
	if (!rc) {
		memcpy(data_lo, buf64, 4);
		memcpy(data_hi, (buf64 + 4), 4);
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE, "0x%08x%08x\n",
			  *(u32 *) data_hi, *(u32 *) data_lo);
	} else {
		scnprintf(p_phy_result_buf,
			  MAX_CHAR_PER_LINE,
			  "Failed placing phy_core command\n");
	}

	return rc;
}

/* phy raw write */
int qed_phy_raw_write(struct qed_hwfn *p_hwfn,
		      u32 port,
		      u32 lane,
		      u32 addr,
		      u32 data_lo, u32 data_hi, char *p_phy_result_buf)
{
	int rc = -EINVAL;

	/* check if the enterd port is in the range */
	if (port > 3) {
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Port must be in range of 0..3\n");
		return rc;
	}

	/* check if the enterd lane is in the range */
	if (lane > 6) {
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Lane must be in range of 0..6\n");
		return rc;
	}

	/* write to address */
	rc = qed_phy_write(p_hwfn, port, lane, addr, data_lo, data_hi,
			   QED_PHY_RAW_WRITE);
	if (!rc)
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE, "0\n");
	else
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Failed placing phy_core command\n");

	return rc;
}

/* phy raw read */
int qed_phy_raw_read(struct qed_hwfn *p_hwfn,
		     u32 port, u32 lane, u32 addr, char *p_phy_result_buf)
{
	int rc = -EINVAL;
	u8 buf64[8] = { 0 };
	u8 data_hi[4];
	u8 data_lo[4];

	/* check if the enterd port is in the range */
	if (port > 3) {
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Port must be in range of 0..3\n");
		return rc;
	}

	/* check if the enterd lane is in the range */
	if (lane > 6) {
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Lane must be in range of 0..6\n");
		return rc;
	}

	/* read from address */
	rc = qed_phy_read(p_hwfn, port, lane, addr, QED_PHY_RAW_READ, buf64);
	if (!rc) {
		memcpy(data_lo, buf64, 4);
		memcpy(data_hi, (buf64 + 4), 4);
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE, "0x%08x%08x\n",
			  *(u32 *) data_hi, *(u32 *) data_lo);
	} else {
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Failed placing phy_core command\n");
	}

	return rc;
}

static u32 qed_phy_get_nvm_cfg1_addr(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt)
{
	u32 nvm_cfg_addr, nvm_cfg1_offset;

	nvm_cfg_addr = qed_rd(p_hwfn, p_ptt, MISC_REG_GEN_PURP_CR0);
	nvm_cfg1_offset = qed_rd(p_hwfn, p_ptt, nvm_cfg_addr +
				 offsetof(struct nvm_cfg,
					  sections_offset
					  [NVM_CFG_SECTION_NVM_CFG1]));
	return MCP_REG_SCRATCH + nvm_cfg1_offset;
}

/* get phy info */
int qed_phy_info(struct qed_hwfn *p_hwfn,
		 struct qed_ptt *p_ptt, char *p_phy_result_buf)
{
	u32 nvm_cfg1_addr = qed_phy_get_nvm_cfg1_addr(p_hwfn, p_ptt);
	u32 port_mode, port, max_ports, core_cfg, length = 0;
	int rc = -EINVAL;
	u8 buf64[8] = { 0 };
	u8 data_hi[4];
	u8 data_lo[4];

	if (QED_IS_BB(p_hwfn->cdev))
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "Device: BB ");
	else
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "Device: AH ");

	core_cfg = qed_rd(p_hwfn, p_ptt, nvm_cfg1_addr +
			  offsetof(struct nvm_cfg1, glob.core_cfg));
	port_mode = GET_MFW_FIELD(core_cfg, NVM_CFG1_GLOB_NETWORK_PORT_MODE);
	switch (port_mode) {
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_1X100G:
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "1x100G\n");
		max_ports = 1;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_1X40G:
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "1x40G\n");
		max_ports = 1;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_1X25G:
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "1x25G\n");
		max_ports = 1;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_2X40G:
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "2x40G\n");
		max_ports = 2;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X50G:
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "2x50G\n");
		max_ports = 2;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X25G:
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "2x25G\n");
		max_ports = 2;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X10G:
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "2x10G\n");
		max_ports = 2;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_4X10G_F:
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "4x10G\n");
		max_ports = 4;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_4X10G_E:
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "4x10G\n");
		max_ports = 4;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_4X20G:
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "4x20G\n");
		max_ports = 4;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_4X25G:
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "4x25G\n");
		max_ports = 4;
		break;
	default:
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "Wrong port mode\n");
		return rc;
	}

	if (QED_IS_BB(p_hwfn->cdev)) {
		for (port = 0; port < max_ports; port++) {
			rc = qed_phy_read(p_hwfn, port, 0, SERDESID,
					  DRV_MSG_CODE_PHY_RAW_READ, buf64);
			if (!rc) {
				length += scnprintf(&p_phy_result_buf[length],
						    MAX_CHAR_PER_LINE,
						    "Port %d is in ", port);
				memcpy(data_lo, buf64, 4);
				memcpy(data_hi, (buf64 + 4), 4);
				if ((data_lo[0] & 0x3f) == 0x14) {
					length +=
					    scnprintf(&p_phy_result_buf[length],
						      MAX_CHAR_PER_LINE,
						      "Falcon\n");
				} else {
					length +=
					    scnprintf(&p_phy_result_buf[length],
						      MAX_CHAR_PER_LINE,
						      "Eagle\n");
				}
			}
		}
	} else {
		/* @@@TMP until qed_phy_read() on AH is supported */
		for (port = 0; port < max_ports; port++)
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "Port %d is in MPS25\n", port);
		rc = 0;
	}

	return rc;
}

struct tsc_stat {
	u32 reg;
	char *name;
	char *desc;
};

static struct tsc_stat ah_stat_regs[] = {
	{0x000100, "ETHERSTATSOCTETS               ", "total, good and bad"},
/*	{0x000104, "ETHERSTATSOCTETS_H             ", "total, good and bad"},*/
	{0x000108, "OCTETSOK                       ", "total, good"},
/*	{0x00010c, "OCTETSOK_H                     ", "total, good"}, */
	{0x000110, "AALIGNMENTERRORS               ", "Wrong SFD detected"},
/*	{0x000114, "AALIGNMENTERRORS_H             ", "Wrong SFD detected"}, */
	{0x000118, "APAUSEMACCTRLFRAMES            ",
	 "Good Pause frames received"},
/*	{0x00011c, "APAUSEMACCTRLFRAMES_H          ", "Good Pause frames received"}, */
	{0x000120, "FRAMESOK                       ", "Good frames received"},
/*	{0x000124, "FRAMESOK_H                     ", "Good frames received"}, */
	{0x000128, "CRCERRORS                      ",
	 "wrong CRC and good length received"},
/*	{0x00012c, "CRCERRORS_H                    ", "wrong CRC and good length received"}, */
	{0x000130, "VLANOK                         ",
	 "Good Frames with VLAN tag received"},
/*	{0x000134, "VLANOK_H                       ", "Good Frames with VLAN tag received"}, */
	{0x000138, "IFINERRORS                     ",
	 "Errored frames received"},
/*	{0x00013c, "IFINERRORS_H                   ", "Errored frames received"}, */
	{0x000140, "IFINUCASTPKTS                  ", "Good Unicast received"},
/*	{0x000144, "IFINUCASTPKTS_H                ", "Good Unicast received"}, */
	{0x000148, "IFINMCASTPKTS                  ",
	 "Good Multicast received"},
/*	{0x00014c, "IFINMCASTPKTS_H                ", "Good Multicast received"}, */
	{0x000150, "IFINBCASTPKTS                  ",
	 "Good Broadcast received"},
/*	{0x000154, "IFINBCASTPKTS_H                ", "Good Broadcast received"}, */
	{0x000158, "ETHERSTATSDROPEVENTS           ", "Dropped frames"},
/*	{0x00015c, "ETHERSTATSDROPEVENTS_H         ", "Dropped frames"}, */
	{0x000160, "ETHERSTATSPKTS                 ",
	 "Frames received, good and bad"},
/*	{0x000164, "ETHERSTATSPKTS_H               ", "Frames received, good and bad"}, */
	{0x000168, "ETHERSTATSUNDERSIZEPKTS        ",
	 "Frames received less 64 with good crc"},
/*	{0x00016c, "ETHERSTATSUNDERSIZEPKTS_H      ", "Frames received less 64 with good crc"}, */
	{0x000170, "ETHERSTATSPKTS64               ",
	 "Frames of 64 octets received"},
/*	{0x000174, "ETHERSTATSPKTS64_H             ", "Frames of 64 octets received"}, */
	{0x000178, "ETHERSTATSPKTS65TO127          ",
	 "Frames of 65 to 127 octets received"},
/*       {0x00017c, "ETHERSTATSPKTS65TO127_H        ", "Frames of 65 to 127 octets received"}, */
	{0x000180, "ETHERSTATSPKTS128TO255         ",
	 "Frames of 128 to 255 octets received"},
/*	{0x000184, "ETHERSTATSPKTS128TO255_H       ", "Frames of 128 to 255 octets received"}, */
	{0x000188, "ETHERSTATSPKTS256TO511         ",
	 "Frames of 256 to 511 octets received"},
/*	{0x00018c, "ETHERSTATSPKTS256TO511_H       ", "Frames of 256 to 511 octets received"},*/
	{0x000190, "ETHERSTATSPKTS512TO1023        ",
	 "Frames of 512 to 1023 octets received"},
/*	{0x000194, "ETHERSTATSPKTS512TO1023_H      ", "Frames of 512 to 1023 octets received"},*/
	{0x000198, "ETHERSTATSPKTS1024TO1518       ",
	 "Frames of 1024 to 1518 octets received"},
/*	{0x00019c, "ETHERSTATSPKTS1024TO1518_H     ", "Frames of 1024 to 1518 octets received"},*/
	{0x0001a0, "ETHERSTATSPKTS1519TOMAX        ",
	 "Frames of 1519 to FRM_LENGTH octets received"},
/*	{0x0001a4, "ETHERSTATSPKTS1519TOMAX_H      ", "Frames of 1519 to FRM_LENGTH octets received"},*/
	{0x0001a8, "ETHERSTATSPKTSOVERSIZE         ",
	 "Frames greater FRM_LENGTH and good CRC received"},
/*	{0x0001ac, "ETHERSTATSPKTSOVERSIZE_H       ", "Frames greater FRM_LENGTH and good CRC received"},*/
	{0x0001b0, "ETHERSTATSJABBERS              ",
	 "Frames greater FRM_LENGTH and bad CRC received"},
/*	{0x0001b4, "ETHERSTATSJABBERS_H            ", "Frames greater FRM_LENGTH and bad CRC received"},*/
	{0x0001b8, "ETHERSTATSFRAGMENTS            ",
	 "Frames less 64 and bad CRC received"},
/*	{0x0001bc, "ETHERSTATSFRAGMENTS_H          ", "Frames less 64 and bad CRC received"},*/
	{0x0001c0, "AMACCONTROLFRAMES              ",
	 "Good frames received of type 0x8808 but not Pause"},
/*	{0x0001c4, "AMACCONTROLFRAMES_H            ", "Good frames received of type 0x8808 but not Pause"},*/
	{0x0001c8, "AFRAMETOOLONG                  ",
	 "Good and bad frames exceeding FRM_LENGTH received"},
/*	{0x0001cc, "AFRAMETOOLONG_H                ", "Good and bad frames exceeding FRM_LENGTH received"},*/
	{0x0001d0, "AINRANGELENGTHERROR            ",
	 "Good frames with invalid length field (not supported)"},
/*	{0x0001d4, "AINRANGELENGTHERROR_H          ", "Good frames with invalid length field (not supported)"},*/
	{0x000200, "TXETHERSTATSOCTETS             ", "total, good and bad"},
/*	{0x000204, "TXETHERSTATSOCTETS_H           ", "total, good and bad"},*/
	{0x000208, "TXOCTETSOK                     ", "total, good"},
/*	{0x00020c, "TXOCTETSOK_H                   ", "total, good"},*/
	{0x000218, "TXAPAUSEMACCTRLFRAMES          ",
	 "Good Pause frames transmitted"},
/*	{0x00021c, "TXAPAUSEMACCTRLFRAMES_H        ", "Good Pause frames transmitted"},*/
	{0x000220, "TXFRAMESOK                     ",
	 "Good frames transmitted"},
/*	{0x000224, "TXFRAMESOK_H                   ", "Good frames transmitted"},*/
	{0x000228, "TXCRCERRORS                    ", "wrong CRC transmitted"},
/*	{0x00022c, "TXCRCERRORS_H                  ", "wrong CRC transmitted"},*/
	{0x000230, "TXVLANOK                       ",
	 "Good Frames with VLAN tag transmitted"},
/*	{0x000234, "TXVLANOK_H                     ", "Good Frames with VLAN tag transmitted"},*/
	{0x000238, "IFOUTERRORS                    ",
	 "Errored frames transmitted"},
/*	{0x00023c, "IFOUTERRORS_H                  ", "Errored frames transmitted"},*/
	{0x000240, "IFOUTUCASTPKTS                 ",
	 "Good Unicast transmitted"},
/*	{0x000244, "IFOUTUCASTPKTS_H               ", "Good Unicast transmitted"},*/
	{0x000248, "IFOUTMCASTPKTS                 ",
	 "Good Multicast transmitted"},
/*	{0x00024c, "IFOUTMCASTPKTS_H               ", "Good Multicast transmitted"},*/
	{0x000250, "IFOUTBCASTPKTS                 ",
	 "Good Broadcast transmitted"},
/*	{0x000254, "IFOUTBCASTPKTS_H               ", "Good Broadcast transmitted"},*/
	{0x000258, "TXETHERSTATSDROPEVENTS         ",
	 "Dropped frames (unused, reserved)"},
/*	{0x00025c, "TXETHERSTATSDROPEVENTS_H       ", "Dropped frames (unused, reserved)"},*/
	{0x000260, "TXETHERSTATSPKTS               ",
	 "Frames transmitted, good and bad"},
/*	{0x000264, "TXETHERSTATSPKTS_H             ", "Frames transmitted, good and bad"},*/
	{0x000268, "TXETHERSTATSUNDERSIZEPKTS      ",
	 "Frames transmitted less 64"},
/*	{0x00026c, "TXETHERSTATSUNDERSIZEPKTS_H    ", "Frames transmitted less 64"},*/
	{0x000270, "TXETHERSTATSPKTS64             ",
	 "Frames of 64 octets transmitted"},
/*	{0x000274, "TXETHERSTATSPKTS64_H           ", "Frames of 64 octets transmitted"},*/
	{0x000278, "TXETHERSTATSPKTS65TO127        ",
	 "Frames of 65 to 127 octets transmitted"},
/*	{0x00027c, "TXETHERSTATSPKTS65TO127_H      ", "Frames of 65 to 127 octets transmitted"},*/
	{0x000280, "TXETHERSTATSPKTS128TO255       ",
	 "Frames of 128 to 255 octets transmitted"},
/*	{0x000284, "TXETHERSTATSPKTS128TO255_H     ", "Frames of 128 to 255 octets transmitted"},*/
	{0x000288, "TXETHERSTATSPKTS256TO511       ",
	 "Frames of 256 to 511 octets transmitted"},
/*	{0x00028c, "TXETHERSTATSPKTS256TO511_H     ", "Frames of 256 to 511 octets transmitted"},*/
	{0x000290, "TXETHERSTATSPKTS512TO1023      ",
	 "Frames of 512 to 1023 octets transmitted"},
/*	{0x000294, "TXETHERSTATSPKTS512TO1023_H    ", "Frames of 512 to 1023 octets transmitted"},*/
	{0x000298, "TXETHERSTATSPKTS1024TO1518     ",
	 "Frames of 1024 to 1518 octets transmitted"},
/*	{0x00029c, "TXETHERSTATSPKTS1024TO1518_H   ", "Frames of 1024 to 1518 octets transmitted"},*/
	{0x0002a0, "TXETHERSTATSPKTS1519TOTX_MTU   ",
	 "Frames of 1519 to FRM_LENGTH.TX_MTU octets transmitted"},
/*	{0x0002a4, "TXETHERSTATSPKTS1519TOTX_MTU_H ", "Frames of 1519 to FRM_LENGTH.TX_MTU octets transmitted"},*/
	{0x0002c0, "TXAMACCONTROLFRAMES            ",
	 "Good frames transmitted of type 0x8808 but not Pause"},
/*	{0x0002c4, "TXAMACCONTROLFRAMES_H          ", "Good frames transmitted of type 0x8808 but not Pause"},*/
	{0x000380, "ACBFCPAUSEFRAMESRECEIVED_0     ",
	 "Set of 8 objects recording the number of CBFC (Class Based Flow Control) pause frames received for each class."},
/*	{0x000384, "ACBFCPAUSEFRAMESRECEIVED_0_H   ", "Upper 32bit of 64bit counter."},*/
	{0x000388, "ACBFCPAUSEFRAMESRECEIVED_1     ",
	 "Set of 8 objects recording the number of CBFC (Class Based Flow Control) pause frames received for each class."},
/*	{0x00038c, "ACBFCPAUSEFRAMESRECEIVED_1_H   ", "Upper 32bit of 64bit counter."},*/
	{0x000390, "ACBFCPAUSEFRAMESRECEIVED_2     ",
	 "Set of 8 objects recording the number of CBFC (Class Based Flow Control) pause frames received for each class."},
/*	{0x000394, "ACBFCPAUSEFRAMESRECEIVED_2_H   ", "Upper 32bit of 64bit counter."},*/
	{0x000398, "ACBFCPAUSEFRAMESRECEIVED_3     ",
	 "Set of 8 objects recording the number of CBFC (Class Based Flow Control) pause frames received for each class."},
/*	{0x00039c, "ACBFCPAUSEFRAMESRECEIVED_3_H   ", "Upper 32bit of 64bit counter."},*/
	{0x0003a0, "ACBFCPAUSEFRAMESRECEIVED_4     ",
	 "Set of 8 objects recording the number of CBFC (Class Based Flow Control) pause frames received for each class."},
/*	{0x0003a4, "ACBFCPAUSEFRAMESRECEIVED_4_H   ", "Upper 32bit of 64bit counter."},*/
	{0x0003a8, "ACBFCPAUSEFRAMESRECEIVED_5     ",
	 "Set of 8 objects recording the number of CBFC (Class Based Flow Control) pause frames received for each class."},
/*	{0x0003ac, "ACBFCPAUSEFRAMESRECEIVED_5_H   ", "Upper 32bit of 64bit counter."},*/
	{0x0003b0, "ACBFCPAUSEFRAMESRECEIVED_6     ",
	 "Set of 8 objects recording the number of CBFC (Class Based Flow Control) pause frames received for each class."},
/*	{0x0003b4, "ACBFCPAUSEFRAMESRECEIVED_6_H   ", "Upper 32bit of 64bit counter."},*/
	{0x0003b8, "ACBFCPAUSEFRAMESRECEIVED_7     ",
	 "Set of 8 objects recording the number of CBFC (Class Based Flow Control) pause frames received for each class."},
/*	{0x0003bc, "ACBFCPAUSEFRAMESRECEIVED_7_H   ", "Upper 32bit of 64bit counter."},*/
	{0x0003c0, "ACBFCPAUSEFRAMESTRANSMITTED_0  ",
	 "Set of 8 objects recording the number of CBFC (Class Based Flow Control) pause frames transmitted for each class."},
/*	{0x0003c4, "ACBFCPAUSEFRAMESTRANSMITTED_0_H", "Upper 32bit of 64bit counter."},*/
	{0x0003c8, "ACBFCPAUSEFRAMESTRANSMITTED_1  ",
	 "Set of 8 objects recording the number of CBFC (Class Based Flow Control) pause frames transmitted for each class."},
/*	{0x0003cc, "ACBFCPAUSEFRAMESTRANSMITTED_1_H", "Upper 32bit of 64bit counter."},*/
	{0x0003d0, "ACBFCPAUSEFRAMESTRANSMITTED_2  ",
	 "Set of 8 objects recording the number of CBFC (Class Based Flow Control) pause frames transmitted for each class."},
/*	{0x0003d4, "ACBFCPAUSEFRAMESTRANSMITTED_2_H", "Upper 32bit of 64bit counter."},*/
	{0x0003d8, "ACBFCPAUSEFRAMESTRANSMITTED_3  ",
	 "Set of 8 objects recording the number of CBFC (Class Based Flow Control) pause frames transmitted for each class."},
/*	{0x0003dc, "ACBFCPAUSEFRAMESTRANSMITTED_3_H", "Upper 32bit of 64bit counter."},*/
	{0x0003e0, "ACBFCPAUSEFRAMESTRANSMITTED_4  ",
	 "Set of 8 objects recording the number of CBFC (Class Based Flow Control) pause frames transmitted for each class."},
/*	{0x0003e4, "ACBFCPAUSEFRAMESTRANSMITTED_4_H", "Upper 32bit of 64bit counter."},*/
	{0x0003e8, "ACBFCPAUSEFRAMESTRANSMITTED_5  ",
	 "Set of 8 objects recording the number of CBFC (Class Based Flow Control) pause frames transmitted for each class."},
/*	{0x0003ec, "ACBFCPAUSEFRAMESTRANSMITTED_5_H", "Upper 32bit of 64bit counter."},*/
	{0x0003f0, "ACBFCPAUSEFRAMESTRANSMITTED_6  ",
	 "Set of 8 objects recording the number of CBFC (Class Based Flow Control) pause frames transmitted for each class."},
/*	{0x0003f4, "ACBFCPAUSEFRAMESTRANSMITTED_6_H", "Upper 32bit of 64bit counter."},*/
	{0x0003f8, "ACBFCPAUSEFRAMESTRANSMITTED_7  ",
	 "Set of 8 objects recording the number of CBFC (Class Based Flow Control) pause frames transmitted for each class."},
/*	{0x0003fc, "ACBFCPAUSEFRAMESTRANSMITTED_7_H", "Upper 32bit of 64bit counter."}*/
};

static struct tsc_stat bb_stat_regs[] = {
	{0x00000000, "GRX64", "RX 64-byte frame counter"},
	{0x00000001, "GRX127", "RX 65 to 127 byte frame counter"},
	{0x00000002, "GRX255", "RX 128 to 255 byte frame counter"},
	{0x00000003, "GRX511", "RX 256 to 511 byte frame counter"},
	{0x00000004, "GRX1023", "RX 512 to 1023 byte frame counter"},
	{0x00000005, "GRX1518", "RX 1024 to 1518 byte frame counter"},
	{0x00000006, "GRX1522",
	 "RX 1519 to 1522 byte VLAN-tagged frame counter"},
	{0x00000007, "GRX2047", "RX 1519 to 2047 byte frame counter"},
	{0x00000008, "GRX4095", "RX 2048 to 4095 byte frame counter"},
	{0x00000009, "GRX9216", "RX 4096 to 9216 byte frame counter"},
	{0x0000000a, "GRX16383", "RX 9217 to 16383 byte frame counter"},
	{0x0000000b, "GRXPKT", "RX frame counter (all packets)"},
	{0x0000000c, "GRXUCA", "RX UC frame counter"},
	{0x0000000d, "GRXMCA", "RX MC frame counter"},
	{0x0000000e, "GRXBCA", "RX BC frame counter"},
	{0x0000000f, "GRXFCS", "RX FCS error frame counter"},
	{0x00000010, "GRXCF", "RX control frame counter"},
	{0x00000011, "GRXPF", "RX pause frame counter"},
	{0x00000012, "GRXPP", "RX PFC frame counter"},
	{0x00000013, "GRXUO", "RX unsupported opcode frame counter"},
	{0x00000014, "GRXUDA",
	 "RX unsupported DA for pause/PFC frame counter"},
	{0x00000015, "GRXWSA", "RX incorrect SA counter"},
	{0x00000016, "GRXALN", "RX alignment error counter"},
	{0x00000017, "GRXFLR", "RX out-of-range length frame counter"},
	{0x00000018, "GRXFRERR", "RX code error frame counter"},
	{0x00000019, "GRXFCR", "RX false carrier counter"},
	{0x0000001a, "GRXOVR", "RX oversized frame counter"},
	{0x0000001b, "GRXJBR", "RX jabber frame counter"},
	{0x0000001c, "GRXMTUE", "RX MTU check error frame counter"},
	{0x0000001d, "GRXMCRC",
	 "RX packet with 4-Byte CRC matching MACSEC_PROG_TX_CRC."},
	{0x0000001e, "GRXPRM", "RX promiscuous packet counter"},
	{0x0000001f, "GRXVLN",
	 "RX single and double VLAN tagged frame counter"},
	{0x00000020, "GRXDVLN", "RX double VLANG tagged frame counter"},
	{0x00000021, "GRXTRFU",
	 "RX truncated frame (due to RX FIFO full) counter"},
	{0x00000022, "GRXPOK",
	 "RX good frame (good CRC, not oversized, no ERROR)"},
	{0x00000023, "GRXPFCOFF0",
	 "RX PFC frame transition XON to XOFF for Priority0"},
	{0x00000024, "GRXPFCOFF1",
	 "RX PFC frame transition XON to XOFF for Priority1"},
	{0x00000025, "GRXPFCOFF2",
	 "RX PFC frame transition XON to XOFF for Priority2"},
	{0x00000026, "GRXPFCOFF3",
	 "RX PFC frame transition XON to XOFF for Priority3"},
	{0x00000027, "GRXPFCOFF4",
	 "RX PFC frame transition XON to XOFF for Priority4"},
	{0x00000028, "GRXPFCOFF5",
	 "RX PFC frame transition XON to XOFF for Priority5"},
	{0x00000029, "GRXPFCOFF6",
	 "RX PFC frame transition XON to XOFF for Priority6"},
	{0x0000002a, "GRXPFCOFF7",
	 "RX PFC frame transition XON to XOFF for Priority7"},
	{0x0000002b, "GRXPFCP0",
	 "RX PFC frame with enable bit set for Priority0"},
	{0x0000002c, "GRXPFCP1",
	 "RX PFC frame with enable bit set for Priority1"},
	{0x0000002d, "GRXPFCP2",
	 "RX PFC frame with enable bit set for Priority2"},
	{0x0000002e, "GRXPFCP3",
	 "RX PFC frame with enable bit set for Priority3"},
	{0x0000002f, "GRXPFCP4",
	 "RX PFC frame with enable bit set for Priority4"},
	{0x00000030, "GRXPFCP5",
	 "RX PFC frame with enable bit set for Priority5"},
	{0x00000031, "GRXPFCP6",
	 "RX PFC frame with enable bit set for Priority6"},
	{0x00000032, "GRXPFCP7",
	 "RX PFC frame with enable bit set for Priority7"},
	{0x00000033, "GRXSCHCRC",
	 "RX frame with SCH CRC error. For LH mode only"},
	{0x00000034, "GRXUND", "RX undersized frame counter"},
	{0x00000035, "GRXFRG", "RX fragment counter"},
	{0x00000036, "RXEEELPI", "RX EEE LPI counter"},
	{0x00000037, "RXEEELPIDU", "RX EEE LPI duration counter"},
	{0x00000038, "RXLLFCPHY", "RX LLFC PHY COUNTER"},
	{0x00000039, "RXLLFCLOG", "RX LLFC LOG COUNTER"},
	{0x0000003a, "RXLLFCCRC", "RX LLFC CRC COUNTER"},
	{0x0000003b, "RXHCFC", "RX HCFC COUNTER"},
	{0x0000003c, "RXHCFCCRC", "RX HCFC CRC COUNTER"},
	{0x0000003d, "GRXBYT", "RX byte counter"},
	{0x0000003e, "GRXRBYT", "RX runt byte counter"},
	{0x0000003f, "GRXRPKT", "RX packet counter"},
	{0x00000040, "GTX64", "TX 64-byte frame counter"},
	{0x00000041, "GTX127", "TX 65 to 127 byte frame counter"},
	{0x00000042, "GTX255", "TX 128 to 255 byte frame counter"},
	{0x00000043, "GTX511", "TX 256 to 511 byte frame counter"},
	{0x00000044, "GTX1023", "TX 512 to 1023 byte frame counter"},
	{0x00000045, "GTX1518", "TX 1024 to 1518 byte frame counter"},
	{0x00000046, "GTX1522",
	 "TX 1519 to 1522 byte VLAN-tagged frame counter"},
	{0x00000047, "GTX2047", "TX 1519 to 2047 byte frame counter"},
	{0x00000048, "GTX4095", "TX 2048 to 4095 byte frame counte"},
	{0x00000049, "GTX9216", "TX 4096 to 9216 byte frame counter"},
	{0x0000004a, "GTX16383", "TX 9217 to 16383 byte frame counter"},
	{0x0000004b, "GTXPOK", "TX good frame counter"},
	{0x0000004c, "GTXPKT", "TX frame counter (all packets"},
	{0x0000004d, "GTXUCA", "TX UC frame counter"},
	{0x0000004e, "GTXMCA", "TX MC frame counter"},
	{0x0000004f, "GTXBCA", "TX BC frame counter"},
	{0x00000050, "GTXPF", "TX pause frame counter"},
	{0x00000051, "GTXPP", "TX PFC frame counter"},
	{0x00000052, "GTXJBR", "TX jabber counter"},
	{0x00000053, "GTXFCS", "TX FCS error counter"},
	{0x00000054, "GTXCF", "TX control frame counter"},
	{0x00000055, "GTXOVR", "TX oversize packet counter"},
	{0x00000056, "GTXDFR", "TX Single Deferral Frame Counter"},
	{0x00000057, "GTXEDF", "TX Multiple Deferral Frame Counter"},
	{0x00000058, "GTXSCL", "TX Single Collision Frame Counter"},
	{0x00000059, "GTXMCL", "TX Multiple Collision Frame Counter"},
	{0x0000005a, "GTXLCL", "TX Late Collision Frame Counter"},
	{0x0000005b, "GTXXCL", "TX Excessive Collision Frame Counter"},
	{0x0000005c, "GTXFRG", "TX fragment counter"},
	{0x0000005d, "GTXERR", "TX error (set by system) frame counter"},
	{0x0000005e, "GTXVLN", "TX VLAN Tag Frame Counter"},
	{0x0000005f, "GTXDVLN", "TX Double VLAN Tag Frame Counter"},
	{0x00000060, "GTXRPKT", "TX RUNT Frame Counter"},
	{0x00000061, "GTXUFL", "TX FIFO Underrun Counter"},
	{0x00000062, "GTXPFCP0",
	 "TX PFC frame with enable bit set for Priority0"},
	{0x00000063, "GTXPFCP1",
	 "TX PFC frame with enable bit set for Priority1"},
	{0x00000064, "GTXPFCP2",
	 "TX PFC frame with enable bit set for Priority2"},
	{0x00000065, "GTXPFCP3",
	 "TX PFC frame with enable bit set for Priority3"},
	{0x00000066, "GTXPFCP4",
	 "TX PFC frame with enable bit set for Priority4"},
	{0x00000067, "GTXPFCP5",
	 "TX PFC frame with enable bit set for Priority5"},
	{0x00000068, "GTXPFCP6",
	 "TX PFC frame with enable bit set for Priority6"},
	{0x00000069, "GTXPFCP7",
	 "TX PFC frame with enable bit set for Priority7"},
	{0x0000006a, "TXEEELPI", "TX EEE LPI Event Counter"},
	{0x0000006b, "TXEEELPIDU", "TX EEE LPI Duration Counter"},
	{0x0000006c, "TXLLFCLOG",
	 "Transmit Logical Type LLFC message counter"},
	{0x0000006d, "TXHCFC",
	 "Transmit Logical Type LLFC message counter"},
	{0x0000006e, "GTXNCL", "Transmit Total Collision Counter"},
	{0x0000006f, "GTXBYT", "TX byte counter"}
};

/* get mac status */
static int qed_bb_phy_mac_stat(struct qed_hwfn *p_hwfn,
			       u32 port, char *p_phy_result_buf)
{
	u8 buf64[8] = { 0 }, data_hi[4], data_lo[4];
	bool b_false_alarm = false;
	u32 length, reg_id, addr;
	int rc = -EINVAL;

	length = scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			   "MAC stats for port %d (only non-zero)\n", port);

	for (reg_id = 0; reg_id < ARRAY_SIZE(bb_stat_regs); reg_id++) {
		addr = bb_stat_regs[reg_id].reg;
		rc = qed_phy_read(p_hwfn, port, 0 /*lane */ , addr,
				  QED_PHY_CORE_READ, buf64);

		memcpy(data_lo, buf64, 4);
		memcpy(data_hi, (buf64 + 4), 4);

		if (!rc) {
			if (*(u32 *) data_lo != 0) {	/* Only non-zero */
				length += scnprintf(&p_phy_result_buf[length],
						    MAX_CHAR_PER_LINE,
						    "%-10s: 0x%08x (%s)\n",
						    bb_stat_regs[reg_id].name,
						    *(u32 *) data_lo,
						    bb_stat_regs[reg_id].desc);
				if ((bb_stat_regs[reg_id].reg == 0x0000000f) ||
				    (bb_stat_regs[reg_id].reg == 0x00000018) ||
				    (bb_stat_regs[reg_id].reg == 0x00000035))
					b_false_alarm = true;
			}
		} else {
			scnprintf(p_phy_result_buf,
				  MAX_CHAR_PER_LINE,
				  "Failed reading stat 0x%x\n\n", addr);
		}
	}

	if (b_false_alarm)
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE,
				    "Note: GRXFCS/GRXFRERR/GRXFRG may "
				    "increment when the port shuts down\n");

	return rc;
}

/* get mac status */
static int qed_ah_phy_mac_stat(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt,
			       u32 port, char *p_phy_result_buf)
{
	u32 length, reg_id, addr, data_lo /*, data_hi */ ;

	length = scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			   "MAC stats for port %d (only non-zero)\n", port);

	for (reg_id = 0; reg_id < ARRAY_SIZE(ah_stat_regs); reg_id++) {
		addr = ah_stat_regs[reg_id].reg;
		data_lo = qed_rd(p_hwfn, p_ptt,
				 NWM_REG_MAC0_K2 +
				 NWM_REG_MAC0_SIZE * 4 * port + addr);
		/*data_hi = qed_rd(p_hwfn, p_ptt,
		 *                 NWM_REG_MAC0_K2 +
		 *                 NWM_REG_MAC0_SIZE * 4 * port +
		 *                 addr + 4);
		 */

		if (data_lo) {	/* Only non-zero */
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "%-10s: 0x%08x (%s)\n",
					    ah_stat_regs[reg_id].name,
					    data_lo, ah_stat_regs[reg_id].desc);
		}
	}

	return 0;
}

int qed_phy_mac_stat(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt, u32 port, char *p_phy_result_buf)
{
	int num_ports = qed_device_num_ports(p_hwfn->cdev);

	if (port >= (u32) num_ports) {
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Port must be in range of 0..%d\n", num_ports);
		return -EINVAL;
	}

	if (QED_IS_BB(p_hwfn->cdev))
		return qed_bb_phy_mac_stat(p_hwfn, port, p_phy_result_buf);
	else
		return qed_ah_phy_mac_stat(p_hwfn, p_ptt, port,
					   p_phy_result_buf);
}

#define SFP_RX_LOS_OFFSET 110
#define SFP_TX_DISABLE_OFFSET 110
#define SFP_TX_FAULT_OFFSET 110

#define QSFP_RX_LOS_OFFSET 3
#define QSFP_TX_DISABLE_OFFSET 86
#define QSFP_TX_FAULT_OFFSET 4

/* Set SFP error string */
static int qed_sfp_set_error(int rc,
			     u32 offset,
			     char *p_phy_result_buf, char *p_err_str)
{
	if (rc) {
		if (rc == -ENODEV)
			scnprintf((char *)&p_phy_result_buf[offset],
				  MAX_CHAR_PER_LINE,
				  "Transceiver is unplugged.\n");
		else
			scnprintf((char *)&p_phy_result_buf[offset],
				  MAX_CHAR_PER_LINE, "%s", p_err_str);

		return rc;
	}

	return 0;
}

/* Validate SFP port */
static int qed_validate_sfp_port(struct qed_hwfn *p_hwfn,
				 u32 port, char *p_phy_result_buf)
{
	/* Verify <port> field is between 0 and number of ports */
	u32 num_ports = qed_device_num_ports(p_hwfn->cdev);

	if (port >= num_ports) {
		if (num_ports == 1)
			scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
				  "Bad port number, must be 0.\n");
		else
			scnprintf(p_phy_result_buf,
				  MAX_CHAR_PER_LINE,
				  "Bad port number, must be between 0 and %d.\n",
				  num_ports - 1);

		return -EINVAL;
	}

	return 0;
}

/* Validate SFP parameters */
static int
qed_validate_sfp_parameters(struct qed_hwfn *p_hwfn,
			    u32 port,
			    u32 addr,
			    u32 offset, u32 size, char *p_phy_result_buf)
{
	int rc;

	/* Verify <port> field is between 0 and number of ports */
	rc = qed_validate_sfp_port(p_hwfn, port, p_phy_result_buf);
	if (rc)
		return rc;

	/* Verify <I2C> field is 0xA0 or 0xA2 */
	if ((addr != 0xA0) && (addr != 0xA2)) {
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Bad I2C address, must be 0xA0 or 0xA2.\n");
		return -EINVAL;
	}

	/* Verify <size> field is 1 - MAX_I2C_TRANSCEIVER_PAGE_SIZE */
	if ((size == 0) || (size > MAX_I2C_TRANSCEIVER_PAGE_SIZE)) {
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Bad size, must be between 1 and %d.\n",
			  MAX_I2C_TRANSCEIVER_PAGE_SIZE);
		return -EINVAL;
	}

	/* Verify <offset> + <size> <= MAX_I2C_TRANSCEIVER_PAGE_SIZE */
	if (offset + size > MAX_I2C_TRANSCEIVER_PAGE_SIZE) {
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Bad offset and size, must not exceed %d.\n",
			  MAX_I2C_TRANSCEIVER_PAGE_SIZE);
		return -EINVAL;
	}

	return rc;
}

/* Write to SFP */
int qed_phy_sfp_write(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt,
		      u32 port,
		      u32 addr,
		      u32 offset, u32 size, u32 val, char *p_phy_result_buf)
{
	int rc;

	rc = qed_validate_sfp_parameters(p_hwfn, port, addr, offset, size,
					 p_phy_result_buf);
	if (!rc) {
		rc = qed_mcp_phy_sfp_write(p_hwfn, p_ptt, port, addr,
					   offset, size, (u8 *) & val);

		if (rc)
			return qed_sfp_set_error(rc,
						 0,
						 p_phy_result_buf,
						 "Error writing to transceiver.\n");

		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Written successfully to transceiver.\n");
	}

	return rc;
}

/* Read from SFP */
int qed_phy_sfp_read(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt,
		     u32 port,
		     u32 addr, u32 offset, u32 size, char *p_phy_result_buf)
{
	int rc;
	u32 i;

	rc = qed_validate_sfp_parameters(p_hwfn, port, addr, offset, size,
					 p_phy_result_buf);
	if (!rc) {
		int length = 0;
		u8 buf[MAX_I2C_TRANSCEIVER_PAGE_SIZE];

		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, addr,
					  offset, size, buf);
		if (rc)
			return qed_sfp_set_error(rc,
						 0,
						 p_phy_result_buf,
						 "Error reading from transceiver.\n");
		for (i = 0; i < size; i++)
			length += scnprintf((char *)&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE, "%02x ", buf[i]);
	}

	return rc;
}

static int qed_decode_sfp_info(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt,
			       u32 port, u32 length, char *p_phy_result_buf)
{
	/* SFP EEPROM contents are described in SFF-8024 and SFF-8472 */
	/***********************************************/
	/* SFP DATA and locations                      */
	/* get specification complianace bytes 3-10    */
	/* get signal rate byte 12                     */
	/* get extended compliance code byte 36        */
	/* get vendor length bytes 14-19               */
	/* get vendor name bytes bytes 20-35           */
	/* get vendor OUI bytes 37-39                  */
	/* get vendor PN  bytes 40-55                  */
	/* get vendor REV bytes 56-59                  */
	/* validated                                   */
	/***********************************************/
	int rc;
	u8 buf[32];

	/* Read byte 12 - signal rate, and if nothing matches */
	/* check byte 8 for 10G copper                        */
	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  12, 1, buf);
	if (rc)
		return qed_sfp_set_error(rc,
					 length,
					 p_phy_result_buf,
					 "Error reading specification compliance field.\n");

	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "BYTE 12 signal rate: %d\n", buf[0]);

	if (buf[0] >= 250) {
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE,
				    "25G signal rate: %d\n", buf[0]);
		/* 25G - This should be copper - could double check */
		/* Read byte 3 - optics, and if nothing matches     */
		/* check byte 8 for 10G copper                      */
		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_TRANSCEIVER_ADDR, 3, 1, buf);
		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading optics field.\n");

		switch (buf[0]) {
		case 1:
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "25G Passive copper detected\n");
			break;
		case 2:
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "25G Active copper detected\n");
			break;
		default:
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "UNKNOWN 25G cable detected: %x\n",
					    buf[0]);
			break;
		}
	} else if (buf[3] >= 100) {
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE,
				    "10G signal rate: %d\n", buf[0]);
		/* 10G - Read byte 3 for optics and byte 8 for copper, and */
		/* byte 2 for AOC                                          */
		/* Read byte 3 - optics, and if nothing matches check byte */
		/* 8 for 10G copper                                        */
		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_TRANSCEIVER_ADDR, 3, 1, buf);
		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading optics field.\n");

		switch (buf[0]) {
		case 0x10:
			/* 10G SR */
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "10G SR detected\n");
			break;
		case 0x20:
			/* 10G LR */
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "10G LR detected\n");
			break;
		case 0x40:
			/* 10G LRM */
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "10G LRM detected\n");
			break;
		case 0x80:
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "10G ER detected\n");
			break;
		default:
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "SFP/SFP+/SFP-28 transceiver type 0x%x not known...  Check for 10G copper.\n",
					    buf[0]);
			/* Read 3, check 8 too */
			rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
						  I2C_TRANSCEIVER_ADDR,
						  8, 1, buf);
			if (rc)
				return qed_sfp_set_error(rc,
							 length,
							 p_phy_result_buf,
							 "Error reading 10G copper field.\n");

			switch (buf[0]) {
			case 0x04:
			case 0x84:
				length += scnprintf(&p_phy_result_buf[length],
						    MAX_CHAR_PER_LINE,
						    "10G Passive copper detected\n");
				break;
			case 0x08:
			case 0x88:
				length += scnprintf(&p_phy_result_buf[length],
						    MAX_CHAR_PER_LINE,
						    "10G Active copper detected\n");
				break;
			default:
				length += scnprintf(&p_phy_result_buf[length],
						    MAX_CHAR_PER_LINE,
						    "Unexpected SFP/SFP+/SFP-28 transceiver type 0x%x\n",
						    buf[3]);
				break;
			}	/* switch byte 8 */
		}		/* switch byte 3 */
	} else if (buf[0] >= 10) {
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE,
				    "1G signal rate: %d\n", buf[3]);
		/* 1G -  Read byte 6 for optics and byte 8 for copper */
		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_TRANSCEIVER_ADDR, 6, 1, buf);
		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading optics field.\n");

		switch (buf[0]) {
		case 1:
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "1G SX detected\n");
			break;
		case 2:
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "1G LX detected\n");
			break;
		default:
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "Assume 1G Passive copper detected\n");
			break;
		}
	}

	/* get vendor length bytes 14-19 */
	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  14, 6, buf);
	if (rc)
		return qed_sfp_set_error(rc, length, p_phy_result_buf,
					 "Error reading vendor length bytes.\n");

	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Length (SMF, km) 0x%x\n", buf[0]);
	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Length (SMF) 0x%x\n", buf[1]);
	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Length (50 um) 0x%x\n", buf[2]);
	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Length (62.5 um) 0x%x\n", buf[3]);
	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Length (OM4 or copper cable) 0x%x\n", buf[4]);
	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Length (OM3) 0x%x\n", buf[5]);

	/* get vendor name bytes bytes 20-35 */
	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  20, 16, buf);
	if (rc)
		return qed_sfp_set_error(rc, length, p_phy_result_buf,
					 "Error reading vendor name.\n");

	buf[16] = 0;
	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Vendor name: %s\n", buf);

	/* get vendor OUI bytes 37-39 */
	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  37, 3, buf);
	if (rc)
		return qed_sfp_set_error(rc, length, p_phy_result_buf,
					 "Error reading vendor OUI.\n");

	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Vendor OUI: %02x%02x%02x\n",
			    buf[0], buf[1], buf[2]);

	/* get vendor PN  bytes 40-55 */
	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  40, 16, buf);
	if (rc)
		return qed_sfp_set_error(rc, length, p_phy_result_buf,
					 "Error reading vendor PN.\n");

	buf[16] = 0;
	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Vendor PN: %s\n", buf);

	/* get vendor REV bytes 56-59 */
	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  56, 4, buf);
	if (rc)
		return qed_sfp_set_error(rc, length, p_phy_result_buf,
					 "Error reading vendor rev.\n");

	buf[4] = 0;
	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Vendor rev: %s\n", buf);

	return rc;
}

static int qed_decode_qsfp_info(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt,
				u32 port, u32 length, char *p_phy_result_buf)
{
	/* QSFP EEPROM contents are described in SFF-8024 and SFF-8636 */
	/***********************************************/
	/* QSFP DATA and locations                     */
	/* get specification complianace bytes 131-138 */
	/* get extended rate select bytes 141          */
	/* get vendor length bytes 142-146             */
	/* get device technology byte 147              */
	/* get vendor name bytes bytes 148-163         */
	/* get vendor OUI bytes 165-167                */
	/* get vendor PN  bytes 168-183                */
	/* get vendor REV bytes 184-185                */
	/* validated                                   */
	/***********************************************/
	int rc;
	u8 buf[32];

	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  131, 1, buf);
	if (rc)
		return qed_sfp_set_error(rc,
					 length,
					 p_phy_result_buf,
					 "Error reading transceiver compliance code.\n");

	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Transceiver compliance code 0x%x\n", buf[0]);

	switch (buf[0]) {
	case 0x1:
		/* 40G Active (XLPPI) */
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE,
				    "40G Active (XLPPI) detected.\n");
		break;
	case 0x2:
		/* 40G LR-4 */
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "40G LR-4 detected.\n");
		break;
	case 0x4:
		/* 40G SR-4 */
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "40G SR-4 detected.\n");
		break;
	case 0x8:
		/* 40G CR-4 */
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "40G CR-4 detected.\n");
		break;
	case 0x10:
		/* 10G SR */
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "10G SR detected.\n");
		break;
	case 0x20:
		/* 10G LR */
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "10G LR detected.\n");
		break;
	case 0x40:
		/* 10G LRM */
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "10G LRM detected.\n");
		break;
	case 0x88:		/* Could be 40G/100G CR4 cable, check 192 for 100G CR4 */
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE,
				    "Multi-rate transceiver: 40G CR-4 detected...\n");
		break;
	case 0x80:
		/* Use extended technology field */
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE,
				    "Use extended technology field\n");
		/* Byte 93 & 129 is supposed to have power info. During    */
		/* testing all reads 0.  Ignore for now                    */
		/* 0-127 is in the first page  this in high region -       */
		/* see what page it is.                                    */
		/*  buf[3] = 0;                                            */
		/*  ret_val = read_transceiver_data(g_port, i2c_addr, 129, */
		/*  buf, 1);                                               */
		/*  length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,      */
		/*  "Read transceiver power data.  Value read: 0x%hx\n\n", */
		/*  buf[3]);                                               */

		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_TRANSCEIVER_ADDR, 192, 1, buf);
		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading technology compliance field.\n");

		switch (buf[0]) {
		case 0:
			/* Unspecified */
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "Unspecified detected.\n");
			break;
		case 0x1:
			/* 100G AOC (active optical cable) */
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "100G AOC (active optical cable) detected\n");
			break;
		case 0x2:
			/* 100G SR-4 */
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "100G SR-4 detected\n");
			break;
		case 0x3:
			/* 100G LR-4 */
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "100G LR-4 detected\n");
			break;
		case 0x4:
			/* 100G ER-4 */
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "100G ER-4 detected\n");
			break;
		case 0x8:
			/* 100G ACC (active copper cable) */
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "100G ACC (active copper cable detected\n");
			break;
		case 0xb:
			/* 100G CR-4 */
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "100G CR-4 detected\n");
			break;
		case 0x11:
			/* 4x10G SR */
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "4x10G SR detected\n");
			break;
		default:
			length += scnprintf(&p_phy_result_buf[length],
					    MAX_CHAR_PER_LINE,
					    "Unexpected technology. NEW COMPLIANCE CODE TO SUPPORT 0x%x\n",
					    buf[0]);
			break;
		}
		break;
	default:
		/* Unexpected technology compliance field */
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE,
				    "WARNING: Unexpected technology compliance field detected 0x%x\n",
				    buf[0]);
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE,
				    "Assume SR-4 detected\n");
		break;
	}

	/* get extended rate select bytes 141 */
	/* get vendor length bytes 142-146 */
	/* get device technology byte 147 */
	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  141, 7, buf);
	if (rc)
		return qed_sfp_set_error(rc,
					 length,
					 p_phy_result_buf,
					 "Error reading extended rate select bytes.\n");

	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Extended rate select bytes 0x%x\n", buf[0]);
	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Length (SMF) 0x%x\n", buf[1]);
	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Length (OM3 50 um) 0x%x\n", buf[2]);
	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Length (OM2 50 um) 0x%x\n", buf[3]);
	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Length (OM1 62.5 um) 0x%x\n", buf[4]);
	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Length (Passive or active) 0x%x\n", buf[5]);
	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Device technology byte 0x%x\n", buf[6]);

	/* get vendor name bytes bytes 148-163 */
	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  148, 16, buf);
	if (rc)
		return qed_sfp_set_error(rc, length, p_phy_result_buf,
					 "Error reading vendor name.\n");

	buf[16] = 0;
	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Vendor name: %s\n", buf);

	/* get vendor OUI bytes 165-167 */
	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  165, 3, buf);
	if (rc)
		return qed_sfp_set_error(rc, length, p_phy_result_buf,
					 "Error reading vendor OUI.\n");

	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Vendor OUI: %02x%02x%02x\n",
			    buf[0], buf[1], buf[2]);

	/* get vendor PN  bytes 168-183 */
	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  168, 16, buf);
	if (rc)
		return qed_sfp_set_error(rc, length, p_phy_result_buf,
					 "Error reading vendor PN.\n");

	buf[16] = 0;
	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Vendor PN: %s\n", buf);

	/* get vendor REV bytes 184-185 */
	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  184, 2, buf);
	if (rc)
		return qed_sfp_set_error(rc, length, p_phy_result_buf,
					 "Error reading vendor rev.\n");

	buf[2] = 0;
	length += scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			    "Vendor rev: %s\n", buf);

	return rc;
}

/* Decode SFP information */
int qed_phy_sfp_decode(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt, u32 port, char *p_phy_result_buf)
{
	int rc;
	u32 length = 0;
	u8 buf[4];

	/* Verify <port> field is between 0 and number of ports */
	rc = qed_validate_sfp_port(p_hwfn, port, p_phy_result_buf);
	if (rc)
		return rc;

	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  0, 1, buf);
	if (rc)
		return qed_sfp_set_error(rc,
					 length,
					 p_phy_result_buf,
					 "Error reading transceiver identification field.\n");

	switch (buf[0]) {
	case 0x3:		/* SFP, SFP+, SFP-28 */
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE,
				    "SFP, SFP+ or SFP-28 inserted.\n");
		rc = qed_decode_sfp_info(p_hwfn, p_ptt, port,
					 length, p_phy_result_buf);
		break;
	case 0xc:		/* QSFP */
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "QSFP inserted.\n");
		rc = qed_decode_qsfp_info(p_hwfn, p_ptt, port,
					  length, p_phy_result_buf);
		break;
	case 0xd:		/* QSFP+ */
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "QSFP+ inserted.\n");
		rc = qed_decode_qsfp_info(p_hwfn, p_ptt, port,
					  length, p_phy_result_buf);
		break;
	case 0x11:		/* QSFP-28 */
		length += scnprintf(&p_phy_result_buf[length],
				    MAX_CHAR_PER_LINE, "QSFP-28 inserted.\n");
		rc = qed_decode_qsfp_info(p_hwfn, p_ptt, port,
					  length, p_phy_result_buf);
		break;
	case 0x12:		/* CXP2 (CXP-28) */
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "CXP2 (CXP-28) inserted.\n");
		rc = -EINVAL;
		break;
	default:
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Unknown transceiver type inserted.\n");
		rc = -EINVAL;
		break;
	}

	return rc;
}

/* Get SFP inserted status */
int qed_phy_sfp_get_inserted(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt,
			     u32 port, char *p_phy_result_buf)
{
	u32 transceiver_state;
	u32 addr = SECTION_OFFSIZE_ADDR(p_hwfn->mcp_info->public_base,
					PUBLIC_PORT);
	u32 mfw_mb_offsize = qed_rd(p_hwfn, p_ptt, addr);
	u32 port_addr = SECTION_ADDR(mfw_mb_offsize, port);

	transceiver_state = qed_rd(p_hwfn, p_ptt,
				   port_addr +
				   offsetof(struct public_port,
					    transceiver_data));

	transceiver_state = GET_MFW_FIELD(transceiver_state,
					  ETH_TRANSCEIVER_STATE);

	scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE, "%d",
		  (transceiver_state == ETH_TRANSCEIVER_STATE_PRESENT));

	return 0;
}

/* Translate port to lane map for QSFPs */
static int
qed_phy_sfp_translate_port_to_lane(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt,
				   u32 port, u32 * lane_mask)
{
	u32 nvm_cfg_addr, nvm_cfg1_offset, port_cfg_addr, num_ports, port_type;

	nvm_cfg_addr = qed_rd(p_hwfn, p_ptt, MISC_REG_GEN_PURP_CR0);
	nvm_cfg1_offset = qed_rd(p_hwfn, p_ptt, nvm_cfg_addr + 4);
	port_cfg_addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
	    offsetof(struct nvm_cfg1, port[port]);
	port_type = qed_rd(p_hwfn, p_ptt, port_cfg_addr +
			   offsetof(struct nvm_cfg1_port, board_cfg));
	port_type = GET_MFW_FIELD(port_type, NVM_CFG1_PORT_PORT_TYPE);

	/* For now only identify first lane of the port.
	 * To get full lane map, need to take into account how many
	 * ports are sharing this QSFP and what speed the port is
	 * currently configured for.
	 */
	if (port_type == NVM_CFG1_PORT_PORT_TYPE_MODULE) {
		*lane_mask = 1;
	} else if (port_type == NVM_CFG1_PORT_PORT_TYPE_MODULE_SLAVE) {
		num_ports = qed_device_num_ports(p_hwfn->cdev);
		if (num_ports == 2)
			*lane_mask = 1 << (port * 2);
		else		/* either 1 or 4 ports */
			*lane_mask = 1 << port;
	} else {
		return -EINVAL;
	}

	/* TBD need to handle port swap? */
	return 0;
}

/* Get SFP TX disable status */
int qed_phy_sfp_get_txdisable(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt,
			      u32 port, char *p_phy_result_buf)
{
	u32 transceiver_state, transceiver_type, lane_mask;
	int rc;
	u32 length = 0;
	u8 buf[4];

	/* Verify <port> field is between 0 and number of ports */
	rc = qed_validate_sfp_port(p_hwfn, port, p_phy_result_buf);
	if (rc)
		return rc;

	qed_mcp_get_transceiver_data(p_hwfn, p_ptt, &transceiver_state,
				     &transceiver_type);
	if (transceiver_state != ETH_TRANSCEIVER_STATE_PRESENT) {
		return qed_sfp_set_error(-ENODEV, length,
					 p_phy_result_buf,
					 "Transceiver not present\n");
	}

	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  0, 1, buf);
	if (rc)
		return qed_sfp_set_error(rc,
					 length,
					 p_phy_result_buf,
					 "Error reading transceiver identification field.\n");

	switch (buf[0]) {
	case 0x3:		/* SFP, SFP+, SFP-28 */
		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_DEV_ADDR_A0, 93, 1, buf);

		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading transceiver enhanced options status field.\n");

		if ((buf[0] & 0x40) == 0)
			return qed_sfp_set_error(-EINVAL,
						 length,
						 p_phy_result_buf,
						 "Optional soft TX_DISABLE Select not supported on this device\n");

		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_DEV_ADDR_A2, 110, 1, buf);
		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading transceiver tx disable status field.\n");
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE, "%d",
			  ((buf[0] & 0x40) ? 1 : 0));
		break;
	case 0xc:		/* QSFP */
	case 0xd:		/* QSFP+ */
	case 0x11:		/* QSFP-28 */
		rc = qed_phy_sfp_translate_port_to_lane(p_hwfn, p_ptt, port,
							&lane_mask);

		if (rc)
			return qed_sfp_set_error(rc,
						 0,
						 p_phy_result_buf,
						 "nvm cfg indicates PORT_TYPE not module based\n");

		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_TRANSCEIVER_ADDR, 86, 1, buf);
		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading transceiver tx disable status field.\n");
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE, "%d",
			  ((buf[0] & lane_mask) ? 1 : 0));
		break;
	default:
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Unknown transceiver type inserted.\n");
		rc = -EINVAL;
		break;
	}

	return rc;
}

/* Set SFP TX disable */
int qed_phy_sfp_set_txdisable(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt,
			      u32 port, u8 txdisable, char *p_phy_result_buf)
{
	u32 transceiver_state, transceiver_type, lane_mask;
	int rc;
	u32 length = 0;
	u8 buf[4];

	/* Verify <txdisable> field is between 0 and 1 */
	if (txdisable > 1) {
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Bad tx disable value, must be 0 or 1.\n");
		return -EINVAL;
	}

	/* Verify <port> field is between 0 and number of ports */
	rc = qed_validate_sfp_port(p_hwfn, port, p_phy_result_buf);
	if (rc)
		return rc;

	qed_mcp_get_transceiver_data(p_hwfn, p_ptt, &transceiver_state,
				     &transceiver_type);
	if (transceiver_state != ETH_TRANSCEIVER_STATE_PRESENT) {
		return qed_sfp_set_error(-ENODEV, length,
					 p_phy_result_buf,
					 "Transceiver not present\n");
	}

	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  0, 1, buf);
	if (rc)
		return qed_sfp_set_error(rc,
					 length,
					 p_phy_result_buf,
					 "Error reading transceiver identification field.\n");

	switch (buf[0]) {
	case 0x3:		/* SFP, SFP+, SFP-28 */
		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_DEV_ADDR_A0, 93, 1, buf);

		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading transceiver enhanced options status field.\n");

		if ((buf[0] & 0x40) == 0)
			return qed_sfp_set_error(-EINVAL,
						 length,
						 p_phy_result_buf,
						 "Optional soft TX_DISABLE control not supported on this device\n");

		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_DEV_ADDR_A2,
					  SFP_TX_DISABLE_OFFSET, 1, buf);
		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading transceiver tx disable status field.\n");

		if (txdisable)
			buf[0] |= 0x40;
		else
			buf[0] &= ~0x40;

		rc = qed_mcp_phy_sfp_write(p_hwfn, p_ptt, port,
					   I2C_DEV_ADDR_A2,
					   SFP_TX_DISABLE_OFFSET, 1, buf);
		if (rc)
			scnprintf(&p_phy_result_buf[length],
				  MAX_CHAR_PER_LINE,
				  "Error setting transceiver tx disable status field.\n");
		break;
	case 0xc:		/* QSFP */
	case 0xd:		/* QSFP+ */
	case 0x11:		/* QSFP-28 */
		rc = qed_phy_sfp_translate_port_to_lane(p_hwfn, p_ptt, port,
							&lane_mask);

		if (rc)
			return qed_sfp_set_error(rc,
						 0,
						 p_phy_result_buf,
						 "nvm cfg indicates PORT_TYPE not module based\n");

		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_TRANSCEIVER_ADDR,
					  QSFP_TX_DISABLE_OFFSET, 1, buf);
		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading transceiver tx disable status field.\n");

		if (txdisable)
			buf[0] |= lane_mask;
		else
			buf[0] &= ~lane_mask;

		rc = qed_mcp_phy_sfp_write(p_hwfn, p_ptt, port,
					   I2C_TRANSCEIVER_ADDR,
					   QSFP_TX_DISABLE_OFFSET, 1, buf);
		if (rc)
			scnprintf(&p_phy_result_buf[length],
				  MAX_CHAR_PER_LINE,
				  "Error setting transceiver tx disable status field.\n");
		break;
	default:
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Unknown transceiver type inserted.\n");
		rc = -EINVAL;
		break;
	}

	return rc;
}

/* Get SFP TX fault status */
int qed_phy_sfp_get_txreset(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt,
			    u32 port, char *p_phy_result_buf)
{
	u32 transceiver_state, transceiver_type, lane_mask;
	int rc;
	u32 length = 0;
	u8 buf[4];

	/* Verify <port> field is between 0 and number of ports */
	rc = qed_validate_sfp_port(p_hwfn, port, p_phy_result_buf);
	if (rc)
		return rc;

	qed_mcp_get_transceiver_data(p_hwfn, p_ptt, &transceiver_state,
				     &transceiver_type);
	if (transceiver_state != ETH_TRANSCEIVER_STATE_PRESENT)
		return qed_sfp_set_error(-ENODEV, length,
					 p_phy_result_buf,
					 "Transceiver not present\n");

	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  0, 1, buf);
	if (rc)
		return qed_sfp_set_error(rc,
					 length,
					 p_phy_result_buf,
					 "Error reading transceiver identification field.\n");

	switch (buf[0]) {
	case 0x3:		/* SFP, SFP+, SFP-28 */
		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_DEV_ADDR_A0, 93, 1, buf);

		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading transceiver enhanced options status field.\n");

		if ((buf[0] & 0x20) == 0)
			return qed_sfp_set_error(-EINVAL,
						 length,
						 p_phy_result_buf,
						 "Optional soft TX_FAULT monitoring not supported on this device\n");

		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_DEV_ADDR_A2,
					  SFP_TX_FAULT_OFFSET, 1, buf);
		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading transceiver tx fault status field.\n");
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE, "%d",
			  ((buf[0] & 0x04) ? 1 : 0));
		break;
	case 0xc:		/* QSFP */
	case 0xd:		/* QSFP+ */
	case 0x11:		/* QSFP-28 */
		rc = qed_phy_sfp_translate_port_to_lane(p_hwfn, p_ptt, port,
							&lane_mask);

		if (rc)
			return qed_sfp_set_error(rc,
						 0,
						 p_phy_result_buf,
						 "nvm cfg indicates PORT_TYPE not module based\n");

		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_TRANSCEIVER_ADDR,
					  QSFP_TX_FAULT_OFFSET, 1, buf);
		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading transceiver tx fault status field.\n");

		/* read twice to get current state, not latched state */
		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_TRANSCEIVER_ADDR,
					  QSFP_TX_FAULT_OFFSET, 1, buf);
		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading transceiver tx fault status field.\n");
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE, "%d",
			  ((buf[0] & lane_mask) ? 1 : 0));
		break;
	default:
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Unknown transceiver type inserted.\n");
		rc = -EINVAL;
		break;
	}

	return rc;
}

/* Get SFP RX los status */
int qed_phy_sfp_get_rxlos(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt,
			  u32 port, char *p_phy_result_buf)
{
	u32 transceiver_state, transceiver_type, lane_mask;
	int rc;
	u32 length = 0;
	u8 buf[4];

	/* Verify <port> field is between 0 and number of ports */
	rc = qed_validate_sfp_port(p_hwfn, port, p_phy_result_buf);
	if (rc)
		return rc;

	qed_mcp_get_transceiver_data(p_hwfn, p_ptt, &transceiver_state,
				     &transceiver_type);
	if (transceiver_state != ETH_TRANSCEIVER_STATE_PRESENT)
		return qed_sfp_set_error(-ENODEV, length,
					 p_phy_result_buf,
					 "Transceiver not present\n");

	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  0, 1, buf);
	if (rc)
		return qed_sfp_set_error(rc,
					 length,
					 p_phy_result_buf,
					 "Error reading transceiver identification field.\n");

	switch (buf[0]) {
	case 0x3:		/* SFP, SFP+, SFP-28 */
		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_DEV_ADDR_A0, 93, 1, buf);

		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading transceiver enhanced options status field.\n");

		if ((buf[0] & 0x10) == 0)
			return qed_sfp_set_error(-EINVAL,
						 length,
						 p_phy_result_buf,
						 "Optional soft RX_LOS monitoring not supported on this device\n");

		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_DEV_ADDR_A2,
					  SFP_RX_LOS_OFFSET, 1, buf);
		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading transceiver rx los status field.\n");
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE, "%d",
			  ((buf[0] & 0x02) ? 1 : 0));
		break;
	case 0xc:		/* QSFP */
	case 0xd:		/* QSFP+ */
	case 0x11:		/* QSFP-28 */
		rc = qed_phy_sfp_translate_port_to_lane(p_hwfn, p_ptt, port,
							&lane_mask);

		if (rc)
			return qed_sfp_set_error(rc,
						 0,
						 p_phy_result_buf,
						 "nvm cfg indicates PORT_TYPE not module based\n");

		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_TRANSCEIVER_ADDR,
					  QSFP_RX_LOS_OFFSET, 1, buf);
		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading transceiver rx los status field.\n");

		/* read twice to get current state, not latched state */
		rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port,
					  I2C_TRANSCEIVER_ADDR,
					  QSFP_RX_LOS_OFFSET, 1, buf);
		if (rc)
			return qed_sfp_set_error(rc,
						 length,
						 p_phy_result_buf,
						 "Error reading transceiver rx los status field.\n");

		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE, "%d",
			  ((buf[0] & lane_mask) ? 1 : 0));
		break;
	default:
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Unknown transceiver type inserted.\n");
		rc = -EINVAL;
		break;
	}

	return rc;
}

/* Get SFP EEPROM memory dump */
int qed_phy_sfp_get_eeprom(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt,
			   u32 port, char *p_phy_result_buf)
{
	int rc;
	u8 buf[4];

	/* Verify <port> field is between 0 and number of ports */
	rc = qed_validate_sfp_port(p_hwfn, port, p_phy_result_buf);
	if (rc)
		return rc;

	rc = qed_mcp_phy_sfp_read(p_hwfn, p_ptt, port, I2C_TRANSCEIVER_ADDR,
				  0, 1, buf);
	if (rc)
		return qed_sfp_set_error(rc,
					 0,
					 p_phy_result_buf,
					 "Error reading transceiver identification field.\n");

	switch (buf[0]) {
	case 0x3:		/* SFP, SFP+, SFP-28 */
	case 0xc:		/* QSFP */
	case 0xd:		/* QSFP+ */
	case 0x11:		/* QSFP-28 */
		rc = qed_phy_sfp_read(p_hwfn, p_ptt, port,
				      I2C_TRANSCEIVER_ADDR, 0,
				      MAX_I2C_TRANSCEIVER_PAGE_SIZE,
				      p_phy_result_buf);
		break;
	default:
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Unknown transceiver type inserted.\n");
		rc = -EINVAL;
		break;
	}

	return rc;
}

/* Write to gpio */
int qed_phy_gpio_write(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       u16 gpio, u16 gpio_val, char *p_phy_result_buf)
{
	int rc;

	rc = qed_mcp_gpio_write(p_hwfn, p_ptt, gpio, gpio_val);

	if (!rc)
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Written successfully to gpio number %d.\n", gpio);
	else
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Can't write to gpio %d\n", gpio);

	return rc;
}

/* Read from gpio */
int qed_phy_gpio_read(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, u16 gpio, char *p_phy_result_buf)
{
	int rc;
	u32 param;

	rc = qed_mcp_gpio_read(p_hwfn, p_ptt, gpio, &param);

	if (!rc)
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE, "%x", param);
	else
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Can't read from gpio %d\n", gpio);

	return rc;
}

/* Get information from gpio */
int qed_phy_gpio_info(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, u16 gpio, char *p_phy_result_buf)
{
	u32 direction, ctrl, length = 0;
	int rc;

	rc = qed_mcp_gpio_info(p_hwfn, p_ptt, gpio, &direction, &ctrl);

	if (rc) {
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Can't get information for gpio %d\n", gpio);
		return rc;
	}

	length = scnprintf(p_phy_result_buf,
			   MAX_CHAR_PER_LINE,
			   "Gpio %d is %s - ",
			   gpio, ((direction == 0) ? "output" : "input"));
	switch (ctrl) {
	case 0:
		scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			  "control is uninitialized\n");
		break;
	case 1:
		scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			  "control is path 0\n");
		break;
	case 2:
		scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			  "control is path 1\n");
		break;
	case 3:
		scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			  "control is shared\n");
		break;
	default:
		scnprintf(&p_phy_result_buf[length], MAX_CHAR_PER_LINE,
			  "\nError - control is invalid\n");
		break;
	}

	return 0;
}

/* Get information from gpio */
int qed_phy_extphy_read(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt,
			u16 port, u16 devad, u16 reg, char *p_phy_result_buf)
{
	int rc;
	u32 param = 0;
	u32 resp_cmd;
	u32 val;

	if (p_hwfn->cdev->recov_in_prog) {
		DP_ERR(p_hwfn, "Error recovery in progress\n");
		return -EAGAIN;
	}

	SET_MFW_FIELD(param, DRV_MB_PARAM_PORT, port);
	SET_MFW_FIELD(param, DRV_MB_PARAM_DEVAD, devad);
	SET_MFW_FIELD(param, DRV_MB_PARAM_ADDR, reg);
	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_EXT_PHY_READ,
			 param, &resp_cmd, &val);

	if ((rc != 0) || (resp_cmd != FW_MSG_CODE_PHY_OK)) {
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Failed reading external PHY\n");
		return rc;
	}
	scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE, "0x%04x\n", val);
	return 0;
}

/* Get information from gpio */
int qed_phy_extphy_write(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 u16 port,
			 u16 devad, u16 reg, u16 val, char *p_phy_result_buf)
{
	int rc;
	u32 param = 0;
	u32 resp_cmd;
	u32 fw_param;

	if (p_hwfn->cdev->recov_in_prog) {
		DP_ERR(p_hwfn, "Error recovery in progress\n");
		return -EAGAIN;
	}

	SET_MFW_FIELD(param, DRV_MB_PARAM_PORT, port);
	SET_MFW_FIELD(param, DRV_MB_PARAM_DEVAD, devad);
	SET_MFW_FIELD(param, DRV_MB_PARAM_ADDR, reg);
	rc = qed_mcp_nvm_wr_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_EXT_PHY_WRITE,
				param,
				&resp_cmd,
				&fw_param, sizeof(u32), (u32 *) & val, true);

	if ((rc != 0) || (resp_cmd != FW_MSG_CODE_PHY_OK)) {
		scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE,
			  "Failed writing external PHY\n");
		return rc;
	}
	scnprintf(p_phy_result_buf, MAX_CHAR_PER_LINE, "0\n");
	return 0;
}
