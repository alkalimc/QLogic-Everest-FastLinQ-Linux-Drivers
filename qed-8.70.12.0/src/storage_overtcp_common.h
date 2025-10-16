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

#ifndef __STORAGE_OVERTCP_COMMON__
#define __STORAGE_OVERTCP_COMMON__

/*
 * iscsi debug modes
 */
struct iscsi_debug_modes {
	u8 flags;
#define ISCSI_DEBUG_MODES_ASSERT_IF_RX_CONN_ERROR_MASK             0x1	/* Assert on Rx connection error */
#define ISCSI_DEBUG_MODES_ASSERT_IF_RX_CONN_ERROR_SHIFT            0
#define ISCSI_DEBUG_MODES_ASSERT_IF_RECV_RESET_MASK                0x1	/* Assert if TCP RESET arrived */
#define ISCSI_DEBUG_MODES_ASSERT_IF_RECV_RESET_SHIFT               1
#define ISCSI_DEBUG_MODES_ASSERT_IF_RECV_FIN_MASK                  0x1	/* Assert if TCP FIN arrived */
#define ISCSI_DEBUG_MODES_ASSERT_IF_RECV_FIN_SHIFT                 2
#define ISCSI_DEBUG_MODES_ASSERT_IF_RECV_CLEANUP_MASK              0x1	/* Assert if cleanup flow */
#define ISCSI_DEBUG_MODES_ASSERT_IF_RECV_CLEANUP_SHIFT             3
#define ISCSI_DEBUG_MODES_ASSERT_IF_RECV_REJECT_OR_ASYNC_MASK      0x1	/* Assert if REJECT PDU or ASYNC PDU arrived */
#define ISCSI_DEBUG_MODES_ASSERT_IF_RECV_REJECT_OR_ASYNC_SHIFT     4
#define ISCSI_DEBUG_MODES_ASSERT_IF_RECV_NOP_MASK                  0x1	/* Assert if NOP IN PDU or NOP OUT PDU arrived */
#define ISCSI_DEBUG_MODES_ASSERT_IF_RECV_NOP_SHIFT                 5
#define ISCSI_DEBUG_MODES_ASSERT_IF_DIF_OR_DATA_DIGEST_ERROR_MASK  0x1	/* Assert if DIF or data digest error */
#define ISCSI_DEBUG_MODES_ASSERT_IF_DIF_OR_DATA_DIGEST_ERROR_SHIFT 6
#define ISCSI_DEBUG_MODES_ASSERT_IF_HQ_CORRUPT_MASK                0x1	/* Assert if HQ corruption detected */
#define ISCSI_DEBUG_MODES_ASSERT_IF_HQ_CORRUPT_SHIFT               7
};

#endif /* __STORAGE_OVERTCP_COMMON__ */
