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

#include <linux/dma-mapping.h>
#include <linux/crc32.h>
#include <linux/iommu.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/udp.h>

#include <rdma/ib_verbs.h>
#include <rdma/ib_user_verbs.h>
#include <rdma/iw_cm.h>
#include <rdma/ib_umem.h>
#include <rdma/ib_addr.h>
#include <rdma/ib_cache.h>

#include "common_hsi.h"
#include "qedr_hsi_rdma.h"
#include "qedr.h"
#include "verbs.h"
#include "qedr_user.h"
#include "qedr_roce_cm.h"
#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
#include "qedr_compat.h"
#endif

void qedr_inc_sw_gsi_cons(struct qedr_qp_hwq_info *info)
{
	info->gsi_cons = (info->gsi_cons + 1) % info->max_wr;
}

void qedr_store_gsi_qp_cq(struct qedr_dev *dev, struct qedr_qp *qp,
			  struct ib_qp_init_attr *attrs)
{
	dev->gsi_qp_created = 1;
	dev->gsi_sqcq = get_qedr_cq(attrs->send_cq);
	dev->gsi_rqcq = get_qedr_cq(attrs->recv_cq);
	dev->gsi_qp = qp;
}

static void qedr_ll2_complete_tx_packet(void *cxt, u8 connection_handle,
					void *cookie,
					dma_addr_t first_frag_addr,
					bool b_last_fragment,
					bool b_last_packet)
{
	struct qedr_dev *dev = (struct qedr_dev *)cxt;
	struct qed_roce_ll2_packet *pkt = cookie;
	struct qedr_cq *cq = dev->gsi_sqcq;
	struct qedr_qp *qp = dev->gsi_qp;
	unsigned long flags;

	DP_DEBUG(dev, QEDR_MSG_GSI,
		 "LL2 TX CB: gsi_sqcq=%p, gsi_rqcq=%p, gsi_cons=%d, ibcq_comp=%s\n",
		 dev->gsi_sqcq, dev->gsi_rqcq, qp->sq.gsi_cons,
		 cq->ibcq.comp_handler ? "Yes" : "No");

	dma_free_coherent(&dev->pdev->dev, pkt->header.len, pkt->header.vaddr,
			  pkt->header.baddr);
	kfree(pkt);

	spin_lock_irqsave(&qp->q_lock, flags);
	qedr_inc_sw_gsi_cons(&qp->sq);
	spin_unlock_irqrestore(&qp->q_lock, flags);

	if (cq->ibcq.comp_handler)
		(*cq->ibcq.comp_handler) (&cq->ibcq, cq->ibcq.cq_context);
}

#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
static inline void qedr_print_hex_dump(const char *str, void *buf, size_t size)
{
	print_hex_dump(KERN_INFO, str, DUMP_PREFIX_OFFSET,
		       32 /* row size */, 1 /* group size */, buf,
		       size, false /* w/o ASCII */);
}
#endif

static void qedr_ll2_complete_rx_packet(void *cxt,
					struct qed_ll2_comp_rx_data *data)
{
	struct qedr_dev *dev = (struct qedr_dev *)cxt;
	struct qedr_cq *cq = dev->gsi_rqcq;
	struct qedr_qp *qp = dev->gsi_qp;
	unsigned long flags;

#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
	DP_DEBUG(dev, QEDR_MSG_GSI,
		 "roce ll2 rx complete: bus_addr=%p, len=%d, data_len_err=%d\n",
		 (void *)(uintptr_t)data->rx_buf_addr, data->length.data_length,
		 data->u.data_length_error);

	if (data->u.data_length_error) {
		DP_ERR(dev, "roce ll2 rx complete: data length error %d, length=%d\n",
		       data->u.data_length_error, data->length.data_length);
		/* TODO: add statistic */
	}

#endif
	spin_lock_irqsave(&qp->q_lock, flags);

	qp->rqe_wr_id[qp->rq.gsi_cons].rc = data->u.data_length_error ?
		-EINVAL : 0;
	qp->rqe_wr_id[qp->rq.gsi_cons].vlan = data->vlan;
	/* note: length stands for data length i.e. GRH is excluded */
	qp->rqe_wr_id[qp->rq.gsi_cons].sg_list[0].length =
		data->length.data_length;
	*((u32 *)&qp->rqe_wr_id[qp->rq.gsi_cons].smac[0]) =
		ntohl(data->opaque_data_0);
	*((u16 *)&qp->rqe_wr_id[qp->rq.gsi_cons].smac[4]) =
		ntohs((u16)data->opaque_data_1);

	qedr_inc_sw_gsi_cons(&qp->rq);

	spin_unlock_irqrestore(&qp->q_lock, flags);

	if (cq->ibcq.comp_handler)
		(*cq->ibcq.comp_handler) (&cq->ibcq, cq->ibcq.cq_context);
}

static void qedr_ll2_release_rx_packet(void *cxt, u8 connection_handle,
				       void *cookie, dma_addr_t rx_buf_addr,
				       bool b_last_packet)
{
	/* Do nothing... */
}

