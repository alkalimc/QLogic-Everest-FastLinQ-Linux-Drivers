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

#ifndef __QEDR_VERBS_H__
#define __QEDR_VERBS_H__

#ifdef DEFINE_QUERY_DEVICE_PASS_VENDOR_SPECIFIC_DATA /* QEDR_UPSTREAM */
int qedr_query_device(struct ib_device *ibdev,
		      struct ib_device_attr *attr, struct ib_udata *udata);
#else
int qedr_query_device(struct ib_device *, struct ib_device_attr *props);

#endif
int qedr_query_port(struct ib_device *, COMPAT_PORT(port), struct ib_port_attr *props);
int qedr_modify_port(struct ib_device *, COMPAT_PORT(port), int mask,
		     struct ib_port_modify *props);

#ifndef NOT_DEFINED_GET_CACHED_GID /* !QEDR_UPSTREAM */
int qedr_query_gid(struct ib_device *, COMPAT_PORT(port), int index, union ib_gid *gid);
#endif

int qedr_iw_query_gid(struct ib_device *ibdev, COMPAT_PORT(port),
		      int index, union ib_gid *gid);

int qedr_query_pkey(struct ib_device *, COMPAT_PORT(port), u16 index, u16 *pkey);

#ifndef _HAS_UCONTEXT_ALLOCATION /* !QEDR_UPSTREAM */
struct ib_ucontext *qedr_alloc_ucontext(struct ib_device *, struct ib_udata *);
int qedr_dealloc_ucontext(struct ib_ucontext *);
#else
int qedr_alloc_ucontext(struct ib_ucontext *uctx, struct ib_udata *udata);
void qedr_dealloc_ucontext(struct ib_ucontext *uctx);
#endif

int qedr_mmap(struct ib_ucontext *, struct vm_area_struct *vma);
#if DEFINE_ROCE_GID_TABLE /* QEDR_UPSTREAM */
#ifndef REMOVE_DEVICE_ADD_DEL_GID /*!QEDR_UPSTREAM */
int qedr_del_gid(struct ib_device *device, COMPAT_PORT(port_num),
		 unsigned int index, void **context);
int qedr_add_gid(struct ib_device *device, COMPAT_PORT(port_num),
		 unsigned int index, const union ib_gid *gid,
		 const struct ib_gid_attr *attr, void **context);
#endif
#endif

COMPAT_ALLOC_PD_DECLARE_RET
qedr_alloc_pd(COMPAT_ALLOC_PD_IBDEV(struct ib_device *ibdev)
	      COMPAT_ALLOC_PD_PD(struct ib_pd *ibpd)
	      COMPAT_ALLOC_PD_CXT(struct ib_ucontext *context)
	      struct ib_udata *udata);

COMPAT_DEALLOC_PD_DECLARE_RET
qedr_dealloc_pd(struct ib_pd *ibpd
		COMPAT_DEALLOC_PD_UDATA(struct ib_udata *udata));

COMPAT_QEDR_ALLOC_XRCD_RET_PARAM
qedr_alloc_xrcd(COMPAT_ALLOC_XRCD_FIRST_PARAM,
		COMPAT_ALLOC_XRCD_CXT(struct ib_ucontext *context)
		struct ib_udata *udata);

COMPAT_QEDR_DEALLOC_XRCD_DECLARE_RET
qedr_dealloc_xrcd(struct ib_xrcd *ibxrcd
		      COMPAT_DEALLOC_XRCD_UDATA(struct ib_udata *udata));

#ifdef DEFINE_CREATE_CQ_ATTR  /* QEDR_UPSTREAM */
COMPAT_CREATE_CQ_DECLARE_RET
qedr_create_cq(COMPAT_CREATE_CQ_IBDEV(struct ib_device *ibdev)
	       COMPAT_CREATE_CQ_CQ(struct ib_cq *ibcq)
	       const struct ib_cq_init_attr *attr,
	       COMPAT_CREATE_CQ_CTX(struct ib_ucontext *ib_ctx)
	       struct ib_udata *udata);
