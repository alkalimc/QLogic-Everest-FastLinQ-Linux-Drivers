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

#include <linux/types.h>
#include <asm/byteorder.h>
#include <linux/dma-mapping.h>
#include <linux/if_vlan.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <net/ipv6.h>
#include <asm/cache.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#define __PREVENT_DUMP_MEM_ARR__
#define __PREVENT_PXP_GLOBAL_WIN__
#include "qed.h"
#include "qed_compat.h"
#include "qed_cxt.h"
#include "qed_dev_api.h"
#include "qed_hsi.h"
#include "qed_hw.h"
#include "qed_int.h"
#include "qed_iro_hsi.h"
#include "qed_ll2.h"
#include "qed_ll2_if.h"
#include "qed_mcp.h"
#include "qed_ooo.h"
#include "qed_reg_addr.h"
#include "qed_sp.h"
#include "qed_sriov.h"
#include "qed_vf.h"
#include "qed_ll2_if.h"

#define QED_LL2_TX_SIZE (256)
#define QED_LL2_RX_SIZE (4096)

struct qed_cb_ll2_info {
	int rx_cnt;
	u32 rx_size;
	u8 handle;

	/* Lock protecting LL2 buffer lists in sleepless context */
	spinlock_t lock;
	struct list_head list;

	const struct qed_ll2_cb_ops *cbs;
	void *cb_cookie;
};

struct qed_ll2_buffer {
	struct list_head list;
/*	QED_RX_DATA *data;*/
	void *data;
	dma_addr_t phys_addr;
};

static void qed_ll2b_complete_tx_packet(void *cxt,
					u8 connection_handle,
					void *cookie,
					dma_addr_t first_frag_addr,
					bool b_last_fragment,
					bool b_last_packet)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)cxt;
	struct qed_dev *cdev = p_hwfn->cdev;

	/* All we need to do is release the mapping */
	/* @@@TBD - add Tx data in order to use dmae_unset_addr */

	struct sk_buff *skb = cookie;

	dma_unmap_single(&p_hwfn->cdev->pdev->dev, first_frag_addr,
			 skb_headlen(skb), DMA_TO_DEVICE);

	if (cdev->ll2->cbs && cdev->ll2->cbs->tx_cb)
		cdev->ll2->cbs->tx_cb(cdev->ll2->cb_cookie, skb,
				      b_last_fragment);

	dev_kfree_skb_any(skb);
}

static int qed_ll2_alloc_buffer(struct qed_dev *cdev,
				QED_RX_DATA ** data, dma_addr_t * phys_addr)
{
#if RHEL_IS_VERSION(6, 2)	/* ! QED_UPSTREAM */
	/* RHEL 6.2 requires the ndev to be supplied to skb alloc to associate
	 * it with a NUMA node. edev can be obtained via ops_cookie but there
	 * is no way to determine the offset of ndev with edev nicely. We assume
	 * it won't change.
	 *
	 * WARNING: This is valid only on top of QEDE!
	 */
	u8 *edev = (u8 *) cdev->ops_cookie;
	u8 *ndev = edev + sizeof(struct qed_dev *);

	*data = QED_ALLOC_RX_DATA((void *)ndev, cdev->ll2->rx_size, GFP_ATOMIC);
#else
	*data = QED_ALLOC_RX_DATA(cdev->ll2->rx_size, GFP_ATOMIC);
#endif
	if (!(*data)) {
		DP_NOTICE(cdev, "Failed to allocate LL2 buffer data\n");
		return -ENOMEM;
	}

	*phys_addr = dma_map_single(&cdev->pdev->dev,
				    QED_RX_DATA_PTR((*data)),
				    cdev->ll2->rx_size, DMA_FROM_DEVICE);
	if (dma_mapping_error(&cdev->pdev->dev, *phys_addr)) {
		DP_NOTICE(cdev, "Failed to map LL2 buffer data\n");
		QED_FREE_RX_DATA((*data));
		return -ENOMEM;
	}

	return 0;
}

static int qed_ll2_dealloc_buffer(struct qed_dev *cdev,
				  struct qed_ll2_buffer *buffer)
{
	spin_lock_bh(&cdev->ll2->lock);

	dma_unmap_single(&cdev->pdev->dev, buffer->phys_addr,
			 cdev->ll2->rx_size, DMA_FROM_DEVICE);
	QED_FREE_RX_DATA(buffer->data);
	list_del(&buffer->list);

	cdev->ll2->rx_cnt--;
	if (!cdev->ll2->rx_cnt)
		DP_INFO(cdev, "All LL2 entries were removed\n");

	spin_unlock_bh(&cdev->ll2->lock);

	return 0;
}

static void qed_ll2_kill_buffers(struct qed_dev *cdev)
{
	struct qed_ll2_buffer *buffer, *tmp_buffer;

	list_for_each_entry_safe(buffer, tmp_buffer, &cdev->ll2->list, list)
	    qed_ll2_dealloc_buffer(cdev, buffer);
}

static void qed_ll2b_complete_rx_packet(void *cxt,
					struct qed_ll2_comp_rx_data *data)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)cxt;
	struct qed_ll2_buffer *buffer = data->cookie;
	struct qed_dev *cdev = p_hwfn->cdev;
	dma_addr_t new_phys_addr;
	struct sk_buff *skb;
	bool reuse = false;
	int rc = -EINVAL;
	QED_RX_DATA *new_data;

	DP_VERBOSE(p_hwfn,
		   (NETIF_MSG_RX_STATUS | QED_MSG_STORAGE | NETIF_MSG_PKTDATA),
		   "Got an LL2 Rx completion: [Buffer at phys 0x%llx, offset 0x%02x] Length 0x%04x Parse_flags 0x%04x vlan 0x%04x Opaque data [0x%08x:0x%08x]\n",
		   (u64) data->rx_buf_addr,
		   data->u.placement_offset,
		   data->length.packet_length,
		   data->parse_flags,
		   data->vlan, data->opaque_data_0, data->opaque_data_1);

	if ((cdev->dp_module & NETIF_MSG_PKTDATA) && buffer->data) {
		print_hex_dump(KERN_INFO, "",
			       DUMP_PREFIX_OFFSET, 16, 1,
			       buffer->data, data->length.packet_length, false);
	}

	/* Determine if data is valid */
	if (data->length.packet_length < ETH_HLEN)
		reuse = true;

	/* Allocate a replacement for buffer; Reuse upon failure */
	if (!reuse)
		rc = qed_ll2_alloc_buffer(p_hwfn->cdev, &new_data,
					  &new_phys_addr);

	if (rc)
		goto out_post;

	dma_unmap_single(&cdev->pdev->dev, buffer->phys_addr,
			 cdev->ll2->rx_size, DMA_FROM_DEVICE);

	/* TODO - need to verify skb got created.
	 * Also, in upstream there's a leak here if !skb.
	 */
	skb = QED_BUILD_SKB(buffer->data);
	if (!skb) {
		DP_INFO(cdev, "Failed to build SKB\n");
		QED_FREE_RX_DATA(buffer->data);
		goto out_post1;
	}

	QED_SET_RX_PAD(data->u.placement_offset);
	skb_reserve(skb, data->u.placement_offset);
	skb_put(skb, data->length.packet_length);
	skb_checksum_none_assert(skb);

	/* Get ethernet information instead of eth_type_trans().
	 * notice this isn't complete, e.g., we don't set pkt_type on
	 * the SKB - but we don't have an associated net_device, so not
	 * sure what can be done.
	 */
	skb_reset_mac_header(skb);
	skb->protocol = eth_hdr(skb)->h_proto;

	/* Pass SKB onward */
	if (cdev->ll2->cbs && cdev->ll2->cbs->rx_cb) {
#ifdef MODERN_VLAN
		if (data->vlan)
			__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q),
					       data->vlan);
		cdev->ll2->cbs->rx_cb(cdev->ll2->cb_cookie, skb,
				      data->opaque_data_0, data->opaque_data_1);
#else
		cdev->ll2->cbs->rx_cb(cdev->ll2->cb_cookie, skb, data->vlan,
				      data->opaque_data_0, data->opaque_data_1);
#endif
	} else {
		DP_VERBOSE(p_hwfn, (NETIF_MSG_RX_STATUS | QED_MSG_STORAGE |
				    NETIF_MSG_PKTDATA),
			   "Dropping the packet\n");
		QED_FREE_RX_DATA(buffer->data);
	}

out_post1:
	/* Update Buffer information and update FW producer */
	buffer->data = new_data;
	buffer->phys_addr = new_phys_addr;

out_post:
	rc = qed_ll2_post_rx_buffer(p_hwfn, cdev->ll2->handle, buffer->phys_addr, 0,	/* buffer length is required in gsi_offloaded mode only */
				    buffer, 1);
	if (rc)
		qed_ll2_dealloc_buffer(cdev, buffer);
}

static struct qed_ll2_info *__qed_ll2_handle_sanity(struct qed_hwfn *p_hwfn,
						    u8 connection_handle,
						    bool b_lock,
						    bool b_only_active)
{
	struct qed_ll2_info *p_ll2_conn, *p_ret = NULL;
	u8 num_conns;

	num_conns = QED_MAX_NUM_OF_LL2_CONNS(p_hwfn);

	if (connection_handle >= num_conns)
		return NULL;

	if (!p_hwfn->p_ll2_info)
		return NULL;

	/* TODO - is there really need for the locked vs. unlocked
	 * variant? I simply used what was already there.
	 */
	p_ll2_conn = &p_hwfn->p_ll2_info[connection_handle];

	if (b_only_active) {
		if (b_lock)
			mutex_lock(&p_ll2_conn->mutex);
		if (p_ll2_conn->b_active)
			p_ret = p_ll2_conn;
		if (b_lock)
			mutex_unlock(&p_ll2_conn->mutex);
	} else {
		p_ret = p_ll2_conn;
	}

	return p_ret;
}

static struct qed_ll2_info *qed_ll2_handle_sanity(struct qed_hwfn *p_hwfn,
						  u8 connection_handle)
{
	return __qed_ll2_handle_sanity(p_hwfn, connection_handle, false, true);
}

struct qed_ll2_info *qed_ll2_handle_sanity_lock(struct qed_hwfn *p_hwfn,
						u8 connection_handle)
{
	return __qed_ll2_handle_sanity(p_hwfn, connection_handle, true, true);
}

static struct qed_ll2_info *qed_ll2_handle_sanity_inactive(struct qed_hwfn
							   *p_hwfn,
							   u8 connection_handle)
{
	return __qed_ll2_handle_sanity(p_hwfn, connection_handle, false, false);
}

static enum core_error_handle
qed_ll2_get_error_choice(enum qed_ll2_error_handle err)
{
	switch (err) {
	case QED_LL2_DROP_PACKET:
		return LL2_DROP_PACKET;
	case QED_LL2_DO_NOTHING:
		return LL2_DO_NOTHING;
	case QED_LL2_ASSERT:
		return LL2_ASSERT;
	default:
		return LL2_DO_NOTHING;
	}
}

static void qed_ll2_txq_flush(struct qed_hwfn *p_hwfn, u8 connection_handle)
{
	bool b_last_packet = false, b_last_frag = false;
	struct qed_ll2_tx_packet *p_pkt = NULL;
	struct qed_ll2_info *p_ll2_conn;
	struct qed_ll2_tx_queue *p_tx;
	unsigned long flags = 0;
	dma_addr_t tx_frag;

	p_ll2_conn = qed_ll2_handle_sanity_inactive(p_hwfn, connection_handle);
	if (p_ll2_conn == NULL)
		return;

	p_tx = &p_ll2_conn->tx_queue;

	spin_lock_irqsave(&p_tx->lock, flags);
	while (!list_empty(&p_tx->active_descq)) {
		p_pkt = list_first_entry(&p_tx->active_descq,
					 struct qed_ll2_tx_packet, list_entry);

		if (p_pkt == NULL)
			break;

		list_del(&p_pkt->list_entry);
		b_last_packet = list_empty(&p_tx->active_descq);
		list_add_tail(&p_pkt->list_entry, &p_tx->free_descq);
		spin_unlock_irqrestore(&p_tx->lock, flags);
		if (p_ll2_conn->input.conn_type == QED_LL2_TYPE_OOO) {
			struct qed_ooo_buffer *p_buffer;

			p_buffer = (struct qed_ooo_buffer *)p_pkt->cookie;
			qed_ooo_put_free_buffer(p_hwfn->p_ooo_info, p_buffer);
		} else {
			p_tx->cur_completing_packet = *p_pkt;
			p_tx->cur_completing_bd_idx = 1;
			b_last_frag =
			    p_tx->cur_completing_bd_idx == p_pkt->bd_used;

			tx_frag = p_pkt->bds_set[0].tx_frag;
			p_ll2_conn->cbs.tx_release_cb(p_ll2_conn->cbs.cookie,
						      p_ll2_conn->my_id,
						      p_pkt->cookie,
						      tx_frag,
						      b_last_frag,
						      b_last_packet);
		}
		spin_lock_irqsave(&p_tx->lock, flags);
	}
	spin_unlock_irqrestore(&p_tx->lock, flags);
}

static int qed_ll2_txq_completion(struct qed_hwfn *p_hwfn, void *p_cookie)
{
	struct qed_ll2_info *p_ll2_conn = (struct qed_ll2_info *)p_cookie;
	u16 new_idx = 0, num_bds = 0, num_bds_in_packet = 0;
	struct qed_ll2_tx_packet *p_pkt;
	struct qed_ll2_tx_queue *p_tx;
	bool b_last_frag = false;
	unsigned long flags;
	int rc = -EINVAL;

	if (!p_ll2_conn)
		return rc;

	p_tx = &p_ll2_conn->tx_queue;

	spin_lock_irqsave(&p_tx->lock, flags);
	if (p_tx->b_completing_packet) {
		/* TODO - this looks completely unnecessary to me - the only
		 * way we can re-enter is by the DPC calling us again, but this
		 * would only happen AFTER we return, and we unset this at end
		 * of the function.
		 */
		rc = -EBUSY;
		goto out;
	}

	new_idx = le16_to_cpu(*p_tx->p_fw_cons);
	num_bds = ((s16) new_idx - (s16) p_tx->bds_idx);
	while (num_bds) {
		if (list_empty(&p_tx->active_descq))
			goto out;

		p_pkt = list_first_entry(&p_tx->active_descq,
					 struct qed_ll2_tx_packet, list_entry);
		if (!p_pkt)
			goto out;

		p_tx->b_completing_packet = true;
		p_tx->cur_completing_packet = *p_pkt;
		num_bds_in_packet = p_pkt->bd_used;
		list_del(&p_pkt->list_entry);

		if (num_bds < num_bds_in_packet) {
			DP_NOTICE(p_hwfn,
				  "Rest of BDs does not cover whole packet\n");
			goto out;
		}

		num_bds -= num_bds_in_packet;
		p_tx->bds_idx += num_bds_in_packet;
		while (num_bds_in_packet--)
			qed_chain_consume(&p_tx->txq_chain);

		p_tx->cur_completing_bd_idx = 1;
		b_last_frag = p_tx->cur_completing_bd_idx == p_pkt->bd_used;
		list_add_tail(&p_pkt->list_entry, &p_tx->free_descq);

		spin_unlock_irqrestore(&p_tx->lock, flags);

		p_ll2_conn->cbs.tx_comp_cb(p_ll2_conn->cbs.cookie,
					   p_ll2_conn->my_id,
					   p_pkt->cookie,
					   p_pkt->bds_set[0].tx_frag,
					   b_last_frag, !num_bds);

		spin_lock_irqsave(&p_tx->lock, flags);
	}

	p_tx->b_completing_packet = false;
	rc = 0;
out:
	spin_unlock_irqrestore(&p_tx->lock, flags);
	return rc;
}

