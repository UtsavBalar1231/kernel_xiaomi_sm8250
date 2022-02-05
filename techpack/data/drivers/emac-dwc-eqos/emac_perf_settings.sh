#!/vendor/bin/sh
#Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
echo 12582912 > /proc/sys/net/core/wmem_max;
echo 12582912 > /proc/sys/net/core/rmem_max;
echo 10240 87380 12582912 > /proc/sys/net/ipv4/tcp_rmem;
echo 10240 87380 12582912 > /proc/sys/net/ipv4/tcp_wmem;
echo 12582912 > /proc/sys/net/ipv4/udp_rmem_min;
echo 12582912 > /proc/sys/net/ipv4/udp_wmem_min;
echo 1 > /proc/sys/net/ipv4/tcp_window_scaling;
echo 18 > /sys/class/net/eth0/queues/rx-0/rps_cpus;

