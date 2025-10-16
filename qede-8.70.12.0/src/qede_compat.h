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

#ifndef _QEDE_COMPAT_H_
#define _QEDE_COMPAT_H_
#ifndef __OFED_BUILD__
#include <linux/version.h>
#include <linux/skbuff.h>
#include <linux/random.h>
#include <linux/etherdevice.h>
#include <linux/tcp.h>
#include <linux/ethtool.h>
#ifdef CONFIG_QEDE_VXLAN /* QEDE_UPSTREAM */
#include <net/vxlan.h>
#endif

#ifdef _HAS_MMIOWB_SPIN_LOCK /* QEDE_UPSTREAM */
#define mmiowb() do { } while (0)
#endif

#ifndef _HAS_IP_PROTO_GRE
#define IPPROTO_GRE 47
#endif

#ifndef _HAS_GRE_H_SECTION
#define GRE_HEADER_SECTION 4
#endif

#if defined(_HAS_TC_FLOWER) || defined(_HAS_FLOW_BLOCK_OFFLOAD) /* QEDE_UPSTREAM */
#include <linux/tc_act/tc_mirred.h>
#include <net/tc_act/tc_mirred.h>

#ifndef to_mirred
#define to_mirred(a) ((struct tcf_mirred *)a)
#endif

#ifndef _HAS_TC_MIRRED
static inline bool is_tcf_mirred_egress_redirect(const struct tc_action *a)
{
#ifdef CONFIG_NET_CLS_ACT
	if (a->ops && a->ops->type == TCA_ACT_MIRRED)
		return to_mirred(a)->tcfm_eaction == TCA_EGRESS_REDIR;
#endif
	return false;
}

#ifndef _HAS_TC_MIRRED_IFIDX
static inline int tcf_mirred_ifindex(const struct tc_action *a)
{
	return to_mirred(a)->tcfm_ifindex;
}
#endif
#endif
#endif

#ifdef _HAS_TC_BLOCK
#include <net/pkt_cls.h>
static inline int qede_block_cb_register(struct tc_block_offload *f,
					 tc_setup_cb_t *cb, void *cb_ident,
					 void *cb_priv)
{
#ifdef _HAS_TC_EXT_ACK /* QEDE_UPSTREAM */
	return tcf_block_cb_register(f->block, cb, cb_ident, cb_priv,
				     f->extack);
#else
	return tcf_block_cb_register(f->block, cb, cb_ident, cb_priv);
#endif
}
#endif

#ifndef NETIF_F_CSUM_MASK
#define NETIF_F_CSUM_MASK	(NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM | \
				 NETIF_F_HW_CSUM)
#endif

#if !defined(_HAS_PAGE_PFMEMALLOC_API) && defined(_HAS_PAGE_PFMEMALLOC)
static inline bool page_is_pfmemalloc(struct page *page)
{
	return page->pfmemalloc;
}
#endif

#ifndef _HAS_PAGE_REF_COUNT
static inline int page_ref_count(struct page *page)
{
	return page_count(page);
}
#endif

#ifndef _HAS_PAGE_REF_INC
static inline void page_ref_inc(struct page *page)
{
	atomic_inc(&page->_count);
}
#endif

#ifndef _HAS_SKB_HASH_RAW
#ifndef _HAS_SKB_GET_RXHASH
static inline __u32 skb_get_rxhash(struct sk_buff *skb)
{
	return skb->rxhash;
}
#endif
static inline __u32 skb_get_hash_raw(const struct sk_buff *skb)
{
	struct sk_buff *sk = (struct sk_buff *)(skb);

	return skb_get_rxhash(sk);
}
#endif

#define HAS_ETHTOOL(feat) (defined(_HAS_ETHTOOL_ ## feat) || defined(_HAS_ETHTOOL_EXT_ ## feat))
#define HAS_NDO(feat)	(defined(_HAS_NDO_ ## feat) || defined (_HAS_NDO_EXT_ ## feat))

#ifndef RHEL_RELEASE_VERSION
#define RHEL_RELEASE_VERSION(a, b) 0
#endif

#ifndef skb_vlan_tag_present
#define skb_vlan_tag_present	vlan_tx_tag_present
#endif

