ifeq ($(QEDE_DIR),)
	override QEDE_DIR = $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))/../../qede-8.70.12.0/src
endif
#!/usr/bin/make
# Makefile for building Linux Broadcom 40-Gigabit ethernet driver as a module.

KVER =
ifeq ($(KVER),)
    KVER = $(shell uname -r)
endif

KOBJ = /lib/modules/$(KVER)/build

__ARCH=$(shell uname -m)


#
# determine OFED type (OOB/INBOX) and version
#
ifeq ($(OOB_OFED),)
    ifeq ($(wildcard /usr/bin/ofed_info),)
        OOB_OFED=0
    else
        OOB_OFED=1
        OFED_VERSION = $(shell ofed_info 2>/dev/null | head -1 | grep -o [0-9\.]* | head -1)
        ifeq ("MLNX_OFED_LINUX",$(filter "MLNX_OFED_LINUX",$(OFED_VERSION)))
            ifeq (,$(filter 3.0,$(OFED_VERSION)))
                $(error only MLNX_OFED OOB versions 3.0 and is (not officialy) supported (found $(OFED_VERSION)))
            endif
            OOB_OFED_TYPE=MLX
            OOB_OFED_COMPAT_INCLUDE = -include /usr/src/ofa_kernel/default/include/linux/compat-2.6.h
            ib_verbs_h = /usr/src/ofa_kernel/default/include/rdma/ib_verbs.h
	    ib_umem_h = /usr/src/ofa_kernel/default/include/rdma/ib_umem.h
	    iw_cm_h = /usr/src/ofa_kernel/default/include/rdma/iw_cm.h
            
        else
            ifeq (,$(filter 3.12 3.18 4.8 4.17,$(OFED_VERSION)))
                $(error only OFED OOB versions 3.12, 3.18 and 4.8 are supported (found $(OFED_VERSION)))
            endif
            OOB_OFED_TYPE=OFA
            ifeq (3.12 , $(filter 3.12 ,$(OFED_VERSION)))
		COMPAT_DIR_SUFFIX=""
	    else ifeq (3.18 , $(filter 3.18 ,$(OFED_VERSION)))
		COMPAT_DIR_SUFFIX="-3.18"
	    else ifeq (4.8 , $(filter 4.8 ,$(OFED_VERSION)))
		COMPAT_DIR_SUFFIX="-4.8"
	    else ifeq (4.17 , $(filter 4.17 ,$(OFED_VERSION)))
		COMPAT_DIR_SUFFIX="-4.17"
	    else ifeq ( "1" , "1" )
	    	$(error only OFED OOB versions 3.12, 3.18 4.8 and 4.17 are supported (found $(OFED_VERSION)))
	    endif
            OOB_OFED_COMPAT_INCLUDE = -include /usr/src/compat-rdma$(COMPAT_DIR_SUFFIX)/include/linux/compat-2.6.h
            ib_verbs_h = /usr/src/compat-rdma$(COMPAT_DIR_SUFFIX)/include/rdma/ib_verbs.h
            ib_umem_h = /usr/src/compat-rdma$(COMPAT_DIR_SUFFIX)/include/rdma/ib_umem.h
            iw_cm_h = /usr/src/compat-rdma$(COMPAT_DIR_SUFFIX)/include/rdma/iw_cm.h
            ib_pack_h = /usr/src/compat-rdma$(COMPAT_DIR_SUFFIX)/include/rdma/ib_pack.h
        endif
    endif
endif

# PREFIX may be set by the RPM build to set the effective root.
PREFIX =
ifeq ($(shell ls $(KOBJ) > /dev/null 2>&1 && echo build),"aaa")
# SuSE source RPMs
    _KVER=$(shell echo $(KVER) | cut -d "-" -f1,2)
    _KFLA=$(shell echo $(KVER) | cut -d "-" -f3)
    _ARCH=$(shell file -b $(KOBJ) | cut -d "/" -f5)
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
    BCMCFGS := $(KOBJ) \
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

SOURCES = main.c verbs.c qedr_roce_cm.c qedr_iw_cm.c verbs.h qedr.h qedr_hsi_rdma.h qedr_user.h qedr_roce_cm.h qedr_iw_cm.h qedr_compat.h qedr_debugfs.c qedr_debugfs.h qedr_compat.c qedr_gdr.h

MODULE_NAME = qedr

override EXTRA_CFLAGS += -DCONFIG_QED_LL2 -DDRV_MODULE_NAME=\"$(MODULE_NAME)\"

ifneq ($(WITHOUT_ROCE),1)
	obj-m += $(MODULE_NAME).o 
endif

ifeq ($(WITH_GDR), 1)
	SOURCES += qedr_gdr.c
	override EXTRA_CFLAGS += -DDEFINE_WITH_GDR
endif

qedr-objs = $(patsubst %.c,%.o,$(filter %.c, $(SOURCES)))

# The && / || logic is very peculiar, and works opposite to what is expected, I.e., if The command running
# has an exit value of 0 [henceforth 'RES'], then RES && echo "hello world" WILL PRINT while RES || echo ...
# will not. Someone needs to figure out exactly the makefile/shell evaluations and fix this...

ifeq ($(DISABLE_WERROR),)
        override EXTRA_CFLAGS += -Werror
endif

ifneq ($(sles_distro),)
SLES_VERSION = $(shell cat /etc/SuSE-release | grep VERSION | grep -o -P [0-9]+)
SLES_PATCHLEVEL = $(shell cat /etc/SuSE-release | grep PATCHLEVEL | grep -o -P [0-9]+)
PADDED_PATCHLEVEL = $(shell if [ 10 -gt $(SLES_PATCHLEVEL) ]; then echo 0$(SLES_PATCHLEVEL); else echo $(SLES_PATCHLEVEL); fi)
SLES_DISTRO_VER = "0x$(SLES_VERSION)$(PADDED_PATCHLEVEL)"
override EXTRA_CFLAGS += -DSLES_DISTRO=$(SLES_DISTRO_VER)
endif