static void qedr_destroy_gsi_cq(struct qedr_dev *dev,
				struct ib_qp_init_attr *attrs)
{
	struct qed_rdma_destroy_cq_in_params iparams;
	struct qed_rdma_destroy_cq_out_params oparams;
	struct qedr_cq *cq;

	cq = get_qedr_cq(attrs->send_cq);
	iparams.icid = cq->icid;
	dev->ops->rdma_destroy_cq(dev->rdma_ctx, &iparams, &oparams);
	dev->ops->common->chain_free(dev->cdev, &cq->pbl);

	cq = get_qedr_cq(attrs->recv_cq);
	/* if a dedicated recv_cq was used, delete it too */
	if (iparams.icid != cq->icid) {
		iparams.icid = cq->icid;
		dev->ops->rdma_destroy_cq(dev->rdma_ctx, &iparams, &oparams);
		dev->ops->common->chain_free(dev->cdev, &cq->pbl);
	}
}

static inline int qedr_check_gsi_qp_attrs(struct qedr_dev *dev,
					  struct ib_qp_init_attr *attrs)
{
	if (attrs->cap.max_recv_sge > QEDR_GSI_MAX_RECV_SGE) {
		DP_ERR(dev,
		       " create gsi qp: failed. max_recv_sge is larger the max %d>%d\n",
		       attrs->cap.max_recv_sge, QEDR_GSI_MAX_RECV_SGE);
		return -EINVAL;
	}

	if (attrs->cap.max_recv_wr > QEDR_GSI_MAX_RECV_WR) {
		DP_ERR(dev,
		       " create gsi qp: failed. max_recv_wr is too large %d>%d\n",
		       attrs->cap.max_recv_wr, QEDR_GSI_MAX_RECV_WR);
		return -EINVAL;
	}

	if (attrs->cap.max_send_wr > QEDR_GSI_MAX_SEND_WR) {
		DP_ERR(dev,
		       " create gsi qp: failed. max_send_wr is too large %d>%d\n",
		       attrs->cap.max_send_wr, QEDR_GSI_MAX_SEND_WR);
		return -EINVAL;
	}

	return 0;
}

static int qedr_ll2_post_tx(struct qedr_dev *dev,
			    struct qed_roce_ll2_packet *pkt)
{
	enum qed_ll2_roce_flavor_type roce_flavor;
	struct qed_ll2_tx_pkt_info ll2_tx_pkt;
	int rc;
	int i;

	memset(&ll2_tx_pkt, 0, sizeof(ll2_tx_pkt));

	roce_flavor = (pkt->roce_mode == ROCE_V1) ?
	    QED_LL2_ROCE : QED_LL2_RROCE;

	if (pkt->roce_mode == ROCE_V2_IPV4)
		ll2_tx_pkt.enable_ip_cksum = 1;

	ll2_tx_pkt.num_of_bds = 1 /* hdr */ + pkt->n_seg;
	ll2_tx_pkt.vlan = 0;
	switch (pkt->tx_dest) {
	case QED_ROCE_LL2_TX_DEST_NW:
		ll2_tx_pkt.tx_dest = QED_LL2_TX_DEST_NW;
		break;
	case QED_ROCE_LL2_TX_DEST_LB:
		ll2_tx_pkt.tx_dest = QED_LL2_TX_DEST_LB;
		break;
	default:
		ll2_tx_pkt.tx_dest = QED_LL2_TX_DEST_DROP;
	}
	ll2_tx_pkt.qed_roce_flavor = roce_flavor;
	ll2_tx_pkt.first_frag = pkt->header.baddr;
	ll2_tx_pkt.first_frag_len = pkt->header.len;
	ll2_tx_pkt.cookie = pkt;

	/* tx header */
	rc = dev->ops->ll2_prepare_tx_packet(dev->rdma_ctx,
					     dev->gsi_ll2_handle,
					     &ll2_tx_pkt, 1);
	if (rc) {
		/* TX failed while posting header - release resources */
		dma_free_coherent(&dev->pdev->dev, pkt->header.len,
				  pkt->header.vaddr, pkt->header.baddr);
		kfree(pkt);

		DP_ERR(dev, "roce ll2 tx: header failed (rc=%d)\n", rc);
		return rc;
	}

	/* tx payload */
	for (i = 0; i < pkt->n_seg; i++) {
		rc = dev->ops->ll2_set_fragment_of_tx_packet(
			dev->rdma_ctx,
			dev->gsi_ll2_handle,
			pkt->payload[i].baddr,
			pkt->payload[i].len);

		if (rc) {
			/* if failed not much to do here, partial packet has
			 * been posted we can't free memory, will need to wait
			 * for completion
			 */
			DP_ERR(dev, "ll2 tx: payload failed (rc=%d)\n", rc);
			return rc;
		}
	}

	return 0;
}

