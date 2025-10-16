#ifndef _E4_ISCSI_FW_FUNCS_H
#define _E4_ISCSI_FW_FUNCS_H
/*******************************************************************************
 * File name : iscsi_init.h
 * Author    : Asaf Ravid
 *******************************************************************************
 *******************************************************************************
 * Description: 
 * ISCSI HSI functions header
 *
 *******************************************************************************
 * Notes: This is the input to the auto generated file drv_iscsi_fw_funcs.h
 * 
 *******************************************************************************/

#include "drv_scsi_fw_funcs.h"
#include "drv_scsi_fw_funcs_al.h"


 /* Offset to scsi_sge - SGL descriptor(SGEs 0..[num_sges-1]) */
#define E4_ISCSI_TQ_SGES_OFFSET      offsetof(iscsi_task_context, mstorm_st_context) +      offsetof(mstorm_iscsi_task_st_ctx, data_desc)

 /* Offset to TQe opaque - tqe_opaque field */
#define E4_ISCSI_TQ_TQE_OPAQUE_OFFSET      offsetof(iscsi_task_context, ustorm_st_context) +      offsetof(ustorm_iscsi_task_st_ctx, tqe_opaque_list)

 /* Offset to SGL address - uint64 field */
#define E4_ISCSI_TQ_SGL_ADDR_OFFSET      offsetof(iscsi_task_context, mstorm_st_context) +      offsetof(mstorm_iscsi_task_st_ctx, sgl_params) +      offsetof(scsi_sgl_params, sgl_addr)

struct iscsi_conn_params {
    u32  first_burst_length;
    u32  max_send_pdu_length;
    u32  max_burst_length;
    bool initial_r2t;
    bool immediate_data;
};

struct e4_iscsi_task_params {
    struct iscsi_task_context *context; /* Output parameter [set/filled by the HSI function] */
    struct iscsi_wqe          *sqe;     /* Output parameter [set/filled by the HSI function] */
           u32                 tx_io_size;    /* in bytes (Without DIF, if exists) */
           u32                 rx_io_size;    /* in bytes (Without DIF, if exists) */
           u16                 conn_icid;
           u16                 itid;
           u8                  cq_rss_number;
};


/******************************/
/* Initiator Fast-path Tasks: */
/******************************/

/**
* @brief init_initiator_read_iscsi_task - initializes iSCSI Initiator Read task context
*
*
* @param task_params	 - Pointer to task parameters struct
* @param conn_params	 - Connection Parameters
* @param cmd_params	     - command specific parameters
* @param cmd_pdu_header	 - PDU Header Parameters
* @param sgl_task_params - Pointer to SGL task params
* @param dif_task_params - Pointer to DIF parameters struct
*/
int init_initiator_rw_iscsi_task(struct e4_iscsi_task_params         *task_params,
                                       struct iscsi_conn_params         *conn_params,
                                       struct scsi_initiator_cmd_params *cmd_params,
                                       struct iscsi_cmd_hdr             *cmd_pdu_header,
                                       struct scsi_sgl_task_params      *tx_sgl_task_params,
                                       struct scsi_sgl_task_params      *rx_sgl_task_params,
                                       struct scsi_dif_task_params      *dif_task_params);
#ifndef _INITIATOR_ONLY_ 

    /***************************/
    /* Target Fast-path Tasks: */
    /***************************/
    
    /**
    * @brief init_target_read_iscsi_task - initializes iSCSI Target Read task context
    *
    *
    * @param task_params		- Pointer to task parameters struct
    * @param conn_params		- Connection Parameters
    * @param data_in_pdu_header - PDU Header Parameters
    * @param sgl_task_params	- Pointer to SGL task params
    * @param dif_task_params	- Pointer to DIF parameters struct
    */
    int init_target_read_iscsi_task(struct e4_iscsi_task_params    *task_params,
                                          struct iscsi_conn_params    *conn_params,
                                          struct iscsi_data_in_hdr    *data_in_pdu_header,
                                          struct scsi_sgl_task_params *sgl_task_params,                              
                                          struct scsi_dif_task_params *dif_task_params);
    /**
    * @brief init_target_write_iscsi_task - initializes iSCSI Target Write task context
    *
    *
    * @param task_params	 - Pointer to task parameters struct
    * @param conn_params	 - Connection Parameters
    * @param r2t_pdu_header  - PDU Header Parameters
    * @param sgl_task_params - Pointer to SGL task params
    * @param dif_task_params - Pointer to DIF parameters struct
    */
    int init_target_write_iscsi_task(struct e4_iscsi_task_params    *task_params,
                                           struct iscsi_conn_params    *conn_params,
                                           struct iscsi_r2t_hdr        *r2t_pdu_header,
                                           struct scsi_sgl_task_params *sgl_task_params,                              
                                           struct scsi_dif_task_params *dif_task_params);

    /**
    * @brief init_target_tq_iscsi_task - initializes iSCSI Target TQ task context
    *
    *
    * @param task_context	   - Pointer to task context struct
    * @param sgl_task_params   - Pointer to SGL task params
    * @param tqe_opaque_params - Pointer to TQe opaque values
    */
    int init_target_tq_iscsi_task(struct iscsi_task_context    *task_context,
                                        struct scsi_sgl_task_params     *sgl_task_params,
                                        struct tqe_opaque               *tqe_opaque_params);

