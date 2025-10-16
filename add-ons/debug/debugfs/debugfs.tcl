#!/usr/bin/tclsh
set bdf ""
set nic ""

# private implementation of tcls help mechanism, since unavailable in some environment
dict set helpDict "" ""

proc add_help {a b} {
	global helpDict
	dict set helpDict $a $b
}

proc help {a} {
	global helpDict
	puts [dict get $helpDict "$a"]
}

set script_full_path [info script]
set script_rel_path "[file dirname $script_full_path]/internal"
source $script_rel_path/dbgUtils.tcl
source $script_rel_path/qed_debugfs_if.tcl

# emulates diags device command, directing the debug commands to the relevant device
proc dev {dev_num} {
	global script_rel_path
	global bdf
	global nic
	set bdf [exec $script_rel_path/get_dev_info.sh dev_num $dev_num bdf]
	set_bdf $bdf
	set nic [exec $script_rel_path/get_dev_info.sh dev_num $dev_num nic]
	flush stdout
}

# emulates diags prompt
proc debugfs_prompt {} {
	global bdf
	global nic
	puts -nonewline "${nic}"
        puts -nonewline "> "
}

# emulates diag’s dev command by invoking nics.sh script
proc devs {} {

	# print qede nics
	global script_rel_path
	puts [exec ../nics.sh qede]
}

# check if debugfs is mounted if not mount it
exec $script_rel_path/mount_debugfs.sh

# default actions - call nics.sh and set the first device (dev 0)
devs
dev 0
