/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#include <linux/jhash.h>
#include <net/ip6_checksum.h>
#include <net/tcp.h>
#include <linux/module.h>
#include <asm/unaligned.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_config.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_map.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_private.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_handlers.h>
#include "rmnet_perf_tcp_opt.h"
#include "rmnet_perf_core.h"
#include "rmnet_perf_config.h"

/* Max number of bytes we allow tcp_opt to aggregate per flow */
unsigned int rmnet_perf_tcp_opt_flush_limit __read_mostly = 65536;
module_param(rmnet_perf_tcp_opt_flush_limit, uint, 0644);
MODULE_PARM_DESC(rmnet_perf_tcp_opt_flush_limit,
		 "Max flush limiit for tcp_opt");

/* Stat showing reason for flushes of flow nodes */
unsigned long int rmnet_perf_tcp_opt_flush_reason_cnt[
					RMNET_PERF_TCP_OPT_NUM_CONDITIONS];
module_param_array(rmnet_perf_tcp_opt_flush_reason_cnt, ulong, 0, 0444);
MODULE_PARM_DESC(rmnet_perf_tcp_opt_flush_reason_cnt,
		 "tcp_opt performance statistics");

/* Number of ip packets leaving tcp_opt. Should be less than "pre" */
unsigned long int rmnet_perf_tcp_opt_post_ip_count;
module_param(rmnet_perf_tcp_opt_post_ip_count, ulong, 0644);
MODULE_PARM_DESC(rmnet_perf_tcp_opt_post_ip_count,
		 "Number of packets of MTU size, post-tcp_opt");

/* If true then we allocate all large SKBs */
unsigned long int rmnet_perf_tcp_opt_skb_recycle_off = 1;
module_param(rmnet_perf_tcp_opt_skb_recycle_off, ulong, 0644);
MODULE_PARM_DESC(rmnet_perf_tcp_opt_skb_recycle_off, "Num skbs max held");

unsigned long int rmnet_perf_tcp_opt_fn_seq = 0;
module_param(rmnet_perf_tcp_opt_fn_seq, ulong, 0644);
MODULE_PARM_DESC(rmnet_perf_tcp_opt_fn_seq, "flow node seq");

unsigned long int rmnet_perf_tcp_opt_pkt_seq = 0;
module_param(rmnet_perf_tcp_opt_pkt_seq, ulong, 0644);
MODULE_PARM_DESC(rmnet_perf_tcp_opt_pkt_seq, "incoming pkt seq");

/* flow hash table */
DEFINE_HASHTABLE(rmnet_perf_tcp_opt_fht, RMNET_PERF_FLOW_HASH_TABLE_BITS);

/* rmnet_perf_tcp_opt_ip_flag_flush() - Check IP header flags to decide if
 *		immediate flush required
 * @pkt_info: characteristics of the current packet
 *
 * If the IP header has any flag set that GRO won't accept, we will flush the
 * packet right away.
 *
 * Return:
 *    - true if need flush
 *    - false if immediate flush may not be needed
 **/
static bool
rmnet_perf_tcp_opt_ip_flag_flush(struct rmnet_perf_tcp_opt_flow_node *flow_node,
				 struct rmnet_perf_pkt_info *pkt_info)
{
	struct iphdr *ip4h;
	struct ipv6hdr *ip6h;
	__be32 first_word;
	//struct rmnet_perf_tcp_opt_ip_flags *flags;

	switch (pkt_info->ip_proto) {
	case 0x04:
		ip4h = pkt_info->iphdr.v4hdr;
		if ((ip4h->ttl ^ flow_node->ip_flags.ip4_flags.ip_ttl) ||
			(ip4h->tos ^ flow_node->ip_flags.ip4_flags.ip_tos) ||
			(ip4h->frag_off ^
			 flow_node->ip_flags.ip4_flags.ip_frag_off))
			return true;
		break;
	case 0x06:
		ip6h = (struct ipv6hdr *) pkt_info->iphdr.v6hdr;
		first_word = *(__be32 *)ip6h ^ flow_node->ip_flags.first_word;
		if (!!(first_word & htonl(0x0FF00000)))
			return true;
		break;
	default:
		pr_err("Unsupported ip version %d", pkt_info->ip_proto);
		rmnet_perf_tcp_opt_flush_reason_cnt[
				RMNET_PERF_TCP_OPT_PACKET_CORRUPT_ERROR]++;
	}
	return false;
}

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
				struct rmnet_perf_tcp_opt_flow_node *flow_node,
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
	}
	return RMNET_PERF_TCP_OPT_MERGE_SUCCESS;
}

