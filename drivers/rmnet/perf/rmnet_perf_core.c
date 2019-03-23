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
 * RMNET core functionalities. Common helpers
 *
 */
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/jhash.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <net/ip6_checksum.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_map.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_private.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_handlers.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_config.h>
#include "rmnet_perf_opt.h"
#include "rmnet_perf_core.h"
#include "rmnet_perf_config.h"

#ifdef CONFIG_QCOM_QMI_POWER_COLLAPSE
#include <soc/qcom/qmi_rmnet.h>
#endif

/* Each index tells us the number of iterations it took us to find a recycled
 * skb
 */
unsigned long int
rmnet_perf_core_skb_recycle_iterations[RMNET_PERF_NUM_64K_BUFFS + 1];
module_param_array(rmnet_perf_core_skb_recycle_iterations, ulong, 0, 0444);
MODULE_PARM_DESC(rmnet_perf_core_skb_recycle_iterations,
		 "Skb recycle statistics");

/* Number of SKBs we are allowed to accumulate from HW before we must flush
 * everything
 */
unsigned long int rmnet_perf_core_num_skbs_max = 300;
module_param(rmnet_perf_core_num_skbs_max, ulong, 0644);
MODULE_PARM_DESC(rmnet_perf_core_num_skbs_max, "Num skbs max held from HW");

/* Toggle to flush all coalesced packets when physical device is out of
 * packets
 */
unsigned long int rmnet_perf_core_bm_flush_on = 1;
module_param(rmnet_perf_core_bm_flush_on, ulong, 0644);
MODULE_PARM_DESC(rmnet_perf_core_bm_flush_on, "turn on bm flushing");

/* Number of ip packets coming into rmnet from physical device */
unsigned long int rmnet_perf_core_pre_ip_count;
module_param(rmnet_perf_core_pre_ip_count, ulong, 0644);
MODULE_PARM_DESC(rmnet_perf_core_pre_ip_count,
		 "Number of ip packets from physical device");

/* Number of ip packets leaving rmnet */
unsigned long int rmnet_perf_core_post_ip_count;
module_param(rmnet_perf_core_post_ip_count, ulong, 0644);
MODULE_PARM_DESC(rmnet_perf_core_post_ip_count,
		 "Number of ip packets leaving rmnet");

/* Stat which shows what distribution of coalescing we are getting */
unsigned long int rmnet_perf_core_pkt_size[RMNET_PERF_CORE_DEBUG_BUCKETS_MAX];
module_param_array(rmnet_perf_core_pkt_size, ulong, 0, 0444);
MODULE_PARM_DESC(rmnet_perf_core_pkt_size,
		 "rmnet perf coalescing size statistics");

/* Stat showing reason for flushes of flow nodes */
unsigned long int
rmnet_perf_core_flush_reason_cnt[RMNET_PERF_CORE_NUM_CONDITIONS];
module_param_array(rmnet_perf_core_flush_reason_cnt, ulong, 0, 0444);
MODULE_PARM_DESC(rmnet_perf_core_flush_reason_cnt,
		 "Reasons for flushing statistics");

unsigned long int rmnet_perf_core_chain_count[50];
module_param_array(rmnet_perf_core_chain_count, ulong, 0, 0444);
MODULE_PARM_DESC(rmnet_perf_core_chain_count,
		 "Number of chained SKBs we process");

unsigned long int enable_packet_dropper;
module_param(enable_packet_dropper, ulong, 0644);
MODULE_PARM_DESC(enable_packet_dropper, "enable_packet_dropper");

unsigned long int packet_dropper_time = 1;
module_param(packet_dropper_time, ulong, 0644);
MODULE_PARM_DESC(packet_dropper_time, "packet_dropper_time");

unsigned long int rmnet_perf_flush_shs = 0;
module_param(rmnet_perf_flush_shs, ulong, 0644);
MODULE_PARM_DESC(rmnet_perf_flush_shs, "rmnet_perf_flush_shs");

unsigned long int rmnet_perf_frag_flush = 0;
module_param(rmnet_perf_frag_flush, ulong, 0444);
MODULE_PARM_DESC(rmnet_perf_frag_flush,
		 "Number of packet fragments flushed to stack");

#define SHS_FLUSH 0

