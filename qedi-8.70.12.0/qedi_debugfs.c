/*
 *  QLogic iSCSI Offload Driver
 *  Copyright (c) 2015-2018 Cavium Inc.
 *
 *  See LICENSE.qedi for copyright and licensing details.
 */

#include "qedi.h"

#ifdef CONFIG_DEBUG_FS

static ssize_t
qedi_dbg_do_not_recover_enable(struct qedi_dbg_ctx *qedi_dbg)
{
	if (!do_not_recover)
		do_not_recover = 1;

	QEDI_INFO(qedi_dbg, QEDI_LOG_DEBUGFS, "do_not_recover=%d\n",
		  do_not_recover);
	return 0;
}

static ssize_t
qedi_dbg_do_not_recover_disable(struct qedi_dbg_ctx *qedi_dbg)
{
	if (do_not_recover)
		do_not_recover = 0;

	QEDI_INFO(qedi_dbg, QEDI_LOG_DEBUGFS, "do_not_recover=%d\n",
		  do_not_recover);
	return 0;
}

static struct qedi_list_of_funcs qedi_dbg_do_not_recover_ops[] = {
	{ "enable", qedi_dbg_do_not_recover_enable },
	{ "disable", qedi_dbg_do_not_recover_disable },
	{ NULL, NULL }
};

struct qedi_debugfs_ops qedi_debugfs_ops[] = {
	{ "gbl_ctx", NULL },
	{ "do_not_recover", qedi_dbg_do_not_recover_ops},
	{ "io_trace", NULL },
	{ "nvram", NULL },
	{ "nvram_raw", NULL },
#ifdef ERROR_INJECT
	{ "error_inject", NULL },
#endif
	{ NULL, NULL }
};

static ssize_t
qedi_dbg_do_not_recover_cmd_write(struct file *filp, const char __user *buffer,
				  size_t count, loff_t *ppos)
{
	size_t cnt = 0;
	struct qedi_dbg_ctx *qedi_dbg =
			(struct qedi_dbg_ctx *)filp->private_data;
	struct qedi_list_of_funcs *lof = qedi_dbg_do_not_recover_ops;

	if (*ppos)
		return 0;

	while (lof) {
		if (!(lof->oper_str))
			break;

		if (!strncmp(lof->oper_str, buffer, strlen(lof->oper_str))) {
			cnt = lof->oper_func(qedi_dbg);
			break;
		}

		lof++;
	}
	return (count - cnt);
}

static ssize_t
qedi_dbg_do_not_recover_cmd_read(struct file *filp, char __user *buffer,
				 size_t count, loff_t *ppos)
{
	size_t cnt = 0;
	struct qedi_dbg_ctx *qedi_dbg =
			(struct qedi_dbg_ctx *)filp->private_data;

	QEDI_INFO(qedi_dbg, QEDI_LOG_DEBUGFS, "entered\n");

	if (*ppos)
		return 0;

	cnt = sprintf(buffer, "do_not_recover=%d\n", do_not_recover);
	cnt = min_t(int, count, cnt - *ppos);
	*ppos += cnt;
	return cnt;
}

static int
qedi_gbl_ctx_show(struct seq_file *s, void *unused)
{
	struct qedi_fastpath *fp = NULL;
	struct qed_sb_info *sb_info = NULL;
	struct status_block *sb = NULL;
	struct global_queue *que = NULL;
	int id;
	u16 prod_idx;
	struct qedi_ctx *qedi = s->private;
	unsigned long flags;

	seq_printf(s, " DUMP CQ CONTEXT:\n");

	for (id = 0; id < qedi->num_queues; id++) {
		spin_lock_irqsave(&qedi->hba_lock, flags);
		seq_printf(s, "=========FAST CQ PATH [%d] ==========\n", id);
		fp = &qedi->fp_array[id];
		sb_info = fp->sb_info;
		sb = sb_info->sb_virt;
		prod_idx = (sb->pi_array[QEDI_PROTO_CQ_PROD_IDX] &
			    STATUS_BLOCK_PROD_INDEX_MASK);
		seq_printf(s, "SB PROD IDX: %d\n", prod_idx);
		que = qedi->global_queues[fp->sb_id];
		seq_printf(s, "DRV CONS IDX: %d\n", que->cq_cons_idx);
		seq_printf(s, "CQ complete host memory: %d\n", fp->sb_id);
		seq_printf(s, "=========== END ==================\n\n\n");
		spin_unlock_irqrestore(&qedi->hba_lock, flags);
	}
	return 0;
}

