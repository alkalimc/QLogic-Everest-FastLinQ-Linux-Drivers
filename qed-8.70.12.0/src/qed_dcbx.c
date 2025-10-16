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
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#define __PREVENT_DUMP_MEM_ARR__
#define __PREVENT_PXP_GLOBAL_WIN__
#include "qed.h"
#include "qed_cxt.h"
#include "qed_dcbx.h"
#include "qed_hsi.h"
#include "qed_iro_hsi.h"
#include "qed_rdma.h"
#include "qed_sp.h"
#include "qed_sriov.h"
static const struct qed_dcbx_app_metadata qed_dcbx_app_update[] = {
	{DCBX_PROTOCOL_ISCSI, "ISCSI", QED_PCI_ISCSI},
	{DCBX_PROTOCOL_FCOE, "FCOE", QED_PCI_FCOE},
	{DCBX_PROTOCOL_ROCE, "ROCE", QED_PCI_ETH_ROCE},
	{DCBX_PROTOCOL_ROCE_V2, "ROCE_V2", QED_PCI_ETH_ROCE},
	{DCBX_PROTOCOL_ETH, "ETH", QED_PCI_ETH},
	{DCBX_PROTOCOL_IWARP, "IWARP", QED_PCI_ETH_IWARP}
};

#if IS_ENABLED(CONFIG_QED_RDMA)
#endif

#define QED_DCBX_MAX_MIB_READ_TRY       (100)
#define QED_ETH_TYPE_DEFAULT            (0)
#define QED_ETH_TYPE_ROCE               (0x8915)
#define QED_UDP_PORT_TYPE_ROCE_V2       (0x12B7)
#define QED_ETH_TYPE_FCOE               (0x8906)
#define QED_TCP_PORT_ISCSI              (0xCBC)

#define QED_DCBX_INVALID_PRIORITY       0xFF

/* Get Traffic Class from priority traffic class table, 4 bits represent
 * the traffic class corresponding to the priority.
 */
#define QED_DCBX_PRIO2TC(prio_tc_tbl, prio) \
	((u32)(prio_tc_tbl >> ((7 - prio) * 4)) & 0xF)

static bool qed_dcbx_app_ethtype(u32 app_info_bitmap)
{
	return ! !(GET_MFW_FIELD(app_info_bitmap, DCBX_APP_SF) ==
		   DCBX_APP_SF_ETHTYPE);
}

static bool qed_dcbx_ieee_app_ethtype(u32 app_info_bitmap)
{
	u8 mfw_val = GET_MFW_FIELD(app_info_bitmap, DCBX_APP_SF_IEEE);

	/* Old MFW */
	if (mfw_val == DCBX_APP_SF_IEEE_RESERVED)
		return qed_dcbx_app_ethtype(app_info_bitmap);

	return ! !(mfw_val == DCBX_APP_SF_IEEE_ETHTYPE);
}

static bool qed_dcbx_app_port(u32 app_info_bitmap)
{
	return ! !(GET_MFW_FIELD(app_info_bitmap, DCBX_APP_SF) ==
		   DCBX_APP_SF_PORT);
}

static bool qed_dcbx_ieee_app_port(u32 app_info_bitmap, u8 type)
{
	u8 mfw_val = GET_MFW_FIELD(app_info_bitmap, DCBX_APP_SF_IEEE);

	/* Old MFW */
	if (mfw_val == DCBX_APP_SF_IEEE_RESERVED)
		return qed_dcbx_app_port(app_info_bitmap);

	return ! !(mfw_val == type || mfw_val == DCBX_APP_SF_IEEE_TCP_UDP_PORT);
}

static bool qed_dcbx_default_tlv(u32 app_info_bitmap, u16 proto_id, bool ieee)
{
	bool ethtype;

	if (ieee)
		ethtype = qed_dcbx_ieee_app_ethtype(app_info_bitmap);
	else
		ethtype = qed_dcbx_app_ethtype(app_info_bitmap);

	return ! !(ethtype && (proto_id == QED_ETH_TYPE_DEFAULT));
}

static bool qed_dcbx_iscsi_tlv(u32 app_info_bitmap, u16 proto_id, bool ieee)
{
	bool port;

	if (ieee)
		port = qed_dcbx_ieee_app_port(app_info_bitmap,
					      DCBX_APP_SF_IEEE_TCP_PORT);
	else
		port = qed_dcbx_app_port(app_info_bitmap);

	return ! !(port && (proto_id == QED_TCP_PORT_ISCSI));
}

static bool qed_dcbx_fcoe_tlv(u32 app_info_bitmap, u16 proto_id, bool ieee)
{
	bool ethtype;

	if (ieee)
		ethtype = qed_dcbx_ieee_app_ethtype(app_info_bitmap);
	else
		ethtype = qed_dcbx_app_ethtype(app_info_bitmap);

	return ! !(ethtype && (proto_id == QED_ETH_TYPE_FCOE));
}

static bool qed_dcbx_roce_tlv(u32 app_info_bitmap, u16 proto_id, bool ieee)
{
	bool ethtype;

	if (ieee)
		ethtype = qed_dcbx_ieee_app_ethtype(app_info_bitmap);
	else
		ethtype = qed_dcbx_app_ethtype(app_info_bitmap);

	return ! !(ethtype && (proto_id == QED_ETH_TYPE_ROCE));
}

static bool qed_dcbx_roce_v2_tlv(u32 app_info_bitmap, u16 proto_id, bool ieee)
{
	bool port;

	if (ieee)
		port = qed_dcbx_ieee_app_port(app_info_bitmap,
					      DCBX_APP_SF_IEEE_UDP_PORT);
	else
		port = qed_dcbx_app_port(app_info_bitmap);

	return ! !(port && (proto_id == QED_UDP_PORT_TYPE_ROCE_V2));
}

static bool qed_dcbx_iwarp_tlv(struct qed_hwfn *p_hwfn,
			       u32 app_info_bitmap, u16 proto_id, bool ieee)
{
	bool port;

	if (!p_hwfn->p_dcbx_info->iwarp_port)
		return false;

	if (ieee)
		port = qed_dcbx_ieee_app_port(app_info_bitmap,
					      DCBX_APP_SF_IEEE_TCP_PORT);
	else
		port = qed_dcbx_app_port(app_info_bitmap);

	return ! !(port && (proto_id == p_hwfn->p_dcbx_info->iwarp_port));
}

static void
qed_dcbx_dp_protocol(struct qed_hwfn *p_hwfn, struct qed_dcbx_results *p_data)
{
	enum dcbx_protocol_type id;
	u32 i;

	DP_VERBOSE(p_hwfn, QED_MSG_DCB, "DCBX negotiated: %d\n",
		   p_data->dcbx_enabled);

	for (i = 0; i < ARRAY_SIZE(qed_dcbx_app_update); i++) {
		id = qed_dcbx_app_update[i].id;

		DP_VERBOSE(p_hwfn,
			   QED_MSG_DCB,
			   "%s info: update %d, enable %d, prio %d, tc %d, num_active_tc %d dscp_enable = %d dscp_val = %d\n",
			   qed_dcbx_app_update[i].name,
			   p_data->arr[id].update,
			   p_data->arr[id].enable,
			   p_data->arr[id].priority,
			   p_data->arr[id].tc,
			   p_hwfn->hw_info.num_active_tc,
			   p_data->arr[id].dscp_enable,
			   p_data->arr[id].dscp_val);
	}
}

u8 qed_dcbx_get_dscp_value(struct qed_hwfn *p_hwfn, u8 pri)
{
	struct qed_dcbx_dscp_params *dscp = &p_hwfn->p_dcbx_info->get.dscp;
	u8 i;

	if (!dscp->enabled)
		return QED_DCBX_DSCP_DISABLED;

	for (i = 0; i < QED_DCBX_DSCP_SIZE; i++)
		if (pri == dscp->dscp_pri_map[i])
			return i;

	return QED_DCBX_DSCP_DISABLED;
}

static void
qed_dcbx_set_params(struct qed_dcbx_results *p_data,
		    struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    bool app_tlv,
		    bool enable,
		    u8 prio,
		    u8 tc,
		    enum dcbx_protocol_type type,
		    enum qed_pci_personality personality)
{
	/* PF update ramrod data */
	p_data->arr[type].enable = enable;
	p_data->arr[type].priority = prio;
	p_data->arr[type].tc = tc;
	p_data->arr[type].dscp_val = qed_dcbx_get_dscp_value(p_hwfn, prio);
	if (p_data->arr[type].dscp_val == QED_DCBX_DSCP_DISABLED) {
		p_data->arr[type].dscp_enable = false;
		p_data->arr[type].dscp_val = 0;
	} else {
		p_data->arr[type].dscp_enable = enable;
	}

	p_data->arr[type].update = UPDATE_DCB_DSCP;

	if (test_bit(QED_MF_DONT_ADD_VLAN0_TAG, &p_hwfn->cdev->mf_bits))
		p_data->arr[type].dont_add_vlan0 = true;

	/* QM reconf data */
	if (app_tlv && p_hwfn->hw_info.personality == personality)
		qed_hw_info_set_offload_tc(&p_hwfn->hw_info, tc);

	/* Configure dcbx vlan priority in doorbell block for roce EDPM */
	if (test_bit(QED_MF_UFP_SPECIFIC, &p_hwfn->cdev->mf_bits) &&
	    (type == DCBX_PROTOCOL_ROCE)) {
		qed_wr(p_hwfn, p_ptt, DORQ_REG_TAG1_OVRD_MODE, 1);
		qed_wr(p_hwfn, p_ptt, DORQ_REG_PF_PCP, prio << 1);
	}
}

/* Update app protocol data and hw_info fields with the TLV info */
static void
qed_dcbx_update_app_info(struct qed_dcbx_results *p_data,
			 struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 bool app_tlv,
			 bool enable,
			 u8 prio, u8 tc, enum dcbx_protocol_type type)
{
	enum qed_pci_personality personality;
	enum dcbx_protocol_type id;
	u32 i;

	for (i = 0; i < ARRAY_SIZE(qed_dcbx_app_update); i++) {
		id = qed_dcbx_app_update[i].id;

		if (type != id)
			continue;

		personality = qed_dcbx_app_update[i].personality;

		qed_dcbx_set_params(p_data, p_hwfn, p_ptt, app_tlv, enable,
				    prio, tc, type, personality);
	}
}

static int qed_dcbx_get_app_priority(u8 pri_bitmap, u8 * priority)
{
	u32 pri_mask, pri = QED_MAX_PFC_PRIORITIES;
	u32 index = QED_MAX_PFC_PRIORITIES - 1;
	int rc = 0;

	/* Bitmap 1 corresponds to priority 0, return priority 0 */
	if (pri_bitmap == 1) {
		*priority = 0;
		return rc;
	}

	/* Choose the highest priority */
	while ((QED_MAX_PFC_PRIORITIES == pri) && index) {
		pri_mask = 1 << index;
		if (pri_bitmap & pri_mask)
			pri = index;
		index--;
	}

	if (pri < QED_MAX_PFC_PRIORITIES)
		*priority = (u8) pri;
	else
		rc = -EINVAL;

	return rc;
}

static bool
qed_dcbx_get_app_protocol_type(struct qed_hwfn *p_hwfn,
			       u32 app_prio_bitmap,
			       u16 id, enum dcbx_protocol_type *type, bool ieee)
{
	if (qed_dcbx_fcoe_tlv(app_prio_bitmap, id, ieee)) {
		*type = DCBX_PROTOCOL_FCOE;
	} else if (qed_dcbx_roce_tlv(app_prio_bitmap, id, ieee)) {
		*type = DCBX_PROTOCOL_ROCE;
	} else if (qed_dcbx_iscsi_tlv(app_prio_bitmap, id, ieee)) {
		*type = DCBX_PROTOCOL_ISCSI;
	} else if (qed_dcbx_default_tlv(app_prio_bitmap, id, ieee)) {
		*type = DCBX_PROTOCOL_ETH;
	} else if (qed_dcbx_roce_v2_tlv(app_prio_bitmap, id, ieee)) {
		*type = DCBX_PROTOCOL_ROCE_V2;
	} else if (qed_dcbx_iwarp_tlv(p_hwfn, app_prio_bitmap, id, ieee)) {
		*type = DCBX_PROTOCOL_IWARP;
	} else {
		*type = DCBX_MAX_PROTOCOL_TYPE;
		DP_VERBOSE(p_hwfn, QED_MSG_DCB,
			   "No action required, App TLV entry = 0x%x\n",
			   app_prio_bitmap);
		return false;
	}

	return true;
}

/* Parse app TLV's to update TC information in hw_info structure for
 * reconfiguring QM. Get protocol specific data for PF update ramrod command.
 */
static int
qed_dcbx_process_tlv(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt,
		     struct qed_dcbx_results *p_data,
		     struct dcbx_app_priority_entry *p_tbl,
		     u32 pri_tc_tbl, int count, u8 dcbx_version)
{
	enum dcbx_protocol_type type;
	bool enable, ieee, eth_tlv;
	u8 tc, priority_map;
	u16 protocol_id;
	u8 priority = 0;
	int rc = 0;
	int i;

	DP_VERBOSE(p_hwfn, QED_MSG_DCB,
		   "Num APP entries = %d pri_tc_tbl = 0x%x dcbx_version = %u\n",
		   count, pri_tc_tbl, dcbx_version);

	ieee = ((dcbx_version == DCBX_CONFIG_VERSION_IEEE) ||
		((dcbx_version == DCBX_CONFIG_VERSION_DISABLED) &&
		 p_hwfn->p_dcbx_info->is_admin_cfg_applied));

	eth_tlv = false;
	/* Parse APP TLV */
	for (i = 0; i < count; i++) {
		protocol_id = GET_MFW_FIELD(p_tbl[i].entry,
					    DCBX_APP_PROTOCOL_ID);
		priority_map = GET_MFW_FIELD(p_tbl[i].entry, DCBX_APP_PRI_MAP);
		DP_VERBOSE(p_hwfn, QED_MSG_DCB, "Id = 0x%x pri_map = %u\n",
			   protocol_id, priority_map);
		rc = qed_dcbx_get_app_priority(priority_map, &priority);
		if (rc == -EINVAL)
			DP_ERR(p_hwfn,
			       "Invalid priority Id = 0x%x pri_map = %u\n",
			       protocol_id, priority_map);

		tc = QED_DCBX_PRIO2TC(pri_tc_tbl, priority);
		if (qed_dcbx_get_app_protocol_type(p_hwfn, p_tbl[i].entry,
						   protocol_id, &type, ieee)) {
			/* ETH always have the enable bit reset, as it gets
			 * vlan information per packet. For other protocols,
			 * should be set according to the dcbx_enabled
			 * indication, but we only got here if there was an
			 * app tlv for the protocol, so dcbx must be enabled.
			 */
			if (type == DCBX_PROTOCOL_ETH) {
				enable = false;
				eth_tlv = true;
			} else {
				enable = true;
			}

			qed_dcbx_update_app_info(p_data, p_hwfn, p_ptt, true,
						 enable, priority, tc, type);
		}
	}

	/* If Eth TLV is not detected, use UFP TC as default TC */
	if (test_bit(QED_MF_UFP_SPECIFIC, &p_hwfn->cdev->mf_bits) && !eth_tlv)
		p_data->arr[DCBX_PROTOCOL_ETH].tc = p_hwfn->ufp_info.tc;

	/* Update ramrod protocol data and hw_info fields
	 * with default info when corresponding APP TLV's are not detected.
	 * The enabled field has a different logic for ethernet as only for
	 * ethernet dcb should disabled by default, as the information arrives
	 * from the OS (unless an explicit app tlv was present).
	 */
	tc = p_data->arr[DCBX_PROTOCOL_ETH].tc;
	priority = p_data->arr[DCBX_PROTOCOL_ETH].priority;
	for (type = 0; type < DCBX_MAX_PROTOCOL_TYPE; type++) {
		if (p_data->arr[type].update)
			continue;

		/* if no app tlv was present, don't override in FW */
		qed_dcbx_update_app_info(p_data, p_hwfn, p_ptt, false,
					 p_data->arr[DCBX_PROTOCOL_ETH].enable,
					 priority, tc, type);
	}

	return 0;
}

