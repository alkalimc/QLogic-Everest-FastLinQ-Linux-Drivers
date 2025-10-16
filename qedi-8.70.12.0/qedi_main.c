/*
 *  QLogic iSCSI Offload Driver
 *  Copyright (c) 2015-2018 Cavium Inc.
 *
 *  See LICENSE.qedi for copyright and licensing details.
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/if_arp.h>
#include <scsi/iscsi_if.h>
#include <linux/inet.h>
#include <net/arp.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/if_vlan.h>
#include <linux/cpu.h>
#include <linux/iscsi_boot_sysfs.h>
#include <linux/aer.h>

#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi.h>

#include "qedi.h"
#include "qedi_gbl.h"
#include "qedi_iscsi.h"

static uint qedi_qed_debug;
module_param(qedi_qed_debug, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(qedi_qed_debug, " QED debug level 0 (default)");

static uint qedi_fw_debug;
module_param(qedi_fw_debug, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(qedi_fw_debug, " Firmware debug level 0(default) to 3");

uint qedi_dbg_log = QEDI_LOG_WARN | QEDI_LOG_INFO | QEDI_LOG_SCSI_TM;
module_param(qedi_dbg_log, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(qedi_dbg_log, " Default debug level");

uint qedi_io_tracing;
module_param(qedi_io_tracing, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(qedi_io_tracing,
		 " Enable logging of SCSI requests/completions into trace buffer. (default off).");

uint qedi_ll2_buf_size = 0x400;
module_param(qedi_ll2_buf_size, uint, S_IRUGO);
MODULE_PARM_DESC(qedi_ll2_buf_size, "parameter to set ping packet size,"
				   "Default - 0x400, Jumbo packets - 0x2400\n");

static uint qedi_flags_override;
module_param(qedi_flags_override, uint, S_IRUGO);
MODULE_PARM_DESC(qedi_flags_override, "Disable error flags bits");

const struct qed_iscsi_ops *qedi_ops;
static struct scsi_transport_template *qedi_scsi_transport;
static struct pci_driver qedi_pci_driver;
static DEFINE_PER_CPU(struct qedi_percpu_s, qedi_percpu);
static bool qedi_trans_register = false;

/* Static function declaration */
static int qedi_alloc_global_queues(struct qedi_ctx *qedi);
static void qedi_free_global_queues(struct qedi_ctx *qedi);
static void qedi_recovery_handler(struct work_struct *work);
static void qedi_board_disable_work(struct work_struct *work);
static void qedi_shutdown(struct pci_dev *pdev);
static int qedi_resume(struct pci_dev *pdev);
static int qedi_suspend(struct pci_dev *pdev, pm_message_t state);

static LIST_HEAD(qedi_udev_list);

static void qedi_reset_uio_rings(struct qedi_uio_dev *udev);
static void qedi_ll2_free_skbs(struct qedi_ctx *qedi);
static struct nvm_iscsi_block * qedi_get_nvram_block(struct qedi_ctx *qedi);
static pci_ers_result_t qedi_pci_error_detected(struct pci_dev *pdev,
						pci_channel_state_t state);
static pci_ers_result_t qedi_pci_slot_reset(struct pci_dev *pdev);
static void qedi_pci_resume(struct pci_dev *pdev);
static void qedi_mark_conn_recovery(struct iscsi_cls_session *cls_session);
static void qedi_set_conn_recovery(struct iscsi_cls_session *cls_session);
static int __qedi_probe(struct pci_dev *pdev, int mode);
static void __qedi_remove(struct pci_dev *pdev, int mode);

static int qedi_iscsi_event_cb(void *context, u8 fw_event_code, void *fw_handle)
{
	struct qedi_ctx *qedi;
	struct qedi_endpoint *qedi_ep;
	struct iscsi_eqe_data *data;
	int rval = 0;

	if (!context || !fw_handle) {
		QEDI_ERR(NULL, "Recv event with ctx NULL\n");
		return -EINVAL;
	}

	qedi = (struct qedi_ctx *)context;
	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
		  "Recv Event %d fw_handle %p\n", fw_event_code, fw_handle);

	data = (struct iscsi_eqe_data *)fw_handle;
	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
		  "icid=0x%x conn_id=0x%x err-code=0x%x error-pdu-opcode-reserved=0x%x\n",
		   data->icid, data->conn_id, data->error_code,
		   data->error_pdu_opcode_reserved);

	qedi_ep = qedi->ep_tbl[data->icid];

	if (!qedi_ep) {
		QEDI_WARN(&qedi->dbg_ctx,
			  "Cannot process event, ep already disconnected, cid=0x%x\n",
			   data->icid);
		WARN_ON(1);
		return -ENODEV;
	}

	switch (fw_event_code) {
	case ISCSI_EVENT_TYPE_ASYN_CONNECT_COMPLETE:
		if (qedi_ep->state == EP_STATE_OFLDCONN_START)
			qedi_ep->state = EP_STATE_OFLDCONN_COMPL;

		wake_up_interruptible(&qedi_ep->tcp_ofld_wait);
		break;
	case ISCSI_EVENT_TYPE_ASYN_TERMINATE_DONE:
		qedi_ep->state = EP_STATE_DISCONN_COMPL;
		wake_up_interruptible(&qedi_ep->tcp_ofld_wait);
		break;
	case ISCSI_EVENT_TYPE_ISCSI_CONN_ERROR:
		qedi_process_iscsi_error(qedi_ep, data);
		break;
	case ISCSI_EVENT_TYPE_ASYN_ABORT_RCVD:
	case ISCSI_EVENT_TYPE_ASYN_SYN_RCVD:
	case ISCSI_EVENT_TYPE_ASYN_MAX_RT_TIME:
	case ISCSI_EVENT_TYPE_ASYN_MAX_RT_CNT:
	case ISCSI_EVENT_TYPE_ASYN_MAX_KA_PROBES_CNT:
	case ISCSI_EVENT_TYPE_ASYN_FIN_WAIT2:
	case ISCSI_EVENT_TYPE_TCP_CONN_ERROR:
		qedi_process_tcp_error(qedi_ep, data);
		break;
	case ISCSI_EVENT_TYPE_ASYN_CLOSE_RCVD:
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
			  "Recv async close event\n");
		break;
	default:
		QEDI_ERR(&qedi->dbg_ctx, "Recv Unknown Event %u\n",
			 fw_event_code);
	}

	return rval;
}

static int qedi_uio_open(struct uio_info *uinfo, struct inode *inode)
{
	struct qedi_uio_dev *udev = uinfo->priv;
	struct qedi_ctx *qedi = udev->qedi;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (udev->uio_dev != -1)
		return -EBUSY;

	rtnl_lock();
	udev->uio_dev = iminor(inode);
	qedi_reset_uio_rings(udev);
	set_bit(UIO_DEV_OPENED, &qedi->flags);
	rtnl_unlock();

	return 0;
}

static int qedi_uio_close(struct uio_info *uinfo, struct inode *inode)
{
	struct qedi_uio_dev *udev = uinfo->priv;
	struct qedi_ctx *qedi = udev->qedi;

	udev->uio_dev = -1;
	clear_bit(UIO_DEV_OPENED, &qedi->flags);
	qedi_ll2_free_skbs(qedi);
	return 0;
}

static void __qedi_free_uio_rings(struct qedi_uio_dev *udev)
{
	if (udev->uctrl) {
		free_page((unsigned long)udev->uctrl);
		udev->uctrl = NULL;
	}

	if (udev->ll2_ring) {
		free_page((unsigned long)udev->ll2_ring);
		udev->ll2_ring = NULL;
	}

	if (udev->ll2_buf) {
		free_pages((unsigned long)udev->ll2_buf, get_order(udev->ll2_buf_size));
		udev->ll2_buf = NULL;
	}
}

static void __qedi_free_uio(struct qedi_uio_dev *udev)
{
	uio_unregister_device(&udev->qedi_uinfo);

	__qedi_free_uio_rings(udev);

	pci_dev_put(udev->pdev);
	kfree(udev);
}

static void qedi_free_uio(struct qedi_uio_dev *udev)
{
	if (!udev)
		return;

	list_del_init(&udev->list);
	__qedi_free_uio(udev);
}

static void qedi_reset_uio_rings(struct qedi_uio_dev *udev)
{
	struct qedi_ctx *qedi = NULL;
	struct qedi_uio_ctrl *uctrl = NULL;

	qedi = udev->qedi;
	uctrl = udev->uctrl;

	spin_lock_bh(&qedi->ll2_lock);
	uctrl->host_rx_cons = 0;
	uctrl->hw_rx_prod = 0;
	uctrl->hw_rx_bd_prod = 0;
	uctrl->host_rx_bd_cons = 0;

	memset(udev->ll2_ring, 0, udev->ll2_ring_size);
	memset(udev->ll2_buf, 0, udev->ll2_buf_size);
	spin_unlock_bh(&qedi->ll2_lock);
}

static int __qedi_alloc_uio_rings(struct qedi_uio_dev *udev, u16 pages)
{
	if (udev->ll2_ring || udev->ll2_buf)
		return 0;

	/* Memory for control area.  */
	udev->uctrl = (void *)get_zeroed_page(GFP_KERNEL);
	if (!udev->uctrl)
		return -ENOMEM;

	/* Allocating memory for LL2 ring  */
	udev->ll2_ring_size = pages * QEDI_PAGE_SIZE;
	udev->ll2_ring = (void *)get_zeroed_page(GFP_KERNEL | __GFP_COMP);
	if (!udev->ll2_ring)
		return -ENOMEM;

	/* Allocating memory for Tx/Rx pkt buffer */
	udev->ll2_buf_size = TX_RX_RING * qedi_ll2_buf_size;
	udev->ll2_buf_size = QEDI_PAGE_ALIGN(udev->ll2_buf_size);
	udev->ll2_buf = (void *)__get_free_pages(GFP_KERNEL | __GFP_COMP |
						 __GFP_ZERO, get_order(udev->ll2_buf_size));
	if (!udev->ll2_buf)
		return -ENOMEM;

	return 0;
}

static int qedi_alloc_uio_rings(struct qedi_ctx *qedi, u16 pages)
{
	struct qedi_uio_dev *udev = NULL;
	int rc = 0;

	list_for_each_entry(udev, &qedi_udev_list, list) {
		if (udev->pdev == qedi->pdev) {
			udev->qedi = qedi;
			if (__qedi_alloc_uio_rings(udev, pages)) {
				udev->qedi = NULL;
				return -ENOMEM;
			}
			qedi->udev = udev;
			return 0;
		}
	}

	udev = kzalloc(sizeof(*udev), GFP_KERNEL);
	if (!udev) {
		rc = -ENOMEM;
		goto err_udev;
	}

	udev->uio_dev = -1;

	udev->qedi = qedi;
	udev->pdev = qedi->pdev;

	rc = __qedi_alloc_uio_rings(udev, pages);
	if (rc)
		goto err_uctrl;

	list_add(&udev->list, &qedi_udev_list);

	pci_dev_get(udev->pdev);
	qedi->udev = udev;

	udev->tx_pkt = udev->ll2_buf;
	udev->rx_pkt = udev->ll2_buf + qedi_ll2_buf_size;
	return 0;

 err_uctrl:
	kfree(udev);
 err_udev:
	return -ENOMEM;
}

static int qedi_init_uio(struct qedi_ctx *qedi)
{
	struct qedi_uio_dev *udev = qedi->udev;
	struct uio_info *uinfo;
	int ret = 0;

	if (!udev)
		return -ENOMEM;

	uinfo = &udev->qedi_uinfo;

	uinfo->mem[0].addr = (unsigned long)udev->uctrl;
	uinfo->mem[0].size = sizeof(struct qedi_uio_ctrl);
	uinfo->mem[0].memtype = UIO_MEM_LOGICAL;

	uinfo->mem[1].addr = (unsigned long)udev->ll2_ring;
	uinfo->mem[1].size = udev->ll2_ring_size;
	uinfo->mem[1].memtype = UIO_MEM_LOGICAL;

	uinfo->mem[2].addr = (unsigned long)udev->ll2_buf;
	uinfo->mem[2].size = udev->ll2_buf_size;
	uinfo->mem[2].memtype = UIO_MEM_LOGICAL;

	uinfo->name = "qedi_uio";
	uinfo->version = QEDI_MODULE_VERSION;
	uinfo->irq = UIO_IRQ_CUSTOM;

	uinfo->open = qedi_uio_open;
	uinfo->release = qedi_uio_close;

	if (udev->uio_dev == -1) {
		if (!uinfo->priv) {
			uinfo->priv = udev;

			ret = uio_register_device(&udev->pdev->dev, uinfo);
			if (ret) {
				QEDI_ERR(&qedi->dbg_ctx,
					 "UIO registration failed\n");
			}
		}
	}

	return ret;
}

static int qedi_alloc_and_init_sb(struct qedi_ctx *qedi,
				  struct qed_sb_info *sb_info, u16 sb_id)
{
	struct status_block *sb_virt;
	dma_addr_t sb_phys;
	int ret;

	sb_virt = dma_alloc_coherent(&qedi->pdev->dev,
				     sizeof(struct status_block), &sb_phys,
				     GFP_KERNEL);
	if (!sb_virt) {
		QEDI_ERR(&qedi->dbg_ctx,
			 "Status block allocation failed for id = %d.\n",
			  sb_id);
		return -ENOMEM;
	}

	ret = qedi_ops->common->sb_init(qedi->cdev, sb_info, sb_virt, sb_phys,
				       sb_id, QED_SB_TYPE_STORAGE);
	if (ret) {
		QEDI_ERR(&qedi->dbg_ctx,
			 "Status block initialization failed for id = %d.\n",
			  sb_id);
		return ret;
	}

	return 0;
}

static void qedi_free_sb(struct qedi_ctx *qedi)
{
	struct qed_sb_info *sb_info;
	u16 id;

	for (id = 0; id < qedi->num_queues; id++) {
		sb_info = &qedi->sb_array[id];
		if (sb_info->sb_virt)
			dma_free_coherent(&qedi->pdev->dev,
					  sizeof(*sb_info->sb_virt),
					  (void *)sb_info->sb_virt,
					  sb_info->sb_phys);
	}
}

static void qedi_free_fp(struct qedi_ctx *qedi)
{
	kfree(qedi->fp_array);
	kfree(qedi->sb_array);
}

static void qedi_destroy_fp(struct qedi_ctx *qedi)
{
	qedi_free_sb(qedi);
	qedi_free_fp(qedi);
}

static int qedi_alloc_fp(struct qedi_ctx *qedi)
{
	int ret = 0;

	qedi->fp_array = kcalloc(qedi->num_queues,
				 sizeof(struct qedi_fastpath), GFP_KERNEL);
	if (!qedi->fp_array) {
		QEDI_ERR(&qedi->dbg_ctx,
			 "fastpath fp array allocation failed.\n");
		return -ENOMEM;
	}

	qedi->sb_array = kcalloc(qedi->num_queues,
				 sizeof(struct qed_sb_info), GFP_KERNEL);
	if (!qedi->sb_array) {
		QEDI_ERR(&qedi->dbg_ctx,
			 "fastpath sb array allocation failed.\n");
		ret = -ENOMEM;
		goto free_fp;
	}

	return ret;

free_fp:
	qedi_free_fp(qedi);
	return ret;
}

static void qedi_int_fp(struct qedi_ctx *qedi)
{
	struct qedi_fastpath *fp;
	u16 id;

	memset((void *)qedi->fp_array, 0, qedi->num_queues *
	       sizeof(*qedi->fp_array));
	memset((void *)qedi->sb_array, 0, qedi->num_queues *
	       sizeof(*qedi->sb_array));

	for (id = 0; id < qedi->num_queues; id++) {
		fp = &qedi->fp_array[id];
		fp->sb_info = &qedi->sb_array[id];
		fp->sb_id = id;
		fp->qedi = qedi;
		snprintf(fp->name, sizeof(fp->name), "%s-fp-%d",
			 "qedi", id);

		/* fp_array[i] ---- irq cookie
		 * So init data which is needed in int ctx
		 */
	}
}

static int qedi_prepare_fp(struct qedi_ctx *qedi)
{
	struct qedi_fastpath *fp;
	int ret = 0;
	u16 id;

	ret = qedi_alloc_fp(qedi);
	if (ret)
		goto err;

	qedi_int_fp(qedi);

	for (id = 0; id < qedi->num_queues; id++) {
		fp = &qedi->fp_array[id];
		ret = qedi_alloc_and_init_sb(qedi, fp->sb_info, fp->sb_id);
		if (ret) {
			QEDI_ERR(&qedi->dbg_ctx,
				 "SB allocation and initialization failed.\n");
			ret = -EIO;
			goto err_init;
		}
	}

	return 0;

err_init:
	qedi_free_sb(qedi);
	qedi_free_fp(qedi);
err:
	return ret;
}

static int qedi_setup_cid_que(struct qedi_ctx *qedi)
{
	int mem_size;
	u16 i;

	mem_size = qedi->max_active_conns * sizeof(u32);

	qedi->cid_que.cid_que_base = kmalloc(mem_size, GFP_KERNEL);
	if (!qedi->cid_que.cid_que_base)
		return -ENOMEM;

	mem_size = qedi->max_active_conns * sizeof(struct qedi_conn *);
	qedi->cid_que.conn_cid_tbl = kmalloc(mem_size, GFP_KERNEL);
	if (!qedi->cid_que.conn_cid_tbl) {
		kfree(qedi->cid_que.cid_que_base);
		qedi->cid_que.cid_que_base = NULL;
		return -ENOMEM;
	}

	qedi->cid_que.cid_que = (u32 *)qedi->cid_que.cid_que_base;
	qedi->cid_que.cid_q_prod_idx = 0;
	qedi->cid_que.cid_q_cons_idx = 0;
	qedi->cid_que.cid_q_max_idx = qedi->max_active_conns;
	qedi->cid_que.cid_free_cnt = qedi->max_active_conns;

	for (i = 0; i < qedi->max_active_conns; i++) {
		qedi->cid_que.cid_que[i] = i;
		qedi->cid_que.conn_cid_tbl[i] = NULL;
	}

	return 0;
}

static void qedi_release_cid_que(struct qedi_ctx *qedi)
{
	kfree(qedi->cid_que.cid_que_base);
	qedi->cid_que.cid_que_base = NULL;

	kfree(qedi->cid_que.conn_cid_tbl);
	qedi->cid_que.conn_cid_tbl = NULL;
}

static int qedi_init_id_tbl(struct qedi_portid_tbl *id_tbl, u16 size,
			    u16 start_id, u16 next)
{
	id_tbl->start = start_id;
	id_tbl->max = size;
	id_tbl->next = next;
	spin_lock_init(&id_tbl->lock);
	id_tbl->table = kzalloc(DIV_ROUND_UP(size, 32) * 4, GFP_KERNEL);
	if (!id_tbl->table)
		return -ENOMEM;

	return 0;
}

static void qedi_free_id_tbl(struct qedi_portid_tbl *id_tbl)
{
	kfree(id_tbl->table);
	id_tbl->table = NULL;
}

int qedi_alloc_id(struct qedi_portid_tbl *id_tbl, u16 id)
{
	int ret = -1;

	id -= id_tbl->start;
	if (id >= id_tbl->max)
		return ret;

	spin_lock(&id_tbl->lock);
	if (!test_bit(id, id_tbl->table)) {
		set_bit(id, id_tbl->table);
		ret = 0;
	}
	spin_unlock(&id_tbl->lock);
	return ret;
}

/* Returns -1 if not successful */
u16 qedi_alloc_new_id(struct qedi_portid_tbl *id_tbl)
{
	u16 id;

	spin_lock(&id_tbl->lock);
	id = find_next_zero_bit(id_tbl->table, id_tbl->max, id_tbl->next);
	if (id >= id_tbl->max) {
		id = QEDI_LOCAL_PORT_INVALID;
		if (id_tbl->next != 0) {
			id = find_first_zero_bit(id_tbl->table, id_tbl->next);
			if (id >= id_tbl->next)
				id = QEDI_LOCAL_PORT_INVALID;
		}
	}

	if (id < id_tbl->max) {
		set_bit(id, id_tbl->table);
		id_tbl->next = (id + 1) & (id_tbl->max - 1);
		id += id_tbl->start;
	}

	spin_unlock(&id_tbl->lock);

	return id;
}

void qedi_free_id(struct qedi_portid_tbl *id_tbl, u16 id)
{
	if (id == QEDI_LOCAL_PORT_INVALID)
		return;

	id -= id_tbl->start;
	if (id >= id_tbl->max)
		return;

	clear_bit(id, id_tbl->table);
}

static void qedi_cm_free_mem(struct qedi_ctx *qedi)
{
	kfree(qedi->ep_tbl);
	qedi->ep_tbl = NULL;
	qedi_free_id_tbl(&qedi->lcl_port_tbl);
}

