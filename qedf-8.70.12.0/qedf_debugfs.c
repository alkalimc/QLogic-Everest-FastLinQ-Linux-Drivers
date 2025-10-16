/*
 *  QLogic FCoE Offload Driver
 *  Copyright (c) 2015-2018 Cavium Inc.
 *
 *  See LICENSE.qedf for copyright and licensing details.
 */

#ifdef CONFIG_DEBUG_FS

#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/module.h>

#include "qedf.h"
#include "qedf_dbg.h"

static struct dentry *qedf_dbg_root;

/**
 * qedf_dbg_host_init - setup the debugfs file for the pf
 * @pf: the pf that is starting up
 **/
void
qedf_dbg_host_init(struct qedf_dbg_ctx *qedf_dbg,
		    const struct qedf_debugfs_ops *dops,
		    const struct file_operations *fops)
{
	char host_dirname[32];
	struct dentry *file_dentry = NULL;

	QEDF_INFO(qedf_dbg, QEDF_LOG_DEBUGFS, "Creating debugfs host node\n");
	/* create pf dir */
	sprintf(host_dirname, "host%u", qedf_dbg->host_no);
	qedf_dbg->bdf_dentry = debugfs_create_dir(host_dirname, qedf_dbg_root);
	if (!qedf_dbg->bdf_dentry) {
		QEDF_ERR(qedf_dbg, "bdf_dentry is NULL\n");
		return;
	}

	/* create debugfs files */
	while (dops) {
		if (!(dops->name))
			break;

		file_dentry = debugfs_create_file(dops->name, 0600,
						  qedf_dbg->bdf_dentry, qedf_dbg,
						  fops);
		if (!file_dentry) {
			QEDF_INFO(qedf_dbg, QEDF_LOG_DEBUGFS,
				   "Debugfs entry %s creation failed\n",
				   dops->name);
			debugfs_remove_recursive(qedf_dbg->bdf_dentry);
			return;
		}
		dops++;
		fops++;
	}
}

/**
 * qedf_dbg_host_exit - clear out the pf's debugfs entries
 * @pf: the pf that is stopping
 **/
void
qedf_dbg_host_exit(struct qedf_dbg_ctx *qedf_dbg)
{
	QEDF_INFO(qedf_dbg, QEDF_LOG_DEBUGFS, "Destroying debugfs host "
		   "entry\n");
	/* remove debugfs  entries of this PF */
	debugfs_remove_recursive(qedf_dbg->bdf_dentry);
	qedf_dbg->bdf_dentry = NULL;
}

/**
 * qedf_dbg_init - start up debugfs for the driver
 **/
void
qedf_dbg_init(char *drv_name)
{
	QEDF_INFO(NULL, QEDF_LOG_DEBUGFS, "Creating debugfs root node\n");

	/* create qed dir in root of debugfs. NULL means debugfs root */
	qedf_dbg_root = debugfs_create_dir(drv_name, NULL);
	if (!qedf_dbg_root)
		QEDF_INFO(NULL, QEDF_LOG_DEBUGFS, "Init of debugfs "
			   "failed\n");
}

/**
 * qedf_dbg_exit - clean out the driver's debugfs entries
 **/
void
qedf_dbg_exit(void)
{
	QEDF_INFO(NULL, QEDF_LOG_DEBUGFS, "Destroying debugfs root "
		   "entry\n");

	/* remove qed dir in root of debugfs */
	debugfs_remove_recursive(qedf_dbg_root);
	qedf_dbg_root = NULL;
}

const struct qedf_debugfs_ops qedf_debugfs_ops[] = {
	{ "fp_int", NULL },
	{ "io_trace", NULL },
	{ "debug", NULL },
	{ "stop_io_on_error", NULL},
	{ "driver_stats", NULL},
	{ "clear_stats", NULL},
	{ "offload_stats", NULL},
#ifdef ERROR_INJECT
	{ "error_inject", NULL},
#endif
	{ "dcbx_info", NULL},
	/* This must be last */
	{ NULL, NULL }
};

DECLARE_PER_CPU(struct qedf_percpu_iothread_s, qedf_percpu_iothreads);

