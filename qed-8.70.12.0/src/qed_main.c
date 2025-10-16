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

#include <linux/stddef.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <asm/byteorder.h>
#include <linux/dma-mapping.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/ethtool.h>
#include <linux/etherdevice.h>
#include <linux/vmalloc.h>
#ifdef _HAS_KTIME_GET_REAL_SECONDS /* QED_UPSTREAM */
#include <linux/ktime.h>
#else
#include <linux/time.h>
#endif
#include <linux/crash_dump.h>
#include <linux/stat.h>
#include <linux/aer.h>
#include <linux/efi.h>

#define __PREVENT_DUMP_MEM_ARR__
#define __PREVENT_PXP_GLOBAL_WIN__
#define __PREVENT_COND_ARR__

#include "qed.h"
#include "qed_sriov.h"
#include "qed_sp.h"
#include "qed_dev_api.h"
#include "qed_ll2.h"
#include "qed_fcoe.h"
#include "qed_iscsi.h"

#include "qed_mcp.h"
#include "qed_reg_addr.h"

#include "qed_compat.h"
#include "qed_if.h"
#include "qed_eth_if.h"
#include "qed_ll2_if.h"
#include "qed_selftest.h"

#include "qed_hw.h"
#include "qed_rdma.h"
#include "qed_rdma_if.h"
#include "qed_phy_api.h"

#include "qed_debug.h"
#include "qed_dcbx.h"
#include "qed_devlink.h"

static char version[] =
	"QLogic FastLinQ 4xxxx Core Module " DRV_MODULE_NAME " " DRV_MODULE_VERSION;

MODULE_DESCRIPTION("QLogic FastLinQ 4xxxx Core Module");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_MODULE_VERSION);

static uint int_debug = QED_DP_INT_LOG_DEFAULT_MASK;
module_param(int_debug, uint, S_IRUGO);
MODULE_PARM_DESC(int_debug, " Default internal debug msglevel");

static uint int_debug_size = QED_INT_DEBUG_SIZE_DEF;
module_param(int_debug_size, uint, S_IRUGO);
MODULE_PARM_DESC(int_debug_size, " Internal debug buffer size");

#ifndef QED_UPSTREAM
static uint npar_tx_switching = 1;
module_param(npar_tx_switching, uint, S_IRUGO);
MODULE_PARM_DESC(npar_tx_switching, " Enable(1)/Disable(0) NPAR tx switching [Enabled by default]");

static uint qed_pkt_pacing;
module_param(qed_pkt_pacing, uint, S_IRUGO);
MODULE_PARM_DESC(qed_pkt_pacing, " Enable(1)/Disable(0) qed_pkt_pacing [Disabled by default]");

static uint tx_switching = 1;
module_param(tx_switching, uint, S_IRUGO);
MODULE_PARM_DESC(tx_switching, " Enable(1)/Disable(0) per function tx switching [Enabled by default]");

static uint personality = QED_PCI_DEFAULT;
module_param(personality, uint, S_IRUGO);
MODULE_PARM_DESC(personality, " ETH=0, FCOE=1, ISCSI=2, ROCE=3, IWARP=4");

static uint pci_relax_order = QED_DEFAULT_RLX_ODR;
module_param(pci_relax_order, uint, S_IRUGO);
MODULE_PARM_DESC(pci_relax_order, " Do nothing=0, Enable=1, Disable=2 PCI relax ordering [Do nothing by default]");

static uint rx_asymmetric_bw_mode;
module_param(rx_asymmetric_bw_mode, uint, S_IRUGO);
MODULE_PARM_DESC(rx_asymmetric_bw_mode, " Fair bandwidth treatment for all ports=0 [Default], Low index ports get high bandwidth=1,\
		 High index ports get high bandwidth=2, Even ports get high bandwidth=3, Odd ports get high bandwidth=4");

enum {
	QED_MAX_DEVICES        = 32,
	QED_DEVS_TBL_SIZE      = QED_MAX_DEVICES + 1,
	QED_BDF2VAL_STR_SIZE   = 512,
	QED_ENDOF_TBL          = -1LL
};

struct qed_bdf2val {
	u32 dbdf;
	int val;
};

static struct qed_bdf2val	qed_load_function_tbl[QED_DEVS_TBL_SIZE];
static char			qed_load_function_str[QED_BDF2VAL_STR_SIZE];

module_param_string(load_function_map, qed_load_function_str,
		    sizeof(qed_load_function_str), S_IRUGO);

MODULE_PARM_DESC(load_function_map,
		 " Determine which device functions will be loaded\n"
		 "\t\tstring lists loaded device function numbers (e.g. '02:00.0,02:01.0').\n"
		 "\t\tmaximum of 32 entries is supported");

static struct qed_bdf2val	rdma_protocol_map_tbl[QED_DEVS_TBL_SIZE];
static char			rdma_protocol_map_str[QED_BDF2VAL_STR_SIZE];

module_param_string(rdma_protocol_map, rdma_protocol_map_str,
		    sizeof(rdma_protocol_map_str), S_IRUGO);

MODULE_PARM_DESC(rdma_protocol_map,
		 " Determine the rdma protocol which will run on the device\n"
		 "\t\tstring maps device function numbers to their requested protocol (e.g. '02:00.0-1,02:01.0-2').\n"
		 "\t\tmaximum of 32 entries is supported\n"
		 "\t\tValid values types: 0-Take default (what's configured on board, favors roce over iwarp) 1-none, 2-roce, 3-iwarp");

static uint drv_resc_alloc;
module_param(drv_resc_alloc, uint, S_IRUGO);
MODULE_PARM_DESC(drv_resc_alloc, " Force the driver's default resource allocation (0 do-not-force (default); 1 force)");

static uint chk_reg_fifo;
module_param(chk_reg_fifo, uint, S_IRUGO);
MODULE_PARM_DESC(chk_reg_fifo, " Check the reg_fifo after any register access (0 do-not-check (default); 1 check)");

static uint initiate_pf_flr = 1;
module_param(initiate_pf_flr, uint, S_IRUGO);
MODULE_PARM_DESC(initiate_pf_flr, " Initiate PF FLR as part of driver load (0 do-not-initiate; 1 initiate (default))");

static uint allow_mdump;
module_param(allow_mdump, uint, S_IRUGO);
MODULE_PARM_DESC(allow_mdump, " Allow the MFW to collect a crash dump (0 do-not-allow (default); 1 allow)");

static uint loopback_mode;
module_param(loopback_mode, uint, S_IRUGO);
MODULE_PARM_DESC(loopback_mode, " Force a loopback mode (0 no-loopback (default))");

static uint avoid_eng_reset;
module_param(avoid_eng_reset, uint, S_IRUGO);
MODULE_PARM_DESC(avoid_eng_reset, " Avoid engine reset when first PF loads on it (0 do-not-avoid (default); 1 avoid)");

static uint override_force_load;
module_param(override_force_load, uint, S_IRUGO);
MODULE_PARM_DESC(override_force_load, " Override the default force load behavior (0 do-not-override (default); 1 always; 2 never)");

static uint wc_disabled;
module_param(wc_disabled, uint, S_IRUGO);
MODULE_PARM_DESC(wc_disabled, " Write combine enabled/disabled (0 enabled (default); 1 disabled) (When disabling WC consider disabling EDPM too, via the module parameter roce_edpm, otherwise many EDPM failures can appear, resulting in even worse latency)");

static uint limit_msix_vectors;
module_param(limit_msix_vectors, uint, S_IRUGO);
MODULE_PARM_DESC(limit_msix_vectors, " Upper limit value for the requested number of MSI-X vectors. A value of 0 means no limit.");

static uint limit_l2_queues;
module_param(limit_l2_queues, uint, S_IRUGO);
MODULE_PARM_DESC(limit_l2_queues, " Upper limit value for the number of L2 queues. A value of 0 means no limit.");

static uint avoid_eng_affin;
module_param(avoid_eng_affin, uint, S_IRUGO);
MODULE_PARM_DESC(avoid_eng_affin, " Avoid engine affinity for RoCE/storage in case of CMT mode (0 do-not-avoid (default); 1 avoid)");

static uint ilt_page_size = 6;
module_param(ilt_page_size, uint, S_IRUGO);
MODULE_PARM_DESC(ilt_page_size, " Set the ILT page size (6 (default); allowed range for page sizes [4 - 10])");

static uint allow_vf_mac_change_mode = 0;
module_param(allow_vf_mac_change_mode, uint, S_IRUGO);
MODULE_PARM_DESC(allow_vf_mac_change_mode, " Allow VF to change MAC despite PF set force MAC (0 Disable (default); 1 Enable))");

static uint vf_mac_origin;
module_param(vf_mac_origin, uint, S_IRUGO);
MODULE_PARM_DESC(vf_mac_origin, " Origin of the initial VF MAC address (0 - nvram if available else zero (default); 1 - nvram if available else random; 2 - zero; 3 - random)");

static uint monitored_hw_addr;
module_param(monitored_hw_addr, uint, S_IRUGO);
MODULE_PARM_DESC(monitored_hw_addr, " Monitored address by ecore_rd()/ecore_wr()");

static uint periodic_db_rec;
module_param(periodic_db_rec, uint, S_IRUGO);
MODULE_PARM_DESC(periodic_db_rec, " Always run periodic Doorbell Overflow Recovery (0 Disable (default); 1 Enable)");

static uint roce_lag_delay;
module_param(roce_lag_delay, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(roce_lag_delay, " Delay (in msec) between DCBx negotiation and failback. When 0, do not wait for DCBx negotiation before failback. ");

#endif

#if IS_ENABLED(CONFIG_QED_RDMA) /* ! QED_UPSTREAM */
/* RoCE configurations
 * -------------------
 * The following must be supplied to QED during resource allocation as they
 * affect it, in specific, the ILT and the doorbell bar. Increasing the number
 * of QPs consumes ILT resources and increases the size of the normal region
 * in the doorbell bar.(TODO: update the following line if/when it becomes
 * obsolete) Increasing the number of DPIs increases the size of the PWM region
 * in the doorbell bar.
 * In practice the following algorithm is performed:
 *  1) Check if the number of QPs requested can reside in the ILT. If yes,
 *  2) Check if the number of QPs can reside in the normal region. If yes,
 *  3) Check if the remaining doorbell bar i.e. the PWM region, can contain
 *     enough DPIs. Note that the size of a DPI varies according to the number
 *     of CPUs. For more information Follow the algorithm itself
 * The actual numbers calculated according to the following use case:
 *  - Bar size is 512kB
 *  - Assume 8192 QPs => 16384 CIDs => Normal Region ~64kB (2048x4 byte DEMS)
 *    the tilde is to account for L2, and core CIDs.
 *  - This leaves ~448kB of BAR.
 *  - Assume 32 CPU cores, since WID size is 1kB for we get 448/32 ~= 14 DPIs.
 *    Hence we force at least 8.
 *  (*) The full logic of this calculation is within QED.
 */

#define QED_RDMA_QPS			(16384)
#define QED_VF_RDMA_QPS			(1024)
#define QED_VF_RDMA_TASKS		(16384)
#define QED_RDMA_DPIS			(8)
#define QED_RDMA_SRQS			QED_RDMA_QPS
#define QED_VF_RDMA_SRQS		QED_VF_RDMA_QPS
#define QED_RDMA_NUM_VF_CNQS		(80)
#define QED_RFS_MAX_FLTR		256

static uint num_rdma_qps = QED_RDMA_QPS;
module_param(num_rdma_qps, uint, S_IRUGO);
MODULE_PARM_DESC(num_rdma_qps, " The number of RDMA QPs is by default 16384 and can be raised ideally up to ~32k");

static uint num_vf_rdma_qps = QED_VF_RDMA_QPS;
module_param(num_vf_rdma_qps, uint, S_IRUGO);
MODULE_PARM_DESC(num_vf_rdma_qps, " The number of VF RDMA QPs is by default 1024 and can be raised ideally up to ~32k");

static uint num_vf_rdma_tasks = QED_VF_RDMA_TASKS;
module_param(num_vf_rdma_tasks, uint, S_IRUGO);
MODULE_PARM_DESC(num_vf_rdma_tasks, " The number of VF RDMA tasks is by default 1024 and can be raised ideally up to ~128k");

static uint num_vf_rdma_srqs = QED_VF_RDMA_SRQS;
module_param(num_vf_rdma_srqs, uint, S_IRUGO);
MODULE_PARM_DESC(num_vf_rdma_srqs, " The number of VF RDMA SRQs is by default 1024 and can be raised ideally up to ~32k");

static uint num_roce_srqs = QED_RDMA_SRQS;
module_param(num_roce_srqs, uint, S_IRUGO);
MODULE_PARM_DESC(num_roce_srqs, " The number of RoCE SRQs is by default 8192 and can be raised ideally up to ~32k");

static uint min_rdma_dpis = QED_RDMA_DPIS;
module_param(min_rdma_dpis, uint, S_IRUGO);
MODULE_PARM_DESC(min_rdma_dpis, " The minimum number of RDMA DPIs is by default 8 and can be lowered down to 4");

static uint roce_edpm;
module_param(roce_edpm, uint, S_IRUGO);
MODULE_PARM_DESC(roce_edpm, " The EDPM mode to load the driver with (0-Enable EDPM if BAR size is adequate, 1-Force EDPM (modprobe may fail on small BARs), 2-Disable EDPM)");

static uint dcqcn_enable;
module_param(dcqcn_enable, uint, S_IRUGO);
MODULE_PARM_DESC(dcqcn_enable, " enable roce dcqcn.");

static uint num_vf_cnqs = QED_RDMA_NUM_VF_CNQS;
module_param(num_vf_cnqs, uint, S_IRUGO);
MODULE_PARM_DESC(num_vf_cnqs, " num_vf_cnqs");

#endif

#define FW_FILE_VERSION				\
	__stringify(FW_MAJOR_VERSION) "."	\
	__stringify(FW_MINOR_VERSION) "."	\
	__stringify(FW_REVISION_VERSION) "."	\
	__stringify(FW_ENGINEERING_VERSION)

#ifdef CONFIG_QED_ZIPPED_FW
#define QED_FW_FILE_NAME	\
	"qed/qed_init_values_zipped-" FW_FILE_VERSION ".bin"
#ifndef QED_UPSTREAM /* ! QED_UPSTREAM */
#define QED_FW_FILE_NAME_DUD	\
	"qed_init_values_zipped-" FW_FILE_VERSION ".bin"
#endif
#else
#define QED_FW_FILE_NAME	\
	"qed/qed_init_values-" FW_FILE_VERSION ".bin"
#endif

MODULE_FIRMWARE(QED_FW_FILE_NAME);

#if defined(CONFIG_QED_RDMA) || !defined(QED_UPSTREAM) /* ! QED_UPSTREAM */
/* function up to 4 bits, device takes up to 8 bits */
#define QED_BDF_TO_DBDF(_bus, _dev, _fn) \
	(((_bus) << 12) | ((_dev) << 4) | (_fn))

static int qed_bdf2val_find(struct qed_bdf2val *bdf2val_map,
			    struct pci_dev *pdev)
{
	int val = bdf2val_map[0].val;
	int i = 1;
	u32 dbdf;

	if (!pdev || !pdev->bus)
		return val;

	dbdf = QED_BDF_TO_DBDF(pdev->bus->number, PCI_SLOT(pdev->devfn),
			       PCI_FUNC(pdev->devfn));

	while ((i < QED_DEVS_TBL_SIZE) &&
	       (bdf2val_map[i].dbdf != QED_ENDOF_TBL)) {
		if (bdf2val_map[i].dbdf == dbdf) {
			val = bdf2val_map[i].val;
			return val;
		}
		i++;
	}

	return val;
}
#endif

#ifndef QED_UPSTREAM
#define QED_LOAD_FUNCTION_MAP_SIZE strlen("xx:xx.x")

static int qed_fill_load_function_map(void)
{
	int bus, dev, fn, i = 1;
	u32 dbdf;
	char *p;

	p = qed_load_function_str;

	/* First entry will always be default in case no match is found */
	qed_load_function_tbl[0].val = 1;
	qed_load_function_tbl[1].dbdf = QED_ENDOF_TBL;

	if (!strlen(p))
		return 0;

	/* List is not empty, do not load all functions */
	qed_load_function_tbl[0].val = 0;

	while (strlen(p) >= QED_LOAD_FUNCTION_MAP_SIZE) {
		if (i >= QED_DEVS_TBL_SIZE) {
			pr_warn("qed module parameter load_functions: Too many devices\n");
			goto err;
		}

		if (sscanf(p, "%02x:%02x.%x", &bus, &dev, &fn) != 3) {
			/* expected 3 values matching the scan format */
			pr_err("qed module parameter load_functions: Invalid bdf: %s\n",
			       p);
			goto err;
		}

		dbdf = QED_BDF_TO_DBDF(bus, dev, fn);

		qed_load_function_tbl[i].val = 1;
		qed_load_function_tbl[i].dbdf = dbdf;

		p += QED_LOAD_FUNCTION_MAP_SIZE;
		if (strlen(p)) {
			if (*p == ',') {
				p++; /* separator */
			} else {
				pr_warn("qed module parameter load_functions: Separator invalid %c, expecting ,\n",
					*p);
				goto err;
			}
		}

		i++;
		if (i < QED_DEVS_TBL_SIZE)
			qed_load_function_tbl[i].dbdf = QED_ENDOF_TBL;
	}

	if (strlen(p)) {
		pr_warn("qed module parameter load_functions: Invalid value %s\n",
			p);
		goto err;
	}

	return 0;

err:
	qed_load_function_tbl[1].dbdf = QED_ENDOF_TBL;
	pr_warn("qed: The value of load_functions is incorrect!\n");
	return -EINVAL;
}
#endif

#ifdef CONFIG_QED_RDMA /* ! QED_UPSTREAM */
#define QED_RDMA_PROTOCOL_MAP_SIZE strlen("xx:xx.x-p")

static int qed_rdma_fill_protocol_map(void)
{
	int bus, dev, fn, protocol;
	int j, i = 1;
	u32 dbdf;
	char *p;

	p = rdma_protocol_map_str;

	/* First entry will always be default in case no match is found */
	rdma_protocol_map_tbl[0].val = QED_RDMA_PROTOCOL_DEFAULT;
	rdma_protocol_map_tbl[1].dbdf = QED_ENDOF_TBL;

	if (!strlen(p))
		return 0;

	while (strlen(p) >= QED_RDMA_PROTOCOL_MAP_SIZE) {
		if (i >= QED_DEVS_TBL_SIZE) {
			pr_warn("qed module parameter rdma_protocol_map: Too many devices\n");
			goto err;
		}

		if (sscanf(p, "%02x:%02x.%x-%d",
			   &bus, &dev, &fn, &protocol) != 4) {
			/* expected 4 values matching the scan format */
			pr_err("qed module parameter rdma_protocol_map: Invalid bdf: %s\n",
			       p);
			goto err;
		}

		dbdf = QED_BDF_TO_DBDF(bus, dev, fn);

		for (j = 1; j < i; j++)
			if (rdma_protocol_map_tbl[j].dbdf == dbdf) {
				pr_warn("qed module parameter rdma_protocol_map: %02x:%02x.%x appears multiple times\n",
					bus, dev, fn);
				goto err;
			}

		if (protocol > QED_RDMA_PROTOCOL_IWARP) {
			pr_warn("qed module parameter rdma_protocol_map: Protocol value out of range %d\n",
				protocol);
			goto err;
		}

		rdma_protocol_map_tbl[i].val = protocol;
		rdma_protocol_map_tbl[i].dbdf = dbdf;

		p += QED_RDMA_PROTOCOL_MAP_SIZE;
		if (strlen(p)) {
			if (*p == ',') {
				p++; /* separator */
			} else {
				pr_warn("qed module parameter rdma_protocol_map: Separator invalid %c, expecting ,\n",
					*p);
				goto err;
			}
		}

		i++;
		if (i < QED_DEVS_TBL_SIZE)
			rdma_protocol_map_tbl[i].dbdf = QED_ENDOF_TBL;
	}

	if (strlen(p)) {
		pr_warn("qed module parameter rdma_protocol_map: Invalid value %s\n",
			p);
		goto err;
	}

	return 0;

err:
	rdma_protocol_map_tbl[1].dbdf = QED_ENDOF_TBL;
	pr_warn("qed: The value of rdma_protocol_map is incorrect!\n");
	return -EINVAL;
}
#endif

static int __init qed_init(void)
{
#if defined(CONFIG_QED_RDMA) || !defined(QED_UPSTREAM) /* ! QED_UPSTREAM */
	int rc;
#endif

	pr_notice("qed_init called\n");

	pr_info("%s\n", version);

#ifndef QED_UPSTREAM
	rc = qed_fill_load_function_map();
	if (rc)
		return rc;
#endif
#ifdef CONFIG_QED_RDMA /* ! QED_UPSTREAM */
	rc = qed_rdma_fill_protocol_map();
	if (rc)
		return rc;
#endif

	if (_efi_enabled) {
		qed_sysfs_init(); /* create sysfs in lockdown kernel */
		pr_notice("qed driver running in secureboot, creating sysfs entries\n");
	} else {
		qed_dbg_init();   /* create debugfs */
	}

	return 0;
}

static void __exit qed_cleanup(void)
{
	pr_notice("qed_cleanup called\n");

	if (_efi_enabled)
		qed_sysfs_exit(); /* destroy sysfs */
	else
		qed_dbg_exit();   /* destroy debugfs */
}

module_init(qed_init);
module_exit(qed_cleanup);

/* Check if the DMA controller on the machine can properly handle the DMA
 * addressing required by the device.
*/
static int qed_set_coherency_mask(struct qed_dev *cdev)
{
	struct device *dev = &cdev->pdev->dev;

	if (dma_set_mask(dev, DMA_BIT_MASK(64)) == 0) {
		if (dma_set_coherent_mask(dev, DMA_BIT_MASK(64)) != 0) {
			DP_NOTICE(cdev, "Can't request 64-bit consistent allocations\n");
			return -EIO;
		}
	} else if (dma_set_mask(dev, DMA_BIT_MASK(32)) != 0) {
		DP_NOTICE(cdev, "Can't request 64b/32b DMA addresses\n");
		return -EIO;
	}

	return 0;
}

static void qed_free_pci(struct qed_dev *cdev)
{
	struct pci_dev *pdev = cdev->pdev;

	pci_disable_pcie_error_reporting(pdev);
	if (cdev->doorbells && cdev->db_size)
		iounmap(cdev->doorbells);
	if (cdev->regview)
		iounmap(cdev->regview);
	if (atomic_read(&pdev->enable_cnt) == 1)
		pci_release_regions(pdev);

	pci_disable_device(pdev);
}

#define PCI_REVISION_ID_ERROR_VAL	0xff

/* Performs PCI initializations as well as initializing PCI-related parameters
 * in the device structrue. Returns 0 in case of success.
 */
static int qed_init_pci(struct qed_dev *cdev,
			  struct pci_dev *pdev)
{
	u8 rev_id;
	int rc;

	cdev->pdev = pdev;

	rc = pci_enable_device(pdev);
	if (rc) {
		DP_NOTICE(cdev, "Cannot enable PCI device\n");
		goto err0;
	}

	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM)) {
		DP_NOTICE(cdev, "No memory region found in bar #0\n");
		rc = -EIO;
		goto err1;
	}

	if (IS_PF(cdev) && !(pci_resource_flags(pdev, 2) & IORESOURCE_MEM)) {
		DP_NOTICE(cdev, "No memory region found in bar #2\n");
		rc = -EIO;
		goto err1;
	}

	if (atomic_read(&pdev->enable_cnt) == 1) {
		rc = pci_request_regions(pdev, DRV_MODULE_NAME);
		if (rc) {
			DP_NOTICE(cdev, "Failed to request PCI memory resources\n");
			goto err1;
		}
		pci_set_master(pdev);
		pci_save_state(pdev);
	}

	pci_read_config_byte(pdev, PCI_REVISION_ID, &rev_id);
	if (rev_id == PCI_REVISION_ID_ERROR_VAL) {
		DP_NOTICE(cdev,
			  "Detected PCI device error [rev_id 0x%x]. Probably due to prior fan failure or over temperature indication. Aborting.\n",
			  rev_id);
		rc = -ENODEV;
		goto err2;
	}

	if (!pci_is_pcie(pdev)) {
		DP_NOTICE(cdev, "The bus is not PCI Express\n");
		rc = -EIO;
		goto err2;
	}

	cdev->pci_params.pm_cap = pci_find_capability(pdev, PCI_CAP_ID_PM);
	if (IS_PF(cdev) && cdev->pci_params.pm_cap == 0) {
		DP_NOTICE(cdev, "Cannot find power management capability\n");
		/* FIXME - emulation currently has no PM (13_06_04) */
		/* rc = -EIO;
		 * goto err2;
		 */
	}

	rc = qed_set_coherency_mask(cdev);
	if (rc) {
		DP_NOTICE(cdev, "qed_set_coherency_mask failed\n");
		goto err2;
	}

	cdev->pci_params.mem_start = pci_resource_start(pdev, 0);
	cdev->pci_params.mem_end = pci_resource_end(pdev, 0);
	cdev->pci_params.irq = pdev->irq;

	cdev->regview = pci_ioremap_bar(pdev, 0);
	if (!cdev->regview) {
		DP_NOTICE(cdev, "Cannot map register space, aborting\n");
		rc = -ENOMEM;
		goto err2;
	}

	dev_info(&pdev->dev, "pci_resource_base = 0x%zx pci_resource_len = 0x%08llx\n",
		(ptrdiff_t __force __iomem)cdev->regview,
		(unsigned long long)pci_resource_len(pdev, 0));

	cdev->db_phys_addr = pci_resource_start(cdev->pdev, 2);
	cdev->db_size = pci_resource_len(cdev->pdev, 2);
	if (!cdev->db_size) {
		if (IS_PF(cdev)) {
			DP_NOTICE(cdev, "No Doorbell bar available\n");
			return -EINVAL;
		} else {
			cdev->db_phys_addr = cdev->pci_params.mem_start +
					     PXP_VF_BAR0_START_DQ;
			return 0;
		}
	}

