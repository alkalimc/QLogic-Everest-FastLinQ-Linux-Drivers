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
#ifdef _HAS_TRACE_XDP_EXCEPTION /* QEDE_UPSTREAM */
#include <linux/bpf_trace.h>
#endif
#if defined(_HAS_NDO_UDP_TUNNEL_CONFIG) || \
    defined(_HAS_NDO_EXT_UDP_TUNNEL_CONFIG) /* QEDE_UPSTREAM */
#include <net/udp_tunnel.h>
#endif
#ifdef _HAS_GRO_HEADER /* QEDE_UPSTREAM */
#include <net/gro.h>
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
#include <linux/vmalloc.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif
#include <net/udp.h>
#include <linux/workqueue.h>

#include "qed_if.h"
#include "qede_compat.h"
#include "qede_hsi.h"
#include "qede.h"
#include "qede_ptp.h"

#ifdef ENC_SUPPORTED
#include <net/gre.h>
#endif

/***************************
 * Logging Related content *
 ***************************/

#ifdef TIME_FP_DEBUG /* ! QEDE_UPSTREAM */
static bool ksoftirqd_running(struct qede_fp_time *stats,
			      enum qede_fp_time_event event)
{
	/* This is approximate based; If it's 11+ consecutive NAPI run,
	 * we'll be in ksofriqd.
	 */
	bool rc = !!(stats->consecutive_napi >= 10);

	if (event == QEDE_FP_TIME_END)
		stats->consecutive_napi = 0;
	else
		stats->consecutive_napi++;

	return rc;
}

static void qede_log_time(struct qede_dev *edev, struct qede_fastpath *fp,
			  enum qede_fp_time_event event)
{
	struct qede_fp_time *logger = &fp->time_log;
	struct qede_fp_time_stats *stats;
	struct timeval tv;
	u64 length;
	int cos;

	do_gettimeofday(&tv);

	/* On start, mark the time & fullness of the Rx-queue [if possible] */
	if (event == QEDE_FP_TIME_START) {
		u16 wrk;

		logger->start_tv = tv;

		if (fp->type & QEDE_FASTPATH_RX) {
			wrk = le16_to_cpu(*fp->rxq->hw_cons_ptr) -
			      qed_chain_get_cons_idx(&fp->rxq->rx_comp_ring);
			logger->start_rx = wrk;
		}

		if (fp->type & QEDE_FASTPATH_TX) {
			for_each_cos_in_txq(edev, cos) {
				struct qede_tx_queue *txq = &fp->txq[cos];
				struct qed_chain *pbl;
				u16 cons, cons_idx;

				if (qede_txq_has_work(txq)) {
					pbl = &txq->tx_pbl;

					cons = le16_to_cpu(*txq->hw_cons_ptr);
					cons_idx = qed_chain_get_cons_idx(pbl);

					wrk = cons - cons_idx;
					logger->start_tx[cos] = wrk;
				}
			}
		}

		return;
	}

	/* If NAPI is ending, correct the various accounts */
	if (ksoftirqd_running(logger, event))
		stats = &logger->ksoftirqd;
	else
		stats = &logger->softirq;

	/* While U64, it's not really expected to pass U32 */
	length = ((u32)tv.tv_sec - (u32)logger->start_tv.tv_sec) * 1000000;
	length += (u32)tv.tv_usec;
	length -= (u32)logger->start_tv.tv_usec;

	/* To improve when functions return amount */
	stats->runs++;
	stats->rx += (logger->start_rx < 64 ? logger->start_rx : 64);

	for_each_cos_in_txq(edev, cos)
		stats->tx[cos] += logger->start_tx[cos];

	stats->usecs += length;
	if (length > stats->longest)
		stats->longest = length;
}

static ssize_t qede_log_time_write(struct file *filep,
				   const char __user *buffer,
				   size_t count, loff_t *ppos)
{
	struct qede_dev *edev = filep->private_data;
	int i;

	if (!edev)
		return count;

	__qede_lock(edev);
	for_each_queue(i)
		memset(&edev->fp_array[i].time_log, 0,
		       sizeof(edev->fp_array[i].time_log));
	__qede_unlock(edev);

	return count;
}

static ssize_t qede_log_time_read(struct file *filp, char __user *user_buffer,
				  size_t count, loff_t *ppos)
{
	struct qede_dev *edev = filp->private_data;
	u64 rx = 0, tx = 0, rx_usec = 0, tx_usec = 0;
	char *buffer, *t_buffer;
	int i, len = 0;

	if (!edev)
		return 0;

	__qede_lock(edev);
	buffer = kmalloc(sizeof(char) * QEDE_QUEUE_CNT(edev) * 256 * 2,
			 GFP_KERNEL);
	if (!buffer)
		goto out;
	t_buffer = buffer;

	for_each_queue(i) {
		struct qede_fastpath *fp = &edev->fp_array[i];
		struct qede_fp_time *logger = &fp->time_log;
		struct qede_fp_time_stats *softirq = &logger->softirq;
		struct qede_fp_time_stats *ksoftirqd = &logger->ksoftirqd;
		int cos;

		t_buffer += sprintf(t_buffer, "FP[%d], softirq:\n", fp->id);
		t_buffer += sprintf(t_buffer,
				    "\tNAPI iterations %llu\n\tRx CQEs %llu usecs %llu\n",
				    softirq->runs, softirq->rx, softirq->usecs);

		for_each_cos_in_txq(edev, cos)
			t_buffer += sprintf(t_buffer,
					    "\tTx completions cos %d:%llu\n",
					    cos, softirq->tx[cos]);

		if ((fp->type & QEDE_FASTPATH_RX) && softirq->usecs) {
				t_buffer += sprintf(t_buffer,
						    "\tCQE/usec: %d.%d\n",
						    (int)(softirq->rx /
							  softirq->usecs),
						    (int)((softirq->rx * 100) /
							  softirq->usecs) %
							 100);
				rx_usec += softirq->usecs;
		}
		if ((fp->type & QEDE_FASTPATH_TX) && softirq->usecs) {
			for_each_cos_in_txq(edev, cos) {
				t_buffer += sprintf(t_buffer,
						    "\tTx comp/usec: cos %d.%d.%d\n",
						    cos,
						    (int)(softirq->tx[cos] /
							softirq->usecs),
						    (int)((softirq->tx[cos] *
							100) / softirq->usecs) %
								100);
			}
			tx_usec += softirq->usecs;
		}
		t_buffer += sprintf(t_buffer,
				    "\tLongest single NAPI run [usec]: %llu\n",
				    softirq->longest);

		rx += softirq->rx;

		for_each_cos_in_txq(edev, cos)
			tx += softirq->tx[cos];

		t_buffer += sprintf(t_buffer, "FP[%d], ksoftirqd:\n", fp->id);
		t_buffer += sprintf(t_buffer,
				    "\tNAPI iterations %llu\n\tRx CQEs %llu usecs %llu\n",
				    ksoftirqd->runs, ksoftirqd->rx,
				    ksoftirqd->usecs);
		for_each_cos_in_txq(edev, cos) {
			t_buffer += sprintf(t_buffer,
					    "\tTx completions %llu\n",
					    ksoftirqd->tx[cos]);
		}

		if ((fp->type & QEDE_FASTPATH_RX) && ksoftirqd->usecs) {
				t_buffer += sprintf(t_buffer,
						    "\tCQE/usec: %d.%d\n",
						    (int)(ksoftirqd->rx /
							  ksoftirqd->usecs),
						    (int)((ksoftirqd->rx *
							   100) /
							  ksoftirqd->usecs) %
							 100);
				rx_usec += ksoftirqd->usecs;
		}
		if ((fp->type & QEDE_FASTPATH_TX) && ksoftirqd->usecs) {
			for_each_cos_in_txq(edev, cos) {
				t_buffer += sprintf(t_buffer,
						    "\tTx comp/usec: cos:%d.%d.%d\n",
						    cos,
						    (int)(ksoftirqd->tx[cos] /
							ksoftirqd->usecs),
						    (int)((ksoftirqd->tx[cos] *
							   100) /
							  ksoftirqd->usecs) %
							 100);
			}
			tx_usec += ksoftirqd->usecs;
		}
		t_buffer += sprintf(t_buffer,
				    "\tLongest single NAPI run [usec]: %llu\n",
				    ksoftirqd->longest);

		rx += ksoftirqd->rx;

		for_each_cos_in_txq(edev, cos)
			tx += ksoftirqd->tx[cos];
	}

	t_buffer += sprintf(t_buffer,
			    "Total Rx %llu [Usec %llu] Tx %llu [Usec %llu]\n",
			    rx, rx_usec, tx, tx_usec);
	if (rx_usec > 0)
		t_buffer += sprintf(t_buffer, "      Rx CQE/usec: %d.%d\n",
				    (int)(rx / rx_usec),
				    (int)((rx * 100) / rx_usec) % 100);
	if (tx_usec > 0)
		t_buffer += sprintf(t_buffer, "      Tx comp/usec %d.%d\n",
				    (int)(tx / tx_usec),
				    (int)((tx * 100) / tx_usec) % 100);

	/* Write the buffer to user */
	len = simple_read_from_buffer(user_buffer, count, ppos,
				      buffer, strlen(buffer));

	kfree(buffer);
out:
	__qede_unlock(edev);

	return len;
}

static const struct file_operations qede_time_fops = {
	.owner  = THIS_MODULE,
	.open   = simple_open,
	.read   = qede_log_time_read,
	.write	= qede_log_time_write,
};

#endif

static struct qede_fp_logger_gen *
qede_logger_next_entry(struct qede_fastpath *fp,
		       enum qede_fp_logger_type type)
{
	u8 idx = fp->logger.idx++;
	struct qede_fp_logger_gen *log = &fp->logger.data[idx].gen;

	log->type = type;
	log->ts = jiffies;

	return log;
}

static void qede_log_intr(struct qede_fastpath *fp, u8 command)
{
	struct qede_fp_logger_int *p_int;

	p_int = (struct qede_fp_logger_int *)
		qede_logger_next_entry(fp, QEDE_FP_LOGGER_INT);

	p_int->ack = fp->sb_info->sb_ack;
	p_int->command = command;
}

static void qede_log_napi(struct qede_fastpath *fp, int rc)
{
	struct qede_fp_logger_napi *p_napi;

	p_napi = (struct qede_fp_logger_napi *)
		 qede_logger_next_entry(fp, QEDE_FP_LOGGER_NAPI);

	p_napi->rc = rc;

	if (fp->type & QEDE_FASTPATH_RX)
		p_napi->rx_cons = qed_chain_get_cons_idx(&fp->rxq->rx_comp_ring);

	if (fp->type & QEDE_FASTPATH_TX) {
		int cos;

		for_each_cos_in_txq(fp->edev, cos) {
			struct qede_tx_queue *txq = &fp->txq[cos];
			u16 cons, prod;

			cons = qed_chain_get_cons_idx(&txq->tx_pbl);
			prod = qed_chain_get_prod_idx(&txq->tx_pbl);

			p_napi->tx_cons[cos] = cons;
			p_napi->tx_prod[cos] = prod;
		}
	}
}

static void qede_log_db(struct qede_tx_queue *txq)
{
	struct qede_fp_logger_db *p_db = &txq->fp->logger.last_db;

	p_db->db = txq->tx_db.data.bd_prod;
	p_db->common.ts = jiffies;
}

static void qede_fp_sb_dump(struct qede_dev *edev, struct qede_fastpath *fp)
{
	char *p_sb = (char *)fp->sb_info->sb_virt;
	u32 sb_size = fp->sb_info->sb_size, i;

	for (i = 0; i < sb_size; i += 8)
		DP_NOTICE(edev,
			  "%02hhX %02hhX %02hhX %02hhX  %02hhX %02hhX %02hhX %02hhX\n",
			  p_sb[i], p_sb[i+1], p_sb[i+2], p_sb[i+3],
			  p_sb[i+4], p_sb[i+5], p_sb[i+6], p_sb[i+7]);
}

static void qede_tx_log_print_one(struct qede_dev *edev,
				  struct qede_fastpath *fp,
				  struct qede_fp_logger_gen *log,
				  u8 idx)
{
	if (log->type == QEDE_FP_LOGGER_NAPI) {
		struct qede_fp_logger_napi *p_napi;
		int cos;

		p_napi = (struct qede_fp_logger_napi *)log;
		DP_NOTICE(edev,
			  "[%02x]: NAPI rc %d Rx cons %04x [Jiffies %lu]\n",
			  idx, p_napi->rc, p_napi->rx_cons, p_napi->common.ts);
		for_each_cos_in_txq(edev, cos)
			DP_NOTICE(edev,
				  "COS:%d Tx prod %04x Tx cons %04x\n", cos,
				  p_napi->tx_prod[cos], p_napi->tx_cons[cos]);
	} else if (log->type == QEDE_FP_LOGGER_INT) {
		struct qede_fp_logger_int *p_int;

		p_int = (struct qede_fp_logger_int *)log;

		DP_NOTICE(edev, "[%02x]: INT ACK %08x ENABLE %02x [Jiffies %lu]\n",
			  idx, p_int->ack,
			  (p_int->command == IGU_INT_ENABLE) ? 1 : 0,
			  p_int->common.ts);
	}
}

void qede_txq_fp_log_metadata(struct qede_dev *edev,
			      struct qede_fastpath *fp, struct qede_tx_queue *txq)
{
	struct qed_chain *p_chain = &txq->tx_pbl;

	/* Dump txq/fp/sb ids etc. other metadata */
	DP_NOTICE(edev,
		  "fpid 0x%x sbid 0x%x txqid [0x%x] ndev_qid [0x%x] cos [0x%x] p_chain %p cap %d size %d jiffies %lu HZ 0x%x\n",
		  fp->id, fp->sb_info->igu_sb_id, txq->index, txq->ndev_txq_id, txq->cos,
		  p_chain, p_chain->capacity, p_chain->size, jiffies, HZ);

	/* Dump all the relevant prod/cons indexes */
	DP_NOTICE(edev, "hw cons %04x sw_tx_prod=0x%x, sw_tx_cons=0x%x, bd_prod 0x%x bd_cons 0x%x\n",
		  le16_to_cpu(*txq->hw_cons_ptr), txq->sw_tx_prod, txq->sw_tx_cons,
		  qed_chain_get_prod_idx(p_chain), qed_chain_get_cons_idx(p_chain));
}

void qede_tx_log_print(struct qede_dev *edev, struct qede_fastpath *fp,
		       struct qede_tx_queue *txq)
{
	struct qede_fp_logger_db *p_db = &fp->logger.last_db;
	u8 idx = fp->logger.idx, i, count;
	struct netdev_queue *netdev_txq;
	struct qed_sb_info_dbg sb_dbg;
	int rc;

	if (fp->logger.spilled)
		return;
	fp->logger.spilled = true;

	netdev_txq = netdev_get_tx_queue(txq->fp->ndev, txq->ndev_txq_id);

	/* sb info */
	qede_fp_sb_dump(edev, fp);