/* rmnet_perf_core_free_held_skbs() - Free held SKBs given to us by physical
 *		device
 * @perf: allows access to our required global structures
 *
 * Requires caller does any cleanup of protocol specific data structures
 * i.e. for tcp_opt the flow nodes must first be flushed so that we are
 * free to free the SKBs from physical device
 *
 * Return:
 *		- void
 **/
void rmnet_perf_core_free_held_skbs(struct rmnet_perf *perf)
{
	struct rmnet_perf_core_skb_list *skb_list;

	skb_list = perf->core_meta->skb_needs_free_list;
	if (skb_list->num_skbs_held > 0)
		kfree_skb_list(skb_list->head);
	skb_list->num_skbs_held = 0;
}

/* rmnet_perf_core_reset_recycled_skb() - Clear/prepare recycled SKB to be
 *		used again
 * @skb: recycled buffer we are clearing out
 *
 * Make sure any lingering frags tacked on my GRO are gone. Also make sure
 * miscellaneous fields are cleaned up
 *
 * Return:
 *		- void
 **/
void rmnet_perf_core_reset_recycled_skb(struct sk_buff *skb)
{
	unsigned int size;
	int i;
	struct skb_shared_info *shinfo = skb_shinfo(skb);
	unsigned char *head = skb->head;

	/* Free up frags that might have been appended by gro previously */
	for (i = 0; i < shinfo->nr_frags; i++)
		__skb_frag_unref(&shinfo->frags[i]);

	if (shinfo->frag_list)
		kfree_skb_list(shinfo->frag_list);

	skb->data = head;
	skb->data_len = 0;
	skb_reset_tail_pointer(skb);
	atomic_inc((atomic_t *) &skb->users);
	size = SKB_WITH_OVERHEAD(ksize(head));
	memset(skb, 0, offsetof(struct sk_buff, tail));
	skb->truesize = SKB_TRUESIZE(size);
	skb->mac_header = (typeof(skb->mac_header))~0U;
	skb->transport_header = (typeof(skb->transport_header))~0U;

	memset(shinfo, 0, offsetof(struct skb_shared_info, dataref));
}

/* rmnet_perf_core_elligible_for_cache_skb() - Find elligible recycled skb
 * @perf: allows access to our recycled buffer cache
 * @len: the outgoing packet length we plan to send out
 *
 * Traverse the buffer cache to see if we have any free buffers not
 * currently being used by NW stack, as is evident through the
 * users count.
 *
 * Return:
 *		- skbn: the buffer to be used
 *		- NULL: if the length is not elligible or if all buffers
 *				are busy in NW stack
 **/
struct sk_buff *rmnet_perf_core_elligible_for_cache_skb(struct rmnet_perf *perf,
							u32 len)
{
	struct rmnet_perf_core_64k_buff_pool *buff_pool;
	u8 circ_index, iterations;
	struct sk_buff *skbn;
	int user_count;

	if (len < 51200)
		return NULL;
	buff_pool = perf->core_meta->buff_pool;
	circ_index = buff_pool->index;
	iterations = 0;
	while (iterations < RMNET_PERF_NUM_64K_BUFFS) {
		user_count = (int) atomic_read((atomic_t *)
			&buff_pool->available[circ_index]->users);
		if (user_count > 1) {
			circ_index = (circ_index + 1) %
				     RMNET_PERF_NUM_64K_BUFFS;
			iterations++;
			continue;
		}

		skbn = buff_pool->available[circ_index];
		rmnet_perf_core_reset_recycled_skb(skbn);
		buff_pool->index = (circ_index + 1) %
				   RMNET_PERF_NUM_64K_BUFFS;
		rmnet_perf_core_skb_recycle_iterations[iterations]++;
		return skbn;
	}
	/* if we increment this stat, then we know we did'nt find a
	 * suitable buffer
	 */
	rmnet_perf_core_skb_recycle_iterations[iterations]++;
	return NULL;
}

/* rmnet_perf_core_compute_flow_hash() - calculate hash of a given packets
 *		5 tuple
 * @pkt_info: characteristics of the current packet
 *
 * TODO: expand to 5 tuple once this becomes generic (right now we
 * ignore protocol because we know that we have TCP only for tcp_opt)
 *
 * Return:
 *    - hash_key: unsigned 32 bit integer that is produced
 **/