static void qed_ll2_rxq_parse_gsi(union core_rx_cqe_union *p_cqe,
				  struct qed_ll2_comp_rx_data *data)
{
	data->parse_flags = le16_to_cpu(p_cqe->rx_cqe_gsi.parse_flags.flags);
	data->length.data_length = le16_to_cpu(p_cqe->rx_cqe_gsi.data_length);
	data->vlan = le16_to_cpu(p_cqe->rx_cqe_gsi.vlan);
	data->opaque_data_0 = le32_to_cpu(p_cqe->rx_cqe_gsi.src_mac_addrhi);
	data->opaque_data_1 = le16_to_cpu(p_cqe->rx_cqe_gsi.src_mac_addrlo);
	data->u.data_length_error = p_cqe->rx_cqe_gsi.data_length_error;
	data->qp_id = le16_to_cpu(p_cqe->rx_cqe_gsi.qp_id);
	data->src_qp = le32_to_cpu(p_cqe->rx_cqe_gsi.src_qp);
	qed_roce_pvrdma_parse_gsi(p_cqe, data);
}

static void qed_ll2_rxq_parse_reg(union core_rx_cqe_union *p_cqe,
				  struct qed_ll2_comp_rx_data *data)
{
	data->parse_flags = le16_to_cpu(p_cqe->rx_cqe_fp.parse_flags.flags);
	data->err_flags = le16_to_cpu(p_cqe->rx_cqe_fp.err_flags.flags);
	data->length.packet_length =
	    le16_to_cpu(p_cqe->rx_cqe_fp.packet_length);
	data->vlan = le16_to_cpu(p_cqe->rx_cqe_fp.vlan);
	data->opaque_data_0 = le32_to_cpu(p_cqe->rx_cqe_fp.opaque_data.data[0]);
	data->opaque_data_1 = le32_to_cpu(p_cqe->rx_cqe_fp.opaque_data.data[1]);
	data->u.placement_offset = p_cqe->rx_cqe_fp.placement_offset;
}

static int
qed_ll2_handle_slowpath(struct qed_hwfn *p_hwfn,
			struct qed_ll2_info *p_ll2_conn,
			union core_rx_cqe_union *p_cqe,
			unsigned long *p_lock_flags)
{
	struct qed_ll2_rx_queue *p_rx = &p_ll2_conn->rx_queue;
	struct core_rx_slow_path_cqe *sp_cqe;

	sp_cqe = &p_cqe->rx_cqe_sp;
	if (sp_cqe->ramrod_cmd_id != CORE_RAMROD_RX_QUEUE_FLUSH) {
		DP_NOTICE(p_hwfn,
			  "LL2 - unexpected Rx CQE slowpath ramrod_cmd_id:%d\n",
			  sp_cqe->ramrod_cmd_id);
		return -EINVAL;
	}

	if (p_ll2_conn->cbs.slowpath_cb == NULL) {
		DP_NOTICE(p_hwfn,
			  "LL2 - received RX_QUEUE_FLUSH but no callback was provided\n");
		return -EINVAL;
	}

	spin_unlock_irqrestore(&p_rx->lock, *p_lock_flags);

	p_ll2_conn->cbs.slowpath_cb(p_ll2_conn->cbs.cookie,
				    p_ll2_conn->my_id,
				    le32_to_cpu(sp_cqe->opaque_data.data[0]),
				    le32_to_cpu(sp_cqe->opaque_data.data[1]));

	spin_lock_irqsave(&p_rx->lock, *p_lock_flags);

	return 0;
}

static int
qed_ll2_rxq_handle_completion(struct qed_hwfn *p_hwfn,
			      struct qed_ll2_info *p_ll2_conn,
			      union core_rx_cqe_union *p_cqe,
			      unsigned long *p_lock_flags, bool b_last_cqe)
{
	struct qed_ll2_rx_queue *p_rx = &p_ll2_conn->rx_queue;
	struct qed_ll2_rx_packet *p_pkt = NULL;
	struct qed_ll2_comp_rx_data data;

	if (!list_empty(&p_rx->active_descq))
		p_pkt = list_first_entry(&p_rx->active_descq,
					 struct qed_ll2_rx_packet, list_entry);
	if (!p_pkt) {
		DP_NOTICE(p_hwfn,
			  "[%d] LL2 Rx completion but active_descq is empty\n",
			  p_ll2_conn->input.conn_type);

		return -EIO;
	}

	list_del(&p_pkt->list_entry);

	if (p_cqe->rx_cqe_sp.type == CORE_RX_CQE_TYPE_REGULAR)
		qed_ll2_rxq_parse_reg(p_cqe, &data);
	else
		qed_ll2_rxq_parse_gsi(p_cqe, &data);

	if (qed_chain_consume(&p_rx->rxq_chain) != p_pkt->rxq_bd)
		DP_NOTICE(p_hwfn,
			  "Mismatch between active_descq and the LL2 Rx chain\n");
	/* TODO - didn't return error value since this wasn't handled
	 * before, but this is obviously lacking.
	 */

	list_add_tail(&p_pkt->list_entry, &p_rx->free_descq);

	data.connection_handle = p_ll2_conn->my_id;
	data.cookie = p_pkt->cookie;
	data.rx_buf_addr = p_pkt->rx_buf_addr;
	data.b_last_packet = b_last_cqe;

	spin_unlock_irqrestore(&p_rx->lock, *p_lock_flags);
	p_ll2_conn->cbs.rx_comp_cb(p_ll2_conn->cbs.cookie, &data);

	spin_lock_irqsave(&p_rx->lock, *p_lock_flags);

	return 0;
}

static int qed_ll2_rxq_completion(struct qed_hwfn *p_hwfn, void *cookie)
{
	struct qed_ll2_info *p_ll2_conn = (struct qed_ll2_info *)cookie;
	union core_rx_cqe_union *cqe = NULL;
	u16 cq_new_idx = 0, cq_old_idx = 0;
	struct qed_ll2_rx_queue *p_rx;
	unsigned long flags = 0;
	int rc = 0;

	if (!p_ll2_conn)
		return rc;

	p_rx = &p_ll2_conn->rx_queue;

	spin_lock_irqsave(&p_rx->lock, flags);

	if (!QED_LL2_RX_REGISTERED(p_ll2_conn)) {
		spin_unlock_irqrestore(&p_rx->lock, flags);
		return 0;
	}

	cq_new_idx = le16_to_cpu(*p_rx->p_fw_cons);
	cq_old_idx = qed_chain_get_cons_idx(&p_rx->rcq_chain);

	while (cq_new_idx != cq_old_idx) {
		bool b_last_cqe = (cq_new_idx == cq_old_idx);

		cqe =
		    (union core_rx_cqe_union *)qed_chain_consume(&p_rx->
								 rcq_chain);
		cq_old_idx = qed_chain_get_cons_idx(&p_rx->rcq_chain);

		DP_VERBOSE(p_hwfn,
			   QED_MSG_LL2,
			   "LL2 [sw. cons %04x, fw. at %04x] - Got Packet of type %02x\n",
			   cq_old_idx, cq_new_idx, cqe->rx_cqe_sp.type);

		switch (cqe->rx_cqe_sp.type) {
		case CORE_RX_CQE_TYPE_SLOW_PATH:
			rc = qed_ll2_handle_slowpath(p_hwfn, p_ll2_conn,
						     cqe, &flags);
			break;
		case CORE_RX_CQE_TYPE_GSI_OFFLOAD:
		case CORE_RX_CQE_TYPE_REGULAR:
			rc = qed_ll2_rxq_handle_completion(p_hwfn, p_ll2_conn,
							   cqe, &flags,
							   b_last_cqe);
			break;
		default:
			rc = -EIO;
		}
	}

	spin_unlock_irqrestore(&p_rx->lock, flags);
	return rc;
}

static void qed_ll2_rxq_flush(struct qed_hwfn *p_hwfn, u8 connection_handle)
{
	struct qed_ll2_info *p_ll2_conn = NULL;
	struct qed_ll2_rx_packet *p_pkt = NULL;
	struct qed_ll2_rx_queue *p_rx;
	unsigned long flags = 0;

	p_ll2_conn = qed_ll2_handle_sanity_inactive(p_hwfn, connection_handle);
	if (p_ll2_conn == NULL)
		return;

	p_rx = &p_ll2_conn->rx_queue;

	spin_lock_irqsave(&p_rx->lock, flags);
	while (!list_empty(&p_rx->active_descq)) {
		bool b_last;
		p_pkt = list_first_entry(&p_rx->active_descq,
					 struct qed_ll2_rx_packet, list_entry);
		if (p_pkt == NULL)
			break;
		list_del(&p_pkt->list_entry);
		list_add_tail(&p_pkt->list_entry, &p_rx->free_descq);
		b_last = list_empty(&p_rx->active_descq);
		spin_unlock_irqrestore(&p_rx->lock, flags);

		if (p_ll2_conn->input.conn_type == QED_LL2_TYPE_OOO) {
			struct qed_ooo_buffer *p_buffer;

			p_buffer = (struct qed_ooo_buffer *)p_pkt->cookie;
			qed_ooo_put_free_buffer(p_hwfn->p_ooo_info, p_buffer);
		} else {
			dma_addr_t rx_buf_addr = p_pkt->rx_buf_addr;
			void *cookie = p_pkt->cookie;

			p_ll2_conn->cbs.rx_release_cb(p_ll2_conn->cbs.cookie,
						      p_ll2_conn->my_id,
						      cookie,
						      rx_buf_addr, b_last);
		}
		spin_lock_irqsave(&p_rx->lock, flags);
	}
	spin_unlock_irqrestore(&p_rx->lock, flags);
}

static bool
qed_ll2_lb_rxq_handler_slowpath(struct qed_hwfn *p_hwfn,
				struct core_rx_slow_path_cqe *p_cqe)
{
	struct ooo_opaque *iscsi_ooo;
	u32 cid;

	if (p_cqe->ramrod_cmd_id != CORE_RAMROD_RX_QUEUE_FLUSH)
		return false;

	iscsi_ooo = (struct ooo_opaque *)&p_cqe->opaque_data;
	if (iscsi_ooo->ooo_opcode != TCP_EVENT_DELETE_ISLES)
		return false;

	/* Need to make a flush */
	cid = le32_to_cpu(iscsi_ooo->cid);
	qed_ooo_release_connection_isles(p_hwfn->p_ooo_info, cid);

	return true;
}

static int
qed_ll2_lb_rxq_handler(struct qed_hwfn *p_hwfn, struct qed_ll2_info *p_ll2_conn)
{
	struct qed_ll2_rx_queue *p_rx = &p_ll2_conn->rx_queue;
	u16 packet_length = 0, parse_flags = 0, vlan = 0;
	struct qed_ll2_rx_packet *p_pkt = NULL;
	u32 num_ooo_add_to_peninsula = 0, cid;
	union core_rx_cqe_union *cqe = NULL;
	u16 cq_new_idx = 0, cq_old_idx = 0;
	struct qed_ooo_buffer *p_buffer;
	struct ooo_opaque *iscsi_ooo;
	u8 placement_offset = 0;
	u8 cqe_type;

	cq_new_idx = le16_to_cpu(*p_rx->p_fw_cons);
	cq_old_idx = qed_chain_get_cons_idx(&p_rx->rcq_chain);
	if (cq_new_idx == cq_old_idx)
		return 0;

	while (cq_new_idx != cq_old_idx) {
		struct core_rx_fast_path_cqe *p_cqe_fp;

		cqe =
		    (union core_rx_cqe_union *)qed_chain_consume(&p_rx->
								 rcq_chain);
		cq_old_idx = qed_chain_get_cons_idx(&p_rx->rcq_chain);
		cqe_type = cqe->rx_cqe_sp.type;

		if (cqe_type == CORE_RX_CQE_TYPE_SLOW_PATH)
			if (qed_ll2_lb_rxq_handler_slowpath(p_hwfn,
							    &cqe->rx_cqe_sp))
				continue;

		if (cqe_type != CORE_RX_CQE_TYPE_REGULAR) {
			DP_NOTICE(p_hwfn,
				  "Got a non-regular LB LL2 completion [type 0x%02x]\n",
				  cqe_type);
			return -EINVAL;
		}
		p_cqe_fp = &cqe->rx_cqe_fp;

		placement_offset = p_cqe_fp->placement_offset;
		parse_flags = le16_to_cpu(p_cqe_fp->parse_flags.flags);
		packet_length = le16_to_cpu(p_cqe_fp->packet_length);
		vlan = le16_to_cpu(p_cqe_fp->vlan);
		iscsi_ooo = (struct ooo_opaque *)&p_cqe_fp->opaque_data;
		qed_ooo_save_history_entry(p_hwfn->p_ooo_info, iscsi_ooo);
		cid = le32_to_cpu(iscsi_ooo->cid);

		/* Process delete isle first */
		if (iscsi_ooo->drop_size)
			qed_ooo_delete_isles(p_hwfn, p_hwfn->p_ooo_info, cid,
					     iscsi_ooo->drop_isle,
					     iscsi_ooo->drop_size);

		if (iscsi_ooo->ooo_opcode == TCP_EVENT_NOP)
			continue;

		/* Now process create/add/join isles */
		if (list_empty(&p_rx->active_descq)) {
			DP_NOTICE(p_hwfn,
				  "LL2 OOO RX chain has no submitted buffers\n");
			return -EIO;
		}

		p_pkt = list_first_entry(&p_rx->active_descq,
					 struct qed_ll2_rx_packet, list_entry);

		if ((iscsi_ooo->ooo_opcode == TCP_EVENT_ADD_NEW_ISLE) ||
		    (iscsi_ooo->ooo_opcode == TCP_EVENT_ADD_ISLE_RIGHT) ||
		    (iscsi_ooo->ooo_opcode == TCP_EVENT_ADD_ISLE_LEFT) ||
		    (iscsi_ooo->ooo_opcode == TCP_EVENT_ADD_PEN) ||
		    (iscsi_ooo->ooo_opcode == TCP_EVENT_JOIN)) {
			if (!p_pkt) {
				DP_NOTICE(p_hwfn,
					  "LL2 OOO RX packet is not valid\n");
				return -EIO;
			}
			list_del(&p_pkt->list_entry);
			p_buffer = (struct qed_ooo_buffer *)p_pkt->cookie;
			p_buffer->packet_length = packet_length;
			p_buffer->parse_flags = parse_flags;
			p_buffer->vlan = vlan;
			p_buffer->placement_offset = placement_offset;
			if (qed_chain_consume(&p_rx->rxq_chain) !=
			    p_pkt->rxq_bd) {
			 /**/}
			qed_ooo_dump_rx_event(p_hwfn, iscsi_ooo, p_buffer);
			list_add_tail(&p_pkt->list_entry, &p_rx->free_descq);

			switch (iscsi_ooo->ooo_opcode) {
			case TCP_EVENT_ADD_NEW_ISLE:
				qed_ooo_add_new_isle(p_hwfn,
						     p_hwfn->p_ooo_info,
						     cid,
						     iscsi_ooo->ooo_isle,
						     p_buffer);
				break;
			case TCP_EVENT_ADD_ISLE_RIGHT:
				qed_ooo_add_new_buffer(p_hwfn,
						       p_hwfn->p_ooo_info,
						       cid,
						       iscsi_ooo->ooo_isle,
						       p_buffer,
						       QED_OOO_RIGHT_BUF);
				break;
			case TCP_EVENT_ADD_ISLE_LEFT:
				qed_ooo_add_new_buffer(p_hwfn,
						       p_hwfn->p_ooo_info,
						       cid,
						       iscsi_ooo->ooo_isle,
						       p_buffer,
						       QED_OOO_LEFT_BUF);
				break;
			case TCP_EVENT_JOIN:
				qed_ooo_add_new_buffer(p_hwfn,
						       p_hwfn->p_ooo_info,
						       cid,
						       iscsi_ooo->ooo_isle +
						       1,
						       p_buffer,
						       QED_OOO_LEFT_BUF);
				qed_ooo_join_isles(p_hwfn,
						   p_hwfn->p_ooo_info,
						   cid, iscsi_ooo->ooo_isle);
				break;
			case TCP_EVENT_ADD_PEN:
				num_ooo_add_to_peninsula++;
				qed_ooo_put_ready_buffer(p_hwfn->p_ooo_info,
							 p_buffer, true);
				break;
			}
		} else {
			DP_NOTICE(p_hwfn,
				  "Unexpected event (%d) TX OOO completion\n",
				  iscsi_ooo->ooo_opcode);
		}
	}

	return 0;
}

