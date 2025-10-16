#ifndef _DRV_ISCSI_FW_FUNCS_AL_ 
#define _DRV_ISCSI_FW_FUNCS_AL_ 

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/mm.h>

#include <linux/types.h>
#include <asm/byteorder.h>
#include "qedi_hsi.h"
#include "qed_if.h"
#include "iscsi_common.h"

#define HWAL_CPU_TO_LE16                cpu_to_le16
#define HWAL_CPU_TO_LE32                cpu_to_le32
#define HWAL_CPU_TO_LE64                cpu_to_le64

#define HWAL_MEMSET             memset
#define HWAL_MEMCPY             memcpy

#define MASK_FIELD(_name, _value) \
        ((_value) &= (_name ## _MASK))

#define FIELD_VALUE(_name, _value) \
        ((_value & _name ## _MASK) << _name ## _SHIFT)

#define GET_FIELD(value, name) \
        (((value) >> (name ## _SHIFT)) & name ## _MASK)

#define HWAL_GET_FIELD          GET_FIELD
#define HWAL_SET_FIELD          SET_FIELD
#define HWAL_ARRAY_SIZE		ARRAY_SIZE

#define _INITIATOR_ONLY_

#endif
