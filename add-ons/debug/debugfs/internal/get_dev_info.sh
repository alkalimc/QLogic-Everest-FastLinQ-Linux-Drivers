#!/bin/bash

echo_bdf()
{
	ls /sys/kernel/debug/qed/$1/bus &> /dev/null
	rc=$?
	if [ "$rc" != "0" ]; then
		bdf=`echo $1 | sed -e 's/\./.0/g'`
	fi

	echo $bdf
}

dev=0

# search through all the devices until find the requested
for nic in `for i in \`ls /sys/bus/pci/drivers/qede/\`; do ls /sys/bus/pci/devices/$i/net 2> /dev/null; done`;
do	
	bdf=`ethtool -i $nic | grep bus | grep "[0-9a-f][0-9a-f]:[0-9a-f][0-9a-f]\.[0-9a-f]" -o`

	# search by device number
	if [ "$1" == "dev_num" ] ; then

		# cehck if match requested interface
		if [ $dev == $2 ]; then
			
			# return nic name
			if [ "$3" == "nic" ]; then
				echo $nic
				break

			# return bdf number
			else
				echo_bdf $bdf
				break
			fi 
		fi

	# search by nic name
	else
		if [ $nic == $2 ]; then
			echo_bdf $bdf
			break
		fi	
	fi

	dev=$[$dev + 1]	
done
