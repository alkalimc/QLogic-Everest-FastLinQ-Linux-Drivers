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

#define __PREVENT_INT_ATTN__
#define __PREVENT_CHIP_NAMES__
#define __PREVENT_REG_TYPE_NAMES__
#define __PREVENT_DUMP_MEM_ARR__
#define __PREVENT_PXP_GLOBAL_WIN__
#define __PREVENT_COND_ARR__

#include <linux/msi.h>
#include <linux/etherdevice.h>
#include "qed_tests.h"
#include "qed_hw.h"
#include "qed_mcp.h"
#include "qed_reg_addr.h"
#include "qed_chain.h"
#include "qed_selftest.h"
#include "qed_rdma.h"
#include "qed_dcbx.h"
#include "qed_hsi.h"

extern int qed_qm_reconf(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
extern void qed_dp_init_qm_params(struct qed_hwfn *p_hwfn);

int qed_dmae_err_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	qed_dmae_err(p_hwfn, p_ptt);

	return 0;
}

int qed_fw_assert_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	qed_sp_fw_assert(p_hwfn);

	return 0;
}

int qed_qm_reconf_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{

	qed_qm_reconf(p_hwfn, p_ptt);

	return 0;
}

int qed_ets_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct init_ets_req ets = {0};
	int i;

	for (i = 0; i < NUM_OF_TCS; i++){
		ets.tc_req[i].use_sp = 0;
		ets.tc_req[i].use_wfq = 1;
		ets.tc_req[i].weight = 1;
	}

	ets.tc_req[1].weight = 4;
	/* qed_init_nig_ets(p_hwfn, p_ptt, &ets, false); */

	return 0;
}

int qed_phony_dcbx_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 load_code, param;

	qed_mcp_cmd(p_hwfn, p_hwfn->p_main_ptt, DRV_MSG_CODE_SET_DCBX,
		    1 << DRV_MB_PARAM_DCBX_NOTIFY_OFFSET, &load_code, &param);

	return 0;
}

int qed_mcp_halt_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	qed_mcp_halt(p_hwfn, p_ptt);
	return 0;
}

int qed_mcp_resume_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	qed_mcp_resume(p_hwfn, p_ptt);
	return 0;
}

int qed_mcp_mask_parities_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	return qed_mcp_mask_parities(p_hwfn, p_ptt, 1);
}

int qed_mcp_unmask_parities_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	return qed_mcp_mask_parities(p_hwfn, p_ptt, 0);
}

int qed_test_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u32 rc)
{
	printk("test\n");
	return rc;
}

int qed_coal_vf_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		     u16 rx_coal, u16 tx_coal, u16 vf_id)
{
	int qid;

	for (qid = 0; qid < 16; qid++) {
		qed_iov_pf_configure_vf_queue_coalesce(p_hwfn, rx_coal,
						       tx_coal, vf_id, qid);
	}

	return 0;
}

int qed_gen_process_kill_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      u8 is_common_block)
{
	qed_gen_process_kill(p_hwfn, p_ptt, is_common_block);

	return 0;
}

int qed_gen_system_kill_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	qed_gen_system_kill(p_hwfn, p_ptt);

	return 0;
}

int qed_trigger_recovery_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	qed_start_recovery_process(p_hwfn, p_ptt);

	return 0;
}

/* Mask/Unmask a specific MSI-X vectorr; Required for integration test of the 'interrupt_test'. */
int qed_msix_vector_mask_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u8 vector, u8 b_mask)
{
#ifndef CONFIG_XEN
	struct msi_desc *entry = NULL;
	u32 __iomem *ptr, val, new_val;
#ifdef _HAS_MSI_LIST /* ! QED_UPSTREAM */
	struct pci_dev *pdev = p_hwfn->cdev->pdev;

	entry = list_first_entry(&pdev->msi_list, struct msi_desc, list);
#endif
	if (!entry)
		return -EINVAL;

#ifdef _PCI_MSI_DESC_HAS_MASK_BASE /* QED_UPSTREAM */
	ptr = entry->pci.mask_base + vector * 4 * sizeof(u32);
#else
	ptr = entry->mask_base + vector * 4 * sizeof(u32);
#endif

	val = ptr[3];
	new_val = ptr[3] & 0xfffffffe;

	if (b_mask)
		new_val |= 0x1;

	printk(KERN_ERR "MSI-X[%d]: %08x:%08x:%08x:%08x -> %08x:%08x:%08x:%08x\n",
	       vector, ptr[0], ptr[1], ptr[2], val, ptr[0], ptr[1], ptr[2], new_val);

	ptr[3] = new_val;
#endif
	return 0;
}

/* Copied from drivers/pci/pci.h */
static inline void pci_msix_clear_and_set_ctrl(struct pci_dev *dev, u16 clear, u16 set)
{
	u16 msix_cap = pci_find_capability(dev, PCI_CAP_ID_MSIX);
	u16 ctrl;

	pci_read_config_word(dev, msix_cap + PCI_MSIX_FLAGS, &ctrl);
	ctrl &= ~clear;
	ctrl |= set;
	pci_write_config_word(dev, msix_cap + PCI_MSIX_FLAGS, ctrl);
}

/* Mask/Unmask the MSI-X pci capability; Required for integration test of the `interrupt_test'. */
int qed_msix_mask_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u8 b_mask)
{
	struct pci_dev *pdev = p_hwfn->cdev->pdev;

	if (b_mask)
		pci_msix_clear_and_set_ctrl(pdev, 0, PCI_MSIX_FLAGS_MASKALL);
	else
		pci_msix_clear_and_set_ctrl(pdev, PCI_MSIX_FLAGS_MASKALL, 0);

	DP_NOTICE(p_hwfn, "%s MSI-X interrupts\n", b_mask ? "Masked" : "Unmasked");

	return 0;
}

/* Disable/Enable the MSI-X pci capability; Required for integration of the `interrupt_test'. */
int qed_msix_disable_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u8 b_disable)
{
	struct pci_dev *pdev = p_hwfn->cdev->pdev;

	if (b_disable)
		pci_msix_clear_and_set_ctrl(pdev, PCI_MSIX_FLAGS_ENABLE, 0);
	else
		pci_msix_clear_and_set_ctrl(pdev, 0, PCI_MSIX_FLAGS_ENABLE);

	DP_NOTICE(p_hwfn, "%s MSI-X interrupts\n", b_disable ? "Disabled" : "Enabled");

	return 0;
}