u32 rmnet_perf_core_compute_flow_hash(struct rmnet_perf_pkt_info *pkt_info)
{
	u32 hash_key;
	struct tcphdr *tp;
	struct udphdr *uhdr;
	u32 hash_five_tuple[11];

	if (pkt_info->ip_proto == 0x04) {
		struct iphdr *ip4h = pkt_info->iphdr.v4hdr;

		hash_five_tuple[0] = ip4h->daddr;
		hash_five_tuple[1] = ip4h->saddr;
		hash_five_tuple[2] = ip4h->protocol;
		switch (pkt_info->trans_proto) {
		case (IPPROTO_TCP):
			tp = pkt_info->trns_hdr.tp;
			hash_five_tuple[3] = tp->dest;
			hash_five_tuple[4] = tp->source;
			break;
		case (IPPROTO_UDP):
			uhdr = pkt_info->trns_hdr.up;
			hash_five_tuple[3] = uhdr->dest;
			hash_five_tuple[4] = uhdr->source;
			break;
		default:
			hash_five_tuple[3] = 0;
			hash_five_tuple[4] = 0;
			break;
		}
		hash_key = jhash2(hash_five_tuple, 5, 0);
	} else {
		struct ipv6hdr *ip6h = (struct ipv6hdr *) pkt_info->iphdr.v6hdr;

		struct	in6_addr daddr = ip6h->daddr;
		struct	in6_addr saddr = ip6h->saddr;

		hash_five_tuple[0] =  ((u32 *) &daddr)[0];
		hash_five_tuple[1] = ((u32 *) &daddr)[1];
		hash_five_tuple[2] = ((u32 *) &daddr)[2];
		hash_five_tuple[3] = ((u32 *) &daddr)[3];
		hash_five_tuple[4] = ((u32 *) &saddr)[0];
		hash_five_tuple[5] = ((u32 *) &saddr)[1];
		hash_five_tuple[6] = ((u32 *) &saddr)[2];
		hash_five_tuple[7] = ((u32 *) &saddr)[3];
		hash_five_tuple[8] = ip6h->nexthdr;
		switch (pkt_info->trans_proto) {
		case (IPPROTO_TCP):
			tp = pkt_info->trns_hdr.tp;
			hash_five_tuple[9] = tp->dest;
			hash_five_tuple[10] = tp->source;
			break;
		case (IPPROTO_UDP):
			uhdr = pkt_info->trns_hdr.up;
			hash_five_tuple[9] = uhdr->dest;
			hash_five_tuple[10] = uhdr->source;
			break;
		default:
			hash_five_tuple[9] = 0;
			hash_five_tuple[10] = 0;
			break;
		}
		hash_key = jhash2(hash_five_tuple, 11, 0);
	}
	return hash_key;
}

/* rmnet_perf_core_accept_new_skb() - Add SKB to list to be freed later
 * @perf: allows access to our required global structures
 * @skb: the incoming aggregated MAP frame from PND
 *
 * Adds to a running list of SKBs which we will free at a later
 * point in time. By not freeing them right away we are able
 * to grow our tcp_opt'd packets to be larger than a single max
 * sized aggregated MAP frame is
 *
 * Return:
 *		- void
 **/
static void rmnet_perf_core_accept_new_skb(struct rmnet_perf *perf,
					   struct sk_buff *skb)
{
	struct rmnet_perf_core_skb_list *skb_needs_free_list;

	skb_needs_free_list = perf->core_meta->skb_needs_free_list;
	if (!skb_needs_free_list->num_skbs_held) {
		skb_needs_free_list->head = skb;
		skb_needs_free_list->tail = skb;
	} else {
		skb_needs_free_list->tail->next = skb;
		skb_needs_free_list->tail = skb_needs_free_list->tail->next;
	}
	skb_needs_free_list->num_skbs_held++;
}

/* rmnet_perf_core_packet_sz_stats() - update stats indicating the coalescing
 *		of rmnet
 * @len: length of the packet being pushed up
 *
 * Return:
 *    -void
 **/
