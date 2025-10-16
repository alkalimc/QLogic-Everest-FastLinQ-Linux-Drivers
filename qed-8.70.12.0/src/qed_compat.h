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

#ifndef _QED_COMPAT_H_
#define _QED_COMPAT_H_
#include <linux/version.h>
#include <linux/skbuff.h>
#include <linux/list.h>
#ifdef _HAS_DEVLINK
#include <net/devlink.h>
#endif

#define HAS_NDO(feat) \
	(defined(_HAS_NDO_ ## feat) || defined (_HAS_NDO_EXT_ ## feat))

#ifndef RHEL_RELEASE_VERSION
#define RHEL_RELEASE_VERSION(a, b) 0
#endif
#define NOT_RHEL_OR_PRE_VERSION(a, b) \
	(!defined(RHEL_RELEASE_CODE) || \
	 RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION((a), (b)))
#define LINUX_PRE_VERSION(a, b, c) \
	(LINUX_VERSION_CODE < KERNEL_VERSION((a), (b), (c)))

#define RHEL_IS_VERSION(a, b) \
	(defined(RHEL_RELEASE_CODE) && \
	 RHEL_RELEASE_CODE ==  RHEL_RELEASE_VERSION((a), (b)))

#define RHEL_STARTING_AT_VERSION(a, b) \
	(defined(RHEL_RELEASE_CODE) && \
	 RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION((a), (b)))

#if ((defined(NETIF_F_GSO_GRE) && !defined(RHEL_RELEASE_CODE)) || \
     (defined(NETIF_F_GSO_GRE) && defined(RHEL_RELEASE_CODE) && \
      (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 0))))
#define QED_ENC_SUPPORTED 1
#endif

#ifndef SLES_DISTRO
#ifdef CONFIG_SUSE_KERNEL
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 18))
/* SLES15 SP2 is at least 5.3.18+ based */
#define SLES_DISTRO 0x1502
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 14))
/* SLES15 is at least 4.12.14+ based */
#define SLES_DISTRO 0x1502
#endif
#endif /* CONFIG_SUSE_KERNEL */
#endif

#define NOT_SLES_OR_PRE_VERSION(a) \
	(!defined(SLES_DISTRO) || (SLES_DISTRO < (a)))
#define SLES_STARTING_AT_VERSION(a) \
	(defined(SLES_DISTRO) && (SLES_DISTRO >= (a)))
#define SLES11_SP3	0x1103
#define SLES15_SP2	0x1502
#define SLES15_SP3	0x1503

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20))
#define atomic_read(&pdev->enable_cnt) 1
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 11))
#define pci_save_state(pdev) pci_save_state(pdev, cdev->pci_params.pci_state)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28))
#define pci_ioremap_bar(pdev, 0)	\
	ioremap_nocache(cdev->pci_params.mem_start, pci_resource_len(pdev, 0))
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 5, 0))
#define ioremap_nocache ioremap
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 34))
#define dma_set_mask(dev, x) 		\
	pci_set_dma_mask((container_of(dev, struct pci_dev, dev)), x)
#define dma_set_coherent_mask(dev, x) 	\
	pci_set_consistent_dma_mask((container_of(dev, struct pci_dev, dev)), x)
#endif

#ifdef CONFIG_DEBUG_FS
/* In RHEL versions before 7.0, regdump has higher chance to fail because
 * ethtool allocates the buffer using kzalloc, as oppose to vzalloc in upstream
 * kernel and newer RHEL versions.
 */
#if defined(RHEL_RELEASE_CODE) && NOT_RHEL_OR_PRE_VERSION(7, 0)
#define REGDUMP_MAX_SIZE			0x300000
#else
#define REGDUMP_MAX_SIZE			0x1000000
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 6, 0))
#include <linux/export.h>
#endif
#if defined(_DEFINE_SIMPLE_OPEN)
#include <linux/fs.h>
static inline int simple_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
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

