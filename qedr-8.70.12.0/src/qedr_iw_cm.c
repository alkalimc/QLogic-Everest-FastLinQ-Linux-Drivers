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

#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
#include <linux/dma-mapping.h>
#include <linux/crc32.h>
#include <linux/iommu.h>
#include <linux/inetdevice.h>
#endif

#include <net/ip.h>
#include <net/ipv6.h>
#include <net/udp.h>
#include <net/addrconf.h>
#include <net/route.h>
#include <net/ip6_route.h>
#include <net/flow.h>
#include "qedr.h"
#include "qedr_iw_cm.h"
#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
#include "qedr_compat.h"
#endif

static inline void
qedr_fill_sockaddr4(const struct qed_iwarp_cm_info *cm_info,
		    struct iw_cm_event *event)
{
	struct sockaddr_in *laddr = (struct sockaddr_in *)&event->local_addr;
	struct sockaddr_in *raddr = (struct sockaddr_in *)&event->remote_addr;

	laddr->sin_family = AF_INET;
	raddr->sin_family = AF_INET;

	laddr->sin_port = htons(cm_info->local_port);
	raddr->sin_port = htons(cm_info->remote_port);

	laddr->sin_addr.s_addr = htonl(cm_info->local_ip[0]);
	raddr->sin_addr.s_addr = htonl(cm_info->remote_ip[0]);
}

static inline void
qedr_fill_sockaddr6(const struct qed_iwarp_cm_info *cm_info,
		    struct iw_cm_event *event)
{
	struct sockaddr_in6 *laddr6 = (struct sockaddr_in6 *)&event->local_addr;
	struct sockaddr_in6 *raddr6 =
		(struct sockaddr_in6 *)&event->remote_addr;
	int i;

	laddr6->sin6_family = AF_INET6;
	raddr6->sin6_family = AF_INET6;

	laddr6->sin6_port = htons(cm_info->local_port);
	raddr6->sin6_port = htons(cm_info->remote_port);

	for (i = 0; i < 4; i++) {
		laddr6->sin6_addr.in6_u.u6_addr32[i] =
			htonl(cm_info->local_ip[i]);
		raddr6->sin6_addr.in6_u.u6_addr32[i] =
			htonl(cm_info->remote_ip[i]);
	}
}

static void qedr_iw_free_qp(struct kref *ref)
{
	struct qedr_qp *qp = container_of(ref, struct qedr_qp, refcnt);

#ifdef _HAS_QP_ALLOCATION
	complete(&qp->qp_rel_comp);
#else
	kfree(qp);
#endif
}

static void
qedr_iw_free_ep(struct kref *ref)
{
	struct qedr_iw_ep *ep = container_of(ref, struct qedr_iw_ep, refcnt);

	if (ep->qp)
		kref_put(&ep->qp->refcnt, qedr_iw_free_qp);
	if (ep->cm_id)
		ep->cm_id->rem_ref(ep->cm_id);

	qedr_recov_obj_del(ep->dev, &ep->recov_info);
	kfree(ep);
}

static void
qedr_iw_mpa_request(void *context, struct qed_iwarp_cm_event_params *params)
{
	struct qedr_iw_listener *listener = (struct qedr_iw_listener *)context;
	struct qedr_dev *dev = listener->dev;
	struct iw_cm_event event;
	struct qedr_iw_ep *ep;

	ep = kzalloc(sizeof(*ep), GFP_ATOMIC);
	if (!ep)
		return;

	ep->dev = dev;
	ep->qed_context = params->ep_context;
	kref_init(&ep->refcnt);

	memset(&event, 0, sizeof(event));
	event.event = IW_CM_EVENT_CONNECT_REQUEST;
	event.status = params->status;

	if (!IS_ENABLED(CONFIG_IPV6) ||
	    params->cm_info->ip_version == QED_TCP_IPV4)
		qedr_fill_sockaddr4(params->cm_info, &event);
	else
		qedr_fill_sockaddr6(params->cm_info, &event);

	event.provider_data = (void *)ep;
	event.private_data = (void *)params->cm_info->private_data;
	event.private_data_len = (u8)params->cm_info->private_data_len;
#ifdef DEFINE_PROPOGATE_ORD_IRD /* QEDR_UPSTREAM */
	event.ord = params->cm_info->ord;
	event.ird = params->cm_info->ird;
#endif

	qedr_recov_obj_add(dev, &ep->recov_info);
	listener->cm_id->event_handler(listener->cm_id, &event);
}

#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
static void
qedr_iw_print_event(struct qedr_iw_ep *ep,
		    struct iw_cm_event *event)
{
	struct qedr_dev *dev = ep->dev;

	if (!ep->cm_id)
		return;

