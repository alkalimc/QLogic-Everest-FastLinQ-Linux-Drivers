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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <asm/byteorder.h>
#include <asm/param.h>
#include <linux/io.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)) /* QEDE_UPSTREAM */
#include <linux/netdev_features.h>
#endif
#include <linux/udp.h>
#include <linux/tcp.h>
#ifdef _HAS_ADD_VXLAN_PORT /* QEDE_UPSTREAM */
#include <net/vxlan.h>
#endif
#ifdef _HAS_ADD_GENEVE_PORT
#include <net/geneve.h>
#endif
#if defined(_HAS_NDO_UDP_TUNNEL_CONFIG) || \
    defined(_HAS_NDO_EXT_UDP_TUNNEL_CONFIG) /* QEDE_UPSTREAM */
#include <net/udp_tunnel.h>
#endif
#include <linux/ip.h>
#include <net/ipv6.h>
#include <net/tcp.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/pkt_sched.h>
#include <linux/ethtool.h>
#include <linux/in.h>
#include <linux/random.h>
#include <net/ip6_checksum.h>
#include <linux/bitops.h>
#include <linux/mm.h>
#include <linux/if_macvlan.h>

#include "qed_if.h"
#include "qede_compat.h"
#include "qede_hsi.h"
#include "qede.h"
#include "qede_ptp.h"

#ifndef QEDE_UPSTREAM /* ! QEDE_UPSTREAM */
extern uint gro_disable;
#endif

#if defined(ENC_SUPPORTED) || defined(_HAS_GRE_H_SECTION)
#include <net/gre.h>
#endif

#define VFQ_RSS_NUM 0xffff
#define QEDE_FILTER_PRINT_MAX_LEN	(64)
struct qede_arfs_tuple {
	union {
		__be32 src_ipv4;
		struct in6_addr src_ipv6;
	};
	union {
		__be32 dst_ipv4;
		struct in6_addr dst_ipv6;
	};
	__be16	src_port;
	__be16	dst_port;
	__be16	eth_proto;
	u8	ip_proto;

	unsigned char dst_mac[ETH_ALEN];
	u16 vlan_id;

	/* VFQ Selected instead of RSS */
	bool		b_vfq_enabled;

	/* Describe filtering mode needed for this kind of filter */
	enum qed_filter_config_mode mode;

#ifndef QEDE_UPSTREAM /* ! QEDE_UPSTREAM */
	u8	tunn_hdr_size;
#endif
	/* Used to compare new/old filters. Return true iff IPs match */
	bool (*ip_comp)(struct qede_arfs_tuple *a, struct qede_arfs_tuple *b);

	/* Given an address into a ethhdr build a header from tuple info */
	void (*build_hdr)(struct qede_dev *edev, struct qede_arfs_tuple *t, void *header);

	/* Stringify the tuple for a print into the provided buffer */
	void (*stringify)(struct qede_arfs_tuple *t, void *buffer);
};

struct qede_arfs_fltr_node {
	struct hlist_node	node;

	/* Parsed meta-data about the requested flow */
	struct qede_arfs_tuple	tuple;

	/* Configuring the filter in hardware would require to have a mapped
	 * header matching the requested flow
	 */
	void			*data;
	dma_addr_t		mapping;
	int			buf_len;

#define QEDE_FLTR_VALID		0
	unsigned long		state;

	/* aRFS-only, the SW flow-id that's associated with the flow */
	u32			flow_id;

	/* Index inside the [internal] filter bitmap */
	u64			sw_id;

	/* Queue which is target of the classification.
	 * For aRFS-only, also maintain the next-queue [for flow shifts].
	 */
	u16			rxq_id;
	u16			next_rxq_id;

	/* 1-based vfid target of classification; 0 if target is the PF */
	u8 vfid;
	u8 vport_id;

	/* Indiciation whether filter is being added/deleted, to take
	 * action when firmware completes it [in regard to DB].
	 * TODO - can we use 'state for this'?
	 */
	bool			filter_op;

	/* Indication filter is currently being posted by firmware; Should
	 * prevent re-sending flow until response comes back.
	 * TODO - given we have 'state' as bitmask, can we push it there?
	 */
	bool			used;

	/* Used to indicate to polling context in case firmware has failed
	 * the configuration.
	 * TODO - can we use 'state' for this?
	 */
	u8			fw_rc;

	/* Packets needs to be dropped */
	bool			b_is_drop;
};

struct qede_arfs {
#define QEDE_ARFS_POLL_COUNT	200000
#define QEDE_RFS_FLW_BITSHIFT	(4)
#define QEDE_RFS_FLW_MASK	((1 << QEDE_RFS_FLW_BITSHIFT) - 1)
	struct hlist_head	arfs_hl_head[1 << QEDE_RFS_FLW_BITSHIFT];

	/* lock for filter list access */
	spinlock_t		arfs_list_lock;
	unsigned long		*arfs_fltr_bmap;
	int			filter_count;

	/* flow which is added via tc/ethtool */
	bool b_user_flow;

	/* Currently configured filtering mode */
	enum qed_filter_config_mode mode;
};

void qede_udp_ports_update(void *dev, u16 vxlan_port, u16 geneve_port)
{
	struct qede_dev *edev = dev;

	if (edev->vxlan_dst_port != vxlan_port)
		edev->vxlan_dst_port = 0;

	if (edev->geneve_dst_port != geneve_port)
		edev->geneve_dst_port = 0;
}

void qede_force_mac(void *dev, u8 *mac, bool forced)
{
	struct qede_dev *edev = dev;

	__qede_lock(edev);

	if (!is_valid_ether_addr(mac)) {
		__qede_unlock(edev);
		return;
	}

	eth_hw_addr_set(edev->ndev, mac);

	__qede_unlock(edev);
}

void qede_fill_rss_params(struct qede_dev *edev,
			  struct qed_update_vport_rss_params *rss,
			  u8 *update)
{
	bool need_reset = false;
	int i;

	if (QEDE_BASE_RSS_COUNT(edev) <= 1) {
		memset(rss, 0, sizeof(*rss));
		*update = 0;
		return;
	}

	/* Need to validate current RSS config uses valid entries */
	for (i = 0; i < QED_RSS_IND_TABLE_SIZE; i++) {
		if (edev->rss_ind_table[i] >= QEDE_BASE_RSS_COUNT(edev)) {
			need_reset = true;
			break;
		}
	}

	if (!(edev->rss_params_inited & QEDE_RSS_INDIR_INITED) || need_reset) {
		for (i = 0; i < QED_RSS_IND_TABLE_SIZE; i++) {
			u16 indir_val, val;

			val = QEDE_BASE_RSS_COUNT(edev);
			indir_val = ethtool_rxfh_indir_default(i, val);
			edev->rss_ind_table[i] = indir_val;
		}
		edev->rss_params_inited |= QEDE_RSS_INDIR_INITED;
	}

	/* Now that we have the queue-indirection, prepare the handles */
	for (i = 0; i < QED_RSS_IND_TABLE_SIZE; i++) {
		u16 idx = QEDE_RX_QUEUE_IDX(edev, edev->rss_ind_table[i]);

		rss->rss_ind_table[i] = edev->fp_array[idx].rxq->handle;
	}

	if (!(edev->rss_params_inited & QEDE_RSS_KEY_INITED)) {
		netdev_rss_key_fill(edev->rss_key, sizeof(edev->rss_key));
		edev->rss_params_inited |= QEDE_RSS_KEY_INITED;
	}
	memcpy(rss->rss_key, edev->rss_key, sizeof(rss->rss_key));

	if (!(edev->rss_params_inited & QEDE_RSS_CAPS_INITED)) {
		edev->rss_caps = QED_RSS_IPV4 | QED_RSS_IPV6 |
				 QED_RSS_IPV4_TCP | QED_RSS_IPV6_TCP;
		edev->rss_params_inited |= QEDE_RSS_CAPS_INITED;
	}
	rss->rss_caps = edev->rss_caps;

	*update = 1;
}

#ifdef BCM_VLAN /* ! QEDE_UPSTREAM */
static int qede_vlan_stripping_cmd(struct qede_dev *edev, bool set_stripping)
{
	struct qed_update_vport_params *vport_update_params;
	int rc;

	vport_update_params = vzalloc(sizeof(*vport_update_params));
	if (!vport_update_params)
		return -ENOMEM;

	/* Send vport_update ramrod with enable / disable vlan stripping */
	vport_update_params->update_inner_vlan_removal_flg = 1;
	vport_update_params->inner_vlan_removal_flg = set_stripping;
	rc = edev->ops->vport_update(edev->cdev, vport_update_params);
	if (rc) {
		DP_ERR(edev, "Update V-PORT failed %d\n", rc);
		return rc;
	}

	vfree(vport_update_params);
	return 0;
}

/* called with rtnl_lock */
void qede_vlan_rx_register(struct net_device *ndev, struct vlan_group *vlgrp)
{
	struct qede_dev	*edev = netdev_priv(ndev);

	/* Configure VLAN stripping if NIC is up.
	 * Otherwise just set the edev->vlan_group and stripping will be
	 * configured in qede_nic_load().
	 */
	__qede_lock(edev);
	if (edev->state == QEDE_STATE_OPEN)
		qede_vlan_stripping_cmd(edev, (vlgrp != NULL ? true : false));
	__qede_unlock(edev);

	edev->vlan_group = vlgrp;
}
#endif /* BCM_VLAN */

static int qede_set_ucast_rx_mac(struct qede_dev *edev,
				 enum qed_filter_xcast_params_type opcode,
				 const unsigned char mac[ETH_ALEN], u8 vport_id)
{
	struct qed_filter_params filter_cmd;

	memset(&filter_cmd, 0, sizeof(filter_cmd));
	filter_cmd.type = QED_FILTER_TYPE_UCAST;
	filter_cmd.filter.ucast.type = opcode;
	filter_cmd.filter.ucast.mac_valid = 1;
	filter_cmd.vport_id = vport_id;
	ether_addr_copy(filter_cmd.filter.ucast.mac, mac);

	return edev->ops->filter_config(edev->cdev, &filter_cmd);
}

#ifdef _HAS_VLAN_RX_ADD_VID /* QEDE_UPSTREAM */
static int qede_set_ucast_rx_vlan(struct qede_dev *edev,
				  enum qed_filter_xcast_params_type opcode,
				  u16 vid)
{
	struct qed_filter_params filter_cmd;

	memset(&filter_cmd, 0, sizeof(filter_cmd));
	filter_cmd.type = QED_FILTER_TYPE_UCAST;
	filter_cmd.filter.ucast.type = opcode;
	filter_cmd.filter.ucast.vlan_valid = 1;
	filter_cmd.filter.ucast.vlan = vid;

	return edev->ops->filter_config(edev->cdev, &filter_cmd);
}
#endif

static int qede_config_accept_any_vlan(struct qede_dev *edev, bool action,
				       u8 vport_id)
{
	struct qed_update_vport_params *params;
	int rc;

	/* Proceed only if action actually needs to be performed */
	if (edev->accept_any_vlan == action)
		return 0;

	params = vzalloc(sizeof(*params));
	if (!params)
		return -ENOMEM;

	params->vport_id = vport_id;
	params->accept_any_vlan = action;
	params->update_accept_any_vlan_flg = 1;

	rc = edev->ops->vport_update(edev->cdev, params);
	if (rc) {
		DP_ERR(edev, "Failed to %s accept-any-vlan\n",
		       action ? "enable" : "disable");
	} else {
		DP_INFO(edev, "%s accept-any-vlan\n",
			action ? "enabled" : "disabled");
		edev->accept_any_vlan = action;
	}

	vfree(params);
	return 0;
}

#ifdef _HAS_VLAN_RX_ADD_VID /* QEDE_UPSTREAM */

int qede_vlan_rx_add_vid(struct net_device *dev, __be16 proto, u16 vid)
{
	struct qede_dev *edev = netdev_priv(dev);
	struct qede_vlan *vlan, *tmp;
	int rc = 0;

	DP_VERBOSE(edev, NETIF_MSG_IFUP, "Adding vlan 0x%04x\n", vid);

	vlan = kzalloc(sizeof(*vlan), GFP_KERNEL);
	if (!vlan) {
		DP_INFO(edev, "Failed to allocate struct for vlan\n");
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&vlan->list);
	vlan->vid = vid;
	vlan->configured = false;

	/* Verify vlan isn't already configured */
	list_for_each_entry(tmp, &edev->vlan_list, list) {
		if (tmp->vid == vlan->vid) {
			DP_VERBOSE(edev, (NETIF_MSG_IFUP | NETIF_MSG_IFDOWN),
				   "vlan already configured\n");
			kfree(vlan);
			return -EEXIST;
		}
	}

	/* If interface is down, cache this VLAN ID and return */
	__qede_lock(edev);
	if (edev->state != QEDE_STATE_OPEN) {
		DP_VERBOSE(edev, NETIF_MSG_IFDOWN,
			   "Interface is down, VLAN %d will be configured when interface is up\n",
			   vid);
		if (vid != 0)
			edev->non_configured_vlans++;
		list_add(&vlan->list, &edev->vlan_list);
		goto out;
	}

	/* Check for the filter limit.
	 * Note - vlan0 has a reserved filter and can be added without
	 * worrying about quota
	 */
	if ((edev->configured_vlans < edev->dev_info.num_vlan_filters) ||
	    (vlan->vid == 0)) {
		rc = qede_set_ucast_rx_vlan(edev,
					    QED_FILTER_XCAST_TYPE_ADD,
					    vlan->vid);
		if (rc) {
			DP_ERR(edev, "Failed to configure VLAN %d\n",
			       vlan->vid);
			kfree(vlan);
			goto out;
		}
		vlan->configured = true;

		/* vlan0 filter isn't consuming out of our quota */
		if (vlan->vid != 0)
			edev->configured_vlans++;
	} else {
		u8 vport_id = QEDE_BASE_DEV_VPORT_ID;

		/* Out of quota; Activate accept-any-VLAN mode */
		if (!edev->non_configured_vlans) {
			rc = qede_config_accept_any_vlan(edev, true, vport_id);
			if (rc) {
				kfree(vlan);
				goto out;
			}
		}

		edev->non_configured_vlans++;
	}

	list_add(&vlan->list, &edev->vlan_list);

out:
	__qede_unlock(edev);
	return rc;
}

static void qede_del_vlan_from_list(struct qede_dev *edev,
				    struct qede_vlan *vlan)
{
	/* vlan0 filter isn't consuming out of our quota */
	if (vlan->vid != 0) {
		if (vlan->configured)
			edev->configured_vlans--;
		else
			edev->non_configured_vlans--;
	}

