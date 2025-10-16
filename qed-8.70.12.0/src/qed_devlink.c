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

#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <net/devlink.h>
#define __PREVENT_PXP_GLOBAL_WIN__
#include "qed_if.h"
#include "qed.h"
#include "qed_devlink.h"
#include "qed_dev_api.h"
#include "qed_mcp.h"
#include "qed_compat.h"
#include "qed_sriov.h"

enum qed_devlink_param_id {
	QED_DEVLINK_PARAM_ID_BASE = QED_DEVLINK_PARAM_ID_START,
	QED_DEVLINK_PARAM_ID_IWARP_CMT,

	/* add param IDs above */
	QED_DEVLINK_PARAM_ID_MAX = QED_DEVLINK_PARAM_ID_END,
};

struct qed_fw_fatal_ctx {
	enum qed_hw_err_type err_type;
	int recov_enable;
};

int qed_report_fatal_error(struct devlink *devlink,
			   enum qed_hw_err_type err_type, int recov_enable)
{
	struct qed_devlink *qdl = devlink_priv(devlink);
	struct qed_fw_fatal_ctx fw_fatal_ctx = {
		.err_type = err_type,
		.recov_enable = recov_enable,
	};

	if (qdl->fw_reporter)
		devlink_health_report(qdl->fw_reporter,
				      "Fatal error occurred", &fw_fatal_ctx);

	return 0;
}

#ifdef _HAS_DEVLINK_DUMP /* QEDE_UPSTREAM */
static int
qed_fw_fatal_reporter_dump(struct devlink_health_reporter *reporter,
			   struct devlink_fmsg *fmsg, void *priv_ctx,
			   struct netlink_ext_ack *extack)
{
	struct qed_devlink *qdl = devlink_health_reporter_priv(reporter);
	struct qed_fw_fatal_ctx *fw_fatal_ctx = priv_ctx;
	struct qed_dev *cdev = qdl->cdev;

	int err;

	/* Having context means that was a dump request after fatal,
	 * so we enable extra debugging while gathering the dump,
	 * just in case
	 */
	qed_dbg_save_all_data(cdev, fw_fatal_ctx ? true : false);

	if (!cdev->p_dbg_data_buf) {
		DP_NOTICE(cdev, "Failed to obtain debug data\n");
		return -ENODATA;
	}

	err = devlink_fmsg_binary_pair_put(fmsg, "dump_data",
					   cdev->p_dbg_data_buf,
					   cdev->dbg_data_buf_size);

	return err;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 5, 0) || RHEL_STARTING_AT_VERSION(8, 3) || SLES_STARTING_AT_VERSION(SLES15_SP2)) /* QEDE_UPSTREAM */
static int
qed_fw_fatal_reporter_recover(struct devlink_health_reporter *reporter,
			      void *priv_ctx,
			      struct netlink_ext_ack *extack)
#else
static int
qed_fw_fatal_reporter_recover(struct devlink_health_reporter *reporter,
			      void *priv_ctx)
#endif
{
	struct qed_devlink *qdl = devlink_health_reporter_priv(reporter);
	struct qed_fw_fatal_ctx *fw_fatal_ctx = priv_ctx;
	struct qed_dev *cdev = qdl->cdev;

	/* Dont enable devlink recovery if override flag is used */
	if (fw_fatal_ctx->recov_enable)
		qed_recovery_process(cdev);
	else
		DP_NOTICE(cdev, "Devlink recovery will not happen due to override\n");

	return 0;
}

static const struct devlink_health_reporter_ops qed_fw_fatal_reporter_ops = {
		.name = "fw_fatal",
		.recover = qed_fw_fatal_reporter_recover,
#ifdef _HAS_DEVLINK_DUMP /* QEDE_UPSTREAM */
		.dump = qed_fw_fatal_reporter_dump,
#endif
};

#define QED_REPORTER_FW_GRACEFUL_PERIOD 0

void qed_fw_reporters_create(struct devlink *devlink)
{
	struct qed_devlink *dl = devlink_priv(devlink);

	dl->fw_reporter = devlink_health_reporter_create(devlink, &qed_fw_fatal_reporter_ops,
							 QED_REPORTER_FW_GRACEFUL_PERIOD, dl);
	if (IS_ERR(dl->fw_reporter)) {
		DP_NOTICE(dl->cdev, "Failed to create fw reporter, err = %ld\n",
			  PTR_ERR(dl->fw_reporter));
		dl->fw_reporter = NULL;
	}
}

void qed_fw_reporters_destroy(struct devlink *devlink)
{
	struct qed_devlink *dl = devlink_priv(devlink);
	struct devlink_health_reporter *rep;

	rep = dl->fw_reporter;

	if (!IS_ERR_OR_NULL(rep))
		devlink_health_reporter_destroy(rep);
}

static int qed_dl_param_get_iwarp_cmt(struct devlink *dl, u32 id,
				      struct devlink_param_gset_ctx *ctx)
{
	struct qed_devlink *qed_dl = devlink_priv(dl);
	struct qed_dev *cdev;

	cdev = qed_dl->cdev;
	ctx->val.vbool = cdev->iwarp_cmt;

	return 0;
}

static int qed_dl_param_set_iwarp_cmt(struct devlink *dl, u32 id,
				      struct devlink_param_gset_ctx *ctx)
{
	struct qed_devlink *qed_dl = devlink_priv(dl);
	struct qed_dev *cdev;

	cdev = qed_dl->cdev;
	cdev->iwarp_cmt = ctx->val.vbool;

	return 0;
}

