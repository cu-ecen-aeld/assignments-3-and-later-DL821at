#!/bin/bash

filesdir=$1
searchstr=$2

if [ -z "$filesdir" ] || [ -z "$searchstr" ]; then
    echo "Error: Arguments were not provided."
    exit 1
fi

if [ ! -d "$filesdir" ]; then
    echo "Error: The directory does not exist."
    exit 1
fi

num_files=$(find "$filesdir" -type f| wc -l)

num_matches=$(grep -r "$searchstr" "$filesdir" | wc -l)

echo "The number of files are $num_files and the number of matching lines are $num_matches"

exit 0
