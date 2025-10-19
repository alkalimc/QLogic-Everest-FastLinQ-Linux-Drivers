#!/bin/bash
parser_location=`cat $(dirname $0)/../data/location.txt | grep grc_parser | cut -f 2`
grc_analyzer_location="$(dirname $0)/../grc_analyzer.sh"
parsing_rc=0
pwd=$(dirname $0)

parse_and_check() {
	file_to_parse=$1
	file_type=$2

	# start analyzing the GrcDump in the background
	if [[ $file_type == "grc" ]]; then
		analysis_log=`echo $file_to_parse | sed 's/bin$/log/g'`
		$grc_analyzer_location $file_to_parse > $analysis_log 2> /dev/null &
		grc_analyzer_pid=`echo $!`
	fi

	# ILT dump is not always present
	if [[ $file_type == "ilt" && ! -f $file_to_parse ]]; then
		return
	fi

	$parser_location $file_to_parse
	if [ "$?" != 0 ]; then
		echo "Parsing $file_to_parse failed"
		parsing_rc=1
	fi

	# finished parsing the GrcDump, wait for the analyzer to finish
	if [[ $file_type == "grc" ]]; then
		echo "Running grc_analyzer..."
		wait $grc_analyzer_pid
		tail -n +2 $analysis_log
		echo -e "grc_analyzer log is located at $analysis_log\n"
	fi

	# if filetype requires tabulzation, tabulize
	if [[ $file_type == "csv" ]]; then
		echo "Tabulizing..."
		parsed_dir=`echo ${file_to_parse} | sed 's/.txt/_parsed/'`
		$(dirname $0)/tabulize_csv.sh $parsed_dir/*.csv
	fi		
}

parse_nvm_cfg() {
	nvm_cfg=$1
	nvm_meta=$2
	idle_chk=$3

	if [[ ! -f $nvm_cfg || ! -f $nvm_meta || ! -f $idle_chk ]]; then
		return
	fi
	
	parse_script=${nvm_cfg}_parse.tcl
	meta_temp=${nvm_meta}_temp
	nvm_cfg_parsed=`echo ${nvm_cfg} | sed 's/\.bin$/\.txt/g'`

	chipnum=`cat $idle_chk | grep 'chip:' -m 1 | sed 's/.*bb/57980/' | sed 's/.*ah/57940/'`
	if [[ $chipnum != 57980 && $chipnum != 57940 ]]; then
		echo "Parsing $nvm_cfg failed (unknown chip number $chipnum)"
		return
	fi

	# create a temporary tcl parsing script
	echo "#!/usr/bin/tclsh" >> $parse_script
	echo "source $pwd/internal/nvm_cfg.tcl" >> $parse_script
	echo "source $pwd/internal/parse_nvm_cfg.tcl" >> $parse_script
	echo "#                      Chip_number      NvmCfg_image     NvmMeta_image    Output_file" >> $parse_script
	echo "set rc [parse_nvm_cfg [lindex \$argv 0] [lindex \$argv 1] [lindex \$argv 2] [lindex \$argv 3]]" >> $parse_script
	echo "puts \"parse_result \$rc\"" >> $parse_script
	chmod +x $parse_script

	diag_nvm_meta=$pwd/internal/nvm_meta.tcl

	echo "Parsing nvram configuration file $nvm_cfg..."
	rc=`$parse_script $chipnum $nvm_cfg $diag_nvm_meta $nvm_cfg_parsed | grep parse_result | sed 's/parse_result //g'`

	# delete temporary files
	rm $parse_script

	# check parsing script return code
	if [[ $rc != 0 ]]; then
		echo "Parsing $nvm_cfg failed"
		return
	fi
}

create_parsed_file() {
	local filename=$1
	local scripts_dir=$2
	local parsed_folder=$3

	#run dos2unix on the file
	dos2unix $filename 2> /dev/null
	if [ "$?" != "0" ]; then
		echo "dos2unix not found. Assuming output has linux line ending."
		echo "If the parse fails please install dos2unix."
	fi

	ethtool_dump=`grep Offset $filename -m 1 -oh`
	devlink_dump=`grep dump_data $filename -m 1 -oh`
	if [ "$ethtool_dump" == "Offset" ]; then
		# remove first two lines from ethtool and the address column
		cat $filename | sed -e "s/:[ \t]*/:/" | cut -d":" -f 2- | tail -n +3 2> /dev/null > $parsed_folder/temp
	elif [ "$devlink_dump" == "dump_data" ]; then
		# remove first line from devlink dump
		cat $filename | tail -n +2 2> /dev/null > $parsed_folder/temp
	else
		# turn binary dump into ethtool-like format
		hexdump -vC $filename | awk '{print substr($0,11)}' | sed -e 's/.\{20\}$//g' | tr -s ' ' > $parsed_folder/temp
	fi
		
	# call the parsing tool
	output=`$scripts_dir/internal/ethtool-d $parsed_folder/temp $parsed_folder`
	local rc=$?

	#remove the temp file
	rm -rf $parsed_folder/temp

	return $rc
}

# check if filename was entered
if [[ $# != 1 && $# != 2 ]]
then
	echo "Usage example:"
	echo "./ethtool-d.sh <filename> [-p (for parsing grcDump files)]"
	exit 1
fi
filename=$1
parse_option=$2

# check if the file exist
if [ -f $filename ]; then
	# check if the file is given in relative path or full path
	if [ `echo "$filename" | grep -c "/"` -gt 0 ] ; then
		path=${filename%/*}
	else
		path=`pwd`
	fi

	path+="/`basename $filename`_parsed"
	mkdir -p $path

	# create file
	create_parsed_file $filename $(dirname $0) $path
	if [[ $? != 0 ]]; then
		exit 1
	fi

	echo "$output"
	result=`echo $output | cut -d " " -f1`
	num_of_engines=`ls $path/GrcDump*.bin | wc -l`

	echo "The files were created in $path"
else
	echo "Failed to create parsed file, check the filename or path"
	exit 1
fi

#check if user wants to parse GrcDump
if [ "$parse_option" == "-p" ] ; then
	if [ "$num_of_engines" == "1" ]; then
		parse_and_check $path/GrcDump0.bin grc
		parse_and_check $path/IdleChk0.txt
		parse_and_check $path/IdleChk1.txt
		parse_and_check $path/FwAsserts0.txt
		parse_nvm_cfg $path/NvmCfg10.bin $path/NvmMeta0.bin $path/IdleChk0.txt
		parse_and_check $path/ProtectionOverride0.txt csv
		parse_and_check $path/RegFifo0.txt csv
		parse_and_check $path/Ilt0.bin ilt
	else
		for i in `seq 0 $((num_of_engines - 1))`
		do
			parse_and_check $path/GrcDump$i-engine$i.bin grc
			parse_and_check $path/IdleChk$((2*i))-engine$i.txt
			parse_and_check $path/IdleChk$((2*i+1))-engine$i.txt
			parse_and_check $path/FwAsserts$i-engine$i.txt
			parse_and_check $path/ProtectionOverride$i-engine$i.txt csv
			parse_and_check $path/RegFifo$i-engine$i.txt csv
			parse_and_check $path/Ilt$i-engine$i.bin ilt
		done
		parse_nvm_cfg $path/NvmCfg10.bin $path/NvmMeta0.bin $path/IdleChk0-engine0.txt
	fi
	#parse_and_check $path/LinkDump0
fi

exit $parsing_rc
