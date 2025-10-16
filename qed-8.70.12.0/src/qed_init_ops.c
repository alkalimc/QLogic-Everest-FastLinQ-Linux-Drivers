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
#include <asm/byteorder.h>
#include <linux/compiler.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#define __PREVENT_DUMP_MEM_ARR__
#define __PREVENT_PXP_GLOBAL_WIN__
#include "qed.h"
#include "qed_hsi.h"
#include "qed_hw.h"
#include "qed_init_ops.h"
#include "qed_iro_hsi.h"
#include "qed_reg_addr.h"
#include "qed_sriov.h"
#if !defined(CONFIG_QED_ZIPPED_FW) && !defined(CONFIG_QED_BINARY_FW)
#include "qed_init_values.h"
#endif
#if defined(CONFIG_QED_ZIPPED_FW) && !defined(CONFIG_QED_BINARY_FW)
#include "qed_init_values_zipped.h"
#endif
/* include the precompiled configuration values - only once */

#ifndef CONFIG_QED_BINARY_FW
#ifdef CONFIG_QED_ZIPPED_FW
#else
#endif
#endif

#define QED_INIT_MAX_POLL_COUNT 100
#define QED_INIT_POLL_PERIOD_US 500

void qed_init_iro_array(struct qed_dev *cdev)
{
	cdev->iro_arr = iro_arr + E4_IRO_ARR_OFFSET;
}

void qed_init_store_rt_reg(struct qed_hwfn *p_hwfn, u32 rt_offset, u32 val)
{
	if (rt_offset >= RUNTIME_ARRAY_SIZE) {
		DP_ERR(p_hwfn,
		       "Avoid storing %u in rt_data at index %u since RUNTIME_ARRAY_SIZE is %u!\n",
		       val, rt_offset, RUNTIME_ARRAY_SIZE);
		return;
	}

	p_hwfn->rt_data.init_val[rt_offset] = val;
	p_hwfn->rt_data.b_valid[rt_offset] = true;
}

void qed_init_store_rt_agg(struct qed_hwfn *p_hwfn,
			   u32 rt_offset, u32 * p_val, size_t size)
{
	size_t i;

	if ((rt_offset + size - 1) >= RUNTIME_ARRAY_SIZE) {
		DP_ERR(p_hwfn,
		       "Avoid storing values in rt_data at indices %u-%u since RUNTIME_ARRAY_SIZE is %u!\n",
		       rt_offset,
		       (u32) (rt_offset + size - 1), RUNTIME_ARRAY_SIZE);
		return;
	}

	for (i = 0; i < size / sizeof(u32); i++) {
		p_hwfn->rt_data.init_val[rt_offset + i] = p_val[i];
		p_hwfn->rt_data.b_valid[rt_offset + i] = true;
	}
}

static int qed_init_rt(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       u32 addr, u16 rt_offset, u16 size, bool b_must_dmae)
{
	u32 *p_init_val = &p_hwfn->rt_data.init_val[rt_offset];
	bool *p_valid = &p_hwfn->rt_data.b_valid[rt_offset];
	u16 i, segment;
	u32 j;
	int rc = 0;

	/* Since not all RT entries are initialized, go over the RT and
	 * for each segment of initialized values use DMA.
	 */
	for (i = 0; i < size; i++) {
		if (!p_valid[i])
			continue;

		/* In case there isn't any wide-bus configuration here,
		 * simply write the data instead of using dmae.
		 */
		if (!b_must_dmae) {
			qed_wr(p_hwfn, p_ptt, addr + (i << 2), p_init_val[i]);
			p_valid[i] = false;
			continue;
		}

		/* Start of a new segment */
		for (segment = 1; i + segment < size; segment++)
			if (!p_valid[i + segment])
				break;

		rc = qed_dmae_host2grc(p_hwfn, p_ptt,
				       (uintptr_t) (p_init_val + i),
				       addr + (i << 2), segment,
				       NULL /* default parameters */ );
		if (rc)
			return rc;

		/* invalidate after writing */
		/* Addition converts to signed int so casting to unsigned. */
		for (j = i; j < (u32) (i + segment); j++)
			p_valid[j] = false;

		/* Jump over the entire segment, including invalid entry */
		i += segment;
	}

	return rc;
}

