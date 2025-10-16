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

#ifndef _QED_SRIOV_H
#define _QED_SRIOV_H
#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/crc32.h>
#include <linux/if_ether.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include "common_hsi.h"
#include "qed_hsi.h"
#include "qed_l2.h"
#include "qed_ll2.h"
#include "qed_rdma.h"
#include "qed_vf.h"

#define QED_ETH_VF_NUM_MAC_FILTERS 1
#define QED_ETH_VF_NUM_VLAN_FILTERS 2
#define QED_VF_ARRAY_LENGTH (3)

#define IS_VF(cdev)             ((cdev)->b_is_vf)
#define IS_PF(cdev)             (!((cdev)->b_is_vf))
#ifdef CONFIG_QED_SRIOV
#define IS_PF_SRIOV(p_hwfn)     (!!((p_hwfn)->cdev->p_iov_info))
#else
#define IS_PF_SRIOV(p_hwfn)     (0)
#endif
#define IS_PF_SRIOV_ALLOC(p_hwfn)       (!!((p_hwfn)->pf_iov_info))
#define IS_PF_PDA(p_hwfn)       0	/* @@TBD Michalk */

/* @@@ TBD MichalK - what should this number be*/
#define QED_MAX_QUEUE_VF_CHAINS_PER_PF 16
#define QED_MAX_CNQ_VF_CHAINS_PER_PF 16
#define QED_MAX_VF_CHAINS_PER_PF \
	(QED_MAX_QUEUE_VF_CHAINS_PER_PF + QED_MAX_CNQ_VF_CHAINS_PER_PF)

/* vport update extended feature tlvs flags */
enum qed_iov_vport_update_flag {
	QED_IOV_VP_UPDATE_ACTIVATE = 0,
	QED_IOV_VP_UPDATE_VLAN_STRIP = 1,
	QED_IOV_VP_UPDATE_TX_SWITCH = 2,
	QED_IOV_VP_UPDATE_MCAST = 3,
	QED_IOV_VP_UPDATE_ACCEPT_PARAM = 4,
	QED_IOV_VP_UPDATE_RSS = 5,
	QED_IOV_VP_UPDATE_ACCEPT_ANY_VLAN = 6,
	QED_IOV_VP_UPDATE_SGE_TPA = 7,
	QED_IOV_VP_UPDATE_MAX = 8,
};

/*PF to VF STATUS is part of vfpf-channel API
 * and must be forward compatible */
enum qed_iov_pf_to_vf_status {
	PFVF_STATUS_WAITING = 0,
	PFVF_STATUS_SUCCESS,
	PFVF_STATUS_FAILURE,
	PFVF_STATUS_NOT_SUPPORTED,
	PFVF_STATUS_NO_RESOURCE,
	PFVF_STATUS_FORCED,
	PFVF_STATUS_MALICIOUS,
	PFVF_STATUS_ACQUIRED,
	PFVF_STATUS_MAX,
};

struct qed_mcp_link_params;
struct qed_mcp_link_state;
struct qed_mcp_link_capabilities;

/* These defines are used by the hw-channel; should never change order */
#define VFPF_ACQUIRE_OS_LINUX (0)
#define VFPF_ACQUIRE_OS_WINDOWS (1)
#define VFPF_ACQUIRE_OS_ESX (2)
#define VFPF_ACQUIRE_OS_SOLARIS (3)
#define VFPF_ACQUIRE_OS_LINUX_USERSPACE (4)
#define VFPF_ACQUIRE_OS_FREEBSD (5)

struct qed_vf_acquire_sw_info {
	u32 driver_version;
	u8 os_type;
};

struct qed_public_vf_info {
	/* These copies will later be reflected in the bulletin board,
	 * but this copy should be newer.
	 */
	u8 forced_mac[ETH_ALEN];
	u16 forced_vlan;