	list_del(&vlan->list);
	kfree(vlan);
}

int qede_configure_vlan_filters(struct qede_dev *edev)
{
	int rc = 0, real_rc = 0, accept_any_vlan = 0;
	struct qed_dev_eth_info *dev_info;
	struct qede_vlan *vlan = NULL;

	if (list_empty(&edev->vlan_list))
		return 0;

	dev_info = &edev->dev_info;

	/* Configure non-configured vlans */
	list_for_each_entry(vlan, &edev->vlan_list, list) {
		if (vlan->configured)
			continue;

		/*  We have used all our credits, now enable accept_any_vlan */
		if ((vlan->vid != 0) &&
		    (edev->configured_vlans == dev_info->num_vlan_filters)) {
			accept_any_vlan = 1;
			continue;
		}

		DP_VERBOSE(edev, NETIF_MSG_IFUP, "Adding vlan %d\n", vlan->vid);

		rc = qede_set_ucast_rx_vlan(edev, QED_FILTER_XCAST_TYPE_ADD,
					    vlan->vid);
		if (rc) {
			DP_ERR(edev, "Failed to configure VLAN %u\n",
			       vlan->vid);
			real_rc = rc;
			continue;
		}

		vlan->configured = true;
		/* vlan0 filter doesn't consume our VLAN filter's quota */
		if (vlan->vid != 0) {
			edev->non_configured_vlans--;
			edev->configured_vlans++;
		}
	}

	/* enable accept_any_vlan mode if we have more VLANs than credits,
	 * or remove accept_any_vlan mode if we've actually removed
	 * a non-configured vlan, and all remaining vlans are truly configured.
	 */

	if (accept_any_vlan)
		rc = qede_config_accept_any_vlan(edev, true,
						 QEDE_BASE_DEV_VPORT_ID);
	else if (!edev->non_configured_vlans)
		rc = qede_config_accept_any_vlan(edev, false,
						 QEDE_BASE_DEV_VPORT_ID);

	if (rc && !real_rc)
		real_rc = rc;

	return real_rc;
}

int qede_vlan_rx_kill_vid(struct net_device *dev, __be16 proto, u16 vid)
{
	struct qede_dev *edev = netdev_priv(dev);
	struct qede_vlan *vlan = NULL;
	int rc = 0;

	DP_VERBOSE(edev, NETIF_MSG_IFDOWN, "Removing vlan 0x%04x\n", vid);

	/* Find whether entry exists */
	__qede_lock(edev);
	list_for_each_entry(vlan, &edev->vlan_list, list)
		if (vlan->vid == vid)
			break;

	if (!vlan || (vlan->vid != vid)) {
		DP_VERBOSE(edev, (NETIF_MSG_IFUP | NETIF_MSG_IFDOWN),
			   "Vlan isn't configured\n");
		goto out;
	}

	if (edev->state != QEDE_STATE_OPEN) {
		/* As interface is already down, we don't have a VPORT
		 * instance to remove vlan filter.So just update vlan list
		 */
		DP_VERBOSE(edev, NETIF_MSG_IFDOWN,
			   "Interface is down, removing VLAN from list only\n");
		qede_del_vlan_from_list(edev, vlan);
		goto out;
	}

	/* Remove vlan */
	if (vlan->configured) {
		rc = qede_set_ucast_rx_vlan(edev, QED_FILTER_XCAST_TYPE_DEL,
					    vid);
		if (rc) {
			DP_ERR(edev, "Failed to remove VLAN %d\n", vid);
			goto out;
		}
	}

	qede_del_vlan_from_list(edev, vlan);

	/* We have removed a VLAN, try to see if we can
	 * configure non-configured VLAN from the list
	 */
	rc = qede_configure_vlan_filters(edev);

out:
	__qede_unlock(edev);
	return rc;
}

void qede_vlan_mark_nonconfigured(struct qede_dev *edev)
{
	struct qede_vlan *vlan = NULL;

	if (list_empty(&edev->vlan_list))
		return;

	list_for_each_entry(vlan, &edev->vlan_list, list) {
		if (!vlan->configured)
			continue;

		vlan->configured = false;

		/* vlan0 filter isn't consuming out of our quota */
		if (vlan->vid != 0) {
			edev->non_configured_vlans++;
			edev->configured_vlans--;
		}

		DP_VERBOSE(edev, NETIF_MSG_IFDOWN,
			   "marked vlan %d as non-configured\n",
			   vlan->vid);
	}

	edev->accept_any_vlan = false;
}
#else
void qede_vlan_mark_nonconfigured(struct qede_dev *edev) {}
int qede_configure_vlan_filters(struct qede_dev *edev) { return 0; }
#endif

#ifdef _HAS_VLAN_RX_ADD_VID /* ! QEDE_UPSTREAM */
#ifdef _VLAN_RX_ADD_VID_RETURNS_VOID
void qede_vlan_rx_add_vid_void(struct net_device *dev, u16 vid)
{
	qede_vlan_rx_add_vid(dev, htons(ETH_P_8021Q), vid);
}

void qede_vlan_rx_kill_vid_void(struct net_device *dev, u16 vid)
{
	qede_vlan_rx_kill_vid(dev, htons(ETH_P_8021Q), vid);
}
#elif defined(_VLAN_RX_ADD_VID_NO_PROTO)
int qede_vlan_rx_add_vid_no_proto(struct net_device *dev, u16 vid)
{
	return qede_vlan_rx_add_vid(dev, htons(ETH_P_8021Q), vid);
}

int qede_vlan_rx_kill_vid_no_proto(struct net_device *dev, u16 vid)
{
	return qede_vlan_rx_kill_vid(dev, htons(ETH_P_8021Q), vid);
}

#endif
#endif

#ifdef HAS_NDO_FIX_FEATURES /* QEDE_UPSTREAM */
static void qede_set_features_reload(struct qede_dev *edev,
				     struct qede_reload_args *args)
{
	edev->ndev->features = args->u.features;
}

netdev_features_t qede_fix_features(struct net_device *dev,
				    netdev_features_t features)
{
#ifdef _HAS_NET_GRO_HW /* QEDE_UPSTREAM */
	struct qede_dev *edev = netdev_priv(dev);

	if (edev->xdp_prog || edev->ndev->mtu > PAGE_SIZE ||
	    !(features & NETIF_F_GRO))
		features &= ~NETIF_F_GRO_HW;
#endif

#ifdef ENC_SUPPORTED
#ifdef _HAS_GSO_TUN_L4_CSUM /* QEDE_UPSTREAM */
	if (!(features & NETIF_F_GSO_UDP_TUNNEL))
		features &= ~NETIF_F_GSO_UDP_TUNNEL_CSUM;
#endif
#endif
	return features;
}

int qede_set_features(struct net_device *dev, netdev_features_t features)
{
	struct qede_dev *edev = netdev_priv(dev);
	netdev_features_t changes = features ^ dev->features;
	bool need_reload = false;

#ifdef _HAS_NET_GRO_HW /* QEDE_UPSTREAM */
	if (changes & NETIF_F_GRO_HW)
		need_reload = true;
#else
	/* No action needed if hardware GRO is disabled during driver load */
	if (changes & NETIF_F_GRO) {
		if (dev->features & NETIF_F_GRO)
			need_reload = !edev->gro_disable;
		else
			need_reload = edev->gro_disable;
	}
#endif

#ifndef QEDE_UPSTREAM
	/* If FW-GRO is disabled, no need to ever reload */
	if (gro_disable)
		need_reload = false;
#endif

	if (need_reload) {
		struct qede_reload_args args;

		memset(&args, 0, sizeof(args));
		args.u.features = features;
		args.func = &qede_set_features_reload;
		args.flags = QEDE_UPDATE_COMPLETE;

		/* Make sure that we definitely need to reload.
		 * In case of an eBPF attached program, there will be no FW
		 * aggregations, so no need to actually reload.
		 */
		__qede_lock(edev);
		args.is_locked = true;
		if (edev->xdp_prog)
			args.func(edev, &args);
		else
			qede_reload(edev, &args);
		__qede_unlock(edev);

		return 1;
	}

	return 0;
}
#endif

#if HAS_NDO(UDP_TUNNEL_CONFIG) /* QEDE_UPSTREAM */
void qede_udp_tunnel_add(struct net_device *dev,
			 struct udp_tunnel_info *ti)
{
	struct qede_dev *edev = netdev_priv(dev);
	struct qed_tunn_params tunn_params;
	u16 t_port = ntohs(ti->port);
	int rc;

	memset(&tunn_params, 0, sizeof(tunn_params));

	switch (ti->type) {
	case UDP_TUNNEL_TYPE_VXLAN:
		if (!edev->dev_info.common.vxlan_enable)
			return;

		if (edev->vxlan_dst_port)
			return;

		tunn_params.update_vxlan_port = 1;
		tunn_params.vxlan_port = t_port;

		__qede_lock(edev);
		rc = edev->ops->tunn_config(edev->cdev, &tunn_params);
		__qede_unlock(edev);

		if (!rc) {
			edev->vxlan_dst_port = t_port;
			DP_VERBOSE(edev, QED_MSG_DEBUG, "Added vxlan port=%d\n",
				   t_port);
		} else {
			DP_NOTICE(edev, "Failed to add vxlan UDP port=%d\n",
				  t_port);
		}

		break;
	case UDP_TUNNEL_TYPE_GENEVE:
		if (!edev->dev_info.common.geneve_enable)
			return;

		if (edev->geneve_dst_port)
			return;

		tunn_params.update_geneve_port = 1;
		tunn_params.geneve_port = t_port;

		__qede_lock(edev);
		rc = edev->ops->tunn_config(edev->cdev, &tunn_params);
		__qede_unlock(edev);

		if (!rc) {
			edev->geneve_dst_port = t_port;
			DP_VERBOSE(edev, QED_MSG_DEBUG,
				   "Added geneve port=%d\n", t_port);
		} else {
			DP_NOTICE(edev, "Failed to add geneve UDP port=%d\n",
				  t_port);
		}

		break;
	default:
		return;
	}
}

void qede_udp_tunnel_del(struct net_device *dev,
			 struct udp_tunnel_info *ti)
{
	struct qede_dev *edev = netdev_priv(dev);
	struct qed_tunn_params tunn_params;
	u16 t_port = ntohs(ti->port);

	memset(&tunn_params, 0, sizeof(tunn_params));

	switch (ti->type) {
	case UDP_TUNNEL_TYPE_VXLAN:
		if (t_port != edev->vxlan_dst_port)
			return;

		tunn_params.update_vxlan_port = 1;
		tunn_params.vxlan_port = 0;

		__qede_lock(edev);
		edev->ops->tunn_config(edev->cdev, &tunn_params);
		__qede_unlock(edev);

		edev->vxlan_dst_port = 0;

		DP_VERBOSE(edev, QED_MSG_DEBUG, "Deleted vxlan port=%d\n",
			   t_port);

		break;
	case UDP_TUNNEL_TYPE_GENEVE:
		if (t_port != edev->geneve_dst_port)
			return;

		tunn_params.update_geneve_port = 1;
		tunn_params.geneve_port = 0;

		__qede_lock(edev);
		edev->ops->tunn_config(edev->cdev, &tunn_params);
		__qede_unlock(edev);

		edev->geneve_dst_port = 0;

		DP_VERBOSE(edev, QED_MSG_DEBUG, "Deleted geneve port=%d\n",
			   t_port);
		break;
	default:
		return;
	}
}
#endif

#ifdef _HAS_ADD_VXLAN_PORT /* ! QEDE_UPSTREAM */
void qede_add_vxlan_port(struct net_device *dev,
			 sa_family_t sa_family, __be16 port)
{
	struct qede_dev *edev = netdev_priv(dev);
	struct qed_tunn_params tunn_params;
	u16 t_port = ntohs(port);
	int rc;

	memset(&tunn_params, 0, sizeof(tunn_params));

	if (!edev->dev_info.common.vxlan_enable)
		return;

	if (edev->vxlan_dst_port)
		return;

	tunn_params.update_vxlan_port = 1;
	tunn_params.vxlan_port = t_port;

	__qede_lock(edev);
	rc = edev->ops->tunn_config(edev->cdev, &tunn_params);
	__qede_unlock(edev);

	if (!rc) {
		edev->vxlan_dst_port = t_port;
		DP_VERBOSE(edev, QED_MSG_DEBUG, "Added vxlan port=%d\n",
			   t_port);
	} else {
		DP_NOTICE(edev, "Failed to add vxlan UDP port = %d\n",
			  t_port);
	}
}

void qede_del_vxlan_port(struct net_device *dev,
			 sa_family_t sa_family, __be16 port)
{
	struct qede_dev *edev = netdev_priv(dev);
	struct qed_tunn_params tunn_params;
	u16 t_port = ntohs(port);

	memset(&tunn_params, 0, sizeof(tunn_params));

	if (t_port != edev->vxlan_dst_port)
		return;

	tunn_params.update_vxlan_port = 1;
	tunn_params.vxlan_port = 0;

	__qede_lock(edev);
	edev->ops->tunn_config(edev->cdev, &tunn_params);
	__qede_unlock(edev);

	edev->vxlan_dst_port = 0;

	DP_VERBOSE(edev, QED_MSG_DEBUG, "Deleted vxlan port=%d\n",
		   t_port);
}
#endif

#ifdef _HAS_ADD_GENEVE_PORT /* ! QEDE_UPSTREAM */
void qede_add_geneve_port(struct net_device *dev,
			  sa_family_t sa_family, __be16 port)
{
	struct qede_dev *edev = netdev_priv(dev);
	struct qed_tunn_params tunn_params;
	u16 t_port = ntohs(port);
	int rc;

	memset(&tunn_params, 0, sizeof(tunn_params));

	if (!edev->dev_info.common.geneve_enable)
		return;

	if (edev->geneve_dst_port)
		return;

	tunn_params.update_geneve_port = 1;
	tunn_params.geneve_port = t_port;

	__qede_lock(edev);
	rc = edev->ops->tunn_config(edev->cdev, &tunn_params);
	__qede_unlock(edev);

	if (!rc) {
		edev->geneve_dst_port = t_port;
		DP_VERBOSE(edev, QED_MSG_DEBUG, "Added geneve port=%d\n",
			   t_port);
	} else {
		DP_NOTICE(edev, "Failed to add geneve UDP port = %d\n",
			  t_port);
	}
}