int qed_init_alloc(struct qed_hwfn *p_hwfn)
{
	struct qed_rt_data *rt_data = &p_hwfn->rt_data;

	if (IS_VF(p_hwfn->cdev))
		return 0;

	rt_data->b_valid = kzalloc(RUNTIME_ARRAY_SIZE, GFP_KERNEL);
	if (!rt_data->b_valid)
		return -ENOMEM;

	rt_data->init_val = kzalloc(sizeof(u32) * RUNTIME_ARRAY_SIZE,
				    GFP_KERNEL);
	if (!rt_data->init_val) {
		kfree(rt_data->b_valid);
		rt_data->b_valid = NULL;
		return -ENOMEM;
	}

	return 0;
}

void qed_init_free(struct qed_hwfn *p_hwfn)
{
	kfree(p_hwfn->rt_data.init_val);
	p_hwfn->rt_data.init_val = NULL;
	kfree(p_hwfn->rt_data.b_valid);
	p_hwfn->rt_data.b_valid = NULL;
}

static int qed_init_array_dmae(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt,
			       u32 addr,
			       u32 dmae_data_offset,
			       u32 size,
			       const u32 * p_buf,
			       bool b_must_dmae, bool b_can_dmae)
{
	int rc = 0;

	/* Perform DMAE only for lengthy enough sections or for wide-bus */
#ifndef ASIC_ONLY
	if ((CHIP_REV_IS_SLOW(p_hwfn->cdev) && (size < 16)) ||
	    !b_can_dmae || (!b_must_dmae && (size < 16))) {
#else
	if (!b_can_dmae || (!b_must_dmae && (size < 16))) {
#endif
		const u32 *data = p_buf + dmae_data_offset;
		u32 i;

		for (i = 0; i < size; i++)
			qed_wr(p_hwfn, p_ptt, addr + (i << 2), data[i]);
	} else {
		rc = qed_dmae_host2grc(p_hwfn, p_ptt,
				       (uintptr_t) (p_buf +
						    dmae_data_offset),
				       addr, size,
				       NULL /* default parameters */ );
	}

	return rc;
}

static int qed_init_fill_dmae(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, u32 addr, u32 fill_count)
{
	static u32 zero_buffer[DMAE_MAX_RW_SIZE];
	struct dmae_params params;

	memset(zero_buffer, 0, sizeof(u32) * DMAE_MAX_RW_SIZE);

	memset(&params, 0, sizeof(params));
	SET_FIELD(params.flags, DMAE_PARAMS_RW_REPL_SRC, 0x1);
	return qed_dmae_host2grc(p_hwfn, p_ptt,
				 (uintptr_t) (&(zero_buffer[0])),
				 addr, fill_count, &params);
}

static void qed_init_fill(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt,
			  u32 addr, u32 fill, u32 fill_count)
{
	u32 i;

	for (i = 0; i < fill_count; i++, addr += sizeof(u32))
		qed_wr(p_hwfn, p_ptt, addr, fill);
}

static int qed_init_cmd_array(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt,
			      struct init_write_op *cmd,
			      bool b_must_dmae, bool b_can_dmae)
{
	u32 dmae_array_offset = le32_to_cpu(cmd->args.array_offset);
	u32 data = le32_to_cpu(cmd->data);
	u32 addr = GET_FIELD(data, INIT_WRITE_OP_ADDRESS) << 2;

#ifdef CONFIG_QED_ZIPPED_FW
	u32 offset, output_len, input_len, max_size;
#endif
	struct qed_dev *cdev = p_hwfn->cdev;
	union init_array_hdr *hdr;
	const u32 *array_data;
	int rc = 0;
	u32 size;

	array_data = cdev->fw_data->arr_data;

	hdr = (union init_array_hdr *)(array_data + dmae_array_offset);
	data = le32_to_cpu(hdr->raw.data);
	switch (GET_FIELD(data, INIT_ARRAY_RAW_HDR_TYPE)) {
	case INIT_ARR_ZIPPED:
#ifdef CONFIG_QED_ZIPPED_FW
		offset = dmae_array_offset + 1;
		input_len = GET_FIELD(data, INIT_ARRAY_ZIPPED_HDR_ZIPPED_SIZE);
		max_size = MAX_ZIPPED_SIZE * 4;
		memset(p_hwfn->unzip_buf, 0, max_size);

		output_len = qed_unzip_data(p_hwfn, input_len,
					    (u8 *) & array_data[offset],
					    max_size, (u8 *) p_hwfn->unzip_buf);
		if (output_len) {
			rc = qed_init_array_dmae(p_hwfn, p_ptt, addr, 0,
						 output_len,
						 p_hwfn->unzip_buf,
						 b_must_dmae, b_can_dmae);
		} else {
			DP_NOTICE(p_hwfn, "Failed to unzip dmae data\n");
			rc = -EINVAL;
		}
#else
		DP_NOTICE(p_hwfn,
			  "Using zipped firmware without config enabled\n");
		rc = -EINVAL;
#endif
		break;
	case INIT_ARR_PATTERN:
		{
			u32 repeats = GET_FIELD(data,
						INIT_ARRAY_PATTERN_HDR_REPETITIONS);
			u32 i;

			size = GET_FIELD(data,
					 INIT_ARRAY_PATTERN_HDR_PATTERN_SIZE);

			for (i = 0; i < repeats; i++, addr += size << 2) {
				rc = qed_init_array_dmae(p_hwfn, p_ptt, addr,
							 dmae_array_offset + 1,
							 size, array_data,
							 b_must_dmae,
							 b_can_dmae);
				if (rc)
					break;
			}
			break;
		}
	case INIT_ARR_STANDARD:
		size = GET_FIELD(data, INIT_ARRAY_STANDARD_HDR_SIZE);
		rc = qed_init_array_dmae(p_hwfn, p_ptt, addr,
					 dmae_array_offset + 1,
					 size, array_data,
					 b_must_dmae, b_can_dmae);
		break;
	}

	return rc;
}

/* init_ops write command */
static int qed_init_cmd_wr(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt,
			   struct init_write_op *p_cmd, bool b_can_dmae)
{
	u32 data = le32_to_cpu(p_cmd->data);
	bool b_must_dmae = GET_FIELD(data, INIT_WRITE_OP_WIDE_BUS);
	u32 addr = GET_FIELD(data, INIT_WRITE_OP_ADDRESS) << 2;
	int rc = 0;

	/* Sanitize */
	if (b_must_dmae && !b_can_dmae) {
		DP_NOTICE(p_hwfn,
			  "Need to write to %08x for Wide-bus but DMAE isn't allowed\n",
			  addr);
		return -EINVAL;
	}

	switch (GET_FIELD(data, INIT_WRITE_OP_SOURCE)) {
	case INIT_SRC_INLINE:
		data = le32_to_cpu(p_cmd->args.inline_val);
		qed_wr(p_hwfn, p_ptt, addr, data);
		break;
	case INIT_SRC_ZEROS:
		data = le32_to_cpu(p_cmd->args.zeros_count);
		if (b_must_dmae || (b_can_dmae && (data >= 64)))
			rc = qed_init_fill_dmae(p_hwfn, p_ptt, addr, data);
		else
			qed_init_fill(p_hwfn, p_ptt, addr, 0, data);
		break;
	case INIT_SRC_ARRAY:
		rc = qed_init_cmd_array(p_hwfn, p_ptt, p_cmd,
					b_must_dmae, b_can_dmae);
		break;
	case INIT_SRC_RUNTIME:
		rc = qed_init_rt(p_hwfn, p_ptt, addr,
				 le16_to_cpu(p_cmd->args.runtime.offset),
				 le16_to_cpu(p_cmd->args.runtime.size),
				 b_must_dmae);
		break;
	}

	return rc;
}

static inline bool comp_eq(u32 val, u32 expected_val)
{
	return val == expected_val;
}

static inline bool comp_and(u32 val, u32 expected_val)
{
	return (val & expected_val) == expected_val;
}

static inline bool comp_or(u32 val, u32 expected_val)
{
	return (val | expected_val) > 0;
}

/* init_ops read/poll commands */
static void qed_init_cmd_rd(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, struct init_read_op *cmd)
{
	bool(*comp_check) (u32 val, u32 expected_val);
	u32 delay = QED_INIT_POLL_PERIOD_US, val;
	u32 data, addr, poll;
	int i;

	data = le32_to_cpu(cmd->op_data);
	addr = GET_FIELD(data, INIT_READ_OP_ADDRESS) << 2;
	poll = GET_FIELD(data, INIT_READ_OP_POLL_TYPE);

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->cdev))
		delay *= 100;