#ifndef QED_UPSTREAM
	if (wc_disabled)
		cdev->doorbells = ioremap_nocache(cdev->db_phys_addr,
						  cdev->db_size);
	else
		cdev->doorbells = ioremap_wc(cdev->db_phys_addr,
					     cdev->db_size);
#else
	cdev->doorbells = ioremap_wc(cdev->db_phys_addr, cdev->db_size);
#endif

	if (!cdev->doorbells) {
		DP_NOTICE(cdev, "Cannot map doorbell space\n");
		return -ENOMEM;
	}

	/* AER (Advanced Error reporting) configuration */
	rc = pci_enable_pcie_error_reporting(pdev);
	if (rc)
		DP_VERBOSE(cdev, NETIF_MSG_DRV, "Failed to configure PCIe AER [%d]\n", rc);

	return 0;

err2:
	pci_release_regions(pdev);
err1:
	pci_disable_device(pdev);
err0:
	return rc;
}

int qed_fill_dev_info(struct qed_dev *cdev, struct qed_dev_info *dev_info)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_hw_info *hw_info = &p_hwfn->hw_info;
	struct qed_tunnel_info *tun = &cdev->tunnel;
	struct qed_ptt *ptt = NULL;

	memset(dev_info, 0, sizeof(struct qed_dev_info));

	if (tun->vxlan.tun_cls == QED_TUNN_CLSS_MAC_VLAN &&
	    tun->vxlan.b_mode_enabled)
		dev_info->vxlan_enable = true;

	if (tun->l2_gre.b_mode_enabled && tun->ip_gre.b_mode_enabled &&
	    tun->l2_gre.tun_cls == QED_TUNN_CLSS_MAC_VLAN &&
	    tun->ip_gre.tun_cls == QED_TUNN_CLSS_MAC_VLAN)
		dev_info->gre_enable = true;

	if (tun->l2_geneve.b_mode_enabled && tun->ip_geneve.b_mode_enabled &&
	    tun->l2_geneve.tun_cls == QED_TUNN_CLSS_MAC_VLAN &&
	    tun->ip_geneve.tun_cls == QED_TUNN_CLSS_MAC_VLAN)
		dev_info->geneve_enable = true;

	dev_info->num_hwfns = cdev->num_hwfns;
	dev_info->pci_mem_start = cdev->pci_params.mem_start;
	dev_info->pci_mem_end = cdev->pci_params.mem_end;
	dev_info->pci_irq = cdev->pci_params.irq;

	if (is_kdump_kernel())
		dev_info->rdma_supported = false;
	else
		dev_info->rdma_supported = QED_IS_RDMA_PERSONALITY(p_hwfn);

	dev_info->dev_type = cdev->type;

	ether_addr_copy(dev_info->hw_mac, hw_info->hw_mac_addr);

	if (IS_PF(cdev)) {
		dev_info->fw_major = FW_MAJOR_VERSION;
		dev_info->fw_minor = FW_MINOR_VERSION;
		dev_info->fw_rev = FW_REVISION_VERSION;
		dev_info->fw_eng = FW_ENGINEERING_VERSION;
		dev_info->b_inter_pf_switch = test_bit(QED_MF_INTER_PF_SWITCH,
						       &cdev->mf_bits);
		if (!test_bit(QED_MF_DISABLE_ARFS, &cdev->mf_bits))
			dev_info->b_arfs_capable = true;
#ifndef QED_UPSTREAM
		dev_info->tx_switching = tx_switching ? true : false;
#else
		dev_info->tx_switching = true;
#endif

		if (p_hwfn->hw_info.b_wol_support == QED_WOL_SUPPORT_PME)
			dev_info->wol_support = true;

		dev_info->smart_an = qed_mcp_is_smart_an_supported(p_hwfn);

		dev_info->esl = qed_mcp_is_esl_supported(p_hwfn);

		ptt = qed_ptt_acquire(QED_LEADING_HWFN(cdev));
		if (ptt) {
			qed_mcp_get_mfw_ver(QED_LEADING_HWFN(cdev), ptt,
					    &dev_info->mfw_rev, NULL);

			qed_mcp_get_mbi_ver(QED_LEADING_HWFN(cdev), ptt,
					    &dev_info->mbi_version);

			qed_mcp_get_flash_size(QED_LEADING_HWFN(cdev), ptt,
					       &dev_info->flash_size);

			qed_ptt_release(QED_LEADING_HWFN(cdev), ptt);
		}

		dev_info->abs_pf_id = p_hwfn->abs_pf_id;
	} else {
		qed_vf_get_fw_version(&cdev->hwfns[0], &dev_info->fw_major,
				      &dev_info->fw_minor, &dev_info->fw_rev,
				      &dev_info->fw_eng);

		qed_mcp_get_mfw_ver(QED_LEADING_HWFN(cdev), NULL,
				    &dev_info->mfw_rev, NULL);

		/* enable arfs for VF */
		if (!test_bit(QED_MF_DISABLE_ARFS, &cdev->mf_bits))
			dev_info->b_arfs_capable = true;
	}

	dev_info->mtu = hw_info->mtu;
	cdev->common_dev_info = *dev_info;

	return 0;
}

static void qed_free_cdev(struct qed_dev *cdev)
{
	kfree(cdev->internal_trace.buf);
	cdev->internal_trace.buf = NULL;

	if (!cdev->b_reuse_dev)
		kfree((void *)cdev);
}

static struct qed_dev *qed_alloc_cdev(struct qed_dev *cdev)
{
	if (!cdev) {
		cdev = kzalloc(sizeof(*cdev), GFP_KERNEL);
		if (!cdev) {
			pr_err("Failed to allocate cdev\n");

			return cdev;
		}
	} else {
		/* In BFS scenarios, recovery path cannot reach filesystem for
		 * request_firmware(). Use prev. fw_buff stored in cdev. which
		 * was not free'ed (preserved) in slowpath_stop()
		 */
		u8 *prev_fw_buf = cdev->fw_buf;

		memset(cdev, 0, sizeof(*cdev));

		cdev->fw_buf = prev_fw_buf;
		cdev->b_reuse_dev = true;
	}

	qed_init_struct(cdev);

	return cdev;
}

/* Sets the requested power state */
static int qed_set_power_state(struct qed_dev *cdev,
				 pci_power_t state)
{
	if (!cdev)
		return -ENODEV;

	/* FIXME - emulation currently does not support PM (13_06_04) */
	DP_VERBOSE(cdev, NETIF_MSG_DRV, "Omitting Power state change\n");
	return 0;
}

#ifndef QED_UPSTREAM /* ! QED_UPSTREAM */
static enum qed_vf_mac_origin qed_get_vf_mac_origin(struct qed_dev *cdev)
{
	switch (vf_mac_origin) {
	case 1:
		return QED_VF_MAC_NVRAM_OR_RANDOM;
	case 2:
		return QED_VF_MAC_ZERO;
	case 3:
		return QED_VF_MAC_RANDOM;
	default:
		DP_INFO(cdev,
			"qed module parameter vf_mac_origin: Unexpected value %d, setting to 0.\n",
			vf_mac_origin);
		/* Fall through */
		COMPAT_FALLTHROUGH;
	case 0:
		return QED_VF_MAC_NVRAM_OR_ZERO;
	}
}
#endif

unsigned long qed_get_epoch_time(void)
{
#ifdef _HAS_KTIME_GET_REAL_SECONDS /* QED_UPSTREAM */
	return ktime_get_real_seconds();
#else
	/* the get_seconds() interface is not y2038 safe on 32bit systems */
	return get_seconds();
#endif
}

#ifndef ASIC_ONLY
#define PCI_EXP_COMP_TIMEOUT_4_TO_13_SEC	0xd

static int qed_set_pci_bridge_comp_timeout(struct qed_dev *cdev,
					   u16 timeout_range)
{
	struct pci_bus *bus = cdev->pdev->bus;
	struct pci_dev *bridge;
	int rc;

	/* Set the completion timeout range all the way to the root complex */
	while (bus) {
		bridge = bus->self;
		rc = pcie_capability_clear_and_set_word(bridge, PCI_EXP_DEVCTL2,
			PCI_EXP_DEVCTL2_COMP_TIMEOUT, timeout_range);
		if (rc)
			return rc;

		bus = bridge->bus->parent;
	}

	return 0;
}
#endif

/* probing */
static struct qed_dev *qed_probe(struct pci_dev *pdev,
				 struct qed_probe_params *params)
{
	struct qed_hw_prepare_params hw_prepare_params;
	struct qed_dev *cdev;
	int rc;

#ifndef TEDIBEAR /* ! QED_UPSTREAM */
	/* Check whether this device should be loaded (1-yes 0-no) */
	rc = qed_bdf2val_find(qed_load_function_tbl, pdev);
	if (!rc) {
		pr_err("qede %02x:%02x.%x: loading device is disabled\n",
		       pdev->bus->number, PCI_SLOT(pdev->devfn),
		       PCI_FUNC(pdev->devfn));
		goto err0;
	}
#endif
	cdev = qed_alloc_cdev(params->cdev);
	if (!cdev)
		goto err0;

	cdev->drv_type = DRV_ID_DRV_TYPE_LINUX;
	cdev->protocol = params->protocol;

	if (params->is_vf) {
		cdev->b_is_vf = true;
	}

	cdev->internal_trace.size = int_debug_size;
	cdev->internal_trace.buf = kzalloc(cdev->internal_trace.size,
					   GFP_KERNEL);
	qed_config_debug(int_debug, &cdev->dp_int_module, &cdev->dp_int_level);
	qed_init_dp(cdev, params->dp_module, params->dp_level, NULL);
	qed_init_int_dp(cdev, cdev->dp_int_module, cdev->dp_int_level);

	cdev->recov_in_prog = params->recov_in_prog;

	rc = qed_init_pci(cdev, pdev);
	if (rc) {
		DP_ERR(cdev, "init pci failed\n");
		goto err1;
	}
	DP_INFO(cdev, "PCI init completed successfully\n");

#ifndef QED_UPSTREAM /* ! QED_UPSTREAM */
	cdev->tx_switching = !!tx_switching;
	cdev->vf_mac_origin = qed_get_vf_mac_origin(cdev);
#else
	cdev->tx_switching = true;
	cdev->vf_mac_origin = QED_VF_MAC_NVRAM_OR_ZERO;
#endif

	memset(&hw_prepare_params, 0, sizeof(hw_prepare_params));

	if (params->is_vf)
		hw_prepare_params.acquire_retry_cnt = QED_VF_ACQUIRE_THRESH;

#ifndef QED_UPSTREAM /* ! QED_UPSTREAM */
	hw_prepare_params.personality = personality;
	hw_prepare_params.drv_resc_alloc = !!drv_resc_alloc;
	hw_prepare_params.chk_reg_fifo = !!chk_reg_fifo;
	hw_prepare_params.initiate_pf_flr = !!initiate_pf_flr;
	hw_prepare_params.allow_mdump = !!allow_mdump;
	hw_prepare_params.b_en_pacing = qed_pkt_pacing;
	hw_prepare_params.monitored_hw_addr = monitored_hw_addr;
	hw_prepare_params.roce_edpm_mode = roce_edpm;
	hw_prepare_params.num_vf_cnqs = num_vf_cnqs;
	hw_prepare_params.b_en_dcqcn = !!dcqcn_enable;
	hw_prepare_params.mcp_resc_lock_retry_cnt =
		params->mcp_resc_lock_retry_cnt;
#else
	hw_prepare_params.personality = QED_PCI_DEFAULT;
	hw_prepare_params.drv_resc_alloc = false;
	hw_prepare_params.chk_reg_fifo = false;
	hw_prepare_params.initiate_pf_flr = true;
	hw_prepare_params.allow_mdump = false;
	hw_prepare_params.b_en_pacing = false;
	hw_prepare_params.monitored_hw_addr = 0;
	hw_prepare_params.roce_edpm_mode = 0;
	hw_prepare_params.num_vf_cnqs = 0;
	hw_prepare_params.b_en_dcqcn = 0;
	hw_prepare_params.mcp_resc_lock_retry_cnt = 0;
#endif

	if (is_kdump_kernel()) {
		DP_ERR(cdev, "Running in kdump kernel\n");
		hw_prepare_params.b_sriov_disable = true;
	}

	hw_prepare_params.epoch = (u32)qed_get_epoch_time();
	rc = qed_hw_prepare(cdev, &hw_prepare_params);
	if (rc) {
		pr_err("qed %02x:%02x.%x: hw prepare failed\n",
		       pdev->bus->number, PCI_SLOT(pdev->devfn),
		       PCI_FUNC(pdev->devfn));
		goto err2;
	}

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(cdev)) {
		rc = qed_set_pci_bridge_comp_timeout(cdev,
			PCI_EXP_COMP_TIMEOUT_4_TO_13_SEC);
		if (rc) {
			DP_ERR(cdev,
			       "Failed to set the PCI completion timeout of the parent bridge devices\n");
			goto err2;
		}
	}
#endif

#ifdef TEDIBEAR
	tedibear_create_bar(&cdev->hwfns[0]);
#endif

	DP_INFO(cdev, "qed_probe completed successfully\n");

	return cdev;

err2:
	qed_free_pci(cdev);
err1:
	qed_free_cdev(cdev);
err0:
	return NULL;
}

static void qed_remove(struct qed_dev *cdev)
{
	if (!cdev)
		return;

	qed_hw_remove(cdev);

	qed_free_pci(cdev);

	qed_set_power_state(cdev, PCI_D3hot);

	qed_free_cdev(cdev);
}

static void qed_disable_msix(struct qed_dev *cdev)
{
	if (cdev->int_params.out.int_mode == QED_INT_MODE_MSIX) {
		if (cdev->int_params.msix_table) {
			pci_disable_msix(cdev->pdev);
			kfree(cdev->int_params.msix_table);
			cdev->int_params.msix_table = NULL;
		}

	} else if (cdev->int_params.out.int_mode == QED_INT_MODE_MSI) {
		pci_disable_msi(cdev->pdev);
	}

	memset(&cdev->int_params.out, 0, sizeof(struct qed_int_param));
}

