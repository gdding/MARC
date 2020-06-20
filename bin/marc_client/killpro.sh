#! /bin/sh
for i in `/sbin/pidof $1`;
do
cmd=`ls -l /proc/$i/exe  | awk '{print $11}'`
echo $i $cmd
if [ $cmd = $2 ]; then
        kill -9 $i ;
fi
done;