#endif

	val = qed_rd(p_hwfn, p_ptt, addr);

	if (poll == INIT_POLL_NONE)
		return;

	switch (poll) {
	case INIT_POLL_EQ:
		comp_check = comp_eq;
		break;
	case INIT_POLL_OR:
		comp_check = comp_or;
		break;
	case INIT_POLL_AND:
		comp_check = comp_and;
		break;
	default:
		DP_ERR(p_hwfn, "Invalid poll comparison type %08x\n",
		       cmd->op_data);
		return;
	}

	data = le32_to_cpu(cmd->expected_val);
	for (i = 0; i < QED_INIT_MAX_POLL_COUNT && !comp_check(val, data); i++) {
		udelay(delay);
		val = qed_rd(p_hwfn, p_ptt, addr);
	}

	if (i == QED_INIT_MAX_POLL_COUNT) {
		DP_ERR(p_hwfn,
		       "Timeout when polling reg: 0x%08x [ Waiting-for: %08x Got: %08x (comparison %08x)]\n",
		       addr,
		       le32_to_cpu(cmd->expected_val),
		       val, le32_to_cpu(cmd->op_data));
	}
}

/* init_ops callbacks entry point */
static int qed_init_cmd_cb(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt,
			   struct init_callback_op *p_cmd)
{
	int rc;

