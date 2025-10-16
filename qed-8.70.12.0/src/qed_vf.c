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
#include <asm/param.h>
#include <asm/byteorder.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/crc32.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/if_ether.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#define __PREVENT_DUMP_MEM_ARR__
#define __PREVENT_PXP_GLOBAL_WIN__
#include "qed.h"
#include "qed_eth_if.h"
#include "qed_hsi.h"
#include "qed_int.h"
#include "qed_l2.h"
#include "qed_ll2.h"
#include "qed_mcp.h"
#include "qed_rdma.h"
#include "qed_reg_addr.h"
#include "qed_sriov.h"
#include "qed_vf.h"
#include "qed_rdma_if.h"


#define YSTORM_VFZONE_READ(p_hwfn, offset) \
	REG_RD(p_hwfn, PXP_VF_BAR0_START_YSDM_ZONE_B + (offset))

static const char *const qed_iov_pf_to_vf_status_str[] = {
	"WAITING",
	"SUCCESS",
	"FAILURE",
	"NOT_SUPPORTED",
	"NO_RESOURCE",
	"FORCED",
	"MALICIOUS",
	"ACQUIRED",
};

static int qed_vf_get_channel_liveness(struct qed_hwfn *p_hwfn)
{
	/* Read the first word of YSTORM_VFZONE denoting the channel
	 * status
	 */
	return YSTORM_VFZONE_READ(p_hwfn, 0);
}

static void *qed_vf_pf_prep(struct qed_hwfn	*p_hwfn,
			    u16			type,
			    u16			length)
{
	struct qed_vf_iov	*p_iov = p_hwfn->vf_iov_info;
	void			*p_tlv;

	/* This lock is released when we receive PF's response
	 * in qed_send_msg2pf().
	 * So, qed_vf_pf_prep() and qed_send_msg2pf()
	 * must come in sequence.
	 */
	mutex_lock(&(p_iov->mutex));

	DP_VERBOSE(p_hwfn,
		   QED_MSG_IOV,
		   "preparing to send %s tlv over vf pf channel\n",
		   qed_channel_tlvs_string[type]);

	/* Reset Requst offset */
	p_iov->offset = (u8 *)p_iov->vf2pf_request;

	/* Clear mailbox - both request and reply */
	memset(p_iov->vf2pf_request, 0,
	       sizeof(union vfpf_tlvs));
	memset(p_iov->pf2vf_reply, 0,
	       sizeof(union pfvf_tlvs));

	/* Init type and length */
	p_tlv = qed_add_tlv(&p_iov->offset, type, length);

	/* Init first tlv header */
	((struct vfpf_first_tlv *)p_tlv)->reply_address =
		(u64)p_iov->pf2vf_reply_phys;

	return p_tlv;
}

static void qed_vf_pf_req_end(struct qed_hwfn	*p_hwfn,
			      int		req_status)
{
	union pfvf_tlvs *resp = p_hwfn->vf_iov_info->pf2vf_reply;

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "VF request status = 0x%x, PF reply status = 0x%x\n",
		   req_status, resp->default_resp.hdr.status);

	mutex_unlock(&(p_hwfn->vf_iov_info->mutex));
}


#define QED_VF_CHANNEL_ITERATIONS               200
#define QED_VF_CHANNEL_USLEEP_DELAY             100
#define QED_VF_CHANNEL_MSLEEP_DELAY             25

static int
qed_send_msg2pf(struct qed_hwfn *p_hwfn,
		u8		*done,
		u32		resp_size)
{
	union vfpf_tlvs			*p_req =
		p_hwfn->vf_iov_info->vf2pf_request;
	int				rc = 0;
	struct ustorm_trigger_vf_zone	trigger;
	struct ustorm_vf_zone		*zone_data;
	u32				vfid, pfid;
	int				iter = 0;
	int				ch_val;

	pfid = GET_FIELD(p_hwfn->hw_info.concrete_fid,
			 PXP_CONCRETE_FID_PFID);
	vfid = GET_FIELD(p_hwfn->hw_info.concrete_fid,
			 PXP_CONCRETE_FID_VFID);

	ch_val = qed_vf_get_channel_liveness(p_hwfn);
	/* return if channel is dead */
	if (ch_val) {
		DP_NOTICE(p_hwfn, "Channel is dead VF[%d] ch_val:0x%x\n",
			  vfid, ch_val);

		return -EINVAL;
	}

	zone_data = (struct ustorm_vf_zone *)PXP_VF_BAR0_START_USDM_ZONE_B;

	/* output tlvs list */
	qed_dp_tlv_list(p_hwfn, p_req);

	/* need to add the END TLV to the message size */
	resp_size += sizeof(struct channel_list_end_tlv);


	/* Send TLVs over HW channel */
	memset(&trigger, 0, sizeof(struct ustorm_trigger_vf_zone));
	trigger.vf_pf_msg_valid = 1;

	DP_VERBOSE(
		p_hwfn,
		QED_MSG_IOV,
		"VF[0x%02x-> PF [0x%02x] message: [0x%08x, 0x%08x] --> %p, 0x%08x --> %p\n",
		vfid,
		pfid,
		upper_32_bits(p_hwfn->vf_iov_info->vf2pf_request_phys),
		lower_32_bits(p_hwfn->vf_iov_info->vf2pf_request_phys),
		&zone_data->non_trigger.vf_pf_msg_addr,
		*((u32 *)&trigger),
		&zone_data->trigger);

	REG_WR(p_hwfn,
	       (uintptr_t)&zone_data->non_trigger.vf_pf_msg_addr.lo,
	       lower_32_bits(p_hwfn->vf_iov_info->vf2pf_request_phys));

	REG_WR(p_hwfn,
	       (uintptr_t)&zone_data->non_trigger.vf_pf_msg_addr.hi,
	       upper_32_bits(p_hwfn->vf_iov_info->vf2pf_request_phys));

	/* The message data must be written first, to prevent trigger before
	 * data is written.Memory barrier here is not needed as its already present
	 * REG_WR.
	 */

	REG_WR(p_hwfn, (uintptr_t)&zone_data->trigger, *((u32 *)&trigger));

	/* When PF would be done with the response, it would write back to the
	 * `done' address. Poll until then.
	 */
	while ((!*done) && (iter < QED_VF_CHANNEL_ITERATIONS)) {
		udelay(QED_VF_CHANNEL_USLEEP_DELAY);
		iter++;
	}

	iter = 0;

	while ((!*done) && (iter < QED_VF_CHANNEL_ITERATIONS)) {
		msleep(QED_VF_CHANNEL_MSLEEP_DELAY);
		iter++;
	}

	if (!*done) {
		DP_NOTICE(p_hwfn,
			  "VF[0x%02x] <-- PF[0x%02x] Timeout [Type %d]\n",
			  vfid,
			  pfid,
			  p_req->first_tlv.tl.type);

		rc = -EBUSY;
	} else {
		if ((*done != PFVF_STATUS_SUCCESS) &&
		    (*done != PFVF_STATUS_NO_RESOURCE)) DP_NOTICE(
				p_hwfn,
				"PF[0x%02x] response: %d [Type %d]\n",
				pfid,
				*done,
				p_req->
				first_tlv.tl.type);
		else
			DP_VERBOSE(p_hwfn, QED_MSG_IOV,
				   "PF[0x%02x] response: %d [Type %d]\n",
				   pfid, *done, p_req->first_tlv.tl.type);
	}

	/* Memory barrier here to prevent any speculative execution based on the done value
	 * to return to the calling context and use stale data from the response.
	 */
	rmb();

	return rc;
}

static void qed_vf_pf_add_qid(struct qed_hwfn		*p_hwfn,
			      struct qed_queue_cid	*p_cid)
{
	struct qed_vf_iov	*p_iov = p_hwfn->vf_iov_info;
	struct vfpf_qid_tlv	*p_qid_tlv;

	/* Only add QIDs for the queue if it was negotiated with PF */
	if (!(p_iov->acquire_resp.pfdev_info.capabilities &
	      PFVF_ACQUIRE_CAP_QUEUE_QIDS))
		return;

	p_qid_tlv = qed_add_tlv(&p_iov->offset,
				CHANNEL_TLV_QID, sizeof(*p_qid_tlv));
	p_qid_tlv->qid = p_cid->qid_usage_idx;
}

static int _qed_vf_pf_release(struct qed_hwfn	*p_hwfn,
			      bool		b_final)
{
	struct qed_vf_iov		*p_iov = p_hwfn->vf_iov_info;
	struct pfvf_def_resp_tlv	*resp;
	struct vfpf_first_tlv		*req;
	u32				size;
	int				rc;

	/* clear mailbox and prep first tlv */
	req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RELEASE, sizeof(*req));

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	resp	= &p_iov->pf2vf_reply->default_resp;
	rc	= qed_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));

	if (rc == 0 && resp->hdr.status != PFVF_STATUS_SUCCESS)
		rc = -EAGAIN;

	qed_vf_pf_req_end(p_hwfn, rc);
	if (!b_final)
		return rc;

	p_hwfn->b_int_enabled = 0;

	if (p_iov->vf2pf_request) dma_free_coherent(&p_hwfn->cdev->pdev->dev,
						    sizeof(union vfpf_tlvs),
						    p_iov->vf2pf_request,
						    p_iov->vf2pf_request_phys);
	if (p_iov->pf2vf_reply) dma_free_coherent(&p_hwfn->cdev->pdev->dev,
						  sizeof(union pfvf_tlvs),
						  p_iov->pf2vf_reply,
						  p_iov->pf2vf_reply_phys);

	if (p_iov->bulletin.p_virt) {
		size = sizeof(struct qed_bulletin_content);
		dma_free_coherent(&p_hwfn->cdev->pdev->dev,
				  size,
				  p_iov->bulletin.p_virt,
				  p_iov->bulletin.phys);
	}


	kfree(p_hwfn->vf_iov_info);
	p_hwfn->vf_iov_info = NULL;

	return rc;
}

int qed_vf_pf_release(struct qed_hwfn *p_hwfn)
{
	return _qed_vf_pf_release(p_hwfn, true);
}

static void
qed_vf_pf_rdma_acquire_reduce_resc(struct qed_hwfn			*p_hwfn,
				   struct vfpf_rdma_acquire_tlv		*p_req,
				   struct pfvf_rdma_acquire_resp_tlv	*p_resp)
{
	DP_VERBOSE(
		p_hwfn,
		QED_MSG_IOV,
		"PF unwilling to fullill resource request: cnqs [%u/%u]. Try PF recommended amount\n",
		p_req->num_cnqs,
		p_resp->num_cnqs);

	p_req->num_cnqs = p_resp->num_cnqs;
}

static void qed_vf_pf_acquire_reduce_resc(struct qed_hwfn		*p_hwfn,
					  struct vf_pf_resc_request	*p_req,
					  struct pf_vf_resc		*p_resp)
{
	DP_VERBOSE(
		p_hwfn,
		QED_MSG_IOV,
		"PF unwilling to fullill resource request: rxq [%u/%u] txq [%u/%u] sbs [%u/%u] mac [%u/%u] vlan [%u/%u] mc [%u/%u] cids [%u/%u]. Try PF recommended amount\n",
		p_req->num_rxqs,
		p_resp->num_rxqs,
		p_req->num_rxqs,
		p_resp->num_txqs,
		p_req->num_sbs,
		p_resp->num_sbs,
		p_req->num_mac_filters,
		p_resp->num_mac_filters,
		p_req->num_vlan_filters,
		p_resp->num_vlan_filters,
		p_req->num_mc_filters,
		p_resp->num_mc_filters,
		p_req->num_cids,
		p_resp->num_cids);

	/* humble our request */
	p_req->num_txqs		= p_resp->num_txqs;
	p_req->num_rxqs		= p_resp->num_rxqs;
	p_req->num_sbs		= p_resp->num_sbs;
	p_req->num_mac_filters	= p_resp->num_mac_filters;
	p_req->num_vlan_filters = p_resp->num_vlan_filters;
	p_req->num_mc_filters	= p_resp->num_mc_filters;
	p_req->num_cids		= p_resp->num_cids;
}

static int qed_vf_pf_rdma_acquire(struct qed_hwfn *p_hwfn)
{
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	struct pfvf_rdma_acquire_resp_tlv	*resp	=
		&p_iov->pf2vf_reply->rdma_acquire_resp;
	int					rc = 0;
	struct vfpf_rdma_acquire_tlv		*req;
	struct qed_dpi_info			*dpi_info =
		&p_hwfn->dpi_info;
	bool					resources_acquired	= false;
	int					attempts		= 0;

	/* clear mailbox and prep first tlv */
	req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_ACQUIRE, sizeof(*req));

	req->num_cnqs = QED_MAX_CNQ_VF_CHAINS_PER_PF;


	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	while (!resources_acquired) {
		DP_VERBOSE(p_hwfn,
			   QED_MSG_IOV,
			   "attempting to acquire resources\n");

		/* Clear response buffer, as this might be a re-send */
		memset(p_iov->pf2vf_reply, 0,
		       sizeof(union pfvf_tlvs));

		/* send rdma acquire request */
		rc = qed_send_msg2pf(p_hwfn,
				     &resp->hdr.status,
				     sizeof(*resp));
		if (rc)
			goto exit;

		attempts++;

		if (resp->hdr.status == PFVF_STATUS_SUCCESS) {
			/* PF agrees to allocate our resources */
			DP_VERBOSE(p_hwfn,
				   QED_MSG_IOV,
				   "RDMA resources acquired\n");
			resources_acquired = true;
		} else if (resp->hdr.status == PFVF_STATUS_NO_RESOURCE &&
			   attempts < QED_VF_ACQUIRE_THRESH) {
			/* PF refuses to allocate our resources */
			qed_vf_pf_rdma_acquire_reduce_resc(p_hwfn, req, resp);
		} else {
			DP_ERR(
				p_hwfn,
				"PF returned status:%s to VF RDMA acquisition request\n",
				(resp->hdr.status < PFVF_STATUS_MAX) ?
				qed_iov_pf_to_vf_status_str[resp->hdr.status] :
				"Unknown");

			rc = -EAGAIN;
			goto exit;
		}
	}

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "resc acquired : cnqs [%u]\n", resp->num_cnqs);

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "max_queue_zones=%u\n", resp->max_queue_zones);

	dpi_info->wid_count		= resp->wid_cound;
	dpi_info->dpi_count		= resp->dpi_count;
	dpi_info->dpi_size		= resp->dpi_size;
	dpi_info->dpi_start_offset	= resp->dpi_start_offset;

	DP_VERBOSE(
		p_hwfn,
		QED_MSG_IOV,
		"PWM region data: wid_count=%d, dpi_count=%d, dpi_size=%d, dpi_start_offset=0x%x\n",
		dpi_info->wid_count,
		dpi_info->dpi_count,
		dpi_info->dpi_size,
		dpi_info->dpi_start_offset);


	/* copy acquire response from buffer to p_hwfn */
	memcpy(&p_iov->rdma_acquire_resp, resp,
	       sizeof(p_iov->rdma_acquire_resp));

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

static int
qed_vf_pf_soft_flr_acquire(struct qed_hwfn *p_hwfn)
{
	struct qed_vf_iov		*p_iov = p_hwfn->vf_iov_info;
	struct pfvf_def_resp_tlv	*resp;
	struct vfpf_soft_flr_tlv	*req;
	int				rc;

	req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_SOFT_FLR, sizeof(*req));

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	resp	= &p_iov->pf2vf_reply->default_resp;
	rc	= qed_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));

	DP_VERBOSE(p_hwfn, QED_MSG_IOV, "rc=0x%x\n", rc);

	/* to release the mutex as qed_vf_pf_acquire() take the mutex */
	qed_vf_pf_req_end(p_hwfn, -EAGAIN);

	/* As of today, there is no mechanism in place for VF to know the FLR
	 * status, so sufficiently (worst case time) wait for FLR to complete,
	 * as mailbox request to MFW by the PF for initiating VF flr and PF
	 * processing VF FLR could take time.
	 */
	msleep(3000);

	return qed_vf_pf_acquire(p_hwfn);
}