static int qedi_cm_alloc_mem(struct qedi_ctx *qedi)
{
	u16 port_id;
	u32 mem_size;

	mem_size = qedi->max_active_conns * sizeof(struct qedi_endpoint *);
	qedi->ep_tbl = kzalloc(mem_size, GFP_KERNEL);
	if (!qedi->ep_tbl)
		return -ENOMEM;
#if defined PRANDOM_API || PRANDOM_U32
	port_id = prandom_u32() % QEDI_LOCAL_PORT_RANGE;
#else
	port_id = random32() % QEDI_LOCAL_PORT_RANGE;
#endif
	if (qedi_init_id_tbl(&qedi->lcl_port_tbl, QEDI_LOCAL_PORT_RANGE,
			     QEDI_LOCAL_PORT_MIN, port_id)) {
		qedi_cm_free_mem(qedi);
		return -ENOMEM;
	}

	return 0;
}

static struct qedi_ctx *qedi_host_alloc(struct pci_dev *pdev)
{
	struct Scsi_Host *shost;
	struct qedi_ctx *qedi = NULL;

	shost = iscsi_host_alloc(&qedi_host_template,
				 sizeof(struct qedi_ctx), 0);
	if (!shost) {
		QEDI_ERR(NULL, "Could not allocate shost\n");
		goto exit_setup_shost;
	}

	shost->max_id = QEDI_MAX_ISCSI_CONNS_PER_HBA - 1;
	shost->max_channel = 0;
	shost->max_lun = MAX_ISCSI_LUNS;
	shost->max_cmd_len = 16;
	shost->transportt = qedi_scsi_transport;

	qedi = iscsi_host_priv(shost);
	memset(qedi, 0, sizeof(*qedi));
	qedi->dbg_ctx.host_no = shost->host_no;
	qedi->pdev = pdev;
	qedi->dbg_ctx.pdev = pdev;
	qedi->max_active_conns = ISCSI_MAX_SESS_PER_HBA;
	qedi->max_sqes = QEDI_SQ_SIZE;
	qedi->grcdump = NULL;
	qedi->grcdump_size = 0;
	qedi->shost = shost;
	pci_set_drvdata(pdev, qedi);

exit_setup_shost:
	return qedi;
}

#ifdef MODERN_VLAN
static int qedi_ll2_rx(void *cookie, struct sk_buff *skb, u32 arg1, u32 arg2)
#else
static int qedi_ll2_rx(void *cookie, struct sk_buff *skb, u16 vlan, u32 arg1,
		       u32 arg2)
#endif
{
	struct qedi_ctx *qedi = (struct qedi_ctx *)cookie;
	struct qedi_uio_dev *udev;
	struct qedi_uio_ctrl *uctrl;
	struct skb_work_list *work;
	struct ethhdr *eh;

	if (!qedi) {
		QEDI_ERR(NULL, "qedi is NULL\n");
		return -1;
	}

	if (!test_bit(UIO_DEV_OPENED, &qedi->flags)) {
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_UIO,
			  "UIO DEV is not opened\n");
		kfree_skb(skb);
		return 0;
	}

	eh = (struct ethhdr *)skb->data;
	/* Undo VLAN encapsulation */
	if (eh->h_proto == htons(ETH_P_8021Q)) {
		memmove((u8 *)eh + VLAN_HLEN, eh, ETH_ALEN * 2);
		eh = (struct ethhdr *)skb_pull(skb, VLAN_HLEN);
		skb_reset_mac_header(skb);
	}

	/* Filter out non FIP/FCoE frames here to free them faster */
	if (eh->h_proto != htons(ETH_P_ARP) &&
	    eh->h_proto != htons(ETH_P_IP) &&
	    eh->h_proto != htons(ETH_P_IPV6)) {
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_LL2,
			  "Dropping frame ethertype [0x%x] len [0x%x].\n",
			  eh->h_proto, skb->len);
		kfree_skb(skb);
		return 0;
	}

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_LL2,
		"Allowed frame ethertype [0x%x] len [0x%x].\n",
		eh->h_proto, skb->len);

	udev = qedi->udev;
	uctrl = udev->uctrl;

	work = kzalloc(sizeof(*work), GFP_ATOMIC);
	if (!work) {
		QEDI_WARN(&qedi->dbg_ctx,
			  "Could not allocate work so dropping frame.\n");
		kfree_skb(skb);
		return 0;
	}

	INIT_LIST_HEAD(&work->list);
	work->skb = skb;

#ifdef MODERN_VLAN
	if (skb_vlan_tag_present(skb))
		work->vlan_id = skb_vlan_tag_get(skb);
#else
	work->vlan_id = vlan;
#endif
	if (work->vlan_id)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
		__vlan_insert_tag(work->skb, htons(ETH_P_8021Q), work->vlan_id);
#else
		__vlan_insert_tag(work->skb, work->vlan_id);
#endif

	spin_lock_bh(&qedi->ll2_lock);
	list_add_tail(&work->list, &qedi->ll2_skb_list);
	spin_unlock_bh(&qedi->ll2_lock);

	wake_up_process(qedi->ll2_recv_thread);

	return 0;
}

/* map this skb to iscsiuio mmaped region */
static int qedi_ll2_process_skb(struct qedi_ctx *qedi, struct sk_buff *skb,
				u16 vlan_id)
{
	struct qedi_uio_dev *udev = NULL;
	struct qedi_uio_ctrl *uctrl = NULL;
	struct qedi_rx_bd rxbd;
	struct qedi_rx_bd *p_rxbd;
	void *pkt;
	void *rx_ring;
	u32 prod, len=0;

	if (!qedi) {
		QEDI_ERR(NULL, "qedi is NULL\n");
		return -1;
	}

	udev = qedi->udev;
	uctrl = udev->uctrl;

	++uctrl->hw_rx_prod_cnt;
	prod = (uctrl->hw_rx_prod + 1) % RX_RING;

	pkt = udev->rx_pkt + (prod * qedi_ll2_buf_size);
	len = min_t(u32, skb->len, (u32)qedi_ll2_buf_size);
	memcpy(pkt, skb->data, len);

	memset(&rxbd, 0, sizeof(rxbd));
	rxbd.rx_pkt_index = prod;
	rxbd.rx_pkt_len = len;
	rxbd.vlan_id = vlan_id;

	uctrl->hw_rx_bd_prod = (uctrl->hw_rx_bd_prod + 1) % QEDI_NUM_RX_BD;
	p_rxbd = (struct qedi_rx_bd *)udev->ll2_ring;
	rx_ring = udev->ll2_ring + (uctrl->hw_rx_bd_prod * sizeof(rxbd));
	p_rxbd += uctrl->hw_rx_bd_prod;

	memcpy(p_rxbd, &rxbd, sizeof(rxbd));

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_LL2,
		  "hw_rx_prod [%d] prod [%d] hw_rx_bd_prod [%d] rx_pkt_idx [%d] rx_len [%d].\n",
		  uctrl->hw_rx_prod, prod, uctrl->hw_rx_bd_prod,
		  rxbd.rx_pkt_index, rxbd.rx_pkt_len);
	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_LL2,
		  "host_rx_cons [%d] hw_rx_bd_cons [%d].\n",
		  uctrl->host_rx_cons, uctrl->host_rx_bd_cons);

	uctrl->hw_rx_prod = prod;

	/* notify the iscsiuio about new packet */
	uio_event_notify(&udev->qedi_uinfo);

	return 0;
}

static void qedi_ll2_free_skbs(struct qedi_ctx *qedi)
{
	struct skb_work_list *work, *work_tmp;

	spin_lock_bh(&qedi->ll2_lock);
	list_for_each_entry_safe(work, work_tmp, &qedi->ll2_skb_list, list) {
		list_del(&work->list);
		if (work->skb)
			kfree_skb(work->skb);
		kfree(work);
	}
	spin_unlock_bh(&qedi->ll2_lock);
}

static int qedi_ll2_recv_thread(void *arg)
{
	struct qedi_ctx *qedi = (struct qedi_ctx *)arg;
	struct skb_work_list *work, *work_tmp;

	set_user_nice(current, -20);

	while (!kthread_should_stop()) {
		spin_lock_bh(&qedi->ll2_lock);
		list_for_each_entry_safe(work, work_tmp, &qedi->ll2_skb_list,
					 list) {
			list_del(&work->list);
			qedi_ll2_process_skb(qedi, work->skb, work->vlan_id);
			kfree_skb(work->skb);
			kfree(work);
		}
		set_current_state(TASK_INTERRUPTIBLE);
		spin_unlock_bh(&qedi->ll2_lock);
		schedule();
	}

	__set_current_state(TASK_RUNNING);
	return 0;
}

static int qedi_set_iscsi_pf_param(struct qedi_ctx *qedi)
{
	u8 num_sq_pages;
	u32 log_page_size;
	u16 todo_rqe_log_size;
	int rval = 0;

	num_sq_pages = (MAX_OUSTANDING_TASKS_PER_CON * 8) / QEDI_PAGE_SIZE;

	qedi->num_queues = MIN_NUM_CPUS_MSIX(qedi);
	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO, "Number of Queues: %d\n",
		  qedi->num_queues);

	todo_rqe_log_size = 12;

	memset(&qedi->pf_params.iscsi_pf_params, 0,
	       sizeof(qedi->pf_params.iscsi_pf_params));

	qedi->p_cpuq = pci_alloc_consistent(qedi->pdev,
			qedi->num_queues * sizeof(struct qedi_glbl_q_params),
			&qedi->hw_p_cpuq);
	if (!qedi->p_cpuq) {
		QEDI_ERR(&qedi->dbg_ctx, "pci_alloc_consistent fail\n");
		rval = -1;
		goto err_alloc_mem;
	}

	rval = qedi_alloc_global_queues(qedi);
	if (rval) {
		QEDI_ERR(&qedi->dbg_ctx, "Global queue allocation failed.\n");
		rval = -1;
		goto err_alloc_mem;
	}

	qedi->pf_params.iscsi_pf_params.num_cons = QEDI_MAX_ISCSI_CONNS_PER_HBA;
	qedi->pf_params.iscsi_pf_params.num_tasks = QEDI_MAX_ISCSI_TASK;
	qedi->pf_params.iscsi_pf_params.half_way_close_timeout = 10;
	qedi->pf_params.iscsi_pf_params.num_sq_pages_in_ring = num_sq_pages;
	qedi->pf_params.iscsi_pf_params.num_uhq_pages_in_ring = num_sq_pages;
	qedi->pf_params.iscsi_pf_params.num_queues = qedi->num_queues;
	qedi->pf_params.iscsi_pf_params.debug_mode = qedi_fw_debug;
	qedi->pf_params.iscsi_pf_params.two_msl_timer = 4000;
	qedi->pf_params.iscsi_pf_params.tx_sws_timer = 500;
	qedi->pf_params.iscsi_pf_params.max_fin_rt = 2;

	for (log_page_size = 0 ; log_page_size < 32 ; log_page_size++) {
		if ((1 << log_page_size) == QEDI_PAGE_SIZE)
			break;
	}
	qedi->pf_params.iscsi_pf_params.log_page_size = log_page_size;

	qedi->pf_params.iscsi_pf_params.glbl_q_params_addr = qedi->hw_p_cpuq;

	/* RQ BDQ initializations.
	 * rq_num_entries: suggested value for Initiator is 16 (4KB RQ)
	 * rqe_log_size: 8 for 256B RQE
	 */
	qedi->pf_params.iscsi_pf_params.rqe_log_size = 8;
	/* BDQ address and size */
	qedi->pf_params.iscsi_pf_params.bdq_pbl_base_addr[BDQ_ID_RQ] =
							qedi->bdq_pbl_list_dma;
	qedi->pf_params.iscsi_pf_params.bdq_pbl_num_entries[BDQ_ID_RQ] =
						qedi->bdq_pbl_list_num_entries;
	qedi->pf_params.iscsi_pf_params.rq_buffer_size = QEDI_BDQ_BUF_SIZE;

	/* cq_num_entries: num_tasks + rq_num_entries */
	qedi->pf_params.iscsi_pf_params.cq_num_entries = 2048;

	qedi->pf_params.iscsi_pf_params.gl_rq_pi = QEDI_PROTO_CQ_PROD_IDX;
	qedi->pf_params.iscsi_pf_params.gl_cmd_pi = 1;

err_alloc_mem:
	return rval;
}

/* Free DMA coherent memory for array of queue pointers we pass to qed */
static void qedi_free_iscsi_pf_param(struct qedi_ctx *qedi)
{
	size_t size = 0;

	if (qedi->p_cpuq) {
		size = qedi->num_queues * sizeof(struct qedi_glbl_q_params);
		pci_free_consistent(qedi->pdev, size, qedi->p_cpuq,
				    qedi->hw_p_cpuq);
	}

	qedi_free_global_queues(qedi);

	kfree(qedi->global_queues);
}

static void qedi_schedule_recovery_handler(void *dev)
{
	struct qedi_ctx *qedi = dev;

	if (test_and_set_bit(QEDI_IN_RECOVERY, &qedi->flags))
		return;

	atomic_set(&qedi->link_state, QEDI_LINK_DOWN);

	QEDI_ERR(&qedi->dbg_ctx, "Recovery handler scheduled.\n");
	schedule_delayed_work(&qedi->recovery_work, 0);
}

static void qedi_set_conn_recovery(struct iscsi_cls_session *cls_session)
{
	struct iscsi_session *session = cls_session->dd_data;
	struct iscsi_conn *conn = session->leadconn;
	struct qedi_conn *qedi_conn = conn->dd_data;

	qedi_start_conn_recovery(qedi_conn->qedi, qedi_conn);
}

static void qedi_link_update(void *dev, struct qed_link_output *link)
{
	struct qedi_ctx *qedi = (struct qedi_ctx *)dev;

	if (link->link_up) {
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO, "Link Up event.\n");
		atomic_set(&qedi->link_state, QEDI_LINK_UP);
	} else {
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
			  "Link Down event.\n");
		atomic_set(&qedi->link_state, QEDI_LINK_DOWN);
		iscsi_host_for_each_session(qedi->shost, qedi_set_conn_recovery);
	}
}

void qedi_recovery_process(struct work_struct *work)
{
	struct qedi_ctx *qedi =
	    container_of(work, struct qedi_ctx, recovery_process_work.work);

	QEDI_ERR(&(qedi->dbg_ctx), "Initiate MFW recovery process.\n");

	if (test_bit(QEDI_IN_RECOVERY, &qedi->flags) ||
	    test_bit(QEDI_IN_SHUTDOWN, &qedi->flags)) {
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
			  "Abort Recovery process.\n");
		return;
	}

	qedi_ops->common->dbg_save_all_data(qedi->cdev, true);
	qedi_ops->common->recovery_process(qedi->cdev);
}

void qedi_wq_grcdump(struct work_struct *work)
{
	struct qedi_ctx *qedi =
	    container_of(work, struct qedi_ctx, grcdump_work.work);

	QEDI_ERR(&(qedi->dbg_ctx), "Collecting GRC dump.\n");
	qedi_capture_grc_dump(qedi);
}

void qedi_schedule_hw_err_handler(void *dev,
				  enum qed_hw_err_type err_type)
{
	struct qedi_ctx *qedi = (struct qedi_ctx *)dev;
	unsigned long override_flags = qedi_flags_override;

	if (override_flags && test_bit(QEDI_ERR_OVERRIDE_EN, &override_flags))
		qedi->qedi_err_flags = qedi_flags_override;

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
		  "HW error handler scheduled, err=%d err_flags=0x%x\n",
		  err_type, qedi->qedi_err_flags);

	switch (err_type) {
	case QED_HW_ERR_FAN_FAIL:
		schedule_delayed_work(&qedi->board_disable_work, 0);
		break;
	case QED_HW_ERR_MFW_RESP_FAIL:
	case QED_HW_ERR_HW_ATTN:
	case QED_HW_ERR_DMAE_FAIL:
	case QED_HW_ERR_RAMROD_FAIL:
	case QED_HW_ERR_FW_ASSERT:
		/* Prevent HW attentions from being reasserted */
		if (test_bit(QEDI_ERR_ATTN_CLR_EN, &qedi->qedi_err_flags))
			qedi_ops->common->attn_clr_enable(qedi->cdev, true);

		if (((err_type == QED_HW_ERR_RAMROD_FAIL) ||
		     (err_type == QED_HW_ERR_DMAE_FAIL)) &&
		     test_bit(QEDI_ERR_IS_RECOVERABLE, &qedi->qedi_err_flags)) {
			schedule_delayed_work(&qedi->recovery_process_work, 0);
		}

		break;
	default:
		break;
	}
}

static void qedi_dcbx_aen(void			*dev,
			  struct qed_dcbx_get	*get,
			  u32			mib_type)
{
	struct qedi_ctx *qedi = (struct qedi_ctx *)dev;

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
		  "DCBx AEN handler scheduled\n");
}

static void qedi_get_boot_tgt_info(struct nvm_iscsi_block *block,
 				   struct qedi_boot_target *tgt, u8 index)
{
	u32 ipv6_en;

	ipv6_en = !!(block->generic.ctrl_flags &
	    		NVM_ISCSI_CFG_GEN_IPV6_ENABLED);

	snprintf(tgt->iscsi_name, sizeof(tgt->iscsi_name), "%s",
		block->target[index].target_name.byte);

	tgt->ipv6_en = ipv6_en;

	if (ipv6_en)
		snprintf(tgt->ip_addr, IPV6_LEN, "%pI6\n",
			 block->target[index].ipv6_addr.byte);
	else
		snprintf(tgt->ip_addr, IPV4_LEN, "%pI4\n",
			 block->target[index].ipv4_addr.byte);	

}

static int qedi_find_boot_info(struct qedi_ctx *qedi,
			       struct qed_mfw_tlv_iscsi *iscsi,
			       struct nvm_iscsi_block *block)
{
	struct iscsi_cls_session *cls_sess;
	struct iscsi_cls_conn *cls_conn;
	struct iscsi_conn *conn;
	struct iscsi_session *sess;
	struct qedi_conn *qedi_conn;
	struct qedi_boot_target *pri_tgt = NULL;
	struct qedi_boot_target *sec_tgt = NULL;
	char ep_ip_addr[64];
	u32 pri_ctrl_flags = 0;
	u32 sec_ctrl_flags = 0;
	u32 found = 0;
	int ret = 0;
	u16 i;

	pri_ctrl_flags = !!(block->target[0].ctrl_flags &
					NVM_ISCSI_CFG_TARGET_ENABLED);
	if (pri_ctrl_flags) {
		pri_tgt = kzalloc(sizeof(struct qedi_boot_target), GFP_KERNEL);
		if (!pri_tgt)
			return -1;
		qedi_get_boot_tgt_info(block, pri_tgt, 0);	
	}

	sec_ctrl_flags = !!(block->target[1].ctrl_flags &
					NVM_ISCSI_CFG_TARGET_ENABLED);
	if (sec_ctrl_flags) {
		sec_tgt = kzalloc(sizeof(struct qedi_boot_target), GFP_KERNEL);
		if (!sec_tgt) {
			ret = -1;
			goto free_tgt;
		}
		qedi_get_boot_tgt_info(block, sec_tgt, 1);	
	}

	for (i = 0; i < qedi->max_active_conns; i++) {
		qedi_conn = qedi_get_conn_from_id(qedi, i);
		if (!qedi_conn)
			continue;

		if (qedi_conn->ep->ip_type == TCP_IPV4)
			snprintf(ep_ip_addr, IPV4_LEN, "%pI4\n",
				 qedi_conn->ep->dst_addr);
		else
			snprintf(ep_ip_addr, IPV6_LEN, "%pI6\n",
				 qedi_conn->ep->dst_addr);

		cls_conn = qedi_conn->cls_conn;
		conn = cls_conn->dd_data;
		cls_sess = iscsi_conn_to_session(cls_conn);
		sess = cls_sess->dd_data;
		
		if (!iscsi_is_session_online(cls_sess))
			continue;

		if (!sess->targetname)
			continue;

		if (pri_ctrl_flags) {
			if (!strcmp(pri_tgt->iscsi_name, sess->targetname) &&
			    !strcmp(pri_tgt->ip_addr, ep_ip_addr)) {
				found = 1;
				break;
			}
		}
		
		if (sec_ctrl_flags) {
			if (!strncmp(sec_tgt->iscsi_name, sess->targetname,
				     sizeof(sec_tgt->iscsi_name)) &&
			    !strncmp(sec_tgt->ip_addr, ep_ip_addr,
				     sizeof(sec_tgt->ip_addr))) {
				found = 1;
				break;
			}
		}
	}

	if (found) {
		if (conn->hdrdgst_en) {
			iscsi->header_digest_set = true;
			iscsi->header_digest = 1;
		}

		if (conn->datadgst_en) {
			iscsi->data_digest_set = true;
			iscsi->data_digest = 1;
		}
		iscsi->boot_taget_portal_set = true;
		iscsi->boot_taget_portal = sess->tpgt;

	} else {
		ret = -1;
	}

