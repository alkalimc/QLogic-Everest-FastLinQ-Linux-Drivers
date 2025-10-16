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

#ifndef __ETH_COMMON__
#define __ETH_COMMON__
/********************/
/* ETH FW CONSTANTS */
/********************/

/* FP HSI version. FP HSI is compatible if (fwVer.major == drvVer.major && fwVer.minor >= drvVer.minor) */
#define ETH_HSI_VER_MAJOR                   3	/* ETH FP HSI Major version */
#define ETH_HSI_VER_MINOR                   11	/* ETH FP HSI Minor version */

#define ETH_HSI_VER_NO_PKT_LEN_TUNN         5	/* Alias for 8.7.x.x/8.8.x.x ETH FP HSI MINOR version. In this version driver is not required to set pkt_len field in eth_tx_1st_bd struct, and tunneling offload is not supported. */

#define ETH_PINNED_CONN_MAX_NUM             32	/* Maximum number of pinned L2 connections (CIDs) */

#define ETH_CACHE_LINE_SIZE                 64
#define ETH_RX_CQE_GAP                      32
#define ETH_MAX_RAMROD_PER_CON              8
#define ETH_TX_BD_PAGE_SIZE_BYTES           4096
#define ETH_RX_BD_PAGE_SIZE_BYTES           4096
#define ETH_RX_CQE_PAGE_SIZE_BYTES          4096
#define ETH_RX_NUM_NEXT_PAGE_BDS            2

/* Limitation for Tunneled LSO Packets on the offset (in bytes) of the inner IP header (relevant to LSO for tunneled packet): */
#define ETH_MAX_TUNN_LSO_INNER_IPV4_OFFSET          253	/* E4 Only: Offset is limited to 253 bytes (inclusive). */
#define ETH_MAX_TUNN_LSO_INNER_IPV6_OFFSET          251	/* E4 Only: Offset is limited to 251 bytes (inclusive). */

#define ETH_TX_MIN_BDS_PER_NON_LSO_PKT              1
#define ETH_TX_MAX_BDS_PER_NON_LSO_PACKET           18
#define ETH_TX_MAX_BDS_PER_LSO_PACKET               255
#define ETH_TX_MAX_LSO_HDR_NBD                      4
#define ETH_TX_MIN_BDS_PER_LSO_PKT                  3
#define ETH_TX_MIN_BDS_PER_TUNN_IPV6_WITH_EXT_PKT   3
#define ETH_TX_MIN_BDS_PER_IPV6_WITH_EXT_PKT        2
#define ETH_TX_MIN_BDS_PER_PKT_W_LOOPBACK_MODE      2
#define ETH_TX_MIN_BDS_PER_PKT_W_VPORT_FORWARDING   4
#define ETH_TX_MAX_NON_LSO_PKT_LEN                  (9700 - (4 + 4 + 12 + 8))	/* (QM_REG_TASKBYTECRDCOST_0, QM_VOQ_BYTE_CRD_TASK_COST) - (VLAN-TAG + CRC + IPG + PREAMBLE) */
#define ETH_TX_MAX_LSO_HDR_BYTES                    510
#define ETH_TX_LSO_WINDOW_BDS_NUM                   (18 - 1)	/* Number of BDs to consider for LSO sliding window restriction is (ETH_TX_LSO_WINDOW_BDS_NUM - hdr_nbd) */
#define ETH_TX_LSO_WINDOW_MIN_LEN                   9700	/* Minimum data length (in bytes) in LSO sliding window */
#define ETH_TX_MAX_LSO_PAYLOAD_LEN                  0xFE000	/* Maximum LSO packet TCP payload length (in bytes) */
#define ETH_TX_NUM_SAME_AS_LAST_ENTRIES             320	/* Number of same-as-last resources in tx switching */
#define ETH_TX_INACTIVE_SAME_AS_LAST                0xFFFF	/* Value for a connection for which same as last feature is disabled */

#define ETH_NUM_STATISTIC_COUNTERS                  MAX_NUM_VPORTS	/* Maximum number of statistics counters */
#define ETH_NUM_STATISTIC_COUNTERS_DOUBLE_VF_ZONE   (ETH_NUM_STATISTIC_COUNTERS	\
						     - MAX_NUM_VFS / 2)	/* Maximum number of statistics counters when doubled VF zone used */
#define ETH_NUM_STATISTIC_COUNTERS_QUAD_VF_ZONE     (ETH_NUM_STATISTIC_COUNTERS	\
						     - 3 * MAX_NUM_VFS / 4)	/* Maximum number of statistics counters when quad VF zone used */

#define ETH_RX_MAX_BUFF_PER_PKT             5	/* Maximum number of buffers, used for RX packet placement */
#define ETH_RX_BD_THRESHOLD                16	/* Minimum number of free BDs in RX ring, that guarantee receiving of at least one RX packet. */

/* num of MAC/VLAN filters */
#define ETH_NUM_MAC_FILTERS                 512
#define ETH_NUM_VLAN_FILTERS                512

/* approx. multicast constants */
#define ETH_MULTICAST_BIN_FROM_MAC_SEED     0	/* CRC seed for multicast bin calculation */
#define ETH_MULTICAST_MAC_BINS              256
#define ETH_MULTICAST_MAC_BINS_IN_REGS      (ETH_MULTICAST_MAC_BINS / 32)

