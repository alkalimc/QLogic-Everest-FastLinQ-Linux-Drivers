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

#ifndef _QED_HW_H
#define _QED_HW_H
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "qed.h"

/* Forward declaration */
struct qed_ptt;

enum reserved_ptts {
	RESERVED_PTT_EDIAG,
	RESERVED_PTT_USER_SPACE,
	RESERVED_PTT_MAIN,
	RESERVED_PTT_DPC,
	RESERVED_PTT_MAX
};

/* @@@TMP - in earlier versions of the emulation, the HW lock started from 1
 * instead of 0, this should be fixed in later HW versions.
 */

/* Definitions for DMA constants */
#define DMAE_GO_VALUE   0x1

#ifdef __BIG_ENDIAN
#define DMAE_COMPLETION_VAL     0xAED10000
#define DMAE_CMD_ENDIANITY      0x3
#else
#define DMAE_COMPLETION_VAL     0xD1AE
#define DMAE_CMD_ENDIANITY      0x2
#endif

#define DMAE_CMD_SIZE   14

/* Size of DMAE command structure to fill.. DMAE_CMD_SIZE-5 */
#define DMAE_CMD_SIZE_TO_FILL   (DMAE_CMD_SIZE - 5)

/* Minimum wait for dmae operation to complete 2 milliseconds */
#define DMAE_MIN_WAIT_TIME      0x2
#define DMAE_MAX_CLIENTS        32

/**
 * @brief qed_gtt_init - Initialize GTT windows
 *
 * @param p_hwfn
 */