/* Due to lack of C99 support in MSVC */
#ifndef MSVC
#define INIT_STRUCT_FIELD(field, value) .field = value
#else
#define INIT_STRUCT_FIELD(field, value) value
#endif

#ifdef _DEFINE_SUPPORTED_40000BASEKR4_FULL
#define SUPPORTED_40000baseKR4_Full	(1 << 23)
#define SUPPORTED_40000baseCR4_Full	(1 << 24)
#define SUPPORTED_40000baseSR4_Full	(1 << 25)
#define SUPPORTED_40000baseLR4_Full	(1 << 26)
#endif
#ifdef _DEFINE_SUPPORTED_25000BASEKR_FULL
#define SUPPORTED_25000baseKR_Full    (1<<27)
#define SUPPORTED_50000baseKR2_Full   (1<<28)
#define SUPPORTED_100000baseKR4_Full  (1<<29)
#define SUPPORTED_100000baseCR4_Full  (1<<30)
#endif

#ifndef _DEFINE_IFLA_VF_LINKSTATE
enum {
	IFLA_VF_LINK_STATE_AUTO,	/* link state of the uplink */
	IFLA_VF_LINK_STATE_ENABLE,	/* link always up */
	IFLA_VF_LINK_STATE_DISABLE,	/* link always down */
	__IFLA_VF_LINK_STATE_MAX,
};
#endif

#ifdef _HAS_BUILD_SKB
#define QED_RX_DATA	u8
#define QED_ALLOC_RX_DATA(x, y)	\
	kmalloc(x + NET_SKB_PAD + \
		SKB_DATA_ALIGN(sizeof(struct skb_shared_info)), y)
#define QED_RX_DATA_PTR(_data) (_data + NET_SKB_PAD)
#define QED_FREE_RX_DATA(data) kfree(data)
#define QED_SET_RX_PAD(pad) (pad += NET_SKB_PAD)
#ifdef _HAS_BUILD_SKB_V2
#define QED_BUILD_SKB(data) build_skb(data, 0)
#else
#define QED_BUILD_SKB(data) build_skb(data)
#endif
#else
#define QED_RX_DATA	struct sk_buff
#if RHEL_IS_VERSION(6, 2)
/* RHEL 6.2 requires the ndev to associate the skb to a NUMA node */
#define QED_ALLOC_RX_DATA(d, x, y) netdev_alloc_skb(d, x);
#else
#define QED_ALLOC_RX_DATA(x, y) netdev_alloc_skb(NULL, x);
#endif
#define QED_RX_DATA_PTR(_data) ((_data)->data)
#define QED_FREE_RX_DATA(data) dev_kfree_skb_any(data)
#define QED_SET_RX_PAD(pad)
#define QED_BUILD_SKB(data) data
#endif

#ifndef _DEFINE_HASHTABLE
#include <linux/hashtable.h>
#else

#ifndef TEDIBEAR
#include <linux/hash.h>
#endif

#define hlist_entry(ptr, type, member) container_of(ptr,type,member)

/* 2^31 + 2^29 - 2^25 + 2^22 - 2^19 - 2^16 + 1 */
#define GOLDEN_RATIO_PRIME_32 0x9e370001UL
/*  2^63 + 2^61 - 2^57 + 2^54 - 2^51 - 2^18 + 1 */
#define GOLDEN_RATIO_PRIME_64 0x9e37fffffffc0001UL

#ifdef TEDIBEAR
#define BITS_PER_LONG (32)
#endif

#define DECLARE_HASHTABLE(name, bits)                                   	\
	struct hlist_head name[1 << (bits)]

#define HASH_SIZE(name) (ARRAY_SIZE(name))
#define HASH_BITS(name) ilog2(HASH_SIZE(name))

/* Use hash_32 when possible to allow for fast 32bit hashing in 64bit kernels. */
#define hash_min(val, bits)							\
	(sizeof(val) <= 4 ? hash_32(val, bits) : hash_long(val, bits))