void qede_del_geneve_port(struct net_device *dev,
			  sa_family_t sa_family, __be16 port)
{
	struct qede_dev *edev = netdev_priv(dev);
	struct qed_tunn_params tunn_params;
	u16 t_port = ntohs(port);

	memset(&tunn_params, 0, sizeof(tunn_params));

	if (t_port != edev->geneve_dst_port)
		return;

	tunn_params.update_geneve_port = 1;
	tunn_params.geneve_port = 0;

	__qede_lock(edev);
	edev->ops->tunn_config(edev->cdev, &tunn_params);
	__qede_unlock(edev);

	edev->geneve_dst_port = 0;

	DP_VERBOSE(edev, QED_MSG_DEBUG, "Deleted geneve port=%d\n",
		   t_port);
}
#endif

#define QEDE_ARFS_BUCKET_HEAD(edev, idx) (&(edev)->arfs->arfs_hl_head[idx])

static void qede_configure_arfs_fltr(struct qede_dev *edev,
				     struct qede_arfs_fltr_node *n,
				     u16 rxq_id, bool add_fltr)
{
	const struct qed_eth_ops *op = edev->ops;
	struct qed_ntuple_filter_params params;

	if (IS_VF(edev) && !add_fltr)
		n->used = false;

	if (n->used)
		return;

	memset(&params, 0, sizeof(params));
	params.addr = n->mapping;
	params.length = n->buf_len;
	params.qid = rxq_id;
	params.b_is_add = add_fltr;
	params.vport_id = n->vport_id;
	params.b_is_drop = n->b_is_drop;
	params.b_vfq_enabled = n->tuple.b_vfq_enabled;
	params.pkt_virt_addr = n->data;

	if (n->vfid) {
		params.b_is_vf = true;
		params.vf_id = n->vfid - 1;
	}

	if (n->tuple.stringify) {
		char tuple_buffer[QEDE_FILTER_PRINT_MAX_LEN];
		char vf_print[10];

		if (n->vfid)
			snprintf(vf_print, 10, "vfid %02x ", params.vf_id);
		else
			vf_print[0] = '\0';

		n->tuple.stringify(&n->tuple, tuple_buffer);
		DP_VERBOSE(edev, NETIF_MSG_RX_STATUS,
			   "%s sw_id[0x%llx]: %s [%squeue %d]\n",
			   add_fltr ? "Adding" : "Deleting",
			   n->sw_id, tuple_buffer, vf_print, rxq_id);
	} else {
		DP_VERBOSE(edev, NETIF_MSG_RX_STATUS,
			   "%s arfs filter flow_id=%d, sw_id=0x%llx, src_port=%d, dst_port=%d, rxq=%d\n",
			   add_fltr ? "Adding" : "Deleting",
			   n->flow_id, n->sw_id, ntohs(n->tuple.src_port),
			   (n->tuple.dst_port), rxq_id);
	}

	n->used = true;
	n->filter_op = add_fltr;
	params.mode = n->tuple.mode;
	op->ntuple_filter_config(edev->cdev, n, &params);
}

static void qede_free_arfs_filter(struct qede_dev *edev,
				  struct qede_arfs_fltr_node *fltr)
{
	kfree(fltr->data);
	if (fltr->sw_id < QEDE_RFS_MAX_FLTR)
		clear_bit(fltr->sw_id, edev->arfs->arfs_fltr_bmap);
	kfree(fltr);
}

static int
qede_enqueue_fltr_and_config_searcher(struct qede_dev *edev,
				      struct qede_arfs_fltr_node *fltr,
				      u16 bucket_idx, bool b_user)
{
	edev->arfs->b_user_flow = b_user;

	fltr->mapping = dma_map_single(&edev->pdev->dev, fltr->data,
				       fltr->buf_len, DMA_TO_DEVICE);
	if (dma_mapping_error(&edev->pdev->dev, fltr->mapping)) {
		DP_NOTICE(edev, "Failed to map DMA memory for rule\n");
		qede_free_arfs_filter(edev, fltr);
		return -ENOMEM;
	}

	INIT_HLIST_NODE(&fltr->node);
	hlist_add_head(&fltr->node,
		       QEDE_ARFS_BUCKET_HEAD(edev, bucket_idx));
	edev->arfs->filter_count++;

	if (edev->arfs->filter_count == 1 &&
	    edev->arfs->mode == QED_FILTER_CONFIG_MODE_DISABLE) {
		edev->ops->configure_arfs_searcher(edev->cdev,
						   fltr->tuple.mode);
		edev->arfs->mode = fltr->tuple.mode;
	}

	return 0;
}

static void
qede_dequeue_fltr_and_config_searcher(struct qede_dev *edev,
				      struct qede_arfs_fltr_node *fltr)
{
	hlist_del(&fltr->node);
	dma_unmap_single(&edev->pdev->dev, fltr->mapping,
			 fltr->buf_len, DMA_TO_DEVICE);

	qede_free_arfs_filter(edev, fltr);
	edev->arfs->filter_count--;

	if (!edev->arfs->filter_count &&
	    edev->arfs->mode != QED_FILTER_CONFIG_MODE_DISABLE) {
		enum qed_filter_config_mode mode;

		mode = QED_FILTER_CONFIG_MODE_DISABLE;
		edev->ops->configure_arfs_searcher(edev->cdev, mode);
		edev->arfs->mode = QED_FILTER_CONFIG_MODE_DISABLE;
	}
}

#ifdef CONFIG_RFS_ACCEL
static bool qede_compare_ip_addr(struct qede_arfs_fltr_node *tpos,
				 const struct sk_buff *skb)
{
	if (skb->protocol == htons(ETH_P_IP)) {
		if (tpos->tuple.src_ipv4 == ip_hdr(skb)->saddr &&
		    tpos->tuple.dst_ipv4 == ip_hdr(skb)->daddr)
			return true;
		else
			return false;
	} else {
		struct in6_addr *src = &tpos->tuple.src_ipv6;
		u8 size = sizeof(struct in6_addr);

		if (!memcmp(src, &ipv6_hdr(skb)->saddr, size) &&
		    !memcmp(&tpos->tuple.dst_ipv6, &ipv6_hdr(skb)->daddr, size))
			return true;
		else
			return false;
	}
}

static struct qede_arfs_fltr_node *
qede_arfs_htbl_key_search(struct hlist_head *h, const struct sk_buff *skb,
			  __be16 src_port, __be16 dst_port, u8 ip_proto)
{
	struct qede_arfs_fltr_node *tpos;
#ifdef _HAS_LEGACY_HLIST_ITERATION /* ! QEDE_UPSTREAM */
	struct hlist_node *tmp_hnode;

	hlist_for_each_entry(tpos, tmp_hnode, h, node)
#else
	hlist_for_each_entry(tpos, h, node)
#endif
	if (tpos->tuple.ip_proto == ip_proto &&
	    tpos->tuple.eth_proto == skb->protocol &&
	    qede_compare_ip_addr(tpos, skb) &&
	    tpos->tuple.src_port == src_port &&
	    tpos->tuple.dst_port == dst_port)
		return tpos;

	return NULL;
}

static struct qede_arfs_fltr_node *
qede_alloc_filter(struct qede_dev *edev, int min_hlen)
{
	struct qede_arfs_fltr_node *n;
	int bit_id;

	bit_id = find_first_zero_bit(edev->arfs->arfs_fltr_bmap,
				     QEDE_RFS_MAX_FLTR);

	if (bit_id >= QEDE_RFS_MAX_FLTR)
		return NULL;

	n = kzalloc(sizeof(*n), GFP_ATOMIC);
	if (!n)
		return NULL;

	n->data = kzalloc(min_hlen, GFP_ATOMIC);
	if (!n->data) {
		kfree(n);
		return NULL;
	}

	n->sw_id = (u16)bit_id;
	set_bit(bit_id, edev->arfs->arfs_fltr_bmap);
	return n;
}

int qede_rx_flow_steer(struct net_device *dev, const struct sk_buff *skb,
		       u16 rxq_index, u32 flow_id)
{
	struct qede_dev *edev = netdev_priv(dev);
	struct qede_arfs_fltr_node *n;
	int min_hlen, rc, tp_offset;
	struct ethhdr *eth;
	__be16 *ports;
	u16 tbl_idx;
	u8 ip_proto;

#ifdef ENC_SUPPORTED
	if (skb->encapsulation)
		return -EPROTONOSUPPORT;
#endif
	if (skb->protocol != htons(ETH_P_IP) &&
	    skb->protocol != htons(ETH_P_IPV6))
		return -EPROTONOSUPPORT;

	if (skb->protocol == htons(ETH_P_IP)) {
		ip_proto = ip_hdr(skb)->protocol;
		tp_offset = sizeof(struct iphdr);
	} else {
		ip_proto = ipv6_hdr(skb)->nexthdr;
		tp_offset = sizeof(struct ipv6hdr);
	}

	if (ip_proto != IPPROTO_TCP && ip_proto != IPPROTO_UDP)
		return -EPROTONOSUPPORT;

	ports = (__be16 *)(skb->data + tp_offset);

	tbl_idx = skb_get_hash_raw(skb) & QEDE_RFS_FLW_MASK;

	spin_lock_bh(&edev->arfs->arfs_list_lock);

	n = qede_arfs_htbl_key_search(QEDE_ARFS_BUCKET_HEAD(edev, tbl_idx),
				      skb, ports[0], ports[1], ip_proto);

	if (n) {
		/* Filter match */
		n->next_rxq_id = rxq_index;

		if (test_bit(QEDE_FLTR_VALID, &n->state)) {
			if (n->rxq_id != rxq_index)
				qede_configure_arfs_fltr(edev, n, n->rxq_id,
							 false);
		} else {
			if (!n->used) {
				n->rxq_id = rxq_index;
				qede_configure_arfs_fltr(edev, n, n->rxq_id,
							 true);
			}
		}

		rc = n->sw_id;
		goto ret_unlock;
	}

	min_hlen = ETH_HLEN + skb_headlen(skb);

	n = qede_alloc_filter(edev, min_hlen);
	if (!n) {
		rc = -ENOMEM;
		goto ret_unlock;
	}

	n->buf_len = min_hlen;
	n->rxq_id = rxq_index;
	n->next_rxq_id = rxq_index;
	n->tuple.src_port = ports[0];
	n->tuple.dst_port = ports[1];
	n->flow_id = flow_id;

	if (skb->protocol == htons(ETH_P_IP)) {
		n->tuple.src_ipv4 = ip_hdr(skb)->saddr;
		n->tuple.dst_ipv4 = ip_hdr(skb)->daddr;
	} else {
		memcpy(&n->tuple.src_ipv6, &ipv6_hdr(skb)->saddr,
		       sizeof(struct in6_addr));
		memcpy(&n->tuple.dst_ipv6, &ipv6_hdr(skb)->daddr,
		       sizeof(struct in6_addr));
	}

	eth = (struct ethhdr *)n->data;
	eth->h_proto = skb->protocol;
	n->tuple.eth_proto = skb->protocol;
	n->tuple.ip_proto = ip_proto;
	n->tuple.mode = QED_FILTER_CONFIG_MODE_5_TUPLE;
	memcpy(n->data + ETH_HLEN, skb->data, skb_headlen(skb));

	rc = qede_enqueue_fltr_and_config_searcher(edev, n, tbl_idx, false);
	if (rc)
		goto ret_unlock;

	qede_configure_arfs_fltr(edev, n, n->rxq_id, true);

	spin_unlock_bh(&edev->arfs->arfs_list_lock);

	set_bit(QEDE_SP_ARFS_CONFIG, &edev->sp_flags);
	schedule_delayed_work(&edev->sp_task, 0);
	return n->sw_id;

ret_unlock:
	spin_unlock_bh(&edev->arfs->arfs_list_lock);
	return rc;
}
#endif

#ifdef _HAS_NDO_XDP /* QEDE_UPSTREAM */
static void qede_xdp_reload_func(struct qede_dev *edev,
				 struct qede_reload_args *args)
{
	struct bpf_prog *old;

	old = xchg(&edev->xdp_prog, args->u.new_prog);
	if (old)
		bpf_prog_put(old);
}

static int qede_xdp_set(struct qede_dev *edev, struct bpf_prog *prog)
{
	struct qede_reload_args args;

	/* If we're called, there was already a bpf reference increment */
	memset(&args, 0, sizeof(args));
	args.func = &qede_xdp_reload_func;
	args.u.new_prog = prog;
	args.flags = QEDE_UPDATE_COMPLETE;
	qede_reload(edev, &args);

	return 0;
}

#ifdef _HAS_NDO_BPF /* QEDE_UPSTREAM */
int qede_xdp(struct net_device *dev, struct netdev_bpf *xdp)
#else
int qede_xdp(struct net_device *dev, struct netdev_xdp *xdp)
#endif
{
	struct qede_dev *edev = netdev_priv(dev);

	/* TODO - Dave doesn't like this too much; Perhaps we'd be forced to
	 * have multiple NDO structs. Align once we have answer.
	 */
	if (!edev->dev_info.xdp_supported) {
#ifdef _HAS_XDP_QUERY_PROG
		if (xdp->command == XDP_QUERY_PROG)
			return 0;
#endif
		pr_notice_once("qede %02x:%02x.%x:Device doesn't support XDP\n",
			       edev->pdev->bus->number,
			       PCI_SLOT(edev->pdev->devfn),
			       PCI_FUNC(edev->pdev->devfn));
		return -EOPNOTSUPP;
	}

	if (edev->num_fwd_devs) {
#ifdef _HAS_XDP_QUERY_PROG
		if (xdp->command == XDP_QUERY_PROG)
			return 0;
#endif
		DP_NOTICE(edev,
			  "L2 forwarding offload enabled, can't support XDP\n");
		return -EOPNOTSUPP;
	}

	switch (xdp->command) {
	case XDP_SETUP_PROG:
		return qede_xdp_set(edev, xdp->prog);
#ifdef _HAS_XDP_QUERY_PROG
	case XDP_QUERY_PROG:
#ifdef _HAS_XDP_PROG_ID /* QEDE_UPSTREAM */
		xdp->prog_id = edev->xdp_prog ? edev->xdp_prog->aux->id : 0;
#endif
#ifdef _HAS_XDP_PROG_ATTACHED /* ! QEDE_UPSTREAM */
		xdp->prog_attached = !!edev->xdp_prog;
#endif
#endif
		return 0;
	default:
		return -EINVAL;
	}
}
#endif

