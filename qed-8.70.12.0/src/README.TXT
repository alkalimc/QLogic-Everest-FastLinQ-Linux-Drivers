
                README

        QLogic Everest Driver Core Module
        Cavium, Inc.
        Copyright (c) 2010-2017 Cavium, Inc.


Table of Contents
=================

  Introduction
  Link Speed
  Supported Kernels / Distros
  Limitations
  AER Recovery
  Advanced Features
  Module Parameters
  Compilation based options

Introduction
============

This file describes the QED (QLogic Everest Driver) core module for QL4xxx
Series converged network interface cards. The Core Module is not a driver,
but a module designed to work in conjunction with one or more protocol drivers:

QEDE - QLogic Everest Driver for Ethernet protocol
QEDI - QLogic Everest Driver for iSCSI protocol
QEDF - QLogic Everest Driver for FCoE protocol
QEDR - QLogic Everest Driver for RoCE & iWARP protocols

All of the protocol drivers are dependent on the QED Core module.


Link Speed
==========
The devices with which this core module interacts support the following link
speed:

1x100G
2x50G
2x40G
4x25G
2x25G
4x10G
2x10G


Supported Kernels / Distros
===========================
RHEL6.6, RHEL 6.7, RHEL6.8, RHEL6.9, RHEL7.0, RHEL7.1, RHEL 7.2, RHEL7.4, RHEL7.5
SLES11 SP3, SLES11 SP4, SLES12, SLES12 SP1, SLES12 SP2, SLES12 SP3
Ubuntu 14.04 LTs, Ubuntu 16.04 LTs
CentOS 7.0, CentOS 7.1, CentOS 7.2
Citrix 7.0

SR-IOV
Hypervisor - RHEL7.1, RHEL7.2, SLES12, SLES12 SP1, Ubuntu 16.04 LTs, CentOS7.2
VMs - All supported distros [with the exception of Citrix 7.0].

Limitations
===========
- Big endian support is not available.

AER Recovery
============
- During AER recovery, driver will go through the driver load/unload process, therefore
  IOs will stop intermittently. Once recovery is successful, IOs will resume. PCI device
  HOT_RESET may or may not happen depending on AER error type or platform.

- In a particular scenario, where storage PFs are enabled but storage driver is not loaded,
  if AER error is injected, driver will not be able to recover due to kernel's implementation.
  As per the kernel's implementation, if for any device in PCI subtree error_detected callback
  is not available, it will result into kernel not attempting recovery by preventing any further
  callbacks, such as slot_reset, resume etc.

Known Issues
============
- Ubuntu 14.04+ utilizes module signing. Regular driver installation would not
  by signed, hence leading to warning messages appearing during compilation.
  E.g:
   SSL error:02001002:system library:fopen:No such file or directory: bss_file.c:175
   SSL error:2006D080:BIO routines:BIO_new_file:no such file: bss_file.c:178
   sign-file: certs/signing_key.pem: No such file or directory
  Other than appearing during compilation, these warnings should be considered
  as benign.

- On some distros with older BIOS, following messages may appear in the kernel logs.
    pcieport 0000:00:03.0: AER: Corrected error received: id=0000
    qede 0000:02:00.0: PCIe Bus Error: severity=Corrected, type=Data Link Layer, id=0200(Transmitter ID)
    qede 0000:02:00.0:   device [1077:1644] error status/mask=00001000/00002000
    qede 0000:02:00.0:    [12] Replay Timer Timeout
  These logs should be considered as benign. Setting the kernel parameter pcie_aspm=off
  could resolve the issue.

- On some platforms where pci completion may take a long time, it may be required
  to increase the device timeout for pci completion from the default 50ms. This can be
  using setpci command.
  Example configuration:
	# lspci -vvv -s 04:00.0 | grep  DevCtl2 <== Reading completion timeout range
		DevCtl2: Completion Timeout: 65ms to 210ms, TimeoutDis-, LTR-, OBFF Disabled
	#
	# setpci -s  0000:04:00.0  CAP_EXP+0x28.W=0xa <== Configure new completion timeout range
	# lspci -vvv -s 04:00.0 | grep  DevCtl2
		DevCtl2: Completion Timeout: 1s to 3.5s, TimeoutDis-, LTR-, OBFF Disabled

