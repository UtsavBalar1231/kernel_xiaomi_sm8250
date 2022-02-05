/* Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
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
 * RMNET Data Smart Hash stamping solution
 *
 */

#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/ip.h>
#include <linux/oom.h>
#include <net/ip.h>

#include <linux/ipv6.h>
#include <linux/netdevice.h>
#include <linux/percpu-defs.h>
#include "rmnet_shs.h"
#include "rmnet_shs_config.h"
#include "rmnet_shs_wq.h"

/* Local Macros */
#define RMNET_SHS_FORCE_FLUSH_TIME_NSEC 2000000
#define NS_IN_MS 1000000
#define LPWR_CLUSTER 0
#define PERF_CLUSTER 4
#define DEF_CORE_WAIT 10

#define PERF_CORES 4

#define INVALID_CPU -1

#define WQ_DELAY 2000000
#define MIN_MS 5
#define BACKLOG_CHECK 1

#define GET_PQUEUE(CPU) (per_cpu(softnet_data, CPU).input_pkt_queue)
#define GET_IQUEUE(CPU) (per_cpu(softnet_data, CPU).process_queue)
#define GET_QTAIL(SD, CPU) (per_cpu(SD, CPU).input_queue_tail)
#define GET_QHEAD(SD, CPU) (per_cpu(SD, CPU).input_queue_head)
#define GET_CTIMER(CPU) rmnet_shs_cfg.core_flush[CPU].core_timer

/* Specific CPU RMNET runs on */
#define RMNET_CPU 1
#define SKB_FLUSH 0
#define INCREMENT 1
#define DECREMENT 0
/* Local Definitions and Declarations */
unsigned int rmnet_oom_pkt_limit __read_mostly = 5000;
module_param(rmnet_oom_pkt_limit, uint, 0644);
MODULE_PARM_DESC(rmnet_oom_pkt_limit, "Max rmnet pre-backlog");

DEFINE_SPINLOCK(rmnet_shs_ht_splock);
DEFINE_HASHTABLE(RMNET_SHS_HT, RMNET_SHS_HT_SIZE);
struct rmnet_shs_cpu_node_s rmnet_shs_cpu_node_tbl[MAX_CPUS];
int cpu_num_flows[MAX_CPUS];

/* Maintains a list of flows associated with a core
 * Also keeps track of number of packets processed on that core
 */

struct rmnet_shs_cfg_s rmnet_shs_cfg;
/* This flag is set to true after a successful SHS module init*/

struct rmnet_shs_flush_work shs_rx_work;
/* Delayed workqueue that will be used to flush parked packets*/
unsigned long rmnet_shs_switch_reason[RMNET_SHS_SWITCH_MAX_REASON];
module_param_array(rmnet_shs_switch_reason, ulong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_switch_reason, "rmnet shs skb core swtich type");

unsigned long rmnet_shs_flush_reason[RMNET_SHS_FLUSH_MAX_REASON];
module_param_array(rmnet_shs_flush_reason, ulong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_flush_reason, "rmnet shs skb flush trigger type");

unsigned int rmnet_shs_byte_store_limit __read_mostly = 271800 * 80;
module_param(rmnet_shs_byte_store_limit, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_byte_store_limit, "Maximum byte module will park");


unsigned int rmnet_shs_in_count = 0;
module_param(rmnet_shs_in_count, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_in_count, "SKb in count");

unsigned int rmnet_shs_out_count = 0;
module_param(rmnet_shs_out_count, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_out_count, "SKb out count");

unsigned int rmnet_shs_wq_fb_limit = 10;
module_param(rmnet_shs_wq_fb_limit, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_wq_fb_limit, "Final fb timer");


unsigned int rmnet_shs_pkts_store_limit __read_mostly = 2100 * 8;
module_param(rmnet_shs_pkts_store_limit, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_pkts_store_limit, "Maximum pkts module will park");

unsigned int rmnet_shs_max_core_wait __read_mostly = 45;
module_param(rmnet_shs_max_core_wait, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_max_core_wait,
		 "Max wait module will wait during move to perf core in ms");

unsigned int rmnet_shs_inst_rate_interval __read_mostly = 20;
module_param(rmnet_shs_inst_rate_interval, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_inst_rate_interval,
		 "Max interval we sample for instant burst prioritizing");

unsigned int rmnet_shs_inst_rate_switch __read_mostly = 1;
module_param(rmnet_shs_inst_rate_switch, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_inst_rate_switch,
		 "Configurable option to enable rx rate cpu switching");

unsigned int rmnet_shs_fall_back_timer __read_mostly = 1;
module_param(rmnet_shs_fall_back_timer, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_fall_back_timer,
		 "Option to enable fall back limit for parking");

unsigned int rmnet_shs_backlog_max_pkts __read_mostly = 1100;
module_param(rmnet_shs_backlog_max_pkts, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_backlog_max_pkts,
		 "Max pkts in backlog prioritizing");

unsigned int rmnet_shs_inst_rate_max_pkts __read_mostly = 2500;
module_param(rmnet_shs_inst_rate_max_pkts, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_inst_rate_max_pkts,
		 "Max pkts in a instant burst interval before prioritizing");

unsigned int rmnet_shs_timeout __read_mostly = 6;
module_param(rmnet_shs_timeout, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_timeout, "Option to configure fall back duration");

unsigned int rmnet_shs_switch_cores __read_mostly = 1;
module_param(rmnet_shs_switch_cores, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_switch_cores, "Switch core upon hitting threshold");

unsigned int rmnet_shs_cpu_max_qdiff[MAX_CPUS];
module_param_array(rmnet_shs_cpu_max_qdiff, uint, 0, 0644);
MODULE_PARM_DESC(rmnet_shs_cpu_max_qdiff, "Max queue length seen of each core");

unsigned int rmnet_shs_cpu_ooo_count[MAX_CPUS];
module_param_array(rmnet_shs_cpu_ooo_count, uint, 0, 0644);
MODULE_PARM_DESC(rmnet_shs_cpu_ooo_count, "OOO count for each cpu");

unsigned int rmnet_shs_cpu_max_coresum[MAX_CPUS];
module_param_array(rmnet_shs_cpu_max_coresum, uint, 0, 0644);
MODULE_PARM_DESC(rmnet_shs_cpu_max_coresum, "Max coresum seen of each core");

static void rmnet_shs_change_cpu_num_flows(u16 map_cpu, bool inc)
{
	if (map_cpu < MAX_CPUS)
		(inc) ? cpu_num_flows[map_cpu]++: cpu_num_flows[map_cpu]--;
	else
		rmnet_shs_crit_err[RMNET_SHS_CPU_FLOWS_BNDS_ERR]++;
}

void rmnet_shs_cpu_node_remove(struct rmnet_shs_skbn_s *node)
{
	SHS_TRACE_LOW(RMNET_SHS_CPU_NODE, RMNET_SHS_CPU_NODE_FUNC_REMOVE,
			    0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);

	list_del_init(&node->node_id);
	rmnet_shs_change_cpu_num_flows(node->map_cpu, DECREMENT);

}

void rmnet_shs_cpu_node_add(struct rmnet_shs_skbn_s *node,
			    struct list_head *hd)
{
	SHS_TRACE_LOW(RMNET_SHS_CPU_NODE, RMNET_SHS_CPU_NODE_FUNC_ADD,
			    0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);

	list_add(&node->node_id, hd);
	rmnet_shs_change_cpu_num_flows(node->map_cpu, INCREMENT);
}

void rmnet_shs_cpu_node_move(struct rmnet_shs_skbn_s *node,
			     struct list_head *hd, int oldcpu)
{
	SHS_TRACE_LOW(RMNET_SHS_CPU_NODE, RMNET_SHS_CPU_NODE_FUNC_MOVE,
			    0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);

	list_move(&node->node_id, hd);
	rmnet_shs_change_cpu_num_flows(node->map_cpu, INCREMENT);
	rmnet_shs_change_cpu_num_flows((u16) oldcpu, DECREMENT);
}

static void rmnet_shs_cpu_ooo(u8 cpu, int count)
{
	if (cpu < MAX_CPUS)
	{
		rmnet_shs_cpu_ooo_count[cpu]+=count;
	}
}
/* Evaluates the incoming transport protocol of the incoming skb. Determines
 * if the skb transport protocol will be supported by SHS module
 */
