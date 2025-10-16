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

#ifndef _QED_IRO_HSI_H
#define _QED_IRO_HSI_H
#include <linux/types.h>
#ifndef __IRO_H__
#define __IRO_H__

enum {
	IRO_YSTORM_FLOW_CONTROL_MODE_GTT,
	IRO_YSTORM_VF_ZONE,
	IRO_PSTORM_PKT_DUPLICATION_CFG,
	IRO_TSTORM_PORT_STAT,
	IRO_TSTORM_LL2_PORT_STAT,
	IRO_TSTORM_PKT_DUPLICATION_CFG,
	IRO_USTORM_VF_PF_CHANNEL_READY_GTT,
	IRO_USTORM_FLR_FINAL_ACK_GTT,
	IRO_USTORM_EQE_CONS_GTT,
	IRO_USTORM_ETH_QUEUE_ZONE_GTT,
	IRO_USTORM_COMMON_QUEUE_CONS_GTT,
	IRO_XSTORM_PQ_INFO,
	IRO_XSTORM_INTEG_TEST_DATA,
	IRO_YSTORM_INTEG_TEST_DATA,
	IRO_PSTORM_INTEG_TEST_DATA,
	IRO_TSTORM_INTEG_TEST_DATA,
	IRO_MSTORM_INTEG_TEST_DATA,
	IRO_USTORM_INTEG_TEST_DATA,
	IRO_XSTORM_OVERLAY_BUF_ADDR,
	IRO_YSTORM_OVERLAY_BUF_ADDR,
	IRO_PSTORM_OVERLAY_BUF_ADDR,
	IRO_TSTORM_OVERLAY_BUF_ADDR,
	IRO_MSTORM_OVERLAY_BUF_ADDR,
	IRO_USTORM_OVERLAY_BUF_ADDR,
	IRO_TSTORM_LL2_RX_PRODS_GTT,
	IRO_CORE_LL2_TSTORM_PER_QUEUE_STAT,
	IRO_CORE_LL2_USTORM_PER_QUEUE_STAT,
	IRO_CORE_LL2_PSTORM_PER_QUEUE_STAT,
	IRO_MSTORM_QUEUE_STAT,
	IRO_MSTORM_TPA_TIMEOUT_US,
	IRO_MSTORM_ETH_VF_PRODS,
	IRO_MSTORM_ETH_PF_PRODS_GTT,
	IRO_MSTORM_ETH_PF_STAT,
	IRO_USTORM_QUEUE_STAT,
	IRO_USTORM_ETH_PF_STAT,
	IRO_PSTORM_QUEUE_STAT,
	IRO_PSTORM_ETH_PF_STAT,
	IRO_PSTORM_CTL_FRAME_ETHTYPE_GTT,
	IRO_TSTORM_ETH_PRS_INPUT,
	IRO_ETH_RX_RATE_LIMIT,
	IRO_TSTORM_ETH_RSS_UPDATE_GTT,
	IRO_XSTORM_ETH_QUEUE_ZONE_GTT,
	IRO_YSTORM_TOE_CQ_PROD,
	IRO_USTORM_TOE_CQ_PROD,
	IRO_USTORM_TOE_GRQ_PROD,
	IRO_TSTORM_SCSI_CMDQ_CONS_GTT,
	IRO_TSTORM_SCSI_BDQ_EXT_PROD_GTT,
	IRO_MSTORM_SCSI_BDQ_EXT_PROD_GTT,
	IRO_TSTORM_ISCSI_RX_STATS,
	IRO_MSTORM_ISCSI_RX_STATS,
	IRO_USTORM_ISCSI_RX_STATS,
	IRO_XSTORM_ISCSI_TX_STATS,
	IRO_YSTORM_ISCSI_TX_STATS,
	IRO_PSTORM_ISCSI_TX_STATS,
	IRO_TSTORM_FCOE_RX_STATS,
	IRO_PSTORM_FCOE_TX_STATS,
	IRO_PSTORM_RDMA_QUEUE_STAT,
	IRO_TSTORM_RDMA_QUEUE_STAT,
	IRO_XSTORM_RDMA_ASSERT_LEVEL,
	IRO_YSTORM_RDMA_ASSERT_LEVEL,
	IRO_PSTORM_RDMA_ASSERT_LEVEL,
	IRO_TSTORM_RDMA_ASSERT_LEVEL,
	IRO_MSTORM_RDMA_ASSERT_LEVEL,
	IRO_USTORM_RDMA_ASSERT_LEVEL,
	IRO_XSTORM_IWARP_RXMIT_STATS,
	IRO_TSTORM_ROCE_EVENTS_STAT,
	IRO_YSTORM_ROCE_DCQCN_RECEIVED_STATS,
	IRO_YSTORM_ROCE_ERROR_STATS,
	IRO_PSTORM_ROCE_DCQCN_SENT_STATS,
	IRO_USTORM_ROCE_CQE_STATS,
	IRO_TSTORM_NVMF_PORT_TASKPOOL_PRODUCER_CONSUMER,
};

/* Pstorm LiteL2 queue statistics */
#define CORE_LL2_PSTORM_PER_QUEUE_STAT_OFFSET(core_tx_stats_id) \
        (IRO[IRO_CORE_LL2_PSTORM_PER_QUEUE_STAT].base           \
         + ((core_tx_stats_id) * IRO[IRO_CORE_LL2_PSTORM_PER_QUEUE_STAT].m1))
#define CORE_LL2_PSTORM_PER_QUEUE_STAT_SIZE			(IRO[                                       \
                                                                         IRO_CORE_LL2_PSTORM_PER_QUEUE_STAT \
                                                                 ].size)
/* Tstorm LightL2 queue statistics */
#define CORE_LL2_TSTORM_PER_QUEUE_STAT_OFFSET(core_rx_queue_id) \
        (IRO[IRO_CORE_LL2_TSTORM_PER_QUEUE_STAT].base           \
         + ((core_rx_queue_id) * IRO[IRO_CORE_LL2_TSTORM_PER_QUEUE_STAT].m1))
#define CORE_LL2_TSTORM_PER_QUEUE_STAT_SIZE			(IRO[                                       \
                                                                         IRO_CORE_LL2_TSTORM_PER_QUEUE_STAT \
                                                                 ].size)
/* Ustorm LiteL2 queue statistics */
#define CORE_LL2_USTORM_PER_QUEUE_STAT_OFFSET(core_rx_queue_id) \
        (IRO[IRO_CORE_LL2_USTORM_PER_QUEUE_STAT].base           \
         + ((core_rx_queue_id) * IRO[IRO_CORE_LL2_USTORM_PER_QUEUE_STAT].m1))
#define CORE_LL2_USTORM_PER_QUEUE_STAT_SIZE			(IRO[                                       \
                                                                         IRO_CORE_LL2_USTORM_PER_QUEUE_STAT \
                                                                 ].size)