	/* Trusted VFs can configure promiscuous mode and
	 * set MAC address inspite PF has set forced MAC.
	 * Also store shadow promisc configuration if needed.
	 */
	bool is_trusted_configured;
	bool is_trusted_request;
	u8 rx_accept_mode;
	u8 tx_accept_mode;
	u8 mac[ETH_ALEN];
	bool accept_any_vlan;

	/* IFLA_VF_LINK_STATE_<X> */
	int link_state;

	/* Currently configured Tx rate in MB/sec. 0 if unconfigured */
	int tx_rate;
};

struct qed_iov_vf_init_params {
	u16 rel_vf_id;

	/* Number of requested Queues; Currently, don't support different
	 * number of Rx/Tx queues.
	 */
	/* TODO - remove this limitation */
	u8 num_queues;
	u8 num_cnqs;

	u8 cnq_offset;

	/* Allow the client to choose which qzones to use for Rx/Tx,
	 * and which queue_base to use for Tx queues on a per-queue basis.
	 * Notice values should be relative to the PF resources.
	 */
	u16 req_rx_queue[QED_MAX_QUEUE_VF_CHAINS_PER_PF];
	u16 req_tx_queue[QED_MAX_QUEUE_VF_CHAINS_PER_PF];

	u8 vport_id;

	/* Should be set in case RSS is going to be used for VF */
	u8 rss_eng_id;
};

/* This struct is part of qed_dev and contains data relevant to all hwfns;
 * Initialized only if SR-IOV cpabability is exposed in PCIe config space.
 */
struct qed_hw_sriov_info {
	/* standard SRIOV capability fields, mostly for debugging */
	int pos;		/* capability position */
	int nres;		/* number of resources */
	u32 cap;		/* SR-IOV Capabilities */
	u16 ctrl;		/* SR-IOV Control */
	u16 total_vfs;		/* total VFs associated with the PF */
	u16 num_vfs;		/* number of vfs that have been started */
	u16 initial_vfs;	/* initial VFs associated with the PF */
	u16 nr_virtfn;		/* number of VFs available */
	u16 offset;		/* first VF Routing ID offset */
	u16 stride;		/* following VF stride */
	u16 vf_device_id;	/* VF device id */
	u32 pgsz;		/* page size for BAR alignment */
	u8 link;		/* Function Dependency Link */

	u32 first_vf_in_pf;
};

#ifdef CONFIG_QED_SRIOV
/**
 * @brief qed_iov_pci_enable_prolog - Called before enabling sriov on pci.
 *        Reconfigure QM to initialize PQs for the
 *        max_active_vfs.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param max_active_vfs
 *
 * @return int
 */
int qed_iov_pci_enable_prolog(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, u8 max_active_vfs);

/**
 * @brief qed_iov_pci_disable_epilog - Called after disabling sriov on pci.
 *        Reconfigure QM to delete VF PQs.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @return int
 */
