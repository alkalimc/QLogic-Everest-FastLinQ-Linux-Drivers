# synopsis
if [ "$1" == "" ]
then
	echo "extract_asserts.sh <file> [-p]"
	echo "file is a dmesg or /var/log/messages file with an qed fw assert dump"
	echo "-p auto parse the asserts"
	exit 0;
fi

# file locations
tmpfile=/tmp/assert.sed
resfile=/tmp/assert.res
parser_location=`cat $(dirname $0)/../data/location.txt | grep grc_parser | cut -f 2`

# 1. Replace any #012 with newline
# 2. Remove timestamps in THUNDERX logs
# 3. Remove log lines standard prefix
cat $1 | sed 's/#012/\n/g' | sed 's/\[.*\] //g' | sed 's/.*kernel: //g' > $tmpfile

hdr_endline=`cat $tmpfile | grep -n -m 1 "dump-type: fw-asserts" | cut -d":" -f1`
hdr_lines=`tac $tmpfile | tail -$hdr_endline | grep -n -m 1 '^fw-version: ' | cut -d":" -f1`
hdr_lines=`echo $((hdr_lines-1))`

# build output manually. First the header (and empty line)
grep "dump-type: fw-asserts" -m 1 -B $hdr_lines $tmpfile > $resfile
echo >> $resfile

# then the asserts themselves
for i in T M U X Y P; do grep -m 1 $i"STORM" -A8 $tmpfile; echo ; done >> $resfile

# auto-parse
if [ "$2" == "-p" ]
then
	$parser_location $resfile
	if [ "$?" != 0 ]; then
		echo "Parsing failed"
		rm -f $tmpfile $resfile
		exit 10
	fi
else
	cat $resfile
fi

rm -f $tmpfile $resfile

