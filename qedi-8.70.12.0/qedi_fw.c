/*
 *  QLogic iSCSI Offload Driver
 *  Copyright (c) 2015-2018 Cavium Inc.
 *
 *  See LICENSE.qedi for copyright and licensing details.
 */

#include <linux/blkdev.h>
#include <scsi/scsi_tcq.h>
#include <linux/delay.h>

#include "qedi.h"
#include "qedi_iscsi.h"
#include "qedi_gbl.h"
#include "drv_e4_iscsi_fw_funcs.h"
#include "drv_scsi_fw_funcs.h"

static int send_iscsi_tmf(struct qedi_conn *qedi_conn,
			  struct iscsi_task *mtask, struct iscsi_task *ctask);

static void qedi_ring_doorbell(struct qedi_conn *qedi_conn)
{
	qedi_conn->ep->db_data.sq_prod = qedi_conn->ep->fw_sq_prod_idx;

	/* wmb - Make sure fw idx is coherent */
	wmb();
	writel(*(u32 *)&qedi_conn->ep->db_data, qedi_conn->ep->p_doorbell);
 
	/* Fence required to flush the write combined buffer, since another
	 * CPU may write to the same doorbell address and data may be lost
	 * due to relaxed order nature of write combined bar.
	 */
	wmb();
	QEDI_INFO(&qedi_conn->qedi->dbg_ctx, QEDI_LOG_MP_REQ,
		  "prod_idx=0x%x, fw_prod_idx=0x%x, cid=0x%x\n",
		  qedi_conn->ep->sq_prod_idx, qedi_conn->ep->fw_sq_prod_idx,
		  qedi_conn->iscsi_conn_id);
}

static u16 qedi_get_wqe_idx(struct qedi_conn *qedi_conn)
{
	struct qedi_endpoint *ep;
	u16 rval;

	ep = qedi_conn->ep;
	rval = ep->sq_prod_idx;

	/* Increament SQ index */
	ep->sq_prod_idx++;
	ep->fw_sq_prod_idx++;
	if (ep->sq_prod_idx == QEDI_SQ_SIZE)
		ep->sq_prod_idx = 0;

	return rval;
}

void qedi_dump_buffer(uint8_t *b, uint32_t size)
{
	uint32_t cnt;
	uint8_t c;

	pr_err(" 0   1   2   3   4   5   6   7   8   "
	    "9  Ah  Bh  Ch  Dh  Eh  Fh\n");
	pr_err("----------------------------------"
	    "----------------------------\n");

	pr_err(" ");
	for (cnt = 0; cnt < size;) {
		c = *b++;
		printk("%02x", (uint32_t) c);
		cnt++;
		if (!(cnt % 16))
			printk("\n");
		else
			printk("  ");
	}
	if (cnt % 16)
		pr_err("\n");
}

void print_params(struct e4_iscsi_task_params    *task_params,
		struct iscsi_conn_params    *conn_params,
		struct iscsi_cmd_hdr  *iscsi_cmd_hdr,
		struct scsi_sgl_task_params *tx_sgl_task_params,
		struct scsi_sgl_task_params *rx_sgl_task_params)
{
	//pr_crit("%s(): ystorm_iscsi_task_st_ctx 0x%d.\n", __func__, sizeof(struct ystorm_iscsi_task_st_ctx));
	//pr_crit("%s(): mstorm_iscsi_task_st_ctx 0x%d.\n", __func__, sizeof(struct mstorm_iscsi_task_st_ctx));
	pr_crit("%s(): task_params->context=0x%p.\n", __func__, task_params->context);
	pr_crit("%s(): task_params->sqe=0x%p.\n", __func__, task_params->sqe);
	pr_crit("%s(): task_params->tx_io_size=%d.\n", __func__, task_params->tx_io_size);
	pr_crit("%s(): task_params->rx_io_size=%d.\n", __func__, task_params->rx_io_size);
	pr_crit("%s(): task_params->conn_icid=%04x.\n", __func__, task_params->conn_icid);
	pr_crit("%s(): task_params->itid=0x%x.\n", __func__, task_params->itid);
	pr_crit("%s(): task_params->cq_rss_number=%d.\n", __func__, task_params->cq_rss_number);

	if (conn_params != NULL) {
		pr_crit("%s(): conn_params->first_burst_length=0x%x.\n",
			 __func__, conn_params->first_burst_length);
		pr_crit("%s(): conn_params->max_send_pdu_length=0x%x.\n",
			__func__, conn_params->max_send_pdu_length);
		pr_crit("%s(): conn_params->max_burst_length=0x%x.\n",
			__func__, conn_params->max_burst_length);
		pr_crit("%s(): conn_params->initial_r2t=0x%x.\n",
			 __func__, conn_params->initial_r2t);
		pr_crit("%s(): conn_params->immediate_data=0x%x.\n",
			__func__, conn_params->immediate_data);
	}

	if (tx_sgl_task_params != NULL) {
		pr_crit("%s(): tx_sgl_task_params.sgl=0x%p.\n",
			__func__, tx_sgl_task_params->sgl);
		pr_crit("%s(): tx_sgl_task_params.sgl_phys_addr.lo=0x%08x",
			 __func__, tx_sgl_task_params->sgl_phys_addr.lo);
		pr_crit("%s(): tx_sgl_task_params.sgl_phys_addr.hi=0x%08x",
			__func__, tx_sgl_task_params->sgl_phys_addr.hi);
		pr_crit("%s(): tx_sgl_task_params.num_sges=%d.\n",
			__func__, tx_sgl_task_params->num_sges);
		pr_crit("%s(): tx_sgl_task_params.total_buffer_size=%d.\n",
			__func__, tx_sgl_task_params->total_buffer_size);
		pr_crit("%s(): tx_sgl_task_params.small_mid_sge=%d.\n",
			__func__, tx_sgl_task_params->small_mid_sge);
	}

	if (rx_sgl_task_params != NULL) {
		pr_crit("%s(): rx_sgl_task_params.sgl=0x%p.\n",
			__func__, rx_sgl_task_params->sgl);
		pr_crit("%s(): rx_sgl_task_params.sgl_phys_addr.lo=0x%08x",
			__func__, rx_sgl_task_params->sgl_phys_addr.lo);
		pr_crit("%s(): rx_sgl_task_params.sgl_phys_addr.hi=0x%08x",
			__func__, rx_sgl_task_params->sgl_phys_addr.hi);
		pr_crit("%s(): rx_sgl_task_params.num_sges=%d.\n",
			__func__, rx_sgl_task_params->num_sges);
		pr_crit("%s(): rx_sgl_task_params.total_buffer_size=%d.\n",
			 __func__, rx_sgl_task_params->total_buffer_size);
		pr_crit("%s(): rx_sgl_task_params.small_mid_sge=%d.\n",
			__func__, rx_sgl_task_params->small_mid_sge);
	}
}

int qedi_send_iscsi_login(struct qedi_conn *qedi_conn,
			  struct iscsi_task *task)
{
	struct iscsi_login_req_hdr login_req_pdu_header;
	struct scsi_sgl_task_params tx_sgl_task_params;
	struct scsi_sgl_task_params rx_sgl_task_params;
	struct e4_iscsi_task_params task_params;
	struct iscsi_task_context *fw_task_ctx;
	struct qedi_ctx *qedi = qedi_conn->qedi;
#if defined STRUCT_LOGIN_REQ
	struct iscsi_login_req *login_hdr;
#else
	struct iscsi_login *login_hdr;
#endif
	struct scsi_sge *req_sge = NULL;
	struct scsi_sge *resp_sge = NULL;
	struct qedi_cmd *qedi_cmd;
	struct qedi_endpoint *ep;
	s16 tid = 0;
	u16 sq_idx = 0;
	int rval = 0;