static void rmnet_perf_core_packet_sz_stats(unsigned int len)
{
	rmnet_perf_core_post_ip_count++;
	if (len > 50000)
		rmnet_perf_core_pkt_size[RMNET_PERF_CORE_50000_PLUS]++;
	else if (len > 30000)
		rmnet_perf_core_pkt_size[RMNET_PERF_CORE_30000_PLUS]++;
	else if (len > 23000)
		rmnet_perf_core_pkt_size[RMNET_PERF_CORE_23000_PLUS]++;
	else if (len > 14500)
		rmnet_perf_core_pkt_size[RMNET_PERF_CORE_14500_PLUS]++;
	else if (len > 7000)
		rmnet_perf_core_pkt_size[RMNET_PERF_CORE_7000_PLUS]++;
	else if (len > 1400)
		rmnet_perf_core_pkt_size[RMNET_PERF_CORE_1400_PLUS]++;
	else
		rmnet_perf_core_pkt_size[RMNET_PERF_CORE_0_PLUS]++;
}

/* rmnet_perf_core_send_skb() - Send (potentially) tcp_opt'd SKB to NW stack
 * @skb: packet to send
 * @ep: VND to send packet to
 * @perf: allows access to our required global structures
 *
 * Take newly formed linear SKB from tcp_opt and flush it up the stack
 * Also works with a non-tcp_opt'd packet, i.e. regular UDP packet
 *
 * Return:
 *    - void
 **/
void rmnet_perf_core_send_skb(struct sk_buff *skb, struct rmnet_endpoint *ep,
			      struct rmnet_perf *perf, struct rmnet_perf_pkt_info *pkt_info)
{
	unsigned char ip_version;
	unsigned char *data;
	struct iphdr *ip4hn;
	struct ipv6hdr *ip6hn;

	rmnet_perf_core_packet_sz_stats(skb->len);
	data = (unsigned char *)(skb->data);
	if (perf->rmnet_port->data_format & 8)
		skb->dev = ep->egress_dev;
	ip_version = (*data & 0xF0) >> 4;
	if (ip_version == 0x04) {
		ip4hn = (struct iphdr *) data;
		rmnet_set_skb_proto(skb);
		/* If the checksum is unnecessary, update the header fields.
		 * Otherwise, we know that this is a single packet that
		 * either failed checksum validation, or is not coalescable
		 * (fragment, ICMP, etc), so don't touch the headers.
		 */
		if (skb_csum_unnecessary(skb)) {
			ip4hn->tot_len = htons(skb->len);
			ip4hn->check = 0;
			ip4hn->check = ip_fast_csum(ip4hn, (int)ip4hn->ihl);
		}
		rmnet_deliver_skb(skb, perf->rmnet_port);
	} else if (ip_version == 0x06) {
		ip6hn = (struct ipv6hdr *)data;
		rmnet_set_skb_proto(skb);
		if (skb_csum_unnecessary(skb)) {
			ip6hn->payload_len = htons(skb->len -
						   sizeof(struct ipv6hdr));
		}
		rmnet_deliver_skb(skb, perf->rmnet_port);
	} else {
		pr_err("%s(): attempted to send invalid ip packet up stack\n",
		       __func__);
	}
}

/* rmnet_perf_core_flush_curr_pkt() - Send a single ip packet up the stack
 * @perf: allows access to our required global structures
 * @skb: packet to send
 * @pkt_info: characteristics of the current packet
 * @packet_len: length of the packet we need to allocate for
 *
 * In this case we know that the packet we are sending up is a single
 * MTU sized IP packet and it did not get tcp_opt'd
 *
 * Return:
 *    - void
 **/
void rmnet_perf_core_flush_curr_pkt(struct rmnet_perf *perf,
				    struct sk_buff *skb,
				    struct rmnet_perf_pkt_info *pkt_info,
				    u16 packet_len, bool flush_shs,
				    bool skip_hash)
{
	struct sk_buff *skbn;
	struct rmnet_endpoint *ep = pkt_info->ep;

	if (packet_len > 65536) {
		pr_err("%s(): Packet too long", __func__);
		return;
	}

	/* allocate the sk_buff of proper size for this packet */
	skbn = alloc_skb(packet_len + RMNET_MAP_DEAGGR_SPACING,
			 GFP_ATOMIC);
	if (!skbn)
		return;

	skb_reserve(skbn, RMNET_MAP_DEAGGR_HEADROOM);
	skb_put(skbn, packet_len);
	memcpy(skbn->data, pkt_info->iphdr.v4hdr, packet_len);

	/* If the packet passed checksum validation, tell the stack */
	if (pkt_info->csum_valid)
		skbn->ip_summed = CHECKSUM_UNNECESSARY;
	skbn->dev = skb->dev;

	/* Only set hash info if we actually calculated it */
	if (!skip_hash) {
		skbn->hash = pkt_info->hash_key;
		skbn->sw_hash = 1;
	}

	skbn->cb[SHS_FLUSH] = (char) flush_shs;
	rmnet_perf_core_send_skb(skbn, ep, perf, pkt_info);
}