#endif /* #ifndef _INITIATOR_ONLY_  */

/********************************/
/* Initiator Middle-path Tasks: */
/********************************/

/**
* @brief init_initiator_login_request_task - initializes iSCSI Initiator Login Request task context
*
*
* @param task_params		  - Pointer to task parameters struct
* @param login_req_pdu_header - PDU Header Parameters
* @param tx_sgl_task_params	  - Pointer to SGL task params
* @param rx_sgl_task_params	  - Pointer to SGL task params
*/
int init_initiator_login_request_task(struct e4_iscsi_task_params    *task_params,
                                            struct iscsi_login_req_hdr  *login_req_pdu_header,
                                            struct scsi_sgl_task_params *tx_sgl_task_params,
                                            struct scsi_sgl_task_params *rx_sgl_task_params);
/**
* @brief init_initiator_nop_out_task - initializes iSCSI Initiator NOP Out task context
*
*
* @param task_params		- Pointer to task parameters struct
* @param nop_out_pdu_header - PDU Header Parameters
* @param tx_sgl_task_params	- Pointer to SGL task params
* @param rx_sgl_task_params	- Pointer to SGL task params
*/
int init_initiator_nop_out_task(struct e4_iscsi_task_params    *task_params,
                                      struct iscsi_nop_out_hdr    *nop_out_pdu_header,
                                      struct scsi_sgl_task_params *tx_sgl_task_params,
                                      struct scsi_sgl_task_params *rx_sgl_task_params);
/**
* @brief init_initiator_logout_request_task - initializes iSCSI Initiator Logout Request task context
*
*
* @param task_params		- Pointer to task parameters struct
* @param logout_pdu_header  - PDU Header Parameters
* @param tx_sgl_task_params	- Pointer to SGL task params
* @param rx_sgl_task_params	- Pointer to SGL task params
*/
int init_initiator_logout_request_task(struct e4_iscsi_task_params    *task_params,
                                             struct iscsi_logout_req_hdr *logout_pdu_header,
                                             struct scsi_sgl_task_params *tx_sgl_task_params,
                                             struct scsi_sgl_task_params *rx_sgl_task_params);
/**
* @brief init_initiator_tmf_request_task - initializes iSCSI Initiator TMF task context
*
*
* @param task_params	- Pointer to task parameters struct
* @param tmf_pdu_header - PDU Header Parameters
*/
int init_initiator_tmf_request_task(struct e4_iscsi_task_params     *task_params,
                                          struct iscsi_tmf_request_hdr *tmf_pdu_header);
/**
* @brief init_initiator_text_request_task - initializes iSCSI Initiator Text Request task context
*
*
* @param task_params		     - Pointer to task parameters struct
* @param text_request_pdu_header - PDU Header Parameters
* @param tx_sgl_task_params	     - Pointer to Tx SGL task params
* @param rx_sgl_task_params	     - Pointer to Rx SGL task params
*/
int init_initiator_text_request_task(struct e4_iscsi_task_params      *task_params,
                                           struct iscsi_text_request_hdr *text_request_pdu_header,
                                           struct scsi_sgl_task_params   *tx_sgl_task_params,
                                           struct scsi_sgl_task_params   *rx_sgl_task_params);
