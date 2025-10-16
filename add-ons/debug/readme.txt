==========================
DEBUG PACKAGE README FILE:
==========================

--------------------------
ethtool-d folder includes:
--------------------------

- ethtool-d.sh
- ethtool-d_tool.sh

Internal files (tools for the script):
- ethtool-d
- ethtool-d.c

With ethtool-d.sh you can parse the output of 'ethtool -d'.
The tool will create the following files:
* pair of idle_chk
* binary grcDump
* igu fifo
* reg fifo
* protection override
* phy
* mcp trace
* ilt
For 100G the tool will create the files for each engine, except from phy and mcp trace (because they are common for both engines).
The grcDump needs to be further processed to human readable form.
For automatically parsing grcDump add '-p' to the command. 

Usage:
ethtool -d <interface_number> > output_file
./ethtool-d.sh output_file <-p>

By using ethtool-d_tool.sh the tool will automatically create output for ethtool -d and parse the GrcDump.
Usage:
./ethtool-d_tool.sh <interface_number> 

The files will be created in the same folder that output_file is located.

------------------------
debugfs folder includes:
------------------------

- debugfs.tcl
- debugfs.sh
- debugfs_dump.sh
- debugfs_conf.sh

Internal files (tools for the scripts):
- dbgUtils.tcl
- qed_debugfs_if.tcl
- get_bdf.sh
- mount_debugfs.sh

With these tools you are able to get Grc Dump, Idle Check, enter commands to the debugfs preconfig and more.

# debugfs.tcl – used to configure and obtain debug information through debugfs. Can be used directly against the device, or to produce a configuration file which can be configured to device later as direct debugbus configuration or as preconfig.
You can get detailed documentation of the dbgConfig command through dbgConfig –help.
To activate the script type 'tclsh' and then 'source debugfs.tcl'
List of the nics that avaliabe will apear. For changing the device use 'dev < device number (appears in the left column) >' 
For Idle Check type 'idleChk'
For Grc Dump type 'grcDump <output file>'
Usage example:
tclsh
% source debugfs.tcl
% idleChk
% grcDump <output file>
% dbgConfig –rh x
% dbgStart
% <scenario>
% dbgDump <filename>

The debug.tcl script can also be used to create scripts which can later be used via debugfs. This is done with the set_file option. Invoking set_file <filename> before a sequence of commands would cause the commands to be stored into <filename> instead of actually being executed. The commands would be stored in a format suitable for debugfs invocation, can be stored for later or used in another machine, without diag or any other supplementary tool or script.
Usage example:
tclsh
% source debugfs.tcl
% set_file /tmp/rhx
% dbgConfig -rh x
% dbgStart
% exit
This would create a file called bus_rhx under /tmp
This file can be used either directly aginst debugfs:
cat bus_rhx > /sys/kernel/debug/qed/<bdf>/bus
<scenario>
echo dump > /sys/kernel/debug/qed/<bdf>/bus
cp /sys/kernel/debug/qed/<bdf>/bus /tmp/
or through the debugfs.sh script, as a source file for debugbus option (-s option, see below for more details on debugfs.sh)

# debugfs.sh – used to interface with the debugfs, for obtaining idleChk, grcDump,and debugBus preconfig.
User needs to indicate which debug feature to activate and against which pci function. Additional parameters may be required for input/output files.
Pci function can be identified through running index in nics.sh display, network interface name or bdf.