/* DL marker is off, we need to flush more aggresively at end of chains */
void rmnet_perf_core_ps_on(void *port)
{
	struct rmnet_perf *perf = rmnet_perf_config_get_perf();

	rmnet_perf_core_bm_flush_on = 0;
	rmnet_perf_opt_flush_all_flow_nodes(perf);
	rmnet_perf_core_flush_reason_cnt[RMNET_PERF_CORE_PS_MODE_ON]++;
	/* Essentially resets expected packet count to safe state */
	perf->core_meta->bm_state->expect_packets = -1;
}

/* DL marker on, we can try to coalesce more packets */
void rmnet_perf_core_ps_off(void *port)
{
	rmnet_perf_core_bm_flush_on = 1;
}

void
rmnet_perf_core_handle_map_control_start(struct rmnet_map_dl_ind_hdr *dlhdr)
{
	struct rmnet_perf *perf = rmnet_perf_config_get_perf();
	struct rmnet_perf_core_burst_marker_state *bm_state;

	bm_state = perf->core_meta->bm_state;
	/* if we get two starts in a row, without an end, then we flush
	 * and carry on
	 */
	if (!bm_state->wait_for_start) {
		/* flush everything, we got a 2nd start */
		rmnet_perf_opt_flush_all_flow_nodes(perf);
		rmnet_perf_core_flush_reason_cnt[
					RMNET_PERF_CORE_DL_MARKER_FLUSHES]++;
	} else {
		bm_state->wait_for_start = false;
	}

	bm_state->curr_seq = dlhdr->le.seq;
	bm_state->expect_packets = dlhdr->le.pkts;
	trace_rmnet_perf_low(RMNET_PERF_MODULE, RMNET_PERF_START_DL_MRK,
						bm_state->expect_packets, 0xDEF, 0xDEF, 0xDEF, NULL,
						NULL);
}

void rmnet_perf_core_handle_map_control_end(struct rmnet_map_dl_ind_trl *dltrl)
{
	struct rmnet_perf *perf = rmnet_perf_config_get_perf();
	struct rmnet_perf_core_burst_marker_state *bm_state;

	bm_state = perf->core_meta->bm_state;
	rmnet_perf_opt_flush_all_flow_nodes(perf);
	rmnet_perf_core_flush_reason_cnt[RMNET_PERF_CORE_DL_MARKER_FLUSHES]++;
	bm_state->wait_for_start = true;
	bm_state->curr_seq = 0;
	bm_state->expect_packets = 0;
	trace_rmnet_perf_low(RMNET_PERF_MODULE, RMNET_PERF_END_DL_MRK, 0xDEF, 0xDEF,
						0xDEF, 0xDEF, NULL, NULL);
}

int rmnet_perf_core_validate_pkt_csum(struct sk_buff *skb,
				      struct rmnet_perf_pkt_info *pkt_info)
{
	int result;
	unsigned int pkt_len = pkt_info->header_len + pkt_info->payload_len;

	skb_pull(skb, sizeof(struct rmnet_map_header));
	if (pkt_info->ip_proto == 0x04) {
		skb->protocol = htons(ETH_P_IP);
	} else if (pkt_info->ip_proto == 0x06) {
		skb->protocol = htons(ETH_P_IPV6);
	} else {
		pr_err("%s(): protocol field not set properly, protocol = %u\n",
		       __func__, pkt_info->ip_proto);
	}
	result = rmnet_map_checksum_downlink_packet(skb, pkt_len);
	skb_push(skb, sizeof(struct rmnet_map_header));
	/* Mark the current packet as OK if csum is valid */
	if (likely(result == 0))
		pkt_info->csum_valid = true;
	return result;
}