/* Tstorm Eth limit Rx rate */
#define ETH_RX_RATE_LIMIT_OFFSET(pf_id)  \
        (IRO[IRO_ETH_RX_RATE_LIMIT].base \
         + ((pf_id) * IRO[IRO_ETH_RX_RATE_LIMIT].m1))
#define ETH_RX_RATE_LIMIT_SIZE					(IRO[                          \
                                                                         IRO_ETH_RX_RATE_LIMIT \
                                                                 ].size)
/* Mstorm ETH PF queues producers */
#define MSTORM_ETH_PF_PRODS_GTT_OFFSET(queue_id) \
        (IRO[IRO_MSTORM_ETH_PF_PRODS_GTT].base   \
         + ((queue_id) * IRO[IRO_MSTORM_ETH_PF_PRODS_GTT].m1))
#define MSTORM_ETH_PF_PRODS_GTT_SIZE				(IRO[                                \
                                                                         IRO_MSTORM_ETH_PF_PRODS_GTT \
                                                                 ].size)
/* Mstorm pf statistics */
#define MSTORM_ETH_PF_STAT_OFFSET(pf_id)  \
        (IRO[IRO_MSTORM_ETH_PF_STAT].base \
         + ((pf_id) * IRO[IRO_MSTORM_ETH_PF_STAT].m1))
#define MSTORM_ETH_PF_STAT_SIZE					(IRO[                           \
                                                                         IRO_MSTORM_ETH_PF_STAT \
                                                                 ].size)
/* Mstorm ETH VF queues producers offset in RAM. Used in default VF zone size mode. */
#define MSTORM_ETH_VF_PRODS_OFFSET(vf_id, vf_queue_id) \
        (IRO[IRO_MSTORM_ETH_VF_PRODS].base             \
         + ((vf_id) * IRO[IRO_MSTORM_ETH_VF_PRODS].m1) \
         + ((vf_queue_id) * IRO[IRO_MSTORM_ETH_VF_PRODS].m2))
#define MSTORM_ETH_VF_PRODS_SIZE				(IRO[                            \
                                                                         IRO_MSTORM_ETH_VF_PRODS \
                                                                 ].size)
/* Mstorm Integration Test Data */
#define MSTORM_INTEG_TEST_DATA_OFFSET				(IRO[                               \
                                                                         IRO_MSTORM_INTEG_TEST_DATA \
                                                                 ].base)
#define MSTORM_INTEG_TEST_DATA_SIZE				(IRO[                               \
                                                                         IRO_MSTORM_INTEG_TEST_DATA \
                                                                 ].size)
/* Mstorm iSCSI RX stats */
#define MSTORM_ISCSI_RX_STATS_OFFSET(storage_func_id) \
        (IRO[IRO_MSTORM_ISCSI_RX_STATS].base          \
         + ((storage_func_id) * IRO[IRO_MSTORM_ISCSI_RX_STATS].m1))
#define MSTORM_ISCSI_RX_STATS_SIZE				(IRO[                              \
                                                                         IRO_MSTORM_ISCSI_RX_STATS \
                                                                 ].size)
/* Mstorm overlay buffer host address */
#define MSTORM_OVERLAY_BUF_ADDR_OFFSET				(IRO[                                \
                                                                         IRO_MSTORM_OVERLAY_BUF_ADDR \
                                                                 ].base)
#define MSTORM_OVERLAY_BUF_ADDR_SIZE				(IRO[                                \
                                                                         IRO_MSTORM_OVERLAY_BUF_ADDR \
                                                                 ].size)
/* Mstorm queue statistics */
#define MSTORM_QUEUE_STAT_OFFSET(stat_counter_id) \
        (IRO[IRO_MSTORM_QUEUE_STAT].base          \
         + ((stat_counter_id) * IRO[IRO_MSTORM_QUEUE_STAT].m1))
#define MSTORM_QUEUE_STAT_SIZE					(IRO[                          \
                                                                         IRO_MSTORM_QUEUE_STAT \
                                                                 ].size)
/* Mstorm error level for assert */
#define MSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id)  \
        (IRO[IRO_MSTORM_RDMA_ASSERT_LEVEL].base \
         + ((pf_id) * IRO[IRO_MSTORM_RDMA_ASSERT_LEVEL].m1))
#define MSTORM_RDMA_ASSERT_LEVEL_SIZE				(IRO[                                 \
                                                                         IRO_MSTORM_RDMA_ASSERT_LEVEL \
                                                                 ].size)
/* Mstorm bdq-external-producer of given BDQ resource ID, BDqueue-id */
#define MSTORM_SCSI_BDQ_EXT_PROD_GTT_OFFSET(storage_func_id, bdq_id)      \
        (IRO[IRO_MSTORM_SCSI_BDQ_EXT_PROD_GTT].base                       \
         + ((storage_func_id) * IRO[IRO_MSTORM_SCSI_BDQ_EXT_PROD_GTT].m1) \
         + ((bdq_id) * IRO[IRO_MSTORM_SCSI_BDQ_EXT_PROD_GTT].m2))
#define MSTORM_SCSI_BDQ_EXT_PROD_GTT_SIZE			(IRO[                                     \
                                                                         IRO_MSTORM_SCSI_BDQ_EXT_PROD_GTT \
                                                                 ].size)
/* TPA aggregation timeout in us resolution (on ASIC) */
#define MSTORM_TPA_TIMEOUT_US_OFFSET				(IRO[                              \
                                                                         IRO_MSTORM_TPA_TIMEOUT_US \
                                                                 ].base)
#define MSTORM_TPA_TIMEOUT_US_SIZE				(IRO[                              \
                                                                         IRO_MSTORM_TPA_TIMEOUT_US \
                                                                 ].size)
/* Control frame's EthType configuration for TX control frame security */
#define PSTORM_CTL_FRAME_ETHTYPE_GTT_OFFSET(ethType_id) \
        (IRO[IRO_PSTORM_CTL_FRAME_ETHTYPE_GTT].base     \
         + ((ethType_id) * IRO[IRO_PSTORM_CTL_FRAME_ETHTYPE_GTT].m1))
#define PSTORM_CTL_FRAME_ETHTYPE_GTT_SIZE			(IRO[                                     \
                                                                         IRO_PSTORM_CTL_FRAME_ETHTYPE_GTT \
                                                                 ].size)
/* Pstorm pf statistics */
#define PSTORM_ETH_PF_STAT_OFFSET(pf_id)  \
        (IRO[IRO_PSTORM_ETH_PF_STAT].base \
         + ((pf_id) * IRO[IRO_PSTORM_ETH_PF_STAT].m1))
#define PSTORM_ETH_PF_STAT_SIZE					(IRO[                           \
                                                                         IRO_PSTORM_ETH_PF_STAT \
                                                                 ].size)
/* Pstorm FCoE TX stats */
#define PSTORM_FCOE_TX_STATS_OFFSET(pf_id)  \
        (IRO[IRO_PSTORM_FCOE_TX_STATS].base \
         + ((pf_id) * IRO[IRO_PSTORM_FCOE_TX_STATS].m1))
