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

#ifndef _QED_DEV_API_H
#define _QED_DEV_API_H
#include <linux/types.h>
#include <linux/if_ether.h>
#include <linux/slab.h>
#include "qed_chain.h"
#include "qed_int.h"

#define QED_DEFAULT_ILT_PAGE_SIZE 4

struct qed_wake_info {
	u32 wk_info;
	u32 wk_details;
	u32 wk_pkt_len;
	u8 wk_buffer[256];
};

/**
 * @brief qed_init_dp - initialize the debug level
 *
 * @param cdev
 * @param dp_module
 * @param dp_level
 * @param dp_ctx
 */
void qed_init_dp(struct qed_dev *cdev,
		 u32 dp_module, u8 dp_level, void *dp_ctx);

/**
 * @brief qed_init_int_dp - initialize the internal debug level
 *
 * @param cdev
 * @param dp_module
 * @param dp_level
 */
void qed_init_int_dp(struct qed_dev *cdev, u32 dp_module, u8 dp_level);

/**
 * @brief qed_dp_internal_log - store into internal log
 *
 * @param cdev
 * @param buf
 * @param len
 */
void qed_dp_internal_log(struct qed_dev *cdev, char *fmt, ...);

/**
 * @brief qed_init_struct - initialize the device structure to
 *        its defaults
 *
 * @param cdev
 */
int qed_init_struct(struct qed_dev *cdev);

/**
 * @brief qed_resc_free -
 *
 * @param cdev
 */
void qed_resc_free(struct qed_dev *cdev);

/**
 * @brief qed_resc_alloc -
 *
 * @param cdev
 *
 * @return int
 */
int qed_resc_alloc(struct qed_dev *cdev);

/**
 * @brief qed_resc_setup -
 *
 * @param cdev
 */
void qed_resc_setup(struct qed_dev *cdev);

enum qed_mfw_timeout_fallback {
	QED_TO_FALLBACK_TO_NONE,
	QED_TO_FALLBACK_TO_DEFAULT,
	QED_TO_FALLBACK_FAIL_LOAD,
};

enum qed_override_force_load {
	QED_OVERRIDE_FORCE_LOAD_NONE,
	QED_OVERRIDE_FORCE_LOAD_ALWAYS,
	QED_OVERRIDE_FORCE_LOAD_NEVER,
};

struct qed_drv_load_params {
	/* Indicates whether the driver is running over a crash kernel.
	 * As part of the load request, this will be used for providing the
	 * driver role to the MFW.
	 * In case of a crash kernel over PDA - this should be set to false.
	 */
	bool is_crash_kernel;

	/* The timeout value that the MFW should use when locking the engine for
	 * the driver load process.
	 * A value of '0' means the default value, and '255' means no timeout.
	 */
	u8 mfw_timeout_val;
#define QED_LOAD_REQ_LOCK_TO_DEFAULT    0
#define QED_LOAD_REQ_LOCK_TO_NONE       255

	/* Action to take in case the MFW doesn't support timeout values other
	 * then default and none.
	 */
	enum qed_mfw_timeout_fallback mfw_timeout_fallback;

	/* Avoid engine reset when first PF loads on it */
	bool avoid_eng_reset;

	/* Allow overriding the default force load behavior */
	enum qed_override_force_load override_force_load;
};

struct qed_hw_init_params {
	/* Tunneling parameters */
	struct qed_tunnel_info *p_tunn;

	bool b_hw_start;

	/* Interrupt mode [msix, inta, etc.] to use */
	enum qed_int_mode int_mode;

	/* NPAR tx switching to be used for vports configured for tx-switching */
	bool allow_npar_tx_switch;

	/* PCI relax ordering to be configured by MFW or qed client */
	enum qed_pci_rlx_odr pci_rlx_odr_mode;

	/* Binary fw data pointer in binary fw file */
	const u8 *bin_fw_data;

	/* Driver load parameters */
	struct qed_drv_load_params *p_drv_load_params;

	/* Avoid engine affinity for RoCE/storage in case of CMT mode */
	bool avoid_eng_affin;

