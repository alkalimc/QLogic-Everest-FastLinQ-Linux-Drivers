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

#ifndef __QEDR_H__
#define __QEDR_H__

#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#endif

#include <linux/pci.h>
#include <linux/idr.h>
#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
#include <rdma/ib_verbs.h>
#include <rdma/ib_user_verbs.h>
#endif
#include <rdma/ib_addr.h>
#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
#define QEDR_MODULE_VERSION		"8.70.12.0"
#define QEDR_ROCE_INTERFACE_VERSION	7012

#if !defined(CONFIG_QED_L2) && !defined(CONFIG_QED_ROCE) && \
    !defined(CONFIG_QED_FCOE) && !defined(CONFIG_QED_ISCSI) && \
    !defined(CONFIG_QED_LL2)
#define CONFIG_QED_L2
#define CONFIG_QED_SRIOV
#define CONFIG_QED_ROCE
#define CONFIG_QED_FCOE
#define CONFIG_QED_ISCSI
#endif
/* INTERNAL: This needs to always be defined for RDMA to work */
#ifndef CONFIG_QED_LL2
#define CONFIG_QED_LL2
#endif

#define CONFIG_IWARP
#include <rdma/iw_cm.h>
#include "qedr_compat.h"
#include "common_hsi.h"
#endif
#include "qedr_hsi_rdma.h"
#include "qed_if.h"
#include "qed_chain.h"
#include "qed_rdma_if.h"
#include "qede_rdma.h"
#include "roce_common.h"
#include "iwarp_common.h"

#define QEDR_NODE_DESC "QLogic 579xx RoCE HCA"
#define DP_NAME(dev) ((dev)->ibdev.name)

#define IS_IWARP(_dev) ((_dev)->rdma_type == QED_RDMA_TYPE_IWARP)
#define IS_ROCE(_dev) ((_dev)->rdma_type == QED_RDMA_TYPE_ROCE)

#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM*/
#define DP_DEBUG DP_VERBOSE
#endif

#define QED_INT_DBG_STORE(P_DEV, ...)					\
	do {								\
		if ((P_DEV) && (P_DEV)->ops)				\
			(P_DEV)->ops->common->internal_trace((P_DEV)->cdev, \
							     __VA_ARGS__); \
	} while (0)

/* INTERNAL: Fast path debug prints */
#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
#define FP_DP_VERBOSE(...)
/* #define FP_DP_VERBOSE(...)	DP_VERBOSE(__VA_ARGS__) */

enum DP_QEDR_MODULE {
	QEDR_MSG_INIT		= 0x10000,
	QEDR_MSG_FAIL		= 0x10000,
	QEDR_MSG_CQ		= 0x20000,
	QEDR_MSG_RQ		= 0x40000,
	QEDR_MSG_SQ		= 0x80000,
	QEDR_MSG_QP		= (QEDR_MSG_SQ | QEDR_MSG_RQ),
	QEDR_MSG_MR		= 0x100000,
	QEDR_MSG_GSI		= 0x200000,
	QEDR_MSG_MISC		= 0x400000,
	QEDR_MSG_SRQ		= 0x800000,
	QEDR_MSG_IWARP		= 0x1000000,
	/* to be added...up to 0x8000000 */
};
#else

