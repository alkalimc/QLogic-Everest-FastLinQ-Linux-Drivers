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

#ifndef _QED_PHY_API_H
#define _QED_PHY_API_H
#include <linux/types.h>

/**
 * @brief Phy core write
 *
 *  @param p_hwfn
 *  @param port
 *  @param addr - nvm offset
 *  @param p_phy_result_buf - result buffer
 *  @param data_hi - low 32 bit of data to write
 *  @param data_lo - high 32 bit of data to write
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_core_write(struct qed_hwfn *p_hwfn,
		       u32 port,
		       u32 addr,
		       u32 data_lo, u32 data_hi, char *p_phy_result_buf);

/**
 * @brief Phy core read
 *
 *  @param p_hwfn
 *  @param port
 *  @param addr - nvm offset
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_core_read(struct qed_hwfn *p_hwfn,
		      u32 port, u32 addr, char *p_phy_result_buf);

/**
 * @brief Phy raw write
 *
 *  @param p_hwfn
 *  @param port
 *  @param lane
 *  @param addr - nvm offset
 *  @param p_phy_result_buf - result buffer
 *  @param data_hi - low 32 bit of data to write
 *  @param data_lo - high 32 bit of data to write
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_raw_write(struct qed_hwfn *p_hwfn,
		      u32 port,
		      u32 lane,
		      u32 addr,
		      u32 data_lo, u32 data_hi, char *p_phy_result_buf);

/**
 * @brief Phy raw read
 *
 *  @param p_hwfn
 *  @param port
 *  @param lane
 *  @param addr - nvm offset
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_raw_read(struct qed_hwfn *p_hwfn,
		     u32 port, u32 lane, u32 addr, char *p_phy_result_buf);

/**
 * @brief Phy mac status
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param port
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_mac_stat(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt, u32 port, char *p_phy_result_buf);

/**
 * @brief Phy info
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_info(struct qed_hwfn *p_hwfn,
		 struct qed_ptt *p_ptt, char *p_phy_result_buf);

/**
 * @brief Sfp write
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param port
 *  @param addr - I2C address
 *  @param offset - EEPROM offset
 *  @param size - number of bytes to write
 *  @param val - byte array to write (1, 2 or 4 bytes)
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_sfp_write(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt,
		      u32 port,
		      u32 addr,
		      u32 offset, u32 size, u32 val, char *p_phy_result_buf);

/**
 * @brief Sfp read
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param port
 *  @param addr - I2C address
 *  @param offset - EEPROM offset
 *  @param size - number of bytes to read
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_sfp_read(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt,
		     u32 port,
		     u32 addr, u32 offset, u32 size, char *p_phy_result_buf);

/**
 * @brief Sfp decode
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param port
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_sfp_decode(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt, u32 port, char *p_phy_result_buf);

/**
 * @brief Sfp get inserted
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param port
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_sfp_get_inserted(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt,
			     u32 port, char *p_phy_result_buf);

/**
 * @brief Sfp get txdisable
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param port
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_sfp_get_txdisable(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt,
			      u32 port, char *p_phy_result_buf);

/**
 * @brief Sfp set txdisable
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param port
 *  @param txdisable - tx disable value to set
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_sfp_set_txdisable(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt,
			      u32 port, u8 txdisable, char *p_phy_result_buf);

/**
 * @brief Sfp get txreset
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param port
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_sfp_get_txreset(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt,
			    u32 port, char *p_phy_result_buf);

/**
 * @brief Sfp get rxlos
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param port
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_sfp_get_rxlos(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt,
			  u32 port, char *p_phy_result_buf);

/**
 * @brief Sfp get eeprom
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param port
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_sfp_get_eeprom(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt,
			   u32 port, char *p_phy_result_buf);

/**
 * @brief Gpio write
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param gpio - gpio number
 *  @param gpio_val - value to write to gpio
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_gpio_write(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       u16 gpio, u16 gpio_val, char *p_phy_result_buf);

/**
 * @brief Gpio read
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param gpio - gpio number
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_gpio_read(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, u16 gpio, char *p_phy_result_buf);

/**
 * @brief Gpio get information
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param gpio - gpio number
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_gpio_info(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, u16 gpio, char *p_phy_result_buf);

/**
 * @brief Ext-Phy Read operation
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param port - port number
 *  @param devad - device address
 *  @param reg - register
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_extphy_read(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt,
			u16 port, u16 devad, u16 reg, char *p_phy_result_buf);

/**
 * @brief Ext-Phy Write operation
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param port - port number
 *  @param devad - device address
 *  @param reg - register
 *  @param val - value to be written
 *  @param p_phy_result_buf - result buffer
 *
 * @return int - 0 - operation was successful.
 */
int qed_phy_extphy_write(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 u16 port,
			 u16 devad, u16 reg, u16 val, char *p_phy_result_buf);

#endif
