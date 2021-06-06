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

#include <linux/skbuff.h>
#include <net/udp.h>
#include "rmnet_perf_core.h"
#include "rmnet_perf_opt.h"

#ifndef _RMNET_PERF_UDP_OPT_H_
#define _RMNET_PERF_UDP_OPT_H_

enum rmnet_perf_udp_opt_merge_check_rc {
	/* merge pkt into flow node */
	RMNET_PERF_UDP_OPT_MERGE_SUCCESS,
	/* flush flow nodes, but insert pkt into newly empty flow */
	RMNET_PERF_UDP_OPT_FLUSH_SOME,
};

enum rmnet_perf_udp_opt_flush_reasons {
	RMNET_PERF_UDP_OPT_FLAG_MISMATCH,
	RMNET_PERF_UDP_OPT_LENGTH_MISMATCH,
	RMNET_PERF_UDP_OPT_64K_LIMIT,
	RMNET_PERF_UDP_OPT_NO_SPACE_IN_NODE,
	RMNET_PERF_UDP_OPT_NUM_CONDITIONS
};

void rmnet_perf_udp_opt_ingress(struct rmnet_perf_opt_flow_node *flow_node,
				struct rmnet_perf_pkt_info *pkt_info,
				bool flush);
#endif /* _RMNET_PERF_UDP_OPT_H_ */
