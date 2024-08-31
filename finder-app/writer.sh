#!/bin/bash

if [ $# -ne 2 ]; then
   echo "Error: Need to provide 2 arguments [writefile] & [writestr]."
   exit 1
fi

writefile=$1
writestr=$2

mkdir -p "$(dirname "$writefile")" 2>/dev/null

if [ ! -d "$(dirname "$writefile")" ]; then
    echo "Error: Failed to create the directory for '$writefile'."
    exit 1
fi

echo "$writestr" > "$writefile" 2>/dev/null


if [ $? -ne 0 ]; then
   echo "Error: Failed to write to '$writefile'."
   exit 1   
fi

echo "Successfully wrote to '$writefile'."
