#!/vendor/bin/sh
#Copyright (c) 2019, 2021, The Linux Foundation. All rights reserved.
#
#This program is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License version 2 and
#only version 2 as published by the Free Software Foundation.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.
#
#
plateform_hana=1
plateform_talos=2
plateform_poipu=3
irq_num=`cat /proc/interrupts | grep -i DWC_ETH_QOS| grep -i gic | awk {'print $1'} | awk -F :  {'print $1'}`;
echo irqnum=$irq_num;
case $1 in
	$plateform_hana)
		echo $2 > /sys/class/net/eth0/queues/rx-0/rps_cpus;
		;;
	$plateform_poipu)
		echo $2 > /sys/class/net/eth0/queues/rx-0/rps_cpus;
		;;
	$plateform_talos)
		# Here 08 is forcing ISR to CPU 3
		echo 08 > /proc/irq/$irq_num/smp_affinity;
		echo $2 > /sys/class/net/eth0/queues/rx-0/rps_cpus;
		;;
	*)
		echo "Invalid plateform $1";
		;;
esac