#define DP_DEBUG(dev, module, fmt, ...)					\
	pr_debug("(%s) " module ": " fmt,				\
		 DP_NAME(dev) ? DP_NAME(dev) : "", ## __VA_ARGS__)

#define QEDR_MSG_INIT "INIT"
#define QEDR_MSG_MISC "MISC"
#define QEDR_MSG_CQ   "  CQ"
#define QEDR_MSG_MR   "  MR"
#define QEDR_MSG_RQ   "  RQ"
#define QEDR_MSG_SQ   "  SQ"
#define QEDR_MSG_QP   "  QP"
#define QEDR_MSG_GSI  " GSI"
#define QEDR_MSG_IWARP  " IW"

#endif

/* INTERNAL: The following number is used to determine if a handle recevied from the FW
 * actually point to a CQ/QP.
 */
#define QEDR_CQ_MAGIC_NUMBER	(0x11223344)
#define QEDR_QP_MAGIC_NUMBER	(0x77889900)

#define FW_PAGE_SIZE		(RDMA_RING_PAGE_SIZE)
#define FW_PAGE_SHIFT		(12)

struct qedr_dev;

struct qedr_cnq {
	struct qedr_dev		*dev;
	struct qed_chain	pbl;
	struct qed_sb_info	*sb;
	char			name[32];
	u64			n_comp;
	__le16			*hw_cons_ptr;
	u8			index;
};

#define QEDR_MAX_SGID 128 /* TBD - add more source gids... */

struct qedr_device_attr {
	u32	vendor_id;	/* INTERNAL: Vendor specific information */
	u32	vendor_part_id;
	u32	hw_ver;
	u64	fw_ver;
	u64	node_guid;      /* INTERNAL: node GUID */
	u64	sys_image_guid; /* INTERNAL: System image GUID */
	u8	max_cnq;
	u8	max_sge;        /* INTERNAL: Maximum # of scatter/gather entries
				 * per Work Request supported
				 */
	u16	max_inline;
	u32	max_sqe;        /* INTERNAL: Maximum number of send outstanding send work
				 * requests on any Work Queue supported
				 */
	u32	max_rqe;        /* INTERNAL: Maximum number of receive outstanding receive
				 * work requests on any Work Queue supported
				 */
	u8	max_qp_resp_rd_atomic_resc;     /* INTERNAL: Maximum number of RDMA Reads
						 * & atomic operation that can
						 * be outstanding per QP
						 */
	u8	max_qp_req_rd_atomic_resc;      /* INTERNAL: The maximum depth per QP for
						 * initiation of RDMA Read
						 * & atomic operations
						 */
	u64	max_dev_resp_rd_atomic_resc;
	u32	max_cq;
	u32	max_qp;
	u32	max_mr;         /* INTERNAL:  Maximum # of MRs supported */
	u64	max_mr_size;    /* INTERNAL: Size (in bytes) of largest contiguous memory
				 * block that can be registered by this device
				 */
	u32	max_cqe;
	u32	max_mw;         /* INTERNAL: Maximum # of memory windows supported */
#ifdef _HAS_FMR_SUPPORT /* !QEDR_UPSTREAM */
	u32	max_fmr;
#endif
	u32	max_mr_mw_fmr_pbl;
	u64	max_mr_mw_fmr_size;
	u32	max_pd;         /* INTERNAL: Maximum # of protection domains supported */
	u32	max_ah;
	u8	max_pkey;
	u32	max_srq;        /* INTERNAL: Maximum number of SRQs */
	u32	max_srq_wr;     /* INTERNAL: Maximum number of WRs per SRQ */
	u8	max_srq_sge;     /* INTERNAL: Maximum number of SGE per WQE */
	u8	max_stats_queues; /* INTERNAL: Maximum number of statistics queues */

	u64	page_size_caps;
	u8	dev_ack_delay;
	u32	reserved_lkey;	 /* INTERNAL Value of reserved L_key */
	u32	bad_pkey_counter;/* INTERNAL: Bad P_key counter support indicator */
	struct qed_rdma_events events;
};

#if DEFINE_THIS_CPU_INC /* ! QEDR_UPSTREAM */
struct qedr_stats {
	u64 send_wr[MAX_RDMA_SQ_REQ_TYPE];
	u64 recv_wr;
	u64 send_bad_wr;
	u64 recv_bad_wr;
};
#endif

#define QEDR_ENET_STATE_BIT	(0)

struct qedr_idr {
	spinlock_t idr_lock; /* Protect idr data-structure */
	struct idr idr;
};

struct qedr_recov_info {
	spinlock_t recov_lock; /* lock for recovery flows and objects */
	bool dead;	/* device experienced internal error and is unusable */
	bool recov_in_prog;
	struct completion recov_comp;
	struct kref recov_refcnt;
	struct list_head recov_obj_list;
};

struct qedr_dev {
	struct ib_device	ibdev;
	struct qed_dev		*cdev;
	struct pci_dev		*pdev;
	struct net_device	*ndev;

	enum ib_atomic_cap	atomic_cap;

	void *rdma_ctx;
	struct qedr_device_attr attr;

	const struct qed_rdma_ops *ops;
	struct qed_int_info	int_info;

	struct qed_sb_info	*sb_array;
	struct qedr_cnq		*cnq_array;
	int			num_cnq;
	int			sb_start;

	void __iomem		*db_addr;
	u64			db_phys_addr;
	u32			db_size;
	u16			dpi;
	u16			wid_count;

	union ib_gid *sgid_tbl;

	/* Lock for sgid table */
	spinlock_t sgid_lock;
	u64			guid;
#ifndef DEFINE_NO_IP_BASED_GIDS /* !QEDR_UPSTREAM */
	struct notifier_block nb_inet;
#endif
#if IS_ENABLED(CONFIG_IPV6) /* !QEDR_UPSTREAM */
	struct notifier_block nb_inet6;
#endif
#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
	struct dentry *dbgfs;
	struct kobject *sysfs;
#endif
	u32			debug_msglvl;
	u32			dp_module;
	u8			dp_level;
	u32			dp_int_module;
	u8			dp_int_level;
	u8			num_hwfns;
#define QEDR_IS_CMT(dev)	((dev)->num_hwfns > 1)
	u8			affin_hwfn_idx;

	u8			gsi_ll2_handle;

	uint			wq_multiplier;
	u8			gsi_ll2_mac_address[ETH_ALEN];
	int			gsi_qp_created;
	struct qedr_cq		*gsi_sqcq;
	struct qedr_cq		*gsi_rqcq;
	struct qedr_qp		*gsi_qp;
	struct tasklet_struct	*vf_ll2_tasklet;
	bool			b_vf_ll2_tasklet_en;
	enum qed_rdma_type	rdma_type;
	struct qedr_idr		srqidr;
	struct qedr_idr		qpidr;
	struct workqueue_struct *iwarp_wq;
	u16			iwarp_max_mtu;

	unsigned long enet_state;

	u8 dpm_enabled;
	u8 kernel_ldpm_enabled;

#if DEFINE_THIS_CPU_INC /* ! QEDR_UPSTREAM */
	struct qedr_stats __percpu *stats;
#endif
#ifndef QEDR_UPSTREAM /* !QEDR_UPSTREAM */
	bool insert_udp_src_port;
	u16 new_udp_src_port;
#endif

	bool lag_enabled;
	bool is_vf;

	enum qed_dev_type dev_type;
#define QEDR_IS_BB(dev)	((dev)->dev_type == QED_DEV_TYPE_BB)
#define QEDR_IS_AH(dev)	((dev)->dev_type == QED_DEV_TYPE_AH)

	struct qedr_recov_info recov_info;
};

struct qedr_recov_obj_info {
	struct list_head entry;
	bool reset;

	void (*recov_cb)(struct qedr_dev *dev,
			 struct qedr_recov_obj_info *info, int phase);

	bool added;
};

/* INTERNAL: QEDR Limitations */

/* INTERNAL: SQ/RQ Limitations
 * An S/RQ PBL contains a list a pointers to pages. Each page contains S/RQE
 * elements. Several S/RQE elements make an S/RQE, up to a certain maximum that
 * is different between SQ and RQ. The size of the PBL was chosen such as not to
 * limit the MAX_WR supported by ECORE, and rounded up to a power of two.
 */
/* INTERNAL: SQ */
#define QEDR_MAX_SQ_PBL			(0x8000)
#define QEDR_MAX_SQ_PBL_ENTRIES		(0x10000 / sizeof(void *))
#define QEDR_SQE_ELEMENT_SIZE		(sizeof(struct rdma_sq_sge))
#define QEDR_MAX_SQE_ELEMENTS_PER_SQE	(ROCE_REQ_MAX_SINGLE_SQ_WQE_SIZE / \
					 QEDR_SQE_ELEMENT_SIZE)