void rmnet_perf_core_handle_packet_ingress(struct sk_buff *skb,
					   struct rmnet_endpoint *ep,
					   struct rmnet_perf_pkt_info *pkt_info,
					   u32 frame_len, u32 trailer_len)
{
	unsigned char *payload = (unsigned char *)
				 (skb->data + sizeof(struct rmnet_map_header));
	struct rmnet_perf *perf = rmnet_perf_config_get_perf();
	u16 pkt_len;
	bool skip_hash = false;

	pkt_len = frame_len - sizeof(struct rmnet_map_header) - trailer_len;
	pkt_info->ep = ep;
	pkt_info->ip_proto = (*payload & 0xF0) >> 4;
	if (pkt_info->ip_proto == 4) {
		struct iphdr *iph = (struct iphdr *)payload;

		pkt_info->iphdr.v4hdr = iph;
		pkt_info->trans_proto = iph->protocol;
		pkt_info->header_len = iph->ihl * 4;
		skip_hash = !!(ntohs(iph->frag_off) & (IP_MF | IP_OFFSET));
	} else if (pkt_info->ip_proto == 6) {
		struct ipv6hdr *iph = (struct ipv6hdr *)payload;

		pkt_info->iphdr.v6hdr = iph;
		pkt_info->trans_proto = iph->nexthdr;
		pkt_info->header_len = sizeof(*iph);
		skip_hash = iph->nexthdr == NEXTHDR_FRAGMENT;
	} else {
		return;
	}

	/* Push out fragments immediately */
	if (skip_hash) {
		rmnet_perf_frag_flush++;
		goto flush;
	}

	if (pkt_info->trans_proto == IPPROTO_TCP) {
		struct tcphdr *tp = (struct tcphdr *)
				    (payload + pkt_info->header_len);

		pkt_info->trns_hdr.tp = tp;
		pkt_info->header_len += tp->doff * 4;
		pkt_info->payload_len = pkt_len - pkt_info->header_len;
		pkt_info->hash_key =
			rmnet_perf_core_compute_flow_hash(pkt_info);

		if (rmnet_perf_core_validate_pkt_csum(skb, pkt_info))
			goto flush;

		if (!rmnet_perf_opt_ingress(perf, skb, pkt_info))
			goto flush;
	} else if (pkt_info->trans_proto == IPPROTO_UDP) {
		struct udphdr *up = (struct udphdr *)
				    (payload + pkt_info->header_len);

		pkt_info->trns_hdr.up = up;
		pkt_info->header_len += sizeof(*up);
		pkt_info->payload_len = pkt_len - pkt_info->header_len;
		pkt_info->hash_key =
			rmnet_perf_core_compute_flow_hash(pkt_info);

		if (rmnet_perf_core_validate_pkt_csum(skb, pkt_info))
			goto flush;

		if (!rmnet_perf_opt_ingress(perf, skb, pkt_info))
			goto flush;
	} else {
		pkt_info->payload_len = pkt_len - pkt_info->header_len;
		skip_hash = true;
		/* We flush anyway, so the result of the validation
		 * does not need to be checked.
		 */
		rmnet_perf_core_validate_pkt_csum(skb, pkt_info);
		goto flush;
	}

	return;

flush:
	rmnet_perf_core_flush_curr_pkt(perf, skb, pkt_info, pkt_len, false,
				       skip_hash);
}

/* rmnet_perf_core_deaggregate() - Deaggregated ip packets from map frame
 * @port: allows access to our required global structures
 * @skb: the incoming aggregated MAP frame from PND
 *
 * If the packet is TCP then send it down the way of tcp_opt.
 * Otherwise we can send it down some other path.
 *
 * Return:
 *		- void
 **/
void rmnet_perf_core_deaggregate(struct sk_buff *skb,
				 struct rmnet_port *port)
{
	u8 mux_id;
	struct rmnet_map_header *maph;
	uint32_t map_frame_len;
	struct rmnet_endpoint *ep;
	struct rmnet_perf_pkt_info pkt_info;
	struct rmnet_perf *perf;
	struct timespec curr_time, diff;
	static struct timespec last_drop_time;
	u32 trailer_len = 0;
	int co = 0;
	int chain_count = 0;

