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

#include <rdma/ib_verbs.h>
#include <rdma/iw_cm.h>
#include <linux/iommu.h>
#include <linux/pci.h>
#include <linux/if_vlan.h>
#include "qedr_compat.h"
#include "qedr.h"

#if DEFINE_IB_DEV_OPS
/* This function is copied from kernel */
void ib_set_device_ops(struct ib_device *dev, const struct ib_device_ops *ops)
{
	struct ib_device *dev_ops = dev; /* compat line */

#define SET_DEVICE_OP(ptr, name, ops_name)                                     \
	do {                                                                   \
		if (ops->ops_name)                                             \
			if (!((ptr)->name))				       \
				(ptr)->name = ops->ops_name;                   \
	} while (0)
#ifndef REMOVE_DEVICE_ADD_DEL_GID /* !QEDR_UPSTREAM */
#if DEFINE_ROCE_GID_TABLE
	SET_DEVICE_OP(dev_ops, add_gid, add_gid);
#endif
#endif
	SET_DEVICE_OP(dev_ops, alloc_fmr, alloc_fmr);
#ifdef DEFINE_ALLOC_MR 
	SET_DEVICE_OP(dev_ops, alloc_mr, alloc_mr);
#endif
#ifdef _HAS_MW_SUPPORT
	SET_DEVICE_OP(dev_ops, alloc_mw, alloc_mw);
#endif
	SET_DEVICE_OP(dev_ops, alloc_pd, alloc_pd);
	SET_DEVICE_OP(dev_ops, alloc_ucontext, alloc_ucontext);
#ifdef _HAS_XRC_SUPPORT
	SET_DEVICE_OP(dev_ops, alloc_xrcd, alloc_xrcd);
#endif

	SET_DEVICE_OP(dev_ops, create_ah, create_ah);
	SET_DEVICE_OP(dev_ops, create_cq, create_cq);
	SET_DEVICE_OP(dev_ops, create_qp, create_qp);
	SET_DEVICE_OP(dev_ops, create_srq, create_srq);
	SET_DEVICE_OP(dev_ops, dealloc_fmr, dealloc_fmr);
#ifdef _HAS_MW_SUPPORT
	SET_DEVICE_OP(dev_ops, dealloc_mw, dealloc_mw);
#endif
	SET_DEVICE_OP(dev_ops, dealloc_pd, dealloc_pd);
	SET_DEVICE_OP(dev_ops, dealloc_ucontext, dealloc_ucontext);
#ifdef _HAS_XRC_SUPPORT
	SET_DEVICE_OP(dev_ops, dealloc_xrcd, dealloc_xrcd);
#endif
#ifndef REMOVE_DEVICE_ADD_DEL_GID /* !QEDR_UPSTREAM */
#if DEFINE_ROCE_GID_TABLE
	SET_DEVICE_OP(dev_ops, del_gid, del_gid);
#endif
#endif
	SET_DEVICE_OP(dev_ops, dereg_mr, dereg_mr);
	SET_DEVICE_OP(dev_ops, destroy_ah, destroy_ah);
	SET_DEVICE_OP(dev_ops, destroy_cq, destroy_cq);
	SET_DEVICE_OP(dev_ops, destroy_qp, destroy_qp);
	SET_DEVICE_OP(dev_ops, destroy_srq, destroy_srq);
	
#if DEFINE_GET_DEV_FW_STR /* QEDR_UPSTREAM */
	SET_DEVICE_OP(dev_ops, get_dev_fw_str, get_dev_fw_str);
#endif
	SET_DEVICE_OP(dev_ops, get_dma_mr, get_dma_mr);
	SET_DEVICE_OP(dev_ops, get_link_layer, get_link_layer);
#ifdef DEFINE_GET_NETDEV  
	SET_DEVICE_OP(dev_ops, get_netdev, get_netdev);
#endif
#if DEFINE_PORT_IMMUTABLE /* QEDR_UPSTREAM */
	SET_DEVICE_OP(dev_ops, get_port_immutable, get_port_immutable);
#endif
#ifdef DEFINE_ALLOC_MR 
	SET_DEVICE_OP(dev_ops, map_mr_sg, map_mr_sg);
#endif
	SET_DEVICE_OP(dev_ops, map_phys_fmr, map_phys_fmr);
	SET_DEVICE_OP(dev_ops, mmap, mmap);
	SET_DEVICE_OP(dev_ops, modify_ah, modify_ah);
	SET_DEVICE_OP(dev_ops, modify_port, modify_port);
	SET_DEVICE_OP(dev_ops, modify_qp, modify_qp);
	SET_DEVICE_OP(dev_ops, modify_srq, modify_srq);
	SET_DEVICE_OP(dev_ops, peek_cq, peek_cq);
	SET_DEVICE_OP(dev_ops, poll_cq, poll_cq);
	SET_DEVICE_OP(dev_ops, post_recv, post_recv);
	SET_DEVICE_OP(dev_ops, post_send, post_send);
	SET_DEVICE_OP(dev_ops, post_srq_recv, post_srq_recv);
	SET_DEVICE_OP(dev_ops, process_mad, process_mad);
	SET_DEVICE_OP(dev_ops, query_ah, query_ah);
	SET_DEVICE_OP(dev_ops, query_device, query_device);
	SET_DEVICE_OP(dev_ops, query_gid, query_gid);
	SET_DEVICE_OP(dev_ops, query_pkey, query_pkey);
	SET_DEVICE_OP(dev_ops, query_port, query_port);
	SET_DEVICE_OP(dev_ops, query_qp, query_qp);
	SET_DEVICE_OP(dev_ops, query_srq, query_srq);
	SET_DEVICE_OP(dev_ops, reg_user_mr, reg_user_mr);
	SET_DEVICE_OP(dev_ops, req_ncomp_notif, req_ncomp_notif);
	SET_DEVICE_OP(dev_ops, req_notify_cq, req_notify_cq);
	SET_DEVICE_OP(dev_ops, resize_cq, resize_cq);
	SET_DEVICE_OP(dev_ops, unmap_fmr, unmap_fmr);
	if (dev_ops->iwcm == NULL)
		return;
	SET_DEVICE_OP(dev_ops->iwcm, add_ref, iw_add_ref);
	SET_DEVICE_OP(dev_ops->iwcm, rem_ref, iw_rem_ref);
	SET_DEVICE_OP(dev_ops->iwcm, get_qp, iw_get_qp);
	SET_DEVICE_OP(dev_ops->iwcm, connect, iw_connect);
	SET_DEVICE_OP(dev_ops->iwcm, accept, iw_accept);
	SET_DEVICE_OP(dev_ops->iwcm, reject, iw_reject);
	SET_DEVICE_OP(dev_ops->iwcm, create_listen, iw_create_listen);
	SET_DEVICE_OP(dev_ops->iwcm, destroy_listen, iw_destroy_listen);
}
#endif