	if (sec_ctrl_flags)
		kfree(sec_tgt);
free_tgt:
	if (pri_ctrl_flags)
		kfree(pri_tgt);

	return ret;
}

/*
 * Protocol TLV handler
 */
void qedi_get_protocol_tlv_data(void *dev, void *data)
{
	struct qedi_ctx *qedi = dev;
	struct qed_mfw_tlv_iscsi *iscsi = data;
	struct qed_iscsi_stats *fw_iscsi_stats;
	struct nvm_iscsi_block *block = NULL;
	u32 chap_en = 0;
	u32 mchap_en = 0;
	int rval = 0;

	fw_iscsi_stats = kmalloc(sizeof(struct qed_iscsi_stats), GFP_KERNEL);
	if (!fw_iscsi_stats) {
		QEDI_ERR(&(qedi->dbg_ctx), "Could not allocate memory for "
			"fw_iscsi_stats.\n");
		goto exit_get_data;
	}

	mutex_lock(&qedi->stats_lock);
	/* Query firmware for offload stats */
	qedi_ops->get_stats(qedi->cdev, fw_iscsi_stats);
	mutex_unlock(&qedi->stats_lock);

	iscsi->rx_frames_set = true;
	iscsi->rx_frames = fw_iscsi_stats->iscsi_rx_packet_cnt;
	iscsi->rx_bytes_set = true;
	iscsi->rx_bytes = fw_iscsi_stats->iscsi_rx_bytes_cnt;
	iscsi->tx_frames_set = true;
	iscsi->tx_frames = fw_iscsi_stats->iscsi_tx_packet_cnt;
	iscsi->tx_bytes_set = true;
	iscsi->tx_bytes = fw_iscsi_stats->iscsi_tx_bytes_cnt;
	iscsi->frame_size_set = true;
	iscsi->frame_size = qedi->ll2_mtu;
	block = qedi_get_nvram_block(qedi);
	if (block) {
		chap_en = !!(block->generic.ctrl_flags &
			     NVM_ISCSI_CFG_GEN_CHAP_ENABLED);
		mchap_en = !!(block->generic.ctrl_flags &
			      NVM_ISCSI_CFG_GEN_CHAP_MUTUAL_ENABLED);
		
		iscsi->auth_method_set = (chap_en || mchap_en) ? true : false;
		iscsi->auth_method = 1;
		if (chap_en)
			iscsi->auth_method = 2;
		if (mchap_en)
			iscsi->auth_method = 3;

		iscsi->tx_desc_size_set = true;
		iscsi->tx_desc_size = QEDI_SQ_SIZE;
		iscsi->rx_desc_size_set = true;
		iscsi->rx_desc_size = QEDI_CQ_SIZE;

		/* tpgt, hdr digest, data digest */
		rval = qedi_find_boot_info(qedi, iscsi, block);
		if (rval)
			QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
				  "Boot target not set");
	}

	kfree(fw_iscsi_stats);
exit_get_data:
	return;
}

/* Generic TLV data callback */
void qedi_get_generic_tlv_data (void *dev, struct qed_generic_tlvs *data)
{
	struct qedi_ctx *qedi;

	if (dev == NULL) {
		QEDI_INFO(NULL, QEDI_LOG_EVT,
		    "dev is NULL so ignoring get_generic_tlv_data request.\n");
		return;
	}
	qedi = (struct qedi_ctx *)dev;

	memset(data, 0, sizeof(struct qed_generic_tlvs));
	ether_addr_copy(data->mac[0], qedi->mac);
}

static struct qed_iscsi_cb_ops qedi_cb_ops = {
	{
		.link_update =		qedi_link_update,
		.schedule_recovery_handler =	qedi_schedule_recovery_handler,
		.schedule_hw_err_handler =	qedi_schedule_hw_err_handler,
		.dcbx_aen =			qedi_dcbx_aen,
		.get_protocol_tlv_data =	qedi_get_protocol_tlv_data,
		.get_generic_tlv_data =		qedi_get_generic_tlv_data,
	}
};

void qedi_iscsi_unmap_sg_list(struct qedi_cmd *cmd)
{
	struct scsi_cmnd *sc = cmd->scsi_cmd;

	if (cmd->io_tbl.sge_valid && sc) {
		scsi_dma_unmap(sc);
		cmd->io_tbl.sge_valid = 0;
	}
}

static void qedi_process_logout_resp(struct qedi_ctx *qedi,
				     union iscsi_cqe *cqe,
				     struct iscsi_task *task,
				     struct qedi_conn *qedi_conn)
{
	struct iscsi_conn *conn = qedi_conn->cls_conn->dd_data;
	struct iscsi_logout_rsp *resp_hdr;
	struct iscsi_session *session = conn->session;
	struct iscsi_logout_response_hdr *cqe_logout_response;
	struct qedi_cmd *cmd;

	cmd = (struct qedi_cmd *)task->dd_data;
	cqe_logout_response = &cqe->cqe_common.iscsi_hdr.logout_response;
#if defined SESS_LOCK
	spin_lock(&session->back_lock);
#else
	spin_lock(&session->lock);
#endif
	resp_hdr = (struct iscsi_logout_rsp *)&qedi_conn->gen_pdu.resp_hdr;
	memset(resp_hdr, 0, sizeof(struct iscsi_hdr));
	resp_hdr->opcode = cqe_logout_response->opcode;
	resp_hdr->flags = cqe_logout_response->flags;
	resp_hdr->hlength = 0;

	resp_hdr->itt = build_itt(cqe->cqe_solicited.itid, conn->session->age);
	resp_hdr->statsn = cpu_to_be32(cqe_logout_response->stat_sn);
	resp_hdr->exp_cmdsn = cpu_to_be32(cqe_logout_response->exp_cmd_sn);
	resp_hdr->max_cmdsn = cpu_to_be32(cqe_logout_response->max_cmd_sn);

	resp_hdr->t2wait = cpu_to_be32(cqe_logout_response->time_2_wait);
	resp_hdr->t2retain = cpu_to_be32(cqe_logout_response->time_2_retain);

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_TID,
		  "Freeing tid=0x%x for cid=0x%x\n",
		  cmd->task_id, qedi_conn->iscsi_conn_id);

	spin_lock(&qedi_conn->list_lock);
	if (likely(cmd->io_cmd_in_list)) {
		cmd->io_cmd_in_list = false;
		list_del_init(&cmd->io_cmd);
		qedi_conn->active_cmd_count--;
	} else {
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
			  "Active cmd list node already deleted, tid=0x%x, cid=0x%x, io_cmd_node=%p\n",
			  cmd->task_id, qedi_conn->iscsi_conn_id,
			  &cmd->io_cmd);
	}
	spin_unlock(&qedi_conn->list_lock);

	cmd->state = RESPONSE_RECEIVED;
	__iscsi_complete_pdu(conn, (struct iscsi_hdr *)resp_hdr, NULL, 0);

#if defined SESS_LOCK
	spin_unlock(&session->back_lock);
#else
	spin_unlock(&session->lock);
#endif
}

static void qedi_process_text_resp(struct qedi_ctx *qedi,
				   union iscsi_cqe *cqe,
				   struct iscsi_task *task,
				   struct qedi_conn *qedi_conn)
{
	struct iscsi_conn *conn = qedi_conn->cls_conn->dd_data;
	struct iscsi_session *session = conn->session;
	struct iscsi_task_context *task_ctx;
	struct iscsi_text_rsp *resp_hdr_ptr;
	struct iscsi_text_response_hdr *cqe_text_response;
	struct qedi_cmd *cmd;
	u32 pld_len;
	u32 *tmp;

	cmd = (struct qedi_cmd *)task->dd_data;
	task_ctx =
	       (struct iscsi_task_context *)qedi_get_task_mem(&qedi->tasks,
							      cmd->task_id);

	cqe_text_response = &cqe->cqe_common.iscsi_hdr.text_response;
#if defined SESS_LOCK
	spin_lock(&session->back_lock);
#else
	spin_lock(&session->lock);
#endif
	resp_hdr_ptr =  (struct iscsi_text_rsp *)&qedi_conn->gen_pdu.resp_hdr;
	memset(resp_hdr_ptr, 0, sizeof(struct iscsi_hdr));
	resp_hdr_ptr->opcode = cqe_text_response->opcode;
	resp_hdr_ptr->flags = cqe_text_response->flags;
	resp_hdr_ptr->hlength = 0;

	hton24(resp_hdr_ptr->dlength,
	       (cqe_text_response->hdr_second_dword &
		ISCSI_TEXT_RESPONSE_HDR_DATA_SEG_LEN_MASK));
	tmp = (u32 *)resp_hdr_ptr->dlength;

	resp_hdr_ptr->itt = build_itt(cqe->cqe_solicited.itid,
				      conn->session->age);
	resp_hdr_ptr->ttt = cqe_text_response->ttt;
	resp_hdr_ptr->statsn = cpu_to_be32(cqe_text_response->stat_sn);
	resp_hdr_ptr->exp_cmdsn = cpu_to_be32(cqe_text_response->exp_cmd_sn);
	resp_hdr_ptr->max_cmdsn = cpu_to_be32(cqe_text_response->max_cmd_sn);

	pld_len = cqe_text_response->hdr_second_dword &
		  ISCSI_TEXT_RESPONSE_HDR_DATA_SEG_LEN_MASK;
	qedi_conn->gen_pdu.resp_wr_ptr = qedi_conn->gen_pdu.resp_buf + pld_len;

	memset(task_ctx, '\0', sizeof(*task_ctx));

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_TID,
		  "Freeing tid=0x%x for cid=0x%x\n",
		  cmd->task_id, qedi_conn->iscsi_conn_id);

	spin_lock(&qedi_conn->list_lock);
	if (likely(cmd->io_cmd_in_list)) {
		cmd->io_cmd_in_list = false;
		list_del_init(&cmd->io_cmd);
		qedi_conn->active_cmd_count--;
	} else {
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
			  "Active cmd list node already deleted, tid=0x%x, cid=0x%x, io_cmd_node=%p\n",
			  cmd->task_id, qedi_conn->iscsi_conn_id,
			  &cmd->io_cmd);
	}
	spin_unlock(&qedi_conn->list_lock);

	cmd->state = RESPONSE_RECEIVED;

	__iscsi_complete_pdu(conn, (struct iscsi_hdr *)resp_hdr_ptr,
			     qedi_conn->gen_pdu.resp_buf,
			     (qedi_conn->gen_pdu.resp_wr_ptr -
			      qedi_conn->gen_pdu.resp_buf));
#if defined SESS_LOCK
	spin_unlock(&session->back_lock);
#else
	spin_unlock(&session->lock);
#endif
}

static void qedi_tmf_resp_work(struct work_struct *work)
{
	struct qedi_cmd *qedi_cmd =
				container_of(work, struct qedi_cmd, tmf_work);
	struct qedi_conn *qedi_conn = qedi_cmd->conn;
	struct qedi_ctx *qedi = qedi_conn->qedi;
	struct iscsi_conn *conn = qedi_conn->cls_conn->dd_data;
	struct iscsi_session *session = conn->session;
	struct iscsi_tm_rsp *resp_hdr_ptr;
	struct iscsi_cls_session *cls_sess;
	int rval = 0;

	resp_hdr_ptr =  (struct iscsi_tm_rsp *)qedi_cmd->tmf_resp_buf;
	cls_sess = iscsi_conn_to_session(qedi_conn->cls_conn);

	rval = qedi_cleanup_all_io(qedi, qedi_conn, qedi_cmd->task, true);
	if (rval)
		goto exit_tmf_resp;

#if defined SESS_LOCK
	spin_lock(&session->back_lock);
#else
	spin_lock(&session->lock);
#endif
	__iscsi_complete_pdu(conn, (struct iscsi_hdr *)resp_hdr_ptr, NULL, 0);
#if defined SESS_LOCK
	spin_unlock(&session->back_lock);
#else
	spin_unlock(&session->lock);
#endif

exit_tmf_resp:
	kfree(resp_hdr_ptr);

	spin_lock(&qedi_conn->tmf_work_lock);
	qedi_conn->fw_cleanup_works--;
	spin_unlock(&qedi_conn->tmf_work_lock);
}

static void qedi_process_tmf_resp(struct qedi_ctx *qedi,
				  union iscsi_cqe *cqe,
				  struct iscsi_task *task,
				  struct qedi_conn *qedi_conn)

{
	struct iscsi_conn *conn = qedi_conn->cls_conn->dd_data;
	struct iscsi_session *session = conn->session;
	struct iscsi_tmf_response_hdr *cqe_tmp_response;
	struct iscsi_tm_rsp *resp_hdr_ptr;
	struct iscsi_tm *tmf_hdr;
	struct qedi_cmd *qedi_cmd = NULL;
	u32 *tmp;

#ifdef ERROR_INJECT
	if (qedi->drop_tmf) {
		qedi->drop_tmf--;
		return;
	}
#endif

	cqe_tmp_response = &cqe->cqe_common.iscsi_hdr.tmf_response;

	qedi_cmd = task->dd_data;
	qedi_cmd->tmf_resp_buf = kzalloc(sizeof(*resp_hdr_ptr), GFP_KERNEL);
	if (!qedi_cmd->tmf_resp_buf) {
		QEDI_ERR(&qedi->dbg_ctx,
			 "Failed to allocate resp buf, cid=0x%x\n",
			  qedi_conn->iscsi_conn_id);
		return;
	}

#if defined SESS_LOCK
	spin_lock(&session->back_lock);
#else
	spin_lock(&session->lock);
#endif
	resp_hdr_ptr =  (struct iscsi_tm_rsp *)qedi_cmd->tmf_resp_buf;
	memset(resp_hdr_ptr, 0, sizeof(struct iscsi_tm_rsp));

	/* Fill up the header */
	resp_hdr_ptr->opcode = cqe_tmp_response->opcode;
	resp_hdr_ptr->flags = cqe_tmp_response->hdr_flags;
	resp_hdr_ptr->response = cqe_tmp_response->hdr_response;
	resp_hdr_ptr->hlength = 0;

	hton24(resp_hdr_ptr->dlength,
	       (cqe_tmp_response->hdr_second_dword &
		ISCSI_TMF_RESPONSE_HDR_DATA_SEG_LEN_MASK));
	tmp = (u32 *)resp_hdr_ptr->dlength;
	resp_hdr_ptr->itt = build_itt(cqe->cqe_solicited.itid,
				      conn->session->age);
	resp_hdr_ptr->statsn = cpu_to_be32(cqe_tmp_response->stat_sn);
	resp_hdr_ptr->exp_cmdsn  = cpu_to_be32(cqe_tmp_response->exp_cmd_sn);
	resp_hdr_ptr->max_cmdsn = cpu_to_be32(cqe_tmp_response->max_cmd_sn);

	tmf_hdr = (struct iscsi_tm *)qedi_cmd->task->hdr;

	spin_lock(&qedi_conn->list_lock);
	if (likely(qedi_cmd->io_cmd_in_list)) {
		qedi_cmd->io_cmd_in_list = false;
		list_del_init(&qedi_cmd->io_cmd);
		qedi_conn->active_cmd_count--;
	}
	spin_unlock(&qedi_conn->list_lock);

	spin_lock(&qedi_conn->tmf_work_lock);
	switch (tmf_hdr->flags & ISCSI_FLAG_TM_FUNC_MASK) {
	case ISCSI_TM_FUNC_LOGICAL_UNIT_RESET:
	case ISCSI_TM_FUNC_TARGET_WARM_RESET:
	case ISCSI_TM_FUNC_TARGET_COLD_RESET:
		if (qedi_conn->ep_disconnect_starting) {
			/* Session is down so ep_disconnect will clean up */
			spin_unlock(&qedi_conn->tmf_work_lock);
			goto unblock_sess;
		}

		qedi_conn->fw_cleanup_works++;
		spin_unlock(&qedi_conn->tmf_work_lock);

		INIT_WORK(&qedi_cmd->tmf_work, qedi_tmf_resp_work);
		queue_work(qedi->tmf_thread, &qedi_cmd->tmf_work);
		goto unblock_sess;
	}
	spin_unlock(&qedi_conn->tmf_work_lock);

	__iscsi_complete_pdu(conn, (struct iscsi_hdr *)resp_hdr_ptr, NULL, 0);
	kfree(resp_hdr_ptr);

unblock_sess:
#if defined SESS_LOCK
	spin_unlock(&session->back_lock);
#else
	spin_unlock(&session->lock);
#endif
}

static void qedi_process_login_resp(struct qedi_ctx *qedi,
				    union iscsi_cqe *cqe,
				    struct iscsi_task *task,
				    struct qedi_conn *qedi_conn)
{
	struct iscsi_conn *conn = qedi_conn->cls_conn->dd_data;
	struct iscsi_session *session = conn->session;
	struct iscsi_task_context *task_ctx;
	struct iscsi_login_rsp *resp_hdr_ptr;
	struct iscsi_login_response_hdr *cqe_login_response;
	struct qedi_cmd *cmd;
	u32 pld_len;
	u32 *tmp;

	cmd = (struct qedi_cmd *)task->dd_data;

	cqe_login_response = &cqe->cqe_common.iscsi_hdr.login_response;
	task_ctx =
	       (struct iscsi_task_context *)qedi_get_task_mem(&qedi->tasks,
							      cmd->task_id);
#if defined SESS_LOCK
	spin_lock(&session->back_lock);
#else
	spin_lock(&session->lock);
#endif
	resp_hdr_ptr =  (struct iscsi_login_rsp *)&qedi_conn->gen_pdu.resp_hdr;
	memset(resp_hdr_ptr, 0, sizeof(struct iscsi_login_rsp));
	resp_hdr_ptr->opcode = cqe_login_response->opcode;
	resp_hdr_ptr->flags = cqe_login_response->flags_attr;
	resp_hdr_ptr->hlength = 0;

	hton24(resp_hdr_ptr->dlength,
	       (cqe_login_response->hdr_second_dword &
		ISCSI_LOGIN_RESPONSE_HDR_DATA_SEG_LEN_MASK));
	tmp = (u32 *)resp_hdr_ptr->dlength;
	resp_hdr_ptr->itt = build_itt(cqe->cqe_solicited.itid,
				      conn->session->age);
	resp_hdr_ptr->tsih = cqe_login_response->tsih;
	resp_hdr_ptr->statsn = cpu_to_be32(cqe_login_response->stat_sn);
	resp_hdr_ptr->exp_cmdsn = cpu_to_be32(cqe_login_response->exp_cmd_sn);
	resp_hdr_ptr->max_cmdsn = cpu_to_be32(cqe_login_response->max_cmd_sn);
	resp_hdr_ptr->status_class = cqe_login_response->status_class;
	resp_hdr_ptr->status_detail = cqe_login_response->status_detail;
	pld_len = cqe_login_response->hdr_second_dword &
		  ISCSI_LOGIN_RESPONSE_HDR_DATA_SEG_LEN_MASK;
	qedi_conn->gen_pdu.resp_wr_ptr = qedi_conn->gen_pdu.resp_buf + pld_len;

	spin_lock(&qedi_conn->list_lock);
	if (likely(cmd->io_cmd_in_list)) {
		cmd->io_cmd_in_list = false;
		list_del_init(&cmd->io_cmd);
		qedi_conn->active_cmd_count--;
	}
	spin_unlock(&qedi_conn->list_lock);

	memset(task_ctx, '\0', sizeof(*task_ctx));

	__iscsi_complete_pdu(conn, (struct iscsi_hdr *)resp_hdr_ptr,
			     qedi_conn->gen_pdu.resp_buf,
			     (qedi_conn->gen_pdu.resp_wr_ptr -
			     qedi_conn->gen_pdu.resp_buf));

#if defined SESS_LOCK
	spin_unlock(&session->back_lock);
#else
	spin_unlock(&session->lock);
#endif
	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_TID,
		  "Freeing tid=0x%x for cid=0x%x\n",
		  cmd->task_id, qedi_conn->iscsi_conn_id);
	cmd->state = RESPONSE_RECEIVED;
}

/**
 * qedi_get_rq_bdq_buf - copy RQ BDQ buffer contents to driver buffer
 * @qedi_conn:	iscsi connection on which RQ event occurred
 * @ptr:	driver buffer to which BDQ buffer contents is to be copied
 * @len:	payload length of valid data inside BDQ buf
 *
 * Copies RQ BDQ buffer contents from shared (DMA'able) memory region to
 * driver buffer. RQ BDQ is used to DMA unsolicited iSCSI PDU's.
 */
