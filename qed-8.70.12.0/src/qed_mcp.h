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

#ifndef _QED_MCP_H
#define _QED_MCP_H
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/if_ether.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include "qed.h"
#include "qed_dev_api.h"
#include "qed_mfw_hsi.h"
#include "qed_fcoe_if.h"
#include "qed_iscsi_if.h"

struct qed_mcp_link_speed_params {
	bool autoneg;
	u32 advertised_speeds;	/* bitmask of DRV_SPEED_CAPABILITY */
	u32 forced_speed;	/* In Mb/s */
};

struct qed_mcp_link_pause_params {
	bool autoneg;
	bool forced_rx;
	bool forced_tx;
};

enum qed_mcp_eee_mode {
	QED_MCP_EEE_DISABLED,
	QED_MCP_EEE_ENABLED,
	QED_MCP_EEE_UNSUPPORTED
};

struct qed_mcp_link_params {
	struct qed_mcp_link_speed_params speed;
	struct qed_mcp_link_pause_params pause;
	u32 loopback_mode;	/* in PMM_LOOPBACK values */
	struct qed_link_eee_params eee;
	u32 fec;
};

struct qed_mcp_link_capabilities {
	u32 speed_capabilities;
	bool default_speed_autoneg;	/* In Mb/s */

	u32 fec_default;
	enum qed_mcp_eee_mode default_eee;
	u32 eee_lpi_timer;
	u8 eee_speed_caps;
};

struct qed_mcp_link_state {
	bool link_up;

	u32 min_pf_rate;	/* In Mb/s */

	/* Actual link speed in Mb/s */
	u32 line_speed;

	/* PF max speed in MB/s, deduced from line_speed
	 * according to PF max bandwidth configuration.
	 */
	u32 speed;
	bool full_duplex;

	bool an;
	bool an_complete;
	bool parallel_detection;
	bool pfc_enabled;

#define QED_LINK_PARTNER_SPEED_1G_HD    (1 << 0)
#define QED_LINK_PARTNER_SPEED_1G_FD    (1 << 1)
#define QED_LINK_PARTNER_SPEED_10G      (1 << 2)
#define QED_LINK_PARTNER_SPEED_20G      (1 << 3)
#define QED_LINK_PARTNER_SPEED_25G      (1 << 4)
#define QED_LINK_PARTNER_SPEED_40G      (1 << 5)
#define QED_LINK_PARTNER_SPEED_50G      (1 << 6)
#define QED_LINK_PARTNER_SPEED_100G     (1 << 7)
	u32 partner_adv_speed;

	bool partner_tx_flow_ctrl_en;
	bool partner_rx_flow_ctrl_en;

#define QED_LINK_PARTNER_SYMMETRIC_PAUSE (1)
#define QED_LINK_PARTNER_ASYMMETRIC_PAUSE (2)
#define QED_LINK_PARTNER_BOTH_PAUSE (3)
	u8 partner_adv_pause;

	bool sfp_tx_fault;

	bool eee_active;
	u8 eee_adv_caps;
	u8 eee_lp_adv_caps;

	u32 fec_active;
};

struct qed_mcp_function_info {
	u8 pause_on_host;

	enum qed_pci_personality protocol;

	u8 bandwidth_min;
	u8 bandwidth_max;

	u8 mac[ETH_ALEN];

	u64 wwn_port;
	u64 wwn_node;

#define QED_MCP_VLAN_UNSET              (0xffff)
	u16 ovlan;

	u16 mtu;
};

struct qed_mcp_drv_version {
	u32 version;
	u8 name[MCP_DRV_VER_STR_SIZE - 4];
};

struct qed_mcp_lan_stats {
	u64 ucast_rx_pkts;
	u64 ucast_tx_pkts;
	u32 fcs_err;
};

#ifndef QED_PROTO_STATS
#define QED_PROTO_STATS
struct qed_mcp_fcoe_stats {
	u64 rx_pkts;
	u64 tx_pkts;
	u32 fcs_err;
	u32 login_failure;
};

struct qed_mcp_iscsi_stats {
	u64 rx_pdus;
	u64 tx_pdus;
	u64 rx_bytes;
	u64 tx_bytes;
};

struct qed_mcp_rdma_stats {
	u64 rx_pkts;
	u64 tx_pkts;
	u64 rx_bytes;
	u64 tx_byts;
};

enum qed_mcp_protocol_type {
	QED_MCP_LAN_STATS,
	QED_MCP_FCOE_STATS,
	QED_MCP_ISCSI_STATS,
	QED_MCP_RDMA_STATS
};

union qed_mcp_protocol_stats {
	struct qed_mcp_lan_stats lan_stats;
	struct qed_mcp_fcoe_stats fcoe_stats;
	struct qed_mcp_iscsi_stats iscsi_stats;
	struct qed_mcp_rdma_stats rdma_stats;
};
#endif

enum qed_ov_client {
	QED_OV_CLIENT_DRV,
	QED_OV_CLIENT_USER,
	QED_OV_CLIENT_VENDOR_SPEC
};

enum qed_ov_driver_state {
	QED_OV_DRIVER_STATE_NOT_LOADED,
	QED_OV_DRIVER_STATE_DISABLED,
	QED_OV_DRIVER_STATE_ACTIVE
};

enum qed_ov_wol {
	QED_OV_WOL_DEFAULT,
	QED_OV_WOL_DISABLED,
	QED_OV_WOL_ENABLED
};

enum qed_ov_eswitch {
	QED_OV_ESWITCH_NONE,
	QED_OV_ESWITCH_VEB,
	QED_OV_ESWITCH_VEPA
};

struct qed_temperature_sensor {
	u8 sensor_location;
	u8 threshold_high;
	u8 critical;
	u8 current_temp;
};

#define QED_MAX_NUM_OF_SENSORS  7
struct qed_temperature_info {
	u32 num_sensors;
	struct qed_temperature_sensor sensors[QED_MAX_NUM_OF_SENSORS];
};

enum qed_mba_img_idx {
	QED_MBA_LEGACY_IDX,
	QED_MBA_PCI3CLP_IDX,
	QED_MBA_PCI3_IDX,
	QED_MBA_FCODE_IDX,
	QED_EFI_X86_IDX,
	QED_EFI_IPF_IDX,
	QED_EFI_EBC_IDX,
	QED_EFI_X64_IDX,
	QED_MAX_NUM_OF_ROMIMG
};

struct qed_mba_vers {
	u32 mba_vers[QED_MAX_NUM_OF_ROMIMG];
};

enum qed_mfw_tlv_type {
	QED_MFW_TLV_GENERIC = 0x1,	/* Core driver TLVs */
	QED_MFW_TLV_ETH = 0x2,	/* L2 driver TLVs */
	QED_MFW_TLV_FCOE = 0x4,	/* FCoE protocol TLVs */
	QED_MFW_TLV_ISCSI = 0x8,	/* SCSI protocol TLVs */
	QED_MFW_TLV_MAX = 0x16,
};

struct qed_mfw_tlv_generic {
	struct {
		u8 ipv4_csum_offload;
		u8 lso_supported;
		bool b_set;
	} flags;

#define QED_MFW_TLV_MAC_COUNT 3
	/* First entry for primary MAC, 2 secondary MACs possible */
	u8 mac[QED_MFW_TLV_MAC_COUNT][6];
	bool mac_set[QED_MFW_TLV_MAC_COUNT];

	u64 rx_frames;
	bool rx_frames_set;
	u64 rx_bytes;
	bool rx_bytes_set;
	u64 tx_frames;
	bool tx_frames_set;
	u64 tx_bytes;
	bool tx_bytes_set;
};