Advanced Features
=================

- Network Partitioning (NPAR)
NPAR must be enabled via HII boot time interface. See HII documentation for more
details. The device will enumerate on the pci as multiple pci functions. Each of
the pci functions can be for any of the protocols (iSCSI, FCoE, RoCE or
ethernet).

The device supports a total of 16 physical functions.

- Single Root Input Output Virtualization (SR-IOV)
SR-IOV must be enabled via HII boot time interface. See HII documentation for
more details. The SR-IOV capability will be present in pci configuration
space. Virtual functions will only appear after the feature is dynamically
enabled in the kernel via the standard linux sysfs interface. e.g.

echo 16 > /sys/bus/pci/devices/0000\:02\:00.0/sriov_numvfs

After this stage virtual devices will enumerate on the pci.
Current software supports up to 120 VFs per function, depending on the number of
the enabled functions.

Path-through of virtual devices to virtual machines requires IOMMU-vtd in
BIOS and kernel.

Alternate Requestor ID capability is required in device as well as in all of the
parent devices in the path to the root complex in order to support more than 8
physical functions, and for large numbers of VFs.

tx-switching - VFs spawned by a given physical function can communicate
between themselves and with the parent PF via the device (which serves as an
e-switch). Module parameters are provided to control the scope of vf and pf
communications. Please see the relevant section.

Data Center Bridging - The device supports both CEE and IEEE based DCB. This
includes Priority-based Flow Control (PFC), Enhanced Transmission Selection
(ETS), strict priority, auto negotiation (DCBx). IEEE or CEE must match the peer
configuration. DCBx can be configured via HII.


Module Parameters
=================
The following module parameters exist for qed module:

npar_tx_switching - for enabling or disabling npar tx switching - this means
                    entities residing on same physical port will be able to
                    communicate with each other in NPAR mode via internal
                    loopback in the device [no need for traffic to reach the
                    line].
                    [Enabled(1) by default].
tx_switching - for enabling or disabling per function tx switching. Without it,
               PFs will not be able to communicate with VFs via internal
               traffic loopback [do notice tx switching incurs a performance
               penalty].
               [Enabled(1) by default].
