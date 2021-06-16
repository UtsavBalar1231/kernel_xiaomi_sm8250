/* Copyright (c) 2017, 2019, The Linux Foundation. All rights reserved.

* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.

* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>

#include <linux/inetdevice.h>
#include <net/addrconf.h>
#include <linux/inet.h>
#include <asm/uaccess.h>

#define EMAC_DRV_NAME "qcom-emac-dwc-eqos"

static int __init DWC_ETH_QOS_app_init (void)
{
	struct net_device *dev;
	rtnl_lock();
	for_each_netdev(&init_net, dev) {
		if(strncmp (EMAC_DRV_NAME, netdev_drivername(dev), strlen(EMAC_DRV_NAME)) == 0)
			if (dev_change_flags(dev, dev->flags | IFF_UP) < 0)
				pr_err("EMAC_DRV_NAME:DWC_ETH_QOS_app_init: Failed to open %s\n", dev->name);
	}
	rtnl_unlock();

	pr_info("Call DWC_ETH_QOS_open function for test purpose\r\n");
	return 0;
}


static void __exit DWC_ETH_QOS_app_cleanup (void)
{
	return;
}


module_init(DWC_ETH_QOS_app_init);
module_exit(DWC_ETH_QOS_app_cleanup);
