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

#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/list.h>
#include <linux/mutex.h>

#include "qede.h"
#include "qede_rdma.h"

static struct qedr_driver *qedr_drv;
static LIST_HEAD(qedr_dev_list);
static DEFINE_MUTEX(qedr_dev_list_lock);

bool qede_rdma_supported(struct qede_dev *dev)
{
	return dev->dev_info.common.rdma_supported;
}

static void qede_rdma_reset(struct qede_dev *edev, enum qede_reset_event event)
{
	if (!qede_rdma_supported(edev))
		return;

	if (qedr_drv && edev->rdma_info.qedr_dev && qedr_drv->reset)
		qedr_drv->reset(edev->rdma_info.qedr_dev, edev->cdev, event);
}

struct qedr_dev *_qede_rdma_dev_add(struct qede_dev *edev, bool lag_enable)
{
	if (!qedr_drv)
		return NULL;

	return qedr_drv->add(edev->cdev, edev->pdev, edev->ndev, lag_enable,
			     IS_VF(edev));
}

static int qede_rdma_create_wq(struct qede_dev *edev)
{
	INIT_LIST_HEAD(&edev->rdma_info.rdma_event_list);
	kref_init(&edev->rdma_info.refcnt);
	init_completion(&edev->rdma_info.event_comp);

	edev->rdma_info.rdma_wq = create_singlethread_workqueue("rdma_wq");
	if (!edev->rdma_info.rdma_wq) {
		DP_NOTICE(edev, "qedr: Could not create workqueue\n");
		return -ENOMEM;
	}

	return 0;
}

static void qede_rdma_cleanup_event(struct qede_dev *edev)
{
	struct list_head *head = &edev->rdma_info.rdma_event_list;
	struct qede_rdma_event_work *event_node;

	flush_workqueue(edev->rdma_info.rdma_wq);
	while (!list_empty(head)) {
		event_node = list_entry(head->next, struct qede_rdma_event_work,
					list);
		cancel_work_sync(&event_node->work);
		list_del(&event_node->list);
		kfree(event_node);
	}
}

static void qede_rdma_complete_event(struct kref *ref)
{
	struct qede_rdma_dev *rdma_dev =
		container_of(ref, struct qede_rdma_dev, refcnt);

	/* no more events will be added after this */
	complete(&rdma_dev->event_comp);
}

static void qede_rdma_destroy_wq(struct qede_dev *edev)
{
	/* Avoid race with add_event flow, make sure it finishes before
	 * we start accessing the list and cleaning up the work
	 */
	kref_put(&edev->rdma_info.refcnt, qede_rdma_complete_event);
	wait_for_completion(&edev->rdma_info.event_comp);

	qede_rdma_cleanup_event(edev);
	destroy_workqueue(edev->rdma_info.rdma_wq);
	edev->rdma_info.rdma_wq = NULL;
}

int qede_rdma_dev_add(struct qede_dev *edev, bool reset_mode)
{
	struct qede_dev *temp_edev;
	bool lag_dev = false;
	int rc = 0;

	if (!qede_rdma_supported(edev))
		return 0;

	/* Arrived here from reset flow, which is now complete. Mark as such */
	if (reset_mode) {
		qede_rdma_reset(edev, QEDE_RESET_EVENT_DONE);

		return 0;
	}

	rc = qede_rdma_create_wq(edev);
	if (rc)
		return rc;

	INIT_LIST_HEAD(&edev->rdma_info.entry);
	mutex_lock(&qedr_dev_list_lock);
	list_add_tail(&edev->rdma_info.entry, &qedr_dev_list);

	list_for_each_entry(temp_edev, &qedr_dev_list, rdma_info.entry) {
		if (temp_edev->rdma_info.lag_enabled &&
		    (PCI_FUNC(temp_edev->pdev->devfn) % 2 == 0)) {
			DP_NOTICE(temp_edev,
				  "Found LAG enabled while probing qede devices\n");
			lag_dev = true;
		}
	}

	if (!(lag_dev && PCI_FUNC(edev->pdev->devfn) % 2 == 1))
		edev->rdma_info.qedr_dev = _qede_rdma_dev_add(edev, lag_dev);
	else
		DP_NOTICE(edev,
			  "Skipping adding odd PCI functions, LAG enabled\n");

	mutex_unlock(&qedr_dev_list_lock);

	return rc;
}

