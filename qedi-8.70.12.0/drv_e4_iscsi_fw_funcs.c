/*******************************************************************************
 * File name : iscsi_init.c
 * Author    : Asaf Ravid
 *******************************************************************************
 *******************************************************************************
 * Description: 
 * ISCSI HSI functions
 *
 *******************************************************************************
 * Notes: This is the input to the auto generated file drv_iscsi_fw_funcs.c
 * 
 *******************************************************************************/

#include "drv_scsi_fw_funcs.h"
#include "drv_e4_iscsi_fw_funcs.h"
#include "drv_scsi_fw_funcs_al.h"




/* The following function calculates the RW Task Size according to: 
   1. SGL buffer size
   2. DIF at host
   3. DIF at network */
static inline u32 calc_rw_task_size(struct e4_iscsi_task_params  *task_params,
                                    enum   iscsi_task_type       task_type,
                                    struct scsi_sgl_task_params *sgl_task_params,
                                    struct scsi_dif_task_params *dif_task_params)
{
    u32 io_size;    /* IO size without DIF */

    
    if (task_type == ISCSI_TASK_TYPE_INITIATOR_WRITE || task_type == ISCSI_TASK_TYPE_TARGET_READ)
        io_size = task_params->tx_io_size;
    else
        io_size = task_params->rx_io_size;
    
    if (!io_size)
        return 0;

    if (!dif_task_params)
        return io_size;

    return !dif_task_params->dif_on_network ? io_size : sgl_task_params->total_buffer_size;
}

/* The following function initializes DIF flags in task context: */
static inline void init_dif_context_flags(struct iscsi_dif_flags *ctx_dif_flags, struct scsi_dif_task_params *dif_task_params)
{
    if (dif_task_params) {
        HWAL_SET_FIELD(ctx_dif_flags->flags, ISCSI_DIF_FLAGS_PROT_INTERVAL_SIZE_LOG, dif_task_params->dif_block_size_log);
        HWAL_SET_FIELD(ctx_dif_flags->flags, ISCSI_DIF_FLAGS_DIF_TO_PEER,            dif_task_params->dif_on_network ? 1 : 0); /* If DIF protection is configured against target (0=no, 1=yes)*/
        HWAL_SET_FIELD(ctx_dif_flags->flags, ISCSI_DIF_FLAGS_HOST_INTERFACE,         dif_task_params->dif_on_host    ? 1 : 0); /* If DIF/DIX protection is configured against the host (0=none, 1=DIF, 2=DIX 2 bytes, 3=DIX 4 bytes, 4=DIX 8 bytes)*/
    }
}

static inline void init_sqe(struct e4_iscsi_task_params         *task_params,
                            struct scsi_sgl_task_params      *sgl_task_params,
                            struct scsi_dif_task_params      *dif_task_params,
                            struct iscsi_common_hdr          *pdu_header,
                            struct scsi_initiator_cmd_params *cmd_params,
	                        enum   iscsi_task_type            task_type,
                            bool                              is_cleanup)
{
    if (task_params->sqe) {
        HWAL_MEMSET(task_params->sqe, 0, sizeof(*(task_params->sqe)));

        task_params->sqe->task_id = HWAL_CPU_TO_LE16(task_params->itid);

        if (is_cleanup) {
            HWAL_SET_FIELD(task_params->sqe->flags, ISCSI_WQE_WQE_TYPE, ISCSI_WQE_TYPE_TASK_CLEANUP);	/* e.g. vendor specific */
        } else {
            switch (task_type) {
                case ISCSI_TASK_TYPE_INITIATOR_WRITE:
                {   /* In case there is at least one SGL with its size less than 4k this is Slow SGL task */
                    u32 buf_size = 0;
                    u32 num_sges = 0;

                    init_dif_context_flags(&task_params->sqe->prot_flags, dif_task_params);
                    HWAL_SET_FIELD(task_params->sqe->flags, ISCSI_WQE_WQE_TYPE, ISCSI_WQE_TYPE_NORMAL);
                
                    if (task_params->tx_io_size) { /* buf_size and num_sges may change: */
                        buf_size = calc_rw_task_size(task_params, task_type, sgl_task_params, dif_task_params);
                        if (scsi_is_slow_sgl(sgl_task_params->num_sges, sgl_task_params->small_mid_sge))
                            num_sges = ISCSI_WQE_NUM_SGES_SLOWIO;
                        else
                            num_sges = HWAL_MIN32(sgl_task_params->num_sges, SCSI_NUM_SGES_SLOW_SGL_THR);
                    }                    
                    HWAL_SET_FIELD(task_params->sqe->flags,           ISCSI_WQE_NUM_SGES, num_sges);
                    HWAL_SET_FIELD(task_params->sqe->contlen_cdbsize, ISCSI_WQE_CONT_LEN, buf_size);
                    
                    if (HWAL_GET_FIELD(pdu_header->hdr_second_dword, ISCSI_CMD_HDR_TOTAL_AHS_LEN))
                        HWAL_SET_FIELD(task_params->sqe->contlen_cdbsize, ISCSI_WQE_CDB_SIZE, cmd_params->extended_cdb_sge.sge_len);
                }
                break;

                case ISCSI_TASK_TYPE_INITIATOR_READ:
                    HWAL_SET_FIELD(task_params->sqe->flags, ISCSI_WQE_WQE_TYPE, ISCSI_WQE_TYPE_NORMAL);
                    if (HWAL_GET_FIELD(pdu_header->hdr_second_dword, ISCSI_CMD_HDR_TOTAL_AHS_LEN))
                        HWAL_SET_FIELD(task_params->sqe->contlen_cdbsize, ISCSI_WQE_CDB_SIZE, cmd_params->extended_cdb_sge.sge_len);
                    break;

                case ISCSI_TASK_TYPE_TARGET_READ:
                {   /* In case there is at least one SGL with its size less than 4k this is Slow SGL task */
                    u32 buf_size = 0;
                    u32 num_sges = 0;

                    init_dif_context_flags(&task_params->sqe->prot_flags, dif_task_params);
                    HWAL_SET_FIELD(task_params->sqe->flags, ISCSI_WQE_WQE_TYPE, ISCSI_WQE_TYPE_NORMAL);

                    if (HWAL_GET_FIELD(pdu_header->hdr_flags, ISCSI_DATA_IN_HDR_STATUS))
                        HWAL_SET_FIELD(task_params->sqe->flags, ISCSI_WQE_RESPONSE, 1); /* target read response without status bit is unlikely  */
                    
                    if (task_params->tx_io_size) { /* buf_size and num_sges may change: */
                        buf_size = calc_rw_task_size(task_params, task_type, sgl_task_params, dif_task_params);
                        if (scsi_is_slow_sgl(sgl_task_params->num_sges, sgl_task_params->small_mid_sge))
                            num_sges = ISCSI_WQE_NUM_SGES_SLOWIO;
                        else
                            num_sges = HWAL_MIN32(sgl_task_params->num_sges, SCSI_NUM_SGES_SLOW_SGL_THR);
                    }

                    /* BitField flags The driver will give a hint about sizes of SGEs for better credits evaluation at Xstorm*/
                    HWAL_SET_FIELD(task_params->sqe->flags,           ISCSI_WQE_NUM_SGES, num_sges);
                    HWAL_SET_FIELD(task_params->sqe->contlen_cdbsize, ISCSI_WQE_CONT_LEN, buf_size);
                }
                break;

                case ISCSI_TASK_TYPE_TARGET_WRITE:
                    HWAL_SET_FIELD(task_params->sqe->flags, ISCSI_WQE_WQE_TYPE, ISCSI_WQE_TYPE_FIRST_R2T_CONT);
                    break;

                case ISCSI_TASK_TYPE_TARGET_RESPONSE: // Fastpath
                     HWAL_SET_FIELD(task_params->sqe->flags, ISCSI_WQE_WQE_TYPE, ISCSI_WQE_TYPE_RESPONSE);
                     HWAL_SET_FIELD(task_params->sqe->flags, ISCSI_WQE_RESPONSE, 1); /* advances statsn */
                     if (task_params->tx_io_size) { /* This is an SGL: */
                         HWAL_SET_FIELD(task_params->sqe->contlen_cdbsize, ISCSI_WQE_CONT_LEN, task_params->tx_io_size);

                         if (scsi_is_slow_sgl(sgl_task_params->num_sges, sgl_task_params->small_mid_sge))
                             HWAL_SET_FIELD(task_params->sqe->flags, ISCSI_WQE_NUM_SGES, ISCSI_WQE_NUM_SGES_SLOWIO);
                         else /* BitField flags The driver will give a hint about sizes of SGEs for better credits evaluation at Xstorm*/
                             HWAL_SET_FIELD(task_params->sqe->flags, ISCSI_WQE_NUM_SGES, HWAL_MIN32(sgl_task_params->num_sges, SCSI_NUM_SGES_SLOW_SGL_THR));
                     }
                     break;

                case ISCSI_TASK_TYPE_LOGIN_RESPONSE:  // MiddlePath
                case ISCSI_TASK_TYPE_MIDPATH:
                {   /* should advance stat_sn: by a middle-path PDU only of type text-response or of type nop-in request which does not need nop-out inresponse */
                    bool advance_statsn = true; 

                    if (task_type == ISCSI_TASK_TYPE_LOGIN_RESPONSE)
                        HWAL_SET_FIELD(task_params->sqe->flags, ISCSI_WQE_WQE_TYPE, ISCSI_WQE_TYPE_LOGIN);
                    else // ISCSI_TASK_TYPE_MIDPATH
                        HWAL_SET_FIELD(task_params->sqe->flags, ISCSI_WQE_WQE_TYPE, ISCSI_WQE_TYPE_MIDDLE_PATH);

                    if (task_type == ISCSI_TASK_TYPE_MIDPATH) {
                        u8 opcode = HWAL_GET_FIELD(pdu_header->hdr_first_byte, ISCSI_COMMON_HDR_OPCODE); /* Common Opcode Settings */
                        if (opcode != ISCSI_OPCODE_TEXT_RESPONSE && opcode != ISCSI_OPCODE_REJECT && 
                            opcode != ISCSI_OPCODE_ASYNC_MSG && opcode != ISCSI_OPCODE_TMF_RESPONSE &&
                            opcode != ISCSI_OPCODE_LOGOUT_RESPONSE &&
                           (opcode != ISCSI_OPCODE_NOP_IN || pdu_header->itt == ISCSI_TTT_ALL_ONES)
                           )
                            advance_statsn = false;
                    }
                    HWAL_SET_FIELD(task_params->sqe->flags, ISCSI_WQE_RESPONSE, advance_statsn ? 1 : 0); /* advance statsn */

                    if (task_params->tx_io_size) { /* This is an SGL: */
                        HWAL_SET_FIELD(task_params->sqe->contlen_cdbsize, ISCSI_WQE_CONT_LEN, task_params->tx_io_size);
                       
                        if (scsi_is_slow_sgl(sgl_task_params->num_sges, sgl_task_params->small_mid_sge))
                            HWAL_SET_FIELD(task_params->sqe->flags, ISCSI_WQE_NUM_SGES, ISCSI_WQE_NUM_SGES_SLOWIO);
                        else /* BitField flags The driver will give a hint about sizes of SGEs for better credits evaluation at Xstorm*/
                            HWAL_SET_FIELD(task_params->sqe->flags, ISCSI_WQE_NUM_SGES, HWAL_MIN32(sgl_task_params->num_sges, SCSI_NUM_SGES_SLOW_SGL_THR));
                    }
                }
                break;

                default:
                    /* (Debug Mode) */
                    break;
            }
        }
    }
}

