#!/bin/bash

mkdir -p "logs"

file=$1
# default values
low=${2:-524288}
high=${3:-134217728}
step=${4:-16}

# file path and name
if [[ ! -x "$file" || -d "$file" ]]; then
    echo "Cannot execute \"$file\"."
    exit 2
fi
filename=$(basename $file)

# numbers
re='^[0-9]+$'
if ! [[ $low =~ $re ]] ; then
   echo "error: Low not a number" >&2; exit 1
fi

if ! [[ $high =~ $re ]] ; then
   echo "error: High not a number" >&2; exit 1
fi

if ! [[ $step =~ $re ]] ; then
   echo "error: Step not a number" >&2; exit 1
fi

#
timestamp=$(date +%s)

raw_output="logs/$timestamp-$filename-raw.log"
csv_output="logs/$timestamp-$filename.csv"

echo -e "Writing to $raw_output and $csv_output.\n"


# prepare log and csv

echo "" > "$raw_output"
echo -e "n,p,g,average,median,minimum,stddev" > "$csv_output"

# Info on the computer
lscpu >> $raw_output

function extract_timing {
  input=$1
  name=$2
  # need to capture scientific notation
  echo "$input" | grep -Po "$name\K[0-9.]+(?:e[-+]?\d+)?"
}

# start our loop for data collection
for ((n = $low; n <= $high; n*=$step)); do
  for ((p = 1; p <= 15; p++)); do
    for ((g = 1; g <= 8192; g*=2)); do
      command="taskset -c 1-15 $file --nproc $p -n $n -c -g $g"
      output=$($command)

      o="n = $n, p = $p, g = $g: $command\n"
      echo -e "$o"
      echo -e "$o" >> $raw_output
      echo "$output"
      echo "$output" >> $raw_output

      average=$(extract_timing "$output" "Running time average: ")
      median=$(extract_timing "$output" "Running time median:  ")
      minimum=$(extract_timing "$output" "Running time minimum: ")
      stddev=$(extract_timing "$output" "Std. dev: ")

      # append
      echo -e "$n,$p,$g,$average,$median,$minimum,$stddev" >> "$csv_output"
    done
  done
done
