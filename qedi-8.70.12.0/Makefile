
# To enable Makefile debug pass cmd line arg to make as -
# make -C /lib/modules/2.6.32-358.el6.x86_64/build/ M=$PWD debug=1
DEBUG := debug
UPSTREAM := upstream
EXTRA_CFLAGS += -g

# Check if RHEL6.X os and set correct OS flag
BOOTDIR := /boot
K_VERSION := $(shell uname -r)

REDHAT := $(shell grep -so "Red Hat" /etc/system-release)
CENTOS := $(shell grep -so "CentOS" /etc/system-release)
rhel_distro := $(wildcard /etc/redhat-release)
ifeq ("$(CENTOS)", "CentOS")
REDHAT := Red Hat
endif
SLES := $(wildcard /etc/SuSE-release)

ifeq ($(SLES),)
    SLES := $(shell grep -so "SLES" /etc/os-release)
endif

CITRIX := $(shell grep -so "xenenterprise" /etc/redhat-release)
UBUNTU := $(shell lsb_release -is 2> /dev/null | grep Ubuntu)

KVER := $(shell uname -r)
INC_DIR := /lib/modules/$(KVER)/build/include
ifneq ($(SLES),)
	_KVER=$(shell echo $(KVER) | cut -d "-" -f1,2)
	INC_DIR := /usr/src/linux-$(_KVER)/include
endif

# Check if the base OS is SLES, and latest upstream kernel
# is cloned and installed
ifeq ($(wildcard $(INC_DIR)/scsi/scsi.h),)
	ifeq ($(SLES),)
		KVER := $(shell uname -r)
		INC_DIR := /lib/modules/$(KVER)/build/include
	endif
	ifeq ($(UBUNTU),)
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

DEFINES += $(call set-def,SESS_LOCK,scsi/libiscsi.h,back_lock)
DEFINES += $(call set-def,PRANDOM_API,linux/random.h,prandom_u32)
DEFINES += $(call set-def,PRANDOM_U32,linux/prandom.h,prandom_u32)
DEFINES += $(call set-def,STRUCT_SCSI_RSP,scsi/iscsi_proto.h,iscsi_scsi_rsp)
DEFINES += $(call set-def,STRUCT_SCSI_REQ,scsi/iscsi_proto.h,iscsi_scsi_req)
DEFINES += $(call set-def,STRUCT_LOGIN_REQ,scsi/iscsi_proto.h,iscsi_login_req)
DEFINES += $(call set-def,UMODE_T_USED,\
		scsi/scsi_transport_iscsi.h,umode_t.*attr_is_visible)
DEFINES += $(call set-def,VLAN_INSERT_TAG,linux/if_vlan.h,__vlan_insert_tag)
DEFINES += $(call set-def,ETHER_ADDR_COPY,linux/etherdevice.h,ether_addr_copy)
DEFINES += $(call set-def,CHANGE_QDEPTH,\
		scsi/scsi_host.h,scsi_change_queue_depth)
DEFINES += $(call set-def,NR_HW_QUEUES,scsi/scsi_host.h,nr_hw_queues)
DEFINES += $(call set-def,USE_BLK_MQ,scsi/scsi_host.h,shost_use_blk_mq)
DEFINES += $(call set-def,ISCSI_PARAM_BOOT,\
		scsi/iscsi_if.h,ISCSI_PARAM_BOOT_ROOT)
DEFINES += $(call set-def,USE_CPU_HP,linux/cpuhotplug.h,cpuhp_setup_state)
DEFINES += $(call set-def,KTHREAD_CREATE_ON_NODE,linux/kthread.h,kthread_create_on_node)
DEFINES += $(call set-def,REFCOUNT_READ,linux/refcount.h,refcount_read)
DEFINES += $(call set-def,ENABLE_CLUSTER,scsi/scsi_host.h,use_clustering)
DEFINES += $(call set-def,_HAS_MMIOWB_SPIN_LOCK,asm-generic/mmiowb.h,mmiowb_spin_lock)
DEFINES += $(call set-def,BLK_DEV_SPECIAL,linux/blkdev.h,*special)
DEFINES += $(call set-def,CLEAR_NONFATAL_STATUS,linux/aer.h,pci_aer_clear_nonfatal_status)
DEFINES += $(call set-def,UNBIND_CONN,scsi/scsi_transport_iscsi.h,unbind_conn)
DEFINES += $(call set-def,ISCSI_TASK_IS_COMPLETED,scsi/libiscsi.h,iscsi_task_is_completed)
DEFINES += $(call set-def,FRWD_LOCK,scsi/libiscsi.h,frwd_lock)
DEFINES += $(call set-def,IS_SHUTDOWN,scsi/libiscsi.h,is_shutdown)