static inline void __hash_init(struct hlist_head *ht, unsigned int sz)
{
	unsigned int i;

	for (i = 0; i < sz; i++)
		INIT_HLIST_HEAD(&ht[i]);
}

/**
 * hash_init - initialize a hash table
 * @hashtable: hashtable to be initialized
 *
 * Calculates the size of the hashtable from the given parameter, otherwise
 * same as hash_init_size.
 *
 * This has to be a macro since HASH_BITS() will not work on pointers since
 * it calculates the size during preprocessing.
 */
#define hash_init(hashtable) __hash_init(hashtable, HASH_SIZE(hashtable))

/**
 * hash_add - add an object to a hashtable
 * @hashtable: hashtable to add to
 * @node: the &struct hlist_node of the object to be added
 * @key: the key of the object to be added
 */
#define hash_add(hashtable, node, key)						\
	hlist_add_head(node, &hashtable[hash_min(key, HASH_BITS(hashtable))])

static inline bool __hash_empty(struct hlist_head *ht, unsigned int sz)
{
	unsigned int i;

	for (i = 0; i < sz; i++)
		if (!hlist_empty(&ht[i]))
			return false;

	return true;
}

/**
 * hash_empty - check whether a hashtable is empty
 * @hashtable: hashtable to check
 *
 * This has to be a macro since HASH_BITS() will not work on pointers since
 * it calculates the size during preprocessing.
 */
#define hash_empty(hashtable) __hash_empty(hashtable, HASH_SIZE(hashtable))

#define hlist_entry_safe(ptr, type, member) \
	({ typeof(ptr) ____ptr = (ptr); \
	   ____ptr ? hlist_entry(____ptr, type, member) : NULL; \
	})

/**
 * hash_for_each_possible - iterate over all possible objects hashing to the
 * same bucket
 * @name: hashtable to iterate
 * @obj: the type * to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 * @key: the key of the objects to iterate over
 */
#define hash_for_each_possible(name, obj, member, key)			\
	for (obj = hlist_entry_safe((&name[hash_min(key, HASH_BITS(name))])->first, \
				    typeof(*(obj)), member); \
	     obj; \
	     obj = hlist_entry_safe((obj)->member.next, typeof(*(obj)), member))
/*	hlist_for_each_entry(obj, &name[hash_min(key, HASH_BITS(name))], member)*/

#endif /* _DEFINE_HASHTABLE */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32))
static inline struct page *skb_frag_page(const skb_frag_t *frag)
{
	return frag->page;
}
#endif

/* Define atomic operations for latest upstream */
#ifndef smp_mb__before_atomic
#define smp_mb__before_atomic		smp_mb__before_clear_bit
#endif

#ifndef smp_mb__after_atomic
#define smp_mb__after_atomic		smp_mb__after_clear_bit
#endif

#ifndef skb_vlan_tag_present
#define skb_vlan_tag_present	vlan_tx_tag_present
#endif

#ifndef skb_vlan_tag_get
#define skb_vlan_tag_get	vlan_tx_tag_get
#endif

#if defined(_DEFINE_PCI_ENABLE_MSIX_RANGE)
#include <linux/pci.h>
static inline int pci_enable_msix_range(struct pci_dev *dev,
					struct msix_entry *entries,
					int minvec, int maxvec)
{
	int rc;

	rc = pci_enable_msix(dev, entries, maxvec);
	if (!rc)
		return maxvec;

	/* Try with less - but still in range */
	if (rc >= minvec) {
		int try = rc;

		rc = pci_enable_msix(dev, entries, try);
		if (!rc)
			return try;
	}

	/* If can't supply in range but can supply something */
	if (rc > 0)
		return -ENOSPC;

	/* Return error */
	return rc;
}
#endif

#if defined(_DEFINE_PCI_ENABLE_MSIX_EXACT)
#include <linux/pci.h>
static inline int pci_enable_msix_exact(struct pci_dev *dev,
					struct msix_entry *entries, int nvec)
{
	int rc = pci_enable_msix_range(dev, entries, nvec, nvec);