#ifndef skb_vlan_tag_get
#define skb_vlan_tag_get	vlan_tx_tag_get
#endif

#define LINUX_PRE_VERSION(a, b, c) \
	(LINUX_VERSION_CODE < KERNEL_VERSION((a), (b), (c)))
#define LINUX_POST_VERSION(a, b, c) \
	(LINUX_VERSION_CODE > KERNEL_VERSION((a), (b), (c)))
#define LINUX_STARTING_AT_VERSION(a, b, c) \
	(LINUX_VERSION_CODE >= KERNEL_VERSION((a), (b), (c)))

/* RHEL version macros */
#define NOT_RHEL_OR_PRE_VERSION(a, b) \
	(!defined(RHEL_RELEASE_CODE) || \
	 RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION((a), (b)))
#define RHEL_PRE_VERSION(a, b) \
	(defined(RHEL_RELEASE_CODE) && \
	 RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION((a), (b)))
#define RHEL_STARTING_AT_VERSION(a, b) \
	(defined(RHEL_RELEASE_CODE) && \
	 RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION((a), (b)))
#define RHEL_IS_VERSION(a, b) \
	(defined(RHEL_RELEASE_CODE) && \
	 RHEL_RELEASE_CODE ==  RHEL_RELEASE_VERSION((a), (b)))

/* Due to lack of C99 support in MSVC */
#ifndef MSVC
#define INIT_STRUCT_FIELD(field, value) .field = value
#else
#define INIT_STRUCT_FIELD(field, value) value
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)) && !defined(NETIF_F_MULTI_QUEUE)
#define QEDE_MULTI_QUEUE
#endif

/* Always support SRIOV in oob driver if possible */
#if !defined(CONFIG_QED_SRIOV) && defined(SYS_INC_SRIOV)
#define CONFIG_QED_SRIOV
#endif

#ifndef SUPPORTED_40000baseLR4_Full
#define SUPPORTED_40000baseLR4_Full	(1 << 26)
#endif

#ifndef SUPPORTED_20000baseKR2_Full
#define SUPPORTED_20000baseKR2_Full	(1 << 22)
#endif

#ifndef SPEED_20000
#define SPEED_20000	20000
#endif
#ifndef SPEED_25000
#define SPEED_25000	25000
#endif
#ifndef SPEED_40000
#define SPEED_40000	40000
#endif
#ifndef SPEED_50000
#define SPEED_50000	50000
#endif
#ifndef SPEED_100000
#define SPEED_100000	100000
#endif

#if defined(_DEFINE_NETIF_GET_NUM_DEFAULT_RSS_QUEUES)
static inline int netif_get_num_default_rss_queues(void)
{
	return min_t(int, 8, num_online_cpus());
}
#endif

#ifndef NAPI_POLL_WEIGHT
#define NAPI_POLL_WEIGHT      64
#endif

#if (LINUX_PRE_VERSION(2, 6, 37) && NOT_RHEL_OR_PRE_VERSION(6, 4))
static inline int netif_set_real_num_rx_queues(struct net_device *dev, int num)
{
	return 0;
}
#endif

#ifndef QEDE_MULTI_QUEUE
#define	netif_tx_stop_queue(txq)		netif_stop_queue(edev->ndev)
#define netif_tx_wake_queue(txq)		netif_wake_queue(edev->ndev)
#define netif_tx_queue_stopped(txq)		netif_queue_stopped(edev->ndev)
#define __netif_tx_lock(txq, y)			netif_tx_lock(edev->ndev)
#define __netif_tx_unlock(txq)			netif_tx_unlock(edev->ndev)
#endif

#ifndef NETIF_F_HW_CSUM_ENC
#define NETIF_F_HW_CSUM_ENC 0
#endif

#if ((defined(NETIF_F_GSO_GRE) && !defined(RHEL_RELEASE_CODE)) || \
     (defined(NETIF_F_GSO_GRE) && defined(RHEL_RELEASE_CODE) && \
     (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 0))))
#define ENC_SUPPORTED 1
#else
#define skb_inner_network_header skb_network_header
#define skb_inner_transport_header skb_transport_header
#define skb_inner_mac_header skb_mac_header
#define inner_tcp_hdrlen tcp_hdrlen

#ifndef NETIF_F_GSO_GRE
#define NETIF_F_GSO_GRE 0
#endif

