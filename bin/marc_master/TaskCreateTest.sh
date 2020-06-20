#!/bin/sh
echo TaskDirPath=$1 >$1/TaskDirPath.txt
echo ClientID=$2 >$1/ClientID.txt
#cp /tmp/test.dat $1/
#sleep 1
echo test >$1/.success
