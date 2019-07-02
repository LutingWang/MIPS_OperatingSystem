#########################################################################
# File Name: file_concat.sh
# Author: ma6174
# mail: ma6174@163.com
# Created Time: Thu Apr  4 23:52:25 2019
#########################################################################
#!/bin/bash

if [[ $# > 1 ]]; then
	echo Too much arguments.
	exit
fi

# tags have space behand them
dir_tag="<!--DIR--> "
file_tag="<!--FILE--> "
end_tag="<!--END--> "

if [[ $# == 1 ]]; then
	output=$1
else
	output=file_concat_output
fi

dirname=
dir_v=0
ls -R . | while read dir
do
	if [[ $dir_v = 1 ]]; then
		if [[ $dir = "" ]]; then
			dir_v=0
		else
			filename=${dirname}${dir}
			if [[ -d $filename ]]; then
				continue
			fi
			if [[ ! -f $filename ]]; then
				echo $filename is not an ordinary file.
				continue
			fi
			echo ${file_tag}${filename} >> $output
			cat $filename >> $output
			echo >> $output
			echo ${end_tag} >> $output
		fi
	else
		if [[ ${dir:0-1} != : ]]; then
			echo Unexpected exception thrown parsing $dir.
			exit
		fi
		dir_v=1
		dirname=${dir%:*}"/"
		echo "${dir_tag}${dirname}" >> $output
	fi
done