static int
qedi_dbg_gbl_ctx_open(struct inode *inode, struct file *file)
{
	struct qedi_dbg_ctx *qedi_dbg = inode->i_private;
	struct qedi_ctx *qedi = container_of(qedi_dbg, struct qedi_ctx,
					     dbg_ctx);

	return single_open(file, qedi_gbl_ctx_show, qedi);
}

static int
qedi_io_trace_show(struct seq_file *s, void *unused)
{
	u32 id, idx = 0;
	struct qedi_ctx *qedi = s->private;
	struct qedi_io_log *io_log;
	unsigned long flags;

	seq_printf(s, " DUMP IO LOGS:\n");
	spin_lock_irqsave(&qedi->io_trace_lock, flags);
	idx = qedi->io_trace_idx;
	for (id = 0; id < QEDI_IO_TRACE_SIZE; id++) {
		io_log = &qedi->io_trace_buf[idx];
		seq_printf(s, "iodir-%d:", io_log->direction);
		seq_printf(s, "tid-0x%x:", io_log->task_id);
		seq_printf(s, "cid-0x%x:", io_log->cid);
		seq_printf(s, "lun-%d:", io_log->lun);
		seq_printf(s, "op-0x%02x:", io_log->op);
		seq_printf(s, "0x%02x%02x%02x%02x:", io_log->lba[0],
			   io_log->lba[1], io_log->lba[2], io_log->lba[3]);
		seq_printf(s, "buflen-%d:", io_log->bufflen);
		seq_printf(s, "sgcnt-%d:", io_log->sg_count);
		seq_printf(s, "res-0x%08x:", io_log->result);
		seq_printf(s, "jif-%lu:", io_log->jiffies);
		seq_printf(s, "blk_req_cpu-%d:", io_log->blk_req_cpu);
		seq_printf(s, "req_cpu-%d:", io_log->req_cpu);
		seq_printf(s, "intr_cpu-%d:", io_log->intr_cpu);
		seq_printf(s, "blk_rsp_cpu-%d\n", io_log->blk_rsp_cpu);

		idx++;
		if (idx == QEDI_IO_TRACE_SIZE)
			idx = 0;
	}
	spin_unlock_irqrestore(&qedi->io_trace_lock, flags);
	return 0;
}

static int
qedi_dbg_io_trace_open(struct inode *inode, struct file *file)
{
	struct qedi_dbg_ctx *qedi_dbg = inode->i_private;
	struct qedi_ctx *qedi = container_of(qedi_dbg, struct qedi_ctx,
					     dbg_ctx);

	return single_open(file, qedi_io_trace_show, qedi);
}

static void
qedi_ip_show(struct seq_file *s, char *prefix, char *fmt, u8 *buf)
{
	seq_printf(s, prefix);
	seq_printf(s, fmt, buf);
}