	if (rc < 0)
		return rc;
	return 0;
}
#endif

#if defined(_DEFINE_PCIE_CAPS_REG)
#include <linux/pci.h>
static inline u16 pcie_caps_reg(const struct pci_dev *dev)
{
#if defined(_HAS_PCIE_FLAGS_REG)
	return dev->pcie_flags_reg;
#else
	return 0;
#endif
}
#endif

#if defined(_DEFINE_PCI_PCIE_TYPE)
#include <linux/pci.h>
static inline int pci_pcie_type(const struct pci_dev *dev)
{
	return (pcie_caps_reg(dev) & PCI_EXP_FLAGS_TYPE) >> 4;
}
#endif

#if defined(_DEFINE_PCIE_CAPABILITY_READ_WORD) || \
    defined(_DEFINE_PCIE_CAPABILITY_WRITE_WORD)
#include <linux/pci.h>

static inline int pcie_cap_version(const struct pci_dev *dev)
{
	return pcie_caps_reg(dev) & PCI_EXP_FLAGS_VERS;
}

#ifndef PCI_EXP_TYPE_PCIE_BRIDGE
#define PCI_EXP_TYPE_PCIE_BRIDGE	0x8	/* PCI/PCI-X to PCIe Bridge */
#endif

static bool pcie_downstream_port(const struct pci_dev *dev)
{
	int type = pci_pcie_type(dev);

	return type == PCI_EXP_TYPE_ROOT_PORT ||
	       type == PCI_EXP_TYPE_DOWNSTREAM ||
	       type == PCI_EXP_TYPE_PCIE_BRIDGE;
}

static inline bool pcie_cap_has_lnkctl(const struct pci_dev *dev)
{
	int type = pci_pcie_type(dev);

	return type == PCI_EXP_TYPE_ENDPOINT ||
	       type == PCI_EXP_TYPE_LEG_END ||
	       type == PCI_EXP_TYPE_ROOT_PORT ||
	       type == PCI_EXP_TYPE_UPSTREAM ||
	       type == PCI_EXP_TYPE_DOWNSTREAM ||
	       type == PCI_EXP_TYPE_PCI_BRIDGE ||
	       type == PCI_EXP_TYPE_PCIE_BRIDGE;
}

static inline bool pcie_cap_has_sltctl(const struct pci_dev *dev)
{
	return pcie_downstream_port(dev) &&
	       pcie_caps_reg(dev) & PCI_EXP_FLAGS_SLOT;
}

static inline bool pcie_cap_has_rtctl(const struct pci_dev *dev)
{
	int type = pci_pcie_type(dev);

	return type == PCI_EXP_TYPE_ROOT_PORT ||
	       type == PCI_EXP_TYPE_RC_EC;
}

#ifndef PCI_EXP_LNKCAP2
#define PCI_EXP_LNKCAP2	44	/* Link Capabilities 2 */
#endif
#ifndef PCI_EXP_LNKSTA2
#define PCI_EXP_LNKSTA2	50	/* Link Status 2 */
#endif

static inline bool pcie_capability_reg_implemented(struct pci_dev *dev, int pos)
{
	if (!pci_is_pcie(dev))
		return false;

	switch (pos) {
	case PCI_EXP_FLAGS:
		return true;
	case PCI_EXP_DEVCAP:
	case PCI_EXP_DEVCTL:
	case PCI_EXP_DEVSTA:
		return true;
	case PCI_EXP_LNKCAP:
	case PCI_EXP_LNKCTL:
	case PCI_EXP_LNKSTA:
		return pcie_cap_has_lnkctl(dev);
	case PCI_EXP_SLTCAP:
	case PCI_EXP_SLTCTL:
	case PCI_EXP_SLTSTA:
		return pcie_cap_has_sltctl(dev);
	case PCI_EXP_RTCTL:
	case PCI_EXP_RTCAP:
	case PCI_EXP_RTSTA:
		return pcie_cap_has_rtctl(dev);
	case PCI_EXP_DEVCAP2:
	case PCI_EXP_DEVCTL2:
	case PCI_EXP_LNKCAP2:
	case PCI_EXP_LNKCTL2:
	case PCI_EXP_LNKSTA2:
		return pcie_cap_version(dev) > 1;
	default:
		return false;
	}
}