/* Configure OBFF FSM; Required for integration of the 'OBFF test'. */
int qed_config_obff_fsm_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 val;

	/* Read register CPMU_REG_OBFF_MODE_CONTROL, modify bit 1 (OBFF_ENGINE_IDLE_EN) to 0 */
	val = qed_rd(p_hwfn, p_ptt, CPMU_REG_OBFF_MODE_CONTROL);
	val &= ~(0x1 << 1);

	/* When the OBFF state changes to OBFF/IDLE, are there already VQs which are not empty?
	 * If yes, these VQs prevent the FSM from getting into the STALL state.
	 * Write value of 0 to bits 8 and 9 of the CPMU_REG_OBFF_MODE_CONTROL register to not to
	 * condition the FSM transfer to stall state with VOs not empty.
	 */
	val &= ~(0x11 << 9);

	qed_wr(p_hwfn, p_ptt, CPMU_REG_OBFF_MODE_CONTROL, val);

	/* Write 0xFFFF_FFFF to registers
	 *  CPMU_REG_OBFF_MEM_TIMER_SHORT_THRESHOLD (Offset: 0x22C; Width: 32)
	 *  CPMU_REG_OBFF_MEM_TIMER_LONG_THRESHOLD (Offset: 0x230; Width: 32)
	 *  CPMU_REG_OBFF_INT_TIMER_SHORT_THRESHOLD (Offset: 0x234; Width: 32)
	 *  CPMU_REG_OBFF_INT_TIMER_LONG_THRESHOLD (Offset: 0x238; Width: 32)
	 *  	[Set the thresholds to infinite value]
	 */
	qed_wr(p_hwfn, p_ptt, CPMU_REG_OBFF_MEM_TIMER_SHORT_THRESHOLD, 0xFFFFFFFF);
	qed_wr(p_hwfn, p_ptt, CPMU_REG_OBFF_MEM_TIMER_LONG_THRESHOLD, 0xFFFFFFFF);
	qed_wr(p_hwfn, p_ptt, CPMU_REG_OBFF_INT_TIMER_SHORT_THRESHOLD, 0xFFFFFFFF);
	qed_wr(p_hwfn, p_ptt, CPMU_REG_OBFF_INT_TIMER_LONG_THRESHOLD, 0xFFFFFFFF);

	/* Write 0xFFFF_FFFF to registers
	 *  CPMU_REG_OBFF_STALL_ON_IDLE_STATE_0 (Offset: 0x244; Width: 32)
	 *  CPMU_REG_OBFF_STALL_ON_IDLE_STATE_1 (Offset: 0x248; Width: 32)
	 *  [Causes all VQs in IDLE state to use the counters with the infinite values].
	 */
	qed_wr(p_hwfn, p_ptt, CPMU_REG_OBFF_STALL_ON_IDLE_STATE_0, 0xFFFFFFFF);
	qed_wr(p_hwfn, p_ptt, CPMU_REG_OBFF_STALL_ON_IDLE_STATE_1, 0xFFFFFFFF);

	/* Write to 0x20_0000 to register CPMU_REG_OBFF_STALL_ON_OBFF_STATE_0(Offset: 0x23C;
	 *  Width: 32) [Causes IGU VQ #0x15 in OBFF state to use the counter with the infinite
	 *  values; all other VQs the stall state is removed immediately (if VQ not empty].
	 */
	qed_wr(p_hwfn, p_ptt, CPMU_REG_OBFF_STALL_ON_OBFF_STATE_0, 0x200000);

	/* Write 0x1 to register CPMU_REG_OBFF_MODE_CONFIG (Offset: 0x220; Width: 32)
	 * [enable OBFF].
	 */
	qed_wr(p_hwfn, p_ptt, CPMU_REG_OBFF_MODE_CONFIG, 0x1);

	/* Read register BRB_REG_OUT_IF_ENABLE (Offset: 0xF2C; Width: 32), modify bit 25
	 *  (PM_OUT_IF_EN) to 0 [Disables BRB above threshold indication].
	 */
	val = qed_rd(p_hwfn, p_ptt, BRB_REG_OUT_IF_ENABLE);
	val &= ~(0x1 << 25);
	qed_wr(p_hwfn, p_ptt, BRB_REG_OUT_IF_ENABLE, val);

	DP_NOTICE(p_hwfn, "Configured OBFF FSM\n");

	return 0;
}

/* Print OBFF statistics; Required for integration of the 'OBFF test'. */
int qed_dump_obff_stats_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{

	DP_NOTICE(p_hwfn, "STALL_MEM_STAT = %x\n", qed_rd(p_hwfn, p_ptt, CPMU_REG_OBFF_STALL_MEM_STAT));
	DP_NOTICE(p_hwfn, "STALL_MEM_DURATION_STAT = %x\n", qed_rd(p_hwfn, p_ptt, CPMU_REG_OBFF_STALL_MEM_DURATION_STAT));
	DP_NOTICE(p_hwfn, "STALL_INT_STAT = %x\n", qed_rd(p_hwfn, p_ptt, CPMU_REG_OBFF_STALL_INT_STAT));
	DP_NOTICE(p_hwfn, "STALL_INT_DURATION_STAT  = %x\n", qed_rd(p_hwfn, p_ptt, CPMU_REG_OBFF_STALL_INT_DURATION_STAT));

	return 0;
}

/* Set OBFF state; Required for integration of the 'OBFF test'. */
int qed_set_obff_state_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u8 state)
{
	u32 val;

	/* Read register CPMU_REG_OBFF_MODE_CONTROL */
	val = qed_rd(p_hwfn, p_ptt, CPMU_REG_OBFF_MODE_CONTROL);

	switch (state) {
	case 0:
		/* Modify bit 7-6 (OBFF_ENGINE_IDLE_EN) to 0x0 */
		val &= ~(0x1 << 6);
		val &= ~(0x1 << 7);
		DP_NOTICE(p_hwfn, "OBFF is set to ‘cpu active’ mode\n");
		break;
	case 1:
		/* Modify bit 7-6 (OBFF_ENGINE_IDLE_EN) to 0x1 */
		val |= (0x1 << 6);
		val &= ~(0x1 << 7);
		DP_NOTICE(p_hwfn, "OBFF is set to ‘cpu obff’ mode\n");
		break;
	case 2:
		/* Modify bit 7-6 (OBFF_ENGINE_IDLE_EN) to 0x2 */
		val &= ~(0x1 << 6);
		val |= (0x1 << 7);
		DP_NOTICE(p_hwfn, "OBFF is set to ‘cpu idle’ mode\n");
		break;
	default:
		DP_NOTICE(p_hwfn, "Invalid OBFF state value\n");
		return -1;
	}

	qed_wr(p_hwfn, p_ptt, CPMU_REG_OBFF_MODE_CONTROL, val);
	return 0;
}

int qed_ramrod_flood_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  u32 ramrod_amount, u8 blocking)
{
	struct qed_spq_entry *p_ent = NULL;
	struct qed_sp_init_data init_data;
	int rc = 0, i;

	for (i = 0; i < ramrod_amount; i++) {
		DP_NOTICE(p_hwfn, "about to send empty ramrod %d\n", i);

		memset(&init_data, 0, sizeof(init_data));
		init_data.cid = qed_spq_get_cid(p_hwfn);
		init_data.opaque_fid = p_hwfn->hw_info.opaque_fid;
		init_data.comp_mode =
			blocking ? QED_SPQ_MODE_EBLOCK : QED_SPQ_MODE_CB;

		rc = qed_sp_init_request(p_hwfn, &p_ent,
					 COMMON_RAMROD_EMPTY, PROTOCOLID_COMMON,
					 &init_data);
		if (rc)
			return rc;

		rc = qed_spq_post(p_hwfn, p_ent, NULL);
		if (rc) {
			if (blocking)
				DP_NOTICE(p_hwfn, "post failed for ramrod %d\n",
					  i);
			else
				return rc;
		}
	}

	return rc;
}

int qed_gen_ramrod_stuck_test(struct qed_hwfn *p_hwfn)
{
	qed_spq_drop_next_completion(p_hwfn);

	return 0;
}

int qed_gen_fan_failure_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			     u8 is_over_temp)
{
	u32 mcp_resp, mcp_param;

	return qed_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_INDUCE_FAILURE,
			   is_over_temp ? DRV_MSG_TEMPERATURE_FAILURE_TYPE
					: DRV_MSG_FAN_FAILURE_TYPE,
			   &mcp_resp, &mcp_param);
}

int qed_bist_register_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	return qed_mcp_bist_register_test(p_hwfn, p_ptt);
}

int qed_bist_clock_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	return qed_mcp_bist_clock_test(p_hwfn, p_ptt);

}

int qed_bist_nvm_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	return qed_selftest_nvram(p_hwfn->cdev);
}

int qed_get_temperature_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_temperature_sensor *p_temp_sensor;
	struct qed_temperature_info temp_info;
	int rc, i;

	rc = qed_mcp_get_temperature_info(p_hwfn, p_ptt, &temp_info);
	if (rc)
		return rc;

	DP_NOTICE(p_hwfn, "Number of sensors %d\n",  temp_info.num_sensors);
	for (i = 0; i < temp_info.num_sensors; i++) {
		p_temp_sensor = &temp_info.sensors[i];
		DP_NOTICE(p_hwfn,
			  "[sensor %d] sensor_location %hhu, threshold_high %hhu, critical %hhu, current_temp %hhu\n",
			  i, p_temp_sensor->sensor_location,
			  p_temp_sensor->threshold_high,
			  p_temp_sensor->critical,
			  p_temp_sensor->current_temp);
	}

	return 0;
}