static const struct devlink_param qed_iwarp_devlink_params[] = {
	DEVLINK_PARAM_DRIVER(QED_DEVLINK_PARAM_ID_IWARP_CMT,
			     "iwarp_cmt", DEVLINK_PARAM_TYPE_BOOL,
			     BIT(DEVLINK_PARAM_CMODE_RUNTIME),
			     qed_dl_param_get_iwarp_cmt,
			     qed_dl_param_set_iwarp_cmt, NULL),
};

static int qed_devlink_info_get(struct devlink *devlink,
				struct devlink_info_req *req,
				struct netlink_ext_ack *extack)
{
	struct qed_devlink *qed_dl = devlink_priv(devlink);
	struct qed_dev *cdev = qed_dl->cdev;
	struct qed_dev_info *dev_info;
	struct qed_hwfn *hwfn;
	char buf[100];
	int err;

	dev_info = &cdev->common_dev_info;
	hwfn = QED_LEADING_HWFN(cdev);

	err = devlink_info_driver_name_put(req, KBUILD_MODNAME);
	if (err)
		return err;

	memcpy(buf, hwfn->hw_info.part_num, sizeof(hwfn->hw_info.part_num));
	buf[sizeof(hwfn->hw_info.part_num)] = 0;

	if (buf[0]) {
		err = devlink_info_board_serial_number_put(req, buf);
		if (err)
			return err;
	}

	snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
		 GET_MFW_FIELD(dev_info->mfw_rev, QED_MFW_VERSION_3),
		 GET_MFW_FIELD(dev_info->mfw_rev, QED_MFW_VERSION_2),
		 GET_MFW_FIELD(dev_info->mfw_rev, QED_MFW_VERSION_1),
		 GET_MFW_FIELD(dev_info->mfw_rev, QED_MFW_VERSION_0));

	err = devlink_info_version_stored_put(req, DEVLINK_INFO_VERSION_GENERIC_FW_MGMT, buf);
	if (err)
		return err;

	snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
		 dev_info->fw_major,
		 dev_info->fw_minor,
		 dev_info->fw_rev,
		 dev_info->fw_eng);

	err = devlink_info_version_running_put(req, DEVLINK_INFO_VERSION_GENERIC_FW_APP, buf);
	if (err)
		return err;

	return devlink_info_version_running_put(req, DEVLINK_INFO_VERSION_GENERIC_FW_ROCE, buf);
}

static const struct devlink_ops qed_dl_ops = {
	.info_get = qed_devlink_info_get,
};

struct devlink *qed_devlink_register(struct qed_dev *cdev, void *drv_ctx)
{
	union devlink_param_value value;
	struct qed_devlink *qdevlink;
	struct devlink *dl;
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	int rc;

#ifdef _HAS_DEVLINK_STRUCT_DEV_DEVLINK_REGISTER
	dl = devlink_alloc(&qed_dl_ops, sizeof(struct qed_devlink));
#else
	dl = devlink_alloc(&qed_dl_ops, sizeof(struct qed_devlink),
			   &cdev->pdev->dev);
#endif
	if (!dl)
		return ERR_PTR(-ENOMEM);

	qdevlink = devlink_priv(dl);
	qdevlink->cdev = cdev;
	qdevlink->drv_ctx = drv_ctx;

#ifdef _HAS_DEVLINK_PARAMS_PUBLISH
	rc = __qed_devlink_register((void *)dl, &cdev->pdev->dev);
	if (rc)
		goto err_free;
#endif

	if (!IS_VF(cdev)) {
		if (QED_IS_IWARP_PERSONALITY(p_hwfn)) {
			rc = devlink_params_register(dl,
						     qed_iwarp_devlink_params,
						     ARRAY_SIZE(qed_iwarp_devlink_params));
			if (rc)
				goto err_unregister;

			value.vbool = false;
			cdev->iwarp_cmt = false;
			devlink_param_driverinit_value_set(dl,
							   QED_DEVLINK_PARAM_ID_IWARP_CMT,
							   value);
		}

#ifdef _HAS_DEVLINK_PARAMS_PUBLISH
		devlink_params_publish(dl);
#endif
	}

	qed_fw_reporters_create(dl);

#ifndef _HAS_DEVLINK_PARAMS_PUBLISH
	rc = __qed_devlink_register(dl, &cdev->pdev->dev);
	if (rc)
		goto err_free;
#endif

	return dl;

err_unregister:
#ifdef _HAS_DEVLINK_PARAMS_PUBLISH
	devlink_unregister(dl);
#endif

err_free:
	devlink_free(dl);

	return ERR_PTR(rc);
}

void qed_devlink_unregister(struct devlink *devlink)
{
	struct qed_devlink *qed_dl;
	struct qed_dev *cdev;
	struct qed_hwfn *p_hwfn;

	if (!devlink)
		return;

	qed_dl = devlink_priv(devlink);
	cdev = qed_dl->cdev;
	p_hwfn = QED_LEADING_HWFN(cdev);

#ifndef _HAS_DEVLINK_PARAMS_PUBLISH
	devlink_unregister(devlink);
#endif
	qed_fw_reporters_destroy(devlink);

	if (!IS_VF(cdev)) {
		if (QED_IS_IWARP_PERSONALITY(p_hwfn)) {
			devlink_params_unregister(devlink,
						  qed_iwarp_devlink_params,
						  ARRAY_SIZE(qed_iwarp_devlink_params));
		}
	}

#ifdef _HAS_DEVLINK_PARAMS_PUBLISH
	devlink_unregister(devlink);
#endif
	devlink_free(devlink);
}