/* The following function initializes default values to all tasks */
static inline void init_default_iscsi_task(struct e4_iscsi_task_params *task_params,
                                           struct data_hdr          *pdu_header,
                                           enum   iscsi_task_type    task_type)
{
    u16 dwordIndex;
    struct iscsi_task_context *context = task_params->context;

	/* preserve validation byte before clearing context*/
	const u8 val_byte = context->mstorm_ag_context.cdu_validation;

    HWAL_MEMSET(context, 0, sizeof(*context));
	context->mstorm_ag_context.cdu_validation = val_byte;

    /* Y-Storm Context: Copy the whole input PDU header */
    for (dwordIndex = 0; dwordIndex < HWAL_ARRAY_SIZE(context->ystorm_st_context.pdu_hdr.data.data); dwordIndex++)
        context->ystorm_st_context.pdu_hdr.data.data[dwordIndex] = HWAL_CPU_TO_LE32(pdu_header->data[dwordIndex]);

    /* M-Storm Context: */
    context->mstorm_st_context.task_type = (u8)(task_type);
    context->mstorm_ag_context.task_cid = HWAL_CPU_TO_LE16(task_params->conn_icid);

    /* Ustorm Context: */
    HWAL_SET_FIELD(context->ustorm_ag_context.flags1, USTORM_ISCSI_TASK_AG_CTX_R2T2RECV, 1);
    context->ustorm_st_context.task_type     = (u8)(task_type);
    context->ustorm_st_context.cq_rss_number = task_params->cq_rss_number;
    context->ustorm_ag_context.icid          = HWAL_CPU_TO_LE16(task_params->conn_icid);
}

/* The following function initializes RW tasks CDB context DIF flags in task context: */
static inline void init_initiator_rw_cdb_ystorm_context(struct ystorm_iscsi_task_st_ctx  *ystorm_st_context,
                                                        struct scsi_initiator_cmd_params *cmd_params)
{
    union iscsi_task_hdr *ctx_pdu_hdr = &ystorm_st_context->pdu_hdr;

    if (cmd_params->extended_cdb_sge.sge_len) { /* In extended CDB case, CDB is Not pass through (FW requires the CDB SGE input on hte CDB field) */
        /* Extended CDB (CDB size greater than 16, which is the default): the driver will initialize total_ahs_len field in the command header to the CDB size
           and the CDB field to the Extended CDB's SGE FW will build the header and AHS by itself. Important: In case of extended CDB, FW expects the extended CDB size without padding (FW implementation) */
        HWAL_SET_FIELD(ctx_pdu_hdr->ext_cdb_cmd.hdr_second_dword, ISCSI_EXT_CDB_CMD_HDR_CDB_SIZE, cmd_params->extended_cdb_sge.sge_len);

        ctx_pdu_hdr->ext_cdb_cmd.cdb_sge.sge_addr.hi = HWAL_CPU_TO_LE32(cmd_params->extended_cdb_sge.sge_addr.hi);
        ctx_pdu_hdr->ext_cdb_cmd.cdb_sge.sge_addr.lo = HWAL_CPU_TO_LE32(cmd_params->extended_cdb_sge.sge_addr.lo);
        ctx_pdu_hdr->ext_cdb_cmd.cdb_sge.sge_len  = HWAL_CPU_TO_LE32(cmd_params->extended_cdb_sge.sge_len );
    }
}