int rmnet_shs_is_skb_stamping_reqd(struct sk_buff *skb)
{
	int ret_val = 0;
	struct iphdr *ip4h;
	struct ipv6hdr *ip6h;

	/* This only applies to linear SKBs */
	if (!skb_is_nonlinear(skb)) {
		/* SHS will ignore ICMP and frag pkts completely */
		switch (skb->protocol) {
		case htons(ETH_P_IP):
			if (!ip_is_fragment(ip_hdr(skb)) &&
			((ip_hdr(skb)->protocol == IPPROTO_TCP) ||
			(ip_hdr(skb)->protocol == IPPROTO_UDP))){
				ret_val =  1;
				break;
			}
			/* RPS logic is skipped if RPS hash is 0 while sw_hash
			 * is set as active and packet is processed on the same
			 * CPU as the initial caller.
			 */
			if (ip_hdr(skb)->protocol == IPPROTO_ICMP) {
			    skb->hash = 0;
			    skb->sw_hash = 1;
			}
			break;

		case htons(ETH_P_IPV6):
			if (!(ipv6_hdr(skb)->nexthdr == NEXTHDR_FRAGMENT) &&
			((ipv6_hdr(skb)->nexthdr == IPPROTO_TCP) ||
			(ipv6_hdr(skb)->nexthdr == IPPROTO_UDP))) {
				ret_val =  1;
				break;
			}

			/* RPS logic is skipped if RPS hash is 0 while sw_hash
			 * is set as active and packet is processed on the same
			 * CPU as the initial caller.
			 */
			if (ipv6_hdr(skb)->nexthdr == IPPROTO_ICMP) {
			    skb->hash = 0;
			    skb->sw_hash = 1;
			}

			break;

		default:
			break;
		}
	} else {
		switch (skb->protocol) {
		case htons(ETH_P_IP):
			ip4h = (struct iphdr *)rmnet_map_data_ptr(skb);

			if (!(ntohs(ip4h->frag_off) & IP_MF) &&
			    ((ntohs(ip4h->frag_off) & IP_OFFSET) == 0) &&
			    (ip4h->protocol == IPPROTO_TCP ||
			     ip4h->protocol == IPPROTO_UDP)) {
				ret_val =  1;
				break;
			}
			/* RPS logic is skipped if RPS hash is 0 while sw_hash
			 * is set as active and packet is processed on the same
			 * CPU as the initial caller.
			 */
			if (ip4h->protocol == IPPROTO_ICMP) {
			    skb->hash = 0;
			    skb->sw_hash = 1;
			}

			break;

		case htons(ETH_P_IPV6):
			ip6h = (struct ipv6hdr *)rmnet_map_data_ptr(skb);

			if (!(ip6h->nexthdr == NEXTHDR_FRAGMENT) &&
			((ip6h->nexthdr == IPPROTO_TCP) ||
			(ip6h->nexthdr == IPPROTO_UDP))) {
				ret_val =  1;
				break;
			}
			/* RPS logic is skipped if RPS hash is 0 while sw_hash
			 * is set as active and packet is processed on the same
			 * CPU as the initial caller.
			 */
			if (ip6h->nexthdr == IPPROTO_ICMP) {
			    skb->hash = 0;
			    skb->sw_hash = 1;
			}

			break;

		default:
			break;
		}


	}
	SHS_TRACE_LOW(RMNET_SHS_SKB_STAMPING, RMNET_SHS_SKB_STAMPING_END,
			    ret_val, 0xDEF, 0xDEF, 0xDEF, skb, NULL);

	return ret_val;
}

static void rmnet_shs_update_core_load(int cpu, int burst)
{

	struct  timespec time1;
	struct  timespec *time2;
	long curinterval;
	int maxinterval = (rmnet_shs_inst_rate_interval < MIN_MS) ? MIN_MS :
			   rmnet_shs_inst_rate_interval;

	getnstimeofday(&time1);
	time2 = &rmnet_shs_cfg.core_flush[cpu].coretime;

	curinterval = RMNET_SHS_SEC_TO_NSEC(time1.tv_sec - time2->tv_sec)  +
		   time1.tv_nsec - time2->tv_nsec;

	if (curinterval >= maxinterval * NS_IN_MS) {
		if (rmnet_shs_cfg.core_flush[cpu].coresum >
			rmnet_shs_cpu_max_coresum[cpu])
			rmnet_shs_cpu_max_coresum[cpu] = rmnet_shs_cfg.core_flush[cpu].coresum;

		rmnet_shs_cfg.core_flush[cpu].coretime.tv_sec = time1.tv_sec;
		rmnet_shs_cfg.core_flush[cpu].coretime.tv_nsec = time1.tv_nsec;
		rmnet_shs_cfg.core_flush[cpu].coresum = burst;

	} else {
		rmnet_shs_cfg.core_flush[cpu].coresum += burst;
	}

}

/* We deliver packets to GRO module only for TCP traffic*/
static int rmnet_shs_check_skb_can_gro(struct sk_buff *skb)
{
	int ret_val = 0;
	struct iphdr *ip4h;
	struct ipv6hdr *ip6h;

	if (!skb_is_nonlinear(skb)) {
		switch (skb->protocol) {
		case htons(ETH_P_IP):
			if (ip_hdr(skb)->protocol == IPPROTO_TCP)
				ret_val = 1;
			break;

		case htons(ETH_P_IPV6):
			if (ipv6_hdr(skb)->nexthdr == IPPROTO_TCP)
				ret_val = 1;
			break;
		default:
			break;
		}
	} else {
		switch (skb->protocol) {
		case htons(ETH_P_IP):
			ip4h = (struct iphdr *)rmnet_map_data_ptr(skb);
			if (ip4h->protocol == IPPROTO_TCP)
				ret_val = 1;
			break;
		case htons(ETH_P_IPV6):
			ip6h = (struct ipv6hdr *)rmnet_map_data_ptr(skb);
			if (ip6h->nexthdr == IPPROTO_TCP)
				ret_val = 1;
			break;
		default:
			break;
		}
	}

	SHS_TRACE_LOW(RMNET_SHS_SKB_CAN_GRO, RMNET_SHS_SKB_CAN_GRO_END,
			    ret_val, 0xDEF, 0xDEF, 0xDEF, skb, NULL);

	return ret_val;
}

/* Delivers skb's to the next module */
static void rmnet_shs_deliver_skb(struct sk_buff *skb)
{
	struct rmnet_priv *priv;
	struct napi_struct *napi;

	rmnet_shs_out_count++;

	SHS_TRACE_LOW(RMNET_SHS_DELIVER_SKB, RMNET_SHS_DELIVER_SKB_START,
			    0xDEF, 0xDEF, 0xDEF, 0xDEF, skb, NULL);

	if (rmnet_shs_check_skb_can_gro(skb)) {
		napi = get_current_napi_context();
		if (napi) {
			napi_gro_receive(napi, skb);
		} else {
			priv = netdev_priv(skb->dev);
			gro_cells_receive(&priv->gro_cells, skb);
		}
	} else {
		netif_receive_skb(skb);
	}
}

static void rmnet_shs_deliver_skb_wq(struct sk_buff *skb)
{
	struct rmnet_priv *priv;

	SHS_TRACE_LOW(RMNET_SHS_DELIVER_SKB, RMNET_SHS_DELIVER_SKB_START,
			    0xDEF, 0xDEF, 0xDEF, 0xDEF, skb, NULL);
	rmnet_shs_out_count++;

	priv = netdev_priv(skb->dev);
	gro_cells_receive(&priv->gro_cells, skb);
}

static struct sk_buff *rmnet_shs_skb_partial_segment(struct sk_buff *skb,
						     u16 segments_per_skb)
{
	struct skb_shared_info *shinfo = skb_shinfo(skb);
	struct sk_buff *segments, *tmp;
	u16 gso_size = shinfo->gso_size;
	u16 gso_segs = shinfo->gso_segs;
	unsigned int gso_type = shinfo->gso_type;

	if (segments_per_skb >= gso_segs) {
		return NULL;
	}

	/* Update the numbers for the main skb */
	shinfo->gso_segs = DIV_ROUND_UP(gso_segs, segments_per_skb);
	shinfo->gso_size = gso_size * segments_per_skb;
	segments = __skb_gso_segment(skb, NETIF_F_SG, false);
	if (unlikely(IS_ERR_OR_NULL(segments))) {
		/* return to the original state */
		shinfo->gso_size = gso_size;
		shinfo->gso_segs = gso_segs;
		return NULL;
	}

	/* No need to set gso info if single segments */
	if (segments_per_skb <= 1)
		return segments;

	/* Mark correct number of segments, size, and type in the new skbs */
	for (tmp = segments; tmp; tmp = tmp->next) {
		struct skb_shared_info *new_shinfo = skb_shinfo(tmp);

		new_shinfo->gso_type = gso_type;
		new_shinfo->gso_size = gso_size;

		if (gso_segs >= segments_per_skb)
			new_shinfo->gso_segs = segments_per_skb;
		else
			new_shinfo->gso_segs = gso_segs;

		gso_segs -= segments_per_skb;

		if (gso_segs <= 1) {
			break;
		}
	}

	return segments;
}

/* Delivers skbs after segmenting, directly to network stack */
static void rmnet_shs_deliver_skb_segmented(struct sk_buff *in_skb,
					    u8 ctext,
					    u16 segs_per_skb)
{
	struct sk_buff *skb = NULL;
	struct sk_buff *nxt_skb = NULL;
	struct sk_buff *segs = NULL;
	int count = 0;

	SHS_TRACE_LOW(RMNET_SHS_DELIVER_SKB, RMNET_SHS_DELIVER_SKB_START,
			    0x1, 0xDEF, 0xDEF, 0xDEF, in_skb, NULL);

	rmnet_shs_out_count++;
	segs = rmnet_shs_skb_partial_segment(in_skb, segs_per_skb);

	if (segs == NULL) {
		if (ctext == RMNET_RX_CTXT)
			netif_receive_skb(in_skb);
		else
			netif_rx(in_skb);

		return;
	}

	/* Send segmented skb */
	for ((skb = segs); skb != NULL; skb = nxt_skb) {
		nxt_skb = skb->next;

		skb->hash = in_skb->hash;
		skb->dev = in_skb->dev;
		skb->next = NULL;

		if (ctext == RMNET_RX_CTXT)
			netif_receive_skb(skb);
		else
			netif_rx(skb);

		count += 1;
	}

	consume_skb(in_skb);

	return;
}

int rmnet_shs_flow_num_perf_cores(struct rmnet_shs_skbn_s *node_p)
{
	int ret = 0;
	int core = 1;
	u16 idx = 0;

	for (idx = 0; idx < MAX_CPUS; idx++) {
		if (node_p->hstats->pri_core_msk & core)
			ret++;
		core = core << 1;
	}
	return ret;
}

inline int rmnet_shs_is_lpwr_cpu(u16 cpu)
{
	return !((1 << cpu) & PERF_MASK);
}