/* Parse app TLV's to update TC information in hw_info structure for
 * reconfiguring QM. Get protocol specific data for PF update ramrod command.
 */
static int
qed_dcbx_process_mib_info(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct dcbx_app_priority_feature *p_app;
	struct dcbx_app_priority_entry *p_tbl;
	struct qed_dcbx_results data;
	struct dcbx_ets_feature *p_ets;
	struct qed_hw_info *p_info;
	u32 pri_tc_tbl, flags;
	u8 dcbx_version;
	int num_entries;
	int rc = 0;

	flags = p_hwfn->p_dcbx_info->operational.flags;
	dcbx_version = GET_MFW_FIELD(flags, DCBX_CONFIG_VERSION);

	p_app = &p_hwfn->p_dcbx_info->operational.features.app;
	p_tbl = p_app->app_pri_tbl;

	p_ets = &p_hwfn->p_dcbx_info->operational.features.ets;
	pri_tc_tbl = p_ets->pri_tc_tbl[0];

	p_info = &p_hwfn->hw_info;
	num_entries = GET_MFW_FIELD(p_app->flags, DCBX_APP_NUM_ENTRIES);

	memset(&data, 0, sizeof(struct qed_dcbx_results));
	rc = qed_dcbx_process_tlv(p_hwfn, p_ptt, &data, p_tbl, pri_tc_tbl,
				  num_entries, dcbx_version);
	if (rc)
		return rc;

	p_info->num_active_tc = GET_MFW_FIELD(p_ets->flags, DCBX_ETS_MAX_TCS);
	p_hwfn->qm_info.ooo_tc = GET_MFW_FIELD(p_ets->flags, DCBX_OOO_TC);
	data.pf_id = p_hwfn->rel_pf_id;
	data.dcbx_enabled = ! !dcbx_version;

	qed_dcbx_dp_protocol(p_hwfn, &data);

	memcpy(&p_hwfn->p_dcbx_info->results, &data,
	       sizeof(struct qed_dcbx_results));

	return 0;
}

static int
qed_dcbx_copy_mib(struct qed_hwfn *p_hwfn,
		  struct qed_ptt *p_ptt,
		  struct qed_dcbx_mib_meta_data *p_data,
		  enum qed_mib_read_type type)
{
	u32 prefix_seq_num, suffix_seq_num;
	int read_count = 0;
	int rc = 0;

	/* The data is considered to be valid only if both sequence numbers are
	 * the same.
	 */
	do {
		if (type == QED_DCBX_REMOTE_LLDP_MIB) {
			qed_memcpy_from(p_hwfn, p_ptt, p_data->lldp_remote,
					p_data->addr, p_data->size);
			prefix_seq_num = p_data->lldp_remote->prefix_seq_num;
			suffix_seq_num = p_data->lldp_remote->suffix_seq_num;
		} else if (type == QED_DCBX_LLDP_TLVS) {
			qed_memcpy_from(p_hwfn, p_ptt, p_data->lldp_tlvs,
					p_data->addr, p_data->size);
			prefix_seq_num = p_data->lldp_tlvs->prefix_seq_num;
			suffix_seq_num = p_data->lldp_tlvs->suffix_seq_num;
		} else {
			qed_memcpy_from(p_hwfn, p_ptt, p_data->mib,
					p_data->addr, p_data->size);
			prefix_seq_num = p_data->mib->prefix_seq_num;
			suffix_seq_num = p_data->mib->suffix_seq_num;
		}
		read_count++;

		DP_VERBOSE(p_hwfn,
			   QED_MSG_DCB,
			   "mib type = %d, try count = %d prefix seq num  = %d suffix seq num = %d\n",
			   type, read_count, prefix_seq_num, suffix_seq_num);
	} while ((prefix_seq_num != suffix_seq_num) &&
		 (read_count < QED_DCBX_MAX_MIB_READ_TRY));

	if (read_count >= QED_DCBX_MAX_MIB_READ_TRY) {
		DP_ERR(p_hwfn,
		       "MIB read err, mib type = %d, try count = %d prefix seq num = %d suffix seq num = %d\n",
		       type, read_count, prefix_seq_num, suffix_seq_num);
		rc = -EIO;
	}

	return rc;
}

static void
qed_dcbx_get_priority_info(struct qed_hwfn *p_hwfn,
			   struct qed_dcbx_app_prio *p_prio,
			   struct qed_dcbx_results *p_results)
{
	u8 val;

	p_prio->roce = QED_DCBX_INVALID_PRIORITY;
	p_prio->roce_v2 = QED_DCBX_INVALID_PRIORITY;
	p_prio->iscsi = QED_DCBX_INVALID_PRIORITY;
	p_prio->fcoe = QED_DCBX_INVALID_PRIORITY;

	if (p_results->arr[DCBX_PROTOCOL_ROCE].update &&
	    p_results->arr[DCBX_PROTOCOL_ROCE].enable)
		p_prio->roce = p_results->arr[DCBX_PROTOCOL_ROCE].priority;

	if (p_results->arr[DCBX_PROTOCOL_ROCE_V2].update &&
	    p_results->arr[DCBX_PROTOCOL_ROCE_V2].enable) {
		val = p_results->arr[DCBX_PROTOCOL_ROCE_V2].priority;
		p_prio->roce_v2 = val;
	}

	if (p_results->arr[DCBX_PROTOCOL_ISCSI].update &&
	    p_results->arr[DCBX_PROTOCOL_ISCSI].enable)
		p_prio->iscsi = p_results->arr[DCBX_PROTOCOL_ISCSI].priority;

	if (p_results->arr[DCBX_PROTOCOL_FCOE].update &&
	    p_results->arr[DCBX_PROTOCOL_FCOE].enable)
		p_prio->fcoe = p_results->arr[DCBX_PROTOCOL_FCOE].priority;

	if (p_results->arr[DCBX_PROTOCOL_ETH].update &&
	    p_results->arr[DCBX_PROTOCOL_ETH].enable)
		p_prio->eth = p_results->arr[DCBX_PROTOCOL_ETH].priority;

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DCB,
		   "Priorities: iscsi %d, roce %d, roce v2 %d, fcoe %d, eth %d\n",
		   p_prio->iscsi,
		   p_prio->roce, p_prio->roce_v2, p_prio->fcoe, p_prio->eth);
}

static void
qed_dcbx_get_app_data(struct qed_hwfn *p_hwfn,
		      struct dcbx_app_priority_feature *p_app,
		      struct dcbx_app_priority_entry *p_tbl,
		      struct qed_dcbx_params *p_params, bool ieee)
{
	int rc = 0;
	struct qed_app_entry *entry;
	u8 pri_map;
	int i;

	p_params->app_willing = GET_MFW_FIELD(p_app->flags, DCBX_APP_WILLING);
	p_params->app_valid = GET_MFW_FIELD(p_app->flags, DCBX_APP_ENABLED);
	p_params->app_error = GET_MFW_FIELD(p_app->flags, DCBX_APP_ERROR);
	p_params->num_app_entries = GET_MFW_FIELD(p_app->flags,
						  DCBX_APP_NUM_ENTRIES);
	if (p_params->num_app_entries > QED_DCBX_MAX_APP_PROTOCOL)
		p_params->num_app_entries = QED_DCBX_MAX_APP_PROTOCOL;
	for (i = 0; i < p_params->num_app_entries; i++) {
		entry = &p_params->app_entry[i];
		if (ieee) {
			u8 sf_ieee;
			u32 val;

			sf_ieee = GET_MFW_FIELD(p_tbl[i].entry,
						DCBX_APP_SF_IEEE);
			switch (sf_ieee) {
			case DCBX_APP_SF_IEEE_RESERVED:
				/* Old MFW */
				val = GET_MFW_FIELD(p_tbl[i].entry,
						    DCBX_APP_SF);
				entry->sf_ieee = val ?
				    QED_DCBX_SF_IEEE_TCP_UDP_PORT :
				    QED_DCBX_SF_IEEE_ETHTYPE;
				break;
			case DCBX_APP_SF_IEEE_ETHTYPE:
				entry->sf_ieee = QED_DCBX_SF_IEEE_ETHTYPE;
				break;
			case DCBX_APP_SF_IEEE_TCP_PORT:
				entry->sf_ieee = QED_DCBX_SF_IEEE_TCP_PORT;
				break;
			case DCBX_APP_SF_IEEE_UDP_PORT:
				entry->sf_ieee = QED_DCBX_SF_IEEE_UDP_PORT;
				break;
			case DCBX_APP_SF_IEEE_TCP_UDP_PORT:
				entry->sf_ieee = QED_DCBX_SF_IEEE_TCP_UDP_PORT;
				break;
			}
		} else {
			entry->ethtype = !(GET_MFW_FIELD(p_tbl[i].entry,
							 DCBX_APP_SF));
		}

		pri_map = GET_MFW_FIELD(p_tbl[i].entry, DCBX_APP_PRI_MAP);
		rc = qed_dcbx_get_app_priority(pri_map, &entry->prio);
		if (rc == -EINVAL)
			DP_ERR(p_hwfn,
			       "Invalid priority pri_map = %u\n", pri_map);

		entry->proto_id = GET_MFW_FIELD(p_tbl[i].entry,
						DCBX_APP_PROTOCOL_ID);
		qed_dcbx_get_app_protocol_type(p_hwfn, p_tbl[i].entry,
					       entry->proto_id,
					       &entry->proto_type, ieee);
	}

	DP_VERBOSE(p_hwfn, QED_MSG_DCB,
		   "APP params: willing %d, valid %d error = %d\n",
		   p_params->app_willing, p_params->app_valid,
		   p_params->app_error);
}

static void
qed_dcbx_get_pfc_data(struct qed_hwfn *p_hwfn,
		      u32 pfc, struct qed_dcbx_params *p_params)
{
	u8 pfc_map;

	p_params->pfc.willing = GET_MFW_FIELD(pfc, DCBX_PFC_WILLING);
	p_params->pfc.max_tc = GET_MFW_FIELD(pfc, DCBX_PFC_CAPS);
	p_params->pfc.mbc = GET_MFW_FIELD(pfc, DCBX_PFC_MBC);
	p_params->pfc.enabled = GET_MFW_FIELD(pfc, DCBX_PFC_ENABLED);
	pfc_map = GET_MFW_FIELD(pfc, DCBX_PFC_PRI_EN_BITMAP);
	p_params->pfc.prio[0] = ! !(pfc_map & DCBX_PFC_PRI_EN_BITMAP_PRI_0);
	p_params->pfc.prio[1] = ! !(pfc_map & DCBX_PFC_PRI_EN_BITMAP_PRI_1);
	p_params->pfc.prio[2] = ! !(pfc_map & DCBX_PFC_PRI_EN_BITMAP_PRI_2);
	p_params->pfc.prio[3] = ! !(pfc_map & DCBX_PFC_PRI_EN_BITMAP_PRI_3);
	p_params->pfc.prio[4] = ! !(pfc_map & DCBX_PFC_PRI_EN_BITMAP_PRI_4);
	p_params->pfc.prio[5] = ! !(pfc_map & DCBX_PFC_PRI_EN_BITMAP_PRI_5);
	p_params->pfc.prio[6] = ! !(pfc_map & DCBX_PFC_PRI_EN_BITMAP_PRI_6);
	p_params->pfc.prio[7] = ! !(pfc_map & DCBX_PFC_PRI_EN_BITMAP_PRI_7);

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DCB,
		   "PFC params: willing %d, pfc_bitmap %u max_tc = %u enabled = %d mbc = %d\n",
		   p_params->pfc.willing,
		   pfc_map,
		   p_params->pfc.max_tc,
		   p_params->pfc.enabled, p_params->pfc.mbc);
}

static void
qed_dcbx_get_ets_data(struct qed_hwfn *p_hwfn,
		      struct dcbx_ets_feature *p_ets,
		      struct qed_dcbx_params *p_params)
{
	u32 bw_map[2], tsa_map[2], pri_map;
	int i;

	p_params->ets_willing = GET_MFW_FIELD(p_ets->flags, DCBX_ETS_WILLING);
	p_params->ets_enabled = GET_MFW_FIELD(p_ets->flags, DCBX_ETS_ENABLED);
	p_params->ets_cbs = GET_MFW_FIELD(p_ets->flags, DCBX_ETS_CBS);
	p_params->max_ets_tc = GET_MFW_FIELD(p_ets->flags, DCBX_ETS_MAX_TCS);
	DP_VERBOSE(p_hwfn,
		   QED_MSG_DCB,
		   "ETS params: willing %d, enabled = %d ets_cbs %d pri_tc_tbl_0 %x max_ets_tc %d\n",
		   p_params->ets_willing,
		   p_params->ets_enabled,
		   p_params->ets_cbs,
		   p_ets->pri_tc_tbl[0], p_params->max_ets_tc);
	if (p_params->ets_enabled && !p_params->max_ets_tc) {
		p_params->max_ets_tc = QED_MAX_PFC_PRIORITIES;
		DP_VERBOSE(p_hwfn, QED_MSG_DCB,
			   "ETS params: max_ets_tc is forced to %d\n",
			   p_params->max_ets_tc);
	}
	/* 8 bit tsa and bw data corresponding to each of the 8 TC's are
	 * encoded in a type u32 array of size 2.
	 */
	bw_map[0] = be32_to_cpu(p_ets->tc_bw_tbl[0]);
	bw_map[1] = be32_to_cpu(p_ets->tc_bw_tbl[1]);
	tsa_map[0] = be32_to_cpu(p_ets->tc_tsa_tbl[0]);
	tsa_map[1] = be32_to_cpu(p_ets->tc_tsa_tbl[1]);
	pri_map = p_ets->pri_tc_tbl[0];
	for (i = 0; i < QED_MAX_PFC_PRIORITIES; i++) {
		p_params->ets_tc_bw_tbl[i] = ((u8 *) bw_map)[i];
		p_params->ets_tc_tsa_tbl[i] = ((u8 *) tsa_map)[i];
		p_params->ets_pri_tc_tbl[i] = QED_DCBX_PRIO2TC(pri_map, i);
		DP_VERBOSE(p_hwfn, QED_MSG_DCB,
			   "elem %d  bw_tbl %x tsa_tbl %x\n",
			   i, p_params->ets_tc_bw_tbl[i],
			   p_params->ets_tc_tsa_tbl[i]);
	}
}

