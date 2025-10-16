#!/bin/bash

declare -a msg_list=(
	"Call Trace"
	"WARNING:.* at .*"
	"BUG: .*"
	"Oops:.*"
	"HW error occurred.*"
	"FW assertion"
	"Tx timeout"
	"ksmd.*oom-killer"
	"CQE.*ERR"
	"Failed.*traffic"
	"bnx2x_process_kill.*"
	"bnx2x_io_error_detected.*"
	"DMAE timeout"
	"dmae.*Timed-out.*"
	"qed.*Fatal attention"
	"qed_dbg_parse_attn.*"
	"CQ.*not freed.*"
	"Trying to send a MFW mailbox command.*in parallel to.*"
	"[^ ]* segfault.*libqedr.*"
	"kernel.*PCIe Bus Error"
	"MC assert"
	"driver assert"
	"begin crash dump"
	"qedr_affiliated_event.*"
	"failed to allocate a DPI for a new RoCE application"
	"kernel:.*page allocation failure.*"
	"IMB-MPI1.*segfault.*"
	"roce register tid returned an error"
	"ib_.*segfault.*"
	"bitmap not free"
	"fw_return_code = [1-9]"
	"qperf.*segfault.*"
	"init_qm_get_idx_from_flags.*"
	"get_cm_pq_idx.*"
	"get_qm_vport_idx.*"
	"Expected [0-9]{1,} arguments"
	"roce_get_qp_tc.*failed"
	"roce_get_qp_tc.*misconfiguration"
	"Ramrod is stuck.*"
	"At least one CNQ is required"
	"ILT error - Details.*"
	"Malicious behavior.*"
	"VF <-- PF Timeout \[Type .*\]"
)


if [ $# -ne 1 ]; then
	exit 1
fi

msg_pattern=""
for ((i=0; i<${#msg_list[@]}; i++)); do
	msg_pattern+="${msg_list[$i]}"
	if [[ $i != $((${#msg_list[@]}-1)) ]]; then
		msg_pattern+="|"
	fi
done

res=`egrep "$msg_pattern" $1 -oh --text | head -n 1`

if [ "$res" == "FW assertion" ]
then
	rm -rf /tmp/assert_parsed
	`dirname $0`/asserts/extract_assert.sh $1 -p > /tmp/assert.log
	if [ "$?" == "0" ]
	then
		cat /tmp/assert_parsed/fw_asserts.txt | grep FATAL -A 5 | grep "string =.*" -o | sed 's/string = /FW assert:/g'
	else
		echo $res
	fi
else
	echo $res
fi

exit 0