#
# determine if RHEL distro
#
ifneq ($(wildcard /etc/redhat-release),)
ifeq ($(shell cat /etc/redhat-release | grep "Fedora" && echo Fedora),)
RHEL_VER = $(shell cat /etc/redhat-release | grep -o -P [0-9.]+)
RHEL_BASE_VER = $(shell echo $(RHEL_VER) | cut -f1 -d .)
RHEL_SUB_VER = $(shell echo $(RHEL_VER) | cut -f2 -d .)
ifneq ($(shell [[ "$(RHEL_BASE_VER)" -lt "6" || "$(RHEL_BASE_VER)" -eq "6" && "$(RHEL_SUB_VER)" -lt "2" ]] && echo not supported),)
$(error QEDR cannot be built with RHEL version lower than 6.2 (detected $(RHEL_VER)))
endif
endif
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

DRV_VER_SUFFIX = "-"${USER}
EXTRA_CFLAGS += -DQEDR_MOD_VER=\"$(DRV_VER_SUFFIX)\"

# targets and rules
.PHONY: all modules_install initramfs_install clean clobber light_install install list_sources #docs_install

autoconf_h=$(shell /bin/ls -1 $(LINUXSRC)/include/*/autoconf.h 2> /dev/null | head -1)
ifneq ($(autoconf_h),)
	OOB_OFED_AUTOCONF_INCLUDE = -include $(autoconf_h)
endif

kconfig_h=$(shell /bin/ls -1 $(LINUXSRC)/include/*/kconfig.h 2> /dev/null | head -1)
ifneq ($(kconfig_h),)
	OOB_OFED_KCONFIG_INCLUDE = -include $(kconfig_h)
endif

ib_verbs_h?=$(shell /bin/ls -1 $(LINUXSRC)/include/*/ib_verbs.h 2> /dev/null | head -1)
iw_cm_h?=$(shell /bin/ls -1 $(LINUXSRC)/include/*/iw_cm.h 2> /dev/null | head -1)
ib_umem_h?=$(shell /bin/ls -1 $(LINUXSRC)/include/*/ib_umem.h 2> /dev/null | head -1)
ib_cache_h?=$(shell /bin/ls -1 $(LINUXSRC)/include/*/ib_cache.h 2> /dev/null | head -1)
if_inet6_h?=$(shell /bin/ls -1 $(LINUXSRC)/include/*/if_inet6.h 2> /dev/null | head -1)
netdevice_h?=$(shell /bin/ls -1 $(LINUXSRC)/include/*/netdevice.h 2> /dev/null | head -1)
ib_pack_h?=$(LINUXSRC)/include/rdma/ib_pack.h

QED_DIR ?=
ifeq ($(QED_DIR),)
	QED_DIR = $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))/../qed
endif

QEDE_DIR ?=
ifeq ($(QEDE_DIR),)
	QEDE_DIR = $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))/../qede
endif
KBUILD_EXTRA_SYMBOLS := $(QED_DIR)/Module.symvers $(QEDE_DIR)/Module.symvers

OOB_OFED_PATH = 