static void
qed_dcbx_get_common_params(struct qed_hwfn *p_hwfn,
			   struct dcbx_app_priority_feature *p_app,
			   struct dcbx_app_priority_entry *p_tbl,
			   struct dcbx_ets_feature *p_ets,
			   u32 pfc, struct qed_dcbx_params *p_params, bool ieee)
{
	qed_dcbx_get_app_data(p_hwfn, p_app, p_tbl, p_params, ieee);
	qed_dcbx_get_ets_data(p_hwfn, p_ets, p_params);
	qed_dcbx_get_pfc_data(p_hwfn, pfc, p_params);
}

static void
qed_dcbx_get_local_params(struct qed_hwfn *p_hwfn, struct qed_dcbx_get *params)
{
	struct dcbx_features *p_feat;

	p_feat = &p_hwfn->p_dcbx_info->local_admin.features;
	qed_dcbx_get_common_params(p_hwfn, &p_feat->app,
				   p_feat->app.app_pri_tbl, &p_feat->ets,
				   p_feat->pfc, &params->local.params, false);
	params->local.valid = true;
}

static void
qed_dcbx_get_remote_params(struct qed_hwfn *p_hwfn, struct qed_dcbx_get *params)
{
	struct dcbx_features *p_feat;

	p_feat = &p_hwfn->p_dcbx_info->remote.features;
	qed_dcbx_get_common_params(p_hwfn, &p_feat->app,
				   p_feat->app.app_pri_tbl, &p_feat->ets,
				   p_feat->pfc, &params->remote.params, false);
	params->remote.valid = true;
}

static void qed_dcbx_get_dscp_params(struct qed_hwfn *p_hwfn,
				     struct qed_dcbx_get *params)
{
	struct qed_dcbx_dscp_params *p_dscp;
	struct dcb_dscp_map *p_dscp_map;
	int i, j, entry;
	u32 pri_map;

	p_dscp = &params->dscp;
	p_dscp_map = &p_hwfn->p_dcbx_info->dscp_map;
	p_dscp->enabled = GET_MFW_FIELD(p_dscp_map->flags, DCB_DSCP_ENABLE);

	/* MFW encodes 64 dscp entries into 8 element array of u32 entries,
	 * where each entry holds the 4bit priority map for 8 dscp entries.
	 */
	for (i = 0, entry = 0; i < QED_DCBX_DSCP_SIZE / 8; i++) {
		pri_map = p_dscp_map->dscp_pri_map[i];
		DP_VERBOSE(p_hwfn, QED_MSG_DCB, "elem %d pri_map 0x%x\n",
			   entry, pri_map);
		for (j = 0; j < QED_DCBX_DSCP_SIZE / 8; j++, entry++)
			p_dscp->dscp_pri_map[entry] = (u32) (pri_map >>
							     (j * 4)) & 0xf;
	}
}

static void
qed_dcbx_get_operational_params(struct qed_hwfn *p_hwfn,
				struct qed_dcbx_get *params)
{
	struct qed_dcbx_operational_params *p_operational;
	struct qed_dcbx_results *p_results;
	struct dcbx_features *p_feat;
	bool enabled, err;
	u32 flags;
	bool val;

	flags = p_hwfn->p_dcbx_info->operational.flags;

	/* If DCBx version is non zero, then negotiation
	 * was successfuly performed
	 */
	p_operational = &params->operational;
	enabled = ! !(GET_MFW_FIELD(flags, DCBX_CONFIG_VERSION) !=
		      DCBX_CONFIG_VERSION_DISABLED);

	p_feat = &p_hwfn->p_dcbx_info->operational.features;
	p_results = &p_hwfn->p_dcbx_info->results;

	val = ! !(GET_MFW_FIELD(flags, DCBX_CONFIG_VERSION) ==
		  DCBX_CONFIG_VERSION_IEEE);
	p_operational->ieee = val;

	val = ! !(GET_MFW_FIELD(flags, DCBX_CONFIG_VERSION) ==
		  DCBX_CONFIG_VERSION_CEE);
	p_operational->cee = val;

	val = ! !(GET_MFW_FIELD(flags, DCBX_CONFIG_VERSION) ==
		  DCBX_CONFIG_VERSION_STATIC);
	p_operational->local = val;

	DP_VERBOSE(p_hwfn, QED_MSG_DCB,
		   "Version support: ieee %d, cee %d, static %d\n",
		   p_operational->ieee, p_operational->cee,
		   p_operational->local);

	qed_dcbx_get_common_params(p_hwfn, &p_feat->app,
				   p_feat->app.app_pri_tbl, &p_feat->ets,
				   p_feat->pfc, &params->operational.params,
				   p_operational->ieee);
	qed_dcbx_get_priority_info(p_hwfn, &p_operational->app_prio, p_results);
	err = GET_MFW_FIELD(p_feat->app.flags, DCBX_APP_ERROR);
	p_operational->err = err;
	p_operational->enabled = enabled;
	p_operational->valid = true;
}

static void qed_dcbx_get_local_lldp_params(struct qed_hwfn *p_hwfn,
					   struct qed_dcbx_get *params)
{
	struct lldp_config_params_s *p_local;

	p_local = &p_hwfn->p_dcbx_info->lldp_local[LLDP_NEAREST_BRIDGE];

	memcpy(params->lldp_local.local_chassis_id,
	       p_local->local_chassis_id,
	       sizeof(params->lldp_local.local_chassis_id));
	memcpy(params->lldp_local.local_port_id, p_local->local_port_id,
	       sizeof(params->lldp_local.local_port_id));
}

static void qed_dcbx_get_remote_lldp_params(struct qed_hwfn *p_hwfn,
					    struct qed_dcbx_get *params)
{
	struct lldp_status_params_s *p_remote;

	p_remote = &p_hwfn->p_dcbx_info->lldp_remote[LLDP_NEAREST_BRIDGE];

	memcpy(params->lldp_remote.peer_chassis_id,
	       p_remote->peer_chassis_id,
	       sizeof(params->lldp_remote.peer_chassis_id));
	memcpy(params->lldp_remote.peer_port_id, p_remote->peer_port_id,
	       sizeof(params->lldp_remote.peer_port_id));
}

static int
qed_dcbx_get_params(struct qed_hwfn *p_hwfn,
		    struct qed_dcbx_get *p_params, enum qed_mib_read_type type)
{
	switch (type) {
	case QED_DCBX_REMOTE_MIB:
		qed_dcbx_get_remote_params(p_hwfn, p_params);
		break;
	case QED_DCBX_LOCAL_MIB:
		qed_dcbx_get_local_params(p_hwfn, p_params);
		break;
	case QED_DCBX_OPERATIONAL_MIB:
		qed_dcbx_get_operational_params(p_hwfn, p_params);
		break;
	case QED_DCBX_REMOTE_LLDP_MIB:
		qed_dcbx_get_remote_lldp_params(p_hwfn, p_params);
		break;
	case QED_DCBX_LOCAL_LLDP_MIB:
		qed_dcbx_get_local_lldp_params(p_hwfn, p_params);
		break;
	default:
		DP_ERR(p_hwfn, "MIB read err, unknown mib type %d\n", type);
		return -EINVAL;
	}

	return 0;
}

static int
qed_dcbx_read_local_lldp_mib(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_dcbx_mib_meta_data data;
	int rc = 0;

	memset(&data, 0, sizeof(data));
	data.addr = p_hwfn->mcp_info->port_addr + offsetof(struct public_port,
							   lldp_config_params);
	data.lldp_local = p_hwfn->p_dcbx_info->lldp_local;
	data.size = sizeof(struct lldp_config_params_s);
	qed_memcpy_from(p_hwfn, p_ptt, data.lldp_local, data.addr, data.size);

	return rc;
}

static int
qed_dcbx_read_remote_lldp_mib(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt,
			      enum qed_mib_read_type type)
{
	struct qed_dcbx_mib_meta_data data;
	int rc = 0;

	memset(&data, 0, sizeof(data));
	data.addr = p_hwfn->mcp_info->port_addr + offsetof(struct public_port,
							   lldp_status_params);
	data.lldp_remote = p_hwfn->p_dcbx_info->lldp_remote;
	data.size = sizeof(struct lldp_status_params_s);
	rc = qed_dcbx_copy_mib(p_hwfn, p_ptt, &data, type);

	return rc;
}

static int
qed_dcbx_read_operational_mib(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt,
			      enum qed_mib_read_type type)
{
	struct qed_dcbx_mib_meta_data data;
	int rc = 0;

	memset(&data, 0, sizeof(data));
	data.addr = p_hwfn->mcp_info->port_addr +
	    offsetof(struct public_port, operational_dcbx_mib);
	data.mib = &p_hwfn->p_dcbx_info->operational;
	data.size = sizeof(struct dcbx_mib);
	rc = qed_dcbx_copy_mib(p_hwfn, p_ptt, &data, type);

	return rc;
}

static int
qed_dcbx_read_remote_mib(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt, enum qed_mib_read_type type)
{
	struct qed_dcbx_mib_meta_data data;
	int rc = 0;

	memset(&data, 0, sizeof(data));
	data.addr = p_hwfn->mcp_info->port_addr +
	    offsetof(struct public_port, remote_dcbx_mib);
	data.mib = &p_hwfn->p_dcbx_info->remote;
	data.size = sizeof(struct dcbx_mib);
	rc = qed_dcbx_copy_mib(p_hwfn, p_ptt, &data, type);

	return rc;
}

static int
qed_dcbx_read_local_mib(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_dcbx_mib_meta_data data;
	int rc = 0;

	memset(&data, 0, sizeof(data));
	data.addr = p_hwfn->mcp_info->port_addr +
	    offsetof(struct public_port, local_admin_dcbx_mib);
	data.local_admin = &p_hwfn->p_dcbx_info->local_admin;
	data.size = sizeof(struct dcbx_local_params);
	qed_memcpy_from(p_hwfn, p_ptt, data.local_admin, data.addr, data.size);

	return rc;
}

static void
qed_dcbx_read_dscp_mib(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_dcbx_mib_meta_data data;

	data.addr = p_hwfn->mcp_info->port_addr +
	    offsetof(struct public_port, dcb_dscp_map);
	data.dscp_map = &p_hwfn->p_dcbx_info->dscp_map;
	data.size = sizeof(struct dcb_dscp_map);
	qed_memcpy_from(p_hwfn, p_ptt, data.dscp_map, data.addr, data.size);
}

static int qed_dcbx_read_mib(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, enum qed_mib_read_type type)
{
	int rc = -EINVAL;

	switch (type) {
	case QED_DCBX_OPERATIONAL_MIB:
		qed_dcbx_read_dscp_mib(p_hwfn, p_ptt);
		rc = qed_dcbx_read_operational_mib(p_hwfn, p_ptt, type);
		break;
	case QED_DCBX_REMOTE_MIB:
		rc = qed_dcbx_read_remote_mib(p_hwfn, p_ptt, type);
		break;
	case QED_DCBX_LOCAL_MIB:
		rc = qed_dcbx_read_local_mib(p_hwfn, p_ptt);
		break;
	case QED_DCBX_REMOTE_LLDP_MIB:
		rc = qed_dcbx_read_remote_lldp_mib(p_hwfn, p_ptt, type);
		break;
	case QED_DCBX_LOCAL_LLDP_MIB:
		rc = qed_dcbx_read_local_lldp_mib(p_hwfn, p_ptt);
		break;
	default:
		DP_ERR(p_hwfn, "MIB read err, unknown mib type %d\n", type);
	}

	return rc;
}

static int
qed_dcbx_dscp_map_enable(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt, bool b_en)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	u8 ppfid, abs_ppfid, pfid;
	u32 addr, val;
	u16 fid;
	int rc;

	if (!test_bit(QED_MF_DSCP_TO_TC_MAP, &cdev->mf_bits))
		return -EINVAL;

	addr = NIG_REG_DSCP_TO_TC_MAP_ENABLE;
	val = b_en ? 0x1 : 0x0;

	if (!QED_IS_AH(cdev))
		return qed_all_ppfids_wr(p_hwfn, p_ptt, addr, val);

	/* Workaround for a HW bug (only AH is affected):
	 * Instead of writing to "NIG_REG_DSCP_TO_TC_MAP_ENABLE[ppfid]", write
	 * to "NIG_REG_DSCP_TO_TC_MAP_ENABLE[n]", where "n" is the "pfid" which
	 * is read from "NIG_REG_LLH_PPFID2PFID_TBL[ppfid]".
	 */
	for (ppfid = 0; ppfid < qed_llh_get_num_ppfid(cdev); ppfid++) {
		rc = qed_abs_ppfid(cdev, ppfid, &abs_ppfid);
		if (rc)
			return rc;

		/* Cannot just take "rel_pf_id" since a ppfid could have been
		 * loaned to another pf (e.g. RDMA bonding).
		 */
		pfid = (u8) qed_rd(p_hwfn, p_ptt,
				   NIG_REG_LLH_PPFID2PFID_TBL_0 +
				   abs_ppfid * 0x4);

		fid = FIELD_VALUE(PXP_PRETEND_CONCRETE_FID_PFID, pfid);
		qed_fid_pretend(p_hwfn, p_ptt, fid);

		qed_wr(p_hwfn, p_ptt, addr, val);

		fid = FIELD_VALUE(PXP_PRETEND_CONCRETE_FID_PFID,
				  p_hwfn->rel_pf_id);
		qed_fid_pretend(p_hwfn, p_ptt, fid);
	}

	return 0;
}

/*
 * Read updated MIB.
 * Reconfigure QM and invoke PF update ramrod command if operational MIB
 * change is detected.
 */
