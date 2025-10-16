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
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <asm/byteorder.h>
#include <asm/param.h>
#include <linux/io.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)) /* QEDE_UPSTREAM */
#include <linux/netdev_features.h>
#endif
#include <linux/udp.h>
#include <linux/tcp.h>
#ifdef _HAS_ADD_VXLAN_PORT /* QEDE_UPSTREAM */
#include <net/vxlan.h>
#endif
#ifdef _HAS_ADD_GENEVE_PORT
#include <net/geneve.h>
#endif
#if defined(_HAS_NDO_UDP_TUNNEL_CONFIG) || \
    defined(_HAS_NDO_EXT_UDP_TUNNEL_CONFIG) /* QEDE_UPSTREAM */
#include <net/udp_tunnel.h>
#endif
#include <linux/ip.h>
#include <net/ipv6.h>
#include <net/tcp.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/pkt_sched.h>
#include <linux/ethtool.h>
#include <linux/in.h>
#include <linux/random.h>
#include <net/ip6_checksum.h>
#include <linux/bitops.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/crash_dump.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif
#include <net/udp.h>
#include <linux/workqueue.h>
#include <linux/stat.h>
#include <linux/aer.h>

#include "qed_if.h"
#include "qede_compat.h"
#include "qede_hsi.h"
#include "qede.h"
#include "qede_ptp.h"

#ifdef ENC_SUPPORTED
#include <net/gre.h>
#endif

#if defined(_HAS_NDO_SETUP_TC_CHAIN) || defined(_HAS_NDO_SETUP_TC_HANDLE) || defined(_HAS_TC_SETUP_TYPE) /* ! QEDE_UPSTREAM */
#ifndef _HAS_TC_SETUP_QDISC_MQPRIO
#define TC_SETUP_QDISC_MQPRIO TC_SETUP_MQPRIO
#endif
#endif

static char version[] =
		"QLogic FastLinQ 4xxxx Ethernet Driver " DRV_MODULE_NAME " " DRV_MODULE_VERSION;

MODULE_DESCRIPTION("QLogic FastLinQ 4xxxx Ethernet Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_MODULE_VERSION);

#ifndef TEDIBEAR /* QEDE_UPSTREAM */
static
#endif
uint debug;
module_param(debug, uint, S_IRUGO);
MODULE_PARM_DESC(debug, " Default debug msglevel");

#define TX_TIMEOUT (5)

static uint watchdog_timeo = TX_TIMEOUT;
module_param(watchdog_timeo, uint, S_IRUGO);
MODULE_PARM_DESC(watchdog_timeo, " Default watchdog tx timeout:(5 sec default)");

static uint int_debug = QED_DP_INT_LOG_DEFAULT_MASK;
module_param(int_debug, uint, S_IRUGO);
MODULE_PARM_DESC(int_debug, " Default debug msglevel for internal trace");

#ifndef QEDE_UPSTREAM /* ! QEDE_UPSTREAM */
static uint int_mode;
module_param(int_mode, uint, S_IRUGO);
MODULE_PARM_DESC(int_mode, " Force interrupt mode other than MSI-X:(1 INT#x; 2 MSI)");

uint gro_disable = 0;
module_param(gro_disable, uint, S_IRUGO);
MODULE_PARM_DESC(gro_disable, " Force HW gro disable:(0 enable (default); 1 disable)");

static uint err_flags_override;
module_param(err_flags_override, uint, S_IRUGO);
MODULE_PARM_DESC(err_flags_override, " Bitmap for disabling or forcing the actions taken according to the respective error flags bits");

static uint rdma_lag_support = 1;
module_param(rdma_lag_support, uint, S_IRUGO);
MODULE_PARM_DESC(rdma_lag_support, " RDMA Bonding support enable - preview mode:(0 disable; 1 enable (default))");

bool numa_native = 1;
module_param(numa_native, bool, 0444);
MODULE_PARM_DESC(numa_native, "Enable NUMA Aware Memory Allocation & IRQ allocation (0=disabled, 1=enabled(default)");
#endif

static const struct qed_eth_ops *qed_ops;

#define CHIP_NUM_57980S_40		0x1634
#define CHIP_NUM_57980S_10		0x1666
#define CHIP_NUM_57980S_MF		0x1636 /* To be removed */
#define CHIP_NUM_57980S_100		0x1644
#define CHIP_NUM_57980S_50		0x1654
#define CHIP_NUM_57980S_25		0x1656
#define CHIP_NUM_57980S_IOV		0x1664
#define CHIP_NUM_AH			0x8070
#define CHIP_NUM_AH_IOV			0x8090
#define CHIP_NUM_AH_T_MCM		0x80d0
#define CHIP_NUM_E5			0x8170
#define CHIP_NUM_E5_IOV			0x8190

#ifndef PCI_DEVICE_ID_NX2_57980E
#define PCI_DEVICE_ID_57980S_40		CHIP_NUM_57980S_40
#define PCI_DEVICE_ID_57980S_10		CHIP_NUM_57980S_10
#define PCI_DEVICE_ID_57980S_MF		CHIP_NUM_57980S_MF
#define PCI_DEVICE_ID_57980S_100	CHIP_NUM_57980S_100
#define PCI_DEVICE_ID_57980S_50		CHIP_NUM_57980S_50
#define PCI_DEVICE_ID_57980S_25		CHIP_NUM_57980S_25
#define PCI_DEVICE_ID_57980S_IOV	CHIP_NUM_57980S_IOV
#define PCI_DEVICE_ID_AH		CHIP_NUM_AH
#define PCI_DEVICE_ID_AH_IOV		CHIP_NUM_AH_IOV
#define PCI_DEVICE_ID_AH_T_MCM		CHIP_NUM_AH_T_MCM
#define PCI_DEVICE_ID_E5		CHIP_NUM_E5
#define PCI_DEVICE_ID_E5_IOV		CHIP_NUM_E5_IOV

#endif

enum qede_pci_private {
	QEDE_PRIVATE_PF,
	QEDE_PRIVATE_VF
};

static const struct pci_device_id qede_pci_tbl[] = {
	{ PCI_VDEVICE(QLOGIC, PCI_DEVICE_ID_57980S_40), QEDE_PRIVATE_PF },
	{ PCI_VDEVICE(QLOGIC, PCI_DEVICE_ID_57980S_10), QEDE_PRIVATE_PF },
	{ PCI_VDEVICE(QLOGIC, PCI_DEVICE_ID_57980S_MF), QEDE_PRIVATE_PF },
	{ PCI_VDEVICE(QLOGIC, PCI_DEVICE_ID_57980S_100), QEDE_PRIVATE_PF },
	{ PCI_VDEVICE(QLOGIC, PCI_DEVICE_ID_57980S_50), QEDE_PRIVATE_PF },
	{ PCI_VDEVICE(QLOGIC, PCI_DEVICE_ID_57980S_25), QEDE_PRIVATE_PF },
#ifdef CONFIG_QED_SRIOV
	{ PCI_VDEVICE(QLOGIC, PCI_DEVICE_ID_57980S_IOV), QEDE_PRIVATE_VF },
#endif
	{ PCI_VDEVICE(QLOGIC, PCI_DEVICE_ID_AH), QEDE_PRIVATE_PF },
#ifdef CONFIG_QED_SRIOV
	{ PCI_VDEVICE(QLOGIC, PCI_DEVICE_ID_AH_IOV), QEDE_PRIVATE_VF },
#endif
	{ PCI_VDEVICE(QLOGIC, PCI_DEVICE_ID_AH_T_MCM), QEDE_PRIVATE_PF },
	{ PCI_VDEVICE(QLOGIC, PCI_DEVICE_ID_E5), QEDE_PRIVATE_PF },
#ifdef CONFIG_QED_SRIOV
	{ PCI_VDEVICE(QLOGIC, PCI_DEVICE_ID_E5_IOV), QEDE_PRIVATE_VF },
#endif
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, qede_pci_tbl);

int qede_probe(struct pci_dev *pdev, const struct pci_device_id *id);
static void qede_io_resume(struct pci_dev *pdev);
static pci_ers_result_t
qede_io_error_detected(struct pci_dev *pdev, pci_channel_state_t state);
static pci_ers_result_t qede_io_slot_reset(struct pci_dev *pdev);
static int qede_suspend(struct pci_dev *pdev, pm_message_t state);
static int qede_resume(struct pci_dev *pdev);
#ifdef SYS_INC_RESET_PREP
static void qede_reset_prepare(struct pci_dev *pdev);
static void qede_reset_done(struct pci_dev *pdev);
#endif

#define DUMP_PACKET_DATA 0

/* Utilize last protocol index for XDP */
#define XDP_PI	11

/* Q-in-Q header length */
#define QINQ_HDR_LEN 8

void qede_remove(struct pci_dev *pdev);
void qede_shutdown(struct pci_dev *pdev);

#ifdef HAS_BOND_OFFLOAD_SUPPORT /* QEDE_UPSTREAM */
static void qede_handle_link_change(struct qede_dev *edev,
				    struct qed_link_output *link);
#endif

static void qede_link_update(void *dev, struct qed_link_output *link);
static void qede_schedule_recovery_handler(void *dev);
static void qede_recovery_handler(struct qede_dev *edev);
static void qede_schedule_hw_err_handler(void *dev,
					 enum qed_hw_err_type err_type);
static void qede_get_eth_tlv_data(void *edev, void *data);
static void qede_get_generic_tlv_data(void *edev,
				      struct qed_generic_tlvs *data);
static void qede_fan_fail_handler(struct work_struct *work);
static void qede_generic_hw_err_handler(struct qede_dev *edev);

static void qede_init_debugfs(void);
static void qede_remove_debugfs(void);
static void qede_debugfs_add_features(struct qede_dev *edev);
static void qede_reinit_fwd_dev_info(struct qede_dev *edev);

#if HAS_NDO(DFWD_ADD_STATION) /* QEDE_UPSTREAM */
static void *qede_fwd_add_station(struct net_device *real_dev,
				  struct net_device *upper_dev);
static void qede_fwd_del_station(struct net_device *real_dev, void *priv);
#endif

#ifdef SYS_INC_SRIOV
#if HAS_NDO(VF_VLAN_PROTO) /* QEDE_UPSTREAM */
static int qede_set_vf_vlan(struct net_device *ndev, int vf, u16 vlan, u8 qos,
			    __be16 vlan_proto)
#else
static int qede_set_vf_vlan(struct net_device *ndev, int vf, u16 vlan, u8 qos)
#endif
{
	struct qede_dev *edev = netdev_priv(ndev);

	if (vlan > 4095) {
		DP_NOTICE(edev, "Illegal vlan value %d\n", vlan);
		return -EINVAL;
	}

#if HAS_NDO(VF_VLAN_PROTO) /* QEDE_UPSTREAM */
	if (vlan_proto != htons(ETH_P_8021Q))
		return -EPROTONOSUPPORT;
#endif

	DP_VERBOSE(edev, QED_MSG_IOV, "Setting Vlan 0x%04x to VF [%d]\n",
		   vlan, vf);

	return edev->ops->iov->set_vlan(edev->cdev, vlan, vf);
}

static int qede_set_vf_mac(struct net_device *ndev, int vfidx, u8 *mac)
{
	struct qede_dev *edev = netdev_priv(ndev);

	DP_VERBOSE(edev, QED_MSG_IOV, "Setting MAC %02x:%02x:%02x:%02x:%02x:%02x to VF [%d]\n",
		   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], vfidx);

	if (!is_valid_ether_addr(mac)) {
		DP_VERBOSE(edev, QED_MSG_IOV, "MAC address isn't valid\n");
		return -EINVAL;
	}

	return edev->ops->iov->set_mac(edev->cdev, mac, vfidx);
}

static int qede_sriov_configure(struct pci_dev *pdev, int num_vfs_param)
{
	struct qede_dev *edev = netdev_priv(pci_get_drvdata(pdev));
	struct qed_dev_info *qed_info = &edev->dev_info.common;
	struct qed_update_vport_params *vport_params;
	int rc;

	if (edev->num_fwd_devs) {
		DP_VERBOSE(edev, QED_MSG_IOV,
			   "L2 forwarding offload enabled, can't configure SRIOV\n");
		return -EINVAL;
	}

	vport_params = vzalloc(sizeof(*vport_params));
	if (!vport_params)
		return -ENOMEM;

	DP_VERBOSE(edev, QED_MSG_IOV, "Requested %d VFs\n", num_vfs_param);

	rc = edev->ops->iov->configure(edev->cdev, num_vfs_param);

	/* Enable/Disable Tx switching for PF */
	if (rc == num_vfs_param) {
		edev->num_vfs = rc;
		if (netif_running(edev->ndev) &&
		    !qed_info->b_inter_pf_switch &&
		    qed_info->tx_switching) {
			vport_params->vport_id = QEDE_BASE_DEV_VPORT_ID;
			vport_params->update_tx_switching_flg = 1;
			vport_params->tx_switching_flg = num_vfs_param ? 1 : 0;
			edev->ops->vport_update(edev->cdev, vport_params);
		}
	}

	vfree(vport_params);
	return rc;
}
#endif

#if defined(HAS_SRIOV_PCI_DRIVER_RH) /* ! QEDE_UPSTREAM */
static struct pci_driver_rh qede_pci_driver_rh = {
#ifdef SYS_INC_SRIOV
	INIT_STRUCT_FIELD(sriov_configure, qede_sriov_configure),
#endif
};
#endif

static struct pci_error_handlers qede_err_handler = {
	.error_detected = qede_io_error_detected,
	.slot_reset = qede_io_slot_reset,
	.resume = qede_io_resume,
#ifdef SYS_INC_RESET_PREP
	.reset_prepare = qede_reset_prepare,
	.reset_done = qede_reset_done,
#endif
};

static struct pci_driver qede_pci_driver = {
	INIT_STRUCT_FIELD(name, DRV_MODULE_NAME),
	INIT_STRUCT_FIELD(id_table, qede_pci_tbl),
	INIT_STRUCT_FIELD(probe, qede_probe),
	INIT_STRUCT_FIELD(remove, __devexit_p(qede_remove)),
	INIT_STRUCT_FIELD(shutdown, qede_shutdown),
	INIT_STRUCT_FIELD(err_handler, &qede_err_handler),
#ifndef HAS_SRIOV_PCI_DRIVER_RH /* QEDE_UPSTREAM */
#ifdef SYS_INC_SRIOV
	INIT_STRUCT_FIELD(sriov_configure, qede_sriov_configure),
#endif
#endif
#if defined(HAS_SRIOV_PCI_DRIVER_RH) /* ! QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(rh_reserved, &qede_pci_driver_rh),
#endif
	INIT_STRUCT_FIELD(suspend, qede_suspend),
	INIT_STRUCT_FIELD(resume, qede_resume),
     /* INIT_STRUCT_FIELD(err_handler, NULL), */
};

static struct qed_eth_cb_ops qede_ll_ops = {
	{
		INIT_STRUCT_FIELD(arfs_filter_op, qede_arfs_filter_op),
		INIT_STRUCT_FIELD(link_update, qede_link_update),
		INIT_STRUCT_FIELD(schedule_recovery_handler, qede_schedule_recovery_handler),
		INIT_STRUCT_FIELD(schedule_hw_err_handler, qede_schedule_hw_err_handler),
		INIT_STRUCT_FIELD(get_generic_tlv_data, qede_get_generic_tlv_data),
		INIT_STRUCT_FIELD(get_protocol_tlv_data, qede_get_eth_tlv_data),
	},
	INIT_STRUCT_FIELD(force_mac, qede_force_mac),
	INIT_STRUCT_FIELD(ports_update, qede_udp_ports_update),
};

struct qede_fan_fail_info {
	struct list_head list;
	struct pci_dev *pdev;
};
static LIST_HEAD(fan_fail_list);
static DEFINE_SPINLOCK(fan_fail_lock);
static DECLARE_DELAYED_WORK(fan_fail_task, qede_fan_fail_handler);

static int qede_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	struct qede_dev *edev;

	if (!ndev) {
		dev_err(&pdev->dev, "BAD net device\n");
		return -ENODEV;
	}

	edev = netdev_priv(ndev);
	DP_ERR(edev, "driver does not support suspend.\n");

	return -EOPNOTSUPP;
}

static int qede_resume(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	struct qede_dev *edev;

	if (!ndev) {
		dev_err(&pdev->dev, "BAD net device\n");
		return -ENODEV;
	}

	edev = netdev_priv(ndev);
	DP_ERR(edev, "driver does not support resume.\n");

	return -EOPNOTSUPP;
}

#ifdef HAS_BOND_OFFLOAD_SUPPORT /* QEDE_UPSTREAM */

static int
qede_handle_lower_state_change_event(struct net_device *ndev,
				     struct netdev_notifier_changelowerstate_info *info)
{
	struct netdev_lag_lower_state_info *lag_lower_info;
	struct qede_dev *edev = netdev_priv(ndev);

#ifndef QEDE_UPSTREAM /* ! QEDE_UPSTREAM */
	if (!rdma_lag_support)
		return 0;
#endif

	if (!netif_is_lag_port(ndev))
		return 0;

	lag_lower_info = info->lower_state_info;
	edev->lag.port_active = lag_lower_info->link_up &&
				lag_lower_info->tx_enabled;

	if (!edev->rdma_info.lag_enabled || !edev->ops->lag ||
	    !edev->lag.lag_cdev)
		return 0;

	if (edev->lag.port_active)
		edev->ops->common->wait_for_dcbx_to_enable(edev->cdev);

	/* There is a chance that lag_cdev doesn't exist anymore as it is
	 * shared with the other device in the bond and could be set to NULL
	 * while waiting for DCBX.
	 */
	if (!edev->lag.lag_cdev)
		return 0;

	/* Master slave device -> we can update ourselves */
	edev->ops->lag->lag_modify(edev->lag.lag_cdev,
				   PCI_FUNC(edev->pdev->devfn),
				   edev->lag.port_active);

	return 0;
}

static bool qede_validate_bond(struct qede_dev *edev0, struct qede_dev *edev1,
			       int num_slaves, unsigned int mode)
{
	if (num_slaves == 2 && edev0 && edev1 &&
	    (edev0->pdev->bus->number == edev1->pdev->bus->number) &&
	    qede_rdma_supported(edev0) && /* it's enough that RoCE is enabled only on edev0 */
	    (mode == NETDEV_LAG_TX_TYPE_ACTIVEBACKUP ||
	     mode == NETDEV_LAG_TX_TYPE_HASH)) {
		u8 pci_func0 = PCI_FUNC(edev0->pdev->devfn);
		u8 pci_func1 = PCI_FUNC(edev1->pdev->devfn);

		/* check whether the interfaces are belong to either PF0 and PF1 or PF2 and PF3 */
		if (pci_func0 >> 1 != pci_func1 >> 1) {
			DP_NOTICE(edev0, "RDMA bonding - Can't bond PF%d and PF%d\n", pci_func0, pci_func1);
			return false;
		} else {
			return true;
		}
	}

	return false;
}

static int
qede_handle_changeupper_event(struct qede_dev *edev,
			      struct netdev_notifier_changeupper_info *info)
{
	struct net_device *upper = info->upper_dev, *ndev_tmp;
	struct netdev_lag_upper_info *lag_upper_info = NULL;
	struct qede_dev *edev0 = NULL, *edev1 = NULL;
	struct ethtool_drvinfo drvinfo;
	unsigned int mode = 0;
	bool is_bond = false;
	int num_slaves = 0;
	u8 active_ports;
	int rc;

#ifndef QEDE_UPSTREAM /* ! QEDE_UPSTREAM */
	if (!rdma_lag_support)
		return 0;
#endif

	if (!netif_is_lag_master(upper))
		return 0;

	if (info->linking) {
		lag_upper_info = info->upper_info;
		mode = lag_upper_info->tx_type;
	}

	rcu_read_lock();
	for_each_netdev_in_bond_rcu(upper, ndev_tmp) {
		memset(&drvinfo, 0, sizeof(drvinfo));

		/* Check if qede netdevice is bonded */
		if (ndev_tmp->ethtool_ops &&
		    ndev_tmp->ethtool_ops->get_drvinfo) {
			ndev_tmp->ethtool_ops->get_drvinfo(ndev_tmp, &drvinfo);
			if (!strcmp(drvinfo.driver, DRV_MODULE_NAME)) {
				if (num_slaves == 0) {
					edev0 = netdev_priv(ndev_tmp);
				} else if (num_slaves == 1) {
					edev1 = netdev_priv(ndev_tmp);
				} else {
					DP_NOTICE(edev, "RDMA bonding - bonding of more than 2 interfaces isn't supported\n");
					rcu_read_unlock();
					return 0;
				}
			}
		}
		num_slaves++;
	}
	rcu_read_unlock();

	/* Validate if it's our bond */
	is_bond = qede_validate_bond(edev0, edev1, num_slaves, mode);

	if (is_bond && ((edev0 && !edev0->rdma_info.lag_enabled) ||
			(edev1 && !edev1->rdma_info.lag_enabled))) {
		enum qed_lag_type lag_type;

		if (mode == NETDEV_LAG_TX_TYPE_ACTIVEBACKUP)
			lag_type = QED_LAG_TYPE_ACTIVEBACKUP;
		else
			lag_type = QED_LAG_TYPE_ACTIVEACTIVE;

		active_ports = (edev0->lag.port_active << PCI_FUNC(edev0->pdev->devfn)) |
			       (edev1->lag.port_active << PCI_FUNC(edev1->pdev->devfn));

		rc = edev0->ops->lag->lag_create(edev0->cdev,
						 lag_type,
						 NULL, NULL,
						 active_ports);
		if (rc) {
			DP_NOTICE(edev0,
				  "RDMA bonding will not be configured for pf%u ndev=%p\n",
				  PCI_FUNC(edev0->pdev->devfn), edev0->ndev);
		} else {
			edev0->rdma_info.lag_enabled = true;
			DP_NOTICE(edev0, "Enabled LAG successfully\n");
		}

		rc = edev1->ops->lag->lag_create(edev1->cdev,
						 lag_type,
						 NULL, NULL,
						 active_ports);
		if (rc) {
			DP_NOTICE(edev1,
				  "RDMA bonding will not be configured for pf%u ndev=%p\n",
				  PCI_FUNC(edev1->pdev->devfn), edev1->ndev);
		} else {
			edev1->rdma_info.lag_enabled = true;
			DP_NOTICE(edev1, "Enabled LAG successfully\n");
		}


		if (PCI_FUNC(edev0->pdev->devfn) % 2 == 0)
			edev0->lag.lag_cdev = edev1->lag.lag_cdev = edev0->cdev;
		else
			edev0->lag.lag_cdev = edev1->lag.lag_cdev = edev1->cdev;

	} else if (!is_bond && edev && edev->rdma_info.lag_enabled) {
		rc = edev->ops->lag->lag_destroy(edev->cdev);
		if (rc)
			DP_NOTICE(edev, "failed to destory lag\n");
		else
			DP_NOTICE(edev, "Disabled LAG successfully\n");

		edev->rdma_info.lag_enabled = false;
		edev->lag.lag_cdev = NULL;
	}

	return 0;
}
#endif

static int qede_netdev_event(struct notifier_block *this, unsigned long event,
			     void *ptr)
{
	struct net_device *ndev = netdev_notifier_info_to_dev(ptr);
	struct ethtool_drvinfo drvinfo;
	struct qede_dev *edev = NULL;

        /* Check whether this is a qede device */
        if (!ndev || !ndev->ethtool_ops || !ndev->ethtool_ops->get_drvinfo)
                goto done;

        memset(&drvinfo, 0, sizeof(drvinfo));
        ndev->ethtool_ops->get_drvinfo(ndev, &drvinfo);
        if (strcmp(drvinfo.driver, DRV_MODULE_NAME))
                goto done;

	edev = netdev_priv(ndev);

	switch (event) {
	case NETDEV_CHANGENAME:
		/* Notify qed of the name change */
		if (!edev->ops || !edev->ops->common)
			goto done;
		edev->ops->common->set_name(edev->cdev, edev->ndev->name);
		break;
	case NETDEV_CHANGEADDR:
		edev = netdev_priv(ndev);
		qede_rdma_event_changeaddr(edev);
		break;
#ifdef HAS_BOND_OFFLOAD_SUPPORT /* QEDE_UPSTREAM */
	case NETDEV_CHANGEUPPER:
		qede_handle_changeupper_event(edev, ptr);
		break;
	case NETDEV_CHANGELOWERSTATE:
		qede_handle_lower_state_change_event(ndev, ptr);
		break;
#endif
	}
done:
	return NOTIFY_DONE;
}

static struct notifier_block qede_netdev_notifier = {
	INIT_STRUCT_FIELD(notifier_call, qede_netdev_event),
};