	memset(&sb_dbg, 0, sizeof(sb_dbg));
	rc = edev->ops->common->get_sb_info(edev->cdev, fp->sb_info, (u16)fp->id, &sb_dbg);

	DP_NOTICE(edev, "IGU: prod %08x cons %08x CAU Tx %04x\n",
		  sb_dbg.igu_prod, sb_dbg.igu_cons,
		  sb_dbg.pi[TX_PI(txq->cos)]);

	/* NAPI logger */
	DP_NOTICE(edev, "Index 0x%x Last DB %08x [Jiffies %lu] trans_start %lu\n",
		  idx, p_db->db, p_db->common.ts, netdev_txq->trans_start);

	for (i = idx - 1, count = 16; (i != idx) && count; i--, count--)
		qede_tx_log_print_one(edev, fp, &fp->logger.data[i].gen, i);

	/* report to mfw */
	edev->ops->common->mfw_report(edev->cdev,
				      "Txq[%d]: FW cons [host] %04x, SW cons %04x, SW prod %04x [idx %02x] [Jiffies %lu]\n",
				      txq->index, le16_to_cpu(*txq->hw_cons_ptr),
				      qed_chain_get_cons_idx(&txq->tx_pbl),
				      qed_chain_get_prod_idx(&txq->tx_pbl), idx, jiffies);

	if (!rc) {
		edev->ops->common->mfw_report(edev->cdev,
					      "Txq[%d]: SB[0x%04x] - IGU: prod %08x cons %08x CAU Tx %04x\n",
					      txq->index, fp->sb_info->igu_sb_id,
					      sb_dbg.igu_prod, sb_dbg.igu_cons,
					      sb_dbg.pi[TX_PI(txq->cos)]);
	}

	edev->ops->common->mfw_report(edev->cdev, "Last DB: %08x [Jiffies %lu]\n",
				      p_db->db, p_db->common.ts);
}

/***************************
 * Memory pool related     *
 ***************************/

static bool qede_get_page_from_pool(struct qede_rx_queue *rxq,
				    struct page **src_page, dma_addr_t *addr)
{
	struct qede_page_pool *pool = &rxq->page_pool;
	int size = pool->size;

	if (unlikely(pool->cons == pool->prod))
		goto fail;

	if (page_ref_count(pool->page_pool[pool->cons].page) != 1)
		goto fail;

	*src_page = pool->page_pool[pool->cons].page;
	*addr = pool->page_pool[pool->cons].mapping;
	pool->cons = (pool->cons + 1) & (size - 1);
	return true;
fail:
	rxq->pool_unready++;
	return false;
}

static bool qede_add_page_to_pool(struct qede_rx_queue *rxq,
				  struct page *p_page, dma_addr_t addr,
				  bool inc_refcount)
{
	struct qede_page_pool *pool = &rxq->page_pool;
	int size = pool->size;
	u32 prod_next;

	prod_next = (pool->prod + 1) & (size - 1);

	if (unlikely(prod_next == pool->cons)) {
		rxq->pool_full++;
		return false;
	}

#if defined(_HAS_PAGE_PFMEMALLOC) || defined(_HAS_PAGE_PFMEMALLOC_API) /* QEDE_UPSTREAM */
	if (unlikely(page_is_pfmemalloc(p_page)))
		return false;
#endif

	pool->page_pool[pool->prod].page = p_page;
	pool->page_pool[pool->prod].mapping = addr;

	pool->prod = prod_next;

	if (inc_refcount)
		page_ref_inc(p_page);

	return true;
}

/*********************************
 * Content also used by slowpath *
 *********************************/

int qede_alloc_rx_buffer(struct qede_rx_queue *rxq, bool allow_lazy)
{
	int node = dev_to_node(rxq->dev);
	struct sw_rx_data *sw_rx_data;
	struct eth_rx_bd *rx_bd;
	dma_addr_t mapping;
	struct page *data;

	/* In case lazy-allocation is allowed, postpone allocation until the
	 * end of the NAPI run. We'd still need to make sure the Rx ring has
	 * sufficient entries to guarantee an Rx interrupt.
	 */
	if (likely(allow_lazy) &&
	    likely(rxq->filled_buffers > ETH_RX_BD_THRESHOLD)) {
		rxq->filled_buffers--;
		return 0;
	}

	if (qede_get_page_from_pool(rxq, &data, &mapping))
		goto set_page;

	if (likely(numa_native)) {
		data = alloc_pages_node(node, GFP_ATOMIC, 0);
		if (unlikely(!data))
			data = alloc_pages(GFP_ATOMIC, 0);
	} else {
		data = alloc_pages(GFP_ATOMIC, 0);
	}

	if (unlikely(!data))
		return -ENOMEM;

	/* Map the entire page as it would be used
	 * for multiple RX buffer segment size mapping.
	 */
	mapping = dma_map_page(rxq->dev, data, 0,
			       PAGE_SIZE, rxq->data_direction);
	if (unlikely(dma_mapping_error(rxq->dev, mapping))) {
		__free_page(data);
		return -ENOMEM;
	}

set_page:
	sw_rx_data = &rxq->sw_rx_ring[rxq->sw_rx_prod & NUM_RX_BDS_MAX];
	sw_rx_data->page_offset = 0;
	sw_rx_data->data = data;
	sw_rx_data->mapping = mapping;

	/* Advance PROD and get BD pointer */
	rx_bd = (struct eth_rx_bd *)qed_chain_produce(&rxq->rx_bd_ring);
	QEDE_FAST_PATH_BUG_ON(!rx_bd);
	rx_bd->addr.hi = cpu_to_le32(upper_32_bits(mapping));
	rx_bd->addr.lo = cpu_to_le32(lower_32_bits(mapping) +
				     rxq->rx_headroom);

	rxq->sw_rx_prod++;
	rxq->filled_buffers++;
	return 0;
}

/* Unmap the data and free skb */
int qede_free_tx_pkt(struct qede_dev *edev,
		     struct qede_tx_queue *txq, int *len)
{
	u16 idx = txq->sw_tx_cons;
	struct sk_buff *skb = txq->sw_tx_ring.skbs[idx].skb;
	struct eth_tx_1st_bd *first_bd;
	struct eth_tx_bd *tx_data_bd;
	int bds_consumed = 0;
	int nbds;
	bool data_split = txq->sw_tx_ring.skbs[idx].flags & QEDE_TSO_SPLIT_BD;
	int i, split_bd_len = 0;

	if (unlikely(!skb)) {
		DP_ERR(edev,
		       "skb is null for txq idx=%d txq->sw_tx_cons=%d txq->sw_tx_prod=%d\n",
		       idx, txq->sw_tx_cons, txq->sw_tx_prod);
		return -1;
	}

	*len = skb->len;

	first_bd = (struct eth_tx_1st_bd *)qed_chain_consume(&txq->tx_pbl);

	bds_consumed++;

	nbds = first_bd->data.nbds;

	if (data_split) {
		struct eth_tx_bd *split = (struct eth_tx_bd *)
			qed_chain_consume(&txq->tx_pbl);
		split_bd_len = BD_UNMAP_LEN(split);
		bds_consumed++;
#ifdef FULL_TX_DEBUG
		BD_SET_UNMAP_ADDR_LEN(split, 0, 0);
#endif
	}
#ifdef FULL_TX_DEBUG
	if (BD_UNMAP_ADDR(first_bd) == 0)
		BUG();
#endif
	dma_unmap_single(&edev->pdev->dev, BD_UNMAP_ADDR(first_bd),
			 BD_UNMAP_LEN(first_bd) + split_bd_len, DMA_TO_DEVICE);
#ifdef FULL_TX_DEBUG
	BD_SET_UNMAP_ADDR_LEN(first_bd, 0, 0);
#endif

	/* Unmap the data of the skb frags */
	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++, bds_consumed++) {
		tx_data_bd = (struct eth_tx_bd *)
			qed_chain_consume(&txq->tx_pbl);
#ifdef FULL_TX_DEBUG
		if (BD_UNMAP_ADDR(tx_data_bd) == 0)
			BUG();
#endif
		dma_unmap_page(&edev->pdev->dev, BD_UNMAP_ADDR(tx_data_bd),
			       BD_UNMAP_LEN(tx_data_bd), DMA_TO_DEVICE);
#ifdef FULL_TX_DEBUG
		BD_SET_UNMAP_ADDR_LEN(tx_data_bd, 0, 0);
#endif
	}

#ifdef FULL_TX_DEBUG
	/* empty BDs can exist in following situaltion:
	 *      2nd + 3rd are empty in nonLSO and (ipv6withExt or tunneling)
	 *      3rd in LSO with only one frag
	 */
	if (nbds > 3 && bds_consumed < nbds)
		BUG();
#endif
	while (bds_consumed++ < nbds)
		qed_chain_consume(&txq->tx_pbl);

	/* Free skb */
	dev_kfree_skb_any(skb);
	txq->sw_tx_ring.skbs[idx].skb = NULL;
	txq->sw_tx_ring.skbs[idx].flags = 0;

	return 0;
}

/* Unmap the data and free skb when mapping failed during start_xmit */
static void qede_free_failed_tx_pkt(struct qede_tx_queue *txq,
				    struct eth_tx_1st_bd *first_bd,
				    int nbd,
				    bool data_split)
{
	u16 idx = txq->sw_tx_prod;
	struct sk_buff *skb = txq->sw_tx_ring.skbs[idx].skb;
	struct eth_tx_bd *tx_data_bd;
	int i, split_bd_len = 0;

	/* Return prod to its position before this skb was handled */
	qed_chain_set_prod(&txq->tx_pbl,
			   le16_to_cpu(txq->tx_db.data.bd_prod),
			   first_bd);

	first_bd = (struct eth_tx_1st_bd *)qed_chain_produce(&txq->tx_pbl);

	if (data_split) {
		struct eth_tx_bd *split = (struct eth_tx_bd *)
					  qed_chain_produce(&txq->tx_pbl);
		split_bd_len = BD_UNMAP_LEN(split);
#ifdef FULL_TX_DEBUG
		BD_SET_UNMAP_ADDR_LEN(split, 0, 0);
#endif
		nbd--;
	}

	dma_unmap_single(txq->dev, BD_UNMAP_ADDR(first_bd),
			 BD_UNMAP_LEN(first_bd) + split_bd_len, DMA_TO_DEVICE);
#ifdef FULL_TX_DEBUG
	BD_SET_UNMAP_ADDR_LEN(first_bd, 0, 0);
#endif

	/* Unmap the data of the skb frags */
	for (i = 0; i < nbd; i++) {
		tx_data_bd = (struct eth_tx_bd *)
			qed_chain_produce(&txq->tx_pbl);
		if (tx_data_bd->nbytes)
			dma_unmap_page(txq->dev,
				       BD_UNMAP_ADDR(tx_data_bd),
				       BD_UNMAP_LEN(tx_data_bd), DMA_TO_DEVICE);
#ifdef FULL_TX_DEBUG
		BD_SET_UNMAP_ADDR_LEN(tx_data_bd, 0, 0);
#endif
	}

	/* Return again prod to its position before this skb was handled */
	qed_chain_set_prod(&txq->tx_pbl,
			   le16_to_cpu(txq->tx_db.data.bd_prod),
			   first_bd);

	/* Free skb */
	dev_kfree_skb_any(skb);
	txq->sw_tx_ring.skbs[idx].skb = NULL;
	txq->sw_tx_ring.skbs[idx].flags = 0;
}

static u32 qede_xmit_type(struct sk_buff *skb,
			  int *ipv6_ext)
{
	u32 rc = XMIT_L4_CSUM;
	__be16 l3_proto;

	if (skb->ip_summed != CHECKSUM_PARTIAL)
		return XMIT_PLAIN;

	l3_proto = vlan_get_protocol(skb);
	if (l3_proto == htons(ETH_P_IPV6) &&
	    (ipv6_hdr(skb)->nexthdr == NEXTHDR_IPV6))
		*ipv6_ext = 1;
#ifdef ENC_SUPPORTED
	if (skb->encapsulation) {
		rc |= XMIT_ENC;
		if (skb_is_gso(skb)) {
#ifdef _HAS_GSO_TUN_L4_CSUM /* QEDE_UPSTREAM */
			unsigned short gso_type = skb_shinfo(skb)->gso_type;

			if ((gso_type & SKB_GSO_UDP_TUNNEL_CSUM) ||
			    (gso_type & SKB_GSO_GRE_CSUM))
				rc |= XMIT_ENC_GSO_L4_CSUM;
#endif
			rc |= XMIT_LSO;
			return rc;
		}
	}
#endif

	if (skb_is_gso(skb))
		rc |= XMIT_LSO;

	return rc;
}

static void qede_set_params_for_ipv6_ext(struct sk_buff *skb,
					 struct eth_tx_2nd_bd *second_bd,
					 struct eth_tx_3rd_bd *third_bd)
{
	u8 l4_proto;
	u16 bd2_bits1 = 0, bd2_bits2 = 0;

	bd2_bits1 |= (1 << ETH_TX_DATA_2ND_BD_IPV6_EXT_SHIFT);

	bd2_bits2 |= ((((u8 *)skb_transport_header(skb) - skb->data) >> 1) &
		      ETH_TX_DATA_2ND_BD_L4_HDR_START_OFFSET_W_MASK)
		     << ETH_TX_DATA_2ND_BD_L4_HDR_START_OFFSET_W_SHIFT;

	bd2_bits1 |= (ETH_L4_PSEUDO_CSUM_CORRECT_LENGTH <<
		      ETH_TX_DATA_2ND_BD_L4_PSEUDO_CSUM_MODE_SHIFT);

	if (vlan_get_protocol(skb) == htons(ETH_P_IPV6))
		l4_proto = ipv6_hdr(skb)->nexthdr;
	else
		l4_proto = ip_hdr(skb)->protocol;

	if (l4_proto == IPPROTO_UDP)
		bd2_bits1 |= 1 << ETH_TX_DATA_2ND_BD_L4_UDP_SHIFT;

	if (third_bd)
		third_bd->data.bitfields |=
			cpu_to_le16(((tcp_hdrlen(skb) / 4) &
				    ETH_TX_DATA_3RD_BD_TCP_HDR_LEN_DW_MASK) <<
				    ETH_TX_DATA_3RD_BD_TCP_HDR_LEN_DW_SHIFT);
	if (second_bd) {
		second_bd->data.bitfields1 = cpu_to_le16(bd2_bits1);
		second_bd->data.bitfields2 = cpu_to_le16(bd2_bits2);
	}
}

static int map_frag_to_bd(struct qede_tx_queue *txq,
			  skb_frag_t *frag,
			  struct eth_tx_bd *bd)
{
	dma_addr_t mapping;

