#!/bin/sh
# File: finder.sh
# Author: Sundar Krishnakumar

FILESDIR=0
SEARCHSTR=""

x=0 # No. of files in target dir and its sub-dir
y=0 # No. of string matches in the target dir

# Command line argument validation
if [ $# -lt 2 ]
then
    echo "ERROR: Total no. of arguments should be 2."
	echo "Specify 1) the path to directory and 2) the search string."
    exit 1
else # if it is not a directory exit with error code
    if [ -d "$1" ]
    then
	    FILESDIR=$1
	    SEARCHSTR=$2
    else
        echo "ERROR: ${1} does not represent a directory on the file system"
        exit 1
    fi
fi

# -R recursive. -r does not follow symbolic links. 
# -i turn of case checking
# --files-with-matches show only the files that have a match
# wc -l cmd does line count on the grep output
x=$(grep -Ri "${SEARCHSTR}" "${FILESDIR}" --files-with-matches | wc -l)

y=$(grep -Ri "${SEARCHSTR}" "${FILESDIR}" | wc -l)

echo "The number of files are ${x} and the number of matching lines are ${y}"

# Exit with success code 
exit 0