	req_sge = (struct scsi_sge *)qedi_conn->gen_pdu.req_bd_tbl;
	resp_sge = (struct scsi_sge *)qedi_conn->gen_pdu.resp_bd_tbl;
	qedi_cmd = (struct qedi_cmd *)task->dd_data;
	ep = qedi_conn->ep;
#if defined STRUCT_LOGIN_REQ
	login_hdr = (struct iscsi_login_req *)task->hdr;
#else
	login_hdr = (struct iscsi_login *)task->hdr;
#endif

	tid = qedi_get_task_idx(qedi);
	if (tid == -1)
		return -ENOMEM;

	fw_task_ctx =
	  (struct iscsi_task_context *)qedi_get_task_mem(&qedi->tasks, tid);
	memset(fw_task_ctx, 0, sizeof(struct iscsi_task_context));

	qedi_cmd->task_id = tid;

	memset(&task_params, 0, sizeof(task_params));
	memset(&login_req_pdu_header, 0, sizeof(login_req_pdu_header));
	memset(&tx_sgl_task_params, 0, sizeof(tx_sgl_task_params));
	memset(&rx_sgl_task_params, 0, sizeof(rx_sgl_task_params));
	/* Update header info */
	login_req_pdu_header.opcode = login_hdr->opcode;
	login_req_pdu_header.version_min = login_hdr->min_version;
	login_req_pdu_header.version_max = login_hdr->max_version;
	login_req_pdu_header.flags_attr = login_hdr->flags;
	login_req_pdu_header.isid_tabc = swab32p((u32 *)login_hdr->isid);
	login_req_pdu_header.isid_d = swab16p((u16 *)&login_hdr->isid[4]);

	login_req_pdu_header.tsih = login_hdr->tsih;
	login_req_pdu_header.hdr_second_dword = ntoh24(login_hdr->dlength);

	qedi_update_itt_map(qedi, tid, task->itt);
	login_req_pdu_header.itt = qedi_set_itt(tid, get_itt(task->itt));
	login_req_pdu_header.cid = qedi_conn->iscsi_conn_id;
	login_req_pdu_header.cmd_sn = be32_to_cpu(login_hdr->cmdsn);
	login_req_pdu_header.exp_stat_sn = be32_to_cpu(login_hdr->exp_statsn);
	login_req_pdu_header.exp_stat_sn = 0;

	/* Fill tx AHS and rx buffer */
	tx_sgl_task_params.sgl = (struct scsi_sge *)qedi_conn->gen_pdu.req_bd_tbl;
	tx_sgl_task_params.sgl_phys_addr.lo = (u32)(qedi_conn->gen_pdu.req_dma_addr); 
	tx_sgl_task_params.sgl_phys_addr.hi =
				   (u32)((u64)qedi_conn->gen_pdu.req_dma_addr >> 32);
	tx_sgl_task_params.total_buffer_size = ntoh24(login_hdr->dlength);
	tx_sgl_task_params.num_sges = 1;

	rx_sgl_task_params.sgl = (struct scsi_sge *)qedi_conn->gen_pdu.resp_bd_tbl;
	rx_sgl_task_params.sgl_phys_addr.lo = (u32)(qedi_conn->gen_pdu.resp_dma_addr); 
	rx_sgl_task_params.sgl_phys_addr.hi =
				   (u32)((u64)qedi_conn->gen_pdu.resp_dma_addr >> 32);
	rx_sgl_task_params.total_buffer_size = resp_sge->sge_len;
	rx_sgl_task_params.num_sges = 1;

	/* Fill fw input params */
	task_params.context = fw_task_ctx;
	task_params.conn_icid = (u16)qedi_conn->iscsi_conn_id;
	task_params.itid = tid;
	task_params.cq_rss_number = 0;
	task_params.tx_io_size = ntoh24(login_hdr->dlength);
	task_params.rx_io_size = resp_sge->sge_len;

	sq_idx = qedi_get_wqe_idx(qedi_conn);
	task_params.sqe = &ep->sq[sq_idx];

	memset(task_params.sqe, 0, sizeof(struct iscsi_wqe));
	rval = init_initiator_login_request_task(&task_params,
						 &login_req_pdu_header,
						 &tx_sgl_task_params,
						 &rx_sgl_task_params);
	if (rval)
		return -1;

	spin_lock(&qedi_conn->list_lock);
	list_add_tail(&qedi_cmd->io_cmd, &qedi_conn->active_cmd_list);
	qedi_cmd->io_cmd_in_list = true;
	qedi_conn->active_cmd_count++;
	spin_unlock(&qedi_conn->list_lock);

	qedi_ring_doorbell(qedi_conn);
	return 0;
}

int qedi_send_iscsi_logout(struct qedi_conn *qedi_conn,
			   struct iscsi_task *task)
{
	struct iscsi_logout_req_hdr logout_pdu_header;
	struct scsi_sgl_task_params tx_sgl_task_params;
	struct scsi_sgl_task_params rx_sgl_task_params;
	struct e4_iscsi_task_params task_params;
	struct iscsi_task_context *fw_task_ctx;
	struct iscsi_logout *logout_hdr = NULL;
	struct qedi_ctx *qedi = qedi_conn->qedi;
	struct qedi_cmd *qedi_cmd;
	struct qedi_endpoint *ep;
	s16 tid = 0;
	u16 sq_idx = 0;
	int rval = 0;

	qedi_cmd = (struct qedi_cmd *)task->dd_data;
	logout_hdr = (struct iscsi_logout *)task->hdr;
	ep = qedi_conn->ep;

	tid = qedi_get_task_idx(qedi);
	if (tid == -1)
		return -ENOMEM;

	fw_task_ctx =
	  (struct iscsi_task_context *)qedi_get_task_mem(&qedi->tasks, tid);
	memset(fw_task_ctx, 0, sizeof(struct iscsi_task_context));

	qedi_cmd->task_id = tid;

	memset(&task_params, 0, sizeof(task_params));
	memset(&logout_pdu_header, 0, sizeof(logout_pdu_header));
	memset(&tx_sgl_task_params, 0, sizeof(tx_sgl_task_params));
	memset(&rx_sgl_task_params, 0, sizeof(rx_sgl_task_params));

	/* Update header info */
	logout_pdu_header.opcode = logout_hdr->opcode;
	logout_pdu_header.reason_code = 0x80 | logout_hdr->flags;
	qedi_update_itt_map(qedi, tid, task->itt);
	logout_pdu_header.itt = qedi_set_itt(tid, get_itt(task->itt));
	logout_pdu_header.exp_stat_sn = be32_to_cpu(logout_hdr->exp_statsn);
	logout_pdu_header.cmd_sn = be32_to_cpu(logout_hdr->cmdsn);
	logout_pdu_header.cid = qedi_conn->iscsi_conn_id;

	/* Fill fw input params */
	task_params.context = fw_task_ctx; 
	task_params.conn_icid = (u16)qedi_conn->iscsi_conn_id;
	task_params.itid = tid;
	task_params.cq_rss_number = 0;
	task_params.tx_io_size = 0;
	task_params.rx_io_size = 0;

	sq_idx = qedi_get_wqe_idx(qedi_conn);
	task_params.sqe = &ep->sq[sq_idx];
	memset(task_params.sqe, 0, sizeof(struct iscsi_wqe));