#define PSTORM_FCOE_TX_STATS_SIZE				(IRO[                             \
                                                                         IRO_PSTORM_FCOE_TX_STATS \
                                                                 ].size)
/* Pstorm Integration Test Data */
#define PSTORM_INTEG_TEST_DATA_OFFSET				(IRO[                               \
                                                                         IRO_PSTORM_INTEG_TEST_DATA \
                                                                 ].base)
#define PSTORM_INTEG_TEST_DATA_SIZE				(IRO[                               \
                                                                         IRO_PSTORM_INTEG_TEST_DATA \
                                                                 ].size)
/* Pstorm iSCSI TX stats */
#define PSTORM_ISCSI_TX_STATS_OFFSET(storage_func_id) \
        (IRO[IRO_PSTORM_ISCSI_TX_STATS].base          \
         + ((storage_func_id) * IRO[IRO_PSTORM_ISCSI_TX_STATS].m1))
#define PSTORM_ISCSI_TX_STATS_SIZE				(IRO[                              \
                                                                         IRO_PSTORM_ISCSI_TX_STATS \
                                                                 ].size)
/* Pstorm overlay buffer host address */
#define PSTORM_OVERLAY_BUF_ADDR_OFFSET				(IRO[                                \
                                                                         IRO_PSTORM_OVERLAY_BUF_ADDR \
                                                                 ].base)
#define PSTORM_OVERLAY_BUF_ADDR_SIZE				(IRO[                                \
                                                                         IRO_PSTORM_OVERLAY_BUF_ADDR \
                                                                 ].size)
/* Pstorm LL2 packet duplication configuration. Use pstorm_pkt_dup_cfg data type. */
#define PSTORM_PKT_DUPLICATION_CFG_OFFSET(pf_id)  \
        (IRO[IRO_PSTORM_PKT_DUPLICATION_CFG].base \
         + ((pf_id) * IRO[IRO_PSTORM_PKT_DUPLICATION_CFG].m1))
#define PSTORM_PKT_DUPLICATION_CFG_SIZE				(IRO[                                   \
                                                                         IRO_PSTORM_PKT_DUPLICATION_CFG \
                                                                 ].size)
/* Pstorm queue statistics */
#define PSTORM_QUEUE_STAT_OFFSET(stat_counter_id) \
        (IRO[IRO_PSTORM_QUEUE_STAT].base          \
         + ((stat_counter_id) * IRO[IRO_PSTORM_QUEUE_STAT].m1))
#define PSTORM_QUEUE_STAT_SIZE					(IRO[                          \
                                                                         IRO_PSTORM_QUEUE_STAT \
                                                                 ].size)
/* Pstorm error level for assert */
#define PSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id)  \
        (IRO[IRO_PSTORM_RDMA_ASSERT_LEVEL].base \
         + ((pf_id) * IRO[IRO_PSTORM_RDMA_ASSERT_LEVEL].m1))
#define PSTORM_RDMA_ASSERT_LEVEL_SIZE				(IRO[                                 \
                                                                         IRO_PSTORM_RDMA_ASSERT_LEVEL \
                                                                 ].size)
/* Pstorm RDMA queue statistics */
#define PSTORM_RDMA_QUEUE_STAT_OFFSET(rdma_stat_counter_id) \
        (IRO[IRO_PSTORM_RDMA_QUEUE_STAT].base               \
         + ((rdma_stat_counter_id) * IRO[IRO_PSTORM_RDMA_QUEUE_STAT].m1))
#define PSTORM_RDMA_QUEUE_STAT_SIZE				(IRO[                               \
                                                                         IRO_PSTORM_RDMA_QUEUE_STAT \
                                                                 ].size)
/* DCQCN Sent Statistics */
#define PSTORM_ROCE_DCQCN_SENT_STATS_OFFSET(roce_pf_id) \
        (IRO[IRO_PSTORM_ROCE_DCQCN_SENT_STATS].base     \
         + ((roce_pf_id) * IRO[IRO_PSTORM_ROCE_DCQCN_SENT_STATS].m1))
#define PSTORM_ROCE_DCQCN_SENT_STATS_SIZE			(IRO[                                     \
                                                                         IRO_PSTORM_ROCE_DCQCN_SENT_STATS \
                                                                 ].size)
/* Tstorm last parser message */
#define TSTORM_ETH_PRS_INPUT_OFFSET				(IRO[                             \
                                                                         IRO_TSTORM_ETH_PRS_INPUT \
                                                                 ].base)
#define TSTORM_ETH_PRS_INPUT_SIZE				(IRO[                             \
                                                                         IRO_TSTORM_ETH_PRS_INPUT \
                                                                 ].size)
/* RSS indirection table entry update command per PF offset in TSTORM PF BAR0. Use eth_tstorm_rss_update_data for update. */
#define TSTORM_ETH_RSS_UPDATE_GTT_OFFSET(pf_id)  \
        (IRO[IRO_TSTORM_ETH_RSS_UPDATE_GTT].base \
         + ((pf_id) * IRO[IRO_TSTORM_ETH_RSS_UPDATE_GTT].m1))
#define TSTORM_ETH_RSS_UPDATE_GTT_SIZE				(IRO[                                  \
                                                                         IRO_TSTORM_ETH_RSS_UPDATE_GTT \
                                                                 ].size)
/* Tstorm FCoE RX stats */
#define TSTORM_FCOE_RX_STATS_OFFSET(pf_id)  \
        (IRO[IRO_TSTORM_FCOE_RX_STATS].base \
         + ((pf_id) * IRO[IRO_TSTORM_FCOE_RX_STATS].m1))
#define TSTORM_FCOE_RX_STATS_SIZE				(IRO[                             \
                                                                         IRO_TSTORM_FCOE_RX_STATS \
                                                                 ].size)
/* Tstorm Integration Test Data */
#define TSTORM_INTEG_TEST_DATA_OFFSET				(IRO[                               \
                                                                         IRO_TSTORM_INTEG_TEST_DATA \
                                                                 ].base)
#define TSTORM_INTEG_TEST_DATA_SIZE				(IRO[                               \
                                                                         IRO_TSTORM_INTEG_TEST_DATA \
                                                                 ].size)
/* Tstorm iSCSI RX stats */
#define TSTORM_ISCSI_RX_STATS_OFFSET(storage_func_id) \
        (IRO[IRO_TSTORM_ISCSI_RX_STATS].base          \
         + ((storage_func_id) * IRO[IRO_TSTORM_ISCSI_RX_STATS].m1))
#define TSTORM_ISCSI_RX_STATS_SIZE				(IRO[                              \
                                                                         IRO_TSTORM_ISCSI_RX_STATS \
                                                                 ].size)
/* Tstorm ll2 port statistics */
#define TSTORM_LL2_PORT_STAT_OFFSET(port_id) \
        (IRO[IRO_TSTORM_LL2_PORT_STAT].base  \
         + ((port_id) * IRO[IRO_TSTORM_LL2_PORT_STAT].m1))