#ifndef TEDIBEAR /* QEDE_UPSTREAM */
static
#endif
int __init qede_init(void)
{
	int ret;
 #ifndef QEDE_UPSTREAM
	u32 qed_ver;
#endif

	pr_info("qede_init: %s\n", version);

#ifndef QEDE_UPSTREAM
	qed_ver = qed_get_protocol_version(QED_PROTOCOL_ETH);
	if (qed_ver !=  QEDE_ETH_INTERFACE_VERSION) {
		pr_notice("Version mismatch [%08x != %08x]\n",
			  qed_ver,
			  QEDE_ETH_INTERFACE_VERSION);
		return -EINVAL;
	}

	qed_ops = qed_get_eth_ops(QEDE_ETH_INTERFACE_VERSION);
#else
	qed_ops = qed_get_eth_ops();
#endif

	qede_init_debugfs();

	if (!qed_ops) {
		pr_notice("Failed to get qed ethtool operations\n");
		return -EINVAL;
	}

	/* Must register notifier before pci ops, since we might miss
	 * interface rename after pci probe and netdev registeration.
	 */
#ifdef _HAS_REGISTER_NETDEVICE_NOTIFIER_RH /* ! QED_UPSTREAM */
	ret = register_netdevice_notifier_rh(&qede_netdev_notifier);
#else
	ret = register_netdevice_notifier(&qede_netdev_notifier);
#endif
	if (ret) {
		pr_notice("Failed to register netdevice_notifier\n");
		qed_put_eth_ops();
		return -EINVAL;
	}

	ret = pci_register_driver(&qede_pci_driver);
	if (ret) {
		pr_notice("Failed to register driver\n");
#ifdef _HAS_REGISTER_NETDEVICE_NOTIFIER_RH /* ! QED_UPSTREAM */
		unregister_netdevice_notifier_rh(&qede_netdev_notifier);
#else
		unregister_netdevice_notifier(&qede_netdev_notifier);
#endif
		qed_put_eth_ops();
		return -EINVAL;
	}

	return 0;
}

static void qede_fan_fail_cleanup(void)
{
	struct qede_fan_fail_info *p_fan_fail_info, next, *p_next = &next;

	cancel_delayed_work_sync(&fan_fail_task);

	list_for_each_entry_safe(p_fan_fail_info, p_next, &fan_fail_list,
				 list) {
		list_del(&p_fan_fail_info->list);
		kfree(p_fan_fail_info);
	}
}

static void __exit qede_cleanup(void)
{
	if (debug & QED_LOG_INFO_MASK)
		pr_info("qede_cleanup called\n");

	qede_remove_debugfs();
	qede_fan_fail_cleanup();
#ifdef _HAS_REGISTER_NETDEVICE_NOTIFIER_RH /* ! QED_UPSTREAM */
	unregister_netdevice_notifier_rh(&qede_netdev_notifier);
#else
	unregister_netdevice_notifier(&qede_netdev_notifier);
#endif
	pci_unregister_driver(&qede_pci_driver);
	qed_put_eth_ops();
}

module_init(qede_init);
module_exit(qede_cleanup);

#ifndef QEDE_UPSTREAM /* ! QEDE_UPSTREAM */
/* -------------------------------------------------------------------------
 * Module Params parsers
 * -------------------------------------------------------------------------
 */
static enum qed_int_mode qede_int_mode_to_enum(void)
{
	switch (int_mode) {
	case 0:	return QED_INT_MODE_MSIX;
	case 1:	return QED_INT_MODE_INTA;
	case 2:	return QED_INT_MODE_MSI;
	default:
		printk(KERN_ERR "Unknown qede_int_mode=%08x; Defaulting to MSI-x\n",
		       int_mode);
		return QED_INT_MODE_MSIX;
	}
}
#endif

static int qede_open(struct net_device *ndev);
static int qede_close(struct net_device *ndev);

void qede_fill_by_demand_stats(struct qede_dev *edev)
{
	struct qede_stats_common *p_common = &edev->stats.common;
	struct qed_eth_stats stats;

	edev->ops->common->get_vport_stats(edev->cdev, &stats);

	p_common->no_buff_discards = stats.common.no_buff_discards;
	p_common->packet_too_big_discard = stats.common.packet_too_big_discard;
	p_common->ttl0_discard = stats.common.ttl0_discard;
	p_common->rx_ucast_bytes = stats.common.rx_ucast_bytes;
	p_common->rx_mcast_bytes = stats.common.rx_mcast_bytes;
	p_common->rx_bcast_bytes = stats.common.rx_bcast_bytes;
	p_common->rx_ucast_pkts = stats.common.rx_ucast_pkts;
	p_common->rx_mcast_pkts = stats.common.rx_mcast_pkts;
	p_common->rx_bcast_pkts = stats.common.rx_bcast_pkts;
	p_common->mftag_filter_discards = stats.common.mftag_filter_discards;
	p_common->mac_filter_ucast_discards =
		stats.common.mac_filter_ucast_discards;
	p_common->mac_filter_mcast_discards =
		stats.common.mac_filter_mcast_discards;
	p_common->mac_filter_bcast_discards =
		stats.common.mac_filter_bcast_discards;
	p_common->gft_filter_drop = stats.common.gft_filter_drop;

	p_common->tx_ucast_bytes = stats.common.tx_ucast_bytes;
	p_common->tx_mcast_bytes = stats.common.tx_mcast_bytes;
	p_common->tx_bcast_bytes = stats.common.tx_bcast_bytes;
	p_common->tx_ucast_pkts = stats.common.tx_ucast_pkts;
	p_common->tx_mcast_pkts = stats.common.tx_mcast_pkts;
	p_common->tx_bcast_pkts = stats.common.tx_bcast_pkts;
	p_common->tx_err_drop_pkts = stats.common.tx_err_drop_pkts;
	p_common->coalesced_pkts = stats.common.tpa_coalesced_pkts;
	p_common->coalesced_events = stats.common.tpa_coalesced_events;
	p_common->coalesced_aborts_num = stats.common.tpa_aborts_num;
	p_common->non_coalesced_pkts = stats.common.tpa_not_coalesced_pkts;
	p_common->coalesced_bytes = stats.common.tpa_coalesced_bytes;

	p_common->rx_64_byte_packets = stats.common.rx_64_byte_packets;
	p_common->rx_65_to_127_byte_packets =
		stats.common.rx_65_to_127_byte_packets;
	p_common->rx_128_to_255_byte_packets =
		stats.common.rx_128_to_255_byte_packets;
	p_common->rx_256_to_511_byte_packets =
		stats.common.rx_256_to_511_byte_packets;
	p_common->rx_512_to_1023_byte_packets =
		stats.common.rx_512_to_1023_byte_packets;
	p_common->rx_1024_to_1518_byte_packets =
		stats.common.rx_1024_to_1518_byte_packets;
	p_common->rx_crc_errors = stats.common.rx_crc_errors;
	p_common->rx_mac_crtl_frames = stats.common.rx_mac_crtl_frames;
	p_common->rx_pause_frames = stats.common.rx_pause_frames;
	p_common->rx_pfc_frames = stats.common.rx_pfc_frames;
	p_common->rx_align_errors = stats.common.rx_align_errors;
	p_common->rx_carrier_errors = stats.common.rx_carrier_errors;
	p_common->rx_oversize_packets = stats.common.rx_oversize_packets;
	p_common->rx_jabbers = stats.common.rx_jabbers;
	p_common->rx_undersize_packets = stats.common.rx_undersize_packets;
	p_common->rx_fragments = stats.common.rx_fragments;
	p_common->tx_64_byte_packets = stats.common.tx_64_byte_packets;
	p_common->tx_65_to_127_byte_packets =
		stats.common.tx_65_to_127_byte_packets;
	p_common->tx_128_to_255_byte_packets =
		stats.common.tx_128_to_255_byte_packets;
	p_common->tx_256_to_511_byte_packets =
		stats.common.tx_256_to_511_byte_packets;
	p_common->tx_512_to_1023_byte_packets =
		stats.common.tx_512_to_1023_byte_packets;
	p_common->tx_1024_to_1518_byte_packets =
		stats.common.tx_1024_to_1518_byte_packets;
	p_common->tx_pause_frames = stats.common.tx_pause_frames;
	p_common->tx_pfc_frames = stats.common.tx_pfc_frames;
	p_common->brb_truncates = stats.common.brb_truncates;
	p_common->brb_discards = stats.common.brb_discards;
	p_common->tx_mac_ctrl_frames = stats.common.tx_mac_ctrl_frames;
	p_common->link_change_count = stats.common.link_change_count;
	p_common->ptp_skip_txts = edev->ptp_skip_txts;
	p_common->pfm_state_changes = stats.common.pfm_state_changes;
	p_common->nig_drain_cnt = stats.common.nig_drain_cnt;

	if (QEDE_IS_BB(edev)) {
		struct qede_stats_bb *p_bb = &edev->stats.bb;

		p_bb->rx_1519_to_1522_byte_packets =
			stats.bb.rx_1519_to_1522_byte_packets;
		p_bb->rx_1519_to_2047_byte_packets =
			stats.bb.rx_1519_to_2047_byte_packets;
		p_bb->rx_2048_to_4095_byte_packets =
			stats.bb.rx_2048_to_4095_byte_packets;
		p_bb->rx_4096_to_9216_byte_packets =
			stats.bb.rx_4096_to_9216_byte_packets;
		p_bb->rx_9217_to_16383_byte_packets =
			stats.bb.rx_9217_to_16383_byte_packets;
		p_bb->tx_1519_to_2047_byte_packets =
			stats.bb.tx_1519_to_2047_byte_packets;
		p_bb->tx_2048_to_4095_byte_packets =
			stats.bb.tx_2048_to_4095_byte_packets;
		p_bb->tx_4096_to_9216_byte_packets =
			stats.bb.tx_4096_to_9216_byte_packets;
		p_bb->tx_9217_to_16383_byte_packets =
			stats.bb.tx_9217_to_16383_byte_packets;
		p_bb->tx_lpi_entry_count = stats.bb.tx_lpi_entry_count;
		p_bb->tx_total_collisions = stats.bb.tx_total_collisions;
	} else {
		struct qede_stats_ah *p_ah = &edev->stats.ah;

		p_ah->rx_1519_to_max_byte_packets =
			stats.ah.rx_1519_to_max_byte_packets;
		p_ah->tx_1519_to_max_byte_packets =
			stats.ah.tx_1519_to_max_byte_packets;
	}
}

#if defined(_HAS_RTNL_LINK_STATS64) /* QEDE_UPSTREAM */
#if defined(_HAS_RTNL_LINK_STATS64_VOID) /* QEDE_UPSTREAM */
static void qede_get_stats64(struct net_device *dev,
			     struct rtnl_link_stats64 *stats)
#else
static
struct rtnl_link_stats64 *qede_get_stats64(struct net_device *dev,
					   struct rtnl_link_stats64 *stats)
#endif
{
#else
static struct net_device_stats *qede_get_stats(struct net_device *dev)
{
	struct net_device_stats *stats = &dev->stats;
#endif
	struct qede_dev *edev = netdev_priv(dev);
	struct qede_stats_common *p_common;

	if (edev->aer_recov_prog) {
		DP_NOTICE(edev, "AER in progress\n");
#if !defined(_HAS_RTNL_LINK_STATS64_VOID) /* ! QEDE_UPSTREAM */
		return stats;
#else
		return;
#endif
	}

	qede_fill_by_demand_stats(edev);
	p_common = &edev->stats.common;

	stats->rx_packets = p_common->rx_ucast_pkts + p_common->rx_mcast_pkts +
			    p_common->rx_bcast_pkts;
	stats->tx_packets = p_common->tx_ucast_pkts + p_common->tx_mcast_pkts +
			    p_common->tx_bcast_pkts;

	stats->rx_bytes = p_common->rx_ucast_bytes + p_common->rx_mcast_bytes +
			  p_common->rx_bcast_bytes;
	stats->tx_bytes = p_common->tx_ucast_bytes + p_common->tx_mcast_bytes +
			  p_common->tx_bcast_bytes;

	stats->tx_errors = p_common->tx_err_drop_pkts;
	stats->multicast = p_common->rx_mcast_pkts + p_common->rx_bcast_pkts;

	stats->rx_fifo_errors = p_common->no_buff_discards;

	if (QEDE_IS_BB(edev))
		stats->collisions = edev->stats.bb.tx_total_collisions;
	stats->rx_crc_errors = p_common->rx_crc_errors;
	stats->rx_frame_errors = p_common->rx_align_errors;

	/*
	stats->rx_errors;
	stats->rx_dropped;
	stats->tx_dropped;
	stats->rx_length_errors;
	stats->rx_over_errors;
	stats->rx_missed_errors;
	stats->tx_aborted_errors;
	stats->tx_carrier_errors;
	stats->tx_fifo_errors;
	stats->tx_heartbeat_errors;
	stats->tx_window_errors;
	stats->rx_compressed;
	stats->tx_compressed;
	*/

#if !defined(_HAS_RTNL_LINK_STATS64_VOID) /* ! QEDE_UPSTREAM */
	return stats;
#endif
}

#if defined(_HAS_PHYS_PORT_ID) /* ! QEDE_UPSTREAM */
static int qede_get_phys_port_id(struct net_device *netdev,
				 struct netdev_phys_port_id *ppid)
{
	struct qede_dev *edev = netdev_priv(netdev);

	ppid->id_len = ETH_ALEN;
	memcpy(ppid->id, edev->dev_info.port_mac, ETH_ALEN);

	return 0;
}
#endif

#ifdef SYS_INC_SRIOV
#ifdef _HAS_NDO_GET_VF_CONFIG /* QEDE_UPSTREAM */
int qede_get_vf_config(struct net_device *dev, int vfidx,
			      struct ifla_vf_info *ivi)
{
	struct qede_dev *edev = netdev_priv(dev);

	if (!edev->ops)
		return -EINVAL;

	return edev->ops->iov->get_config(edev->cdev, vfidx, ivi);
}
#endif

#ifdef _HAS_IFLA_VF_RATE /* QEDE_UPSTREAM */
static int qede_set_vf_rate(struct net_device *dev, int vfidx,
			    int min_tx_rate, int max_tx_rate)
#else
static int qede_set_vf_tx_rate(struct net_device *dev, int vfidx,
			       int max_tx_rate)
#endif
{
	struct qede_dev *edev = netdev_priv(dev);

#ifdef _HAS_IFLA_VF_RATE /* QEDE_UPSTREAM */
	return edev->ops->iov->set_rate(edev->cdev, vfidx, min_tx_rate,
					max_tx_rate);
#else
	return edev->ops->iov->set_rate(edev->cdev, vfidx, 0, max_tx_rate);
#endif
}

#ifdef _HAS_NDO_SET_VF_SPOOFCHK /* QEDE_UPSTREAM */
static int qede_set_vf_spoofchk(struct net_device *dev, int vfidx,
				bool val)
{
	struct qede_dev *edev = netdev_priv(dev);

	if (!edev->ops)
		return -EINVAL;

	return edev->ops->iov->set_spoof(edev->cdev, vfidx, val);
}
#endif

#ifdef _HAS_NDO_SET_VF_LINK_STATE /* QEDE_UPSTREAM */
static int qede_set_vf_link_state(struct net_device *dev, int vfidx,
				  int link_state)
{
	struct qede_dev *edev = netdev_priv(dev);

	if (!edev->ops)
		return -EINVAL;

	return edev->ops->iov->set_link_state(edev->cdev, vfidx, link_state);
}
#endif

#if HAS_NDO(SET_VF_TRUST) /* QEDE_UPSTREAM */
static int qede_set_vf_trust(struct net_device *dev, int vfidx,
			     bool setting)
{
	struct qede_dev *edev = netdev_priv(dev);

	if (!edev->ops)
		return -EINVAL;

	return edev->ops->iov->set_trust(edev->cdev, vfidx, setting);
}
#endif
#endif

static int qede_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct qede_dev *edev = netdev_priv(dev);

	if (!netif_running(dev))
		return -EAGAIN;

	switch (cmd) {
#ifdef SIOCSHWTSTAMP /* QEDE_UPSTREAM */
	case SIOCSHWTSTAMP:
		return qede_ptp_hw_ts(edev, ifr);
#endif
	default:
		DP_INFO(edev, "default IOCTL cmd 0x%x\n", cmd);
		return -EOPNOTSUPP;
	}
}

#ifdef _HAS_NDO_TXTO_QUEUE /* QEDE_UPSTREAM */
static void qede_tx_timeout(struct net_device *dev, unsigned int txq_id)
#else
static void qede_tx_timeout(struct net_device *dev)
#endif
{
	struct qede_dev *edev = netdev_priv(dev);
	struct qed_chain *p_chain_to_dump = NULL;
	struct qede_fastpath *fp;
	u32 cur_idx = 0;
	int i;

	netif_carrier_off(dev);

	DP_NOTICE(edev, "Tx timeout!\n");
	if (edev->aer_recov_prog) {
		DP_NOTICE(edev, "AER in progress\n");
		return;
	}

	for_each_queue(i) {
		struct qede_tx_queue *txq;
		int cos;

		fp = &edev->fp_array[i];
		if (!(fp->type & QEDE_FASTPATH_TX))
			continue;

		for_each_cos_in_txq(edev, cos) {
			txq = &fp->txq[cos];

#ifdef _HAS_NDO_TXTO_QUEUE /* QEDE_UPSTREAM */
			if (txq_id == txq->ndev_txq_id)
				p_chain_to_dump = &txq->tx_pbl;
#endif
			/* Dump basic metadata for all queues */
			qede_txq_fp_log_metadata(edev, fp, txq);

			if (qed_chain_get_cons_idx(&txq->tx_pbl) !=
			    qed_chain_get_prod_idx(&txq->tx_pbl)) {
				qede_tx_log_print(edev, fp, txq);
			}
		}
	}

	/* dump all bds for the indicated txq by stack */
	if (p_chain_to_dump) {
		edev->ops->common->chain_print(p_chain_to_dump, NULL, 0, &cur_idx,
					       p_chain_to_dump->capacity, false, true,
					       NULL, NULL);
	}

	if (IS_VF(edev))
		return;

	/* TODO - are we going to protect the state access in this scenario? */
	if (test_and_set_bit(QEDE_ERR_IS_HANDLED, &edev->err_flags) ||
	    edev->state == QEDE_STATE_RECOVERY || edev->aer_recov_prog) {
		DP_INFO(edev,
			"Avoid handling a Tx timeout while another HW error is being handled\n");
		return;
	}

	set_bit(QEDE_ERR_IS_RECOVERABLE, &edev->err_flags);
	set_bit(QEDE_ERR_GET_DBG_INFO, &edev->err_flags);
	set_bit(QEDE_SP_HW_ERR, &edev->sp_flags);
	schedule_delayed_work(&edev->sp_task, 0);
}

#ifdef _HAS_NDO_SETUP_TC /* QEDE_UPSTREAM */
static int qede_setup_tc(struct net_device *ndev, u8 num_tc)
{
	struct qede_dev *edev = netdev_priv(ndev);
	int cos, count, offset;

	if (num_tc > edev->dev_info.num_tc)
		return -EINVAL;

	netdev_reset_tc(ndev);
	netdev_set_num_tc(ndev, num_tc);

	for_each_cos_in_txq(edev, cos) {
		count = QEDE_BASE_TSS_COUNT(edev);
		offset = cos * QEDE_BASE_TSS_COUNT(edev);
		netdev_set_tc_queue(ndev, cos, count, offset);
	}

	return 0;
}
#endif

#if defined(_HAS_TC_FLOWER) || defined(_HAS_FLOW_BLOCK_OFFLOAD) /* QEDE_UPSTREAM */
#ifdef _HAS_FLOW_BLOCK_OFFLOAD /* QEDE_UPSTREAM */
static int
qede_set_flower(struct qede_dev *edev, struct flow_cls_offload *f,
		__be16 proto)
#else
static int
qede_set_flower(struct qede_dev *edev, struct tc_cls_flower_offload *f,
		__be16 proto)
#endif
{
	switch (f->command) {
#if defined(_HAS_FLOW_BLOCK_OFFLOAD) /* QEDE_UPSTREAM */
	case FLOW_CLS_REPLACE:
#else
	case TC_CLSFLOWER_REPLACE:
#endif
		return qede_add_tc_flower_fltr(edev, proto, f);
#if defined(_HAS_FLOW_BLOCK_OFFLOAD) /* QEDE_UPSTREAM */
	case FLOW_CLS_DESTROY:
#else
	case TC_CLSFLOWER_DESTROY:
#endif
		return qede_delete_flow_filter(edev, f->cookie);
	default:
		return -EOPNOTSUPP;
	}
}
#endif

#ifdef _HAS_TC_SETUP_TYPE /* QEDE_UPSTREAM */
#if defined(_HAS_TC_BLOCK) || defined(_HAS_FLOW_BLOCK_OFFLOAD) /* QEDE_UPSTREAM */
static int qede_setup_tc_block_cb(enum tc_setup_type type, void *type_data,
				  void *cb_priv)
{
#if defined(_HAS_FLOW_BLOCK_OFFLOAD) /* QEDE_UPSTREAM */
	struct flow_cls_offload *f;
#else
	struct tc_cls_flower_offload *f;
#endif
	struct qede_dev *edev = cb_priv;

#if defined(_HAS_FLOW_BLOCK_OFFLOAD) /* QEDE_UPSTREAM */
	if (!tc_cls_can_offload_and_chain0(edev->ndev, type_data))
#else
	if (!tc_can_offload(edev->ndev))
#endif
		return -EOPNOTSUPP;

	switch (type) {
	case TC_SETUP_CLSFLOWER:
		f = type_data;
		return qede_set_flower(edev, f, f->common.protocol);
	default:
		return -EOPNOTSUPP;
	}
}

#if !defined(_HAS_FLOW_BLOCK_OFFLOAD) /* ! QEDE_UPSTREAM */
static int qede_setup_tc_block(struct qede_dev *edev,
			       struct tc_block_offload *f)
{
	if (f->binder_type != TCF_BLOCK_BINDER_TYPE_CLSACT_INGRESS)
		return -EOPNOTSUPP;

	switch (f->command) {
	case TC_BLOCK_BIND:
		return qede_block_cb_register(f, qede_setup_tc_block_cb, edev,
					      edev);
	case TC_BLOCK_UNBIND:
		tcf_block_cb_unregister(f->block, qede_setup_tc_block_cb, edev);
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}
#else
static LIST_HEAD(qede_block_cb_list);
#endif
#endif
static int __qede_setup_tc_type(struct net_device *dev, enum tc_setup_type type,
				void *type_data)
{
	struct qede_dev *edev = netdev_priv(dev);
#if !defined(_HAS_TC_BLOCK) && !defined(_HAS_FLOW_BLOCK_OFFLOAD) /* ! QEDE_UPSTREAM */
	struct tc_cls_flower_offload *f;
#endif
	struct tc_mqprio_qopt *mqprio;

	switch (type) {
#if defined(_HAS_TC_BLOCK) || defined(_HAS_FLOW_BLOCK_OFFLOAD) /* QEDE_UPSTREAM */
	case TC_SETUP_BLOCK:
#if defined(_HAS_FLOW_BLOCK_OFFLOAD) /* QEDE_UPSTREAM */
		return flow_block_cb_setup_simple(type_data,
						  &qede_block_cb_list,
						  qede_setup_tc_block_cb,
						  edev, edev, true);
#else
		return qede_setup_tc_block(edev, type_data);
#endif
#else
	case TC_SETUP_CLSFLOWER:
		f = type_data;

		if (!is_classid_clsact_ingress(f->common.classid) ||
		    f->common.chain_index)
			return -EOPNOTSUPP;

		return qede_set_flower(edev, f, f->common.protocol);
#endif
	case TC_SETUP_QDISC_MQPRIO:
		mqprio = type_data;
		mqprio->hw = TC_MQPRIO_HW_OFFLOAD_TCS;
		return qede_setup_tc(dev, mqprio->num_tc);
	default:
		return -EOPNOTSUPP;
	}
}
#else
#if defined(_HAS_NDO_SETUP_TC_CHAIN) || defined(_HAS_NDO_SETUP_TC_HANDLE)
#ifdef _HAS_NDO_SETUP_TC_CHAIN
static int __qede_setup_tc(struct net_device *dev, u32 handle, u32 chain_index,
			   __be16 proto, struct tc_to_netdev *tc)
#else
static int __qede_setup_tc(struct net_device *dev, u32 handle,
			   __be16 proto, struct tc_to_netdev *tc)
#endif
{
#ifdef _HAS_TC_FLOWER
	struct qede_dev *edev = netdev_priv(dev);

	if (TC_H_MAJ(handle) != TC_H_MAJ(TC_H_INGRESS))
		goto mqprio;

	switch (tc->type) {
	case TC_SETUP_CLSFLOWER:
		return qede_set_flower(edev, tc->cls_flower, proto);
	default:
		DP_NOTICE(edev, "Unsupported classifier=0x%x\n", tc->type);
		return -EOPNOTSUPP;
	}
mqprio:
#endif
	if (tc->type != TC_SETUP_QDISC_MQPRIO)
		return -EINVAL;

#ifdef _HAS_NDO_MQPRIO_OPT
	tc->mqprio->hw = TC_MQPRIO_HW_OFFLOAD_TCS;
	return qede_setup_tc(dev, tc->mqprio->num_tc);
#else
	return qede_setup_tc(dev, tc->tc);
#endif
}
#endif
#endif

#if HAS_NDO(SET_TX_MAXRATE) /* QEDE_UPSTREAM */
static int qede_set_txq_max_rate(struct net_device *ndev, int queue_index,
				 u32 maxrate)
{
	struct qede_dev *edev = netdev_priv(ndev);
	struct qede_tx_queue *txq;

	if (!netif_running(ndev)) {
		DP_INFO(edev, "Interface is not running\n");
		return -EINVAL;
	}

	txq = QEDE_NDEV_TXQ_ID_TO_TXQ(edev, queue_index);

	return edev->ops->q_maxrate(edev->cdev, queue_index,
				    txq->handle, maxrate);
}
#endif

#ifdef _HAS_NDO_GET_VF_STATS /* QEDE_UPSTREAM */
static int qede_ndo_get_vf_stats(struct net_device *dev, int vf,
			  struct ifla_vf_stats *stats)
{
	struct qede_stats_common *p_common;
	struct qede_dev *edev = netdev_priv(dev);