#else
struct ib_cq *qedr_create_cq(struct ib_device *, int entries, int vector,
			       struct ib_ucontext *, struct ib_udata *);
#endif

int qedr_resize_cq(struct ib_cq *, int cqe, struct ib_udata *);
COMPAT_DESTROY_CQ_DECLARE_RET
qedr_destroy_cq(struct ib_cq *ibcq
		COMPAT_DESTROY_CQ_UDATA(struct ib_udata *udata));
int qedr_arm_cq(struct ib_cq *ibcq, enum ib_cq_notify_flags flags);
#ifdef _HAS_QP_ALLOCATION
int qedr_create_qp(struct ib_qp *, struct ib_qp_init_attr *qp_init_attr,
		   struct ib_udata *udata);
#else
struct ib_qp *qedr_create_qp(struct ib_pd *, struct ib_qp_init_attr *attrs,
			     struct ib_udata *);
#endif
int qedr_modify_qp(struct ib_qp *, struct ib_qp_attr *attr,
		   int attr_mask, struct ib_udata *udata);
int qedr_query_qp(struct ib_qp *, struct ib_qp_attr *qp_attr,
		  int qp_attr_mask, struct ib_qp_init_attr *);
int qedr_destroy_qp(struct ib_qp *ibqp
		    COMPAT_DESTROY_QP_UDATA(struct ib_udata *udata));

COMPAT_CREATE_SRQ_DECLARE_RET
qedr_create_srq(COMPAT_CREATE_SRQ_IBPD(struct ib_pd *ibpd)
		COMPAT_CREATE_SRQ_SRQ(struct ib_srq *ibsrq)
		struct ib_srq_init_attr *init_attr,
		struct ib_udata *udata);

int qedr_modify_srq(struct ib_srq *, struct ib_srq_attr *,
		      enum ib_srq_attr_mask, struct ib_udata *);
int qedr_query_srq(struct ib_srq *, struct ib_srq_attr *);
COMPAT_DESTROY_SRQ_DECLARE_RET
qedr_destroy_srq(struct ib_srq *ibsrq
		COMPAT_DESTROY_SRQ_UDATA(struct ib_udata *udata));
int qedr_post_srq_recv(struct ib_srq *, IB_CONST struct ib_recv_wr *,
		       IB_CONST struct ib_recv_wr **bad_recv_wr);
int qedr_modify_srq(struct ib_srq *, struct ib_srq_attr *,
		    enum ib_srq_attr_mask, struct ib_udata *);

/* INTERNAL: Create_AH prototypes */
COMPAT_DESTROY_AH_DECLARE_RET
qedr_destroy_ah(struct ib_ah *ibah
		COMPAT_DESTROY_AH_FLAGS(u32 flags));

COMPAT_CREATE_AH_DECLARE_RET
qedr_create_ah(COMPAT_CREATE_AH_IBPD(struct ib_pd *ibpd)
	       COMPAT_CREATE_AH_AH(struct ib_ah *ibah)
	       COMPAT_CREATE_AH_ATTR(attr)
	       COMPAT_CREATE_AH_FLAGS(u32 flags)
	       COMPAT_CREATE_AH_UDATA(struct ib_udata *udata));

#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
int qedr_query_ah(struct ib_ah *ibah, struct rdma_ah_attr *attr);
int qedr_modify_ah(struct ib_ah *ibah, struct rdma_ah_attr *attr);
#endif

int qedr_dereg_mr(struct ib_mr *ib_mr
		  COMPAT_DEREG_MR_UDATA(struct ib_udata *udata));