/* The following function initializes the U-Storm Task Contexts */
static inline void init_ustorm_task_contexts(struct ustorm_iscsi_task_st_ctx *ustorm_st_context,
                                             struct ustorm_iscsi_task_ag_ctx *ustorm_ag_context,
                                                    u32                       remaining_recv_len,
                                                    u32                       expected_data_transfer_len,
                                                    u8                        num_sges,
                                                    bool                      tx_dif_conn_err_en)
{
    ustorm_st_context->rem_rcv_len           = HWAL_CPU_TO_LE32(remaining_recv_len); /* Remaining data to be received in bytes. Used in validations*/
    ustorm_ag_context->exp_data_acked        = HWAL_CPU_TO_LE32(expected_data_transfer_len);
    ustorm_st_context->exp_data_transfer_len = HWAL_CPU_TO_LE32(expected_data_transfer_len);

    HWAL_SET_FIELD(ustorm_st_context->reg1.reg1_map, ISCSI_REG1_NUM_SGES,                      num_sges);
    HWAL_SET_FIELD(ustorm_ag_context->flags2, USTORM_ISCSI_TASK_AG_CTX_DIF_ERROR_CF_EN, tx_dif_conn_err_en ? 1 : 0);
}

/* The following function initializes the expected data acked and Expected Continuation length values */
static inline
void set_rw_exp_data_acked_and_cont_len(struct iscsi_task_context *context,
                                                      struct iscsi_conn_params  *conn_params,
                                                      enum   iscsi_task_type     task_type,
                                                             u32                 task_size,
                                                             u32                 exp_data_transfer_len,
                                                             u8                  total_ahs_length)
{
    if (total_ahs_length && (task_type == ISCSI_TASK_TYPE_INITIATOR_WRITE || task_type == ISCSI_TASK_TYPE_INITIATOR_READ))
        HWAL_SET_FIELD(context->ustorm_st_context.flags2, USTORM_ISCSI_TASK_ST_CTX_AHS_EXIST, 1);

    switch (task_type) {
    case ISCSI_TASK_TYPE_INITIATOR_WRITE:
    {
        u32 max_unsolicited_data = 0;

        if (!conn_params->initial_r2t)
            max_unsolicited_data = conn_params->first_burst_length;
        else if (conn_params->immediate_data == true)
            max_unsolicited_data = HWAL_MIN32(conn_params->first_burst_length, conn_params->max_send_pdu_length);

        context->ustorm_ag_context.exp_data_acked =
            HWAL_CPU_TO_LE32(total_ahs_length == 0 ? HWAL_MIN32(exp_data_transfer_len, max_unsolicited_data) : ((u32)(total_ahs_length + ISCSI_AHS_CNTL_SIZE)) );
        break;
    }
    case ISCSI_TASK_TYPE_TARGET_READ:
        context->ustorm_ag_context.exp_data_acked = HWAL_CPU_TO_LE32(exp_data_transfer_len);
        break;
    case ISCSI_TASK_TYPE_INITIATOR_READ:
        context->ustorm_ag_context.exp_data_acked = HWAL_CPU_TO_LE32((total_ahs_length == 0 ? 0 : total_ahs_length + ISCSI_AHS_CNTL_SIZE));
        break;
    case ISCSI_TASK_TYPE_TARGET_WRITE:
        context->ustorm_ag_context.exp_cont_len = HWAL_CPU_TO_LE32(task_size);
        break;
    default:
        /* Log/Assert in DebugMode */
        break;
    }
}

