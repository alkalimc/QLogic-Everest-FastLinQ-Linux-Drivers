#Synopsis
if [ $# != 3 ]; then
	echo "$0 [ -i index | -n netdev | -b bdf ] conf_file"
	exit -1
fi

# locate debug.fs script
dbg="$(dirname $0)/debugfs.sh $1 $2"
conf=$3

function abort {
	echo "Error -  $1"
	exit
}

# sanity
stat $conf &> /dev/null || abort "conf file not found"
grep '\-1' $conf > /dev/null && (echo "Conf file contains -1, which indicates debugfs errors were encountered while creating the conf file"; exit)

# Disable hardware commit of the DCBX config. The config will be applied using 'dcbx_hw_commit' command.
$dbg -t dcbx_set_cfg_commit 0  2> /dev/null 

# dcbx mode
grep mode $conf > /dev/null &&
	(read mode <<< `cat $conf | grep '^mode' | grep "[0-9]*" -o`
	 $dbg -t dcbx_set_mode $mode)

# read priority table. elimintate text and spaces to read numbers.
for line in `cat $conf | grep priority.*tc.*pfc | sed 's/[a-z]*//g' | sed 's/ \+/,/g'`
do 
	read pri tc pfc <<< `echo $line | sed 's/,/ /g'`
	$dbg -t dcbx_set_pri_to_tc $pri $tc
	$dbg -t dcbx_set_pfc $pri $pfc
done

# transmit send algorithms
for line in `cat $conf | grep tc.*tsa | sed 's/[a-z()%]*//g' | sed 's/ \+/,/g'`
	do read tc tsa bw <<< `echo $line | sed 's/,/ /g'`
	if [ "$bw" == "" ]; then
		bw=0
	fi
	$dbg -t dcbx_set_tc_bw_tsa $tc $bw $tsa
done

# dscp enabled
grep dscp_pfc $conf > /dev/null &&
	(read dscp_pfc_en <<< `cat $conf | grep dscp_pfc | grep "[0-9]" -o`
	 $dbg -t dscp_pfc_enable $dscp_pfc_en)

# dscp to priority mapping
for line in `cat $conf  | grep dscp.*prios | sed 's/[a-z()%]*//g' | sed 's/ \+/,/g'`
do
	read range p0 p1 p2 p3 p4 p5 p6 p7 <<< `echo $line | sed 's/,/ /g'`
	range_start=`echo $range | grep "^[0-9]*" -o`

	# check whether driver supports configuring dscp_pfc in batch mode
	$dbg -t list | grep dscp_pfc_batch_set > /dev/null

	if [ $? == 0 ]; then
		$dbg -t dscp_pfc_batch_set $range_start 0x$p0$p1$p2$p3$p4$p5$p6$p7
	else
		for i in {0..7}
	 	do
	 		dscp_val=`expr $range_start + $i`
	 		$dbg -t dscp_pfc_set $dscp_val $[p$i]
	 	done
	fi
done

# app tlvs
$dbg -t list > /dev/null
$dbg -t list | grep dcbx_app_tlv_del_all > /dev/null
if [ $? == 0 ]; then
	cat $conf | grep app_tlvs  > /dev/null && $dbg -t dcbx_app_tlv_del_all
else
	echo “driver does not support app tlv removal. Skipping”
fi

for line in `cat $conf | grep app.*value.*type | sed 's/ \+/,/g'`
do 
	value=`echo $line | grep '\(0x[a-fA-F0-9]*\)' -o`
	line=`echo $line | sed 's/[a-zA-F()%]*//g' | sed 's/ \+/,/g'`
	read idx value_garbage type pri <<< `echo $line | sed 's/,/ /g'`
	$dbg -t dcbx_app_tlv_set_app $type $value $pri
done

# global rdma configurations
grep "global.*vlan-priority" $conf > /dev/null &&
	(read vlan_pri vlan_pri_en <<< `cat $conf | grep "global.*vlan-priority" | sed 's/[a-z()%-]*//g'`
	 $dbg -t rdma_glob_vlan_pri $vlan_pri
	 $dbg -t rdma_glob_vlan_pri_en $vlan_pri_en)
grep "global.*ecn" $conf > /dev/null &&
	(read ecn ecn_en <<< `cat $conf | grep "global.*ecn" | sed 's/[a-z()%-]*//g'`
	 $dbg -t rdma_glob_ecn $ecn
	 $dbg -t rdma_glob_ecn_en $ecn_en)
grep "global.*dscp" $conf > /dev/null &&
	(read dscp dscp_en <<< `cat $conf | grep "global.*dscp" | sed 's/[a-z()%-]*//g'`
	 $dbg -t rdma_glob_dscp $dscp
	 $dbg -t rdma_glob_dscp_en $dscp_en)

# disable auto commit and cache dcqcn configurations in driver to reduce overhead
$dbg -t list > /dev/null
$dbg -t list | grep dcqcn_hw_commit_set > /dev/null
if [ $? == 0 ]; then
	$dbg -t  dcqcn_hw_commit_set 0 2 > /dev/null
fi

# Flush test results, Check that driver supports dcqcn param setting. Configure them.
for line in `grep "dcqcn_.*[0-9]\+" $conf | sed 's/[ ]\+/,/'`
do
	read param val <<< `echo $line | sed 's/,/ /g'`
	$dbg -t list > /dev/null
	$dbg -t list | grep $param > /dev/null
	if [ $? == 0 ];	then
		test=${param}_set
		$dbg -t $test $val
	else
		echo -e "skipping since dynamic setting is not supported by this driver version - $param"
	fi	
done 
# flush cached dcqcn configurations to HW

$dbg -t list > /dev/null
$dbg -t list | grep dcqcn_hw_commit_set > /dev/null
if [ $? == 0 ]; then
	$dbg -t  dcqcn_hw_commit_set 1 2 > /dev/null
fi

# newer drivers only actually interact with HW after the hw_commit
$dbg -t list > /dev/null
$dbg -t list | grep hw_commit > /dev/null &&
	$dbg -t dcbx_hw_commit

# Re-enable hardware commit of the DCBX config.

$dbg -t list > /dev/null
$dbg -t list | grep dcqcn_hw_commit_set > /dev/null
if [ $? == 0 ]; then
	$dbg -t dcbx_set_cfg_commit 1 2 > /dev/null
fi
