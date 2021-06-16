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
 * RMNET tcp_opt
 *
 */

#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/ip6_checksum.h>
#include <net/tcp.h>
#include <linux/module.h>
#include <asm/unaligned.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_map.h>
#include "rmnet_perf_opt.h"
#include "rmnet_perf_tcp_opt.h"
#include "rmnet_perf_core.h"
#include "rmnet_perf_config.h"

/* Max number of bytes we allow tcp_opt to aggregate per flow */
unsigned int rmnet_perf_tcp_opt_flush_limit __read_mostly = 65000;
module_param(rmnet_perf_tcp_opt_flush_limit, uint, 0644);
MODULE_PARM_DESC(rmnet_perf_tcp_opt_flush_limit,
		 "Max flush limiit for tcp_opt");

/* Stat showing reason for flushes of flow nodes */
unsigned long int
rmnet_perf_tcp_opt_flush_reason_cnt[RMNET_PERF_TCP_OPT_NUM_CONDITIONS];
module_param_array(rmnet_perf_tcp_opt_flush_reason_cnt, ulong, 0, 0444);
MODULE_PARM_DESC(rmnet_perf_tcp_opt_flush_reason_cnt,
		 "tcp_opt performance statistics");

/* Number of ip packets leaving tcp_opt. Should be less than "pre" */
unsigned long int rmnet_perf_tcp_opt_post_ip_count;
module_param(rmnet_perf_tcp_opt_post_ip_count, ulong, 0644);
MODULE_PARM_DESC(rmnet_perf_tcp_opt_post_ip_count,
		 "Number of packets of MTU size, post-tcp_opt");

unsigned long int rmnet_perf_tcp_opt_fn_seq = 0;
module_param(rmnet_perf_tcp_opt_fn_seq, ulong, 0644);
MODULE_PARM_DESC(rmnet_perf_tcp_opt_fn_seq, "flow node seq");

unsigned long int rmnet_perf_tcp_opt_pkt_seq = 0;
module_param(rmnet_perf_tcp_opt_pkt_seq, ulong, 0644);
MODULE_PARM_DESC(rmnet_perf_tcp_opt_pkt_seq, "incoming pkt seq");

/* rmnet_perf_tcp_opt_tcp_flag_flush() - Check TCP header flag to decide if
 *		immediate flush required
 * @pkt_info: characteristics of the current packet
 *
 * If the TCP header has any flag set that GRO won't accept, we will flush the
 * packet right away.
 *
 * Return:
 *    - true if need flush
 *    - false if immediate flush may not be needed
 **/
static bool
rmnet_perf_tcp_opt_tcp_flag_flush(struct rmnet_perf_pkt_info *pkt_info)
{
	struct tcphdr *tp = pkt_info->trans_hdr.tp;

	if ((pkt_info->payload_len == 0 && tp->ack) || tp->cwr || tp->syn ||
	    tp->fin || tp->rst || tp->urg || tp->psh)
		return true;

	return false;
}

/* rmnet_perf_tcp_opt_pkt_can_be_merged() - Check if packet can be merged
 * @flow_node:  flow node meta data for checking condition
 * @pkt_info: characteristics of the current packet
 *
 * 1. check src/dest IP addr and TCP port & next seq match
 * 2. check if size overflow
 *
 * Return:
 *    - true if Pkt can be merged
 *    - false if not
 **/
static enum rmnet_perf_tcp_opt_merge_check_rc
rmnet_perf_tcp_opt_pkt_can_be_merged(
				struct rmnet_perf_opt_flow_node *flow_node,
				struct rmnet_perf_pkt_info *pkt_info)
{
	struct tcphdr *tp = pkt_info->trans_hdr.tp;
	u32 tcp_seq = ntohl(tp->seq);
	u16 gso_len;

	/* Use any previous GRO information, if present */
	if (pkt_info->frag_desc && pkt_info->frag_desc->gso_size)
		gso_len = pkt_info->frag_desc->gso_size;
	else
		gso_len = pkt_info->payload_len;

	/* Use stamped TCP SEQ number if we have it */
	if (pkt_info->frag_desc && pkt_info->frag_desc->tcp_seq_set)
		tcp_seq = ntohl(pkt_info->frag_desc->tcp_seq);

	/* 1. check ordering */
	if (flow_node->next_seq ^ tcp_seq) {
		rmnet_perf_tcp_opt_fn_seq = flow_node->next_seq;
		rmnet_perf_tcp_opt_pkt_seq = ntohl(tp->seq);
		rmnet_perf_tcp_opt_flush_reason_cnt[
				RMNET_PERF_TCP_OPT_OUT_OF_ORDER_SEQ]++;
		return RMNET_PERF_TCP_OPT_FLUSH_ALL;
	}