int qed_vf_pf_acquire(struct qed_hwfn *p_hwfn)
{
	struct qed_vf_iov		*p_iov	= p_hwfn->vf_iov_info;
	struct pfvf_acquire_resp_tlv	*resp	=
		&p_iov->pf2vf_reply->acquire_resp;
	struct pf_vf_pfdev_info		*pfdev_info = &resp->pfdev_info;
	struct qed_vf_acquire_sw_info	vf_sw_info;
	struct qed_dev			*cdev		= p_hwfn->cdev;
	u8				retry_cnt	=
		p_iov->acquire_retry_cnt;
	struct vf_pf_resc_request	*p_resc;
	bool				resources_acquired = false;
	struct vfpf_acquire_tlv		*req;
	int				attempts	= 0;
	int				rc		= 0;

	/* clear mailbox and prep first tlv */
	req	= qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_ACQUIRE, sizeof(*req));
	p_resc	= &req->resc_request;

	/* @@@ TBD: PF may not be ready bnx2x_get_vf_id... */
	req->vfdev_info.opaque_fid = p_hwfn->hw_info.opaque_fid;

	p_resc->num_rxqs		= QED_MAX_QUEUE_VF_CHAINS_PER_PF;
	p_resc->num_txqs		= QED_MAX_QUEUE_VF_CHAINS_PER_PF;
	p_resc->num_sbs			= QED_MAX_VF_CHAINS_PER_PF;
	p_resc->num_mac_filters		= QED_ETH_VF_NUM_MAC_FILTERS;
	p_resc->num_vlan_filters	= QED_ETH_VF_NUM_VLAN_FILTERS;
	p_resc->num_cids		= QED_ETH_VF_DEFAULT_NUM_CIDS;

	memset(&vf_sw_info, 0, sizeof(vf_sw_info));
	qed_vf_fill_driver_data(p_hwfn, &vf_sw_info);

	req->vfdev_info.os_type			= vf_sw_info.os_type;
	req->vfdev_info.driver_version		= vf_sw_info.driver_version;
	req->vfdev_info.fw_major		= FW_MAJOR_VERSION;
	req->vfdev_info.fw_minor		= FW_MINOR_VERSION;
	req->vfdev_info.fw_revision		= FW_REVISION_VERSION;
	req->vfdev_info.fw_engineering		= FW_ENGINEERING_VERSION;
	req->vfdev_info.eth_fp_hsi_major	= ETH_HSI_VER_MAJOR;
	req->vfdev_info.eth_fp_hsi_minor	= ETH_HSI_VER_MINOR;

	/* Fill capability field with any non-deprecated config we support */
	req->vfdev_info.capabilities |= VFPF_ACQUIRE_CAP_100G;

	/* If doorbell bar size is sufficient, try using queue qids */
	if (p_hwfn->db_size > PXP_VF_BAR0_DQ_LENGTH) {
		req->vfdev_info.capabilities |= VFPF_ACQUIRE_CAP_PHYSICAL_BAR |
			VFPF_ACQUIRE_CAP_QUEUE_QIDS;
		p_resc->num_cids = QED_ETH_VF_MAX_NUM_CIDS;
	}

	/* Currently, fill ROCE by default. */
	req->vfdev_info.capabilities |= VFPF_ACQUIRE_CAP_ROCE;

	/* Check if VF uses EDPM and needs DORQ flush when overflowed */
	if (p_hwfn->roce_edpm_mode != QED_ROCE_EDPM_MODE_DISABLE)
		req->vfdev_info.capabilities |= VFPF_ACQUIRE_CAP_EDPM;

	/* This VF version supports async events */
	req->vfdev_info.capabilities |= VFPF_ACQUIRE_CAP_EQ;

	/* Set when IWARP is supported. */
	/* req->vfdev_info.capabilities |= VFPF_ACQUIRE_CAP_IWARP; */

	/* pf 2 vf bulletin board address */
	req->bulletin_addr	= p_iov->bulletin.phys;
	req->bulletin_size	= p_iov->bulletin.size;

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	while (!resources_acquired) {
		DP_VERBOSE(p_hwfn,
			   QED_MSG_IOV,
			   "attempting to acquire resources\n");

		/* Clear response buffer, as this might be a re-send */
		memset(p_iov->pf2vf_reply, 0,
		       sizeof(union pfvf_tlvs));

		/* send acquire request */
		rc = qed_send_msg2pf(p_hwfn,
				     &resp->hdr.status,
				     sizeof(*resp));
		/* In general, PF shouldn't take long time to respond for a
		 * successful message sent by the VF over VPC. If for
		 * sufficiently long time (~2.5 sec) there is no response,
		 * which is mainly possible due to VF being FLRed which could
		 * cause message send failure by VF to the PF over VPC as being
		 * disallowed due to FLR and cause such timeout, in such cases
		 * retry for the acquire message which can later be successful
		 * after VF FLR handling is completed.
		 */
		if (retry_cnt && rc == -EBUSY) {
			DP_VERBOSE(
				p_hwfn,
				QED_MSG_IOV,
				"VF retrying to acquire due to VPC timeout\n");
			retry_cnt--;
			continue;
		}

		if (rc)
			goto exit;

		/* copy acquire response from buffer to p_hwfn */
		memcpy(&p_iov->acquire_resp,
		       resp,
		       sizeof(p_iov->acquire_resp));

		attempts++;

		if (resp->hdr.status == PFVF_STATUS_SUCCESS) {
			/* PF agrees to allocate our resources */
			if (!(resp->pfdev_info.capabilities &
			      PFVF_ACQUIRE_CAP_POST_FW_OVERRIDE)) {
				/* It's possible legacy PF mistakenly accepted;
				 * but we don't care - simply mark it as
				 * legacy and continue.
				 */
				req->vfdev_info.capabilities |=
					VFPF_ACQUIRE_CAP_PRE_FP_HSI;
			}

			DP_VERBOSE(p_hwfn, (QED_MSG_IOV | QED_MSG_RDMA),
				   "VF AQUIRE-RDMA capabilities is:%llx\n",
				   resp->pfdev_info.capabilities);

			if (resp->pfdev_info.capabilities &
			    PFVF_ACQUIRE_CAP_ROCE)
				p_hwfn->hw_info.personality =
					QED_PCI_ETH_ROCE;

			if (resp->pfdev_info.capabilities &
			    PFVF_ACQUIRE_CAP_IWARP)
				p_hwfn->hw_info.personality =
					QED_PCI_ETH_IWARP;

			DP_VERBOSE(p_hwfn, QED_MSG_IOV, "resources acquired\n");
			resources_acquired = true;
		} /* PF refuses to allocate our resources */
		else if (resp->hdr.status == PFVF_STATUS_NO_RESOURCE &&
			 attempts < QED_VF_ACQUIRE_THRESH) {
			qed_vf_pf_acquire_reduce_resc(p_hwfn, p_resc,
						      &resp->resc);
		} else if (resp->hdr.status == PFVF_STATUS_NOT_SUPPORTED) {
			if (pfdev_info->major_fp_hsi &&
			    (pfdev_info->major_fp_hsi != ETH_HSI_VER_MAJOR)) {
				DP_NOTICE(
					p_hwfn,
					"PF uses an incompatible fastpath HSI %02x.%02x [VF requires %02x.%02x]. Please change to a VF driver using %02x.xx.\n",
					pfdev_info->major_fp_hsi,
					pfdev_info->minor_fp_hsi,
					ETH_HSI_VER_MAJOR,
					ETH_HSI_VER_MINOR,
					pfdev_info->major_fp_hsi);
				rc = -EINVAL;
				goto exit;
			}

			if (!pfdev_info->major_fp_hsi) {
				if (req->vfdev_info.capabilities &
				    VFPF_ACQUIRE_CAP_PRE_FP_HSI) {
					DP_NOTICE(
						p_hwfn,
						"PF uses very old drivers. Please change to a VF driver using no later than 8.8.x.x.\n");
					rc = -EINVAL;
					goto exit;
				} else {
					DP_INFO(
						p_hwfn,
						"PF is old - try re-acquire to see if it supports FW-version override\n");
					req->vfdev_info.capabilities |=
						VFPF_ACQUIRE_CAP_PRE_FP_HSI;
					continue;
				}
			}

			/* If PF/VF are using same Major, PF must have had
			 * it's reasons. Simply fail.
			 */
			DP_NOTICE(p_hwfn, "PF rejected acquisition by VF\n");
			rc = -EINVAL;
			goto exit;
		} else if (resp->hdr.status == PFVF_STATUS_ACQUIRED) {
			qed_vf_pf_req_end(p_hwfn, -EAGAIN);
			return qed_vf_pf_soft_flr_acquire(p_hwfn);
		} else {
			DP_ERR(
				p_hwfn,
				"PF returned error %d to VF acquisition request\n",
				resp->hdr.status);
			rc = -EAGAIN;
			goto exit;
		}
	}

	DP_VERBOSE(
		p_hwfn,
		QED_MSG_IOV,
		"resc acquired : rxq [%u] txq [%u] sbs [%u] mac [%u] vlan [%u] mc [%u] cids [%u].\n",
		resp->resc.num_rxqs,
		resp->resc.num_txqs,
		resp->resc.num_sbs,
		resp->resc.num_mac_filters,
		resp->resc.num_vlan_filters,
		resp->resc.num_mc_filters,
		resp->resc.num_cids);

	/* Mark the PF as legacy, if needed */
	if (req->vfdev_info.capabilities &
	    VFPF_ACQUIRE_CAP_PRE_FP_HSI)
		p_iov->b_pre_fp_hsi = true;

	/* In case PF doesn't support multi-queue Tx, update the number of
	 * CIDs to reflect the number of queues [older PFs didn't fill that
	 * field].
	 */
	if (!(resp->pfdev_info.capabilities &
	      PFVF_ACQUIRE_CAP_QUEUE_QIDS))
		resp->resc.num_cids = resp->resc.num_rxqs +
			resp->resc.num_txqs;


	/* Update bulletin board size with response from PF */
	p_iov->bulletin.size = resp->bulletin_size;

	/* get HW info */
	cdev->type		= resp->pfdev_info.dev_type;
	cdev->chip_rev		= (u8)resp->pfdev_info.chip_rev;
	cdev->chip_metal	= (u8)resp->pfdev_info.chip_metal;

	DP_INFO(p_hwfn, "Chip details - %s %d\n",
		QED_IS_BB(cdev) ? "BB" : "AH",
		CHIP_REV_IS_A0(p_hwfn->cdev) ? 0 :
		cdev->chip_metal);

	cdev->chip_num = pfdev_info->chip_num & 0xffff;

	/* Learn of the possibility of CMT */
	if (IS_LEAD_HWFN(p_hwfn)) {
		if (resp->pfdev_info.capabilities & PFVF_ACQUIRE_CAP_100G) {
			DP_NOTICE(p_hwfn, "100g VF\n");
			cdev->num_hwfns = 2;
		}
	}

	if (!p_iov->b_pre_fp_hsi &&
	    (resp->pfdev_info.minor_fp_hsi < ETH_HSI_VER_MINOR)) {
		DP_INFO(
			p_hwfn,
			"PF is using older fastpath HSI; %02x.%02x is configured\n",
			ETH_HSI_VER_MAJOR,
			resp->pfdev_info.minor_fp_hsi);
	}

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

u32 qed_vf_hw_bar_size(struct qed_hwfn	*p_hwfn,
		       enum BAR_ID	bar_id)
{
	u32 bar_size;

	/* Regview size is fixed */
	if (bar_id == BAR_ID_0)
		return 1 << 17;

	/* Doorbell is received from PF */
	bar_size = p_hwfn->vf_iov_info->acquire_resp.pfdev_info.bar_size;
	if (bar_size)
		return 1 << bar_size;
	return 0;
}

int
qed_vf_hw_prepare(struct qed_hwfn		*p_hwfn,
		  struct qed_hw_prepare_params	*p_params)
{
	struct qed_hwfn		*p_lead = QED_LEADING_HWFN(p_hwfn->cdev);
	struct qed_vf_iov	*p_iov;
	u32			reg;
	int			rc;

	/* Set number of hwfns - might be overriden once leading hwfn learns
	 * actual configuration from PF.
	 */
	if (IS_LEAD_HWFN(p_hwfn))
		p_hwfn->cdev->num_hwfns = 1;

	reg				= PXP_VF_BAR0_ME_OPAQUE_ADDRESS;
	p_hwfn->hw_info.opaque_fid	= (u16)REG_RD(p_hwfn, reg);

	reg				= PXP_VF_BAR0_ME_CONCRETE_ADDRESS;
	p_hwfn->hw_info.concrete_fid	= REG_RD(p_hwfn, reg);

	/* Allocate vf sriov info */
	p_iov = kzalloc(sizeof(*p_iov), GFP_KERNEL);
	if (!p_iov) {
		DP_NOTICE(p_hwfn, "Failed to allocate `struct qed_sriov'\n");
		return -ENOMEM;
	}

	/* Doorbells are tricky; Upper-layer has alreday set the hwfn doorbell
	 * value, but there are several incompatibily scenarios where that
	 * would be incorrect and we'd need to override it.
	 */
	if (p_hwfn->doorbells == NULL) {
		p_hwfn->doorbells = (u8 __iomem *)p_hwfn->regview +
			PXP_VF_BAR0_START_DQ;
		p_hwfn->db_size = PXP_VF_BAR0_DQ_LENGTH;
	} else if (p_hwfn == p_lead) {
		/* For leading hw-function, value is always correct, but need
		 * to handle scenario where legacy PF would not support 100g
		 * mapped bars later.
		 */
		p_iov->b_doorbell_bar = true;
	} else {
		/* here, value would be correct ONLY if the leading hwfn
		 * received indication that mapped-bars are supported.
		 */
		if (p_lead->vf_iov_info->b_doorbell_bar)
			p_iov->b_doorbell_bar = true;
		else
			p_hwfn->doorbells = (u8 __iomem *)p_hwfn->regview +
				PXP_VF_BAR0_START_DQ;
	}

	/* Allocate vf2pf msg */
	p_iov->vf2pf_request = dma_alloc_coherent(&p_hwfn->cdev->pdev->dev,
						  sizeof(union vfpf_tlvs),
						  &p_iov->vf2pf_request_phys,
						  GFP_KERNEL);
	if (!p_iov->vf2pf_request) {
		DP_NOTICE(p_hwfn,
			  "Failed to allocate `vf2pf_request' DMA memory\n");
		goto free_p_iov;
	}

	p_iov->pf2vf_reply = dma_alloc_coherent(&p_hwfn->cdev->pdev->dev,
						sizeof(union pfvf_tlvs),
						&p_iov->pf2vf_reply_phys,
						GFP_KERNEL);
	if (!p_iov->pf2vf_reply) {
		DP_NOTICE(p_hwfn,
			  "Failed to allocate `pf2vf_reply' DMA memory\n");
		goto free_vf2pf_request;
	}

	DP_VERBOSE(
		p_hwfn,
		QED_MSG_IOV,
		"VF's Request mailbox [%p virt 0x%llx phys], Response mailbox [%p virt 0x%llx phys]\n",
		p_iov->vf2pf_request,
		(u64)p_iov->vf2pf_request_phys,
		p_iov->pf2vf_reply,
		(u64)p_iov->pf2vf_reply_phys);