#define QEDR_MAX_SQE_ELEMENTS_PER_PAGE	((RDMA_RING_PAGE_SIZE) / \
					 QEDR_SQE_ELEMENT_SIZE)
#define QEDR_MAX_SQE			((QEDR_MAX_SQ_PBL_ENTRIES) *\
					 (RDMA_RING_PAGE_SIZE) / \
					 (QEDR_SQE_ELEMENT_SIZE) /\
					 (QEDR_MAX_SQE_ELEMENTS_PER_SQE))
/* RQ */
#define QEDR_MAX_RQ_PBL			(0x2000)
#define QEDR_MAX_RQ_PBL_ENTRIES		(0x10000 / sizeof(void *))
#define QEDR_RQE_ELEMENT_SIZE		(sizeof(struct rdma_rq_sge))
#define QEDR_MAX_RQE_ELEMENTS_PER_RQE	(RDMA_MAX_SGE_PER_RQ_WQE)
#define QEDR_MAX_RQE_ELEMENTS_PER_PAGE	((RDMA_RING_PAGE_SIZE) / \
					 QEDR_RQE_ELEMENT_SIZE)
#define QEDR_MAX_RQE			((QEDR_MAX_RQ_PBL_ENTRIES) *\
					 (RDMA_RING_PAGE_SIZE) / \
					 (QEDR_RQE_ELEMENT_SIZE) /\
					 (QEDR_MAX_RQE_ELEMENTS_PER_RQE))
