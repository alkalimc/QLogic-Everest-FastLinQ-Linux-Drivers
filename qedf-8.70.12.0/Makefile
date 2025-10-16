DEBUG := debug
EXTRA_CFLAGS+= -g

sles_distro := $(wildcard /etc/SuSE-release)
rhel_distro := $(wildcard /etc/redhat-release)
ubuntu_distro := $(shell lsb_release -is 2> /dev/null | grep Ubuntu)

KVER := $(shell uname -r)
INC_DIR := /lib/modules/$(KVER)/build/include

# Check for a standard header, say scsi.h, in the usual location.
# Lack of which usually means a SLES machine, which does not have
# headers like other distros; this way an upstream compile would
# work on a SLES machine.
ifeq ($(wildcard $(INC_DIR)/scsi/scsi.h),) # very likely SLES
	ifeq ($(sles_distro),)
		_KVER=$(shell echo $(KVER) | cut -d "-" -f1,2)
		INC_DIR := /usr/src/linux-$(_KVER)/include
	endif
	ifeq ($(ubuntu_distro),)
		INC_DIR := /lib/modules/$(KVER)/source/include
	endif
endif

#
# set-def:-
# $(call set-def,<define>,<include-file>,<regex-pattern>)
# 	- returns <define> if pattern is in include file.
#
# pattern should have word boundaries (-w option) and should not have
# embedded space (use \s instead).
#
define set-def
$(shell grep -qsw "$(strip $3)" \
        $(INC_DIR)/$(strip $2) && echo "$(strip $1)")
endef

DEFINES += $(call set-def,DEFINE_FIP_VLAN_DESC,scsi/fc/fc_fip.h,fip_vlan_desc)
DEFINES += $(call set-def,DEFINE_FIP_SC_VL_REP,scsi/fc/fc_fip.h,FIP_SC_VL_REP)
DEFINES += $(call set-def,ETHER_ADDR_COPY,linux/etherdevice.h,ether_addr_copy)
DEFINES += $(call set-def,ETHER_ADDR_EQUAL,linux/etherdevice.h,ether_addr_equal)
DEFINES += $(call set-def,ETH_ZERO_ADDR,linux/etherdevice.h,eth_zero_addr)
DEFINES += $(call set-def,KMALLOC_ARRAY,linux/slab.h,kmalloc_array)
DEFINES += $(call set-def,FC_DISC_CONFIG,scsi/libfc.h,fc_disc_config)
DEFINES += $(call set-def,KREF_READ,linux/kref.h,kref_read)
DEFINES += $(call set-def,FC_RPORT_LOGIN,scsi/libfc.h,fc_rport_login)
DEFINES += $(call set-def,FC_RPORT_LOGOFF,scsi/libfc.h,fc_rport_logoff)
DEFINES += $(call set-def,FC_RPORT_CREATE,scsi/libfc.h,fc_rport_create)
DEFINES += $(call set-def,FC_RPORT_DESTROY,scsi/libfc.h,fc_rport_destroy)
DEFINES += $(call set-def,FC_RPORT_LOOKUP,scsi/libfc.h,fc_rport_lookup)
DEFINES += $(call set-def,FC_RPORT_INIT,scsi/libfc.h,fc_rport_init)
DEFINES += $(call set-def,KREF_GET_UNLESS_ZERO,linux/kref.h,kref_get_unless_zero)
DEFINES += $(call set-def,NR_HW_QUEUES,scsi/scsi_host.h,nr_hw_queues)
DEFINES += $(call set-def,USE_BLK_MQ,scsi/scsi_host.h,shost_use_blk_mq)
DEFINES += $(call set-def,_DEFINE_CRC8,include/linux/crc8.h,crc8)
DEFINES += $(call set-def,ENABLE_CLUSTER,scsi/scsi_host.h,use_clustering)
DEFINES += $(call set-def,_HAS_MMIOWB_SPIN_LOCK,asm-generic/mmiowb.h,mmiowb_spin_lock)
DEFINES += $(call set-def,BLK_DEV_SPECIAL,linux/blkdev.h,*special)
DEFINES += $(call set-def,SKB_FRAG,linux/skbuff.h,skb_frag_struct)
DEFINES += $(call set-def,CLEAR_NONFATAL_STATUS,linux/aer.h,pci_aer_clear_nonfatal_status)
DEFINES += $(call set-def,FDMI_VERSION,include/scsi/fc/fc_ms.h,FDMI_V2)

ifeq ($(wildcard $(INC_DIR)/scsi/fc_encode.h),)
DEFINES += INCLUDE_FC_ENCODE
endif

ifeq ($(shell ls $(INC_DIR)/linux/hashtable.h > /dev/null 2>&1 && echo hashtable),)
	DEFINES	+= _DEFINE_HASHTABLE