#define TSTORM_LL2_PORT_STAT_SIZE				(IRO[                             \
                                                                         IRO_TSTORM_LL2_PORT_STAT \
                                                                 ].size)
/* Tstorm producers */
#define TSTORM_LL2_RX_PRODS_GTT_OFFSET(core_rx_queue_id) \
        (IRO[IRO_TSTORM_LL2_RX_PRODS_GTT].base           \
         + ((core_rx_queue_id) * IRO[IRO_TSTORM_LL2_RX_PRODS_GTT].m1))
#define TSTORM_LL2_RX_PRODS_GTT_SIZE				(IRO[                                \
                                                                         IRO_TSTORM_LL2_RX_PRODS_GTT \
                                                                 ].size)
/* Tstorm NVMf per port per producer consumer data */
#define TSTORM_NVMF_PORT_TASKPOOL_PRODUCER_CONSUMER_OFFSET(port_num_id,    \
                                                           taskpool_index) \
        (IRO[IRO_TSTORM_NVMF_PORT_TASKPOOL_PRODUCER_CONSUMER].base         \
         + ((port_num_id) *                                                \
            IRO[IRO_TSTORM_NVMF_PORT_TASKPOOL_PRODUCER_CONSUMER].m1)       \
         + ((taskpool_index) *                                             \
            IRO[IRO_TSTORM_NVMF_PORT_TASKPOOL_PRODUCER_CONSUMER].m2))
#define TSTORM_NVMF_PORT_TASKPOOL_PRODUCER_CONSUMER_SIZE	(IRO[                                                    \
                                                                         IRO_TSTORM_NVMF_PORT_TASKPOOL_PRODUCER_CONSUMER \
                                                                 ].size)
/* Tstorm overlay buffer host address */
#define TSTORM_OVERLAY_BUF_ADDR_OFFSET				(IRO[                                \
                                                                         IRO_TSTORM_OVERLAY_BUF_ADDR \
                                                                 ].base)
#define TSTORM_OVERLAY_BUF_ADDR_SIZE				(IRO[                                \
                                                                         IRO_TSTORM_OVERLAY_BUF_ADDR \
                                                                 ].size)
/* Tstorm LL2 packet duplication configuration. Use tstorm_pkt_dup_cfg data type. */
#define TSTORM_PKT_DUPLICATION_CFG_OFFSET(pf_id)  \
        (IRO[IRO_TSTORM_PKT_DUPLICATION_CFG].base \
         + ((pf_id) * IRO[IRO_TSTORM_PKT_DUPLICATION_CFG].m1))
#define TSTORM_PKT_DUPLICATION_CFG_SIZE				(IRO[                                   \
                                                                         IRO_TSTORM_PKT_DUPLICATION_CFG \
                                                                 ].size)
/* Tstorm port statistics */
#define TSTORM_PORT_STAT_OFFSET(port_id) \
        (IRO[IRO_TSTORM_PORT_STAT].base  \
         + ((port_id) * IRO[IRO_TSTORM_PORT_STAT].m1))
#define TSTORM_PORT_STAT_SIZE					(IRO[                         \
                                                                         IRO_TSTORM_PORT_STAT \
                                                                 ].size)
/* Tstorm error level for assert */
#define TSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id)  \
        (IRO[IRO_TSTORM_RDMA_ASSERT_LEVEL].base \
         + ((pf_id) * IRO[IRO_TSTORM_RDMA_ASSERT_LEVEL].m1))
#define TSTORM_RDMA_ASSERT_LEVEL_SIZE				(IRO[                                 \
                                                                         IRO_TSTORM_RDMA_ASSERT_LEVEL \
                                                                 ].size)
/* Tstorm RDMA queue statistics */
#define TSTORM_RDMA_QUEUE_STAT_OFFSET(rdma_stat_counter_id) \
        (IRO[IRO_TSTORM_RDMA_QUEUE_STAT].base               \
         + ((rdma_stat_counter_id) * IRO[IRO_TSTORM_RDMA_QUEUE_STAT].m1))
#define TSTORM_RDMA_QUEUE_STAT_SIZE				(IRO[                               \
                                                                         IRO_TSTORM_RDMA_QUEUE_STAT \
                                                                 ].size)
/* Tstorm RoCE Event Statistics */
#define TSTORM_ROCE_EVENTS_STAT_OFFSET(roce_pf_id) \
        (IRO[IRO_TSTORM_ROCE_EVENTS_STAT].base     \
         + ((roce_pf_id) * IRO[IRO_TSTORM_ROCE_EVENTS_STAT].m1))
#define TSTORM_ROCE_EVENTS_STAT_SIZE				(IRO[                                \
                                                                         IRO_TSTORM_ROCE_EVENTS_STAT \
                                                                 ].size)
/* Tstorm (reflects M-Storm) bdq-external-producer of given function ID, BDqueue-id */
#define TSTORM_SCSI_BDQ_EXT_PROD_GTT_OFFSET(storage_func_id, bdq_id)      \
        (IRO[IRO_TSTORM_SCSI_BDQ_EXT_PROD_GTT].base                       \
         + ((storage_func_id) * IRO[IRO_TSTORM_SCSI_BDQ_EXT_PROD_GTT].m1) \
         + ((bdq_id) * IRO[IRO_TSTORM_SCSI_BDQ_EXT_PROD_GTT].m2))
#define TSTORM_SCSI_BDQ_EXT_PROD_GTT_SIZE			(IRO[                                     \
                                                                         IRO_TSTORM_SCSI_BDQ_EXT_PROD_GTT \
                                                                 ].size)
/* Tstorm cmdq-cons of given command queue-id */
#define TSTORM_SCSI_CMDQ_CONS_GTT_OFFSET(cmdq_queue_id) \
        (IRO[IRO_TSTORM_SCSI_CMDQ_CONS_GTT].base        \
         + ((cmdq_queue_id) * IRO[IRO_TSTORM_SCSI_CMDQ_CONS_GTT].m1))
#define TSTORM_SCSI_CMDQ_CONS_GTT_SIZE				(IRO[                                  \
                                                                         IRO_TSTORM_SCSI_CMDQ_CONS_GTT \
                                                                 ].size)
/* Ustorm Common Queue ring consumer */
#define USTORM_COMMON_QUEUE_CONS_GTT_OFFSET(queue_zone_id) \
        (IRO[IRO_USTORM_COMMON_QUEUE_CONS_GTT].base        \
         + ((queue_zone_id) * IRO[IRO_USTORM_COMMON_QUEUE_CONS_GTT].m1))
#define USTORM_COMMON_QUEUE_CONS_GTT_SIZE			(IRO[                                     \
                                                                         IRO_USTORM_COMMON_QUEUE_CONS_GTT \
                                                                 ].size)
/* Ustorm Event ring consumer */
#define USTORM_EQE_CONS_GTT_OFFSET(pf_id)  \
        (IRO[IRO_USTORM_EQE_CONS_GTT].base \
         + ((pf_id) * IRO[IRO_USTORM_EQE_CONS_GTT].m1))