void qed_gtt_init(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_ptt_invalidate - Forces all ptt entries to be re-configured
 *
 * @param p_hwfn
 */
void qed_ptt_invalidate(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_ptt_pool_alloc - Allocate and initialize PTT pool
 *
 * @param p_hwfn
 *
 * @return struct _qed_status - success (0), negative - error.
 */
int qed_ptt_pool_alloc(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_ptt_pool_free -
 *
 * @param p_hwfn
 */
void qed_ptt_pool_free(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_ptt_get_bar_addr - Get PPT's external BAR address
 *
 * @param p_ptt
 *
 * @return u32
 */
u32 qed_ptt_get_bar_addr(struct qed_ptt *p_ptt);

/**
 * @brief qed_ptt_set_win - Set PTT Window's GRC BAR address
 *
 * @param p_hwfn
 * @param p_ptt
 * @param new_hw_addr
 */
void qed_ptt_set_win(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt, u32 new_hw_addr);

/**
 * @brief qed_get_reserved_ptt - Get a specific reserved PTT
 *
 * @param p_hwfn
 * @param ptt_idx
 *
 * @return struct qed_ptt *
 */
struct qed_ptt *qed_get_reserved_ptt(struct qed_hwfn *p_hwfn,
				     enum reserved_ptts ptt_idx);

/**
 * @brief qed_wr - Write value to BAR using the given ptt
 *
 * @param p_hwfn
 * @param p_ptt
 * @param hw_addr
 * @param val
 */
void qed_wr(struct qed_hwfn *p_hwfn,
	    struct qed_ptt *p_ptt, u32 hw_addr, u32 val);

/**
 * @brief qed_rd - Read value from BAR using the given ptt
 *
 * @param p_hwfn
 * @param p_ptt
 * @param hw_addr
 */
u32 qed_rd(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u32 hw_addr);

/**
 * @brief qed_memset_hw - set (n / 4) DWORDs from BAR using the given ptt
 *        with DWORD value
 *
 * @param p_hwfn
 * @param p_ptt
 * @param value
 * @param hw_addr
 * @param n
 */
void qed_memzero_hw(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt, u32 hw_addr, size_t n);

/**
 * @brief qed_memcpy_from - copy n bytes from BAR using the given
 *        ptt
 *
 * @param p_hwfn
 * @param p_ptt
 * @param dest
 * @param hw_addr
 * @param n
 */
void qed_memcpy_from(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt, void *dest, u32 hw_addr, size_t n);

/**
 * @brief qed_memcpy_to - copy n bytes to BAR using the given
 *        ptt
 *
 * @param p_hwfn
 * @param p_ptt
 * @param hw_addr
 * @param src
 * @param n
 */
void qed_memcpy_to(struct qed_hwfn *p_hwfn,
		   struct qed_ptt *p_ptt, u32 hw_addr, void *src, size_t n);
/**
 * @brief qed_fid_pretend - pretend to another function when
 *        accessing the ptt window. There is no way to unpretend
 *        a function. The only way to cancel a pretend is to
 *        pretend back to the original function.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param fid - fid field of pxp_pretend structure. Can contain
 *            either pf / vf, port/path fields are don't care.
 */
void qed_fid_pretend(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u16 fid);

/**
 * @brief qed_port_pretend - pretend to another port when
 *        accessing the ptt window
 *
 * @param p_hwfn
 * @param p_ptt
 * @param port_id - the port to pretend to
 */
void qed_port_pretend(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, u8 port_id);

/**
 * @brief qed_port_unpretend - cancel any previously set port
 *        pretend
 *
 * @param p_hwfn
 * @param p_ptt
 */
void qed_port_unpretend(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

/**
 * @brief qed_port_fid_pretend - pretend to another port and another function
 *        when accessing the ptt window
 *
 * @param p_hwfn
 * @param p_ptt
 * @param port_id - the port to pretend to
 * @param fid - fid field of pxp_pretend structure. Can contain either pf / vf.
 */
void qed_port_fid_pretend(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, u8 port_id, u16 fid);

/**
 * @brief qed_vfid_to_concrete - build a concrete FID for a
 *        given VF ID
 *
 * @param p_hwfn
 * @param p_ptt
 * @param vfid
 */
u32 qed_vfid_to_concrete(struct qed_hwfn *p_hwfn, u8 vfid);

/**
 * @brief qed_dmae_info_alloc - Init the dmae_info structure
 * which is part of p_hwfn.
 * @param p_hwfn
 */
int qed_dmae_info_alloc(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_dmae_info_free - Free the dmae_info structure
 * which is part of p_hwfn
 *
 * @param p_hwfn
 */
void qed_dmae_info_free(struct qed_hwfn *p_hwfn);

/**
 * @brief qed_dmae_host2grc - copy data from source address to
 * dmae registers using the given ptt
 *
 * @param p_hwfn
 * @param p_ptt
 * @param source_addr
 * @param grc_addr (dmae_data_offset)
 * @param size_in_dwords
 * @param p_params (default parameters will be used in case of NULL)
 *
 * @return int
 */
int
qed_dmae_host2grc(struct qed_hwfn *p_hwfn,
		  struct qed_ptt *p_ptt,
		  u64 source_addr,
		  u32 grc_addr,
		  u32 size_in_dwords, struct dmae_params *p_params);

/**
 * @brief qed_dmae_grc2host - Read data from dmae data offset
 * to source address using the given ptt
 *
 * @param p_ptt
 * @param grc_addr (dmae_data_offset)
 * @param dest_addr
 * @param size_in_dwords
 * @param p_params (default parameters will be used in case of NULL)
 *
 * @return int
 */
int
qed_dmae_grc2host(struct qed_hwfn *p_hwfn,
		  struct qed_ptt *p_ptt,
		  u32 grc_addr,
		  dma_addr_t dest_addr,
		  u32 size_in_dwords, struct dmae_params *p_params);

/**
 * @brief qed_dmae_host2host - copy data from to source address
 * to a destination address (for SRIOV) using the given ptt
 *
 * @param p_hwfn
 * @param p_ptt
 * @param source_addr
 * @param dest_addr
 * @param size_in_dwords
 * @param p_params (default parameters will be used in case of NULL)
 *
 * @return int
 */
int
qed_dmae_host2host(struct qed_hwfn *p_hwfn,
		   struct qed_ptt *p_ptt,
		   dma_addr_t source_addr,
		   dma_addr_t dest_addr,
		   u32 size_in_dwords, struct dmae_params *p_params);

int qed_dmae_sanity(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt, const char *phase);

int qed_init_fw_data(struct qed_dev *cdev, const u8 * fw_data);

#define QED_HW_ERR_MAX_STR_SIZE 256

/**
 * @brief qed_hw_err_notify - Notify upper layer driver and management FW
 *	about a HW error.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param err_type
 * @param fmt - debug data buffer to send to the MFW
 * @param ... - buffer format args
 */
void qed_hw_err_notify(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       enum qed_hw_err_type err_type, char *fmt, ...);

/**
 * @brief qed_ppfid_wr - Write value to BAR using the given ptt while
 *	pretending to a PF to which the given PPFID pertains.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param abs_ppfid
 * @param hw_addr
 * @param val
 */
void qed_ppfid_wr(struct qed_hwfn *p_hwfn,
		  struct qed_ptt *p_ptt, u8 abs_ppfid, u32 hw_addr, u32 val);

/**
 * @brief qed_ppfid_rd - Read value from BAR using the given ptt while
 *	 pretending to a PF to which the given PPFID pertains.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param abs_ppfid
 * @param hw_addr
 */
u32 qed_ppfid_rd(struct qed_hwfn *p_hwfn,
		 struct qed_ptt *p_ptt, u8 abs_ppfid, u32 hw_addr);

#endif
