
# usage: "bash sys_info.sh"         -> to collect sys info along with ehttool dump.
#        "bash sys_info.sh no_dump" -> to collect sys info withtout ethtool dump.
collect_dump=$1
report=sysinfo-`hostname`-`date | sed 's/ \|:/_/g'`
pwd=$PWD
targetdir=/tmp
basedir=$targetdir/$report
general=$basedir/general
logs=$basedir/logs
hardware=$basedir/hardware
network=$basedir/network
software=$basedir/software
roce=$basedir/roce
virtualization=$basedir/virtualization
script_errors=$basedir/errors.log

mkdir $basedir
sh `dirname $0`/debugfs/internal/mount_debugfs.sh 

#general
mkdir $general
hostname > $general/hostname
date > $general/date
whoami > $general/whoami
uptime > $general/uptime
echo "obtained general info"
#logs
mkdir $logs
dmesg > $logs/dmesg
cp /var/log/messages 2>1 $logs/var_log_messages
ls /var/crash > $logs/crash
latest_crash_dir=`ls -rt /var/crash 2>1 | tail -1`
echo $latest_crash_dir > $logs/vmcore-dmesg.txt
cat /var/crash/$latest_crash_dir/vmcore-dmesg.txt  &>> $logs/vmcore-dmesg.txt
echo "obtained logs"

#hardware
mkdir $hardware
cat /proc/cpuinfo > $hardware/cpuinfo
cat /proc/meminfo > $hardware/meminfo
cat /proc/iomem > $hardware/iomem
cat /proc/pagetypeinfo > $hardware/pagetypeinfo
cat /proc/zoneinfo > $hardware/zoneinfo
cat /proc/buddyinfo > $hardware/buddyinfo
cat /proc/interrupts > $hardware/interupts
for irq in `ls /proc/irq`; do echo -n "irq $irq affinity "; cat /proc/irq/$irq/smp_affinity 2> /dev/null; done > $hardware/affinity
df -h > $hardware/df
lspci -xxxx -vvv > $hardware/lspci 2>> $script_errors
lspci -tv > $hardware/lspci-tv 2>> $script_errors
lscpu > $hardware/lscpu
dmidecode > $hardware/dmidecode
echo "obtained hardware info"

#software
mkdir $software
uname -a > $software/uname
cat /proc/cmdline > $software/cmdline
cat /etc/os-release &> $software/os-release
cp /lib/modules/`uname -a | cut -d" " -f3`/build/.config $software/config
lsb_release -a > $software/lsb_release 2>> $script_errors
lsmod > $software/lsmod
rpm -qa > $software/rpm 2>> $script_errors
modinfo bnx2x > $software/modinfo-bnx2x 2>> $script_errors
modinfo qed > $software/modinfo-qed 2>> $script_errors
modinfo qede > $software/modinfo-qede 2>> $script_errors
service --status-all &> $software/services
cp ~/.bash_history 2>1 $software/cmd_history
echo "::Secure Boot info::" &> $software/misc
mokutil --sb-state &>> $software/misc

netstat -nlp &> $software/netstat
netstat -s &>> $software/netstat
netstat -nr &>> $software/netstat
netstat -i &>> $software/netstat