	edev->ops->common->set_vf_stats_bin_id(edev->cdev, vf);
	qede_fill_by_demand_stats(edev);
	p_common = &edev->stats.common;
	memset(stats, 0, sizeof(*stats));
	stats->rx_packets = p_common->rx_ucast_pkts + p_common->rx_mcast_pkts +
		p_common->rx_bcast_pkts;
	stats->tx_packets = p_common->tx_ucast_pkts + p_common->tx_mcast_pkts +
		p_common->tx_bcast_pkts;
	stats->rx_bytes = p_common->rx_ucast_bytes + p_common->rx_mcast_bytes +
		p_common->rx_bcast_bytes;
	stats->tx_bytes = p_common->tx_ucast_bytes + p_common->tx_mcast_bytes +
		p_common->tx_bcast_bytes;
	stats->broadcast = p_common->rx_bcast_pkts;
	stats->multicast = p_common->rx_mcast_pkts;
	return 0;
};
#endif

static const struct net_device_ops qede_netdev_ops = {
#ifdef _HAS_NDO_SIZE_EXT_OPS /* ! QEDE_UPSTREAM */
	.ndo_size = sizeof(struct net_device_ops),
#endif
	INIT_STRUCT_FIELD(ndo_open, qede_open),
	INIT_STRUCT_FIELD(ndo_stop, qede_close),
	INIT_STRUCT_FIELD(ndo_start_xmit, qede_start_xmit),
	INIT_STRUCT_FIELD(ndo_select_queue, qede_select_queue),
	INIT_STRUCT_FIELD(ndo_set_rx_mode, qede_set_rx_mode),
	INIT_STRUCT_FIELD(ndo_set_mac_address, qede_set_mac_addr),
	INIT_STRUCT_FIELD(ndo_validate_addr, eth_validate_addr),
#ifdef CONFIG_NET_POLL_CONTROLLER
	INIT_STRUCT_FIELD(ndo_poll_controller, qede_poll_controller),
#endif
#ifndef _HAS_NDO_EXT_CHANGE_MTU /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_change_mtu, qede_change_mtu),
#endif
	INIT_STRUCT_FIELD(ndo_do_ioctl, qede_ioctl),
	INIT_STRUCT_FIELD(ndo_tx_timeout, qede_tx_timeout),
#ifdef SYS_INC_SRIOV
	INIT_STRUCT_FIELD(ndo_set_vf_mac, qede_set_vf_mac),
#ifndef _HAS_NDO_EXT_VF_VLAN_PROTO /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_set_vf_vlan, qede_set_vf_vlan),
#endif
#ifdef _HAS_NDO_SET_VF_TRUST /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_set_vf_trust, qede_set_vf_trust),
#endif
#ifdef _HAS_NDO_GET_VF_STATS /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_get_vf_stats, qede_ndo_get_vf_stats),
#endif
#endif
#ifdef _HAS_VLAN_RX_ADD_VID /* QEDE_UPSTREAM */
#if !defined(_VLAN_RX_ADD_VID_RETURNS_VOID) && !defined(_VLAN_RX_ADD_VID_NO_PROTO) /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_vlan_rx_add_vid, qede_vlan_rx_add_vid),
	INIT_STRUCT_FIELD(ndo_vlan_rx_kill_vid, qede_vlan_rx_kill_vid),
#elif defined(_VLAN_RX_ADD_VID_RETURNS_VOID)
	INIT_STRUCT_FIELD(ndo_vlan_rx_add_vid, qede_vlan_rx_add_vid_void),
	INIT_STRUCT_FIELD(ndo_vlan_rx_kill_vid, qede_vlan_rx_kill_vid_void),
#else
	INIT_STRUCT_FIELD(ndo_vlan_rx_add_vid, qede_vlan_rx_add_vid_no_proto),
	INIT_STRUCT_FIELD(ndo_vlan_rx_kill_vid, qede_vlan_rx_kill_vid_no_proto),
#endif
#endif /*_HAS_VLAN_RX_ADD_VID*/
#ifdef BCM_VLAN /* ! QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_vlan_rx_register, qede_vlan_rx_register),
#endif
#ifdef CONFIG_NET_POLL_CONTROLLER /* QEDE_UPSTREAM */
/* @@@TBD - INIT_STRUCT_FIELD(ndo_poll_controller, poll_qede), */
#endif

#ifdef _HAS_NDO_SETUP_TC /* QEDE_UPSTREAM */
#ifdef _HAS_TC_SETUP_TYPE /* QEDE_UPSTREAM */
#ifndef _HAS_NDO_EXT_SETUP_TC /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_setup_tc, __qede_setup_tc_type),
#endif
#else
#ifdef _HAS_NDO_SETUP_TC_CHAIN
	INIT_STRUCT_FIELD(ndo_setup_tc, __qede_setup_tc),
#else
#ifdef _HAS_NDO_SETUP_TC_HANDLE
	INIT_STRUCT_FIELD(ndo_setup_tc, __qede_setup_tc),
#else
	INIT_STRUCT_FIELD(ndo_setup_tc, qede_setup_tc),
#endif
#endif
#endif
#endif
#if !(RHEL_STARTING_AT_VERSION(6, 6) && RHEL_PRE_VERSION(7, 0)) /* QEDE_UPSTREAM */
#if defined(HAS_NDO_FIX_FEATURES) /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_fix_features, qede_fix_features),
	INIT_STRUCT_FIELD(ndo_set_features, qede_set_features),
#endif
#if defined(_HAS_RTNL_LINK_STATS64) /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_get_stats64, qede_get_stats64),
#else
	INIT_STRUCT_FIELD(ndo_get_stats, qede_get_stats),
#endif
#if defined(_HAS_PHYS_PORT_ID) /* ! QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_get_phys_port_id, qede_get_phys_port_id),
#endif

#ifdef SYS_INC_SRIOV
#ifdef _HAS_NDO_SET_VF_LINK_STATE /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_set_vf_link_state, qede_set_vf_link_state),
#endif
#ifdef _HAS_NDO_SET_VF_SPOOFCHK /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_set_vf_spoofchk, qede_set_vf_spoofchk),
#endif
#endif
#endif  /* not (RHEL 6.X; X > 5) */
#ifdef SYS_INC_SRIOV
#ifdef _HAS_NDO_GET_VF_CONFIG /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_get_vf_config, qede_get_vf_config),
#endif
#ifdef _HAS_IFLA_VF_RATE /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_set_vf_rate, qede_set_vf_rate),
#else
	INIT_STRUCT_FIELD(ndo_set_vf_tx_rate, qede_set_vf_tx_rate),
#endif
#endif
#ifdef _HAS_NDO_UDP_TUNNEL_CONFIG /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_udp_tunnel_add, qede_udp_tunnel_add),
	INIT_STRUCT_FIELD(ndo_udp_tunnel_del, qede_udp_tunnel_del),
#endif
#ifdef _HAS_ADD_VXLAN_PORT /* ! QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_add_vxlan_port, qede_add_vxlan_port),
	INIT_STRUCT_FIELD(ndo_del_vxlan_port, qede_del_vxlan_port),
#endif
#ifdef _HAS_ADD_GENEVE_PORT /* ! QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_add_geneve_port, qede_add_geneve_port),
	INIT_STRUCT_FIELD(ndo_del_geneve_port, qede_del_geneve_port),
#endif
#ifdef _HAS_NDO_FEATURES_CHECK /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_features_check, qede_features_check),
#endif
#ifndef _HAS_NDO_EXT_XDP /* QEDE_UPSTREAM */
#ifdef _HAS_NDO_XDP /* QEDE_UPSTREAM */
#ifdef _HAS_NDO_BPF /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_bpf, qede_xdp),
#else
	INIT_STRUCT_FIELD(ndo_xdp, qede_xdp),
#endif
#endif
#endif
#ifdef CONFIG_RFS_ACCEL
#ifndef _HAS_NDEV_RFS_INFO /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_rx_flow_steer, qede_rx_flow_steer),
#endif
#endif
#ifdef _HAS_NDO_DFWD_ADD_STATION /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_dfwd_add_station, qede_fwd_add_station),
	INIT_STRUCT_FIELD(ndo_dfwd_del_station, qede_fwd_del_station),
#endif
#if defined(_HAS_NDO_EXT_SET_VF_TRUST) || \
    defined(_HAS_NDO_EXT_VF_VLAN_PROTO) || \
    defined(_HAS_NDO_EXT_DFWD_ADD_STATION) ||	\
    defined(_HAS_NDO_EXT_CHANGE_MTU) ||	\
    defined(_HAS_NDO_EXT_UDP_TUNNEL_CONFIG) ||	\
    defined(_HAS_NDO_EXT_SETUP_TC) ||	\
    defined(_HAS_NDO_EXT_SET_TX_MAXRATE) || \
    defined(_HAS_NDO_EXT_XDP) /* ! QEDE_UPSTREAM */
	.extended = {
#if defined(_HAS_NDO_EXT_SET_VF_TRUST)
		.ndo_set_vf_trust = &qede_set_vf_trust,
#endif
#ifdef SYS_INC_SRIOV
#if defined(_HAS_NDO_EXT_VF_VLAN_PROTO)
		.ndo_set_vf_vlan = &qede_set_vf_vlan,
#endif
#endif
#if defined(_HAS_NDO_EXT_DFWD_ADD_STATION)
		INIT_STRUCT_FIELD(ndo_dfwd_add_station, qede_fwd_add_station),
		INIT_STRUCT_FIELD(ndo_dfwd_del_station, qede_fwd_del_station),
#endif
#if defined(_HAS_NDO_EXT_SET_TX_MAXRATE)
		INIT_STRUCT_FIELD(ndo_set_tx_maxrate, qede_set_txq_max_rate),
#endif
#ifdef _HAS_NDO_EXT_CHANGE_MTU
		INIT_STRUCT_FIELD(ndo_change_mtu, qede_change_mtu),
#endif
#if defined(_HAS_NDO_EXT_UDP_TUNNEL_CONFIG)
		INIT_STRUCT_FIELD(ndo_udp_tunnel_add, qede_udp_tunnel_add),
		INIT_STRUCT_FIELD(ndo_udp_tunnel_del, qede_udp_tunnel_del),
#endif
#ifdef _HAS_NDO_SETUP_TC
#ifdef _HAS_TC_SETUP_TYPE
#ifdef _HAS_NDO_EXT_SETUP_TC
		INIT_STRUCT_FIELD(ndo_setup_tc_rh, __qede_setup_tc_type),
#endif
#endif
#endif
#ifdef _HAS_NDO_EXT_XDP /* ! QEDE_UPSTREAM */
#ifdef _HAS_NDO_XDP
#ifdef _HAS_NDO_BPF
		INIT_STRUCT_FIELD(ndo_bpf, qede_xdp),
#else
		INIT_STRUCT_FIELD(ndo_xdp, qede_xdp),
#endif
#endif
#endif
	},
#endif
#ifdef _HAS_NDO_SET_TX_MAXRATE /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_set_tx_maxrate, qede_set_txq_max_rate),
#endif
};

#ifdef CONFIG_DEBUG_FS
static const struct net_device_ops qede_netdev_ops_tx_timeout = {
	INIT_STRUCT_FIELD(ndo_open, qede_open),
	INIT_STRUCT_FIELD(ndo_stop, qede_close),
	INIT_STRUCT_FIELD(ndo_start_xmit, qede_start_xmit_tx_timeout),
	INIT_STRUCT_FIELD(ndo_tx_timeout, qede_tx_timeout),
};
#endif

#if (RHEL_STARTING_AT_VERSION(6, 6) && RHEL_PRE_VERSION(7, 0)) /* ! QEDE_UPSTREAM */
static const struct net_device_ops_ext qede_netdev_ops_ext = {
	.size = sizeof(struct net_device_ops_ext),
	INIT_STRUCT_FIELD(ndo_fix_features, qede_fix_features),
	INIT_STRUCT_FIELD(ndo_set_features, qede_set_features),
	INIT_STRUCT_FIELD(ndo_get_phys_port_id, qede_get_phys_port_id),
	INIT_STRUCT_FIELD(ndo_get_stats64, qede_get_stats64),
	INIT_STRUCT_FIELD(ndo_set_vf_link_state, qede_set_vf_link_state),
	INIT_STRUCT_FIELD(ndo_set_vf_spoofchk, qede_set_vf_spoofchk),
};
#endif /* RHEL 6.X; X > 5 */

static const struct net_device_ops qede_netdev_vf_ops = {
#ifdef _HAS_NDO_SIZE_EXT_OPS /* ! QEDE_UPSTREAM */
	.ndo_size = sizeof(struct net_device_ops),
#endif
	INIT_STRUCT_FIELD(ndo_open, qede_open),
	INIT_STRUCT_FIELD(ndo_stop, qede_close),
	INIT_STRUCT_FIELD(ndo_start_xmit, qede_start_xmit),
	INIT_STRUCT_FIELD(ndo_select_queue, qede_select_queue),
	INIT_STRUCT_FIELD(ndo_set_rx_mode, qede_set_rx_mode),
	INIT_STRUCT_FIELD(ndo_set_mac_address, qede_set_mac_addr),
	INIT_STRUCT_FIELD(ndo_validate_addr, eth_validate_addr),
#ifndef _HAS_NDO_EXT_CHANGE_MTU /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_change_mtu, qede_change_mtu),
#endif
	INIT_STRUCT_FIELD(ndo_tx_timeout, qede_tx_timeout),
#ifdef _HAS_VLAN_RX_ADD_VID /* QEDE_UPSTREAM */
#if !defined(_VLAN_RX_ADD_VID_RETURNS_VOID) && !defined(_VLAN_RX_ADD_VID_NO_PROTO) /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_vlan_rx_add_vid, qede_vlan_rx_add_vid),
	INIT_STRUCT_FIELD(ndo_vlan_rx_kill_vid, qede_vlan_rx_kill_vid),
#elif defined(_VLAN_RX_ADD_VID_RETURNS_VOID)
	INIT_STRUCT_FIELD(ndo_vlan_rx_add_vid, qede_vlan_rx_add_vid_void),
	INIT_STRUCT_FIELD(ndo_vlan_rx_kill_vid, qede_vlan_rx_kill_vid_void),
#else
	INIT_STRUCT_FIELD(ndo_vlan_rx_add_vid, qede_vlan_rx_add_vid_no_proto),
	INIT_STRUCT_FIELD(ndo_vlan_rx_kill_vid, qede_vlan_rx_kill_vid_no_proto),
#endif
#endif /*_HAS_VLAN_RX_ADD_VID*/
#ifdef BCM_VLAN /* ! QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_vlan_rx_register, qede_vlan_rx_register),
#endif
#if !(RHEL_STARTING_AT_VERSION(6, 6) && RHEL_PRE_VERSION(7, 0)) /* QEDE_UPSTREAM */
#if defined(HAS_NDO_FIX_FEATURES) /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_fix_features, qede_fix_features),
	INIT_STRUCT_FIELD(ndo_set_features, qede_set_features),
#endif
#if defined(_HAS_RTNL_LINK_STATS64) /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_get_stats64, qede_get_stats64),
#else
	INIT_STRUCT_FIELD(ndo_get_stats, qede_get_stats),
#endif
#if defined(_HAS_PHYS_PORT_ID) /* ! QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_get_phys_port_id, qede_get_phys_port_id),
#endif
#endif  /* not (RHEL 6.X; X > 5) */
#ifdef _HAS_NDO_UDP_TUNNEL_CONFIG /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_udp_tunnel_add, qede_udp_tunnel_add),
	INIT_STRUCT_FIELD(ndo_udp_tunnel_del, qede_udp_tunnel_del),
#endif
#ifdef _HAS_ADD_VXLAN_PORT /* ! QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_add_vxlan_port, qede_add_vxlan_port),
	INIT_STRUCT_FIELD(ndo_del_vxlan_port, qede_del_vxlan_port),
#endif
#ifdef _HAS_ADD_GENEVE_PORT /* ! QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_add_geneve_port, qede_add_geneve_port),
	INIT_STRUCT_FIELD(ndo_del_geneve_port, qede_del_geneve_port),
#endif
#ifdef _HAS_NDO_FEATURES_CHECK /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ndo_features_check, qede_features_check),
#endif
#ifndef _HAS_NDO_EXT_XDP /* QEDE_UPSTREAM */
#ifdef _HAS_NDO_XDP /* QEDE_UPSTREAM */
#ifdef _HAS_NDO_BPF /* QEDE_UPSTREAM */
	/* TODO - how to prevent regression */
	INIT_STRUCT_FIELD(ndo_bpf, qede_xdp),
#else
	INIT_STRUCT_FIELD(ndo_xdp, qede_xdp),
#endif
#endif
#endif
#if defined(_HAS_NDO_EXT_UDP_TUNNEL_CONFIG) || \
    defined(_HAS_NDO_EXT_CHANGE_MTU) || \
    defined(_HAS_NDO_EXT_XDP) /* ! QEDE_UPSTREAM */
	.extended = {
#if defined(_HAS_NDO_EXT_UDP_TUNNEL_CONFIG)
		INIT_STRUCT_FIELD(ndo_udp_tunnel_add, qede_udp_tunnel_add),
		INIT_STRUCT_FIELD(ndo_udp_tunnel_del, qede_udp_tunnel_del),
#endif
#if defined(_HAS_NDO_EXT_CHANGE_MTU)
		INIT_STRUCT_FIELD(ndo_change_mtu, qede_change_mtu),
#endif
#ifdef _HAS_NDO_EXT_XDP /* ! QEDE_UPSTREAM */
#ifdef _HAS_NDO_XDP
#ifdef _HAS_NDO_BPF
		INIT_STRUCT_FIELD(ndo_bpf, qede_xdp),
#else
		INIT_STRUCT_FIELD(ndo_xdp, qede_xdp),
#endif
#endif
#endif
	},
#endif
};

/* -------------------------------------------------------------------------
 * START OF PROBE / REMOVE
 * -------------------------------------------------------------------------
 */

static struct qede_dev *qede_alloc_etherdev(struct qed_dev *cdev,
					    struct pci_dev *pdev,
					    struct qed_dev_eth_info *info,
					    u32 dp_module,
					    u8 dp_level)
{
	struct net_device *ndev;
	struct qede_dev *edev;

#ifndef _HAS_NDO_SETUP_TC /* ! QEDE_UPSTREAM */
	info->num_tc = 1;
#endif
	ndev = alloc_etherdev_mqs(sizeof(*edev),
				  info->num_queues * info->num_tc,
				  info->num_queues);
	if (!ndev) {
		pr_err("etherdev allocation failed\n");
		return NULL;
	}

	edev = netdev_priv(ndev);
	edev->ndev = ndev;
	edev->cdev = cdev;
	edev->pdev = pdev;
	edev->dp_module = dp_module;
	edev->dp_level = dp_level;
#ifndef QEDE_UPSTREAM
	edev->err_flags_override = err_flags_override;
#endif
	edev->ops = qed_ops;

	if (is_kdump_kernel()) {
		edev->q_num_rx_buffers = NUM_RX_BDS_KDUMP_MIN;
		edev->q_num_tx_buffers = NUM_TX_BDS_KDUMP_MIN;
	} else {
		edev->q_num_rx_buffers = NUM_RX_BDS_DEF;
		edev->q_num_tx_buffers = NUM_TX_BDS_DEF;
	}

	DP_INFO(edev, "Allocated netdev with %d tx queues and %d rx queues\n",
		info->num_queues, info->num_queues);

	SET_NETDEV_DEV(ndev, &pdev->dev);

	memset(&edev->stats, 0, sizeof(edev->stats));
	memcpy(&edev->dev_info, info, sizeof(*info));

	/* As ethtool doesn't have the ability to show WoL behavior as
	 * 'default', if device supports it declare it's enabled.
	 */
	if (edev->dev_info.common.wol_support)
		edev->wol_enabled = true;

	INIT_LIST_HEAD(&edev->vlan_list);
	INIT_LIST_HEAD(&edev->fwd_dev_list);
	edev->num_fwd_devs = 0;

	return edev;
}

static void qede_init_ndev(struct qede_dev *edev)
{
	struct net_device *ndev = edev->ndev;
	struct pci_dev *pdev = edev->pdev;
#ifdef ENC_SUPPORTED
	bool udp_tunnel_enable = false;
#endif
	bool set_ntuple = false;
	u64 hw_features = 0;

#ifdef CONFIG_RFS_ACCEL
	if (edev->dev_info.common.b_arfs_capable)
		set_ntuple = true;
#ifdef _HAS_NDEV_RFS_INFO /* ! QEDE_UPSTREAM */
	if (set_ntuple) {
		struct net_device_extended *dev_ext;

		dev_ext = netdev_extended(edev->ndev);
		dev_ext->rfs_data.ndo_rx_flow_steer = qede_rx_flow_steer;
	}
#endif
#endif
	pci_set_drvdata(pdev, ndev);

	ndev->mem_start = edev->dev_info.common.pci_mem_start;
	ndev->base_addr = ndev->mem_start;
	ndev->mem_end = edev->dev_info.common.pci_mem_end;
	ndev->irq = edev->dev_info.common.pci_irq;

	ndev->watchdog_timeo = (watchdog_timeo * HZ);

	if (IS_VF(edev))
		ndev->netdev_ops = &qede_netdev_vf_ops;
	else
		ndev->netdev_ops = &qede_netdev_ops;

#if (RHEL_STARTING_AT_VERSION(6, 6) && RHEL_PRE_VERSION(7, 0)) /* ! QEDE_UPSTREAM */
	set_netdev_ops_ext(ndev, &qede_netdev_ops_ext);
#endif /* RH6.X; X > 5 */
	qede_set_ethtool_ops(ndev);

#ifdef HAS_NDO_FIX_FEATURES /* QEDE_UPSTREAM */
	ndev->priv_flags |= IFF_UNICAST_FLT;

	/* user-changeble features */
	hw_features = NETIF_F_GRO | NETIF_F_SG |
		      NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM |
		      NETIF_F_TSO | NETIF_F_TSO6;

#ifdef _HAS_NET_GRO_HW /* QEDE_UPSTREAM */
	hw_features |= NETIF_F_GRO_HW;
#ifndef QEDE_UPSTREAM
	if (gro_disable)
		hw_features &= ~NETIF_F_GRO_HW;
#endif
#endif

	/* Encap features*/
#ifdef ENC_SUPPORTED
	if (edev->dev_info.common.vxlan_enable ||
	    edev->dev_info.common.geneve_enable)
		udp_tunnel_enable = true;

	if (udp_tunnel_enable || edev->dev_info.common.gre_enable) {
		hw_features |= NETIF_F_TSO_ECN;
		ndev->hw_enc_features = NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM |
					NETIF_F_SG | NETIF_F_TSO |
					NETIF_F_TSO_ECN | NETIF_F_TSO6 |
					NETIF_F_RXCSUM;
	}

	if (udp_tunnel_enable) {
		hw_features |= NETIF_F_GSO_UDP_TUNNEL;
		ndev->hw_enc_features |= NETIF_F_GSO_UDP_TUNNEL;
#ifdef _HAS_GSO_TUN_L4_CSUM /* QEDE_UPSTREAM */
		hw_features |= NETIF_F_GSO_UDP_TUNNEL_CSUM;
		ndev->hw_enc_features |= NETIF_F_GSO_UDP_TUNNEL_CSUM;
#endif
	}

	if (edev->dev_info.common.gre_enable) {
		hw_features |= NETIF_F_GSO_GRE;
		ndev->hw_enc_features |= NETIF_F_GSO_GRE;
#ifdef _HAS_GSO_TUN_L4_CSUM /* QEDE_UPSTREAM */
		hw_features |= NETIF_F_GSO_GRE_CSUM;
		ndev->hw_enc_features |= NETIF_F_GSO_GRE_CSUM;
#endif
	}
#endif
	ndev->vlan_features = hw_features | NETIF_F_RXHASH | NETIF_F_RXCSUM | NETIF_F_HIGHDMA;

	if (set_ntuple)
		hw_features |= NETIF_F_NTUPLE;

	ndev->features = hw_features | NETIF_F_RXHASH | NETIF_F_RXCSUM |
			 NETIF_F_HW_VLAN_CTAG_RX | NETIF_F_HIGHDMA |
			 NETIF_F_HW_VLAN_CTAG_FILTER | NETIF_F_HW_VLAN_CTAG_TX;

#if !(RHEL_STARTING_AT_VERSION(6, 6) && RHEL_PRE_VERSION(7, 0)) /* QEDE_UPSTREAM */
	ndev->hw_features = hw_features;

#if HAS_NDO(DFWD_ADD_STATION) /* QEDE_UPSTREAM */
	/* Don't enable L2 forwarding offload in case of CMT,
	 * since it's a special case
	 */
	if (!QEDE_IS_CMT(edev))
		ndev->hw_features |= NETIF_F_HW_L2FW_DOFFLOAD;
#endif

#ifdef _HAS_MAX_MTU /* QEDE_UPSTREAM */
	/* MTU range: 46 - 9600 */
	ndev->min_mtu = ETH_ZLEN - ETH_HLEN;
	ndev->max_mtu = QEDE_MAX_JUMBO_PACKET_SIZE;
#else
#ifdef _HAS_EXT_MAX_MTU
	ndev->extended->min_mtu = ETH_ZLEN - ETH_HLEN;
	ndev->extended->max_mtu = QEDE_MAX_JUMBO_PACKET_SIZE;
#endif
#endif

#else
	set_netdev_hw_features(ndev, hw_features);
#endif /* RH6.X; X > 5 */
#else
	hw_features = NETIF_F_SG | NETIF_F_HIGHDMA;
	hw_features |= NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM;
	hw_features |= NETIF_F_TSO | NETIF_F_TSO_ECN;
	hw_features |= NETIF_F_TSO6;
	hw_features |= NETIF_F_RXHASH;
	hw_features |= NETIF_F_GRO;
	ndev->features = hw_features;
#if defined(BCM_VLAN) || !defined(OLD_VLAN)
	ndev->features |= (NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX |
			   NETIF_F_HW_VLAN_FILTER);
	ndev->vlan_features |= ndev->features;
#endif
	if (set_ntuple)
		ndev->features |= NETIF_F_NTUPLE;
#endif
#if defined(_HAS_TC_FLOWER) || defined(_HAS_FLOW_BLOCK_OFFLOAD) /* QEDE_UPSTREAM */
	ndev->features |= NETIF_F_HW_TC;
	ndev->hw_features |= NETIF_F_HW_TC;
#endif
	/* Set network device HW mac */
	eth_hw_addr_set(edev->ndev, edev->dev_info.common.hw_mac);
	LEGACY_QEDE_SET_PERM_ADDR(edev);

	if (IS_VF(edev)) {
		char mac[ETH_ALEN];

		memset(mac, 0, ETH_ALEN);
		edev->ops->init_admin_mac(edev->cdev, mac);
		eth_hw_addr_set(edev->ndev, mac);
	}

	ndev->mtu = edev->dev_info.common.mtu;
}

/* This function converts from 32b param to two params of level and module
 * Input 32b decoding:
 * b31 - enable all NOTICE prints. NOTICE prints are for deviation from the
 * 'happy' flow, e.g. memory allocation failed.
 * b30 - enable all INFO prints. INFO prints are for major steps in the flow
 * and provide important parameters.
 * b29-b0 - per-module bitmap, where each bit enables VERBOSE prints of that
 * module. VERBOSE prints are for tracking the specific flow in low level.
 *
 * Notice that the level should be that of the lowest required logs.
 */
void qede_config_debug(uint debug, u32 *p_dp_module, u8 *p_dp_level)
{
	*p_dp_level = QED_LEVEL_NOTICE;
	*p_dp_module = 0;

	if (debug & QED_LOG_VERBOSE_MASK) {
		*p_dp_level = QED_LEVEL_VERBOSE;
		*p_dp_module = (debug & 0x3FFFFFFF);
	} else if (debug & QED_LOG_INFO_MASK) {
		*p_dp_level = QED_LEVEL_INFO;
	} else if (debug & QED_LOG_NOTICE_MASK) {
		*p_dp_level = QED_LEVEL_NOTICE;
	}
}

static void qede_free_fp_array(struct qede_dev *edev)
{
	if (edev->fp_array) {
		struct qede_fastpath *fp;
		int i;

		for_each_queue(i) {
			fp = &edev->fp_array[i];

			kfree(fp->sb_info);
			kfree(fp->rxq);
			kfree(fp->xdp_tx);
			kfree(fp->txq);
		}

		kfree(edev->fp_array);
		edev->fp_array = NULL;
	}

	edev->num_queues = 0;
	edev->base_num_queues = 0;
	edev->fwd_dev_queues = 0;
	edev->fp_num_tx = 0;
	edev->fp_num_rx = 0;
}

static int qede_alloc_fp_array(struct qede_dev *edev)
{
	u8 fp_tx, fp_combined, fp_rx = edev->fp_num_rx;
	struct qede_fwd_dev *fwd_dev = NULL;
	struct qede_fastpath *fp;
	int i;

	if (!list_empty(&edev->fwd_dev_list))
		fwd_dev = list_first_entry(&edev->fwd_dev_list,
					   struct qede_fwd_dev, list);
	else
		DP_VERBOSE(edev, QED_MSG_DEBUG, "forwarding device list empty\n");

	edev->fp_array = kcalloc(QEDE_QUEUE_CNT(edev),
				 sizeof(*edev->fp_array), GFP_KERNEL);
	if (!edev->fp_array) {
		DP_NOTICE(edev, "fp array allocation failed\n");
		goto err;
	}

	fp_tx = edev->fp_num_tx;
	fp_combined = QEDE_BASE_QUEUE_CNT(edev) - fp_rx - fp_tx;

	/* Allocate the FP elements for Rx queues followed by combined then
	 * the Tx and then queues for fwd offload.
	 * This ordering should be maintained so that the respective
	 * queues (Rx or Tx) will be together in the fastpath array and the
	 * associated ids will be sequential.
	 */
	for_each_queue(i) {
		fp = &edev->fp_array[i];

		fp->sb_info = kzalloc(sizeof(*fp->sb_info), GFP_KERNEL);
		if (!fp->sb_info) {
			DP_NOTICE(edev, "sb info struct allocation failed\n");
			goto err;
		}

		fp->ndev = edev->ndev;
		fp->vport_id = QEDE_BASE_DEV_VPORT_ID;
		if (fp_rx) {
			fp->type = QEDE_FASTPATH_RX;
			fp_rx--;
		} else if (fp_combined) {
			fp->type = QEDE_FASTPATH_COMBINED;
			fp_combined--;
		} else if (fp_tx) {
			fp->type = QEDE_FASTPATH_TX;
			fp_tx--;
		} else {
			/* remaining queues are for L2 forwarding offload */
			fp->fwd_dev = fwd_dev;
			fp->type = QEDE_FASTPATH_COMBINED;
			fp->fwd_fp = true;
			fp->ndev = fwd_dev->upper_ndev;
			fp->vport_id = fwd_dev->vport_id;
			/* check if we need to move to next forwarding dev */
			if (i >= (fwd_dev->base_queue_id +
				  fwd_dev->num_queues - 1))
				fwd_dev = list_next_entry(fwd_dev, list);
		}

		if (fp->type & QEDE_FASTPATH_TX) {
			if (numa_native) {
				fp->txq = kzalloc_node(edev->dev_info.num_tc *
						       sizeof(*fp->txq), GFP_KERNEL,
						       dev_to_node(&edev->pdev->dev));
				if (!fp->txq)
					fp->txq = kcalloc(edev->dev_info.num_tc,
							  sizeof(*fp->txq), GFP_KERNEL);

			} else {
				fp->txq = kcalloc(edev->dev_info.num_tc,
						  sizeof(*fp->txq), GFP_KERNEL);
			}
			if (!fp->txq)
				goto err;
		}

		if (fp->type & QEDE_FASTPATH_RX) {
			if (numa_native) {
				fp->rxq = kzalloc_node(sizeof(*fp->rxq), GFP_KERNEL,
						       dev_to_node(&edev->pdev->dev));
				if (!fp->rxq)
					fp->rxq = kzalloc(sizeof(*fp->rxq), GFP_KERNEL);
			} else {
				fp->rxq = kzalloc(sizeof(*fp->rxq), GFP_KERNEL);
			}

			if (!fp->rxq)
				goto err;

			if (edev->xdp_prog) {
				fp->xdp_tx = kzalloc(sizeof(*fp->xdp_tx),
						     GFP_KERNEL);
				if (!fp->xdp_tx)
					goto err;
				fp->type |= QEDE_FASTPATH_XDP;
			}
		}
	}

	return 0;

err:
	qede_free_fp_array(edev);
	return -ENOMEM;
}

/* The qede lock is used to protect driver state change and driver flows that
 * are not reentrant.
 */
void __qede_lock(struct qede_dev *edev)
{
	mutex_lock(&edev->qede_lock);
}

void __qede_unlock(struct qede_dev *edev)
{
	mutex_unlock(&edev->qede_lock);
}

/* This version of the lock should be used when acquiring the RTNL lock is also
 * needed in addition to the internal qede lock.
 */
void qede_lock(struct qede_dev *edev)
{
	rtnl_lock();
	__qede_lock(edev);
}

void qede_unlock(struct qede_dev *edev)
{
	__qede_unlock(edev);
	rtnl_unlock();
}

static void qede_sp_task(struct work_struct *work)
{
	struct qede_dev *edev = container_of(work, struct qede_dev,
					     sp_task.work);

	/* Disable execution of this deferred work once
	 * qede removal is in progress, this stop any future
	 * scheduling of sp_task.
	 */
	if (test_bit(QEDE_SP_DISABLE, &edev->sp_flags))
		return;

	if (test_and_clear_bit(QEDE_SP_LINK_UP, &edev->sp_flags)) {
		struct qed_link_output link;

		link.link_up = true;
		qede_handle_link_change(edev, &link);
	}

	/* The locking scheme depends on the specific flag:
	 * In case of QEDE_SP_RECOVERY or QEDE_SP_HW_ERR, acquiring the RTNL
	 * lock is required to ensure that ongoing flows are ended and new ones
	 * are not started.
	 * In other cases - only the internal qede lock should be acquired.
	 */

	if (test_and_clear_bit(QEDE_SP_RECOVERY, &edev->sp_flags)) {
#ifdef SYS_INC_SRIOV
		/* SRIOV must be disabled outside the lock to avoid a deadlock.
		 * The recovery of the active VFs is currently not supported.
		 */
		if (edev->num_vfs)
			qede_sriov_configure(edev->pdev, 0);
#endif
		qede_lock(edev);
		qede_recovery_handler(edev);
		qede_unlock(edev);
	}

	if (test_and_clear_bit(QEDE_SP_RX_MODE, &edev->sp_flags)) {
		__qede_lock(edev);
		if (edev->state == QEDE_STATE_OPEN)
			qede_config_rx_mode_for_all(edev);
		__qede_unlock(edev);
	}

#ifdef CONFIG_RFS_ACCEL
	if (test_and_clear_bit(QEDE_SP_ARFS_CONFIG, &edev->sp_flags)) {
		__qede_lock(edev);
		if (edev->state == QEDE_STATE_OPEN)
			qede_process_arfs_filters(edev, false);
		__qede_unlock(edev);
	}
#endif
	if (test_and_clear_bit(QEDE_SP_HW_ERR, &edev->sp_flags)) {
		qede_lock(edev);
		qede_generic_hw_err_handler(edev);
		qede_unlock(edev);
	}

	if (test_and_clear_bit(QEDE_SP_AER, &edev->sp_flags)) {
#ifdef SYS_INC_SRIOV
		/* SRIOV must be disabled outside the lock to avoid a deadlock.
		 * The recovery of the active VFs is currently not supported.
		 */
		if (edev->num_vfs)
			qede_sriov_configure(edev->pdev, 0);
#endif
		edev->ops->common->recovery_process(edev->cdev);
	}
}

static void qede_update_pf_params(struct qed_dev *cdev)
{
	struct qed_pf_params pf_params;
	u16 num_cons;

	/* Need a context for Rx, Tx and XDP Tx queues. Since at this point we
	 * can't tell how many queues will be available, use the max possible
	 * for the device. Qed would update us of actual number via dev_info.
	 */
	memset(&pf_params, 0, sizeof(struct qed_pf_params));
#ifdef _HAS_NDO_SETUP_TC /* QEDE_UPSTREAM */
	num_cons = 2 + QEDE_MAX_TC;
#else
	num_cons = 3;
#endif
	pf_params.eth_pf_params.num_cons = (MAX_SB_PER_PF_MIMD - 1) * num_cons;

	/* Same for VFs - make sure they'll have sufficient connections
	 * to support XDP Tx queues.
	 */
	pf_params.eth_pf_params.num_vf_cons = 48;

	pf_params.eth_pf_params.num_arfs_filters = QEDE_RFS_MAX_FLTR;
	qed_ops->common->update_pf_params(cdev, &pf_params);
}

#define QEDE_FW_VER_STR_SIZE	80

static void qede_log_probe(struct qede_dev *edev)
{
	struct qed_dev_info *p_dev_info = &edev->dev_info.common;
	u8 buf[QEDE_FW_VER_STR_SIZE];
	size_t left_size;

	snprintf(buf, QEDE_FW_VER_STR_SIZE,
		 "Storm FW %d.%d.%d.%d, Management FW %d.%d.%d.%d",
		 p_dev_info->fw_major, p_dev_info->fw_minor, p_dev_info->fw_rev,
		 p_dev_info->fw_eng,
		 GET_MFW_FIELD(p_dev_info->mfw_rev, QED_MFW_VERSION_3),
		 GET_MFW_FIELD(p_dev_info->mfw_rev, QED_MFW_VERSION_2),
		 GET_MFW_FIELD(p_dev_info->mfw_rev, QED_MFW_VERSION_1),
		 GET_MFW_FIELD(p_dev_info->mfw_rev, QED_MFW_VERSION_0));

	left_size = QEDE_FW_VER_STR_SIZE - strlen(buf);
	if (p_dev_info->mbi_version && left_size)
		snprintf(buf + strlen(buf), left_size, " [MBI %d.%d.%d]",
			 GET_MFW_FIELD(p_dev_info->mbi_version,
				       QED_MBI_VERSION_2),
			 GET_MFW_FIELD(p_dev_info->mbi_version,
				       QED_MBI_VERSION_1),
			 GET_MFW_FIELD(p_dev_info->mbi_version,
				       QED_MBI_VERSION_0));

	pr_info("qede %02x:%02x.%x: %s [%s]\n", edev->pdev->bus->number,
		PCI_SLOT(edev->pdev->devfn), PCI_FUNC(edev->pdev->devfn),
		buf, edev->ndev->name);
}

enum qede_probe_mode {
	QEDE_PROBE_NORMAL,
	QEDE_PROBE_RECOVERY,
	QEDE_PROBE_RESET,
};

static int __qede_probe(struct pci_dev *pdev, u32 dp_module, u8 dp_level,
			bool is_vf, enum qede_probe_mode mode)
{
	struct qed_probe_params probe_params;
	struct qed_slowpath_params sp_params;
	struct qed_dev_eth_info dev_info;
	struct qede_dev *edev = NULL;
	struct qed_dev *cdev;
	int rc;

	if (unlikely(dp_level & QED_LEVEL_INFO))
		pr_notice("Starting qede probe\n");

	memset(&probe_params, 0, sizeof(probe_params));
	probe_params.protocol = QED_PROTOCOL_ETH;
	probe_params.dp_module = dp_module;
	probe_params.dp_level = dp_level;
	probe_params.is_vf = is_vf;
	probe_params.recov_in_prog = (mode == QEDE_PROBE_RECOVERY);
	if (mode != QEDE_PROBE_NORMAL) {
		struct net_device *ndev = pci_get_drvdata(pdev);

		if (!ndev) {
			rc = -ENODEV;
			goto err0;
		}
		edev = netdev_priv(ndev);
		probe_params.cdev = edev->cdev;
	}

	cdev = qed_ops->common->probe(pdev, &probe_params);
	if (!cdev) {
		rc = -ENODEV;
		goto err0;
	}

	qede_update_pf_params(cdev);

	/* Start the Slowpath-process */
	memset(&sp_params, 0, sizeof(sp_params));
#ifdef QEDE_UPSTREAM /* QEDE_UPSTREAM */
	sp_params.int_mode = QED_INT_MODE_MSIX;
#else
	sp_params.int_mode = qede_int_mode_to_enum();
#endif
	sp_params.drv_major = QEDE_MAJOR_VERSION;
	sp_params.drv_minor = QEDE_MINOR_VERSION;
	sp_params.drv_rev = QEDE_REVISION_VERSION;
	sp_params.drv_eng = QEDE_ENGINEERING_VERSION;
	strlcpy(sp_params.name, "qede LAN", QED_DRV_VER_STR_SIZE);
	rc = qed_ops->common->slowpath_start(cdev, &sp_params);
	if (rc) {
		pr_notice("qed %02x:%02x.%x: Cannot start slowpath\n",
			  pdev->bus->number, PCI_SLOT(pdev->devfn),
			  PCI_FUNC(pdev->devfn));

		goto err1;
	}

	/* Learn information crucial for qede to progress */
	rc = qed_ops->fill_dev_info(cdev, &dev_info);
	if (rc)
		goto err2;

	if (mode == QEDE_PROBE_NORMAL) {
		edev = qede_alloc_etherdev(cdev, pdev, &dev_info, dp_module,
					   dp_level);
		if (!edev) {
			rc = -ENOMEM;
			goto err2;
		}

	} else {
		memset(&edev->stats, 0, sizeof(edev->stats));
		memcpy(&edev->dev_info, &dev_info, sizeof(dev_info));
	}

	if (edev->devlink) {
		struct qed_devlink *qdl = devlink_priv(edev->devlink);

		if (qdl) {
			qdl->cdev = cdev;
			qdl->drv_ctx = edev;
		}
	} else {
		edev->devlink = qed_ops->common->devlink_register(cdev, edev);
		if (IS_ERR(edev->devlink)) {
			DP_NOTICE(edev, "Cannot register devlink\n");
			edev->devlink = NULL;
			/* Go on, we can live without devlink */
		}
	}

	if (is_vf)
		set_bit(QEDE_FLAGS_IS_VF, &edev->flags);

	qede_init_ndev(edev);
	qed_config_debug(int_debug, &edev->dp_int_module, &edev->dp_int_level);
	qed_ops->common->update_int_msglvl(cdev, edev->dp_int_module,
					   edev->dp_int_level);

	if (mode == QEDE_PROBE_NORMAL) {
		/* Prepare the lock prior to the registration of the netdev,
		 * as once it's registered we might reach flows requiring it
		 * [it's even possible to reach a flow needing it directly
		 * from there, although it's unlikely].
		 */
		INIT_DELAYED_WORK(&edev->sp_task, qede_sp_task);
		mutex_init(&edev->qede_lock);

		rc = register_netdev(edev->ndev);
		if (rc) {
			DP_NOTICE(edev, "Cannot register net-device\n");
			goto err3;
		}
	}

	edev->ops->common->set_name(cdev, edev->ndev->name);

	/* PTP not supported on VF's */
	if (!is_vf)
		qede_ptp_enable(edev);

	edev->ops->register_ops(cdev, &qede_ll_ops, edev);

	rc = qede_rdma_dev_add(edev, mode != QEDE_PROBE_NORMAL);
	if (rc)
		goto err4;

#ifdef CONFIG_DCB
	if (!IS_VF(edev))
		qede_set_dcbnl_ops(edev->ndev);
#endif

	qede_debugfs_add_features(edev);
	edev->rx_copybreak = QEDE_RX_HDR_SIZE;

	qede_log_probe(edev);

	if (qede_alloc_arfs(edev)) {
#ifdef CONFIG_RFS_ACCEL
		edev->ndev->features &= ~NETIF_F_NTUPLE;
#endif
		edev->dev_info.common.b_arfs_capable = false;
	}

	edev->ops->common->set_dev_reuse(cdev, false);

	return 0;

/* Error handling */
err4:
	if (!is_vf)
		qede_ptp_disable(edev);

	unregister_netdev(edev->ndev);
err3:
	if (edev->devlink) {
		qed_ops->common->devlink_unregister(edev->devlink);
		edev->devlink = NULL;
	}

	if (mode != QEDE_PROBE_RECOVERY)
		free_netdev(edev->ndev);
	else
		edev->cdev = NULL;
err2:
	qed_ops->common->slowpath_stop(cdev);
err1:
	qed_ops->common->remove(cdev);
err0:
	return rc;
}

int qede_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
#ifdef _HAS_PCI_RO_CHECK_FLAG
	struct pci_dev *root = NULL;
#endif
	bool is_vf = false;
	u32 dp_module = 0;
	u8 dp_level = 0;