/* Forms a new hash from the incoming hash based on the number of cores
 * available for processing. This new hash will be stamped by
 * SHS module (for all the packets arriving with same incoming hash)
 * before delivering them to next layer.
 */
u32 rmnet_shs_form_hash(u32 index, u32 maplen, u32 hash)
{
	int offsetmap[MAX_CPUS / 2] = {8, 4, 3, 2};
	u32 ret = 0;

	if (!maplen) {
		rmnet_shs_crit_err[RMNET_SHS_MAIN_MAP_LEN_INVALID]++;
		return ret;
	}

	/* Override MSB of skb hash to steer. Save most of Hash bits
	 * Leave some as 0 to allow for easy debugging.
	 */
	if (maplen < MAX_CPUS)
		ret = ((((index + ((maplen % 2) ? 1 : 0))) << 28)
			* offsetmap[(maplen - 1) >> 1]) | (hash & 0x0FFFFF);

	SHS_TRACE_LOW(RMNET_SHS_HASH_MAP, RMNET_SHS_HASH_MAP_FORM_HASH,
			    ret, hash, index, maplen, NULL, NULL);

	return ret;
}

u8 rmnet_shs_mask_from_map(struct rps_map *map)
{
	u8 mask = 0;
	u8 i;

	for (i = 0; i < map->len; i++)
		mask |= 1 << map->cpus[i];

	return mask;
}

int rmnet_shs_get_mask_len(u8 mask)
{
	u8 i;
	u8 sum = 0;

	for (i = 0; i < MAX_CPUS; i++) {
		if (mask & (1 << i))
			sum++;
	}
	return sum;
}

int rmnet_shs_get_core_prio_flow(u8 mask)
{
	int ret = INVALID_CPU;
	int least_flows = INVALID_CPU;
	u8 curr_idx = 0;
	u8 i;

	/* Return 1st free core or the core with least # flows
	 */
	for (i = 0; i < MAX_CPUS; i++) {

		if (!(mask & (1 << i)))
			continue;

		if (mask & (1 << i))
			curr_idx++;

		if (list_empty(&rmnet_shs_cpu_node_tbl[i].node_list_id))
			return i;

		if (cpu_num_flows[i] <= least_flows ||
		    least_flows == INVALID_CPU) {
			ret = i;
			least_flows = cpu_num_flows[i];
		}

	}

	return ret;
}

/* Take a index and a mask and returns what active CPU is
 * in that index.
 */
int rmnet_shs_cpu_from_idx(u8 index, u8 mask)
{
	int ret = INVALID_CPU;
	u8 curr_idx = 0;
	u8 i;

	for (i = 0; i < MAX_CPUS; i++) {
		/* If core is enabled & is the index'th core
		 * return that CPU
		 */
		if (curr_idx == index && (mask & (1 << i)))
			return i;

		if (mask & (1 << i))
			curr_idx++;
	}
	return ret;
}

/* Takes a CPU and a CPU mask and computes what index of configured
 * the CPU is in. Returns INVALID_CPU if CPU is not enabled in the mask.
 */
int rmnet_shs_idx_from_cpu(u8 cpu, u8 mask)
{
	int ret = INVALID_CPU;
	u8 idx = 0;
	u8 i;

	/* If not in mask return invalid*/
	if (!(mask & 1 << cpu))
		return ret;

	/* Find idx by counting all other configed CPUs*/
	for (i = 0; i < MAX_CPUS; i++) {
		if (i == cpu  && (mask & (1 << i))) {
			ret = idx;
			break;
		}
		if (mask & (1 << i))
			idx++;
	}
	return ret;
}

/* Assigns a CPU to process packets corresponding to new flow. For flow with
 * small incoming burst a low power core handling least number of packets
 * per second will be assigned.
 *
 * For a flow with a heavy incoming burst, a performance core with the least
 * number of packets processed per second  will be assigned
 *
 * If two or more cores within a cluster are handling the same number of
 * packets per second, the first match will be assigned.
 */
int rmnet_shs_new_flow_cpu(u64 burst_size, struct net_device *dev)
{
	int flow_cpu = INVALID_CPU;

	if (burst_size < RMNET_SHS_MAX_SILVER_CORE_BURST_CAPACITY)
		flow_cpu = rmnet_shs_wq_get_lpwr_cpu_new_flow(dev);
	if (flow_cpu == INVALID_CPU ||
	    burst_size >= RMNET_SHS_MAX_SILVER_CORE_BURST_CAPACITY)
		flow_cpu = rmnet_shs_wq_get_perf_cpu_new_flow(dev);

	SHS_TRACE_HIGH(RMNET_SHS_ASSIGN,
			     RMNET_SHS_ASSIGN_GET_NEW_FLOW_CPU,
			     flow_cpu, burst_size, 0xDEF, 0xDEF, NULL, NULL);

	return flow_cpu;
}

int rmnet_shs_get_suggested_cpu(struct rmnet_shs_skbn_s *node)
{
	int cpu = INVALID_CPU;

	/* Return same perf core unless moving to gold from silver*/
	if (rmnet_shs_cpu_node_tbl[node->map_cpu].prio &&
	    rmnet_shs_is_lpwr_cpu(node->map_cpu)) {
		cpu = rmnet_shs_get_core_prio_flow(PERF_MASK &
						   rmnet_shs_cfg.map_mask);
		if (cpu < 0 && node->hstats != NULL)
			cpu = node->hstats->suggested_cpu;
	} else if (node->hstats != NULL)
		cpu = node->hstats->suggested_cpu;

	return cpu;
}

int rmnet_shs_get_hash_map_idx_to_stamp(struct rmnet_shs_skbn_s *node)
{
	int cpu, idx = INVALID_CPU;

	cpu = rmnet_shs_get_suggested_cpu(node);
	idx = rmnet_shs_idx_from_cpu(cpu, rmnet_shs_cfg.map_mask);

	/* If suggested CPU is no longer in mask. Try using current.*/
	if (unlikely(idx < 0))
		idx = rmnet_shs_idx_from_cpu(node->map_cpu,
					     rmnet_shs_cfg.map_mask);

	SHS_TRACE_LOW(RMNET_SHS_HASH_MAP,
			    RMNET_SHS_HASH_MAP_IDX_TO_STAMP,
			    node->hash, cpu, idx, 0xDEF, node, NULL);
	return idx;
}

u32 rmnet_shs_get_cpu_qhead(u8 cpu_num)
{
	u32 ret = 0;

	if (cpu_num < MAX_CPUS)
		ret = rmnet_shs_cpu_node_tbl[cpu_num].qhead;

	SHS_TRACE_LOW(RMNET_SHS_CORE_CFG, RMNET_SHS_CORE_CFG_GET_QHEAD,
			    cpu_num, ret, 0xDEF, 0xDEF, NULL, NULL);
	return ret;
}

u32 rmnet_shs_get_cpu_qtail(u8 cpu_num)
{
	u32 ret = 0;

	if (cpu_num < MAX_CPUS)
		ret =  rmnet_shs_cpu_node_tbl[cpu_num].qtail;

	SHS_TRACE_LOW(RMNET_SHS_CORE_CFG, RMNET_SHS_CORE_CFG_GET_QTAIL,
			    cpu_num, ret, 0xDEF, 0xDEF, NULL, NULL);

	return ret;
}

u32 rmnet_shs_get_cpu_qdiff(u8 cpu_num)
{
	u32 ret = 0;

	if (cpu_num < MAX_CPUS)
		ret =  rmnet_shs_cpu_node_tbl[cpu_num].qdiff;

	SHS_TRACE_LOW(RMNET_SHS_CORE_CFG, RMNET_SHS_CORE_CFG_GET_QTAIL,
			    cpu_num, ret, 0xDEF, 0xDEF, NULL, NULL);

	return ret;
}

static int rmnet_shs_is_core_loaded(int cpu, int backlog_check, int parked_pkts)
{
	int ret = 0;

	if (rmnet_shs_cfg.core_flush[cpu].coresum >=
            rmnet_shs_inst_rate_max_pkts) {
		ret = RMNET_SHS_SWITCH_PACKET_BURST;
	}
	if (backlog_check && ((rmnet_shs_get_cpu_qdiff(cpu) + parked_pkts) >=
	    rmnet_shs_backlog_max_pkts))
		ret = RMNET_SHS_SWITCH_CORE_BACKLOG;

	return ret;
}

/* Takes a snapshot of absolute value of the CPU Qhead and Qtail counts for
 * a given core.
 *
 * CPU qhead reports the count of number of packets processed on a core
 * CPU qtail keeps track of total number of pkts on a core
 * qtail - qhead = pkts yet to be processed by next layer
 */
void rmnet_shs_update_cpu_proc_q(u8 cpu_num)
{
	if (cpu_num >= MAX_CPUS)
		return;

	rcu_read_lock();
	rmnet_shs_cpu_node_tbl[cpu_num].qhead =
	   GET_QHEAD(softnet_data, cpu_num);
	rmnet_shs_cpu_node_tbl[cpu_num].qtail =
	   GET_QTAIL(softnet_data, cpu_num);
	rcu_read_unlock();

	rmnet_shs_cpu_node_tbl[cpu_num].qdiff =
	rmnet_shs_cpu_node_tbl[cpu_num].qtail -
	rmnet_shs_cpu_node_tbl[cpu_num].qhead;

	SHS_TRACE_LOW(RMNET_SHS_CORE_CFG,
			    RMNET_SHS_CORE_CFG_GET_CPU_PROC_PARAMS,
			    cpu_num, rmnet_shs_cpu_node_tbl[cpu_num].qhead,
			    rmnet_shs_cpu_node_tbl[cpu_num].qtail,
			    0xDEF, NULL, NULL);
}

