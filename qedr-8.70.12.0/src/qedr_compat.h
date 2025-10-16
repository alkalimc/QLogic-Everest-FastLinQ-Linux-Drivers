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

#ifndef _QEDR_COMPAT_H_
#define _QEDR_COMPAT_H_

#define QEDR_BACKPORT(__sym) backport_ ##__sym

#ifndef ROCE_V2_UDP_DPORT
#define ROCE_V2_UDP_DPORT	(4791)
#endif

/* use wmb when dma_wmb is undefined */
#ifndef dma_wmb
#define dma_wmb()	wmb()
#endif

#ifndef USHRT_MAX
#define USHRT_MAX       ((u16)(~0U))
#endif

#ifdef _HAS_U32_PORT_NUM
#define COMPAT_PORT(_param) u32 _param
#else
#define COMPAT_PORT(_param) u8 _param
#endif

#ifndef _HAS_IB_PORT_PHYS_STATE
enum ib_port_phys_state {
	IB_PORT_PHYS_STATE_SLEEP = 1,
	IB_PORT_PHYS_STATE_POLLING = 2,
	IB_PORT_PHYS_STATE_DISABLED = 3,
	IB_PORT_PHYS_STATE_PORT_CONFIGURATION_TRAINING = 4,
	IB_PORT_PHYS_STATE_LINK_UP = 5,
	IB_PORT_PHYS_STATE_LINK_ERROR_RECOVERY = 6,
	IB_PORT_PHYS_STATE_PHY_TEST = 7,
};
#endif

#if !DEFINED_RDMA_AH_ATTR
#define rdma_ah_attr			ib_ah_attr

static inline void rdma_ah_set_path_bits(struct ib_ah_attr *attr,
					 u8 src_path_bits)
{
	attr->src_path_bits = src_path_bits;
}
#if DEFINE_IB_AH_ATTR_WITH_DMAC
static inline u8 *rdma_ah_retrieve_dmac(struct ib_ah_attr *attr)
{
	return attr->dmac;
}
#endif

static inline const struct ib_global_route
		*rdma_ah_read_grh(const struct ib_ah_attr *attr)
{
	return &attr->grh;
}

static inline u8 rdma_ah_get_port_num(const struct ib_ah_attr *attr)
{
	return attr->port_num;
}

static inline void rdma_ah_set_port_num(struct rdma_ah_attr *attr, COMPAT_PORT(port_num))
{
	attr->port_num = port_num;
}

static inline void rdma_ah_set_sl(struct rdma_ah_attr *attr, u8 sl)
{
	attr->sl = sl;
}

static inline void rdma_ah_set_static_rate(struct rdma_ah_attr *attr,
					   u8 static_rate)
{
	attr->static_rate = static_rate;
}

#endif

#ifndef DEFINED_COPY_AH_ATTR
static inline void rdma_copy_ah_attr(struct rdma_ah_attr *dest,
		       const struct rdma_ah_attr *src)
{
	*dest = *src;
}

static inline void rdma_destroy_ah_attr(struct rdma_ah_attr *ah_attr)
{
}
#endif

#ifdef NOT_DEFINED_IOMMU_PRESENT
static inline bool iommu_present(struct bus_type *bus)
{
	return iommu_found();
}
#endif

#ifndef array_size
#define array_size(x)  (sizeof(x) / sizeof((x)[0]))
#endif