	switch ((enum qede_pci_private)id->driver_data) {
	case QEDE_PRIVATE_VF:
		if (debug & QED_LOG_VERBOSE_MASK)
			dev_err(&pdev->dev, "Probing a VF\n");
		is_vf = true;
		break;
	default:
		if (debug & QED_LOG_VERBOSE_MASK)
			dev_err(&pdev->dev, "Probing a PF\n");
	}

#ifdef _HAS_PCI_RO_CHECK_FLAG
	root = pcie_find_root_port(pdev);
	if (root) {
		if (root->dev_flags & PCI_DEV_FLAGS_NO_RELAXED_ORDERING)
			dev_info(&pdev->dev, "relaxed ordering not supported in root port\n");
		else
			dev_info(&pdev->dev, "relaxed ordering supported in root port\n");
	}
#endif

	qede_config_debug(debug, &dp_module, &dp_level);

	return __qede_probe(pdev, dp_module, dp_level, is_vf,
			    QEDE_PROBE_NORMAL);
}

enum qede_remove_mode {
	QEDE_REMOVE_NORMAL,
	QEDE_REMOVE_RECOVERY,
	QEDE_REMOVE_RESET,
};

static void __qede_remove(struct pci_dev *pdev, enum qede_remove_mode mode)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	struct qede_dev *edev;
	struct qed_dev *cdev;

	if (!ndev) {
		dev_info(&pdev->dev, "Device has already been removed\n");
		return;
	}

	edev = netdev_priv(ndev);
	cdev = edev->cdev;

	if (mode != QEDE_REMOVE_NORMAL)
		edev->ops->common->set_dev_reuse(cdev, true);

	DP_INFO(edev, "Starting qede_remove\n");

	qede_rdma_dev_remove(edev,
			     mode != QEDE_REMOVE_NORMAL || edev->aer_recov_prog);

	if (mode == QEDE_REMOVE_NORMAL) {
		if (!edev->aer_recov_prog) {
			set_bit(QEDE_SP_DISABLE, &edev->sp_flags);
			unregister_netdev(ndev);
		}

		if (edev->dev_info.common.b_arfs_capable) {
			qede_poll_for_freeing_arfs_filters(edev);
			qede_free_arfs(edev);
		}

		cancel_delayed_work_sync(&edev->sp_task);
		edev->ops->common->set_power_state(cdev, PCI_D0);

		if (!edev->aer_recov_prog)
			pci_set_drvdata(pdev, NULL);
	}

	if (!IS_VF(edev))
		qede_ptp_disable(edev);

#ifdef _HAS_NDO_XDP /* QEDE_UPSTREAM */
	/* Release edev's reference to XDP's bpf if such exist */
	if (edev->xdp_prog)
		bpf_prog_put(edev->xdp_prog);