/*  ethernet vport update constants */
#define ETH_FILTER_RULES_COUNT              10
#define ETH_RSS_IND_TABLE_ENTRIES_NUM       128	/* number of RSS indirection table entries, per Vport) */
#define ETH_RSS_IND_TABLE_MASK_SIZE_REGS    (ETH_RSS_IND_TABLE_ENTRIES_NUM / 32)	/* RSS indirection table mask size in registers */
#define ETH_RSS_KEY_SIZE_REGS               10	/* Length of RSS key (in regs) */
#define ETH_RSS_ENGINE_NUM_K2               207	/* number of available RSS engines in AH */
#define ETH_RSS_ENGINE_NUM_BB               127	/* number of available RSS engines in BB */

/* TPA constants */
#define ETH_TPA_MAX_AGGS_NUM                64	/* Maximum number of open TPA aggregations */
#define ETH_TPA_CQE_START_BW_LEN_LIST_SIZE  2	/* TPA-start CQE additional BD list length. Used for backward compatible  */
#define ETH_TPA_CQE_CONT_LEN_LIST_SIZE      6	/* Maximum number of buffers, reported by TPA-continue CQE */
#define ETH_TPA_CQE_END_LEN_LIST_SIZE       4	/* Maximum number of buffers, reported by TPA-end CQE */

/* Control frame check constants */
#define ETH_CTL_FRAME_ETH_TYPE_NUM        4	/* Number of etherType values configured by the driver for control frame check */

/* GFS constants */
#define ETH_GFT_TRASHCAN_VPORT         0x1FF	/* GFT drop flow vport number */

#define ETH_GFS_NUM_OF_ACTIONS         10	/* Maximum number of GFS actions supported */
#define ETH_TGFS_NUM_OF_IND_INDEX      511	/* Number of indirect indexes supported by TGFS */
#define ETH_RGFS_NUM_OF_IND_INDEX      256	/* Number of indirect indexes supported by RGFS */
#define ETH_GFS_MAX_NUM_COUNTER_TIDS   (1 << 18)	/* Maximum number GFS Flow Counter TIDs */
#define ETH_GFS_INVALID_INDIRECT_RAM_INDEX  0x1ff	/* Value that defines an invalid index in GFS redirect table, in this case, the table wont be updated */

#define ETH_GFS_PUSH_IND_INDEX_LIST_SIZE_ETH            1	/*Number of indirect data table indexes used by ETH_GFS_PUSH_ETH command */
#define ETH_GFS_PUSH_IND_INDEX_LIST_SIZE_GRE_V4         1	/*Number of indirect data table indexes used by ETH_GFS_PUSH_GRE_V4 command */
#define ETH_GFS_PUSH_IND_INDEX_LIST_SIZE_GRE_V6         3	/*Number of indirect data table indexes used by ETH_GFS_PUSH_GRE_V6 command */
#define ETH_GFS_PUSH_IND_INDEX_LIST_SIZE_VXLAN_V4       1	/*Number of indirect data table indexes used by ETH_GFS_PUSH_VXLAN_V4 command */
#define ETH_GFS_PUSH_IND_INDEX_LIST_SIZE_VXLAN_V6       3	/*Number of indirect data table indexes used by ETH_GFS_PUSH_VXLAN_V6 command */
#define ETH_GFS_PUSH_IND_INDEX_LIST_SIZE_VXLAN_V4_ETH   2	/*Number of indirect data table indexes used by ETH_GFS_PUSH_VXLAN_V4_ETH command */
#define ETH_GFS_PUSH_IND_INDEX_LIST_SIZE_VXLAN_V6_ETH   4	/*Number of indirect data table indexes used by ETH_GFS_PUSH_VXLAN_V6_ETH command */
#define ETH_GFS_PUSH_IND_INDEX_LIST_SIZE_CUSTOM         8	/*Number of indirect data table indexes used by ETH_GFS_PUSH_CUSTOM command */

#define RGFS_FW_HINT_SET_DROP          0x1	/* RGFS, FWhint bit 0 (Drop): */
#define RGFS_FW_HINT_DUP_CMD_NUM_SHIFT 0x1	/* RGFS, FWhint bits 1,2 (dupCmdNum): */
#define TGFS_FW_HINT_SET_FIRST_DUP_FLG 0x1	/* TGFS, FWhint bit 0 (firstDupFlg): */

/* TGFS, FWhint bits 1,2 (destType) - Indicate if destination is regular (TX switch), RX VPORT or VFC bypass (including drop): */
#define TGFS_FW_HINT_DEST_TYPE_SHIFT           0x1
#define TGFS_FW_HINT_DEST_TYPE_MASK            0x3
#define TGFS_FW_HINT_DEST_TYPE_DROP           (0 << \
					       TGFS_FW_HINT_DEST_TYPE_SHIFT)	// TODO: ASAFR: Instead of working with shifts and masks, better use struct and enum, and let compiler do the dirty work
#define TGFS_FW_HINT_DEST_TYPE_PHY_PORT_ONLY  (1 << \
					       TGFS_FW_HINT_DEST_TYPE_SHIFT)
#define TGFS_FW_HINT_DEST_TYPE_LB_PORT_ONLY   (2 << \
					       TGFS_FW_HINT_DEST_TYPE_SHIFT)
#define TGFS_FW_HINT_DEST_TYPE_VFC_BYPASS     (3 << \
					       TGFS_FW_HINT_DEST_TYPE_SHIFT)

/* Destination port mode */
enum dst_port_mode {
	DST_PORT_PHY,		/* HSI_COMMENT: Send to physical port. */
	DST_PORT_LOOPBACK,	/* HSI_COMMENT: Send to loopback port. */
	DST_PORT_PHY_LOOPBACK,	/* HSI_COMMENT: Send to physical and loopback port. */
	DST_PORT_DROP,		/* HSI_COMMENT: Drop the packet in PBF. */
	MAX_DST_PORT_MODE
};