static void qedi_get_rq_bdq_buf(struct qedi_ctx *qedi,
				struct iscsi_cqe_unsolicited *cqe,
				char *ptr, u32 len)
{
	u16 idx = 0;

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_CONN,
		  "pld_len [%d], bdq_prod_idx [%d], idx [%d]\n",
		  len, qedi->bdq_prod_idx,
		  (qedi->bdq_prod_idx % qedi->rq_num_entries));

	/* Obtain buffer address from rqe_opaque */
	idx = cqe->rqe_opaque;

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_CONN,
		  "rqe_opaque [0x%p], idx [%d]\n", cqe->rqe_opaque, idx);

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_CONN,
		  "unsol_cqe_type = %d\n", cqe->unsol_cqe_type);
	switch (cqe->unsol_cqe_type) {
	case ISCSI_CQE_UNSOLICITED_SINGLE:
	case ISCSI_CQE_UNSOLICITED_FIRST:
		if (len)
			memcpy(ptr, (void *)qedi->bdq[idx].buf_addr, len);
		break;
	case ISCSI_CQE_UNSOLICITED_MIDDLE:
	case ISCSI_CQE_UNSOLICITED_LAST:
		break;
	default:
		break;
	}
}

/**
 * qedi_put_rq_bdq_buf - Replenish RQ BDQ buffer and ring prim and sec doorbell
 * @qedi_conn:	iscsi connection on which event to post
 * @count:	number of RQ BDQ buffers being posted to chip
 *
 */
static void qedi_put_rq_bdq_buf(struct qedi_ctx *qedi,
				struct iscsi_cqe_unsolicited *cqe,
				u32 count)
{
	u16 tmp;
	u16 idx = 0;
	struct scsi_bd *pbl;

	/* Obtain buffer address from rqe_opaque */
	idx = cqe->rqe_opaque;

	pbl = (struct scsi_bd *)qedi->bdq_pbl;
	pbl += (qedi->bdq_prod_idx % qedi->rq_num_entries);
	pbl->address.hi = cpu_to_le32(U64_HI(qedi->bdq[idx].buf_dma));
	pbl->address.lo = cpu_to_le32(U64_LO(qedi->bdq[idx].buf_dma));
	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_CONN,
		  "pbl [0x%p] pbl->address hi [0x%llx] lo [0x%llx] idx [%d]\n",
		  pbl, pbl->address.hi, pbl->address.lo, idx);
	pbl->opaque.iscsi_opaque.reserved_zero[0] = 0;
	pbl->opaque.iscsi_opaque.reserved_zero[1] = 0;
	pbl->opaque.iscsi_opaque.reserved_zero[2] = 0;
	pbl->opaque.iscsi_opaque.opaque = cpu_to_le32(idx);

	/* Increment producer to let f/w know we've handled the frame */
	qedi->bdq_prod_idx += count;

	writew(qedi->bdq_prod_idx, qedi->bdq_primary_prod);
	tmp = readw(qedi->bdq_primary_prod);

	writew(qedi->bdq_prod_idx, qedi->bdq_secondary_prod);
	tmp = readw(qedi->bdq_secondary_prod);
}

static void qedi_unsol_pdu_adjust_bdq(struct qedi_ctx *qedi,
				      struct iscsi_cqe_unsolicited *cqe,
				      u32 pdu_len, u32 num_bdqs,
				      char *bdq_data)
{
	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_CONN,
		  "num_bdqs [%d]\n", num_bdqs);

	qedi_get_rq_bdq_buf(qedi, cqe, bdq_data, pdu_len);
	qedi_put_rq_bdq_buf(qedi, cqe, (num_bdqs + 1));
}

static int qedi_process_nopin_mesg(struct qedi_ctx *qedi,
				   union iscsi_cqe *cqe,
				   struct iscsi_task *task,
				   struct qedi_conn *qedi_conn, u16 que_idx)
{
	struct iscsi_conn *conn = qedi_conn->cls_conn->dd_data;
	struct iscsi_session *session = conn->session;
	struct iscsi_nop_in_hdr *cqe_nop_in;
	struct iscsi_nopin *hdr;
	struct qedi_cmd *cmd;
	u32 tgt_async_nop = 0;
	u32 scsi_lun[2];
	u32 pdu_len, num_bdqs;
	char bdq_data[QEDI_BDQ_BUF_SIZE];
	unsigned long flags;

#if defined SESS_LOCK
	spin_lock_bh(&session->back_lock);
#else
	spin_lock_bh(&session->lock);
#endif
	cqe_nop_in = &cqe->cqe_common.iscsi_hdr.nop_in;

	pdu_len = cqe_nop_in->hdr_second_dword &
		  ISCSI_NOP_IN_HDR_DATA_SEG_LEN_MASK;
	num_bdqs = pdu_len / QEDI_BDQ_BUF_SIZE;

	hdr = (struct iscsi_nopin *)&qedi_conn->gen_pdu.resp_hdr;
	memset(hdr, 0, sizeof(struct iscsi_hdr));
	hdr->opcode = cqe_nop_in->opcode;
	hdr->max_cmdsn = cpu_to_be32(cqe_nop_in->max_cmd_sn);
	hdr->exp_cmdsn = cpu_to_be32(cqe_nop_in->exp_cmd_sn);
	hdr->statsn = cpu_to_be32(cqe_nop_in->stat_sn);
	hdr->ttt = cpu_to_be32(cqe_nop_in->ttt);

	if (cqe->cqe_common.cqe_type == ISCSI_CQE_TYPE_UNSOLICITED) {
		spin_lock_irqsave(&qedi->hba_lock, flags);
		qedi_unsol_pdu_adjust_bdq(qedi, &cqe->cqe_unsolicited,
					  pdu_len, num_bdqs, bdq_data);
		hdr->itt = RESERVED_ITT;
		tgt_async_nop = 1;
		spin_unlock_irqrestore(&qedi->hba_lock, flags);
		goto done;
	}

	/* Response to one of our nop-outs */
	if (task) {
		cmd = task->dd_data;
		hdr->flags = ISCSI_FLAG_CMD_FINAL;
		hdr->itt = build_itt(cqe->cqe_solicited.itid,
				     conn->session->age);
		scsi_lun[0] = 0xffffffff;
		scsi_lun[1] = 0xffffffff;
		memcpy(&hdr->lun, scsi_lun, sizeof(struct scsi_lun));
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_TID,
			  "Freeing tid=0x%x for cid=0x%x\n",
			  cmd->task_id, qedi_conn->iscsi_conn_id);
		cmd->state = RESPONSE_RECEIVED;
		spin_lock(&qedi_conn->list_lock);
		if (likely(cmd->io_cmd_in_list)) {
			cmd->io_cmd_in_list = false;
			list_del_init(&cmd->io_cmd);
			qedi_conn->active_cmd_count--;
		}

		spin_unlock(&qedi_conn->list_lock);
	}

done:
	__iscsi_complete_pdu(conn, (struct iscsi_hdr *)hdr, bdq_data, pdu_len);

#if defined SESS_LOCK
	spin_unlock_bh(&session->back_lock);
#else
	spin_unlock_bh(&session->lock);
#endif
	return tgt_async_nop;
}

/**
 ** qedi_process_async_mesg - this function handles iscsi async message
 ** @session:            iscsi session pointer
 ** @bnx2i_conn:         iscsi connection pointer
 ** @cqe:                pointer to newly DMA'ed CQE entry for processing
 **
 ** process iSCSI ASYNC Message
 **/
static void qedi_process_async_mesg(struct qedi_ctx *qedi,
				    union iscsi_cqe *cqe,
				    struct iscsi_task *task,
				    struct qedi_conn *qedi_conn,
				    u16 que_idx)
{
	struct iscsi_conn *conn = qedi_conn->cls_conn->dd_data;
	struct iscsi_session *session = conn->session;
	struct iscsi_async_msg_hdr *cqe_async_msg;
	struct iscsi_async *resp_hdr;
	u32 scsi_lun[2];
	u32 pdu_len, num_bdqs;
	char bdq_data[QEDI_BDQ_BUF_SIZE];
	unsigned long flags;

#if defined SESS_LOCK
	spin_lock_bh(&session->back_lock);
#else
	spin_lock_bh(&session->lock);
#endif

	cqe_async_msg = &cqe->cqe_common.iscsi_hdr.async_msg;
	pdu_len = cqe_async_msg->hdr_second_dword &
		ISCSI_ASYNC_MSG_HDR_DATA_SEG_LEN_MASK;
	num_bdqs = pdu_len / QEDI_BDQ_BUF_SIZE;

	if (cqe->cqe_common.cqe_type == ISCSI_CQE_TYPE_UNSOLICITED) {
		spin_lock_irqsave(&qedi->hba_lock, flags);
		qedi_unsol_pdu_adjust_bdq(qedi, &cqe->cqe_unsolicited,
					  pdu_len, num_bdqs, bdq_data);
		spin_unlock_irqrestore(&qedi->hba_lock, flags);
	}

	resp_hdr = (struct iscsi_async *)&qedi_conn->gen_pdu.resp_hdr;
	memset(resp_hdr, 0, sizeof(struct iscsi_hdr));
	resp_hdr->opcode = cqe_async_msg->opcode;
	resp_hdr->flags = 0x80;

	scsi_lun[0] = cpu_to_be32(cqe_async_msg->lun.lo);
	scsi_lun[1] = cpu_to_be32(cqe_async_msg->lun.hi);
	memcpy(&resp_hdr->lun, scsi_lun, sizeof(struct scsi_lun));
	resp_hdr->exp_cmdsn = cpu_to_be32(cqe_async_msg->exp_cmd_sn);
	resp_hdr->max_cmdsn = cpu_to_be32(cqe_async_msg->max_cmd_sn);
	resp_hdr->statsn = cpu_to_be32(cqe_async_msg->stat_sn);

	resp_hdr->async_event = cqe_async_msg->async_event;
	resp_hdr->async_vcode = cqe_async_msg->async_vcode;

	resp_hdr->param1 = cpu_to_be16(cqe_async_msg->param1_rsrv);
	resp_hdr->param2 = cpu_to_be16(cqe_async_msg->param2_rsrv);
	resp_hdr->param3 = cpu_to_be16(cqe_async_msg->param3_rsrv);

	__iscsi_complete_pdu(conn, (struct iscsi_hdr *)resp_hdr, bdq_data,
			     pdu_len);

#if defined SESS_LOCK
	spin_unlock_bh(&session->back_lock);
#else
	spin_unlock_bh(&session->lock);
#endif
}

static void qedi_process_reject_mesg(struct qedi_ctx *qedi,
				     union iscsi_cqe *cqe,
				     struct iscsi_task *task,
				     struct qedi_conn *qedi_conn,
				     uint16_t que_idx)
{
	struct iscsi_conn *conn = qedi_conn->cls_conn->dd_data;
	struct iscsi_session *session = conn->session;
	struct iscsi_reject_hdr *cqe_reject;
	struct iscsi_reject *hdr;
	u32 pld_len, num_bdqs;
	unsigned long flags;

#if defined SESS_LOCK
	spin_lock_bh(&session->back_lock);
#else
	spin_lock_bh(&session->lock);
#endif
	cqe_reject = &cqe->cqe_common.iscsi_hdr.reject;
	pld_len = cqe_reject->hdr_second_dword &
		  ISCSI_REJECT_HDR_DATA_SEG_LEN_MASK;
	num_bdqs = pld_len / QEDI_BDQ_BUF_SIZE;

	if (cqe->cqe_common.cqe_type == ISCSI_CQE_TYPE_UNSOLICITED) {
		spin_lock_irqsave(&qedi->hba_lock, flags);
		qedi_unsol_pdu_adjust_bdq(qedi, &cqe->cqe_unsolicited,
					  pld_len, num_bdqs, conn->data);
		spin_unlock_irqrestore(&qedi->hba_lock, flags);
	}
	hdr = (struct iscsi_reject *)&qedi_conn->gen_pdu.resp_hdr;
	memset(hdr, 0, sizeof(struct iscsi_hdr));
	hdr->opcode = cqe_reject->opcode;
	hdr->reason = cqe_reject->hdr_reason;
	hdr->flags = cqe_reject->hdr_flags;
	hton24(hdr->dlength, (cqe_reject->hdr_second_dword &
			      ISCSI_REJECT_HDR_DATA_SEG_LEN_MASK));
	hdr->max_cmdsn = cpu_to_be32(cqe_reject->max_cmd_sn);
	hdr->exp_cmdsn = cpu_to_be32(cqe_reject->exp_cmd_sn);
	hdr->statsn = cpu_to_be32(cqe_reject->stat_sn);
	hdr->ffffffff = cpu_to_be32(0xffffffff);

	__iscsi_complete_pdu(conn, (struct iscsi_hdr *)hdr,
			     conn->data, pld_len);
#if defined SESS_LOCK
	spin_unlock_bh(&session->back_lock);
#else
	spin_unlock_bh(&session->lock);
#endif
}

bool qedi_list_del_entry_valid(struct qedi_ctx *qedi,
			       struct list_head *entry)
{
	struct list_head *prev, *next;

	prev = entry->prev;
	next = entry->next;

	if (prev->next != entry) {
		QEDI_ERR(&qedi->dbg_ctx,
			 "list_del corruption. prev->next should be %p, but was %p\n",
			 entry, prev->next);
		return false;
	} else if(next->prev != entry) {
		QEDI_ERR(&qedi->dbg_ctx,
			 "list_del corruption. next->prev should be %p, but was %p\n",
			 entry, next->prev);
		return false;
	}

	return true;
}

static void qedi_scsi_completion(struct qedi_ctx *qedi,
				 union iscsi_cqe *cqe,
				 struct iscsi_task *task,
				 struct iscsi_conn *conn)
{
	struct scsi_cmnd *sc_cmd;
	struct qedi_cmd *cmd = task->dd_data;
	struct iscsi_session *session = conn->session;
#if defined STRUCT_SCSI_RSP
	struct iscsi_scsi_rsp *hdr;
#else
	struct iscsi_cmd_rsp *hdr;
#endif
	struct iscsi_data_in_hdr *cqe_data_in;
	struct qedi_conn *qedi_conn;
	u32 iscsi_cid;
	u32 datalen = 0;
	bool mark_cmd_node_deleted = false;
	u8 cqe_err_bits = 0;

	iscsi_cid  = cqe->cqe_common.conn_id;
	qedi_conn = qedi->cid_que.conn_cid_tbl[iscsi_cid];

	cqe_data_in = &cqe->cqe_common.iscsi_hdr.data_in;
	cqe_err_bits =
		cqe->cqe_common.error_bitmap.error_bits.cqe_error_status_bits;

#if defined SESS_LOCK
	spin_lock_bh(&session->back_lock);
#else
	spin_lock_bh(&session->lock);
#endif
	/* get the scsi command */
	sc_cmd = cmd->scsi_cmd;

	if (!sc_cmd) {
		QEDI_WARN(&qedi->dbg_ctx, "sc_cmd is NULL!\n");
		goto error;
	}

	if (!sc_cmd->SCp.ptr) {
		QEDI_WARN(&qedi->dbg_ctx,
			  "SCp.ptr is NULL, returned in another context.\n");
		goto error;
	}

	if (!sc_cmd->request) {
		QEDI_WARN(&qedi->dbg_ctx,
			  "sc_cmd->request is NULL, sc_cmd=%p.\n",
			  sc_cmd);
		goto error;
	}

#if defined BLK_DEV_SPECIAL
	if (!sc_cmd->request->special) {
		QEDI_WARN(&qedi->dbg_ctx,
			  "request->special is NULL so request not valid, sc_cmd=%p.\n",
			  sc_cmd);
		goto error;
	}
#endif

	if (!sc_cmd->request->q) {
		QEDI_WARN(&qedi->dbg_ctx,
			  "request->q is NULL so request is not valid, sc_cmd=%p.\n",
			  sc_cmd);
		goto error;
	}

#if defined STRUCT_SCSI_RSP
	hdr = (struct iscsi_scsi_rsp *)task->hdr;
#else
	hdr = (struct iscsi_cmd_rsp *)task->hdr;
#endif
	hdr->opcode = cqe_data_in->opcode;
	hdr->max_cmdsn = cpu_to_be32(cqe_data_in->max_cmd_sn);
	hdr->exp_cmdsn = cpu_to_be32(cqe_data_in->exp_cmd_sn);
	hdr->itt = build_itt(cqe->cqe_solicited.itid, conn->session->age);
	hdr->response = cqe_data_in->reserved1;
	hdr->cmd_status = cqe_data_in->status_rsvd;
	hdr->flags = cqe_data_in->flags;
	hdr->residual_count = cpu_to_be32(cqe_data_in->residual_count);

	if (hdr->cmd_status == SAM_STAT_CHECK_CONDITION) {
		datalen = cqe_data_in->reserved2 &
			  ISCSI_COMMON_HDR_DATA_SEG_LEN_MASK;
		memcpy((char *)conn->data, (char *)cmd->sense_buffer, datalen);
	}

#ifdef ERROR_INJECT
	if (qedi->dbg_underrun) {
		SET_FIELD(cqe_err_bits, CQE_ERROR_BITMAP_UNDER_RUN_ERR, 1);
		qedi->dbg_underrun--;
	}
#endif
	/* If f/w reports data underrun err then set residual to IO transfer
	 * length, set Underrun flag and clear Overrun flag explicitly
	 */
	if (unlikely(cqe_err_bits &&
		     GET_FIELD(cqe_err_bits, CQE_ERROR_BITMAP_UNDER_RUN_ERR))) {
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
			  "Under flow itt=0x%x proto flags=0x%x tid=0x%x cid 0x%x fw resid 0x%x sc dlen 0x%x\n",
			  hdr->itt, cqe_data_in->flags, cmd->task_id,
			  qedi_conn->iscsi_conn_id, hdr->residual_count,
			  scsi_bufflen(sc_cmd));
		hdr->residual_count = cpu_to_be32(scsi_bufflen(sc_cmd));
		hdr->flags |= ISCSI_FLAG_CMD_UNDERFLOW;
		hdr->flags &= (~ISCSI_FLAG_CMD_OVERFLOW);
	}

	spin_lock(&qedi_conn->list_lock);
	if (likely(cmd->io_cmd_in_list)) {
		if (qedi_list_del_entry_valid(qedi, &cmd->io_cmd) == false) {
			QEDI_ERR(&qedi->dbg_ctx,
				 "WARNING !! Ignoring non-existant/stale IO tid=0x%x itt=0x%x driver itt [0x%x] cid=0x%x\n",
				 cmd->task_id, cqe->cqe_solicited.itid, hdr->itt, qedi_conn->iscsi_conn_id);
			spin_unlock(&qedi_conn->list_lock);
			goto error;
		}
		cmd->io_cmd_in_list = false;
		list_del_init(&cmd->io_cmd);
		qedi_conn->active_cmd_count--;
		mark_cmd_node_deleted = true;
	}
	spin_unlock(&qedi_conn->list_lock);

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_TID,
		  "Freeing tid=0x%x for cid=0x%x\n",
		  cmd->task_id, qedi_conn->iscsi_conn_id);
	cmd->state = RESPONSE_RECEIVED;
	if (qedi_io_tracing)
		qedi_trace_io(qedi, task, cmd->task_id, QEDI_IO_TRACE_RSP);

	qedi_iscsi_unmap_sg_list(cmd);

	__iscsi_complete_pdu(conn, (struct iscsi_hdr *)hdr,
			     conn->data, datalen);
error:
#if defined SESS_LOCK
	spin_unlock_bh(&session->back_lock);
#else
	spin_unlock_bh(&session->lock);
#endif
}

static void qedi_mtask_completion(struct qedi_ctx *qedi,
				  union iscsi_cqe *cqe,
				  struct iscsi_task *task,
				  struct qedi_conn *conn, uint16_t que_idx)
{
	struct iscsi_conn *iscsi_conn;
	u32 hdr_opcode;

	hdr_opcode = cqe->cqe_common.iscsi_hdr.common.hdr_first_byte;
	iscsi_conn = conn->cls_conn->dd_data;

	switch (hdr_opcode) {
	case ISCSI_OPCODE_SCSI_RESPONSE:
	case ISCSI_OPCODE_DATA_IN:
#ifdef ERROR_INJECT
		if (qedi->drop_cmd) {
			qedi->drop_cmd--;
			break;
		}
#endif
		qedi_scsi_completion(qedi, cqe, task, iscsi_conn);
		break;
	case ISCSI_OPCODE_LOGIN_RESPONSE:
#ifdef ERROR_INJECT
		if (qedi->drop_login) {
			qedi->drop_login--;
			break;
		}
#endif
		qedi_process_login_resp(qedi, cqe, task, conn);
		break;
	case ISCSI_OPCODE_TMF_RESPONSE:
		qedi_process_tmf_resp(qedi, cqe, task, conn);
		break;
	case ISCSI_OPCODE_TEXT_RESPONSE:
#ifdef ERROR_INJECT
		if (qedi->drop_text) {
			qedi->drop_text--;
			break;
		}
#endif
		qedi_process_text_resp(qedi, cqe, task, conn);
		break;
	case ISCSI_OPCODE_LOGOUT_RESPONSE:
#ifdef ERROR_INJECT
		if (qedi->drop_logout) {
			qedi->drop_logout--;
			break;
		}
#endif
		qedi_process_logout_resp(qedi, cqe, task, conn);
		break;
	case ISCSI_OPCODE_NOP_IN:
		qedi_process_nopin_mesg(qedi, cqe, task, conn, que_idx);
		break;
	default:
		QEDI_ERR(&qedi->dbg_ctx, "unknown opcode\n");
	}
}