static inline struct ipv6hdr *ipv6_inner_hdr(const struct sk_buff *skb)
{
	return (struct ipv6hdr *)skb_inner_network_header(skb);
}

static inline struct iphdr *ip_inner_hdr(const struct sk_buff *skb)
{
	return (struct iphdr *)skb_inner_network_header(skb);
}

static inline bool skb_inner_mac_was_set(const struct sk_buff *skb)
{
	return false;
}

static inline struct ethhdr *eth_inner_hdr(const struct sk_buff *skb)
{
	return (struct ethhdr *)skb_inner_mac_header(skb);
}

#if defined(_DEFINE_INNER_TCP_HDR)
static inline unsigned int inner_tcp_hdrlen(const struct sk_buff *skb)
{
	return (unsigned int)tcp_hdrlen(skb);
}
#endif
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0))
#define skb_frag_size(frag) ((frag)->size)
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36) && (!defined(RHEL_RELEASE_CODE) || RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(6, 4)))
static inline void skb_tx_timestamp(struct sk_buff *skb)
{
	return;
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0))
#define netdev_features_t u32
#endif

#if defined(_DEFINE_ETHER_ADDR_COPY)
static inline void ether_addr_copy(u8 *dst, const u8 *src)
{
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS)
	*(u32 *)dst = *(const u32 *)src;
	*(u16 *)(dst + 4) = *(const u16 *)(src + 4);
#else
	u16 *a = (u16 *)dst;
	const u16 *b = (const u16 *)src;

	a[0] = b[0];
	a[1] = b[1];
	a[2] = b[2];
#endif
}
#endif

#if defined(_DEFINE_ETHER_ADDR_EQUAL)
static inline bool ether_addr_equal(const u8 *addr1, const u8 *addr2)
{
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS)
	u32 fold = ((*(const u32 *)addr1) ^ (*(const u32 *)addr2)) |
		   ((*(const u16 *)(addr1 + 4)) ^ (*(const u16 *)(addr2 + 4)));

	return fold == 0;
#else
	const u16 *a = (const u16 *)addr1;
	const u16 *b = (const u16 *)addr2;

	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) == 0;
#endif
}
#endif

#ifndef _HAS_ETH_HW_ADDR_SET /* ! QEDE_UPSTREAM */
static inline void eth_hw_addr_set(struct net_device *dev, const u8 *addr)
{
	ether_addr_copy(dev->dev_addr, addr);
}
#endif

#if defined(_DEFINE_NAPI_SCHEDULE_IRQOFF)
#define napi_schedule_irqoff	napi_schedule
#endif

#ifdef _DEFINE_CYCLECOUNTER_MASK
#define CYCLECOUNTER_MASK CLOCKSOURCE_MASK
#endif

#ifndef _HAS_TIMESPEC64
#define timespec64_to_ns(ts)  ((ts)->tv_sec * NSEC_PER_SEC + (ts)->tv_nsec)
#endif

#if defined(_DEFINE_NETDEV_TX_COMPLETED_QUEUE)
static inline void netdev_tx_completed_queue(struct netdev_queue *q,
					    unsigned int a, unsigned int b) { }
static inline void netdev_tx_reset_queue(struct netdev_queue *q) { }
static inline void netdev_tx_sent_queue(struct netdev_queue *q,
					unsigned int len) { }
#endif
#if defined(_DEFINE_ETHTOOL_RXFH_INDIR_DEFAULT)
static inline u32 ethtool_rxfh_indir_default(u32 index, u32 n_rx_rings)
{
	return index % n_rx_rings;
}
#endif

#ifndef QEDE_MULTI_QUEUE
#define alloc_etherdev_mqs(x, y, z) alloc_etherdev(x)
#define skb_get_queue_mapping(skb) 0
#define netdev_get_tx_queue(ndev, txq_index) 0
#endif

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 38)) && defined(QEDE_MULTI_QUEUE) && NOT_RHEL_OR_PRE_VERSION(6, 2))
/* Older kernels do not support different amount of mqs.
 * Only txqs is used for TX structure allocation.
 */