static void
qedi_target_show(struct seq_file *s, struct nvm_iscsi_target *tgt, u32 flags, char *fmt, u8 *ip)
{
	seq_printf(s, "Flags: 0x%04x (enabled=%x boot-time=%x)\n",
		flags,
		!!(flags & NVM_ISCSI_CFG_TARGET_ENABLED),
		!!(flags & NVM_ISCSI_CFG_BOOT_TIME_LOGIN_STATUS));
	seq_printf(s, "Name: %s\n", tgt->target_name.byte);
	qedi_ip_show(s, "IP: ", fmt, ip);
	seq_printf(s, "Port: %d\n",
		GET_FIELD2(tgt->generic_cont0, NVM_ISCSI_CFG_TARGET_TCP_PORT));
	seq_printf(s, "LUN: %.*d\n", tgt->lun.value[1], tgt->lun.value[0]);
	seq_printf(s, "Chap Name: %s\n", tgt->chap_name.byte);
	seq_printf(s, "Chap Password: %s\n", tgt->chap_password.byte);
}

static int
qedi_block_show(struct seq_file *s, struct nvm_iscsi_block *block)
{
	struct nvm_iscsi_initiator *initiator;
	struct nvm_iscsi_generic *generic;
	struct nvm_iscsi_target *tgt;
	u32 not_empty, pf_mapped;
	u32 ipv6_en, vlan_en;
	u32 flags;
	char *fmt, *ip, *sub, *gw, *pdns, *sdns, *dhcp_addr, *isns_server, *slp_server, *pradius_server, *sradius_server;
	u32 i;

	flags = block->id;
	not_empty = !!(GET_FIELD2(flags, NVM_ISCSI_CFG_BLK_CTRL_FLAG) &
		NVM_ISCSI_CFG_BLK_CTRL_FLAG_IS_NOT_EMPTY);
	pf_mapped = !!(GET_FIELD2(flags, NVM_ISCSI_CFG_BLK_CTRL_FLAG) &
		NVM_ISCSI_CFG_BLK_CTRL_FLAG_PF_MAPPED);

	seq_printf(s, "ID: 0x%04x (pf=%x not-empty=%x pf-mapped=%x)\n",
		flags, GET_FIELD2(flags, NVM_ISCSI_CFG_BLK_MAPPED_PF_ID),
		not_empty, pf_mapped);

	if (!not_empty || !pf_mapped)
		return 0;

	seq_printf(s, "\nGeneric:\n");
	generic = &block->generic;
	flags = generic->ctrl_flags;

	seq_printf(s, "Flags: 0x%04x (chap=%x mchap=%x dhcp-tcp=%x dhcp-iscsi=%x "
		"dhcp-ipv6=%x dhcp-ipv4-fall=%x isns-world=%x isns-sel=%x)\n",
		flags,
		!!(flags & NVM_ISCSI_CFG_GEN_CHAP_ENABLED),
		!!(flags & NVM_ISCSI_CFG_GEN_CHAP_MUTUAL_ENABLED),
		!!(flags & NVM_ISCSI_CFG_GEN_DHCP_TCPIP_CONFIG_ENABLED),
		!!(flags & NVM_ISCSI_CFG_GEN_DHCP_ISCSI_CONFIG_ENABLED),
		!!(flags & NVM_ISCSI_CFG_GEN_IPV6_ENABLED),
		!!(flags & NVM_ISCSI_CFG_GEN_IPV4_FALLBACK_ENABLED),
		!!(flags & NVM_ISCSI_CFG_GEN_ISNS_WORLD_LOGIN),
		!!(flags & NVM_ISCSI_CFG_GEN_ISNS_SELECTIVE_LOGIN));
	seq_printf(s, "Request Timeout: %u\n",
		GET_FIELD2(generic->timeout, NVM_ISCSI_CFG_GEN_DHCP_REQUEST_TIMEOUT));
	seq_printf(s, "Login Timeout: %u\n",
		GET_FIELD2(generic->timeout, NVM_ISCSI_CFG_GEN_PORT_LOGIN_TIMEOUT));
	seq_printf(s, "DHCP Vendor-ID: %s\n", generic->dhcp_vendor_id.byte);

	ipv6_en = block->generic.ctrl_flags &
		NVM_ISCSI_CFG_GEN_IPV6_ENABLED;
        fmt = ipv6_en ? "%pI6\n": "%pI4\n";

	seq_printf(s, "\nInitiator:\n");
	initiator = &block->initiator;
	vlan_en = initiator->ctrl_flags & NVM_ISCSI_CFG_INITIATOR_VLAN_ENABLED;
	ip = ipv6_en ? initiator->ipv6.addr.byte: initiator->ipv4.addr.byte;
	sub = ipv6_en ? initiator->ipv6.subnet_mask.byte: initiator->ipv4.subnet_mask.byte;
	gw = ipv6_en ? initiator->ipv6.gateway.byte: initiator->ipv4.gateway.byte;
	pdns = ipv6_en ? initiator->ipv6.primary_dns.byte: initiator->ipv4.primary_dns.byte;
	sdns = ipv6_en ? initiator->ipv6.secondary_dns.byte: initiator->ipv4.secondary_dns.byte;
	dhcp_addr = ipv6_en ? initiator->ipv6.dhcp_addr.byte: initiator->ipv4.dhcp_addr.byte;
	isns_server = ipv6_en ? initiator->ipv6.isns_server.byte: initiator->ipv4.isns_server.byte;
	slp_server = ipv6_en ? initiator->ipv6.slp_server.byte: initiator->ipv4.slp_server.byte;
	pradius_server = ipv6_en ? initiator->ipv6.primay_radius_server.byte: initiator->ipv4.primay_radius_server.byte;
	sradius_server = ipv6_en ? initiator->ipv6.secondary_radius_server.byte: initiator->ipv4.secondary_radius_server.byte;

	seq_printf(s, "Name: %s\n", initiator->initiator_name.byte);
	seq_printf(s, "VLAN-ID: 0x%x\n", vlan_en ?
		GET_FIELD2(initiator->generic_cont0, NVM_ISCSI_CFG_INITIATOR_VLAN) :
		0);
	qedi_ip_show(s, "IP: ", fmt, ip);
	qedi_ip_show(s, "Subnet Mask: ", fmt, sub);
	qedi_ip_show(s, "Gateway: ", fmt, gw);
	qedi_ip_show(s, "Primary DNS: ", fmt, pdns);
	qedi_ip_show(s, "Secondary DNS: ", fmt, sdns);
	qedi_ip_show(s, "DHCP Address: ", fmt, dhcp_addr);
	qedi_ip_show(s, "iSNS Server: ", fmt, isns_server);
	qedi_ip_show(s, "SLP Server: ", fmt, slp_server);
	qedi_ip_show(s, "Primary Radius Server: ", fmt, pradius_server);
	qedi_ip_show(s, "Secondary Radius Server: ", fmt, sradius_server);
	seq_printf(s, "Chap Name: %s\n", initiator->chap_name.byte);
	seq_printf(s, "Chap Password: %s\n", initiator->chap_password.byte);

	tgt = block->target;
	for (i = 0; i < NUM_OF_ISCSI_TARGET_PER_PF; i++, tgt++) {
		seq_printf(s, "\nTarget %d:\n", i);
		ip = ipv6_en ? tgt->ipv6_addr.byte: tgt->ipv4_addr.byte;
		flags = tgt->ctrl_flags;
		qedi_target_show(s, tgt, flags, fmt, ip);
	}
	return 0;
}