	DP_VERBOSE(dev, QEDR_MSG_IWARP, "event=[%d]\n", event->event);
	DP_VERBOSE(dev, QEDR_MSG_IWARP, "status=[%d]\n", event->status);

	DP_VERBOSE(dev, QEDR_MSG_IWARP, "private_data_len=[%d]\n",
		   event->private_data_len);
#ifdef DEFINE_PROPOGATE_ORD_IRD
	DP_VERBOSE(dev, QEDR_MSG_IWARP, "ord=[%d]\n", event->ord);
	DP_VERBOSE(dev, QEDR_MSG_IWARP, "ird=[%d]\n", event->ird);
#endif
}
#endif

static void
qedr_iw_issue_event(void *context,
		    struct qed_iwarp_cm_event_params *params,
		    enum iw_cm_event_type event_type)
{
	struct qedr_iw_ep *ep = (struct qedr_iw_ep *)context;
	struct iw_cm_event event;

	memset(&event, 0, sizeof(event));
	event.status = params->status;
	event.event = event_type;

	if (params->cm_info) {
#ifdef DEFINE_PROPOGATE_ORD_IRD /* QEDR_UPSTREAM */
		event.ird = params->cm_info->ird;
		event.ord = params->cm_info->ord;
#endif
		/* Only connect_request and reply have valid private data
		 * the rest of the events this may be left overs from
		 * connection establishment. CONNECT_REQUEST is issued via
		 * qedr_iw_mpa_request
		 */
		if (event_type == IW_CM_EVENT_CONNECT_REPLY) {
			event.private_data_len =
				params->cm_info->private_data_len;
			event.private_data =
				(void *)params->cm_info->private_data;
		}
	}

	if (ep->cm_id)
		ep->cm_id->event_handler(ep->cm_id, &event);
#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
	qedr_iw_print_event(ep, &event);
#endif
}

static void
qedr_iw_close_event(void *context, struct qed_iwarp_cm_event_params *params)
{
	struct qedr_iw_ep *ep = (struct qedr_iw_ep *)context;

	if (ep->cm_id)
		qedr_iw_issue_event(context, params, IW_CM_EVENT_CLOSE);

	kref_put(&ep->refcnt, qedr_iw_free_ep);
}

static void
qedr_iw_qp_event(void *context,
		 struct qed_iwarp_cm_event_params *params,
		 enum ib_event_type ib_event, char *str)
{
	struct qedr_iw_ep *ep = (struct qedr_iw_ep *)context;
	struct qedr_dev *dev = ep->dev;
	struct ib_qp *ibqp = &(ep->qp->ibqp);
	struct ib_event event;

	DP_DEBUG(dev, QEDR_MSG_IWARP, "Got %s\n", str);

	if (ibqp->event_handler) {
		event.event = ib_event;
		event.device = ibqp->device;
		event.element.qp = ibqp;
		ibqp->event_handler(&event, ibqp->qp_context);
	}
}

struct qedr_discon_work {
	struct work_struct		work;
	struct qedr_iw_ep		*ep;
	enum qed_iwarp_event_type	event;
	int				status;
};

static void qedr_iw_disconnect_worker(struct work_struct *work)
{
	struct qedr_discon_work *dwork =
		container_of(work, struct qedr_discon_work, work);
	struct qed_rdma_modify_qp_in_params qp_params = { 0 };
	struct qedr_iw_ep *ep = dwork->ep;
	struct qedr_dev *dev = ep->dev;
	struct qedr_qp *qp = ep->qp;
	struct iw_cm_event event;

	/* The qp won't be released until we release the ep.
	 * the ep's refcnt was increased before calling this
	 * function, therefore it is safe to access qp
	 */
	if (test_and_set_bit(QEDR_IWARP_CM_WAIT_FOR_DISCONNECT,
			     &qp->iwarp_cm_flags))
		goto out;

	memset(&event, 0, sizeof(event));
	event.status = dwork->status;
	event.event = IW_CM_EVENT_DISCONNECT;

	/* Success means graceful disconnect was requested. modifying
	 * to SQD is translated to graceful disconnect. O/w reset is sent
	 */
	if (dwork->status)
		qp_params.new_state = QED_ROCE_QP_STATE_ERR;
	else
		qp_params.new_state = QED_ROCE_QP_STATE_SQD;

	if (ep->cm_id)
		ep->cm_id->event_handler(ep->cm_id, &event);

	SET_FIELD(qp_params.modify_flags,
		  QED_RDMA_MODIFY_QP_VALID_NEW_STATE, 1);

	dev->ops->rdma_modify_qp(dev->rdma_ctx, qp->qed_qp, &qp_params);
	complete(&ep->qp->iwarp_cm_comp);
out:
	kfree(dwork);
	kref_put(&ep->refcnt, qedr_iw_free_ep);
}