static inline struct net_device *alloc_etherdev_mqs(int sizeof_priv,
						    unsigned int txqs,
						    unsigned int rxqs)
{
	return alloc_etherdev_mq(sizeof_priv, txqs);
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22))
#define	SET_NETDEV_DEV(x, y) 	\
	SET_MODULE_OWNER(x); SET_NETDEV_DEV(x, y)
#endif

#ifdef _DEFINE_DMA_ALLOC_COHERENT
#define dma_alloc_coherent(z, size, y, flags)	\
	pci_alloc_consistent(edev->pdev, size, y)
#define	dma_free_coherent(z, size, x, y)	\
	pci_free_consistent(edev->pdev, size, x, y)
#endif

#ifndef QEDE_MULTI_QUEUE
#define netif_set_real_num_tx_queues(x, y)	0
#endif

#ifndef eth_hw_addr_random
#define eth_hw_addr_random(dev) random_ether_addr(dev->dev_addr)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)) && \
	(!defined(RHEL_RELEASE_CODE) || RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(6, 3))
static inline void usleep_range(unsigned long min, unsigned long max)
{
	if (min < 1000)
		udelay(min);
	else
		msleep(min / 1000);
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0))
#undef __devinit
#define __devinit
#undef __devexit
#define __devexit
#undef __devinitdata
#define __devinitdata
#undef __devexit_p
#define __devexit_p(val)	val
#endif

#ifdef _DEFINE_SKB_FRAG_DMA_MAP
#define skb_frag_dma_map(x, y, z, v, w) \
	dma_map_single(x, page_address((y)->page) + z, v, w)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27))
#define dma_mapping_error(x, y) dma_mapping_error(y)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37)) && (!defined(RHEL_RELEASE_CODE) || (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7, 0)))
#define OLD_VLAN
#endif

#if defined(OLD_VLAN) && (defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE))
#define BCM_VLAN			1
#endif

#ifndef NETIF_F_HW_VLAN_CTAG_FILTER
#ifdef NETIF_F_HW_VLAN_FILTER
#define NETIF_F_HW_VLAN_CTAG_FILTER	NETIF_F_HW_VLAN_FILTER
#else
#define NETIF_F_HW_VLAN_CTAG_FILTER 0
#endif
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
#define VLAN_8021AD_ADDED
#else
#define __vlan_hwaccel_put_tag(x, y, z) __vlan_hwaccel_put_tag(x,z)
#endif

#ifndef NETIF_F_HW_VLAN_CTAG_RX
#define NETIF_F_HW_VLAN_CTAG_RX	NETIF_F_HW_VLAN_RX
#endif

#ifndef NETIF_F_HW_VLAN_CTAG_TX
#define NETIF_F_HW_VLAN_CTAG_TX	NETIF_F_HW_VLAN_TX
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19))

/* The below code is similar to what is done random32() in
 * 2.6.19 but much simpler. ;)
 */
#if !defined(RHEL_RELEASE_CODE)
#define TAUSWORTHE(s, a, b, c, d) \
	((((s)&(c)) << (d)) ^ ((((s) << (a)) ^ (s)) >> (b)))
static inline u32 random32(void)
{
	static u32 s1 = 4294967294UL;
	static u32 s2 = 4294967288UL;
	static u32 s3 = 4294967280UL;
	u32 cycles;

	/* This would be our seed for this step */
	cycles = get_cycles();

	s1 = TAUSWORTHE(s1 + cycles, 13, 19, 4294967294UL, 12);
	s2 = TAUSWORTHE(s2 + cycles, 2, 25, 4294967288UL, 4);
	s3 = TAUSWORTHE(s3 + cycles, 3, 11, 4294967280UL, 17);

	return s1 ^ s2 ^ s3;
}
#elif (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(5, 6))
#define TAUSWORTHE(s, a, b, c, d) \
	((((s)&(c)) << (d)) ^ ((((s) << (a)) ^ (s)) >> (b)))
static inline u32 random32(void)
{
	static u32 s1 = 4294967294UL;
	static u32 s2 = 4294967288UL;
	static u32 s3 = 4294967280UL;
	u32 cycles;

	/* This would be our seed for this step */
	cycles = get_cycles();

	s1 = TAUSWORTHE(s1 + cycles, 13, 19, 4294967294UL, 12);
	s2 = TAUSWORTHE(s2 + cycles, 2, 25, 4294967288UL, 4);
	s3 = TAUSWORTHE(s3 + cycles, 3, 11, 4294967280UL, 17);

	return s1 ^ s2 ^ s3;
}
#endif
#endif

