#!/bin/bash
nics()
{
chk=`lsmod | grep $1 | wc --lines`
if [ "$chk" -eq 0 ]
then
	echo "Module" $1 "not present";
	return
fi
echo $1 "nics"
echo $1 "nics" | sed 's/./=/g'
dev=-1

chip_by_num()
{
	case $1 in
		164e|164f|1650)
			echo E1 ;;
		1662|1663|166f)
			echo E2 ;;
		1651|1652|168a|16a5|16a9|168e|16ae|16af|163d|163e|163f|168d|16ab|16a1|16a2|16a4|16ad)
			echo E3 ;;
		1634|1666|1636|1644|1654|1656|1664)
			echo BB ;;
		8070|8090)
			echo AH ;;
		8170|8190)
			echo E5 ;;
	esac
}

# calculating the pf number according to the device and function fields in the bdf.
pf_num_calc()
{
	local func=`ethtool -i $1 | grep bus | grep .$ -o`
	if [ -z $func ]; then
		echo "-"
		return
	fi
	local device_hex=`ethtool -i $1 | grep bus | cut -d '.' -f 1 | grep .$ -o`
	if [ -z $device_hex ]; then
		echo "-"
		return
	fi
	local device_dec=`echo $((16#$device_hex))`
	local num_func=`expr $func '+' $device_dec '*' 8`
	echo $num_func
}

column_names="dev port func nic board chip bdf driver mfw mac-address ip-address state link|type rdma mdump"

