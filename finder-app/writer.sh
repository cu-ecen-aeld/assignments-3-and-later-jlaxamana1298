#!/bin/sh

# initialize arguments
if [ "$#" -ne 2 ]
then
    echo "All parameters not specified correctly"
    exit 1
fi

writefile="$1"
writestr="$2"

# check if directory exists
writedir=$(dirname "$writefile")
if [ ! -d "$writedir" ]
then
    mkdir -p "$writedir"
fi
if [ $? -ne 0 ]
then
    echo "Error: could not create directory"
    exit 1
fi

# Write to file
echo "$writestr" > "$writefile"
if [ $? -ne 0 ]
then
    echo "Error: failed to write to file"
    exit 1
fi

echo "Successfuly wrote file"
exit 0