	/* SPQ block timeout in msec */
	u32 spq_timeout_ms;
};

/**
 * @brief qed_hw_init -
 *
 * @param cdev
 * @param p_params
 *
 * @return int
 */
int qed_hw_init(struct qed_dev *cdev, struct qed_hw_init_params *p_params);

/**
 * @brief qed_hw_timers_stop_all -
 *
 * @param cdev
 *
 * @return void
 */
void qed_hw_timers_stop_all(struct qed_dev *cdev);

/**
 * @brief qed_hw_stop -
 *
 * @param cdev
 *
 * @return int
 */
int qed_hw_stop(struct qed_dev *cdev);

/**
 * @brief qed_hw_stop_fastpath -should be called incase
 *        slowpath is still required for the device,
 *        but fastpath is not.
 *
 * @param cdev
 *
 * @return int
 */
int qed_hw_stop_fastpath(struct qed_dev *cdev);

/**
 * @brief qed_hw_start_fastpath -restart fastpath traffic,
 *        only if hw_stop_fastpath was called
 *
 * @param p_hwfn
 *
 * @return int
 */
int qed_hw_start_fastpath(struct qed_hwfn *p_hwfn);

enum qed_hw_prepare_result {
	QED_HW_PREPARE_SUCCESS,

	/* FAILED results indicate probe has failed & cleaned up */
	QED_HW_PREPARE_FAILED_ENG2,
	QED_HW_PREPARE_FAILED_ME,
	QED_HW_PREPARE_FAILED_MEM,
	QED_HW_PREPARE_FAILED_DEV,
	QED_HW_PREPARE_FAILED_NVM,

	/* BAD results indicate probe is passed even though some wrongness
	 * has occurred; Trying to actually use [I.e., hw_init()] might have
	 * dire reprecautions.
	 */
	QED_HW_PREPARE_BAD_IOV,
	QED_HW_PREPARE_BAD_MCP,
	QED_HW_PREPARE_BAD_IGU,
};

enum QED_ROCE_EDPM_MODE {
	QED_ROCE_EDPM_MODE_ENABLE = 0,
	QED_ROCE_EDPM_MODE_FORCE_ON = 1,
	QED_ROCE_EDPM_MODE_DISABLE = 2,
};

struct qed_hw_prepare_params {
	/* Personality to initialize */
	int personality;

	/* Force the driver's default resource allocation */
	bool drv_resc_alloc;

	/* Check the reg_fifo after any register access */
	bool chk_reg_fifo;

	/* Monitored address by qed_rd()/qed_wr() */
	u32 monitored_hw_addr;

	/* Request the MFW to initiate PF FLR */
	bool initiate_pf_flr;

	/* The OS Epoch time in seconds */
	u32 epoch;

	/* Allow the MFW to collect a crash dump */
	bool allow_mdump;

	/* Allow prepare to pass even if some initializations are failing.
	 * If set, the `p_prepare_res' field would be set with the return,
	 * and might allow probe to pass even if there are certain issues.
	 */
	bool b_relaxed_probe;
	enum qed_hw_prepare_result p_relaxed_res;

	/* Enable/disable request by qed client for pacing */
	bool b_en_pacing;

	/* Enable/disable request by qed client for dcqcn */
	bool b_en_dcqcn;

	/* Indicates whether this PF serves a storage target */
	bool b_is_target;

	/* EDPM can be enabled/forced_on/disabled */
	u8 roce_edpm_mode;

	/* retry count for VF acquire on channel timeout */
	u8 acquire_retry_cnt;

	/* Num of VF CNQs resources that will be requested */
	u8 num_vf_cnqs;

	/* Disable SRIOV */
	bool b_sriov_disable;

	/* retry count to acquire MCP resource lock */
	u8 mcp_resc_lock_retry_cnt;
};

/**
 * @brief qed_hw_prepare -
 *
 * @param cdev
 * @param p_params
 *
 * @return int
 */
int qed_hw_prepare(struct qed_dev *cdev,
		   struct qed_hw_prepare_params *p_params);

/**
 * @brief qed_hw_remove -
 *
 * @param cdev
 */