static void
qed_ooo_submit_tx_buffers(struct qed_hwfn *p_hwfn,
			  struct qed_ll2_info *p_ll2_conn)
{
	struct qed_ll2_tx_pkt_info tx_pkt;
	struct qed_ooo_buffer *p_buffer;
	dma_addr_t first_frag;
	u16 l4_hdr_offset_w;
	u8 bd_flags;
	int rc;

	/* Submit Tx buffers here */
	while ((p_buffer = qed_ooo_get_ready_buffer(p_hwfn->p_ooo_info))) {
		l4_hdr_offset_w = 0;
		bd_flags = 0;

		first_frag = p_buffer->rx_buffer_phys_addr +
		    p_buffer->placement_offset;
		SET_FIELD(bd_flags, CORE_TX_BD_DATA_FORCE_VLAN_MODE, 1);
		SET_FIELD(bd_flags, CORE_TX_BD_DATA_L4_PROTOCOL, 1);

		memset(&tx_pkt, 0, sizeof(tx_pkt));
		tx_pkt.num_of_bds = 1;
		tx_pkt.vlan = p_buffer->vlan;
		tx_pkt.bd_flags = bd_flags;
		tx_pkt.l4_hdr_offset_w = l4_hdr_offset_w;
		switch (p_ll2_conn->tx_dest) {
		case CORE_TX_DEST_NW:
			tx_pkt.tx_dest = QED_LL2_TX_DEST_NW;
			break;
		case CORE_TX_DEST_LB:
			tx_pkt.tx_dest = QED_LL2_TX_DEST_LB;
			break;
		case CORE_TX_DEST_DROP:
		default:
			tx_pkt.tx_dest = QED_LL2_TX_DEST_DROP;
			break;
		}
		tx_pkt.first_frag = first_frag;
		tx_pkt.first_frag_len = p_buffer->packet_length;
		tx_pkt.cookie = p_buffer;

		rc = qed_ll2_prepare_tx_packet(p_hwfn, p_ll2_conn->my_id,
					       &tx_pkt, true);
		if (rc) {
			qed_ooo_put_ready_buffer(p_hwfn->p_ooo_info,
						 p_buffer, false);
			break;
		}
	}
}

static void
qed_ooo_submit_rx_buffers(struct qed_hwfn *p_hwfn,
			  struct qed_ll2_info *p_ll2_conn)
{
	struct qed_ooo_buffer *p_buffer;
	int rc;

	while ((p_buffer = qed_ooo_get_free_buffer(p_hwfn->p_ooo_info))) {
		rc = qed_ll2_post_rx_buffer(p_hwfn,
					    p_ll2_conn->my_id,
					    p_buffer->rx_buffer_phys_addr,
					    0, p_buffer, true);
		if (rc) {
			qed_ooo_put_free_buffer(p_hwfn->p_ooo_info, p_buffer);
			break;
		}
	}
}

static int qed_ll2_lb_rxq_completion(struct qed_hwfn *p_hwfn, void *p_cookie)
{
	struct qed_ll2_info *p_ll2_conn = (struct qed_ll2_info *)p_cookie;
	int rc;

	if (!QED_LL2_RX_REGISTERED(p_ll2_conn))
		return 0;

	rc = qed_ll2_lb_rxq_handler(p_hwfn, p_ll2_conn);
	if (rc)
		return rc;

	qed_ooo_submit_rx_buffers(p_hwfn, p_ll2_conn);
	qed_ooo_submit_tx_buffers(p_hwfn, p_ll2_conn);

	return 0;
}

static int qed_ll2_lb_txq_completion(struct qed_hwfn *p_hwfn, void *p_cookie)
{
	struct qed_ll2_info *p_ll2_conn = (struct qed_ll2_info *)p_cookie;
	struct qed_ll2_tx_queue *p_tx = &p_ll2_conn->tx_queue;
	struct qed_ll2_tx_packet *p_pkt = NULL;
	struct qed_ooo_buffer *p_buffer;
	bool b_dont_submit_rx = false;
	u16 new_idx = 0, num_bds = 0;
	int rc;

	new_idx = le16_to_cpu(*p_tx->p_fw_cons);
	num_bds = ((s16) new_idx - (s16) p_tx->bds_idx);

	if (!num_bds)
		return 0;

	while (num_bds) {
		if (list_empty(&p_tx->active_descq))
			return -EINVAL;

		p_pkt = list_first_entry(&p_tx->active_descq,
					 struct qed_ll2_tx_packet, list_entry);
		if (!p_pkt)
			return -EINVAL;

		if (p_pkt->bd_used != 1) {
			DP_NOTICE(p_hwfn,
				  "Unexpectedly many BDs(%d) in TX OOO completion\n",
				  p_pkt->bd_used);
			return -EINVAL;
		}

		list_del(&p_pkt->list_entry);

		num_bds--;
		p_tx->bds_idx++;
		qed_chain_consume(&p_tx->txq_chain);

		p_buffer = (struct qed_ooo_buffer *)p_pkt->cookie;
		list_add_tail(&p_pkt->list_entry, &p_tx->free_descq);

		if (b_dont_submit_rx) {
			qed_ooo_put_free_buffer(p_hwfn->p_ooo_info, p_buffer);
			continue;
		}

		rc = qed_ll2_post_rx_buffer(p_hwfn, p_ll2_conn->my_id,
					    p_buffer->rx_buffer_phys_addr, 0,
					    p_buffer, true);
		if (rc) {
			qed_ooo_put_free_buffer(p_hwfn->p_ooo_info, p_buffer);
			b_dont_submit_rx = true;
		}
	}

	qed_ooo_submit_tx_buffers(p_hwfn, p_ll2_conn);

	return 0;
}

static int
qed_ll2_establish_connection_rx(struct qed_hwfn *p_hwfn,
				struct qed_ll2_info *p_ll2_conn)
{
	struct qed_sp_ll2_rx_queue_start_params params;
	int rc;

	if (!QED_LL2_RX_REGISTERED(p_ll2_conn))
		return 0;

	memset(&params, 0, sizeof(params));

	params.err_packet_too_big = p_ll2_conn->input.ai_err_packet_too_big;
	params.err_no_buf = p_ll2_conn->input.ai_err_no_buf;
	params.bd_base_addr = p_ll2_conn->rx_queue.rxq_chain.p_phys_addr;
	params.cid = p_ll2_conn->cid;
	params.cqe_pbl_addr =
	    qed_chain_get_pbl_phys(&p_ll2_conn->rx_queue.rcq_chain);
	params.drop_ttl0_flg = p_ll2_conn->input.rx_drop_ttl0_flg;
	params.gsi_enable = p_ll2_conn->input.gsi_enable;
	params.inner_vlan_stripping_en = p_ll2_conn->input.rx_vlan_removal_en;
	params.main_func_queue = p_ll2_conn->main_func_queue;

	/* Used for eCore clients that doesn't have L2 (like storage).
	 * In MF we need to set these bits so mcast/bcast will arrive to the
	 * ll2 queues (otherwise they will not arrive at all).
	 */
	if (test_bit(QED_MF_LL2_NON_UNICAST,
		     &p_hwfn->cdev->mf_bits) &&
	    params.main_func_queue &&
	    ((p_ll2_conn->input.conn_type != QED_LL2_TYPE_ROCE) &&
	     (p_ll2_conn->input.conn_type != QED_LL2_TYPE_IWARP))) {
		params.mf_si_bcast_accept_all = 1;
		params.mf_si_mcast_accept_all = 1;
	} else {
		params.mf_si_bcast_accept_all = 0;
		params.mf_si_mcast_accept_all = 0;
	}

	if (p_ll2_conn->input.conn_type == QED_LL2_TYPE_FCOE) {
		params.mf_si_bcast_accept_all = 0;
		params.mf_si_mcast_accept_all = 0;
	}

	params.mtu = p_ll2_conn->input.mtu;
	params.num_of_pbl_pages =
	    (u16) qed_chain_get_page_cnt(&p_ll2_conn->rx_queue.rcq_chain);

	params.opaque_fid = p_hwfn->hw_info.opaque_fid;

	if (test_bit(QED_MF_UFP_SPECIFIC, &p_hwfn->cdev->mf_bits) &&
	    (p_ll2_conn->input.conn_type == QED_LL2_TYPE_FCOE))
		params.report_outer_vlan = 1;

	params.queue_id = p_ll2_conn->queue_id;
	params.sb_id = qed_int_get_sp_sb_id(p_hwfn);
	params.sb_index = p_ll2_conn->rx_queue.rx_sb_index;

	/* assocaition of vport should be useful only for VFs */
	params.vport_id_valid = 0;
	params.vport_id = 0;

	rc = qed_sp_ll2_rx_queue_start(p_hwfn, &params);
	if (rc)
		return rc;

	p_ll2_conn->rx_queue.b_started = true;
	if (p_ll2_conn->rx_queue.ctx_based) {
		rc = qed_db_recovery_add(p_hwfn->cdev,
					 p_ll2_conn->rx_queue.set_prod_addr,
					 &p_ll2_conn->rx_queue.db_data,
					 DB_REC_WIDTH_64B, DB_REC_KERNEL);
	}

	return rc;
}

int
qed_sp_ll2_rx_queue_start(struct qed_hwfn *p_hwfn,
			  struct qed_sp_ll2_rx_queue_start_params *params)
{
	struct core_rx_start_ramrod_data *p_ramrod = NULL;
	struct qed_spq_entry *p_ent = NULL;
	struct qed_sp_init_data init_data;
	int rc = 0;

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = params->cid;
	init_data.opaque_fid = params->opaque_fid;
	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	rc = qed_sp_init_request(p_hwfn, &p_ent,
				 CORE_RAMROD_RX_QUEUE_START,
				 PROTOCOLID_CORE, &init_data);
	if (rc)
		return rc;

	p_ramrod = &p_ent->ramrod.core_rx_queue_start;

	p_ramrod->sb_id = cpu_to_le16(params->sb_id);
	p_ramrod->sb_index = params->sb_index;
	p_ramrod->complete_event_flg = 1;

	p_ramrod->mtu = cpu_to_le16(params->mtu);
	DMA_REGPAIR_LE(p_ramrod->bd_base, params->bd_base_addr);

	p_ramrod->num_of_pbl_pages = cpu_to_le16(params->num_of_pbl_pages);
	DMA_REGPAIR_LE(p_ramrod->cqe_pbl_addr, params->cqe_pbl_addr);

	p_ramrod->drop_ttl0_flg = params->drop_ttl0_flg;
	p_ramrod->inner_vlan_stripping_en = params->inner_vlan_stripping_en;
	p_ramrod->report_outer_vlan = params->report_outer_vlan;

	p_ramrod->queue_id = params->queue_id;
	p_ramrod->main_func_queue = params->main_func_queue;
	p_ramrod->zero_prod_flg = 1;

	p_ramrod->mf_si_bcast_accept_all = params->mf_si_bcast_accept_all;
	p_ramrod->mf_si_mcast_accept_all = params->mf_si_mcast_accept_all;

	SET_FIELD(p_ramrod->action_on_error.error_type,
		  CORE_RX_ACTION_ON_ERROR_PACKET_TOO_BIG,
		  qed_ll2_get_error_choice(params->err_packet_too_big));

	SET_FIELD(p_ramrod->action_on_error.error_type,
		  CORE_RX_ACTION_ON_ERROR_NO_BUFF,
		  qed_ll2_get_error_choice(params->err_no_buf));

	p_ramrod->gsi_offload_flag = params->gsi_enable;

	p_ramrod->vport_id_valid = params->vport_id_valid;
	p_ramrod->vport_id = params->vport_id;

	return qed_spq_post(p_hwfn, p_ent, NULL);
}

static int
qed_ll2_establish_connection_tx(struct qed_hwfn *p_hwfn,
				struct qed_ll2_info *p_ll2_conn)
{
	struct qed_ll2_tx_queue *p_tx = &p_ll2_conn->tx_queue;
	struct qed_sp_ll2_tx_queue_start_params params;
	int rc = -EOPNOTSUPP;

	if (!QED_LL2_TX_REGISTERED(p_ll2_conn))
		return 0;

	memset(&params, 0, sizeof(params));

	params.cid = p_ll2_conn->cid;
	params.conn_type = p_ll2_conn->input.conn_type;
	params.gsi_enable = p_ll2_conn->input.gsi_enable;
	params.mtu = p_ll2_conn->input.mtu;
	params.opaque_fid = p_hwfn->hw_info.opaque_fid;
	params.pbl_addr = qed_chain_get_pbl_phys(&p_tx->txq_chain);
	params.pbl_size = (u16) qed_chain_get_page_cnt(&p_tx->txq_chain);
	params.pq_id = qed_get_cm_pq_idx_ll2(p_hwfn, p_ll2_conn->input.tx_tc);
	params.sb_id = qed_int_get_sp_sb_id(p_hwfn);
	params.sb_index = p_tx->tx_sb_index;
	params.stats_en = p_ll2_conn->tx_stats_en;
	params.stats_id = p_ll2_conn->tx_stats_id;

	if (p_ll2_conn->input.tx_vport_offset > RESC_NUM(p_hwfn, QED_VPORT)) {
		DP_ERR(p_hwfn, "tx_vport_offset %d exceeded vports %d\n",
		       p_ll2_conn->input.tx_vport_offset,
		       RESC_NUM(p_hwfn, QED_VPORT));
		return -EINVAL;
	}

	/* For ll2, vport id is relevant only for roce tx connections. Here
	 * vport is used for tx-switching classification check by the FW.
	 * By default qed will use first vport of the PF, i.e. the default
	 * vport. Client can provide a different value, within the PF’s vport
	 * range, if necessary.
	 */
	if (p_ll2_conn->input.conn_type == QED_LL2_TYPE_ROCE) {
		params.vport_id_valid = 1;
		params.vport_id = (u8) RESC_START(p_hwfn, QED_VPORT) +
		    p_ll2_conn->input.tx_vport_offset;
	}

	rc = qed_sp_ll2_tx_queue_start(p_hwfn, &params);
	if (rc)
		return rc;

	p_ll2_conn->tx_queue.b_started = true;
	rc = qed_db_recovery_add(p_hwfn->cdev,
				 p_tx->doorbell_addr,
				 &p_tx->db_msg,
				 DB_REC_WIDTH_32B, DB_REC_KERNEL);

	return rc;
}

