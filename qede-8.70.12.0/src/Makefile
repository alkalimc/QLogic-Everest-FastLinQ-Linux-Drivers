ifeq ($(QED_DIR),)
	override QED_DIR = $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))/../../qed-8.70.12.0/src
endif
ROCE = 1
#!/usr/bin/make
# Makefile for building Linux QLogic 40/50/100-Gigabit ethernet driver as a module.

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

ROCE ?= 1

SOURCES = qede_main.c qede_fp.c qede_filter.c qede_ethtool.c qede_dcbnl.c qede_ptp.h qede_ptp.c qede.h qede_compat.h qede_hsi.h
ifeq ($(ROCE), 1)
	SOURCES += qede_rdma.c qede_rdma.h
endif

MODULE_NAME = qede

EXTRA_CFLAGS += -DDRV_MODULE_NAME=\"$(MODULE_NAME)\" -DCONFIG_DCB
obj-m += $(MODULE_NAME).o
qede-objs = $(patsubst %.c,%.o,$(filter %.c, $(SOURCES)))

# The && / || logic is very peculiar, and works opposite to what is expected, I.e., if The command running
# has an exit value of 0 [henceforth 'RES'], then RES && echo "hello world" WILL PRINT while RES || echo ...
# will not. Someone needs to figure out exactly the makefile/shell evaluations and fix this...

ifeq ($(DISABLE_WERROR),)
        override EXTRA_CFLAGS += -Werror
endif

ifeq ($(ENABLE_TIME_DEBUG),1)
	override EXTRA_CFLAGS += -DTIME_FP_DEBUG
endif

ifeq ($(ENABLE_FULL_TX_DEBUG),1)
	override EXTRA_CFLAGS += -DFULL_TX_DEBUG
endif

ifneq ($(sles_distro),)
SLES_VERSION = $(shell cat /etc/SuSE-release | grep VERSION | grep -o -P [0-9]+)
SLES_PATCHLEVEL = $(shell cat /etc/SuSE-release | grep PATCHLEVEL | grep -o -P [0-9]+)
PADDED_PATCHLEVEL = $(shell if [ 10 -gt $(SLES_PATCHLEVEL) ]; then echo 0$(SLES_PATCHLEVEL); else echo $(SLES_PATCHLEVEL); fi)
SLES_DISTRO_VER = "0x$(SLES_VERSION)$(PADDED_PATCHLEVEL)"
override EXTRA_CFLAGS += -DSLES_DISTRO=$(SLES_DISTRO_VER)
endif

ifneq ($(rhel_distro),)
RHEL_MAJVER := $(shell grep "MAJOR" /usr/include/linux/version.h | sed -e 's/.*MAJOR \([0-9]\)/\1/')
RHEL_MINVER := $(shell grep "MINOR" /usr/include/linux/version.h | sed -e 's/.*MINOR \([0-9]\)/\1/')
endif

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

ifeq ($(shell grep "prandom_bytes" $(LINUXSRC)/include/linux/random.h > /dev/null 2>&1 && echo prandom_bytes),)
ifeq ($(shell grep "prandom_bytes" $(LINUXSRC)/include/linux/prandom.h > /dev/null 2>&1 && echo prandom_bytes),)
	override EXTRA_CFLAGS += -D_DEFINE_PRANDOM_BYTES_
endif
endif

ifneq ($(shell grep -A3 "pci_driver_rh" $(LINUXSRC)/include/linux/pci.h | grep "sriov_configure" > /dev/null 2>&1 && echo pci_driver_rh),)
	override EXTRA_CFLAGS += -DHAS_SRIOV_PCI_DRIVER_RH
endif

ifneq ($(shell grep "ndo_fix_features" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo ndo_fix_features),)
	override EXTRA_CFLAGS += -DHAS_NDO_FIX_FEATURES
endif

ifeq ($(shell grep "sriov_configure" $(LINUXSRC)/include/linux/pci.h > /dev/null 2>&1 || echo "Not Found"),)
	override EXTRA_CFLAGS += -DSYS_INC_SRIOV
endif

ifneq ($(shell grep "PCI_DEV_FLAGS_NO_RELAXED_ORDERING" $(LINUXSRC)/include/linux/pci.h > /dev/null 2>&1 && echo "RO Flag"),)
ifneq ($(shell grep "pcie_find_root_port" $(LINUXSRC)/include/linux/pci.h > /dev/null 2>&1 && echo "pcie_find_root_port"),)
        override EXTRA_CFLAGS += -D_HAS_PCI_RO_CHECK_FLAG
endif
endif

ifeq ($(shell grep "netif_get_num_default_rss_queues" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo netif_get_num_default_rss_queues),)
	override EXTRA_CFLAGS += -D_DEFINE_NETIF_GET_NUM_DEFAULT_RSS_QUEUES
endif

ifeq ($(shell grep "netdev_tx_completed_queue" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo netdev_tx_completed_queue),)
	override EXTRA_CFLAGS += -D_DEFINE_NETDEV_TX_COMPLETED_QUEUE
endif

ifeq ($(shell grep "ethtool_rxfh_indir_default" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo ethtool_rxfh_indir_default),)
	override EXTRA_CFLAGS += -D_DEFINE_ETHTOOL_RXFH_INDIR_DEFAULT
endif

ifneq ($(shell grep "ETH_SS_RSS_HASH_FUNCS" $(LINUXSRC)/include/uapi/linux/ethtool.h > /dev/null 2>&1 && echo RSS_HASH),)
	override EXTRA_CFLAGS += -D_HAS_RSS_HASH_FUNCS
endif
ifneq ($(shell grep "ETH_SS_RSS_HASH_FUNCS" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo RSS_HASH),)
# At least in RH6.8, It's backported to here
	override EXTRA_CFLAGS += -D_HAS_RSS_HASH_FUNCS
endif

ifneq ($(shell grep "struct ethtool_tcpip6_spec" $(LINUXSRC)/include/uapi/linux/ethtool.h > /dev/null 2>&1 && echo ethtool_tcpip6_spec),)
        override EXTRA_CFLAGS += -D_HAS_IPV6_ETHTOOL_SPEC
endif
ifneq ($(shell grep "struct ethtool_tcpip6_spec" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo ethtool_tcpip6_spec),)
        override EXTRA_CFLAGS += -D_HAS_IPV6_ETHTOOL_SPEC