/* INTERNAL: CQE Limitation
 * Although FW supports two layer PBL we use single layer since it is more
 * than enough. For that layer we use a maximum size of 512 kB, again, because
 * it reaches the maximum number of page pointers. Notice is the '-1' in the
 * calculation that comes from having a u16 for the number of pages i.e. 0xffff
 * is the maximum number of pages (in single layer).
 */
#define QEDR_CQE_SIZE	(sizeof(union rdma_cqe))
#define QEDR_MAX_CQE_PBL_SIZE (512 * 1024)
#define QEDR_MAX_CQE_PBL_ENTRIES (((QEDR_MAX_CQE_PBL_SIZE) / \
				  sizeof(u64)) - 1)
#define QEDR_MAX_CQES ((u32)((QEDR_MAX_CQE_PBL_ENTRIES) * \
			     (QED_CHAIN_PAGE_SIZE) / QEDR_CQE_SIZE))
/* INTERNAL: CNQ size Limitation
 * The maximum CNQ size is not reachable because the FW supports a chain of u16
 * (specifically 64k-1). The FW can buffer CNQ elements avoiding an overflow, on
 * the expense of performance. Hence we set it to an arbitrarily smaller value
 * than the maximum.
 */
#define QEDR_ROCE_MAX_CNQ_SIZE		(0x4000) /* INTERNAL: 2^16 */

#define QEDR_MAX_PORT			(1)
#define QEDR_PORT			(1)

#define QEDR_UVERBS(CMD_NAME) (1ull << IB_USER_VERBS_CMD_##CMD_NAME)

#define QEDR_ROCE_PKEY_MAX 1
#define QEDR_ROCE_PKEY_TABLE_LEN 1
#define QEDR_ROCE_PKEY_DEFAULT 0xffff

struct qedr_pbl {
	struct list_head list_entry;
	void *va;
	dma_addr_t pa;
};

struct qedr_ucontext {
	struct ib_ucontext ibucontext;
	struct qedr_dev *dev;
	struct qedr_pd *pd;
	u64 dpi_addr;
	u64 dpi_phys_addr;
	u32 dpi_size;
	u16 dpi;
	u8 edpm_mode;

	struct list_head mm_head;

	/* Lock to protect mm list */
	struct mutex mm_list_lock;

	struct qedr_recov_obj_info recov_info;
};

union db_prod32 {
	struct rdma_pwm_val16_data data;
	u32 raw;
};

union db_prod64 {
	struct rdma_pwm_val32_data data;
	u64 raw;
};

enum qedr_cq_type {
	QEDR_CQ_TYPE_GSI,
	QEDR_CQ_TYPE_KERNEL,
	QEDR_CQ_TYPE_USER,
};

struct qedr_pbl_info {
	u32 num_pbls;
	u32 num_pbes;
	u32 pbl_size;
	u32 pbe_size;
	bool two_layered;
};

struct qedr_userq {
	struct ib_umem *umem;
	struct qedr_pbl_info pbl_info;
	struct qedr_pbl *pbl_tbl;
	u64 buf_addr;
	size_t buf_len;
#ifndef QEDR_UPSTREAM_DB_REC /* ! QEDR_UPSTREAM_DB_REC */
	/* doorbell recovery */
	void __iomem *db_addr;
	struct ib_umem *db_rec_umem;
	u64 db_rec_addr;
	struct qedr_user_db_rec *db_rec_virt;
#endif
	void __iomem *db_rec_db2_addr;
	union db_prod32 db_rec_db2_data;
};

