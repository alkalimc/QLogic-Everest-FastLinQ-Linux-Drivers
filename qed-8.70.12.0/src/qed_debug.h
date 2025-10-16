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

#ifndef _QED_DEBUG_H
#define _QED_DEBUG_H
#include <linux/types.h>
#include <linux/list.h>
#ifndef _QED_DEBUGFS_H
#define _QED_DEBUGFS_H
enum qed_dbg_features {
	DBG_FEATURE_BUS,
	DBG_FEATURE_GRC,
	DBG_FEATURE_IDLE_CHK,
	DBG_FEATURE_MCP_TRACE,
	DBG_FEATURE_REG_FIFO,
	DBG_FEATURE_IGU_FIFO,
	DBG_FEATURE_PROTECTION_OVERRIDE,
	DBG_FEATURE_FW_ASSERTS,
	DBG_FEATURE_ILT,
	DBG_FEATURE_INTERNAL_TRACE,
	DBG_FEATURE_LINKDUMP_PHYDUMP,
	DBG_FEATURE_NUM
};

/* Forward Declaration */
struct qed_dev;
struct qed_hwfn;
struct qed_ptt;

void qed_dbg_init(void);
void qed_dbg_exit(void);
void qed_dbg_pf_init(struct qed_dev *cdev);
void qed_dbg_pf_exit(struct qed_dev *cdev);
void qed_sysfs_init(void);
void qed_sysfs_exit(void);
void qed_sysfs_pf_init(struct qed_dev *cdev);
void qed_sysfs_pf_exit(struct qed_dev *cdev);

void qed_copy_preconfig_to_bus(struct qed_dev *cdev, u8 init_engine);
int qed_copy_bus_to_postconfig(struct qed_dev *cdev, u8 down_engine);
int qed_dbg_grc(struct qed_dev *cdev, void *buffer, u32 * num_dumped_bytes);
int qed_dbg_grc_size(struct qed_dev *cdev);
int qed_dbg_idle_chk(struct qed_dev *cdev,
		     void *buffer, u32 * num_dumped_bytes);
int qed_dbg_idle_chk_size(struct qed_dev *cdev);
int qed_dbg_reg_fifo(struct qed_dev *cdev,
		     void *buffer, u32 * num_dumped_bytes);
int qed_dbg_reg_fifo_size(struct qed_dev *cdev);
int qed_dbg_igu_fifo(struct qed_dev *cdev,
		     void *buffer, u32 * num_dumped_bytes);
int qed_dbg_igu_fifo_size(struct qed_dev *cdev);
int qed_dbg_protection_override(struct qed_dev *cdev,
				void *buffer, u32 * num_dumped_bytes);
int qed_dbg_protection_override_size(struct qed_dev *cdev);
int qed_dbg_fw_asserts(struct qed_dev *cdev,
		       void *buffer, u32 * num_dumped_bytes);
int qed_dbg_fw_asserts_size(struct qed_dev *cdev);
int qed_dbg_ilt(struct qed_dev *cdev, void *buffer, u32 * num_dumped_bytes);
int qed_dbg_ilt_size(struct qed_dev *cdev);
int qed_dbg_mcp_trace(struct qed_dev *cdev,
		      void *buffer, u32 * num_dumped_bytes);
int qed_dbg_mcp_trace_size(struct qed_dev *cdev);
int qed_dbg_internal_trace(struct qed_dev *cdev,
			   void *buffer, u32 * num_dumped_bytes);
int qed_dbg_internal_trace_size(struct qed_dev *cdev);
int qed_dbg_linkdump_pyhdump(struct qed_dev *cdev,
			     void *buffer, u32 * num_dumped_bytes);