int
qed_sp_ll2_tx_queue_start(struct qed_hwfn *p_hwfn,
			  struct qed_sp_ll2_tx_queue_start_params *params)
{
	struct core_tx_start_ramrod_data *p_ramrod = NULL;
	struct qed_spq_entry *p_ent = NULL;
	struct qed_sp_init_data init_data;
	int rc = -EOPNOTSUPP;

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = params->cid;
	init_data.opaque_fid = params->opaque_fid;
	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	rc = qed_sp_init_request(p_hwfn, &p_ent,
				 CORE_RAMROD_TX_QUEUE_START,
				 PROTOCOLID_CORE, &init_data);
	if (rc)
		return rc;

	p_ramrod = &p_ent->ramrod.core_tx_queue_start;

	p_ramrod->sb_id = cpu_to_le16(params->sb_id);
	p_ramrod->sb_index = params->sb_index;
	p_ramrod->mtu = cpu_to_le16(params->mtu);
	p_ramrod->stats_en = params->stats_en;
	p_ramrod->stats_id = params->stats_id;

	DMA_REGPAIR_LE(p_ramrod->pbl_base_addr, params->pbl_addr);
	p_ramrod->pbl_size = cpu_to_le16(params->pbl_size);

	p_ramrod->qm_pq_id = cpu_to_le16(params->pq_id);

	switch (params->conn_type) {
	case QED_LL2_TYPE_FCOE:
		p_ramrod->conn_type = PROTOCOLID_FCOE;
		break;
	case QED_LL2_TYPE_ISCSI:
		p_ramrod->conn_type = PROTOCOLID_ISCSI;
		break;
	case QED_LL2_TYPE_ROCE:
		p_ramrod->conn_type = PROTOCOLID_ROCE;
		break;
	case QED_LL2_TYPE_IWARP:
		p_ramrod->conn_type = PROTOCOLID_IWARP;
		break;
	case QED_LL2_TYPE_OOO:
		if (p_hwfn->hw_info.personality == QED_PCI_ISCSI)
			p_ramrod->conn_type = PROTOCOLID_ISCSI;
		else
			p_ramrod->conn_type = PROTOCOLID_IWARP;
		break;
	default:
		p_ramrod->conn_type = PROTOCOLID_ETH;
		DP_NOTICE(p_hwfn, "Unknown connection type: %d\n",
			  params->conn_type);
	}

	p_ramrod->gsi_offload_flag = params->gsi_enable;
	p_ramrod->vport_id_valid = params->vport_id_valid;
	p_ramrod->vport_id = params->vport_id;

	rc = qed_spq_post(p_hwfn, p_ent, NULL);

	return rc;
}

int
qed_sp_ll2_tx_queue_update(struct qed_hwfn *p_hwfn,
			   struct qed_sp_ll2_tx_queue_update_params *params)
{
	struct core_tx_update_ramrod_data *p_ramrod = NULL;
	struct qed_spq_entry *p_ent = NULL;
	int rc = -EOPNOTSUPP;
	struct qed_sp_init_data init_data;

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = params->cid;
	init_data.opaque_fid = params->opaque_fid;
	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	rc = qed_sp_init_request(p_hwfn, &p_ent,
				 CORE_RAMROD_TX_QUEUE_UPDATE,
				 PROTOCOLID_CORE, &init_data);
	if (rc)
		return rc;

	p_ramrod = &p_ent->ramrod.core_tx_queue_update;
	if (params->update_flag & QED_UPDATE_LL2_TXQ_PQ) {
		p_ramrod->qm_pq_id = cpu_to_le16(params->pq_id);
		p_ramrod->update_qm_pq_id_flg = true;
	} else if (params->update_flag & QED_UPDATE_LL2_TXQ_PQ_SET) {
		u16 pq_id;

		qed_qm_acquire_access(p_hwfn);
		pq_id = qed_get_cm_pq_idx_tc(p_hwfn,
					     PQ_FLAGS_OFLD,
					     params->pq_set_id, params->tc);
		qed_qm_release_access(p_hwfn);
		p_ramrod->qm_pq_id = cpu_to_le16(pq_id);
		p_ramrod->update_qm_pq_id_flg = true;
	}

	rc = qed_spq_post(p_hwfn, p_ent, NULL);

	return rc;
}

static int
qed_ll2_terminate_connection_rx(struct qed_hwfn *p_hwfn,
				struct qed_ll2_info *p_ll2_conn)
{
	struct qed_sp_ll2_rx_queue_stop_params params;

	if (!p_ll2_conn->rx_queue.b_started)
		return 0;

	if (p_ll2_conn->rx_queue.ctx_based)
		qed_db_recovery_del(p_hwfn->cdev,
				    p_ll2_conn->rx_queue.set_prod_addr,
				    &p_ll2_conn->rx_queue.db_data);

	memset(&params, 0, sizeof(params));
	params.cid = p_ll2_conn->cid;
	params.opaque_fid = p_hwfn->hw_info.opaque_fid;
	params.queue_id = p_ll2_conn->queue_id;

	p_ll2_conn->rx_queue.b_started = false;

	return qed_sp_ll2_rx_queue_stop(p_hwfn, &params);
}

int
qed_sp_ll2_rx_queue_stop(struct qed_hwfn *p_hwfn,
			 struct qed_sp_ll2_rx_queue_stop_params *params)
{
	struct core_rx_stop_ramrod_data *p_ramrod = NULL;
	struct qed_spq_entry *p_ent = NULL;
	struct qed_sp_init_data init_data;
	int rc = -EOPNOTSUPP;

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = params->cid;
	init_data.opaque_fid = params->opaque_fid;
	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	rc = qed_sp_init_request(p_hwfn, &p_ent,
				 CORE_RAMROD_RX_QUEUE_STOP,
				 PROTOCOLID_CORE, &init_data);
	if (rc)
		return rc;

	p_ramrod = &p_ent->ramrod.core_rx_queue_stop;

	p_ramrod->complete_event_flg = 1;
	p_ramrod->queue_id = params->queue_id;

	return qed_spq_post(p_hwfn, p_ent, NULL);
}

static int
qed_ll2_terminate_connection_tx(struct qed_hwfn *p_hwfn,
				struct qed_ll2_info *p_ll2_conn)
{
	struct qed_sp_ll2_tx_queue_stop_params params;

	if (!p_ll2_conn->tx_queue.b_started)
		return 0;

	qed_db_recovery_del(p_hwfn->cdev, p_ll2_conn->tx_queue.doorbell_addr,
			    &p_ll2_conn->tx_queue.db_msg);

	memset(&params, 0, sizeof(params));
	params.cid = p_ll2_conn->cid;
	params.opaque_fid = p_hwfn->hw_info.opaque_fid;

	p_ll2_conn->tx_queue.b_started = false;

	return qed_sp_ll2_tx_queue_stop(p_hwfn, &params);
}

int
qed_sp_ll2_tx_queue_stop(struct qed_hwfn *p_hwfn,
			 struct qed_sp_ll2_tx_queue_stop_params *params)
{
	struct qed_spq_entry *p_ent = NULL;
	struct qed_sp_init_data init_data;
	int rc = -EOPNOTSUPP;

	/* Get SPQ entry */
	memset(&init_data, 0, sizeof(init_data));
	init_data.cid = params->cid;
	init_data.opaque_fid = params->opaque_fid;
	init_data.comp_mode = QED_SPQ_MODE_EBLOCK;

	rc = qed_sp_init_request(p_hwfn, &p_ent,
				 CORE_RAMROD_TX_QUEUE_STOP,
				 PROTOCOLID_CORE, &init_data);
	if (rc)
		return rc;

	return qed_spq_post(p_hwfn, p_ent, NULL);
}

static int
qed_ll2_acquire_connection_rx(struct qed_hwfn *p_hwfn,
			      struct qed_ll2_info *p_ll2_info)
{
	struct qed_chain_params chain_params;
	struct qed_ll2_rx_packet *p_descq;
	u32 capacity;
	int rc = 0;

	if (!p_ll2_info->input.rx_num_desc)
		goto out;

	qed_chain_params_init(&chain_params,
			      QED_CHAIN_USE_TO_CONSUME_PRODUCE,
			      QED_CHAIN_MODE_NEXT_PTR,
			      QED_CHAIN_CNT_TYPE_U16,
			      p_ll2_info->input.rx_num_desc,
			      sizeof(struct core_rx_bd));
	rc = qed_chain_alloc(p_hwfn->cdev, &p_ll2_info->rx_queue.rxq_chain,
			     &chain_params);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed to allocate ll2 rxq chain\n");
		goto out;
	}

	capacity = qed_chain_get_capacity(&p_ll2_info->rx_queue.rxq_chain);
	p_descq = kzalloc(capacity * sizeof(struct qed_ll2_rx_packet),
			  GFP_KERNEL);
	if (!p_descq) {
		rc = -ENOMEM;
		DP_NOTICE(p_hwfn, "Failed to allocate ll2 Rx desc\n");
		goto out;
	}
	p_ll2_info->rx_queue.descq_array = p_descq;

	qed_chain_params_init(&chain_params,
			      QED_CHAIN_USE_TO_CONSUME_PRODUCE,
			      QED_CHAIN_MODE_PBL,
			      QED_CHAIN_CNT_TYPE_U16,
			      p_ll2_info->input.rx_num_desc,
			      sizeof(struct core_rx_fast_path_cqe));
	rc = qed_chain_alloc(p_hwfn->cdev, &p_ll2_info->rx_queue.rcq_chain,
			     &chain_params);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed to allocate ll2 rcq chain\n");
		goto out;
	}

	DP_VERBOSE(p_hwfn, QED_MSG_LL2,
		   "Allocated LL2 Rxq [Type %08x] with 0x%08x buffers\n",
		   p_ll2_info->input.conn_type, p_ll2_info->input.rx_num_desc);

out:
	return rc;
}

static int
qed_ll2_acquire_connection_tx(struct qed_hwfn *p_hwfn,
			      struct qed_ll2_info *p_ll2_info)
{
	struct qed_chain_params chain_params;
	struct qed_ll2_tx_packet *p_descq;
	u32 capacity;
	int rc = 0;
	u32 desc_size;

	if (!p_ll2_info->input.tx_num_desc)
		goto out;

	qed_chain_params_init(&chain_params,
			      QED_CHAIN_USE_TO_CONSUME_PRODUCE,
			      QED_CHAIN_MODE_PBL,
			      QED_CHAIN_CNT_TYPE_U16,
			      p_ll2_info->input.tx_num_desc,
			      sizeof(struct core_tx_bd));
	rc = qed_chain_alloc(p_hwfn->cdev, &p_ll2_info->tx_queue.txq_chain,
			     &chain_params);
	if (rc)
		goto out;

	capacity = qed_chain_get_capacity(&p_ll2_info->tx_queue.txq_chain);
	desc_size = (sizeof(*p_descq) +
		     (p_ll2_info->input.tx_max_bds_per_packet - 1) *
		     sizeof(p_descq->bds_set));

	p_descq = kzalloc(capacity * desc_size, GFP_KERNEL);
	if (!p_descq) {
		rc = -ENOMEM;
		goto out;
	}
	p_ll2_info->tx_queue.descq_array = p_descq;

	DP_VERBOSE(p_hwfn, QED_MSG_LL2,
		   "Allocated LL2 Txq [Type %08x] with 0x%08x buffers\n",
		   p_ll2_info->input.conn_type, p_ll2_info->input.tx_num_desc);

out:	if (rc)
		DP_NOTICE(p_hwfn,
			  "Can't allocate memory for Tx LL2 with 0x%08x buffers\n",
			  p_ll2_info->input.tx_num_desc);
	return rc;
}

static int
qed_ll2_acquire_connection_ooo(struct qed_hwfn *p_hwfn,
			       struct qed_ll2_info *p_ll2_info, u16 mtu)
{
	struct qed_ooo_buffer *p_buf = NULL;
	u32 rx_buffer_size = 0;
	void *p_virt;
	u16 buf_idx;
	int rc = 0;

	if (p_ll2_info->input.conn_type != QED_LL2_TYPE_OOO)
		return rc;

	/* Correct number of requested OOO buffers if needed */
	if (!p_ll2_info->input.rx_num_ooo_buffers) {
		u16 num_desc = p_ll2_info->input.rx_num_desc;

		if (!num_desc)
			return -EINVAL;
		p_ll2_info->input.rx_num_ooo_buffers = num_desc * 2;
	}

	/* TODO - use some defines for buffer size */
	rx_buffer_size = mtu + 14 + 4 + 8 + ETH_CACHE_LINE_SIZE;
	rx_buffer_size = (rx_buffer_size + ETH_CACHE_LINE_SIZE - 1) &
	    ~(ETH_CACHE_LINE_SIZE - 1);

	for (buf_idx = 0; buf_idx < p_ll2_info->input.rx_num_ooo_buffers;
	     buf_idx++) {
		p_buf = kzalloc(sizeof(*p_buf), GFP_KERNEL);
		if (!p_buf) {
			DP_NOTICE(p_hwfn,
				  "Failed to allocate ooo descriptor\n");
			rc = -ENOMEM;
			goto out;
		}

		p_buf->rx_buffer_size = rx_buffer_size;
		p_virt = dma_alloc_coherent(&p_hwfn->cdev->pdev->dev,
					    p_buf->rx_buffer_size,
					    &p_buf->rx_buffer_phys_addr,
					    GFP_KERNEL);
		if (!p_virt) {
			DP_NOTICE(p_hwfn, "Failed to allocate ooo buffer\n");
			kfree(p_buf);
			rc = -ENOMEM;
			goto out;
		}
		p_buf->rx_buffer_virt_addr = p_virt;
		qed_ooo_put_free_buffer(p_hwfn->p_ooo_info, p_buf);
	}

	DP_VERBOSE(p_hwfn, QED_MSG_LL2,
		   "Allocated [%04x] LL2 OOO buffers [each of size 0x%08x]\n",
		   p_ll2_info->input.rx_num_ooo_buffers, rx_buffer_size);

out:
	return rc;
}

static int
qed_ll2_set_cbs(struct qed_ll2_info *p_ll2_info, const struct qed_ll2_cbs *cbs)
{
	if (!cbs || (!cbs->rx_comp_cb ||
		     !cbs->rx_release_cb ||
		     !cbs->tx_comp_cb || !cbs->tx_release_cb || !cbs->cookie))
		return -EINVAL;

	p_ll2_info->cbs.rx_comp_cb = cbs->rx_comp_cb;
	p_ll2_info->cbs.rx_release_cb = cbs->rx_release_cb;
	p_ll2_info->cbs.tx_comp_cb = cbs->tx_comp_cb;
	p_ll2_info->cbs.tx_release_cb = cbs->tx_release_cb;
	p_ll2_info->cbs.slowpath_cb = cbs->slowpath_cb;
	p_ll2_info->cbs.cookie = cbs->cookie;

	return 0;
}

static void _qed_ll2_calc_allowed_conns(struct qed_hwfn *p_hwfn,
					struct qed_ll2_acquire_data *data,
					u8 * start_idx, u8 * last_idx)
{
	/* LL2 queues handles will be split as follows:
	 * for PF - first will be the legacy queues, and then the ctx based.
	 * for VF - it will always start from 0 and will have its own max
	 * allowed connections.
	 */
	if (IS_PF(p_hwfn->cdev)) {
		if (data->input.rx_conn_type == QED_LL2_RX_TYPE_LEGACY) {
			*start_idx = QED_LL2_LEGACY_CONN_BASE_PF;
			*last_idx = *start_idx +
			    QED_MAX_NUM_OF_LEGACY_LL2_CONNS_PF;
		} else {
			/* QED_LL2_RX_TYPE_CTX */
			*start_idx = QED_LL2_CTX_CONN_BASE_PF;
			*last_idx = *start_idx +
			    QED_MAX_NUM_OF_CTX_LL2_CONNS_PF;
		}
	} else {
		/* VF */
		*start_idx = 0;
		*last_idx = *start_idx + QED_MAX_NUM_OF_LL2_CONNS_VF;
	}
}

