#!/bin/bash

###############################################################################
# THE SOFTWARE IS PROVIDED AS A REFERENCE ONLY AND "AS IS", WITHOUT WARRANTY  #
# OF ANY KIND, EXPRESS OR IMPLIED.                                            #
###############################################################################

#
# main
#

mount -t configfs none /sys/kernel/config &> /dev/null
if [ ! -d "/sys/kernel/config/rdma_cm/" ]; then
        echo "RDMA CM supports RoCE v1 only"
	exit 0
fi

cd /sys/kernel/config/rdma_cm/
for i in `ls -d /sys/class/infiniband/qedr* | cut -d/ -f5`; do
	mkdir ${i}
	ROCE_VER=`cat ${i}/ports/1/default_roce_mode`
	rmdir ${i}
	echo "${i} is configured to ${ROCE_VER}"
done

cd - &> /dev/null