int qed_dbg_linkdump_phydump_size(struct qed_dev *cdev);
int qed_dbg_phy(struct qed_dev *cdev, void *buffer, u32 * num_dumped_bytes);
int qed_dbg_phy_size(struct qed_dev *cdev);
int qed_dbg_all_data(struct qed_dev *cdev, void *buffer);
int qed_dbg_all_data_size(struct qed_dev *cdev);
void qed_dbg_save_all_data(struct qed_dev *cdev, bool print_dbg_data);
u8 qed_get_debug_engine(struct qed_dev *cdev);
void qed_set_debug_engine(struct qed_dev *cdev, int engine_number);
int qed_dbg_feature(struct qed_dev *cdev,
		    void *buffer,
		    enum qed_dbg_features feature, u32 * num_dumped_bytes);
int qed_dbg_feature_size(struct qed_dev *cdev, enum qed_dbg_features feature);
int qed_str_engine(struct qed_hwfn *p_hwfn,
		   struct qed_ptt *p_ptt, char *params_string);
void qed_set_platform_str_linux(struct qed_hwfn *p_hwfn,
				char *buf_str, u32 buf_size);

enum qed_dbg_uevent_sfp_type { QED_DBG_UEVENT_SFP_UPDATE,
	QED_DBG_UEVENT_SFP_TX_FLT,
	QED_DBG_UEVENT_SFP_RX_LOS
};

void qed_dbg_uevent_sfp(struct qed_dev *cdev,
			enum qed_dbg_uevent_sfp_type type);

int qed_str_bus_reset(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, char *params_string);
int qed_str_bus_set_pci_output(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, char *params_string);
int qed_str_bus_set_nw_output(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, char *params_string);
int qed_str_bus_enable_block(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, char *params_string);
int qed_str_bus_enable_storm(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, char *params_string);
int qed_str_bus_enable_timestamp(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt, char *params_string);
int qed_str_bus_add_eid_range_sem_filter(struct qed_hwfn *p_hwfn,
					 struct qed_ptt *p_ptt,
					 char *params_string);
int qed_str_bus_add_eid_mask_sem_filter(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt,
					char *params_string);
int qed_str_bus_add_cid_sem_filter(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt, char *params_string);
int qed_str_bus_enable_filter(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, char *params_string);
int qed_str_bus_enable_trigger(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, char *params_string);
int qed_str_bus_add_trigger_state(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string);
int qed_str_bus_add_constraint(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, char *params_string);
int qed_str_bus_start(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, char *params_string);
int qed_str_bus_stop(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt, char *params_string);
int qed_str_bus_dump(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt, char *params_string);
int qed_str_grc_config(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt, char *params_string);
int qed_str_grc_dump(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt, char *params_string);
int qed_str_idle_chk_dump(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, char *params_string);
int qed_str_mcp_trace_dump(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, char *params_string);
int qed_str_reg_fifo_dump(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, char *params_string);
int qed_str_igu_fifo_dump(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, char *params_string);
int qed_str_protection_override_dump(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     char *params_string);
int qed_str_fw_asserts_dump(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, char *params_string);
int qed_str_ilt_dump(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt, char *params_string);
int qed_str_internal_trace_dump(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt, char *params_string);
int qed_str_linkdump_phydump_dump(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string);
int qed_str_qm_reconf_test(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, char *params_string);
int qed_str_ets_test(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt, char *params_string);
int qed_str_phony_dcbx_test(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, char *params_string);
int qed_str_mcp_halt_test(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, char *params_string);
int qed_str_mcp_resume_test(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, char *params_string);
int qed_str_mcp_mask_parities_test(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt, char *params_string);
int qed_str_mcp_unmask_parities_test(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     char *params_string);
int qed_str_test_test(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, char *params_string);
int qed_str_coal_vf_test(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt, char *params_string);
int qed_str_gen_process_kill_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string);
int qed_str_gen_system_kill_test(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt, char *params_string);
int qed_str_trigger_recovery_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string);
int qed_str_fw_assert_test(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, char *params_string);
int qed_str_dmae_err_test(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, char *params_string);
int qed_str_msix_vector_mask_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string);
int qed_str_msix_mask_test(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, char *params_string);
int qed_str_msix_disable_test(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, char *params_string);
int qed_str_config_obff_fsm_test(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt, char *params_string);
int qed_str_dump_obff_stats_test(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt, char *params_string);
int qed_str_set_obff_state_test(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt, char *params_string);
int qed_str_ramrod_flood_test(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, char *params_string);
int qed_str_gen_ramrod_stuck_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string);
int qed_str_gen_fan_failure_test(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt, char *params_string);
int qed_str_bist_register_test(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, char *params_string);
int qed_str_bist_clock_test(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, char *params_string);
int qed_str_bist_nvm_test(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, char *params_string);
int qed_str_get_temperature_test(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt, char *params_string);
int qed_str_get_mba_versions_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string);
int qed_str_mcp_resc_lock_test(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, char *params_string);
int qed_str_mcp_resc_unlock_test(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt, char *params_string);
int qed_str_read_dpm_register_test(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt, char *params_string);
int qed_str_iwarp_tcp_cids_weight_test(struct qed_hwfn *p_hwfn,
				       struct qed_ptt *p_ptt,
				       char *params_string);