#endif

	/* Use global ops since we've freed edev */
	qed_ops->common->slowpath_stop(cdev);
	if (system_state == SYSTEM_POWER_OFF)
		return;

	if (mode == QEDE_REMOVE_NORMAL && edev->devlink) {
		qed_ops->common->devlink_unregister(edev->devlink);
		edev->devlink = NULL;
	}
	qed_ops->common->remove(cdev);
	if (mode == QEDE_REMOVE_NORMAL)
		edev->cdev = NULL;

	/* Since this can happen out-of-sync with other flows,
	 * don't release the netdevice until after slowpath stop
	 * has been called to guarantee various other contexts
	 * [e.g., QED register callbacks] won't break anything when
	 * accessing the netdevice.
	 */
	if (mode == QEDE_REMOVE_NORMAL && !edev->aer_recov_prog)
		free_netdev(ndev);

	dev_info(&pdev->dev, "Ending qede_remove successfully\n");
}

void qede_remove(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);

	/* Avoid calling this function more than once */
	if (!ndev) {
		pr_notice("The net device is null. Probably due to a prior fan failure handling.\n");
		return;
	}

	__qede_remove(pdev, QEDE_REMOVE_NORMAL);
}

void qede_shutdown(struct pci_dev *pdev)
{
	dev_err(&pdev->dev, "Shutting down\n");
	qede_remove(pdev);
}

/* -------------------------------------------------------------------------
 * END OF PROBE / REMOVE
 * -------------------------------------------------------------------------
 */

/* -------------------------------------------------------------------------
 * START OF LOAD / UNLOAD
 * -------------------------------------------------------------------------
 */

static int qede_set_num_queues(struct qede_dev *edev)
{
	int rc;
	u16 num_queues;

	/* Setup queues according to needed and possible resources*/
	if (edev->req_queues) {
		edev->base_num_queues = edev->req_queues;
	} else {
		u16 base_num_queues;

		base_num_queues =  min_t(u16, QEDE_MAX_QUEUE_CNT(edev),
					 netif_get_num_default_rss_queues() *
					 edev->dev_info.common.num_hwfns);
		edev->base_num_queues = base_num_queues;
	}

	/* re-init forwarding dev database according to new queue count */
	if (!list_empty(&edev->fwd_dev_list))
		qede_reinit_fwd_dev_info(edev);

	/* consider fwd dev queues */
	if (edev->req_fwd_dev_queues)
		edev->fwd_dev_queues = edev->req_fwd_dev_queues;

	num_queues = edev->base_num_queues + edev->fwd_dev_queues;
	num_queues = min_t(u16, QEDE_MAX_QUEUE_CNT(edev), num_queues);

	rc = edev->ops->common->set_fp_int(edev->cdev, num_queues);
	if (rc > 0) {
		/* Managed to request interrupts for our queues */
		edev->num_queues = rc;
		DP_INFO(edev,
			"Managed %d [of %d] fastpath queues for base device including %d L2 forwarding offload queues\n",
			QEDE_QUEUE_CNT(edev), num_queues, edev->fwd_dev_queues);
		rc = 0;
	}

	edev->fp_num_tx = edev->req_num_tx;
	edev->fp_num_rx = edev->req_num_rx;

	return rc;
}

static void qede_free_mem_sb(struct qede_dev *edev, struct qed_sb_info *sb_info,
			     u16 sb_id)
{
	if (!sb_info->sb_virt)
		return;

	edev->ops->common->sb_release(edev->cdev, sb_info, sb_id,
				      QED_SB_TYPE_L2_QUEUE);
	dma_free_coherent(&edev->pdev->dev, sb_info->sb_size, sb_info->sb_virt,
			  sb_info->sb_phys);
	memset(sb_info, 0, sizeof(*sb_info));
}

/* This function allocates fast-path status block memory */
static int qede_alloc_mem_sb(struct qede_dev *edev,
			      struct qed_sb_info *sb_info,
			      u16 sb_id)
{
	dma_addr_t sb_phys;
	void *sb_virt;
	u32 sb_size;
	int rc;

	sb_size = sizeof(struct status_block);
	sb_virt = dma_alloc_coherent(&edev->pdev->dev, sb_size, &sb_phys,
				     GFP_KERNEL);
	if (!sb_virt) {
		DP_ERR(edev, "Status block allocation failed\n");
		return -ENOMEM;
	}

	rc = edev->ops->common->sb_init(edev->cdev, sb_info, sb_virt, sb_phys,
					sb_id, QED_SB_TYPE_L2_QUEUE);
	if (rc) {
		DP_ERR(edev, "Status block initialization failed\n");
		dma_free_coherent(&edev->pdev->dev, sb_size, sb_virt, sb_phys);
		return rc;
	}

	return 0;
}

static void qede_free_rx_buffers(struct qede_dev *edev,
			      struct qede_rx_queue *rxq)
{
	u16 pool_size = rxq->page_pool.size, i;

	for (i = rxq->sw_rx_cons; i != rxq->sw_rx_prod; i++) {
		struct sw_rx_data *rx_buf;
		struct page *data;

		rx_buf = &rxq->sw_rx_ring[i & NUM_RX_BDS_MAX];
		data = rx_buf->data;

		dma_unmap_page(&edev->pdev->dev,
			       rx_buf->mapping,
			       PAGE_SIZE, rxq->data_direction);

		rx_buf->data = NULL;
		__free_page(data);
	}

	for (i = rxq->page_pool.cons; i != rxq->page_pool.prod;
	     i = (i + 1) & (pool_size - 1)) {
		struct qede_pool_dma_info *dma_info;

		dma_info = &rxq->page_pool.page_pool[i];

		dma_unmap_page(&edev->pdev->dev,
			       dma_info->mapping, PAGE_SIZE,
			       rxq->data_direction);
		__free_page(dma_info->page);
	}
}

#ifndef _HAS_BUILD_SKB_V2 /* ! QEDE_UPSTREAM */
static void qede_free_sge_mem(struct qede_dev *edev,
			      struct qede_rx_queue *rxq) {
	int i;

	if (edev->gro_disable)
		return;

	for (i = 0; i < ETH_TPA_MAX_AGGS_NUM; i++) {
		struct qede_agg_info *tpa_info = &rxq->tpa_info[i];
		struct sw_rx_data *replace_buf = &tpa_info->buffer;

		if (replace_buf->data) {
			dma_unmap_page(&edev->pdev->dev,
				       replace_buf->mapping,
				       PAGE_SIZE, DMA_FROM_DEVICE);

			__free_page(replace_buf->data);
		}
	}
}

static int qede_alloc_sge_mem(struct qede_dev *edev,
			      struct qede_rx_queue *rxq)
{
	dma_addr_t mapping;
	int i;

	if (edev->gro_disable)
		return 0;

	/* For each aggregation we allocate a substitute bd buffer, this is so
	 * that on start of each aggregation we'll always have a mapped
	 * allocated bd buf to place on the bd-chain instead of buffer taken
	 */
	for (i = 0; i < ETH_TPA_MAX_AGGS_NUM; i++) {
		struct qede_agg_info *tpa_info = &rxq->tpa_info[i];
		struct sw_rx_data *replace_buf = &tpa_info->buffer;

		replace_buf->data = alloc_pages(GFP_ATOMIC, 0);
		if (unlikely(!replace_buf->data)) {
			DP_NOTICE(edev, "Failed to allocate TPA skb pool [replacement buffer]\n");
			goto err;
		}

		mapping = dma_map_page(&edev->pdev->dev, replace_buf->data, 0,
				       PAGE_SIZE, DMA_FROM_DEVICE);
		if (unlikely(dma_mapping_error(&edev->pdev->dev, mapping))) {
			DP_NOTICE(edev, "Failed to map TPA replacement buffer\n");
			goto err;
		}

		replace_buf->mapping = mapping;
		tpa_info->buffer.page_offset = 0;
		tpa_info->buffer_mapping = mapping + rxq->rx_headroom;
		tpa_info->state = QEDE_AGG_STATE_NONE;
		#ifdef DEBUG_GRO
		tpa_info->num_of_sges = 0;
		tpa_info->total_packet_len = 0;
		#endif
		#ifdef ENC_SUPPORTED
		tpa_info->inner_vlan_exist = 0;
		#endif
	}

	return 0;
err:
	qede_free_sge_mem(edev, rxq);
	edev->gro_disable = 1;
#ifdef _HAS_NET_GRO_HW /* QEDE_UPSTREAM */
	edev->ndev->features &= ~NETIF_F_GRO_HW;
#endif
	return -ENOMEM;
}
#endif

static void qede_free_mem_rxq(struct qede_dev *edev,
			      struct qede_rx_queue *rxq)
{
#ifndef _HAS_BUILD_SKB_V2 /* ! QEDE_UPSTREAM */
	qede_free_sge_mem(edev, rxq);
#endif

	/* Free rx buffers */
	qede_free_rx_buffers(edev, rxq);

	/* Free the parallel SW ring */
	kfree(rxq->sw_rx_ring);

	/* Free the real RQ ring used by FW */

	edev->ops->common->chain_free(edev->cdev, &rxq->rx_bd_ring);
	edev->ops->common->chain_free(edev->cdev, &rxq->rx_comp_ring);

	/* Free page pool */
	vfree(rxq->page_pool.page_pool);

	memset(rxq, 0, sizeof(*rxq));
}

static int qede_alloc_page_pool(struct qede_rx_queue *rxq)
{
	int size = rounddown_pow_of_two(rxq->num_rx_buffers);

	rxq->page_pool.page_pool = vzalloc(size *
					   sizeof(*rxq->page_pool.page_pool));
	if (!rxq->page_pool.page_pool)
		return -ENOMEM;
	rxq->page_pool.size = size;

	rxq->page_pool.cons = 0;
	rxq->page_pool.prod = 0;

	return 0;
}

static void qede_set_tpa_param(struct qede_rx_queue *rxq)
{
	int i;

	for (i = 0; i < ETH_TPA_MAX_AGGS_NUM; i++) {
		struct qede_agg_info *tpa_info = &rxq->tpa_info[i];

		tpa_info->state = QEDE_AGG_STATE_NONE;
#ifdef DEBUG_GRO
		tpa_info->num_of_sges = 0;
		tpa_info->total_packet_len = 0;
#endif
#ifdef ENC_SUPPORTED
		tpa_info->inner_vlan_exist = 0;
#endif
	}
}

/* This function allocates all memory needed per Rx queue */
static int qede_alloc_mem_rxq(struct qede_dev *edev,
			      struct qede_rx_queue *rxq)
{
	struct qed_chain_params chain_params;
	int i, rc, size;

	rxq->num_rx_buffers = edev->q_num_rx_buffers;

	/* TODO - should allocate more bytes in case encountering with an
	 * architecture on which the address of the allocated buffer is not
	 * cache line aligned, so the driver can do manipulations on the
	 * address.
	 */
	rxq->rx_buf_size = NET_IP_ALIGN + ETH_OVERHEAD +
			   edev->ndev->mtu;

#ifdef _HAS_BUILD_SKB_V2 /* QEDE_UPSTREAM */
	rxq->rx_headroom = edev->xdp_prog ? XDP_PACKET_HEADROOM : NET_SKB_PAD;
	size = rxq->rx_headroom +
		SKB_DATA_ALIGN(sizeof(struct skb_shared_info));
#else
	rxq->rx_headroom = edev->xdp_prog ? XDP_PACKET_HEADROOM : 0;
	size = rxq->rx_headroom;
#endif

	/* Make sure that the headroom and  payload fit in a single page */
	if (rxq->rx_buf_size + size > PAGE_SIZE)
		rxq->rx_buf_size = PAGE_SIZE - size;

	/* Segment size to spilt a page in multiple equal parts ,
	 * unless XDP is used in which case we'd use the entire page.
	 */
	if (!edev->xdp_prog) {
		size = size + rxq->rx_buf_size;
		rxq->rx_buf_seg_size = roundup_pow_of_two(size);
	} else {
		rxq->rx_buf_seg_size = PAGE_SIZE;
	}

	/* Allocate the parallel driver ring for Rx buffers */
	size = sizeof(*rxq->sw_rx_ring) * RX_RING_SIZE;
	rxq->sw_rx_ring = kzalloc(size, GFP_KERNEL);
	if (!rxq->sw_rx_ring) {
		DP_ERR(edev, "Rx buffers ring allocation failed\n");
		rc = -ENOMEM;
		goto err;
	}

	/* Allocate FW Rx ring  */
	edev->ops->common->chain_params_init(&chain_params,
					    QED_CHAIN_USE_TO_CONSUME_PRODUCE,
					    QED_CHAIN_MODE_NEXT_PTR,
					    QED_CHAIN_CNT_TYPE_U16,
					    RX_RING_SIZE,
					    sizeof(struct eth_rx_bd));
	rc = edev->ops->common->chain_alloc(edev->cdev, &rxq->rx_bd_ring,
					    &chain_params);
	if (rc)
		goto err;

	/* Allocate FW completion ring */
	edev->ops->common->chain_params_init(&chain_params,
					    QED_CHAIN_USE_TO_CONSUME,
					    QED_CHAIN_MODE_PBL,
					    QED_CHAIN_CNT_TYPE_U16,
					    RX_RING_SIZE,
					    sizeof(union eth_rx_cqe));
	rc = edev->ops->common->chain_alloc(edev->cdev, &rxq->rx_comp_ring,
					    &chain_params);
	if (rc)
		goto err;

	rc = qede_alloc_page_pool(rxq);
	if (rc) {
		DP_ERR(edev, "Page pool allocation failed\n");
		goto err;
	}

	/* Allocate buffers for the Rx ring */
	rxq->filled_buffers = 0;
	for (i = 0; i < rxq->num_rx_buffers; i++) {
		rc = qede_alloc_rx_buffer(rxq, false);
		if (rc) {
			DP_ERR(edev,
			       "Rx buffers allocation failed at index %d\n", i);
			goto err;
		}

		/* Since page-pool allocation would fail [as its empty], we
		 * need to fix the statistics here,
		 */
		rxq->pool_unready = 0;
	}

	/* Don't perform FW aggregations in case of XDP */
	if (edev->xdp_prog || edev->ndev->mtu > PAGE_SIZE) {
		edev->gro_disable = 1;
#ifdef _HAS_NET_GRO_HW /* QEDE_UPSTREAM */
		edev->ndev->features &= ~NETIF_F_GRO_HW;
#endif
	}

	/* Allocate TPA buffers for SGE ring, this may fail in which case
	 * tpa_disable will be switched on after function call
	 */
#ifndef _HAS_BUILD_SKB_V2 /* ! QEDE_UPSTREAM */
	rc = qede_alloc_sge_mem(edev, rxq);
#endif
	if (!edev->gro_disable)
		qede_set_tpa_param(rxq);

err:
	return rc;
}

static void qede_free_mem_txq(struct qede_dev *edev,
			      struct qede_tx_queue *txq)
{
	/* Free the parallel SW ring */
	if (txq->is_xdp)
		kfree(txq->sw_tx_ring.xdp);
	else
		kfree(txq->sw_tx_ring.skbs);

	/* Free the real RQ ring used by FW */
	edev->ops->common->chain_free(edev->cdev, &txq->tx_pbl);

	memset(txq, 0, sizeof(*txq));
}

/* This function allocates all memory needed per Tx queue */
static int qede_alloc_mem_txq(struct qede_dev *edev,
			      struct qede_tx_queue *txq)
{
	struct qed_chain_params chain_params;
	union eth_tx_bd_types *p_virt;
	int size, rc;

	txq->num_tx_buffers = edev->q_num_tx_buffers;

	/* Allocate the parallel driver ring for Tx buffers */
	if (txq->is_xdp) {
		size = sizeof(*txq->sw_tx_ring.xdp) * txq->num_tx_buffers;
		txq->sw_tx_ring.xdp = kzalloc(size, GFP_KERNEL);
		if (!txq->sw_tx_ring.xdp)
			goto err;
	} else {
		size = sizeof(*txq->sw_tx_ring.skbs) * txq->num_tx_buffers;
		txq->sw_tx_ring.skbs = kzalloc(size, GFP_KERNEL);
		if (!txq->sw_tx_ring.skbs)
			goto err;
	}

	edev->ops->common->chain_params_init(&chain_params,
					    QED_CHAIN_USE_TO_CONSUME_PRODUCE,
					    QED_CHAIN_MODE_PBL,
					    QED_CHAIN_CNT_TYPE_U16,
					    txq->num_tx_buffers,
					    sizeof(*p_virt));
	rc = edev->ops->common->chain_alloc(edev->cdev, &txq->tx_pbl,
					    &chain_params);
	if (rc)
		goto err;

	return 0;

err:
	qede_free_mem_txq(edev, txq);
	return -ENOMEM;
}

/* This function frees all memory of a single fp */
static void qede_free_mem_fp(struct qede_dev *edev,
			      struct qede_fastpath *fp)
{
	qede_free_mem_sb(edev, fp->sb_info, fp->id);

	if (fp->type & QEDE_FASTPATH_RX)
		qede_free_mem_rxq(edev, fp->rxq);

	if (fp->type & QEDE_FASTPATH_XDP)
		qede_free_mem_txq(edev, fp->xdp_tx);

	if (fp->type & QEDE_FASTPATH_TX) {
		int cos;

		for_each_cos_in_txq(edev, cos)
			qede_free_mem_txq(edev, &fp->txq[cos]);
	}
}

/* This function allocates all memory needed for a single fp (i.e. an entity
 * which contains status block, one rx queue and/or multiple per-TC tx queues.
 */
static int qede_alloc_mem_fp(struct qede_dev *edev,
			      struct qede_fastpath *fp)
{
	int rc = 0;

	rc = qede_alloc_mem_sb(edev, fp->sb_info, fp->id);
	if (rc)
		goto out;

	if (fp->type & QEDE_FASTPATH_RX) {
		rc = qede_alloc_mem_rxq(edev, fp->rxq);
		if (rc)
			goto out;
	}

	if (fp->type & QEDE_FASTPATH_XDP) {
		rc = qede_alloc_mem_txq(edev, fp->xdp_tx);
		if (rc)
			goto out;
	}

	if (fp->type & QEDE_FASTPATH_TX) {
		int cos;

		for_each_cos_in_txq(edev, cos) {
			rc = qede_alloc_mem_txq(edev, &fp->txq[cos]);
			if (rc)
				goto out;
		}
	}

out:
	return rc;
}

static void qede_free_mem_load(struct qede_dev *edev)
{
	int i;

	for_each_queue(i) {
		struct qede_fastpath *fp = &edev->fp_array[i];

		qede_free_mem_fp(edev, fp);
	}
}

/* This function allocates all qede memory at NIC load. */
static int qede_alloc_mem_load(struct qede_dev *edev)
{
	int rc = 0, queue_id;

	for (queue_id = 0; queue_id < QEDE_QUEUE_CNT(edev); queue_id++) {
		struct qede_fastpath *fp = &edev->fp_array[queue_id];

		rc = qede_alloc_mem_fp(edev, fp);
		if (rc) {
			/* @TBD - try shrinking */
			DP_ERR(edev,
			       "Failed to allocate memory for fastpath - rss id = %d\n",
			       queue_id);
			qede_free_mem_load(edev);
			return rc;
		}
	}

	return 0;
}

static void qede_empty_tx_queue(struct qede_dev *edev,
				struct qede_tx_queue *txq)
{
	unsigned int pkts_compl = 0, bytes_compl = 0;
	struct netdev_queue *netdev_txq;
	int rc, len = 0;

	netdev_txq = netdev_get_tx_queue(txq->fp->ndev, txq->ndev_txq_id);

	while (qed_chain_get_cons_idx(&txq->tx_pbl) !=
	       qed_chain_get_prod_idx(&txq->tx_pbl)) {
		DP_VERBOSE(edev, NETIF_MSG_IFDOWN,
			   "Freeing a packet on tx queue[%d]: chain_cons 0x%x, chain_prod 0x%x\n",
			   txq->index, qed_chain_get_cons_idx(&txq->tx_pbl),
			   qed_chain_get_prod_idx(&txq->tx_pbl));

		rc = qede_free_tx_pkt(edev, txq, &len);
		if (rc) {
			DP_NOTICE(edev,
				  "Failed to free a packet on tx queue[%d]: chain_cons 0x%x, chain_prod 0x%x\n",
				  txq->index,
				  qed_chain_get_cons_idx(&txq->tx_pbl),
				  qed_chain_get_prod_idx(&txq->tx_pbl));
			break;
		}

		bytes_compl += len;
		pkts_compl++;
		txq->sw_tx_cons = (txq->sw_tx_cons + 1) % txq->num_tx_buffers;
	}

	netdev_tx_completed_queue(netdev_txq, pkts_compl, bytes_compl);
}

static void qede_empty_tx_queues(struct qede_dev *edev)
{
	struct qede_tx_queue *txq;
	int i;

	for_each_queue(i)
		if (edev->fp_array[i].type & QEDE_FASTPATH_TX) {
			int cos;

			for_each_cos_in_txq(edev, cos) {
				struct qede_fastpath *fp;

				fp = &edev->fp_array[i];
				txq = &fp->txq[cos];
				qede_empty_tx_queue(edev, txq);
				netdev_tx_reset_queue(netdev_get_tx_queue(edev->ndev,
									  txq->ndev_txq_id));
			}
		}
}

/* This function inits fp content and resets the SB, RXQ and TXQ structures */
static void qede_init_fp(struct qede_dev *edev)
{
	int queue_id, rxq_index = 0, txq_index = 0;
	struct qede_fastpath *fp;

	for_each_queue(queue_id) {
		fp = &edev->fp_array[queue_id];

		fp->edev = edev;
		fp->id = queue_id;

		if (fp->type & QEDE_FASTPATH_XDP) {
			fp->xdp_tx->index = QEDE_TXQ_IDX_TO_XDP(edev,
								rxq_index);
			fp->xdp_tx->is_xdp = 1;
		}

		if (fp->type & QEDE_FASTPATH_RX) {
			fp->rxq->rxq_id = rxq_index++;

			/* Determine how to map buffers for this queue */
			if (fp->type & QEDE_FASTPATH_XDP)
				fp->rxq->data_direction = DMA_BIDIRECTIONAL;
			else
				fp->rxq->data_direction = DMA_FROM_DEVICE;
			fp->rxq->dev = &edev->pdev->dev;
			fp->rxq->fp = fp;
		}

		if (fp->type & QEDE_FASTPATH_TX) {
			int cos;

			for_each_cos_in_txq(edev, cos) {
				struct qede_tx_queue *txq = &fp->txq[cos];
				struct qede_fwd_dev *fwd_dev;
				u16 ndev_tx_id;

				txq->cos = cos;
				txq->index = txq_index;
				ndev_tx_id = QEDE_TXQ_TO_NDEV_TXQ_ID(edev, txq);

				if (fp->fwd_fp) {
					fwd_dev = fp->fwd_dev;
					ndev_tx_id = txq_index -
						     fwd_dev->base_queue_id;
				}

				txq->ndev_txq_id = ndev_tx_id;

				if (edev->dev_info.is_legacy)
					txq->is_legacy = 1;

				txq->fp = fp;
				txq->dev = &edev->pdev->dev;
			}

			txq_index++;
		}

		snprintf(fp->name, sizeof(fp->name), "%s-fp-%d",
			 edev->ndev->name, queue_id);
	}

#ifdef _HAS_NET_GRO_HW /* QEDE_UPSTREAM */
	edev->gro_disable = !(edev->ndev->features & NETIF_F_GRO_HW);
#else
	edev->gro_disable = !(edev->ndev->features & NETIF_F_GRO);
#endif

#ifndef QEDE_UPSTREAM
	edev->gro_disable |= gro_disable;
#endif
}

static int _qede_set_real_num_queues(struct qede_dev *edev,
				     struct net_device *ndev,
				     int tx, int rx)
{
	int rc = 0;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 36)) /* QEDE_UPSTREAM */
	rc = netif_set_real_num_tx_queues(ndev, tx);
	if (rc) {
		DP_NOTICE(edev,
			  "Failed to set real number of Tx queues [%d] for %s\n",
			  tx, ndev->name);
		return rc;
	}
#else
	netif_set_real_num_tx_queues(ndev, tx);
#endif
	rc = netif_set_real_num_rx_queues(ndev, rx);
	if (rc) {
		DP_NOTICE(edev,
			  "Failed to set real number of Rx queues [%d] for %s\n",
			  rx, ndev->name);
		return rc;
	}

	return 0;
}

static int qede_set_real_num_queues(struct qede_dev *edev)
{
	struct qede_fwd_dev *fwd_dev = NULL;
	int num_tx, num_rx, rc = 0;
	struct net_device *ndev;

	/* Since we try to allocate Fastpath queues based on the number queues
	 * of macvlan devices. There is a chance we may not get required amount
	 * of queues, so set actual queues for macvlan devices.
	 */
	list_for_each_entry(fwd_dev, &edev->fwd_dev_list, list) {
		if (!fwd_dev)
			break;

		ndev = fwd_dev->upper_ndev;
		_qede_set_real_num_queues(edev, ndev, fwd_dev->num_queues,
					  fwd_dev->num_queues);
	}

	num_tx = QEDE_BASE_TSS_COUNT(edev) * edev->dev_info.num_tc;
	num_rx = QEDE_BASE_RSS_COUNT(edev);
	rc = _qede_set_real_num_queues(edev, edev->ndev, num_tx, num_rx);

	return rc;
}

