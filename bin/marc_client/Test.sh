#! /bin/sh
echo AppName=$0 >$2/AppName.txt
echo ClientID=$3 >$2/ClientID.txt
tree /home/ >$2/tree_c.txt
cp $1/*.* $2/
sleep 20
echo test >.success