void qede_arfs_filter_op(void *dev, void *filter, u8 fw_rc)
{
	struct qede_arfs_fltr_node *fltr = filter;
	struct qede_dev *edev = dev;

	fltr->fw_rc = fw_rc;

	if (fw_rc) {
		DP_NOTICE(edev,
			  "Failed arfs filter configuration fw_rc=%d, flow_id=%d, sw_id=0x%llx, src_port=%d, dst_port=%d, rxq=%d\n",
			  fw_rc, fltr->flow_id, fltr->sw_id,
			  ntohs(fltr->tuple.src_port),
			  ntohs(fltr->tuple.dst_port), fltr->rxq_id);

		spin_lock_bh(&edev->arfs->arfs_list_lock);

		fltr->used = false;
		clear_bit(QEDE_FLTR_VALID, &fltr->state);

		spin_unlock_bh(&edev->arfs->arfs_list_lock);
		return;
	}

	spin_lock_bh(&edev->arfs->arfs_list_lock);

	fltr->used = false;

	if (fltr->filter_op) {
		set_bit(QEDE_FLTR_VALID, &fltr->state);
		if (fltr->rxq_id != fltr->next_rxq_id)
			qede_configure_arfs_fltr(edev, fltr, fltr->rxq_id,
						 false);
	} else {
		clear_bit(QEDE_FLTR_VALID, &fltr->state);
		if (fltr->rxq_id != fltr->next_rxq_id) {
			fltr->rxq_id = fltr->next_rxq_id;
			qede_configure_arfs_fltr(edev, fltr,
						 fltr->rxq_id, true);
		}
	}

	spin_unlock_bh(&edev->arfs->arfs_list_lock);
}

/* Should be called while qede_lock is held */
void qede_process_arfs_filters(struct qede_dev *edev, bool free_fltr)
{
	int i;

	for (i = 0; i <= QEDE_RFS_FLW_MASK; i++) {
		struct hlist_node *temp;
#ifdef _HAS_LEGACY_HLIST_ITERATION /* ! QEDE_UPSTREAM */
		struct hlist_node *tmp1;
#endif
		struct hlist_head *head;
		struct qede_arfs_fltr_node *fltr;

		head = QEDE_ARFS_BUCKET_HEAD(edev, i);

#ifdef _HAS_LEGACY_HLIST_ITERATION /* ! QEDE_UPSTREAM */
		hlist_for_each_entry_safe(fltr, temp, tmp1, head, node) {
#else
		hlist_for_each_entry_safe(fltr, temp, head, node) {
#endif
			bool del = false;

			if (edev->state != QEDE_STATE_OPEN)
				del = true;

			spin_lock_bh(&edev->arfs->arfs_list_lock);

			if ((!test_bit(QEDE_FLTR_VALID, &fltr->state) &&
			     !fltr->used) || free_fltr) {
				qede_dequeue_fltr_and_config_searcher(edev,
								      fltr);
			} else {
				bool flow_exp = false;
#ifdef CONFIG_RFS_ACCEL
				flow_exp = rps_may_expire_flow(edev->ndev,
							       fltr->rxq_id,
							       fltr->flow_id,
							       fltr->sw_id);
#endif
				if ((flow_exp || del) && !free_fltr)
					qede_configure_arfs_fltr(edev, fltr,
								 fltr->rxq_id,
								 false);
			}

			spin_unlock_bh(&edev->arfs->arfs_list_lock);
		}
	}

	spin_lock_bh(&edev->arfs->arfs_list_lock);

	if (edev->arfs->filter_count && !edev->arfs->b_user_flow) {
		set_bit(QEDE_SP_ARFS_CONFIG, &edev->sp_flags);
		schedule_delayed_work(&edev->sp_task,
				      QEDE_SP_TASK_POLL_DELAY);
	}

	spin_unlock_bh(&edev->arfs->arfs_list_lock);
}

/* This function waits for the moment till all arfs filters
 * gets freed, as we send ramrods in non blocking mode
 */
void qede_poll_for_freeing_arfs_filters(struct qede_dev *edev)
{
	int count = QEDE_ARFS_POLL_COUNT;

	if (IS_VF(edev)) {
		DP_VERBOSE(edev, QED_MSG_DEBUG, "Force freeing all rules\n");
		qede_process_arfs_filters(edev, true);
	}

	while (count) {
		qede_process_arfs_filters(edev, false);

		if (!edev->arfs->filter_count)
			break;

		usleep_range(10, 11);
		count--;
	}

	if (!count) {
		DP_NOTICE(edev, "Timeout in polling for arfs filter free\n");

		/* Something is terribly wrong, free forcefully */
		qede_process_arfs_filters(edev, true);
	}
}

int qede_alloc_arfs(struct qede_dev *edev)
{
	int i;

	if (!edev->dev_info.common.b_arfs_capable)
		return -EINVAL;

	edev->arfs = vzalloc(sizeof(*edev->arfs));
	if (!edev->arfs) {
		DP_NOTICE(edev, "Failed to allocate aRFS memory\n");
		return -ENOMEM;
	}

	spin_lock_init(&edev->arfs->arfs_list_lock);

	for (i = 0; i <= QEDE_RFS_FLW_MASK; i++)
		INIT_HLIST_HEAD(QEDE_ARFS_BUCKET_HEAD(edev, i));

	edev->arfs->arfs_fltr_bmap = vzalloc(BITS_TO_LONGS(QEDE_RFS_MAX_FLTR) *
					     sizeof(long));
	if (!edev->arfs->arfs_fltr_bmap) {
		DP_NOTICE(edev, "Failed to allocate aRFS filter bmap\n");
		vfree(edev->arfs);
		edev->arfs = NULL;
		return -ENOMEM;
	}

	return 0;
}

void qede_free_arfs(struct qede_dev *edev)
{
	vfree(edev->arfs->arfs_fltr_bmap);
	edev->arfs->arfs_fltr_bmap = NULL;
	vfree(edev->arfs);
	edev->arfs = NULL;
}

#ifdef CONFIG_RFS_ACCEL
void qede_delete_non_user_arfs_flows(struct qede_dev *edev)
{
	if (edev->arfs->b_user_flow)
		return;

	qede_poll_for_freeing_arfs_filters(edev);
}

int qede_alloc_cpu_rmap(struct qede_dev *edev)
{
	struct cpu_rmap *rmap = NULL;

	rmap = alloc_irq_cpu_rmap(QEDE_BASE_RSS_COUNT(edev));
	if (!rmap)
		return -ENOMEM;

#ifdef _HAS_NDEV_RFS_INFO /* ! QEDE_UPSTREAM */
	netdev_extended(edev->ndev)->rfs_data.rx_cpu_rmap = rmap;
#else
	edev->ndev->rx_cpu_rmap = rmap;
#endif
	return 0;
}

void qede_free_cpu_rmap(struct qede_dev *edev)
{
	struct cpu_rmap *rmap = NULL;

#ifdef _HAS_NDEV_RFS_INFO /* ! QEDE_UPSTREAM */
	rmap = netdev_extended(edev->ndev)->rfs_data.rx_cpu_rmap;
	netdev_extended(edev->ndev)->rfs_data.rx_cpu_rmap = NULL;
#else
	rmap = edev->ndev->rx_cpu_rmap;
	edev->ndev->rx_cpu_rmap = NULL;
#endif
	if (rmap)
		free_irq_cpu_rmap(rmap);
}
#endif

static int qede_set_mcast_rx_mac(struct qede_dev *edev,
				 enum qed_filter_xcast_params_type opcode,
				 unsigned char *mac, int num_macs, u8 vport_id)
{
	struct qed_filter_params filter_cmd;
	int i;

	memset(&filter_cmd, 0, sizeof(filter_cmd));
	filter_cmd.type = QED_FILTER_TYPE_MCAST;
	filter_cmd.filter.mcast.type = opcode;
	filter_cmd.filter.mcast.num = num_macs;
	filter_cmd.vport_id = vport_id;

	for (i = 0; i < num_macs; i++, mac += ETH_ALEN)
		ether_addr_copy(filter_cmd.filter.mcast.mac[i], mac);

	return edev->ops->filter_config(edev->cdev, &filter_cmd);
}

int qede_set_mac_addr(struct net_device *ndev, void *p)
{
	struct qede_dev *edev = netdev_priv(ndev);
	struct sockaddr *addr = p;
	int rc = 0;

	/* Make sure the state doesn't transition while changing the MAC.
	 * Also, all flows accessing the dev_addr field are doing that under
	 * this lock.
	 */
	__qede_lock(edev);

	if (!is_valid_ether_addr(addr->sa_data)) {
		DP_NOTICE(edev, "The MAC address is not valid\n");
		rc = -EFAULT;
		goto out;
	}

	if (!edev->ops->check_mac(edev->cdev, addr->sa_data)) {
		DP_NOTICE(edev, "qed prevents setting MAC %pM\n",
			  addr->sa_data);
		rc = -EINVAL;
		goto out;
	}

	if (edev->state == QEDE_STATE_OPEN) {
		/* Remove the previous primary mac */
		rc = qede_set_ucast_rx_mac(edev, QED_FILTER_XCAST_TYPE_DEL,
					   ndev->dev_addr,
					   QEDE_BASE_DEV_VPORT_ID);
		if (rc)
			goto out;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)) && (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)) /* ! QEDE_UPSTREAM */
	ndev->addr_assign_type &= ~NET_ADDR_RANDOM;
#endif
	eth_hw_addr_set(ndev, addr->sa_data);
	DP_INFO(edev, "Setting device MAC to %pM\n", addr->sa_data);

	if (edev->state != QEDE_STATE_OPEN) {
		DP_VERBOSE(edev, NETIF_MSG_IFDOWN,
			   "The device is currently down\n");
		/* Ask PF to explicitly update a copy in bulletin board */
		if (IS_VF(edev) && edev->ops->req_bulletin_update_mac)
			edev->ops->req_bulletin_update_mac(edev->cdev,
							   ndev->dev_addr);
		goto out;
	}

	edev->ops->common->update_mac(edev->cdev, ndev->dev_addr);

	rc = qede_set_ucast_rx_mac(edev, QED_FILTER_XCAST_TYPE_ADD,
				   ndev->dev_addr, QEDE_BASE_DEV_VPORT_ID);
out:
	__qede_unlock(edev);
	return rc;
}

static int
qede_configure_mcast_filtering(struct qede_dev *edev,
			       struct net_device *ndev,
			       enum qed_filter_rx_mode_type *accept_flags,
			       u8 vport_id)
{
	unsigned char *mc_macs, *temp;
	struct QEDE_MC_ADDR *ha;
	int rc = 0, mc_count;
	size_t size;

	size = 64 * ETH_ALEN;

	mc_macs = kzalloc(size, GFP_KERNEL);
	if (!mc_macs) {
		DP_NOTICE(edev, "Failed to allocate memory for multicast MACs\n");
		rc = -ENOMEM;
		goto exit;
	}

	temp = mc_macs;

	/* Remove all previously configured MAC filters */
	rc = qede_set_mcast_rx_mac(edev, QED_FILTER_XCAST_TYPE_DEL,
				   mc_macs, 1, vport_id);
	if (rc)
		goto exit;

	netif_addr_lock_bh(ndev);

	mc_count = netdev_mc_count(ndev);
	if (mc_count <= 64) {
		netdev_for_each_mc_addr(ha, ndev) {
			ether_addr_copy(temp, qede_mc_addr(ha));
			temp += ETH_ALEN;
		}
	}

	netif_addr_unlock_bh(ndev);

	/* Check for all multicast @@@TBD resource allocation */
	if ((ndev->flags & IFF_ALLMULTI) ||
	    (mc_count > 64)) {
		if (*accept_flags == QED_FILTER_RX_MODE_TYPE_REGULAR)
			*accept_flags = QED_FILTER_RX_MODE_TYPE_MULTI_PROMISC;
	} else {
		/* Add all multicast MAC filters */
		rc = qede_set_mcast_rx_mac(edev, QED_FILTER_XCAST_TYPE_ADD,
					   mc_macs, mc_count, vport_id);
	}

exit:
	kfree(mc_macs);
	return rc;
}

void qede_set_rx_mode(struct net_device *ndev)
{
	struct qede_dev *edev = netdev_priv(ndev);

	set_bit(QEDE_SP_RX_MODE, &edev->sp_flags);
	schedule_delayed_work(&edev->sp_task, 0);
}

static void _qede_config_rx_mode(struct qede_dev *edev,
				 struct net_device *ndev,
				 u8 vport_id)
{
	enum qed_filter_rx_mode_type accept_flags;
	struct qed_filter_params rx_mode;
	unsigned char *uc_macs, *temp;
	struct DEV_ADDR_LIST *ha;
	int rc, uc_count;
	size_t size;

	netif_addr_lock_bh(ndev);

	uc_count = netdev_uc_count(ndev);
	size = uc_count * ETH_ALEN;

	uc_macs = kzalloc(size, GFP_ATOMIC);
	if (!uc_macs) {
		DP_NOTICE(edev, "Failed to allocate memory for unicast MACs\n");
		netif_addr_unlock_bh(ndev);
		return;
	}

	temp = uc_macs;
	netdev_for_each_uc_addr(ha, ndev) {
		ether_addr_copy(temp, ha->addr);
		temp += ETH_ALEN;
	}

	netif_addr_unlock_bh(ndev);

	/* Configure the struct for the Rx mode */
	memset(&rx_mode, 0, sizeof(struct qed_filter_params));
	rx_mode.type = QED_FILTER_TYPE_RX_MODE;

	/* Remove all previous unicast secondary macs and multicast macs
	 * (configrue / leave the primary mac)
	 */
	rc = qede_set_ucast_rx_mac(edev, QED_FILTER_XCAST_TYPE_REPLACE,
				   ndev->dev_addr, vport_id);
	if (rc)
		goto out;

	/* Check for promiscuous */
	if (ndev->flags & IFF_PROMISC)
		accept_flags = QED_FILTER_RX_MODE_TYPE_PROMISC;
	else
		accept_flags = QED_FILTER_RX_MODE_TYPE_REGULAR;

	/* Configure all filters regardless, in case promisc is rejected */
	if (uc_count < edev->dev_info.num_mac_filters) {
		int i;

		temp = uc_macs;
		for (i = 0; i < uc_count; i++) {
			rc = qede_set_ucast_rx_mac(edev,
						   QED_FILTER_XCAST_TYPE_ADD,
						   temp, vport_id);
			if (rc)
				goto out;

			temp += ETH_ALEN;
		}
	} else {
		accept_flags = QED_FILTER_RX_MODE_TYPE_PROMISC;
	}

	rc = qede_configure_mcast_filtering(edev, ndev, &accept_flags,
					    vport_id);
	if (rc)
		goto out;

	/* take care of VLAN mode */
	if (ndev->flags & IFF_PROMISC) {
		qede_config_accept_any_vlan(edev, true, vport_id);
	} else if (!edev->non_configured_vlans) {
		/* It's possible that accept_any_vlan mode is set due to a
		 * previous setting of IFF_PROMISC. If vlan credits are
		 * sufficient, disable accept_any_vlan.
		 */
		qede_config_accept_any_vlan(edev, false, vport_id);
	}

	rx_mode.filter.accept_flags = accept_flags;
	rx_mode.vport_id = vport_id;
	edev->ops->filter_config(edev->cdev, &rx_mode);
out:
	kfree(uc_macs);
}