	/* Map skb non-linear frag data for DMA */
	mapping = skb_frag_dma_map(txq->dev, frag, 0,
				   skb_frag_size(frag),
				   DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(txq->dev, mapping)))
		return -ENOMEM;

	/* Setup the data pointer of the frag data */
	BD_SET_UNMAP_ADDR_LEN(bd, mapping, skb_frag_size(frag));

	return 0;
}
/* #define FULL_TX_DEBUG 1 */
#ifdef FULL_TX_DEBUG
static void debug_print_tx_queued(struct qede_dev	*edev,
				  u16			txq_index,
				  u8			xmit_type,
				  struct eth_tx_1st_bd	*first_bd,
				  struct eth_tx_2nd_bd	*second_bd,
				  struct eth_tx_3rd_bd	*third_bd)
{
	if (unlikely((edev->dp_level <= QED_LEVEL_VERBOSE) &&
		     (edev->dp_module & NETIF_MSG_TX_QUEUED))) {
		struct qede_tx_queue *txq = edev->fp_array[edev->fp_num_rx +
							   txq_index].txq;

		DP_VERBOSE(edev, NETIF_MSG_TX_QUEUED,
			   "Sending packet on txq_index = %d, PROD = %d, num BDs = %d, vlan-insert %s, L4 checksum %s, TSO %s\n",
			   txq_index,
			   qed_chain_get_prod_idx(&txq->tx_pbl),
			   first_bd->data.nbds,
			   ((first_bd->data.bd_flags.bitfields &
				(1 << ETH_TX_1ST_BD_FLAGS_VLAN_INSERTION_SHIFT))
			    ? "requested" : "not requested"),
			   ((xmit_type & XMIT_L4_CSUM) ? "requested" :
			    "not requested"),
			   ((xmit_type & XMIT_LSO) ? "requested" :
			    "not requested"));
		DP_VERBOSE(edev, NETIF_MSG_TX_QUEUED,
			   "first bd: nbytes = %d, nbds = %d, bd_flags = %d, bitfields = %d\n",
			   le16_to_cpu(first_bd->nbytes), first_bd->data.nbds,
			   first_bd->data.bd_flags.bitfields,
			   le16_to_cpu(first_bd->data.bitfields));
		if (second_bd) {
			DP_VERBOSE(edev, NETIF_MSG_TX_QUEUED,
				   "second bd: nbytes = %d, bitfields = %d\n",
				   le16_to_cpu(second_bd->nbytes),
				   second_bd->data.bitfields1);
		}
		if (third_bd) {
			DP_VERBOSE(edev, NETIF_MSG_TX_QUEUED,
				   "third bd: nbytes = %d, bitfields = %d, mss = %d\n",
				   le16_to_cpu(third_bd->nbytes),
				   le16_to_cpu(third_bd->data.bitfields),
				   le16_to_cpu(third_bd->data.lso_mss));
		}
	}
}
#endif

static u16 qede_get_skb_hlen(struct sk_buff *skb, bool is_encap_pkt)
{
	if (is_encap_pkt)
		return (skb_inner_transport_header(skb) +
			inner_tcp_hdrlen(skb) - skb->data);
	else
		return (skb_transport_header(skb) +
			tcp_hdrlen(skb) - skb->data);
}

/* +2 for 1st BD for headers and 2nd BD for headlen (if required) */
#if ((MAX_SKB_FRAGS + 2) > ETH_TX_MAX_BDS_PER_NON_LSO_PACKET)
static bool qede_pkt_req_lin(struct sk_buff *skb, u8 xmit_type)
{
	int allowed_frags = ETH_TX_MAX_BDS_PER_NON_LSO_PACKET - 1;

	if (xmit_type & XMIT_LSO) {
		int hlen;

		hlen = qede_get_skb_hlen(skb, xmit_type & XMIT_ENC);

		/* linear payload would require its own BD */
		if (skb_headlen(skb) > hlen)
			allowed_frags--;
	}

	return (skb_shinfo(skb)->nr_frags > allowed_frags);
}
#endif

static inline void qede_update_tx_producer(struct qede_tx_queue *txq)
{
	/* wmb makes sure that the BDs data is updated before updating the
	 * producer, otherwise FW may read old data from the BDs.
	 */
	wmb();
	writel(txq->tx_db.raw, txq->doorbell_addr);

	/* Fence required to flush the write combined buffer, since another
	 * CPU may write to the same doorbell address and data may be lost
	 * due to relaxed order nature of write combined bar.
	 */
	wmb();

	if (!txq->is_xdp)
		qede_log_db(txq);
}

#ifdef _HAS_NDO_XDP /* QEDE_UPSTREAM */
static int qede_xdp_xmit(struct qede_dev *edev, struct qede_fastpath *fp,
			 struct sw_rx_data *metadata, u16 padding, u16 length)
{
	struct qede_tx_queue *txq = fp->xdp_tx;
	struct eth_tx_1st_bd *first_bd;
	u16 idx = txq->sw_tx_prod;

	if (!qed_chain_get_elem_left(&txq->tx_pbl)) {
		txq->stopped_cnt++;
		return -ENOMEM;
	}

	first_bd = (struct eth_tx_1st_bd *)qed_chain_produce(&txq->tx_pbl);

	memset(first_bd, 0, sizeof(*first_bd));
	first_bd->data.bd_flags.bitfields =
				BIT(ETH_TX_1ST_BD_FLAGS_START_BD_SHIFT);
	first_bd->data.bitfields |=
		(length & ETH_TX_DATA_1ST_BD_PKT_LEN_MASK) <<
		ETH_TX_DATA_1ST_BD_PKT_LEN_SHIFT;
	first_bd->data.nbds = 1;

	/* We can safely ignore the offset, as it's 0 for XDP */
	BD_SET_UNMAP_ADDR_LEN(first_bd, metadata->mapping + padding,
			      length);

	/* Synchronize the buffer back to device, as program [probably]
	 * have changed it.
	 */
	dma_sync_single_for_device(&edev->pdev->dev,
				   metadata->mapping + padding,
				   length, DMA_TO_DEVICE);

	txq->sw_tx_ring.xdp[idx].page = metadata->data;
	txq->sw_tx_ring.xdp[idx].mapping = metadata->mapping;
	txq->sw_tx_prod = (txq->sw_tx_prod + 1) % txq->num_tx_buffers;

	/* Mark the fastpath for future XDP doorbell */
	fp->xdp_xmit = 1;

	return 0;
}
#endif

int qede_txq_has_work(struct qede_tx_queue *txq)
{
	u16 hw_bd_cons;

	/* Tell compiler that consumer and producer can change */
	barrier();
	hw_bd_cons = le16_to_cpu(*txq->hw_cons_ptr);
	if (qed_chain_get_cons_idx(&txq->tx_pbl) == hw_bd_cons + 1)
		return 0;

	return hw_bd_cons != qed_chain_get_cons_idx(&txq->tx_pbl);
}

static void qede_xdp_tx_int(struct qede_dev *edev, struct qede_fastpath *fp)
{
	struct qede_tx_queue *txq = fp->xdp_tx;
	u16 hw_bd_cons;

	hw_bd_cons = le16_to_cpu(*txq->hw_cons_ptr);
	barrier();

	while (hw_bd_cons != qed_chain_get_cons_idx(&txq->tx_pbl)) {
		struct qede_rx_queue *rxq = fp->rxq;
		struct sw_tx_xdp *xdp_page;

		qed_chain_consume(&txq->tx_pbl);
		xdp_page = &txq->sw_tx_ring.xdp[txq->sw_tx_cons];

		if (!qede_add_page_to_pool(rxq, xdp_page->page,
					   xdp_page->mapping, false)) {
			dma_unmap_page(&edev->pdev->dev, xdp_page->mapping,
				       PAGE_SIZE, DMA_BIDIRECTIONAL);
			__free_page(xdp_page->page);
		}

		txq->sw_tx_cons = (txq->sw_tx_cons + 1) % txq->num_tx_buffers;
		txq->xmit_pkts++;
	}
}

static int qede_tx_int(struct qede_dev *edev,
		       struct qede_tx_queue *txq)
{
	unsigned int pkts_compl = 0, bytes_compl = 0;
	struct netdev_queue *netdev_txq;
	u16 hw_bd_cons;
	int rc;

	netdev_txq = netdev_get_tx_queue(txq->fp->ndev, txq->ndev_txq_id);

	hw_bd_cons = le16_to_cpu(*txq->hw_cons_ptr);

	/* prevent cpu doing speculative read of hw consumer */
	rmb();

/*	DP_VERBOSE(edev, NETIF_MSG_TX_DONE,
		   "Tx int on queue[%d]: hw_bd_cons %d, sw_bd_cons %d sw_bd_prod %d\n",
		   txq->index, hw_bd_cons,
		   qed_chain_get_cons_idx(&txq->tx_bd_ring),
		   qed_chain_get_prod_idx(&txq->tx_bd_ring));
*/
	while (hw_bd_cons != qed_chain_get_cons_idx(&txq->tx_pbl)) {
		int len = 0;

		rc = qede_free_tx_pkt(edev, txq, &len);
		if (rc) {
			DP_NOTICE(edev, "hw_bd_cons = %d, chain_cons=%d\n",
				  hw_bd_cons,
				  qed_chain_get_cons_idx(&txq->tx_pbl));
			break;
		}

		bytes_compl += len;
		pkts_compl++;
		txq->sw_tx_cons = (txq->sw_tx_cons + 1) % txq->num_tx_buffers;
		txq->xmit_pkts++;
/*		DP_VERBOSE(edev, NETIF_MSG_TX_DONE,
			   "Released a packet with %d BDs on tx queue[%d]\n",
			   nbds, txq->index);
*/
	}

	netdev_tx_completed_queue(netdev_txq, pkts_compl, bytes_compl);

	/* Need to make the tx_bd_cons update visible to start_xmit()
	 * before checking for netif_tx_queue_stopped().  Without the
	 * memory barrier, there is a small possibility that
	 * start_xmit() will miss it and cause the queue to be stopped
	 * forever.
	 * On the other hand we need an rmb() here to ensure the proper
	 * ordering of bit testing in the following
	 * netif_tx_queue_stopped(txq) call.
	 */
	smp_mb();

	if (unlikely(netif_tx_queue_stopped(netdev_txq))) {
		/* Taking tx_lock is needed to prevent reenabling the queue
		 * while it's empty. This could have happen if rx_action() gets
		 * suspended in qede_tx_int() after the condition before
		 * netif_tx_wake_queue(), while tx_action (qede_start_xmit()):
		 *
		 * stops the queue->sees fresh tx_bd_cons->releases the queue->
		 * sends some packets consuming the whole queue again->
		 * stops the queue
		 */

		__netif_tx_lock(netdev_txq, smp_processor_id());

		if ((netif_tx_queue_stopped(netdev_txq)) &&
		    (edev->state == QEDE_STATE_OPEN) &&
		    (netif_carrier_ok(edev->ndev)) &&
		    (qed_chain_get_elem_left(&txq->tx_pbl)
		      >= (MAX_SKB_FRAGS + 1))) {
			netif_tx_wake_queue(netdev_txq);
			DP_VERBOSE(edev, NETIF_MSG_TX_DONE,
				   "Wake queue was called\n");
		}

		__netif_tx_unlock(netdev_txq);
	}

	return 0;
}

bool qede_has_rx_work(struct qede_rx_queue *rxq)
{
	u16 hw_comp_cons, sw_comp_cons;

	/* Tell compiler that status block fields can change */
	barrier();

	hw_comp_cons = le16_to_cpu(*rxq->hw_cons_ptr);
	sw_comp_cons = qed_chain_get_cons_idx(&rxq->rx_comp_ring);

	return hw_comp_cons != sw_comp_cons;
}

static inline void qede_rx_bd_ring_consume(struct qede_rx_queue *rxq)
{
	qed_chain_consume(&rxq->rx_bd_ring);
	rxq->sw_rx_cons++;
}

/* This function reuses the buffer(from an offset) from
 * consumer index to producer index in the bd ring
 */
static inline void qede_reuse_page(struct qede_rx_queue *rxq,
				   struct sw_rx_data *curr_cons)
{
	struct eth_rx_bd *rx_bd_prod = qed_chain_produce(&rxq->rx_bd_ring);
	struct sw_rx_data *curr_prod;
	dma_addr_t new_mapping;

	curr_prod = &rxq->sw_rx_ring[rxq->sw_rx_prod & NUM_RX_BDS_MAX];
	*curr_prod = *curr_cons;

	QEDE_FAST_PATH_BUG_ON(!rx_bd_prod);

	new_mapping = curr_prod->mapping + curr_prod->page_offset;

	rx_bd_prod->addr.hi = cpu_to_le32(upper_32_bits(new_mapping));
	rx_bd_prod->addr.lo = cpu_to_le32(lower_32_bits(new_mapping) +
					  rxq->rx_headroom);

	rxq->sw_rx_prod++;
	curr_cons->data = NULL;
}

/* In case of allocation failures reuse buffers
 * from consumer index to produce buffers for firmware
 */
void qede_recycle_rx_bd_ring(struct qede_rx_queue *rxq, u8 count)
{
	struct sw_rx_data *curr_cons;

	for (; count > 0; count--) {
		curr_cons = &rxq->sw_rx_ring[rxq->sw_rx_cons & NUM_RX_BDS_MAX];
		qede_reuse_page(rxq, curr_cons);
		qede_rx_bd_ring_consume(rxq);
	}
}

static inline int qede_realloc_rx_buffer(struct qede_rx_queue *rxq,
					 struct sw_rx_data *curr_cons)
{
	/* Move to the next segment in the page */
	curr_cons->page_offset += rxq->rx_buf_seg_size;

	if (curr_cons->page_offset == PAGE_SIZE) {
		if (unlikely(qede_alloc_rx_buffer(rxq, true))) {
			/* Since we failed to allocate new buffer
			 * current buffer can be used again.
			 */
			curr_cons->page_offset -= rxq->rx_buf_seg_size;

			return -ENOMEM;
		}

		if (!qede_add_page_to_pool(rxq, curr_cons->data,
					   curr_cons->mapping, true))
			dma_unmap_page(rxq->dev, curr_cons->mapping,
				       PAGE_SIZE, rxq->data_direction);
	} else {
		/* Increment refcount of the page as we don't want
		 * network stack to take the ownership of the page
		 * which can be recycled multiple times by the driver.
		 */
		page_ref_inc(curr_cons->data);
		qede_reuse_page(rxq, curr_cons);
	}

	return 0;
}

void qede_update_rx_prod(struct qede_dev *edev, struct qede_rx_queue *rxq)
{
	u16 bd_prod = qed_chain_get_prod_idx(&rxq->rx_bd_ring);
	u16 cqe_prod = qed_chain_get_prod_idx(&rxq->rx_comp_ring);
	struct eth_rx_prod_data rx_prods = {0};

	/* Update producers */
	rx_prods.bd_prod = cpu_to_le16(bd_prod);
	rx_prods.cqe_prod = cpu_to_le16(cqe_prod);

	/* Make sure that the BD and SGE data is updated before updating the
	 * producers since FW might read the BD/SGE right after the producer
	 * is updated.
	 */
	wmb();

	internal_ram_wr(rxq->hw_rxq_prod_addr, sizeof(rx_prods),
			(u32 *)&rx_prods);
#ifdef FULL_TX_DEBUG
	DP_VERBOSE(edev, NETIF_MSG_RX_STATUS,
		   "bd_prod %u  cqe_prod %u\n",
		   bd_prod, cqe_prod);
#endif
}

