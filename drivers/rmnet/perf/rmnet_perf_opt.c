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
#include <net/ip6_checksum.h>
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

/* flow hash table */
DEFINE_HASHTABLE(rmnet_perf_opt_fht, RMNET_PERF_FLOW_HASH_TABLE_BITS);

static void rmnet_perf_opt_flush_flow_nodes_by_protocol(u8 protocol)
{
	struct rmnet_perf_opt_flow_node *flow_node;
	int bkt_cursor;

	hash_for_each(rmnet_perf_opt_fht, bkt_cursor, flow_node, list) {
		if (flow_node->num_pkts_held > 0 &&
		    flow_node->trans_proto == protocol)
			rmnet_perf_opt_flush_single_flow_node(flow_node);
	}
}

static int rmnet_perf_set_opt_mode(const char *val,
				   const struct kernel_param *kp)
{
	int old_mode = rmnet_perf_opt_mode;
	int rc = -EINVAL;
	char value[4];

	strlcpy(value, val, 4);
	value[3] = '\0';

	rmnet_perf_core_grab_lock();

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
	switch (rmnet_perf_opt_mode) {
	case RMNET_PERF_OPT_MODE_TCP:
		rmnet_perf_opt_flush_flow_nodes_by_protocol(IPPROTO_UDP);
		break;
	case RMNET_PERF_OPT_MODE_UDP:
		rmnet_perf_opt_flush_flow_nodes_by_protocol(IPPROTO_TCP);
		break;
	case RMNET_PERF_OPT_MODE_NON:
		rmnet_perf_opt_flush_all_flow_nodes();
		break;
	}

out:
	rmnet_perf_core_release_lock();

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

/* rmnet_perf_optimize_protocol() - Check if we should optimize the given
 * protocol
 * @protocol: The IP protocol number to check
 *
 * Return:
 *    - true if protocol should use the flow node infrastructure
 *    - false if packets og the given protocol should be flushed
 **/
static bool rmnet_perf_optimize_protocol(u8 protocol)
{
	if (rmnet_perf_opt_mode == RMNET_PERF_OPT_MODE_ALL)
		return true;
	else if (protocol == IPPROTO_TCP)
		return rmnet_perf_opt_mode == RMNET_PERF_OPT_MODE_TCP;
	else if (protocol == IPPROTO_UDP)
		return rmnet_perf_opt_mode == RMNET_PERF_OPT_MODE_UDP;

	return false;
}

/* rmnet_perf_opt_ip_flag_flush() - Check IP header flags to decide if
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
rmnet_perf_opt_ip_flag_flush(struct rmnet_perf_opt_flow_node *flow_node,
			     struct rmnet_perf_pkt_info *pkt_info)
{
	struct iphdr *ip4h;
	struct ipv6hdr *ip6h;
	__be32 first_word;

	switch (pkt_info->ip_proto) {
	case 0x04:
		ip4h = pkt_info->ip_hdr.v4hdr;

		if ((ip4h->ttl ^ flow_node->ip_flags.ip4_flags.ip_ttl) ||
		    (ip4h->tos ^ flow_node->ip_flags.ip4_flags.ip_tos) ||
		    (ip4h->frag_off ^
		     flow_node->ip_flags.ip4_flags.ip_frag_off) ||
		     ip4h->ihl > 5)
			return true;

		break;
	case 0x06:
		ip6h = (struct ipv6hdr *) pkt_info->ip_hdr.v6hdr;
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

/* rmnet_perf_opt_identify_flow() - Tell whether packet corresponds to
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
rmnet_perf_opt_identify_flow(struct rmnet_perf_opt_flow_node *flow_node,
			     struct rmnet_perf_pkt_info *pkt_info)
{
	struct iphdr *ip4h;
	struct ipv6hdr *ip6h;
	/* Actually protocol generic. UDP and TCP headers have the source
	 * and dest ports in the same location. ;)
	 */
	struct udphdr *up = pkt_info->trans_hdr.up;

	/* if pkt count == 0 and hash is the same, then we give this one as
	 * pass as good enough since at this point there is no address stuff
	 * to check/verify.
	 */
	if (flow_node->num_pkts_held == 0 &&
	    flow_node->hash_value == pkt_info->hash_key)
		return true;

	/* protocol must match */
	if (flow_node->trans_proto != pkt_info->trans_proto)
		return false;

	/* cast iph to right ip header struct for ip_version */
	switch (pkt_info->ip_proto) {
	case 0x04:
		ip4h = pkt_info->ip_hdr.v4hdr;
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
		ip6h = pkt_info->ip_hdr.v6hdr;
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

/* rmnet_perf_opt_add_flow_subfrags() - Associates the frag descriptor held by
 *		the flow_node to the main descriptor
 * @flow_node: opt structure containing packet we are allocating for
 *
 * Return:
 *		- void
 **/

static void
rmnet_perf_opt_add_flow_subfrags(struct rmnet_perf_opt_flow_node *flow_node)
{
	struct rmnet_perf *perf = rmnet_perf_config_get_perf();
	struct rmnet_perf_opt_pkt_node *pkt_list;
	struct rmnet_frag_descriptor *head_frag;
	u8 i;

	pkt_list = flow_node->pkt_list;
	head_frag = pkt_list[0].frag_desc;

	/* GSO segs might not be initialized yet (i.e. csum offload,
	 * RSB/RSC frames with only 1 packet, etc)
	 */
	if (!head_frag->gso_segs)
		head_frag->gso_segs = 1;

	head_frag->gso_size = flow_node->gso_len;

	for (i = 1; i < flow_node->num_pkts_held; i++) {
		struct rmnet_frag_descriptor *new_frag;

		new_frag = pkt_list[i].frag_desc;
		/* Pull headers if they're there */
		if (new_frag->hdr_ptr == rmnet_frag_data_ptr(new_frag))
			rmnet_frag_pull(new_frag, perf->rmnet_port,
					flow_node->ip_len +
					flow_node->trans_len);

		/* Move the fragment onto the subfrags list */
		list_move_tail(&new_frag->list, &head_frag->sub_frags);
		head_frag->gso_segs += (new_frag->gso_segs) ?: 1;
	}
}

/* rmnet_perf_opt_alloc_flow_skb() - Allocate a new SKB for holding flow node
 *		data
 * @headlen: The amount of space to allocate for linear data. Does not include
 *		extra deaggregation headeroom.
 *
 * Allocates a new SKB large enough to hold the amount of data provided, or
 * returns a preallocated SKB if recycling is enabled and there are cached
 * buffers available.
 *
 * Return:
 *		- skb: the new SKb to use
 *		- NULL: memory failure
 **/
static struct sk_buff *rmnet_perf_opt_alloc_flow_skb(u32 headlen)
{
	struct sk_buff *skb;

	/* Grab a preallocated SKB if possible */
	if (!rmnet_perf_opt_skb_recycle_off) {
		skb = rmnet_perf_core_elligible_for_cache_skb(headlen);
		if (skb)
			return skb;
	}

	skb = alloc_skb(headlen + RMNET_MAP_DEAGGR_SPACING, GFP_ATOMIC);
	if (!skb)
		return NULL;

	skb_reserve(skb, RMNET_MAP_DEAGGR_HEADROOM);
	return skb;
}

/* rmnet_perf_opt_make_flow_skb() - Allocate and populate SKBs for flow node
 *		that is being pushed up the stack
 * @flow_node: opt structure containing packet we are allocating for
 *
 * Return:
 *		- skb: The new SKB to use
 *		- NULL: memory failure
 **/
static struct sk_buff *
rmnet_perf_opt_make_flow_skb(struct rmnet_perf_opt_flow_node *flow_node)
{
	struct sk_buff *skb;
	struct rmnet_perf_opt_pkt_node *pkt_list;
	int i;
	u32 alloc_len;
	u32 total_pkt_size = 0;

	pkt_list = flow_node->pkt_list;
	alloc_len = flow_node->len + flow_node->ip_len + flow_node->trans_len;
	skb = rmnet_perf_opt_alloc_flow_skb(alloc_len);
	if (!skb)
		return NULL;

	/* Copy the headers over */
	skb_put_data(skb, pkt_list[0].header_start,
		     flow_node->ip_len + flow_node->trans_len);

	for (i = 0; i < flow_node->num_pkts_held; i++) {
		skb_put_data(skb, pkt_list[i].data_start, pkt_list[i].data_len);
		total_pkt_size += pkt_list[i].data_len;
	}

	if (flow_node->len != total_pkt_size)
		pr_err("%s(): flow_node->len = %u, pkt_size = %u\n", __func__,
		       flow_node->len, total_pkt_size);

	return skb;
}

static void
rmnet_perf_opt_flow_skb_fixup(struct sk_buff *skb,
			      struct rmnet_perf_opt_flow_node *flow_node)
{
	struct skb_shared_info *shinfo = skb_shinfo(skb);
	struct iphdr *iph = (struct iphdr *)skb->data;
	struct tcphdr *tp;
	struct udphdr *up;
	__sum16 pseudo;
	u16 datagram_len;
	bool ipv4 = (iph->version == 4);

	/* Avoid recalculating the hash later on */
	skb->hash = flow_node->hash_value;
	skb->sw_hash = 1;
	/* We've already validated all data in the flow nodes */
	skb->ip_summed = CHECKSUM_UNNECESSARY;

	/* GSO information only needs to be added/updated if we actually
	 * coaleced any packets.
	*/
	if (flow_node->num_pkts_held <= 1)
		return;

	datagram_len = skb->len - flow_node->ip_len;

	/* Update headers to reflect the new packet length.
	 * Transport checksum needs to be set to the pseudo header checksum
	 * since we need to mark the SKB as CHECKSUM_PARTIAL so the stack can
	 * segment properly.
	 */
	if (ipv4) {
		iph->tot_len = htons(datagram_len + flow_node->ip_len);
		pseudo = ~csum_tcpudp_magic(iph->saddr, iph->daddr,
					    datagram_len,
					    flow_node->trans_proto, 0);
		iph->check = 0;
		iph->check = ip_fast_csum((u8 *)iph, iph->ihl);
	} else {
		struct ipv6hdr *ip6h = (struct ipv6hdr *)iph;

		/* Payload len includes any extension headers */
		ip6h->payload_len = htons(skb->len - sizeof(*ip6h));
		pseudo = ~csum_ipv6_magic(&ip6h->saddr, &ip6h->daddr,
					  datagram_len, flow_node->trans_proto,
					  0);
	}

	switch (flow_node->trans_proto) {
	case IPPROTO_TCP:
		tp = (struct tcphdr *)((u8 *)iph + flow_node->ip_len);
		tp->check = pseudo;
		skb->csum_start = (u8 *)tp - skb->head;
		skb->csum_offset = offsetof(struct tcphdr, check);
		shinfo->gso_type = (ipv4) ? SKB_GSO_TCPV4 : SKB_GSO_TCPV6;
		break;
	case IPPROTO_UDP:
		up = (struct udphdr *)((u8 *)iph + flow_node->ip_len);
		up->len = htons(datagram_len);
		up->check = pseudo;
		skb->csum_start = (u8 *)up - skb->head;
		skb->csum_offset = offsetof(struct udphdr, check);
		shinfo->gso_type = SKB_GSO_UDP_L4;
		break;
	default:
		return;
	}

	/* Update GSO metadata */
	shinfo->gso_size = flow_node->gso_len;
	skb->ip_summed = CHECKSUM_PARTIAL;
}

/* rmnet_perf_opt_get_new_flow_index() - Pull flow node from node pool
 *
 * Fetch the flow node from the node pool. If we have already given
 * out all the flow nodes then we will always hit the else case and
 * thereafter we will use modulo arithmetic to choose which flow node
 * to evict and use.
 *
 * Return:
 *		- flow_node: node to be used by caller function
 **/
static struct rmnet_perf_opt_flow_node *rmnet_perf_opt_get_new_flow_index(void)
{
	struct rmnet_perf *perf = rmnet_perf_config_get_perf();
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
	rmnet_perf_opt_flush_single_flow_node(flow_node_ejected);
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
		struct iphdr *iph = pkt_info->ip_hdr.v4hdr;
		/* Frags don't make it this far, so this is all we care about */
		__be16 flags = iph->frag_off & htons(IP_CE | IP_DF);

		flow_node->ip_flags.ip4_flags.ip_ttl = iph->ttl;
		flow_node->ip_flags.ip4_flags.ip_tos = iph->tos;
		flow_node->ip_flags.ip4_flags.ip_frag_off = flags;
	} else if (pkt_info->ip_proto == 0x06) {
		__be32 *word = (__be32 *)pkt_info->ip_hdr.v6hdr;

		flow_node->ip_flags.first_word = *word;
	}
}

/* rmnet_perf_opt_flush_single_flow_node() - Send a given flow node up
 *		NW stack.
 * @flow_node: opt structure containing packet we are allocating for
 *
 * Send a given flow up NW stack via specific VND
 *
 * Return:
 *    - Void
 **/
void rmnet_perf_opt_flush_single_flow_node(
				struct rmnet_perf_opt_flow_node *flow_node)
{
	if (flow_node->num_pkts_held) {
		if (!rmnet_perf_core_is_deag_mode()) {
			struct rmnet_frag_descriptor *frag_desc;

			rmnet_perf_opt_add_flow_subfrags(flow_node);
			frag_desc = flow_node->pkt_list[0].frag_desc;
			frag_desc->hash = flow_node->hash_value;
			rmnet_perf_core_send_desc(frag_desc);
		} else {
			struct sk_buff *skb;

			skb = rmnet_perf_opt_make_flow_skb(flow_node);
			if (skb) {
				rmnet_perf_opt_flow_skb_fixup(skb, flow_node);
				rmnet_perf_core_send_skb(skb, flow_node->ep);
			} else {
				rmnet_perf_opt_oom_drops +=
					flow_node->num_pkts_held;
			}
		}

		/* equivalent to memsetting the flow node */
		flow_node->num_pkts_held = 0;
		flow_node->len = 0;
	}
}

/* rmnet_perf_opt_flush_flow_by_hash() - Iterate through all flow nodes
 *	that match a certain hash and flush the match
 * @hash_val: hash value we are looking to match and hence flush
 *
 * Return:
 *    - void
 **/
void rmnet_perf_opt_flush_flow_by_hash(u32 hash_val)
{
	struct rmnet_perf_opt_flow_node *flow_node;

	hash_for_each_possible(rmnet_perf_opt_fht, flow_node, list, hash_val) {
		if (hash_val == flow_node->hash_value &&
		    flow_node->num_pkts_held > 0)
			rmnet_perf_opt_flush_single_flow_node(flow_node);
	}
}

/* rmnet_perf_opt_flush_all_flow_nodes() - Iterate through all flow nodes
 *		and flush them individually
 *
 * Return:
 *    - void
 **/
void rmnet_perf_opt_flush_all_flow_nodes(void)
{
	struct rmnet_perf_opt_flow_node *flow_node;
	int bkt_cursor;
	int num_pkts_held;
	u32 hash_val;

	hash_for_each(rmnet_perf_opt_fht, bkt_cursor, flow_node, list) {
		hash_val = flow_node->hash_value;
		num_pkts_held = flow_node->num_pkts_held;
		if (num_pkts_held > 0) {
			rmnet_perf_opt_flush_single_flow_node(flow_node);
		}
	}
}

/* rmnet_perf_opt_chain_end() - Handle end of SKB chain notification
 *
 * Return:
 *    - void
 **/
void rmnet_perf_opt_chain_end(void)
{
	rmnet_perf_core_grab_lock();
	rmnet_perf_opt_flush_reason_cnt[RMNET_PERF_OPT_CHAIN_END]++;
	rmnet_perf_opt_flush_all_flow_nodes();
	rmnet_perf_core_release_lock();
}

/* rmnet_perf_opt_insert_pkt_in_flow() - Inserts single IP packet into
 *		opt meta structure
 * @flow_node: flow node we are going to insert the ip packet into
 * @pkt_info: characteristics of the current packet
 *
 * Return:
 *    - void
 **/
void rmnet_perf_opt_insert_pkt_in_flow(
			struct rmnet_perf_opt_flow_node *flow_node,
			struct rmnet_perf_pkt_info *pkt_info)
{
	struct rmnet_perf_opt_pkt_node *pkt_node;
	struct tcphdr *tp = pkt_info->trans_hdr.tp;
	void *iph = (void *)pkt_info->ip_hdr.v4hdr;
	u16 header_len = pkt_info->ip_len + pkt_info->trans_len;
	u16 payload_len = pkt_info->payload_len;
	unsigned char ip_version = pkt_info->ip_proto;

	pkt_node = &flow_node->pkt_list[flow_node->num_pkts_held];
	pkt_node->header_start = (unsigned char *)iph;
	pkt_node->data_len = payload_len;
	flow_node->len += payload_len;
	flow_node->num_pkts_held++;

	/* Set appropriate data pointers based on mode */
	if (!rmnet_perf_core_is_deag_mode()) {
		pkt_node->frag_desc = pkt_info->frag_desc;
		pkt_node->data_start = rmnet_frag_data_ptr(pkt_info->frag_desc);
		pkt_node->data_start += header_len;
	} else {
		pkt_node->data_start = (unsigned char *)iph + header_len;
	}

	if (pkt_info->first_packet) {
		/* Copy over flow information */
		flow_node->ep = pkt_info->ep;
		flow_node->ip_proto = ip_version;
		flow_node->trans_proto = pkt_info->trans_proto;
		flow_node->src_port = tp->source;
		flow_node->dest_port = tp->dest;
		flow_node->ip_len = pkt_info->ip_len;
		flow_node->trans_len = pkt_info->trans_len;
		flow_node->hash_value = pkt_info->hash_key;
		/* Use already stamped gso_size if available */
		if (!rmnet_perf_core_is_deag_mode() &&
		    pkt_info->frag_desc->gso_size)
			flow_node->gso_len = pkt_info->frag_desc->gso_size;
		else
			flow_node->gso_len = payload_len;

		if (ip_version == 0x04) {
			flow_node->saddr.saddr4 =
				(__be32)((struct iphdr *)iph)->saddr;
			flow_node->daddr.daddr4 =
				(__be32)((struct iphdr *)iph)->daddr;
			flow_node->trans_proto =
				((struct iphdr *)iph)->protocol;
		} else {
			flow_node->saddr.saddr6 =
				((struct ipv6hdr *)iph)->saddr;
			flow_node->daddr.daddr6 =
				((struct ipv6hdr *)iph)->daddr;
			flow_node->trans_proto =
				((struct ipv6hdr *)iph)->nexthdr;
		}

		/* Set initial TCP SEQ number */
		if (pkt_info->trans_proto == IPPROTO_TCP) {
			if (pkt_info->frag_desc &&
			    pkt_info->frag_desc->tcp_seq_set) {
				__be32 seq = pkt_info->frag_desc->tcp_seq;

				flow_node->next_seq = ntohl(seq);
			} else {
				flow_node->next_seq = ntohl(tp->seq);
			}
		}

	}

	if (pkt_info->trans_proto == IPPROTO_TCP)
		flow_node->next_seq += payload_len;
}
void
rmnet_perf_free_hash_table()
{
	int i;
	struct rmnet_perf_opt_flow_node *flow_node;
	struct hlist_node *tmp;

	hash_for_each_safe(rmnet_perf_opt_fht, i, tmp, flow_node, list) {
		hash_del(&flow_node->list);
	}

}

/* rmnet_perf_opt_ingress() - Core business logic of optimization framework
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
bool rmnet_perf_opt_ingress(struct rmnet_perf_pkt_info *pkt_info)
{
	struct rmnet_perf_opt_flow_node *flow_node;
	struct rmnet_perf_opt_flow_node *flow_node_recycled;
	bool flush;
	bool handled = false;
	bool flow_node_exists = false;

	if (!rmnet_perf_optimize_protocol(pkt_info->trans_proto))
		goto out;

handle_pkt:
	hash_for_each_possible(rmnet_perf_opt_fht, flow_node, list,
			       pkt_info->hash_key) {
		if (!rmnet_perf_opt_identify_flow(flow_node, pkt_info))
			continue;

		flush = rmnet_perf_opt_ip_flag_flush(flow_node, pkt_info);

		/* set this to true by default. Let the protocol helpers
		 * change this if it is needed.
		 */
		pkt_info->first_packet = true;
		flow_node_exists = true;

		switch (pkt_info->trans_proto) {
		case IPPROTO_TCP:
			rmnet_perf_tcp_opt_ingress(flow_node, pkt_info, flush);
			handled = true;
			goto out;
		case IPPROTO_UDP:
			rmnet_perf_udp_opt_ingress(flow_node, pkt_info, flush);
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
		flow_node_recycled = rmnet_perf_opt_get_new_flow_index();
		flow_node_recycled->hash_value = pkt_info->hash_key;
		rmnet_perf_opt_update_flow(flow_node_recycled, pkt_info);
		hash_add(rmnet_perf_opt_fht, &flow_node_recycled->list,
			 pkt_info->hash_key);
		goto handle_pkt;
	}

out:
	return handled;
}