#ifdef _HAS_SIMPLE_WRITE_TO_BUFFER
static inline ssize_t simple_write_to_buffer(void *to, size_t available,
					loff_t *ppos, const void __user *from,
					 size_t count)
{
	loff_t pos = *ppos;
	size_t res;

	if (pos < 0)
		return -EINVAL;
	if (pos >= available || !count)
		return 0;
	if (count > available - pos)
		count = available - pos;
		res = copy_from_user(to + pos, from, count);
	if (res == count)
		return -EFAULT;
		count -= res;
		*ppos = pos + count;
	return count;
}
#endif

#ifdef CONFIG_DEBUG_FS
#ifdef _DEFINE_SIMPLE_OPEN
static inline int simple_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;
	return 0;
}
#endif
#endif

#if defined(_DEFINE_PRANDOM_BYTES_)
static inline void prandom_bytes(void *buf, int bytes)
{
	int i;

	for (i = 0; i < bytes / 4; i++)
		((u32 *)buf)[i] = random32();
}
#endif

#if LINUX_PRE_VERSION(2, 6, 36)
#define usleep_range(a, b)     msleep((a) / 1000)
#endif

#if (LINUX_PRE_VERSION(2, 6, 31))
#define LEGACY_QEDE_SET_JIFFIES(a, jiffies) \
	a = (jiffies)
#else
#define LEGACY_QEDE_SET_JIFFIES(a, jiffies)
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 31))
#define DEV_ADDR_LIST netdev_hw_addr
#else
#define DEV_ADDR_LIST dev_addr_list
#endif

#if !defined(netdev_hw_addr_list_for_each)
#define QEDE_MC_ADDR dev_mc_list
#define qede_mc_addr(ha) ha->dmi_addr
#else
#define QEDE_MC_ADDR netdev_hw_addr
#define qede_mc_addr(ha) ha->addr
#endif

#define QEDE_FAST_PATH_BUG_ON(e) BUG_ON(e)

#ifdef _HAS_BUILD_SKB
#define QEDE_RX_DATA	u8
#define QEDE_ALLOC_RX_DATA(x, y) kmalloc(x, y)
#define QEDE_RX_DATA_PTR(_data) (_data)
#define QEDE_FREE_RX_DATA(data) kfree(data)
#ifdef _HAS_BUILD_SKB_V2
#define QEDE_BUILD_SKB(data) build_skb(data, 0)
#else
#define QEDE_BUILD_SKB(data) build_skb(data)
#endif
#else
#define QEDE_RX_DATA	struct sk_buff
#define QEDE_ALLOC_RX_DATA(x, y) \
	netdev_alloc_skb(edev->ndev, (x) - NET_SKB_PAD);
#define QEDE_RX_DATA_PTR(_data) (_data)->data
#define QEDE_FREE_RX_DATA(data) dev_kfree_skb_any(data)
#define QEDE_BUILD_SKB(data) data
#endif

#ifdef TCP_V4_CHECK_NEW /* QEDE_UPSTREAM */
#define tcp_v4_check(x, y, z, w) tcp_v4_check(th, x, y, z, w)
#endif

#if defined(_DEFINE_RSS_KEY_FILL)
#define netdev_rss_key_fill	prandom_bytes
#endif

/* SLES version macros */
#define NOT_SLES_OR_PRE_VERSION(a) \
	(!defined(SLES_DISTRO) || (SLES_DISTRO < (a)))
#define SLES_STARTING_AT_VERSION(a) \
	(defined(SLES_DISTRO) && (SLES_DISTRO >= (a)))
#define SLES11_SP3	0x1103
#define SLES11_SP4	0x1104
#define SLES12_SP1	0x1201

