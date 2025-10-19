#!/bin/bash

# mount /sys/kernel/debug/
$(dirname $0)/internal/mount_debugfs.sh
readme_file="$(dirname $0)/../readme.txt"

error_message(){
	# get usage message from readme file
	start=`grep "Debugfs usage:" $readme_file -n | cut -f1 -d:` # get the first line
	end=`grep "# End of debugfs usage" $readme_file -n | cut -f1 -d:` # get the last line
	length=$(( end - start )) # calculate the length of the text
	end=$(( end - 1 )) # don't print the last line 
	head -n $end $readme_file | tail -n $length # print the result text
	exit 255
}

count=1
test_input=""
for input in $*
do
	# s_command checks if it suppose to get option or operation
	s_command=$(( $count % 2 ))

	# prefix takes the first char of every command
	prefix=`expr substr $input 1 1`

	# checking if the command is odd and contains '-'
	if [ "$prefix" == "-" -a  $s_command !=  0 ]
	then

		# option contains the chosen option 
		option=`expr substr $input 2 2`

		if [ "$option" == p  ]; then
			# check on which location the script is running
			parser_location=`cat $(dirname $0)/../data/location.txt | grep grc_parser | cut -f 2`
			count=0
		fi

	# handle the chosen option
	elif [ "$prefix" != "-" -a $s_command ==  0 ]
	then
		if [ "$option" == n ]; then
			bdf=`$(dirname $0)/internal/get_dev_info.sh nic $input bdf`
		fi
		if [ "$option" == i ]; then
			bdf=`$(dirname $0)/internal/get_dev_info.sh dev_num $input bdf`
		fi
		if [ "$option" == b ]; then
			bdf=$input
		fi
		if [ "$option" == d ]; then
			operation=$input
		fi
		if [ "$option" == o ]; then
			output_file=$input
		fi
		if [ "$option" == s ]; then
			source_file=$input
		fi
		if [ "$option" == P ]; then
			port=$input
		fi
		if [ "$option" == t ]; then
				if [ "$test_input" == "" ]; then
						test_input="$input"
				else
						test_input="$test_input $input"
				fi
				count=$[$count - 1]
		fi

	else
		error_message	
	fi
	count=$[$count + 1]
done

# checks if all the necessary varibles were entered
if [ "$1" == "" ]
then
	echo "No options were entered"
	error_message
fi

# qede debugfs node
if [ "$operation" == "tx_timeout" ]; then
	if [ ! -f /sys/kernel/debug/qede/$bdf/$operation ]; then
		echo "qede is not loaded"
		exit 10
	fi

	if [ "$test_input" == "" ]; then
		cat /sys/kernel/debug/qede/$bdf/$operation
	else
		echo $test_input > /sys/kernel/debug/qede/$bdf/$operation
	fi

	exit 0
fi

if [[ "$bdf" == ""   && ("$operation" != "preconfig"  &&  "$operation" != "postconfig") ]] ; then
	echo "Unrecognized Bdf/Function name entered"
	error_message
fi


if [ "$test_input" != "" ]; then
	if [ "$test_input" == "list" ]; then
		cat /sys/kernel/debug/qed/$bdf/tests
	else
		echo $test_input > /sys/kernel/debug/qed/$bdf/tests
	fi
	exit 0 
fi

if [ "$operation" == "" ]; then
	echo "No Feature was entered"
	error_message
else
	if ( [ "$operation" != "phy" ] && [ "$operation" != "phy_mac_stat" ] && [ "$operation" != "phy_info" ] && [ "$operation" != "preconfig" ] && [ "$operation" != "grc" ] && [ "$operation" != "idle_chk" ] && [ "$operation" != "mcp_trace" ] && [ "$operation" != "bus" ] && [ "$operation" != "postconfig" ] && [ "$operation" != "reg_fifo" ] && [ "$operation" != "igu_fifo" ] && [ "$operation" != "protection_override" ] && [ "$operation" != "ilt" ] && [ "$operation" != "internal_trace" ] && [ "$operation" != "linkdump_phydump" ] ); then
		echo "Unrecognized debug feature. Available features are: idle_chk, mcp_trace, grc, preconfig, postconfig, bus, reg_fifo, igu_fifo, protection_override, ilt, internal_trace, linkdump_phydump."
		error_message
	fi
fi