/* Ethernet address type */
enum eth_addr_type {
	BROADCAST_ADDRESS,
	MULTICAST_ADDRESS,
	UNICAST_ADDRESS,
	UNKNOWN_ADDRESS,
	MAX_ETH_ADDR_TYPE
};

struct eth_tx_1st_bd_flags {
	u8 bitfields;
#define ETH_TX_1ST_BD_FLAGS_START_BD_MASK       0x1	/* HSI_COMMENT: Set to 1 in the first BD. (for debug) */
#define ETH_TX_1ST_BD_FLAGS_START_BD_SHIFT      0
#define ETH_TX_1ST_BD_FLAGS_FORCE_VLAN_MODE_MASK        0x1	/* HSI_COMMENT: Do not allow additional VLAN manipulations on this packet. */
#define ETH_TX_1ST_BD_FLAGS_FORCE_VLAN_MODE_SHIFT       1
#define ETH_TX_1ST_BD_FLAGS_IP_CSUM_MASK        0x1	/* HSI_COMMENT: Recalculate IP checksum. For tunneled packet - relevant to inner header. Note: For LSO packets, must be set. */
#define ETH_TX_1ST_BD_FLAGS_IP_CSUM_SHIFT       2
#define ETH_TX_1ST_BD_FLAGS_L4_CSUM_MASK        0x1	/* HSI_COMMENT: Recalculate TCP/UDP checksum. For tunneled packet - relevant to inner header. */
#define ETH_TX_1ST_BD_FLAGS_L4_CSUM_SHIFT       3
#define ETH_TX_1ST_BD_FLAGS_VLAN_INSERTION_MASK 0x1	/* HSI_COMMENT: If set, insert VLAN tag from vlan field to the packet. For tunneled packet - relevant to outer header. */
#define ETH_TX_1ST_BD_FLAGS_VLAN_INSERTION_SHIFT        4
#define ETH_TX_1ST_BD_FLAGS_LSO_MASK    0x1	/* HSI_COMMENT: If set, this is an LSO packet. Note: For Tunneled LSO packets, the offset of the inner IPV4 (and IPV6) header is limited to 253 (and 251 respectively) bytes, inclusive. */
#define ETH_TX_1ST_BD_FLAGS_LSO_SHIFT   5
#define ETH_TX_1ST_BD_FLAGS_TUNN_IP_CSUM_MASK   0x1	/* HSI_COMMENT: Recalculate Tunnel IP Checksum (if Tunnel IP Header is IPv4) */
#define ETH_TX_1ST_BD_FLAGS_TUNN_IP_CSUM_SHIFT  6
#define ETH_TX_1ST_BD_FLAGS_TUNN_L4_CSUM_MASK   0x1	/* HSI_COMMENT: Recalculate Tunnel UDP/GRE Checksum (Depending on Tunnel Type). In case of GRE tunnel, this flag means GRE CSO, and in this case GRE checksum field Must be present. */
#define ETH_TX_1ST_BD_FLAGS_TUNN_L4_CSUM_SHIFT  7
};

/* The parsing information data for the first tx bd of a given packet. */
struct eth_tx_data_1st_bd {
	__le16 vlan;		/* HSI_COMMENT: VLAN tag to insert to packet (if enabled by vlan_insertion flag). */
	u8 nbds;		/* HSI_COMMENT: Number of BDs in packet. Should be at least 1 in non-LSO packet and at least 3 in LSO (or Tunnel with IPv6+ext) packet. */
	struct eth_tx_1st_bd_flags bd_flags;
	__le16 bitfields;
#define ETH_TX_DATA_1ST_BD_TUNN_FLAG_MASK       0x1	/* HSI_COMMENT: Indicates a tunneled packet. Must be set for encapsulated packet. */
#define ETH_TX_DATA_1ST_BD_TUNN_FLAG_SHIFT      0
#define ETH_TX_DATA_1ST_BD_RESERVED0_MASK       0x1
#define ETH_TX_DATA_1ST_BD_RESERVED0_SHIFT      1
#define ETH_TX_DATA_1ST_BD_PKT_LEN_MASK 0x3FFF	/* HSI_COMMENT: Total packet length - must be filled for non-LSO packets. */
#define ETH_TX_DATA_1ST_BD_PKT_LEN_SHIFT        2
};