static void
qedr_iw_disconnect_event(void *context,
			 struct qed_iwarp_cm_event_params *params)
{
	struct qedr_discon_work *work;
	struct qedr_iw_ep *ep = (struct qedr_iw_ep *)context;
	struct qedr_dev *dev = ep->dev;

	work = kzalloc(sizeof(*work), GFP_ATOMIC);
	if (!work)
		return;

	/* We can't get a close event before disconnect, but since
	 * we're scheduling a work queue we need to make sure close
	 * won't delete the ep, so we increase the refcnt
	 */
	kref_get(&ep->refcnt);

	work->ep = ep;
	work->event = params->event;
	work->status = params->status;

	INIT_WORK(&work->work, qedr_iw_disconnect_worker);
	queue_work(dev->iwarp_wq, &work->work);
}

static void
qedr_iw_passive_complete(void *context,
			 struct qed_iwarp_cm_event_params *params)
{
	struct qedr_iw_ep *ep = (struct qedr_iw_ep *)context;
	struct qedr_dev *dev = ep->dev;

	/* We will only reach the following state if MPA_REJECT was called on
	 * passive. In this case there will be no associated QP.
	 */
	if ((params->status == -ECONNREFUSED) && (!ep->qp)) {
		DP_DEBUG(dev, QEDR_MSG_IWARP,
			 "PASSIVE connection refused releasing ep...\n");
		kref_put(&ep->refcnt, qedr_iw_free_ep);
		return;
	}

	complete(&ep->qp->iwarp_cm_comp);

	/* INTERNAL: We always issue an established event, however, ofed does not look
	 * at event code for established. So if there was a failure, we follow
	 * with close...
	 */
	qedr_iw_issue_event(context, params, IW_CM_EVENT_ESTABLISHED);

	if (params->status < 0)
		qedr_iw_close_event(context, params);
}

static void
qedr_iw_active_complete(void *context,
			struct qed_iwarp_cm_event_params *params)
{
	struct qedr_iw_ep *ep = (struct qedr_iw_ep *)context;

	complete(&ep->qp->iwarp_cm_comp);
	qedr_iw_issue_event(context, params, IW_CM_EVENT_CONNECT_REPLY);

	if (params->status < 0)
		kref_put(&ep->refcnt, qedr_iw_free_ep);
}

/* INTERNAL: no sense in notifying upper layer of this event, simply send rtr and notify
 * once mpa negotiatin is complete
 */
static int
qedr_iw_mpa_reply(void *context, struct qed_iwarp_cm_event_params *params)
{
	struct qedr_iw_ep *ep = (struct qedr_iw_ep *)context;
	struct qedr_dev *dev = ep->dev;
	struct qed_iwarp_send_rtr_in rtr_in;

	rtr_in.ep_context = params->ep_context;

	return dev->ops->iwarp_send_rtr(dev->rdma_ctx, &rtr_in);
}

static int
qedr_iw_event_handler(void *context, struct qed_iwarp_cm_event_params *params)
{
	struct qedr_iw_ep *ep = (struct qedr_iw_ep *)context;
	struct qedr_dev *dev = ep->dev;

	switch (params->event) {
	case QED_IWARP_EVENT_MPA_REQUEST: /* INTERNAL: Passive side request received */
		qedr_iw_mpa_request(context, params);
		break;
	case QED_IWARP_EVENT_ACTIVE_MPA_REPLY:
		qedr_iw_mpa_reply(context, params);
		break;
	case QED_IWARP_EVENT_PASSIVE_COMPLETE: /* INTERNAL: Passive side established ( ack on mpa response ) */
		qedr_iw_passive_complete(context, params);
		break;
	/* INTERNAL: Active side reply received */
	case QED_IWARP_EVENT_ACTIVE_COMPLETE:
		qedr_iw_active_complete(context, params);
		break;
	case QED_IWARP_EVENT_DISCONNECT:
		qedr_iw_disconnect_event(context, params);
		break;
	case QED_IWARP_EVENT_CLOSE:
		qedr_iw_close_event(context, params);
		break;
	case QED_IWARP_EVENT_RQ_EMPTY:
		qedr_iw_qp_event(context, params, IB_EVENT_QP_FATAL,
				 "QED_IWARP_EVENT_RQ_EMPTY");
		break;
	case QED_IWARP_EVENT_IRQ_FULL:
		qedr_iw_qp_event(context, params, IB_EVENT_QP_FATAL,
				 "QED_IWARP_EVENT_IRQ_FULL");
		break;
	case QED_IWARP_EVENT_LLP_TIMEOUT:
		qedr_iw_qp_event(context, params, IB_EVENT_QP_FATAL,
				 "QED_IWARP_EVENT_LLP_TIMEOUT");
		break;
	case QED_IWARP_EVENT_REMOTE_PROTECTION_ERROR:
		qedr_iw_qp_event(context, params, IB_EVENT_QP_ACCESS_ERR,
				 "QED_IWARP_EVENT_REMOTE_PROTECTION_ERROR");
		break;
	case QED_IWARP_EVENT_CQ_OVERFLOW:
		qedr_iw_qp_event(context, params, IB_EVENT_QP_FATAL,
				 "QED_IWARP_EVENT_CQ_OVERFLOW");
		break;
	case QED_IWARP_EVENT_QP_CATASTROPHIC:
		qedr_iw_qp_event(context, params, IB_EVENT_QP_FATAL,
				 "QED_IWARP_EVENT_QP_CATASTROPHIC");
		break;
	case QED_IWARP_EVENT_LOCAL_ACCESS_ERROR:
		qedr_iw_qp_event(context, params, IB_EVENT_QP_ACCESS_ERR,
				 "QED_IWARP_EVENT_LOCAL_ACCESS_ERROR");
		break;
	case QED_IWARP_EVENT_REMOTE_OPERATION_ERROR:
		qedr_iw_qp_event(context, params, IB_EVENT_QP_FATAL,
				 "QED_IWARP_EVENT_REMOTE_OPERATION_ERROR");
		break;
	case QED_IWARP_EVENT_TERMINATE_RECEIVED:
		DP_NOTICE(dev, "Got terminate message\n");
		break;
	default:
		DP_NOTICE(dev, "Unknown event received %d\n", params->event);
		break;
	}
	return 0;
}