static void qede_get_rxhash(struct sk_buff *skb, u8 bitfields, __le32 rss_hash)
{
	enum pkt_hash_types hash_type = PKT_HASH_TYPE_NONE;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35)) /* QEDE_UPSTREAM */
	enum rss_hash_type htype;
	u32 hash = 0;

	htype = GET_FIELD(bitfields, ETH_FAST_PATH_RX_REG_CQE_RSS_HASH_TYPE);

#ifndef HAS_NDO_FIX_FEATURES /* ! QEDE_UPSTREAM */
	/* On modern kernels user can't disable it */
	if (!(skb->dev->features & NETIF_F_RXHASH))
		goto out;
#endif
	if (htype) {
		hash_type = ((htype == RSS_HASH_TYPE_IPV4) ||
			     (htype == RSS_HASH_TYPE_IPV6)) ?
			     PKT_HASH_TYPE_L3 : PKT_HASH_TYPE_L4;
		hash = le32_to_cpu(rss_hash);
	}

#ifndef HAS_NDO_FIX_FEATURES /* ! QEDE_UPSTREAM */
out:
#endif
#else
	u32 hash = 0;

#endif
	skb_set_hash(skb, hash, hash_type);
}

static void qede_set_skb_csum(struct sk_buff *skb, u8 csum_flag)
{
	skb_checksum_none_assert(skb);

	if (csum_flag & QEDE_CSUM_UNNECESSARY)
		skb->ip_summed = CHECKSUM_UNNECESSARY;

	if (csum_flag & QEDE_TUNN_CSUM_UNNECESSARY) {
#ifdef _HAS_CSUM_LEVEL	/* QEDE_UPSTREAM */
		skb->csum_level = 1;
#endif
#ifdef ENC_SUPPORTED /* QEDE_UPSTREAM */
		skb->encapsulation = 1;
#endif
	}
}

static inline void qede_skb_receive(struct qede_dev *edev,
				    struct qede_fastpath *fp,
				    struct qede_rx_queue *rxq,
				    struct sk_buff *skb,
				    u16 vlan_tag)
{
#ifdef BCM_VLAN /* ! QEDE_UPSTREAM */
	if ((edev->vlan_group != NULL) && vlan_tag)
		vlan_gro_receive(&fp->napi, edev->vlan_group,
				 vlan_tag, skb);
	else
#elif !defined(OLD_VLAN) /* QEDE_UPSTREAM */
	/* CR TPA - can FW differentiate vlans ? (8021Q or s-tag) */
	if (vlan_tag)
		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q),
				       vlan_tag);

#endif
	napi_gro_receive(&fp->napi, skb);
}

#ifdef DUMP_PACKET_DATA /* ! QEDE_UPSTREAM */
#ifdef _HAS_BUILD_SKB
#define QEDE_DBG_DUMP_PACKET() \
	{ \
		int k; \
		printk(KERN_ERR "New Packet (Rx)\n"); \
		for (k = 0; k < len; k++) \
		printk(KERN_ERR "%02x: %02x\n", k,((u8 *)data)[k+pad]); \
	}
#else
#define QEDE_DBG_DUMP_PACKET() \
	{ \
		int k; \
		printk(KERN_ERR "New Packet (Rx) - no build SKB\n"); \
		for (k = 0; k < len; k++) \
		printk(KERN_ERR "%02x: %02x\n", k+pad,((u8 *)data)[k+pad]); \
	}
#endif
#else
#define QEDE_DBG_DUMP_PACKET()
#endif

#ifdef QEDE_FP_DEBUG
#define QEDE_DBG_FP_VERBOSE(...) DP_VERBOSE(edev, ##__VA_ARGS__)
#else
#define QEDE_DBG_FP_VERBOSE(...)
#endif

/* Timestamp option length allowed for TPA aggregation:
 *
 *		nop nop kind length echo val
 */
#define QEDE_TPA_TSTAMP_OPT_LEN	12

static void qede_set_gro_params(struct qede_dev *edev,
				struct sk_buff *skb,
				struct eth_fast_path_rx_tpa_start_cqe *cqe)
{
	u16 parsing_flags = le16_to_cpu(cqe->pars_flags.flags);

	if (((parsing_flags >> PARSING_AND_ERR_FLAGS_L3TYPE_SHIFT) &
	    PARSING_AND_ERR_FLAGS_L3TYPE_MASK) == 2)
		skb_shinfo(skb)->gso_type = SKB_GSO_TCPV6;
	else
		skb_shinfo(skb)->gso_type = SKB_GSO_TCPV4;

	skb_shinfo(skb)->gso_size = le16_to_cpu(cqe->len_on_first_bd) -
				    cqe->header_len;
}

static int qede_fill_frag_skb(struct qede_dev *edev,
			      struct qede_rx_queue *rxq,
			      u8 tpa_agg_index,
			      u16 len_on_bd)
{
	struct sw_rx_data *current_bd = &rxq->sw_rx_ring[rxq->sw_rx_cons &
							 NUM_RX_BDS_MAX];
	struct qede_agg_info *tpa_info = &rxq->tpa_info[tpa_agg_index];
	struct sk_buff *skb = tpa_info->skb;

#ifdef DEBUG_GRO
	rxq->tpa_info[tpa_agg_index].num_of_bds++;
	rxq->tpa_info[tpa_agg_index].total_packet_len += len_on_bd;
#endif

	if (unlikely(tpa_info->state != QEDE_AGG_STATE_START)) {
		DP_ERR(edev, "tpa segment without tpa_start\n");
		goto out;
	}

	/* Add one frag and update the appropriate fields in the skb */
	skb_fill_page_desc(skb, tpa_info->frag_id++,
			   current_bd->data,
			   current_bd->page_offset + rxq->rx_headroom,
			   len_on_bd);

	if (unlikely(qede_realloc_rx_buffer(rxq, current_bd))) {
		/* Incr page ref count to reuse on allocation failure
		 * so that it doesn't get freed while freeing SKB.
		 */
		page_ref_inc(current_bd->data);
		goto out;
	}

	qede_rx_bd_ring_consume(rxq);

	skb->data_len += len_on_bd;
	skb->truesize += rxq->rx_buf_seg_size;
	skb->len += len_on_bd;

	QEDE_DBG_FP_VERBOSE(NETIF_MSG_RX_STATUS,
			    "TPA[%02x] - Mapped Buffer [%04x bytes] as %d Frag [page at %p]. Lengths are: len: %04x, data_len: %04x\n",
			    tpa_agg_index, len_on_bd, tpa_info->frag_id,
			    page_address(current_bd->data), skb->len,
			    skb->data_len);

	return 0;

out:
	tpa_info->state = QEDE_AGG_STATE_ERROR;
	qede_recycle_rx_bd_ring(rxq, 1);

	return -ENOMEM;
}

#ifdef ENC_SUPPORTED
static bool qede_tunn_exist(u16 flag)
{
	return !!(flag & (PARSING_AND_ERR_FLAGS_TUNNELEXIST_MASK <<
			  PARSING_AND_ERR_FLAGS_TUNNELEXIST_SHIFT));
}

static u8 qede_check_tunn_csum(u16 flag)
{
	u16 csum_flag = 0;
	u8 tcsum = 0;

	if (flag & (PARSING_AND_ERR_FLAGS_TUNNELL4CHKSMWASCALCULATED_MASK <<
		    PARSING_AND_ERR_FLAGS_TUNNELL4CHKSMWASCALCULATED_SHIFT))
		csum_flag |= PARSING_AND_ERR_FLAGS_TUNNELL4CHKSMERROR_MASK <<
			     PARSING_AND_ERR_FLAGS_TUNNELL4CHKSMERROR_SHIFT;

	if (flag & (PARSING_AND_ERR_FLAGS_L4CHKSMWASCALCULATED_MASK <<
		    PARSING_AND_ERR_FLAGS_L4CHKSMWASCALCULATED_SHIFT)) {
		csum_flag |= PARSING_AND_ERR_FLAGS_L4CHKSMERROR_MASK <<
			     PARSING_AND_ERR_FLAGS_L4CHKSMERROR_SHIFT;
		tcsum = QEDE_TUNN_CSUM_UNNECESSARY;
	}

	csum_flag |= PARSING_AND_ERR_FLAGS_TUNNELIPHDRERROR_MASK <<
		     PARSING_AND_ERR_FLAGS_TUNNELIPHDRERROR_SHIFT |
		     PARSING_AND_ERR_FLAGS_IPHDRERROR_MASK <<
		     PARSING_AND_ERR_FLAGS_IPHDRERROR_SHIFT;

	if (csum_flag & flag)
		return QEDE_CSUM_ERROR;

	return QEDE_CSUM_UNNECESSARY | tcsum;
}
#endif

#ifdef _HAS_BUILD_SKB_V2 /* QEDE_UPSTREAM */
static inline struct sk_buff *
qede_build_skb(struct qede_rx_queue *rxq,
	       struct sw_rx_data *bd, u16 len, u16 pad)
{
	struct sk_buff *skb;
	void *buf;

	buf = page_address(bd->data) + bd->page_offset;
	skb = build_skb(buf, rxq->rx_buf_seg_size);
	if (unlikely(!skb))
		return NULL;

	skb_reserve(skb, pad);
	skb_put(skb, len);

	return skb;
}

static struct sk_buff *
qede_tpa_rx_build_skb(struct qede_dev *edev,
		      struct qede_rx_queue *rxq,
		      struct sw_rx_data *bd, u16 len, u16 pad,
		      bool alloc_skb)
{
	struct sk_buff *skb;

	skb = qede_build_skb(rxq, bd, len, pad);
	if (unlikely(!skb))
		return NULL;

	bd->page_offset += rxq->rx_buf_seg_size;

	if (bd->page_offset == PAGE_SIZE) {
		if (unlikely(qede_alloc_rx_buffer(rxq, true))) {
			DP_NOTICE(edev,
				  "Failed to allocate RX buffer for tpa start\n");
			bd->page_offset -= rxq->rx_buf_seg_size;
			page_ref_inc(bd->data);
			dev_kfree_skb_any(skb);
			return NULL;
		}

		if (qede_add_page_to_pool(rxq, bd->data,
					  bd->mapping, true))
			bd->page_offset = 0;
	} else {
		page_ref_inc(bd->data);
		qede_reuse_page(rxq, bd);
	}

	/* We've consumed the first BD and prepared an SKB */
	qede_rx_bd_ring_consume(rxq);

	return skb;
}

static struct sk_buff *
qede_rx_build_skb(struct qede_dev *edev,
		  struct qede_rx_queue *rxq,
		  struct sw_rx_data *bd, u16 len, u16 pad)
{
	struct sk_buff *skb = NULL;

	if ((len + pad <= edev->rx_copybreak)) {
		unsigned int offset = bd->page_offset + pad;

		skb = netdev_alloc_skb(rxq->fp->ndev, QEDE_RX_HDR_SIZE);
		if (unlikely(!skb))
			return NULL;

		skb_reserve(skb, pad);
		memcpy(skb_put(skb, len),
		       page_address(bd->data) + offset, len);
		qede_reuse_page(rxq, bd);
		goto out;
	}

	skb = qede_build_skb(rxq, bd, len, pad);
	if (unlikely(!skb))
		return NULL;

	if (unlikely(qede_realloc_rx_buffer(rxq, bd))) {
		/* Incr page ref count to reuse on allocation failure so
		 * that it doesn't get freed while freeing SKB [as its
		 * already mapped there].
		 */
		page_ref_inc(bd->data);
		dev_kfree_skb_any(skb);
		return NULL;
	}
out:
	/* We've consumed the first BD and prepared an SKB */
	qede_rx_bd_ring_consume(rxq);

	return skb;
}
#else
static struct sk_buff *qede_rx_allocate_skb(struct qede_dev *edev,
					    struct qede_rx_queue *rxq,
					    struct sw_rx_data *bd, u16 len,
					    u16 pad)
{
	unsigned int offset = bd->page_offset + pad;
	struct skb_frag_struct *frag;
	struct page *page = bd->data;
	unsigned int pull_len;
	struct sk_buff *skb;
	unsigned char *va;

	/* Allocate a new SKB with a sufficient large header len */
	skb = netdev_alloc_skb(rxq->fp->ndev,
			       QEDE_RX_HDR_SIZE + pad);
	if (unlikely(!skb))
		return NULL;

	skb_reserve(skb, pad);

	/* Copy data into SKB - if it's small, we can simply copy it and
	 * re-use the already allcoated & mapped memory.
	 */
	if (len <= edev->rx_copybreak) {
		memcpy(skb_put(skb, len),
		       page_address(page) + offset, len);
		qede_reuse_page(rxq, bd);
		goto out;
	}

	frag = &skb_shinfo(skb)->frags[0];

	QEDE_SKB_ADD_RX_FRAG(skb, skb_shinfo(skb)->nr_frags,
			     page, offset,
			     len, rxq->rx_buf_seg_size);

	va = skb_frag_address(frag);
	pull_len = QEDE_GET_HLEN(va, len);

	/* Align the pull_len to optimize memcpy */
	memcpy(skb->data, va, ALIGN(pull_len, sizeof(long)));

	/* Correct the skb & frag sizes offset after the pull */
	skb_frag_size_sub(frag, pull_len);
	frag->page_offset += pull_len;
	skb->data_len -= pull_len;
	skb->tail += pull_len;

	if (unlikely(qede_realloc_rx_buffer(rxq, bd))) {
		/* Incr page ref count to reuse on allocation failure so
		 * that it doesn't get freed while freeing SKB [as its
		 * already mapped there].
		 */
		page_ref_inc(page);
		dev_kfree_skb_any(skb);
		return NULL;
	}

out:
	/* We've consumed the first BD and prepared an SKB */
	qede_rx_bd_ring_consume(rxq);
	return skb;
}
#endif

static void qede_tpa_start(struct qede_dev *edev,
			   struct qede_rx_queue *rxq,
			   struct eth_fast_path_rx_tpa_start_cqe *cqe)
{
	struct qede_agg_info *tpa_info = &rxq->tpa_info[cqe->tpa_agg_index];
#ifndef _HAS_BUILD_SKB_V2 /* ! QEDE_UPSTREAM */
	struct sw_rx_data *replace_buf = &tpa_info->buffer;
	dma_addr_t mapping = tpa_info->buffer_mapping;
	struct sw_rx_data *sw_rx_data_prod;
	struct eth_rx_bd *rx_bd_cons;
	struct eth_rx_bd *rx_bd_prod;
#endif
	struct sw_rx_data *sw_rx_data_cons;
	struct qede_fastpath *fp = rxq->fp;
	u16 pad;

	if (unlikely(tpa_info->state != QEDE_AGG_STATE_NONE)) {
		DP_ERR(edev, "fp[%u] start of bin not in none [%d]\n",
		       fp->id, cqe->tpa_agg_index);
		goto cons_buf;
	}

