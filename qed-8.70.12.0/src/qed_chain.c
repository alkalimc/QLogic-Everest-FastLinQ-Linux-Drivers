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
#include <linux/kernel.h>
#include <linux/slab.h>
#define __PREVENT_DUMP_MEM_ARR__
#define __PREVENT_PXP_GLOBAL_WIN__
#include "qed_chain.h"

#define CHAIN_PRINT_DONE 200
#define MAX_PRINT_ELEM_SIZE 250
#define MAX_PRINT_METADATA_SIZE 1000

/**
 * @brief qed_chain_get_elem_by_idx -
 *
 * Returns a pointer to an element of a chain by its index
 *
 * @param p_chain
 * @param idx
 *
 * @return void*
 */
static void *qed_chain_get_elem_by_idx(struct qed_chain *p_chain, u32 idx)
{
	struct qed_chain_next *p_next = NULL;
	u32 elem_idx, page_idx, size, i;
	void *p_virt_addr = NULL;

	if (!p_chain->p_virt_addr)
		// add DP_ERROR
		return p_virt_addr;

	elem_idx = idx % p_chain->capacity;
	page_idx = elem_idx / p_chain->elem_per_page;

	switch (p_chain->mode) {
	case QED_CHAIN_MODE_NEXT_PTR:
		size = p_chain->elem_size * p_chain->usable_per_page;
		p_virt_addr = p_chain->p_virt_addr;
		p_next = (struct qed_chain_next *)((u8 *) p_virt_addr + size);
		for (i = 0; i < page_idx; i++) {
			p_virt_addr = p_next->next_virt;
			p_next =
			    (struct qed_chain_next *)((u8 *) p_virt_addr +
						      size);
			elem_idx -= p_chain->elem_per_page;
		}
		break;
	case QED_CHAIN_MODE_SINGLE:
		p_virt_addr = p_chain->p_virt_addr;
		break;
	case QED_CHAIN_MODE_PBL:
		p_virt_addr = p_chain->pbl.pp_virt_addr_tbl[page_idx];
		elem_idx -= page_idx * p_chain->elem_per_page;
		break;
	}
	/* p_virt_addr points at this stage to the page that inclues the
	 * requested element, and elem_idx equals to its relative index inside
	 * the page.
	 */
	p_virt_addr = (u8 *) p_virt_addr + p_chain->elem_size * elem_idx;
	return p_virt_addr;
}

static int qed_chain_print_element_raw(struct qed_chain *p_chain,
				       void *p_element, char *buffer)
{
	/* this will be a service function for the per chain print element function */
	int pos = 0, length, elem_size = p_chain->elem_size;

	/* print element byte by byte */
	while (elem_size > 0) {
		length = sprintf(buffer + pos, " %02x", *(u8 *) p_element);
		if (length < 0) {
			pr_err("Failed to copy data to buffer\n");
			return length;
		}
		pos += length;
		elem_size--;
		p_element++;
	}

	/* add tab between every element */
	length = sprintf(buffer + pos, "	");
	if (length < 0) {
		pr_err("Failed to copy data to buffer");
		return length;
	}

	pos += length;

	return pos;
}

static int qed_chain_print_metadata_raw(struct qed_chain *p_chain, char *buffer)
{
	int pos = 0, length;

	length = sprintf(buffer, "prod 0x%x [%03d], cons 0x%x [%03d]\n",
			 qed_chain_get_prod_idx(p_chain),
			 qed_chain_get_prod_idx(p_chain) & 0xff,
			 qed_chain_get_cons_idx(p_chain),
			 qed_chain_get_cons_idx(p_chain) & 0xff);
	if (length < 0) {
		pr_err("Failed to copy Metadata to buffer\n");
		return length;
	}

	pos += length;
	length = sprintf(buffer + pos,
			 "Chain capacity: %d, Chain size: %d\n",
			 p_chain->capacity, p_chain->size);
	if (length < 0) {
		pr_err("Failed to copy Metadata to buffer\n");
		return length;
	}

	pos += length;

	return pos;
}

/**
 * @chain: The chain for printing
 *
 * @element_indx: This is both an in parameter and an out
 *                      parameter. The function starts at the element at
 *                      this index and prints elements either until stop
 *                      element is reached or buffer is exhausted. The
 *                      value of this parameter *after* the function
 *                      returns holds the element index reached.
 * @stop_indx: The final index for printing, the element till this
 *             index is printed.
 **/