static int
qedf_dcbx_info_show(struct seq_file *s, void *unused)
{
	char *dcbx_state = NULL;
	struct qedf_ctx *qedf = s->private;


	QEDF_INFO(&qedf->dbg_ctx, QEDF_LOG_DEBUGFS, "%s entered\n", __func__);

	if (atomic_read(&qedf->dcbx) == QEDF_DCBX_DONE) {
		dcbx_state = "Converged";
		seq_printf(s, "DCBx State: %s(%d) ", dcbx_state, qedf->dcbx_session);
		seq_printf(s, "PFC %d ", qedf->pfc);
		seq_printf(s, "Prio %d(%d) ", qedf->prio, qedf->vprio);
		seq_printf(s, "\n");

	} else {
		dcbx_state = "Not Converged";
		seq_printf(s, "DCBx State: %s(%d) ", dcbx_state, qedf->dcbx_session);
		seq_printf(s, "\n");
	}


	return 0;

}

static int
qedf_dbg_dcbx_info_open(struct inode *inode, struct file *file)
{
	struct qedf_dbg_ctx *qedf_dbg = inode->i_private;
	struct qedf_ctx *qedf = container_of(qedf_dbg,
	    struct qedf_ctx, dbg_ctx);

	return single_open(file, qedf_dcbx_info_show, qedf);
}

static int
qedf_fp_int_show(struct seq_file *s, void *unused)
{
	uint32_t id;
	struct qedf_fastpath *fp = NULL;
	struct qedf_ctx *qedf = s->private;

	QEDF_INFO(&qedf->dbg_ctx, QEDF_LOG_DEBUGFS, "%s entered\n", __func__);

	seq_printf(s, "\nFastpath I/O completions\n\n");

	for (id = 0; id < qedf->num_queues; id++) {
		fp = &(qedf->fp_array[id]);
		if (fp->sb_id == QEDF_SB_ID_NULL)
			continue;
		seq_printf(s, "#%d: %lu\n", id,
			       fp->completions);
	}

	return 0;
}

static int
qedf_dbg_fp_int_open(struct inode *inode, struct file *file)
{
	struct qedf_dbg_ctx *qedf_dbg = inode->i_private;
	struct qedf_ctx *qedf = container_of(qedf_dbg,
	struct qedf_ctx, dbg_ctx);

	return single_open(file, qedf_fp_int_show, qedf);
}

static int
qedf_debug_show(struct seq_file *s, void *unused)
{

	struct qedf_ctx *qedf = s->private;

	QEDF_INFO(&qedf->dbg_ctx, QEDF_LOG_DEBUGFS, "%s entered\n", __func__);
	seq_printf(s, "debug mask = 0x%x\n", qedf_debug);
	seq_printf(s, "\n");

	return 0;
}

static int
qedf_dbg_debug_open(struct inode *inode, struct file *file)
{
	struct qedf_dbg_ctx *qedf_dbg = inode->i_private;
	struct qedf_ctx *qedf = container_of(qedf_dbg,
	struct qedf_ctx, dbg_ctx);

	return single_open(file, qedf_debug_show, qedf);
}

static ssize_t
qedf_dbg_stop_io_on_error_cmd_read(struct file *filp, char __user *buffer,
				   size_t count, loff_t *ppos)
{
	int cnt;
	char *True = "true\n";
	char *False = "false\n";
	struct qedf_dbg_ctx *qedf_dbg =
				(struct qedf_dbg_ctx *)filp->private_data;
	struct qedf_ctx *qedf = container_of(qedf_dbg,
	    struct qedf_ctx, dbg_ctx);

	QEDF_INFO(qedf_dbg, QEDF_LOG_DEBUGFS, "entered\n");

	if(qedf->stop_io_on_error)
		for(cnt = 0; cnt < sizeof(True); cnt++)
			put_user(*(True++), buffer++);
	else
		for(cnt = 0; cnt < sizeof(False); cnt++)
			put_user(*(False++), buffer++);

	cnt--;
	cnt = min_t(int, count, cnt - *ppos);
	*ppos += cnt;
	return cnt;
}

static ssize_t
qedf_dbg_stop_io_on_error_cmd_write(struct file *filp,
				    const char __user *buffer, size_t count,
				    loff_t *ppos)
{
	void *kern_buf;
	struct qedf_dbg_ctx *qedf_dbg =
				(struct qedf_dbg_ctx *)filp->private_data;
	struct qedf_ctx *qedf = container_of(qedf_dbg, struct qedf_ctx, dbg_ctx);

	QEDF_INFO(qedf_dbg, QEDF_LOG_DEBUGFS, "entered\n");

	if (!count || *ppos)
		return 0;

	kern_buf = memdup_user(buffer, 6);
	if (IS_ERR(kern_buf))
		return PTR_ERR(kern_buf);

	if (strncmp(kern_buf, "false", 5) == 0)
		qedf->stop_io_on_error = false;
	else if (strncmp(kern_buf, "true", 4) == 0)
		qedf->stop_io_on_error = true;
	else if (strncmp(kern_buf, "now", 3) == 0)
		/* Trigger from user to stop all I/O on this host */
		set_bit(QEDF_DBG_STOP_IO, &qedf->flags);

	kfree(kern_buf);
	return count;
}

