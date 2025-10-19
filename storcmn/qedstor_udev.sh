#!/bin/sh -e

#
# ACTION FILE: located in /lib/udev/
#

err() {
	echo "$@" >&2
	if [ -x /bin/logger ]; then
		/bin/logger -t "${0##*/}[$$]" "$@"
	fi
}

SYSFS=/sys
HOST=${GRCDUMP}
QGRCD=${SYSFS}/class/scsi_host/host${HOST}/device/grcdump
DFILE_PATH=/opt/QLogic_Corporation/GRC_Dumps
DFILE=${DFILE_PATH}/qedstor_grcdump_${HOST}_`eval date +%Y%m%d_%H%M%S`.bin

mkdir -p $DFILE_PATH &> /dev/null
# Verify grcdump binary-attribute file
if ! test -f ${QGRCD} ; then
	err "qedstor: no grcdump sysfs attribute for host $HOST!!!"
	exit 1
fi

# Go with dump
cat ${QGRCD} > ${DFILE}

# Clear dump buffer
# echo 0 > ${QGRCD}

if ! test -s "${DFILE}" ; then
	err "qedstor: no grcdump file for host ${HOST}!!!"
	rm ${DFILE}
	exit 1
fi

gzip ${DFILE}
err "qedstor: grcdump saved to file ${DFILE}.gz."
exit 0