int
qed_dcbx_mib_update_event(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, enum qed_mib_read_type type)
{
	int rc = 0;

	rc = qed_dcbx_read_mib(p_hwfn, p_ptt, type);
	if (rc)
		goto dcbx_err;

	DP_VERBOSE(p_hwfn, QED_MSG_DCB,
		   "MFW DCBx: MIB Type:%d   Admin Notification:%d\n",
		   type, p_hwfn->p_dcbx_info->is_admin_cfg_applied);

	if (type == QED_DCBX_OPERATIONAL_MIB) {
		qed_dcbx_get_dscp_params(p_hwfn, &p_hwfn->p_dcbx_info->get);

		rc = qed_dcbx_process_mib_info(p_hwfn, p_ptt);
		if (!rc) {
			/* reconfigure tcs of QM queues according
			 * to negotiation results
			 */
			qed_qm_reconf_intr(p_hwfn, p_ptt);

			/* update storm FW with negotiation results */
			qed_sp_pf_update_dcbx(p_hwfn);

#if IS_ENABLED(CONFIG_QED_RDMA)
			/* for roce PFs, we may want to enable/disable DPM
			 * when DCBx change occurs
			 */
			if (QED_IS_ROCE_PERSONALITY(p_hwfn))
				qed_roce_dpm_dcbx(p_hwfn, p_ptt);
#endif
		}
	}

	qed_dcbx_get_params(p_hwfn, &p_hwfn->p_dcbx_info->get, type);

	if (type == QED_DCBX_OPERATIONAL_MIB) {
		struct qed_dcbx_results *p_data;
		u16 val;

		/* Enable/disable the DSCP to TC mapping if required */
		if (p_hwfn->p_dcbx_info->dscp_nig_update) {
			bool b_en = p_hwfn->p_dcbx_info->get.dscp.enabled;

			rc = qed_dcbx_dscp_map_enable(p_hwfn, p_ptt, b_en);
			if (rc) {
				DP_NOTICE(p_hwfn,
					  "Failed to update the DSCP to TC mapping enable bit\n");
				return rc;
			}

			p_hwfn->p_dcbx_info->dscp_nig_update = false;
		}

		/* Configure in NIG which protocols support EDPM and should
		 * honor PFC.
		 */
		p_data = &p_hwfn->p_dcbx_info->results;
		val = (0x1 << p_data->arr[DCBX_PROTOCOL_ROCE].tc) |
		    (0x1 << p_data->arr[DCBX_PROTOCOL_ROCE_V2].tc);
		val <<= NIG_REG_TX_EDPM_CTRL_TX_EDPM_TC_EN_SHIFT;
		val |= NIG_REG_TX_EDPM_CTRL_TX_EDPM_EN;
		qed_wr(p_hwfn, p_ptt, NIG_REG_TX_EDPM_CTRL, val);
	}

	qed_dcbx_aen(p_hwfn, type);

dcbx_err:
	if (type == QED_DCBX_OPERATIONAL_MIB &&
	    p_hwfn->p_dcbx_info->is_admin_cfg_applied)
		p_hwfn->p_dcbx_info->is_admin_cfg_applied = false;

	return rc;
}

int qed_dcbx_info_alloc(struct qed_hwfn *p_hwfn)
{
	p_hwfn->p_dcbx_info = kzalloc(sizeof(*p_hwfn->p_dcbx_info), GFP_KERNEL);
	if (!p_hwfn->p_dcbx_info) {
		DP_NOTICE(p_hwfn, "Failed to allocate `struct qed_dcbx_info'");
		return -ENOMEM;
	}

	p_hwfn->p_dcbx_info->iwarp_port =
	    p_hwfn->pf_params.rdma_pf_params.iwarp_port;

	return 0;
}

void qed_dcbx_info_free(struct qed_hwfn *p_hwfn)
{
	kfree(p_hwfn->p_dcbx_info);
	p_hwfn->p_dcbx_info = NULL;
}

static void qed_dcbx_update_protocol_data(struct protocol_dcb_data *p_data,
					  struct qed_dcbx_results *p_src,
					  enum dcbx_protocol_type type)
{
	p_data->dcb_enable_flag = p_src->arr[type].enable;
	p_data->dcb_priority = p_src->arr[type].priority;
	p_data->dcb_tc = p_src->arr[type].tc;
	p_data->dscp_enable_flag = p_src->arr[type].dscp_enable;
	p_data->dscp_val = p_src->arr[type].dscp_val;
	p_data->dcb_dont_add_vlan0 = p_src->arr[type].dont_add_vlan0;
}

/* Set pf update ramrod command params */
void qed_dcbx_set_pf_update_params(struct qed_dcbx_results *p_src,
				   struct pf_update_ramrod_data *p_dest)
{
	struct protocol_dcb_data *p_dcb_data;
	u8 update_flag;

	update_flag = p_src->arr[DCBX_PROTOCOL_FCOE].update;
	p_dest->update_fcoe_dcb_data_mode = update_flag;

	update_flag = p_src->arr[DCBX_PROTOCOL_ROCE].update;
	p_dest->update_roce_dcb_data_mode = update_flag;

	update_flag = p_src->arr[DCBX_PROTOCOL_ROCE_V2].update;
	p_dest->update_rroce_dcb_data_mode = update_flag;

	update_flag = p_src->arr[DCBX_PROTOCOL_ISCSI].update;
	p_dest->update_iscsi_dcb_data_mode = update_flag;
	update_flag = p_src->arr[DCBX_PROTOCOL_ETH].update;
	p_dest->update_eth_dcb_data_mode = update_flag;
	update_flag = p_src->arr[DCBX_PROTOCOL_IWARP].update;
	p_dest->update_iwarp_dcb_data_mode = update_flag;

	p_dcb_data = &p_dest->fcoe_dcb_data;
	qed_dcbx_update_protocol_data(p_dcb_data, p_src, DCBX_PROTOCOL_FCOE);
	p_dcb_data = &p_dest->roce_dcb_data;
	qed_dcbx_update_protocol_data(p_dcb_data, p_src, DCBX_PROTOCOL_ROCE);
	p_dcb_data = &p_dest->rroce_dcb_data;
	qed_dcbx_update_protocol_data(p_dcb_data, p_src, DCBX_PROTOCOL_ROCE_V2);
	p_dcb_data = &p_dest->iscsi_dcb_data;
	qed_dcbx_update_protocol_data(p_dcb_data, p_src, DCBX_PROTOCOL_ISCSI);
	p_dcb_data = &p_dest->eth_dcb_data;
	qed_dcbx_update_protocol_data(p_dcb_data, p_src, DCBX_PROTOCOL_ETH);
	p_dcb_data = &p_dest->iwarp_dcb_data;
	qed_dcbx_update_protocol_data(p_dcb_data, p_src, DCBX_PROTOCOL_IWARP);
}

bool qed_dcbx_get_dscp_state(struct qed_hwfn *p_hwfn)
{
	struct qed_dcbx_get *p_dcbx_info = &p_hwfn->p_dcbx_info->get;

	return p_dcbx_info->dscp.enabled;
}

u8 qed_dcbx_get_priority_tc(struct qed_hwfn * p_hwfn, u8 pri)
{
	struct qed_dcbx_get *dcbx_info = &p_hwfn->p_dcbx_info->get;

	if (pri >= QED_MAX_PFC_PRIORITIES) {
		DP_ERR(p_hwfn, "Invalid priority %d\n", pri);
		return QED_DCBX_DEFAULT_TC;
	}

	if (!dcbx_info->operational.valid) {
		DP_VERBOSE(p_hwfn, QED_MSG_DCB,
			   "Dcbx parameters not available\n");
		return QED_DCBX_DEFAULT_TC;
	}

	return dcbx_info->operational.params.ets_pri_tc_tbl[pri];
}

int qed_dcbx_query_params(struct qed_hwfn *p_hwfn,
			  struct qed_dcbx_get *p_get,
			  enum qed_mib_read_type type)
{
	struct qed_ptt *p_ptt;
	int rc;

#ifndef ASIC_ONLY
	if (!qed_mcp_is_init(p_hwfn))
		return -EINVAL;
#endif

	if (IS_VF(p_hwfn->cdev))
		return -EINVAL;

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return -EBUSY;

	rc = qed_dcbx_read_mib(p_hwfn, p_ptt, type);
	if (rc)
		goto out;

	qed_dcbx_get_dscp_params(p_hwfn, p_get);

	rc = qed_dcbx_get_params(p_hwfn, p_get, type);

out:
	qed_ptt_release(p_hwfn, p_ptt);
	return rc;
}

static void
qed_dcbx_set_pfc_data(struct qed_hwfn *p_hwfn,
		      u32 * pfc, struct qed_dcbx_params *p_params)
{
	u8 pfc_map = 0;
	int i;

	SET_MFW_FIELD(*pfc, DCBX_PFC_ERROR, 0);
	SET_MFW_FIELD(*pfc, DCBX_PFC_WILLING, p_params->pfc.willing ? 1 : 0);
	SET_MFW_FIELD(*pfc, DCBX_PFC_ENABLED, p_params->pfc.enabled ? 1 : 0);
	SET_MFW_FIELD(*pfc, DCBX_PFC_CAPS, (u32) p_params->pfc.max_tc);
	SET_MFW_FIELD(*pfc, DCBX_PFC_MBC, p_params->pfc.mbc ? 1 : 0);

	for (i = 0; i < QED_MAX_PFC_PRIORITIES; i++)
		if (p_params->pfc.prio[i])
			pfc_map |= (1 << i);
	SET_MFW_FIELD(*pfc, DCBX_PFC_PRI_EN_BITMAP, pfc_map);

	DP_VERBOSE(p_hwfn, QED_MSG_DCB, "pfc = 0x%x\n", *pfc);
}

static void
qed_dcbx_set_ets_data(struct qed_hwfn *p_hwfn,
		      struct dcbx_ets_feature *p_ets,
		      struct qed_dcbx_params *p_params)
{
	u8 *bw_map, *tsa_map;
	u32 val;
	int i;

	SET_MFW_FIELD(p_ets->flags, DCBX_ETS_WILLING,
		      p_params->ets_willing ? 1 : 0);
	SET_MFW_FIELD(p_ets->flags, DCBX_ETS_CBS, p_params->ets_cbs ? 1 : 0);
	SET_MFW_FIELD(p_ets->flags, DCBX_ETS_ENABLED,
		      p_params->ets_enabled ? 1 : 0);
	SET_MFW_FIELD(p_ets->flags, DCBX_ETS_MAX_TCS,
		      (u32) p_params->max_ets_tc);

	bw_map = (u8 *) & p_ets->tc_bw_tbl[0];
	tsa_map = (u8 *) & p_ets->tc_tsa_tbl[0];
	p_ets->pri_tc_tbl[0] = 0;
	for (i = 0; i < QED_MAX_PFC_PRIORITIES; i++) {
		bw_map[i] = p_params->ets_tc_bw_tbl[i];
		tsa_map[i] = p_params->ets_tc_tsa_tbl[i];
		/* Copy the priority value to the corresponding 4 bits in the
		 * traffic class table.
		 */
		val = (((u32) p_params->ets_pri_tc_tbl[i]) << ((7 - i) * 4));
		p_ets->pri_tc_tbl[0] |= val;
	}
	for (i = 0; i < 2; i++) {
		p_ets->tc_bw_tbl[i] = cpu_to_be32(p_ets->tc_bw_tbl[i]);
		p_ets->tc_tsa_tbl[i] = cpu_to_be32(p_ets->tc_tsa_tbl[i]);
	}

	DP_VERBOSE(p_hwfn,
		   QED_MSG_DCB,
		   "flags = 0x%x pri_tc = 0x%x tc_bwl[] = {0x%x, 0x%x} tc_tsa = {0x%x, 0x%x}\n",
		   p_ets->flags,
		   p_ets->pri_tc_tbl[0],
		   p_ets->tc_bw_tbl[0],
		   p_ets->tc_bw_tbl[1],
		   p_ets->tc_tsa_tbl[0], p_ets->tc_tsa_tbl[1]);
}

static void
qed_dcbx_set_app_data(struct qed_hwfn *p_hwfn,
		      struct dcbx_app_priority_feature *p_app,
		      struct qed_dcbx_params *p_params, bool ieee)
{
	u32 *entry;
	int i;

	SET_MFW_FIELD(p_app->flags, DCBX_APP_WILLING,
		      p_params->app_willing ? 1 : 0);
	SET_MFW_FIELD(p_app->flags, DCBX_APP_ENABLED,
		      p_params->app_valid ? 1 : 0);
	SET_MFW_FIELD(p_app->flags, DCBX_APP_NUM_ENTRIES,
		      (u32) p_params->num_app_entries);

	for (i = 0; i < p_params->num_app_entries; i++) {
		entry = &p_app->app_pri_tbl[i].entry;
		*entry = 0;
		if (ieee) {
			SET_MFW_FIELD(*entry, DCBX_APP_SF_IEEE, 0);
			SET_MFW_FIELD(*entry, DCBX_APP_SF, 0);
			switch (p_params->app_entry[i].sf_ieee) {
			case QED_DCBX_SF_IEEE_ETHTYPE:
				SET_MFW_FIELD(*entry, DCBX_APP_SF_IEEE,
					      (u32) DCBX_APP_SF_IEEE_ETHTYPE);
				SET_MFW_FIELD(*entry, DCBX_APP_SF,
					      (u32) DCBX_APP_SF_ETHTYPE);
				break;
			case QED_DCBX_SF_IEEE_TCP_PORT:
				SET_MFW_FIELD(*entry, DCBX_APP_SF_IEEE,
					      (u32) DCBX_APP_SF_IEEE_TCP_PORT);
				SET_MFW_FIELD(*entry, DCBX_APP_SF,
					      (u32) DCBX_APP_SF_PORT);
				break;
			case QED_DCBX_SF_IEEE_UDP_PORT:
				SET_MFW_FIELD(*entry, DCBX_APP_SF_IEEE,
					      (u32) DCBX_APP_SF_IEEE_UDP_PORT);
				SET_MFW_FIELD(*entry, DCBX_APP_SF,
					      (u32) DCBX_APP_SF_PORT);
				break;
			case QED_DCBX_SF_IEEE_TCP_UDP_PORT:
				SET_MFW_FIELD(*entry, DCBX_APP_SF_IEEE, (u32)
					      DCBX_APP_SF_IEEE_TCP_UDP_PORT);
				SET_MFW_FIELD(*entry, DCBX_APP_SF,
					      (u32) DCBX_APP_SF_PORT);
				break;
			}
		} else {
			SET_MFW_FIELD(*entry, DCBX_APP_SF,
				      p_params->app_entry[i].ethtype ?
				      (u32) DCBX_APP_SF_ETHTYPE :
				      (u32) DCBX_APP_SF_PORT);
		}
		SET_MFW_FIELD(*entry, DCBX_APP_PROTOCOL_ID,
			      (u32) p_params->app_entry[i].proto_id);
		SET_MFW_FIELD(*entry, DCBX_APP_PRI_MAP,
			      (u32) BIT(p_params->app_entry[i].prio));
	}

	DP_VERBOSE(p_hwfn, QED_MSG_DCB, "flags = 0x%x\n", p_app->flags);
}

static void
qed_dcbx_set_local_params(struct qed_hwfn *p_hwfn,
			  struct dcbx_local_params *local_admin,
			  struct qed_dcbx_set *params)
{
	bool ieee = false;

	local_admin->flags = 0;
	memcpy(&local_admin->features,
	       &p_hwfn->p_dcbx_info->operational.features,
	       sizeof(local_admin->features));