int qedr_ll2_stop(struct qedr_dev *dev)
{
	int rc;

	if (dev->gsi_ll2_handle == QED_LL2_UNUSED_HANDLE)
		return 0;

	/* remove LL2 MAC address filter */
	rc = dev->ops->ll2_set_mac_filter(dev->cdev,
					  dev->gsi_ll2_mac_address, NULL);

	rc = dev->ops->ll2_terminate_connection(dev->rdma_ctx,
						dev->gsi_ll2_handle);
	if (rc)
		DP_ERR(dev, "Failed to terminate LL2 connection (rc=%d)\n", rc);

	dev->ops->ll2_release_connection(dev->rdma_ctx, dev->gsi_ll2_handle);

	dev->gsi_ll2_handle = QED_LL2_UNUSED_HANDLE;

	return rc;
}

static int qedr_gsi_repost_rq_buffers(struct qedr_dev *dev)
{
	struct qedr_qp *qp = dev->gsi_qp;
	struct ib_sge *sg_list;
	unsigned long flags;
	int curr;
	int rc;

	spin_lock_irqsave(&qp->q_lock, flags);

	/* We want to repost buffers that weren't processed, therefore we
	 * start from the gsi_cons, buffers between rq.con --> gsi.cons
	 * can still be completed to stack
	 */
	if (qp->rq.gsi_cons == qp->rq.prod)
		return 0;

	/* We re-post the buffers, but don't change the cons / prod. The
	 * rqe_wr_id remains the same, and pointers point to the same location
	 */
	curr = qp->rq.gsi_cons;
	while (curr != qp->rq.prod) {
		sg_list = qp->rqe_wr_id[curr].sg_list;
		rc = dev->ops->ll2_post_rx_buffer(dev->rdma_ctx,
						  dev->gsi_ll2_handle,
						  sg_list->addr,
						  sg_list->length,
						  NULL /* cookie */,
						  1 /* notify_fw */);
		if (rc) {
			DP_ERR(dev,
			       "gsi repost recv: failed to post rx buffer (rc=%d)\n",
			       rc);
		}

		curr = (curr + 1) % qp->rq.max_wr;
	}

	spin_unlock_irqrestore(&qp->q_lock, flags);

	return 0;
}

int qedr_ll2_start(struct qedr_dev *dev, struct qedr_qp *qp)
{
	struct qed_ll2_acquire_data data;
	struct qed_ll2_cbs cbs;
	int rc;

	/* configure and start LL2 */
	cbs.rx_comp_cb = qedr_ll2_complete_rx_packet;
	cbs.tx_comp_cb = qedr_ll2_complete_tx_packet;
	cbs.rx_release_cb = qedr_ll2_release_rx_packet;
	cbs.tx_release_cb = qedr_ll2_complete_tx_packet;
	cbs.cookie = dev;

	memset(&data, 0, sizeof(data));
	data.input.conn_type = QED_LL2_TYPE_ROCE;
	data.input.mtu = dev->ndev->mtu;
	data.input.rx_num_desc = qp->rq.max_wr;
	data.input.rx_drop_ttl0_flg = true;
	data.input.rx_vlan_removal_en = false;
	data.input.tx_num_desc = qp->sq.max_wr;
	data.input.tx_tc = 0;
	data.input.tx_dest = QED_LL2_TX_DEST_NW;
	data.input.ai_err_packet_too_big = QED_LL2_DROP_PACKET;
	data.input.ai_err_no_buf = QED_LL2_DROP_PACKET;
	data.input.gsi_enable = 1;
	data.p_connection_handle = &dev->gsi_ll2_handle;
	data.cbs = &cbs;

	rc = dev->ops->ll2_acquire_connection(dev->rdma_ctx, &data);
	if (rc) {
		DP_ERR(dev,
		       "ll2 start: failed to acquire LL2 connection (rc=%d)\n",
		       rc);
		return rc;
	}

	rc = dev->ops->ll2_establish_connection(dev->rdma_ctx,
						dev->gsi_ll2_handle);
	if (rc) {
		DP_ERR(dev,
		       "ll2 start: failed to establish LL2 connection (rc=%d)\n",
		       rc);
		goto err1;
	}

	rc = dev->ops->ll2_set_mac_filter(dev->cdev, NULL, dev->ndev->dev_addr);
	if (rc)
		goto err2;

	/* If this is the first creation - gsi_qp will be NULL */
	if (dev->gsi_qp)
		qedr_gsi_repost_rq_buffers(dev);

	return 0;

err2:
	dev->ops->ll2_terminate_connection(dev->rdma_ctx, dev->gsi_ll2_handle);
err1:
	dev->ops->ll2_release_connection(dev->rdma_ctx, dev->gsi_ll2_handle);

	return rc;
}

#ifdef _HAS_QP_ALLOCATION
int qedr_create_gsi_qp(struct qedr_dev *dev,
		       struct ib_qp_init_attr *attrs,
		       struct qedr_qp *qp)
#else
struct ib_qp *qedr_create_gsi_qp(struct qedr_dev *dev,
				 struct ib_qp_init_attr *attrs,
				 struct qedr_qp *qp)