/* The following function initializes the TDIF and RDIF Task Contexts */
static inline void init_rtdif_task_context(struct rdif_task_context    *rdif_context,
                                           struct tdif_task_context    *tdif_context,
                                           struct scsi_dif_task_params *dif_task_params,
                                           enum   iscsi_task_type       task_type)
{
	if (dif_task_params->dif_on_network || dif_task_params->dif_on_host) {

		if (task_type == ISCSI_TASK_TYPE_TARGET_WRITE || task_type == ISCSI_TASK_TYPE_INITIATOR_READ) {
	        /* RDIF: */
            rdif_context->initial_ref_tag = HWAL_CPU_TO_LE32(dif_task_params->initial_ref_tag);
            rdif_context->app_tag_value = HWAL_CPU_TO_LE16(dif_task_params->application_tag);
            rdif_context->app_tag_mask = HWAL_CPU_TO_LE16(dif_task_params->application_tag_mask);
            
            HWAL_SET_FIELD(rdif_context->flags0, RDIF_TASK_CONTEXT_IGNORE_APP_TAG,     dif_task_params->ignore_app_tag);
            HWAL_SET_FIELD(rdif_context->flags0, RDIF_TASK_CONTEXT_INITIAL_REF_TAG_VALID, 1);
            HWAL_SET_FIELD(rdif_context->flags0, RDIF_TASK_CONTEXT_HOST_GUARD_TYPE,    dif_task_params->host_guard_type);
            HWAL_SET_FIELD(rdif_context->flags0, RDIF_TASK_CONTEXT_PROTECTION_TYPE,    dif_task_params->protection_type);
            HWAL_SET_FIELD(rdif_context->flags0, RDIF_TASK_CONTEXT_CRC_SEED,           dif_task_params->crc_seed ? 1 : 0);
            HWAL_SET_FIELD(rdif_context->flags0, RDIF_TASK_CONTEXT_KEEP_REF_TAG_CONST, dif_task_params->keep_ref_tag_const ? 1 : 0);
            
            rdif_context->partial_crc_value = HWAL_CPU_TO_LE16(dif_task_params->crc_seed ? 0xffff : 0x0000);
			
            HWAL_SET_FIELD(rdif_context->flags1, RDIF_TASK_CONTEXT_VALIDATE_GUARD,            (dif_task_params->validate_guard   && dif_task_params->dif_on_network) ? 1 : 0);
            HWAL_SET_FIELD(rdif_context->flags1, RDIF_TASK_CONTEXT_VALIDATE_APP_TAG,          (dif_task_params->validate_app_tag && dif_task_params->dif_on_network) ? 1 : 0);
            HWAL_SET_FIELD(rdif_context->flags1, RDIF_TASK_CONTEXT_VALIDATE_REF_TAG,          (dif_task_params->validate_ref_tag && dif_task_params->dif_on_network) ? 1 : 0);
            HWAL_SET_FIELD(rdif_context->flags1, RDIF_TASK_CONTEXT_FORWARD_GUARD,             dif_task_params->forward_guard ? 1 : 0);
            HWAL_SET_FIELD(rdif_context->flags1, RDIF_TASK_CONTEXT_FORWARD_APP_TAG,           dif_task_params->forward_app_tag ? 1 : 0);
            HWAL_SET_FIELD(rdif_context->flags1, RDIF_TASK_CONTEXT_FORWARD_REF_TAG,           dif_task_params->forward_ref_tag ? 1 : 0);
            HWAL_SET_FIELD(rdif_context->flags1, RDIF_TASK_CONTEXT_INTERVAL_SIZE,             dif_task_params->dif_block_size_log - 9);
            HWAL_SET_FIELD(rdif_context->flags1, RDIF_TASK_CONTEXT_HOST_INTERFACE,            dif_task_params->dif_on_host ? 1 : 0);
            HWAL_SET_FIELD(rdif_context->flags1, RDIF_TASK_CONTEXT_NETWORK_INTERFACE,         dif_task_params->dif_on_network ? 1 : 0);
            HWAL_SET_FIELD(rdif_context->flags1, RDIF_TASK_CONTEXT_FORWARD_APP_TAG_WITH_MASK, dif_task_params->forward_app_tag_with_mask ? 1 : 0);
            HWAL_SET_FIELD(rdif_context->flags1, RDIF_TASK_CONTEXT_FORWARD_REF_TAG_WITH_MASK, dif_task_params->forward_ref_tag_with_mask ? 1 : 0);

            HWAL_SET_FIELD(rdif_context->state, RDIF_TASK_CONTEXT_REF_TAG_MASK,              dif_task_params->ref_tag_mask);
		}

		if (task_type == ISCSI_TASK_TYPE_TARGET_READ || task_type == ISCSI_TASK_TYPE_INITIATOR_WRITE) {
			/* TDIF: */
            tdif_context->initial_ref_tag = HWAL_CPU_TO_LE32(dif_task_params->initial_ref_tag);
            tdif_context->app_tag_value = HWAL_CPU_TO_LE16(dif_task_params->application_tag);
            tdif_context->app_tag_mask = HWAL_CPU_TO_LE16(dif_task_params->application_tag_mask);
            tdif_context->partial_crc_value_b = HWAL_CPU_TO_LE16(dif_task_params->crc_seed ? 0xffff : 0x0000);

            HWAL_SET_FIELD(tdif_context->flags0, TDIF_TASK_CONTEXT_IGNORE_APP_TAG,        dif_task_params->ignore_app_tag ? 1 : 0);
            HWAL_SET_FIELD(tdif_context->flags0, TDIF_TASK_CONTEXT_INITIAL_REF_TAG_VALID, dif_task_params->initial_ref_tag_is_valid ? 1 : 0);
            HWAL_SET_FIELD(tdif_context->flags0, TDIF_TASK_CONTEXT_HOST_GUARD_TYPE,       dif_task_params->host_guard_type);
            /* This Bit needs to be set in order (to cause NIG to drop the erroroneous TDIF packets. */
            HWAL_SET_FIELD(tdif_context->flags0, TDIF_TASK_CONTEXT_SET_ERROR_WITH_EOP,    dif_task_params->tx_dif_conn_err_en ? 1 : 0);
            HWAL_SET_FIELD(tdif_context->flags0, TDIF_TASK_CONTEXT_PROTECTION_TYPE,       dif_task_params->protection_type);
            HWAL_SET_FIELD(tdif_context->flags0, TDIF_TASK_CONTEXT_CRC_SEED,              dif_task_params->crc_seed ? 1 : 0);

            HWAL_SET_FIELD(tdif_context->flags1, TDIF_TASK_CONTEXT_VALIDATE_GUARD,            (dif_task_params->validate_guard      && dif_task_params->dif_on_host) ? 1 : 0);
            HWAL_SET_FIELD(tdif_context->flags1, TDIF_TASK_CONTEXT_VALIDATE_APP_TAG,          (dif_task_params->validate_app_tag    && dif_task_params->dif_on_host) ? 1 : 0);
            HWAL_SET_FIELD(tdif_context->flags1, TDIF_TASK_CONTEXT_VALIDATE_REF_TAG,          (dif_task_params->validate_ref_tag    && dif_task_params->dif_on_host) ? 1 : 0);
            HWAL_SET_FIELD(tdif_context->flags1, TDIF_TASK_CONTEXT_FORWARD_GUARD,             dif_task_params->forward_guard ? 1 : 0);
            HWAL_SET_FIELD(tdif_context->flags1, TDIF_TASK_CONTEXT_FORWARD_APP_TAG,           dif_task_params->forward_app_tag ? 1 : 0);
            HWAL_SET_FIELD(tdif_context->flags1, TDIF_TASK_CONTEXT_FORWARD_REF_TAG,           dif_task_params->forward_ref_tag ? 1 : 0);
            HWAL_SET_FIELD(tdif_context->flags1, TDIF_TASK_CONTEXT_INTERVAL_SIZE,             dif_task_params->dif_block_size_log - 9);
            HWAL_SET_FIELD(tdif_context->flags1, TDIF_TASK_CONTEXT_HOST_INTERFACE,            dif_task_params->dif_on_host ? 1 : 0);
            HWAL_SET_FIELD(tdif_context->flags1, TDIF_TASK_CONTEXT_NETWORK_INTERFACE,         dif_task_params->dif_on_network ? 1 : 0);
            HWAL_SET_FIELD(tdif_context->flags1, TDIF_TASK_CONTEXT_REF_TAG_MASK,              dif_task_params->ref_tag_mask);
            HWAL_SET_FIELD(tdif_context->flags1, TDIF_TASK_CONTEXT_FORWARD_APP_TAG_WITH_MASK, dif_task_params->forward_app_tag_with_mask ? 1 : 0);
            HWAL_SET_FIELD(tdif_context->flags1, TDIF_TASK_CONTEXT_FORWARD_REF_TAG_WITH_MASK, dif_task_params->forward_ref_tag_with_mask ? 1 : 0);
            HWAL_SET_FIELD(tdif_context->flags1, TDIF_TASK_CONTEXT_KEEP_REF_TAG_CONST,        dif_task_params->keep_ref_tag_const ? 1 : 0);

            tdif_context->partial_crc_value_a = HWAL_CPU_TO_LE16(dif_task_params->crc_seed ? 0xffff : 0x0000);
		}
	}
}

/* The following function initializes Local Completion Contexts: */
static inline
void set_local_completion_context(struct iscsi_task_context *context)
{
    HWAL_SET_FIELD(context->ystorm_st_context.state.flags, YSTORM_ISCSI_TASK_STATE_LOCAL_COMP,  1);
    HWAL_SET_FIELD(context->ustorm_st_context.flags,       USTORM_ISCSI_TASK_ST_CTX_LOCAL_COMP, 1);
}

#ifndef _INITIATOR_ONLY_ 

static inline int init_tq_iscsi_task(struct iscsi_task_context   *task_context,
                                     struct scsi_sgl_task_params    *sgl_task_params,
                                     struct tqe_opaque              *tqe_opaque_params)
{
	/* preserve validation byte before clearing context*/
	const u8 val_byte = task_context->mstorm_ag_context.cdu_validation;

	HWAL_MEMSET(task_context, 0, sizeof(*task_context));
	task_context->mstorm_ag_context.cdu_validation = val_byte;

    /* M-Storm Context: */
    task_context->mstorm_st_context.task_type = (u8)ISCSI_TASK_TYPE_TARGET_IMM_W_DIF;
    task_context->mstorm_st_context.dif_task_icid = HWAL_CPU_TO_LE16(0xffff);

    /* Ustorm Context: */
    task_context->ustorm_st_context.task_type = (u8)ISCSI_TASK_TYPE_TARGET_IMM_W_DIF;
    task_context->ustorm_st_context.tqe_opaque_list.opaque[0] = HWAL_CPU_TO_LE16(tqe_opaque_params->opaque[0]);
    task_context->ustorm_st_context.tqe_opaque_list.opaque[1] = HWAL_CPU_TO_LE16(tqe_opaque_params->opaque[1]);

