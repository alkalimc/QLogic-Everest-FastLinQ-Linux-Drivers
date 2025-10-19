#!/bin/bash

###############################################################################
# THE SOFTWARE IS PROVIDED AS A REFERENCE ONLY AND "AS IS", WITHOUT WARRANTY  #
# OF ANY KIND, EXPRESS OR IMPLIED.                                            #
###############################################################################


#
# parameters
#
function usage
{
        echo "usage: $0 <RoCE version>"
        echo "where RoCE version is either v1 or v2"
}

if [ $# -ne 1 ]; then
	usage
        exit -1
fi

if [ "$1" == "v1" ]; then
	ROCE_VER="IB/RoCE v1"
elif [ "$1" == "v2" ]; then
	ROCE_VER="RoCE v2"
else
	usage
	exit -1
fi

#
# main
#

mount -t configfs none /sys/kernel/config &> /dev/null
if [ ! -d "/sys/kernel/config/rdma_cm/" ]; then
	if [ "$1" == "v2" ] ; then
		echo "rdma_cm doesn't support default_roce_mode configuration. Failed to configure RDMA CM to RoCE v2"
		exit -1
	else
		echo "rdma_cm doesn't support default_roce_mode configuration but it is anyway by default working as RoCE v1"
		exit 0
	fi
fi

cd /sys/kernel/config/rdma_cm/
for i in `ls -d /sys/class/infiniband/qedr* | cut -d/ -f5`; do
	mkdir ${i}
	echo ${ROCE_VER} > ${i}/ports/1/default_roce_mode
	rmdir ${i}
	echo "configured rdma_cm for ${i} to ${ROCE_VER}"
done

cd - &> /dev/null

