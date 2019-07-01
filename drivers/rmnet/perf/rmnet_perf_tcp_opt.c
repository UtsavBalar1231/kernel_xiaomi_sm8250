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
unsigned int rmnet_perf_tcp_opt_flush_limit __read_mostly = 65536;
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
	struct tcphdr *tp = pkt_info->trns_hdr.tp;

	if ((pkt_info->payload_len == 0 && tp->ack) || tp->cwr || tp->syn ||
	    tp->fin || tp->rst || tp->urg || tp->psh)
		return true;

	return false;
}

/* rmnet_perf_tcp_opt_pkt_can_be_merged() - Check if packet can be merged
 * @skb:        Source socket buffer containing current MAP frames
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
rmnet_perf_tcp_opt_pkt_can_be_merged(struct sk_buff *skb,
				struct rmnet_perf_opt_flow_node *flow_node,
				struct rmnet_perf_pkt_info *pkt_info)
{
	struct iphdr *ip4h;
	struct ipv6hdr *ip6h;
	u16 payload_len = pkt_info->payload_len;
	struct tcphdr *tp = pkt_info->trns_hdr.tp;

	/* cast iph to right ip header struct for ip_version */
	switch (pkt_info->ip_proto) {
	case 0x04:
		ip4h = pkt_info->iphdr.v4hdr;
		if (((__force u32)flow_node->next_seq ^
		    (__force u32) ntohl(tp->seq))) {
			rmnet_perf_tcp_opt_fn_seq = flow_node->next_seq;
			rmnet_perf_tcp_opt_pkt_seq = ntohl(tp->seq);
			rmnet_perf_tcp_opt_flush_reason_cnt[
				RMNET_PERF_TCP_OPT_OUT_OF_ORDER_SEQ]++;
			return RMNET_PERF_TCP_OPT_FLUSH_ALL;
		}
		break;
	case 0x06:
		ip6h = (struct ipv6hdr *) pkt_info->iphdr.v6hdr;
		if (((__force u32)flow_node->next_seq ^
		    (__force u32) ntohl(tp->seq))) {
			rmnet_perf_tcp_opt_fn_seq = flow_node->next_seq;
			rmnet_perf_tcp_opt_pkt_seq = ntohl(tp->seq);
			rmnet_perf_tcp_opt_flush_reason_cnt[
				RMNET_PERF_TCP_OPT_OUT_OF_ORDER_SEQ]++;
			return RMNET_PERF_TCP_OPT_FLUSH_ALL;
		}
		break;
	default:
		pr_err("Unsupported ip version %d", pkt_info->ip_proto);
		rmnet_perf_tcp_opt_flush_reason_cnt[
				RMNET_PERF_TCP_OPT_PACKET_CORRUPT_ERROR]++;
		return RMNET_PERF_TCP_OPT_FLUSH_SOME;
	}

	/* 2. check if size overflow */
	if ((payload_len + flow_node->len >= rmnet_perf_tcp_opt_flush_limit)) {
		rmnet_perf_tcp_opt_flush_reason_cnt[
						RMNET_PERF_TCP_OPT_64K_LIMIT]++;
		return RMNET_PERF_TCP_OPT_FLUSH_SOME;
	} else if ((flow_node->num_pkts_held >= 50)) {
		rmnet_perf_tcp_opt_flush_reason_cnt[
					RMNET_PERF_TCP_OPT_NO_SPACE_IN_NODE]++;
		return RMNET_PERF_TCP_OPT_FLUSH_SOME;
	} else if (flow_node->gso_len != payload_len) {
		rmnet_perf_tcp_opt_flush_reason_cnt[
					RMNET_PERF_TCP_OPT_LENGTH_MISMATCH]++;
		return RMNET_PERF_TCP_OPT_FLUSH_SOME;
	}
	return RMNET_PERF_TCP_OPT_MERGE_SUCCESS;
}

/* rmnet_perf_tcp_opt_check_timestamp() -Check timestamp of incoming packet
 * @skb: incoming packet to check
 * @tp: pointer to tcp header of incoming packet
 *
 * If the tcp segment has extended headers then parse them to check to see
 * if timestamps are included. If so, return the value
 *
 * Return:
 *		- timestamp: if a timestamp is valid
 *		- 0: if there is no timestamp extended header
 **/
