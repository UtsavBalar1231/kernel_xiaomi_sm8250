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
 *
 * RMNET udp_opt
 *
 */

#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/ip6_checksum.h>
#include <net/udp.h>
#include <linux/module.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_map.h>
#include "rmnet_perf_opt.h"
#include "rmnet_perf_udp_opt.h"
#include "rmnet_perf_core.h"
#include "rmnet_perf_config.h"

/* Max number of bytes we allow udp_opt to aggregate per flow */
unsigned int rmnet_perf_udp_opt_flush_limit __read_mostly = 65000;
module_param(rmnet_perf_udp_opt_flush_limit, uint, 0644);
MODULE_PARM_DESC(rmnet_perf_udp_opt_flush_limit,
		 "Max flush limiit for udp_opt");

/* Stat showing reason for flushes of flow nodes */
unsigned long int
rmnet_perf_udp_opt_flush_reason_cnt[RMNET_PERF_UDP_OPT_NUM_CONDITIONS];
module_param_array(rmnet_perf_udp_opt_flush_reason_cnt, ulong, 0, 0444);
MODULE_PARM_DESC(rmnet_perf_udp_opt_flush_reason_cnt,
		 "udp_opt performance statistics");

/* update_udp_flush_stat() - Increment a given flush statistic
 * @stat: The statistic to increment
 *
 * Return:
 *    - void
 */
static inline void
update_udp_flush_stat(enum rmnet_perf_udp_opt_flush_reasons stat)
{
	if (stat < RMNET_PERF_UDP_OPT_NUM_CONDITIONS)
		rmnet_perf_udp_opt_flush_reason_cnt[stat]++;
}

/* udp_pkt_can_be_merged() - Check if packet can be merged
 * @flow_node:  flow node meta data for checking condition
 * @pkt_info: characteristics of the current packet
 *
 * 1. validate packet length
 * 2. check for size overflow
 *
 * Return:
 *    - rmnet_perf_upd_opt_merge_check_rc enum indicating
 *      merge status
 **/
static enum rmnet_perf_udp_opt_merge_check_rc
udp_pkt_can_be_merged(struct rmnet_perf_opt_flow_node *flow_node,
		      struct rmnet_perf_pkt_info *pkt_info)
{
	u16 gso_len;

	/* Use any previous GRO information, if present */
	if (pkt_info->frag_desc && pkt_info->frag_desc->gso_size)
		gso_len = pkt_info->frag_desc->gso_size;
	else
		gso_len = pkt_info->payload_len;

	/* 1. validate length */
	if (flow_node->gso_len != gso_len) {
		update_udp_flush_stat(RMNET_PERF_UDP_OPT_LENGTH_MISMATCH);
		return RMNET_PERF_UDP_OPT_FLUSH_SOME;
	}

	/* 2. check for size/count overflow */
	if (pkt_info->payload_len + flow_node->len >=
	    rmnet_perf_udp_opt_flush_limit) {
		update_udp_flush_stat(RMNET_PERF_UDP_OPT_64K_LIMIT);
		return RMNET_PERF_UDP_OPT_FLUSH_SOME;
	} else if (flow_node->num_pkts_held >= 50) {
		update_udp_flush_stat(RMNET_PERF_UDP_OPT_NO_SPACE_IN_NODE);
		return RMNET_PERF_UDP_OPT_FLUSH_SOME;
	}
	return RMNET_PERF_UDP_OPT_MERGE_SUCCESS;
}

/* rmnet_perf_udp_opt_ingress() - Core business logic of udp_opt
 * @pkt_info: characteristics of the current packet
 * @flush: IP flag mismatch detected
 *
 * Makes determination of what to do with a given incoming
 * ip packet. All other udp_opt based checks originate from here.
 * If we are working within this context then we know that
 * we are operating on UDP packets.
 *
 * Return:
 *		- void
 **/
void rmnet_perf_udp_opt_ingress(struct rmnet_perf_opt_flow_node *flow_node,
				struct rmnet_perf_pkt_info *pkt_info,
				bool flush)
{
	enum rmnet_perf_udp_opt_merge_check_rc rc;

	if (flush) {
		rmnet_perf_opt_update_flow(flow_node, pkt_info);
		rmnet_perf_opt_flush_single_flow_node(flow_node);
		rmnet_perf_core_flush_curr_pkt(pkt_info,
					       pkt_info->ip_len +
					       pkt_info->trans_len +
					       pkt_info->payload_len, false,
					       false);
		update_udp_flush_stat(RMNET_PERF_UDP_OPT_FLAG_MISMATCH);
		return;
	}

	/* Go ahead and insert the packet now if we're not holding anything.
	 * We know at this point that it's a normal packet in the flow
	 */
	if (!flow_node->num_pkts_held)
		goto insert;

	rc = udp_pkt_can_be_merged(flow_node, pkt_info);
	if (rc == RMNET_PERF_UDP_OPT_FLUSH_SOME)
		rmnet_perf_opt_flush_single_flow_node(flow_node);
	else if (rc == RMNET_PERF_UDP_OPT_MERGE_SUCCESS)
		pkt_info->first_packet = false;

insert:
	rmnet_perf_opt_insert_pkt_in_flow(flow_node, pkt_info);
}