int qed_str_iwarp_ep_free_list_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string);
int qed_str_iwarp_ep_active_list_test(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      char *params_string);
int qed_str_iwarp_create_listen_test(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     char *params_string);
int qed_str_iwarp_remove_listen_test(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     char *params_string);
int qed_str_iwarp_listeners_test(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt, char *params_string);
int qed_str_rdma_query_stats_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string);
int qed_str_db_recovery_dp_test(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt, char *params_string);
int qed_str_db_recovery_execute_test(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     char *params_string);
int qed_str_dscp_pfc_get_enable_test(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     char *params_string);
int qed_str_dscp_pfc_enable_test(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt, char *params_string);
int qed_str_dscp_pfc_get_test(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, char *params_string);
int qed_str_dscp_pfc_set_test(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, char *params_string);
int qed_str_dscp_pfc_batch_get_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string);
int qed_str_dscp_pfc_batch_set_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string);
int qed_str_dcbx_set_mode_test(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, char *params_string);
int qed_str_dcbx_get_mode_test(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, char *params_string);
int qed_str_dcbx_get_pfc_test(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, char *params_string);
int qed_str_dcbx_set_pfc_test(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, char *params_string);
int qed_str_dcbx_get_pri_to_tc_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string);
int qed_str_dcbx_set_pri_to_tc_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string);
int qed_str_dcbx_get_tc_bw_test(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt, char *params_string);
int qed_str_dcbx_get_tc_tsa_test(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt, char *params_string);
int qed_str_dcbx_set_tc_bw_tsa_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string);
int qed_str_dcbx_get_num_tcs_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string);
int qed_str_dcbx_get_ets_tcs_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string);
int qed_str_dcbx_app_tlv_get_count_test(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt,
					char *params_string);
int qed_str_dcbx_app_tlv_get_value_by_idx_test(struct qed_hwfn *p_hwfn,
					       struct qed_ptt *p_ptt,
					       char *params_string);
int qed_str_dcbx_app_tlv_get_type_by_idx_test(struct qed_hwfn *p_hwfn,
					      struct qed_ptt *p_ptt,
					      char *params_string);
int qed_str_dcbx_app_tlv_get_pri_by_idx_test(struct qed_hwfn *p_hwfn,
					     struct qed_ptt *p_ptt,
					     char *params_string);
int qed_str_dcbx_app_tlv_set_app_test(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      char *params_string);
int qed_str_dcbx_get_willing_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string);
int qed_str_dcbx_set_willing_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string);
int qed_str_dcbx_hw_commit_test(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt, char *params_string);
int qed_str_dcbx_set_cfg_commit_test(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     char *params_string);
int qed_str_dcbx_app_tlv_del_all_test(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      char *params_string);
int qed_str_rdma_glob_vlan_pri_en_test(struct qed_hwfn *p_hwfn,
				       struct qed_ptt *p_ptt,
				       char *params_string);
int qed_str_rdma_glob_get_vlan_pri_en_test(struct qed_hwfn *p_hwfn,
					   struct qed_ptt *p_ptt,
					   char *params_string);