void qede_config_rx_mode_for_all(struct qede_dev *edev)
{
	struct qede_fwd_dev *fwd_dev;

	/* configure Rx mode for base device first */
	_qede_config_rx_mode(edev, edev->ndev, QEDE_BASE_DEV_VPORT_ID);

	/* see if we have L2 forwarding offload instances and configure those */
	if (list_empty(&edev->fwd_dev_list))
		return;

	list_for_each_entry(fwd_dev, &edev->fwd_dev_list, list)
		_qede_config_rx_mode(edev, fwd_dev->upper_ndev,
				     fwd_dev->vport_id);
}

/* Must be called with qede_lock held */
void qede_config_rx_mode(struct net_device *ndev)
{
	struct qede_dev *edev = netdev_priv(ndev);

	/* config_rx_mode can be called from macvlan module
	 * when there is a change in MAC filters of macvlan device. In this case
	 * UC and MC lists are synced with base device's list.
	 * As there is no way to explicitly inform UC and MC list
	 * from macvlan device to base device, we'll just update the rx_mode by
	 * traversing upper macvlan devices and updating it
	 */
	qede_config_rx_mode_for_all(edev);
}

static struct qede_arfs_fltr_node *
qede_get_arfs_fltr_by_loc(struct hlist_head *head, u64 location)
{
	struct qede_arfs_fltr_node *fltr;
#ifdef _HAS_LEGACY_HLIST_ITERATION /* ! QEDE_UPSTREAM */
	struct hlist_node *tmp_hnode;

	hlist_for_each_entry(fltr, tmp_hnode, head, node)
#else
	hlist_for_each_entry(fltr, head, node)
#endif
	if (location == fltr->sw_id)
		return fltr;

	return NULL;
}

int qede_get_cls_rule_all(struct qede_dev *edev, struct ethtool_rxnfc *info,
			  u32 *rule_locs)
{
	struct qede_arfs_fltr_node *fltr;
#ifdef _HAS_LEGACY_HLIST_ITERATION /* ! QEDE_UPSTREAM */
	struct hlist_node *tmp_hnode;
#endif
	struct hlist_head *head;
	int cnt = 0, rc = 0;

	info->data = QEDE_RFS_MAX_FLTR;

	__qede_lock(edev);

	if (!edev->arfs) {
		rc = -EPERM;
		goto unlock;
	}

	head = QEDE_ARFS_BUCKET_HEAD(edev, 0);

#ifdef _HAS_LEGACY_HLIST_ITERATION /* ! QEDE_UPSTREAM */
	hlist_for_each_entry(fltr, tmp_hnode, head, node) {
#else
	hlist_for_each_entry(fltr, head, node) {
#endif
		if (cnt == info->rule_cnt) {
			rc = -EMSGSIZE;
			goto unlock;
		}

		rule_locs[cnt] = fltr->sw_id;
		cnt++;
	}

	info->rule_cnt = cnt;

unlock:
	__qede_unlock(edev);
	return rc;
}

int qede_get_cls_rule_entry(struct qede_dev *edev, struct ethtool_rxnfc *cmd)
{
	struct ethtool_rx_flow_spec *fsp = &cmd->fs;
	struct qede_arfs_fltr_node *fltr = NULL;
	int rc = 0;

	cmd->data = QEDE_RFS_MAX_FLTR;

	__qede_lock(edev);

	if (!edev->arfs) {
		rc = -EPERM;
		goto unlock;
	}

	fltr = qede_get_arfs_fltr_by_loc(QEDE_ARFS_BUCKET_HEAD(edev, 0),
					 fsp->location);
	if (!fltr) {
		DP_NOTICE(edev, "Rule not found - location=0x%x\n",
			  fsp->location);
		rc = -EINVAL;
		goto unlock;
	}

	if (fltr->tuple.eth_proto == htons(ETH_P_IP)) {
		if (fltr->tuple.ip_proto == IPPROTO_TCP)
			fsp->flow_type = TCP_V4_FLOW;
		else
			fsp->flow_type = UDP_V4_FLOW;

		if (fltr->tuple.vlan_id) {
			fsp->flow_type = FLOW_EXT | fsp->flow_type;
			fsp->h_ext.vlan_tci  = htons(fltr->tuple.vlan_id);
			fsp->h_ext.data[1] = fltr->vfid;
		}

		fsp->h_u.tcp_ip4_spec.psrc = fltr->tuple.src_port;
		fsp->h_u.tcp_ip4_spec.pdst = fltr->tuple.dst_port;
		fsp->h_u.tcp_ip4_spec.ip4src = fltr->tuple.src_ipv4;
		fsp->h_u.tcp_ip4_spec.ip4dst = fltr->tuple.dst_ipv4;
#ifdef _HAS_IPV6_ETHTOOL_SPEC /* QEDE_UPSTREAM */
	} else {
		if (fltr->tuple.ip_proto == IPPROTO_TCP)
			fsp->flow_type = TCP_V6_FLOW;
		else
			fsp->flow_type = UDP_V6_FLOW;
		fsp->h_u.tcp_ip6_spec.psrc = fltr->tuple.src_port;
		fsp->h_u.tcp_ip6_spec.pdst = fltr->tuple.dst_port;
		memcpy(&fsp->h_u.tcp_ip6_spec.ip6src,
		       &fltr->tuple.src_ipv6, sizeof(struct in6_addr));
		memcpy(&fsp->h_u.tcp_ip6_spec.ip6dst,
		       &fltr->tuple.dst_ipv6, sizeof(struct in6_addr));
#endif
	}

	fsp->ring_cookie = fltr->rxq_id;
#ifdef _HAS_FLOW_EXT /* QEDE_UPSTREAM */
	if (fltr->vfid && !fltr->tuple.b_vfq_enabled) {
		fsp->ring_cookie |= ((u64)fltr->vfid) <<
				    ETHTOOL_RX_FLOW_SPEC_RING_VF_OFF;
		fsp->flow_type |= FLOW_EXT;
	}
#endif
	if (fltr->b_is_drop)
		fsp->ring_cookie = RX_CLS_FLOW_DISC;
unlock:
	__qede_unlock(edev);
	return rc;
}

static int
qede_poll_arfs_filter_config(struct qede_dev *edev,
			     struct qede_arfs_fltr_node *fltr)
{
	int count = QEDE_ARFS_POLL_COUNT;

	if (IS_VF(edev))
		return 0;

	while (fltr->used && count) {
		usleep_range(10, 11);
		count--;
	}

	if (count == 0 || fltr->fw_rc) {
		DP_NOTICE(edev,
			  "Failed to configure filter, count=0x%x, fw_rc=0x%x\n",
			  count, fltr->fw_rc);
		qede_dequeue_fltr_and_config_searcher(edev, fltr);
		return -EIO;
	}

	return fltr->fw_rc;
}

static int qede_flow_get_min_header_size(struct qede_arfs_tuple *t)
{
	int size = VLAN_ETH_HLEN;

	if (t->eth_proto == htons(ETH_P_IP))
		size += sizeof(struct iphdr);
	else
		size += sizeof(struct ipv6hdr);

	if (t->ip_proto == IPPROTO_TCP)
		size += sizeof(struct tcphdr);
#ifndef QEDE_UPSTREAM /* ! QEDE_UPSTREAM */
	else if (t->ip_proto == IPPROTO_GRE)
		size += GRE_HEADER_SECTION;
#endif
	else
		size += sizeof(struct udphdr);

#ifndef QEDE_UPSTREAM /* ! QEDE_UPSTREAM */
	if (t->tunn_hdr_size)
		size += (ETH_HLEN + t->tunn_hdr_size);
#endif

	return size;
}

static bool qede_flow_spec_ipv4_cmp(struct qede_arfs_tuple *a,
				    struct qede_arfs_tuple *b)
{
	if (a->eth_proto != htons(ETH_P_IP) ||
	    b->eth_proto != htons(ETH_P_IP))
		return false;

	return ((a->src_ipv4 == b->src_ipv4) &&
		(a->dst_ipv4 == b->dst_ipv4) &&
		(a->vlan_id == b->vlan_id));
}

static void qede_flow_build_ipv4_hdr(struct qede_dev *edev,
				     struct qede_arfs_tuple *t,
				     void *header)
{
	__be16 *ports = (__be16 *)(header + ETH_HLEN + sizeof(struct iphdr));
	struct iphdr *ip = (struct iphdr *)(header + ETH_HLEN);
	struct ethhdr *eth = (struct ethhdr *)header;
	struct vlan_hdr *vlan;

	ether_addr_copy(eth->h_dest, t->dst_mac);
	eth->h_proto = t->eth_proto;

	if (t->vlan_id) {
		eth->h_proto = htons(ETH_P_8021Q);
		vlan = (struct vlan_hdr *)(header + ETH_HLEN);
		vlan->h_vlan_TCI = htons(t->vlan_id);
		vlan->h_vlan_encapsulated_proto = t->eth_proto;
		ip = (void *)ip + sizeof(struct vlan_hdr);
		ports = (void *)ports + sizeof(struct vlan_hdr);
	}

	ip->saddr = t->src_ipv4;
	ip->daddr = t->dst_ipv4;
	ip->version = 0x4;
	ip->ihl = 0x5;
	ip->protocol = t->ip_proto;
	ip->tot_len = cpu_to_be16(qede_flow_get_min_header_size(t) - ETH_HLEN);

	/* ports is weakly typed to suit both TCP and UDP ports */
	ports[0] = t->src_port;
	ports[1] = t->dst_port;

#ifndef QEDE_UPSTREAM /* ! QEDE_UPSTREAM */
	if (t->tunn_hdr_size) {
		struct udphdr *uh;
		struct ethhdr *eth;

		uh = (struct udphdr *)(header + ETH_HLEN +
					sizeof(struct iphdr));
		eth = (struct ethhdr *)((u8 *)uh + t->tunn_hdr_size);
		uh->len = cpu_to_be16(sizeof(struct udphdr) +
					t->tunn_hdr_size + ETH_HLEN);
		eth->h_proto = t->eth_proto;
	}

	/* Set protocol field in GRE header */
	if (t->ip_proto == IPPROTO_GRE)
		ports[1] = htons(ETH_P_IP);
#endif
}

static void qede_flow_stringify_ipv4_hdr(struct qede_arfs_tuple *t,
					 void *buffer)
{
	const char *prefix = t->ip_proto == IPPROTO_TCP ? "TCP" : "UDP";

	if (t->mode == QED_FILTER_CONFIG_MODE_5_TUPLE ||
	    t->mode == QED_FILTER_CONFIG_MODE_IP_DEST)
		snprintf(buffer, QEDE_FILTER_PRINT_MAX_LEN,
			 "%s %pI4 (%04x) -> %pI4 (%04x)",
			 prefix, &t->src_ipv4, t->src_port,
			 &t->dst_ipv4, t->dst_port);
	else
		snprintf(buffer, QEDE_FILTER_PRINT_MAX_LEN,
			 "%s destination port (%04x)",
			 prefix, ntohs(t->dst_port));
}

#ifdef _HAS_IPV6_ETHTOOL_SPEC /* QEDE_UPSTREAM */
static bool qede_flow_spec_ipv6_cmp(struct qede_arfs_tuple *a,
				    struct qede_arfs_tuple *b)
{
	if (a->eth_proto != htons(ETH_P_IPV6) ||
	    b->eth_proto != htons(ETH_P_IPV6))
		return false;

	return (!memcmp(&a->src_ipv6, &b->src_ipv6, sizeof(struct in6_addr)) &&
		!memcmp(&a->dst_ipv6, &b->dst_ipv6, sizeof(struct in6_addr)) &&
		(a->vlan_id == b->vlan_id));
}

static void qede_flow_build_ipv6_hdr(struct qede_dev *edev,
				     struct qede_arfs_tuple *t,
				     void *header)
{
	__be16 *ports = (__be16 *)(header + ETH_HLEN + sizeof(struct ipv6hdr));
	struct ipv6hdr *ip6 = (struct ipv6hdr *)(header + ETH_HLEN);
	struct ethhdr *eth = (struct ethhdr *)header;
	struct vlan_hdr *vlan;

	ether_addr_copy(eth->h_dest, t->dst_mac);
	eth->h_proto = t->eth_proto;

	if (t->vlan_id) {
		eth->h_proto = htons(ETH_P_8021Q);
		vlan = (struct vlan_hdr *)(header + ETH_HLEN);
		vlan->h_vlan_TCI = htons(t->vlan_id);
		vlan->h_vlan_encapsulated_proto = t->eth_proto;
		ip6 = (void *)ip6 + sizeof(struct vlan_hdr);
		ports = (void *)ports + sizeof(struct vlan_hdr);
	}

	memcpy(&ip6->saddr, &t->src_ipv6, sizeof(struct in6_addr));
	memcpy(&ip6->daddr, &t->dst_ipv6, sizeof(struct in6_addr));
	ip6->version = 0x6;

	if (t->ip_proto == IPPROTO_TCP) {
		ip6->nexthdr = NEXTHDR_TCP;
		ip6->payload_len = cpu_to_be16(sizeof(struct tcphdr));
	} else {
		ip6->nexthdr = NEXTHDR_UDP;
		ip6->payload_len = cpu_to_be16(sizeof(struct udphdr));

#ifndef QEDE_UPSTREAM /* ! QEDE_UPSTREAM */
		if (t->tunn_hdr_size) {
			u8 temp = t->tunn_hdr_size + ETH_HLEN;

			ip6->payload_len += cpu_to_be16((u16)temp);
		}
#endif
	}

	/* ports is weakly typed to suit both TCP and UDP ports */
	ports[0] = t->src_port;
	ports[1] = t->dst_port;

#ifndef QEDE_UPSTREAM /* ! QEDE_UPSTREAM */
#define FILTER_HDR_DUMP
#ifdef FILTER_HDR_DUMP
	print_hex_dump(KERN_INFO, "FILTER_HDR:", DUMP_PREFIX_NONE, 16,
		       1, header, qede_flow_get_min_header_size(t), 0);
#endif
#endif
}

/* TODO - stringify IPv6 */
#endif

/* Validate all fields no driver flow accepts are indeed unset */
static int qede_flow_spec_validate_unused(struct qede_dev *edev,
					  struct ethtool_rx_flow_spec *fs)
{
#ifdef _HAS_FLOW_EXT
	if (fs->flow_type & FLOW_MAC_EXT) {
		DP_ERR(edev, "Don't support MAC extensions\n");
		return -EOPNOTSUPP;
	}
#endif
	return 0;
}

#ifndef QEDE_UPSTREAM /* ! QEDE_UPSTREAM */
static void qede_set_udp_port_profile_mode(struct qede_arfs_tuple *t)
{
	/* Check for IANA and Linux standard ports */
	if ((t->dst_port == cpu_to_be16(4789) ||
	     t->dst_port == cpu_to_be16(8472)) &&
	     t->ip_proto == IPPROTO_UDP) {
		t->mode = QED_FILTER_CONFIG_MODE_TUNN_TYPE;
		t->tunn_hdr_size = QEDE_VXLAN_HDR_SIZE;
	} else {
		t->mode = QED_FILTER_CONFIG_MODE_L4_PORT;
	}
}

#ifndef _HAS_ETH_RX_FLOW_RULE
static bool qede_is_flow_for_gre(struct ethtool_rx_flow_spec *fs)
{
	/* All tuples zero means flow for GRE */
	if (!fs->h_u.tcp_ip4_spec.ip4src &&
	    !fs->h_u.tcp_ip4_spec.ip4dst &&
	    !fs->h_u.tcp_ip4_spec.psrc &&
	    !fs->h_u.tcp_ip4_spec.pdst) {
		return true;
	} else {
		return false;
	}
}
#endif
#endif

static int qede_set_v4_tuple_to_profile(struct qede_dev *edev,
					struct qede_arfs_tuple *t)
{
	if (t->src_port && t->dst_port && t->src_ipv4 && t->dst_ipv4) {
		t->mode = QED_FILTER_CONFIG_MODE_5_TUPLE;
		/* TODO:to support other filter for VF */
		if (t->b_vfq_enabled)
			t->mode = QED_FILTER_CONFIG_MODE_VLAN_L4_DST;
	} else if (!t->src_port && t->dst_port &&
		   !t->src_ipv4 && !t->dst_ipv4) {
#ifndef QEDE_UPSTREAM /* ! QEDE_UPSTREAM */
		qede_set_udp_port_profile_mode(t);
#else
		t->mode = QED_FILTER_CONFIG_MODE_L4_PORT;
#endif
		if (t->b_vfq_enabled)
			t->mode = QED_FILTER_CONFIG_MODE_VLAN_L4_DST;
	} else if (!t->src_port && !t->dst_port &&
		   !t->dst_ipv4 && t->src_ipv4) {
		t->mode = QED_FILTER_CONFIG_MODE_IP_SRC;
	} else if (!t->src_port && !t->dst_port &&
		   t->dst_ipv4 && !t->src_ipv4) {
		t->mode = QED_FILTER_CONFIG_MODE_IP_DEST;
#ifndef QEDE_UPSTREAM /* ! QEDE_UPSTREAM */
	} else if (t->ip_proto == IPPROTO_GRE) {
		t->mode = QED_FILTER_CONFIG_MODE_TUNN_TYPE;
#endif
	} else {
		DP_INFO(edev, "Invalid N-tuple\n");
		return -EOPNOTSUPP;
	}

	DP_NOTICE(edev, "Flow Profile configured %d\n", t->mode);
	t->ip_comp = qede_flow_spec_ipv4_cmp;
	t->build_hdr = qede_flow_build_ipv4_hdr;
	t->stringify = qede_flow_stringify_ipv4_hdr;

	return 0;
}

#ifdef _HAS_IPV6_ETHTOOL_SPEC /* QEDE_UPSTREAM */
static int qede_set_v6_tuple_to_profile(struct qede_dev *edev,
					struct qede_arfs_tuple *t,
					struct in6_addr *zaddr)
{
	if (t->src_port && t->dst_port &&
	    memcmp(&t->src_ipv6, zaddr, sizeof(struct in6_addr)) &&
	    memcmp(&t->dst_ipv6, zaddr, sizeof(struct in6_addr))) {
		t->mode = QED_FILTER_CONFIG_MODE_5_TUPLE;
		if (t->b_vfq_enabled)
			t->mode = QED_FILTER_CONFIG_MODE_VLAN_L4_DST;

	} else if (!t->src_port && t->dst_port &&
		   !memcmp(&t->src_ipv6, zaddr, sizeof(struct in6_addr)) &&
		   !memcmp(&t->dst_ipv6, zaddr, sizeof(struct in6_addr))) {
#ifndef QEDE_UPSTREAM /* ! QEDE_UPSTREAM */
		qede_set_udp_port_profile_mode(t);
#else
		t->mode = QED_FILTER_CONFIG_MODE_L4_PORT;
#endif
		if (t->b_vfq_enabled)
			t->mode = QED_FILTER_CONFIG_MODE_VLAN_L4_DST;
	} else if (!t->src_port && !t->dst_port &&
		   !memcmp(&t->dst_ipv6, zaddr, sizeof(struct in6_addr)) &&
		   memcmp(&t->src_ipv6, zaddr, sizeof(struct in6_addr))) {
		t->mode = QED_FILTER_CONFIG_MODE_IP_SRC;
	} else if (!t->src_port && !t->dst_port &&
		   memcmp(&t->dst_ipv6, zaddr, sizeof(struct in6_addr)) &&
		   !memcmp(&t->src_ipv6, zaddr, sizeof(struct in6_addr))) {
		t->mode = QED_FILTER_CONFIG_MODE_IP_DEST;
	} else {
		DP_INFO(edev, "Invalid N-tuple\n");
		return -EOPNOTSUPP;
	}

	t->ip_comp = qede_flow_spec_ipv6_cmp;
	t->build_hdr = qede_flow_build_ipv6_hdr;
	DP_NOTICE(edev, "Flow Profile configured %d\n", t->mode);
	return 0;
}
#endif

#if defined(_HAS_ETH_RX_FLOW_RULE) || defined(_HAS_TC_FLOWER) /* QEDE_UPSTREM */
#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
static int
qede_flow_parse_ports(struct qede_dev *edev, struct flow_rule *rule,
		      struct qede_arfs_tuple *t)
#else
static int qede_flow_parse_ports(struct qede_dev *edev,
				 struct tc_cls_flower_offload *f,
				 struct qede_arfs_tuple *t)
#endif
{
#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_PORTS)) {
		struct flow_match_ports match;

		flow_rule_match_ports(rule, &match);
		if ((match.key->src && match.mask->src != U16_MAX) ||
		    (match.key->dst && match.mask->dst != U16_MAX)) {
			DP_NOTICE(edev, "Do not support ports masks\n");
			return -EINVAL;
		}

		t->src_port = match.key->src;
		t->dst_port = match.key->dst;
	}
#else
	if (dissector_uses_key(f->dissector, FLOW_DISSECTOR_KEY_PORTS)) {
		struct flow_dissector_key_ports *key, *mask;

		key = skb_flow_dissector_target(f->dissector,
						FLOW_DISSECTOR_KEY_PORTS,
						f->key);
		mask = skb_flow_dissector_target(f->dissector,
						 FLOW_DISSECTOR_KEY_PORTS,
						 f->mask);

		if ((key->src && mask->src != cpu_to_be16(U16_MAX)) ||
		    (key->dst && mask->dst != cpu_to_be16(U16_MAX))) {
			DP_NOTICE(edev, "Do not support ports masks\n");
			return -EINVAL;
		}

		t->src_port = key->src;
		t->dst_port = key->dst;
	}
#endif
	return 0;
}

