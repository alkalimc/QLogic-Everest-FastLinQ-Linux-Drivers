/*******************************************************************************
 * File name : fcoe_init.c
 * Author    : Asaf Ravid
 *******************************************************************************
 *******************************************************************************
 * Description: 
 * FCoE HSI functions
 *
 *******************************************************************************
 * Notes: This is the input to the auto generated file drv_fcoe_fw_funcs.c
 * 
 *******************************************************************************/

#include "drv_scsi_fw_funcs.h"
#include "drv_e4_fcoe_fw_funcs.h"
#include "fcoe_common.h"




#define FCOE_RX_ID 0xFFFFu


static inline void init_common_sqe(struct e4_fcoe_task_params         *task_params,
								   enum fcoe_sqe_request_type       request_type)
{
	HWAL_MEMSET(task_params->sqe, 0, sizeof(*(task_params->sqe)));
	
	HWAL_SET_FIELD(task_params->sqe->flags, FCOE_WQE_REQ_TYPE, request_type);
	task_params->sqe->task_id = task_params->itid;
}

int init_initiator_rw_fcoe_task(struct e4_fcoe_task_params        *task_params,
									  struct scsi_sgl_task_params    *sgl_task_params,
									  struct regpair			     sense_data_buffer_phys_addr,
									  		 u32				     task_retry_id,
									  		 u8					     fcp_cmd_payload[32])
{
	bool slow_sgl;
	u32 io_size;

	/* preserve validaiton byte*/
	const u8 val_byte = task_params->context->ystorm_ag_context.byte0;

	HWAL_MEMSET(task_params->context, 0, sizeof(*(task_params->context)));
	task_params->context->ystorm_ag_context.byte0 = val_byte;

	slow_sgl = scsi_is_slow_sgl(sgl_task_params->num_sges, sgl_task_params->small_mid_sge);
	io_size = (task_params->task_type == FCOE_TASK_TYPE_WRITE_INITIATOR ? task_params->tx_io_size : task_params->rx_io_size);
  	
	// Ystorm task_params->context
  	task_params->context->ystorm_st_context.data_2_trns_rem = HWAL_CPU_TO_LE32(io_size);
  	task_params->context->ystorm_st_context.task_rety_identifier = HWAL_CPU_TO_LE32(task_retry_id);
	task_params->context->ystorm_st_context.task_type = (u8)(task_params->task_type);

	HWAL_MEMCPY((void*)&task_params->context->ystorm_st_context.tx_info_union.fcp_cmd_payload,
		fcp_cmd_payload, sizeof(struct fcoe_fcp_cmd_payload));
  
  	// Tstorm task_params->context
  	task_params->context->tstorm_st_context.read_only.dev_type = (u8)(task_params->is_tape_device == 1 ? FCOE_TASK_DEV_TYPE_TAPE : FCOE_TASK_DEV_TYPE_DISK);
  	task_params->context->tstorm_st_context.read_only.cid = HWAL_CPU_TO_LE32(task_params->conn_cid);
  	task_params->context->tstorm_st_context.read_only.glbl_q_num = HWAL_CPU_TO_LE32(task_params->cq_rss_number);
  	task_params->context->tstorm_st_context.read_only.fcp_cmd_trns_size = HWAL_CPU_TO_LE32(io_size);
	task_params->context->tstorm_st_context.read_only.task_type = (u8)(task_params->task_type);

  	HWAL_SET_FIELD(task_params->context->tstorm_st_context.read_write.flags, FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_EXP_FIRST_FRAME, 1);
  	task_params->context->tstorm_st_context.read_write.rx_id = HWAL_CPU_TO_LE16(FCOE_RX_ID);
  
  	// Ustorm task_params->context
  	task_params->context->ustorm_ag_context.global_cq_num = HWAL_CPU_TO_LE32(task_params->cq_rss_number);

	// Mstorm buffer for sense/rsp data placement
	task_params->context->mstorm_st_context.rsp_buf_addr.lo = HWAL_CPU_TO_LE32(sense_data_buffer_phys_addr.lo);
	task_params->context->mstorm_st_context.rsp_buf_addr.hi = HWAL_CPU_TO_LE32(sense_data_buffer_phys_addr.hi);

  	if (task_params->task_type == FCOE_TASK_TYPE_WRITE_INITIATOR) {
  		// WRITE TASK
  
  		// Ystorm task_params->context
  		task_params->context->ystorm_st_context.expect_first_xfer = 1;
  		
  
  		// Set the amount of super SGEs. Can be up to 4.
  		HWAL_SET_FIELD(task_params->context->ystorm_st_context.sgl_mode, YSTORM_FCOE_TASK_ST_CTX_TX_SGL_MODE, (slow_sgl ? SCSI_TX_SLOW_SGL : SCSI_FAST_SGL));
  
		init_scsi_sgl_context(&task_params->context->ystorm_st_context.sgl_params,
			&task_params->context->ystorm_st_context.data_desc,
			sgl_task_params);
  		
  		// Mstorm task_params->context
  		 HWAL_SET_FIELD(task_params->context->mstorm_st_context.flags, MSTORM_FCOE_TASK_ST_CTX_TX_SGL_MODE, (slow_sgl ? SCSI_TX_SLOW_SGL : SCSI_FAST_SGL));

		 task_params->context->mstorm_st_context.sgl_params.sgl_num_sges = HWAL_CPU_TO_LE16(sgl_task_params->num_sges);

  	} else {
  		// READ TASK
 
  		// Tstorm task_params->context

  		HWAL_SET_FIELD(task_params->context->tstorm_st_context.read_write.flags, FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_RX_SGL_MODE, (slow_sgl ? SCSI_TX_SLOW_SGL : SCSI_FAST_SGL));
  		
  		// Mstorm task_params->context
  		task_params->context->mstorm_st_context.data_2_trns_rem = HWAL_CPU_TO_LE32(io_size);
  
		init_scsi_sgl_context(&task_params->context->mstorm_st_context.sgl_params,
			&task_params->context->mstorm_st_context.data_desc,
			sgl_task_params);
  	}
  
  	// Init Sqe:
  	init_common_sqe(task_params, SEND_FCOE_CMD);

	return 0;
}