ifeq ($(OOB_OFED), 1)
	OFA_DIR=/usr/src/compat-rdma
	OFA = $(shell test -d $(OFA_DIR)/default && echo $(OFA_DIR)/default || (test -d /var/lib/dkms/compat-rdma/ && ls -d /var/lib/dkms/compat-rdma/*/build) || echo $(OFA_DIR))
	ifneq ($(shell test -d $(OFA) && echo YES || echo ""),)
		include $(OFA)/configure.mk.kernel
		INCLUDE_COMPAT = -include $(OFA)/include/linux/compat-2.6.h
	endif

	KBUILD_EXTRA_SYMBOLS += $(OFA)/Module*.symvers
	autoconf_h=$(shell /bin/ls -1 $(KOBJ)/include/*/autoconf.h 2> /dev/null | head -1)
	ofa_autoconf_h=$(shell /bin/ls -1 $(OFA)/include/*/autoconf.h 2> /dev/null | head -1)

        ifneq ($(ofa_autoconf_h),)
                OFA_AUTOCONF_H = -include $(ofa_autoconf_h)
        endif

	OOB_OFED_PATH=LINUXINCLUDE=' \
		-D__OFED_BUILD__ \
		-include $(autoconf_h) \
		$(OFA_AUTOCONF_H) \
		$(INCLUDE_COMPAT) \
		-I$(OFA)/include \
		-I$(OFA)/include/uapi \
		-I$$(srctree)/arch/$$(SRCARCH)/include \
		-Iarch/$$(SRCARCH)/include/generated \
		-Iinclude \
		-I$$(srctree)/arch/$$(SRCARCH)/include/uapi \
		-Iarch/$$(SRCARCH)/include/generated/uapi \
		-I$$(srctree)/include \
		-I$$(srctree)/include/uapi \
		-Iinclude/generated/uapi \
		$$(if $$(KBUILD_SRC),-Iinclude2 -I$$(srctree)/include) \
		-I$$(srctree)/arch/$$(SRCARCH)/include \
		-Iarch/$$(SRCARCH)/include/generated \
		' \
		modulesymfile=$(TEST_SRC)/Module.symvers \
		modules
	

endif

ifneq ($(shell grep "RDMA_NETWORK_ROCE_V1" $(ib_verbs_h)  > /dev/null 2>&1 && echo RDMA_NETWORK_ROCE_V1),)
	     override EXTRA_CFLAGS += -D_HAS_RDMA_NETWORK_ROCE_V1
endif

ifneq ($(shell awk '/ib_device_attr {/,/^}/' $(ib_verbs_h) >/dev/null | grep fmr > /dev/null 2>&1 && echo fmr),)
        override EXTRA_CFLAGS += -D_HAS_FMR_SUPPORT
endif

ifeq ($(shell grep -A2 ib_modify_qp_is_ok $(ib_verbs_h) | grep rdma_link_layer > /dev/null 2>&1 && echo modify_ok_with_lltype),)
	override EXTRA_CFLAGS += -DDEFINE_QP_MODIFY_OK_WITHOUT_LLTYPE
endif

ifneq ($(shell grep -A10 "struct.*ib_ah_attr.*{" $(ib_verbs_h) | grep dmac > /dev/null 2>&1 && echo ip_based_gids),)
override EXTRA_CFLAGS += -DDEFINE_IB_AH_ATTR_WITH_DMAC=1
ifeq ($(shell grep -A10 "struct.*ib_ah_attr.*{" $(ib_verbs_h) | grep vlan_id > /dev/null 2>&1 && echo ip_based_gids),)
override EXTRA_CFLAGS += -DDEFINE_NO_VLAN_IN_GIDS
endif
else
override EXTRA_CFLAGS += -DDEFINE_NO_IP_BASED_GIDS
override EXTRA_CFLAGS += -DDEFINE_IB_AH_ATTR_WITH_DMAC=0
endif
ifneq ($(shell grep "rdma_protocol_iwarp" $(ib_verbs_h) > /dev/null 2>&1 && echo rdma_protocol),)
override EXTRA_CFLAGS += -DDEFINE_RDMA_PROTOCOL
endif

ifneq ($(shell grep "ib_umem_chunk" $(ib_umem_h) > /dev/null 2>&1 && echo ib_umem_with_chunk),)
	override EXTRA_CFLAGS += -DDEFINE_IB_UMEM_WITH_CHUNK
endif

ifneq ($(shell grep "hugetlb" $(ib_umem_h) > /dev/null 2>&1 && echo ib_umem_hugetlb),)
	override EXTRA_CFLAGS += -D_IB_UMEM_HUGETLB
endif

ifneq ($(shell grep "dmasync" $(ib_umem_h) > /dev/null 2>&1 && echo ib_umem_dmasync),)
	override EXTRA_CFLAGS += -D_HAS_IB_UMEM_GET_DMASYNC
endif

ifneq ($(shell grep "rdma_umem_for_each_dma_block" $(ib_umem_h) > /dev/null 2>&1 && echo rdma_umem_for_each_dma_block),)
	        override EXTRA_CFLAGS += -D_HAS_UMEM_DMA_BLOCK
endif

ifeq ($(shell grep -A6 "struct ib_umem {" $(ib_umem_h) | grep -e page_shift -e page_size > /dev/null 2>&1 && echo ib_umem_no_pg),)
	override EXTRA_CFLAGS += -DDEFINE_IB_UMEM_NO_PAGE_PARAM
endif

ifeq ($(shell grep -A30 "^struct.*inet6_ifaddr.*{" $(if_inet6_h) | grep if_list > /dev/null 2>&1 && echo ifnet_with_if_list),)
        override EXTRA_CFLAGS += -DDEFINE_IFNET6_WITHOUT_IF_LIST
endif

ifeq ($(shell grep -A5 "reg_user_mr" $(ib_verbs_h) | grep mr_id > /dev/null 2>&1 && echo with_mr_id),)
        override EXTRA_CFLAGS += -DDEFINE_USER_NO_MR_ID
endif

ifeq ($(shell grep "*add_gid" $(ib_verbs_h) | grep "device" > /dev/null 2>&1 && echo remove_gid_add/del),)
	override EXTRA_CFLAGS += -DREMOVE_DEVICE_ADD_DEL_GID
endif

ifneq ($(shell grep "int (\*\create_srq)" $(ib_verbs_h) > /dev/null 2>&1 && echo has_srq_allocation),)
        override EXTRA_CFLAGS += -D_HAS_SRQ_ALLOCATION
endif

ifneq ($(shell grep "(\*\destroy_srq)" $(ib_verbs_h) | grep void  > /dev/null 2>&1 && echo has_destroy_srq_void_return),)
        override EXTRA_CFLAGS += -D_HAS_DESTROY_SRQ_VOID_RETURN
endif

ifneq ($(shell grep "(\*\destroy_qp)" $(ib_verbs_h) | grep udata > /dev/null 2>&1 && echo udata),)
        override EXTRA_CFLAGS += -D_DESTROY_SRQ_HAS_UDATA
endif

ifeq ($(shell grep -A7 "struct.*ib_srq_init_attr.*{" $(ib_verbs_h) | grep "struct ib_cq"> /dev/null 2>&1 && echo ib_cq),)
        override EXTRA_CFLAGS += -DDEFINE_CQ_IN_XRC
endif

ifneq ($(shell grep "IB_QPT_XRC_TGT" $(ib_verbs_h) > /dev/null 2>&1 && echo xrc),)
        override EXTRA_CFLAGS += -D_HAS_XRC_SUPPORT
endif

ifneq ($(shell grep -A2 "ib_register_device" $(ib_verbs_h) | grep name > /dev/null 2>&1 && echo ib_register_device),)
        override EXTRA_CFLAGS += -D_HAS_IB_REGISTER_DEVICE_NAME
endif

ifneq ($(shell grep -A2 "ib_register_device" $(ib_verbs_h) | grep port_callback > /dev/null 2>&1 && echo ib_register_device),)
        override EXTRA_CFLAGS += -D_HAS_IB_REGISTER_DEVICE_PORT_CALLBACK
endif

ifneq ($(shell grep -A2 "ib_alloc_device" $(ib_verbs_h) | grep drv_struct > /dev/null 2>&1 && echo ib_alloc_device),)
        override EXTRA_CFLAGS += -D_HAS_SAFE_IB_ALLOC
endif

ifneq ($(shell awk '/struct ib_umem.{/,/^}/' $(ib_umem_h) 2>/dev/null | grep address > /dev/null 2>&1 && echo with_umem_address_not_ofset),)
        override EXTRA_CFLAGS += -DDEFINE_UMEM_ADDRESS_TO_OFFSET
endif

ifneq ($(shell grep -A8 "^struct ib_umem.*{" $(ib_umem_h) | grep page_shift > /dev/null 2>&1 && echo page_shift),)
        override EXTRA_CFLAGS += -DDEFINE_IB_UMEM_PAGE_SHIFT=1
else
        override EXTRA_CFLAGS += -DDEFINE_IB_UMEM_PAGE_SHIFT=0
endif

ifneq ($(shell grep "ib_umem_get" $(ib_umem_h) | grep udata > /dev/null 2>&1 && echo page_shift),)
        override EXTRA_CFLAGS += -D_HAS_IB_UMEM_GET_UDATA
endif

ifneq ($(shell grep "ib_umem_get" $(ib_umem_h) | grep ib_device > /dev/null 2>&1 && echo page_shift),)
        override EXTRA_CFLAGS += -D_HAS_IB_UMEM_GET_IBDEV
endif

ifneq ($(shell grep "for_each_netdev_rcu" $(netdevice_h) > /dev/null 2>&1 && echo with_for_each_netdev_rcu),)
        override EXTRA_CFLAGS += -DDEFINE_NETDEV_RCU
endif

ifneq ($(shell grep -A6 -e "^struct ib_cq \*ib_create_cq" -e "define ib_create_cq" $(ib_verbs_h) | grep -e ib_cq_init_attr -e cq_attr > /dev/null 2>&1 && echo compact_create_cq),)
        override EXTRA_CFLAGS += -DDEFINE_CREATE_CQ_ATTR
endif

ifneq ($(shell grep "int (\*\create_cq)" $(ib_verbs_h)  > /dev/null 2>&1 && echo has_cq_allocation),)
        override EXTRA_CFLAGS += -D_HAS_CQ_ALLOCATION
endif

ifneq ($(shell grep "(\*\destroy_cq)" $(ib_verbs_h) | grep udata > /dev/null 2>&1 && echo udata),)
        override EXTRA_CFLAGS += -D_DESTROY_CQ_HAS_UDATA
endif

ifneq ($(shell grep "(\*\destroy_cq)" $(ib_verbs_h) | grep void > /dev/null 2>&1 && echo destroy_cq_void_return),)
        override EXTRA_CFLAGS += -D_DESTROY_CQ_HAS_VOID_RETURN
endif

ifneq ($(shell grep "(\*\destroy_qp)" $(ib_verbs_h) | grep udata > /dev/null 2>&1 && echo udata),)
        override EXTRA_CFLAGS += -D_DESTROY_QP_HAS_UDATA
endif

ifneq ($(shell grep -A3 "(\*query_device)" $(ib_verbs_h) | grep udata > /dev/null 2>&1 && echo query_device_udata),)
        override EXTRA_CFLAGS += -DDEFINE_QUERY_DEVICE_PASS_VENDOR_SPECIFIC_DATA
endif

ifneq ($(shell grep -A4 "(\*process_mad)" $(ib_verbs_h) | grep "const struct ib_wc" > /dev/null 2>&1 && echo process_mad_variable_size),)
        override EXTRA_CFLAGS += -DDEFINE_PROCESS_MAD_CONST_INPUTS
endif

ifneq ($(shell grep -A6 "(\*process_mad)" $(ib_verbs_h) | grep "const struct ib_mad_hdr" > /dev/null 2>&1 && echo process_mad_const_ib_mad_hdr),)
        override EXTRA_CFLAGS += -DDEFINE_PROCESS_MAD_CONST_IB_MAD_HDR
endif

ifneq ($(shell grep -A9 "(\*process_mad)" $(ib_verbs_h) | grep out_mad_pkey_index > /dev/null 2>&1 && echo process_mad_variable_size),)
        override EXTRA_CFLAGS += -DDEFINE_PROCESS_MAD_VARIABLE_SIZE
endif

ifneq ($(shell grep -A2 "get_port_immutable" $(ib_verbs_h) | grep "struct ib_port_immutable"> /dev/null 2>&1 && echo port_immutable),)
        override EXTRA_CFLAGS += -DDEFINE_PORT_IMMUTABLE=1
else
        override EXTRA_CFLAGS += -DDEFINE_PORT_IMMUTABLE=0
endif

ifneq ($(shell grep "get_dev_fw_str" $(ib_verbs_h) > /dev/null 2>&1 && echo get_dev_fw_str),)
        override EXTRA_CFLAGS += -DDEFINE_GET_DEV_FW_STR=1
else
        override EXTRA_CFLAGS += -DDEFINE_GET_DEV_FW_STR=0
endif

ifneq ($(shell grep "IB_FW_VERSION_NAME_MAX" $(ib_verbs_h) > /dev/null 2>&1 && echo get_dev_fw_str),)
        override EXTRA_CFLAGS += -DDEFINE_GET_DEV_FW_STR_FIX_LEN=1
else
        override EXTRA_CFLAGS += -DDEFINE_GET_DEV_FW_STR_FIX_LEN=0
endif

ifneq ($(shell grep -A19 "enum ib_event_type {" $(ib_verbs_h) | grep "IB_EVENT_GID_CHANGE,"> /dev/null 2>&1 && echo event_on_gid_change),)
        override EXTRA_CFLAGS += -DDEFINE_EVENT_ON_GID_CHANGE=1
else
        override EXTRA_CFLAGS += -DDEFINE_EVENT_ON_GID_CHANGE=0
endif

ifeq ($(shell grep "simple_open" $(LINUXSRC)/include/linux/fs.h > /dev/null 2>&1 && echo simple_open),)
        override EXTRA_CFLAGS += -DDEFINE_SIMPLE_OPEN=1
else
        override EXTRA_CFLAGS += -DDEFINE_SIMPLE_OPEN=0
endif

ifneq ($(shell grep "IB_SPEED_SDR" $(ib_verbs_h) > /dev/null 2>&1 && echo ib_port_speed),)
        override EXTRA_CFLAGS += -DDEFINE_IB_PORT_SPEED=1
else
        override EXTRA_CFLAGS += -DDEFINE_IB_PORT_SPEED=0
endif

ifneq ($(shell grep "IB_SPEED_HDR" $(ib_verbs_h) > /dev/null 2>&1 && echo ib_port_speed),)
        override EXTRA_CFLAGS += -DDEFINE_IB_PORT_SPEED_50G
endif

ifneq ($(shell grep "rdma_cap_roce_gid_table" $(ib_verbs_h) > /dev/null 2>&1 && echo rdma_cap_roce_gid_table),)
        override EXTRA_CFLAGS += -DDEFINE_ROCE_GID_TABLE=1
else
        override EXTRA_CFLAGS += -DDEFINE_ROCE_GID_TABLE=0
endif

ifneq ($(shell grep "IB_GID_TYPE_ROCE_UDP_ENCAP" $(ib_verbs_h) > /dev/null 2>&1 && echo rdma_cap_roce_gid_table),)
        override EXTRA_CFLAGS += -DDEFINE_ROCE_V2_SUPPORT=1
else
        override EXTRA_CFLAGS += -DDEFINE_ROCE_V2_SUPPORT=0
endif

ifneq ($(shell grep "pcie_capability_read_dword" $(LINUXSRC)/include/linux/pci.h > /dev/null 2>&1 && echo pcie_capability_ops),)
        override EXTRA_CFLAGS += -DDEFINE_PCIE_CAPABILITY_OPS=1
else
        override EXTRA_CFLAGS += -DDEFINE_PCIE_CAPABILITY_OPS=0
endif

ifneq ($(shell grep "reg_phys_mr" $(ib_verbs_h) > /dev/null 2>&1 && echo reg_phys_mr),)
	override EXTRA_CFLAGS += -DDEFINE_REG_PHYS_MR
endif

ifneq ($(shell grep "alloc_mr" $(ib_verbs_h) > /dev/null 2>&1 && echo alloc_mr),)
	override EXTRA_CFLAGS += -DDEFINE_ALLOC_MR
endif

ifneq ($(shell grep "get_netdev" $(ib_verbs_h) > /dev/null 2>&1 && echo get_netdev),)
	override EXTRA_CFLAGS += -DDEFINE_GET_NETDEV
endif

ifneq ($(shell grep "map_mr_sg" $(ib_verbs_h) > /dev/null 2>&1 && echo map_mr_sg),)
	override EXTRA_CFLAGS += -DDEFINE_MAP_MR_SG
endif

ifneq ($(shell grep -A3 "map_mr_sg" $(ib_verbs_h) |  grep "unsigned int sg_nents" > /dev/null 2>&1 && echo map_mr_sg),)
	override EXTRA_CFLAGS += -DDEFINE_MAP_MR_SG_UNSIGNED
endif

ifneq ($(shell grep -A3 "map_mr_sg" $(ib_verbs_h) |  grep "sg_offset" > /dev/null 2>&1 && echo map_mr_sg),)
	override EXTRA_CFLAGS += -DDEFINE_MAP_MR_SG_OFFSET
endif

ifneq ($(shell grep -A1 "(\*\alloc_mr)" $(ib_verbs_h) | grep udata > /dev/null 2>&1 && echo udata),)
        override EXTRA_CFLAGS += -D_ALLOC_MR_HAS_UDATA
endif

ifneq ($(shell grep "(\*\dereg_mr)" $(ib_verbs_h) | grep udata > /dev/null 2>&1 && echo udata),)
        override EXTRA_CFLAGS += -D_DEREG_MR_HAS_UDATA
endif

ifeq ($(shell grep "ib_ud_wr" $(ib_verbs_h) > /dev/null 2>&1 && echo ib_ud_wr),)
	override EXTRA_CFLAGS += -DNOT_DEFINED_IB_UD_WR
endif

ifeq ($(shell grep "ib_atomic_wr" $(ib_verbs_h) > /dev/null 2>&1 && echo ib_atomic_wr),)
	override EXTRA_CFLAGS += -DNOT_DEFINED_IB_ATOMIC_WR
endif

ifeq ($(shell grep "ib_rdma_wr" $(ib_verbs_h) > /dev/null 2>&1 && echo ib_rdma_wr),)
	override EXTRA_CFLAGS += -DNOT_DEFINED_IB_RDMA_WR
endif

ifeq ($(shell grep "ib_unpacked_ip4" $(ib_pack_h) > /dev/null 2>&1 && echo ib_unpacked_ip4),)
	override EXTRA_CFLAGS += -DDEFINE_IB_IP4_STRUCT_NOTDEFINED
endif

ifneq ($(shell grep -A3 "create_ah" $(ib_verbs_h) | grep udata > /dev/null 2>&1 && echo udata),)
	override EXTRA_CFLAGS += -D_HAS_CREATE_AH_UDATA
endif

ifneq ($(shell grep -A3 "create_ah" $(ib_verbs_h) | grep flags > /dev/null 2>&1 && echo flags),)
        override EXTRA_CFLAGS += -D_HAS_CREATE_AH_FLAGS
endif

ifneq ($(shell grep "int (\*\create_ah)" $(ib_verbs_h) > /dev/null 2>&1 && echo has_ah_allocation),)
        override EXTRA_CFLAGS += -D_HAS_AH_ALLOCATION
endif

ifneq ($(shell grep "rdma_ah_init_attr" $(ib_verbs_h)  > /dev/null 2>&1 && echo rdma_ah_init_attr),)
        override EXTRA_CFLAGS += -D_HAS_AH_INIT_ATTR
endif

ifneq ($(shell grep "(\*\destroy_ah)" $(ib_verbs_h) | grep void > /dev/null 2>&1 && echo has_destroy_ah_void),)
        override EXTRA_CFLAGS += -D_DESTROY_AH_HAS_VOID_RETURN
endif

ifneq ($(shell grep "ib_mw_type" $(ib_verbs_h) > /dev/null 2>&1 && echo alloc_mw),)
	override EXTRA_CFLAGS += -D_HAS_MW_SUPPORT
endif

ifneq ($(shell grep -A2 "(\*\alloc_mw)" $(ib_verbs_h) | grep ib_udata > /dev/null 2>&1 && echo alloc_mw),)
        ifneq ($(shell grep -A2 "(\*\alloc_mw)" $(ib_verbs_h) | grep ib_mw_type > /dev/null 2>&1 && echo alloc_mw_v2),)
        override EXTRA_CFLAGS += -D_HAS_ALLOC_MW_V2
else
        override EXTRA_CFLAGS += -D_HAS_ALLOC_MW_V3	
endif
endif

ifneq ($(shell grep -A8 "ib_ud_header_init" $(ib_pack_h) | grep udp_present > /dev/null 2>&1 && echo udp_present),)
        override EXTRA_CFLAGS += -DDEFINE_IB_UD_HEADER_INIT_UDP_PRESENT
endif

ifneq ($(shell grep "ib_fast_reg_page_list" $(ib_verbs_h) > /dev/null 2>&1 && echo ib_fast_reg_page_list),)
	override EXTRA_CFLAGS += -DDEFINE_IB_FAST_REG
endif

ifneq ($(shell grep "IB_WC_WITH_SMAC" $(ib_verbs_h) > /dev/null 2>&1 && echo ib_fast_reg_page_list),)
	override EXTRA_CFLAGS += -DDEFINED_IB_WC_WITH_SMAC=1
else
	override EXTRA_CFLAGS += -DDEFINED_IB_WC_WITH_SMAC=0
endif

ifneq ($(shell grep "IB_WC_IP_CSUM_OK" $(ib_verbs_h) > /dev/null 2>&1 && echo ib_fast_reg_page_list),)
	override EXTRA_CFLAGS += -DDEFINED_IB_WC_IP_CSUM_OK=1
else
	override EXTRA_CFLAGS += -DDEFINED_IB_WC_IP_CSUM_OK=0
endif

ifneq ($(shell grep "IB_WC_WITH_VLAN" $(ib_verbs_h) > /dev/null 2>&1 && echo ib_fast_reg_page_list),)
	override EXTRA_CFLAGS += -DDEFINED_IB_WC_WITH_VLAN=1
else
	override EXTRA_CFLAGS += -DDEFINED_IB_WC_WITH_VLAN=0
endif


ifneq ($(shell grep "ib_mtu_int_to_enum" $(ib_verbs_h) > /dev/null 2>&1 && echo ib_mtu_int_to_enum),)
	override EXTRA_CFLAGS += -DDEFINED_IB_MTU_TO_ENUM
endif

ifneq ($(shell grep "rdma_ah_attr" $(ib_verbs_h) > /dev/null 2>&1 && echo rdma_ah_attr),)
	override EXTRA_CFLAGS += -DDEFINED_RDMA_AH_ATTR=1
else
	override EXTRA_CFLAGS += -DDEFINED_RDMA_AH_ATTR=0
endif

ifeq ($(shell grep "ib_get_cached_gid" $(ib_cache_h) > /dev/null 2>&1 && echo ib_get_cached_gid),)
	override EXTRA_CFLAGS += -DNOT_DEFINED_GET_CACHED_GID
endif

ifneq ($(shell grep "rdma_read_gid_l2_fields" $(ib_cache_h) > /dev/null 2>&1 && echo rdma_read_gid_l2_fields),)
	override EXTRA_CFLAGS += -DDEFINED_GET_L2_FIELDS
endif

ifneq ($(shell grep "adj_list" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo adj_list),)
	override EXTRA_CFLAGS += -DDEFINED_NETDEV_ADJ_LIST
endif

ifneq ($(shell grep "netdev_walk_all_lower_dev_rcu" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo netdev_walk_all_lower_dev_rcu),)
	override EXTRA_CFLAGS += -DDEFINED_NETDEV_WALK_LOWER
endif

ifneq ($(shell grep "rdma_wr" $(ib_verbs_h) | grep "const"  > /dev/null 2>&1 && echo rdma_const),)
	override EXTRA_CFLAGS += -DRDMA_CONST=const
else
	override EXTRA_CFLAGS += -DRDMA_CONST=
endif

ifneq ($(shell grep -A1 "ib_post_send" $(ib_verbs_h) | grep "const"  > /dev/null 2>&1 && echo ib_const),)
	override EXTRA_CFLAGS += -DIB_CONST=const
else
	override EXTRA_CFLAGS += -DIB_CONST=
endif

ifneq ($(shell grep "rdma_copy_ah_attr" $(ib_verbs_h) > /dev/null 2>&1 && echo rdma_copy_ah_attr),)
	 override EXTRA_CFLAGS += -DDEFINED_COPY_AH_ATTR
endif

ifneq ($(shell grep "sgid_attr" $(ib_verbs_h) > /dev/null 2>&1 && echo sgid_attr),)
	override EXTRA_CFLAGS += -DDEFINED_SGID_ATTR
	override EXTRA_CFLAGS += -DSGID_CONST=const
else
	override EXTRA_CFLAGS += -DSGID_CONST=
endif

ifneq ($(shell grep -A 15 "ib_device_attr" $(ib_verbs_h) | grep "max_send_sge" > /dev/null 2>&1 && echo max_send_recv_sge),)
	override EXTRA_CFLAGS += -DDEFINE_MAX_SEND_RECV_DEVICE_SGE
endif

ifneq ($(shell grep -A 10 "ib_port_attr" $(ib_verbs_h) | grep "ip_gids" > /dev/null 2>&1 && echo ip_gids),)
	override EXTRA_CFLAGS += -DDEFINE_IP_GIDS
endif


ifeq ($(shell grep "iommu_present" $(LINUXSRC)/include/linux/iommu.h > /dev/null 2>&1 && echo iommu_present),)
	override EXTRA_CFLAGS += -DNOT_DEFINED_IOMMU_PRESENT
endif

ifneq ($(shell grep "ifname" $(LINUXSRC)/include/rdma/iw_cm.h > /dev/null 2>&1 && echo ifname),)
	override EXTRA_CFLAGS += -DDEFINED_IW_IFNAME
endif

ifeq ($(shell grep "ether_addr_copy" $(LINUXSRC)/include/linux/etherdevice.h > /dev/null 2>&1 && echo ether_addr_copy),)
	override EXTRA_CFLAGS += -D_DEFINE_ETHER_ADDR_COPY
endif

ifeq ($(shell grep "ether_addr_equal" $(LINUXSRC)/include/linux/etherdevice.h > /dev/null 2>&1 && echo ether_addr_equal),)
	override EXTRA_CFLAGS += -D_DEFINE_ETHER_ADDR_EQUAL
endif

ifneq ($(shell grep "idr_preload" $(LINUXSRC)/include/linux/idr.h > /dev/null 2>&1 && echo idr_preload),)
	override EXTRA_CFLAGS += -DDEFINED_IDR_PRELOAD
endif

ifneq ($(shell grep -w "ip_route_output" $(LINUXSRC)/include/net/route.h > /dev/null 2>&1 && echo ip_route_output),)
	override EXTRA_CFLAGS += -DDEFINE_IP_ROUTE_OUTPUT
endif

ifneq ($(shell grep "flowi6" $(LINUXSRC)/include/net/ip6_route.h > /dev/null 2>&1 && echo ip6_route_output),)
	override EXTRA_CFLAGS += -DDEFINE_IP6_FLOWI6
endif

ifneq ($(shell grep -A 1 "struct.*rtable.*{" $(LINUXSRC)/include/net/route.h | grep dst > /dev/null 2>&1 && echo ip_route_output),)
	override EXTRA_CFLAGS += -DDEFINE_RTABLE_DST_FLAT
endif

ifneq ($(shell grep -A 1 "dst_neigh_lookup" $(LINUXSRC)/include/net/dst.h > /dev/null 2>&1 && echo ip_route_output),)
	override EXTRA_CFLAGS += -DDEFINED_DST_NEIGH_LOOKUP
endif

ifeq ($(shell grep "local_addr" $(iw_cm_h) | grep "sockaddr_in"> /dev/null 2>&1 && echo sockaddr_in),)
        override EXTRA_CFLAGS += -DDEFINE_IWARP_IPV6_SUPPORTED
endif

ifneq ($(shell grep -A 10 "struct.*iw_cm_event.*{" $(iw_cm_h) | grep "ord"> /dev/null 2>&1 && echo ord),)
        override EXTRA_CFLAGS += -DDEFINE_PROPOGATE_ORD_IRD
endif

ifneq ($(shell grep "m_local_addr" $(iw_cm_h) > /dev/null 2>&1 && echo port_mapper),)
        override EXTRA_CFLAGS += -DDEFINE_PORT_MAPPER=1
else
        override EXTRA_CFLAGS += -DDEFINE_PORT_MAPPER=0
endif

ifeq ($(shell grep "data_access_exceeds_word_size" $(LINUXSRC)/include/linux/compiler.h > /dev/null 2>&1 && echo XXX),)
        override EXTRA_CFLAGS += -DDEFINE_DATA_ACCESS_EXCEEDS_WORD_SIZE
endif

ifeq ($(shell grep "READ_ONCE" $(LINUXSRC)/include/linux/compiler.h > /dev/null 2>&1 && echo XXX),)
ifeq ($(shell grep "READ_ONCE" $(LINUXSRC)/include/asm-generic/rwonce.h > /dev/null 2>&1 && echo XXX),)
        override EXTRA_CFLAGS += -DDEFINE_READ_ONCE
endif
endif

ifneq ($(shell grep "this_cpu_inc" $(LINUXSRC)/include/linux/percpu.h > /dev/null 2>&1 && echo this_cpu_inc),)
        override EXTRA_CFLAGS += -DDEFINE_THIS_CPU_INC=1
else
        override EXTRA_CFLAGS += -DDEFINE_THIS_CPU_INC=0
endif

ifeq ($(shell grep -B 1 "struct device.*dma_device" $(ib_verbs_h) > /dev/null 2>&1 && echo dma_ops),)
        override EXTRA_CFLAGS += -DDEFINE_CONSOLIDATED_DMA_MAPPING=1
else ifneq ($(shell grep -B 1 "struct device.*dma_device" $(ib_verbs_h) | grep "Do not access" > /dev/null 2>&1 && echo dma_ops),)
	override EXTRA_CFLAGS += -DDEFINE_CONSOLIDATED_DMA_MAPPING=1
else
	override EXTRA_CFLAGS += -DDEFINE_CONSOLIDATED_DMA_MAPPING=0
endif

ifeq ($(shell grep "ib_device_ops" $(ib_verbs_h) > /dev/null 2>&1 && echo ib_device_ops),)
	override EXTRA_CFLAGS += -DDEFINE_IB_DEV_OPS=1
else
	override EXTRA_CFLAGS += -DDEFINE_IB_DEV_OPS=0
endif

ifneq ($(shell grep "ib_device {" -A30 $(ib_verbs_h) | grep iw_cm_verbs > /dev/null 2>&1 && echo ib_device_iwcm),)
	override EXTRA_CFLAGS += -DDEFINE_IB_DEV_IWARP
endif

ifneq ($(shell grep "driver_id" $(ib_verbs_h) > /dev/null 2>&1 && echo driver_id),)
	override EXTRA_CFLAGS += -D_HAS_IB_DRIVER_ID
endif

ifneq ($(shell grep "ib_device_ops {" -A4 $(ib_verbs_h) | grep owner > /dev/null 2>&1 && echo ib_device_ops_owner),)
	override EXTRA_CFLAGS += -DDEVICE_OPS_OWNER
endif

ifneq ($(shell grep "ib_device_ops {" -A4 $(ib_verbs_h) | grep abi > /dev/null 2>&1 && echo ib_device_ops_abi),)
	override EXTRA_CFLAGS += -DDEVICE_OPS_ABI
endif

ifneq ($(shell grep "ib_device_ops {" -A4 $(ib_verbs_h) | grep driver_id > /dev/null 2>&1 && echo ib_device_ops_driver_id),)
	override EXTRA_CFLAGS += -DDEVICE_OPS_DRIVER_ID
endif

ifneq ($(shell grep "mmiowb_spin_lock" $(LINUXSRC)/include/asm-generic/mmiowb.h > /dev/null 2>&1 && echo mmiowb_spin_lock),)
        override EXTRA_CFLAGS += -D_HAS_MMIOWB_SPIN_LOCK
endif

ifeq ($(shell grep "rdma_udata_to_drv_context"  $(LINUXSRC)/include/rdma/uverbs_ioctl.h > /dev/null 2>&1 && echo rdma_udata_to_drv_context),)
	override EXTRA_CFLAGS += -D_HAS_IB_UCONTEXT
endif

ifneq ($(shell grep "DECLARE_RDMA_OBJ_SIZE" $(ib_verbs_h) | grep "ib_ucontext" > /dev/null 2>&1 && echo driver_id),)
	override EXTRA_CFLAGS += -D_HAS_UCONTEXT_ALLOCATION
endif

ifneq ($(shell grep "int (\*\alloc_pd)" $(ib_verbs_h) > /dev/null 2>&1 && echo has_pd_allocation),)
        override EXTRA_CFLAGS += -D_HAS_PD_ALLOCATION
endif

ifneq ($(shell grep "(\*\dealloc_pd)" $(ib_verbs_h) | grep udata > /dev/null 2>&1 && echo dealloc_pd_has_udata),)
        override EXTRA_CFLAGS += -D_DEALLOC_PD_HAS_UDATA
endif

ifneq ($(shell grep "(\*\dealloc_pd)" $(ib_verbs_h) | grep void > /dev/null 2>&1 && echo dealloc_pd_has_void_return),)
        override EXTRA_CFLAGS += -D_DEALLOC_PD_HAS_VOID_RETURN
endif

ifeq ($(shell grep "(\*\alloc_xrcd)" $(ib_verbs_h) | grep ib_device > /dev/null 2>&1 && echo dealloc_xrcd_has_udata),)
        override EXTRA_CFLAGS += -D_HAS_ALLOC_XRCD_IB_XRCD
endif

ifneq ($(shell grep "(\*\dealloc_xrcd)" $(ib_verbs_h) | grep udata > /dev/null 2>&1 && echo dealloc_xrcd_has_udata),)
        override EXTRA_CFLAGS += -D_DEALLOC_XRCD_HAS_UDATA
endif

ifneq ($(shell grep "(\*\dealloc_xrcd)" $(ib_verbs_h) | grep void > /dev/null 2>&1 && echo dealloc_xrcd_has_void_return),)
        override EXTRA_CFLAGS += -D_DEALLOC_XRCD_HAS_VOID_RETURN
endif

ifneq ($(shell grep "ib_alloc_xrcd_user" $(ib_verbs_h) > /dev/null 2>&1 && echo has_xrc_alloc_user),)
        override EXTRA_CFLAGS += -D_HAS_XRC_ALLOC_USER
endif

ifneq ($(shell grep -A2 "(\*\alloc_pd)" $(ib_verbs_h) | grep context > /dev/null 2>&1 && echo has_ib_context),)
        override EXTRA_CFLAGS += -D_HAS_IB_CONTEXT
endif

ifneq ($(shell grep "rdma_device_to_drv_device" $(ib_verbs_h)  > /dev/null 2>&1 && echo rdma_device_to_drv_device),)
	override EXTRA_CFLAGS += -DDEFINE_DEVICE_TO_DRV
endif

ifneq ($(shell grep "dma_zalloc_coherent" $(LINUXSRC)/include/linux/dma-mapping.h > /dev/null 2>&1 && echo dma_zalloc_coherent),)
	override EXTRA_CFLAGS += -DDEFINE_DMA_ZALLOC
endif

ifeq ($(shell grep "simple_write_to_buffer" $(LINUXSRC)/include/linux/fs.h > /dev/null 2>&1 && echo simple_write_to_buffer),)
        override EXTRA_CFLAGS += -D_HAS_SIMPLE_WRITE_TO_BUFFER
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

ifneq ($(shell awk '/ib_port_attr.{/,/^}/' $(ib_verbs_h) 2>/dev/null | grep active_speed | grep u16 > /dev/null 2>&1 && echo u16_active_speed),)
        override EXTRA_CFLAGS += -D_HAS_U16_ACTIVE_SPEED
endif

ifneq ($(shell grep -A2 "ib_register_device" $(ib_verbs_h) | grep dma_device > /dev/null 2>&1 && echo ib_register_device),)
	override EXTRA_CFLAGS += -D_HAS_IB_REGISTER_DEVICE_DMA_DEVICE
endif

ifneq ($(shell grep -s -A1 '(*get_port_immutable)' $(ib_verbs_h) | grep "u32 port_num" > /dev/null 2>&1 && echo u32_port_num),)
	override EXTRA_CFLAGS += -D_HAS_U32_PORT_NUM
endif

ifneq ($(shell grep "IB_DEVICE_LOCAL_DMA_LKEY" $(ib_verbs_h) > /dev/null 2>&1 && echo IB_DEVICE_LOCAL_DMA_LKEY),)
        override EXTRA_CFLAGS += -D_HAS_IB_DEVICE_LOCAL_DMA_LKEY
endif

ifneq ($(shell grep "int (\*\create_qp)" $(ib_verbs_h) > /dev/null 2>&1 && echo has_qp_allocation),)
	override EXTRA_CFLAGS += -D_HAS_QP_ALLOCATION
endif

ifneq ($(shell awk '/struct ib_umem.{/,/^}/' $(ib_umem_h) 2>/dev/null | grep sg_head > /dev/null 2>&1 && echo with_umem_sg_head),)
        override EXTRA_CFLAGS += -DDEFINE_UMEM_WITH_SG_HEAD
endif

ifneq ($(shell grep "ib_drain_rq" $(ib_verbs_h)  > /dev/null 2>&1 && echo ib_drain_rq),)
	override EXTRA_CFLAGS += -D_HAS_IB_DRAIN
endif

ifeq ($(shell grep "reinit_completion" $(LINUXSRC)/include/linux/completion.h  > /dev/null 2>&1 && echo reinit_completion),)
	override EXTRA_CFLAGS += -DDEFINE_REINIT_COMPLETION
endif

ifneq ($(shell grep "ib_port_phys_state" $(ib_verbs_h)  > /dev/null 2>&1 && echo ib_port_phys_state),)
	override EXTRA_CFLAGS += -D_HAS_IB_PORT_PHYS_STATE
endif

 override EXTRA_CFLAGS += -D_HAS_PTPTIME64

all default: $(obj-m)

$(obj-m): $(SOURCES)
	@if [ "$(OOB_OFED)" = "1" ];\
		then echo -e "\n\n**** Compiling for OOB OFED version $(OFED_VERSION) ******\n\n"; \
	else echo -e "\n\n****** Compiling for INBOX OFED ******\n\n"; \
	fi
	
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` $(OOB_OFED_PATH)


modules_install: $(obj-m)
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` INSTALL_MOD_PATH=$(PREFIX) INSTALL_MOD_DIR=$(DRV_DIR) modules_install 2>&1 | grep -v depmod
	@if [ "$(sles_distro)" = "" -a "$(ubuntu_distro)" = "" ]; then \
		echo "override qedr * $(DRV_DIR)" > qedr.conf ;\
		install -m 644 -D qedr.conf $(PREFIX)/etc/depmod.d/qedr.conf ;\
	fi
	@if [ "$(PREFIX)" = "" ] ;\
		then /sbin/depmod -a ;\
		else echo " *** Run '/sbin/depmod -a' to update the module database." ;\
	fi

initramfs_install: $(obj-m)
	@if [ -f /boot/initramfs-$(KVER).img ] && [ "`lsinitrd /boot/initramfs-$(KVER).img | grep qedr.ko`" ] ; then\
		echo "back up initrd to /boot/initramfs-$(KVER).img.bak" ;\
		cp /boot/initramfs-$(KVER).img /boot/initramfs-$(KVER).img.bak ;\
		echo "add driver qedr to initrd" ;\
		dracut -f --add-drivers qedr ;\
	elif [ -f /boot/initrd-$(KVER) ]; then\
		echo "back up initrd to /boot/initrd-$(KVER).bak";\
		cp /boot/initrd-$(KVER) /boot/initrd-$(KVER).bak;\
		echo "updating initrd" ;\
		mkinitrd -k /boot/vmlinuz-$(KVER) -i /boot/initrd-$(KVER) -m qedr &>/dev/null;\
	else \
		echo "warning: initrd isn't being updated"; \
	fi

#docs_install:
#	gzip -c qedr.4 > qedr.4.gz
#	install -m 644 qedr.4.gz $(MAN_DIR)

clean:
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` clean

clobber: clean

light_install: modules_install

install: modules_install initramfs_install #docs_install

# an option to get all sources required for compilation, used in OFED packaging
list_sources:
	@echo $(realpath $(SOURCES)) | tr " " "\n"

-include Makefile.gen