struct qedr_cq {
	struct ib_cq ibcq;			/* INTERNAL: must be first */

	enum qedr_cq_type cq_type;
	u32 sig;
	u16 icid;

	/* INTERNAL: relevant to cqs created from kernel space only (ULPs) */
	spinlock_t		cq_lock;
	u8			arm_flags;
	struct qed_chain	pbl;

	void __iomem		*db_addr;	/* INTERNAL: db address for cons update */
	union db_prod64		db;

	u8			pbl_toggle;
	union rdma_cqe		*latest_cqe;
	union rdma_cqe		*toggle_cqe;

	/* INTERNAL: relevant to cqs created from user space only (applications) */
	struct qedr_userq	q;

	/* INTERNAL: destroy-IRQ handler race prevention */
	u8			destroyed;
	u16			cnq_notif;

	/* Recovery related fields */
	struct qed_rdma_create_cq_in_params params;
	struct qedr_recov_obj_info recov_info;
	bool reset_notify_added;
	bool polled_for_reset_qp;
	bool reopened;
	bool arm_cq;
	enum ib_cq_notify_flags arm_cq_flags;
	struct list_head sq_qp_list;
	struct list_head rq_qp_list;
};

struct qedr_pd {
	struct ib_pd ibpd;
	u32 pd_id;
	struct qedr_ucontext *uctx;

	struct qedr_recov_obj_info recov_info;
};

struct qedr_xrcd {
#ifdef _HAS_XRC_SUPPORT /* QEDR_UPSTREAM */
	struct ib_xrcd ibxrcd;
#endif
	u16 xrcd_id;

	struct qedr_recov_obj_info recov_info;
};

struct qedr_mm {
	struct {
		u64 phy_addr;
		unsigned long len;
	} key;
	struct list_head entry;
};

struct qedr_qp_hwq_info {
	/* WQE Elements*/
	struct qed_chain	pbl;
	u64			p_phys_addr_tbl;
	u32			max_sges;

	/* WQE */
	u16			prod;     /* INTERNAL: WQE prod index for SW ring */
	u16			cons;     /* INTERNAL: WQE cons index for SW ring */
	u16			wqe_cons;
	u16			gsi_cons; /* INTERNAL: filled in by GSI implementation */
	u16			max_wr;

	/* DB */
	void __iomem		*db;      /* INTERNAL: Doorbell address */
	void			*dpm_db;
	union db_prod32		db_data;  /* INTERNAL: Doorbell data */

	/* Required for iwarp_only */
	void __iomem		*iwarp_db2;      /* Doorbell address */
	union db_prod32		iwarp_db2_data;  /* Doorbell data */
};

#define QEDR_INC_SW_IDX(p_info, index)					\
	do {								\
		p_info->index = (p_info->index + 1) &			\
				qed_chain_get_capacity(p_info->pbl)	\
	} while (0)

struct qedr_srq_hwq_info {
	u32 max_sges;
	u32 max_wr;
	struct qed_chain pbl;
	u64 p_phys_addr_tbl;
	u32 wqe_prod;         /* WQE prod index in HW ring */
	u32 sge_prod;         /* SGE prod index in HW ring */
	u32 wr_prod_cnt;      /* wr producer count */
	atomic_t wr_cons_cnt; /* wr consumer count */
	u32 num_elems;

	struct rdma_srq_producers *virt_prod_pair_addr; /* producer pair virtual address */
	dma_addr_t phy_prod_pair_addr; /* producer pair physical address */
};

struct qedr_srq {
	struct ib_srq ibsrq;
	struct qedr_dev *dev;

	/* Relevant to cqs created from user space only (applications) */
	struct qedr_userq	usrq;
	struct qedr_srq_hwq_info hw_srq;
	struct ib_umem *prod_umem;
	u16 srq_id;
	u32 srq_limit;
	bool is_xrc;
	/* lock to protect srq recv post */
	spinlock_t lock;

	struct qedr_recov_obj_info recov_info;
};

/*  Max LDPM payload = max size of single SQ WQE without inline data -
 *  rdma wqe + 4 SGEs (6 chain elements)
 */
#define QEDR_MAX_DPM_PAYLOAD	(QEDR_SQE_ELEMENT_SIZE * \
				 (RDMA_MAX_SGE_PER_SQ_WQE + 2))