	/* Allocate Bulletin board */
	p_iov->bulletin.size	= sizeof(struct qed_bulletin_content);
	p_iov->bulletin.p_virt	= dma_alloc_coherent(&p_hwfn->cdev->pdev->dev,
						     p_iov->bulletin.size,
						     &p_iov->bulletin.phys,
						     GFP_KERNEL);
	if (!p_iov->bulletin.p_virt) {
		DP_NOTICE(p_hwfn, "Failed to alloc bulletin memory\n");
		goto free_pf2vf_reply;
	}
	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "VF's bulletin Board [%p virt 0x%llx phys 0x%08x bytes]\n",
		   p_iov->bulletin.p_virt,
		   (u64)p_iov->bulletin.phys,
		   p_iov->bulletin.size);

	mutex_init(&p_iov->mutex);

	p_iov->acquire_retry_cnt	= p_params->acquire_retry_cnt;
	p_hwfn->vf_iov_info		= p_iov;

	p_hwfn->hw_info.personality = QED_PCI_ETH;

	rc = qed_vf_pf_acquire(p_hwfn);
	if (rc)
		return rc;

	/* Personality will be set in the response of ACQUIRE TLV above.
	 * In case rdma_acquire fails, set personality to ETH.
	 */
	if (QED_IS_RDMA_PERSONALITY(p_hwfn)) {
		if (qed_vf_pf_rdma_acquire(p_hwfn) != 0) {
			DP_ERR(
				p_hwfn->cdev,
				"PF declined RDMA capabilities for VF. Proceeding as L2 only\n");
			p_hwfn->hw_info.personality = QED_PCI_ETH;
		}
	}

	/* If VF is 100g using a mapped bar and PF is too old to support that,
	 * acquisition would succeed - but the VF would have no way knowing
	 * the size of the doorbell bar configured in HW and thus will not
	 * know how to split it for 2nd hw-function.
	 * In this case we re-try without the indication of the mapped
	 * doorbell.
	 */
	if (rc == 0 &&
	    p_iov->b_doorbell_bar &&
	    !qed_vf_hw_bar_size(p_hwfn, BAR_ID_1) &&
	    QED_IS_CMT(p_hwfn->cdev)) {
		rc = _qed_vf_pf_release(p_hwfn, false);
		if (rc)
			return rc;

		p_iov->b_doorbell_bar	= false;
		p_hwfn->doorbells	= (u8 __iomem *)p_hwfn->regview +
			PXP_VF_BAR0_START_DQ;
		rc = qed_vf_pf_acquire(p_hwfn);
	}

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Regview [%p], Doorbell [%p], Device-doorbell [%p]\n",
		   p_hwfn->regview, p_hwfn->doorbells,
		   p_hwfn->cdev->doorbells);

	return rc;

free_pf2vf_reply: dma_free_coherent(&p_hwfn->cdev->pdev->dev,
				    sizeof(union pfvf_tlvs),
				    p_iov->pf2vf_reply,
				    p_iov->pf2vf_reply_phys);
free_vf2pf_request: dma_free_coherent(&p_hwfn->cdev->pdev->dev,
				      sizeof(union vfpf_tlvs),
				      p_iov->vf2pf_request,
				      p_iov->vf2pf_request_phys);
free_p_iov:
	kfree(p_iov);

	return -ENOMEM;
}

static void
__qed_vf_prep_tunn_req_tlv(struct vfpf_update_tunn_param_tlv	*p_req,
			   struct qed_tunn_update_type		*p_src,
			   enum qed_tunn_mode			mask,
			   u8					*p_cls)
{
	if (p_src->b_update_mode) {
		p_req->tun_mode_update_mask |= BIT(mask);

		if (p_src->b_mode_enabled)
			p_req->tunn_mode |= BIT(mask);
	}

	*p_cls = p_src->tun_cls;
}

static void
qed_vf_prep_tunn_req_tlv(struct vfpf_update_tunn_param_tlv	*p_req,
			 struct qed_tunn_update_type		*p_src,
			 enum qed_tunn_mode			mask,
			 u8					*p_cls,
			 struct qed_tunn_update_udp_port	*p_port,
			 u8					*p_update_port,
			 u16					*p_udp_port)
{
	if (p_port->b_update_port) {
		*p_update_port	= 1;
		*p_udp_port	= p_port->port;
	}

	__qed_vf_prep_tunn_req_tlv(p_req, p_src, mask, p_cls);
}

void qed_vf_set_vf_start_tunn_update_param(struct qed_tunnel_info *p_tun)
{
	if (p_tun->vxlan.b_mode_enabled)
		p_tun->vxlan.b_update_mode = true;
	if (p_tun->l2_geneve.b_mode_enabled)
		p_tun->l2_geneve.b_update_mode = true;
	if (p_tun->ip_geneve.b_mode_enabled)
		p_tun->ip_geneve.b_update_mode = true;
	if (p_tun->l2_gre.b_mode_enabled)
		p_tun->l2_gre.b_update_mode = true;
	if (p_tun->ip_gre.b_mode_enabled)
		p_tun->ip_gre.b_update_mode = true;

	p_tun->b_update_rx_cls	= true;
	p_tun->b_update_tx_cls	= true;
}

static void
__qed_vf_update_tunn_param(struct qed_tunn_update_type	*p_tun,
			   u16				feature_mask,
			   u8				tunn_mode,
			   u8				tunn_cls,
			   enum qed_tunn_mode		val)
{
	if (feature_mask & BIT(val)) {
		p_tun->b_mode_enabled	= tunn_mode;
		p_tun->tun_cls		= tunn_cls;
	} else {
		p_tun->b_mode_enabled = false;
	}
}

static void
qed_vf_update_tunn_param(struct qed_hwfn			*p_hwfn,
			 struct qed_tunnel_info			*p_tun,
			 struct pfvf_update_tunn_param_tlv	*p_resp)
{
	/* Update mode and classes provided by PF */
	u16 feat_mask = p_resp->tunn_feature_mask;

	__qed_vf_update_tunn_param(&p_tun->vxlan, feat_mask,
				   p_resp->vxlan_mode, p_resp->vxlan_clss,
				   QED_MODE_VXLAN_TUNN);
	__qed_vf_update_tunn_param(&p_tun->l2_geneve, feat_mask,
				   p_resp->l2geneve_mode,
				   p_resp->l2geneve_clss,
				   QED_MODE_L2GENEVE_TUNN);
	__qed_vf_update_tunn_param(&p_tun->ip_geneve, feat_mask,
				   p_resp->ipgeneve_mode,
				   p_resp->ipgeneve_clss,
				   QED_MODE_IPGENEVE_TUNN);
	__qed_vf_update_tunn_param(&p_tun->l2_gre, feat_mask,
				   p_resp->l2gre_mode, p_resp->l2gre_clss,
				   QED_MODE_L2GRE_TUNN);
	__qed_vf_update_tunn_param(&p_tun->ip_gre, feat_mask,
				   p_resp->ipgre_mode, p_resp->ipgre_clss,
				   QED_MODE_IPGRE_TUNN);
	p_tun->geneve_port.port = p_resp->geneve_udp_port;
	p_tun->vxlan_port.port	= p_resp->vxlan_udp_port;

	DP_VERBOSE(
		p_hwfn,
		QED_MSG_IOV,
		"tunn mode: vxlan=0x%x, l2geneve=0x%x, ipgeneve=0x%x, l2gre=0x%x, ipgre=0x%x",
		p_tun->vxlan.b_mode_enabled,
		p_tun->l2_geneve.b_mode_enabled,
		p_tun->ip_geneve.b_mode_enabled,
		p_tun->l2_gre.b_mode_enabled,
		p_tun->ip_gre.b_mode_enabled);
}

int
qed_vf_pf_tunnel_param_update(struct qed_hwfn		*p_hwfn,
			      struct qed_tunnel_info	*p_src)
{
	struct qed_tunnel_info			*p_tun	= &p_hwfn->cdev->tunnel;
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	struct pfvf_update_tunn_param_tlv	*p_resp;
	struct vfpf_update_tunn_param_tlv	*p_req;
	int					rc;

	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_UPDATE_TUNN_PARAM,
			       sizeof(*p_req));

	if (p_src->b_update_rx_cls && p_src->b_update_tx_cls)
		p_req->update_tun_cls = 1;

	qed_vf_prep_tunn_req_tlv(p_req, &p_src->vxlan, QED_MODE_VXLAN_TUNN,
				 &p_req->vxlan_clss, &p_src->vxlan_port,
				 &p_req->update_vxlan_port,
				 &p_req->vxlan_port);
	p_req->update_non_l2_vxlan	= p_src->update_non_l2_vxlan;
	p_req->non_l2_vxlan_enable	= p_src->non_l2_vxlan_enable;
	qed_vf_prep_tunn_req_tlv(p_req, &p_src->l2_geneve,
				 QED_MODE_L2GENEVE_TUNN,
				 &p_req->l2geneve_clss, &p_src->geneve_port,
				 &p_req->update_geneve_port,
				 &p_req->geneve_port);
	__qed_vf_prep_tunn_req_tlv(p_req, &p_src->ip_geneve,
				   QED_MODE_IPGENEVE_TUNN,
				   &p_req->ipgeneve_clss);
	__qed_vf_prep_tunn_req_tlv(p_req, &p_src->l2_gre,
				   QED_MODE_L2GRE_TUNN, &p_req->l2gre_clss);
	__qed_vf_prep_tunn_req_tlv(p_req, &p_src->ip_gre,
				   QED_MODE_IPGRE_TUNN, &p_req->ipgre_clss);

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp	= &p_iov->pf2vf_reply->tunn_param_resp;
	rc	= qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));

	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		DP_VERBOSE(p_hwfn, QED_MSG_IOV,
			   "Failed to update tunnel parameters\n");
		rc = -EINVAL;
	}

	qed_vf_update_tunn_param(p_hwfn, p_tun, p_resp);
exit:
	qed_vf_pf_req_end(p_hwfn, rc);
	return rc;
}

int
qed_vf_pf_rxq_start(struct qed_hwfn		*p_hwfn,
		    struct qed_queue_cid	*p_cid,
		    u16				bd_max_bytes,
		    dma_addr_t			bd_chain_phys_addr,
		    dma_addr_t			cqe_pbl_addr,
		    u16				cqe_pbl_size,
		    void __iomem		**pp_prod)
{
	struct qed_vf_iov			*p_iov = p_hwfn->vf_iov_info;
	struct pfvf_start_queue_resp_tlv	*resp;
	struct vfpf_start_rxq_tlv		*req;
	u16					rx_qid = p_cid->rel.queue_id;
	int					rc;

	/* clear mailbox and prep first tlv */
	req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_START_RXQ, sizeof(*req));

	req->rx_qid		= rx_qid;
	req->cqe_pbl_addr	= cqe_pbl_addr;
	req->cqe_pbl_size	= cqe_pbl_size;
	req->rxq_addr		= bd_chain_phys_addr;
	req->hw_sb		= p_cid->sb_igu_id;
	req->sb_index		= p_cid->sb_idx;
	req->bd_max_bytes	= bd_max_bytes;
	req->stat_id		= -1; /* Keep initialized, for future compatibility */

	/* If PF is legacy, we'll need to calculate producers ourselves
	 * as well as clean them.
	 */
	if (p_iov->b_pre_fp_hsi) {
		u8	hw_qid =
			p_iov->acquire_resp.resc.hw_qid[rx_qid];
		u32	init_prod_val = 0;

		*pp_prod = (u8 __iomem *)p_hwfn->regview +
			MSTORM_QZONE_START(p_hwfn->cdev) +
			hw_qid * MSTORM_QZONE_SIZE;

		/* Init the rcq, rx bd and rx sge (if valid) producers to 0 */
		__internal_ram_wr(p_hwfn, *pp_prod, sizeof(u32),
				  (u32 *)(&init_prod_val));
	}

	qed_vf_pf_add_qid(p_hwfn, p_cid);

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	resp	= &p_iov->pf2vf_reply->queue_start;
	rc	= qed_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		goto exit;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

	/* Learn the address of the producer from the response */
	if (!p_iov->b_pre_fp_hsi) {
		u32 init_prod_val = 0;

		*pp_prod = (u8 __iomem *)p_hwfn->regview + resp->offset;
		DP_VERBOSE(p_hwfn, QED_MSG_IOV,
			   "Rxq[0x%02x]: producer at %p [offset 0x%08x]\n",
			   rx_qid, *pp_prod, resp->offset);

		/* Init the rcq, rx bd and rx sge (if valid) producers to 0.
		 * It was actually the PF's responsibility, but since some
		 * old PFs might fail to do so, we do this as well.
		 */
		BUILD_BUG_ON(ETH_HSI_VER_MAJOR != 3);
		__internal_ram_wr(p_hwfn, *pp_prod, sizeof(u32),
				  (u32 *)&init_prod_val);
	}

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int qed_vf_pf_rxq_stop(struct qed_hwfn		*p_hwfn,
		       struct qed_queue_cid	*p_cid,
		       bool			cqe_completion)
{
	struct qed_vf_iov		*p_iov = p_hwfn->vf_iov_info;
	struct vfpf_stop_rxqs_tlv	*req;
	struct pfvf_def_resp_tlv	*resp;
	int				rc;

	/* clear mailbox and prep first tlv */
	req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_STOP_RXQS, sizeof(*req));

	req->rx_qid		= p_cid->rel.queue_id;
	req->num_rxqs		= 1;
	req->cqe_completion	= cqe_completion;

	qed_vf_pf_add_qid(p_hwfn, p_cid);

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	resp	= &p_iov->pf2vf_reply->default_resp;
	rc	= qed_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		goto exit;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_txq_start(struct qed_hwfn		*p_hwfn,
		    struct qed_queue_cid	*p_cid,
		    u8				tc,
		    dma_addr_t			pbl_addr,
		    u16				pbl_size,
		    void __iomem		**pp_doorbell)
{
	struct qed_vf_iov			*p_iov = p_hwfn->vf_iov_info;
	struct pfvf_start_queue_resp_tlv	*resp;
	struct vfpf_start_txq_tlv		*req;
	u16					qid = p_cid->rel.queue_id;
	int					rc;

	/* clear mailbox and prep first tlv */
	req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_START_TXQ, sizeof(*req));

	req->tx_qid = qid;

	/* Tx */
	req->pbl_addr	= pbl_addr;
	req->pbl_size	= pbl_size;
	req->hw_sb	= p_cid->sb_igu_id;
	req->sb_index	= p_cid->sb_idx;
	req->tc		= tc;

	qed_vf_pf_add_qid(p_hwfn, p_cid);

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	resp	= &p_iov->pf2vf_reply->queue_start;
	rc	= qed_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		goto exit;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

	/* Modern PFs provide the actual offsets, while legacy
	 * provided only the queue id.
	 */
	if (!p_iov->b_pre_fp_hsi) {
		*pp_doorbell = (u8 __iomem *)p_hwfn->doorbells + resp->offset;
	} else {
		u8 cid = p_iov->acquire_resp.resc.cid[qid];

		*pp_doorbell = (u8 __iomem *)p_hwfn->doorbells +
			DB_ADDR_VF(cid, DQ_DEMS_LEGACY);
	}

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Txq[0x%02x.%02x]: doorbell at %p [offset 0x%08x]\n",
		   qid, p_cid->qid_usage_idx, *pp_doorbell, resp->offset);
exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int qed_vf_pf_txq_stop(struct qed_hwfn		*p_hwfn,
		       struct qed_queue_cid	*p_cid)
{
	struct qed_vf_iov		*p_iov = p_hwfn->vf_iov_info;
	struct vfpf_stop_txqs_tlv	*req;
	struct pfvf_def_resp_tlv	*resp;
	int				rc;

	/* clear mailbox and prep first tlv */
	req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_STOP_TXQS, sizeof(*req));

	req->tx_qid	= p_cid->rel.queue_id;
	req->num_txqs	= 1;

	qed_vf_pf_add_qid(p_hwfn, p_cid);

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	resp	= &p_iov->pf2vf_reply->default_resp;
	rc	= qed_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		goto exit;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}