int qed_get_mba_versions_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mba_vers mba_vers;
	char *str_arr[QED_MAX_NUM_OF_ROMIMG] = {
		"legacy MBA", "PCI30_CLP MBA", "PCI30 MBA", "FCODE",
		"EFI x86", "EFI IPF", "EFI EBC", "EFI x64"
	};
	int rc, i, found = 0;

	rc = qed_mcp_get_mba_versions(p_hwfn, p_ptt, &mba_vers);
	if (rc)
		return rc;

	for (i = 0; i < QED_MAX_NUM_OF_ROMIMG; i++) {
		if (mba_vers.mba_vers[i] == 0)
			continue;

		if (found == 0) {
			DP_NOTICE(p_hwfn, "MBA versions:\n");
			found = 1;
		}

		if (i < QED_EFI_X86_IDX)
			DP_NOTICE(p_hwfn, "  %s %d.%d.%d\n",
				  str_arr[i],
				  (mba_vers.mba_vers[i] & 0xff000) >> 12,
				  (mba_vers.mba_vers[i] & 0x0f00) >> 8,
				  mba_vers.mba_vers[i] & 0xff);
		else
			DP_NOTICE(p_hwfn, "  %s %d.%d.%d.%d\n",
				  str_arr[i],
				  (mba_vers.mba_vers[i] & 0xf0000) >> 16,
				  (mba_vers.mba_vers[i] & 0xf000) >> 12,
				  (mba_vers.mba_vers[i] & 0x0f00) >> 8,
				  mba_vers.mba_vers[i] & 0xff);
	}

	if (found == 0)
		DP_NOTICE(p_hwfn, "No MBA versions can be found.\n");

	return 0;
}

int qed_mcp_resc_lock_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			   u8 resource, u8 timeout)
{
	struct qed_resc_lock_params resc_lock_params;
	int rc;

	qed_mcp_resc_lock_default_init(p_hwfn, &resc_lock_params, NULL,
				       resource, false);
	resc_lock_params.timeout = timeout;
	rc = qed_mcp_resc_lock(p_hwfn, p_ptt, &resc_lock_params);
	if (rc)
		DP_NOTICE(p_hwfn, "qed_mcp_resc_lock failed. rc = %d.\n", rc);

	DP_NOTICE(p_hwfn,
		  "Resource Lock: resc %d, timeout %d, granted %d, owner %d\n",
		  resc_lock_params.resource, resc_lock_params.timeout,
		  resc_lock_params.b_granted, resc_lock_params.owner);

	return rc;
}

int qed_mcp_resc_unlock_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			     u8 resource, u8 force)
{
	struct qed_resc_unlock_params resc_unlock_params;
	int rc;

	qed_mcp_resc_lock_default_init(p_hwfn, NULL, &resc_unlock_params,
				       resource, false);
	resc_unlock_params.b_force = !!force;
	rc = qed_mcp_resc_unlock(p_hwfn, p_ptt, &resc_unlock_params);
	if (rc)
		DP_NOTICE(p_hwfn, "qed_mcp_resc_unlock failed. rc = %d.\n", rc);

	DP_NOTICE(p_hwfn, "Resource Unlock: resc %d, released %d\n",
		  resc_unlock_params.resource, resc_unlock_params.b_released);

	return rc;
}

int qed_read_dpm_register_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			       u32 hw_addr)
{
	/* Note that the register's value is unsigned while we return a signed
	 * value. Also, there's a double use of '-1' that can mean either an
	 * error or the value 0xFFFFFFFF.
	 */
	if ((hw_addr != DORQ_REG_DPM_ROCE_SUCCESS_CNT) &&
	    (hw_addr != DORQ_REG_DPM_LEG_SUCCESS_CNT) &&
	    (hw_addr != DORQ_REG_DPM_ABORT_CNT) &&
	    (hw_addr != DORQ_REG_DB_DROP_CNT))
		return -1;
	else
		return qed_rd(p_hwfn, p_ptt, hw_addr);
}

#ifdef CONFIG_IWARP

int qed_iwarp_tcp_cids_weight_test(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt)
{
	int weight = 0;

	weight = bitmap_weight(p_hwfn->p_rdma_info->tcp_cid_map.bitmap,
			       p_hwfn->p_rdma_info->tcp_cid_map.max_count);

	return p_hwfn->p_rdma_info->tcp_cid_map.max_count-weight;
}

static void qed_iwarp_ep_list_print_ep_info(struct qed_hwfn *p_hwfn,
					    struct qed_iwarp_ep *ep)
{
	struct qed_iwarp_cm_info *cm_info = &ep->cm_info;

	DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) State: %d\n",
		  ep->tcp_cid, ep->cid, ep->state);

	DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) connect_mode: %s\n",
		  ep->tcp_cid, ep->cid,
		  (ep->connect_mode == TCP_CONNECT_PASSIVE) ? "PASSIVE" :
		  "ACTIVE");

	DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) rtr_type: %d\n",
		  ep->tcp_cid, ep->cid, ep->rtr_type);

	DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) mpa_rev: %d\n",
		  ep->tcp_cid, ep->cid, ep->mpa_rev);

	DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) mss: %d\n",
		  ep->tcp_cid, ep->cid, ep->mss);

	if (ep->connect_mode == TCP_CONNECT_ACTIVE) {
		DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) mpa_reply_processed: %d\n",
			  ep->tcp_cid, ep->cid, ep->mpa_reply_processed);
	}

	DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) Remote mac: [%pM]\n",
		  ep->tcp_cid, ep->cid, ep->remote_mac_addr);

	DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) local mac: [%pM]\n",
		  ep->tcp_cid, ep->cid, ep->local_mac_addr);

	DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) ip_version = %d\n",
		  ep->tcp_cid, ep->cid, cm_info->ip_version);
	DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) remote_ip %x.%x.%x.%x\n",
		  ep->tcp_cid, ep->cid,
		  cm_info->remote_ip[0],
		  cm_info->remote_ip[1],
		  cm_info->remote_ip[2],
		  cm_info->remote_ip[3]);
	DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) local_ip %x.%x.%x.%x\n",
		  ep->tcp_cid, ep->cid,
		  cm_info->local_ip[0],
		  cm_info->local_ip[1],
		  cm_info->local_ip[2],
		  cm_info->local_ip[3]);
	DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) remote_port = %x\n",
		  ep->tcp_cid, ep->cid,
		  cm_info->remote_port);
	DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) local_port = %x\n",
		  ep->tcp_cid, ep->cid,
		  cm_info->local_port);
	DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) vlan = %x\n",
		  ep->tcp_cid, ep->cid,
		  cm_info->vlan);
	DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) private_data_len = %x\n",
		  ep->tcp_cid, ep->cid,
		  cm_info->private_data_len);
	DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) ord = %d\n",
		  ep->tcp_cid, ep->cid,
		  cm_info->ord);
	DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) ird = %d\n",
		  ep->tcp_cid, ep->cid,
		  cm_info->ird);
}

int qed_iwarp_ep_free_list_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_iwarp_ep *ep;
	int num_free = 0;
	int invalid_cid = 0;

	DP_NOTICE(p_hwfn, "EP Free List\n");
	list_for_each_entry(ep,
			    &p_hwfn->p_rdma_info->iwarp.ep_free_list,
			    list_entry) {
		num_free++;
		DP_NOTICE(p_hwfn, "(0x%x)\n", ep->tcp_cid);
		if (ep->tcp_cid == 0xffffffff)
			invalid_cid++;
	}
	DP_NOTICE(p_hwfn, "num free=%d\n", num_free);

	return invalid_cid;
}

int qed_iwarp_ep_active_list_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt)
{
	struct qed_iwarp_ep *ep;
	int cnt = 0;

	DP_NOTICE(p_hwfn, "EP Active List\n");
	list_for_each_entry(ep,
			    &p_hwfn->p_rdma_info->iwarp.ep_list,
			    list_entry) {
		DP_NOTICE(p_hwfn, "TCP_CID(0x%x) CID(0x%x) State: %d\n",
			  ep->tcp_cid, ep->cid, ep->state);
		qed_iwarp_ep_list_print_ep_info(p_hwfn, ep);
		cnt++;
	}

	return cnt;
}

#define IS_IWARP(_p_hwfn) (_p_hwfn->p_rdma_info->proto == PROTOCOLID_IWARP)