struct qedr_dpm {
	u8 is_ldpm;

	union {
		struct db_rdma_dpm_data data;
		u64 raw;
	} msg;

	u8 payload[QEDR_MAX_DPM_PAYLOAD];
	u32 payload_size;
};

enum qedr_qp_err_bitmap {
	QEDR_QP_ERR_SQ_FULL	= 1 << 0,
	QEDR_QP_ERR_RQ_FULL	= 1 << 1,
	QEDR_QP_ERR_BAD_SR	= 1 << 2,
	QEDR_QP_ERR_BAD_RR	= 1 << 3,
	QEDR_QP_ERR_SQ_PBL_FULL	= 1 << 4,
	QEDR_QP_ERR_RQ_PBL_FULL	= 1 << 5,
};

enum qedr_qp_create_type {
	QEDR_QP_CREATE_NONE,
	QEDR_QP_CREATE_USER,
	QEDR_QP_CREATE_KERNEL,
};

enum qedr_iwarp_cm_flags {
	QEDR_IWARP_CM_WAIT_FOR_CONNECT    = BIT(0),
	QEDR_IWARP_CM_WAIT_FOR_DISCONNECT = BIT(1),
};

struct qedr_qp {
	struct ib_qp ibqp;	/* must be first */
	struct qedr_dev *dev;
	struct qedr_qp_hwq_info sq;
	struct qedr_qp_hwq_info rq;

	u32 max_inline_data;

	/* Lock for QP's */
	spinlock_t q_lock ____cacheline_aligned;
	struct qedr_cq *sq_cq;
	struct qedr_cq *rq_cq;
	struct qedr_srq *srq;
	enum qed_roce_qp_state state;
	u32 id;
	struct qedr_pd *pd;
	enum ib_qp_type qp_type;
	enum qedr_qp_create_type create_type;
	struct qed_rdma_qp *qed_qp;
	u32 qp_id;
	u16 icid;
	u16 mtu;
	int sgid_idx;
	u32 rq_psn;
	u32 sq_psn;
	u32 qkey;
	u32 dest_qp_num;
	u8 timeout;
	u32 sig;		/* unique siganture to identify valid QP */

	/* Relevant to qps created from kernel space only (ULPs) */
	u8 prev_wqe_size;
	u16 wqe_cons;
	u32 err_bitmap;
	bool signaled;

	/* SQ shadow */
	struct {
		u64 wr_id;
		enum ib_wc_opcode opcode;
		u32 bytes_len;
		u8 wqe_size;
		bool signaled;
		dma_addr_t icrc_mapping;
		u32 *icrc;
#ifdef DEFINE_IB_FAST_REG /* ! QEDR_UPSTREAM */
		struct qedr_fast_reg_page_list *frmr;
#endif
		struct qedr_mr *mr;
	} *wqe_wr_id;

	/* RQ shadow */
	struct {
		u64 wr_id;
		struct ib_sge sg_list[RDMA_MAX_SGE_PER_RQ_WQE];
		u8 wqe_size;

		/* INTERNAL: for GSI only */
		u8 smac[ETH_ALEN];
		u16 vlan;
		int rc;
	} *rqe_wr_id;

	/* Relevant to qps created from user space only (applications) */
	struct qedr_userq usq;
	struct qedr_userq urq;
	struct kref refcnt;
	struct completion iwarp_cm_comp;
#ifdef _HAS_QP_ALLOCATION
	struct completion qp_rel_comp;
#endif
	unsigned long iwarp_cm_flags; /* enum iwarp_cm_flags */

	/* Recovery related fields */
	struct qedr_recov_obj_info recov_info;
	struct list_head cq_sq_list_entry;
	struct list_head cq_rq_list_entry;
};

struct qedr_ah {
	struct ib_ah ibah;
	struct rdma_ah_attr attr;
};

enum qedr_mr_type {
	QEDR_MR_USER,
	QEDR_MR_KERNEL,
	QEDR_MR_DMA,
	QEDR_MR_FRMR,
};

struct mr_info {
	struct qedr_pbl *pbl_table;
	struct qedr_pbl_info pbl_info;
	struct list_head free_pbl_list;
	struct list_head inuse_pbl_list;
	u32 completed;
	u32 completed_handled;
};

