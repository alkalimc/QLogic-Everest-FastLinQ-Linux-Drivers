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

#ifndef _QED_FCOE_IF_H
#define _QED_FCOE_IF_H
#include <linux/types.h>
#include <linux/if_ether.h>
#include "qed_if.h"
struct qed_fcoe_stats {
	u64 fcoe_rx_byte_cnt;
	u64 fcoe_rx_data_pkt_cnt;
	u64 fcoe_rx_xfer_pkt_cnt;
	u64 fcoe_rx_other_pkt_cnt;
	u32 fcoe_silent_drop_pkt_cmdq_full_cnt;
	u32 fcoe_silent_drop_pkt_rq_full_cnt;
	u32 fcoe_silent_drop_pkt_crc_error_cnt;
	u32 fcoe_silent_drop_pkt_task_invalid_cnt;
	u32 fcoe_silent_drop_total_pkt_cnt;

	u64 fcoe_tx_byte_cnt;
	u64 fcoe_tx_data_pkt_cnt;
	u64 fcoe_tx_xfer_pkt_cnt;
	u64 fcoe_tx_other_pkt_cnt;
};
struct qed_fcoe_caps {
	/* Maximum number of I/Os per connection */
	u16 max_ios;

	/* Maximum number of Logins per port */
	u16 max_log;

	/* Maximum number of exchanges */
	u16 max_exch;

	/* Maximum NPIV WWN per port */
	u16 max_npiv;

	/* Maximum number of targets supported */
	u16 max_tgt;

	/* Maximum number of outstanding commands across all connections */
	u16 max_outstnd;
};

struct qed_dev_fcoe_info {
	struct qed_dev_info common;

	void __iomem *primary_dbq_rq_addr;
	void __iomem *secondary_bdq_rq_addr;

	u64 wwpn;
	u64 wwnn;

	u8 num_cqs;
};

struct qed_fcoe_params_offload {
	dma_addr_t sq_pbl_addr;
	dma_addr_t sq_curr_page_addr;
	dma_addr_t sq_next_page_addr;

	u8 src_mac[ETH_ALEN];
	u8 dst_mac[ETH_ALEN];

	u16 tx_max_fc_pay_len;
	u16 e_d_tov_timer_val;
	u16 rec_tov_timer_val;
	u16 rx_max_fc_pay_len;
	u16 vlan_tag;

	struct fc_addr_nw s_id;
	u8 max_conc_seqs_c3;
	struct fc_addr_nw d_id;
	u8 flags;
	u8 def_q_idx;
};

#define MAX_TID_BLOCKS_FCOE (512)
struct qed_fcoe_tid {
	u32 size;		/* In bytes per task */
	u32 num_tids_per_block;
	u8 *blocks[MAX_TID_BLOCKS_FCOE];
};

struct qed_fcoe_cb_ops {
	struct qed_common_cb_ops common;
	 u32(*get_login_failures) (void *cookie);
	int (*get_fcoe_capabilities) (void
				      *cookie, struct qed_fcoe_caps * p_caps);
};

void qed_fcoe_set_pf_params(struct qed_dev *, struct qed_fcoe_pf_params *);

/* TODO - we need to pass the pf_params, but probably this should be added
 * inside the common ops [either as seperate, or as part of probe/slowpath
 * start].
 */

/**
 * struct qed_fcoe_ops - qed FCoE operations.
 * @common:		common operations pointer
 * @fill_dev_info:	fills FCoE specific information
 *			@param cdev
 *			@param info
 *			@return 0 on sucesss, otherwise error value.
 * @register_ops:	register FCoE operations
 *			@param cdev
 *			@param ops - specified using qed_iscsi_cb_ops
 *			@param cookie - driver private
 * @ll2:		light L2 operations pointer
 * @start:		fcoe in FW
 *			@param cdev
 *			@param tasks - qed will fill information about tasks
 *			return 0 on success, otherwise error value.
 * @stop:		stops fcoe in FW
 *			@param cdev
 *			return 0 on success, otherwise error value.
 * @acquire_conn:	acquire a new fcoe connection
 *			@param cdev
 *			@param handle - qed will fill handle that should be
 *				used henceforth as identifier of the
 *				connection.
 *			@param p_doorbell - qed will fill the address of the
 *				doorbell.
 *			return 0 on sucesss, otherwise error value.
 * @release_conn:	release a previously acquired fcoe connection
 *			@param cdev
 *			@param handle - the connection handle.
 *			return 0 on success, otherwise error value.
 * @offload_conn:	configures an offloaded connection
 *			@param cdev
 *			@param handle - the connection handle.
 *			@param conn_info - the configuration to use for the
 *				offload.
 *			return 0 on success, otherwise error value.
 * @destroy_conn:	stops an offloaded connection
 *			@param cdev
 *			@param handle - the connection handle.
 *			@param terminate_params
 *			return 0 on success, otherwise error value.
 * @get_stats:		gets FCoE related statistics
 *			@param cdev
 *			@param stats - pointer to struck that would be filled
 *				we stats
 *			return 0 on success, error otherwise.
 */

struct qed_fcoe_ops {
	const struct qed_common_ops *common;

	int (*fill_dev_info) (struct qed_dev
			      * cdev, struct qed_dev_fcoe_info * info);

	void (*register_ops) (struct qed_dev
			      * cdev,
			      struct qed_fcoe_cb_ops * ops, void *cookie);

	const struct qed_ll2_ops *ll2;

	int (*start) (struct qed_dev * cdev, struct qed_fcoe_tid * tasks);

	int (*stop) (struct qed_dev * cdev);

	int (*acquire_conn) (struct qed_dev * cdev,
			     u32 * handle,
			     u32 * fw_cid, void __iomem ** p_doorbell);

	int (*release_conn) (struct qed_dev * cdev, u32 handle);

	int (*offload_conn) (struct qed_dev
			     *
			     cdev,
			     u32
			     handle,
			     struct qed_fcoe_params_offload * conn_info);

	int (*destroy_conn) (struct qed_dev * cdev,
			     u32 handle, dma_addr_t terminate_params);

	int (*get_stats) (struct qed_dev * cdev, struct qed_fcoe_stats * stats);

/**
 * @brief get_fc_npiv - Get the npiv table.
 *
 * @param cdev
 * @param qed_fc_npiv_tbl - structure to be filled with npiv values.
 *
 * @return 0 on success, error otherwise.
 */
	int (*get_fc_npiv) (struct qed_dev * cdev,
			    struct qed_fc_npiv_tbl * table);

/**
 * @brief update_fcoe_cvid - update the fcoe vlan value to MFW.
 *
 * @param cdev
 * @param vlan - vlan id
 *
 * @return 0 on success, error otherwise.
 */
	int (*update_fcoe_cvid) (struct qed_dev * cdev, u16 vlan);

/**
 * @brief update_fcoe_fabric_name - update the fcoe fabric name to MFW.
 *
 * @param cdev
 * @param wwn - wwn of the fabric that this PF is logged into.
 *
 * @return 0 on success, error otherwise.
 */
	int (*update_fcoe_fabric_name) (struct qed_dev * cdev, u8 * wwn);
};

#ifdef QED_UPSTREAM
const struct qed_fcoe_ops *qed_get_fcoe_ops(void);
#else
const struct qed_fcoe_ops *qed_get_fcoe_ops(u32 version);
#endif
void qed_put_fcoe_ops(void);
#endif