static int
qedf_io_trace_show(struct seq_file *s, void *unused)
{
	uint32_t i, idx = 0;
	struct qedf_ctx *qedf = s->private;
	struct qedf_dbg_ctx *qedf_dbg = &qedf->dbg_ctx;
	struct qedf_io_log *io_log;
	unsigned long flags;

	if (!qedf_io_tracing) {
		seq_puts(s, "I/O tracing not enabled.\n");
		goto out;
	}

	QEDF_INFO(qedf_dbg, QEDF_LOG_DEBUGFS, "entered\n");

	spin_lock_irqsave(&qedf->io_trace_lock, flags);
	idx = qedf->io_trace_idx;
	for (i = 0; i < QEDF_IO_TRACE_SIZE; i++) {
		io_log = &qedf->io_trace_buf[idx];
		seq_printf(s, "%d:", io_log->direction);
		seq_printf(s, "0x%x:", io_log->task_id);
		seq_printf(s, "0x%06x:", io_log->port_id);
		seq_printf(s, "%d:", io_log->lun);
		seq_printf(s, "0x%02x:", io_log->op);
		seq_printf(s, "0x%02x%02x%02x%02x:", io_log->lba[0],
		    io_log->lba[1], io_log->lba[2], io_log->lba[3]);
		seq_printf(s, "%d:", io_log->bufflen);
		seq_printf(s, "%d:", io_log->sg_count);
		seq_printf(s, "0x%08x:", io_log->result);
		seq_printf(s, "%lu:", io_log->jiffies);
		seq_printf(s, "%d:", io_log->refcount);
		seq_printf(s, "%d:", io_log->req_cpu);
		seq_printf(s, "%d:", io_log->int_cpu);
		seq_printf(s, "%d:", io_log->rsp_cpu);
		seq_printf(s, "%d\n", io_log->sge_type);

		idx++;
		if (idx == QEDF_IO_TRACE_SIZE)
			idx = 0;
	}
	spin_unlock_irqrestore(&qedf->io_trace_lock, flags);

out:
	return 0;
}

static int
qedf_dbg_io_trace_open(struct inode *inode, struct file *file)
{
	struct qedf_dbg_ctx *qedf_dbg = inode->i_private;
	struct qedf_ctx *qedf = container_of(qedf_dbg,
	    struct qedf_ctx, dbg_ctx);

	return single_open(file, qedf_io_trace_show, qedf);
}

/* Based on fip_state enum from libfcoe.h */
static char *fip_state_names[] = {
	"FIP_ST_DISABLED",
	"FIP_ST_LINK_WAIT",
	"FIP_ST_AUTO",
	"FIP_ST_NON_FIP",
	"FIP_ST_ENABLED",
	"FIP_ST_VNMP_START",
	"FIP_ST_VNMP_PROBE1",
	"FIP_ST_VNMP_PROBE2",
	"FIP_ST_VNMP_CLAIM",
	"FIP_ST_VNMP_UP",
};

/* Based on fc_rport_state enum from libfc.h */
static char *fc_rport_state_names[] = {
	"RPORT_ST_INIT",
	"RPORT_ST_FLOGI",
	"RPORT_ST_PLOGI_WAIT",
	"RPORT_ST_PLOGI",
	"RPORT_ST_PRLI",
	"RPORT_ST_RTV",
	"RPORT_ST_READY",
	"RPORT_ST_ADISC",
	"RPORT_ST_DELETE",
};