	if (unlikely(cqe->bw_ext_bd_len_list[1])) {
		DP_ERR(edev,
		       "fp[%u] tpa_start unexpected bw_ext_bd_len_list [%d]\n",
		       fp->id, cqe->tpa_agg_index);
		goto cons_buf;
	}

#ifndef _HAS_BUILD_SKB_V2 /* ! QEDE_UPSTREAM */
	rx_bd_cons = qed_chain_consume(&rxq->rx_bd_ring);
	rx_bd_prod = qed_chain_produce(&rxq->rx_bd_ring);
#endif

	sw_rx_data_cons = &rxq->sw_rx_ring[rxq->sw_rx_cons & NUM_RX_BDS_MAX];
	pad = cqe->placement_offset + rxq->rx_headroom;

#ifndef _HAS_BUILD_SKB_V2 /* ! QEDE_UPSTREAM */
	/* Use pre-allocated replacement buffer - we can't release the agg.
	 * start until its over and we don't want to risk allocation failing
	 * here, so re-allocate when aggregation will be over.
	 */
	sw_rx_data_prod = &rxq->sw_rx_ring[rxq->sw_rx_prod & NUM_RX_BDS_MAX];
	sw_rx_data_prod->mapping = replace_buf->mapping;
	sw_rx_data_prod->data = replace_buf->data;
	rx_bd_prod->addr.hi = cpu_to_le32(upper_32_bits(mapping));
	rx_bd_prod->addr.lo = cpu_to_le32(lower_32_bits(mapping));
	sw_rx_data_prod->page_offset = replace_buf->page_offset;
	rxq->sw_rx_prod++;

	/* move partial skb from cons to pool (don't unmap yet)
	 * save mapping, incase we drop the packet later on.
	 */
	tpa_info->buffer = *sw_rx_data_cons;
	mapping = HILO_U64(le32_to_cpu(rx_bd_cons->addr.hi),
			   le32_to_cpu(rx_bd_cons->addr.lo));

	tpa_info->buffer_mapping = mapping;
	rxq->sw_rx_cons++;
	tpa_info->skb = netdev_alloc_skb(rxq->fp->ndev, pad +
					 le16_to_cpu(cqe->len_on_first_bd));
#else
	tpa_info->skb = qede_tpa_rx_build_skb(edev, rxq, sw_rx_data_cons,
					      le16_to_cpu(cqe->len_on_first_bd),
					      pad, false);
	tpa_info->buffer.page_offset = sw_rx_data_cons->page_offset;
	tpa_info->buffer.mapping = sw_rx_data_cons->mapping;
#endif
	if (unlikely(!tpa_info->skb)) {
		DP_NOTICE(edev, "Failed to allocate SKB for gro\n");
#ifdef _HAS_BUILD_SKB_V2 /* QEDE_UPSTREAM */
		qede_recycle_rx_bd_ring(rxq, 1);
#endif
		tpa_info->state = QEDE_AGG_STATE_ERROR;
		return;
	}

#ifdef DEBUG_GRO
	DP_VERBOSE(edev, NETIF_MSG_RX_STATUS,
		   "TPA start[%d] - length %04x [header %02x] [bd_list[0] %04x], placement offset %02x [seg_len %04x]\n",
		   cqe->tpa_agg_index, le16_to_cpu(cqe->len_on_first_bd),
		   cqe->header_len, le16_to_cpu(cqe->bw_ext_bd_len_list[0]),
		   cqe->placement_offset, le16_to_cpu(cqe->seg_len));

	tpa_info->num_aggregations++;
#endif

#ifdef ENC_SUPPORTED
	if (qede_tunn_exist(le16_to_cpu(cqe->pars_flags.flags))) {
		u8 flags = cqe->tunnel_pars_flags.flags, shift;

		shift = ETH_TUNNEL_PARSING_FLAGS_TYPE_SHIFT;
		tpa_info->tunnel_type = (flags >> shift) &
					 ETH_TUNNEL_PARSING_FLAGS_TYPE_MASK;

		/* FW indicating whether there is inner/outer vlan using
		 * TAG8021QEXIST and TUNNEL8021QTAGEXIST flags.
		 * if tunnel exists:TUNNEL8021QTAGEXIST indicating outer vlan
		 * and TAG8021QEXIST (if inner L2 exists) indicating inner vlan.
		 * if tunnel not exist, TAG8021QEXIST indicating outer vlan.
		 */
		if (GET_FIELD(le16_to_cpu(cqe->pars_flags.flags),
			      PARSING_AND_ERR_FLAGS_TAG8021QEXIST) &&
		    GET_FIELD(flags,
			      ETH_TUNNEL_PARSING_FLAGS_NEXT_PROTOCOL) == e_l2)
			tpa_info->inner_vlan_exist = 1;

		if (GET_FIELD(le16_to_cpu(cqe->pars_flags.flags),
			      PARSING_AND_ERR_FLAGS_TUNNEL8021QTAGEXIST))
			tpa_info->vlan_tag = le16_to_cpu(cqe->vlan_tag);
		else
			tpa_info->vlan_tag = 0;
	} else
#endif
	if (GET_FIELD(le16_to_cpu(cqe->pars_flags.flags),
		      PARSING_AND_ERR_FLAGS_TAG8021QEXIST))
		tpa_info->vlan_tag = le16_to_cpu(cqe->vlan_tag);
	else
		tpa_info->vlan_tag = 0;

	tpa_info->frag_id = 0;
	tpa_info->state = QEDE_AGG_STATE_START;

#ifndef _HAS_BUILD_SKB_V2 /* ! QEDE_UPSTREAM */
	/* Store some information from first CQE */
	tpa_info->start_cqe_placement_offset = cqe->placement_offset;
	tpa_info->start_cqe_bd_len = le16_to_cpu(cqe->len_on_first_bd);
	skb_reserve(tpa_info->skb, pad);
	skb_put(tpa_info->skb, tpa_info->start_cqe_bd_len);
#endif

#ifdef DEBUG_GRO
	DP_VERBOSE(edev, NETIF_MSG_RX_STATUS,
		   "TPA_START - BD is at %p [%04x entry in Ring]; after pulling SKB->len == %04x\n",
		   page_address(sw_rx_data_cons->data),
		   (rxq->sw_rx_cons - 1) & NUM_RX_BDS_MAX, tpa_info->skb->len);
	tpa_info->num_of_bds = 1;
	tpa_info->total_packet_len = cqe->len_on_first_bd;
#endif

	qede_get_rxhash(tpa_info->skb, cqe->bitfields, cqe->rss_hash);

	/* This is needed in order to enable forwarding support */
	qede_set_gro_params(edev, tpa_info->skb, cqe);

	/* For some odd MTUs (~= PAGE_SIZE) cases where BD size
	 * is set less than the MTU by the driver to accommodate
	 * ETH_OVERHEAD, rx_headroom and aligned skb_shared_info
	 * structure etc. In such cases firmware may use first
	 * extended BD in the list for the remaining packet payload
	 * regardless of tpa_hdr_data_split_flg value. Handle such
	 * corner case by processing first extended BD in the list.
	 */
	if (unlikely(cqe->bw_ext_bd_len_list[0])) {
		qede_fill_frag_skb(edev, rxq, cqe->tpa_agg_index,
				   le16_to_cpu(cqe->bw_ext_bd_len_list[0]));
	}

	return;

cons_buf:
	tpa_info->state = QEDE_AGG_STATE_ERROR;
	tpa_info->skb = NULL;
	qede_recycle_rx_bd_ring(rxq, 1);
}

#ifdef CONFIG_INET
void qede_gro_ip_csum(struct sk_buff *skb)
{
	const struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *th = tcp_hdr(skb);

	th->check = ~tcp_v4_check(skb->len - skb_transport_offset(skb),
				  iph->saddr, iph->daddr, 0);

	tcp_gro_complete(skb);
}

void qede_gro_ipv6_csum(struct sk_buff *skb)
{
	struct ipv6hdr *iph = ipv6_hdr(skb);
	struct tcphdr *th = tcp_hdr(skb);

	th->check = ~tcp_v6_check(skb->len - skb_transport_offset(skb),
				  &iph->saddr, &iph->daddr, 0);
	tcp_gro_complete(skb);
}

#ifdef ENC_SUPPORTED
static void qede_set_nh_th_offset(struct sk_buff *skb, int off)
{
	skb_set_network_header(skb, off);

	if (skb_shinfo(skb)->gso_type & SKB_GSO_TCPV4) {
		off += sizeof(struct iphdr);
		skb_set_transport_header(skb, off);
	} else {
		off += sizeof(struct ipv6hdr);
		skb_set_transport_header(skb, off);
	}
}

static void qede_handle_udp_tunnel_gro(struct sk_buff *skb, u8 inner_vlan_exist)
{
	int vlan_offset = inner_vlan_exist ? VLAN_HLEN : 0;

	skb_shinfo(skb)->gso_type |= SKB_GSO_UDP_TUNNEL;

	switch (skb->protocol) {
	case htons(ETH_P_IP):
		qede_set_nh_th_offset(skb, VXLAN_HEADROOM + vlan_offset);
		udp_gro_complete(skb, sizeof(struct iphdr),
				 udp4_lib_lookup_skb);
		break;
	case htons(ETH_P_IPV6):
		qede_set_nh_th_offset(skb, VXLAN6_HEADROOM + vlan_offset);
		udp_gro_complete(skb, sizeof(struct ipv6hdr),
				 udp6_lib_lookup_skb);
		break;
	default:
		WARN_ONCE(1, "Unsupported UDP tunnel GRO proto=0x%x\n",
			  skb->protocol);
		break;
	}
}

static void qede_handle_gre_tunnel_gro(struct sk_buff *skb, u8 inner_vlan_exist)
{
	unsigned int grehlen, gre_headroom;
	struct gre_base_hdr *greh;
	int nhoff = 0;

	switch (skb->protocol) {
	case htons(ETH_P_IP):
		nhoff = sizeof(struct iphdr);
		break;
	case htons(ETH_P_IPV6):
		nhoff = sizeof(struct ipv6hdr);
		WARN_ONCE(1, "GRE GRO received over IPv6 transport\n");
		break;
	default:
		WARN_ONCE(1, "Unsupported GRE tunnel GRO, proto=0x%x\n",
			  skb->protocol);
		break;
	}

	greh = (struct gre_base_hdr *)(skb->data + nhoff);
	skb_shinfo(skb)->gso_type |= SKB_GSO_GRE;
	skb->encapsulation = 1;
	grehlen = sizeof(*greh);

	if (greh->flags & GRE_KEY)
		grehlen += GRE_HEADER_SECTION;

	if (greh->flags & GRE_CSUM)
		grehlen += GRE_HEADER_SECTION;

	gre_headroom = nhoff + grehlen;

	/* L2 GRE */
	if (htons(greh->protocol) == ETH_P_TEB) {
		gre_headroom += ETH_HLEN;

		if (inner_vlan_exist)
			gre_headroom += VLAN_HLEN;
	}

	skb_set_network_header(skb, gre_headroom);

	if (skb_shinfo(skb)->gso_type & SKB_GSO_TCPV4) {
		skb_set_transport_header(skb,
					 gre_headroom + sizeof(struct iphdr));
		qede_gro_ip_csum(skb);
	} else {
		skb_set_transport_header(skb,
					 gre_headroom + sizeof(struct ipv6hdr));
		qede_gro_ipv6_csum(skb);
	}

	skb->inner_mac_header = skb->data - skb->head;
	skb->inner_mac_header += (nhoff + grehlen);
}

static void qede_handle_tunnel_gro(struct qede_dev *edev,
				   struct sk_buff *skb, u8 tunnel_type,
				   u8 inner_vlan_exist)
{
	switch (tunnel_type) {
	case ETH_RX_TUNN_VXLAN:
	case ETH_RX_TUNN_GENEVE:
		qede_handle_udp_tunnel_gro(skb, inner_vlan_exist);
		break;
	case ETH_RX_TUNN_GRE:
		qede_handle_gre_tunnel_gro(skb, inner_vlan_exist);
		break;
	default:
		WARN_ONCE(1, "Unsupported tunnel GRO, tunnel type=0x%x\n",
			  tunnel_type);
		break;
	}
}
#endif
#endif

static void qede_gro_receive(struct qede_dev *edev,
			     struct qede_fastpath *fp,
			     struct sk_buff *skb,
			     struct qede_agg_info *tpa_info)
{
	struct qede_rx_queue *rxq = NULL;
	/* FW can send a single MTU sized packet from gro flow
	 * due to aggregation timeout/last segment etc. which
	 * is not expected to be a gro packet. If a skb has zero
	 * frags then simply push it in the stack as non gso skb.
	 */
	if (unlikely(!skb->data_len)) {
		skb_shinfo(skb)->gso_type = 0;
		skb_shinfo(skb)->gso_size = 0;
		goto send_skb;
	}

#ifdef CONFIG_INET
	if (skb_shinfo(skb)->gso_size) {
#ifdef ENC_SUPPORTED
		if (tpa_info->tunnel_type) {
			qede_handle_tunnel_gro(edev, skb, tpa_info->tunnel_type,
					       tpa_info->inner_vlan_exist);
			goto send_skb;
		}
#endif
#ifndef _HAS_GRO_RECEIVE_AGG /* ! QEDE_UPSTREAM */
		skb_set_network_header(skb, 0);
		switch (skb->protocol) {
		case htons(ETH_P_IP):
			skb_set_transport_header(skb, sizeof(struct iphdr));
			qede_gro_ip_csum(skb);
			break;
		case htons(ETH_P_IPV6):
			skb_set_transport_header(skb, sizeof(struct ipv6hdr));
			qede_gro_ipv6_csum(skb);
			break;

			/* In old distros it possible vlan-stripping would not
			 * be configured. If a packets with vlan would be
			 * received but no vlan device will be configured for
			 * that vlan, it might reach this default - thus we
			 * can't have the following print there.
			 */
			 /* TODO - isn't this a replication of bnx2x issue? */
		}
#endif
	}

#endif

send_skb:
	rxq = fp->rxq;
	skb_record_rx_queue(skb, fp->fwd_fp ?
				 (rxq->rxq_id - fp->fwd_dev->base_queue_id) :
				 rxq->rxq_id);
	qede_skb_receive(edev, fp, rxq, skb, tpa_info->vlan_tag);
}

static inline void qede_tpa_cont(struct qede_dev *edev,
				 struct qede_rx_queue *rxq,
				 struct eth_fast_path_rx_tpa_cont_cqe *cqe)
{
	int i;

	QEDE_DBG_FP_VERBOSE(NETIF_MSG_RX_STATUS,
			    "TPA cont[%02x] - len_list [%04x %04x] - Will consume BD %04x\n",
			    cqe->tpa_agg_index, le16_to_cpu(cqe->len_list[0]),
			    le16_to_cpu(cqe->len_list[1]),
			    rxq->sw_rx_cons & NUM_RX_BDS_MAX);

	for (i = 0; cqe->len_list[i]; i++)
		qede_fill_frag_skb(edev, rxq, cqe->tpa_agg_index,
				   le16_to_cpu(cqe->len_list[i]));

	if (unlikely(i > 1)) {
		struct qede_agg_info *tpa_info;

		tpa_info = &rxq->tpa_info[cqe->tpa_agg_index];

		DP_ERR(edev,
		       "Strange - TPA cont with more than a single len_list entry\n");
		tpa_info->state = QEDE_AGG_STATE_ERROR;
	}
}