static u32 rmnet_perf_tcp_opt_check_timestamp(struct sk_buff *skb,
					      struct tcphdr *tp,
					      struct net_device *dev)
{
	int length = tp->doff * 4 - sizeof(*tp);
	unsigned char *ptr = (unsigned char *)(tp + 1);

	while (length > 0) {
		int code = *ptr++;
		int size = *ptr++;

		/* Partial or malformed options */
		if (size < 2 || size > length)
			return 0;

		switch (code) {
		case TCPOPT_EOL:
			/* No more options */
			return 0;
		case TCPOPT_NOP:
			/* Empty option */
			length--;
			continue;
		case TCPOPT_TIMESTAMP:
			if (size == TCPOLEN_TIMESTAMP &&
			    dev_net(dev)->ipv4.sysctl_tcp_timestamps)
				return get_unaligned_be32(ptr);
		}

		ptr += size - 2;
		length -= size;
	}

	/* No timestamp in the options */
	return 0;
}

/* rmnet_perf_tcp_opt_ingress() - Core business logic of tcp_opt
 * @perf: allows access to our required global structures
 * @skb: the incoming ip packet
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
void rmnet_perf_tcp_opt_ingress(struct rmnet_perf *perf, struct sk_buff *skb,
				struct rmnet_perf_opt_flow_node *flow_node,
				struct rmnet_perf_pkt_info *pkt_info,
				bool flush)
{
	bool timestamp_mismatch;
	enum rmnet_perf_tcp_opt_merge_check_rc rc;
	struct napi_struct *napi = NULL;

	if (flush || rmnet_perf_tcp_opt_tcp_flag_flush(pkt_info)) {
		rmnet_perf_opt_update_flow(flow_node, pkt_info);
		rmnet_perf_opt_flush_single_flow_node(perf, flow_node);
		napi = get_current_napi_context();
		napi_gro_flush(napi, false);
		rmnet_perf_core_flush_curr_pkt(perf, skb, pkt_info,
					       pkt_info->header_len +
					       pkt_info->payload_len, true,
					       false);
		napi_gro_flush(napi, false);
		rmnet_perf_tcp_opt_flush_reason_cnt[
			RMNET_PERF_TCP_OPT_TCP_FLUSH_FORCE]++;
		return;
	}

	/* Go ahead and insert the packet now if we're not holding anything.
	 * We know at this point that it's a normal packet in the flow
	 */
	if (!flow_node->num_pkts_held) {
		rmnet_perf_opt_insert_pkt_in_flow(skb, flow_node, pkt_info);
		return;
	}

	pkt_info->curr_timestamp =
		rmnet_perf_tcp_opt_check_timestamp(skb,
						   pkt_info->trns_hdr.tp,
						   perf->core_meta->dev);
	timestamp_mismatch = flow_node->timestamp != pkt_info->curr_timestamp;

	rc = rmnet_perf_tcp_opt_pkt_can_be_merged(skb, flow_node, pkt_info);
	if (rc == RMNET_PERF_TCP_OPT_FLUSH_ALL) {
		rmnet_perf_opt_flush_single_flow_node(perf, flow_node);
		rmnet_perf_core_flush_curr_pkt(perf, skb, pkt_info,
					       pkt_info->header_len +
					       pkt_info->payload_len, false,
					       false);
	} else if (rc == RMNET_PERF_TCP_OPT_FLUSH_SOME) {
		rmnet_perf_opt_flush_single_flow_node(perf, flow_node);
		rmnet_perf_opt_insert_pkt_in_flow(skb, flow_node, pkt_info);
	} else if (timestamp_mismatch) {
		rmnet_perf_opt_flush_single_flow_node(perf, flow_node);
		rmnet_perf_opt_insert_pkt_in_flow(skb, flow_node, pkt_info);
		rmnet_perf_tcp_opt_flush_reason_cnt[
			RMNET_PERF_TCP_OPT_TIMESTAMP_MISMATCH]++;
	} else if (rc == RMNET_PERF_TCP_OPT_MERGE_SUCCESS) {
		pkt_info->first_packet = false;
		rmnet_perf_opt_insert_pkt_in_flow(skb, flow_node, pkt_info);
	}
}
