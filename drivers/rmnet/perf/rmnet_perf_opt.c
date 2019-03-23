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
 * RMNET generic protocol optimization handlers
 *
 */

#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <net/ip.h>
#include <net/checksum.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_config.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_map.h>
#include "rmnet_perf_opt.h"
#include "rmnet_perf_tcp_opt.h"
#include "rmnet_perf_udp_opt.h"
#include "rmnet_perf_core.h"
#include "rmnet_perf_config.h"

/* If true then we allocate all large SKBs */
unsigned long int rmnet_perf_opt_skb_recycle_off = 1;
module_param(rmnet_perf_opt_skb_recycle_off, ulong, 0644);
MODULE_PARM_DESC(rmnet_perf_opt_skb_recycle_off, "Num skbs max held");

/* Stat showing reason for flushes of flow nodes */
unsigned long int
rmnet_perf_opt_flush_reason_cnt[RMNET_PERF_OPT_NUM_CONDITIONS];
module_param_array(rmnet_perf_opt_flush_reason_cnt, ulong, 0, 0444);
MODULE_PARM_DESC(rmnet_perf_opt_flush_reason_cnt,
		 "opt performance statistics");

/* Stat showing packets dropped due to lack of memory */
unsigned long int rmnet_perf_opt_oom_drops = 0;
module_param(rmnet_perf_opt_oom_drops, ulong, 0644);
MODULE_PARM_DESC(rmnet_perf_opt_oom_drops,
		 "Number of packets dropped because we couldn't allocate SKBs");

enum {
	RMNET_PERF_OPT_MODE_TCP,
	RMNET_PERF_OPT_MODE_UDP,
	RMNET_PERF_OPT_MODE_ALL,
	RMNET_PERF_OPT_MODE_NON,
};

/* What protocols we optimize */
static int rmnet_perf_opt_mode = RMNET_PERF_OPT_MODE_ALL;

/* Lock around flow nodes for syncornization with rmnet_perf_opt_mode changes */
static DEFINE_SPINLOCK(rmnet_perf_opt_lock);

/* flow hash table */
DEFINE_HASHTABLE(rmnet_perf_opt_fht, RMNET_PERF_FLOW_HASH_TABLE_BITS);

static void flush_flow_nodes_by_protocol(struct rmnet_perf *perf, u8 protocol)
{
	struct rmnet_perf_opt_flow_node *flow_node;
	int bkt_cursor;

	hash_for_each(rmnet_perf_opt_fht, bkt_cursor, flow_node, list) {
		if (flow_node->num_pkts_held > 0 &&
		    flow_node->protocol == protocol)
			rmnet_perf_opt_flush_single_flow_node(perf, flow_node);
	}
}

static int rmnet_perf_set_opt_mode(const char *val,
				   const struct kernel_param *kp)
{
	struct rmnet_perf *perf;
	int old_mode = rmnet_perf_opt_mode;
	int rc = -EINVAL;
	char value[4];

	strlcpy(value, val, 4);
	value[3] = '\0';
	spin_lock(&rmnet_perf_opt_lock);
	if (!strcmp(value, "tcp"))
		rmnet_perf_opt_mode = RMNET_PERF_OPT_MODE_TCP;
	else if (!strcmp(value, "udp"))
		rmnet_perf_opt_mode = RMNET_PERF_OPT_MODE_UDP;
	else if (!strcmp(value, "all"))
		rmnet_perf_opt_mode = RMNET_PERF_OPT_MODE_ALL;
	else if (!strcmp(value, "non"))
		rmnet_perf_opt_mode = RMNET_PERF_OPT_MODE_NON;
	else
		goto out;

	rc = 0;

	/* If we didn't change anything, or if all we did was add the
	 * other protocol, no nodes need to be flushed out
	 */
	if (old_mode == rmnet_perf_opt_mode ||
	    rmnet_perf_opt_mode == RMNET_PERF_OPT_MODE_ALL)
		goto out;

	/* Flush out any nodes of the protocol we are no longer optimizing */
	perf = rmnet_perf_config_get_perf();
	switch (rmnet_perf_opt_mode) {
	case RMNET_PERF_OPT_MODE_TCP:
		flush_flow_nodes_by_protocol(perf, IPPROTO_UDP);
		break;
	case RMNET_PERF_OPT_MODE_UDP:
		flush_flow_nodes_by_protocol(perf, IPPROTO_TCP);
		break;
	case RMNET_PERF_OPT_MODE_NON:
		flush_flow_nodes_by_protocol(perf, IPPROTO_TCP);
		flush_flow_nodes_by_protocol(perf, IPPROTO_UDP);
		break;
	}

out:
	spin_unlock(&rmnet_perf_opt_lock);
	return rc;
}

