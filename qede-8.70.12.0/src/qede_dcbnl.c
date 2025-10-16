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

#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <net/dcbnl.h>
#include "qede.h"

static u8 qede_dcbnl_getstate(struct net_device *netdev)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->getstate(edev->cdev);
}

static u8 qede_dcbnl_setstate(struct net_device *netdev, u8 state)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->setstate(edev->cdev, state);
}

static void qede_dcbnl_getpermhwaddr(struct net_device *netdev, u8 *perm_addr)
{
	memcpy(perm_addr, netdev->dev_addr, netdev->addr_len);
}

static void qede_dcbnl_getpgtccfgtx(struct net_device *netdev, int prio,
				    u8 *prio_type, u8 *pgid, u8 *bw_pct,
				    u8 *up_map)
{
	struct qede_dev *edev = netdev_priv(netdev);

	edev->ops->common->dcb->getpgtccfgtx(edev->cdev, prio, prio_type,
				     pgid, bw_pct, up_map);
}

static void qede_dcbnl_getpgbwgcfgtx(struct net_device *netdev,
				     int pgid, u8 *bw_pct)
{
	struct qede_dev *edev = netdev_priv(netdev);

	edev->ops->common->dcb->getpgbwgcfgtx(edev->cdev, pgid, bw_pct);
}

static void qede_dcbnl_getpgtccfgrx(struct net_device *netdev, int prio,
				    u8 *prio_type, u8 *pgid, u8 *bw_pct,
				    u8 *up_map)
{
	struct qede_dev *edev = netdev_priv(netdev);

	edev->ops->common->dcb->getpgtccfgrx(edev->cdev, prio, prio_type, pgid, bw_pct,
				     up_map);
}

static void qede_dcbnl_getpgbwgcfgrx(struct net_device *netdev,
				     int pgid, u8 *bw_pct)
{
	struct qede_dev *edev = netdev_priv(netdev);

	edev->ops->common->dcb->getpgbwgcfgrx(edev->cdev, pgid, bw_pct);
}

static void qede_dcbnl_getpfccfg(struct net_device *netdev, int prio,
				 u8 *setting)
{
	struct qede_dev *edev = netdev_priv(netdev);

	edev->ops->common->dcb->getpfccfg(edev->cdev, prio, setting);
}

static void qede_dcbnl_setpfccfg(struct net_device *netdev, int prio,
				 u8 setting)
{
	struct qede_dev *edev = netdev_priv(netdev);

	edev->ops->common->dcb->setpfccfg(edev->cdev, prio, setting);
}

static u8 qede_dcbnl_getcap(struct net_device *netdev, int capid, u8 *cap)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->getcap(edev->cdev, capid, cap);
}

#if defined(_GETNUMTCS_RETURNS_INT) /* QEDE_UPSTREAM */
static int qede_dcbnl_getnumtcs(struct net_device *netdev, int tcid, u8 *num)
#else
static u8 qede_dcbnl_getnumtcs(struct net_device *netdev, int tcid, u8 *num)
#endif
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->getnumtcs(edev->cdev, tcid, num);
}

static u8 qede_dcbnl_getpfcstate(struct net_device *netdev)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->getpfcstate(edev->cdev);
}

#if defined(_GETAPP_RETURNS_INT) /* QEDE_UPSTREAM */
static int qede_dcbnl_getapp(struct net_device *netdev, u8 idtype, u16 id)
#else
static u8 qede_dcbnl_getapp(struct net_device *netdev, u8 idtype, u16 id)
#endif
{
#ifdef CONFIG_DCB
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->getapp(edev->cdev, idtype, id);
#else
	return -EINVAL;
#endif
}

static u8 qede_dcbnl_getdcbx(struct net_device *netdev)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->getdcbx(edev->cdev); 
}

static void qede_dcbnl_setpgtccfgtx(struct net_device *netdev, int prio,
				    u8 pri_type, u8 pgid, u8 bw_pct, u8 up_map)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->setpgtccfgtx(edev->cdev, prio, pri_type, pgid,
					    bw_pct, up_map);
}

static void qede_dcbnl_setpgtccfgrx(struct net_device *netdev, int prio,
				    u8 pri_type, u8 pgid, u8 bw_pct, u8 up_map)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->setpgtccfgrx(edev->cdev, prio, pri_type, pgid,
					    bw_pct, up_map);
}

static void qede_dcbnl_setpgbwgcfgtx(struct net_device *netdev, int pgid,
				     u8 bw_pct)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->setpgbwgcfgtx(edev->cdev, pgid, bw_pct);
}