int qed_iov_pci_disable_epilog(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief Check if given VF ID @vfid is valid
 *        w.r.t. @b_enabled_only value
 *        if b_enabled_only = true - only enabled VF id is valid
 *        else any VF id less than max_vfs is valid
 *
 * @param p_hwfn
 * @param rel_vf_id - Relative VF ID
 * @param b_enabled_only - consider only enabled VF
 * @param b_non_malicious - true iff we want to validate vf isn't malicious.
 *
 * @return bool - true for valid VF ID
 */
bool qed_iov_is_valid_vfid(struct qed_hwfn *p_hwfn,
			   int rel_vf_id,
			   bool b_enabled_only, bool b_non_malicious);

/**
 * @brief qed_pf_configure_vf_queue_coalesce - PF configure coalesce parameters
 *    of VFs for Rx and Tx queue.
 *    While the API allows setting coalescing per-qid, all queues sharing a SB
 *    should be in same range [i.e., either 0-0x7f, 0x80-0xff or 0x100-0x1ff]
 *    otherwise configuration would break.
 *
 * @param p_hwfn
 * @param rx_coal - Rx Coalesce value in micro seconds.
 * @param tx_coal - TX Coalesce value in micro seconds.
 * @param vf_id
 * @param qid
 *
 * @return int
 **/
int
qed_iov_pf_configure_vf_queue_coalesce(struct qed_hwfn *p_hwfn,
				       u16 rx_coal,
				       u16 tx_coal, u16 vf_id, u16 qid);

/**
 * @brief - Given a VF index, return index of next [including that] active VF.
 *
 * @param p_hwfn
 * @param rel_vf_id
 *
 * @return MAX_NUM_VFS_E4 in case no further active VFs, otherwise index.
 */
u16 qed_iov_get_next_active_vf(struct qed_hwfn *p_hwfn, u16 rel_vf_id);
void qed_iov_bulletin_set_udp_ports(struct qed_hwfn *p_hwfn,
				    int vfid, u16 vxlan_port, u16 geneve_port);

/**
 * @brief Run DORQ flush for VFs that may use EDPM, and notify VFs that need
 *        to perform doorbell overflow recovery. A VF needs to perform recovery
 *        if it or its PF overflowed.
 *
 * @param p_hwfn
 */
int qed_iov_db_rec_handler(struct qed_hwfn *p_hwfn);

#else
static inline bool
qed_iov_is_valid_vfid(struct qed_hwfn __maybe_unused * p_hwfn,
		      int __maybe_unused rel_vf_id,
		      bool __maybe_unused b_enabled_only,
		      bool __maybe_unused b_non_malicious)
{
	return false;
}

static inline void qed_iov_bulletin_set_udp_ports(struct qed_hwfn __maybe_unused
						  * p_hwfn,
						  int __maybe_unused vfid,
						  u16 __maybe_unused vxlan_port,
						  u16 __maybe_unused
						  geneve_port)
{
	return;
}

static inline u16 qed_iov_get_next_active_vf(struct qed_hwfn __maybe_unused *
					     p_hwfn,
					     u16 __maybe_unused rel_vf_id)
{
	return MAX_NUM_VFS;
}

static inline int qed_iov_db_rec_handler(struct qed_hwfn __maybe_unused *
					 p_hwfn)
{
	return -EINVAL;
}

#endif

#define qed_for_each_vf(_p_hwfn, _i)			  \
	for (_i = qed_iov_get_next_active_vf(_p_hwfn, 0); \
	     _i < MAX_NUM_VFS;				  \
	     _i = qed_iov_get_next_active_vf(_p_hwfn, _i + 1))

#define QED_ETH_MAX_VF_NUM_VLAN_FILTERS	\
	(MAX_NUM_VFS_E4 * QED_ETH_VF_NUM_VLAN_FILTERS)

/* Represents a full message. Both the request filled by VF
 * and the response filled by the PF. The VF needs one copy
 * of this message, it fills the request part and sends it to
 * the PF. The PF will copy the response to the response part for
 * the VF to later read it. The PF needs to hold a message like this
 * per VF, the request that is copied to the PF is placed in the
 * request size, and the response is filled by the PF before sending
 * it to the VF.
 */
struct qed_vf_mbx_msg {
	union vfpf_tlvs req;
	union pfvf_tlvs resp;
};

/* This mailbox is maintained per VF in its PF
 * contains all information required for sending / receiving
 * a message
 */
struct qed_iov_vf_mbx {
	union vfpf_tlvs *req_virt;
	dma_addr_t req_phys;
	union pfvf_tlvs *reply_virt;
	dma_addr_t reply_phys;

	/* Address in VF where a pending message is located */
	dma_addr_t pending_req;

	/* Message from VF awaits handling */
	bool b_pending_msg;

	u8 *offset;

	/* VF GPA address */
	u32 vf_addr_lo;
	u32 vf_addr_hi;

	struct vfpf_first_tlv first_tlv;	/* saved VF request header */

	u8 flags;
#define VF_MSG_INPROCESS        0x1	/* failsafe - the FW should prevent
					 * more then one pending msg
					 */
};

#define QED_IOV_LEGACY_QID_RX (0)
#define QED_IOV_LEGACY_QID_TX (1)
#define QED_IOV_QID_INVALID (0xFE)

struct qed_vf_queue_cid {
	bool b_is_tx;
	struct qed_queue_cid *p_cid;
};

/* Describes a qzone associated with the VF */
struct qed_vf_queue {
	/* Input from upper-layer, mapping relateive queue to queue-zone */
	u16 fw_rx_qid;
	u16 fw_tx_qid;

	struct qed_vf_queue_cid cids[MAX_QUEUES_PER_QZONE];
};

enum vf_state {
	VF_FREE = 0,		/* VF ready to be acquired holds no resc */
	VF_ACQUIRED = 1,	/* VF, aquired, but not initalized */
	VF_ENABLED = 2,		/* VF, Enabled */
	VF_RESET = 3,		/* VF, FLR'd, pending cleanup */
	VF_STOPPED = 4		/* VF, Stopped */
};

struct qed_vf_vlan_shadow {
	bool used;
	u16 vid;
};

struct qed_vf_shadow_config {
	/* Shadow copy of all guest vlans */
	struct qed_vf_vlan_shadow vlans[QED_ETH_VF_NUM_VLAN_FILTERS + 1];

	/* Shadow copy of all configured MACs; Empty if forcing MACs */
	u8 macs[QED_ETH_VF_NUM_MAC_FILTERS][ETH_ALEN];
	u8 inner_vlan_removal;
};

struct qed_vf_ll2_queue {
	u32 cid;
	u8 qid;			/* abs queue id */
	bool used;
};

struct qed_vf_db_rec_info {
	/* VF doorbell overflow counter */
	u32 count;

	/* VF doesn't need to flush DORQ if it doesn't use EDPM */
	bool db_bar_no_edpm;

	/* VF doorbell overflow sticky indicator was cleared in the DORQ
	 * attention callback, but still needs to execute doorbell recovery.
	 * This value doesn't require a lock but must use atomic operations.
	 */
	unsigned long overflow;
};

struct event_ring_list_entry {
	struct list_head list_entry;
	struct event_ring_entry eqe;
};

struct qed_vf_eq_info {
	bool eq_support;
	bool eq_active;
	struct list_head eq_list;
	spinlock_t eq_list_lock;
	u16 eq_list_size;
};

/* PFs maintain an array of this structure, per VF */
struct qed_vf_info {
	struct qed_iov_vf_mbx vf_mbx;
	enum vf_state state;
	bool b_init;
	bool b_malicious;
	u8 to_disable;

	struct qed_bulletin bulletin;
	dma_addr_t vf_bulletin;

	/* PF saves a copy of the last VF acquire message */
	struct vfpf_acquire_tlv acquire;

	u32 concrete_fid;
	u16 opaque_fid;
	u16 mtu;

	u8 vport_id;
	u8 rss_eng_id;
	bool rss_enabled;
	bool rss_ind_tbl_valid;

	/* indirection table size when the table was updated */
	u16 rss_ind_tbl_size;

	/* current configured rss table size which can be samller than
	 * rss_ind_tbl_size but not larger.
	 */
	u16 rss_tbl_size;
	u8 relative_vf_id;
	u8 abs_vf_id;
#define QED_VF_ABS_ID(p_hwfn, p_vf)     (QED_PATH_ID(p_hwfn) ?		      \
					 (p_vf)->abs_vf_id + MAX_NUM_VFS_BB : \
					 (p_vf)->abs_vf_id)

	u8 vport_instance;	/* Number of active vports */
	u8 num_rxqs;
	u8 num_txqs;
	u8 num_cnqs;
	u8 cnq_offset;

	/* The numbers which are communicated to the VF */
	u8 actual_num_rxqs;
	u8 actual_num_txqs;

	u16 cnq_sb_start_id;

	u16 rx_coal;
	u16 tx_coal;

	u8 num_sbs;

	u8 num_mac_filters;
	u8 num_vlan_filters;

	struct qed_vf_queue vf_queues[QED_MAX_QUEUE_VF_CHAINS_PER_PF];
	struct qed_vf_ll2_queue vf_ll2_queues[QED_MAX_NUM_OF_LL2_CONNS_VF];

	u16 igu_sbs[QED_MAX_VF_CHAINS_PER_PF];

	/* TODO - Only windows is using it - should be removed */
	u8 was_malicious;
	u8 num_active_rxqs;
	void *ctx;
	struct qed_public_vf_info p_vf_info;
	bool spoof_chk;		/* Current configured on HW */
	bool req_spoofchk_val;	/* Requested value */

	/* Stores the configuration requested by VF */
	struct qed_vf_shadow_config shadow_config;

	/* A bitfield using bulletin's valid-map bits, used to indicate
	 * which of the bulletin board features have been configured.
	 */
	u64 configured_features;
#define QED_IOV_CONFIGURED_FEATURES_MASK        (BIT(MAC_ADDR_FORCED) |	\
						 BIT(VLAN_ADDR_FORCED))
	struct qed_rdma_info *rdma_info;

	struct qed_vf_db_rec_info db_recovery_info;

	/* Pending EQs list to send to VF */
	struct qed_vf_eq_info vf_eq_info;
};