static int rmnet_perf_get_opt_mode(char *buf,
				   const struct kernel_param *kp)
{
	switch (rmnet_perf_opt_mode) {
	case RMNET_PERF_OPT_MODE_TCP:
		strlcpy(buf, "tcp\n", 5);
		break;
	case RMNET_PERF_OPT_MODE_UDP:
		strlcpy(buf, "udp\n", 5);
		break;
	case RMNET_PERF_OPT_MODE_ALL:
		strlcpy(buf, "all\n", 5);
		break;
	case RMNET_PERF_OPT_MODE_NON:
		strlcpy(buf, "non\n", 5);
		break;
	}

	return strlen(buf);
}

static const struct kernel_param_ops rmnet_perf_opt_mode_ops = {
	.set = rmnet_perf_set_opt_mode,
	.get = rmnet_perf_get_opt_mode,
};

module_param_cb(rmnet_perf_opt_mode, &rmnet_perf_opt_mode_ops, NULL, 0644);

/* optimize_protocol() - Check if we should optimize the given protocol
 * @protocol: The IP protocol number to check
 *
 * Return:
 *    - true if protocol should use the flow node infrastructure
 *    - false if packets og the given protocol should be flushed
 **/
static bool optimize_protocol(u8 protocol)
{
	if (rmnet_perf_opt_mode == RMNET_PERF_OPT_MODE_ALL)
		return true;
	else if (protocol == IPPROTO_TCP)
		return rmnet_perf_opt_mode == RMNET_PERF_OPT_MODE_TCP;
	else if (protocol == IPPROTO_UDP)
		return rmnet_perf_opt_mode == RMNET_PERF_OPT_MODE_UDP;

	return false;
}

/* ip_flag_flush() - Check IP header flags to decide if
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
static bool ip_flag_flush(struct rmnet_perf_opt_flow_node *flow_node,
			  struct rmnet_perf_pkt_info *pkt_info)
{
	struct iphdr *ip4h;
	struct ipv6hdr *ip6h;
	__be32 first_word;

	switch (pkt_info->ip_proto) {
	case 0x04:
		ip4h = pkt_info->iphdr.v4hdr;

		if ((ip4h->ttl ^ flow_node->ip_flags.ip4_flags.ip_ttl) ||
		    (ip4h->tos ^ flow_node->ip_flags.ip4_flags.ip_tos) ||
		    (ip4h->frag_off ^
		     flow_node->ip_flags.ip4_flags.ip_frag_off) ||
		     ip4h->ihl > 5)
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
		rmnet_perf_opt_flush_reason_cnt[
				RMNET_PERF_OPT_PACKET_CORRUPT_ERROR]++;
	}
	return false;
}

/* identify_flow() - Tell whether packet corresponds to
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
static bool identify_flow(struct rmnet_perf_opt_flow_node *flow_node,
			  struct rmnet_perf_pkt_info *pkt_info)
{
	struct iphdr *ip4h;
	struct ipv6hdr *ip6h;
	/* Actually protocol generic. UDP and TCP headers have the source
	 * and dest ports in the same location. ;)
	 */
	struct udphdr *up = pkt_info->trns_hdr.up;

	/* if pkt count == 0 and hash is the same, then we give this one as
	 * pass as good enough since at this point there is no address stuff
	 * to check/verify.
	 */
	if (flow_node->num_pkts_held == 0 &&
	    flow_node->hash_value == pkt_info->hash_key)
		return true;

	/* protocol must match */
	if (flow_node->protocol != pkt_info->trans_proto)
		return false;

	/* cast iph to right ip header struct for ip_version */
	switch (pkt_info->ip_proto) {
	case 0x04:
		ip4h = pkt_info->iphdr.v4hdr;
		if (((__force u32)flow_node->saddr.saddr4 ^
		     (__force u32)ip4h->saddr) |
		    ((__force u32)flow_node->daddr.daddr4 ^
		     (__force u32)ip4h->daddr) |
		    ((__force u16)flow_node->src_port ^
		     (__force u16)up->source) |
		    ((__force u16)flow_node->dest_port ^
		     (__force u16)up->dest))
			return false;
		break;
	case 0x06:
		ip6h = pkt_info->iphdr.v6hdr;
		if ((ipv6_addr_cmp(&(flow_node->saddr.saddr6), &ip6h->saddr)) |
			(ipv6_addr_cmp(&(flow_node->daddr.daddr6),
				       &ip6h->daddr)) |
			((__force u16)flow_node->src_port ^
			 (__force u16)up->source) |
			((__force u16)flow_node->dest_port ^
			 (__force u16)up->dest))
			return false;
		break;
	default:
		pr_err("Unsupported ip version %d", pkt_info->ip_proto);
		rmnet_perf_opt_flush_reason_cnt[
				RMNET_PERF_OPT_PACKET_CORRUPT_ERROR]++;
		return false;
	}
	return true;
}

