# Synopsis
if [ $# != 2 ]; then
	echo "$0 [ -i index | -n netdev | -b bdf ]"
	exit -1
fi

conf=/tmp/conf.txt
echo dumping configuration from device to $conf...
./debugfs_dump.sh $1 $2 > /tmp/conf.txt
vim /tmp/conf.txt
echo applying configuration from $conf to device...
./debugfs_conf.sh $1 $2 /tmp/conf.txt
echo done