int qed_vf_pf_vport_start(struct qed_hwfn	*p_hwfn,
			  u8			vport_id,
			  u16			mtu,
			  u8			inner_vlan_removal,
			  enum qed_tpa_mode	tpa_mode,
			  u8			max_buffers_per_cqe,
			  u8			only_untagged,
			  u8			zero_placement_offset)
{
	struct qed_vf_iov		*p_iov = p_hwfn->vf_iov_info;
	struct vfpf_vport_start_tlv	*req;
	struct pfvf_def_resp_tlv	*resp;
	int				rc;
	int				i;

	/* clear mailbox and prep first tlv */
	req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_VPORT_START, sizeof(*req));

	req->mtu			= mtu;
	req->vport_id			= vport_id;
	req->inner_vlan_removal		= inner_vlan_removal;
	req->tpa_mode			= tpa_mode;
	req->max_buffers_per_cqe	= max_buffers_per_cqe;
	req->only_untagged		= only_untagged;
	req->zero_placement_offset	= zero_placement_offset;

	/* status blocks */
	for (i = 0; i < p_hwfn->vf_iov_info->acquire_resp.resc.num_rxqs; i++) {
		struct qed_sb_info *p_sb = p_hwfn->vf_iov_info->sbs_info[i];

		if (p_sb)
			req->sb_addr[i] = p_sb->sb_phys;
	}

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	resp	= &p_iov->pf2vf_reply->default_resp;
	rc	= qed_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		goto exit;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int qed_vf_pf_vport_stop(struct qed_hwfn *p_hwfn)
{
	struct qed_vf_iov		*p_iov	= p_hwfn->vf_iov_info;
	struct pfvf_def_resp_tlv	*resp	=
		&p_iov->pf2vf_reply->default_resp;
	int				rc;

	/* clear mailbox and prep first tlv */
	qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_VPORT_TEARDOWN,
		       sizeof(struct vfpf_first_tlv));

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	rc = qed_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		goto exit;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

static bool
qed_vf_handle_vp_update_is_needed(struct qed_hwfn			*p_hwfn,
				  struct qed_sp_vport_update_params	*p_data,
				  u16					tlv)
{
	switch (tlv) {
	case CHANNEL_TLV_VPORT_UPDATE_ACTIVATE:
		return !!(p_data->update_vport_active_rx_flg ||
			  p_data->update_vport_active_tx_flg);
	case CHANNEL_TLV_VPORT_UPDATE_TX_SWITCH:
#ifndef ASIC_ONLY
		/* FPGA doesn't have PVFC and so can't support tx-switching */
		return !!(p_data->update_tx_switching_flg &&
			  !CHIP_REV_IS_FPGA(p_hwfn->cdev));
#else
		return !!p_data->update_tx_switching_flg;
#endif
	case CHANNEL_TLV_VPORT_UPDATE_VLAN_STRIP:
		return !!p_data->update_inner_vlan_removal_flg;
	case CHANNEL_TLV_VPORT_UPDATE_ACCEPT_ANY_VLAN:
		return !!p_data->update_accept_any_vlan_flg;
	case CHANNEL_TLV_VPORT_UPDATE_MCAST:
		return !!p_data->update_approx_mcast_flg;
	case CHANNEL_TLV_VPORT_UPDATE_ACCEPT_PARAM:
		return !!(p_data->accept_flags.update_rx_mode_config ||
			  p_data->accept_flags.update_tx_mode_config);
	case CHANNEL_TLV_VPORT_UPDATE_RSS:
		return !!p_data->rss_params;
	case CHANNEL_TLV_VPORT_UPDATE_SGE_TPA:
		return !!p_data->sge_tpa_params;
	default:
		DP_INFO(p_hwfn, "Unexpected vport-update TLV[%d] %s\n",
			tlv, qed_channel_tlvs_string[tlv]);
		return false;
	}
}

static void
qed_vf_handle_vp_update_tlvs_resp(struct qed_hwfn			*p_hwfn,
				  struct qed_sp_vport_update_params	*p_data)
{
	struct qed_vf_iov		*p_iov = p_hwfn->vf_iov_info;
	struct pfvf_def_resp_tlv	*p_resp;
	u16				tlv;

	for (tlv = CHANNEL_TLV_VPORT_UPDATE_ACTIVATE;
	     tlv < CHANNEL_TLV_VPORT_UPDATE_MAX;
	     tlv++) {
		if (!qed_vf_handle_vp_update_is_needed(p_hwfn, p_data, tlv))
			continue;

		p_resp = (struct pfvf_def_resp_tlv *)
			qed_iov_search_list_tlvs(p_hwfn, p_iov->pf2vf_reply,
						 tlv);
		if (p_resp && p_resp->hdr.status) {
			DP_VERBOSE(p_hwfn, QED_MSG_IOV,
				   "TLV[%d] type %s Configuration %s\n",
				   tlv, qed_channel_tlvs_string[tlv],
				   (p_resp && p_resp->hdr.status) ? "succeeded"
				   : "failed");
		}
	}
}

int qed_vf_pf_vport_update(struct qed_hwfn			*p_hwfn,
			   struct qed_sp_vport_update_params	*p_params)
{
	struct qed_vf_iov		*p_iov = p_hwfn->vf_iov_info;
	struct vfpf_vport_update_tlv	*req;
	struct pfvf_def_resp_tlv	*resp;
	u8				update_rx, update_tx;
	u32				resp_size = 0;
	u16				size, tlv;
	int				rc;

	resp		= &p_iov->pf2vf_reply->default_resp;
	resp_size	= sizeof(*resp);

	update_rx	= p_params->update_vport_active_rx_flg;
	update_tx	= p_params->update_vport_active_tx_flg;

	/* clear mailbox and prep header tlv */
	qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_VPORT_UPDATE, sizeof(*req));

	/* Prepare extended tlvs */
	if (update_rx || update_tx) {
		struct vfpf_vport_update_activate_tlv *p_act_tlv;

		size		= sizeof(struct vfpf_vport_update_activate_tlv);
		p_act_tlv	= qed_add_tlv(&p_iov->offset,
					      CHANNEL_TLV_VPORT_UPDATE_ACTIVATE,
					      size);
		resp_size += sizeof(struct pfvf_def_resp_tlv);

		if (update_rx) {
			p_act_tlv->update_rx	= update_rx;
			p_act_tlv->active_rx	= p_params->vport_active_rx_flg;
		}

		if (update_tx) {
			p_act_tlv->update_tx	= update_tx;
			p_act_tlv->active_tx	= p_params->vport_active_tx_flg;
		}
	}

#ifndef QED_UPSTREAM
	if (p_params->update_inner_vlan_removal_flg) {
		struct vfpf_vport_update_vlan_strip_tlv *p_vlan_tlv;

		size =
			sizeof(struct vfpf_vport_update_vlan_strip_tlv);
		p_vlan_tlv = qed_add_tlv(
				&p_iov->offset,
				CHANNEL_TLV_VPORT_UPDATE_VLAN_STRIP,
				size);
		resp_size += sizeof(struct pfvf_def_resp_tlv);

		p_vlan_tlv->remove_vlan = p_params->inner_vlan_removal_flg;
	}
#endif

	if (p_params->update_tx_switching_flg) {
		struct vfpf_vport_update_tx_switch_tlv *p_tx_switch_tlv;

		size		= sizeof(struct vfpf_vport_update_tx_switch_tlv);
		tlv		= CHANNEL_TLV_VPORT_UPDATE_TX_SWITCH;
		p_tx_switch_tlv = qed_add_tlv(&p_iov->offset,
					      tlv, size);
		resp_size += sizeof(struct pfvf_def_resp_tlv);

		p_tx_switch_tlv->tx_switching = p_params->tx_switching_flg;
	}

	if (p_params->update_approx_mcast_flg) {
		struct vfpf_vport_update_mcast_bin_tlv *p_mcast_tlv;

		size		= sizeof(struct vfpf_vport_update_mcast_bin_tlv);
		p_mcast_tlv	= qed_add_tlv(&p_iov->offset,
					      CHANNEL_TLV_VPORT_UPDATE_MCAST,
					      size);
		resp_size += sizeof(struct pfvf_def_resp_tlv);

		memcpy(p_mcast_tlv->bins, p_params->bins,
		       sizeof(u32) * ETH_MULTICAST_MAC_BINS_IN_REGS);
	}

	update_rx	= p_params->accept_flags.update_rx_mode_config;
	update_tx	= p_params->accept_flags.update_tx_mode_config;

	if (update_rx || update_tx) {
		struct vfpf_vport_update_accept_param_tlv *p_accept_tlv;

		tlv	= CHANNEL_TLV_VPORT_UPDATE_ACCEPT_PARAM;
		size	=
			sizeof(struct vfpf_vport_update_accept_param_tlv);
		p_accept_tlv	= qed_add_tlv(&p_iov->offset, tlv, size);
		resp_size	+= sizeof(struct pfvf_def_resp_tlv);

		if (update_rx) {
			p_accept_tlv->update_rx_mode	= update_rx;
			p_accept_tlv->rx_accept_filter	=
				p_params->accept_flags.rx_accept_filter;
		}

		if (update_tx) {
			p_accept_tlv->update_tx_mode	= update_tx;
			p_accept_tlv->tx_accept_filter	=
				p_params->accept_flags.tx_accept_filter;
		}
	}

	if (p_params->rss_params) {
		struct qed_rss_params			*rss_params =
			p_params->rss_params;
		struct vfpf_vport_update_rss_tlv	*p_rss_tlv;
		int					i, table_size;

		size		= sizeof(struct vfpf_vport_update_rss_tlv);
		p_rss_tlv	= qed_add_tlv(&p_iov->offset,
					      CHANNEL_TLV_VPORT_UPDATE_RSS,
					      size);
		resp_size += sizeof(struct pfvf_def_resp_tlv);

		if (rss_params->update_rss_config)
			p_rss_tlv->update_rss_flags |=
				VFPF_UPDATE_RSS_CONFIG_FLAG;
		if (rss_params->update_rss_capabilities)
			p_rss_tlv->update_rss_flags |=
				VFPF_UPDATE_RSS_CAPS_FLAG;
		if (rss_params->update_rss_ind_table)
			p_rss_tlv->update_rss_flags |=
				VFPF_UPDATE_RSS_IND_TABLE_FLAG;
		if (rss_params->update_rss_key)
			p_rss_tlv->update_rss_flags |=
				VFPF_UPDATE_RSS_KEY_FLAG;

		p_rss_tlv->rss_enable		= rss_params->rss_enable;
		p_rss_tlv->rss_caps		= rss_params->rss_caps;
		p_rss_tlv->rss_table_size_log	=
			rss_params->rss_table_size_log;

		table_size = min_t(int, T_ETH_INDIRECTION_TABLE_SIZE,
				   1 << p_rss_tlv->rss_table_size_log);
		for (i = 0; i < table_size; i++) {
			struct qed_queue_cid *p_queue;

			p_queue =
				rss_params->rss_ind_table[i];
			p_rss_tlv->rss_ind_table[i] = p_queue->rel.queue_id;
		}

		memcpy(p_rss_tlv->rss_key, rss_params->rss_key,
		       sizeof(rss_params->rss_key));
	}

	if (p_params->update_accept_any_vlan_flg) {
		struct vfpf_vport_update_accept_any_vlan_tlv *p_any_vlan_tlv;

		size =
			sizeof(struct vfpf_vport_update_accept_any_vlan_tlv);
		tlv		= CHANNEL_TLV_VPORT_UPDATE_ACCEPT_ANY_VLAN;
		p_any_vlan_tlv	= qed_add_tlv(&p_iov->offset, tlv, size);

		resp_size +=
			sizeof(struct pfvf_def_resp_tlv);
		p_any_vlan_tlv->accept_any_vlan =
			p_params->accept_any_vlan;
		p_any_vlan_tlv->update_accept_any_vlan_flg =
			p_params->update_accept_any_vlan_flg;
	}


	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	rc = qed_send_msg2pf(p_hwfn, &resp->hdr.status, resp_size);
	if (rc)
		goto exit;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

	qed_vf_handle_vp_update_tlvs_resp(p_hwfn, p_params);

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int qed_vf_pf_reset(struct qed_hwfn *p_hwfn)
{
	struct qed_vf_iov		*p_iov = p_hwfn->vf_iov_info;
	struct pfvf_def_resp_tlv	*resp;
	struct vfpf_first_tlv		*req;
	int				rc;

	/* clear mailbox and prep first tlv */
	req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_CLOSE, sizeof(*req));

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	resp	= &p_iov->pf2vf_reply->default_resp;
	rc	= qed_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		goto exit;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EAGAIN;
		goto exit;
	}

	p_hwfn->b_int_enabled = 0;

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

void qed_vf_pf_filter_mcast(struct qed_hwfn		*p_hwfn,
			    struct qed_filter_mcast	*p_filter_cmd)
{
	struct qed_sp_vport_update_params	sp_params;
	int					i;

	memset(&sp_params, 0, sizeof(sp_params));
	sp_params.update_approx_mcast_flg = 1;

	if (p_filter_cmd->opcode == QED_FILTER_ADD) {
		for (i = 0; i < p_filter_cmd->num_mc_addrs; i++) {
			u32 bit;

			bit =
				qed_mcast_bin_from_mac(p_filter_cmd->mac[i]);
			sp_params.bins[bit / 32] |= 1 << (bit % 32);
		}
	}

	qed_vf_pf_vport_update(p_hwfn, &sp_params);
}

int qed_vf_pf_filter_ucast(struct qed_hwfn		*p_hwfn,
			   struct qed_filter_ucast	*p_ucast)
{
	struct qed_vf_iov		*p_iov = p_hwfn->vf_iov_info;
	struct vfpf_ucast_filter_tlv	*req;
	struct pfvf_def_resp_tlv	*resp;
	int				rc;


	/* clear mailbox and prep first tlv */
	req		= qed_vf_pf_prep(p_hwfn,
					 CHANNEL_TLV_UCAST_FILTER,
					 sizeof(*req));
	req->opcode	= (u8)p_ucast->opcode;
	req->type	= (u8)p_ucast->type;
	memcpy(req->mac, p_ucast->mac, ETH_ALEN);
	req->vlan = p_ucast->vlan;

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	resp	= &p_iov->pf2vf_reply->default_resp;
	rc	= qed_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		goto exit;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EAGAIN;
		goto exit;
	}

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int qed_vf_pf_int_cleanup(struct qed_hwfn *p_hwfn)
{
	struct qed_vf_iov		*p_iov	= p_hwfn->vf_iov_info;
	struct pfvf_def_resp_tlv	*resp	=
		&p_iov->pf2vf_reply->default_resp;
	int				rc;

	/* clear mailbox and prep first tlv */
	qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_INT_CLEANUP,
		       sizeof(struct vfpf_first_tlv));

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	rc = qed_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		goto exit;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int qed_vf_pf_get_coalesce(struct qed_hwfn	*p_hwfn,
			   u16			*p_coal,
			   struct qed_queue_cid *p_cid)
{
	struct qed_vf_iov		*p_iov = p_hwfn->vf_iov_info;
	struct pfvf_read_coal_resp_tlv	*resp;
	struct vfpf_read_coal_req_tlv	*req;
	int				rc;

	/* clear mailbox and prep header tlv */
	req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_COALESCE_READ,
			     sizeof(*req));
	req->qid	= p_cid->rel.queue_id;
	req->is_rx	= p_cid->b_is_rx ? 1 : 0;

	qed_add_tlv(&p_iov->offset, CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));
	resp = &p_iov->pf2vf_reply->read_coal_resp;

	rc = qed_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		goto exit;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS)
		goto exit;

	*p_coal = resp->coal;
exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_bulletin_update_mac(struct qed_hwfn	*p_hwfn,
			      const u8		*p_mac)
{
	struct qed_vf_iov			*p_iov = p_hwfn->vf_iov_info;
	struct vfpf_bulletin_update_mac_tlv	*p_req;
	struct pfvf_def_resp_tlv		*p_resp;
	int					rc;

	if (!p_mac)
		return -EINVAL;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_BULLETIN_UPDATE_MAC,
			       sizeof(*p_req));
	memcpy(p_req->mac, p_mac, ETH_ALEN);
	DP_VERBOSE(
		p_hwfn,
		QED_MSG_IOV,
		"Requesting bulletin update for MAC[%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx]\n",
		p_mac[0],
		p_mac[1],
		p_mac[2],
		p_mac[3],
		p_mac[4],
		p_mac[5]);

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp	= &p_iov->pf2vf_reply->default_resp;
	rc	= qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_set_coalesce(struct qed_hwfn		*p_hwfn,
		       u16			rx_coal,
		       u16			tx_coal,
		       struct qed_queue_cid	*p_cid)
{
	struct qed_vf_iov		*p_iov = p_hwfn->vf_iov_info;
	struct vfpf_update_coalesce	*req;
	struct pfvf_def_resp_tlv	*resp;
	int				rc;