#if defined(_DEFINE_PCIE_CAPABILITY_READ_WORD)
static inline int pcie_capability_read_word(struct pci_dev *dev, int pos,
					    u16 *val)
{
	int ret;

	*val = 0;
	if (pos & 1)
		return -EINVAL;

	if (pcie_capability_reg_implemented(dev, pos)) {
		ret = pci_read_config_word(dev, pci_pcie_cap(dev) + pos, val);
		if (ret)
			*val = 0;
		return ret;
	}

	if (pci_is_pcie(dev) && pcie_downstream_port(dev) &&
	    pos == PCI_EXP_SLTSTA)
		*val = PCI_EXP_SLTSTA_PDS;

	return 0;
}
#endif

#if defined(_DEFINE_PCIE_CAPABILITY_WRITE_WORD)
static inline int pcie_capability_write_word(struct pci_dev *dev, int pos,
					     u16 val)
{
	if (pos & 1)
		return -EINVAL;

	if (!pcie_capability_reg_implemented(dev, pos))
		return 0;

	return pci_write_config_word(dev, pci_pcie_cap(dev) + pos, val);
}
#endif
#endif

#if defined(_DEFINE_PCIE_CAPABILITY_CLEAR_AND_SET_WORD)
#include <linux/pci.h>
static inline int pcie_capability_clear_and_set_word(struct pci_dev *dev,
						     int pos, u16 clear,
						     u16 set)
{
	int ret;
	u16 val;

	ret = pcie_capability_read_word(dev, pos, &val);
	if (!ret) {
		val &= ~clear;
		val |= set;
		ret = pcie_capability_write_word(dev, pos, val);
	}

	return ret;
}
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

#ifndef PCI_EXP_DEVCTL2_COMP_TIMEOUT
#define PCI_EXP_DEVCTL2_COMP_TIMEOUT	0x000f	/* Completion Timeout Value */
#endif
#ifndef PCI_EXP_DEVCTL2_LTR_EN
#define PCI_EXP_DEVCTL2_LTR_EN		0x0400	/* Enable LTR mechanism */
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37)) && !defined(MODERN_VLAN)
#define OLD_VLAN
#else
#define MODERN_VLAN
#endif
#if (LINUX_PRE_VERSION(3, 10, 0))
#define __vlan_hwaccel_put_tag(x, y, z) __vlan_hwaccel_put_tag(x,z)
#endif

#if (LINUX_PRE_VERSION(2, 6, 36) && NOT_RHEL_OR_PRE_VERSION(6, 3))
static inline void usleep_range(unsigned long min, unsigned long max)
{
	if (min < 1000)
		udelay(min);
	else
		msleep(min / 1000);
}
#endif

#if (LINUX_PRE_VERSION(3, 2, 0) && NOT_RHEL_OR_PRE_VERSION(6, 3) && NOT_SLES_OR_PRE_VERSION(SLES11_SP3))
static inline struct page *skb_frag_page(const skb_frag_t *frag)
{
	return frag->page;
}

static inline dma_addr_t skb_frag_dma_map(struct device *dev,
					  const skb_frag_t *frag,
					  size_t offset, size_t size,
					  enum dma_data_direction dir)
{
	return dma_map_page(dev, skb_frag_page(frag),
			    frag->page_offset + offset, size, dir);
}
#endif

#if (LINUX_PRE_VERSION(3, 2, 0))
#define skb_frag_size(frag) ((frag)->size)
#endif

#ifdef _DEFINE_KSTRTOUL
static inline int kstrtoul(const char *s, unsigned int base, unsigned long *res)
{
	*res = simple_strtoul(s, NULL, base);

	return 0;
}
#endif