static int
qedf_driver_stats_show(struct seq_file *s, void *unused)
{
	struct qedf_ctx *qedf = s->private;
	struct qedf_rport *fcport;
	struct fc_rport_priv *rdata;

	seq_printf(s, "Host WWNN/WWPN: %016llx/%016llx\n",
	    qedf->wwnn, qedf->wwpn);
	seq_printf(s, "Host NPortID: %06x\n", qedf->lport->port_id);
	seq_printf(s, "Link State: %s\n", atomic_read(&qedf->link_state) ?
	    "Up" : "Down");
	seq_printf(s, "Logical Link State: %s\n", qedf->lport->link_up ?
	    "Up" : "Down");
	seq_printf(s, "FIP state: %s\n", fip_state_names[qedf->ctlr.state]);
	seq_printf(s, "FIP VLAN ID: %d\n", qedf->vlan_id & 0xfff);
	seq_printf(s, "FIP 802.1Q Priority: %d\n", qedf->prio);
	if (qedf->ctlr.sel_fcf) {
		seq_printf(s, "FCF WWPN: %016llx\n",
		    qedf->ctlr.sel_fcf->switch_name);
		seq_printf(s, "FCF MAC: %pM\n", qedf->ctlr.sel_fcf->fcf_mac);
	} else
		seq_printf(s, "FCF not selected\n");

	seq_printf(s, "\nSGE stats:\n\n");
	seq_printf(s, "cmg_mgr free io_reqs: %d\n",
	    atomic_read(&qedf->cmd_mgr->free_list_cnt));
	seq_printf(s, "slow SGEs: %d\n", qedf->slow_sge_ios);
	seq_printf(s, "fast SGEs: %d\n\n", qedf->fast_sge_ios);

	seq_puts(s, "Offloaded ports:\n\n");

	rcu_read_lock();
	list_for_each_entry_rcu(fcport, &qedf->fcports, peers) {
		rdata = fcport->rdata;
		if (rdata == NULL)
			continue;
		seq_printf(s, "%016llx/%016llx/%06x: state=%s, free_sqes=%d, num_active_ios=%d\n",
		    rdata->rport->node_name, rdata->rport->port_name,
		    rdata->ids.port_id, fc_rport_state_names[rdata->rp_state],
		    atomic_read(&fcport->free_sqes),
		    atomic_read(&fcport->num_active_ios));
	}
	rcu_read_unlock();

	return 0;
}

static int
qedf_dbg_driver_stats_open(struct inode *inode, struct file *file)
{
	struct qedf_dbg_ctx *qedf_dbg = inode->i_private;
	struct qedf_ctx *qedf = container_of(qedf_dbg,
	    struct qedf_ctx, dbg_ctx);

	return single_open(file, qedf_driver_stats_show, qedf);
}

static ssize_t
qedf_dbg_clear_stats_cmd_read(struct file *filp, char __user *buffer,
				   size_t count, loff_t *ppos)
{
	int cnt = 0;

	/* Essentially a read stub */
	cnt = min_t(int, count, cnt - *ppos);
	*ppos += cnt;
	return cnt;
}

static ssize_t
qedf_dbg_clear_stats_cmd_write(struct file *filp,
				    const char __user *buffer, size_t count,
				    loff_t *ppos)
{
	struct qedf_dbg_ctx *qedf_dbg =
				(struct qedf_dbg_ctx *)filp->private_data;
	struct qedf_ctx *qedf = container_of(qedf_dbg, struct qedf_ctx, dbg_ctx);

	QEDF_INFO(qedf_dbg, QEDF_LOG_DEBUGFS, "Clearing stat counters.\n");

	if (!count || *ppos)
		return 0;

	/* Clear stat counters exposed by 'stats' node */
	qedf->slow_sge_ios = 0;
	qedf->fast_sge_ios = 0;

	return count;
}

static int
qedf_offload_stats_show(struct seq_file *s, void *unused)
{
	struct qedf_ctx *qedf = s->private;
	struct qed_fcoe_stats *fw_fcoe_stats;

	fw_fcoe_stats = kmalloc(sizeof(struct qed_fcoe_stats), GFP_KERNEL);
	if (!fw_fcoe_stats) {
		QEDF_ERR(&(qedf->dbg_ctx), "Could not allocate memory for "
		    "fw_fcoe_stats.\n");
		goto out;
	}

	/* Query firmware for offload stats */
	qed_ops->get_stats(qedf->cdev, fw_fcoe_stats);

	seq_printf(s, "fcoe_rx_byte_cnt=%llu\n"
	    "fcoe_rx_data_pkt_cnt=%llu\n"
	    "fcoe_rx_xfer_pkt_cnt=%llu\n"
	    "fcoe_rx_other_pkt_cnt=%llu\n"
	    "fcoe_silent_drop_pkt_cmdq_full_cnt=%u\n"
	    "fcoe_silent_drop_pkt_crc_error_cnt=%u\n"
	    "fcoe_silent_drop_pkt_task_invalid_cnt=%u\n"
	    "fcoe_silent_drop_total_pkt_cnt=%u\n"
	    "fcoe_silent_drop_pkt_rq_full_cnt=%u\n"
	    "fcoe_tx_byte_cnt=%llu\n"
	    "fcoe_tx_data_pkt_cnt=%llu\n"
	    "fcoe_tx_xfer_pkt_cnt=%llu\n"
	    "fcoe_tx_other_pkt_cnt=%llu\n",
	    fw_fcoe_stats->fcoe_rx_byte_cnt,
	    fw_fcoe_stats->fcoe_rx_data_pkt_cnt,
	    fw_fcoe_stats->fcoe_rx_xfer_pkt_cnt,
	    fw_fcoe_stats->fcoe_rx_other_pkt_cnt,
	    fw_fcoe_stats->fcoe_silent_drop_pkt_cmdq_full_cnt,
	    fw_fcoe_stats->fcoe_silent_drop_pkt_crc_error_cnt,
	    fw_fcoe_stats->fcoe_silent_drop_pkt_task_invalid_cnt,
	    fw_fcoe_stats->fcoe_silent_drop_total_pkt_cnt,
	    fw_fcoe_stats->fcoe_silent_drop_pkt_rq_full_cnt,
	    fw_fcoe_stats->fcoe_tx_byte_cnt,
	    fw_fcoe_stats->fcoe_tx_data_pkt_cnt,
	    fw_fcoe_stats->fcoe_tx_xfer_pkt_cnt,
	    fw_fcoe_stats->fcoe_tx_other_pkt_cnt);

	kfree(fw_fcoe_stats);
out:
	return 0;
}

