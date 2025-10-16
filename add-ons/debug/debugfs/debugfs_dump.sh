#Synopsis
if [ $# != 2 ]; then
	echo "$0 [ -i index | -n netdev | -b bdf ]"
	exit -1
fi

# qed* presence
chk=`lsmod | grep 'qede\|qedi\|qedf' | wc --lines`
if [ "$chk" -eq 0 ]
then
        echo "Driver not present";
        exit -2
fi

# print title function
function title {
	title=$1
	echo -e "\n$title"
	for i in `seq 1 ${#title}`; do echo -n '-'; done
	echo
}

# debugfs script shortcut
#path=`dirname $(readlink -f $0)`
path=`dirname $0`
dbg="$path/debugfs.sh $1 $2"

function truncate_dbg {
	$dbg -t list | tr -d '\0'
}

function dcbx {

#print dcbx mode
title "dcbx mode"
$dbg -t dcbx_get_mode
dcb=`truncate_dbg`
if [ $dcb == 0 ]; then dcb_txt="disabled"
elif [ $dcb == 1 ]; then dcb_txt="host"
elif [ $dcb == 2 ]; then dcb_txt="lld managed"
elif [ $dcb == 4 ]; then dcb_txt="cee"
elif [ $dcb == 8 ]; then dcb_txt="ieee"
elif [ $dcb == 16 ]; then dcb_txt="static"
elif [ $dcb == -1 ]; then echo "DCBX operational params not available. Skipping DCBX section"; return
fi
echo "mode $dcb_txt ($dcb)"

#if dcbx is disabled don't continue to dump
if [ $dcb == 0 ]; then return; fi

# print prio to tc mapping
title "priority to tc mapping"
(for pri in {0..7}; do
	$dbg -t dcbx_get_pri_to_tc $pri
	tc=`$dbg -t list`
	$dbg -t dcbx_get_pfc $pri
	pfc=`$dbg -t list`
	echo priority $pri tc $tc pfc $pfc
done) | column -t

# print tc to tsa mapping
title "transmit send algorithm"
(for tc in {0..3}; do
	`$dbg -t dcbx_get_tc_tsa $tc`
	tsa=`$dbg -t list`
	bw=''
	if [ $tsa == 0 ]; then tsa_txt="strict"
	elif [ $tsa == 1 ]; then tsa_txt="cbs"
	elif [ $tsa == 2 ]; then
		 tsa_txt="ets"
		`$dbg -t dcbx_get_tc_bw $tc`
		bw="(`$dbg -t list`%)"
	fi
	echo "tc $tc tsa $tsa_txt($tsa) $bw"
done) | column -t

# print app tlvs
title "app_tlvs"
$dbg -t list > /dev/null
$dbg -t list | grep dcbx_app_tlv_get_count > /dev/null
if [ $? == 0 ]; then
	$dbg -t dcbx_app_tlv_get_count
	app_num=`$dbg -t list`
	(for i in `seq 1 $app_num`; do

		# app idx is zero based
		idx=`expr $i - 1`

		# read app params
		$dbg -t dcbx_app_tlv_get_value_by_idx $idx
		value=`$dbg -t list`
		$dbg -t dcbx_app_tlv_get_type_by_idx $idx
		type=`$dbg -t list`
		$dbg -t dcbx_app_tlv_get_pri_by_idx $idx
		pri=`$dbg -t list`

		# recognize type
		if [ $type == 0 ]; then type_txt="ethtype"
		elif [ $type == 1 ]; then type_txt="udp"
		elif [ $type == 2 ]; then type_txt="tcp"
		elif [ $type == 3 ]; then type_txt="port"
		else type_txt="unknown"
		fi
	
		# recognize value
		hexval=`printf "0x%x" $value`
		if [ $hexval == 0x8915 ] && [ $type_txt == "ethtype" ]; then value_txt="roce-v1"
		elif [ $hexval == 0x12b7 ] && $([ $type_txt == "udp" ] || [ $type_txt == "port" ]); then value_txt="roce-v2"
		elif [ $hexval == 0x8906 ] && [ $type_txt == "ethtype" ]; then value_txt="fcoe"
		elif [ $hexval == 0xcbc ] && $([ $type_txt == "tcp" ] || [ $type_txt == "port" ]); then value_txt="iSCSI"
		else value_txt="unknown"
		fi
	
		#print app
		echo "app $i value $value_txt($hexval) type $type_txt($type) priority $pri"
	done) | column -t
else
	echo "Driver doesn't support app tlv getters"
fi

}

function dscp_pfc {
title "dscp pfc"
dscp_en=1
$dbg -t list | grep dscp_pfc_get_enable > /dev/null
if [ $? == 0 ]; then
	`$dbg -t dscp_pfc_get_enable`
	dscp_en=`truncate_dbg`
	[ $dscp_en == 0 ] && dscp_en_txt="disabled" || dscp_en_txt="enabled"
	echo "dscp_pfc $dscp_en_txt($dscp_en)"
else
	echo "Driver doesn't support getter for dscp_pfc enabled"
fi

if [ $dscp_en == 1 ]; then
	title "dscp to priority mapping"

	# not all drivers support dscp batch mode
	$dbg -t list | grep dscp_pfc_batch_get > /dev/null
	(if [ $? == 0 ]; then

		# get dscp config in batch
		for i in {0..7}; do
			dscp_val=`expr $i \* 8`
			echo -n dscp $dscp_val..`expr $dscp_val + 7` prios
			$dbg -t dscp_pfc_batch_get $dscp_val
			batch=`$dbg -t list`
			prios=`printf "%08x" $batch | sed 's/\([0-9a-f]\)/ \1/g' | rev`
			echo ' ' $prios
		done
	else
		# get dscp one by one
	 	for dscp_val in {0..63}; do
	 		if [ `expr $dscp_val % 8` == 0 ]; then
	 			echo
	 			echo -n dscp $dscp_val..`expr $dscp_val + 7` prios
	 		fi
	 		$dbg -t dscp_pfc_get $dscp_val
	 		pri=`$dbg -t list`
	 		echo -n ' ' $pri
		done
	fi) | column -t
fi
}

function rdma_glob {
title "rdma global configurations"

#check if driver supports this
$dbg -t list > /dev/null
$dbg -t list | grep rdma_glob_get > /dev/null
if [ $? == 0 ]; then
	($dbg -t rdma_glob_get_vlan_pri_en
	 vlan_pri_en=`truncate_dbg`
	 [ $vlan_pri_en == 1 ] && vlan_pri_en_txt=enabled || vlan_pri_en_txt=disabled
	 $dbg -t rdma_glob_get_vlan_pri
	 vlan_pri=`truncate_dbg`
	 $dbg -t rdma_glob_get_ecn_en
	 ecn_en=`truncate_dbg`
	 [ $ecn_en == 1 ] && ecn_en_txt=enabled || ecn_en_txt=disabled
	 $dbg -t rdma_glob_get_ecn
	 ecn=`truncate_dbg`
	 $dbg -t rdma_glob_get_dscp_en
	 dscp_en=`truncate_dbg`
	 [ $dscp_en == 1 ] && dscp_en_txt=enabled || dscp_en_txt=disabled
	 $dbg -t rdma_glob_get_dscp
	 dscp=`truncate_dbg`
	 echo "global vlan-priority $vlan_pri $vlan_pri_en_txt($vlan_pri_en)"
	 echo "global ecn $ecn $ecn_en_txt($ecn_en)"
	 echo "global dscp $dscp $dscp_en_txt($dscp_en)") | column -t
else
	echo "Driver doesn't support rdma global config getters"
fi
}

function dcqcn {
# older drivers have these only as module params, newer ones as debugfs getter/setters
title dcqcn_params

# check whether dcqcn is enabled
$dbg -t list > /dev/null
$dbg -t list | grep dcqcn_enable > /dev/null
if [ $? == 0 ]; then
	$dbg -t dcqcn_enable
	dcqcn_enable=`$dbg -t list`
else
	dcqcn_enable=`cat /sys/module/qed/parameters/dcqcn_enable`
fi
if [ $dcqcn_enable == 0 ]; then
	echo dcqcn_enable 0
	return
fi

# iterate over all dcqcn params
(for param in `ls /sys/module/qed/parameters/ | grep dcqcn`
do
	val=`cat /sys/module/qed/parameters/$param`

	# override with debugfs getter if applicable
	getter=${param}_get

	$dbg -t list | grep $getter > /dev/null
	if [ $? == 0 ]; then
		$dbg -t $getter; val=`$dbg -t list`
	fi
	echo $param $val

done) | column -t
}

#dcbx params
dcbx

# print dscp to priority mapping
dscp_pfc

# rdma global configurations
rdma_glob

# dcqcn params
dcqcn

echo