	/* clear mailbox and prep header tlv */
	req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_COALESCE_UPDATE,
			     sizeof(*req));

	req->rx_coal	= rx_coal;
	req->tx_coal	= tx_coal;
	req->qid	= p_cid->rel.queue_id;

	DP_VERBOSE(
		p_hwfn,
		QED_MSG_IOV,
		"Setting coalesce rx_coal = %d, tx_coal = %d at queue = %d\n",
		rx_coal,
		tx_coal,
		req->qid);

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset, CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	resp	= &p_iov->pf2vf_reply->default_resp;
	rc	= qed_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));

	if (rc)
		goto exit;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS)
		goto exit;

	p_hwfn->cdev->rx_coalesce_usecs = rx_coal;
	p_hwfn->cdev->tx_coalesce_usecs = tx_coal;

exit:
	qed_vf_pf_req_end(p_hwfn, rc);
	return rc;
}

int
qed_vf_pf_update_mtu(struct qed_hwfn	*p_hwfn,
		     u16		mtu)
{
	struct qed_vf_iov		*p_iov = p_hwfn->vf_iov_info;
	struct vfpf_update_mtu_tlv	*p_req;
	struct pfvf_def_resp_tlv	*p_resp;
	int				rc;

	if (!mtu)
		return -EINVAL;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_UPDATE_MTU,
			       sizeof(*p_req));
	p_req->mtu = mtu;
	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting MTU update to %d\n", mtu);

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp	= &p_iov->pf2vf_reply->default_resp;
	rc	= qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (p_resp->hdr.status == PFVF_STATUS_NOT_SUPPORTED)
		rc = -EINVAL;

	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int qed_vf_pf_establish_ll2_conn(struct qed_hwfn	*p_hwfn,
				 u8			connection_handle)
{
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	struct vfpf_est_ll2_conn_tlv		*p_req	= NULL;
	struct pfvf_est_ll2_conn_resp_tlv	*p_resp;
	struct qed_ll2_info			*p_ll2_conn;
	struct qed_ll2_rx_queue			*p_rx;
	struct qed_ll2_tx_queue			*p_tx;
	int					rc;

	p_ll2_conn = qed_ll2_handle_sanity_lock(p_hwfn, connection_handle);
	if (!p_ll2_conn)
		return -EINVAL;

	p_rx	= &p_ll2_conn->rx_queue;
	p_tx	= &p_ll2_conn->tx_queue;

	if (!p_rx || !p_tx)
		return -EINVAL;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_ESTABLISH_LL2_CONN,
			       sizeof(*p_req));

	p_req->rel_qid			= connection_handle;
	p_req->rx_bd_base_addr		= p_rx->rxq_chain.p_phys_addr;
	p_req->rx_cqe_pbl_addr		= qed_chain_get_pbl_phys(
			&p_rx->rcq_chain);
	p_req->drop_ttl0_flg		= p_ll2_conn->input.rx_drop_ttl0_flg;
	p_req->err_no_buf		= p_ll2_conn->input.ai_err_no_buf;
	p_req->err_packet_too_big	=
		p_ll2_conn->input.ai_err_packet_too_big;
	p_req->gsi_enable		= p_ll2_conn->input.gsi_enable;
	p_req->inner_vlan_stripping_en	= p_ll2_conn->input.rx_vlan_removal_en;
	p_req->mtu			= p_ll2_conn->input.mtu;
	p_req->rx_cq_num_pbl_pages	=
		(u16)qed_chain_get_page_cnt(&p_rx->rcq_chain);
	p_req->tx_pbl_addr	= qed_chain_get_pbl_phys(&p_tx->txq_chain);
	p_req->tx_num_pbl_pages =
		(u16)qed_chain_get_page_cnt(&p_tx->txq_chain);
	p_req->conn_type = p_ll2_conn->input.conn_type;
	/* p_req->tx_stats_en = p_ll2_conn->tx_stats_en; */
	p_req->rx_cb_registered = QED_LL2_RX_REGISTERED(p_ll2_conn);
	p_req->tx_cb_registered = QED_LL2_TX_REGISTERED(p_ll2_conn);

	/* VF LL2 will use the first RDMA SB (that is used for CNQ) */
	p_req->sb_id		= qed_vf_rdma_cnq_sb_start_id(p_hwfn);
	p_req->txq_sb_pi	= LL2_VF_TX_PI;
	p_req->rxq_sb_pi	= LL2_VF_RX_PI;

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting to establish ll2 conn %d\n", connection_handle);

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp	= &p_iov->pf2vf_reply->establish_ll2_resp;
	rc	= qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));

	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

	/* Setting LL2 parameters according to PF resp */

	/* rx queue params */
	p_rx->ctx_based		= 1;    /* vf uses only ctx based ll2 queues */
	p_rx->set_prod_addr	= (u8 __iomem *)p_hwfn->doorbells +
	                                /* using dpi 0 */
		p_hwfn->dpi_info.dpi_start_offset +
		p_resp->rxq_db_offset;

	memcpy(&p_rx->db_data,
	       (struct core_pwm_prod_update_data *)&p_resp->rxq_db_msg,
	       sizeof(p_rx->db_data));

	rc = qed_db_recovery_add(p_hwfn->cdev,
				 p_rx->set_prod_addr,
				 &p_rx->db_data, DB_REC_WIDTH_64B,
				 DB_REC_KERNEL);

	if (rc)
		goto exit;

	/* tx queue params */
	p_tx->doorbell_addr = (u8 __iomem *)p_hwfn->doorbells +
		p_resp->txq_db_offset;

	memcpy(&p_tx->db_msg, (struct core_db_data *)&p_resp->txq_db_msg,
	       sizeof(p_tx->db_msg));

	rc = qed_db_recovery_add(p_hwfn->cdev, p_tx->doorbell_addr,
				 &p_tx->db_msg, DB_REC_WIDTH_32B,
				 DB_REC_KERNEL);

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_terminate_ll2_conn(struct qed_hwfn	*p_hwfn,
			     u8			conn_handle)
{
	struct vfpf_terminate_ll2_conn_tlv	*p_req	= NULL;
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	struct qed_ll2_info			*p_ll2_conn;
	struct pfvf_def_resp_tlv		*p_resp;
	int					rc;

	p_ll2_conn = qed_ll2_handle_sanity_lock(p_hwfn, conn_handle);

	if (!p_ll2_conn)
		return -EINVAL;

	/* Clearing db_recovery for both tx and rx queues */
	qed_db_recovery_del(p_hwfn->cdev, p_ll2_conn->tx_queue.doorbell_addr,
			    &p_ll2_conn->tx_queue.db_msg);

	qed_db_recovery_del(p_hwfn->cdev,
			    p_ll2_conn->rx_queue.set_prod_addr,
			    &p_ll2_conn->rx_queue.db_data);

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_TERMINATE_LL2_CONN,
			       sizeof(*p_req));

	p_req->conn_handle	= conn_handle;
	p_req->rx_cb_registered = QED_LL2_RX_REGISTERED(p_ll2_conn);
	p_req->tx_cb_registered = QED_LL2_TX_REGISTERED(p_ll2_conn);

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting to terminate ll2 conn, handle %d\n",
		   conn_handle);

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp	= &p_iov->pf2vf_reply->default_resp;
	rc	= qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));

	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

static void
qed_vf_fill_rdma_sb_addr(struct qed_hwfn	*p_hwfn,
			 u64			*sb_addr)
{
	u8 num_cnqs, cnq_sb_start_id, sb_id;

	qed_vf_get_num_cnqs(p_hwfn, &num_cnqs);
	cnq_sb_start_id = qed_vf_rdma_cnq_sb_start_id(p_hwfn);

	/* rdma status blocks */
	for (sb_id = 0; sb_id < num_cnqs; sb_id++) {
		struct qed_sb_info *p_sb =
			p_hwfn->vf_iov_info->sbs_info[cnq_sb_start_id + sb_id];

		if (p_sb)
			sb_addr[sb_id] = p_sb->sb_phys;
	}
}

static void
qed_vf_pf_populate_tlv_req(struct qed_hwfn	*p_hwfn,
			   u16			type,
			   void			*req,
			   void			*in_params)
{
	switch (type) {
	case CHANNEL_TLV_RDMA_START:
	{
		struct qed_rdma_start_in_params *params =
			(struct qed_rdma_start_in_params *)in_params;
		struct vfpf_rdma_start_tlv	*p_req =
			(struct vfpf_rdma_start_tlv *)req;

		p_req->desired_cnq	= params->desired_cnq;
		p_req->max_mtu		= params->max_mtu;

		memcpy(p_req->mac_addr,
		       params->mac_addr,
		       sizeof(p_req->mac_addr));

		p_req->cq_mode = params->roce.cq_mode;

		p_req->dcqcn_params.notification_point =
			params->roce.dcqcn_params.notification_point;
		p_req->dcqcn_params.reaction_point =
			params->roce.dcqcn_params.reaction_point;
		p_req->dcqcn_params.cnp_send_timeout =
			params->roce.dcqcn_params.cnp_send_timeout;
		p_req->dcqcn_params.cnp_dscp =
			params->roce.dcqcn_params.cnp_dscp;
		p_req->dcqcn_params.cnp_vlan_priority =
			params->roce.dcqcn_params.cnp_vlan_priority;
		p_req->dcqcn_params.rl_bc_rate =
			params->roce.dcqcn_params.rl_bc_rate;
		p_req->dcqcn_params.rl_max_rate =
			params->roce.dcqcn_params.rl_max_rate;
		p_req->dcqcn_params.rl_r_ai =
			params->roce.dcqcn_params.rl_r_ai;
		p_req->dcqcn_params.rl_r_hai =
			params->roce.dcqcn_params.rl_r_hai;
		p_req->dcqcn_params.dcqcn_gd =
			params->roce.dcqcn_params.dcqcn_gd;
		p_req->dcqcn_params.dcqcn_k_us =
			params->roce.dcqcn_params.dcqcn_k_us;
		p_req->dcqcn_params.dcqcn_timeout_us =
			params->roce.dcqcn_params.dcqcn_timeout_us;

		p_req->ll2_handle = params->roce.ll2_handle;

		qed_vf_fill_rdma_sb_addr(p_hwfn, p_req->sb_addr);

		break;
	}
	case CHANNEL_TLV_RDMA_REGISTER_TID:
	{
		struct qed_rdma_register_tid_in_params	*params =
			(struct qed_rdma_register_tid_in_params *)in_params;
		struct vfpf_rdma_register_tid_tlv	*p_req =
			(struct vfpf_rdma_register_tid_tlv *)req;

		p_req->itid			= params->itid;
		p_req->tid_type			= params->tid_type;
		p_req->key			= params->key;
		p_req->pd			= params->pd;
		p_req->local_read		= params->local_read;
		p_req->local_write		= params->local_write;
		p_req->remote_read		= params->remote_read;
		p_req->remote_write		= params->remote_write;
		p_req->remote_atomic		= params->remote_atomic;
		p_req->mw_bind			= params->mw_bind;
		p_req->pbl_ptr			= params->pbl_ptr;
		p_req->pbl_two_level		= params->pbl_two_level;
		p_req->pbl_page_size_log	= params->pbl_page_size_log;
		p_req->page_size_log		= params->page_size_log;
		p_req->fbo			= params->fbo;
		p_req->length			= params->length;
		p_req->vaddr			= params->vaddr;
		p_req->zbva			= params->zbva;
		p_req->phy_mr			= params->phy_mr;
		p_req->dma_mr			= params->dma_mr;
		p_req->dif_enabled		= params->dif_enabled;
		p_req->dif_error_addr		= params->dif_error_addr;

		break;
	}
	case CHANNEL_TLV_RDMA_CREATE_CQ:
	{
		struct qed_rdma_create_cq_in_params	*params =
			(struct qed_rdma_create_cq_in_params *)in_params;
		struct vfpf_rdma_create_cq_tlv		*p_req =
			(struct vfpf_rdma_create_cq_tlv *)req;

		p_req->cq_handle_lo		= params->cq_handle_lo;
		p_req->cq_handle_hi		= params->cq_handle_hi;
		p_req->cq_size			= params->cq_size;
		p_req->dpi			= params->dpi;
		p_req->pbl_two_level		= params->pbl_two_level;
		p_req->pbl_ptr			= params->pbl_ptr;
		p_req->pbl_num_pages		= params->pbl_num_pages;
		p_req->pbl_page_size_log	= params->pbl_page_size_log;
		p_req->cnq_id			= params->cnq_id;
		p_req->int_timeout		= params->int_timeout;

		break;
	}
	case CHANNEL_TLV_RDMA_RESIZE_CQ:
	{
		struct qed_rdma_resize_cq_in_params	*params =
			(struct qed_rdma_resize_cq_in_params *)in_params;
		struct vfpf_rdma_resize_cq_tlv		*p_req =
			(struct vfpf_rdma_resize_cq_tlv *)req;

		p_req->icid			= params->icid;
		p_req->cq_size			= params->cq_size;
		p_req->pbl_two_level		= params->pbl_two_level;
		p_req->pbl_ptr			= params->pbl_ptr;
		p_req->pbl_num_pages		= params->pbl_num_pages;
		p_req->pbl_page_size_log	= params->pbl_page_size_log;

		break;
	}
	case CHANNEL_TLV_RDMA_CREATE_QP:
	{
		struct qed_rdma_create_qp_in_params	*params =
			(struct qed_rdma_create_qp_in_params *)in_params;
		struct vfpf_rdma_create_qp_tlv		*p_req =
			(struct vfpf_rdma_create_qp_tlv *)req;

		p_req->qp_handle_lo		= params->qp_handle_lo;
		p_req->qp_handle_hi		= params->qp_handle_hi;
		p_req->qp_handle_async_lo	= params->qp_handle_async_lo;
		p_req->qp_handle_async_hi	= params->qp_handle_async_hi;
		p_req->use_srq			= params->use_srq;
		p_req->signal_all		= params->signal_all;
		p_req->fmr_and_reserved_lkey	= params->fmr_and_reserved_lkey;
		p_req->pd			= params->pd;
		p_req->dpi			= params->dpi;
		p_req->sq_cq_id			= params->sq_cq_id;
		p_req->sq_num_pages		= params->sq_num_pages;
		p_req->sq_pbl_ptr		= params->sq_pbl_ptr;
		p_req->max_sq_sges		= params->max_sq_sges;
		p_req->rq_cq_id			= params->rq_cq_id;
		p_req->rq_num_pages		= params->rq_num_pages;
		p_req->rq_pbl_ptr		= params->rq_pbl_ptr;
		p_req->srq_id			= params->srq_id;
		p_req->stats_queue		= params->stats_queue;
		p_req->qp_type			= params->qp_type;
		p_req->xrcd_id			= params->xrcd_id;
		p_req->create_flags		= params->create_flags;

		break;
	}
	case CHANNEL_TLV_RDMA_MODIFY_QP:
	{
		struct qed_rdma_modify_qp_in_params	*params =
			(struct qed_rdma_modify_qp_in_params *)in_params;
		struct vfpf_rdma_modify_qp_tlv		*p_req =
			(struct vfpf_rdma_modify_qp_tlv *)req;

		p_req->modify_flags		= params->modify_flags;
		p_req->new_state		= params->new_state;
		p_req->pkey			= params->pkey;
		p_req->incoming_rdma_read_en	= params->incoming_rdma_read_en;
		p_req->incoming_rdma_write_en	=
			params->incoming_rdma_write_en;
		p_req->incoming_atomic_en	= params->incoming_atomic_en;
		p_req->e2e_flow_control_en	= params->e2e_flow_control_en;
		p_req->dest_qp			= params->dest_qp;
		p_req->mtu			= params->mtu;
		p_req->traffic_class_tos	= params->traffic_class_tos;
		p_req->hop_limit_ttl		= params->hop_limit_ttl;
		p_req->flow_label		= params->flow_label;
		memcpy(p_req->sgid.bytes, params->sgid.bytes,
		       sizeof(p_req->sgid));
		memcpy(p_req->dgid.bytes, params->dgid.bytes,
		       sizeof(p_req->dgid));
		p_req->udp_src_port		= params->udp_src_port;
		p_req->vlan_id			= params->vlan_id;
		p_req->rq_psn			= params->rq_psn;
		p_req->sq_psn			= params->sq_psn;
		p_req->max_rd_atomic_resp	= params->max_rd_atomic_resp;
		p_req->max_rd_atomic_req	= params->max_rd_atomic_req;
		p_req->ack_timeout		= params->ack_timeout;
		p_req->retry_cnt		= params->retry_cnt;
		p_req->rnr_retry_cnt		= params->rnr_retry_cnt;
		p_req->min_rnr_nak_timer	= params->min_rnr_nak_timer;
		p_req->sqd_async		= params->sqd_async;

		memcpy(p_req->remote_mac_addr,
		       params->remote_mac_addr,
		       sizeof(p_req->remote_mac_addr));

		memcpy(p_req->local_mac_addr,
		       params->local_mac_addr,
		       sizeof(p_req->local_mac_addr));

		p_req->use_local_mac	= params->use_local_mac;
		p_req->roce_mode	= params->roce_mode;

		break;
	}
	case CHANNEL_TLV_RDMA_CREATE_SRQ:
	{
		struct qed_rdma_create_srq_in_params	*params =
			(struct qed_rdma_create_srq_in_params *)in_params;
		struct vfpf_rdma_create_srq_tlv		*p_req =
			(struct vfpf_rdma_create_srq_tlv *)req;
		p_req->pbl_base_addr	= params->pbl_base_addr;
		p_req->prod_pair_addr	= params->prod_pair_addr;
		p_req->num_pages	= params->num_pages;
		p_req->pd_id		= params->pd_id;
		p_req->page_size	= params->page_size;
		if (params->is_xrc) {
			p_req->is_xrc		= params->is_xrc;
			p_req->xrcd_id		= params->xrcd_id;
			p_req->cq_cid		= params->cq_cid;
			p_req->reserved_key_en	= params->reserved_key_en;
		}

		break;
	}
	case CHANNEL_TLV_RDMA_MODIFY_SRQ:
	{
		struct qed_rdma_modify_srq_in_params	*params =
			(struct qed_rdma_modify_srq_in_params *)in_params;
		struct vfpf_rdma_modify_srq_tlv		*p_req =
			(struct vfpf_rdma_modify_srq_tlv *)req;
		p_req->wqe_limit	= params->wqe_limit;
		p_req->srq_id		= params->srq_id;
		p_req->is_xrc		= params->is_xrc;

		break;
	}
	case CHANNEL_TLV_RDMA_DESTROY_SRQ:
	{
		struct qed_rdma_destroy_srq_in_params	*params =
			(struct qed_rdma_destroy_srq_in_params *)in_params;
		struct vfpf_rdma_destroy_srq_tlv	*p_req =
			(struct vfpf_rdma_destroy_srq_tlv *)req;
		p_req->srq_id	= params->srq_id;
		p_req->is_xrc	= params->is_xrc;

		break;
	}
#ifdef CONFIG_IWARP
	case CHANNEL_TLV_RDMA_IWARP_CONNECT:
	case CHANNEL_TLV_RDMA_IWARP_ACCEPT:
	case CHANNEL_TLV_RDMA_IWARP_CREATE_LISTEN:
	case CHANNEL_TLV_RDMA_IWARP_DESTROY_LISTEN:
	case CHANNEL_TLV_RDMA_IWARP_PAUSE_LISTEN:
	case CHANNEL_TLV_RDMA_IWARP_REJECT:
	case CHANNEL_TLV_RDMA_IWARP_SEND_RTR:
#endif
	default:
		break;
	}

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
		   "Type %u: in_params ==> TLV request\n", type);
}

