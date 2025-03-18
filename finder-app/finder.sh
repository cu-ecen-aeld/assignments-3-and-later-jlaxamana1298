#!/bin/sh

# initialize arguments
if [ "$#" -ne 2 ] 
then
    echo "All parameters not specified correctly"
    exit 1
fi

filesdir="$1"
searchstr="$2"

# check if filesdir is dir
if [ ! -d "$filesdir" ]
then
    echo "Provided filesdir is not valid dir"
    exit 1
fi 

# get numfiles
numfiles=$(grep "$searchstr" -r -l "$filesdir" | wc -l)
# get numlines
numlines=$(grep "$searchstr" -s -r "$filesdir" | wc -l)

# print output
echo "The number of files are $numfiles and the number of matching lines are $numlines"

exit 0