#endif
{
	int rc;

	rc = qedr_check_gsi_qp_attrs(dev, attrs);
	if (rc)
#ifdef _HAS_QP_ALLOCATION
		return rc;
#else
		return ERR_PTR(rc);
#endif

	/* We add one to differentiate between full and empty array */
	qp->rq.max_wr = attrs->cap.max_recv_wr + 1;
	qp->sq.max_wr = attrs->cap.max_send_wr + 1;

	rc = qedr_ll2_start(dev, qp);
	if (rc) {
		DP_ERR(dev, "create gsi qp: failed on ll2 start. rc=%d\n", rc);
#ifdef _HAS_QP_ALLOCATION
		return rc;
#else
		return ERR_PTR(rc);
#endif
	}

	/* create QP */
	qp->ibqp.qp_num = 1;

	qp->rqe_wr_id = kcalloc(qp->rq.max_wr, sizeof(*qp->rqe_wr_id),
				GFP_KERNEL);
	if (!qp->rqe_wr_id)
		goto err;
	qp->wqe_wr_id = kcalloc(qp->sq.max_wr, sizeof(*qp->wqe_wr_id),
				GFP_KERNEL);
	if (!qp->wqe_wr_id)
		goto err;

	qedr_store_gsi_qp_cq(dev, qp, attrs);
	ether_addr_copy(dev->gsi_ll2_mac_address, dev->ndev->dev_addr);

	/* the GSI CQ is handled by the driver so remove it from the FW */
	qedr_destroy_gsi_cq(dev, attrs);
	dev->gsi_rqcq->cq_type = QEDR_CQ_TYPE_GSI;
	dev->gsi_sqcq->cq_type = QEDR_CQ_TYPE_GSI;

	DP_DEBUG(dev, QEDR_MSG_GSI, "created GSI QP %p\n", qp);

#ifdef _HAS_QP_ALLOCATION
	return 0;
#else
	return &qp->ibqp;
#endif

err:
	kfree(qp->rqe_wr_id);

	rc = qedr_ll2_stop(dev);
	if (rc)
		DP_ERR(dev, "create gsi qp: failed destroy on create\n");

#ifdef _HAS_QP_ALLOCATION
	return -ENOMEM;
#else
	return ERR_PTR(-ENOMEM);
#endif
}

/* Assumption, destroy_gsi_qp is called from destroy_qp which waits
 * for any recovery attempt to complete before proceeding. Therefore
 * it's safe to assume that (a) we're not in the middle of a recovery
 * process (b) ll2 exists - re-created at the end of the recovery
 */
int qedr_destroy_gsi_qp(struct qedr_dev *dev)
{
	return qedr_ll2_stop(dev);
}

#if !DEFINE_ROCE_GID_TABLE  /* !QEDR_UPSTREAM */
static inline bool qedr_get_vlan_id_gsi(struct rdma_ah_attr *ah_attr,
					u16 *vlan_id)
{
	u16 tmp_vlan_id;
#ifdef DEFINE_NO_IP_BASED_GIDS
	union ib_gid *dgid = &ah_attr->grh.dgid;
#endif

#ifdef DEFINE_NO_IP_BASED_GIDS
	tmp_vlan_id = (dgid->raw[11] << 8) | dgid->raw[12];
#else
	tmp_vlan_id = ah_attr->vlan_id;
#endif

	if (tmp_vlan_id < VLAN_CFI_MASK) {
		*vlan_id = tmp_vlan_id;
		return true;
	} else {
		*vlan_id = 0;
		return false;
	}
}
#endif