	switch (p_cmd->callback_id) {
	case DMAE_READY_CB:
		rc = qed_dmae_sanity(p_hwfn, p_ptt, "engine_phase");
		break;
	default:
		DP_NOTICE(p_hwfn, "Unexpected init op callback ID %d\n",
			  p_cmd->callback_id);
		return -EINVAL;
	}

	return rc;
}

static u8 qed_init_cmd_mode_match(struct qed_hwfn *p_hwfn,
				  u16 * p_offset, int modes)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	u8 arg1, arg2, tree_val;
	const u8 *modes_tree;

	modes_tree = cdev->fw_data->modes_tree_buf;
	tree_val = modes_tree[(*p_offset)++];
	switch (tree_val) {
	case INIT_MODE_OP_NOT:
		return qed_init_cmd_mode_match(p_hwfn, p_offset, modes) ^ 1;
	case INIT_MODE_OP_OR:
		arg1 = qed_init_cmd_mode_match(p_hwfn, p_offset, modes);
		arg2 = qed_init_cmd_mode_match(p_hwfn, p_offset, modes);
		return arg1 | arg2;
	case INIT_MODE_OP_AND:
		arg1 = qed_init_cmd_mode_match(p_hwfn, p_offset, modes);
		arg2 = qed_init_cmd_mode_match(p_hwfn, p_offset, modes);
		return arg1 & arg2;
	default:
		tree_val -= MAX_INIT_MODE_OPS;
		return (modes & BIT(tree_val)) ? 1 : 0;
	}
}

static u32 qed_init_cmd_mode(struct qed_hwfn *p_hwfn,
			     struct init_if_mode_op *p_cmd, int modes)
{
	u16 offset = le16_to_cpu(p_cmd->modes_buf_offset);

	if (qed_init_cmd_mode_match(p_hwfn, &offset, modes))
		return 0;
	else
		return GET_FIELD(le32_to_cpu(p_cmd->op_data),
				 INIT_IF_MODE_OP_CMD_OFFSET);
}

static u32 qed_init_cmd_phase(struct init_if_phase_op *p_cmd,
			      u32 phase, u32 phase_id)
{
	u32 data = le32_to_cpu(p_cmd->phase_data);
	u32 op_data = le32_to_cpu(p_cmd->op_data);

	if (!(GET_FIELD(data, INIT_IF_PHASE_OP_PHASE) == phase &&
	      (GET_FIELD(data, INIT_IF_PHASE_OP_PHASE_ID) == ANY_PHASE_ID ||
	       GET_FIELD(data, INIT_IF_PHASE_OP_PHASE_ID) == phase_id)))
		return GET_FIELD(op_data, INIT_IF_PHASE_OP_CMD_OFFSET);
	else
		return 0;
}