endif

ifneq ($(shell grep "set_per_queue_coalesce" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo set_per_queue_coalesce),)
        override EXTRA_CFLAGS += -D_HAS_ETHTOOL_PER_QUEUE_COAL
endif

ifneq ($(shell grep "rtnl_link_stats64" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo rtnl_link_stats64),)
	override EXTRA_CFLAGS += -D_HAS_RTNL_LINK_STATS64
ifneq ($(shell grep "ndo_get_stats64" $(LINUXSRC)/include/linux/netdevice.h | grep "void" > /dev/null 2>&1 && echo rtnl_link_stats64),)
	override EXTRA_CFLAGS += -D_HAS_RTNL_LINK_STATS64_VOID
endif
endif
ifneq ($(shell grep "netdev_phys_port_id" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo netdev_phys_port_id),)
	override EXTRA_CFLAGS += -D_HAS_PHYS_PORT_ID
endif

ifneq ($(shell grep "netdev_notifier_info_to_dev" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo netdev_notifier_info_to_dev),)
	override EXTRA_CFLAGS += -D_HAS_NETDEV_NOTIFIER
endif

ifneq ($(shell grep "ndo_get_vf_config" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo netdev_get_vf_config),)
	override EXTRA_CFLAGS += -D_HAS_NDO_GET_VF_CONFIG
endif

