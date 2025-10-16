# synopsis
if [ "$#" != "1" ]
then
	echo "usage:	grc_analyzer.sh [grc dump text/bin file]"
	echo "example:	grc_analyzer.sh GrcDump.txt"
	exit 1
fi

# params
grc_file=$1
xsltfile=`dirname $0`/GrcAnalyzer/GRCAHtmlViewer.xslt

# sanity on input
if [[ `cat $grc_file | grep -oE 'dump-type..grc-dump'` ]]
then
	echo "Sending dump to Agent1 for analysis..."
else
	echo "input file is not a grc dump file, skipping"
	exit 0
fi

# connectivity to ildtviewer01.qlc.com
ping ildtviewer01.qlc.com -c1 &> /dev/null
if [ "$?" != "0" ]
then
	echo "cannot reach ildtviewer01.qlc.com"
	exit 1
fi

outbase="${grc_file%.*}"
xmlfile=$outbase.xml
htmlfile=$outbase.html

# get analysis xml frmo agent1
curl -X POST -F 'params=-f -t raw -p 0 1 -w auto -m auto' -F 'grcdmp_file=@'${grc_file}';type=text/plain' http://ildtviewer01.qlc.com/GrcAnalyzer-Official\(Z:\)/GrcAnalyzer.run > $xmlfile
if [ "$?" != "0" ]
then
	echo "curl failed"
	exit 1
else
	echo created $xmlfile
fi

# convert to html with xslt
xsltproc $xsltfile  $xmlfile  > $htmlfile 2> /dev/null
if [ "$?" != "0" ]
then
	echo "xslt failed"
	exit 1
else
	echo created $htmlfile
fi