static u16 qedr_iw_get_vlan_ipv4(struct qedr_dev *dev, u32 *addr)
{
	struct net_device *ndev;
	u16 vlan_id = 0;

	ndev = ip_dev_find(&init_net, htonl(addr[0]));

	if (ndev) {
		vlan_id = rdma_vlan_dev_vlan_id(ndev);
		dev_put(ndev);
	}
	if (vlan_id == 0xffff)
		vlan_id = 0;
	return vlan_id;
}

#ifdef QEDR_IWARP_IPV6_SUPPORTED /* QEDR_UPSTREAM */
static u16 qedr_iw_get_vlan_ipv6(u32 *addr)
{
	struct net_device *ndev = NULL;
	struct in6_addr laddr6;
	u16 vlan_id = 0;
	int i;

	if (!IS_ENABLED(CONFIG_IPV6))
		return vlan_id;

	for (i = 0; i < 4; i++)
		laddr6.in6_u.u6_addr32[i] = htonl(addr[i]);

#ifdef DEFINE_NETDEV_RCU /* QEDR_UPSTREAM */
	rcu_read_lock();
	for_each_netdev_rcu(&init_net, ndev) {
#else
	read_lock(&dev_base_lock);
	rcu_read_lock();
	for_each_netdev(&init_net, ndev) {
#endif
		if (ipv6_chk_addr(&init_net, &laddr6, ndev, 1)) {
			vlan_id = rdma_vlan_dev_vlan_id(ndev);
			break;
		}
	}

#ifdef DEFINE_NETDEV_RCU /* QEDR_UPSTREAM */
	rcu_read_unlock();
#else
	rcu_read_unlock();
	read_unlock(&dev_base_lock);
#endif
	if (vlan_id == 0xffff)
		vlan_id = 0;

	return vlan_id;
}
#endif

static bool qedr_ipv4_is_loopback(u32 loc_addr, u32 rem_addr)
{
	return ipv4_is_loopback(htonl(rem_addr)) || (loc_addr == rem_addr);
}

static inline void qedr_copy_ip_htonl(__be32 *dst, u32 *src)
{
	*dst++ = htonl(*src++);
	*dst++ = htonl(*src++);
	*dst++ = htonl(*src++);
	*dst = htonl(*src);
}

#ifdef QEDR_IWARP_IPV6_SUPPORTED /* QEDR_UPSTREAM */
static bool qedr_ipv6_is_loopback(u32 *loc_addr, u32 *rem_addr)
{
	struct in6_addr raddr6;

	qedr_copy_ip_htonl(raddr6.in6_u.u6_addr32, rem_addr);
	return !memcmp(loc_addr, rem_addr, sizeof(struct in6_addr)) ||
	       ipv6_addr_loopback(&raddr6);
}
#endif

static int
qedr_addr4_resolve(struct qedr_dev *dev,
		   struct sockaddr_in *src_in,
		   struct sockaddr_in *dst_in, u8 *dst_mac)
{
	__be32 src_ip = src_in->sin_addr.s_addr;
	__be32 dst_ip = dst_in->sin_addr.s_addr;
	struct neighbour *neigh = NULL;
	struct rtable *rt = NULL;
#ifndef DEFINE_IP_ROUTE_OUTPUT /* !QEDR_UPSTREAM */
	struct flowi fl;
#endif
	int rc = 0;

	DP_DEBUG(dev, QEDR_MSG_IWARP, "src_ip=%pI4 dst_ip=%pI4\n",
		 &src_ip, &dst_ip);

#ifdef DEFINE_IP_ROUTE_OUTPUT /* QEDR_UPSTREAM */
	rt = ip_route_output(&init_net, dst_ip, src_ip, 0, 0);
	if (IS_ERR(rt)) {
		DP_ERR(dev, "ip_route_output returned error\n");
		return -EINVAL;
	}

#ifdef DEFINED_DST_NEIGH_LOOKUP /* QEDR_UPSTREAM */
	neigh = dst_neigh_lookup(&rt->dst, &dst_ip);
#else
	DP_ERR(dev, "NO NEIGHBOR LOOKUP SUPPORT\n");
	return -EINVAL;
#endif
#else
	fl.oif = 0;
	fl.nl_u.ip4_u.daddr = dst_ip;
	fl.nl_u.ip4_u.saddr = src_ip;
	fl.nl_u.ip4_u.tos = 0; /* FIXME */
	fl.proto = IPPROTO_TCP;
	fl.uli_u.ports.sport = src_in->sin_port;
	fl.uli_u.ports.dport = dst_in->sin_port;

	if (ip_route_output_flow(&init_net, &rt, &fl, NULL, 0))
		return -EINVAL;
#ifdef DEFINE_RTABLE_DST_FLAT
	neigh = rt->dst.neighbour;
#else
	neigh = rt->u.dst.neighbour;
#endif
#endif

	if (neigh) {
		rcu_read_lock();
		if (neigh->nud_state & NUD_VALID) {
			ether_addr_copy(dst_mac, neigh->ha);
			DP_DEBUG(dev, QEDR_MSG_QP, "mac_addr=[%pM]\n", dst_mac);
		} else {
			neigh_event_send(neigh, NULL);
		}
		rcu_read_unlock();
#ifdef DEFINED_DST_NEIGH_LOOKUP /* QEDR_UPSTREAM */
		neigh_release(neigh);
#endif
	}

	ip_rt_put(rt);

	return rc;
}

#ifdef QEDR_IWARP_IPV6_SUPPORTED /* QEDR_UPSTREAM */
static int
qedr_addr6_resolve(struct qedr_dev *dev,
		   struct sockaddr_in6 *src_in,
		   struct sockaddr_in6 *dst_in, u8 *dst_mac)

{
	struct neighbour *neigh = NULL;
	struct dst_entry *dst;
#ifdef DEFINE_IP6_FLOWI6 /* QEDR_UPSTREAM */
	struct flowi6 fl6;
#else
	struct flowi fl6;
#endif
	int rc = 0;

	memset(&fl6, 0, sizeof(fl6));
#ifdef DEFINE_IP6_FLOWI6 /* QEDR_UPSTREAM */
	fl6.daddr = dst_in->sin6_addr;
	fl6.saddr = src_in->sin6_addr;
#else
	memcpy(&fl6.nl_u.ip6_u.daddr, dst_in->sin6_addr.s6_addr, 16);
	memcpy(&fl6.nl_u.ip6_u.saddr, src_in->sin6_addr.s6_addr, 16);
#endif

	dst = ip6_route_output(dev_net(dev->ndev), NULL, &fl6);

	if ((!dst) || dst->error) {
		if (dst) {
			dst_release(dst);
			DP_ERR(dev,
			       "ip6_route_output returned dst->error = %d\n",
			       dst->error);
		}
		return -EINVAL;
	}

#ifdef DEFINED_DST_NEIGH_LOOKUP /* QEDR_UPSTREAM */
	neigh = dst_neigh_lookup(dst, &fl6.daddr);
#else
	neigh = dst->neighbour;
#endif

	if (neigh) {
		rcu_read_lock();
		if (neigh->nud_state & NUD_VALID) {
			ether_addr_copy(dst_mac, neigh->ha);
			DP_DEBUG(dev, QEDR_MSG_QP, "mac_addr=[%pM]\n", dst_mac);
		} else {
			neigh_event_send(neigh, NULL);
		}
		rcu_read_unlock();
#ifdef DEFINED_DST_NEIGH_LOOKUP /* QEDR_UPSTREAM */
		neigh_release(neigh);
#endif
	}

	dst_release(dst);

	return rc;
}
#endif

static struct qedr_qp *qedr_iw_load_qp(struct qedr_dev *dev, u32 qpn)
{
	struct qedr_qp *qp;

	spin_lock_irq(&dev->qpidr.idr_lock);
	qp = idr_find(&dev->qpidr.idr, qpn);

	if (qp)
		kref_get(&qp->refcnt);
	spin_unlock_irq(&dev->qpidr.idr_lock);

	return qp;
}

int qedr_iw_connect(struct iw_cm_id *cm_id, struct iw_cm_conn_param *conn_param)
{
	struct qedr_dev *dev = get_qedr_dev(cm_id->device);
	struct qed_iwarp_connect_out out_params;
	struct qed_iwarp_connect_in in_params;
	struct qed_iwarp_cm_info *cm_info;
#ifdef QEDR_IWARP_IPV6_SUPPORTED /* QEDR_UPSTREAM */
	struct sockaddr_in6 *laddr6;
	struct sockaddr_in6 *raddr6;
	int i;
#endif
	struct sockaddr_in *laddr;
	struct sockaddr_in *raddr;
	struct qedr_iw_ep *ep;
	struct qedr_qp *qp;
	int rc = 0;

	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
		return rc;

#if DEFINE_PORT_MAPPER /* QEDR_UPSTREAM */
	laddr = (struct sockaddr_in *)&cm_id->m_local_addr;
	raddr = (struct sockaddr_in *)&cm_id->m_remote_addr;

#ifdef QEDR_IWARP_IPV6_SUPPORTED /* QEDR_UPSTREAM */
	laddr6 = (struct sockaddr_in6 *)&cm_id->m_local_addr;
	raddr6 = (struct sockaddr_in6 *)&cm_id->m_remote_addr;
#endif

	DP_DEBUG(dev, QEDR_MSG_IWARP, "MAPPED %d %d\n",
		 ntohs(((struct sockaddr_in *)&cm_id->remote_addr)->sin_port),
		 ntohs(raddr->sin_port));
#else
	laddr = (struct sockaddr_in *)&cm_id->local_addr;
	raddr = (struct sockaddr_in *)&cm_id->remote_addr;
#ifdef QEDR_IWARP_IPV6_SUPPORTED
	laddr6 = (struct sockaddr_in6 *)&cm_id->local_addr;
	raddr6 = (struct sockaddr_in6 *)&cm_id->remote_addr;
#endif
#endif
	DP_DEBUG(dev, QEDR_MSG_IWARP,
		 "Connect source address: %pISpc, remote address: %pISpc\n",
		 &cm_id->local_addr, &cm_id->remote_addr);

	if (!laddr->sin_port || !raddr->sin_port)
		return -EINVAL;

	ep = kzalloc(sizeof(*ep), GFP_KERNEL);
	if (!ep)
		return -ENOMEM;

	ep->dev = dev;
	kref_init(&ep->refcnt);

	qp = qedr_iw_load_qp(dev, conn_param->qpn);
	if (!qp || qp->recov_info.reset) {
		rc = -EINVAL;
		goto err;
	}

	ep->qp = qp;
	cm_id->add_ref(cm_id);
	ep->cm_id = cm_id;

	in_params.event_cb = qedr_iw_event_handler;
	in_params.cb_context = ep;

	cm_info = &in_params.cm_info;
	memset(cm_info->local_ip, 0, sizeof(cm_info->local_ip));
	memset(cm_info->remote_ip, 0, sizeof(cm_info->remote_ip));

#ifdef QEDR_IWARP_IPV6_SUPPORTED /* QEDR_UPSTREAM */
	if (!IS_ENABLED(CONFIG_IPV6) ||
	    cm_id->remote_addr.ss_family == AF_INET) {
#endif
		cm_info->ip_version = QED_TCP_IPV4;

		cm_info->remote_ip[0] = ntohl(raddr->sin_addr.s_addr);
		cm_info->local_ip[0] = ntohl(laddr->sin_addr.s_addr);
		cm_info->remote_port = ntohs(raddr->sin_port);
		cm_info->local_port = ntohs(laddr->sin_port);
		cm_info->vlan = qedr_iw_get_vlan_ipv4(dev, cm_info->local_ip);

		/* if loopback copy local mac as remote mac */
		if (qedr_ipv4_is_loopback(cm_info->remote_ip[0], cm_info->local_ip[0]))
			memcpy(in_params.remote_mac_addr, dev->ndev->dev_addr, ETH_ALEN);
		else
			rc = qedr_addr4_resolve(dev, laddr, raddr,
					(u8 *)in_params.remote_mac_addr);

		in_params.mss = dev->iwarp_max_mtu -
			(sizeof(struct iphdr) + sizeof(struct tcphdr));

#ifdef QEDR_IWARP_IPV6_SUPPORTED /* QEDR_UPSTREAM */
	} else {
		in_params.cm_info.ip_version = QED_TCP_IPV6;

		for (i = 0; i < 4; i++) {
			cm_info->remote_ip[i] =
			    ntohl(raddr6->sin6_addr.in6_u.u6_addr32[i]);
			cm_info->local_ip[i] =
			    ntohl(laddr6->sin6_addr.in6_u.u6_addr32[i]);
		}

		cm_info->local_port = ntohs(laddr6->sin6_port);
		cm_info->remote_port = ntohs(raddr6->sin6_port);

		in_params.mss = dev->iwarp_max_mtu -
			(sizeof(struct ipv6hdr) + sizeof(struct tcphdr));

		cm_info->vlan = qedr_iw_get_vlan_ipv6(cm_info->local_ip);

		/* if loopback copy local mac as remote mac */
		if (qedr_ipv6_is_loopback(cm_info->local_ip, cm_info->remote_ip))
			memcpy(in_params.remote_mac_addr, dev->ndev->dev_addr, ETH_ALEN);
		else
			rc = qedr_addr6_resolve(dev, laddr6, raddr6,
					(u8 *)in_params.remote_mac_addr);
	}
#endif
	if (rc)
		goto err;

	DP_DEBUG(dev, QEDR_MSG_IWARP,
		 "ord = %d ird=%d private_data=%p private_data_len=%d rq_psn=%d\n",
		 conn_param->ord, conn_param->ird, conn_param->private_data,
		 conn_param->private_data_len, qp->rq_psn);

	cm_info->ord = conn_param->ord;
	cm_info->ird = conn_param->ird;
	cm_info->private_data = conn_param->private_data;
	cm_info->private_data_len = conn_param->private_data_len;
	in_params.qp = qp->qed_qp;
	memcpy(in_params.local_mac_addr, dev->ndev->dev_addr, ETH_ALEN);

	if (test_and_set_bit(QEDR_IWARP_CM_WAIT_FOR_CONNECT,
			     &qp->iwarp_cm_flags))
		goto err; /* QP already being destroyed */

	rc = dev->ops->iwarp_connect(dev->rdma_ctx, &in_params, &out_params);

	if (rc) {
		complete(&qp->iwarp_cm_comp);
		goto err;
	}

	qedr_recov_obj_add(dev, &ep->recov_info);

	return rc;
err:
	kref_put(&ep->refcnt, qedr_iw_free_ep);
	return rc;
}

int qedr_iw_create_listen(struct iw_cm_id *cm_id, int backlog)
{
	struct qedr_dev *dev = get_qedr_dev(cm_id->device);
	struct qedr_iw_listener *listener;
	struct qed_iwarp_listen_in iparams;
	struct qed_iwarp_listen_out oparams;
	struct sockaddr_in *laddr;
#ifdef QEDR_IWARP_IPV6_SUPPORTED /* QEDR_UPSTREAM */
	struct sockaddr_in6 *laddr6;
	int i;
#endif
	int rc;

	DP_VERBOSE(dev, QEDR_MSG_IWARP, ">>\n");
	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
		return rc;

#if DEFINE_PORT_MAPPER /* QEDR_UPSTREAM */
	laddr = (struct sockaddr_in *)&cm_id->m_local_addr;
#ifdef QEDR_IWARP_IPV6_SUPPORTED /* QEDR_UPSTREAM */
	laddr6 = (struct sockaddr_in6 *)&cm_id->m_local_addr;
#endif
	DP_DEBUG(dev, QEDR_MSG_IWARP,
		 "Create Listener address: %pISpc\n", &cm_id->m_local_addr);
#else
	laddr = (struct sockaddr_in *)&cm_id->local_addr;
#ifdef QEDR_IWARP_IPV6_SUPPORTED
	laddr6 = (struct sockaddr_in6 *)&cm_id->local_addr;
#endif
	DP_DEBUG(dev, QEDR_MSG_IWARP,
		 "Create Listener address: %pISpc\n", &cm_id->local_addr);
#endif

	listener = kzalloc(sizeof(*listener), GFP_KERNEL);
	if (!listener)
		return -ENOMEM;

	listener->dev = dev;
	cm_id->add_ref(cm_id);
	listener->cm_id = cm_id;
	listener->backlog = backlog;

	iparams.cb_context = listener;
	iparams.event_cb = qedr_iw_event_handler;
	iparams.max_backlog = backlog;

#ifdef QEDR_IWARP_IPV6_SUPPORTED /* QEDR_UPSTREAM */
	if (!IS_ENABLED(CONFIG_IPV6) ||
	    cm_id->local_addr.ss_family == AF_INET) {
#endif
		iparams.ip_version = QED_TCP_IPV4;
		memset(iparams.ip_addr, 0, sizeof(iparams.ip_addr));

		iparams.ip_addr[0] = ntohl(laddr->sin_addr.s_addr);
		iparams.port = ntohs(laddr->sin_port);
		iparams.vlan = qedr_iw_get_vlan_ipv4(dev, iparams.ip_addr);
#ifdef QEDR_IWARP_IPV6_SUPPORTED /* QEDR_UPSTREAM */
	} else {
		iparams.ip_version = QED_TCP_IPV6;

		for (i = 0; i < 4; i++) {
			iparams.ip_addr[i] =
				ntohl(laddr6->sin6_addr.in6_u.u6_addr32[i]);
		}

		iparams.port = ntohs(laddr6->sin6_port);

		iparams.vlan = qedr_iw_get_vlan_ipv6(iparams.ip_addr);
	}
#endif

	memcpy(iparams.mac_addr, dev->ndev->dev_addr, ETH_ALEN);
	rc = dev->ops->iwarp_create_listen(dev->rdma_ctx, &iparams, &oparams);
	if (rc)
		goto err;

	listener->qed_handle = oparams.handle;
	cm_id->provider_data = listener;
	qedr_recov_obj_add(dev, &listener->recov_info);

	return rc;

err:
	cm_id->rem_ref(cm_id);
	kfree(listener);
	return rc;
}

int qedr_iw_destroy_listen(struct iw_cm_id *cm_id)
{
	struct qedr_iw_listener *listener = cm_id->provider_data;
	struct qedr_dev *dev = get_qedr_dev(cm_id->device);
	int rc = 0;

	if (!listener->recov_info.reset && listener->qed_handle)
		rc = dev->ops->iwarp_destroy_listen(dev->rdma_ctx,
						    listener->qed_handle);

	cm_id->rem_ref(cm_id);
	qedr_recov_obj_del(dev, &listener->recov_info);
	kfree(listener);

	return rc;
}

int qedr_iw_accept(struct iw_cm_id *cm_id, struct iw_cm_conn_param *conn_param)
{
	struct qedr_iw_ep *ep = (struct qedr_iw_ep *)cm_id->provider_data;
	struct qedr_dev *dev = ep->dev;
	struct qedr_qp *qp;
	struct qed_iwarp_accept_in params;
	int rc;

	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
		return rc;

	DP_DEBUG(dev, QEDR_MSG_IWARP, "Accept on qpid=%d\n", conn_param->qpn);

	qp = qedr_iw_load_qp(dev, conn_param->qpn);
	if (!qp || qp->recov_info.reset) {
		DP_ERR(dev, "Invalid QP number %d\n", conn_param->qpn);
		return -EINVAL;
	}

	ep->qp = qp;
	cm_id->add_ref(cm_id);
	ep->cm_id = cm_id;

	params.ep_context = ep->qed_context;
	params.cb_context = ep;
	params.qp = ep->qp->qed_qp;
	params.private_data = conn_param->private_data;
	params.private_data_len = conn_param->private_data_len;
	params.ird = conn_param->ird;
	params.ord = conn_param->ord;

	if (test_and_set_bit(QEDR_IWARP_CM_WAIT_FOR_CONNECT,
			     &qp->iwarp_cm_flags)) {
		rc = -EPERM;
		goto err; /* QP already destroyed */
	}

	rc = dev->ops->iwarp_accept(dev->rdma_ctx, &params);
	if (rc) {
		complete(&qp->iwarp_cm_comp);
		goto err;
	}

	return rc;
err:
	kref_put(&ep->refcnt, qedr_iw_free_ep);
	return rc;
}

int qedr_iw_reject(struct iw_cm_id *cm_id, const void *pdata, u8 pdata_len)
{
	struct qedr_iw_ep *ep = (struct qedr_iw_ep *)cm_id->provider_data;
	struct qedr_dev *dev = ep->dev;
	struct qed_iwarp_reject_in params;
	int rc;

	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
		return rc;

	params.ep_context = ep->qed_context;
	params.cb_context = ep;
	params.private_data = pdata;
	params.private_data_len = pdata_len;
	ep->qp = NULL;

	if (!ep->recov_info.reset)
		return dev->ops->iwarp_reject(dev->rdma_ctx, &params);

	return 0;
}

void qedr_iw_qp_add_ref(struct ib_qp *ibqp)
{
	struct qedr_qp *qp = get_qedr_qp(ibqp);

	kref_get(&qp->refcnt);
}

void qedr_iw_qp_rem_ref(struct ib_qp *ibqp)
{
	struct qedr_qp *qp = get_qedr_qp(ibqp);

	kref_put(&qp->refcnt, qedr_iw_free_qp);
}

struct ib_qp *qedr_iw_get_qp(struct ib_device *ibdev, int qpn)
{
	struct qedr_dev *dev = get_qedr_dev(ibdev);

	return idr_find(&dev->qpidr.idr, qpn);
}