static void qedi_process_nopin_local_cmpl(struct qedi_ctx *qedi,
					  struct iscsi_cqe_solicited *cqe,
					  struct iscsi_task *task,
					  struct qedi_conn *qedi_conn)
{
	struct iscsi_conn *conn = qedi_conn->cls_conn->dd_data;
	struct iscsi_session *session = conn->session;
	struct qedi_cmd *cmd = task->dd_data;

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_UNSOL,
		  "itid=0x%x, cmd task id=0x%x\n",
		  cqe->itid, cmd->task_id);

	cmd->state = RESPONSE_RECEIVED;

#if defined SESS_LOCK
	spin_lock_bh(&session->back_lock);
#else
	spin_lock_bh(&session->lock);
#endif
	__iscsi_put_task(task);

#if defined SESS_LOCK
	spin_unlock_bh(&session->back_lock);
#else
	spin_unlock_bh(&session->lock);
#endif
}

static void qedi_process_cmd_cleanup_resp(struct qedi_ctx *qedi,
					  struct iscsi_cqe_solicited *cqe,
					  struct iscsi_conn *conn)
{
	struct qedi_work_map *work, *work_tmp;
	u32 proto_itt = cqe->itid;
	int found = 0;
	struct qedi_cmd *qedi_cmd = NULL;
	u32 iscsi_cid;
	struct qedi_conn *qedi_conn;
	struct qedi_cmd *dbg_cmd;
	struct iscsi_task *mtask, *task;
	struct iscsi_tm *tmf_hdr = NULL;

	iscsi_cid = cqe->conn_id;
	qedi_conn = qedi->cid_que.conn_cid_tbl[iscsi_cid];
	if (!qedi_conn) {
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
			  "icid not found 0x%x\n", cqe->conn_id);
		return;
	}

	/* Based on this itt get the corresponding qedi_cmd */
	spin_lock_bh(&qedi_conn->tmf_work_lock);
	list_for_each_entry_safe(work, work_tmp, &qedi_conn->tmf_work_list,
				 list) {
		if (work->rtid == proto_itt) {
			/* We found the command */
			qedi_cmd = work->qedi_cmd;
			if (!qedi_cmd->list_tmf_work) {
				QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_SCSI_TM,
					  "TMF work not found, cqe->tid=0x%x, cid=0x%x\n",
					  proto_itt, qedi_conn->iscsi_conn_id);
				WARN_ON(1);
			}
			found = 1;
			mtask = qedi_cmd->task;
			task = work->ctask;
			tmf_hdr = (struct iscsi_tm *)mtask->hdr;

			list_del_init(&work->list);
			kfree(work);
			qedi_cmd->list_tmf_work = NULL;
		}
	}
	spin_unlock_bh(&qedi_conn->tmf_work_lock);

	if (!found)
		goto check_cleanup_reqs;

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_SCSI_TM,
		  "TMF work, cqe->tid=0x%x, tmf flags=0x%x, cid=0x%x\n",
		  proto_itt, tmf_hdr->flags, qedi_conn->iscsi_conn_id);

	#if defined SESS_LOCK
		spin_lock_bh(&conn->session->back_lock);
	#else
		spin_lock_bh(&conn->session->lock);
	#endif
	if (iscsi_task_is_completed(task)) {
		QEDI_NOTICE(&qedi->dbg_ctx,
			    "IO task completed, tmf rtt=0x%x, cid=0x%x\n",
			   get_itt(tmf_hdr->rtt), qedi_conn->iscsi_conn_id);
		goto unlock;
	}

	dbg_cmd = task->dd_data;

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_SCSI_TM,
		  "Abort tmf rtt=0x%x, i/o itt=0x%x, i/o tid=0x%x, cid=0x%x\n",
		  get_itt(tmf_hdr->rtt), get_itt(task->itt), dbg_cmd->task_id,
		  qedi_conn->iscsi_conn_id);

	spin_lock(&qedi_conn->list_lock);
	if (likely(dbg_cmd->io_cmd_in_list)) {
		dbg_cmd->io_cmd_in_list = false;
		list_del_init(&dbg_cmd->io_cmd);
		qedi_conn->active_cmd_count--;
	}
	spin_unlock(&qedi_conn->list_lock);
	qedi_cmd->state = CLEANUP_RECV;

unlock:
	#if defined SESS_LOCK
		spin_unlock_bh(&conn->session->back_lock);
	#else
		spin_unlock_bh(&conn->session->lock);
	#endif

	wake_up_interruptible(&qedi_conn->wait_queue);
	return;

check_cleanup_reqs:
	if (atomic_inc_return(&qedi_conn->cmd_cleanup_cmpl) ==
	    atomic_read(&qedi_conn->cmd_cleanup_req)) {
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_SCSI_TM,
			  "Freeing tid=0x%x for cid=0x%x\n",
			  cqe->itid, qedi_conn->iscsi_conn_id);

		wake_up(&qedi_conn->wait_queue);
	}
}

static void qedi_fp_process_cqes(struct qedi_ctx *qedi, union iscsi_cqe *cqe,
				 uint16_t que_idx)
{
	struct iscsi_task *task = NULL;
	struct iscsi_nopout *nopout_hdr;
	struct qedi_conn *q_conn;
	struct iscsi_conn *conn;
	struct iscsi_task_context *fw_task_ctx;
	u32 comp_type;
	u32 iscsi_cid;
	u32 hdr_opcode;
	u32 ptmp_itt = 0;
	itt_t proto_itt = 0;
	u8 cqe_err_bits = 0;

	comp_type = cqe->cqe_common.cqe_type;
	hdr_opcode = cqe->cqe_common.iscsi_hdr.common.hdr_first_byte;
	cqe_err_bits =
		cqe->cqe_common.error_bitmap.error_bits.cqe_error_status_bits;

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_CONN,
		  "fw_cid=0x%x, cqe type=0x%x, opcode=0x%x\n",
		  cqe->cqe_common.conn_id, comp_type, hdr_opcode);

	if (comp_type >= MAX_ISCSI_CQES_TYPE) {
		QEDI_WARN(&qedi->dbg_ctx, "Invalid CqE type\n");
		return;
	}

	iscsi_cid  = cqe->cqe_common.conn_id;
	q_conn = qedi->cid_que.conn_cid_tbl[iscsi_cid];
	if (!q_conn) {
		QEDI_WARN(&qedi->dbg_ctx,
			  "Session no longer exists for cid=0x%x!!\n",
			  iscsi_cid);
		return;
	}

	conn = q_conn->cls_conn->dd_data;

	if (unlikely(cqe_err_bits &&
		     GET_FIELD(cqe_err_bits,
			       CQE_ERROR_BITMAP_DATA_DIGEST_ERR))) {
		iscsi_conn_failure(conn, ISCSI_ERR_DATA_DGST);
		return;
	}

	switch (comp_type) {
	case ISCSI_CQE_TYPE_SOLICITED:
	case ISCSI_CQE_TYPE_SOLICITED_WITH_SENSE:
		fw_task_ctx =
		(struct iscsi_task_context *)qedi_get_task_mem(&qedi->tasks,
						      cqe->cqe_solicited.itid);
		if (GET_FIELD(fw_task_ctx->ystorm_st_context.state.flags,
		    YSTORM_ISCSI_TASK_STATE_LOCAL_COMP))  {
			qedi_get_proto_itt(qedi, cqe->cqe_solicited.itid,
					   &ptmp_itt);
			proto_itt = build_itt(ptmp_itt, conn->session->age);
		} else {
			cqe->cqe_solicited.itid =
					    qedi_get_itt(cqe->cqe_solicited);
			proto_itt = build_itt(cqe->cqe_solicited.itid,
					      conn->session->age);
		}

		#if defined SESS_LOCK
		spin_lock_bh(&conn->session->back_lock);
		#else
		spin_lock_bh(&conn->session->lock);
		#endif
		task = iscsi_itt_to_task(conn, proto_itt);
		#if defined SESS_LOCK
		spin_unlock_bh(&conn->session->back_lock);
		#else
		spin_unlock_bh(&conn->session->lock);
		#endif

		if (!task) {
			QEDI_WARN(&qedi->dbg_ctx, "task is NULL\n");
			return;
		}

		/* Process NOPIN local completion */
		nopout_hdr = (struct iscsi_nopout *)task->hdr;
		if ((nopout_hdr->itt == RESERVED_ITT) &&
		    (cqe->cqe_solicited.itid != (u16)RESERVED_ITT))
			qedi_process_nopin_local_cmpl(qedi, &cqe->cqe_solicited,
						      task, q_conn);
		else
			/* Process other solicited responses */
			qedi_mtask_completion(qedi, cqe, task, q_conn, que_idx);
		break;
	case ISCSI_CQE_TYPE_UNSOLICITED:
		switch (hdr_opcode) {
		case ISCSI_OPCODE_NOP_IN:
#ifdef ERROR_INJECT
		if (qedi->drop_nopin) {
			qedi->drop_nopin--;
			goto exit_fp_process;
		}
#endif
			qedi_process_nopin_mesg(qedi, cqe, task, q_conn,
						que_idx);
			break;
		case ISCSI_OPCODE_ASYNC_MSG:
			qedi_process_async_mesg(qedi, cqe, task, q_conn,
						que_idx);
			break;
		case ISCSI_OPCODE_REJECT:
			qedi_process_reject_mesg(qedi, cqe, task, q_conn,
						 que_idx);
			break;
		}
		goto exit_fp_process;
	case ISCSI_CQE_TYPE_DUMMY:
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_SCSI_TM, "Dummy CqE\n");
		goto exit_fp_process;
	case ISCSI_CQE_TYPE_TASK_CLEANUP:
#ifdef ERROR_INJECT
		if (qedi->drop_cleanup) {
			qedi->drop_cleanup--;
			goto exit_fp_process;
		}
#endif
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_SCSI_TM, "CleanUp CqE\n");
		qedi_process_cmd_cleanup_resp(qedi, &cqe->cqe_solicited, conn);
		goto exit_fp_process;
	default:
		QEDI_ERR(&qedi->dbg_ctx, "Error cqe comp_type=0x%x.\n",
		    comp_type);
		break;
	}

exit_fp_process:
	return;
}

static bool qedi_process_completions(struct qedi_fastpath *fp)
{
	struct qedi_work *qedi_work = NULL;
	struct qedi_ctx *qedi = fp->qedi;
	struct qed_sb_info *sb_info = fp->sb_info;
	struct status_block *sb = sb_info->sb_virt;
	struct qedi_percpu_s *p = NULL;
	struct global_queue *que;
	u16 prod_idx;
	unsigned long flags;
	union iscsi_cqe *cqe;
	u16 cpu;

	/* Get the current firmware producer index */
	prod_idx = sb->pi_array[QEDI_PROTO_CQ_PROD_IDX];

	if (prod_idx >= QEDI_CQ_SIZE)
		prod_idx = prod_idx % QEDI_CQ_SIZE;

	que = qedi->global_queues[fp->sb_id];
	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_IO,
		  "Before: global queue=%p prod_idx=%d cons_idx=%d, sb_id=%d\n",
		  que, prod_idx, que->cq_cons_idx, fp->sb_id);

	qedi->intr_cpu = fp->sb_id;
	cpu = smp_processor_id();
	p = &per_cpu(qedi_percpu, cpu);

	if (unlikely(!p->iothread))
		WARN_ON(1);

	spin_lock_irqsave(&p->p_work_lock, flags);
	while (que->cq_cons_idx != prod_idx) {
		cqe = &que->cq[que->cq_cons_idx];

		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_IO,
			  "cqe=%p prod_idx=%d cons_idx=%d.\n",
			  cqe, prod_idx, que->cq_cons_idx);

		/* Alloc and copy to the cqe */
		qedi_work = kzalloc(sizeof(*qedi_work), GFP_ATOMIC);
		if (qedi_work) {
			INIT_LIST_HEAD(&qedi_work->list);
			qedi_work->qedi = qedi;
			memcpy(&qedi_work->cqe, cqe, sizeof(union iscsi_cqe));
			qedi_work->que_idx = fp->sb_id;
			list_add_tail(&qedi_work->list, &p->work_list);
		} else {
			WARN_ON(1);
			continue;
		}

		que->cq_cons_idx++;
		if (que->cq_cons_idx == QEDI_CQ_SIZE)
			que->cq_cons_idx = 0;
	}
	wake_up_process(p->iothread);
	spin_unlock_irqrestore(&p->p_work_lock, flags);

	return true;
}

static bool qedi_fp_has_work(struct qedi_fastpath *fp)
{
	struct qedi_ctx *qedi = fp->qedi;
	struct global_queue *que;
	struct qed_sb_info *sb_info = fp->sb_info;
	struct status_block *sb = sb_info->sb_virt;
	u16 prod_idx;

	barrier();

	/* Get the current firmware producer index */
	prod_idx = sb->pi_array[QEDI_PROTO_CQ_PROD_IDX];

	/* Get the pointer to the global CQ this completion is on */
	que = qedi->global_queues[fp->sb_id];

	/* prod idx wrap around uint16 */
	if (prod_idx >= QEDI_CQ_SIZE)
		prod_idx = prod_idx % QEDI_CQ_SIZE;

	return (que->cq_cons_idx != prod_idx);
}

/* MSI-X fastpath handler code */
static irqreturn_t qedi_msix_handler(int irq, void *dev_id)
{
	struct qedi_fastpath *fp = dev_id;
	bool wake_io_thread = true;

	qed_sb_ack(fp->sb_info, IGU_INT_DISABLE, 0);

process_again:
	wake_io_thread = qedi_process_completions(fp);

	if (qedi_fp_has_work(fp) == 0)
		qed_sb_update_sb_idx(fp->sb_info);

	/* Check for more work */
	rmb();

	if (qedi_fp_has_work(fp) == 0)
		qed_sb_ack(fp->sb_info, IGU_INT_ENABLE, 1);
	else
		goto process_again;

	return IRQ_HANDLED;
}

/* simd handler for MSI/INTa */
static void qedi_simd_int_handler(void *cookie)
{
	/* Cookie is qedi_ctx struct */
	struct qedi_ctx *qedi = (struct qedi_ctx *)cookie;

	QEDI_WARN(&qedi->dbg_ctx, "qedi=%p.\n", qedi);
}

#define QEDI_SIMD_HANDLER_NUM		0
static void qedi_sync_free_irqs(struct qedi_ctx *qedi)
{
	u16 i, vector_idx;

	if (qedi->int_info.msix_cnt) {
		for (i = 0; i < qedi->int_info.used_cnt; i++) {
			vector_idx = i * qedi->dev_info.common.num_hwfns +
				qedi_ops->common->get_affin_hwfn_idx(qedi->cdev);
			QEDI_INFO(&(qedi->dbg_ctx), QEDI_LOG_DISC,
			    "Freeing IRQ #%d vector_idx=%d.\n", i, vector_idx);
			synchronize_irq(qedi->int_info.msix[vector_idx].vector);
			irq_set_affinity_hint(
			    qedi->int_info.msix[vector_idx].vector,
			    NULL);
			irq_set_affinity_notifier(
                            qedi->int_info.msix[vector_idx].vector,
                            NULL);
			free_irq(qedi->int_info.msix[vector_idx].vector,
				 &qedi->fp_array[i]);
		}
	} else {
		qedi_ops->common->simd_handler_clean(qedi->cdev,
						     QEDI_SIMD_HANDLER_NUM);
	}

	qedi->int_info.used_cnt = 0;
	qedi_ops->common->set_fp_int(qedi->cdev, 0);
}

static int qedi_request_msix_irq(struct qedi_ctx *qedi)
{
	int rc;
	u16 i, cpu, vector_idx;

	cpu = cpumask_first(cpu_online_mask);
	for (i = 0; i < qedi->msix_count; i++) {
		vector_idx = i * qedi->dev_info.common.num_hwfns +
		   qedi_ops->common->get_affin_hwfn_idx(qedi->cdev);
		QEDI_INFO(&(qedi->dbg_ctx), QEDI_LOG_DISC,
		    "Requesting IRQ #%d vector_idx=%d.\n", i, vector_idx);
		rc = request_irq(qedi->int_info.msix[vector_idx].vector,
				 qedi_msix_handler, 0, "qedi",
				 &qedi->fp_array[i]);

		if (rc) {
			QEDI_WARN(&qedi->dbg_ctx, "request_irq failed.\n");
			qedi_sync_free_irqs(qedi);
			return rc;
		}
		qedi->int_info.used_cnt++;
		rc = irq_set_affinity_hint(
		    qedi->int_info.msix[vector_idx].vector,
		    get_cpu_mask(cpu));
		cpu = cpumask_next(cpu, cpu_online_mask);
	}

	return 0;
}

static int qedi_setup_int(struct qedi_ctx *qedi)
{
	int rc = 0;

	rc = qedi_ops->common->set_fp_int(qedi->cdev, qedi->num_queues);
	if (rc > 0)
		qedi->msix_count = rc;
	else
		goto exit_setup_int;
		
	rc = qedi_ops->common->get_fp_int(qedi->cdev, &qedi->int_info);
	if (rc)
		goto exit_setup_int;

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
		  "Total msix_cnt [0x%x] Num of queues [0x%x] min msix cnt [0x%x]\n",
		   qedi->int_info.msix_cnt, qedi->num_queues,
		   qedi->msix_count);

	if (qedi->int_info.msix_cnt) {
		rc = qedi_request_msix_irq(qedi);
		goto exit_setup_int;
	} else {
		qedi_ops->common->simd_handler_config(qedi->cdev, &qedi,
						      QEDI_SIMD_HANDLER_NUM,
						      qedi_simd_int_handler);
		qedi->int_info.used_cnt = 1;
		QEDI_ERR(&qedi->dbg_ctx,
			"Cannot load driver due to a lack of MSI-X vectors.\n");
		rc = -EINVAL;
	}

exit_setup_int:
	return rc;
}

static void qedi_free_nvm_iscsi_cfg(struct qedi_ctx *qedi)
{
	if (qedi->iscsi_image)
		dma_free_coherent(&qedi->pdev->dev,
				  sizeof(struct qedi_nvm_iscsi_image),
				  qedi->iscsi_image, qedi->nvm_buf_dma);
}

static int qedi_alloc_nvm_iscsi_cfg(struct qedi_ctx *qedi)
{
	qedi->iscsi_image = dma_alloc_coherent(&qedi->pdev->dev,
		                               sizeof(struct qedi_nvm_iscsi_image),
					       &qedi->nvm_buf_dma, GFP_KERNEL);
	if (!qedi->iscsi_image) {
		QEDI_ERR(&qedi->dbg_ctx, "Could not allocate NVM BUF.\n");
		return -ENOMEM;
	}
	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
		  "NVM BUF addr=0x%p dma=0x%llx.\n", qedi->iscsi_image,
		  qedi->nvm_buf_dma);

	return 0;
}

static void qedi_free_bdq(struct qedi_ctx *qedi)
{
	u16 i;

	if (qedi->bdq_pbl_list)
		dma_free_coherent(&qedi->pdev->dev, QEDI_PAGE_SIZE,
				  qedi->bdq_pbl_list, qedi->bdq_pbl_list_dma);

	if (qedi->bdq_pbl)
		dma_free_coherent(&qedi->pdev->dev, qedi->bdq_pbl_mem_size,
				  qedi->bdq_pbl, qedi->bdq_pbl_dma);

	for (i = 0; i < QEDI_BDQ_NUM; i++) {
		if (qedi->bdq[i].buf_addr) {
			dma_free_coherent(&qedi->pdev->dev, QEDI_BDQ_BUF_SIZE,
					  qedi->bdq[i].buf_addr,
					  qedi->bdq[i].buf_dma);
		}
	}
}

static void qedi_free_global_queues(struct qedi_ctx *qedi)
{
	u16 i;
	struct global_queue **gl = qedi->global_queues;

	for (i = 0; i < qedi->num_queues; i++) {
		if (!gl[i])
			continue;

		if (gl[i]->cq)
			dma_free_coherent(&qedi->pdev->dev, gl[i]->cq_mem_size,
					  gl[i]->cq, gl[i]->cq_dma);
		if (gl[i]->cq_pbl)
			dma_free_coherent(&qedi->pdev->dev, gl[i]->cq_pbl_size,
					  gl[i]->cq_pbl, gl[i]->cq_pbl_dma);

		kfree(gl[i]);
	}
	qedi_free_bdq(qedi);
	qedi_free_nvm_iscsi_cfg(qedi);
}