static int qed_enable_msix(struct qed_dev *cdev,
			   struct qed_int_params *int_params)
{
	int i, rc, cnt;

	cnt = int_params->in.num_vectors;

	for (i = 0; i < cnt; i++)
		int_params->msix_table[i].entry = i;

	rc = pci_enable_msix_range(cdev->pdev, int_params->msix_table,
				   int_params->in.min_msix_cnt, cnt);
	if (rc < cnt && rc >= int_params->in.min_msix_cnt &&
	    (rc % cdev->num_hwfns)) {
		pci_disable_msix(cdev->pdev);

		/* If fastpath is initialized, we need at least one interrupt
		 * per hwfn [and the slow path interrupts]. New requested number
		 * should be a multiple of the number of hwfns.
		 */
		cnt = (rc / cdev->num_hwfns) * cdev->num_hwfns;
		DP_NOTICE(cdev, "Trying to enable MSI-X with less vectors (%d out of %d)\n",
			  cnt, int_params->in.num_vectors);
		rc = pci_enable_msix_exact(cdev->pdev,
					   int_params->msix_table, cnt);
		if (!rc)
			rc = cnt;
	}

	/* For VFs, we should return with error in case we didn't get the exact
	 * number of msix verctors as we requested. Not doing that will lead
	 * to a crash when starting queues for this VF.
	 *
	 * TODO: Need do fix the VF flow to be more flexible.
	 */
	if ((IS_PF(cdev) && rc > 0) || (IS_VF(cdev) && rc == cnt)) {
		/* MSI-x configuration was achieved */
		int_params->out.int_mode = QED_INT_MODE_MSIX;
		int_params->out.num_vectors = rc;
		rc = 0;
	} else {
		DP_NOTICE(cdev, "Failed to enable MSI-X [Requested %d vectors][rc %d]\n",
			  cnt, rc);
	}

	return rc;
}

/* This function outputs the int mode and the number of enabled msix vector */
static int qed_set_int_mode(struct qed_dev *cdev, bool force_mode)
{
	struct qed_int_params *int_params = &cdev->int_params;
	struct msix_entry *tbl;
	u8 info_str[20] = "";
	int rc = 0, cnt;

	switch (int_params->in.int_mode) {
	case QED_INT_MODE_MSIX:
		/* Allocate MSIX table */
		cnt = int_params->in.num_vectors;
		int_params->msix_table = kcalloc(cnt, sizeof(*tbl), GFP_KERNEL);
		if (!int_params->msix_table) {
			rc = -ENOMEM;
			goto out;
		}

		/* Enable MSIX */
		rc = qed_enable_msix(cdev, int_params);
		if (!rc) {
			snprintf(info_str, sizeof(info_str), " [%hd vectors]",
				 int_params->out.num_vectors);
			goto out;
		}

		DP_NOTICE(cdev, "Failed to enable MSI-X\n");
		kfree(int_params->msix_table);
		if (force_mode)
			goto out;
		/* Fallthrough */
		COMPAT_FALLTHROUGH;

	case QED_INT_MODE_MSI:
		if (!QED_IS_CMT(cdev)) {
			rc = pci_enable_msi(cdev->pdev);
			if (!rc) {
				int_params->out.int_mode = QED_INT_MODE_MSI;
				goto out;
			}

			DP_NOTICE(cdev, "Failed to enable MSI\n");
			if (force_mode)
				goto out;
		}
		/* Fallthrough */
		COMPAT_FALLTHROUGH;
	case QED_INT_MODE_INTA:
			int_params->out.int_mode = QED_INT_MODE_INTA;
			rc = 0;
			goto out;
	default:
		DP_NOTICE(cdev, "Unknown int_mode value %d\n",
			  int_params->in.int_mode);
		rc = -EINVAL;
	}

out:
	if (!rc)
		DP_INFO(cdev, "Using %s interrupts%s\n",
			int_params->out.int_mode == QED_INT_MODE_INTA ?
			"INTa" : int_params->out.int_mode == QED_INT_MODE_MSI ?
			"MSI" : "MSIX",
			info_str);
	cdev->int_coalescing_mode = QED_COAL_MODE_ENABLE;

	return rc;
}

static void qed_simd_handler_config(struct qed_dev *cdev, void *token,
				      int index, void(*handler)(void *))
{
	struct qed_hwfn *hwfn = &cdev->hwfns[index % cdev->num_hwfns];
	int relative_idx = index / cdev->num_hwfns;

	hwfn->simd_proto_handler[relative_idx].func = handler;
	hwfn->simd_proto_handler[relative_idx].token = token;
}

static void qed_simd_handler_clean(struct qed_dev *cdev, int index)
{
	struct qed_hwfn *hwfn = &cdev->hwfns[index % cdev->num_hwfns];
	int relative_idx = index / cdev->num_hwfns;

	memset(&hwfn->simd_proto_handler[relative_idx], 0,
	       sizeof(struct qed_simd_fp_handler));
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)) /* QED_UPSTREAM */
static irqreturn_t qed_msix_sp_int(int irq, void *tasklet)
#else
static irqreturn_t qed_msix_sp_int(int irq, void *tasklet,
				   struct pt_regs *regs)
#endif
{
	tasklet_schedule((struct tasklet_struct *)tasklet);
	return IRQ_HANDLED;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)) /* QED_UPSTREAM */
static irqreturn_t qed_single_int(int irq, void *dev_instance)
#else
static irqreturn_t qed_single_int(int irq, void *dev_instance,
				  struct pt_regs *regs)
#endif
{
	struct qed_dev *cdev = (struct qed_dev *)dev_instance;
	struct qed_hwfn *hwfn;
	irqreturn_t rc = IRQ_NONE;
	u64 status;
	int i, j;

	for (i = 0; i < cdev->num_hwfns; i++) {
		status = qed_int_igu_read_sisr_reg(&cdev->hwfns[i]);

		if (!status)
			continue;

		hwfn = &cdev->hwfns[i];

		/* Slowpath interrupt */
		if (unlikely(status & 0x1)) {
			tasklet_schedule(hwfn->sp_dpc);
			status &= ~0x1;
			rc = IRQ_HANDLED;
		}

		/* Fastpath interrupts */
		for (j = 0; j < 64; j++) {
			if ((0x2ULL << j) & status) {
				struct qed_simd_fp_handler *p_handler =
					&hwfn->simd_proto_handler[j];

				if (p_handler->func)
					p_handler->func(p_handler->token);
				else
					DP_NOTICE(hwfn,
						  "Not calling fastpath handler as it is NULL [handler #%d, status 0x%llx]\n",
						  j, status);

				status &= ~(0x2ULL << j);
				rc = IRQ_HANDLED;
			}
		}

		if (unlikely(status))
			DP_VERBOSE(hwfn, NETIF_MSG_INTR,
				   "got an unknown interrupt status 0x%llx\n",
				   status);
	}

	return rc;
}

int qed_slowpath_irq_req(struct qed_hwfn *hwfn)
{
	struct qed_dev *cdev = hwfn->cdev;
	u32 int_mode;
	int rc = 0;
	u8 id;

	int_mode = cdev->int_params.out.int_mode;

	if (int_mode == QED_INT_MODE_MSIX) {
		id = hwfn->my_id;
		snprintf(hwfn->name, NAME_SIZE, "sp-%d-%02x:%02x.%x",
			 id, cdev->pdev->bus->number,
			 PCI_SLOT(cdev->pdev->devfn),
			 PCI_FUNC(cdev->pdev->devfn));
		rc = request_irq(cdev->int_params.msix_table[id].vector,
				 qed_msix_sp_int, 0, hwfn->name, hwfn->sp_dpc);
	} else {
		unsigned long flags = 0;

		snprintf(cdev->name, NAME_SIZE, "%02x:%02x.%x",
			 cdev->pdev->bus->number, PCI_SLOT(cdev->pdev->devfn),
			 PCI_FUNC(cdev->pdev->devfn));

		if (cdev->int_params.out.int_mode == QED_INT_MODE_INTA)
			flags |= IRQF_SHARED;

		rc = request_irq(cdev->pdev->irq, qed_single_int,
				 flags, cdev->name, cdev);
	}

	if (rc)
		DP_NOTICE(cdev, "request_irq failed, rc = %d\n", rc);
	else
		DP_VERBOSE(hwfn, (NETIF_MSG_INTR | QED_MSG_SP),
			   "Requested slowpath %s\n",
			   (int_mode == QED_INT_MODE_MSIX) ? "MSI-X" : "IRQ");

	return rc;
}

static void qed_slowpath_tasklet_flush(struct qed_hwfn *p_hwfn)
{
	/* Calling the disable function will make sure that any
	 * currently-running function is completed. The following call to the
	 * enable function makes this sequence a flush-like operation.
	 */
	if (p_hwfn->b_sp_dpc_enabled) {
		tasklet_disable(p_hwfn->sp_dpc);
		tasklet_enable(p_hwfn->sp_dpc);
	}
}

void qed_slowpath_irq_sync(struct qed_hwfn *p_hwfn)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	u8 id = p_hwfn->my_id;
	u32 int_mode;

	int_mode = cdev->int_params.out.int_mode;
	if (int_mode == QED_INT_MODE_MSIX)
		synchronize_irq(cdev->int_params.msix_table[id].vector);
	else
		synchronize_irq(cdev->pdev->irq);

	qed_slowpath_tasklet_flush(p_hwfn);
}

static void qed_slowpath_irq_free(struct qed_dev *cdev)
{
	int i;

	if (cdev->int_params.out.int_mode == QED_INT_MODE_MSIX) {
		for_each_hwfn(cdev, i) {
			if (!cdev->hwfns[i].b_int_requested)
				break;
			synchronize_irq(cdev->int_params.msix_table[i].vector);
			free_irq(cdev->int_params.msix_table[i].vector,
				 cdev->hwfns[i].sp_dpc);
		}
	} else {
		/* @@@TODO - correct squence for freeing INTA ? */
		if (QED_LEADING_HWFN(cdev)->b_int_requested)
			free_irq(cdev->pdev->irq, cdev);
	}
	qed_int_disable_post_isr_release(cdev);
}

static int qed_nic_stop(struct qed_dev *cdev)
{
	int i, rc;

	rc = qed_hw_stop(cdev);

	for (i = 0; i < cdev->num_hwfns; i++) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];

		if (p_hwfn->b_sp_dpc_enabled) {
			tasklet_disable(p_hwfn->sp_dpc);
			p_hwfn->b_sp_dpc_enabled = false;
			DP_VERBOSE(cdev, NETIF_MSG_IFDOWN,
				   "Disabled sp taskelt [hwfn %d] at %p\n",
				   i, p_hwfn->sp_dpc);
		}
	}

	if (IS_PF(cdev)) {
		if (_efi_enabled)
			qed_sysfs_pf_exit(cdev);
		else
			qed_dbg_pf_exit(cdev);
	}

	return rc;
}

static int qed_nic_setup(struct qed_dev *cdev)
{
	u8 ilt_pg_sz;
	int rc, i;

	/* Determine if interface is going to require LL2 */
	if (QED_LEADING_HWFN(cdev)->hw_info.personality != QED_PCI_ETH) {
		for (i = 0; i < cdev->num_hwfns; i++) {
			struct qed_hwfn *p_hwfn = &cdev->hwfns[i];

			p_hwfn->using_ll2 = true;
		}
	}

#ifndef QED_UPSTREAM
	ilt_pg_sz = (u8)ilt_page_size;
	/* double the ilt allocation in case MF enabled */
	if (test_bit(QED_MF_LARGE_ILT, &cdev->mf_bits))
		ilt_pg_sz++;

	/* valid range of ilt_page_size is from 0 to 10 based on
	 * PSWRQ2_REG_DBG_P_SIZE but 0,1,2,3 is too small and can
	 * be considered as illegal.
	 */
	ilt_pg_sz = ilt_pg_sz < 4 ? 4 :
		ilt_pg_sz > 10 ? 10 : ilt_pg_sz;

	qed_set_ilt_page_size(cdev, ilt_pg_sz);
#else
	qed_set_ilt_page_size(cdev, 5);
#endif

	rc = qed_resc_alloc(cdev);
	if (rc)
		return rc;

	DP_INFO(cdev, "Allocated qed resources with ilt_page_size %d\n", ilt_pg_sz);

	qed_resc_setup(cdev);

	return rc;
}

static int qed_set_int_fp(struct qed_dev *cdev, u16 cnt)
{
	int limit = 0;

	/* Mark the fastpath as free/used */
	cdev->int_params.fp_initialized = cnt ? true : false;

	if (cdev->int_params.out.int_mode != QED_INT_MODE_MSIX)
		limit = cdev->num_hwfns * 63;
	else if (cdev->int_params.fp_msix_cnt)
		limit = cdev->int_params.fp_msix_cnt;

	if (!limit)
		return -ENOMEM;

	return min_t(int, cnt, limit);
}

static int qed_get_int_fp(struct qed_dev *cdev, struct qed_int_info *info)
{
	memset(info, 0, sizeof(struct qed_int_info));

	if (!cdev->int_params.fp_initialized) {
		DP_INFO(cdev, "Protocol driver requested interrupt information, but its support is not yet configured\n");
		return -EINVAL;
	}

	/* Need to expose only MSI-X information; Single IRQ is handled solely
	 * by qed.
	 */
	if (cdev->int_params.out.int_mode == QED_INT_MODE_MSIX) {
		int msix_base = cdev->int_params.fp_msix_base;

		info->msix_cnt = cdev->int_params.fp_msix_cnt;
		info->msix = &cdev->int_params.msix_table[msix_base];
	}

	return 0;
}

static void qed_slowpath_setup_msix(struct qed_dev *cdev)
{
	int i;

	/* Divide allocated MSI-X vectors between slowpath and fastpath */
	cdev->int_params.fp_msix_base = cdev->num_hwfns;
	cdev->int_params.fp_msix_cnt = cdev->int_params.out.num_vectors -
				       cdev->num_hwfns;

	DP_VERBOSE(cdev, NETIF_MSG_INTR, "fp_msix_base %d, fp_msix_cnt %d\n",
		   cdev->int_params.fp_msix_base, cdev->int_params.fp_msix_cnt);

	/* The actual number of L2 queues depends on the number of:
	 * (1) requested connections, (2) allocated L2 queues, and (3) available
	 * MSI-X vectors.
	 */
	if (QED_IS_L2_PERSONALITY(QED_LEADING_HWFN(cdev))) {
		struct qed_hwfn *p_hwfn;

		for_each_hwfn(cdev, i) {
			u16 l2_queues, cids;

			p_hwfn = &cdev->hwfns[i];
			l2_queues = (u16)FEAT_NUM(p_hwfn, QED_PF_L2_QUE);
			cids = p_hwfn->pf_params.eth_pf_params.num_cons;
			cids /= 2 /* rx and xdp */ + p_hwfn->hw_info.num_hw_tc;

			cdev->num_l2_queues += min_t(u16, l2_queues, cids);
		}

		cdev->num_l2_queues = min_t(u16,
					    cdev->num_l2_queues,
					    cdev->int_params.fp_msix_cnt);

		p_hwfn = QED_LEADING_HWFN(cdev);
		DP_VERBOSE(cdev, NETIF_MSG_INTR,
			   "num_l2_queues %d [num_hwfns %d, FEAT_NUM[PF_L2_QUEUE] %d, num_cons %d, fp_msix_cnt %d]\n",
			   cdev->num_l2_queues, cdev->num_hwfns,
			   FEAT_NUM(p_hwfn, QED_PF_L2_QUE),
			   p_hwfn->pf_params.eth_pf_params.num_cons,
			   cdev->int_params.fp_msix_cnt);

#ifndef QED_UPSTREAM
		if (limit_l2_queues && cdev->num_l2_queues > limit_l2_queues) {
			DP_INFO(cdev,
				"Limit the number of L2 queues to %hd [original value = %hd]\n",
				limit_l2_queues, cdev->num_l2_queues);
			cdev->num_l2_queues = limit_l2_queues;
		}
#endif
	}

	/* Need to further split the fastpath interrupts between L2 and RDMA */
	if (IS_ENABLED(CONFIG_QED_RDMA) &&
	    QED_IS_RDMA_PERSONALITY(QED_LEADING_HWFN(cdev))) {
		if (cdev->int_params.fp_msix_cnt > cdev->num_l2_queues) {
			cdev->int_params.rdma_msix_base =
				cdev->int_params.fp_msix_base +
				cdev->num_l2_queues;
			cdev->int_params.rdma_msix_cnt =
				(cdev->int_params.fp_msix_cnt -
				 cdev->num_l2_queues) /
				cdev->num_hwfns; /* RDMA uses a single engine */
			cdev->int_params.fp_msix_cnt = cdev->num_l2_queues;
		} else {
			/* Disable RDMA since no MSI-X vectors are available */
			cdev->int_params.rdma_msix_cnt = 0;
			DP_NOTICE(cdev,
				  "No MSI-X vectors are available for RDMA [fp_msix_cnt %d, num_l2_queues %d]\n",
				  cdev->int_params.fp_msix_cnt,
				  cdev->num_l2_queues);
		}

		DP_VERBOSE(cdev, (NETIF_MSG_INTR | QED_MSG_RDMA),
			   "rdma_msix_base %d, rdma_msix_cnt %d\n",
			   cdev->int_params.rdma_msix_base,
			   cdev->int_params.rdma_msix_cnt);
	}
}

static int qed_slowpath_setup_int(struct qed_dev *cdev,
				  enum qed_int_mode int_mode)
{
	struct qed_sb_cnt_info sb_cnt_info;
	int i, rc;

	if ((int_mode == QED_INT_MODE_MSI) && QED_IS_CMT(cdev)) {
		DP_NOTICE(cdev, "MSI mode is not supported for CMT devices\n");
		return -EINVAL;
	}

	memset(&cdev->int_params, 0, sizeof(struct qed_int_params));
	cdev->int_params.in.int_mode = int_mode;
	/* @@@TBD - request MSIX entries which suffice for all vectors assigned
	 * to that Interface in the IGU CAM. There should probably be some
	 * additional limits to this value.
	 */
	for_each_hwfn(cdev, i) {
		memset(&sb_cnt_info, 0, sizeof(sb_cnt_info));
		qed_int_get_num_sbs(&cdev->hwfns[i], &sb_cnt_info);
		cdev->int_params.in.num_vectors += sb_cnt_info.cnt;
		cdev->int_params.in.num_vectors++; /* slowpath */
	}

	/* We want a minimum of one slowpath and one fastpath vector per hwfn */
	cdev->int_params.in.min_msix_cnt = cdev->num_hwfns * 2;

	if (is_kdump_kernel()) {
		DP_INFO(cdev,
			"Kdump kernel: Limit the max number of requested MSI-X vectors to %hd\n",
			cdev->int_params.in.min_msix_cnt);
		cdev->int_params.in.num_vectors =
			cdev->int_params.in.min_msix_cnt;
	}
#ifndef QED_UPSTREAM
	if (limit_msix_vectors &&
	    cdev->int_params.in.num_vectors > limit_msix_vectors) {
		DP_INFO(cdev,
			"Limit the max number of requested MSI-X vectors to %hd [original value = %hd]\n",
			limit_msix_vectors, cdev->int_params.in.num_vectors);
		cdev->int_params.in.num_vectors = limit_msix_vectors;
	}
#endif
	rc = qed_set_int_mode(cdev, false);
	if (rc)  {
		DP_ERR(cdev, "qed_slowpath_setup_int ERR\n");
		return rc;
	}

	if (cdev->int_params.out.int_mode == QED_INT_MODE_MSIX)
		qed_slowpath_setup_msix(cdev);

	return 0;
}