int qed_iwarp_create_listen_test(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt, u32 ip_addr, u32 port)
{
	struct qed_iwarp_listen_in iparams = {NULL};
	struct qed_iwarp_listen_out oparams = {NULL};
	struct qed_iwarp_listener *listener = NULL;
	int rc;

	DP_NOTICE(p_hwfn, "Create Listener Test\n");

	if (!IS_IWARP(p_hwfn)) {
		DP_NOTICE(p_hwfn, "Test called on RoCE!\n");
		return -1;
	}

	iparams.cb_context = NULL;
	iparams.event_cb = NULL;
	iparams.ip_addr[0] = ip_addr;
	iparams.ip_version = 4;
	iparams.port = port;
	ether_addr_copy(iparams.mac_addr, p_hwfn->p_rdma_info->iwarp.mac_addr);

	rc = qed_iwarp_create_listen(p_hwfn, &iparams, &oparams);
	if (rc)
		return rc;

	listener = oparams.handle;
	if (rc || !listener) {
		DP_NOTICE(p_hwfn, "Failed in creating listener\n");
		return rc;
	}
	listener->drop = true;
	DP_NOTICE(p_hwfn, "ListenerHandle %x %x\n",
		  PTR_HI(listener), PTR_LO(listener));

	return 0;
}

int qed_iwarp_remove_listen_test(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt,
				 u32 handle_hi, u32 handle_lo)
{
	struct qed_iwarp_listener *listener = NULL;

	DP_NOTICE(p_hwfn, "Remove Listener Test\n");

	if (!IS_IWARP(p_hwfn)) {
		DP_NOTICE(p_hwfn, "Test called on RoCE!\n");
		return -1;
	}

	listener = (struct qed_iwarp_listener *)(uintptr_t)HILO_64(handle_hi,
								   handle_lo);
	DP_NOTICE(p_hwfn, "listener to be removed: %p\n", listener);
	qed_iwarp_destroy_listen(p_hwfn, listener);

	return 0;
}

static void qed_iwarp_print_listener_info(struct qed_hwfn *p_hwfn,
					  struct qed_iwarp_listener *listener)
{
	if (!listener)
		return;

	DP_NOTICE(p_hwfn, "Listener(0x%p) IP(0x%x:0x%x:0x%x:0x%x) PORT(0x%x) Vlan(0x%x) Drop(0x%x)\n",
		  listener, listener->ip_addr[0], listener->ip_addr[1],
		  listener->ip_addr[2], listener->ip_addr[3],
		  listener->port, listener->vlan, listener->drop);
}

int qed_iwarp_listeners_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_iwarp_listener *listener;
	int cnt = 0;

	list_for_each_entry(listener,
			    &p_hwfn->p_rdma_info->iwarp.listen_list,
			    list_entry) {
		qed_iwarp_print_listener_info(p_hwfn, listener);
		cnt++;
	}

	return cnt;
}
#endif

int qed_rdma_query_stats_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_rdma_stats_out_params out_params;
	int rc;

	rc = qed_rdma_query_stats(p_hwfn, 0, &out_params);

	return rc;
}

int qed_db_recovery_dp_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	qed_db_recovery_dp(p_hwfn);

	return 0;
}

int qed_db_recovery_execute_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	qed_db_recovery_execute(p_hwfn);

	return 0;
}

int qed_dscp_pfc_get_enable_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	return p_hwfn->p_dcbx_info->get.dscp.enabled;
}

int qed_dscp_pfc_enable_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			     u8 enable)
{
	struct qed_dcbx_set dcbx_set;
	int rc;

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(p_hwfn, &dcbx_set);
	if (rc)
		return rc;

	dcbx_set.override_flags |= QED_DCBX_OVERRIDE_DSCP_CFG;
	dcbx_set.dscp.enabled = !!enable;

	return qed_dcbx_config_params(p_hwfn, p_ptt, &dcbx_set,
				      !!p_hwfn->cdev->b_dcbx_cfg_commit);
}

int qed_dscp_pfc_get_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  u8 index)
{
	if (index >= QED_DCBX_DSCP_SIZE) {
		DP_ERR(p_hwfn, "Invalid dscp index %d\n", index);
		return -EINVAL;
	}

	return p_hwfn->p_dcbx_info->get.dscp.dscp_pri_map[index];
}

int qed_dscp_pfc_set_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  u8 index, u8 pri_val)
{
	struct qed_dcbx_set dcbx_set;
	int rc;

	if (index >= QED_DCBX_DSCP_SIZE ||
	    pri_val >= QED_MAX_PFC_PRIORITIES) {
		DP_ERR(p_hwfn, "Invalid dscp params: index = %d pri = %d\n",
		       index, pri_val);
		return -EINVAL;
	}

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(p_hwfn, &dcbx_set);
	if (rc)
		return rc;

	dcbx_set.override_flags |= QED_DCBX_OVERRIDE_DSCP_CFG;
	dcbx_set.dscp.dscp_pri_map[index] = pri_val;

	return qed_dcbx_config_params(p_hwfn, p_ptt, &dcbx_set,
				      !!p_hwfn->cdev->b_dcbx_cfg_commit);
}

#define U32_NIBBLES 8
#define NIBBLE_SIZE 4
#define NIBBLE_MASK 0xf

int qed_dscp_pfc_batch_get_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  u8 index)
{
	int res = 0, i;
	u8 dscp_val;

	/* prepare a u32 with a priority in each nibble staring form provided index */
	for (i = 0; i < U32_NIBBLES; i++) {
		if (qed_dcbx_get_dscp_priority(p_hwfn, index + i, &dscp_val)) {
			DP_INFO(p_hwfn, "Failed to get dscp value for %d\n", index);
			return -1;
		}

		res |= dscp_val << i * NIBBLE_SIZE;
		DP_VERBOSE(p_hwfn, QED_MSG_SP, "res was %x, i was %d, dscp val was %x\n", res, i, dscp_val);
	}

	return res;
}

int qed_dscp_pfc_batch_set_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  u8 index, u32 prios)
{
	int rc, i;
	u8 pri_val = 0;

	/* prios 8 priorities, one in each nibble */
	for (i = 0; i < U32_NIBBLES; i++) {
		pri_val = (prios >> (i * NIBBLE_SIZE)) & NIBBLE_MASK;
		rc = qed_dcbx_set_dscp_priority(p_hwfn, p_ptt, index + i, pri_val);
		if (rc)
			DP_INFO(p_hwfn, "Failed to set proirity %d at %d\n",
				pri_val, index);
	}

	return rc;
}

int qed_dcbx_get_mode_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	if (!p_hwfn->p_dcbx_info->get.operational.valid) {
		DP_ERR(p_hwfn, "Dcbx parameters not available\n");
		return 0;
	}

	if (p_hwfn->p_dcbx_info->get.operational.ieee)
		return DCB_CAP_DCBX_VER_IEEE;
	else if (p_hwfn->p_dcbx_info->get.operational.cee)
		return DCB_CAP_DCBX_VER_CEE;
	else if (p_hwfn->p_dcbx_info->get.operational.local)
		return DCB_CAP_DCBX_STATIC;

	return 0;
}

int qed_dcbx_set_mode_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			   u8 mode)
{
	struct qed_dcbx_set dcbx_set;
	int rc;

	DP_ERR(p_hwfn, "new mode = %x\n", mode);

	if ((mode & (DCB_CAP_DCBX_VER_IEEE | DCB_CAP_DCBX_VER_CEE |
		     DCB_CAP_DCBX_STATIC)) != mode) {
		DP_ERR(p_hwfn, "Allowed modes are cee, ieee, static or disable\n");
		return -1;
	}

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(p_hwfn, &dcbx_set);
	if (rc)
		return rc;

	dcbx_set.ver_num = 0;
	if (mode & DCB_CAP_DCBX_VER_CEE) {
		dcbx_set.ver_num |= DCBX_CONFIG_VERSION_CEE;
		dcbx_set.enabled = true;
	}

	if (mode & DCB_CAP_DCBX_VER_IEEE) {
		dcbx_set.ver_num |= DCBX_CONFIG_VERSION_IEEE;
		dcbx_set.enabled = true;
	}

	if (mode & DCB_CAP_DCBX_STATIC) {
		dcbx_set.ver_num |= DCBX_CONFIG_VERSION_STATIC;
		dcbx_set.enabled = true;
	}

	return qed_dcbx_config_params(p_hwfn, p_ptt, &dcbx_set,
				      !!p_hwfn->cdev->b_dcbx_cfg_commit);
}