#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
static int
qede_flow_parse_v6_common(struct qede_dev *edev, struct flow_rule *rule,
			  struct qede_arfs_tuple *t)
#else
static int qede_flow_parse_v6_common(struct qede_dev *edev,
				     struct tc_cls_flower_offload *f,
				     struct qede_arfs_tuple *t)
#endif
{
	struct in6_addr zero_addr, addr;

	memset(&zero_addr, 0, sizeof(addr));
	memset(&addr, 0xff, sizeof(addr));

#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_IPV6_ADDRS)) {
		struct flow_match_ipv6_addrs match;

		flow_rule_match_ipv6_addrs(rule, &match);

		if ((memcmp(&match.key->src, &zero_addr, sizeof(addr)) &&
		     memcmp(&match.mask->src, &addr, sizeof(addr))) ||
		    (memcmp(&match.key->dst, &zero_addr, sizeof(addr)) &&
		     memcmp(&match.mask->dst, &addr, sizeof(addr)))) {
			DP_NOTICE(edev,
				  "Do not support IPv6 address prefix/mask\n");
			return -EINVAL;
		}

		memcpy(&t->src_ipv6, &match.key->src, sizeof(addr));
		memcpy(&t->dst_ipv6, &match.key->dst, sizeof(addr));
	}
#else
	if (dissector_uses_key(f->dissector, FLOW_DISSECTOR_KEY_IPV6_ADDRS)) {
		struct flow_dissector_key_ipv6_addrs *key, *mask;

		key = skb_flow_dissector_target(f->dissector,
						FLOW_DISSECTOR_KEY_IPV6_ADDRS,
						f->key);
		mask = skb_flow_dissector_target(f->dissector,
						 FLOW_DISSECTOR_KEY_IPV6_ADDRS,
						 f->mask);
		if ((memcmp(&key->src, &zero_addr, sizeof(addr)) &&
		     memcmp(&mask->src, &addr, sizeof(addr))) ||
		    (memcmp(&key->dst, &zero_addr, sizeof(addr)) &&
		     memcmp(&mask->dst, &addr, sizeof(addr)))) {
			DP_NOTICE(edev,
				  "Do not support IPv6 address prefix/mask\n");
			return -EINVAL;
		}

		memcpy(&t->src_ipv6, &key->src, sizeof(addr));
		memcpy(&t->dst_ipv6, &key->dst, sizeof(addr));
	}
#endif

#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
	if (qede_flow_parse_ports(edev, rule, t))
#else
	if (qede_flow_parse_ports(edev, f, t))
#endif
		return -EINVAL;

	return qede_set_v6_tuple_to_profile(edev, t, &zero_addr);
}

#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
static int qede_flow_parse_v4_common(struct qede_dev *edev,
				     struct flow_rule *rule,
				     struct qede_arfs_tuple *t)
#else
static int qede_flow_parse_v4_common(struct qede_dev *edev,
				     struct tc_cls_flower_offload *f,
				     struct qede_arfs_tuple *t)
#endif
{
#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_IPV4_ADDRS)) {
		struct flow_match_ipv4_addrs match;

		flow_rule_match_ipv4_addrs(rule, &match);

		if ((match.key->src && match.mask->src != U32_MAX) ||
		    (match.key->dst && match.mask->dst != U32_MAX)) {
			DP_NOTICE(edev, "Do not support ipv4 prefix/masks\n");
			return -EINVAL;
		}

		t->src_ipv4 = match.key->src;
		t->dst_ipv4 = match.key->dst;
	}
#else
	if (dissector_uses_key(f->dissector, FLOW_DISSECTOR_KEY_IPV4_ADDRS)) {
		struct flow_dissector_key_ipv4_addrs *key, *mask;

		key = skb_flow_dissector_target(f->dissector,
						FLOW_DISSECTOR_KEY_IPV4_ADDRS,
						f->key);
		mask = skb_flow_dissector_target(f->dissector,
						 FLOW_DISSECTOR_KEY_IPV4_ADDRS,
						 f->mask);

		if ((key->src && mask->src != cpu_to_be32(U32_MAX)) ||
		    (key->dst && mask->dst != cpu_to_be32(U32_MAX))) {
			DP_NOTICE(edev, "Do not support ipv4 prefix/masks\n");
			return -EINVAL;
		}

		t->src_ipv4 = key->src;
		t->dst_ipv4 = key->dst;
	}
#endif

#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
	if (qede_flow_parse_ports(edev, rule, t))
#else
	if (qede_flow_parse_ports(edev, f, t))
#endif
		return -EINVAL;

	return qede_set_v4_tuple_to_profile(edev, t);
}

#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
static int
qede_flow_parse_tcp_v6(struct qede_dev *edev, struct flow_rule *rule,
		       struct qede_arfs_tuple *tuple)
#else
static int
qede_flow_parse_tcp_v6(struct qede_dev *edev, struct tc_cls_flower_offload *f,
		       struct qede_arfs_tuple *tuple)
#endif
{
	tuple->ip_proto = IPPROTO_TCP;
	tuple->eth_proto = htons(ETH_P_IPV6);

#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
	return qede_flow_parse_v6_common(edev, rule, tuple);
#else
	return qede_flow_parse_v6_common(edev, f, tuple);
#endif
}

#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
static int
qede_flow_parse_tcp_v4(struct qede_dev *edev, struct flow_rule *rule,
		       struct qede_arfs_tuple *tuple)
#else
static int
qede_flow_parse_tcp_v4(struct qede_dev *edev, struct tc_cls_flower_offload *f,
		       struct qede_arfs_tuple *tuple)
#endif
{
	tuple->ip_proto = IPPROTO_TCP;
	tuple->eth_proto = htons(ETH_P_IP);

#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
	return qede_flow_parse_v4_common(edev, rule, tuple);
#else
	return qede_flow_parse_v4_common(edev, f, tuple);
#endif
}

#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
static int
qede_flow_parse_udp_v6(struct qede_dev *edev, struct flow_rule *rule,
		       struct qede_arfs_tuple *tuple)
#else
static int
qede_flow_parse_udp_v6(struct qede_dev *edev, struct tc_cls_flower_offload *f,
		       struct qede_arfs_tuple *tuple)
#endif
{
	tuple->ip_proto = IPPROTO_UDP;
	tuple->eth_proto = htons(ETH_P_IPV6);

#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
	return qede_flow_parse_v6_common(edev, rule, tuple);
#else
	return qede_flow_parse_v6_common(edev, f, tuple);
#endif
}

#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
static int
qede_flow_parse_udp_v4(struct qede_dev *edev, struct flow_rule *rule,
		       struct qede_arfs_tuple *tuple)
#else
static int
qede_flow_parse_udp_v4(struct qede_dev *edev, struct tc_cls_flower_offload *f,
		       struct qede_arfs_tuple *tuple)
#endif
{
	tuple->ip_proto = IPPROTO_UDP;
	tuple->eth_proto = htons(ETH_P_IP);

#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
	return qede_flow_parse_v4_common(edev, rule, tuple);
#else
	return qede_flow_parse_v4_common(edev, f, tuple);
#endif
}

#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
static int
qede_parse_flow_attr(struct qede_dev *edev, __be16 proto,
		     struct flow_rule *rule,
		     struct qede_arfs_tuple *tuple)