static int qed_slowpath_vf_setup_int(struct qed_dev *cdev)
{
	u8 total_rxqs, total_cnqs;
	u16 num_vectors;
	int rc;

	memset(&cdev->int_params, 0, sizeof(struct qed_int_params));
	cdev->int_params.in.int_mode = QED_INT_MODE_MSIX;

	qed_vf_get_num_rxqs(QED_LEADING_HWFN(cdev), &total_rxqs);
	qed_vf_get_num_cnqs(QED_AFFIN_HWFN(cdev), &total_cnqs);

	if (QED_IS_CMT(cdev)) {
		u8 rxqs, cnqs;

		qed_vf_get_num_rxqs(&cdev->hwfns[1], &rxqs);
		qed_vf_get_num_cnqs(&cdev->hwfns[1], &cnqs);

		total_rxqs += rxqs;
		total_cnqs += cnqs;
	}

	cdev->int_params.in.num_vectors = total_rxqs + total_cnqs;

	/* We want a minimum of one fastpath vector per vf hwfn */
	cdev->int_params.in.min_msix_cnt = cdev->num_hwfns;

	rc = qed_set_int_mode(cdev, true);
	if (rc)
		return rc;

	cdev->int_params.fp_msix_base = 0;
	num_vectors = cdev->int_params.out.num_vectors;

	if (num_vectors > total_rxqs) {
		cdev->int_params.fp_msix_cnt = total_rxqs;
		cdev->int_params.rdma_msix_base = total_rxqs;
		cdev->int_params.rdma_msix_cnt = num_vectors - total_rxqs;
	} else {
		cdev->int_params.fp_msix_cnt = num_vectors;
		cdev->int_params.rdma_msix_base = 0;
		cdev->int_params.rdma_msix_cnt = 0;
		DP_ERR(cdev,
		       "No MSI-X vectors are available for RDMA [fp_msix_cnt %d, num_l2_queues %d]\n",
		       num_vectors, total_rxqs);
	}

	return 0;
}

u32 qed_unzip_data(struct qed_hwfn *p_hwfn, u32 input_len,
		   u8 *input_buf, u32 max_size, u8 *unzip_buf)
{
	int rc;

	p_hwfn->stream->next_in = input_buf;
	p_hwfn->stream->avail_in = input_len;
	p_hwfn->stream->next_out = unzip_buf;
	p_hwfn->stream->avail_out = max_size;

	rc = zlib_inflateInit2(p_hwfn->stream, MAX_WBITS);

	if (rc != Z_OK) {
		DP_VERBOSE(p_hwfn, NETIF_MSG_DRV, "zlib init failed, rc = %d\n",
			   rc);
		return 0;
	}

	rc = zlib_inflate(p_hwfn->stream, Z_FINISH);
	zlib_inflateEnd(p_hwfn->stream);

	if (rc != Z_OK && rc != Z_STREAM_END) {
		DP_VERBOSE(p_hwfn, NETIF_MSG_DRV, "FW unzip error: %s, rc=%d\n",
			   p_hwfn->stream->msg, rc);
		return 0;
	}

	return p_hwfn->stream->total_out / 4;
}

static int qed_alloc_stream_mem(struct qed_dev *cdev)
{
	int i;
	void *workspace;

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];

		p_hwfn->stream = kzalloc(sizeof(*p_hwfn->stream), GFP_KERNEL);
		if (!p_hwfn->stream)
			return -ENOMEM;

		workspace = vzalloc(zlib_inflate_workspacesize());
		if (!workspace)
			return -ENOMEM;
		p_hwfn->stream->workspace = workspace;
	}

	return 0;
}

static void qed_free_stream_mem(struct qed_dev *cdev)
{
	int i;

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];

		if (!p_hwfn->stream)
			return;

		vfree(p_hwfn->stream->workspace);
		kfree(p_hwfn->stream);
	}
}

static void qed_update_pf_params(struct qed_dev *cdev,
				 struct qed_pf_params *params)
{
	int i;

	if (IS_ENABLED(CONFIG_QED_RDMA)) {
#ifndef TEDIBEAR /* ! QED_UPSTREAM */
		/* Sadly, the dynamic isn't sufficient alone since the module
		 * parameters would be compiled-out. So we need to wrap them
		 * as well.
		 */
		params->rdma_pf_params.num_qps = num_rdma_qps;
		params->rdma_pf_params.num_vf_qps = num_vf_rdma_qps;
		params->rdma_pf_params.num_vf_tasks = num_vf_rdma_tasks;
		params->rdma_pf_params.num_srqs = num_roce_srqs;
		params->rdma_pf_params.num_vf_srqs = num_vf_rdma_srqs;
		params->rdma_pf_params.min_dpis = min_rdma_dpis;
		/* divide by 3 the MRs to avoid MF ILT overflow */
		params->rdma_pf_params.rdma_protocol =
					qed_bdf2val_find(rdma_protocol_map_tbl,
							 cdev->pdev);
#endif
		params->rdma_pf_params.gl_pi = QED_ROCE_PROTOCOL_INDEX;
	}

	params->eth_pf_params.rx_asymmetric_bw_mode = rx_asymmetric_bw_mode;

	if (IS_VF(cdev))
		params->eth_pf_params.num_arfs_filters = QED_RFS_MAX_FLTR;

	/* if CMT don't support filter even for VF */
	if (QED_IS_CMT(cdev))
		params->eth_pf_params.num_arfs_filters = 0;

	/* In case we might support RDMA, don't allow qede to be greedy
	 * with the L2 contexts. Allow for 64 queues [rx, tx coses, xdp]
	 * per hwfn.
	 */
	if (QED_IS_RDMA_PERSONALITY(QED_LEADING_HWFN(cdev))) {
		u16 *num_cons;

		num_cons = &params->eth_pf_params.num_cons;
		*num_cons = min_t(u16, *num_cons, 384/* 64 * 6 */);
	}

#ifndef QED_UPSTREAM
	params->eth_pf_params.allow_vf_mac_change = allow_vf_mac_change_mode;
#endif

	for (i = 0; i < cdev->num_hwfns; i++) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];
		p_hwfn->pf_params = *params;
	}
}

#define QED_PERIODIC_DB_REC_COUNT		10
#define QED_PERIODIC_DB_REC_INTERVAL_MS		100
#define QED_PERIODIC_DB_REC_INTERVAL \
	msecs_to_jiffies(QED_PERIODIC_DB_REC_INTERVAL_MS)

static int qed_slowpath_delayed_work(struct qed_hwfn *hwfn,
				     enum qed_slowpath_wq_flag wq_flag,
				     unsigned long delay)
{
	if (!test_bit(QED_SLOWPATH_ACTIVE, &hwfn->slowpath_task_flags))
		return -EINVAL;

	if (wq_flag != QED_SLOWPATH_RESCHEDULE) {
		smp_mb__before_atomic();
		set_bit(wq_flag, &hwfn->slowpath_task_flags);
		smp_mb__after_atomic();
	}

	queue_delayed_work(hwfn->slowpath_wq, &hwfn->slowpath_task, delay);

	return 0;
}

void qed_periodic_db_rec_start(struct qed_hwfn *p_hwfn)
{
	/* Reset periodic Doorbell Recovery counter */
	p_hwfn->periodic_db_rec_count = QED_PERIODIC_DB_REC_COUNT;

	/* Don't schedule periodic Doorbell Recovery if already scheduled */
	if (test_bit(QED_SLOWPATH_PERIODIC_DB_REC,
		     &p_hwfn->slowpath_task_flags))
		return;

	qed_slowpath_delayed_work(p_hwfn, QED_SLOWPATH_PERIODIC_DB_REC,
				  QED_PERIODIC_DB_REC_INTERVAL);
}

static void qed_slowpath_wq_stop(struct qed_dev *cdev)
{
	int i;

	if (IS_VF(cdev))
		return;

	for_each_hwfn(cdev, i) {
		if (!cdev->hwfns[i].slowpath_wq)
			continue;

		/* Stop queuing new delayed works */
		clear_bit(QED_SLOWPATH_ACTIVE,
			  &cdev->hwfns[i].slowpath_task_flags);

		/* Calling queue_delayed_work concurrently with
		 * destroy_workqueue might race to an unkown outcome -
		 * scheduled task after wq or other resources (like ptt_pool)
		 * are freed (yields NULL pointer derefernce).
		 * cancel_delayed_work kills off a pending delayed_work -
		 * cancels the timer triggered for scheduleding a new task.
		 */
		cancel_delayed_work(&cdev->hwfns[i].slowpath_task);
		destroy_workqueue(cdev->hwfns[i].slowpath_wq);
		cdev->hwfns[i].slowpath_wq = NULL;
	}
}

static void qed_slowpath_task(struct work_struct *work)
{
	struct qed_hwfn *hwfn = container_of(work, struct qed_hwfn,
					     slowpath_task.work);
	struct qed_ptt *ptt = qed_ptt_acquire(hwfn);

	if (!ptt) {
		if (test_bit(QED_SLOWPATH_ACTIVE, &hwfn->slowpath_task_flags))
			qed_slowpath_delayed_work(hwfn,
						  QED_SLOWPATH_RESCHEDULE, 0);

		return;
	}

	if (test_and_clear_bit(QED_SLOWPATH_MFW_TLV_REQ,
			       &hwfn->slowpath_task_flags))
		qed_mfw_process_tlv_req(hwfn, ptt);

	if (test_and_clear_bit(QED_SLOWPATH_PERIODIC_DB_REC,
			       &hwfn->slowpath_task_flags)) {
		/* skip db_rec_handler during recovery/unload */
		if (hwfn->cdev->recov_in_prog ||
		    !test_bit(QED_SLOWPATH_ACTIVE, &hwfn->slowpath_task_flags))
			goto out;

		qed_db_rec_handler(hwfn, ptt);
		if (hwfn->periodic_db_rec_count--)
			qed_slowpath_delayed_work(hwfn,
						  QED_SLOWPATH_PERIODIC_DB_REC,
						  QED_PERIODIC_DB_REC_INTERVAL);
#ifndef QED_UPSTREAM /* ! QED_UPSTREAM */
		else if (periodic_db_rec)
			qed_periodic_db_rec_start(hwfn);
#endif
	}

	if (test_and_clear_bit(QED_SLOWPATH_SFP_UPDATE,
			       &hwfn->slowpath_task_flags))
		qed_dbg_uevent_sfp(hwfn->cdev, QED_DBG_UEVENT_SFP_UPDATE);

	if (test_and_clear_bit(QED_SLOWPATH_SFP_TX_FLT,
			       &hwfn->slowpath_task_flags))
		qed_dbg_uevent_sfp(hwfn->cdev, QED_DBG_UEVENT_SFP_TX_FLT);

	if (test_and_clear_bit(QED_SLOWPATH_SFP_RX_LOS,
			       &hwfn->slowpath_task_flags))
		qed_dbg_uevent_sfp(hwfn->cdev, QED_DBG_UEVENT_SFP_RX_LOS);
out:
	qed_ptt_release(hwfn, ptt);
}

static int qed_slowpath_wq_start(struct qed_dev *cdev)
{
	struct qed_hwfn *hwfn;
	char name[NAME_SIZE];
	int i;

	if (IS_VF(cdev))
		return 0;

	for_each_hwfn(cdev, i) {
		hwfn = &cdev->hwfns[i];

		snprintf(name, NAME_SIZE, "slowpath-%02x:%02x.%02x",
			 cdev->pdev->bus->number,
			 PCI_SLOT(cdev->pdev->devfn), hwfn->abs_pf_id);

#ifdef _HAS_ALLOC_WORKQUEUE /* QED_UPSTREAM */
		hwfn->slowpath_wq = alloc_workqueue(name, 0, 0);
#else
		hwfn->slowpath_wq = create_singlethread_workqueue(name);
#endif
		if (!hwfn->slowpath_wq) {
			DP_NOTICE(hwfn, "Cannot create slowpath workqueue\n");
			return -ENOMEM;
		}

		INIT_DELAYED_WORK(&hwfn->slowpath_task, qed_slowpath_task);
		set_bit(QED_SLOWPATH_ACTIVE, &hwfn->slowpath_task_flags);

#ifndef QED_UPSTREAM /* ! QED_UPSTREAM */
		if (periodic_db_rec)
			qed_periodic_db_rec_start(hwfn);
#endif
	}

	return 0;
}

#ifdef CONFIG_QED_BINARY_FW
#ifndef QED_UPSTREAM
static int qed_request_firmware(struct qed_dev *cdev, const char *name)
{
	struct pci_dev *pdev = cdev->pdev;
	int rc;

	if (QED_RECOV_IN_PROG(cdev)) {
		/* In BFS scenarios, recovery path cannot reach filesystem for
		 * request_firmware(). Use prev. save fw_buf if recov. in prog.
		 */
		pr_notice("qed %02x:%02x.%x: Recovery in progress loading from firmware buffer\n",
			  pdev->bus->number, PCI_SLOT(pdev->devfn),
			  PCI_FUNC(pdev->devfn));

		return 0;
	}

	rc = request_firmware(&cdev->firmware, name, &cdev->pdev->dev);

	if (!rc) {
		cdev->fw_buf = kzalloc(cdev->firmware->size, GFP_KERNEL);
		if (!cdev->fw_buf) {
			DP_NOTICE(cdev,
				  "Failed to allocate memory for the firmware data buffer\n");
			rc = -ENOMEM;
			goto out;
		}

		memcpy(cdev->fw_buf, cdev->firmware->data,
		       cdev->firmware->size);
	}
out:
	release_firmware(cdev->firmware);

	return rc;
}

static void qed_release_firmware(struct qed_dev *cdev)
{
	/* In BFS scenarios, recovery path cannot reach filesystem for
	 * request_firmware(). Retain prev. fw_buf if recov. in prog.
	 * Buffer will be free'd on regular unload/shutdown
	 */
	if (!QED_RECOV_IN_PROG(cdev)) {
		if (cdev->fw_buf) {
			kfree(cdev->fw_buf);
			cdev->fw_buf = NULL;
		}
	} else {
		DP_INFO(cdev, "Recovery in progress retain fw buffer\n");
	}
}
#endif
#endif

static int qed_slowpath_start(struct qed_dev *cdev,
			      struct qed_slowpath_params *params)
{
	struct qed_drv_load_params drv_load_params;
	struct qed_hw_init_params hw_init_params;
	struct qed_mcp_drv_version drv_version;
	struct pci_dev *pdev = cdev->pdev;
#ifdef QED_ENC_SUPPORTED
	struct qed_tunnel_info tunn_info;
#endif
	bool allow_npar_tx_switching;
	const u8 *data = NULL;
	struct qed_hwfn *hwfn;
	struct qed_ptt *p_ptt;
	int rc = -ENOMEM;

	if (qed_iov_wq_start(cdev))
		goto err;

	if (qed_slowpath_wq_start(cdev))
		goto err;

	if (IS_PF(cdev)) {
#ifdef CONFIG_QED_BINARY_FW
		rc = qed_request_firmware(cdev, QED_FW_FILE_NAME);
		if (rc) {
#ifndef QED_UPSTREAM /* ! QED_UPSTREAM */
			/* There are isses in some DUD-based installations; See
			 * CQ85839 for reference.
			 */
			rc = qed_request_firmware(cdev, QED_FW_FILE_NAME_DUD);
			if (rc) {
				pr_notice("qed %02x:%02x.%x: Failed to find firmware file - /lib/firmware/%s and /lib/firmware/%s\n",
					  pdev->bus->number, PCI_SLOT(pdev->devfn),
					  PCI_FUNC(pdev->devfn),
					  QED_FW_FILE_NAME_DUD,
					  QED_FW_FILE_NAME);
				goto err;
			}
#else
			DP_NOTICE(cdev,
				  "Failed to find firmwre file - /lib/firmware/%s\n",
				  QED_FW_FILE_NAME);
			goto err;
#endif
		}
#endif

		if (!QED_IS_CMT(cdev)) {
			p_ptt = qed_ptt_acquire(QED_LEADING_HWFN(cdev));
			if (p_ptt) {
				QED_LEADING_HWFN(cdev)->p_arfs_ptt = p_ptt;
			} else {
				DP_NOTICE(cdev,
					  "Failed to acquire PTT for aRFS\n");
				goto err;
			}
		}
	}

	cdev->rx_coalesce_usecs = QED_DEFAULT_RX_USECS;
	rc = qed_nic_setup(cdev);
	if (rc)
		goto err;

	if (IS_PF(cdev))
		rc = qed_slowpath_setup_int(cdev, params->int_mode);
	else
		rc = qed_slowpath_vf_setup_int(cdev);
	if (rc)
		goto err1;

	if (IS_PF(cdev)) {
		/* Allocate stream for unzipping */
		rc = qed_alloc_stream_mem(cdev);
		if (rc) {
			DP_NOTICE(cdev, "Failed to allocate stream memory\n");
			goto err2;
		}
	}

#ifdef CONFIG_QED_BINARY_FW
	/* First Dword used to diffrentiate between various sources */
	if (IS_PF(cdev))
#ifndef QED_UPSTREAM
		data = cdev->fw_buf + sizeof(u32);
#else
		data = cdev->firmware->data + sizeof(u32);
#endif
#endif

#ifndef QED_UPSTREAM
	allow_npar_tx_switching = npar_tx_switching ? true : false;
#else
	allow_npar_tx_switching = true;
#endif

	if (IS_PF(cdev)) {
		if (_efi_enabled)
			qed_sysfs_pf_init(cdev);
		else
			qed_dbg_pf_init(cdev);
	}

	/* Start the slowpath */
	memset(&hw_init_params, 0, sizeof(hw_init_params));

#ifdef QED_ENC_SUPPORTED
	memset(&tunn_info, 0, sizeof(tunn_info));
	tunn_info.vxlan.b_mode_enabled = true;
	tunn_info.l2_gre.b_mode_enabled = true;
	tunn_info.ip_gre.b_mode_enabled = true;
	tunn_info.l2_geneve.b_mode_enabled = true;
	tunn_info.ip_geneve.b_mode_enabled = true;
	tunn_info.vxlan.tun_cls = QED_TUNN_CLSS_MAC_VLAN;
	tunn_info.l2_gre.tun_cls = QED_TUNN_CLSS_MAC_VLAN;
	tunn_info.ip_gre.tun_cls = QED_TUNN_CLSS_MAC_VLAN;
	tunn_info.l2_geneve.tun_cls = QED_TUNN_CLSS_MAC_VLAN;
	tunn_info.ip_geneve.tun_cls = QED_TUNN_CLSS_MAC_VLAN;
	hw_init_params.p_tunn = &tunn_info;
#endif

	hw_init_params.b_hw_start = true;
	hw_init_params.int_mode = cdev->int_params.out.int_mode;
	hw_init_params.allow_npar_tx_switch = allow_npar_tx_switching;
	hw_init_params.bin_fw_data = data;
#ifndef QED_UPSTREAM
	hw_init_params.pci_rlx_odr_mode = pci_relax_order;
#endif
	memset(&drv_load_params, 0, sizeof(drv_load_params));
	drv_load_params.is_crash_kernel = is_kdump_kernel(); /* TODO - add "&& !kdump_over_pda" */
	drv_load_params.mfw_timeout_val = QED_LOAD_REQ_LOCK_TO_DEFAULT;
#ifndef QED_UPSTREAM
	drv_load_params.avoid_eng_reset = !!avoid_eng_reset;
	drv_load_params.override_force_load =
		(override_force_load == 1) ? QED_OVERRIDE_FORCE_LOAD_ALWAYS :
		(override_force_load == 2) ? QED_OVERRIDE_FORCE_LOAD_NEVER :
		QED_OVERRIDE_FORCE_LOAD_NONE;
	hw_init_params.avoid_eng_affin = !!avoid_eng_affin;
#else
	drv_load_params.avoid_eng_reset = false;
	drv_load_params.override_force_load = QED_OVERRIDE_FORCE_LOAD_NONE;
	hw_init_params.avoid_eng_affin = false;
#endif
	hw_init_params.p_drv_load_params = &drv_load_params;

	rc = qed_hw_init(cdev, &hw_init_params);
	if (rc)
		goto err2;

	DP_INFO(cdev, "HW initialization and function start completed successfully\n");

	if (IS_PF(cdev)) {
		cdev->tunn_feature_mask = (BIT(QED_MODE_VXLAN_TUNN) |
					   BIT(QED_MODE_L2GENEVE_TUNN) |
					   BIT(QED_MODE_IPGENEVE_TUNN) |
					   BIT(QED_MODE_L2GRE_TUNN) |
					   BIT(QED_MODE_IPGRE_TUNN));
	}