/* Takes a snapshot of absolute value of the CPU Qhead and Qtail counts for
 * all cores.
 *
 * CPU qhead reports the count of number of packets processed on a core
 * CPU qtail keeps track of total number of pkts on a core
 * qtail - qhead = pkts yet to be processed by next layer
 */
void rmnet_shs_update_cpu_proc_q_all_cpus(void)
{
	u8 cpu_num;

	rcu_read_lock();
	for (cpu_num = 0; cpu_num < MAX_CPUS; cpu_num++) {
		rmnet_shs_update_cpu_proc_q(cpu_num);

		SHS_TRACE_LOW(RMNET_SHS_CORE_CFG,
				    RMNET_SHS_CORE_CFG_GET_CPU_PROC_PARAMS,
				    cpu_num,
				    rmnet_shs_cpu_node_tbl[cpu_num].qhead,
				    rmnet_shs_cpu_node_tbl[cpu_num].qtail,
				    0xDEF, NULL, NULL);
	}
	rcu_read_unlock();

}
int rmnet_shs_node_can_flush_pkts(struct rmnet_shs_skbn_s *node, u8 force_flush)
{
	int cpu_map_index;
	u32 cur_cpu_qhead;
	u32 node_qhead;
	int ret = 0;
	int prev_cpu = -1;
	int ccpu;
	int cpu_num;
	int new_cpu;
	struct rmnet_shs_cpu_node_s *cpun;
	u8 map = rmnet_shs_cfg.map_mask;
	u32 old_cpu_qlen;

	cpu_map_index = rmnet_shs_get_hash_map_idx_to_stamp(node);
	do {
		prev_cpu = node->map_cpu;
		if (cpu_map_index < 0) {
			node->is_shs_enabled = 0;
			ret = 1;
			break;
		}
		node->is_shs_enabled = 1;
		if (!map) {
			node->is_shs_enabled = 0;
			ret = 1;
			break;
		}

		/* If the flow is going to the same core itself
		 */
		if (cpu_map_index == node->map_index) {
			ret = 1;
			break;
		}

		cur_cpu_qhead = rmnet_shs_get_cpu_qhead(node->map_cpu);
		node_qhead = node->queue_head;
		cpu_num = node->map_cpu;
		old_cpu_qlen = GET_PQUEUE(cpu_num).qlen + GET_IQUEUE(cpu_num).qlen;

		if ((cur_cpu_qhead >= node_qhead) || force_flush || (!old_cpu_qlen && ++rmnet_shs_flush_reason[RMNET_SHS_FLUSH_Z_QUEUE_FLUSH])) {
			if (rmnet_shs_switch_cores) {

				/* Move the amount parked to other core's count
				 * Update old core's parked to not include diverted
				 * packets and update new core's packets
				 */
				new_cpu = rmnet_shs_cpu_from_idx(cpu_map_index,
								 rmnet_shs_cfg.map_mask);
				if (new_cpu < 0) {
					ret = 1;
					break;
				}
				rmnet_shs_cpu_node_tbl[new_cpu].parkedlen += node->skb_list.num_parked_skbs;
				rmnet_shs_cpu_node_tbl[node->map_cpu].parkedlen -= node->skb_list.num_parked_skbs;
				node->map_index = cpu_map_index;
				node->map_cpu = new_cpu;
				ccpu = node->map_cpu;

				if (cur_cpu_qhead < node_qhead) {
					rmnet_shs_switch_reason[RMNET_SHS_OOO_PACKET_SWITCH]++;
					rmnet_shs_switch_reason[RMNET_SHS_OOO_PACKET_TOTAL] +=
							(node_qhead -
							cur_cpu_qhead);
					rmnet_shs_cpu_ooo(cpu_num, node_qhead - cur_cpu_qhead);
				}
				/* Mark gold core as prio to prevent
				 * flows from moving in wq
				 */
				if (rmnet_shs_cpu_node_tbl[cpu_num].prio) {
					node->hstats->suggested_cpu = ccpu;
					rmnet_shs_cpu_node_tbl[ccpu].wqprio = 1;
					rmnet_shs_switch_reason[RMNET_SHS_SWITCH_INSTANT_RATE]++;

				} else {

					rmnet_shs_switch_reason[RMNET_SHS_SWITCH_WQ_RATE]++;

				}
				cpun = &rmnet_shs_cpu_node_tbl[node->map_cpu];
				rmnet_shs_update_cpu_proc_q_all_cpus();
				node->queue_head = cpun->qhead;

				rmnet_shs_cpu_node_move(node,
							&cpun->node_list_id,
							cpu_num);
				SHS_TRACE_HIGH(RMNET_SHS_FLUSH,
					RMNET_SHS_FLUSH_NODE_CORE_SWITCH,
					node->map_cpu, prev_cpu,
					0xDEF, 0xDEF, node, NULL);
			}
			ret = 1;
		}
	} while (0);

	SHS_TRACE_LOW(RMNET_SHS_FLUSH,
			    RMNET_SHS_FLUSH_CHK_NODE_CAN_FLUSH,
			    ret, node->map_cpu, prev_cpu,
			    0xDEF, node, NULL);
	return ret;
}

void rmnet_shs_flush_core(u8 cpu_num)
{
	struct rmnet_shs_skbn_s *n;
	struct list_head *ptr, *next;
	unsigned long ht_flags;
	u32 cpu_tail;
	u32 num_pkts_flush = 0;
	u32 num_bytes_flush = 0;
	u32 total_pkts_flush = 0;
	u32 total_bytes_flush = 0;

	/* Record a qtail + pkts flushed or move if reqd
	 * currently only use qtail for non TCP flows
	 */
	rmnet_shs_update_cpu_proc_q_all_cpus();
	SHS_TRACE_HIGH(RMNET_SHS_FLUSH, RMNET_SHS_FLUSH_START,
			     rmnet_shs_cfg.num_pkts_parked,
			     rmnet_shs_cfg.num_bytes_parked,
			     0xDEF, 0xDEF, NULL, NULL);
	local_bh_disable();
	spin_lock_irqsave(&rmnet_shs_ht_splock, ht_flags);
		cpu_tail = rmnet_shs_get_cpu_qtail(cpu_num);
		list_for_each_safe(ptr, next,
			&rmnet_shs_cpu_node_tbl[cpu_num].node_list_id) {
			n = list_entry(ptr, struct rmnet_shs_skbn_s, node_id);
			if (n != NULL && n->skb_list.num_parked_skbs) {
				num_pkts_flush = n->skb_list.num_parked_skbs;
				num_bytes_flush = n->skb_list.num_parked_bytes;

				rmnet_shs_chk_and_flush_node(n, 1,
							RMNET_WQ_CTXT);

				total_pkts_flush += num_pkts_flush;
				total_bytes_flush += num_bytes_flush;
				if (n->map_cpu == cpu_num) {
					cpu_tail += num_pkts_flush;
					n->queue_head = cpu_tail;

				}

			}
		}

	rmnet_shs_cfg.num_bytes_parked -= total_bytes_flush;
	rmnet_shs_cfg.num_pkts_parked -= total_pkts_flush;
	rmnet_shs_cpu_node_tbl[cpu_num].prio = 0;
	/* Reset coresum in case of instant rate switch */
	rmnet_shs_cfg.core_flush[cpu_num].coresum = 0;
	rmnet_shs_cpu_node_tbl[cpu_num].parkedlen = 0;
	spin_unlock_irqrestore(&rmnet_shs_ht_splock, ht_flags);
	local_bh_enable();

	SHS_TRACE_HIGH(RMNET_SHS_FLUSH, RMNET_SHS_FLUSH_END,
	     rmnet_shs_cfg.num_pkts_parked,
			     rmnet_shs_cfg.num_bytes_parked,
			     total_pkts_flush, total_bytes_flush, NULL, NULL);

}

static void rmnet_shs_flush_core_work(struct work_struct *work)
{
	struct core_flush_s *core_work = container_of(work,
				 struct core_flush_s, work);

	rmnet_shs_flush_core(core_work->core);
	rmnet_shs_flush_reason[RMNET_SHS_FLUSH_WQ_CORE_FLUSH]++;
}

/* Flushes all the packets parked in order for this flow */
void rmnet_shs_flush_node(struct rmnet_shs_skbn_s *node, u8 ctext)
{
	struct sk_buff *skb = NULL;
	struct sk_buff *nxt_skb = NULL;
	u32 skbs_delivered = 0;
	u32 skb_bytes_delivered = 0;
	u32 hash2stamp = 0; /* the default value of skb->hash*/
	u8 map = 0, maplen = 0;
	u16 segs_per_skb = 0;

	if (!node->skb_list.head)
		return;

	map = rmnet_shs_cfg.map_mask;
	maplen = rmnet_shs_cfg.map_len;

	if (map) {
		hash2stamp = rmnet_shs_form_hash(node->map_index,
						 maplen,
						 node->skb_list.head->hash);
	} else {
		node->is_shs_enabled = 0;
	}
	SHS_TRACE_HIGH(RMNET_SHS_FLUSH,
			     RMNET_SHS_FLUSH_NODE_START,
			     node->hash, hash2stamp,
			     node->skb_list.num_parked_skbs,
			     node->skb_list.num_parked_bytes,
			     node, node->skb_list.head);

	segs_per_skb = (u16) node->hstats->segs_per_skb;

	for ((skb = node->skb_list.head); skb != NULL; skb = nxt_skb) {

		nxt_skb = skb->next;
		if (node->is_shs_enabled)
			skb->hash = hash2stamp;

		skb->next = NULL;
		skbs_delivered += 1;
		skb_bytes_delivered += skb->len;

		if (segs_per_skb > 0) {
			if (node->skb_tport_proto == IPPROTO_UDP)
				rmnet_shs_crit_err[RMNET_SHS_UDP_SEGMENT]++;
			rmnet_shs_deliver_skb_segmented(skb, ctext,
							segs_per_skb);
		} else {
			if (ctext == RMNET_RX_CTXT)
				rmnet_shs_deliver_skb(skb);
			else
				rmnet_shs_deliver_skb_wq(skb);
		}
	}

	node->skb_list.num_parked_skbs = 0;
	node->skb_list.num_parked_bytes = 0;
	node->skb_list.head = NULL;
	node->skb_list.tail = NULL;

	SHS_TRACE_HIGH(RMNET_SHS_FLUSH, RMNET_SHS_FLUSH_NODE_END,
			     node->hash, hash2stamp,
			     skbs_delivered, skb_bytes_delivered, node, NULL);
}