int qed_dcbx_get_pfc_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  u8 priority)
{
	if (priority >= QED_MAX_PFC_PRIORITIES) {
		DP_ERR(p_hwfn, "Invalid priority %d\n", priority);
		return -1;
	}

	if (!p_hwfn->p_dcbx_info->get.operational.valid) {
		DP_ERR(p_hwfn, "Dcbx parameters not available\n");
		return -1;
	}

	return p_hwfn->p_dcbx_info->get.operational.params.pfc.prio[priority];
}

int qed_dcbx_set_pfc_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  u8 priority, u8 enable)
{
	struct qed_dcbx_set dcbx_set;
	int rc;

	if (priority >= QED_MAX_PFC_PRIORITIES) {
		DP_ERR(p_hwfn, "Invalid priority %d\n", priority);
		return -1;
	}

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(p_hwfn, &dcbx_set);
	if (rc)
		return rc;

	dcbx_set.override_flags |= QED_DCBX_OVERRIDE_PFC_CFG;
	dcbx_set.config.params.pfc.prio[priority] = !!enable;

	return qed_dcbx_config_params(p_hwfn, p_ptt, &dcbx_set,
				      !!p_hwfn->cdev->b_dcbx_cfg_commit);
}

int qed_dcbx_get_num_tcs_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	if (!p_hwfn->p_dcbx_info->get.operational.valid) {
		DP_ERR(p_hwfn, "Dcbx parameters not available\n");
		return -1;
	}

	return p_hwfn->p_dcbx_info->get.operational.params.pfc.max_tc;
}

int qed_dcbx_get_pri_to_tc_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				u8 pri)
{
	if (pri >= QED_MAX_PFC_PRIORITIES) {
		DP_ERR(p_hwfn, "Invalid priority %d\n", pri);
		return -1;
	}

	if (!p_hwfn->p_dcbx_info->get.operational.valid) {
		DP_ERR(p_hwfn, "Dcbx parameters not available\n");
		return -1;
	}

	return p_hwfn->p_dcbx_info->get.operational.params.ets_pri_tc_tbl[pri];
}

int qed_dcbx_set_pri_to_tc_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				u8 pri, u8 tc)
{
	struct qed_dcbx_set dcbx_set;
	int rc;

	if (pri >= QED_MAX_PFC_PRIORITIES || tc >= QED_MAX_PFC_PRIORITIES) {
		DP_ERR(p_hwfn, "Invalid priority/tc %d/%d\n", pri, tc);
		return -1;
	}

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(p_hwfn, &dcbx_set);
	if (rc)
		return rc;

	dcbx_set.override_flags |= QED_DCBX_OVERRIDE_ETS_CFG;
	dcbx_set.config.params.ets_pri_tc_tbl[pri] = tc;

	return qed_dcbx_config_params(p_hwfn, p_ptt, &dcbx_set,
				      !!p_hwfn->cdev->b_dcbx_cfg_commit);
}

int qed_dcbx_get_tc_bw_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    u8 tc)
{
	if (tc >= QED_MAX_PFC_PRIORITIES) {
		DP_ERR(p_hwfn, "Invalid TC %d\n", tc);
		return -1;
	}

	if (!p_hwfn->p_dcbx_info->get.operational.valid) {
		DP_ERR(p_hwfn, "Dcbx parameters not available\n");
		return -1;
	}

	return p_hwfn->p_dcbx_info->get.operational.params.ets_tc_bw_tbl[tc];
}

int qed_dcbx_get_tc_tsa_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			     u8 tc)
{
	if (tc >= QED_MAX_PFC_PRIORITIES) {
		DP_ERR(p_hwfn, "Invalid TC %d\n", tc);
		return -1;
	}

	if (!p_hwfn->p_dcbx_info->get.operational.valid) {
		DP_ERR(p_hwfn, "Dcbx parameters not available\n");
		return -1;
	}

	return p_hwfn->p_dcbx_info->get.operational.params.ets_tc_tsa_tbl[tc];
}

int qed_dcbx_set_tc_bw_tsa_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				u8 tc, u8 bw_pct, u8 tsa_type)
{
	struct qed_dcbx_set dcbx_set;
	int rc;

	if (tc >= QED_MAX_PFC_PRIORITIES || bw_pct > 100 ||
	    (tsa_type != DCBX_ETS_TSA_STRICT && tsa_type != DCBX_ETS_TSA_CBS &&
	     tsa_type != DCBX_ETS_TSA_ETS)) {
		DP_ERR(p_hwfn, "Invalid inputs TC = %d bw = %d TSA = %d\n",
		       tc, bw_pct, tsa_type);
		return -1;
	}

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(p_hwfn, &dcbx_set);
	if (rc)
		return rc;

	dcbx_set.override_flags |= QED_DCBX_OVERRIDE_ETS_CFG;
	dcbx_set.config.params.ets_tc_bw_tbl[tc] = bw_pct;
	dcbx_set.config.params.ets_tc_tsa_tbl[tc] = tsa_type;

	return qed_dcbx_config_params(p_hwfn, p_ptt, &dcbx_set,
				      !!p_hwfn->cdev->b_dcbx_cfg_commit);
}

int qed_dcbx_get_ets_tcs_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	if (!p_hwfn->p_dcbx_info->get.operational.valid) {
		DP_ERR(p_hwfn, "Dcbx parameters not available\n");
		return -1;
	}

	return p_hwfn->p_dcbx_info->get.operational.params.max_ets_tc;
}

int qed_dcbx_app_tlv_set_app_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt,
				  u8 idtype, u16 idval, u8 pri)
{
	bool ethtype = false, ieee, found = false;
	struct qed_dcbx_set dcbx_set;
	struct qed_app_entry *entry;
	int i, rc;

	if (pri >= QED_MAX_PFC_PRIORITIES) {
		DP_ERR(p_hwfn, "Invalid priority %d\n", pri);
		return -1;
	}

	if (!p_hwfn->p_dcbx_info->get.operational.valid) {
		DP_ERR(p_hwfn, "Dcbx parameters are not available\n");
		return -1;
	}

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(p_hwfn, &dcbx_set);
	if (rc) {
		DP_ERR(p_hwfn, "Dcbx parameters are not available\n");
		return rc;
	}

	ieee = p_hwfn->p_dcbx_info->get.operational.ieee;
	if (!ieee)
		ethtype = !idtype;

	for (i = 0; i < dcbx_set.config.params.num_app_entries; i++) {
		entry = &dcbx_set.config.params.app_entry[i];
		if (ieee) {
			if ((entry->sf_ieee == idtype) &&
			    (entry->proto_id == idval)) {
				found = true;
				break;
			}
		} else if ((entry->ethtype == ethtype) &&
			   (entry->proto_id == idval)) {
			found = true;
			break;
		}
	}

	if (i >= QED_DCBX_MAX_APP_PROTOCOL) {
		DP_ERR(p_hwfn, "App table is full\n");
		return -EBUSY;
	}

	/* Add new entry */
	if (!found)
		dcbx_set.config.params.num_app_entries++;

	dcbx_set.override_flags |= QED_DCBX_OVERRIDE_APP_CFG;
	if (ieee)
		dcbx_set.config.params.app_entry[i].sf_ieee = idtype;
	else
		dcbx_set.config.params.app_entry[i].ethtype = ethtype;
	dcbx_set.config.params.app_entry[i].proto_id = idval;
	dcbx_set.config.params.app_entry[i].prio = pri;

	return qed_dcbx_config_params(p_hwfn, p_ptt, &dcbx_set,
				      !!p_hwfn->cdev->b_dcbx_cfg_commit);
}