	qed_ll2_dealloc_if(cdev);
	/* Allocate LL2 interface if needed */
	if (QED_LEADING_HWFN(cdev)->using_ll2) {
		rc = qed_ll2_alloc_if(cdev);
		if (rc)
			goto err3;
	}

	if (IS_PF(cdev)) {
		hwfn = QED_LEADING_HWFN(cdev);
		drv_version.version = (params->drv_major << 24) |
				      (params->drv_minor << 16) |
				      (params->drv_rev << 8) |
				      (params->drv_eng);
		strlcpy(drv_version.name, params->name,
			MCP_DRV_VER_STR_SIZE - 4);
		rc = qed_mcp_send_drv_version(hwfn, hwfn->p_main_ptt,
					      &drv_version);
		if (rc) {
			DP_NOTICE(cdev, "Failed sending drv version command\n");
			goto err4;
		}
	}

	qed_reset_vport_stats(cdev);

#ifndef QED_UPSTREAM /* ! QED_UPSTREAM */
	cdev->b_dcbx_cfg_commit = true;
#endif
	return 0;

err4:
	qed_ll2_dealloc_if(cdev);
err3:
	qed_hw_stop(cdev);
err2:
	qed_hw_timers_stop_all(cdev);
	/* TODO - need to handle possible failure during init */
	if (IS_PF(cdev))
		qed_slowpath_irq_free(cdev);
	qed_copy_bus_to_postconfig(cdev, qed_get_debug_engine(cdev));
	qed_free_stream_mem(cdev);
	qed_disable_msix(cdev);
err1:
	qed_resc_free(cdev);
err:
#ifdef CONFIG_QED_BINARY_FW
	if (IS_PF(cdev))
		qed_release_firmware(cdev);
#endif
	if (IS_PF(cdev) && !QED_IS_CMT(cdev) &&
	    QED_LEADING_HWFN(cdev)->p_arfs_ptt)
		qed_ptt_release(QED_LEADING_HWFN(cdev),
				QED_LEADING_HWFN(cdev)->p_arfs_ptt);
	qed_iov_wq_stop(cdev, false);

	qed_slowpath_wq_stop(cdev);

	return rc;
}

static int qed_slowpath_stop(struct qed_dev *cdev)
{
	if (!cdev)
		return -ENODEV;

	qed_slowpath_wq_stop(cdev);

	qed_ll2_dealloc_if(cdev);

	if (IS_PF(cdev)) {
		if (!QED_IS_CMT(cdev))
			qed_ptt_release(QED_LEADING_HWFN(cdev),
					QED_LEADING_HWFN(cdev)->p_arfs_ptt);

		if (IS_QED_ETH_IF(cdev) && IS_QED_SRIOV(cdev))
			qed_sriov_disable(cdev, true);
	}

	qed_nic_stop(cdev);

	if (IS_PF(cdev))
		qed_slowpath_irq_free(cdev);

	qed_disable_msix(cdev);

	qed_resc_free(cdev);

	qed_iov_wq_stop(cdev, true);

#ifdef CONFIG_QED_BINARY_FW
	if (IS_PF(cdev)) {
		qed_free_stream_mem(cdev);
		qed_release_firmware(cdev);
	}
#endif

	return 0;
}

static void qed_set_name(struct qed_dev *cdev, char name[NAME_SIZE])
{
	int i;

	memcpy(cdev->name, name, NAME_SIZE);
	for_each_hwfn(cdev, i)
		snprintf(cdev->hwfns[i].name, NAME_SIZE, "%s-%d", name, i);
}

static u32 qed_sb_init(struct qed_dev *cdev,
			 struct qed_sb_info *sb_info,
			 void *sb_virt_addr,
			 dma_addr_t sb_phy_addr, u16 sb_id,
			 enum qed_sb_type type)
{
	struct qed_hwfn *p_hwfn;
	struct qed_ptt *p_ptt;
	u16 rel_sb_id;
	u32 rc;

	/* RoCE/Storage use a single engine in CMT mode while L2 uses both */
	if (type == QED_SB_TYPE_L2_QUEUE) {
		p_hwfn = &cdev->hwfns[sb_id % cdev->num_hwfns];
		rel_sb_id = sb_id / cdev->num_hwfns;
	} else {
		p_hwfn = QED_AFFIN_HWFN(cdev);
		rel_sb_id = sb_id;
	}

	DP_VERBOSE(cdev, NETIF_MSG_INTR,
		   "hwfn [%d] <--[init]-- SB %04x [0x%04x upper]\n",
		   IS_LEAD_HWFN(p_hwfn) ? 0 : 1, rel_sb_id, sb_id);

	if (IS_PF(p_hwfn->cdev)) {
		p_ptt = qed_ptt_acquire(p_hwfn);
		if (!p_ptt)
			return -EBUSY;

		rc = qed_int_sb_init(p_hwfn, p_ptt, sb_info, sb_virt_addr,
				     sb_phy_addr, rel_sb_id);
		qed_ptt_release(p_hwfn, p_ptt);
	} else {
		rc = qed_int_sb_init(p_hwfn, NULL, sb_info, sb_virt_addr,
				     sb_phy_addr, rel_sb_id);
	}

	return rc;
}

static u32 qed_sb_release(struct qed_dev *cdev,
			  struct qed_sb_info *sb_info,
			  u16 sb_id,
			  enum qed_sb_type type)
{
	struct qed_hwfn *p_hwfn;
	u16 rel_sb_id;
	u32 rc;

	/* RoCE/Storage use a single engine in CMT mode while L2 uses both */
	if (type == QED_SB_TYPE_L2_QUEUE) {
		p_hwfn = &cdev->hwfns[sb_id % cdev->num_hwfns];
		rel_sb_id = sb_id / cdev->num_hwfns;
	} else {
		p_hwfn = QED_AFFIN_HWFN(cdev);
		rel_sb_id = sb_id;
	}

	DP_VERBOSE(cdev, NETIF_MSG_INTR,
		   "hwfn [%d] <--[init]-- SB %04x [0x%04x upper]\n",
		   IS_LEAD_HWFN(p_hwfn) ? 0 : 1, rel_sb_id, sb_id);

	rc = qed_int_sb_release(p_hwfn, sb_info, rel_sb_id);

	return rc;
}

static bool qed_can_link_change(struct qed_dev *cdev)
{
	return true;
}

int qed_set_link(struct qed_dev *cdev,
			  struct qed_link_params *params)
{
	struct qed_hwfn *hwfn;
	struct qed_mcp_link_params *link_params;
	struct qed_ptt *ptt;
	u32 sup_caps;
	int rc;

	if (!cdev)
		return -ENODEV;

	/* The link should be set only once per PF */
	hwfn = &cdev->hwfns[0];

	/* When VF wants to set link, force it to read the bulletin instead.
	 * This mimics the PF behavior, where a noitification [both immediate
	 * and possible later] would be generated when changing properties.
	 */
	if (IS_VF(cdev)) {
		qed_schedule_iov(hwfn, QED_IOV_WQ_VF_FORCE_LINK_QUERY_FLAG);
		return 0;
	}

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return -EBUSY;

	link_params = qed_mcp_get_link_params(hwfn);

	if (link_params &&
	    params->override_flags & QED_LINK_OVERRIDE_SPEED_AUTONEG)
		link_params->speed.autoneg = params->autoneg;
	if (link_params &&
	    (params->override_flags & QED_LINK_OVERRIDE_SPEED_ADV_SPEEDS)) {
		link_params->speed.advertised_speeds = 0;
		sup_caps = QED_LM_1000baseT_Full_BIT |
			   QED_LM_1000baseKX_Full_BIT |
			   QED_LM_1000baseX_Full_BIT;
		if (params->adv_speeds & sup_caps)
			link_params->speed.advertised_speeds |=
				NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_1G;
		sup_caps = QED_LM_10000baseT_Full_BIT |
			   QED_LM_10000baseKR_Full_BIT |
			   QED_LM_10000baseKX4_Full_BIT |
			   QED_LM_10000baseR_FEC_BIT |
			   QED_LM_10000baseCR_Full_BIT |
			   QED_LM_10000baseSR_Full_BIT |
			   QED_LM_10000baseLR_Full_BIT |
			   QED_LM_10000baseLRM_Full_BIT;
		if (params->adv_speeds & sup_caps)
			link_params->speed.advertised_speeds |=
				NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_10G;
		if (params->adv_speeds & QED_LM_20000baseKR2_Full_BIT)
			link_params->speed.advertised_speeds |=
				NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_20G;
		sup_caps = QED_LM_25000baseKR_Full_BIT |
			   QED_LM_25000baseCR_Full_BIT |
			   QED_LM_25000baseSR_Full_BIT;
		if (params->adv_speeds & sup_caps)
			link_params->speed.advertised_speeds |=
				NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_25G;
		sup_caps = QED_LM_40000baseLR4_Full_BIT |
			   QED_LM_40000baseKR4_Full_BIT |
			   QED_LM_40000baseCR4_Full_BIT |
			   QED_LM_40000baseSR4_Full_BIT;
		if (params->adv_speeds & sup_caps)
			link_params->speed.advertised_speeds |=
				NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_40G;
		sup_caps = QED_LM_50000baseKR2_Full_BIT |
			   QED_LM_50000baseCR2_Full_BIT |
			   QED_LM_50000baseSR2_Full_BIT;
		if (params->adv_speeds & sup_caps)
			link_params->speed.advertised_speeds |=
				NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_50G;
		sup_caps = QED_LM_100000baseKR4_Full_BIT |
			   QED_LM_100000baseSR4_Full_BIT |
			   QED_LM_100000baseCR4_Full_BIT |
			   QED_LM_100000baseLR4_ER4_Full_BIT;
		if (params->adv_speeds & sup_caps)
			link_params->speed.advertised_speeds |=
				NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_BB_AHP_100G;
	}
	if (link_params &&
	    (params->override_flags & QED_LINK_OVERRIDE_SPEED_FORCED_SPEED))
		link_params->speed.forced_speed = params->forced_speed;

	if (link_params &&
	    (params->override_flags & QED_LINK_OVERRIDE_PAUSE_CONFIG)) {
		if (params->pause_config & QED_LINK_PAUSE_AUTONEG_ENABLE)
			link_params->pause.autoneg = true;
		else
			link_params->pause.autoneg = false;
		if (params->pause_config & QED_LINK_PAUSE_RX_ENABLE)
			link_params->pause.forced_rx = true;
		else
			link_params->pause.forced_rx = false;
		if (params->pause_config & QED_LINK_PAUSE_TX_ENABLE)
			link_params->pause.forced_tx = true;
		else
			link_params->pause.forced_tx = false;
	}

#ifndef QED_UPSTREAM
	if (loopback_mode &&
	    !(params->override_flags & QED_LINK_OVERRIDE_LOOPBACK_MODE)) {
		params->override_flags |= QED_LINK_OVERRIDE_LOOPBACK_MODE;
		params->loopback_mode = loopback_mode;
	}
#endif
	if (link_params &&
	    (params->override_flags & QED_LINK_OVERRIDE_LOOPBACK_MODE)) {
		switch (params->loopback_mode) {
		case QED_LINK_LOOPBACK_INT_PHY:
			link_params->loopback_mode = ETH_LOOPBACK_INT_PHY;
			break;
		case QED_LINK_LOOPBACK_EXT_PHY:
			link_params->loopback_mode = ETH_LOOPBACK_EXT_PHY;
			break;
		case QED_LINK_LOOPBACK_EXT:
			link_params->loopback_mode = ETH_LOOPBACK_EXT;
			break;
		case QED_LINK_LOOPBACK_MAC:
			link_params->loopback_mode = ETH_LOOPBACK_MAC;
			break;
		case QED_LINK_LOOPBACK_CNIG_AH_ONLY_0123:
			link_params->loopback_mode =
				ETH_LOOPBACK_CNIG_AH_ONLY_0123;
			break;
		case QED_LINK_LOOPBACK_CNIG_AH_ONLY_2301:
			link_params->loopback_mode =
				ETH_LOOPBACK_CNIG_AH_ONLY_2301;
			break;
		case QED_LINK_LOOPBACK_PCS_AH_ONLY:
			link_params->loopback_mode = ETH_LOOPBACK_PCS_AH_ONLY;
			break;
		case QED_LINK_LOOPBACK_REVERSE_MAC_AH_ONLY:
			link_params->loopback_mode =
				ETH_LOOPBACK_REVERSE_MAC_AH_ONLY;
			break;
		case QED_LINK_LOOPBACK_INT_PHY_FEA_AH_ONLY:
			link_params->loopback_mode =
				ETH_LOOPBACK_INT_PHY_FEA_AH_ONLY;
			break;
		default:
			link_params->loopback_mode = ETH_LOOPBACK_NONE;
			break;
		}
	}

	if (link_params &&
	    (params->override_flags & QED_LINK_OVERRIDE_EEE_CONFIG))
		memcpy(&link_params->eee, &params->eee,
		       sizeof(link_params->eee));

	if (link_params &&
	    (params->override_flags & QED_LINK_OVERRIDE_FEC_CONFIG)) {
		if (params->fec & QED_MCP_FEC_NONE)
			link_params->fec = QED_FEC_MODE_NONE;
		if (params->fec & QED_MCP_FEC_FIRECODE)
			link_params->fec = QED_FEC_MODE_FIRECODE;
		if (params->fec & QED_MCP_FEC_RS)
			link_params->fec = QED_FEC_MODE_RS;
		if (params->fec & QED_MCP_FEC_AUTO)
			link_params->fec = QED_FEC_MODE_AUTO;
	}

	/* TODO - override default values */
	rc = qed_mcp_set_link(hwfn, ptt, params->link_up);

	qed_ptt_release(hwfn, ptt);

	return rc;
}

static int qed_get_port_type(u32 media_type)
{
	int port_type;

	switch (media_type) {
	case MEDIA_SFPP_10G_FIBER:
	case MEDIA_SFP_1G_FIBER:
	case MEDIA_XFP_FIBER:
	case MEDIA_MODULE_FIBER:
		port_type = PORT_FIBRE;
		break;
	case MEDIA_DA_TWINAX:
		port_type = PORT_DA;
		break;
	case MEDIA_BASE_T:
		port_type = PORT_TP;
		break;
	case MEDIA_KR:
	case MEDIA_NOT_PRESENT:
		port_type = PORT_NONE;
		break;
	case MEDIA_UNSPECIFIED:
	default:
		port_type = PORT_OTHER;
		break;
	}
	return port_type;
}

static void qed_fill_link_capability(struct qed_hwfn *hwfn,
				struct qed_ptt *ptt, u32 capability,
				u32 *if_capability)
{
	u32 media_type, tcvr_state, tcvr_type;
	u32 speed_mask, board_cfg;

	if (qed_mcp_get_media_type(hwfn, ptt, &media_type))
		media_type = MEDIA_UNSPECIFIED;

	if (qed_mcp_get_transceiver_data(hwfn, ptt, &tcvr_state, &tcvr_type))
		tcvr_type = ETH_TRANSCEIVER_STATE_UNPLUGGED;

	if (qed_mcp_trans_speed_mask(hwfn, ptt, &speed_mask))
		speed_mask = 0xFFFFFFFF;

	if (qed_mcp_get_board_config(hwfn, ptt, &board_cfg))
		board_cfg = NVM_CFG1_PORT_PORT_TYPE_UNDEFINED;

	DP_VERBOSE(hwfn->cdev, NETIF_MSG_DRV,
		   "Media_type = 0x%x tcvr_state = 0x%x tcvr_type = 0x%x speed_mask = 0x%x board_cfg = 0x%x\n",
		   media_type, tcvr_state, tcvr_type, speed_mask, board_cfg);

	switch (media_type) {
	case MEDIA_DA_TWINAX:     /* DAC */

		*if_capability |= QED_LM_FIBRE_BIT;
		if (capability & NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_20G)
			*if_capability |= QED_LM_20000baseKR2_Full_BIT;
		/* For DAC media multiple speed capabilities are supported*/
		capability = capability | speed_mask;
		if (capability & NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_1G)
			*if_capability |= QED_LM_1000baseKX_Full_BIT;
		if (capability & NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_10G)
			*if_capability |= QED_LM_10000baseCR_Full_BIT;
		if (capability & NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_40G)
			if ((tcvr_type == ETH_TRANSCEIVER_TYPE_40G_CR4) ||
			    (tcvr_type ==
			    ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_CR) ||
			    (tcvr_type ==
			    ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_CR))
				*if_capability |= QED_LM_40000baseCR4_Full_BIT;
		if (capability & NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_25G)
			*if_capability |= QED_LM_25000baseCR_Full_BIT;
		if (capability & NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_50G)
			*if_capability |= QED_LM_50000baseCR2_Full_BIT;
		if (capability &
			NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_BB_AHP_100G)
			if ((tcvr_type == ETH_TRANSCEIVER_TYPE_100G_CR4) ||
			    (tcvr_type ==
			    ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_CR))
				*if_capability |= QED_LM_100000baseCR4_Full_BIT;
		break;

	case MEDIA_BASE_T:

		*if_capability |= QED_LM_TP_BIT;
		if (GET_MFW_FIELD(board_cfg, NVM_CFG1_PORT_PORT_TYPE) ==
		    NVM_CFG1_PORT_PORT_TYPE_EXT_PHY) {
			if (capability &
			    NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_1G) {
				*if_capability |= QED_LM_1000baseT_Full_BIT;
			}
			if (capability &
				NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_10G) {
				*if_capability |= QED_LM_10000baseT_Full_BIT;
			}
		}
		if (GET_MFW_FIELD(board_cfg, NVM_CFG1_PORT_PORT_TYPE) ==
		    NVM_CFG1_PORT_PORT_TYPE_MODULE) {
			*if_capability |= QED_LM_FIBRE_BIT;
			if (tcvr_type == ETH_TRANSCEIVER_TYPE_1000BASET)
				*if_capability |= QED_LM_1000baseT_Full_BIT;
			if (tcvr_type == ETH_TRANSCEIVER_TYPE_10G_BASET)
				*if_capability |= QED_LM_10000baseT_Full_BIT;

		}
		break;

	case MEDIA_SFP_1G_FIBER:
	case MEDIA_SFPP_10G_FIBER:
	case MEDIA_XFP_FIBER:
	case MEDIA_MODULE_FIBER:     /* optical*/

		*if_capability |= QED_LM_FIBRE_BIT;
		capability = capability | speed_mask;
		if (capability &
			NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_1G) {
			if ((tcvr_type == ETH_TRANSCEIVER_TYPE_1G_LX) ||
			    (tcvr_type == ETH_TRANSCEIVER_TYPE_1G_SX) ||
			    (tcvr_type ==
			     ETH_TRANSCEIVER_TYPE_MULTI_RATE_1G_10G_SR) ||
			    (tcvr_type ==
			     ETH_TRANSCEIVER_TYPE_MULTI_RATE_1G_10G_LR))
				*if_capability |= QED_LM_1000baseKX_Full_BIT;
		}
		if (capability &
			NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_10G) {
			if ((tcvr_type == ETH_TRANSCEIVER_TYPE_10G_SR) ||
			    (tcvr_type ==
			     ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_SR) ||
			    (tcvr_type ==
			     ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_25G_SR) ||
			    (tcvr_type ==
			     ETH_TRANSCEIVER_TYPE_MULTI_RATE_1G_10G_SR))
				*if_capability |= QED_LM_10000baseSR_Full_BIT;
			if ((tcvr_type == ETH_TRANSCEIVER_TYPE_10G_LR) ||
			    (tcvr_type ==
			     ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_LR) ||
			    (tcvr_type ==
			     ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_25G_LR) ||
			    (tcvr_type ==
			     ETH_TRANSCEIVER_TYPE_MULTI_RATE_1G_10G_LR))
				*if_capability |= QED_LM_10000baseLR_Full_BIT;
			if (tcvr_type == ETH_TRANSCEIVER_TYPE_10G_LRM)
				*if_capability |= QED_LM_10000baseLRM_Full_BIT;
			if (tcvr_type == ETH_TRANSCEIVER_TYPE_10G_ER)
				*if_capability |= QED_LM_10000baseR_FEC_BIT;
		}
		if (capability & NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_20G)
			*if_capability |= QED_LM_20000baseKR2_Full_BIT;
		if (capability &
			NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_25G) {
			if ((tcvr_type == ETH_TRANSCEIVER_TYPE_25G_SR) ||
			    (tcvr_type ==
			     ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_25G_SR))
				*if_capability |= QED_LM_25000baseSR_Full_BIT;
		}
		if (capability &
			NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_40G) {
			if ((tcvr_type == ETH_TRANSCEIVER_TYPE_40G_LR4) ||
			    (tcvr_type ==
			    ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_LR) ||
			    (tcvr_type ==
			    ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_LR))
				*if_capability |= QED_LM_40000baseLR4_Full_BIT;
			if ((tcvr_type == ETH_TRANSCEIVER_TYPE_40G_SR4) ||
			    (tcvr_type ==
			     ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_SR) ||
			    (tcvr_type ==
			     ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_SR))
				*if_capability |= QED_LM_40000baseSR4_Full_BIT;
		}
		if (capability &
			NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_50G)
			*if_capability |= QED_LM_50000baseKR2_Full_BIT;
		if (capability &
			NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_BB_AHP_100G) {
			if ((tcvr_type == ETH_TRANSCEIVER_TYPE_100G_SR4) ||
			    (tcvr_type ==
			     ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_SR))
				*if_capability |= QED_LM_100000baseSR4_Full_BIT;
			if (tcvr_type ==
			    ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_LR)
				*if_capability |=
					QED_LM_100000baseLR4_ER4_Full_BIT;
		}

		break;

	case MEDIA_KR:
		*if_capability |= QED_LM_Backplane_BIT;
		if (capability & NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_20G)
			*if_capability |= QED_LM_20000baseKR2_Full_BIT;
		if (capability &
			NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_1G)
			*if_capability |= QED_LM_1000baseKX_Full_BIT;
		if (capability &
			NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_10G)
			*if_capability |= QED_LM_10000baseKR_Full_BIT;
		if (capability &
			NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_25G)
			*if_capability |= QED_LM_25000baseKR_Full_BIT;
		if (capability &
			NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_40G)
			*if_capability |= QED_LM_40000baseKR4_Full_BIT;
		if (capability &
			NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_50G)
			*if_capability |= QED_LM_50000baseKR2_Full_BIT;
		if (capability &
			NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_BB_AHP_100G)
			*if_capability |= QED_LM_100000baseKR4_Full_BIT;
		break;

	case MEDIA_UNSPECIFIED:
	case MEDIA_NOT_PRESENT:
		DP_VERBOSE(hwfn->cdev, QED_MSG_DEBUG,
			   "Unknown media and transceiver type;\n");
		break;
	}
}

