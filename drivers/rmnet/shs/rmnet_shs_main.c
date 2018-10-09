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
 * RMNET Data Smart Hash stamping solution
 *
 */

#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/ip.h>
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
#define INVALID_CPU -1

#define GET_QTAIL(SD, CPU) (per_cpu(SD, CPU).input_queue_tail)
#define GET_QHEAD(SD, CPU) (per_cpu(SD, CPU).input_queue_head)
#define GET_CTIMER(CPU) rmnet_shs_cfg.core_flush[CPU].core_timer

/* Local Definitions and Declarations */
DEFINE_SPINLOCK(rmnet_shs_ht_splock);
DEFINE_HASHTABLE(RMNET_SHS_HT, RMNET_SHS_HT_SIZE);
struct rmnet_shs_cpu_node_s rmnet_shs_cpu_node_tbl[MAX_CPUS];
/* Maintains a list of flows associated with a core
 * Also keeps track of number of packets processed on that core
 */

struct rmnet_shs_cfg_s rmnet_shs_cfg;
static u8 rmnet_shs_init_complete;
/* This flag is set to true after a successful SHS module init*/

struct rmnet_shs_flush_work shs_delayed_work;
/* Delayed workqueue that will be used to flush parked packets*/

unsigned long int rmnet_shs_flush_reason[RMNET_SHS_FLUSH_MAX_REASON];
module_param_array(rmnet_shs_flush_reason, ulong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_flush_reason, "rmnet shs skb flush trigger type");

unsigned int rmnet_shs_byte_store_limit __read_mostly = 271800 * 8;
module_param(rmnet_shs_byte_store_limit, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_byte_store_limit, "Maximum byte module will park");

unsigned int rmnet_shs_pkts_store_limit __read_mostly = 2100;
module_param(rmnet_shs_pkts_store_limit, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_pkts_store_limit, "Maximum pkts module will park");

unsigned int rmnet_shs_max_core_wait __read_mostly = 10;
module_param(rmnet_shs_max_core_wait, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_max_core_wait,
		 "Max wait module will wait during move to perf core in ms");

unsigned int rmnet_shs_inst_rate_interval __read_mostly = 15;
module_param(rmnet_shs_inst_rate_interval, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_inst_rate_interval,
		 "Max interval we sample for instant burst prioritizing");

unsigned int rmnet_shs_inst_rate_max_pkts __read_mostly = 1800;
module_param(rmnet_shs_inst_rate_max_pkts, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_inst_rate_max_pkts,
		 "Max pkts in a instant burst interval before prioritizing");

unsigned int rmnet_shs_switch_cores __read_mostly = 1;
module_param(rmnet_shs_switch_cores, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_switch_cores, "Switch core upon hitting threshold");

unsigned int rmnet_shs_cpu_max_qdiff[MAX_CPUS];
module_param_array(rmnet_shs_cpu_max_qdiff, uint, 0, 0644);
MODULE_PARM_DESC(rmnet_shs_cpu_max_qdiff, "Max queue length seen of each core");

unsigned int rmnet_shs_cpu_max_coresum[MAX_CPUS];
module_param_array(rmnet_shs_cpu_max_coresum, uint, 0, 0644);
MODULE_PARM_DESC(rmnet_shs_cpu_max_coresum, "Max coresum seen of each core");

void rmnet_shs_cpu_node_remove(struct rmnet_shs_skbn_s *node)
{
	trace_rmnet_shs_low(RMNET_SHS_CPU_NODE, RMNET_SHS_CPU_NODE_FUNC_REMOVE,
			    0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);

	list_del_init(&node->node_id);
}

void rmnet_shs_cpu_node_add(struct rmnet_shs_skbn_s *node,
			    struct list_head *hd)
{
	trace_rmnet_shs_low(RMNET_SHS_CPU_NODE, RMNET_SHS_CPU_NODE_FUNC_ADD,
			    0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);

	list_add(&node->node_id, hd);
}

void rmnet_shs_cpu_node_move(struct rmnet_shs_skbn_s *node,
			     struct list_head *hd)
{
	trace_rmnet_shs_low(RMNET_SHS_CPU_NODE, RMNET_SHS_CPU_NODE_FUNC_MOVE,
			    0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);

	list_move(&node->node_id, hd);
}

/* Evaluates the incoming transport protocol of the incoming skb. Determines
 * if the skb transport protocol will be supported by SHS module
 */
int rmnet_shs_is_skb_stamping_reqd(struct sk_buff *skb)
{
	int ret_val = 0;

	switch (skb->protocol) {
	case htons(ETH_P_IP):
		if ((ip_hdr(skb)->protocol == IPPROTO_TCP) ||
		    (ip_hdr(skb)->protocol == IPPROTO_UDP))
			ret_val =  1;

		break;

	case htons(ETH_P_IPV6):
		if ((ipv6_hdr(skb)->nexthdr == IPPROTO_TCP) ||
		    (ipv6_hdr(skb)->nexthdr == IPPROTO_UDP))
			ret_val =  1;

		break;

	default:
		break;
	}

	trace_rmnet_shs_low(RMNET_SHS_SKB_STAMPING, RMNET_SHS_SKB_STAMPING_END,
			    ret_val, 0xDEF, 0xDEF, 0xDEF, skb, NULL);

	return ret_val;
}