struct qedr_mr {
	struct ib_mr 	ibmr;
	struct ib_umem 	*umem;

	struct qed_rdma_register_tid_in_params hw_mr;
	enum qedr_mr_type type;

	struct qedr_dev *dev;
	struct mr_info info;

	u64 *pages;
	u32 npages;

	/* GDR Related */
	bool gdr;
	atomic_t invalidated;
	struct completion invalidation_comp;
	struct completion mr_initialized;

	struct qedr_recov_obj_info recov_info;
};

#ifdef _HAS_MW_SUPPORT /* QEDR_UPSTREAM */
struct qedr_mw {
	struct ib_mw	ibmw;
	struct qed_rdma_register_tid_in_params hw_mw;
	struct qedr_dev *dev;

	struct qedr_recov_obj_info recov_info;
};
#endif

#define SET_FIELD2(value, name, flag) ((value) |= ((flag) << (name ## _SHIFT)))

#define QEDR_RESP_IMM	(RDMA_CQE_RESPONDER_IMM_FLG_MASK << \
			 RDMA_CQE_RESPONDER_IMM_FLG_SHIFT)
#define QEDR_RESP_RDMA	(RDMA_CQE_RESPONDER_RDMA_FLG_MASK << \
			 RDMA_CQE_RESPONDER_RDMA_FLG_SHIFT)
#define QEDR_RESP_INV	(RDMA_CQE_RESPONDER_INV_FLG_MASK << \
			 RDMA_CQE_RESPONDER_INV_FLG_SHIFT)

static inline void qedr_inc_sw_cons(struct qedr_qp_hwq_info *info)
{
	info->cons = (info->cons + 1) % info->max_wr;
	info->wqe_cons++;
}

static inline void qedr_inc_sw_prod(struct qedr_qp_hwq_info *info)
{
	info->prod = (info->prod + 1) % info->max_wr;
}

static inline int qedr_get_dmac(struct qedr_dev *dev,
				struct rdma_ah_attr *ah_attr, u8 *mac_addr)
{
	union ib_gid zero_sgid = { { 0 } };
	struct in6_addr in6;
#ifdef DEFINE_NO_IP_BASED_GIDS /* ! QEDR_UPSTREAM */
	u8 *guid;
#else
	u8 *dmac;
#endif

	if (!memcmp(&ah_attr->grh.dgid, &zero_sgid, sizeof(union ib_gid))) {
		DP_ERR(dev, "Local port GID not supported\n");
		memset(mac_addr, 0x00, ETH_ALEN);
		return -EINVAL;
	}

	memcpy(&in6, ah_attr->grh.dgid.raw, sizeof(in6));
#ifdef DEFINE_NO_IP_BASED_GIDS /* ! QEDR_UPSTREAM */
	guid = &ah_attr->grh.dgid.raw[8]; /* GID's 64 MSBs are the GUID */
	/* get the MAC address from the GUID i.e. EUI-64 to MAC address */
	mac_addr[0] = guid[0] ^ 2; /* toggle the local/universal bit to local */
	mac_addr[1] = guid[1];
	mac_addr[2] = guid[2];
	mac_addr[3] = guid[5];
	mac_addr[4] = guid[6];
	mac_addr[5] = guid[7];
#else
	dmac = rdma_ah_retrieve_dmac(ah_attr);
	if (!dmac) {
		DP_ERR(dev, "NOT able to retrieve dmac\n");
		memset(mac_addr, 0x00, ETH_ALEN);

		return -EINVAL;
	}
	memcpy(mac_addr, dmac, ETH_ALEN);
#endif

	return 0;
}

#ifdef DEFINE_IB_FAST_REG /* ! QEDR_UPSTREAM */
struct qedr_fast_reg_page_list {
	struct ib_fast_reg_page_list ibfrpl;
	struct qedr_dev *dev;
	struct mr_info info;
};
#endif

struct qedr_iw_listener {
	struct qedr_dev *dev;
	struct iw_cm_id *cm_id;
	int		backlog;
	void		*qed_handle;

	struct qedr_recov_obj_info recov_info;
};

struct qedr_iw_ep {
	struct qedr_dev	*dev;
	struct iw_cm_id	*cm_id;
	struct qedr_qp	*qp;
	void		*qed_context;
	struct kref	refcnt;

