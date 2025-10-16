
                README

        QLogic Everest Driver for RDMA protocols (RoCE and iWARP)
        Cavium, Inc.
        Copyright (c) 2010-2017 Cavium, Inc.


Table of Contents
=================
  Introduction
  Link speed
  Device configuration
  Supported kernels / distros / OFED
  Supported software
  RoCE statistics and counters
  RoCE v2 support
  CPU affinity
  Optimizing the doorbell BAR usage
  Working with large or many resources
  RoCE LAG (RDMA bonding)
  NVMf (NVMe over fabrics)
  Installation
  Module parameters
  Limitations
  Troubleshooting


Introduction
============
This file describes the QEDR (QLogic Everest Driver for RDMA) driver for QL4xxxx
series of converged network interface cards. The RDMA driver is designed to work
in OFED environment in conjunction with the QED core module and the QEDE
Ethernet module. In addition, userspace applications require that the rdma_core
user library be installed on the server. The RDMA driver supports both RoCE
and iWARP, which protocol will run is determined in NVRAM.


Link speed
==========
The devices driven by this driver support the following link speed:
- 2x10G
- 4x10G
- 2x25G
- 4x25G
- 2x40G
- 2x50G
- 1x100G


Device configuration
====================
RoCE support is enabled by default in NVRAM. iWARP support can also be enabled
in NVRAM. Flow control and priority can also be enabled and configured in NVRAM
including legacy and DCBx IEEE/CEE/Auto.
Configuration of the NVRAM can be performed via the FastLinQ 4xxxx Diagnostic
Tool for Linux, a.k.a. Qlediag, via the preboot configuration tools and via the
userspace configuration application.


Supported kernels / distros / OFED
==================================
- With Inbox OFED:
     RHEL6.8, RHEL7.2, RHEL7.3
     CentOS7.2
     SLES12 SP1, SLES12 SP2,
     Ubuntu 14.04.5 LTS, Ubuntu 16.04.1 LTS
- With Outbox OFED 3.18-2:
     RHEL7.2
     CentOS7.2
     SLES12 SP1


Supported software
==================
Below is the list of benchmark and test applications tested with this driver:
- ibv_rc_pingpong, ibv_srq_pingpong (RoCE only)
- ibv_devinfo
- ib_send_bw/lat, ib_write_bw/lat, ib_read_bw/lat, ib_atomic_bw/lat
- rdma_server / rdma_client
- rping
- qperf
- fio
- cmtime
- riostream
- ucmtose

Below is the list of ULPs tested with this driver:
- iser: targetcli, tgtd, iscsiadm
- NFSoRDMA
- NVMf

Below is the list of applications tested with this driver:
- MVAPICH2
- Open MPI
- Intel MPI


note1: it is recommend not to unload qedr (or any other RDMA driver for that
       matter) while a RoCE application or ULP is running.
note2: iSER was tested as part of INBOX OFED only in:
       RHEL7.2, RHEL7.3
       CentOS7.2
       SLES12 SP1, SLES12 SP2,
       Ubuntu 14.04.5 LTS, Ubuntu 16.04.1 LTS
note3: iSER over iWARP: Qedr does not support port mapper that is not part of
       the kernel. Therefore, for running iSER over iWARP on the listed
       distros above two LUNS need to be defined, one default and one with
       iSER enabled. Discovery should be done against the default port, and
       login should be done against the iSER enabled port.

RoCE statistics and counters
============================
To dump RDMA statistics perform the following:
> mount -t debugfs nodev /sys/kernel/debug
> cat /sys/kernel/debug/qedr/qedrX/stats           #where X is the device number

To reset RDMA statistics perfrom the following:
> echo 0 > /sys/kernel/debug/qedr/qedrX/stats      #where X is the device number

To dump RDMA counters perform the following:
> mount -t debugfs nodev /sys/kernel/debug
> cat /sys/kernel/debug/qedr/qedrX/counters        #where X is the device number