/* rmnet_perf_tcp_opt_make_new_skb_for_flow() - Allocate and populate SKB for
 *		flow node that is being pushed up the stack
 * @perf: allows access to our required global structures
 * @flow_node: tcp_opt structure containing packet we are allocating for
 *
 * Allocate skb of proper size for tcp_opt'd packet, and memcpy data
 * into the buffer
 *
 * Return:
 *		- skbn: sk_buff to then push up the NW stack
 *		- NULL: if memory allocation failed
 **/
static struct sk_buff *
rmnet_perf_tcp_opt_make_new_skb_for_flow(struct rmnet_perf *perf,
				struct rmnet_perf_tcp_opt_flow_node *flow_node)
{
	struct sk_buff *skbn;
	struct rmnet_perf_tcp_opt_pkt_node *pkt_list;
	int i;
	u32 pkt_size;
	u32 total_pkt_size = 0;

	if (rmnet_perf_tcp_opt_skb_recycle_off) {
		skbn = alloc_skb(flow_node->len + RMNET_MAP_DEAGGR_SPACING,
				 GFP_ATOMIC);
		if (!skbn)
			return NULL;
	} else {
		skbn = rmnet_perf_core_elligible_for_cache_skb(perf, flow_node->len);
		if (!skbn) {
			skbn = alloc_skb(flow_node->len + RMNET_MAP_DEAGGR_SPACING,
				 GFP_ATOMIC);
			if (!skbn)
				return NULL;
		}
	}
	pkt_list = flow_node->pkt_list;

	for (i = 0; i < flow_node->num_pkts_held; i++) {
		pkt_size = pkt_list[i].data_end - pkt_list[i].data_start;
		memcpy(skbn->data + skbn->len, pkt_list[i].data_start,
		       pkt_size);
		skb_put(skbn, pkt_size);
		total_pkt_size += pkt_size;
	}
	if (flow_node->len != total_pkt_size)
		pr_err("%s(): skbn = %pK, flow_node->len = %u, pkt_size = %u\n",
		       __func__, skbn, flow_node->len, total_pkt_size);

	return skbn;
}

/* rmnet_perf_tcp_opt_update_flow() - Update stored IP flow information
 * @flow_node: tcp_opt structure containing flow information
 * @pkt_info: characteristics of the current packet
 *
 * Update IP-specific flags stored about the flow (i.e. ttl, tos/traffic class,
 * fragment information, flags).
 *
 * Return:
 *		- void
 **/
static void
rmnet_perf_tcp_opt_update_flow(struct rmnet_perf_tcp_opt_flow_node *flow_node,
			       struct rmnet_perf_pkt_info *pkt_info)
{
	if (pkt_info->ip_proto == 0x04) {
		struct iphdr *iph = pkt_info->iphdr.v4hdr;

		flow_node->ip_flags.ip4_flags.ip_ttl = iph->ttl;
		flow_node->ip_flags.ip4_flags.ip_tos = iph->tos;
		flow_node->ip_flags.ip4_flags.ip_frag_off = iph->frag_off;
	} else if (pkt_info->ip_proto == 0x06) {
		__be32 *word = (__be32 *)pkt_info->iphdr.v6hdr;

		flow_node->ip_flags.first_word = *word;
	}
}

/* rmnet_perf_tcp_opt_flush_single_flow_node() - Send a given flow node up
 *		NW stack.
 * @perf: allows access to our required global structures
 * @flow_node: tcp_opt structure containing packet we are allocating for
 *
 * Send a given flow up NW stack via specific VND
 *
 * Return:
 *    - skbn: sk_buff to then push up the NW stack
 **/
static void rmnet_perf_tcp_opt_flush_single_flow_node(struct rmnet_perf *perf,
				struct rmnet_perf_tcp_opt_flow_node *flow_node)
{
	struct sk_buff *skbn;
	struct rmnet_endpoint *ep;

	/* future change: when inserting the first packet in a flow,
	 * save away the ep value so we dont have to look it up every flush
	 */
	hlist_for_each_entry_rcu(ep,
				 &perf->rmnet_port->muxed_ep[flow_node->mux_id],
				 hlnode) {
		if (ep->mux_id == flow_node->mux_id) {
			if (flow_node->num_pkts_held) {
				skbn =
				rmnet_perf_tcp_opt_make_new_skb_for_flow(perf,
								flow_node);
				if (!skbn) {
					pr_err("%s(): skbn is NULL\n",
					       __func__);
				} else {
					skbn->hash = flow_node->hash_value;
					skbn->sw_hash = 1;
					/* data is already validated */
					skbn->ip_summed = CHECKSUM_UNNECESSARY;
					rmnet_perf_core_send_skb(skbn, ep,
								 perf, NULL);
				}
				/* equivalent to memsetting the flow node */
				flow_node->num_pkts_held = 0;
			}
		}
	}
}