Debugfs usage:
./debugfs.sh [ <-n network interface> | <-b bdf> | <-i device index> ] [ <-d feature> | <-t test> ] <-P port for phy> <-o output file name>  <-s source file> <-p>
-n   network interface name, e.g. eth18
-b   pci bus device function, e.g. 02:00.0
-i   device index as appears in nics.sh
-d   features available: grc, idle_chk, mcp_trace, preconfig, bus, reg_fifo, igu_fifo, protection_override, phy, phy_info, phy_mac_stat
-o   for grc and debug bus and preconfig output files
-s   source file for accepting a script
-p   for parsing grcDump output file
-t   for running a test from tests node
-P   for indicating from which port to run the phy feature
Notes:
Debug bus feature:
If bus feature and source file was entered the source file will be used to configure the debug bus
If bus feature and output file was entered then the collected data will be written into the output file
Tests feature:
Use '-t list' to view the list of tests available
You must enter which device you want to run the test before the test option, parameters for determining which device would be used (-i, -b or -n) must be provided prior to the -t option 
Please note, some tests may have adverse effects if invoked at the wrong time or in wrong order
Phy feature:
For using phy info, use the commands '-d phy_info'
For using phy mac_stat, use the command '-d phy_mac_stat', and specify which port will be used with the phy command with the '-P' option
For using other phy commands, use '-d phy -s intput.txt', when you specify in input.txt the exact command and parameters
EXAMPLES:
(1) ./debugfs.sh -b 02:00.3 -d idle_chk
(2) ./debugfs.sh -i 0 -d mcp_trace
(3) ./debugfs.sh -i 1 -d reg_fifo
(4) ./debugfs.sh -n ens6f0 -d protection_override
(5) ./debugfs.sh -n eth18 -d grc -o result.txt
(6) ./debugfs.sh -i 3 -d grc -o result.txt -p
(7) ./debugfs.sh -b 02:00.5 -d grc -o result.txt -s input.txt
(8a) -n eth10 -d bus -s input.txt
(8b) -n eth10 -d bus -o output.txt
(9a) -d preconfig -s input.txt
(9b) -i 8 -d bus -o output.txt
(10) -i 0 -t ets
(11a) -i 1 -d phy_info
(11b) -i 5 -d phy -s input.txt
(11c) -i 0 -d phy_mac_stat -P 0
# End of debugfs usage

Preconfig feature:
(1) Entering commands to preconfig:
./debugfs.sh -i 1 -d preconfig -s input.txt 
(2) Stop recording data from debug bus and copying to output file :
./debugfs.sh -i 1 -d preconfig -o output.txt

TX timeout feature:
Can be generated by writing "on" to qede's debugfs node "tx_timeout".
./debugfs.sh -i 1 -d tx_timeout -t on

#mount_debugfs.sh - (located in 'internal' folder) used to mount debugfs file system

------------
sys_info.sh
------------

This tool provides information regarding the system of the machine.
It is a non intrusive tool using standard OS APIs for collecting information regarding the server.
If networking drivers are probed, information would be collected for qlogic devices via standard ethtool APIs.
The tool doesn't require parameters.
It provides the following types system information:
- general
- hardware
- software
- network
- virtualization
- logs
It also creates an error log for errors it encountered while collecting data.
This script employs the nics.sh script also available as part of the debug package.
A report is created under /tmp/<timestamp>

For running the tool use:
./sys_info.sh

-------
nics.sh
-------

This tools prints all available  functions for each installed driver in the machine.

For listing function of particular driver specify the driver name.
Example:
(1) ./nics.sh qede
(2) ./nics.sh bnx2x
(3) ./nics.sh tg3

---------------
debugfs_dump.sh
---------------

This tool dumps current device configuration (dcb, dscp-pfc, global rdma configurations, etc).
The output of this tool can be redirected to a file.
Such a file can be used by the debugfs_conf.sh tool (see below).
Device is selected using the same semantics as debugfs.sh (see above).

---------------
debugfs_conf.sh
---------------

This tool applies configuration from a file to the device.
The file format is per the debugfs_dump.sh tool output (see above).
the tool can also accept sub sections of such output and apply them.
A set of prefabricated configrations is available under the /configs folder.

---------------
debugfs_edit.sh
---------------

This tool invokes the debugfs_dump.sh tool, then an editor to open the
configuration file, and then the debugfs_dump.sh tool to apply the
configuration, effectively giving a "live" configuration tool.

---------------------
data folder includes:
---------------------

-ethtool_stats.csv
-location.txt
-phy_mac_stats.csv
-verbosity_log_level.txt

These files includes the information of ethtool stats, phy mac stats and
the debug flags for different verbosity levels.