#define QEDR_MAX_UD_HEADER_SIZE	(100)
#define QEDR_GSI_QPN		(1)
static inline int qedr_gsi_build_header(struct qedr_dev *dev,
					struct qedr_qp *qp,
					RDMA_CONST struct ib_send_wr *swr,
					struct ib_ud_header *udh,
					int *roce_mode)
{
	bool has_vlan = false, has_grh_ipv6 = true;
	struct rdma_ah_attr *ah_attr = &get_qedr_ah(ud_wr(swr)->ah)->attr;
	const struct ib_global_route *grh = rdma_ah_read_grh(ah_attr);
	u8 mac[ETH_ALEN] __attribute__((aligned(16)));
	u8 vlan_pri, dscp;
	int send_size = 0;
	u16 vlan_id = 0;
	u16 ether_type;
	SGID_CONST union ib_gid *sgid;
	int i;
#if DEFINE_IB_AH_ATTR_WITH_DMAC /* QEDR_UPSTREAM */
	u8 *rdma_ah_dmac;
#endif

#if DEFINE_ROCE_V2_SUPPORT /* QEDR_UPSTREAM */
	int ip_ver = 0;
#endif

#ifdef DEFINE_IB_UD_HEADER_INIT_UDP_PRESENT /* QEDR_UPSTREAM */
	bool has_udp = false;
#endif

#if DEFINE_ROCE_GID_TABLE /* QEDR_UPSTREAM */
	int rc;
#ifdef DEFINED_SGID_ATTR /* QEDR_UPSTREAM */
	const struct ib_gid_attr *sgid_attr = grh->sgid_attr;

	sgid = &sgid_attr->gid;
#else
	struct ib_gid_attr tmp_sgid_attr;
	struct ib_gid_attr *sgid_attr = &tmp_sgid_attr;
	union ib_gid tmp_sgid;

	sgid = &tmp_sgid;
#endif
#endif

	send_size = 0;
	for (i = 0; i < swr->num_sge; ++i)
		send_size += swr->sg_list[i].length;

	ether_addr_copy(mac, dev->ndev->dev_addr);
#if DEFINE_ROCE_GID_TABLE /* QEDR_UPSTREAM */
#ifndef DEFINED_SGID_ATTR /* !QEDR_UPSTREAM */
	rc = ib_get_cached_gid(qp->ibqp.device, rdma_ah_get_port_num(ah_attr),
			       grh->sgid_index, sgid, sgid_attr);
	if (rc) {
		DP_ERR(dev,
		       "gsi post send: failed to get cached GID (port=%d, ix=%d)\n",
		       rdma_ah_get_port_num(ah_attr),
		       grh->sgid_index);
		return rc;
	}
#endif
	if (!sgid_attr->ndev) {
		DP_ERR(dev, "gsi post send: NULL ndev\n");
		return -EINVAL;
	}

	rc = rdma_read_gid_l2_fields(sgid_attr, &vlan_id, mac);
	if (rc)
		return rc;

	if (vlan_id < VLAN_CFI_MASK)
		has_vlan = true;

#ifndef DEFINED_SGID_ATTR /* !QEDR_UPSTREAM */
	dev_put(sgid_attr->ndev);

	if (!memcmp(sgid, &zgid, sizeof(*sgid))) {
		DP_ERR(dev, "gsi post send: GID not found GID index %d\n",
		       grh->sgid_index);
		return -ENOENT;
	}
#endif
#if DEFINE_ROCE_V2_SUPPORT /* QEDR_UPSTREAM */
	has_udp = (sgid_attr->gid_type == IB_GID_TYPE_ROCE_UDP_ENCAP);
	if (!has_udp) {
		/* RoCE v1 */
		ether_type = ETH_P_IBOE;
		*roce_mode = ROCE_V1;
	} else if (ipv6_addr_v4mapped((struct in6_addr *)sgid)) {
		/* RoCE v2 IPv4 */
		ip_ver = 4;
		ether_type = ETH_P_IP;
		has_grh_ipv6 = false;
		*roce_mode = ROCE_V2_IPV4;
	} else {
		/* RoCE v2 IPv6 */
		ip_ver = 6;
		ether_type = ETH_P_IPV6;
		*roce_mode = ROCE_V2_IPV6;
	}
#else
	ether_type = ETH_P_IBOE;
	*roce_mode = ROCE_V1;

#endif
#else
	has_vlan = qedr_get_vlan_id_gsi(ah_attr, &vlan_id);
	ether_type = ETH_P_IBOE;
	*roce_mode = ROCE_V1;
	if (grh->sgid_index < QEDR_MAX_SGID)
		sgid = &dev->sgid_tbl[grh->sgid_index];
	else
		sgid = &dev->sgid_tbl[0];
#endif

#ifdef DEFINE_IB_UD_HEADER_INIT_UDP_PRESENT /* QEDR_UPSTREAM */
	rc = ib_ud_header_init(send_size, false, true, has_vlan,
			       has_grh_ipv6, ip_ver, has_udp, 0, udh);
	if (rc) {
		DP_ERR(dev, "gsi post send: failed to init header\n");
		return rc;
	}
#else
	ib_ud_header_init(send_size, false /* LRH */, true /* ETH */,
			  has_vlan, has_grh_ipv6, 0 /* immediate */, udh);
#endif

	/* ENET + VLAN headers */
	ether_addr_copy(udh->eth.smac_h, mac);
#if DEFINE_IB_AH_ATTR_WITH_DMAC /* QEDR_UPSTREAM */
	rdma_ah_dmac = rdma_ah_retrieve_dmac(ah_attr);
	if (!rdma_ah_dmac) {
		DP_ERR(dev, "rdma_ah_retrieve_dmac: NULL DMAC\n");
		return -EINVAL;
	}
	ether_addr_copy(udh->eth.dmac_h, rdma_ah_dmac);
#else
	qedr_get_dmac(dev, ah_attr, mac);
	ether_addr_copy(udh->eth.dmac_h, mac);
#endif
	if (has_vlan) {
		dscp = GET_FIELD(grh->traffic_class, QED_RDMA_TC_TOS_DSCP);

		/* get the vlan priority associated with the dscp value */
		if (!dev->ops->rdma_get_dscp_priority(dev->rdma_ctx, dscp,
						      &vlan_pri))
			vlan_id |= vlan_pri << VLAN_PRIO_SHIFT;

		udh->vlan.tag = htons(vlan_id);

		udh->eth.type = htons(ETH_P_8021Q);
		udh->vlan.type = htons(ether_type);
	} else {
		udh->eth.type = htons(ether_type);
	}

	/* BTH */
	udh->bth.solicited_event = !!(swr->send_flags & IB_SEND_SOLICITED);
	udh->bth.pkey = QEDR_ROCE_PKEY_DEFAULT;
	udh->bth.destination_qpn = htonl(ud_wr(swr)->remote_qpn);
	udh->bth.psn = htonl((qp->sq_psn++) & ((1 << 24) - 1));
	udh->bth.opcode = IB_OPCODE_UD_SEND_ONLY;

	/* DETH */
	udh->deth.qkey = htonl(0x80010000);
	udh->deth.source_qpn = htonl(QEDR_GSI_QPN);

	if (has_grh_ipv6) {
		/* GRH / IPv6 header */
		udh->grh.traffic_class = grh->traffic_class;
		udh->grh.flow_label = grh->flow_label;
		udh->grh.hop_limit = grh->hop_limit;
		udh->grh.destination_gid = grh->dgid;
		memcpy(&udh->grh.source_gid.raw, sgid->raw,
		       sizeof(udh->grh.source_gid.raw));
	}
#ifdef DEFINE_IB_UD_HEADER_INIT_UDP_PRESENT /* QEDR_UPSTREAM */
	else {
		/* IPv4 header */
		u32 ipv4_addr;

		udh->ip4.protocol = IPPROTO_UDP;
		udh->ip4.tos = grh->traffic_class;
		udh->ip4.frag_off = htons(IP_DF);
		udh->ip4.ttl = grh->hop_limit;
		ipv4_addr = qedr_get_ipv4_from_gid(sgid->raw);
		udh->ip4.saddr = ipv4_addr;
		ipv4_addr = qedr_get_ipv4_from_gid(grh->dgid.raw);
		udh->ip4.daddr = ipv4_addr;
		/* note: checksum is calculated by the device */
	}
#endif

#ifdef DEFINE_IB_UD_HEADER_INIT_UDP_PRESENT /* QEDR_UPSTREAM */
	/* UDP */
	if (has_udp) {
		udh->udp.sport = htons(QEDR_ROCE_V2_UDP_SPORT);
		udh->udp.dport = htons(ROCE_V2_UDP_DPORT);
		udh->udp.csum = 0;
		/* UDP length is untouched hence is zero */
		/* INTERNAL: TODO: set UDP source port to a hash of the GIDs and QP #.
		 * Why: it may be used by switches along the way for path
		 * selection. Hence it must be fixed for flows that should
		 * maintain order.
		 */
	}
#endif
	return 0;
}

