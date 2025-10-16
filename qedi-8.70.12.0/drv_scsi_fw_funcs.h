#ifndef _SCSI_FW_FUNCS_H
#define _SCSI_FW_FUNCS_H
#include "drv_scsi_fw_funcs_al.h"
/*******************************************************************************
 * File name : scsi_init.h
 * Author    : Asaf Ravid
 *******************************************************************************
 *******************************************************************************
 * Description: 
 * SCSI HSI functions header
 *
 *******************************************************************************
 * Notes: This is the input to the auto generated file drv_scsi_fw_funcs.h
 * 
 *******************************************************************************/

#include "drv_scsi_fw_funcs_al.h"

struct scsi_sgl_task_params { /* scsi sgl task parameters context */
    struct scsi_sge *sgl;                  /* SGL descriptor(SGEs 0..[num_sges-1])                                                        */
    struct regpair   sgl_phys_addr;        /* SGL physical address                                                                        */
           u32       total_buffer_size;    /* SGL total size With DIF (if exists)                                                         */
           u16       num_sges;             /* Number of SGEs in SGL (Maximal size is 255)                                                 */
           bool      small_mid_sge;        /* true if SGL contains a small (< 4KB) SGE in middle(not 1st or last) -> relevant for tx only */
};

struct scsi_dif_task_params { /* scsi DIF task parameters context */
    u32  initial_ref_tag;
    bool initial_ref_tag_is_valid;
    u16  application_tag;
    u16  application_tag_mask; /* Mask is in bits      */
    u16  dif_block_size_log;   /* Log(2) Size in Bytes */
    bool dif_on_network;
    bool dif_on_host;
    u8   host_guard_type;     /* 0 = IP checksum, 1 = CRC                                                                                                                                                                                                                           */
    u8   protection_type;     /* 1/2/3 - Protection Type                                                                                                                                                                                                                            */
    u8   ref_tag_mask;        /* Mask is in bytes – bit[i] = byte[i]                                                                                                                                                                                                                */
    bool crc_seed;	          /* 1 – all ones seed, 0 – all-zeros seed 0=0x0000, 1=0xffff                                                                                                                                                                                           */
    bool tx_dif_conn_err_en;  /* Enable Connection error upon DIF error (segments with DIF errors are dropped)                                                                                                                                                                      */
    bool ignore_app_tag;      /* Ignore application tag for guard. false = don’t ignore (i.e. disable protection information check when application tag is 0xFFFF + other conditions), true = ignore (i.e. enable protection information check regardless of application tag value).*/
    bool keep_ref_tag_const;
    /* Validate: */
    bool validate_guard;
    bool validate_app_tag;
    bool validate_ref_tag;
    /* Forward without validate: */
    bool forward_guard;
    bool forward_app_tag;
    bool forward_ref_tag;
    bool forward_app_tag_with_mask;
    bool forward_ref_tag_with_mask;
};

struct scsi_initiator_cmd_params {
    struct scsi_sge extended_cdb_sge;            /* for cdb_size >  default CDB size (extended CDB > 16 bytes) -> pointer to the CDB buffer SGE */
    struct regpair  sense_data_buffer_phys_addr; /* Physical address of sense data buffer for sense data - 256B buffer                          */
};

/**
* @brief scsi_is_slow_sgl - checks for slow SGL 
*
*
* @param num_sges	     - number of sges in SGL
* @param small_mid_sge	 - True is the SGL contains an SGE which is smaller than 4KB and its not the 1st or last SGE in the SGL
*/
bool scsi_is_slow_sgl(u16 num_sges, bool small_mid_sge);

/**
* @brief init_scsi_sgl_context - initializes SGL task context
*
*
* @param sgl_params	     - SGL context parameters to initialize        (output parameter)
* @param data_desc	     - context struct containing SGEs array to set (output parameter)
* @param sgl_task_params - SGL parameters (input)
*/
void init_scsi_sgl_context(struct scsi_sgl_params      *sgl_params,
                                 struct scsi_cached_sges     *ctx_data_desc,
                                 struct scsi_sgl_task_params *sgl_task_params);
#endif
