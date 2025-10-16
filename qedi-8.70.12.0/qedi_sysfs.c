/*
 *  QLogic iSCSI Offload Driver
 *  Copyright (c) 2015-2018 Cavium Inc.
 *
 *  See LICENSE.qedi for copyright and licensing details.
 */

#include "qedi.h"
#include "qedi_gbl.h"
#include "qedi_iscsi.h"
#include "qedi_dbg.h"

static inline struct qedi_ctx *qedi_dev_to_hba(struct device *dev)
{
	struct Scsi_Host *shost = class_to_shost(dev);

	return iscsi_host_priv(shost);
}

static ssize_t
qedi_sysfs_read_grcdump(struct file *filep, struct kobject *kobj,
			struct bin_attribute *ba, char *buf, loff_t off,
			size_t count)
{
	struct qedi_ctx *qedi = NULL;
	ssize_t ret = 0;

	qedi = iscsi_host_priv(dev_to_shost(container_of(kobj, struct device,
							 kobj)));

	if (test_bit(QEDI_GRCDUMP_CAPTURE, &qedi->flags)) {
		ret = memory_read_from_buffer(buf, count, &off,
					      qedi->grcdump,
					      qedi->grcdump_size);
	} else {
		QEDI_ERR(&qedi->dbg_ctx, "GRC Dump not captured!\n");
	}

	return ret;
}

static ssize_t
qedi_sysfs_write_grcdump(struct file *filep, struct kobject *kobj,
			 struct bin_attribute *ba, char *buf, loff_t off,
			 size_t count)
{
	struct qedi_ctx *qedi = NULL;
	long reading;
	int ret = 0;
	char msg[40];

	if (off != 0)
		return ret;

	qedi = iscsi_host_priv(dev_to_shost(container_of(kobj, struct device,
							 kobj)));
	buf[1] = 0;
	ret = kstrtol(buf, 10, &reading);
	if (ret) {
		QEDI_ERR(&qedi->dbg_ctx, "Invalid input, err(%d)\n", ret);
		return ret;
	}

	memset(msg, 0, sizeof(msg));
	switch (reading) {
	case 0:
		memset(qedi->grcdump, 0, qedi->grcdump_size);
		clear_bit(QEDI_GRCDUMP_CAPTURE, &qedi->flags);
		break;
	case 1:
		qedi_capture_grc_dump(qedi);
		break;
	}

	return count;
}

static struct bin_attribute sysfs_grcdump_attr = {
	.attr = {
		.name = "grcdump",
		.mode = S_IRUSR | S_IWUSR,
	},
	.size = 0,
	.read = qedi_sysfs_read_grcdump,
	.write = qedi_sysfs_write_grcdump,
};

static struct sysfs_bin_attrs bin_file_entries[] = {
	{"grcdump", &sysfs_grcdump_attr},
	{NULL},
};

int qedi_create_sysfs_ctx_attr(struct qedi_ctx *qedi)
{
	return qedi_create_sysfs_attr(qedi->shost, bin_file_entries);
}

void qedi_remove_sysfs_ctx_attr(struct qedi_ctx *qedi)
{
	qedi_remove_sysfs_attr(qedi->shost, bin_file_entries);
}

void qedi_capture_grc_dump(struct qedi_ctx *qedi)
{
	if (!test_bit(QEDI_GRCDUMP_SETUP, &qedi->flags)) {
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
			  "GRC Dump capture not setup\n");
		return;
	}

	if (test_bit(QEDI_GRCDUMP_CAPTURE, &qedi->flags)) {
		QEDI_INFO(&qedi->dbg_ctx, QEDI_LOG_INFO,
			  "GRC Dump already captured\n");
		return;
	}

	qedi_get_grc_dump(qedi->cdev, qedi_ops->common, &qedi->grcdump,
			  &qedi->grcdump_size);
	set_bit(QEDI_GRCDUMP_CAPTURE, &qedi->flags);
	qedi_uevent_emit(qedi->shost, QEDI_UEVENT_CODE_GRCDUMP, NULL);
}

static ssize_t port_state_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct qedi_ctx *qedi = qedi_dev_to_hba(dev);

	if (atomic_read(&qedi->link_state) == QEDI_LINK_UP)
		return sprintf(buf, "Online\n");
	else
		return sprintf(buf, "Linkdown\n");
}

static ssize_t speed_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct qedi_ctx *qedi = qedi_dev_to_hba(dev);
	struct qed_link_output if_link;

	qedi_ops->common->get_link(qedi->cdev, &if_link);

	return sprintf(buf, "%d Gbit\n", if_link.speed / 1000);
}

static DEVICE_ATTR_RO(port_state);
static DEVICE_ATTR_RO(speed);


struct device_attribute *qedi_shost_attrs[] = {
	&dev_attr_port_state,
	&dev_attr_speed,
	NULL
};