static inline int qedr_gsi_build_packet(struct qedr_dev *dev,
					struct qedr_qp *qp,
					RDMA_CONST struct ib_send_wr *swr,
					struct qed_roce_ll2_packet **p_packet)
{
	u8 ud_header_buffer[QEDR_MAX_UD_HEADER_SIZE];
	struct qed_roce_ll2_packet *packet;
	struct pci_dev *pdev = dev->pdev;
	int roce_mode, header_size;
	struct ib_ud_header udh;
	int i, rc;

	*p_packet = NULL;

	rc = qedr_gsi_build_header(dev, qp, swr, &udh, &roce_mode);
	if (rc)
		return rc;

	header_size = ib_ud_header_pack(&udh, &ud_header_buffer);

	packet = kzalloc(sizeof(*packet), GFP_ATOMIC);
	if (!packet)
		return -ENOMEM;

	packet->header.vaddr = dma_alloc_coherent(&pdev->dev, header_size,
						  &packet->header.baddr,
						  GFP_ATOMIC);
	if (!packet->header.vaddr) {
		kfree(packet);
		return -ENOMEM;
	}

	if (ether_addr_equal((u8 *)udh.eth.smac_h, (u8 *)udh.eth.dmac_h))
		packet->tx_dest = QED_ROCE_LL2_TX_DEST_LB;
	else
		packet->tx_dest = QED_ROCE_LL2_TX_DEST_NW;

	packet->roce_mode = roce_mode;
	memcpy(packet->header.vaddr, ud_header_buffer, header_size);
	packet->header.len = header_size;
	packet->n_seg = swr->num_sge;
	for (i = 0; i < packet->n_seg; i++) {
		packet->payload[i].baddr = swr->sg_list[i].addr;
		packet->payload[i].len = swr->sg_list[i].length;
	}

	*p_packet = packet;

	return 0;
}

