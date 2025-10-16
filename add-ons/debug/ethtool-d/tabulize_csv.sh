csv_file=$1
out_file=$csv_file.txt
cat $csv_file | sed 's/ /-/g' | column -t -s ',' > $out_file
