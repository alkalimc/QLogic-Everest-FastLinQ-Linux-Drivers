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

#ifndef _QEDE_PTP_H_
#define _QEDE_PTP_H_

#if (defined(QEDE_PTP_SUPPORT) && \
	(defined(CONFIG_PTP_1588_CLOCK) || \
	defined(CONFIG_PTP_1588_CLOCK_MODULE)))	/* QEDE_UPSTREAM */

#include <linux/ptp_clock_kernel.h>
#include <linux/net_tstamp.h>
#ifndef _DEFINE_CYCLECOUNTER_MASK	/* QEDE_UPSTREAM */
#include <linux/timecounter.h>
#else
#include <linux/clocksource.h>
#endif
#include "qede.h"

void qede_ptp_rx_ts(struct qede_dev *, struct sk_buff *);
void qede_ptp_tx_ts(struct qede_dev *, struct sk_buff *);
int qede_ptp_hw_ts(struct qede_dev *, struct ifreq *);
void qede_ptp_disable(struct qede_dev *);
int qede_ptp_enable(struct qede_dev *);
int qede_ptp_get_ts_info(struct qede_dev *, struct ethtool_ts_info *);

static inline void qede_ptp_record_rx_ts(struct qede_dev *edev,
					 union eth_rx_cqe *cqe,
					 struct sk_buff *skb)
{
	/* Check if this packet was timestamped */
	if (unlikely(le16_to_cpu(cqe->fast_path_regular.pars_flags.flags) &
		     (1 << PARSING_AND_ERR_FLAGS_TIMESTAMPRECORDED_SHIFT))) {
		if (likely(le16_to_cpu(cqe->fast_path_regular.pars_flags.flags)
		    & (1 << PARSING_AND_ERR_FLAGS_TIMESYNCPKT_SHIFT))) {
			qede_ptp_rx_ts(edev, skb);
		} else {
			DP_INFO(edev,
				"Timestamp recorded for non PTP packets \n");
		}
	}
}

#else

static inline void qede_ptp_rx_ts(struct qede_dev *dev, struct sk_buff *skb)
{
	return;
}

static inline void qede_ptp_tx_ts(struct qede_dev *dev, struct sk_buff *skb)
{
	return;
}

static inline int qede_ptp_hw_ts(struct qede_dev *dev, struct ifreq *req)
{
	return 0;
}

static inline int qede_ptp_enable(struct qede_dev *dev)
{
	return 0;
}

static inline void qede_ptp_disable(struct qede_dev *dev)
{
	return;
}

struct ethtool_ts_info;
static inline int qede_ptp_get_ts_info(struct qede_dev *edev,
				       struct ethtool_ts_info *info)
{
	return 0;
}

static inline void qede_ptp_record_rx_ts(struct qede_dev *edev,
					 union eth_rx_cqe *cqe,
					 struct sk_buff *skb)
{
}

struct qede_ptp {
	int dummy;
};

#endif /* QEDE_PTP_SUPPORT */

#endif /* _QEDE_PTP_H_ */