/* The parsing information data for the second tx bd of a given packet. */
struct eth_tx_data_2nd_bd {
	__le16 tunn_ip_size;	/* HSI_COMMENT: For tunnel with IPv6+ext - Tunnel header IP datagram length (in BYTEs) */
	__le16 bitfields1;
#define ETH_TX_DATA_2ND_BD_TUNN_INNER_L2_HDR_SIZE_W_MASK        0xF	/* HSI_COMMENT: For Tunnel header with IPv6 ext. - Inner L2 Header Size (in 2-byte WORDs) */
#define ETH_TX_DATA_2ND_BD_TUNN_INNER_L2_HDR_SIZE_W_SHIFT       0
#define ETH_TX_DATA_2ND_BD_TUNN_INNER_ETH_TYPE_MASK     0x3	/* HSI_COMMENT: For Tunnel header with IPv6 ext. - Inner L2 Header MAC DA Type (use enum eth_addr_type) */
#define ETH_TX_DATA_2ND_BD_TUNN_INNER_ETH_TYPE_SHIFT    4
#define ETH_TX_DATA_2ND_BD_DST_PORT_MODE_MASK   0x3	/* HSI_COMMENT: Destination port mode, applicable only when tx_dst_port_mode_config == ETH_TX_DST_MODE_CONFIG_FORWARD_DATA_IN_BD. (use enum dst_port_mode) */
#define ETH_TX_DATA_2ND_BD_DST_PORT_MODE_SHIFT  6
#define ETH_TX_DATA_2ND_BD_START_BD_MASK        0x1	/* HSI_COMMENT: Should be 0 in all the BDs, except the first one. (for debug) */
#define ETH_TX_DATA_2ND_BD_START_BD_SHIFT       8
#define ETH_TX_DATA_2ND_BD_TUNN_TYPE_MASK       0x3	/* HSI_COMMENT: For Tunnel header with IPv6 ext. - Tunnel Type (use enum eth_tx_tunn_type) */
#define ETH_TX_DATA_2ND_BD_TUNN_TYPE_SHIFT      9
#define ETH_TX_DATA_2ND_BD_TUNN_INNER_IPV6_MASK 0x1	/* HSI_COMMENT: Relevant only for LSO. When Tunnel is with IPv6+ext - Set if inner header is IPv6 */
#define ETH_TX_DATA_2ND_BD_TUNN_INNER_IPV6_SHIFT        11
#define ETH_TX_DATA_2ND_BD_IPV6_EXT_MASK        0x1	/* HSI_COMMENT: In tunneling mode - Set to 1 when the Inner header is IPv6 with extension. Otherwise set to 1 if the header is IPv6 with extension. */
#define ETH_TX_DATA_2ND_BD_IPV6_EXT_SHIFT       12
#define ETH_TX_DATA_2ND_BD_TUNN_IPV6_EXT_MASK   0x1	/* HSI_COMMENT: Set to 1 if Tunnel (outer = encapsulating) header has IPv6 ext. (Note: 3rd BD is required, hence EDPM does not support Tunnel [outer] header with Ipv6Ext) */
#define ETH_TX_DATA_2ND_BD_TUNN_IPV6_EXT_SHIFT  13
#define ETH_TX_DATA_2ND_BD_L4_UDP_MASK  0x1	/* HSI_COMMENT: Set if (inner) L4 protocol is UDP. (Required when IPv6+ext (or tunnel with inner or outer Ipv6+ext) and l4_csum is set) */
#define ETH_TX_DATA_2ND_BD_L4_UDP_SHIFT 14
#define ETH_TX_DATA_2ND_BD_L4_PSEUDO_CSUM_MODE_MASK     0x1	/* HSI_COMMENT: The pseudo header checksum type in the L4 checksum field. Required when IPv6+ext and l4_csum is set. (use enum eth_l4_pseudo_checksum_mode) */
#define ETH_TX_DATA_2ND_BD_L4_PSEUDO_CSUM_MODE_SHIFT    15
	__le16 bitfields2;
#define ETH_TX_DATA_2ND_BD_L4_HDR_START_OFFSET_W_MASK   0x1FFF	/* HSI_COMMENT: For inner/outer header IPv6+ext - (inner) L4 header offset (in 2-byte WORDs). For regular packet - offset from the beginning of the packet. For tunneled packet - offset from the beginning of the inner header */
#define ETH_TX_DATA_2ND_BD_L4_HDR_START_OFFSET_W_SHIFT  0
#define ETH_TX_DATA_2ND_BD_RESERVED0_MASK       0x7
#define ETH_TX_DATA_2ND_BD_RESERVED0_SHIFT      13
};

/* Firmware data for L2-EDPM packet. */
struct eth_edpm_fw_data {
	struct eth_tx_data_1st_bd data_1st_bd;	/* HSI_COMMENT: Parsing information data from the 1st BD. */
	struct eth_tx_data_2nd_bd data_2nd_bd;	/* HSI_COMMENT: Parsing information data from the 2nd BD. */
	__le32 reserved;
};

/* tunneling parsing flags */
struct eth_tunnel_parsing_flags {
	u8 flags;
#define ETH_TUNNEL_PARSING_FLAGS_TYPE_MASK      0x3	/* HSI_COMMENT: 0 - no tunneling, 1 - GENEVE, 2 - GRE, 3 - VXLAN (use enum eth_rx_tunn_type) */
#define ETH_TUNNEL_PARSING_FLAGS_TYPE_SHIFT     0
#define ETH_TUNNEL_PARSING_FLAGS_TENNANT_ID_EXIST_MASK  0x1	/* HSI_COMMENT:  If it s not an encapsulated packet then put 0x0. If it s an encapsulated packet but the tenant-id doesn t exist then put 0x0. Else put 0x1 */
#define ETH_TUNNEL_PARSING_FLAGS_TENNANT_ID_EXIST_SHIFT 2
#define ETH_TUNNEL_PARSING_FLAGS_NEXT_PROTOCOL_MASK     0x3	/* HSI_COMMENT: Type of the next header above the tunneling: 0 - unknown, 1 - L2, 2 - Ipv4, 3 - IPv6 (use enum tunnel_next_protocol) */
#define ETH_TUNNEL_PARSING_FLAGS_NEXT_PROTOCOL_SHIFT    3
#define ETH_TUNNEL_PARSING_FLAGS_FIRSTHDRIPMATCH_MASK   0x1	/* HSI_COMMENT: The result of comparing the DA-ip of the tunnel header. */
#define ETH_TUNNEL_PARSING_FLAGS_FIRSTHDRIPMATCH_SHIFT  5
#define ETH_TUNNEL_PARSING_FLAGS_IPV4_FRAGMENT_MASK     0x1
#define ETH_TUNNEL_PARSING_FLAGS_IPV4_FRAGMENT_SHIFT    6
#define ETH_TUNNEL_PARSING_FLAGS_IPV4_OPTIONS_MASK      0x1
#define ETH_TUNNEL_PARSING_FLAGS_IPV4_OPTIONS_SHIFT     7
};