static void qed_lp_caps_to_speed_mask(u32 caps, u32 *speed_mask)
{
	if (caps & QED_LINK_PARTNER_SPEED_1G_FD ||
	    caps & QED_LINK_PARTNER_SPEED_1G_HD)
		*speed_mask |= NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_1G;
	if (caps & QED_LINK_PARTNER_SPEED_10G)
		*speed_mask |= NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_10G;
	if (caps & QED_LINK_PARTNER_SPEED_20G)
		*speed_mask |= NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_20G;
	if (caps & QED_LINK_PARTNER_SPEED_25G)
		*speed_mask |= NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_25G;
	if (caps & QED_LINK_PARTNER_SPEED_40G)
		*speed_mask |= NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_40G;
	if (caps & QED_LINK_PARTNER_SPEED_50G)
		*speed_mask |= NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_50G;
	if (caps & QED_LINK_PARTNER_SPEED_100G)
		*speed_mask |= NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_BB_AHP_100G;
}

static void qed_fill_link(struct qed_hwfn *hwfn,
			  struct qed_ptt *ptt,
			  struct qed_link_output *if_link)
{
	struct qed_mcp_link_capabilities link_caps;
	struct qed_mcp_link_params params;
	struct qed_mcp_link_state link;
	u32 media_type, speed_mask;
	void *mcp_link;

	memset(if_link, 0, sizeof(*if_link));

	/* Prepare source inputs */
	if (IS_PF(hwfn->cdev)) {
		mcp_link = qed_mcp_get_link_params(hwfn);
		if (!mcp_link)
			return;
		memcpy(&params, mcp_link, sizeof(params));
		mcp_link = qed_mcp_get_link_state(hwfn);
		if (!mcp_link)
			return;
		memcpy(&link, mcp_link, sizeof(link));
		mcp_link = qed_mcp_get_link_capabilities(hwfn);
		if (!mcp_link)
			return;
		memcpy(&link_caps, mcp_link, sizeof(link_caps));
	} else {
		qed_vf_get_link_params(hwfn, &params);
		qed_vf_get_link_state(hwfn, &link);
		qed_vf_get_link_caps(hwfn, &link_caps);
	}

	/* Set the link parameters to pass to protocol driver */
	if (link.link_up)
		if_link->link_up = true;

	/* TODO - at the moment assume supported and advertised speed equal */
	if (link_caps.default_speed_autoneg)
		if_link->supported_caps |= QED_LM_Autoneg_BIT;
	if (params.pause.autoneg ||
	    (params.pause.forced_rx && params.pause.forced_tx))
		if_link->supported_caps |= QED_LM_Asym_Pause_BIT;
	if (params.pause.autoneg || params.pause.forced_rx ||
	    params.pause.forced_tx)
		if_link->supported_caps |= QED_LM_Pause_BIT;

	if_link->advertised_caps = if_link->supported_caps;
	if (params.speed.autoneg)
		if_link->advertised_caps |= QED_LM_Autoneg_BIT;
	else
		if_link->advertised_caps &= ~QED_LM_Autoneg_BIT;

	if (link_caps.fec_default & QED_MCP_FEC_NONE)
		if_link->sup_fec |= QED_FEC_MODE_NONE;
	if (link_caps.fec_default & QED_MCP_FEC_FIRECODE)
		if_link->sup_fec |= QED_FEC_MODE_FIRECODE;
	if (link_caps.fec_default & QED_MCP_FEC_RS)
		if_link->sup_fec |= QED_FEC_MODE_RS;
	if (link_caps.fec_default & QED_MCP_FEC_AUTO)
		if_link->sup_fec |= QED_FEC_MODE_AUTO;
	if (link_caps.fec_default & QED_MCP_FEC_UNSUPPORTED)
		if_link->sup_fec |= QED_FEC_MODE_UNSUPPORTED;

	if (params.fec & QED_MCP_FEC_NONE)
		if_link->active_fec |= QED_FEC_MODE_NONE;
	if (params.fec & QED_MCP_FEC_FIRECODE)
		if_link->active_fec |= QED_FEC_MODE_FIRECODE;
	if (params.fec & QED_MCP_FEC_RS)
		if_link->active_fec |= QED_FEC_MODE_RS;
	if (params.fec & QED_MCP_FEC_AUTO)
		if_link->active_fec |= QED_FEC_MODE_AUTO;
	if (params.fec & QED_MCP_FEC_UNSUPPORTED)
		if_link->active_fec |= QED_FEC_MODE_UNSUPPORTED;

	/* Fill link advertised capability*/
	qed_fill_link_capability(hwfn, ptt, params.speed.advertised_speeds,
				 &if_link->advertised_caps);
	/* Fill link supported capability*/
	qed_fill_link_capability(hwfn, ptt, link_caps.speed_capabilities,
				 &if_link->supported_caps);

	/* Fill partner advertised capability */
	speed_mask = 0;
	qed_lp_caps_to_speed_mask(link.partner_adv_speed, &speed_mask);
	qed_fill_link_capability(hwfn, ptt, speed_mask, &if_link->lp_caps);

	if (link.link_up)
		if_link->speed = link.speed;

	/* TODO - fill duplex properly */
	if_link->duplex = DUPLEX_FULL;
	qed_mcp_get_media_type(hwfn, ptt, &media_type);
	if_link->port = qed_get_port_type(media_type);
	if_link->autoneg = params.speed.autoneg;

	if (params.pause.autoneg)
		if_link->pause_config |= QED_LINK_PAUSE_AUTONEG_ENABLE;
	if (params.pause.forced_rx)
		if_link->pause_config |= QED_LINK_PAUSE_RX_ENABLE;
	if (params.pause.forced_tx)
		if_link->pause_config |= QED_LINK_PAUSE_TX_ENABLE;

	if (link.an_complete)
		if_link->lp_caps |= QED_LM_Autoneg_BIT;

	if (link.partner_adv_pause)
		if_link->lp_caps |= QED_LM_Pause_BIT;
	if (link.partner_adv_pause == QED_LINK_PARTNER_ASYMMETRIC_PAUSE ||
	    link.partner_adv_pause == QED_LINK_PARTNER_BOTH_PAUSE)
		if_link->lp_caps |= QED_LM_Asym_Pause_BIT;

	if (link_caps.default_eee == QED_MCP_EEE_UNSUPPORTED) {
		if_link->eee_supported = false;
	} else {
		if_link->eee_supported = true;
		if_link->eee_active = link.eee_active;
		if_link->sup_caps = link_caps.eee_speed_caps;
		/* MFW clears adv_caps on eee disable; use configured value */
		if_link->eee.adv_caps = link.eee_adv_caps ? link.eee_adv_caps :
					params.eee.adv_caps;
		if_link->eee.lp_adv_caps = link.eee_lp_adv_caps;
		if_link->eee.enable = params.eee.enable;
		if_link->eee.tx_lpi_enable = params.eee.tx_lpi_enable;
		if_link->eee.tx_lpi_timer = params.eee.tx_lpi_timer;
	}
}

void qed_get_current_link(struct qed_dev *cdev,
				   struct qed_link_output *if_link)
{
	struct qed_hwfn *hwfn;
	struct qed_ptt *ptt;
	int i;

	hwfn = &cdev->hwfns[0];
	if (IS_PF(cdev)) {
		ptt = qed_ptt_acquire(hwfn);
		if (ptt) {
			qed_fill_link(hwfn, ptt, if_link);
			qed_ptt_release(hwfn, ptt);
		} else {
			DP_NOTICE(hwfn, "Failed to fill link; No PTT\n");
		}
	} else {
		qed_fill_link(hwfn, NULL, if_link);
	}

	for_each_hwfn(cdev, i)
		qed_inform_vf_link_state(&cdev->hwfns[i]);
}

void qed_link_update(struct qed_hwfn *hwfn, struct qed_ptt *ptt)
{
	void *cookie = hwfn->cdev->ops_cookie;
	struct qed_common_cb_ops *op = hwfn->cdev->protocol_ops.common;
	struct qed_link_output if_link;

	qed_fill_link(hwfn, ptt, &if_link);
	qed_inform_vf_link_state(hwfn);

	if (IS_LEAD_HWFN(hwfn) && cookie && op && op->link_update)
		op->link_update(cookie, &if_link);
}

void qed_bw_update(struct qed_hwfn *hwfn, struct qed_ptt *ptt)
{
	void *cookie = hwfn->cdev->ops_cookie;
	struct qed_common_cb_ops *op = hwfn->cdev->protocol_ops.common;

	if (IS_LEAD_HWFN(hwfn) && cookie && op && op->bw_update)
		op->bw_update(cookie);
}

void qed_transceiver_update(struct qed_hwfn *p_hwfn)
{
	qed_slowpath_delayed_work(p_hwfn, QED_SLOWPATH_SFP_UPDATE, 0);
}

void qed_transceiver_tx_fault(struct qed_hwfn *p_hwfn)
{
	qed_slowpath_delayed_work(p_hwfn, QED_SLOWPATH_SFP_TX_FLT, 0);
}

void qed_transceiver_rx_los(struct qed_hwfn *p_hwfn)
{
	qed_slowpath_delayed_work(p_hwfn, QED_SLOWPATH_SFP_RX_LOS, 0);
}

static int qed_drain(struct qed_dev *cdev)
{
	struct qed_hwfn *hwfn;
	struct qed_ptt *ptt;
	int i, rc;

	if (IS_VF(cdev))
		return 0;

	for_each_hwfn(cdev, i) {
		hwfn = &cdev->hwfns[i];
		ptt = qed_ptt_acquire(hwfn);
		if (!ptt) {
			DP_NOTICE(hwfn, "Failed to drain NIG; No PTT\n");
			return -EBUSY;
		}
		rc = qed_mcp_drain(hwfn, ptt);
		qed_ptt_release(hwfn, ptt);
		if (rc)
			return rc;
	}

	return 0;
}

static int qed_nvm_get_cmd(struct qed_dev *cdev, u32 cmd, u32 addr,
			   u8 *buf, u32 len)
{
	int rc = 0;

	switch (cmd) {
	case QED_NVM_READ_NVRAM:
		rc = qed_mcp_nvm_read(cdev, addr, buf, len);
		break;
	case QED_GET_MCP_NVM_RESP:
		*(u32 *)buf = cdev->mcp_nvm_resp;
		break;
	default:
		rc = -EOPNOTSUPP;
		cdev->mcp_nvm_resp = FW_MSG_CODE_NVM_OPERATION_FAILED;
		DP_NOTICE(cdev, "Unknown command %d\n", cmd);
		break;
	}

	return rc;
}

static int qed_nvm_set_cmd(struct qed_dev *cdev, u32 cmd, u32 addr,
			   u8 *buf, u32 len)
{
	int rc = 0;

	switch (cmd) {
	case QED_NVM_DEL_FILE:
		 rc = qed_mcp_nvm_del_file(cdev, addr);
		break;
	case QED_PUT_FILE_BEGIN:
	case QED_PUT_FILE_DATA:
	case QED_NVM_WRITE_NVRAM:
	case QED_EXT_PHY_FW_UPGRADE:
	case QED_ENCRYPT_PASSWORD:
		 rc = qed_mcp_nvm_write(cdev, cmd, addr, buf, len);
		break;
	default:
		rc = -EOPNOTSUPP;
		cdev->mcp_nvm_resp = FW_MSG_CODE_NVM_OPERATION_FAILED;
		DP_NOTICE(cdev, "Unknown command 0x%x\n", cmd);
		break;
	}

	return rc;
}

static u32 qed_nvm_flash_image_access_crc(struct qed_dev *cdev,
					  struct qed_nvm_image_att *nvm_image,
					  u32 *crc)
{
	u8 *buf = NULL;
	int rc, j;
	u32 val;

	/* Allocate a buffer for holding the nvram image */
	buf = kzalloc(nvm_image->length, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	/* Read image into buffer */
	rc = qed_mcp_nvm_read(cdev, nvm_image->start_addr,
			      buf, nvm_image->length);
	if (rc) {
		DP_ERR(cdev, "Failed reading image from nvm\n");
		goto out;
	}

	/* Convert the buffer into big-endian format (excluding the
	 * closing 4 bytes of CRC).
	 */
	for (j = 0; j < nvm_image->length - 4; j += 4) {
		val = cpu_to_be32(*(u32 *)&buf[j]);
		*(u32 *)&buf[j] = val;
	}

	/* Calc CRC for the "actual" image buffer, i.e. not including
	 * the last 4 CRC bytes.
	 */
	*crc = (~cpu_to_be32(crc32(0xffffffff, buf, nvm_image->length - 4)));

out:
	kfree(buf);

	return rc;
}

/* Binary file format -
 *     /----------------------------------------------------------------------\
 * 0B  |                       0x4 [command index]                            |
 * 4B  | image_type     | Options        |  Number of register settings       |
 * 8B  |                       Value                                          |
 * 12B |                       Mask                                           |
 * 16B |                       Offset                                         |
 *     \----------------------------------------------------------------------/
 * There can be serveral Value-Mask-Offset sets as specified by 'Number of...'.
 * Options - 0'b - Calculate & Update CRC for image
 */
static int qed_nvm_flash_image_access(struct qed_dev *cdev, const u8 **data,
				      bool *check_resp)
{
	struct qed_nvm_image_att nvm_image;
	struct qed_hwfn *p_hwfn;
	bool is_crc = false;
	u32 image_type;
	int rc = 0, i;
	u16 len;

	*data += 4;
	image_type = **data;
	p_hwfn = QED_LEADING_HWFN(cdev);
	for (i = 0; i < p_hwfn->nvm_info.num_images; i++)
		if (image_type == p_hwfn->nvm_info.image_att[i].image_type)
			break;
	if (i == p_hwfn->nvm_info.num_images) {
		DP_ERR(cdev, "Failed to find nvram image of type %08x\n",
		       image_type);
		return -ENOENT;
	}

	nvm_image.start_addr = p_hwfn->nvm_info.image_att[i].nvm_start_addr;
	nvm_image.length = p_hwfn->nvm_info.image_att[i].len;

	DP_VERBOSE(cdev, NETIF_MSG_DRV,
		   "Read image %02x; type = %08x; NVM [%08x,...,%08x]\n",
		  **data, nvm_image.start_addr, image_type,
		  nvm_image.start_addr + nvm_image.length - 1);
	(*data)++;
	is_crc = !!(**data);
	(*data)++;
	len = *((u16 *) *data);
	*data += 2;
	if (is_crc) {
		u32 crc = 0;

		rc = qed_nvm_flash_image_access_crc(cdev, &nvm_image, &crc);
		if (rc) {
			DP_ERR(cdev, "Failed calculating CRC, rc = %d\n", rc);
			goto exit;
		}

		rc = qed_mcp_nvm_write(cdev, QED_NVM_WRITE_NVRAM,
				       (nvm_image.start_addr +
					nvm_image.length - 4), (u8 *)&crc, 4);
		if (rc)
			DP_ERR(cdev, "Failed writing to %08x, rc = %d\n",
			       nvm_image.start_addr + nvm_image.length - 4, rc);
		goto exit;
	}

	/* Iterate over the values for setting */
	while (len) {
		u32 offset, mask, value, cur_value;
		u8 buf[4];

		value = *((u32 *)*data);
		*data += 4;
		mask = *((u32 *)*data);
		*data += 4;
		offset = *((u32 *)*data);
		*data += 4;

		rc = qed_mcp_nvm_read(cdev, nvm_image.start_addr + offset, buf,
				      4);
		if (rc) {
			DP_ERR(cdev, "Failed reading from %08x\n",
			       nvm_image.start_addr + offset);
			goto exit;
		}

		cur_value = le32_to_cpu(*((__le32 *)buf));
		DP_VERBOSE(cdev, NETIF_MSG_DRV,
			   "NVM %08x: %08x -> %08x [Value %08x Mask %08x]\n",
			   nvm_image.start_addr + offset, cur_value,
			   (cur_value & ~mask) | (value & mask), value, mask);
		value = (value & mask) | (cur_value & ~mask);
		rc = qed_mcp_nvm_write(cdev, QED_NVM_WRITE_NVRAM,
				       nvm_image.start_addr + offset,
				       (u8 *)&value, 4);
		if (rc) {
			DP_ERR(cdev, "Failed writing to %08x\n",
			       nvm_image.start_addr + offset);
			goto exit;
		}

		len--;
	}
exit:
	return rc;
}

/* Binary file format -
 *     /----------------------------------------------------------------------\
 * 0B  |                       0x3 [command index]                            |
 * 4B  | b'0: check_response?   | b'1-127  reserved                           |
 * 8B  | File-type |                   reserved                               |
 * 12B |                    Image length in bytes                             |
 *     \----------------------------------------------------------------------/
 *     Start a new file of the provided type
 */
static int qed_nvm_flash_image_file_start(struct qed_dev *cdev,
					  const u8 **data,
					  bool *check_resp)
{
	u32 file_type, file_size = 0;
	int rc;

	*data += 4;
	*check_resp = !!(**data);
	*data += 4;
	file_type = **data;

	DP_VERBOSE(cdev, NETIF_MSG_DRV,
		   "About to start a new file of type %02x\n", file_type);
	if (file_type == DRV_MB_PARAM_NVM_PUT_FILE_BEGIN_MBI) {
		*data += 4;
		file_size = *((u32 *)(*data));
	}

	rc = qed_mcp_nvm_write(cdev, QED_PUT_FILE_BEGIN, file_type,
			       (u8 *)(&file_size), 4);
	*data += 4;

	return rc;
}

/* Binary file format -
 *     /----------------------------------------------------------------------\
 * 0B  |                       0x2 [command index]                            |
 * 4B  |                       Length in bytes                                |
 * 8B  | b'0: check_response?   | b'1-127  reserved                           |
 * 12B |                       Offset in bytes                                |
 * 16B |                       Data ...                                       |
 *     \----------------------------------------------------------------------/
 *     Write data as part of a file that was previously started. Data should be
 *     of length equal to that provided in the message
 */
static int qed_nvm_flash_image_file_data(struct qed_dev *cdev,
					 const u8 **data,
					 bool *check_resp)
{
	u32 offset, len;
	int rc;

	*data += 4;
	len = *((u32 *)(*data));
	*data += 4;
	*check_resp = !!(**data);
	*data += 4;
	offset = *((u32 *)(*data));
	*data += 4;

	DP_VERBOSE(cdev, NETIF_MSG_DRV,
		   "About to write File-data: %08x bytes to offset %08x\n",
		   len, offset);

	rc = qed_mcp_nvm_write(cdev, QED_PUT_FILE_DATA, offset,
			       (char *)(*data), len);
	*data += len;

	return rc;
}

/* Binary file format [General header] -
 *     /----------------------------------------------------------------------\
 * 0B  |                       0x12435687 [signature]                         |
 * 4B  |                       Length in bytes                                |
 * 8B  | Highest command in this batchfile |          Reserved                |
 *     \----------------------------------------------------------------------/
 */
static int qed_nvm_flash_image_validate(struct qed_dev *cdev,
					const struct firmware *image,
					const u8 **data)
{
	u32 signature, len;

	/* Check minimum size */
	if (image->size < 12) {
		DP_ERR(cdev, "Image is too short [%08x]\n", (u32)image->size);
		return -EINVAL;
	}

	/* Check signature */
	signature = *((u32 *)(*data));
	if (signature != 0x12435687) {
		DP_ERR(cdev, "Wrong signature '%08x'\n", signature);
		return -EINVAL;
	}

	*data += 4;
	/* Validate internal size equals the image-size */
	len = *((u32 *)(*data));
	if (len != image->size) {
		DP_ERR(cdev, "Size mismatch: internal = %08x image = %08x\n",
		       len, (u32)image->size);
		return -EINVAL;
	}

	*data += 4;
	/* Make sure driver familiar with all commands necessary for this */
	if (*((u16 *)(*data)) >= QED_NVM_FLASH_CMD_NVM_MAX) {
		DP_ERR(cdev, "File contains unsupported commands [Need %04x]\n",
		       *((u16 *)(*data)));
		return -EINVAL;
	}

	*data += 4;

	return 0;
}

/* Binary file format -
 *     /----------------------------------------------------------------------\
 * 0B  |                       0x5 [command index]                            |
 * 4B  | Number of config attributes     |          Reserved                  |
 * 8B  | Config ID                       | Entity ID      | Length            |
 * 16B | Value                                                                |
 *     |                                                                      |
 *     \----------------------------------------------------------------------/
 * There can be several cfg_id-entity_id-Length-Value sets as specified by
 * 'Number of config attributes'.
 *
 * The API parses config attributes from the user provided buffer and flashes
 * them to the respective NVM path using Management FW inerface.
 */
static int qed_nvm_flash_cfg_write(struct qed_dev *cdev, const u8 **data)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	u8 entity_id, len, buf[32];
	bool need_nvm_init = true;
	struct qed_ptt *ptt;
	u16 cfg_id, count;
	int rc = 0, i;
	u32 flags;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return -EAGAIN;

	/* NVM CFG ID attribute header */
	*data += 4;
	count = *((u16 *)*data);
	*data += 4;

	DP_VERBOSE(cdev, NETIF_MSG_DRV,
		   "Read config ids: num_attrs = %0d\n", count);
	/* NVM CFG ID attributes. Start loop index from 1 to avoid additional
	 * arithmetic operations in the implementation.
	 */
	for (i = 1; i <= count; i++) {
		cfg_id = *((u16 *)*data);
		*data += 2;
		entity_id = **data;
		(*data)++;
		len = **data;
		(*data)++;
		memcpy(buf, *data, len);
		*data += len;

		flags = QED_NVM_CFG_OPTION_ENTITY_SEL;
		if (need_nvm_init) {
			flags |= QED_NVM_CFG_OPTION_INIT;
			need_nvm_init = false;
		}

		/* Commit to flash and free the resources */
		if (!(i % QED_NVM_CFG_MAX_ATTRS) || i == count) {
			flags |= QED_NVM_CFG_OPTION_COMMIT |
				 QED_NVM_CFG_OPTION_FREE;
			need_nvm_init = true;
		}

		DP_VERBOSE(cdev, NETIF_MSG_DRV,
			   "cfg_id = %d entity = %d len = %d\n", cfg_id,
			   entity_id, len);
		rc = qed_mcp_nvm_set_cfg(hwfn, ptt, cfg_id, entity_id, flags,
					 buf, len);
		if (rc) {
			DP_ERR(cdev, "Error %d configuring %d\n", rc, cfg_id);
			break;
		}
	}

	qed_ptt_release(hwfn, ptt);

	return rc;
}