static void qede_dcbnl_setpgbwgcfgrx(struct net_device *netdev, int pgid,
				     u8 bw_pct)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->setpgbwgcfgrx(edev->cdev, pgid, bw_pct);
}

static u8 qede_dcbnl_setall(struct net_device *netdev)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->setall(edev->cdev);
}

#if defined(_GETNUMTCS_RETURNS_INT) /* QEDE_UPSTREAM */
static int qede_dcbnl_setnumtcs(struct net_device *netdev, int tcid, u8 num)
#else
static u8 qede_dcbnl_setnumtcs(struct net_device *netdev, int tcid, u8 num)
#endif
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->setnumtcs(edev->cdev, tcid, num);
}

static void qede_dcbnl_setpfcstate(struct net_device *netdev, u8 state)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->setpfcstate(edev->cdev, state);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)) || SLES_STARTING_AT_VERSION(SLES12_SP1) /* QEDE_UPSTREAM */
static int qede_dcbnl_setapp(struct net_device *netdev, u8 idtype, u16 idval,
			     u8 up)
#else
static u8 qede_dcbnl_setapp(struct net_device *netdev, u8 idtype, u16 idval,
			     u8 up)
#endif
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->setapp(edev->cdev, idtype, idval, up);
}

static u8 qede_dcbnl_setdcbx(struct net_device *netdev, u8 state)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->setdcbx(edev->cdev, state);
}

static u8 qede_dcbnl_getfeatcfg(struct net_device *netdev, int featid,
				u8 *flags)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->getfeatcfg(edev->cdev, featid, flags);
}

static u8 qede_dcbnl_setfeatcfg(struct net_device *netdev, int featid, u8 flags)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->setfeatcfg(edev->cdev, featid, flags);
}

#ifdef DCB_CEE_SUPPORT /* QEDE_UPSTREAM */
static int qede_dcbnl_peer_getappinfo(struct net_device *netdev,
				      struct dcb_peer_app_info *info,
				      u16 *count)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->peer_getappinfo(edev->cdev, info, count);
}

static int qede_dcbnl_peer_getapptable(struct net_device *netdev,
				       struct dcb_app *app)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->peer_getapptable(edev->cdev, app);
}

static int qede_dcbnl_cee_peer_getpfc(struct net_device *netdev,
				      struct cee_pfc *pfc)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->cee_peer_getpfc(edev->cdev, pfc);
}

static int qede_dcbnl_cee_peer_getpg(struct net_device *netdev,
				     struct cee_pg *pg)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->cee_peer_getpg(edev->cdev, pg);
}
#endif

static int qede_dcbnl_ieee_getpfc(struct net_device *netdev,
				  struct ieee_pfc *pfc)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->ieee_getpfc(edev->cdev, pfc);
}

static int qede_dcbnl_ieee_setpfc(struct net_device *netdev,
				  struct ieee_pfc *pfc)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->ieee_setpfc(edev->cdev, pfc);
}

static int qede_dcbnl_ieee_getets(struct net_device *netdev,
				  struct ieee_ets *ets)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->ieee_getets(edev->cdev, ets);
}

static int qede_dcbnl_ieee_setets(struct net_device *netdev,
				  struct ieee_ets *ets)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->ieee_setets(edev->cdev, ets);
}

#ifdef _IEEE_8021QAZ_APP /* QEDE_UPSTREAM */
static int qede_dcbnl_ieee_getapp(struct net_device *netdev,
				  struct dcb_app *app)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->ieee_getapp(edev->cdev, app);
}

static int qede_dcbnl_ieee_setapp(struct net_device *netdev,
				  struct dcb_app *app)
{
	struct qede_dev *edev = netdev_priv(netdev);
	int err;

	err = dcb_ieee_setapp(netdev, app);
	if (err)
		return err;

	return edev->ops->common->dcb->ieee_setapp(edev->cdev, app);
}
#endif

#ifdef DCB_CEE_SUPPORT /* QEDE_UPSTREAM */
static int qede_dcbnl_ieee_peer_getpfc(struct net_device *netdev,
				       struct ieee_pfc *pfc)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->ieee_peer_getpfc(edev->cdev, pfc);
}

static int qede_dcbnl_ieee_peer_getets(struct net_device *netdev,
				       struct ieee_ets *ets)
{
	struct qede_dev *edev = netdev_priv(netdev);

	return edev->ops->common->dcb->ieee_peer_getets(edev->cdev, ets);
}
#endif