int qed_dcbx_app_tlv_del_all_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt)
{
	struct qed_dcbx_set dcbx_set;
	int rc;

	if (!p_hwfn->p_dcbx_info->get.operational.valid) {
		DP_ERR(p_hwfn, "Dcbx parameters are not available\n");
		return -1;
	}

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(p_hwfn, &dcbx_set);
	if (rc) {
		DP_ERR(p_hwfn, "Dcbx parameters are not available\n");
		return rc;
	}

	dcbx_set.config.params.num_app_entries = 0;
	memset(dcbx_set.config.params.app_entry, 0,
	       sizeof(struct qed_app_entry) * QED_DCBX_MAX_APP_PROTOCOL);

	dcbx_set.override_flags |= QED_DCBX_OVERRIDE_APP_CFG;

	return qed_dcbx_config_params(p_hwfn, p_ptt, &dcbx_set,
				      !!p_hwfn->cdev->b_dcbx_cfg_commit);
}

int qed_dcbx_app_tlv_get_count_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt)
{
	struct qed_dcbx_get *dcbx_get = &p_hwfn->p_dcbx_info->get;

	if (!dcbx_get->operational.valid) {
		DP_ERR(p_hwfn, "Dcbx parameters are not available\n");
		return -1;
	}

	return dcbx_get->operational.params.num_app_entries;
}

int qed_dcbx_app_tlv_get_value_by_idx_test(struct qed_hwfn *p_hwfn,
					   struct qed_ptt *p_ptt, u8 idx)
{
	struct qed_dcbx_get *dcbx_get = &p_hwfn->p_dcbx_info->get;

	if (!dcbx_get->operational.valid) {
		DP_ERR(p_hwfn, "Dcbx parameters are not available\n");
		return -1;
	}

	if (idx >= dcbx_get->operational.params.num_app_entries) {
		DP_ERR(p_hwfn, "Invalid index = %d, app count = %d\n", idx,
		       dcbx_get->operational.params.num_app_entries);
		return -1;
	}

	return dcbx_get->operational.params.app_entry[idx].proto_id;
}

int qed_dcbx_app_tlv_get_type_by_idx_test(struct qed_hwfn *p_hwfn,
					  struct qed_ptt *p_ptt, u8 idx)
{
	struct qed_dcbx_get *dcbx_get = &p_hwfn->p_dcbx_info->get;

	if (!dcbx_get->operational.valid) {
		DP_ERR(p_hwfn, "Dcbx parameters are not available\n");
		return -1;
	}

	if (idx >= dcbx_get->operational.params.num_app_entries) {
		DP_ERR(p_hwfn, "Invalid index = %d, app count = %d\n", idx,
		       dcbx_get->operational.params.num_app_entries);
		return -1;
	}

	if (dcbx_get->operational.ieee) {
		return dcbx_get->operational.params.app_entry[idx].sf_ieee;
	} else {
		if (dcbx_get->operational.params.app_entry[idx].ethtype)
			return QED_DCBX_SF_IEEE_ETHTYPE;
		else
			return QED_DCBX_SF_IEEE_TCP_UDP_PORT;
	}
}

int qed_dcbx_app_tlv_get_pri_by_idx_test(struct qed_hwfn *p_hwfn,
					 struct qed_ptt *p_ptt, u8 idx)
{
	struct qed_dcbx_get *dcbx_get = &p_hwfn->p_dcbx_info->get;

	if (!dcbx_get->operational.valid) {
		DP_ERR(p_hwfn, "Dcbx parameters are not available\n");
		return -1;
	}

	if (idx >= dcbx_get->operational.params.num_app_entries) {
		DP_ERR(p_hwfn, "Invalid index = %d, app count = %d\n", idx,
		       dcbx_get->operational.params.num_app_entries);
		return -1;
	}

	return dcbx_get->operational.params.app_entry[idx].prio;
}

#define QED_DCB_FEATCFG_PFC 0
#define QED_DCB_FEATCFG_PG  1
#define QED_DCB_FEATCFG_APP 2

int qed_dcbx_get_willing_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      u8 featid)
{
	struct qed_dcbx_get *dcbx_get = &p_hwfn->p_dcbx_info->get;

	if (!dcbx_get->operational.valid) {
		DP_ERR(p_hwfn, "Dcbx parameters are not available\n");
		return -1;
	}

	switch (featid) {
	case QED_DCB_FEATCFG_PFC:
		return !!dcbx_get->operational.params.pfc.willing;
	case QED_DCB_FEATCFG_PG:
		return !!dcbx_get->operational.params.ets_willing;
	case QED_DCB_FEATCFG_APP:
		return !!dcbx_get->operational.params.app_willing;
	default:
		DP_ERR(p_hwfn, "Invalid feature-ID %d\n", featid);
	}

	return -1;
}

int qed_dcbx_set_willing_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      u8 featid, u8 enable)
{
	struct qed_dcbx_set dcbx_set;
	int rc;

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(p_hwfn, &dcbx_set);
	if (rc)
		return rc;

	switch (featid) {
	case QED_DCB_FEATCFG_PFC:
		dcbx_set.override_flags |= QED_DCBX_OVERRIDE_PFC_CFG;
		dcbx_set.config.params.pfc.willing = !!enable;
		break;
	case QED_DCB_FEATCFG_PG:
		dcbx_set.override_flags |= QED_DCBX_OVERRIDE_ETS_CFG;
		dcbx_set.config.params.ets_willing = !!enable;
		break;
	case QED_DCB_FEATCFG_APP:
		dcbx_set.override_flags |= QED_DCBX_OVERRIDE_APP_CFG;
		dcbx_set.config.params.app_willing = !!enable;
		break;
	default:
		DP_ERR(p_hwfn, "Invalid feature-ID %d\n", featid);
		return -1;
	}

	return qed_dcbx_config_params(p_hwfn, p_ptt, &dcbx_set,
				      !!p_hwfn->cdev->b_dcbx_cfg_commit);
}

int qed_dcbx_hw_commit_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_dcbx_set dcbx_set;
	int rc;

	memset(&dcbx_set, 0, sizeof(dcbx_set));
	rc = qed_dcbx_get_config_params(p_hwfn, &dcbx_set);
	if (rc)
		return rc;

	return qed_dcbx_config_params(p_hwfn, p_ptt, &dcbx_set, 1);
}

int qed_dcbx_set_cfg_commit_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 u8 enable)
{
	p_hwfn->cdev->b_dcbx_cfg_commit = !!enable;

	return 0;
}

int qed_rdma_glob_vlan_pri_en_test(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt, u8 pri_en_val)
{
	if (!p_hwfn->p_rdma_info) {
		DP_NOTICE(p_hwfn,
			  "RDMA is disabled, setting vlan_pri_en failed\n");
		return -1;
	}

	if (pri_en_val > 1) {
		DP_ERR(p_hwfn,
		       "Invalid glob pri en val %d. Values should be 0..1\n",
		       pri_en_val);
		return -1;
	}

	p_hwfn->p_rdma_info->glob_cfg.vlan_pri_en = pri_en_val;
	DP_NOTICE(p_hwfn, "rdma global vlan priority enable value set to %d\n",
		  pri_en_val);

	return 0;
}

int qed_rdma_glob_get_vlan_pri_en_test(struct qed_hwfn *p_hwfn,
				       struct qed_ptt *p_ptt)
{
	if (!p_hwfn->p_rdma_info) {
		DP_NOTICE(p_hwfn,
			  "RDMA is disabled, getting vlan_pri_en failed\n");
		return -1;
	}

	DP_NOTICE(p_hwfn, "rdma global vlan priority enable value is %d\n",
		  p_hwfn->p_rdma_info->glob_cfg.vlan_pri_en);

	return p_hwfn->p_rdma_info->glob_cfg.vlan_pri_en;
}

int qed_rdma_glob_vlan_pri_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				u8 pri_val)
{
	if (!p_hwfn->p_rdma_info) {
		DP_NOTICE(p_hwfn,
			  "RDMA is disabled, setting vlan_pri failed\n");
		return -1;
	}

	if (pri_val > QED_MAX_PFC_PRIORITIES) {
		DP_ERR(p_hwfn,
		       "Invalid glob vlan pri val %d. Values should be 0..7\n",
		       pri_val);
		return -1;
	}

	p_hwfn->p_rdma_info->glob_cfg.vlan_pri = pri_val;
	p_hwfn->p_rdma_info->glob_cfg.vlan_pri_en = 1;
	DP_NOTICE(p_hwfn, "rdma global vlan priority value set to %d\n",
		  pri_val);

	return 0;
}