struct ib_mr *qedr_get_dma_mr(struct ib_pd *, int acc);
#ifdef DEFINE_REG_PHYS_MR /* ! QEDR_UPSTREAM */
struct ib_mr *qedr_reg_kernel_mr(struct ib_pd *,
				   struct ib_phys_buf *buffer_list,
				   int num_phys_buf, int acc, u64 *iova_start);
#endif

#ifdef DEFINE_USER_NO_MR_ID /* QEDR_UPSTREAM */
struct ib_mr *qedr_reg_user_mr(struct ib_pd *, u64 start, u64 length,
			       u64 virt, int acc, struct ib_udata *);
#else
struct ib_mr *qedr_reg_user_mr(struct ib_pd *, u64 start, u64 length,
			       u64 virt, int acc, struct ib_udata *,
			       int mr_id);
#endif

#ifdef DEFINE_MAP_MR_SG /* QEDR_UPSTREAM */
#ifdef DEFINE_MAP_MR_SG_OFFSET /* QEDR_UPSTREAM */
int qedr_map_mr_sg(struct ib_mr *ibmr, struct scatterlist *sg,
		   int sg_nents, unsigned int *sg_offset);
#else
#ifndef DEFINE_MAP_MR_SG_UNSIGNED
int qedr_map_mr_sg(struct ib_mr *ibmr, struct scatterlist *sg,
		   int sg_nents);
#else
int qedr_map_mr_sg(struct ib_mr *ibmr, struct scatterlist *sg,
		   unsigned int sg_nents);
#endif
#endif
#endif

#ifdef DEFINE_ALLOC_MR /* QEDR_UPSTREAM */
struct ib_mr *qedr_alloc_mr(struct ib_pd *ibpd,
			    enum ib_mr_type mr_type, u32 max_num_sg
			    COMPAT_ALLOC_MR_UDATA(struct ib_udata *udata));
#else
struct ib_mr *qedr_alloc_frmr(struct ib_pd *pd, int max_page_list_len);
struct ib_fast_reg_page_list *qedr_alloc_frmr_page_list(struct ib_device
							*ibdev,
							int page_list_len);
void qedr_free_frmr_page_list(struct ib_fast_reg_page_list *page_list);
#endif
#ifdef _HAS_MW_SUPPORT /* QEDR_UPSTREAM */
#ifdef _HAS_ALLOC_MW_V2 /* QEDR_UPSTREAM */
struct ib_mw *qedr_alloc_mw(struct ib_pd *ibpd,  enum ib_mw_type type,
			    struct ib_udata *udata);
#elif defined(_HAS_ALLOC_MW_V3)
int qedr_alloc_mw(struct ib_mw *ibmw, struct ib_udata *udata);
#else
struct ib_mw *qedr_alloc_mw(struct ib_pd *ibpd,  enum ib_mw_type type);
#endif
int qedr_dealloc_mw(struct ib_mw *ib_mw);
#endif
int qedr_poll_cq(struct ib_cq *, int num_entries, struct ib_wc *wc);
int qedr_post_send(struct ib_qp *, IB_CONST struct ib_send_wr *,
		   IB_CONST struct ib_send_wr **bad_wr, bool drain);
int qedr_post_recv(struct ib_qp *, IB_CONST struct ib_recv_wr *,
		   IB_CONST struct ib_recv_wr **bad_wr, bool drain);
#ifdef DEFINE_PROCESS_MAD_VARIABLE_SIZE /* QEDR_UPSTREAM */
#ifdef DEFINE_PROCESS_MAD_CONST_IB_MAD_HDR
int qedr_process_mad(struct ib_device *ibdev, int process_mad_flags,
		     COMPAT_PORT(port_num), const struct ib_wc *in_wc,
		     const struct ib_grh *in_grh,
		     const struct ib_mad_hdr *in_mad,
		     size_t in_mad_size, struct ib_mad_hdr *out_mad,
		     size_t *out_mad_size, u16 *out_mad_pkey_index);