/* PMD flow control bits */
struct eth_pmd_flow_flags {
	u8 flags;
#define ETH_PMD_FLOW_FLAGS_VALID_MASK   0x1	/* HSI_COMMENT: CQE valid bit */
#define ETH_PMD_FLOW_FLAGS_VALID_SHIFT  0
#define ETH_PMD_FLOW_FLAGS_TOGGLE_MASK  0x1	/* HSI_COMMENT: CQE ring toggle bit */
#define ETH_PMD_FLOW_FLAGS_TOGGLE_SHIFT 1
#define ETH_PMD_FLOW_FLAGS_RESERVED_MASK        0x3F
#define ETH_PMD_FLOW_FLAGS_RESERVED_SHIFT       2
};

/* Regular ETH Rx FP CQE.  */
struct eth_fast_path_rx_reg_cqe {
	u8 type;		/* HSI_COMMENT: CQE type (use enum eth_rx_cqe_type) */
	u8 bitfields;
#define ETH_FAST_PATH_RX_REG_CQE_RSS_HASH_TYPE_MASK     0x7	/* HSI_COMMENT: Type of calculated RSS hash (use enum rss_hash_type) */
#define ETH_FAST_PATH_RX_REG_CQE_RSS_HASH_TYPE_SHIFT    0
#define ETH_FAST_PATH_RX_REG_CQE_TC_MASK        0xF	/* HSI_COMMENT: Traffic Class */
#define ETH_FAST_PATH_RX_REG_CQE_TC_SHIFT       3
#define ETH_FAST_PATH_RX_REG_CQE_RESERVED0_MASK 0x1
#define ETH_FAST_PATH_RX_REG_CQE_RESERVED0_SHIFT        7
	__le16 pkt_len;		/* HSI_COMMENT: Total packet length (from the parser) */
	struct parsing_and_err_flags pars_flags;	/* HSI_COMMENT: Parsing and error flags from the parser */
	__le16 vlan_tag;	/* HSI_COMMENT: 802.1q VLAN tag */
	__le32 rss_hash;	/* HSI_COMMENT: RSS hash result */
	__le16 len_on_first_bd;	/* HSI_COMMENT: Number of bytes placed on first BD */
	u8 placement_offset;	/* HSI_COMMENT: Offset of placement from BD start */
	struct eth_tunnel_parsing_flags tunnel_pars_flags;	/* HSI_COMMENT: Tunnel Parsing Flags */
	u8 bd_num;		/* HSI_COMMENT: Number of BDs, used for packet */
	u8 reserved;
	__le16 reserved2;
	__le32 flow_id_or_resource_id;	/* HSI_COMMENT: aRFS flow ID or Resource ID - Indicates a Vport ID from which packet was sent, used when sending from VF to VF Representor. */
	u8 reserved1[7];
	struct eth_pmd_flow_flags pmd_flags;	/* HSI_COMMENT: CQE valid and toggle bits */
};

/* TPA-continue ETH Rx FP CQE. */
struct eth_fast_path_rx_tpa_cont_cqe {
	u8 type;		/* HSI_COMMENT: CQE type (use enum eth_rx_cqe_type) */
	u8 tpa_agg_index;	/* HSI_COMMENT: TPA aggregation index */
	__le16 len_list[ETH_TPA_CQE_CONT_LEN_LIST_SIZE];	/* HSI_COMMENT: List of the segment sizes */
	u8 reserved;
	u8 reserved1;		/* HSI_COMMENT: FW reserved. */
	__le16 reserved2[ETH_TPA_CQE_CONT_LEN_LIST_SIZE];	/* HSI_COMMENT: FW reserved. */
	u8 reserved3[3];
	struct eth_pmd_flow_flags pmd_flags;	/* HSI_COMMENT: CQE valid and toggle bits */
};

/* TPA-end ETH Rx FP CQE .  */
struct eth_fast_path_rx_tpa_end_cqe {
	u8 type;		/* HSI_COMMENT: CQE type (use enum eth_rx_cqe_type) */
	u8 tpa_agg_index;	/* HSI_COMMENT: TPA aggregation index */
	__le16 total_packet_len;	/* HSI_COMMENT: Total aggregated packet length */
	u8 num_of_bds;		/* HSI_COMMENT: Total number of BDs comprising the packet */
	u8 end_reason;		/* HSI_COMMENT: Aggregation end reason. Use enum eth_tpa_end_reason */
	__le16 num_of_coalesced_segs;	/* HSI_COMMENT: Number of coalesced TCP segments */
	__le32 ts_delta;	/* HSI_COMMENT: TCP timestamp delta */
	__le16 len_list[ETH_TPA_CQE_END_LEN_LIST_SIZE];	/* HSI_COMMENT: List of the segment sizes */
	__le16 reserved3[ETH_TPA_CQE_END_LEN_LIST_SIZE];	/* HSI_COMMENT: FW reserved. */
	__le16 reserved1;
	u8 reserved2;		/* HSI_COMMENT: FW reserved. */
	struct eth_pmd_flow_flags pmd_flags;	/* HSI_COMMENT: CQE valid and toggle bits */
};