static int qed_chain_print_to_kernel(struct qed_chain *p_chain,
				     u32 * element_indx, u32 stop_indx)
{
	char prefix[32];
	void *p_element;
	int rc = 0;

	if (stop_indx != p_chain->capacity)
		stop_indx++;

	while (*element_indx != stop_indx) {
		*element_indx = *element_indx % p_chain->capacity;
		p_element = qed_chain_get_elem_by_idx(p_chain, *element_indx);

		scnprintf(prefix,
			  sizeof(prefix), "%p-%d:", p_chain, *element_indx);
		print_hex_dump(KERN_INFO,
			       prefix,
			       DUMP_PREFIX_NONE,
			       16, 1, p_element, p_chain->elem_size, false);

		(*element_indx)++;
	}

	/* check if all elements were printed */
	if (*element_indx != stop_indx)
		rc = CHAIN_PRINT_DONE;

	return rc;
}

static int qed_chain_print_to_buff(struct qed_chain *p_chain, char
				   *buffer,
				   int (*func_ptr_print_element) (struct
								  qed_chain *
								  p_chain,
								  void
								  *p_element,
								  char
								  *buf_to_print),
				   u32 * element_indx, u32 stop_indx,
				   u32 buffer_size)
{
	u32 prod_indx, cons_indx, pos = 0;
	char producer[13] = " [Producer] ";
	char consumer[13] = " [Consumer] ";
	void *p_element;
	int rc = 0;

	/* If the stop index doesn't equal to the chain capacity, add one to
	 * print the last index.
	 */
	if (stop_indx != p_chain->capacity)
		stop_indx++;

	/* print elements to buffer until buffer is full
	 * or done traversing all elements
	 */
	while (buffer_size > MAX_PRINT_ELEM_SIZE + pos &&
	       *element_indx != stop_indx) {
		*element_indx = *element_indx % p_chain->capacity;

		/* get index of cons and prod */
		prod_indx = qed_chain_get_prod_idx(p_chain) % p_chain->capacity;
		cons_indx = qed_chain_get_cons_idx(p_chain) % p_chain->capacity;

		pos += sprintf(buffer + pos, "%6d:", *element_indx);

		if (prod_indx == *element_indx)
			pos += scnprintf(buffer + pos, sizeof(producer),
					 producer);

		if (cons_indx == *element_indx)
			pos += scnprintf(buffer + pos, sizeof(consumer),
					 consumer);

		p_element = qed_chain_get_elem_by_idx(p_chain, *element_indx);
		rc = (*func_ptr_print_element) (p_chain, p_element, buffer +
						pos);
		if (rc < 0)
			break;

		pos += strlen(buffer + pos);
		(*element_indx)++;
	}

	/* check if all elements were printed */
	if (*element_indx != stop_indx)
		rc = CHAIN_PRINT_DONE;

	return rc;
}

/**
 * @chain: The chain for printing
 * @buffer: The buffer that the elements are copied to. The user
 *              is responsible for managing the buffer, copying the
 *              data from the buffer when the buffer is full and
 *              allocating a new buffer and calling the function
 *              again if not all data was printed.
 * @buffer_size: allocated buffer size
 * @element_indx: This is both an in parameter and an out
 *                      parameter. The function starts at the element at
 *                      this index and prints elements either until stop
 *                      element is reached or buffer is exhausted. The
 *                      value of this parameter *after* the function
 *                      returns holds the element index reached.
 * @stop_indx: The final index for printing, the element in this
 *                      index is printed.
 * @print_metadata: Indicates if chain metadata should be
 *                              printed.
 * @print_to_kernel: dump all chain elements in os/kernel logs,
 *                   if set then the api will not print anything to buffer.
 **/
/* the function will also get a pointer to the function that prints the element and pointer to a function that prints the metadata */
int qed_chain_print(struct qed_chain *p_chain, char
		    *buffer,
		    u32
		    buffer_size,
		    u32
		    * element_indx,
		    u32
		    stop_indx,
		    bool
		    print_metadata,
		    bool
		    print_to_kernel,
		    int (*func_ptr_print_element) (struct qed_chain * p_chain,
						   void
						   *p_element,
						   char
						   *buffer),
		    int (*func_ptr_print_metadata) (struct qed_chain * p_chain,
						    char *buffer))
{
	int pos = 0;
	int rc = 0;

	/* if print_to_kernel set, it will not print in buffer */
	if (print_to_kernel) {
		rc = qed_chain_print_to_kernel(p_chain, element_indx,
					       stop_indx);
		return rc;
	}

	/* if file pointers are empty point to default functions */
	if (!func_ptr_print_element)
		func_ptr_print_element = qed_chain_print_element_raw;
	if (!func_ptr_print_metadata)
		func_ptr_print_metadata = qed_chain_print_metadata_raw;

	if (print_metadata) {
		if (MAX_PRINT_METADATA_SIZE < buffer_size) {
			pos += func_ptr_print_metadata(p_chain, buffer);
		} else {
			pr_err
			    ("Failed to copy Metadata to buffer, buffer is too small\n");
			return -1;
		}
	}

	/* print the elements to the buffer */
	rc = qed_chain_print_to_buff(p_chain, buffer + pos,
				     func_ptr_print_element, element_indx,
				     stop_indx, buffer_size);

	return rc;
}