#ifdef _HAS_SIMPLE_WRITE_TO_BUFFER
static inline ssize_t simple_write_to_buffer(void *to, size_t available,
					     loff_t *ppos,
					     const void __user *from,
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

#ifdef DEFINE_REINIT_COMPLETION
static inline void reinit_completion(struct completion *x)
{
	x->done = 0;
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

#ifndef PCI_EXP_DEVCAP2_ATOMIC_COMP64
#define PCI_EXP_DEVCAP2_ATOMIC_COMP64	(1 << 8)
#endif
#ifndef PCI_EXP_DEVCAP2_ATOMIC_ROUTE
#define PCI_EXP_DEVCAP2_ATOMIC_ROUTE		(1 << 6)
#endif
#ifndef PCI_EXP_DEVCTL2_ATOMIC_REQ
#define PCI_EXP_DEVCTL2_ATOMIC_REQ		(1 << 6)
#endif
#ifndef PCI_EXP_DEVCTL2_ATOMIC_EGRESS_BLOCK
#define PCI_EXP_DEVCTL2_ATOMIC_EGRESS_BLOCK	(1 << 7)
#endif

#ifdef NOT_DEFINED_IB_RDMA_WR
#define rdma_wr(_wr) (&(_wr->wr.rdma))
#endif

#ifdef NOT_DEFINED_IB_UD_WR
#define ud_wr(_wr) (&(_wr->wr.ud))
#endif

#ifdef NOT_DEFINED_IB_ATOMIC_WR
#define atomic_wr(_wr) (&(_wr->wr.atomic))
#endif

#if !DEFINE_PCIE_CAPABILITY_OPS
static inline int pcie_capability_read_word(struct pci_dev *dev, int pos,
					    u16 *val)
{
	int ret;

	*val = 0;
	if (pos & 1)
		return -EINVAL;

	ret = pci_read_config_word(dev, pci_pcie_cap(dev) + pos, val);

	/* Reset *val to 0 if pci_read_config_word() fails, it may
	 * have been written as 0xFFFF if hardware error happens
	 * during pci_read_config_word().
	 */

	if (ret)
		*val = 0;
	return ret;
}

static inline int pcie_capability_read_dword(struct pci_dev *dev, int pos,
					     u32 *val)
{
	int ret;

	ret = pci_read_config_dword(dev, pci_pcie_cap(dev) + pos, val);
	if (ret)
		*val = 0;
	return ret;
}

static inline int pcie_capability_clear_word(struct pci_dev *dev, int pos,
					     u16 clear)
{
	int ret;
	u16 val;

	if (pos & 1)
		return -EINVAL;

	ret = pci_read_config_word(dev, pci_pcie_cap(dev) + pos, &val);
	if (!ret) {
		val &= ~clear;
		ret = pci_write_config_word(dev, pci_pcie_cap(dev) + pos, val);
	}

	return ret;
}

static inline int pcie_capability_set_word(struct pci_dev *dev,
					   int pos, u16 set)
{
	int ret;
	u16 val;

	if (pos & 1)
		return -EINVAL;

	ret = pci_read_config_word(dev, pci_pcie_cap(dev) + pos, &val);
	if (!ret) {
		val |= set;
		ret = pci_write_config_word(dev, pci_pcie_cap(dev) + pos, val);
	}

	return ret;
}
#endif

#ifndef writeq
#define writeq writeq
static inline void writeq(u64 value, volatile void __iomem *addr)
{
	writel((u32)value, addr);
	writel((u32)(value >> 32), addr + 4);
}
#endif

#ifndef readq
#define readq readq
static inline u64 readq(void __iomem *addr)
{
	return readl(addr) | (((u64) readl(addr + 4)) << 32LL);
}
#endif

#ifndef IS_ENABLED
#define __ARG_PLACEHOLDER_1 0,
#define config_enabled(cfg) _config_enabled(cfg)
#define _config_enabled(value) __config_enabled(__ARG_PLACEHOLDER_##value)
#define __config_enabled(arg1_or_junk) ___config_enabled(arg1_or_junk 1, 0)
#define ___config_enabled(__ignored, val, ...) val
#define genl_dump_check_consistent(cb, user_hdr, family)
#define IS_ENABLED(option) \
	(config_enabled(option) || config_enabled(option##_MODULE))
#endif

#ifndef ETH_P_IBOE
#define ETH_P_IBOE		(0x8915)
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

#ifndef IS_ENABLED
#define __ARG_PLACEHOLDER_1 0,
#define __take_second_arg(__ignored, val, ...) val
#define __is_defined(x)			___is_defined(x)
#define ___is_defined(val)		____is_defined(__ARG_PLACEHOLDER_##val)
#define ____is_defined(arg1_or_junk)	__take_second_arg(arg1_or_junk 1, 0)
#define IS_ENABLED(option)		__is_defined(option)
#endif

#ifndef DEFINE_RDMA_PROTOCOL
#define rdma_protocol_iwarp(ibdev, portnum) IS_IWARP(get_qedr_dev(ibdev))
#define rdma_protocol_roce(ibdev, portnum) IS_ROCE(get_qedr_dev(ibdev))
#endif

#ifndef DEFINE_DMA_ZALLOC
#define dma_zalloc_coherent QEDR_BACKPORT(dma_zalloc_coherent)
static inline void *dma_zalloc_coherent(struct device *dev, size_t size,
                                        dma_addr_t *dma_handle, gfp_t flag)
{
        void *ret = dma_alloc_coherent(dev, size, dma_handle, flag);
        if (ret)
                memset(ret, 0, size);
        return ret;
}
#endif

#ifdef DEFINE_IB_PORT_SPEED_50G
#define QEDR_SPEED_50G	IB_SPEED_HDR
#define QEDR_WIDTH_50G	IB_WIDTH_1X
#else
#if DEFINE_IB_PORT_SPEED
#define QEDR_SPEED_50G	IB_SPEED_QDR
#else
#define QEDR_SPEED_50G	(4)
#endif
#define QEDR_WIDTH_50G	IB_WIDTH_4X
#endif

#ifndef _HAS_RDMA_NETWORK_ROCE_V1
#define RDMA_NETWORK_ROCE_V1 RDMA_NETWORK_IB
#endif

#ifdef _HAS_IB_REGISTER_DEVICE_NAME /* QEDR_UPSTREAM */

#ifdef _HAS_IB_REGISTER_DEVICE_PORT_CALLBACK
#define COMPAT_IB_DEVICE_NAME(name) name,
#else
#define COMPAT_IB_DEVICE_NAME(name) name
#endif

#else
#define COMPAT_IB_DEVICE_NAME(name)
#endif

#ifdef _HAS_IB_REGISTER_DEVICE_PORT_CALLBACK
#define COMPAT_IB_PORT_CALLBACK(ptr) ptr
#else
#define COMPAT_IB_PORT_CALLBACK(ptr)
#endif

#ifdef _HAS_IB_DRIVER_ID
#ifndef DEVICE_OPS_DRIVER_ID
#define COMPAT_SET_DRIVER_ID(_assignment) _assignment
#else
#define COMPAT_SET_DRIVER_ID(_assignment)
#endif
#else
#define COMPAT_SET_DRIVER_ID(_assignment)
#endif

#ifndef _HAS_IB_REGISTER_DEVICE_NAME /* QEDR_UPSTREAM */
#define COMPAT_COPY_DRIVER_NAME(_copy) _copy
#else
#define COMPAT_COPY_DRIVER_NAME(_copy)
#endif

#ifdef _HAS_AH_ALLOCATION
#define COMPAT_CREATE_AH_DECLARE_RET int
#define COMPAT_HAS_AH_RET(_statement) _statement
#define COMPAT_NO_AH_RET(_statement)
#define COMPAT_CREATE_AH_IBPD(_param)
#define COMPAT_CREATE_AH_AH(_param) _param,
#else
#define COMPAT_CREATE_AH_DECLARE_RET struct ib_ah *
#define COMPAT_HAS_AH_RET(_statement)
#define COMPAT_NO_AH_RET(_statement) _statement
#define COMPAT_CREATE_AH_IBPD(_param) _param,
#define COMPAT_CREATE_AH_AH(_param)
#endif

#ifdef _HAS_AH_INIT_ATTR
#define COMPAT_CREATE_AH_ATTR(_param) struct rdma_ah_init_attr *_param
#define COMPAT_AH_ATTR(_param) _param->ah_attr
#else
#define COMPAT_CREATE_AH_ATTR(_param) struct rdma_ah_attr *_param
#define COMPAT_AH_ATTR(_param) _param
#endif

#ifdef _HAS_CREATE_AH_FLAGS /* QEDR_UPSTREAM */
#ifdef _HAS_AH_INIT_ATTR
#define COMPAT_CREATE_AH_FLAGS(_param)
#else
#define COMPAT_CREATE_AH_FLAGS(_param)  , _param
#endif
#define COMPAT_DESTROY_AH_FLAGS(_param) , _param
#else
#define COMPAT_CREATE_AH_FLAGS(_param)
#define COMPAT_DESTROY_AH_FLAGS(_param)
#endif

#ifdef _DESTROY_AH_HAS_VOID_RETURN
#define COMPAT_DESTROY_AH_DECLARE_RET void
#define COMPAT_DESTROY_AH_RET(_param)
#else
#define COMPAT_DESTROY_AH_DECLARE_RET int
#define COMPAT_DESTROY_AH_RET(_param) _param
#endif

#ifdef _HAS_CREATE_AH_UDATA
#define COMPAT_CREATE_AH_UDATA(_param) , _param
#else
#define COMPAT_CREATE_AH_UDATA(_param)
#endif

#ifdef _HAS_UCONTEXT_ALLOCATION
#define COMPAT_ALLOC_UCTX_DECLARE_RET int
#define COMPAT_ALLOC_UCTX_IBDEV(_param)
#define COMPAT_ALLOC_UCTX_UCTX(_param) _param,
#define COMPAT_ALLOC_UCTX(_statement) _statement;
#define COMPAT_NO_ALLOC_UCTX(_statement)
#define COMPAT_ALLOC_UCTX_RET(_ret) _ret
#define COMPAT_DEALLOC_UCTX_RET void
#else
#define COMPAT_ALLOC_UCTX_DECLARE_RET struct ib_ucontext *
#define COMPAT_ALLOC_UCTX_IBDEV(_param) _param,
#define COMPAT_ALLOC_UCTX_UCTX(_param)
#define COMPAT_ALLOC_UCTX(_statement)
#define COMPAT_NO_ALLOC_UCTX(_statement) _statement
#define COMPAT_ALLOC_UCTX_RET(_ret) ERR_PTR(_ret)
#define COMPAT_DEALLOC_UCTX_RET int
#endif

#ifdef _HAS_PD_ALLOCATION
#define COMPAT_ALLOC_PD_DECLARE_RET int
#define COMPAT_ALLOC_PD_IBDEV(_param)
#define COMPAT_ALLOC_PD_PD(_param) _param,
#define COMPAT_ALLOC_PD(_statement) _statement;
#define COMPAT_NO_ALLOC_PD(_statement)
#define COMPAT_ALLOC_PD_RET(_ret) _ret
#else
#define COMPAT_ALLOC_PD_DECLARE_RET struct ib_pd *
#define COMPAT_ALLOC_PD_IBDEV(_param) _param,
#define COMPAT_ALLOC_PD_PD(_param)
#define COMPAT_ALLOC_PD(_statement)
#define COMPAT_NO_ALLOC_PD(_statement) _statement
#define COMPAT_ALLOC_PD_RET(_ret) ERR_PTR(_ret)
#endif

#ifdef _DEALLOC_PD_HAS_VOID_RETURN
#define COMPAT_DEALLOC_PD_DECLARE_RET void
#define COMPAT_DEALLOC_PD_RET(_param)
#else
#define COMPAT_DEALLOC_PD_DECLARE_RET int
#define COMPAT_DEALLOC_PD_RET(_param) _param
#endif

#ifdef _DEALLOC_PD_HAS_UDATA
#define COMPAT_DEALLOC_PD_UDATA(_param) , _param
#else
#define COMPAT_DEALLOC_PD_UDATA(_param)
#endif

#ifdef _HAS_IB_CONTEXT
#define COMPAT_ALLOC_PD_CXT(_param) _param,
#define GET_DRV_CXT(_param) get_qedr_ucontext(_param)
#define COMPAT_CREATE_CQ_CTX(_param) _param,
#define COMPAT_ALLOC_XRCD_CXT(_param) _param,
#else
#define COMPAT_ALLOC_PD_CXT(_param)
#define GET_DRV_CXT(_param) rdma_udata_to_drv_context(udata,\
					struct qedr_ucontext, ibucontext);
#define COMPAT_CREATE_CQ_CTX(_param)
#define COMPAT_ALLOC_XRCD_CXT(_param)
#endif

#ifdef _HAS_ALLOC_XRCD_IB_XRCD
#define COMPAT_ALLOC_XRCD_FIRST_PARAM struct ib_xrcd *ibxrcd
#define COMPAT_QEDR_ALLOC_XRCD_RET(_param) return (_param)
#define COMPAT_QEDR_ALLOC_XRCD_RET_PARAM int
#define COMPAT_QEDR_KFREE(_param)
#else
#define COMPAT_ALLOC_XRCD_FIRST_PARAM struct ib_device *ibdev
#define COMPAT_QEDR_ALLOC_XRCD_RET(_param) return ERR_PTR(_param)
#define COMPAT_QEDR_ALLOC_XRCD_RET_PARAM struct ib_xrcd *
#define COMPAT_QEDR_KFREE(_param) kfree(_param)
#endif

#ifdef _HAS_CQ_ALLOCATION
#define COMPAT_CREATE_CQ_DECLARE_RET int
#define COMPAT_CREATE_CQ_ERR(_ret) _ret
#define COMPAT_HAS_CQ_RET(_statement) _statement
#define COMPAT_NO_CQ_RET(_statement)
#define COMPAT_CREATE_CQ_IBDEV(_param)
#define COMPAT_CREATE_CQ_CQ(_param) _param,
#else
#define COMPAT_CREATE_CQ_DECLARE_RET struct ib_cq *
#define COMPAT_CREATE_CQ_ERR(_ret) ERR_PTR(_ret)
#define COMPAT_HAS_CQ_RET(_statement)
#define COMPAT_NO_CQ_RET(_statement) _statement
#define COMPAT_CREATE_CQ_IBDEV(_param) _param,
#define COMPAT_CREATE_CQ_CQ(_param)
#endif

#ifdef _DESTROY_CQ_HAS_VOID_RETURN
#define COMPAT_DESTROY_CQ_DECLARE_RET void
#define COMPAT_DESTROY_CQ_RET(_statement) return
#else
#define COMPAT_DESTROY_CQ_DECLARE_RET int
#define COMPAT_DESTROY_CQ_RET(_statement) _statement
#endif

#ifdef _DESTROY_CQ_HAS_UDATA
#define COMPAT_DESTROY_CQ_UDATA(_param) , _param
#else
#define COMPAT_DESTROY_CQ_UDATA(_param)
#endif

#ifdef _HAS_SRQ_ALLOCATION
#define COMPAT_CREATE_SRQ_DECLARE_RET int
#define COMPAT_CREATE_SRQ_ERR(_ret) _ret
#define COMPAT_HAS_SRQ_RET(_statement) _statement
#define COMPAT_NO_SRQ_RET(_statement)
#define COMPAT_CREATE_SRQ_IBPD(_param)
#define COMPAT_CREATE_SRQ_SRQ(_param) _param,
#else
#define COMPAT_CREATE_SRQ_DECLARE_RET struct ib_srq *
#define COMPAT_CREATE_SRQ_ERR(_ret) ERR_PTR(_ret)
#define COMPAT_HAS_SRQ_RET(_statement)
#define COMPAT_NO_SRQ_RET(_statement) _statement
#define COMPAT_CREATE_SRQ_IBPD(_param) _param,
#define COMPAT_CREATE_SRQ_SRQ(_param)
#endif

#ifdef _HAS_DESTROY_SRQ_VOID_RETURN
#define COMPAT_DESTROY_SRQ_DECLARE_RET void
#define COMPAT_DESTROY_SRQ_RET(_param)
#else
#define COMPAT_DESTROY_SRQ_DECLARE_RET int
#define COMPAT_DESTROY_SRQ_RET(_param) _param
#endif

#ifdef _DESTROY_SRQ_HAS_UDATA
#define COMPAT_DESTROY_SRQ_UDATA(_param) , _param
#else
#define COMPAT_DESTROY_SRQ_UDATA(_param)
#endif

#ifdef _DESTROY_QP_HAS_UDATA
#define COMPAT_DESTROY_QP_UDATA(_param) , _param
#else
#define COMPAT_DESTROY_QP_UDATA(_param)
#endif

#ifdef _DEALLOC_XRCD_HAS_UDATA
#define COMPAT_DEALLOC_XRCD_UDATA(_param) , _param
#else
#define COMPAT_DEALLOC_XRCD_UDATA(_param)
#endif

#ifdef _DEALLOC_XRCD_HAS_VOID_RETURN
#define COMPAT_QEDR_DEALLOC_XRCD_DECLARE_RET void
#define COMPAT_QEDR_DEALLOC_XRCD_RET(_param) return
#else
#define COMPAT_QEDR_DEALLOC_XRCD_DECLARE_RET int
#define COMPAT_QEDR_DEALLOC_XRCD_RET(_param) return (_param)
#endif

#ifdef _DEREG_MR_HAS_UDATA
#define COMPAT_DEREG_MR_UDATA(_param) , _param
#else
#define COMPAT_DEREG_MR_UDATA(_param)
#endif

#ifdef _ALLOC_MR_HAS_UDATA
#define COMPAT_ALLOC_MR_UDATA(_param) , _param
#else
#define COMPAT_ALLOC_MR_UDATA(_param)
#endif

#ifdef _HAS_IB_UMEM_GET_UDATA
#ifdef _HAS_IB_UMEM_GET_DMASYNC
#define compat_ib_umem_get(ib_ctx, udata, ibdev, db_rec_addr, page_size, \
			   access, dmasync) \
	ib_umem_get(udata, db_rec_addr, page_size, access, dmasync)
#else
#define compat_ib_umem_get(ib_ctx, udata, ibdev, db_rec_addr, page_size, \
			   access, dmasync) \
	ib_umem_get(udata, db_rec_addr, page_size, access)
#endif
#else
#ifdef _HAS_IB_UMEM_GET_IBDEV
#define compat_ib_umem_get(ib_ctx, udata, ibdev, db_rec_addr, page_size, \
			   access, dmasync) \
	ib_umem_get(ibdev, db_rec_addr, page_size, access)