static void qede_napi_disable_remove(struct qede_dev *edev)
{
	int i;

	for_each_queue(i) {
		napi_disable(&edev->fp_array[i].napi);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)) /* QEDE_UPSTREAM */
		netif_napi_del(&edev->fp_array[i].napi);
#endif
	}
}

static void qede_napi_add_enable(struct qede_dev *edev)
{
	int i;

	/* Add NAPI objects */
	for_each_queue(i) {
		netif_napi_add(edev->ndev, &edev->fp_array[i].napi,
			       qede_poll, NAPI_POLL_WEIGHT);
		napi_enable(&edev->fp_array[i].napi);
	}
}

static void qede_sync_free_irqs(struct qede_dev *edev)
{
	int i;

	for (i = 0; i < edev->int_info.used_cnt; i++) {
		if (edev->int_info.msix_cnt) {
			synchronize_irq(edev->int_info.msix[i].vector);
#ifdef _HAS_IRQ_SET_AFFINITY_HINT
			irq_set_affinity_hint(edev->int_info.msix[i].vector, NULL);
#endif
			free_irq(edev->int_info.msix[i].vector,
				 &edev->fp_array[i]);
		} else {
			/* @@@TODO - how to sync. simd ? */
			edev->ops->common->simd_handler_clean(edev->cdev, i);
		}
	}

	edev->int_info.used_cnt = 0;
}

static int qede_req_msix_irqs(struct qede_dev *edev)
{
	int i, rc;

#ifdef _HAS_IRQ_SET_AFFINITY_HINT
		int cpu;
#endif
#ifdef CONFIG_RFS_ACCEL
	struct cpu_rmap *rmap = NULL;
	bool flag = true;
#endif
	/* Sanitize number of interrupts == number of prepared RSS queues */
	if (QEDE_QUEUE_CNT(edev) > edev->int_info.msix_cnt) {
		DP_ERR(edev,
		       "Interrupt mismatch: %d fast path queues > %d MSI-x vectors\n",
		       QEDE_QUEUE_CNT(edev), edev->int_info.msix_cnt);
		return -EINVAL;
	}

#ifdef CONFIG_RFS_ACCEL
#ifdef _HAS_NDEV_RFS_INFO /* ! QEDE_UPSTREAM */
	rmap = netdev_extended(edev->ndev)->rfs_data.rx_cpu_rmap;
#else
	rmap = edev->ndev->rx_cpu_rmap;
#endif
#endif

	for (i = 0; i < QEDE_QUEUE_CNT(edev); i++) {
#ifdef CONFIG_RFS_ACCEL
		struct qede_fastpath *fp = &edev->fp_array[i];

		if (rmap && flag &&
		    (fp->type & QEDE_FASTPATH_RX && !fp->fwd_fp)) {
			rc = irq_cpu_rmap_add(rmap,
					      edev->int_info.msix[i].vector);
			if (rc) {
				DP_ERR(edev, "Failed to add CPU rmap\n");
				qede_free_cpu_rmap(edev);
				flag = false;
			}
		}
#endif
		rc = request_irq(edev->int_info.msix[i].vector,
				 qede_msix_fp_int, 0, edev->fp_array[i].name,
				 &edev->fp_array[i]);
		if (rc) {
			DP_ERR(edev, "Request fp %d irq failed\n", i);
#ifdef CONFIG_RFS_ACCEL
			qede_free_cpu_rmap(edev);
#endif
			qede_sync_free_irqs(edev);
			return rc;
		}

#ifdef _HAS_IRQ_SET_AFFINITY_HINT
		cpu = cpumask_local_spread(i, dev_to_node(&edev->pdev->dev));
		irq_set_affinity_hint(edev->int_info.msix[i].vector, get_cpu_mask(cpu));
		DP_VERBOSE(edev, NETIF_MSG_INTR, "IRQ (%d) masked to CPU (%d)\n",
			   edev->int_info.msix[i].vector, cpu);
#endif
		DP_VERBOSE(edev, NETIF_MSG_INTR,
			   "Requested fp irq for %s [entry %d]. Cookie is at %p\n",
			   edev->fp_array[i].name, i,
			   &edev->fp_array[i]);
		edev->int_info.used_cnt++;
	}

	return 0;
}

static void qede_simd_fp_handler(void *cookie)
{
	struct qede_fastpath *fp = (struct qede_fastpath *)cookie;

	napi_schedule_irqoff(&fp->napi);
}

static int qede_setup_irqs(struct qede_dev *edev)
{
	int i, rc = 0;

	/* Learn Interrupt configuration */
	rc = edev->ops->common->get_fp_int(edev->cdev, &edev->int_info);
	if (rc)
		return rc;

	if (edev->int_info.msix_cnt) {
		rc = qede_req_msix_irqs(edev);
		if (rc)
			return rc;
		edev->ndev->irq = edev->int_info.msix[0].vector;
	} else {
		const struct qed_common_ops *ops;

		/* qed should learn receive the RSS ids and callbacks */
		ops = edev->ops->common;
		for (i = 0; i < QEDE_QUEUE_CNT(edev); i++)
			ops->simd_handler_config(edev->cdev,
						 &edev->fp_array[i], i,
						 qede_simd_fp_handler);
		edev->int_info.used_cnt = QEDE_QUEUE_CNT(edev);
	}
	return 0;
}

static int qede_drain_txq(struct qede_dev *edev,
			  struct qede_tx_queue *txq,
			  bool allow_drain)
{
	int rc, cnt = 1000;

	while (txq->sw_tx_cons != txq->sw_tx_prod) {
		if (!cnt) {
			if (allow_drain) {
				DP_NOTICE(edev, "Tx queue[%d] is stuck, requesting MCP to drain\n",
					  txq->index);
				rc = edev->ops->common->drain(edev->cdev);
				if (rc)
					return rc;
				return qede_drain_txq(edev, txq, false);
			}
			DP_NOTICE(edev, "Timeout waiting for tx queue[%d]: PROD=%d, CONS=%d\n",
				  txq->index, txq->sw_tx_prod,
				  txq->sw_tx_cons);
			return -ENODEV;
		}
		cnt--;
		usleep_range(1000, 2000);
		barrier();
	}

	/* FW finished processing, wait for HW to transmit all tx packets */
	usleep_range(1000, 2000);

	return 0;
}

static int qede_stop_txq(struct qede_dev *edev,
			 struct qede_tx_queue *txq, int rss_id)
{
	int rc;

	/* delete doorbell from doorbell recovery mechanism */
	rc = edev->ops->common->db_recovery_del(edev->cdev, txq->doorbell_addr,
		&txq->tx_db);
/*	@@@TBD
 *	if (rc)
 *		return rc; --> this doesn't end well...
 */

	return edev->ops->q_tx_stop(edev->cdev, rss_id, txq->handle);
}

static int qede_stop_queues(struct qede_dev *edev)
{
	struct qed_update_vport_params *vport_update_params;
	struct qed_dev *cdev = edev->cdev;
	struct qede_fastpath *fp;
	int rc, i;

	/* Disable the vport */
	vport_update_params = vzalloc(sizeof(*vport_update_params));
	if (!vport_update_params)
		return -ENOMEM;

	vport_update_params->update_vport_active_flg = 1;
	vport_update_params->vport_active_flg = 0;
	vport_update_params->update_rss_flg = 0;

	for (i = 0; i <= edev->vport_id_used; i++) {
		vport_update_params->vport_id = i;

		rc = edev->ops->vport_update(cdev, vport_update_params);
		if (rc) {
			vfree(vport_update_params);
			DP_ERR(edev, "Failed to update vport\n");
			return rc;
		}
	}

	vfree(vport_update_params);

	/* Flush Tx queues. If needed, request drain from MCP */
	for_each_queue(i) {
		fp = &edev->fp_array[i];

		if (fp->type & QEDE_FASTPATH_TX) {
			int cos;

			for_each_cos_in_txq(edev, cos) {
				rc = qede_drain_txq(edev, &fp->txq[cos], true);
				if (rc)
					return rc;
			}
		}

		if (fp->type & QEDE_FASTPATH_XDP) {
			rc = qede_drain_txq(edev, fp->xdp_tx, true);
			if (rc)
				return rc;
		}
	}

	/* Stop all Queues in reverse order*/
	for (i = QEDE_QUEUE_CNT(edev) - 1; i >= 0; i--) {

		fp = &edev->fp_array[i];

		/* Stop the Tx Queue(s)*/
		if (fp->type & QEDE_FASTPATH_TX) {
			struct qede_tx_queue *txq;
			int cos;

			for_each_cos_in_txq(edev, cos) {
				txq = &fp->txq[cos];
				rc = qede_stop_txq(edev, txq, i);
				if (rc)
					return rc;

				netdev_tx_reset_queue(netdev_get_tx_queue(edev->ndev,
									  txq->ndev_txq_id));
			}
		}

		/* Stop the Rx Queue*/
		if (fp->type & QEDE_FASTPATH_RX) {
			rc = edev->ops->q_rx_stop(cdev, i, fp->rxq->handle);
			if (rc) {
				DP_ERR(edev, "Failed to stop RXQ #%d\n", i);
				return rc;
			}
		}

#ifdef _HAS_NDO_XDP /* QEDE_UPSTREAM */
		/* Stop the XDP forwarding queue */
		if (fp->type & QEDE_FASTPATH_XDP) {
			rc = qede_stop_txq(edev, fp->xdp_tx, i);
			if (rc)
				return rc;

			bpf_prog_put(fp->rxq->xdp_prog);
		}
#endif
	}

	/* Stop the vport */
	for (i = 0; i <= edev->vport_id_used; i++) {
		rc = edev->ops->vport_stop(cdev, i);
		if (rc) {
			DP_ERR(edev, "Failed to stop VPORT %d\n", i);
			break;
		}
	}

	return rc;
}

static int qede_start_txq(struct qede_dev *edev,
			  struct qede_fastpath *fp,
			  struct qede_tx_queue *txq,
			  u8 rss_id, u16 sb_idx)
{
	dma_addr_t phys_table = qed_chain_get_pbl_phys(&txq->tx_pbl);
	u32 page_cnt = qed_chain_get_page_cnt(&txq->tx_pbl);
	struct qed_queue_start_common_params params;
	struct qed_txq_start_ret_params ret_params;
	int rc;

	memset(&params, 0, sizeof(params));
	memset(&ret_params, 0, sizeof(ret_params));

	/* Let the XDP queue share the queue-zone with one of the regular txq.
	 * We don't really care about its coalescing.
	 */
	if (txq->is_xdp)
		params.queue_id = QEDE_TXQ_XDP_TO_IDX(edev, txq);
	else
		params.queue_id = txq->index;

	params.p_sb = fp->sb_info;
	params.sb_idx = sb_idx;
	params.tc = txq->cos;
	params.vport_id = fp->vport_id;

	rc = edev->ops->q_tx_start(edev->cdev, rss_id, &params, phys_table,
				   page_cnt, &ret_params);
	if (rc) {
		DP_ERR(edev, "Start TXQ #%d failed %d\n",
		       txq->index, rc);
		return rc;
	}

	txq->doorbell_addr = ret_params.p_doorbell;
	txq->handle = ret_params.p_handle;

	/* Determine the FW consumer address associated */
	txq->hw_cons_ptr = &fp->sb_info->sb_pi_array[sb_idx];

	/* Prepare the doorbell parameters */
	SET_FIELD(txq->tx_db.data.params, ETH_DB_DATA_DEST, DB_DEST_XCM);
	SET_FIELD(txq->tx_db.data.params, ETH_DB_DATA_AGG_CMD, DB_AGG_CMD_SET);
	SET_FIELD(txq->tx_db.data.params, ETH_DB_DATA_AGG_VAL_SEL,
		  DQ_XCM_ETH_TX_BD_PROD_CMD);
	txq->tx_db.data.agg_flags = DQ_XCM_ETH_DQ_CF_CMD;

	/* register doorbell with doorbell recovery mechanism */
	rc = edev->ops->common->db_recovery_add(edev->cdev, txq->doorbell_addr,
						&txq->tx_db, DB_REC_WIDTH_32B,
						DB_REC_KERNEL);

	return rc;
}

static int qede_start_queues(struct qede_dev *edev, bool clear_stats)
{
	int vlan_removal_en = 1;
	struct qed_dev *cdev = edev->cdev;
	struct qed_dev_info *qed_info = &edev->dev_info.common;
	struct qed_update_vport_params *vport_update_params;
	struct qed_queue_start_common_params q_params;
	struct qed_start_vport_params start = {0};
	int i, rc = 0;

#if defined(OLD_VLAN) /* ! QEDE_UPSTREAM */
	if (!edev->vlan_group)
		vlan_removal_en = 0;
#endif
	if (!edev->num_queues) {
		DP_ERR(edev, "Cannot update V-VPORT as active as there are no Rx queues\n");
		return -EINVAL;
	}

	vport_update_params = vzalloc(sizeof(*vport_update_params));
	if (!vport_update_params)
		return -ENOMEM;

	start.handle_ptp_pkts = !!(edev->ptp);
	start.gro_enable = !edev->gro_disable;
	start.mtu = edev->ndev->mtu + QINQ_HDR_LEN;
	start.drop_ttl0 = false;
	start.remove_inner_vlan = vlan_removal_en;
	start.clear_stats = clear_stats;

	if (!NET_IP_ALIGN)
		start.zero_placement_offset = 1;

	for (i = 0; i <= edev->vport_id_used; i++) {
		start.vport_id = i;

		rc = edev->ops->vport_start(cdev, &start);
		if (rc) {
			DP_ERR(edev, "Start V-PORT failed %d\n", rc);
			goto out;
		}

		DP_VERBOSE(edev, NETIF_MSG_IFUP,
			   "Start vport ramrod passed, vport_id = %d, MTU = %d, vlan_removal_en = %d\n",
			   start.vport_id, edev->ndev->mtu + 0xe,
			   vlan_removal_en);
	}

	for_each_queue(i) {
		struct qede_fastpath *fp = &edev->fp_array[i];
		dma_addr_t p_phys_table;
		u32 page_cnt;

		if (fp->type & QEDE_FASTPATH_RX) {
			struct qed_rxq_start_ret_params ret_params;
			struct qede_rx_queue *rxq = fp->rxq;
			__le16 *val;

			memset(&ret_params, 0, sizeof(ret_params));
			memset(&q_params, 0, sizeof(q_params));
			q_params.queue_id = rxq->rxq_id;
			q_params.vport_id = fp->vport_id;
			q_params.p_sb = fp->sb_info;
			q_params.sb_idx = RX_PI;

			p_phys_table = qed_chain_get_pbl_phys(
						&rxq->rx_comp_ring);
			page_cnt = qed_chain_get_page_cnt(&rxq->rx_comp_ring);

			rc = edev->ops->q_rx_start(cdev, i, &q_params,
						   rxq->rx_buf_size,
						   rxq->rx_bd_ring.p_phys_addr,
						   p_phys_table,
						   page_cnt,
						   &ret_params);
			if (rc) {
				DP_ERR(edev, "Start RXQ #%d failed %d\n", i,
				       rc);
				goto out;
			}

			/* Use the return parameters */
			rxq->hw_rxq_prod_addr = ret_params.p_prod;
			rxq->handle = ret_params.p_handle;

			val = &fp->sb_info->sb_pi_array[RX_PI];
			rxq->hw_cons_ptr = val;

			qede_update_rx_prod(edev, rxq);
		}

#ifdef _HAS_NDO_XDP /* QEDE_UPSTREAM */
		if (fp->type & QEDE_FASTPATH_XDP) {
			rc = qede_start_txq(edev, fp, fp->xdp_tx, i, XDP_PI);
			if (rc)
				goto out;
#ifndef _HAS_VOID_BPF_PROG_ADD /* ! QEDE_UPSTREAM */
			fp->rxq->xdp_prog = bpf_prog_add(edev->xdp_prog, 1);
			if (IS_ERR(fp->rxq->xdp_prog)) {
				rc = PTR_ERR(fp->rxq->xdp_prog);
				fp->rxq->xdp_prog = NULL;
				goto out;
			}
#else
			bpf_prog_add(edev->xdp_prog, 1);
			fp->rxq->xdp_prog = edev->xdp_prog;
#endif
		}
#endif

		if (fp->type & QEDE_FASTPATH_TX) {
			int cos;

			for_each_cos_in_txq(edev, cos) {
				rc = qede_start_txq(edev, fp, &fp->txq[cos], i,
						    TX_PI(cos));
				if (rc)
					goto out;
			}
		}
	}

	vport_update_params->update_vport_active_flg = 1;
	vport_update_params->vport_active_flg = 1;

	if ((qed_info->b_inter_pf_switch || pci_num_vf(edev->pdev)) &&
	    qed_info->tx_switching) {
		vport_update_params->update_tx_switching_flg = 1;
		vport_update_params->tx_switching_flg = 1;
	}

	for (i = 0; i <= edev->vport_id_used; i++) {
		vport_update_params->vport_id = i;

		memset(&vport_update_params->rss_params, 0,
		       sizeof(vport_update_params->rss_params));
		vport_update_params->update_rss_flg = 0;

		/* fill RSS only for base device's vport */
		if (i == QEDE_BASE_DEV_VPORT_ID) {
			u8 *update_rss_flg;

			update_rss_flg = &vport_update_params->update_rss_flg;
			qede_fill_rss_params(edev,
					     &vport_update_params->rss_params,
					     update_rss_flg);
		}

		rc = edev->ops->vport_update(cdev, vport_update_params);
		if (rc) {
			DP_ERR(edev, "Update V-PORT id %d failed %d\n", rc, i);
			goto out;
		}
	}
out:
	vfree(vport_update_params);
	return rc;
}

static void qede_dev_close_tx(struct net_device *ndev)
{
	netif_tx_stop_all_queues(ndev);
	netif_carrier_off(ndev);
	netif_tx_disable(ndev);
}

static void qede_close_os_tx(struct qede_dev *edev)
{
	struct qede_fwd_dev *fwd_dev = NULL;

	/* close upper devices Tx queues first */
	if (!list_empty(&edev->fwd_dev_list)) {
		list_for_each_entry(fwd_dev, &edev->fwd_dev_list, list) {
			if (!fwd_dev)
				break;

			qede_dev_close_tx(fwd_dev->upper_ndev);
		}
	}

	/* now close Tx queue of base device */
	qede_dev_close_tx(edev->ndev);
}

static void qede_dev_start_tx(struct net_device *ndev)
{
	netif_carrier_on(ndev);
	netif_tx_start_all_queues(ndev);
}

static void qede_start_os_tx(struct qede_dev *edev)
{
	struct qede_fwd_dev *fwd_dev = NULL;

	/* start Tx queue of base device */
	qede_dev_start_tx(edev->ndev);

	/* now start upper device's Tx queues */
	if (!list_empty(&edev->fwd_dev_list)) {
		list_for_each_entry(fwd_dev, &edev->fwd_dev_list, list) {
			if (!fwd_dev)
				break;

			qede_dev_start_tx(fwd_dev->upper_ndev);
		}
	}
}

static int qede_unload(struct qede_dev *edev, enum qede_update_flags flags,
			bool is_locked)
{
	struct qed_link_params link_params;
	int rc = 0;

	DP_INFO(edev, "Starting qede unload, flags = 0x%x, is_lock=%d edev->state=%d\n",
		flags, is_locked, edev->state);

	if (!is_locked)
		__qede_lock(edev);

	clear_bit(QEDE_FLAGS_LINK_REQUESTED, &edev->flags);

	/* Dont change the state during recovery & reload condition
	 * like mtu and ring param changes.
	 */
	if (!(flags & QEDE_UPDATE_RECOVERY) && !(flags & QEDE_RELOAD))
		edev->state = QEDE_STATE_CLOSED;

	qede_rdma_dev_event_close(edev);

	/* Close OS Tx */
	qede_close_os_tx(edev);

	/* TODO - it's possible link will be re-activated by a different
	 * PF; Inner qede state should prevent netif_carrier_on, etc.
	 * from bein called.
	 */

	if (!(flags & QEDE_UPDATE_RECOVERY) && !edev->aer_recov_prog) {
		/* Reset the link */
		memset(&link_params, 0, sizeof(link_params));
		link_params.link_up = false;
		edev->ops->common->set_link(edev->cdev, &link_params);

		/* Set qede inner state for closure */
		LEGACY_QEDE_SET_JIFFIES(edev->ndev->trans_start, jiffies);

		rc = qede_stop_queues(edev);
		if (rc)
			DP_NOTICE(edev, "Stopping queue failed\n");
	}

	/* unmark configured vlans so they'll
	 * be reconfigured on subsequent reload
	 */
	qede_vlan_mark_nonconfigured(edev);

	if (!edev->aer_recov_prog)
		edev->ops->fastpath_stop(edev->cdev);

#ifdef CONFIG_RFS_ACCEL
	if (edev->dev_info.common.b_arfs_capable) {
		qede_delete_non_user_arfs_flows(edev);
		qede_free_cpu_rmap(edev);
	}
#endif
	qede_napi_disable_remove(edev);

	/* Release the interrupts */
	qede_sync_free_irqs(edev);
	edev->ops->common->set_fp_int(edev->cdev, 0);

	if (!(flags & (QEDE_UPDATE_FP_BUFFERS | QEDE_UPDATE_COMPLETE)))
		qede_empty_tx_queues(edev);

	if (flags & (QEDE_UPDATE_FP_BUFFERS | QEDE_UPDATE_COMPLETE |
		     QEDE_UPDATE_RECOVERY))
		qede_free_mem_load(edev);

	if (flags & (QEDE_UPDATE_FP_ELEM | QEDE_UPDATE_COMPLETE |
		     QEDE_UPDATE_RECOVERY))
		qede_free_fp_array(edev);

	edev->num_queues = 0;
	edev->ptp_skip_txts = 0;

	if (!(flags & QEDE_UPDATE_RECOVERY))
		DP_NOTICE(edev, "Link is down\n");

        if (!is_locked)
                __qede_unlock(edev);

	DP_INFO(edev, "Ending qede unload\n");

	return rc;
}

static int qede_load(struct qede_dev *edev, enum qede_update_flags flags,
		     bool is_locked)
{
	struct qed_link_params link_params;
	struct ethtool_coalesce coal;
#ifdef _HAS_NDO_SETUP_TC /* QEDE_UPSTREAM */
	u8 num_tc;
#endif
	int rc;

	DP_INFO(edev, "Starting qede load, flags = 0x%x, is_locked=%d\n", flags, is_locked);

	if (!is_locked)
		__qede_lock(edev);

	rc = qede_set_num_queues(edev);
	if (rc)
		goto out;

	if (flags & (QEDE_UPDATE_FP_ELEM | QEDE_UPDATE_COMPLETE |
		     QEDE_UPDATE_RECOVERY)) {
		rc = qede_alloc_fp_array(edev);
		if (rc)
			goto out;
	}

	qede_init_fp(edev);

	if (flags & (QEDE_UPDATE_FP_BUFFERS | QEDE_UPDATE_COMPLETE |
		     QEDE_UPDATE_RECOVERY)) {
		rc = qede_alloc_mem_load(edev);
		if (rc)
			goto err1;
		DP_INFO(edev,
			"Allocated %d Rx, %d Tx queues for base device and %d L2 forwarding offload queues\n",
			QEDE_BASE_RSS_COUNT(edev), QEDE_BASE_TSS_COUNT(edev),
			edev->fwd_dev_queues);
	}

	rc = qede_set_real_num_queues(edev);
	if (rc)
		goto err2;

#ifdef CONFIG_RFS_ACCEL
	if (edev->dev_info.common.b_arfs_capable) {
		rc = qede_alloc_cpu_rmap(edev);
		if (rc)
			DP_NOTICE(edev, "Failed to allocate cpu rmap\n");
	}
#endif
	rc = qede_setup_irqs(edev);
	if (rc) {
#ifdef CONFIG_RFS_ACCEL
		if (edev->dev_info.common.b_arfs_capable)
			qede_free_cpu_rmap(edev);
#endif
		goto err2;
	}
	DP_INFO(edev, "Setup IRQs succeeded\n");

	qede_napi_add_enable(edev);
	DP_INFO(edev, "Napi added and enabled\n");

	rc = qede_start_queues(edev, !!(flags & (QEDE_UPDATE_COMPLETE |
						 QEDE_UPDATE_RECOVERY)));
	if (rc)
		goto err3;
	DP_INFO(edev, "Start VPORT, RXQ and TXQ succeeded\n");

#ifdef _HAS_NDO_SETUP_TC /* QEDE_UPSTREAM */
	num_tc = netdev_get_num_tc(edev->ndev);
	num_tc = num_tc ? num_tc : edev->dev_info.num_tc;
	qede_setup_tc(edev->ndev, num_tc);
#endif
	/* Program un-configured VLANs */
	qede_configure_vlan_filters(edev);

	set_bit(QEDE_FLAGS_LINK_REQUESTED, &edev->flags);

	/* Ask for link-up using current configuration */
	memset(&link_params, 0, sizeof(link_params));
	link_params.link_up = true;
	edev->ops->common->set_link(edev->cdev, &link_params);

	edev->state = QEDE_STATE_OPEN;

	memset(&coal, 0, sizeof(coal));
	coal.rx_coalesce_usecs = QED_DEFAULT_RX_USECS;
	coal.tx_coalesce_usecs = QED_DEFAULT_TX_USECS;

#ifdef _HAS_KERNEL_ETHTOOL_COALESCE
	qede_set_coalesce(edev->ndev, &coal, NULL, NULL);
#else
	qede_set_coalesce(edev->ndev, &coal);
#endif
	DP_INFO(edev, "Ending successfully qede load\n");

	goto out;
err3:
	qede_napi_disable_remove(edev);

#ifdef CONFIG_RFS_ACCEL
	if (edev->dev_info.common.b_arfs_capable)
		qede_free_cpu_rmap(edev);
#endif
	qede_sync_free_irqs(edev);
	memset(&edev->int_info, 0, sizeof(struct qed_int_info));
err2:
	if (flags & (QEDE_UPDATE_FP_BUFFERS | QEDE_UPDATE_COMPLETE | QEDE_UPDATE_RECOVERY))
		qede_free_mem_load(edev);
err1:
	edev->ops->common->set_fp_int(edev->cdev, 0);

	if (flags & (QEDE_UPDATE_FP_ELEM | QEDE_UPDATE_COMPLETE | QEDE_UPDATE_RECOVERY))
		qede_free_fp_array(edev);

	edev->num_queues = 0;
	edev->fp_num_tx = 0;
	edev->fp_num_rx = 0;
out:
	if (!is_locked)
		__qede_unlock(edev);

	return rc;
}

