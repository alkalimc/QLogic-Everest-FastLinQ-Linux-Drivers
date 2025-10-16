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

#include <linux/module.h>
#include <linux/iommu.h>
#include <linux/sysfs.h>
#include "common_hsi.h"
#include "qedr_hsi_rdma.h"
#include "rdma_common.h"
#include "qedr.h"
#include "qedr_debugfs.h"
#include "qedr_compat.h"

#ifdef CONFIG_DEBUG_FS /* ! QEDR_UPSTREAM */
#include <linux/debugfs.h>
#endif

#ifdef _HAS_SYSFS_BIN_ATTR_INIT
static struct kobject *qedr_sysfs_root;
struct qedr_sysfs_obj {
	struct bin_attribute bin_attr;
	struct kobject *sysfs;
	struct list_head list;
};

static struct list_head qedr_sysfs_stats_head;

#define __init_sysfs(feature) \
{ \
	sysfs_bin_attr_init(&feature##_info->bin_attr);\
	feature##_info->bin_attr.attr.mode = 0600;\
	feature##_info->bin_attr.size = 4096;\
	feature##_info->bin_attr.read = sysfs_show;\
	feature##_info->bin_attr.write = sysfs_store;\
	feature##_info->bin_attr.private = dev;\
	feature##_info->sysfs = NULL;\
}
#endif

#ifdef CONFIG_DEBUG_FS /* ! QEDR_UPSTREAM */
static struct dentry *qedr_dbgfs_dir;

static ssize_t __qedr_read_stats(struct qedr_dev *dev, char *buffer,
		size_t usr_buf_len, loff_t *ppos,
		bool is_sysfs);

static ssize_t __qedr_write_stats(struct qedr_dev *dev,
		const char __user *buffer,
		size_t usr_buf_len, loff_t *ppos,
		bool is_sysfs);

#define QED_RDMA_STRING_LEN	80	/* max characters in stat line */
#define QED_RDMA_NAME_SPACE	-25	/* stat name alignment */

#define QED_RDMA_STAT_OFFSET(stat_name) \
	(offsetof(struct qed_rdma_stats_out_params, stat_name))
#define QED_RDMA_STAT(stat_name) \
	 {QED_RDMA_STAT_OFFSET(stat_name), #stat_name}
#define QED_RDMA_STATS_DATA(stats, index) \
	(*((u64 *)(uintptr_t)(((u64) ((uintptr_t)stats)) + \
			qed_rdma_stats_arr[(index)].offset)))
#if DEFINE_THIS_CPU_INC
#define QEDR_RDMA_STAT_OFFSET(stat_name) \
	(offsetof(struct qedr_stats, stat_name))
#define QEDR_RDMA_STAT(stat_name) \
	 {QEDR_RDMA_STAT_OFFSET(stat_name), #stat_name}
#define QEDR_RDMA_SEND_STAT_OFFSET(stat_ix) \
		(offsetof(struct qedr_stats, send_wr) + \
		 sizeof(u64) * stat_ix)
#define QEDR_RDMA_SEND_STAT(stat_name, index) \
	 {QEDR_RDMA_SEND_STAT_OFFSET(index), #stat_name}
#define QEDR_RDMA_STATS_DATA(stats, index) \
	(*((u64 *)(uintptr_t)(((u64) ((uintptr_t)stats)) + \
			qedr_rdma_stats_arr[(index)].offset)))
#endif

#define QED_IWARP_STATS_DATA(stats, index) \
	(*((u64 *)(uintptr_t)(((u64) ((uintptr_t)stats)) + \
			qed_iwarp_stats_arr[(index)].offset)))

#define QED_ROCE_STATS_DATA(stats, index) \
	(*((u64 *)(uintptr_t)(((u64) ((uintptr_t)stats)) + \
			qed_roce_stats_arr[(index)].offset)))

static const struct {
	u64 offset;
	char string[QED_RDMA_STRING_LEN];
} qed_rdma_stats_arr[] = {
	QED_RDMA_STAT(sent_bytes),
	QED_RDMA_STAT(sent_pkts),
	QED_RDMA_STAT(rcv_bytes),
	QED_RDMA_STAT(rcv_pkts),
};

static const struct {
	u64 offset;
	char string[QED_RDMA_STRING_LEN];
} qed_roce_stats_arr[] = {
	QED_RDMA_STAT(icrc_errors),
	QED_RDMA_STAT(retransmit_events),
	QED_RDMA_STAT(silent_drops),
	QED_RDMA_STAT(rnr_nacks_sent),
	QED_RDMA_STAT(ecn_pkt_rcv),
	QED_RDMA_STAT(cnp_pkt_rcv),
	QED_RDMA_STAT(cnp_pkt_sent),
	QED_RDMA_STAT(cnp_pkt_reject),
	QED_RDMA_STAT(implied_nak_seq_err),
	QED_RDMA_STAT(duplicate_request),
	QED_RDMA_STAT(local_ack_timeout_err),
	QED_RDMA_STAT(out_of_sequence),
	QED_RDMA_STAT(packet_seq_err),
	QED_RDMA_STAT(rnr_nak_retry_err),
	QED_RDMA_STAT(req_cqe_error),
	QED_RDMA_STAT(req_remote_access_errors),
	QED_RDMA_STAT(req_remote_invalid_request),
	QED_RDMA_STAT(resp_cqe_error),
	QED_RDMA_STAT(resp_local_length_error),
	QED_RDMA_STAT(resp_remote_access_errors),
};

#if DEFINE_THIS_CPU_INC
static const struct {
	u64 offset;
	char string[QED_RDMA_STRING_LEN];
} qedr_rdma_stats_arr[] = {
	QEDR_RDMA_SEND_STAT(send_wr_send, RDMA_SQ_REQ_TYPE_SEND),
	QEDR_RDMA_SEND_STAT(send_wr_send_imm, RDMA_SQ_REQ_TYPE_SEND_WITH_IMM),
	QEDR_RDMA_SEND_STAT(send_wr_send_inv,
			    RDMA_SQ_REQ_TYPE_SEND_WITH_INVALIDATE),
	QEDR_RDMA_SEND_STAT(send_wr_rdma_write, RDMA_SQ_REQ_TYPE_RDMA_WR),
	QEDR_RDMA_SEND_STAT(send_wr_rdma_write_imm,
			    RDMA_SQ_REQ_TYPE_RDMA_WR_WITH_IMM),
	QEDR_RDMA_SEND_STAT(send_wr_rdma_read, RDMA_SQ_REQ_TYPE_RDMA_RD),
	QEDR_RDMA_SEND_STAT(send_wr_atomic_cmp_swp,
			    RDMA_SQ_REQ_TYPE_ATOMIC_CMP_AND_SWAP),
	QEDR_RDMA_SEND_STAT(send_wr_atomic_fetch_add,
			    RDMA_SQ_REQ_TYPE_ATOMIC_ADD),
	QEDR_RDMA_SEND_STAT(send_wr_local_inv,
			    RDMA_SQ_REQ_TYPE_LOCAL_INVALIDATE),
	QEDR_RDMA_SEND_STAT(send_wr_fast_mr, RDMA_SQ_REQ_TYPE_FAST_MR),
	QEDR_RDMA_SEND_STAT(send_wr_bind, RDMA_SQ_REQ_TYPE_BIND),
	QEDR_RDMA_STAT(recv_wr),
	QEDR_RDMA_STAT(send_bad_wr),
	QEDR_RDMA_STAT(recv_bad_wr)
};
#endif

static const struct {
	u64 offset;
	char string[QED_RDMA_STRING_LEN];
} qed_iwarp_stats_arr[] = {
	QED_RDMA_STAT(iwarp_tx_fast_rxmit_cnt),
	QED_RDMA_STAT(iwarp_tx_slow_start_cnt),
	QED_RDMA_STAT(unalign_rx_comp),
};

#define QED_LL2_STAT_OFFSET(stat_name) \
	(offsetof(struct qed_ll2_stats, stat_name))
#define QED_LL2_STAT(stat_name) \
	 {QED_LL2_STAT_OFFSET(stat_name), #stat_name}
#define QED_LL2_STATS_DATA(stats, index) \
	(*((u64 *)(uintptr_t)(((u64) ((uintptr_t)stats)) + \
			qed_ll2_stats_arr[(index)].offset)))

static const struct {
	u64 offset;
	char string[QED_RDMA_STRING_LEN];
} qed_ll2_stats_arr[] = {
	QED_LL2_STAT(gsi_invalid_hdr),
	QED_LL2_STAT(gsi_invalid_pkt_length),
	QED_LL2_STAT(gsi_unsupported_pkt_typ),
	QED_LL2_STAT(gsi_crcchksm_error),

	QED_LL2_STAT(packet_too_big_discard),
	QED_LL2_STAT(no_buff_discard),

	QED_LL2_STAT(rcv_ucast_bytes),
	QED_LL2_STAT(rcv_mcast_bytes),
	QED_LL2_STAT(rcv_bcast_bytes),
	QED_LL2_STAT(rcv_ucast_pkts),
	QED_LL2_STAT(rcv_mcast_pkts),
	QED_LL2_STAT(rcv_bcast_pkts),

	QED_LL2_STAT(sent_ucast_bytes),
	QED_LL2_STAT(sent_mcast_bytes),
	QED_LL2_STAT(sent_bcast_bytes),
	QED_LL2_STAT(sent_ucast_pkts),
	QED_LL2_STAT(sent_mcast_pkts),
	QED_LL2_STAT(sent_bcast_pkts),
};

#if DEFINE_THIS_CPU_INC
static void qedr_collect_stats(struct qedr_dev *dev, struct qedr_stats *stats)
{
	u64 *per_cpu_stats, *stats_64;
	unsigned int cpu, i;

	memset(stats, 0, sizeof(*stats));

	stats_64 = (u64 *)stats;

	for_each_present_cpu(cpu) {
		per_cpu_stats = (u64 *)per_cpu_ptr(dev->stats, cpu);

		for (i = 0; i < ARRAY_SIZE(qedr_rdma_stats_arr); i++)
			stats_64[i] += per_cpu_stats[i];
	}
}
#endif

#ifdef _HAS_SYSFS_BIN_ATTR_INIT
static ssize_t sysfs_show(struct file *filp, struct kobject *kobp,
			  struct bin_attribute *bin_attr, char *buffer,
			  loff_t pos, size_t count)
{
	struct qedr_dev *dev = (struct qedr_dev *)bin_attr->private;

	if (strcmp(bin_attr->attr.name, "stats") == 0)
		return __qedr_read_stats(dev, buffer, count, &pos, true);
	return 0;
}
#endif

static ssize_t qedr_dbgfs_read_stats(struct file *filp, char __user *buffer,
				     size_t usr_buf_len, loff_t *ppos)
{
	struct qedr_dev *dev = filp->private_data;

	return __qedr_read_stats(dev, buffer, usr_buf_len, ppos, false);
}

static ssize_t __qedr_read_stats(struct qedr_dev *dev, char *buffer,
		size_t usr_buf_len, loff_t *ppos,
		bool is_sysfs)
{
	struct qed_rdma_stats_out_params stats;
	size_t lines, offset = 0;
	u8 stats_queue = 0;
	ssize_t res;
	char *data;
	int i, rc;

	/* No partial reads */
	if (*ppos != 0)
		return 0;

	lines = ARRAY_SIZE(qed_rdma_stats_arr);
	if (IS_ROCE(dev))
		lines += ARRAY_SIZE(qed_roce_stats_arr);
	else
		lines += ARRAY_SIZE(qed_iwarp_stats_arr);

	data = kcalloc(lines, QED_RDMA_STRING_LEN, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	rc = dev->ops->rdma_get_stats_queue(dev->rdma_ctx, &stats_queue);
	if (rc) {
		res = -EFAULT;
		goto out;
	}

	rc = dev->ops->rdma_query_stats(dev->rdma_ctx, stats_queue, &stats);
	if (rc) {
		res = -EFAULT;
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(qed_rdma_stats_arr); i++) {
		res = sprintf(data + offset, "%*s: %llu\n",
			QED_RDMA_NAME_SPACE,
			qed_rdma_stats_arr[i].string,
			QED_RDMA_STATS_DATA(&stats, i));
		if (res < 0)
			break;
		offset += res;
	}

	if (IS_ROCE(dev)) {
		for (i = 0; i < ARRAY_SIZE(qed_roce_stats_arr); i++) {
			res = sprintf(data + offset, "%*s: %llu\n",
				      QED_RDMA_NAME_SPACE,
				      qed_roce_stats_arr[i].string,
				      QED_ROCE_STATS_DATA(&stats, i));
			if (res < 0)
				break;
			offset += res;
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(qed_iwarp_stats_arr); i++) {
			res = sprintf(data + offset, "%*s: %llu\n",
				      QED_RDMA_NAME_SPACE,
				      qed_iwarp_stats_arr[i].string,
				      QED_IWARP_STATS_DATA(&stats, i));
			if (res < 0)
				break;
			offset += res;
		}
	}

	if (strlen(data) > usr_buf_len) {
		res = -ENOSPC;
		goto out;
	}

	if (!is_sysfs)
		res = simple_read_from_buffer(buffer, usr_buf_len, ppos, data,
				strlen(data));
	else
		res = memory_read_from_buffer(buffer, usr_buf_len, ppos, data,
				strlen(data));

out:
	kfree(data);

	return res;
}

#ifdef _HAS_SYSFS_BIN_ATTR_INIT
static ssize_t sysfs_store(struct file *filp, struct kobject *kobj,
		struct bin_attribute *bin_attr, char *buffer,
		loff_t pos, size_t count)
{
	struct qedr_dev *dev = (struct qedr_dev *)bin_attr->private;

	if (strcmp(bin_attr->attr.name, "stats") == 0)
		return __qedr_write_stats(dev, buffer, count, &pos, true);
	return 0;
}
#endif

static ssize_t qedr_dbgfs_write_stats(struct file *filp,
				      const char __user *buffer,
				      size_t usr_buf_len, loff_t *ppos)
{
	struct qedr_dev *dev = filp->private_data;

	return __qedr_write_stats(dev, buffer, usr_buf_len, ppos, false);
}

static ssize_t __qedr_write_stats(struct qedr_dev *dev,
		const char __user *buffer,
		size_t usr_buf_len, loff_t *ppos,
		bool is_sysfs)
{
	char *buf;
	int len;

	buf = kmalloc(usr_buf_len + 1, GFP_KERNEL); /* +1 for '\0' */
	if (!buf) {
		DP_ERR(dev, "Allocation failed\n");
		goto err;
	}
	if (!dev)
		goto err;

	if (!is_sysfs) {
		len = simple_write_to_buffer(buf, usr_buf_len, ppos, buffer,
				usr_buf_len);
	} else {
		memcpy(buf, buffer, usr_buf_len);
		len = usr_buf_len;
	}

	if (len < 0) {
		DP_ERR(dev, "Copy from user failed\n");
		goto err;
	}

	buf[len - 1] = '\0';

	/* The only valid input is "0" */
	if (strlen(buf) != 1 || strncmp(buf, "0", 1)) {
		DP_ERR(dev,
		       "Invalid input - in order to reset stats need to write 0\n");
		goto err;
	}
	qedr_reset_stats(dev);
	kfree(buf);
	return len;
err:
	kfree(buf);
	return usr_buf_len;
}

static ssize_t qedr_dbgfs_read_gsi(struct file *filp, char __user *buffer,
				   size_t usr_buf_len, loff_t *ppos)
{
	struct qedr_dev *dev = filp->private_data;
	struct qed_ll2_stats ll2_stats;
	ssize_t offset = 0, res = 0;
	char *data;
	int i, rc;

	/* No partial reads */
	if (*ppos != 0)
		return 0;

	if (IS_IWARP(dev))
		return 0;

	data = kcalloc(ARRAY_SIZE(qed_ll2_stats_arr), QED_RDMA_STRING_LEN,
				  GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	rc = dev->ops->ll2_get_stats(dev->rdma_ctx, dev->gsi_ll2_handle,
				     &ll2_stats);
	if (rc) {
		res = -EFAULT;
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(qed_ll2_stats_arr); i++) {
		res = sprintf(data + offset, "%*s: %llu\n",
			      QED_RDMA_NAME_SPACE,
			      qed_ll2_stats_arr[i].string,
			      QED_LL2_STATS_DATA(&ll2_stats, i));
		if (res < 0)
			break;
		offset += res;
	}

	if (strlen(data) > usr_buf_len) {
		res = -ENOSPC;
		goto out;
	}

	res = simple_read_from_buffer(buffer, usr_buf_len, ppos, data,
				     strlen(data));
out:
	kfree(data);

	return res;
}

#define QEDR_STATS_CNQ_MAX		(256 + 2) /* MAX CNQs +2 for "\n\0" */
#define QEDR_STATS_CNQ_CHARS_MAX	(3 + 16) /* "0xff...ff */
static ssize_t qedr_dbgfs_read_cnq(struct file *filp, char __user *buffer,
				   size_t usr_buf_len, loff_t *ppos)
{
	struct qedr_dev *dev = filp->private_data;
	ssize_t offset = 0, res = 0;
	char *data;
	int i;

	/* No partial reads */
	if (*ppos != 0)
		return 0;

	data = kcalloc(QEDR_STATS_CNQ_MAX, QEDR_STATS_CNQ_CHARS_MAX,
		       GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	for (i = 0; i < dev->num_cnq; i++) {
		res = sprintf(data + offset, "0x%llx ",
			      dev->cnq_array[i].n_comp);
		if (res < 0)
			break;
		offset += res;
	}
	res = sprintf(data + offset, "\n");
	offset += res;

	if (strlen(data) > usr_buf_len) {
		res = -ENOSPC;
		goto out;
	}

	res = simple_read_from_buffer(buffer, usr_buf_len, ppos, data,
				     strlen(data));
out:
	kfree(data);

	return res;
}

#if DEFINE_THIS_CPU_INC
static ssize_t qedr_dbgfs_read_verbs(struct file *filp, char __user *buffer,
				     size_t usr_buf_len, loff_t *ppos)
{
	struct qedr_dev *dev = filp->private_data;
	struct qedr_stats qedr_stats;
	ssize_t offset = 0, res = 0;
	char *data;
	int i;

	/* No partial reads */
	if (*ppos != 0)
		return 0;

	data = kcalloc(ARRAY_SIZE(qedr_rdma_stats_arr), QED_RDMA_STRING_LEN,
		       GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	qedr_collect_stats(dev, &qedr_stats);
	for (i = 0; i < ARRAY_SIZE(qedr_rdma_stats_arr); i++) {
		res = sprintf(data + offset, "%*s: %llu\n",
			QED_RDMA_NAME_SPACE,
			qedr_rdma_stats_arr[i].string,
			QEDR_RDMA_STATS_DATA(&qedr_stats, i));
		if (res < 0)
			break;
		offset += res;
	}

	if (strlen(data) > usr_buf_len) {
		res = -ENOSPC;
		goto out;
	}

	res = simple_read_from_buffer(buffer, usr_buf_len, ppos, data,
				     strlen(data));
out:
	kfree(data);

	return res;
}
#endif

#define QED_ROCE_COUNTERS_OFFSET(counter_name) \
	(offsetof(struct qed_rdma_counters_out_params, counter_name))
#define QED_ROCE_COUNTER(counter_name) \
	 {QED_ROCE_COUNTERS_OFFSET(counter_name), #counter_name}
#define QED_ROCE_COUNTERS_DATA(counters, index) \
	(*((u64 *)(uintptr_t)(((u64) ((uintptr_t)counters)) + \
			qed_rdma_counters_arr[(index)].offset)))

static const struct {
	u64 offset;
	char string[QED_RDMA_STRING_LEN];
} qed_rdma_counters_arr[] = {
	QED_ROCE_COUNTER(pd_count),
	QED_ROCE_COUNTER(max_pd),
	QED_ROCE_COUNTER(dpi_count),
	QED_ROCE_COUNTER(max_dpi),
	QED_ROCE_COUNTER(cq_count),
	QED_ROCE_COUNTER(max_cq),
	QED_ROCE_COUNTER(qp_count),
	QED_ROCE_COUNTER(max_qp),
	QED_ROCE_COUNTER(tid_count),
	QED_ROCE_COUNTER(max_tid),
	QED_ROCE_COUNTER(srq_count),
	QED_ROCE_COUNTER(max_srq),
	QED_ROCE_COUNTER(xrc_srq_count),
	QED_ROCE_COUNTER(max_xrc_srq),
	QED_ROCE_COUNTER(xrcd_count),
	QED_ROCE_COUNTER(max_xrcd),
};

static ssize_t qedr_dbgfs_read_counters(struct file *filp, char __user *buffer,
					size_t usr_buf_len, loff_t *ppos)
{
	struct qedr_dev *dev = filp->private_data;
	struct qed_rdma_counters_out_params counters;
	ssize_t offset = 0, res = 0;
	char *data;
	int i, rc;

	/* No partial reads */
	if (*ppos != 0)
		return 0;

	data = kcalloc(ARRAY_SIZE(qed_rdma_counters_arr), QED_RDMA_STRING_LEN,
		       GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/* RoCE counters */
	rc = dev->ops->rdma_query_counters(dev->rdma_ctx, &counters);
	if (rc) {
		res = -EFAULT;
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(qed_rdma_counters_arr); i += 2) {
		res = sprintf(data + offset, "%*s: %llu/%llu\n",
			QED_RDMA_NAME_SPACE,
			qed_rdma_counters_arr[i].string,
			QED_ROCE_COUNTERS_DATA(&counters, i),
			QED_ROCE_COUNTERS_DATA(&counters, i+1));
		if (res < 0)
			break;
		offset += res;
	}

	if (strlen(data) > usr_buf_len) {
		res = -ENOSPC;
		goto out;
	}

	res = simple_read_from_buffer(buffer, usr_buf_len, ppos, data,
				     strlen(data));
out:
	kfree(data);

	return res;
}

#define QEDR_INFO_LINES	(3)
static ssize_t qedr_dbgfs_read_info(struct file *filp, char __user *buffer,
				    size_t usr_buf_len, loff_t *ppos)
{
	struct qedr_dev *dev = filp->private_data;
	ssize_t offset = 0, res = 0;
	char *data;

	/* No partial reads */
	if (*ppos != 0)
		return 0;

	data = kcalloc(QEDR_INFO_LINES, QED_RDMA_STRING_LEN, GFP_KERNEL);

	if (!data)
		return -ENOMEM;

	res = sprintf(data + offset, "%*s: 0x%08x\n", QED_RDMA_NAME_SPACE,
			"debug bitmap", dev->debug_msglvl);
	if (res < 0) {
		res = -EFAULT;
		goto out;
	}
	offset += res;

	res = sprintf(data + offset, "%*s: %d\n", QED_RDMA_NAME_SPACE,
			"wq multiplier", dev->wq_multiplier);
	if (res < 0) {
		res = -EFAULT;
		goto out;
	}
	offset += res;

	if (strlen(data) > usr_buf_len) {
		res = -ENOSPC;
		goto out;
	}

	res = simple_read_from_buffer(buffer, usr_buf_len, ppos, data,
				      strlen(data));
out:
	kfree(data);

	return res;
}

static ssize_t qedr_dbgfs_write_stub(struct file *filp,
				     const char __user *buffer,
				     size_t usr_buf_len, loff_t *ppos)
{
	pr_notice("No action is performed by writing to this file\n");
	return usr_buf_len;
}

#if DEFINE_SIMPLE_OPEN /* ! QEDR_UPSTREAM */
static inline int simple_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}
#endif

#define QEDR_DEBUGFS_OPS(name, read_func, write_func) \
	static const struct file_operations qedr_dbgfs_ops_##name = {	\
		.owner = THIS_MODULE,					\
		.open = simple_open,					\
		.read = read_func,					\
		.write = write_func,					\
	}

#define QEDR_DEBUGFS_OPS_READ_WRITE(name) \
	QEDR_DEBUGFS_OPS(name, qedr_dbgfs_read_##name, qedr_dbgfs_write_##name)

#define QEDR_DEBUGFS_OPS_READ(name) \
	QEDR_DEBUGFS_OPS(name, qedr_dbgfs_read_##name, qedr_dbgfs_write_stub)

QEDR_DEBUGFS_OPS_READ_WRITE(stats);
QEDR_DEBUGFS_OPS_READ(gsi);
QEDR_DEBUGFS_OPS_READ(cnq);
QEDR_DEBUGFS_OPS_READ(counters);
QEDR_DEBUGFS_OPS_READ(info);

#if DEFINE_THIS_CPU_INC
QEDR_DEBUGFS_OPS_READ(verbs);
#endif

void qedr_debugfs_add_stats(struct qedr_dev *dev)
{
	if (!qedr_dbgfs_dir)
		return;

	dev->dbgfs = debugfs_create_dir(dev->ibdev.name, qedr_dbgfs_dir);

	if (!debugfs_create_file("stats", S_IRUSR|S_IWUSR, dev->dbgfs, dev,
				 &qedr_dbgfs_ops_stats))
		goto err;

	if (!debugfs_create_file("gsi", S_IRUSR, dev->dbgfs, dev,
				 &qedr_dbgfs_ops_gsi))
		goto err;

	if (!debugfs_create_file("cnq", S_IRUSR, dev->dbgfs, dev,
				 &qedr_dbgfs_ops_cnq))
		goto err;

#if DEFINE_THIS_CPU_INC
	if (!debugfs_create_file("verbs", S_IRUSR, dev->dbgfs, dev,
				 &qedr_dbgfs_ops_verbs))
		goto err;
#endif

	if (!debugfs_create_file("counters", S_IRUSR, dev->dbgfs, dev,
				 &qedr_dbgfs_ops_counters))
		goto err;

	if (!debugfs_create_file("info", S_IRUSR, dev->dbgfs, dev,
				 &qedr_dbgfs_ops_info))
		goto err;

	return;
err:

	debugfs_remove_recursive(dev->dbgfs);
	dev->dbgfs = NULL;
}

void qedr_debugfs_remove_stats(struct qedr_dev *dev)
{
	debugfs_remove_recursive(dev->dbgfs);
	dev->dbgfs = NULL;
}

void qedr_init_debugfs(void)
{
	/* create base directory */
	pr_notice("created qedr debugfs\n");
	qedr_dbgfs_dir = debugfs_create_dir("qedr", NULL);
}

void qedr_remove_debugfs(void)
{
	pr_notice("remove qedr debugfs\n");
	debugfs_remove_recursive(qedr_dbgfs_dir);
}
#endif /* CONFIG_DEBUG_FS */

#ifdef _HAS_SYSFS_BIN_ATTR_INIT
/* sysfs functions */
void qedr_remove_sysfs(void)
{
	pr_notice("destroying qedr sysfs root entry\n");
	kobject_del(qedr_sysfs_root);
	qedr_sysfs_root = NULL;
}

void qedr_sysfs_remove_stats(struct qedr_dev *dev)
{
	struct qedr_sysfs_obj *info = NULL;
	struct qedr_sysfs_obj *temp = NULL;

	/* remove ysfs entries and memory allocated of this */
	kobject_del(dev->sysfs);
	list_for_each_entry_safe_reverse(info, temp,
					 &qedr_sysfs_stats_head,
					 list)
	{
		if (info->sysfs ==  dev->sysfs) {
			list_del(&info->list);
			kfree(info);
			break;
		}
	}
	dev->sysfs = NULL;
}

void qedr_init_sysfs(void)
{
	pr_notice("created qedr sysfs\n");
	/* create only once */
	if (!qedr_sysfs_root)
		qedr_sysfs_root = kobject_create_and_add("qedr", NULL);
	INIT_LIST_HEAD(&qedr_sysfs_stats_head);
}

void qedr_sysfs_add_stats(struct qedr_dev *dev)
{
	struct qedr_sysfs_obj *stats_info = NULL;

	dev->sysfs = NULL;
	/* skip if /sys/qedr failed */
	if (!qedr_sysfs_root)
		return;
	/* sysfs entries creation */
	stats_info = kzalloc(sizeof(*stats_info), GFP_KERNEL);
	if (!stats_info) {
		pr_err("sysfs memory allocation failed\n");
		goto err;
	}
	__init_sysfs(stats);
	/* /sys/qedr/qedr%d/ */
	dev->sysfs = kobject_create_and_add(dev->ibdev.name, qedr_sysfs_root);
	if (!dev->sysfs) {
		kfree(stats_info);
		goto err;
	}
	/* /sys/qedr/qedr%d/stats */
	stats_info->bin_attr.attr.name = "stats";
	if (sysfs_create_bin_file(dev->sysfs,  &stats_info->bin_attr)) {
		kobject_del(dev->sysfs);
		kfree(stats_info);
		goto err;
	}
	stats_info->sysfs = dev->sysfs;
	list_add_tail(&stats_info->list, &qedr_sysfs_stats_head);
	return;
err:
	pr_notice("qedr sysfs stats creation failed\n");
	dev->sysfs = NULL;
}
#else
void qedr_init_sysfs(void) {}
void qedr_remove_sysfs(void) {}
void qedr_sysfs_add_stats(struct qedr_dev *dev) {}
void qedr_sysfs_remove_stats(struct qedr_dev *dev) {}
#endif