	if (params->enabled) {
		local_admin->config = params->ver_num;
		ieee = ! !(params->ver_num & DCBX_CONFIG_VERSION_IEEE);
	} else {
		local_admin->config = DCBX_CONFIG_VERSION_DISABLED;
	}

	DP_VERBOSE(p_hwfn, QED_MSG_DCB, "Dcbx version = %d\n",
		   local_admin->config);

	if (params->override_flags & QED_DCBX_OVERRIDE_PFC_CFG)
		qed_dcbx_set_pfc_data(p_hwfn, &local_admin->features.pfc,
				      &params->config.params);

	if (params->override_flags & QED_DCBX_OVERRIDE_ETS_CFG)
		qed_dcbx_set_ets_data(p_hwfn, &local_admin->features.ets,
				      &params->config.params);

	if (params->override_flags & QED_DCBX_OVERRIDE_APP_CFG)
		qed_dcbx_set_app_data(p_hwfn, &local_admin->features.app,
				      &params->config.params, ieee);
}

static int
qed_dcbx_set_dscp_params(struct qed_hwfn *p_hwfn,
			 struct dcb_dscp_map *p_dscp_map,
			 struct qed_dcbx_set *p_params)
{
	int entry, i, j;
	u32 val;

	memcpy(p_dscp_map, &p_hwfn->p_dcbx_info->dscp_map, sizeof(*p_dscp_map));

	SET_MFW_FIELD(p_dscp_map->flags, DCB_DSCP_ENABLE,
		      p_params->dscp.enabled ? 1 : 0);

	for (i = 0, entry = 0; i < 8; i++) {
		val = 0;
		for (j = 0; j < 8; j++, entry++)
			val |= (((u32) p_params->dscp.dscp_pri_map[entry]) <<
				(j * 4));

		p_dscp_map->dscp_pri_map[i] = val;
	}

	p_hwfn->p_dcbx_info->dscp_nig_update = true;

	DP_VERBOSE(p_hwfn, QED_MSG_DCB, "flags = 0x%x\n", p_dscp_map->flags);
	DP_VERBOSE(p_hwfn, QED_MSG_DCB,
		   "pri_map[] = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		   p_dscp_map->dscp_pri_map[0], p_dscp_map->dscp_pri_map[1],
		   p_dscp_map->dscp_pri_map[2], p_dscp_map->dscp_pri_map[3],
		   p_dscp_map->dscp_pri_map[4], p_dscp_map->dscp_pri_map[5],
		   p_dscp_map->dscp_pri_map[6], p_dscp_map->dscp_pri_map[7]);

	return 0;
}

int qed_dcbx_config_params(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt,
			   struct qed_dcbx_set *params, bool hw_commit)
{
	u32 resp = 0, param = 0, drv_mb_param = 0;
	struct dcbx_local_params local_admin;
	struct qed_dcbx_mib_meta_data data;
	struct dcb_dscp_map dscp_map;
	int rc = 0;

	memcpy(&p_hwfn->p_dcbx_info->set, params,
	       sizeof(p_hwfn->p_dcbx_info->set));
	if (!hw_commit)
		return 0;

	memset(&local_admin, 0, sizeof(local_admin));
	qed_dcbx_set_local_params(p_hwfn, &local_admin, params);

	data.addr = p_hwfn->mcp_info->port_addr +
	    offsetof(struct public_port, local_admin_dcbx_mib);
	data.local_admin = &local_admin;
	data.size = sizeof(struct dcbx_local_params);
	qed_memcpy_to(p_hwfn, p_ptt, data.addr, data.local_admin, data.size);

	if (params->override_flags & QED_DCBX_OVERRIDE_DSCP_CFG) {
		memset(&dscp_map, 0, sizeof(dscp_map));
		qed_dcbx_set_dscp_params(p_hwfn, &dscp_map, params);

		data.addr = p_hwfn->mcp_info->port_addr +
		    offsetof(struct public_port, dcb_dscp_map);
		data.dscp_map = &dscp_map;
		data.size = sizeof(struct dcb_dscp_map);
		qed_memcpy_to(p_hwfn, p_ptt, data.addr, data.dscp_map,
			      data.size);
	}

	/* For Non static dcbx mode, send NOTIFY command to MFW. */
	if (params->ver_num != DCBX_CONFIG_VERSION_STATIC &&
	    params->override_flags & QED_DCBX_OVERRIDE_ADMIN_CFG) {
		/* To receive notification from MFW */
		SET_MFW_FIELD(drv_mb_param,
			      DRV_MB_PARAM_DCBX_ADMIN_CFG_NOTIFY, 1);
	}

	SET_MFW_FIELD(drv_mb_param, DRV_MB_PARAM_LLDP_SEND, 1);
	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_SET_DCBX,
			 drv_mb_param, &resp, &param);
	if (rc)
		DP_NOTICE(p_hwfn, "Failed to send DCBX update request\n");

	return rc;
}

int qed_dcbx_get_config_params(struct qed_hwfn *p_hwfn,
			       struct qed_dcbx_set *params)
{
	struct qed_dcbx_get *dcbx_info;
	int rc;

	if (p_hwfn->p_dcbx_info->set.config.valid) {
		memcpy(params, &p_hwfn->p_dcbx_info->set,
		       sizeof(struct qed_dcbx_set));
		return 0;
	}

	dcbx_info = kmalloc(sizeof(*dcbx_info), GFP_KERNEL);
	if (!dcbx_info)
		return -ENOMEM;

	memset(dcbx_info, 0, sizeof(*dcbx_info));
	rc = qed_dcbx_query_params(p_hwfn, dcbx_info, QED_DCBX_OPERATIONAL_MIB);
	if (rc) {
		kfree(dcbx_info);
		return rc;
	}
	p_hwfn->p_dcbx_info->set.override_flags = 0;

	p_hwfn->p_dcbx_info->set.ver_num = DCBX_CONFIG_VERSION_DISABLED;
	if (dcbx_info->operational.cee)
		p_hwfn->p_dcbx_info->set.ver_num |= DCBX_CONFIG_VERSION_CEE;
	if (dcbx_info->operational.ieee)
		p_hwfn->p_dcbx_info->set.ver_num |= DCBX_CONFIG_VERSION_IEEE;
	if (dcbx_info->operational.local)
		p_hwfn->p_dcbx_info->set.ver_num |= DCBX_CONFIG_VERSION_STATIC;

	p_hwfn->p_dcbx_info->set.enabled = dcbx_info->operational.enabled;
	memcpy(&p_hwfn->p_dcbx_info->set.dscp,
	       &p_hwfn->p_dcbx_info->get.dscp,
	       sizeof(struct qed_dcbx_dscp_params));
	memcpy(&p_hwfn->p_dcbx_info->set.config.params,
	       &dcbx_info->operational.params,
	       sizeof(p_hwfn->p_dcbx_info->set.config.params));
	p_hwfn->p_dcbx_info->set.config.valid = true;

	memcpy(params, &p_hwfn->p_dcbx_info->set, sizeof(struct qed_dcbx_set));

	kfree(dcbx_info);

	return 0;
}

int qed_lldp_register_tlv(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt,
			  enum qed_lldp_agent agent, u8 tlv_type)
{
	u32 mb_param = 0, mcp_resp = 0, mcp_param = 0, val = 0;
	int rc = 0;

	switch (agent) {
	case QED_LLDP_NEAREST_BRIDGE:
		val = LLDP_NEAREST_BRIDGE;
		break;
	case QED_LLDP_NEAREST_NON_TPMR_BRIDGE:
		val = LLDP_NEAREST_NON_TPMR_BRIDGE;
		break;
	case QED_LLDP_NEAREST_CUSTOMER_BRIDGE:
		val = LLDP_NEAREST_CUSTOMER_BRIDGE;
		break;
	default:
		DP_ERR(p_hwfn, "Invalid agent type %d\n", agent);
		return -EINVAL;
	}

	SET_MFW_FIELD(mb_param, DRV_MB_PARAM_LLDP_AGENT, val);
	SET_MFW_FIELD(mb_param, DRV_MB_PARAM_LLDP_TLV_RX_TYPE, tlv_type);

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_REGISTER_LLDP_TLVS_RX,
			 mb_param, &mcp_resp, &mcp_param);
	if (rc)
		DP_NOTICE(p_hwfn, "Failed to register TLV\n");

	return rc;
}

int qed_lldp_mib_update_event(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_dcbx_mib_meta_data data;
	int rc = 0;
	struct lldp_received_tlvs_s tlvs;
	int i;

	for (i = 0; i < LLDP_MAX_LLDP_AGENTS; i++) {
		memset(&data, 0, sizeof(data));
		data.addr = p_hwfn->mcp_info->port_addr +
		    offsetof(struct public_port, lldp_received_tlvs[i]);
		data.lldp_tlvs = &tlvs;
		data.size = sizeof(tlvs);
		rc = qed_dcbx_copy_mib(p_hwfn, p_ptt, &data,
				       QED_DCBX_LLDP_TLVS);
		if (rc) {
			DP_NOTICE(p_hwfn, "Failed to read lldp TLVs\n");
			return rc;
		}

		if (!tlvs.length)
			continue;

		for (i = 0; i < MAX_TLV_BUFFER; i++)
			tlvs.tlvs_buffer[i] = cpu_to_be32(tlvs.tlvs_buffer[i]);
	}

	return rc;
}

int
qed_lldp_get_params(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    struct qed_lldp_config_params *p_params)
{
	struct lldp_config_params_s lldp_params;
	u32 addr, val;
	int i;

	switch (p_params->agent) {
	case QED_LLDP_NEAREST_BRIDGE:
		val = LLDP_NEAREST_BRIDGE;
		break;
	case QED_LLDP_NEAREST_NON_TPMR_BRIDGE:
		val = LLDP_NEAREST_NON_TPMR_BRIDGE;
		break;
	case QED_LLDP_NEAREST_CUSTOMER_BRIDGE:
		val = LLDP_NEAREST_CUSTOMER_BRIDGE;
		break;
	default:
		DP_ERR(p_hwfn, "Invalid agent type %d\n", p_params->agent);
		return -EINVAL;
	}

	addr = p_hwfn->mcp_info->port_addr +
	    offsetof(struct public_port, lldp_config_params[val]);

	qed_memcpy_from(p_hwfn, p_ptt, &lldp_params, addr, sizeof(lldp_params));

	p_params->tx_interval = GET_MFW_FIELD(lldp_params.config,
					      LLDP_CONFIG_TX_INTERVAL);
	p_params->tx_hold = GET_MFW_FIELD(lldp_params.config, LLDP_CONFIG_HOLD);
	p_params->tx_credit = GET_MFW_FIELD(lldp_params.config,
					    LLDP_CONFIG_MAX_CREDIT);
	p_params->rx_enable = GET_MFW_FIELD(lldp_params.config,
					    LLDP_CONFIG_ENABLE_RX);
	p_params->tx_enable = GET_MFW_FIELD(lldp_params.config,
					    LLDP_CONFIG_ENABLE_TX);

	memcpy(p_params->chassis_id_tlv, lldp_params.local_chassis_id,
	       sizeof(p_params->chassis_id_tlv));
	for (i = 0; i < QED_LLDP_CHASSIS_ID_STAT_LEN; i++)
		p_params->chassis_id_tlv[i] =
		    be32_to_cpu(p_params->chassis_id_tlv[i]);

	memcpy(p_params->port_id_tlv, lldp_params.local_port_id,
	       sizeof(p_params->port_id_tlv));
	for (i = 0; i < QED_LLDP_PORT_ID_STAT_LEN; i++)
		p_params->port_id_tlv[i] =
		    be32_to_cpu(p_params->port_id_tlv[i]);

	return 0;
}

int
qed_lldp_set_params(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    struct qed_lldp_config_params *p_params)
{
	u32 mb_param = 0, mcp_resp = 0, mcp_param = 0;
	struct lldp_config_params_s lldp_params;
	int rc = 0;
	u32 addr, val;
	int i;

	switch (p_params->agent) {
	case QED_LLDP_NEAREST_BRIDGE:
		val = LLDP_NEAREST_BRIDGE;
		break;
	case QED_LLDP_NEAREST_NON_TPMR_BRIDGE:
		val = LLDP_NEAREST_NON_TPMR_BRIDGE;
		break;
	case QED_LLDP_NEAREST_CUSTOMER_BRIDGE:
		val = LLDP_NEAREST_CUSTOMER_BRIDGE;
		break;
	default:
		DP_ERR(p_hwfn, "Invalid agent type %d\n", p_params->agent);
		return -EINVAL;
	}

	SET_MFW_FIELD(mb_param, DRV_MB_PARAM_LLDP_AGENT, val);
	addr = p_hwfn->mcp_info->port_addr +
	    offsetof(struct public_port, lldp_config_params[val]);

	memset(&lldp_params, 0, sizeof(lldp_params));
	SET_MFW_FIELD(lldp_params.config, LLDP_CONFIG_TX_INTERVAL,
		      p_params->tx_interval);
	SET_MFW_FIELD(lldp_params.config, LLDP_CONFIG_HOLD, p_params->tx_hold);
	SET_MFW_FIELD(lldp_params.config, LLDP_CONFIG_MAX_CREDIT,
		      p_params->tx_credit);
	SET_MFW_FIELD(lldp_params.config, LLDP_CONFIG_ENABLE_RX,
		      ! !p_params->rx_enable);
	SET_MFW_FIELD(lldp_params.config, LLDP_CONFIG_ENABLE_TX,
		      ! !p_params->tx_enable);

	for (i = 0; i < QED_LLDP_CHASSIS_ID_STAT_LEN; i++)
		p_params->chassis_id_tlv[i] =
		    cpu_to_be32(p_params->chassis_id_tlv[i]);
	memcpy(lldp_params.local_chassis_id, p_params->chassis_id_tlv,
	       sizeof(lldp_params.local_chassis_id));

	for (i = 0; i < QED_LLDP_PORT_ID_STAT_LEN; i++)
		p_params->port_id_tlv[i] =
		    cpu_to_be32(p_params->port_id_tlv[i]);
	memcpy(lldp_params.local_port_id, p_params->port_id_tlv,
	       sizeof(lldp_params.local_port_id));

	qed_memcpy_to(p_hwfn, p_ptt, addr, &lldp_params, sizeof(lldp_params));

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_SET_LLDP,
			 mb_param, &mcp_resp, &mcp_param);
	if (rc)
		DP_NOTICE(p_hwfn, "SET_LLDP failed, error = %d\n", rc);

	return rc;
}