	rval = init_initiator_logout_request_task(&task_params,
						  &logout_pdu_header,
						  NULL, NULL);
	if (rval)
		return -1;

	spin_lock(&qedi_conn->list_lock);
	list_add_tail(&qedi_cmd->io_cmd, &qedi_conn->active_cmd_list);
	qedi_cmd->io_cmd_in_list = true;
	qedi_conn->active_cmd_count++;
	spin_unlock(&qedi_conn->list_lock);

	qedi_ring_doorbell(qedi_conn);
	return 0;
}

int qedi_cleanup_all_io(struct qedi_ctx *qedi, struct qedi_conn *qedi_conn,
			struct iscsi_task *task, bool in_recovery)
{
	int rval;
	struct iscsi_task *ctask;
	struct qedi_cmd *cmd, *cmd_tmp;
	struct iscsi_tm *tmf_hdr;
	unsigned int lun = 0;
	bool lun_reset = false;
	struct iscsi_conn *conn = qedi_conn->cls_conn->dd_data;
	struct iscsi_session *session = conn->session;

	/* From recovery, task is NULL or from tmf resp valid task */
	if (task) {
		tmf_hdr = (struct iscsi_tm *)task->hdr;

		if ((tmf_hdr->flags & ISCSI_FLAG_TM_FUNC_MASK) ==
			ISCSI_TM_FUNC_LOGICAL_UNIT_RESET) {
			lun_reset = true;
			lun = scsilun_to_int(&tmf_hdr->lun);
		}
	}

	atomic_set(&qedi_conn->cmd_cleanup_req, 0);
	atomic_set(&qedi_conn->cmd_cleanup_cmpl, 0);

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_SCSI_TM,
		  "active_cmd_count=%d, cid=0x%x, in_recovery=%d, lun_reset=%d\n",
		  qedi_conn->active_cmd_count, qedi_conn->iscsi_conn_id,
		  in_recovery, lun_reset);

	if (lun_reset) {
	#if defined SESS_LOCK
		spin_lock_bh(&session->back_lock);
	#else
		spin_lock_bh(&session->lock);
	#endif
	}

	spin_lock(&qedi_conn->list_lock);

	list_for_each_entry_safe(cmd, cmd_tmp, &qedi_conn->active_cmd_list,
				 io_cmd) {
		ctask = cmd->task;
		if (ctask == task)
			continue;

		if (lun_reset) {
			if (cmd->scsi_cmd && cmd->scsi_cmd->device) {
				QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_SCSI_TM,
					  "tid=0x%x itt=0x%x scsi_cmd_ptr=%p device=%p task_state=%d cmd_state=0%x cid=0x%x\n",
					  cmd->task_id, get_itt(ctask->itt),
					  cmd->scsi_cmd, cmd->scsi_cmd->device,
					  ctask->state, cmd->state,
					  qedi_conn->iscsi_conn_id);
				if (cmd->scsi_cmd->device->lun != lun)
					continue;
			}
		}
		atomic_inc(&qedi_conn->cmd_cleanup_req);
		qedi_iscsi_cleanup_task(ctask, true);

		cmd->io_cmd_in_list = false;
		list_del_init(&cmd->io_cmd);
		qedi_conn->active_cmd_count--;
		QEDI_WARN(&qedi->dbg_ctx,
			  "Deleted active cmd list node io_cmd=%p, cid=0x%x\n",
			  &cmd->io_cmd, qedi_conn->iscsi_conn_id);
	}

	spin_unlock(&qedi_conn->list_lock);

	if (lun_reset) {
	#if defined SESS_LOCK
		spin_unlock_bh(&session->back_lock);
	#else
		spin_unlock_bh(&session->lock);
	#endif
	}

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_SCSI_TM,
		  "cmd_cleanup_req=%d, cid=0x%x\n",
		  atomic_read(&qedi_conn->cmd_cleanup_req),
		  qedi_conn->iscsi_conn_id);

	rval  = wait_event_interruptible_timeout(qedi_conn->wait_queue,
						 (atomic_read(&qedi_conn->cmd_cleanup_req) ==
						 atomic_read(&qedi_conn->cmd_cleanup_cmpl)) ||
						 test_bit(QEDI_IN_RECOVERY, &qedi->flags),
						 5 * HZ);
	if (rval) {
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_SCSI_TM,
			  "i/o cmd_cleanup_req=%d, equal to cmd_cleanup_cmpl=%d, cid=0x%x\n",
			  atomic_read(&qedi_conn->cmd_cleanup_req),
			  atomic_read(&qedi_conn->cmd_cleanup_cmpl),
			  qedi_conn->iscsi_conn_id);

		return 0;
	}

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_SCSI_TM,
		  "i/o cmd_cleanup_req=%d, not equal to cmd_cleanup_cmpl=%d, cid=0x%x\n",
		  atomic_read(&qedi_conn->cmd_cleanup_req),
		  atomic_read(&qedi_conn->cmd_cleanup_cmpl),
		  qedi_conn->iscsi_conn_id);

	iscsi_host_for_each_session(qedi->shost,
				    qedi_mark_device_missing);
	qedi_ops->common->drain(qedi->cdev);

	/* Enable IOs for all other sessions except current.*/
	if (!wait_event_interruptible_timeout(qedi_conn->wait_queue,
					      (atomic_read(&qedi_conn->cmd_cleanup_req) ==
					       atomic_read(&qedi_conn->cmd_cleanup_cmpl)) ||
					       test_bit(QEDI_IN_RECOVERY, &qedi->flags),
					      5 * HZ)) {
		iscsi_host_for_each_session(qedi->shost,
					    qedi_mark_device_available);
		return -1;
	}

	iscsi_host_for_each_session(qedi->shost,
				    qedi_mark_device_available);

	return 0;
}

void qedi_clearsq(struct qedi_ctx *qedi, struct qedi_conn *qedi_conn,
		  struct iscsi_task *task)
{
	struct qedi_endpoint *qedi_ep;
	int rval;

	qedi_ep = qedi_conn->ep;
	atomic_set(&qedi_conn->cmd_cleanup_req, 0);
	atomic_set(&qedi_conn->cmd_cleanup_cmpl, 0);

	if (!qedi_ep) {
		QEDI_WARN(&qedi->dbg_ctx,
			  "Cannot proceed, ep already disconnected, cid=0x%x\n",
			  qedi_conn->iscsi_conn_id);
		return;
	}

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
		  "Clearing SQ for cid=0x%x, conn=%p, ep=%p\n",
		  qedi_conn->iscsi_conn_id, qedi_conn, qedi_ep);

	qedi_ops->clear_sq(qedi->cdev, qedi_ep->handle);

	rval = qedi_cleanup_all_io(qedi, qedi_conn, task, true);
	if (rval) {
		QEDI_ERR(&qedi->dbg_ctx,
			 "fatal error, need hard reset, cid=0x%x\n",
			 qedi_conn->iscsi_conn_id);
		WARN_ON(1);
	}
}

static int qedi_wait_for_cleanup_request(struct qedi_ctx *qedi,
					 struct qedi_conn *qedi_conn,
					 struct iscsi_task *task,
					 struct qedi_cmd *qedi_cmd,
					 struct qedi_work_map *list_work)
{
	struct qedi_cmd *cmd = (struct qedi_cmd *)task->dd_data;
	int wait;