if [ "$operation" == "phy" ]; then
	if [ "$source_file" == "" ]; then
		error_message
		exit 0
	else
		cat $source_file > /sys/kernel/debug/qed/$bdf/phy
		if [ "$output_file" == ""  ]; then
			cat /sys/kernel/debug/qed/$bdf/phy
		else
			cp /sys/kernel/debug/qed/$bdf/phy $output_file
		fi
	fi
fi

if [ "$operation" == "phy_info" ]; then
	if [ "$source_file" != "" ]; then
		error_message
		exit 0
	else
		echo info > /sys/kernel/debug/qed/$bdf/phy
		if [ "$output_file" == ""  ]; then
			cat /sys/kernel/debug/qed/$bdf/phy
		else
			cp /sys/kernel/debug/qed/$bdf/phy $output_file
		fi
		exit 0
	fi
fi

if [ "$operation" == "phy_mac_stat" ]; then
	if ( [ "$source_file" != "" ] || [ "$port" == "" ] ); then
		error_message
		exit 0
	else
		echo mac_stat $port > /sys/kernel/debug/qed/$bdf/phy
		if [ "$output_file" == ""  ]; then
			cat /sys/kernel/debug/qed/$bdf/phy
		else
			cp /sys/kernel/debug/qed/$bdf/phy $output_file
		fi
		exit 0
	fi
fi


# checks if operation variable was entered and no source file was entered
# if source file was entered the source file will be entered into debugfs
# else 'dump' will be entered
if [ "$source_file" == "" ] ; then 
	if ( [ "$operation" != "preconfig" ] && [ "$operation" != "postconfig" ] ) ; then
		echo dump > /sys/kernel/debug/qed/$bdf/$operation
	fi
else
	if ( [ "$operation" == "preconfig" ] || [ "$operation" == "postconfig" ] ) ; then
		# preconfig and postconfig are located above the debugfs bdf dir
		if [ "$bdf" != "" ]; then
			echo "For using preconfig or postconfig there is no need for choosing device"
			error_message
		fi
		if  [ "$operation" != "postconfig" ] ; then
			cat $source_file > /sys/kernel/debug/qed/preconfig
			exit 0
		fi
	else
		cat $source_file > /sys/kernel/debug/qed/$bdf/$operation
		# bus operation is not expected to continue after receiving source file
		if  [ "$operation" == "bus" ] ; then
			exit 0
		fi
	fi
fi

# checks if certain commands and output file was entered, copies dump to output file
# if preconfig and output file exist enters 'dump' to debug bus and copies debug bus to output file
if ( [ "$operation" == "grc" ] || [ "$operation" == "preconfig" ] || [ "$operation" == "bus" ]  || [ "$operation" == "postconfig" ] || [ "$operation" == "ilt" ] || [ "$operation" == "internal_trace" ] || [ "$operation" == "linkdump_phydump" ] ); then
	if [ "$output_file" != ""  ]; then
		if  [ "$operation" == "preconfig" ]; then
			echo "To receive output after using preconfig please use bus output"
			error_message
		elif [ "$operation" == "postconfig" ]; then
			cp /sys/kernel/debug/qed/$operation $output_file
		else
			cp /sys/kernel/debug/qed/$bdf/$operation $output_file
		fi
		chmod a+rw $output_file

		if [ "$parser_location" != "" ]; then
			$parser_location $output_file 
			if [ "$?" != 0 ]; then
				echo "Parsing failed"
				exit 10
			fi
		fi

	else
		echo "No output file was entered"
		error_message
	fi
fi

# checks if idle check or mcp trace or reg fifo or protection override command was entered - if yes print the idle check
if ( [ "$operation" == "idle_chk" ] || [ "$operation" == "mcp_trace" ] || [ "$operation" == "reg_fifo" ] || [ "$operation" == "igu_fifo" ] || [ "$operation" == "protection_override" ] || [ "$operation" == "ilt" ] ) ; then
	if [ "$output_file" == ""  ] ; then
		cat /sys/kernel/debug/qed/$bdf/$operation
	else
		cat /sys/kernel/debug/qed/$bdf/$operation > $output_file
		if ([ "$operation" == "idle_chk" ] && [ "$parser_location" != "" ]); then
			$parser_location $output_file 
			if [ "$?" != 0 ]; then
				echo "Parsing failed"
				exit 10
			fi
		fi
	fi
fi

exit 0