#if (LINUX_PRE_VERSION(3, 8, 0))
#ifndef _HAVE_ARCH_IPV6_CSUM
#if ((defined(RHEL_RELEASE_CODE) && (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6, 0))) || (defined(SLES_DISTRO) && (SLES_DISTRO > SLES11_SP1)))
#include <net/ip6_checksum.h>
#else
static inline __sum16 csum_ipv6_magic(const struct in6_addr *saddr,
					  const struct in6_addr *daddr,
					  __u32 len, unsigned short proto,
					  __wsum csum)
{

	int carry;
	__u32 ulen;
	__u32 uproto;
	__u32 sum = (__force u32)csum;

	sum += (__force u32)saddr->s6_addr32[0];
	carry = (sum < (__force u32)saddr->s6_addr32[0]);
	sum += carry;

	sum += (__force u32)saddr->s6_addr32[1];
	carry = (sum < (__force u32)saddr->s6_addr32[1]);
	sum += carry;

	sum += (__force u32)saddr->s6_addr32[2];
	carry = (sum < (__force u32)saddr->s6_addr32[2]);
	sum += carry;

	sum += (__force u32)saddr->s6_addr32[3];
	carry = (sum < (__force u32)saddr->s6_addr32[3]);
	sum += carry;

	sum += (__force u32)daddr->s6_addr32[0];
	carry = (sum < (__force u32)daddr->s6_addr32[0]);
	sum += carry;

	sum += (__force u32)daddr->s6_addr32[1];
	carry = (sum < (__force u32)daddr->s6_addr32[1]);
	sum += carry;

	sum += (__force u32)daddr->s6_addr32[2];
	carry = (sum < (__force u32)daddr->s6_addr32[2]);
	sum += carry;

	sum += (__force u32)daddr->s6_addr32[3];
	carry = (sum < (__force u32)daddr->s6_addr32[3]);
	sum += carry;

	ulen = (__force u32)htonl((__u32) len);
	sum += ulen;
	carry = (sum < ulen);
	sum += carry;

	uproto = (__force u32)htonl(proto);
	sum += uproto;
	carry = (sum < uproto);
	sum += carry;

	return csum_fold((__force __wsum)sum);
}
#endif
#endif
#if defined(_DEFINE_TCP_V6_CHECK_)
static inline __sum16 tcp_v6_check(int len,
					struct in6_addr *saddr,
					struct in6_addr *daddr,
					__wsum base)
{
	return csum_ipv6_magic(saddr, daddr, len, IPPROTO_TCP, base);
}
#endif
#endif

#if (LINUX_STARTING_AT_VERSION(2, 6, 21))
#define TCP_V4_CHECK_NEW
#endif

#ifdef _DEFINE_SKB_SET_HASH
enum pkt_hash_types {
	PKT_HASH_TYPE_NONE,     /* Undefined type */
	PKT_HASH_TYPE_L2,       /* Input: src_MAC, dest_MAC */
	PKT_HASH_TYPE_L3,       /* Input: src_IP, dst_IP */
	PKT_HASH_TYPE_L4,       /* Input: src_IP, dst_IP, src_port, dst_port */
};

static inline void
skb_set_hash(struct sk_buff *skb, __u32 hash, enum pkt_hash_types type)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0))
	skb->l4_rxhash = (type == PKT_HASH_TYPE_L4);
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 34))
	skb->rxhash = hash;
#endif
}
#endif

#ifndef _HAS_NETDEV_NOTIFIER
#define netdev_notifier_info_to_dev(ptr)	((struct net_device *)(ptr))
#endif

#if defined(ETHTOOL_GPERMADDR) && LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0)
#define LEGACY_QEDE_SET_PERM_ADDR(edev) ether_addr_copy((edev)->ndev->perm_addr, \
							(edev)->ndev->dev_addr)
#else
#define LEGACY_QEDE_SET_PERM_ADDR(edev)	do {} while(0)
#endif

/* empty features */
#ifndef NETIF_F_GRO
#define NETIF_F_GRO			0
#endif
#ifndef NETIF_F_TSO
#define NETIF_F_TSO			0
#define NETIF_F_TSO_ECN			0
#endif
#ifndef NETIF_F_TSO6
#define NETIF_F_TSO6			0
#endif
#ifndef NETIF_F_RXHASH
#define NETIF_F_RXHASH			0
#endif

#else