	wait  = wait_event_interruptible_timeout(qedi_conn->wait_queue,
						 ((qedi_cmd->state ==
						   CLEANUP_RECV) ||
						 ((qedi_cmd->type == TYPEIO) &&
						  (cmd->state ==
						   RESPONSE_RECEIVED))),
						 5 * HZ);
	if (!wait) {
		qedi_cmd->state = CLEANUP_WAIT_FAILED;

		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_SCSI_TM,
			  "Cleanup timedout tid=0x%x, issue connection recovery, cid=0x%x\n",
			  cmd->task_id, qedi_conn->iscsi_conn_id);

		return -1;
	}
	return 0;
}

static void qedi_abort_work(struct work_struct *work)
{
	struct qedi_cmd *qedi_cmd =
		container_of(work, struct qedi_cmd, tmf_work);
	struct qedi_conn *qedi_conn = qedi_cmd->conn;
	struct qedi_ctx *qedi = qedi_conn->qedi;
	struct iscsi_conn *conn = qedi_conn->cls_conn->dd_data;
	struct iscsi_cls_session *cls_sess;
	struct qedi_work_map *list_work = NULL;
	struct iscsi_task *mtask;
	struct qedi_cmd *cmd;
	struct iscsi_task *ctask;
	struct iscsi_tm *tmf_hdr;
	s16 rval = 0;

	mtask = qedi_cmd->task;
	tmf_hdr = (struct iscsi_tm *)mtask->hdr;
	cls_sess = iscsi_conn_to_session(qedi_conn->cls_conn);

	#if defined SESS_LOCK
		spin_lock_bh(&conn->session->back_lock);
	#else
		spin_lock_bh(&conn->session->lock);
	#endif

	ctask = iscsi_itt_to_ctask(conn, tmf_hdr->rtt);
	if (!ctask) {
	#if defined SESS_LOCK
		spin_unlock_bh(&conn->session->back_lock);
	#else
		spin_unlock_bh(&conn->session->lock);
	#endif
		QEDI_ERR(&qedi->dbg_ctx, "Invalid RTT. Letting abort timeout.\n");
		goto clear_cleanup;
	}

	if (iscsi_task_is_completed(ctask)) {
	#if defined SESS_LOCK
		spin_unlock_bh(&conn->session->back_lock);
	#else
		spin_unlock_bh(&conn->session->lock);
	#endif
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
			  "Task already completed\n");
		/*
		 * We have to still send the TMF because libiscsi needs the
		 * response to avoid a timeout.
		 */
		goto send_tmf;
	}
	#if defined SESS_LOCK
		spin_unlock_bh(&conn->session->back_lock);
	#else
		spin_unlock_bh(&conn->session->lock);
	#endif
 
	cmd = (struct qedi_cmd *)ctask->dd_data;
	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
		  "Abort tmf rtt=0x%x, cmd itt=0x%x, cmd tid=0x%x, cid=0x%x\n",
		  get_itt(tmf_hdr->rtt), get_itt(ctask->itt), cmd->task_id,
		  qedi_conn->iscsi_conn_id);

	if (do_not_recover) {
		QEDI_ERR(&qedi->dbg_ctx, "DONT SEND CLEANUP/ABORT %d\n",
			 do_not_recover);
		goto clear_cleanup;
	}

	list_work = kzalloc(sizeof(*list_work), GFP_NOIO);
	if (!list_work) {
		QEDI_ERR(&qedi->dbg_ctx, "Memory allocation failed\n");
		goto clear_cleanup;
	}

	qedi_cmd->type = TYPEIO;
	qedi_cmd->state = CLEANUP_WAIT;
	list_work->qedi_cmd = qedi_cmd;
	list_work->rtid = cmd->task_id;
	list_work->state = QEDI_WORK_SCHEDULED;
	list_work->ctask = ctask;
	qedi_cmd->list_tmf_work = list_work;

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_SCSI_TM,
		  "Queue tmf work=%p, list node=%p, cid=0x%x, tmf flags=0x%x\n",
		  list_work->ptr_tmf_work, list_work, qedi_conn->iscsi_conn_id,
		  tmf_hdr->flags);

	spin_lock_bh(&qedi_conn->tmf_work_lock);
	list_add_tail(&list_work->list, &qedi_conn->tmf_work_list);
	spin_unlock_bh(&qedi_conn->tmf_work_lock);

	qedi_iscsi_cleanup_task(ctask, false);

	rval = qedi_wait_for_cleanup_request(qedi, qedi_conn, ctask, qedi_cmd,
					     list_work);
	if (rval == -1) {
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
			  "FW cleanup got escalated, cid=0x%x\n",
			  qedi_conn->iscsi_conn_id);
		goto ldel_exit;
	}

send_tmf:
	send_iscsi_tmf(qedi_conn, qedi_cmd->task, ctask);
	goto clear_cleanup;

ldel_exit:
	spin_lock_bh(&qedi_conn->tmf_work_lock);
	if (qedi_cmd->list_tmf_work) {
		list_del_init(&list_work->list);
		qedi_cmd->list_tmf_work = NULL;
		kfree(list_work);
	}
	spin_unlock_bh(&qedi_conn->tmf_work_lock);

	spin_lock(&qedi_conn->list_lock);
	if (likely(cmd->io_cmd_in_list)) {
		cmd->io_cmd_in_list = false;
		list_del_init(&cmd->io_cmd);
		qedi_conn->active_cmd_count--;
	}
	spin_unlock(&qedi_conn->list_lock);

clear_cleanup:
	spin_lock(&qedi_conn->tmf_work_lock);
	qedi_conn->fw_cleanup_works--;
	spin_unlock(&qedi_conn->tmf_work_lock);
}

static int send_iscsi_tmf(struct qedi_conn *qedi_conn, struct iscsi_task *mtask,
			  struct iscsi_task *ctask)
{
	struct iscsi_tmf_request_hdr tmf_pdu_header;
	struct e4_iscsi_task_params task_params;
	struct qedi_ctx *qedi = qedi_conn->qedi;
	struct iscsi_task_context *fw_task_ctx;
	struct iscsi_tm *tmf_hdr;
	struct qedi_cmd *qedi_cmd;
	struct qedi_cmd *cmd;
	struct qedi_endpoint *ep;
	u32 scsi_lun[2];
	s16 tid = 0;
	u16 sq_idx = 0;

	tmf_hdr = (struct iscsi_tm *)mtask->hdr;
	qedi_cmd = (struct qedi_cmd *)mtask->dd_data;
	ep = qedi_conn->ep;
	if (!ep)
		return -ENODEV;

	tid = qedi_get_task_idx(qedi);
	if (tid == -1)
		return -ENOMEM;

	fw_task_ctx =
	  (struct iscsi_task_context *)qedi_get_task_mem(&qedi->tasks, tid);
	memset(fw_task_ctx, 0, sizeof(struct iscsi_task_context));

	qedi_cmd->task_id = tid;

	memset(&task_params, 0, sizeof(task_params));
	memset(&tmf_pdu_header, 0, sizeof(tmf_pdu_header));

	/* Update header info */
	qedi_update_itt_map(qedi, tid, mtask->itt);
	tmf_pdu_header.itt = qedi_set_itt(tid, get_itt(mtask->itt));
	tmf_pdu_header.cmd_sn = be32_to_cpu(tmf_hdr->cmdsn);

	memcpy(scsi_lun, &tmf_hdr->lun, sizeof(struct scsi_lun));
	tmf_pdu_header.lun.lo = be32_to_cpu(scsi_lun[0]);
	tmf_pdu_header.lun.hi = be32_to_cpu(scsi_lun[1]);