static int
qedf_dbg_offload_stats_open(struct inode *inode, struct file *file)
{
	struct qedf_dbg_ctx *qedf_dbg = inode->i_private;
	struct qedf_ctx *qedf = container_of(qedf_dbg,
	    struct qedf_ctx, dbg_ctx);

	return single_open(file, qedf_offload_stats_show, qedf);
}

#ifdef ERROR_INJECT
static ssize_t
qedf_dbg_error_inject_cmd_read(struct file *filp, char __user *buffer,
				   size_t count, loff_t *ppos)
{
	int cnt = 0;
	struct qedf_dbg_ctx *qedf_dbg =
				(struct qedf_dbg_ctx *)filp->private_data;

	QEDF_INFO(qedf_dbg, QEDF_LOG_DEBUGFS, "entered\n");

	cnt = sprintf(buffer, "Usage: echo \"command X\" > error_inject\n\n");
	cnt += sprintf(buffer + cnt, "drop_cmd X: Drops X SCSI completions to "
	    "force an abort of the commands.\n");
	cnt += sprintf(buffer + cnt, "drop_cleanup X: Drops a X number of "
	    "cleanup completions to force cleanup timeouts.\n");
	cnt += sprintf(buffer + cnt, "drop_els X: Drops X number of middlepath"
	    " ELS requests for force request timeouts.\n");
	cnt += sprintf(buffer + cnt, "drop_abts X: Drops X number of ABTS "
	    "completions to force ABTS timeouts.\n");
	cnt += sprintf(buffer + cnt, "drop_abts_queue X: Forces X number of "
	    "ABTS requests to not be queued and fail.\n");
	cnt += sprintf(buffer + cnt, "error_detect X: Forces X number of "
	    "I/O completions to be error_detect completions.\n");
	cnt += sprintf(buffer + cnt, "adisc FID: Sends an ADISC to the fabric "
	    "ID in FID.  Note the arg value needs to be in decimal.\n");
	cnt += sprintf(buffer + cnt, "link_down 0: Simulates queuing a link "
	    "down event.\n");
	cnt += sprintf(buffer + cnt, "link_up 0: Simulates queuing a link up "
	    "event.\n");
	cnt += sprintf(buffer + cnt, "recovery 0: Simulates qed calling our "
	    "schedule_recovery handler.\n");
	cnt += sprintf(buffer + cnt, "fan 0: Simulates qed calling our "
	    "hardware error handler for a fan failure event.  Note this will "
	    "disable the PCI function.\n");
	cnt += sprintf(buffer + cnt, "fcp_prot_error X: Simulates a FCP "
	    "protocol error.\n");
	cnt += sprintf(buffer + cnt, "underrun_error X: Simulates a firmware "
	    "detected underrun condition.\n");
	cnt += sprintf(buffer + cnt, "tlv_data 0: Calls the driver\'s "
	    "protocol TLV handler.\n");
	cnt += sprintf(buffer + cnt, "abts_acc X: Forces an accept in an ABTS "
	    "response.\n");
	cnt += sprintf(buffer + cnt, "hw_attr_update 0: Simulates a hardware "
	    "update callback when the STAG changes.\n");
	cnt += sprintf(buffer + cnt, "stag_null_cookie 0: Simulates passing a "
	    "NULL cookie to the hw_attr_update handler in qedf.\n");
	cnt += sprintf(buffer + cnt, "restart_rport 0: Logs out and back in "
	    "a specific rport similar to an ELS timeout.\n");
	cnt += sprintf(buffer + cnt, "generic_tlv_data 0: Calls the driver\'s "
	    "get_generic_tlv_data callback.\n");
	cnt += sprintf(buffer + cnt, "drop_tmf X: Simulates a task management "
	    "response being dropped.\n");
	cnt += sprintf(buffer + cnt, "no_abts_tmo X: Simulate a timeout not "
	    "happening for an ABTS request.\n");
	cnt += sprintf(buffer + cnt, "fail_post_io_rq X: Simulate a failure "
	    "to post an io_req in queuecommand.\n");
	cnt += sprintf(buffer + cnt, "null_device X: Simulate a NULL "
	    "sc_cmd->device pointer.\n");
	cnt += sprintf(buffer + cnt, "fcoe_cap 0: Simulate a call from the qed "
	    "FCoE capabilities callback.\n");
	cnt = min_t(int, count, cnt - *ppos);
	*ppos += cnt;
	return cnt;
}

