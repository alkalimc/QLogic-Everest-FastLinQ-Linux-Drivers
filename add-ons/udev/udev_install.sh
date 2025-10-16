#!/bin/bash

udev_src_dir=`dirname $0`
udev_rules_dir='/etc/udev/rules.d'
udev_rules_file='99-qed.rules'
udev_script_dir='/lib/udev'
udev_dbg_script='qed_udev_dbg.sh'

usage_error_rc=1
no_udev_tool_rc=2

function usage() {
	cat << EOF
Usage:
	`basename $0` -h, --help		Display this usage
	`basename $0` -i, --install		Install udev rules and script
	`basename $0` -u, --uninstall		Uninstall udev rules and script
EOF
}

function get_udev_reload_op() {
	if [ -f /sbin/udevadm ]; then
		echo '/sbin/udevadm control --reload-rules'
	elif [ -f /bin/udevadm ]; then
		echo '/bin/udevadm control --reload-rules'
	elif [ -f /sbin/udevcontrol ]; then
		echo '/sbin/udevcontrol reload_rules'
	elif [ -f /bin/udevcontrol ]; then
		echo '/bin/udevcontrol reload_rules'
	else
		echo 'none'
	fi
}

function udev_install() {
	local udev_reload_op=$(get_udev_reload_op)
	if [[ "$udev_reload_op" == "none" ]]; then
		echo "failed to find a management tool for udev"
		return $no_udev_tool_rc
	fi

	mkdir -p ${udev_script_dir}
	diff ${udev_src_dir}/${udev_rules_file} ${udev_rules_dir}/${udev_rules_file} &> /dev/null
	local ret=$?
	diff ${udev_src_dir}/${udev_dbg_script} ${udev_script_dir}/${udev_dbg_script} &> /dev/null
	((ret+=$?))

	if [[ $ret != 0 ]]; then
		echo "Installing udev files ${udev_rules_file} and ${udev_dbg_script}"
		install -m 755 ./${udev_src_dir}/${udev_rules_file} ${udev_rules_dir}/
		install -m 755 ./${udev_src_dir}/${udev_dbg_script} ${udev_script_dir}/
		$udev_reload_op
	else
		echo "udev rules already installed"
	fi

	return 0
}

function udev_uninstall() {
	local udev_reload_op=$(get_udev_reload_op)
	if [[ "$udev_reload_op" == "none" ]]; then
		echo "failed to find a udev management tool"
		return $no_udev_tool_rc
	fi

	echo "Removing udev files ${udev_rules_file} and ${udev_dbg_script}"
	rm -f ${udev_rules_dir}/${udev_rules_file} &> /dev/null
	rm -f ${udev_script_dir}/${udev_dbg_script} &> /dev/null
	$udev_reload_op

	return 0
}

if [ $# -lt 1 ]; then
	usage
	exit $usage_error_rc
fi

exit_code=0
case "$1" in
-h | --help)
	usage
	;;
-i | --install)
	udev_install
	exit_code=$?
	;;
-u | --uninstall)
	udev_uninstall
	exit_code=$?
	;;
*)
	usage
	;;
esac

exit $exit_code
