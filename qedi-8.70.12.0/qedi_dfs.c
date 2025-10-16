/*
 *  QLogic iSCSI Offload Driver
 *  Copyright (c) 2015-2018 Cavium Inc.
 *
 *  See LICENSE.qedi for copyright and licensing details.
 */

#include "qedi_dbg.h"

#ifdef CONFIG_DEBUG_FS
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/module.h>

static struct dentry *qedi_dbg_root;

/**
 * qedi_dbg_host_init - setup the debugfs file for the pf
 * @pf: the pf that is starting up
 **/
void
qedi_dbg_host_init(struct qedi_dbg_ctx *qedi,
		   struct qedi_debugfs_ops *dops,
		   struct file_operations *fops)
{
	char host_dirname[32];
	struct dentry *file_dentry = NULL;

	QEDI_INFO(qedi, QEDI_LOG_DEBUGFS, "Creating debugfs host node\n");
	/* create pf dir */
	sprintf(host_dirname, "host%u", qedi->host_no);
	qedi->bdf_dentry = debugfs_create_dir(host_dirname, qedi_dbg_root);
	if (!qedi->bdf_dentry)
		return;

	/* create debugfs files */
	while (dops) {
		if (!(dops->name))
			break;

		file_dentry = debugfs_create_file(dops->name, 0600,
						  qedi->bdf_dentry, qedi,
						  fops);
		if (!file_dentry) {
			QEDI_INFO(qedi, QEDI_LOG_DEBUGFS,
				  "Debugfs entry %s creation failed\n",
				  dops->name);
			debugfs_remove_recursive(qedi->bdf_dentry);
			return;
		}
		dops++;
		fops++;
	}
}

/**
 * qedi_dbg_host_exit - clear out the pf's debugfs entries
 * @pf: the pf that is stopping
 **/
void
qedi_dbg_host_exit(struct qedi_dbg_ctx *qedi)
{
	QEDI_INFO(qedi, QEDI_LOG_DEBUGFS, "Destroying debugfs host entry\n");
	/* remove debugfs  entries of this PF */
	debugfs_remove_recursive(qedi->bdf_dentry);
	qedi->bdf_dentry = NULL;
}

/**
 * qedi_dbg_init - start up debugfs for the driver
 **/
void
qedi_dbg_init(char *drv_name)
{
	QEDI_INFO(NULL, QEDI_LOG_DEBUGFS, "Creating debugfs root node\n");

	/* create qed dir in root of debugfs. NULL means debugfs root */
	qedi_dbg_root = debugfs_create_dir(drv_name, NULL);
	if (!qedi_dbg_root)
		QEDI_INFO(NULL, QEDI_LOG_DEBUGFS, "Init of debugfs failed\n");
}

/**
 * qedi_dbg_exit - clean out the driver's debugfs entries
 **/
void
qedi_dbg_exit(void)
{
	QEDI_INFO(NULL, QEDI_LOG_DEBUGFS, "Destroying debugfs root entry\n");

	/* remove qed dir in root of debugfs */
	debugfs_remove_recursive(qedi_dbg_root);
	qedi_dbg_root = NULL;
}

#else /* CONFIG_DEBUG_FS */
void qedi_dbg_host_init(struct qedi_dbg_ctx *);
void qedi_dbg_host_exit(struct qedi_dbg_ctx *);
void qedi_dbg_init(char *);
void qedi_dbg_exit(void);
#endif /* CONFIG_DEBUG_FS */