void _qede_rdma_dev_remove(struct qede_dev *edev)
{
	if (qedr_drv && qedr_drv->remove && edev->rdma_info.qedr_dev)
		qedr_drv->remove(edev->rdma_info.qedr_dev);
}

void qede_rdma_dev_remove(struct qede_dev *edev, bool reset_mode)
{
	if (!qede_rdma_supported(edev))
		return;

	/* Arrived here to perform reset. Mark the begining */
	if (reset_mode) {
		qede_rdma_reset(edev, QEDE_RESET_EVENT_START);

		return;
	}

	qede_rdma_destroy_wq(edev);

	mutex_lock(&qedr_dev_list_lock);
	_qede_rdma_dev_remove(edev);
	edev->rdma_info.qedr_dev = NULL;
	list_del(&edev->rdma_info.entry);
	mutex_unlock(&qedr_dev_list_lock);
}

void _qede_rdma_dev_open(struct qede_dev *edev)
{
	if (qedr_drv && edev->rdma_info.qedr_dev && qedr_drv->notify)
		qedr_drv->notify(edev->rdma_info.qedr_dev, QEDE_UP);
}

static void qede_rdma_dev_open(struct qede_dev *edev)
{
	if (!qede_rdma_supported(edev))
		return;

	mutex_lock(&qedr_dev_list_lock);
	_qede_rdma_dev_open(edev);
	mutex_unlock(&qedr_dev_list_lock);
}

static void _qede_rdma_dev_close(struct qede_dev *edev)
{
	if (qedr_drv && edev->rdma_info.qedr_dev && qedr_drv->notify)
		qedr_drv->notify(edev->rdma_info.qedr_dev, QEDE_DOWN);
}

static void qede_rdma_dev_close(struct qede_dev *edev)
{
	if (!qede_rdma_supported(edev))
		return;

	mutex_lock(&qedr_dev_list_lock);
	_qede_rdma_dev_close(edev);
	mutex_unlock(&qedr_dev_list_lock);
}

int qede_rdma_register_driver(struct qedr_driver *drv)
{
	struct net_device *ndev;
	struct qede_dev *edev;
	bool lag_dev;
	u8 qedr_counter = 0;

	mutex_lock(&qedr_dev_list_lock);
	if (qedr_drv) {
		mutex_unlock(&qedr_dev_list_lock);
		return -EINVAL;
	}
	qedr_drv = drv;

	list_for_each_entry(edev, &qedr_dev_list, rdma_info.entry) {
		lag_dev = edev->rdma_info.lag_enabled;

		if (lag_dev) {
			if (PCI_FUNC(edev->pdev->devfn) % 2 == 0)
				DP_NOTICE(edev,
					  "Found LAG enabled during qedr load\n");
			else /* In case LAG is supported, rdma device should be created only for even PFs*/
				continue;
		}

		edev->rdma_info.qedr_dev = _qede_rdma_dev_add(edev, lag_dev);
		if (edev->rdma_info.qedr_dev)
			qedr_counter++;

		ndev = edev->ndev;
		if (netif_running(ndev) && netif_oper_up(ndev))
			_qede_rdma_dev_open(edev);
	}
	mutex_unlock(&qedr_dev_list_lock);

	pr_notice("qedr: discovered and registered %d RDMA funcs\n",
		  qedr_counter);

	return 0;
}
EXPORT_SYMBOL(qede_rdma_register_driver);

void qede_rdma_unregister_driver(struct qedr_driver *drv)
{
	struct qede_dev *edev;

	mutex_lock(&qedr_dev_list_lock);
	list_for_each_entry(edev, &qedr_dev_list, rdma_info.entry) {
		if (edev->rdma_info.qedr_dev) {
			_qede_rdma_dev_remove(edev);
			edev->rdma_info.qedr_dev = NULL;
		}
	}
	qedr_drv = NULL;
	mutex_unlock(&qedr_dev_list_lock);
}
EXPORT_SYMBOL(qede_rdma_unregister_driver);