	if ((tmf_hdr->flags & ISCSI_FLAG_TM_FUNC_MASK) ==
	     ISCSI_TM_FUNC_ABORT_TASK) {
		cmd = (struct qedi_cmd *)ctask->dd_data;
		tmf_pdu_header.rtt =
				qedi_set_itt(cmd->task_id,
					     get_itt(tmf_hdr->rtt));
	} else {
		tmf_pdu_header.rtt = ISCSI_RESERVED_TAG;
	}

	tmf_pdu_header.opcode = tmf_hdr->opcode;
	tmf_pdu_header.function = tmf_hdr->flags;
	tmf_pdu_header.hdr_second_dword = ntoh24(tmf_hdr->dlength);
	tmf_pdu_header.ref_cmd_sn = be32_to_cpu(tmf_hdr->refcmdsn);

	/* Fill fw input params */
	task_params.context = fw_task_ctx;
	task_params.conn_icid = (u16)qedi_conn->iscsi_conn_id;
	task_params.itid = tid;
	task_params.cq_rss_number = 0;
	task_params.tx_io_size = 0;
	task_params.rx_io_size = 0;

	sq_idx = qedi_get_wqe_idx(qedi_conn);
	task_params.sqe = &ep->sq[sq_idx];

	memset(task_params.sqe, 0, sizeof(struct iscsi_wqe));
	init_initiator_tmf_request_task(&task_params, &tmf_pdu_header);

	spin_lock(&qedi_conn->list_lock);
	list_add_tail(&qedi_cmd->io_cmd, &qedi_conn->active_cmd_list);
	qedi_cmd->io_cmd_in_list = true;
	qedi_conn->active_cmd_count++;
	spin_unlock(&qedi_conn->list_lock);

	qedi_ring_doorbell(qedi_conn);
	return 0;
}

int qedi_send_iscsi_tmf(struct qedi_conn *qedi_conn, struct iscsi_task *mtask)
{
	struct iscsi_tm *tmf_hdr = (struct iscsi_tm *)mtask->hdr;
	struct qedi_cmd *qedi_cmd = mtask->dd_data;
	struct qedi_ctx *qedi = qedi_conn->qedi;
	int rc = 0;

#ifdef ERROR_INJECT
	if (qedi->drop_abort_queue) {
		qedi->drop_abort_queue--;
		return -1;
	}
#endif
	switch (tmf_hdr->flags & ISCSI_FLAG_TM_FUNC_MASK) {
	case ISCSI_TM_FUNC_ABORT_TASK:
		spin_lock(&qedi_conn->tmf_work_lock);
		qedi_conn->fw_cleanup_works++;
		spin_unlock(&qedi_conn->tmf_work_lock);

		INIT_WORK(&qedi_cmd->tmf_work, qedi_abort_work);
		queue_work(qedi->tmf_thread, &qedi_cmd->tmf_work);
		break;
	case ISCSI_TM_FUNC_LOGICAL_UNIT_RESET:
	case ISCSI_TM_FUNC_TARGET_WARM_RESET:
	case ISCSI_TM_FUNC_TARGET_COLD_RESET:
		rc = send_iscsi_tmf(qedi_conn, mtask, NULL);
		break;
	default:
		QEDI_ERR(&qedi->dbg_ctx, "Invalid tmf, cid=0x%x\n",
			 qedi_conn->iscsi_conn_id);
		return -EINVAL;
	}

	return rc;
}

int qedi_send_iscsi_text(struct qedi_conn *qedi_conn,
			 struct iscsi_task *task)
{
	struct iscsi_text_request_hdr text_request_pdu_header;
	struct scsi_sgl_task_params tx_sgl_task_params;
	struct scsi_sgl_task_params rx_sgl_task_params;
	struct e4_iscsi_task_params task_params;
	struct iscsi_task_context *fw_task_ctx;
	struct qedi_ctx *qedi = qedi_conn->qedi;
	struct iscsi_text *text_hdr;
	struct scsi_sge *req_sge = NULL;
	struct scsi_sge *resp_sge = NULL;
	struct qedi_cmd *qedi_cmd;
	struct qedi_endpoint *ep;
	s16 tid = 0;
	u16 sq_idx = 0;
	int rval = 0;

	req_sge = (struct scsi_sge *)qedi_conn->gen_pdu.req_bd_tbl;
	resp_sge = (struct scsi_sge *)qedi_conn->gen_pdu.resp_bd_tbl;
	qedi_cmd = (struct qedi_cmd *)task->dd_data;
	text_hdr = (struct iscsi_text *)task->hdr;
	ep = qedi_conn->ep;

	tid = qedi_get_task_idx(qedi);
	if (tid == -1)
		return -ENOMEM;

	fw_task_ctx =
	  (struct iscsi_task_context *)qedi_get_task_mem(&qedi->tasks, tid);
	memset(fw_task_ctx, 0, sizeof(struct iscsi_task_context));

	qedi_cmd->task_id = tid;

	memset(&task_params, 0, sizeof(task_params));
	memset(&text_request_pdu_header, 0, sizeof(text_request_pdu_header));
	memset(&tx_sgl_task_params, 0, sizeof(tx_sgl_task_params));
	memset(&rx_sgl_task_params, 0, sizeof(rx_sgl_task_params));

	/* Update header info */
	text_request_pdu_header.opcode = text_hdr->opcode;
	text_request_pdu_header.flags_attr = text_hdr->flags;

	qedi_update_itt_map(qedi, tid, task->itt);
	text_request_pdu_header.itt = qedi_set_itt(tid, get_itt(task->itt));
	text_request_pdu_header.ttt = text_hdr->ttt;
	text_request_pdu_header.cmd_sn = be32_to_cpu(text_hdr->cmdsn);
	text_request_pdu_header.exp_stat_sn = be32_to_cpu(text_hdr->exp_statsn);
	text_request_pdu_header.hdr_second_dword = ntoh24(text_hdr->dlength);

	/* Fill tx AHS and rx buffer */
	tx_sgl_task_params.sgl = (struct scsi_sge *)qedi_conn->gen_pdu.req_bd_tbl;
	tx_sgl_task_params.sgl_phys_addr.lo = (u32)(qedi_conn->gen_pdu.req_dma_addr); 
	tx_sgl_task_params.sgl_phys_addr.hi =
				   (u32)((u64)qedi_conn->gen_pdu.req_dma_addr >> 32); 
	tx_sgl_task_params.total_buffer_size = req_sge->sge_len;
	tx_sgl_task_params.num_sges = 1;

	rx_sgl_task_params.sgl = (struct scsi_sge *)qedi_conn->gen_pdu.resp_bd_tbl;
	rx_sgl_task_params.sgl_phys_addr.lo = (u32)(qedi_conn->gen_pdu.resp_dma_addr);
	rx_sgl_task_params.sgl_phys_addr.hi =
				   (u32)((u64)qedi_conn->gen_pdu.resp_dma_addr >> 32);
	rx_sgl_task_params.total_buffer_size = resp_sge->sge_len;
	rx_sgl_task_params.num_sges = 1;

	/* Fill fw input params */
	task_params.context = fw_task_ctx;
	task_params.conn_icid = (u16)qedi_conn->iscsi_conn_id;
	task_params.itid = tid;
	task_params.cq_rss_number = 0;
	task_params.tx_io_size = ntoh24(text_hdr->dlength);
	task_params.rx_io_size = resp_sge->sge_len;