static int qede_tpa_end(struct qede_dev *edev,
			struct qede_fastpath *fp,
			struct eth_fast_path_rx_tpa_end_cqe *cqe)
{
	struct qede_rx_queue *rxq = fp->rxq;
	struct qede_agg_info *tpa_info;
	struct sk_buff *skb;
#ifndef _HAS_BUILD_SKB_V2 /* ! QEDE_UPSTREAM */
	unsigned int offset;
#endif
	int i;

	tpa_info = &rxq->tpa_info[cqe->tpa_agg_index];
	skb = tpa_info->skb;

#ifdef _HAS_BUILD_SKB_V2 /* QEDE_UPSTREAM */
	if (tpa_info->buffer.page_offset == PAGE_SIZE)
		dma_unmap_page(rxq->dev, tpa_info->buffer.mapping,
			       PAGE_SIZE, rxq->data_direction);
#endif

	QEDE_DBG_FP_VERBOSE(NETIF_MSG_RX_STATUS,
			    "TPA_END[%02x] - Total Length %04x Num Bds %02x [SGEs %04x] - Len list [%04x %04x] - might consume BD %04x\n",
			    cqe->tpa_agg_index,
			    le16_to_cpu(cqe->total_packet_len),
			    cqe->num_of_bds,
			    le16_to_cpu(cqe->num_of_coalesced_segs),
			    le16_to_cpu(cqe->len_list[0]),
			    le16_to_cpu(cqe->len_list[1]),
			    rxq->sw_rx_cons & NUM_RX_BDS_MAX);

	for (i = 0; cqe->len_list[i]; i++)
		qede_fill_frag_skb(edev, rxq, cqe->tpa_agg_index,
				   le16_to_cpu(cqe->len_list[i]));
	if (unlikely(i > 1)) {
		DP_ERR(edev,
		       "fp[%u] tpa_end: more than a single len_list entry\n",
		       fp->id);
		goto err;
	}

	if (unlikely(tpa_info->state != QEDE_AGG_STATE_START)) {
		DP_ERR(edev, "fp[%u] tpa_end: segment without tpa_start\n",
		       fp->id);
		goto err;
	}

	/* Sanity */
	if (unlikely(cqe->num_of_bds != tpa_info->frag_id + 1)) {
		DP_ERR(edev,
		       "fp[%u] tpa_end had %02x BDs, but SKB has only %d frags\n",
		       fp->id, cqe->num_of_bds, tpa_info->frag_id);
		goto err;
	}

	if (unlikely(skb->len != le16_to_cpu(cqe->total_packet_len))) {
		DP_ERR(edev,
		       "fp[%u] tpa_end total len is %4x but skb has len %04x\n",
		       fp->id, le16_to_cpu(cqe->total_packet_len), skb->len);
		goto err;
	}

#ifndef _HAS_BUILD_SKB_V2 /* ! QEDE_UPSTREAM */
	offset = tpa_info->start_cqe_placement_offset +
		 tpa_info->buffer.page_offset + rxq->rx_headroom;

	memcpy(skb->data,
	       page_address(tpa_info->buffer.data) + offset,
	       tpa_info->start_cqe_bd_len);

	QEDE_DBG_FP_VERBOSE(NETIF_MSG_RX_STATUS,
			    "memcopied %04x bytes from original buffer\n",
			    tpa_info->start_cqe_bd_len);
#endif

	/* Finalize the SKB */
	skb->protocol = eth_type_trans(skb, fp->ndev);
	skb->ip_summed = CHECKSUM_UNNECESSARY;

	/* tcp_gro_complete() will copy NAPI_GRO_CB(skb)->count
	 * to skb_shinfo(skb)->gso_segs
	 */
	NAPI_GRO_CB(skb)->count = le16_to_cpu(cqe->num_of_coalesced_segs);

	qede_gro_receive(edev, fp, skb, tpa_info);

#ifdef ENC_SUPPORTED
	tpa_info->tunnel_type = 0;
	tpa_info->inner_vlan_exist = 0;
#endif
	tpa_info->state = QEDE_AGG_STATE_NONE;
	tpa_info->skb = NULL;

	return 1;
err:
	tpa_info->state = QEDE_AGG_STATE_NONE;
	dev_kfree_skb_any(tpa_info->skb);
	tpa_info->skb = NULL;
#ifdef ENC_SUPPORTED
	tpa_info->tunnel_type = 0;
	tpa_info->inner_vlan_exist = 0;
#endif
	return 0;
}

static u8 qede_check_notunn_csum(u16 flag)
{
	u16 csum_flag = 0;
	u8 csum = 0;

	if (flag & (PARSING_AND_ERR_FLAGS_L4CHKSMWASCALCULATED_MASK <<
		    PARSING_AND_ERR_FLAGS_L4CHKSMWASCALCULATED_SHIFT)) {
		csum_flag |= PARSING_AND_ERR_FLAGS_L4CHKSMERROR_MASK <<
			     PARSING_AND_ERR_FLAGS_L4CHKSMERROR_SHIFT;
		csum = QEDE_CSUM_UNNECESSARY;
	}

	csum_flag |= PARSING_AND_ERR_FLAGS_IPHDRERROR_MASK <<
		     PARSING_AND_ERR_FLAGS_IPHDRERROR_SHIFT;

	if (csum_flag & flag)
		return QEDE_CSUM_ERROR;

	return csum;
}

static u8 qede_check_csum(u16 flag)
{
	if (!qede_tunn_exist(flag))
		return qede_check_notunn_csum(flag);
	else
		return qede_check_tunn_csum(flag);
}

static bool qede_pkt_is_ip_fragmented(struct eth_fast_path_rx_reg_cqe *cqe,
				      u16 flag)
{
	u8 tun_pars_flg = cqe->tunnel_pars_flags.flags;

	if ((tun_pars_flg & (ETH_TUNNEL_PARSING_FLAGS_IPV4_FRAGMENT_MASK <<
			    ETH_TUNNEL_PARSING_FLAGS_IPV4_FRAGMENT_SHIFT)) ||
	    (flag & (PARSING_AND_ERR_FLAGS_IPV4FRAG_MASK <<
		     PARSING_AND_ERR_FLAGS_IPV4FRAG_SHIFT)))
		return true;

	return false;
}

#ifdef _HAS_NDO_XDP /* QEDE_UPSTREAM */
/* Return true iff packet is to be passed to stack */
static bool qede_rx_xdp(struct qede_dev *edev,
			struct qede_fastpath *fp,
			struct qede_rx_queue *rxq,
			struct bpf_prog *prog,
			struct sw_rx_data *bd,
			struct eth_fast_path_rx_reg_cqe *cqe,
			u16 *data_offset, u16 *len)
{
	struct xdp_buff xdp;
	enum xdp_action act;

	/* Some XDP sanities */
	WARN_ON(cqe->bd_num != 1);
	WARN_ON(bd->page_offset);

#ifdef _HAS_NDO_XDP_HARD_START /* QEDE_UPSTREAM */
	xdp.data_hard_start = page_address(bd->data);
	xdp.data = xdp.data_hard_start + *data_offset;
#else
	xdp.data = page_address(bd->data) + *data_offset;
#endif
	xdp.data_end = xdp.data + *len;

#ifdef _HAS_XDP_FRAME_SZ /* QEDE_UPSTREAM */
	xdp.frame_sz = rxq->rx_buf_seg_size; /* PAGE_SIZE when XDP enabled */
#endif
	/* Queues always have a full reset currently, so for the time
	 * being until there's atomic program replace just mark read
	 * side for map helpers.
	 */
	rcu_read_lock();
	act = bpf_prog_run_xdp(prog, &xdp);
	rcu_read_unlock();

#ifdef _HAS_NDO_XDP_HARD_START /* QEDE_UPSTREAM */
	/* Recalculate, as XDP might have changed the headers */
	*data_offset = xdp.data - xdp.data_hard_start;
	*len = xdp.data_end - xdp.data;
#endif

	if (act == XDP_PASS) {
		/* Count number of packets to be passed to stack */
		rxq->xdp_pass++;
		return true;
	}

	/* Count number of packets not to be passed to stack */
	rxq->xdp_no_pass++;

	switch (act) {
	case XDP_TX:
		/* We need the replacement buffer before transmit. */
		if (qede_alloc_rx_buffer(rxq, true)) {
			qede_recycle_rx_bd_ring(rxq, 1);
			trace_xdp_exception(edev->ndev, prog, act);
			return false;
		}

		/* Now if there's a transmission problem, we'd still have to
		 * throw current buffer, as replacement was already allocated.
		 */
		if (qede_xdp_xmit(edev, fp, bd,
				  *data_offset, *len)) {
			if (!qede_add_page_to_pool(rxq, bd->data,
						   bd->mapping, false)) {
				dma_unmap_page(rxq->dev, bd->mapping,
					       PAGE_SIZE, DMA_BIDIRECTIONAL);
				__free_page(bd->data);
			}
			trace_xdp_exception(edev->ndev, prog, act);
		}

		/* Regardless, we've consumed an Rx BD */
		qede_rx_bd_ring_consume(rxq);
		return false;

	default:
#ifdef _HAS_BPF_PROG /* QEDE_UPSTREAM */
		bpf_warn_invalid_xdp_action(edev->ndev, prog, act);
#else
		bpf_warn_invalid_xdp_action(act);
#endif
		/* Fall through */
		COMPAT_FALLTHROUGH;
	case XDP_ABORTED:
		trace_xdp_exception(edev->ndev, prog, act);
		/* Fall through */
		COMPAT_FALLTHROUGH;
	case XDP_DROP:
		qede_recycle_rx_bd_ring(rxq, cqe->bd_num);
	}

	return false;
}
#endif

static int qede_rx_build_jumbo(struct qede_dev *edev,
			       struct qede_rx_queue *rxq,
			       struct sk_buff *skb,
			       struct eth_fast_path_rx_reg_cqe *cqe,
			       u16 first_bd_len)
{
	u16 pkt_len = le16_to_cpu(cqe->pkt_len);
	struct sw_rx_data *bd;
	u16 bd_cons_idx;
	u8 num_frags;

	QEDE_DBG_FP_VERBOSE(NETIF_MSG_RX_STATUS,
			    "Got a Jumbo-over-BD packet: %02x BDs, len on first: %04x, Total Length: %04x\n",
			    cqe->bd_num, len, pkt_len);

	pkt_len -= first_bd_len;

	/* We've already used one BD for the SKB. Now take care of the rest */
	for (num_frags = cqe->bd_num - 1; num_frags > 0; num_frags--) {
		u16 cur_size = pkt_len > rxq->rx_buf_size ? rxq->rx_buf_size :
							    pkt_len;

		if (unlikely(!cur_size)) {
			DP_ERR(edev,
			       "Still got %d BDs for mapping jumbo, but length became 0\n",
				num_frags);
			goto out;
		}

		/* We need a replacement buffer for each BD */
		if (unlikely(qede_alloc_rx_buffer(rxq, true)))
			goto out;

		/* Now that we've allocated the replacement buffer,
		 * we can safely consume the next BD and map it to the SKB.
		 */
		bd_cons_idx = rxq->sw_rx_cons & NUM_RX_BDS_MAX;
		bd = &rxq->sw_rx_ring[bd_cons_idx];
		qede_rx_bd_ring_consume(rxq);

		if (!qede_add_page_to_pool(rxq, bd->data, bd->mapping, true))
			dma_unmap_page(rxq->dev, bd->mapping,
				       PAGE_SIZE, DMA_FROM_DEVICE);

		QEDE_SKB_ADD_RX_FRAG(skb, skb_shinfo(skb)->nr_frags, bd->data,
				     rxq->rx_headroom,
				     cur_size, PAGE_SIZE);

		QEDE_DBG_FP_VERBOSE(NETIF_MSG_RX_STATUS,
				    "Mapped %d buffer of size %04x bytes [Previous length of SKB is %04x/%04x] in address %p\n",
				    skb_shinfo(skb)->nr_frags,
				    cur_size, skb->len, skb->data_len,
				    sw_rx_data->data);

		pkt_len -= cur_size;
	}

	if (unlikely(pkt_len))
		DP_ERR(edev,
		       "Mapped all BDs of jumbo, but still have %d bytes\n",
		       pkt_len);

out:
	return num_frags;
}

static int qede_rx_process_tpa_cqe(struct qede_dev *edev,
				   struct qede_fastpath *fp,
				   struct qede_rx_queue *rxq,
				   union eth_rx_cqe *cqe,
				   enum eth_rx_cqe_type type)
{
	/* TODO - currently only TPA_END counts against budget */

	switch (type) {
	case ETH_RX_CQE_TYPE_TPA_START:
		qede_tpa_start(edev, rxq, &cqe->fast_path_tpa_start);
		return 0;
	case ETH_RX_CQE_TYPE_TPA_CONT:
		qede_tpa_cont(edev, rxq, &cqe->fast_path_tpa_cont);
		return 0;
	case ETH_RX_CQE_TYPE_TPA_END:
		return qede_tpa_end(edev, fp, &cqe->fast_path_tpa_end);
	default:
		return 0;
	}
}

static int qede_rx_process_cqe(struct qede_dev *edev,
			       struct qede_fastpath *fp,
			       struct qede_rx_queue *rxq)
{
#ifdef _HAS_NDO_XDP /* QEDE_UPSTREAM */
	struct bpf_prog *xdp_prog = READ_ONCE(rxq->xdp_prog);
#endif
	struct eth_fast_path_rx_reg_cqe *fp_cqe;
	u16 len, pad, bd_cons_idx, parse_flag;
	enum eth_rx_cqe_type cqe_type;
	union eth_rx_cqe *cqe;
	struct sw_rx_data *bd;
	struct sk_buff *skb;
	__le16 flags;
	u8 csum_flag;

	/* Get the CQE from the completion ring */
	cqe = (union eth_rx_cqe *)qed_chain_consume(&rxq->rx_comp_ring);
	cqe_type = cqe->fast_path_regular.type;

	/* Process an unlikely slowpath event */
	if (unlikely(cqe_type == ETH_RX_CQE_TYPE_SLOW_PATH)) {
		struct eth_slow_path_rx_cqe *sp_cqe;

		QEDE_DBG_FP_VERBOSE((NETIF_MSG_RX_STATUS | NETIF_MSG_INTR),
				    "Got a slowath CQE\n");

		sp_cqe = (struct eth_slow_path_rx_cqe *)cqe;
		edev->ops->eth_cqe_completion(edev->cdev, fp->id, sp_cqe);
		return 0;
	}

	QEDE_DBG_DUMP_PACKET();

	/* Handle TPA cqes */
	if (cqe_type != ETH_RX_CQE_TYPE_REGULAR)
		return qede_rx_process_tpa_cqe(edev, fp, rxq, cqe, cqe_type);

	/* Get the data from the SW ring; Consume it only after its evident
	 * we wouldn't recycle it.
	 */
	bd_cons_idx = rxq->sw_rx_cons & NUM_RX_BDS_MAX;
	bd = &rxq->sw_rx_ring[bd_cons_idx];

