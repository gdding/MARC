#!/bin/sh
eth="eth0 eth1 "

function get_eth_info()
{
	name=$1
	cat /proc/net/dev | awk -F ':' '{if(NR>2)print $1" "$2" "$9" "$10}' | awk '{print$1" "$2" "$3" "$5}' | while \
	read line
	do
		# echo $line
		data=`echo $name" "$line | awk '{if($1==$2)print $3" "$4" "$5}'`
		if ! [ -z "$data" ]
		then
			echo $data
		fi
	done
}

function sum_eth()
{
	rm -fr marc_eth.tmp
	for name in $eth
	do
		ret=`get_eth_info $name`
		echo $ret >> marc_eth.tmp
	done
	cat marc_eth.tmp | awk 'BEGIN{bps=0;pps=0;dps=0}{bps+=$1;pps+=$2;dps+=$3}END{print bps" "pps" "dps}'
}

flow=`sum_eth`
disk=`df -l -P | awk 'BEGIN{total=0;avl=0;used=0;}NR > 1{total+=$2;used+=$3;avl+=$4;}END{printf"%d", avl/total*100}'`
mem=`top -b -n 1 | grep -w Mem | awk '{printf"%d",$6/$2*100}'`
#cpu=`top -b -n 1 | grep -w Cpu | awk '{print$5}' | awk -F '%' '{printf"%d",$1}'`
cpu=`top -b -n 1 | grep -w Cpu | cut -d ',' -f 4 | awk -F '%' '{printf"%d",$1}'`
tm=`date +%s`
if ! [ -f "marc_flow.tmp" ]
then
	echo $tm" "$flow >marc_flow.tmp
	sleep 1
	tm=`date +%s`
	flow=`sum_eth`
fi
old_flow=`cat marc_flow.tmp`
new_flow=`echo $tm $flow`
echo $new_flow >marc_flow.tmp
#final_flow=`echo $old_flow $new_flow | awk '{dif_tm=$5-$1;dif_byte=$6-$2;dif_pkt=$7-$3;dif_dpkt=$8-$4;printf"%d %d %d",dif_byte*8/dif_tm, dif_pkt/dif_tm, dif_dpkt/dif_tm}'`
bps=`echo $old_flow $new_flow | awk '{dif_tm=$5-$1;dif_byte=$6-$2;dif_pkt=$7-$3;dif_dpkt=$8-$4;printf"%d",dif_byte*8/dif_tm}'`
echo "cpu_idle_ratio "$cpu >> marc_cdmn.txt
echo "disk_avail_ratio "$disk >> marc_cdmn.txt
echo "memory_avail_ratio "$mem >> marc_cdmn.txt
#echo "nic_usage(bps,pps,dps) "$final_flow
echo "nic_bps "$bps >> marc_cdmn.txt
rm -fr marc_eth.tmp
rm -fr marc_flow.tmp