else
ifneq ($(shell grep "hash_for_each_possible" $(INC_DIR)/linux/hashtable.h | grep "node" > /dev/null 2>&1 && echo hash_for_each_possible_v1),)
	DEFINES += _HAS_HASH_FOR_EACH_POSSIBLE_V1
endif
endif
ifeq ($(shell test `find /lib/modules/$(KVER) -name crc8.ko` > /dev/null 2>&1 && echo crc8.ko),)
	DEFINES += _MISSING_CRC8_MODULE
endif

# defines so driver compiles when including qed_compat.h which is needed for qed.h
DEFINES += _HAS_ETH_RANDOM_ADDR _HAS_TIME_TO_TM __PREVENT_PXP_GLOBAL_WIN__

# Get SLES version if available
ifneq ($(sles_distro),)
SLES_VERSION = $(shell cat /etc/SuSE-release | grep VERSION | grep -o -P [0-9]+)
SLES_PATCHLEVEL = $(shell cat /etc/SuSE-release | grep PATCHLEVEL | grep -o -P [0-9]+)
PADDED_PATCHLEVEL = $(shell if [ 10 -gt $(SLES_PATCHLEVEL) ]; then echo 0$(SLES_PATCHLEVEL); else echo $(SLES_PATCHLEVEL); fi)
SLES_DISTRO_VER = "0x$(SLES_VERSION)$(PADDED_PATCHLEVEL)"
override EXTRA_CFLAGS += -DSLES_DISTRO=$(SLES_DISTRO_VER)
endif

# Get RHEL version if available
ifneq ($(rhel_distro),)
RHEL_MAJVER := $(shell grep "RHEL_MAJOR" /usr/include/linux/version.h | sed -e 's/.*MAJOR \([0-9]\)/\1/')
RHEL_MINVER := $(shell grep "MINOR" /usr/include/linux/version.h | sed -e 's/.*MINOR \([0-9]\)/\1/')
RHEL_DISTRO_VER = "0x$(MAJVER)$(MINVER)"
endif

# Checks to see if we should use thunk-extern
ifeq ($(RHEL_DISTRO_VER), "0x0610")
  USE_THUNK_EXTERN = 1
endif

ifeq ($(RHEL_MAJVER), 7)
  ifneq ($(shell test $(RHEL_MINVER) -gt 4 && echo thunk_extern),)
    USE_THUNK_EXTERN = 1
  endif
endif

ifeq ($(RHEL_MAJVER), 8)
	USE_THUNK_EXTERN = 1
endif

ifeq ($(RHEL_MAJVER), 9)
	USE_THUNK_EXTERN = 1
endif

SUSE_BRAND = $(shell cat /etc/SUSE-brand 2>/dev/null | grep VERSION | sed 's/VERSION = //')
SUSE_PATCHLEVEL = $(shell cat /etc/SuSE-release 2>/dev/null | grep PATCHLEVEL | sed 's/PATCHLEVEL = //')
ifeq ($(SUSE_BRAND), 12)
  ifneq ($(shell test $(SUSE_PATCHLEVEL) -gt 3 && echo thunk_extern),)
    USE_THUNK_EXTERN = 1
  endif
endif
ifeq ($(SUSE_BRAND), 15)
  USE_THUNK_EXTERN = 1
endif

ifeq ($(USE_THUNK_EXTERN),1)
  THUNK_OPTIONS = -mindirect-branch=thunk-extern -mindirect-branch-register
else
  THUNK_OPTIONS = -mindirect-branch=thunk-inline -mindirect-branch-register -DTHUNK_INLINE
endif

ifneq ($(shell echo 'int main(){return 0;}' | gcc -x c $(THUNK_OPTIONS) - 2>/dev/null && echo thunk),)
  override EXTRA_CFLAGS += $(THUNK_OPTIONS) 
endif

override EXTRA_CFLAGS += $(addprefix -D,$(DEFINES))

# Addition defines via command line, call: make EXTRA_DEFINES=XYZ
override EXTRA_CFLAGS += $(addprefix -D,$(EXTRA_DEFINES))

ifneq ($(debug),)
$(warning EXTRA_CFLAGS=($(EXTRA_CFLAGS)))
endif

obj-m := qedf.o
qedf-objs = qedf_dbg.o qedf_main.o qedf_io.o qedf_fip.o \
	    qedf_attr.o qedf_els.o qedf_debugfs.o \
	    drv_scsi_fw_funcs.o drv_e4_fcoe_fw_funcs.o

KVER_DIR=/lib/modules/$(KVER)
current_modules := $(shell grep -s ^INITRD_MODULES /etc/sysconfig/kernel | awk -F'"' '$$0=$$2')

