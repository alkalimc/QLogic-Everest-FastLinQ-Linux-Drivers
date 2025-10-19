#!/bin/bash

debugfs_dir='/sys/kernel/debug'
dbg_data_path='/tmp'

mount_error_rc=1
missing_env_key_rc=2
no_debugfs_node_rc=3

# Mount debugfs if not already mounted
function mount_debugfs() {
	local is_mounted=`mount | grep /sys/kernel/debug | wc --lines`
	if [[ $is_mounted != 0 ]]; then
		return 0
	fi

	mount -t debugfs nodev $debugfs_dir
	if [[ $? != 0 ]]; then
		return $mount_error_rc
	fi

	return 0
}

function print_to_log() {
	logger "$1" 2> /dev/null
	echo "$1" > /dev/kmsg 2> /dev/null
}

# Dump qed debug data for the specific PF which is indicated in the "QED_DEBUGFS_BDF_DBG" environment key
function collect_qed_dump() {
	if [[ -z $QED_DEBUGFS_BDF_DBG ]]; then
		return $missing_env_key_rc
	fi

	local debugfs_node="${debugfs_dir}/qed/${QED_DEBUGFS_BDF_DBG}/all_data"
	if [[ ! -f $debugfs_node ]]; then
		return $no_debugfs_node_rc
	fi

	mkdir -p $dbg_data_path
	local dump_file_name="${dbg_data_path}/qed_dump-${QED_DEBUGFS_BDF_DBG}_`date +"%m-%d-%Y_%H-%M-%S"`.bin"
	echo 'dump' > $debugfs_node
	cat $debugfs_node > $dump_file_name

	print_to_log "qed $QED_DEBUGFS_BDF_DBG: Saved qed debug data at \"$dump_file_name\""

	return 0
}

mount_debugfs
rc=$? 
if [[ $rc != 0 ]]; then
	exit $rc
fi

collect_qed_dump
rc=$? 
if [[ $rc != 0 ]]; then
	exit $rc
fi

exit 0
