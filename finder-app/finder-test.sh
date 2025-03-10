#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Modified to ensure compatibility with new file path requirements and output redirection
# Author: Siddhant Jajoo

set -e
set -u

# Ensure the PATH includes /usr/bin
export PATH=$PATH:/usr/bin

# Define the configuration directory
CONFIG_DIR="/etc/finder-app/conf"

NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data
username=$(cat "$CONFIG_DIR/username.txt")
assignment=$(cat "$CONFIG_DIR/assignment.txt")

if [ $# -lt 3 ]
then
    echo "Using default value ${WRITESTR} for string to write"
    if [ $# -lt 1 ]
    then
        echo "Using default value ${NUMFILES} for number of files to write"
    else
        NUMFILES=$1
    fi  
else
    NUMFILES=$1
    WRITESTR=$2
    WRITEDIR=/tmp/aeld-data/$3
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

echo "Writing ${NUMFILES} files containing string ${WRITESTR} to ${WRITEDIR}"

rm -rf "${WRITEDIR}"

# Change the configuration file path
if [ "$assignment" != 'assignment1' ]
then
    mkdir -p "$WRITEDIR"
    if [ ! -d "$WRITEDIR" ]
    then
        echo "Failed to create directory $WRITEDIR"
        exit 1
    fi
fi

for i in $( seq 1 $NUMFILES)
do
    writer "$WRITEDIR/${username}$i.txt" "$WRITESTR"
done

OUTPUTSTRING=$(finder.sh "$WRITEDIR" "$WRITESTR")

# Write output to a file
echo "${OUTPUTSTRING}" > /tmp/assignment4-result.txt

# remove temporary directories
rm -rf /tmp/aeld-data

set +e
echo ${OUTPUTSTRING} | grep "${MATCHSTR}"
if [ $? -eq 0 ]; then
    echo "success"
    exit 0
else
    echo "failed: expected  ${MATCHSTR} in ${OUTPUTSTRING} but instead found"
    exit 1
fi
