#!/bin/bash

mkdir -p "logs"

# file path and name
file=$1
if [[ ! -x "$file" || -d "$file" ]]; then
    echo "Cannot execute \"$file\"."
    exit 2
fi
filename=$(basename $file)

#
timestamp=$(date +%s)

raw_output="logs/$timestamp-$filename-raw.log"
csv_output="logs/$timestamp-$filename.csv"

echo -e "Writing to $raw_output and $csv_output.\n"


# prepare log and csv

echo "" > "$raw_output"
echo -e "n,p,average,median,minimum,stddev" > "$csv_output"

# Info on the computer
lscpu >> $raw_output

function extract_timing {
  input=$1
  name=$2
  echo "$input" | grep -Po "$name\K[0-9.]*"
}

# start our loop for data collection
for ((n = 100000; n <= 100000000; n*=10)); do
  for ((p = 1; p <= 20; p++)); do
    output=$(taskset -c 0-19 "$file" --nproc $p -n $n -c)

    o="n = $n, p = $p: \n"
    echo -e "$o"
    echo -e "$o" >> $raw_output
    echo "$output" >> $raw_output

    average=$(extract_timing "$output" "Running time average: ")
    median=$(extract_timing "$output" "Running time median:  ")
    minimum=$(extract_timing "$output" "Running time minimum: ")
    stddev=$(extract_timing "$output" "Std. dev: ")

    # append
    echo -e "$n,$p,$average,$median,$minimum,$stddev" >> "$csv_output"
  done
done
