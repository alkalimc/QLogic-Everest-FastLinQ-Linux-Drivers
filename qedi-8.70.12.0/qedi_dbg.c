/*
 *  QLogic iSCSI Offload Driver
 *  Copyright (c) 2015-2018 Cavium Inc.
 *
 *  See LICENSE.qedi for copyright and licensing details.
 */

#include "qedi_dbg.h"
#include <linux/vmalloc.h>

void
qedi_dbg_err(struct qedi_dbg_ctx *qedi, const char *func, u32 line,
	     const char *fmt, ...)
{
	va_list va;
	struct va_format vaf;

	va_start(va, fmt);

	vaf.fmt = fmt;
	vaf.va = &va;

	if (likely(qedi) && likely(qedi->pdev))
		pr_crit("[%s]:[%s:%d]:%d: %pV", dev_name(&qedi->pdev->dev),
			func, line, qedi->host_no, &vaf);
	else
		pr_crit("[0000:00:00.0]:[%s:%d]: %pV", func, line, &vaf);

	va_end(va);
}

void
qedi_dbg_warn(struct qedi_dbg_ctx *qedi, const char *func, u32 line,
	      const char *fmt, ...)
{
	va_list va;
	struct va_format vaf;

	va_start(va, fmt);

	vaf.fmt = fmt;
	vaf.va = &va;

	if (!(qedi_dbg_log & QEDI_LOG_WARN))
		goto ret;

	if (likely(qedi) && likely(qedi->pdev))
		pr_warn("[%s]:[%s:%d]:%d: %pV", dev_name(&qedi->pdev->dev),
			func, line, qedi->host_no, &vaf);
	else
		pr_warn("[0000:00:00.0]:[%s:%d]: %pV", func, line, &vaf);

ret:
	va_end(va);
}

void
qedi_dbg_notice(struct qedi_dbg_ctx *qedi, const char *func, u32 line,
		const char *fmt, ...)
{
	va_list va;
	struct va_format vaf;

	va_start(va, fmt);

	vaf.fmt = fmt;
	vaf.va = &va;

	if (!(qedi_dbg_log & QEDI_LOG_NOTICE))
		goto ret;

	if (likely(qedi) && likely(qedi->pdev))
		pr_notice("[%s]:[%s:%d]:%d: %pV",
			  dev_name(&qedi->pdev->dev), func, line,
			  qedi->host_no, &vaf);
	else
		pr_notice("[0000:00:00.0]:[%s:%d]: %pV", func, line, &vaf);

ret:
	va_end(va);
}

void
qedi_dbg_info(struct qedi_dbg_ctx *qedi, const char *func, u32 line,
	      u32 level, const char *fmt, ...)
{
	va_list va;
	struct va_format vaf;

	va_start(va, fmt);

	vaf.fmt = fmt;
	vaf.va = &va;

	if (!(qedi_dbg_log & level))
		goto ret;

	if (likely(qedi) && likely(qedi->pdev))
		pr_info("[%s]:[%s:%d]:%d: %pV", dev_name(&qedi->pdev->dev),
			func, line, qedi->host_no, &vaf);
	else
		pr_info("[0000:00:00.0]:[%s:%d]: %pV", func, line, &vaf);

ret:
	va_end(va);
}

int
qedi_alloc_grc_dump_buf(u8 **buf, uint32_t len)
{
		*buf = vmalloc(len);
		if (!(*buf))
			return -ENOMEM;

		memset(*buf, 0, len);
		return 0;
}

void
qedi_free_grc_dump_buf(uint8_t **buf)
{
		vfree(*buf);
		*buf = NULL;
}

int
qedi_get_grc_dump(struct qed_dev *cdev, const struct qed_common_ops *common,
		  u8 **buf, uint32_t *grcsize)
{
	if (!*buf)
		return -EINVAL;

	return common->dbg_all_data(cdev, *buf);
}

void
qedi_uevent_emit(struct Scsi_Host *shost, u32 code, char *msg)
{
	char event_string[40];
	char *envp[] = {event_string, NULL};

	memset(event_string, 0, sizeof(event_string));
	switch (code) {
	case QEDI_UEVENT_CODE_GRCDUMP:
		if (msg)
			strlcpy(event_string, msg, strlen(msg));
		else
			sprintf(event_string, "GRCDUMP=%u", shost->host_no);
		break;
	default:
		/* do nothing */
		break;
	}

	kobject_uevent_env(&shost->shost_gendev.kobj, KOBJ_CHANGE, envp);
}

int
qedi_create_sysfs_attr(struct Scsi_Host *shost, struct sysfs_bin_attrs *iter)
{
	int ret = 0;

	for (; iter->name; iter++) {
		ret = sysfs_create_bin_file(&shost->shost_gendev.kobj,
					    iter->attr);
		if (ret)
			pr_err("Unable to create sysfs %s attr, err(%d).\n",
			       iter->name, ret);
	}
	return ret;
}

void
qedi_remove_sysfs_attr(struct Scsi_Host *shost, struct sysfs_bin_attrs *iter)
{
	for (; iter->name; iter++)
		sysfs_remove_bin_file(&shost->shost_gendev.kobj, iter->attr);
}