static void
qed_vf_pf_populate_tlv_resp(struct qed_hwfn	*p_hwfn,
			    u16			type,
			    void		*out_params,
			    void		*in_resp)
{
	switch (type) {
	case CHANNEL_TLV_RDMA_QUERY_COUNTERS:
	{
		struct pfvf_rdma_query_counters_resp_tlv	*p_resp =
			(struct pfvf_rdma_query_counters_resp_tlv *)in_resp;
		struct qed_rdma_counters_out_params		*params =
			(struct qed_rdma_counters_out_params *)out_params;

		params->pd_count	= p_resp->pd_count;
		params->max_pd		= p_resp->max_pd;
		params->dpi_count	= p_resp->dpi_count;
		params->max_dpi		= p_resp->max_dpi;
		params->cq_count	= p_resp->cq_count;
		params->max_cq		= p_resp->max_cq;
		params->qp_count	= p_resp->qp_count;
		params->max_qp		= p_resp->max_qp;
		params->tid_count	= p_resp->tid_count;
		params->max_tid		= p_resp->max_tid;
		params->srq_count	= p_resp->srq_count;
		params->max_srq		= p_resp->max_srq;
		params->xrc_srq_count	= p_resp->xrc_srq_count;
		params->max_xrc_srq	= p_resp->max_xrc_srq;
		params->xrcd_count	= p_resp->xrcd_count;
		params->max_xrcd	= p_resp->max_xrcd;

		break;
	}
	case CHANNEL_TLV_RDMA_RESIZE_CQ:
	{
		struct qed_rdma_resize_cq_out_params	*params =
			(struct qed_rdma_resize_cq_out_params *)out_params;
		struct pfvf_rdma_resize_cq_resp_tlv	*p_resp =
			(struct pfvf_rdma_resize_cq_resp_tlv *)in_resp;

		params->prod	= p_resp->prod;
		params->cons	= p_resp->cons;

		break;
	}
	case CHANNEL_TLV_RDMA_CREATE_QP:
	{
		struct qed_rdma_create_qp_out_params	*params =
			(struct qed_rdma_create_qp_out_params *)out_params;
		struct pfvf_rdma_create_qp_resp_tlv	*p_resp =
			(struct pfvf_rdma_create_qp_resp_tlv *)in_resp;

		params->qp_id	= p_resp->qp_id;
		params->icid	= p_resp->icid;

		break;
	}
	case CHANNEL_TLV_RDMA_QUERY_QP:
	{
		struct qed_rdma_query_qp_out_params	*params =
			(struct qed_rdma_query_qp_out_params *)out_params;
		struct pfvf_rdma_query_qp_resp_tlv	*p_resp =
			(struct pfvf_rdma_query_qp_resp_tlv *)in_resp;

		params->state			= p_resp->state;
		params->rq_psn			= p_resp->rq_psn;
		params->sq_psn			= p_resp->sq_psn;
		params->draining		= p_resp->draining;
		params->mtu			= p_resp->mtu;
		params->dest_qp			= p_resp->dest_qp;
		params->incoming_rdma_read_en	= p_resp->incoming_rdma_read_en;
		params->incoming_rdma_write_en	=
			p_resp->incoming_rdma_write_en;
		params->incoming_atomic_en	= p_resp->incoming_atomic_en;
		params->e2e_flow_control_en	= p_resp->e2e_flow_control_en;
		memcpy(params->sgid.bytes, p_resp->sgid.bytes,
		       sizeof(p_resp->sgid));
		memcpy(params->dgid.bytes, p_resp->dgid.bytes,
		       sizeof(p_resp->dgid));
		params->flow_label		= p_resp->flow_label;
		params->hop_limit_ttl		= p_resp->hop_limit_ttl;
		params->traffic_class_tos	= p_resp->traffic_class_tos;
		params->timeout			= p_resp->timeout;
		params->rnr_retry		= p_resp->rnr_retry;
		params->retry_cnt		= p_resp->retry_cnt;
		params->min_rnr_nak_timer	= p_resp->min_rnr_nak_timer;
		params->pkey_index		= p_resp->pkey_index;
		params->max_rd_atomic		= p_resp->max_rd_atomic;
		params->max_dest_rd_atomic	= p_resp->max_dest_rd_atomic;
		params->sqd_async		= p_resp->sqd_async;

		break;
	}
	case CHANNEL_TLV_RDMA_DESTROY_QP:
	{
		struct qed_rdma_destroy_qp_out_params	*params =
			(struct qed_rdma_destroy_qp_out_params *)out_params;
		struct pfvf_rdma_destroy_qp_resp_tlv	*p_resp =
			(struct pfvf_rdma_destroy_qp_resp_tlv *)in_resp;

		params->sq_cq_prod	= p_resp->sq_cq_prod;
		params->rq_cq_prod	= p_resp->rq_cq_prod;

		break;
	}
	case CHANNEL_TLV_RDMA_QUERY_PORT:
	{
		struct  pfvf_rdma_query_port_resp_tlv	*p_resp =
			(struct pfvf_rdma_query_port_resp_tlv *)in_resp;
		struct qed_rdma_port			*params =
			(struct qed_rdma_port *)out_params;

		params->port_state	= p_resp->port_state;
		params->link_speed	= p_resp->link_speed;
		params->max_msg_size	= p_resp->max_msg_size;

		break;
	}
	case CHANNEL_TLV_RDMA_QUERY_DEVICE:
	{
		struct  pfvf_rdma_query_device_resp_tlv *p_resp =
			(struct pfvf_rdma_query_device_resp_tlv *)in_resp;
		struct qed_rdma_device			*vf_device =
			(struct qed_rdma_device *)out_params;

		vf_device->vendor_id		= p_resp->vendor_id;
		vf_device->vendor_part_id	=
			p_resp->vendor_part_id;
		vf_device->hw_ver			= p_resp->hw_ver;
		vf_device->fw_ver			= p_resp->fw_ver;
		vf_device->max_cnq			= p_resp->max_cnq;
		vf_device->max_sge			= p_resp->max_sge;
		vf_device->max_srq_sge			= p_resp->max_srq_sge;
		vf_device->max_inline			= p_resp->max_inline;
		vf_device->max_wqe			= p_resp->max_wqe;
		vf_device->max_srq_wqe			= p_resp->max_srq_wqe;
		vf_device->max_qp_resp_rd_atomic_resc	=
			p_resp->max_qp_resp_rd_atomic_resc;
		vf_device->max_qp_req_rd_atomic_resc =
			p_resp->max_qp_req_rd_atomic_resc;
		vf_device->max_dev_resp_rd_atomic_resc =
			p_resp->max_dev_resp_rd_atomic_resc;
		vf_device->max_cq		= p_resp->max_cq;
		vf_device->max_qp		= p_resp->max_qp;
		vf_device->max_srq		= p_resp->max_srq;
		vf_device->max_mr		= p_resp->max_mr;
		vf_device->max_mr_size		= p_resp->max_mr_size;
		vf_device->max_cqe		= p_resp->max_cqe;
		vf_device->max_mw		= p_resp->max_mw;
		vf_device->max_fmr		= p_resp->max_fmr;
		vf_device->max_mr_mw_fmr_pbl	= p_resp->max_mr_mw_fmr_pbl;
		vf_device->max_mr_mw_fmr_size	= p_resp->max_mr_mw_fmr_size;
		vf_device->max_pd		= p_resp->max_pd;
		vf_device->max_ah		= p_resp->max_ah;
		vf_device->max_pkey		= p_resp->max_pkey;
		vf_device->max_srq_wr		= p_resp->max_srq_wr;
		vf_device->srq_limit		= p_resp->srq_limit;
		vf_device->max_stats_queues	= p_resp->max_stats_queues;
		vf_device->page_size_caps	= p_resp->page_size_caps;
		vf_device->dev_ack_delay	= p_resp->dev_ack_delay;
		vf_device->reserved_lkey	= p_resp->reserved_lkey;
		vf_device->bad_pkey_counter	= p_resp->bad_pkey_counter;

		break;
	}
	default:
		break;
	}

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
		   "Type %u: TLV response ==> out_params\n", type);
}

static int
qed_vf_rdma_alloc(struct qed_hwfn	*p_hwfn,
		  struct qed_rdma_info	*p_rdma_info)
{
	int rc;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Allocating RDMA\n");

	if (p_hwfn->hw_info.personality == QED_PCI_ETH_IWARP)
		p_rdma_info->proto = PROTOCOLID_IWARP;
	else
		p_rdma_info->proto = PROTOCOLID_ROCE;

	/* Allocate a struct for device params */
	p_rdma_info->dev = kzalloc(sizeof(*p_rdma_info->dev), GFP_KERNEL);

	if (!p_rdma_info->dev) {
		DP_NOTICE(
			p_hwfn,
			"qed rdma alloc failed: cannot allocate memory (rdma info dev)\n");
		return -ENOMEM;
	}

	/* Allocate a struct with port params and fill it */
	p_rdma_info->port = kzalloc(sizeof(*p_rdma_info->port), GFP_KERNEL);
	if (!p_rdma_info->port) {
		DP_NOTICE(
			p_hwfn,
			"qed rdma alloc failed: cannot allocate memory (rdma info port)\n");
		return -ENOMEM;
	}

	/* Allocate pd bitmap */
	rc = qed_rdma_bmap_alloc(p_hwfn, &p_rdma_info->pd_map, RDMA_MAX_PDS,
				 "PD");
	if (rc) {
		DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
			   "Failed to allocate pd_map,rc = %d\n", rc);
		return rc;
	}

	/* Allocate DPI bitmap */
	rc = qed_rdma_bmap_alloc(p_hwfn, &p_rdma_info->dpi_map,
				 p_hwfn->dpi_info.dpi_count, "DPI");
	if (rc) {
		DP_ERR(p_hwfn, "Failed to allocate DPI bitmap, rc = %d\n", rc);
		return rc;
	}

	return rc;
}

static void
qed_vf_pf_rdma_free(struct qed_hwfn		*p_hwfn,
		    struct qed_rdma_info	*p_rdma_info)
{
	DP_VERBOSE(p_hwfn, QED_MSG_RDMA, "Free RDMA\n");

	/* Free pd bitmap */
	qed_rdma_bmap_free(p_hwfn, &p_rdma_info->pd_map, 1);
	/* Free DPI bitmap */
	qed_rdma_bmap_free(p_hwfn, &p_rdma_info->dpi_map, 1);

	kfree(p_rdma_info->port);
	p_rdma_info->port = NULL;

	kfree(p_rdma_info->dev);
	p_rdma_info->dev = NULL;
}

static void
qed_vf_rdma_init_devifno(struct qed_hwfn	*p_hwfn,
			 struct qed_rdma_info	*p_rdma_info)
{
	struct qed_rdma_device *dev = p_rdma_info->dev;

	qed_rdma_get_guid(p_hwfn, (u8 *)(&dev->sys_image_guid));
	dev->node_guid = dev->sys_image_guid;
}

static void qed_vf_rdma_init_events(
	struct qed_rdma_start_in_params *params,
	struct qed_rdma_info		*p_rdma_info)
{
	struct qed_rdma_events *events;

	events = &p_rdma_info->events;

	events->unaffiliated_event	= params->events->unaffiliated_event;
	events->affiliated_event	= params->events->affiliated_event;
	events->context			= params->events->context;
}

static void
qed_vf_rdma_setup(struct qed_hwfn			*p_hwfn,
		  struct qed_rdma_start_in_params	*params,
		  struct qed_rdma_info			*p_rdma_info)
{
	struct qed_vf_iov			*p_iov_info =
		p_hwfn->vf_iov_info;
	struct pfvf_rdma_acquire_resp_tlv	*p_resp =
		&p_iov_info->rdma_acquire_resp;

	p_rdma_info->num_cnqs		= p_resp->num_cnqs;
	p_rdma_info->max_queue_zones	= p_resp->max_queue_zones;

	qed_vf_rdma_init_devifno(p_hwfn, p_rdma_info);
	qed_vf_rdma_init_events(params, p_rdma_info);
}