	struct qedr_recov_obj_info recov_info;
};

static inline
struct qedr_ucontext *get_qedr_ucontext(struct ib_ucontext *ibucontext)
{
	return container_of(ibucontext, struct qedr_ucontext, ibucontext);
}

static inline struct qedr_dev *get_qedr_dev(struct ib_device *ibdev)
{
	return container_of(ibdev, struct qedr_dev, ibdev);
}

static inline struct qedr_pd *get_qedr_pd(struct ib_pd *ibpd)
{
	return container_of(ibpd, struct qedr_pd, ibpd);
}

#ifdef _HAS_XRC_SUPPORT /* QEDR_UPSTREAM */
static inline struct qedr_xrcd *get_qedr_xrcd(struct ib_xrcd *ibxrcd)
{
	return container_of(ibxrcd, struct qedr_xrcd, ibxrcd);
}
#endif

static inline struct qedr_cq *get_qedr_cq(struct ib_cq *ibcq)
{
	return container_of(ibcq, struct qedr_cq, ibcq);
}

static inline struct qedr_qp *get_qedr_qp(struct ib_qp *ibqp)
{
	return container_of(ibqp, struct qedr_qp, ibqp);
}

static inline struct qedr_ah *get_qedr_ah(struct ib_ah *ibah)
{
	return container_of(ibah, struct qedr_ah, ibah);
}

static inline struct qedr_mr *get_qedr_mr(struct ib_mr *ibmr)
{
	return container_of(ibmr, struct qedr_mr, ibmr);
}

#ifdef _HAS_MW_SUPPORT /* QEDR_UPSTREAM */
static inline struct qedr_mw *get_qedr_mw(struct ib_mw *ibmw)
{
	return container_of(ibmw, struct qedr_mw, ibmw);
}
#endif

static inline bool qedr_qp_has_srq(struct qedr_qp *qp)
{
	return !!qp->srq;
}

static inline bool qedr_qp_has_sq(struct qedr_qp *qp)
{
	if (qp->qp_type == IB_QPT_GSI || qp->qp_type == IB_QPT_XRC_TGT)
		return 0;

	return 1;
}

static inline bool qedr_qp_has_rq(struct qedr_qp *qp)
{
	if (qp->qp_type == IB_QPT_GSI || qp->qp_type == IB_QPT_XRC_INI ||
	    qp->qp_type == IB_QPT_XRC_TGT || qedr_qp_has_srq(qp))
		return 0;

	return 1;
}

static inline int qedr_copy_to_udata(struct ib_udata *udata, void *src,
				     size_t len)
{
	return ib_copy_to_udata(udata, src, min_t(size_t, len, udata->outlen));
}

static inline int qedr_copy_from_udata(void *dest, struct ib_udata *udata,
				       size_t len)
{
	return ib_copy_from_udata(dest, udata, min_t(size_t, len,
				  udata->inlen));
}

static inline struct qedr_srq *get_qedr_srq(struct ib_srq *ibsrq)
{
	return container_of(ibsrq, struct qedr_srq, ibsrq);
}

#ifdef DEFINE_IB_FAST_REG /* ! QEDR_UPSTREAM */
static inline struct qedr_fast_reg_page_list *get_qedr_frmr_list(
	struct ib_fast_reg_page_list *ifrpl)
{
	return container_of(ifrpl, struct qedr_fast_reg_page_list, ibfrpl);
}
#endif

void qedr_reset_stats(struct qedr_dev *dev);

void consume_cqe(struct qedr_cq *cq);
#ifndef QEDR_UPSTREAM_DB_REC /* !QEDR_UPSTREAM_DB_REC */
int qedr_db_recovery_add(struct qedr_dev *dev,
			 void __iomem *db_addr,
			 void *db_data,
			 enum qed_db_rec_width db_width,
			 enum qed_db_rec_space db_space);

int qedr_db_recovery_del(struct qedr_dev *dev,
			void __iomem *db_addr, void *db_data,
			bool obj_reset);
#endif

int qedr_recov_check_state(struct qedr_dev *dev, const char *func_name);
void qedr_recov_obj_add(struct qedr_dev *dev, struct qedr_recov_obj_info *info);
void qedr_recov_obj_del(struct qedr_dev *dev, struct qedr_recov_obj_info *info);
#endif