int init_initiator_midpath_unsolicited_fcoe_task(struct e4_fcoe_task_params *task_params,
				struct fcoe_tx_mid_path_params  *mid_path_fc_header,
				struct scsi_sgl_task_params     *tx_sgl_task_params,
				struct scsi_sgl_task_params     *rx_sgl_task_params,
				u8 fw_to_place_fc_header)
{
	/* preserve validaiton byte*/
	const u8 val_byte = task_params->context->ystorm_ag_context.byte0;

	HWAL_MEMSET(task_params->context, 0, sizeof(*(task_params->context)));
	task_params->context->ystorm_ag_context.byte0 = val_byte;

	// Init Ystorm 
	init_scsi_sgl_context(&task_params->context->ystorm_st_context.sgl_params,
		&task_params->context->ystorm_st_context.data_desc, tx_sgl_task_params);

	HWAL_SET_FIELD(task_params->context->ystorm_st_context.sgl_mode, YSTORM_FCOE_TASK_ST_CTX_TX_SGL_MODE, SCSI_FAST_SGL);

	task_params->context->ystorm_st_context.data_2_trns_rem = HWAL_CPU_TO_LE32(task_params->tx_io_size);
	task_params->context->ystorm_st_context.task_type = (u8)(task_params->task_type);

	HWAL_MEMCPY((void*)&task_params->context->ystorm_st_context.tx_info_union.tx_params.mid_path,
		mid_path_fc_header, sizeof(struct fcoe_tx_mid_path_params));

	// Init Mstorm
	init_scsi_sgl_context(&task_params->context->mstorm_st_context.sgl_params,
		&task_params->context->mstorm_st_context.data_desc, rx_sgl_task_params);

	HWAL_SET_FIELD(task_params->context->mstorm_st_context.flags, MSTORM_FCOE_TASK_ST_CTX_MP_INCLUDE_FC_HEADER, fw_to_place_fc_header);
	task_params->context->mstorm_st_context.data_2_trns_rem = HWAL_CPU_TO_LE32(task_params->rx_io_size);

	// Init Tstorm
	task_params->context->tstorm_st_context.read_only.cid = HWAL_CPU_TO_LE32(task_params->conn_cid);
	task_params->context->tstorm_st_context.read_only.glbl_q_num = HWAL_CPU_TO_LE32(task_params->cq_rss_number);
	task_params->context->tstorm_st_context.read_only.task_type = (u8)(task_params->task_type);
	HWAL_SET_FIELD(task_params->context->tstorm_st_context.read_write.flags, FCOE_TSTORM_FCOE_TASK_ST_CTX_READ_WRITE_EXP_FIRST_FRAME, 1);
	task_params->context->tstorm_st_context.read_write.rx_id = HWAL_CPU_TO_LE16(FCOE_RX_ID);

	// Init Ustorm
	task_params->context->ustorm_ag_context.global_cq_num = HWAL_CPU_TO_LE32(task_params->cq_rss_number);

	// Init SQE
	init_common_sqe(task_params, SEND_FCOE_MIDPATH);

	task_params->sqe->additional_info_union.burst_length = tx_sgl_task_params->total_buffer_size;
	HWAL_SET_FIELD(task_params->sqe->flags, FCOE_WQE_NUM_SGES, tx_sgl_task_params->num_sges);
	HWAL_SET_FIELD(task_params->sqe->flags, FCOE_WQE_SGL_MODE, SCSI_FAST_SGL);

	return 0;
}

int init_initiator_abort_fcoe_task(struct e4_fcoe_task_params    *task_params)
{
	init_common_sqe(task_params, SEND_FCOE_ABTS_REQUEST);

	return 0;
}

int init_initiator_cleanup_fcoe_task(struct e4_fcoe_task_params    *task_params)
{
	init_common_sqe(task_params, FCOE_EXCHANGE_CLEANUP);

	return 0;
}

int init_initiator_sequence_recovery_fcoe_task(struct e4_fcoe_task_params   *task_params, u32 desired_offset)
{
	init_common_sqe(task_params, FCOE_SEQUENCE_RECOVERY);

	task_params->sqe->additional_info_union.seq_rec_updated_offset = desired_offset;
	
	return 0;
}