ifneq ($(shell grep "ethtool_ops_ext " $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo ethtool_ops_ext),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_OPS_EXT
endif

ifneq ($(shell grep "new_ethtool_ops" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structnew_ethtool_ops > /dev/null 2>&1 && echo new_ethtool_ops),)
	override EXTRA_CFLAGS += -D_HAS_NEW_ETHTOOL_OPS
endif

ifneq ($(shell grep "get_rxfh:" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo get_rxfh),)
ifneq ($(shell grep -B50 "\*get_rxfh.*\*indir" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo get_rxfh_indir_size),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_EXT_GET_RXFH
else
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_GET_RXFH
endif
endif

ifneq ($(shell grep "set_rxfh:" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo get_rxfh),)
ifneq ($(shell grep -B50 "\*set_rxfh.*\*indir" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo get_rxfh_indir_size),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_EXT_SET_RXFH
else
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_SET_RXFH
endif
endif

ifneq ($(shell grep "get_rxfh_indir_size" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo get_rxfh_indir_size),)
ifneq ($(shell grep -B50 "\*get_rxfh_indir_size" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo get_rxfh_indir_size),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_EXT_GET_RXF_INDIR_SIZE
else
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_GET_RXF_INDIR_SIZE
endif
endif

ifneq ($(shell grep "get_rxfh_key_size" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo get_rxfh_key_size),)
ifneq ($(shell grep -B50 "\*get_rxfh_key_size" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo get_rxfh_key_size),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_EXT_GET_RXF_KEY_SIZE
else
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_GET_RXF_KEY_SIZE
endif
endif

ifneq ($(shell grep "get_rxfh_indir" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo get_rxfh_indir),)
ifneq ($(shell grep -B50 "\*get_rxfh_indir" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo get_rxfh_indir),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_EXT_GET_RXF_INDIR
else
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_GET_RXF_INDIR
endif
endif

ifneq ($(shell grep "set_rxfh_indir" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo set_rxfh_indir),)
ifneq ($(shell grep -B50 "\*set_rxfh_indir" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo set_rxfh_indir),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_EXT_SET_RXF_INDIR
else
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_SET_RXF_INDIR
endif
endif

ifneq ($(shell grep "get_channels" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo get_channels),)
ifneq ($(shell grep -B50 "\*get_channels" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo get_channels),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_EXT_GET_CHANNELS
else
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_GET_CHANNELS
endif
endif

ifneq ($(shell grep "set_channels" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo set_channels),)
ifneq ($(shell grep -B50 "\*set_channels" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo set_channels),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_EXT_SET_CHANNELS
else
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_SET_CHANNELS
endif
endif

ifneq ($(shell grep "get_dump_flag" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo get_dump_flag),)
ifneq ($(shell grep -B50 "\*get_dump_flag" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo get_dump_flag),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_EXT_GET_DUMP_FLAG
else
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_GET_DUMP_FLAG
endif
endif

ifneq ($(shell grep "get_dump_data" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo get_dump_data),)
ifneq ($(shell grep -B50 "\*get_dump_data" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo get_dump_data),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_EXT_GET_DUMP_DATA
else
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_GET_DUMP_DATA
endif
endif

ifneq ($(shell grep "set_dump" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo set_dump),)
ifneq ($(shell grep -B50 "\*set_dump" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo set_dump),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_EXT_SET_DUMP
else
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_SET_DUMP
endif
endif

ifneq ($(shell grep "get_module_info" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo get_module_info),)

ifneq ($(shell grep -B50 "\*get_module_info" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo get_module_info),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_EXT_GET_MODULE_INFO
else
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_GET_MODULE_INFO
endif
endif

ifneq ($(shell grep "\*get_module_eeprom" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo get_module_eeprom),)
ifneq ($(shell grep -B50 "\*get_module_eeprom" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo get_module_eeprom),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_EXT_GET_MODULE_EEPROM
else
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_GET_MODULE_EEPROM
endif
endif

ifneq ($(shell grep "\*set_phys_id" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo set_phys_id),)
ifneq ($(shell grep -B50 "\*set_phys_id" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo set_phys_id),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_EXT_SET_PHYS_ID
else
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_SET_PHYS_ID
endif
ifneq ($(shell grep "\*set_phys_id" $(LINUXSRC)/include/linux/ethtool.h | grep ethtool_phys_id_state > /dev/null 2>&1 && echo set_phys_id),)
	override EXTRA_CFLAGS += -D_HAS_PHYS_ID_STATE
endif
endif

ifneq ($(shell grep -B50 "\*reset" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo reset),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_EXT_RESET
endif

ifneq ($(shell grep "\*get_eee" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo get_eee),)
ifneq ($(shell grep -B50 "\*get_eee" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo get_eee),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_EXT_GET_EEE
else
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_GET_EEE
endif
endif

ifneq ($(shell grep "ndo_set_vf_trust" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo netdev_set_vf_trst),)
ifneq ($(shell grep -B50 "\*ndo_set_vf_trust" $(LINUXSRC)/include/linux/netdevice.h | sed 's/[ ,\t]//gi' | grep net_device_ops_extended > /dev/null 2>&1 && echo set_vf_trust),)
	override EXTRA_CFLAGS += -D_HAS_NDO_EXT_SET_VF_TRUST
else
	override EXTRA_CFLAGS += -D_HAS_NDO_SET_VF_TRUST
endif
endif

ifneq ($(shell grep "\*ndo_get_vf_stats" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo ndo_get_vf_stats),)
override EXTRA_CFLAGS += -D_HAS_NDO_GET_VF_STATS
endif

ifneq ($(shell grep "ndo_change_mtu" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo ndo_change_mtu),)
ifneq ($(shell grep -B50 "\*ndo_change_mtu" $(LINUXSRC)/include/linux/netdevice.h | sed 's/[ ,\t]//gi' | grep net_device_ops_extended > /dev/null 2>&1 && echo ndo_change_mtu),)
	override EXTRA_CFLAGS += -D_HAS_NDO_EXT_CHANGE_MTU
endif
endif

ifneq ($(shell grep -A1 "set_vf_vlan" $(LINUXSRC)/include/linux/netdevice.h | grep "proto" > /dev/null 2>&1 && echo set_vf_vlan_proto),)
ifneq ($(shell grep -B50 "\*ndo_set_vf_vlan" $(LINUXSRC)/include/linux/netdevice.h | sed 's/[ ,\t]//gi' | grep net_device_ops_extended > /dev/null 2>&1 && echo set_vf_trust),)
	override EXTRA_CFLAGS += -D_HAS_NDO_EXT_VF_VLAN_PROTO
else
	override EXTRA_CFLAGS += -D_HAS_NDO_VF_VLAN_PROTO
endif
endif

ifneq ($(shell grep "ndo_udp_tunnel_add" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo ndo_udp_tunnel_add),)
ifneq ($(shell grep -B50 "\*ndo_udp_tunnel_add" $(LINUXSRC)/include/linux/netdevice.h | sed 's/[ ,\t]//gi' | grep net_device_ops_extended > /dev/null 2>&1 && echo set_vf_trust),)
	override EXTRA_CFLAGS += -D_HAS_NDO_EXT_UDP_TUNNEL_CONFIG
else
	override EXTRA_CFLAGS += -D_HAS_NDO_UDP_TUNNEL_CONFIG
endif
else
ifneq ($(shell grep "ndo_add_vxlan_port" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo add_vxlan_port),)
	override EXTRA_CFLAGS += -D_HAS_ADD_VXLAN_PORT
endif
ifneq ($(shell grep "ndo_add_geneve_port" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo add_geneve_port),)
	override EXTRA_CFLAGS += -D_HAS_ADD_GENEVE_PORT
endif
endif

ifneq ($(shell grep "\*get_ts_info" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo get_ts_info),)
ifneq ($(shell grep -B50 "\*get_ts_info" $(LINUXSRC)/include/linux/ethtool.h | sed 's/[ ,\t]//gi' | grep structethtool_ops_ext > /dev/null 2>&1 && echo get_ts_info),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_EXT_TS_INFO
else
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_TS_INFO
endif
endif

ifneq ($(shell grep "get_link_ksettings" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo get_link_ksettings),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_GET_LINK_KSETTINGS
endif

ifneq ($(shell grep "\*get_settings" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo get_settings),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_GET_SETTINGS
endif

ifneq ($(shell grep "get_fecparam" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo get_fecparam),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_GET_FEC
endif

ifneq ($(shell grep "ETHTOOL_LINK_MODE_50000baseSR2_Full_BIT" $(LINUXSRC)/include/uapi/linux/ethtool.h > /dev/null 2>&1 && echo 50000baseSR2_Full_BIT),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_50000baseSR2_Full_BIT
endif

ifneq ($(shell grep "ETHTOOL_LINK_MODE_1000baseX_Full_BIT" $(LINUXSRC)/include/uapi/linux/ethtool.h > /dev/null 2>&1 && echo 1000baseX_Full_BIT),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_1000baseX_Full_BIT
endif

ifneq ($(shell grep "SUPPORTED_40000baseKR4_Full" $(LINUXSRC)/include/uapi/linux/ethtool.h > /dev/null 2>&1 && echo 40000baseKR4_Full),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_40000baseKR4_Full
endif

ifneq ($(shell grep "ETHTOOL_LINK_MODE_25000baseKR_Full_BIT" $(LINUXSRC)/include/uapi/linux/ethtool.h > /dev/null 2>&1 && echo 25000baseKR_Full_BIT),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_HIGHER_SPEED_BITS
endif

ifneq ($(shell grep "ETHTOOL_LINK_MODE_FEC_NONE_BIT" $(LINUXSRC)/include/uapi/linux/ethtool.h > /dev/null 2>&1 && echo FEC_NONE_BIT),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_LINK_MODE_FEC_BIT
endif

ifneq ($(shell grep "ndo_set_vf_link_state" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo netdev_set_vf_link_state),)
	override EXTRA_CFLAGS += -D_HAS_NDO_SET_VF_LINK_STATE
endif

ifneq ($(shell grep "SKBTX_HW_TSTAMP" $(LINUXSRC)/include/linux/skbuff.h > /dev/null 2>&1 && echo ptp_clock_kernel),)
	override EXTRA_CFLAGS += -DQEDE_PTP_SUPPORT
endif

ifeq ($(shell grep "skb_set_hash" $(LINUXSRC)/include/linux/skbuff.h > /dev/null 2>&1 && echo skb_set_hash),)
	override EXTRA_CFLAGS += -D_DEFINE_SKB_SET_HASH
endif

ifeq ($(shell grep tcp_v6_check $(LINUXSRC)/include/net/ip6_checksum.h > /dev/null 2>&1 && echo tcp_v6_check),)
        override EXTRA_CFLAGS += -D_DEFINE_TCP_V6_CHECK_
endif

ifneq ($(shell grep "vlan_rx_add_vid" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo vlan_rx_add_vid),)
       override EXTRA_CFLAGS += -D_HAS_VLAN_RX_ADD_VID
endif

ifneq ($(shell grep "vlan_rx_add_vid" $(LINUXSRC)/include/linux/netdevice.h | cut -d'*' -f1 | grep void > /dev/null 2>&1 && echo vlan_rx_add_vid),)
	override EXTRA_CFLAGS += -D_VLAN_RX_ADD_VID_RETURNS_VOID
endif

ifeq ($(shell grep -A1 "vlan_rx_add_vid" $(LINUXSRC)/include/linux/netdevice.h | grep proto > /dev/null 2>&1 && echo vlan_rx_add_vid),)
	override EXTRA_CFLAGS += -D_VLAN_RX_ADD_VID_NO_PROTO
endif

ifeq ($(shell grep "ether_addr_copy" $(LINUXSRC)/include/linux/etherdevice.h > /dev/null 2>&1 && echo ether_addr_copy),)
	override EXTRA_CFLAGS += -D_DEFINE_ETHER_ADDR_COPY
endif

ifeq ($(shell grep "ether_addr_equal" $(LINUXSRC)/include/linux/etherdevice.h > /dev/null 2>&1 && echo ether_addr_equal),)
	override EXTRA_CFLAGS += -D_DEFINE_ETHER_ADDR_EQUAL
endif

ifneq ($(shell grep "eth_hw_addr_set" $(LINUXSRC)/include/linux/etherdevice.h > /dev/null 2>&1 && echo eth_hw_addr_set),)
        override EXTRA_CFLAGS += -D_HAS_ETH_HW_ADDR_SET
endif

ifeq ($(shell grep "napi_schedule_irqoff" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo napi_schedule_irqoff),)
	override EXTRA_CFLAGS += -D_DEFINE_NAPI_SCHEDULE_IRQOFF
endif

ifeq ($(shell grep "netdev_rss_key_fill" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo netdev_rss_key_fill),)
	override EXTRA_CFLAGS += -D_DEFINE_RSS_KEY_FILL
endif

ifneq ($(shell grep "ndo_set_vf_spoofchk" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo netdev_set_vf_spoofchk),)
	override EXTRA_CFLAGS += -D_HAS_NDO_SET_VF_SPOOFCHK
endif

ifneq ($(shell grep "IFLA_VF_RATE" $(LINUXSRC)/include/uapi/linux/if_link.h > /dev/null 2>&1 && echo IFLA_VF_RATE),)
	override EXTRA_CFLAGS += -D_HAS_IFLA_VF_RATE
endif

ifneq ($(shell grep "build_skb" $(LINUXSRC)/include/linux/skbuff.h > /dev/null 2>&1 && echo build_skb),)
	override EXTRA_CFLAGS += -D_HAS_BUILD_SKB
endif

ifneq ($(shell grep "csum_level" $(LINUXSRC)/include/linux/skbuff.h > /dev/null 2>&1 && echo csum_level),)
	override EXTRA_CFLAGS += -D_HAS_CSUM_LEVEL
endif

ifneq ($(shell grep "build_skb" $(LINUXSRC)/include/linux/skbuff.h | grep "frag_size" > /dev/null 2>&1 && echo build_skb_v2),)
	override EXTRA_CFLAGS += -D_HAS_BUILD_SKB_V2
endif

ifeq ($(shell grep "CYCLECOUNTER_MASK" $(LINUXSRC)/include/linux/timecounter.h > /dev/null 2>&1 && echo CYCLECOUNTER_MASK),)
	override EXTRA_CFLAGS += -D_DEFINE_CYCLECOUNTER_MASK
endif
ifneq ($(shell grep "timecounter_adjtime" $(LINUXSRC)/include/linux/timecounter.h > /dev/null 2>&1 && echo timecounter_adjtime),)
	override EXTRA_CFLAGS += -D_HAS_TIMECOUNTER_ADJTIME
endif
ifneq ($(shell grep "timespec64" $(LINUXSRC)/include/linux/time64.h > /dev/null 2>&1 && echo timecounter_adjtime),)
	override EXTRA_CFLAGS += -D_HAS_TIMESPEC64
endif
ifneq ($(shell grep "timespec64" $(LINUXSRC)/include/linux/time.h > /dev/null 2>&1 && echo timecounter_adjtime),)
# At least in RH6.8, this is in time.h and not time64.h
	override EXTRA_CFLAGS += -D_HAS_TIMESPEC64
endif
ifneq ($(shell grep "gettime64" $(LINUXSRC)/include/linux/ptp_clock_kernel.h > /dev/null 2>&1 && echo timecounter_adjtime),)
	override EXTRA_CFLAGS += -D_HAS_PTPTIME64
endif
ifneq ($(shell grep "eth_get_headlen" $(LINUXSRC)/include/linux/etherdevice.h > /dev/null 2>&1 && echo eth_get_headlen),)
	override EXTRA_CFLAGS += -D_HAS_GET_HEAD_LEN
endif
ifeq ($(shell grep "simple_write_to_buffer" $(LINUXSRC)/include/linux/fs.h > /dev/null 2>&1 && echo simple_write_to_buffer),)
        override EXTRA_CFLAGS += -D_HAS_SIMPLE_WRITE_TO_BUFFER
endif
ifeq ($(shell grep "simple_open" $(LINUXSRC)/include/linux/fs.h > /dev/null 2>&1 && echo simple_open),)
	override EXTRA_CFLAGS += -D_DEFINE_SIMPLE_OPEN
endif

ifneq ($(shell grep "page_ref_inc" $(LINUXSRC)/include/linux/page_ref.h > /dev/null 2>&1 && echo page_ref_inc),)
	override EXTRA_CFLAGS += -D_HAS_PAGE_REF_INC
endif

ifneq ($(shell grep "page_ref_count" $(LINUXSRC)/include/linux/mm.h > /dev/null 2>&1 && echo page_ref_count),)
        override EXTRA_CFLAGS += -D_HAS_PAGE_REF_COUNT
endif

ifneq ($(shell grep "IPPROTO_GRE" $(LINUXSRC)/include/linux/in.h > /dev/null 2>&1 && echo ipproto_gre),)
	override EXTRA_CFLAGS += -D_HAS_IP_PROTO_GRE
endif

ifneq ($(shell grep "IPPROTO_GRE" $(LINUXSRC)/include/uapi/linux/in.h > /dev/null 2>&1 && echo ipproto_gre),)
	override EXTRA_CFLAGS += -D_HAS_IP_PROTO_GRE
endif

ifneq ($(shell grep "GRE_HEADER_SECTION" $(LINUXSRC)/include/net/gre.h > /dev/null 2>&1 && echo gre_header_section),)
	override EXTRA_CFLAGS += -D_HAS_GRE_H_SECTION
endif

ifneq ($(shell grep "skb_get_hash_raw" $(LINUXSRC)/include/linux/skbuff.h > /dev/null 2>&1 && echo skb_get_hash_raw),)
	override EXTRA_CFLAGS += -D_HAS_SKB_HASH_RAW
endif

ifneq ($(shell grep "hlist_for_each_entry_safe" $(LINUXSRC)/include/linux/list.h | grep "tpos"),)
	override EXTRA_CFLAGS += -D_HAS_LEGACY_HLIST_ITERATION
endif

ifneq ($(shell grep "netdev_rfs_info" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo netdev_rfs_info),)
	override EXTRA_CFLAGS += -D_HAS_NDEV_RFS_INFO
endif

ifneq ($(shell grep "skb_get_rxhash" $(LINUXSRC)/include/linux/skbuff.h > /dev/null 2>&1 && echo skb_get_rxhash),)
	override EXTRA_CFLAGS += -D_HAS_SKB_GET_RXHASH
endif

ifneq ($(shell grep "struct vxlanhdr" $(LINUXSRC)/include/net/vxlan.h > /dev/null 2>&1 && echo "struct vxlanhdr"),)
	override EXTRA_CFLAGS += -D_HAS_VXLAN_HDR_DEFINED
endif

ifneq ($(shell grep "udp_gro_complete" $(LINUXSRC)/include/net/udp.h | grep "lookup"),)
	override EXTRA_CFLAGS += -D_HAS_UDP_GRO_COMP_EXPORTED
endif

ifneq ($(shell ls $(LINUXSRC)/include/net/gro.h > /dev/null 2>&1 && echo 1),)
	override EXTRA_CFLAGS += -D_HAS_GRO_HEADER
endif

ifneq ($(shell grep "xmit_more" $(LINUXSRC)/include/linux/skbuff.h > /dev/null 2>&1 && echo xmit_more),)
	override EXTRA_CFLAGS += -D_HAS_XMIT_MORE
endif

ifneq ($(shell grep "set_tunable" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo set_tunable),)
	override EXTRA_CFLAGS += -D_HAS_SET_TUNABLE
endif

ifneq ($(shell grep "NETIF_F_GSO_UDP_TUNNEL_CSUM" $(LINUXSRC)/include/linux/netdev_features.h > /dev/null 2>&1 && echo NETIF_F_GSO_UDP_TUNNEL_CSUM),)
	override EXTRA_CFLAGS += -D_HAS_GSO_TUN_L4_CSUM
endif

ifneq ($(shell grep "NETIF_F_GRO_HW" $(LINUXSRC)/include/linux/netdev_features.h > /dev/null 2>&1 && echo NETIF_F_GRO_HW),)
	override EXTRA_CFLAGS += -D_HAS_NET_GRO_HW
endif

ifneq ($(shell grep "ndo_features_check" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo ndo_gso_check),)
	override EXTRA_CFLAGS += -D_HAS_NDO_FEATURES_CHECK
endif

ifneq ($(shell ls $(LINUXSRC)/include/net/dcbnl.h  > /dev/null 2>&1 && echo dcbnl),)
ifneq ($(shell grep "getnumtcs" $(LINUXSRC)/include/net/dcbnl.h | cut -d'*' -f1 | grep int > /dev/null 2>&1 && echo getnumtcs),)
	override EXTRA_CFLAGS += -D_GETNUMTCS_RETURNS_INT
endif
ifneq ($(shell grep "\*getapp" $(LINUXSRC)/include/net/dcbnl.h | cut -d'*' -f1 | grep int > /dev/null 2>&1 && echo getapp),)
	override EXTRA_CFLAGS += -D_GETAPP_RETURNS_INT
endif
endif

ifneq ($(shell grep "IEEE_8021QAZ_APP" $(LINUXSRC)/include/uapi/linux/dcbnl.h > /dev/null 2>&1 && echo IEEE_8021QAZ_APP),)
        override EXTRA_CFLAGS += -D_IEEE_8021QAZ_APP
endif

ifneq ($(shell grep -B3 accel_priv $(LINUXSRC)/include/linux/netdevice.h | grep ndo_select_queue > /dev/null 2>&1 && echo has_accel_priv),)
        override EXTRA_CFLAGS += -DQEDE_SELECTQUEUE_HAS_ACCEL_PARAM
endif
ifneq ($(shell grep -B4 fallback $(LINUXSRC)/include/linux/netdevice.h | grep ndo_select_queue > /dev/null 2>&1 && echo has_accel_priv),)
        override EXTRA_CFLAGS += -DQEDE_SELECTQUEUE_HAS_FALLBACK_PARAM
endif

ifneq ($(shell grep -B4 sb_dev $(LINUXSRC)/include/linux/netdevice.h | grep ndo_select_queue > /dev/null 2>&1 && echo has_sb_dev),)
ifneq ($(shell grep -B4 fallback $(LINUXSRC)/include/linux/netdevice.h | grep ndo_select_queue > /dev/null 2>&1 && echo has_sb_dev),)
        override EXTRA_CFLAGS += -DQEDE_SELECTQUEUE_HAS_FALLBACK_SBDEV_PARAM
else
        override EXTRA_CFLAGS += -DQEDE_SELECTQUEUE_HAS_SBDEV_PARAM
endif
endif

ifneq ($(shell grep "register_netdevice_notifier_rh" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo register_netdevice_notifier_rh),)
	override EXTRA_CFLAGS += -D_HAS_REGISTER_NETDEVICE_NOTIFIER_RH
endif

ifneq ($(shell grep "ndo_xdp" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo ndo_xdp),)
	override EXTRA_CFLAGS += -D_HAS_NDO_XDP
endif

ifneq ($(shell grep "ndo_bpf" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo ndo_bpf),)
	override EXTRA_CFLAGS += -D_HAS_NDO_BPF
endif

ifneq ($(shell grep "XDP_QUERY_PROG" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo XDP_QUERY_PROG),)
	override EXTRA_CFLAGS += -D_HAS_XDP_QUERY_PROG
endif

ifneq ($(shell grep "prog_id" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo prog_id),)
	override EXTRA_CFLAGS += -D_HAS_XDP_PROG_ID
endif

ifneq ($(shell grep "prog_attached" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo prog_attached),)
	override EXTRA_CFLAGS += -D_HAS_XDP_PROG_ATTACHED
endif

ifneq ($(shell grep "data_hard_start" $(LINUXSRC)/include/linux/filter.h > /dev/null 2>&1 && echo xdp),)
	override EXTRA_CFLAGS += -D_HAS_NDO_XDP_HARD_START
endif

ifneq ($(shell grep "bpf_warn_invalid_xdp_action" $(LINUXSRC)/include/linux/filter.h | grep bpf_prog > /dev/null 2>&1 && echo bpf_prog),)
        override EXTRA_CFLAGS += -D_HAS_BPF_PROG
endif

ifneq ($(shell grep "max_mtu" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo max_mtu),)
ifneq ($(shell grep -B50 "max_mtu" $(LINUXSRC)/include/linux/netdevice.h | sed 's/[ ,\t]//gi' | grep net_device_extended > /dev/null 2>&1 && echo max_mtu),)
        override EXTRA_CFLAGS += -D_HAS_EXT_MAX_MTU
else
        override EXTRA_CFLAGS += -D_HAS_MAX_MTU
endif
endif

ifneq ($(shell grep "page_is_pfmemalloc" $(LINUXSRC)/include/linux/mm.h > /dev/null 2>&1 && echo pfmemalloc),)
	override EXTRA_CFLAGS += -D_HAS_PAGE_PFMEMALLOC_API
endif

ifneq ($(shell grep "pfmemalloc" $(LINUXSRC)/include/linux/mm_types.h > /dev/null 2>&1 && echo pfmemalloc),)
	override EXTRA_CFLAGS += -D_HAS_PAGE_PFMEMALLOC
endif

ifeq ($(shell grep "skb_frag_dma_map" $(LINUXSRC)/include/linux/skbuff.h > /dev/null 2>&1 && echo skb_frag_dma_map),)
	override EXTRA_CFLAGS += -D_DEFINE_SKB_FRAG_DMA_MAP
endif

ifeq ($(shell grep "typedef u64 cycle_t" $(LINUXSRC)/include/linux/types.h > /dev/null 2>&1 && echo cycle_t),)
	override EXTRA_CFLAGS += -D_HAS_REMOVED_CYCLE_T
endif

ifneq ($(shell grep "xdp_exception" $(LINUXSRC)/include/trace/events/xdp.h > /dev/null 2>&1 && echo xdp_exception),)
	override EXTRA_CFLAGS += -D_HAS_TRACE_XDP_EXCEPTION
endif

ifneq ($(shell grep "napi_complete_done" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo XXX),)
	override EXTRA_CFLAGS += -D_HAS_NAPI_COMPLETE_DONE
endif

ifeq ($(shell grep "get_flow_spec_ring_vf" $(LINUXSRC)/include/uapi/linux/ethtool.h > /dev/null 2>&1 && echo XXX),)
	override EXTRA_CFLAGS += -D_DEFINE_FLOW_SPEC_RING
endif

ifneq ($(shell grep "FLOW_EXT" $(LINUXSRC)/include/uapi/linux/ethtool.h > /dev/null 2>&1 && echo XXX),)
	override EXTRA_CFLAGS += -D_HAS_FLOW_EXT
endif

ifneq ($(shell grep -A1 "ndo_setup_tc" $(LINUXSRC)/include/linux/netdevice.h | grep handle > /dev/null 2>&1 && echo ndo_setup_tc),)
	override EXTRA_CFLAGS += -D_HAS_NDO_SETUP_TC_HANDLE
endif

ifneq ($(shell grep -A1 "ndo_setup_tc" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo ndo_setup_tc),)
	override EXTRA_CFLAGS += -D_HAS_NDO_SETUP_TC
endif

ifneq ($(shell grep -A1 "ndo_size" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo ndo_size),)
        override EXTRA_CFLAGS += -D_HAS_NDO_SIZE_EXT_OPS
endif

ifneq ($(shell grep "netif_elide_gro" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo XXX),)
	# This is not entirely correct [as the inclusion isn't strictly related],
	# but we needed some .h reference to a .c change
	override EXTRA_CFLAGS += -D_HAS_GRO_RECEIVE_AGG
endif

ifneq ($(shell grep -A1 "tc_mqprio_qopt" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo tc_mqprio_qopt),)
	override EXTRA_CFLAGS += -D_HAS_NDO_MQPRIO_OPT
endif

ifneq ($(shell grep -A1 "ndo_setup_tc" $(LINUXSRC)/include/linux/netdevice.h | grep chain_index > /dev/null 2>&1 && echo ndo_setup_tc),)
	override EXTRA_CFLAGS += -D_HAS_NDO_SETUP_TC_CHAIN
endif

ifneq ($(shell grep "ndo_set_tx_maxrate" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo ndo_set_tx_maxrate),)
ifneq ($(shell grep -B50 "\*ndo_set_tx_maxrate" $(LINUXSRC)/include/linux/netdevice.h | sed 's/[ ,\t]//gi' | grep net_device_ops_extended > /dev/null 2>&1 && echo ndo_set_tx_maxrate),)
	override EXTRA_CFLAGS += -D_HAS_NDO_EXT_SET_TX_MAXRATE
else
	override EXTRA_CFLAGS += -D_HAS_NDO_SET_TX_MAXRATE
endif
endif

ifneq ($(shell grep -A1 "tc_cls_flower_offload" $(LINUXSRC)/include/net/pkt_cls.h > /dev/null 2>&1 && echo tc_flower),)
	override EXTRA_CFLAGS += -D_HAS_TC_FLOWER
endif

ifneq ($(shell grep -A1 "tc_block_offload" $(LINUXSRC)/include/net/pkt_cls.h > /dev/null 2>&1 && echo tc_block_offload),)
	override EXTRA_CFLAGS += -D_HAS_TC_BLOCK
endif

ifneq ($(shell grep -A1 "tcf_exts_to_list" $(LINUXSRC)/include/net/pkt_cls.h > /dev/null 2>&1 && echo tc_exts_to_list),)
	override EXTRA_CFLAGS += -D_HAS_TC_EXTS_TO_LIST
endif

ifneq ($(shell grep -A1 "tcf_exts_for_each_action" $(LINUXSRC)/include/net/pkt_cls.h > /dev/null 2>&1 && echo tc_exts_to_list),)
        override EXTRA_CFLAGS += -D_HAS_TC_EXTS_ITER
endif

ifneq ($(shell grep -A1 "is_tcf_mirred_egress_redirect" $(LINUXSRC)/include/net/tc_act/tc_mirred.h > /dev/null 2>&1 && echo tc_mirred),)
	override EXTRA_CFLAGS += -D_HAS_TC_MIRRED
endif

ifneq ($(shell grep -A1 "tcf_mirred_ifindex" $(LINUXSRC)/include/net/tc_act/tc_mirred.h > /dev/null 2>&1 && echo tc_mirred),)
	override EXTRA_CFLAGS += -D_HAS_TC_MIRRED_IFIDX
endif

ifneq ($(shell grep -A1 "tcf_mirred_dev" $(LINUXSRC)/include/net/tc_act/tc_mirred.h > /dev/null 2>&1 && echo tcf_mirred_dev),)
	override EXTRA_CFLAGS += -D_HAS_TCF_MIRRED_DEV
endif

ifneq ($(shell grep "TC_SETUP_QDISC_MQPRIO" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo tc_setup_qdisc_mqprio),)
	override EXTRA_CFLAGS += -D_HAS_TC_SETUP_QDISC_MQPRIO
endif

ifneq ($(shell grep -A1 "tcf_exts_has_actions" $(LINUXSRC)/include/net/pkt_cls.h > /dev/null 2>&1 && echo tc_exts_has_act),)
	override EXTRA_CFLAGS += -D_HAS_EXTS_ACTION
endif

ifneq ($(shell grep -A1 "enum tc_setup_type" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo tc_setup_type),)
	override EXTRA_CFLAGS += -D_HAS_TC_SETUP_TYPE
endif

ifneq ($(shell grep -A2 "tcf_block_cb_register" $(LINUXSRC)/include/net/pkt_cls.h | grep "struct netlink_ext_ack" > /dev/null 2>&1 && echo tc_ext_ack),)
        override EXTRA_CFLAGS += -D_HAS_TC_EXT_ACK
endif

ifneq ($(shell grep "ndo_tx_timeout" $(LINUXSRC)/include/linux/netdevice.h | grep "txqueue" > /dev/null 2>&1 && echo ndo_tx_timeout),)
	override EXTRA_CFLAGS += -D_HAS_NDO_TXTO_QUEUE
endif

ifneq ($(shell ls $(LINUXSRC)/include/linux/bpf.h > /dev/null 2>&1 && echo bpf),)
ifneq ($(shell grep "bpf_prog_add" $(LINUXSRC)/include/linux/bpf.h | grep "void" > /dev/null 2>&1 && echo bpf_prog_add),)
	override EXTRA_CFLAGS += -D_HAS_VOID_BPF_PROG_ADD
endif
endif

ifneq ($(shell grep "ndo_setup_tc_rh" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo ndo_setup_tc_rh),)
ifneq ($(shell grep -B50 "\*ndo_setup_tc_rh" $(LINUXSRC)/include/linux/netdevice.h | sed 's/[ ,\t]//gi' | grep net_device_ops_extended > /dev/null 2>&1 && echo ndo_setup_tc_rh),)
	override EXTRA_CFLAGS += -D_HAS_NDO_EXT_SETUP_TC
endif
endif

ifneq ($(shell grep "ndo_xdp" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo ndo_xdp),)
ifneq ($(shell grep -B53 "\*ndo_xdp" $(LINUXSRC)/include/linux/netdevice.h | sed 's/[ ,\t]//gi' | grep net_device_ops_extended > /dev/null 2>&1 && echo ndo_xdp),)
	override EXTRA_CFLAGS += -D_HAS_NDO_EXT_XDP
endif
endif

ifneq ($(shell grep "ndo_dfwd_add_station" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo ndo_dfwd_add_station),)
ifneq ($(shell grep -B50 "\*ndo_dfwd_add_station" $(LINUXSRC)/include/linux/netdevice.h | sed 's/[ ,\t]//gi' | grep net_device_ops_extended > /dev/null 2>&1 && echo ndo_dfwd_add_station),)
        override EXTRA_CFLAGS += -D_HAS_NDO_EXT_DFWD_ADD_STATION
else
        override EXTRA_CFLAGS += -D_HAS_NDO_DFWD_ADD_STATION
endif
endif

ifneq ($(shell grep -A10 "struct macvlan_dev" $(LINUXSRC)/include/linux/if_macvlan.h | grep accel_priv > /dev/null 2>&1 && echo accel_priv),)
	override EXTRA_CFLAGS += -D_HAS_MACVLAN_DEV_ACCEL_PRIV
endif

ifeq ($(shell grep "list_next_entry" $(LINUXSRC)/include/linux/list.h > /dev/null 2>&1 && echo list_next_entry),)
	override EXTRA_CFLAGS += -D_DEFINE_LIST_NEXT_ENTRY
endif

ifneq ($(shell grep "NETDEV_CHANGELOWERSTATE" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo has_bond_offload_support),)
	override EXTRA_CFLAGS += -DHAS_BOND_OFFLOAD_SUPPORT
endif

ifneq ($(shell grep "ethtool_rx_flow_rule" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo has_ethtool_rx_flow_rule),)
	override EXTRA_CFLAGS += -D_HAS_ETH_RX_FLOW_RULE
endif

ifneq ($(shell grep "FLOW_DISSECTOR_KEY_VLAN," $(LINUXSRC)/include/net/flow_dissector.h > /dev/null 2>&1 && echo has_flow_dissector_key_vlan),)
        override EXTRA_CFLAGS += -D_HAS_FLOW_DISSECTOR_KEY_VLAN
endif

ifneq ($(shell grep "mmiowb_spin_lock" $(LINUXSRC)/include/asm-generic/mmiowb.h > /dev/null 2>&1 && echo mmiowb_spin_lock),)
	override EXTRA_CFLAGS += -D_HAS_MMIOWB_SPIN_LOCK
endif

ifneq ($(shell grep "flow_block_offload" $(LINUXSRC)/include/net/flow_offload.h > /dev/null 2>&1 && echo flow_block_offload),)
	override EXTRA_CFLAGS += -D_HAS_FLOW_BLOCK_OFFLOAD
endif

ifneq ($(shell grep "netdev_xmit_more" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo netdev_xmit_more),)
	override EXTRA_CFLAGS += -D_HAS_NDEV_XMIT_MORE
endif

ifeq ($(shell grep "kref_get_unless_zero" $(LINUXSRC)/include/linux/kref.h > /dev/null 2>&1 && echo kref_get_unless_zero),)
	override EXTRA_CFLAGS += -D_DEFINE_KREF_GET_UNLESS_ZERO
endif

ifneq ($(shell grep "devlink_health_reporter_ops" $(LINUXSRC)/include/net/devlink.h > /dev/null 2>&1 && echo devlink_health_reporter_ops),)
        override EXTRA_CFLAGS += -D_HAS_DEVLINK
ifneq ($(shell grep -A2 "devlink_fmsg_binary_pair_put" $(LINUXSRC)/include/net/devlink.h | grep u32 > /dev/null 2>&1 && echo devlink_fmsg_binary_pair_put),)
        override EXTRA_CFLAGS += -D_HAS_DEVLINK_DUMP
endif
endif

ifneq ($(shell awk '/xdp_buff.{/,/^}/'  $(LINUXSRC)/include/net/xdp.h 2>/dev/null | grep frame_sz > /dev/null 2>&1 && echo frame_sz),)
	override EXTRA_CFLAGS += -D_HAS_XDP_FRAME_SZ
endif

ifneq ($(shell grep "supported_coalesce_params" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo supported_coalesce),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_SUPPORTED_COALESCE_PARAMS
endif

ifneq ($(shell grep "kernel_ethtool_ringparam" $(LINUXSRC)/include/linux/ethtool.h > /dev/null 2>&1 && echo kernel_ethtool_ringparam),)
	override EXTRA_CFLAGS += -D_HAS_ETHTOOL_KERNEL_ERING
endif

ifeq ($(shell grep "reset_prepare" $(LINUXSRC)/include/linux/pci.h > /dev/null 2>&1 || echo "reset_prepare"),)
	override EXTRA_CFLAGS += -DSYS_INC_RESET_PREP
endif

ifneq ($(shell grep "pci_cleanup_aer_uncorrect_error_status" $(LINUXSRC)/include/linux/aer.h > /dev/null 2>&1 && echo pci_cleanup_aer_uncorrect_error_status),)
        override EXTRA_CFLAGS += -D_HAS_CLEANUP_AER_ERROR
endif

ifneq ($(shell sed -n -E "/\(\*get_coalesce\)/,/\;/p" $(LINUXSRC)/include/linux/ethtool.h 2>&1  | grep kernel_ethtool_coalesce > /dev/null 2>&1 && echo kernel_ethtool_coalesce),)
	override EXTRA_CFLAGS += -D_HAS_KERNEL_ETHTOOL_COALESCE
endif

ifneq ($(shell grep  "irq_set_affinity_hint" $(LINUXSRC)/include/linux/interrupt.h > /dev/null 2>&1 && echo irq_set_affinity_hint),)
	ifneq ($(shell grep  "cpumask_local_spread" $(LINUXSRC)/include/linux/cpumask.h > /dev/null 2>&1 && echo cpumask_local_spread),)
		override EXTRA_CFLAGS += -D_HAS_IRQ_SET_AFFINITY_HINT
	endif
endif

ifneq ($(shell grep "NETIF_F_GSO_UDP_TUNNEL_CSUM" $(LINUXSRC)/include/linux/netdev_features.h > /dev/null 2>&1 && echo NETIF_F_GSO_UDP_TUNNEL_CSUM),)
	override EXTRA_CFLAGS += -D_HAS_GSO_TUN_L4_CSUM
endif

ifeq ($(ROCE), 1)
	override EXTRA_CFLAGS += -DCONFIG_QED_RDMA
endif

DRV_VER_SUFFIX = "-"${USER}
EXTRA_CFLAGS += -DQEDE_MOD_VER=\"$(DRV_VER_SUFFIX)\"

# targets and rules
.PHONY: all modules_install initramfs_install clean clobber light_install install list_sources #docs_install

QED_DIR ?=
ifeq ($(QED_DIR),)
	QED_DIR = $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))/../qed
endif

KBUILD_EXTRA_SYMBOLS := $(QED_DIR)/Module.symvers

all default: $(obj-m)

$(obj-m): $(SOURCES)
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` modules

modules_install: $(obj-m)
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` INSTALL_MOD_PATH=$(PREFIX) INSTALL_MOD_DIR=$(DRV_DIR) modules_install 2>&1 | grep -v depmod
	@if [ "$(sles_distro)" = "" -a "$(ubuntu_distro)" = "" ]; then \
		echo "override qede * $(DRV_DIR)" > qede.conf ;\
		install -m 644 -D qede.conf $(PREFIX)/etc/depmod.d/qede.conf ;\
	fi
	@if [ "$(PREFIX)" = "" ] ;\
		then /sbin/depmod -a ;\
		else echo " *** Run '/sbin/depmod -a' to update the module database." ;\
	fi

initramfs_install: $(obj-m)
	@if [ -f /boot/initramfs-$(KVER).img ] && [ "`lsinitrd /boot/initramfs-$(KVER).img | grep qede.ko`" ] ; then\
		echo "back up initrd to /boot/initramfs-$(KVER).img.bak" ;\
		cp /boot/initramfs-$(KVER).img /boot/initramfs-$(KVER).img.bak ;\
		echo "add driver qede to initrd" ;\
		dracut -f --add-drivers qede ;\
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
		mkinitrd -k /boot/vmlinuz-$(KVER) -i /boot/initrd-$(KVER) -m qede &>/dev/null;\
	else \
		echo "warning: initrd isn't being updated"; \
	fi

#docs_install:
#	gzip -c qede.4 > qede.4.gz
#	install -m 644 qede.4.gz $(MAN_DIR)

clean:
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` clean

clobber: clean

light_install: modules_install

install: modules_install initramfs_install #docs_install

# an option to get all sources required for compilation, used in OFED packaging
list_sources:
	@echo $(realpath $(SOURCES)) | tr " " "\n"

-include Makefile.gen