ulimit -a &>> $software/misc
echo "::uptime::" &>> $software/misc
uptime &>> $software/misc
echo "::devlink health::" &>> $software/misc
devlink health &>> $software/misc
echo "::Firmware files::" &> $software/firmware
ls /lib/firmware/qed/* &>> $software/firmware
echo "::Process info::" &> $software/processes
echo "PS" &>> $software/processes
ps -eo pcpu,pid,user,args | sort -k 1 -r &>> $software/processes
echo "TOP" &>> $software/processes
top -b -d 2 -n 2 &>> $software/processes
echo "MPSTAT" &>> $software/processes
mpstat -P ALL 2 2 &>> $software/processes

#docker
echo "Collecting Docker networking info"
which docker &> /dev/null
if [[ $? == 0 ]] ; then
docker ps -a &> $software/docker
docker network ls &>> $software/docker
for container in `docker ps -q`; do
	# show the name of the container
	docker inspect --format='{{.Name}}' $container &>> $software/docker
	docker inspect --format='{{range .NetworkSettings.Networks}}{{.MacAddress}}{{end}}' $container &>> $software/docker
	docker inspect --format='{{range $p, $conf := .NetworkSettings.Ports}} {{$p}} -> {{(index $conf 0).HostPort}} {{end}}' $container &>> $software/docker
done
fi
echo "obtained software info"

#ovs info
ps -ef | grep ovs &> $software/ovs
ovs-ctl show 2>1  &> $software/ovs
ovs-vsctl show 2>1  &>> $software/ovs

#RoCE
mkdir $roce
modinfo qedr &>> $roce/roceinfo
if type ofed_info &>/dev/null; then
        ofed_info &>> $roce/roceinfo
else
        echo "INBOX OFED" >> $roce/roceinfo
fi
if type ibv_devinfo &>/dev/null; then
        ibv_devinfo -v  &>> $roce/roceinfo
else
        echo "ibv_devinfo missing" &>> $roce/roceinfo
fi
if type ib_write_bw &> /dev/null; then
        echo "ib_write_bw" &>> $roce/roceinfo
        ib_write_bw --version &>> $roce/roceinfo
else
        echo "ib_write_bw is missing" &>> $roce/roceinfo
fi
for QEDR in `ls /sys/kernel/debug/qedr 2> /dev/null`; do
	echo >> $roce/roceinfo
	echo $QEDR >> $roce/roceinfo
	cat /sys/kernel/debug/qedr/$QEDR/stats &>> $roce/roceinfo
done 
cp /etc/security/limits.conf $roce/limits.conf
cp -r /etc/security/limits.d $roce
cp /proc/sys/vm/max_map_count $roce
echo "obtained roce info"

#network
mkdir $network 
lshw -c network -businfo > $network/lshw_net 2>1
ip -d -d -o link show > $network/ip-link-show
ip address show > $network/ip-address-show
ifconfig -a > $network/ifconfig-a
route > $network/route
arp > $network/arp
netstat -s > $network/netstat-s
tc -s qdisc > $network/qdisc
for nic in `for i in \`ls /sys/bus/pci/devices/\`; do ls /sys/bus/pci/devices/$i/net 2> /dev/null; done`; do echo -e "\n$nic"; ethtool -i $nic 2> /dev/null; done > $network/ethtool-i
for nic in `for i in \`ls /sys/bus/pci/devices/\`; do ls /sys/bus/pci/devices/$i/net 2> /dev/null; done`; do echo -e "\n$nic"; ethtool -k $nic 2> /dev/null; done > $network/ethtool-k
for nic in `for i in \`ls /sys/bus/pci/devices/\`; do ls /sys/bus/pci/devices/$i/net 2> /dev/null; done`; do echo -e "\n$nic"; ethtool -S $nic 2> /dev/null; done > $network/ethtool-S
for nic in `for i in \`ls /sys/bus/pci/devices/\`; do ls /sys/bus/pci/devices/$i/net 2> /dev/null; done`; do echo -e "\n$nic"; ethtool -a $nic 2> /dev/null; done > $network/ethtool-a
for nic in `for i in \`ls /sys/bus/pci/devices/\`; do ls /sys/bus/pci/devices/$i/net 2> /dev/null; done`; do echo -e "\n$nic"; ethtool -g $nic 2> /dev/null; done > $network/ethtool-g
for nic in `for i in \`ls /sys/bus/pci/devices/\`; do ls /sys/bus/pci/devices/$i/net 2> /dev/null; done`; do echo -e "\n$nic"; ethtool -l $nic 2> /dev/null; done > $network/ethtool-l
for nic in `for i in \`ls /sys/bus/pci/devices/\`; do ls /sys/bus/pci/devices/$i/net 2> /dev/null; done`; do echo -e "\n$nic"; ethtool -n $nic 2> /dev/null; done > $network/ethtool-n

for nic in `for i in \`ls /sys/bus/pci/devices/\`; do ls /sys/bus/pci/devices/$i/net 2> /dev/null; done`; do echo -e "\n$nic"; ethtool --module-info $nic 2> /dev/null; done > $network/ethtool-module-info
for nic in `for i in \`ls /sys/bus/pci/devices/\`; do ls /sys/bus/pci/devices/$i/net 2> /dev/null; done`; do echo -e "\n$nic"; ethtool $nic 2> /dev/null; done > $network/ethtool
cp -r /etc/sysconfig/network-scripts $network 2>> $script_errors
cp -r /etc/sysconfig/network/scripts $network 2>> $script_errors
`dirname $0`/nics.sh > $network/nics 2>> $script_errors
echo "obtained network info"

#virtualization
mkdir $virtualization
virsh list --all > $virtualization/virsh-list 2>> $script_errors
cp /sys/module/kvm/parameters/allow_unsafe_assigned_interrupts $virtualization/allow_unsafe_assigned_interrupts 2>> $script_errors
echo "obtained virtualization info"

#collection ethtool dump
if [[ $collect_dump == "no_dump" ]]; then
	echo "Skipping ethtool dump collection"
else
	for nic in `for i in \`ls /sys/bus/pci/drivers/qede/\`; do ls /sys/bus/pci/devices/$i/net 2> /dev/null; pci=$i; done`; do
		dev=$[$dev + 1];
		timeout 5 ethtool -d $nic raw on > $basedir/ethd_$nic.bin
		break
	done
fi

#collect autogenerated dump latest 8
ls -lt /tmp/qed_dump*.bin 2>1 |  head -8 | awk '{print $NF}' | xargs -i cp -r {} $basedir/.

echo report created under $basedir
cd $targetdir
tar czvf $report.tar.gz $report > /dev/null
echo "All Logs collected Please send -> $targetdir/$report.tar.gz"
cd $pwd