#define USTORM_EQE_CONS_GTT_SIZE				(IRO[                            \
                                                                         IRO_USTORM_EQE_CONS_GTT \
                                                                 ].size)
/* Ustorm pf statistics */
#define USTORM_ETH_PF_STAT_OFFSET(pf_id)  \
        (IRO[IRO_USTORM_ETH_PF_STAT].base \
         + ((pf_id) * IRO[IRO_USTORM_ETH_PF_STAT].m1))
#define USTORM_ETH_PF_STAT_SIZE					(IRO[                           \
                                                                         IRO_USTORM_ETH_PF_STAT \
                                                                 ].size)
/* Ustorm eth queue zone */
#define USTORM_ETH_QUEUE_ZONE_GTT_OFFSET(queue_zone_id) \
        (IRO[IRO_USTORM_ETH_QUEUE_ZONE_GTT].base        \
         + ((queue_zone_id) * IRO[IRO_USTORM_ETH_QUEUE_ZONE_GTT].m1))
#define USTORM_ETH_QUEUE_ZONE_GTT_SIZE				(IRO[                                  \
                                                                         IRO_USTORM_ETH_QUEUE_ZONE_GTT \
                                                                 ].size)
/* Ustorm Final flr cleanup ack */
#define USTORM_FLR_FINAL_ACK_GTT_OFFSET(pf_id)  \
        (IRO[IRO_USTORM_FLR_FINAL_ACK_GTT].base \
         + ((pf_id) * IRO[IRO_USTORM_FLR_FINAL_ACK_GTT].m1))
#define USTORM_FLR_FINAL_ACK_GTT_SIZE				(IRO[                                 \
                                                                         IRO_USTORM_FLR_FINAL_ACK_GTT \
                                                                 ].size)
/* Ustorm Integration Test Data */
#define USTORM_INTEG_TEST_DATA_OFFSET				(IRO[                               \
                                                                         IRO_USTORM_INTEG_TEST_DATA \
                                                                 ].base)
#define USTORM_INTEG_TEST_DATA_SIZE				(IRO[                               \
                                                                         IRO_USTORM_INTEG_TEST_DATA \
                                                                 ].size)
/* Ustorm iSCSI RX stats */
#define USTORM_ISCSI_RX_STATS_OFFSET(storage_func_id) \
        (IRO[IRO_USTORM_ISCSI_RX_STATS].base          \
         + ((storage_func_id) * IRO[IRO_USTORM_ISCSI_RX_STATS].m1))
#define USTORM_ISCSI_RX_STATS_SIZE				(IRO[                              \
                                                                         IRO_USTORM_ISCSI_RX_STATS \
                                                                 ].size)
/* Ustorm overlay buffer host address */
#define USTORM_OVERLAY_BUF_ADDR_OFFSET				(IRO[                                \
                                                                         IRO_USTORM_OVERLAY_BUF_ADDR \
                                                                 ].base)
#define USTORM_OVERLAY_BUF_ADDR_SIZE				(IRO[                                \
                                                                         IRO_USTORM_OVERLAY_BUF_ADDR \
                                                                 ].size)
/* Ustorm queue statistics */
#define USTORM_QUEUE_STAT_OFFSET(stat_counter_id) \
        (IRO[IRO_USTORM_QUEUE_STAT].base          \
         + ((stat_counter_id) * IRO[IRO_USTORM_QUEUE_STAT].m1))
#define USTORM_QUEUE_STAT_SIZE					(IRO[                          \
                                                                         IRO_USTORM_QUEUE_STAT \
                                                                 ].size)
/* Ustorm error level for assert */
#define USTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id)  \
        (IRO[IRO_USTORM_RDMA_ASSERT_LEVEL].base \
         + ((pf_id) * IRO[IRO_USTORM_RDMA_ASSERT_LEVEL].m1))
#define USTORM_RDMA_ASSERT_LEVEL_SIZE				(IRO[                                 \
                                                                         IRO_USTORM_RDMA_ASSERT_LEVEL \
                                                                 ].size)
/* RoCE CQEs Statistics */
#define USTORM_ROCE_CQE_STATS_OFFSET(roce_pf_id) \
        (IRO[IRO_USTORM_ROCE_CQE_STATS].base     \
         + ((roce_pf_id) * IRO[IRO_USTORM_ROCE_CQE_STATS].m1))
#define USTORM_ROCE_CQE_STATS_SIZE				(IRO[                              \
                                                                         IRO_USTORM_ROCE_CQE_STATS \
                                                                 ].size)
/* Ustorm cqe producer */
#define USTORM_TOE_CQ_PROD_OFFSET(rss_id) \
        (IRO[IRO_USTORM_TOE_CQ_PROD].base \
         + ((rss_id) * IRO[IRO_USTORM_TOE_CQ_PROD].m1))
#define USTORM_TOE_CQ_PROD_SIZE					(IRO[                           \
                                                                         IRO_USTORM_TOE_CQ_PROD \
                                                                 ].size)
/* Ustorm grq producer */
#define USTORM_TOE_GRQ_PROD_OFFSET(pf_id)  \
        (IRO[IRO_USTORM_TOE_GRQ_PROD].base \
         + ((pf_id) * IRO[IRO_USTORM_TOE_GRQ_PROD].m1))
#define USTORM_TOE_GRQ_PROD_SIZE				(IRO[                            \
                                                                         IRO_USTORM_TOE_GRQ_PROD \
                                                                 ].size)
/* Ustorm VF-PF Channel ready flag */
#define USTORM_VF_PF_CHANNEL_READY_GTT_OFFSET(vf_id)  \
        (IRO[IRO_USTORM_VF_PF_CHANNEL_READY_GTT].base \
         + ((vf_id) * IRO[IRO_USTORM_VF_PF_CHANNEL_READY_GTT].m1))
#define USTORM_VF_PF_CHANNEL_READY_GTT_SIZE			(IRO[                                       \
                                                                         IRO_USTORM_VF_PF_CHANNEL_READY_GTT \
                                                                 ].size)
/* Xstorm queue zone */
#define XSTORM_ETH_QUEUE_ZONE_GTT_OFFSET(queue_id) \
        (IRO[IRO_XSTORM_ETH_QUEUE_ZONE_GTT].base   \
         + ((queue_id) * IRO[IRO_XSTORM_ETH_QUEUE_ZONE_GTT].m1))
#define XSTORM_ETH_QUEUE_ZONE_GTT_SIZE				(IRO[                                  \
                                                                         IRO_XSTORM_ETH_QUEUE_ZONE_GTT \
                                                                 ].size)
/* Xstorm Integration Test Data */
#define XSTORM_INTEG_TEST_DATA_OFFSET				(IRO[                               \
                                                                         IRO_XSTORM_INTEG_TEST_DATA \
                                                                 ].base)
#define XSTORM_INTEG_TEST_DATA_SIZE				(IRO[                               \
                                                                         IRO_XSTORM_INTEG_TEST_DATA \
                                                                 ].size)