int qedr_gsi_post_send(struct ib_qp *ibqp, IB_CONST struct ib_send_wr *wr,
		       IB_CONST struct ib_send_wr **bad_wr)
{
	struct qed_roce_ll2_packet *pkt = NULL;
	struct qedr_qp *qp = get_qedr_qp(ibqp);
	struct ib_cq *cq = ibqp->send_cq;
	struct qedr_dev *dev = qp->dev;
	unsigned long flags;
	int rc;

	/* The GSI QP is reset and restored as part of the recovery process,
	 * so we don't want to fail new requests - instead, just delay them
	 * until the recovery is over
	 */
	if (dev->recov_info.recov_in_prog) {
		struct qedr_cq *qcq;

		qcq = get_qedr_cq(cq);

		spin_lock_irqsave(&qp->q_lock, flags);
		qcq->polled_for_reset_qp = true;
		qcq->reset_notify_added = 1;
		qp->wqe_wr_id[qp->sq.prod].wr_id = wr->wr_id;
		qedr_inc_sw_prod(&qp->sq);

		/* Manually increase gsi_con to support SW POll */
		qedr_inc_sw_gsi_cons(&qp->sq);
		spin_unlock_irqrestore(&qp->q_lock, flags);

		DP_DEBUG(qp->dev, QEDR_MSG_GSI,
			 "gsi post send: opcode=%d, in_irq=%ld, irqs_disabled=%d, wr_id=%llx\n",
			 wr->opcode, in_irq(), irqs_disabled(), wr->wr_id);

		return 0;
	}

	if (dev->recov_info.dead) {
		*bad_wr = wr;
		DP_ERR(dev, "Device is dead, can no longer post wr\n");
		return -EINVAL;
	}

	if (qp->state != QED_ROCE_QP_STATE_RTS) {
		*bad_wr = wr;
		DP_ERR(dev,
		       "gsi post recv: failed to post rx buffer. state is %d and not QED_ROCE_QP_STATE_RTS\n",
		       qp->state);
		return -EINVAL;
	}

	if (wr->num_sge > RDMA_MAX_SGE_PER_SQ_WQE) {
		DP_ERR(dev, "gsi post send: num_sge is too large (%d>%d)\n",
		       wr->num_sge, RDMA_MAX_SGE_PER_SQ_WQE);
		rc = -EINVAL;
		goto err;
	}

	if (wr->opcode != IB_WR_SEND) {
		DP_ERR(dev,
		       "gsi post send: failed due to unsupported opcode %d\n",
		       wr->opcode);
		rc = -EINVAL;
		goto err;
	}

	spin_lock_irqsave(&qp->q_lock, flags);

	rc = qedr_gsi_build_packet(dev, qp, wr, &pkt);
	if (rc) {
		spin_unlock_irqrestore(&qp->q_lock, flags);
		goto err;
	}

#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
	if (unlikely((dev->dp_level <= QED_LEVEL_VERBOSE) &&
		     (dev->dp_module & QEDR_MSG_GSI)))
		qedr_print_hex_dump("TX CM Header ", pkt->header.vaddr,
				     pkt->header.len);
#endif

	rc = qedr_ll2_post_tx(dev, pkt);

	if (!rc) {
		qp->wqe_wr_id[qp->sq.prod].wr_id = wr->wr_id;
		qedr_inc_sw_prod(&qp->sq);
		DP_DEBUG(qp->dev, QEDR_MSG_GSI,
			 "gsi post send: opcode=%d, in_irq=%ld, irqs_disabled=%d, wr_id=%llx\n",
			 wr->opcode, in_irq(), irqs_disabled(), wr->wr_id);
	} else {
		DP_ERR(dev, "gsi post send: failed to transmit (rc=%d)\n", rc);
		rc = -EAGAIN;
		*bad_wr = wr;
	}

	spin_unlock_irqrestore(&qp->q_lock, flags);

	/* INTERNAL: TODO: to be aligned to the protocol we should add code that allows
	 * posting more than one WR. This currently isn't the case.
	 */
	if (wr->next) {
		DP_ERR(dev,
		       "gsi post send: failed second WR. Only one WR may be passed at a time\n");
		*bad_wr = wr->next;
		rc = -EINVAL;
	}

	return rc;

err:
	*bad_wr = wr;
	return rc;
}

