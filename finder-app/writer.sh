#!/bin/sh
# File: writer.sh
# Author: Sundar Krishnakumar

WRITEFILE=""
WRITESTR=""

# Command line argument validation
if [ $# -lt 2 ]
then
    echo "ERROR: Total no. of arguments should be 2."
	echo "1) A full path to the file (including filename)"
    echo "2) Text string which will be written within this file."
    exit 1
else # if it is not a file then exit with error code
    if [ -d "$1" ]
    then
        echo "ERROR: ${1} does not represent a file on the file system"
        exit 1

    else
        WRITEFILE=$1
	    WRITESTR=$2
    fi
fi

# > refirection op to overwrite the file. Creates a file if it does not exist.
$(echo ${WRITESTR} > ${WRITEFILE})

# check for return code of the echo command
if [ $? -eq 0 ]
then 
    exit 0
else
    echo "ERROR: File could not be created for the path -> ${WRITEFILE}"
    exit 1
fi 
