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

#ifndef QEDE_RDMA_H
#define QEDE_RDMA_H

struct qedr_dev;
struct qed_dev;
struct qede_dev;

enum qede_rdma_event {
	QEDE_UP,
	QEDE_CHANGE_ADDR,
	QEDE_DOWN,
	QEDE_CHANGE_MTU,
};

struct qede_rdma_event_work {
	struct list_head	list;
	struct work_struct	work;
	void			*ptr;
	enum qede_rdma_event	event;
};

enum qede_reset_event {
	QEDE_RESET_EVENT_START,
	QEDE_RESET_EVENT_DONE,
	QEDE_RESET_EVENT_ERROR
};

struct qedr_driver {
	unsigned char name[32];

	struct qedr_dev *(*add)(struct qed_dev *cdev,
				struct pci_dev *pdev,
				struct net_device *ndev,
				bool lag_enable, bool is_vf);

	void (*remove) (struct qedr_dev *dev);

	void (*notify) (struct qedr_dev *dev , enum qede_rdma_event);

	void (*reset) (struct qedr_dev *dev, struct qed_dev *cdev,
		       enum qede_reset_event);
};

/* APIs for RDMA driver to register callback handlers,
 * which will be invoked when device is added, removed, ifup, ifdown
 */
int qede_rdma_register_driver(struct qedr_driver *drv);
void qede_rdma_unregister_driver(struct qedr_driver *drv);
bool qede_rdma_supported(struct qede_dev *dev);

#if IS_ENABLED(CONFIG_QED_RDMA)
int qede_rdma_dev_add(struct qede_dev *dev, bool reset_mode);
void qede_rdma_dev_event_open(struct qede_dev *dev);
void qede_rdma_dev_event_close(struct qede_dev *dev);
void qede_rdma_dev_remove(struct qede_dev *dev, bool reset_mode);
void qede_rdma_event_changeaddr(struct qede_dev *edr);
void qede_rdma_event_change_mtu(struct qede_dev *edev);
#else
static inline int qede_rdma_dev_add(struct qede_dev *dev, bool reset_mode)
{return 0; }
static inline void qede_rdma_dev_event_open(struct qede_dev *dev) {}
static inline void qede_rdma_dev_event_close(struct qede_dev *dev) {}
static inline void qede_rdma_dev_remove(struct qede_dev *dev, bool reset_mode)
{}
static inline void qede_rdma_event_changeaddr(struct qede_dev *edr) {}
static inline void qede_rdma_event_change_mtu(struct qede_dev *edev) {}
#endif

#endif