/* This structure is part of qed_hwfn and used only for PFs that have sriov
 * capability enabled.
 */
struct qed_pf_iov {
	struct qed_vf_info vfs_array[MAX_NUM_VFS];
	u64 pending_flr[QED_VF_ARRAY_LENGTH];

	/* Allocate message address continuosuly and split to each VF */
	void *mbx_msg_virt_addr;
	dma_addr_t mbx_msg_phys_addr;
	u32 mbx_msg_size;
	void *mbx_reply_virt_addr;
	dma_addr_t mbx_reply_phys_addr;
	u32 mbx_reply_size;
	void *p_bulletins;
	dma_addr_t bulletins_phys;
	u32 bulletins_size;

#define QED_IS_VF_RDMA(p_hwfn)  (((p_hwfn)->pf_iov_info) && \
				 ((p_hwfn)->pf_iov_info->rdma_enable))

	/* Indicates whether vf-rdma is supported */
	bool rdma_enable;

	/* Hold the original numbers of l2_queue and cnq features */
	u8 num_l2_queue_feat;
	u8 num_cnq_feat;

	struct qed_dpi_info dpi_info;

	void *p_rdma_info;

	/* VFs doorbell bar size is sufficient and they can use EDPM */
	bool vfs_edpm;

	/* Keep doorbell recovery statistics on corresponding VFs:
	 * Which VF overflowed the most, and how many times.
	 */
	u32 max_db_rec_count;
	u16 max_db_rec_vfid;

	/* PF doorbell overflow sticky indicator was cleared in the DORQ
	 * attention callback, but all its child VFs still need to execute
	 * doorbell recovery, because when PF overflows, VF doorbells are also
	 * silently dropped without raising the VF sticky.
	 * This value doesn't require a lock but must use atomic operations.
	 */
	unsigned long overflow;

	/* Holds the max number of VFs which can be loaded */
	u8 max_active_vfs;
};