static int qedi_alloc_bdq(struct qedi_ctx *qedi)
{
	struct scsi_bd *pbl;
	u64 *list;
	dma_addr_t page;
	u16 i;

	/* Alloc dma memory for BDQ buffers */
	for (i = 0; i < QEDI_BDQ_NUM; i++) {
		qedi->bdq[i].buf_addr =
				dma_alloc_coherent(&qedi->pdev->dev,
						   QEDI_BDQ_BUF_SIZE,
						   &qedi->bdq[i].buf_dma,
						   GFP_KERNEL);
		if (!qedi->bdq[i].buf_addr) {
			QEDI_ERR(&qedi->dbg_ctx,
				 "Could not allocate BDQ buffer %d.\n", i);
			return -ENOMEM;
		}
	}

	/* Alloc dma memory for BDQ page buffer list */
	qedi->bdq_pbl_mem_size = QEDI_BDQ_NUM * sizeof(struct scsi_bd);
	/* TBD: check page size */
	qedi->bdq_pbl_mem_size = ALIGN(qedi->bdq_pbl_mem_size, QEDI_PAGE_SIZE);
	qedi->rq_num_entries = qedi->bdq_pbl_mem_size / sizeof(struct scsi_bd);
	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_CONN, "rq_num_entries = %d.\n",
		  qedi->rq_num_entries);

	qedi->bdq_pbl = dma_alloc_coherent(&qedi->pdev->dev,
					   qedi->bdq_pbl_mem_size,
					   &qedi->bdq_pbl_dma, GFP_KERNEL);
	if (!qedi->bdq_pbl) {
		QEDI_ERR(&qedi->dbg_ctx, "Could not allocate BDQ PBL.\n");
		return -ENOMEM;
	}

	/*
	 * Populate BDQ PBL with physical and virtual address of individual
	 * BDQ buffers
	 */
	pbl = (struct scsi_bd  *)qedi->bdq_pbl;
	for (i = 0; i < QEDI_BDQ_NUM; i++) {
		pbl->address.hi = cpu_to_le32(U64_HI(qedi->bdq[i].buf_dma));
		pbl->address.lo = cpu_to_le32(U64_LO(qedi->bdq[i].buf_dma));
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_CONN,
			  "pbl [0x%p] pbl->address hi [0x%llx] lo [0x%llx], idx [%d]\n",
			  pbl, pbl->address.hi, pbl->address.lo, i);
		pbl->opaque.iscsi_opaque.reserved_zero[0] = 0;
		pbl->opaque.iscsi_opaque.reserved_zero[1] = 0;
		pbl->opaque.iscsi_opaque.reserved_zero[2] = 0;
		pbl->opaque.iscsi_opaque.opaque = cpu_to_le16(i);
		pbl++;
	}

	/* Allocate list of PBL pages */
	qedi->bdq_pbl_list = dma_alloc_coherent(&qedi->pdev->dev,
						QEDI_PAGE_SIZE,
						&qedi->bdq_pbl_list_dma,
						GFP_KERNEL);
	if (!qedi->bdq_pbl_list) {
		QEDI_ERR(&qedi->dbg_ctx,
			 "Could not allocate list of PBL pages.\n");
		return -ENOMEM;
	}
	memset(qedi->bdq_pbl_list, 0, QEDI_PAGE_SIZE);

	/*
	 * Now populate PBL list with pages that contain pointers to the
	 * individual buffers.
	 */
	qedi->bdq_pbl_list_num_entries = qedi->bdq_pbl_mem_size / QEDI_PAGE_SIZE;
	list = (u64 *)qedi->bdq_pbl_list;
	page = qedi->bdq_pbl_list_dma;
	for (i = 0; i < qedi->bdq_pbl_list_num_entries; i++) {
		*list = qedi->bdq_pbl_dma;
		list++;
		page += QEDI_PAGE_SIZE;
	}

	return 0;
}

static int qedi_alloc_global_queues(struct qedi_ctx *qedi)
{
	u32 *list;
	int status = 0, rc;
	u32 *pbl;
	dma_addr_t page;
	u16 i, num_pages;

	/*
	 * Number of global queues (CQ / RQ). This should
	 * be <= number of available MSIX vectors for the PF
	 */
	if (!qedi->num_queues) {
		QEDI_ERR(&qedi->dbg_ctx, "No MSI-X vectors available!\n");
		return 1;
	}

	/* Make sure we allocated the PBL that will contain the physical
	 * addresses of our queues
	 */
	if (!qedi->p_cpuq) {
		status = 1;
		goto mem_alloc_failure;
	}

	qedi->global_queues = kzalloc((sizeof(struct global_queue *) *
				       qedi->num_queues), GFP_KERNEL);
	if (!qedi->global_queues) {
		QEDI_ERR(&qedi->dbg_ctx,
			 "Unable to allocate global queues array ptr memory\n");
		return -ENOMEM;
	}
	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_DISC,
		  "qedi->global_queues=%p.\n", qedi->global_queues);

	/* Allocate DMA coherent buffers for BDQ */
	rc = qedi_alloc_bdq(qedi);
	if (rc)
		goto mem_alloc_failure;

	/* Allocate DMA coherent buffers for NVM_ISCSI_CFG */
	rc = qedi_alloc_nvm_iscsi_cfg(qedi);
	if (rc)
		goto mem_alloc_failure;

	/* Allocate a CQ and an associated PBL for each MSI-X
	 * vector.
	 */
	for (i = 0; i < qedi->num_queues; i++) {
		qedi->global_queues[i] = kzalloc(sizeof(struct global_queue),
						 GFP_KERNEL);
		if (!qedi->global_queues[i]) {
			QEDI_ERR(&qedi->dbg_ctx,
				 "Unable to allocation global queue %d.\n", i);
			goto mem_alloc_failure;
		}

		qedi->global_queues[i]->cq_mem_size =
		    (QEDI_CQ_SIZE + 8) * sizeof(union iscsi_cqe);
		qedi->global_queues[i]->cq_mem_size =
		    (qedi->global_queues[i]->cq_mem_size +
		    (QEDI_PAGE_SIZE - 1));

		qedi->global_queues[i]->cq_pbl_size =
		    (qedi->global_queues[i]->cq_mem_size /
		    QEDI_PAGE_SIZE) * sizeof(void *);
		qedi->global_queues[i]->cq_pbl_size =
		    (qedi->global_queues[i]->cq_pbl_size +
		    (QEDI_PAGE_SIZE - 1));

		qedi->global_queues[i]->cq =
		    dma_alloc_coherent(&qedi->pdev->dev,
				       qedi->global_queues[i]->cq_mem_size,
				       &qedi->global_queues[i]->cq_dma,
				       GFP_KERNEL);

		if (!qedi->global_queues[i]->cq) {
			QEDI_WARN(&qedi->dbg_ctx,
				  "Could not allocate cq.\n");
			status = -ENOMEM;
			goto mem_alloc_failure;
		}
		memset(qedi->global_queues[i]->cq, 0,
		       qedi->global_queues[i]->cq_mem_size);

		qedi->global_queues[i]->cq_pbl =
		    dma_alloc_coherent(&qedi->pdev->dev,
				       qedi->global_queues[i]->cq_pbl_size,
				       &qedi->global_queues[i]->cq_pbl_dma,
				       GFP_KERNEL);

		if (!qedi->global_queues[i]->cq_pbl) {
			QEDI_WARN(&qedi->dbg_ctx,
				  "Could not allocate cq PBL.\n");
			status = -ENOMEM;
			goto mem_alloc_failure;
		}
		memset(qedi->global_queues[i]->cq_pbl, 0,
		       qedi->global_queues[i]->cq_pbl_size);

		/* Create PBL */
		num_pages = qedi->global_queues[i]->cq_mem_size /
		    QEDI_PAGE_SIZE;
		page = qedi->global_queues[i]->cq_dma;
		pbl = (u32 *)qedi->global_queues[i]->cq_pbl;

		while (num_pages--) {
			*pbl = (u32)page;
			pbl++;
			*pbl = (u32)((u64)page >> 32);
			pbl++;
			page += QEDI_PAGE_SIZE;
		}
	}

	list = (u32 *)qedi->p_cpuq;

	/*
	 * The list is built as follows: CQ#0 PBL pointer, RQ#0 PBL pointer,
	 * CQ#1 PBL pointer, RQ#1 PBL pointer, etc.  Each PBL pointer points
	 * to the physical address which contains an array of pointers to the
	 * physical addresses of the specific queue pages.
	 */
	for (i = 0; i < qedi->num_queues; i++) {
		*list = (u32)qedi->global_queues[i]->cq_pbl_dma;
		list++;
		*list = (u32)((u64)qedi->global_queues[i]->cq_pbl_dma >> 32);
		list++;

		*list = (u32)0;
		list++;
		*list = (u32)((u64)0 >> 32);
		list++;
	}

	return 0;

mem_alloc_failure:
	qedi_free_global_queues(qedi);
	return status;
}

int qedi_alloc_sq(struct qedi_ctx *qedi, struct qedi_endpoint *ep)
{
	int rval = 0;
	u32 *pbl;
	dma_addr_t page;
	u16 num_pages;

	if (!ep)
		return -EIO;

	/* Calculate appropriate queue and PBL sizes */
	ep->sq_mem_size = QEDI_SQ_SIZE * sizeof(struct iscsi_wqe);
	ep->sq_mem_size += QEDI_PAGE_SIZE - 1;

	ep->sq_pbl_size = (ep->sq_mem_size / QEDI_PAGE_SIZE) * sizeof(void *);
	ep->sq_pbl_size = ep->sq_pbl_size + QEDI_PAGE_SIZE;

	ep->sq = dma_alloc_coherent(&qedi->pdev->dev, ep->sq_mem_size,
				    &ep->sq_dma, GFP_KERNEL);
	if (!ep->sq) {
		QEDI_WARN(&qedi->dbg_ctx,
			  "Could not allocate send queue.\n");
		rval = -ENOMEM;
		goto out;
	}
	memset(ep->sq, 0, ep->sq_mem_size);

	ep->sq_pbl = dma_alloc_coherent(&qedi->pdev->dev, ep->sq_pbl_size,
					&ep->sq_pbl_dma, GFP_KERNEL);
	if (!ep->sq_pbl) {
		QEDI_WARN(&qedi->dbg_ctx,
			  "Could not allocate send queue PBL.\n");
		rval = -ENOMEM;
		goto out_free_sq;
	}
	memset(ep->sq_pbl, 0, ep->sq_pbl_size);

	/* Create PBL */
	num_pages = ep->sq_mem_size / QEDI_PAGE_SIZE;
	page = ep->sq_dma;
	pbl = (u32 *)ep->sq_pbl;

	while (num_pages--) {
		*pbl = (u32)page;
		pbl++;
		*pbl = (u32)((u64)page >> 32);
		pbl++;
		page += QEDI_PAGE_SIZE;
	}

	return rval;

out_free_sq:
	dma_free_coherent(&qedi->pdev->dev, ep->sq_mem_size, ep->sq,
			  ep->sq_dma);
out:
	return rval;
}

void qedi_free_sq(struct qedi_ctx *qedi, struct qedi_endpoint *ep)
{
	if (ep->sq_pbl)
		dma_free_coherent(&qedi->pdev->dev, ep->sq_pbl_size, ep->sq_pbl,
				  ep->sq_pbl_dma);
	if (ep->sq)
		dma_free_coherent(&qedi->pdev->dev, ep->sq_mem_size, ep->sq,
				  ep->sq_dma);
}

int qedi_get_task_idx(struct qedi_ctx *qedi)
{
	s16 tmp_idx;

again:
	tmp_idx = find_first_zero_bit(qedi->task_idx_map,
				      MAX_ISCSI_TASK_ENTRIES);

	if (tmp_idx >= MAX_ISCSI_TASK_ENTRIES) {
		QEDI_ERR(&qedi->dbg_ctx, "FW task context pool is full.\n");
		tmp_idx = -1;
		goto err_idx;
	}

	if (test_and_set_bit(tmp_idx, qedi->task_idx_map))
		goto again;

err_idx:
	return tmp_idx;
}

void qedi_clear_task_idx(struct qedi_ctx *qedi, int idx)
{
	if (!test_and_clear_bit(idx, qedi->task_idx_map)) {
		QEDI_ERR(&qedi->dbg_ctx,
			 "FW task context, already cleared, tid=0x%x\n", idx);
	}
}

void qedi_update_itt_map(struct qedi_ctx *qedi, u32 tid, u32 proto_itt)
{
	qedi->itt_map[tid].itt = proto_itt;

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_CONN,
		  "update itt map tid=0x%x, with proto itt=0x%x\n", tid,
		  qedi->itt_map[tid].itt);
}

void qedi_get_task_tid(struct qedi_ctx *qedi, u32 itt, s16 *tid)
{
	u16 i;

	for (i = 0; i < MAX_ISCSI_TASK_ENTRIES; i++) {
		if (qedi->itt_map[i].itt == itt) {
			*tid = i;
			QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_CONN,
				  "Ref itt=0x%x, found at tid=0x%x\n",
				  itt, *tid);
			return;
		}
	}

	WARN_ON(1);
}

void qedi_get_proto_itt(struct qedi_ctx *qedi, u32 tid, u32 *proto_itt)
{
	*proto_itt = qedi->itt_map[tid].itt;
	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_CONN,
		  "Get itt map tid [0x%x with proto itt[0x%x]",
		  tid, *proto_itt);
}

static int qedi_alloc_itt(struct qedi_ctx *qedi)
{
	qedi->itt_map = kzalloc((sizeof(struct qedi_itt_map) *
				MAX_ISCSI_TASK_ENTRIES), GFP_KERNEL);
	if (!qedi->itt_map) {
		QEDI_ERR(&qedi->dbg_ctx,
			 "Unable to allocate itt map array memory\n");
		return -ENOMEM;
	}

	return 0;
}

static void qedi_free_itt(struct qedi_ctx *qedi)
{
	kfree(qedi->itt_map);
}

static struct qed_ll2_cb_ops qedi_ll2_cb_ops = {
	.rx_cb = qedi_ll2_rx,
	.tx_cb = NULL,
};

static int qedi_percpu_io_thread(void *arg)
{
	struct qedi_percpu_s *p = arg;
	struct qedi_work *work, *tmp;
	unsigned long flags;
	LIST_HEAD(work_list);

	set_user_nice(current, -20);

	while (!kthread_should_stop()) {
		spin_lock_irqsave(&p->p_work_lock, flags);
		while (!list_empty(&p->work_list)) {
			list_splice_init(&p->work_list, &work_list);
			spin_unlock_irqrestore(&p->p_work_lock, flags);

			list_for_each_entry_safe(work, tmp, &work_list, list) {
				list_del_init(&work->list);
				qedi_fp_process_cqes(work->qedi, &work->cqe,
						     work->que_idx);
				kfree(work);
			}
			cond_resched();
			spin_lock_irqsave(&p->p_work_lock, flags);
		}
		set_current_state(TASK_INTERRUPTIBLE);
		spin_unlock_irqrestore(&p->p_work_lock, flags);
		schedule();
	}
	__set_current_state(TASK_RUNNING);

	return 0;
}

#ifdef USE_CPU_HP
static int qedi_cpu_online(unsigned int cpu)
{
	struct qedi_percpu_s *p = this_cpu_ptr(&qedi_percpu);
	struct task_struct *thread;

	thread = kthread_create_on_node(qedi_percpu_io_thread, (void *)p,
					cpu_to_node(cpu),
					"qedi_thread/%d", cpu);
	if (IS_ERR(thread))
		return PTR_ERR(thread);

	kthread_bind(thread, cpu);
	p->iothread = thread;
	wake_up_process(thread);
	return 0;
}

static int qedi_cpu_offline(unsigned int cpu)
{
	struct qedi_percpu_s *p = this_cpu_ptr(&qedi_percpu);
	struct qedi_work *work, *tmp;
	struct task_struct *thread;

	spin_lock_bh(&p->p_work_lock);
	thread = p->iothread;
	p->iothread = NULL;

	list_for_each_entry_safe(work, tmp, &p->work_list, list) {
		list_del_init(&work->list);
		qedi_fp_process_cqes(work->qedi, &work->cqe, work->que_idx);
		kfree(work);
	}

	spin_unlock_bh(&p->p_work_lock);
	if (thread)
		kthread_stop(thread);

	return 0;
}
#else
static void qedi_percpu_thread_create(unsigned int cpu)
{
	struct qedi_percpu_s *p;
	struct task_struct *thread;

	p = &per_cpu(qedi_percpu, cpu);

	thread = kthread_create_on_node(qedi_percpu_io_thread, (void *)p,
					cpu_to_node(cpu),
					"qedi_thread/%d", cpu);
	if (likely(!IS_ERR(thread))) {
		kthread_bind(thread, cpu);
		p->iothread = thread;
		wake_up_process(thread);
	}
}

static void qedi_percpu_thread_destroy(unsigned int cpu)
{
	struct qedi_percpu_s *p;
	struct task_struct *thread;
	struct qedi_work *work, *tmp;

	p = &per_cpu(qedi_percpu, cpu);
	spin_lock_bh(&p->p_work_lock);
	thread = p->iothread;
	p->iothread = NULL;

	list_for_each_entry_safe(work, tmp, &p->work_list, list) {
		list_del_init(&work->list);
		qedi_fp_process_cqes(work->qedi, &work->cqe, work->que_idx);
		kfree(work);
	}

	spin_unlock_bh(&p->p_work_lock);
	if (thread)
		kthread_stop(thread);
}

static int qedi_cpu_callback(struct notifier_block *nfb,
			     unsigned long action, void *hcpu)
{
	unsigned cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		QEDI_ERR(NULL, "CPU %d online.\n", cpu);
		qedi_percpu_thread_create(cpu);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		QEDI_ERR(NULL, "CPU %d offline.\n", cpu);
		qedi_percpu_thread_destroy(cpu);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block qedi_cpu_notifier = {
	.notifier_call = qedi_cpu_callback,
};
#endif

void qedi_reset_host_mtu(struct qedi_ctx *qedi, u16 mtu)
{
	struct qed_ll2_params params;

	qedi_recover_all_conns(qedi);

	qedi_ops->ll2->stop(qedi->cdev);
	qedi_ll2_free_skbs(qedi);

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO, "old MTU %u, new MTU %u\n",
		  qedi->ll2_mtu, mtu);
	memset(&params, 0, sizeof(params));
	qedi->ll2_mtu = mtu;
	params.mtu = qedi->ll2_mtu + IPV6_HDR_LEN + TCP_HDR_LEN;
	params.drop_ttl0_packets = 0;
	params.rx_vlan_stripping = 1;
	ether_addr_copy(params.ll2_mac_address, qedi->dev_info.common.hw_mac);
	qedi_ops->ll2->start(qedi->cdev, &params);
}

int qedi_validate_mtu(struct qedi_ctx *qedi, struct iscsi_path *path_data)
{
	if (path_data->pmtu > JUMBO_MTU) {
		QEDI_ERR(NULL, "Invalid MTU %u\n", path_data->pmtu);
		return -EINVAL;
	}

	if (path_data->pmtu < DEF_PATH_MTU) {
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_DISC,
			  "MTU cannot be %u, using default MTU %u\n",
			  path_data->pmtu, DEF_PATH_MTU);
		path_data->pmtu = DEF_PATH_MTU;
	}

	if (path_data->pmtu != qedi->ll2_mtu)
		qedi_reset_host_mtu(qedi, path_data->pmtu);

	return 0;
}

static void qedi_clear_session_ctx(struct iscsi_cls_session *cls_sess)
{
	struct iscsi_session *session = cls_sess->dd_data;
	struct iscsi_conn *conn = session->leadconn;
	struct qedi_conn *qedi_conn = conn->dd_data;

	if (qedi_is_session_online(cls_sess))
		qedi_ep_disconnect(qedi_conn->iscsi_ep);

	qedi_conn_destroy(qedi_conn->cls_conn);

	qedi_session_destroy(cls_sess);
}

/**
 * qedi_get_nvram_block: - Scan through the iSCSI NVRAM block (while accounting
 * for gaps) for the matching absolute-pf-id of the QEDI device.
 */
static struct nvm_iscsi_block *
qedi_get_nvram_block(struct qedi_ctx *qedi)
{
	u8 pf;
	u32 flags;
	struct nvm_iscsi_block *block;
	u16 i;

	pf = qedi->dev_info.common.abs_pf_id;
	block = &qedi->iscsi_image->iscsi_cfg.block[0];
	for (i = 0; i < NUM_OF_ISCSI_PF_SUPPORTED; i++, block++) {
		flags = ((block->id) & NVM_ISCSI_CFG_BLK_CTRL_FLAG_MASK) >>
			NVM_ISCSI_CFG_BLK_CTRL_FLAG_OFFSET;
		if (flags & (NVM_ISCSI_CFG_BLK_CTRL_FLAG_IS_NOT_EMPTY |
				NVM_ISCSI_CFG_BLK_CTRL_FLAG_PF_MAPPED) &&
			(pf == (block->id & NVM_ISCSI_CFG_BLK_MAPPED_PF_ID_MASK)
				>> NVM_ISCSI_CFG_BLK_MAPPED_PF_ID_OFFSET))
			return block;
	}
	return NULL;
}