union qed_mfw_tlv_data {
	struct qed_mfw_tlv_generic generic;
	struct qed_mfw_tlv_eth eth;
	struct qed_mfw_tlv_fcoe fcoe;
	struct qed_mfw_tlv_iscsi iscsi;
};

#define QED_NVM_CFG_OPTION_ALL  (1 << 0)
#define QED_NVM_CFG_OPTION_INIT (1 << 1)
#define QED_NVM_CFG_OPTION_COMMIT       (1 << 2)
#define QED_NVM_CFG_OPTION_FREE (1 << 3)
#define QED_NVM_CFG_OPTION_ENTITY_SEL   (1 << 4)
#define QED_NVM_CFG_GET_FLAGS           0xA
#define QED_NVM_CFG_GET_PF_FLAGS        0x1A
#define QED_NVM_CFG_MAX_ATTRS           50

enum qed_nvm_flash_cmd {
	QED_NVM_FLASH_CMD_FILE_DATA = 0x2,
	QED_NVM_FLASH_CMD_FILE_START = 0x3,
	QED_NVM_FLASH_CMD_NVM_CHANGE = 0x4,
	QED_NVM_FLASH_CMD_NVM_CFG_ID = 0x5,
	QED_NVM_FLASH_CMD_NVM_MAX,
};

/**
 * @brief - returns the link params of the hw function
 *
 * @param p_hwfn
 *
 * @returns pointer to link params
 */
struct qed_mcp_link_params *qed_mcp_get_link_params(struct qed_hwfn
						    *p_hwfn);

/**
 * @brief - return the link state of the hw function
 *
 * @param p_hwfn
 *
 * @returns pointer to link state
 */
struct qed_mcp_link_state *qed_mcp_get_link_state(struct qed_hwfn
						  *p_hwfn);

/**
 * @brief - return the link capabilities of the hw function
 *
 * @param p_hwfn
 *
 * @returns pointer to link capabilities
 */
struct qed_mcp_link_capabilities
*qed_mcp_get_link_capabilities(struct qed_hwfn *p_hwfn);

/**
 * @brief Request the MFW to set the the link according to 'link_input'.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param b_up - raise link if `true'. Reset link if `false'.
 *
 * @return int
 */
int qed_mcp_set_link(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, bool b_up);

/**
 * @brief Get the management firmware version value
 *
 * @param p_hwfn
 * @param p_ptt
 * @param p_mfw_ver    - mfw version value
 * @param p_running_bundle_id	- image id in nvram; Optional.
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_get_mfw_ver(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt,
			u32 * p_mfw_ver, u32 * p_running_bundle_id);

/**
 * @brief Get the MBI version value
 *
 * @param p_hwfn
 * @param p_ptt
 * @param p_mbi_ver - A pointer to a variable to be filled with the MBI version.
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_get_mbi_ver(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt, u32 * p_mbi_ver);

/**
 * @brief Get media type value of the port.
 *
 * @param cdev      - qed dev pointer
 * @param p_ptt
 * @param mfw_ver    - media type value
 *
 * @return int -
 *      0 - Operation was successful.
 *      -EBUSY - Operation failed
 */
int qed_mcp_get_media_type(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, u32 * media_type);

/**
 * @brief Get transceiver data of the port.
 *
 * @param cdev      - qed dev pointer
 * @param p_ptt
 * @param p_transceiver_state - transceiver state.
 * @param p_transceiver_type - media type value
 *
 * @return int -
 *      0 - Operation was successful.
 *      -EBUSY - Operation failed
 */
int qed_mcp_get_transceiver_data(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt,
				 u32 * p_transceiver_state,
				 u32 * p_tranceiver_type);

/**
 * @brief Get transciever supported speed mask.
 *
 * @param cdev      - qed dev pointer
 * @param p_ptt
 * @param p_speed_mask - Bit mask of all supported speeds.
 *
 * @return int -
 *      0 - Operation was successful.
 *      -EBUSY - Operation failed
 */

int qed_mcp_trans_speed_mask(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, u32 * p_speed_mask);

/**
 * @brief Get board configuration.
 *
 * @param cdev      - qed dev pointer
 * @param p_ptt
 * @param p_board_config - Board config.
 *
 * @return int -
 *      0 - Operation was successful.
 *      -EBUSY - Operation failed
 */
int qed_mcp_get_board_config(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, u32 * p_board_config);

/**
 * @brief - Sends a command to the MCP mailbox from sleepable context.
 *
 * @param p_hwfn      - hw function
 * @param p_ptt       - PTT required for register access
 * @param cmd         - command to be sent to the MCP
 * @param param       - Optional param
 * @param o_mcp_resp  - The MCP response code (exclude sequence)
 * @param o_mcp_param - Optional parameter provided by the MCP response
 *
 * @return int -
 *      0 - operation was successful
 *      -EBUSY    - operation failed
 */
int qed_mcp_cmd(struct qed_hwfn *p_hwfn,
		struct qed_ptt *p_ptt,
		u32 cmd, u32 param, u32 * o_mcp_resp, u32 * o_mcp_param);

/**
 * @brief - Sends a command to the MCP mailbox from non-sleepable context.
 *
 * @param p_hwfn      - hw function
 * @param p_ptt       - PTT required for register access
 * @param cmd         - command to be sent to the MCP
 * @param param       - Optional param
 * @param o_mcp_resp  - The MCP response code (exclude sequence)
 * @param o_mcp_param - Optional parameter provided by the MCP response
 *
 * @return int -
 *      0 - operation was successful
 *      -EBUSY    - operation failed
 */
int qed_mcp_cmd_nosleep(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt,
			u32 cmd,
			u32 param, u32 * o_mcp_resp, u32 * o_mcp_param);

/**
 * @brief - drains the nig, allowing completion to pass in case of pauses.
 *          (Should be called only from sleepable context)
 *
 * @param p_hwfn
 * @param p_ptt
 */