void rmnet_shs_clear_node(struct rmnet_shs_skbn_s *node, u8 ctxt)
{
	struct sk_buff *skb;
	struct sk_buff *nxt_skb = NULL;
	u32 skbs_delivered = 0;
	u32 skb_bytes_delivered = 0;
	u32 hash2stamp;
	u8 map, maplen;

	if (!node->skb_list.head)
		return;
	map = rmnet_shs_cfg.map_mask;
	maplen = rmnet_shs_cfg.map_len;

	if (map) {
		hash2stamp = rmnet_shs_form_hash(node->map_index,
						 maplen,
						 node->skb_list.head->hash);
	} else {
		node->is_shs_enabled = 0;
	}

	for ((skb = node->skb_list.head); skb != NULL; skb = nxt_skb) {
		nxt_skb = skb->next;
		if (node->is_shs_enabled)
			skb->hash = hash2stamp;

		skb->next = NULL;
		skbs_delivered += 1;
		skb_bytes_delivered += skb->len;
		if (ctxt == RMNET_RX_CTXT)
			rmnet_shs_deliver_skb(skb);
		else
			rmnet_shs_deliver_skb_wq(skb);
	}
	rmnet_shs_crit_err[RMNET_SHS_WQ_COMSUME_PKTS]++;

	rmnet_shs_cfg.num_bytes_parked -= skb_bytes_delivered;
	rmnet_shs_cfg.num_pkts_parked -= skbs_delivered;
	rmnet_shs_cpu_node_tbl[node->map_cpu].parkedlen -= skbs_delivered;
}

/* Evaluates if all the packets corresponding to a particular flow can
 * be flushed.
 */
int rmnet_shs_chk_and_flush_node(struct rmnet_shs_skbn_s *node,
				 u8 force_flush, u8 ctxt)
{
	int ret_val = 0;
	/* Shoud stay int for error reporting*/
	int map = rmnet_shs_cfg.map_mask;
	int map_idx;

	SHS_TRACE_HIGH(RMNET_SHS_FLUSH,
			     RMNET_SHS_FLUSH_CHK_AND_FLUSH_NODE_START,
			     force_flush, ctxt, 0xDEF, 0xDEF,
			     node, NULL);
	/* Return saved cpu assignment if an entry found*/
	if (rmnet_shs_cpu_from_idx(node->map_index, map) != node->map_cpu) {

		/* Keep flow on the same core if possible
		 * or put Orphaned flow on the default 1st core
		 */
		map_idx = rmnet_shs_idx_from_cpu(node->map_cpu,
							map);
		if (map_idx >= 0) {
			node->map_index = map_idx;
			node->map_cpu = rmnet_shs_cpu_from_idx(map_idx, map);

		} else {
			/*Put on default Core if no match*/
			node->map_index = MAIN_CORE;
			node->map_cpu = rmnet_shs_cpu_from_idx(MAIN_CORE, map);
			if (node->map_cpu < 0)
				node->map_cpu = MAIN_CORE;
		}
		force_flush = 1;
		rmnet_shs_crit_err[RMNET_SHS_RPS_MASK_CHANGE]++;
		SHS_TRACE_ERR(RMNET_SHS_ASSIGN,
					RMNET_SHS_ASSIGN_MASK_CHNG,
					0xDEF, 0xDEF, 0xDEF, 0xDEF,
					NULL, NULL);
	}

	if (rmnet_shs_node_can_flush_pkts(node, force_flush)) {
		rmnet_shs_flush_node(node, ctxt);
		ret_val = 1;
	}
	SHS_TRACE_HIGH(RMNET_SHS_FLUSH,
			     RMNET_SHS_FLUSH_CHK_AND_FLUSH_NODE_END,
			     ret_val, force_flush, 0xDEF, 0xDEF,
			     node, NULL);
	return ret_val;
}

/* Check if cpu_num should be marked as a priority core and  take care of
 * marking it as priority and configuring  all the changes need for a core
 * switch.
 */
static void rmnet_shs_core_prio_check(u8 cpu_num, u8 segmented, u32 parked_pkts)
{
	u32 wait = (!rmnet_shs_max_core_wait) ? 1 : rmnet_shs_max_core_wait;
	int load_reason;

	if ((load_reason = rmnet_shs_is_core_loaded(cpu_num, segmented, parked_pkts)) &&
	    rmnet_shs_is_lpwr_cpu(cpu_num) &&
	    !rmnet_shs_cpu_node_tbl[cpu_num].prio) {


		wait = (!segmented)? DEF_CORE_WAIT: wait;
		rmnet_shs_cpu_node_tbl[cpu_num].prio = 1;
		rmnet_shs_boost_cpus();
		if (hrtimer_active(&GET_CTIMER(cpu_num)))
			hrtimer_cancel(&GET_CTIMER(cpu_num));

		hrtimer_start(&GET_CTIMER(cpu_num),
				ns_to_ktime(wait * NS_IN_MS),
				HRTIMER_MODE_REL);

		rmnet_shs_switch_reason[load_reason]++;
	}
}

/* Flushes all the packets that have been parked so far across all the flows
 * The order of flushing depends on the CPU<=>flow association
 * The flows associated with low power cores are flushed before flushing
 * packets of all the flows associated with perf core.
 *
 * If more than two flows are associated with the same CPU, the packets
 * corresponding to the most recent flow will be flushed first
 *
 * Each time a flushing is invoked we also keep track of the number of
 * packets waiting & have been processed by the next layers.
 */

void rmnet_shs_flush_lock_table(u8 flsh, u8 ctxt)
{
	struct rmnet_shs_skbn_s *n = NULL;
	struct list_head *ptr = NULL, *next = NULL;
	int cpu_num;
	u32 cpu_tail;
	u32 num_pkts_flush = 0;
	u32 num_bytes_flush = 0;
	u32 skb_seg_pending = 0;
	u32 total_pkts_flush = 0;
	u32 total_bytes_flush = 0;
	u32 total_cpu_gro_flushed = 0;
	u32 total_node_gro_flushed = 0;
	u8 is_flushed = 0;

	/* Record a qtail + pkts flushed or move if reqd
	 * currently only use qtail for non TCP flows
	 */
	rmnet_shs_update_cpu_proc_q_all_cpus();
	SHS_TRACE_HIGH(RMNET_SHS_FLUSH, RMNET_SHS_FLUSH_START,
			     rmnet_shs_cfg.num_pkts_parked,
			     rmnet_shs_cfg.num_bytes_parked,
			     0xDEF, 0xDEF, NULL, NULL);

	for (cpu_num = 0; cpu_num < MAX_CPUS; cpu_num++) {

		cpu_tail = rmnet_shs_get_cpu_qtail(cpu_num);
		total_cpu_gro_flushed = 0;
		skb_seg_pending = 0;
		list_for_each_safe(ptr, next,
				   &rmnet_shs_cpu_node_tbl[cpu_num].node_list_id) {
			n = list_entry(ptr, struct rmnet_shs_skbn_s, node_id);
			skb_seg_pending += n->skb_list.skb_load;
		}
		if (rmnet_shs_inst_rate_switch) {
			rmnet_shs_core_prio_check(cpu_num, BACKLOG_CHECK,
						  skb_seg_pending);
		}

		list_for_each_safe(ptr, next,
				   &rmnet_shs_cpu_node_tbl[cpu_num].node_list_id) {
			n = list_entry(ptr, struct rmnet_shs_skbn_s, node_id);

			if (n != NULL && n->skb_list.num_parked_skbs) {
				num_pkts_flush = n->skb_list.num_parked_skbs;
				num_bytes_flush = n->skb_list.num_parked_bytes;
				total_node_gro_flushed = n->skb_list.skb_load;

				is_flushed = rmnet_shs_chk_and_flush_node(n,
									  flsh,
									  ctxt);

				if (is_flushed) {
					total_cpu_gro_flushed += total_node_gro_flushed;
					total_pkts_flush += num_pkts_flush;
					total_bytes_flush += num_bytes_flush;
					rmnet_shs_cpu_node_tbl[n->map_cpu].parkedlen -= num_pkts_flush;
					n->skb_list.skb_load = 0;
					if (n->map_cpu == cpu_num) {
						cpu_tail += num_pkts_flush;
						n->queue_head = cpu_tail;

					}
				}
			}

		}

		/* If core is loaded set core flows as priority and
		 * start a 10ms hard flush timer
		 */
		if (rmnet_shs_inst_rate_switch) {
			/* Update cpu load with prev flush for check */
			if (rmnet_shs_is_lpwr_cpu(cpu_num) &&
			    !rmnet_shs_cpu_node_tbl[cpu_num].prio)
				rmnet_shs_update_core_load(cpu_num,
				total_cpu_gro_flushed);

			rmnet_shs_core_prio_check(cpu_num, BACKLOG_CHECK, 0);

		}

		if (rmnet_shs_cpu_node_tbl[cpu_num].parkedlen < 0)
			rmnet_shs_crit_err[RMNET_SHS_CPU_PKTLEN_ERR]++;

		if (rmnet_shs_get_cpu_qdiff(cpu_num) >=
		    rmnet_shs_cpu_max_qdiff[cpu_num])
			rmnet_shs_cpu_max_qdiff[cpu_num] =
					rmnet_shs_get_cpu_qdiff(cpu_num);
		}

	rmnet_shs_cfg.num_bytes_parked -= total_bytes_flush;
	rmnet_shs_cfg.num_pkts_parked -= total_pkts_flush;

	SHS_TRACE_HIGH(RMNET_SHS_FLUSH, RMNET_SHS_FLUSH_END,
			     rmnet_shs_cfg.num_pkts_parked,
			     rmnet_shs_cfg.num_bytes_parked,
			     total_pkts_flush, total_bytes_flush, NULL, NULL);

	if ((rmnet_shs_cfg.num_bytes_parked <= 0) ||
	    (rmnet_shs_cfg.num_pkts_parked <= 0)) {
		rmnet_shs_cfg.ff_flag = 0;
		rmnet_shs_cfg.num_bytes_parked = 0;
		rmnet_shs_cfg.num_pkts_parked = 0;
		rmnet_shs_cfg.is_pkt_parked = 0;
		rmnet_shs_cfg.force_flush_state = RMNET_SHS_FLUSH_DONE;
		if (rmnet_shs_fall_back_timer) {
			if (hrtimer_active(&rmnet_shs_cfg.hrtimer_shs))
				hrtimer_cancel(&rmnet_shs_cfg.hrtimer_shs);
		}
	}
}