int
qed_vf_pf_rdma_start(struct qed_hwfn			*p_hwfn,
		     struct qed_rdma_start_in_params	*params,
		     struct qed_rdma_info		*p_rdma_info)
{
	struct qed_rdma_cnq_params	*cnq_pbl_list_virt_addr;
	struct qed_vf_iov		*p_iov	= p_hwfn->vf_iov_info;
	struct vfpf_rdma_start_tlv	*p_req	= NULL;
	struct pfvf_def_resp_tlv	*p_resp = NULL;
	int				rc;

	DP_VERBOSE(p_hwfn, QED_MSG_RDMA,
		   "desired_cnq = %08x\n", params->desired_cnq);

	rc = qed_vf_rdma_alloc(p_hwfn, p_rdma_info);
	if (rc)
		return rc;

	qed_vf_rdma_setup(p_hwfn, params, p_rdma_info);

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_START,
			       sizeof(*p_req));

	cnq_pbl_list_virt_addr = dma_alloc_coherent(
			&p_hwfn->cdev->pdev->dev,
			sizeof(params->cnq_pbl_list),
			&p_req->
			cnq_pbl_list_phy_addr,
			GFP_KERNEL);

	if (!cnq_pbl_list_virt_addr) {
		DP_NOTICE(
			p_hwfn,
			"Failed to allocate `cnq_pbl_list_virt_addr' DMA memory\n");
		rc = -ENOMEM;
		goto exit;
	}

	memcpy((void *)cnq_pbl_list_virt_addr,
	       params->cnq_pbl_list, sizeof(params->cnq_pbl_list));

	qed_vf_pf_populate_tlv_req(p_hwfn, CHANNEL_TLV_RDMA_START,
				   p_req, params);

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA start.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->default_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc)
		goto exit_free_mem;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit_free_mem;
	}

	if (IS_ROCE(p_hwfn)) {
		rc = qed_roce_setup(p_hwfn);
		if (rc)
			goto exit_free_mem;
	}

exit_free_mem: dma_free_coherent(&p_hwfn->cdev->pdev->dev,
				 sizeof(params->cnq_pbl_list),
				 cnq_pbl_list_virt_addr,
				 p_req->cnq_pbl_list_phy_addr);
exit:
	if (rc)
		qed_vf_pf_rdma_free(p_hwfn, p_hwfn->p_rdma_info);

	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_rdma_stop(struct qed_hwfn *p_hwfn)
{
	struct qed_vf_iov		*p_iov	= p_hwfn->vf_iov_info;
	struct vfpf_rdma_stop_tlv	*p_req	= NULL;
	struct pfvf_def_resp_tlv	*p_resp = NULL;
	int				rc;

	qed_vf_pf_rdma_free(p_hwfn, p_hwfn->p_rdma_info);

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_STOP,
			       sizeof(*p_req));

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA stop.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->default_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_rdma_query_counters(struct qed_hwfn	*p_hwfn,
			      struct qed_rdma_counters_out_params
						*out_params)
{
	struct pfvf_rdma_query_counters_resp_tlv	*p_resp = NULL;
	struct vfpf_rdma_query_counters_tlv		*p_req	= NULL;
	struct qed_vf_iov				*p_iov	=
		p_hwfn->vf_iov_info;
	int						rc = 0;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_QUERY_COUNTERS,
			       sizeof(*p_req));

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA query counters\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp	= &p_iov->pf2vf_reply->rdma_query_counters_resp;
	rc	= qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

	qed_vf_pf_populate_tlv_resp(p_hwfn, CHANNEL_TLV_RDMA_QUERY_COUNTERS,
				    out_params, p_resp);

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_rdma_alloc_tid(struct qed_hwfn	*p_hwfn,
			 u32			*p_tid)
{
	struct pfvf_rdma_alloc_tid_resp_tlv	*p_resp = NULL;
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	struct vfpf_rdma_alloc_tid_tlv		*p_req	= NULL;
	int					rc;

	if (!p_tid)
		return -ENOMEM;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_ALLOC_TID,
			       sizeof(*p_req));

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA alloc tid.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->rdma_alloc_tid_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

	*p_tid = p_resp->tid;
exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_rdma_register_tid(struct qed_hwfn				*p_hwfn,
			    struct qed_rdma_register_tid_in_params	*params)
{
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	struct vfpf_rdma_register_tid_tlv	*p_req	= NULL;
	struct pfvf_def_resp_tlv		*p_resp = NULL;
	int					rc;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_REGISTER_TID,
			       sizeof(*p_req));

	qed_vf_pf_populate_tlv_req(p_hwfn, CHANNEL_TLV_RDMA_REGISTER_TID,
				   p_req, params);

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA register tid.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->default_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_rdma_deregister_tid(struct qed_hwfn	*p_hwfn,
			      u32		tid)
{
	struct vfpf_rdma_deregister_tid_tlv	*p_req	= NULL;
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	struct pfvf_def_resp_tlv		*p_resp = NULL;
	int					rc;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_DEREGISTER_TID,
			       sizeof(*p_req));
	p_req->tid = tid;

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA deregister tid.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->default_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_rdma_free_tid(struct qed_hwfn *p_hwfn,
			u32		tid)
{
	struct qed_vf_iov		*p_iov	= p_hwfn->vf_iov_info;
	struct vfpf_rdma_free_tid_tlv	*p_req	= NULL;
	struct pfvf_def_resp_tlv	*p_resp = NULL;
	int				rc;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_FREE_TID,
			       sizeof(*p_req));
	p_req->tid = tid;

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA free tid.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->default_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_rdma_create_cq(struct qed_hwfn			*p_hwfn,
			 struct qed_rdma_create_cq_in_params	*in_params,
			 u16					*icid)
{
	struct pfvf_rdma_create_cq_resp_tlv	*p_resp = NULL;
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	struct vfpf_rdma_create_cq_tlv		*p_req	= NULL;
	int					rc;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_CREATE_CQ,
			       sizeof(*p_req));

	qed_vf_pf_populate_tlv_req(p_hwfn, CHANNEL_TLV_RDMA_CREATE_CQ,
				   p_req, in_params);

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA create CQ.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->rdma_create_cq_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

	*icid = (u16)p_resp->cq_icid;

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_rdma_resize_cq(struct qed_hwfn			*p_hwfn,
			 struct qed_rdma_resize_cq_in_params	*in_params,
			 struct qed_rdma_resize_cq_out_params	*out_params)
{
	struct pfvf_rdma_resize_cq_resp_tlv	*p_resp = NULL;
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	struct vfpf_rdma_resize_cq_tlv		*p_req	= NULL;
	int					rc;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_RESIZE_CQ,
			       sizeof(*p_req));

	qed_vf_pf_populate_tlv_req(p_hwfn, CHANNEL_TLV_RDMA_RESIZE_CQ,
				   p_req, in_params);

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA resize CQ.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->rdma_resize_cq_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

	qed_vf_pf_populate_tlv_resp(p_hwfn, CHANNEL_TLV_RDMA_RESIZE_CQ,
				    out_params, p_resp);
exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_rdma_destroy_cq(struct qed_hwfn			*p_hwfn,
			  struct qed_rdma_destroy_cq_in_params	*in_params,
			  struct qed_rdma_destroy_cq_out_params
								*out_params)
{
	struct pfvf_rdma_destroy_cq_resp_tlv	*p_resp = NULL;
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	struct vfpf_rdma_destroy_cq_tlv		*p_req	= NULL;
	int					rc;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_DESTROY_CQ,
			       sizeof(*p_req));

	p_req->icid = in_params->icid;

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA destroy CQ.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->rdma_destroy_cq_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

	out_params->num_cq_notif = p_resp->num_cq_notif;

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

struct qed_rdma_qp *
qed_vf_pf_rdma_create_qp(struct qed_hwfn			*p_hwfn,
			 struct qed_rdma_create_qp_in_params	*in_params,
			 struct qed_rdma_create_qp_out_params	*out_params)
{
	struct pfvf_rdma_create_qp_resp_tlv	*p_resp = NULL;
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	struct vfpf_rdma_create_qp_tlv		*p_req	= NULL;
	struct qed_rdma_qp			*qp;
	int					rc;

	qp = kzalloc(sizeof(struct qed_rdma_qp), GFP_KERNEL);
	if (!qp) {
		DP_NOTICE(p_hwfn, "Failed to allocate qed_rdma_qp\n");
		return NULL;
	}

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_CREATE_QP,
			       sizeof(*p_req));

	qed_vf_pf_populate_tlv_req(p_hwfn, CHANNEL_TLV_RDMA_CREATE_QP,
				   p_req, in_params);

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA create QP.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->rdma_create_qp_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc) {
		kfree(qp);
		qp = NULL;
		goto exit;
	}

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		kfree(qp);
		qp = NULL;
		goto exit;
	}

	out_params->icid	= p_resp->icid;
	out_params->qp_id	= p_resp->qp_id;

	qed_vfpf_channel_qp_to_qp(qp, &p_resp->channel_qp);

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return qp;
}

int
qed_vf_pf_rdma_modify_qp(struct qed_hwfn			*p_hwfn,
			 struct qed_rdma_qp			*qp,
			 struct qed_rdma_modify_qp_in_params	*in_params)
{
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	struct vfpf_rdma_modify_qp_tlv		*p_req	= NULL;
	struct pfvf_rdma_modify_qp_resp_tlv	*p_resp = NULL;
	int					rc;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_MODIFY_QP,
			       sizeof(*p_req));

	qed_vfpf_qp_to_channel_qp(qp, &p_req->channel_qp);

	qed_vf_pf_populate_tlv_req(p_hwfn, CHANNEL_TLV_RDMA_MODIFY_QP,
				   p_req, in_params);

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA modify qp.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->rdma_modify_qp_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

	qed_vfpf_channel_qp_to_qp(qp, &p_resp->channel_qp);
exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_rdma_query_qp(struct qed_hwfn				*p_hwfn,
			struct qed_rdma_qp			*qp,
			struct qed_rdma_query_qp_out_params	*out_params)
{
	struct pfvf_rdma_query_qp_resp_tlv	*p_resp = NULL;
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	struct vfpf_rdma_query_qp_tlv		*p_req	= NULL;
	int					rc;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_QUERY_QP,
			       sizeof(*p_req));

	qed_vfpf_qp_to_channel_qp(qp, &p_req->channel_qp);

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA query qp.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->rdma_query_qp_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

	qed_vf_pf_populate_tlv_resp(p_hwfn, CHANNEL_TLV_RDMA_QUERY_QP,
				    out_params, p_resp);

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_rdma_destroy_qp(struct qed_hwfn	*p_hwfn,
			  struct qed_rdma_qp	*qp,
			  struct qed_rdma_destroy_qp_out_params
						*out_params)
{
	struct pfvf_rdma_destroy_qp_resp_tlv	*p_resp = NULL;
	struct vfpf_rdma_destroy_qp_tlv		*p_req	= NULL;
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	int					rc;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_DESTROY_QP,
			       sizeof(*p_req));

	qed_vfpf_qp_to_channel_qp(qp, &p_req->channel_qp);

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA destroy qp.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->rdma_destroy_qp_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

	qed_vf_pf_populate_tlv_resp(p_hwfn, CHANNEL_TLV_RDMA_DESTROY_QP,
				    out_params, p_resp);

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_rdma_create_srq(struct qed_hwfn			*p_hwfn,
			  struct qed_rdma_create_srq_in_params	*in_params,
			  struct qed_rdma_create_srq_out_params *out_params)
{
	struct pfvf_rdma_create_srq_resp_tlv	*p_resp = NULL;
	struct vfpf_rdma_create_srq_tlv		*p_req	= NULL;
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	int					rc;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_CREATE_SRQ,
			       sizeof(*p_req));

	qed_vf_pf_populate_tlv_req(p_hwfn, CHANNEL_TLV_RDMA_CREATE_SRQ,
				   p_req, in_params);

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA create SRQ.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->rdma_create_srq_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

	out_params->srq_id = p_resp->srq_id;

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_rdma_modify_srq(struct qed_hwfn			*p_hwfn,
			  struct qed_rdma_modify_srq_in_params	*in_params)
{
	struct pfvf_rdma_modify_srq_resp_tlv	*p_resp = NULL;
	struct vfpf_rdma_modify_srq_tlv		*p_req	= NULL;
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	int					rc;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_MODIFY_SRQ,
			       sizeof(*p_req));

	qed_vf_pf_populate_tlv_req(p_hwfn, CHANNEL_TLV_RDMA_MODIFY_SRQ,
				   p_req, in_params);

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA modify SRQ.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->rdma_modify_srq_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS)
		rc = -EINVAL;

	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_rdma_destroy_srq(
	struct qed_hwfn *p_hwfn,
	struct qed_rdma_destroy_srq_in_params	     *
	in_params)
{
	struct pfvf_rdma_destroy_srq_resp_tlv	*p_resp = NULL;
	struct vfpf_rdma_destroy_srq_tlv	*p_req	= NULL;
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	int					rc;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_DESTROY_SRQ,
			       sizeof(*p_req));

	qed_vf_pf_populate_tlv_req(p_hwfn, CHANNEL_TLV_RDMA_DESTROY_SRQ,
				   p_req, in_params);

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA destroy SRQ.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->rdma_destroy_srq_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS)
		rc = -EINVAL;

	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

struct qed_rdma_port *
qed_vf_pf_rdma_query_port(struct qed_hwfn *p_hwfn)
{
	struct pfvf_rdma_query_port_resp_tlv	*p_resp = NULL;
	struct vfpf_rdma_query_port_tlv		*p_req	= NULL;
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	struct qed_rdma_port			*port	= NULL;
	int					rc	= 0;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_QUERY_PORT,
			       sizeof(*p_req));

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA query port.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->rdma_query_port_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS)
		goto exit;

	port = p_hwfn->p_rdma_info->port;

	qed_vf_pf_populate_tlv_resp(p_hwfn, CHANNEL_TLV_RDMA_QUERY_PORT,
				    port, p_resp);

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return port;
}

int
qed_vf_pf_rdma_query_device(struct qed_hwfn *p_hwfn)
{
	struct pfvf_rdma_query_device_resp_tlv	*p_resp = NULL;
	struct vfpf_rdma_query_device_tlv	*p_req	= NULL;
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	int					rc	= 0;

	/* clear mailbox and prep header tlv */
	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_RDMA_QUERY_DEVICE,
			       sizeof(*p_req));

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Requesting RDMA query device.\n");

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset, CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp = &p_iov->pf2vf_reply->rdma_query_device_resp;

	rc = qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

	qed_vf_pf_populate_tlv_resp(p_hwfn, CHANNEL_TLV_RDMA_QUERY_DEVICE,
				    p_hwfn->p_rdma_info->dev, p_resp);

exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

int
qed_vf_pf_filter_cfg(struct qed_hwfn			*p_hwfn,
		     struct qed_ntuple_filter_params	*params)
{
	struct qed_vf_iov		*p_iov = p_hwfn->vf_iov_info;
	struct vfpf_filter_cfg_tlv	*p_req;
	struct pfvf_def_resp_tlv	*p_resp;
	int				rc;

	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_FILTER_CFG, sizeof(*p_req));

	/* Fill structure here */
	p_req->qid		= params->qid;
	p_req->mode		= params->mode;
	p_req->b_is_add		= params->b_is_add;
	p_req->b_is_drop	= params->b_is_drop;
	p_req->length		= min_t(u32, params->length, MAX_PKT_HDR_LEN);
	memcpy(p_req->packet_hdr_buf, params->pkt_virt_addr, p_req->length);

	/* add list termination tlv */
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));
	p_resp	= &p_iov->pf2vf_reply->default_resp;
	rc	= qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc) {
		DP_ERR(
			p_hwfn,
			"Sending Filter rule configuration for VF failed rc=0x%x\n",
			rc);
		goto exit;
	}

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		DP_ERR(p_hwfn,
		       "Filter rule configuration for VF failed rc=0x%x\n",
		       rc);
	}
exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}