/* 'func' should be able to run between unload and reload assuming interface
 * is actually running, or afterwards in case it's currently DOWN.
 */
void qede_reload(struct qede_dev *edev, struct qede_reload_args *args)
{
	enum qede_update_flags flags = args->flags;

	if ((flags & QEDE_UPDATE_FP_ELEM) &&
	    !(flags & QEDE_UPDATE_FP_BUFFERS)) {
		DP_ERR(edev,
		       "Incorrect reload request flags, performing the complete reload\n");
		flags = QEDE_UPDATE_COMPLETE;
	}

	if (!args->is_locked)
		__qede_lock(edev);

	/* Since qede_lock is held, internal state wouldn't change even
	 * if netdev state would start transitioning. Check whether current
	 * internal configuration indicates device is up, then reload.
	 */
	if (edev->state == QEDE_STATE_OPEN) {
		/* Indicate the unload is part of reload */
		flags |= QEDE_RELOAD;

		if (qede_unload(edev, flags, true)) {
			DP_ERR(edev, "Driver unload failed\n");
			goto skip;
		}

		if (args->func)
			args->func(edev, args);

		qede_load(edev, flags, true);

		/* Since no one is going to do it for us, re-configure */
		if (edev->state == QEDE_STATE_OPEN)
			qede_config_rx_mode_for_all(edev);
	} else {
		if (args->func)
			args->func(edev, args);
	}
skip:
	if (!args->is_locked)
		__qede_unlock(edev);
}

/* called with rtnl_lock */
static int qede_open(struct net_device *ndev)
{
	struct qede_dev *edev = netdev_priv(ndev);
	int rc;

	if (edev->ops->common->get_recov_in_prog(edev->cdev)) {
		DP_INFO(edev, "recovery in prog. avoid load\n");

		return -EAGAIN;
	}

	netif_carrier_off(ndev);

	edev->ops->common->set_power_state(edev->cdev, PCI_D0);

	rc = qede_load(edev, QEDE_UPDATE_COMPLETE, false);
	if (rc)
		return rc;

#if HAS_NDO(UDP_TUNNEL_CONFIG) /* QEDE_UPSTREAM */
	udp_tunnel_get_rx_info(ndev);
#else
#ifdef _HAS_ADD_VXLAN_PORT
	vxlan_get_rx_port(ndev);
#endif
#ifdef _HAS_ADD_GENEVE_PORT
	geneve_get_rx_port(ndev);
#endif
#endif
	edev->ops->common->update_drv_state(edev->cdev, true);

	return 0;
}

static int qede_close(struct net_device *ndev)
{
	enum qede_update_flags flag = QEDE_UPDATE_COMPLETE;
	struct qede_dev *edev = netdev_priv(ndev);

	if (edev->ops->common->get_recov_in_prog(edev->cdev)) {
		DP_INFO(edev, "recovery in prog. perform recovery unload\n");
		edev->state = QEDE_STATE_RECOVERY;
		flag = QEDE_UPDATE_RECOVERY;
	}

	qede_unload(edev, flag, false);

	if (edev->cdev)
		edev->ops->common->update_drv_state(edev->cdev, false);

	return 0;
}

#ifdef HAS_BOND_OFFLOAD_SUPPORT /* QEDE_UPSTREAM */
static void qede_handle_link_change(struct qede_dev *edev,
				    struct qed_link_output *link)
{
	struct netdev_notifier_changelowerstate_info info;
	struct netdev_lag_lower_state_info ls_info;

	ls_info.link_up = !!link->link_up;
	ls_info.tx_enabled = !!link->link_up;
	info.lower_state_info = &ls_info;
	qede_handle_lower_state_change_event(edev->ndev, &info);
}
#endif

static void qede_link_update(void *dev, struct qed_link_output *link)
{
	struct qede_dev *edev = dev;
	bool link_changed = false;

	if (!test_bit(QEDE_FLAGS_LINK_REQUESTED, &edev->flags)) {
		DP_VERBOSE(edev, NETIF_MSG_LINK, "Interface is not ready\n");
		return;
	}

	if (link->link_up) {
		if (!netif_carrier_ok(edev->ndev)) {
			DP_NOTICE(edev, "Link is up\n");
			qede_rdma_dev_event_open(edev);
			qede_start_os_tx(edev);
			link_changed = true;
		}
	} else {
		if (netif_carrier_ok(edev->ndev)) {
			DP_NOTICE(edev, "Link is down\n");
			qede_rdma_dev_event_close(edev);
			qede_close_os_tx(edev);
			link_changed = true;
		}
	}

	DP_VERBOSE(edev, NETIF_MSG_LINK, "Link has %s\n",
		   link_changed ? "changed" : "not changed");

	/* In case link is up and RoCE LAG is supported, we might need to sleep
	 * during DCBx negotiation (and maybe some time after).
	 * However, this function can be called in interrupt context.
	 * To overcome this, we add it to the SP task workqueue.
	 */
	if (link_changed) {
		if (link->link_up) {
			set_bit(QEDE_SP_LINK_UP, &edev->sp_flags);
			schedule_delayed_work(&edev->sp_task, 0);
		} else {
			qede_handle_link_change(edev, link);
		}
	}
}

static void qede_schedule_recovery_handler(void *dev)
{
	struct qede_dev *edev = dev;

	if (edev->state == QEDE_STATE_RECOVERY || edev->aer_recov_prog) {
		DP_NOTICE(edev,
			  "Avoid scheduling a recovery handling since already in recovery state\n");
		return;
	}

	set_bit(QEDE_SP_RECOVERY, &edev->sp_flags);
	schedule_delayed_work(&edev->sp_task, 0);

	DP_INFO(edev, "Scheduled a recovery handler\n");
}

static void qede_recovery_failed(struct qede_dev *edev)
{
	netdev_err(edev->ndev, "Recovery handling has failed. Power cycle is needed.\n");

	netif_device_detach(edev->ndev);

	if (edev->cdev)
		edev->ops->common->set_power_state(edev->cdev, PCI_D3hot);
}

static void qede_recovery_handler(struct qede_dev *edev)
{
	u32 curr_state = edev->state;
	int rc;

	DP_NOTICE(edev, "Starting a recovery process\n");

	/* No need to acquire first the qede_lock since is done by qede_sp_task
	 * before calling this function.
	 */
	edev->state = QEDE_STATE_RECOVERY;

	edev->ops->common->recovery_prolog(edev->cdev);

	if (curr_state == QEDE_STATE_OPEN)
		qede_unload(edev, QEDE_UPDATE_RECOVERY, true);

	__qede_remove(edev->pdev, QEDE_REMOVE_RECOVERY);

	rc = __qede_probe(edev->pdev, edev->dp_module, edev->dp_level,
			  IS_VF(edev), QEDE_PROBE_RECOVERY);
	if (rc) {
		edev->cdev = NULL;
		goto err;
	}

	if (curr_state == QEDE_STATE_OPEN) {
		rc = qede_load(edev, QEDE_UPDATE_RECOVERY, true);
		if (rc)
			goto err;

		qede_config_rx_mode_for_all(edev);
#if HAS_NDO(UDP_TUNNEL_CONFIG) /* QEDE_UPSTREAM */
		udp_tunnel_get_rx_info(edev->ndev);
#else
#ifdef _HAS_ADD_VXLAN_PORT
		vxlan_get_rx_port(edev->ndev);
#endif
#ifdef _HAS_ADD_GENEVE_PORT
		geneve_get_rx_port(edev->ndev);
#endif
#endif
	}

	edev->state = curr_state;
	edev->err_flags = 0;

	DP_NOTICE(edev, "Recovery handling is done\n");

	return;

err:
	qede_recovery_failed(edev);
}

static void qede_fan_fail_handler(struct work_struct *work)
{
	struct qede_fan_fail_info *p_fan_fail_info;

	pr_notice("Starting a fan failure handling\n");

	for (;;) {
		/* The lock is required since the scheduler of the fan failure
		 * handler can concurrently add elements to the list.
		 */
		spin_lock_bh(&fan_fail_lock);
		if (list_empty(&fan_fail_list)) {
			spin_unlock_bh(&fan_fail_lock);
			break;
		}
		p_fan_fail_info = list_first_entry(&fan_fail_list,
						   struct qede_fan_fail_info,
						   list);
		list_del(&p_fan_fail_info->list);
		spin_unlock_bh(&fan_fail_lock);

		qede_remove(p_fan_fail_info->pdev);

		kfree(p_fan_fail_info);
	}

	pr_notice("Fan failure handling is done\n");

	/* No need to clear the "QEDE_ERR_IS_HANDLED" bit in the err_flags */
}

static void qede_schedule_fan_fail_handler(struct qede_dev *edev)
{
	struct qede_fan_fail_info *p_fan_fail_info;

	DP_NOTICE(edev, "Fan failure was detected\n");

	p_fan_fail_info = kmalloc(sizeof(*p_fan_fail_info), GFP_ATOMIC);
	if (!p_fan_fail_info) {
		DP_ERR(edev,
		       "Failed to allocate a fan failure info structure\n");
		return;
	}
	p_fan_fail_info->pdev = edev->pdev;

	/* The lock is required since the fan failure handler can concurrently
	 * read/delete elements from the list.
	 */
	spin_lock_bh(&fan_fail_lock);
	list_add_tail(&p_fan_fail_info->list, &fan_fail_list);
	spin_unlock_bh(&fan_fail_lock);

	schedule_delayed_work(&fan_fail_task, 0);
}

static bool qede_is_err_flag_bit_on(struct qede_dev *edev, int bit)
{
	if (test_bit(QEDE_ERR_OVERRIDE_EN, &edev->err_flags_override))
		return test_bit(bit, &edev->err_flags_override);
	else
		return test_bit(bit, &edev->err_flags);
}

static void qede_atomic_hw_err_handler(struct qede_dev *edev)
{
	DP_NOTICE(edev,
		  "Generic non-sleepable HW error handling started - err_flags 0x%lx\n",
		  edev->err_flags);

	/* Prevent HW attentions from being reasserted */
	if (qede_is_err_flag_bit_on(edev, QEDE_ERR_ATTN_CLR_EN))
		edev->ops->common->attn_clr_enable(edev->cdev, true);

	/* Get a call trace of the flow that led to the error */
	WARN_ON(qede_is_err_flag_bit_on(edev, QEDE_ERR_WARN));

	DP_NOTICE(edev, "Generic non-sleepable HW error handling is done\n");
}

static void qede_generic_hw_err_handler(struct qede_dev *edev)
{
	DP_NOTICE(edev,
		  "Starting a generic sleepable HW error handling - err_flags 0x%lx, err_flags_override 0x%lx\n",
		  edev->err_flags, edev->err_flags_override);

#ifndef _HAS_DEVLINK_DUMP /* ! QEDE_UPSTREAM */
	/* Collect debug data */
	if (qede_is_err_flag_bit_on(edev, QEDE_ERR_GET_DBG_INFO))
		edev->ops->common->dbg_save_all_data(edev->cdev, true);
#endif
	if (edev->devlink) {
		DP_NOTICE(edev, "Reporting fatal error to devlink\n");
		edev->ops->common->report_fatal_error(edev->devlink,
						      edev->last_err_type,
						      qede_is_err_flag_bit_on(edev, QEDE_ERR_IS_RECOVERABLE));
	}
	else if (qede_is_err_flag_bit_on(edev, QEDE_ERR_IS_RECOVERABLE)) {
		DP_NOTICE(edev, "Triggering Automatic recovery process\n");
		edev->ops->common->recovery_process(edev->cdev);
	}

	DP_NOTICE(edev, "Generic sleepable HW error handling is done\n");
}

static void qede_set_hw_err_flags(struct qede_dev *edev,
				  enum qed_hw_err_type err_type)
{
	unsigned long err_flags = 0;

	switch (err_type) {
	case QED_HW_ERR_DMAE_FAIL:
		set_bit(QEDE_ERR_WARN, &err_flags);
		COMPAT_FALLTHROUGH;
		/* fallthrough */
	case QED_HW_ERR_MFW_RESP_FAIL:
	case QED_HW_ERR_HW_ATTN:
	case QED_HW_ERR_RAMROD_FAIL:
	case QED_HW_ERR_FW_ASSERT:
		set_bit(QEDE_ERR_ATTN_CLR_EN, &err_flags);
		set_bit(QEDE_ERR_GET_DBG_INFO, &err_flags);
		/* make this error as recverable and start recovery*/
		set_bit(QEDE_ERR_IS_RECOVERABLE, &err_flags);
		break;

	default:
		DP_NOTICE(edev, "Unexpected HW error [%d]\n", err_type);
		break;
	}

	edev->err_flags |= err_flags;
}

static void qede_schedule_hw_err_handler(void *dev,
					 enum qed_hw_err_type err_type)
{
	struct qede_dev *edev = dev;

	/* Fan failure cannot be masked by handling of another HW error or by a
	 * concurrent recovery process.
	 */
	if ((test_and_set_bit(QEDE_ERR_IS_HANDLED, &edev->err_flags) ||
	     edev->state == QEDE_STATE_RECOVERY || edev->aer_recov_prog) &&
	    err_type != QED_HW_ERR_FAN_FAIL) {
		DP_INFO(edev,
			"Avoid scheduling an error handling while another HW error is being handled\n");
		return;
	}

	switch (err_type) {
	case QED_HW_ERR_FAN_FAIL:
		qede_schedule_fan_fail_handler(edev);
		break;

	case QED_HW_ERR_MFW_RESP_FAIL:
	case QED_HW_ERR_HW_ATTN:
	case QED_HW_ERR_DMAE_FAIL:
	case QED_HW_ERR_RAMROD_FAIL:
	case QED_HW_ERR_FW_ASSERT:
		edev->last_err_type = err_type;
		qede_set_hw_err_flags(edev, err_type);
		qede_atomic_hw_err_handler(edev);
		set_bit(QEDE_SP_HW_ERR, &edev->sp_flags);
		schedule_delayed_work(&edev->sp_task, 0);
		break;

	default:
		DP_NOTICE(edev, "Unknown HW error [%d]\n", err_type);
		clear_bit(QEDE_ERR_IS_HANDLED, &edev->err_flags);
		return;
	}

	DP_INFO(edev, "Scheduled a error handler [err_type %d]\n", err_type);
}

static bool qede_is_txq_full(struct qede_dev *edev, struct qede_tx_queue *txq)
{
	struct netdev_queue *netdev_txq;

	netdev_txq = netdev_get_tx_queue(txq->fp->ndev, txq->ndev_txq_id);
#ifdef _HAS_XMIT_MORE /* QEDE_UPSTREAM */
	if (netif_xmit_stopped(netdev_txq))
		return true;
#else
	if (netif_tx_queue_stopped(netdev_txq))
		return true;
#endif

	return false;
}

static void qede_get_generic_tlv_data(void *dev, struct qed_generic_tlvs *data)
{
	struct qede_dev *edev = dev;
	struct DEV_ADDR_LIST *ha;
	int i;

	if (edev->ndev->features & NETIF_F_IP_CSUM)
		data->feat_flags |= QED_TLV_IP_CSUM;
	if (edev->ndev->features & NETIF_F_TSO)
		data->feat_flags |= QED_TLV_LSO;

	ether_addr_copy(data->mac[0], edev->ndev->dev_addr);
	memset(data->mac[1], 0, ETH_ALEN);
	memset(data->mac[2], 0, ETH_ALEN);
	netif_addr_lock_bh(edev->ndev);
	i = 1;
	netdev_for_each_uc_addr(ha, edev->ndev) {
		ether_addr_copy(data->mac[i++], ha->addr);
		if (i == 3)
			break;
	}

	netif_addr_unlock_bh(edev->ndev);
}

static void qede_get_eth_tlv_data(void *dev, void *data)
{
	struct qed_mfw_tlv_eth *etlv = data;
	struct qede_dev *edev = dev;
	struct qede_fastpath *fp;
	int i;

	etlv->lso_maxoff_size = 0XFFFF;
	etlv->lso_maxoff_size_set = true;
	etlv->lso_minseg_size = (u16)ETH_TX_LSO_WINDOW_MIN_LEN;
	etlv->lso_minseg_size_set = true;
	if (edev->ndev->flags & IFF_PROMISC)
		etlv->prom_mode = 1;
	etlv->prom_mode_set = true;
	etlv->tx_descr_size = QEDE_BASE_TSS_COUNT(edev);
	etlv->tx_descr_size_set = true;
	etlv->rx_descr_size = QEDE_BASE_RSS_COUNT(edev);
	etlv->rx_descr_size_set = true;
	etlv->iov_offload = QED_MFW_TLV_IOV_OFFLOAD_VEB;
	etlv->iov_offload_set = true;

	/* Fill information regarding queues; Should be done under the qede
	 * lock to guarantee those don't change beneath our feet.
	 */
	etlv->txqs_empty = true;
	etlv->rxqs_empty = true;
	etlv->num_txqs_full = 0;
	etlv->num_rxqs_full = 0;

	__qede_lock(edev);
	for_each_queue(i) {
		fp = &edev->fp_array[i];
		if (fp->type & QEDE_FASTPATH_TX) {
			struct qede_tx_queue *txq = QEDE_FP_TC0_TXQ(fp);

			if (txq->sw_tx_cons != txq->sw_tx_prod)
				etlv->txqs_empty = false;
			if (qede_is_txq_full(edev, txq))
				etlv->num_txqs_full++;
		}
		if (fp->type & QEDE_FASTPATH_RX) {
			if (qede_has_rx_work(fp->rxq))
				etlv->rxqs_empty = false;

			/* This on is a bit tricky; Firmware might stop
			 * placing packets if ring is not yet full.
			 * Give an approximation.
			 */
			if (le16_to_cpu(*fp->rxq->hw_cons_ptr) -
			    qed_chain_get_cons_idx(&fp->rxq->rx_comp_ring) >
			    RX_RING_SIZE - 100)
				etlv->num_rxqs_full++;
		}
	}
	__qede_unlock(edev);

	etlv->txqs_empty_set = true;
	etlv->rxqs_empty_set = true;
	etlv->num_txqs_full_set = true;
	etlv->num_rxqs_full_set = true;
}

/* We are here because one of the L2 forwarding offload station is
 * added or removed or queue count of base device is changed which results in
 * total queue count change.
 * This requires reinitialization of L2 forwarding offload related
 * elements to readjust the resource assignments such as vport_id,
 * base_queue_id etc for each L2 forwarding offload instance to avoid
 * any holes in resource allocation.
 */
static void qede_reinit_fwd_dev_info(struct qede_dev *edev)
{
	struct qede_fwd_dev *fwd_dev = NULL;

	edev->req_fwd_dev_queues = 0;
	edev->vport_id_used = 0;
	edev->num_fwd_devs = 0;

	list_for_each_entry(fwd_dev, &edev->fwd_dev_list, list) {
		edev->num_fwd_devs++;
		edev->vport_id_used++;
		fwd_dev->vport_id = edev->vport_id_used;
		fwd_dev->base_queue_id = edev->req_fwd_dev_queues +
					 edev->base_num_queues;
		edev->req_fwd_dev_queues += fwd_dev->num_queues;
	}
}

#if HAS_NDO(DFWD_ADD_STATION) /* QEDE_UPSTREAM */
static void _qede_fwd_add_station(struct qede_dev *edev,
				  struct qede_reload_args *args)
{
	struct qede_fwd_dev *fwd_dev = args->u.fwd_dev;

	list_add_tail(&fwd_dev->list, &edev->fwd_dev_list);

	edev->num_fwd_devs++;

	/* work with TC = 1 */
	if (edev->num_fwd_devs == 1) {
		edev->dev_info.num_tc = 1;
		netdev_reset_tc(edev->ndev);
		netdev_set_num_tc(edev->ndev, edev->dev_info.num_tc);
	}

}

/* Although HW is capable of performing RSS on L2 forwarding instance,
 * restrict max queues to 1 for simplicity.
 * RSS feature will be added later.
 */
#define QEDE_MAX_QUEUES_FOR_L2_FWD 1
#define QEDE_MAX_L2_FWD_OFFLOAD_INSTANCES 64

static void *qede_fwd_add_station(struct net_device *rdev,
				  struct net_device *udev)
{
	struct qede_fwd_dev *fwd_dev;
	struct qede_reload_args args;
	struct qede_dev *edev;

	edev = netdev_priv(rdev);

	DP_INFO(edev, "Adding L2 forwarding offload station for %s\n",
		udev->name);

	if (edev->num_vfs) {
		DP_INFO(edev, "SR-IOV is enabled, can't add station\n");
		return ERR_PTR(-EBUSY);
	}

	if (edev->xdp_prog) {
		DP_INFO(edev, "XDP is enabled, can not add station\n");
		return ERR_PTR(-EBUSY);
	}

	if (edev->num_fwd_devs >= QEDE_MAX_L2_FWD_OFFLOAD_INSTANCES) {
		DP_ERR(edev,
		       "Can't add more than %d L2 forwarding stations. Existing station count = %d\n",
		       QEDE_MAX_L2_FWD_OFFLOAD_INSTANCES, edev->num_fwd_devs);
		return ERR_PTR(-EINVAL);
	}

	if (udev->num_rx_queues > QEDE_MAX_QUEUES_FOR_L2_FWD) {
		DP_ERR(edev,
		       "Can't allow more than %d queues per upper device. Requested queues = %d\n",
		       QEDE_MAX_QUEUES_FOR_L2_FWD, udev->num_rx_queues);
		return ERR_PTR(-EINVAL);
	}

	/* HW queues for L2 forwarding station are of type - combined */
	if (udev->num_rx_queues != udev->num_tx_queues) {
		DP_ERR(edev,
		       "Assymetric number of Rx (%d) and Tx (%d) queues are not supported\n",
		       udev->num_rx_queues, udev->num_tx_queues);
		return ERR_PTR(-EINVAL);
	}

	if ((edev->vport_id_used + 1) >= edev->dev_info.num_vports) {
		DP_ERR(edev,
		       "Ran out of vports (%d), can not add L2 forwarding stations anymore\n",
		       edev->dev_info.num_vports);
		return ERR_PTR(-EBUSY);
	}

	if ((QEDE_QUEUE_CNT(edev) + udev->num_rx_queues) >
	    QEDE_MAX_QUEUE_CNT(edev)) {
		DP_ERR(edev, "Not enough HW queues available\n");
		return ERR_PTR(-EBUSY);
	}