int qed_mcp_drain(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief Get the flash size value
 *
 * @param p_hwfn
 * @param p_ptt
 * @param p_flash_size  - flash size in bytes to be filled.
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_get_flash_size(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, u32 * p_flash_size);

/**
 * @brief Send driver version to MFW
 *
 * @param p_hwfn
 * @param p_ptt
 * @param version - Version value
 * @param name - Protocol driver name
 *
 * @return int - 0 - operation was successful.
 */
int
qed_mcp_send_drv_version(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 struct qed_mcp_drv_version *p_ver);

/**
 * @brief Read the MFW process kill counter
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @return u32
 */
u32 qed_get_process_kill_counter(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt);

/**
 * @brief Trigger a recovery process
 *
 *  @param p_hwfn
 *  @param p_ptt
 *
 * @return int
 */
int qed_start_recovery_process(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief A recovery handler must call this function as its first step.
 *        It is assumed that the handler is not run from an interrupt context.
 *
 *  @param cdev
 *  @param p_ptt
 *
 * @return int
 */
int qed_recovery_prolog(struct qed_dev *cdev);

/**
 * @brief Notify MFW about the change in base device properties
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param client - qed client type
 *
 * @return int - 0 - operation was successful.
 */
int
qed_mcp_ov_update_current_config(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt,
				 enum qed_ov_client client);

/**
 * @brief Notify MFW about the driver state
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param drv_state - Driver state
 *
 * @return int - 0 - operation was successful.
 */
int
qed_mcp_ov_update_driver_state(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt,
			       enum qed_ov_driver_state drv_state);

/**
 * @brief Read NPIV settings form the MFW
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param p_table - Array to hold the FC NPIV data. Client need allocate the
 *                   required buffer. The field 'count' specifies number of NPIV
 *                   entries. A value of 0 means the table was not populated.
 *
 * @return int - 0 - operation was successful.
 */
int
qed_mcp_ov_get_fc_npiv(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt, struct qed_fc_npiv_tbl *p_table);

/**
 * @brief Send MTU size to MFW
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param mtu - MTU size
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_ov_update_mtu(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, u16 mtu);

/**
 * @brief Send MAC address to MFW
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param mac - MAC address
 *
 * @return int - 0 - operation was successful.
 */
int
qed_mcp_ov_update_mac(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, const u8 * mac);

/**
 * @brief Send WOL mode to MFW
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param wol - WOL mode
 *
 * @return int - 0 - operation was successful.
 */
int
qed_mcp_ov_update_wol(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, enum qed_ov_wol wol);

/**
 * @brief Send eswitch mode to MFW
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param eswitch - eswitch mode
 *
 * @return int - 0 - operation was successful.
 */
int
qed_mcp_ov_update_eswitch(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, enum qed_ov_eswitch eswitch);

/**
 * @brief Set LED status
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param mode - LED mode
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_set_led(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt, enum qed_led_mode mode);

/**
 * @brief Write to phy
 *
 *  @param cdev
 *  @param addr - nvm offset
 *  @param cmd - nvm command
 *  @param p_buf - nvm write buffer
 *  @param len - buffer len
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_phy_write(struct qed_dev *cdev,
		      u32 cmd, u32 addr, u8 * p_buf, u32 len);

/**
 * @brief Write to nvm
 *
 *  @param cdev
 *  @param addr - nvm offset
 *  @param cmd - nvm command
 *  @param p_buf - nvm write buffer
 *  @param len - buffer len
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_nvm_write(struct qed_dev *cdev,
		      u32 cmd, u32 addr, u8 * p_buf, u32 len);

/**
 * @brief Delete file
 *
 *  @param cdev
 *  @param addr - nvm offset
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_nvm_del_file(struct qed_dev *cdev, u32 addr);

/**
 * @brief Read from phy
 *
 *  @param cdev
 *  @param addr - nvm offset
 *  @param cmd - nvm command
 *  @param p_buf - nvm read buffer
 *  @param len - buffer len
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_phy_read(struct qed_dev *cdev,
		     u32 cmd, u32 addr, u8 * p_buf, u32 * p_len);

/**
 * @brief Read from nvm
 *
 *  @param cdev
 *  @param addr - nvm offset
 *  @param p_buf - nvm read buffer
 *  @param len - buffer len
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_nvm_read(struct qed_dev *cdev, u32 addr, u8 * p_buf, u32 len);

struct qed_nvm_image_att {
	u32 start_addr;
	u32 length;
};

/**
 * @brief Allows reading a whole nvram image
 *
 * @param p_hwfn
 * @param image_id - image to get attributes for
 * @param p_image_att - image attributes structure into which to fill data
 *
 * @return int - 0 - operation was successful.
 */
int
qed_mcp_get_nvm_image_att(struct qed_hwfn *p_hwfn,
			  enum qed_nvm_images image_id,
			  struct qed_nvm_image_att *p_image_att);

/**
 * @brief Allows reading a whole nvram image
 *
 * @param p_hwfn
 * @param image_id - image requested for reading
 * @param p_buffer - allocated buffer into which to fill data
 * @param buffer_len - length of the allocated buffer.
 *
 * @return 0 iff p_buffer now contains the nvram image.
 */
int qed_mcp_get_nvm_image(struct qed_hwfn *p_hwfn,
			  enum qed_nvm_images image_id,
			  u8 * p_buffer, u32 buffer_len);

/**
 * @brief - Sends an NVM write command request to the MFW with
 *          payload.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param cmd - Command: Either DRV_MSG_CODE_NVM_WRITE_NVRAM or
 *            DRV_MSG_CODE_NVM_PUT_FILE_DATA
 * @param param - [0:23] - Offset [24:31] - Size
 * @param o_mcp_resp - MCP response
 * @param o_mcp_param - MCP response param
 * @param i_txn_size -  Buffer size
 * @param i_buf - Pointer to the buffer
 * @param b_can_sleep - Whether sleep is allowed in this execution path.
 *
 * @param return 0 upon success.
 */
int qed_mcp_nvm_wr_cmd(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       u32 cmd,
		       u32 param,
		       u32 * o_mcp_resp,
		       u32 * o_mcp_param,
		       u32 i_txn_size, u32 * i_buf, bool b_can_sleep);

/**
 * @brief - Sends an NVM read command request to the MFW to get
 *        a buffer.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param cmd - Command: DRV_MSG_CODE_NVM_GET_FILE_DATA or
 *            DRV_MSG_CODE_NVM_READ_NVRAM commands
 * @param param - [0:23] - Offset [24:31] - Size
 * @param o_mcp_resp - MCP response
 * @param o_mcp_param - MCP response param
 * @param o_txn_size -  Buffer size output
 * @param o_buf - Pointer to the buffer returned by the MFW.
 * @param b_can_sleep - Whether sleep is allowed in this execution path.
 *
 * @param return 0 upon success.
 */
int qed_mcp_nvm_rd_cmd(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       u32 cmd,
		       u32 param,
		       u32 * o_mcp_resp,
		       u32 * o_mcp_param,
		       u32 * o_txn_size, u32 * o_buf, bool b_can_sleep);

/**
 * @brief Read from sfp
 *
 *  @param p_hwfn - hw function
 *  @param p_ptt  - PTT required for register access
 *  @param port   - transceiver port
 *  @param addr   - I2C address
 *  @param offset - offset in sfp
 *  @param len    - buffer length
 *  @param p_buf  - buffer to read into
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_phy_sfp_read(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 u32 port, u32 addr, u32 offset, u32 len, u8 * p_buf);

/**
 * @brief Write to sfp
 *
 *  @param p_hwfn - hw function
 *  @param p_ptt  - PTT required for register access
 *  @param port   - transceiver port
 *  @param addr   - I2C address
 *  @param offset - offset in sfp
 *  @param len    - buffer length
 *  @param p_buf  - buffer to write from
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_phy_sfp_write(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt,
			  u32 port, u32 addr, u32 offset, u32 len, u8 * p_buf);

/**
 * @brief Gpio read
 *
 *  @param p_hwfn    - hw function
 *  @param p_ptt     - PTT required for register access
 *  @param gpio      - gpio number
 *  @param gpio_val  - value read from gpio
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_gpio_read(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, u16 gpio, u32 * gpio_val);

/**
 * @brief Gpio write
 *
 *  @param p_hwfn    - hw function
 *  @param p_ptt     - PTT required for register access
 *  @param gpio      - gpio number
 *  @param gpio_val  - value to write to gpio
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_gpio_write(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt, u16 gpio, u16 gpio_val);

/**
 * @brief Gpio get information
 *
 *  @param p_hwfn          - hw function
 *  @param p_ptt           - PTT required for register access
 *  @param gpio            - gpio number
 *  @param gpio_direction  - gpio is output (0) or input (1)
 *  @param gpio_ctrl       - gpio control is uninitialized (0),
 *                         path 0 (1), path 1 (2) or shared(3)
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_gpio_info(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt,
		      u16 gpio, u32 * gpio_direction, u32 * gpio_ctrl);

/**
 * @brief Bist register test
 *
 *  @param p_hwfn    - hw function
 *  @param p_ptt     - PTT required for register access
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_bist_register_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief Bist clock test
 *
 *  @param p_hwfn    - hw function
 *  @param p_ptt     - PTT required for register access
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_bist_clock_test(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief Bist nvm test - get number of images
 *
 *  @param p_hwfn       - hw function
 *  @param p_ptt        - PTT required for register access
 *  @param num_images   - number of images if operation was
 *			  successful. 0 if not.
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_bist_nvm_get_num_images(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt, u32 * num_images);

/**
 * @brief Bist nvm test - get image attributes by index
 *
 *  @param p_hwfn      - hw function
 *  @param p_ptt       - PTT required for register access
 *  @param p_image_att - Attributes of image
 *  @param image_index - Index of image to get information for
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_bist_nvm_get_image_att(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt,
				   struct bist_nvm_image_att *p_image_att,
				   u32 image_index);

/**
 * @brief qed_mcp_get_temperature_info - get the status of the temperature
 *                                         sensors
 *
 *  @param p_hwfn        - hw function
 *  @param p_ptt         - PTT required for register access
 *  @param p_temp_status - A pointer to an qed_temperature_info structure to
 *                         be filled with the temperature data
 *
 * @return int - 0 - operation was successful.
 */
int
qed_mcp_get_temperature_info(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt,
			     struct qed_temperature_info *p_temp_info);

/**
 * @brief Get MBA versions - get MBA sub images versions
 *
 *  @param p_hwfn      - hw function
 *  @param p_ptt       - PTT required for register access
 *  @param p_mba_vers  - MBA versions array to fill
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_get_mba_versions(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt,
			     struct qed_mba_vers *p_mba_vers);

/**
 * @brief Count memory ecc events
 *
 *  @param p_hwfn      - hw function
 *  @param p_ptt       - PTT required for register access
 *  @param num_events  - number of memory ecc events
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_mem_ecc_events(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, u64 * num_events);

struct qed_mdump_info {
	u32 reason;
	u32 version;
	u32 config;
	u32 epoch;
	u32 num_of_logs;
	u32 valid_logs;
};

/**
 * @brief - Gets the MFW crash dump configuration and logs info.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param p_mdump_info
 *
 * @param return 0 upon success.
 */
int
qed_mcp_mdump_get_info(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       struct qed_mdump_info *p_mdump_info);

/**
 * @brief - Clears the MFW crash dump logs.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @param return 0 upon success.
 */
int qed_mcp_mdump_clear_logs(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief - Clear the mdump retained data.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @param return 0 upon success.
 */
int qed_mcp_mdump_clr_retain(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief - Get mdump2 offset and size.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param buff_byte_size - pointer to get mdump2 buffer size in bytes
 * @param buff_byte_addr - pointer to get mdump2 buffer buffer address.
 * @param return 0 upon success.
 */
int qed_mdump2_req_offsize(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt,
			   u32 * buff_byte_size, u32 * buff_byte_addr);

/**
 * @brief - Request to free mdump2 buffer.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param return 0 upon success.
 */
int qed_mdump2_req_free(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief - Gets the LLDP MAC address.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param lldp_mac_addr - a buffer to be filled with the read LLDP MAC address.
 *
 * @param return 0 upon success.
 */
int qed_mcp_get_lldp_mac(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt, u8 lldp_mac_addr[ETH_ALEN]);

/**
 * @brief - Sets the LLDP MAC address.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param lldp_mac_addr - a buffer with the LLDP MAC address to be written.
 *
 * @param return 0 upon success.
 */
int qed_mcp_set_lldp_mac(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt, u8 lldp_mac_addr[ETH_ALEN]);

/**
 * @brief - Processes the TLV request from MFW i.e., get the required TLV info
 *          from the qed client and send it to the MFW.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @param return 0 upon success.
 */
int qed_mfw_process_tlv_req(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief - Update fcoe vlan id value to the MFW.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param vlan - fcoe vlan
 *
 * @param return 0 upon success.
 */
int
qed_mcp_update_fcoe_cvid(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt, u16 vlan);

/**
 * @brief - Update fabric name (wwn) value to the MFW.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param wwn - world wide name
 *
 * @param return 0 upon success.
 */
int
qed_mcp_update_fcoe_fabric_name(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt, u8 * wwn);

/**
 * @brief - Return whether management firmware support smart AN
 *
 * @param p_hwfn
 *
 * @return bool - true if feature is supported.
 */
bool qed_mcp_is_smart_an_supported(struct qed_hwfn *p_hwfn);

/**
 * @brief - Return whether management firmware support setting of
 *          PCI relaxed ordering.
 *
 * @param p_hwfn
 *
 * @return bool - true if feature is supported.
 */
bool qed_mcp_rlx_odr_supported(struct qed_hwfn *p_hwfn);

/**
 * @brief - Triggers a HW dump procedure.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @param return 0 upon success.
 */
int qed_mcp_hw_dump_trigger(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

int
qed_mcp_nvm_get_cfg(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    u16 option_id,
		    u8 entity_id, u16 flags, u8 * p_buf, u32 * p_len);

int
qed_mcp_nvm_set_cfg(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    u16 option_id,
		    u8 entity_id, u16 flags, u8 * p_buf, u32 len);

int
qed_mcp_is_tx_flt_attn_enabled(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, u8 * enabled);

int
qed_mcp_is_rx_los_attn_enabled(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, u8 * enabled);

int
qed_mcp_enable_tx_flt_attn(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, u8 enable);

int
qed_mcp_enable_rx_los_attn(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, u8 enable);

/**
 * @brief - Gets the permanent VF MAC address of the given relative VF ID.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param rel_vf_id
 * @param mac_addr - a buffer to be filled with the read VF MAC address.
 *
 * @param return 0 upon success.
 */
int qed_mcp_get_perm_vf_mac(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt,
			    u32 rel_vf_id, u8 mac_addr[ETH_ALEN]);

/**
 * @brief Send raw debug data to the MFW
 *
 * @param p_hwfn
 * @param p_ptt
 * @param p_buf - raw debug data buffer
 * @param size - buffer size
 */
int
qed_mcp_send_raw_debug_data(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, u8 * p_buf, u32 size);

/**
 * @brief Configure min/max bandwidths.
 *
 * @param cdev
 *
 * @return int
 */
int
qed_mcp_set_bandwidth(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, u8 bw_min, u8 bw_max);

/**
 * @brief - Return whether management firmware support ESL or not.
 *
 * @param cdev
 *
 * @return bool - true if feature is supported.
 */
bool qed_mcp_is_esl_supported(struct qed_hwfn *p_hwfn);

/**
 * @brief Get enhanced system lockdown status
 *
 * @param p_hwfn
 * @param p_ptt
 * @param active - ESL active status
 */
int
qed_mcp_get_esl_status(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt, bool * active);

/**
 * @brief - Instruct mfw to collect idlechk and fw asserts.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @param return 0 upon success.
 */
int qed_mcp_gen_mdump_idlechk(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/* Using hwfn number (and not pf_num) is required since in CMT mode,
 * same pf_num may be used by two different hwfn
 * TODO - this shouldn't really be in .h file, but until all fields
 * required during hw-init will be placed in their correct place in shmem
 * we need it in qed_dev.c [for readin the nvram reflection in shmem].
 */
#define MCP_PF_ID_BY_REL(p_hwfn, rel_pfid) (QED_IS_BB((p_hwfn)->cdev) ?	       \
					    ((rel_pfid) |		       \
					     ((p_hwfn)->abs_pf_id & 1) << 3) : \
					    rel_pfid)
#define MCP_PF_ID(p_hwfn)       MCP_PF_ID_BY_REL(p_hwfn, (p_hwfn)->rel_pf_id)

struct qed_mcp_info {
	/* Flag to indicate whether the MFW is running */
	bool b_mfw_running;

	/* List for mailbox commands which were sent and wait for a response */
	struct list_head cmd_list;

	/* Spinlock used for protecting the access to the mailbox commands list
	 * and the sending of the commands.
	 */
	spinlock_t cmd_lock;

	/* Flag to indicate whether sending a MFW mailbox command is blocked */
	bool b_block_cmd;

	/* Flag to indicate whether MFW was halted */
	bool b_halted;

	/* Flag to indicate that driver discovered that mcp is running in
	 * recovery mode.
	 */
	bool recovery_mode;

	/* Spinlock used for syncing SW link-changes and link-changes
	 * originating from attention context.
	 */
	spinlock_t link_lock;

	/* Address of the MCP public area */
	u32 public_base;
	/* Address of the driver mailbox */
	u32 drv_mb_addr;
	/* Address of the MFW mailbox */
	u32 mfw_mb_addr;
	/* Address of the port configuration (link) */
	u32 port_addr;
	/* size of the port configuration (for compatability) */
	u32 port_size;

	/* Current driver mailbox sequence */
	u16 drv_mb_seq;
	/* Current driver pulse sequence */
	u16 drv_pulse_seq;

	struct qed_mcp_link_params link_input;
	struct qed_mcp_link_state link_output;
	struct qed_mcp_link_capabilities link_capabilities;

	struct qed_mcp_function_info func_info;

	u8 *mfw_mb_cur;
	u8 *mfw_mb_shadow;
	u16 mfw_mb_length;
	u32 mcp_hist;

	/* Capabilities negotiated with the MFW */
	u32 capabilities;

	/* S/N for debug data mailbox commands and a spinlock to protect it */
	u16 dbg_data_seq;
	spinlock_t dbg_data_lock;
	spinlock_t unload_lock;
	unsigned long mcp_handling_status;
#define QED_MCP_BYPASS_PROC_BIT 0
#define QED_MCP_IN_PROCESSING_BIT       1
};

struct qed_mcp_name_table {
	u32 value;
	char name[64];
};

static const struct qed_mcp_name_table qed_mcp_cmd_name_table[] = {
	{DRV_MSG_CODE_NVM_PUT_FILE_BEGIN,
	 "DRV_MSG_CODE_NVM_PUT_FILE_BEGIN"},
	{DRV_MSG_CODE_NVM_PUT_FILE_DATA,
	 "DRV_MSG_CODE_NVM_PUT_FILE_DATA"},
	{DRV_MSG_CODE_NVM_GET_FILE_ATT,
	 "DRV_MSG_CODE_NVM_GET_FILE_ATT"},
	{DRV_MSG_CODE_NVM_READ_NVRAM,
	 "DRV_MSG_CODE_NVM_READ_NVRAM"},
	{DRV_MSG_CODE_NVM_WRITE_NVRAM,
	 "DRV_MSG_CODE_NVM_WRITE_NVRAM"},
	{DRV_MSG_CODE_NVM_DEL_FILE, "DRV_MSG_CODE_NVM_DEL_FILE"},
	{DRV_MSG_CODE_MCP_RESET, "DRV_MSG_CODE_MCP_RESET"},
	{DRV_MSG_CODE_PHY_RAW_READ, "DRV_MSG_CODE_PHY_RAW_READ"},
	{DRV_MSG_CODE_PHY_RAW_WRITE,
	 "DRV_MSG_CODE_PHY_RAW_WRITE"},
	{DRV_MSG_CODE_PHY_CORE_READ,
	 "DRV_MSG_CODE_PHY_CORE_READ"},
	{DRV_MSG_CODE_PHY_CORE_WRITE,
	 "DRV_MSG_CODE_PHY_CORE_WRITE"},
	{DRV_MSG_CODE_SET_VERSION, "DRV_MSG_CODE_SET_VERSION"},
	{DRV_MSG_CODE_MCP_HALT, "DRV_MSG_CODE_MCP_HALT"},
	{DRV_MSG_CODE_SET_VMAC, "DRV_MSG_CODE_SET_VMAC"},
	{DRV_MSG_CODE_GET_VMAC, "DRV_MSG_CODE_GET_VMAC"},
	{DRV_MSG_CODE_GET_STATS, "DRV_MSG_CODE_GET_STATS"},
	{DRV_MSG_CODE_PMD_DIAG_DUMP,
	 "DRV_MSG_CODE_PMD_DIAG_DUMP"},
	{DRV_MSG_CODE_PMD_DIAG_EYE, "DRV_MSG_CODE_PMD_DIAG_EYE"},
	{DRV_MSG_CODE_TRANSCEIVER_READ,
	 "DRV_MSG_CODE_TRANSCEIVER_READ"},
	{DRV_MSG_CODE_TRANSCEIVER_WRITE,
	 "DRV_MSG_CODE_TRANSCEIVER_WRITE"},
	{DRV_MSG_CODE_OCBB_DATA, "DRV_MSG_CODE_OCBB_DATA"},
	{DRV_MSG_CODE_SET_BW, "DRV_MSG_CODE_SET_BW"},
	{DRV_MSG_CODE_MASK_PARITIES,
	 "DRV_MSG_CODE_MASK_PARITIES"},
	{DRV_MSG_CODE_INDUCE_FAILURE,
	 "DRV_MSG_CODE_INDUCE_FAILURE"},
	{DRV_MSG_CODE_GPIO_READ, "DRV_MSG_CODE_GPIO_READ"},
	{DRV_MSG_CODE_GPIO_WRITE, "DRV_MSG_CODE_GPIO_WRITE"},
	{DRV_MSG_CODE_BIST_TEST, "DRV_MSG_CODE_BIST_TEST"},
	{DRV_MSG_CODE_GET_TEMPERATURE,
	 "DRV_MSG_CODE_GET_TEMPERATURE"},
	{DRV_MSG_CODE_SET_LED_MODE, "DRV_MSG_CODE_SET_LED_MODE"},
	{DRV_MSG_CODE_TIMESTAMP, "DRV_MSG_CODE_TIMESTAMP"},
	{DRV_MSG_CODE_EMPTY_MB, "DRV_MSG_CODE_EMPTY_MB"},
	{DRV_MSG_CODE_RESOURCE_CMD, "DRV_MSG_CODE_RESOURCE_CMD"},
	{DRV_MSG_CODE_GET_MBA_VERSION,
	 "DRV_MSG_CODE_GET_MBA_VERSION"},
	{DRV_MSG_CODE_MDUMP_CMD, "DRV_MSG_CODE_MDUMP_CMD"},
	{DRV_MSG_CODE_MEM_ECC_EVENTS,
	 "DRV_MSG_CODE_MEM_ECC_EVENTS"},
	{DRV_MSG_CODE_GPIO_INFO, "DRV_MSG_CODE_GPIO_INFO"},
	{DRV_MSG_CODE_EXT_PHY_READ, "DRV_MSG_CODE_EXT_PHY_READ"},
	{DRV_MSG_CODE_EXT_PHY_WRITE,
	 "DRV_MSG_CODE_EXT_PHY_WRITE"},
	{DRV_MSG_CODE_EXT_PHY_FW_UPGRADE,
	 "DRV_MSG_CODE_EXT_PHY_FW_UPGRADE"},
	{DRV_MSG_CODE_GET_PF_RDMA_PROTOCOL,
	 "DRV_MSG_CODE_GET_PF_RDMA_PROTOCOL"},
	{DRV_MSG_CODE_SET_LLDP_MAC, "DRV_MSG_CODE_SET_LLDP_MAC"},
	{DRV_MSG_CODE_GET_LLDP_MAC, "DRV_MSG_CODE_GET_LLDP_MAC"},
	{DRV_MSG_CODE_OS_WOL, "DRV_MSG_CODE_OS_WOL"},
	{DRV_MSG_CODE_GET_TLV_DONE, "DRV_MSG_CODE_GET_TLV_DONE"},
	{DRV_MSG_CODE_FEATURE_SUPPORT,
	 "DRV_MSG_CODE_FEATURE_SUPPORT"},
	{DRV_MSG_CODE_GET_MFW_FEATURE_SUPPORT,
	 "DRV_MSG_CODE_GET_MFW_FEATURE_SUPPORT"},
	{DRV_MSG_CODE_READ_WOL_REG, "DRV_MSG_CODE_READ_WOL_REG"},
	{DRV_MSG_CODE_WRITE_WOL_REG,
	 "DRV_MSG_CODE_WRITE_WOL_REG"},
	{DRV_MSG_CODE_GET_WOL_BUFFER,
	 "DRV_MSG_CODE_GET_WOL_BUFFER"},
	{DRV_MSG_CODE_ATTRIBUTE, "DRV_MSG_CODE_ATTRIBUTE"},
	{DRV_MSG_CODE_ENCRYPT_PASSWORD,
	 "DRV_MSG_CODE_ENCRYPT_PASSWORD"},
	{DRV_MSG_CODE_GET_ENGINE_CONFIG,
	 "DRV_MSG_CODE_GET_ENGINE_CONFIG"},
	{DRV_MSG_CODE_PMBUS_READ, "DRV_MSG_CODE_PMBUS_READ"},
	{DRV_MSG_CODE_PMBUS_WRITE, "DRV_MSG_CODE_PMBUS_WRITE"},
	{DRV_MSG_CODE_GENERIC_IDC, "DRV_MSG_CODE_GENERIC_IDC"},
	{DRV_MSG_CODE_RESET_CHIP, "DRV_MSG_CODE_RESET_CHIP"},
	{DRV_MSG_CODE_SET_RETAIN_VMAC,
	 "DRV_MSG_CODE_SET_RETAIN_VMAC"},
	{DRV_MSG_CODE_GET_RETAIN_VMAC,
	 "DRV_MSG_CODE_GET_RETAIN_VMAC"},
	{DRV_MSG_CODE_GET_NVM_CFG_OPTION,
	 "DRV_MSG_CODE_GET_NVM_CFG_OPTION"},
	{DRV_MSG_CODE_SET_NVM_CFG_OPTION,
	 "DRV_MSG_CODE_SET_NVM_CFG_OPTION"},
	{DRV_MSG_CODE_PCIE_STATS_START,
	 "DRV_MSG_CODE_PCIE_STATS_START"},
	{DRV_MSG_CODE_PCIE_STATS_GET,
	 "DRV_MSG_CODE_PCIE_STATS_GET"},
	{DRV_MSG_CODE_GET_ATTN_CONTROL,
	 "DRV_MSG_CODE_GET_ATTN_CONTROL"},
	{DRV_MSG_CODE_SET_ATTN_CONTROL,
	 "DRV_MSG_CODE_SET_ATTN_CONTROL"},
	{DRV_MSG_CODE_SET_TRACE_FILTER,
	 "DRV_MSG_CODE_SET_TRACE_FILTER"},
	{DRV_MSG_CODE_RESTORE_TRACE_FILTER,
	 "DRV_MSG_CODE_RESTORE_TRACE_FILTER"},
	{DRV_MSG_CODE_INITIATE_FLR_DEPRECATED,
	 "DRV_MSG_CODE_INITIATE_FLR_DEPRECATED"},
	{DRV_MSG_CODE_INITIATE_PF_FLR,
	 "DRV_MSG_CODE_INITIATE_PF_FLR"},
	{DRV_MSG_CODE_INITIATE_VF_FLR,
	 "DRV_MSG_CODE_INITIATE_VF_FLR"},
	{DRV_MSG_CODE_LOAD_REQ, "DRV_MSG_CODE_LOAD_REQ"},
	{DRV_MSG_CODE_LOAD_DONE, "DRV_MSG_CODE_LOAD_DONE"},
	{DRV_MSG_CODE_INIT_HW, "DRV_MSG_CODE_INIT_HW"},
	{DRV_MSG_CODE_CANCEL_LOAD_REQ,
	 "DRV_MSG_CODE_CANCEL_LOAD_REQ"},
	{DRV_MSG_CODE_UNLOAD_REQ, "DRV_MSG_CODE_UNLOAD_REQ"},
	{DRV_MSG_CODE_UNLOAD_DONE, "DRV_MSG_CODE_UNLOAD_DONE"},
	{DRV_MSG_CODE_INIT_PHY, "DRV_MSG_CODE_INIT_PHY"},
	{DRV_MSG_CODE_LINK_RESET, "DRV_MSG_CODE_LINK_RESET"},
	{DRV_MSG_CODE_SET_LLDP, "DRV_MSG_CODE_SET_LLDP"},
	{DRV_MSG_CODE_REGISTER_LLDP_TLVS_RX,
	 "DRV_MSG_CODE_REGISTER_LLDP_TLVS_RX"},
	{DRV_MSG_CODE_SET_DCBX, "DRV_MSG_CODE_SET_DCBX"},
	{DRV_MSG_CODE_OV_UPDATE_CURR_CFG,
	 "DRV_MSG_CODE_OV_UPDATE_CURR_CFG"},
	{DRV_MSG_CODE_OV_UPDATE_BUS_NUM,
	 "DRV_MSG_CODE_OV_UPDATE_BUS_NUM"},
	{DRV_MSG_CODE_OV_UPDATE_BOOT_PROGRESS,
	 "DRV_MSG_CODE_OV_UPDATE_BOOT_PROGRESS"},
	{DRV_MSG_CODE_OV_UPDATE_STORM_FW_VER,
	 "DRV_MSG_CODE_OV_UPDATE_STORM_FW_VER"},
	{DRV_MSG_CODE_NIG_DRAIN, "DRV_MSG_CODE_NIG_DRAIN"},
	{DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE,
	 "DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE"},
	{DRV_MSG_CODE_BW_UPDATE_ACK,
	 "DRV_MSG_CODE_BW_UPDATE_ACK"},
	{DRV_MSG_CODE_OV_UPDATE_MTU,
	 "DRV_MSG_CODE_OV_UPDATE_MTU"},
	{DRV_MSG_CODE_OV_UPDATE_WOL,
	 "DRV_MSG_CODE_OV_UPDATE_WOL"},
	{DRV_MSG_CODE_OV_UPDATE_ESWITCH_MODE,
	 "DRV_MSG_CODE_OV_UPDATE_ESWITCH_MODE"},
	{DRV_MSG_CODE_S_TAG_UPDATE_ACK,
	 "DRV_MSG_CODE_S_TAG_UPDATE_ACK"},
	{DRV_MSG_CODE_OEM_UPDATE_FCOE_CVID,
	 "DRV_MSG_CODE_OEM_UPDATE_FCOE_CVID"},
	{DRV_MSG_CODE_OEM_UPDATE_FCOE_FABRIC_NAME,
	 "DRV_MSG_CODE_OEM_UPDATE_FCOE_FABRIC_NAME"},
	{DRV_MSG_CODE_OEM_UPDATE_BOOT_CFG,
	 "DRV_MSG_CODE_OEM_UPDATE_BOOT_CFG"},
	{DRV_MSG_CODE_OEM_RESET_TO_DEFAULT,
	 "DRV_MSG_CODE_OEM_RESET_TO_DEFAULT"},
	{DRV_MSG_CODE_OV_GET_CURR_CFG,
	 "DRV_MSG_CODE_OV_GET_CURR_CFG"},
	{DRV_MSG_CODE_GET_OEM_UPDATES,
	 "DRV_MSG_CODE_GET_OEM_UPDATES"},
	{DRV_MSG_CODE_GET_LLDP_STATS,
	 "DRV_MSG_CODE_GET_LLDP_STATS"},
	{DRV_MSG_CODE_GET_PPFID_BITMAP,
	 "DRV_MSG_CODE_GET_PPFID_BITMAP"},
	{DRV_MSG_CODE_VF_DISABLED_DONE,
	 "DRV_MSG_CODE_VF_DISABLED_DONE"},
	{DRV_MSG_CODE_CFG_VF_MSIX, "DRV_MSG_CODE_CFG_VF_MSIX"},
	{DRV_MSG_CODE_CFG_PF_VFS_MSIX,
	 "DRV_MSG_CODE_CFG_PF_VFS_MSIX"},
	{DRV_MSG_CODE_GET_PERM_MAC, "DRV_MSG_CODE_GET_PERM_MAC"},
	{DRV_MSG_CODE_DEBUG_DATA_SEND,
	 "DRV_MSG_CODE_DEBUG_DATA_SEND"},
	{DRV_MSG_CODE_GET_FCOE_CAP, "DRV_MSG_CODE_GET_FCOE_CAP"},
	{DRV_MSG_CODE_VF_WITH_MORE_16SB,
	 "DRV_MSG_CODE_VF_WITH_MORE_16SB"},
	{DRV_MSG_CODE_GET_MANAGEMENT_STATUS,
	 "DRV_MSG_CODE_GET_MANAGEMENT_STATUS"},
	{DRV_MSG_GET_RESOURCE_ALLOC_MSG,
	 "DRV_MSG_GET_RESOURCE_ALLOC_MSG"},
	{DRV_MSG_SET_RESOURCE_VALUE_MSG,
	 "DRV_MSG_SET_RESOURCE_VALUE_MSG"},
};				/* LOOKUP_TABLE_END */

struct qed_mcp_mb_params {
	u32 cmd;
	u32 param;
	void *p_data_src;
	void *p_data_dst;
	u8 data_src_size;
	u8 data_dst_size;
	u32 mcp_resp;
	u32 mcp_param;
	u32 flags;
#define QED_MB_FLAG_CAN_SLEEP           (0x1 << 0)
#define QED_MB_FLAG_AVOID_BLOCK (0x1 << 1)
#define QED_MB_FLAGS_IS_SET(params, flag) \
	((params) != NULL && ((params)->flags & QED_MB_FLAG_ ## flag))
};

struct qed_drv_tlv_hdr {
	u8 tlv_type;		/* According to the enum below */
	u8 tlv_length;		/* In dwords - not including this header */
	u8 tlv_reserved;
#define QED_DRV_TLV_FLAGS_CHANGED 0x01
	u8 tlv_flags;
};

/**
 * @brief Initialize the interface with the MCP
 *
 * @param p_hwfn - HW func
 * @param p_ptt - PTT required for register access
 *
 * @return int
 */
int qed_mcp_cmd_init(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief Initialize the port interface with the MCP
 *
 * @param p_hwfn
 * @param p_ptt
 * Can only be called after `num_ports_in_engine' is set
 */
void qed_mcp_cmd_port_init(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
/**
 * @brief Releases resources allocated during the init process.
 *
 * @param p_hwfn - HW func
 * @param p_ptt - PTT required for register access
 *
 * @return int
 */

int qed_mcp_free(struct qed_hwfn *p_hwfn);

/**
 * @brief This function is called from the DPC context. After
 * pointing PTT to the mfw mb, check for events sent by the MCP
 * to the driver and ack them. In case a critical event
 * detected, it will be handled here, otherwise the work will be
 * queued to a sleepable work-queue.
 *
 * @param p_hwfn - HW function
 * @param p_ptt - PTT required for register access
 * @return int - 0 - operation
 * was successul.
 */
int qed_mcp_handle_events(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief When MFW doesn't get driver pulse for couple of seconds, at some
 * threshold before timeout expires, it will generate interrupt
 * through a dedicated status block (DPSB - Driver Pulse Status
 * Block), which the driver should respond immediately, by
 * providing keepalive indication after setting the PTT to the
 * driver-MFW mailbox. This function is called directly from the
 * DPC upon receiving the DPSB attention.
 *
 * @param p_hwfn - hw function
 * @param p_ptt - PTT required for register access
 * @return int - 0 - operation
 * was successful.
 */
int qed_issue_pulse(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

enum qed_drv_role {
	QED_DRV_ROLE_OS,
	QED_DRV_ROLE_KDUMP,
};

struct qed_load_req_params {
	/* Input params */
	enum qed_drv_role drv_role;
	u8 timeout_val;		/* 1..254, '0' - default value, '255' - no timeout */
	bool avoid_eng_reset;
	enum qed_override_force_load override_force_load;

	/* Output params */
	u32 load_code;
};

/**
 * @brief Sends a LOAD_REQ to the MFW, and in case the operation succeeds,
 *        returns whether this PF is the first on the engine/port or function.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param p_params
 *
 * @return int - 0 - Operation was successful.
 */
int qed_mcp_load_req(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt,
		     struct qed_load_req_params *p_params);

/**
 * @brief Sends a LOAD_DONE message to the MFW
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @return int - 0 - Operation was successful.
 */
int qed_mcp_load_done(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief Sends a CANCEL_LOAD_REQ message to the MFW
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @return int - 0 - Operation was successful.
 */
int qed_mcp_cancel_load_req(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief Sends a UNLOAD_REQ message to the MFW
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @return int - 0 - Operation was successful.
 */
int qed_mcp_unload_req(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief Sends a UNLOAD_DONE message to the MFW
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @return int - 0 - Operation was successful.
 */
int qed_mcp_unload_done(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief Read the MFW mailbox into Current buffer.
 *
 * @param p_hwfn
 * @param p_ptt
 */
void qed_mcp_read_mb(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief Ack to mfw that driver finished FLR process for VFs
 *
 * @param p_hwfn
 * @param p_ptt
 * @param vfs_to_ack - bit mask of all engine VFs for which the PF acks.
 *
 * @param return int - 0 upon success.
 */
int qed_mcp_ack_vf_flr(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt, u32 * vfs_to_ack);
int qed_mcp_vf_flr(struct qed_hwfn *p_hwfn,
		   struct qed_ptt *p_ptt, u8 rel_vf_id);

/**
 * @brief - calls during init to read shmem of all function-related info.
 *
 * @param p_hwfn
 *
 * @param return 0 upon success.
 */
int qed_mcp_fill_shmem_func_info(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt);

/**
 * @brief - Reset the MCP using mailbox command.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @param return 0 upon success.
 */
int qed_mcp_reset(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief indicates whether the MFW objects [under mcp_info] are accessible
 *
 * @param p_hwfn
 *
 * @return true iff MFW is running and mcp_info is initialized
 */
bool qed_mcp_is_init(struct qed_hwfn *p_hwfn);

/**
 * @brief request MFW to configure MSI-X for a VF
 *
 * @param p_hwfn
 * @param p_ptt
 * @param vf_id - absolute inside engine
 * @param num_sbs - number of entries to request
 *
 * @return int
 */
int qed_mcp_config_vf_msix(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, u8 vf_id, u8 num);

/**
 * @brief - Halt the MCP.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @param return 0 upon success.
 */
int qed_mcp_halt(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief - Wake up the MCP.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @param return 0 upon success.
 */
int qed_mcp_resume(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
int __qed_configure_pf_max_bandwidth(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     struct qed_mcp_link_state *p_link,
				     u8 max_bw);
int __qed_configure_pf_min_bandwidth(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     struct qed_mcp_link_state *p_link,
				     u8 min_bw);
int qed_mcp_mask_parities(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, u32 mask_parities);
#if 0
int qed_hw_init_first_eth(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, u8 * p_pf);
#endif

/**
 * @brief - Sends crash mdump related info to the MFW.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param epoch
 *
 * @param return 0 upon success.
 */
int qed_mcp_mdump_set_values(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, u32 epoch);

/**
 * @brief - Triggers a MFW crash dump procedure.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @param return 0 upon success.
 */
int qed_mcp_mdump_trigger(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

struct qed_mdump_retain_data {
	u32 valid;
	u32 epoch;
	u32 pf;
	u32 status;
};

/**
 * @brief - Gets the mdump retained data from the MFW.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param p_mdump_retain
 *
 * @param return 0 upon success.
 */
int
qed_mcp_mdump_get_retain(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 struct qed_mdump_retain_data *p_mdump_retain);

/**
 * @brief - Sets the MFW's max value for the given resource
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param res_id
 *  @param resc_max_val
 *  @param p_mcp_resp
 *
 * @return int - 0 - operation was successful.
 */
int
qed_mcp_set_resc_max_val(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 enum qed_resources res_id,
			 u32 resc_max_val, u32 * p_mcp_resp);

/**
 * @brief - Gets the MFW allocation info for the given resource
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param res_id
 *  @param p_mcp_resp
 *  @param p_resc_num
 *  @param p_resc_start
 *
 * @return int - 0 - operation was successful.
 */
int
qed_mcp_get_resc_info(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt,
		      enum qed_resources res_id,
		      u32 * p_mcp_resp, u32 * p_resc_num, u32 * p_resc_start);

/**
 * @brief - Initiates PF FLR
 *
 *  @param p_hwfn
 *  @param p_ptt
 *
 * @return int - 0 - operation was successful.
 */
int qed_mcp_initiate_pf_flr(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

#define QED_MCP_RESC_LOCK_MIN_VAL       RESOURCE_DUMP	/* 0 */
#define QED_MCP_RESC_LOCK_MAX_VAL       31

enum qed_resc_lock {
	QED_RESC_LOCK_DBG_DUMP = QED_MCP_RESC_LOCK_MIN_VAL,
	/* Locks that the MFW is aware of should be added here downwards */

	/* QED only locks should be added here upwards */
	QED_RESC_LOCK_QM_RECONF = 25,
	QED_RESC_LOCK_IND_TABLE = 26,
	QED_RESC_LOCK_PTP_PORT0 = 27,
	QED_RESC_LOCK_PTP_PORT1 = 28,
	QED_RESC_LOCK_PTP_PORT2 = 29,
	QED_RESC_LOCK_PTP_PORT3 = 30,
	QED_RESC_LOCK_RESC_ALLOC = QED_MCP_RESC_LOCK_MAX_VAL,

	/* A dummy value to be used for auxillary functions in need of
	 * returning an 'error' value.
	 */
	QED_RESC_LOCK_RESC_INVALID,
};

struct qed_resc_lock_params {
	/* Resource number [valid values are 0..31] */
	u8 resource;

	/* Lock timeout value in seconds [default, none or 1..254] */
	u8 timeout;
#define QED_MCP_RESC_LOCK_TO_DEFAULT    0
#define QED_MCP_RESC_LOCK_TO_NONE       255

	/* Number of times to retry locking */
	u8 retry_num;
#define QED_MCP_RESC_LOCK_RETRY_CNT_DFLT        10

	/* The interval in usec between retries */
	u32 retry_interval;
#define QED_MCP_RESC_LOCK_RETRY_VAL_DFLT        10000

	/* Use sleep or delay between retries */
	bool sleep_b4_retry;

	/* Will be set as true if the resource is free and granted */
	bool b_granted;

	/* Will be filled with the resource owner.
	 * [0..15 = PF0-15, 16 = MFW, 17 = diag over serial]
	 */
	u8 owner;
};

/**
 * @brief Acquires MFW generic resource lock
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param p_params
 *
 * @return int - 0 - operation was successful.
 */
int
qed_mcp_resc_lock(struct qed_hwfn *p_hwfn,
		  struct qed_ptt *p_ptt, struct qed_resc_lock_params *p_params);

struct qed_resc_unlock_params {
	/* Resource number [valid values are 0..31] */
	u8 resource;

	/* Allow to release a resource even if belongs to another PF */
	bool b_force;

	/* Will be set as true if the resource is released */
	bool b_released;
};

/**
 * @brief Releases MFW generic resource lock
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param p_params
 *
 * @return int - 0 - operation was successful.
 */
int
qed_mcp_resc_unlock(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    struct qed_resc_unlock_params *p_params);

/**
 * @brief - default initialization for lock/unlock resource structs
 *
 * @param p_lock - lock params struct to be initialized; Can be NULL
 * @param p_unlock - unlock params struct to be initialized; Can be NULL
 * @param resource - the requested resource
 * @paral b_is_permanent - disable retries & aging when set
 */
void qed_mcp_resc_lock_default_init(struct qed_hwfn *p_hwfn,
				    struct qed_resc_lock_params *p_lock,
				    struct qed_resc_unlock_params *p_unlock,
				    enum qed_resc_lock
				    resource, bool b_is_permanent);

void qed_mcp_wol_wr(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt, u32 offset, u32 val);

/**
 * @brief Learn of supported MFW features; To be done during early init
 *
 * @param p_hwfn
 * @param p_ptt
 */
int qed_mcp_get_capabilities(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief Inform MFW of set of features supported by driver. Should be done
 * inside the contet of the LOAD_REQ.
 *
 * @param p_hwfn
 * @param p_ptt
 */
int qed_mcp_set_capabilities(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief Initialize MFW mailbox and sequence values for driver interaction.
 *
 * @param p_hwfn
 * @param p_ptt
 */
int qed_load_mcp_offsets(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

enum qed_mcp_drv_attr_cmd {
	QED_MCP_DRV_ATTR_CMD_READ,
	QED_MCP_DRV_ATTR_CMD_WRITE,
	QED_MCP_DRV_ATTR_CMD_READ_CLEAR,
	QED_MCP_DRV_ATTR_CMD_CLEAR,
};

struct qed_mcp_drv_attr {
	enum qed_mcp_drv_attr_cmd attr_cmd;
	u32 attr_num;

	/* R/RC - will be set with the read value
	 * W - should hold the required value to be written
	 * C - DC
	 */
	u32 val;

	/* W - mask/offset to be applied on the given value
	 * R/RC/C - DC
	 */
	u32 mask;
	u32 offset;
};

/**
 * @brief Handle the drivers' attributes that are kept by the MFW.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param p_drv_attr
 */
int
qed_mcp_drv_attribute(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt,
		      struct qed_mcp_drv_attr *p_drv_attr);

/**
 * @brief Read ufp config from the shared memory.
 *
 * @param p_hwfn
 * @param p_ptt
 */
void qed_mcp_read_ufp_config(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief Read qinq config from the shared memory.
 *
 * @param p_hwfn
 * @param p_ptt
 */
void qed_mcp_read_qinq_config(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief Get the engine affinity configuration.
 *
 * @param p_hwfn
 * @param p_ptt
 */
int qed_mcp_get_engine_config(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief Get the PPFID bitmap.
 *
 * @param p_hwfn
 * @param p_ptt
 */
int qed_mcp_get_ppfid_bitmap(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief Acquire MCP lock to access to HW indirection table entries
 *
 * @param p_hwfn
 * @param p_ptt
 * @param retry_num
 * @param retry_interval
 */
int
qed_mcp_ind_table_lock(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt, u8 retry_num, u32 retry_interval);

/**
 * @brief Release MCP lock of access to HW indirection table entries
 *
 * @param p_hwfn
 * @param p_ptt
 */
int qed_mcp_ind_table_unlock(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief Populate the nvm info shadow in the given hardware function
 *
 * @param p_hwfn
 */
int qed_mcp_nvm_info_populate(struct qed_hwfn *p_hwfn);

/**
 * @brief Delete nvm info shadow in the given hardware function
 *
 * @param p_hwfn
 */
void qed_mcp_nvm_info_free(struct qed_hwfn *p_hwfn);

int
qed_mcp_set_trace_filter(struct qed_hwfn *p_hwfn,
			 u32 * dbg_level, u32 * dbg_modules);

int qed_mcp_restore_trace_filter(struct qed_hwfn *p_hwfn);
#endif
