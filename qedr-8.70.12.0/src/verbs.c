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
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/udp.h>
#include <linux/iommu.h>
#include <linux/inetdevice.h>
#include <net/addrconf.h>
#include <net/route.h>
#include <net/ip6_route.h>
#include <linux/hugetlb.h>
#include <net/flow.h>
#include <rdma/ib_verbs.h>
#include <rdma/ib_user_verbs.h>
#include <rdma/iw_cm.h>
#include <rdma/ib_umem.h>
#include <rdma/ib_addr.h>
#if DEFINE_ROCE_GID_TABLE /* QEDR_UPSTREAM */
#include <rdma/ib_cache.h>
#endif
#ifndef _HAS_IB_CONTEXT
#include <rdma/uverbs_ioctl.h>
#endif

#include "common_hsi.h"
#include "qedr_hsi_rdma.h"
#include "qed_if.h"
#include "qedr.h"
#include "verbs.h"
#include "qedr_roce_cm.h"
#include "qedr_iw_cm.h"

#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
#include "qedr_user.h"
#include "qedr_compat.h"
#endif

#include "qedr_gdr.h"

#define QEDR_SRQ_WQE_ELEM_SIZE	sizeof(union rdma_srq_elm)
#define	RDMA_MAX_SGE_PER_SRQ	(4)	/* Should be part of HSI */
/* Should be part of HSI */
#define RDMA_MAX_SRQ_WQE_SIZE	(RDMA_MAX_SGE_PER_SRQ + 1)	/* +1 for header */

/* Increment srq wr producer by one */
static void qedr_inc_srq_wr_prod(struct qedr_srq_hwq_info *info)
{
	info->wr_prod_cnt++;
}

/* Increment srq wr consumer by one */
static void qedr_inc_srq_wr_cons(struct qedr_srq_hwq_info *info)
{
	atomic_inc(&info->wr_cons_cnt);
}

int qedr_query_pkey(struct ib_device *ibdev, COMPAT_PORT(port), u16 index, u16 *pkey)
{
	if (index > QEDR_ROCE_PKEY_TABLE_LEN)
		return -EINVAL;

	*pkey = QEDR_ROCE_PKEY_DEFAULT;

	return 0;
}

int qedr_iw_query_gid(struct ib_device *ibdev, COMPAT_PORT(port),
		      int index, union ib_gid *sgid)
{
	struct qedr_dev *dev = get_qedr_dev(ibdev);

	memset(sgid->raw, 0, sizeof(sgid->raw));
	ether_addr_copy(sgid->raw, dev->ndev->dev_addr);

	DP_DEBUG(dev, QEDR_MSG_INIT, "QUERY sgid[%d]=%llx:%llx\n", index,
		 sgid->global.interface_id, sgid->global.subnet_prefix);

	return 0;
}

#ifndef NOT_DEFINED_GET_CACHED_GID /* !QEDR_UPSTREAM */
int qedr_query_gid(struct ib_device *ibdev, COMPAT_PORT(port), int index,
		   union ib_gid *sgid)
{
	struct qedr_dev *dev = get_qedr_dev(ibdev);
	int rc = 0;

#if DEFINE_ROCE_GID_TABLE /* QEDR_UPSTREAM */
	if (!rdma_cap_roce_gid_table(ibdev, port))
		return -ENODEV;

	rc = ib_get_cached_gid(ibdev, port, index, sgid, NULL);
	if (rc == -EAGAIN) {
		memcpy(sgid, &zgid, sizeof(*sgid));
		return 0;
	}
#else
	if ((index >= QEDR_MAX_SGID) || (index < 0)) {
		DP_ERR(dev, "query gid: invalid gid index %d\n", index);
		memset(sgid, 0, sizeof(*sgid));
		return -EINVAL;
	}
	memcpy(sgid, &dev->sgid_tbl[index], sizeof(*sgid));
#endif

	DP_DEBUG(dev, QEDR_MSG_INIT, "query gid: index=%d %llx:%llx\n", index,
		 sgid->global.interface_id, sgid->global.subnet_prefix);

	return rc;
}
#endif

#if DEFINE_ROCE_GID_TABLE
#ifndef REMOVE_DEVICE_ADD_DEL_GID /* !QEDR_UPSTREAM */
int qedr_add_gid(struct ib_device *device, COMPAT_PORT(port_num),
		 unsigned int index, const union ib_gid *gid,
		 const struct ib_gid_attr *attr, void **context)
{
	if (!rdma_cap_roce_gid_table(device, port_num))
		return -EINVAL;

	if (port_num > QEDR_MAX_PORT)
		return -EINVAL;

	if (!context)
		return -EINVAL;

	return 0;
}

int qedr_del_gid(struct ib_device *device, COMPAT_PORT(port_num),
		 unsigned int index, void **context)
{
	if (!rdma_cap_roce_gid_table(device, port_num))
		return -EINVAL;

	if (port_num > QEDR_MAX_PORT)
		return -EINVAL;

	if (!context)
		return -EINVAL;

	return 0;
}
#endif
#endif

int qedr_query_srq(struct ib_srq *ibsrq, struct ib_srq_attr *srq_attr)
{
	struct qedr_dev *dev = get_qedr_dev(ibsrq->device);
	struct qedr_device_attr *qattr = &dev->attr;
	struct qedr_srq *srq = get_qedr_srq(ibsrq);

	memset(srq_attr, 0, sizeof(*srq_attr));

	srq_attr->srq_limit = srq->srq_limit;
	srq_attr->max_wr = qattr->max_srq_wr;
	srq_attr->max_sge = qattr->max_sge;

	return 0;
}

#ifdef DEFINE_QUERY_DEVICE_PASS_VENDOR_SPECIFIC_DATA /* QEDR_UPSTREAM */
int qedr_query_device(struct ib_device *ibdev,
		      struct ib_device_attr *attr, struct ib_udata *udata)
#else
int qedr_query_device(struct ib_device *ibdev, struct ib_device_attr *attr)
#endif
{
	struct qedr_dev *dev = get_qedr_dev(ibdev);
	struct qedr_device_attr *qattr = &dev->attr;

#ifdef DEFINE_QUERY_DEVICE_PASS_VENDOR_SPECIFIC_DATA /* QEDR_UPSTREAM */
	if (udata->inlen || udata->outlen)
		return -EINVAL;
#endif

	if (!dev->rdma_ctx) {
		DP_ERR(dev,
		       "qedr_query_device called with invalid params rdma_ctx=%p\n",
		       dev->rdma_ctx);
		return -EINVAL;
	}

	memset(attr, 0, sizeof(*attr));

	attr->fw_ver = qattr->fw_ver;
	attr->sys_image_guid = qattr->sys_image_guid;
	attr->max_mr_size = qattr->max_mr_size;
	attr->page_size_cap = qattr->page_size_caps;
	attr->vendor_id = qattr->vendor_id;
	attr->vendor_part_id = qattr->vendor_part_id;
	attr->hw_ver = qattr->hw_ver;
	attr->max_qp = qattr->max_qp;
	attr->max_qp_wr = max_t(u32, qattr->max_sqe, qattr->max_rqe);
	attr->device_cap_flags = IB_DEVICE_CURR_QP_STATE_MOD |
	    IB_DEVICE_RC_RNR_NAK_GEN |
#ifdef _HAS_MW_SUPPORT /* QEDR_UPSTREAM */
	    IB_DEVICE_MEM_WINDOW_TYPE_2B |
#endif
#ifdef _HAS_IB_DEVICE_LOCAL_DMA_LKEY
	    IB_DEVICE_LOCAL_DMA_LKEY |
#endif
	    IB_DEVICE_MEM_MGT_EXTENSIONS;

#ifndef _HAS_IB_DEVICE_LOCAL_DMA_LKEY /* QEDR_UPSTREAM */
	attr->kernel_cap_flags = IBK_LOCAL_DMA_LKEY;
#endif

#ifdef _HAS_XRC_SUPPORT /* QEDR_UPSTREAM */
	if (!IS_IWARP(dev))
		attr->device_cap_flags |= IB_DEVICE_XRC;
#endif

#ifdef DEFINE_MAX_SEND_RECV_DEVICE_SGE /* QEDR_UPSTREAM */
	attr->max_send_sge = qattr->max_sge;
	attr->max_recv_sge = qattr->max_sge;
#else
	attr->max_sge = qattr->max_sge;
#endif
	attr->max_sge_rd = qattr->max_sge;
	attr->max_cq = qattr->max_cq;
	attr->max_cqe = qattr->max_cqe;
	attr->max_mr = qattr->max_mr;
	attr->max_mw = qattr->max_mw;
	attr->max_pd = qattr->max_pd;
	attr->atomic_cap = dev->atomic_cap;

#ifdef _HAS_FMR_SUPPORT /* !QEDR_UPSTREAM */
	attr->max_fmr = qattr->max_fmr;
	attr->max_map_per_fmr = INT_MAX;
#endif
	/* INTERNAL: There is an implicit assumption in some of the ib_xxx apps
	 * that the qp_rd_atom is smaller than the qp_init_rd_atom.
	 * Specifically, incommunication the qp_rd_atom is passed to the other
	 * side and used as init_rd_atom without check device capabilities for
	 * init_rd_atom. for this reason, we set the qp_rd_atom to be the
	 * minimum between the two...There is an additional assumption in mlx4
	 * driver that the values are power of two, fls is performed on the
	 * value - 1, which in fact gives a larger power of two for values which
	 * are not a power of two. This should be fixed in mlx4 driver, but
	 * until then -> we provide a value that is a power of two in our code.
	 */
	attr->max_qp_init_rd_atom =
	    1 << (fls(qattr->max_qp_req_rd_atomic_resc) - 1);
	attr->max_qp_rd_atom =
	    min(1 << (fls(qattr->max_qp_resp_rd_atomic_resc) - 1),
		attr->max_qp_init_rd_atom);

	attr->max_srq = qattr->max_srq;
	attr->max_srq_sge = qattr->max_srq_sge;
	attr->max_srq_wr = qattr->max_srq_wr;

	/* INTERNAL: TODO: R&D to more properly configure the following */
	attr->local_ca_ack_delay = qattr->dev_ack_delay;
	attr->max_fast_reg_page_list_len = qattr->max_mr / 8;
	attr->max_pkeys = QEDR_ROCE_PKEY_MAX;
	attr->max_ah = qattr->max_ah;

	return 0;
}

#if DEFINE_IB_PORT_SPEED	/* !QEDR_UPSTREAM */
#define QEDR_SPEED_SDR		(IB_SPEED_SDR)
#define QEDR_SPEED_DDR		(IB_SPEED_DDR)
#define QEDR_SPEED_QDR		(IB_SPEED_QDR)
#define QEDR_SPEED_FDR10	(IB_SPEED_FDR10)
#define QEDR_SPEED_FDR		(IB_SPEED_FDR)
#define QEDR_SPEED_EDR		(IB_SPEED_EDR)
#else
#define QEDR_SPEED_SDR		(1)
#define QEDR_SPEED_DDR		(2)
#define QEDR_SPEED_QDR		(4)
#define QEDR_SPEED_FDR10	(8)
#define QEDR_SPEED_FDR		(16)
#define QEDR_SPEED_EDR		(32)
#endif

static inline void get_link_speed_and_width(int speed, COMPAT_IB_SPEED_TYPE,
					    u8 *ib_width)
{
	switch (speed) {
	case 1000:
		*ib_speed = QEDR_SPEED_SDR;
		*ib_width = IB_WIDTH_1X;
		break;
	case 10000:
		*ib_speed = QEDR_SPEED_QDR;
		*ib_width = IB_WIDTH_1X;
		break;

	case 20000:
		*ib_speed = QEDR_SPEED_DDR;
		*ib_width = IB_WIDTH_4X;
		break;

	case 25000:
		*ib_speed = QEDR_SPEED_EDR;
		*ib_width = IB_WIDTH_1X;
		break;

	case 40000:
		*ib_speed = QEDR_SPEED_QDR;
		*ib_width = IB_WIDTH_4X;
		break;

	case 50000:
		*ib_speed = QEDR_SPEED_50G;
		*ib_width = QEDR_WIDTH_50G;
		break;

	case 100000:
		*ib_speed = QEDR_SPEED_EDR;
		*ib_width = IB_WIDTH_4X;
		break;

	default:
		/* Unsupported */
		*ib_speed = QEDR_SPEED_SDR;
		*ib_width = IB_WIDTH_1X;
	}
}

int qedr_query_port(struct ib_device *ibdev, COMPAT_PORT(port), struct ib_port_attr *attr)
{
	struct qedr_dev *dev;
	struct qed_rdma_port *rdma_port;

	dev = get_qedr_dev(ibdev);

	if (port > 1) {
		DP_ERR(dev, "invalid_port=0x%x\n", port);
		return -EINVAL;
	}

	if (!dev->rdma_ctx) {
		DP_ERR(dev, "rdma_ctx is NULL\n");
		return -EINVAL;
	}

	/* If recovery is in progress, fill in some defaults... */
	if (dev->recov_info.recov_in_prog || dev->recov_info.dead) {
		attr->state = IB_PORT_DOWN;
		attr->phys_state = IB_PORT_PHYS_STATE_DISABLED;

		return 0;
	}

	rdma_port = dev->ops->rdma_query_port(dev->rdma_ctx);
	
	if (!rdma_port) {
		DP_ERR(dev, "rdma_port is NULL\n");
		return -EINVAL;
	}

	/* *attr being zeroed by the caller, avoid zeroing it here */
	if (rdma_port->port_state == QED_RDMA_PORT_UP) {
		attr->state = IB_PORT_ACTIVE;
		attr->phys_state = 5;
	} else {
		attr->state = IB_PORT_DOWN;
		attr->phys_state = 3;
	}

	attr->max_mtu = IB_MTU_4096;

	attr->lid = 0;
	attr->lmc = 0;
	attr->sm_lid = 0;
	attr->sm_sl = 0;
#ifdef DEFINE_IB_UD_HEADER_INIT_UDP_PRESENT /* QEDR_UPSTREAM */
#ifdef DEFINE_IP_GIDS
	attr->ip_gids = true;
	attr->port_cap_flags = 0;
#else
	attr->port_cap_flags = IB_PORT_IP_BASED_GIDS;
#endif
#else
	attr->port_cap_flags = 0;
#endif
	if (rdma_protocol_iwarp(&dev->ibdev, 1)) {
		attr->active_mtu = iboe_get_mtu(dev->iwarp_max_mtu);
		attr->gid_tbl_len = 1;
		attr->pkey_tbl_len = 1;
	} else {
		attr->active_mtu = iboe_get_mtu(dev->ndev->mtu);
		attr->gid_tbl_len = QEDR_MAX_SGID;
		attr->pkey_tbl_len = QEDR_ROCE_PKEY_TABLE_LEN;
	}

	if (!dev->is_vf) {
		attr->bad_pkey_cntr = rdma_port->pkey_bad_counter;
		attr->qkey_viol_cntr = 0;
		attr->max_msg_sz = rdma_port->max_msg_size;
	}
	get_link_speed_and_width(rdma_port->link_speed,
				 &attr->active_speed,
				 &attr->active_width);
	attr->max_vl_num = 4; /* INTERNAL: TODO -> figure this one out... */

	return 0;
}

int qedr_modify_port(struct ib_device *ibdev, COMPAT_PORT(port), int mask,
		     struct ib_port_modify *props)
{
	struct qedr_dev *dev;

	dev = get_qedr_dev(ibdev);

	if (port > 1) {
		DP_ERR(dev, "invalid_port=0x%x\n", port);
		return -EINVAL;
	}

	return 0;
}

static int qedr_add_mmap(struct qedr_ucontext *uctx, u64 phy_addr,
			 unsigned long len)
{
	struct qedr_mm *mm;

	mm = kzalloc(sizeof(*mm), GFP_KERNEL);
	if (!mm)
		return -ENOMEM;

	mm->key.phy_addr = phy_addr;
	/* This function might be called with a length which is not a multiple
	 * of PAGE_SIZE, while the mapping is PAGE_SIZE grained and the kernel
	 * forces this granularity by increasing the requested size if needed.
	 * When qedr_mmap is called, it will search the list with the updated
	 * length as a key. To prevent search failures, the length is rounded up
	 * in advance to PAGE_SIZE.
	 */
	mm->key.len = roundup(len, PAGE_SIZE);
	INIT_LIST_HEAD(&mm->entry);

	mutex_lock(&uctx->mm_list_lock);
	list_add(&mm->entry, &uctx->mm_head);
	mutex_unlock(&uctx->mm_list_lock);

	DP_DEBUG(uctx->dev, QEDR_MSG_MISC,
		 "added (addr=0x%llx,len=0x%lx) for ctx=%p\n",
		 (unsigned long long)mm->key.phy_addr,
		 (unsigned long)mm->key.len, uctx);

	return 0;
}

static bool qedr_search_mmap(struct qedr_ucontext *uctx, u64 phy_addr,
			     unsigned long len)
{
	bool found = false;
	struct qedr_mm *mm;

	mutex_lock(&uctx->mm_list_lock);
	list_for_each_entry(mm, &uctx->mm_head, entry) {
		if (len != mm->key.len || phy_addr != mm->key.phy_addr)
			continue;

		found = true;
		break;
	}
	mutex_unlock(&uctx->mm_list_lock);
	DP_DEBUG(uctx->dev, QEDR_MSG_MISC,
		 "searched for (addr=0x%llx,len=0x%lx) for ctx=%p, result=%d\n",
		 mm->key.phy_addr, mm->key.len, uctx, found);

	return found;
}

int qedr_recov_check_state(struct qedr_dev *dev, const char *func_name)
{
	if (dev->recov_info.dead) {
		DP_ERR(dev, "Failed %s. Device is in fatal error state\n",
		       func_name);

		return -EFAULT;
	}

	/* Note - we could decide to wait for recovery to end at this state */
	if (dev->recov_info.recov_in_prog) {
		DP_ERR(dev, "Failed %s. Device is during recovery\n",
		       func_name);

		return -EAGAIN;
	}

	return 0;
}

COMPAT_ALLOC_UCTX_DECLARE_RET
qedr_alloc_ucontext(COMPAT_ALLOC_UCTX_IBDEV(struct ib_device *ibdev)
		    COMPAT_ALLOC_UCTX_UCTX(struct ib_ucontext *uctx)
		    struct ib_udata *udata)

{
	COMPAT_ALLOC_UCTX(struct ib_device *ibdev = uctx->device)
	struct qedr_ucontext *ctx;
	struct qedr_alloc_ucontext_resp uresp = {};
	struct qedr_dev *dev = get_qedr_dev(ibdev);
	struct qed_rdma_add_user_out_params oparams = {};
	struct qedr_alloc_ucontext_req ureq = {};
	int rc = 0;

	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
		return COMPAT_ALLOC_UCTX_RET(rc);

	if (!udata)
		return COMPAT_ALLOC_UCTX_RET(-EFAULT);

#ifndef _HAS_UCONTEXT_ALLOCATION /* !QEDR_UPSTREAM */
	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);
#else
	ctx = get_qedr_ucontext(uctx);
#endif

	if (udata->inlen <= sizeof(struct ib_uverbs_cmd_hdr)) {
		/* Old rdma-core version, nothing to copy from user.
		* inlen could be equal to the size of ib_uverbs_cmd_hdr
		* since in older kernel versions this size isn't reduced
		* from the user buffer size. We might get inlen greater than
		* zero even though the user didn't send any data.
		* This bug is solved in newer kernel versions
		*/
		ctx->edpm_mode = 0;
	} else {
		rc = qedr_copy_from_udata(&ureq, udata, sizeof(ureq));
		if (rc) {
			DP_ERR(dev, "Problem copying data from user space\n");
			kfree(ctx);
			return COMPAT_ALLOC_UCTX_RET(-EFAULT);
		}
		ctx->edpm_mode = GET_FIELD(ureq.context_flags,
					   QEDR_ROCE_EDPM_MODE_V1);
	}

	rc = dev->ops->rdma_add_user(dev->rdma_ctx, &oparams);
	if (rc) {
		DP_ERR(dev,
		       "failed to allocate a DPI for a new RoCE application, rc=%d. To overcome this consider to increase the number of DPIs, increase the doorbell BAR size or just close unnecessary RoCE applications. In order to increase the number of DPIs consult the qedr readme\n",
		       rc);
		goto err;
	}

	ctx->dpi = oparams.dpi;
	ctx->dpi_addr = oparams.dpi_addr;
	ctx->dpi_phys_addr = oparams.dpi_phys_addr;
	ctx->dpi_size = oparams.dpi_size;
	INIT_LIST_HEAD(&ctx->mm_head);
	mutex_init(&ctx->mm_list_lock);

#ifndef QEDR_UPSTREAM_DPM /* !QEDR_UPSTREAM_DPM */
	uresp.dpm_enabled = dev->dpm_enabled;
	uresp.wids_enabled = 1;
	uresp.wid_count = oparams.wid_count;
#endif
	uresp.db_pa = ctx->dpi_phys_addr;
	uresp.db_size = ctx->dpi_size;
	uresp.max_send_wr = dev->attr.max_sqe;
	uresp.max_recv_wr = dev->attr.max_rqe;
	uresp.max_srq_wr = dev->attr.max_srq_wr;
	uresp.sges_per_send_wr = QEDR_MAX_SQE_ELEMENTS_PER_SQE;
	uresp.sges_per_recv_wr = QEDR_MAX_RQE_ELEMENTS_PER_RQE;
	uresp.sges_per_srq_wr = dev->attr.max_srq_sge;
	uresp.max_cqes = QEDR_MAX_CQES;

	rc = qedr_copy_to_udata(udata, &uresp, sizeof(uresp));
	if (rc)
		goto err;

	ctx->dev = dev;

	rc = qedr_add_mmap(ctx, ctx->dpi_phys_addr, ctx->dpi_size);
	if (rc)
		goto err;

	qedr_recov_obj_add(dev, &ctx->recov_info);

	DP_DEBUG(dev, QEDR_MSG_INIT, "Allocating user context %p\n",
		 &ctx->ibucontext);
#ifndef _HAS_UCONTEXT_ALLOCATION /* !QEDR_UPSTREAM */
	return &ctx->ibucontext;
#else
	return 0;
#endif

err:
	COMPAT_NO_ALLOC_UCTX(kfree(ctx));
	return COMPAT_ALLOC_UCTX_RET(rc);
}

COMPAT_DEALLOC_UCTX_RET qedr_dealloc_ucontext(struct ib_ucontext *ibctx)
{
	struct qedr_ucontext *uctx = get_qedr_ucontext(ibctx);
	struct qedr_mm *mm, *tmp;
	COMPAT_NO_ALLOC_UCTX(int status = 0);

	DP_DEBUG(uctx->dev, QEDR_MSG_INIT, "Deallocating user context %p\n",
		 uctx);

	uctx->dev->ops->rdma_remove_user(uctx->dev->rdma_ctx, uctx->dpi);

	list_for_each_entry_safe(mm, tmp, &uctx->mm_head, entry) {
		DP_DEBUG(uctx->dev, QEDR_MSG_MISC,
			 "deleted (addr=0x%llx,len=0x%lx) for ctx=%p\n",
			 mm->key.phy_addr, mm->key.len, uctx);
		list_del(&mm->entry);
		kfree(mm);
	}

	qedr_recov_obj_del(uctx->dev, &uctx->recov_info);

	COMPAT_NO_ALLOC_UCTX(kfree(uctx));
	COMPAT_NO_ALLOC_UCTX(return status);
}

int qedr_mmap(struct ib_ucontext *context, struct vm_area_struct *vma)
{
	struct qedr_ucontext *ucontext = get_qedr_ucontext(context);
	struct qedr_dev *dev = get_qedr_dev(context->device);
	unsigned long phys_addr = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long len = (vma->vm_end - vma->vm_start);
	unsigned long dpi_start;
	int rc;

	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
		return -EPERM;

	dpi_start = dev->db_phys_addr + (ucontext->dpi * ucontext->dpi_size);

	DP_DEBUG(dev, QEDR_MSG_INIT,
		 "mmap invoked with vm_start=0x%lx, vm_end=0x%lx,vm_pgoff=0x%lx; dpi_start=0x%lx dpi_size=0x%x\n",
		 vma->vm_start, vma->vm_end, vma->vm_pgoff, dpi_start,
		 ucontext->dpi_size);

	if ((vma->vm_start & (PAGE_SIZE - 1)) || (len & (PAGE_SIZE - 1))) {
		DP_ERR(dev,
		       "failed mmap, adrresses must be page aligned: start=0x%lx, end=0x%lx\n",
		       vma->vm_start, vma->vm_end);
		return -EINVAL;
	}

	if (!qedr_search_mmap(ucontext, phys_addr, len)) {
		DP_ERR(dev, "failed mmap, vm_pgoff=0x%lx is not authorized\n",
		       vma->vm_pgoff);
		return -EINVAL;
	}

	if ((phys_addr < dpi_start) ||
	    ((phys_addr + len) > (dpi_start + ucontext->dpi_size))) {
		DP_ERR(dev,
		       "failed mmap, pages are outside of dpi; page address=0x%lx, dpi_start=0x%lx, dpi_size=0x%x\n",
		       phys_addr, dpi_start, ucontext->dpi_size);
		return -EINVAL;
	}

	if (vma->vm_flags & VM_READ) {
		DP_ERR(dev, "failed mmap, cannot map doorbell bar for read\n");
		return -EINVAL;
	}

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	return io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, len,
				  vma->vm_page_prot);
}

void qedr_recov_obj_add(struct qedr_dev *dev, struct qedr_recov_obj_info *info)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->recov_info.recov_lock, flags);
	if (dev->recov_info.recov_in_prog)
		info->reset = true;
	list_add_tail(&info->entry, &dev->recov_info.recov_obj_list);
	info->added = true;
	spin_unlock_irqrestore(&dev->recov_info.recov_lock, flags);
}

void qedr_recov_obj_del(struct qedr_dev *dev, struct qedr_recov_obj_info *info)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->recov_info.recov_lock, flags);
	if (info->added)
		list_del(&info->entry);
	info->added = false;
	spin_unlock_irqrestore(&dev->recov_info.recov_lock, flags);
}

static void qedr_recov_cq_reopen(struct qedr_dev *dev, struct qedr_cq *cq)
{
	struct qed_chain_params chain_params;
	struct ib_cq *ibcq = &cq->ibcq;
	int page_cnt, rc = 0;
	u32 db_offset;
	u64 pbl_ptr;
	u16 icid;

	if (cq->cq_type == QEDR_CQ_TYPE_GSI)  /* QEDR_UPSTREAM_REMOVE_COLON */
		return;

	db_offset = DB_ADDR_SHIFT(DQ_PWM_OFFSET_UCM_RDMA_CQ_CONS_32BIT);

	cq->cnq_notif = 0;

	/* Free older chain */
	dev->ops->common->chain_free(dev->cdev, &cq->pbl);

	/* Init new chain */
	dev->ops->common->chain_params_init(&chain_params,
					    QED_CHAIN_USE_TO_CONSUME,
					    QED_CHAIN_MODE_PBL,
					    QED_CHAIN_CNT_TYPE_U32,
					    cq->params.cq_size + 1,
					    sizeof(union rdma_cqe));
	rc = dev->ops->common->chain_alloc(dev->cdev, &cq->pbl, &chain_params);
	if (rc) {
		DP_ERR(dev,
		       "can't alloc chain CQ icid=0x%0x,addr=%p\n",
			cq->icid, cq);
		goto err;
	}

	page_cnt = qed_chain_get_page_cnt(&cq->pbl);
	pbl_ptr = qed_chain_get_pbl_phys(&cq->pbl);
	cq->ibcq.cqe = cq->pbl.capacity;
	cq->params.pbl_num_pages = page_cnt;
	cq->params.pbl_ptr = pbl_ptr;
	rc = dev->ops->rdma_create_cq(dev->rdma_ctx, &cq->params, &icid);
	if (rc) {
		DP_ERR(dev,
		       "can't reopen CQ icid=0x%0x, addr=%p\n",
			cq->icid, cq);
		goto err;
	}

	DP_DEBUG(dev, QEDR_MSG_CQ,
		 "old icid=0x%0x, new icid=0x%0x addr=%p vector=%d\n",
		 cq->icid, icid, cq, cq->params.cnq_id);

	cq->icid = icid;

	cq->db_addr = (void *)(uintptr_t)dev->db_addr + db_offset;
	cq->db.data.icid = cpu_to_le16(cq->icid);
	cq->db.data.params = DB_AGG_CMD_MAX <<
		RDMA_PWM_VAL32_DATA_AGG_CMD_SHIFT;

	/* point to the very last element, passing it we will toggle */
	cq->toggle_cqe = qed_chain_get_last_elem(&cq->pbl);
	cq->pbl_toggle = RDMA_CQE_REQUESTER_TOGGLE_BIT_MASK;

	/* INTERNAL: Latest CQE must be different from pbl_toggle */
	cq->latest_cqe = NULL;

	consume_cqe(cq);

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
	rc = qedr_db_recovery_add(dev, cq->db_addr, &cq->db.data,
				  DB_REC_WIDTH_64B, DB_REC_KERNEL);
	if (rc) {
		DP_ERR(dev,
		       "KRNL:db recov add fail icid=0x%0x, addr=%p\n",
			cq->icid, cq);
		goto err;
	}
#endif
	cq->recov_info.reset = false;
	cq->reset_notify_added = false;

	/* Re-arm CQs (for which STACK asked) to generate HW CNQ */
	if (cq->arm_cq) {
		cq->reopened = true;
		qedr_arm_cq(ibcq, cq->arm_cq_flags);

		cq->reopened = false;
		cq->arm_cq = false;
		cq->arm_cq_flags = 0;
	}

err:

	return;
}

static void qedr_recov_cq_completion_cb(struct qedr_dev *dev,
					struct qedr_cq *cq)
{
	bool call_comp = false;
	unsigned long flags;

	/* Relevant only for CQs with completion handlers */
	if (!cq->ibcq.comp_handler || cq->reset_notify_added)
		return;

	/* Use recovery lock not to clash with cq lock on poll_cq/comp_cb.. */
	spin_lock_irqsave(&dev->recov_info.recov_lock, flags);
	if (!cq->reset_notify_added) {
		cq->reset_notify_added = true;
		call_comp = true;
	}
	spin_unlock_irqrestore(&dev->recov_info.recov_lock, flags);

	/* There is no use in calling completion callback if it was called from
	 * the drain functionality as that is the last WQE
	 */
	if (call_comp)
		(*cq->ibcq.comp_handler)(&cq->ibcq, cq->ibcq.cq_context);
}

/* Called under the recov_lock */
static void qedr_recov_cq(struct qedr_dev *dev,
			  struct qedr_recov_obj_info *info, int event)
{
	struct qedr_cq *cq = container_of(info, struct qedr_cq, recov_info);

	if (cq->cq_type == QEDR_CQ_TYPE_USER)
		return;

	switch (event) {
	case QEDE_RESET_EVENT_START:
		qedr_recov_cq_completion_cb(dev, cq);
	break;

	case QEDE_RESET_EVENT_DONE:
		/* Wait 100 ms and SW handle for any DREQ GSI posted after
		 * QEDE_RESET_EVENT_START and before qedr_recov_cq_reopen().
		 */
		msleep(100);
		if (cq->polled_for_reset_qp) {
			cq->reset_notify_added = false;
			qedr_recov_cq_completion_cb(dev, cq);
		}

		qedr_recov_cq_reopen(dev, cq);
	break;
	}
}

COMPAT_ALLOC_PD_DECLARE_RET
qedr_alloc_pd(COMPAT_ALLOC_PD_IBDEV(struct ib_device *ibdev)
	      COMPAT_ALLOC_PD_PD(struct ib_pd *ibpd)
	      COMPAT_ALLOC_PD_CXT(struct ib_ucontext *context)
	      struct ib_udata *udata)
{
	COMPAT_ALLOC_PD(struct ib_device *ibdev = ibpd->device)
	struct qedr_dev *dev = get_qedr_dev(ibdev);
	struct qedr_pd *pd;
	u16 pd_id;
	int rc = 0;

	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
		return COMPAT_ALLOC_PD_RET(rc);

	DP_DEBUG(dev, QEDR_MSG_INIT, "Function called from: %s\n",
		 (udata) ? "User Lib" : "Kernel");