	sq_idx = qedi_get_wqe_idx(qedi_conn);
	task_params.sqe = &ep->sq[sq_idx];

	memset(task_params.sqe, 0, sizeof(struct iscsi_wqe));
	rval = init_initiator_text_request_task(&task_params,
						&text_request_pdu_header,
						&tx_sgl_task_params,
						&rx_sgl_task_params);
	if (rval)
		return -1;

	spin_lock(&qedi_conn->list_lock);
	list_add_tail(&qedi_cmd->io_cmd, &qedi_conn->active_cmd_list);
	qedi_cmd->io_cmd_in_list = true;
	qedi_conn->active_cmd_count++;
	spin_unlock(&qedi_conn->list_lock);

	qedi_ring_doorbell(qedi_conn);
	return 0;
}

int qedi_send_iscsi_nopout(struct qedi_conn *qedi_conn,
			   struct iscsi_task *task,
			   char *datap, int data_len, int unsol)
{
	struct iscsi_nop_out_hdr nop_out_pdu_header;
	struct scsi_sgl_task_params tx_sgl_task_params;
	struct scsi_sgl_task_params rx_sgl_task_params;
	struct e4_iscsi_task_params task_params;
	struct qedi_ctx *qedi = qedi_conn->qedi;
	struct iscsi_task_context *fw_task_ctx;
	struct iscsi_nopout *nopout_hdr;
	struct scsi_sge *req_sge = NULL;
	struct scsi_sge *resp_sge = NULL;
	struct qedi_cmd *qedi_cmd;
	struct qedi_endpoint *ep;
	u32 scsi_lun[2];
	s16 tid = 0;
	u16 sq_idx = 0;
	int rval = 0;

	req_sge = (struct scsi_sge *)qedi_conn->gen_pdu.req_bd_tbl;
	resp_sge = (struct scsi_sge *)qedi_conn->gen_pdu.resp_bd_tbl;
	qedi_cmd = (struct qedi_cmd *)task->dd_data;
	nopout_hdr = (struct iscsi_nopout *)task->hdr;
	ep = qedi_conn->ep;

	tid = qedi_get_task_idx(qedi);
	if (tid == -1)
		return -ENOMEM;

	fw_task_ctx =
	  (struct iscsi_task_context *)qedi_get_task_mem(&qedi->tasks, tid);
	memset(fw_task_ctx, 0, sizeof(struct iscsi_task_context));

	qedi_cmd->task_id = tid;

	memset(&task_params, 0, sizeof(task_params));
	memset(&nop_out_pdu_header, 0, sizeof(nop_out_pdu_header));
	memset(&tx_sgl_task_params, 0, sizeof(tx_sgl_task_params));
	memset(&rx_sgl_task_params, 0, sizeof(rx_sgl_task_params));

	/* Update header info */
	nop_out_pdu_header.opcode = nopout_hdr->opcode;
	SET_FIELD(nop_out_pdu_header.flags_attr, ISCSI_NOP_OUT_HDR_CONST1, 1);
	SET_FIELD(nop_out_pdu_header.flags_attr, ISCSI_NOP_OUT_HDR_RSRV, 0);

	memcpy(scsi_lun, &nopout_hdr->lun, sizeof(struct scsi_lun));
	nop_out_pdu_header.lun.lo = be32_to_cpu(scsi_lun[0]);
	nop_out_pdu_header.lun.hi = be32_to_cpu(scsi_lun[1]);
	nop_out_pdu_header.cmd_sn = be32_to_cpu(nopout_hdr->cmdsn);
	nop_out_pdu_header.exp_stat_sn = be32_to_cpu(nopout_hdr->exp_statsn);

	qedi_update_itt_map(qedi, tid, task->itt);

	if (nopout_hdr->ttt != ISCSI_TTT_ALL_ONES) {
		nop_out_pdu_header.itt = be32_to_cpu(nopout_hdr->itt);
		nop_out_pdu_header.ttt = be32_to_cpu(nopout_hdr->ttt);
	} else {
		nop_out_pdu_header.itt = qedi_set_itt(tid, get_itt(task->itt));
		nop_out_pdu_header.ttt = ISCSI_TTT_ALL_ONES;

		spin_lock(&qedi_conn->list_lock);
		list_add_tail(&qedi_cmd->io_cmd, &qedi_conn->active_cmd_list);
		qedi_cmd->io_cmd_in_list = true;
		qedi_conn->active_cmd_count++;
		spin_unlock(&qedi_conn->list_lock);
	}

	/* Fill tx AHS and rx buffer */
	if (data_len) {
		tx_sgl_task_params.sgl = (struct scsi_sge *)qedi_conn->gen_pdu.req_bd_tbl;
		tx_sgl_task_params.sgl_phys_addr.lo = (u32)(qedi_conn->gen_pdu.req_dma_addr);
		tx_sgl_task_params.sgl_phys_addr.hi =
					   (u32)((u64)qedi_conn->gen_pdu.req_dma_addr >> 32);
		tx_sgl_task_params.total_buffer_size = data_len;
		tx_sgl_task_params.num_sges = 1;

		rx_sgl_task_params.sgl = (struct scsi_sge *)qedi_conn->gen_pdu.resp_bd_tbl;
		rx_sgl_task_params.sgl_phys_addr.lo = (u32)(qedi_conn->gen_pdu.resp_dma_addr);
		rx_sgl_task_params.sgl_phys_addr.hi =
					   (u32)((u64)qedi_conn->gen_pdu.resp_dma_addr >> 32);
		rx_sgl_task_params.total_buffer_size = resp_sge->sge_len;
		rx_sgl_task_params.num_sges = 1;
	}

	/* Fill fw input params */
	task_params.context = fw_task_ctx;
	task_params.conn_icid = (u16)qedi_conn->iscsi_conn_id;
	task_params.itid = tid;
	task_params.cq_rss_number = 0;
	task_params.tx_io_size = data_len;
	task_params.rx_io_size = resp_sge->sge_len;

	sq_idx = qedi_get_wqe_idx(qedi_conn);
	task_params.sqe = &ep->sq[sq_idx];

	memset(task_params.sqe, 0, sizeof(struct iscsi_wqe));
	rval = init_initiator_nop_out_task(&task_params,
					   &nop_out_pdu_header,
					   &tx_sgl_task_params,
					   &rx_sgl_task_params);
	if (rval)
		return -1;

	qedi_ring_doorbell(qedi_conn);
	return 0;
}

static int qedi_map_scsi_sg(struct qedi_ctx *qedi, struct qedi_cmd *cmd)
{
	struct scsi_cmnd *sc = cmd->scsi_cmd;
	struct scsi_sge *bd = cmd->io_tbl.sge_tbl;
	struct scatterlist *sg;
	enum qedi_sge_type sge_type = QEDI_UNKNOWN_SGE;
	u32 byte_count = 0;
	u32 bd_count = 0;
	u32 sg_count;
	u32 sg_len;
	u64 addr;
	u32 i;

	WARN_ON(scsi_sg_count(sc) > QEDI_ISCSI_MAX_BDS_PER_CMD);

	sg_count = dma_map_sg(&qedi->pdev->dev, scsi_sglist(sc),
			      scsi_sg_count(sc), sc->sc_data_direction);

	sg = scsi_sglist(sc);

	if ((sg_count <= 8) || !(sc->sc_data_direction == DMA_TO_DEVICE))
		sge_type = QEDI_FAST_SGE;

	scsi_for_each_sg(sc, sg, sg_count, i) {
		sg_len = sg_dma_len(sg);
		addr = (u64)sg_dma_address(sg);

		if ((sge_type == QEDI_UNKNOWN_SGE) && (i) &&
		    (i != (sg_count - 1)) && (sg_len < QEDI_PAGE_SIZE)) {
			sge_type = QEDI_SLOW_SGE;
			cmd->use_slowpath = true;
		}

		bd[bd_count].sge_addr.lo = addr & 0xffffffff;
		bd[bd_count].sge_addr.hi = addr >> 32;
		bd[bd_count].sge_len = (uint32_t) sg_len;
		byte_count += sg_len;
		bd_count += 1;
	}

	if (byte_count != scsi_bufflen(sc))
		QEDI_ERR(&qedi->dbg_ctx,
			 "byte_count = %d != scsi_bufflen = %d\n", byte_count,
			 scsi_bufflen(sc));
	else
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_IO, "byte_count = %d\n",
			  byte_count);

	WARN_ON(byte_count != scsi_bufflen(sc));

	return bd_count;
}