#else
int qedr_process_mad(struct ib_device *device,
		     int process_mad_flags,
		     COMPAT_PORT(port_num),
		     const struct ib_wc *in_wc,
		     const struct ib_grh *in_grh,
		     const struct ib_mad *in_mad,
		     struct ib_mad *out_mad,
		     size_t *out_mad_size,
		     u16 *out_mad_pkey_index);
#endif
#elif defined(DEFINE_PROCESS_MAD_CONST_INPUTS)
int qedr_process_mad(struct ib_device *ibdev,
		     int process_mad_flags,
		     COMPAT_PORT(port_num),
		     const struct ib_wc *in_wc,
		     const struct ib_grh *in_grh,
		     const struct ib_mad *in_mad,
		     struct ib_mad *out_mad);
#else
int qedr_process_mad(struct ib_device *ibdev,
		     int process_mad_flags,
		     COMPAT_PORT(port_num),
		     struct ib_wc *in_wc,
		     struct ib_grh *in_grh,
		     struct ib_mad *in_mad,
		     struct ib_mad *out_mad);
#endif

#if DEFINE_PORT_IMMUTABLE /* QEDR_UPSTREAM */
int qedr_port_immutable(struct ib_device *ibdev, COMPAT_PORT(port_num),
			struct ib_port_immutable *immutable);
#endif

void qedr_free_pbl(struct qedr_dev *dev,
		   struct qedr_pbl_info *pbl_info, struct qedr_pbl *pbl);

void qedr_iw_qp_add_ref(struct ib_qp *qp);
void qedr_iw_qp_rem_ref(struct ib_qp *qp);
struct ib_qp *qedr_iw_get_qp(struct ib_device *device, int qpn);
int qedr_iw_connect(struct iw_cm_id *cm_id,
		    struct iw_cm_conn_param *conn_param);
int qedr_iw_accept(struct iw_cm_id *cm_id, struct iw_cm_conn_param *conn_param);
int qedr_iw_reject(struct iw_cm_id *cm_id, const void *pdata, u8 pdata_len);
int qedr_iw_create_listen(struct iw_cm_id *cm_id, int backlog);
int qedr_iw_destroy_listen(struct iw_cm_id *cm_id);

#if DEFINE_ROCE_GID_TABLE /* !QEDR_UPSTREAM */
#ifndef DEFINED_GET_L2_FIELDS
int rdma_read_gid_l2_fields(const struct ib_gid_attr *attr,
			    u16 *vlan_id, u8 *smac);
#endif
#endif

#ifndef DEFINE_DEVICE_TO_DRV
#define rdma_device_to_drv_device(dev, drv_dev_struct, ibdev_member)           \
	dev_get_drvdata(dev)
#endif
#endif

static inline int
qedr_post_send_no_drain(struct ib_qp *ibqp, IB_CONST struct ib_send_wr *wr,
			IB_CONST struct ib_send_wr **bad_wr)
{
	return qedr_post_send(ibqp, wr, bad_wr, false);
}

static inline int
qedr_post_send_drain(struct ib_qp *ibqp, IB_CONST struct ib_send_wr *wr,
		     IB_CONST struct ib_send_wr **bad_wr)
{
	return qedr_post_send(ibqp, wr, bad_wr, true);
}

static inline int
qedr_post_recv_no_drain(struct ib_qp *ibqp, IB_CONST struct ib_recv_wr *wr,
			IB_CONST struct ib_recv_wr **bad_wr)
{
	return qedr_post_recv(ibqp, wr, bad_wr, false);
}

static inline int
qedr_post_recv_drain(struct ib_qp *ibqp, IB_CONST struct ib_recv_wr *wr,
		     IB_CONST struct ib_recv_wr **bad_wr)
{
	return qedr_post_recv(ibqp, wr, bad_wr, true);
}

#ifdef _HAS_IB_DRAIN
void qedr_drain_sq(struct ib_qp *qp);
void qedr_drain_rq(struct ib_qp *qp);
#endif