u16 qed_vf_get_igu_sb_id(struct qed_hwfn	*p_hwfn,
			 u16			sb_id)
{
	struct qed_vf_iov	*p_iov = p_hwfn->vf_iov_info;
	u8			num_rxqs;

	if (!p_iov) {
		DP_NOTICE(p_hwfn, "vf_sriov_info isn't initialized\n");
		return 0;
	}

	qed_vf_get_num_rxqs(p_hwfn, &num_rxqs);

	if (sb_id < num_rxqs)
		return p_iov->acquire_resp.resc.hw_sbs[sb_id].hw_sb_id;
	else
		return p_iov->rdma_acquire_resp.hw_sbs[sb_id -
						       num_rxqs].hw_sb_id;
}

void qed_vf_set_sb_info(struct qed_hwfn		*p_hwfn,
			u16			sb_id,
			struct qed_sb_info	*p_sb)
{
	struct qed_vf_iov *p_iov = p_hwfn->vf_iov_info;

	if (!p_iov) {
		DP_NOTICE(p_hwfn, "vf_sriov_info isn't initialized\n");
		return;
	}

	if (sb_id >= PFVF_MAX_SBS_PER_VF) {
		DP_NOTICE(p_hwfn, "Can't configure SB %04x\n", sb_id);
		return;
	}
	p_iov->sbs_info[sb_id] = p_sb;
}

void qed_vf_init_admin_mac(struct qed_hwfn	*p_hwfn,
			   u8			*p_mac)
{
	struct qed_vf_iov		*p_iov = p_hwfn->vf_iov_info;
	struct qed_bulletin_content	temp;
	u32				crc, crc_size;

	crc_size = sizeof(p_iov->bulletin.p_virt->crc);
	memcpy(&temp, p_iov->bulletin.p_virt, p_iov->bulletin.size);

	/* If version did not update, no need to do anything */
	if (temp.version == p_iov->bulletin_shadow.version) {
		DP_VERBOSE(p_hwfn, QED_MSG_IOV,
			   "Bulletin version didn't update\n");
		return;
	}

	/* Verify the bulletin we see is valid */
	crc = crc32(0, (u8 *)&temp + crc_size,
		    p_iov->bulletin.size - crc_size);
	if (crc != temp.crc) {
		DP_VERBOSE(p_hwfn, QED_MSG_IOV,
			   "Bulletin CRC validation failed\n");
		return;
	}

	if ((temp.valid_bitmap & BIT(MAC_ADDR_FORCED)) ||
	    (temp.valid_bitmap & (1 << VFPF_BULLETIN_MAC_ADDR))) {
		memcpy(p_mac, temp.mac, ETH_ALEN);
		DP_VERBOSE(
			p_hwfn,
			QED_MSG_IOV,
			"Admin MAC read from bulletin:[%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx]\n",
			p_mac[0],
			p_mac[1],
			p_mac[2],
			p_mac[3],
			p_mac[4],
			p_mac[5]);
	}
}

int qed_vf_read_bulletin(struct qed_hwfn	*p_hwfn,
			 u8			*p_change)
{
	struct qed_vf_iov		*p_iov = p_hwfn->vf_iov_info;
	struct qed_bulletin_content	shadow;
	u32				crc, crc_size;

	crc_size	= sizeof(p_iov->bulletin.p_virt->crc);
	*p_change	= 0;

	/* Need to guarantee PF is not in the middle of writing it */
	memcpy(&shadow, p_iov->bulletin.p_virt, p_iov->bulletin.size);

	/* If version did not update, no need to do anything */
	if (shadow.version == p_iov->bulletin_shadow.version)
		return 0;

	/* Verify the bulletin we see is valid */
	crc = crc32(0, (u8 *)&shadow + crc_size,
		    p_iov->bulletin.size - crc_size);
	if (crc != shadow.crc)
		return -EAGAIN;

	/* Set the shadow bulletin and process it */
	memcpy(&p_iov->bulletin_shadow, &shadow, p_iov->bulletin.size);

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Read a bulletin update %08x\n", shadow.version);

	*p_change = 1;

	return 0;
}

void __qed_vf_get_link_params(struct qed_mcp_link_params	*p_params,
			      struct qed_bulletin_content	*p_bulletin)
{
	memset(p_params, 0, sizeof(*p_params));

	p_params->speed.autoneg			= p_bulletin->req_autoneg;
	p_params->speed.advertised_speeds	= p_bulletin->req_adv_speed;
	p_params->speed.forced_speed		= p_bulletin->req_forced_speed;
	p_params->pause.autoneg			= p_bulletin->req_autoneg_pause;
	p_params->pause.forced_rx		= p_bulletin->req_forced_rx;
	p_params->pause.forced_tx		= p_bulletin->req_forced_tx;
	p_params->loopback_mode			= p_bulletin->req_loopback;
}

void qed_vf_get_link_params(struct qed_hwfn		*p_hwfn,
			    struct qed_mcp_link_params	*params)
{
	__qed_vf_get_link_params(params,
				 &(p_hwfn->vf_iov_info->bulletin_shadow));
}

void __qed_vf_get_link_state(struct qed_mcp_link_state		*p_link,
			     struct qed_bulletin_content	*p_bulletin)
{
	memset(p_link, 0, sizeof(*p_link));

	p_link->link_up			= p_bulletin->link_up;
	p_link->speed			= p_bulletin->speed;
	p_link->full_duplex		= p_bulletin->full_duplex;
	p_link->an			= p_bulletin->autoneg;
	p_link->an_complete		= p_bulletin->autoneg_complete;
	p_link->parallel_detection	= p_bulletin->parallel_detection;
	p_link->pfc_enabled		= p_bulletin->pfc_enabled;
	p_link->partner_adv_speed	= p_bulletin->partner_adv_speed;
	p_link->partner_tx_flow_ctrl_en = p_bulletin->partner_tx_flow_ctrl_en;
	p_link->partner_rx_flow_ctrl_en = p_bulletin->partner_rx_flow_ctrl_en;
	p_link->partner_adv_pause	= p_bulletin->partner_adv_pause;
	p_link->sfp_tx_fault		= p_bulletin->sfp_tx_fault;
}

void qed_vf_get_link_state(struct qed_hwfn		*p_hwfn,
			   struct qed_mcp_link_state	*link)
{
	__qed_vf_get_link_state(link,
				&(p_hwfn->vf_iov_info->bulletin_shadow));
}

void __qed_vf_get_link_caps(struct qed_mcp_link_capabilities	*p_link_caps,
			    struct qed_bulletin_content		*p_bulletin)
{
	memset(p_link_caps, 0, sizeof(*p_link_caps));
	p_link_caps->speed_capabilities = p_bulletin->capability_speed;
}

void qed_vf_get_link_caps(struct qed_hwfn			*p_hwfn,
			  struct qed_mcp_link_capabilities	*p_link_caps)
{
	__qed_vf_get_link_caps(p_link_caps,
			       &(p_hwfn->vf_iov_info->bulletin_shadow));
}

void qed_vf_get_num_sbs(struct qed_hwfn *p_hwfn,
			u8		*num_sbs)
{
	*num_sbs = p_hwfn->vf_iov_info->acquire_resp.resc.num_sbs;
}

void qed_vf_get_num_rxqs(struct qed_hwfn	*p_hwfn,
			 u8			*num_rxqs)
{
	*num_rxqs = p_hwfn->vf_iov_info->acquire_resp.resc.num_rxqs;
}

void qed_vf_get_num_txqs(struct qed_hwfn	*p_hwfn,
			 u8			*num_txqs)
{
	*num_txqs = p_hwfn->vf_iov_info->acquire_resp.resc.num_txqs;
}

void qed_vf_get_num_cids(struct qed_hwfn	*p_hwfn,
			 u8			*num_cids)
{
	*num_cids = p_hwfn->vf_iov_info->acquire_resp.resc.num_cids;
}

void qed_vf_get_num_cnqs(struct qed_hwfn	*p_hwfn,
			 u8			*num_cnqs)
{
	*num_cnqs = p_hwfn->vf_iov_info->rdma_acquire_resp.num_cnqs;
}

u8 qed_vf_rdma_cnq_sb_start_id(struct qed_hwfn *p_hwfn)
{
	/* VF's CNQ will start right after the the L2 queues */
	u8 num_rxqs;

	qed_vf_get_num_rxqs(p_hwfn, &num_rxqs);
	return num_rxqs;
}

void qed_vf_get_port_mac(struct qed_hwfn	*p_hwfn,
			 u8			*port_mac)
{
	memcpy(port_mac,
	       p_hwfn->vf_iov_info->acquire_resp.pfdev_info.port_mac,
	       ETH_ALEN);
}

void qed_vf_get_num_vlan_filters(struct qed_hwfn	*p_hwfn,
				 u8			*num_vlan_filters)
{
	struct qed_vf_iov *p_vf;

	p_vf			= p_hwfn->vf_iov_info;
	*num_vlan_filters	= p_vf->acquire_resp.resc.num_vlan_filters;
}

void qed_vf_get_num_mac_filters(struct qed_hwfn *p_hwfn,
				u8		*num_mac_filters)
{
	struct qed_vf_iov *p_vf = p_hwfn->vf_iov_info;

	*num_mac_filters = p_vf->acquire_resp.resc.num_mac_filters;
}

bool qed_vf_check_mac(struct qed_hwfn	*p_hwfn,
		      u8		*mac)
{
	struct qed_bulletin_content *bulletin;

	bulletin = &p_hwfn->vf_iov_info->bulletin_shadow;
	if (!(bulletin->valid_bitmap & BIT(MAC_ADDR_FORCED)))
		return true;

	/* Forbid VF from changing a MAC enforced by PF */
	if (memcmp(bulletin->mac, mac, ETH_ALEN))
		return false;

	return false;
}

static bool qed_vf_bulletin_get_forced_mac(struct qed_hwfn	*hwfn,
					   u8			*dst_mac,
					   u8			*p_is_forced)
{
	struct qed_bulletin_content *bulletin;

	bulletin = &hwfn->vf_iov_info->bulletin_shadow;

	if (bulletin->valid_bitmap & BIT(MAC_ADDR_FORCED)) {
		if (p_is_forced)
			*p_is_forced = 1;
	} else if (bulletin->valid_bitmap & BIT(VFPF_BULLETIN_MAC_ADDR)) {
		if (p_is_forced)
			*p_is_forced = 0;
	} else {
		return false;
	}

	memcpy(dst_mac, bulletin->mac, ETH_ALEN);

	return true;
}

void qed_vf_bulletin_get_udp_ports(struct qed_hwfn	*p_hwfn,
				   u16			*p_vxlan_port,
				   u16			*p_geneve_port)
{
	struct qed_bulletin_content *p_bulletin;

	p_bulletin = &p_hwfn->vf_iov_info->bulletin_shadow;

	*p_vxlan_port	= p_bulletin->vxlan_udp_port;
	*p_geneve_port	= p_bulletin->geneve_udp_port;
}


void qed_vf_get_fw_version(struct qed_hwfn	*p_hwfn,
			   u16			*fw_major,
			   u16			*fw_minor,
			   u16			*fw_rev,
			   u16			*fw_eng)
{
	struct pf_vf_pfdev_info *info;

	info = &p_hwfn->vf_iov_info->acquire_resp.pfdev_info;

	*fw_major	= info->fw_major;
	*fw_minor	= info->fw_minor;
	*fw_rev		= info->fw_rev;
	*fw_eng		= info->fw_eng;
}

void qed_vf_update_mac(struct qed_hwfn	*p_hwfn,
		       u8		*mac)
{
	if (p_hwfn->vf_iov_info)
		memcpy(p_hwfn->vf_iov_info->mac_addr, mac, ETH_ALEN);
}


static void qed_vf_db_recovery_execute(struct qed_hwfn *p_hwfn)
{
	struct qed_vf_iov		*p_iov = p_hwfn->vf_iov_info;
	struct qed_bulletin_content	*p_bulletin;

	p_bulletin = &p_iov->bulletin_shadow;

	/* Got a new indication from PF to execute doorbell overflow recovery */
	if (p_iov->db_recovery_execute_prev != p_bulletin->db_recovery_execute)
		qed_db_recovery_execute(p_hwfn);

	p_iov->db_recovery_execute_prev = p_bulletin->db_recovery_execute;
}

static int
qed_vf_async_event_req(struct qed_hwfn *p_hwfn)
{
	struct qed_vf_iov			*p_iov	= p_hwfn->vf_iov_info;
	struct pfvf_async_event_resp_tlv	*p_resp = NULL;
	struct vfpf_async_event_req_tlv		*p_req	= NULL;
	struct qed_bulletin_content		*p_bulletin;
	int					rc;
	u8					i;

	p_bulletin = &p_iov->bulletin_shadow;
	if (p_iov->eq_completion_prev == p_bulletin->eq_completion)
		return 0;

	/* Update prev value for future comparison */
	p_iov->eq_completion_prev = p_bulletin->eq_completion;

	p_req = qed_vf_pf_prep(p_hwfn, CHANNEL_TLV_ASYNC_EVENT,
			       sizeof(*p_req));
	qed_add_tlv(&p_iov->offset,
		    CHANNEL_TLV_LIST_END,
		    sizeof(struct channel_list_end_tlv));

	p_resp	= &p_iov->pf2vf_reply->async_event_resp;
	rc	= qed_send_msg2pf(p_hwfn, &p_resp->hdr.status, sizeof(*p_resp));
	if (rc)
		goto exit;

	if (p_resp->hdr.status != PFVF_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

	for (i = 0; i < p_resp->eq_count; i++) {
		struct event_ring_entry *p_eqe = &p_resp->eqs[i];

		qed_event_ring_entry_dp(p_hwfn, p_eqe);
		rc = qed_async_event_completion(p_hwfn, p_eqe);
		if (rc) DP_NOTICE(
				p_hwfn,
				"Processing async event completion failed - %s:0x%02x %s:0x%02x echo %x\n",
				qed_get_event_ring_entry_opcode_str(p_eqe),
				p_eqe->opcode.raw,
				qed_get_protocol_type_str(p_eqe->protocol_id),
				p_eqe->protocol_id,
				le16_to_cpu(p_eqe->echo));
	}

	rc = 0;
exit:
	qed_vf_pf_req_end(p_hwfn, rc);

	return rc;
}


static void qed_handle_bulletin_change(struct qed_hwfn *hwfn)
{
	struct qed_eth_cb_ops	*ops = hwfn->cdev->protocol_ops.eth;
	u8			mac[ETH_ALEN], is_mac_exist, is_mac_forced;
	void			*cookie = hwfn->cdev->ops_cookie;
	u16			vxlan_port, geneve_port;

	qed_vf_bulletin_get_udp_ports(hwfn, &vxlan_port, &geneve_port);
	is_mac_exist = qed_vf_bulletin_get_forced_mac(hwfn, mac,
						      &is_mac_forced);
	if (is_mac_exist && cookie) {
		ops->force_mac(cookie, mac, !!is_mac_forced);
		qed_vf_update_mac(hwfn, mac);
	}

	ops->ports_update(cookie, vxlan_port, geneve_port);
	/* Always update link configuration according to bulletin */
	qed_link_update(hwfn, NULL);

	qed_vf_db_recovery_execute(hwfn);

	qed_vf_async_event_req(hwfn);
}

void qed_iov_vf_task(struct work_struct *work)
{
	struct qed_hwfn *hwfn = container_of(work, struct qed_hwfn,
					     iov_task.work);
	u8		change = 0;

	if (test_and_clear_bit(QED_IOV_WQ_STOP_WQ_FLAG,
			       &hwfn->iov_task_flags))
		return;

	/* Handle bulletin board changes */
	qed_vf_read_bulletin(hwfn, &change);
	if (test_and_clear_bit(QED_IOV_WQ_VF_FORCE_LINK_QUERY_FLAG,
			       &hwfn->iov_task_flags))
		change = 1;
	if (change)
		qed_handle_bulletin_change(hwfn);

	/* As VF is polling bulletin board, need to constantly re-schedule */
	queue_delayed_work(hwfn->iov_wq, &hwfn->iov_task, HZ);
}