/* Xstorm iSCSI TX stats */
#define XSTORM_ISCSI_TX_STATS_OFFSET(storage_func_id) \
        (IRO[IRO_XSTORM_ISCSI_TX_STATS].base          \
         + ((storage_func_id) * IRO[IRO_XSTORM_ISCSI_TX_STATS].m1))
#define XSTORM_ISCSI_TX_STATS_SIZE				(IRO[                              \
                                                                         IRO_XSTORM_ISCSI_TX_STATS \
                                                                 ].size)
/* Xstorm iWARP rxmit stats */
#define XSTORM_IWARP_RXMIT_STATS_OFFSET(pf_id)  \
        (IRO[IRO_XSTORM_IWARP_RXMIT_STATS].base \
         + ((pf_id) * IRO[IRO_XSTORM_IWARP_RXMIT_STATS].m1))
#define XSTORM_IWARP_RXMIT_STATS_SIZE				(IRO[                                 \
                                                                         IRO_XSTORM_IWARP_RXMIT_STATS \
                                                                 ].size)
/* Xstorm overlay buffer host address */
#define XSTORM_OVERLAY_BUF_ADDR_OFFSET				(IRO[                                \
                                                                         IRO_XSTORM_OVERLAY_BUF_ADDR \
                                                                 ].base)
#define XSTORM_OVERLAY_BUF_ADDR_SIZE				(IRO[                                \
                                                                         IRO_XSTORM_OVERLAY_BUF_ADDR \
                                                                 ].size)
/* Xstorm common PQ info */
#define XSTORM_PQ_INFO_OFFSET(pq_id)  \
        (IRO[IRO_XSTORM_PQ_INFO].base \
         + ((pq_id) * IRO[IRO_XSTORM_PQ_INFO].m1))
#define XSTORM_PQ_INFO_SIZE					(IRO[                       \
                                                                         IRO_XSTORM_PQ_INFO \
                                                                 ].size)
/* Xstorm error level for assert */
#define XSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id)  \
        (IRO[IRO_XSTORM_RDMA_ASSERT_LEVEL].base \
         + ((pf_id) * IRO[IRO_XSTORM_RDMA_ASSERT_LEVEL].m1))
#define XSTORM_RDMA_ASSERT_LEVEL_SIZE				(IRO[                                 \
                                                                         IRO_XSTORM_RDMA_ASSERT_LEVEL \
                                                                 ].size)
/* Ystorm flow control mode. Use enum fw_flow_ctrl_mode */
#define YSTORM_FLOW_CONTROL_MODE_GTT_OFFSET			(IRO[                                     \
                                                                         IRO_YSTORM_FLOW_CONTROL_MODE_GTT \
                                                                 ].base)
#define YSTORM_FLOW_CONTROL_MODE_GTT_SIZE			(IRO[                                     \
                                                                         IRO_YSTORM_FLOW_CONTROL_MODE_GTT \
                                                                 ].size)
/* Ystorm Integration Test Data */
#define YSTORM_INTEG_TEST_DATA_OFFSET				(IRO[                               \
                                                                         IRO_YSTORM_INTEG_TEST_DATA \
                                                                 ].base)
#define YSTORM_INTEG_TEST_DATA_SIZE				(IRO[                               \
                                                                         IRO_YSTORM_INTEG_TEST_DATA \
                                                                 ].size)
/* Ystorm iSCSI TX stats */
#define YSTORM_ISCSI_TX_STATS_OFFSET(storage_func_id) \
        (IRO[IRO_YSTORM_ISCSI_TX_STATS].base          \
         + ((storage_func_id) * IRO[IRO_YSTORM_ISCSI_TX_STATS].m1))
#define YSTORM_ISCSI_TX_STATS_SIZE				(IRO[                              \
                                                                         IRO_YSTORM_ISCSI_TX_STATS \
                                                                 ].size)
/* Ystorm overlay buffer host address */
#define YSTORM_OVERLAY_BUF_ADDR_OFFSET				(IRO[                                \
                                                                         IRO_YSTORM_OVERLAY_BUF_ADDR \
                                                                 ].base)
#define YSTORM_OVERLAY_BUF_ADDR_SIZE				(IRO[                                \
                                                                         IRO_YSTORM_OVERLAY_BUF_ADDR \
                                                                 ].size)
/* Ystorm error level for assert */
#define YSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id)  \
        (IRO[IRO_YSTORM_RDMA_ASSERT_LEVEL].base \
         + ((pf_id) * IRO[IRO_YSTORM_RDMA_ASSERT_LEVEL].m1))
#define YSTORM_RDMA_ASSERT_LEVEL_SIZE				(IRO[                                 \
                                                                         IRO_YSTORM_RDMA_ASSERT_LEVEL \
                                                                 ].size)
/* DCQCN Received Statistics */
#define YSTORM_ROCE_DCQCN_RECEIVED_STATS_OFFSET(roce_pf_id) \
        (IRO[IRO_YSTORM_ROCE_DCQCN_RECEIVED_STATS].base     \
         + ((roce_pf_id) * IRO[IRO_YSTORM_ROCE_DCQCN_RECEIVED_STATS].m1))
#define YSTORM_ROCE_DCQCN_RECEIVED_STATS_SIZE			(IRO[                                         \
                                                                         IRO_YSTORM_ROCE_DCQCN_RECEIVED_STATS \
                                                                 ].size)
/* RoCE Error Statistics */
#define YSTORM_ROCE_ERROR_STATS_OFFSET(roce_pf_id) \
        (IRO[IRO_YSTORM_ROCE_ERROR_STATS].base     \
         + ((roce_pf_id) * IRO[IRO_YSTORM_ROCE_ERROR_STATS].m1))
#define YSTORM_ROCE_ERROR_STATS_SIZE				(IRO[                                \
                                                                         IRO_YSTORM_ROCE_ERROR_STATS \
                                                                 ].size)
/* Ystorm cqe producer */
#define YSTORM_TOE_CQ_PROD_OFFSET(rss_id) \
        (IRO[IRO_YSTORM_TOE_CQ_PROD].base \
         + ((rss_id) * IRO[IRO_YSTORM_TOE_CQ_PROD].m1))
#define YSTORM_TOE_CQ_PROD_SIZE					(IRO[                           \
                                                                         IRO_YSTORM_TOE_CQ_PROD \
                                                                 ].size)
/* Ystorm VF-PF Channel VF Zone */
#define YSTORM_VF_ZONE_OFFSET(vf_id)  \
        (IRO[IRO_YSTORM_VF_ZONE].base \
         + ((vf_id) * IRO[IRO_YSTORM_VF_ZONE].m1))
#define YSTORM_VF_ZONE_SIZE					(IRO[                       \
                                                                         IRO_YSTORM_VF_ZONE \
                                                                 ].size)

#endif

#ifndef __IRO_VALUES_H__
#define __IRO_VALUES_H__

/* Per-chip offsets in iro_arr in dwords */
#define E4_IRO_ARR_OFFSET    0

