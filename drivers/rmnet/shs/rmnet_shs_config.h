/* Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * RMNET Data Smart Hash stamping solution
 *
 */


#include <linux/init.h>
#include <linux/module.h>

#ifndef _RMNET_SHS_CONFIG_H_
#define _RMNET_SHS_CONFIG_H_

#define RMNET_SHS_LOG_LEVEL_ERROR	1
#define RMNET_SHS_LOG_LEVEL_INFO	2
#define RMNET_SHS_LOG_LEVEL_DEBUG	3

enum rmnet_shs_crit_err_e {
	RMNET_SHS_NETDEV_ERR,
	RMNET_SHS_INVALID_CPU_ERR,
	RMNET_SHS_MAIN_SHS_NOT_REQD,
	RMNET_SHS_MAIN_SHS_RPS_INIT_ERR,
	RMNET_SHS_MAIN_MALLOC_ERR,
	RMNET_SHS_MAIN_MAP_LEN_INVALID,
	RMNET_SHS_MAX_FLOWS,
	RMNET_SHS_WQ_ALLOC_WQ_ERR,
	RMNET_SHS_WQ_ALLOC_DEL_WQ_ERR,
	RMNET_SHS_WQ_ALLOC_HSTAT_ERR,
	RMNET_SHS_WQ_ALLOC_EP_TBL_ERR,
	RMNET_SHS_WQ_GET_RMNET_PORT_ERR,
	RMNET_SHS_WQ_EP_ACCESS_ERR,
	RMNET_SHS_WQ_COMSUME_PKTS,
	RMNET_SHS_CPU_PKTLEN_ERR,
	RMNET_SHS_NULL_SKB_HEAD,
	RMNET_SHS_RPS_MASK_CHANGE,
	RMNET_SHS_CRIT_ERR_MAX
};

extern unsigned int rmnet_shs_debug;
extern unsigned int rmnet_shs_stats_enabled;
extern unsigned long int rmnet_shs_crit_err[RMNET_SHS_CRIT_ERR_MAX];
extern struct rmnet_shs_cfg_s rmnet_shs_cfg;
extern int rmnet_is_real_dev_registered(const struct net_device *real_dev);

int __init rmnet_shs_module_init(void);
void __exit rmnet_shs_module_exit(void);


#endif /* _RMNET_SMHS_CONFIG_H_ */
