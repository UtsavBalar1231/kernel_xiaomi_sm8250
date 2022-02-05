/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <net/tcp.h>
#include "rmnet_perf_core.h"
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_config.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_map.h>

#ifndef _RMNET_PERF_TCP_OPT_H_
#define _RMNET_PERF_TCP_OPT_H_

enum rmnet_perf_tcp_opt_merge_check_rc {
	/* merge pkt into flow node */
	RMNET_PERF_TCP_OPT_MERGE_SUCCESS,
	/* flush flow nodes, and also flush the current pkt */
	RMNET_PERF_TCP_OPT_FLUSH_ALL,
	/* flush flow nodes, but insert pkt into newly empty flow */
	RMNET_PERF_TCP_OPT_FLUSH_SOME,
};

enum rmnet_perf_tcp_opt_flush_reasons {
	RMNET_PERF_TCP_OPT_TCP_FLUSH_FORCE,
	RMNET_PERF_TCP_OPT_OPTION_MISMATCH,
	RMNET_PERF_TCP_OPT_64K_LIMIT,
	RMNET_PERF_TCP_OPT_NO_SPACE_IN_NODE,
	RMNET_PERF_TCP_OPT_FLOW_NODE_SHORTAGE,
	RMNET_PERF_TCP_OPT_OUT_OF_ORDER_SEQ,
	RMNET_PERF_TCP_OPT_PACKET_CORRUPT_ERROR,
	RMNET_PERF_TCP_OPT_LENGTH_MISMATCH,
	RMNET_PERF_TCP_OPT_NUM_CONDITIONS
};

void rmnet_perf_tcp_opt_ingress(struct rmnet_perf_opt_flow_node *flow_node,
				struct rmnet_perf_pkt_info *pkt_info,
				bool flush);
#endif /* _RMNET_PERF_TCP_OPT_H_ */
