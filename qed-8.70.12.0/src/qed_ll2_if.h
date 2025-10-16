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

#ifndef _QED_LL2_IF_H
#define _QED_LL2_IF_H
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/if_ether.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include "qed_if.h"

struct qed_roce_pvrdma_ll2_data {
	u32 opaque_data_0;
	u32 opaque_data_1;
};

enum qed_ll2_conn_type {
	QED_LL2_TYPE_FCOE /* FCoE L2 connection */ ,
	QED_LL2_TYPE_ISCSI /* Iscsi L2 connection */ ,
	QED_LL2_TYPE_TEST /* Eth TB test connection */ ,
	QED_LL2_TYPE_OOO /* Iscsi OOO L2 connection */ ,
	QED_LL2_TYPE_TOE /* toe L2 connection */ ,
	QED_LL2_TYPE_ROCE /* RoCE L2 connection */ ,
	QED_LL2_TYPE_IWARP,
	MAX_QED_LL2_CONN_TYPE
};

enum qed_ll2_rx_conn_type {
	QED_LL2_RX_TYPE_LEGACY /* legacy rx ll2 queue (ram based) */ ,
	QED_LL2_RX_TYPE_CTX /* ctx based rx ll2 queue */ ,
	MAX_QED_LL2_RX_CONN_TYPE
};

enum qed_ll2_roce_flavor_type {
	QED_LL2_ROCE,		/* use this as default or d/c */
	QED_LL2_RROCE,
	MAX_QED_LL2_ROCE_FLAVOR_TYPE
};

enum qed_ll2_tx_dest {
	QED_LL2_TX_DEST_NW /* Light L2 TX Destination to the Network */ ,
	QED_LL2_TX_DEST_LB /* Light L2 TX Destination to the Loopback */ ,
	QED_LL2_TX_DEST_DROP /* Light L2 Drop the TX packet */ ,
	QED_LL2_TX_DEST_MAX
};

enum qed_ll2_error_handle {
	QED_LL2_DROP_PACKET /* If error occurs drop packet */ ,
	QED_LL2_DO_NOTHING /* If error occurs do nothing */ ,
	QED_LL2_ASSERT /* If error occurs assert */ ,
};

struct qed_ll2_stats {
	u64 gsi_invalid_hdr;
	u64 gsi_invalid_pkt_length;
	u64 gsi_unsupported_pkt_typ;
	u64 gsi_crcchksm_error;

	u64 packet_too_big_discard;
	u64 no_buff_discard;

	u64 rcv_ucast_bytes;
	u64 rcv_mcast_bytes;
	u64 rcv_bcast_bytes;
	u64 rcv_ucast_pkts;
	u64 rcv_mcast_pkts;
	u64 rcv_bcast_pkts;

	u64 sent_ucast_bytes;
	u64 sent_mcast_bytes;
	u64 sent_bcast_bytes;
	u64 sent_ucast_pkts;
	u64 sent_mcast_pkts;
	u64 sent_bcast_pkts;
};

struct qed_ll2_comp_rx_data {
	u8 connection_handle;
	void *cookie;
	dma_addr_t rx_buf_addr;
	u16 parse_flags;
	u16 err_flags;
	u16 vlan;
	bool b_last_packet;

	union {
		u8 placement_offset;
		u8 data_length_error;
	} u;
	union {
		u16 packet_length;
		u16 data_length;
	} length;

	u32 opaque_data_0;	/* src_mac_addr_hi */
	u32 opaque_data_1;	/* src_mac_addr_lo */

	struct qed_roce_pvrdma_ll2_data roce_pvrdma_data;

	/* GSI only */
	u32 src_qp;
	u16 qp_id;
};

typedef
void (*qed_ll2_complete_rx_packet_cb) (void *cxt,
				       struct qed_ll2_comp_rx_data * data);

typedef
void (*qed_ll2_release_rx_packet_cb) (void *cxt,
				      u8 connection_handle,
				      void *cookie,
				      dma_addr_t rx_buf_addr,
				      bool b_last_packet);

typedef
void (*qed_ll2_complete_tx_packet_cb) (void *cxt,
				       u8 connection_handle,
				       void *cookie,
				       dma_addr_t first_frag_addr,
				       bool b_last_fragment,
				       bool b_last_packet);

typedef
void (*qed_ll2_release_tx_packet_cb) (void *cxt,
				      u8 connection_handle,
				      void *cookie,
				      dma_addr_t first_frag_addr,
				      bool b_last_fragment, bool b_last_packet);

typedef
void (*qed_ll2_slowpath_cb) (void *cxt,
			     u8 connection_handle,
			     u32 opaque_data_0, u32 opaque_data_1);

struct qed_ll2_cbs {
	qed_ll2_complete_rx_packet_cb rx_comp_cb;
	qed_ll2_release_rx_packet_cb rx_release_cb;
	qed_ll2_complete_tx_packet_cb tx_comp_cb;
	qed_ll2_release_tx_packet_cb tx_release_cb;
	qed_ll2_slowpath_cb slowpath_cb;
	void *cookie;
};

struct qed_ll2_acquire_data_inputs {
	enum qed_ll2_rx_conn_type rx_conn_type;
	enum qed_ll2_conn_type conn_type;
	u16 mtu;		/* Maximum bytes that can be placed on a BD */
	u16 rx_num_desc;