	fwd_dev = kzalloc(sizeof(*fwd_dev), GFP_KERNEL);
	if (!fwd_dev)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&fwd_dev->list);
	fwd_dev->edev = edev;
	fwd_dev->upper_ndev = udev;
	fwd_dev->num_queues = udev->num_rx_queues;

	memset(&args, 0, sizeof(args));
	args.func = _qede_fwd_add_station;
	args.u.fwd_dev = fwd_dev;
	args.flags = QEDE_UPDATE_FP_ELEM | QEDE_UPDATE_FP_BUFFERS;

	/* Allocate HW fastpath resources through reload flow */
	qede_reload(edev, &args);

	return fwd_dev;
}

static void _qede_fwd_del_station(struct qede_dev *edev,
				  struct qede_reload_args *args)
{
	struct qede_fwd_dev *fwd_dev = args->u.fwd_dev;

	/* delete the node from list */
	list_del(&fwd_dev->list);
	kfree(fwd_dev);

	if (list_empty(&edev->fwd_dev_list)) {
		edev->req_fwd_dev_queues = 0;
		edev->vport_id_used = 0;
		edev->num_fwd_devs = 0;
	}
}

static void qede_fwd_del_station(struct net_device *real_dev, void *priv)
{
	struct qede_fwd_dev *fwd_dev = priv;
	struct qede_dev *edev = fwd_dev->edev;
	struct qede_reload_args args;

	if (list_empty(&edev->fwd_dev_list)) {
		DP_ERR(edev,
		       "%s is called but we don't have any L2 forwarding offload station enabled\n",
		       __func__);
		return;
	}

	memset(&args, 0, sizeof(args));
	args.func = _qede_fwd_del_station;
	args.u.fwd_dev = fwd_dev;
	args.flags = QEDE_UPDATE_FP_ELEM | QEDE_UPDATE_FP_BUFFERS;
	/* Update HW fastpath queue count throgh reload flow */
	qede_reload(edev, &args);
}
#endif /* _HAS_NDO_DFWD_ADD_STATION */

#ifdef CONFIG_DEBUG_FS
static struct dentry *qede_dbgfs_dir;
#define CHAIN_PRINT_DONE 200

static int qede_chain_print_element(struct qed_chain *p_chain, void *p_element,
				    char *buffer)
{
	/* this will be a service function for the per chain print element
	 * function.
	 */
	int pos = 0, length, elem_size = p_chain->elem_size;

	/* print element byte by byte */
	while (elem_size > 0) {
		length = sprintf(buffer + pos, " %02x", *(u8 *)p_element);
		if (length < 0) {
			pr_notice("Failed to copy data to buffer\n");
			return length;
		}
		pos += length;
		elem_size--;
		p_element++;
	}

	length = sprintf(buffer + pos, "\n");
	if (length < 0) {
		pr_notice("Failed to copy data to buffer\n");
		return length;
	}

	pos += length;

	return pos;
}

static int qede_chain_print_metadata(struct qed_chain *p_chain, char *buffer)
{
	int pos = 0, length;

	length = sprintf(buffer, "prod 0x%x [%04d], cons 0x%x [%04d]\n",
			 qed_chain_get_prod_idx(p_chain),
			 qed_chain_get_prod_idx(p_chain) & 0xfff,
			 qed_chain_get_cons_idx(p_chain),
			 qed_chain_get_cons_idx(p_chain) & 0xfff);
	if (length < 0) {
		pr_notice("Failed to copy Metadata to buffer\n");
		return length;
	}

	pos += length;
	length = sprintf(buffer + pos, "Chain capacity: %d, Chain size: %d\n",
			 p_chain->capacity, p_chain->size);
	if (length < 0) {
		pr_notice("Failed to copy Metadata to buffer\n");
		return length;
	}

	pos += length;

	return pos;
}

static void qede_fill_key_list(struct qede_dev *edev, char *key_list,
			       int *key_list_len)
{
	struct qede_fastpath *fp;
	int pos = 0, len;
	int qid, tc;

	if (!edev->fp_array)
		return;

	for_each_queue(qid) {
		fp = &edev->fp_array[qid];

		if (fp->type & QEDE_FASTPATH_TX) {
			for_each_cos_in_txq(edev, tc) {
				len = sprintf(key_list + pos, "txq%d_tc%1d ",
					      fp->txq[tc].index, tc);
				if (len < 0) {
					pr_notice("Failed to get Tx queues\n");
					return;
				}
				pos += len;
			}
		}

		if (fp->type & QEDE_FASTPATH_RX) {
			len = sprintf(key_list + pos, "rxq%d_bd rxq%d_cqe\n",
				      fp->rxq->rxq_id, fp->rxq->rxq_id);
			if (len < 0) {
				pr_notice("Failed to get Rx queues\n");
				return;
			}
			pos += len;
		}
	}

	*key_list_len = pos;
}

#define MAX_TXQ_STR_LEN 10 /* strlen("txq%2d_tc%1d ") */
#define MAX_RXQ_STR_LEN 20 /* strlen("rxq%2d_tc%1d\n") */

static ssize_t qede_dbg_chain_print_cmd_read(struct file *filp,
					     char __user *buffer,
					     size_t count, loff_t *ppos)
{
	struct qede_dev *edev = filp->private_data;
	char key_list_def[] = "No queues exist\n";
	int key_list_max_len, key_list_len = 0;
	char *key_list = NULL;
	int len = 0, rc = 0;
	char *buf;

	if (!edev)
		return 0;

	if (!edev->chain_info.b_key_entered) {
		/* Print key_list only in case there are no reminders from
		 * last ring dump.
		 */
		if (*ppos > 0)
			return 0;

		key_list_max_len = QEDE_QUEUE_CNT(edev) *
				   (edev->dev_info.num_tc *
				    MAX_TXQ_STR_LEN + MAX_RXQ_STR_LEN);
		key_list = kmalloc(sizeof(char) * key_list_max_len,
				   GFP_KERNEL);
		if (!key_list) {
			pr_notice("allocating key_list buffer failed\n");
			return 0;
		}

		qede_fill_key_list(edev, key_list, &key_list_len);

		if (key_list_len)
			len = simple_read_from_buffer(buffer, count, ppos,
						      key_list, key_list_len);
		else
			len = simple_read_from_buffer(buffer, count, ppos,
						      key_list_def,
						      strlen(key_list_def));

		kfree(key_list);
		return len;
	} else {
		buf = kmalloc(sizeof(char) * count, GFP_KERNEL);
		if (!buf) {
			pr_notice("allocating buffer failed\n");
			return 0;
		}

		rc = edev->ops->common->chain_print(edev->chain_info.chain, buf,
						    count, &edev->chain_info.current_index,
						    edev->chain_info.final_index,
						    edev->chain_info.print_metadata, false,
						    qede_chain_print_element,
						    qede_chain_print_metadata);
		if (rc < 0) {
			DP_ERR(edev, "printing chain to buffer failed\n");
			return 0;
		}

		len = simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
		edev->chain_info.print_metadata = false;
		kfree(buf);
		if (rc == CHAIN_PRINT_DONE)
			*ppos -= len;
		else
			edev->chain_info.b_key_entered = false;
		return len;
	}
}

static int qede_get_params(char *buf, int *is_tx, int *queue_idx, int *tc,
			   int *is_bd)
{
	int args;

	if (!strncmp(buf, "txq", strlen("txq")))
		*is_tx = true;
	else if (!strncmp(buf, "rxq", strlen("rxq")))
		*is_tx = false;
	else
		return -1;

	if (*is_tx) {
		args = sscanf(buf, "txq%d_tc%d", queue_idx, tc);
		if (args != 2)
			return -1;
	} else {
		char rx_type[4];

		args = sscanf(buf, "rxq%d_%3s", queue_idx, rx_type);
		if (args != 2)
			return -1;

		if (!strncmp(rx_type, "bd", strlen("bd")))
			*is_bd = true;
		else if (!strncmp(rx_type, "cqe", strlen("cqe")))
			*is_bd = false;
		else
			return -1;
	}

	return 0;
}

static struct qed_chain *qede_get_tx_chain(struct qede_dev *edev,
					   int queue_idx, u8 tc)
{
	struct qede_fastpath *fp;
	int qid;

	if (!edev->fp_array)
		return NULL;

	if (tc >= edev->dev_info.num_tc)
		return NULL;

	for_each_queue(qid) {
		fp = &edev->fp_array[qid];

		if (!(fp->type & QEDE_FASTPATH_TX))
			continue;

		if (fp->txq[tc].index == queue_idx)
			return &fp->txq[tc].tx_pbl;
	}

	return NULL;
}

static struct qed_chain *qede_get_rx_chain(struct qede_dev *edev,
					   int queue_idx, bool is_bd)
{
	struct qede_fastpath *fp;
	int qid;

	if (!edev->fp_array)
		return NULL;

	for_each_queue(qid) {
		fp = &edev->fp_array[qid];

		if (!(fp->type & QEDE_FASTPATH_RX))
			continue;

		if (fp->rxq->rxq_id == queue_idx)
			return is_bd ? &fp->rxq->rx_bd_ring :
				       &fp->rxq->rx_comp_ring;
	}

	return NULL;
}

static ssize_t qede_dbg_chain_print_cmd_write(struct file *filp,
					      const char __user *buffer,
					      size_t count, loff_t *ppos)
{
	/* prepare list element */
	struct qede_dev *edev = filp->private_data;
	struct qed_chain *chain = NULL;
	int len, queue_idx, is_tx, rc;
	int tc = -1, is_bd = 0;
	char *buf;

	buf = kmalloc(count + 1, GFP_ATOMIC); /* +1 for '\0' */
	if (!buf) {
		pr_notice("allocating buffer failed\n");
		goto error;
	}

	if (!edev)
		goto error;

	len = simple_write_to_buffer(buf, count, ppos, buffer, count);
	if (len < 0) {
		pr_notice("copy from user failed\n");
		goto error;
	}

	/* Get rid of an extra space for later parsing */
	buf[len-1] = '\0';

	rc = qede_get_params(buf, &is_tx, &queue_idx, &tc, &is_bd);
	if (rc) {
		pr_notice("Invalid input - should be txq<qid>_tc<tc_num> or rxq<qid>_cqe or rxq<qid>_bd (as seen in ethtool -S)\n");
		goto error;
	}

	if (is_tx)
		chain = qede_get_tx_chain(edev, queue_idx, (u8)tc);
	else
		chain = qede_get_rx_chain(edev, queue_idx, (bool)is_bd);

	if (!chain) {
		pr_notice("Requested chain doesn't exist\n");
		goto error;
	}

	edev->chain_info.chain = chain;
	edev->chain_info.current_index = 0;
	edev->chain_info.b_key_entered = true;
	edev->chain_info.print_metadata = true;
	edev->chain_info.final_index = chain->capacity;

	kfree(buf);
	return len;

error:
	kfree(buf);
	pr_notice("due to the error that occurred chain print will not work\n");
	return count;
}

static ssize_t qede_dbg_tx_timeout_cmd_read(struct file *filp,
					    char __user *buffer,
					    size_t count, loff_t *ppos)
{
	struct qede_dev *edev = filp->private_data;
	int len = 0;
	char key_on[] = "on\n";
	char key_off[] = "off\n";

	if (!edev)
		return 0;

	if (!edev->gen_tx_timeout)
		len = simple_read_from_buffer(buffer, count, ppos, key_off,
					      strlen(key_off));
	else
		len = simple_read_from_buffer(buffer, count, ppos, key_on,
					      strlen(key_on));

	return len;
}

static ssize_t qede_dbg_tx_timeout_cmd_write(struct file *filp,
					     const char __user *buffer,
					     size_t count, loff_t *ppos)
{
	struct qede_dev *edev = filp->private_data;
	char *buf;
	int len;

	buf = kmalloc(count + 1, GFP_ATOMIC); /* +1 for '\0' */
	if (!buf) {
		pr_notice("allocating buffer failed\n");
		goto error;
	}

	if (!edev)
		goto error;

	len = simple_write_to_buffer(buf, count, ppos, buffer, count);
	if (len < 0) {
		pr_notice("copy from user failed\n");
		goto error;
	}

	buf[len] = '\0';

	if (!strncmp(buf, "on", 2)) {
		edev->gen_tx_timeout = true;
		edev->ndev->netdev_ops = &qede_netdev_ops_tx_timeout;
	} else if (!strncmp(buf, "off", 3)) {
		edev->gen_tx_timeout = false;
		edev->ndev->netdev_ops = &qede_netdev_ops;
	}

	kfree(buf);
	return len;

error:
	kfree(buf);
	pr_notice("due to the error that occurred tx timeout will not work\n");
	return count;
}

static ssize_t
qede_dbg_watchdog_timeo_cmd_read(struct file *filp, char __user *buffer,
				 size_t count, loff_t *ppos)
{
	struct qede_dev *edev = filp->private_data;
	char data[4]; /* 2 for "\0" and "\n' and 2 for watchdog_timeo value */
	int len;

	if (!edev)
		return -EPERM;

	snprintf(data, sizeof(data), "%d\n", edev->ndev->watchdog_timeo / HZ);

	len = simple_read_from_buffer(buffer, count, ppos, data, strlen(data));

	return len;
}

static ssize_t
qede_dbg_watchdog_timeo_cmd_write(struct file *filp, const char __user *buffer,
				  size_t count, loff_t *ppos)
{
	struct qede_dev *edev = filp->private_data;
	int len, args, timeo;
	char *buf;

	if (!edev)
		return -EPERM;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf) {
		pr_notice("allocating buffer failed\n");
		goto error;
	}

	len = simple_write_to_buffer(buf, count, ppos, buffer, count);
	if (len < 0) {
		pr_notice("copy from user failed\n");
		goto error;
	}

	args = sscanf(buf, "%d", &timeo);
	if (args != 1) {
		pr_notice("Invalid watchdog timeout argument\n");
		goto error;
	}

	/* watchdog_timeo range - min:3 max:60 */
	if ((timeo > 60) || (timeo < 3)) {
		pr_notice("Invalid watchdog timeout value %d, range is [3-60]\n", timeo);
		goto error;
	}

	edev->ndev->watchdog_timeo = timeo * HZ;
	kfree(buf);
	return len;
error:
	kfree(buf);
	return -EINVAL;
}

/*** prepare file operations structures for debug features ***/

static const struct file_operations chain_print_fops = {
	.owner  = THIS_MODULE,
	.open   = simple_open,
	.read   = qede_dbg_chain_print_cmd_read,
	.write  = qede_dbg_chain_print_cmd_write,
};

static const struct file_operations tx_timeout_fops = {
	.owner  = THIS_MODULE,
	.open   = simple_open,
	.read   = qede_dbg_tx_timeout_cmd_read,
	.write  = qede_dbg_tx_timeout_cmd_write,
};

static const struct file_operations watchdog_timeo_fops = {
	.owner  = THIS_MODULE,
	.open   = simple_open,
	.read   = qede_dbg_watchdog_timeo_cmd_read,
	.write  = qede_dbg_watchdog_timeo_cmd_write,
};

static void qede_init_debugfs(void)
{
	/* create base directory */
	qede_dbgfs_dir = debugfs_create_dir("qede", NULL);
	if (!qede_dbgfs_dir)
		pr_notice("Qede node failed to create\n");
}

static void qede_debugfs_add_features(struct qede_dev *edev)
{
	char pf_dirname[9]; /* e.g. 00:04:00 +1 null termination */

	if (!qede_dbgfs_dir)
		return;

	/* create pf dir */
	sprintf(pf_dirname, "%02x:%02x.%x", edev->pdev->bus->number,
		PCI_SLOT(edev->pdev->devfn), PCI_FUNC(edev->pdev->devfn));
	edev->bdf_dentry = debugfs_create_dir(pf_dirname, qede_dbgfs_dir);
	if (!edev->bdf_dentry)
		return;

	if (!debugfs_create_file("chain_print", 0600, edev->bdf_dentry, edev,
							 &chain_print_fops))
		goto err;

	if (!debugfs_create_file("tx_timeout", 0600, edev->bdf_dentry, edev,
				 &tx_timeout_fops))
		goto err;

	if (!debugfs_create_file("watchdog_timeo", 0600, edev->bdf_dentry, edev,
				 &watchdog_timeo_fops))
		goto err;

#ifdef TIME_FP_DEBUG /* ! QEDE_UPSTREAM */
	if (!debugfs_create_file("napi_time", 0600, edev->bdf_dentry, edev,
				 &qede_time_fops))
		goto err;
#endif

	return;
err:
	debugfs_remove_recursive(edev->bdf_dentry);
	edev->bdf_dentry = NULL;
}

static void qede_remove_debugfs(void)
{
	debugfs_remove_recursive(qede_dbgfs_dir);
	qede_dbgfs_dir = NULL;
}
#else /* CONFIG_DEBUG_FS */
static void qede_init_debugfs(void) {}
static void qede_remove_debugfs(void) {}
static void qede_debugfs_add_features(struct qede_dev *edev) {}
#endif /* CONFIG_DEBUG_FS */

/**
 * qede_io_error_detected - called when PCI error is detected
 * @pdev: Pointer to PCI device
 * @state: The current pci connection state
 *
 * This function is called after a PCI bus error affecting
 * this device has been detected.
 */
static pci_ers_result_t
qede_io_error_detected(struct pci_dev *pdev, pci_channel_state_t state)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct qede_dev *edev = netdev_priv(dev);
	pci_ers_result_t rc = PCI_ERS_RESULT_NEED_RESET;
	u32 curr_state;

	if (!edev)
		return PCI_ERS_RESULT_NONE;

	curr_state = edev->state;

	DP_NOTICE(edev, "IO error detected [%d]\n", state);

	__qede_lock(edev);

	if (edev->state == QEDE_STATE_RECOVERY || edev->aer_recov_prog) {
		DP_ERR(edev, "Recovery in progress\n");
		rc = PCI_ERS_RESULT_NONE;
		goto exit;
	}

	/* PF handles the recovery of its VFs */
	if (IS_VF(edev)) {
		DP_VERBOSE(edev, QED_MSG_IOV,
			   "VF recovery is handled by its PF\n");
		rc = PCI_ERS_RESULT_RECOVERED;
		goto exit;
	}

	if (edev->ops->common->is_hot_reset_occured_or_in_prgs(edev->cdev)) {
		/* Close OS Tx */
		qede_close_os_tx(edev);

		set_bit(QEDE_SP_AER, &edev->sp_flags);
		schedule_delayed_work(&edev->sp_task, 0);

		rc = PCI_ERS_RESULT_CAN_RECOVER;
		goto exit;
	} else {
		edev->ops->common->set_aer_state(edev->cdev, true);
		edev->aer_recov_prog = true;
		edev->pre_aer_state = curr_state;
		edev->state = QEDE_STATE_RECOVERY;
		edev->ops->common->recovery_prolog(edev->cdev);

		if (curr_state == QEDE_STATE_OPEN) {
			edev->ops->common->set_power_state(edev->cdev, PCI_D0);
			DP_VERBOSE(edev, NETIF_MSG_LINK, "Performing driver unload\n");
			qede_unload(edev, QEDE_UPDATE_RECOVERY, true);
		}

		__qede_remove(edev->pdev, QEDE_REMOVE_RESET);

		if (state == pci_channel_io_perm_failure) {
			DP_NOTICE(edev, "AER : pci channel state is io_perm_failure\n");
			rc = PCI_ERS_RESULT_DISCONNECT;
			goto exit;
		}
	}

exit:
	__qede_unlock(edev);
	return rc;
}

static void qede_restore_save_pci_state(struct qede_dev *edev)
{
	pci_restore_state(edev->pdev);
	pci_save_state(edev->pdev);
}

static pci_ers_result_t qede_io_slot_reset(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct qede_dev *edev = netdev_priv(dev);
	int rc;

	if (!edev)
		return PCI_ERS_RESULT_NONE;

	DP_NOTICE(edev, "IO slot reset initializing...\n");

	if (!edev->aer_recov_prog)
		return PCI_ERS_RESULT_RECOVERED;

	rc = pci_enable_device(pdev);
	if (rc) {
		DP_NOTICE(edev, "Cannot enable PCI device after reset\n");
		return PCI_ERS_RESULT_NONE;
	}

	pci_set_master(pdev);
	qede_restore_save_pci_state(edev);

#ifdef _HAS_CLEANUP_AER_ERROR /* ! QEDE_UPSTREAM */
	/* Perform clenup of the PCIe registers */
	if (pci_cleanup_aer_uncorrect_error_status(pdev))
		DP_ERR(edev, "pci_cleanup_aer_uncorrect_error_status failed\n");
	else
		DP_ERR(edev, "pci_cleanup_aer_uncorrect_error_status succeeded\n");
#endif

	edev->is_slot_reset_done = true;

	return PCI_ERS_RESULT_RECOVERED;
}

static void qede_io_resume(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct qede_dev *edev = netdev_priv(dev);
	int rc;

	if (!edev)
		return;

	DP_NOTICE(edev, "IO Resume...\n");

	if (!edev->aer_recov_prog)
		return;

	/* In certain configurations, driver dosen't receive slot_reset call. In
	 * such scenarios, driver need to restore and save PCI channel's state.
	 */
	if (!edev->is_slot_reset_done)
		qede_restore_save_pci_state(edev);

	qede_lock(edev);

	rc = __qede_probe(edev->pdev, edev->dp_module, edev->dp_level,
			  IS_VF(edev), QEDE_PROBE_RECOVERY);
	if (rc) {
		DP_ERR(edev, "Probe failed %d\n", rc);
		edev->cdev = NULL;
		goto exit;
	}

	if (edev->pre_aer_state == QEDE_STATE_OPEN) {
		rc = qede_load(edev, QEDE_UPDATE_RECOVERY, true);
		if (rc) {
			DP_ERR(edev, "Load failed %d\n", rc);
			goto exit;
		}

		qede_config_rx_mode_for_all(edev);
#if HAS_NDO(UDP_TUNNEL_CONFIG) /* QEDE_UPSTREAM */
		udp_tunnel_get_rx_info(edev->ndev);
#else
#ifdef _HAS_ADD_VXLAN_PORT
		vxlan_get_rx_port(edev->ndev);
#endif
#ifdef _HAS_ADD_GENEVE_PORT
		geneve_get_rx_port(edev->ndev);
#endif
#endif
	}

	edev->state = edev->pre_aer_state;
	edev->err_flags = 0;

exit:
	edev->is_slot_reset_done = false;
	edev->aer_recov_prog = false;

	qede_unlock(edev);

	return;
}

#ifdef SYS_INC_RESET_PREP
static void qede_reset_failed(struct qede_dev *edev)
{
	netdev_err(edev->ndev, "Reset failed.Power cycle is needed\n");

	netif_device_detach(edev->ndev);

	if (edev->cdev)
		edev->ops->common->set_power_state(edev->cdev, PCI_D3hot);
}

static void qede_reset_prepare(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	struct qede_dev *edev = netdev_priv(ndev);

	if (!edev)
		return;

	DP_NOTICE(edev, " User initiated FLR in progress...\n");

	if (edev->state == QEDE_STATE_RECOVERY) {
		DP_NOTICE(edev, "Device already in the recovery state\n");
		return;
	}

	/* PF handles the recovery of its VFs */
	if (IS_VF(edev)) {
		DP_VERBOSE(edev, QED_MSG_IOV, "VF recovery handled by its PF\n");
		return;
	}

	if (edev->num_vfs)
		qede_sriov_configure(edev->pdev, 0);

	qede_lock(edev);

	if (netif_running(ndev))
		qede_unload(edev, QEDE_UPDATE_COMPLETE, true);

	edev->ops->common->update_drv_state(edev->cdev, false);
	__qede_remove(edev->pdev, QEDE_REMOVE_RESET);

	qede_unlock(edev);
}

static void qede_reset_done(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	struct qede_dev *edev = netdev_priv(ndev);
	int rc;

	if (!edev)
		return;

	qede_lock(edev);

	rc = __qede_probe(edev->pdev, edev->dp_module, edev->dp_level,
			  IS_VF(edev), QEDE_PROBE_RESET);
	if (rc) {
		edev->cdev = NULL;
		goto err;
	}

	if (netif_running(ndev)) {
		qede_load(edev, QEDE_UPDATE_COMPLETE, true);
		qede_config_rx_mode_for_all(edev);
#if HAS_NDO(UDP_TUNNEL_CONFIG) /* QEDE_UPSTREAM */
		udp_tunnel_get_rx_info(ndev);
#else
#ifdef _HAS_ADD_VXLAN_PORT
		vxlan_get_rx_port(ndev);
#endif
#ifdef _HAS_ADD_GENEVE_PORT
		geneve_get_rx_port(ndev);
#endif
#endif
		edev->ops->common->update_drv_state(edev->cdev, true);
	}

	qede_unlock(edev);

	DP_NOTICE(edev, "Reset handling is done\n");

	return;
err:
	qede_reset_failed(edev);
}
#endif