/* rmnet_perf_tcp_opt_flush_all_flow_nodes() - Iterate through all flow nodes
 *		and flush them individually
 * @perf: allows access to our required global structures
 *
 * Return:
 *    - void
 **/
void rmnet_perf_tcp_opt_flush_all_flow_nodes(struct rmnet_perf *perf)
{
	struct rmnet_perf_tcp_opt_flow_node *flow_node;
	int bkt_cursor;
	int num_pkts_held;
	u32 hash_val;

	hash_for_each(rmnet_perf_tcp_opt_fht, bkt_cursor, flow_node, list) {
		hash_val = flow_node->hash_value;
		num_pkts_held = flow_node->num_pkts_held;
		//if (num_pkts_held > 0 && num_pkts_held < 3)
		if (num_pkts_held > 0) {
			rmnet_perf_tcp_opt_flush_single_flow_node(perf,
								  flow_node);
			//rmnet_perf_core_flush_single_gro_flow(hash_val);
		}
	}
}

/* rmnet_perf_tcp_opt_insert_pkt_in_flow() - Inserts single IP packet into
 *		tcp_opt meta structure
 * @skb: pointer to packet given to us by physical device
 * @flow_node: flow node we are going to insert the ip packet into
 * @pkt_info: characteristics of the current packet
 *
 * Return:
 *    - void
 **/
static void rmnet_perf_tcp_opt_insert_pkt_in_flow(struct sk_buff *skb,
			struct rmnet_perf_tcp_opt_flow_node *flow_node,
			struct rmnet_perf_pkt_info *pkt_info)
{
	struct rmnet_perf_tcp_opt_pkt_node *pkt_node;
	struct tcphdr *tp = pkt_info->trns_hdr.tp;
	void *iph = (void *) pkt_info->iphdr.v4hdr;
	u16 header_len = pkt_info->header_len;
	u16 payload_len = pkt_info->payload_len;
	unsigned char ip_version = pkt_info->ip_proto;

	pkt_node = &flow_node->pkt_list[flow_node->num_pkts_held];
	pkt_node->data_end = (unsigned char *) iph + header_len + payload_len;
	flow_node->next_seq = ntohl(tp->seq) + (__force u32) payload_len;

	if (pkt_info->first_packet) {
		pkt_node->ip_start = (unsigned char *) iph;
		pkt_node->data_start = (unsigned char *) iph;
		flow_node->len = header_len + payload_len;
		flow_node->mux_id = RMNET_MAP_GET_MUX_ID(skb);
		flow_node->src_port = tp->source;
		flow_node->dest_port = tp->dest;
		flow_node->timestamp = pkt_info->curr_timestamp;
		flow_node->hash_value = pkt_info->hash_key;
		if (ip_version == 0x04) {
			flow_node->saddr.saddr4 =
				(__be32) ((struct iphdr *) iph)->saddr;
			flow_node->daddr.daddr4 =
				(__be32) ((struct iphdr *) iph)->daddr;
			flow_node->protocol = ((struct iphdr *) iph)->protocol;
		} else if (ip_version == 0x06) {
			flow_node->saddr.saddr6 =
				((struct ipv6hdr *) iph)->saddr;
			flow_node->daddr.daddr6 =
				((struct ipv6hdr *) iph)->daddr;
			flow_node->protocol = ((struct ipv6hdr *) iph)->nexthdr;
		} else {
			pr_err("%s(): Encountered invalid ip version\n",
			       __func__);
			/* TODO as Vamsi mentioned get a way to handle
			 * this case... still want to send packet up NW stack
			 */
		}
		flow_node->num_pkts_held = 1;
	} else {
		pkt_node->ip_start = (unsigned char *) iph;
		pkt_node->data_start = (unsigned char *) iph + header_len;
		flow_node->len += payload_len;
		flow_node->num_pkts_held++;
	}
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

/* rmnet_perf_tcp_opt_identify_flow() - Tell whether packet corresponds to
 *		given flow
 * @flow_node: Node we are checking against
 * @pkt_info: characteristics of the current packet
 *
 * Checks to see if the incoming packet is a match for a given flow node
 *
 * Return:
 *		- true: it is a match
 *		- false: not a match
 **/
static bool
rmnet_perf_tcp_opt_identify_flow(struct rmnet_perf_tcp_opt_flow_node *flow_node,
				 struct rmnet_perf_pkt_info *pkt_info)
{
	struct iphdr *ip4h;
	struct ipv6hdr *ip6h;
	struct tcphdr *tp = pkt_info->trns_hdr.tp;