#else
#define compat_ib_umem_get(ib_ctx, udata, ibdev, db_rec_addr, page_size, \
			   access, dmasync) \
	ib_umem_get(ib_ctx, db_rec_addr, page_size, access, dmasync)
#endif
#endif

#ifdef _HAS_UMEM_DMA_BLOCK /* QEDR_UPSTREAM */
#define COMPAT_IB_UMEM_COUNT(_param) \
	ib_umem_num_dma_blocks(_param, 1 << FW_PAGE_SHIFT)
#else
#define COMPAT_IB_UMEM_COUNT(_param) \
	ib_umem_page_count(_param) << (PAGE_SHIFT - FW_PAGE_SHIFT)
#ifndef DEFINE_IB_UMEM_NO_PAGE_PARAM
#if DEFINE_IB_UMEM_PAGE_SHIFT
#undef COMPAT_IB_UMEM_COUNT
#define COMPAT_IB_UMEM_COUNT(_param) \
	ib_umem_page_count(_param) << (_param->page_shift - FW_PAGE_SHIFT)
#else
#undef COMPAT_IB_UMEM_COUNT
#define COMPAT_IB_UMEM_COUNT(_param) \
	ib_umem_page_count(_param) * (_param->page_size / FW_PAGE_SIZE)
#endif
#endif
#endif