/* make_flow_skb() - Allocate and populate SKB for
 *		flow node that is being pushed up the stack
 * @perf: allows access to our required global structures
 * @flow_node: opt structure containing packet we are allocating for
 *
 * Allocate skb of proper size for opt'd packet, and memcpy data
 * into the buffer
 *
 * Return:
 *		- skbn: sk_buff to then push up the NW stack
 *		- NULL: if memory allocation failed
 **/
static struct sk_buff *make_flow_skb(struct rmnet_perf *perf,
				     struct rmnet_perf_opt_flow_node *flow_node)
{
	struct sk_buff *skbn;
	struct rmnet_perf_opt_pkt_node *pkt_list;
	int i;
	u32 pkt_size;
	u32 total_pkt_size = 0;

	if (rmnet_perf_opt_skb_recycle_off) {
		skbn = alloc_skb(flow_node->len + RMNET_MAP_DEAGGR_SPACING,
				 GFP_ATOMIC);
		if (!skbn)
			return NULL;
	} else {
		skbn = rmnet_perf_core_elligible_for_cache_skb(perf,
							       flow_node->len);
		if (!skbn) {
			skbn = alloc_skb(flow_node->len + RMNET_MAP_DEAGGR_SPACING,
				 GFP_ATOMIC);
			if (!skbn)
				return NULL;
		}
	}

	skb_reserve(skbn, RMNET_MAP_DEAGGR_HEADROOM);
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

static void flow_skb_fixup(struct sk_buff *skb,
			   struct rmnet_perf_opt_flow_node *flow_node)
{
	struct skb_shared_info *shinfo;
	struct iphdr *iph = (struct iphdr *)skb->data;
	struct tcphdr *tp;
	struct udphdr *up;
	__wsum pseudo;
	u16 datagram_len, ip_len;
	u16 proto;
	bool ipv4 = (iph->version == 4);
	skb->hash = flow_node->hash_value;
	skb->sw_hash = 1;
	/* We've already validated all data */
	skb->ip_summed = CHECKSUM_UNNECESSARY;

	/* Aggregated flows can be segmented by the stack
	 * during forwarding/tethering scenarios, so pretend
	 * we ran through the GRO logic to coalesce the packets
	 */

	if (flow_node->num_pkts_held <= 1)
		return;

	datagram_len = flow_node->gso_len * flow_node->num_pkts_held;

	/* Update transport header fields to reflect new length.
	 * Checksum is set to the pseudoheader checksum value
	 * since we'll need to mark the SKB as CHECKSUM_PARTIAL.
	 */
	if (ipv4) {
		ip_len = iph->ihl * 4;
		pseudo = csum_partial(&iph->saddr,
				      sizeof(iph->saddr) * 2, 0);
		proto = iph->protocol;
	} else {
		struct ipv6hdr *ip6h = (struct ipv6hdr *)iph;

		ip_len = sizeof(*ip6h);
		pseudo = csum_partial(&ip6h->saddr,
				      sizeof(ip6h->saddr) * 2, 0);
		proto = ip6h->nexthdr;
	}

	pseudo = csum16_add(pseudo, htons(proto));
	switch (proto) {
	case IPPROTO_TCP:
		tp = (struct tcphdr *)((char *)iph + ip_len);
		datagram_len += tp->doff * 4;
		pseudo = csum16_add(pseudo, htons(datagram_len));
		tp->check = ~csum_fold(pseudo);
		skb->csum_start = (unsigned char *) tp - skb->head;
		skb->csum_offset = offsetof(struct tcphdr, check);
		skb_shinfo(skb)->gso_type = (ipv4) ? SKB_GSO_TCPV4:
					     SKB_GSO_TCPV6;
		break;
	case IPPROTO_UDP:
		up = (struct udphdr *)((char *)iph + ip_len);
		datagram_len += sizeof(*up);
		up->len = htons(datagram_len);
		pseudo = csum16_add(pseudo, up->len);
		up->check = ~csum_fold(pseudo);
		skb->csum_start = (unsigned char *)up - skb->head;
		skb->csum_offset = offsetof(struct udphdr, check);
		skb_shinfo(skb)->gso_type = SKB_GSO_UDP_L4;
		break;
	default:
		return;
	}

