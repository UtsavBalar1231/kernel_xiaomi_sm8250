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
extern void rmnet_deliver_skb(struct sk_buff *skb, struct rmnet_port *port);
extern struct rmnet_endpoint *rmnet_get_endpoint(struct rmnet_port *port,
						 u8 mux_id);
extern int rmnet_is_real_dev_registered(const struct net_device *real_dev);
extern void rmnet_set_skb_proto(struct sk_buff *skb);
extern int (*rmnet_perf_deag_entry)(struct sk_buff *skb);
extern int rmnet_map_checksum_downlink_packet(struct sk_buff *skb, u16 len);
extern struct napi_struct *get_current_napi_context(void);
//extern int napi_gro_complete(struct sk_buff *skb);

extern int rmnet_map_flow_command(struct sk_buff *skb, struct rmnet_port *port,
				bool rmnet_perf);
extern int rmnet_map_dl_ind_register(struct rmnet_port *port,
			      struct rmnet_map_dl_ind *dl_ind);
extern int rmnet_map_dl_ind_deregister(struct rmnet_port *port,
				struct rmnet_map_dl_ind *dl_ind);
extern struct rmnet_port *rmnet_get_port(struct net_device *real_dev);
extern void rmnet_map_cmd_init(struct rmnet_port *port);
extern void rmnet_map_cmd_exit(struct rmnet_port *port);

/* Function declarations */
struct rmnet_perf *rmnet_perf_config_get_perf(void);
enum rmnet_perf_resource_management_e
	rmnet_perf_config_register_callbacks(struct net_device *dev,
					     struct rmnet_port *port);


#endif /* _RMNET_PERF_CONFIG_H_ */