#ifdef _HAS_UMEM_DMA_BLOCK
#define COMPAT_INIT_MR_INFO(_param) \
	init_mr_info(dev, &_param->info, \
		     ib_umem_num_dma_blocks(_param->umem, PAGE_SIZE), 1)
#else
#define COMPAT_INIT_MR_INFO(_param) \
	init_mr_info(dev, &_param->info, \
		     ib_umem_page_count(_param->umem), 1)
#endif

#ifdef _HAS_ALLOC_MW_V3
#define COMPAT_ALLOC_MW_RET(rc) return (rc)
#else
#define COMPAT_ALLOC_MW_RET(rc) return ERR_PTR(rc)
#endif

#ifdef _HAS_U16_ACTIVE_SPEED
#define COMPAT_IB_SPEED_TYPE u16 *ib_speed
#else
#define COMPAT_IB_SPEED_TYPE u8 *ib_speed
#endif

#ifndef _HAS_RDMA_NETWORK_ROCE_V1
#define RDMA_NETWORK_ROCE_V1 RDMA_NETWORK_IB
#endif

#if DEFINE_IB_DEV_OPS
struct ib_device_ops {
	int (*post_send)(struct ib_qp *qp, IB_CONST struct ib_send_wr *send_wr,
			 IB_CONST struct ib_send_wr **bad_send_wr);
	int (*post_recv)(struct ib_qp *qp, IB_CONST struct ib_recv_wr *recv_wr,
			 IB_CONST struct ib_recv_wr **bad_recv_wr);
	void (*drain_rq)(struct ib_qp *qp);
	void (*drain_sq)(struct ib_qp *qp);
	int (*poll_cq)(struct ib_cq *cq, int num_entries, struct ib_wc *wc);
	int (*peek_cq)(struct ib_cq *cq, int wc_cnt);
	int (*req_notify_cq)(struct ib_cq *cq, enum ib_cq_notify_flags flags);
	int (*req_ncomp_notif)(struct ib_cq *cq, int wc_cnt);
	int (*post_srq_recv)(struct ib_srq *srq,
			     IB_CONST struct ib_recv_wr *recv_wr,
			     IB_CONST struct ib_recv_wr **bad_recv_wr);
#ifdef DEFINE_PROCESS_MAD_VARIABLE_SIZE /* QEDR_UPSTREAM */
	int (*process_mad)(struct ib_device *ibdev, int process_mad_flags,
			   COMPAT_PORT(port_num), const struct ib_wc *in_wc,
			   const struct ib_grh *in_grh,
			   const struct ib_mad_hdr *in_mad,
			   size_t in_mad_size, struct ib_mad_hdr *out_mad,
			   size_t *out_mad_size, u16 *out_mad_pkey_index);
#elif defined(DEFINE_PROCESS_MAD_CONST_INPUTS)
	int (*process_mad)(struct ib_device *ibdev,
			   int process_mad_flags,
			   COMPAT_PORT(port_num),
			   const struct ib_wc *in_wc,
			   const struct ib_grh *in_grh,
			   const struct ib_mad *in_mad,
			   struct ib_mad *out_mad);
#else
	int (*process_mad)(struct ib_device *ibdev,
			   int process_mad_flags,
			   COMPAT_PORT(port_num),
			   struct ib_wc *in_wc,
			   struct ib_grh *in_grh,
			   struct ib_mad *in_mad,
			   struct ib_mad *out_mad);
#endif

#ifdef DEFINE_QUERY_DEVICE_PASS_VENDOR_SPECIFIC_DATA /* QEDR_UPSTREAM */
	int (*query_device)(struct ib_device *ibdev,
			      struct ib_device_attr *attr, struct ib_udata *udata);
#else
	int (*query_device)(struct ib_device *, struct ib_device_attr *props);

#endif

