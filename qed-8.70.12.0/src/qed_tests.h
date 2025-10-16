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

#ifndef _QED_TESTS_H
#define _QED_TESTS_H

#include "qed.h"
#include "qed_hsi.h"
#include "qed_sp.h"
#include "qed_sriov.h"

int qed_qm_reconf_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_ets_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_phony_dcbx_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_mcp_halt_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_mcp_resume_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_mcp_mask_parities_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_mcp_unmask_parities_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_test_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u32 rc);

int qed_coal_vf_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		     u16 rx_coal, u16 tx_coal, u16 vf_id);

int qed_gen_process_kill_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      u8 is_common_block);

int qed_gen_system_kill_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_trigger_recovery_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_fw_assert_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_dmae_err_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/* Mask/Unmask a specific MSI-X vector; Required for integration test of the 'interrupt_test'. */
int qed_msix_vector_mask_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u8 vector, u8 b_mask);

/* Mask/Unmask the MSI-X pci capability; Required for integration test of the `interrupt_test'. */
int qed_msix_mask_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u8 b_mask);

/* Disable/Enable the MSI-X pci capability; Required for integration of the `interrupt_test'. */
int qed_msix_disable_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u8 b_disable);

/* Configure OBFF FSM; Required for integration of the 'OBFF test'. */
int qed_config_obff_fsm_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/* Print OBFF statistics; Required for integration of the 'OBFF test'. */
int qed_dump_obff_stats_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/* Set OBFF state; Required for integration of the 'OBFF test'. */
int qed_set_obff_state_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u8 state);

int qed_ramrod_flood_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  u32 ramrod_amount, u8 blocking);

int qed_gen_ramrod_stuck_test(struct qed_hwfn *p_hwfn);

int qed_gen_fan_failure_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			     u8 is_over_temp);

int qed_bist_register_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_bist_clock_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_bist_nvm_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_get_temperature_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_get_mba_versions_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_mcp_resc_lock_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			   u8 resource, u8 timeout);

int qed_mcp_resc_unlock_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			     u8 resource, u8 force);

int qed_read_dpm_register_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			       u32 hw_addr);

#ifdef CONFIG_IWARP

int qed_iwarp_tcp_cids_weight_test(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt);

int qed_iwarp_ep_free_list_test(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt);

int qed_iwarp_ep_active_list_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt);

int qed_iwarp_create_listen_test(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt,
				 u32 ip_addr, u32 port);

int qed_iwarp_remove_listen_test(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt,
				 u32 handle_hi, u32 handle_lo);

int qed_iwarp_listeners_test(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt);

#endif

int qed_rdma_query_stats_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/* dump to dmesg the doorbell recovery mechanism data */
int qed_db_recovery_dp_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/* execute doorbell recovery */
int qed_db_recovery_execute_test(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt);

/* Get dscp enable state on the pf. */
int qed_dscp_pfc_get_enable_test(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt);

/* Enable/disable dscp on the pf. */
int qed_dscp_pfc_enable_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			     u8 enable);

/* Returns priority value for a given dscp entry. It returns error status when
 * API fails.
 */
int qed_dscp_pfc_get_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  u8 index);

/* Configure priority value for a given dscp entry. */
int qed_dscp_pfc_set_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  u8 index, u8 pri_val);

/* Returns priorities values for given 8 consecutive dscp entries. */
int qed_dscp_pfc_batch_get_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			        u8 index);

/* Configure priority values for eight dscp indices */
int qed_dscp_pfc_batch_set_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				u8 index, u32 pri_val);

/* Configure dcbx mode for the PF. */
int qed_dcbx_set_mode_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			   u8 mode);

/* Returns operational dcbx mode of the PF. */
int qed_dcbx_get_mode_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/* Returns pfc enable value for a given priority */
int qed_dcbx_get_pfc_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  u8 priority);

/* Set pfc enable value for a given priority */
int qed_dcbx_set_pfc_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  u8 priority, u8 enable);

/* Returns TC value for a given priority */
int qed_dcbx_get_pri_to_tc_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				u8 pri);

/* Set TC value for a given priority */
int qed_dcbx_set_pri_to_tc_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				u8 pri, u8 tc);

/* Returns bandwidth percentage for a given TC */
int qed_dcbx_get_tc_bw_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    u8 tc);

/* Returns TSA value for a given TC */
int qed_dcbx_get_tc_tsa_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			     u8 tc);

/* Set bandwidth percentage and TSA for a given TC */
int qed_dcbx_set_tc_bw_tsa_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				u8 tc, u8 bw_pct, u8 tsa_type);

/* Get max PFC TCs supportted */
int qed_dcbx_get_num_tcs_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/* Get max ETS TCs supportted */
int qed_dcbx_get_ets_tcs_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/* Return count of recognized TLVs */
int qed_dcbx_app_tlv_get_count_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt);

/* Returns the protocol value of the APP TLV at a given index */
int qed_dcbx_app_tlv_get_value_by_idx_test(struct qed_hwfn *p_hwfn,
					   struct qed_ptt *p_ptt, u8 idx);

/* Returns the protocol type of the APP TLV at a given index.
 * Possible values are,
 *    Ethtype  - 0
 *    TCP port - 1 (Applicable in ieee mode only)
 *    UDP port - 2 (Applicable in ieee mode only)
 *    Any Port - 3
 */
int qed_dcbx_app_tlv_get_type_by_idx_test(struct qed_hwfn *p_hwfn,
					  struct qed_ptt *p_ptt, u8 idx);

/* Returns the priority value of the APP TLV at a given index. */
int qed_dcbx_app_tlv_get_pri_by_idx_test(struct qed_hwfn *p_hwfn,
					 struct qed_ptt *p_ptt, u8 idx);

