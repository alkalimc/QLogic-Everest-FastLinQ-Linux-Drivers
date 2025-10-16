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

#ifndef __QEDR_GDR_H__
#define __QEDR_GDR_H__

#ifdef DEFINE_WITH_GDR
int qedr_gdr_ib_umem_get(struct ib_ucontext *context, unsigned long addr,
			  size_t size, int access, int dmasync,
			  struct qedr_mr *mr);

void qedr_gdr_reg_user_complete(struct qedr_mr *mr);

int qedr_gdr_dereg(struct qedr_mr *mr);

#else

static int qedr_gdr_ib_umem_get(struct ib_ucontext *context, unsigned long addr,
			  size_t size, int access, int dmasync,
			  struct qedr_mr *mr)
{
	mr->gdr = false;
	return 0;
}

static void qedr_gdr_reg_user_complete(struct qedr_mr *mr) {}

static int qedr_gdr_dereg(struct qedr_mr *mr)
{
	/* unexpected call if no gdr */
	return -EINVAL;
}

#endif
#endif