ifeq ($(ERROR_INJECT), 1)
DEFINES += ERROR_INJECT
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

obj-m := qedi.o
qedi-objs = qedi_main.o qedi_iscsi.o qedi_fw.o qedi_sysfs.o \
	    qedi_dbg.o qedi_dfs.o qedi_debugfs.o \
	    drv_scsi_fw_funcs.o drv_e4_iscsi_fw_funcs.o

PREFIX=
ifneq ($(rhel_distro),)
DISTRO_DRV_DIR := extra
else
DISTRO_DRV_DIR := updates
endif
DRV_DIR := $(DISTRO_DRV_DIR)/qlgc-fastlinq

KERN_VER_DIR=/lib/modules/$(KVER)
NPROCS:=$(shell grep -c ^processor /proc/cpuinfo)
DRACUT:=$(shell which dracut 2>/dev/null)
UPDATE_INITRAMFS:=$(shell which update-initramfs 2>/dev/null)

.PHONY: build_pre sparse sparse_endian smatch modules_install install

KBUILD_EXTRA_SYMBOLS := $(QED_DIR)/Module.symvers
build: build_pre
	make -C $(KERN_VER_DIR)/build -j$(NPROCS) M=`pwd` modules

build_pre:
	cp -f ../qed-8.70.12.0/src/Module.symvers .
	cp -f ../qed-8.70.12.0/src/qed_if.h .
	cp -f ../qed-8.70.12.0/src/qed_iscsi_if.h .
	cp -f ../qed-8.70.12.0/src/qed_ll2_if.h .
	cp -f ../qed-8.70.12.0/src/qed_chain.h .

sparse: build_pre
	if which sparse 1>/dev/null 2>&1; then\
		make -C $(KERN_VER_DIR)/build C=2 M=`pwd` modules;\
	fi

sparse_endian: build_pre
	if which sparse 1>/dev/null 2>&1; then\
		make -C $(KERN_VER_DIR)/build C=2 CF="-D_CHECK_ENDIAN_" M=`pwd` modules;\
	fi

smatch: build_pre
	if which smatch 1>/dev/null 2>&1; then\
		make -C $(KERN_VER_DIR)/build CHECK="/root/bin/smatch -p=kernel" C=2 M=`pwd` modules;\
	fi

clean:
	rm -f Module.symvers
	rm -f modules.order
	make -C $(KERN_VER_DIR)/build M=`pwd` clean

install: build
	make -C $(KERN_VER_DIR)/build M=`pwd` INSTALL_MOD_PATH=$(PREFIX) INSTALL_MOD_DIR=$(DRV_DIR) modules_install

ifeq ("$(REDHAT)","Red Hat")
	@echo "override qedi * $(DRV_DIR)" > qedi.conf ;
	install -m 644 -D qedi.conf $(PREFIX)/etc/depmod.d/qedi.conf;
endif

	@if [ "$(PREFIX)" = "" ] ;\
		then /sbin/depmod -a ;\
		else echo " *** Run '/sbin/depmod -a' to update the module database." ;\
	fi

	@if [ -f "$(BOOTDIR)/initrd-$(K_VERSION).img" ]; then \
		cp $(BOOTDIR)/initrd-$(K_VERSION).img $(BOOTDIR)/initrd-$(K_VERSION).bak.img; \
	fi \

	@if [ -f "$(BOOTDIR)/initramfs-$(K_VERSION).img" ]; then \
		cp $(BOOTDIR)/initramfs-$(K_VERSION).img $(BOOTDIR)/initramfs-$(K_VERSION).bak.img; \
	fi \

	@echo "Adding driver QEDI to INITRD image, rebuilding INITRD image"

	@if [ -f "$(DRACUT)" ] && [ -f "$(BOOTDIR)/initramfs-$(K_VERSION).img" ]; then \
		dracut -f --add-drivers qedi ;\
	elif [ -f "$(UPDATE_INITRAMFS)" ]; then\
		update-initramfs -u;\
	else \
		if [ -f "$(rhel_distro)" ]; then \
			mkinitrd -f /boot/initrd-$(K_VERSION).img $(K_VERSION) >& /dev/null; \
		else \
			mkinitrd -k /boot/vmlinuz-$(K_VERSION) -i /boot/initrd-$(K_VERSION) >& /dev/null; \
		fi \
	fi
	@echo "Done."