void qed_hw_remove(struct qed_dev *cdev);

/**
 * @brief qed_set_nwuf_reg -
 *
 * @param cdev
 * @param reg_idx - Index of the pattern register
 * @param pattern_size - size of pattern
 * @param crc - CRC value of patter & mask
 *
 * @return int
 */
int qed_set_nwuf_reg(struct qed_dev *cdev,
		     u32 reg_idx, u32 pattern_size, u32 crc);

/**
 * @brief qed_get_wake_info - get magic packet buffer
 *
 * @param p_hwfn
 * @param p_ppt
 * @param wake_info - pointer to qed_wake_info buffer
 *
 * @return int
 */
int qed_get_wake_info(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, struct qed_wake_info *wake_info);

/**
 * @brief qed_wol_buffer_clear - Clear magic package buffer
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @return void
 */
void qed_wol_buffer_clear(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief qed_ptt_acquire - Allocate a PTT window
 *
 * Should be called at the entry point to the driver (at the beginning of an
 * exported function)
 *
 * @param p_hwfn
 *
 * @return struct qed_ptt
 */
struct qed_ptt *qed_ptt_acquire(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_ptt_release - Release PTT Window
 *
 * Should be called at the end of a flow - at the end of the function that
 * acquired the PTT.
 *
 *
 * @param p_hwfn
 * @param p_ptt
 */
void qed_ptt_release(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief qed_get_dev_name - get device name, e.g., "BB B0"
 *
 * @param p_hwfn
 * @param name - this is where the name will be written to
 * @param max_chars - maximum chars that can be written to name including '\0'
 */
void qed_get_dev_name(struct qed_dev *cdev, u8 * name, u8 max_chars);

void qed_chain_params_init(struct qed_chain_params *p_params,
			   enum qed_chain_use_mode intended_use,
			   enum qed_chain_mode mode,
			   enum qed_chain_cnt_type cnt_type,
			   u32 num_elems, size_t elem_size);
/**
 * @brief qed_chain_alloc - Allocate and initialize a chain
 *
 * @param p_hwfn
 * @param intended_use
 * @param mode
 * @param num_elems
 * @param elem_size
 * @param p_chain
 *
 * @return int
 */
int
qed_chain_alloc(struct qed_dev *cdev,
		struct qed_chain *p_chain, struct qed_chain_params *p_params);

/**
 * @brief qed_chain_free - Free chain DMA memory
 *
 * @param p_hwfn
 * @param p_chain
 */
void qed_chain_free(struct qed_dev *cdev, struct qed_chain *p_chain);

/**
 * @@brief qed_fw_l2_queue - Get absolute L2 queue ID
 *
 *  @param p_hwfn
 *  @param src_id - relative to p_hwfn
 *  @param dst_id - absolute per engine
 *
 *  @return int
 */
int qed_fw_l2_queue(struct qed_hwfn *p_hwfn, u16 src_id, u16 * dst_id);

/**
 * @@brief qed_fw_vport - Get absolute vport ID
 *
 *  @param p_hwfn
 *  @param src_id - relative to p_hwfn
 *  @param dst_id - absolute per engine
 *
 *  @return int
 */
int qed_fw_vport(struct qed_hwfn *p_hwfn, u8 src_id, u8 * dst_id);

/**
 * @@brief qed_fw_rss_eng - Get absolute RSS engine ID
 *
 *  @param p_hwfn
 *  @param src_id - relative to p_hwfn
 *  @param dst_id - absolute per engine
 *
 *  @return int
 */
int qed_fw_rss_eng(struct qed_hwfn *p_hwfn, u8 src_id, u8 * dst_id);

/**
 * @brief qed_llh_get_num_ppfid - Return the allocated number of LLH filter
 *	banks that are allocated to the PF.
 *
 * @param cdev
 *
 * @return u8 - Number of LLH filter banks
 */
u8 qed_llh_get_num_ppfid(struct qed_dev *cdev);

enum qed_eng {
	QED_ENG0,
	QED_ENG1,
	QED_BOTH_ENG,
};

/**
 * @brief qed_llh_get_l2_affinity_hint - Return the hint for the L2 affinity
 *
 * @param cdev
 *
 * @return enum qed_eng - L2 affintiy hint
 */
enum qed_eng qed_llh_get_l2_affinity_hint(struct qed_dev *cdev);

/**
 * @brief qed_llh_set_ppfid_affinity - Set the engine affinity for the given
 *	LLH filter bank.
 *
 * @param cdev
 * @param ppfid - relative within the allocated ppfids ('0' is the default one).
 * @param eng
 *
 * @return int
 */
int qed_llh_set_ppfid_affinity(struct qed_dev *cdev,
			       u8 ppfid, enum qed_eng eng);

/**
 * @brief qed_llh_set_roce_affinity - Set the RoCE engine affinity
 *
 * @param cdev
 * @param eng
 *
 * @return int
 */
int qed_llh_set_roce_affinity(struct qed_dev *cdev, enum qed_eng eng);

/**
 * @brief qed_llh_add_mac_filter - Add a LLH MAC filter into the given filter
 *	bank.
 *
 * @param cdev
 * @param ppfid - relative within the allocated ppfids ('0' is the default one).
 * @param mac_addr - MAC to add
 *
 * @return int
 */
int qed_llh_add_mac_filter(struct qed_dev *cdev,
			   u8 ppfid, const u8 mac_addr[ETH_ALEN]);

/**
 * @brief qed_llh_remove_mac_filter - Remove a LLH MAC filter from the given
 *	filter bank.
 *
 * @param cdev
 * @param ppfid - relative within the allocated ppfids ('0' is the default one).
 * @param mac_addr - MAC to remove
 */
void qed_llh_remove_mac_filter(struct qed_dev *cdev,
			       u8 ppfid, u8 mac_addr[ETH_ALEN]);

enum qed_llh_prot_filter_type_t {
	QED_LLH_FILTER_ETHERTYPE,
	QED_LLH_FILTER_TCP_SRC_PORT,
	QED_LLH_FILTER_TCP_DEST_PORT,
	QED_LLH_FILTER_TCP_SRC_AND_DEST_PORT,
	QED_LLH_FILTER_UDP_SRC_PORT,
	QED_LLH_FILTER_UDP_DEST_PORT,
	QED_LLH_FILTER_UDP_SRC_AND_DEST_PORT
};

/**
 * @brief qed_llh_add_protocol_filter - Add a LLH protocol filter into the
 *	given filter bank.
 *
 * @param cdev
 * @param ppfid - relative within the allocated ppfids ('0' is the default one).
 * @param type - type of filters and comparing
 * @param source_port_or_eth_type - source port or ethertype to add
 * @param dest_port - destination port to add
 *
 * @return int
 */
int
qed_llh_add_protocol_filter(struct qed_dev *cdev,
			    u8 ppfid,
			    enum qed_llh_prot_filter_type_t type,
			    u16 source_port_or_eth_type, u16 dest_port);

/**
 * @brief qed_llh_remove_protocol_filter - Remove a LLH protocol filter from
 *	the given filter bank.
 *
 * @param cdev
 * @param ppfid - relative within the allocated ppfids ('0' is the default one).
 * @param type - type of filters and comparing
 * @param source_port_or_eth_type - source port or ethertype to add
 * @param dest_port - destination port to add
 */
void qed_llh_remove_protocol_filter(struct qed_dev *cdev,
				    u8 ppfid,
				    enum qed_llh_prot_filter_type_t type,
				    u16 source_port_or_eth_type, u16 dest_port);

/**
 * @brief qed_llh_clear_ppfid_filters - Remove all LLH filters from the given
 *	filter bank.
 *
 * @param cdev
 * @param ppfid - relative within the allocated ppfids ('0' is the default one).
 */
void qed_llh_clear_ppfid_filters(struct qed_dev *cdev, u8 ppfid);

/**
 * @brief qed_llh_clear_all_filters - Remove all LLH filters
 *
 * @param cdev
 */
void qed_llh_clear_all_filters(struct qed_dev *cdev);

/**
 * @brief qed_llh_set_function_as_default - set function as defult per port
 *
 * @param p_hwfn
 * @param p_ptt
 */
int
qed_llh_set_function_as_default(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * *@brief Cleanup of previous driver remains prior to load
 *
 * @param p_hwfn
 * @param p_ptt
 * @param id - For PF, engine-relative. For VF, PF-relative.
 * @param is_vf - true iff cleanup is made for a VF.
 *
 * @return int
 */
int qed_final_cleanup(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, u16 id, bool is_vf);

/**
 * @brief qed_get_queue_coalesce - Retrieve coalesce value for a given queue.
 *
 * @param p_hwfn
 * @param p_coal - store coalesce value read from the hardware.
 * @param p_handle
 *
 * @return int
 **/
int qed_get_queue_coalesce(struct qed_hwfn *p_hwfn, u16 * coal, void *handle);

/**
 * @brief qed_set_queue_coalesce - Configure coalesce parameters for Rx and
 *    Tx queue. The fact that we can configure coalescing to up to 511, but on
 *    varying accuracy [the bigger the value the less accurate] up to a mistake
 *    of 3usec for the highest values.
 *    While the API allows setting coalescing per-qid, all queues sharing a SB
 *    should be in same range [i.e., either 0-0x7f, 0x80-0xff or 0x100-0x1ff]
 *    otherwise configuration would break.
 *
 * @param p_hwfn
 * @param rx_coal - Rx Coalesce value in micro seconds.
 * @param tx_coal - TX Coalesce value in micro seconds.
 * @param p_handle
 *
 * @return int
 **/
int
qed_set_queue_coalesce(struct qed_hwfn *p_hwfn,
		       u16 rx_coal, u16 tx_coal, void *p_handle);

/**
 * @brief - Recalculate feature distributions based on HW resources and
 * user inputs. Currently this affects RDMA_CNQ, PF_L2_QUE and VF_L2_QUE.
 * As a result, this must not be called while RDMA is active or while VFs
 * are enabled.
 *
 * @param p_hwfn
 */
void qed_hw_set_feat(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_pglueb_set_pfid_enable - Enable or disable PCI BUS MASTER
 *
 * @param p_hwfn
 * @param p_ptt
 * @param b_enable - true/false
 *
 * @return int
 */
int qed_pglueb_set_pfid_enable(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, bool b_enable);

/**
 * @brief db_recovery_add - add doorbell information to the doorbell
 * recovery mechanism.
 *
 * @param cdev
 * @param db_addr - doorbell address
 * @param db_data - address of where db_data is stored
 * @param db_width - doorbell is 32b pr 64b
 * @param db_space - doorbell recovery addresses are user or kernel space
 */
int qed_db_recovery_add(struct qed_dev *cdev,
			void __iomem * db_addr,
			void *db_data,
			enum qed_db_rec_width db_width,
			enum qed_db_rec_space db_space);

/**
 * @brief db_recovery_del - remove doorbell information from the doorbell
 * recovery mechanism. db_data serves as key (db_addr is not unique).
 *
 * @param cdev
 * @param db_addr - doorbell address
 * @param db_data - address where db_data is stored. Serves as key for the
 *                  entry to delete.
 */
int qed_db_recovery_del(struct qed_dev *cdev,
			void __iomem * db_addr, void *db_data);

/**
 * @brief qed_set_dev_access_enable - Enable or disable access to the device
 *
 * @param p_hwfn
 * @param b_enable - true/false
 */
void qed_set_dev_access_enable(struct qed_dev *cdev, bool b_enable);

/**
 * @brief qed_set_ilt_page_size - Set ILT page size
 *
 * @param cdev
 * @param ilt_size
 *
 * @return int
 */
void qed_set_ilt_page_size(struct qed_dev *cdev, u8 ilt_size);

/**
 * @brief Create Lag
 *
 *        two ports of the same device are bonded or unbonded,
 *        or link status changes.
 *
 * @param lag_type: LAG_TYPE_NONE: Disable lag
 *               LAG_TYPE_ACTIVEACTIVE: Utilize all ports
 *               LAG_TYPE_ACTIVEBACKUP: Configure all queues to
 *               active port
 * @param active_ports: Bitmap, each bit represents whether the
 *               port is active or not (1 - active )
 * @param link_change_cb: Callback function to call if port
 *              settings change such as dcbx.
 * @param cxt:	Parameter will be passed to the
 *              link_change_cb function
 *
 * @param p_hwfn
 * @return int
 */
int qed_lag_create(struct qed_dev *dev,
		   enum qed_lag_type lag_type,
		   void (*link_change_cb) (void *cxt),
		   void *cxt, u8 active_ports);
/**
 * @brief Modify lag link status of a given port
 *
 * @param port_id: the port id that change
 * @param link_active: current link state
 */
int qed_lag_modify(struct qed_dev *dev, u8 port_id, u8 link_active);

/**
 * @brief Exit lag mode
 *
 * @param p_hwfn
 */
int qed_lag_destroy(struct qed_dev *dev);

bool qed_lag_is_active(struct qed_hwfn *p_hwfn);

/**
 * @brief Whether FIP discovery fallback special mode is enabled or not.
 *
 * @param cdev
 *
 * @return true if device is in FIP special mode, false otherwise.
 */
bool qed_is_mf_fip_special(struct qed_dev *cdev);

/**
 * @brief Whether device allows DSCP to TC mapping or not.
 *
 * @param cdev
 *
 * @return true if device allows dscp to tc mapping.
 */
bool qed_is_dscp_to_tc_capable(struct qed_dev *cdev);

/**
 * @brief Returns the number of PFs.
 *
 * @param p_hwfn
 *
 * @return u8 - Number of PFs.
 */
u8 qed_get_num_funcs_on_engine(struct qed_hwfn *p_hwfn);

int
qed_qm_update_rt_wfq_of_pqset(struct qed_hwfn *p_hwfn,
			      u16 pq_set_id, u8 tc, u32 min_bw);

int
qed_qm_update_rt_rl_of_pqset(struct qed_hwfn *p_hwfn,
			     u16 pq_set_id, u32 max_bw);

int
qed_qm_update_wfq_of_pqset(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt,
			   u16 pq_set_id, u8 tc, u32 min_bw);

int
qed_qm_update_rl_of_pqset(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, u16 pq_set_id, u32 max_bw);

int
qed_qm_connect_pqset_to_wfq(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt,
			    u16 pq_set_id, u8 tc, u16 wfq_id);

int
qed_qm_connect_pqset_to_rl(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt,
			   u16 pq_set_id, u8 tc, u8 rl_id);

int
qed_qm_get_wfq_of_pqset(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt,
			u16 pq_set_id, u8 tc, u16 * p_wfq_id);

int
qed_qm_get_rl_of_pqset(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       u16 pq_set_id, u8 tc, u16 * p_rl_id);

int
qed_qm_config_pq_wfq(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt, u16 abs_pq_id, u16 wfq_id);

int
qed_qm_config_pq_rl(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt, u16 abs_pq_id, u8 rl_id);

void qed_qm_acquire_access(struct qed_hwfn *p_hwfn);
void qed_qm_release_access(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_sp_fw_assert -
 *
 * The API triggers the FW assert by closing an active VPort.
 *
 * @param p_hwfn
 *
 * @return int
 */
int qed_sp_fw_assert(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_gen_system_kill -
 *
 * The API triggers the killing of the system.
 *
 * @param p_hwfn - HW Device
 * @param p_ptt  - ptt window used for writing the registers.
 */
void qed_gen_system_kill(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief qed_gen_process_kill -
 *
 * The API triggers the killing of the mcp process.
 *
 * @param p_hwfn - HW Device
 * @param p_ptt  - ptt window used for writing the registers.
 * @is_common_block
 */
void qed_gen_process_kill(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, u8 is_common_block);

/**
 * @brief qed_dmae_err -
 *
 * The API triggers the DMAE error.
 *
 * @param p_hwfn - HW Device
 * @param p_ptt  - ptt window used for writing the registers.
 */
void qed_dmae_err(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
#endif