int qed_rdma_glob_get_vlan_pri_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt)
{
	if (!p_hwfn->p_rdma_info) {
		DP_NOTICE(p_hwfn,
			  "RDMA is disabled, getting vlan_pri failed\n");
		return -1;
	}

	DP_NOTICE(p_hwfn, "rdma global vlan priority value is %d\n",
		  p_hwfn->p_rdma_info->glob_cfg.vlan_pri);

	return p_hwfn->p_rdma_info->glob_cfg.vlan_pri;
}

int qed_rdma_glob_ecn_en_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      u8 ecn_en_val)
{
	if (!p_hwfn->p_rdma_info) {
		DP_NOTICE(p_hwfn,
			  "RDMA is disabled, setting ecn_en failed\n");
		return -1;
	}

	if (ecn_en_val > 1) {
		DP_ERR(p_hwfn,
		       "Invalid glob ecn en val %d. Values should be 0..1\n",
		       ecn_en_val);
		return -1;
	}

	p_hwfn->p_rdma_info->glob_cfg.ecn_en = ecn_en_val;
	DP_NOTICE(p_hwfn, "rdma global ecn enable value set to %d\n",
		  ecn_en_val);

	return 0;
}

int qed_rdma_glob_get_ecn_en_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt)
{
	if (!p_hwfn->p_rdma_info) {
		DP_NOTICE(p_hwfn,
			  "RDMA is disabled, getting ecn_en failed\n");
		return -1;
	}

	DP_NOTICE(p_hwfn, "rdma global ecn enable value is %d\n",
		  p_hwfn->p_rdma_info->glob_cfg.ecn_en);

	return p_hwfn->p_rdma_info->glob_cfg.ecn_en;
}

int qed_rdma_glob_ecn_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			   u8 ecn_val)
{
	if (!p_hwfn->p_rdma_info) {
		DP_NOTICE(p_hwfn,
			  "RDMA is disabled, setting ecn failed\n");
		return -1;
	}

	if (ecn_val > QED_DCBX_DSCP_SIZE) {
		DP_ERR(p_hwfn,
		       "Invalid glob ecn val %d. Values should be 0..63\n",
		       ecn_val);
		return -1;
	}

	p_hwfn->p_rdma_info->glob_cfg.ecn = ecn_val;
	p_hwfn->p_rdma_info->glob_cfg.ecn_en = 1;
	DP_NOTICE(p_hwfn, "rdma global ecn value set to %d\n", ecn_val);

	return 0;
}

int qed_rdma_glob_get_ecn_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	if (!p_hwfn->p_rdma_info) {
		DP_NOTICE(p_hwfn,
			  "RDMA is disabled, getting ecn failed\n");
		return -1;
	}

	DP_NOTICE(p_hwfn, "rdma global ecn value is %d\n",
		  p_hwfn->p_rdma_info->glob_cfg.ecn);

	return p_hwfn->p_rdma_info->glob_cfg.ecn;
}

int qed_rdma_glob_dscp_en_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			       u8 dscp_en_val)
{
	if (!p_hwfn->p_rdma_info) {
		DP_NOTICE(p_hwfn,
			  "RDMA is disabled, setting dscp_en failed\n");
		return -1;
	}

	if (dscp_en_val > 1) {
		DP_ERR(p_hwfn,
		       "Invalid glob dscp en val %d. Values should be 0..1\n",
		       dscp_en_val);
		return -1;
	}

	p_hwfn->p_rdma_info->glob_cfg.dscp_en = dscp_en_val;
	DP_NOTICE(p_hwfn, "rdma global dscp enable value set to %d\n",
		  dscp_en_val);

	return 0;
}

int qed_rdma_glob_get_dscp_en_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	if (!p_hwfn->p_rdma_info) {
		DP_NOTICE(p_hwfn,
			  "RDMA is disabled, getting dscp_en failed\n");
		return -1;
	}

	DP_NOTICE(p_hwfn, "rdma global dscp enable value is %d\n",
		  p_hwfn->p_rdma_info->glob_cfg.dscp_en);

	return p_hwfn->p_rdma_info->glob_cfg.dscp_en;
}

int qed_rdma_glob_dscp_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    u8 dscp_val)
{
	if (!p_hwfn->p_rdma_info) {
		DP_NOTICE(p_hwfn,
			  "RDMA is disabled, setting dscp failed\n");
		return -1;
	}

	if (dscp_val > QED_DCBX_DSCP_SIZE) {
		DP_ERR(p_hwfn,
		       "Invalid glob dscp val %d. Values should be 0..63\n",
		       dscp_val);
		return -1;
	}

	p_hwfn->p_rdma_info->glob_cfg.dscp = dscp_val;
	p_hwfn->p_rdma_info->glob_cfg.dscp_en = 1;
	DP_NOTICE(p_hwfn, "rdma global dscp value set to %d\n", dscp_val);

	return 0;
}

int qed_rdma_glob_get_dscp_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	if (!p_hwfn->p_rdma_info) {
		DP_NOTICE(p_hwfn,
			  "RDMA is disabled, getting dscp failed\n");
		return -1;
	}

	DP_NOTICE(p_hwfn, "rdma global dscp value is %d\n",
		  p_hwfn->p_rdma_info->glob_cfg.dscp);

	return p_hwfn->p_rdma_info->glob_cfg.dscp;
}

int qed_gen_hw_err_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			u8 hw_err_type)
{
	qed_hw_err_notify(p_hwfn, p_ptt, hw_err_type, NULL, 0);

	return 0;
}

int qed_set_dev_access_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    u8 enable)
{
	qed_set_dev_access_enable(p_hwfn->cdev, !!enable);

	return 0;
}

int qed_reg_read_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u32 addr)
{
	u32 value = qed_rd(p_hwfn, p_ptt, addr);

	DP_VERBOSE(p_hwfn, QED_MSG_SP, "Read value 0x%08x from addr 0x%08x\n",
		   value, addr);

	return value;
}

int qed_reg_write_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u32 addr,
		       u32 value)
{
	qed_wr(p_hwfn, p_ptt, addr, value);

	DP_VERBOSE(p_hwfn, QED_MSG_SP, "Write value 0x%08x to addr 0x%08x\n",
		   value, addr);

	return qed_rd(p_hwfn, p_ptt, addr);
}

int qed_dump_llh_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	return qed_llh_dump_all(p_hwfn->cdev);
}

int qed_pq_group_count_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u8 count)
{
	p_hwfn->qm_info.offload_group_count = count;
	return 0;
}

int qed_pq_group_set_pq_port_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u16 idx, u8 port, u8 tc)
{
	p_hwfn->qm_info.offload_group[idx].port = port;
	p_hwfn->qm_info.offload_group[idx].tc = tc;
	return 0;
}

int qed_get_multi_tc_roce_en_test(struct qed_hwfn *p_hwfn)
{
	return IS_QED_MULTI_TC_ROCE(p_hwfn);
}

int qed_get_offload_tc_test(struct qed_hwfn *p_hwfn)
{
	if (!p_hwfn->hw_info.offload_tc_set)
		return -1;

	return p_hwfn->hw_info.offload_tc;
}

int qed_set_offload_tc_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    u8 tc)
{
	if (tc >= qed_init_qm_get_num_tcs(p_hwfn)) {
		DP_INFO(p_hwfn, "Invalid tc %d\n", tc);
		return -1;
	}

	p_hwfn->hw_info.offload_tc = tc;
	p_hwfn->hw_info.offload_tc_set = true;
	qed_qm_reconf(p_hwfn, p_ptt);

	return 0;
}

int qed_unset_offload_tc_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	p_hwfn->hw_info.offload_tc_set = false;
	qed_qm_reconf(p_hwfn, p_ptt);

	return 0;
}