	/* Update GSO metadata */
	shinfo = skb_shinfo(skb);
	shinfo->gso_size = flow_node->gso_len;
	shinfo->gso_segs = flow_node->num_pkts_held;
	skb->ip_summed = CHECKSUM_PARTIAL;
}

/* get_new_flow_index() - Pull flow node from node pool
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
static struct rmnet_perf_opt_flow_node *
get_new_flow_index(struct rmnet_perf *perf)
{
	struct rmnet_perf_opt_flow_node_pool *node_pool;
	struct rmnet_perf_opt_flow_node *flow_node_ejected;

	node_pool = perf->opt_meta->node_pool;
	/* once this value gets too big it never goes back down.
	 * from that point forward we use flow node repurposing techniques
	 * instead
	 */
	if (node_pool->num_flows_in_use < RMNET_PERF_NUM_FLOW_NODES)
		return node_pool->node_list[node_pool->num_flows_in_use++];

	flow_node_ejected = node_pool->node_list[
		node_pool->flow_recycle_counter++ % RMNET_PERF_NUM_FLOW_NODES];
	rmnet_perf_opt_flush_single_flow_node(perf, flow_node_ejected);
	hash_del(&flow_node_ejected->list);
	return flow_node_ejected;
}

/* rmnet_perf_opt_update_flow() - Update stored IP flow information
 * @flow_node: opt structure containing flow information
 * @pkt_info: characteristics of the current packet
 *
 * Update IP-specific flags stored about the flow (i.e. ttl, tos/traffic class,
 * fragment information, flags).
 *
 * Return:
 *		- void
 **/
void
rmnet_perf_opt_update_flow(struct rmnet_perf_opt_flow_node *flow_node,
			   struct rmnet_perf_pkt_info *pkt_info)
{
	if (pkt_info->ip_proto == 0x04) {
		struct iphdr *iph = pkt_info->iphdr.v4hdr;
		/* Frags don't make it this far, so this is all we care about */
		__be16 flags = iph->frag_off & htons(IP_CE | IP_DF);

		flow_node->ip_flags.ip4_flags.ip_ttl = iph->ttl;
		flow_node->ip_flags.ip4_flags.ip_tos = iph->tos;
		flow_node->ip_flags.ip4_flags.ip_frag_off = flags;
	} else if (pkt_info->ip_proto == 0x06) {
		__be32 *word = (__be32 *)pkt_info->iphdr.v6hdr;

		flow_node->ip_flags.first_word = *word;
	}
}

/* rmnet_perf_opt_flush_single_flow_node() - Send a given flow node up
 *		NW stack.
 * @perf: allows access to our required global structures
 * @flow_node: opt structure containing packet we are allocating for
 *
 * Send a given flow up NW stack via specific VND
 *
 * Return:
 *    - skbn: sk_buff to then push up the NW stack
 **/
void rmnet_perf_opt_flush_single_flow_node(struct rmnet_perf *perf,
				struct rmnet_perf_opt_flow_node *flow_node)
{
	struct sk_buff *skbn;
	struct rmnet_endpoint *ep;

	/* future change: when inserting the first packet in a flow,
	 * save away the ep value so we dont have to look it up every flush
	 */
	hlist_for_each_entry_rcu(ep,
				 &perf->rmnet_port->muxed_ep[flow_node->mux_id],
				 hlnode) {
		if (ep->mux_id == flow_node->mux_id &&
		    flow_node->num_pkts_held) {
			skbn = make_flow_skb(perf, flow_node);
			if (skbn) {
				flow_skb_fixup(skbn, flow_node);
				rmnet_perf_core_send_skb(skbn, ep, perf, NULL);
			} else {
				rmnet_perf_opt_oom_drops +=
					flow_node->num_pkts_held;
			}
			/* equivalent to memsetting the flow node */
			flow_node->num_pkts_held = 0;
		}
	}
}

/* rmnet_perf_opt_flush_all_flow_nodes() - Iterate through all flow nodes
 *		and flush them individually
 * @perf: allows access to our required global structures
 *
 * Return:
 *    - void
 **/
void rmnet_perf_opt_flush_all_flow_nodes(struct rmnet_perf *perf)
{
	struct rmnet_perf_opt_flow_node *flow_node;
	int bkt_cursor;
	int num_pkts_held;
	u32 hash_val;

	hash_for_each(rmnet_perf_opt_fht, bkt_cursor, flow_node, list) {
		hash_val = flow_node->hash_value;
		num_pkts_held = flow_node->num_pkts_held;
		if (num_pkts_held > 0) {
			rmnet_perf_opt_flush_single_flow_node(perf, flow_node);
			//rmnet_perf_core_flush_single_gro_flow(hash_val);
		}
	}
}