	QEDE_DBG_FP_VERBOSE(NETIF_MSG_RX_STATUS,
			    "Non-TPA - consuming buffer from %04x\n",
			    bd_cons_idx);

	fp_cqe = &cqe->fast_path_regular;
	len = le16_to_cpu(fp_cqe->len_on_first_bd);
	pad = fp_cqe->placement_offset + rxq->rx_headroom;

	QEDE_DBG_FP_VERBOSE(NETIF_MSG_RX_STATUS,
			    "CQE type = %x, flags = %x, vlan = %x, len %u, parsing flags = %d\n",
			    cqe_type, fp_cqe->bitfields,
			    le16_to_cpu(fp_cqe->vlan_tag),
			    len, le16_to_cpu(fp_cqe->pars_flags.flags));

#ifdef _HAS_NDO_XDP /* QEDE_UPSTREAM */
	/* Run eBPF program if one is attached */
	if (xdp_prog)
		if (!qede_rx_xdp(edev, fp, rxq, xdp_prog,
				 bd, fp_cqe, &pad, &len))
			return 0;
#endif

	/* If this is an error packet then drop it */
	flags = cqe->fast_path_regular.pars_flags.flags;
	parse_flag = le16_to_cpu(flags);

	csum_flag = qede_check_csum(parse_flag);
	if (unlikely(csum_flag == QEDE_CSUM_ERROR)) {
		if (qede_pkt_is_ip_fragmented(fp_cqe, parse_flag))
			rxq->rx_ip_frags++;
		else
			rxq->rx_csum_errors++;
	}

	/* Basic validation passed; Need to prepare an SKB. This would also
	 * guaranteed to finally consume the first BD upon success.
	 */
#ifdef _HAS_BUILD_SKB_V2 /* QEDE_UPSTREAM */
	skb = qede_rx_build_skb(edev, rxq, bd, len, pad);
#else
	skb = qede_rx_allocate_skb(edev, rxq, bd, len, pad);
#endif
	if (!skb) {
		rxq->rx_alloc_errors++;
		qede_recycle_rx_bd_ring(rxq, fp_cqe->bd_num);
		return 0;
	}

	/* In case of Jumbo packet, several PAGE_SIZEd buffers will be pointed
	 * by a single cqe.
	 */
	if (fp_cqe->bd_num > 1) {
		u16 unmapped_frags = qede_rx_build_jumbo(edev, rxq, skb,
							 fp_cqe, len);

		if (unlikely(unmapped_frags > 0)) {
			qede_recycle_rx_bd_ring(rxq, unmapped_frags);
			dev_kfree_skb_any(skb);
			return 0;
		}
	}

	/* The SKB contains all the data. Now prepare meta-magic */
	skb->protocol = eth_type_trans(skb, fp->ndev);
	qede_get_rxhash(skb, fp_cqe->bitfields, fp_cqe->rss_hash);
	qede_set_skb_csum(skb, csum_flag);
	skb_record_rx_queue(skb, fp->fwd_fp ?
				 (rxq->rxq_id - fp->fwd_dev->base_queue_id) :
				 rxq->rxq_id);
	qede_ptp_record_rx_ts(edev, cqe, skb);

	/* SKB is prepared - pass it to stack */
	qede_skb_receive(edev, fp, rxq, skb, le16_to_cpu(fp_cqe->vlan_tag));

	LEGACY_QEDE_SET_JIFFIES(fp->ndev->last_rx, jiffies);

	return 1;
}

static int qede_rx_int(struct qede_fastpath *fp, int budget)
{
	struct qede_rx_queue *rxq = fp->rxq;
	struct qede_dev *edev = fp->edev;
	int work_done = 0, rcv_pkts = 0;
	u16 hw_comp_cons, sw_comp_cons;

	hw_comp_cons = le16_to_cpu(*rxq->hw_cons_ptr);
	sw_comp_cons = qed_chain_get_cons_idx(&rxq->rx_comp_ring);

	QEDE_DBG_FP_VERBOSE(NETIF_MSG_RX_STATUS,
			    "Rx int on queue[%d]:  hw_comp_cons %u  sw_comp_cons %u\n",
			    fp->id, hw_comp_cons, sw_comp_cons);

	/* Memory barrier to prevent the CPU from doing speculative reads of CQE
	 * / BD in the while-loop before reading hw_comp_cons. If the CQE is
	 * read before it is written by FW, then FW writes CQE and SB, and then
	 * the CPU reads the hw_comp_cons, it will use an old CQE.
	 */
	rmb();

	/* Loop to complete all indicated BDs */
	while ((sw_comp_cons != hw_comp_cons) && (work_done < budget)) {
		rcv_pkts += qede_rx_process_cqe(edev, fp, rxq);
		qed_chain_recycle_consumed(&rxq->rx_comp_ring);
		sw_comp_cons = qed_chain_get_cons_idx(&rxq->rx_comp_ring);
		work_done++;
	}

	rxq->rcv_pkts += rcv_pkts;

	/* Allocate replacement buffers */
	while (rxq->num_rx_buffers - rxq->filled_buffers)
		if (qede_alloc_rx_buffer(rxq, false))
			break;

	/* Update producers */
	qede_update_rx_prod(edev, rxq);

	return work_done;
}

static bool qede_poll_is_more_work(struct qede_fastpath *fp)
{
	qed_sb_update_sb_idx(fp->sb_info);

	if (likely(fp->type & QEDE_FASTPATH_RX))
		if (qede_has_rx_work(fp->rxq))
			return true;

	if (fp->type & QEDE_FASTPATH_XDP)
		if (qede_txq_has_work(fp->xdp_tx))
			return true;

	if (likely(fp->type & QEDE_FASTPATH_TX)) {
		int cos;

		for_each_cos_in_txq(fp->edev, cos) {
			if (qede_txq_has_work(&fp->txq[cos]))
				return true;
		}
	}

	return false;
}

/*********************
 * NDO & API related *
 *********************/
int qede_poll(struct napi_struct *napi, int budget)
{
	struct qede_fastpath *fp = container_of(napi, struct qede_fastpath,
						 napi);
	struct qede_dev *edev = fp->edev;
	int rx_work_done = 0;

#ifdef TIME_FP_DEBUG /* ! QEDE_UPSTREAM */
	qede_log_time(edev, fp, QEDE_FP_TIME_START);
#endif

	if (likely(fp->type & QEDE_FASTPATH_TX)) {
		int cos;

		for_each_cos_in_txq(fp->edev, cos) {
			if (qede_txq_has_work(&fp->txq[cos]))
				qede_tx_int(edev, &fp->txq[cos]);
		}
	}

	if ((fp->type & QEDE_FASTPATH_XDP) &&
	    qede_txq_has_work(fp->xdp_tx))
		qede_xdp_tx_int(edev, fp);

	rx_work_done = (likely(fp->type & QEDE_FASTPATH_RX) &&
			qede_has_rx_work(fp->rxq)) ?
			qede_rx_int(fp, budget) : 0;
	if (rx_work_done < budget) {
		if (!qede_poll_is_more_work(fp)) {
			napi_complete_done(napi, rx_work_done);
			qede_log_napi(fp, 0);

			/* Update and reenable interrupts */
			qede_log_intr(fp, IGU_INT_ENABLE);
			qed_sb_ack(fp->sb_info, IGU_INT_ENABLE, 1);
		} else {
			rx_work_done = budget;
		}
	}

	/* TODO - if NAPI is to be rescheduled, do we still want this now? */
	if (fp->xdp_xmit) {
		u16 xdp_prod = qed_chain_get_prod_idx(&fp->xdp_tx->tx_pbl);

		fp->xdp_xmit = 0;
		fp->xdp_tx->tx_db.data.bd_prod = cpu_to_le16(xdp_prod);
		qede_update_tx_producer(fp->xdp_tx);
	}

#ifdef TIME_FP_DEBUG /* ! QEDE_UPSTREAM */
	qede_log_time(edev, fp,
		      (budget == rx_work_done) ?
		      QEDE_FP_TIME_END_RESCHEDULE :
		      QEDE_FP_TIME_END);
#endif
	qede_log_napi(fp, rx_work_done);
	return rx_work_done;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)) /* QEDE_UPSTREAM */
irqreturn_t qede_msix_fp_int(int irq, void *fp_cookie)
#else
irqreturn_t qede_msix_fp_int(int irq, void *fp_cookie,
			     struct pt_regs *regs)
#endif
{
	struct qede_fastpath *fp = fp_cookie;

	qede_log_intr(fp, IGU_INT_DISABLE);

	qed_sb_ack(fp->sb_info, IGU_INT_DISABLE, 0 /*do not update*/);

	napi_schedule_irqoff(&fp->napi);
	return IRQ_HANDLED;
}

/* Main transmit function */
netdev_tx_t qede_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct qede_dev *edev = netdev_priv(ndev);
	struct netdev_queue *netdev_txq;
	struct qede_tx_queue *txq;
	struct eth_tx_1st_bd *first_bd;
	struct eth_tx_2nd_bd *second_bd = NULL;
	struct eth_tx_3rd_bd *third_bd = NULL;
	struct eth_tx_bd *tx_data_bd = NULL;
	u16 txq_index;
	u8 nbd = 0;
	dma_addr_t mapping;
	int rc, frag_idx = 0, ipv6_ext = 0;
	u8 xmit_type;
	u16 idx;
	u16 hlen;
	bool data_split = false;

	/* Get tx-queue context and netdev index */
	txq_index = skb_get_queue_mapping(skb);

	QEDE_FAST_PATH_BUG_ON(txq_index >=
			      ((QEDE_BASE_TSS_COUNT(edev) +
				edev->fwd_dev_queues) *
			       edev->dev_info.num_tc));

	txq = QEDE_NDEV_TXQ_ID_TO_TXQ(edev, txq_index);
	netdev_txq = netdev_get_tx_queue(txq->fp->ndev, txq->ndev_txq_id);

	QEDE_FAST_PATH_BUG_ON(qed_chain_get_elem_left(&txq->tx_pbl) <
			       (MAX_SKB_FRAGS + 1));

	xmit_type = qede_xmit_type(skb, &ipv6_ext);

/* +2 for 1st BD for headers and 2nd BD for headlen (if required) */
#if ((MAX_SKB_FRAGS + 2) > ETH_TX_MAX_BDS_PER_NON_LSO_PACKET)
	if (qede_pkt_req_lin(skb, xmit_type)) {
		if (skb_linearize(skb)) {
			txq->tx_mem_alloc_err++;

			dev_kfree_skb_any(skb);
			return NETDEV_TX_OK;
		}
	}
#endif
	/* Fill the entry in the SW ring and the BDs in the FW ring */
	idx = txq->sw_tx_prod;
	txq->sw_tx_ring.skbs[idx].skb = skb;
	first_bd = (struct eth_tx_1st_bd *)
		   qed_chain_produce(&txq->tx_pbl);
	memset(first_bd, 0, sizeof(*first_bd)); /* @@@ TBD Will be removed*/
	first_bd->data.bd_flags.bitfields =
		1 << ETH_TX_1ST_BD_FLAGS_START_BD_SHIFT;
#ifdef QEDE_PTP_SUPPORT /* QEDE_UPSTREAM */
	if (unlikely(skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP))
		qede_ptp_tx_ts(edev, skb);
#endif
#ifdef DUMP_PACKET_DATA	/* ! QEDE_UPSTREAM */
	{
		int k;
		printk(KERN_ERR "New Packet (Tx)\n");
		for (k = 0; k < skb_headlen(skb); k++)
			printk(KERN_ERR "%02x: %02x\n", k, ((u8 *)skb->data)[k]);
	}
#endif
	/* Map skb linear data for DMA and set in the first BD */
	mapping = dma_map_single(txq->dev, skb->data,
				 skb_headlen(skb), DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(txq->dev, mapping))) {
		DP_NOTICE(edev, "SKB mapping failed\n");
		qede_free_failed_tx_pkt(txq, first_bd, 0, false);
		qede_update_tx_producer(txq);
		return NETDEV_TX_OK;
	}
	nbd++;
	BD_SET_UNMAP_ADDR_LEN(first_bd, mapping, skb_headlen(skb));

#ifdef FULL_TX_DEBUG
	DP_VERBOSE(edev, NETIF_MSG_TX_QUEUED,
		   "Mapped %d bytes of linear data to first bd\n",
		   first_bd->nbytes);
#endif
	/* In case there is IPv6 with extension headers or LSO we need 2nd and
	 * 3rd BDs.
	 */
	if (unlikely((xmit_type & XMIT_LSO) | ipv6_ext)) {
		second_bd = (struct eth_tx_2nd_bd *)
			qed_chain_produce(&txq->tx_pbl);
		memset(second_bd, 0, sizeof(*second_bd));

		nbd++;
		third_bd = (struct eth_tx_3rd_bd *)
			qed_chain_produce(&txq->tx_pbl);
		memset(third_bd, 0, sizeof(*third_bd));

		nbd++;
		/* We need to fill in additional data in second_bd... */
		tx_data_bd = (struct eth_tx_bd *)second_bd;
	}