query_nic()
{
	nic=$1
	dev=$2
	bdf=`ethtool -i $nic | grep bus | grep "[0-9a-f][0-9a-f]:[0-9a-f][0-9a-f]\.[0-9a-f]" -o`
	if [ -z $bdf ]
	then
		bdf="-"
	fi
	dbg=/sys/kernel/debug/qed/$bdf/
	board=`lspci -s $bdf -vv 2> /dev/null | grep -m 1 -P '(BCM|QL)[0-9]\S*' -o`
	if [ -z $board ]
	then
		board="-"
	fi
	chip_num=`lspci -s $bdf -nn | sed 's/.*\[.*:\(.*\)\].*/\1/g'`
	chip=$(chip_by_num $chip_num)
	if [ -z $chip ]
	then
		chip="-"
	fi
	drv_ver=`ethtool -i $nic | grep ^version | cut -d" " -f2`
	if [ -z $drv_ver ]
	then
		drv_ver="-"
	fi
	bc_ver=`ethtool -i $nic | grep firmware | grep "mfw [0-9]*.[0-9]*.[0-9]*.[0-9]*" -o | cut -d " " -f2`
	if [ -z $bc_ver ]
	then
		bc_ver=`ethtool -i $nic | grep firmware | grep "[0-9].[0-9].[0-9].[0-9]" | cut -d" " -f2`
		if [ -z $bc_ver ]
		then
			bc_ver="-"
		fi
	fi
	mac=`ip -o link show $nic | grep -o ..:..:..:..:..:.. | head -n 1`
	if [ -z $mac ]
	then
		mac="-"
	fi

	state=`ip -o link show $nic | grep ',UP' -o | grep 'UP' -o`
	if [ -z $state ]
	then
		state="-"
	fi

	num_func=$(pf_num_calc $nic)
	func="PF$num_func"
	if [ -d /sys/bus/pci/devices/0000:$bdf/physfn ]
	then
		physfn=/sys/bus/pci/devices/0000:$bdf/physfn
		for virtfn in `ls $physfn | grep virtfn`
		do
			num_func=$(pf_num_calc `ls $physfn/net`)
			ls $physfn/$virtfn/net/$nic &> /dev/null && func="VF`echo $virtfn | grep "[0-9]*" -o`_of_PF$num_func"
		done
	fi
	
	link_type=`ethtool $nic | grep ports: | awk '{print $4}' `
	link1=`ethtool $nic | grep "Link detected" | grep "yes\|no" -o`
	link="$link1|$link_type"
	if [ -z $link ]
	then
		link="-"
	fi

	ip="`ip -o -4 a s $nic | grep -o '[0-9]*.[0-9]*.[0-9]*.[0-9]*/[0-9]*'`  "
	ipv6=" `ip -o -6 a s $nic  | grep "scope global" | grep -oE [a-f0-9]+:+[a-f0-9]+.*\/[0-9]+` "
	if [ "$ipv6" != "  " ]; then ip=$ip$ipv6; fi
	
	if [ "$ip" == "  " ]; then ip=" - \t\t"; fi

	rdma=`ls -d /sys/class/infiniband/*/device/net/$nic 2> /dev/null| cut -d'/' -f5`;
	if [ -z $rdma ]
	then
		rdma="-"
	else
		rdma_type=`cat /sys/class/infiniband/$rdma/node_type | cut -f1 -d":"`
		if [[ ${rdma_type} == 1 ]]
		then
			rdma="$rdma,RoCE"
		elif [[ ${rdma_type} == 4 ]]
		then
			rdma="$rdma,iWARP"
		else
			rdma="$rdma,$rdma_type"
		fi
	fi
	
	mdump="-"
	if [[ -f $dbg/mdump ]]
	then
		echo status > $dbg/mdump
		if [[ $? == 0 ]]
		then 
			num_dump=`cat $dbg/mdump | grep --text num_of_logs | cut -d " " -f2 | head -n 1`
			if [[ ! -z $num_dump && $num_dump != 0 ]]
			then
				mdump="yes"
			fi
		fi
	fi

	port="-"
	if [[ -f $dbg/tests ]]
	then
		cat $dbg/tests > /dev/null
		cat $dbg/tests | grep get_phys_port > /dev/null &&
			echo get_phys_port > $dbg/tests &&
				port=`tr -d '\0' < $dbg/tests`

		if [[ $port == "100" ]]
		then
			port="-"
		fi
	fi

	if [[ "$ip" == " - \t\t" ]] 
	then
		echo -e "$dev $port $func $nic $board $chip $bdf $drv_ver $bc_ver $mac $ip $state $link $rdma $mdump"

	else	
		ip_count=0;
		for i in $ip; do
			if [[ $ip_count == 0 ]]
			then
				echo -e "$dev $port $func $nic $board $chip $bdf $drv_ver $bc_ver $mac $i $state $link $rdma $mdump"
			else
				echo -e "-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t$i\t-\t-\t-"
			fi
		ip_count=$((ip_count + 1))
		done
	fi
	vlans_if=`ip -o link show | grep $nic | grep @ | cut -d " " -f 2 | cut -d "@" -f 1`
	for vlan in $vlans_if; do
		mac=`ip -o link show $vlan | grep -o ..:..:..:..:..:.. | head -n 1`
		if [ -z $mac ]
		then
			mac="-"
		fi
		state=`ip -o link show $vlan | grep ',UP' -o | grep 'UP' -o`
		if [ -z $state ]
		then
			state="-"
		fi
		link=`ethtool $vlan | grep "Link detected" | grep "yes\|no" -o`
		if [ -z $link ]
		then
			link="-"
		fi

		ip="`ip -o -4 a s $vlan | grep -o '[0-9]*.[0-9]*.[0-9]*.[0-9]*/[0-9]*'`  "
		ipv6=" `ip -o -6 a s $nic  | grep "scope global" | grep -oE [a-f0-9]+:+[a-f0-9]+.*\/[0-9]+` "
		if [ "$ipv6" != "  " ]; then ip=$ip$ipv6; fi
		
		if [ "$ip" == "  " ]; then ip=" - \t\t"; fi

		if [[ "$ip" == " - \t\t" ]] 
		then
			echo -e "-\t$vlan\t-\t-\t-\t-\t-\t-\t$ip\t$state\t$link\t-\t-"

		else
			ip_count=0;
			for i in $ip; do
				if [[ $ip_count == 0 ]]
				then
					echo -e "-\t$vlan\t-\t-\t-\t-\t-\t-\t$i\t$state\t$link\t-\t-"
				else
					echo -e "-\t-\t-\t-\t-\t-\t-\t-\t$i\t-\t-\t-\t-"
				fi
			ip_count=$((ip_count + 1))
			done
		fi
	done	
		
}

# sandbox for child synchronization 
sandbox=/tmp/nics_sandbox
rm -rf $sandbox
mkdir -p $sandbox

# fork child for each nic 
for nic in `for i in \`ls /sys/bus/pci/drivers/$1/\`; do ls /sys/bus/pci/devices/$i/net 2> /dev/null; pci=$i; done`; do
	dev=$[$dev + 1];
	(query_nic $nic $dev > $sandbox/$nic) &
done

# wait for children
wait

# need to sort according to first line in file in case of multiple ips, etc.
# We only care about the first datum which is the enumerated dev
# Print a list of "dev filename" pairs, sort it, and grab the filename
files=$(
for i in `ls $sandbox`; do
	echo -n `head -1 $sandbox/$i | cut -f1 -d" "`
	echo " $i"
done | sort -n | cut -f2 -d" "
)

# dump the data
(
echo $column_names
echo $column_names | sed 's/[a-zA-Z0-9]/-/g' 
for i in $files; do cat $sandbox/$i; done
) | column -t

}

# main
if [ -z $1 ]
then
	nics bnx2x
	echo ""
	nics qede
else
	nics $1
fi