int qed_init_run(struct qed_hwfn *p_hwfn,
		 struct qed_ptt *p_ptt, int phase, int phase_id, int modes)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	bool b_dmae = (phase != PHASE_ENGINE);
	u32 cmd_num, num_init_ops;
	union init_op *init;
	int rc = 0;

	num_init_ops = cdev->fw_data->init_ops_size;
	init = cdev->fw_data->init_ops;

#ifdef CONFIG_QED_ZIPPED_FW
	p_hwfn->unzip_buf = kzalloc(MAX_ZIPPED_SIZE * 4, GFP_ATOMIC);
	if (!p_hwfn->unzip_buf) {
		DP_NOTICE(p_hwfn, "Failed to allocate unzip buffer\n");
		return -ENOMEM;
	}
#endif

	for (cmd_num = 0; cmd_num < num_init_ops; cmd_num++) {
		union init_op *cmd = &init[cmd_num];
		u32 data = le32_to_cpu(cmd->raw.op_data);

		switch (GET_FIELD(data, INIT_CALLBACK_OP_OP)) {
		case INIT_OP_WRITE:
			rc = qed_init_cmd_wr(p_hwfn, p_ptt, &cmd->write,
					     b_dmae);
			break;

		case INIT_OP_READ:
			qed_init_cmd_rd(p_hwfn, p_ptt, &cmd->read);
			break;

		case INIT_OP_IF_MODE:
			cmd_num += qed_init_cmd_mode(p_hwfn, &cmd->if_mode,
						     modes);
			break;
		case INIT_OP_IF_PHASE:
			cmd_num += qed_init_cmd_phase(&cmd->if_phase, phase,
						      phase_id);
			break;
		case INIT_OP_DELAY:
			/* qed_init_run is always invoked from
			 * sleep-able context
			 */
			udelay(cmd->delay.delay);
			break;

		case INIT_OP_CALLBACK:
			rc = qed_init_cmd_cb(p_hwfn, p_ptt, &cmd->callback);
			if (phase == PHASE_ENGINE &&
			    cmd->callback.callback_id == DMAE_READY_CB)
				b_dmae = true;
			break;
		}

		if (rc)
			break;
	}
#ifdef CONFIG_QED_ZIPPED_FW
	kfree(p_hwfn->unzip_buf);
	p_hwfn->unzip_buf = NULL;
#endif
	return rc;
}

int qed_init_fw_data(struct qed_dev *cdev,
#ifdef CONFIG_QED_BINARY_FW
		     const u8 * fw_data)
#else
		     const u8 __maybe_unused * fw_data)
#endif
{
	struct qed_fw_data *fw = cdev->fw_data;

#ifdef CONFIG_QED_BINARY_FW
	struct bin_buffer_hdr *buf_hdr;
	u32 offset, len;

	if (!fw_data) {
		DP_NOTICE(cdev, "Invalid fw data\n");
		return -EINVAL;
	}

	buf_hdr = (struct bin_buffer_hdr *)fw_data;

	offset = buf_hdr[BIN_BUF_INIT_FW_VER_INFO].offset;
	fw->fw_ver_info = (struct fw_ver_info *)(fw_data + offset);

	offset = buf_hdr[BIN_BUF_INIT_CMD].offset;
	fw->init_ops = (union init_op *)(fw_data + offset);

	offset = buf_hdr[BIN_BUF_INIT_VAL].offset;
	fw->arr_data = (u32 *) (fw_data + offset);

	offset = buf_hdr[BIN_BUF_INIT_MODE_TREE].offset;
	fw->modes_tree_buf = (u8 *) (fw_data + offset);
	len = buf_hdr[BIN_BUF_INIT_CMD].length;
	fw->init_ops_size = len / sizeof(struct init_raw_op);

	offset = buf_hdr[BIN_BUF_INIT_OVERLAYS].offset;
	fw->fw_overlays = (u32 *) (fw_data + offset);
	len = buf_hdr[BIN_BUF_INIT_OVERLAYS].length;
	fw->fw_overlays_len = len;
#else
	fw->init_ops = (union init_op *)init_ops;
	fw->arr_data = (u32 *) init_val;
	fw->modes_tree_buf = (u8 *) modes_tree_buf;
	fw->init_ops_size = init_ops_size;
	fw->fw_overlays = fw_overlays;
	fw->fw_overlays_len = sizeof(fw_overlays);
#endif

	return 0;
}