#if !defined(OLD_VLAN) /* QEDE_UPSTREAM */
	if (skb_vlan_tag_present(skb)) {
#else
	if ((edev->vlan_group != NULL) && skb_vlan_tag_present(skb)) {
#endif
		first_bd->data.vlan = cpu_to_le16(skb_vlan_tag_get(skb));
		first_bd->data.bd_flags.bitfields |=
			1 << ETH_TX_1ST_BD_FLAGS_VLAN_INSERTION_SHIFT;
	}

	/* Fill the parsing flags & params according to the requested offload */
	if (xmit_type & XMIT_L4_CSUM) {
		/* We don't re-calculate IP checksum as it is already done by
		 * the upper stack
		 */
		first_bd->data.bd_flags.bitfields |=
			1 << ETH_TX_1ST_BD_FLAGS_L4_CSUM_SHIFT;
		if (xmit_type & XMIT_ENC) {
			first_bd->data.bd_flags.bitfields |=
				1 << ETH_TX_1ST_BD_FLAGS_IP_CSUM_SHIFT;

			first_bd->data.bitfields |=
				1 << ETH_TX_DATA_1ST_BD_TUNN_FLAG_SHIFT;
		}

		/* Legacy FW had flipped behavior in regard to this bit -
		 * I.e., needed to set to prevent FW from touching encapsulated
		 * packets when it didn't need to.
		 */
		if (unlikely(txq->is_legacy))
			first_bd->data.bitfields ^=
				1 << ETH_TX_DATA_1ST_BD_TUNN_FLAG_SHIFT;

		/* If the packet is IPv6 with extension header, indicate that
		 * to FW and pass few params, since the device cracker doesn't
		 * support parsing IPv6 with extension header/s.
		 */
		if (unlikely(ipv6_ext))
			qede_set_params_for_ipv6_ext(skb, second_bd, third_bd);
	}

#ifdef NETIF_F_TSO /* QEDE_UPSTREAM */
	if (xmit_type & XMIT_LSO) {
		first_bd->data.bd_flags.bitfields |=
			(1 << ETH_TX_1ST_BD_FLAGS_LSO_SHIFT);
		third_bd->data.lso_mss =
			cpu_to_le16(skb_shinfo(skb)->gso_size);

		if (unlikely(xmit_type & XMIT_ENC)) {
			first_bd->data.bd_flags.bitfields |=
				1 << ETH_TX_1ST_BD_FLAGS_TUNN_IP_CSUM_SHIFT;

#ifdef _HAS_GSO_TUN_L4_CSUM /* QEDE_UPSTREAM */
			if (xmit_type & XMIT_ENC_GSO_L4_CSUM) {
				u8 tmp = ETH_TX_1ST_BD_FLAGS_TUNN_L4_CSUM_SHIFT;

				first_bd->data.bd_flags.bitfields |= 1 << tmp;
			}
#endif
			hlen = qede_get_skb_hlen(skb, true);
		} else {
			first_bd->data.bd_flags.bitfields |=
			1 << ETH_TX_1ST_BD_FLAGS_IP_CSUM_SHIFT;
			hlen = qede_get_skb_hlen(skb, false);
		}

		/* @@@TBD - if will not be removed need to check */
		third_bd->data.bitfields |=
			cpu_to_le16(1 << ETH_TX_DATA_3RD_BD_HDR_NBD_SHIFT);

		/* Make life easier for FW guys who can't deal with header and
		 * data on same BD. If we need to split, use the second bd...
		 */
		if (unlikely(skb_headlen(skb) > hlen)) {
			DP_VERBOSE(edev, NETIF_MSG_TX_QUEUED,
				   "TSO split header size is %d (%x:%x)\n",
				   first_bd->nbytes, first_bd->addr.hi,
				   first_bd->addr.lo);

			mapping = HILO_U64(le32_to_cpu(first_bd->addr.hi),
					   le32_to_cpu(first_bd->addr.lo)) +
					   hlen;

			BD_SET_UNMAP_ADDR_LEN(tx_data_bd, mapping,
					      le16_to_cpu(first_bd->nbytes) -
					      hlen);

			/* this marks the BD as one that has no
			 * individual mapping
			 */
			txq->sw_tx_ring.skbs[idx].flags |= QEDE_TSO_SPLIT_BD;

			first_bd->nbytes = cpu_to_le16(hlen);

			tx_data_bd = (struct eth_tx_bd *)third_bd;
			data_split = true;
		}
	} else {
#endif
		if (skb->len >= ETH_TX_MAX_NON_LSO_PKT_LEN) {
			DP_ERR(edev, "skb->len = %d\n", skb->len);
			qede_free_failed_tx_pkt(txq, first_bd, 0, false);
			qede_update_tx_producer(txq);
			return NETDEV_TX_OK;
		}
		first_bd->data.bitfields |=
			(skb->len & ETH_TX_DATA_1ST_BD_PKT_LEN_MASK) <<
			ETH_TX_DATA_1ST_BD_PKT_LEN_SHIFT;
#ifdef NETIF_F_TSO /* QEDE_UPSTREAM */
	}
#endif

	/* Handle fragmented skb */
	/* special handle for frags inside 2nd and 3rd bds.. */
	while (tx_data_bd && frag_idx < skb_shinfo(skb)->nr_frags) {
		rc = map_frag_to_bd(txq,
				    &skb_shinfo(skb)->frags[frag_idx],
				    tx_data_bd);
		if (rc) {
			qede_free_failed_tx_pkt(txq, first_bd, nbd,
						data_split);
			qede_update_tx_producer(txq);
			return NETDEV_TX_OK;
		}
#ifdef FULL_TX_DEBUG
		if (BD_UNMAP_ADDR(tx_data_bd) == 0)
			BUG();
#endif

		if (tx_data_bd == (struct eth_tx_bd *)second_bd)
			tx_data_bd = (struct eth_tx_bd *)third_bd;
		else
			tx_data_bd = NULL;

		frag_idx++;
	}

	/* map last frags into 4th, 5th .... */
	for (; frag_idx < skb_shinfo(skb)->nr_frags; frag_idx++, nbd++) {
		tx_data_bd = (struct eth_tx_bd *)
			     qed_chain_produce(&txq->tx_pbl);

		memset(tx_data_bd, 0, sizeof(*tx_data_bd));

		rc = map_frag_to_bd(txq,
				    &skb_shinfo(skb)->frags[frag_idx],
				    tx_data_bd);
		if (rc) {
			qede_free_failed_tx_pkt(txq, first_bd, nbd,
						data_split);
			qede_update_tx_producer(txq);
			return NETDEV_TX_OK;
		}
#ifdef FULL_TX_DEBUG
		if (BD_UNMAP_ADDR(tx_data_bd) == 0)
			BUG();
#endif
	}

	/* update the first BD with the actual num BDs */
	first_bd->data.nbds = nbd;

#ifdef FULL_TX_DEBUG
	DP_VERBOSE(edev, NETIF_MSG_TX_QUEUED,
		   "(1): ADDR (%08x:%08x) bd_flags.bdflags.bitfield 0x%x data.bitfield 0x%x data.nbds %d data.vlan %d nbytes %d\n",
	first_bd->addr.hi,
	first_bd->addr.lo,
	first_bd->data.bd_flags.bitfields,
	le16_to_cpu(first_bd->data.bitfields),
	first_bd->data.nbds,
	le16_to_cpu(first_bd->data.vlan),
	le16_to_cpu(first_bd->nbytes));

	if (second_bd) {
		DP_VERBOSE(edev, NETIF_MSG_TX_QUEUED,
			   "(2): ADDR (%08x:%08x) data.tunn_ip_size %d data.bitfields1 0x%x data.bitfields2 0x%x nbytes %d\n",
		second_bd->addr.hi,
		second_bd->addr.lo,
		le16_to_cpu(second_bd->data.tunn_ip_size),
		second_bd->data.bitfields1,
		second_bd->data.bitfields2,
		le16_to_cpu(second_bd->nbytes));
	}
	if (third_bd) {
		DP_VERBOSE(edev, NETIF_MSG_TX_QUEUED,
			   "(3): ADDR (%08x:%08x) bitfields 0x%x lso_mss %d nbytes %d tunn_l4_hdr_start_offset_w 0x%x tunn_hdr_size_w 0x%x\n",
		third_bd->addr.hi,
		third_bd->addr.lo,
		le16_to_cpu(third_bd->data.bitfields),
		le16_to_cpu(third_bd->data.lso_mss),
		le16_to_cpu(third_bd->nbytes),
		third_bd->data.tunn_l4_hdr_start_offset_w,
		third_bd->data.tunn_hdr_size_w);
	}
#endif
	netdev_tx_sent_queue(netdev_txq, skb->len);

	skb_tx_timestamp(skb);

	/* Advance packet producer only before sending the packet since mapping
	 * of pages may fail.
	 */
	txq->sw_tx_prod = (txq->sw_tx_prod + 1) % txq->num_tx_buffers;

	/* 'next page' entries are counted in the producer value */
	txq->tx_db.data.bd_prod =
		cpu_to_le16(qed_chain_get_prod_idx(&txq->tx_pbl));

#ifdef FULL_TX_DEBUG
	debug_print_tx_queued(edev, txq_index, xmit_type,
			      first_bd, second_bd, third_bd);
#endif

#if defined(_HAS_XMIT_MORE) || defined(_HAS_NDEV_XMIT_MORE) /* QEDE_UPSTREAM */
#ifdef _HAS_NDEV_XMIT_MORE
	if (!netdev_xmit_more() || netif_xmit_stopped(netdev_txq))
#else
	if (!skb->xmit_more || netif_xmit_stopped(netdev_txq))
#endif
		qede_update_tx_producer(txq);
#else
	qede_update_tx_producer(txq);
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 31)) /* ! QEDE_UPSTREAM */
	/* In kernels starting from 2.6.31 netdev layer does this */
	dev->trans_start = jiffies;
#endif

	if (unlikely(qed_chain_get_elem_left(&txq->tx_pbl)
		      < (MAX_SKB_FRAGS + 1))) {
#if defined(_HAS_XMIT_MORE) || defined(_HAS_NDEV_XMIT_MORE) /* QEDE_UPSTREAM */
#ifdef _HAS_NDEV_XMIT_MORE /* QEDE_UPSTREAM */
		if (netdev_xmit_more())
#else
		if (skb->xmit_more)
#endif
			qede_update_tx_producer(txq);
#else
		qede_update_tx_producer(txq);
#endif
		netif_tx_stop_queue(netdev_txq);
		txq->stopped_cnt++;
		DP_VERBOSE(edev, NETIF_MSG_TX_QUEUED,
			   "Stop queue was called\n");
		/* paired memory barrier is in qede_tx_int(), we have to keep
		 * ordering of set_bit() in netif_tx_stop_queue() and read of
		 * fp->bd_tx_cons
		 */
		smp_mb();

		if (qed_chain_get_elem_left(&txq->tx_pbl)
		     >= (MAX_SKB_FRAGS + 1) &&
		    (edev->state == QEDE_STATE_OPEN)) {
			netif_tx_wake_queue(netdev_txq);
			DP_VERBOSE(edev, NETIF_MSG_TX_QUEUED,
				   "Wake queue was called\n");
		}
	}

	return NETDEV_TX_OK;
}

#ifdef CONFIG_DEBUG_FS
netdev_tx_t qede_start_xmit_tx_timeout(struct sk_buff *skb,
				       struct net_device *ndev)
{
	struct qede_dev *edev = netdev_priv(ndev);
	struct netdev_queue *netdev_txq;
	struct qede_tx_queue *txq;
	u16 txq_index;

	txq_index = skb_get_queue_mapping(skb);
	txq = QEDE_NDEV_TXQ_ID_TO_TXQ(edev, txq_index);
	netdev_txq = netdev_get_tx_queue(txq->fp->ndev, txq->ndev_txq_id);

	netdev_tx_sent_queue(netdev_txq, skb->len);
	return NETDEV_TX_OK;
}
#endif

#ifdef QEDE_SELECTQUEUE_HAS_SBDEV_PARAM /* QEDE_UPSTREAM */
u16 qede_select_queue(struct net_device *dev, struct sk_buff *skb,
		      struct net_device *sb_dev)
#elif defined(QEDE_SELECTQUEUE_HAS_FALLBACK_SBDEV_PARAM)
u16 qede_select_queue(struct net_device *dev, struct sk_buff *skb,
		      struct net_device *sb_dev,
		      select_queue_fallback_t fallback)
#elif defined(QEDE_SELECTQUEUE_HAS_FALLBACK_PARAM)
u16 qede_select_queue(struct net_device *dev, struct sk_buff *skb,
		      void *accel_priv, select_queue_fallback_t fallback)
#elif defined(QEDE_SELECTQUEUE_HAS_ACCEL_PARAM)
u16 qede_select_queue(struct net_device *dev, struct sk_buff *skb,
		      void *accel_priv)
#else
u16 qede_select_queue(struct net_device *dev, struct sk_buff *skb)
#endif
{
	struct qede_dev *edev = netdev_priv(dev);
	int total_txq;

	total_txq = QEDE_BASE_TSS_COUNT(edev) * edev->dev_info.num_tc;

#if (LINUX_STARTING_AT_VERSION(3, 9, 0)) /* QEDE_UPSTREAM */
#if defined(QEDE_SELECTQUEUE_HAS_ACCEL_PARAM)
	if (accel_priv) {
		struct qede_fwd_dev *fwd_dev = accel_priv;

		return skb_get_queue_mapping(skb) + fwd_dev->base_queue_id;
	}
#endif
#ifdef QEDE_SELECTQUEUE_HAS_SBDEV_PARAM /* QEDE_UPSTREAM */
	return QEDE_BASE_TSS_COUNT(edev) ?
		netdev_pick_tx(dev, skb, NULL) % total_txq :  0;
#elif defined(QEDE_SELECTQUEUE_HAS_FALLBACK_SBDEV_PARAM)
	return QEDE_BASE_TSS_COUNT(edev) ?
		fallback(dev, skb, NULL) % total_txq :  0;
#elif defined(QEDE_SELECTQUEUE_HAS_FALLBACK_PARAM)
	return QEDE_BASE_TSS_COUNT(edev) ?
		fallback(dev, skb) % total_txq :  0;
#else
	return QEDE_BASE_TSS_COUNT(edev) ?
		__netdev_pick_tx(dev, skb) % total_txq : 0;
#endif
#else
	return QEDE_BASE_TSS_COUNT(edev) ?
		__skb_tx_hash(dev, skb, QEDE_BASE_TSS_COUNT(edev)) %
		total_txq : 0;
#endif
}

#ifdef _HAS_NDO_FEATURES_CHECK /* QEDE_UPSTREAM */
/* 8B udp header + 8B base tunnel header + 32B option length */
#define QEDE_MAX_TUN_HDR_LEN 48

netdev_features_t qede_features_check(struct sk_buff *skb,
				      struct net_device *dev,
				      netdev_features_t features)
{
	features = vlan_features_check(skb, features);

	if (skb->encapsulation) {
		u8 l4_proto = 0;

		switch (vlan_get_protocol(skb)) {
		case htons(ETH_P_IP):
			l4_proto = ip_hdr(skb)->protocol;
			break;
		case htons(ETH_P_IPV6):
			l4_proto = ipv6_hdr(skb)->nexthdr;
			break;
		default:
			return features;
		}

		/* Disable offloads for geneve tunnels, as HW can't parse
		 * the geneve header which has option length greater than 32b
		 * and disable offloads for the ports which are not offloaded.
		 */
		if (l4_proto == IPPROTO_UDP) {
			struct qede_dev *edev = netdev_priv(dev);
			u16 hdrlen, vxln_port, gnv_port;

			hdrlen = QEDE_MAX_TUN_HDR_LEN;
			vxln_port = edev->vxlan_dst_port;
			gnv_port = edev->geneve_dst_port;

			if ((skb_inner_mac_header(skb) -
			     skb_transport_header(skb)) > hdrlen ||
			     (ntohs(udp_hdr(skb)->dest) != vxln_port &&
			      ntohs(udp_hdr(skb)->dest) != gnv_port))
				return features & ~(NETIF_F_CSUM_MASK |
						    NETIF_F_GSO_MASK);
		} else if (l4_proto == IPPROTO_IPIP) {
			/* IPIP tunnels are unknown to the device or at least unsupported natively,
			 * offloads for them can't be done trivially, so disable them for such skb.
			 */
			return features & ~(NETIF_F_CSUM_MASK | NETIF_F_GSO_MASK);
		}
	}

	return features;
}
#endif

#ifdef CONFIG_NET_POLL_CONTROLLER
/* This is used by netconsole to send skbs without having to re-enable
 * interrupts.  It's not called while the normal interrupt routine is executing.
 */
void qede_poll_controller(struct net_device *dev)
{
	struct qede_dev *edev = netdev_priv(dev);
	int i;

	for_each_queue(i)
		napi_schedule(&edev->fp_array[i].napi);
}
#endif