#if LINUX_PRE_VERSION(3, 17, 0)
/* This cannot be decided by checking header symbols from kernel includes,
 * since the change making this possible is an exported symbol in a .c file.
 */
#define is_kdump_kernel()	(reset_devices)
#endif

#ifdef _DEFINE_CRC8
#define CRC8_INIT_VALUE	0xFF
#define CRC8_TABLE_SIZE	256
#define DECLARE_CRC8_TABLE(_table) \
	static u8 _table[CRC8_TABLE_SIZE]

static inline void crc8_populate_msb(u8 table[CRC8_TABLE_SIZE], u8 polynomial)
{
	int i, j;
	const u8 msbit = 0x80;
	u8 t = msbit;

	table[0] = 0;

	for (i = 1; i < CRC8_TABLE_SIZE; i *= 2) {
		t = (t << 1) ^ (t & msbit ? polynomial : 0);
		for (j = 0; j < i; j++)
			table[i+j] = table[j] ^ t;
	}
}

static inline
u8 crc8(const u8 table[CRC8_TABLE_SIZE], u8 *pdata, size_t nbytes, u8 crc)
{
	/* loop over the buffer data */
	while (nbytes-- > 0)
		crc = table[(crc ^ *pdata++) & 0xff];

	return crc;
}
#endif /* _DEFINE_CRC8 */

#ifndef _HAS_ETH_RANDOM_ADDR
#define ETH_ALEN	6

#ifndef _HAS_ETH_RANDOM_BYTES
#define RANDOM_32	17

static inline void get_random_bytes(void *buf, int nbytes)
{
	int i;

	for (i = 0; i < nbytes / 4; i++)
		((u32 *)buf)[i] = RANDOM_32;
}
#endif

static inline void eth_random_addr(u8 *addr)
{
	get_random_bytes(addr, ETH_ALEN);
	addr[0] &= 0xfe;	/* clear multicast bit */
	addr[0] |= 0x02;	/* set local assignment bit (IEEE802) */
}
#endif /* _HAS_ETH_RANDOM_ADDR */

#ifdef _MISSING_CRC8_MODULE
#ifndef CRC8_INIT_VALUE
#define CRC8_INIT_VALUE	0xFF
#endif
#ifndef CRC8_TABLE_SIZE
#define CRC8_TABLE_SIZE	256
#endif
#ifndef DECLARE_CRC8_TABLE
#define DECLARE_CRC8_TABLE(_table) \
	static u8 _table[CRC8_TABLE_SIZE]
#endif

#define crc8_populate_msb(table, polynomial) \
	qed_crc8_populate_msb(table, polynomial)
#define crc8(table, pdata, nbytes, crc)	qed_crc8(table, pdata, nbytes, crc)
#endif /* _MISSING_CRC8_MODULE */

#ifndef writeq
#define writeq writeq
static inline void writeq(u64 value, volatile void __iomem *addr)
{
	writel((u32)value, addr);
	writel((u32)(value >> 32), addr + 4);
	wmb();
}
#endif

#ifndef readq
#define readq readq
static inline u64 readq(void __iomem *addr)
{
	rmb();

	return readl(addr) | (((u64) readl(addr + 4)) << 32LL);
}
#endif

#ifndef IS_ENABLED
#define __ARG_PLACEHOLDER_1 0,
#define __take_second_arg(__ignored, val, ...) val
#define __is_defined(x)			___is_defined(x)
#define ___is_defined(val)		____is_defined(__ARG_PLACEHOLDER_##val)
#define ____is_defined(arg1_or_junk)	__take_second_arg(arg1_or_junk 1, 0)
#define IS_ENABLED(option)		__is_defined(option)
#endif

#ifdef _DEFINE_ETH_ZERO_ADDR
#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif
static inline void eth_zero_addr(u8 *addr)
{
	memset(addr, 0x00, ETH_ALEN);
}
#endif

#ifndef _HAS_TIME_T
#define time_t time64_t
#endif