/* rmnet_perf_opt_insert_pkt_in_flow() - Inserts single IP packet into
 *		opt meta structure
 * @skb: pointer to packet given to us by physical device
 * @flow_node: flow node we are going to insert the ip packet into
 * @pkt_info: characteristics of the current packet
 *
 * Return:
 *    - void
 **/
void rmnet_perf_opt_insert_pkt_in_flow(struct sk_buff *skb,
			struct rmnet_perf_opt_flow_node *flow_node,
			struct rmnet_perf_pkt_info *pkt_info)
{
	struct rmnet_perf_opt_pkt_node *pkt_node;
	struct tcphdr *tp = pkt_info->trns_hdr.tp;
	void *iph = (void *) pkt_info->iphdr.v4hdr;
	u16 header_len = pkt_info->header_len;
	u16 payload_len = pkt_info->payload_len;
	unsigned char ip_version = pkt_info->ip_proto;

	pkt_node = &flow_node->pkt_list[flow_node->num_pkts_held];
	pkt_node->data_end = (unsigned char *) iph + header_len + payload_len;
	if (pkt_info->trans_proto == IPPROTO_TCP)
		flow_node->next_seq = ntohl(tp->seq) +
				      (__force u32) payload_len;

	if (pkt_info->first_packet) {
		pkt_node->ip_start = (unsigned char *) iph;
		pkt_node->data_start = (unsigned char *) iph;
		flow_node->len = header_len + payload_len;
		flow_node->mux_id = RMNET_MAP_GET_MUX_ID(skb);
		flow_node->src_port = tp->source;
		flow_node->dest_port = tp->dest;
		flow_node->hash_value = pkt_info->hash_key;
		flow_node->gso_len = payload_len;

		if (pkt_info->trans_proto == IPPROTO_TCP)
			flow_node->timestamp = pkt_info->curr_timestamp;

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

/* rmnet_perf_opt_ingress() - Core business logic of optimization framework
 * @perf: allows access to our required global structures
 * @skb: the incoming ip packet
 * @pkt_info: characteristics of the current packet
 *
 * Makes determination of what to do with a given incoming
 * ip packet. Find matching flow if it exists and call protocol-
 * specific helper to try and insert the packet and handle any
 * flushing needed.
 *
 * Return:
 *		- true if packet has been handled
 *		- false if caller needs to flush packet
 **/
bool rmnet_perf_opt_ingress(struct rmnet_perf *perf, struct sk_buff *skb,
			    struct rmnet_perf_pkt_info *pkt_info)
{
	struct rmnet_perf_opt_flow_node *flow_node;
	struct rmnet_perf_opt_flow_node *flow_node_recycled;
	bool flush;
	bool handled = false;
	bool flow_node_exists = false;

	spin_lock(&rmnet_perf_opt_lock);
	if (!optimize_protocol(pkt_info->trans_proto))
		goto out;

handle_pkt:
	hash_for_each_possible(rmnet_perf_opt_fht, flow_node, list,
			       pkt_info->hash_key) {
		if (!identify_flow(flow_node, pkt_info))
			continue;

		flush = ip_flag_flush(flow_node, pkt_info);

		/* set this to true by default. Let the protocol helpers
		 * change this if it is needed.
		 */
		pkt_info->first_packet = true;
		flow_node_exists = true;

		switch (pkt_info->trans_proto) {
		case IPPROTO_TCP:
			rmnet_perf_tcp_opt_ingress(perf, skb, flow_node,
						   pkt_info, flush);
			handled = true;
			goto out;
		case IPPROTO_UDP:
			rmnet_perf_udp_opt_ingress(perf, skb, flow_node,
						   pkt_info, flush);
			handled = true;
			goto out;
		default:
			pr_err("%s(): Unhandled protocol %u\n",
			       __func__, pkt_info->trans_proto);
			goto out;
		}
	}

	/* If we didn't find the flow, we need to add it and try again */
	if (!flow_node_exists) {
		flow_node_recycled = get_new_flow_index(perf);
		flow_node_recycled->hash_value = pkt_info->hash_key;
		rmnet_perf_opt_update_flow(flow_node_recycled, pkt_info);
		hash_add(rmnet_perf_opt_fht, &flow_node_recycled->list,
			 pkt_info->hash_key);
		goto handle_pkt;
	}

out:
	spin_unlock(&rmnet_perf_opt_lock);
	return handled;
}