/* TPA-start ETH Rx FP CQE. */
struct eth_fast_path_rx_tpa_start_cqe {
	u8 type;		/* HSI_COMMENT: CQE type (use enum eth_rx_cqe_type) */
	u8 bitfields;
#define ETH_FAST_PATH_RX_TPA_START_CQE_RSS_HASH_TYPE_MASK       0x7	/* HSI_COMMENT: Type of calculated RSS hash (use enum rss_hash_type) */
#define ETH_FAST_PATH_RX_TPA_START_CQE_RSS_HASH_TYPE_SHIFT      0
#define ETH_FAST_PATH_RX_TPA_START_CQE_TC_MASK  0xF	/* HSI_COMMENT: Traffic Class */
#define ETH_FAST_PATH_RX_TPA_START_CQE_TC_SHIFT 3
#define ETH_FAST_PATH_RX_TPA_START_CQE_RESERVED0_MASK   0x1
#define ETH_FAST_PATH_RX_TPA_START_CQE_RESERVED0_SHIFT  7
	__le16 seg_len;		/* HSI_COMMENT: Segment length (packetLen from the parser) */
	struct parsing_and_err_flags pars_flags;	/* HSI_COMMENT: Parsing and error flags from the parser */
	__le16 vlan_tag;	/* HSI_COMMENT: 802.1q VLAN tag */
	__le32 rss_hash;	/* HSI_COMMENT: RSS hash result */
	__le16 len_on_first_bd;	/* HSI_COMMENT: Number of bytes placed on first BD */
	u8 placement_offset;	/* HSI_COMMENT: Offset of placement from BD start */
	struct eth_tunnel_parsing_flags tunnel_pars_flags;	/* HSI_COMMENT: Tunnel Parsing Flags */
	u8 tpa_agg_index;	/* HSI_COMMENT: TPA aggregation index */
	u8 header_len;		/* HSI_COMMENT: Packet L2+L3+L4 header length */
	__le16 bw_ext_bd_len_list[ETH_TPA_CQE_START_BW_LEN_LIST_SIZE];	/* HSI_COMMENT: Additional BDs length list. Used for backward compatible. */
	__le16 reserved2;
	__le32 flow_id_or_resource_id;	/* HSI_COMMENT: aRFS or GFS flow ID or Resource ID - Indicates a Vport ID from which packet was sent, used when sending from VF to VF Representor. */
	u8 reserved[3];
	struct eth_pmd_flow_flags pmd_flags;	/* HSI_COMMENT: CQE valid and toggle bits */
};

/* The L4 pseudo checksum mode for Ethernet */
enum eth_l4_pseudo_checksum_mode {
	ETH_L4_PSEUDO_CSUM_CORRECT_LENGTH,	/* HSI_COMMENT: Pseudo Header checksum on packet is calculated with the correct (as defined in RFC) packet length field. */
	ETH_L4_PSEUDO_CSUM_ZERO_LENGTH,	/* HSI_COMMENT: (Supported only for LSO) Pseudo Header checksum on packet is calculated with zero length field. */
	MAX_ETH_L4_PSEUDO_CHECKSUM_MODE
};

struct eth_rx_bd {
	struct regpair addr;	/* HSI_COMMENT: single continues buffer */
};

/* regular ETH Rx SP CQE */
struct eth_slow_path_rx_cqe {
	u8 type;		/* HSI_COMMENT: CQE type (use enum eth_rx_cqe_type) */
	u8 ramrod_cmd_id;
	u8 error_flag;
	u8 reserved[25];
	__le16 echo;
	u8 reserved1;
	struct eth_pmd_flow_flags pmd_flags;	/* HSI_COMMENT: CQE valid and toggle bits */
};

/* union for all ETH Rx CQE types */
union eth_rx_cqe {
	struct eth_fast_path_rx_reg_cqe fast_path_regular;	/* HSI_COMMENT: Regular FP CQE */
	struct eth_fast_path_rx_tpa_start_cqe fast_path_tpa_start;	/* HSI_COMMENT: TPA-start CQE */
	struct eth_fast_path_rx_tpa_cont_cqe fast_path_tpa_cont;	/* HSI_COMMENT: TPA-continue CQE */
	struct eth_fast_path_rx_tpa_end_cqe fast_path_tpa_end;	/* HSI_COMMENT: TPA-end CQE */
	struct eth_slow_path_rx_cqe slow_path;	/* HSI_COMMENT: SP CQE */
};

/* ETH Rx CQE type */
enum eth_rx_cqe_type {
	ETH_RX_CQE_TYPE_UNUSED,
	ETH_RX_CQE_TYPE_REGULAR,	/* HSI_COMMENT: Regular FP ETH Rx CQE */
	ETH_RX_CQE_TYPE_SLOW_PATH,	/* HSI_COMMENT: Slow path ETH Rx CQE */
	ETH_RX_CQE_TYPE_TPA_START,	/* HSI_COMMENT: TPA start ETH Rx CQE */
	ETH_RX_CQE_TYPE_TPA_CONT,	/* HSI_COMMENT: TPA Continue ETH Rx CQE */
	ETH_RX_CQE_TYPE_TPA_END,	/* HSI_COMMENT: TPA end ETH Rx CQE */
	MAX_ETH_RX_CQE_TYPE
};

/* Wrapper for PD RX CQE - used in order to cover full cache line when writing CQE */
struct eth_rx_pmd_cqe {
	union eth_rx_cqe cqe;	/* HSI_COMMENT: CQE data itself */
	u8 reserved[ETH_RX_CQE_GAP];
};