PREFIX=
ifneq ($(rhel_distro),)
DISTRO_DRV_DIR := extra
else
DISTRO_DRV_DIR := updates
endif
DRV_DIR := $(DISTRO_DRV_DIR)/qlgc-fastlinq

ifeq ($(ERROR_INJECT), 1)
override EXTRA_CFLAGS += -DERROR_INJECT
endif

NPROCS:=$(shell grep -c ^processor /proc/cpuinfo)

.PHONY: build_pre sparse sparse_endian smatch modcopy modules_install install

KBUILD_EXTRA_SYMBOLS := $(QED_DIR)/Module.symvers
QED_FILES := Module.symvers qed_if.h qed_fcoe_if.h qed_ll2_if.h qed_chain.h qed_hsi.h tcp_common.h eth_common.h iscsi_common.h iwarp_common.h rdma_common.h roce_common.h qed_iscsi_if.h qed_compat.h qed_eth_if.h qed_iov_if.h qed_reg_addr.h qed_mcp.h qed_int.h qed_dev_api.h qed_hw.h qed_dcbx.h qed.h qed_debug.h storage_overtcp_common.h qed_dbg_hsi.h qed_mfw_hsi.h

build: build_pre
	make -C $(KVER_DIR)/build -j$(NPROCS) M=`pwd` modules

$(QED_FILES): $(addprefix ../qed-8.70.12.0/src/,$(QED_FILES))
	cp -f ../qed-8.70.12.0/src/$@ .

build_pre: $(QED_FILES)

sparse: build_pre
	if which sparse 1>/dev/null 2>&1; then\
		make -C $(KVER_DIR)/build C=2 M=`pwd` modules;\
	fi

sparse_endian: build_pre
	if which sparse 1>/dev/null 2>&1; then\
		make -C $(KVER_DIR)/build C=2 CF="-D_CHECK_ENDIAN_" M=`pwd` modules;\
	fi

smatch: build_pre
	if which smatch 1>/dev/null 2>&1; then\
		make -C $(KVER_DIR)/build CHECK="/root/bin/smatch -p=kernel" C=2 M=`pwd` modules;\
	fi
	
clean:
	rm -f Module.symvers
	rm -f modules.order
	rm -f ../storcmn/\.*.cmd ../storcmn/*.o
	rm -f $(QED_FILES)
	make -C $(KVER_DIR)/build M=`pwd` clean

modcopy: build
	make -C $(KVER_DIR)/build M=`pwd` INSTALL_MOD_PATH=$(PREFIX) INSTALL_MOD_DIR=$(DRV_DIR) modules_install
	@if [ "$(rhel_distro)" != "" ]; then \
		echo "override qedf * $(DRV_DIR)" > qedf.conf ;\
		install -m 644 -D qedf.conf $(PREFIX)/etc/depmod.d/qedf.conf ;\
	fi
	@if [ "$(PREFIX)" = "" ] ;\
		then /sbin/depmod -a ;\
		else echo " *** Run '/sbin/depmod -a' to update the module database." ;\
	fi

modules_install: modcopy
	@if [ -f /boot/initramfs-$(KVER).img ]; then\
		echo "back up initrd to /boot/initramfs-$(KVER).img.bak" ;\
		cp /boot/initramfs-$(KVER).img /boot/initramfs-$(KVER).img.bak ;\
	elif [ -f /boot/initrd-$(KVER) ]; then\
		echo "back up initrd to /boot/initrd-$(KVER).bak";\
		cp /boot/initrd-$(KVER) /boot/initrd-$(KVER).bak;\
	elif [ -f /boot/initrd.img-$(KVER) ]; then\
		echo "back up initrd to /boot/initrd.img-$(KVER).bak";\
		cp /boot/initrd.img-$(KVER) /boot/initrd.img-$(KVER).bak;\
	fi
	@echo "add driver qedf to initrd"
	@if which dracut 1>/dev/null 2>&1 && [ -f "/boot/initramfs-$(KVER).img" ]; then\
		dracut -f --add-drivers qedf;\
	elif [ -f /boot/initrd.img-$(KVER) ] && [ -f /usr/sbin/update-initramfs ]; then\
		echo "updating initrd" ;\
		update-initramfs -u;\
	else\
		if [ -e /etc/sysconfig/kernel ];\
		then\
		        if [ `echo $(current_modules) | grep -c qedf` -eq 0 ];\
			then\
				echo "need to add qedf to /sys/config/kernel";\
				sed -i "s/^INITRD_MODULES=\"${current_modules}\"/INITRD_MODULES=\"${current_modules} qedf\"/" /etc/sysconfig/kernel;\
		        fi;\
		fi;\
		mkinitrd -m qedf;\
	fi

install: modules_install