#ifdef _HAS_BUILD_SKB
#define QEDE_RX_DATA	u8
#define QEDE_ALLOC_RX_DATA(x, y) kmalloc(x, y)
#define QEDE_RX_DATA_PTR(_data) (_data)
#define QEDE_FREE_RX_DATA(data) kfree(data)
#ifdef _HAS_BUILD_SKB_V2
#define QEDE_BUILD_SKB(data) build_skb(data, 0)
#else
#define QEDE_BUILD_SKB(data) build_skb(data)
#endif
#else
#define QEDE_RX_DATA	struct sk_buff
#define QEDE_ALLOC_RX_DATA(x, y) \
	netdev_alloc_skb(edev->ndev, (x) - NET_SKB_PAD);
#define QEDE_RX_DATA_PTR(_data) ((_data)->data)
#define QEDE_FREE_RX_DATA(data) dev_kfree_skb_any(data)
#define QEDE_BUILD_SKB(data) data
#endif

#endif /* __OFED_BUILD__ */

#ifndef _HAS_GET_HEAD_LEN
#define QEDE_GET_HLEN(x, y)	min_t(u16, y, QEDE_RX_HDR_SIZE)
#else
#define QEDE_GET_HLEN(x, y)	eth_get_headlen(x, QEDE_RX_HDR_SIZE)
#endif

#ifndef SPEED_UNKNOWN
#define SPEED_UNKNOWN (-1)
#endif

#ifndef DUPLEX_UNKNOWN
#define DUPLEX_UNKNOWN (0xff)
#endif

#ifndef IPV4_FLOW
#define IPV4_FLOW	0x10
#endif

#ifndef IPV6_FLOW
#define IPV6_FLOW	0x11
#endif

#ifndef __always_unused
#define __always_unused
#endif

/* For rhel6.2 or ubuntu 11.4 */
#if RHEL_IS_VERSION(6, 2) || (LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 38))
#define QEDE_SKB_ADD_RX_FRAG(skb, i, page, off, size, truesize) skb_add_rx_frag(skb, i, page, off, size)
static inline struct page *skb_frag_page(const skb_frag_t *frag)
{
	return frag->page;
}
static inline void *skb_frag_address(const skb_frag_t *frag)
{
	return page_address(skb_frag_page(frag)) + frag->page_offset;
}
static inline void skb_frag_size_sub(skb_frag_t *frag, int delta)
{
	frag->size -= delta;
}
#else
#define QEDE_SKB_ADD_RX_FRAG skb_add_rx_frag
#endif

#ifndef ETH_TEST_FL_EXTERNAL_LB /* ! QEDE_UPSTREAM */
#define ETH_TEST_FL_EXTERNAL_LB		(1 << 2)
#endif
#ifndef ETH_TEST_FL_EXTERNAL_LB_DONE /* ! QEDE_UPSTREAM */
#define ETH_TEST_FL_EXTERNAL_LB_DONE	(1 << 3)
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0) || RHEL_STARTING_AT_VERSION(6, 2))
#define DCB_CEE_SUPPORT 1
#endif

#ifndef __HAS_VXLAN_HDR_DEFINED
#define QEDE_VXLAN_HDR_SIZE 8
#else
#define QEDE_VXLAN_HDR_SIZE sizeof(struct vxlanhdr)
#endif

#ifdef ENC_SUPPORTED

#ifndef VXLAN_HEADROOM
/* IP header + UDP + VXLAN + Ethernet header */
#define VXLAN_HEADROOM (20 + 8 + 8 + 14)
#endif

#ifndef VXLAN6_HEADROOM
/* IPv6 header + UDP + VXLAN + Ethernet header */
#define VXLAN6_HEADROOM (40 + 8 + 8 + 14)
#endif

#ifdef CONFIG_INET
void qede_gro_ip_csum(struct sk_buff *skb);
void qede_gro_ipv6_csum(struct sk_buff *skb);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)) || \
	!defined(_HAS_UDP_GRO_COMP_EXPORTED)  /* ! QEDE_UPSTREAM */
static inline int qede_udp_gro_complete(struct sk_buff *skb, int nhoff)
{
	if (skb_shinfo(skb)->gso_type & SKB_GSO_TCPV4)
		qede_gro_ip_csum(skb);
	else
		qede_gro_ipv6_csum(skb);

	skb->encapsulation = 1;
	skb->inner_mac_header = skb->data - skb->head;
	skb->inner_mac_header += (nhoff + QEDE_VXLAN_HDR_SIZE);

	return 0;
}
#define udp_gro_complete(skb, nhoff, func) qede_udp_gro_complete(skb, nhoff)
#endif
#endif