#ifndef _INITIATOR_ONLY_ 

    /*****************************/
    /* Target Middle-path Tasks: */
    /*****************************/
    
    /**
    * @brief init_target_login_response_task - initializes iSCSI Target Login Response task context
    *
    *
    * @param task_params		   - Pointer to task parameters struct
    * @param login_resp_pdu_header - PDU Header Parameters
    * @param sgl_task_params	   - Pointer to SGL task params
    */
    int init_target_login_response_task(struct e4_iscsi_task_params        *task_params,
                                              struct iscsi_login_response_hdr *login_resp_pdu_header,
    								    	  struct scsi_sgl_task_params     *tx_sgl_task_params);
    /**                                 
    * @brief init_target_response_task - initializes iSCSI Target Response task context
    *
    *
    * @param task_params		- Pointer to task parameters struct
    * @param resp_pdu_header    - PDU Header Parameters
    * @param tx_sgl_task_params - Pointer to SGL task params
    */
    int init_target_response_task(struct e4_iscsi_task_params    *task_params,
                                        struct iscsi_response_hdr   *resp_pdu_header,
                                        struct scsi_sgl_task_params *tx_sgl_task_params);    
    /**
    * @brief init_target_tmf_response_task - initializes iSCSI Target TMF response task context
    *
    *
    * @param task_params			 - Pointer to task parameters struct
    * @param tmf_response_pdu_header - PDU Header Parameters
    */
    int init_target_tmf_response_task(struct e4_iscsi_task_params      *task_params,
                                            struct iscsi_tmf_response_hdr *tmf_response_pdu_header);
    /**
    * @brief init_target_text_response_task - initializes iSCSI Target Text Response task context
    *
    *
    * @param task_params		  - Pointer to task parameters struct
    * @param text_resp_pdu_header - PDU Header Parameters
    * @param tx_sgl_task_params	  - Pointer to SGL task params
    */
    int init_target_text_response_task(struct e4_iscsi_task_params       *task_params,
                                             struct iscsi_text_response_hdr *text_resp_pdu_header,
                                             struct scsi_sgl_task_params    *tx_sgl_task_params);    
    /**
    * @brief init_target_async_msg_task - initializes iSCSI Target Async task context
    *
    *
    * @param task_params		  - Pointer to task parameters struct
    * @param async_msg_pdu_header - PDU Header Parameters
    * @param tx_sgl_task_params	  - Pointer to SGL task params
    */
    int init_target_async_msg_task(struct e4_iscsi_task_params    *task_params,
                                         struct iscsi_async_msg_hdr  *async_msg_pdu_header,
                                         struct scsi_sgl_task_params *tx_sgl_task_params);
    /**
    * @brief init_target_reject_task - initializes iSCSI Target Reject task context
    *
    *
    * @param task_params		- Pointer to task parameters struct
    * @param reject_pdu_header  - PDU Header Parameters
    * @param tx_sgl_task_params	- Pointer to SGL task params
    */
    int init_target_reject_task(struct e4_iscsi_task_params    *task_params,
                                      struct iscsi_reject_hdr     *reject_pdu_header,
                                      struct scsi_sgl_task_params *tx_sgl_task_params);
    /**
    * @brief init_target_nop_in_task - initializes iSCSI Target NOP In task context
    *
    *
    * @param task_params		 - Pointer to task parameters struct
    * @param nop_in_pdu_header   - PDU Header Parameters
    * @param tx_sgl_task_params	 - Pointer to SGL task params
    */
    int init_target_nop_in_task(struct e4_iscsi_task_params    *task_params,
                                      struct iscsi_nop_in_hdr     *nop_in_pdu_header,
                                      struct scsi_sgl_task_params *tx_sgl_task_params);
    /**
    * @brief init_target_logout_response_task - initializes iSCSI Target Logout Response task context
    *
    *
    * @param task_params	   	             - Pointer to task parameters struct
    * @param target_logout_pdu_header_params - PDU Header Parameters
    * @param tx_sgl_task_params		         - Pointer to SGL task params
    */
    int init_target_logout_response_task(struct e4_iscsi_task_params         *task_params,
                                               struct iscsi_logout_response_hdr *logout_pdu_header,
                                               struct scsi_sgl_task_params      *tx_sgl_task_params);
#endif /* #ifndef _INITIATOR_ONLY_ */

/******************************/
/* General Middle-path Tasks: */
/******************************/

/**
* @brief init_cleanup_task - initializes Clean task (SQE)
*
*
* @param task_params - Pointer to task parameters struct
*/
int init_cleanup_task(struct e4_iscsi_task_params *task_params);
#endif
