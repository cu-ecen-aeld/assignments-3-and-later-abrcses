#!/bin/sh

if [ $# -lt 2 ]
then
	echo "You have to provide a file path and the file content."
	exit 1
fi

FILEPATH=$1
CONTENT=$2
DIRNAME=$(dirname $FILEPATH)

if [ ! -d DIRNAME ]
then
	mkdir -p "$DIRNAME"
	if [ $? -ne 0 ]
	then
		# Error message will come from mkdir
		exit 1
	fi
fi

echo "$CONTENT" > "$FILEPATH"
if [ $? -ne 0 ]
then
	# Error message will come from echo
	exit 1
fi