void rmnet_shs_flush_table(u8 flsh, u8 ctxt)
{
	unsigned long ht_flags;

	spin_lock_irqsave(&rmnet_shs_ht_splock, ht_flags);

	rmnet_shs_flush_lock_table(flsh, ctxt);
	if (ctxt == RMNET_WQ_CTXT) {
		/* If packets remain restart the timer in case there are no
		* more NET_RX flushes coming so pkts are no lost
		*/
		if (rmnet_shs_fall_back_timer &&
		rmnet_shs_cfg.num_bytes_parked &&
		rmnet_shs_cfg.num_pkts_parked){
			if (hrtimer_active(&rmnet_shs_cfg.hrtimer_shs))
				hrtimer_cancel(&rmnet_shs_cfg.hrtimer_shs);
			hrtimer_start(&rmnet_shs_cfg.hrtimer_shs,
				ns_to_ktime(rmnet_shs_timeout * NS_IN_MS),
				HRTIMER_MODE_REL);
		if (rmnet_shs_fall_back_timer &&
		    rmnet_shs_cfg.num_bytes_parked &&
		    rmnet_shs_cfg.num_pkts_parked){
				rmnet_shs_cfg.ff_flag++;
		}

		}
		rmnet_shs_flush_reason[RMNET_SHS_FLUSH_WQ_FB_FLUSH]++;
	}

	spin_unlock_irqrestore(&rmnet_shs_ht_splock, ht_flags);

}

/* After we have decided to handle the incoming skb we park them in order
 * per flow
 */
void rmnet_shs_chain_to_skb_list(struct sk_buff *skb,
				 struct rmnet_shs_skbn_s *node)
{
	u8 pushflush = 0;
	struct napi_struct *napi = get_current_napi_context();

	/* Early flush for TCP if PSH packet.
	 * Flush before parking PSH packet.
	 */
	if (skb->cb[SKB_FLUSH]) {
		rmnet_shs_flush_lock_table(0, RMNET_RX_CTXT);
		rmnet_shs_flush_reason[RMNET_SHS_FLUSH_PSH_PKT_FLUSH]++;
		napi_gro_flush(napi, false);
		pushflush = 1;
	}

	/* Support for gso marked packets */
	if (skb_shinfo(skb)->gso_segs) {
		node->num_skb += skb_shinfo(skb)->gso_segs;
		rmnet_shs_cpu_node_tbl[node->map_cpu].parkedlen++;
		node->skb_list.skb_load += skb_shinfo(skb)->gso_segs;
	} else {
		node->num_skb += 1;
		rmnet_shs_cpu_node_tbl[node->map_cpu].parkedlen++;
		node->skb_list.skb_load++;

	}

	node->num_skb_bytes += skb->len;
	node->skb_list.num_parked_bytes += skb->len;
	rmnet_shs_cfg.num_bytes_parked  += skb->len;

	if (node->skb_list.num_parked_skbs > 0) {
		node->skb_list.tail->next = skb;
		node->skb_list.tail = node->skb_list.tail->next;
	} else {
		node->skb_list.head = skb;
		node->skb_list.tail = skb;
	}

	/* skb_list.num_parked_skbs Number of packets are parked for this flow
	 */
	node->skb_list.num_parked_skbs += 1;
	rmnet_shs_cfg.num_pkts_parked  += 1;

	if (unlikely(pushflush)) {
		rmnet_shs_flush_lock_table(0, RMNET_RX_CTXT);
		napi_gro_flush(napi, false);

	}

	SHS_TRACE_HIGH(RMNET_SHS_ASSIGN,
			     RMNET_SHS_ASSIGN_PARK_PKT_COMPLETE,
			     node->skb_list.num_parked_skbs,
			     node->skb_list.num_parked_bytes,
			     rmnet_shs_cfg.num_pkts_parked,
			     rmnet_shs_cfg.num_bytes_parked,
			     skb, node);
}
/* Invoked when all the packets that are parked to be flushed through
 * the workqueue.
 */
static void rmnet_flush_buffered(struct work_struct *work)
{

	SHS_TRACE_HIGH(RMNET_SHS_FLUSH,
			     RMNET_SHS_FLUSH_DELAY_WQ_START, rmnet_shs_cfg.ff_flag,
			     rmnet_shs_cfg.force_flush_state, 0xDEF,
			     0xDEF, NULL, NULL);

	if (rmnet_shs_cfg.num_pkts_parked &&
	   rmnet_shs_cfg.force_flush_state == RMNET_SHS_FLUSH_ON) {
		local_bh_disable();
		if (rmnet_shs_cfg.ff_flag >= rmnet_shs_wq_fb_limit) {
			rmnet_shs_flush_reason[RMNET_SHS_FLUSH_WQ_FB_FF_FLUSH]++;

		}
		rmnet_shs_flush_table(rmnet_shs_cfg.ff_flag >= rmnet_shs_wq_fb_limit,
				      RMNET_WQ_CTXT);

		local_bh_enable();
	}
	SHS_TRACE_HIGH(RMNET_SHS_FLUSH,
			     RMNET_SHS_FLUSH_DELAY_WQ_END,
			     rmnet_shs_cfg.ff_flag, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);
}
/* Invoked when the flushing timer has expired.
 * Upon first expiry, we set the flag that will trigger force flushing of all
 * packets that have been parked so far. The timer is then restarted
 *
 * Upon the next expiry, if the packets haven't yet been delivered to the
 * next layer, a workqueue will be scheduled to flush all the parked packets.
 */
enum hrtimer_restart rmnet_shs_map_flush_queue(struct hrtimer *t)
{
	enum hrtimer_restart ret = HRTIMER_NORESTART;

	SHS_TRACE_HIGH(RMNET_SHS_FLUSH,
			     RMNET_SHS_FLUSH_PARK_TMR_EXPIRY,
			     rmnet_shs_cfg.force_flush_state, 0xDEF,
			     0xDEF, 0xDEF, NULL, NULL);
	if (rmnet_shs_cfg.num_pkts_parked > 0) {
		if (rmnet_shs_cfg.force_flush_state == RMNET_SHS_FLUSH_OFF) {
			rmnet_shs_cfg.force_flush_state = RMNET_SHS_FLUSH_ON;
			hrtimer_forward(t, hrtimer_cb_get_time(t),
					ns_to_ktime(WQ_DELAY));
			ret = HRTIMER_RESTART;

			SHS_TRACE_HIGH(RMNET_SHS_FLUSH,
					     RMNET_SHS_FLUSH_PARK_TMR_RESTART,
					     rmnet_shs_cfg.num_pkts_parked,
					     0xDEF, 0xDEF, 0xDEF, NULL, NULL);
		} else if (rmnet_shs_cfg.force_flush_state ==
			   RMNET_SHS_FLUSH_DONE) {
			rmnet_shs_cfg.force_flush_state = RMNET_SHS_FLUSH_OFF;

		} else if (rmnet_shs_cfg.force_flush_state ==
			   RMNET_SHS_FLUSH_ON) {
			SHS_TRACE_HIGH(RMNET_SHS_FLUSH,
					     RMNET_SHS_FLUSH_DELAY_WQ_TRIGGER,
					     rmnet_shs_cfg.force_flush_state,
					     0xDEF, 0xDEF, 0xDEF, NULL, NULL);
			schedule_work((struct work_struct *)&shs_rx_work);
		}
	}
	return ret;
}

enum hrtimer_restart rmnet_shs_queue_core(struct hrtimer *t)
{
	const enum hrtimer_restart ret = HRTIMER_NORESTART;
	struct core_flush_s *core_work = container_of(t,
				 struct core_flush_s, core_timer);

	rmnet_shs_reset_cpus();

	schedule_work(&core_work->work);
	return ret;
}

