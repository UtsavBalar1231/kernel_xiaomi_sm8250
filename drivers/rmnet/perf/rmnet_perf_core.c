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
#include <linux/spinlock.h>
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

/* Number of non-ip packets coming into rmnet_perf */
unsigned long int rmnet_perf_core_non_ip_count;
module_param(rmnet_perf_core_non_ip_count, ulong, 0444);
MODULE_PARM_DESC(rmnet_perf_core_non_ip_count,
		 "Number of non-ip packets entering rmnet_perf");

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

unsigned long int rmnet_perf_qmap_size_mismatch = 0;
module_param(rmnet_perf_qmap_size_mismatch, ulong, 0444);
MODULE_PARM_DESC(rmnet_perf_qmap_size_mismatch,
		 "Number of mismatches b/w QMAP and IP lengths");

/* Handle deag by default for legacy behavior */
static bool rmnet_perf_ingress_deag = true;
module_param(rmnet_perf_ingress_deag, bool, 0444);
MODULE_PARM_DESC(rmnet_perf_ingress_deag,
		 "If true, rmnet_perf will handle QMAP deaggregation");

#define SHS_FLUSH				0
#define RECYCLE_BUFF_SIZE_THRESH		51200

/* Lock around flow nodes for syncornization with rmnet_perf_opt_mode changes */
static DEFINE_SPINLOCK(rmnet_perf_core_lock);

void rmnet_perf_core_grab_lock(void)
{
	spin_lock_bh(&rmnet_perf_core_lock);
}

void rmnet_perf_core_release_lock(void)
{
	spin_unlock_bh(&rmnet_perf_core_lock);
}

/* rmnet_perf_core_set_ingress_hook() - sets appropriate ingress hook
 *		in the core rmnet driver
 *
 * Return:
 *		- void
 **/
void rmnet_perf_core_set_ingress_hook(void)
{
	if (rmnet_perf_core_is_deag_mode()) {
		RCU_INIT_POINTER(rmnet_perf_deag_entry,
				 rmnet_perf_core_deaggregate);
		RCU_INIT_POINTER(rmnet_perf_desc_entry, NULL);
	} else {
		RCU_INIT_POINTER(rmnet_perf_deag_entry, NULL);
		RCU_INIT_POINTER(rmnet_perf_desc_entry,
				 rmnet_perf_core_desc_entry);
		RCU_INIT_POINTER(rmnet_perf_chain_end,
				 rmnet_perf_opt_chain_end);
	}
}

/* rmnet_perf_core_is_deag_mode() - get the ingress mode of the module
 *
 * Return:
 * 		- true: rmnet_perf is handling deaggregation
 *		- false: rmnet_perf is not handling deaggregation
 **/
inline bool rmnet_perf_core_is_deag_mode(void)
{
	return rmnet_perf_ingress_deag;
}

/* rmnet_perf_core_free_held_skbs() - Free held SKBs given to us by physical
 *		device
 *
 * Requires caller does any cleanup of protocol specific data structures
 * i.e. for tcp_opt the flow nodes must first be flushed so that we are
 * free to free the SKBs from physical device
 *
 * Return:
 *		- void
 **/