int
qed_lldp_set_system_tlvs(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 struct qed_lldp_sys_tlvs *p_params)
{
	u32 mb_param = 0, mcp_resp = 0, mcp_param = 0;
	int rc = 0;
	struct lldp_system_tlvs_buffer_s lld_tlv_buf;
	u32 addr, *p_val;
	u8 len;
	int i;

	p_val = (u32 *) p_params->buf;
	for (i = 0; i < QED_LLDP_SYS_TLV_SIZE / 4; i++)
		p_val[i] = cpu_to_be32(p_val[i]);

	memset(&lld_tlv_buf, 0, sizeof(lld_tlv_buf));
	SET_MFW_FIELD(lld_tlv_buf.flags, LLDP_SYSTEM_TLV_VALID, 1);
	SET_MFW_FIELD(lld_tlv_buf.flags, LLDP_SYSTEM_TLV_MANDATORY,
		      ! !p_params->discard_mandatory_tlv);
	SET_MFW_FIELD(lld_tlv_buf.flags, LLDP_SYSTEM_TLV_LENGTH,
		      p_params->buf_size);
	len = QED_LLDP_SYS_TLV_SIZE / 2;
	memcpy(lld_tlv_buf.data, p_params->buf, len);

	addr = p_hwfn->mcp_info->port_addr +
	    offsetof(struct public_port, system_lldp_tlvs_buf);
	qed_memcpy_to(p_hwfn, p_ptt, addr, &lld_tlv_buf, sizeof(lld_tlv_buf));

	if (p_params->buf_size > len) {
		addr = p_hwfn->mcp_info->port_addr +
		    offsetof(struct public_port, system_lldp_tlvs_buf2);
		qed_memcpy_to(p_hwfn, p_ptt, addr, &p_params->buf[len],
			      QED_LLDP_SYS_TLV_SIZE / 2);
	}

	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_SET_LLDP,
			 mb_param, &mcp_resp, &mcp_param);
	if (rc)
		DP_NOTICE(p_hwfn, "SET_LLDP failed, error = %d\n", rc);

	return rc;
}

int
qed_dcbx_get_dscp_priority(struct qed_hwfn *p_hwfn,
			   u8 dscp_index, u8 * p_dscp_pri)
{
	struct qed_dcbx_get *p_dcbx_info;
	int rc;

	if (IS_VF(p_hwfn->cdev)) {
		DP_ERR(p_hwfn->cdev,
		       "qed rdma get dscp priority not supported for VF.\n");
		return -EINVAL;
	}

	if (dscp_index >= QED_DCBX_DSCP_SIZE) {
		DP_ERR(p_hwfn, "Invalid dscp index %d\n", dscp_index);
		return -EINVAL;
	}

	p_dcbx_info = kmalloc(sizeof(*p_dcbx_info), GFP_KERNEL);
	if (!p_dcbx_info)
		return -ENOMEM;

	memset(p_dcbx_info, 0, sizeof(*p_dcbx_info));
	rc = qed_dcbx_query_params(p_hwfn, p_dcbx_info,
				   QED_DCBX_OPERATIONAL_MIB);
	if (rc) {
		kfree(p_dcbx_info);
		return rc;
	}

	*p_dscp_pri = p_dcbx_info->dscp.dscp_pri_map[dscp_index];
	kfree(p_dcbx_info);

	return 0;
}

int
qed_dcbx_set_dscp_priority(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, u8 dscp_index, u8 pri_val)
{
	struct qed_dcbx_set dcbx_set;
	int rc;

	if (IS_VF(p_hwfn->cdev)) {
		DP_ERR(p_hwfn->cdev,
		       "qed rdma set dscp priority not supported for VF.\n");
		return -EINVAL;
	}

	if (dscp_index >= QED_DCBX_DSCP_SIZE ||
	    pri_val >= QED_MAX_PFC_PRIORITIES) {
		DP_ERR(p_hwfn, "Invalid dscp params: index = %d pri = %d\n",
		       dscp_index, pri_val);
		return -EINVAL;
	}

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(p_hwfn, &dcbx_set);
	if (rc)
		return rc;

	dcbx_set.override_flags = QED_DCBX_OVERRIDE_DSCP_CFG;
	dcbx_set.dscp.dscp_pri_map[dscp_index] = pri_val;

	return qed_dcbx_config_params(p_hwfn, p_ptt, &dcbx_set, 1);
}

int
qed_lldp_get_stats(struct qed_hwfn *p_hwfn,
		   struct qed_ptt *p_ptt, struct qed_lldp_stats *p_params)
{
	u32 mcp_resp = 0, mcp_param = 0, drv_mb_param = 0, addr, val;
	struct lldp_stats_stc lldp_stats;
	int rc;

	switch (p_params->agent) {
	case QED_LLDP_NEAREST_BRIDGE:
		val = LLDP_NEAREST_BRIDGE;
		break;
	case QED_LLDP_NEAREST_NON_TPMR_BRIDGE:
		val = LLDP_NEAREST_NON_TPMR_BRIDGE;
		break;
	case QED_LLDP_NEAREST_CUSTOMER_BRIDGE:
		val = LLDP_NEAREST_CUSTOMER_BRIDGE;
		break;
	default:
		DP_ERR(p_hwfn, "Invalid agent type %d\n", p_params->agent);
		return -EINVAL;
	}

	SET_MFW_FIELD(drv_mb_param, DRV_MB_PARAM_LLDP_STATS_AGENT, val);
	rc = qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_GET_LLDP_STATS,
			 drv_mb_param, &mcp_resp, &mcp_param);
	if (rc) {
		DP_ERR(p_hwfn, "GET_LLDP_STATS failed, error = %d\n", rc);
		return rc;
	}

	addr = p_hwfn->mcp_info->drv_mb_addr +
	    offsetof(struct public_drv_mb, union_data);

	qed_memcpy_from(p_hwfn, p_ptt, &lldp_stats, addr, sizeof(lldp_stats));

	p_params->tx_frames = lldp_stats.tx_frames_total;
	p_params->rx_frames = lldp_stats.rx_frames_total;
	p_params->rx_discards = lldp_stats.rx_frames_discarded;
	p_params->rx_age_outs = lldp_stats.rx_age_outs;

	return 0;
}

bool qed_dcbx_is_enabled(struct qed_hwfn * p_hwfn)
{
	struct qed_dcbx_operational_params *op_params =
	    &p_hwfn->p_dcbx_info->get.operational;

	if (op_params->valid && op_params->enabled)
		return true;

	return false;
}

void qed_dcbx_aen(struct qed_hwfn *hwfn, u32 mib_type)
{
	struct qed_common_cb_ops *op = hwfn->cdev->protocol_ops.common;
	void *cookie = hwfn->cdev->ops_cookie;

	if (cookie && op->dcbx_aen)
		op->dcbx_aen(cookie, &hwfn->p_dcbx_info->get, mib_type);
}

static struct qed_dcbx_get *qed_dcbnl_get_dcbx(struct qed_hwfn *hwfn,
					       enum qed_mib_read_type type)
{
	struct qed_dcbx_get *dcbx_info;

	dcbx_info = kmalloc(sizeof(*dcbx_info), GFP_ATOMIC);
	if (!dcbx_info) {
		DP_ERR(hwfn->cdev, "Failed to allocate memory for dcbx_info\n");
		return NULL;
	}

	memset(dcbx_info, 0, sizeof(*dcbx_info));
	if (qed_dcbx_query_params(hwfn, dcbx_info, type)) {
		kfree(dcbx_info);
		return NULL;
	}

	if ((type == QED_DCBX_OPERATIONAL_MIB) &&
	    !dcbx_info->operational.enabled) {
		DP_INFO(hwfn, "DCBX is not enabled/operational\n");
		kfree(dcbx_info);
		return NULL;
	}

	return dcbx_info;
}

static u8 qed_dcbnl_getstate(struct qed_dev *cdev)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_get *dcbx_info;
	bool enabled;

	dcbx_info = qed_dcbnl_get_dcbx(hwfn, QED_DCBX_OPERATIONAL_MIB);
	if (!dcbx_info)
		return 0;

	enabled = dcbx_info->operational.enabled;
	DP_VERBOSE(hwfn, QED_MSG_DCB, "DCB state = %d\n", enabled);
	kfree(dcbx_info);

	return enabled;
}

static u8 qed_dcbnl_setstate(struct qed_dev *cdev, u8 state)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_get *dcbx_info;
	struct qed_dcbx_set dcbx_set;
	struct qed_ptt *ptt;
	int rc;

	DP_VERBOSE(hwfn, QED_MSG_DCB, "DCB state = %d\n", state);

	if (qed_dcbnl_getstate(cdev) == state)
		return 0;

	if (!hwfn->p_dcbx_info->set.config.valid) {
		dcbx_info = kmalloc(sizeof(*dcbx_info), GFP_KERNEL);
		if (!dcbx_info)
			return -ENOMEM;

		memset(dcbx_info, 0, sizeof(*dcbx_info));
		rc = qed_dcbx_query_params(hwfn, dcbx_info, QED_DCBX_LOCAL_MIB);
		if (rc) {
			kfree(dcbx_info);
			return rc;
		}

		hwfn->p_dcbx_info->set.ver_num = DCBX_CONFIG_VERSION_STATIC;
		memcpy(&hwfn->p_dcbx_info->set.dscp,
		       &hwfn->p_dcbx_info->get.dscp,
		       sizeof(struct qed_dcbx_dscp_params));
		memcpy(&hwfn->p_dcbx_info->set.config.params,
		       &dcbx_info->local.params,
		       sizeof(hwfn->p_dcbx_info->set.config.params));
		hwfn->p_dcbx_info->set.config.valid = true;

		kfree(dcbx_info);
	}

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	memcpy(&dcbx_set, &hwfn->p_dcbx_info->set, sizeof(struct qed_dcbx_set));
	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return 1;
	dcbx_set.enabled = ! !state;
	rc = qed_dcbx_config_params(hwfn, ptt, &dcbx_set, 1);
	qed_ptt_release(hwfn, ptt);

	return rc ? 1 : 0;
}

static void qed_dcbnl_getpgtccfgtx(struct qed_dev *cdev,
				   int tc,
				   u8 * prio_type,
				   u8 * pgid, u8 * bw_pct, u8 * up_map)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_get *dcbx_info;

	DP_VERBOSE(hwfn, QED_MSG_DCB, "tc = %d\n", tc);
	*prio_type = *pgid = *bw_pct = *up_map = 0;
	if (tc < 0 || tc >= QED_MAX_PFC_PRIORITIES) {
		DP_INFO(hwfn, "Invalid tc %d\n", tc);
		return;
	}

	dcbx_info = qed_dcbnl_get_dcbx(hwfn, QED_DCBX_OPERATIONAL_MIB);
	if (!dcbx_info)
		return;

	*pgid = dcbx_info->operational.params.ets_pri_tc_tbl[tc];
	kfree(dcbx_info);
}

static void qed_dcbnl_getpgbwgcfgtx(struct qed_dev *cdev, int pgid, u8 * bw_pct)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_get *dcbx_info;

	*bw_pct = 0;
	DP_VERBOSE(hwfn, QED_MSG_DCB, "pgid = %d\n", pgid);
	if (pgid < 0 || pgid >= QED_MAX_PFC_PRIORITIES) {
		DP_INFO(hwfn, "Invalid pgid %d\n", pgid);
		return;
	}

	dcbx_info = qed_dcbnl_get_dcbx(hwfn, QED_DCBX_OPERATIONAL_MIB);
	if (!dcbx_info)
		return;

	*bw_pct = dcbx_info->operational.params.ets_tc_bw_tbl[pgid];
	DP_VERBOSE(hwfn, QED_MSG_DCB, "bw_pct = %d\n", *bw_pct);
	kfree(dcbx_info);
}

static void qed_dcbnl_getpgtccfgrx(struct qed_dev *cdev,
				   int tc,
				   u8 * prio,
				   u8 * bwg_id, u8 * bw_pct, u8 * up_map)
{
	DP_INFO(QED_LEADING_HWFN(cdev), "Rx ETS is not supported\n");
	*prio = *bwg_id = *bw_pct = *up_map = 0;
}

static void qed_dcbnl_getpgbwgcfgrx(struct qed_dev *cdev,
				    int bwg_id, u8 * bw_pct)
{
	DP_INFO(QED_LEADING_HWFN(cdev), "Rx ETS is not supported\n");
	*bw_pct = 0;
}

static void qed_dcbnl_getpfccfg(struct qed_dev *cdev,
				int priority, u8 * setting)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_get *dcbx_info;

	DP_VERBOSE(hwfn, QED_MSG_DCB, "priority = %d\n", priority);
	if (priority < 0 || priority >= QED_MAX_PFC_PRIORITIES) {
		DP_INFO(hwfn, "Invalid priority %d\n", priority);
		return;
	}

	dcbx_info = qed_dcbnl_get_dcbx(hwfn, QED_DCBX_OPERATIONAL_MIB);
	if (!dcbx_info)
		return;

	*setting = dcbx_info->operational.params.pfc.prio[priority];
	DP_VERBOSE(hwfn, QED_MSG_DCB, "setting = %d\n", *setting);
	kfree(dcbx_info);
}

static void qed_dcbnl_setpfccfg(struct qed_dev *cdev, int priority, u8 setting)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_set dcbx_set;
	struct qed_ptt *ptt;
	int rc;

	DP_VERBOSE(hwfn, QED_MSG_DCB, "priority = %d setting = %d\n",
		   priority, setting);
	if (priority < 0 || priority >= QED_MAX_PFC_PRIORITIES) {
		DP_INFO(hwfn, "Invalid priority %d\n", priority);
		return;
	}

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(hwfn, &dcbx_set);
	if (rc)
		return;

	dcbx_set.override_flags |= QED_DCBX_OVERRIDE_PFC_CFG;
	dcbx_set.config.params.pfc.prio[priority] = ! !setting;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return;

	rc = qed_dcbx_config_params(hwfn, ptt, &dcbx_set, 1);

	qed_ptt_release(hwfn, ptt);
}

static u8 qed_dcbnl_getcap(struct qed_dev *cdev, int capid, u8 * cap)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_get *dcbx_info;
	int rc = 0;

	DP_VERBOSE(hwfn, QED_MSG_DCB, "capid = %d\n", capid);
	dcbx_info = qed_dcbnl_get_dcbx(hwfn, QED_DCBX_OPERATIONAL_MIB);
	if (!dcbx_info)
		return 1;

	switch (capid) {
	case DCB_CAP_ATTR_PG:
	case DCB_CAP_ATTR_PFC:
	case DCB_CAP_ATTR_UP2TC:
	case DCB_CAP_ATTR_GSP:
		*cap = true;
		break;
	case DCB_CAP_ATTR_PG_TCS:
	case DCB_CAP_ATTR_PFC_TCS:
		*cap = 0x80;
		break;
	case DCB_CAP_ATTR_DCBX:
		*cap = (DCB_CAP_DCBX_VER_CEE | DCB_CAP_DCBX_VER_IEEE |
			DCB_CAP_DCBX_HOST);
		break;
	default:
		*cap = false;
		rc = 1;
	}

	DP_VERBOSE(hwfn, QED_MSG_DCB, "id = %d caps = %d\n", capid, *cap);
	kfree(dcbx_info);

	return rc;
}