#else
static int
qede_parse_flow_attr(struct qede_dev *edev, __be16 proto,
		     struct tc_cls_flower_offload *f,
		     struct qede_arfs_tuple *tuple)
#endif
{
#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
	struct flow_dissector *dissector = rule->match.dissector;
#else
	struct flow_dissector *dissector = f->dissector;
#endif
	int rc = -EINVAL;
	u8 ip_proto = 0;

	if (dissector->used_keys &
	    ~(BIT(FLOW_DISSECTOR_KEY_CONTROL) |
	      BIT(FLOW_DISSECTOR_KEY_IPV4_ADDRS) |
	      BIT(FLOW_DISSECTOR_KEY_BASIC) |
	      BIT(FLOW_DISSECTOR_KEY_IPV6_ADDRS) |
#ifdef _HAS_FLOW_DISSECTOR_KEY_VLAN /* QEDE_UPSTREM */
	      BIT(FLOW_DISSECTOR_KEY_VLAN) |
#endif
	      BIT(FLOW_DISSECTOR_KEY_PORTS))) {
		DP_NOTICE(edev, "Unsupported key set:0x%x\n",
				dissector->used_keys);
		return -EOPNOTSUPP;
	}

	if (proto != htons(ETH_P_IP) &&
	    proto != htons(ETH_P_IPV6)) {
		DP_NOTICE(edev, "Unsupported proto=0x%x\n", proto);
		return -EPROTONOSUPPORT;
	}

#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_BASIC)) {
		struct flow_match_basic match;

		flow_rule_match_basic(rule, &match);
		ip_proto = match.key->ip_proto;
	}
#else
	if (dissector_uses_key(f->dissector, FLOW_DISSECTOR_KEY_BASIC)) {
		struct flow_dissector_key_basic *key;

		key = skb_flow_dissector_target(f->dissector,
						FLOW_DISSECTOR_KEY_BASIC,
						f->key);
		ip_proto = key->ip_proto;
	}
#endif

	if (ip_proto == IPPROTO_TCP && proto == htons(ETH_P_IP))
#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
		rc = qede_flow_parse_tcp_v4(edev, rule, tuple);
#else
		rc = qede_flow_parse_tcp_v4(edev, f, tuple);
#endif
	else if (ip_proto == IPPROTO_TCP && proto == htons(ETH_P_IPV6))
#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
		rc = qede_flow_parse_tcp_v6(edev, rule, tuple);
#else
		rc = qede_flow_parse_tcp_v6(edev, f, tuple);
#endif
	else if (ip_proto == IPPROTO_UDP && proto == htons(ETH_P_IP))
#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
		rc = qede_flow_parse_udp_v4(edev, rule, tuple);
#else
		rc = qede_flow_parse_udp_v4(edev, f, tuple);
#endif
	else if (ip_proto == IPPROTO_UDP && proto == htons(ETH_P_IPV6))
#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
		rc = qede_flow_parse_udp_v6(edev, rule, tuple);
#else
		rc = qede_flow_parse_udp_v6(edev, f, tuple);
#endif
	else
		DP_NOTICE(edev, "Invalid tc protocol request\n");

	return rc;
}

#ifndef _HAS_ETH_RX_FLOW_RULE
static int qede_get_macvlan_vport(struct qede_dev *edev, int ifindex,
				  int *vport_id)
{
	struct net_device *upper_ndev;

	upper_ndev = __dev_get_by_index(dev_net(edev->ndev), ifindex);
	if (upper_ndev && netif_is_macvlan(upper_ndev)) {
		struct macvlan_dev *vlan = netdev_priv(upper_ndev);
		struct qede_fwd_dev *fwd_dev;
#ifdef _HAS_MACVLAN_DEV_ACCEL_PRIV
		if (!vlan->accel_priv)
			return -EINVAL;

		fwd_dev = vlan->accel_priv;
#else
		if (!vlan->fwd_priv)
			return -EINVAL;

		fwd_dev = vlan->fwd_priv;
#endif
		*vport_id = fwd_dev->vport_id;
		return 0;
	}

	return -EINVAL;
}

static int qede_get_vf_by_ifindex(struct qede_dev *edev, int ifindex,
				int *vf_idx)
{
	struct pci_dev *pdev = edev->pdev;
	u16 vendor = pdev->vendor;
	struct net_device *upper;
	struct pci_dev *vfdev;
	int vf = 0;
	u16 vf_id;
	int pos;

	if (IS_VF(edev))
		pdev = pci_physfn(pdev);

	pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_SRIOV);
	if (!pos)
		return -EINVAL;

	pci_read_config_word(pdev, pos + PCI_SRIOV_VF_DID, &vf_id);
	vfdev = pci_get_device(vendor, vf_id, NULL);

	for (; vfdev; vfdev = pci_get_device(vendor, vf_id, vfdev)) {
		if (!vfdev->is_virtfn)
			continue;

#ifdef CONFIG_QED_SRIOV
		if (vfdev->physfn != pdev)
			continue;
#endif
		if (!IS_VF(edev)) {
			if (vf >= edev->num_vfs)
				continue;
		}

		upper = pci_get_drvdata(vfdev);
		if (upper && upper->ifindex == ifindex) {
			*vf_idx = ++vf;

			return 0;
		}

		++vf;
	}

	return -EINVAL;
}