static void qede_rdma_changeaddr(struct qede_dev *edev)
{
	if (!qede_rdma_supported(edev))
		return;

	if (qedr_drv && edev->rdma_info.qedr_dev && qedr_drv->notify)
		qedr_drv->notify(edev->rdma_info.qedr_dev, QEDE_CHANGE_ADDR);
}

static void qede_rdma_change_mtu(struct qede_dev *edev)
{
	if (qede_rdma_supported(edev)) {
		if (qedr_drv && edev->rdma_info.qedr_dev && qedr_drv->notify)
			qedr_drv->notify(edev->rdma_info.qedr_dev,
					 QEDE_CHANGE_MTU);
	}
}

static struct qede_rdma_event_work *
qede_rdma_get_free_event_node(struct qede_dev *edev)
{
	struct qede_rdma_event_work *event_node = NULL;
	struct list_head *list_node = NULL;
	bool found = false;

	list_for_each(list_node, &edev->rdma_info.rdma_event_list) {
		event_node = list_entry(list_node, struct qede_rdma_event_work,
					list);
		if (!work_pending(&event_node->work)) {
			found = true;
			break;
		}
	}

	if (!found) {
		event_node = kzalloc(sizeof(*event_node), GFP_ATOMIC);
		if (!event_node) {
			DP_NOTICE(edev,
				  "qedr: Could not allocate memory for rdma work\n");
			return NULL;
		}
		list_add_tail(&event_node->list,
			      &edev->rdma_info.rdma_event_list);
	}

	return event_node;
}

static void qede_rdma_handle_event(struct work_struct *work)
{
	struct qede_rdma_event_work *event_node;
	enum qede_rdma_event event;
	struct qede_dev *edev;

	event_node = container_of(work, struct qede_rdma_event_work, work);
	event = event_node->event;
	edev = event_node->ptr;

	switch (event) {
	case QEDE_UP:
		qede_rdma_dev_open(edev);
		break;
	case QEDE_DOWN:
		qede_rdma_dev_close(edev);
		break;
	case QEDE_CHANGE_ADDR:
		qede_rdma_changeaddr(edev);
		break;
	case QEDE_CHANGE_MTU:
		qede_rdma_change_mtu(edev);
		break;
	default:
		DP_NOTICE(edev, "qede: Invalid rdma event %d", event);
	}
}

static void qede_rdma_add_event(struct qede_dev *edev,
				enum qede_rdma_event event)
{
	struct qede_rdma_event_work *event_node;

	if (!edev->rdma_info.qedr_dev || !edev->rdma_info.rdma_wq)
		return;

	/* We don't want the cleanup flow to start while we're allocating and
	 * scheduling the work
	 */
	if (!kref_get_unless_zero(&edev->rdma_info.refcnt))
		return; /* already being destroyed */

	event_node = qede_rdma_get_free_event_node(edev);
	if (!event_node)
		return;

	event_node->event = event;
	event_node->ptr = edev;

	INIT_WORK(&event_node->work, qede_rdma_handle_event);
	queue_work(edev->rdma_info.rdma_wq, &event_node->work);
	kref_put(&edev->rdma_info.refcnt, qede_rdma_complete_event);
}

void qede_rdma_dev_event_open(struct qede_dev *edev)
{
	qede_rdma_add_event(edev, QEDE_UP);
}

void qede_rdma_dev_event_close(struct qede_dev *edev)
{
	qede_rdma_add_event(edev, QEDE_DOWN);
}

void qede_rdma_event_changeaddr(struct qede_dev *edev)
{
	qede_rdma_add_event(edev, QEDE_CHANGE_ADDR);
}

void qede_rdma_event_change_mtu(struct qede_dev *edev)
{
	qede_rdma_add_event(edev, QEDE_CHANGE_MTU);
}