    init_scsi_sgl_context(&task_context->mstorm_st_context.sgl_params,
        &task_context->mstorm_st_context.data_desc,
        sgl_task_params);

    return 0;
}

#endif /* #ifndef _INITIATOR_ONLY_  */

/* Common Fastpath task init function: */
static inline int init_rw_iscsi_task(struct e4_iscsi_task_params         *task_params,
                                     enum   iscsi_task_type            task_type,
                                     struct iscsi_conn_params         *conn_params,
                                     struct iscsi_common_hdr          *pdu_header,
                                     struct scsi_sgl_task_params      *sgl_task_params,
                                     struct scsi_initiator_cmd_params *cmd_params,
                                     struct scsi_dif_task_params      *dif_task_params)
{
    u32   task_size             = calc_rw_task_size(task_params, task_type, sgl_task_params, dif_task_params);
    u32   exp_data_transfer_len = conn_params->max_burst_length;
    u8    num_sges              = 0;
    bool  slow_io               = false;

    init_default_iscsi_task(task_params, (struct data_hdr*)pdu_header, task_type);

	/* Target/Initiator: */
    if (task_type == ISCSI_TASK_TYPE_TARGET_READ) {
        set_local_completion_context(task_params->context);
	} else if (task_type == ISCSI_TASK_TYPE_TARGET_WRITE) {
        task_params->context->ystorm_st_context.pdu_hdr.r2t.desired_data_trns_len =
             HWAL_CPU_TO_LE32(task_size + ((struct iscsi_r2t_hdr*)pdu_header)->buffer_offset);  /* buffer size = continuation size (including the immediate data length [if exists]) */
        task_params->context->mstorm_st_context.expected_itt = HWAL_CPU_TO_LE32(pdu_header->itt);
    } else { /* if (task_type == ISCSI_TASK_TYPE_INITIATOR_WRITE || task_type == ISCSI_TASK_TYPE_INITIATOR_READ) */
        task_params->context->ystorm_st_context.pdu_hdr.cmd.expected_transfer_length = HWAL_CPU_TO_LE32(task_size);
        init_initiator_rw_cdb_ystorm_context( &task_params->context->ystorm_st_context, cmd_params);
        task_params->context->mstorm_st_context.sense_db.lo = HWAL_CPU_TO_LE32(cmd_params->sense_data_buffer_phys_addr.lo); /* Mstorm buffer for sense data placement */
        task_params->context->mstorm_st_context.sense_db.hi = HWAL_CPU_TO_LE32(cmd_params->sense_data_buffer_phys_addr.hi); /* Mstorm buffer for sense data placement */
    }

    /* Tx/Rx: */
    if (task_params->tx_io_size) { /* if data to transmit: */
        init_dif_context_flags(&task_params->context->ystorm_st_context.state.dif_flags, dif_task_params);
		init_dif_context_flags(&task_params->context->ustorm_st_context.dif_flags, dif_task_params);
        init_scsi_sgl_context(&task_params->context->ystorm_st_context.state.sgl_params,
                                    &task_params->context->ystorm_st_context.state.data_desc,
                                     sgl_task_params);
        slow_io = scsi_is_slow_sgl(sgl_task_params->num_sges, sgl_task_params->small_mid_sge);
        num_sges = (u8)(!slow_io ? HWAL_MIN32(sgl_task_params->num_sges, SCSI_NUM_SGES_SLOW_SGL_THR) : ISCSI_WQE_NUM_SGES_SLOWIO);
        if (slow_io) {
            HWAL_SET_FIELD(task_params->context->ystorm_st_context.state.flags, YSTORM_ISCSI_TASK_STATE_SLOW_IO, 1);
        }
    } else if (task_params->rx_io_size) { /* if data to receive: */
        init_dif_context_flags(&task_params->context->mstorm_st_context.dif_flags, dif_task_params);
        init_scsi_sgl_context(&task_params->context->mstorm_st_context.sgl_params,
                                    &task_params->context->mstorm_st_context.data_desc,
                                    sgl_task_params);
        num_sges = (u8)(!scsi_is_slow_sgl(sgl_task_params->num_sges, sgl_task_params->small_mid_sge) ? 
                       HWAL_MIN32(sgl_task_params->num_sges, SCSI_NUM_SGES_SLOW_SGL_THR) : ISCSI_WQE_NUM_SGES_SLOWIO);
        task_params->context->mstorm_st_context.rem_task_size = HWAL_CPU_TO_LE32(task_size);
    }

    /* Ustorm context: */
    if (exp_data_transfer_len > task_size  || task_type != ISCSI_TASK_TYPE_TARGET_WRITE)
        exp_data_transfer_len = task_size; /* The size of the transmitted task*/
    init_ustorm_task_contexts(&task_params->context->ustorm_st_context,
                              &task_params->context->ustorm_ag_context,
                              task_size,                              /* Remaining Receive length is the Task Size */
                              exp_data_transfer_len,                  /* The size of the transmitted task          */
                              num_sges,                               /* num_sges                                  */
                              dif_task_params ? dif_task_params->tx_dif_conn_err_en : false);   /* tx_dif_conn_err_en                        */
    set_rw_exp_data_acked_and_cont_len(task_params->context,
                                       conn_params,
                                       task_type,
                                       task_size,
                                       exp_data_transfer_len,
                                       HWAL_GET_FIELD(pdu_header->hdr_second_dword, ISCSI_CMD_HDR_TOTAL_AHS_LEN));
    /* DIF context: */
    if (dif_task_params)
        init_rtdif_task_context(&task_params->context->rdif_context, &task_params->context->tdif_context, dif_task_params, task_type);

    init_sqe(task_params,
             sgl_task_params,
             dif_task_params,
             pdu_header,
             cmd_params,
             task_type,
             false);
    return 0;
}
                                                                          
int init_initiator_rw_iscsi_task(struct e4_iscsi_task_params         *task_params,
                                       struct iscsi_conn_params         *conn_params,
                                       struct scsi_initiator_cmd_params *cmd_params,
                                       struct iscsi_cmd_hdr             *cmd_pdu_header,
                                       struct scsi_sgl_task_params      *tx_sgl_task_params,
                                       struct scsi_sgl_task_params      *rx_sgl_task_params,
                                       struct scsi_dif_task_params      *dif_task_params)
{   /* Bi-directional IO not supported yet */
    if (HWAL_GET_FIELD(cmd_pdu_header->flags_attr, ISCSI_CMD_HDR_WRITE))
        return init_rw_iscsi_task(task_params,
                                  ISCSI_TASK_TYPE_INITIATOR_WRITE,
                                  conn_params,
                                  (struct iscsi_common_hdr*)cmd_pdu_header,
                                  tx_sgl_task_params,
                                  cmd_params,
                                  dif_task_params);
    else if (HWAL_GET_FIELD(cmd_pdu_header->flags_attr, ISCSI_CMD_HDR_READ) ||
            (task_params->rx_io_size == 0 && task_params->tx_io_size == 0))
        return init_rw_iscsi_task(task_params,
                                  ISCSI_TASK_TYPE_INITIATOR_READ,
                                  conn_params,
                                  (struct iscsi_common_hdr*)cmd_pdu_header,
                                  rx_sgl_task_params,
                                  cmd_params,
                                  dif_task_params);
    else 
        return -1; /* either write bit or read bit should be set */

}

