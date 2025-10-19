#check if debugfs is mounted if not mount it
chk=`mount | grep /sys/kernel/debug | wc --lines`
if [ "$chk" -eq 0 ]
then
        echo "Mounting debugfs to /sys/kernel/debug"
        mount -t debugfs nodev /sys/kernel/debug/
fi