static void qed_ll2_register_cb(void *cxt, struct qed_ll2_info *p_ll2_info)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)cxt;
	qed_int_comp_cb_t comp_rx_cb, comp_tx_cb;
	struct qed_sb_info *p_sb;
	u8 vf_sb_id;

	/* Register callbacks for the Rx/Tx queues */
	if (p_ll2_info->input.conn_type == QED_LL2_TYPE_OOO) {
		comp_rx_cb = qed_ll2_lb_rxq_completion;
		comp_tx_cb = qed_ll2_lb_txq_completion;
	} else {
		comp_rx_cb = qed_ll2_rxq_completion;
		comp_tx_cb = qed_ll2_txq_completion;
	}

	if (p_ll2_info->input.rx_num_desc) {
		if (IS_PF(p_hwfn->cdev)) {
			qed_int_register_cb(p_hwfn, comp_rx_cb, p_ll2_info,
					    &p_ll2_info->rx_queue.rx_sb_index,
					    &p_ll2_info->rx_queue.p_fw_cons);
		} else {
			/* VF LL2 will use the CNQ's SB */
			vf_sb_id = qed_vf_rdma_cnq_sb_start_id(p_hwfn);
			p_sb = p_hwfn->vf_iov_info->sbs_info[vf_sb_id];

			p_ll2_info->rx_queue.p_fw_cons =
			    &p_sb->sb_pi_array[LL2_VF_RX_PI];
			*p_ll2_info->rx_queue.p_fw_cons = 0;
		}

		p_ll2_info->rx_queue.b_cb_registered = true;
	}

	if (p_ll2_info->input.tx_num_desc) {
		if (IS_PF(p_hwfn->cdev)) {
			qed_int_register_cb(p_hwfn, comp_tx_cb, p_ll2_info,
					    &p_ll2_info->tx_queue.tx_sb_index,
					    &p_ll2_info->tx_queue.p_fw_cons);
		} else {
			/* VF LL2 will use the CNQ's SB */
			vf_sb_id = qed_vf_rdma_cnq_sb_start_id(p_hwfn);
			p_sb = p_hwfn->vf_iov_info->sbs_info[vf_sb_id];

			p_ll2_info->tx_queue.p_fw_cons =
			    &p_sb->sb_pi_array[LL2_VF_TX_PI];
			*p_ll2_info->tx_queue.p_fw_cons = 0;
		}

		p_ll2_info->tx_queue.b_cb_registered = true;
	}
}

int qed_ll2_acquire_connection(void *cxt, struct qed_ll2_acquire_data *data)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)cxt;
	struct qed_ll2_info *p_ll2_info = NULL;
	u8 i, first_idx, last_idx, *p_tx_max;
	int rc;

	_qed_ll2_calc_allowed_conns(p_hwfn, data, &first_idx, &last_idx);

	if (!data->p_connection_handle || !p_hwfn->p_ll2_info) {
		DP_NOTICE(p_hwfn,
			  "Invalid connection handle, ll2_info not allocated\n");
		return -EINVAL;
	}

	for (i = first_idx; i < last_idx; i++) {
		mutex_lock(&p_hwfn->p_ll2_info[i].mutex);
		if (p_hwfn->p_ll2_info[i].b_active) {
			mutex_unlock(&p_hwfn->p_ll2_info[i].mutex);
			continue;
		}

		p_hwfn->p_ll2_info[i].b_active = true;
		p_ll2_info = &p_hwfn->p_ll2_info[i];
		mutex_unlock(&p_hwfn->p_ll2_info[i].mutex);
		break;
	}
	if (p_ll2_info == NULL) {
		DP_NOTICE(p_hwfn, "No available ll2 connection\n");
		return -EBUSY;
	}

	memcpy(&p_ll2_info->input, &data->input, sizeof(p_ll2_info->input));

	switch (data->input.tx_dest) {
	case QED_LL2_TX_DEST_NW:
		p_ll2_info->tx_dest = CORE_TX_DEST_NW;
		break;
	case QED_LL2_TX_DEST_LB:
		p_ll2_info->tx_dest = CORE_TX_DEST_LB;
		break;
	case QED_LL2_TX_DEST_DROP:
		p_ll2_info->tx_dest = CORE_TX_DEST_DROP;
		break;
	default:
		return -EINVAL;
	}

	if ((data->input.conn_type == QED_LL2_TYPE_OOO) ||
	    data->input.secondary_queue)
		p_ll2_info->main_func_queue = false;
	else
		p_ll2_info->main_func_queue = true;

	p_ll2_info->tx_stats_en =
	    (data->input.conn_type == QED_LL2_TYPE_OOO) ? 0 : 1;

	/* Correct maximum number of Tx BDs */
	p_tx_max = &p_ll2_info->input.tx_max_bds_per_packet;
	if (*p_tx_max == 0)
		*p_tx_max = CORE_LL2_TX_MAX_BDS_PER_PACKET;
	else
		*p_tx_max = min_t(u8, *p_tx_max,
				  CORE_LL2_TX_MAX_BDS_PER_PACKET);

	rc = qed_ll2_set_cbs(p_ll2_info, data->cbs);
	if (rc) {
		DP_NOTICE(p_hwfn, "Invalid callback functions\n");
		goto q_allocate_fail;
	}

	rc = qed_ll2_acquire_connection_rx(p_hwfn, p_ll2_info);
	if (rc) {
		DP_NOTICE(p_hwfn, "ll2 acquire rx connection failed\n");
		goto q_allocate_fail;
	}

	rc = qed_ll2_acquire_connection_tx(p_hwfn, p_ll2_info);
	if (rc) {
		DP_NOTICE(p_hwfn, "ll2 acquire tx connection failed\n");
		goto q_allocate_fail;
	}

	rc = qed_ll2_acquire_connection_ooo(p_hwfn, p_ll2_info,
					    data->input.mtu);
	if (rc) {
		DP_NOTICE(p_hwfn, "ll2 acquire ooo connection failed\n");
		goto q_allocate_fail;
	}

	*(data->p_connection_handle) = i;
	return rc;

q_allocate_fail:
	qed_ll2_release_connection(p_hwfn, i);
	return -ENOMEM;
}

static void
qed_ll2_establish_connection_ooo(struct qed_hwfn *p_hwfn,
				 struct qed_ll2_info *p_ll2_conn)
{
	if (p_ll2_conn->input.conn_type != QED_LL2_TYPE_OOO)
		return;

	qed_ooo_release_all_isles(p_hwfn->p_ooo_info);
	qed_ooo_submit_rx_buffers(p_hwfn, p_ll2_conn);
}

int qed_ll2_establish_connection(void *cxt, u8 connection_handle)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)cxt;
	struct qed_ll2_tx_packet *p_pkt;
	struct qed_ll2_info *p_ll2_conn;
	struct core_conn_context *p_cxt;
	struct qed_ll2_rx_queue *p_rx;
	struct qed_ll2_tx_queue *p_tx;
	struct qed_cxt_info cxt_info;
	struct qed_ptt *p_ptt;
	int rc = -EOPNOTSUPP;
	u32 i, capacity;
	u8 qid, stats_id;
	u32 desc_size;

	p_ll2_conn = qed_ll2_handle_sanity_lock(p_hwfn, connection_handle);
	if (p_ll2_conn == NULL) {
		rc = -EINVAL;
		return rc;
	}

	p_rx = &p_ll2_conn->rx_queue;
	p_tx = &p_ll2_conn->tx_queue;

	qed_chain_reset(&p_rx->rxq_chain);
	qed_chain_reset(&p_rx->rcq_chain);
	INIT_LIST_HEAD(&p_rx->active_descq);
	INIT_LIST_HEAD(&p_rx->free_descq);
	INIT_LIST_HEAD(&p_rx->posting_descq);
	spin_lock_init(&p_rx->lock);
	p_ll2_conn->rx_queue.b_started = false;
	capacity = qed_chain_get_capacity(&p_rx->rxq_chain);
	for (i = 0; i < capacity; i++)
		list_add_tail(&p_rx->descq_array[i].list_entry,
			      &p_rx->free_descq);

	qed_chain_reset(&p_tx->txq_chain);
	INIT_LIST_HEAD(&p_tx->active_descq);
	INIT_LIST_HEAD(&p_tx->free_descq);
	INIT_LIST_HEAD(&p_tx->sending_descq);
	spin_lock_init(&p_tx->lock);
	p_ll2_conn->tx_queue.b_started = false;
	capacity = qed_chain_get_capacity(&p_tx->txq_chain);
	/* The size of the element in descq_array is flexible */
	desc_size = (sizeof(*p_pkt) +
		     (p_ll2_conn->input.tx_max_bds_per_packet - 1) *
		     sizeof(p_pkt->bds_set));

	for (i = 0; i < capacity; i++) {
		p_pkt = (struct qed_ll2_tx_packet *)((u8 *) p_tx->descq_array +
						     desc_size * i);
		list_add_tail(&p_pkt->list_entry, &p_tx->free_descq);
	}
	p_tx->cur_completing_bd_idx = 0;
	p_tx->bds_idx = 0;
	p_tx->b_completing_packet = false;
	p_tx->cur_send_packet = NULL;
	p_tx->cur_send_frag_num = 0;
	p_tx->cur_completing_frag_num = 0;

	qed_ll2_register_cb(cxt, p_ll2_conn);

	if (IS_VF(p_hwfn->cdev)) {
		rc = qed_vf_pf_establish_ll2_conn(p_hwfn, connection_handle);
		return rc;
	}

	rc = qed_cxt_acquire_cid(p_hwfn, PROTOCOLID_CORE, &p_ll2_conn->cid);
	if (rc)
		return rc;

	cxt_info.iid = p_ll2_conn->cid;
	rc = qed_cxt_get_cid_info(p_hwfn, &cxt_info);
	if (rc) {
		DP_NOTICE(p_hwfn, "Cannot find context info for cid=%d\n",
			  p_ll2_conn->cid);
		return rc;
	}

	p_cxt = cxt_info.p_cxt;

	/* @@@TBD we zero the context until we have ilt_reset implemented. */
	memset(p_cxt, 0, sizeof(*p_cxt));

	qid = qed_ll2_handle_to_queue_id(p_hwfn, connection_handle,
					 p_ll2_conn->input.rx_conn_type,
					 QED_CXT_PF_CID);

	if (qid == QED_LL2_INVALID_QID)
		return -EINVAL;

	stats_id = qed_ll2_handle_to_stats_id(p_hwfn,
					      p_ll2_conn->input.rx_conn_type,
					      qid);

	p_ll2_conn->queue_id = qid;
	p_ll2_conn->tx_stats_id = stats_id;

	/* If there is no valid stats id for this connection, disable stats */
	if (p_ll2_conn->tx_stats_id == QED_LL2_INVALID_STATS_ID) {
		p_ll2_conn->tx_stats_en = 0;
		DP_VERBOSE(p_hwfn,
			   QED_MSG_LL2,
			   "Disabling stats for queue %d - not enough counters\n",
			   qid);
	}

	DP_VERBOSE(p_hwfn,
		   QED_MSG_LL2,
		   "Establishing ll2 queue. PF %d ctx_bsaed=%d abs qid=%d stats_id=%d\n",
		   p_hwfn->rel_pf_id,
		   p_ll2_conn->input.rx_conn_type, qid, stats_id);

	if (p_ll2_conn->input.rx_conn_type == QED_LL2_RX_TYPE_LEGACY) {
		p_rx->set_prod_addr =
		    (u8 __iomem *) p_hwfn->regview +
		    GET_GTT_REG_ADDR(GTT_BAR0_MAP_REG_TSDM_RAM,
				     TSTORM_LL2_RX_PRODS, qid);
	} else {
		/* QED_LL2_RX_TYPE_CTX - using doorbell */
		p_rx->ctx_based = 1;

		p_rx->set_prod_addr = (u8 __iomem *) p_hwfn->doorbells + p_hwfn->dpi_info.dpi_start_offset +	/* using dpi 0 */
		    DB_ADDR_SHIFT(DQ_PWM_OFFSET_TCM_LL2_PROD_UPDATE);

		/* prepare db data */
		p_rx->db_data.icid = cpu_to_le16((u16) p_ll2_conn->cid);
		SET_FIELD(p_rx->db_data.params,
			  CORE_PWM_PROD_UPDATE_DATA_AGG_CMD, DB_AGG_CMD_SET);
		SET_FIELD(p_rx->db_data.params,
			  CORE_PWM_PROD_UPDATE_DATA_RESERVED1, 0);
	}

	p_tx->doorbell_addr = (u8 __iomem *) p_hwfn->doorbells +
	    DB_ADDR(p_ll2_conn->cid, DQ_DEMS_LEGACY);

	/* prepare db data */
	SET_FIELD(p_tx->db_msg.params, CORE_DB_DATA_DEST, DB_DEST_XCM);
	SET_FIELD(p_tx->db_msg.params, CORE_DB_DATA_AGG_CMD, DB_AGG_CMD_SET);
	SET_FIELD(p_tx->db_msg.params, CORE_DB_DATA_AGG_VAL_SEL,
		  DQ_XCM_CORE_TX_BD_PROD_CMD);
	p_tx->db_msg.agg_flags = DQ_XCM_CORE_DQ_CF_CMD;

	rc = qed_ll2_establish_connection_rx(p_hwfn, p_ll2_conn);
	if (rc)
		goto err;

	rc = qed_ll2_establish_connection_tx(p_hwfn, p_ll2_conn);
	if (rc)
		goto err;

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt) {
		rc = -EAGAIN;
		goto err;
	}

	if (!QED_IS_RDMA_PERSONALITY(p_hwfn))
		qed_wr(p_hwfn, p_ptt, PRS_REG_USE_LIGHT_L2, 1);

	qed_ll2_establish_connection_ooo(p_hwfn, p_ll2_conn);

	if (p_ll2_conn->input.conn_type == QED_LL2_TYPE_FCOE) {
		if (!test_bit(QED_MF_UFP_SPECIFIC, &p_hwfn->cdev->mf_bits))
			qed_llh_add_protocol_filter(p_hwfn->cdev, 0,
						    QED_LLH_FILTER_ETHERTYPE,
						    0x8906, 0);
		qed_llh_add_protocol_filter(p_hwfn->cdev, 0,
					    QED_LLH_FILTER_ETHERTYPE,
					    0x8914, 0);
	}

	qed_ptt_release(p_hwfn, p_ptt);

	return 0;
err:
	qed_ll2_terminate_connection(p_hwfn, connection_handle);

	return rc;
}

static void qed_ll2_post_rx_buffer_notify_fw(struct qed_hwfn *p_hwfn,
					     struct qed_ll2_rx_queue *p_rx,
					     struct qed_ll2_rx_packet *p_curp)
{
	struct qed_ll2_rx_packet *p_posting_packet = NULL;
	struct core_ll2_rx_prod rx_prod = { 0, 0 };
	bool b_notify_fw = false;
	u16 bd_prod, cq_prod;