static const struct dcbnl_rtnl_ops qede_dcbnl_ops = {
	INIT_STRUCT_FIELD(ieee_getpfc, qede_dcbnl_ieee_getpfc),
	INIT_STRUCT_FIELD(ieee_setpfc, qede_dcbnl_ieee_setpfc),
	INIT_STRUCT_FIELD(ieee_getets, qede_dcbnl_ieee_getets),
	INIT_STRUCT_FIELD(ieee_setets, qede_dcbnl_ieee_setets),
#ifdef _IEEE_8021QAZ_APP /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ieee_getapp, qede_dcbnl_ieee_getapp),
	INIT_STRUCT_FIELD(ieee_setapp, qede_dcbnl_ieee_setapp),
#endif
#ifdef DCB_CEE_SUPPORT /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(ieee_peer_getpfc, qede_dcbnl_ieee_peer_getpfc),
	INIT_STRUCT_FIELD(ieee_peer_getets, qede_dcbnl_ieee_peer_getets),
#endif
	INIT_STRUCT_FIELD(getstate, qede_dcbnl_getstate),
	INIT_STRUCT_FIELD(setstate, qede_dcbnl_setstate),
	INIT_STRUCT_FIELD(getpermhwaddr, qede_dcbnl_getpermhwaddr),
	INIT_STRUCT_FIELD(getpgtccfgtx, qede_dcbnl_getpgtccfgtx),
	INIT_STRUCT_FIELD(getpgbwgcfgtx, qede_dcbnl_getpgbwgcfgtx),
	INIT_STRUCT_FIELD(getpgtccfgrx, qede_dcbnl_getpgtccfgrx),
	INIT_STRUCT_FIELD(getpgbwgcfgrx, qede_dcbnl_getpgbwgcfgrx),
	INIT_STRUCT_FIELD(getpfccfg, qede_dcbnl_getpfccfg),
	INIT_STRUCT_FIELD(setpfccfg, qede_dcbnl_setpfccfg),
	INIT_STRUCT_FIELD(getcap, qede_dcbnl_getcap),
	INIT_STRUCT_FIELD(getnumtcs, qede_dcbnl_getnumtcs),
	INIT_STRUCT_FIELD(getpfcstate, qede_dcbnl_getpfcstate),
	INIT_STRUCT_FIELD(getapp, qede_dcbnl_getapp),
	INIT_STRUCT_FIELD(getdcbx, qede_dcbnl_getdcbx),
	INIT_STRUCT_FIELD(setpgtccfgtx, qede_dcbnl_setpgtccfgtx),
	INIT_STRUCT_FIELD(setpgtccfgrx, qede_dcbnl_setpgtccfgrx),
	INIT_STRUCT_FIELD(setpgbwgcfgtx, qede_dcbnl_setpgbwgcfgtx),
	INIT_STRUCT_FIELD(setpgbwgcfgrx, qede_dcbnl_setpgbwgcfgrx),
	INIT_STRUCT_FIELD(setall, qede_dcbnl_setall),
	INIT_STRUCT_FIELD(setnumtcs, qede_dcbnl_setnumtcs),
	INIT_STRUCT_FIELD(setpfcstate, qede_dcbnl_setpfcstate),
	INIT_STRUCT_FIELD(setapp, qede_dcbnl_setapp),
	INIT_STRUCT_FIELD(setdcbx, qede_dcbnl_setdcbx),
	INIT_STRUCT_FIELD(setfeatcfg, qede_dcbnl_setfeatcfg),
	INIT_STRUCT_FIELD(getfeatcfg, qede_dcbnl_getfeatcfg),
#ifdef DCB_CEE_SUPPORT /* QEDE_UPSTREAM */
	INIT_STRUCT_FIELD(peer_getappinfo, qede_dcbnl_peer_getappinfo),
	INIT_STRUCT_FIELD(peer_getapptable, qede_dcbnl_peer_getapptable),
	INIT_STRUCT_FIELD(cee_peer_getpfc, qede_dcbnl_cee_peer_getpfc),
	INIT_STRUCT_FIELD(cee_peer_getpg, qede_dcbnl_cee_peer_getpg),
#endif
};

void qede_set_dcbnl_ops(struct net_device *dev)
{
#ifndef QEDE_UPSTREAM /* ! QEDE_UPSTREAM */
	/* We don't bother having this file compile only if CONFIG_DCB
	 * is set [like in the upstream], but this field would not exist
	 * in the netdevice otherwise, and compilation would fail.
	 */
#endif
#ifdef CONFIG_DCB /* QEDE_UPSTREAM */
	dev->dcbnl_ops = &qede_dcbnl_ops;
#endif
}