static int
qedi_nvram_show(struct seq_file *s, void *unused)
{
	struct qedi_ctx *qedi = s->private;
	struct nvm_iscsi_block *block;
	u32 i;
	u8 pf;

	pf = qedi->dev_info.common.abs_pf_id;
	seq_printf(s, "NVRAM iSCSI Block (abs-id=0x%x)\n", pf);
	block = qedi->iscsi_image->iscsi_cfg.block;
	for (i = 0; i < NUM_OF_ISCSI_PF_SUPPORTED; i++, block++)
		if (pf == GET_FIELD2(block->id, NVM_ISCSI_CFG_BLK_MAPPED_PF_ID))
			goto found;

	seq_printf(s, "<NOT MAPPED>\n");
	return 0;
found:
	qedi_block_show(s, block);
	seq_printf(s, "---------------------------------------------\n");
	return 0;
}

static int
qedi_nvram_raw_show(struct seq_file *s, void *unused)
{
	struct qedi_ctx *qedi = s->private;
	struct nvm_iscsi_block *block;
	u32 i;

	block = qedi->iscsi_image->iscsi_cfg.block;
	for (i = 0; i < NUM_OF_ISCSI_PF_SUPPORTED; i++, block++) {
		seq_printf(s, "NVRAM iSCSI Block %d\n", i);
		qedi_block_show(s, block);
		seq_printf(s, "---------------------------------------------\n\n");
	}
	return 0;
}