int qed_str_rdma_glob_vlan_pri_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string);
int qed_str_rdma_glob_get_vlan_pri_test(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt,
					char *params_string);
int qed_str_rdma_glob_ecn_en_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string);
int qed_str_rdma_glob_get_ecn_en_test(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      char *params_string);
int qed_str_rdma_glob_ecn_test(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, char *params_string);
int qed_str_rdma_glob_get_ecn_test(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt, char *params_string);
int qed_str_rdma_glob_dscp_en_test(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt, char *params_string);
int qed_str_rdma_glob_get_dscp_en_test(struct qed_hwfn *p_hwfn,
				       struct qed_ptt *p_ptt,
				       char *params_string);
int qed_str_rdma_glob_dscp_test(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt, char *params_string);
int qed_str_rdma_glob_get_dscp_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string);
int qed_str_gen_hw_err_test(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, char *params_string);
int qed_str_set_dev_access_test(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt, char *params_string);
int qed_str_reg_read_test(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, char *params_string);
int qed_str_reg_write_test(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, char *params_string);
int qed_str_dump_llh_test(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, char *params_string);
int qed_str_pq_group_count_test(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt, char *params_string);
int qed_str_pq_group_set_pq_port_test(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      char *params_string);
int qed_str_get_multi_tc_roce_en_test(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      char *params_string);
int qed_str_get_offload_tc_test(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt, char *params_string);
int qed_str_set_offload_tc_test(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt, char *params_string);
int qed_str_unset_offload_tc_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string);
int qed_str_link_down_test(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, char *params_string);
int qed_str_lag_create_test(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, char *params_string);
int qed_str_lag_modify_test(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, char *params_string);
int qed_str_lag_destroy_test(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, char *params_string);
int qed_str_set_fec_test(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt, char *params_string);
int qed_str_get_fec_test(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt, char *params_string);
int qed_str_monitored_hw_addr_test(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt, char *params_string);
int qed_str_get_phys_port_test(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, char *params_string);
int qed_str_set_led_test(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt, char *params_string);
int qed_str_nvm_get_cfg_len_test(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt, char *params_string);
int qed_str_nvm_get_cfg_test(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, char *params_string);
int qed_str_nvm_set_cfg_test(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, char *params_string);
int qed_str_mcp_get_tx_flt_attn_en_test(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt,
					char *params_string);
int qed_str_mcp_get_rx_los_attn_en_test(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt,
					char *params_string);
int qed_str_mcp_enable_tx_flt_attn_test(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt,
					char *params_string);
int qed_str_mcp_enable_rx_los_attn_test(struct qed_hwfn *p_hwfn,
					struct qed_ptt *p_ptt,
					char *params_string);
int qed_str_set_bw_test(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt, char *params_string);
int qed_str_set_trace_filter_test(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string);
int qed_str_restore_trace_filter_test(struct qed_hwfn *p_hwfn,
				      struct qed_ptt *p_ptt,
				      char *params_string);
