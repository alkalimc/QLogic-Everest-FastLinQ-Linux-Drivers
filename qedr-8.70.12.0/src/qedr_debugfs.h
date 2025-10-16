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

#ifndef __QEDR_DEBUGFS_H__
#define __QEDR_DEBUGFS_H__

#ifdef CONFIG_DEBUG_FS /* ! QEDR_UPSTREAM */
void qedr_init_debugfs(void);
void qedr_remove_debugfs(void);
void qedr_debugfs_add_stats(struct qedr_dev *dev);
void qedr_debugfs_remove_stats(struct qedr_dev *dev);
#else
void qedr_init_debugfs(void) {}
void qedr_remove_debugfs(void) {}
void qedr_debugfs_add_stats(struct qedr_dev *dev) {}
void qedr_debugfs_remove_stats(struct qedr_dev *dev) {}
#endif

void qedr_init_sysfs(void);
void qedr_remove_sysfs(void);
void qedr_sysfs_add_stats(struct qedr_dev *dev);
void qedr_sysfs_remove_stats(struct qedr_dev *dev);
#endif