int qed_link_down_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u8 link_up)
{
	p_hwfn->mcp_info->link_output.link_up = link_up;
	qed_link_update(p_hwfn, p_ptt);
	return 0;
}

int qed_set_fec_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			   u16 fec_mode)
{
	struct qed_link_output current_link;
	struct qed_link_params params;

	memset(&current_link, 0, sizeof(current_link));
	qed_get_current_link(p_hwfn->cdev, &current_link);

	memset(&params, 0, sizeof(params));
	params.override_flags |= QED_LINK_OVERRIDE_FEC_CONFIG;

	params.fec = fec_mode;
	params.link_up = true;

	qed_set_link(p_hwfn->cdev, &params);

	return 0;
}

int qed_get_fec_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_link_output current_link;

	memset(&current_link, 0, sizeof(current_link));
	qed_get_current_link(p_hwfn->cdev, &current_link);

	return current_link.active_fec;
}

int qed_lag_create_test(struct qed_hwfn *p_hwfn)
{
	return qed_lag_create(p_hwfn->cdev,
                   QED_LAG_TYPE_ACTIVEACTIVE,
                   NULL, NULL, 0x3);
}

int qed_lag_modify_test(struct qed_hwfn *p_hwfn, u8 port_id, u8 link_active)
{
	return qed_lag_modify(p_hwfn->cdev, port_id, link_active);
}

int qed_lag_destroy_test(struct qed_hwfn *p_hwfn)
{
	return qed_lag_destroy(p_hwfn->cdev);
}

int qed_monitored_hw_addr_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u32 hw_addr)
{
	p_hwfn->cdev->monitored_hw_addr = hw_addr;
	return 0;
}

int qed_get_phys_port_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	if (IS_VF(p_hwfn->cdev))
		return 1;
	return MFW_PORT(p_hwfn);
}

int qed_set_led_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		     u8 led_state)
{
	if (led_state != QED_LED_MODE_OFF && led_state != QED_LED_MODE_ON &&
	    led_state != QED_LED_MODE_RESTORE) {
		DP_NOTICE(p_hwfn, "Invalid LED state value %d\n", led_state);
		return -1;
	}

	return qed_mcp_set_led(p_hwfn, p_ptt, led_state);
}

int qed_nvm_get_cfg_len_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			     u16 option_id)
{
	u8 p_buf[128];
	u32 len = 0;
	int rc;

	memset(p_buf, 0, sizeof(p_buf));
	rc = qed_mcp_nvm_get_cfg(p_hwfn, p_ptt, option_id, 0, 10, p_buf, &len);
	if (rc)
		return rc;

	return len;
}

int qed_nvm_get_cfg_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			 u16 option_id, u8 entity_id, u16 flags, u8 offset)
{
	u8 p_buf[128];
	u32 len = 0;
	int rc;

	memset(p_buf, 0, sizeof(p_buf));
	rc = qed_mcp_nvm_get_cfg(p_hwfn, p_ptt, option_id, entity_id, flags,
				 p_buf, &len);
	if (rc)
		return rc;

	if (len > 4)
		DP_NOTICE(p_hwfn,
			  "Size of NVM field is %d bytes, returning contents at offset %d\n",
			  len, offset);

	return *(u32 *)&p_buf[offset];
}

int qed_nvm_set_cfg_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			 u16 option_id, u8 entity_id, u16 flags, u32 p_buf,
			 u8 offset)
{
	u8 p_temp[128];
	u32 len = 0;
	int rc;

	memset(p_temp, 0, sizeof(p_temp));
	if (offset) {
		rc = qed_mcp_nvm_get_cfg(p_hwfn, p_ptt, option_id, entity_id,
					 flags, p_temp, &len);
		if (rc)
			return rc;
	} else {
		len = sizeof(u32);
	}

	memcpy(&p_temp[offset], (void *)&p_buf, sizeof(u32));
	rc = qed_mcp_nvm_set_cfg(p_hwfn, p_ptt, option_id, entity_id, flags,
				 (u8 *)&p_temp, len);

	return rc;
}

int qed_mcp_get_tx_flt_attn_en_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt)
{
	u8 enabled;
	int rc;

	rc = qed_mcp_is_tx_flt_attn_enabled(p_hwfn, p_ptt, &enabled);
	if (rc)
		return rc;

	DP_VERBOSE(p_hwfn, QED_MSG_SP, "TX_FAULT attention is %s\n",
		   enabled ? "enabled" : "disabled");

	return enabled;
}

int qed_mcp_get_rx_los_attn_en_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt)
{
	u8 enabled;
	int rc;

	rc = qed_mcp_is_rx_los_attn_enabled(p_hwfn, p_ptt, &enabled);
	if (rc)
		return rc;

	DP_VERBOSE(p_hwfn, QED_MSG_SP, "RX_LOS attention is %s\n",
		   enabled ? "enabled" : "disabled");

	return enabled;
}

int qed_mcp_enable_tx_flt_attn_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt,
				    u8 enable)
{
	int rc;

	rc = qed_mcp_enable_tx_flt_attn(p_hwfn, p_ptt, !!enable);
	if (rc)
		return rc;

	DP_VERBOSE(p_hwfn, QED_MSG_SP, "TX_FAULT attention was %s\n",
		   enable ? "enabled" : "disabled");

	return 0;
}

int qed_mcp_enable_rx_los_attn_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt,
				    u8 enable)
{
	int rc;

	rc = qed_mcp_enable_rx_los_attn(p_hwfn, p_ptt, !!enable);
	if (rc)
		return rc;

	DP_VERBOSE(p_hwfn, QED_MSG_SP, "RX_LOS attention was %s\n",
		   enable ? "enabled" : "disabled");

	return 0;
}

int qed_set_bw_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		    u8 bw_min, u8 bw_max)
{
	if (bw_min < 1 || bw_min > 100 || bw_max < 1 || bw_max > 100) {
		DP_NOTICE(p_hwfn, "PF min/max bw valid range is [1-100]\n");
		return -EINVAL;
	}

	DP_VERBOSE(p_hwfn, QED_MSG_SP, "Configuring BW values <%d %d>\n",
		   bw_min, bw_max);

	return qed_mcp_set_bandwidth(p_hwfn, p_ptt, bw_min, bw_max);
}

int qed_set_trace_filter_test(struct qed_hwfn *p_hwfn, u32 dbg_level,
			      u32 dbg_modules)
{
	int rc;

	rc = qed_mcp_set_trace_filter(p_hwfn, &dbg_level, &dbg_modules);
	if (rc)
		return rc;

	DP_VERBOSE(p_hwfn, QED_MSG_SP,
		   "Trace filter set to dbg_level = %d dbg_modules = %d\n",
		   dbg_level, dbg_modules);

	return 0;
}

int qed_restore_trace_filter_test(struct qed_hwfn *p_hwfn)
{
	int rc;

	rc = qed_mcp_restore_trace_filter(p_hwfn);
	if (rc)
		return rc;

	DP_VERBOSE(p_hwfn, QED_MSG_SP, " Trace filter restored\n");

	return 0;
}

int qed_get_print_dbg_data_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	DP_VERBOSE(p_hwfn, QED_MSG_SP, "print_dbg_data is %d\n",
		   p_hwfn->cdev->print_dbg_data);

	return p_hwfn->cdev->print_dbg_data;
}

int qed_set_print_dbg_data_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				bool print_dbg_data)
{
	p_hwfn->cdev->print_dbg_data = print_dbg_data;
	DP_VERBOSE(p_hwfn, QED_MSG_SP, "print_dbg_data is set to %d\n",
		   print_dbg_data);

	return 0;
}

int qed_esl_supported_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	return qed_mcp_is_esl_supported(p_hwfn);
}

int qed_esl_active_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	bool  esl_active;
	int rc = 0;

	rc = qed_mcp_get_esl_status(p_hwfn, p_ptt, &esl_active);
	if (rc)
		return 0;

	return esl_active;
}

int qed_gen_mdump_idlechk_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	return qed_mcp_gen_mdump_idlechk(p_hwfn, p_ptt);
}

int qed_set_vf_stats_bin_id_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 u16 vf_id)
{
	return qed_set_vf_stats_bin_id(p_hwfn->cdev, vf_id);
}