static int qed_dcbnl_getnumtcs(struct qed_dev *cdev, int tcid, u8 * num)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_get *dcbx_info;
	int rc = 0;

	DP_VERBOSE(hwfn, QED_MSG_DCB, "tcid = %d\n", tcid);
	dcbx_info = qed_dcbnl_get_dcbx(hwfn, QED_DCBX_OPERATIONAL_MIB);
	if (!dcbx_info)
		return -EINVAL;

	switch (tcid) {
	case DCB_NUMTCS_ATTR_PG:
		*num = dcbx_info->operational.params.max_ets_tc;
		break;
	case DCB_NUMTCS_ATTR_PFC:
		*num = dcbx_info->operational.params.pfc.max_tc;
		break;
	default:
		rc = -EINVAL;
	}

	kfree(dcbx_info);
	DP_VERBOSE(hwfn, QED_MSG_DCB, "numtcs = %d\n", *num);

	return rc;
}

static u8 qed_dcbnl_getpfcstate(struct qed_dev *cdev)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_get *dcbx_info;
	bool enabled;

	dcbx_info = qed_dcbnl_get_dcbx(hwfn, QED_DCBX_OPERATIONAL_MIB);
	if (!dcbx_info)
		return 0;

	enabled = dcbx_info->operational.params.pfc.enabled;
	DP_VERBOSE(hwfn, QED_MSG_DCB, "pfc state = %d\n", enabled);
	kfree(dcbx_info);

	return enabled;
}

static u8 qed_dcbnl_getdcbx(struct qed_dev *cdev)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_get *dcbx_info;
	u8 mode = 0;

	dcbx_info = qed_dcbnl_get_dcbx(hwfn, QED_DCBX_OPERATIONAL_MIB);
	if (!dcbx_info)
		return 0;

	mode = DCB_CAP_DCBX_VER_IEEE | DCB_CAP_DCBX_VER_CEE | DCB_CAP_DCBX_HOST;

	DP_VERBOSE(hwfn, QED_MSG_DCB, "dcb mode = %d\n", mode);
	kfree(dcbx_info);

	return mode;
}

static void qed_dcbnl_setpgtccfgtx(struct qed_dev *cdev,
				   int tc,
				   u8 pri_type, u8 pgid, u8 bw_pct, u8 up_map)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_set dcbx_set;
	struct qed_ptt *ptt;
	int rc;

	DP_VERBOSE(hwfn, QED_MSG_DCB,
		   "tc = %d pri_type = %d pgid = %d bw_pct = %d up_map = %d\n",
		   tc, pri_type, pgid, bw_pct, up_map);

	if (tc < 0 || tc >= QED_MAX_PFC_PRIORITIES) {
		DP_INFO(hwfn, "Invalid tc %d\n", tc);
		return;
	}

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(hwfn, &dcbx_set);
	if (rc)
		return;

	dcbx_set.override_flags |= QED_DCBX_OVERRIDE_ETS_CFG;
	dcbx_set.config.params.ets_pri_tc_tbl[tc] = pgid;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return;

	rc = qed_dcbx_config_params(hwfn, ptt, &dcbx_set, 1);

	qed_ptt_release(hwfn, ptt);
}

static void qed_dcbnl_setpgtccfgrx(struct qed_dev *cdev,
				   int prio,
				   u8 pri_type, u8 pgid, u8 bw_pct, u8 up_map)
{
	DP_INFO(QED_LEADING_HWFN(cdev), "Rx ETS is not supported\n");
}

static void qed_dcbnl_setpgbwgcfgtx(struct qed_dev *cdev, int pgid, u8 bw_pct)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_set dcbx_set;
	struct qed_ptt *ptt;
	int rc;

	DP_VERBOSE(hwfn, QED_MSG_DCB, "pgid = %d bw_pct = %d\n", pgid, bw_pct);
	if (pgid < 0 || pgid >= QED_MAX_PFC_PRIORITIES) {
		DP_INFO(hwfn, "Invalid pgid %d\n", pgid);
		return;
	}

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(hwfn, &dcbx_set);
	if (rc)
		return;

	dcbx_set.override_flags |= QED_DCBX_OVERRIDE_ETS_CFG;
	dcbx_set.config.params.ets_tc_bw_tbl[pgid] = bw_pct;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return;

	rc = qed_dcbx_config_params(hwfn, ptt, &dcbx_set, 1);

	qed_ptt_release(hwfn, ptt);
}

static void qed_dcbnl_setpgbwgcfgrx(struct qed_dev *cdev, int pgid, u8 bw_pct)
{
	DP_INFO(QED_LEADING_HWFN(cdev), "Rx ETS is not supported\n");
}

static u8 qed_dcbnl_setall(struct qed_dev *cdev)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_set dcbx_set;
	struct qed_ptt *ptt;
	int rc;

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(hwfn, &dcbx_set);
	if (rc)
		return 1;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return 1;

	dcbx_set.ver_num = DCBX_CONFIG_VERSION_STATIC;
	rc = qed_dcbx_config_params(hwfn, ptt, &dcbx_set, 1);

	qed_ptt_release(hwfn, ptt);

	return rc;
}

static int qed_dcbnl_setnumtcs(struct qed_dev *cdev, int tcid, u8 num)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_set dcbx_set;
	struct qed_ptt *ptt;
	int rc;

	DP_VERBOSE(hwfn, QED_MSG_DCB, "tcid = %d num = %d\n", tcid, num);
	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(hwfn, &dcbx_set);
	if (rc)
		return 1;

	switch (tcid) {
	case DCB_NUMTCS_ATTR_PG:
		dcbx_set.override_flags |= QED_DCBX_OVERRIDE_ETS_CFG;
		dcbx_set.config.params.max_ets_tc = num;
		break;
	case DCB_NUMTCS_ATTR_PFC:
		dcbx_set.override_flags |= QED_DCBX_OVERRIDE_PFC_CFG;
		dcbx_set.config.params.pfc.max_tc = num;
		break;
	default:
		DP_INFO(hwfn, "Invalid tcid %d\n", tcid);
		return -EINVAL;
	}

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return -EINVAL;

	rc = qed_dcbx_config_params(hwfn, ptt, &dcbx_set, 1);

	qed_ptt_release(hwfn, ptt);

	return 0;
}

static void qed_dcbnl_setpfcstate(struct qed_dev *cdev, u8 state)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_set dcbx_set;
	struct qed_ptt *ptt;
	int rc;

	DP_VERBOSE(hwfn, QED_MSG_DCB, "new state = %d\n", state);

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(hwfn, &dcbx_set);
	if (rc)
		return;

	dcbx_set.override_flags |= QED_DCBX_OVERRIDE_PFC_CFG;
	dcbx_set.config.params.pfc.enabled = ! !state;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return;

	rc = qed_dcbx_config_params(hwfn, ptt, &dcbx_set, 1);

	qed_ptt_release(hwfn, ptt);
}

static int qed_dcbnl_getapp(struct qed_dev *cdev, u8 idtype, u16 idval)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_get *dcbx_info;
	struct qed_app_entry *entry;
	bool ethtype;
	u8 prio = 0;
	int i;

	dcbx_info = qed_dcbnl_get_dcbx(hwfn, QED_DCBX_OPERATIONAL_MIB);
	if (!dcbx_info)
		return -EINVAL;

	ethtype = ! !(idtype == DCB_APP_IDTYPE_ETHTYPE);
	for (i = 0; i < QED_DCBX_MAX_APP_PROTOCOL; i++) {
		entry = &dcbx_info->operational.params.app_entry[i];
		if ((entry->ethtype == ethtype) && (entry->proto_id == idval)) {
			prio = entry->prio;
			break;
		}
	}

	if (i == QED_DCBX_MAX_APP_PROTOCOL) {
		DP_ERR(cdev, "App entry (%d, %d) not found\n", idtype, idval);
		kfree(dcbx_info);
		return -EINVAL;
	}

	kfree(dcbx_info);

	return prio;
}

static int qed_dcbnl_setapp(struct qed_dev *cdev,
			    u8 idtype, u16 idval, u8 pri_map)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_set dcbx_set;
	struct qed_app_entry *entry;
	struct qed_ptt *ptt;
	bool ethtype;
	int rc, i;

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(hwfn, &dcbx_set);
	if (rc)
		return -EINVAL;

	ethtype = ! !(idtype == DCB_APP_IDTYPE_ETHTYPE);
	for (i = 0; i < QED_DCBX_MAX_APP_PROTOCOL; i++) {
		entry = &dcbx_set.config.params.app_entry[i];
		if ((entry->ethtype == ethtype) && (entry->proto_id == idval))
			break;
		/* First empty slot */
		if (!entry->proto_id) {
			dcbx_set.config.params.num_app_entries++;
			break;
		}
	}

	if (i == QED_DCBX_MAX_APP_PROTOCOL) {
		DP_ERR(cdev, "App table is full\n");
		return -EBUSY;
	}

	dcbx_set.override_flags |= QED_DCBX_OVERRIDE_APP_CFG;
	dcbx_set.config.params.app_entry[i].ethtype = ethtype;
	dcbx_set.config.params.app_entry[i].proto_id = idval;
	dcbx_set.config.params.app_entry[i].prio = pri_map;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return -EBUSY;

	rc = qed_dcbx_config_params(hwfn, ptt, &dcbx_set, 1);

	qed_ptt_release(hwfn, ptt);

	return rc;
}

static u8 qed_dcbnl_setdcbx(struct qed_dev *cdev, u8 mode)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_set dcbx_set;
	struct qed_ptt *ptt;
	int rc;

	DP_VERBOSE(hwfn, QED_MSG_DCB, "new mode = %x\n", mode);

	if (!(mode & DCB_CAP_DCBX_VER_IEEE) && !(mode & DCB_CAP_DCBX_VER_CEE)) {
		DP_INFO(hwfn, "Allowed modes are cee, ieee or static\n");
		return 1;
	}

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(hwfn, &dcbx_set);
	if (rc)
		return 1;

	dcbx_set.ver_num = DCBX_CONFIG_VERSION_STATIC;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return 1;

	rc = qed_dcbx_config_params(hwfn, ptt, &dcbx_set, 1);

	qed_ptt_release(hwfn, ptt);

	return rc;
}

static u8 qed_dcbnl_getfeatcfg(struct qed_dev *cdev, int featid, u8 * flags)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_get *dcbx_info;

	DP_VERBOSE(hwfn, QED_MSG_DCB, "Feature id  = %d\n", featid);
	dcbx_info = qed_dcbnl_get_dcbx(hwfn, QED_DCBX_OPERATIONAL_MIB);
	if (!dcbx_info)
		return 1;

	*flags = 0;
	switch (featid) {
	case DCB_FEATCFG_ATTR_PG:
		if (dcbx_info->operational.params.ets_enabled)
			*flags = DCB_FEATCFG_ENABLE;
		else
			*flags = DCB_FEATCFG_ERROR;
		break;
	case DCB_FEATCFG_ATTR_PFC:
		if (dcbx_info->operational.params.pfc.enabled)
			*flags = DCB_FEATCFG_ENABLE;
		else
			*flags = DCB_FEATCFG_ERROR;
		break;
	case DCB_FEATCFG_ATTR_APP:
		if (dcbx_info->operational.params.app_valid)
			*flags = DCB_FEATCFG_ENABLE;
		else
			*flags = DCB_FEATCFG_ERROR;
		break;
	default:
		DP_INFO(hwfn, "Invalid feature-ID %d\n", featid);
		kfree(dcbx_info);
		return 1;
	}

	DP_VERBOSE(hwfn, QED_MSG_DCB, "flags = %d\n", *flags);
	kfree(dcbx_info);

	return 0;
}

static u8 qed_dcbnl_setfeatcfg(struct qed_dev *cdev, int featid, u8 flags)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_set dcbx_set;
	bool enabled, willing;
	struct qed_ptt *ptt;
	int rc;

	DP_VERBOSE(hwfn, QED_MSG_DCB, "featid = %d flags = %d\n",
		   featid, flags);
	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(hwfn, &dcbx_set);
	if (rc)
		return 1;

	enabled = ! !(flags & DCB_FEATCFG_ENABLE);
	willing = ! !(flags & DCB_FEATCFG_WILLING);
	switch (featid) {
	case DCB_FEATCFG_ATTR_PG:
		dcbx_set.override_flags |= QED_DCBX_OVERRIDE_ETS_CFG;
		dcbx_set.config.params.ets_enabled = enabled;
		dcbx_set.config.params.ets_willing = willing;
		break;
	case DCB_FEATCFG_ATTR_PFC:
		dcbx_set.override_flags |= QED_DCBX_OVERRIDE_PFC_CFG;
		dcbx_set.config.params.pfc.enabled = enabled;
		dcbx_set.config.params.pfc.willing = willing;
		break;
	case DCB_FEATCFG_ATTR_APP:
		dcbx_set.override_flags |= QED_DCBX_OVERRIDE_APP_CFG;
		dcbx_set.config.params.app_willing = willing;
		break;
	default:
		DP_INFO(hwfn, "Invalid feature-ID %d\n", featid);
		return 1;
	}

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return 1;

	rc = qed_dcbx_config_params(hwfn, ptt, &dcbx_set, 1);

	qed_ptt_release(hwfn, ptt);

	return 0;
}

#ifdef DCB_CEE_SUPPORT		/* QED_UPSTREAM */
static int qed_dcbnl_peer_getappinfo(struct qed_dev *cdev,
				     struct dcb_peer_app_info *info,
				     u16 * app_count)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_get *dcbx_info;

	dcbx_info = qed_dcbnl_get_dcbx(hwfn, QED_DCBX_REMOTE_MIB);
	if (!dcbx_info)
		return -EINVAL;

	info->willing = dcbx_info->remote.params.app_willing;
	info->error = dcbx_info->remote.params.app_error;
	*app_count = dcbx_info->remote.params.num_app_entries;
	kfree(dcbx_info);

	return 0;
}

static int qed_dcbnl_peer_getapptable(struct qed_dev *cdev,
				      struct dcb_app *table)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_get *dcbx_info;
	int i;

	dcbx_info = qed_dcbnl_get_dcbx(hwfn, QED_DCBX_REMOTE_MIB);
	if (!dcbx_info)
		return -EINVAL;

	for (i = 0; i < dcbx_info->remote.params.num_app_entries; i++) {
		if (dcbx_info->remote.params.app_entry[i].ethtype)
			table[i].selector = DCB_APP_IDTYPE_ETHTYPE;
		else
			table[i].selector = DCB_APP_IDTYPE_PORTNUM;
		table[i].priority = dcbx_info->remote.params.app_entry[i].prio;
		table[i].protocol =
		    dcbx_info->remote.params.app_entry[i].proto_id;
	}

	kfree(dcbx_info);

	return 0;
}