	if (!dev->rdma_ctx) {
		DP_ERR(dev, "invalid RDMA context\n");
		return COMPAT_ALLOC_PD_RET(-EINVAL);
	}

#ifndef _HAS_PD_ALLOCATION /* !QEDR_UPSTREAM */
	pd = kzalloc(sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return ERR_PTR(-ENOMEM);
#else
	pd = get_qedr_pd(ibpd);
#endif

	rc = dev->ops->rdma_alloc_pd(dev->rdma_ctx, &pd_id);
	if (rc)
		goto err;

	pd->pd_id = pd_id;

	if (udata) {
		struct qedr_alloc_pd_uresp uresp = {
			.pd_id = pd_id,
		};
		struct qedr_ucontext *ctx = GET_DRV_CXT(context);

		rc = qedr_copy_to_udata(udata, &uresp, sizeof(uresp));
		if (rc) {
			DP_ERR(dev, "copy error pd_id=0x%x.\n", pd_id);
			dev->ops->rdma_dealloc_pd(dev->rdma_ctx, pd_id);
			goto err;
		}

		pd->uctx = ctx;
		pd->uctx->pd = pd;
	}

	qedr_recov_obj_add(dev, &pd->recov_info);

	COMPAT_ALLOC_PD(return rc);
	COMPAT_NO_ALLOC_PD(return &pd->ibpd);
err:

	COMPAT_NO_ALLOC_PD(kfree(pd));
	return COMPAT_ALLOC_PD_RET(rc);
}

COMPAT_DEALLOC_PD_DECLARE_RET
qedr_dealloc_pd(struct ib_pd *ibpd
		COMPAT_DEALLOC_PD_UDATA(struct ib_udata *udata))
{
	struct qedr_dev *dev = get_qedr_dev(ibpd->device);
	struct qedr_pd *pd = get_qedr_pd(ibpd);

#ifndef _HAS_PD_ALLOCATION /* !QEDR_UPSTREAM */
	if (!pd) {
		DP_ERR(dev, "qedr_dealloc_pd failed due to NULL PD\n");
		return -EINVAL;
	}
#endif

	DP_DEBUG(dev, QEDR_MSG_INIT, "Deallocating PD %d\n", pd->pd_id);

	if (!pd->recov_info.reset)
		dev->ops->rdma_dealloc_pd(dev->rdma_ctx, pd->pd_id);

	qedr_recov_obj_del(dev, &pd->recov_info);

	COMPAT_NO_ALLOC_PD(kfree(pd)); /* !QEDR_UPSTREAM */
	COMPAT_DEALLOC_PD_RET(return 0;) /* !QEDR_UPSTREAM */
}

#ifdef _HAS_XRC_SUPPORT /* QEDR_UPSTREAM */
COMPAT_QEDR_ALLOC_XRCD_RET_PARAM
qedr_alloc_xrcd(COMPAT_ALLOC_XRCD_FIRST_PARAM,
		COMPAT_ALLOC_XRCD_CXT(struct ib_ucontext *context)
		struct ib_udata *udata)
{
#ifdef _HAS_ALLOC_XRCD_IB_XRCD
	struct qedr_dev *dev = get_qedr_dev(ibxrcd->device);
	struct qedr_xrcd *xrcd = get_qedr_xrcd(ibxrcd);
#else
	struct qedr_dev *dev = get_qedr_dev(ibdev);
	struct qedr_xrcd *xrcd;
#endif
	u16 xrcd_id;
	int rc;

	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
		COMPAT_QEDR_ALLOC_XRCD_RET(rc);

	DP_DEBUG(dev, QEDR_MSG_INIT, "Function called from: %s\n",
		 (udata) ? "User Lib" : "Kernel");

#ifndef _HAS_ALLOC_XRCD_IB_XRCD
	xrcd = kzalloc(sizeof(*xrcd), GFP_KERNEL);
	if (!xrcd)
		return ERR_PTR(-ENOMEM);
#endif
	rc = dev->ops->rdma_alloc_xrcd(dev->rdma_ctx, &xrcd_id);
	if (rc)
		goto err;

	xrcd->xrcd_id = xrcd_id;

	qedr_recov_obj_add(dev, &xrcd->recov_info);

#ifndef _HAS_ALLOC_XRCD_IB_XRCD
	return &xrcd->ibxrcd;
#endif
err:
	COMPAT_QEDR_KFREE(xrcd);
	COMPAT_QEDR_ALLOC_XRCD_RET(rc);
}

COMPAT_QEDR_DEALLOC_XRCD_DECLARE_RET
qedr_dealloc_xrcd(struct ib_xrcd *ibxrcd
		      COMPAT_DEALLOC_XRCD_UDATA(struct ib_udata *udata))
{
	struct qedr_dev *dev = get_qedr_dev(ibxrcd->device);
	struct qedr_xrcd *xrcd = get_qedr_xrcd(ibxrcd);

	if (!xrcd) {
		DP_ERR(dev, "qedr_dealloc_xrcd failed due to NULL xrcd\n");
		COMPAT_QEDR_DEALLOC_XRCD_RET(-EINVAL);
	}

	DP_DEBUG(dev, QEDR_MSG_INIT, "Deallocating XRCD %d\n", xrcd->xrcd_id);

	if (!xrcd->recov_info.reset) /* !QEDR_UPSTREAM_RECOVER_REMOVE_LINE */
		dev->ops->rdma_dealloc_xrcd(dev->rdma_ctx, xrcd->xrcd_id);

	qedr_recov_obj_del(dev, &xrcd->recov_info);

	COMPAT_QEDR_KFREE(xrcd);
	COMPAT_QEDR_DEALLOC_XRCD_RET(0);
}
#endif

void qedr_free_pbl(struct qedr_dev *dev,
		   struct qedr_pbl_info *pbl_info, struct qedr_pbl *pbl)
{
	struct pci_dev *pdev = dev->pdev;
	int i;

	for (i = 0; i < pbl_info->num_pbls; i++) {
		if (!pbl[i].va)
			continue;
		dma_free_coherent(&pdev->dev, pbl_info->pbl_size,
				  pbl[i].va, pbl[i].pa);
	}

	kfree(pbl);
}

#define MIN_FW_PBL_PAGE_SIZE (4 * 1024)
#define MAX_FW_PBL_PAGE_SIZE (64 * 1024)

#define NUM_PBES_ON_PAGE(_page_size) (_page_size / sizeof(u64))
#define MAX_PBES_ON_PAGE NUM_PBES_ON_PAGE(MAX_FW_PBL_PAGE_SIZE)
#define MAX_PBES_TWO_LAYER (MAX_PBES_ON_PAGE * MAX_PBES_ON_PAGE)

static struct qedr_pbl *qedr_alloc_pbl_tbl(struct qedr_dev *dev,
					   struct qedr_pbl_info *pbl_info,
					   gfp_t flags)
{
	struct pci_dev *pdev = dev->pdev;
	struct qedr_pbl *pbl_table;
	dma_addr_t *pbl_main_tbl;
	dma_addr_t pa;
	void *va;
	int i;

	pbl_table = kcalloc(pbl_info->num_pbls, sizeof(*pbl_table), flags);
	if (!pbl_table)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < pbl_info->num_pbls; i++) {
		va = dma_zalloc_coherent(&pdev->dev, pbl_info->pbl_size,
					 &pa, flags);
		if (!va)
			goto err;

		pbl_table[i].va = va;
		pbl_table[i].pa = pa;
	}

	/* Two-Layer PBLs, if we have more than one pbl we need to initialize
	 * the first one with physical pointers to all of the rest
	 */
	pbl_main_tbl = (dma_addr_t *)pbl_table[0].va;
	for (i = 0; i < pbl_info->num_pbls - 1; i++)
		pbl_main_tbl[i] = pbl_table[i + 1].pa;

	return pbl_table;

err:
	for (i--; i >= 0; i--)
		dma_free_coherent(&pdev->dev, pbl_info->pbl_size,
				  pbl_table[i].va, pbl_table[i].pa);

	qedr_free_pbl(dev, pbl_info, pbl_table);

	return ERR_PTR(-ENOMEM);
}

static int qedr_prepare_pbl_tbl(struct qedr_dev *dev,
				struct qedr_pbl_info *pbl_info,
				u32 num_pbes, int two_layer_capable)
{
	u32 pbl_capacity;
	u32 pbl_size;
	u32 num_pbls;

	if ((num_pbes > MAX_PBES_ON_PAGE) && two_layer_capable) {
		if (num_pbes > MAX_PBES_TWO_LAYER) {
			DP_ERR(dev, "prepare pbl table: too many pages %d\n",
			       num_pbes);
			return -EINVAL;
		}

		/* calculate required pbl page size */
		pbl_size = MIN_FW_PBL_PAGE_SIZE;
		pbl_capacity = NUM_PBES_ON_PAGE(pbl_size) *
			       NUM_PBES_ON_PAGE(pbl_size);

		while (pbl_capacity < num_pbes) {
			pbl_size *= 2;
			pbl_capacity = pbl_size / sizeof(u64);
			pbl_capacity = pbl_capacity * pbl_capacity;
		}

		num_pbls = DIV_ROUND_UP(num_pbes, NUM_PBES_ON_PAGE(pbl_size));
		num_pbls++;	/* One for the layer0 ( points to the pbls) */
		pbl_info->two_layered = true;
	} else {
		/* One layered PBL */
		num_pbls = 1;
		pbl_size = max_t(u32, MIN_FW_PBL_PAGE_SIZE,
				 roundup_pow_of_two((num_pbes * sizeof(u64))));
		pbl_info->two_layered = false;
	}

	pbl_info->num_pbls = num_pbls;
	pbl_info->pbl_size = pbl_size;
	pbl_info->num_pbes = num_pbes;

	DP_DEBUG(dev, QEDR_MSG_MR,
		 "prepare pbl table: num_pbes=%d, num_pbls=%d, pbl_size=%d\n",
		 pbl_info->num_pbes, pbl_info->num_pbls, pbl_info->pbl_size);

	return 0;
}

static void qedr_populate_pbls(struct qedr_dev *dev, struct ib_umem *umem,
			       struct qedr_pbl *pbl, u64 page_mask,
			       struct qedr_pbl_info *pbl_info, u32 pg_shift)
{
	int shift, pbe_cnt, total_num_pbes = 0;
	u32 fw_pg_cnt, fw_pg_per_umem_pg;
#ifndef _IB_UMEM_HUGETLB
	struct sg_dma_page_iter sg_iter;
#else
	int pages, entry, pg_cnt;
	struct scatterlist *sg;
#endif
	struct qedr_pbl *pbl_tbl;
	struct regpair *pbe;
	u64 pg_addr;
#ifdef DEFINE_IB_UMEM_WITH_CHUNK /* ! QEDR_UPSTREAM */
	struct ib_umem_chunk *chunk;
#endif

	if (!pbl_info) {
		DP_ERR(dev, "PBL_INFO not initialized\n");
		return;
	}
	if (!pbl_info->num_pbes)
		return;

	/* If we have a two layered pbl, the first pbl points to the rest
	 * of the pbls and the first entry lays on the second pbl in the table
	 */
	if (pbl_info->two_layered)
		pbl_tbl = &pbl[1];
	else
		pbl_tbl = pbl;

	pbe = (struct regpair *)pbl_tbl->va;
	if (!pbe) {
		DP_ERR(dev, "cannot populate PBL due to a NULL PBE\n");
		return;
	}

	pbe_cnt = 0;

#ifdef DEFINE_IB_UMEM_NO_PAGE_PARAM
	fw_pg_per_umem_pg = BIT(PAGE_SHIFT - pg_shift);
	shift = PAGE_SHIFT;
#endif

#ifdef _IB_UMEM_HUGETLB
#if DEFINE_IB_UMEM_PAGE_SHIFT /* QEDR_UPSTREAM */
	shift = umem->page_shift;