/* Set priority value for a given protocol type.
 * idtype - protocol identifier type
 *    Ethtype  - 0
 *    TCP port - 1 (Applicable in ieee mode only)
 *    UDP port - 2 (Applicable in ieee mode only)
 *    Any Port - 3
 *
 * idval - protocol value
 *
 * pri   - priority value
 *
 */
int qed_dcbx_app_tlv_set_app_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt,
				  u8 idtype, u16 idval, u8 pri);

/* Returns Willingness value for a given feature (e.g., pfc) */
int qed_dcbx_get_willing_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      u8 featid);

/* Set Willingness value for a given feature (e.g., pfc) */
int qed_dcbx_set_willing_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      u8 featid, u8 enable);

/* Performs the commit of cached dcbx config to the hardware. dcbx_set commands
 * updates only the driver cache (of dcbx config), need to invoke this  API to
 * cached dcbx paramters to mfw/hardware
 */
int qed_dcbx_hw_commit_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/* Enable/disable the commit of dcbx config (to MFW) for dcbx_set commands.
 * When the commit is disabled, user need to invoke 'dcbx_hw_commit' command
 * (after invoking one/more dcbx_set commands) to apply the config to MFW.
 */
int qed_dcbx_set_cfg_commit_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 u8 enable);
/* Delete all dcbx APP TLVs. Applicable to "Static" dcbx mode only. */
int qed_dcbx_app_tlv_del_all_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt);

/* configure global vlan priority enable for roce qps */
int qed_rdma_glob_vlan_pri_en_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				   u8 pri_en_val);

/* Get global vlan priority enable for roce qps */
int qed_rdma_glob_get_vlan_pri_en_test(struct qed_hwfn *p_hwfn,
				       struct qed_ptt *p_ptt);

/* configure global vlan priority for roce qps */
int qed_rdma_glob_vlan_pri_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				u8 pri_val);

/* Get global vlan priority for roce qps */
int qed_rdma_glob_get_vlan_pri_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt);

/* configure global ecn enable value for roce */
int qed_rdma_glob_ecn_en_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			      u8 ecn_en_val);

/* Get global ecn enable value for roce */
int qed_rdma_glob_get_ecn_en_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt);

/* configure global ecn value for roce */
int qed_rdma_glob_ecn_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			   u8 ecn_val);

/* Get global ecn value for roce */
int qed_rdma_glob_get_ecn_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/* configure global dscp enable value for roce */
int qed_rdma_glob_dscp_en_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			       u8 dscp_en_val);

/* Get global dscp enable value for roce */
int qed_rdma_glob_get_dscp_en_test(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt);

/* configure global dscp value for roce */
int qed_rdma_glob_dscp_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    u8 dscp_val);

/* Get global dscp value for roce */
int qed_rdma_glob_get_dscp_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_gen_hw_err_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			u8 hw_err_type);

int qed_set_dev_access_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    u8 enable);

int qed_reg_read_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u32 addr);

int qed_reg_write_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u32 addr,
		       u32 value);

int qed_dump_llh_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_pq_group_count_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u8 count);

int qed_pq_group_set_pq_port_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u16 idx, u8 port, u8 tc);

int qed_get_multi_tc_roce_en_test(struct qed_hwfn *p_hwfn);

int qed_get_offload_tc_test(struct qed_hwfn *p_hwfn);

int qed_set_offload_tc_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    u8 tc);

int qed_unset_offload_tc_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_link_down_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u8 link_up);

int qed_lag_create_test(struct qed_hwfn *p_hwfn);

int qed_lag_modify_test(struct qed_hwfn *p_hwfn, u8 port_id, u8 link_active);

int qed_lag_destroy_test(struct qed_hwfn *p_hwfn);

/* configure forced fec mode */
int qed_set_fec_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u16 fec_mode);

int qed_get_fec_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/* configure an address to be monitored by ecore_rd()/ecore_wr() */
int qed_monitored_hw_addr_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u32 hw_addr);

int qed_get_phys_port_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/* Set port LED state.
 * The input parameter is defined by 'enum qed_led_mode'.
 *    0 (QED_LED_MODE_OFF) - Turn off the LED
 *    1 (QED_LED_MODE_ON) - Turn on the LED
 *    2 (QED_LED_MODE_RESTORE) - Restore the LED state (Let MFW control the LED
 *                               i.e., blink when traffic is present)
 *
 */
int qed_set_led_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		     u8 led_state);

int qed_nvm_get_cfg_len_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			     u16 option_id);

int qed_nvm_get_cfg_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			 u16 option_id, u8 entity_id, u16 flags, u8 offset);

int qed_nvm_set_cfg_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			 u16 option_id, u8 entity_id, u16 flags, u32 p_buf,
			 u8 offset);

int qed_mcp_get_tx_flt_attn_en_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt);

int qed_mcp_get_rx_los_attn_en_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt);

int qed_mcp_enable_tx_flt_attn_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt,
				    u8 enable);

int qed_mcp_enable_rx_los_attn_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt,
				    u8 enable);

int qed_set_bw_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		    u8 min_bw, u8 max_bw);

int qed_set_trace_filter_test(struct qed_hwfn *p_hwfn, u32 dbg_level,
			      u32 dbg_modules);

int qed_restore_trace_filter_test(struct qed_hwfn *p_hwfn);

int qed_get_print_dbg_data_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_set_print_dbg_data_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				bool print_dbg_data);

int qed_esl_supported_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
int qed_esl_active_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_gen_mdump_idlechk_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int qed_set_vf_stats_bin_id_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				 u16 vf_id);
#endif
