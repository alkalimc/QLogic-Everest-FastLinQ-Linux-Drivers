ROCE = 1
# Makefile for building Linux QLogic 40/50/100-Gigabit Core module.
AT?=@
export AT

KVER =
ifeq ($(KVER),)
    KVER = $(shell uname -r)
endif

__ARCH=$(shell uname -m)

# PREFIX may be set by the RPM build to set the effective root.
PREFIX =
ifeq ($(shell ls /lib/modules/$(KVER)/build > /dev/null 2>&1 && echo build),)
# SuSE source RPMs
    _KVER=$(shell echo $(KVER) | cut -d "-" -f1,2)
    _KFLA=$(shell echo $(KVER) | cut -d "-" -f3)
    _ARCH=$(shell file -b /lib/modules/$(shell uname -r)/build | cut -d "/" -f5)
    ifeq ($(_ARCH),)
        _ARCH=$(__ARCH)
    endif
    ifeq ($(shell ls /usr/src/linux-$(_KVER)-obj > /dev/null 2>&1 && echo linux),)
      ifeq ($(shell ls /usr/src/kernels/$(KVER)-$(__ARCH) > /dev/null 2>&1 && echo linux),)
        LINUX=
      else
        LINUX=/usr/src/kernels/$(KVER)-$(__ARCH)
        LINUXSRC=$(LINUX)
        ifeq ($(KERNEL_DIR),)
            KERNEL_DIR=$(LINUX)
        endif
      endif
    else
        ifeq ($(KERNEL_DIR),)
            KERNEL_DIR=/usr/src/linux-$(_KVER)-obj/$(_ARCH)/$(_KFLA)
        endif
        LINUXSRC=/usr/src/linux-$(_KVER)
    endif
else
    BCMPROC := $(__ARCH)
    BCMCFGS := /lib/modules/$(KVER)/build \
               /usr/src/kernels/$(KVER)-$(BCMPROC) \
               /usr/src/linux-$(shell echo $(KVER) | sed "s|\(.*\)-\([a-z]*\)|\1-obj/$(BCMPROC)/\2|") \
               /usr/src/linux

    # Delete any entries that do not exist.
    BCMTSTDIR = $(shell if ls $(DIR) > /dev/null 2>&1; then echo $(DIR); fi)
    BCMCFGS := $(foreach DIR, $(BCMCFGS), $(BCMTSTDIR))

    ifeq ($(BCMCFGS),)
            $(error Linux kernel config directory not found)
    else
        ifeq ($(KERNEL_DIR),)
            KERNEL_DIR := $(firstword $(BCMCFGS))
	    ifeq ($(LINUXSRC),)
	        ifneq ($(shell ls /lib/modules/$(KVER)/source > /dev/null 2>&1 && echo source),)
		    LINUXSRC=/lib/modules/$(KVER)/source
	        endif
	    endif
        endif
    endif
endif

ifeq ($(LINUXSRC),)
   LINUXSRC=$(KERNEL_DIR)
endif

#FW version
FW_MAJOR_VERSION=$(shell cat common_hsi.h | grep -a FW_MAJOR_VERSION | tr -dc '0-9')
FW_MINOR_VERSION=$(shell cat common_hsi.h | grep -a FW_MINOR_VERSION | tr -dc '0-9')
FW_REVISION_VERSION=$(shell cat common_hsi.h | grep -a FW_REVISION_VERSION | tr -dc '0-9')
FW_ENGINEERING_VERSION=$(shell cat common_hsi.h | grep -a FW_ENGINEERING_VERSION | tr -dc '0-9')
FW_VER_STR=${FW_MAJOR_VERSION}.${FW_MINOR_VERSION}.${FW_REVISION_VERSION}.${FW_ENGINEERING_VERSION}
# paths
sles_distro := $(wildcard /etc/SuSE-release)
rhel_distro := $(wildcard /etc/redhat-release)
ubuntu_distro := $(shell lsb_release -is 2> /dev/null | grep Ubuntu)
ifeq ($(sles_distro)$(ubuntu_distro),)
    DISTRO_DRV_DIR = extra