	int (*modify_device)(struct ib_device *device, int device_modify_mask,
			     struct ib_device_modify *device_modify);
#if DEFINE_GET_DEV_FW_STR
#if DEFINE_GET_DEV_FW_STR_FIX_LEN
	void (*get_dev_fw_str)(struct ib_device *ibdev, char *str);
#else
	void (*get_dev_fw_str)(struct ib_device *ibdev, char *str,
			       size_t str_len);
#endif
#endif
	const struct cpumask *(*get_vector_affinity)(struct ib_device *ibdev,
						     int comp_vector);
	int (*query_port)(struct ib_device *device, COMPAT_PORT(port_num),
			  struct ib_port_attr *port_attr);
	int (*modify_port)(struct ib_device *device, COMPAT_PORT(port_num),
			   int port_modify_mask,
			   struct ib_port_modify *port_modify);
#if DEFINE_PORT_IMMUTABLE /* QEDR_UPSTREAM */
	/**
	 * The following mandatory functions are used only at device
	 * registration.  Keep functions such as these at the end of this
	 * structure to avoid cache line misses when accessing struct ib_device
	 * in fast paths.
	 */
	int (*get_port_immutable)(struct ib_device *device, COMPAT_PORT(port_num),
				  struct ib_port_immutable *immutable);
#endif
	enum rdma_link_layer (*get_link_layer)(struct ib_device *device,
					       COMPAT_PORT(port_num));
	/**
	 * When calling get_netdev, the HW vendor's driver should return the
	 * net device of device @device at port @port_num or NULL if such
	 * a net device doesn't exist. The vendor driver should call dev_hold
	 * on this net device. The HW vendor's device driver must guarantee
	 * that this function returns NULL before the net device has finished
	 * NETDEV_UNREGISTER state.
	 */
#ifdef DEFINE_GET_NETDEV /* QEDR_UPSTREAM */
	struct net_device *(*get_netdev)(struct ib_device *device, COMPAT_PORT(port_num));
#endif

