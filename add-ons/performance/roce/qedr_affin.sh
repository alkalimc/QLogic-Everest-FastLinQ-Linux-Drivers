#!/bin/bash

###############################################################################
# THE SOFTWARE IS PROVIDED AS A REFERENCE ONLY AND "AS IS", WITHOUT WARRANTY  #
# OF ANY KIND, EXPRESS OR IMPLIED.                                            #
###############################################################################



usage() {
	echo "Device $1 is not configured"
	echo "Usage: $0 [-d device ID] [-s]"
	echo "Examples"
	echo "  $0 -d 0 -s    - to show smp affinity of qedr device 0"
	echo "  $0            - to set smp affinity of all qedr devices"
}


while getopts ":d:sh" option; do
        case $option in
        d)      DEVICE_ID=$OPTARG
                ;;
        s)	SHOW=1 
                ;;
        h)	usage 
                exit 0
                ;;
        \?)
                echo "unknown option: -$OPTARG"
                exit 1
                ;;
        :)
                echo "missing option argument for -$OPTARG"
                exit 1
        ;;
        esac
done

TOTAL_INTS=`cat /proc/interrupts | grep qedr${DEVICE_ID} | wc -l`
if [ $TOTAL_INTS == "0" ]; then
        echo "qedr$1 is not loaded"
	exit 1
fi

CURRENT_INT=`grep qedr${DEVICE_ID} /proc/interrupts | cut -d ":" -f 1`
CURRENT_CPU=0
LAST_CPU=`cat /proc/cpuinfo | grep processor | tail -n1 | cut -d":" -f2`

for ((A=1; A<=${TOTAL_INTS}; A=${A}+1)) ; do
	CURRENT_INT=`grep -m $A qedr${DEVICE_ID} /proc/interrupts | tail -1  | cut -d ":" -f 1`

	if [[ "$SHOW" != "1" ]] ; then
		echo ${CURRENT_CPU} > /proc/irq/$((${CURRENT_INT}))/smp_affinity_list
	fi
	RESULT=`cat /proc/irq/$((${CURRENT_INT}))/smp_affinity`
	echo "interrupt $((${CURRENT_INT})) has affinity to $RESULT"

	((CURRENT_CPU++))
	if [[ $((CURRENT_CPU)) -gt $((LAST_CPU)) ]]; then
		CURRENT_CPU=0
	fi
done