/* Eth RX Tunnel Type */
enum eth_rx_tunn_type {
	ETH_RX_NO_TUNN,		/* HSI_COMMENT: No Tunnel. */
	ETH_RX_TUNN_GENEVE,	/* HSI_COMMENT: GENEVE Tunnel. */
	ETH_RX_TUNN_GRE,	/* HSI_COMMENT: GRE Tunnel. */
	ETH_RX_TUNN_VXLAN,	/* HSI_COMMENT: VXLAN Tunnel. */
	MAX_ETH_RX_TUNN_TYPE
};

/* Aggregation end reason. */
enum eth_tpa_end_reason {
	ETH_AGG_END_UNUSED,
	ETH_AGG_END_SP_UPDATE,	/* HSI_COMMENT: SP configuration update */
	ETH_AGG_END_MAX_LEN,	/* HSI_COMMENT: Maximum aggregation length or maximum buffer number used. */
	ETH_AGG_END_LAST_SEG,	/* HSI_COMMENT: TCP PSH flag or TCP payload length below continue threshold. */
	ETH_AGG_END_TIMEOUT,	/* HSI_COMMENT: Timeout expiration. */
	ETH_AGG_END_NOT_CONSISTENT,	/* HSI_COMMENT: Packet header not consistency: different IPv4 TOS, TTL or flags, IPv6 TC, Hop limit or Flow label, TCP header length or TS options. In GRO different TS value, SMAC, DMAC, ackNum, windowSize or VLAN */
	ETH_AGG_END_OUT_OF_ORDER,	/* HSI_COMMENT: Out of order or retransmission packet: sequence, ack or timestamp not consistent with previous segment. */
	ETH_AGG_END_NON_TPA_SEG,	/* HSI_COMMENT: Next segment cant be aggregated due to LLC/SNAP, IP error, IP fragment, IPv4 options, IPv6 extension, IP ECN = CE, TCP errors, TCP options, zero TCP payload length , TCP flags or not supported tunnel header options.  */
	MAX_ETH_TPA_END_REASON
};

/* The first tx bd of a given packet */
struct eth_tx_1st_bd {
	struct regpair addr;	/* HSI_COMMENT: Single continuous buffer */
	__le16 nbytes;		/* HSI_COMMENT: Number of bytes in this BD. */
	struct eth_tx_data_1st_bd data;	/* HSI_COMMENT: Parsing information data. */
};

/* The second tx bd of a given packet */
struct eth_tx_2nd_bd {
	struct regpair addr;	/* HSI_COMMENT: Single continuous buffer */
	__le16 nbytes;		/* HSI_COMMENT: Number of bytes in this BD. */
	struct eth_tx_data_2nd_bd data;	/* HSI_COMMENT: Parsing information data. */
};

/* The parsing information data for the third tx bd of a given packet. */
struct eth_tx_data_3rd_bd {
	__le16 lso_mss;		/* HSI_COMMENT: For LSO packet - the MSS in bytes. */
	__le16 bitfields;
#define ETH_TX_DATA_3RD_BD_TCP_HDR_LEN_DW_MASK  0xF	/* HSI_COMMENT: Inner (for tunneled packets, and single for non-tunneled packets) TCP header length (in 4 byte words). This value is required in LSO mode, when the inner/outer header is Ipv6 with extension headers. */
#define ETH_TX_DATA_3RD_BD_TCP_HDR_LEN_DW_SHIFT 0
#define ETH_TX_DATA_3RD_BD_HDR_NBD_MASK 0xF	/* HSI_COMMENT: LSO - number of BDs which contain headers. value should be in range (1..ETH_TX_MAX_LSO_HDR_NBD). */
#define ETH_TX_DATA_3RD_BD_HDR_NBD_SHIFT        4
#define ETH_TX_DATA_3RD_BD_START_BD_MASK        0x1	/* HSI_COMMENT: Should be 0 in all the BDs, except the first one. (for debug) */
#define ETH_TX_DATA_3RD_BD_START_BD_SHIFT       8
#define ETH_TX_DATA_3RD_BD_RESERVED0_MASK       0x7F
#define ETH_TX_DATA_3RD_BD_RESERVED0_SHIFT      9
	u8 tunn_l4_hdr_start_offset_w;	/* HSI_COMMENT: For tunnel with IPv6+ext - Pointer to the tunnel L4 Header (in 2-byte WORDs) */
	u8 tunn_hdr_size_w;	/* HSI_COMMENT: For tunnel with IPv6+ext - Total size of the Tunnel Header (in 2-byte WORDs) */
};

/* The third tx bd of a given packet */
struct eth_tx_3rd_bd {
	struct regpair addr;	/* HSI_COMMENT: Single continuous buffer */
	__le16 nbytes;		/* HSI_COMMENT: Number of bytes in this BD. */
	struct eth_tx_data_3rd_bd data;	/* HSI_COMMENT: Parsing information data. */
};

/* The parsing information data for the forth tx bd of a given packet. */
struct eth_tx_data_4th_bd {
	u8 dst_vport_id;	/* HSI_COMMENT: Destination Vport ID to forward the packet, applicable only when tx_dst_port_mode_config == ETH_TX_DST_MODE_CONFIG_FORWARD_DATA_IN_BD and dst_port_mode== DST_PORT_LOOPBACK, used to route the packet from VF Representor to VF */
	u8 reserved4;
	__le16 bitfields;
#define ETH_TX_DATA_4TH_BD_DST_VPORT_ID_VALID_MASK      0x1	/* HSI_COMMENT: if set, dst_vport_id has a valid value and will be used in FW */
#define ETH_TX_DATA_4TH_BD_DST_VPORT_ID_VALID_SHIFT     0
#define ETH_TX_DATA_4TH_BD_RESERVED1_MASK       0x7F
#define ETH_TX_DATA_4TH_BD_RESERVED1_SHIFT      1
#define ETH_TX_DATA_4TH_BD_START_BD_MASK        0x1	/* HSI_COMMENT: Should be 0 in all the BDs, except the first one. (for debug) */
#define ETH_TX_DATA_4TH_BD_START_BD_SHIFT       8
#define ETH_TX_DATA_4TH_BD_RESERVED2_MASK       0x7F
#define ETH_TX_DATA_4TH_BD_RESERVED2_SHIFT      9
	__le16 reserved3;
};