static int
qedi_dbg_nvram_open(struct inode *inode, struct file *file)
{
	struct qedi_dbg_ctx *qedi_dbg = inode->i_private;
	struct qedi_ctx *qedi = container_of(qedi_dbg, struct qedi_ctx,
					     dbg_ctx);

	return single_open(file, qedi_nvram_show, qedi);
}

static int
qedi_dbg_nvram_raw_open(struct inode *inode, struct file *file)
{
	struct qedi_dbg_ctx *qedi_dbg = inode->i_private;
	struct qedi_ctx *qedi = container_of(qedi_dbg, struct qedi_ctx,
					     dbg_ctx);

	return single_open(file, qedi_nvram_raw_show, qedi);
}

#ifdef ERROR_INJECT
#define QEDI_INJECT_STR_SIZE		25
static ssize_t
qedi_dbg_error_inject_cmd_read(struct file *filp, char __user *buffer,
	size_t count, loff_t *ppos)
{
	ssize_t cnt = 0;
	struct qedi_dbg_ctx *qedi_debug =
	    (struct qedi_dbg_ctx *)filp->private_data;

	QEDI_INFO(qedi_debug, QEDI_LOG_DEBUGFS,
	    "You are here.\n");

	cnt = min_t(int, count, cnt - *ppos);
	*ppos += cnt;
	return cnt;
}

void qedi_dbg_show_protocol_tlv_data(struct qedi_ctx *qedi,
	struct qed_mfw_tlv_iscsi *iscsi)
{
	QEDI_ERR(&qedi->dbg_ctx, "rx_frames=%d\n", iscsi->rx_frames);
	QEDI_ERR(&qedi->dbg_ctx, "rx_bytes=%d\n", iscsi->rx_bytes);
	QEDI_ERR(&qedi->dbg_ctx, "tx_frames=%d\n", iscsi->rx_frames);
	QEDI_ERR(&qedi->dbg_ctx, "tx_bytes=%d\n", iscsi->tx_bytes);
	QEDI_ERR(&qedi->dbg_ctx, "frame_size=%d\n", iscsi->frame_size);
	QEDI_ERR(&qedi->dbg_ctx, "auth_method=%d\n", iscsi->auth_method);
	QEDI_ERR(&qedi->dbg_ctx, "tx_desc_size=%d\n", iscsi->tx_desc_size);
	QEDI_ERR(&qedi->dbg_ctx, "rx_desc_size=%d\n", iscsi->rx_desc_size);
}