The counters display the currently used resources and the total amount of
resources per all resource types including QPs, CQs, DPIs (user contexts),
TIDs (MRs) and etc.

RoCE v2 support
===============

Overview
--------
RoCE v2, a.k.a. Routable RoCE allows routing RoCE packets via IPv4 or IPv6.
Destination port number 4791 is used to indicate a RoCE v2 packet while the
source port can vary. Packets with the same UDP source port and the same
destination address must not be reordered. Packets with different UDP source
port numbers and the same destination address may be sent over different links
to that destination address. Devices driven by qedr fully support RoCE v2 in
both Ipv4 and IPv6 modes. In order to operate in RoCE v2 both the driver and
OFED version must also support RoCE v2.

Operation
---------
Some apps/benchmarks can be configured to use RoCE v2 by using a specific GID.
GIDs can be of types RoCEv1, RoCEv2-IPv6 or RoCEv2-IPv4. By choosing the
appropriate GID, usually via the GID index, the RDMA traffic type will be set
accordingly. In order to run ULPs and RDMA CM applications the RDMA CM must be
configured to RoCE v2 mode. Note that RDMA CM may only be in either RoCEv1 or
RoCEv2 mode. The release package contains scripts for showing and configuring
GIDs and RDMA CM:
- show_rdma_cm_roce_ver.sh - shows the current RDMA CM RoCE version.
- config_rdma_cm_roce_ver.sh - configures the RDMA CM RoCE version.
- show_gids.sh - shows a table of all GIDs including their index and type.

note: as of the day this document was written RoCE v2 is supported in upstream
      kernel and by the OFED 4.8 daily build.


CPU affinity
====================================
The affinity of a device's completion vectors to CPUs can be easily set so that
each vector will interrupt a different CPU by using 'qedr_affin.sh'. This is
relevant to applications and ULPs that use interrupt mode. The release package
contains a script to configure to affinity of the CPUs. For example, to set the
interrupt affinity of qedr3:

> sh qedr_affin.sh 3

To set the affinity of all devices:

> sh qedr_affin.sh


Optimizing the doorbell BAR usage
=================================
Everest 4 BigBear and Arrowhead use a PCI bar to issue doorbells to the NIC. The
size of this BAR is configured in the NVRAM and takes effect when the server
boots.
The more Queue Pairs required by the RoCE driver, the more applications run
simultaneously and the more CPUs, the larger the BAR size should be.
However, having a bigger BAR increases the risk of the kernel failing to
provision all of the PFs with their BAR requirements, resulting with PFs which
fail to probe.

Module parameters are available for tuning the BAR usage. See the relevant
section for the list of parameters.


Working with large or many resources
====================================
When working with large resources such as CQ, SQ or RQ, fine tuning the server
may be required. In order to accommodate the mapping of large memory regions
and/or the mapping of many small pages, the following may require tuning:
1) In RHEL the maximum size of allocated memory region is configured in per user
    in: /etc/security/limits.conf. It is given in the number of kbytes or simply
    set to "unlimited". For example:
      *  soft  memlock  <number>
      *  hard  memlock  <number>
   where number stands for kbytes or may be simply the word "unlimited" and "*"
   stands for the all user in this example.
2) In RHEL the maximum number of mappings per process may be read by:
     > cat /proc/sys/vm/max_map_count
   and configure by:
     > echo <number> > /proc/sys/vm/max_map_count
   where number stands for the number of mappings per process.

RoCE LAG (RDMA bonding)
=======================
RoCE LAG is a feature meant for mimicking Ethernet bonding over RoCE.
It provides the ability to double the RDMA throughput by aggregating two ports
on the one hand and to handle a failover on a port by migrating its payload to
the second one on the other hand.
This feature requires that the ethernet bonding driver module will be loaded.
In addition, qedr driver must be loaded after ethernet bonding is configured -
it means that the correct sequence to configure RoCE LAG is the following:
	* modprobe qede (qedr has probably loaded as well).
	* rmmod qedr.
	* configuring ethernet bonding.
	* modprobe qedr.
