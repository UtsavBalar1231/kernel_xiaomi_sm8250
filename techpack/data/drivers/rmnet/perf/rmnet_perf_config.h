/* Copyright (c) 2013-2014, 2016-2017, 2019 The Linux Foundation. All rights reserved.
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
 * RMNET PERF Data configuration engine
 *
 */

#include <linux/skbuff.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_config.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_descriptor.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_handlers.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_map.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_trace.h>
#include <../include/soc/qcom/qmi_rmnet.h>
#include "rmnet_perf_core.h"


#ifndef _RMNET_PERF_CONFIG_H_
#define _RMNET_PERF_CONFIG_H_

enum rmnet_perf_resource_management_e {
	RMNET_PERF_RESOURCE_MGMT_SUCCESS,
	RMNET_PERF_RESOURCE_MGMT_SEMI_FAIL,
	RMNET_PERF_RESOURCE_MGMT_FAIL,
};

/* rmnet based variables that we rely on*/
extern int (*rmnet_perf_deag_entry)(struct sk_buff *skb);
extern void (*rmnet_perf_desc_entry)(struct rmnet_frag_descriptor *frag_desc,
				     struct rmnet_port *port);
extern void (*rmnet_perf_chain_end)(void);


/* Function declarations */
struct rmnet_perf *rmnet_perf_config_get_perf(void);
enum rmnet_perf_resource_management_e
	rmnet_perf_config_register_callbacks(struct net_device *dev,
					     struct rmnet_port *port);


#endif /* _RMNET_PERF_CONFIG_H_ */