	/* 2. check if size overflow */
	if (pkt_info->payload_len + flow_node->len >=
	    rmnet_perf_tcp_opt_flush_limit) {
		rmnet_perf_tcp_opt_flush_reason_cnt[
						RMNET_PERF_TCP_OPT_64K_LIMIT]++;
		return RMNET_PERF_TCP_OPT_FLUSH_SOME;
	} else if (flow_node->num_pkts_held >= 50) {
		rmnet_perf_tcp_opt_flush_reason_cnt[
					RMNET_PERF_TCP_OPT_NO_SPACE_IN_NODE]++;
		return RMNET_PERF_TCP_OPT_FLUSH_SOME;
	} else if (flow_node->gso_len != gso_len) {
		rmnet_perf_tcp_opt_flush_reason_cnt[
					RMNET_PERF_TCP_OPT_LENGTH_MISMATCH]++;
		return RMNET_PERF_TCP_OPT_FLUSH_SOME;
	}
	return RMNET_PERF_TCP_OPT_MERGE_SUCCESS;
}

/* rmnet_perf_tcp_opt_cmp_options() - Compare the TCP options of the packets
 *		in a given flow node with an incoming packet in the flow
 * @flow_node: The flow node representing the current flow
 * @pkt_info: The characteristics of the incoming packet
 *
 * Return:
 *    - true: The TCP headers have differing option fields
 *    - false: The TCP headers have the same options
 **/
static bool
rmnet_perf_tcp_opt_cmp_options(struct rmnet_perf_opt_flow_node *flow_node,
			       struct rmnet_perf_pkt_info *pkt_info)
{
	struct tcphdr *flow_header;
	struct tcphdr *new_header;
	u32 optlen, i;

	flow_header = (struct tcphdr *)
		      (flow_node->pkt_list[0].header_start +
		       flow_node->ip_len);
	new_header = pkt_info->trans_hdr.tp;
	optlen = flow_header->doff * 4;
	if (new_header->doff * 4 != optlen)
		return true;

	/* Compare the bytes of the options */
	for (i = sizeof(*flow_header); i < optlen; i += 4) {
		if (*(u32 *)((u8 *)flow_header + i) ^
		    *(u32 *)((u8 *)new_header + i))
			return true;
	}

	return false;
}

/* rmnet_perf_tcp_opt_ingress() - Core business logic of tcp_opt
 * @pkt_info: characteristics of the current packet
 * @flush: IP flag mismatch detected
 *
 * Makes determination of what to do with a given incoming
 * ip packet. All other tcp_opt based checks originate from here.
 * If we are working within this context then we know that
 * we are operating on TCP packets.
 *
 * Return:
 *		- void
 **/
void rmnet_perf_tcp_opt_ingress(struct rmnet_perf_opt_flow_node *flow_node,
				struct rmnet_perf_pkt_info *pkt_info,
				bool flush)
{
	struct napi_struct *napi;
	bool option_mismatch;
	enum rmnet_perf_tcp_opt_merge_check_rc rc;
	u16 pkt_len;

	pkt_len = pkt_info->ip_len + pkt_info->trans_len +
		  pkt_info->payload_len;

	if (flush || rmnet_perf_tcp_opt_tcp_flag_flush(pkt_info)) {
		rmnet_perf_opt_update_flow(flow_node, pkt_info);
		rmnet_perf_opt_flush_single_flow_node(flow_node);
		napi = get_current_napi_context();
		napi_gro_flush(napi, false);
		rmnet_perf_core_flush_curr_pkt(pkt_info, pkt_len, true, false);
		napi_gro_flush(napi, false);
		rmnet_perf_tcp_opt_flush_reason_cnt[
			RMNET_PERF_TCP_OPT_TCP_FLUSH_FORCE]++;
		return;
	}

	/* Go ahead and insert the packet now if we're not holding anything.
	 * We know at this point that it's a normal packet in the flow
	 */
	if (!flow_node->num_pkts_held) {
		rmnet_perf_opt_insert_pkt_in_flow(flow_node, pkt_info);
		return;
	}

	option_mismatch = rmnet_perf_tcp_opt_cmp_options(flow_node, pkt_info);

	rc = rmnet_perf_tcp_opt_pkt_can_be_merged(flow_node, pkt_info);
	if (rc == RMNET_PERF_TCP_OPT_FLUSH_ALL) {
		rmnet_perf_opt_flush_single_flow_node(flow_node);
		rmnet_perf_core_flush_curr_pkt(pkt_info, pkt_len, false,
					       false);
	} else if (rc == RMNET_PERF_TCP_OPT_FLUSH_SOME) {
		rmnet_perf_opt_flush_single_flow_node(flow_node);
		rmnet_perf_opt_insert_pkt_in_flow(flow_node, pkt_info);
	} else if (option_mismatch) {
		rmnet_perf_opt_flush_single_flow_node(flow_node);
		rmnet_perf_opt_insert_pkt_in_flow(flow_node, pkt_info);
		rmnet_perf_tcp_opt_flush_reason_cnt[
			RMNET_PERF_TCP_OPT_OPTION_MISMATCH]++;
	} else if (rc == RMNET_PERF_TCP_OPT_MERGE_SUCCESS) {
		pkt_info->first_packet = false;
		rmnet_perf_opt_insert_pkt_in_flow(flow_node, pkt_info);
	}
}