else # SuSE|Ubuntu
    DISTRO_DRV_DIR = updates
endif
ifeq ($(shell ls /lib/modules/$(KVER)/$(DISTRO_DRV_DIR) > /dev/null 2>&1 && echo 1),1)
    DRV_DIR = $(DISTRO_DRV_DIR)/qlgc-fastlinq
else
    ifeq ($(shell grep -q "search.*[[:space:]]$(DISTRO_DRV_DIR)" /etc/depmod.conf > /dev/null 2>&1 && echo 1),1)
        DRV_DIR = $(DISTRO_DRV_DIR)/qlgc-fastlinq
    else
        ifeq ($(shell grep -q "search.*[[:space:]]$(DISTRO_DRV_DIR)" /etc/depmod.d/* > /dev/null 2>&1 && echo 1),1)
            DRV_DIR = $(DISTRO_DRV_DIR)/qlgc-fastlinq
        else
            ifeq ($(shell if [ $(shell echo $(KVER) | cut -d. -f1) -ge 3 -a $(shell echo $(KVER) | cut -d. -f2) -ge 2 ]; then echo 1; fi),1)
                DRV_DIR = kernel/drivers/net/ethernet/qlogic
            else # KVER < 3.2
                DRV_DIR = kernel/drivers/net
            endif
        endif
    endif
endif

MAN_DIR = $(PREFIX)/usr/share/man/man4
FW_DIR = $(PREFIX)/lib/firmware/qed

-include core.list

ROCE ?= 1
IWARP ?= 1
FCOE ?= 1
ISCSI ?= 1
L2 ?= 1
SRIOV ?= 1
PTP ?= 1
PVRDMA ?= 0

SOURCES = qed_main.c qed_debug.c qed_debug.h qed_tests.c qed_devlink.c qed_devlink.h qed.h qed_init_fw_funcs.c qed_init_ops.c qed_init_ops.h qed_init_values.h qed_hw.h qed_hw.c qed_spq.c qed_chain.h qed_int.c qed_int.h qed_cxt.c qed_cxt.h qed_dev.c qed_sp_commands.c qed_sp.h qed_sriov.h qed_vf.h qed_reg_addr.h qed_mcp.c qed_mcp.h qed_roce.h qed_roce_pvrdma.h qed_iwarp.h qed_ll2.h qed_iscsi.h qed_dev_api.h qed_l2.h qed_dcbx.c qed_dcbx.h qed_hsi.h qed_mfw_hsi.h qed_iro_hsi.h qed_dbg_hsi.h qed_if.h qed_fcoe.h qed_ooo.h qed_phy.c qed_phy_api.h qed_init_values_zipped.bin qed_init_values.bin qed_selftest.c qed_selftest.h qed_chain.c qed_mng_tlv.c qed_rdma.h qed_init_values_zipped.h

MODULE_NAME = qed

EXTRA_CFLAGS += -DDRV_MODULE_NAME=\"$(MODULE_NAME)\"

obj-m += $(MODULE_NAME).o
qed-objs = qed_main.o qed_init_fw_funcs.o qed_init_ops.o qed_hw.o qed_spq.o qed_sp_commands.o qed_chain.o qed_dev.o qed_cxt.o qed_int.o qed_hw.o qed_mcp.o qed_debug.o qed_tests.o qed_phy.o qed_dcbx.o qed_selftest.o qed_mng_tlv.o

ifeq ($(DISABLE_WERROR),)
	EXTRA_CFLAGS += -Werror
endif

ifneq ($(sles_distro),)
SLES_VERSION = $(shell cat /etc/SuSE-release | grep VERSION | grep -o -P [0-9]+)
SLES_PATCHLEVEL = $(shell cat /etc/SuSE-release | grep PATCHLEVEL | grep -o -P [0-9]+)
PADDED_PATCHLEVEL = $(shell if [ 10 -gt $(SLES_PATCHLEVEL) ]; then echo 0$(SLES_PATCHLEVEL); else echo $(SLES_PATCHLEVEL); fi)
SLES_DISTRO_VER = "0x$(SLES_VERSION)$(PADDED_PATCHLEVEL)"
EXTRA_CFLAGS += -DSLES_DISTRO=$(SLES_DISTRO_VER)
endif

ifneq ($(rhel_distro),)
RHEL_MAJVER := $(shell grep "MAJOR" /usr/include/linux/version.h | sed -e 's/.*MAJOR \([0-9]\)/\1/')
RHEL_MINVER := $(shell grep "MINOR" /usr/include/linux/version.h | sed -e 's/.*MINOR \([0-9]\)/\1/')
endif

DRV_VER_SUFFIX = "-"${USER}
EXTRA_CFLAGS += -DQEDE_MOD_VER=\"$(DRV_VER_SUFFIX)\"

# targets and rules
.PHONY: all modules_install initramfs_install clean fw_install light_install install list_sources candidate #docs_install

DEP_SOURCES = common_hsi.h

EXTRA_CFLAGS += -DATTN_DESC -DCONFIG_QED_ZIPPED_FW -DCONFIG_QED_BINARY_FW

ifeq ($(PVRDMA), 1)
	ROCE = 1
	DEP_SOURCES += qed_roce_pvrdma.c
	EXTRA_CFLAGS += -DCONFIG_QED_ROCE_PVRDMA
	qed-objs += qed_roce_pvrdma.o
endif

ifeq ($(ROCE), 1)
	RDMA = 1
	DEP_SOURCES += qed_roce.c roce_common.h
	EXTRA_CFLAGS +=  -DCONFIG_DCQCN
	qed-objs += qed_roce.o
endif

ifeq ($(IWARP), 1)
	RDMA = 1
	OOO = 1
	DEP_SOURCES += qed_iwarp.c iwarp_common.h
	EXTRA_CFLAGS += -DCONFIG_IWARP
	qed-objs += qed_iwarp.o
endif

ifeq ($(RDMA), 1)
	LL2 = 1
	DEP_SOURCES += qed_rdma_if.h qed_rdma.c rdma_common.h
	EXTRA_CFLAGS += -DCONFIG_QED_RDMA
	qed-objs += qed_rdma.o
endif

ifeq ($(FCOE), 1)
	STORAGE = 1
	LL2 = 1
	DEP_SOURCES += qed_fcoe.c fcoe_common.h qed_fcoe_if.h
	EXTRA_CFLAGS += -DCONFIG_QED_FCOE
	qed-objs += qed_fcoe.o
endif

ifeq ($(ISCSI), 1)
	STORAGE = 1
	LL2 = 1
	OOO = 1
	DEP_SOURCES += qed_iscsi.c iscsi_common.h tcp_common.h qed_iscsi_if.h
	EXTRA_CFLAGS += -DCONFIG_QEDI
	qed-objs += qed_iscsi.o
endif

ifeq ($(SRIOV), 1)
# SRIOV is too entwined in the L2 code; So if we want SRIOV, Let's enable L2.
# TODO - we might want to disconnect the two features in the future
	L2 = 1
	DEP_SOURCES += qed_vf.c qed_sriov.c
	TAR_HEADERS += qed_iov_if.h
	EXTRA_CFLAGS += -DCONFIG_QED_SRIOV
	qed-objs += qed_vf.o qed_sriov.o
endif

ifeq ($(L2), 1)
	DEP_SOURCES += qed_l2.c eth_common.h qed_eth_if.h
	EXTRA_CFLAGS += -DCONFIG_QEDE
	qed-objs += qed_l2.o
endif

ifeq ($(LL2), 1)
	DEP_SOURCES += qed_ll2.c qed_ll2_if.h
	EXTRA_CFLAGS += -DCONFIG_QED_LL2
	qed-objs += qed_ll2.o
endif

ifeq ($(OOO), 1)
	DEP_SOURCES += qed_ooo.c
	EXTRA_CFLAGS += -DCONFIG_QED_OOO
	qed-objs += qed_ooo.o
endif

ifeq ($(STORAGE), 1)
	DEP_SOURCES += storage_common.h storage_overtcp_common.h
endif

ifeq ($(PTP), 1)
	DEP_SOURCES += qed_ptp.c
	EXTRA_CFLAGS += -DCONFIG_QED_PTP
	qed-objs += qed_ptp.o
endif

SOURCES += $(DEP_SOURCES)
TAR_HEADERS += qed_tests.h qed_compat.h

# Checks to see if we should use thunk-extern
ifneq ($(rhel_distro),)
  ifeq ($(RHEL_MAJVER), 6)
    ifneq ($(shell test $(RHEL_MINVER) -gt 9 && echo thunk_extern),)
      USE_THUNK_EXTERN = 1
    endif
  endif
  ifeq ($(RHEL_MAJVER), 7)
    ifneq ($(shell test $(RHEL_MINVER) -gt 4 && echo thunk_extern),)
      USE_THUNK_EXTERN = 1
    endif
  endif
  ifeq ($(RHEL_MAJVER), 8)
    USE_THUNK_EXTERN = 1
  endif
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

ifdef CONFIG_RETPOLINE
  USE_THUNK_EXTERN = 1
endif

ifeq ($(USE_THUNK_EXTERN),1)
  THUNK_OPTIONS = -mindirect-branch=thunk-extern -mindirect-branch-register
else
  THUNK_OPTIONS = -mindirect-branch=thunk-inline -mindirect-branch-register
endif

ifneq ($(shell echo 'int main(){return 0;}' | gcc -x c $(THUNK_OPTIONS) - 2>/dev/null && echo thunk),)
	override EXTRA_CFLAGS += $(THUNK_OPTIONS)
endif

ifeq ($(shell grep "simple_open" $(LINUXSRC)/include/linux/fs.h > /dev/null 2>&1 && echo simple_open),)
	override EXTRA_CFLAGS += -D_DEFINE_SIMPLE_OPEN
endif
ifeq ($(SRIOV), 1)
endif
ifneq ($(shell grep "IFLA_VF_RATE" $(LINUXSRC)/include/uapi/linux/if_link.h > /dev/null 2>&1 && echo IFLA_VF_RATE),)
        override EXTRA_CFLAGS += -D_HAS_IFLA_VF_RATE
endif
ifneq ($(shell grep "spoofchk" $(LINUXSRC)/include/linux/if_link.h > /dev/null 2>&1 && echo spoofchk),)
        override EXTRA_CFLAGS += -D_DEFINE_IFLA_VF_SPOOFCHK
endif
ifneq ($(shell grep "linkstate" $(LINUXSRC)/include/linux/if_link.h > /dev/null 2>&1 && echo linkstate),)
        override EXTRA_CFLAGS += -D_DEFINE_IFLA_VF_LINKSTATE
endif

ifeq ($(shell ls $(LINUXSRC)/include/linux/hashtable.h > /dev/null 2>&1 && echo hashtable),)
	override EXTRA_CFLAGS += -D_DEFINE_HASHTABLE
else
ifneq ($(shell grep "hash_for_each_possible" $(LINUXSRC)/include/linux/hashtable.h | grep "node" > /dev/null 2>&1 && echo hash_for_each_possible_v1),)
	override EXTRA_CFLAGS += -D_HAS_HASH_FOR_EACH_POSSIBLE_V1
endif
endif

ifeq ($(shell grep "pci_enable_msix_range" $(LINUXSRC)/include/linux/pci.h > /dev/null 2>&1 && echo pci_enable_msix_range),)
	override EXTRA_CFLAGS += -D_DEFINE_PCI_ENABLE_MSIX_RANGE
endif
ifeq ($(shell grep "pci_enable_msix_exact" $(LINUXSRC)/include/linux/pci.h > /dev/null 2>&1 && echo pci_enable_msix_exact),)
	override EXTRA_CFLAGS += -D_DEFINE_PCI_ENABLE_MSIX_EXACT
endif

ifeq ($(shell grep "pcie_caps_reg" $(LINUXSRC)/include/linux/pci.h > /dev/null 2>&1 && echo pcie_caps_reg),)
	override EXTRA_CFLAGS += -D_DEFINE_PCIE_CAPS_REG
ifneq ($(shell grep -A50 "struct pci_dev" $(LINUXSRC)/include/linux/pci.h | grep "pcie_flags_reg" > /dev/null 2>&1 && echo pcie_flags_reg),)
        override EXTRA_CFLAGS += -D_HAS_PCIE_FLAGS_REG
endif
endif
ifeq ($(shell grep "pci_pcie_type" $(LINUXSRC)/include/linux/pci.h > /dev/null 2>&1 && echo pci_pcie_type),)
	override EXTRA_CFLAGS += -D_DEFINE_PCI_PCIE_TYPE
endif
ifeq ($(shell grep "pcie_capability_read_word" $(LINUXSRC)/include/linux/pci.h > /dev/null 2>&1 && echo pcie_capability_read_word),)
	override EXTRA_CFLAGS += -D_DEFINE_PCIE_CAPABILITY_READ_WORD
endif
ifeq ($(shell grep "pcie_capability_write_word" $(LINUXSRC)/include/linux/pci.h > /dev/null 2>&1 && echo pcie_capability_write_word),)
	override EXTRA_CFLAGS += -D_DEFINE_PCIE_CAPABILITY_WRITE_WORD
endif
ifeq ($(shell grep "pcie_capability_clear_and_set_word" $(LINUXSRC)/include/linux/pci.h > /dev/null 2>&1 && echo pcie_capability_clear_and_set_word),)
	override EXTRA_CFLAGS += -D_DEFINE_PCIE_CAPABILITY_CLEAR_AND_SET_WORD
endif

ifeq ($(shell grep "ether_addr_copy" $(LINUXSRC)/include/linux/etherdevice.h > /dev/null 2>&1 && echo ether_addr_copy),)
	override EXTRA_CFLAGS += -D_DEFINE_ETHER_ADDR_COPY
endif
ifeq ($(shell grep "ether_addr_equal" $(LINUXSRC)/include/linux/etherdevice.h > /dev/null 2>&1 && echo ether_addr_equal),)
	override EXTRA_CFLAGS += -D_DEFINE_ETHER_ADDR_EQUAL
endif

ifeq ($(shell grep "eth_zero_addr" $(LINUXSRC)/include/linux/etherdevice.h > /dev/null 2>&1 && echo X),)
	override EXTRA_CFLAGS += -D_DEFINE_ETH_ZERO_ADDR
endif
ifeq ($(shell grep "simple_write_to_buffer" $(LINUXSRC)/include/linux/fs.h > /dev/null 2>&1 && echo simple_write_to_buffer),)
        override EXTRA_CFLAGS += -D_HAS_SIMPLE_WRITE_TO_BUFFER
endif
ifneq ($(shell grep "ktime_get_real_seconds" $(LINUXSRC)/include/linux/timekeeping.h > /dev/null 2>&1 && echo timekeeping),)
	override EXTRA_CFLAGS += -D_HAS_KTIME_GET_REAL_SECONDS
endif
ifneq ($(shell grep "IEEE_8021QAZ_APP" $(LINUXSRC)/include/uapi/linux/dcbnl.h > /dev/null 2>&1 && echo IEEE_8021QAZ_APP),)
        override EXTRA_CFLAGS += -D_IEEE_8021QAZ_APP
endif
ifneq ($(shell grep "struct list_head msi_list" $(LINUXSRC)/include/linux/pci.h > /dev/null 2>&1 && echo msi_list),)
	override EXTRA_CFLAGS += -D_HAS_MSI_LIST
endif

ifeq ($(shell grep "kstrtoul" $(LINUXSRC)/include/linux/kernel.h > /dev/null 2>&1 && echo kstrtoul),)
ifeq ($(shell grep "kstrtoul" $(LINUXSRC)/include/linux/kstrtox.h > /dev/null 2>&1 && echo kstrtoul),)
	override EXTRA_CFLAGS += -D_DEFINE_KSTRTOUL
endif
endif

ifneq ($(shell grep "define alloc_workqueue" $(LINUXSRC)/include/linux/workqueue.h> /dev/null 2>&1 && echo alloc_workqueue),)
	override EXTRA_CFLAGS += -D_HAS_ALLOC_WORKQUEUE
endif
ifeq ($(shell grep "SUPPORTED_40000baseKR4_Full" $(LINUXSRC)/include/uapi/linux/ethtool.h > /dev/null 2>&1 && echo SUPPORTED_40000baseKR4_Full),)
	override EXTRA_CFLAGS += -D_DEFINE_SUPPORTED_40000BASEKR4_FULL
endif
ifeq ($(shell grep "SUPPORTED_25000baseKR_Full" $(LINUXSRC)/include/uapi/linux/ethtool.h > /dev/null 2>&1 && echo SUPPORTED_25000baseKR_Full),)
	override EXTRA_CFLAGS += -D_DEFINE_SUPPORTED_25000BASEKR_FULL
endif
ifeq ($(shell grep "data_access_exceeds_word_size" $(LINUXSRC)/include/linux/compiler.h > /dev/null 2>&1 && echo XXX),)
	override EXTRA_CFLAGS += -DDEFINE_DATA_ACCESS_EXCEEDS_WORD_SIZE
endif
ifeq ($(shell grep "READ_ONCE" $(LINUXSRC)/include/linux/compiler.h > /dev/null 2>&1 && echo XXX),)
ifeq ($(shell grep "READ_ONCE" $(LINUXSRC)/include/asm-generic/rwonce.h > /dev/null 2>&1 && echo XXX),)
	override EXTRA_CFLAGS += -DDEFINE_READ_ONCE
endif
endif
ifneq ($(shell grep "build_skb" $(LINUXSRC)/include/linux/skbuff.h > /dev/null 2>&1 && echo build_skb),)
	override EXTRA_CFLAGS += -D_HAS_BUILD_SKB
endif
ifneq ($(shell grep "build_skb" $(LINUXSRC)/include/linux/skbuff.h | grep "frag_size" > /dev/null 2>&1 && echo build_skb_v2),)
	override EXTRA_CFLAGS += -D_HAS_BUILD_SKB_V2
endif
ifneq ($(shell grep "eth_random_addr" $(LINUXSRC)/include/linux/etherdevice.h > /dev/null 2>&1 && echo eth_random_addr),)
	override EXTRA_CFLAGS += -D_HAS_ETH_RANDOM_ADDR
endif
ifneq ($(shell grep "get_random_bytes" $(LINUXSRC)/include/linux/random.h > /dev/null 2>&1 && echo get_random_bytes),)
	override EXTRA_CFLAGS += -D_HAS_ETH_RANDOM_BYTES
endif
ifneq ($(shell grep "time_t" $(LINUXSRC)/include/linux/types.h > /dev/null 2>&1 && echo time_t),)
	override EXTRA_CFLAGS += -D_HAS_TIME_T
endif
ifneq ($(shell grep "time_to_tm" $(LINUXSRC)/include/linux/time.h > /dev/null 2>&1 && echo time_to_tm),)
	override EXTRA_CFLAGS += -D_HAS_TIME_TO_TM
endif
ifneq ($(shell grep "time_to_tm" $(LINUXSRC)/include/linux/time32.h > /dev/null 2>&1 && echo time_to_tm),)
	override EXTRA_CFLAGS += -D_HAS_TIME_TO_TM
endif
ifeq ($(shell grep "crc8" $(LINUXSRC)/include/linux/crc8.h > /dev/null 2>&1 && echo crc8),)
	override EXTRA_CFLAGS += -D_DEFINE_CRC8
else
ifeq ($(shell test `find /lib/modules/$(KVER) -name crc8.ko 2> /dev/null` && echo crc8.ko),)
	override EXTRA_CFLAGS += -D_MISSING_CRC8_MODULE
endif
ifeq ($(OVERRIDE_CRC8), 1)
	override EXTRA_CFLAGS += -D_MISSING_CRC8_MODULE
endif
endif

ifeq ($(shell grep "vfs_fsync" $(LINUXSRC)/include/linux/fs.h | grep "dentry" > /dev/null 2>&1 && echo vfs_fsync_v2),)
	override EXTRA_CFLAGS += -D_HAS_VFS_FSYNC_V2
endif

ifneq ($(shell grep "\skernel_write" $(LINUXSRC)/include/linux/fs.h | grep "loff_t \*" > /dev/null 2>&1 && echo kernel_write_v2),)
	override EXTRA_CFLAGS += -D_HAS_KERNEL_WRITE_V2
endif

ifneq ($(shell grep "mmiowb_spin_lock" $(LINUXSRC)/include/asm-generic/mmiowb.h > /dev/null 2>&1 && echo mmiowb_spin_lock),)
	override EXTRA_CFLAGS += -D_HAS_MMIOWB_SPIN_LOCK
endif

ifneq ($(shell grep "devlink_health_reporter_ops" $(LINUXSRC)/include/net/devlink.h > /dev/null 2>&1 && echo devlink_health_reporter_ops),)
        override EXTRA_CFLAGS += -D_HAS_DEVLINK
	qed-objs += qed_devlink.o
ifneq ($(shell grep -A2 "devlink_fmsg_binary_pair_put" $(LINUXSRC)/include/net/devlink.h | grep u32 > /dev/null 2>&1 && echo devlink_fmsg_binary_pair_put),)
        override EXTRA_CFLAGS += -D_HAS_DEVLINK_DUMP
endif
endif

ifneq ($(shell grep -s -A3 "devlink_health_reporter_create" $(LINUXSRC)/include/net/devlink.h | grep auto_recover > /dev/null 2>&1 && echo auto_recover),)
        override EXTRA_CFLAGS += -D_HAS_DEVLINK_AUTO_RECOVER
endif

ifneq ($(shell grep "get_fs" $(LINUXSRC)/arch/x86/include/asm/uaccess.h > /dev/null 2>&1 && echo get_fs),)
	override EXTRA_CFLAGS += -D_HAS_GET_FS
endif
ifneq ($(shell ls $(LINUXSRC)/include/linux/time64.h > /dev/null 2>&1 && echo timespec64),)
	override EXTRA_CFLAGS += -D_HAS_TIMESPEC64
endif

ifneq ($(shell grep -e "bool efi_enabled" -e "int efi_enabled" $(LINUXSRC)/include/linux/efi.h > /dev/null 2>&1 && echo efi_enabled),)
 	       override EXTRA_CFLAGS += -D_HAS_EFI_ENABLED
endif

ifneq ($(shell grep "EFI_SECURE_BOOT" $(LINUXSRC)/include/linux/efi.h > /dev/null 2>&1 && echo SECURE_BOOT),)
	override EXTRA_CFLAGS += -D_HAS_SECURE_BOOT
endif

ifneq ($(shell grep "sysfs_bin_attr_init" $(LINUXSRC)/include/linux/sysfs.h > /dev/null 2>&1 && echo sysfs_bin_attr_init),)
	override EXTRA_CFLAGS += -D_HAS_SYSFS_BIN_ATTR_INIT
endif

ifneq ($(shell sed -n -E "/devlink_register\(/,/\\);/p" $(LINUXSRC)/include/net/devlink.h 2>&1 | grep "struct device" > /dev/null 2>&1 && echo struct_device),)
        override EXTRA_CFLAGS += -D_HAS_DEVLINK_STRUCT_DEV_DEVLINK_REGISTER
endif

ifneq ($(shell sed -n -E "/devlink_params_publish\(/,/\\);/p" $(LINUXSRC)/include/net/devlink.h 2>&1 | grep "devlink_params_publish" > /dev/null 2>&1 && echo param_publish),)
        override EXTRA_CFLAGS += -D_HAS_DEVLINK_PARAMS_PUBLISH
endif

ifneq ($(shell sed -n -E "/devlink_register\(/,/\\);/p" $(LINUXSRC)/include/net/devlink.h 2>&1 | grep "int" > /dev/null 2>&1 && echo int_return),)
        override EXTRA_CFLAGS += -D_HAS_DEVLINK_INT_DEVLINK_REGISTER
endif

ifneq ($(shell grep "ndo_set_vf_trust" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo netdev_set_vf_trst),)
ifneq ($(shell grep -B50 "\*ndo_set_vf_trust" $(LINUXSRC)/include/linux/netdevice.h | sed 's/[ ,\t]//gi' | grep net_device_ops_extended > /dev/null 2>&1 && echo set_vf_trust),)
        override EXTRA_CFLAGS += -D_HAS_NDO_EXT_SET_VF_TRUST
else
        override EXTRA_CFLAGS += -D_HAS_NDO_SET_VF_TRUST
endif
endif

ifneq ($(shell awk '/struct pci_msi_desc.{/,/^}/'  $(LINUXSRC)/include/linux/msi.h 2>/dev/null | grep mask_base > /dev/null 2>&1 && echo mask_base),)
        override EXTRA_CFLAGS += -D_PCI_MSI_DESC_HAS_MASK_BASE
endif

all default: $(obj-m)

candidate: $(SOURCES)

$(obj-m): $(SOURCES)
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` modules

modules_install: $(obj-m)
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` INSTALL_MOD_PATH=$(PREFIX) INSTALL_MOD_DIR=$(DRV_DIR) modules_install  2>&1 | grep -v depmod
	$(AT)if [ "$(sles_distro)" = "" -a "$(ubuntu_distro)" = "" ]; then \
		echo "override qed * $(DRV_DIR)" > qed.conf ;\
		install -m 644 -D qed.conf $(PREFIX)/etc/depmod.d/qed.conf ;\
	fi
	$(AT)if [ "$(PREFIX)" = "" ] ;\
		then /sbin/depmod -a ;\
		else echo " *** Run '/sbin/depmod -a' to update the module database." ;\
	fi

initramfs_install: $(obj-m)
	$(AT)if [ -f /boot/initramfs-$(KVER).img ] && [ "`lsinitrd /boot/initramfs-$(KVER).img | grep qed.ko`" ] ; then\
		echo "back up initrd to /boot/initramfs-$(KVER).img.bak" ;\
		cp /boot/initramfs-$(KVER).img /boot/initramfs-$(KVER).img.bak ;\
		echo "add driver qed to initrd" ;\
		dracut -f --add-drivers qed ;\
	elif [ -f /boot/initrd.img-$(KVER) ] && [ -f /usr/sbin/update-initramfs ]; then\
		echo "back up initrd to /boot/initrd.img-$(KVER).bak";\
		cp /boot/initrd.img-$(KVER) /boot/initrd.img-$(KVER).bak;\
		echo "updating initrd" ;\
		update-initramfs -u;\
	elif [ -f /boot/initrd.img-$(KVER) ]; then\
		echo "back up initrd to /boot/initrd.img-$(KVER).bak";\
		cp /boot/initrd.img-$(KVER) /boot/initrd.img-$(KVER).bak;\
		echo "updating initrd" ;\
		mkinitrd -o /boot/initrd.img-$(KVER) $(KVER);\
	elif [ -f /boot/initrd-$(KVER) ]; then\
		echo "back up initrd to /boot/initrd-$(KVER).bak";\
		cp /boot/initrd-$(KVER) /boot/initrd-$(KVER).bak;\
		echo "updating initrd" ;\
		mkinitrd -k /boot/vmlinuz-$(KVER) -i /boot/initrd-$(KVER) -m qed &>/dev/null;\
	else \
		echo "warning: initrd isn't being updated"; \
	fi

#docs_install:
#	gzip -c qede.4 > qede.4.gz
#	install -m 644 qede.4.gz $(MAN_DIR)

clean:
	$(AT)make -C $(KERNEL_DIR) M=`pwd` clean

fw_install: common_hsi.h qed_init_values_zipped.bin qed_init_values.bin
	mkdir -p $(FW_DIR)
	install -m 755 qed_init_values_zipped.bin $(FW_DIR)/qed_init_values_zipped-${FW_VER_STR}.bin
	install -m 755 qed_init_values.bin $(FW_DIR)/qed_init_values-${FW_VER_STR}.bin

light_install: fw_install modules_install

install: fw_install modules_install initramfs_install #doc_install

# an option to get all sources required for compilation, used in OFED packaging
list_sources:
	@echo $(realpath $(SOURCES)) | tr " " "\n"

# generate the include files
-include Makefile.gen