	/* Relevant only for OOO connection if 0 OOO rx buffers=2*rx_num_desc */
	u16 rx_num_ooo_buffers;
	u8 rx_drop_ttl0_flg;

	/* if set, 802.1q tags will be removed and copied to CQE */
	u8 rx_vlan_removal_en;
	u16 tx_num_desc;
	u8 tx_max_bds_per_packet;
	u8 tx_tc;
	u16 tx_vport_offset;
	enum qed_ll2_tx_dest tx_dest;
	enum qed_ll2_error_handle ai_err_packet_too_big;
	enum qed_ll2_error_handle ai_err_no_buf;
	u8 secondary_queue;
	u8 gsi_enable;
};

struct qed_ll2_acquire_data {
	struct qed_ll2_acquire_data_inputs input;
	const struct qed_ll2_cbs *cbs;

	/* Output container for LL2 connection's handle */
	u8 *p_connection_handle;
};
struct qed_ll2_tx_pkt_info {
	u8 num_of_bds;
	u16 vlan;
	u8 bd_flags;
	u16 l4_hdr_offset_w;	/* from start of packet */
	enum qed_ll2_tx_dest tx_dest;
	enum qed_ll2_roce_flavor_type qed_roce_flavor;
	dma_addr_t first_frag;
	u16 first_frag_len;
	bool enable_ip_cksum;
	bool enable_l4_cksum;
	bool calc_ip_len;
	void *cookie;
	bool remove_stag;
};

#if !defined(QED_UPSTREAM)
/* It's nasty that we have to have these defines inside the interface header,
 * as they match the qed_compat.h headers - but protocol clients won't include
 * that file, thus the duplication
 */
#ifndef RHEL_RELEASE_VERSION
#define RHEL_RELEASE_VERSION(a, b) 0
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)) && !defined(MODERN_VLAN)
#define MODERN_VLAN
#endif

#endif

#define QED_LL2_UNUSED_HANDLE   (0xff)

struct qed_ll2_cb_ops {
#ifdef MODERN_VLAN
	int (*rx_cb) (void *, struct sk_buff *, u32, u32);
#else
	int (*rx_cb) (void *, struct sk_buff *, u16 vlan, u32, u32);
#endif
	int (*tx_cb) (void *, struct sk_buff *, bool);
};

struct qed_ll2_params {
	u16 mtu;
	bool drop_ttl0_packets;
	bool rx_vlan_stripping;
	u8 tx_tc;
	bool frags_mapped;
	u8 ll2_mac_address[ETH_ALEN];
};

enum qed_ll2_xmit_flags {
	/* FIP discovery packet */
	QED_LL2_XMIT_FLAGS_FIP_DISCOVERY
};

struct qed_ll2_ops {
/**
 * @brief start - initializes ll2
 *
 * @param cdev
 * @param params - protocol driver configuration for the ll2.
 *
 * @return 0 on success, otherwise error value.
 */
	int (*start) (struct qed_dev * cdev, struct qed_ll2_params * params);

/**
 * @brief stop - stops the ll2
 *
 * @param cdev
 *
 * @return 0 on success, otherwise error value.
 */
	int (*stop) (struct qed_dev * cdev);

/**
 * @brief start_xmit - transmits an skb over the ll2 interface
 *
 * @param cdev
 * @param skb
 * @param xmit_flags - Transmit options defined by the enum qed_ll2_xmit_flags.
 *
 * @return 0 on success, otherwise error value.
 */
	int (*start_xmit) (struct qed_dev * cdev,
			   struct sk_buff * skb, unsigned long xmit_flags);

/**
 * @brief register_cb_ops - protocol driver register the callback for Rx/Tx
 * packets. Should be called before `start'.
 *
 * @param cdev
 * @param cookie - to be passed to the callback functions.
 * @param ops - the callback functions to register for Rx / Tx.
 *
 * @return 0 on success, otherwise error value.
 */
	void (*register_cb_ops) (struct qed_dev * cdev,
				 const struct qed_ll2_cb_ops * ops,
				 void *cookie);

/**
 * @brief get LL2 related statistics
 *
 * @param cdev
 * @param stats - pointer to struct that would be filled with stats
 *
 * @return 0 on success, error otherwise.
 */
	int (*get_stats) (struct qed_dev * cdev, struct qed_ll2_stats * stats);
};

#ifdef CONFIG_QED_LL2
int qed_ll2_alloc_if(struct qed_dev *);
void qed_ll2_dealloc_if(struct qed_dev *);
#else
static const struct qed_ll2_ops qed_ll2_ops_pass = {
	INIT_STRUCT_FIELD(start, NULL),
	INIT_STRUCT_FIELD(stop, NULL),
	INIT_STRUCT_FIELD(start_xmit, NULL),
	INIT_STRUCT_FIELD(register_cb_ops, NULL),
	INIT_STRUCT_FIELD(get_stats, NULL),
};

static inline int qed_ll2_alloc_if(struct qed_dev *cdev)
{
	return 0;
}

static inline void qed_ll2_dealloc_if(struct qed_dev *cdev)
{
}
#endif
#endif