#ifndef _INITIATOR_ONLY_ 

int init_target_read_iscsi_task(struct e4_iscsi_task_params       *task_params,
                                      struct iscsi_conn_params       *conn_params,
                                      struct iscsi_data_in_hdr       *data_in_pdu_header,
                                      struct scsi_sgl_task_params    *sgl_task_params,
                                      struct scsi_dif_task_params    *dif_task_params)
{
    return init_rw_iscsi_task(task_params,
                              ISCSI_TASK_TYPE_TARGET_READ,
                              conn_params,
                              (struct iscsi_common_hdr*)data_in_pdu_header,
                              sgl_task_params,
                              NULL,
                              dif_task_params);
}

int init_target_write_iscsi_task(struct e4_iscsi_task_params       *task_params,
                                       struct iscsi_conn_params       *conn_params,
                                       struct iscsi_r2t_hdr           *r2t_pdu_header,
                                       struct scsi_sgl_task_params    *sgl_task_params,
                                       struct scsi_dif_task_params    *dif_task_params)
{
    return init_rw_iscsi_task(task_params,
                              ISCSI_TASK_TYPE_TARGET_WRITE,
                              conn_params,
                              (struct iscsi_common_hdr*)r2t_pdu_header,
                              sgl_task_params,
                              NULL,
                              dif_task_params);
}

int init_target_tq_iscsi_task(struct iscsi_task_context    *task_context,
                                    struct scsi_sgl_task_params     *sgl_task_params,
                                    struct tqe_opaque               *tqe_opaque_params)
{
    return init_tq_iscsi_task(task_context,
                              sgl_task_params,
                              tqe_opaque_params);
}

#endif /* #ifndef _INITIATOR_ONLY_  */

/* The following function initializes Login task in initiator mode: */
int init_initiator_login_request_task(struct e4_iscsi_task_params    *task_params,
                                            struct iscsi_login_req_hdr  *login_req_pdu_header,
                                            struct scsi_sgl_task_params *tx_sgl_task_params,
                                            struct scsi_sgl_task_params *rx_sgl_task_params)
{
    init_default_iscsi_task(task_params, (struct data_hdr*)login_req_pdu_header, ISCSI_TASK_TYPE_MIDPATH);

    /* Ustorm Context: */
    init_ustorm_task_contexts(&task_params->context->ustorm_st_context,
                              &task_params->context->ustorm_ag_context,
                              task_params->rx_io_size ? rx_sgl_task_params->total_buffer_size : 0, /* Remaining Receive length is the Task Size */
                              task_params->tx_io_size ? tx_sgl_task_params->total_buffer_size : 0, /* The size of the transmitted task          */
                              0,                                                                   /* num_sges                                  */
                              0);                                                                  /* tx_dif_conn_err_en                        */
    /* SGL context: */
    if (task_params->tx_io_size)
        init_scsi_sgl_context(&task_params->context->ystorm_st_context.state.sgl_params,
                                    &task_params->context->ystorm_st_context.state.data_desc,
                                     tx_sgl_task_params);
    if (task_params->rx_io_size)
        init_scsi_sgl_context(&task_params->context->mstorm_st_context.sgl_params,
                                    &task_params->context->mstorm_st_context.data_desc,
                                     rx_sgl_task_params);

    task_params->context->mstorm_st_context.rem_task_size = HWAL_CPU_TO_LE32(task_params->rx_io_size ? rx_sgl_task_params->total_buffer_size : 0);

    init_sqe(task_params,
             tx_sgl_task_params,
             NULL,
             (struct iscsi_common_hdr*)login_req_pdu_header,
             NULL,
             ISCSI_TASK_TYPE_MIDPATH,
             false);
    return 0;
}

/* The following function initializes NOP out task context in Initiator Mode: */
int init_initiator_nop_out_task(struct e4_iscsi_task_params    *task_params,
                                      struct iscsi_nop_out_hdr    *nop_out_pdu_header,
                                      struct scsi_sgl_task_params *tx_sgl_task_params,
                                      struct scsi_sgl_task_params *rx_sgl_task_params)
{
    init_default_iscsi_task(task_params, (struct data_hdr*)nop_out_pdu_header, ISCSI_TASK_TYPE_MIDPATH);

    if (nop_out_pdu_header->itt == ISCSI_ITT_ALL_ONES)
        set_local_completion_context(task_params->context); /* Local Completion */

    /* SGL Context: */
    if (task_params->tx_io_size)
        init_scsi_sgl_context(&task_params->context->ystorm_st_context.state.sgl_params,
                                    &task_params->context->ystorm_st_context.state.data_desc,
                                     tx_sgl_task_params);
    if (task_params->rx_io_size)
        init_scsi_sgl_context(&task_params->context->mstorm_st_context.sgl_params,
                                    &task_params->context->mstorm_st_context.data_desc,
                                     rx_sgl_task_params);
    /* Ustorm Context: */
    init_ustorm_task_contexts(&task_params->context->ustorm_st_context,
                              &task_params->context->ustorm_ag_context,
                              task_params->rx_io_size ? rx_sgl_task_params->total_buffer_size : 0, /* Remaining Receive length is the Task Size */
                              task_params->tx_io_size ? tx_sgl_task_params->total_buffer_size : 0, /* The size of the transmitted task          */
                              0,                                                                   /* num_sges                                  */
                              0);                                                                  /* tx_dif_conn_err_en                        */
    
    task_params->context->mstorm_st_context.rem_task_size = HWAL_CPU_TO_LE32(task_params->rx_io_size ? rx_sgl_task_params->total_buffer_size : 0);

    init_sqe(task_params,
             tx_sgl_task_params,
             NULL,
             (struct iscsi_common_hdr*)nop_out_pdu_header,
             NULL,
             ISCSI_TASK_TYPE_MIDPATH,
             false);
    return 0;
}

/* The following function initializes Logout task context in Initiator Mode: */
int init_initiator_logout_request_task(struct e4_iscsi_task_params    *task_params,
                                             struct iscsi_logout_req_hdr *logout_pdu_header,
                                             struct scsi_sgl_task_params *tx_sgl_task_params,
                                             struct scsi_sgl_task_params *rx_sgl_task_params)
{
    init_default_iscsi_task(task_params, (struct data_hdr*)logout_pdu_header, ISCSI_TASK_TYPE_MIDPATH);

    /* SGL context: */
    if (task_params->tx_io_size)
        init_scsi_sgl_context(&task_params->context->ystorm_st_context.state.sgl_params,
                                    &task_params->context->ystorm_st_context.state.data_desc,
                                     tx_sgl_task_params);
    if (task_params->rx_io_size)
        init_scsi_sgl_context(&task_params->context->mstorm_st_context.sgl_params,
                                    &task_params->context->mstorm_st_context.data_desc,
                                     rx_sgl_task_params);
    /* Ustorm context: */
    init_ustorm_task_contexts(&task_params->context->ustorm_st_context,
                              &task_params->context->ustorm_ag_context,
                              task_params->rx_io_size ? rx_sgl_task_params->total_buffer_size : 0, /* Remaining Receive length is the Task Size */
                              task_params->tx_io_size ? tx_sgl_task_params->total_buffer_size : 0, /* The size of the transmitted task          */
                              0,                                                                   /* num_sges                                  */
                              0);                                                                  /* tx_dif_conn_err_en                        */
    