#ifdef CONFIG_QED_SRIOV
/**
 * @brief Read sriov related information and allocated resources
 *  reads from configuraiton space, shmem, etc.
 *
 * @param p_hwfn
 *
 * @return int
 */
int qed_iov_hw_info(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_add_tlv - place a given tlv on the tlv buffer at next offset
 *
 * @param offset
 * @param type
 * @param length
 *
 * @return pointer to the newly placed tlv
 */
void *qed_add_tlv(u8 ** offset, u16 type, u16 length);

/**
 * @brief list the types and lengths of the tlvs on the buffer
 *
 * @param p_hwfn
 * @param tlvs_list
 */
void qed_dp_tlv_list(struct qed_hwfn *p_hwfn, void *tlvs_list);

/**
 * @brief qed_sriov_vfpf_malicious - handle malicious VF/PF
 *
 * @param p_fwfn
 * @param p_data
 */
void qed_sriov_vfpf_malicious(struct qed_hwfn *p_hwfn,
			      struct fw_err_data *p_data);

/**
 * @brief qed_sriov_eqe_event - callback for SRIOV events
 *
 * @param p_hwfn
 * @param opcode
 * @param echo
 * @param data
 * @param fw_return_code
 * @param vf_id
 *
 * @return int
 */
int qed_sriov_eqe_event(struct qed_hwfn *p_hwfn,
			u8 opcode,
			u16 __maybe_unused echo,
			union event_ring_data *data,
			u8 __maybe_unused fw_return_code, u8 vf_id);

/**
 * @brief qed_iov_alloc - allocate sriov related resources
 *
 * @param p_hwfn
 *
 * @return int
 */
int qed_iov_alloc(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_iov_setup - setup sriov related resources
 *
 * @param p_hwfn
 */
void qed_iov_setup(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_iov_free - free sriov related resources
 *
 * @param p_hwfn
 */
void qed_iov_free(struct qed_hwfn *p_hwfn);

/**
 * @brief free sriov related memory that was allocated during hw_prepare
 *
 * @param cdev
 */
void qed_iov_free_hw_info(struct qed_dev *cdev);

/**
 * @brief Mark structs of vfs that have been FLR-ed.
 *
 * @param p_hwfn
 * @param disabled_vfs - bitmask of all VFs on path that were FLRed
 *
 * @return true iff one of the PF's vfs got FLRed. false otherwise.
 */
bool qed_iov_mark_vf_flr(struct qed_hwfn *p_hwfn, u32 * disabled_vfs);

/**
 * @brief Search extended TLVs in request/reply buffer.
 *
 * @param p_hwfn
 * @param p_tlvs_list - Pointer to tlvs list
 * @param req_type - Type of TLV
 *
 * @return pointer to tlv type if found, otherwise returns NULL.
 */
void *qed_iov_search_list_tlvs(struct qed_hwfn *p_hwfn,
			       void *p_tlvs_list, u16 req_type);

/**
 * @brief qed_iov_get_vf_info - return the database of a
 *        specific VF
 *
 * @param p_hwfn
 * @param relative_vf_id - relative id of the VF for which info
 *			 is requested
 * @param b_enabled_only - false iff want to access even if vf is disabled
 *
 * @return struct qed_vf_info*
 */
struct qed_vf_info *qed_iov_get_vf_info(struct qed_hwfn *p_hwfn,
					u16 relative_vf_id,
					bool b_enabled_only);

/**
 * @brief get the VF doorbell bar size.
 *
 * @param cdev
 * @param p_ptt
 */
u32 qed_iov_vf_db_bar_size(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief get the relative id from absolute id.
 *
 * @param p_hwfn
 * @param abs_id
 */
u8 qed_iov_abs_to_rel_id(struct qed_hwfn *p_hwfn, u8 abs_id);

/**
 * @brief store the EQ and notify the VF
 *
 * @param cdev
 * @param p_eqe
 */
int
qed_iov_async_event_completion(struct qed_hwfn *p_hwfn,
			       struct event_ring_entry *p_eqe);

/**
 * @brief initialaize the VF doorbell bar
 *
 * @param p_hwfn
 * @param p_ptt
 */
int
qed_iov_init_vf_doorbell_bar(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

enum qed_iov_validate_q_mode {
	QED_IOV_VALIDATE_Q_NA,
	QED_IOV_VALIDATE_Q_ENABLE,
	QED_IOV_VALIDATE_Q_DISABLE,
};

bool qed_iov_validate_queue_mode(struct qed_vf_info *p_vf,
				 u16 qid,
				 enum qed_iov_validate_q_mode mode,
				 bool b_is_tx);

bool qed_iov_validate_rxq(struct qed_hwfn *p_hwfn,
			  struct qed_vf_info *p_vf,
			  u16 rx_qid, enum qed_iov_validate_q_mode mode);
/**
 * @brief qed_iov_handle_trust_change(struct qed_hwfn *hwfn) - handles trust mode
 *
 * @param p_hwfn
 */
void qed_iov_handle_trust_change(struct qed_hwfn *hwfn);

/**
 * @brief qed_disable_channel_for_all_vfs(struct qed_hwfn *p_hwfn,
 *                                          qed_hwfn *hwfn) - mark channel indication
 *                                          for all vf as disable
 *
 * @param p_hwfn
 */
void qed_disable_channel_for_all_vfs(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt);

#else
static inline int qed_iov_hw_info(struct qed_hwfn __maybe_unused * p_hwfn)
{
	return 0;
}

static inline void *qed_add_tlv(u8 __maybe_unused ** offset,
				__maybe_unused u16 type,
				__maybe_unused u16 length)
{
	return NULL;
}

static inline void qed_dp_tlv_list(struct qed_hwfn __maybe_unused * p_hwfn,
				   void __maybe_unused * tlvs_list)
{
}

static inline void qed_sriov_vfpf_malicious(struct qed_hwfn __maybe_unused *
					    p_hwfn,
					    struct fw_err_data __maybe_unused *
					    p_data)
{
}

static inline int qed_sriov_eqe_event(struct qed_hwfn __maybe_unused * p_hwfn,
				      u8 __maybe_unused
				      opcode,
				      u16 __maybe_unused
				      echo,
				      union event_ring_data __maybe_unused
				      * data,
				      u8 __maybe_unused
				      fw_return_code, u8 __maybe_unused vf_id)
{
	return 0;
}

static inline int qed_iov_alloc(struct qed_hwfn __maybe_unused * p_hwfn)
{
	return 0;
}

static inline void qed_iov_setup(struct qed_hwfn __maybe_unused * p_hwfn)
{
}

static inline void qed_iov_free(struct qed_hwfn __maybe_unused * p_hwfn)
{
}

static inline void qed_iov_free_hw_info(struct qed_dev __maybe_unused * cdev)
{
}

static inline u32 qed_crc32(u32 __maybe_unused crc,
			    u8 __maybe_unused * ptr, u32 __maybe_unused length)
{
	return 0;
}

static inline bool qed_iov_mark_vf_flr(struct qed_hwfn __maybe_unused * p_hwfn,
				       u32 __maybe_unused * disabled_vfs)
{
	return false;
}

static inline void *qed_iov_search_list_tlvs(struct qed_hwfn __maybe_unused *
					     p_hwfn,
					     void __maybe_unused * p_tlvs_list,
					     u16 __maybe_unused req_type)
{
	return NULL;
}

static inline struct qed_vf_info *qed_iov_get_vf_info(struct qed_hwfn
						      __maybe_unused * p_hwfn,
						      u16 __maybe_unused
						      relative_vf_id,
						      bool __maybe_unused
						      b_enabled_only)
{
	return NULL;
}

static inline u32 qed_iov_vf_db_bar_size(struct qed_hwfn __maybe_unused *
					 p_hwfn,
					 struct qed_ptt __maybe_unused * p_ptt)
{
	return 0;
}

static inline u8 qed_iov_abs_to_rel_id(struct qed_hwfn __maybe_unused * p_hwfn,
				       u8 __maybe_unused abs_id)
{
	return 0;
}

static inline int qed_iov_async_event_completion(struct qed_hwfn __maybe_unused
						 * p_hwfn,
						 struct event_ring_entry
						 __maybe_unused * p_eqe)
{
	return 0;
}

static inline int qed_iov_init_vf_doorbell_bar(struct qed_hwfn __maybe_unused *
					       p_hwfn,
					       struct qed_ptt __maybe_unused *
					       p_ptt)
{
	return 0;
}

static inline void qed_iov_handle_trust_change(struct qed_hwfn
					       __maybe_unused * hwfn)
{
}

static inline void qed_disable_channel_for_all_vfs(struct qed_hwfn
						   __maybe_unused * hwfn,
						   struct qed_ptt __maybe_unused
						   * p_ptt)
{
}
#endif

enum qed_iov_wq_flag {
	QED_IOV_WQ_MSG_FLAG,
	QED_IOV_WQ_SET_UNICAST_FILTER_FLAG,
	QED_IOV_WQ_BULLETIN_UPDATE_FLAG,
	QED_IOV_WQ_STOP_WQ_FLAG,
	QED_IOV_WQ_FLR_FLAG,
	QED_IOV_WQ_TRUST_FLAG,
	QED_IOV_WQ_VF_FORCE_LINK_QUERY_FLAG,
	QED_IOV_WQ_DB_REC_HANDLER,
};

#ifndef QED_UPSTREAM
int qed_iov_pre_start_vport(struct qed_hwfn *hwfn,
			    u8 rel_vfid,
			    struct qed_sp_vport_start_params *params);
#endif

/**
 * @brief - start the IOV workqueue[s]
 *
 * @param cdev
 */
void qed_vf_start_iov_wq(struct qed_dev *cdev);

void qed_iov_clean_vf(struct qed_hwfn *p_hwfn, u8 vfid);

int qed_iov_pre_update_vport(struct qed_hwfn *hwfn,
			     u8 vfid,
			     struct qed_sp_vport_update_params *params,
			     u16 * tlvs);
int qed_pf_validate_modify_tunn_config(struct qed_hwfn *p_hwfn,
				       u16 * feature_mask,
				       bool * update,
				       struct qed_tunnel_info *info);

#ifdef CONFIG_QED_SRIOV
void qed_iov_wq_stop(struct qed_dev *cdev, bool schedule_first);
int qed_iov_wq_start(struct qed_dev *cdev);

void qed_schedule_iov(struct qed_hwfn *hwfn, enum qed_iov_wq_flag flag);
int qed_pf_vf_msg(struct qed_hwfn *hwfn, u8 vfid);
int qed_iov_chk_ucast(struct qed_hwfn *hwfn,
		      int vfid, struct qed_filter_ucast *params);
int qed_sriov_disable(struct qed_dev *cdev, bool pci_enabled);
void qed_iov_pf_task(struct work_struct *work);
void qed_inform_vf_link_state(struct qed_hwfn *hwfn);
void qed_vf_fill_driver_data(struct qed_hwfn *hwfn,
			     struct qed_vf_acquire_sw_info *info);
int qed_sriov_pf_set_mac(struct qed_dev *cdev, u8 * mac, int vf_id);
#else
static inline void qed_iov_wq_stop(struct qed_dev *cdev, bool schedule_first)
{
}

static inline int qed_iov_wq_start(struct qed_dev *cdev)
{
	return 0;
}

static inline void qed_schedule_iov(struct qed_hwfn *hwfn,
				    enum qed_iov_wq_flag flag)
{
}

static inline int qed_pf_vf_msg(struct qed_hwfn *hwfn, u8 vfid)
{
	return 0;
}

static inline int qed_iov_chk_ucast(struct qed_hwfn *hwfn,
				    int vfid, struct qed_filter_ucast *params)
{
	return 0;
}

static inline int qed_sriov_disable(struct qed_dev *cdev, bool pci_enabled)
{
	return 0;
}

static inline void qed_iov_pf_task(struct work_struct *work)
{
}

static inline void qed_inform_vf_link_state(struct qed_hwfn *hwfn)
{
}
#endif
#endif