static void qedf_dbg_send_adisc(struct qedf_ctx *qedf, uint32_t port_id)
{
	struct fc_lport *lport = qedf->lport;
	struct fc_rport_priv *rdata;

#ifdef FC_RPORT_LOOKUP
	rdata = fc_rport_lookup(lport, port_id);
#else
	rdata = lport->tt.rport_lookup(lport, port_id);
#endif

	if (rdata) {
		QEDF_ERR(&(qedf->dbg_ctx), "ADISC port_id=%x.\n", port_id);
		/*
		 * Calling rport_login when the rport is in the ready state
		 * will make libfc send an ADISC.
		 */
		rdata->flags &= ~FC_RP_STARTED;
#ifdef FC_RPORT_LOGIN
		fc_rport_login(rdata);
#else
		lport->tt.rport_login(rdata);
#endif
	} else {
		QEDF_ERR(&(qedf->dbg_ctx), "Could not find port_id=%x.\n",
			  port_id);
	}
}

static void qedf_dbg_restart_rport(struct qedf_ctx *qedf, uint32_t port_id)
{
	struct fc_lport *lport = qedf->lport;
	struct fc_rport_priv *rdata;
	struct fc_rport *rport;
	struct fc_rport_libfc_priv *rp;
	struct qedf_rport *fcport;

#ifdef FC_RPORT_LOOKUP
	rdata = fc_rport_lookup(lport, port_id);
#else
	rdata = lport->tt.rport_lookup(lport, port_id);
#endif

	if (rdata) {
		rport = rdata->rport;
		if (!rport)
			goto out_err;
		rp = rport->dd_data;
		fcport = (struct qedf_rport *)&rp[1];
		QEDF_ERR(&(qedf->dbg_ctx), "Restarting port_id=%x.\n", port_id);
		qedf_restart_rport(fcport);
	} else
		goto out_err;

	return;

out_err:
	QEDF_ERR(&(qedf->dbg_ctx), "Could not find port_id=%x.\n",
	    port_id);

}

static void qedf_show_protocol_tlv_data(struct qedf_ctx *qedf,
	struct qed_mfw_tlv_fcoe *fcoe)
{
	char buf[32];