static void rmnet_shs_update_core_load(int cpu, int burst)
{

	struct  timespec time1;
	struct  timespec *time2;
	long int curinterval;
	int maxinterval = (rmnet_shs_inst_rate_interval < 5) ? 5 :
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

static int rmnet_shs_is_core_loaded(int cpu)
{

	return rmnet_shs_cfg.core_flush[cpu].coresum >=
		rmnet_shs_inst_rate_max_pkts;

}

/* We deliver packets to GRO module only for TCP traffic*/
static int rmnet_shs_check_skb_can_gro(struct sk_buff *skb)
{
	int ret_val = -EPROTONOSUPPORT;

	switch (skb->protocol) {
	case htons(ETH_P_IP):
		if (ip_hdr(skb)->protocol == IPPROTO_TCP)
			ret_val =  0;
		break;

	case htons(ETH_P_IPV6):
		if (ipv6_hdr(skb)->nexthdr == IPPROTO_TCP)
			ret_val =  0;
		break;
	default:
		ret_val =  -EPROTONOSUPPORT;
		break;
	}

	trace_rmnet_shs_low(RMNET_SHS_SKB_CAN_GRO, RMNET_SHS_SKB_CAN_GRO_END,
			    ret_val, 0xDEF, 0xDEF, 0xDEF, skb, NULL);

	return ret_val;
}

/* Delivers skb's to the next module */
static void rmnet_shs_deliver_skb(struct sk_buff *skb)
{
	struct rmnet_priv *priv;
	struct napi_struct *napi;

	trace_rmnet_shs_low(RMNET_SHS_DELIVER_SKB, RMNET_SHS_DELIVER_SKB_START,
			    0xDEF, 0xDEF, 0xDEF, 0xDEF, skb, NULL);

	if (!rmnet_shs_check_skb_can_gro(skb)) {
		if ((napi = get_current_napi_context())) {
			napi_gro_receive(napi, skb);
		} else {
			priv = netdev_priv(skb->dev);
			gro_cells_receive(&priv->gro_cells, skb);
		}
	} else {
		netif_receive_skb(skb);
	}
}

/* Returns the number of low power cores configured and available
 * for packet processing
 */
int rmnet_shs_num_lpwr_cores_configured(struct rps_map *map)
{
	int ret = 0;
	u16 idx = 0;

	for (idx = 0; idx < map->len; idx++)
		if (map->cpus[idx] < PERF_CLUSTER)
			ret += 1;

	trace_rmnet_shs_low(RMNET_SHS_CORE_CFG,
			    RMNET_SHS_CORE_CFG_NUM_LO_CORES,
			    ret, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);
	return ret;
}

/* Returns the number of performance cores configured and available
 * for packet processing
 */
int rmnet_shs_num_perf_cores_configured(struct rps_map *map)
{
	int ret = 0;
	u16 idx = 0;

	for (idx = 0; idx < map->len; idx++)
		if (map->cpus[idx] >= PERF_CLUSTER)
			ret += 1;

	trace_rmnet_shs_low(RMNET_SHS_CORE_CFG,
			    RMNET_SHS_CORE_CFG_NUM_HI_CORES,
			    ret, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);

	return ret;
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

int rmnet_shs_is_lpwr_cpu(u16 cpu)
{
	int ret = 1;
	u32 big_cluster_mask = (1 << PERF_CLUSTER) - 1;

	if ((1 << cpu) >= big_cluster_mask)
		ret = 0;

	trace_rmnet_shs_low(RMNET_SHS_CORE_CFG,
			    RMNET_SHS_CORE_CFG_CHK_LO_CPU,
			    ret, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);
	return ret;
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

	if (maplen < MAX_CPUS)
		ret = ((((index + ((maplen % 2) ? 1 : 0))) << 28)
			* offsetmap[(maplen - 1) >> 1]) | (hash & 0xFFFFFF);

	trace_rmnet_shs_low(RMNET_SHS_HASH_MAP, RMNET_SHS_HASH_MAP_FORM_HASH,
			    ret, hash, index, maplen, NULL, NULL);

	return ret;
}

int rmnet_shs_map_idx_from_cpu(u16 cpu, struct rps_map *map)
{
	int ret = INVALID_CPU;
	u16 idx;

	for (idx = 0; idx < map->len; idx++) {
		if (cpu == map->cpus[idx]) {
			ret = idx;
			break;
		}
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
	else
		flow_cpu = rmnet_shs_wq_get_perf_cpu_new_flow(dev);

	trace_rmnet_shs_high(RMNET_SHS_ASSIGN,
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
		cpu = rmnet_shs_wq_get_least_utilized_core(0xF0);
		if (cpu < 0)
			cpu = node->hstats->suggested_cpu;
	} else if (node->hstats != NULL)
		cpu = node->hstats->suggested_cpu;

	return cpu;
}

int rmnet_shs_get_hash_map_idx_to_stamp(struct rmnet_shs_skbn_s *node_p)
{
	int cpu, idx = INVALID_CPU;
	struct rps_map *map;

	cpu = rmnet_shs_get_suggested_cpu(node_p);


	map = rcu_dereference(node_p->dev->_rx->rps_map);
	if (!node_p->dev || !node_p->dev->_rx || !map)
		return idx;

	idx = rmnet_shs_map_idx_from_cpu(cpu, map);

	trace_rmnet_shs_low(RMNET_SHS_HASH_MAP,
			    RMNET_SHS_HASH_MAP_IDX_TO_STAMP,
			    node_p->hash, cpu, idx, 0xDEF, node_p, NULL);
	return idx;
}

u32 rmnet_shs_get_cpu_qhead(u8 cpu_num)
{
	u32 ret = 0;

	if (cpu_num < MAX_CPUS)
		ret = rmnet_shs_cpu_node_tbl[cpu_num].qhead;

	trace_rmnet_shs_low(RMNET_SHS_CORE_CFG, RMNET_SHS_CORE_CFG_GET_QHEAD,
			    cpu_num, ret, 0xDEF, 0xDEF, NULL, NULL);
	return ret;
}

u32 rmnet_shs_get_cpu_qtail(u8 cpu_num)
{
	u32 ret = 0;

	if (cpu_num < MAX_CPUS)
		ret =  rmnet_shs_cpu_node_tbl[cpu_num].qtail;

	trace_rmnet_shs_low(RMNET_SHS_CORE_CFG, RMNET_SHS_CORE_CFG_GET_QTAIL,
			    cpu_num, ret, 0xDEF, 0xDEF, NULL, NULL);

	return ret;
}

u32 rmnet_shs_get_cpu_qdiff(u8 cpu_num)
{
	u32 ret = 0;

	if (cpu_num < MAX_CPUS)
		ret =  rmnet_shs_cpu_node_tbl[cpu_num].qdiff;

	trace_rmnet_shs_low(RMNET_SHS_CORE_CFG, RMNET_SHS_CORE_CFG_GET_QTAIL,
			    cpu_num, ret, 0xDEF, 0xDEF, NULL, NULL);

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
	rmnet_shs_cpu_node_tbl[cpu_num].qdiff =
	rmnet_shs_cpu_node_tbl[cpu_num].qtail -
	rmnet_shs_cpu_node_tbl[cpu_num].qhead;
	rcu_read_unlock();

	trace_rmnet_shs_low(RMNET_SHS_CORE_CFG,
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

		trace_rmnet_shs_low(RMNET_SHS_CORE_CFG,
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
	struct rps_map *map;
	int cpu_map_index;
	u32 cur_cpu_qhead;
	u32 node_qhead;
	int ret = 0;
	int prev_cpu = -1;
	int ccpu;
	int cpu_num;
	struct rmnet_shs_cpu_node_s *cpun;

	cpu_map_index = rmnet_shs_get_hash_map_idx_to_stamp(node);
	do {
		prev_cpu = node->map_cpu;
		if (cpu_map_index < 0) {
			node->is_shs_enabled = 0;
			ret = 1;
			break;
		}
		node->is_shs_enabled = 1;
		map = rcu_dereference(node->dev->_rx->rps_map);
		if (!node->dev->_rx || !map){
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
		if ((cur_cpu_qhead >= node_qhead) ||
		    (node->skb_tport_proto == IPPROTO_TCP) ||
		    (force_flush)) {
			if (rmnet_shs_switch_cores) {

			/* Move the amount parked to other core's count
			 * Update old core's parked to not include diverted
			 * packets and update new core's packets
			 */
				rmnet_shs_cpu_node_tbl[map->cpus[cpu_map_index]].parkedlen +=
											node->skb_list.num_parked_skbs;
				rmnet_shs_cpu_node_tbl[node->map_cpu].parkedlen -=
											node->skb_list.num_parked_skbs;
				node->map_index = cpu_map_index;
				node->map_cpu = map->cpus[cpu_map_index];
				ccpu = node->map_cpu;

				/* Mark gold core as prio to prevent
				 * flows from moving in wq
				 */
				if (rmnet_shs_cpu_node_tbl[cpu_num].prio) {
					node->hstats->suggested_cpu = ccpu;
					rmnet_shs_cpu_node_tbl[ccpu].wqprio = 1;
				}
				cpun = &rmnet_shs_cpu_node_tbl[node->map_cpu];
				rmnet_shs_update_cpu_proc_q_all_cpus();
				node->queue_head = cpun->qhead;
				rmnet_shs_cpu_node_move(node,
							&cpun->node_list_id);
				trace_rmnet_shs_high(RMNET_SHS_FLUSH,
					RMNET_SHS_FLUSH_NODE_CORE_SWITCH,
					node->map_cpu, prev_cpu,
					0xDEF, 0xDEF, node, NULL);
			}
			ret = 1;
		}
	} while (0);

	trace_rmnet_shs_low(RMNET_SHS_FLUSH,
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
	trace_rmnet_shs_high(RMNET_SHS_FLUSH, RMNET_SHS_FLUSH_START,
			     rmnet_shs_cfg.num_pkts_parked,
			     rmnet_shs_cfg.num_bytes_parked,
			     0xDEF, 0xDEF, NULL, NULL);

	spin_lock_irqsave(&rmnet_shs_ht_splock, ht_flags);
		cpu_tail = rmnet_shs_get_cpu_qtail(cpu_num);
		list_for_each_safe(ptr, next,
			&rmnet_shs_cpu_node_tbl[cpu_num].node_list_id) {
			n = list_entry(ptr, struct rmnet_shs_skbn_s, node_id);
			if (n != NULL && n->skb_list.num_parked_skbs) {
				num_pkts_flush = n->skb_list.num_parked_skbs;
				num_bytes_flush = n->skb_list.num_parked_bytes;

				rmnet_shs_chk_and_flush_node(n, 1);

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
	rmnet_shs_cpu_node_tbl[cpu_num].parkedlen = 0;
	spin_unlock_irqrestore(&rmnet_shs_ht_splock, ht_flags);

	trace_rmnet_shs_high(RMNET_SHS_FLUSH, RMNET_SHS_FLUSH_END,
	     rmnet_shs_cfg.num_pkts_parked,
			     rmnet_shs_cfg.num_bytes_parked,
			     total_pkts_flush, total_bytes_flush, NULL, NULL);

}

static void rmnet_shs_flush_core_work(struct work_struct *work)
{
	struct core_flush_s *core_work = container_of(work,
				 struct core_flush_s, work);

	rmnet_shs_flush_core(core_work->core);
}

/* Flushes all the packets parked in order for this flow */
void rmnet_shs_flush_node(struct rmnet_shs_skbn_s *node)
{
	struct sk_buff *skb;
	struct sk_buff *nxt_skb = NULL;
	struct rps_map *map;
	u32 skbs_delivered = 0;
	u32 skb_bytes_delivered = 0;
	u32 hash2stamp;

	if (!node->skb_list.head)
		return;

	map = rcu_dereference(node->dev->_rx->rps_map);

	if (!map) {
		hash2stamp = rmnet_shs_form_hash(node->map_index,
					 map->len, node->skb_list.head->hash);
	} else {
		node->is_shs_enabled = 0;
	}
	trace_rmnet_shs_high(RMNET_SHS_FLUSH,
			     RMNET_SHS_FLUSH_NODE_START,
			     node->hash, hash2stamp,
			     node->skb_list.num_parked_skbs,
			     node->skb_list.num_parked_bytes,
			     node, node->skb_list.head);

	for ((skb = node->skb_list.head); skb != NULL; skb = nxt_skb) {

		nxt_skb = skb->next;
		if (node->is_shs_enabled)
			skb->hash = hash2stamp;

		skb->next = NULL;
		skbs_delivered += 1;
		skb_bytes_delivered += skb->len;

		rmnet_shs_deliver_skb(skb);

	}

	node->skb_list.num_parked_skbs = 0;
	node->skb_list.num_parked_bytes = 0;
	node->skb_list.head = NULL;
	node->skb_list.tail = NULL;

	trace_rmnet_shs_high(RMNET_SHS_FLUSH, RMNET_SHS_FLUSH_NODE_END,
			     node->hash, hash2stamp,
			     skbs_delivered, skb_bytes_delivered, node, NULL);
}

/* Evaluates if all the packets corresponding to a particular flow can
 * be flushed.
 */
int rmnet_shs_chk_and_flush_node(struct rmnet_shs_skbn_s *node, u8 force_flush)
{
	int ret_val = 0;

	trace_rmnet_shs_high(RMNET_SHS_FLUSH,
			     RMNET_SHS_FLUSH_CHK_AND_FLUSH_NODE_START,
			     force_flush, 0xDEF, 0xDEF, 0xDEF,
			     node, NULL);
	if (rmnet_shs_node_can_flush_pkts(node, force_flush)) {
		rmnet_shs_flush_node(node);
		ret_val = 1;
	}
	trace_rmnet_shs_high(RMNET_SHS_FLUSH,
			     RMNET_SHS_FLUSH_CHK_AND_FLUSH_NODE_END,
			     ret_val, force_flush, 0xDEF, 0xDEF,
			     node, NULL);
	return ret_val;
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
void rmnet_shs_flush_table(u8 flsh)
{
	struct rmnet_shs_skbn_s *n;
	struct list_head *ptr, *next;
	unsigned long ht_flags;
	int cpu_num;
	u32 cpu_tail;
	u32 num_pkts_flush = 0;
	u32 num_bytes_flush = 0;
	u32 total_pkts_flush = 0;
	u32 total_bytes_flush = 0;
	u8 is_flushed = 0;
	u32 wait = (!rmnet_shs_max_core_wait) ? 1 : rmnet_shs_max_core_wait;

	/* Record a qtail + pkts flushed or move if reqd
	 * currently only use qtail for non TCP flows
	 */
	rmnet_shs_update_cpu_proc_q_all_cpus();
	trace_rmnet_shs_high(RMNET_SHS_FLUSH, RMNET_SHS_FLUSH_START,
			     rmnet_shs_cfg.num_pkts_parked,
			     rmnet_shs_cfg.num_bytes_parked,
			     0xDEF, 0xDEF, NULL, NULL);

	spin_lock_irqsave(&rmnet_shs_ht_splock, ht_flags);
	for (cpu_num = 0; cpu_num < MAX_CPUS; cpu_num++) {

		cpu_tail = rmnet_shs_get_cpu_qtail(cpu_num);

		/* If core is loaded set core flows as priority and
		 * start a 10ms hard flush timer
		 */
		if (rmnet_shs_is_lpwr_cpu(cpu_num) &&
		    !rmnet_shs_cpu_node_tbl[cpu_num].prio)
			rmnet_shs_update_core_load(cpu_num,
			rmnet_shs_cpu_node_tbl[cpu_num].parkedlen);

		if (rmnet_shs_is_core_loaded(cpu_num) &&
		    rmnet_shs_is_lpwr_cpu(cpu_num) &&
		    !rmnet_shs_cpu_node_tbl[cpu_num].prio) {

			rmnet_shs_cpu_node_tbl[cpu_num].prio = 1;
			if (hrtimer_active(&GET_CTIMER(cpu_num)))
				hrtimer_cancel(&GET_CTIMER(cpu_num));

			hrtimer_start(&GET_CTIMER(cpu_num),
				      ns_to_ktime(wait * NS_IN_MS),
				      HRTIMER_MODE_REL);

		}

		list_for_each_safe(ptr, next,
			&rmnet_shs_cpu_node_tbl[cpu_num].node_list_id) {
			n = list_entry(ptr, struct rmnet_shs_skbn_s, node_id);

			if (n != NULL && n->skb_list.num_parked_skbs) {
				num_pkts_flush = n->skb_list.num_parked_skbs;
				num_bytes_flush = n->skb_list.num_parked_bytes;
				is_flushed = rmnet_shs_chk_and_flush_node(n,
									  flsh);

				if (is_flushed) {
					total_pkts_flush += num_pkts_flush;
					total_bytes_flush += num_bytes_flush;
					rmnet_shs_cpu_node_tbl[n->map_cpu].parkedlen -= num_pkts_flush;

					if (n->map_cpu == cpu_num) {
						cpu_tail += num_pkts_flush;
						n->queue_head = cpu_tail;

					}
				}
			}
		}
		if (rmnet_shs_cpu_node_tbl[cpu_num].parkedlen < 0)
			rmnet_shs_crit_err[RMNET_SHS_CPU_PKTLEN_ERR]++;

		if (rmnet_shs_get_cpu_qdiff(cpu_num) >=
		    rmnet_shs_cpu_max_qdiff[cpu_num])
			rmnet_shs_cpu_max_qdiff[cpu_num] =
						rmnet_shs_get_cpu_qdiff(cpu_num);
		}

	spin_unlock_irqrestore(&rmnet_shs_ht_splock, ht_flags);

	rmnet_shs_cfg.num_bytes_parked -= total_bytes_flush;
	rmnet_shs_cfg.num_pkts_parked -= total_pkts_flush;

	trace_rmnet_shs_high(RMNET_SHS_FLUSH, RMNET_SHS_FLUSH_END,
			     rmnet_shs_cfg.num_pkts_parked,
			     rmnet_shs_cfg.num_bytes_parked,
			     total_pkts_flush, total_bytes_flush, NULL, NULL);

	if ((rmnet_shs_cfg.num_bytes_parked <= 0) ||
	    (rmnet_shs_cfg.num_pkts_parked <= 0)) {

		rmnet_shs_cfg.num_bytes_parked = 0;
		rmnet_shs_cfg.num_pkts_parked = 0;
		rmnet_shs_cfg.is_pkt_parked = 0;
		rmnet_shs_cfg.force_flush_state = RMNET_SHS_FLUSH_DONE;
	}

}

/* After we have decided to handle the incoming skb we park them in order
 * per flow
 */
void rmnet_shs_chain_to_skb_list(struct sk_buff *skb,
				 struct rmnet_shs_skbn_s *node)
{
	if (node->skb_list.num_parked_skbs > 0) {
		node->skb_list.tail->next = skb;
		node->skb_list.tail = node->skb_list.tail->next;
	} else {
		node->skb_list.head = skb;
		node->skb_list.tail = skb;
	}

	node->skb_list.num_parked_bytes += skb->len;
	rmnet_shs_cfg.num_bytes_parked  += skb->len;
	rmnet_shs_cpu_node_tbl[node->map_cpu].parkedlen++;

	node->skb_list.num_parked_skbs += 1;
	rmnet_shs_cfg.num_pkts_parked  += 1;
	trace_rmnet_shs_high(RMNET_SHS_ASSIGN,
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
	u8 is_force_flush = 0;

	trace_rmnet_shs_high(RMNET_SHS_FLUSH,
			     RMNET_SHS_FLUSH_DELAY_WQ_START, is_force_flush,
			     rmnet_shs_cfg.force_flush_state, 0xDEF,
			     0xDEF, NULL, NULL);

	if (rmnet_shs_cfg.is_pkt_parked &&
	   rmnet_shs_cfg.force_flush_state == RMNET_SHS_FLUSH_ON) {

		rmnet_shs_flush_table(is_force_flush);
		rmnet_shs_flush_reason[RMNET_SHS_FLUSH_TIMER_EXPIRY]++;
	}
	trace_rmnet_shs_high(RMNET_SHS_FLUSH,
			     RMNET_SHS_FLUSH_DELAY_WQ_END,
			     is_force_flush, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);
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

	trace_rmnet_shs_high(RMNET_SHS_FLUSH,
			     RMNET_SHS_FLUSH_PARK_TMR_EXPIRY,
			     rmnet_shs_cfg.force_flush_state, 0xDEF,
			     0xDEF, 0xDEF, NULL, NULL);
	if (rmnet_shs_cfg.num_pkts_parked > 0) {
		if (rmnet_shs_cfg.force_flush_state != RMNET_SHS_FLUSH_ON) {
			rmnet_shs_cfg.force_flush_state = RMNET_SHS_FLUSH_ON;
			hrtimer_forward(t, hrtimer_cb_get_time(t),
					ns_to_ktime(2000000));
			ret = HRTIMER_RESTART;

			trace_rmnet_shs_high(RMNET_SHS_FLUSH,
					     RMNET_SHS_FLUSH_PARK_TMR_RESTART,
					     rmnet_shs_cfg.num_pkts_parked,
					     0xDEF, 0xDEF, 0xDEF, NULL, NULL);
		} else if (rmnet_shs_cfg.force_flush_state ==
			   RMNET_SHS_FLUSH_DONE) {
			rmnet_shs_cfg.force_flush_state == RMNET_SHS_FLUSH_OFF;

		} else {
			trace_rmnet_shs_high(RMNET_SHS_FLUSH,
					     RMNET_SHS_FLUSH_DELAY_WQ_TRIGGER,
					     rmnet_shs_cfg.force_flush_state,
					     0xDEF, 0xDEF, 0xDEF, NULL, NULL);
			schedule_work((struct work_struct *)&shs_delayed_work);
		}
	}
	return ret;
}

enum hrtimer_restart rmnet_shs_queue_core(struct hrtimer *t)
{
	const enum hrtimer_restart ret = HRTIMER_NORESTART;
	struct core_flush_s *core_work = container_of(t,
				 struct core_flush_s, core_timer);

	schedule_work(&core_work->work);
	return ret;
}

void rmnet_shs_aggregate_init(void)
{
	int i;

	for (i = 0; i < MAX_CPUS; i++) {
		rmnet_shs_cfg.core_flush[i].core = i;
		INIT_WORK(&rmnet_shs_cfg.core_flush[i].work,
			  rmnet_shs_flush_core_work);

		hrtimer_init(&rmnet_shs_cfg.core_flush[i].core_timer,
			     CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		rmnet_shs_cfg.core_flush[i].core_timer.function =
							rmnet_shs_queue_core;
	}
	       hrtimer_init(&rmnet_shs_cfg.hrtimer_shs,
			    CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	       rmnet_shs_cfg.hrtimer_shs.function = rmnet_shs_map_flush_queue;
	       INIT_WORK(&shs_delayed_work.work, rmnet_flush_buffered);
}

void rmnet_shs_ps_on_hdlr(void *port)
{
	rmnet_shs_wq_pause();
}

void rmnet_shs_ps_off_hdlr(void *port)
{
	rmnet_shs_wq_restart();
}

void rmnet_shs_dl_hdr_handler(struct rmnet_map_dl_ind_hdr *dlhdr)
{
	trace_rmnet_shs_low(RMNET_SHS_DL_MRK, RMNET_SHS_DL_MRK_HDR_HDLR_START,
			    dlhdr->le.seq, dlhdr->le.pkts,
			    0xDEF, 0xDEF, NULL, NULL);
}

/* Triggers flushing of all packets upon DL trailer
 * receiving a DL trailer marker
 */
void rmnet_shs_dl_trl_handler(struct rmnet_map_dl_ind_trl *dltrl)
{

	u8 is_force_flush = 0;

	trace_rmnet_shs_high(RMNET_SHS_DL_MRK,
			     RMNET_SHS_FLUSH_DL_MRK_TRLR_HDLR_START,
			     rmnet_shs_cfg.num_pkts_parked, is_force_flush,
			     dltrl->seq_le, 0xDEF, NULL, NULL);

	if (rmnet_shs_cfg.num_pkts_parked > 0) {
		rmnet_shs_flush_reason[RMNET_SHS_FLUSH_RX_DL_TRAILER]++;
		rmnet_shs_flush_table(is_force_flush);
	}
}

void rmnet_shs_init(struct net_device *dev)
{
	u8 num_cpu;

	if (rmnet_shs_init_complete)
		return;

	rmnet_shs_cfg.port = rmnet_get_port(dev);

	for (num_cpu = 0; num_cpu < MAX_CPUS; num_cpu++)
		INIT_LIST_HEAD(&rmnet_shs_cpu_node_tbl[num_cpu].node_list_id);

	rmnet_shs_init_complete = 1;
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
	switch (skb->protocol) {
	case htons(ETH_P_IP):
		node_p->skb_tport_proto = ip_hdr(skb)->protocol;
		break;
	case htons(ETH_P_IPV6):
		node_p->skb_tport_proto = ipv6_hdr(skb)->nexthdr;
		break;
	default:
		node_p->skb_tport_proto = IPPROTO_RAW;
		break;
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
	struct rps_map *map = rcu_dereference(skb->dev->_rx->rps_map);
	unsigned long ht_flags;
	int new_cpu;
	int map_cpu;
	u64 brate = 0;
	u32 cpu_map_index, hash;
	u8 is_match_found = 0;
	u8 is_shs_reqd = 0;
	struct rmnet_shs_cpu_node_s *cpu_node_tbl_p;

	/*deliver non TCP/UDP packets right away*/
	if (!rmnet_shs_is_skb_stamping_reqd(skb)) {
		rmnet_shs_deliver_skb(skb);
		return;
	}

	if ((unlikely(!map))|| !rmnet_shs_init_complete) {
		rmnet_shs_deliver_skb(skb);
		trace_rmnet_shs_err(RMNET_SHS_ASSIGN,
				    RMNET_SHS_ASSIGN_CRIT_ERROR_NO_SHS_REQD,
				    0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);
		rmnet_shs_crit_err[RMNET_SHS_MAIN_SHS_NOT_REQD]++;
		return;
	}

	trace_rmnet_shs_high(RMNET_SHS_ASSIGN, RMNET_SHS_ASSIGN_START,
			     0xDEF, 0xDEF, 0xDEF, 0xDEF, skb, NULL);

	hash = skb_get_hash(skb);

	/*  Using do while to spin lock and unlock only once */
	spin_lock_irqsave(&rmnet_shs_ht_splock, ht_flags);
	do {
		hash_for_each_possible_safe(RMNET_SHS_HT, node_p, tmp, list,
					    skb->hash) {
			if (skb->hash != node_p->hash)
				continue;

			/* Return saved cpu assignment if an entry found*/
			if ((node_p->map_index >= map->len) ||
			    ((!node_p->hstats) &&
			     (node_p->hstats->rps_config_msk !=
				 rmnet_shs_wq_get_dev_rps_msk(dev)))) {

				map_cpu = rmnet_shs_new_flow_cpu(brate, dev);
				node_p->map_cpu = map_cpu;
				node_p->map_index =
				rmnet_shs_map_idx_from_cpu(map_cpu, map);

				trace_rmnet_shs_err(RMNET_SHS_ASSIGN,
						    RMNET_SHS_ASSIGN_MASK_CHNG,
						    0xDEF, 0xDEF, 0xDEF, 0xDEF,
						    NULL, NULL);
			}

			trace_rmnet_shs_low(RMNET_SHS_ASSIGN,
				RMNET_SHS_ASSIGN_MATCH_FLOW_COMPLETE,
				0xDEF, 0xDEF, 0xDEF, 0xDEF, skb, NULL);

			node_p->num_skb += 1;
			node_p->num_skb_bytes += skb->len;
			cpu_map_index = node_p->map_index;
			rmnet_shs_chain_to_skb_list(skb, node_p);
			is_match_found = 1;
			is_shs_reqd = 1;

		}
		if (is_match_found)
			break;

		/* We haven't found a hash match upto this point
		 */
		new_cpu = rmnet_shs_new_flow_cpu(brate, dev);
		if (new_cpu < 0)
			break;

		node_p = kzalloc(sizeof(*node_p), 0);

		if (!node_p) {
			rmnet_shs_crit_err[RMNET_SHS_MAIN_MALLOC_ERR]++;
			break;
		}

		node_p->dev = skb->dev;
		node_p->hash = skb->hash;
		node_p->map_cpu = new_cpu;
		cpu_map_index = rmnet_shs_map_idx_from_cpu(node_p->map_cpu,
							   map);
		INIT_LIST_HEAD(&node_p->node_id);
		rmnet_shs_get_update_skb_proto(skb, node_p);
		rmnet_shs_wq_inc_cpu_flow(map->cpus[cpu_map_index]);
		node_p->map_index = cpu_map_index;
		node_p->map_cpu = map->cpus[cpu_map_index];
		node_p->dev = skb->dev;
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
	spin_unlock_irqrestore(&rmnet_shs_ht_splock, ht_flags);

	if (!is_shs_reqd) {
		rmnet_shs_crit_err[RMNET_SHS_MAIN_SHS_NOT_REQD]++;
		rmnet_shs_deliver_skb(skb);
		trace_rmnet_shs_err(RMNET_SHS_ASSIGN,
				    RMNET_SHS_ASSIGN_CRIT_ERROR_NO_SHS_REQD,
				    0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);
		return;
	}

	if (!rmnet_shs_cfg.is_reg_dl_mrk_ind) {
		rmnet_map_dl_ind_register(port, &rmnet_shs_cfg.dl_mrk_ind_cb);
		qmi_rmnet_ps_ind_register(port,
					  &rmnet_shs_cfg.rmnet_idl_ind_cb);

		rmnet_shs_cfg.is_reg_dl_mrk_ind = 1;
		shs_delayed_work.port = port;

	}
	/* We got the first packet after a previous successdul flush. Arm the
	 * flushing timer.
	 */
	if (!rmnet_shs_cfg.is_pkt_parked) {
		rmnet_shs_cfg.is_pkt_parked = 1;
		rmnet_shs_cfg.force_flush_state = RMNET_SHS_FLUSH_OFF;
		if (hrtimer_active(&rmnet_shs_cfg.hrtimer_shs)) {
			trace_rmnet_shs_low(RMNET_SHS_ASSIGN,
				    RMNET_SHS_ASSIGN_PARK_TMR_CANCEL,
				    RMNET_SHS_FORCE_FLUSH_TIME_NSEC,
				    0xDEF, 0xDEF, 0xDEF, skb, NULL);
			hrtimer_cancel(&rmnet_shs_cfg.hrtimer_shs);
		}
		hrtimer_start(&rmnet_shs_cfg.hrtimer_shs,
			      ns_to_ktime(2000000), HRTIMER_MODE_REL);
		trace_rmnet_shs_low(RMNET_SHS_ASSIGN,
				    RMNET_SHS_ASSIGN_PARK_TMR_START,
				    RMNET_SHS_FORCE_FLUSH_TIME_NSEC,
				    0xDEF, 0xDEF, 0xDEF, skb, NULL);
	}

	if (rmnet_shs_cfg.num_pkts_parked >
						rmnet_shs_pkts_store_limit) {

		if (rmnet_shs_stats_enabled)
			rmnet_shs_flush_reason[RMNET_SHS_FLUSH_PKT_LIMIT]++;

		trace_rmnet_shs_high(RMNET_SHS_FLUSH,
				     RMNET_SHS_FLUSH_PKT_LIMIT_TRIGGER, 0,
				     0xDEF, 0xDEF, 0xDEF, NULL, NULL);
		rmnet_shs_flush_table(1);

	} else if (rmnet_shs_cfg.num_bytes_parked >
						rmnet_shs_byte_store_limit) {

		if (rmnet_shs_stats_enabled)
			rmnet_shs_flush_reason[RMNET_SHS_FLUSH_BYTE_LIMIT]++;

		trace_rmnet_shs_high(RMNET_SHS_FLUSH,
				     RMNET_SHS_FLUSH_BYTE_LIMIT_TRIGGER, 0,
				     0xDEF, 0xDEF, 0xDEF, NULL, NULL);
		rmnet_shs_flush_table(1);

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
		trace_rmnet_shs_high(RMNET_SHS_FLUSH,
				     RMNET_SHS_FLUSH_FORCE_TRIGGER, 1,
				     rmnet_shs_cfg.num_pkts_parked,
				     0xDEF, 0xDEF, NULL, NULL);
		rmnet_shs_flush_table(0);

	}

}

/* Cancels the flushing timer if it has been armed
 * Deregisters DL marker indications
 */
void rmnet_shs_exit(void)
{
	qmi_rmnet_ps_ind_deregister(rmnet_shs_cfg.port,
				    &rmnet_shs_cfg.rmnet_idl_ind_cb);

	rmnet_shs_cfg.dl_mrk_ind_cb.dl_hdr_handler = NULL;
	rmnet_shs_cfg.dl_mrk_ind_cb.dl_trl_handler = NULL;
	rmnet_map_dl_ind_deregister(rmnet_shs_cfg.port,
				    &rmnet_shs_cfg.dl_mrk_ind_cb);
	rmnet_shs_cfg.is_reg_dl_mrk_ind = 0;
	if (rmnet_shs_cfg.is_timer_init)
		hrtimer_cancel(&rmnet_shs_cfg.hrtimer_shs);

	memset(&rmnet_shs_cfg, 0, sizeof(rmnet_shs_cfg));
	rmnet_shs_init_complete = 0;

}