rdma_protocol_map - Determine the rdma protocol which will run on the device
                string maps device function numbers to their requested protocol (e.g. '02:00.0-1,02:01.0-2').
                maximum of 32 entries is supported
                Valid values types: 0-Take default (what's configured on board, favors roce over iwarp) 1-none, 2-roce, 3-iwarp (string)
load_function_map - Determine which device functions will be loaded
		string lists loaded device function numbers (e.g. '02:00.0,02:01.0').
		maximum of 32 entries is supported
drv_resc_alloc - force the driver's default resource allocation.
                 [Disabled(0) by default].
chk_reg_fifo - check the reg_fifo after any register access.
               [Disabled(0) by default].
initiate_pf_flr - initiate PF FLR as part of driver load.
                  [Enabled(1) by default].
loopback_mode - force a loopback mode.
                [No-loopback(0) by default].
allow_mdump - allow the MFW to collect a crash dump.
              [Do-not-allow (0) by default].
avoid_eng_reset - avoid engine reset when first PF loads on it.
                  [Do-not-avoid (0) by default].
override_force_load - override the default force load behavior.
                      [Do-not-override=0 (default), always=1, never=2]
dbg_send_uevent - send a uevent upon saving all debug data to internal buffer.
                  [Enabled(1) by default].
dbg_data_path - path for debug data files.
                A non-empty path enables saving auto-collected debug data to
		file. No uevent will be sent in this case.
pci_relax_order - Set PCI relax ordering to be set explicitly or as per MFW policy.
		  [Do nothing(0) by default]
limit_msix_vectors - Upper limit value for the requested number of MSI-X vectors.
                     A value of 0 means no limit.
limit_l2_queues - Upper limit value for the number of L2 queues.
                  A value of 0 means no limit.
avoid_eng_affin - avoid engine affinity for RoCE/storage in case of CMT mode.
                  [Do-not-avoid (0) by default].

ilt_page_size - Set ILT page size.
                [default value is 5, allowed range for page size is 3..12]

qed_pkt_pacing - Enable packet pacing [Default is disabled].

allow_vf_mac_change - Allow VF to change MAC despite PF set force MAC
                      [0 Disable (default); 1 Enable]

use_random_vf_mac - VF start with pre-assigned random MAC address by their
		    parent PFs.
		    [0 disabled (default); 1 Enable].

personality - Force PF to have specific protocol support. Use with extreme caution.
              ETH=0, FCOE=1, ISCSI=2, ROCE=3, IWARP=4 (uint)

wc_disabled - Write combine enabled/disabled (0 enabled (default); 1 disabled)
              Map the doorbell bar as regular bar rather than write combined bar.
              (When disabling WC consider disabling ED PM too, via the module
              parameter roce_edpm, otherwise many EDPM failures can appear,
              resulting in even worse latency) (uint)

vf_mac_origin - Origin of the initial VF MAC address.
                0 - Zero address unless provisioned in the NVRAM (default).
		1 - Random address unless provisioned in the NVRAM.
		2 - Zero address.
		3 - Random address.

monitored_hw_addr - Monitored address by ecore_rd()/ecore_wr() (uint)
                    Use this to detect whether or not driver is accessing
                    specific register access (and form which flow).

num_roce_srqs - The number of RoCE SRQs is by default 8192 and can be raised
                ideally up to ~32k (uint)

periodic_db_rec - Always run periodic Doorbell Overflow Recovery.
                  [0 disabled (default); 1 Enable].

rx_asymmetric_bw_mode - Enable rx aymmetric port bandwitdh.
			[0 - equal bandwidth treatment for all ports(Default)
			 1 - Low index ports get high bandwidth
			 2 - High index ports get high bandwidth
			 3 - Even ports get high bandwidth
			 4 - Odd ports get high bandwidth]

Parameters to control the behaviour of the dcqcn algorithm
----------------------------------------------------------
dcqcn_enable - Enable roce dcqcn. (uint)
dcqcn_cnp_dscp - DSCP value to be used on CNP packets. Values between 0..63 (uint)
dcqcn_cnp_vlan_priority - Vlan priority to be used on CNP packets.
                          Values between 0..7 (uint)
dcqcn_notification_point - 0 - Disable dcqcn notification point.
                           1 - Enable dcqcn notification point (uint)
dcqcn_reaction_point - 0 - Disable dcqcn reaction point.
                       1 - Enable dcqcn reaction point (uint)
dcqcn_rl_bc_rate - Byte counter limit (uint)
dcqcn_rl_max_rate - Maximum rate in Mbps (uint)
dcqcn_rl_r_ai - Active increase rate in Mbps (uint)
dcqcn_rl_r_hai - Hyper active increase rate in Mbps (uint)
dcqcn_gd - Alpha update gain denominator. Set to 32 for 1/32, etc (uint)
dcqcn_k_us - Alpha update interval (uint)
dcqcn_timeout_us - Dcqcn timeout (uint)

The following module parameters are relevant for qedr driver. For more
information see the qedr README.TXT:
num_rdma_qps
min_rdma_dpis
roce_edpm

Compilation based options
=========================
OVERRIDE_CRC8 - By default, module would be dependent on the crc8 module.
However, some environments lack said module even if .config seems to indicate
that should be present - initrd of some anaconda installers being an example
of such. Passing 'OVERRIDE_CRC8=1' during compilation would remove QED's
dependency on the crc8 module.