	QEDF_ERR(&(qedf->dbg_ctx), "ra_tov=%u.\n", fcoe->ra_tov);
	QEDF_ERR(&(qedf->dbg_ctx), "ed_tov=%u.\n", fcoe->ed_tov);
	QEDF_ERR(&(qedf->dbg_ctx), "num_npiv_ids=%d.\n", fcoe->num_npiv_ids);
	fcoe_wwn_to_str(wwn_to_u64(fcoe->switch_name), buf, sizeof(buf));
	QEDF_ERR(&(qedf->dbg_ctx), "switch_name=%s.\n", buf);
	QEDF_ERR(&(qedf->dbg_ctx), "port_state=%d.\n", fcoe->port_state);
	QEDF_ERR(&(qedf->dbg_ctx), "link_failures=%u.\n", fcoe->link_failures);
	QEDF_ERR(&(qedf->dbg_ctx), "fcoe_rxq_depth=%u.\n",
	    fcoe->fcoe_rxq_depth);
	QEDF_ERR(&(qedf->dbg_ctx), "fcoe_txq_depth=%u.\n",
	    fcoe->fcoe_txq_depth);
	QEDF_ERR(&(qedf->dbg_ctx), "fcoe_rx_frames=%llu.\n",
	    fcoe->fcoe_rx_frames);
	QEDF_ERR(&(qedf->dbg_ctx), "fcoe_tx_frames=%llu.\n",
	    fcoe->fcoe_tx_frames);
	QEDF_ERR(&(qedf->dbg_ctx), "fcoe_rx_bytes=%llu.\n",
	    fcoe->fcoe_rx_bytes);
	QEDF_ERR(&(qedf->dbg_ctx), "fcoe_tx_bytes=%llu.\n",
	    fcoe->fcoe_tx_bytes);
	QEDF_ERR(&(qedf->dbg_ctx), "crc_count=%u.\n", fcoe->crc_count);
	QEDF_ERR(&(qedf->dbg_ctx), "tx_abts=%u.\n", fcoe->tx_abts);
	QEDF_ERR(&(qedf->dbg_ctx), "tx_lun_rst=%u.\n", fcoe->tx_lun_rst);
	QEDF_ERR(&(qedf->dbg_ctx), "abort_task_sets=%u.\n",
	    fcoe->abort_task_sets);
	QEDF_ERR(&(qedf->dbg_ctx), "scsi_busy=%u.\n", fcoe->scsi_busy);
	QEDF_ERR(&(qedf->dbg_ctx), "scsi_tsk_full=%u.\n", fcoe->scsi_tsk_full);
	QEDF_ERR(&(qedf->dbg_ctx), "npiv_state=%u.\n", fcoe->npiv_state);
	QEDF_ERR(&(qedf->dbg_ctx), "qos_pri=%u.\n", fcoe->qos_pri);
}

static void qedf_show_fcoe_caps(struct qedf_ctx *qedf,
	struct qed_fcoe_caps *p_caps)
{
	QEDF_ERR(&qedf->dbg_ctx, "max_ios=%u.\n", p_caps->max_ios);
	QEDF_ERR(&qedf->dbg_ctx, "max_log=%u.\n", p_caps->max_log);
	QEDF_ERR(&qedf->dbg_ctx, "max_exch=%u.\n", p_caps->max_exch);
	QEDF_ERR(&qedf->dbg_ctx, "max_npiv=%u.\n", p_caps->max_npiv);
	QEDF_ERR(&qedf->dbg_ctx, "max_tgt=%u.\n", p_caps->max_tgt);
	QEDF_ERR(&qedf->dbg_ctx, "max_outstnd=%u.\n", p_caps->max_outstnd);
}