static void qedi_iscsi_map_sg_list(struct qedi_cmd *cmd)
{
	u32 bd_count;
	struct scsi_cmnd *sc = cmd->scsi_cmd;

	if (scsi_sg_count(sc)) {
		bd_count  = qedi_map_scsi_sg(cmd->conn->qedi, cmd);
		if (bd_count == 0)
			return;
	} else {
		struct scsi_sge *bd = cmd->io_tbl.sge_tbl;

		bd[0].sge_addr.lo = 0;
		bd[0].sge_addr.hi = 0;
		bd[0].sge_len = 0;
		bd_count = 0;
	}
	cmd->io_tbl.sge_valid = bd_count;
}

static void qedi_cpy_scsi_cdb(struct scsi_cmnd *sc, u32 *dstp)
{
	u32 dword;
	u32 lpcnt;
	u8 *srcp;

	lpcnt = sc->cmd_len / sizeof(dword);
	srcp = (u8 *)sc->cmnd;
	while (lpcnt--) {
		memcpy(&dword, (const void *)srcp, 4);
		*dstp = cpu_to_be32(dword);
		srcp += 4;
		dstp++;
	}
	if (sc->cmd_len & 0x3) {
		dword = (u32)srcp[0] | ((u32)srcp[1] << 8);
		*dstp = cpu_to_be32(dword);
	}
}

void qedi_trace_io(struct qedi_ctx *qedi, struct iscsi_task *task,
		   u16 tid, int8_t direction)
{
	struct qedi_io_log *io_log;
	struct iscsi_conn *conn = task->conn;
	struct qedi_conn *qedi_conn = conn->dd_data;
	struct scsi_cmnd *sc_cmd = task->sc;
	unsigned long flags;
	u8 op;

	spin_lock_irqsave(&qedi->io_trace_lock, flags);

	io_log = &qedi->io_trace_buf[qedi->io_trace_idx];
	io_log->direction = direction;
	io_log->task_id = tid;
	io_log->cid = qedi_conn->iscsi_conn_id;
	io_log->lun = sc_cmd->device->lun;
	io_log->op = sc_cmd->cmnd[0];
	op = sc_cmd->cmnd[0];

	if (op == READ_10 || op == WRITE_10) {
		io_log->lba[0] = sc_cmd->cmnd[2];
		io_log->lba[1] = sc_cmd->cmnd[3];
		io_log->lba[2] = sc_cmd->cmnd[4];
		io_log->lba[3] = sc_cmd->cmnd[5];
	} else {
		io_log->lba[0] = 0;
		io_log->lba[1] = 0;
		io_log->lba[2] = 0;
		io_log->lba[3] = 0;
	}
	io_log->bufflen = scsi_bufflen(sc_cmd);
	io_log->sg_count = scsi_sg_count(sc_cmd);
	io_log->fast_sgs = qedi->fast_sgls;
	io_log->cached_sgs = qedi->cached_sgls;
	io_log->slow_sgs = qedi->slow_sgls;
	io_log->cached_sge = qedi->use_cached_sge;
	io_log->slow_sge = qedi->use_slow_sge;
	io_log->fast_sge = qedi->use_fast_sge;
	io_log->result = sc_cmd->result;
	io_log->jiffies = jiffies;
	io_log->blk_req_cpu = smp_processor_id();

	if (direction == QEDI_IO_TRACE_REQ) {
		/* For requests we only care about the submission CPU */
		io_log->req_cpu = smp_processor_id() % qedi->num_queues;
		io_log->intr_cpu = 0;
		io_log->blk_rsp_cpu = 0;
	} else if (direction == QEDI_IO_TRACE_RSP) {
		io_log->req_cpu = smp_processor_id() % qedi->num_queues;
		io_log->intr_cpu = qedi->intr_cpu;
		io_log->blk_rsp_cpu = smp_processor_id();
	}

	qedi->io_trace_idx++;
	if (qedi->io_trace_idx == QEDI_IO_TRACE_SIZE)
		qedi->io_trace_idx = 0;

	qedi->use_cached_sge = false;
	qedi->use_slow_sge = false;
	qedi->use_fast_sge = false;

	spin_unlock_irqrestore(&qedi->io_trace_lock, flags);
}

