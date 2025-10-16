#!/bin/bash

if [ "$1" == ""  ]; then
	echo "Please enter interface name"
	echo "Example: $0 eth8"
	exit
fi

ethtool -d $1 > temp.bin
res=`echo $?`
if [ "$res" != "0" ]; then
	echo "Please enter valid interface name"
	exit
fi

$(dirname $0)/ethtool-d.sh temp.bin -p
rm -f temp.bin