	/* This handles the flushing of already posted buffers */
	while (!list_empty(&p_rx->posting_descq)) {
		p_posting_packet = list_first_entry(&p_rx->posting_descq,
						    struct qed_ll2_rx_packet,
						    list_entry);
		list_del(&p_posting_packet->list_entry);
		list_add_tail(&p_posting_packet->list_entry,
			      &p_rx->active_descq);
		b_notify_fw = true;
	}

	/* This handles the supplied packet [if there is one] */
	if (p_curp) {
		list_add_tail(&p_curp->list_entry, &p_rx->active_descq);
		b_notify_fw = true;
	}

	if (!b_notify_fw)
		return;

	bd_prod = qed_chain_get_prod_idx(&p_rx->rxq_chain);
	cq_prod = qed_chain_get_prod_idx(&p_rx->rcq_chain);
	if (p_rx->ctx_based) {
		/* update producer by giving a doorbell */
		p_rx->db_data.prod.bd_prod = cpu_to_le16(bd_prod);
		p_rx->db_data.prod.cqe_prod = cpu_to_le16(cq_prod);
		DIRECT_REG_WR64(p_rx->set_prod_addr,
				*((u64 *) & p_rx->db_data));
	} else {
		/* update producer by writing to internal RAM */
		rx_prod.bd_prod = cpu_to_le16(bd_prod);
		rx_prod.cqe_prod = cpu_to_le16(cq_prod);
		DIRECT_REG_WR(p_rx->set_prod_addr, *((u32 *) & rx_prod));
	}
}

int qed_ll2_post_rx_buffer(void *cxt,
			   u8 connection_handle,
			   dma_addr_t addr,
			   u16 buf_len, void *cookie, u8 notify_fw)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)cxt;
	struct core_rx_bd_with_buff_len *p_curb = NULL;
	struct qed_ll2_rx_packet *p_curp = NULL;
	struct qed_ll2_info *p_ll2_conn;
	struct qed_ll2_rx_queue *p_rx;
	unsigned long flags;
	void *p_data;
	int rc = 0;

	p_ll2_conn = qed_ll2_handle_sanity(p_hwfn, connection_handle);
	if (p_ll2_conn == NULL)
		return -EINVAL;
	p_rx = &p_ll2_conn->rx_queue;
	if (p_rx->set_prod_addr == NULL)
		return -EIO;

	spin_lock_irqsave(&p_rx->lock, flags);
	if (!list_empty(&p_rx->free_descq))
		p_curp = list_first_entry(&p_rx->free_descq,
					  struct qed_ll2_rx_packet, list_entry);
	if (p_curp) {
		if (qed_chain_get_elem_left(&p_rx->rxq_chain) &&
		    qed_chain_get_elem_left(&p_rx->rcq_chain)) {
			p_data = qed_chain_produce(&p_rx->rxq_chain);
			p_curb = (struct core_rx_bd_with_buff_len *)p_data;
			qed_chain_produce(&p_rx->rcq_chain);
		}
	}

	/* If we're lacking entires, let's try to flush buffers to FW */
	if (!p_curp || !p_curb) {
		rc = -EBUSY;
		p_curp = NULL;
		goto out_notify;
	}

	/* We have an Rx packet we can fill */
	DMA_REGPAIR_LE(p_curb->addr, addr);
	p_curb->buff_length = cpu_to_le16(buf_len);
	p_curp->rx_buf_addr = addr;
	p_curp->cookie = cookie;
	p_curp->rxq_bd = p_curb;
	p_curp->buf_length = buf_len;
	list_del(&p_curp->list_entry);

	/* Check if we only want to enqueue this packet without informing FW */
	if (!notify_fw) {
		list_add_tail(&p_curp->list_entry, &p_rx->posting_descq);
		goto out;
	}

out_notify:
	qed_ll2_post_rx_buffer_notify_fw(p_hwfn, p_rx, p_curp);
out:
	spin_unlock_irqrestore(&p_rx->lock, flags);
	return rc;
}

static void qed_ll2_prepare_tx_packet_set(struct qed_ll2_tx_queue *p_tx,
					  struct qed_ll2_tx_packet *p_curp,
					  struct qed_ll2_tx_pkt_info *pkt,
					  u8 notify_fw)
{
	list_del(&p_curp->list_entry);
	p_curp->cookie = pkt->cookie;
	p_curp->bd_used = pkt->num_of_bds;
	p_curp->notify_fw = notify_fw;
	p_tx->cur_send_packet = p_curp;
	p_tx->cur_send_frag_num = 0;

	p_curp->bds_set[p_tx->cur_send_frag_num].tx_frag = pkt->first_frag;
	p_curp->bds_set[p_tx->cur_send_frag_num].frag_len = pkt->first_frag_len;
	p_tx->cur_send_frag_num++;
}

static void qed_ll2_prepare_tx_packet_set_bd(struct qed_hwfn *p_hwfn,
					     struct qed_ll2_info *p_ll2,
					     struct qed_ll2_tx_packet *p_curp,
					     struct qed_ll2_tx_pkt_info *pkt)
{
	struct qed_chain *p_tx_chain = &p_ll2->tx_queue.txq_chain;
	u16 prod_idx = qed_chain_get_prod_idx(p_tx_chain);
	struct core_tx_bd *start_bd = NULL;
	enum core_roce_flavor_type roce_flavor;
	enum core_tx_dest tx_dest;
	u16 bd_data = 0, frag_idx;

	roce_flavor = (pkt->qed_roce_flavor == QED_LL2_ROCE) ?
	    CORE_ROCE : CORE_RROCE;

	switch (pkt->tx_dest) {
	case QED_LL2_TX_DEST_NW:
		tx_dest = CORE_TX_DEST_NW;
		break;
	case QED_LL2_TX_DEST_LB:
		tx_dest = CORE_TX_DEST_LB;
		break;
	case QED_LL2_TX_DEST_DROP:
		tx_dest = CORE_TX_DEST_DROP;
		break;
	default:
		tx_dest = CORE_TX_DEST_LB;
		break;
	}

	start_bd = (struct core_tx_bd *)qed_chain_produce(p_tx_chain);

	if (QED_IS_IWARP_PERSONALITY(p_hwfn) &&
	    (p_ll2->input.conn_type == QED_LL2_TYPE_OOO)) {
		start_bd->nw_vlan_or_lb_echo =
		    cpu_to_le16(IWARP_LL2_IN_ORDER_TX_QUEUE);
	} else {
		start_bd->nw_vlan_or_lb_echo = cpu_to_le16(pkt->vlan);
		if (test_bit(QED_MF_UFP_SPECIFIC, &p_hwfn->cdev->mf_bits) &&
		    (p_ll2->input.conn_type == QED_LL2_TYPE_FCOE))
			pkt->remove_stag = true;
	}

	SET_FIELD(start_bd->bitfield1, CORE_TX_BD_L4_HDR_OFFSET_W,
		  cpu_to_le16(pkt->l4_hdr_offset_w));
	SET_FIELD(start_bd->bitfield1, CORE_TX_BD_TX_DST, tx_dest);
	bd_data |= pkt->bd_flags;
	SET_FIELD(bd_data, CORE_TX_BD_DATA_START_BD, 0x1);
	SET_FIELD(bd_data, CORE_TX_BD_DATA_NBDS, pkt->num_of_bds);
	SET_FIELD(bd_data, CORE_TX_BD_DATA_ROCE_FLAV, roce_flavor);
	SET_FIELD(bd_data, CORE_TX_BD_DATA_IP_CSUM, ! !(pkt->enable_ip_cksum));
	SET_FIELD(bd_data, CORE_TX_BD_DATA_L4_CSUM, ! !(pkt->enable_l4_cksum));
	SET_FIELD(bd_data, CORE_TX_BD_DATA_IP_LEN, ! !(pkt->calc_ip_len));
	SET_FIELD(bd_data, CORE_TX_BD_DATA_DISABLE_STAG_INSERTION,
		  ! !(pkt->remove_stag));

	start_bd->bd_data.as_bitfield = cpu_to_le16(bd_data);
	DMA_REGPAIR_LE(start_bd->addr, pkt->first_frag);
	start_bd->nbytes = cpu_to_le16(pkt->first_frag_len);

	DP_VERBOSE(p_hwfn,
		   (NETIF_MSG_TX_QUEUED | QED_MSG_LL2),
		   "LL2 [q 0x%02x cid 0x%08x type 0x%08x] Tx Producer at [0x%04x] - set with a %04x bytes %02x BDs buffer at %08x:%08x\n",
		   p_ll2->queue_id,
		   p_ll2->cid,
		   p_ll2->input.conn_type,
		   prod_idx,
		   pkt->first_frag_len,
		   pkt->num_of_bds,
		   le32_to_cpu(start_bd->addr.hi),
		   le32_to_cpu(start_bd->addr.lo));

	if (p_ll2->tx_queue.cur_send_frag_num == pkt->num_of_bds)
		return;

	/* Need to provide the packet with additional BDs for frags */
	for (frag_idx = p_ll2->tx_queue.cur_send_frag_num;
	     frag_idx < pkt->num_of_bds; frag_idx++) {
		struct core_tx_bd **p_bd = &p_curp->bds_set[frag_idx].txq_bd;

		*p_bd = (struct core_tx_bd *)qed_chain_produce(p_tx_chain);
		(*p_bd)->bd_data.as_bitfield = 0;
		(*p_bd)->bitfield1 = 0;
		p_curp->bds_set[frag_idx].tx_frag = 0;
		p_curp->bds_set[frag_idx].frag_len = 0;
	}
}

/* This should be called while the Txq spinlock is being held */
static void qed_ll2_tx_packet_notify(struct qed_hwfn *p_hwfn,
				     struct qed_ll2_info *p_ll2_conn)
{
	bool b_notify = p_ll2_conn->tx_queue.cur_send_packet->notify_fw;
	struct qed_ll2_tx_queue *p_tx = &p_ll2_conn->tx_queue;
	struct qed_ll2_tx_packet *p_pkt = NULL;
	u16 bd_prod;

	/* If there are missing BDs, don't do anything now */
	if (p_ll2_conn->tx_queue.cur_send_frag_num !=
	    p_ll2_conn->tx_queue.cur_send_packet->bd_used)
		return;

	/* Push the current packet to the list and clean after it */
	list_add_tail(&p_ll2_conn->tx_queue.cur_send_packet->list_entry,
		      &p_ll2_conn->tx_queue.sending_descq);
	p_ll2_conn->tx_queue.cur_send_packet = NULL;
	p_ll2_conn->tx_queue.cur_send_frag_num = 0;

	/* Notify FW of packet only if requested to */
	if (!b_notify)
		return;

	bd_prod = qed_chain_get_prod_idx(&p_ll2_conn->tx_queue.txq_chain);

	while (!list_empty(&p_tx->sending_descq)) {
		p_pkt = list_first_entry(&p_tx->sending_descq,
					 struct qed_ll2_tx_packet, list_entry);
		if (p_pkt == NULL)
			break;
		list_del(&p_pkt->list_entry);
		list_add_tail(&p_pkt->list_entry, &p_tx->active_descq);
	}

	p_tx->db_msg.spq_prod = cpu_to_le16(bd_prod);

	/* Make sure the BDs data is updated before ringing the doorbell */
	wmb();

	DIRECT_REG_WR(p_tx->doorbell_addr, *((u32 *) & p_tx->db_msg));

	DP_VERBOSE(p_hwfn,
		   (NETIF_MSG_TX_QUEUED | QED_MSG_LL2),
		   "LL2 [q 0x%02x cid 0x%08x type 0x%08x] Doorbelled [producer 0x%04x]\n",
		   p_ll2_conn->queue_id,
		   p_ll2_conn->cid,
		   p_ll2_conn->input.conn_type, p_tx->db_msg.spq_prod);
}

int qed_ll2_prepare_tx_packet(void *cxt,
			      u8 connection_handle,
			      struct qed_ll2_tx_pkt_info *pkt, bool notify_fw)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)cxt;
	struct qed_ll2_tx_packet *p_curp = NULL;
	struct qed_ll2_info *p_ll2_conn = NULL;
	struct qed_ll2_tx_queue *p_tx;
	struct qed_chain *p_tx_chain;
	unsigned long flags;
	int rc = 0;

	p_ll2_conn = qed_ll2_handle_sanity(p_hwfn, connection_handle);
	if (p_ll2_conn == NULL)
		return -EINVAL;
	p_tx = &p_ll2_conn->tx_queue;
	p_tx_chain = &p_tx->txq_chain;

	if (pkt->num_of_bds > p_ll2_conn->input.tx_max_bds_per_packet)
		return -EIO;	/* coalescing is requireed */

	spin_lock_irqsave(&p_tx->lock, flags);
	if (p_tx->cur_send_packet) {
		rc = -EEXIST;
		goto out;
	}

	/* Get entry, but only if we have tx elements for it */
	if (!list_empty(&p_tx->free_descq))
		p_curp = list_first_entry(&p_tx->free_descq,
					  struct qed_ll2_tx_packet, list_entry);
	if (p_curp && qed_chain_get_elem_left(p_tx_chain) < pkt->num_of_bds)
		p_curp = NULL;

	if (!p_curp) {
		rc = -EBUSY;
		goto out;
	}

	/* Prepare packet and BD, and perhaps send a doorbell to FW */
	qed_ll2_prepare_tx_packet_set(p_tx, p_curp, pkt, notify_fw);

	qed_ll2_prepare_tx_packet_set_bd(p_hwfn, p_ll2_conn, p_curp, pkt);

	qed_ll2_tx_packet_notify(p_hwfn, p_ll2_conn);

out:
	spin_unlock_irqrestore(&p_tx->lock, flags);
	return rc;
}

int qed_ll2_set_fragment_of_tx_packet(void *cxt,
				      u8 connection_handle,
				      dma_addr_t addr, u16 nbytes)
{
	struct qed_ll2_tx_packet *p_cur_send_packet = NULL;
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)cxt;
	struct qed_ll2_info *p_ll2_conn = NULL;
	u16 cur_send_frag_num = 0;
	struct core_tx_bd *p_bd;
	unsigned long flags;

	p_ll2_conn = qed_ll2_handle_sanity(p_hwfn, connection_handle);
	if (p_ll2_conn == NULL)
		return -EINVAL;

	if (!p_ll2_conn->tx_queue.cur_send_packet)
		return -EINVAL;

	p_cur_send_packet = p_ll2_conn->tx_queue.cur_send_packet;
	cur_send_frag_num = p_ll2_conn->tx_queue.cur_send_frag_num;

	if (cur_send_frag_num >= p_cur_send_packet->bd_used)
		return -EINVAL;

	/* Fill the BD information, and possibly notify FW */
	p_bd = p_cur_send_packet->bds_set[cur_send_frag_num].txq_bd;
	DMA_REGPAIR_LE(p_bd->addr, addr);
	p_bd->nbytes = cpu_to_le16(nbytes);
	p_cur_send_packet->bds_set[cur_send_frag_num].tx_frag = addr;
	p_cur_send_packet->bds_set[cur_send_frag_num].frag_len = nbytes;

	p_ll2_conn->tx_queue.cur_send_frag_num++;

	spin_lock_irqsave(&p_ll2_conn->tx_queue.lock, flags);
	qed_ll2_tx_packet_notify(p_hwfn, p_ll2_conn);
	spin_unlock_irqrestore(&p_ll2_conn->tx_queue.lock, flags);

	return 0;
}