    task_params->context->mstorm_st_context.rem_task_size = HWAL_CPU_TO_LE32(task_params->rx_io_size ? rx_sgl_task_params->total_buffer_size : 0);

    init_sqe(task_params,
             tx_sgl_task_params,
             NULL,
             (struct iscsi_common_hdr*)logout_pdu_header,
             NULL,
             ISCSI_TASK_TYPE_MIDPATH,
             false);
    return 0;
}

/* The following function initializes TMF task context in Initiator Mode: */
int init_initiator_tmf_request_task(struct e4_iscsi_task_params     *task_params,
                                          struct iscsi_tmf_request_hdr *tmf_pdu_header)
{
    init_default_iscsi_task(task_params, (struct data_hdr*)tmf_pdu_header, ISCSI_TASK_TYPE_MIDPATH);

    init_sqe(task_params,
             NULL,
             NULL,
             (struct iscsi_common_hdr*)tmf_pdu_header,
             NULL,
             ISCSI_TASK_TYPE_MIDPATH,
             false);
    return 0;
}

/* The following function initializes Tesxt Request task context in Initiator Mode: */
int init_initiator_text_request_task(struct e4_iscsi_task_params      *task_params,
                                           struct iscsi_text_request_hdr *text_request_pdu_header,
                                           struct scsi_sgl_task_params   *tx_sgl_task_params,
                                           struct scsi_sgl_task_params   *rx_sgl_task_params)
{
    init_default_iscsi_task(task_params, (struct data_hdr*)text_request_pdu_header, ISCSI_TASK_TYPE_MIDPATH);

    /* SGL context: */
    if (task_params->tx_io_size)
        init_scsi_sgl_context(&task_params->context->ystorm_st_context.state.sgl_params,
                                    &task_params->context->ystorm_st_context.state.data_desc,
                                     tx_sgl_task_params);
    if (task_params->rx_io_size)
        init_scsi_sgl_context(&task_params->context->mstorm_st_context.sgl_params,
                                    &task_params->context->mstorm_st_context.data_desc,
                                     rx_sgl_task_params);

    task_params->context->mstorm_st_context.rem_task_size = HWAL_CPU_TO_LE32(task_params->rx_io_size ? rx_sgl_task_params->total_buffer_size : 0);

    /* Ustorm context: */
    init_ustorm_task_contexts(&task_params->context->ustorm_st_context,
                              &task_params->context->ustorm_ag_context,
                              task_params->rx_io_size ? rx_sgl_task_params->total_buffer_size : 0, /* Remaining Receive length is the Task Size */
                              task_params->tx_io_size ? tx_sgl_task_params->total_buffer_size : 0, /* The size of the transmitted task          */
                              0,                                                                   /* num_sges                                  */
                              0);                                                                  /* tx_dif_conn_err_en                        */
    init_sqe(task_params,
             tx_sgl_task_params,
             NULL,
             (struct iscsi_common_hdr*)text_request_pdu_header,
             NULL,
             ISCSI_TASK_TYPE_MIDPATH,
             false);
    return 0;
}

#ifndef _INITIATOR_ONLY_ 

/* The following function initializes Login Response task in target mode: */
int init_target_login_response_task(struct e4_iscsi_task_params        *task_params,
                                          struct iscsi_login_response_hdr *login_resp_pdu_header,
                                          struct scsi_sgl_task_params     *tx_sgl_task_params)
{
    init_default_iscsi_task(task_params, (struct data_hdr*)login_resp_pdu_header, ISCSI_TASK_TYPE_LOGIN_RESPONSE);

    /* Local Completion: */
    set_local_completion_context(task_params->context);

    /* Ustorm Context: */
    if (task_params->tx_io_size)
        init_scsi_sgl_context(&task_params->context->ystorm_st_context.state.sgl_params,
                                    &task_params->context->ystorm_st_context.state.data_desc,
                                     tx_sgl_task_params);

    init_ustorm_task_contexts(&task_params->context->ustorm_st_context,
                              &task_params->context->ustorm_ag_context,
                              0,                                                                   /* Remaining Receive length is the Task Size */
                              task_params->tx_io_size ? tx_sgl_task_params->total_buffer_size : 0, /* The size of the transmitted task          */
                              0,                                                                   /* num_sges                                  */
                              0);                                                                  /* tx_dif_conn_err_en                        */
    init_sqe(task_params,
             tx_sgl_task_params,
             NULL,
             (struct iscsi_common_hdr*)login_resp_pdu_header,
             NULL,
             ISCSI_TASK_TYPE_LOGIN_RESPONSE,
             false);
    return 0;
}

/* The following function initializes Response task context in Target Mode: */
int init_target_response_task(struct e4_iscsi_task_params     *task_params,
                                    struct iscsi_response_hdr    *resp_pdu_header,
                                    struct scsi_sgl_task_params  *tx_sgl_task_params)
{
    init_default_iscsi_task(task_params, (struct data_hdr*)resp_pdu_header, ISCSI_TASK_TYPE_TARGET_RESPONSE);

    /* Local Completion: */
    set_local_completion_context(task_params->context);

    /* Ustorm Context: */
    init_ustorm_task_contexts(&task_params->context->ustorm_st_context,
                              &task_params->context->ustorm_ag_context,
                              0,                                                                   /* Remaining Receive length is the Task Size */
                              task_params->tx_io_size ? tx_sgl_task_params->total_buffer_size : 0, /* The size of the transmitted task          */
                              0,                                                                   /* num_sges                                  */
                              0);                                                                  /* tx_dif_conn_err_en                        */

    if (task_params->tx_io_size)
        init_scsi_sgl_context(&task_params->context->ystorm_st_context.state.sgl_params,
                                    &task_params->context->ystorm_st_context.state.data_desc,
                                     tx_sgl_task_params);

    init_sqe(task_params,
             tx_sgl_task_params,
             NULL,
             (struct iscsi_common_hdr*)resp_pdu_header,
             NULL,
             ISCSI_TASK_TYPE_TARGET_RESPONSE,
             false);
    return 0;
}

/* The following function initializes TMF task context in Initiator Mode: */
int init_target_tmf_response_task(struct e4_iscsi_task_params      *task_params,
                                        struct iscsi_tmf_response_hdr *tmf_response_pdu_header)
{
    init_default_iscsi_task(task_params, (struct data_hdr*)tmf_response_pdu_header, ISCSI_TASK_TYPE_MIDPATH);

    /* Local Completion: */
    set_local_completion_context(task_params->context); /* Local Completion */

    init_sqe(task_params,
             NULL,
             NULL,
             (struct iscsi_common_hdr*)tmf_response_pdu_header,
             NULL,
             ISCSI_TASK_TYPE_MIDPATH,
             false);
    return 0;
}


/* The following function initializes Tesxt Response task context in Target Mode: */
int init_target_text_response_task(struct e4_iscsi_task_params       *task_params,
                                         struct iscsi_text_response_hdr *text_resp_pdu_header,
                                         struct scsi_sgl_task_params    *tx_sgl_task_params)
{
    init_default_iscsi_task(task_params, (struct data_hdr*)text_resp_pdu_header, ISCSI_TASK_TYPE_MIDPATH);

    /* Local Completion: */
    set_local_completion_context(task_params->context); /* Local Completion */