#else /* ENC_SUPPORTED */
static inline u8 qede_tunn_exist(u16 flag) { return 0; }
static inline u8 qede_check_tunn_csum(u16 flag) { return 0; }

#endif

/* RH6.x doesn't have IFF_UNICAST_FLT - it has some opposite logic which
 * we don't require in the netdev_extended struct
 */
#ifndef IFF_UNICAST_FLT
#define IFF_UNICAST_FLT 0
#endif

#ifdef TEDIBEAR
/* It's complicated to fix this inside the tedibear itself; Instead, 'fix' this
 * here.
 */
#define jiffies 0
#endif

#ifndef XDP_PACKET_HEADROOM
#define XDP_PACKET_HEADROOM 256
#endif

#ifndef _HAS_TRACE_XDP_EXCEPTION
#define trace_xdp_exception(ndev, prog, action)
#endif

#ifndef _HAS_NAPI_COMPLETE_DONE
#define napi_complete_done(napi, value)	napi_complete(napi)
#endif

#ifndef IS_ENABLED
#define __ARG_PLACEHOLDER_1 0,
#define __take_second_arg(__ignored, val, ...) val
#define __is_defined(x)			___is_defined(x)
#define ___is_defined(val)		____is_defined(__ARG_PLACEHOLDER_##val)
#define ____is_defined(arg1_or_junk)	__take_second_arg(arg1_or_junk 1, 0)
#define IS_ENABLED(option)		__is_defined(option)
#endif

#ifdef _DEFINE_FLOW_SPEC_RING
#define ETHTOOL_RX_FLOW_SPEC_RING	0x00000000FFFFFFFFLL
#define ETHTOOL_RX_FLOW_SPEC_RING_VF	0x000000FF00000000LL
#define ETHTOOL_RX_FLOW_SPEC_RING_VF_OFF 32
static inline __u64 ethtool_get_flow_spec_ring(__u64 ring_cookie)
{
	return ETHTOOL_RX_FLOW_SPEC_RING & ring_cookie;
};

static inline __u64 ethtool_get_flow_spec_ring_vf(__u64 ring_cookie)
{
	return (ETHTOOL_RX_FLOW_SPEC_RING_VF & ring_cookie) >>
				ETHTOOL_RX_FLOW_SPEC_RING_VF_OFF;
};
#endif

#ifndef _HAS_FLOW_EXT
#define FLOW_EXT	0x80000000
#endif

#if defined(_DEFINE_LIST_NEXT_ENTRY)
/**
 * list_next_entry - get the next element in list
 * @pos:        the type * to cursor
 * @member:     the name of the list_head within the struct.
 */
#define list_next_entry(pos, member) \
	list_entry((pos)->member.next, typeof(*(pos)), member)

#endif

#ifndef HAS_BOND_OFFLOAD_SUPPORT
struct qede_dev;
struct qed_link_output;
static inline void qede_handle_link_change(struct qede_dev *edev,
					   struct qed_link_output *link)
{
	return;
}
#endif

#if LINUX_PRE_VERSION(3, 17, 0)
#define is_kdump_kernel() (reset_devices)
#endif

/********************************* KREF-API ********************************/
#ifdef _DEFINE_KREF_GET_UNLESS_ZERO
static inline int __must_check kref_get_unless_zero(struct kref *kref)
{
	return atomic_add_unless(&kref->refcount, 1, 0);
}
#endif

#ifdef __has_attribute
#ifndef __GCC4_has_attribute___fallthrough__
#define __GCC4_has_attribute___fallthrough__ 0
#endif
#if __has_attribute(__fallthrough__)
#define COMPAT_FALLTHROUGH   __attribute__((__fallthrough__))
#else
#define COMPAT_FALLTHROUGH
#endif
#else
#define COMPAT_FALLTHROUGH
#endif

#ifdef _HAS_DEVLINK
#include <net/devlink.h>
#else
struct devlink;
static inline void *devlink_priv(struct devlink *dl)
{
	return NULL;
}
#endif
#endif /* _QEDE_COMPAT_H_ */