	/**
	 * query_gid should be return GID value for @device, when @port_num
	 * link layer is either IB or iWarp. It is no-op if @port_num port
	 * is RoCE link layer.
	 */
	int (*query_gid)(struct ib_device *device, COMPAT_PORT(port_num), int index,
			 union ib_gid *gid);

	/**
	 * When calling add_gid, the HW vendor's driver should add the gid
	 * of device of port at gid index available at @attr. Meta-info of
	 * that gid (for example, the network device related to this gid) is
	 * available at @attr. @context allows the HW vendor driver to store
	 * extra information together with a GID entry. The HW vendor driver may
	 * allocate memory to contain this information and store it in @context
	 * when a new GID entry is written to. Params are consistent until the
	 * next call of add_gid or delete_gid. The function should return 0 on
	 * success or error otherwise. The function could be called
	 * concurrently for different ports. This function is only called when
	 * roce_gid_table is used.
	 */
#ifndef REMOVE_DEVICE_ADD_DEL_GID
#if DEFINE_ROCE_GID_TABLE
	int (*add_gid)(struct ib_device *device, COMPAT_PORT(port_num),
		 unsigned int index, const union ib_gid *gid,
		 const struct ib_gid_attr *attr, void **context);

	/**
	 * When calling del_gid, the HW vendor's driver should delete the
	 * gid of device @device at gid index gid_index of port port_num
	 * available in @attr.
	 * Upon the deletion of a GID entry, the HW vendor must free any
	 * allocated memory. The caller will clear @context afterwards.
	 * This function is only called when roce_gid_table is used.
	 */
	int (*del_gid)(struct ib_device *device, COMPAT_PORT(port_num),
		 unsigned int index, void **context);
#endif
#endif

