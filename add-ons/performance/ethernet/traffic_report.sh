#!/bin/bash

usage()
{
	echo "Usage: traffic_report.sh <netdev> <samples_num> <interval> (<lediag_dir> | NODIAG) [NOMPSTAT]"
	echo "e.g. traffic_report.sh eth0 2 10 NODIAG NOMPSTAT"
	echo "Note: Interval must be over 120 seconds for diag profiling data to be collected"
	exit
}


if [ $# != 4 ] && [ $# != 5 ]
then
	usage
fi

# params
netdev=$1
sample_num=$2
interval=$3
diag=$4

run_mpstat=1
if [ $# == 5 ]
then
	if [ "$5" != "NOMPSTAT" ]
	then
		usage
	fi
	run_mpstat=0
fi

run_diag=0
# diag needs at least 120 seconds
if [ "$diag" != "NODIAG" ]
then
	if (( $interval < 120 ))
	then
		usage
	fi
	run_diag=1
fi

# mpstat presence
mpstat &> /dev/null
if [[ $run_mpstat == 1 && "$?" != "0" ]]
then
	echo "this tool requires presence of mpstat"
	echo "install mpstat or run this tool with NOMPSTAT parameter"
	exit
fi

now=`date +%Y_%m_%d_%H_%M_%S`
outdir=/tmp/traffic_report_$hostname_$netdev_$now
hostname=`hostname`
delta=0

# report
mkdir $outdir
echo "Creating under traffic performance analysis report:"

#samples
for i in `seq 1 $sample_num`
do
	sampledir=$outdir/sample_"$delta"_secs
	mkdir $sampledir
	echo "Sample ${delta}s:"
	delta=`echo "$i $interval * p" | dc`

	echo -n "   - collecting statistics..."
	ethtool -S $netdev > $sampledir/stats.txt
	ethtool -d $netdev > $sampledir/ethd.bin
	netstat -s > $sampledir/netstat.txt
	cat /proc/interrupts | grep $netdev > $sampledir/interrupts.txt
	if [ $run_mpstat == 1 ]
	then
		mpstat -A > $sampledir/mpstat.txt
	fi
	echo " done"

	diag_duration=0
	if [ $run_diag == 1 ]
	then
		diag_starttime=`date +%s`
		profiledir=$sampledir/profile
		mkdir $profiledir
		echo -n "   - collecting diag profile..."
		echo profile -f $profiledir/profile-f.txt > $profiledir/profile.tcl
		echo profile -h $profiledir/profile-h.txt >> $profiledir/profile.tcl
		echo exit >> $profiledir/profile.tcl
		(cd $diag && ./load.sh -rc $profiledir/profile.tcl &> $profiledir/diaglog.txt)
		diag_endtime=`date +%s`
		diag_duration=$((diag_endtime-diag_starttime))
		echo " done"
	fi

	if (( $i < $sample_num ))
	then
		sleep_duration=$((interval-diag_duration))
		if (( $sleep_duration > 0 )); then
			echo "waiting..."
			sleep $sleep_duration
		fi
	fi
done

# archive
echo -n "Creating report..."
tarname="`basename $outdir`.tar.bz2"
(cd $outdir && tar cjvf $tarname * > /dev/null)
echo " done"

# done
echo "Report can be found under $outdir/$tarname"

exit 0