#ifndef _HAS_TIME_TO_TM
static inline void time_to_tm(time_t totalsecs, int offset, struct tm *result)
{
	time64_to_tm(totalsecs, offset, result);
}
#endif

#ifndef _HAS_TIMESPEC64
#define timespec64 timespec
#define ktime_get_real_ts64 getnstimeofday
#endif

#if defined(_HAS_SECURE_BOOT)  && defined(_HAS_EFI_ENABLED)
#define _efi_enabled efi_enabled(EFI_SECURE_BOOT)
#else
#define _efi_enabled 0
#endif

#ifdef DEFINE_DATA_ACCESS_EXCEEDS_WORD_SIZE
static __always_inline void data_access_exceeds_word_size(void)
#ifdef __compiletime_warning
__compiletime_warning("data access exceeds word size and won't be atomic")
#endif
;

static __always_inline void data_access_exceeds_word_size(void)
{
}
#endif

#ifdef DEFINE_READ_ONCE
static __always_inline void __read_once_size(volatile void *p, void *res, int size)
{
	switch (size) {
	case 1: *(__u8 *)res = *(volatile __u8 *)p; break;
	case 2: *(__u16 *)res = *(volatile __u16 *)p; break;
	case 4: *(__u32 *)res = *(volatile __u32 *)p; break;
#ifdef CONFIG_64BIT
	case 8: *(__u64 *)res = *(volatile __u64 *)p; break;
#endif
	default:
		barrier();
		memcpy((void *)res, (const void *)p, size);
		data_access_exceeds_word_size();
		barrier();
	}
}

#ifndef _HAS_XRC_SUPPORT
#define IB_QPT_XRC_INI 9
#define IB_QPT_XRC_TGT 10
#define IB_SRQT_XRC 11
#define IB_USER_VERBS_CMD_OPEN_XRCD 60
#define IB_USER_VERBS_CMD_CLOSE_XRCD 61
#define IB_USER_VERBS_CMD_CREATE_XSRQ 61
#endif

#define READ_ONCE(x) \
	({ typeof(x) __val; __read_once_size(&x, &__val, sizeof(__val)); __val; })

#endif

#ifdef TEDIBEAR
#define PATH_MAX	4096
#endif

#ifndef ETH_P_8021AD
#define ETH_P_8021AD	0x88A8
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

#if (LINUX_PRE_VERSION(5, 9, 0) && NOT_RHEL_OR_PRE_VERSION(8, 4)) && NOT_SLES_OR_PRE_VERSION(SLES15_SP3)
#define devlink_info_board_serial_number_put  devlink_info_serial_number_put
#endif
#if (LINUX_PRE_VERSION(5, 7, 0) && NOT_RHEL_OR_PRE_VERSION(8, 3)) && NOT_SLES_OR_PRE_VERSION(SLES15_SP3)
#ifdef _HAS_DEVLINK_AUTO_RECOVER
#define devlink_health_reporter_create(A, B, C, D)  (devlink_health_reporter_create)((A), (B), (C), true, (D))
#endif
#endif
#ifndef DEVLINK_INFO_VERSION_GENERIC_FW_ROCE
#define DEVLINK_INFO_VERSION_GENERIC_FW_ROCE	"fw.roce"
#endif

static inline int __qed_devlink_register(void *dl,
					 struct device *dev)
{
#ifdef _HAS_DEVLINK
	struct devlink *devlink = (struct devlink *)dl;
	int rc = 0;

#ifdef _HAS_DEVLINK_STRUCT_DEV_DEVLINK_REGISTER
#ifdef _HAS_DEVLINK_INT_DEVLINK_REGISTER
	rc =  devlink_register(devlink, dev);
#else
	devlink_register(devlink, dev);
#endif
#else
#ifdef _HAS_DEVLINK_INT_DEVLINK_REGISTER
	rc = devlink_register(devlink);
#else
	devlink_register(devlink);
#endif
#endif
	return rc;
#else
	return 0;
#endif
}
#endif /* _QED_COMPAT_H_ */