#if DEFINE_ROCE_GID_TABLE /* !QEDR_UPSTREAM */
#ifndef DEFINED_NETDEV_WALK_LOWER
#ifndef DEFINED_NETDEV_ADJACENT
struct netdev_adjacent {
	struct net_device *dev;

	/* upper master flag, there can only be one master device per list */
	bool master;

	/* counter for the number of times this device was added to us */
	u16 ref_nr;

	/* private field for the users */
	void *private;

	struct list_head list;
	struct rcu_head rcu;
};
#endif

static struct net_device *netdev_next_lower_dev_rcu(struct net_device *dev,
						    struct list_head **iter)
{
	struct netdev_adjacent *lower;

	lower = list_entry_rcu((*iter)->next, struct netdev_adjacent, list);
	if (&lower->list == &dev->adj_list.lower)
		return NULL;

	*iter = &lower->list;

	return lower->dev;
}

static int netdev_walk_all_lower_dev_rcu(struct net_device *dev,
				  int (*fn)(struct net_device *lower_dev,
					    void *data),
				  void *data)
{
	struct net_device *ldev;
	struct list_head *iter;
	int ret;

	for (iter = &dev->adj_list.lower,
	     ldev = netdev_next_lower_dev_rcu(dev, &iter);
	     ldev;
	     ldev = netdev_next_lower_dev_rcu(dev, &iter)) {
		/* first is the lower device itself */
		ret = fn(ldev, data);
		if (ret)
			return ret;

		/* then look at all of its lower devices */
		ret = netdev_walk_all_lower_dev_rcu(ldev, fn, data);
		if (ret)
			return ret;
	}

	return 0;
}
#endif

#ifndef DEFINED_GET_L2_FIELDS
static int get_lower_dev_vlan(struct net_device *lower_dev, void *data)
{
	u16 *vlan_id = data;

	if (is_vlan_dev(lower_dev))
		*vlan_id = vlan_dev_vlan_id(lower_dev);
	/* We are interested only in first level vlan device, so
	 * always return 1 to stop iterating over next level devices.
	 */
	return 1;
}

int rdma_read_gid_l2_fields(const struct ib_gid_attr *attr,
			   u16 *vlan_id, u8 *smac)
{
	struct net_device *ndev;

	ndev = attr->ndev;
	if (!ndev)
		return -ENODEV;

	if (smac)
		ether_addr_copy(smac, ndev->dev_addr);
	if (vlan_id) {
		*vlan_id = 0xffff;
		if (is_vlan_dev(ndev)) {
			*vlan_id = vlan_dev_vlan_id(ndev);
		} else {
			/* If the netdev is upper device and if it's
			 * lower device is vlan device, consider vlan
			 * id of the the lower vlan device for this
			 * gid entry.
			 */
			rcu_read_lock();
			netdev_walk_all_lower_dev_rcu(attr->ndev,
					get_lower_dev_vlan, vlan_id);
			rcu_read_unlock();
		}
	}

	return 0;
}
#endif
#endif