int qed_ll2_terminate_connection(void *cxt, u8 connection_handle)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)cxt;
	struct qed_ll2_info *p_ll2_conn = NULL;
	int rc = -EOPNOTSUPP;

	p_ll2_conn = qed_ll2_handle_sanity_lock(p_hwfn, connection_handle);
	if (p_ll2_conn == NULL) {
		rc = -EINVAL;
		return rc;
	}

	/* Stop Tx & Rx of connection, if needed */

	/* Make sure this is seen by ll2_lb_rxq/rxq_completion */
	wmb();

	if (IS_PF(p_hwfn->cdev)) {
		if (QED_LL2_TX_REGISTERED(p_ll2_conn)) {
			p_ll2_conn->tx_queue.b_cb_registered = false;
			/* Make sure this is seen by ll2_(lb)_rxq_completion */
			smp_wmb();
			rc = qed_ll2_terminate_connection_tx(p_hwfn,
							     p_ll2_conn);

			if (rc)
				return rc;

			qed_int_unregister_cb(p_hwfn,
					      p_ll2_conn->tx_queue.tx_sb_index);

			qed_ll2_txq_flush(p_hwfn, connection_handle);
		}

		if (QED_LL2_RX_REGISTERED(p_ll2_conn)) {
			p_ll2_conn->rx_queue.b_cb_registered = false;
			/* Make sure this is seen by ll2_(lb)_rxq_completion */
			smp_wmb();
			rc = qed_ll2_terminate_connection_rx(p_hwfn,
							     p_ll2_conn);

			if (rc)
				return rc;

			qed_int_unregister_cb(p_hwfn,
					      p_ll2_conn->rx_queue.rx_sb_index);

			qed_ll2_rxq_flush(p_hwfn, connection_handle);
		}
	} else {
		/* VF */
		rc = qed_vf_pf_terminate_ll2_conn(p_hwfn, connection_handle);
		if (rc)
			return rc;

		if (QED_LL2_TX_REGISTERED(p_ll2_conn)) {
			p_ll2_conn->tx_queue.b_cb_registered = false;
			qed_ll2_txq_flush(p_hwfn, connection_handle);
		}

		if (QED_LL2_RX_REGISTERED(p_ll2_conn)) {
			p_ll2_conn->rx_queue.b_cb_registered = false;
			qed_ll2_rxq_flush(p_hwfn, connection_handle);
		}
	}

	if (p_ll2_conn->input.conn_type == QED_LL2_TYPE_OOO)
		qed_ooo_release_all_isles(p_hwfn->p_ooo_info);

	if (p_ll2_conn->input.conn_type == QED_LL2_TYPE_FCOE) {
		if (!test_bit(QED_MF_UFP_SPECIFIC, &p_hwfn->cdev->mf_bits))
			qed_llh_remove_protocol_filter(p_hwfn->cdev, 0,
						       QED_LLH_FILTER_ETHERTYPE,
						       0x8906, 0);
		qed_llh_remove_protocol_filter(p_hwfn->cdev, 0,
					       QED_LLH_FILTER_ETHERTYPE,
					       0x8914, 0);
	}

	return rc;
}

static void qed_ll2_release_connection_ooo(struct qed_hwfn *p_hwfn,
					   struct qed_ll2_info *p_ll2_conn)
{
	struct qed_ooo_buffer *p_buffer;

	if (p_ll2_conn->input.conn_type != QED_LL2_TYPE_OOO)
		return;

	qed_ooo_release_all_isles(p_hwfn->p_ooo_info);
	while ((p_buffer = qed_ooo_get_free_buffer(p_hwfn->p_ooo_info))) {
		dma_free_coherent(&p_hwfn->cdev->pdev->dev,
				  p_buffer->rx_buffer_size,
				  p_buffer->rx_buffer_virt_addr,
				  p_buffer->rx_buffer_phys_addr);
		kfree(p_buffer);
	}
}

void qed_ll2_release_connection(void *cxt, u8 connection_handle)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)cxt;
	struct qed_ll2_info *p_ll2_conn = NULL;

	p_ll2_conn = qed_ll2_handle_sanity(p_hwfn, connection_handle);
	if (p_ll2_conn == NULL)
		return;

	kfree(p_ll2_conn->tx_queue.descq_array);
	p_ll2_conn->tx_queue.descq_array = NULL;
	qed_chain_free(p_hwfn->cdev, &p_ll2_conn->tx_queue.txq_chain);

	kfree(p_ll2_conn->rx_queue.descq_array);
	p_ll2_conn->rx_queue.descq_array = NULL;
	qed_chain_free(p_hwfn->cdev, &p_ll2_conn->rx_queue.rxq_chain);
	qed_chain_free(p_hwfn->cdev, &p_ll2_conn->rx_queue.rcq_chain);

	if (IS_PF(p_hwfn->cdev))
		/* for VF it will be done as part of terminate */
		qed_cxt_release_cid(p_hwfn, p_ll2_conn->cid);

	qed_ll2_release_connection_ooo(p_hwfn, p_ll2_conn);

	mutex_lock(&p_ll2_conn->mutex);
	p_ll2_conn->b_active = false;
	mutex_unlock(&p_ll2_conn->mutex);
}

/* QED LL2: internal functions */

int qed_ll2_alloc(struct qed_hwfn *p_hwfn)
{
	struct qed_ll2_info *p_ll2_info;
	u8 i, num_conns;

	num_conns = QED_MAX_NUM_OF_LL2_CONNS(p_hwfn);

	/* Allocate LL2's set struct */
	p_ll2_info = kzalloc(sizeof(struct qed_ll2_info) * num_conns,
			     GFP_KERNEL);
	if (!p_ll2_info) {
		DP_NOTICE(p_hwfn, "Failed to allocate `struct qed_ll2'\n");
		return -ENOMEM;
	}

	p_hwfn->p_ll2_info = p_ll2_info;
	for (i = 0; i < num_conns; i++)
		p_ll2_info[i].my_id = i;

	return 0;
}

void qed_ll2_setup(struct qed_hwfn *p_hwfn)
{
	int i, num_conns;

	num_conns = QED_MAX_NUM_OF_LL2_CONNS(p_hwfn);
	for (i = 0; i < num_conns; i++)
		mutex_init(&p_hwfn->p_ll2_info[i].mutex);
}

void qed_ll2_free(struct qed_hwfn *p_hwfn)
{
	if (!p_hwfn->p_ll2_info)
		return;

	kfree(p_hwfn->p_ll2_info);
	p_hwfn->p_ll2_info = NULL;
}

static void _qed_ll2_get_port_stats(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt,
				    struct qed_ll2_stats *p_stats)
{
	struct core_ll2_port_stats port_stats;

	memset(&port_stats, 0, sizeof(port_stats));
	qed_memcpy_from(p_hwfn, p_ptt, &port_stats,
			BAR0_MAP_REG_TSDM_RAM +
			TSTORM_LL2_PORT_STAT_OFFSET(MFW_PORT(p_hwfn)),
			sizeof(port_stats));

	p_stats->gsi_invalid_hdr += HILO_64_REGPAIR(port_stats.gsi_invalid_hdr);
	p_stats->gsi_invalid_pkt_length +=
	    HILO_64_REGPAIR(port_stats.gsi_invalid_pkt_length);
	p_stats->gsi_unsupported_pkt_typ +=
	    HILO_64_REGPAIR(port_stats.gsi_unsupported_pkt_typ);
	p_stats->gsi_crcchksm_error +=
	    HILO_64_REGPAIR(port_stats.gsi_crcchksm_error);
}

static void _qed_ll2_get_tstats(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt,
				struct qed_ll2_info *p_ll2_conn,
				struct qed_ll2_stats *p_stats)
{
	struct core_ll2_tstorm_per_queue_stat tstats;
	u8 qid = p_ll2_conn->queue_id;
	u32 tstats_addr;

	memset(&tstats, 0, sizeof(tstats));
	tstats_addr = BAR0_MAP_REG_TSDM_RAM +
	    CORE_LL2_TSTORM_PER_QUEUE_STAT_OFFSET(qid);
	qed_memcpy_from(p_hwfn, p_ptt, &tstats, tstats_addr, sizeof(tstats));

	p_stats->packet_too_big_discard +=
	    HILO_64_REGPAIR(tstats.packet_too_big_discard);
	p_stats->no_buff_discard += HILO_64_REGPAIR(tstats.no_buff_discard);
}

static void _qed_ll2_get_ustats(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt,
				struct qed_ll2_info *p_ll2_conn,
				struct qed_ll2_stats *p_stats)
{
	struct core_ll2_ustorm_per_queue_stat ustats;
	u8 qid = p_ll2_conn->queue_id;
	u32 ustats_addr;

	memset(&ustats, 0, sizeof(ustats));
	ustats_addr = BAR0_MAP_REG_USDM_RAM +
	    CORE_LL2_USTORM_PER_QUEUE_STAT_OFFSET(qid);
	qed_memcpy_from(p_hwfn, p_ptt, &ustats, ustats_addr, sizeof(ustats));

	p_stats->rcv_ucast_bytes += HILO_64_REGPAIR(ustats.rcv_ucast_bytes);
	p_stats->rcv_mcast_bytes += HILO_64_REGPAIR(ustats.rcv_mcast_bytes);
	p_stats->rcv_bcast_bytes += HILO_64_REGPAIR(ustats.rcv_bcast_bytes);
	p_stats->rcv_ucast_pkts += HILO_64_REGPAIR(ustats.rcv_ucast_pkts);
	p_stats->rcv_mcast_pkts += HILO_64_REGPAIR(ustats.rcv_mcast_pkts);
	p_stats->rcv_bcast_pkts += HILO_64_REGPAIR(ustats.rcv_bcast_pkts);
}

static void _qed_ll2_get_pstats(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt,
				struct qed_ll2_info *p_ll2_conn,
				struct qed_ll2_stats *p_stats)
{
	struct core_ll2_pstorm_per_queue_stat pstats;
	u8 stats_id = p_ll2_conn->tx_stats_id;
	u32 pstats_addr;

	memset(&pstats, 0, sizeof(pstats));
	pstats_addr = BAR0_MAP_REG_PSDM_RAM +
	    CORE_LL2_PSTORM_PER_QUEUE_STAT_OFFSET(stats_id);
	qed_memcpy_from(p_hwfn, p_ptt, &pstats, pstats_addr, sizeof(pstats));

	p_stats->sent_ucast_bytes += HILO_64_REGPAIR(pstats.sent_ucast_bytes);
	p_stats->sent_mcast_bytes += HILO_64_REGPAIR(pstats.sent_mcast_bytes);
	p_stats->sent_bcast_bytes += HILO_64_REGPAIR(pstats.sent_bcast_bytes);
	p_stats->sent_ucast_pkts += HILO_64_REGPAIR(pstats.sent_ucast_pkts);
	p_stats->sent_mcast_pkts += HILO_64_REGPAIR(pstats.sent_mcast_pkts);
	p_stats->sent_bcast_pkts += HILO_64_REGPAIR(pstats.sent_bcast_pkts);
}

int __qed_ll2_get_stats(void *cxt,
			u8 connection_handle, struct qed_ll2_stats *p_stats)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)cxt;
	struct qed_ll2_info *p_ll2_conn = NULL;
	struct qed_ptt *p_ptt;

	if (IS_VF(p_hwfn->cdev))
		return 0;

	if ((connection_handle >= QED_MAX_NUM_OF_LL2_CONNS_PF) ||
	    !p_hwfn->p_ll2_info)
		return -EINVAL;

	p_ll2_conn = &p_hwfn->p_ll2_info[connection_handle];

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt) {
		DP_ERR(p_hwfn, "Failed to acquire ptt\n");
		return -EINVAL;
	}

	if (p_ll2_conn->input.gsi_enable)
		_qed_ll2_get_port_stats(p_hwfn, p_ptt, p_stats);

	_qed_ll2_get_tstats(p_hwfn, p_ptt, p_ll2_conn, p_stats);

	_qed_ll2_get_ustats(p_hwfn, p_ptt, p_ll2_conn, p_stats);

	if (p_ll2_conn->tx_stats_en)
		_qed_ll2_get_pstats(p_hwfn, p_ptt, p_ll2_conn, p_stats);

	qed_ptt_release(p_hwfn, p_ptt);

	return 0;
}

int qed_ll2_get_stats(void *cxt,
		      u8 connection_handle, struct qed_ll2_stats *p_stats)
{
	memset(p_stats, 0, sizeof(*p_stats));

	return __qed_ll2_get_stats(cxt, connection_handle, p_stats);
}

int qed_ll2_completion(void *cxt, u8 connection_handle)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)cxt;
	struct qed_ll2_info *p_ll2_conn;
	int rc = 0;

	p_ll2_conn = qed_ll2_handle_sanity(p_hwfn, connection_handle);
	if (p_ll2_conn == NULL)
		return -EINVAL;

	rc = qed_ll2_txq_completion(p_hwfn, p_ll2_conn);
	if (rc)
		return rc;

	return qed_ll2_rxq_completion(p_hwfn, p_ll2_conn);
}

 /**/
    static void qed_ll2b_release_rx_packet(void *cxt,
					   u8 connection_handle,
					   void *cookie,
					   dma_addr_t rx_buf_addr,
					   bool b_last_packet)
{
	struct qed_hwfn *p_hwfn = (struct qed_hwfn *)cxt;

	qed_ll2_dealloc_buffer(p_hwfn->cdev, cookie);
}

static void qed_ll2_register_cb_ops(struct qed_dev *cdev,
				    const struct qed_ll2_cb_ops *ops,
				    void *cookie)
{
	cdev->ll2->cbs = ops;
	cdev->ll2->cb_cookie = cookie;
}

static void qed_ll2_stop_ooo(struct qed_hwfn *p_hwfn)
{
	u8 *handle = &p_hwfn->pf_params.iscsi_pf_params.ll2_ooo_queue_id;

	DP_VERBOSE(p_hwfn, (QED_MSG_STORAGE | QED_MSG_LL2),
		   "Stopping LL2 OOO queue [%02x]\n", *handle);

	qed_ll2_terminate_connection(p_hwfn, *handle);
	qed_ll2_release_connection(p_hwfn, *handle);
	*handle = QED_LL2_UNUSED_HANDLE;
}

static struct qed_ll2_cbs ll2_cbs = {
	INIT_STRUCT_FIELD(rx_comp_cb, &qed_ll2b_complete_rx_packet),
	INIT_STRUCT_FIELD(rx_release_cb, &qed_ll2b_release_rx_packet),
	INIT_STRUCT_FIELD(tx_comp_cb, &qed_ll2b_complete_tx_packet),
	INIT_STRUCT_FIELD(tx_release_cb, &qed_ll2b_complete_tx_packet),
};

static void qed_ll2_set_conn_data(struct qed_hwfn *p_hwfn,
				  struct qed_ll2_acquire_data *data,
				  struct qed_ll2_params *params,
				  enum qed_ll2_conn_type conn_type,
				  u8 * handle, bool lb)
{
	memset(data, 0, sizeof(*data));

	data->input.conn_type = conn_type;
	data->input.mtu = params->mtu;
	data->input.rx_num_desc = QED_LL2_RX_SIZE;
	data->input.rx_drop_ttl0_flg = params->drop_ttl0_packets;
	data->input.tx_num_desc = QED_LL2_TX_SIZE;
	data->p_connection_handle = handle;
	data->cbs = &ll2_cbs;
	ll2_cbs.cookie = p_hwfn;

	/* Disable VLAN stripping for OOO LL2 connections */
	if (conn_type == QED_LL2_TYPE_OOO)
		data->input.rx_vlan_removal_en = 0;
	else
		data->input.rx_vlan_removal_en = params->rx_vlan_stripping;

	if (lb) {
		data->input.tx_tc = PKT_LB_TC;
		data->input.tx_dest = QED_LL2_TX_DEST_LB;
	} else {
		data->input.tx_tc = 0;
		data->input.tx_dest = QED_LL2_TX_DEST_NW;
	}
}