static int qede_parse_actions(struct qede_dev *edev, struct tcf_exts *exts,
			      int *vf_id, int *vport_id, bool *drop)
{
	const struct tc_action *a;
	int i, rc = 0, num_act = 0;
	LIST_HEAD(actions);
	bool nr_act;

	i = 0;

#ifdef _HAS_EXTS_ACTION /* QEDE_UPSTREAM */
	nr_act = !tcf_exts_has_actions(exts);
#else
	nr_act = tc_no_actions(exts);
#endif

	if (nr_act) {
		DP_NOTICE(edev, "No tc actions received\n");
		return -EINVAL;
	}
#if defined(_HAS_TC_EXTS_ITER) /* QEDE_UPSTREAM */
	tcf_exts_for_each_action(i, a, exts) {
#elif defined(_HAS_TC_EXTS_TO_LIST)
	tcf_exts_to_list(exts, &actions);
	list_for_each_entry(a, &actions, list) {
#else
	tc_for_each_action(a, exts) {
#endif
		if (is_tcf_mirred_egress_redirect(a)) {
			int ifindex;
#ifdef _HAS_TCF_MIRRED_DEV
			ifindex = tcf_mirred_dev(a)->ifindex;
#else
			ifindex = tcf_mirred_ifindex(a);
#endif
			rc = qede_get_macvlan_vport(edev, ifindex,
						    vport_id);
			if (rc < 0)
				rc = qede_get_vf_by_ifindex(edev, ifindex,
							    vf_id);
			num_act++;
		} else if (is_tcf_gact_shot(a)) {
			*drop = true;
			num_act++;
		} else {
			DP_NOTICE(edev,
				  "Unsupported action received\n");
			return -EINVAL;
		}
	}

	if (num_act > 1) {
		DP_NOTICE(edev, "More than one actions not supported\n");
		return -EINVAL;
	}

	return rc;
}
#else
static int qede_parse_actions(struct qede_dev *edev,
			      struct flow_action *flow_action)
{
	const struct flow_action_entry *act;
	int i;

	if (!flow_action_has_entries(flow_action)) {
		DP_NOTICE(edev, "No actions received\n");
		return -EINVAL;
	}

	flow_action_for_each(i, act, flow_action) {
		switch (act->id) {
		case FLOW_ACTION_DROP:
			break;
		case FLOW_ACTION_QUEUE:
			if (act->queue.vf)
				break;

			if (act->queue.index >= QEDE_BASE_RSS_COUNT(edev)) {
				DP_NOTICE(edev, "Queue out-of-bounds Configured:%d Max queue:%d\n",
					  act->queue.index, QEDE_BASE_RSS_COUNT(edev));
				return -EINVAL;
			}
			break;
		default:
			DP_NOTICE(edev, "No valid flow action\n");
			return -EINVAL;
		}
	}

	return 0;
}
#endif
#endif

/* Should be called only when qede lock is held */
#ifdef _HAS_ETH_RX_FLOW_RULE
static int qede_flow_spec_validate(struct qede_dev *edev,
				   struct flow_action *flow_action,
				   struct qede_arfs_tuple *t,
				   __u32 location)
#else
static int qede_flow_spec_validate(struct qede_dev *edev,
				   struct ethtool_rx_flow_spec *fs,
				   struct qede_arfs_tuple *t,
				   __u32 location)
#endif
{
	if (location >= QEDE_RFS_MAX_FLTR) {
		DP_NOTICE(edev, "Location out-of-bounds\n");
		return -EINVAL;
	}

	/* Check location isn't already in use */
	if (test_bit(location, edev->arfs->arfs_fltr_bmap)) {
		DP_NOTICE(edev, "Location already in use\n");
		return -EINVAL;
	}

	/* TODO_VFQ Check here for PF mode*/
	/* Check if the filtering-mode could support the filter */
	if (edev->arfs->filter_count &&
	    edev->arfs->mode != t->mode) {
		DP_NOTICE(edev,
			  "flow_spec would require filtering mode %08x, but %08x is configured\n",
			  t->mode, edev->arfs->filter_count);
		return -EINVAL;
	}

#ifndef _HAS_ETH_RX_FLOW_RULE
	/* If drop requested then no need to validate other data */
	if (fs->ring_cookie == RX_CLS_FLOW_DISC)
		return 0;

	/* We want to validate the validity of the queue-index, but this
	 * is suitable only for the PF own queues. If it's for its VF queues,
	 * we'd let qed block it instead,
	 */
#ifdef _HAS_FLOW_EXT
	if ((fs->flow_type & FLOW_EXT) &&
	    (fs->h_ext.data[0] || fs->h_ext.data[1]))
		return 0;
#endif
	if (ethtool_get_flow_spec_ring_vf(fs->ring_cookie))
		return 0;

	if (fs->ring_cookie >= QEDE_BASE_RSS_COUNT(edev)) {
		DP_NOTICE(edev, "Queue out-of-bounds\n");
		return -EINVAL;
	}
#else
	if (qede_parse_actions(edev, flow_action)) {
		DP_NOTICE(edev, "Invalid qede_parse_actions\n");
		return -EINVAL;
	}
#endif
	return 0;
}

#ifndef _HAS_ETH_RX_FLOW_RULE /* ! QEDE_UPSTREM */
static int qede_flow_spec_to_tuple_ipv4_common(struct qede_dev *edev,
					       struct qede_arfs_tuple *t,
					       struct ethtool_rx_flow_spec *fs)
{
	if ((fs->h_u.tcp_ip4_spec.ip4src &
	    fs->m_u.tcp_ip4_spec.ip4src) !=
	    fs->h_u.tcp_ip4_spec.ip4src) {
		DP_INFO(edev, "Don't support IP-masks\n");
		return -EOPNOTSUPP;
	}

	if ((fs->h_u.tcp_ip4_spec.ip4dst &
	    fs->m_u.tcp_ip4_spec.ip4dst) !=
	    fs->h_u.tcp_ip4_spec.ip4dst) {
		DP_INFO(edev, "Don't support IP-masks\n");
		return -EOPNOTSUPP;
	}

	if ((fs->h_u.tcp_ip4_spec.psrc &
	    fs->m_u.tcp_ip4_spec.psrc) !=
	    fs->h_u.tcp_ip4_spec.psrc) {
		DP_INFO(edev, "Don't support port-masks\n");
		return -EOPNOTSUPP;
	}

	if ((fs->h_u.tcp_ip4_spec.pdst &
	    fs->m_u.tcp_ip4_spec.pdst) !=
	    fs->h_u.tcp_ip4_spec.pdst) {
		DP_INFO(edev, "Don't support port-masks\n");
		return -EOPNOTSUPP;
	}

	if (fs->h_u.tcp_ip4_spec.tos) {
		DP_INFO(edev, "Don't support tos\n");
		return -EOPNOTSUPP;
	}

	t->eth_proto = htons(ETH_P_IP);
	t->src_ipv4 = fs->h_u.tcp_ip4_spec.ip4src;
	t->dst_ipv4 = fs->h_u.tcp_ip4_spec.ip4dst;
	t->src_port = fs->h_u.tcp_ip4_spec.psrc;
	t->dst_port = fs->h_u.tcp_ip4_spec.pdst;

	return qede_set_v4_tuple_to_profile(edev, t);
}

static int qede_flow_spec_to_tuple_tcpv4(struct qede_dev *edev,
					 struct qede_arfs_tuple *t,
					 struct ethtool_rx_flow_spec *fs)
{
	t->ip_proto = IPPROTO_TCP;

	if (qede_flow_spec_to_tuple_ipv4_common(edev, t, fs))
		return -EINVAL;

	return 0;
}

static int qede_flow_spec_to_tuple_udpv4(struct qede_dev *edev,
					 struct qede_arfs_tuple *t,
					 struct ethtool_rx_flow_spec *fs)
{
	t->ip_proto = IPPROTO_UDP;

#ifndef QEDE_UPSTREAM /* ! QEDE_UPSTREAM */
	if (qede_is_flow_for_gre(fs))
		t->ip_proto = IPPROTO_GRE;
#endif

	if (qede_flow_spec_to_tuple_ipv4_common(edev, t, fs))
		return -EINVAL;

	return 0;
}

#ifdef _HAS_IPV6_ETHTOOL_SPEC /* QEDE_UPSTREAM */
static int qede_flow_spec_to_tuple_ipv6_common(struct qede_dev *edev,
					       struct qede_arfs_tuple *t,
					       struct ethtool_rx_flow_spec *fs)
{
	struct in6_addr zero_addr;

	memset(&zero_addr, 0, sizeof(zero_addr));

	if ((fs->h_u.tcp_ip6_spec.psrc &
	    fs->m_u.tcp_ip6_spec.psrc) !=
	    fs->h_u.tcp_ip6_spec.psrc) {
		DP_INFO(edev, "Don't support port-masks\n");
		return -EOPNOTSUPP;
	}

	if ((fs->h_u.tcp_ip6_spec.pdst &
	    fs->m_u.tcp_ip6_spec.pdst) !=
	    fs->h_u.tcp_ip6_spec.pdst) {
		DP_INFO(edev, "Don't support port-masks\n");
		return -EOPNOTSUPP;
	}

	if (fs->h_u.tcp_ip6_spec.tclass) {
		DP_INFO(edev, "Don't support tclass\n");
		return -EOPNOTSUPP;
	}

	t->eth_proto = htons(ETH_P_IPV6);
	memcpy(&t->src_ipv6, &fs->h_u.tcp_ip6_spec.ip6src,
	       sizeof(struct in6_addr));
	memcpy(&t->dst_ipv6, &fs->h_u.tcp_ip6_spec.ip6dst,
	       sizeof(struct in6_addr));
	t->src_port = fs->h_u.tcp_ip6_spec.psrc;
	t->dst_port = fs->h_u.tcp_ip6_spec.pdst;

	return qede_set_v6_tuple_to_profile(edev, t, &zero_addr);
}

static int qede_flow_spec_to_tuple_tcpv6(struct qede_dev *edev,
					 struct qede_arfs_tuple *t,
					 struct ethtool_rx_flow_spec *fs)
{
	t->ip_proto = IPPROTO_TCP;

	if (qede_flow_spec_to_tuple_ipv6_common(edev, t, fs))
		return -EINVAL;

	return 0;
}

static int qede_flow_spec_to_tuple_udpv6(struct qede_dev *edev,
					 struct qede_arfs_tuple *t,
					 struct ethtool_rx_flow_spec *fs)
{
	t->ip_proto = IPPROTO_UDP;

	if (qede_flow_spec_to_tuple_ipv6_common(edev, t, fs))
		return -EINVAL;

	return 0;
}
#endif

static int qede_flow_spec_to_rule(struct qede_dev *edev,
				  struct qede_arfs_tuple *t,
				  struct ethtool_rx_flow_spec *fs)
{
	if (qede_flow_spec_validate_unused(edev, fs))
		return -EOPNOTSUPP;

	switch ((fs->flow_type & ~FLOW_EXT)) {
	case TCP_V4_FLOW:
		return qede_flow_spec_to_tuple_tcpv4(edev, t, fs);
	case UDP_V4_FLOW:
		return qede_flow_spec_to_tuple_udpv4(edev, t, fs);
#ifdef _HAS_IPV6_ETHTOOL_SPEC /* QEDE_UPSTREAM */
	case TCP_V6_FLOW:
		return qede_flow_spec_to_tuple_tcpv6(edev, t, fs);
	case UDP_V6_FLOW:
		return qede_flow_spec_to_tuple_udpv6(edev, t, fs);
#endif
	default:
		DP_VERBOSE(edev, NETIF_MSG_IFUP,
			   "Can't support flow of type %08x\n", fs->flow_type);
		return -EOPNOTSUPP;
	}

	return 0;
}
#else
static int qede_flow_spec_to_rule(struct qede_dev *edev,
				  struct qede_arfs_tuple *t,
				  struct ethtool_rx_flow_spec *fs)
{
	struct ethtool_rx_flow_spec_input input = {};
	struct ethtool_rx_flow_rule *flow;
	__be16 proto;
	int err = 0;

	if (qede_flow_spec_validate_unused(edev, fs))
		return -EOPNOTSUPP;

	switch ((fs->flow_type & ~FLOW_EXT)) {
	case TCP_V4_FLOW:
	case UDP_V4_FLOW:
		proto = htons(ETH_P_IP);
		break;
	case TCP_V6_FLOW:
	case UDP_V6_FLOW:
		proto = htons(ETH_P_IPV6);
		break;
	default:
		DP_VERBOSE(edev, NETIF_MSG_IFUP,
			   "Can't support flow of type %08x\n", fs->flow_type);
		return -EOPNOTSUPP;
	}

	input.fs = fs;

	flow = ethtool_rx_flow_rule_create(&input);
	if (IS_ERR(flow))
		return PTR_ERR(flow);

	if (qede_parse_flow_attr(edev, proto, flow->rule, t)) {
		err = -EINVAL;
		goto err_out;
	}

	/* Make sure location is valid and filter isn't already set */
	err = qede_flow_spec_validate(edev, &flow->rule->action, t,
				      fs->location);
err_out:
	ethtool_rx_flow_rule_destroy(flow);
	return err;
}
#endif

/* Must be called while qede lock is held */
static struct qede_arfs_fltr_node *
qede_flow_find_fltr(struct qede_dev *edev, struct qede_arfs_tuple *t)
{
	struct qede_arfs_fltr_node *fltr;

#ifdef _HAS_LEGACY_HLIST_ITERATION /* ! QEDE_UPSTREAM */
	struct hlist_node *tmp1;
#endif
	struct hlist_node *temp;
	struct hlist_head *head;

	head = QEDE_ARFS_BUCKET_HEAD(edev, 0);
#ifdef _HAS_LEGACY_HLIST_ITERATION /* ! QEDE_UPSTREAM */
	hlist_for_each_entry_safe(fltr, temp, tmp1, head, node) {
#else
	hlist_for_each_entry_safe(fltr, temp, head, node) {
#endif
		if (fltr->tuple.ip_proto == t->ip_proto &&
		    fltr->tuple.src_port == t->src_port &&
		    fltr->tuple.dst_port == t->dst_port &&
		    t->ip_comp(&fltr->tuple, t))
			return fltr;
	}

	return NULL;
}

static void qede_flow_set_destination(struct qede_dev *edev,
				      struct qede_arfs_fltr_node *n,
				      struct ethtool_rx_flow_spec *fs)
{
	struct ifla_vf_info ivi = {0};

	if (fs->ring_cookie == RX_CLS_FLOW_DISC) {
		n->b_is_drop = true;
		return;
	}

	if (fs->flow_type & FLOW_EXT) {
		/* Non-standard usage; The lower 32-bit of the user-def field
		 * indicate the VF [notice here it's going to be 0-based].
		 * Action is supposed to show all the VFs queues, but it's
		 * insignificant [since we're going to configure RSS].
		 */
#ifdef _HAS_FLOW_EXT
		if (!IS_VF(edev))
			n->vfid = be32_to_cpu(fs->h_ext.data[1]) + 1;
#endif
	} else {
		n->vfid = ethtool_get_flow_spec_ring_vf(fs->ring_cookie);
		n->rxq_id = ethtool_get_flow_spec_ring(fs->ring_cookie);
	}
	n->next_rxq_id = n->rxq_id;

	/* Copy mac and Vlan from interface */
	ether_addr_copy(n->tuple.dst_mac, edev->ndev->dev_addr);

	/* Populate mac and vlan of VF in tuple if rule is for VF
	 * pushed from PF.
	 */
	if (n->vfid) {
		qede_get_vf_config(edev->ndev, n->vfid-1, &ivi);
		/* If VLAN not configured via rule use Hypervisor VLAN */
		if (!n->tuple.vlan_id)
			n->tuple.vlan_id = ivi.vlan;
		ether_addr_copy(n->tuple.dst_mac, ivi.mac);
	}

	if (n->tuple.b_vfq_enabled)
		DP_NOTICE(edev,
			  "Configuring vf queue selection MAC %pM VLAN %d Queue 0x%02x\n",
			   n->tuple.dst_mac, n->tuple.vlan_id, n->rxq_id);
	else
		DP_NOTICE(edev,
			  "Configuring RSS for MAC %pM VLAN %d Queue 0x%02x\n",
			   n->tuple.dst_mac, n->tuple.vlan_id, n->rxq_id);
}

int qede_add_cls_rule(struct qede_dev *edev, struct ethtool_rxnfc *info)
{
	struct ethtool_rx_flow_spec *fsp = &info->fs;
	struct qede_arfs_fltr_node *n;
	struct qede_arfs_tuple t = { {0} };
	int min_hlen, rc;
	u16 vlan_id = 0;

	__qede_lock(edev);

	if (!edev->arfs) {
		rc = -EPERM;
		goto unlock;
	}

	n = kzalloc(sizeof(*n), GFP_KERNEL);
	if (!n) {
		rc = -ENOMEM;
		goto unlock;
	}

	/* if user-def and/or vlan in filter */
	if (fsp->flow_type & FLOW_EXT) {
#ifdef _HAS_FLOW_EXT
		n->rxq_id = (u16)be32_to_cpu(fsp->h_ext.data[0]);

		/* if VLAN in filter use this */
		if (fsp->h_ext.vlan_etype || fsp->h_ext.vlan_tci) {
			n->rxq_id = fsp->ring_cookie;
			vlan_id  = be16_to_cpu(fsp->h_ext.vlan_tci) & VLAN_VID_MASK;
			t.vlan_id = vlan_id;
		}

		if (n->rxq_id == VFQ_RSS_NUM) {
			n->rxq_id = 0;
			if (fsp->h_ext.vlan_etype || fsp->h_ext.vlan_tci)
				fsp->ring_cookie = ((__u64)fsp->ring_cookie &
						    ~ETHTOOL_RX_FLOW_SPEC_RING);
			else
				fsp->h_ext.data[0] = 0;
		} else {
			t.b_vfq_enabled = true;
		}
#endif
	} else { /* non user-def and vlan filter */
		n->rxq_id = ethtool_get_flow_spec_ring(fsp->ring_cookie);
		if (n->rxq_id == VFQ_RSS_NUM) {
			n->rxq_id = 0;
			fsp->ring_cookie = (fsp->ring_cookie &
					    ~ETHTOOL_RX_FLOW_SPEC_RING);
		} else {
			t.b_vfq_enabled = true;
		}
	}

	DP_NOTICE(edev, "Classification rules vlan:%d Queue:0x%x\n", vlan_id, n->rxq_id);

	/* Translate the flow specification into something fittign our DB */
	rc = qede_flow_spec_to_rule(edev, &t, fsp);
	if (rc)
		goto unlock;

#ifndef _HAS_ETH_RX_FLOW_RULE /* ! QEDE_UPSTREM */
	/* Make sure location is valid and filter isn't already set */
	rc = qede_flow_spec_validate(edev, fsp, &t, fsp->location);
	if (rc)
		goto unlock;
#endif

	if (qede_flow_find_fltr(edev, &t)) {
		rc = -EINVAL;
		goto unlock;
	}

	min_hlen = qede_flow_get_min_header_size(&t);
	n->data = kzalloc(min_hlen, GFP_KERNEL);
	if (!n->data) {
		kfree(n);
		rc = -ENOMEM;
		goto unlock;
	}

	n->sw_id = fsp->location;
	set_bit(n->sw_id, edev->arfs->arfs_fltr_bmap);
	n->buf_len = min_hlen;

	memcpy(&n->tuple, &t, sizeof(t));

	qede_flow_set_destination(edev, n, fsp);

	/* Build a minimal header according to the flow */
	if (n->tuple.build_hdr)
		n->tuple.build_hdr(edev, &n->tuple, n->data);

	rc = qede_enqueue_fltr_and_config_searcher(edev, n, 0, true);
	if (rc)
		goto unlock;

	qede_configure_arfs_fltr(edev, n, n->rxq_id, true);
	rc = qede_poll_arfs_filter_config(edev, n);
unlock:
	__qede_unlock(edev);

	return rc;
}

int qede_delete_flow_filter(struct qede_dev *edev, u64 cookie)
{
	struct qede_arfs_fltr_node *fltr = NULL;
	int rc = -EPERM;

	__qede_lock(edev);
	if (!edev->arfs)
		goto unlock;

	fltr = qede_get_arfs_fltr_by_loc(QEDE_ARFS_BUCKET_HEAD(edev, 0),
					 cookie);
	if (!fltr)
		goto unlock;

	qede_configure_arfs_fltr(edev, fltr, fltr->rxq_id, false);

	rc = qede_poll_arfs_filter_config(edev, fltr);
	if (rc == 0)
		qede_dequeue_fltr_and_config_searcher(edev, fltr);

unlock:
	__qede_unlock(edev);
	return rc;
}

int qede_get_arfs_filter_count(struct qede_dev *edev)
{
	int count = 0;

	__qede_lock(edev);

	if (!edev->arfs)
		goto unlock;

	count = edev->arfs->filter_count;

unlock:
	__qede_unlock(edev);
	return count;
}

#if defined(_HAS_TC_FLOWER) || defined(_HAS_FLOW_BLOCK_OFFLOAD) /* QEDE_UPSTREAM */
#if defined(_HAS_FLOW_BLOCK_OFFLOAD) /* QEDE_UPSTREAM */
int qede_add_tc_flower_fltr(struct qede_dev *edev, __be16 proto,
			    struct flow_cls_offload *f)
#else
int qede_add_tc_flower_fltr(struct qede_dev *edev, __be16 proto,
			    struct tc_cls_flower_offload *f)
#endif
{
	int min_hlen, rc = -EINVAL, vf_id = 0, vport_id = 0;
	struct qede_arfs_fltr_node *n;
	struct qede_arfs_tuple t = { {0} };
	bool drop = false;

	__qede_lock(edev);

	if (!edev->arfs) {
		rc = -EPERM;
		goto unlock;
	}

	/* parse flower attribute and prepare filter */
#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
	if (qede_parse_flow_attr(edev, proto, f->rule, &t))
#else
	if (qede_parse_flow_attr(edev, proto, f, &t))
#endif
		goto unlock;

	/* Validate profile mode and number of filters */
	if ((edev->arfs->filter_count && edev->arfs->mode != t.mode) ||
	    edev->arfs->filter_count == QEDE_RFS_MAX_FLTR) {
		DP_NOTICE(edev,
			  "Filter configuration invalidated, filter mode=0x%x, configured mode=0x%x, filter count=0x%x\n",
			  t.mode, edev->arfs->mode, edev->arfs->filter_count);
		goto unlock;
	}

	/* parse tc actions and get the vf_id */
#ifdef _HAS_ETH_RX_FLOW_RULE /* QEDE_UPSTREM */
	drop = true;

	if (qede_parse_actions(edev, &f->rule->action))
#else
	if (qede_parse_actions(edev, f->exts, &vf_id,
			       &vport_id, &drop))
#endif
		goto unlock;

	if (qede_flow_find_fltr(edev, &t))
		goto unlock;

	n = kzalloc(sizeof(*n), GFP_KERNEL);
	if (!n) {
		rc = -ENOMEM;
		goto unlock;
	}

	min_hlen = qede_flow_get_min_header_size(&t);
	n->data = kzalloc(min_hlen, GFP_KERNEL);
	if (!n->data) {
		kfree(n);
		rc = -ENOMEM;
		goto unlock;
	}

	memcpy(&n->tuple, &t, sizeof(n->tuple));

	n->buf_len = min_hlen;
	n->vfid = vf_id;
	n->b_is_drop = drop;
	n->vport_id = vport_id;
	n->sw_id = f->cookie;
	n->tuple.build_hdr(edev, &n->tuple, n->data);

	rc = qede_enqueue_fltr_and_config_searcher(edev, n, 0, true);
	if (rc)
		goto unlock;

	qede_configure_arfs_fltr(edev, n, n->rxq_id, true);
	rc = qede_poll_arfs_filter_config(edev, n);

unlock:
	__qede_unlock(edev);
	return rc;
}
#endif