int qedr_gsi_post_recv(struct ib_qp *ibqp, IB_CONST struct ib_recv_wr *wr,
		       IB_CONST struct ib_recv_wr **bad_wr)
{
	struct qedr_dev *dev = get_qedr_dev(ibqp->device);
	struct qedr_qp *qp = get_qedr_qp(ibqp);
	unsigned long flags;
	int rc = 0;

	if ((qp->state != QED_ROCE_QP_STATE_RTR) &&
	    (qp->state != QED_ROCE_QP_STATE_RTS)) {
		*bad_wr = wr;
		DP_ERR(dev,
		       "gsi post recv: failed to post rx buffer. state is %d and not QED_ROCE_QP_STATE_RTR/S\n",
		       qp->state);
		return -EINVAL;
	}

	spin_lock_irqsave(&qp->q_lock, flags);

	while (wr) {
		if (wr->num_sge > QEDR_GSI_MAX_RECV_SGE) {
			DP_ERR(dev,
			       "gsi post recv: failed to post rx buffer. too many sges %d>%d\n",
			       wr->num_sge, QEDR_GSI_MAX_RECV_SGE);
			goto err;
		}

		rc = dev->ops->ll2_post_rx_buffer(dev->rdma_ctx,
						  dev->gsi_ll2_handle,
						  wr->sg_list[0].addr,
						  wr->sg_list[0].length,
						  NULL /* cookie */,
						  1 /* notify_fw */);
		if (rc) {
			DP_ERR(dev,
			       "gsi post recv: failed to post rx buffer (rc=%d)\n",
			       rc);
			goto err;
		}

		memset(&qp->rqe_wr_id[qp->rq.prod], 0,
		       sizeof(qp->rqe_wr_id[qp->rq.prod]));
		qp->rqe_wr_id[qp->rq.prod].sg_list[0] = wr->sg_list[0];
		qp->rqe_wr_id[qp->rq.prod].wr_id = wr->wr_id;

		qedr_inc_sw_prod(&qp->rq);

		wr = wr->next;
	}

	spin_unlock_irqrestore(&qp->q_lock, flags);

	return rc;
err:
	spin_unlock_irqrestore(&qp->q_lock, flags);
	*bad_wr = wr;
	return -ENOMEM;
}

int qedr_gsi_poll_cq(struct ib_cq *ibcq, int num_entries, struct ib_wc *wc)
{
	struct qedr_dev *dev = get_qedr_dev(ibcq->device);
	struct qedr_cq *cq = get_qedr_cq(ibcq);
	struct qedr_qp *qp = dev->gsi_qp;
	unsigned long flags;
#if DEFINED_IB_WC_WITH_VLAN /* QEDR_UPSTREAM */
	u16 vlan_id;
#endif
	int i = 0;

	/* Reset recovery note: All of this is software based, therefore there
	 * is no need for special handling during or after reset recovery
	 */
	spin_lock_irqsave(&cq->cq_lock, flags);

	while (i < num_entries && qp->rq.cons != qp->rq.gsi_cons) {
		memset(&wc[i], 0, sizeof(*wc));

		wc[i].qp = &qp->ibqp;
		wc[i].wr_id = qp->rqe_wr_id[qp->rq.cons].wr_id;
		wc[i].opcode = IB_WC_RECV;
		wc[i].pkey_index = 0;
		wc[i].status = (qp->rqe_wr_id[qp->rq.cons].rc) ?
		    IB_WC_GENERAL_ERR : IB_WC_SUCCESS;
		/* 0 - currently only one recv sg is supported */
		wc[i].byte_len = qp->rqe_wr_id[qp->rq.cons].sg_list[0].length;
#if DEFINED_IB_WC_IP_CSUM_OK /* QEDR_UPSTREAM */
		wc[i].wc_flags |= IB_WC_GRH | IB_WC_IP_CSUM_OK;
#else
		wc[i].wc_flags |= IB_WC_GRH;
#endif
#if DEFINED_IB_WC_WITH_SMAC /* QEDR_UPSTREAM */
		ether_addr_copy(wc[i].smac, qp->rqe_wr_id[qp->rq.cons].smac);
		wc[i].wc_flags |= IB_WC_WITH_SMAC;
#endif

#if DEFINED_IB_WC_WITH_VLAN /* QEDR_UPSTREAM */
		vlan_id = qp->rqe_wr_id[qp->rq.cons].vlan &
			  VLAN_VID_MASK;
		if (vlan_id) {
			wc[i].wc_flags |= IB_WC_WITH_VLAN;
			wc[i].vlan_id = vlan_id;
			wc[i].sl = (qp->rqe_wr_id[qp->rq.cons].vlan &
				    VLAN_PRIO_MASK) >> VLAN_PRIO_SHIFT;
		}
#endif
		qedr_inc_sw_cons(&qp->rq);
		i++;
	}

	while (i < num_entries && qp->sq.cons != qp->sq.gsi_cons) {
		memset(&wc[i], 0, sizeof(*wc));

		wc[i].qp = &qp->ibqp;
		wc[i].wr_id = qp->wqe_wr_id[qp->sq.cons].wr_id;
		wc[i].opcode = IB_WC_SEND;
		wc[i].status = IB_WC_SUCCESS;
		if (dev->recov_info.recov_in_prog || cq->polled_for_reset_qp) {
			cq->polled_for_reset_qp = false;
			wc[i].status = IB_WC_WR_FLUSH_ERR;
		}

		qedr_inc_sw_cons(&qp->sq);
		i++;
	}

	spin_unlock_irqrestore(&cq->cq_lock, flags);

	DP_DEBUG(dev, QEDR_MSG_GSI,
		 "gsi poll_cq: requested entries=%d, actual=%d, qp->rq.cons=%d, qp->rq.gsi_cons=%d, qp->sq.cons=%d, qp->sq.gsi_cons=%d, qp_num=%d\n",
		 num_entries, i, qp->rq.cons, qp->rq.gsi_cons, qp->sq.cons,
		 qp->sq.gsi_cons, qp->ibqp.qp_num);

	return i;
}
