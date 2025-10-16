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

#if !defined(IB_PEER_MEM_H)
#define IB_PEER_MEM_H

#define QEDR_GDR_MODULE_VERSION "8.42.0.0"
#include <rdma/ib_umem.h>

struct ib_peer_memory_statistics {
	atomic64_t num_alloc_mrs;
	atomic64_t num_dealloc_mrs;
	atomic64_t num_reg_pages;
	atomic64_t num_dereg_pages;
	atomic64_t num_reg_bytes;
	atomic64_t num_dereg_bytes;
	unsigned long num_free_callbacks;
};

typedef int (*umem_invalidate_func_t)(void *invalidation_cookie,
                struct ib_umem *umem,
                unsigned long addr, size_t size);

struct ib_peer_memory_client {
	const struct peer_memory_client *peer_mem;
	struct list_head	core_peer_list;
	int invalidation_required;
	struct kref ref;
	struct completion unload_comp;
	/* lock is used via the invalidation flow */
	struct mutex lock;
	struct list_head   core_ticket_list;
	u64	last_ticket;
	struct kobject *kobj;
	struct attribute_group peer_mem_attr_group;
	struct ib_peer_memory_statistics stats;
};

enum ib_peer_mem_flags {
	IB_PEER_MEM_ALLOW	= 1,
	IB_PEER_MEM_INVAL_SUPP = (1<<1),
};

struct core_ticket {
	unsigned long key;
	void *context;
	struct list_head   ticket_list;
};

struct ib_peer_memory_client *ib_get_peer_client(struct ib_ucontext *context, unsigned long addr,
						 size_t size, unsigned long peer_mem_flags,
						 void **peer_client_context);

void ib_put_peer_client(struct ib_peer_memory_client *ib_peer_client,
			void *peer_client_context);

struct ib_umem *ib_umem_get_peer(struct ib_ucontext *context, unsigned long addr,
				 size_t size, int access, int dmasync);

void ib_peer_umem_release(struct ib_umem *umem, bool peer_callback);

struct ib_gdr_umem_ops {
	int (*invalidation_notifier)(struct ib_umem *ibumem,
						      umem_invalidate_func_t func,
						      void *cookie);
	void (*release)(struct ib_umem *umem, bool peer_callback);
	struct ib_umem *(*get_peer)(struct ib_ucontext *context, unsigned long addr,
				 size_t size, int access, int dmasync);
};

#endif