	fw_pg_per_umem_pg = BIT(umem->page_shift - pg_shift);
#else
	shift = ilog2(umem->page_size);
	fw_pg_per_umem_pg = umem->page_size / BIT(pg_shift);
#endif

#ifndef DEFINE_IB_UMEM_WITH_CHUNK /* QEDR_UPSTREAM */
	for_each_sg(umem->sg_head.sgl, sg, umem->nmap, entry) {
#else
	list_for_each_entry(chunk, &umem->chunk_list, list) {
		/* get all the dma regions from the chunk. */
		for (entry = 0; entry < chunk->nmap; entry++) {
			sg = &chunk->page_list[entry];
#endif
		pages = sg_dma_len(sg) >> shift;
		pg_addr = sg_dma_address(sg);
		for (pg_cnt = 0; pg_cnt < pages; pg_cnt++) {
			for (fw_pg_cnt = 0; fw_pg_cnt < fw_pg_per_umem_pg;) {
				/* this code supports huge page mode -
				 * assign page address to a pbl element if page
				 * address is aligned with page size or if it is
				 * the first page (which may not be aligned).
				 */
				if (entry + pg_cnt == 0 ||
				    !(pg_addr & ~page_mask)) {
					pg_addr = pg_addr & page_mask;
					pbe->lo = cpu_to_le32(pg_addr);
					pbe->hi = cpu_to_le32(upper_32_bits(
							      pg_addr));
					pbe++;
					pbe_cnt++;
				}

				pg_addr += BIT(pg_shift);
				total_num_pbes++;
				if (total_num_pbes == pbl_info->num_pbes)
					return;

				/* If the given pbl is full storing the pbes,
				 * move to next pbl.
				 */
				if (pbe_cnt ==
				    (pbl_info->pbl_size / sizeof(u64))) {
					pbl_tbl++;
					pbe = (struct regpair *)pbl_tbl->va;
					pbe_cnt = 0;
				}

				fw_pg_cnt++;
			}
		}
#ifdef DEFINE_IB_UMEM_WITH_CHUNK /* ! QEDR_UPSTREAM */
		}
#endif
	}
#else
#ifdef DEFINE_UMEM_WITH_SG_HEAD
	for_each_sg_dma_page(umem->sg_head.sgl, &sg_iter, umem->nmap, 0) {
#else
	for_each_sg_dma_page(umem->sgt_append.sgt.sgl, &sg_iter,
			     umem->sgt_append.sgt.nents, 0) {
#endif
		pg_addr = sg_page_iter_dma_address(&sg_iter);
		for (fw_pg_cnt = 0; fw_pg_cnt < fw_pg_per_umem_pg;) {
			pbe->lo = cpu_to_le32(pg_addr);
			pbe->hi = cpu_to_le32(upper_32_bits(pg_addr));

			pg_addr += BIT(pg_shift);
			pbe_cnt++;
			total_num_pbes++;
			pbe++;

			if (total_num_pbes == pbl_info->num_pbes)
				return;

			/* If the given pbl is full storing the pbes,
			 * move to next pbl.
			 */
			if (pbe_cnt == (pbl_info->pbl_size / sizeof(u64))) {
				pbl_tbl++;
				pbe = (struct regpair *)pbl_tbl->va;
				pbe_cnt = 0;
			}

			fw_pg_cnt++;
		}
	}
#endif
}

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
int qedr_db_recovery_add(struct qedr_dev *dev,
			 void __iomem *db_addr,
			 void *db_data,
			 enum qed_db_rec_width db_width,
			 enum qed_db_rec_space db_space)
{
	if (!db_data) {
		DP_VERBOSE(dev, QEDR_MSG_FAIL,
			   "avoiding db rec since old lib\n");

		return 0;
	}

	return dev->ops->common->db_recovery_add(dev->cdev, db_addr, db_data,
						 db_width, db_space);
}

int qedr_db_recovery_del(struct qedr_dev *dev,
			 void __iomem *db_addr, void *db_data,
			 bool obj_reset)
{
	if (obj_reset) {
		DP_VERBOSE(dev, QEDR_MSG_FAIL,
			   "avoiding db rec delete device was reset\n");

		return 0;
	}

	if (!db_data) {
		DP_VERBOSE(dev, QEDR_MSG_FAIL,
			   "avoiding db rec since old lib\n");

		return 0;
	}

	/* A null address indicates the add wasn't invoked to begin-with. So no
	 * need to invoke the del. This likely  due to assymetric error flow.
	 */
	if (!db_addr)
		return 0;

	return dev->ops->common->db_recovery_del(dev->cdev, db_addr, db_data);
}

#endif
static int qedr_copy_cq_uresp(struct qedr_dev *dev, struct qedr_cq *cq,
			      struct ib_udata *udata, u32 db_offset)
{
	struct qedr_create_cq_uresp uresp;
	int rc;

	memset(&uresp, 0, sizeof(uresp));

	uresp.db_offset = db_offset;
	uresp.icid = cq->icid;

	rc = qedr_copy_to_udata(udata, &uresp, sizeof(uresp));
	if (rc)
		DP_ERR(dev, "copy error cqid=0x%x.\n", cq->icid);

	return rc;
}

void consume_cqe(struct qedr_cq *cq)
{
	if (cq->latest_cqe == cq->toggle_cqe)
		cq->pbl_toggle ^= RDMA_CQE_REQUESTER_TOGGLE_BIT_MASK;

	cq->latest_cqe = qed_chain_consume(&cq->pbl);
}

static inline int qedr_align_cq_entries(int entries)
{
	u64 size, aligned_size;

	/* We allocate an extra entry that we don't report to the FW. */
	/* INTERNAL: Why?
	 * The CQE size is 32 bytes but the FW writes in chunks of 64 bytes
	 * (for performance purposes). Allocating an extra entry and telling
	 * the FW we have less prevents overwriting the first entry in case of
	 * a wrap i.e. when the FW writes the last entry and the application
	 * hasn't read the first one.
	 */
	size = (entries + 1) * QEDR_CQE_SIZE;
	/* INTERNAL: We align to PAGE_SIZE.
	 * Why?
	 * Since the CQ is going to be mapped and the mapping is anyhow in whole
	 * kernel pages we benefit from the possibly extra CQEs.
	 */
	aligned_size = ALIGN(size, PAGE_SIZE);
	/* INTERNAL: for CQs created in user space the result of this function
	 * should match the size mapped in user space
	 */
	return aligned_size / QEDR_CQE_SIZE;
}

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
static int qedr_init_user_db_rec(struct ib_ucontext *ib_ctx,
				 struct ib_udata *udata,
				 struct qedr_dev *dev, struct qedr_userq *q,
				 u64 db_rec_addr, int access, int dmasync)
{
#ifdef DEFINE_IB_UMEM_WITH_CHUNK
	struct ib_umem_chunk *chunk;
#endif
	struct scatterlist *sg;

	if (db_rec_addr == 0)
		return 0;
	q->db_rec_addr = db_rec_addr;
	q->db_rec_umem = compat_ib_umem_get(ib_ctx, udata, &dev->ibdev,
					    q->db_rec_addr, PAGE_SIZE,
					    access, dmasync);
#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
	if (IS_ERR(q->db_rec_umem)) {
		/* Kernel bug workaround: the CQ user data passed by uverbs may
		 * contain junk due to a kernel bug so db_rec_addr may have a
		 * bogus value. If we fail the allocation that is probably the
		 * case so we continue the operation just without doorbell
		 * recovery.
		 */
		q->db_rec_umem = NULL;
		DP_VERBOSE(dev, QEDR_MSG_FAIL,
			   "create user queue: failed db_rec ib_umem_get, error was %ld, db_rec_addr was %llx\n",
			   PTR_ERR(q->db_rec_umem), db_rec_addr);
		return 0;
	}
#else
	if (IS_ERR(q->db_rec_umem)) {
		DP_ERR(dev,
		       "create user queue: failed db_rec ib_umem_get, error was %ld, db_rec_addr was %llx\n",
		       PTR_ERR(q->db_rec_umem), db_rec_addr);
		return PTR_ERR(q->db_rec_umem);
	}
#endif

#ifdef DEFINE_IB_UMEM_WITH_CHUNK
	chunk = container_of((&q->db_rec_umem->chunk_list)->next,
		typeof(*chunk), list);

	sg = &chunk->page_list[0];
#else
#ifdef DEFINE_UMEM_WITH_SG_HEAD
	sg = q->db_rec_umem->sg_head.sgl;
#else
	sg = q->db_rec_umem->sgt_append.sgt.sgl;
#endif
#endif

	q->db_rec_virt = sg_virt(sg);

	return 0;
}
#endif

static int qedr_init_user_queue(struct ib_ucontext *ib_ctx,
				struct ib_udata *udata,
				struct qedr_dev *dev, struct qedr_userq *q,
				u64 buf_addr, size_t buf_len, u64 db_rec_addr,
				int access, int dmasync, int alloc_and_init)
{
	u32 fw_pages;
	int rc;

	q->buf_addr = buf_addr;
	q->buf_len = buf_len;
	q->umem = compat_ib_umem_get(ib_ctx, udata, &dev->ibdev, q->buf_addr,
				     q->buf_len, access, dmasync);
	if (IS_ERR(q->umem)) {
		DP_ERR(dev, "create user queue: failed ib_umem_get, got %ld\n",
		       PTR_ERR(q->umem));
		return PTR_ERR(q->umem);
	}

	fw_pages = COMPAT_IB_UMEM_COUNT(q->umem);

	rc = qedr_prepare_pbl_tbl(dev, &q->pbl_info, fw_pages, 0);
	if (rc)
		goto err0;

	if (alloc_and_init) {
		q->pbl_tbl = qedr_alloc_pbl_tbl(dev, &q->pbl_info, GFP_KERNEL);
		if (IS_ERR_OR_NULL(q->pbl_tbl)) {
			rc = PTR_ERR(q->pbl_tbl);
			goto err0;
		}
		qedr_populate_pbls(dev, q->umem, q->pbl_tbl, PAGE_MASK,
				   &q->pbl_info, FW_PAGE_SHIFT);
	} else {
		q->pbl_tbl = kzalloc(sizeof(*q->pbl_tbl), GFP_KERNEL);
		if (!q->pbl_tbl) {
			rc = -ENOMEM;
			goto err0;
		}
	}
#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
	/* mmap the user address used to store doorbell data for recovery */
	rc = qedr_init_user_db_rec(ib_ctx, udata, dev, q, db_rec_addr, access,
				   dmasync);
	if (rc)
		goto err1;
#endif

	return rc;

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
err1:
	if (alloc_and_init)
		qedr_free_pbl(dev, &q->pbl_info, q->pbl_tbl);
	else
		kfree(q->pbl_tbl);
#endif
err0:
	ib_umem_release(q->umem);
	q->umem = NULL;

	return rc;
}

static inline void qedr_init_cq_params(struct qedr_cq *cq,
				       struct qedr_ucontext *ctx,
				       struct qedr_dev *dev, int vector,
				       int chain_entries, int page_cnt,
				       u64 pbl_ptr,
				       struct qed_rdma_create_cq_in_params
				       *params)
{
	memset(params, 0, sizeof(*params));
	params->cq_handle_hi = upper_32_bits((uintptr_t)cq);
	params->cq_handle_lo = lower_32_bits((uintptr_t)cq);
	params->cnq_id = vector;
	/* INTERNAL: see qedr_align_cq_entries for an explanation on the '-1' */
	params->cq_size = chain_entries - 1;
	params->dpi = (ctx) ? ctx->dpi : dev->dpi;
	params->pbl_num_pages = page_cnt;
	params->pbl_ptr = pbl_ptr;
	params->pbl_two_level = 0;
}

static void doorbell_cq(struct qedr_cq *cq, u32 cons, u8 flags)
{
	/* Flush data before signalling doorbell */
	wmb();
	cq->db.data.agg_flags = flags;
	cq->db.data.value = cpu_to_le32(cons);
#ifdef __LP64__ /* QEDR_UPSTREAM */
	writeq(cq->db.raw, cq->db_addr);
#else
	/* Note that since the FW allows 64 bit write only, in 32bit systems
	 * the value of db_addr must be low enough. This is currently not
	 * enforced.
	 */
	writel(cq->db.raw & 0xffffffff, cq->db_addr);
#endif
	/* Make sure write would stick */
	mmiowb();
}

int qedr_arm_cq(struct ib_cq *ibcq, enum ib_cq_notify_flags flags)
{
	struct qedr_cq *cq = get_qedr_cq(ibcq);
	unsigned long sflags;
	struct qedr_dev *dev;

	dev = get_qedr_dev(ibcq->device);

#ifndef QEDR_UPSTREAM_RECOVER /* !QEDR_UPSTREAM_RECOVER */
	if (cq->recov_info.reset && !cq->reopened) {
		if ((cq->cq_type != QEDR_CQ_TYPE_GSI)) {
			cq->arm_cq = true;
			cq->arm_cq_flags = flags;

			DP_DEBUG(dev, QEDR_MSG_CQ, "Re-ARM the CQ\n");
		}

		return 0;
	}
#endif
	if (cq->destroyed) {
		DP_ERR(dev,
		       "warning: arm was invoked after destroy for cq %p (icid=%d)\n",
		       cq, cq->icid);
		return -EINVAL;
	}

	FP_DP_VERBOSE(get_qedr_dev(ibcq->device), QEDR_MSG_CQ,
		      "Arm CQ cons=%x cq=%p\n", qed_chain_get_cons_idx_u32(&cq->pbl) - 1, cq);

	/* INTERNAL: TODO: figure out what to do here... */
	if (cq->cq_type == QEDR_CQ_TYPE_GSI)
		return 0;

	spin_lock_irqsave(&cq->cq_lock, sflags);

	cq->arm_flags = 0;

	if (flags & IB_CQ_SOLICITED)
		cq->arm_flags |= DQ_UCM_ROCE_CQ_ARM_SE_CF_CMD;

	if (flags & IB_CQ_NEXT_COMP)
		cq->arm_flags |= DQ_UCM_ROCE_CQ_ARM_CF_CMD;

	doorbell_cq(cq, qed_chain_get_cons_idx_u32(&cq->pbl) - 1,
		    cq->arm_flags);

	spin_unlock_irqrestore(&cq->cq_lock, sflags);

	return 0;
}

#ifdef DEFINE_CREATE_CQ_ATTR  /* QEDR_UPSTREAM */
COMPAT_CREATE_CQ_DECLARE_RET
qedr_create_cq(COMPAT_CREATE_CQ_IBDEV(struct ib_device *ibdev)
	       COMPAT_CREATE_CQ_CQ(struct ib_cq *ibcq)
	       const struct ib_cq_init_attr *attr,
	       COMPAT_CREATE_CQ_CTX(struct ib_ucontext *ib_ctx)
	       struct ib_udata *udata)
#else
struct ib_cq *qedr_create_cq(struct ib_device *ibdev, int entries, int vector,
			     struct ib_ucontext *ib_ctx,
			     struct ib_udata *udata)
#endif
{
#ifdef _HAS_CQ_ALLOCATION
	struct ib_device *ibdev = ibcq->device;
#endif
	struct qedr_ucontext *ctx;
	struct qed_rdma_destroy_cq_out_params destroy_oparams;
	struct qed_rdma_destroy_cq_in_params destroy_iparams;
	struct qedr_dev *dev = get_qedr_dev(ibdev);
	struct qed_rdma_create_cq_in_params *params;
	struct qed_chain_params chain_params;
	struct qedr_create_cq_ureq ureq;
#ifdef DEFINE_CREATE_CQ_ATTR  /* QEDR_UPSTREAM */
	int vector = attr->comp_vector;
	int entries = attr->cqe;
#endif
	struct qedr_cq *cq;
	int chain_entries;
	u32 db_offset;
	int page_cnt;
	u64 pbl_ptr;
	u16 icid;
	int rc;

	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
		return COMPAT_CREATE_CQ_ERR(rc);

	DP_DEBUG(dev, QEDR_MSG_INIT,
		 "create_cq: called from %s. entries=%d, vector=%d\n",
		 udata ? "User Lib" : "Kernel", entries, vector);

	if (entries > QEDR_MAX_CQES) {
		DP_ERR(dev,
		       "create cq: the number of entries %d is too high. Must be equal or below %d.\n",
		       entries, QEDR_MAX_CQES);
		return COMPAT_CREATE_CQ_ERR(-EINVAL);
	}

	ctx = GET_DRV_CXT(ib_ctx);
	chain_entries = qedr_align_cq_entries(entries);
	chain_entries = min_t(int, chain_entries, QEDR_MAX_CQES);

#ifdef _HAS_CQ_ALLOCATION
	cq = get_qedr_cq(ibcq);
#else
	cq = kzalloc(sizeof(*cq), GFP_KERNEL);
	if (!cq)
		return ERR_PTR(-ENOMEM);
#endif
	params = &cq->params;

	cq->recov_info.recov_cb = qedr_recov_cq;

	/* INTERNAL: calc the db offset. user will add DPI base, kernel will add db addr */
	db_offset = DB_ADDR_SHIFT(DQ_PWM_OFFSET_UCM_RDMA_CQ_CONS_32BIT);

	if (udata) {
		memset(&ureq, 0, sizeof(ureq));
		if (qedr_copy_from_udata(&ureq, udata, sizeof(ureq))) {
			DP_ERR(dev,
			       "create cq: problem copying data from user space\n");
			goto err0;
		}

		if (!ureq.len) {
			DP_ERR(dev,
			       "create cq: cannot create a cq with 0 entries\n");
			goto err0;
		}

		cq->cq_type = QEDR_CQ_TYPE_USER;

		rc = qedr_init_user_queue(&ctx->ibucontext, udata, dev, &cq->q,
					  ureq.addr, ureq.len, ureq.db_rec_addr,
					  IB_ACCESS_LOCAL_WRITE, 1, 1);
		if (rc)
			goto err0;

		pbl_ptr = cq->q.pbl_tbl->pa;
		page_cnt = cq->q.pbl_info.num_pbes;

		cq->ibcq.cqe = chain_entries;
#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
		cq->q.db_addr = (void __iomem *)(uintptr_t) ctx->dpi_addr +
				db_offset;
#endif
	} else {
		cq->cq_type = QEDR_CQ_TYPE_KERNEL;

		dev->ops->common->chain_params_init(&chain_params,
						    QED_CHAIN_USE_TO_CONSUME,
						    QED_CHAIN_MODE_PBL,
						    QED_CHAIN_CNT_TYPE_U32,
						    chain_entries,
						    sizeof(union rdma_cqe));
		rc = dev->ops->common->chain_alloc(dev->cdev, &cq->pbl,
						   &chain_params);
		if (rc)
			goto err1;

		page_cnt = qed_chain_get_page_cnt(&cq->pbl);
		pbl_ptr = qed_chain_get_pbl_phys(&cq->pbl);
		cq->ibcq.cqe = cq->pbl.capacity;
	}

	qedr_init_cq_params(cq, ctx, dev, vector, chain_entries, page_cnt,
			    pbl_ptr, params);

	rc = dev->ops->rdma_create_cq(dev->rdma_ctx, params, &icid);
	if (rc)
		goto err2;

	cq->icid = icid;
	cq->sig = QEDR_CQ_MAGIC_NUMBER;
	spin_lock_init(&cq->cq_lock);
	INIT_LIST_HEAD(&cq->sq_qp_list);
	INIT_LIST_HEAD(&cq->rq_qp_list);

	if (udata) {
		rc = qedr_copy_cq_uresp(dev, cq, udata, db_offset);
		if (rc)
			goto err3;

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
		/* add entry to doorbell drop recovery mechanism */
		rc = qedr_db_recovery_add(dev, cq->q.db_addr,
					  &cq->q.db_rec_virt->db_data,
					  DB_REC_WIDTH_64B, DB_REC_USER);

		if (rc)
			goto err3;
#endif

	} else {
		/* Generate doorbell address. */
		/* INTERNAL: Configure bits 3-9 with DQ_PWM_OFFSET_UCM_RDMA_CQ_CONS_32BIT. */
		/* TODO: consider moving to device scope as it is a function of
		 * the device.
		 */
		/* TODO: add ifdef if plan to support 16 bit.  */
		cq->db_addr = (void *)(uintptr_t)dev->db_addr + db_offset;
		cq->db.data.icid = cpu_to_le16(cq->icid);
		cq->db.data.params = DB_AGG_CMD_MAX <<
		    RDMA_PWM_VAL32_DATA_AGG_CMD_SHIFT;

		/* point to the very last element, passing it we will toggle */
		cq->toggle_cqe = qed_chain_get_last_elem(&cq->pbl);
		cq->pbl_toggle = RDMA_CQE_REQUESTER_TOGGLE_BIT_MASK;

		/* INTERNAL: Latest CQE must be different from pbl_toggle */
		cq->latest_cqe = NULL;

		consume_cqe(cq);

		/* INTERNAL: add entry to doorbell drop recovery mechanism */
#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
		rc = qedr_db_recovery_add(dev, cq->db_addr, &cq->db.data,
					  DB_REC_WIDTH_64B, DB_REC_KERNEL);
		if (rc)
			goto err3;
#endif
	}

	DP_DEBUG(dev, QEDR_MSG_CQ,
		 "create cq: icid=0x%0x, addr=%p, size(entries)=0x%0x\n",
		 cq->icid, cq, params->cq_size);

	qedr_recov_obj_add(dev, &cq->recov_info);

	COMPAT_HAS_CQ_RET(return rc);
	COMPAT_NO_CQ_RET(return &cq->ibcq);

err3:
	destroy_iparams.icid = cq->icid;
	dev->ops->rdma_destroy_cq(dev->rdma_ctx, &destroy_iparams,
				  &destroy_oparams);
err2:
	if (udata)
		qedr_free_pbl(dev, &cq->q.pbl_info, cq->q.pbl_tbl);
	else
		dev->ops->common->chain_free(dev->cdev, &cq->pbl);
err1:
	if (udata) { /* QEDR_UPSTREAM_REMOVE_COLON */
		/* INTERNAL: release user chain */
		ib_umem_release(cq->q.umem);

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
		/* release user doorbell recovery info */
		if (!IS_ERR(cq->q.db_rec_umem))
			ib_umem_release(cq->q.db_rec_umem);
#endif
	}

err0:
#ifndef _HAS_CQ_ALLOCATION
	kfree(cq);
#endif
	return COMPAT_CREATE_CQ_ERR(-EINVAL);
}

int qedr_resize_cq(struct ib_cq *ibcq, int new_cnt, struct ib_udata *udata)
{
	struct qedr_dev *dev = get_qedr_dev(ibcq->device);
	struct qedr_cq *cq = get_qedr_cq(ibcq);

#ifndef QEDR_UPSTREAM_RECOVER /* !QEDR_UPSTREAM_RECOVER */
	if (cq->recov_info.reset) {
		DP_VERBOSE(dev, QEDR_MSG_FAIL,
			   "failed qedr_resize_cq, cq was reset\n");

		return -EPERM;
	}
#endif
	DP_ERR(dev, "cq %p RESIZE NOT SUPPORTED\n", cq);

	return 0;
}

#define QEDR_DESTROY_CQ_MAX_ITERATIONS		(10)
#define QEDR_DESTROY_CQ_ITER_DURATION		(10)
COMPAT_DESTROY_CQ_DECLARE_RET
qedr_destroy_cq(struct ib_cq *ibcq
		COMPAT_DESTROY_CQ_UDATA(struct ib_udata *udata))
{
	struct qedr_dev *dev = get_qedr_dev(ibcq->device);
	struct qed_rdma_destroy_cq_out_params oparams;
	struct qed_rdma_destroy_cq_in_params iparams;
	struct qedr_cq *cq = get_qedr_cq(ibcq);
	int iter;
	int rc;

	if (dev->recov_info.recov_in_prog)
		wait_for_completion(&dev->recov_info.recov_comp);

	cq->destroyed = 1;

	/* GSIs CQs are handled by driver, so they don't exist in the FW */

	if (cq->cq_type == QEDR_CQ_TYPE_GSI) { /* QEDR_UPSTREAM_REMOVE_COLON */
#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
		rc = qedr_db_recovery_del(dev, cq->db_addr, &cq->db.data,
					  cq->recov_info.reset);
		if (rc)
			COMPAT_DESTROY_CQ_RET(return rc);
#endif
		goto done;
	} /* QEDR_UPSTREAM_REMOVE_COLON */

	iparams.icid = cq->icid;
	if (!cq->recov_info.reset) {
		rc = dev->ops->rdma_destroy_cq(dev->rdma_ctx, &iparams,
					       &oparams);
		if (rc)
			COMPAT_DESTROY_CQ_RET(return rc);
	}

	dev->ops->common->chain_free(dev->cdev, &cq->pbl);

	if (ibcq->uobject) {
		/* INTERNAL: release chain information */
		qedr_free_pbl(dev, &cq->q.pbl_info, cq->q.pbl_tbl);
		ib_umem_release(cq->q.umem);
#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
		if (!IS_ERR(cq->q.db_rec_umem)) { /* QEDR_UPSTREAM_REMOVE_COLON */
			/* release db_rec memory and remove entry from doorbell
			 * drop recovery mechanism
			 */
			rc = qedr_db_recovery_del(dev, cq->q.db_addr,
						  &cq->q.db_rec_virt->db_data,
						  cq->recov_info.reset);
			if (rc)
				COMPAT_DESTROY_CQ_RET(return rc);
			if (cq->q.db_rec_umem)
				ib_umem_release(cq->q.db_rec_umem);
		} /* QEDR_UPSTREAM_REMOVE_COLON */
	} else {
		/* delete kernel doorbell info from doorbell recovery mechanism */
		rc = qedr_db_recovery_del(dev, cq->db_addr, &cq->db.data,
					  cq->recov_info.reset);
		if (rc)
			COMPAT_DESTROY_CQ_RET(return rc);
#endif
	}

	if (cq->recov_info.reset)
		goto done;

	/* We don't want the IRQ handler to handle a non-existing CQ so we
	 * wait until all CNQ interrupts, if any, are received. This will always
	 * happen and will always happen very fast. If not, then a serious error
	 * has occured. That is why we can use a long delay.
	 * We spin for a short time so we don’t lose time on context switching
	 * in case all the completions are handled in that span. Otherwise
	 * we sleep for a while and check again. Since the CNQ may be
	 * associated with (only) the current CPU we use msleep to allow the
	 * current CPU to be freed.
	 * The CNQ notification is increased in qedr_irq_handler().
	 */
	iter = QEDR_DESTROY_CQ_MAX_ITERATIONS;
	while (oparams.num_cq_notif != READ_ONCE(cq->cnq_notif) && iter) {
		udelay(QEDR_DESTROY_CQ_ITER_DURATION);
		iter--;
	}

	iter = QEDR_DESTROY_CQ_MAX_ITERATIONS;
	while (oparams.num_cq_notif != READ_ONCE(cq->cnq_notif) && iter) {
		msleep(QEDR_DESTROY_CQ_ITER_DURATION);
		iter--;
	}

	if (oparams.num_cq_notif != cq->cnq_notif)
		goto err;

	/* Note that we don't need to have explicit code to wait for the
	 * completion of the event handler because it is invoked from the EQ.
	 * Since the destroy CQ ramrod has also been received on the EQ we can
	 * be certain that there's no event handler in process.
	 */
done:
	cq->sig = ~cq->sig;

	qedr_recov_obj_del(dev, &cq->recov_info);

#ifndef _HAS_CQ_ALLOCATION
	kfree(cq);
#endif

	COMPAT_DESTROY_CQ_RET(return 0);

err:
	DP_ERR(dev,
	       "CQ %p (icid=%d) not freed, expecting %d ints but got %d ints\n",
	       cq, cq->icid, oparams.num_cq_notif, cq->cnq_notif);

	COMPAT_DESTROY_CQ_RET(return -EINVAL);
}

#if DEFINE_ROCE_GID_TABLE /* QEDR_UPSTREAM */
static inline int get_gid_info_from_table(struct ib_qp *ibqp,
					  struct ib_qp_attr *attr,
					  int attr_mask,
					  struct qed_rdma_modify_qp_in_params
					  *qp_params)
{
	int rc = 0;
	int i;
	SGID_CONST union ib_gid *gid;
#if DEFINE_ROCE_V2_SUPPORT /* QEDR_UPSTREAM */
	enum rdma_network_type nw_type;
	u32 ipv4_addr;
#endif
	const struct ib_global_route *grh = rdma_ah_read_grh(&attr->ah_attr);
#ifdef DEFINED_SGID_ATTR /* QEDR UPSTREAM */
	const struct ib_gid_attr *gid_attr = grh->sgid_attr;

	gid = &gid_attr->gid;
#else
	struct ib_gid_attr tmp_gid_attr;
	struct ib_gid_attr *gid_attr = &tmp_gid_attr;
	union ib_gid tmp_gid;

	gid = &tmp_gid;
#endif

#ifndef DEFINED_SGID_ATTR /* !QEDR UPSTREAM */
	rc = ib_get_cached_gid(ibqp->device,
			       rdma_ah_get_port_num(&attr->ah_attr),
			       grh->sgid_index, gid, gid_attr);
	if (rc)
		return rc;

	if (!memcmp(gid, &zgid, sizeof(*gid)))
		return -ENOENT;
	if (gid_attr->ndev)
		dev_put(gid_attr->ndev);
#endif
	if (gid_attr->ndev) {
		rc = rdma_read_gid_l2_fields(gid_attr, &qp_params->vlan_id,
					    qp_params->local_mac_addr);
		if (rc)
			return rc;
#if DEFINE_ROCE_V2_SUPPORT /* QEDR_UPSTREAM */
#ifdef DEFINED_SGID_ATTR /* QEDR UPSTREAM */
		nw_type = rdma_gid_attr_network_type(gid_attr);
#else
		nw_type = ib_gid_to_network_type(gid_attr->gid_type, gid);
#endif
		switch (nw_type) {
		case RDMA_NETWORK_IPV6:
			memcpy(&qp_params->sgid.bytes[0], &gid->raw[0],
			       sizeof(qp_params->sgid));
			memcpy(&qp_params->dgid.bytes[0],
			       &grh->dgid,
			       sizeof(qp_params->dgid));
			qp_params->roce_mode = ROCE_V2_IPV6;
			SET_FIELD(qp_params->modify_flags,
				  QED_ROCE_MODIFY_QP_VALID_ROCE_MODE, 1);
			break;
		case RDMA_NETWORK_ROCE_V1:
			memcpy(&qp_params->sgid.bytes[0], &gid->raw[0],
			       sizeof(qp_params->sgid));
			memcpy(&qp_params->dgid.bytes[0],
			       &grh->dgid,
			       sizeof(qp_params->dgid));
			qp_params->roce_mode = ROCE_V1;
			break;
		case RDMA_NETWORK_IPV4:
			memset(&qp_params->sgid, 0, sizeof(qp_params->sgid));
			memset(&qp_params->dgid, 0, sizeof(qp_params->dgid));
			ipv4_addr = qedr_get_ipv4_from_gid(gid->raw);
			qp_params->sgid.ipv4_addr = ipv4_addr;
			ipv4_addr =
			    qedr_get_ipv4_from_gid(grh->dgid.raw);
			qp_params->dgid.ipv4_addr = ipv4_addr;
			SET_FIELD(qp_params->modify_flags,
				  QED_ROCE_MODIFY_QP_VALID_ROCE_MODE, 1);
			qp_params->roce_mode = ROCE_V2_IPV4;
			break;
		default:
			return -EINVAL;
		}
#else
		memcpy(&qp_params->sgid.bytes[0], &gid->raw[0],
		       sizeof(qp_params->sgid));
		memcpy(&qp_params->dgid.bytes[0],
		       &grh->dgid,
		       sizeof(qp_params->dgid));
		qp_params->roce_mode = ROCE_V1;
#endif
	}

	/* INTERNAL: qed expects words to be in cpu format */
	for (i = 0; i < 4; i++) {
		qp_params->sgid.dwords[i] = ntohl(qp_params->sgid.dwords[i]);
		qp_params->dgid.dwords[i] = ntohl(qp_params->dgid.dwords[i]);
	}

	if (qp_params->vlan_id >= VLAN_CFI_MASK)
		qp_params->vlan_id = 0;

	return 0;
}
#else /* DEFINE_ROCE_GID_TABLE */
#ifdef DEFINE_NO_IP_BASED_GIDS
static inline bool qedr_get_vlan_id_qp(struct ib_qp_attr *attr, int attr_mask,
				       u16 *vlan_id)
{
	u16 tmp_vlan_id;
	union ib_gid *dgid;

	dgid = &attr->ah_attr.grh.dgid;
	tmp_vlan_id = (dgid->raw[11] << 8) | dgid->raw[12];

	if (tmp_vlan_id < VLAN_CFI_MASK) {
		*vlan_id = tmp_vlan_id;
		return true;
	} else {
		*vlan_id = 0;
		return false;
	}
}
#else
static inline bool qedr_get_vlan_id_qp(struct ib_qp_attr *attr, int attr_mask,
				       u16 *vlan_id)
{
	u16 tmp_vlan_id;

	tmp_vlan_id = attr->vlan_id;
	if ((attr_mask & IB_QP_VID) && (tmp_vlan_id < VLAN_CFI_MASK)) {
		*vlan_id = tmp_vlan_id;
		return true;
	} else {
		*vlan_id = 0;
		return false;
	}
}
#endif
static inline void get_gid_info(struct ib_qp *ibqp, struct ib_qp_attr *attr,
				int attr_mask,
				struct qedr_dev *dev,
				struct qedr_qp *qp,
				struct qed_rdma_modify_qp_in_params *qp_params)
{
	int i;

	memcpy(&qp_params->sgid.bytes[0],
	       &dev->sgid_tbl[qp->sgid_idx].raw[0],
	       sizeof(qp_params->sgid.bytes));
	memcpy(&qp_params->dgid.bytes[0],
	       &attr->ah_attr.grh.dgid.raw[0],
	       sizeof(qp_params->dgid));

	qedr_get_vlan_id_qp(attr, attr_mask, &qp_params->vlan_id);

	/* qed expects words to be in cpu format */
	for (i = 0; i < array_size(qp_params->sgid.dwords); i++) {
		qp_params->sgid.dwords[i] = ntohl(qp_params->sgid.dwords[i]);
		qp_params->dgid.dwords[i] = ntohl(qp_params->dgid.dwords[i]);
	}

	if (qp_params->vlan_id >= VLAN_CFI_MASK)
		qp_params->vlan_id = 0;
}
#endif

static int qedr_check_qp_attrs(struct ib_pd *ibpd, struct qedr_dev *dev,
			       struct ib_qp_init_attr *attrs,
			       struct ib_udata *udata)
{
	struct qedr_device_attr *qattr = &dev->attr;

	if ((attrs->qp_type == IB_QPT_GSI) && udata) {
		DP_ERR(dev,
		       "create qp: unexpected udata when creating GSI QP\n");
		return -EINVAL;
	}

	if (udata && !(ibpd->uobject && ibpd->uobject->context)) {
		DP_ERR(dev, "create qp: called from user without context\n");
		return -EINVAL;
	}

	/* QP0... attrs->qp_type == IB_QPT_GSI */
	if (attrs->qp_type != IB_QPT_RC &&
	    attrs->qp_type != IB_QPT_GSI &&
	    attrs->qp_type != IB_QPT_XRC_INI &&
	    attrs->qp_type != IB_QPT_XRC_TGT) {
		DP_DEBUG(dev, QEDR_MSG_QP,
			 "create qp: unsupported qp type=0x%x requested\n",
			 attrs->qp_type);
		return -EINVAL;
	}

	if (attrs->qp_type == IB_QPT_GSI && attrs->srq) {
		DP_ERR(dev, "create qp: cannot create GSI qp with SRQ\n");
		return -EINVAL;
	}

	if (attrs->cap.max_send_wr > qattr->max_sqe) {
		DP_ERR(dev,
		       "create qp: cannot create a SQ with %d elements (max_send_wr=0x%x)\n",
		       attrs->cap.max_send_wr, qattr->max_sqe);
		return -EINVAL;
	}

	if (!attrs->srq && (attrs->cap.max_recv_wr > qattr->max_rqe)) {
		DP_ERR(dev,
		       "create qp: cannot create a RQ with %d elements (max_recv_wr=0x%x)\n",
		       attrs->cap.max_recv_wr, qattr->max_rqe);
		return -EINVAL;
	}

	if (attrs->cap.max_inline_data > qattr->max_inline) {
		DP_ERR(dev,
		       "create qp: unsupported inline data size=0x%x requested (max_inline=0x%x)\n",
		       attrs->cap.max_inline_data, qattr->max_inline);
		return -EINVAL;
	}

	if (attrs->cap.max_send_sge > qattr->max_sge) {
		DP_ERR(dev,
		       "create qp: unsupported send_sge=0x%x requested (max_send_sge=0x%x)\n",
		       attrs->cap.max_send_sge, qattr->max_sge);
		return -EINVAL;
	}

	if (attrs->cap.max_recv_sge > qattr->max_sge) {
		DP_ERR(dev,
		       "create qp: unsupported recv_sge=0x%x requested (max_recv_sge=0x%x)\n",
		       attrs->cap.max_recv_sge, qattr->max_sge);
		return -EINVAL;
	}

	/* Unprivileged user space cannot create special QP */
	if (attrs->qp_type == IB_QPT_GSI && ibpd->uobject) {
		DP_ERR(dev,
		       "create qp: userspace can't create special QPs of type=0x%x\n",
		       attrs->qp_type);
		return -EINVAL;
	}

	/* allow creating only one GSI type of QP */
	if (attrs->qp_type == IB_QPT_GSI && dev->gsi_qp_created) {
		DP_ERR(dev, "create qp: GSI special QPs already created.\n");
		return -EINVAL;
	}

	/* verify consumer QPs are not trying to use GSI QP's CQ.
	 * TGT QP isn't associated with RQ/SQ
	 */
	if ((attrs->qp_type != IB_QPT_GSI) && (dev->gsi_qp_created) &&
	    (attrs->qp_type != IB_QPT_XRC_TGT) &&
	    (attrs->qp_type != IB_QPT_XRC_INI)) {
		struct qedr_cq *send_cq = get_qedr_cq(attrs->send_cq);
		struct qedr_cq *recv_cq = get_qedr_cq(attrs->recv_cq);

		if ((send_cq->cq_type == QEDR_CQ_TYPE_GSI) ||
		    (recv_cq->cq_type == QEDR_CQ_TYPE_GSI)) {
			DP_ERR(dev,
			       "create qp: consumer QP cannot use GSI CQs.\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int qedr_copy_srq_uresp(struct qedr_dev *dev,
			       struct qedr_srq *srq, struct ib_udata *udata)
{
	struct qedr_create_srq_uresp uresp;
	int rc;

	memset(&uresp, 0, sizeof(uresp));

	uresp.srq_id = srq->srq_id;

	rc = qedr_copy_to_udata(udata, &uresp, sizeof(uresp));
	if (rc)
		DP_ERR(dev, "create srq: problem copying data to user space\n");

	return rc;
}

static void qedr_copy_rq_uresp(struct qedr_dev *dev,
			       struct qedr_create_qp_uresp *uresp,
			       struct qedr_qp *qp)
{
	/* iWARP requires two doorbells per RQ. */
	if (rdma_protocol_iwarp(&dev->ibdev, 1)) {
		uresp->rq_db_offset =
		    DB_ADDR_SHIFT(DQ_PWM_OFFSET_TCM_IWARP_RQ_PROD);
		uresp->rq_db2_offset = DB_ADDR_SHIFT(DQ_PWM_OFFSET_TCM_FLAGS);
	} else {
		uresp->rq_db_offset =
		    DB_ADDR_SHIFT(DQ_PWM_OFFSET_TCM_ROCE_RQ_PROD);
	}

	uresp->rq_icid = qp->icid;
}

static void qedr_copy_sq_uresp(struct qedr_dev *dev,
			       struct qedr_create_qp_uresp *uresp,
			       struct qedr_qp *qp)
{
	uresp->sq_db_offset = DB_ADDR_SHIFT(DQ_PWM_OFFSET_XCM_RDMA_SQ_PROD);

	/* iWARP uses the same cid for rq and sq */
	if (rdma_protocol_iwarp(&dev->ibdev, 1))
		uresp->sq_icid = qp->icid;
	else
		uresp->sq_icid = qp->icid + 1;
}

static int qedr_copy_qp_uresp(struct qedr_dev *dev,
			      struct qedr_qp *qp, struct ib_udata *udata,
			      struct qedr_create_qp_uresp *uresp)
{
	int rc;

	if (qedr_qp_has_sq(qp))
		qedr_copy_sq_uresp(dev, uresp, qp);

	if (qedr_qp_has_rq(qp))
		qedr_copy_rq_uresp(dev, uresp, qp);

	uresp->atomic_supported = dev->atomic_cap != IB_ATOMIC_NONE;
	uresp->qp_id = qp->qp_id;

	rc = qedr_copy_to_udata(udata, uresp, sizeof(*uresp));
	if (rc)
		DP_ERR(dev,
		       "create qp: failed a copy to user space with qp icid=0x%x.\n",
		       qp->icid);
	return rc;
}

static void qedr_set_common_qp_params(struct qedr_dev *dev,
				      struct qedr_qp *qp,
				      struct qedr_pd *pd,
				      struct ib_qp_init_attr *attrs)
{
	spin_lock_init(&qp->q_lock);

	if (rdma_protocol_iwarp(&dev->ibdev, 1)) {
		kref_init(&qp->refcnt);
		init_completion(&qp->iwarp_cm_comp);
#ifdef _HAS_QP_ALLOCATION
		init_completion(&qp->qp_rel_comp);
#endif
	}

	qp->pd = pd;
	/* INTERNAL: must be configured for qedr_qp_has_sq/rq() to work */
	qp->qp_type = attrs->qp_type;
	qp->sig = QEDR_QP_MAGIC_NUMBER;

	qp->state = QED_ROCE_QP_STATE_RESET;
	qp->signaled = (attrs->sq_sig_type == IB_SIGNAL_ALL_WR) ? true : false;
	qp->dev = dev;

	if (attrs->srq)
		qp->srq = get_qedr_srq(attrs->srq);

	if (qedr_qp_has_sq(qp)) {
		qp->sq.max_sges = attrs->cap.max_send_sge;
		qp->sq_cq = get_qedr_cq(attrs->send_cq);
		if (rdma_protocol_iwarp(&dev->ibdev, 1))
			qp->max_inline_data = IWARP_REQ_MAX_INLINE_DATA_SIZE;
		else
			qp->max_inline_data = ROCE_REQ_MAX_INLINE_DATA_SIZE;

		DP_DEBUG(dev, QEDR_MSG_QP,
			 "SQ params:\tsq_max_sges = %d, sq_cq_id = %d\n",
			 qp->sq.max_sges, qp->sq_cq->icid);
	}

	if (qedr_qp_has_rq(qp)) {
		qp->rq_cq = get_qedr_cq(attrs->recv_cq);
		qp->rq.max_sges = attrs->cap.max_recv_sge;
		DP_DEBUG(dev, QEDR_MSG_QP,
			 "RQ params:\trq_max_sges = %d, rq_cq_id = %d\n",
			 qp->rq.max_sges, qp->rq_cq->icid);
	}

	DP_DEBUG(dev, QEDR_MSG_QP,
		 "QP params:\tpd = %d, qp_type = %d, max_inline_data = %d, state = %d, signaled = %d, use_srq=%d\n",
		 (pd) ? pd->pd_id : -EINVAL, qp->qp_type, qp->max_inline_data,
		 qp->state, qp->signaled, (attrs->srq) ? 1 : 0);
}

static int qedr_set_roce_db_info(struct qedr_dev *dev, struct qedr_qp *qp)
{
	int rc = 0;

	if (qedr_qp_has_sq(qp)) {
		qp->sq.dpm_db = dev->db_addr +
		    DB_ADDR_SHIFT(DQ_PWM_OFFSET_DPM_BASE);
		qp->sq.db = dev->db_addr +
		    DB_ADDR_SHIFT(DQ_PWM_OFFSET_XCM_RDMA_SQ_PROD);
		qp->sq.db_data.data.icid = cpu_to_le16(qp->icid + 1);

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
		/* register SQ db info with doorbell drop recovery mechanism */
		rc = qedr_db_recovery_add(dev, qp->sq.db,
					  &qp->sq.db_data,
					  DB_REC_WIDTH_32B, DB_REC_KERNEL);
		if (rc)
			return rc;
#endif
	}

	if (qedr_qp_has_rq(qp)) {
		qp->rq.db = dev->db_addr +
		    DB_ADDR_SHIFT(DQ_PWM_OFFSET_TCM_ROCE_RQ_PROD);
		qp->rq.db_data.data.icid = cpu_to_le16(qp->icid);

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
		/* register RQ db info with doorbell drop recovery mechanism */
		rc = qedr_db_recovery_add(dev, qp->rq.db,
					  &qp->rq.db_data,
					  DB_REC_WIDTH_32B, DB_REC_KERNEL);

		/* remove the SQ db data in case RQ failed */
		if (rc && qedr_qp_has_sq(qp))
			qedr_db_recovery_del(dev, qp->sq.db, &qp->sq.db_data,
					     qp->recov_info.reset);
#endif
	}

	return rc;
}

static int qedr_check_srq_params(struct qedr_dev *dev,
				 struct ib_srq_init_attr *attrs,
				 struct ib_udata *udata)
{
	struct qedr_device_attr *qattr = &dev->attr;

	if (attrs->attr.max_wr > qattr->max_srq_wr) {
		DP_ERR(dev,
		       "create srq: unsupported srq_wr=0x%x requested (max_srq_wr=0x%x)\n",
		       attrs->attr.max_wr, qattr->max_srq_wr);
		return -EINVAL;
	}

	if (attrs->attr.max_sge > qattr->max_sge) {
		DP_ERR(dev,
		       "create srq: unsupported sge=0x%x requested (max_srq_sge=0x%x)\n",
		       attrs->attr.max_sge, qattr->max_sge);
	}

#ifdef _HAS_XRC_SUPPORT /* QEDR_UPSTREAM */
	if (!udata && attrs->srq_type == IB_SRQT_XRC) {
		DP_ERR(dev, "XRC SRQs are not supported in kernel-space\n");
		return -EINVAL;
	}
#endif

	return 0;
}

static void qedr_free_srq_user_params(struct qedr_srq *srq)
{
	qedr_free_pbl(srq->dev, &srq->usrq.pbl_info, srq->usrq.pbl_tbl);
	ib_umem_release(srq->usrq.umem);
	ib_umem_release(srq->prod_umem);
}

static void qedr_free_srq_kernel_params(struct qedr_srq *srq)
{
	struct qedr_srq_hwq_info *hw_srq = &srq->hw_srq;
	struct qedr_dev *dev = srq->dev;

	dev->ops->common->chain_free(dev->cdev, &hw_srq->pbl);

	dma_free_coherent(&dev->pdev->dev, sizeof(struct rdma_srq_producers),
			  hw_srq->virt_prod_pair_addr,
			  hw_srq->phy_prod_pair_addr);
}

static int qedr_init_srq_user_params(struct ib_ucontext *ib_ctx,
				     struct ib_udata *udata,
				     struct qedr_srq *srq,
				     struct qedr_create_srq_ureq *ureq,
				     int access, int dmasync)
{
#ifdef DEFINE_IB_UMEM_WITH_CHUNK
	struct ib_umem_chunk *chunk;
#endif
	struct scatterlist *sg;
	int rc;

	rc = qedr_init_user_queue(ib_ctx, udata, srq->dev, &srq->usrq, ureq->srq_addr,
				  ureq->srq_len, 0, access, dmasync, 1);
	if (rc)
		return rc;

	srq->prod_umem = compat_ib_umem_get(ib_ctx, udata, &srq->dev->ibdev,
					    ureq->prod_pair_addr,
					    sizeof(struct rdma_srq_producers),
					    access, dmasync);
	if (IS_ERR(srq->prod_umem)) {
		qedr_free_pbl(srq->dev, &srq->usrq.pbl_info, srq->usrq.pbl_tbl);
		ib_umem_release(srq->usrq.umem);
		DP_ERR(srq->dev,
		       "create srq: failed ib_umem_get for producer, got %ld\n",
		       PTR_ERR(srq->prod_umem));
		return PTR_ERR(srq->prod_umem);
	}
#ifdef DEFINE_IB_UMEM_WITH_CHUNK
	chunk = container_of((&srq->prod_umem->chunk_list)->next,
			     typeof(*chunk), list);
	sg = &chunk->page_list[0];
#else
#ifdef DEFINE_UMEM_WITH_SG_HEAD
	sg = srq->prod_umem->sg_head.sgl;
#else
	sg = srq->prod_umem->sgt_append.sgt.sgl;
#endif
#endif
	srq->hw_srq.phy_prod_pair_addr = sg_dma_address(sg);

	return 0;
}

static int qedr_alloc_srq_kernel_params(struct qedr_srq *srq,
					struct qedr_dev *dev,
					struct ib_srq_init_attr *init_attr)
{
	struct qedr_srq_hwq_info *hw_srq = &srq->hw_srq;
	struct qed_chain_params chain_params;
	dma_addr_t phy_prod_pair_addr;
	u32 num_elems, max_wr;
	void *va;
	int rc;

	va = dma_alloc_coherent(&dev->pdev->dev,
				sizeof(struct rdma_srq_producers),
				&phy_prod_pair_addr, GFP_KERNEL);
	if (!va) {
		DP_ERR(dev,
		       "create srq: failed to allocate dma memory for producer\n");
		return -ENOMEM;
	}

	hw_srq->phy_prod_pair_addr = phy_prod_pair_addr;
	hw_srq->virt_prod_pair_addr = va;

	max_wr = init_attr->attr.max_wr;

	num_elems = max_wr * RDMA_MAX_SRQ_WQE_SIZE;
	dev->ops->common->chain_params_init(&chain_params,
					    QED_CHAIN_USE_TO_CONSUME_PRODUCE,
					    QED_CHAIN_MODE_PBL,
					    QED_CHAIN_CNT_TYPE_U32,
					    num_elems,
					    QEDR_SRQ_WQE_ELEM_SIZE);
	rc = dev->ops->common->chain_alloc(dev->cdev, &hw_srq->pbl,
					   &chain_params);
	if (rc)
		goto err0;

	hw_srq->max_wr = max_wr;
	hw_srq->num_elems = num_elems;
	hw_srq->max_sges = RDMA_MAX_SGE_PER_SRQ;

	return 0;

err0:
	dma_free_coherent(&dev->pdev->dev, sizeof(struct rdma_srq_producers),
			  va, phy_prod_pair_addr);
	return rc;
}
#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
static int qedr_idr_add(struct qedr_dev *dev, struct qedr_idr *qidr,
			void *ptr, u32 id);
static void qedr_idr_remove(struct qedr_dev *dev, struct qedr_idr *qidr, u32 id);
#endif

COMPAT_CREATE_SRQ_DECLARE_RET
qedr_create_srq(COMPAT_CREATE_SRQ_IBPD(struct ib_pd *ibpd)
		COMPAT_CREATE_SRQ_SRQ(struct ib_srq *ibsrq)
		struct ib_srq_init_attr *init_attr,
		struct ib_udata *udata)
{
	struct qed_rdma_destroy_srq_in_params destroy_in_params;
#ifdef _HAS_SRQ_ALLOCATION
	struct qedr_dev *dev = get_qedr_dev(ibsrq->device);
	struct qedr_pd *pd = get_qedr_pd(ibsrq->pd);
#else
	struct qedr_dev *dev = get_qedr_dev(ibpd->device);
	struct qedr_pd *pd = get_qedr_pd(ibpd);
#endif
	struct qed_rdma_create_srq_out_params out_params;
	struct qed_rdma_create_srq_in_params in_params;
	u64 pbl_base_addr, phy_prod_pair_addr;
	struct ib_ucontext *ib_ctx = NULL;
	struct qedr_srq_hwq_info *hw_srq;
	struct qedr_create_srq_ureq ureq;
	u32 page_cnt, page_size;
	struct qedr_srq *srq;
	int rc = 0;

	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
		return COMPAT_CREATE_SRQ_ERR(rc);

#ifdef _HAS_XRC_SUPPORT
	DP_VERBOSE(dev, QEDR_MSG_QP,
		   "create XRC/SRQ called from %s (pd %p), is_xrc=%u\n",
		   (udata) ? "User lib" : "kernel",
		   pd, init_attr->srq_type == IB_SRQT_XRC);
#endif

	rc = qedr_check_srq_params(dev, init_attr, udata);
	if (rc)
		return COMPAT_CREATE_SRQ_ERR(-EINVAL);

#ifdef _HAS_SRQ_ALLOCATION
	srq = get_qedr_srq(ibsrq);
#else
	srq = kzalloc(sizeof(*srq), GFP_KERNEL);
	if (!srq)
		return COMPAT_CREATE_SRQ_ERR(-ENOMEM);
#endif

	srq->dev = dev;
#ifdef _HAS_XRC_SUPPORT /* QEDR_UPSTREAM */
	srq->is_xrc = (init_attr->srq_type == IB_SRQT_XRC);
#endif
	hw_srq = &srq->hw_srq;
	spin_lock_init(&srq->lock);
	atomic_set(&hw_srq->wr_cons_cnt, 0);
	memset(&in_params, 0, sizeof(in_params));

	hw_srq->max_wr = init_attr->attr.max_wr;
	hw_srq->max_sges = RDMA_MAX_SGE_PER_SRQ;

	if (udata) {
#ifndef _HAS_SRQ_ALLOCATION
		ib_ctx = ibpd->uobject->context;
#endif
		memset(&ureq, 0, sizeof(ureq));
		if (qedr_copy_from_udata(&ureq, udata, sizeof(ureq))) {
			DP_ERR(dev,
			       "create srq: problem copying data from user space\n");
			goto err0;
		}

		rc = qedr_init_srq_user_params(ib_ctx, udata, srq, &ureq, 0, 0);
		if (rc)
			goto err0;

		page_cnt = srq->usrq.pbl_info.num_pbes;
		pbl_base_addr = srq->usrq.pbl_tbl->pa;
		phy_prod_pair_addr = hw_srq->phy_prod_pair_addr;
#ifdef DEFINE_IB_UMEM_NO_PAGE_PARAM
		page_size = PAGE_SIZE;
#else
#if DEFINE_IB_UMEM_PAGE_SHIFT /* QEDR_UPSTREAM */
		page_size = BIT(srq->usrq.umem->page_shift);
#else
		page_size = srq->usrq.umem->page_size;
#endif
#endif
	} else {
		struct qed_chain *pbl;

		rc = qedr_alloc_srq_kernel_params(srq, dev, init_attr);
		if (rc)
			goto err0;
		pbl = &hw_srq->pbl;

		page_cnt = qed_chain_get_page_cnt(pbl);
		pbl_base_addr = qed_chain_get_pbl_phys(pbl);
		phy_prod_pair_addr = hw_srq->phy_prod_pair_addr;
		page_size = pbl->elem_per_page << 4;
	}

	memset(&in_params, 0, sizeof(in_params));
	in_params.pd_id = pd->pd_id;
	in_params.pbl_base_addr = pbl_base_addr;
	in_params.prod_pair_addr = phy_prod_pair_addr;
	in_params.num_pages = page_cnt;
	in_params.page_size = page_size;

#ifdef _HAS_XRC_SUPPORT /* QEDR_UPSTREAM */
	if (srq->is_xrc) {
		struct qedr_xrcd *xrcd = get_qedr_xrcd(init_attr->ext.xrc.xrcd);
#ifndef DEFINE_CQ_IN_XRC /* QEDR_UPSTREAM */
		struct qedr_cq *cq = get_qedr_cq(init_attr->ext.cq);
#else
		struct qedr_cq *cq = get_qedr_cq(init_attr->ext.xrc.cq);
#endif
		in_params.is_xrc = 1;
		in_params.xrcd_id = xrcd->xrcd_id;
		in_params.cq_cid = cq->icid;
	}
#endif

	rc = dev->ops->rdma_create_srq(dev->rdma_ctx, &in_params, &out_params);
	if (rc)
		goto err1;

	srq->srq_id = out_params.srq_id;

	if (udata) {
		rc = qedr_copy_srq_uresp(dev, srq, udata);
		if (rc)
			goto err2;
	}

	rc = qedr_idr_add(dev, &dev->srqidr, srq, srq->srq_id);
	if (rc)
		goto err2;

	DP_VERBOSE(dev, QEDR_MSG_SRQ,
		   "create srq: created srq with srq:%p srq_id=0x%0x num_elems:%d\n",
		   srq, srq->srq_id, srq->hw_srq.num_elems);

	qedr_recov_obj_add(dev, &srq->recov_info);

	COMPAT_HAS_SRQ_RET(return 0);
	COMPAT_NO_SRQ_RET(return &srq->ibsrq);

err2:
	memset(&in_params, 0, sizeof(in_params));
	destroy_in_params.srq_id = srq->srq_id;

	/* Intentionally ignore return value, keep the original rc */
	dev->ops->rdma_destroy_srq(dev->rdma_ctx, &destroy_in_params);
err1:
	if (udata)
		qedr_free_srq_user_params(srq);
	else
		qedr_free_srq_kernel_params(srq);
err0:
#ifndef _HAS_SRQ_ALLOCATION
	kfree(srq);
#endif
	return COMPAT_CREATE_SRQ_ERR(-EFAULT);
}

COMPAT_DESTROY_SRQ_DECLARE_RET
qedr_destroy_srq(struct ib_srq *ibsrq
		COMPAT_DESTROY_SRQ_UDATA(struct ib_udata *udata))
{
	struct qedr_dev *dev = get_qedr_dev(ibsrq->device);
	struct qed_rdma_destroy_srq_in_params in_params;
	struct qedr_srq *srq = get_qedr_srq(ibsrq);

	if (dev->recov_info.recov_in_prog)
		wait_for_completion(&dev->recov_info.recov_comp);

	memset(&in_params, 0, sizeof(in_params));
	in_params.srq_id = srq->srq_id;
	in_params.is_xrc = srq->is_xrc;

	if (!srq->recov_info.reset)
		dev->ops->rdma_destroy_srq(dev->rdma_ctx, &in_params);

	if (ibsrq->pd->uobject)
		qedr_free_srq_user_params(srq);
	else
		qedr_free_srq_kernel_params(srq);

	qedr_idr_remove(dev, &dev->srqidr, srq->srq_id);

	DP_VERBOSE(dev, QEDR_MSG_SRQ,
		   "destroy srq: destroyed xrc/srq with srq_id=0x%0x, is_xrc=%u\n",
		   srq->srq_id, srq->is_xrc);

	qedr_recov_obj_del(dev, &srq->recov_info);

#ifndef _HAS_SRQ_ALLOCATION
	kfree(srq);
#endif
	COMPAT_DESTROY_SRQ_RET(return 0;)
}

int qedr_modify_srq(struct ib_srq *ibsrq, struct ib_srq_attr *attr,
		    enum ib_srq_attr_mask attr_mask, struct ib_udata *udata)
{
	struct qedr_dev *dev = get_qedr_dev(ibsrq->device);
	struct qed_rdma_modify_srq_in_params in_params;
	struct qedr_srq *srq = get_qedr_srq(ibsrq);
	int rc;

	if (srq->recov_info.reset) {
		DP_VERBOSE(dev, QEDR_MSG_FAIL,
			   "failed qedr_modify_srq, srq has been reset\n");

		return -EPERM;
	}

	if (attr_mask & IB_SRQ_MAX_WR) {
		DP_ERR(dev,
		       "modify srq: invalid attribute mask=0x%x specified for %p\n",
		       attr_mask, srq);

		return -EINVAL;
	}

	if (attr_mask & IB_SRQ_LIMIT) {
		if (attr->srq_limit >= srq->hw_srq.max_wr) {
			DP_ERR(dev,
			       "modify srq: invalid srq_limit=0x%x (max_srq_limit=0x%x)\n",
			       attr->srq_limit, srq->hw_srq.max_wr);

			return -EINVAL;
		}

		memset(&in_params, 0, sizeof(in_params));
		in_params.srq_id = srq->srq_id;
		in_params.wqe_limit = attr->srq_limit;
		rc = dev->ops->rdma_modify_srq(dev->rdma_ctx, &in_params);
		if (rc)
			return rc;
	}

	srq->srq_limit = attr->srq_limit;

	DP_VERBOSE(dev, QEDR_MSG_SRQ,
		   "modify srq: modified srq with srq_id=0x%0x\n", srq->srq_id);

	return 0;
}

static enum qed_rdma_qp_type qedr_ib_to_qed_qp_type(enum ib_qp_type ib_qp_type)
{
	switch (ib_qp_type) {
	case IB_QPT_RC:
		return QED_RDMA_QP_TYPE_RC;
#ifdef _HAS_XRC_SUPPORT /* QEDR_UPSTREAM */
	case IB_QPT_XRC_INI:
		return QED_RDMA_QP_TYPE_XRC_INI;
	case IB_QPT_XRC_TGT:
		return QED_RDMA_QP_TYPE_XRC_TGT;
#endif
	default:
		return QED_RDMA_QP_TYPE_INVAL;
	}
}

static inline void
qedr_init_common_qp_in_params(struct qedr_dev *dev,
			      struct qedr_pd *pd,
			      struct qedr_qp *qp,
			      struct ib_qp_init_attr *attrs,
			      bool fmr_and_reserved_lkey,
			      struct qed_rdma_create_qp_in_params *params)
{
	/* QP handle to be written in an async event */
	params->qp_handle_async_lo = lower_32_bits((uintptr_t) qp);
	params->qp_handle_async_hi = upper_32_bits((uintptr_t) qp);

	params->signal_all = (attrs->sq_sig_type == IB_SIGNAL_ALL_WR);
	params->fmr_and_reserved_lkey = fmr_and_reserved_lkey;
	params->qp_type = qedr_ib_to_qed_qp_type(attrs->qp_type);
	dev->ops->rdma_get_stats_queue(dev->rdma_ctx, &params->stats_queue);

	if (pd) {
		/* INTERNAL: relevant for XRC TGT QP */
		params->pd = pd->pd_id;
		params->dpi = pd->uctx ? pd->uctx->dpi : dev->dpi;
	}

	if (qedr_qp_has_sq(qp))
		params->sq_cq_id = get_qedr_cq(attrs->send_cq)->icid;

	if (qedr_qp_has_rq(qp))
		params->rq_cq_id = get_qedr_cq(attrs->recv_cq)->icid;

	if (qedr_qp_has_srq(qp)) {
		/* QP is associated with SRQ instead of RQ */
		params->rq_cq_id = get_qedr_cq(attrs->recv_cq)->icid;
		params->srq_id = qp->srq->srq_id;
		params->use_srq = true;
		return;
	} else {
		params->srq_id = 0;
		params->use_srq = false;
	}
}

static inline void qedr_qp_user_print(struct qedr_dev *dev, struct qedr_qp *qp)
{
	DP_DEBUG(dev, QEDR_MSG_QP, "create qp: successfully created user QP. "
		 "qp=%p. "
		 "sq_addr=0x%llx, "
		 "sq_len=%zd, "
		 "rq_addr=0x%llx, "
		 "rq_len=%zd"
		 "\n",
		 qp,
		 qp->usq.buf_addr,
		 qp->usq.buf_len, qp->urq.buf_addr, qp->urq.buf_len);
}
#ifdef DEFINED_IDR_PRELOAD /* QEDR_UPSTREAM */
static int qedr_idr_add(struct qedr_dev *dev, struct qedr_idr *qidr,
			void *ptr, u32 id)
{
	int rc;

	idr_preload(GFP_KERNEL);
	spin_lock_irq(&qidr->idr_lock);

	rc = idr_alloc(&qidr->idr, ptr, id, id + 1, GFP_ATOMIC);

	spin_unlock_irq(&qidr->idr_lock);
	idr_preload_end();

	return rc < 0 ? rc : 0;
}
#else
static int qedr_idr_add(struct qedr_dev *dev, struct qedr_idr *qidr,
			void *ptr, u32 id)
{
	u32 newid;
	int rc;

	do {
		if (!idr_pre_get(&qidr->idr, GFP_KERNEL))
			return -ENOMEM;

		spin_lock_irq(&qidr->idr_lock);

		rc = idr_get_new_above(&qidr->idr, ptr, id, &newid);
		BUG_ON(!rc && newid != id);

		spin_unlock_irq(&qidr->idr_lock);
	} while (rc == -EAGAIN);
	return rc;
}
#endif

static void qedr_idr_remove(struct qedr_dev *dev, struct qedr_idr *qidr, u32 id)
{
	spin_lock_irq(&qidr->idr_lock);
	idr_remove(&qidr->idr, id);
	spin_unlock_irq(&qidr->idr_lock);
}
static inline void
qedr_iwarp_populate_user_qp(struct qedr_dev *dev,
			    struct qedr_qp *qp,
			    struct qed_rdma_create_qp_out_params *out_params)
{
	qp->usq.pbl_tbl->va = out_params->sq_pbl_virt;
	qp->usq.pbl_tbl->pa = out_params->sq_pbl_phys;

	qedr_populate_pbls(dev, qp->usq.umem, qp->usq.pbl_tbl,
			   PAGE_MASK, &qp->usq.pbl_info, FW_PAGE_SHIFT);

	if (qedr_qp_has_rq(qp)) {
		qp->urq.pbl_tbl->va = out_params->rq_pbl_virt;
		qp->urq.pbl_tbl->pa = out_params->rq_pbl_phys;

		qedr_populate_pbls(dev, qp->urq.umem, qp->urq.pbl_tbl,
				   PAGE_MASK, &qp->urq.pbl_info, FW_PAGE_SHIFT);
	}
}

static int qedr_cleanup_user(struct qedr_dev *dev, struct qedr_qp *qp)
{
	int rc = 0;

	if (qedr_qp_has_sq(qp)) {
		if (qp->usq.umem)
			ib_umem_release(qp->usq.umem);
		qp->usq.umem = NULL;

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
		if (qp->usq.db_rec_umem)
			ib_umem_release(qp->usq.db_rec_umem);
		qp->usq.db_rec_umem = NULL;

		/* remove sq doorbell from doorbell recovery mechanism */
		rc = qedr_db_recovery_del(dev, qp->usq.db_addr,
					  &qp->usq.db_rec_virt->db_data,
					  qp->recov_info.reset);

		if (rdma_protocol_roce(&dev->ibdev, 1))
			qedr_free_pbl(dev, &qp->usq.pbl_info,
				      qp->usq.pbl_tbl);
		else
			kfree(qp->usq.pbl_tbl);
		if (rc)
			return rc;
#endif
	}

	if (qedr_qp_has_rq(qp)) {
		if (qp->urq.umem)
			ib_umem_release(qp->urq.umem);
		qp->urq.umem = NULL;

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
		if (qp->urq.db_rec_umem)
			ib_umem_release(qp->urq.db_rec_umem);
		qp->urq.db_rec_umem = NULL;

		/* remove sq doorbell from doorbell recovery mechanism */
		rc = qedr_db_recovery_del(dev, qp->urq.db_addr,
					  &qp->urq.db_rec_virt->db_data,
					  qp->recov_info.reset);

		if (rdma_protocol_roce(&dev->ibdev, 1))
			qedr_free_pbl(dev, &qp->urq.pbl_info,
				      qp->urq.pbl_tbl);
		else
			kfree(qp->urq.pbl_tbl);
		if (rc)
			return rc;
#endif
	}

	/* release doorbell recovery umem */
#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
	if (rdma_protocol_iwarp(&dev->ibdev, 1))
		rc = qedr_db_recovery_del(dev, qp->urq.db_rec_db2_addr,
					  &qp->urq.db_rec_db2_data,
					  qp->recov_info.reset);
#endif
	return rc;
}

static int qedr_create_user_qp(struct qedr_dev *dev,
			       struct qedr_qp *qp,
			       struct ib_pd *ibpd,
			       struct ib_udata *udata,
			       struct ib_qp_init_attr *attrs)
{
	struct qed_rdma_destroy_qp_out_params d_out_params;
	struct qed_rdma_create_qp_out_params out_params;
	struct qed_rdma_create_qp_in_params in_params;
	struct ib_ucontext *ib_ctx = NULL;
	struct qedr_create_qp_uresp uresp;
	struct qedr_ucontext *ctx = NULL;
	struct qedr_create_qp_ureq ureq;
	int alloc_and_init = IS_ROCE(dev);
	struct qedr_pd *pd = NULL;
	int rc = 0;

	if (ibpd) {
		ib_ctx = ibpd->uobject->context;
		ctx = get_qedr_ucontext(ib_ctx);
		pd = get_qedr_pd(ibpd);
	}

	qp->create_type = QEDR_QP_CREATE_USER;

	memset(&ureq, 0, sizeof(ureq));
	if (udata) {
		rc = qedr_copy_from_udata(&ureq, udata, sizeof(ureq));
		if (rc) {
			DP_ERR(dev, "Problem copying data from user space\n");
			return rc;
		}
	}

	if (qedr_qp_has_sq(qp)) {
		/* SQ - read access only (0), dma sync not required (0) */
		rc = qedr_init_user_queue(ib_ctx, udata, dev, &qp->usq, ureq.sq_addr,
					  ureq.sq_len, ureq.sq_db_rec_addr,
					  0, 0, alloc_and_init);
		if (rc)
			return rc;
	}

	if (qedr_qp_has_rq(qp)) {
		/* RQ - read access only (0), dma sync not required (0) */
		rc = qedr_init_user_queue(ib_ctx, udata, dev, &qp->urq, ureq.rq_addr,
					  ureq.rq_len, ureq.rq_db_rec_addr,
					  0, 0, alloc_and_init);
		if (rc)
			return rc;
	}

	memset(&in_params, 0, sizeof(in_params));
	qedr_init_common_qp_in_params(dev, pd, qp, attrs, false, &in_params);
	in_params.qp_handle_lo = ureq.qp_handle_lo;
	in_params.qp_handle_hi = ureq.qp_handle_hi;

#ifdef _HAS_XRC_SUPPORT /* QEDR_UPSTREAM */
	if (qp->qp_type == IB_QPT_XRC_TGT) {
		struct qedr_xrcd *xrcd = get_qedr_xrcd(attrs->xrcd);

		in_params.xrcd_id = xrcd->xrcd_id;
		/* ib stack doesn't pass to the kernel QP user address,
		 * so we use QP id as QP handle
		 */
		in_params.qp_handle_lo = qp->qp_id;
	}
#endif

	if (qedr_qp_has_sq(qp)) {
		in_params.sq_num_pages = qp->usq.pbl_info.num_pbes;
		in_params.sq_pbl_ptr = qp->usq.pbl_tbl->pa;
	}

	if (qedr_qp_has_rq(qp)) {
		in_params.rq_num_pages = qp->urq.pbl_info.num_pbes;
		in_params.rq_pbl_ptr = qp->urq.pbl_tbl->pa;
	}

	if (ctx)
		SET_FIELD(in_params.create_flags,
			  QED_ROCE_EDPM_MODE_V1, ctx->edpm_mode);

	qp->qed_qp = dev->ops->rdma_create_qp(dev->rdma_ctx,
					      &in_params, &out_params);
	if (!qp->qed_qp) {
		rc = -ENOMEM;
		goto err1;
	}

	if (rdma_protocol_iwarp(&dev->ibdev, 1))
		qedr_iwarp_populate_user_qp(dev, qp, &out_params);

	qp->qp_id = out_params.qp_id;
	qp->icid = out_params.icid;

	memset(&uresp, 0, sizeof(uresp));

	if (udata) {
		rc = qedr_copy_qp_uresp(dev, qp, udata, &uresp);
		if (rc)
			goto err;
	}

	if (ctx && qedr_qp_has_sq(qp)) {
		qp->usq.db_addr = (void __iomem *)(uintptr_t) ctx->dpi_addr +
				  uresp.sq_db_offset;

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
		/* add doorbell addresses to doorbell drop recovery mechanism */
		rc = qedr_db_recovery_add(dev, qp->usq.db_addr,
					  &qp->usq.db_rec_virt->db_data,
					  DB_REC_WIDTH_32B, DB_REC_USER);
		if (rc)
			return rc;
#endif
	}

	if (ctx && qedr_qp_has_rq(qp)) {
		qp->urq.db_addr = (void __iomem *)(uintptr_t) ctx->dpi_addr +
				  uresp.rq_db_offset;

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
		/* add doorbell addresses to doorbell drop recovery mechanism */
		rc = qedr_db_recovery_add(dev, qp->urq.db_addr,
					  &qp->urq.db_rec_virt->db_data,
					  DB_REC_WIDTH_32B, DB_REC_USER);
		if (rc)
			return rc;
#endif
	}

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */

	if (ctx && rdma_protocol_iwarp(&dev->ibdev, 1)) {
		qp->urq.db_rec_db2_addr =
				(void __iomem *)(uintptr_t)ctx->dpi_addr +
				uresp.rq_db2_offset;

		/* calculate the db_rec_db2 data since it is constant so no need
		 * to reflect from user
		 */
		qp->urq.db_rec_db2_data.data.icid = qp->icid;
		qp->urq.db_rec_db2_data.data.value =
				DQ_TCM_IWARP_POST_RQ_CF_CMD;

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
		/* add doorbell addresses to doorbell drop recovery mechanism */
		if (IS_IWARP(dev)) {
			rc = qedr_db_recovery_add(dev, qp->urq.db_rec_db2_addr,
						  &qp->urq.db_rec_db2_data,
						  DB_REC_WIDTH_32B,
						  DB_REC_USER);
			if (rc)
				return rc;
		}
#endif
	}

#endif

	qedr_qp_user_print(dev, qp);

	return rc;
err:
	rc = dev->ops->rdma_destroy_qp(dev->rdma_ctx, qp->qed_qp,
				       &d_out_params);
	if (rc)
		DP_ERR(dev, "create qp: fatal fault. rc=%d", rc);

err1:
	qedr_cleanup_user(dev, qp);
	return rc;
}

static int qedr_set_iwarp_db_info(struct qedr_dev *dev,
				   struct qedr_qp *qp)

{
	int rc = 0;

	qp->sq.dpm_db = dev->db_addr +
	    DB_ADDR_SHIFT(DQ_PWM_OFFSET_DPM_BASE);
	qp->sq.db = dev->db_addr +
		DB_ADDR_SHIFT(DQ_PWM_OFFSET_XCM_RDMA_SQ_PROD);
	qp->sq.db_data.data.icid = qp->icid;

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
	/* register doorbell information with doorbell drop recovery mechanism */
	rc = qedr_db_recovery_add(dev, qp->sq.db,
				  &qp->sq.db_data,
				  DB_REC_WIDTH_32B,
				  DB_REC_KERNEL);
	if (rc)
		return rc;
#endif

	if (qedr_qp_has_rq(qp)) {
		qp->rq.db = dev->db_addr +
			DB_ADDR_SHIFT(DQ_PWM_OFFSET_TCM_IWARP_RQ_PROD);
		qp->rq.db_data.data.icid = qp->icid;

		qp->rq.iwarp_db2 = dev->db_addr +
			DB_ADDR_SHIFT(DQ_PWM_OFFSET_TCM_FLAGS);
		qp->rq.iwarp_db2_data.data.icid = qp->icid;
		qp->rq.iwarp_db2_data.data.value = DQ_TCM_IWARP_POST_RQ_CF_CMD;

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
		/* register doorbell information with doorbell drop recovery mechanism */
		rc = qedr_db_recovery_add(dev, qp->rq.db,
					  &qp->rq.db_data,
					  DB_REC_WIDTH_32B,
					  DB_REC_KERNEL);
		if (rc)
			return rc;

		rc = qedr_db_recovery_add(dev, qp->rq.iwarp_db2,
					  &qp->rq.iwarp_db2_data,
					  DB_REC_WIDTH_32B,
					  DB_REC_KERNEL);
#endif
	}

	return rc;
}

static int
qedr_roce_create_kernel_qp(struct qedr_dev *dev,
			   struct qedr_qp *qp,
			   struct qed_rdma_create_qp_in_params *in_params,
			   u32 n_sq_elems, u32 n_rq_elems)
{
	struct qed_rdma_create_qp_out_params out_params;
	struct qed_chain_params chain_params;
	int rc;

	if (qedr_qp_has_sq(qp)) {
		dev->ops->common->chain_params_init(&chain_params,
						    QED_CHAIN_USE_TO_PRODUCE,
						    QED_CHAIN_MODE_PBL,
						    QED_CHAIN_CNT_TYPE_U32,
						    n_sq_elems,
						    QEDR_SQE_ELEMENT_SIZE);
		rc = dev->ops->common->chain_alloc(dev->cdev, &qp->sq.pbl,
						   &chain_params);
		if (rc)
			return rc;

		in_params->sq_num_pages = qed_chain_get_page_cnt(&qp->sq.pbl);
		in_params->sq_pbl_ptr = qed_chain_get_pbl_phys(&qp->sq.pbl);
	}

	if (qedr_qp_has_rq(qp)) {
		dev->ops->common->chain_params_init(&chain_params,
						    QED_CHAIN_USE_TO_PRODUCE,
						    QED_CHAIN_MODE_PBL,
						    QED_CHAIN_CNT_TYPE_U32,
						    n_rq_elems,
						    QEDR_RQE_ELEMENT_SIZE);
		rc = dev->ops->common->chain_alloc(dev->cdev, &qp->rq.pbl,
						   &chain_params);
		if (rc)
			return rc;

		in_params->rq_num_pages = qed_chain_get_page_cnt(&qp->rq.pbl);
		in_params->rq_pbl_ptr = qed_chain_get_pbl_phys(&qp->rq.pbl);
	}

	qp->qed_qp = dev->ops->rdma_create_qp(dev->rdma_ctx,
					      in_params, &out_params);

	if (!qp->qed_qp)
		return -EINVAL;

	qp->qp_id = out_params.qp_id;
	qp->icid = out_params.icid;

	return qedr_set_roce_db_info(dev, qp);
}

static int
qedr_iwarp_create_kernel_qp(struct qedr_dev *dev,
			    struct qedr_qp *qp,
			    struct qed_rdma_create_qp_in_params *in_params,
			    u32 n_sq_elems, u32 n_rq_elems)
{
	struct qed_rdma_create_qp_out_params out_params;
	struct qed_chain_params chain_params;
	struct qed_chain_ext_pbl ext_pbl;
	int rc;

	in_params->sq_num_pages = QED_CHAIN_PAGE_CNT(n_sq_elems,
						     QEDR_SQE_ELEMENT_SIZE,
						     QED_CHAIN_PAGE_SIZE,
						     QED_CHAIN_MODE_PBL);
	in_params->rq_num_pages = QED_CHAIN_PAGE_CNT(n_rq_elems,
						     QEDR_RQE_ELEMENT_SIZE,
						     QED_CHAIN_PAGE_SIZE,
						     QED_CHAIN_MODE_PBL);

	qp->qed_qp = dev->ops->rdma_create_qp(dev->rdma_ctx,
					      in_params, &out_params);

	if (!qp->qed_qp)
		return -EINVAL;

	/* Now we allocate the chain */
	ext_pbl.p_pbl_virt = out_params.sq_pbl_virt;
	ext_pbl.p_pbl_phys = out_params.sq_pbl_phys;

	dev->ops->common->chain_params_init(&chain_params,
					    QED_CHAIN_USE_TO_PRODUCE,
					    QED_CHAIN_MODE_PBL,
					    QED_CHAIN_CNT_TYPE_U32,
					    n_sq_elems,
					    QEDR_SQE_ELEMENT_SIZE);
	chain_params.ext_pbl = &ext_pbl;
	rc = dev->ops->common->chain_alloc(dev->cdev, &qp->sq.pbl,
					   &chain_params);
	if (rc)
		goto err;

	ext_pbl.p_pbl_virt = out_params.rq_pbl_virt;
	ext_pbl.p_pbl_phys = out_params.rq_pbl_phys;

	if (qedr_qp_has_rq(qp)) {
		dev->ops->common->chain_params_init(&chain_params,
						    QED_CHAIN_USE_TO_PRODUCE,
						    QED_CHAIN_MODE_PBL,
						    QED_CHAIN_CNT_TYPE_U32,
						    n_rq_elems,
						    QEDR_RQE_ELEMENT_SIZE);
		chain_params.ext_pbl = &ext_pbl;
		rc = dev->ops->common->chain_alloc(dev->cdev, &qp->rq.pbl,
						   &chain_params);
		if (rc)
			goto err;
	}

	qp->qp_id = out_params.qp_id;
	qp->icid = out_params.icid;

	return qedr_set_iwarp_db_info(dev, qp);

err:
	dev->ops->rdma_destroy_qp(dev->rdma_ctx, qp->qed_qp, NULL);

	return rc;
}

static int qedr_cleanup_kernel(struct qedr_dev *dev, struct qedr_qp *qp)
{
	int rc = 0;

	if (qedr_qp_has_sq(qp) || qp->qp_type == IB_QPT_GSI) {
		dev->ops->common->chain_free(dev->cdev, &qp->sq.pbl);

		kfree(qp->wqe_wr_id);

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
		/* remove sq doorbell from doorbell recovery mechanism */
		rc = qedr_db_recovery_del(dev, qp->sq.db, &qp->sq.db_data,
					  qp->recov_info.reset);
		if (rc)
			return rc;
#endif
	}

	if (qedr_qp_has_rq(qp) || qp->qp_type == IB_QPT_GSI) {
		dev->ops->common->chain_free(dev->cdev, &qp->rq.pbl);

		kfree(qp->rqe_wr_id);

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
		/* remove rq doorbell from doorbell recovery mechanism */
		rc = qedr_db_recovery_del(dev, qp->rq.db, &qp->rq.db_data,
					  qp->recov_info.reset);
		if (rc)
			return rc;
#endif
	}

#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
	if (rdma_protocol_iwarp(&dev->ibdev, 1)) {
		rc = qedr_db_recovery_del(dev, qp->rq.iwarp_db2,
					  &qp->rq.iwarp_db2_data,
					  qp->recov_info.reset);
		if (rc)
			return rc;
	}
#endif

	return 0;
}

static int qedr_create_kernel_qp(struct qedr_dev *dev,
				 struct qedr_qp *qp,
				 struct ib_pd *ibpd,
				 struct ib_qp_init_attr *attrs)
{
	struct qed_rdma_create_qp_in_params in_params;
	struct qedr_pd *pd = get_qedr_pd(ibpd);
	u32 n_rq_elems = 0;
	u32 n_sq_elems = 0;
	int rc = -EINVAL;

	memset(&in_params, 0, sizeof(in_params));
	qp->create_type = QEDR_QP_CREATE_KERNEL;

	qedr_init_common_qp_in_params(dev, pd, qp, attrs, true, &in_params);

	/* QP handle to be written in CQE */
	in_params.qp_handle_lo = lower_32_bits((uintptr_t) qp);
	in_params.qp_handle_hi = upper_32_bits((uintptr_t) qp);

	if (qedr_qp_has_sq(qp)) {
		u32 n_sq_entries;

		/* A single work request may take up to QEDR_MAX_SQ_WQE_SIZE
		 * elements in the ring. The ring should allow at least a single
		 * WR, even if the user requested none, due to allocation
		 * issues. We should add an extra WR since the prod and cons
		 * indices of wqe_wr_id are managed in such a way that the WQ is
		 * considered full when (prod+1)%max_wr==cons. We currently
		 * don't do that because we double the number of entries due an
		 * iSER issue that pushes far more WRs than indicated. If we
		 * decline its ib_post_send() then we get error prints in the
		 * dmesg we'd like to avoid.
		 */
		qp->sq.max_wr =
			min_t(u32, attrs->cap.max_send_wr * dev->wq_multiplier,
			      dev->attr.max_sqe);

		qp->wqe_wr_id = kzalloc(qp->sq.max_wr * sizeof(*qp->wqe_wr_id),
					GFP_KERNEL);
		if (!qp->wqe_wr_id) {
			DP_ERR(dev,
			       "create qp: failed SQ shadow memory allocation\n");
			return -ENOMEM;
		}

		n_sq_entries = attrs->cap.max_send_wr;
		n_sq_entries = min_t(u32, n_sq_entries, dev->attr.max_sqe);
		n_sq_entries = max_t(u32, n_sq_entries, 1);
		n_sq_elems = n_sq_entries * QEDR_MAX_SQE_ELEMENTS_PER_SQE;
	}

	if (qedr_qp_has_rq(qp)) {
		/* A single work request may take up to QEDR_MAX_RQ_WQE_SIZE
		 * elements in the ring. There ring should allow at least a
		 * single WR, even if the user requested none, due to allocation
		 * issues.
		 */
		qp->rq.max_wr = (u16) max_t(u32, attrs->cap.max_recv_wr, 1);

		/* Allocate driver internal RQ array */
		qp->rqe_wr_id = kzalloc(qp->rq.max_wr * sizeof(*qp->rqe_wr_id),
					GFP_KERNEL);
		if (!qp->rqe_wr_id) {
			DP_ERR(dev,
			       "create qp: failed RQ shadow memory allocation\n");
			kfree(qp->wqe_wr_id);
			return -ENOMEM;
		}

		n_rq_elems = qp->rq.max_wr * QEDR_MAX_RQE_ELEMENTS_PER_RQE;
	}

	if (rdma_protocol_iwarp(&dev->ibdev, 1))
		rc = qedr_iwarp_create_kernel_qp(dev, qp, &in_params,
						 n_sq_elems, n_rq_elems);
	else
		rc = qedr_roce_create_kernel_qp(dev, qp, &in_params,
						n_sq_elems, n_rq_elems);
	if (rc)
		qedr_cleanup_kernel(dev, qp);

	return rc;
}

static int qedr_free_qp_resources(struct qedr_dev *dev, struct qedr_qp *qp)
{
	struct qed_rdma_destroy_qp_out_params oparams;
	int rc = 0;

	if (!qp->recov_info.reset) { /* !QEDR_UPSTREAM_RECOVER_REMOVE_LINE */
		if (qp->qp_type != IB_QPT_GSI) {
			rc = dev->ops->rdma_destroy_qp(dev->rdma_ctx,
						       qp->qed_qp, &oparams);
			if (rc)
				return rc;
		}
	} /* !QEDR_UPSTREAM_RECOVER_REMOVE_LINE */

	if (qp->create_type == QEDR_QP_CREATE_USER)
		rc = qedr_cleanup_user(dev, qp);
	else
		rc = qedr_cleanup_kernel(dev, qp);

	return rc;
}

#ifdef _HAS_QP_ALLOCATION
int qedr_create_qp(struct ib_qp *ibqp,
		   struct ib_qp_init_attr *attrs,
		   struct ib_udata *udata)
#else
struct ib_qp *qedr_create_qp(struct ib_pd *ibpd,
			     struct ib_qp_init_attr *attrs,
			     struct ib_udata *udata)
#endif
{
	struct qedr_xrcd *xrcd = NULL;
	unsigned long spinlock_flags;
#ifdef _HAS_QP_ALLOCATION
	struct ib_pd *ibpd = ibqp->pd;
	struct qedr_pd *pd = get_qedr_pd(ibpd);
	struct qedr_dev *dev = get_qedr_dev(ibqp->device);
	struct qedr_qp *qp = get_qedr_qp(ibqp);
#else
	struct qedr_pd *pd = NULL;
	struct qedr_dev *dev;
	struct qedr_qp *qp;
	struct ib_qp *ibqp;
#endif
	int rc = 0;

#ifdef _HAS_QP_ALLOCATION
#ifdef _HAS_XRC_SUPPORT /* QEDR_UPSTREAM */
	if (attrs->qp_type == IB_QPT_XRC_TGT)
		xrcd = get_qedr_xrcd(attrs->xrcd);
	else
		pd = get_qedr_pd(ibpd);
#else
	pd = get_qedr_pd(ibpd);
#endif
#else
#ifdef _HAS_XRC_SUPPORT /* QEDR_UPSTREAM */
	if (attrs->qp_type == IB_QPT_XRC_TGT) {
		/* XRC TGT QP isn't associated with a PD so we get the device
		 * in a different way.
		 */
		xrcd = get_qedr_xrcd(attrs->xrcd);
		dev = get_qedr_dev(xrcd->ibxrcd.device);
	} else {
		pd = get_qedr_pd(ibpd);
		dev = get_qedr_dev(ibpd->device);
	}
#else
	pd = get_qedr_pd(ibpd);
	dev = get_qedr_dev(ibpd->device);
#endif
#endif

	DP_DEBUG(dev, QEDR_MSG_QP, "create qp: called from %s, pd=%p\n",
		 udata ? "user library" : "kernel", pd);

	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
#ifdef _HAS_QP_ALLOCATION
		return -EPERM;
#else
		return ERR_PTR(rc);
#endif

	rc = qedr_check_qp_attrs(ibpd, dev, attrs, udata);
	if (rc)
#ifdef _HAS_QP_ALLOCATION
		return rc;
#else
		return ERR_PTR(rc);
#endif

	DP_DEBUG(dev, QEDR_MSG_QP,
		 "create qp: called from %s, event_handler=%p, pd=%p sq_cq=%p, sq_icid=%d, rq_cq=%p, rq_icid=%d\n",
		 udata ? "user library" : "kernel", attrs->event_handler, pd,
		 get_qedr_cq(attrs->send_cq),
		 attrs->send_cq ? get_qedr_cq(attrs->send_cq)->icid : 0,
		 get_qedr_cq(attrs->recv_cq),
		 attrs->recv_cq ? get_qedr_cq(attrs->recv_cq)->icid : 0);

#ifndef _HAS_QP_ALLOCATION
	qp = kzalloc(sizeof(*qp), GFP_KERNEL);
	if (!qp)
		return ERR_PTR(-ENOMEM);
#endif

	qedr_set_common_qp_params(dev, qp, pd, attrs);

	if (attrs->qp_type == IB_QPT_GSI) {
#ifdef _HAS_QP_ALLOCATION
		return qedr_create_gsi_qp(dev, attrs, qp);
#else
		ibqp = qedr_create_gsi_qp(dev, attrs, qp);
		if (IS_ERR(ibqp))
			kfree(qp);
		return ibqp;
#endif
	}

	if (udata || xrcd)
		rc = qedr_create_user_qp(dev, qp, ibpd, udata, attrs);
	else
		rc = qedr_create_kernel_qp(dev, qp, ibpd, attrs);

	if (rc)
#ifdef _HAS_QP_ALLOCATION
		return rc;
#else
		goto out_free_qp;
#endif
	qp->ibqp.qp_num = qp->qp_id;

	if (rdma_protocol_iwarp(&dev->ibdev, 1)) {
		rc = qedr_idr_add(dev, &dev->qpidr, qp, qp->qp_id);
		if (rc)
			goto out_free_qp_resources;
	}

	if (qedr_qp_has_sq(qp) && qp->sq_cq) {
		spin_lock_irqsave(&qp->sq_cq->cq_lock, spinlock_flags);
		list_add_tail(&qp->cq_sq_list_entry, &qp->sq_cq->sq_qp_list);
		spin_unlock_irqrestore(&qp->sq_cq->cq_lock, spinlock_flags);
	}

	if (qedr_qp_has_rq(qp) && qp->rq_cq) {
		spin_lock_irqsave(&qp->rq_cq->cq_lock, spinlock_flags);
		list_add_tail(&qp->cq_rq_list_entry, &qp->rq_cq->rq_qp_list);
		spin_unlock_irqrestore(&qp->rq_cq->cq_lock, spinlock_flags);
	}

	qedr_recov_obj_add(dev, &qp->recov_info);

#ifdef _HAS_QP_ALLOCATION
	return 0;
#else
	return &qp->ibqp;
#endif

out_free_qp_resources:
	qedr_free_qp_resources(dev, qp);
#ifdef _HAS_QP_ALLOCATION
	return -EFAULT;
#else
out_free_qp:
	kfree(qp);

	return ERR_PTR(-EFAULT);
#endif
}

static enum ib_qp_state qedr_get_ibqp_state(enum qed_roce_qp_state qp_state)
{
	switch (qp_state) {
	case QED_ROCE_QP_STATE_RESET:
		return IB_QPS_RESET;
	case QED_ROCE_QP_STATE_INIT:
		return IB_QPS_INIT;
	case QED_ROCE_QP_STATE_RTR:
		return IB_QPS_RTR;
	case QED_ROCE_QP_STATE_RTS:
		return IB_QPS_RTS;
	case QED_ROCE_QP_STATE_SQD:
		return IB_QPS_SQD;
	case QED_ROCE_QP_STATE_ERR:
		return IB_QPS_ERR;
	case QED_ROCE_QP_STATE_SQE:
		return IB_QPS_SQE;
	}
	return IB_QPS_ERR;
}

static enum qed_roce_qp_state qedr_get_state_from_ibqp(
					enum ib_qp_state qp_state)
{
	switch (qp_state) {
	case IB_QPS_RESET:
		return QED_ROCE_QP_STATE_RESET;
	case IB_QPS_INIT:
		return QED_ROCE_QP_STATE_INIT;
	case IB_QPS_RTR:
		return QED_ROCE_QP_STATE_RTR;
	case IB_QPS_RTS:
		return QED_ROCE_QP_STATE_RTS;
	case IB_QPS_SQD:
		return QED_ROCE_QP_STATE_SQD;
	case IB_QPS_ERR:
		return QED_ROCE_QP_STATE_ERR;
	default:
		return QED_ROCE_QP_STATE_ERR;
	}
}

static void qedr_reset_qp_hwq_info(struct qedr_qp_hwq_info *qph)
{
	qed_chain_reset(&qph->pbl);
	qph->prod = 0;
	qph->cons = 0;
	qph->wqe_cons = 0;
	qph->db_data.data.value = cpu_to_le16(0);
}

static int qedr_update_qp_state(struct qedr_dev *dev,
				struct qedr_qp *qp,
				enum qed_roce_qp_state cur_state,
				enum qed_roce_qp_state new_state)
{
	int status = 0;

	if (new_state == cur_state)
		return 0;

	switch (cur_state) {
	case QED_ROCE_QP_STATE_RESET:
		switch (new_state) {
		case QED_ROCE_QP_STATE_INIT:
			qp->prev_wqe_size = 0;
			if (qedr_qp_has_sq(qp))
				qedr_reset_qp_hwq_info(&qp->sq);
			if (qedr_qp_has_rq(qp))
				qedr_reset_qp_hwq_info(&qp->rq);
			break;
		default:
			status = -EINVAL;
			break;
		};
		break;
	case QED_ROCE_QP_STATE_INIT:
		/* INIT->XXX */
		switch (new_state) {
		case QED_ROCE_QP_STATE_RTR:
			/* Update doorbell (in case post_recv was
			 * done before move to RTR)
			 */
			if (!qedr_qp_has_rq(qp))
				break;

			if (rdma_protocol_roce(&dev->ibdev, 1)) {
				wmb();
				writel(qp->rq.db_data.raw, qp->rq.db);
				/* Make sure write takes effect */
				mmiowb();
			}
			break;
		case QED_ROCE_QP_STATE_ERR:
			break;
		default:
			/* Invalid state change. */
			status = -EINVAL;
			break;
		};
		break;
	case QED_ROCE_QP_STATE_RTR:
		/* RTR->XXX */
		switch (new_state) {
		case QED_ROCE_QP_STATE_RTS:
			break;
		case QED_ROCE_QP_STATE_ERR:
			break;
		default:
			/* Invalid state change. */
			status = -EINVAL;
			break;
		};
		break;
	case QED_ROCE_QP_STATE_RTS:
		/* RTS->XXX */
		switch (new_state) {
		case QED_ROCE_QP_STATE_SQD:
			break;
		case QED_ROCE_QP_STATE_ERR:
			break;
		default:
			/* Invalid state change. */
			status = -EINVAL;
			break;
		};
		break;
	case QED_ROCE_QP_STATE_SQD:
		/* SQD->XXX */
		switch (new_state) {
		case QED_ROCE_QP_STATE_RTS:
		case QED_ROCE_QP_STATE_ERR:
			break;
		default:
			/* Invalid state change. */
			status = -EINVAL;
			break;
		};
		break;
	case QED_ROCE_QP_STATE_ERR:
		/* ERR->XXX */
		switch (new_state) {
		case QED_ROCE_QP_STATE_RESET:
			if ((qp->rq.prod != qp->rq.cons) ||
			    (qp->sq.prod != qp->sq.cons)) {
				DP_NOTICE(dev,
					  "Error->Reset with rq/sq not empty rq.prod=%x rq.cons=%x sq.prod=%x sq.cons=%x\n",
					  qp->rq.prod, qp->rq.cons, qp->sq.prod,
					  qp->sq.cons);
				status = -EINVAL;
			}
			break;
		default:
			status = -EINVAL;
			break;
		};
		break;
	default:
		status = -EINVAL;
		break;
	};

	return status;
}

int qedr_modify_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr,
		   int attr_mask, struct ib_udata *udata)
{
	struct qedr_qp *qp = get_qedr_qp(ibqp);
	struct qed_rdma_modify_qp_in_params qp_params = { 0 };
	struct qedr_dev *dev = get_qedr_dev(&qp->dev->ibdev);
	const struct ib_global_route *grh = rdma_ah_read_grh(&attr->ah_attr);
	enum ib_qp_state ib_old_qp_state, ib_new_qp_state;
	enum qed_roce_qp_state cur_state;
	int rc = 0;

	DP_DEBUG(dev, QEDR_MSG_QP,
		 "modify qp: qp %p attr_mask=0x%x, state=%d\n", qp, attr_mask,
		 attr->qp_state);

	ib_old_qp_state = qedr_get_ibqp_state(qp->state);
	if (attr_mask & IB_QP_STATE)
		ib_new_qp_state = attr->qp_state;
	else
		ib_new_qp_state = ib_old_qp_state;

	if (rdma_protocol_roce(&dev->ibdev, 1)) {
#ifdef DEFINE_QP_MODIFY_OK_WITHOUT_LLTYPE /* ! QEDR_UPSTREAM */
		if (!ib_modify_qp_is_ok(ib_old_qp_state,
					ib_new_qp_state,
					ibqp->qp_type,
					attr_mask)) {
#else
		if (!ib_modify_qp_is_ok(ib_old_qp_state, ib_new_qp_state,
					ibqp->qp_type, attr_mask,
					IB_LINK_LAYER_ETHERNET)) {
#endif
			DP_ERR(dev,
			       "modify qp: invalid attribute mask=0x%x specified for\n"
			       "qpn=0x%x of type=0x%x old_qp_state=0x%x, new_qp_state=0x%x\n",
			       attr_mask, qp->qp_id, ibqp->qp_type,
			       ib_old_qp_state, ib_new_qp_state);
			rc = -EINVAL;
			goto err;
		}
	}

	/* Translate the masks... */
	if (attr_mask & IB_QP_STATE) {
		SET_FIELD(qp_params.modify_flags,
			  QED_RDMA_MODIFY_QP_VALID_NEW_STATE, 1);
		qp_params.new_state = qedr_get_state_from_ibqp(attr->qp_state);
	}

	/* INTERNAL: TBD consider changing ecore to be a flag as well... */
	if (attr_mask & IB_QP_EN_SQD_ASYNC_NOTIFY)
		qp_params.sqd_async = true;

	if (attr_mask & IB_QP_PKEY_INDEX) {
		SET_FIELD(qp_params.modify_flags,
			  QED_ROCE_MODIFY_QP_VALID_PKEY, 1);
		if (attr->pkey_index >= QEDR_ROCE_PKEY_TABLE_LEN) {
			rc = -EINVAL;
			goto err;
		}

		qp_params.pkey = QEDR_ROCE_PKEY_DEFAULT;
	}

	if (attr_mask & IB_QP_QKEY)
		qp->qkey = attr->qkey;

	/* INTERNAL: TBD consider splitting in ecore.. */
	if (attr_mask & IB_QP_ACCESS_FLAGS) {
		SET_FIELD(qp_params.modify_flags,
			  QED_RDMA_MODIFY_QP_VALID_RDMA_OPS_EN, 1);
		qp_params.incoming_rdma_read_en = attr->qp_access_flags &
						  IB_ACCESS_REMOTE_READ;
		qp_params.incoming_rdma_write_en = attr->qp_access_flags &
						   IB_ACCESS_REMOTE_WRITE;
		qp_params.incoming_atomic_en = attr->qp_access_flags &
					       IB_ACCESS_REMOTE_ATOMIC;
	}

	if (attr_mask & (IB_QP_AV | IB_QP_PATH_MTU)) {
		if (rdma_protocol_iwarp(&dev->ibdev, 1))
			return -EINVAL;

		if (attr_mask & IB_QP_PATH_MTU) {
			if (attr->path_mtu < IB_MTU_256 ||
			    attr->path_mtu > IB_MTU_4096) {
				pr_err("error: Only MTU sizes of 256, 512, 1024, 2048 and 4096 are supported by RoCE\n");
				rc = -EINVAL;
				goto err;
			}
			qp->mtu = min(ib_mtu_enum_to_int(attr->path_mtu),
				      ib_mtu_enum_to_int(iboe_get_mtu
							 (dev->ndev->mtu)));
		}

		if (!qp->mtu) {
			qp->mtu =
			ib_mtu_enum_to_int(iboe_get_mtu(dev->ndev->mtu));
			pr_err("Fixing zeroed MTU to qp->mtu = %d\n", qp->mtu);
		}

		SET_FIELD(qp_params.modify_flags,
			  QED_ROCE_MODIFY_QP_VALID_ADDRESS_VECTOR, 1);

		qp_params.traffic_class_tos = grh->traffic_class;
		qp_params.flow_label = grh->flow_label;
		qp_params.hop_limit_ttl = grh->hop_limit;

		qp->sgid_idx = grh->sgid_index;

#if DEFINE_ROCE_GID_TABLE /* QEDR_UPSTREAM */
		rc = get_gid_info_from_table(ibqp, attr, attr_mask, &qp_params);
		if (rc) {
			DP_ERR(dev,
			       "modify qp: problems with GID index %d (rc=%d)\n",
			       grh->sgid_index, rc);
			return rc;
		}
#else
		get_gid_info(ibqp, attr, attr_mask, dev, qp, &qp_params);
		ether_addr_copy(qp_params.local_mac_addr,
				dev->ndev->dev_addr);
#endif

		rc = qedr_get_dmac(dev, &attr->ah_attr,
				   qp_params.remote_mac_addr);
		if (rc)
			return rc;

		qp_params.use_local_mac = true;

		DP_DEBUG(dev, QEDR_MSG_QP, "dgid=%x:%x:%x:%x\n",
			 qp_params.dgid.dwords[0], qp_params.dgid.dwords[1],
			 qp_params.dgid.dwords[2], qp_params.dgid.dwords[3]);
		DP_DEBUG(dev, QEDR_MSG_QP, "sgid=%x:%x:%x:%x\n",
			 qp_params.sgid.dwords[0], qp_params.sgid.dwords[1],
			 qp_params.sgid.dwords[2], qp_params.sgid.dwords[3]);
		DP_DEBUG(dev, QEDR_MSG_QP, "remote_mac=[%pM]\n",
			 qp_params.remote_mac_addr);

		qp_params.mtu = qp->mtu;
	}

	if (!qp_params.mtu) {
		/* Stay with current MTU */
		if (qp->mtu)
			qp_params.mtu = qp->mtu;
		else
			qp_params.mtu =
			    ib_mtu_enum_to_int(iboe_get_mtu(dev->ndev->mtu));
	}

	if (attr_mask & IB_QP_TIMEOUT) {
		SET_FIELD(qp_params.modify_flags,
			  QED_ROCE_MODIFY_QP_VALID_ACK_TIMEOUT, 1);

		/* The received timeout value is an exponent used like this:
		 *    "12.7.34 LOCAL ACK TIMEOUT
		 *    Value representing the transport (ACK) timeout for use by
		 *    the remote, expressed as: 4.096 * 2^timeout [usec]"
		 * The FW expects timeout in msec so we need to divide the usec
		 * result by 1000. We'll approximate 1000~2^10, and 4.096 ~ 2^2,
		 * so we get: 2^2 * 2^timeout / 2^10 = 2^(timeout - 8).
		 * The value of zero means infinite so we use a 'max_t' to make
		 * sure that sub 1 msec values will be configured as 1 msec.
		 */
		if (attr->timeout)
			qp_params.ack_timeout =
					1 << max_t(int, attr->timeout - 8, 0);
		else
			qp_params.ack_timeout = 0;

		qp->timeout = attr->timeout;
	}

	if (attr_mask & IB_QP_RETRY_CNT) {
		SET_FIELD(qp_params.modify_flags,
			  QED_ROCE_MODIFY_QP_VALID_RETRY_CNT, 1);
		qp_params.retry_cnt = attr->retry_cnt;
	}

	if (attr_mask & IB_QP_RNR_RETRY) {
		SET_FIELD(qp_params.modify_flags,
			  QED_ROCE_MODIFY_QP_VALID_RNR_RETRY_CNT, 1);
		qp_params.rnr_retry_cnt = attr->rnr_retry;
	}

	if (attr_mask & IB_QP_RQ_PSN) {
		SET_FIELD(qp_params.modify_flags,
			  QED_ROCE_MODIFY_QP_VALID_RQ_PSN, 1);
		qp_params.rq_psn = attr->rq_psn;
		qp->rq_psn = attr->rq_psn;
	}

	if (attr_mask & IB_QP_MAX_QP_RD_ATOMIC) {
		if (attr->max_rd_atomic > dev->attr.max_qp_req_rd_atomic_resc) {
			rc = -EINVAL;
			DP_ERR(dev,
			       "unsupported max_rd_atomic=%d, supported=%d\n",
			       attr->max_rd_atomic,
			       dev->attr.max_qp_req_rd_atomic_resc);
			goto err;
		}

		SET_FIELD(qp_params.modify_flags,
			  QED_RDMA_MODIFY_QP_VALID_MAX_RD_ATOMIC_REQ, 1);
		qp_params.max_rd_atomic_req = attr->max_rd_atomic;
	}

	if (attr_mask & IB_QP_MIN_RNR_TIMER) {
		SET_FIELD(qp_params.modify_flags,
			  QED_ROCE_MODIFY_QP_VALID_MIN_RNR_NAK_TIMER, 1);
		qp_params.min_rnr_nak_timer = attr->min_rnr_timer;
	}

	if (attr_mask & IB_QP_SQ_PSN) {
		SET_FIELD(qp_params.modify_flags,
			  QED_ROCE_MODIFY_QP_VALID_SQ_PSN, 1);
		qp_params.sq_psn = attr->sq_psn;
		qp->sq_psn = attr->sq_psn;
	}

	if (attr_mask & IB_QP_MAX_DEST_RD_ATOMIC) {
		if (attr->max_dest_rd_atomic >
		    dev->attr.max_qp_resp_rd_atomic_resc) {
			DP_ERR(dev,
			       "unsupported max_dest_rd_atomic=%d, supported=%d\n",
			       attr->max_dest_rd_atomic,
			       dev->attr.max_qp_resp_rd_atomic_resc);

			rc = -EINVAL;
			goto err;
		}

		SET_FIELD(qp_params.modify_flags,
			  QED_RDMA_MODIFY_QP_VALID_MAX_RD_ATOMIC_RESP, 1);
		qp_params.max_rd_atomic_resp = attr->max_dest_rd_atomic;
	}

	if (attr_mask & IB_QP_DEST_QPN) {
		SET_FIELD(qp_params.modify_flags,
			  QED_ROCE_MODIFY_QP_VALID_DEST_QP, 1);

		qp_params.dest_qp = attr->dest_qp_num;
		qp->dest_qp_num = attr->dest_qp_num;
	}
#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
	if (dev->insert_udp_src_port) {
		qp_params.udp_src_port = dev->new_udp_src_port++;
		if (!qp_params.udp_src_port)
			qp_params.udp_src_port = dev->new_udp_src_port++;
	}
#endif
	cur_state = qp->state;

	/* Update the QP state before the actual ramrod to prevent a race with
	 * fast path. Modifying the QP state to error will cause the device to
	 * flush the CQEs and while polling the flushed CQEs will considered as
	 * a potential issue if the QP isn't in error state.
	 */
	 if ((attr_mask & IB_QP_STATE) && (qp->qp_type != IB_QPT_GSI) &&
	     (!udata) && (qp_params.new_state == QED_ROCE_QP_STATE_ERR))
		qp->state = QED_ROCE_QP_STATE_ERR;

	if (!qp->recov_info.reset && qp->qp_type != IB_QPT_GSI)
		rc = dev->ops->rdma_modify_qp(dev->rdma_ctx,
					      qp->qed_qp, &qp_params);

	if (attr_mask & IB_QP_STATE) {
		if ((qp->qp_type != IB_QPT_GSI) && (!udata))
			rc = qedr_update_qp_state(dev, qp, cur_state,
						  qp_params.new_state);
		qp->state = qp_params.new_state;
	}

err:
	return rc;
}

static int qedr_to_ib_qp_acc_flags(struct qed_rdma_query_qp_out_params *params)
{
	int ib_qp_acc_flags = 0;

	if (params->incoming_rdma_write_en)
		ib_qp_acc_flags |= IB_ACCESS_REMOTE_WRITE;
	if (params->incoming_rdma_read_en)
		ib_qp_acc_flags |= IB_ACCESS_REMOTE_READ;
	if (params->incoming_atomic_en)
		ib_qp_acc_flags |= IB_ACCESS_REMOTE_ATOMIC;

	ib_qp_acc_flags |= IB_ACCESS_LOCAL_WRITE;
	return ib_qp_acc_flags;
}

#ifndef DEFINED_IB_MTU_TO_ENUM /* !QEDR_UPSTREAM */
static inline enum ib_mtu ib_mtu_int_to_enum(u16 mtu)
{
	switch (mtu) {
	case 256:
		return IB_MTU_256;
	case 512:
		return IB_MTU_512;
	case 1024:
		return IB_MTU_1024;
	case 2048:
		return IB_MTU_2048;
	case 4096:
		return IB_MTU_4096;
	default:
		return IB_MTU_1024;
	}
}
#endif

int qedr_query_qp(struct ib_qp *ibqp,
		  struct ib_qp_attr *qp_attr,
		  int attr_mask, struct ib_qp_init_attr *qp_init_attr)
{
	struct qed_rdma_query_qp_out_params params;
	struct qedr_qp *qp = get_qedr_qp(ibqp);
	struct qedr_dev *dev = qp->dev;
	int rc = 0;

	memset(&params, 0, sizeof(params));
	memset(qp_attr, 0, sizeof(*qp_attr));
	memset(qp_init_attr, 0, sizeof(*qp_init_attr));

	if (qp->recov_info.reset)
		goto skip_qed_params;

	if (qp->qed_qp)
		rc = dev->ops->rdma_query_qp(dev->rdma_ctx,
				qp->qed_qp, &params);
	if (!qp->qed_qp || rc) {
		if (qp->qp_type == IB_QPT_GSI)
			qp_attr->qp_state =
				qedr_get_ibqp_state(QED_ROCE_QP_STATE_RTS);
		goto skip_qed_params;
	}

	qp_attr->qp_state = qedr_get_ibqp_state(params.state);
	qp_attr->cur_qp_state = qedr_get_ibqp_state(params.state);
	qp_attr->path_mtu = ib_mtu_int_to_enum(params.mtu);
	qp_attr->rq_psn = params.rq_psn;
	qp_attr->sq_psn = params.sq_psn;
	qp_attr->dest_qp_num = params.dest_qp;

	qp_attr->qp_access_flags = qedr_to_ib_qp_acc_flags(&params);

	memcpy(&qp_attr->ah_attr.grh.dgid.raw[0], &params.dgid.bytes[0],
			sizeof(qp_attr->ah_attr.grh.dgid.raw));

	qp_attr->ah_attr.grh.flow_label = params.flow_label;
	qp_attr->ah_attr.grh.hop_limit = params.hop_limit_ttl;
	qp_attr->ah_attr.grh.traffic_class = params.traffic_class_tos;

	qp_attr->timeout = qp->timeout;
	qp_attr->rnr_retry = params.rnr_retry;
	qp_attr->retry_cnt = params.retry_cnt;
	qp_attr->min_rnr_timer = params.min_rnr_nak_timer;
	qp_attr->pkey_index = params.pkey_index;
	qp_attr->sq_draining = (params.state == QED_ROCE_QP_STATE_SQD) ? 1 : 0;
	qp_attr->max_dest_rd_atomic = params.max_dest_rd_atomic;
	qp_attr->max_rd_atomic = params.max_rd_atomic;
	qp_attr->en_sqd_async_notify = (params.sqd_async) ? 1 : 0;

skip_qed_params:
	qp_attr->path_mig_state = IB_MIG_MIGRATED;
	qp_attr->cap.max_send_wr = qp->sq.max_wr;
	qp_attr->cap.max_recv_wr = qp->rq.max_wr;
	qp_attr->cap.max_send_sge = qp->sq.max_sges;
	qp_attr->cap.max_recv_sge = qp->rq.max_sges;
	qp_attr->cap.max_inline_data = qp->max_inline_data;
	qp_init_attr->cap = qp_attr->cap;
	qp_attr->ah_attr.grh.sgid_index = qp->sgid_idx;
	qp_attr->ah_attr.ah_flags = IB_AH_GRH;
	rdma_ah_set_port_num(&qp_attr->ah_attr, 1);
	rdma_ah_set_sl(&qp_attr->ah_attr, 0);
	qp_attr->port_num = 1;
	rdma_ah_set_path_bits(&qp_attr->ah_attr, 0);
	rdma_ah_set_static_rate(&qp_attr->ah_attr, 0);
	qp_attr->alt_pkey_index = 0;
	qp_attr->alt_port_num = 0;
	qp_attr->alt_timeout = 0;
	memset(&qp_attr->alt_ah_attr, 0, sizeof(qp_attr->alt_ah_attr));

	DP_DEBUG(dev, QEDR_MSG_QP, "QEDR_QUERY_QP: max_inline_data=%d\n",
		 qp_attr->cap.max_inline_data);

	return rc;
}

int qedr_destroy_qp(struct ib_qp *ibqp
		    COMPAT_DESTROY_QP_UDATA(struct ib_udata *udata))
{
	struct qedr_qp *qp = get_qedr_qp(ibqp);
	struct qedr_dev *dev = qp->dev;
	struct ib_qp_attr attr;
	unsigned long flags;
	int attr_mask = 0;
	int rc = 0;

	/* If this QP is marked as reset, delay this operation until recovery
	 * is complete.
	 */
	if (dev->recov_info.recov_in_prog)
		wait_for_completion(&dev->recov_info.recov_comp);

	if (qp->recov_info.reset)
		goto recovery_skip;

	/* TBD: locking and QP Flushing... */
	DP_DEBUG(dev, QEDR_MSG_QP, "destroy qp: destroying %p, qp type=%d\n",
		 qp, qp->qp_type);

	if (rdma_protocol_roce(&dev->ibdev, 1)) {
		if ((qp->state != QED_ROCE_QP_STATE_RESET) &&
		    (qp->state != QED_ROCE_QP_STATE_ERR) &&
		    (qp->state != QED_ROCE_QP_STATE_INIT)) {

			attr.qp_state = IB_QPS_ERR;
			attr_mask |= IB_QP_STATE;

			/* Change the QP state to ERROR */
			qedr_modify_qp(ibqp, &attr, attr_mask, NULL);
		}
	} else {
		/* If connection establishment started the WAIT_FOR_CONNECT
		 * bit will be on and we need to Wait for the establishment
		 * to complete before destroying the qp.
		 */
		if (test_and_set_bit(QEDR_IWARP_CM_WAIT_FOR_CONNECT,
				     &qp->iwarp_cm_flags))
			wait_for_completion(&qp->iwarp_cm_comp);
		/* If graceful disconnect started, the WAIT_FOR_DISCONNECT
		 * bit will be on, and we need to wait for the disconnect to
		 * complete before continuing. We can use the same completion,
		 * iwarp_cm_comp, since this is the only place that waits for
		 * this completion and it is sequential. In addition,
		 * disconnect can't occur before the connection is fully
		 * established, therefore if WAIT_FOR_DISCONNECT is on it
		 * means WAIT_FOR_CONNECT is also on and the completion for
		 * CONNECT already occurred.
		 */
		if (test_and_set_bit(QEDR_IWARP_CM_WAIT_FOR_DISCONNECT,
				     &qp->iwarp_cm_flags))
			wait_for_completion(&qp->iwarp_cm_comp);
	}

recovery_skip:
	if (qp->qp_type == IB_QPT_GSI)
		qedr_destroy_gsi_qp(dev);

	qp->sig = ~qp->sig;

	/* We need to remove the entry from the idr before we release the
	 * qp_id to avoid a race of the qp_id being reallocated and failing
	 * on idr_add
	 */
	if (rdma_protocol_iwarp(&dev->ibdev, 1))
		qedr_idr_remove(dev, &dev->qpidr, qp->qp_id);

	if (qedr_qp_has_sq(qp) && qp->sq_cq) {
		spin_lock_irqsave(&qp->sq_cq->cq_lock, flags);
		list_del(&qp->cq_sq_list_entry);
		spin_unlock_irqrestore(&qp->sq_cq->cq_lock, flags);
	}

	if (qedr_qp_has_rq(qp) && qp->rq_cq) {
		spin_lock_irqsave(&qp->rq_cq->cq_lock, flags);
		list_del(&qp->cq_rq_list_entry);
		spin_unlock_irqrestore(&qp->rq_cq->cq_lock, flags);
	}

	qedr_free_qp_resources(dev, qp);

	if (qp->qp_type != IB_QPT_GSI)
		qedr_recov_obj_del(dev, &qp->recov_info);

#ifdef _HAS_QP_ALLOCATION
	if (rdma_protocol_iwarp(&dev->ibdev, 1)) {
		qedr_iw_qp_rem_ref(&qp->ibqp);
		wait_for_completion(&qp->qp_rel_comp);
	}
#else
	if (rdma_protocol_iwarp(&dev->ibdev, 1))
		qedr_iw_qp_rem_ref(&qp->ibqp);
	else
		kfree(qp);
#endif

	return rc;
}

COMPAT_CREATE_AH_DECLARE_RET
qedr_create_ah(COMPAT_CREATE_AH_IBPD(struct ib_pd *ibpd)
	       COMPAT_CREATE_AH_AH(struct ib_ah *ibah)
	       COMPAT_CREATE_AH_ATTR(attr)
	       COMPAT_CREATE_AH_FLAGS(u32 flags)
	       COMPAT_CREATE_AH_UDATA(struct ib_udata *udata))
{
	struct qedr_ah *ah;

#ifdef _HAS_AH_ALLOCATION
	ah = get_qedr_ah(ibah);
#else
	ah = kzalloc(sizeof(*ah), GFP_ATOMIC);
	if (!ah)
		return ERR_PTR(-ENOMEM);
#endif

	rdma_copy_ah_attr(&ah->attr, COMPAT_AH_ATTR(attr));

	COMPAT_HAS_AH_RET(return 0;)
	COMPAT_NO_AH_RET(return &ah->ibah;)
}

COMPAT_DESTROY_AH_DECLARE_RET
qedr_destroy_ah(struct ib_ah *ibah
		COMPAT_DESTROY_AH_FLAGS(u32 flags))
{
	struct qedr_ah *ah = get_qedr_ah(ibah);

	rdma_destroy_ah_attr(&ah->attr);
#ifndef _HAS_AH_ALLOCATION
	kfree(ah);
#endif
	COMPAT_DESTROY_AH_RET(return 0;)
}

static void free_mr_info(struct qedr_dev *dev, struct mr_info *info)
{
	struct qedr_pbl *pbl, *tmp;

	if (info->pbl_table)
		list_add_tail(&info->pbl_table->list_entry,
			      &info->free_pbl_list);

	if (!list_empty(&info->inuse_pbl_list))
		list_splice(&info->inuse_pbl_list, &info->free_pbl_list);

	list_for_each_entry_safe(pbl, tmp, &info->free_pbl_list, list_entry) {
		list_del(&pbl->list_entry);
		qedr_free_pbl(dev, &info->pbl_info, pbl);
	}
}

static int init_mr_info(struct qedr_dev *dev, struct mr_info *info,
			size_t page_list_len, bool two_layered)
{
	struct qedr_pbl *tmp;
	int rc;

	INIT_LIST_HEAD(&info->free_pbl_list);
	INIT_LIST_HEAD(&info->inuse_pbl_list);

	rc = qedr_prepare_pbl_tbl(dev, &info->pbl_info,
				  page_list_len, two_layered);
	if (rc)
		goto done;

	info->pbl_table = qedr_alloc_pbl_tbl(dev, &info->pbl_info, GFP_KERNEL);
	if (IS_ERR(info->pbl_table)) {
		rc = PTR_ERR(info->pbl_table);
		goto done;
	}

	DP_DEBUG(dev, QEDR_MSG_MR, "pbl_table_pa = %pa\n",
		 &info->pbl_table->pa);

	/* in usual case we use 2 PBLs, so we add one to free
	 * list and allocating another one
	 */
	tmp = qedr_alloc_pbl_tbl(dev, &info->pbl_info, GFP_KERNEL);
	if (IS_ERR(tmp)) {
		DP_DEBUG(dev, QEDR_MSG_MR, "Extra PBL is not allocated\n");
		/* INTERNAL: It is OK if second allocation fails, so rc remains 0 */
		goto done;
	}

	list_add_tail(&tmp->list_entry, &info->free_pbl_list);

	DP_DEBUG(dev, QEDR_MSG_MR, "extra pbl_table_pa = %pa\n", &tmp->pa);

done:
	if (rc)
		free_mr_info(dev, info);

	return rc;
}

#ifdef DEFINE_USER_NO_MR_ID /* QEDR_UPSTREAM */
struct ib_mr *qedr_reg_user_mr(struct ib_pd *ibpd, u64 start, u64 len,
			       u64 usr_addr, int acc, struct ib_udata *udata)
#else
struct ib_mr *qedr_reg_user_mr(struct ib_pd *ibpd, u64 start, u64 len,
			       u64 usr_addr, int acc, struct ib_udata *udata,
			       int mr_id)
#endif
{
	struct qedr_dev *dev = get_qedr_dev(ibpd->device);
#ifdef _IB_UMEM_HUGETLB
	struct vm_area_struct *vma;
	struct hstate *h = NULL;
#endif
	struct qedr_mr *mr;
	struct qedr_pd *pd;
	int rc = -ENOMEM;
	u64 page_mask;

	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
		return ERR_PTR(rc);

	pd = get_qedr_pd(ibpd);
	DP_DEBUG(dev, QEDR_MSG_MR,
		 "qedr_register user mr pd = %d start = %lld, len = %lld, usr_addr = %lld, acc = %d\n",
		 pd->pd_id, start, len, usr_addr, acc);

	if (acc & IB_ACCESS_REMOTE_WRITE && !(acc & IB_ACCESS_LOCAL_WRITE))
		return ERR_PTR(-EINVAL);

	mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (!mr)
		return ERR_PTR(rc);

	mr->dev = dev;
	mr->type = QEDR_MR_USER;
	page_mask = PAGE_MASK;

	rc = qedr_gdr_ib_umem_get(ibpd->uobject->context, start, len, acc, 0, mr);
	if (rc)
		goto err0;
	if (mr->gdr) /* This is GDR memory -> skip getting umem... */
		goto gdr_cont;

	mr->umem = compat_ib_umem_get(ibpd->uobject->context, udata,
				      &dev->ibdev, start, len, acc, 0);
	if (IS_ERR(mr->umem)) {
		rc = -EFAULT;
		goto err0;
	}

#ifdef _IB_UMEM_HUGETLB
	if (mr->umem->hugetlb) {
		vma = find_vma(current->mm, start);
		h = hstate_vma(vma);
		page_mask = huge_page_mask(h);
	}
#endif

gdr_cont:
	rc = COMPAT_INIT_MR_INFO(mr);
	if (rc)
		goto err1;

	qedr_populate_pbls(dev, mr->umem, mr->info.pbl_table, page_mask,
			   &mr->info.pbl_info,
#ifdef DEFINE_IB_UMEM_NO_PAGE_PARAM
			    PAGE_SHIFT);
#else
#if DEFINE_IB_UMEM_PAGE_SHIFT /* QEDR_UPSTREAM */
			   mr->umem->page_shift);
#else
			   ilog2(mr->umem->page_size));
#endif
#endif

	rc = dev->ops->rdma_alloc_tid(dev->rdma_ctx, &mr->hw_mr.itid);
	if (rc) {
		if (rc != -EINVAL)
			DP_ERR(dev, "roce alloc tid returned error %d\n", rc);

		goto err1;
	}

	/* Index only, 18 bit long, lkey = itid << 8 | key */
	mr->hw_mr.tid_type = QED_RDMA_TID_REGISTERED_MR;
	mr->hw_mr.key = 0;
	mr->hw_mr.pd = pd->pd_id;
	mr->hw_mr.local_read = 1;
	mr->hw_mr.local_write = (acc & IB_ACCESS_LOCAL_WRITE) ? 1 : 0;
	mr->hw_mr.remote_read = (acc & IB_ACCESS_REMOTE_READ) ? 1 : 0;
	mr->hw_mr.remote_write = (acc & IB_ACCESS_REMOTE_WRITE) ? 1 : 0;
	mr->hw_mr.remote_atomic = (acc & IB_ACCESS_REMOTE_ATOMIC) ? 1 : 0;
#ifdef _HAS_MW_SUPPORT /* QEDR_UPSTREAM */
	mr->hw_mr.mw_bind = (acc & IB_ACCESS_MW_BIND) ? 1 : 0;
#endif
	mr->hw_mr.pbl_ptr = mr->info.pbl_table[0].pa;
	mr->hw_mr.pbl_two_level = mr->info.pbl_info.two_layered;
	mr->hw_mr.pbl_page_size_log = ilog2(mr->info.pbl_info.pbl_size);
#ifdef DEFINE_IB_UMEM_NO_PAGE_PARAM
	mr->hw_mr.page_size_log = PAGE_SHIFT;
#else
#if DEFINE_IB_UMEM_PAGE_SHIFT /* QEDR_UPSTREAM */
	mr->hw_mr.page_size_log = mr->umem->page_shift;
#else
	mr->hw_mr.page_size_log = ilog2(mr->umem->page_size);          /* for the MR pages */
#endif
#endif
#ifdef DEFINE_UMEM_ADDRESS_TO_OFFSET /* QEDR_UPSTREAM */
	mr->hw_mr.fbo = ib_umem_offset(mr->umem);
#else
	mr->hw_mr.fbo = mr->umem->offset;
#endif

#ifdef _IB_UMEM_HUGETLB
	if (mr->umem->hugetlb) {
		mr->hw_mr.page_size_log = ilog2(huge_page_size(h));
		mr->hw_mr.fbo = start & ~page_mask;
	}
#endif
	mr->hw_mr.length = len;
	mr->hw_mr.vaddr = usr_addr;
	mr->hw_mr.zbva = false; /* INTERNAL: TBD figure when this should be true */
	mr->hw_mr.phy_mr = false; /* INTERNAL: Fast MR - True, Regular Register False */
	mr->hw_mr.dma_mr = false;

	rc = dev->ops->rdma_register_tid(dev->rdma_ctx, &mr->hw_mr);
	if (rc) {
		DP_ERR(dev, "roce register tid returned an error %d\n", rc);
		goto err2;
	}

	mr->ibmr.lkey = mr->hw_mr.itid << 8 | mr->hw_mr.key;
	if (mr->hw_mr.remote_write || mr->hw_mr.remote_read ||
	    mr->hw_mr.remote_atomic)
		mr->ibmr.rkey = mr->hw_mr.itid << 8 | mr->hw_mr.key;

	DP_DEBUG(dev, QEDR_MSG_MR, "register user mr lkey: %x\n",
		 mr->ibmr.lkey);

	qedr_gdr_reg_user_complete(mr);

	qedr_recov_obj_add(dev, &mr->recov_info);

	return &mr->ibmr;

err2:
	dev->ops->rdma_free_tid(dev->rdma_ctx, mr->hw_mr.itid);
err1:
	qedr_free_pbl(dev, &mr->info.pbl_info, mr->info.pbl_table);
err0:
	kfree(mr);
	return ERR_PTR(rc);
}

int qedr_dereg_mr(struct ib_mr *ib_mr
		  COMPAT_DEREG_MR_UDATA(struct ib_udata *udata))
{
	struct qedr_mr *mr = get_qedr_mr(ib_mr);
	struct qedr_dev *dev = get_qedr_dev(ib_mr->device);
	int rc = 0;

	if (mr->gdr)
		return qedr_gdr_dereg(mr);

	if (!mr->recov_info.reset) { /* !QEDR_UPSTREAM_RECOVER_REMOVE_LINE */
		rc = dev->ops->rdma_deregister_tid(dev->rdma_ctx, mr->hw_mr.itid);
		if (rc)
			return rc;

		dev->ops->rdma_free_tid(dev->rdma_ctx, mr->hw_mr.itid);
	} /* !QEDR_UPSTREAM_RECOVER_REMOVE_LINE */

	if (mr->type != QEDR_MR_DMA)
		free_mr_info(dev, &mr->info);

	/* it could be user registered memory. */
	if (mr->umem)
		ib_umem_release(mr->umem);

	qedr_recov_obj_del(dev, &mr->recov_info);

	kfree(mr);

	return rc;
}

#ifdef _HAS_MW_SUPPORT /* QEDR_UPSTREAM */
#ifdef _HAS_ALLOC_MW_V2 /* !QEDR_UPSTREAM */
struct ib_mw *qedr_alloc_mw(struct ib_pd *ibpd,  enum ib_mw_type type,
		struct ib_udata *udata)
#elif defined(_HAS_ALLOC_MW_V3) /* QEDR_UPSTREAM */
int qedr_alloc_mw(struct ib_mw *ibmw, struct ib_udata *udata)
#else
struct ib_mw *qedr_alloc_mw(struct ib_pd *ibpd,  enum ib_mw_type type)
#endif
{
	int mw_type;
#ifdef _HAS_ALLOC_MW_V3
	struct qedr_dev *dev = get_qedr_dev(ibmw->device);
#else
	struct qedr_dev *dev = get_qedr_dev(ibpd->device);
#endif
	struct qedr_mw *mw;
	struct qedr_pd *pd;
	int rc = -ENOMEM;

	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
		COMPAT_ALLOC_MW_RET(rc);

#ifdef _HAS_ALLOC_MW_V3
	pd = get_qedr_pd(ibmw->pd);
	mw_type = ibmw->type;
#else
	pd = get_qedr_pd(ibpd);
	mw_type = type;
#endif
	DP_DEBUG(dev, QEDR_MSG_MR,
		 "qedr_alloc user mw pd = %d mw type %d\n", pd->pd_id, mw_type);
	mw = kzalloc(sizeof(*mw), GFP_KERNEL);
	if (!mw)
		COMPAT_ALLOC_MW_RET(rc);

	rc = dev->ops->rdma_alloc_tid(dev->rdma_ctx, &mw->hw_mw.itid);
	if (rc) {
		if (rc != -EINVAL)
			DP_ERR(dev, "roce alloc tid returned error %d\n", rc);

		goto err0;
	}

	mw->hw_mw.tid_type = QED_RDMA_TID_MW;
	mw->hw_mw.pd = pd->pd_id;
	mw->hw_mw.pbl_page_size_log = 12; /* WA */

	rc = dev->ops->rdma_register_tid(dev->rdma_ctx, &mw->hw_mw);
	if (rc) {
		DP_ERR(dev, "roce register tid returned an error %d\n", rc);
		goto err1;
	}

	mw->ibmw.rkey = mw->hw_mw.itid << 8 | mw->hw_mw.key;

	qedr_recov_obj_add(dev, &mw->recov_info);

	DP_DEBUG(dev, QEDR_MSG_MR, "allocated mw rkey: %x\n",
		 mw->ibmw.rkey);

#ifdef _HAS_ALLOC_MW_V3
	return rc;
#else
	return &mw->ibmw;
#endif

err1:
	dev->ops->rdma_free_tid(dev->rdma_ctx, mw->hw_mw.itid);
err0:
	kfree(mw);
	COMPAT_ALLOC_MW_RET(rc);
}

int qedr_dealloc_mw(struct ib_mw *ib_mw)
{
	struct qedr_dev *dev = get_qedr_dev(ib_mw->device);
	struct qedr_mw *mw = get_qedr_mw(ib_mw);
	int rc = 0;

	if (!mw->recov_info.reset) {
		rc = dev->ops->rdma_deregister_tid(dev->rdma_ctx,
						   mw->hw_mw.itid);
		if (rc)
			return rc;

		dev->ops->rdma_free_tid(dev->rdma_ctx, mw->hw_mw.itid);
	}

	qedr_recov_obj_del(dev, &mw->recov_info);

	kfree(mw);

	return rc;
}
#endif

static struct qedr_mr *__qedr_alloc_mr(struct ib_pd *ibpd,
				       int max_page_list_len)
{
	struct qedr_pd *pd = get_qedr_pd(ibpd);
	struct qedr_dev *dev = get_qedr_dev(ibpd->device);
	struct qedr_mr *mr;
	int rc = -ENOMEM;

	DP_DEBUG(dev, QEDR_MSG_MR,
		 "qedr_alloc_frmr pd = %d max_page_list_len= %d\n", pd->pd_id,
		 max_page_list_len);

	mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (!mr)
		return ERR_PTR(rc);

	mr->dev = dev;
	mr->type = QEDR_MR_FRMR;

	/* INTERNAL: Allocate an MR, possibly dual layer */
	rc = init_mr_info(dev, &mr->info, max_page_list_len, 1);
	if (rc)
		goto err0;

	rc = dev->ops->rdma_alloc_tid(dev->rdma_ctx, &mr->hw_mr.itid);
	if (rc) {
		if (rc != -EINVAL)
			DP_ERR(dev, "roce alloc tid returned error %d\n", rc);

		goto err1;
	}

	/* Index only, 18 bit long, lkey = itid << 8 | key */
	mr->hw_mr.tid_type = QED_RDMA_TID_FMR;
	mr->hw_mr.key = 0;
	mr->hw_mr.pd = pd->pd_id;
	mr->hw_mr.local_read = 1;
	mr->hw_mr.local_write = 0;
	mr->hw_mr.remote_read = 0;
	mr->hw_mr.remote_write = 0;
	mr->hw_mr.remote_atomic = 0;
	mr->hw_mr.mw_bind = false;
	mr->hw_mr.pbl_ptr = 0;	/* INTERNAL: Will be supplied during post */
	mr->hw_mr.pbl_two_level = mr->info.pbl_info.two_layered;
	mr->hw_mr.pbl_page_size_log = ilog2(mr->info.pbl_info.pbl_size);
	mr->hw_mr.fbo = 0;
	mr->hw_mr.length = 0;
	mr->hw_mr.vaddr = 0;
	mr->hw_mr.zbva = false; /* INTERNAL: TBD figure when this should be true */
	mr->hw_mr.phy_mr = true; /* INTERNAL: Fast MR - True, Regular Register False */
	mr->hw_mr.dma_mr = false;

	rc = dev->ops->rdma_register_tid(dev->rdma_ctx, &mr->hw_mr);
	if (rc) {
		DP_ERR(dev, "roce register tid returned an error %d\n", rc);
		goto err2;
	}

	mr->ibmr.lkey = mr->hw_mr.itid << 8 | mr->hw_mr.key;
	mr->ibmr.rkey = mr->ibmr.lkey;

	qedr_recov_obj_add(dev, &mr->recov_info);
	DP_DEBUG(dev, QEDR_MSG_MR, "alloc frmr: %x\n", mr->ibmr.lkey);

	return mr;

err2:
	dev->ops->rdma_free_tid(dev->rdma_ctx, mr->hw_mr.itid);
err1:
	qedr_free_pbl(dev, &mr->info.pbl_info, mr->info.pbl_table);
err0:
	kfree(mr);
	return ERR_PTR(rc);
}

#ifdef DEFINE_ALLOC_MR /* QEDR_UPSTREAM */
struct ib_mr *qedr_alloc_mr(struct ib_pd *ibpd,
			    enum ib_mr_type mr_type, u32 max_num_sg
			    COMPAT_ALLOC_MR_UDATA(struct ib_udata *udata))
{
	struct qedr_dev *dev;
	struct qedr_mr *mr;
	int rc;

	dev = get_qedr_dev(ibpd->device);

	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
		return ERR_PTR(rc);

	if (mr_type != IB_MR_TYPE_MEM_REG)
		return ERR_PTR(-EINVAL);

	mr = __qedr_alloc_mr(ibpd, max_num_sg);

	if (IS_ERR(mr))
		return ERR_PTR(-EINVAL);

	return &mr->ibmr;
}

static int qedr_set_page(struct ib_mr *ibmr, u64 addr)
{
	struct qedr_mr *mr = get_qedr_mr(ibmr);
	struct qedr_pbl *pbl_table;
	struct regpair *pbe;
	u32 pbes_in_page;

	if (unlikely(mr->npages == mr->info.pbl_info.num_pbes)) {
		DP_ERR(mr->dev, "qedr_set_page failes when %d\n", mr->npages);
		return -ENOMEM;
	}

	DP_DEBUG(mr->dev, QEDR_MSG_MR, "qedr_set_page pages[%d] = 0x%llx\n",
		 mr->npages, addr);

	pbes_in_page = mr->info.pbl_info.pbl_size / sizeof(u64);
	pbl_table = mr->info.pbl_table + (mr->npages / pbes_in_page);
	pbe = (struct regpair *)pbl_table->va;
	pbe +=  mr->npages % pbes_in_page;
	pbe->lo = cpu_to_le32((u32)addr);
	pbe->hi = cpu_to_le32((u32)upper_32_bits(addr));

	mr->npages++;

	return 0;
}
#else
struct ib_mr *qedr_alloc_frmr(struct ib_pd *ibpd, int max_page_list_len)
{
	struct qedr_dev *dev = get_qedr_dev(ibpd->device);
	struct qedr_mr *mr;
	int rc;

	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
		return ERR_PTR(rc);

	mr = __qedr_alloc_mr(ibpd, max_page_list_len);

	if (IS_ERR(mr))
		return ERR_PTR(-EINVAL);

	return &mr->ibmr;
}

void qedr_free_frmr_page_list(struct ib_fast_reg_page_list *page_list)
{
	struct qedr_fast_reg_page_list *frmr_list =
		get_qedr_frmr_list(page_list);

	free_mr_info(frmr_list->dev, &frmr_list->info);

	kfree(frmr_list->ibfrpl.page_list);
	kfree(frmr_list);
}

struct ib_fast_reg_page_list *qedr_alloc_frmr_page_list(struct ib_device
							*ibdev,
							int page_list_len)
{
	struct qedr_fast_reg_page_list *frmr_list = NULL;
	struct qedr_dev *dev = get_qedr_dev(ibdev);
	int size = page_list_len * sizeof(u64);
	int rc = -ENOMEM;

	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
		goto err;

	frmr_list = kzalloc(sizeof(*frmr_list), GFP_KERNEL);
	if (!frmr_list)
		goto err;

	frmr_list->dev = dev;
	frmr_list->ibfrpl.page_list = kzalloc(size, GFP_KERNEL);
	if (!frmr_list->ibfrpl.page_list)
		goto err0;

	rc = init_mr_info(dev, &frmr_list->info, page_list_len,
			  1 /* allow dual layer pbl */);
	if (rc)
		goto err1;

	return &frmr_list->ibfrpl;

err1:
	kfree(frmr_list->ibfrpl.page_list);
err0:
	kfree(frmr_list);
err:
	return ERR_PTR(rc);
}
#endif

static void handle_completed_mrs(struct qedr_dev *dev, struct mr_info *info)
{
	int work = info->completed - info->completed_handled - 1;

	DP_DEBUG(dev, QEDR_MSG_MR, "Special FMR work = %d\n", work);
	while (work-- > 0 && !list_empty(&info->inuse_pbl_list)) {
		struct qedr_pbl *pbl;

		/* Free all the page list that are possible to be freed
		 * (all the ones that were invalidated), under the assumption
		 * that if an FMR was completed successfully that means that
		 * if there was an invalidate operation before it also ended
		 */
		pbl = list_first_entry(&info->inuse_pbl_list,
				       struct qedr_pbl, list_entry);
		list_move_tail(&pbl->list_entry, &info->free_pbl_list);
		info->completed_handled++;
	}
}
#ifdef DEFINE_ALLOC_MR /* QEDR_UPSTREAM */
#ifdef DEFINE_MAP_MR_SG /* QEDR_UPSTREAM */
#ifdef DEFINE_MAP_MR_SG_OFFSET /* QEDR_UPSTREAM */

int qedr_map_mr_sg(struct ib_mr *ibmr, struct scatterlist *sg,
		   int sg_nents, unsigned int *sg_offset)
#else
#ifndef DEFINE_MAP_MR_SG_UNSIGNED
int qedr_map_mr_sg(struct ib_mr *ibmr, struct scatterlist *sg,
		   int sg_nents)
#else
int qedr_map_mr_sg(struct ib_mr *ibmr, struct scatterlist *sg,
		   unsigned int sg_nents)
#endif
#endif
{
	struct qedr_mr *mr = get_qedr_mr(ibmr);
	struct qedr_dev *dev = mr->dev;
	int rc;

	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
		return rc;

	mr->npages = 0;

	handle_completed_mrs(mr->dev, &mr->info);
#ifdef DEFINE_MAP_MR_SG_OFFSET /* QEDR_UPSTREAM */
	return ib_sg_to_pages(ibmr, sg, sg_nents, NULL, qedr_set_page);
#else
	return ib_sg_to_pages(ibmr, sg, sg_nents, qedr_set_page);
#endif
}
#endif
#endif

struct ib_mr *qedr_get_dma_mr(struct ib_pd *ibpd, int acc)
{
	struct qedr_dev *dev = get_qedr_dev(ibpd->device);
	struct qedr_pd *pd = get_qedr_pd(ibpd);
	struct qedr_mr *mr;
	int rc;

	rc = qedr_recov_check_state(dev, __func__);
	if (unlikely(rc))
		return ERR_PTR(rc);

#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
	if (acc & IB_ACCESS_MW_BIND)
		DP_ERR(dev, "Unsupported access flags received for dma mr\n");
#endif

	mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (!mr)
		return ERR_PTR(-ENOMEM);

	mr->dev = dev;
	mr->type = QEDR_MR_DMA;

	rc = dev->ops->rdma_alloc_tid(dev->rdma_ctx, &mr->hw_mr.itid);
	if (rc) {
		if (rc != -EINVAL)
			DP_ERR(dev, "roce alloc tid returned error %d\n", rc);

		goto err1;
	}

	/* index only, 18 bit long, lkey = itid << 8 | key */
	mr->hw_mr.tid_type = QED_RDMA_TID_REGISTERED_MR;
	mr->hw_mr.pd = pd->pd_id;
	mr->hw_mr.local_read = 1;
	mr->hw_mr.local_write = (acc & IB_ACCESS_LOCAL_WRITE) ? 1 : 0;
	mr->hw_mr.remote_read = (acc & IB_ACCESS_REMOTE_READ) ? 1 : 0;
	mr->hw_mr.remote_write = (acc & IB_ACCESS_REMOTE_WRITE) ? 1 : 0;
	mr->hw_mr.remote_atomic = (acc & IB_ACCESS_REMOTE_ATOMIC) ? 1 : 0;
	mr->hw_mr.dma_mr = true;

	rc = dev->ops->rdma_register_tid(dev->rdma_ctx, &mr->hw_mr);
	if (rc) {
		DP_ERR(dev, "roce register tid returned an error %d\n", rc);
		goto err2;
	}

	mr->ibmr.lkey = mr->hw_mr.itid << 8 | mr->hw_mr.key;
	if (mr->hw_mr.remote_write || mr->hw_mr.remote_read ||
	    mr->hw_mr.remote_atomic)
		mr->ibmr.rkey = mr->hw_mr.itid << 8 | mr->hw_mr.key;

	qedr_recov_obj_add(dev, &mr->recov_info);

	DP_DEBUG(dev, QEDR_MSG_MR, "get dma mr: lkey = %x\n", mr->ibmr.lkey);

	return &mr->ibmr;

err2:
	dev->ops->rdma_free_tid(dev->rdma_ctx, mr->hw_mr.itid);
err1:
	kfree(mr);
	return ERR_PTR(rc);
}

static inline int qedr_wq_is_full(struct qedr_qp_hwq_info *wq)
{
	return (((wq->prod + 1) % wq->max_wr) == wq->cons);
}

static int sge_data_len(struct ib_sge *sg_list, int num_sge)
{
	int i, len = 0;

	for (i = 0; i < num_sge; i++)
		len += sg_list[i].length;

	return len;
}

static void swap_wqe_data64(u64 *p)
{
	int i;

	for (i = 0; i < QEDR_SQE_ELEMENT_SIZE / sizeof(u64); i++, p++)
		*p = cpu_to_be64(cpu_to_le64(*p));
}

#define QEDR_DPM_LIMIT          (8192)

static inline void qedr_init_dpm_info(struct qedr_dev *dev,
				      struct qedr_qp *qp,
				      IB_CONST struct ib_send_wr *wr,
				      struct qedr_dpm *dpm,
				      u32 data_size)
{
	if (!dev->kernel_ldpm_enabled)
		return;

	/* LDPM will be used only if sq is empty */
	if (qed_chain_get_elem_left_u32(&qp->sq.pbl) != qp->sq.pbl.size)
		return;

	/* LDPM should also supports ATOMIC opcodes, but for now it is not
	 * implemented
	 */
	if ((wr->opcode <= IB_WR_RDMA_READ ||
	     wr->opcode == IB_WR_SEND_WITH_INV ||
	     wr->opcode == IB_WR_RDMA_READ_WITH_INV) &&
	    data_size <= QEDR_DPM_LIMIT && dev->dpm_enabled) {
		/* LDPM is relevant for send, write, read
		 * (and atomic in the future), but limited to 8kb
		 */
		dpm->is_ldpm = 1;
	}
}

static u32 qedr_prepare_sq_inline_data(struct qedr_dev *dev,
				       struct qedr_qp *qp, u8 *wqe_size,
				       IB_CONST struct ib_send_wr *wr,
				       IB_CONST struct ib_send_wr **bad_wr,
				       u8 *bits, u32 data_size, u8 bit)
{
	char *seg_prt, *wqe;
	int i, seg_siz;

	if (data_size > qp->max_inline_data) {
		DP_ERR(dev, "Too much inline data in WR: %d\n", data_size);
		*bad_wr = wr;
		return 0;
	}

	if (!data_size)
		return data_size;

	*bits |= bit;

	seg_prt = NULL;
	wqe = NULL;
	seg_siz = 0;

	/* Copy data inline */
	for (i = 0; i < wr->num_sge; i++) {
		u32 len = wr->sg_list[i].length;
		void *src = (void *)(uintptr_t)wr->sg_list[i].addr;

		while (len > 0) {
			u32 cur;

			/* New segment required */
			if (!seg_siz) {
				wqe = (char *)qed_chain_produce(&qp->sq.pbl);
				seg_prt = wqe;
				seg_siz = sizeof(struct rdma_sq_common_wqe);
				(*wqe_size)++;
			}

			/* Calculate currently allowed length */
			cur = min_t(u32, len, seg_siz);
			memcpy(seg_prt, src, cur);

			/* Update segment variables */
			seg_prt += cur;
			seg_siz -= cur;

			/* Update sge variables */
			src += cur;
			len -= cur;

			/* Swap fully-completed segments */
			if (!seg_siz)
				swap_wqe_data64((u64 *)wqe);
		}
	}

	/* swap last not completed segment */
	if (seg_siz)
		swap_wqe_data64((u64 *)wqe);

	return data_size;
}

#define RQ_SGE_SET(sge, vaddr, vlength, vflags)			\
	do {							\
		DMA_REGPAIR_LE(sge->addr, vaddr);		\
		(sge)->length = cpu_to_le32(vlength);		\
		(sge)->flags = cpu_to_le32(vflags);		\
	} while (0)

#define SRQ_HDR_SET(hdr, vwr_id, num_sge)			\
	do {							\
		DMA_REGPAIR_LE(hdr->wr_id, vwr_id);		\
		(hdr)->num_sges = num_sge;			\
	} while (0)

#define SRQ_SGE_SET(sge, vaddr, vlength, vlkey)			\
	do {							\
		DMA_REGPAIR_LE(sge->addr, vaddr);		\
		(sge)->length = cpu_to_le32(vlength);		\
		(sge)->l_key = cpu_to_le32(vlkey);		\
	} while (0)

static void qedr_prepare_sq_sges(struct qedr_qp *qp, struct qedr_dpm *dpm,
				 u8 *wqe_size, RDMA_CONST struct ib_send_wr *wr)
{
	int i;

	for (i = 0; i < wr->num_sge; i++) {
		struct rdma_sq_sge *sge = qed_chain_produce(&qp->sq.pbl);

		DMA_REGPAIR_LE(sge->addr, wr->sg_list[i].addr);
		sge->l_key = cpu_to_le32(wr->sg_list[i].lkey);
		sge->length = cpu_to_le32(wr->sg_list[i].length);

		if (dpm->is_ldpm) {
			memcpy(&dpm->payload[dpm->payload_size], sge,
			       sizeof(*sge));
			dpm->payload_size += sizeof(*sge);
		}
	}

	if (wqe_size)
		*wqe_size += wr->num_sge;
}

static u32 qedr_prepare_sq_rdma_data(struct qedr_dev *dev,
				     struct qedr_qp *qp,
				     struct qedr_dpm *dpm,
				     struct rdma_sq_rdma_wqe_1st *rwqe,
				     struct rdma_sq_rdma_wqe_2nd *rwqe2,
				     IB_CONST struct ib_send_wr *wr,
				     IB_CONST struct ib_send_wr **bad_wr,
				     u32 data_size)
{
	rwqe2->r_key = cpu_to_le32(rdma_wr(wr)->rkey);
	DMA_REGPAIR_LE(rwqe2->remote_va, rdma_wr(wr)->remote_addr);

	if (wr->send_flags & IB_SEND_INLINE &&
	    (wr->opcode == IB_WR_RDMA_WRITE_WITH_IMM ||
	     wr->opcode == IB_WR_RDMA_WRITE)) {
		u8 flags = 0;

		SET_FIELD2(flags, RDMA_SQ_RDMA_WQE_1ST_INLINE_FLG, 1);
		return qedr_prepare_sq_inline_data(dev, qp, &rwqe->wqe_size,
						   wr, bad_wr, &rwqe->flags,
						   data_size, flags);
	} else {
		if (dpm->is_ldpm)
			dpm->payload_size = sizeof(*rwqe) + sizeof(*rwqe2);

		/* wqe_size will be modified here, thus we copy the headers
		 * only after
		 */
		qedr_prepare_sq_sges(qp, dpm, &rwqe->wqe_size, wr);

		if (dpm->is_ldpm) {
			rwqe->length = cpu_to_le32(data_size);
			memcpy(&dpm->payload[0], rwqe, sizeof(*rwqe));
			memcpy(&dpm->payload[sizeof(*rwqe)], rwqe2,
			       sizeof(*rwqe2));
		}

		return data_size;
	}
}

static u32 qedr_prepare_sq_send_data(struct qedr_dev *dev,
				     struct qedr_qp *qp,
				     struct qedr_dpm *dpm,
				     struct rdma_sq_send_wqe_1st *swqe,
				     struct rdma_sq_send_wqe_2st *swqe2,
				     IB_CONST struct ib_send_wr *wr,
				     IB_CONST struct ib_send_wr **bad_wr,
				     u32 data_size)
{
	memset(swqe2, 0, sizeof(*swqe2));

	if (wr->send_flags & IB_SEND_INLINE) {
		u8 flags = 0;

		SET_FIELD2(flags, RDMA_SQ_SEND_WQE_INLINE_FLG, 1);
		return qedr_prepare_sq_inline_data(dev, qp, &swqe->wqe_size,
						   wr, bad_wr, &swqe->flags,
						   data_size, flags);
	} else {
		if (dpm->is_ldpm)
			dpm->payload_size = sizeof(*swqe) + sizeof(*swqe2);

		/* wqe_size will be modified here, thus we copy the headers
		 * only after
		 */
		qedr_prepare_sq_sges(qp, dpm, &swqe->wqe_size, wr);

		if (dpm->is_ldpm) {
			swqe->length = cpu_to_le32(data_size);
			memcpy(&dpm->payload[0], swqe, sizeof(*swqe));
			memcpy(&dpm->payload[sizeof(*swqe)], swqe2,
			       sizeof(*swqe2));
		}

		return data_size;
	}
}

static inline void qedr_ldpm_prepare_data(struct qedr_qp *qp,
					  struct qedr_dpm *dpm)
{
	int size;

	/* DPM size is given in 8 bytes so we round up */
	size = dpm->payload_size + sizeof(struct db_rdma_dpm_data);
	size = ALIGN(size, sizeof(u64)) / sizeof(u64);

	SET_FIELD(dpm->msg.data.params.params, DB_RDMA_DPM_PARAMS_SIZE, size);
	SET_FIELD(dpm->msg.data.params.params, DB_RDMA_DPM_PARAMS_DPM_TYPE,
		  DPM_LEGACY);
}

#ifdef DEFINE_ALLOC_MR /* QEDR_UPSTREAM */
static int qedr_prepare_reg(struct qedr_qp *qp,
			    struct rdma_sq_fmr_wqe_1st *fwqe1,
			    RDMA_CONST struct ib_reg_wr *wr)
{
	struct qedr_mr *mr = get_qedr_mr(wr->mr);
	struct rdma_sq_fmr_wqe_2nd *fwqe2;

	fwqe2 = (struct rdma_sq_fmr_wqe_2nd *)qed_chain_produce(&qp->sq.pbl);
	fwqe1->addr.hi = upper_32_bits(mr->ibmr.iova);
	fwqe1->addr.lo = lower_32_bits(mr->ibmr.iova);
	fwqe1->l_key = wr->key;

	fwqe2->access_ctrl = 0;

	SET_FIELD2(fwqe2->access_ctrl, RDMA_SQ_FMR_WQE_2ND_REMOTE_READ,
		   !!(wr->access & IB_ACCESS_REMOTE_READ));
	SET_FIELD2(fwqe2->access_ctrl, RDMA_SQ_FMR_WQE_2ND_REMOTE_WRITE,
		   !!(wr->access & IB_ACCESS_REMOTE_WRITE));
	SET_FIELD2(fwqe2->access_ctrl, RDMA_SQ_FMR_WQE_2ND_ENABLE_ATOMIC,
		   !!(wr->access & IB_ACCESS_REMOTE_ATOMIC));
	SET_FIELD2(fwqe2->access_ctrl, RDMA_SQ_FMR_WQE_2ND_LOCAL_READ, 1);
	SET_FIELD2(fwqe2->access_ctrl, RDMA_SQ_FMR_WQE_2ND_LOCAL_WRITE,
		   !!(wr->access & IB_ACCESS_LOCAL_WRITE));
	fwqe2->fmr_ctrl = 0;

	SET_FIELD2(fwqe2->fmr_ctrl, RDMA_SQ_FMR_WQE_2ND_PAGE_SIZE_LOG,
		   ilog2(mr->ibmr.page_size) - 12);

	fwqe2->length_hi = 0; /* INTERNAL: TODO - figure out why length is only 32bit.. */
	fwqe2->length_lo = mr->ibmr.length;
	fwqe2->pbl_addr.hi = upper_32_bits(mr->info.pbl_table->pa);
	fwqe2->pbl_addr.lo = lower_32_bits(mr->info.pbl_table->pa);

	qp->wqe_wr_id[qp->sq.prod].mr = mr;

	return 0;
}
#else

static void build_frmr_pbes(struct ib_send_wr *wr, struct mr_info *info)
{
	int i;
	u64 buf_addr = 0;
	int num_pbes, total_num_pbes = 0;
	struct regpair *pbe;
	struct qedr_pbl *pbl_tbl = info->pbl_table;
	struct qedr_pbl_info *pbl_info = &info->pbl_info;

	pbe = (struct regpair *)pbl_tbl->va;
	num_pbes = 0;

	for (i = 0; i < wr->wr.fast_reg.page_list_len; i++) {
		buf_addr = wr->wr.fast_reg.page_list->page_list[i];
		pbe->lo = cpu_to_le32((u32)buf_addr);
		pbe->hi = cpu_to_le32((u32)upper_32_bits(buf_addr));

		num_pbes += 1;
		pbe++;
		total_num_pbes++;

		if (total_num_pbes == pbl_info->num_pbes)
			return;

		/* if the given pbl is full storing the pbes,
		 * move to next pbl.
		 */
		if (num_pbes ==
		    (pbl_info->pbl_size / sizeof(u64))) {
			pbl_tbl++;
			pbe = (struct regpair *)pbl_tbl->va;
			num_pbes = 0;
		}
	}
}

static int qedr_prepare_safe_pbl(struct qedr_dev *dev, struct mr_info *info)
{
	int rc = 0;

	if (info->completed == 0) {
		DP_VERBOSE(dev, QEDR_MSG_MR, "First FMR\n");
		/* first fmr */
		return 0;
	}

	handle_completed_mrs(dev, info);

	list_add_tail(&info->pbl_table->list_entry, &info->inuse_pbl_list);

	if (list_empty(&info->free_pbl_list)) {
		info->pbl_table = qedr_alloc_pbl_tbl(dev, &info->pbl_info,
							  GFP_ATOMIC);
	} else {
		info->pbl_table = list_first_entry(&info->free_pbl_list,
			struct qedr_pbl,
			list_entry);
		list_del(&info->pbl_table->list_entry);
	}

	if (!info->pbl_table)
		rc = -ENOMEM;

	return rc;
}

static inline int qedr_prepare_fmr(struct qedr_qp *qp,
				   struct rdma_sq_fmr_wqe_1st *fwqe1,
				   struct ib_send_wr *wr)
{
	struct qedr_dev *dev = qp->dev;
	u64 fbo;
	struct qedr_fast_reg_page_list *frmr_list =
		get_qedr_frmr_list(wr->wr.fast_reg.page_list);
	struct rdma_sq_fmr_wqe_2nd *fwqe2 =
		(struct rdma_sq_fmr_wqe_2nd *)qed_chain_produce(&qp->sq.pbl);
	int rc = 0;

	if (wr->wr.fast_reg.page_list_len == 0)
		BUG();

	rc = qedr_prepare_safe_pbl(dev, &frmr_list->info);
	if (rc)
		return rc;

	fwqe1->addr.hi = upper_32_bits(wr->wr.fast_reg.iova_start);
	fwqe1->addr.lo = lower_32_bits(wr->wr.fast_reg.iova_start);
	fwqe1->l_key = wr->wr.fast_reg.rkey;

	fwqe2->access_ctrl = 0;

	SET_FIELD2(fwqe2->access_ctrl, RDMA_SQ_FMR_WQE_2ND_REMOTE_READ,
		   !!(wr->wr.fast_reg.access_flags & IB_ACCESS_REMOTE_READ));
	SET_FIELD2(fwqe2->access_ctrl, RDMA_SQ_FMR_WQE_2ND_REMOTE_WRITE,
		   !!(wr->wr.fast_reg.access_flags & IB_ACCESS_REMOTE_WRITE));
	SET_FIELD2(fwqe2->access_ctrl, RDMA_SQ_FMR_WQE_2ND_ENABLE_ATOMIC,
		   !!(wr->wr.fast_reg.access_flags & IB_ACCESS_REMOTE_ATOMIC));
	SET_FIELD2(fwqe2->access_ctrl, RDMA_SQ_FMR_WQE_2ND_LOCAL_READ, 1);
	SET_FIELD2(fwqe2->access_ctrl, RDMA_SQ_FMR_WQE_2ND_LOCAL_WRITE,
		   !!(wr->wr.fast_reg.access_flags & IB_ACCESS_LOCAL_WRITE));

	fwqe2->fmr_ctrl = 0;

	SET_FIELD2(fwqe2->fmr_ctrl, RDMA_SQ_FMR_WQE_2ND_PAGE_SIZE_LOG,
		   ilog2(1 << wr->wr.fast_reg.page_shift) - 12);
	SET_FIELD2(fwqe2->fmr_ctrl, RDMA_SQ_FMR_WQE_2ND_ZERO_BASED, 0);

	fwqe2->length_hi = 0; /* TODO: figure out why length is only 32bit */
	fwqe2->length_lo = wr->wr.fast_reg.length;
	fwqe2->pbl_addr.hi = upper_32_bits(frmr_list->info.pbl_table->pa);
	fwqe2->pbl_addr.lo = lower_32_bits(frmr_list->info.pbl_table->pa);


	fbo = wr->wr.fast_reg.iova_start -
	    (wr->wr.fast_reg.page_list->page_list[0] & PAGE_MASK);

	FP_DP_VERBOSE(dev, QEDR_MSG_MR,
		      "Prepare fmr...rkey=%x addr=%x:%x length = %x pbl_addr %x:%x access_ctrl=%x\n",
		      wr->wr.fast_reg.rkey, fwqe1->addr.hi, fwqe1->addr.lo,
		      fwqe2->length_lo, fwqe2->pbl_addr.hi, fwqe2->pbl_addr.lo,
		      fwqe2->access_ctrl);

	build_frmr_pbes(wr, &frmr_list->info);

	qp->wqe_wr_id[qp->sq.prod].frmr = frmr_list;

	return 0;
}
#endif

static enum ib_wc_opcode qedr_ib_to_wc_opcode(enum ib_wr_opcode opcode)
{
	switch (opcode) {
	case IB_WR_RDMA_WRITE:
	case IB_WR_RDMA_WRITE_WITH_IMM:
		return IB_WC_RDMA_WRITE;
	case IB_WR_SEND_WITH_IMM:
	case IB_WR_SEND:
	case IB_WR_SEND_WITH_INV:
		return IB_WC_SEND;
	case IB_WR_RDMA_READ:
	case IB_WR_RDMA_READ_WITH_INV:
		return IB_WC_RDMA_READ;
	case IB_WR_ATOMIC_CMP_AND_SWP:
		return IB_WC_COMP_SWAP;
	case IB_WR_ATOMIC_FETCH_AND_ADD:
		return IB_WC_FETCH_ADD;
#ifdef DEFINE_ALLOC_MR /* QEDR_UPSTREAM */
	case IB_WR_REG_MR:
		return IB_WC_REG_MR;
#else
	case IB_WR_FAST_REG_MR:
		return IB_WC_FAST_REG_MR;
#endif
	case IB_WR_LOCAL_INV:
		return IB_WC_LOCAL_INV;
	default:
		return IB_WC_SEND;
	}
}

#define QEDR_WID_SIZE	(1024)

static inline u32 qedr_get_wid_offset(struct qedr_dev *dev,
				      struct qedr_dpm *dpm)
{
	int cpu = smp_processor_id();

	if (likely(cpu < dev->wid_count))
		return cpu * QEDR_WID_SIZE;
	else
		return 0;
}

static inline void qedr_doorbell_dpm_qp(struct qedr_dev *dev,
					struct qedr_qp *qp,
					struct qedr_dpm *dpm)
{
	u64 *payload = (u64 *)dpm->payload;
	u32 num_dwords;
	u32 offset = 0;
	void *db_addr;
	u8 bytes = 0;
	u8 hdr_len;
	u64 data;

	/* Write message header */
	dpm->msg.data.icid = qp->sq.db_data.data.icid;
	dpm->msg.data.prod_val = qp->sq.db_data.data.value;
	db_addr = qp->sq.dpm_db + qedr_get_wid_offset(dev, dpm);
	hdr_len = sizeof(dpm->msg.raw);

	/* Flush all the writes before signalling doorbell */
	wmb();
	writeq(dpm->msg.raw, db_addr);

	/* Write message body */
	bytes += hdr_len;
	db_addr += hdr_len;
	num_dwords = ALIGN(dpm->payload_size, sizeof(u64)) / sizeof(u64);

	while (offset < num_dwords) {
		/* Endianity is different bwetween FW and DORQ HW block */
		data = cpu_to_be64(payload[offset]);

		writeq(data, db_addr);

		bytes += sizeof(u64);
		db_addr += sizeof(u64);

		/* Since we rewrite the buffer every 64 bytes we need to flush
		 * it here, otherwise the CPU could optimize away the
		 * duplicate stores.
		 */
		if (bytes >= 64) {
			/* Flush wc buffer */
			wmb();
			bytes = 0;
		}
		offset++;
	}

	/* Flush wc buffer */
	wmb();
}

static inline bool qedr_can_post_send(struct qedr_qp *qp,
				      RDMA_CONST struct ib_send_wr *wr)
{
	int wq_is_full, err_wr, pbl_is_full;
	struct qedr_dev *dev = qp->dev;

	/* prevent SQ overflow and/or processing of a bad WR */
	err_wr = wr->num_sge > qp->sq.max_sges;
	wq_is_full = qedr_wq_is_full(&qp->sq);
	pbl_is_full = qed_chain_get_elem_left_u32(&qp->sq.pbl) <
		      QEDR_MAX_SQE_ELEMENTS_PER_SQE;
	if (wq_is_full || err_wr || pbl_is_full) {
		if (wq_is_full && !(qp->err_bitmap & QEDR_QP_ERR_SQ_FULL)) {
			DP_ERR(dev,
			       "error: WQ is full. Post send on QP %p failed (this error appears only once)\n",
			       qp);
			qp->err_bitmap |= QEDR_QP_ERR_SQ_FULL;
		}

		if (err_wr && !(qp->err_bitmap & QEDR_QP_ERR_BAD_SR)) {
			DP_ERR(dev,
			       "error: WR is bad. Post send on QP %p failed (this error appears only once)\n",
			       qp);
			qp->err_bitmap |= QEDR_QP_ERR_BAD_SR;
		}

		if (pbl_is_full &&
		    !(qp->err_bitmap & QEDR_QP_ERR_SQ_PBL_FULL)) {
			DP_ERR(dev,
			       "error: WQ PBL is full. Post send on QP %p failed (this error appears only once)\n",
			       qp);
			qp->err_bitmap |= QEDR_QP_ERR_SQ_PBL_FULL;
		}
		return false;
	}
	return true;
}

static int __qedr_post_send(struct ib_qp *ibqp, IB_CONST struct ib_send_wr *wr,
		     IB_CONST struct ib_send_wr **bad_wr, int *db_required)
{
	struct qedr_dev *dev = get_qedr_dev(ibqp->device);
	struct qedr_qp *qp = get_qedr_qp(ibqp);
	struct rdma_sq_atomic_wqe_1st *awqe1;
	struct rdma_sq_atomic_wqe_2nd *awqe2;
	struct rdma_sq_atomic_wqe_3rd *awqe3;
	struct rdma_sq_send_wqe_2st *swqe2;
	struct rdma_sq_local_inv_wqe *iwqe;
	struct rdma_sq_rdma_wqe_2nd *rwqe2;
	struct rdma_sq_send_wqe_1st *swqe;
	struct rdma_sq_rdma_wqe_1st *rwqe;
	struct rdma_sq_fmr_wqe_1st *fwqe1;
	struct rdma_sq_common_wqe *wqe;
	struct qedr_dpm dpm = {0};
	u32 data_size, length;
	int rc = 0;
	bool comp;

	if (!qedr_can_post_send(qp, wr)) {
		*bad_wr = wr;
		return -ENOMEM;
	}

	data_size = sge_data_len(wr->sg_list, wr->num_sge);

	qedr_init_dpm_info(dev, qp, wr, &dpm, data_size);

	wqe = qed_chain_produce(&qp->sq.pbl);
	qp->wqe_wr_id[qp->sq.prod].signaled =
		!!(wr->send_flags & IB_SEND_SIGNALED) || qp->signaled;

	wqe->flags = 0;
	SET_FIELD2(wqe->flags, RDMA_SQ_SEND_WQE_SE_FLG,
		   !!(wr->send_flags & IB_SEND_SOLICITED));
	comp = (!!(wr->send_flags & IB_SEND_SIGNALED)) || qp->signaled;
	SET_FIELD2(wqe->flags, RDMA_SQ_SEND_WQE_COMP_FLG, comp);
	SET_FIELD2(wqe->flags, RDMA_SQ_SEND_WQE_RD_FENCE_FLG,
		   !!(wr->send_flags & IB_SEND_FENCE));
	wqe->prev_wqe_size = qp->prev_wqe_size;

	qp->wqe_wr_id[qp->sq.prod].opcode = qedr_ib_to_wc_opcode(wr->opcode);

	switch (wr->opcode) {
	case IB_WR_SEND_WITH_IMM:
		if (rdma_protocol_iwarp(&dev->ibdev, 1)) {
			DP_ERR(dev, "SEND with immediate not supported over iWARP\n");
			rc = -EINVAL;
			*bad_wr = wr;
			break;
		}
		wqe->req_type = RDMA_SQ_REQ_TYPE_SEND_WITH_IMM;
		swqe = (struct rdma_sq_send_wqe_1st *)wqe;
		swqe->wqe_size = 2;
		swqe2 = qed_chain_produce(&qp->sq.pbl);

		swqe->inv_key_or_imm_data = cpu_to_le32(wr->ex.imm_data);
		length = qedr_prepare_sq_send_data(dev, qp, &dpm, swqe, swqe2,
						   wr, bad_wr, data_size);
		swqe->length = cpu_to_le32(length);

		qp->wqe_wr_id[qp->sq.prod].wqe_size = swqe->wqe_size;
		qp->prev_wqe_size = swqe->wqe_size;
		qp->wqe_wr_id[qp->sq.prod].bytes_len = swqe->length;
		FP_DP_VERBOSE(dev, QEDR_MSG_CQ,
			      "SEND w/ IMM length = %d imm data=%x\n",
			      swqe->length, wr->ex.imm_data);
		break;
	case IB_WR_SEND:
		wqe->req_type = RDMA_SQ_REQ_TYPE_SEND;
		swqe = (struct rdma_sq_send_wqe_1st *)wqe;

		swqe->wqe_size = 2;
		swqe2 = qed_chain_produce(&qp->sq.pbl);
		length = qedr_prepare_sq_send_data(dev, qp, &dpm, swqe, swqe2,
						   wr, bad_wr, data_size);
		swqe->length = cpu_to_le32(length);

		qp->wqe_wr_id[qp->sq.prod].wqe_size = swqe->wqe_size;
		qp->prev_wqe_size = swqe->wqe_size;
		qp->wqe_wr_id[qp->sq.prod].bytes_len = swqe->length;
		FP_DP_VERBOSE(dev, QEDR_MSG_CQ, "SEND w/o IMM length = %d\n",
			      swqe->length);
		break;
	case IB_WR_SEND_WITH_INV:
		wqe->req_type = RDMA_SQ_REQ_TYPE_SEND_WITH_INVALIDATE;
		swqe = (struct rdma_sq_send_wqe_1st *)wqe;
		swqe2 = qed_chain_produce(&qp->sq.pbl);
		swqe->wqe_size = 2;
		swqe->inv_key_or_imm_data = cpu_to_le32(wr->ex.invalidate_rkey);
		length = qedr_prepare_sq_send_data(dev, qp, &dpm, swqe, swqe2,
						   wr, bad_wr, data_size);
		swqe->length = cpu_to_le32(length);

		qp->wqe_wr_id[qp->sq.prod].wqe_size = swqe->wqe_size;
		qp->prev_wqe_size = swqe->wqe_size;
		qp->wqe_wr_id[qp->sq.prod].bytes_len = swqe->length;
		FP_DP_VERBOSE(dev, QEDR_MSG_CQ,
			      "SEND w INVALIDATE length = %d\n", swqe->length);
		break;

	case IB_WR_RDMA_WRITE_WITH_IMM:
		if (rdma_protocol_iwarp(&dev->ibdev, 1)) {
			DP_ERR(dev, "RDMA WRITE with immediate not supported over iWARP\n");
			rc = -EINVAL;
			*bad_wr = wr;
			break;
		}
		wqe->req_type = RDMA_SQ_REQ_TYPE_RDMA_WR_WITH_IMM;
		rwqe = (struct rdma_sq_rdma_wqe_1st *)wqe;

		rwqe->wqe_size = 2;
		rwqe->imm_data = htonl(cpu_to_le32(wr->ex.imm_data));
		rwqe2 = qed_chain_produce(&qp->sq.pbl);
		length = qedr_prepare_sq_rdma_data(dev, qp, &dpm, rwqe, rwqe2,
						   wr, bad_wr, data_size);
		rwqe->length = cpu_to_le32(length);

		qp->wqe_wr_id[qp->sq.prod].wqe_size = rwqe->wqe_size;
		qp->prev_wqe_size = rwqe->wqe_size;
		qp->wqe_wr_id[qp->sq.prod].bytes_len = rwqe->length;
		FP_DP_VERBOSE(dev, QEDR_MSG_CQ,
			      "RDMA WRITE w/ IMM length = %d imm data=%x\n",
			      rwqe->length, rwqe->imm_data);
		break;
	case IB_WR_RDMA_WRITE:
		wqe->req_type = RDMA_SQ_REQ_TYPE_RDMA_WR;
		rwqe = (struct rdma_sq_rdma_wqe_1st *)wqe;

		rwqe->wqe_size = 2;
		rwqe2 = qed_chain_produce(&qp->sq.pbl);
		length = qedr_prepare_sq_rdma_data(dev, qp, &dpm, rwqe, rwqe2,
						   wr, bad_wr, data_size);
		rwqe->length = cpu_to_le32(length);

		qp->wqe_wr_id[qp->sq.prod].wqe_size = rwqe->wqe_size;
		qp->prev_wqe_size = rwqe->wqe_size;
		qp->wqe_wr_id[qp->sq.prod].bytes_len = rwqe->length;
		FP_DP_VERBOSE(dev, QEDR_MSG_CQ,
			      "RDMA WRITE w/o IMM length = %d\n", rwqe->length);
		break;
	case IB_WR_RDMA_READ_WITH_INV:
		SET_FIELD2(wqe->flags, RDMA_SQ_RDMA_WQE_1ST_READ_INV_FLG, 1);
		FP_DP_VERBOSE(dev, QEDR_MSG_CQ, "RDMA READ with invalidate\n");
		/* same is identical to RDMA READ */
		COMPAT_FALLTHROUGH;
	case IB_WR_RDMA_READ:
		wqe->req_type = RDMA_SQ_REQ_TYPE_RDMA_RD;
		rwqe = (struct rdma_sq_rdma_wqe_1st *)wqe;

		rwqe->wqe_size = 2;
		rwqe2 = qed_chain_produce(&qp->sq.pbl);
		length = qedr_prepare_sq_rdma_data(dev, qp, &dpm, rwqe, rwqe2,
						   wr, bad_wr, data_size);

		rwqe->length = cpu_to_le32(length);
		qp->wqe_wr_id[qp->sq.prod].wqe_size = rwqe->wqe_size;
		qp->prev_wqe_size = rwqe->wqe_size;
		qp->wqe_wr_id[qp->sq.prod].bytes_len = rwqe->length;
		FP_DP_VERBOSE(dev, QEDR_MSG_CQ, "RDMA READ length = %d\n",
			      rwqe->length);
		break;

	case IB_WR_ATOMIC_CMP_AND_SWP:
	case IB_WR_ATOMIC_FETCH_AND_ADD:
		FP_DP_VERBOSE(dev, QEDR_MSG_CQ, "ATOMIC\n");
		awqe1 = (struct rdma_sq_atomic_wqe_1st *)wqe;
		awqe1->wqe_size = 4;

		awqe2 = qed_chain_produce(&qp->sq.pbl);
		DMA_REGPAIR_LE(awqe2->remote_va, atomic_wr(wr)->remote_addr);
		awqe2->r_key = cpu_to_le32(atomic_wr(wr)->rkey);

		awqe3 = qed_chain_produce(&qp->sq.pbl);

		if (wr->opcode == IB_WR_ATOMIC_FETCH_AND_ADD) {
			wqe->req_type = RDMA_SQ_REQ_TYPE_ATOMIC_ADD;
			DMA_REGPAIR_LE(awqe3->swap_data,
				       atomic_wr(wr)->compare_add);
		} else {
			wqe->req_type = RDMA_SQ_REQ_TYPE_ATOMIC_CMP_AND_SWAP;
			DMA_REGPAIR_LE(awqe3->swap_data,
				       atomic_wr(wr)->swap);
			DMA_REGPAIR_LE(awqe3->cmp_data,
				       atomic_wr(wr)->compare_add);
		}

		qedr_prepare_sq_sges(qp, &dpm, NULL, wr);

		qp->wqe_wr_id[qp->sq.prod].wqe_size = awqe1->wqe_size;
		qp->prev_wqe_size = awqe1->wqe_size;
		break;

	case IB_WR_LOCAL_INV:
		FP_DP_VERBOSE(dev, QEDR_MSG_CQ, "INVALIDATE length\n");
		iwqe = (struct rdma_sq_local_inv_wqe *)wqe;
		iwqe->wqe_size = 1;

		iwqe->req_type = RDMA_SQ_REQ_TYPE_LOCAL_INVALIDATE;
		iwqe->inv_l_key = wr->ex.invalidate_rkey;
		qp->wqe_wr_id[qp->sq.prod].wqe_size = iwqe->wqe_size;
		qp->prev_wqe_size = iwqe->wqe_size;
		break;

#ifdef DEFINE_ALLOC_MR /* QEDR_UPSTREAM */
	case IB_WR_REG_MR:
		DP_DEBUG(dev, QEDR_MSG_CQ, "REG_MR\n");
		wqe->req_type = RDMA_SQ_REQ_TYPE_FAST_MR;
		fwqe1 = (struct rdma_sq_fmr_wqe_1st *)wqe;
		fwqe1->wqe_size = 2;

		rc = qedr_prepare_reg(qp, fwqe1, reg_wr(wr));
		if (rc) {
			DP_ERR(dev, "IB_REG_MR failed rc=%d\n", rc);
			*bad_wr = wr;
			break;
		}

		qp->wqe_wr_id[qp->sq.prod].wqe_size = fwqe1->wqe_size;
		qp->prev_wqe_size = fwqe1->wqe_size;
		break;
#else
	case IB_WR_FAST_REG_MR:
		FP_DP_VERBOSE(dev, QEDR_MSG_CQ, "FAST_MR\n");
		wqe->req_type = RDMA_SQ_REQ_TYPE_FAST_MR;
		fwqe1 = (struct rdma_sq_fmr_wqe_1st *)wqe;
		fwqe1->wqe_size = 2;

		rc = qedr_prepare_fmr(qp, fwqe1, wr);
		if (rc) {
			DP_ERR(dev, "IB_WR_FAST_REG_MR failed rc=%d\n",
			       rc);
			*bad_wr = wr;
			break;
		}

		qp->wqe_wr_id[qp->sq.prod].wqe_size = fwqe1->wqe_size;
		qp->prev_wqe_size = fwqe1->wqe_size;

		break;
#endif
	default:
		DP_ERR(dev, "invalid opcode 0x%x!\n", wr->opcode);
		rc = -EINVAL;
		*bad_wr = wr;
		break;
	}

	if (*bad_wr) {
		u16 value;

		/* Restore prod to its position before
		 * this WR was processed
		 */
		value = le16_to_cpu(qp->sq.db_data.data.value);
		qed_chain_set_prod(&qp->sq.pbl, value, wqe);

		/* Restore prev_wqe_size */
		qp->prev_wqe_size = wqe->prev_wqe_size;
		rc = -EINVAL;
		DP_ERR(dev, "post send failed with opcode=%d\n", wr->opcode);
#if DEFINE_THIS_CPU_INC /* ! QEDR_UPSTREAM */
		this_cpu_inc(dev->stats->send_bad_wr);
	} else {
		this_cpu_inc(dev->stats->send_wr[wqe->req_type]);
#endif
	}

	if (rc)
		return rc;

	qp->wqe_wr_id[qp->sq.prod].wr_id = wr->wr_id;
	qedr_inc_sw_prod(&qp->sq);
	qp->sq.db_data.data.value++;

	if (qp->recov_info.reset)
		goto out;

	if (dpm.is_ldpm) {
		qedr_ldpm_prepare_data(qp, &dpm);
		qedr_doorbell_dpm_qp(dev, qp, &dpm);
		*db_required = 0;
	} else {
		*db_required = 1;
	}

out:

	return 0;
}

int qedr_post_send(struct ib_qp *ibqp, IB_CONST struct ib_send_wr *wr,
		   IB_CONST struct ib_send_wr **bad_wr, bool drain)
{
	struct qedr_dev *dev = get_qedr_dev(ibqp->device);
	struct qedr_qp *qp = get_qedr_qp(ibqp);
	int db_required = 0;
	unsigned long flags;
	int rc = 0;

	if (qp->recov_info.reset && !drain) {
		DP_VERBOSE(dev, QEDR_MSG_FAIL,
			   "failed qedr_post_send, device was reset\n");
		*bad_wr = wr;

		return -EPERM;
	}

	*bad_wr = NULL;

	if (qp->qp_type == IB_QPT_GSI)
		return qedr_gsi_post_send(ibqp, wr, bad_wr);

	spin_lock_irqsave(&qp->q_lock, flags);

	if (rdma_protocol_roce(&dev->ibdev, 1)) {
		if ((qp->state != QED_ROCE_QP_STATE_RTS) &&
		    (qp->state != QED_ROCE_QP_STATE_ERR) &&
		    (qp->state != QED_ROCE_QP_STATE_SQD)) {
			spin_unlock_irqrestore(&qp->q_lock, flags);
			*bad_wr = wr;
			DP_DEBUG(dev, QEDR_MSG_CQ,
				 "QP in wrong state! QP icid=0x%x state %d\n",
				 qp->icid, qp->state);
			return -EINVAL;
		}
	}

#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
	if (!wr) {
		DP_ERR(dev, "Got an empty post send.\n");
		return -EINVAL;
	}
#endif

	while (wr) {
		rc = __qedr_post_send(ibqp, wr, bad_wr, &db_required);

		if (rc)
			break;

		wr = wr->next;
	}

	/* Trigger doorbell
	 * If there was a failure in the first WR then it will be triggered in
	 * vane. However this is not harmful (as long as the producer value is
	 * unchanged). For performance reasons we avoid checking for this
	 * redundant doorbell.
	 */
	if (!qp->recov_info.reset && db_required) {
		/* Flush all the writes before signalling doorbell */
		wmb();
		writel(qp->sq.db_data.raw, qp->sq.db);

		/* Make sure write sticks */
		mmiowb();
	}

	spin_unlock_irqrestore(&qp->q_lock, flags);

	return rc;
}

static u32 qedr_srq_elem_left(struct qedr_srq_hwq_info *hw_srq)
{
	u32 used;

	/* Calculate number of elements used based on producer
	 * count and consumer count and subtract it from max
	 * work request supported so that we get elements left.
	 */
	used = hw_srq->wr_prod_cnt - (u32)atomic_read(&hw_srq->wr_cons_cnt);

	return hw_srq->max_wr - used;
}

int qedr_post_srq_recv(struct ib_srq *ibsrq, IB_CONST struct ib_recv_wr *wr,
		       IB_CONST struct ib_recv_wr **bad_wr)
{
	struct qedr_srq *srq = get_qedr_srq(ibsrq);
	struct qedr_srq_hwq_info *hw_srq = &srq->hw_srq;
	struct qedr_dev *dev = srq->dev;
	struct qed_chain *pbl;
	unsigned long flags;
	int status = 0;
	u32 num_sge;

	if (srq->recov_info.reset) {
		DP_VERBOSE(dev, QEDR_MSG_FAIL,
			   "failed qedr_post_srq_recv, srq is reset\n");

		return -EPERM;
	}

	spin_lock_irqsave(&srq->lock, flags);

	pbl = &srq->hw_srq.pbl;
	while (wr) {
		struct rdma_srq_wqe_header *hdr;
		int i;

		if (!qedr_srq_elem_left(hw_srq) ||
		    wr->num_sge > srq->hw_srq.max_sges) {
			DP_ERR(dev, "Can't post WR  (%d,%d) || (%d > %d)\n",
			       hw_srq->wr_prod_cnt,
			       atomic_read(&hw_srq->wr_cons_cnt),
			       wr->num_sge, srq->hw_srq.max_sges);
			status = -ENOMEM;
			*bad_wr = wr;
			break;
		}

		hdr = qed_chain_produce(pbl);
		num_sge = wr->num_sge;
		/* Set number of sge and work request id in header */
		SRQ_HDR_SET(hdr, wr->wr_id, num_sge);

		/* PBL is maintained in case of WR granularity.
		 * So increment WR producer in case we post a WR.
		 */
		qedr_inc_srq_wr_prod(hw_srq);
		hw_srq->wqe_prod++;
		hw_srq->sge_prod++;

		DP_VERBOSE(dev, QEDR_MSG_SRQ,
			   "SRQ WR: SGEs: %d with wr_id[0x%llx]  wqe_prod:0x%x sge_prod:0x%x\n",
			   wr->num_sge, wr->wr_id, hw_srq->wqe_prod,
			   hw_srq->sge_prod);

		for (i = 0; i < wr->num_sge; i++) {
			struct rdma_srq_sge *srq_sge = qed_chain_produce(pbl);

			/* Set SGE length, lkey and address */
			SRQ_SGE_SET(srq_sge, wr->sg_list[i].addr,
				    wr->sg_list[i].length, wr->sg_list[i].lkey);

			DP_VERBOSE(dev, QEDR_MSG_SRQ,
				   "[%d]: len %d key %x addr %x:%x\n",
				   i, srq_sge->length, srq_sge->l_key,
				   srq_sge->addr.hi, srq_sge->addr.lo);
			hw_srq->sge_prod++;
		}

		/* Update WQE and SGE information before
		 * updating producer.
		 */
		dma_wmb();

		/* SRQ producer is 8 bytes. Need to update SGE producer index
		 * in first 4 bytes and need to update WQE producer in
		 * next 4 bytes.
		 */
		srq->hw_srq.virt_prod_pair_addr->sge_prod =
			cpu_to_le32(hw_srq->sge_prod);
		/* Make sure sge producer is updated first */
		barrier();
		srq->hw_srq.virt_prod_pair_addr->wqe_prod =
			cpu_to_le32(hw_srq->wqe_prod);
		/* Flush producer after updating it. */
		dma_wmb();
		wr = wr->next;
	}

	DP_VERBOSE(dev, QEDR_MSG_SRQ, "POST: Elements in srq_id:%d %p: %d sge_prod:%d wqe_prod:%d\n",
		   srq->srq_id, srq, qed_chain_get_elem_left_u32(pbl),
		   hw_srq->sge_prod, hw_srq->wqe_prod);

	spin_unlock_irqrestore(&srq->lock, flags);

	return status;
}

int qedr_post_recv(struct ib_qp *ibqp, IB_CONST struct ib_recv_wr *wr,
		   IB_CONST struct ib_recv_wr **bad_wr, bool drain)
{
	struct qedr_qp *qp = get_qedr_qp(ibqp);
	struct qedr_dev *dev = qp->dev;
	unsigned long flags;
#if DEFINE_THIS_CPU_INC /* ! QEDR_UPSTREAM */
	unsigned count = 0;
#endif
	int status = 0;
#ifndef QEDR_UPSTREAM_RECOVER /* !QEDR_UPSTREAM_RECOVER */
	if (qp->recov_info.reset && !drain) {
		DP_VERBOSE(dev, QEDR_MSG_FAIL,
			   "failed qedr_post_recv, device was reset\n");

		return -EPERM;
	}
#endif

	if (qp->qp_type == IB_QPT_GSI)
		return qedr_gsi_post_recv(ibqp, wr, bad_wr);

	if (!qedr_qp_has_rq(qp)) {
		DP_ERR(dev,
		       "cannot post_recv to QP, it has no RQ (perhaps it has SRQ?)\n");

		return -EINVAL;
	}

	spin_lock_irqsave(&qp->q_lock, flags);

	if (qp->state == QED_ROCE_QP_STATE_RESET) {
		spin_unlock_irqrestore(&qp->q_lock, flags);
		*bad_wr = wr;

		return -EINVAL;
	}

	while (wr) {
		int i;

		if (qed_chain_get_elem_left_u32(&qp->rq.pbl) <
		    QEDR_MAX_RQE_ELEMENTS_PER_RQE ||
		    wr->num_sge > qp->rq.max_sges) {
			DP_ERR(dev, "Can't post WR  (%d < %d) || (%d > %d)\n",
			       qed_chain_get_elem_left_u32(&qp->rq.pbl),
			       QEDR_MAX_RQE_ELEMENTS_PER_RQE, wr->num_sge,
			       qp->rq.max_sges);
			status = -ENOMEM;
			*bad_wr = wr;
#if DEFINE_THIS_CPU_INC /* ! QEDR_UPSTREAM */
			this_cpu_inc(dev->stats->recv_bad_wr);
#endif
			break;
		}

		FP_DP_VERBOSE(dev, QEDR_MSG_CQ,
			      "RQ WR: SGEs: %d with wr_id[%d] = %llx\n",
			      wr->num_sge, qp->rq.prod, wr->wr_id);
		for (i = 0; i < wr->num_sge; i++) {
			u32 flags = 0;
			struct rdma_rq_sge *rqe =
			    qed_chain_produce(&qp->rq.pbl);

			/* First one must include the number
			 * of SGE in the list
			 */
			if (!i)
				SET_FIELD(flags, RDMA_RQ_SGE_NUM_SGES,
					  wr->num_sge);

			SET_FIELD(flags, RDMA_RQ_SGE_L_KEY_LO,
				  wr->sg_list[i].lkey);

			RQ_SGE_SET(rqe, wr->sg_list[i].addr,
				   wr->sg_list[i].length, flags);
			FP_DP_VERBOSE(dev, QEDR_MSG_CQ,
				      "[%d]: len %d key %x addr %x:%x\n",
				      i, rqe->length, rqe->flags, rqe->addr.hi,
				      rqe->addr.lo);
		}

		/* Special case of no sges. FW requires between 1-4 sges...
		 * in this case we need to post 1 sge with length zero. this is
		 * because rdma write with immediate consumes an RQ.
		 */
		if (!wr->num_sge) {
			u32 flags = 0;
			struct rdma_rq_sge *rqe =
			    qed_chain_produce(&qp->rq.pbl);

			/* First one must include the number
			 * of SGE in the list
			 */
			SET_FIELD(flags, RDMA_RQ_SGE_L_KEY_LO, 0);
			SET_FIELD(flags, RDMA_RQ_SGE_NUM_SGES, 1);

			RQ_SGE_SET(rqe, 0, 0, flags);
			i = 1;
		}

		qp->rqe_wr_id[qp->rq.prod].wr_id = wr->wr_id;
		qp->rqe_wr_id[qp->rq.prod].wqe_size = i;

		qedr_inc_sw_prod(&qp->rq);

		/* Flush all the writes before signalling doorbell */
		wmb();

		qp->rq.db_data.data.value++;

		if (likely(!qp->recov_info.reset))
			writel(qp->rq.db_data.raw, qp->rq.db);

		/* Make sure write sticks */
		mmiowb();

		if (likely(!qp->recov_info.reset) &&
		    rdma_protocol_iwarp(&dev->ibdev, 1)) {
			writel(qp->rq.iwarp_db2_data.raw, qp->rq.iwarp_db2);
			mmiowb();	/* for second doorbell */
		}

#if DEFINE_THIS_CPU_INC /* ! QEDR_UPSTREAM */
		count++;
#endif
		wr = wr->next;
	}

	FP_DP_VERBOSE(dev, QEDR_MSG_CQ, "POST: Elements in RespQ: %d\n",
		      qed_chain_get_elem_left_u32(&qp->rq.pbl));

	spin_unlock_irqrestore(&qp->q_lock, flags);

#if DEFINE_THIS_CPU_INC /* ! QEDR_UPSTREAM */
	this_cpu_add(dev->stats->recv_wr, count);
#endif
	return status;
}

static int is_valid_cqe(struct qedr_cq *cq, union rdma_cqe *cqe)
{
	struct rdma_cqe_requester *resp_cqe = &cqe->req;

	return (resp_cqe->flags & RDMA_CQE_REQUESTER_TOGGLE_BIT_MASK) ==
		cq->pbl_toggle;
}

static struct qedr_qp *cqe_get_qp(union rdma_cqe *cqe)
{
	struct rdma_cqe_requester *resp_cqe = &cqe->req;
	struct qedr_qp *qp;

	qp = (struct qedr_qp *)(uintptr_t)HILO_GEN(resp_cqe->qp_handle.hi,
						   resp_cqe->qp_handle.lo,
						   u64);
	return qp;
}

static enum rdma_cqe_type cqe_get_type(union rdma_cqe *cqe)
{
	struct rdma_cqe_requester *resp_cqe = &cqe->req;

	return GET_FIELD(resp_cqe->flags, RDMA_CQE_REQUESTER_TYPE);
}

/* Return latest CQE (needs processing) */
static union rdma_cqe *get_cqe(struct qedr_cq *cq)
{
	return cq->latest_cqe;
}

/* In fmr we need to increase the number of fmr completed counter for the fmr
 * algorithm determining whether we can free a pbl or not.
 * we need to perform this whether the work request was signaled or not. for
 * this purpose we call this function from the condition that checks if a wr
 * should be skipped, to make sure we don't miss it ( possibly this fmr
 * operation was not signalted)
 */
static inline void qedr_chk_if_fmr(struct qedr_qp *qp)
{
#ifdef DEFINE_ALLOC_MR /* QEDR_UPSTREAM */
	if (qp->wqe_wr_id[qp->sq.cons].opcode == IB_WC_REG_MR)
		qp->wqe_wr_id[qp->sq.cons].mr->info.completed++;
#else
	if (qp->wqe_wr_id[qp->sq.cons].opcode == IB_WC_FAST_REG_MR)
		qp->wqe_wr_id[qp->sq.cons].frmr->info.completed++;
#endif
}

static int process_req(struct qedr_dev *dev, struct qedr_qp *qp,
		       struct qedr_cq *cq, int num_entries,
		       struct ib_wc *wc, u16 hw_cons, enum ib_wc_status status,
		       int force)
{
	u16 cnt = 0;

	while (num_entries && qp->sq.wqe_cons != hw_cons) {
		if (!qp->wqe_wr_id[qp->sq.cons].signaled && !force) {
			qedr_chk_if_fmr(qp);
			/* skip WC */
			goto next_cqe;
		}

		/* fill WC */
		wc->status = status;
		wc->vendor_err = 0;
		wc->wc_flags = 0;
		wc->src_qp = qp->id;
		wc->qp = &qp->ibqp;

		wc->wr_id = qp->wqe_wr_id[qp->sq.cons].wr_id;
		wc->opcode = qp->wqe_wr_id[qp->sq.cons].opcode;

		switch (wc->opcode) {
		case IB_WC_RDMA_WRITE:
			wc->byte_len = qp->wqe_wr_id[qp->sq.cons].bytes_len;
			FP_DP_VERBOSE(dev, QEDR_MSG_CQ,
				      "POLL REQ CQ: IB_WC_RDMA_WRITE byte_len=%d\n",
				      qp->wqe_wr_id[qp->sq.cons].bytes_len);
			break;
		case IB_WC_COMP_SWAP:
		case IB_WC_FETCH_ADD:
			wc->byte_len = 8;
			break;
#ifdef DEFINE_ALLOC_MR /* QEDR_UPSTREAM */
		case IB_WC_REG_MR:
			qp->wqe_wr_id[qp->sq.cons].mr->info.completed++;
			break;
#else
		case IB_WC_FAST_REG_MR:
			qp->wqe_wr_id[qp->sq.cons].frmr->info.completed++;
			break;
#endif
		case IB_WC_RDMA_READ:
		case IB_WC_SEND:
			wc->byte_len = qp->wqe_wr_id[qp->sq.cons].bytes_len;
			FP_DP_VERBOSE(dev, QEDR_MSG_CQ, "POLL REQ CQ: op=%d\n",
				      wc->opcode);
			break;
		default:
			break;
		}

		num_entries--;
		wc++;
		cnt++;
next_cqe:
		while (qp->wqe_wr_id[qp->sq.cons].wqe_size--)
			qed_chain_consume(&qp->sq.pbl);
		qedr_inc_sw_cons(&qp->sq);
	}

	return cnt;
}

static int qedr_poll_cq_req(struct qedr_dev *dev,
			    struct qedr_qp *qp, struct qedr_cq *cq,
			    int num_entries, struct ib_wc *wc,
			    struct rdma_cqe_requester *req)
{
	int cnt = 0;

	switch (req->status) {
	case RDMA_CQE_REQ_STS_OK:
		cnt = process_req(dev, qp, cq, num_entries, wc, req->sq_cons,
				  IB_WC_SUCCESS, 0);
		break;
	case RDMA_CQE_REQ_STS_WORK_REQUEST_FLUSHED_ERR:
		if (qp->state != QED_ROCE_QP_STATE_ERR)
			DP_DEBUG(dev, QEDR_MSG_CQ,
				   "Error: POLL CQ with RDMA_CQE_REQ_STS_WORK_REQUEST_FLUSHED_ERR. CQ icid=0x%x, QP icid=0x%x, QP state=%d\n",
				   cq->icid, qp->icid, qp->state);
		cnt = process_req(dev, qp, cq, num_entries, wc, req->sq_cons,
				  IB_WC_WR_FLUSH_ERR, 1);
		break;
	default:
		/* process all WQE before the cosumer */
		qp->state = QED_ROCE_QP_STATE_ERR;
		cnt = process_req(dev, qp, cq, num_entries, wc,
				  req->sq_cons - 1, IB_WC_SUCCESS, 0);
		wc += cnt;
		/* if we have extra WC fill it with actual error info */
		if (cnt < num_entries) {
			enum ib_wc_status wc_status;

			switch (req->status) {
			case RDMA_CQE_REQ_STS_BAD_RESPONSE_ERR:
				DP_ERR(dev,
				       "Error: POLL CQ with RDMA_CQE_REQ_STS_BAD_RESPONSE_ERR. CQ icid=0x%x, QP icid=0x%x\n",
				       cq->icid, qp->icid);
				wc_status = IB_WC_BAD_RESP_ERR;
				break;
			case RDMA_CQE_REQ_STS_LOCAL_LENGTH_ERR:
				DP_ERR(dev,
				       "Error: POLL CQ with RDMA_CQE_REQ_STS_LOCAL_LENGTH_ERR. CQ icid=0x%x, QP icid=0x%x\n",
				       cq->icid, qp->icid);
				wc_status = IB_WC_LOC_LEN_ERR;
				break;
			case RDMA_CQE_REQ_STS_LOCAL_QP_OPERATION_ERR:
				DP_ERR(dev,
				       "Error: POLL CQ with RDMA_CQE_REQ_STS_LOCAL_QP_OPERATION_ERR. CQ icid=0x%x, QP icid=0x%x\n",
				       cq->icid, qp->icid);
				wc_status = IB_WC_LOC_QP_OP_ERR;
				break;
			case RDMA_CQE_REQ_STS_LOCAL_PROTECTION_ERR:
				DP_ERR(dev,
				       "Error: POLL CQ with RDMA_CQE_REQ_STS_LOCAL_PROTECTION_ERR. CQ icid=0x%x, QP icid=0x%x\n",
				       cq->icid, qp->icid);
				wc_status = IB_WC_LOC_PROT_ERR;
				break;
			case RDMA_CQE_REQ_STS_MEMORY_MGT_OPERATION_ERR:
				DP_ERR(dev,
				       "Error: POLL CQ with RDMA_CQE_REQ_STS_MEMORY_MGT_OPERATION_ERR. CQ icid=0x%x, QP icid=0x%x\n",
				       cq->icid, qp->icid);
				wc_status = IB_WC_MW_BIND_ERR;
				break;
			case RDMA_CQE_REQ_STS_REMOTE_INVALID_REQUEST_ERR:
				DP_ERR(dev,
				       "Error: POLL CQ with RDMA_CQE_REQ_STS_REMOTE_INVALID_REQUEST_ERR. CQ icid=0x%x, QP icid=0x%x\n",
				       cq->icid, qp->icid);
				wc_status = IB_WC_REM_INV_REQ_ERR;
				break;
			case RDMA_CQE_REQ_STS_REMOTE_ACCESS_ERR:
				DP_ERR(dev,
				       "Error: POLL CQ with RDMA_CQE_REQ_STS_REMOTE_ACCESS_ERR. CQ icid=0x%x, QP icid=0x%x\n",
				       cq->icid, qp->icid);
				wc_status = IB_WC_REM_ACCESS_ERR;
				break;
			case RDMA_CQE_REQ_STS_REMOTE_OPERATION_ERR:
				DP_ERR(dev,
				       "Error: POLL CQ with RDMA_CQE_REQ_STS_REMOTE_OPERATION_ERR. CQ icid=0x%x, QP icid=0x%x\n",
				       cq->icid, qp->icid);
				wc_status = IB_WC_REM_OP_ERR;
				break;
			case RDMA_CQE_REQ_STS_RNR_NAK_RETRY_CNT_ERR:
				DP_ERR(dev,
				       "Error: POLL CQ with RDMA_CQE_REQ_STS_RNR_NAK_RETRY_CNT_ERR. CQ icid=0x%x, QP icid=0x%x\n",
				       cq->icid, qp->icid);
				wc_status = IB_WC_RNR_RETRY_EXC_ERR;
				break;
			case RDMA_CQE_REQ_STS_TRANSPORT_RETRY_CNT_ERR:
				DP_ERR(dev,
				       "Error: POLL CQ with ROCE_CQE_REQ_STS_TRANSPORT_RETRY_CNT_ERR. CQ icid=0x%x, QP icid=0x%x\n",
				       cq->icid, qp->icid);
				wc_status = IB_WC_RETRY_EXC_ERR;
				break;
			default:
				DP_ERR(dev,
				       "Error: POLL CQ with IB_WC_GENERAL_ERR. CQ icid=0x%x, QP icid=0x%x\n",
				       cq->icid, qp->icid);
				wc_status = IB_WC_GENERAL_ERR;
			}
			cnt += process_req(dev, qp, cq, 1, wc, req->sq_cons,
					   wc_status, 1);
		}
	}

	return cnt;
}

static inline int qedr_cqe_resp_status_to_ib(u8 status)
{
	switch (status) {
	case RDMA_CQE_RESP_STS_LOCAL_ACCESS_ERR:
		return IB_WC_LOC_ACCESS_ERR;
	case RDMA_CQE_RESP_STS_LOCAL_LENGTH_ERR:
		return IB_WC_LOC_LEN_ERR;
	case RDMA_CQE_RESP_STS_LOCAL_QP_OPERATION_ERR:
		return IB_WC_LOC_QP_OP_ERR;
	case RDMA_CQE_RESP_STS_LOCAL_PROTECTION_ERR:
		return IB_WC_LOC_PROT_ERR;
	case RDMA_CQE_RESP_STS_MEMORY_MGT_OPERATION_ERR:
		return IB_WC_MW_BIND_ERR;
	case RDMA_CQE_RESP_STS_REMOTE_INVALID_REQUEST_ERR:
		return IB_WC_REM_INV_RD_REQ_ERR;
	case RDMA_CQE_RESP_STS_OK:
		return IB_WC_SUCCESS;
	default:
		return IB_WC_GENERAL_ERR;
	}
}

static inline int qedr_set_ok_cqe_resp_wc(struct rdma_cqe_responder *resp,
					  struct ib_wc *wc)
{
	wc->status = IB_WC_SUCCESS;
	wc->byte_len = le32_to_cpu(resp->length);

	if (resp->flags & QEDR_RESP_IMM) {
		wc->ex.imm_data = le32_to_cpu(resp->imm_data_or_inv_r_Key);
		wc->wc_flags |= IB_WC_WITH_IMM;

		if (resp->flags & QEDR_RESP_RDMA)
			wc->opcode = IB_WC_RECV_RDMA_WITH_IMM;

		if (resp->flags & QEDR_RESP_INV)
			return -EINVAL;

	} else if (resp->flags & QEDR_RESP_INV) {
		wc->ex.imm_data = le32_to_cpu(resp->imm_data_or_inv_r_Key);
		wc->wc_flags |= IB_WC_WITH_INVALIDATE;

		if (resp->flags & QEDR_RESP_RDMA)
			return -EINVAL;

	} else if (resp->flags & QEDR_RESP_RDMA) {
		return -EINVAL;
	}

	return 0;
}

static void __process_resp_one(struct qedr_dev *dev, struct qedr_qp *qp,
			       struct qedr_cq *cq, struct ib_wc *wc,
			       struct rdma_cqe_responder *resp, u64 wr_id)
{
	/* Must fill fields before qedr_set_ok_cqe_resp_wc() */
	wc->opcode = IB_WC_RECV;
	wc->wc_flags = 0;

	if (likely(resp->status == RDMA_CQE_RESP_STS_OK)) {
		if (qedr_set_ok_cqe_resp_wc(resp, wc))
			DP_ERR(dev,
			       "CQ %p (icid=%d) has invalid CQE responder flags=0x%x\n",
			       cq, cq->icid, resp->flags);

	} else {
		wc->status = qedr_cqe_resp_status_to_ib(resp->status);
		if (wc->status == IB_WC_GENERAL_ERR)
			DP_ERR(dev,
			       "CQ %p (icid=%d) contains an invalid CQE status %d\n",
			       cq, cq->icid, resp->status);
	}

	/* Fill the rest of the WC */
	wc->vendor_err = 0;
	wc->src_qp = qp->id;
	wc->qp = &qp->ibqp;
	wc->wr_id = wr_id;

	FP_DP_VERBOSE(dev, QEDR_MSG_CQ,
		      "POLL CQ RESP: cq %p, icid=%d, qp=%p, status=0x%x, wr_id=0x%llx wc_flags=0x%x, data=0x%x, resp_len=%d,\n",
		      cq, cq->icid, qp, wc->status, wr_id, wc->wc_flags,
		      wc->ex.imm_data, wc->byte_len);
}

static int process_resp_one_srq(struct qedr_dev *dev, struct qedr_qp *qp,
				struct qedr_cq *cq, struct ib_wc *wc,
				struct rdma_cqe_responder *resp)
{
	struct qedr_srq *srq = qp->srq;
	struct qed_chain *pbl;
	u64 wr_id;

	pbl = &srq->hw_srq.pbl;
	wr_id = HILO_GEN(resp->srq_wr_id.hi, resp->srq_wr_id.lo, u64);

	if (resp->status == RDMA_CQE_RESP_STS_WORK_REQUEST_FLUSHED_ERR) {
		wc->status = IB_WC_WR_FLUSH_ERR;
		wc->vendor_err = 0;
		wc->wr_id = wr_id;
		wc->byte_len = 0;
		wc->src_qp = qp->id;
		wc->qp = &qp->ibqp;
		wc->wr_id = wr_id;
	} else {
		__process_resp_one(dev, qp, cq, wc, resp, wr_id);
	}

	/* PBL is maintained in case of WR granularity.
	 * So increment WR consumer after consuming WR
	 */
	qedr_inc_srq_wr_cons(&srq->hw_srq);

	DP_VERBOSE(dev, QEDR_MSG_SRQ, "process_resp_one_srq srq:%p srq_id%d elem_left:%d wr_prod_cnt:%d wr_cons_cnt:%d\n",
		   srq, srq->srq_id,
		   qed_chain_get_elem_left_u32(pbl),
		   srq->hw_srq.wr_prod_cnt,
		   (u32)atomic_read(&srq->hw_srq.wr_cons_cnt));

	return 1;
}

static int process_resp_one(struct qedr_dev *dev, struct qedr_qp *qp,
			    struct qedr_cq *cq, struct ib_wc *wc,
			    struct rdma_cqe_responder *resp)
{
	u64 wr_id = qp->rqe_wr_id[qp->rq.cons].wr_id;

	__process_resp_one(dev, qp, cq, wc, resp, wr_id);

	while (qp->rqe_wr_id[qp->rq.cons].wqe_size--)
		qed_chain_consume(&qp->rq.pbl);
	qedr_inc_sw_cons(&qp->rq);

	return 1;
}

static int process_resp_flush(struct qedr_qp *qp, struct qedr_cq *cq,
			      int num_entries, struct ib_wc *wc, u16 hw_cons)
{
	u16 cnt = 0;

	while (num_entries && qp->rq.wqe_cons != hw_cons) {
		/* fill WC */
		wc->status = IB_WC_WR_FLUSH_ERR;
		wc->vendor_err = 0;
		wc->wc_flags = 0;
		wc->src_qp = qp->id;
		wc->byte_len = 0;
		wc->wr_id = qp->rqe_wr_id[qp->rq.cons].wr_id;
		wc->qp = &qp->ibqp;
		num_entries--;
		wc++;
		cnt++;
		while (qp->rqe_wr_id[qp->rq.cons].wqe_size--)
			qed_chain_consume(&qp->rq.pbl);
		qedr_inc_sw_cons(&qp->rq);
	}

	return cnt;
}

static void try_consume_resp_cqe(struct qedr_cq *cq, struct qedr_qp *qp,
				 struct rdma_cqe_responder *resp, int *update)
{
	if (le16_to_cpu(resp->rq_cons_or_srq_id) == qp->rq.wqe_cons) {
		consume_cqe(cq);
		*update |= 1;
	}
}

static int qedr_poll_cq_resp_srq(struct qedr_dev *dev, struct qedr_qp *qp,
				 struct qedr_cq *cq, int num_entries,
				 struct ib_wc *wc,
				 struct rdma_cqe_responder *resp, int *update)
{
	int cnt;

	cnt = process_resp_one_srq(dev, qp, cq, wc, resp);
	consume_cqe(cq);
	*update |= 1;

	return cnt;
}

static int qedr_poll_cq_resp(struct qedr_dev *dev, struct qedr_qp *qp,
			     struct qedr_cq *cq, int num_entries,
			     struct ib_wc *wc, struct rdma_cqe_responder *resp,
			     int *update)
{
	int cnt;

	if (resp->status == RDMA_CQE_RESP_STS_WORK_REQUEST_FLUSHED_ERR) {
		cnt = process_resp_flush(qp, cq, num_entries, wc,
					 resp->rq_cons_or_srq_id);
		try_consume_resp_cqe(cq, qp, resp, update);
	} else {
		cnt = process_resp_one(dev, qp, cq, wc, resp);
		consume_cqe(cq);
		*update |= 1;
	}

	return cnt;
}

static void try_consume_req_cqe(struct qedr_cq *cq, struct qedr_qp *qp,
				struct rdma_cqe_requester *req, int *update)
{
	if (le16_to_cpu(req->sq_cons) == qp->sq.wqe_cons) {
		consume_cqe(cq);
		*update |= 1;
	}
}

static int qedr_poll_qp_hwq(struct qedr_dev *dev, struct qedr_qp *qp,
			    int num_entries, struct ib_wc *wc, bool sq)
{
	struct qedr_qp_hwq_info *hwq = (sq) ? &qp->sq : &qp->rq;
	int done = 0;

	while (num_entries && hwq->cons != hwq->prod) {
		if (sq)
			wc->wr_id = qp->wqe_wr_id[hwq->cons].wr_id;
		else
			wc->wr_id = qp->rqe_wr_id[hwq->cons].wr_id;

		wc->status = IB_WC_WR_FLUSH_ERR;
		wc->qp = &qp->ibqp;
		wc++;

		num_entries--;
		done++;
		qedr_inc_sw_cons(hwq);
	}

	return done;
}

static int qedr_poll_sw_comp(struct qedr_dev *dev, struct qedr_cq *cq,
			     int num_entries, struct ib_wc *wc)
{
	unsigned long flags;
	struct qedr_qp *qp;
	int done = 0;

	spin_lock_irqsave(&cq->cq_lock, flags);

	list_for_each_entry(qp, &cq->sq_qp_list, cq_sq_list_entry)
		done += qedr_poll_qp_hwq(dev, qp, num_entries - done,
					 wc + done, true);

	list_for_each_entry(qp, &cq->rq_qp_list, cq_rq_list_entry)
		done += qedr_poll_qp_hwq(dev, qp, num_entries - done,
					 wc + done, false);

	spin_unlock_irqrestore(&cq->cq_lock, flags);

	return done;
}

int qedr_poll_cq(struct ib_cq *ibcq, int num_entries, struct ib_wc *wc)
{
	struct qedr_dev *dev = get_qedr_dev(ibcq->device);
	struct qedr_cq *cq = get_qedr_cq(ibcq);
	union rdma_cqe *cqe;
	u32 old_cons, cq_cons;
	unsigned long flags;
	int update = 0;
	int done = 0;

	if (cq->destroyed) {
		DP_ERR(dev,
		       "warning: poll was invoked after destroy for cq %p (icid=%d)\n",
		       cq, cq->icid);
		return 0;
	}

	if (cq->cq_type == QEDR_CQ_TYPE_GSI)
		return qedr_gsi_poll_cq(ibcq, num_entries, wc);

	if (cq->recov_info.reset || cq->polled_for_reset_qp) {
		cq->polled_for_reset_qp = false;

		return qedr_poll_sw_comp(dev, cq, num_entries, wc);
	}

	spin_lock_irqsave(&cq->cq_lock, flags);
	cqe = cq->latest_cqe;
	old_cons = qed_chain_get_cons_idx_u32(&cq->pbl);
	while (num_entries && is_valid_cqe(cq, cqe)) {
		struct qedr_qp *qp;
		int cnt = 0;

		/* prevent speculative reads of any field of CQE */
		rmb();

		qp = cqe_get_qp(cqe);
		if (!qp) {
			WARN(1, "Error: CQE QP pointer is NULL. CQE=%p\n", cqe);
			break;
		}

		wc->qp = &qp->ibqp;

		switch (cqe_get_type(cqe)) {
		case RDMA_CQE_TYPE_REQUESTER:
			cnt = qedr_poll_cq_req(dev, qp, cq, num_entries, wc,
					       &cqe->req);
			try_consume_req_cqe(cq, qp, &cqe->req, &update);
			break;
		case RDMA_CQE_TYPE_RESPONDER_RQ:
			cnt = qedr_poll_cq_resp(dev, qp, cq, num_entries, wc,
						&cqe->resp, &update);
			break;
		case RDMA_CQE_TYPE_RESPONDER_SRQ:
			cnt = qedr_poll_cq_resp_srq(dev, qp, cq, num_entries,
						    wc, &cqe->resp, &update);
			break;
		case RDMA_CQE_TYPE_INVALID:
		default:
			DP_ERR(dev, "Error: invalid CQE type = %d\n",
			       cqe_get_type(cqe));
		}
		num_entries -= cnt;
		wc += cnt;
		done += cnt;

		cqe = get_cqe(cq);
	}

	if (update) { /* QEDR_UPSTREAM_REMOVE_COLON */
		/* doorbell notifies abount latest VALID entry,
		 * but chain already point to the next INVALID one
		 */
		cq_cons = qed_chain_get_cons_idx_u32(&cq->pbl) - 1;
		doorbell_cq(cq, cq_cons, cq->arm_flags);
		FP_DP_VERBOSE(dev, QEDR_MSG_CQ,
			      "doorbell_cq cq=%p cons=%x arm_flags=%x db icid=%d\n",
			      cq, cq_cons, cq->arm_flags,
			      cq->db.data.icid);
	} /* QEDR_UPSTREAM_REMOVE_COLON */

	spin_unlock_irqrestore(&cq->cq_lock, flags);
	return done;
}

#ifdef DEFINE_REG_PHYS_MR /* ! QEDR_UPSTREAM */
struct ib_mr *qedr_reg_kernel_mr(struct ib_pd *ibpd,
				   struct ib_phys_buf *buf_list,
				   int buf_cnt, int acc, u64 *iova_start)
{
	int status = -ENOMEM;
	struct qedr_dev *dev = get_qedr_dev(ibpd->device);

	DP_ERR(dev, "Kernel MR not supported yet\n");

	return ERR_PTR(status);
}
#endif

#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
int qedr_query_ah(struct ib_ah *ibah, struct rdma_ah_attr *attr)
{
	pr_err("Query AH not supported\n");
	return -EINVAL;
}

int qedr_modify_ah(struct ib_ah *ibah, struct rdma_ah_attr *attr)
{
	/* modify_ah is unsupported */
	pr_err("Modify AH not supported\n");
	return -ENOSYS;
}
#endif

#ifdef DEFINE_PROCESS_MAD_VARIABLE_SIZE  /* QEDR_UPSTREAM */
#ifdef DEFINE_PROCESS_MAD_CONST_IB_MAD_HDR
int qedr_process_mad(struct ib_device *ibdev, int process_mad_flags,
		     COMPAT_PORT(port_num),
		     const struct ib_wc *in_wc,
		     const struct ib_grh *in_grh,
		     const struct ib_mad_hdr *mad_hdr,
		     size_t in_mad_size, struct ib_mad_hdr *out_mad,
		     size_t *out_mad_size, u16 *out_mad_pkey_index)
#else
int qedr_process_mad(struct ib_device *ibdev,
		     int process_mad_flags,
		     COMPAT_PORT(port_num),
		     const struct ib_wc *in_wc,
		     const struct ib_grh *in_grh,
		     const struct ib_mad *in_mad,
		     struct ib_mad *out_mad,
		     size_t *out_mad_size,
		     u16 *out_mad_pkey_index)
#endif
#elif defined(DEFINE_PROCESS_MAD_CONST_INPUTS)
int qedr_process_mad(struct ib_device *ibdev,
		     int process_mad_flags,
		     COMPAT_PORT(port_num),
		     const struct ib_wc *in_wc,
		     const struct ib_grh *in_grh,
		     const struct ib_mad *in_mad,
		     struct ib_mad *out_mad)
#else
int qedr_process_mad(struct ib_device *ibdev,
		     int process_mad_flags,
		     COMPAT_PORT(port_num),
		     struct ib_wc *in_wc,
		     struct ib_grh *in_grh,
		     struct ib_mad *in_mad, struct ib_mad *out_mad)
#endif
{
	struct qedr_dev *dev = get_qedr_dev(ibdev);

#ifdef DEFINE_PROCESS_MAD_VARIABLE_SIZE  /* QEDR_UPSTREAM */
#ifdef DEFINE_PROCESS_MAD_CONST_IB_MAD_HDR
	DP_DEBUG(dev, QEDR_MSG_GSI,
		 "QEDR_PROCESS_MAD in_mad %x %x %x %x %x %x %x %x\n",
		 mad_hdr->attr_id, mad_hdr->base_version, mad_hdr->attr_mod,
		 mad_hdr->class_specific, mad_hdr->class_version,
		 mad_hdr->method, mad_hdr->mgmt_class, mad_hdr->status);
#else
	DP_VERBOSE(dev, QEDR_MSG_GSI, "QEDR_PROCESS_MAD in_mad %x %x %x %x %x %x %x %x\n",
	       in_mad->mad_hdr.attr_id, in_mad->mad_hdr.base_version,
	       in_mad->mad_hdr.attr_mod, in_mad->mad_hdr.class_specific,
	       in_mad->mad_hdr.class_version, in_mad->mad_hdr.method,
	       in_mad->mad_hdr.mgmt_class, in_mad->mad_hdr.status);
#endif

#else
	DP_VERBOSE(dev, QEDR_MSG_GSI, "QEDR_PROCESS_MAD in_mad %x %x %x %x %x %x %x %x\n",
	       in_mad->mad_hdr.attr_id, in_mad->mad_hdr.base_version,
	       in_mad->mad_hdr.attr_mod, in_mad->mad_hdr.class_specific,
	       in_mad->mad_hdr.class_version, in_mad->mad_hdr.method,
	       in_mad->mad_hdr.mgmt_class, in_mad->mad_hdr.status);
#endif
	return IB_MAD_RESULT_SUCCESS;
}

#ifdef _HAS_IB_DRAIN
struct qedr_ib_drain_cqe {
	struct ib_cqe cqe;
	struct completion done;
};

static void qedr_ib_drain_qp_done(struct ib_cq *cq, struct ib_wc *wc)
{
	struct qedr_ib_drain_cqe *cqe = container_of(wc->wr_cqe,
						     struct qedr_ib_drain_cqe,
						     cqe);

	complete(&cqe->done);
}

static void handle_drain_completion(struct ib_cq *ibcq,
				    struct qedr_ib_drain_cqe *drain,
				    struct qedr_dev *dev)
{
	struct qedr_cq *cq = get_qedr_cq(ibcq);

	if (ibcq->poll_ctx == IB_POLL_DIRECT) {
		while (wait_for_completion_timeout(&drain->done, HZ / 10) <= 0)
			ib_process_cq_direct(ibcq, -1);

		return;
	}

	if (cq->recov_info.reset || cq->polled_for_reset_qp) {
		bool triggered = false;
		unsigned long flags;

		spin_lock_irqsave(&dev->recov_info.recov_lock, flags);
		/* Make sure that the CQ handler won't run if wasn't run yet */
		if (!cq->reset_notify_added)
			cq->reset_notify_added = 1;
		else
			triggered = true;

		if (!cq->recov_info.reset)
			cq->reset_notify_added = false;

		spin_unlock_irqrestore(&dev->recov_info.recov_lock, flags);

		if (triggered) {
			/* Wait for any scheduled/running task to be ended */
			switch (ibcq->poll_ctx) {
			case IB_POLL_SOFTIRQ:
				irq_poll_disable(&ibcq->iop);
				irq_poll_enable(&ibcq->iop);
				break;
			case IB_POLL_WORKQUEUE:
				cancel_work_sync(&ibcq->work);
				break;
			default:
				WARN_ON_ONCE(1);
			}
		}

		if (cq->ibcq.comp_handler)
			(*cq->ibcq.comp_handler)(&cq->ibcq,
						 cq->ibcq.cq_context);
	}

	wait_for_completion_timeout(&drain->done, msecs_to_jiffies(5000));
}

void qedr_drain_sq(struct ib_qp *ibqp)
{
	struct ib_qp_attr attr = { .qp_state = IB_QPS_ERR };
	struct qedr_dev *dev = get_qedr_dev(ibqp->device);
	struct qedr_qp *qp = get_qedr_qp(ibqp);
	IB_CONST struct ib_send_wr *bad_swr;
	struct ib_cq *cq = ibqp->send_cq;
	struct qedr_ib_drain_cqe sdrain;
	struct ib_rdma_wr swr = {
		.wr = {
			.next = NULL,
			{ .wr_cqe	= &sdrain.cqe, },
			.opcode	= IB_WR_RDMA_WRITE,
		},
	};
	struct qedr_cq *qcq;
	int ret;

	qcq = get_qedr_cq(cq);

	ret = ib_modify_qp(ibqp, &attr, IB_QP_STATE);
	if (ret && !qp->recov_info.reset) {
		WARN_ONCE(ret, "failed to drain send queue: %d\n", ret);

		return;
	}

	sdrain.cqe.done = qedr_ib_drain_qp_done;
	init_completion(&sdrain.done);

	ret = qedr_post_send_drain(&qp->ibqp, &swr.wr, &bad_swr);
	if (ret) {
		WARN_ONCE(ret, "failed to drain send queue: %d\n", ret);

		return;
	}

	if (qp->recov_info.reset)
		qcq->polled_for_reset_qp = true;

	handle_drain_completion(cq, &sdrain, dev);
}

void qedr_drain_rq(struct ib_qp *ibqp)
{
	struct ib_qp_attr attr = { .qp_state = IB_QPS_ERR };
	struct qedr_dev *dev = get_qedr_dev(ibqp->device);
	IB_CONST struct ib_recv_wr *bad_rwr;
	struct qedr_qp *qp = get_qedr_qp(ibqp);
	struct ib_cq *cq = ibqp->recv_cq;
	struct qedr_ib_drain_cqe rdrain;
	struct ib_recv_wr rwr = {};
	struct qedr_cq *qcq;
	int ret;

	qcq = get_qedr_cq(cq);

	ret = ib_modify_qp(ibqp, &attr, IB_QP_STATE);
	if (ret && !qp->recov_info.reset) {
		WARN_ONCE(ret, "failed to drain recv queue: %d\n", ret);

		return;
	}

	rwr.wr_cqe = &rdrain.cqe;
	rdrain.cqe.done = qedr_ib_drain_qp_done;
	init_completion(&rdrain.done);

	ret = qedr_post_recv_drain(ibqp, &rwr, &bad_rwr);
	if (ret) {
		WARN_ONCE(ret, "failed to drain recv queue: %d\n", ret);

		return;
	}

	if (qp->recov_info.reset)
		qcq->polled_for_reset_qp = true;

	handle_drain_completion(cq, &rdrain, dev);
}
#endif
