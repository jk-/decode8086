#!/bin/sh

orig_file=$(basename $1)
echo -n "[Testing '${orig_file/.out/}'] "

diff -q $1 $2 1> /dev/null 2>&1

if [ $? -eq 0 ]; then
	echo ' OK'
else
	echo ' Failed'
fi