#define QED_MAX_NVM_BUF_LEN	32
static int qed_nvm_flash_cfg_len(struct qed_dev *cdev, u32 cmd)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	u8 buf[QED_MAX_NVM_BUF_LEN];
	struct qed_ptt *ptt;
	u32 len;
	int rc;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return QED_MAX_NVM_BUF_LEN;

	rc = qed_mcp_nvm_get_cfg(hwfn, ptt, cmd, 0, QED_NVM_CFG_GET_FLAGS, buf,
				 &len);
	if (rc || !len) {
		DP_ERR(cdev, "Error %d reading %d\n", rc, cmd);
		len = QED_MAX_NVM_BUF_LEN;
	}

	qed_ptt_release(hwfn, ptt);

	return len;
}

static int qed_nvm_flash_cfg_read(struct qed_dev *cdev, u8 **data,
				  u32 cmd, u32 entity_id)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *ptt;
	u32 flags, len;
	int rc = 0;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return -EAGAIN;

	DP_VERBOSE(cdev, NETIF_MSG_DRV,
		   "Read config cmd = %d entity id %d\n", cmd, entity_id);
	flags = QED_NVM_CFG_GET_PF_FLAGS;
	rc = qed_mcp_nvm_get_cfg(hwfn, ptt, cmd, entity_id, flags, *data, &len);
	if (rc)
		DP_ERR(cdev, "Error %d reading %d\n", rc, cmd);

	qed_ptt_release(hwfn, ptt);

	return rc;
}

static int qed_nvm_flash(struct qed_dev *cdev, const char *name)
{
	const struct firmware *image;
	const u8 *data, *data_end;
	u32 cmd_type;
	int rc;

	rc = request_firmware(&image, name, &cdev->pdev->dev);
	if (rc) {
		DP_ERR(cdev, "Failed to find '%s'\n", name);
		return rc;
	}

	DP_VERBOSE(cdev, NETIF_MSG_DRV,
		   "Flashing '%s' - firmware's data at %p, size is %08x\n",
		   name, image->data, (u32)image->size);
	data = image->data;
	data_end = data + image->size;

	rc = qed_nvm_flash_image_validate(cdev, image, &data);
	if (rc)
		goto exit;

	while (data < data_end) {
		bool check_resp = false;

		/* Parse the actual command */
		cmd_type = *((u32 *)data);
		switch (cmd_type) {
		case QED_NVM_FLASH_CMD_FILE_DATA:
			rc = qed_nvm_flash_image_file_data(cdev, &data,
							   &check_resp);
			break;
		case QED_NVM_FLASH_CMD_FILE_START:
			rc = qed_nvm_flash_image_file_start(cdev, &data,
							    &check_resp);
			break;
		case QED_NVM_FLASH_CMD_NVM_CHANGE:
			rc = qed_nvm_flash_image_access(cdev, &data,
							&check_resp);
			break;
		case QED_NVM_FLASH_CMD_NVM_CFG_ID:
			rc = qed_nvm_flash_cfg_write(cdev, &data);
			break;
		default:
			DP_ERR(cdev, "Unknown command %08x\n", cmd_type);
			rc = -EINVAL;
			break;
		}

		if (rc) {
			DP_ERR(cdev, "Command %08x failed\n", cmd_type);
			goto exit;
		}

		/* Check reponse if needed */
		if (check_resp) {
			switch (cdev->mcp_nvm_resp & FW_MSG_CODE_MASK) {
			case FW_MSG_CODE_OK:
			case FW_MSG_CODE_NVM_OK:
			case FW_MSG_CODE_NVM_PUT_FILE_FINISH_OK:
			case FW_MSG_CODE_PHY_OK:
				break;
			default:
				DP_ERR(cdev, "MFW returns error: %08x\n",
				       cdev->mcp_nvm_resp);
				rc = -EINVAL;
				goto exit;
			}
		}
	}

exit:
	release_firmware(image);

	return rc;
}

static int qed_nvm_get_image(struct qed_dev *cdev, enum qed_nvm_images type,
			     u8 *buf, u16 len)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);

	return qed_mcp_get_nvm_image(hwfn, type, buf, len);
}

void qed_schedule_recovery_handler(struct qed_hwfn *p_hwfn)
{
	struct qed_common_cb_ops *ops = p_hwfn->cdev->protocol_ops.common;
	void *cookie = p_hwfn->cdev->ops_cookie;

	if (ops && ops->schedule_recovery_handler)
		ops->schedule_recovery_handler(cookie);
}

void qed_hw_error_occurred(struct qed_hwfn *p_hwfn,
			   enum qed_hw_err_type err_type)
{
	struct qed_common_cb_ops *ops = p_hwfn->cdev->protocol_ops.common;
	void *cookie = p_hwfn->cdev->ops_cookie;
	char err_str[32];

	switch (err_type) {
	case QED_HW_ERR_FAN_FAIL:
		strcpy(err_str, "Fan Failure");
		break;
	case QED_HW_ERR_MFW_RESP_FAIL:
		strcpy(err_str, "MFW Response Failure");
		break;
	case QED_HW_ERR_HW_ATTN:
		strcpy(err_str, "HW Attention");
		break;
	case QED_HW_ERR_DMAE_FAIL:
		strcpy(err_str, "DMAE Failure");
		break;
	case QED_HW_ERR_RAMROD_FAIL:
		strcpy(err_str, "Ramrod Failure");
		break;
	case QED_HW_ERR_FW_ASSERT:
		strcpy(err_str, "FW Assertion");
		break;
	default:
		strcpy(err_str, "Unknown");
		break;
	}

	DP_NOTICE(p_hwfn, "HW error occurred [%s]\n", err_str);

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->cdev)) {
		qed_int_attn_clr_enable(p_hwfn->cdev, true);
		return;
	}
#endif
	/* Call the HW error handler of the protocol driver.
	 * If it is not available - perform a minimal handling of preventing
	 * HW attentions from being reasserted.
	 */
	if (ops && ops->schedule_hw_err_handler)
		ops->schedule_hw_err_handler(cookie, err_type);
	else
		qed_int_attn_clr_enable(p_hwfn->cdev, true);
}

static void qed_get_coalesce(struct qed_dev *cdev, u16 *rx_coal, u16 *tx_coal)
{
	*rx_coal = cdev->rx_coalesce_usecs;
	*tx_coal = cdev->tx_coalesce_usecs;
}

static int qed_set_coalesce(struct qed_dev *cdev, u16 rx_coal, u16 tx_coal,
			    void *handle)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);

	return qed_set_queue_coalesce(hwfn, rx_coal, tx_coal, handle);
}

static int qed_set_led(struct qed_dev *cdev, enum qed_led_mode mode)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *ptt;
	int status = 0;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return -EAGAIN;

	status = qed_mcp_set_led(hwfn, ptt, mode);

	qed_ptt_release(hwfn, ptt);

	return status;
}

int qed_recovery_process(struct qed_dev *cdev)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt;
	int rc = 0;

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return -EAGAIN;

	rc = qed_start_recovery_process(p_hwfn, p_ptt);

	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

static int qed_update_wol(struct qed_dev *cdev, bool enabled)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *ptt;
	int rc = 0;

	if (IS_VF(cdev))
		return 0;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return -EAGAIN;

	rc = qed_mcp_ov_update_wol(hwfn, ptt, enabled ? QED_OV_WOL_ENABLED
						      : QED_OV_WOL_DISABLED);
	if (rc)
		goto out;
	rc = qed_mcp_ov_update_current_config(hwfn, ptt, QED_OV_CLIENT_DRV);

out:
	qed_ptt_release(hwfn, ptt);
	return rc;
}

static int qed_update_drv_state(struct qed_dev *cdev, bool active)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *ptt;
	int status = 0;

	if (IS_VF(cdev))
		return 0;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return -EAGAIN;

	status = qed_mcp_ov_update_driver_state(hwfn, ptt, active ?
						QED_OV_DRIVER_STATE_ACTIVE :
						QED_OV_DRIVER_STATE_DISABLED);

	qed_ptt_release(hwfn, ptt);

	return status;
}

static int qed_update_mac(struct qed_dev *cdev, const u8 *mac)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *ptt;
	int status = 0;

	if (IS_VF(cdev))
		return 0;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return -EAGAIN;

	status = qed_mcp_ov_update_mac(hwfn, ptt, mac);
	if (status)
		goto out;

	status = qed_mcp_ov_update_current_config(hwfn, ptt, QED_OV_CLIENT_DRV);

out:
	qed_ptt_release(hwfn, ptt);

	return status;
}

static int qed_update_mtu(struct qed_dev *cdev, u16 mtu)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *ptt;
	int status = 0;

	if (IS_VF(cdev))
		return 0;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return -EAGAIN;

	status = qed_mcp_ov_update_mtu(hwfn, ptt, mtu);
	if (status)
		goto out;

	status = qed_mcp_ov_update_current_config(hwfn, ptt, QED_OV_CLIENT_DRV);

out:
	qed_ptt_release(hwfn, ptt);

	return status;
}

static int qed_get_sb_info(struct qed_dev *cdev, struct qed_sb_info *sb,
			   u16 qid, struct qed_sb_info_dbg *sb_dbg)
{
	struct qed_hwfn *hwfn = &cdev->hwfns[qid % cdev->num_hwfns];
	struct qed_ptt *ptt;
	int rc;

	if (IS_VF(cdev))
		return -EINVAL;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt) {
		DP_NOTICE(hwfn, "Can't acquire PTT\n");
		return -EAGAIN;
	}

	memset(sb_dbg, 0, sizeof(*sb_dbg));
	rc = qed_int_get_sb_dbg(hwfn, ptt, sb, sb_dbg);

	qed_ptt_release(hwfn, ptt);
	return rc;
}

static int qed_read_module_eeprom(struct qed_dev *cdev, char *buf,
				  u8 dev_addr, u32 offset, u32 len)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *ptt;
	int rc = 0;

	if (IS_VF(cdev))
		return 0;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return -EAGAIN;

	rc = qed_mcp_phy_sfp_read(hwfn, ptt, MFW_PORT(hwfn), dev_addr,
				  offset, len, buf);

	qed_ptt_release(hwfn, ptt);

	return rc;
}

static int qed_get_sfp_stats(struct qed_dev *cdev, struct qed_sfp_stats *sfp)
{
	u32 vcc, txb, txp, rxp, temp, addr, len, media;
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	u8 lane, buf[64], port = MFW_PORT(hwfn);
	struct qed_ptt *ptt;
	int rc, i;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return -EAGAIN;

	qed_mcp_get_media_type(hwfn, ptt, &media);
	if (media == MEDIA_UNSPECIFIED  || media == MEDIA_NOT_PRESENT) {
		DP_ERR(cdev, "Unknown media type 0x%x\n", media);
		rc = -EINVAL;
		goto out;
	}

	memset(&buf, 0, sizeof(buf));
	memset(sfp, 0, sizeof(*sfp));
	rc = qed_mcp_phy_sfp_read(hwfn, ptt, port, I2C_TRANSCEIVER_ADDR, 0, 1,
				  buf);
	if (rc)
		goto out;

	sfp->sfp_type = buf[0];
	switch (sfp->sfp_type) {
	case 0x3: /* SFP, SFP+, SFP-28 */
		len = 0xa;
		lane = 1;
		addr = I2C_DEV_ADDR_A2;
		temp = SFP_EEPROM_A2_TEMPERATURE_ADDR;
		vcc = SFP_EEPROM_A2_VCC_ADDR - temp;
		txb = SFP_EEPROM_A2_TX_BIAS_ADDR - temp;
		txp = SFP_EEPROM_A2_TX_POWER_ADDR - temp;
		rxp = SFP_EEPROM_A2_RX_POWER_ADDR - temp;
		break;
	case 0xc: /* QSFP */
	case 0xd: /* QSFP+ */
	case 0x11: /* QSFP-28 */
		len = 0x24;
		lane = 4;
		addr = I2C_DEV_ADDR_A0;
		temp = QSFP_EEPROM_A0_TEMPERATURE_ADDR;
		vcc = QSFP_EEPROM_A0_VCC_ADDR - temp;
		txb = QSFP_EEPROM_A0_TX1_BIAS_ADDR - temp;
		txp = QSFP_EEPROM_A0_TX1_POWER_ADDR - temp;
		rxp = QSFP_EEPROM_A0_RX1_POWER_ADDR - temp;
		break;
	case 0x12: /* CXP2 (CXP-28) */
	default:
		DP_ERR(cdev, "SFP type 0x%x not supported\n", sfp->sfp_type);
		rc = -EINVAL;
		goto out;
	}

	/* Read temperature */
	rc = qed_mcp_phy_sfp_read(hwfn, ptt, port, addr,
				    temp, len, buf);
	if (rc)
		goto out;

	memcpy((u8 *)&sfp->temperature, buf, 2);
	memcpy((u8 *)&sfp->vcc, (buf + vcc), 2);
	for (i = 0; i < lane; i++) {
		memcpy((u8 *)&sfp->lane[i].rx_power, (buf + (rxp + i * 2)), 2);
		memcpy((u8 *)&sfp->lane[i].tx_bias, (buf + (txb + i * 2)), 2);
		memcpy((u8 *)&sfp->lane[i].tx_power, (buf + (txp + i * 2)), 2);
	}

out:
	qed_ptt_release(hwfn, ptt);

	return rc;
}

static u8 qed_get_affin_hwfn_idx(struct qed_dev *cdev)
{
	return QED_AFFIN_HWFN_IDX(cdev);
}

static bool qed_is_fip_special_mode(struct qed_dev *cdev)
{
	return qed_is_mf_fip_special(cdev);
}

#define QED_MFW_REPORT_STR_SIZE 256

static void qed_mfw_report(struct qed_dev *cdev, char *fmt, ...)
{
	char buf[QED_MFW_REPORT_STR_SIZE];
	struct qed_hwfn *p_hwfn;
	struct qed_ptt *p_ptt;
	va_list vl;

	va_start(vl, fmt);
	vsnprintf(buf, QED_MFW_REPORT_STR_SIZE, fmt, vl);
	va_end(vl);

	if (IS_PF(cdev)) {
		p_hwfn = QED_LEADING_HWFN(cdev);
		p_ptt = qed_ptt_acquire(p_hwfn);

		if (p_ptt) {
			qed_mcp_send_raw_debug_data(p_hwfn, p_ptt, buf, strlen(buf));
			qed_ptt_release(p_hwfn, p_ptt);
		}
	}
}

static int qed_set_grc_config(struct qed_dev *cdev, u32 cfg_id, u32 val)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);

	if (IS_VF(cdev))
		return 0;

	return qed_dbg_grc_config(hwfn, cfg_id, val);
}

static void qed_set_recov_in_prog(struct qed_dev *cdev, bool enable)
{
	cdev->recov_in_prog = enable;
}

