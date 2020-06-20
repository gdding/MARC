#!/bin/sh
echo ResultDirPath=$1 >$1/ResultDirPath.txt
echo ClientID=$2 >$1/ClientID.txt
#sleep 1
echo test >$1/.success