	//if pkt count == 0 and hash is the same, then we give this one as
	//pass as good enough
	//since at this point there is no address stuff to check/verify.
	if (flow_node->num_pkts_held == 0 &&
	    flow_node->hash_value == pkt_info->hash_key)
		return true;

	/* cast iph to right ip header struct for ip_version */
	switch (pkt_info->ip_proto) {
	case 0x04:
		ip4h = pkt_info->iphdr.v4hdr;
		if (((__force u32)flow_node->saddr.saddr4 ^
		     (__force u32)ip4h->saddr) |
		    ((__force u32)flow_node->daddr.daddr4 ^
		     (__force u32)ip4h->daddr) |
		    ((__force u16)flow_node->src_port ^
		     (__force u16)tp->source) |
		    ((__force u16)flow_node->dest_port ^
		     (__force u16)tp->dest))
			return false;
		break;
	case 0x06:
		ip6h = pkt_info->iphdr.v6hdr;
		if ((ipv6_addr_cmp(&(flow_node->saddr.saddr6), &ip6h->saddr)) |
			(ipv6_addr_cmp(&(flow_node->daddr.daddr6),
				       &ip6h->daddr)) |
			((__force u16)flow_node->src_port ^
			 (__force u16)tp->source) |
			((__force u16)flow_node->dest_port ^
			 (__force u16)tp->dest))
			return false;
		break;
	default:
		pr_err("Unsupported ip version %d", pkt_info->ip_proto);
		rmnet_perf_tcp_opt_flush_reason_cnt[
				RMNET_PERF_TCP_OPT_PACKET_CORRUPT_ERROR]++;
		return false;
	}
	return true;
}

/* rmnet_perf_tcp_opt_get_new_flow_node_index() - Pull flow node from node pool
 * @perf: allows access to our required global structures
 *
 * Fetch the flow node from the node pool. If we have already given
 * out all the flow nodes then we will always hit the else case and
 * thereafter we will use modulo arithmetic to choose which flow node
 * to evict and use.
 *
 * Return:
 *		- flow_node: node to be used by caller function
 **/
static struct rmnet_perf_tcp_opt_flow_node *
rmnet_perf_tcp_opt_get_new_flow_node_index(struct rmnet_perf *perf)
{
	struct rmnet_perf_tcp_opt_flow_node_pool *node_pool;
	struct rmnet_perf_tcp_opt_flow_node *flow_node_ejected;

	node_pool = perf->tcp_opt_meta->node_pool;
	/* once this value gets too big it never goes back down.
	 * from that point forward we use flow node repurposing techniques
	 * instead
	 */
	if (node_pool->num_flows_in_use < RMNET_PERF_NUM_FLOW_NODES)
		return node_pool->node_list[node_pool->num_flows_in_use++];

	flow_node_ejected = node_pool->node_list[
		node_pool->flow_recycle_counter++ % RMNET_PERF_NUM_FLOW_NODES];
	rmnet_perf_tcp_opt_flush_single_flow_node(perf,
						  flow_node_ejected);
	hash_del(&flow_node_ejected->list);
	return flow_node_ejected;
}

/* rmnet_perf_tcp_opt_ingress() - Core business logic of tcp_opt
 * @perf: allows access to our required global structures
 * @skb: the incoming ip packet
 * @pkt_info: characteristics of the current packet
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
				struct rmnet_perf_pkt_info *pkt_info)
{
	struct rmnet_perf_tcp_opt_flow_node *flow_node;
	struct rmnet_perf_tcp_opt_flow_node *flow_node_recycled;
	bool flush_pkt_now;
	bool timestamp_mismatch;
	bool match;
	enum rmnet_perf_tcp_opt_merge_check_rc rc = 0;
	bool flow_node_exists = 0;
	struct napi_struct *napi = NULL;
	//pkt_info->hash_key = rmnet_perf_core_compute_flow_hash(pkt_info);

handle_pkt:
	hash_for_each_possible(rmnet_perf_tcp_opt_fht, flow_node, list,
			       pkt_info->hash_key) {
		match = rmnet_perf_tcp_opt_identify_flow(flow_node, pkt_info);
		if (!match)
			continue;

		flush_pkt_now = rmnet_perf_tcp_opt_tcp_flag_flush(pkt_info) |
			rmnet_perf_tcp_opt_ip_flag_flush(flow_node, pkt_info);
		pkt_info->curr_timestamp =
			rmnet_perf_tcp_opt_check_timestamp(skb,
						pkt_info->trns_hdr.tp,
						perf->core_meta->dev);
		/* set this to true by default... only change it if we
		 * identify that we have merged successfully
		 */
		pkt_info->first_packet = true;
		timestamp_mismatch = (flow_node->timestamp !=
			pkt_info->curr_timestamp) ? 1 : 0;
		flow_node_exists = 1;