static bool qed_get_recov_in_prog(struct qed_dev *cdev)
{
	return cdev->recov_in_prog;
}

#define DCBX_ENABLE_POLL_ATTEMPTS	(600)
#define DCBX_ENABLE_POLL_MSLEEP		(100)

static void qed_wait_for_dcbx_to_enable(struct qed_dev *cdev)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	u16 iter = DCBX_ENABLE_POLL_ATTEMPTS;

	if (roce_lag_delay == 0)
		return;

	while (!qed_dcbx_is_enabled(hwfn) && iter--)
		msleep(DCBX_ENABLE_POLL_MSLEEP);

	DP_VERBOSE(cdev, NETIF_MSG_LINK,
		   "DCBx is enabled - waiting %u ms before failback\n",
		   roce_lag_delay);

	msleep(roce_lag_delay);
}

static int qed_get_esl_status(struct qed_dev *cdev, bool *esl_active)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *ptt;
	int rc = 0;

	*esl_active = false;
	if (IS_VF(cdev))
		return 0;

	ptt = qed_ptt_acquire(hwfn);
	if (!ptt)
		return -EAGAIN;

	rc = qed_mcp_get_esl_status(hwfn, ptt, esl_active);

	qed_ptt_release(hwfn, ptt);

	return rc;
}

static void qed_set_dev_reuse(struct qed_dev *cdev, bool flag)
{
	cdev->b_reuse_dev = flag;
}

static void qed_set_aer_state(struct qed_dev *cdev, bool aer)
{
	cdev->aer_in_prog = aer;
}

#define		REG_HOT_RESET_CNT_READ_DELAY	1000
#define		REG_HOT_RESET_CNT_READ_LOOP	   2
/* Driver uses HOT_RESET_PREPARED_CNT register to determine if mfw has received
 * hot-reset or not. The value is cached in dev->hot_reset_count and is updated
 * during the init. MFW doesn't implement hot-reset for BB adapters, hence
 * driver requests recovery for such devices. dev->aer_recov_prog tracks whether
 * the device is in AER recovery mode or not. Driver will not invoke any device
 * related functionality in such cases.
 */
static bool qed_is_hot_reset_occured_or_in_prgs(struct qed_dev *cdev)
{
	struct qed_hwfn *hwfn = QED_LEADING_HWFN(cdev);
	bool rc = false;
	u32 val, i;

	if (IS_VF(cdev))
		return false;

	/* Bigbear device doesn't support PCI Hot-reset */
	if (QED_IS_BB(cdev))
		return true;

	/* mfw takes 2 sec to handle the hot-reset event */
	for (i = 0; i < REG_HOT_RESET_CNT_READ_LOOP; i++) {
		msleep(REG_HOT_RESET_CNT_READ_DELAY);
		val = qed_rd(hwfn, hwfn->p_main_ptt,
			     MISCS_REG_HOT_RESET_PREPARED_CNT_K2);

		/* Register space is not valid or an update in hot-reset counter
		 * imply the device reset request is not required from driver.
		 */
		if (val == 0xffffffff || val > cdev->hot_reset_count) {
			rc = true;
			break;
		}
	}

	DP_NOTICE(cdev, "Hot reset count old = %d new = %d\n",
		  cdev->hot_reset_count, val);

	return rc;
}

static struct qed_selftest_ops qed_selftest_ops_pass = {
	INIT_STRUCT_FIELD(selftest_memory, &qed_selftest_memory),
	INIT_STRUCT_FIELD(selftest_interrupt, &qed_selftest_interrupt),
	INIT_STRUCT_FIELD(selftest_register, &qed_selftest_register),
	INIT_STRUCT_FIELD(selftest_clock, &qed_selftest_clock),
	INIT_STRUCT_FIELD(selftest_nvram, &qed_selftest_nvram),
};

extern const struct qed_dcbnl_ops qed_dcbnl_ops_pass;

const struct qed_common_ops qed_common_ops_pass = {
	INIT_STRUCT_FIELD(selftest, &qed_selftest_ops_pass),
	INIT_STRUCT_FIELD(dcb, &qed_dcbnl_ops_pass),
	INIT_STRUCT_FIELD(probe, &qed_probe),
	INIT_STRUCT_FIELD(remove, &qed_remove),
	INIT_STRUCT_FIELD(set_power_state, &qed_set_power_state),
	INIT_STRUCT_FIELD(set_name, &qed_set_name),
	INIT_STRUCT_FIELD(get_dev_name, &qed_get_dev_name),
	INIT_STRUCT_FIELD(update_pf_params, &qed_update_pf_params),
	INIT_STRUCT_FIELD(slowpath_start, &qed_slowpath_start),
	INIT_STRUCT_FIELD(slowpath_stop, &qed_slowpath_stop),
	INIT_STRUCT_FIELD(set_fp_int, &qed_set_int_fp),
	INIT_STRUCT_FIELD(get_fp_int, &qed_get_int_fp),
	INIT_STRUCT_FIELD(sb_init, &qed_sb_init),
	INIT_STRUCT_FIELD(sb_release, &qed_sb_release),
	INIT_STRUCT_FIELD(get_sb_info, &qed_get_sb_info),
	INIT_STRUCT_FIELD(simd_handler_config, &qed_simd_handler_config),
	INIT_STRUCT_FIELD(simd_handler_clean, &qed_simd_handler_clean),
	INIT_STRUCT_FIELD(can_link_change, &qed_can_link_change),
	INIT_STRUCT_FIELD(set_link, &qed_set_link),
	INIT_STRUCT_FIELD(get_link, &qed_get_current_link),
	INIT_STRUCT_FIELD(drain, &qed_drain),
	INIT_STRUCT_FIELD(update_msglvl, &qed_init_dp),
	INIT_STRUCT_FIELD(update_int_msglvl, &qed_init_int_dp),
	INIT_STRUCT_FIELD(internal_trace, &qed_dp_internal_log),
	INIT_STRUCT_FIELD(devlink_register, &qed_devlink_register),
	INIT_STRUCT_FIELD(devlink_unregister, &qed_devlink_unregister),
	INIT_STRUCT_FIELD(report_fatal_error, &qed_report_fatal_error),
#ifdef CONFIG_DEBUG_FS
	INIT_STRUCT_FIELD(dbg_grc, &qed_dbg_grc),
	INIT_STRUCT_FIELD(dbg_grc_size, &qed_dbg_grc_size),
	INIT_STRUCT_FIELD(dbg_idle_chk, &qed_dbg_idle_chk),
	INIT_STRUCT_FIELD(dbg_idle_chk_size, &qed_dbg_idle_chk_size),
	INIT_STRUCT_FIELD(dbg_mcp_trace, &qed_dbg_mcp_trace),
	INIT_STRUCT_FIELD(dbg_mcp_trace_size, &qed_dbg_mcp_trace_size),
	INIT_STRUCT_FIELD(dbg_protection_override, &qed_dbg_protection_override),
	INIT_STRUCT_FIELD(dbg_protection_override_size, &qed_dbg_protection_override_size),
	INIT_STRUCT_FIELD(dbg_reg_fifo, &qed_dbg_reg_fifo),
	INIT_STRUCT_FIELD(dbg_reg_fifo_size, &qed_dbg_reg_fifo_size),
	INIT_STRUCT_FIELD(dbg_igu_fifo, &qed_dbg_igu_fifo),
	INIT_STRUCT_FIELD(dbg_igu_fifo_size, &qed_dbg_igu_fifo_size),
	INIT_STRUCT_FIELD(dbg_phy, &qed_dbg_phy),
	INIT_STRUCT_FIELD(dbg_phy_size, &qed_dbg_phy_size),
	INIT_STRUCT_FIELD(dbg_fw_asserts, &qed_dbg_fw_asserts),
	INIT_STRUCT_FIELD(dbg_fw_asserts_size, &qed_dbg_fw_asserts_size),
	INIT_STRUCT_FIELD(dbg_ilt, &qed_dbg_ilt),
	INIT_STRUCT_FIELD(dbg_ilt_size, &qed_dbg_ilt_size),
	INIT_STRUCT_FIELD(dbg_get_debug_engine, &qed_get_debug_engine),
	INIT_STRUCT_FIELD(dbg_set_debug_engine, &qed_set_debug_engine),
	INIT_STRUCT_FIELD(dbg_all_data, &qed_dbg_all_data),
	INIT_STRUCT_FIELD(dbg_all_data_size, &qed_dbg_all_data_size),
	INIT_STRUCT_FIELD(dbg_save_all_data, &qed_dbg_save_all_data),
#endif
	INIT_STRUCT_FIELD(chain_params_init, &qed_chain_params_init),
	INIT_STRUCT_FIELD(chain_alloc, &qed_chain_alloc),
	INIT_STRUCT_FIELD(chain_free, &qed_chain_free),
	INIT_STRUCT_FIELD(chain_print, &qed_chain_print),
	INIT_STRUCT_FIELD(nvm_get_cmd, &qed_nvm_get_cmd),
	INIT_STRUCT_FIELD(nvm_set_cmd, &qed_nvm_set_cmd),
	INIT_STRUCT_FIELD(nvm_flash, &qed_nvm_flash),
	INIT_STRUCT_FIELD(nvm_get_image, &qed_nvm_get_image),
	INIT_STRUCT_FIELD(get_coalesce, &qed_get_coalesce),
	INIT_STRUCT_FIELD(set_coalesce, &qed_set_coalesce),
	INIT_STRUCT_FIELD(set_led, &qed_set_led),
	INIT_STRUCT_FIELD(recovery_process, &qed_recovery_process),
	INIT_STRUCT_FIELD(recovery_prolog, &qed_recovery_prolog),
	INIT_STRUCT_FIELD(attn_clr_enable, &qed_int_attn_clr_enable),
	INIT_STRUCT_FIELD(update_drv_state, &qed_update_drv_state),
	INIT_STRUCT_FIELD(update_mac, &qed_update_mac),
	INIT_STRUCT_FIELD(update_mtu, &qed_update_mtu),
	INIT_STRUCT_FIELD(update_wol, &qed_update_wol),
	INIT_STRUCT_FIELD(db_recovery_add, &qed_db_recovery_add),
	INIT_STRUCT_FIELD(db_recovery_del, &qed_db_recovery_del),
	INIT_STRUCT_FIELD(read_module_eeprom, &qed_read_module_eeprom),
	INIT_STRUCT_FIELD(get_vport_stats, &qed_get_vport_stats),
	INIT_STRUCT_FIELD(get_sfp_stats, &qed_get_sfp_stats),
	INIT_STRUCT_FIELD(get_affin_hwfn_idx, &qed_get_affin_hwfn_idx),
	INIT_STRUCT_FIELD(is_fip_special_mode, &qed_is_fip_special_mode),
	INIT_STRUCT_FIELD(mfw_report, &qed_mfw_report),
	INIT_STRUCT_FIELD(read_nvm_cfg, &qed_nvm_flash_cfg_read),
	INIT_STRUCT_FIELD(read_nvm_cfg_len, &qed_nvm_flash_cfg_len),
	INIT_STRUCT_FIELD(set_grc_config, &qed_set_grc_config),
	INIT_STRUCT_FIELD(set_recov_in_prog, &qed_set_recov_in_prog),
	INIT_STRUCT_FIELD(get_recov_in_prog, &qed_get_recov_in_prog),
	INIT_STRUCT_FIELD(wait_for_dcbx_to_enable, &qed_wait_for_dcbx_to_enable),
	INIT_STRUCT_FIELD(get_esl_status, &qed_get_esl_status),
	INIT_STRUCT_FIELD(set_dev_reuse, &qed_set_dev_reuse),
	INIT_STRUCT_FIELD(set_aer_state, &qed_set_aer_state),
	INIT_STRUCT_FIELD(is_hot_reset_occured_or_in_prgs, &qed_is_hot_reset_occured_or_in_prgs),
	INIT_STRUCT_FIELD(set_vf_stats_bin_id, &qed_set_vf_stats_bin_id),
};

#ifndef QED_UPSTREAM
u32 qed_get_protocol_version(enum qed_protocol protocol)
{
	switch (protocol) {
	case QED_PROTOCOL_ETH:	return QED_ETH_INTERFACE_VERSION;
	case QED_PROTOCOL_FCOE:	return QED_FCOE_INTERFACE_VERSION;
	case QED_PROTOCOL_ISCSI:	return QED_ISCSI_INTERFACE_VERSION;
	default:
		return 0;
	}
}
EXPORT_SYMBOL(qed_get_protocol_version);
#endif

#ifdef CONFIG_QED_RDMA
static void qed_get_roce_stats(struct qed_dev *cdev,
			       struct qed_rdma_stats_out_params *stats)
{
	u8 stats_queue = 0;
	int rc;

	if (!cdev) {
		memset(stats, 0, sizeof(*stats));
		return;
	}

	/* All RoCE stats are collected by the dedicated qed callback. If qedr
	 * will add non qed functionality for collecting stats in the future we
	 * should climb into qedr here, and allow that function to do its thing
	 * and drop back into qed_rdma_query_stats() instead of invoking it
	 * directly.
	 * Currently RoCE uses only function 0.
	 */
	qed_rdma_get_stats_queue(&cdev->hwfns[0], &stats_queue);
	rc = qed_rdma_query_stats(&cdev->hwfns[0], stats_queue,
				  stats);
	if (rc)
		memset(stats, 0, sizeof(*stats));
}
#endif

void qed_get_protocol_stats(struct qed_dev *cdev,
			    enum qed_mcp_protocol_type type,
			    union qed_mcp_protocol_stats *stats)
{
	struct qed_eth_stats eth_stats;
#ifdef CONFIG_QED_RDMA
	struct qed_rdma_stats_out_params roce_stats;
#endif

	memset(stats, 0, sizeof(*stats));

	/* TODO - all of this is ifdefs here are incorrect [protocol headers
	 * should always be included and have empty implementation in case
	 * protocol is left out]. But for now this is required as it allows
	 * tedibear to compile [doesn't take anything but L2].
	 */
	switch (type) {
	case QED_MCP_LAN_STATS:
		qed_get_vport_stats(cdev, &eth_stats);
		stats->lan_stats.ucast_rx_pkts = eth_stats.common.rx_ucast_pkts;
		stats->lan_stats.ucast_tx_pkts = eth_stats.common.tx_ucast_pkts;
		/* @@@TBD - L2 driver doesn't have this info */
		stats->lan_stats.fcs_err = -1;
		break;
#ifdef CONFIG_QED_RDMA
	case QED_MCP_RDMA_STATS:
		qed_get_roce_stats(cdev, &roce_stats);
		stats->rdma_stats.rx_bytes = roce_stats.rcv_bytes;
		stats->rdma_stats.rx_pkts = roce_stats.rcv_pkts;
		stats->rdma_stats.tx_byts = roce_stats.sent_bytes;
		stats->rdma_stats.tx_pkts = roce_stats.sent_pkts;
		break;
#endif
	case QED_MCP_FCOE_STATS:
		qed_get_protocol_stats_fcoe(cdev, &stats->fcoe_stats);
		break;
	case QED_MCP_ISCSI_STATS:
		qed_get_protocol_stats_iscsi(cdev, &stats->iscsi_stats);
		break;
	default:
		DP_ERR(cdev, "Invalid protocol type = %d\n", type);
		return;
	}
}

int qed_mfw_tlv_req(struct qed_hwfn *hwfn)
{
	DP_VERBOSE(hwfn->cdev, NETIF_MSG_DRV,
		   "Scheduling slowpath task [Flag: %d]\n",
		   QED_SLOWPATH_MFW_TLV_REQ);

	return qed_slowpath_delayed_work(hwfn, QED_SLOWPATH_MFW_TLV_REQ, 0);
}

static void
qed_fill_generic_tlv_data(struct qed_dev *cdev, struct qed_mfw_tlv_generic *tlv)
{
	struct qed_common_cb_ops *op = cdev->protocol_ops.common;
	struct qed_eth_stats_common *p_common;
	struct qed_generic_tlvs gen_tlvs;
	struct qed_eth_stats stats;
	int i;

	memset(&gen_tlvs, 0, sizeof(gen_tlvs));
	op->get_generic_tlv_data(cdev->ops_cookie, &gen_tlvs);

	if (gen_tlvs.feat_flags & QED_TLV_IP_CSUM)
		tlv->flags.ipv4_csum_offload = true;
	if (gen_tlvs.feat_flags & QED_TLV_LSO)
		tlv->flags.lso_supported = true;
	tlv->flags.b_set = true;

	for (i = 0; i < QED_MFW_TLV_MAC_COUNT; i++) {
		if (is_valid_ether_addr(gen_tlvs.mac[i])) {
			ether_addr_copy(tlv->mac[i], gen_tlvs.mac[i]);
			tlv->mac_set[i] = true;
		}
	}

	qed_get_vport_stats(cdev, &stats);
	p_common = &stats.common;
	tlv->rx_frames = p_common->rx_ucast_pkts + p_common->rx_mcast_pkts +
			 p_common->rx_bcast_pkts;
	tlv->rx_frames_set = true;
	tlv->rx_bytes = p_common->rx_ucast_bytes + p_common->rx_mcast_bytes +
			p_common->rx_bcast_bytes;
	tlv->rx_bytes_set = true;
	tlv->tx_frames = p_common->tx_ucast_pkts + p_common->tx_mcast_pkts +
			 p_common->tx_bcast_pkts;
	tlv->tx_frames_set = true;
	tlv->tx_bytes = p_common->tx_ucast_bytes + p_common->tx_mcast_bytes +
			p_common->tx_bcast_bytes;
	tlv->rx_bytes_set = true;
}

int qed_mfw_fill_tlv_data(struct qed_hwfn *hwfn, enum qed_mfw_tlv_type type,
			  union qed_mfw_tlv_data *tlv_buf)
{
	struct qed_dev *cdev = hwfn->cdev;
	struct qed_common_cb_ops *ops = cdev->protocol_ops.common;

	/* TODO - temporary until Storage fills it [prevent break] */
	if (!ops || !ops->get_protocol_tlv_data || !ops->get_generic_tlv_data) {
		DP_NOTICE(hwfn, "Can't collect TLV management info\n");
		return -EINVAL;
	}

	switch (type) {
	case QED_MFW_TLV_GENERIC:
		qed_fill_generic_tlv_data(hwfn->cdev, &tlv_buf->generic);
		break;
	case QED_MFW_TLV_ETH:
		ops->get_protocol_tlv_data(cdev->ops_cookie, &tlv_buf->eth);
		break;
	case QED_MFW_TLV_FCOE:
		ops->get_protocol_tlv_data(cdev->ops_cookie, &tlv_buf->fcoe);
		break;
	case QED_MFW_TLV_ISCSI:
		ops->get_protocol_tlv_data(cdev->ops_cookie, &tlv_buf->iscsi);
		break;
	default:
		break;
	}

	return 0;
}

int qed_hw_attr_update(struct qed_hwfn *hwfn, enum qed_hw_info_change attr)
{
	struct qed_dev *cdev = hwfn->cdev;
	struct qed_common_cb_ops *ops = cdev->protocol_ops.common;

	if (ops && ops->hw_attr_update)
		ops->hw_attr_update(cdev->ops_cookie, attr);

	return 0;
}

#ifdef _MISSING_CRC8_MODULE /* ! QED_UPSTREAM */
void qed_crc8_populate_msb(u8 table[CRC8_TABLE_SIZE], u8 polynomial)
{
	int i, j;
	const u8 msbit = 0x80;
	u8 t = msbit;

	table[0] = 0;

	for (i = 1; i < CRC8_TABLE_SIZE; i *= 2) {
		t = (t << 1) ^ (t & msbit ? polynomial : 0);
		for (j = 0; j < i; j++)
			table[i+j] = table[j] ^ t;
	}
}

u8 qed_crc8(const u8 table[CRC8_TABLE_SIZE], u8 *pdata, size_t nbytes, u8 crc)
{
	/* loop over the buffer data */
	while (nbytes-- > 0)
		crc = table[(crc ^ *pdata++) & 0xff];

	return crc;
}
#endif