void rmnet_shs_rx_wq_init(void)
{
	int i;

	/* Initialize a timer/work for each core for switching */
	for (i = 0; i < MAX_CPUS; i++) {
		rmnet_shs_cfg.core_flush[i].core = i;
		INIT_WORK(&rmnet_shs_cfg.core_flush[i].work,
			  rmnet_shs_flush_core_work);

		hrtimer_init(&rmnet_shs_cfg.core_flush[i].core_timer,
			     CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		rmnet_shs_cfg.core_flush[i].core_timer.function =
							rmnet_shs_queue_core;
	}
	/* Initialize a fallback/failsafe work for when dl ind fails */
	hrtimer_init(&rmnet_shs_cfg.hrtimer_shs,
		     CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	rmnet_shs_cfg.hrtimer_shs.function = rmnet_shs_map_flush_queue;
	INIT_WORK(&shs_rx_work.work, rmnet_flush_buffered);
}

unsigned int rmnet_shs_rx_wq_exit(void)
{
	unsigned int cpu_switch = rmnet_shs_inst_rate_switch;
	int i;

	/* Disable any further core_flush timer starts untill cleanup
	 * is complete.
	 */
	rmnet_shs_inst_rate_switch = 0;

	for (i = 0; i < MAX_CPUS; i++) {
		hrtimer_cancel(&GET_CTIMER(i));

		cancel_work_sync(&rmnet_shs_cfg.core_flush[i].work);
	}

	cancel_work_sync(&shs_rx_work.work);

	return cpu_switch;
}

int rmnet_shs_drop_backlog(struct sk_buff_head *list, int cpu)
{
	struct sk_buff *skb;
	struct softnet_data *sd = &per_cpu(softnet_data, cpu);

	rtnl_lock();
	while ((skb = skb_dequeue_tail(list)) != NULL) {
		if (rmnet_is_real_dev_registered(skb->dev)) {
			rmnet_shs_crit_err[RMNET_SHS_OUT_OF_MEM_ERR]++;
			/* Increment sd and netdev drop stats*/
			atomic_long_inc(&skb->dev->rx_dropped);
			input_queue_head_incr(sd);
			sd->dropped++;
			kfree_skb(skb);
		}
	}
	rtnl_unlock();

	return 0;
}
/* This will run in process context, avoid disabling bh */
static int rmnet_shs_oom_notify(struct notifier_block *self,
			    unsigned long emtpy, void *free)
{
	int input_qlen, process_qlen, cpu;
	int *nfree = (int*)free;
	struct sk_buff_head *process_q;
	struct sk_buff_head *input_q;

	for_each_possible_cpu(cpu) {

		process_q = &GET_PQUEUE(cpu);
		input_q = &GET_IQUEUE(cpu);
		input_qlen = skb_queue_len(process_q);
		process_qlen = skb_queue_len(input_q);

		if (rmnet_oom_pkt_limit &&
		    (input_qlen + process_qlen) >= rmnet_oom_pkt_limit) {
			rmnet_shs_drop_backlog(&per_cpu(softnet_data,
							cpu).input_pkt_queue, cpu);
			input_qlen = skb_queue_len(process_q);
			process_qlen = skb_queue_len(input_q);
			if (process_qlen >= rmnet_oom_pkt_limit) {
				rmnet_shs_drop_backlog(process_q, cpu);
			}
			/* Let oom_killer know memory was freed */
			(*nfree)++;
		}
	}
	return 0;
}

static struct notifier_block rmnet_oom_nb = {
	.notifier_call = rmnet_shs_oom_notify,
};

void rmnet_shs_ps_on_hdlr(void *port)
{
	rmnet_shs_wq_pause();
}

void rmnet_shs_ps_off_hdlr(void *port)
{
	rmnet_shs_wq_restart();
}

void rmnet_shs_dl_hdr_handler_v2(struct rmnet_map_dl_ind_hdr *dlhdr,
			      struct rmnet_map_control_command_header *qcmd)
{
	rmnet_shs_dl_hdr_handler(dlhdr);
}

void rmnet_shs_dl_hdr_handler(struct rmnet_map_dl_ind_hdr *dlhdr)
{

	SHS_TRACE_LOW(RMNET_SHS_DL_MRK, RMNET_SHS_DL_MRK_HDR_HDLR_START,
			    dlhdr->le.seq, dlhdr->le.pkts,
			    0xDEF, 0xDEF, NULL, NULL);
	if (rmnet_shs_cfg.num_pkts_parked > 0 &&
	    rmnet_shs_cfg.dl_ind_state != RMNET_SHS_IND_COMPLETE) {

		rmnet_shs_flush_reason[RMNET_SHS_FLUSH_INV_DL_IND]++;
		rmnet_shs_flush_table(0, RMNET_RX_CTXT);
	}
	rmnet_shs_cfg.dl_ind_state = RMNET_SHS_END_PENDING;
}

/* Triggers flushing of all packets upon DL trailer
 * receiving a DL trailer marker
 */
void rmnet_shs_dl_trl_handler_v2(struct rmnet_map_dl_ind_trl *dltrl,
			      struct rmnet_map_control_command_header *qcmd)
{
	rmnet_shs_dl_trl_handler(dltrl);
}

void rmnet_shs_dl_trl_handler(struct rmnet_map_dl_ind_trl *dltrl)
{
	SHS_TRACE_HIGH(RMNET_SHS_DL_MRK,
			     RMNET_SHS_FLUSH_DL_MRK_TRLR_HDLR_START,
			     rmnet_shs_cfg.num_pkts_parked, 0,
			     dltrl->seq_le, 0xDEF, NULL, NULL);
	rmnet_shs_cfg.dl_ind_state = RMNET_SHS_IND_COMPLETE;

	if (rmnet_shs_cfg.num_pkts_parked > 0) {
		rmnet_shs_flush_reason[RMNET_SHS_FLUSH_RX_DL_TRAILER]++;
		rmnet_shs_flush_table(0, RMNET_RX_CTXT);
	}
}

void rmnet_shs_init(struct net_device *dev, struct net_device *vnd)
{
	struct rps_map *map;
	int rc;
	u8 num_cpu;
	u8 map_mask;
	u8 map_len;

	if (rmnet_shs_cfg.rmnet_shs_init_complete)
		return;
	map = rcu_dereference(vnd->_rx->rps_map);

	if (!map) {
		map_mask = 0;
		map_len = 0;
	} else {
		map_mask = rmnet_shs_mask_from_map(map);
		map_len = rmnet_shs_get_mask_len(map_mask);
	}

	rmnet_shs_cfg.port = rmnet_get_port(dev);
	rmnet_shs_cfg.map_mask = map_mask;
	rmnet_shs_cfg.map_len = map_len;
	for (num_cpu = 0; num_cpu < MAX_CPUS; num_cpu++)
		INIT_LIST_HEAD(&rmnet_shs_cpu_node_tbl[num_cpu].node_list_id);

	rmnet_shs_freq_init();
	rc = register_oom_notifier(&rmnet_oom_nb);
	if (rc < 0) {
		pr_info("Rmnet_shs_oom register failure");
	}

	rmnet_shs_cfg.rmnet_shs_init_complete = 1;
}

/* Invoked during SHS module exit to gracefully consume all
 * the skb's that are parked and that aren't delivered yet
 */
void rmnet_shs_cancel_table(void)
{
	struct hlist_node *tmp;
	struct rmnet_shs_skbn_s *node;
	struct sk_buff *tmpbuf;
	int bkt;
	struct sk_buff *buf;
	unsigned long ht_flags;

	if (!rmnet_shs_cfg.num_pkts_parked)
		return;
	spin_lock_irqsave(&rmnet_shs_ht_splock, ht_flags);
	hash_for_each_safe(RMNET_SHS_HT, bkt, tmp, node, list) {
		for ((buf = node->skb_list.head); buf != NULL; buf = tmpbuf) {
			tmpbuf = buf->next;
			if (buf)
				consume_skb(buf);
		}
		node->skb_list.num_parked_skbs = 0;
		node->skb_list.num_parked_bytes = 0;
		node->skb_list.head = NULL;
		node->skb_list.tail = NULL;
	}
	rmnet_shs_cfg.num_bytes_parked = 0;
	rmnet_shs_cfg.num_pkts_parked = 0;
	rmnet_shs_cfg.is_pkt_parked = 0;
	rmnet_shs_cfg.force_flush_state = RMNET_SHS_FLUSH_DONE;

	spin_unlock_irqrestore(&rmnet_shs_ht_splock, ht_flags);

}

void rmnet_shs_get_update_skb_proto(struct sk_buff *skb,
				    struct rmnet_shs_skbn_s *node_p)
{
	struct iphdr *ip4h;
	struct ipv6hdr *ip6h;

	if (!skb_is_nonlinear(skb)) {
		switch (skb->protocol) {
		case htons(ETH_P_IP):
			node_p->skb_tport_proto = ip_hdr(skb)->protocol;
			break;
		case htons(ETH_P_IPV6):
			node_p->skb_tport_proto = ipv6_hdr(skb)->nexthdr;
			break;
		default:
			break;
		}
	} else {
		switch (skb->protocol) {
		case htons(ETH_P_IP):
			ip4h = (struct iphdr *)rmnet_map_data_ptr(skb);
			node_p->skb_tport_proto = ip4h->protocol;
			break;
		case htons(ETH_P_IPV6):
			ip6h = (struct ipv6hdr *)rmnet_map_data_ptr(skb);
			node_p->skb_tport_proto = ip6h->nexthdr;
			break;
		default:
			break;
		}
	}
}

/* Keeps track of all active flows. Packets reaching SHS are parked in order
 * per flow and then delivered to the next layer upon hitting any of the
 * flushing triggers.
 *
 * Whenever a new hash is observed, cores are chosen round robin so that
 * back to back new flows do not getting assigned to the same core
 */
void rmnet_shs_assign(struct sk_buff *skb, struct rmnet_port *port)
{
	struct rmnet_shs_skbn_s *node_p;
	struct hlist_node *tmp;
	struct net_device *dev = skb->dev;
	int map = rmnet_shs_cfg.map_mask;
	unsigned long ht_flags;
	int new_cpu;
	int map_cpu;
	u64 brate = 0;
	u32 cpu_map_index, hash;
	u8 is_match_found = 0;
	u8 is_shs_reqd = 0;
	struct rmnet_shs_cpu_node_s *cpu_node_tbl_p;

	rmnet_shs_in_count++;

	/*deliver non TCP/UDP packets right away*/
	if (!rmnet_shs_is_skb_stamping_reqd(skb)) {

		rmnet_shs_deliver_skb(skb);
		return;
	}
	if ((unlikely(!map)) || !rmnet_shs_cfg.rmnet_shs_init_complete) {
		rmnet_shs_deliver_skb(skb);
		SHS_TRACE_ERR(RMNET_SHS_ASSIGN,
				    RMNET_SHS_ASSIGN_CRIT_ERROR_NO_SHS_REQD,
				    0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);
		rmnet_shs_crit_err[RMNET_SHS_MAIN_SHS_RPS_INIT_ERR]++;
		return;
	}

	SHS_TRACE_HIGH(RMNET_SHS_ASSIGN, RMNET_SHS_ASSIGN_START,
			     0xDEF, 0xDEF, 0xDEF, 0xDEF, skb, NULL);

	hash = skb_get_hash(skb);

	/*  Using do while to spin lock and unlock only once */
	spin_lock_irqsave(&rmnet_shs_ht_splock, ht_flags);
	do {
		hash_for_each_possible_safe(RMNET_SHS_HT, node_p, tmp, list,
					    hash) {
			if (hash != node_p->hash)
				continue;


			SHS_TRACE_LOW(RMNET_SHS_ASSIGN,
				RMNET_SHS_ASSIGN_MATCH_FLOW_COMPLETE,
				0xDEF, 0xDEF, 0xDEF, 0xDEF, skb, NULL);

			cpu_map_index = node_p->map_index;

			rmnet_shs_chain_to_skb_list(skb, node_p);
			is_match_found = 1;
			is_shs_reqd = 1;
			break;

		}
		if (is_match_found)
			break;

		/* We haven't found a hash match upto this point
		 */
		new_cpu = rmnet_shs_new_flow_cpu(brate, dev);
		if (new_cpu < 0) {
			rmnet_shs_crit_err[RMNET_SHS_RPS_MASK_CHANGE]++;
			break;
		}

		if (rmnet_shs_cfg.num_flows > MAX_FLOWS) {
			rmnet_shs_crit_err[RMNET_SHS_MAX_FLOWS]++;
			break;
		}

		node_p = kzalloc(sizeof(*node_p), GFP_ATOMIC);

		if (!node_p) {
			rmnet_shs_crit_err[RMNET_SHS_MAIN_MALLOC_ERR]++;
			break;
		}

		rmnet_shs_cfg.num_flows++;

		node_p->dev = skb->dev;
		node_p->hash = skb->hash;
		node_p->map_cpu = new_cpu;
		node_p->map_index = rmnet_shs_idx_from_cpu(node_p->map_cpu,
							   map);
		INIT_LIST_HEAD(&node_p->node_id);
		rmnet_shs_get_update_skb_proto(skb, node_p);

		rmnet_shs_wq_inc_cpu_flow(node_p->map_cpu);
		/* Workqueue utilizes some of the values from above
		 * initializations . Therefore, we need to request
		 * for memory (to workqueue) after the above initializations
		 */
		rmnet_shs_wq_create_new_flow(node_p);
		map_cpu = node_p->map_cpu;
		cpu_node_tbl_p = &rmnet_shs_cpu_node_tbl[map_cpu];

		rmnet_shs_cpu_node_add(node_p, &cpu_node_tbl_p->node_list_id);
		hash_add_rcu(RMNET_SHS_HT, &node_p->list, skb->hash);
		/* Chain this pkt to skb list (most likely to skb_list.head)
		 * because this is the first packet for this flow
		 */
		rmnet_shs_chain_to_skb_list(skb, node_p);

		is_shs_reqd = 1;
		break;

	} while (0);

	if (!is_shs_reqd) {
		spin_unlock_irqrestore(&rmnet_shs_ht_splock, ht_flags);
		rmnet_shs_crit_err[RMNET_SHS_MAIN_SHS_NOT_REQD]++;
		rmnet_shs_deliver_skb(skb);
		SHS_TRACE_ERR(RMNET_SHS_ASSIGN,
				    RMNET_SHS_ASSIGN_CRIT_ERROR_NO_SHS_REQD,
				    0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);
		return;
	}

	/* We got the first packet after a previous successdul flush. Arm the
	 * flushing timer.
	 */
	if (!rmnet_shs_cfg.is_pkt_parked &&
	    rmnet_shs_cfg.num_pkts_parked &&
	    rmnet_shs_fall_back_timer) {
		rmnet_shs_cfg.is_pkt_parked = 1;
		rmnet_shs_cfg.force_flush_state = RMNET_SHS_FLUSH_OFF;
		if (hrtimer_active(&rmnet_shs_cfg.hrtimer_shs)) {
			SHS_TRACE_LOW(RMNET_SHS_ASSIGN,
				    RMNET_SHS_ASSIGN_PARK_TMR_CANCEL,
				    RMNET_SHS_FORCE_FLUSH_TIME_NSEC,
				    0xDEF, 0xDEF, 0xDEF, skb, NULL);
			hrtimer_cancel(&rmnet_shs_cfg.hrtimer_shs);
		}
		hrtimer_start(&rmnet_shs_cfg.hrtimer_shs,
			      ns_to_ktime(rmnet_shs_timeout * NS_IN_MS),
					  HRTIMER_MODE_REL);
		SHS_TRACE_LOW(RMNET_SHS_ASSIGN,
				    RMNET_SHS_ASSIGN_PARK_TMR_START,
				    RMNET_SHS_FORCE_FLUSH_TIME_NSEC,
				    0xDEF, 0xDEF, 0xDEF, skb, NULL);
	}
	spin_unlock_irqrestore(&rmnet_shs_ht_splock, ht_flags);

	if (rmnet_shs_cfg.num_pkts_parked >
						rmnet_shs_pkts_store_limit) {

		if (rmnet_shs_stats_enabled)
			rmnet_shs_flush_reason[RMNET_SHS_FLUSH_PKT_LIMIT]++;

		SHS_TRACE_HIGH(RMNET_SHS_FLUSH,
				     RMNET_SHS_FLUSH_PKT_LIMIT_TRIGGER, 0,
				     0xDEF, 0xDEF, 0xDEF, NULL, NULL);
		rmnet_shs_flush_table(1, RMNET_RX_CTXT);

	} else if (rmnet_shs_cfg.num_bytes_parked >
						rmnet_shs_byte_store_limit) {

		if (rmnet_shs_stats_enabled)
			rmnet_shs_flush_reason[RMNET_SHS_FLUSH_BYTE_LIMIT]++;

		SHS_TRACE_HIGH(RMNET_SHS_FLUSH,
				     RMNET_SHS_FLUSH_BYTE_LIMIT_TRIGGER, 0,
				     0xDEF, 0xDEF, 0xDEF, NULL, NULL);
		rmnet_shs_flush_table(1, RMNET_RX_CTXT);

	}
	/* Flushing timer that was armed previously has successfully fired.
	 * Now we trigger force flushing of all packets. If a flow is waiting
	 * to switch to another core, it will be forcefully moved during this
	 * trigger.
	 *
	 * In case the previously delivered packets haven't been processed by
	 * the next layers, the parked packets may be delivered out of order
	 * until all the previously delivered packets have been processed
	 * successully
	 */
	else if (rmnet_shs_cfg.force_flush_state == RMNET_SHS_FLUSH_ON) {
		rmnet_shs_flush_reason[RMNET_SHS_FLUSH_TIMER_EXPIRY]++;
		SHS_TRACE_HIGH(RMNET_SHS_FLUSH,
				     RMNET_SHS_FLUSH_FORCE_TRIGGER, 1,
				     rmnet_shs_cfg.num_pkts_parked,
				     0xDEF, 0xDEF, NULL, NULL);
		rmnet_shs_flush_table(0, RMNET_RX_CTXT);

	} else if (rmnet_shs_cfg.num_pkts_parked &&
		   rmnet_shs_cfg.dl_ind_state != RMNET_SHS_END_PENDING) {
		rmnet_shs_flush_reason[RMNET_SHS_FLUSH_INV_DL_IND]++;
		rmnet_shs_flush_table(0, RMNET_RX_CTXT);
	}
}

/* Cancels the flushing timer if it has been armed
 * Deregisters DL marker indications
 */
void rmnet_shs_exit(unsigned int cpu_switch)
{
	rmnet_shs_freq_exit();
	rmnet_shs_cfg.dl_mrk_ind_cb.dl_hdr_handler = NULL;
	rmnet_shs_cfg.dl_mrk_ind_cb.dl_trl_handler = NULL;
	rmnet_map_dl_ind_deregister(rmnet_shs_cfg.port,
				    &rmnet_shs_cfg.dl_mrk_ind_cb);
	rmnet_shs_cfg.is_reg_dl_mrk_ind = 0;
	unregister_oom_notifier(&rmnet_oom_nb);

	if (rmnet_shs_cfg.is_timer_init)
		hrtimer_cancel(&rmnet_shs_cfg.hrtimer_shs);

	memset(&rmnet_shs_cfg, 0, sizeof(rmnet_shs_cfg));
	rmnet_shs_cfg.port = NULL;
	rmnet_shs_cfg.rmnet_shs_init_complete = 0;
	rmnet_shs_inst_rate_switch = cpu_switch;
}