/* IRO Array */
ARRAY_DECL u32 iro_arr[] = {
	/* E4 */
	0x00000000, 0x00000000, 0x00080000,	/* YSTORM_FLOW_CONTROL_MODE_GTT_OFFSET, offset=0x0, size=0x8 */
	0x00006ac0, 0x00000008, 0x00080000,	/* YSTORM_VF_ZONE_OFFSET(vf_id), offset=0x6ac0, mult1=0x8, size=0x8 */
	0x00004478, 0x00000008, 0x00080000,	/* PSTORM_PKT_DUPLICATION_CFG_OFFSET(pf_id), offset=0x4478, mult1=0x8, size=0x8 */
	0x00003288, 0x00000098, 0x00980000,	/* TSTORM_PORT_STAT_OFFSET(port_id), offset=0x3288, mult1=0x98, size=0x98 */
	0x000058e8, 0x00000020, 0x00200000,	/* TSTORM_LL2_PORT_STAT_OFFSET(port_id), offset=0x58e8, mult1=0x20, size=0x20 */
	0x00003188, 0x00000008, 0x00080000,	/* TSTORM_PKT_DUPLICATION_CFG_OFFSET(pf_id), offset=0x3188, mult1=0x8, size=0x8 */
	0x00000b00, 0x00000008, 0x00040000,	/* USTORM_VF_PF_CHANNEL_READY_GTT_OFFSET(vf_id), offset=0xb00, mult1=0x8, size=0x4 */
	0x00000a80, 0x00000008, 0x00040000,	/* USTORM_FLR_FINAL_ACK_GTT_OFFSET(pf_id), offset=0xa80, mult1=0x8, size=0x4 */
	0x00000000, 0x00000008, 0x00020000,	/* USTORM_EQE_CONS_GTT_OFFSET(pf_id), offset=0x0, mult1=0x8, size=0x2 */
	0x00000080, 0x00000008, 0x00040000,	/* USTORM_ETH_QUEUE_ZONE_GTT_OFFSET(queue_zone_id), offset=0x80, mult1=0x8, size=0x4 */
	0x00000084, 0x00000008, 0x00020000,	/* USTORM_COMMON_QUEUE_CONS_GTT_OFFSET(queue_zone_id), offset=0x84, mult1=0x8, size=0x2 */
	0x00005898, 0x00000004, 0x00040000,	/* XSTORM_PQ_INFO_OFFSET(pq_id), offset=0x5898, mult1=0x4, size=0x4 */
	0x00004f50, 0x00000000, 0x00780000,	/* XSTORM_INTEG_TEST_DATA_OFFSET, offset=0x4f50, size=0x78 */
	0x00003e40, 0x00000000, 0x00780000,	/* YSTORM_INTEG_TEST_DATA_OFFSET, offset=0x3e40, size=0x78 */
	0x00004500, 0x00000000, 0x00780000,	/* PSTORM_INTEG_TEST_DATA_OFFSET, offset=0x4500, size=0x78 */
	0x00003210, 0x00000000, 0x00780000,	/* TSTORM_INTEG_TEST_DATA_OFFSET, offset=0x3210, size=0x78 */
	0x00003b50, 0x00000000, 0x00780000,	/* MSTORM_INTEG_TEST_DATA_OFFSET, offset=0x3b50, size=0x78 */
	0x00007f58, 0x00000000, 0x00780000,	/* USTORM_INTEG_TEST_DATA_OFFSET, offset=0x7f58, size=0x78 */
	0x000060d8, 0x00000000, 0x00080000,	/* XSTORM_OVERLAY_BUF_ADDR_OFFSET, offset=0x60d8, size=0x8 */
	0x00007100, 0x00000000, 0x00080000,	/* YSTORM_OVERLAY_BUF_ADDR_OFFSET, offset=0x7100, size=0x8 */
	0x0000af20, 0x00000000, 0x00080000,	/* PSTORM_OVERLAY_BUF_ADDR_OFFSET, offset=0xaf20, size=0x8 */
	0x000043d8, 0x00000000, 0x00080000,	/* TSTORM_OVERLAY_BUF_ADDR_OFFSET, offset=0x43d8, size=0x8 */
	0x0000a5a0, 0x00000000, 0x00080000,	/* MSTORM_OVERLAY_BUF_ADDR_OFFSET, offset=0xa5a0, size=0x8 */
	0x0000bde8, 0x00000000, 0x00080000,	/* USTORM_OVERLAY_BUF_ADDR_OFFSET, offset=0xbde8, size=0x8 */
	0x00000020, 0x00000004, 0x00040000,	/* TSTORM_LL2_RX_PRODS_GTT_OFFSET(core_rx_queue_id), offset=0x20, mult1=0x4, size=0x4 */
	0x000056c8, 0x00000010, 0x00100000,	/* CORE_LL2_TSTORM_PER_QUEUE_STAT_OFFSET(core_rx_queue_id), offset=0x56c8, mult1=0x10, size=0x10 */
	0x0000c210, 0x00000030, 0x00300000,	/* CORE_LL2_USTORM_PER_QUEUE_STAT_OFFSET(core_rx_queue_id), offset=0xc210, mult1=0x30, size=0x30 */
	0x0000b108, 0x00000038, 0x00380000,	/* CORE_LL2_PSTORM_PER_QUEUE_STAT_OFFSET(core_tx_stats_id), offset=0xb108, mult1=0x38, size=0x38 */
	0x00003d20, 0x00000080, 0x00400000,	/* MSTORM_QUEUE_STAT_OFFSET(stat_counter_id), offset=0x3d20, mult1=0x80, size=0x40 */
	0x0000bf60, 0x00000000, 0x00040000,	/* MSTORM_TPA_TIMEOUT_US_OFFSET, offset=0xbf60, size=0x4 */
	0x00004560, 0x00040080, 0x00040000,	/* MSTORM_ETH_VF_PRODS_OFFSET(vf_id,vf_queue_id), offset=0x4560, mult1=0x80, mult2=0x4, size=0x4 */
	0x000001f8, 0x00000004, 0x00040000,	/* MSTORM_ETH_PF_PRODS_GTT_OFFSET(queue_id), offset=0x1f8, mult1=0x4, size=0x4 */
	0x00003d60, 0x00000080, 0x00200000,	/* MSTORM_ETH_PF_STAT_OFFSET(pf_id), offset=0x3d60, mult1=0x80, size=0x20 */
	0x00008960, 0x00000040, 0x00300000,	/* USTORM_QUEUE_STAT_OFFSET(stat_counter_id), offset=0x8960, mult1=0x40, size=0x30 */
	0x0000e840, 0x00000060, 0x00600000,	/* USTORM_ETH_PF_STAT_OFFSET(pf_id), offset=0xe840, mult1=0x60, size=0x60 */
	0x00004698, 0x00000080, 0x00380000,	/* PSTORM_QUEUE_STAT_OFFSET(stat_counter_id), offset=0x4698, mult1=0x80, size=0x38 */
	0x000107b8, 0x000000c0, 0x00c00000,	/* PSTORM_ETH_PF_STAT_OFFSET(pf_id), offset=0x107b8, mult1=0xc0, size=0xc0 */
	0x000001f8, 0x00000002, 0x00020000,	/* PSTORM_CTL_FRAME_ETHTYPE_GTT_OFFSET(ethType_id), offset=0x1f8, mult1=0x2, size=0x2 */
	0x0000a2a0, 0x00000000, 0x01080000,	/* TSTORM_ETH_PRS_INPUT_OFFSET, offset=0xa2a0, size=0x108 */
	0x0000a3a8, 0x00000008, 0x00080000,	/* ETH_RX_RATE_LIMIT_OFFSET(pf_id), offset=0xa3a8, mult1=0x8, size=0x8 */
	0x000001c0, 0x00000008, 0x00080000,	/* TSTORM_ETH_RSS_UPDATE_GTT_OFFSET(pf_id), offset=0x1c0, mult1=0x8, size=0x8 */
	0x000001f8, 0x00000008, 0x00080000,	/* XSTORM_ETH_QUEUE_ZONE_GTT_OFFSET(queue_id), offset=0x1f8, mult1=0x8, size=0x8 */
	0x00000ac0, 0x00000008, 0x00080000,	/* YSTORM_TOE_CQ_PROD_OFFSET(rss_id), offset=0xac0, mult1=0x8, size=0x8 */
	0x00002578, 0x00000008, 0x00080000,	/* USTORM_TOE_CQ_PROD_OFFSET(rss_id), offset=0x2578, mult1=0x8, size=0x8 */
	0x000024f8, 0x00000008, 0x00080000,	/* USTORM_TOE_GRQ_PROD_OFFSET(pf_id), offset=0x24f8, mult1=0x8, size=0x8 */
	0x00000280, 0x00000008, 0x00080000,	/* TSTORM_SCSI_CMDQ_CONS_GTT_OFFSET(cmdq_queue_id), offset=0x280, mult1=0x8, size=0x8 */
	0x00000680, 0x00080018, 0x00080000,	/* TSTORM_SCSI_BDQ_EXT_PROD_GTT_OFFSET(storage_func_id,bdq_id), offset=0x680, mult1=0x18, mult2=0x8, size=0x8 */
	0x00000b78, 0x00080018, 0x00020000,	/* MSTORM_SCSI_BDQ_EXT_PROD_GTT_OFFSET(storage_func_id,bdq_id), offset=0xb78, mult1=0x18, mult2=0x8, size=0x2 */
	0x0000c640, 0x00000058, 0x003c0000,	/* TSTORM_ISCSI_RX_STATS_OFFSET(storage_func_id), offset=0xc640, mult1=0x58, size=0x3c */
	0x00012038, 0x00000020, 0x00100000,	/* MSTORM_ISCSI_RX_STATS_OFFSET(storage_func_id), offset=0x12038, mult1=0x20, size=0x10 */
	0x00011b00, 0x00000048, 0x00180000,	/* USTORM_ISCSI_RX_STATS_OFFSET(storage_func_id), offset=0x11b00, mult1=0x48, size=0x18 */
	0x00009750, 0x00000050, 0x00200000,	/* XSTORM_ISCSI_TX_STATS_OFFSET(storage_func_id), offset=0x9750, mult1=0x50, size=0x20 */
	0x00008b10, 0x00000040, 0x00280000,	/* YSTORM_ISCSI_TX_STATS_OFFSET(storage_func_id), offset=0x8b10, mult1=0x40, size=0x28 */
	0x000116c0, 0x00000018, 0x00100000,	/* PSTORM_ISCSI_TX_STATS_OFFSET(storage_func_id), offset=0x116c0, mult1=0x18, size=0x10 */
	0x0000c848, 0x00000048, 0x00380000,	/* TSTORM_FCOE_RX_STATS_OFFSET(pf_id), offset=0xc848, mult1=0x48, size=0x38 */
	0x00011790, 0x00000020, 0x00200000,	/* PSTORM_FCOE_TX_STATS_OFFSET(pf_id), offset=0x11790, mult1=0x20, size=0x20 */
	0x000046d0, 0x00000080, 0x00100000,	/* PSTORM_RDMA_QUEUE_STAT_OFFSET(rdma_stat_counter_id), offset=0x46d0, mult1=0x80, size=0x10 */
	0x00003658, 0x00000010, 0x00100000,	/* TSTORM_RDMA_QUEUE_STAT_OFFSET(rdma_stat_counter_id), offset=0x3658, mult1=0x10, size=0x10 */
	0x0000aae8, 0x00000008, 0x00010000,	/* XSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id), offset=0xaae8, mult1=0x8, size=0x1 */
	0x000097a0, 0x00000008, 0x00010000,	/* YSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id), offset=0x97a0, mult1=0x8, size=0x1 */
	0x00011a10, 0x00000008, 0x00010000,	/* PSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id), offset=0x11a10, mult1=0x8, size=0x1 */
	0x0000ea38, 0x00000008, 0x00010000,	/* TSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id), offset=0xea38, mult1=0x8, size=0x1 */
	0x00012648, 0x00000008, 0x00010000,	/* MSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id), offset=0x12648, mult1=0x8, size=0x1 */
	0x000121c8, 0x00000008, 0x00010000,	/* USTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id), offset=0x121c8, mult1=0x8, size=0x1 */
	0x0000b008, 0x00000038, 0x00100000,	/* XSTORM_IWARP_RXMIT_STATS_OFFSET(pf_id), offset=0xb008, mult1=0x38, size=0x10 */
	0x0000d788, 0x00000028, 0x00280000,	/* TSTORM_ROCE_EVENTS_STAT_OFFSET(roce_pf_id), offset=0xd788, mult1=0x28, size=0x28 */
	0x00009e68, 0x00000018, 0x00180000,	/* YSTORM_ROCE_DCQCN_RECEIVED_STATS_OFFSET(roce_pf_id), offset=0x9e68, mult1=0x18, size=0x18 */
	0x00009fe8, 0x00000008, 0x00080000,	/* YSTORM_ROCE_ERROR_STATS_OFFSET(roce_pf_id), offset=0x9fe8, mult1=0x8, size=0x8 */
	0x00013ea8, 0x00000008, 0x00080000,	/* PSTORM_ROCE_DCQCN_SENT_STATS_OFFSET(roce_pf_id), offset=0x13ea8, mult1=0x8, size=0x8 */
	0x00012f18, 0x00000018, 0x00180000,	/* USTORM_ROCE_CQE_STATS_OFFSET(roce_pf_id), offset=0x12f18, mult1=0x18, size=0x18 */
	0x0000e028, 0x00500288, 0x00100000,	/* TSTORM_NVMF_PORT_TASKPOOL_PRODUCER_CONSUMER_OFFSET(port_num_id,taskpool_index), offset=0xe028, mult1=0x288, mult2=0x50, size=0x10 */
};

/* Data size: 852 bytes */

#endif

#endif