	int (*query_pkey)(struct ib_device *device, COMPAT_PORT(port_num), u16 index,
			  u16 *pkey);
	struct ib_ucontext *(*alloc_ucontext)(struct ib_device *device,
					      struct ib_udata *udata);
	int (*dealloc_ucontext)(struct ib_ucontext *context);
	int (*mmap)(struct ib_ucontext *context, struct vm_area_struct *vma);
	void (*disassociate_ucontext)(struct ib_ucontext *ibcontext);

	struct ib_pd *(*alloc_pd)(struct ib_device *device,
				  struct ib_ucontext *context,
				  struct ib_udata *udata);
	int (*dealloc_pd)(struct ib_pd *pd);
#ifdef _HAS_CREATE_AH_UDATA
	struct ib_ah *(*create_ah)(struct ib_pd *ibpd, struct rdma_ah_attr *attr,
			     struct ib_udata *udata);
#else
	struct ib_ah *(*create_ah)(struct ib_pd *ibpd, struct rdma_ah_attr *attr);
#endif

	int (*modify_ah)(struct ib_ah *ah, struct rdma_ah_attr *ah_attr);
	int (*query_ah)(struct ib_ah *ah, struct rdma_ah_attr *ah_attr);
	int (*destroy_ah)(struct ib_ah *ah);
	struct ib_srq *(*create_srq)(struct ib_pd *pd,
				     struct ib_srq_init_attr *srq_init_attr,
				     struct ib_udata *udata);
	int (*modify_srq)(struct ib_srq *srq, struct ib_srq_attr *srq_attr,
			  enum ib_srq_attr_mask srq_attr_mask,
			  struct ib_udata *udata);
	int (*query_srq)(struct ib_srq *srq, struct ib_srq_attr *srq_attr);
	int (*destroy_srq)(struct ib_srq *srq);
#ifdef _HAS_QP_ALLOCATION
	int (*create_qp)(struct ib_qp *qp, struct ib_qp_init_attr *qp_init_attr,
			 struct ib_udata *udata);
#else
	struct ib_qp *(*create_qp)(struct ib_pd *pd,
				   struct ib_qp_init_attr *qp_init_attr,
				   struct ib_udata *udata);
#endif
	int (*modify_qp)(struct ib_qp *qp, struct ib_qp_attr *qp_attr,
			 int qp_attr_mask, struct ib_udata *udata);
	int (*query_qp)(struct ib_qp *qp, struct ib_qp_attr *qp_attr,
			int qp_attr_mask, struct ib_qp_init_attr *qp_init_attr);
	int (*destroy_qp)(struct ib_qp *qp);
#ifdef DEFINE_CREATE_CQ_ATTR  /* QEDR_UPSTREAM */
	struct ib_cq *(*create_cq)(struct ib_device *device,
				   const struct ib_cq_init_attr *attr,
				   struct ib_ucontext *context,
				   struct ib_udata *udata);
#else
	struct ib_cq *(*create_cq)(struct ib_device *ibdev, int entries, int vector,
				   struct ib_ucontext *ib_ctx,
				   struct ib_udata *udata);
#endif