void rmnet_perf_core_free_held_skbs(void)
{
	struct rmnet_perf *perf = rmnet_perf_config_get_perf();
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
struct sk_buff *rmnet_perf_core_elligible_for_cache_skb(u32 len)
{
	struct rmnet_perf *perf = rmnet_perf_config_get_perf();
	struct rmnet_perf_core_64k_buff_pool *buff_pool;
	u8 circ_index, iterations;
	struct sk_buff *skbn;
	int user_count;

	buff_pool = perf->core_meta->buff_pool;
	if (len < RECYCLE_BUFF_SIZE_THRESH || !buff_pool->available[0])
		return NULL;

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
 * Return:
 *    - hash_key: unsigned 32 bit integer that is produced
 **/
u32 rmnet_perf_core_compute_flow_hash(struct rmnet_perf_pkt_info *pkt_info)
{
	u32 hash_key;
	struct udphdr *up;
	u32 hash_five_tuple[11];
	__be16 src = 0, dest = 0;

	if (pkt_info->trans_proto == IPPROTO_TCP ||
	    pkt_info->trans_proto == IPPROTO_UDP) {
		up = pkt_info->trans_hdr.up;
		src = up->source;
		dest = up->dest;
	}

	if (pkt_info->ip_proto == 0x04) {
		struct iphdr *ip4h = pkt_info->ip_hdr.v4hdr;

		hash_five_tuple[0] = ip4h->daddr;
		hash_five_tuple[1] = ip4h->saddr;
		hash_five_tuple[2] = ip4h->protocol;
		hash_five_tuple[3] = dest;
		hash_five_tuple[4] = src;
		hash_key = jhash2(hash_five_tuple, 5, 0);
	} else {
		struct ipv6hdr *ip6h = pkt_info->ip_hdr.v6hdr;
		struct in6_addr daddr = ip6h->daddr;
		struct in6_addr saddr = ip6h->saddr;

		hash_five_tuple[0] = ((u32 *) &daddr)[0];
		hash_five_tuple[1] = ((u32 *) &daddr)[1];
		hash_five_tuple[2] = ((u32 *) &daddr)[2];
		hash_five_tuple[3] = ((u32 *) &daddr)[3];
		hash_five_tuple[4] = ((u32 *) &saddr)[0];
		hash_five_tuple[5] = ((u32 *) &saddr)[1];
		hash_five_tuple[6] = ((u32 *) &saddr)[2];
		hash_five_tuple[7] = ((u32 *) &saddr)[3];
		hash_five_tuple[8] = ip6h->nexthdr;
		hash_five_tuple[9] = dest;
		hash_five_tuple[10] = src;
		hash_key = jhash2(hash_five_tuple, 11, 0);
	}

	return hash_key;
}

/* rmnet_perf_core_accept_new_skb() - Add SKB to list to be freed later
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
void rmnet_perf_core_accept_new_skb(struct sk_buff *skb)
{
	struct rmnet_perf *perf = rmnet_perf_config_get_perf();
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

/* rmnet_perf_core_send_skb() - Send SKB to the network stack
 * @skb: packet to send
 * @ep: VND to send packet to
 *
 * Return:
 *    - void
 **/
void rmnet_perf_core_send_skb(struct sk_buff *skb, struct rmnet_endpoint *ep)
{
	struct rmnet_perf *perf = rmnet_perf_config_get_perf();

	/* Log our outgoing size */
	rmnet_perf_core_packet_sz_stats(skb->len);

	if (perf->rmnet_port->data_format & 8)
		skb->dev = ep->egress_dev;

	rmnet_set_skb_proto(skb);
	rmnet_deliver_skb(skb, perf->rmnet_port);
}

void rmnet_perf_core_send_desc(struct rmnet_frag_descriptor *frag_desc)
{
	struct rmnet_perf *perf = rmnet_perf_config_get_perf();

	/* Log our outgoing size */
	rmnet_perf_core_packet_sz_stats(0);

	rmnet_frag_deliver(frag_desc, perf->rmnet_port);
}

/* rmnet_perf_core_flush_curr_pkt() - Send a single ip packet up the stack
 * @pkt_info: characteristics of the current packet
 * @packet_len: length of the packet we need to allocate for
 *
 * In this case we know that the packet we are sending up is a single
 * MTU sized IP packet and it did not get tcp_opt'd
 *
 * Return:
 *    - void
 **/
void rmnet_perf_core_flush_curr_pkt(struct rmnet_perf_pkt_info *pkt_info,
				    u16 packet_len, bool flush_shs,
				    bool skip_hash)
{
	if (packet_len > 65536) {
		pr_err("%s(): Packet too long", __func__);
		return;
	}

	if (!rmnet_perf_core_is_deag_mode()) {
		struct rmnet_frag_descriptor *frag_desc = pkt_info->frag_desc;

		/* Only set hash info if we actually calculated it */
		if (!skip_hash)
			frag_desc->hash = pkt_info->hash_key;

		frag_desc->flush_shs = flush_shs;
		rmnet_perf_core_send_desc(frag_desc);
	} else {
		struct sk_buff *skb;

		skb = alloc_skb(packet_len + RMNET_MAP_DEAGGR_SPACING,
				GFP_ATOMIC);
		if (!skb)
			return;

		skb_reserve(skb, RMNET_MAP_DEAGGR_HEADROOM);
		skb_put_data(skb, pkt_info->ip_hdr.v4hdr, packet_len);

		/* If the packet passed checksum validation, tell the stack */
		if (pkt_info->csum_valid)
			skb->ip_summed = CHECKSUM_UNNECESSARY;

		skb->dev = pkt_info->skb->dev;

		/* Only set hash information if we actually calculated it */
		if (!skip_hash) {
			skb->hash = pkt_info->hash_key;
			skb->sw_hash = 1;
		}

		skb->cb[SHS_FLUSH] = flush_shs;
		rmnet_perf_core_send_skb(skb, pkt_info->ep);
	}
}

/* DL marker is off, we need to flush more aggresively at end of chains */
void rmnet_perf_core_ps_on(void *port)
{
	struct rmnet_perf *perf = rmnet_perf_config_get_perf();

	rmnet_perf_core_bm_flush_on = 0;
	/* Essentially resets expected packet count to safe state */
	perf->core_meta->bm_state->expect_packets = -1;
}

/* DL marker on, we can try to coalesce more packets */
void rmnet_perf_core_ps_off(void *port)
{
	rmnet_perf_core_bm_flush_on = 1;
}

void
rmnet_perf_core_handle_map_control_start_v2(struct rmnet_map_dl_ind_hdr *dlhdr,
				struct rmnet_map_control_command_header *qcmd)
{
	rmnet_perf_core_handle_map_control_start(dlhdr);
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
		rmnet_perf_opt_flush_all_flow_nodes();
		rmnet_perf_core_flush_reason_cnt[
					RMNET_PERF_CORE_DL_MARKER_FLUSHES]++;
	} else {
		bm_state->wait_for_start = false;
	}

	bm_state->curr_seq = dlhdr->le.seq;
	bm_state->expect_packets = dlhdr->le.pkts;
	trace_rmnet_perf_low(RMNET_PERF_MODULE, RMNET_PERF_START_DL_MRK,
			     bm_state->expect_packets, 0xDEF, 0xDEF, 0xDEF,
			     NULL, NULL);
}

void rmnet_perf_core_handle_map_control_end_v2(struct rmnet_map_dl_ind_trl *dltrl,
				struct rmnet_map_control_command_header *qcmd)
{
	rmnet_perf_core_handle_map_control_end(dltrl);
}

void rmnet_perf_core_handle_map_control_end(struct rmnet_map_dl_ind_trl *dltrl)
{
	struct rmnet_perf *perf = rmnet_perf_config_get_perf();
	struct rmnet_perf_core_burst_marker_state *bm_state;

	bm_state = perf->core_meta->bm_state;
	rmnet_perf_opt_flush_all_flow_nodes();
	rmnet_perf_core_flush_reason_cnt[RMNET_PERF_CORE_DL_MARKER_FLUSHES]++;
	bm_state->wait_for_start = true;
	bm_state->curr_seq = 0;
	bm_state->expect_packets = 0;
	trace_rmnet_perf_low(RMNET_PERF_MODULE, RMNET_PERF_END_DL_MRK, 0xDEF,
			     0xDEF, 0xDEF, 0xDEF, NULL, NULL);
}

int rmnet_perf_core_validate_pkt_csum(struct sk_buff *skb,
				      struct rmnet_perf_pkt_info *pkt_info)
{
	int result;
	unsigned int pkt_len = pkt_info->ip_len + pkt_info->trans_len +
			       pkt_info->payload_len;

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

/* rmnet_perf_core_dissect_pkt() - Extract packet header metadata for easier
 * lookup later
 * @payload: the data to analyze
 * @offset: Offset from start of payload to the IP header
 * @pkt_info: struct to fill in
 * @pkt_len: length of the packet
 * @skip_hash: set to false if rmnet_perf can calculate the hash, true otherwise
 * @len_mismatch: set to true if there is a mismatch between the IP length and
 * the QMAP length of the packet
 *
 * Return:
 *		- true if packet needs to be dropped
 *		- false if rmnet_perf can potentially optimize
 **/
bool rmnet_perf_core_dissect_pkt(unsigned char *payload,
				 struct rmnet_perf_pkt_info *pkt_info,
				 int offset, u16 pkt_len, bool *skip_hash,
				 bool *len_mismatch)
{
	bool flush = true;
	bool mismatch = false;
	u16 ip_pkt_len = 0;

	payload += offset;
	pkt_info->ip_proto = (*payload & 0xF0) >> 4;
	/* Set inital IP packet length based on descriptor size if this packet
	 * has already been segmented for any reason, as the IP header will
	 * no longer be correct */
	if (!rmnet_perf_core_is_deag_mode() &&
	    pkt_info->frag_desc->hdr_ptr !=
	    rmnet_frag_data_ptr(pkt_info->frag_desc)) {
		ip_pkt_len = skb_frag_size(&pkt_info->frag_desc->frag);
		ip_pkt_len += pkt_info->frag_desc->ip_len;
		ip_pkt_len += pkt_info->frag_desc->trans_len;
	}

	if (pkt_info->ip_proto == 4) {
		struct iphdr *iph;

		iph = (struct iphdr *)payload;
		pkt_info->ip_hdr.v4hdr = iph;

		/* Pass off frags immediately */
		if (iph->frag_off & htons(IP_MF | IP_OFFSET)) {
			rmnet_perf_frag_flush++;
			goto done;
		}

		if (!ip_pkt_len)
			ip_pkt_len = ntohs(iph->tot_len);

		mismatch = pkt_len != ip_pkt_len;
		pkt_info->ip_len = iph->ihl * 4;
		pkt_info->trans_proto = iph->protocol;

		if (!rmnet_perf_core_is_deag_mode()) {
			pkt_info->frag_desc->hdrs_valid = 1;
			pkt_info->frag_desc->ip_proto = 4;
			pkt_info->frag_desc->ip_len = pkt_info->ip_len;
			pkt_info->frag_desc->trans_proto =
				pkt_info->trans_proto;
		}
	} else if (pkt_info->ip_proto == 6) {
		struct ipv6hdr *ip6h;
		int len;
		__be16 frag_off;
		u8 protocol;

		ip6h = (struct ipv6hdr *)payload;
		pkt_info->ip_hdr.v6hdr = ip6h;
		protocol = ip6h->nexthdr;

		/* Dive down the header chain */
		if (!rmnet_perf_core_is_deag_mode())
			len = rmnet_frag_ipv6_skip_exthdr(pkt_info->frag_desc,
							  offset +
							  sizeof(*ip6h),
							  &protocol, &frag_off);
		else
			len = ipv6_skip_exthdr(pkt_info->skb,
					       offset + sizeof(*ip6h),
					       &protocol, &frag_off);
		if (len < 0) {
			/* Something somewhere has gone horribly wrong...
			 * Let the stack deal with it.
			 */
			goto done;
		}

		/* Returned length will include the offset value */
		len -= offset;

		/* Pass off frags immediately */
		if (frag_off) {
			/* Add in frag header length for non-first frags.
			 * ipv6_skip_exthdr() doesn't do that for you.
			 */
			if (protocol == NEXTHDR_FRAGMENT)
				len += sizeof(struct frag_hdr);
			pkt_info->ip_len = (u16)len;
			rmnet_perf_frag_flush++;
			goto done;
		}

		if (!ip_pkt_len)
			ip_pkt_len = ntohs(ip6h->payload_len) + sizeof(*ip6h);

		mismatch = pkt_len != ip_pkt_len;
		pkt_info->ip_len = (u16)len;
		pkt_info->trans_proto = protocol;

		if (!rmnet_perf_core_is_deag_mode()) {
			pkt_info->frag_desc->hdrs_valid = 1;
			pkt_info->frag_desc->ip_proto = 6;
			pkt_info->frag_desc->ip_len = pkt_info->ip_len;
			pkt_info->frag_desc->trans_proto =
				pkt_info->trans_proto;
		}
	} else {
		/* Not a valid IP packet */
		return true;
	}

	if (pkt_info->trans_proto == IPPROTO_TCP) {
		struct tcphdr *tp;

		tp = (struct tcphdr *)(payload + pkt_info->ip_len);
		pkt_info->trans_len = tp->doff * 4;
		pkt_info->trans_hdr.tp = tp;

		if (!rmnet_perf_core_is_deag_mode())
			pkt_info->frag_desc->trans_len = pkt_info->trans_len;
	} else if (pkt_info->trans_proto == IPPROTO_UDP) {
		struct udphdr *up;

		up = (struct udphdr *)(payload + pkt_info->ip_len);
		pkt_info->trans_len = sizeof(*up);
		pkt_info->trans_hdr.up = up;

		if (!rmnet_perf_core_is_deag_mode())
			pkt_info->frag_desc->trans_len = pkt_info->trans_len;
	} else {
		/* Not a protocol we can optimize */
		if (!rmnet_perf_core_is_deag_mode())
			pkt_info->frag_desc->hdrs_valid = 0;

		goto done;
	}

	flush = false;
	pkt_info->hash_key = rmnet_perf_core_compute_flow_hash(pkt_info);

done:
	pkt_info->payload_len = pkt_len - pkt_info->ip_len -
				pkt_info->trans_len;
	*skip_hash = flush;
	*len_mismatch = mismatch;
	if (mismatch) {
		rmnet_perf_qmap_size_mismatch++;
		if (!rmnet_perf_core_is_deag_mode())
			pkt_info->frag_desc->hdrs_valid = 0;
	}

	return false;
}

/* rmnet_perf_core_dissect_skb() - Extract packet header metadata for easier
 * lookup later
 * @skb: the skb to analyze
 * @pkt_info: struct to fill in
 * @offset: offset from start of skb data to the IP header
 * @pkt_len: length of the packet
 * @skip_hash: set to false if rmnet_perf can calculate the hash, true otherwise
 * @len_mismatch: set to true if there is a mismatch between the IP length and
 * the QMAP length of the packet
 *
 * Return:
 *		- true if packet needs to be dropped
 *		- false if rmnet_perf can potentially optimize
 **/

bool rmnet_perf_core_dissect_skb(struct sk_buff *skb,
				 struct rmnet_perf_pkt_info *pkt_info,
				 int offset, u16 pkt_len, bool *skip_hash,
				 bool *len_mismatch)
{
	pkt_info->skb = skb;
	return rmnet_perf_core_dissect_pkt(skb->data, pkt_info, offset,
					   pkt_len, skip_hash, len_mismatch);
}

/* rmnet_perf_core_dissect_desc() - Extract packet header metadata for easier
 * lookup later
 * @frag_desc: the descriptor to analyze
 * @pkt_info: struct to fill in
 * @offset: offset from start of descriptor payload to the IP header
 * @pkt_len: length of the packet
 * @skip_hash: set to false if rmnet_perf can calculate the hash, true otherwise
 * @len_mismatch: set tp true if there is a mismatch between the IP length and
 * the QMAP length of the packet
 *
 * Return:
 *		- true if packet needs to be flushed out immediately
 *		- false if rmnet_perf can potentially optimize
 **/

bool rmnet_perf_core_dissect_desc(struct rmnet_frag_descriptor *frag_desc,
				  struct rmnet_perf_pkt_info *pkt_info,
				  int offset, u16 pkt_len, bool *skip_hash,
				  bool *len_mismatch)
{
	u8 *payload = frag_desc->hdr_ptr;

	/* If this was segmented, the headers aren't in the pkt_len. Add them
	 * back for consistency.
	 */
	if (payload != rmnet_frag_data_ptr(frag_desc))
		pkt_len += frag_desc->ip_len + frag_desc->trans_len;

	pkt_info->frag_desc = frag_desc;
	return rmnet_perf_core_dissect_pkt(payload, pkt_info, offset, pkt_len,
					   skip_hash, len_mismatch);
}

void rmnet_perf_core_handle_packet_ingress(struct sk_buff *skb,
					   struct rmnet_endpoint *ep,
					   struct rmnet_perf_pkt_info *pkt_info,
					   u32 frame_len, u32 trailer_len)
{
	unsigned int offset = sizeof(struct rmnet_map_header);
	u16 pkt_len;
	bool skip_hash = false;
	bool len_mismatch = false;

	pkt_len = frame_len - offset - trailer_len;
	memset(pkt_info, 0, sizeof(*pkt_info));
	pkt_info->ep = ep;

	if (rmnet_perf_core_dissect_skb(skb, pkt_info, offset, pkt_len,
					&skip_hash, &len_mismatch)) {
		rmnet_perf_core_non_ip_count++;
		/* account for the bulk add in rmnet_perf_core_deaggregate() */
		rmnet_perf_core_pre_ip_count--;
		return;
	}

	if (skip_hash) {
		/* We're flushing anyway, so no need to check result */
		rmnet_perf_core_validate_pkt_csum(skb, pkt_info);
		goto flush;
	} else if (len_mismatch) {
		/* We're flushing anyway, so no need to check result */
		rmnet_perf_core_validate_pkt_csum(skb, pkt_info);
		/* Flush anything in the hash to avoid any OOO */
		rmnet_perf_opt_flush_flow_by_hash(pkt_info->hash_key);
		goto flush;
	}

	if (rmnet_perf_core_validate_pkt_csum(skb, pkt_info))
		goto flush;

	if (!rmnet_perf_opt_ingress(pkt_info))
		goto flush;

	return;

flush:
	rmnet_perf_core_flush_curr_pkt(pkt_info, pkt_len, false, skip_hash);
}

/* rmnet_perf_core_desc_entry() - Entry point for rmnet_perf's non-deag logic
 * @skb: the incoming skb from core driver
 * @port: the rmnet_perf struct from core driver
 *
 * Return:
 *		- void
 **/
void rmnet_perf_core_desc_entry(struct rmnet_frag_descriptor *frag_desc,
				struct rmnet_port *port)
{
	struct rmnet_perf_pkt_info pkt_info;
	struct rmnet_perf *perf = rmnet_perf_config_get_perf();
	u16 pkt_len = skb_frag_size(&frag_desc->frag);
	bool skip_hash = true;
	bool len_mismatch = false;

	rmnet_perf_core_grab_lock();
	perf->rmnet_port = port;
	memset(&pkt_info, 0, sizeof(pkt_info));
	if (rmnet_perf_core_dissect_desc(frag_desc, &pkt_info, 0, pkt_len,
					 &skip_hash, &len_mismatch)) {
		rmnet_perf_core_non_ip_count++;
		rmnet_recycle_frag_descriptor(frag_desc, port);
		rmnet_perf_core_release_lock();
		return;
	}

	/* We know the packet is an IP packet now */
	rmnet_perf_core_pre_ip_count++;
	if (skip_hash) {
		goto flush;
	} else if (len_mismatch) {
		/* Flush everything in the hash to avoid OOO */
		rmnet_perf_opt_flush_flow_by_hash(pkt_info.hash_key);
		goto flush;
	}

	/* Skip packets with bad checksums.
	 * This check is delayed here to allow packets that won't be
	 * checksummed by hardware (non-TCP/UDP data, fragments, padding) to be
	 * flushed by the above checks. This ensures that we report statistics
	 * correctly (i.e. rmnet_perf_frag_flush increases for each fragment),
	 * and don't report packets with valid checksums that weren't offloaded
	 * as "bad checksum" packets.
	 */
	if (!frag_desc->csum_valid)
		goto flush;

	if (!rmnet_perf_opt_ingress(&pkt_info))
		goto flush;

	rmnet_perf_core_release_lock();
	return;

flush:
	rmnet_perf_core_flush_curr_pkt(&pkt_info, pkt_len, false, skip_hash);
	rmnet_perf_core_release_lock();
}

int __rmnet_perf_core_deaggregate(struct sk_buff *skb, struct rmnet_port *port)
{
	struct rmnet_perf_pkt_info pkt_info;
	struct timespec curr_time, diff;
	static struct timespec last_drop_time;
	struct rmnet_map_header *maph;
	struct rmnet_endpoint *ep;
	u32 map_frame_len;
	u32 trailer_len = 0;
	int count = 0;
	u8 mux_id;

	while (skb->len != 0) {
		maph = (struct rmnet_map_header *)skb->data;

		trace_rmnet_perf_low(RMNET_PERF_MODULE, RMNET_PERF_DEAG_PKT,
				     0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);

		/* Some hardware can send us empty frames. Catch them.
		 * This includes IPA sending end of rx indications.
		 */
		if (ntohs(maph->pkt_len) == 0)
			goto out;

		map_frame_len = ntohs(maph->pkt_len) +
				sizeof(struct rmnet_map_header);

		if (port->data_format & RMNET_FLAGS_INGRESS_MAP_CKSUMV4) {
			trailer_len = sizeof(struct rmnet_map_dl_csum_trailer);
			map_frame_len += trailer_len;
		}

		if (((int)skb->len - (int)map_frame_len) < 0)
			goto out;

		/* Handle any command packets */
		if (maph->cd_bit) {
			/* rmnet_perf is only used on targets with DL marker.
			 * The legacy map commands are not used, so we don't
			 * check for them. If this changes, rmnet_map_command()
			 * will need to be called, and that function updated to
			 * not free SKBs if called from this module.
			 */
			if (port->data_format &
			    RMNET_INGRESS_FORMAT_DL_MARKER)
			        /* rmnet_map_flow_command() will handle pulling
				 * the data for us if it's actually a valid DL
				 * marker.
				 */
				if (!rmnet_map_flow_command(skb, port, true))
					continue;

			goto pull;
		}

		mux_id = maph->mux_id;
		if (mux_id >= RMNET_MAX_LOGICAL_EP)
			goto skip_frame;


		ep = rmnet_get_endpoint(port, mux_id);
		if (!ep)
			goto skip_frame;
		skb->dev = ep->egress_dev;

#ifdef CONFIG_QCOM_QMI_POWER_COLLAPSE
		/* Wakeup PS work on DL packets */
		if ((port->data_format & RMNET_INGRESS_FORMAT_PS) &&
		    !maph->cd_bit)
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
				goto skip_frame;
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
skip_frame:
		count++;
pull:
		skb_pull(skb, map_frame_len);
	}

out:
	return count;
}

/* rmnet_perf_core_deaggregate() - Deaggregate ip packets from map frame
 * @skb: the incoming aggregated MAP frame from PND
 * @port: rmnet_port struct from core driver
 *
 * Return:
 *		- void
 **/
void rmnet_perf_core_deaggregate(struct sk_buff *skb,
				 struct rmnet_port *port)
{
	struct rmnet_perf *perf;
	struct rmnet_perf_core_burst_marker_state *bm_state;
	int co = 0;
	int chain_count = 0;

	perf = rmnet_perf_config_get_perf();
	perf->rmnet_port = port;
	rmnet_perf_core_grab_lock();
	while (skb) {
		struct sk_buff *skb_frag = skb_shinfo(skb)->frag_list;

		skb_shinfo(skb)->frag_list = NULL;
		chain_count++;
		rmnet_perf_core_accept_new_skb(skb);
		co += __rmnet_perf_core_deaggregate(skb, port);
		skb = skb_frag;
	}

	bm_state = perf->core_meta->bm_state;
	bm_state->expect_packets -= co;
	/* if we ran out of data and should have gotten an end marker,
	 * then we can flush everything
	 */
	if (port->data_format == RMNET_INGRESS_FORMAT_DL_MARKER_V2 ||
	    !bm_state->callbacks_valid || !rmnet_perf_core_bm_flush_on ||
	    (int) bm_state->expect_packets <= 0) {
		rmnet_perf_opt_flush_all_flow_nodes();
		rmnet_perf_core_free_held_skbs();
		rmnet_perf_core_flush_reason_cnt[
					RMNET_PERF_CORE_IPA_ZERO_FLUSH]++;
	} else if (perf->core_meta->skb_needs_free_list->num_skbs_held >=
		   rmnet_perf_core_num_skbs_max) {
		rmnet_perf_opt_flush_all_flow_nodes();
		rmnet_perf_core_free_held_skbs();
		rmnet_perf_core_flush_reason_cnt[
					RMNET_PERF_CORE_SK_BUFF_HELD_LIMIT]++;
	}

	rmnet_perf_core_pre_ip_count += co;
	rmnet_perf_core_chain_count[chain_count]++;
	rmnet_perf_core_release_lock();
}
