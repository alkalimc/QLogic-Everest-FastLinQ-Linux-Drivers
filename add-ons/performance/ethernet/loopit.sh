#!/bin/bash

if [ "$(id -u)" != "0" ]; then
        echo "This script must be run as root" 1>&2
        exit 1
fi

function usage() {
	echo "loopit.sh <if1> [if2] "
}

if (( $# > 2 )); then
        usage
	exit 1
fi

if (( $# < 1 )); then
        usage
	exit 1
fi

DEV1=$1
IF1=$DEV1

if (( $# < 2 )); then
	DEV2=$DEV1
	IF2=$DEV1:1
	echo Using device: $DEV1
else
	if [ "$1" == "$2" ]; then
		echo Please use different devices
		exit 1
	fi
	DEV2=$2
	IF2=$DEV2
	echo Using devices: $DEV1 $DEV2
fi
#sanity for devices:
ifconfig $IF1 &> /dev/null
if [ $? != 0 ]; then
	exit 1
fi

ifconfig $IF2 &> /dev/null
if [ $? != 0 ]; then
	exit 1
fi

IP1=10.50.0.1
IP2=10.50.1.1

VIP1=10.60.0.1
VIP2=10.60.1.1

echo " -- MACHINE---                    ---VIRTUAL---"
echo "|             |                  |             |"
echo "|             |                  |             |"
echo "|  $IP1   ------------------   $VIP1  |"
echo "|             |                  |             |"
echo "|             |                  |             |"
echo "|             |                  |             |"
echo "|  $IP2   ------------------   $VIP2  |"
echo "|             |                  |             |"
echo "|             |                  |             |"
echo " -------------                    -------------"

ifconfig $IF1 $IP1/24
ifconfig $IF2 $IP2/24

iptables -t nat -L &> /dev/null
if [ $? != 0 ]; then
	echo "Kernel must support NAT tables: iptable_nat"
	exit 2
fi

# this probably overkill:
iptables --flush -t nat

# nat source IP 10.50.0.1 -> 10.60.0.1 when going to 10.60.1.1
iptables -t nat -A POSTROUTING -s $IP1 -d $VIP2 -j SNAT --to-source $VIP1

# nat inbound 10.60.0.1 -> 10.50.0.1
iptables -t nat -A PREROUTING -d $VIP1 -j DNAT --to-destination $IP1

# nat source IP 10.50.1.1 -> 10.60.1.1 when going to 10.60.0.1
iptables -t nat -A POSTROUTING -s $IP2 -d $VIP1 -j SNAT --to-source $VIP2

# nat inbound 10.60.1.1 -> 10.50.1.1
iptables -t nat -A PREROUTING -d $VIP2 -j DNAT --to-destination $IP2

MAC2=`ifconfig $IF2 | grep -o -E '([[:xdigit:]]{1,2}:){5}[[:xdigit:]]{1,2}'`
MAC1=`ifconfig $IF1 | grep -o -E '([[:xdigit:]]{1,2}:){5}[[:xdigit:]]{1,2}'`

ip route del $VIP2 &> /dev/null
ip route add $VIP2 dev $IF1
arp -d $VIP2 &> /dev/null
arp -i $DEV1 -s $VIP2 $MAC2 # IF2's mac address

ip route del $VIP1 &> /dev/null
ip route add $VIP1 dev $IF2
arp -d $VIP1 &> /dev/null
arp -i $DEV2 -s $VIP1 $MAC1 # IF1's mac address

echo Use $VIP1 when sending traffic to $IP1
echo Use $VIP2 when sending traffic to $IP2