/* The forth tx bd of a given packet */
struct eth_tx_4th_bd {
	struct regpair addr;	/* HSI_COMMENT: Single continuous buffer */
	__le16 nbytes;		/* HSI_COMMENT: Number of bytes in this BD. */
	struct eth_tx_data_4th_bd data;	/* HSI_COMMENT: Parsing information data. */
};

/* Complementary information for the regular tx bd of a given packet. */
struct eth_tx_data_bd {
	__le16 reserved0;
	__le16 bitfields;
#define ETH_TX_DATA_BD_RESERVED1_MASK   0xFF
#define ETH_TX_DATA_BD_RESERVED1_SHIFT  0
#define ETH_TX_DATA_BD_START_BD_MASK    0x1	/* HSI_COMMENT: Should be 0 in all the BDs, except the first one. (for debug) */
#define ETH_TX_DATA_BD_START_BD_SHIFT   8
#define ETH_TX_DATA_BD_RESERVED2_MASK   0x7F
#define ETH_TX_DATA_BD_RESERVED2_SHIFT  9
	__le16 reserved3;
};

/* The common regular TX BD ring element */
struct eth_tx_bd {
	struct regpair addr;	/* HSI_COMMENT: Single continuous buffer */
	__le16 nbytes;		/* HSI_COMMENT: Number of bytes in this BD. */
	struct eth_tx_data_bd data;	/* HSI_COMMENT: Complementary information. */
};

union eth_tx_bd_types {
	struct eth_tx_1st_bd first_bd;	/* HSI_COMMENT: The first tx bd of a given packet */
	struct eth_tx_2nd_bd second_bd;	/* HSI_COMMENT: The second tx bd of a given packet */
	struct eth_tx_3rd_bd third_bd;	/* HSI_COMMENT: The third tx bd of a given packet */
	struct eth_tx_4th_bd fourth_bd;	/* HSI_COMMENT: The fourth tx bd of a given packet */
	struct eth_tx_bd reg_bd;	/* HSI_COMMENT: The common regular bd */
};

/* Eth Tx Tunnel Type */
enum eth_tx_tunn_type {
	ETH_TX_TUNN_GENEVE,	/* HSI_COMMENT: GENEVE Tunnel. */
	ETH_TX_TUNN_TTAG,	/* HSI_COMMENT: T-Tag Tunnel. */
	ETH_TX_TUNN_GRE,	/* HSI_COMMENT: GRE Tunnel. */
	ETH_TX_TUNN_VXLAN,	/* HSI_COMMENT: VXLAN Tunnel. */
	MAX_ETH_TX_TUNN_TYPE
};

/* Mstorm Queue Zone */
struct mstorm_eth_queue_zone {
	struct eth_rx_prod_data rx_producers;	/* HSI_COMMENT: ETH Rx producers data */
	__le32 reserved[3];
};

/* Ystorm Queue Zone */
struct xstorm_eth_queue_zone {
	struct coalescing_timeset int_coalescing_timeset;	/* HSI_COMMENT: Tx interrupt coalescing TimeSet */
	u8 reserved[7];
};

/* ETH doorbell data */
struct eth_db_data {
	u8 params;
#define ETH_DB_DATA_DEST_MASK   0x3	/* HSI_COMMENT: destination of doorbell (use enum db_dest) */
#define ETH_DB_DATA_DEST_SHIFT  0
#define ETH_DB_DATA_AGG_CMD_MASK        0x3	/* HSI_COMMENT: aggregative command to CM (use enum db_agg_cmd_sel) */
#define ETH_DB_DATA_AGG_CMD_SHIFT       2
#define ETH_DB_DATA_BYPASS_EN_MASK      0x1	/* HSI_COMMENT: enable QM bypass */
#define ETH_DB_DATA_BYPASS_EN_SHIFT     4
#define ETH_DB_DATA_RESERVED_MASK       0x1
#define ETH_DB_DATA_RESERVED_SHIFT      5
#define ETH_DB_DATA_AGG_VAL_SEL_MASK    0x3	/* HSI_COMMENT: aggregative value selection */
#define ETH_DB_DATA_AGG_VAL_SEL_SHIFT   6
	u8 agg_flags;		/* HSI_COMMENT: bit for every DQ counter flags in CM context that DQ can increment */
	__le16 bd_prod;
};

/* RSS hash type */
enum rss_hash_type {
	RSS_HASH_TYPE_DEFAULT = 0,
	RSS_HASH_TYPE_IPV4 = 1,
	RSS_HASH_TYPE_TCP_IPV4 = 2,
	RSS_HASH_TYPE_IPV6 = 3,
	RSS_HASH_TYPE_TCP_IPV6 = 4,
	RSS_HASH_TYPE_UDP_IPV4 = 5,
	RSS_HASH_TYPE_UDP_IPV6 = 6,
	MAX_RSS_HASH_TYPE
};

#endif /* __ETH_COMMON__ */
