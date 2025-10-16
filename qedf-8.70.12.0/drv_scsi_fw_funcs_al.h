/*
 *  QLogic FCoE Offload Driver
 *  Copyright (c) 2015-2018 Cavium Inc.
 *
 *  See LICENSE.qedf for copyright and licensing details.
 */
#ifndef _DRV_FCOE_FW_FUNCS_AL_ 
#define _DRV_FCOE_FW_FUNCS_AL_ 

#include <linux/types.h>
#include <asm/byteorder.h>
#include "common_hsi.h"
#include "storage_common.h"
#include "fcoe_common.h"
#include "qedf_hsi.h"
#include "qed_if.h"

#define HWAL_CPU_TO_LE16		cpu_to_le16
#define HWAL_CPU_TO_LE32		cpu_to_le32
#define HWAL_CPU_TO_LE64		cpu_to_le64

#define	HWAL_MEMSET		memset
#define HWAL_MEMCPY		memcpy
#define HWAL_SET_FIELD		SET_FIELD

#endif