	int (*modify_cq)(struct ib_cq *cq, u16 cq_count, u16 cq_period);
	int (*destroy_cq)(struct ib_cq *cq);
	int (*resize_cq)(struct ib_cq *cq, int cqe, struct ib_udata *udata);
	struct ib_mr *(*get_dma_mr)(struct ib_pd *pd, int mr_access_flags);
#ifdef DEFINE_USER_NO_MR_ID /* QEDR_UPSTREAM */
struct ib_mr *(*reg_user_mr)(struct ib_pd *, u64 start, u64 length,
			     u64 virt, int acc, struct ib_udata *);
#else
struct ib_mr *(*reg_user_mr)(struct ib_pd *, u64 start, u64 length,
			     u64 virt, int acc, struct ib_udata *,
			     int mr_id);
#endif

	int (*rereg_user_mr)(struct ib_mr *mr, int flags, u64 start, u64 length,
			     u64 virt_addr, int mr_access_flags,
			     struct ib_pd *pd, struct ib_udata *udata);
	int (*dereg_mr)(struct ib_mr *mr);
#ifdef DEFINE_ALLOC_MR /* QEDR_UPSTREAM */
	struct ib_mr *(*alloc_mr)(struct ib_pd *pd, enum ib_mr_type mr_type,
				  u32 max_num_sg);
#endif

#ifdef DEFINE_MAP_MR_SG /* QEDR_UPSTREAM */
#ifdef DEFINE_MAP_MR_SG_OFFSET /* QEDR_UPSTREAM */
	int (*map_mr_sg)(struct ib_mr *ibmr, struct scatterlist *sg,
		   int sg_nents, unsigned int *sg_offset);
#else
#ifndef DEFINE_MAP_MR_SG_UNSIGNED
	int (*map_mr_sg)(struct ib_mr *ibmr, struct scatterlist *sg,
		   int sg_nents);
#else
	int (*map_mr_sg)(struct ib_mr *ibmr, struct scatterlist *sg,
		   unsigned int sg_nents);
#endif
#endif
#endif

#ifdef _HAS_MW_SUPPORT /* QEDR_UPSTREAM */
#ifdef _HAS_ALLOC_MW_V2 /* QEDR_UPSTREAM */
	struct ib_mw *(*alloc_mw)(struct ib_pd *ibpd,  enum ib_mw_type type,
			    struct ib_udata *udata);
#else
	struct ib_mw *(*alloc_mw)(struct ib_pd *ibpd,  enum ib_mw_type type);
#endif

	int (*dealloc_mw)(struct ib_mw *mw);
#endif

	struct ib_fmr *(*alloc_fmr)(struct ib_pd *pd, int mr_access_flags,
				    struct ib_fmr_attr *fmr_attr);
	int (*map_phys_fmr)(struct ib_fmr *fmr, u64 *page_list, int list_len,
			    u64 iova);
	int (*unmap_fmr)(struct list_head *fmr_list);
	int (*dealloc_fmr)(struct ib_fmr *fmr);
	struct ib_xrcd *(*alloc_xrcd)(struct ib_device *device,
				      struct ib_ucontext *ucontext,
				      struct ib_udata *udata);
	int (*dealloc_xrcd)(struct ib_xrcd *xrcd);

	/* iWarp CM callbacks */
	void (*iw_add_ref)(struct ib_qp *qp);
	void (*iw_rem_ref)(struct ib_qp *qp);
	struct ib_qp *(*iw_get_qp)(struct ib_device *device, int qpn);

	int (*iw_connect)(struct iw_cm_id *cm_id,
			  struct iw_cm_conn_param *conn_param);
	int (*iw_accept)(struct iw_cm_id *cm_id,
			 struct iw_cm_conn_param *conn_param);
	int (*iw_reject)(struct iw_cm_id *cm_id, const void *pdata,
			 u8 pdata_len);
	int (*iw_create_listen)(struct iw_cm_id *cm_id, int backlog);
	int (*iw_destroy_listen)(struct iw_cm_id *cm_id);

};

void ib_set_device_ops(struct ib_device *dev, const struct ib_device_ops *ops);

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

#if defined(_HAS_SECURE_BOOT)  && defined(_HAS_EFI_ENABLED)
#define _efi_enabled efi_enabled(EFI_SECURE_BOOT)
#else
#define _efi_enabled 0
#endif

#endif