int qedi_iscsi_send_ioreq(struct iscsi_task *task)
{
	struct iscsi_conn *conn = task->conn;
	struct iscsi_session *session = conn->session;
	struct Scsi_Host *shost = iscsi_session_to_shost(session->cls_session);
	struct qedi_ctx *qedi = iscsi_host_priv(shost);
	struct qedi_conn *qedi_conn = conn->dd_data;
	struct qedi_cmd *cmd = task->dd_data;
	struct scsi_cmnd *sc = task->sc;
	struct iscsi_cmd_hdr cmd_pdu_header;
	struct scsi_sgl_task_params tx_sgl_task_params;
	struct scsi_sgl_task_params rx_sgl_task_params;
	struct scsi_sgl_task_params *prx_sgl = NULL;
	struct scsi_sgl_task_params *ptx_sgl = NULL;
	struct e4_iscsi_task_params task_params;
	struct iscsi_conn_params conn_params;
	struct scsi_initiator_cmd_params cmd_params;
	struct iscsi_task_context *fw_task_ctx;
	struct iscsi_cls_conn *cls_conn;
#if defined STRUCT_SCSI_REQ
	struct iscsi_scsi_req *hdr = (struct iscsi_scsi_req *)task->hdr;
#else
	struct iscsi_cmd *hdr = (struct iscsi_cmd *)task->hdr;
#endif
	enum iscsi_task_type task_type = MAX_ISCSI_TASK_TYPE;
	struct qedi_endpoint *ep;
	u32 scsi_lun[2];
	s16 tid = 0;
	u16 sq_idx = 0;
	u16 cq_idx;
	int rval = 0;

	ep = qedi_conn->ep;
	cls_conn = qedi_conn->cls_conn;
	conn = cls_conn->dd_data;

	qedi_iscsi_map_sg_list(cmd);
	int_to_scsilun(sc->device->lun, (struct scsi_lun *)scsi_lun);

	tid = qedi_get_task_idx(qedi);
	if (tid == -1)
		return -ENOMEM;

	fw_task_ctx =
	  (struct iscsi_task_context *)qedi_get_task_mem(&qedi->tasks, tid);
	memset(fw_task_ctx, 0, sizeof(struct iscsi_task_context));

	cmd->task_id = tid;

	memset(&task_params, 0, sizeof(task_params));
	memset(&cmd_pdu_header, 0, sizeof(cmd_pdu_header));
	memset(&tx_sgl_task_params, 0, sizeof(tx_sgl_task_params));
	memset(&rx_sgl_task_params, 0, sizeof(rx_sgl_task_params));
	memset(&conn_params, 0, sizeof(conn_params));
	memset(&cmd_params, 0, sizeof(cmd_params));

	cq_idx = smp_processor_id() % qedi->num_queues;
	/* Update header info */
	SET_FIELD(cmd_pdu_header.flags_attr, ISCSI_CMD_HDR_ATTR,
		  ISCSI_ATTR_SIMPLE);
	if (hdr->cdb[0] != TEST_UNIT_READY) {
		if (sc->sc_data_direction == DMA_TO_DEVICE) {
			SET_FIELD(cmd_pdu_header.flags_attr,
				  ISCSI_CMD_HDR_WRITE, 1);
			task_type = ISCSI_TASK_TYPE_INITIATOR_WRITE;
		} else {
			SET_FIELD(cmd_pdu_header.flags_attr,
				  ISCSI_CMD_HDR_READ, 1);
			task_type = ISCSI_TASK_TYPE_INITIATOR_READ;
		}
	}

	cmd_pdu_header.lun.lo = be32_to_cpu(scsi_lun[0]);
	cmd_pdu_header.lun.hi = be32_to_cpu(scsi_lun[1]);

	qedi_update_itt_map(qedi, tid, task->itt);
	cmd_pdu_header.itt = qedi_set_itt(tid, get_itt(task->itt));
	cmd_pdu_header.expected_transfer_length = cpu_to_be32(hdr->data_length);
	cmd_pdu_header.hdr_second_dword = ntoh24(hdr->dlength);
	cmd_pdu_header.cmd_sn = be32_to_cpu(hdr->cmdsn);
	cmd_pdu_header.hdr_first_byte = hdr->opcode;
	qedi_cpy_scsi_cdb(sc, (u32 *)cmd_pdu_header.cdb);

	/* Fill tx AHS and rx buffer */
	if (task_type == ISCSI_TASK_TYPE_INITIATOR_WRITE) {
		tx_sgl_task_params.sgl = cmd->io_tbl.sge_tbl;
		tx_sgl_task_params.sgl_phys_addr.lo = (u32)(cmd->io_tbl.sge_tbl_dma);
		tx_sgl_task_params.sgl_phys_addr.hi =
					   (u32)((u64)cmd->io_tbl.sge_tbl_dma >> 32);
		tx_sgl_task_params.total_buffer_size = scsi_bufflen(sc);
		tx_sgl_task_params.num_sges = cmd->io_tbl.sge_valid;
		if (cmd->use_slowpath == true)
			tx_sgl_task_params.small_mid_sge = true;
	} else if (task_type == ISCSI_TASK_TYPE_INITIATOR_READ) {
		rx_sgl_task_params.sgl = cmd->io_tbl.sge_tbl;
		rx_sgl_task_params.sgl_phys_addr.lo = (u32)(cmd->io_tbl.sge_tbl_dma);
		rx_sgl_task_params.sgl_phys_addr.hi =
					   (u32)((u64)cmd->io_tbl.sge_tbl_dma >> 32);
		rx_sgl_task_params.total_buffer_size = scsi_bufflen(sc);
		rx_sgl_task_params.num_sges = cmd->io_tbl.sge_valid;
	}

	/* Add conn param */
	conn_params.first_burst_length = conn->session->first_burst;
	conn_params.max_send_pdu_length = conn->max_xmit_dlength;
	conn_params.max_burst_length = conn->session->max_burst;
	if (conn->session->initial_r2t_en)
		conn_params.initial_r2t = true;
	if (conn->session->imm_data_en)
		conn_params.immediate_data = true;

	/* Add cmd params */
	cmd_params.sense_data_buffer_phys_addr.lo = (u32)cmd->sense_buffer_dma;
	cmd_params.sense_data_buffer_phys_addr.hi =
					(u32)((u64)cmd->sense_buffer_dma >> 32);
	/* Fill fw input params */
	task_params.context = fw_task_ctx;
	task_params.conn_icid = (u16)qedi_conn->iscsi_conn_id;
	task_params.itid = tid;
	task_params.cq_rss_number = cq_idx;
	if (task_type == ISCSI_TASK_TYPE_INITIATOR_WRITE)
		task_params.tx_io_size = scsi_bufflen(sc);
	else if (task_type == ISCSI_TASK_TYPE_INITIATOR_READ)
		task_params.rx_io_size = scsi_bufflen(sc);
	
	sq_idx = qedi_get_wqe_idx(qedi_conn);
	task_params.sqe = &ep->sq[sq_idx];

	QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_IO,
		  "%s: %s-SGL: sg_len=0x%x num_sges=0x%x first-sge-lo=0x%x first-sge-hi=0x%x",
		  (task_type == ISCSI_TASK_TYPE_INITIATOR_WRITE) ?
		  "Write " : "Read ", (cmd->io_tbl.sge_valid == 1) ?
		  "Single" : (cmd->use_slowpath ? "SLOW" : "FAST"),
		  (u16)cmd->io_tbl.sge_valid, scsi_bufflen(sc),
		  (u32)(cmd->io_tbl.sge_tbl_dma),
		  (u32)((u64)cmd->io_tbl.sge_tbl_dma >> 32));

	memset(task_params.sqe, 0, sizeof(struct iscsi_wqe));

	if (task_params.tx_io_size != 0)
		ptx_sgl = &tx_sgl_task_params;
	if (task_params.rx_io_size != 0)
		prx_sgl = &rx_sgl_task_params;

	rval = init_initiator_rw_iscsi_task(&task_params, &conn_params,
					    &cmd_params, &cmd_pdu_header,
					    ptx_sgl, prx_sgl,
					    NULL);
	if (rval)
		return -1;

	spin_lock(&qedi_conn->list_lock);
	list_add_tail(&cmd->io_cmd, &qedi_conn->active_cmd_list);
	cmd->io_cmd_in_list = true;
	qedi_conn->active_cmd_count++;
	spin_unlock(&qedi_conn->list_lock);

	qedi_ring_doorbell(qedi_conn);
	return 0;
}

int qedi_iscsi_cleanup_task(struct iscsi_task *task, bool mark_cmd_node_deleted)
{
	struct e4_iscsi_task_params task_params;
	struct qedi_endpoint *ep;
	struct iscsi_conn *conn = task->conn;
	struct qedi_conn *qedi_conn = conn->dd_data;
	struct qedi_cmd *cmd = task->dd_data;
	u16 sq_idx = 0;
	int rval = 0;

	QEDI_INFO(&qedi_conn->qedi->dbg_ctx, QEDI_LOG_SCSI_TM,
		  "issue cleanup tid=0x%x itt=0x%x task_state=%d cmd_state=0%x cid=0x%x\n",
		  cmd->task_id, get_itt(task->itt), task->state,
		  cmd->state, qedi_conn->iscsi_conn_id);

	memset(&task_params, 0, sizeof(task_params));
	ep = qedi_conn->ep;

	sq_idx = qedi_get_wqe_idx(qedi_conn);

	task_params.sqe = &ep->sq[sq_idx];
	memset(task_params.sqe, 0, sizeof(struct iscsi_wqe));
	task_params.itid = cmd->task_id;

	rval = init_cleanup_task(&task_params);
	if (rval)
		return rval;

	qedi_ring_doorbell(qedi_conn);
	return 0;
}