static ssize_t qedi_show_boot_eth_info(void *data, int type, char *buf)
{
	struct qedi_ctx *qedi = data;
	struct nvm_iscsi_initiator *initiator;
	int rc = 1;
	u32 ipv6_en, dhcp_en, vlan_en, ip_len;
	struct nvm_iscsi_block *block;
	char *fmt, *ip, *sub, *gw;

	block = qedi_get_nvram_block(qedi);
	if (!block)
		return 0;

	initiator = &block->initiator;
	ipv6_en = block->generic.ctrl_flags &
		  NVM_ISCSI_CFG_GEN_IPV6_ENABLED;
	dhcp_en = block->generic.ctrl_flags &
		  NVM_ISCSI_CFG_GEN_DHCP_TCPIP_CONFIG_ENABLED;
	vlan_en = initiator->ctrl_flags &
		  NVM_ISCSI_CFG_INITIATOR_VLAN_ENABLED;
	/* Static IP assignments. */
	fmt = ipv6_en ? "%pI6\n": "%pI4\n";
	ip_len = ipv6_en ? IPV6_LEN : IPV4_LEN;
	ip = ipv6_en ? initiator->ipv6.addr.byte: initiator->ipv4.addr.byte;
	sub = ipv6_en ? initiator->ipv6.subnet_mask.byte: initiator->ipv4.subnet_mask.byte;
	gw = ipv6_en ? initiator->ipv6.gateway.byte: initiator->ipv4.gateway.byte;
	/* DHCP IP adjustments. */
	fmt = dhcp_en ? "%s\n": fmt;
	if (dhcp_en) {
		ip = sub = gw = ipv6_en ? "0::0": "0.0.0.0";
		ip_len = ipv6_en ? 6 : 9;
	}

	switch (type) {
	case ISCSI_BOOT_ETH_IP_ADDR:
		rc = snprintf(buf, ip_len, fmt, ip);
		break;
	case ISCSI_BOOT_ETH_SUBNET_MASK:
		rc = snprintf(buf, ip_len, fmt, sub);
		break;
	case ISCSI_BOOT_ETH_GATEWAY:
		rc = snprintf(buf, ip_len, fmt, gw);
		break;
	case ISCSI_BOOT_ETH_FLAGS:
		rc = snprintf(buf, ISCSI_BOOT_STR_LEN, "%hhd\n",
			      SYSFS_FLAG_FW_SEL_BOOT);
		break;
	case ISCSI_BOOT_ETH_INDEX:
		rc = snprintf(buf, ISCSI_BOOT_STR_LEN, "0\n");
		break;
	case ISCSI_BOOT_ETH_MAC:
		rc = sysfs_format_mac(buf, qedi->mac, ETH_ALEN);
		break;
	case ISCSI_BOOT_ETH_VLAN:
		rc = snprintf(buf, 12, "%d\n", vlan_en ?
	    	GET_FIELD2(initiator->generic_cont0,
		       NVM_ISCSI_CFG_INITIATOR_VLAN) :
		0);
		break;
	case ISCSI_BOOT_ETH_ORIGIN:
		if (dhcp_en)
			rc = snprintf(buf, ISCSI_BOOT_STR_LEN, "3\n");
		break;
	default:
		rc = 0;
		break;
	}

	return rc;
}

#if defined UMODE_T_USED
static umode_t qedi_eth_get_attr_visibility(void *data, int type)
#else
static mode_t qedi_eth_get_attr_visibility(void *data, int type)
#endif
{
	int rc = 1;

	switch (type) {
	case ISCSI_BOOT_ETH_FLAGS:
	case ISCSI_BOOT_ETH_MAC:
	case ISCSI_BOOT_ETH_INDEX:
	case ISCSI_BOOT_ETH_IP_ADDR:
	case ISCSI_BOOT_ETH_SUBNET_MASK:
	case ISCSI_BOOT_ETH_GATEWAY:
	case ISCSI_BOOT_ETH_ORIGIN:
	case ISCSI_BOOT_ETH_VLAN:
		rc = S_IRUGO;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static ssize_t qedi_show_boot_ini_info(void *data, int type, char *buf)
{
	struct qedi_ctx *qedi = data;
	struct nvm_iscsi_initiator *initiator;
	int rc;
	struct nvm_iscsi_block *block;

	block = qedi_get_nvram_block(qedi);
	if (!block)
		return 0;

	initiator = &block->initiator;

	switch (type) {
	case ISCSI_BOOT_INI_INITIATOR_NAME:
		rc = sprintf(buf, "%.*s\n", NVM_ISCSI_CFG_ISCSI_NAME_MAX_LEN,
			     initiator->initiator_name.byte);
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

#if defined UMODE_T_USED
static umode_t qedi_ini_get_attr_visibility(void *data, int type)
#else
static mode_t qedi_ini_get_attr_visibility(void *data, int type)
#endif
{
	int rc;

	switch (type) {
	case ISCSI_BOOT_INI_INITIATOR_NAME:
		rc = S_IRUGO;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static ssize_t
qedi_show_boot_tgt_info(struct qedi_ctx *qedi, int type,
			char *buf, enum qedi_nvm_tgts idx)
{
	int rc = 1;
	u32 ctrl_flags, ipv6_en, chap_en, mchap_en, ip_len;
	struct nvm_iscsi_block *block;
	char *chap_name, *chap_secret, *fmt;
	char *mchap_name, *mchap_secret;

	block = qedi_get_nvram_block(qedi);
	if (!block)
		goto exit_show_tgt_info;

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_EVT,
		  "Port:%d, tgt_idx:%d\n",
		  GET_FIELD2(block->id, NVM_ISCSI_CFG_BLK_MAPPED_PF_ID), idx);

	ctrl_flags = block->target[idx].ctrl_flags &
		     NVM_ISCSI_CFG_TARGET_ENABLED;

	if (!ctrl_flags) {
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_EVT,
			  "Target disabled\n");
		goto exit_show_tgt_info;
	}

	ipv6_en = block->generic.ctrl_flags &
		  NVM_ISCSI_CFG_GEN_IPV6_ENABLED;
	fmt = ipv6_en ? "%pI6\n": "%pI4\n";
	ip_len = ipv6_en ? IPV6_LEN : IPV4_LEN;

	chap_en = block->generic.ctrl_flags &
		  NVM_ISCSI_CFG_GEN_CHAP_ENABLED;
	chap_name = chap_en ? block->initiator.chap_name.byte : NULL;
	chap_secret = chap_en ? block->initiator.chap_password.byte : NULL;

	mchap_en = block->generic.ctrl_flags &
		  NVM_ISCSI_CFG_GEN_CHAP_MUTUAL_ENABLED;
	mchap_name = mchap_en ? block->target[idx].chap_name.byte : NULL;
	mchap_secret = mchap_en ? block->target[idx].chap_password.byte : NULL;

	switch (type) {
	case ISCSI_BOOT_TGT_NAME:
		rc = sprintf(buf, "%.*s\n", NVM_ISCSI_CFG_ISCSI_NAME_MAX_LEN,
			     block->target[idx].target_name.byte);
		break;
	case ISCSI_BOOT_TGT_IP_ADDR:
		if (ipv6_en)
			rc = snprintf(buf, ip_len, fmt,
				      block->target[idx].ipv6_addr.byte);
		else
			rc = snprintf(buf, ip_len, fmt,
				      block->target[idx].ipv4_addr.byte);
		break;
	case ISCSI_BOOT_TGT_PORT:
		rc = snprintf(buf, 12, "%d\n",
			      GET_FIELD2(block->target[idx].generic_cont0,
				     NVM_ISCSI_CFG_TARGET_TCP_PORT));
		break;
	case ISCSI_BOOT_TGT_LUN:
		rc = snprintf(buf, 22, "%.*d\n",
			      block->target[idx].lun.value[1],
			      block->target[idx].lun.value[0]);
		break;
	case ISCSI_BOOT_TGT_CHAP_NAME:
		rc = sprintf(buf, "%.*s\n", NVM_ISCSI_CFG_CHAP_NAME_MAX_LEN,
			     chap_name);
		break;
	case ISCSI_BOOT_TGT_CHAP_SECRET:
		rc = sprintf(buf, "%.*s\n", NVM_ISCSI_CFG_CHAP_PWD_MAX_LEN,
			     chap_secret);
		break;
	case ISCSI_BOOT_TGT_REV_CHAP_NAME:
		rc = sprintf(buf, "%.*s\n", NVM_ISCSI_CFG_CHAP_NAME_MAX_LEN,
			     mchap_name);
		break;
	case ISCSI_BOOT_TGT_REV_CHAP_SECRET:
		rc = sprintf(buf, "%.*s\n", NVM_ISCSI_CFG_CHAP_PWD_MAX_LEN,
			     mchap_secret);
		break;
	case ISCSI_BOOT_TGT_FLAGS:
		rc = snprintf(buf, ISCSI_BOOT_STR_LEN, "%hhd\n",
			      SYSFS_FLAG_FW_SEL_BOOT);
		break;
	case ISCSI_BOOT_TGT_NIC_ASSOC:
		rc = snprintf(buf, ISCSI_BOOT_STR_LEN, "0\n");
		break;
	default:
		rc = 0;
		break;
	}

exit_show_tgt_info:
	return rc;
}

static ssize_t qedi_show_boot_tgt_pri_info(void *data, int type, char *buf)
{
	struct qedi_ctx *qedi = data;

	return qedi_show_boot_tgt_info(qedi, type, buf, QEDI_NVM_TGT_PRI);
}

static ssize_t qedi_show_boot_tgt_sec_info(void *data, int type, char *buf)
{
	struct qedi_ctx *qedi = data;

	return qedi_show_boot_tgt_info(qedi, type, buf, QEDI_NVM_TGT_SEC);
}

#if defined UMODE_T_USED
static umode_t qedi_tgt_get_attr_visibility(void *data, int type)
#else
static mode_t qedi_tgt_get_attr_visibility(void *data, int type)
#endif
{
	int rc;

	switch (type) {
	case ISCSI_BOOT_TGT_NAME:
	case ISCSI_BOOT_TGT_IP_ADDR:
	case ISCSI_BOOT_TGT_PORT:
	case ISCSI_BOOT_TGT_LUN:
	case ISCSI_BOOT_TGT_CHAP_NAME:
	case ISCSI_BOOT_TGT_CHAP_SECRET:
	case ISCSI_BOOT_TGT_REV_CHAP_NAME:
	case ISCSI_BOOT_TGT_REV_CHAP_SECRET:
	case ISCSI_BOOT_TGT_NIC_ASSOC:
	case ISCSI_BOOT_TGT_FLAGS:
		rc = S_IRUGO;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static void qedi_boot_release(void *data)
{
	struct qedi_ctx *qedi = data;

	scsi_host_put(qedi->shost);
}

static int qedi_get_boot_info(struct qedi_ctx *qedi)
{
	int ret = 1;

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
		  "Get NVM iSCSI CFG image\n");
	ret = qedi_ops->common->nvm_get_image(qedi->cdev,
					      QED_NVM_IMAGE_ISCSI_CFG,
					      (char *)qedi->iscsi_image,
					      sizeof(struct qedi_nvm_iscsi_image));
	if (ret)
		QEDI_ERR(&qedi->dbg_ctx,
			 "Could not get NVM image. ret = %d\n", ret);

	return ret;
}

static int qedi_setup_boot_info(struct qedi_ctx *qedi)
{
	struct iscsi_boot_kobj *boot_kobj;

	if (qedi_get_boot_info(qedi))
		return -EPERM;

	qedi->boot_kset = iscsi_boot_create_host_kset(qedi->shost->host_no);
	if (!qedi->boot_kset)
		goto kset_free;

	if (!scsi_host_get(qedi->shost))
		goto kset_free;

	boot_kobj = iscsi_boot_create_target(qedi->boot_kset, 0, qedi,
					     qedi_show_boot_tgt_pri_info,
					     qedi_tgt_get_attr_visibility,
					     qedi_boot_release);
	if (!boot_kobj)
		goto put_host;

	if (!scsi_host_get(qedi->shost))
		goto kset_free;

	boot_kobj = iscsi_boot_create_target(qedi->boot_kset, 1, qedi,
					     qedi_show_boot_tgt_sec_info,
					     qedi_tgt_get_attr_visibility,
					     qedi_boot_release);
	if (!boot_kobj)
		goto put_host;

	if (!scsi_host_get(qedi->shost))
		goto kset_free;

	boot_kobj = iscsi_boot_create_initiator(qedi->boot_kset, 0, qedi,
						qedi_show_boot_ini_info,
						qedi_ini_get_attr_visibility,
						qedi_boot_release);
	if (!boot_kobj)
		goto put_host;

	if (!scsi_host_get(qedi->shost))
		goto kset_free;

	boot_kobj = iscsi_boot_create_ethernet(qedi->boot_kset, 0, qedi,
					       qedi_show_boot_eth_info,
					       qedi_eth_get_attr_visibility,
					       qedi_boot_release);
	if (!boot_kobj)
		goto put_host;

	return 0;

put_host:
	scsi_host_put(qedi->shost);
kset_free:
	iscsi_boot_destroy_kset(qedi->boot_kset);
	qedi->boot_kset = NULL;
	return -ENOMEM;
}

static void __qedi_remove(struct pci_dev *pdev, int mode)
{
	struct qedi_ctx *qedi = pci_get_drvdata(pdev);
	int rc;
	u16 retry = 10;

	if (mode == QEDI_MODE_SHUTDOWN)
		iscsi_host_for_each_session(qedi->shost, qedi_clear_session_ctx);

	if (mode == QEDI_MODE_NORMAL || mode == QEDI_MODE_SHUTDOWN) {
		if (qedi->tmf_thread) {
			destroy_workqueue(qedi->tmf_thread);
			qedi->tmf_thread = NULL;
		}

		if (qedi->offload_thread) {
			destroy_workqueue(qedi->offload_thread);
			qedi->offload_thread = NULL;
		}
	}

#ifdef CONFIG_DEBUG_FS
	qedi_dbg_host_exit(&qedi->dbg_ctx);
#endif

	if (mode == QEDI_MODE_RECOVERY) {
		qedi_ops->common->set_dev_reuse(qedi->cdev, true);
		qedi_ops->common->set_recov_in_prog(qedi->cdev, true);
	}

	if (!test_bit(QEDI_IN_OFFLINE, &qedi->flags))
		qedi_ops->common->set_power_state(qedi->cdev, PCI_D0);

	qedi_sync_free_irqs(qedi);

	if (!test_bit(QEDI_IN_OFFLINE, &qedi->flags)) {
		while(retry--) {
			rc = qedi_ops->stop(qedi->cdev);
			if (rc < 0)
				msleep(1000);
			else
				break;
		}
		qedi_ops->ll2->stop(qedi->cdev);
	}

	qedi_free_iscsi_pf_param(qedi);

	rc = qedi_ops->common->update_drv_state(qedi->cdev, false);
	if (rc)
		QEDI_ERR(&qedi->dbg_ctx, "Failed to send drv state to MFW\n");

	if (!test_bit(QEDI_IN_OFFLINE, &qedi->flags)) {
		qedi_ops->common->slowpath_stop(qedi->cdev);
		qedi_ops->common->remove(qedi->cdev);
	}

	qedi_destroy_fp(qedi);

	if (mode == QEDI_MODE_NORMAL || mode == QEDI_MODE_SHUTDOWN) {
		qedi_release_cid_que(qedi);
		qedi_cm_free_mem(qedi);
		qedi_free_uio(qedi->udev);
		qedi_free_itt(qedi);

		if (test_and_clear_bit(QEDI_GRCDUMP_SETUP, &qedi->flags)) {
			qedi_free_grc_dump_buf(&qedi->grcdump);
			qedi_remove_sysfs_ctx_attr(qedi);
		}

		if (qedi->ll2_recv_thread) {
			kthread_stop(qedi->ll2_recv_thread);
			qedi->ll2_recv_thread = NULL;
		}
		qedi_ll2_free_skbs(qedi);

		if (qedi->boot_kset)
			iscsi_boot_destroy_kset(qedi->boot_kset);

#if defined IS_SHUTDOWN
		iscsi_host_remove(qedi->shost, false);
#else
		iscsi_host_remove(qedi->shost);
#endif
		iscsi_host_free(qedi->shost);
	}
}

static pci_ers_result_t qedi_pci_error_detected(struct pci_dev *pdev,
						pci_channel_state_t state)
{
	struct qedi_ctx *qedi = pci_get_drvdata(pdev);

	QEDI_ERR(&qedi->dbg_ctx, "%s: PCI error detected [%d]\n",
		 __func__, state);

	if (test_and_set_bit(QEDI_IN_RECOVERY, &qedi->flags)) {
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
			  "Recovery already in progress.\n");
		return PCI_ERS_RESULT_CAN_RECOVER;
	}

	atomic_set(&qedi->link_state, QEDI_LINK_DOWN);

	iscsi_host_for_each_session(qedi->shost, qedi_mark_conn_recovery);

	if (state == pci_channel_io_perm_failure)
		return PCI_ERS_RESULT_DISCONNECT;

	pci_disable_device(pdev);

	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t qedi_pci_slot_reset(struct pci_dev *pdev)
{
	struct qedi_ctx *qedi = pci_get_drvdata(pdev);
	int rc;

	QEDI_ERR(&qedi->dbg_ctx, "%s: PCI slot reset initializing...\n",
		 __func__);

	rc = pci_enable_device(pdev);
	if (rc) {
		dev_err(&pdev->dev,
			"Cannot re-enable PCI device after reset - %d\n", rc);
		return PCI_ERS_RESULT_DISCONNECT;
	}

	pci_restore_state(pdev);
	pci_save_state(pdev);

	qedi_ops->common->recovery_prolog(qedi->cdev);
	qedi_ops->common->set_recov_in_prog(qedi->cdev, true);

	/* Initiate unload path */
	__qedi_remove(qedi->pdev, QEDI_MODE_RECOVERY);

	qedi_ops->common->set_recov_in_prog(qedi->cdev, false);

	/* Perform cleanup of the PCIe registers */
#if defined CLEAR_NONFATAL_STATUS
	if (pci_aer_clear_nonfatal_status(pdev))
		QEDI_ERR(&qedi->dbg_ctx, "pci_aer_clear_nonfatal_status failed\n");
	else
		QEDI_ERR(&qedi->dbg_ctx,
			 "pci_aer_clear_nonfatal_status succeeded\n");
#else
	if (pci_cleanup_aer_uncorrect_error_status(pdev))
		QEDI_ERR(&qedi->dbg_ctx, "pci_cleanup_aer_uncorrect_error_status failed\n");
	else
		QEDI_ERR(&qedi->dbg_ctx,
			 "pci_cleanup_aer_uncorrect_error_status succeeded\n");
#endif

	return PCI_ERS_RESULT_RECOVERED;
}

static void qedi_pci_resume(struct pci_dev *pdev)
{
	struct qedi_ctx *qedi = pci_get_drvdata(pdev);
	int rc;

	QEDI_ERR(&qedi->dbg_ctx, "%s: PCI slot Resume...\n", __func__);

	rc = __qedi_probe(qedi->pdev, QEDI_MODE_RECOVERY);
	if (rc) {
		QEDI_ERR(&qedi->dbg_ctx, "%s: Load failed\n", __func__);
		return;
	}

	clear_bit(QEDI_IN_RECOVERY, &qedi->flags);
}

static void qedi_shutdown(struct pci_dev *pdev)
{
	struct qedi_ctx *qedi = pci_get_drvdata(pdev);

	QEDI_ERR(&qedi->dbg_ctx, "%s: Shutdown qedi\n", __func__);
	if(test_and_set_bit(QEDI_IN_SHUTDOWN, &qedi->flags))
		return;
	__qedi_remove(pdev, QEDI_MODE_SHUTDOWN);
}

int qedi_resume(struct pci_dev *pdev)
{
	struct qedi_ctx *qedi = pci_get_drvdata(pdev);

	QEDI_ERR(&qedi->dbg_ctx, "%s: driver does not support resume\n", __func__);

	return -EPERM;
}

int qedi_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct qedi_ctx *qedi = pci_get_drvdata(pdev);

	QEDI_ERR(&qedi->dbg_ctx, "%s: driver does not support suspend\n", __func__);

	return -EPERM;
}

static int __qedi_probe(struct pci_dev *pdev, int mode)
{
	struct qedi_ctx *qedi;
	struct qed_ll2_params params;
	u8 dp_level = 0;
	bool is_vf = false;
	char host_buf[16];
	struct qed_link_params link_params;
	struct qed_slowpath_params sp_params;
	struct qed_probe_params qed_params;
	void *task_start, *task_end;
	int rc;
	u16 tmp;
	u16 retry_cnt = 10;

	if (mode != QEDI_MODE_RECOVERY) {
		qedi = qedi_host_alloc(pdev);
		if (!qedi) {
			rc = -ENOMEM;
			goto exit_probe;
		}
	} else {
		qedi = pci_get_drvdata(pdev);
	}

retry_probe:
	if (mode == QEDI_MODE_RECOVERY)
		msleep(2000);

	memset(&qed_params, 0, sizeof(qed_params));
	qed_params.protocol = QED_PROTOCOL_ISCSI;
	qed_params.dp_module = qedi_qed_debug;
	qed_params.dp_level = dp_level;
	qed_params.is_vf = is_vf;

	if (mode == QEDI_MODE_RECOVERY) {
		qed_params.recov_in_prog = 1;
		qed_params.cdev = qedi->cdev;
	}

	qedi->cdev = qedi_ops->common->probe(pdev, &qed_params);
	if (!qedi->cdev) {
		if ((mode == QEDI_MODE_RECOVERY) && retry_cnt) {
			QEDI_ERR(&qedi->dbg_ctx,
				 "Retry %d initialize hardware\n", retry_cnt);
			retry_cnt--;
			goto retry_probe;
		}
		rc = -ENODEV;
		QEDI_ERR(&qedi->dbg_ctx, "Cannot initialize hardware\n");
		goto free_host;
	}

	set_bit(QEDI_ERR_ATTN_CLR_EN, &qedi->qedi_err_flags);
	set_bit(QEDI_ERR_IS_RECOVERABLE, &qedi->qedi_err_flags);

	atomic_set(&qedi->link_state, QEDI_LINK_DOWN);

	/* Needed info about global number of CQ queues */
	rc = qedi_ops->fill_dev_info(qedi->cdev, &qedi->dev_info);
	if (rc)
		goto free_host;

	QEDI_INFO(&(qedi->dbg_ctx), QEDI_LOG_DISC,
	    "dev_info: num_hwfns=%d affin_hwfn_idx=%d.\n",
	    qedi->dev_info.common.num_hwfns,
	    qedi_ops->common->get_affin_hwfn_idx(qedi->cdev));

	if (qedi_ll2_buf_size < 0x400 || qedi_ll2_buf_size > 0x2400) {
		qedi_ll2_buf_size = 0x400;
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
			  "Setting LL2 buf size to 0x%x bytes\n",
			  qedi_ll2_buf_size);
	}

	rc = qedi_set_iscsi_pf_param(qedi);
	if (rc) {
		rc = -ENOMEM;
		QEDI_ERR(&qedi->dbg_ctx,
			 "Set iSCSI pf param fail\n");
		goto free_host;
	}

	qedi_ops->common->update_pf_params(qedi->cdev, &qedi->pf_params);

	rc = qedi_prepare_fp(qedi);
	if (rc) {
		QEDI_ERR(&qedi->dbg_ctx, "Cannot start slowpath.\n");
		goto free_pf_params;
	}

	/* Start the Slowpath-process */
	memset(&sp_params, 0, sizeof(struct qed_slowpath_params));
	sp_params.int_mode = QED_INT_MODE_MSIX;
	sp_params.drv_major = QEDI_DRIVER_MAJOR_VER;
	sp_params.drv_minor = QEDI_DRIVER_MINOR_VER;
	sp_params.drv_rev = QEDI_DRIVER_REV_VER;
	sp_params.drv_eng = QEDI_DRIVER_ENG_VER;
	strlcpy(sp_params.name, "qedi iSCSI", QED_DRV_VER_STR_SIZE);
	rc = qedi_ops->common->slowpath_start(qedi->cdev, &sp_params);
	if (rc) {
		QEDI_ERR(&qedi->dbg_ctx, "Cannot start slowpath\n");
		goto stop_hw;
	}

	/* update_pf_params needs to be called before and after slowpath
	 * start
	 */
	qedi_ops->common->update_pf_params(qedi->cdev, &qedi->pf_params);

	rc = qedi_setup_int(qedi);
	if (rc)
		goto stop_iscsi_func;

	qedi_ops->common->set_power_state(qedi->cdev, PCI_D0);

	/* Learn information crucial for qedi to progress */
	rc = qedi_ops->fill_dev_info(qedi->cdev, &qedi->dev_info);
	if (rc)
		goto stop_iscsi_func;

	/* Record BDQ producer doorbell addresses */
	qedi->bdq_primary_prod = qedi->dev_info.primary_dbq_rq_addr;
	qedi->bdq_secondary_prod = qedi->dev_info.secondary_bdq_rq_addr;
	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_DISC,
		  "BDQ primary_prod=%p secondary_prod=%p.\n",
		  qedi->bdq_primary_prod,
		  qedi->bdq_secondary_prod);

	/*
	 * We need to write the number of BDs in the BDQ we've preallocated so
	 * the f/w will do a prefetch and we'll get an unsolicited CQE when a
	 * packet arrives.
	 */
	qedi->bdq_prod_idx = QEDI_BDQ_NUM;
	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_DISC,
		  "Writing %d to primary and secondary BDQ doorbell registers.\n",
		  qedi->bdq_prod_idx);
	writew(qedi->bdq_prod_idx, qedi->bdq_primary_prod);
	tmp = readw(qedi->bdq_primary_prod);
	writew(qedi->bdq_prod_idx, qedi->bdq_secondary_prod);
	tmp = readw(qedi->bdq_secondary_prod);

	ether_addr_copy(qedi->mac, qedi->dev_info.common.hw_mac);
	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_DISC, "MAC address is %pM.\n",
		  qedi->mac);

	snprintf(host_buf, sizeof(host_buf), "host_%d", qedi->shost->host_no);
	qedi_ops->common->set_name(qedi->cdev, host_buf);
	qedi_ops->register_ops(qedi->cdev, &qedi_cb_ops, qedi);

	memset(&params, 0, sizeof(params));
	params.mtu = DEF_PATH_MTU + IPV6_HDR_LEN + TCP_HDR_LEN;
	qedi->ll2_mtu = DEF_PATH_MTU;
	params.drop_ttl0_packets = 0;
	params.rx_vlan_stripping = 1;
	ether_addr_copy(params.ll2_mac_address, qedi->dev_info.common.hw_mac);

	if (mode != QEDI_MODE_RECOVERY) {
		/* set up rx path */
		INIT_LIST_HEAD(&qedi->ll2_skb_list);
		spin_lock_init(&qedi->ll2_lock);
		/* start qedi context */
		spin_lock_init(&qedi->hba_lock);
		spin_lock_init(&qedi->task_idx_lock);
		mutex_init(&qedi->stats_lock);
	}
	qedi_ops->ll2->register_cb_ops(qedi->cdev, &qedi_ll2_cb_ops, qedi);
	qedi_ops->ll2->start(qedi->cdev, &params);

	if (mode != QEDI_MODE_RECOVERY) {
		qedi->ll2_recv_thread = kthread_run(qedi_ll2_recv_thread,
						    (void *)qedi,
						    "qedi_ll2_thread");
	}

	rc = qedi_ops->start(qedi->cdev, &qedi->tasks,
			     qedi, qedi_iscsi_event_cb);
	if (rc) {
		rc = -ENODEV;
		QEDI_ERR(&qedi->dbg_ctx, "Cannot start iSCSI function\n");
		goto stop_slowpath;
	}

	task_start = qedi_get_task_mem(&qedi->tasks, 0);
	task_end = qedi_get_task_mem(&qedi->tasks, MAX_TID_BLOCKS_ISCSI - 1);
	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_DISC,
		  "Task context start=%p, end=%p block_size=%u.\n",
		   task_start, task_end, qedi->tasks.size);

	memset(&link_params, 0, sizeof(link_params));
	link_params.link_up = true;
	rc = qedi_ops->common->set_link(qedi->cdev, &link_params);
	if (rc) {
		QEDI_WARN(&qedi->dbg_ctx, "Link set up failed.\n");
		atomic_set(&qedi->link_state, QEDI_LINK_DOWN);
	}