static ssize_t
qedi_dbg_error_inject_cmd_write(struct file *filp, const char __user *buffer,
			 size_t count, loff_t *ppos)
{
	uint32_t val;
	char inject_cmd[20];
	void *kern_buf;
	struct qedi_dbg_ctx *qedi_debug =
	    (struct qedi_dbg_ctx *)filp->private_data;
	struct qedi_ctx *qedi = container_of(qedi_debug,
	    struct qedi_ctx, dbg_ctx);

	if (!count || *ppos)
		return 0;

	kern_buf = memdup_user(buffer, count);
	if (IS_ERR(kern_buf))
		return PTR_ERR(kern_buf);

	if (sscanf(kern_buf, "%s %d", inject_cmd, &val) != 2) {
		kfree(kern_buf);
		return -EINVAL;
	}

	QEDI_ERR(qedi_debug,
	    "Inject command is %s and argument is %d.\n", inject_cmd, val);

	if (strncmp(inject_cmd, "drop_cmd", QEDI_INJECT_STR_SIZE) == 0)
		qedi->drop_cmd = val;
	else if (strncmp(inject_cmd, "drop_abort_queue",
	    QEDI_INJECT_STR_SIZE) == 0)
		qedi->drop_abort_queue = val;
	else if (strncmp(inject_cmd, "drop_tmf", QEDI_INJECT_STR_SIZE) == 0)
		qedi->drop_tmf = val;
	else if (strncmp(inject_cmd, "underrun", QEDI_INJECT_STR_SIZE) == 0)
		qedi->dbg_underrun = val;
	else if (strncmp(inject_cmd, "drop_cleanup", QEDI_INJECT_STR_SIZE) == 0)
		qedi->drop_cleanup = val;
	else if (strncmp(inject_cmd, "recovery", QEDI_INJECT_STR_SIZE) == 0)
		qedi_ops->common->recovery_process(qedi->cdev);
	else if (strncmp(inject_cmd, "tlv_data", QEDI_INJECT_STR_SIZE) == 0) {
		struct qed_mfw_tlv_iscsi iscsi;
		qedi_get_protocol_tlv_data(qedi, &iscsi);
		qedi_dbg_show_protocol_tlv_data(qedi, &iscsi);
	} else if (strncmp(inject_cmd, "generic_tlv_data",
	     QEDI_INJECT_STR_SIZE) == 0) {
		struct qed_generic_tlvs data;
		qedi_get_generic_tlv_data(qedi, &data);
	} else if (strncmp(inject_cmd, "fan", QEDI_INJECT_STR_SIZE) == 0)
		qedi_schedule_hw_err_handler(qedi, QED_HW_ERR_FAN_FAIL);
	else if (strncmp(inject_cmd, "drop_nopin", QEDI_INJECT_STR_SIZE) == 0)
		qedi->drop_nopin = val;
	else if (strncmp(inject_cmd, "drop_nopout", QEDI_INJECT_STR_SIZE) == 0)
		qedi->drop_nopout = val;
	else if (strncmp(inject_cmd, "drop_login", QEDI_INJECT_STR_SIZE) == 0)
		qedi->drop_login = val;
	else if (strncmp(inject_cmd, "drop_logout", QEDI_INJECT_STR_SIZE) == 0)
		qedi->drop_logout = val;
	else if (strncmp(inject_cmd, "drop_text", QEDI_INJECT_STR_SIZE) == 0)
		qedi->drop_text = val;
	else if (strncmp(inject_cmd, "hw_err", QEDI_INJECT_STR_SIZE) == 0)
		qedi_schedule_hw_err_handler(qedi, QED_HW_ERR_FW_ASSERT);
	else
		QEDI_ERR(qedi_debug, "Unknown error inject command.\n");

	kfree(kern_buf);
	return count;
}
#endif

struct file_operations qedi_dbg_fops[] = {
	qedi_dbg_fileops_seq(qedi, gbl_ctx),
	qedi_dbg_fileops(qedi, do_not_recover),
	qedi_dbg_fileops_seq(qedi, io_trace),
	qedi_dbg_fileops_seq(qedi, nvram),
	qedi_dbg_fileops_seq(qedi, nvram_raw),
#ifdef ERROR_INJECT
	qedi_dbg_fileops(qedi, error_inject),
#endif
	{},
};

#endif /* CONFIG_DEBUG_FS */