if everything went well, qedr_bondX RDMA devices should be shown in ibv_devinfo.

Current limitations:
1) RDMA bonding is supported only on 2-port/4-port AH adapters (It is not
   supported on BB adapters).
2) No NPAR/Flex support.
3) RDMA bonding supports only LACP mode (mode 4) - RDMA bonding devices will be
   created also for modes 1 and 2, but for now only mode 4 has been tested and
   verified.
4) ifconfig down on primary device in the bond (PF0/2) doesn't work - traffic
   stops and the application terminates.
5) Cable pull/Swich port up-down isn't supported if Switch is in Auto-Neg mode.
   Switch and Adapter should be in Force mode.

NVMf (NVMe over fabrics)
========================
Introduction
------------
NVMf is a technology specification designed to enable non-volatile memory
express message-based commands to transfer data between a host computer and a
target solid-state storage device or system over a network, such as Ethernet,
Fibre Channel (FC) or InfiniBand.
nvme program is a linux user space utility to provide standards compliant
tooling for NVMe drives.
In order to start a session between target and initiator, need to configure the
target side and then execute 'nvme discover' and 'nvme connect' from the
initiator side - this is the login process.
The 'nvme connect' command creates I/O queues, each is CQ+QP and allocates a
configurable number of MRs (queue elements) after each queue creation:
The number of I/O queues is equal to the number of cores by default and can be
configured by -i flag. The queue elemenets number (MRs) is 128 by default and
can be configured by -Q flag.
There are a few kinds of timeouts that are related to the login process, two of
which are important to understand:
 - keep-alive timeout: The maximum duration of the login process - 15 seconds
   by default and can be configured by -k flag.
 - connection timeout: The maximum duration for each I/O queue creation
   (including all its elements) - 3 seconds and not configurable.

NVMf over VFs
-------------
There are some factors which can cause a failure over VFs:
While over PFs NVMf works smoothly, over VFs there are two obstacles:
 - Almost every operation which executed by the VF side will be done by the PF
   on behalf of it. Both side communicate through the PF-VF channel, so there is
   some latency which extend the duration of the operation.
   For example: If the server we use has a lot of cores, the nvme will try to
   create a lot of I/O queues during the login process, so it might exceed the
   keep-alive timeout period - in such a case, one should set less number of
   I/O queues (or queue elements).
 - In general, VFs have less resources than PFs, so under some condition it is
   possible to reach out of resources (especially with MRs). One possible
   solution is to set less I/O queues (or queue elements).The other solution is
   to try loading with more resources, i.e increase vf_num_rdma_tasks module
   param value.

Installation
============
To build and install just type:

> make install

When building with OFED 3.18 the compat.h file should be generated in the
following way, prior to building:

> cd /usr/src/compat-rdma-3.18/
> ./configure


Module parameters
=================
the following module parameters exist for the qedr driver:
- debug         - controls driver verbosity level with folowing bitmask.
	QEDR_MSG_INIT		= 0x10000,
	QEDR_MSG_FAIL		= 0x10000,
	QEDR_MSG_CQ		= 0x20000,
	QEDR_MSG_RQ		= 0x40000,
	QEDR_MSG_SQ		= 0x80000,
	QEDR_MSG_QP		= (QEDR_MSG_SQ | QEDR_MSG_RQ),
	QEDR_MSG_MR		= 0x100000,
	QEDR_MSG_GSI		= 0x200000,
	QEDR_MSG_MISC		= 0x400000,
	QEDR_MSG_SRQ		= 0x800000,
	QEDR_MSG_IWARP		= 0x1000000,

- insert_udp_source_port - Insert a non-zero UDP source port for RoCEv2 packets
                  that is unique per QP.
- wq_multiplier - When creating a WQ the actual number of WQE created will be
                  multiplied by this number. See Troubleshooting for more
                  information.

