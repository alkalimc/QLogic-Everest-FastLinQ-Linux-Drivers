#!/bin/bash

#prerequisitve
#dnf install perl-open.noarch
#yum install -y perl-open.noarch > /dev/null
#yum install -y perf > /dev/null

function usage() {
        echo -e "Usage: flamegraph.sh [ -c cpu ] [ -d duration ]"
        echo -e "\t-c\tCPU index (default: all CPUs)"
        echo -e "\t-d\tDuration in seconds (default: 10 seconds)"
        echo
}

function linux_path_to_windows_path() {
        local linux_path=$1
        echo `echo $linux_path | sed -e 's/usr\/qlc/\\\\qlogic.org/g' | tr '/' '\\' 2> /dev/null`
}

FLAMEGRAPH_DIR=/opt/FlameGraph
TMP_DIR="/tmp"
RES_DIR=`dirname $0`/results
RES_FILE=$RES_DIR/`date +'%m-%d-%y_%T' | tr ':' '-'`.svg

which perf &> /dev/null
if [[ $? != 0 ]]; then
        echo "Error: perf package is not installed on this server"
        exit 1
fi

if [ ! -d $FLAMEGRAPH_DIR ]; then
        echo "FlameGraph package wasn't found in /opt"
        echo "Cloning https://github.com/brendangregg/FlameGraph git project"
        git -C /opt clone https://github.com/brendangregg/FlameGraph.git
        #exit 1
fi

[ ! -d $RES_DIR ] && mkdir $RES_DIR

cpu_str="--all-cpus"
cpu="all"
duration=10

while getopts :c:d:h OPTION
do
        case $OPTION in
        c)
                cpu=$OPTARG
                cpu_str="--cpu $cpu"
                ;;
        d)
                duration=$OPTARG
                ;;
        h)
                usage
                exit 0
                ;;
        \?)
                echo "Error: unknown option: -$OPTARG"
                usage
                exit 1
                ;;
        :)
                echo "Error: missing option argument for -$OPTARG"
                exit 1
                ;;
        esac
done

echo
msg="Running perf on $cpu_str for $duration seconds:"
echo "$msg"
echo "$msg" | sed 's/./-/g'

cmd="perf record -F 99 --proc-map-timeout 60000 $cpu_str -g -- sleep $duration"
echo "$cmd"
echo
eval $cmd

perf script -f > $TMP_DIR/perf_script.tmp
cat $TMP_DIR/perf_script.tmp | $FLAMEGRAPH_DIR/stackcollapse-perf.pl > $TMP_DIR/out.perf-folded
cat $TMP_DIR/out.perf-folded | $FLAMEGRAPH_DIR/flamegraph.pl > $RES_FILE
if [ $? -eq 2 ]
then
	   #install missing package
	   echo "Package missing , Installing perl..."
	   yum install -y perl-open.noarch > /dev/null
	   echo "please re-run the flamegraph.sh script again."
fi

echo
echo "Result file:"
echo "$RES_FILE"
#echo `linux_path_to_windows_path $RES_FILE`
echo