		if (flow_node->num_pkts_held > 0) {
			rc = rmnet_perf_tcp_opt_pkt_can_be_merged(skb,
							flow_node, pkt_info);
			if (flush_pkt_now) {
				rmnet_perf_tcp_opt_update_flow(flow_node,
							       pkt_info);
				rmnet_perf_tcp_opt_flush_single_flow_node(perf,
								flow_node);
				napi = get_current_napi_context();
				napi_gro_flush(napi, false);
				rmnet_perf_core_flush_curr_pkt(perf, skb,
							       pkt_info,
				pkt_info->header_len + pkt_info->payload_len);
				napi_gro_flush(napi, false);
				rmnet_perf_tcp_opt_flush_reason_cnt[
					RMNET_PERF_TCP_OPT_TCP_FLUSH_FORCE]++;
			} else if (rc == RMNET_PERF_TCP_OPT_FLUSH_ALL) {
				rmnet_perf_tcp_opt_flush_single_flow_node(perf,
								flow_node);
				rmnet_perf_core_flush_curr_pkt(perf, skb,
							       pkt_info,
				pkt_info->header_len + pkt_info->payload_len);
			} else if (rc == RMNET_PERF_TCP_OPT_FLUSH_SOME) {
				rmnet_perf_tcp_opt_flush_single_flow_node(perf,
								flow_node);
				rmnet_perf_tcp_opt_insert_pkt_in_flow(skb,
							flow_node, pkt_info);
			} else if (rc == RMNET_PERF_TCP_OPT_FLUSH_SOME_GRO) {
				rmnet_perf_tcp_opt_flush_single_flow_node(perf,
								flow_node);
				//rmnet_perf_core_flush_single_gro_flow(
				//			pkt_info->hash_key);
				rmnet_perf_tcp_opt_insert_pkt_in_flow(skb,
							flow_node, pkt_info);
			} else if (timestamp_mismatch) {
				rmnet_perf_tcp_opt_flush_single_flow_node(perf,
								flow_node);
				rmnet_perf_tcp_opt_insert_pkt_in_flow(skb,
							flow_node, pkt_info);
				rmnet_perf_tcp_opt_flush_reason_cnt[
				    RMNET_PERF_TCP_OPT_TIMESTAMP_MISMATCH]++;
			} else if (rc == RMNET_PERF_TCP_OPT_MERGE_SUCCESS) {
				pkt_info->first_packet = false;
				rmnet_perf_tcp_opt_insert_pkt_in_flow(skb,
							flow_node, pkt_info);
			}
		} else if (flush_pkt_now) {
			rmnet_perf_tcp_opt_update_flow(flow_node, pkt_info);
			rmnet_perf_core_flush_curr_pkt(perf, skb, pkt_info,
				pkt_info->header_len + pkt_info->payload_len);
			napi = get_current_napi_context();
			napi_gro_flush(napi, false);
			rmnet_perf_tcp_opt_flush_reason_cnt[
				RMNET_PERF_TCP_OPT_TCP_FLUSH_FORCE]++;
		} else
			rmnet_perf_tcp_opt_insert_pkt_in_flow(skb, flow_node,
							      pkt_info);
		break;
	}
	if (!flow_node_exists) {
		flow_node_recycled =
			rmnet_perf_tcp_opt_get_new_flow_node_index(perf);
		flow_node_recycled->hash_value = pkt_info->hash_key;
		rmnet_perf_tcp_opt_update_flow(flow_node_recycled, pkt_info);
		hash_add(rmnet_perf_tcp_opt_fht, &flow_node_recycled->list,
			 pkt_info->hash_key);
		goto handle_pkt;
	}
}
