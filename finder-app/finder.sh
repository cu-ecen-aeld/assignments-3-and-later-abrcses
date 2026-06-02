#!/bin/sh

if [ $# -lt 2 ]
then
	echo "You have to provide a directory and a search string."
	exit 1
fi

FILESDIR=$1
SEARCHSTR=$2

if [ ! -d "$FILESDIR" ]
then
	echo "$FILESDIR does not name an existing directory."
	exit 1
fi

allfiles=$(find "$FILESDIR" -type f)
numfiles=0
numlinesfound=0
for file in $allfiles
do
	numfiles=$(($numfiles + 1))
	if [ -f "$file" ]
	then
		lineshere=$(grep -c "$SEARCHSTR" "$file")
		numlinesfound=$(($numlinesfound + $lineshere))
	fi
done

echo "The number of files are $numfiles and the number of matching lines are $numlinesfound"