iWARP specific:
- delayed_ack   - Enable / Disable TCP delayed ack feature. Default: disabled.
- timestamp     - Enable / Disable TCP timestamp feature. Default: disabled.
- crc_needed    - Enable / Disable CRC for iWARP packets. Default: enabled.
- rcv_wnd_size  - Set the TCP receive window size in 1K resolution.
                  Default is 1M. The size should be a mutliple of 64K. Minimum
                  window size is 64K.
- iwarp_cmt     - Support CMT mode.
                  Enabling this support comes with a degradation in the L2
                  performance, since all traffic is routed to a single engine.
                  Default: disabled.

iWARP MPA Enhanced mode related:
- peer2peer     - Support peer2peer ULPs 0 - Disabled 1 - Enabled.
                  Default:Enabled (uint)
- mpa_enhanced  - MPA Enhanced mode. Default:1 (uint)
- rtr_type      - RDMAP opcode bitmap to use for the RTR message:
                  1: RDMA_SEND 2: RDMA_WRITE 4: RDMA_READ 7: ALL. Default: 7


the following module parameters exist for the qed driver and affect the qedr
driver:
- min_rdma_qps  - the minimum number of RDMA QPs required by the card. Setting
                  a small/large value will decrease/increase the BAR usage.
                  This number cannot be smaller than 64 or larger than ~8k.
- min_rdma_dpis - the minimum number of simultaneous user applications that
                  will run simultaneously using RDMA. Setting a small/large
                  value will decrease/increase the BAR usage. Note: the more
                  CPUs in a server, the larger will be the DPI size.
- roce_edpm     - RoCE EDPM is a feature that allows improved latency at the
                  expense of BAR size. The more CPUs a server has, the larger
                  will the DPI be. Disabling EDPM via this module parameter
                  decreases the DPI size and the BAR usage as an effect. This
                  parameter allows enabling EDPM, if bar size is large enough,
                  forcing it (like enable, but driver will fail to load if the
                  BAR size is too small), or simply disabling it.

Limitations
===========
- qedr was tested on 64 bit systems only.
- qedr has only little endian support.
- qedr will not work over virtual function in SRIOV environment.
- qedr running over a PF with SRIOV capability was not tested.
- qedr was tested Everest 4 Big Bear and Arrowhead.
- MTU change: iWARP requires reload of qedr for it to have effect.
              RoCE open connections will not be modified to use new mtu, only
              new connections opened afterwards.

For more details please see the QED core module README file.


Troubleshooting
===============
 * We recommend the following configuration for performance scenarios in the
   kernel command line:
      idle=poll intel_idle.max_cstate=0 nosoftlockup mce=ignore_ce
   Failing to do so may lead to lower throughput in large IO transactions. Large
   IO transactions means the CPU(s) is(are) less busy and may enter idle mode.
   Since waking up from idle mode is time consuming, this may affect the
   throughput.
   Notes:
   1) The kernel can turn on CPU idle even if it is disabled in the BIOS,
      hence it isn't sufficient to turn it off in the BIOS.
   2) Some Linux versions limit the length of the command line. Verify the
      kernel had indeed booted with these parameters by 'cat /proc/cmdline'.
* When using the perftests the message "Conflicting CPU frequency values" may
  appear, due to CPU frequency fluctuations, preventing the application from
  executing. This can be bypassed by adding the "-F" parameter or by ensuring
  that the CPU(s) frequencies will be constant. In order to achieve the constant
  frequency the BIOS, cpu power driver and governor should be set accordingly.
  For example - disable the intel driver by adding intel_pstate=disable in the
  kernel command line to have acpi-cpufreq as the cpu power driver, and set the
  governor to 'performance'.
* When isert is run with high block sizes it may indicate a SQ overflow.
  This is a known issue in iSER itself that has negligible effect on the
  throughput and mostly annoys by filling the dmesg buffer. This occurs since
  iSER doesn't have SQ accounting and may try to push more work requests then
  allowed. Trying to set the value of the module parameter wq_multiplier may
  help reducing or avoiding these prints.