#ifdef CONFIG_DEBUG_FS
	qedi_dbg_host_init(&qedi->dbg_ctx, &qedi_debugfs_ops,
			    &qedi_dbg_fops);
#endif

	if (mode != QEDI_MODE_RECOVERY) {
#if defined(NR_HW_QUEUES) && !defined(USE_BLK_MQ)
		qedi->shost->nr_hw_queues = qedi->num_queues;
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
		  "Num. of HW queues set to %d\n",
		  qedi->shost->nr_hw_queues);
#elif defined NR_HW_QUEUES
		/* Set number of hardware dispatch queues for blk-mq */
		if (shost_use_blk_mq(qedi->shost)) {
			qedi->shost->nr_hw_queues = qedi->num_queues;
			QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
			  "Num. of HW queues set to %d\n",
			  qedi->shost->nr_hw_queues);
		}
#endif
		if (iscsi_host_add(qedi->shost, &pdev->dev)) {
			QEDI_ERR(&qedi->dbg_ctx,
				 "Could not add iscsi host\n");
			rc = -ENOMEM;
			goto remove_host;
		}

		/* Allocate uio buffers */
		rc = qedi_alloc_uio_rings(qedi, 1);
		if (rc) {
			QEDI_ERR(&qedi->dbg_ctx,
				 "UIO alloc ring failed err=%d\n", rc);
			goto remove_host;
		}

		rc = qedi_init_uio(qedi);
		if (rc) {
			QEDI_ERR(&qedi->dbg_ctx,
				 "UIO init failed, err=%d\n", rc);
			goto free_uio;
		}

		/* host the array on iscsi_conn */
		rc = qedi_setup_cid_que(qedi);
		if (rc) {
			QEDI_ERR(&qedi->dbg_ctx,
				 "Could not setup cid que\n");
			goto free_uio;
		}

		rc = qedi_cm_alloc_mem(qedi);
		if (rc) {
			QEDI_ERR(&qedi->dbg_ctx,
				 "Could not alloc cm memory\n");
			goto free_cid_que;
		}

		rc = qedi_alloc_itt(qedi);
		if (rc) {
			QEDI_ERR(&qedi->dbg_ctx,
				 "Could not alloc itt memory\n");
			goto free_cid_que;
		}

		sprintf(host_buf, "host_%d", qedi->shost->host_no);
		qedi->tmf_thread = create_singlethread_workqueue(host_buf);
		if (!qedi->tmf_thread) {
			QEDI_ERR(&qedi->dbg_ctx,
				 "Unable to start tmf thread!\n");
			rc = -ENODEV;
			goto free_cid_que;
		}

		sprintf(host_buf, "qedi_ofld%d", qedi->shost->host_no);
		qedi->offload_thread = create_workqueue(host_buf);
		if (!qedi->offload_thread) {
			QEDI_ERR(&qedi->dbg_ctx,
				 "Unable to start offload thread!\n");
			rc = -ENODEV;
			goto free_cid_que;
		}

		INIT_DELAYED_WORK(&qedi->recovery_work, qedi_recovery_handler);
		INIT_DELAYED_WORK(&qedi->board_disable_work,
				  qedi_board_disable_work);
		INIT_DELAYED_WORK(&qedi->grcdump_work, qedi_wq_grcdump);
		INIT_DELAYED_WORK(&qedi->recovery_process_work, qedi_recovery_process);

		qedi->grcdump_size =
		    qedi_ops->common->dbg_all_data_size(qedi->cdev);
		if (qedi->grcdump_size) {
			rc = qedi_alloc_grc_dump_buf(&qedi->grcdump,
						     qedi->grcdump_size);
			if (!rc) {
				QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
					  "grcdump: addr=%p, size=%u\n",
					  qedi->grcdump, qedi->grcdump_size);

				rc = qedi_create_sysfs_ctx_attr(qedi);
				if (!rc)
					set_bit(QEDI_GRCDUMP_SETUP,
						&qedi->flags);
				else
					qedi_free_grc_dump_buf(&qedi->grcdump);

			} else {
				QEDI_ERR(&qedi->dbg_ctx,
					 "GRC Dump buffer alloc failed\n");
				qedi->grcdump = NULL;
			}
		}

		/* F/w needs 1st task context memory entry for performance */
		set_bit(QEDI_RESERVE_TASK_ID, qedi->task_idx_map);
		atomic_set(&qedi->num_offloads, 0);

		if (qedi_setup_boot_info(qedi))
			QEDI_ERR(&qedi->dbg_ctx,
				 "No iSCSI boot target configured\n");
	}

	qedi_ops->common->set_dev_reuse(qedi->cdev, false);

	rc = qedi_ops->common->update_drv_state(qedi->cdev, true);
	if (rc)
		QEDI_ERR(&qedi->dbg_ctx, "Failed to send drv state to MFW\n");

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
		  "QLogic FastLinQ iSCSI Module qedi %s, FW %d.%d.%d.%d\n",
		  QEDI_MODULE_VERSION, FW_MAJOR_VERSION, FW_MINOR_VERSION,
		   FW_REVISION_VERSION, FW_ENGINEERING_VERSION);
	return 0;

free_cid_que:
	qedi_release_cid_que(qedi);
free_uio:
	qedi_free_uio(qedi->udev);
remove_host:
#ifdef CONFIG_DEBUG_FS
	qedi_dbg_host_exit(&qedi->dbg_ctx);
#endif

#if defined IS_SHUTDOWN
	iscsi_host_remove(qedi->shost, false);
#else
	iscsi_host_remove(qedi->shost);
#endif
stop_iscsi_func:
	qedi_ops->stop(qedi->cdev);
stop_slowpath:
	qedi_ops->common->slowpath_stop(qedi->cdev);
stop_hw:
	qedi_ops->common->remove(qedi->cdev);
free_pf_params:
	qedi_free_iscsi_pf_param(qedi);
free_host:
	iscsi_host_free(qedi->shost);
exit_probe:
	return rc;
}

static void qedi_mark_conn_recovery(struct iscsi_cls_session *cls_session)
{
	struct iscsi_session *session = cls_session->dd_data;
	struct iscsi_conn *conn = session->leadconn;
	struct qedi_conn *qedi_conn = conn->dd_data;

	iscsi_conn_failure(qedi_conn->cls_conn->dd_data, ISCSI_ERR_CONN_FAILED);
}

static void qedi_recovery_handler(struct work_struct *work)
{
	struct qedi_ctx *qedi =
			container_of(work, struct qedi_ctx, recovery_work.work);


	iscsi_host_for_each_session(qedi->shost, qedi_mark_conn_recovery);

	QEDI_ERR(&qedi->dbg_ctx, "Recovery work start.\n");

	/*
	 * Call common_ops->recovery_prolog to allow the MFW to quiesce
	 * any PCI transactions.
	 */
	qedi_ops->common->recovery_prolog(qedi->cdev);

	__qedi_remove(qedi->pdev, QEDI_MODE_RECOVERY);
	__qedi_probe(qedi->pdev, QEDI_MODE_RECOVERY);
	clear_bit(QEDI_IN_RECOVERY, &qedi->flags);
	QEDI_ERR(&qedi->dbg_ctx, "Recovery work complete.\n");
}

static void qedi_board_disable_work(struct work_struct *work)
{
	struct qedi_ctx *qedi =
			container_of(work, struct qedi_ctx,
				     board_disable_work.work);

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
		  "Fan failure, Unloading firmware context start.\n");

	if(test_and_set_bit(QEDI_IN_SHUTDOWN, &qedi->flags))
		return;

	__qedi_remove(qedi->pdev, QEDI_MODE_SHUTDOWN);
}

static int qedi_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	if (!qedi_trans_register) {
		qedi_scsi_transport =
			iscsi_register_transport(&qedi_iscsi_transport); 
		if (!qedi_scsi_transport) {
			QEDI_ERR(NULL, "Could not register qedi transport");
			return -ENODEV;
		}
		QEDI_INFO(NULL, QEDI_LOG_INFO, "QEDI iscsi transport registered.\n");
		qedi_trans_register = true;
	}
	return __qedi_probe(pdev, QEDI_MODE_NORMAL);
}

static void qedi_remove(struct pci_dev *pdev)
{
	__qedi_remove(pdev, QEDI_MODE_NORMAL);
}

static struct pci_device_id qedi_pci_tbl[] = {
	{
		.vendor		= PCI_VENDOR_ID_QLOGIC,
		.device		= 0x165E,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
	},
	{
		.vendor         = PCI_VENDOR_ID_QLOGIC,
		.device         = 0x8084,
		.subvendor      = PCI_ANY_ID,
		.subdevice      = PCI_ANY_ID,
	},
	{0, 0},
};
MODULE_DEVICE_TABLE(pci, qedi_pci_tbl);

#ifdef USE_CPU_HP
static enum cpuhp_state qedi_cpuhp_state;
#endif

static struct pci_error_handlers qedi_err_handler = {
	.error_detected = qedi_pci_error_detected,
	.slot_reset = qedi_pci_slot_reset,
	.resume = qedi_pci_resume,
};

static struct pci_driver qedi_pci_driver = {
	.name = QEDI_MODULE_NAME,
	.id_table = qedi_pci_tbl,
	.probe = qedi_probe,
	.remove = qedi_remove,
	.shutdown = qedi_shutdown,
	.resume = qedi_resume,
	.suspend = qedi_suspend,
	.err_handler = &qedi_err_handler,
};

static int __init qedi_init(void)
{
	int rc = 0;
	int ret;
	struct qedi_percpu_s *p;
	unsigned cpu = 0;
	u32 qed_ver;

	/* Print driver banner */
        QEDI_INFO(NULL, QEDI_LOG_INFO, "%s v%s.\n", QEDI_DESCR,
                   QEDI_MODULE_VERSION);

	qed_ver = qed_get_protocol_version(QED_PROTOCOL_ISCSI);
	if (qed_ver !=  QEDI_ISCSI_INTERFACE_VERSION) {
		QEDI_ERR(NULL, "Version mismatch [%08d != %08d]\n",
			 qed_ver, QEDI_ISCSI_INTERFACE_VERSION);
		return -EINVAL;
	}

	qedi_ops = qed_get_iscsi_ops(QEDI_ISCSI_INTERFACE_VERSION);
	if (!qedi_ops) {
		QEDI_ERR(NULL, "Failed to get qed iSCSI operations\n");
		return -EINVAL;
	}

#ifdef CONFIG_DEBUG_FS
	qedi_dbg_init("qedi");
#endif

#ifndef USE_CPU_HP
	register_hotcpu_notifier(&qedi_cpu_notifier);
#endif
	for_each_possible_cpu(cpu) {
		p = &per_cpu(qedi_percpu, cpu);
		INIT_LIST_HEAD(&p->work_list);
		spin_lock_init(&p->p_work_lock);
		p->iothread = NULL;
	}

#ifndef USE_CPU_HP
	for_each_online_cpu(cpu)
		qedi_percpu_thread_create(cpu);
#else
	rc = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "scsi/qedi:online",
			       qedi_cpu_online, qedi_cpu_offline);
	if (rc < 0)
		goto exit_qedi_init_1;
	qedi_cpuhp_state = rc;
#endif
	ret = pci_register_driver(&qedi_pci_driver);
	if (ret) {
		QEDI_ERR(NULL, "Failed to register driver\n");
		rc = -EINVAL;
#ifndef USE_CPU_HP
		goto exit_qedi_init_1;
#else
		goto exit_qedi_hp;
#endif
	}

	return 0;

#ifdef USE_CPU_HP
exit_qedi_hp:
	cpuhp_remove_state(qedi_cpuhp_state);
#endif
exit_qedi_init_1:
#ifdef CONFIG_DEBUG_FS
	qedi_dbg_exit();
#endif
	qed_put_iscsi_ops();
	return rc;
}

static void __exit qedi_cleanup(void)
{
#ifndef USE_CPU_HP
	unsigned cpu = 0;

	for_each_online_cpu(cpu)
		qedi_percpu_thread_destroy(cpu);
#endif
	pci_unregister_driver(&qedi_pci_driver);
#ifdef USE_CPU_HP
	cpuhp_remove_state(qedi_cpuhp_state);
#else
	unregister_hotcpu_notifier(&qedi_cpu_notifier);
#endif
	if (qedi_trans_register)
		iscsi_unregister_transport(&qedi_iscsi_transport);

#ifdef CONFIG_DEBUG_FS
	qedi_dbg_exit();
#endif
	qed_put_iscsi_ops();
}

MODULE_DESCRIPTION("QLogic FastLinQ 4xxxx iSCSI Module");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("QLogic Corporation");
MODULE_VERSION(QEDI_MODULE_VERSION);
#ifdef THUNK_INLINE
MODULE_INFO(retpoline, "Y");
#endif
module_init(qedi_init);
module_exit(qedi_cleanup);