    /* Ustorm Context: */
    if (task_params->tx_io_size)
        init_scsi_sgl_context(&task_params->context->ystorm_st_context.state.sgl_params,
                                    &task_params->context->ystorm_st_context.state.data_desc,
                                     tx_sgl_task_params);

    init_ustorm_task_contexts(&task_params->context->ustorm_st_context,
                              &task_params->context->ustorm_ag_context,
                              0,                                                                   /* Remaining Receive length is the Task Size */
                              task_params->tx_io_size ? tx_sgl_task_params->total_buffer_size : 0, /* The size of the transmitted task          */
                              0,                                                                   /* num_sges                                  */
                              0);                                                                  /* tx_dif_conn_err_en                        */
    init_sqe(task_params,
             tx_sgl_task_params,
             NULL,
             (struct iscsi_common_hdr*)text_resp_pdu_header,
             NULL,
             ISCSI_TASK_TYPE_MIDPATH,
             false);
    return 0;
}

/* The following function initializes Async task context in Target Mode: */
int init_target_async_msg_task(struct e4_iscsi_task_params    *task_params,
                                     struct iscsi_async_msg_hdr  *async_msg_pdu_header,
                                     struct scsi_sgl_task_params *tx_sgl_task_params)
{
    init_default_iscsi_task(task_params, (struct data_hdr*)async_msg_pdu_header, ISCSI_TASK_TYPE_MIDPATH);

    /* Local Completion: */
    set_local_completion_context(task_params->context); /* Local Completion */

    /* Ustorm Context: */
    if (task_params->tx_io_size)
        init_scsi_sgl_context(&task_params->context->ystorm_st_context.state.sgl_params,
                                    &task_params->context->ystorm_st_context.state.data_desc,
                                     tx_sgl_task_params);

    init_ustorm_task_contexts(&task_params->context->ustorm_st_context,
                              &task_params->context->ustorm_ag_context,
                              0,                                                                   /* Remaining Receive length is the Task Size */
                              task_params->tx_io_size ? tx_sgl_task_params->total_buffer_size : 0, /* The size of the transmitted task          */
                              0,                                                                   /* num_sges                                  */
                              0);                                                                  /* tx_dif_conn_err_en                        */
    init_sqe(task_params,
             tx_sgl_task_params,
             NULL,
             (struct iscsi_common_hdr*)async_msg_pdu_header,
             NULL,
             ISCSI_TASK_TYPE_MIDPATH,
             false);
    return 0;
}

/* The following function initializes Async task context in Target Mode: */
int init_target_reject_task(struct e4_iscsi_task_params    *task_params,
                                  struct iscsi_reject_hdr     *reject_pdu_header,
                                  struct scsi_sgl_task_params *tx_sgl_task_params)
{
    init_default_iscsi_task(task_params, (struct data_hdr*)reject_pdu_header, ISCSI_TASK_TYPE_MIDPATH);

    /* Local Completion: */
    set_local_completion_context(task_params->context); /* Local Completion */

    /* Ustorm Context: */
    if (task_params->tx_io_size)
        init_scsi_sgl_context(&task_params->context->ystorm_st_context.state.sgl_params,
                                    &task_params->context->ystorm_st_context.state.data_desc,
                                     tx_sgl_task_params);

    init_ustorm_task_contexts(&task_params->context->ustorm_st_context,
                              &task_params->context->ustorm_ag_context,
                              0,                                                                   /* Remaining Receive length is the Task Size */
                              task_params->tx_io_size ? tx_sgl_task_params->total_buffer_size : 0, /* The size of the transmitted task          */
                              0,                                                                   /* num_sges                                  */
                              0);                                                                  /* tx_dif_conn_err_en                        */
    init_sqe(task_params,
             tx_sgl_task_params,
             NULL,
             (struct iscsi_common_hdr*)reject_pdu_header,
             NULL,
             ISCSI_TASK_TYPE_MIDPATH,
             false);
    return 0;
}

/* The following function initializes NOP in task context in Target Mode: */
int init_target_nop_in_task(struct e4_iscsi_task_params       *task_params,
                                  struct iscsi_nop_in_hdr        *nop_in_pdu_header,
                                  struct scsi_sgl_task_params    *tx_sgl_task_params)
{
    init_default_iscsi_task(task_params, (struct data_hdr*)nop_in_pdu_header, ISCSI_TASK_TYPE_MIDPATH);

    if (nop_in_pdu_header->ttt == ISCSI_TTT_ALL_ONES)
        set_local_completion_context(task_params->context); /* Local Completion if required */

    /* Ustorm Context: */
    if (task_params->tx_io_size)
        init_scsi_sgl_context(&task_params->context->ystorm_st_context.state.sgl_params,
                                    &task_params->context->ystorm_st_context.state.data_desc,
                                     tx_sgl_task_params);

    init_ustorm_task_contexts(&task_params->context->ustorm_st_context,
                              &task_params->context->ustorm_ag_context,
                              0,                                                                   /* Remaining Receive length is the Task Size */
                              task_params->tx_io_size ? tx_sgl_task_params->total_buffer_size : 0, /* The size of the transmitted task          */
                              0,                                                                   /* num_sges                                  */
                              0);                                                                  /* tx_dif_conn_err_en                        */
    init_sqe(task_params,
             tx_sgl_task_params,
             NULL,
             (struct iscsi_common_hdr*)nop_in_pdu_header,
             NULL,
             ISCSI_TASK_TYPE_MIDPATH,
             false);
    return 0;
}

/* The following function initializes Log out response task context in Target Mode: */
int init_target_logout_response_task(struct e4_iscsi_task_params         *task_params,
                                           struct iscsi_logout_response_hdr *logout_pdu_header,
                                           struct scsi_sgl_task_params      *tx_sgl_task_params)
{
    init_default_iscsi_task(task_params, (struct data_hdr*)logout_pdu_header, ISCSI_TASK_TYPE_MIDPATH);

    /* Local Completion: */
    set_local_completion_context(task_params->context);

    /* Ustorm Context: */
    if (task_params->tx_io_size)
        init_scsi_sgl_context(&task_params->context->ystorm_st_context.state.sgl_params,
                                    &task_params->context->ystorm_st_context.state.data_desc,
                                     tx_sgl_task_params);

    init_ustorm_task_contexts(&task_params->context->ustorm_st_context,
                              &task_params->context->ustorm_ag_context,
                              0,                                                                   /* Remaining Receive length is the Task Size */
                              task_params->tx_io_size ? tx_sgl_task_params->total_buffer_size : 0, /* The size of the transmitted task          */
                              0,                                                                   /* num_sges                                  */
                              0);                                                                  /* tx_dif_conn_err_en                        */
    init_sqe(task_params,
             tx_sgl_task_params,
             NULL,
             (struct iscsi_common_hdr*)logout_pdu_header,
             NULL,
             ISCSI_TASK_TYPE_MIDPATH,
             false);
    return 0;
}

#endif /* #ifndef _INITIATOR_ONLY_  */

int init_cleanup_task(struct e4_iscsi_task_params *task_params)
{
    init_sqe(task_params,
             NULL,
             NULL,
             NULL,
             NULL,
		     ISCSI_TASK_TYPE_MIDPATH,
             true);
    return 0;
}
/* CHECKPATCH - Run and add trigger for perforce pre-submit - best run on a linux machine for simplicity of usage */