	perf = rmnet_perf_config_get_perf();
	perf->rmnet_port = port;
	while (skb) {
		struct sk_buff *skb_frag = skb_shinfo(skb)->frag_list;

		skb_shinfo(skb)->frag_list = NULL;
		chain_count++;
		rmnet_perf_core_accept_new_skb(perf, skb);
skip_frame:
		while (skb->len != 0) {
			maph = (struct rmnet_map_header *) skb->data;
			if (port->data_format &
			    RMNET_INGRESS_FORMAT_DL_MARKER) {
				if (!rmnet_map_flow_command(skb, port, true))
					goto skip_frame;
			}

			trace_rmnet_perf_low(RMNET_PERF_MODULE, RMNET_PERF_DEAG_PKT, 0xDEF,
								0xDEF, 0xDEF, 0xDEF, NULL, NULL);

			/* Some hardware can send us empty frames. Catch them */
			/* This includes IPA sending end of rx indications */
			if (ntohs(maph->pkt_len) == 0) {
				pr_err("Dropping empty MAP frame, co = %d", co);
				goto next_chain;
			}

			map_frame_len = ntohs(maph->pkt_len) +
					sizeof(struct rmnet_map_header);

			if (port->data_format &
			    RMNET_FLAGS_INGRESS_MAP_CKSUMV4) {
				trailer_len =
				    sizeof(struct rmnet_map_dl_csum_trailer);
				map_frame_len += trailer_len;
			}

			if ((((int)skb->len) - ((int)map_frame_len)) < 0) {
				pr_err("%s(): Got malformed packet. Dropping",
				       __func__);
				goto next_chain;
			}

			mux_id = RMNET_MAP_GET_MUX_ID(skb);
			if (mux_id >= RMNET_MAX_LOGICAL_EP) {
				pr_err("Got packet on %s with bad mux id %d",
					skb->dev->name, mux_id);
				goto drop_packets;
			}

			ep = rmnet_get_endpoint(port, mux_id);
			if (!ep)
				goto bad_data;
			skb->dev = ep->egress_dev;

#ifdef CONFIG_QCOM_QMI_POWER_COLLAPSE
			/* Wakeup PS work on DL packets */
			if ((port->data_format & RMNET_INGRESS_FORMAT_PS) &&
					!RMNET_MAP_GET_CD_BIT(skb))
				qmi_rmnet_work_maybe_restart(port);
#endif

			if (enable_packet_dropper) {
				getnstimeofday(&curr_time);
				if (last_drop_time.tv_sec == 0 &&
				    last_drop_time.tv_nsec == 0)
					getnstimeofday(&last_drop_time);
				diff = timespec_sub(curr_time, last_drop_time);
				if (diff.tv_sec > packet_dropper_time) {
					getnstimeofday(&last_drop_time);
					pr_err("%s(): Dropped a packet!\n",
					       __func__);
					goto bad_data;
				}
			}
			/* if we got to this point, we are able to proceed
			 * with processing the packet i.e. we know we are
			 * dealing with a packet with no funny business inside
			 */
			rmnet_perf_core_handle_packet_ingress(skb, ep,
							      &pkt_info,
							      map_frame_len,
							      trailer_len);
bad_data:
			skb_pull(skb, map_frame_len);
			co++;
		}
next_chain:
		skb = skb_frag;
	}

	perf->core_meta->bm_state->expect_packets -= co;
	/* if we ran out of data and should have gotten an end marker,
	 * then we can flush everything
	 */
	if (!rmnet_perf_core_bm_flush_on ||
	    (int) perf->core_meta->bm_state->expect_packets <= 0) {
		rmnet_perf_opt_flush_all_flow_nodes(perf);
		rmnet_perf_core_free_held_skbs(perf);
		rmnet_perf_core_flush_reason_cnt[
					RMNET_PERF_CORE_IPA_ZERO_FLUSH]++;
	} else if (perf->core_meta->skb_needs_free_list->num_skbs_held >=
		   rmnet_perf_core_num_skbs_max) {
		rmnet_perf_opt_flush_all_flow_nodes(perf);
		rmnet_perf_core_free_held_skbs(perf);
		rmnet_perf_core_flush_reason_cnt[
					RMNET_PERF_CORE_SK_BUFF_HELD_LIMIT]++;
	}

	goto update_stats;
drop_packets:
	rmnet_perf_opt_flush_all_flow_nodes(perf);
	rmnet_perf_core_free_held_skbs(perf);
update_stats:
	rmnet_perf_core_pre_ip_count += co;
	rmnet_perf_core_chain_count[chain_count]++;
}