static int qed_ll2_start_ooo(struct qed_hwfn *p_hwfn,
			     struct qed_ll2_params *params)
{
	u8 *handle = &p_hwfn->pf_params.iscsi_pf_params.ll2_ooo_queue_id;
	struct qed_ll2_acquire_data data;
	int rc;

	DP_VERBOSE(p_hwfn, (QED_MSG_STORAGE | QED_MSG_LL2),
		   "Starting OOO LL2 queue\n");

	qed_ll2_set_conn_data(p_hwfn, &data, params,
			      QED_LL2_TYPE_OOO, handle, true);

	rc = qed_ll2_acquire_connection(p_hwfn, &data);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed to acquire LL2 OOO connection\n");
		goto fail;
	}

	rc = qed_ll2_establish_connection(p_hwfn, *handle);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed to establish LL2 OOO connection\n");
		goto release_conn;
	}

	return 0;

release_conn:
	qed_ll2_release_connection(p_hwfn, *handle);
fail:
	*handle = QED_LL2_UNUSED_HANDLE;
	return rc;
}

static bool qed_ll2_is_storage_eng1(struct qed_dev *cdev)
{
	return (QED_IS_FCOE_PERSONALITY(QED_LEADING_HWFN(cdev)) ||
		QED_IS_ISCSI_PERSONALITY(QED_LEADING_HWFN(cdev))) &&
	    !IS_LEAD_HWFN(QED_AFFIN_HWFN(cdev));
}

static int __qed_ll2_stop(struct qed_hwfn *p_hwfn)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	int rc;

	rc = qed_ll2_terminate_connection(p_hwfn, cdev->ll2->handle);
	if (rc)
		DP_INFO(cdev, "Failed to terminate LL2 connection\n");

	qed_ll2_release_connection(p_hwfn, cdev->ll2->handle);

	return rc;
}

static int qed_ll2_stop(struct qed_dev *cdev)
{
	bool b_is_storage_eng1 = qed_ll2_is_storage_eng1(cdev);
	struct qed_hwfn *p_hwfn = QED_AFFIN_HWFN(cdev);
	int rc = 0, rc2 = 0;

	if (cdev->ll2->handle == QED_LL2_UNUSED_HANDLE)
		return 0;

	qed_llh_remove_mac_filter(cdev, 0, cdev->ll2_mac_address);
	memset(cdev->ll2_mac_address, 0, ETH_ALEN);

	if (QED_IS_ISCSI_PERSONALITY(p_hwfn))
		qed_ll2_stop_ooo(p_hwfn);

	/* In CMT mode, LL2 is always started on engine 0 for a storage PF */
	if (b_is_storage_eng1) {
		rc2 = __qed_ll2_stop(QED_LEADING_HWFN(cdev));
		if (rc2)
			DP_NOTICE(QED_LEADING_HWFN(cdev),
				  "Failed to stop LL2 on engine 0\n");
	}

	rc = __qed_ll2_stop(p_hwfn);
	if (rc)
		DP_NOTICE(p_hwfn, "Failed to stop LL2\n");

	qed_ll2_kill_buffers(cdev);

	cdev->ll2->handle = QED_LL2_UNUSED_HANDLE;

	return rc | rc2;
}

static int __qed_ll2_start(struct qed_hwfn *p_hwfn,
			   struct qed_ll2_params *params)
{
	struct qed_ll2_buffer *buffer, *tmp_buffer;
	struct qed_dev *cdev = p_hwfn->cdev;
	enum qed_ll2_conn_type conn_type;
	struct qed_ll2_acquire_data data;
	int rc, rx_cnt;

	switch (p_hwfn->hw_info.personality) {
	case QED_PCI_FCOE:
		conn_type = QED_LL2_TYPE_FCOE;
		break;
	case QED_PCI_ISCSI:
		conn_type = QED_LL2_TYPE_ISCSI;
		break;
	case QED_PCI_ETH_ROCE:
		conn_type = QED_LL2_TYPE_ROCE;
		break;
	default:
		/* TODO - move from TYPE_TEST to protocol value */
		conn_type = QED_LL2_TYPE_TEST;
	}

	qed_ll2_set_conn_data(p_hwfn, &data, params, conn_type,
			      &cdev->ll2->handle, false);

	rc = qed_ll2_acquire_connection(p_hwfn, &data);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed to acquire LL2 connection\n");
		return rc;
	}

	rc = qed_ll2_establish_connection(p_hwfn, cdev->ll2->handle);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed to establish LL2 connection\n");
		goto release_conn;
	}

	/* Post all Rx buffers to FW */
	spin_lock_bh(&cdev->ll2->lock);
	rx_cnt = cdev->ll2->rx_cnt;
	list_for_each_entry_safe(buffer, tmp_buffer, &cdev->ll2->list, list) {
		rc = qed_ll2_post_rx_buffer(p_hwfn, cdev->ll2->handle, buffer->phys_addr, 0,	/* buffer length is required in gsi_offloaded mode only */
					    buffer, 1);
		if (rc) {
			DP_INFO(p_hwfn,
				"Failed to post an Rx buffer; Deleting it\n");
			dma_unmap_single(&cdev->pdev->dev, buffer->phys_addr,
					 cdev->ll2->rx_size, DMA_FROM_DEVICE);
			QED_FREE_RX_DATA(buffer->data);
			list_del(&buffer->list);
			kfree(buffer);
		} else {
			rx_cnt++;
		}
	}
	spin_unlock_bh(&cdev->ll2->lock);

	if (rx_cnt == cdev->ll2->rx_cnt) {
		DP_NOTICE(p_hwfn, "Failed passing even a single Rx buffer\n");
		goto terminate_conn;
	}
	cdev->ll2->rx_cnt = rx_cnt;

	return 0;

terminate_conn:
	qed_ll2_terminate_connection(p_hwfn, cdev->ll2->handle);
release_conn:
	qed_ll2_release_connection(p_hwfn, cdev->ll2->handle);
	return rc;
}

static int qed_ll2_start(struct qed_dev *cdev, struct qed_ll2_params *params)
{
	bool b_is_storage_eng1 = qed_ll2_is_storage_eng1(cdev);
	struct qed_hwfn *p_hwfn = QED_AFFIN_HWFN(cdev);
	struct qed_ll2_buffer *buffer;
	int rx_num_desc, i, rc;

	if (!is_valid_ether_addr(params->ll2_mac_address)) {
		DP_NOTICE(cdev, "Invalid Ethernet address\n");
		return -EINVAL;
	}

	WARN_ON(!cdev->ll2->cbs);

	/* Initialize LL2 locks & lists */
	INIT_LIST_HEAD(&cdev->ll2->list);
	spin_lock_init(&cdev->ll2->lock);

	cdev->ll2->rx_size = PRM_DMA_PAD_BYTES_NUM + ETH_HLEN +
	    L1_CACHE_BYTES + params->mtu;

	/* Allocate memory for LL2.
	 * In CMT mode, in case of a storage PF which is affintized to engine 1,
	 * LL2 is started also on engine 0 and thus we need twofold buffers.
	 */
	rx_num_desc = QED_LL2_RX_SIZE * (b_is_storage_eng1 ? 2 : 1);
	DP_INFO(cdev, "Allocating %d LL2 buffers of size %d bytes\n",
		rx_num_desc, cdev->ll2->rx_size);
	for (i = 0; i < rx_num_desc; i++) {
		buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
		if (!buffer) {
			DP_NOTICE(cdev, "Failed to allocate LL2 buffers\n");
			rc = -ENOMEM;
			goto err0;
		}

		rc = qed_ll2_alloc_buffer(cdev, (QED_RX_DATA **) & buffer->data,
					  &buffer->phys_addr);
		if (rc) {
			kfree(buffer);
			goto err0;
		}

		list_add_tail(&buffer->list, &cdev->ll2->list);
	}

	rc = __qed_ll2_start(p_hwfn, params);
	if (rc) {
		DP_NOTICE(cdev, "Failed to start LL2\n");
		goto err0;
	}

	/* In CMT mode, always need to start LL2 on engine 0 for a storage PF,
	 * since broadcast/mutlicast packets are routed to engine 0.
	 */
	if (b_is_storage_eng1) {
		rc = __qed_ll2_start(QED_LEADING_HWFN(cdev), params);
		if (rc) {
			DP_NOTICE(QED_LEADING_HWFN(cdev),
				  "Failed to start LL2 on engine 0\n");
			goto err1;
		}
	}

	if (QED_IS_ISCSI_PERSONALITY(p_hwfn)) {
		rc = qed_ll2_start_ooo(p_hwfn, params);
		if (rc) {
			DP_NOTICE(cdev, "Failed to start OOO LL2\n");
			goto err2;
		}
	}

	rc = qed_llh_add_mac_filter(cdev, 0, params->ll2_mac_address);
	if (rc) {
		DP_NOTICE(cdev, "Failed to add an LLH filter\n");
		goto err3;
	}

	ether_addr_copy(cdev->ll2_mac_address, params->ll2_mac_address);

	return 0;

err3:
	if (QED_IS_ISCSI_PERSONALITY(p_hwfn))
		qed_ll2_stop_ooo(p_hwfn);
err2:
	if (b_is_storage_eng1)
		__qed_ll2_stop(QED_LEADING_HWFN(cdev));
err1:
	__qed_ll2_stop(p_hwfn);
err0:
	qed_ll2_kill_buffers(cdev);
	cdev->ll2->handle = QED_LL2_UNUSED_HANDLE;
	return rc;
}

static int qed_ll2_start_xmit(struct qed_dev *cdev,
			      struct sk_buff *skb, unsigned long xmit_flags)
{
	struct qed_hwfn *p_hwfn = QED_AFFIN_HWFN(cdev);
	struct qed_ll2_tx_pkt_info pkt;
	const skb_frag_t *frag;
	u8 flags = 0, nr_frags;
	int rc = -EINVAL, i;
	dma_addr_t mapping;
	u16 vlan = 0;

	if (unlikely(!cdev->ll2)) {
		DP_ERR(cdev, "Cannot transmit packet ll2 context null\n");
		return -EINVAL;
	}

	if (unlikely(skb->ip_summed != CHECKSUM_NONE)) {
		DP_INFO(cdev, "Cannot transmit a checksumed packet\n");
		return -EINVAL;
	}

	nr_frags = skb_shinfo(skb)->nr_frags;

	if (1 + nr_frags > CORE_LL2_TX_MAX_BDS_PER_PACKET) {
		DP_ERR(cdev, "Cannot transmit a packet with %d fragments\n",
		       1 + nr_frags);
		return -EINVAL;
	}

	/*{
	 *      int k;
	 *      printk(KERN_ERR "New Packet (Tx)\n");
	 *      for (k = 0; k < skb_headlen(skb); k++)
	 *              printk(KERN_ERR "%02x: %02x\n", k, ((u8 *)skb->data)[k]);
	 * }*/

	mapping = dma_map_single(&cdev->pdev->dev, skb->data,
				 skb->len, DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(&cdev->pdev->dev, mapping))) {
		DP_NOTICE(cdev, "SKB mapping failed\n");
		return -EINVAL;
	}

	/* Request HW to calculate IP csum */
	if (!((vlan_get_protocol(skb) == htons(ETH_P_IPV6)) &&
	      ipv6_hdr(skb)->nexthdr == NEXTHDR_IPV6))
		flags |= BIT(CORE_TX_BD_DATA_IP_CSUM_SHIFT);

	/* TODO - this is dead code until we add Tx vlan offload */
	if (skb_vlan_tag_present(skb)) {
		vlan = skb_vlan_tag_get(skb);
		flags |= BIT(CORE_TX_BD_DATA_VLAN_INSERTION_SHIFT);
	}

	memset(&pkt, 0, sizeof(pkt));
	pkt.num_of_bds = 1 + nr_frags;
	pkt.vlan = vlan;
	pkt.bd_flags = flags;
	pkt.tx_dest = QED_LL2_TX_DEST_NW;
	pkt.first_frag = mapping;
	pkt.first_frag_len = skb->len;
	pkt.cookie = skb;
	if (test_bit(QED_MF_UFP_SPECIFIC, &cdev->mf_bits) &&
	    test_bit(QED_LL2_XMIT_FLAGS_FIP_DISCOVERY, &xmit_flags))
		pkt.remove_stag = true;

	rc = qed_ll2_prepare_tx_packet(p_hwfn, cdev->ll2->handle, &pkt, 1);
	if (rc)
		goto err;

	/* In case of non fragmented SKB, skb should not be accessed after
	 * qed_ll2_prepare_tx_packet() call since SKB might get freed from
	 * completion flow and accessing SKB or it's field could lead to
	 * stale access or crash.
	 */
	for (i = 0; i < nr_frags; i++) {
		frag = &skb_shinfo(skb)->frags[i];

		mapping = skb_frag_dma_map(&cdev->pdev->dev, frag, 0,
					   skb_frag_size(frag), DMA_TO_DEVICE);

		if (unlikely(dma_mapping_error(&cdev->pdev->dev, mapping))) {
			DP_NOTICE(cdev,
				  "Unable to map frag - dropping packet\n");
			goto err;
		}

		rc = qed_ll2_set_fragment_of_tx_packet(p_hwfn,
						       cdev->ll2->handle,
						       mapping,
						       skb_frag_size(frag));

		/* if failed not much to do here, partial packet has been posted
		 * we can't free memory, will need to wait for completion
		 */
		if (rc)
			goto err2;
	}

	return 0;

err:
	dma_unmap_single(&cdev->pdev->dev, mapping, skb->len, DMA_TO_DEVICE);
err2:
	return rc;
}

static int qed_ll2_stats(struct qed_dev *cdev, struct qed_ll2_stats *stats)
{
	bool b_is_storage_eng1 = qed_ll2_is_storage_eng1(cdev);
	struct qed_hwfn *p_hwfn = QED_AFFIN_HWFN(cdev);
	int rc;

	if (!cdev->ll2)
		return -EINVAL;

	rc = qed_ll2_get_stats(p_hwfn, cdev->ll2->handle, stats);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed to get LL2 stats\n");
		return rc;
	}

	/* In CMT mode, LL2 is always started on engine 0 for a storage PF */
	if (b_is_storage_eng1) {
		rc = __qed_ll2_get_stats(QED_LEADING_HWFN(cdev),
					 cdev->ll2->handle, stats);
		if (rc) {
			DP_NOTICE(QED_LEADING_HWFN(cdev),
				  "Failed to get LL2 stats on engine 0\n");
			return rc;
		}
	}

	return 0;
}

const struct qed_ll2_ops qed_ll2_ops_pass = {
	INIT_STRUCT_FIELD(start, &qed_ll2_start),
	INIT_STRUCT_FIELD(stop, &qed_ll2_stop),
	INIT_STRUCT_FIELD(start_xmit, &qed_ll2_start_xmit),
	INIT_STRUCT_FIELD(register_cb_ops, &qed_ll2_register_cb_ops),
	INIT_STRUCT_FIELD(get_stats, &qed_ll2_stats),
};

int qed_ll2_alloc_if(struct qed_dev *cdev)
{
	cdev->ll2 = kzalloc(sizeof(*cdev->ll2), GFP_KERNEL);
	return cdev->ll2 ? 0 : -ENOMEM;
}

void qed_ll2_dealloc_if(struct qed_dev *cdev)
{
	kfree(cdev->ll2);
	cdev->ll2 = NULL;
}