static int qed_dcbnl_cee_peer_getpfc(struct qed_dev *cdev, struct cee_pfc *pfc)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_get *dcbx_info;
	int i;

	dcbx_info = qed_dcbnl_get_dcbx(hwfn, QED_DCBX_REMOTE_MIB);
	if (!dcbx_info)
		return -EINVAL;

	for (i = 0; i < QED_MAX_PFC_PRIORITIES; i++)
		if (dcbx_info->remote.params.pfc.prio[i])
			pfc->pfc_en |= BIT(i);

	pfc->tcs_supported = dcbx_info->remote.params.pfc.max_tc;
	DP_VERBOSE(hwfn, QED_MSG_DCB, "pfc state = %d tcs_supported = %d\n",
		   pfc->pfc_en, pfc->tcs_supported);
	kfree(dcbx_info);

	return 0;
}

static int qed_dcbnl_cee_peer_getpg(struct qed_dev *cdev, struct cee_pg *pg)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_get *dcbx_info;
	int i;

	dcbx_info = qed_dcbnl_get_dcbx(hwfn, QED_DCBX_REMOTE_MIB);
	if (!dcbx_info)
		return -EINVAL;

	pg->willing = dcbx_info->remote.params.ets_willing;
	for (i = 0; i < QED_MAX_PFC_PRIORITIES; i++) {
		pg->pg_bw[i] = dcbx_info->remote.params.ets_tc_bw_tbl[i];
		pg->prio_pg[i] = dcbx_info->remote.params.ets_pri_tc_tbl[i];
	}

	DP_VERBOSE(hwfn, QED_MSG_DCB, "willing = %d", pg->willing);
	kfree(dcbx_info);

	return 0;
}
#endif

static int qed_dcbnl_get_ieee_pfc(struct qed_dev *cdev,
				  struct ieee_pfc *pfc, bool remote)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_params *params;
	struct qed_dcbx_get *dcbx_info;
	int rc, i;

	dcbx_info = qed_dcbnl_get_dcbx(hwfn, QED_DCBX_OPERATIONAL_MIB);
	if (!dcbx_info)
		return -EINVAL;

	if (remote) {
		memset(dcbx_info, 0, sizeof(*dcbx_info));
		rc = qed_dcbx_query_params(hwfn, dcbx_info,
					   QED_DCBX_REMOTE_MIB);
		if (rc) {
			kfree(dcbx_info);
			return -EINVAL;
		}

		params = &dcbx_info->remote.params;
	} else {
		params = &dcbx_info->operational.params;
	}

	pfc->pfc_cap = params->pfc.max_tc;
	pfc->pfc_en = 0;
	for (i = 0; i < QED_MAX_PFC_PRIORITIES; i++)
		if (params->pfc.prio[i])
			pfc->pfc_en |= BIT(i);

	kfree(dcbx_info);

	return 0;
}

static int qed_dcbnl_ieee_getpfc(struct qed_dev *cdev, struct ieee_pfc *pfc)
{
	return qed_dcbnl_get_ieee_pfc(cdev, pfc, false);
}

static int qed_dcbnl_ieee_setpfc(struct qed_dev *cdev, struct ieee_pfc *pfc)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_set dcbx_set;
	struct qed_ptt *ptt;
	int rc, i;

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(hwfn, &dcbx_set);
	if (rc)
		return -EINVAL;

	dcbx_set.override_flags |= QED_DCBX_OVERRIDE_PFC_CFG;
	for (i = 0; i < QED_MAX_PFC_PRIORITIES; i++)
		dcbx_set.config.params.pfc.prio[i] = ! !(pfc->pfc_en & BIT(i));

	dcbx_set.config.params.pfc.max_tc = pfc->pfc_cap;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return -EINVAL;

	rc = qed_dcbx_config_params(hwfn, ptt, &dcbx_set, 1);

	qed_ptt_release(hwfn, ptt);

	return rc;
}

static int qed_dcbnl_get_ieee_ets(struct qed_dev *cdev,
				  struct ieee_ets *ets, bool remote)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_get *dcbx_info;
	struct qed_dcbx_params *params;
	int rc;

	dcbx_info = qed_dcbnl_get_dcbx(hwfn, QED_DCBX_OPERATIONAL_MIB);
	if (!dcbx_info)
		return -EINVAL;

	if (remote) {
		memset(dcbx_info, 0, sizeof(*dcbx_info));
		rc = qed_dcbx_query_params(hwfn, dcbx_info,
					   QED_DCBX_REMOTE_MIB);
		if (rc) {
			kfree(dcbx_info);
			return -EINVAL;
		}

		params = &dcbx_info->remote.params;
	} else {
		params = &dcbx_info->operational.params;
	}

	ets->ets_cap = params->max_ets_tc;
	ets->willing = params->ets_willing;
	ets->cbs = params->ets_cbs;
	memcpy(ets->tc_tx_bw, params->ets_tc_bw_tbl, sizeof(ets->tc_tx_bw));
	memcpy(ets->tc_tsa, params->ets_tc_tsa_tbl, sizeof(ets->tc_tsa));
	memcpy(ets->prio_tc, params->ets_pri_tc_tbl, sizeof(ets->prio_tc));
	kfree(dcbx_info);

	return 0;
}

static int qed_dcbnl_ieee_getets(struct qed_dev *cdev, struct ieee_ets *ets)
{
	return qed_dcbnl_get_ieee_ets(cdev, ets, false);
}

static int qed_dcbnl_ieee_setets(struct qed_dev *cdev, struct ieee_ets *ets)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_set dcbx_set;
	struct qed_ptt *ptt;
	int rc;

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(hwfn, &dcbx_set);
	if (rc)
		return -EINVAL;

	dcbx_set.override_flags |= QED_DCBX_OVERRIDE_ETS_CFG;
	dcbx_set.config.params.max_ets_tc = ets->ets_cap;
	dcbx_set.config.params.ets_willing = ets->willing;
	dcbx_set.config.params.ets_cbs = ets->cbs;
	memcpy(dcbx_set.config.params.ets_tc_bw_tbl, ets->tc_tx_bw,
	       sizeof(ets->tc_tx_bw));
	memcpy(dcbx_set.config.params.ets_tc_tsa_tbl, ets->tc_tsa,
	       sizeof(ets->tc_tsa));
	memcpy(dcbx_set.config.params.ets_pri_tc_tbl, ets->prio_tc,
	       sizeof(ets->prio_tc));

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return -EINVAL;

	rc = qed_dcbx_config_params(hwfn, ptt, &dcbx_set, 1);

	qed_ptt_release(hwfn, ptt);

	return rc;
}

static int qed_dcbnl_ieee_peer_getets(struct qed_dev *cdev,
				      struct ieee_ets *ets)
{
	return qed_dcbnl_get_ieee_ets(cdev, ets, true);
}

static int qed_dcbnl_ieee_peer_getpfc(struct qed_dev *cdev,
				      struct ieee_pfc *pfc)
{
	return qed_dcbnl_get_ieee_pfc(cdev, pfc, true);
}

#ifdef _IEEE_8021QAZ_APP	/* QED_UPSTREAM */
static int qed_get_sf_ieee_value(u8 selector, u8 * sf_ieee)
{
	switch (selector) {
	case IEEE_8021QAZ_APP_SEL_ETHERTYPE:
		*sf_ieee = QED_DCBX_SF_IEEE_ETHTYPE;
		break;
	case IEEE_8021QAZ_APP_SEL_STREAM:
		*sf_ieee = QED_DCBX_SF_IEEE_TCP_PORT;
		break;
	case IEEE_8021QAZ_APP_SEL_DGRAM:
		*sf_ieee = QED_DCBX_SF_IEEE_UDP_PORT;
		break;
	case IEEE_8021QAZ_APP_SEL_ANY:
		*sf_ieee = QED_DCBX_SF_IEEE_TCP_UDP_PORT;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int qed_dcbnl_ieee_getapp(struct qed_dev *cdev, struct dcb_app *app)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_get *dcbx_info;
	struct qed_app_entry *entry;
	u8 prio = 0;
	u8 sf_ieee;
	int i;

	DP_VERBOSE(hwfn, QED_MSG_DCB, "selector = %d protocol = %d\n",
		   app->selector, app->protocol);

	if (qed_get_sf_ieee_value(app->selector, &sf_ieee)) {
		DP_INFO(cdev, "Invalid selector field value %d\n",
			app->selector);
		return -EINVAL;
	}

	dcbx_info = qed_dcbnl_get_dcbx(hwfn, QED_DCBX_OPERATIONAL_MIB);
	if (!dcbx_info)
		return -EINVAL;

	for (i = 0; i < QED_DCBX_MAX_APP_PROTOCOL; i++) {
		entry = &dcbx_info->operational.params.app_entry[i];
		if ((entry->sf_ieee == sf_ieee) &&
		    (entry->proto_id == app->protocol)) {
			prio = entry->prio;
			break;
		}
	}

	if (i == QED_DCBX_MAX_APP_PROTOCOL) {
		DP_ERR(cdev, "App entry (%d, %d) not found\n", app->selector,
		       app->protocol);
		kfree(dcbx_info);
		return -EINVAL;
	}

	app->priority = ffs(prio) - 1;

	kfree(dcbx_info);

	return 0;
}

static int qed_dcbnl_ieee_setapp(struct qed_dev *cdev, struct dcb_app *app)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_dcbx_set dcbx_set;
	struct qed_app_entry *entry;
	struct qed_ptt *ptt;
	u8 sf_ieee;
	int rc, i;

	DP_VERBOSE(hwfn, QED_MSG_DCB, "selector = %d protocol = %d pri = %d\n",
		   app->selector, app->protocol, app->priority);
	if (app->priority >= QED_MAX_PFC_PRIORITIES) {
		DP_INFO(hwfn, "Invalid priority %d\n", app->priority);
		return -EINVAL;
	}

	if (qed_get_sf_ieee_value(app->selector, &sf_ieee)) {
		DP_INFO(cdev, "Invalid selector field value %d\n",
			app->selector);
		return -EINVAL;
	}

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(hwfn, &dcbx_set);
	if (rc)
		return -EINVAL;

	for (i = 0; i < QED_DCBX_MAX_APP_PROTOCOL; i++) {
		entry = &dcbx_set.config.params.app_entry[i];
		if ((entry->sf_ieee == sf_ieee) &&
		    (entry->proto_id == app->protocol))
			break;
		/* First empty slot */
		if (!entry->proto_id) {
			dcbx_set.config.params.num_app_entries++;
			break;
		}
	}

	if (i == QED_DCBX_MAX_APP_PROTOCOL) {
		DP_ERR(cdev, "App table is full\n");
		return -EBUSY;
	}

	dcbx_set.override_flags |= QED_DCBX_OVERRIDE_APP_CFG;
	dcbx_set.config.params.app_entry[i].sf_ieee = sf_ieee;
	dcbx_set.config.params.app_entry[i].proto_id = app->protocol;
	dcbx_set.config.params.app_entry[i].prio = BIT(app->priority);

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return -EBUSY;

	rc = qed_dcbx_config_params(hwfn, ptt, &dcbx_set, 1);

	qed_ptt_release(hwfn, ptt);

	return rc;
}
#endif

const struct qed_dcbnl_ops qed_dcbnl_ops_pass = {
	INIT_STRUCT_FIELD(getstate, qed_dcbnl_getstate),
	INIT_STRUCT_FIELD(setstate, qed_dcbnl_setstate),
	INIT_STRUCT_FIELD(getpgtccfgtx, qed_dcbnl_getpgtccfgtx),
	INIT_STRUCT_FIELD(getpgbwgcfgtx, qed_dcbnl_getpgbwgcfgtx),
	INIT_STRUCT_FIELD(getpgtccfgrx, qed_dcbnl_getpgtccfgrx),
	INIT_STRUCT_FIELD(getpgbwgcfgrx, qed_dcbnl_getpgbwgcfgrx),
	INIT_STRUCT_FIELD(getpfccfg, qed_dcbnl_getpfccfg),
	INIT_STRUCT_FIELD(setpfccfg, qed_dcbnl_setpfccfg),
	INIT_STRUCT_FIELD(getcap, qed_dcbnl_getcap),
	INIT_STRUCT_FIELD(getnumtcs, qed_dcbnl_getnumtcs),
	INIT_STRUCT_FIELD(getpfcstate, qed_dcbnl_getpfcstate),
	INIT_STRUCT_FIELD(getdcbx, qed_dcbnl_getdcbx),
	INIT_STRUCT_FIELD(setpgtccfgtx, qed_dcbnl_setpgtccfgtx),
	INIT_STRUCT_FIELD(setpgtccfgrx, qed_dcbnl_setpgtccfgrx),
	INIT_STRUCT_FIELD(setpgbwgcfgtx, qed_dcbnl_setpgbwgcfgtx),
	INIT_STRUCT_FIELD(setpgbwgcfgrx, qed_dcbnl_setpgbwgcfgrx),
	INIT_STRUCT_FIELD(setall, qed_dcbnl_setall),
	INIT_STRUCT_FIELD(setnumtcs, qed_dcbnl_setnumtcs),
	INIT_STRUCT_FIELD(setpfcstate, qed_dcbnl_setpfcstate),
	INIT_STRUCT_FIELD(setapp, qed_dcbnl_setapp),
	INIT_STRUCT_FIELD(setdcbx, qed_dcbnl_setdcbx),
	INIT_STRUCT_FIELD(setfeatcfg, qed_dcbnl_setfeatcfg),
	INIT_STRUCT_FIELD(getfeatcfg, qed_dcbnl_getfeatcfg),
	INIT_STRUCT_FIELD(getapp, qed_dcbnl_getapp),
#ifdef DCB_CEE_SUPPORT		/* QED_UPSTREAM */
	INIT_STRUCT_FIELD(peer_getappinfo, qed_dcbnl_peer_getappinfo),
	INIT_STRUCT_FIELD(peer_getapptable, qed_dcbnl_peer_getapptable),
	INIT_STRUCT_FIELD(cee_peer_getpfc, qed_dcbnl_cee_peer_getpfc),
	INIT_STRUCT_FIELD(cee_peer_getpg, qed_dcbnl_cee_peer_getpg),
#endif
	INIT_STRUCT_FIELD(ieee_getpfc, qed_dcbnl_ieee_getpfc),
	INIT_STRUCT_FIELD(ieee_setpfc, qed_dcbnl_ieee_setpfc),
	INIT_STRUCT_FIELD(ieee_getets, qed_dcbnl_ieee_getets),
	INIT_STRUCT_FIELD(ieee_setets, qed_dcbnl_ieee_setets),
	INIT_STRUCT_FIELD(ieee_peer_getpfc, qed_dcbnl_ieee_peer_getpfc),
	INIT_STRUCT_FIELD(ieee_peer_getets, qed_dcbnl_ieee_peer_getets),
#ifdef _IEEE_8021QAZ_APP	/* QED_UPSTREAM */
	INIT_STRUCT_FIELD(ieee_getapp, qed_dcbnl_ieee_getapp),
	INIT_STRUCT_FIELD(ieee_setapp, qed_dcbnl_ieee_setapp),
#endif
};