int qed_str_get_print_dbg_data_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string);
int qed_str_set_print_dbg_data_test(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, char *params_string);
int qed_str_esl_supported_test(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, char *params_string);
int qed_str_esl_active_test(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, char *params_string);
int qed_str_gen_mdump_idlechk_test(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt, char *params_string);
int qed_str_set_vf_stats_bin_id_test(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     char *params_string);
int qed_str_phy_core_write(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_core_read(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_raw_write(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_raw_read(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_mac_stat(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_info(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_sfp_write(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_sfp_read(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_sfp_decode(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_sfp_get_inserted(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_sfp_get_txdisable(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_sfp_set_txdisable(struct qed_hwfn *p_hwfn,
				  struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_sfp_get_txreset(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_sfp_get_rxlos(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_sfp_get_eeprom(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_gpio_write(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_gpio_read(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_gpio_info(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_extphy_read(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, char *params_string);
int qed_str_phy_extphy_write(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, char *params_string);
#endif
#ifndef _DBG_FUNCS_H
#define _DBG_FUNCS_H

/**
 * @brief qed_dbg_internal_trace_get_dump_buf_size - Calculates size in dwords of
 * required buffer for internal trace.
 *
 * @param p_hwfn -		  HW device data
 * @param p_ptt -		  Ptt window (not used).
 * @param buf_dword_size - a pointer to the binary data with debug arrays.
 */
enum dbg_status qed_dbg_internal_trace_get_dump_buf_size(struct qed_hwfn
							 *p_hwfn, struct qed_ptt
							 *p_ptt,
							 u32 * buf_dword_size);

/**
 * @brief qed_dbg_internal_trace_dump - Dump internal trace
 *
 * @param p_hwfn -	      HW device data
 * @param p_ptt -	      Ptt window (not used)
 * @param dump_buf -	      Pointer to copy the data into.
 * @param buf_dword_size -     Size of the specified buffer in dwords.
 * @param num_dumped_dwords  - OUT: number of dumped dwords.
 *
 * @return error if the specified dump buffer is too small
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_internal_trace_dump(struct qed_hwfn *p_hwfn,
					    struct qed_ptt *p_ptt,
					    u32 * dump_buf,
					    u32 buf_dword_size,
					    u32 * num_dumped_dwords);

/**
 * @brief qed_dbg_linkdump_phydump_get_dump_buf_size - Calculates size in
 * dwords of required buffer for mdump2 data.
 *
 * @param p_hwfn -		  HW device data
 * @param p_ptt -		  Ptt window used for writing the registers.
 * @param buf_dword_size - a pointer to the binary data with debug arrays.
 */
enum dbg_status qed_dbg_linkdump_phydump_get_dump_buf_size(struct qed_hwfn
							   *p_hwfn,
							   struct qed_ptt
							   *p_ptt,
							   u32 *
							   buf_dword_size);

/**
 * @brief qed_dbg_linkdump_phydump_dump - Effectively performs linkdump/phydump
 *
 * @param p_hwfn -	      HW device data
 * @param p_ptt -	      Ptt window used for writing the registers.
 * @param dump_buf -	      Pointer to copy the data into.
 * @param buf_dword_size -     Size of the specified buffer in dwords.
 * @param num_dumped_dwords  - OUT: number of dumped dwords.
 *
 * @return error if the specified dump buffer is too small
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_linkdump_phydump_dump(struct qed_hwfn *p_hwfn,
					      struct qed_ptt *p_ptt,
					      u32 * dump_buf,
					      u32 buf_dword_size,
					      u32 * num_dumped_dwords);

/**
 * @brief qed_get_linkdump_phydump_results_buf_size - Returns the required
 * buffer size for linkdump/phydump results (in bytes).
 *
 * @param p_hwfn -	      HW device data
 * @param dump_buf -	      mdump2 linkdump buffer.
 * @param num_dumped_dwords - number of dwords that were dumped.
 * @param results_buf_size -  OUT: required buffer size (in bytes) for the
 *				parsed results.
 *
 * @return error if the parsing fails, ok otherwise.
 */
enum dbg_status qed_get_linkdump_phydump_results_buf_size(struct qed_hwfn
							  *p_hwfn,
							  u32 * dump_buf,
							  u32 num_dumped_dwords,
							  u32 *
							  results_buf_size);

/**
 * @brief qed_print_linkdump_phydump_results - Prints linkdump/phydump results
 *
 * @param p_hwfn -		      HW device data
 * @param dump_buf -	      mdump2 textual buffer.
 * @param num_dumped_dwords - Number of dwords that were dumped.
 * @param results_buf -	      Buffer for printing the linkdump/phydupm results.
 * @param results_buf_size -  size of results_buf
 *
 * @return error if the parsing fails, ok otherwise.
 */
enum dbg_status qed_print_linkdump_phydump_results(struct qed_hwfn *p_hwfn,
						   u32 *
						   dump_buf,
						   u32
						   num_dumped_dwords,
						   char *results_buf,
						   u32 results_buf_size);

#endif
#endif