static ssize_t
qedf_dbg_error_inject_cmd_write(struct file *filp,
				    const char __user *buffer, size_t count,
				    loff_t *ppos)
{
	void *kern_buf;
	char inject_cmd[25];
	int val;
	struct qedf_dbg_ctx *qedf_dbg =
				(struct qedf_dbg_ctx *)filp->private_data;
	struct qedf_ctx *qedf = container_of(qedf_dbg,
	    struct qedf_ctx, dbg_ctx);

	QEDF_INFO(qedf_dbg, QEDF_LOG_DEBUGFS, "entered\n");

	if (!count || *ppos)
		return 0;

	kern_buf = memdup_user(buffer, 25);
	if (IS_ERR(kern_buf))
		return PTR_ERR(kern_buf);

	if (sscanf(kern_buf, "%s %d", inject_cmd, &val) != 2) {
		kfree(kern_buf);
		return -EINVAL;
	}

	QEDF_ERR(qedf_dbg, "Inject command is %s and argument is %d.\n",
	    inject_cmd, val);

	if (strncmp(inject_cmd, "drop_cmd", 25) == 0)
		qedf->drop_cmd = val;
	else if (strncmp(inject_cmd, "drop_cleanup", 25) == 0)
		qedf->drop_cleanup = val;
	else if (strncmp(inject_cmd, "drop_els", 25) == 0)
		qedf->drop_els = val;
	else if (strncmp(inject_cmd, "drop_abts", 25) == 0)
		qedf->drop_abts = val;
	else if (strncmp(inject_cmd, "drop_abts_queue", 25) == 0)
		qedf->drop_abts_queue = val;
	else if (strncmp(inject_cmd, "error_detect", 25) == 0)
		qedf->error_detect = val;
	else if (strncmp(inject_cmd, "adisc", 25) == 0)
		qedf_dbg_send_adisc(qedf, val);
	else if (strncmp(inject_cmd, "link_down", 25) == 0) {
		QEDF_ERR(qedf_dbg, "Debug LINK DOWN.\n");
		atomic_set(&qedf->link_state, QEDF_LINK_DOWN);
		queue_delayed_work(qedf->link_update_wq, &qedf->link_update,
		    qedf_link_down_tmo * HZ);
	} else if (strncmp(inject_cmd, "link_up", 25) == 0) {
		QEDF_ERR(qedf_dbg, "Debug LINK UP.\n");
		atomic_set(&qedf->link_state, QEDF_LINK_UP);
		qedf->vlan_id  = 0;
		queue_delayed_work(qedf->link_update_wq, &qedf->link_update,
		    0);
	} else if (strncmp(inject_cmd, "recovery", 25) == 0)
		qed_ops->common->recovery_process(qedf->cdev);
	else if (strncmp(inject_cmd, "fan", 25) == 0)
		qedf_schedule_hw_err_handler(qedf, QED_HW_ERR_FAN_FAIL);
	else if (strncmp(inject_cmd, "fcp_prot_error", 25) == 0)
		qedf->fcp_prot_error = val;
	else if (strncmp(inject_cmd, "underrun_error", 25) == 0)
		qedf->underrun_error = val;
	else if (strncmp(inject_cmd, "tlv_data", 25) == 0) {
		struct qed_mfw_tlv_fcoe fcoe;

		qedf_get_protocol_tlv_data(qedf, &fcoe);
		qedf_show_protocol_tlv_data(qedf, &fcoe);
	} else if (strncmp(inject_cmd, "abts_acc", 25) == 0)
		qedf->abts_acc = val;
	else if (strncmp(inject_cmd, "hw_attr_update", 25) == 0)
		qedf_hw_attr_update(qedf, QED_HW_INFO_CHANGE_OVLAN);
	else if (strncmp(inject_cmd, "stag_null_cookie", 25) == 0)
		qedf_hw_attr_update(NULL, QED_HW_INFO_CHANGE_OVLAN);
	else if (strncmp(inject_cmd, "restart_rport", 25) == 0)
		qedf_dbg_restart_rport(qedf, val);
	else if (strncmp(inject_cmd, "generic_tlv_data", 25) == 0) {
		struct qed_generic_tlvs data;

		qedf_get_generic_tlv_data(qedf, &data);
	} else if (strncmp(inject_cmd, "drop_tmf", 25) == 0)
		qedf->drop_tmf = val;
	else if (strncmp(inject_cmd, "hw_err", 25) == 0)
		qedf_schedule_hw_err_handler(qedf, QED_HW_ERR_FW_ASSERT);
	else if (strncmp(inject_cmd, "no_abts_tmo", 25) == 0)
		qedf->no_abts_tmo = val;
	else if (strncmp(inject_cmd, "fail_post_io_req", 25) == 0)
		qedf->fail_post_io_req = val;
	else if (strncmp(inject_cmd, "null_device", 25) == 0)
		qedf->dbg_null_device = val;
	else if (strncmp(inject_cmd, "fcoe_cap", 25) == 0) {
		struct qed_fcoe_caps p_caps;
		qedf_get_fcoe_capabilities(qedf, &p_caps);
		qedf_show_fcoe_caps(qedf, &p_caps);
	} else
		QEDF_ERR(qedf_dbg, "Unknown error inject command");

	kfree(kern_buf);
	return count;
}
#endif /* ERROR_INJECT */

const struct file_operations qedf_dbg_fops[] = {
	qedf_dbg_fileops_seq(qedf, fp_int),
	qedf_dbg_fileops_seq(qedf, io_trace),
	qedf_dbg_fileops_seq(qedf, debug),
	qedf_dbg_fileops(qedf, stop_io_on_error),
	qedf_dbg_fileops_seq(qedf, driver_stats),
	qedf_dbg_fileops(qedf, clear_stats),
	qedf_dbg_fileops_seq(qedf, offload_stats),
#ifdef ERROR_INJECT
	qedf_dbg_fileops(qedf, error_inject),
#endif
	qedf_dbg_fileops_seq(qedf, dcbx_info),
	/* This must be last */
	{},
};

#else /* CONFIG_DEBUG_FS */
void qedf_dbg_host_init(struct qedf_dbg_ctx *);
void qedf_dbg_host_exit(struct qedf_dbg_ctx *);
void qedf_dbg_init(char *);
void qedf_dbg_exit(void);
#endif /* CONFIG_DEBUG_FS */
