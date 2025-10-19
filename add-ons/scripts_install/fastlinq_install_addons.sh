#!/bin/bash

INS_TARGET="/usr/local/bin"
PREFIX=":"
TGT_NAME="$INS_TARGET/add-ons"
FLQ_SH="fastlinq_install.sh"

function usage() {
	echo "$0 -d <add-ons path> to copy scripts to target"
	echo "$0 -i -d <add-ons path> to copy scripts and install PATH"
	echo "$0 -u -d <add-ons path> to uninstall scripts and PATH"
}

# Generate a PATH for all add-ons subdirs.
# Generate a script to be imported in /etc/profile.d
function generate_qpath() {
	add_dir=$1
	# Find folders and append to QPATH
	QPATH=""
	list=`find $add_dir -type d`
	for ent in $list
	do
		QPATH+=$PREFIX$ent
	done

	# Generate script and install in /etc/profile.d/
	echo "export PATH=\$PATH$QPATH" >> $FLQ_SH

	if [ -f /etc/profile.d/$FLQ_SH ]
	then
		diff $FLQ_SH /etc/profile.d/$FLQ_SH >> /dev/null
		if [ $? == 0 ]
		then
			echo "no need to update /etc/profile.d"
			rm -f $FLQ_SH
		else
			mv $FLQ_SH /etc/profile.d/
		fi
	else
		mv $FLQ_SH /etc/profile.d/
	fi
}

# Copy scripts from provided src folder to target folder
function copy_scripts() {
	src=$1
	mv $src/* $TGT_NAME
}

# Check usage
if [ $# -lt 2 ]; then
        usage
        exit 1
fi

flag=""
is_package=""
while getopts ":d:f:iup" option; do
        case $option in
        d)
        	addons_dir=$OPTARG
                ;;
	f)
		script_path=$OPTARG
		;;
	i)
                flag="-i"
                ;;
        u)
                flag="-u"
                ;;
	p)
		is_package="-p"
		;;
        \?)
                echo "unknown option: -$OPTARG"
                exit 1
                ;;
        :)
                echo "option -$OPTARG requires add-ons path as arg."
                exit 2
        esac
done

# Remove scripts if uninstall requested
if [ "$flag" == "-u" ]
then
	rm -f /etc/profile.d/$FLQ_SH
	rm -rf $TGT_NAME

	echo "Fastlinq scripts removed"
	exit 0
fi

# Generate temp folder to hold scripts
temp="temp_addons"
mkdir $temp

# Copy scripts and files to temp folder
if [ "$is_package" == "-p" ]
then
	# Running from fastlinq package
	cp -r $addons_dir/* $temp/
else
	$script_path/fastlinq_get_addons.sh $addons_dir $temp >> /dev/null
fi

# Remove .c files, only compiled bin needed
find $temp -name eth*.c | xargs rm -f
rm -rf $temp/scripts_install

# See if already installed target needs updating
if [ -d $TGT_NAME ]
then
	diff -r $temp $TGT_NAME >> /dev/null
	if [ $? == 0 ]
	then
		echo "No need to update scripts"
	else
		echo "Scripts need to be updated"
		rm -rf $TGT_NAME
		mkdir -p $TGT_NAME
		copy_scripts $temp
	fi
else
	# Target does not exist, create it
	mkdir -p $TGT_NAME
	copy_scripts $temp
fi

# Now we confirmed target folder is in-place,
# Check and install PATH if requested.
if [ "$flag" == "-i" ]
then
	# Path installation requested
	generate_qpath $TGT_NAME
fi

rm -rf $temp

# Check if add-ons is already present in path of current session
present=`echo $PATH | grep add-ons`
if [ "$present" == " " ]
then
	echo "Copied add-ons scripts! Please run 'source /etc/profile' to use scripts in curr session"
else
	echo "Copied add-ons scripts!"
fi
