/* Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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
 * RMNET Data Smart Hash Workqueue solution
 *
 */

#include "rmnet_shs.h"
#include "rmnet_shs_wq_genl.h"
#include "rmnet_shs_wq_mem.h"
#include <linux/workqueue.h>
#include <linux/list_sort.h>
#include <net/sock.h>
#include <linux/skbuff.h>

MODULE_LICENSE("GPL v2");
/* Local Macros */
#define RMNET_SHS_RX_BPNSEC_TO_BPSEC(x) ((x)*1000000000)
#define RMNET_SHS_SEC_TO_NSEC(x) ((x)*1000000000)
#define RMNET_SHS_NSEC_TO_SEC(x) ((x)/1000000000)
#define RMNET_SHS_BYTE_TO_BIT(x) ((x)*8)
#define RMNET_SHS_MIN_HSTAT_NODES_REQD 16
#define RMNET_SHS_FILTER_PKT_LIMIT 200
#define RMNET_SHS_FILTER_FLOW_RATE 100

#define PERIODIC_CLEAN 0
/* FORCE_CLEAN should only used during module de-init.*/
#define FORCE_CLEAN 1

/* Local Definitions and Declarations */
unsigned int rmnet_shs_cpu_prio_dur __read_mostly = 3;
module_param(rmnet_shs_cpu_prio_dur, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_cpu_prio_dur, "Priority ignore duration (wq intervals)");

#define PRIO_BACKOFF ((!rmnet_shs_cpu_prio_dur) ? 2 : rmnet_shs_cpu_prio_dur)

unsigned int rmnet_shs_wq_interval_ms __read_mostly = RMNET_SHS_WQ_INTERVAL_MS;
module_param(rmnet_shs_wq_interval_ms, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_wq_interval_ms, "Interval between wq runs (ms)");

unsigned long rmnet_shs_max_flow_inactivity_sec __read_mostly =
						RMNET_SHS_MAX_SKB_INACTIVE_TSEC;
module_param(rmnet_shs_max_flow_inactivity_sec, ulong, 0644);
MODULE_PARM_DESC(rmnet_shs_max_flow_inactivity_sec,
		 "Max flow inactive time before clean up");

unsigned int rmnet_shs_wq_tuning __read_mostly = 80;
module_param(rmnet_shs_wq_tuning, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_wq_tuning, "moving average weightage");

unsigned long long rmnet_shs_cpu_rx_max_pps_thresh[MAX_CPUS]__read_mostly = {
			 RMNET_SHS_UDP_PPS_LPWR_CPU_UTHRESH,
			 RMNET_SHS_UDP_PPS_LPWR_CPU_UTHRESH,
			 RMNET_SHS_UDP_PPS_LPWR_CPU_UTHRESH,
			 RMNET_SHS_UDP_PPS_LPWR_CPU_UTHRESH,
			 RMNET_SHS_UDP_PPS_PERF_CPU_UTHRESH,
			 RMNET_SHS_UDP_PPS_PERF_CPU_UTHRESH,
			 RMNET_SHS_UDP_PPS_PERF_CPU_UTHRESH,
			 RMNET_SHS_UDP_PPS_PERF_CPU_UTHRESH
			};
module_param_array(rmnet_shs_cpu_rx_max_pps_thresh, ullong, 0, 0644);
MODULE_PARM_DESC(rmnet_shs_cpu_rx_max_pps_thresh, "Max pkts core can handle");

unsigned long long rmnet_shs_cpu_rx_min_pps_thresh[MAX_CPUS]__read_mostly = {
			 RMNET_SHS_UDP_PPS_LPWR_CPU_LTHRESH,
			 RMNET_SHS_UDP_PPS_LPWR_CPU_LTHRESH,
			 RMNET_SHS_UDP_PPS_LPWR_CPU_LTHRESH,
			 RMNET_SHS_UDP_PPS_LPWR_CPU_LTHRESH,
			 RMNET_SHS_UDP_PPS_PERF_CPU_LTHRESH,
			 RMNET_SHS_UDP_PPS_PERF_CPU_LTHRESH,
			 RMNET_SHS_UDP_PPS_PERF_CPU_LTHRESH,
			 RMNET_SHS_UDP_PPS_PERF_CPU_LTHRESH
			};
module_param_array(rmnet_shs_cpu_rx_min_pps_thresh, ullong, 0, 0644);
MODULE_PARM_DESC(rmnet_shs_cpu_rx_min_pps_thresh, "Min pkts core can handle");

unsigned int rmnet_shs_cpu_rx_flows[MAX_CPUS];
module_param_array(rmnet_shs_cpu_rx_flows, uint, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_cpu_rx_flows, "Num flows processed per core");

unsigned int rmnet_shs_cpu_rx_filter_flows[MAX_CPUS];
module_param_array(rmnet_shs_cpu_rx_filter_flows, uint, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_cpu_rx_filter_flows, "Num filtered flows per core");

unsigned long long rmnet_shs_cpu_rx_bytes[MAX_CPUS];
module_param_array(rmnet_shs_cpu_rx_bytes, ullong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_cpu_rx_bytes, "SHS stamp bytes per CPU");

unsigned long long rmnet_shs_cpu_rx_pkts[MAX_CPUS];
module_param_array(rmnet_shs_cpu_rx_pkts, ullong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_cpu_rx_pkts, "SHS stamp total pkts per CPU");

unsigned long long rmnet_shs_cpu_rx_bps[MAX_CPUS];
module_param_array(rmnet_shs_cpu_rx_bps, ullong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_cpu_rx_bps, "SHS stamp enq rate per CPU");

unsigned long long rmnet_shs_cpu_rx_pps[MAX_CPUS];
module_param_array(rmnet_shs_cpu_rx_pps, ullong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_cpu_rx_pps, "SHS stamp pkt enq rate per CPU");

unsigned long long rmnet_shs_cpu_qhead_diff[MAX_CPUS];
module_param_array(rmnet_shs_cpu_qhead_diff, ullong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_cpu_qhead_diff, "SHS nw stack queue processed diff");

unsigned long long rmnet_shs_cpu_qhead_total[MAX_CPUS];
module_param_array(rmnet_shs_cpu_qhead_total, ullong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_cpu_qhead_total, "SHS nw stack queue processed total");

unsigned long rmnet_shs_flow_hash[MAX_SUPPORTED_FLOWS_DEBUG];
module_param_array(rmnet_shs_flow_hash, ulong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_flow_hash, "SHS stamp hash flow");

unsigned long rmnet_shs_flow_proto[MAX_SUPPORTED_FLOWS_DEBUG];
module_param_array(rmnet_shs_flow_proto, ulong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_flow_proto, "SHS stamp hash transport protocol");

unsigned long long rmnet_shs_flow_inactive_tsec[MAX_SUPPORTED_FLOWS_DEBUG];
module_param_array(rmnet_shs_flow_inactive_tsec, ullong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_flow_inactive_tsec, "SHS stamp inactive flow time");

int rmnet_shs_flow_cpu[MAX_SUPPORTED_FLOWS_DEBUG] = {
			-1, -1, -1, -1, -1, -1, -1, -1,
			-1, -1, -1, -1, -1, -1, -1, -1};
module_param_array(rmnet_shs_flow_cpu, int, NULL, 0444);
MODULE_PARM_DESC(rmnet_shs_flow_cpu, "SHS stamp flow processing CPU");

int rmnet_shs_flow_cpu_recommended[MAX_SUPPORTED_FLOWS_DEBUG] = {
			 -1, -1, -1, -1, -1, -1, -1, -1,
			 -1, -1, -1, -1, -1, -1, -1, -1
			 };
module_param_array(rmnet_shs_flow_cpu_recommended, int, NULL, 0444);
MODULE_PARM_DESC(rmnet_shs_flow_cpu_recommended, "SHS stamp flow proc CPU");

unsigned long long rmnet_shs_flow_rx_bytes[MAX_SUPPORTED_FLOWS_DEBUG];
module_param_array(rmnet_shs_flow_rx_bytes, ullong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_flow_rx_bytes, "SHS stamp bytes per flow");

unsigned long long rmnet_shs_flow_rx_pkts[MAX_SUPPORTED_FLOWS_DEBUG];
module_param_array(rmnet_shs_flow_rx_pkts, ullong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_flow_rx_pkts, "SHS stamp total pkts per flow");

unsigned long long rmnet_shs_flow_rx_bps[MAX_SUPPORTED_FLOWS_DEBUG];
module_param_array(rmnet_shs_flow_rx_bps, ullong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_flow_rx_bps, "SHS stamp enq rate per flow");

unsigned long long rmnet_shs_flow_rx_pps[MAX_SUPPORTED_FLOWS_DEBUG];
module_param_array(rmnet_shs_flow_rx_pps, ullong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_flow_rx_pps, "SHS stamp pkt enq rate per flow");

/* Counters for suggestions made by wq */
unsigned long long rmnet_shs_flow_silver_to_gold[MAX_SUPPORTED_FLOWS_DEBUG];
module_param_array(rmnet_shs_flow_silver_to_gold, ullong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_flow_silver_to_gold, "SHS Suggest Silver to Gold");

unsigned long long rmnet_shs_flow_gold_to_silver[MAX_SUPPORTED_FLOWS_DEBUG];
module_param_array(rmnet_shs_flow_gold_to_silver, ullong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_flow_gold_to_silver, "SHS Suggest Gold to Silver");

unsigned long long rmnet_shs_flow_gold_balance[MAX_SUPPORTED_FLOWS_DEBUG];
module_param_array(rmnet_shs_flow_gold_balance, ullong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_flow_gold_balance, "SHS Suggest Gold Balance");

static DEFINE_SPINLOCK(rmnet_shs_hstat_tbl_lock);
static DEFINE_SPINLOCK(rmnet_shs_ep_lock);

static time_t rmnet_shs_wq_tnsec;
static struct workqueue_struct *rmnet_shs_wq;
static struct rmnet_shs_delay_wq_s *rmnet_shs_delayed_wq;
static struct rmnet_shs_wq_rx_flow_s rmnet_shs_rx_flow_tbl;

static struct list_head rmnet_shs_wq_hstat_tbl =
				LIST_HEAD_INIT(rmnet_shs_wq_hstat_tbl);
static int rmnet_shs_flow_dbg_stats_idx_cnt;
struct list_head rmnet_shs_wq_ep_tbl = LIST_HEAD_INIT(rmnet_shs_wq_ep_tbl);

/* Helper functions to add and remove entries to the table
 * that maintains a list of all endpoints (vnd's) available on this device.
 */
void rmnet_shs_wq_ep_tbl_add(struct rmnet_shs_wq_ep_s *ep)
{
	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_EP_TBL, RMNET_SHS_WQ_EP_TBL_ADD,
				0xDEF, 0xDEF, 0xDEF, 0xDEF, ep, NULL);
	list_add(&ep->ep_list_id, &rmnet_shs_wq_ep_tbl);
}

void rmnet_shs_wq_ep_tbl_remove(struct rmnet_shs_wq_ep_s *ep)
{
	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_EP_TBL, RMNET_SHS_WQ_EP_TBL_DEL,
				0xDEF, 0xDEF, 0xDEF, 0xDEF, ep, NULL);
	list_del_init(&ep->ep_list_id);
}

/* Helper functions to add and remove entries to the table
 * that maintains a list of all nodes that maintain statistics per flow
 */
void rmnet_shs_wq_hstat_tbl_add(struct rmnet_shs_wq_hstat_s *hnode)
{
	unsigned long flags;

	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_HSTAT_TBL,
			       RMNET_SHS_WQ_HSTAT_TBL_ADD,
				0xDEF, 0xDEF, 0xDEF, 0xDEF, hnode, NULL);
	spin_lock_irqsave(&rmnet_shs_hstat_tbl_lock, flags);
	list_add(&hnode->hstat_node_id, &rmnet_shs_wq_hstat_tbl);
	spin_unlock_irqrestore(&rmnet_shs_hstat_tbl_lock, flags);
}

void rmnet_shs_wq_hstat_tbl_remove(struct rmnet_shs_wq_hstat_s *hnode)
{
	unsigned long flags;

	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_HSTAT_TBL,
			       RMNET_SHS_WQ_HSTAT_TBL_DEL,
				0xDEF, 0xDEF, 0xDEF, 0xDEF, hnode, NULL);

	spin_lock_irqsave(&rmnet_shs_hstat_tbl_lock, flags);
	list_del_init(&hnode->hstat_node_id);
	spin_unlock_irqrestore(&rmnet_shs_hstat_tbl_lock, flags);

}

/* We maintain a list of all flow nodes processed by a cpu.
 * Below helper functions are used to maintain flow<=>cpu
 * association.*
 */
void rmnet_shs_wq_cpu_list_remove(struct rmnet_shs_wq_hstat_s *hnode)
{
	unsigned long flags;

	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_CPU_HSTAT_TBL,
			    RMNET_SHS_WQ_CPU_HSTAT_TBL_DEL,
			    0xDEF, 0xDEF, 0xDEF, 0xDEF, hnode, NULL);
	spin_lock_irqsave(&rmnet_shs_hstat_tbl_lock, flags);
	list_del_init(&hnode->cpu_node_id);
	spin_unlock_irqrestore(&rmnet_shs_hstat_tbl_lock, flags);

}

void rmnet_shs_wq_cpu_list_add(struct rmnet_shs_wq_hstat_s *hnode,
			    struct list_head *head)
{
	unsigned long flags;

	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_CPU_HSTAT_TBL,
			    RMNET_SHS_WQ_CPU_HSTAT_TBL_ADD,
			    0xDEF, 0xDEF, 0xDEF, 0xDEF, hnode, NULL);

	spin_lock_irqsave(&rmnet_shs_hstat_tbl_lock, flags);
	list_add(&hnode->cpu_node_id, head);
	spin_unlock_irqrestore(&rmnet_shs_hstat_tbl_lock, flags);
}

void rmnet_shs_wq_cpu_list_move(struct rmnet_shs_wq_hstat_s *hnode,
			     struct list_head *head)
{
	unsigned long flags;

	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_CPU_HSTAT_TBL,
			    RMNET_SHS_WQ_CPU_HSTAT_TBL_MOVE,
			    hnode->current_cpu,
			    0xDEF, 0xDEF, 0xDEF, hnode, NULL);
	spin_lock_irqsave(&rmnet_shs_hstat_tbl_lock, flags);
	list_move(&hnode->cpu_node_id, head);
	spin_unlock_irqrestore(&rmnet_shs_hstat_tbl_lock, flags);

}

/* Resets all the parameters used to maintain hash statistics */
void rmnet_shs_wq_hstat_reset_node(struct rmnet_shs_wq_hstat_s *hnode)
{
	hnode->c_epoch = 0;
	hnode->l_epoch = 0;
	hnode->node = NULL;
	hnode->inactive_duration = 0;
	hnode->rx_skb = 0;
	hnode->rx_bytes = 0;
	hnode->rx_pps = 0;
	hnode->rx_bps = 0;
	hnode->last_rx_skb = 0;
	hnode->last_rx_bytes = 0;
	hnode->rps_config_msk = 0;
	hnode->current_core_msk = 0;
	hnode->def_core_msk = 0;
	hnode->pri_core_msk = 0;
	hnode->available_core_msk = 0;
	hnode->hash = 0;
	hnode->suggested_cpu = 0;
	hnode->current_cpu = 0;
	hnode->skb_tport_proto = 0;
	hnode->stat_idx = -1;
	INIT_LIST_HEAD(&hnode->cpu_node_id);
	hnode->is_new_flow = 0;
	/* clear in use flag as a last action. This is required to ensure
	 * the same node does not get allocated until all the paramaeters
	 * are cleared.
	 */
	hnode->in_use = 0;
	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_HSTAT_TBL,
			    RMNET_SHS_WQ_HSTAT_TBL_NODE_RESET,
			    hnode->is_perm, 0xDEF, 0xDEF, 0xDEF, hnode, NULL);
}

/* Preallocates a set of flow nodes that maintain flow level statistics*/
void rmnet_shs_wq_hstat_alloc_nodes(u8 num_nodes_to_allocate, u8 is_store_perm)
{
	struct rmnet_shs_wq_hstat_s *hnode = NULL;

	while (num_nodes_to_allocate > 0) {
		hnode = kzalloc(sizeof(*hnode), GFP_ATOMIC);
		if (hnode) {
			hnode->is_perm = is_store_perm;
			rmnet_shs_wq_hstat_reset_node(hnode);
			INIT_LIST_HEAD(&hnode->hstat_node_id);
			INIT_LIST_HEAD(&hnode->cpu_node_id);
			rmnet_shs_wq_hstat_tbl_add(hnode);
		} else {
			rmnet_shs_crit_err[RMNET_SHS_WQ_ALLOC_HSTAT_ERR]++;
		}
		hnode = NULL;
		num_nodes_to_allocate--;
	}

}

/* If there is an already pre-allocated node available and not in use,
 * we will try to re-use them.
 */
struct rmnet_shs_wq_hstat_s *rmnet_shs_wq_get_new_hstat_node(void)
{
	struct rmnet_shs_wq_hstat_s *hnode = NULL;
	struct rmnet_shs_wq_hstat_s *ret_node = NULL;
	unsigned long flags;

	spin_lock_irqsave(&rmnet_shs_hstat_tbl_lock, flags);
	list_for_each_entry(hnode, &rmnet_shs_wq_hstat_tbl, hstat_node_id) {
		if (hnode == NULL)
			continue;

		if (hnode->in_use == 0) {
			ret_node = hnode;
			ret_node->in_use = 1;
			ret_node->is_new_flow = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&rmnet_shs_hstat_tbl_lock, flags);

	if (ret_node) {
		trace_rmnet_shs_wq_low(RMNET_SHS_WQ_HSTAT_TBL,
				    RMNET_SHS_WQ_HSTAT_TBL_NODE_REUSE,
				    hnode->is_perm, 0xDEF, 0xDEF, 0xDEF,
				    hnode, NULL);
		return ret_node;
	}

	/* We have reached a point where all pre-allocated nodes are in use
	 * Allocating memory to maintain the flow level stats for new flow.
	 * However, this newly allocated memory will be released as soon as we
	 * realize that this flow is inactive
	 */
	ret_node = kzalloc(sizeof(*hnode), GFP_ATOMIC);

	if (!ret_node) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_ALLOC_HSTAT_ERR]++;
		return NULL;
	}

	rmnet_shs_wq_hstat_reset_node(ret_node);
	ret_node->is_perm = 0;
	ret_node->in_use = 1;
	ret_node->is_new_flow = 1;
	INIT_LIST_HEAD(&ret_node->hstat_node_id);
	INIT_LIST_HEAD(&ret_node->cpu_node_id);

	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_HSTAT_TBL,
			    RMNET_SHS_WQ_HSTAT_TBL_NODE_DYN_ALLOCATE,
			    ret_node->is_perm, 0xDEF, 0xDEF, 0xDEF,
			    ret_node, NULL);

	rmnet_shs_wq_hstat_tbl_add(ret_node);

	return ret_node;
}

void rmnet_shs_wq_create_new_flow(struct rmnet_shs_skbn_s *node_p)
{
	struct timespec time;

	if (!node_p) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_PTR_ERR]++;
		return;
	}

	node_p->hstats = rmnet_shs_wq_get_new_hstat_node();
	if (node_p->hstats != NULL) {
		(void)getnstimeofday(&time);

		node_p->hstats->hash = node_p->hash;
		node_p->hstats->skb_tport_proto = node_p->skb_tport_proto;
		node_p->hstats->current_cpu = node_p->map_cpu;
		node_p->hstats->suggested_cpu = node_p->map_cpu;

		/* Start TCP flows with segmentation if userspace connected */
		if (rmnet_shs_userspace_connected &&
		    node_p->hstats->skb_tport_proto == IPPROTO_TCP)
			node_p->hstats->segment_enable = 1;

		node_p->hstats->node = node_p;
		node_p->hstats->c_epoch = RMNET_SHS_SEC_TO_NSEC(time.tv_sec) +
		   time.tv_nsec;
		node_p->hstats->l_epoch = RMNET_SHS_SEC_TO_NSEC(time.tv_sec) +
		   time.tv_nsec;
	}

	trace_rmnet_shs_wq_high(RMNET_SHS_WQ_HSTAT_TBL,
				RMNET_SHS_WQ_HSTAT_TBL_NODE_NEW_REQ,
				0xDEF, 0xDEF, 0xDEF, 0xDEF,
				node_p, node_p->hstats);
}


/* Compute the average pps for a flow based on tuning param
 * Often when we decide to switch from a small cluster core,
 * it is because of the heavy traffic on that core. In such
 * circumstances, we want to switch to a big cluster
 * core as soon as possible. Therefore, we will provide a
 * greater weightage to the most recent sample compared to
 * the previous samples.
 *
 * On the other hand, when a flow which is on a big cluster
 * cpu suddenly starts to receive low traffic we move to a
 * small cluster core after observing low traffic for some
 * more samples. This approach avoids switching back and forth
 * to small cluster cpus due to momentary decrease in data
 * traffic.
 */
static u64 rmnet_shs_wq_get_flow_avg_pps(struct rmnet_shs_wq_hstat_s *hnode)
{
	u64 avg_pps, mov_avg_pps;
	u16 new_weight, old_weight;

	if (!hnode) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_PTR_ERR]++;
		return 0;
	}

	if (rmnet_shs_is_lpwr_cpu(hnode->current_cpu)) {
		/* More weight to current value */
		new_weight = rmnet_shs_wq_tuning;
		old_weight = 100 - rmnet_shs_wq_tuning;
	} else {
		old_weight = rmnet_shs_wq_tuning;
		new_weight = 100 - rmnet_shs_wq_tuning;
	}

	/* computing weighted average per flow, if the flow has just started,
	 * there is no past values, so we use the current pps as the avg
	 */
	if (hnode->last_pps == 0) {
		avg_pps = hnode->rx_pps;
	} else {
		mov_avg_pps = (hnode->last_pps + hnode->avg_pps) / 2;
		avg_pps = (((new_weight * hnode->rx_pps) +
			    (old_weight * mov_avg_pps)) /
			    (new_weight + old_weight));
	}

	return avg_pps;
}

static u64 rmnet_shs_wq_get_cpu_avg_pps(u16 cpu_num)
{
	u64 avg_pps, mov_avg_pps;
	u16 new_weight, old_weight;
	struct rmnet_shs_wq_cpu_rx_pkt_q_s *cpu_node;
	struct rmnet_shs_wq_rx_flow_s *rx_flow_tbl_p = &rmnet_shs_rx_flow_tbl;

	if (cpu_num >= MAX_CPUS) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_CPU_ERR]++;
		return 0;
	}

	cpu_node = &rx_flow_tbl_p->cpu_list[cpu_num];

	if (rmnet_shs_is_lpwr_cpu(cpu_num)) {
		/* More weight to current value */
		new_weight = rmnet_shs_wq_tuning;
		old_weight = 100 - rmnet_shs_wq_tuning;
	} else {
		old_weight = rmnet_shs_wq_tuning;
		new_weight = 100 - rmnet_shs_wq_tuning;
	}

	/* computing weighted average per flow, if the cpu has not past values
	 * for pps, we use the current value as the average
	 */
	if (cpu_node->last_rx_pps == 0) {
		avg_pps = cpu_node->avg_pps;
	} else {
		mov_avg_pps = (cpu_node->last_rx_pps + cpu_node->avg_pps) / 2;
		avg_pps = (((new_weight * cpu_node->rx_pps) +
			    (old_weight * mov_avg_pps)) /
			    (new_weight + old_weight));
	}

	trace_rmnet_shs_wq_high(RMNET_SHS_WQ_CPU_STATS,
			   RMNET_SHS_WQ_CPU_STATS_CORE2SWITCH_EVAL_CPU,
			   cpu_num, cpu_node->rx_pps, cpu_node->last_rx_pps,
			   avg_pps, NULL, NULL);

	return avg_pps;
}

/* Refresh the RPS mask associated with this flow */
void rmnet_shs_wq_update_hstat_rps_msk(struct rmnet_shs_wq_hstat_s *hstat_p)
{
	struct rmnet_shs_skbn_s *node_p = NULL;
	struct rmnet_shs_wq_ep_s *ep = NULL;

	if (!hstat_p) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_PTR_ERR]++;
		return;
	}

	node_p = hstat_p->node;

	/*Map RPS mask from the endpoint associated with this flow*/
	list_for_each_entry(ep, &rmnet_shs_wq_ep_tbl, ep_list_id) {

		if (ep && (node_p->dev == ep->ep)) {
			hstat_p->rps_config_msk = ep->rps_config_msk;
			hstat_p->def_core_msk = ep->default_core_msk;
			hstat_p->pri_core_msk = ep->pri_core_msk;

			/* Update ep tput stats while we're here */
			if (hstat_p->skb_tport_proto == IPPROTO_TCP) {
				rm_err("SHS_UDP: adding TCP bps %lu to ep_total %lu ep name %s",
				       hstat_p->rx_bps, ep->tcp_rx_bps, node_p->dev->name);
				ep->tcp_rx_bps += hstat_p->rx_bps;
			} else if (hstat_p->skb_tport_proto == IPPROTO_UDP) {
				rm_err("SHS_UDP: adding UDP rx_bps %lu to ep_total %lu ep name %s",
				       hstat_p->rx_bps, ep->udp_rx_bps, node_p->dev->name);
				ep->udp_rx_bps += hstat_p->rx_bps;
			}
			break;
		}
	}
	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_FLOW_STATS,
				RMNET_SHS_WQ_FLOW_STATS_UPDATE_MSK,
				hstat_p->rps_config_msk,
				hstat_p->def_core_msk,
				hstat_p->pri_core_msk,
				0xDEF, hstat_p, node_p);
}

void rmnet_shs_wq_update_hash_stats_debug(struct rmnet_shs_wq_hstat_s *hstats_p,
					  struct rmnet_shs_skbn_s *node_p)
{
	int idx = rmnet_shs_flow_dbg_stats_idx_cnt;

	if (!rmnet_shs_stats_enabled)
		return;

	if (!hstats_p || !node_p) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_PTR_ERR]++;
		return;
	}

	if (hstats_p->stat_idx < 0) {
		idx = idx % MAX_SUPPORTED_FLOWS_DEBUG;
		hstats_p->stat_idx = idx;
		rmnet_shs_flow_dbg_stats_idx_cnt++;
	}

	rmnet_shs_flow_hash[hstats_p->stat_idx] = hstats_p->hash;
	rmnet_shs_flow_proto[hstats_p->stat_idx] = node_p->skb_tport_proto;
	rmnet_shs_flow_inactive_tsec[hstats_p->stat_idx] =
			RMNET_SHS_NSEC_TO_SEC(hstats_p->inactive_duration);
	rmnet_shs_flow_rx_bps[hstats_p->stat_idx] = hstats_p->rx_bps;
	rmnet_shs_flow_rx_pps[hstats_p->stat_idx] = hstats_p->rx_pps;
	rmnet_shs_flow_rx_bytes[hstats_p->stat_idx] = hstats_p->rx_bytes;
	rmnet_shs_flow_rx_pkts[hstats_p->stat_idx] = hstats_p->rx_skb;
	rmnet_shs_flow_cpu[hstats_p->stat_idx] = hstats_p->current_cpu;
	rmnet_shs_flow_cpu_recommended[hstats_p->stat_idx] =
						hstats_p->suggested_cpu;
	rmnet_shs_flow_silver_to_gold[hstats_p->stat_idx] =
		hstats_p->rmnet_shs_wq_suggs[RMNET_SHS_WQ_SUGG_SILVER_TO_GOLD];
	rmnet_shs_flow_gold_to_silver[hstats_p->stat_idx] =
		hstats_p->rmnet_shs_wq_suggs[RMNET_SHS_WQ_SUGG_GOLD_TO_SILVER];
	rmnet_shs_flow_gold_balance[hstats_p->stat_idx] =
		hstats_p->rmnet_shs_wq_suggs[RMNET_SHS_WQ_SUGG_GOLD_BALANCE];

}

/* Returns TRUE if this flow received a new packet
 *         FALSE otherwise
 */
u8 rmnet_shs_wq_is_hash_rx_new_pkt(struct rmnet_shs_wq_hstat_s *hstats_p,
				   struct rmnet_shs_skbn_s *node_p)
{
	if (!hstats_p || !node_p) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_PTR_ERR]++;
		return 0;
	}

	if (node_p->num_skb == hstats_p->rx_skb)
		return 0;

	return 1;
}

void rmnet_shs_wq_update_hash_tinactive(struct rmnet_shs_wq_hstat_s *hstats_p,
					struct rmnet_shs_skbn_s *node_p)
{
	time_t tdiff;

	if (!hstats_p || !node_p) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_PTR_ERR]++;
		return;
	}

	tdiff = rmnet_shs_wq_tnsec - hstats_p->c_epoch;
	hstats_p->inactive_duration = tdiff;

	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_FLOW_STATS,
				RMNET_SHS_WQ_FLOW_STATS_FLOW_INACTIVE,
				hstats_p->hash, tdiff, 0xDEF, 0xDEF,
				hstats_p, NULL);
}

void rmnet_shs_wq_update_hash_stats(struct rmnet_shs_wq_hstat_s *hstats_p)
{
	time_t tdiff;
	u64 skb_diff, bytes_diff;
	struct rmnet_shs_skbn_s *node_p;

	if (!hstats_p) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_PTR_ERR]++;
		return;
	}

	node_p = hstats_p->node;

	if (!rmnet_shs_wq_is_hash_rx_new_pkt(hstats_p, node_p)) {
		hstats_p->rx_pps = 0;
		hstats_p->avg_pps = 0;
		hstats_p->rx_bps = 0;
		rmnet_shs_wq_update_hash_tinactive(hstats_p, node_p);
		rmnet_shs_wq_update_hash_stats_debug(hstats_p, node_p);
		return;
	}

	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_FLOW_STATS,
				RMNET_SHS_WQ_FLOW_STATS_START,
				hstats_p->hash, 0xDEF, hstats_p->rx_pps,
				hstats_p->rx_bps, hstats_p, NULL);

	rmnet_shs_wq_update_hstat_rps_msk(hstats_p);
	hstats_p->inactive_duration = 0;
	hstats_p->l_epoch = node_p->hstats->c_epoch;
	hstats_p->last_rx_skb = node_p->hstats->rx_skb;
	hstats_p->last_rx_bytes = node_p->hstats->rx_bytes;

	hstats_p->c_epoch = rmnet_shs_wq_tnsec;
	hstats_p->rx_skb = node_p->num_skb;
	hstats_p->rx_bytes = node_p->num_skb_bytes;

	tdiff = (hstats_p->c_epoch - hstats_p->l_epoch);
	skb_diff = hstats_p->rx_skb - hstats_p->last_rx_skb;
	bytes_diff = hstats_p->rx_bytes - hstats_p->last_rx_bytes;

	hstats_p->rx_pps = RMNET_SHS_RX_BPNSEC_TO_BPSEC(skb_diff)/(tdiff);
	hstats_p->rx_bps = RMNET_SHS_RX_BPNSEC_TO_BPSEC(bytes_diff)/(tdiff);
	hstats_p->rx_bps = RMNET_SHS_BYTE_TO_BIT(hstats_p->rx_bps);
	hstats_p->avg_pps = rmnet_shs_wq_get_flow_avg_pps(hstats_p);
	hstats_p->last_pps = hstats_p->rx_pps;
	rmnet_shs_wq_update_hash_stats_debug(hstats_p, node_p);

	trace_rmnet_shs_wq_high(RMNET_SHS_WQ_FLOW_STATS,
				RMNET_SHS_WQ_FLOW_STATS_END,
				hstats_p->hash, hstats_p->rx_pps,
				hstats_p->rx_bps, (tdiff/1000000),
				hstats_p, NULL);
}

static void rmnet_shs_wq_refresh_cpu_rates_debug(u16 cpu,
				struct rmnet_shs_wq_cpu_rx_pkt_q_s *cpu_p)
{
	if (!rmnet_shs_stats_enabled)
		return;

	if (cpu >= MAX_CPUS) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_CPU_ERR]++;
		return;
	}

	if (!cpu_p) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_PTR_ERR]++;
		return;
	}

	rmnet_shs_cpu_rx_bps[cpu] = cpu_p->rx_bps;
	rmnet_shs_cpu_rx_pps[cpu] = cpu_p->rx_pps;
	rmnet_shs_cpu_rx_flows[cpu] = cpu_p->flows;
	rmnet_shs_cpu_rx_bytes[cpu] = cpu_p->rx_bytes;
	rmnet_shs_cpu_rx_pkts[cpu] = cpu_p->rx_skbs;
	rmnet_shs_cpu_qhead_diff[cpu] = cpu_p->qhead_diff;
	rmnet_shs_cpu_qhead_total[cpu] = cpu_p->qhead_total;
}

static void rmnet_shs_wq_refresh_dl_mrkr_stats(void)
{
	struct rmnet_shs_wq_rx_flow_s *tbl_p = &rmnet_shs_rx_flow_tbl;
	struct rmnet_port *port;
	u64 pkt_diff, byte_diff;
	time_t tdiff;

	tbl_p->dl_mrk_last_rx_bytes = tbl_p->dl_mrk_rx_bytes;
	tbl_p->dl_mrk_last_rx_pkts = tbl_p->dl_mrk_rx_pkts;

	port = rmnet_shs_cfg.port;
	if (!port) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_GET_RMNET_PORT_ERR]++;
		return;
	}
	tbl_p->dl_mrk_rx_pkts = port->stats.dl_hdr_total_pkts;
	tbl_p->dl_mrk_rx_bytes = port->stats.dl_hdr_total_bytes;
	tdiff = rmnet_shs_wq_tnsec - tbl_p->l_epoch;
	pkt_diff = tbl_p->dl_mrk_rx_pkts - tbl_p->dl_mrk_last_rx_pkts;
	byte_diff = tbl_p->dl_mrk_rx_bytes - tbl_p->dl_mrk_last_rx_bytes;
	tbl_p->dl_mrk_rx_pps = RMNET_SHS_RX_BPNSEC_TO_BPSEC(pkt_diff)/tdiff;
	tbl_p->dl_mrk_rx_bps = RMNET_SHS_RX_BPNSEC_TO_BPSEC(byte_diff)/tdiff;
	tbl_p->dl_mrk_rx_bps = RMNET_SHS_BYTE_TO_BIT(tbl_p->dl_mrk_rx_bps);

}

static void rmnet_shs_wq_refresh_total_stats(void)
{
	struct rmnet_shs_wq_rx_flow_s *tbl_p = &rmnet_shs_rx_flow_tbl;
	u64 pkt_diff, byte_diff, pps, bps;
	time_t tdiff;

	tdiff = rmnet_shs_wq_tnsec - tbl_p->l_epoch;
	pkt_diff = (tbl_p->rx_skbs -  tbl_p->last_rx_skbs);
	byte_diff = tbl_p->rx_bytes -  tbl_p->last_rx_bytes;
	pps = RMNET_SHS_RX_BPNSEC_TO_BPSEC(pkt_diff)/tdiff;
	bps = RMNET_SHS_RX_BPNSEC_TO_BPSEC(byte_diff)/tdiff;
	tbl_p->last_rx_bps = tbl_p->rx_bps;
	tbl_p->last_rx_pps = tbl_p->rx_pps;
	tbl_p->rx_bps = RMNET_SHS_BYTE_TO_BIT(bps);
	tbl_p->rx_pps = pps;
	tbl_p->l_epoch  = rmnet_shs_wq_tnsec;
	tbl_p->last_rx_bytes = tbl_p->rx_bytes;
	tbl_p->last_rx_skbs = tbl_p->rx_skbs;

	trace_rmnet_shs_wq_high(RMNET_SHS_WQ_TOTAL_STATS,
				RMNET_SHS_WQ_TOTAL_STATS_UPDATE,
				tbl_p->rx_pps,
				tbl_p->dl_mrk_rx_pps,
				tbl_p->rx_bps,
				tbl_p->dl_mrk_rx_bps, NULL, NULL);

}

static void rmnet_shs_wq_refresh_cpu_stats(u16 cpu)
{
	struct rmnet_shs_wq_cpu_rx_pkt_q_s *cpu_p;
	time_t tdiff;
	u64 new_skbs, new_bytes;
	u64 last_rx_bps, last_rx_pps;
	u32 new_qhead;

	if (cpu >= MAX_CPUS) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_CPU_ERR]++;
		return;
	}

	cpu_p = &rmnet_shs_rx_flow_tbl.cpu_list[cpu];
	new_skbs = cpu_p->rx_skbs - cpu_p->last_rx_skbs;

	new_qhead = rmnet_shs_get_cpu_qhead(cpu);
	if (cpu_p->qhead_start == 0)
		cpu_p->qhead_start = new_qhead;

	cpu_p->last_qhead = cpu_p->qhead;
	cpu_p->qhead = new_qhead;
	cpu_p->qhead_diff = cpu_p->qhead - cpu_p->last_qhead;
	cpu_p->qhead_total = cpu_p->qhead - cpu_p->qhead_start;

	if (rmnet_shs_cpu_node_tbl[cpu].wqprio)
		rmnet_shs_cpu_node_tbl[cpu].wqprio = (rmnet_shs_cpu_node_tbl[cpu].wqprio + 1)
						     % (PRIO_BACKOFF);
	if (new_skbs == 0) {
		cpu_p->l_epoch =  rmnet_shs_wq_tnsec;
		cpu_p->rx_bps = 0;
		cpu_p->rx_pps = 0;
		cpu_p->avg_pps = 0;
		if (rmnet_shs_userspace_connected) {
			rmnet_shs_wq_cpu_caps_list_add(&rmnet_shs_rx_flow_tbl,
						       cpu_p, &cpu_caps);
		}
		rmnet_shs_wq_refresh_cpu_rates_debug(cpu, cpu_p);
		return;
	}

	tdiff = rmnet_shs_wq_tnsec - cpu_p->l_epoch;
	new_bytes = cpu_p->rx_bytes - cpu_p->last_rx_bytes;

	last_rx_bps = cpu_p->rx_bps;
	last_rx_pps = cpu_p->rx_pps;
	cpu_p->rx_pps = RMNET_SHS_RX_BPNSEC_TO_BPSEC(new_skbs)/tdiff;
	cpu_p->rx_bps = RMNET_SHS_RX_BPNSEC_TO_BPSEC(new_bytes)/tdiff;
	cpu_p->rx_bps = RMNET_SHS_BYTE_TO_BIT(cpu_p->rx_bps);
	cpu_p->avg_pps = rmnet_shs_wq_get_cpu_avg_pps(cpu);
	cpu_p->last_rx_bps = last_rx_bps;
	cpu_p->last_rx_pps = last_rx_pps;

	cpu_p->l_epoch =  rmnet_shs_wq_tnsec;
	cpu_p->last_rx_skbs = cpu_p->rx_skbs;
	cpu_p->last_rx_bytes = cpu_p->rx_bytes;
	cpu_p->rx_bps_est = cpu_p->rx_bps;

	if (rmnet_shs_userspace_connected) {
		rmnet_shs_wq_cpu_caps_list_add(&rmnet_shs_rx_flow_tbl,
					       cpu_p, &cpu_caps);
	}

	trace_rmnet_shs_wq_high(RMNET_SHS_WQ_CPU_STATS,
				RMNET_SHS_WQ_CPU_STATS_UPDATE, cpu,
				cpu_p->flows, cpu_p->rx_pps,
				cpu_p->rx_bps, NULL, NULL);
	rmnet_shs_wq_refresh_cpu_rates_debug(cpu, cpu_p);

}

static void rmnet_shs_wq_refresh_all_cpu_stats(void)
{
	u16 cpu;

	trace_rmnet_shs_wq_high(RMNET_SHS_WQ_CPU_STATS,
				RMNET_SHS_WQ_CPU_STATS_START,
				0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);

	for (cpu = 0; cpu < MAX_CPUS; cpu++)
		rmnet_shs_wq_refresh_cpu_stats(cpu);

	trace_rmnet_shs_wq_high(RMNET_SHS_WQ_CPU_STATS,
				RMNET_SHS_WQ_CPU_STATS_END,
				0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);
}

void rmnet_shs_wq_update_cpu_rx_tbl(struct rmnet_shs_wq_hstat_s *hstat_p)
{
	struct rmnet_shs_wq_rx_flow_s *tbl_p = &rmnet_shs_rx_flow_tbl;
	struct rmnet_shs_skbn_s *node_p;
	u64 skb_diff, byte_diff;
	u16 cpu_num;

	if (!hstat_p) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_PTR_ERR]++;
		return;
	}

	node_p = hstat_p->node;

	if (hstat_p->inactive_duration > 0)
		return;

	cpu_num = node_p->map_cpu;

	if (cpu_num >= MAX_CPUS) {
		rmnet_shs_crit_err[RMNET_SHS_INVALID_CPU_ERR]++;
		return;
	}
	skb_diff = hstat_p->rx_skb - hstat_p->last_rx_skb;
	byte_diff = hstat_p->rx_bytes - hstat_p->last_rx_bytes;

	if (hstat_p->is_new_flow) {
		rmnet_shs_wq_cpu_list_add(hstat_p,
				       &tbl_p->cpu_list[cpu_num].hstat_id);
		rm_err("SHS_FLOW: adding flow 0x%x on cpu[%d] "
		       "pps: %llu | avg_pps %llu",
		       hstat_p->hash, hstat_p->current_cpu,
		       hstat_p->rx_pps, hstat_p->avg_pps);
		hstat_p->is_new_flow = 0;
	}
	/* check if the flow has switched to another CPU*/
	if (cpu_num != hstat_p->current_cpu) {
		rm_err("SHS_FLOW: moving flow 0x%x on cpu[%d] to cpu[%d] "
		       "pps: %llu | avg_pps %llu",
		       hstat_p->hash, hstat_p->current_cpu, cpu_num,
		       hstat_p->rx_pps, hstat_p->avg_pps);
		trace_rmnet_shs_wq_high(RMNET_SHS_WQ_FLOW_STATS,
					RMNET_SHS_WQ_FLOW_STATS_UPDATE_NEW_CPU,
					hstat_p->hash, hstat_p->current_cpu,
					cpu_num, 0xDEF, hstat_p, NULL);

		rmnet_shs_wq_cpu_list_move(hstat_p,
				   &tbl_p->cpu_list[cpu_num].hstat_id);

		rmnet_shs_wq_inc_cpu_flow(cpu_num);
		rmnet_shs_wq_dec_cpu_flow(hstat_p->current_cpu);
		hstat_p->current_cpu = cpu_num;
	}

	/* Assuming that the data transfers after the last refresh
	 * interval have happened with the newer CPU
	 */
	tbl_p->cpu_list[cpu_num].rx_skbs += skb_diff;
	tbl_p->cpu_list[cpu_num].rx_bytes += byte_diff;
	tbl_p->rx_skbs += skb_diff;
	tbl_p->rx_bytes += byte_diff;

}

void rmnet_shs_wq_chng_suggested_cpu(u16 old_cpu, u16 new_cpu,
					      struct rmnet_shs_wq_ep_s *ep)
{
	struct rmnet_shs_skbn_s *node_p;
	struct rmnet_shs_wq_hstat_s *hstat_p;
	u16 bkt;

	hash_for_each(RMNET_SHS_HT, bkt, node_p, list) {
		if (!node_p)
			continue;

		if (!node_p->hstats)
			continue;

		hstat_p = node_p->hstats;

		if ((hstat_p->suggested_cpu == old_cpu) &&
		    (node_p->dev == ep->ep)) {

			trace_rmnet_shs_wq_high(RMNET_SHS_WQ_FLOW_STATS,
				RMNET_SHS_WQ_FLOW_STATS_SUGGEST_NEW_CPU,
				hstat_p->hash, hstat_p->suggested_cpu,
				new_cpu, 0xDEF, hstat_p, NULL);

			node_p->hstats->suggested_cpu = new_cpu;
		}
	}
}

/* Increment the per-flow counter for suggestion type */
static void rmnet_shs_wq_inc_sugg_type(u32 sugg_type,
				       struct rmnet_shs_wq_hstat_s *hstat_p)
{
	if (sugg_type >= RMNET_SHS_WQ_SUGG_MAX || hstat_p == NULL)
		return;

	hstat_p->rmnet_shs_wq_suggs[sugg_type] += 1;
}

/* Change suggested cpu, return 1 if suggestion was made, 0 otherwise */
static int rmnet_shs_wq_chng_flow_cpu(u16 old_cpu, u16 new_cpu,
				      struct rmnet_shs_wq_ep_s *ep,
				      u32 hash_to_move, u32 sugg_type)
{
	struct rmnet_shs_skbn_s *node_p;
	struct rmnet_shs_wq_hstat_s *hstat_p;
	int rc = 0;
	u16 bkt;

	if (!ep) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_EP_ACCESS_ERR]++;
		return 0;
	}

	if (old_cpu >= MAX_CPUS || new_cpu >= MAX_CPUS) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_CPU_ERR]++;
		return 0;
	}

	hash_for_each(RMNET_SHS_HT, bkt, node_p, list) {
		if (!node_p)
			continue;

		if (!node_p->hstats)
			continue;

		hstat_p = node_p->hstats;

		if (hash_to_move != 0) {
			/* If hash_to_move is given, only move that flow,
			 * otherwise move all the flows on that cpu
			 */
			if (hstat_p->hash != hash_to_move)
				continue;
		}

		rm_err("SHS_HT: >>  sugg cpu %d | old cpu %d | new_cpu %d | "
		       "map_cpu = %d | flow 0x%x",
		       hstat_p->suggested_cpu, old_cpu, new_cpu,
		       node_p->map_cpu, hash_to_move);

		if ((hstat_p->suggested_cpu == old_cpu) &&
		    (node_p->dev == ep->ep)) {

			trace_rmnet_shs_wq_high(RMNET_SHS_WQ_FLOW_STATS,
				RMNET_SHS_WQ_FLOW_STATS_SUGGEST_NEW_CPU,
				hstat_p->hash, hstat_p->suggested_cpu,
				new_cpu, 0xDEF, hstat_p, NULL);

			node_p->hstats->suggested_cpu = new_cpu;
			rmnet_shs_wq_inc_sugg_type(sugg_type, hstat_p);
			if (hash_to_move) { /* Stop after moving one flow */
				rm_err("SHS_CHNG: moving single flow: flow 0x%x "
				       "sugg_cpu changed from %d to %d",
				       hstat_p->hash, old_cpu,
				       node_p->hstats->suggested_cpu);
				return 1;
			}
			rm_err("SHS_CHNG: moving all flows: flow 0x%x "
			       "sugg_cpu changed from %d to %d",
			       hstat_p->hash, old_cpu,
			       node_p->hstats->suggested_cpu);
			rc |= 1;
		}
	}
	return rc;
}

u64 rmnet_shs_wq_get_max_pps_among_cores(u32 core_msk)
{
	int cpu_num;
	u64 max_pps = 0;
	struct rmnet_shs_wq_rx_flow_s *rx_flow_tbl_p = &rmnet_shs_rx_flow_tbl;

	for (cpu_num = 0; cpu_num < MAX_CPUS; cpu_num++) {
		if (((1 << cpu_num) & core_msk) &&
		     (rx_flow_tbl_p->cpu_list[cpu_num].rx_pps > max_pps)) {
			max_pps = rx_flow_tbl_p->cpu_list[cpu_num].rx_pps;
		}
	}
	return max_pps;
}

/* Returns the least utilized core from a core mask
 * In order of priority
 *    1) Returns leftmost core with no flows (Fully Idle)
 *    2) Returns the core with least flows with no pps (Semi Idle)
 *    3) Returns the core with the least pps (Non-Idle)
 */
int rmnet_shs_wq_get_least_utilized_core(u16 core_msk)
{
	struct rmnet_shs_wq_rx_flow_s *rx_flow_tbl_p = &rmnet_shs_rx_flow_tbl;
	struct rmnet_shs_wq_cpu_rx_pkt_q_s *list_p;
	u64 min_pps = U64_MAX;
	u32 min_flows = U32_MAX;
	int ret_val = -1;
	int semi_idle_ret = -1;
	int full_idle_ret = -1;
	int cpu_num = 0;
	u16 is_cpu_in_msk;

	for (cpu_num = 0; cpu_num < MAX_CPUS; cpu_num++) {

		is_cpu_in_msk = (1 << cpu_num) & core_msk;
		if (!is_cpu_in_msk)
			continue;

		list_p = &rx_flow_tbl_p->cpu_list[cpu_num];
		trace_rmnet_shs_wq_low(RMNET_SHS_WQ_CPU_STATS,
				       RMNET_SHS_WQ_CPU_STATS_CURRENT_UTIL,
				       cpu_num, list_p->rx_pps, min_pps,
				       0, NULL, NULL);

		/* When there are multiple free CPUs the first free CPU will
		 * be returned
		 */
		if (list_p->flows == 0) {
			full_idle_ret = cpu_num;
			break;
		}
		/* When there are semi-idle CPUs the CPU w/ least flows will
		 * be returned
		 */
		if (list_p->rx_pps == 0 && list_p->flows < min_flows) {
			min_flows = list_p->flows;
			semi_idle_ret = cpu_num;
		}

		/* Found a core that is processing even lower packets */
		if (list_p->rx_pps <= min_pps) {
			min_pps = list_p->rx_pps;
			ret_val = cpu_num;
		}
	}

	if (full_idle_ret >= 0)
		ret_val = full_idle_ret;
	else if (semi_idle_ret >= 0)
		ret_val = semi_idle_ret;

	return ret_val;
}

u16 rmnet_shs_wq_find_cpu_to_move_flows(u16 current_cpu,
					struct rmnet_shs_wq_ep_s *ep)
{
	struct rmnet_shs_wq_rx_flow_s *rx_flow_tbl_p = &rmnet_shs_rx_flow_tbl;
	struct rmnet_shs_wq_cpu_rx_pkt_q_s *cpu_list_p, *cur_cpu_list_p;
	u64 cpu_rx_pps, reqd_pps, cur_cpu_rx_pps;
	u64 pps_uthresh, pps_lthresh = 0;
	u16 cpu_to_move = current_cpu;
	u16 cpu_num;
	u8 is_core_in_msk;
	u32 cpu_to_move_util = 0;

	if (!ep) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_EP_ACCESS_ERR]++;
		return cpu_to_move;
	}

	cur_cpu_list_p = &rx_flow_tbl_p->cpu_list[current_cpu];
	cur_cpu_rx_pps = cur_cpu_list_p->rx_pps;
	pps_uthresh = rmnet_shs_cpu_rx_max_pps_thresh[current_cpu];
	pps_lthresh = rmnet_shs_cpu_rx_min_pps_thresh[current_cpu];

	/* If we are already on a perf core and required pps is beyond
	 * beyond the capacity that even perf cores aren't sufficient
	 * there is nothing much we can do. So we will continue to let flows
	 * process packets on same perf core
	 */
	if (!rmnet_shs_is_lpwr_cpu(current_cpu) &&
	    (cur_cpu_rx_pps > pps_lthresh)) {
		return cpu_to_move;
	}
	/* If a core (should only be lpwr was marked prio we don't touch it
	 * for a few ticks and reset it afterwards
	 */

	if (rmnet_shs_cpu_node_tbl[current_cpu].wqprio)
		return current_cpu;

	for (cpu_num = 0; cpu_num < MAX_CPUS; cpu_num++) {

		is_core_in_msk = ((1 << cpu_num) & ep->rps_config_msk);

		/* We are looking for a core that is configured and that
		 * can handle traffic better than the current core
		 */
		if ((cpu_num == current_cpu) || (!is_core_in_msk) ||
		    !cpu_online(current_cpu))
			continue;

		pps_uthresh = rmnet_shs_cpu_rx_max_pps_thresh[cpu_num];
		pps_lthresh = rmnet_shs_cpu_rx_min_pps_thresh[cpu_num];

		cpu_list_p = &rx_flow_tbl_p->cpu_list[cpu_num];
		cpu_rx_pps = cpu_list_p->rx_pps;
		reqd_pps = cpu_rx_pps + cur_cpu_rx_pps;

		trace_rmnet_shs_wq_low(RMNET_SHS_WQ_CPU_STATS,
				       RMNET_SHS_WQ_CPU_STATS_CORE2SWITCH_FIND,
				       current_cpu, cpu_num, reqd_pps,
				       cpu_rx_pps, NULL, NULL);

		/* Return the most available valid CPU */
		if ((reqd_pps > pps_lthresh) && (reqd_pps < pps_uthresh) &&
			cpu_rx_pps <= cpu_to_move_util) {
			cpu_to_move = cpu_num;
			cpu_to_move_util = cpu_rx_pps;
		}
	}

	trace_rmnet_shs_wq_high(RMNET_SHS_WQ_CPU_STATS,
			     RMNET_SHS_WQ_CPU_STATS_CORE2SWITCH_FIND,
			     current_cpu, cpu_to_move, cur_cpu_rx_pps,
			     rx_flow_tbl_p->cpu_list[cpu_to_move].rx_pps,
			     NULL, NULL);
	return cpu_to_move;
}

void rmnet_shs_wq_find_cpu_and_move_flows(u16 cur_cpu)
{
	struct rmnet_shs_wq_ep_s *ep = NULL;
	u16 new_cpu;

	list_for_each_entry(ep, &rmnet_shs_wq_ep_tbl, ep_list_id) {
		if (!ep)
			continue;

		if (!ep->is_ep_active)
			continue;

		new_cpu = rmnet_shs_wq_find_cpu_to_move_flows(cur_cpu, ep);

		if (new_cpu != cur_cpu)
			rmnet_shs_wq_chng_suggested_cpu(cur_cpu, new_cpu, ep);
	}
}

/* Return 1 if we can move a flow to dest_cpu for this endpoint,
 * otherwise return 0. Basically check rps mask and cpu is online
 * Also check that dest cpu is not isolated
 */
int rmnet_shs_wq_check_cpu_move_for_ep(u16 current_cpu, u16 dest_cpu,
				       struct rmnet_shs_wq_ep_s *ep)
{
	u16 cpu_in_rps_mask = 0;

	if (!ep) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_EP_ACCESS_ERR]++;
		return 0;
	}

	if (current_cpu >= MAX_CPUS || dest_cpu >= MAX_CPUS) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_CPU_ERR]++;
		return 0;
	}

	cpu_in_rps_mask = (1 << dest_cpu) & ep->rps_config_msk;

	rm_err("SHS_MASK:  cur cpu [%d] | dest_cpu [%d] | "
	       "cpu isolation_mask = 0x%x | ep_rps_mask = 0x%x | "
	       "cpu_online(dest) = %d cpu_in_rps_mask = %d | "
	       "cpu isolated(dest) = %d",
	       current_cpu, dest_cpu, __cpu_isolated_mask, ep->rps_config_msk,
	       cpu_online(dest_cpu), cpu_in_rps_mask, cpu_isolated(dest_cpu));

	/* We cannot move to dest cpu if the cur cpu is the same,
	 * the dest cpu is offline, dest cpu is not in the rps mask,
	 * or if the dest cpu is isolated
	 */
	if (current_cpu == dest_cpu || !cpu_online(dest_cpu) ||
	    !cpu_in_rps_mask || cpu_isolated(dest_cpu)) {
		return 0;
	}

	return 1;
}

/* rmnet_shs_wq_try_to_move_flow - try to make a flow suggestion
 * return 1 if flow move was suggested, otherwise return 0
 */
int rmnet_shs_wq_try_to_move_flow(u16 cur_cpu, u16 dest_cpu, u32 hash_to_move,
				  u32 sugg_type)
{
	unsigned long flags;
	struct rmnet_shs_wq_ep_s *ep;

	if (cur_cpu >= MAX_CPUS || dest_cpu >= MAX_CPUS) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_CPU_ERR]++;
		return 0;
	}

	/* Traverse end-point list, check if cpu can be used, based
	 * on it if is online, rps mask, isolation, etc. then make
	 * suggestion to change the cpu for the flow by passing its hash
	 */
	spin_lock_irqsave(&rmnet_shs_ep_lock, flags);
	list_for_each_entry(ep, &rmnet_shs_wq_ep_tbl, ep_list_id) {
		if (!ep)
			continue;

		if (!ep->is_ep_active)
			continue;

		if (!rmnet_shs_wq_check_cpu_move_for_ep(cur_cpu,
							dest_cpu,
							ep)) {
			rm_err("SHS_FDESC: >> Cannot move flow 0x%x on ep"
			       " from cpu[%d] to cpu[%d]",
			       hash_to_move, cur_cpu, dest_cpu);
			continue;
		}

		if (rmnet_shs_wq_chng_flow_cpu(cur_cpu, dest_cpu, ep,
					       hash_to_move, sugg_type)) {
			rm_err("SHS_FDESC: >> flow 0x%x was suggested to"
			       " move from cpu[%d] to cpu[%d] sugg_type [%d]",
			       hash_to_move, cur_cpu, dest_cpu, sugg_type);

			spin_unlock_irqrestore(&rmnet_shs_ep_lock, flags);
			return 1;
		}
	}

	spin_unlock_irqrestore(&rmnet_shs_ep_lock, flags);
	return 0;
}

/* Change flow segmentation, return 1 if set, 0 otherwise */
int rmnet_shs_wq_set_flow_segmentation(u32 hash_to_set, u8 seg_enable)
{
	struct rmnet_shs_skbn_s *node_p;
	struct rmnet_shs_wq_hstat_s *hstat_p;
	unsigned long ht_flags;
	u16 bkt;

	spin_lock_irqsave(&rmnet_shs_ht_splock, ht_flags);
	hash_for_each(RMNET_SHS_HT, bkt, node_p, list) {
		if (!node_p)
			continue;

		if (!node_p->hstats)
			continue;

		hstat_p = node_p->hstats;

		if (hstat_p->hash != hash_to_set)
			continue;

		rm_err("SHS_HT: >> segmentation on hash 0x%x enable %u",
		       hash_to_set, seg_enable);

		trace_rmnet_shs_wq_high(RMNET_SHS_WQ_FLOW_STATS,
				RMNET_SHS_WQ_FLOW_STATS_SET_FLOW_SEGMENTATION,
				hstat_p->hash, seg_enable,
				0xDEF, 0xDEF, hstat_p, NULL);

		node_p->hstats->segment_enable = seg_enable;
		spin_unlock_irqrestore(&rmnet_shs_ht_splock, ht_flags);
		return 1;
	}
	spin_unlock_irqrestore(&rmnet_shs_ht_splock, ht_flags);

	rm_err("SHS_HT: >> segmentation on hash 0x%x enable %u not set - hash not found",
	       hash_to_set, seg_enable);
	return 0;
}


/* Comparison function to sort gold flow loads - based on flow avg_pps
 * return -1 if a is before b, 1 if a is after b, 0 if equal
 */
int cmp_fn_flow_pps(void *priv, struct list_head *a, struct list_head *b)
{
	struct rmnet_shs_wq_gold_flow_s *flow_a;
	struct rmnet_shs_wq_gold_flow_s *flow_b;

	if (!a || !b)
		return 0;

	flow_a = list_entry(a, struct rmnet_shs_wq_gold_flow_s, gflow_list);
	flow_b = list_entry(b, struct rmnet_shs_wq_gold_flow_s, gflow_list);

	if (flow_a->avg_pps > flow_b->avg_pps)
		return -1;
	else if (flow_a->avg_pps < flow_b->avg_pps)
		return 1;

	return 0;
}

/* Comparison function to sort cpu capacities - based on cpu avg_pps capacity
 * return -1 if a is before b, 1 if a is after b, 0 if equal
 */
int cmp_fn_cpu_pps(void *priv, struct list_head *a, struct list_head *b)
{
	struct rmnet_shs_wq_cpu_cap_s *cpu_a;
	struct rmnet_shs_wq_cpu_cap_s *cpu_b;

	if (!a || !b)
		return 0;

	cpu_a = list_entry(a, struct rmnet_shs_wq_cpu_cap_s, cpu_cap_list);
	cpu_b = list_entry(b, struct rmnet_shs_wq_cpu_cap_s, cpu_cap_list);

	if (cpu_a->avg_pps_capacity > cpu_b->avg_pps_capacity)
		return -1;
	else if (cpu_a->avg_pps_capacity < cpu_b->avg_pps_capacity)
		return 1;

	return 0;
}


/* Prints cpu stats and flows to dmesg for debugging */
void rmnet_shs_wq_debug_print_flows(void)
{
	struct rmnet_shs_wq_rx_flow_s *rx_flow_tbl_p = &rmnet_shs_rx_flow_tbl;
	struct rmnet_shs_wq_cpu_rx_pkt_q_s *cpu_node;
	struct rmnet_shs_wq_hstat_s *hnode;
	int flows, i;
	u16 cpu_num = 0;

	if (!RMNET_SHS_DEBUG)
		return;

	for (cpu_num = 0; cpu_num < MAX_CPUS; cpu_num++) {
		cpu_node = &rx_flow_tbl_p->cpu_list[cpu_num];
		flows = rx_flow_tbl_p->cpu_list[cpu_num].flows;

		rm_err("SHS_CPU: cpu[%d]: flows=%d pps=%llu bps=%llu "
		       "qhead_diff %u qhead_total = %u qhead_start = %u "
		       "qhead = %u qhead_last = %u isolated = %d ",
		       cpu_num, flows, cpu_node->rx_pps, cpu_node->rx_bps,
		       cpu_node->qhead_diff, cpu_node->qhead_total,
		       cpu_node->qhead_start,
		       cpu_node->qhead, cpu_node->last_qhead,
		       cpu_isolated(cpu_num));

		list_for_each_entry(hnode,
				    &rmnet_shs_wq_hstat_tbl,
				    hstat_node_id) {
			if (!hnode)
				continue;

			if (hnode->in_use == 0)
				continue;

			if (hnode->node) {
				if (hnode->current_cpu == cpu_num)
					rm_err("SHS_CPU:         > flow 0x%x "
					       "with pps %llu avg_pps %llu rx_bps %llu ",
					       hnode->hash, hnode->rx_pps,
					       hnode->avg_pps, hnode->rx_bps);
			}
		} /* loop per flow */

		for (i = 0; i < 3 - flows; i++) {
			rm_err("%s", "SHS_CPU:         > ");
		}
	} /* loop per cpu */
}

/* Prints the sorted gold flow list to dmesg */
void rmnet_shs_wq_debug_print_sorted_gold_flows(struct list_head *gold_flows)
{
	struct rmnet_shs_wq_gold_flow_s *gflow_node;

	if (!RMNET_SHS_DEBUG)
		return;

	if (!gold_flows) {
		rm_err("%s", "SHS_GDMA: Gold Flows List is NULL");
		return;
	}

	rm_err("%s", "SHS_GDMA: List of sorted gold flows:");
	list_for_each_entry(gflow_node, gold_flows, gflow_list) {
		if (!gflow_node)
			continue;

		rm_err("SHS_GDMA: > flow 0x%x with pps %llu on cpu[%d]",
		       gflow_node->hash, gflow_node->rx_pps,
		       gflow_node->cpu_num);
	}
}

/* Userspace evaluation. we send userspace the response to the sync message
 * after we update shared memory. shsusr will send a netlink message if
 * flows should be moved around.
 */
void rmnet_shs_wq_eval_cpus_caps_and_flows(struct list_head *cpu_caps,
					   struct list_head *gold_flows,
					   struct list_head *ss_flows)
{
	if (!cpu_caps || !gold_flows || !ss_flows) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_PTR_ERR]++;
		return;
	}

	list_sort(NULL, cpu_caps, &cmp_fn_cpu_pps);
	list_sort(NULL, gold_flows, &cmp_fn_flow_pps);

	rmnet_shs_wq_mem_update_cached_cpu_caps(cpu_caps);
	rmnet_shs_wq_mem_update_cached_sorted_gold_flows(gold_flows);
	rmnet_shs_wq_mem_update_cached_sorted_ss_flows(ss_flows);
	rmnet_shs_wq_mem_update_cached_netdevs();

	rmnet_shs_genl_send_int_to_userspace_no_info(RMNET_SHS_SYNC_RESP_INT);

	trace_rmnet_shs_wq_high(RMNET_SHS_WQ_SHSUSR, RMNET_SHS_WQ_SHSUSR_SYNC_END,
				0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);
}

/* Default wq evaluation logic, use this if rmnet_shs_userspace_connected is 0 */
void rmnet_shs_wq_eval_suggested_cpu(void)

{
	struct rmnet_shs_wq_rx_flow_s *rx_flow_tbl_p = &rmnet_shs_rx_flow_tbl;
	struct rmnet_shs_wq_cpu_rx_pkt_q_s *cpu_list_p;
	u64 cpu_curr_pps, cpu_last_pps, last_avg_pps;
	u64 moving_avg_pps, avg_pps;
	u64 pps_uthresh, pps_lthresh = 0;
	u16 cpu_num, new_weight, old_weight;
	int flows;

	for (cpu_num = 0; cpu_num < MAX_CPUS; cpu_num++) {
		flows = rx_flow_tbl_p->cpu_list[cpu_num].flows;

		/* Nothing to evaluate if there is no traffic on this cpu */
		if (flows <= 0)
			continue;

		cpu_list_p = &rx_flow_tbl_p->cpu_list[cpu_num];
		cpu_curr_pps = cpu_list_p->rx_pps;
		cpu_last_pps = cpu_list_p->last_rx_pps;
		last_avg_pps = cpu_list_p->avg_pps;
		pps_uthresh = rmnet_shs_cpu_rx_max_pps_thresh[cpu_num];
		pps_lthresh = rmnet_shs_cpu_rx_min_pps_thresh[cpu_num];

		/* Often when we decide to switch from a small cluster core,
		 * it is because of the heavy traffic on that core. In such
		 * circumstances, we want to switch to a big cluster
		 * core as soon as possible. Therefore, we will provide a
		 * greater weightage to the most recent sample compared to
		 * the previous samples.
		 *
		 * On the other hand, when a flow which is on a big cluster
		 * cpu suddenly starts to receive low traffic we move to a
		 * small cluster core after observing low traffic for some
		 * more samples. This approach avoids switching back and forth
		 * to small cluster cpus due to momentary decrease in data
		 * traffic.
		 */
		if (rmnet_shs_is_lpwr_cpu(cpu_num)) {
			new_weight = rmnet_shs_wq_tuning;
			old_weight = 100 - rmnet_shs_wq_tuning;

		} else	{
			old_weight = rmnet_shs_wq_tuning;
			new_weight = 100 - rmnet_shs_wq_tuning;

		}

		/*computing weighted average*/
		moving_avg_pps = (cpu_last_pps + last_avg_pps) / 2;
		avg_pps = ((new_weight * cpu_curr_pps) +
			   (old_weight * moving_avg_pps)) /
			   (new_weight + old_weight);

		cpu_list_p->avg_pps = avg_pps;

		trace_rmnet_shs_wq_high(RMNET_SHS_WQ_CPU_STATS,
				   RMNET_SHS_WQ_CPU_STATS_CORE2SWITCH_EVAL_CPU,
				   cpu_num, cpu_curr_pps, cpu_last_pps,
				   avg_pps, NULL, NULL);

		if ((avg_pps > pps_uthresh) ||
		    ((avg_pps < pps_lthresh) && (cpu_curr_pps < pps_lthresh)))
			rmnet_shs_wq_find_cpu_and_move_flows(cpu_num);
	}

}

void rmnet_shs_wq_refresh_new_flow_list_per_ep(struct rmnet_shs_wq_ep_s *ep)
{
	int lo_core;
	int hi_core;
	u16 rps_msk;
	u16 lo_msk;
	u16 hi_msk;
	u8 lo_core_idx = 0;
	u8 hi_core_idx = 0;

	if (!ep) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_EP_ACCESS_ERR]++;
		return;
	}

	rps_msk = ep->rps_config_msk;
	lo_msk = ep->default_core_msk;
	hi_msk = ep->pri_core_msk;
	memset(ep->new_lo_core, -1, sizeof(*ep->new_lo_core) * MAX_CPUS);
	memset(ep->new_hi_core, -1, sizeof(*ep->new_hi_core) * MAX_CPUS);
	do {
		lo_core = rmnet_shs_wq_get_least_utilized_core(lo_msk);
		if (lo_core >= 0) {
			ep->new_lo_core[lo_core_idx] = lo_core;
			lo_msk = lo_msk & ~(1 << lo_core);
			lo_core_idx++;
		} else {
			break;
		}

	} while (lo_msk != 0);

		trace_rmnet_shs_wq_low(RMNET_SHS_WQ_CPU_STATS,
			    RMNET_SHS_WQ_CPU_STATS_NEW_FLOW_LIST_LO,
			    ep->new_lo_core[0], ep->new_lo_core[1],
			    ep->new_lo_core[2], ep->new_lo_max,
			    ep, NULL);

	do {
		hi_core = rmnet_shs_wq_get_least_utilized_core(hi_msk);
		if (hi_core >= 0) {
			ep->new_hi_core[hi_core_idx] = hi_core;
			hi_msk = hi_msk & ~(1 << hi_core);
			hi_core_idx++;
		} else
			break;

	} while (hi_msk != 0);

	ep->new_lo_max = lo_core_idx;
	ep->new_hi_max = hi_core_idx;
	ep->new_lo_idx = 0;
	ep->new_hi_idx = 0;

	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_CPU_STATS,
			    RMNET_SHS_WQ_CPU_STATS_NEW_FLOW_LIST_HI,
			    ep->new_hi_core[0], ep->new_hi_core[1],
			    ep->new_hi_core[2], ep->new_hi_max,
			    ep, NULL);

	return;

}
void rmnet_shs_wq_refresh_new_flow_list(void)
{
	struct rmnet_shs_wq_ep_s *ep = NULL;

	list_for_each_entry(ep, &rmnet_shs_wq_ep_tbl, ep_list_id) {
		if (!ep)
			continue;
		if (!ep->is_ep_active)
			continue;

		rmnet_shs_wq_refresh_new_flow_list_per_ep(ep);
	}
}

/* Return Invalid core if only pri core available*/
int rmnet_shs_wq_get_lpwr_cpu_new_flow(struct net_device *dev)
{
	u8 lo_idx;
	u8 lo_max;
	int cpu_assigned = -1;
	u8 is_match_found = 0;
	struct rmnet_shs_wq_ep_s *ep = NULL;
	unsigned long flags;

	if (!dev) {
		rmnet_shs_crit_err[RMNET_SHS_NETDEV_ERR]++;
		return cpu_assigned;
	}

	spin_lock_irqsave(&rmnet_shs_ep_lock, flags);
	list_for_each_entry(ep, &rmnet_shs_wq_ep_tbl, ep_list_id) {
		if (!ep)
			continue;
		if (!ep->is_ep_active)
			continue;

		if (ep->ep == dev) {
			is_match_found = 1;
			break;
		}

	}

	if (!is_match_found) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_EP_ACCESS_ERR]++;
		spin_unlock_irqrestore(&rmnet_shs_ep_lock, flags);
		return cpu_assigned;
	}

	lo_idx = ep->new_lo_idx;
	lo_max = ep->new_lo_max;

	while (lo_idx < lo_max) {
		if (ep->new_lo_core[lo_idx] >= 0) {
			cpu_assigned = ep->new_lo_core[lo_idx];
			break;
		}
		lo_idx++;
	}

	/* Increment CPU assignment idx to be ready for next flow assignment*/
	if ((cpu_assigned >= 0) || ((ep->new_lo_idx + 1) >= ep->new_lo_max))
		ep->new_lo_idx = ((ep->new_lo_idx + 1) % ep->new_lo_max);
	spin_unlock_irqrestore(&rmnet_shs_ep_lock, flags);

	return cpu_assigned;
}

int rmnet_shs_wq_get_perf_cpu_new_flow(struct net_device *dev)
{
	struct rmnet_shs_wq_ep_s *ep = NULL;
	int cpu_assigned = -1;
	u8 hi_idx;
	u8 hi_max;
	u8 is_match_found = 0;
	unsigned long flags;

	if (!dev) {
		rmnet_shs_crit_err[RMNET_SHS_NETDEV_ERR]++;
		return cpu_assigned;
	}

	spin_lock_irqsave(&rmnet_shs_ep_lock, flags);
	list_for_each_entry(ep, &rmnet_shs_wq_ep_tbl, ep_list_id) {
		if (!ep)
			continue;

		if (!ep->is_ep_active)
			continue;

		if (ep->ep == dev) {
			is_match_found = 1;
			break;
		}
	}

	if (!is_match_found) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_EP_ACCESS_ERR]++;
		spin_unlock_irqrestore(&rmnet_shs_ep_lock, flags);
		return cpu_assigned;
	}

	hi_idx = ep->new_hi_idx;
	hi_max = ep->new_hi_max;

	while (hi_idx < hi_max) {
		if (ep->new_hi_core[hi_idx] >= 0) {
			cpu_assigned = ep->new_hi_core[hi_idx];
			break;
		}
		hi_idx++;
	}
	/* Increment CPU assignment idx to be ready for next flow assignment*/
	if (cpu_assigned >= 0)
		ep->new_hi_idx = ((hi_idx + 1) % hi_max);
	spin_unlock_irqrestore(&rmnet_shs_ep_lock, flags);

	return cpu_assigned;
}


static int rmnet_shs_wq_time_check(time_t time, int num_flows)
{

	int ret = false;

	if (time > rmnet_shs_max_flow_inactivity_sec)
		ret = true;
	else if (num_flows > FLOW_LIMIT2 && time > INACTIVE_TSEC2)
		ret = true;
	else if (num_flows > FLOW_LIMIT1 && time > INACTIVE_TSEC1)
		ret = true;

	return ret;
}

void rmnet_shs_wq_cleanup_hash_tbl(u8 force_clean)
{
	struct rmnet_shs_skbn_s *node_p = NULL;
	time_t tns2s;
	unsigned long ht_flags;
	struct rmnet_shs_wq_hstat_s *hnode = NULL;
	struct list_head *ptr = NULL, *next = NULL;

	list_for_each_safe(ptr, next, &rmnet_shs_wq_hstat_tbl) {
		hnode = list_entry(ptr,
				   struct rmnet_shs_wq_hstat_s, hstat_node_id);
		if (hnode == NULL)
			continue;

		if (hnode->node == NULL)
			continue;

		node_p = hnode->node;
		tns2s = RMNET_SHS_NSEC_TO_SEC(hnode->inactive_duration);

		/* Flows are cleanup from book keeping faster if
		 * there are a lot of active flows already in memory
		 */

		if (rmnet_shs_wq_time_check(tns2s, rmnet_shs_cfg.num_flows) ||
		    force_clean) {

			trace_rmnet_shs_wq_low(RMNET_SHS_WQ_FLOW_STATS,
			    RMNET_SHS_WQ_FLOW_STATS_FLOW_INACTIVE_TIMEOUT,
			    node_p->hash, tns2s, 0xDEF, 0xDEF, node_p, hnode);

			spin_lock_irqsave(&rmnet_shs_ht_splock, ht_flags);
			rmnet_shs_clear_node(node_p, RMNET_WQ_CTXT);
			rmnet_shs_wq_dec_cpu_flow(hnode->current_cpu);
			if (node_p) {
				rmnet_shs_cpu_node_remove(node_p);
				hash_del_rcu(&node_p->list);
				kfree(node_p);
			}
			rm_err("SHS_FLOW: removing flow 0x%x on cpu[%d] "
			       "pps: %llu avg_pps: %llu",
			       hnode->hash, hnode->current_cpu,
			       hnode->rx_pps, hnode->avg_pps);
			rmnet_shs_wq_cpu_list_remove(hnode);
			if (hnode->is_perm == 0 || force_clean) {
				rmnet_shs_wq_hstat_tbl_remove(hnode);
				kfree(hnode);
			} else {
				rmnet_shs_wq_hstat_reset_node(hnode);
			}
			rmnet_shs_cfg.num_flows--;
			spin_unlock_irqrestore(&rmnet_shs_ht_splock, ht_flags);
		}
	}

}

void rmnet_shs_wq_update_ep_rps_msk(struct rmnet_shs_wq_ep_s *ep)
{
	struct rps_map *map;
	u8 len = 0;

	if (!ep || !ep->ep ) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_EP_ACCESS_ERR]++;
		return;
	}

	rcu_read_lock();
	if (!ep->ep) {
		pr_info(" rmnet_shs invalid state %p", ep->ep);
		rmnet_shs_crit_err[RMNET_SHS_WQ_EP_ACCESS_ERR]++;
		return;
	}
	map = rcu_dereference(ep->ep->_rx->rps_map);

	ep->rps_config_msk = 0;
	if (map != NULL) {
		for (len = 0; len < map->len; len++)
			ep->rps_config_msk |= (1 << map->cpus[len]);
	}
	rcu_read_unlock();

	ep->default_core_msk = ep->rps_config_msk & 0x0F;
	ep->pri_core_msk = ep->rps_config_msk & 0xF0;
}

void rmnet_shs_wq_reset_ep_active(struct net_device *dev)
{
	struct rmnet_shs_wq_ep_s *ep = NULL;
	struct rmnet_shs_wq_ep_s *tmp = NULL;
	unsigned long flags;

	if (!dev) {
		rmnet_shs_crit_err[RMNET_SHS_NETDEV_ERR]++;
		return;
	}

	spin_lock_irqsave(&rmnet_shs_ep_lock, flags);
	list_for_each_entry_safe(ep, tmp, &rmnet_shs_wq_ep_tbl, ep_list_id) {
		if (!ep)
			continue;

		if (ep->ep == dev){
			ep->is_ep_active = 0;
			rmnet_shs_wq_ep_tbl_remove(ep);
			kfree(ep);
			break;
		}
	}

	spin_unlock_irqrestore(&rmnet_shs_ep_lock, flags);
}

void rmnet_shs_wq_set_ep_active(struct net_device *dev)
{
	struct rmnet_shs_wq_ep_s *ep = NULL;
	unsigned long flags;

	if (!dev) {
		rmnet_shs_crit_err[RMNET_SHS_NETDEV_ERR]++;
		return;
	}

	spin_lock_irqsave(&rmnet_shs_ep_lock, flags);

	ep = kzalloc(sizeof(*ep), GFP_ATOMIC);

	if (!ep) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_ALLOC_EP_TBL_ERR]++;
		spin_unlock_irqrestore(&rmnet_shs_ep_lock, flags);
		return;
	}
	ep->ep = dev;
	ep->is_ep_active = 1;

	INIT_LIST_HEAD(&ep->ep_list_id);
	rmnet_shs_wq_update_ep_rps_msk(ep);
	rmnet_shs_wq_ep_tbl_add(ep);

	spin_unlock_irqrestore(&rmnet_shs_ep_lock, flags);
}

void rmnet_shs_wq_refresh_ep_masks(void)
{
	struct rmnet_shs_wq_ep_s *ep = NULL;

	list_for_each_entry(ep, &rmnet_shs_wq_ep_tbl, ep_list_id) {

		if (!ep)
			continue;

		if (!ep->is_ep_active)
			continue;
		rmnet_shs_wq_update_ep_rps_msk(ep);

		/* These tput totals get re-added as we go through each flow */
		ep->udp_rx_bps = 0;
		ep->tcp_rx_bps = 0;

	}
}

void rmnet_shs_update_cfg_mask(void)
{
	/* Start with most avaible mask all eps could share*/
	u8 mask = UPDATE_MASK;
	u8 rps_enabled = 0;
	struct rmnet_shs_wq_ep_s *ep;

	list_for_each_entry(ep, &rmnet_shs_wq_ep_tbl, ep_list_id) {

		if (!ep->is_ep_active)
			continue;
		/* Bitwise and to get common mask from non-null masks.
		 * VNDs with different mask  will have UNDEFINED behavior
		 */
		if (ep->rps_config_msk) {
			mask &= ep->rps_config_msk;
			rps_enabled = 1;
		}
	}

	if (!rps_enabled) {
		rmnet_shs_cfg.map_mask = 0;
		rmnet_shs_cfg.map_len = 0;
		return;
        } else if (rmnet_shs_cfg.map_mask != mask) {
		rmnet_shs_cfg.map_mask = mask;
		rmnet_shs_cfg.map_len = rmnet_shs_get_mask_len(mask);
	}
}

void rmnet_shs_wq_filter(void)
{
	int cpu, cur_cpu;
	int temp;
	struct rmnet_shs_wq_hstat_s *hnode = NULL;

	for (cpu = 0; cpu < MAX_CPUS; cpu++) {
		rmnet_shs_cpu_rx_filter_flows[cpu] = 0;
		rmnet_shs_cpu_node_tbl[cpu].seg = 0;
	}

	/* Filter out flows with low pkt count and
	 * mark CPUS with slowstart flows
	 */
	list_for_each_entry(hnode, &rmnet_shs_wq_hstat_tbl, hstat_node_id) {

		if (hnode->in_use == 0)
			continue;
		if (hnode->avg_pps > RMNET_SHS_FILTER_FLOW_RATE &&
		    hnode->rx_skb > RMNET_SHS_FILTER_PKT_LIMIT)
			if (hnode->current_cpu < MAX_CPUS){
				temp = hnode->current_cpu;
				rmnet_shs_cpu_rx_filter_flows[temp]++;
			}
		cur_cpu = hnode->current_cpu;
		if (cur_cpu >= MAX_CPUS) {
			continue;
		}

		if (hnode->node->hstats->segment_enable) {
			rmnet_shs_cpu_node_tbl[cur_cpu].seg++;
		}
	}
}

void rmnet_shs_wq_update_stats(void)
{
	struct timespec time;
	struct rmnet_shs_wq_hstat_s *hnode = NULL;

	(void) getnstimeofday(&time);
	rmnet_shs_wq_tnsec = RMNET_SHS_SEC_TO_NSEC(time.tv_sec) + time.tv_nsec;
	rmnet_shs_wq_refresh_ep_masks();
	rmnet_shs_update_cfg_mask();

	list_for_each_entry(hnode, &rmnet_shs_wq_hstat_tbl, hstat_node_id) {
		if (!hnode)
			continue;

		if (hnode->in_use == 0)
			continue;

		if (hnode->node) {
			rmnet_shs_wq_update_hash_stats(hnode);
			rmnet_shs_wq_update_cpu_rx_tbl(hnode);

			if (rmnet_shs_userspace_connected) {
				if (!rmnet_shs_is_lpwr_cpu(hnode->current_cpu)) {
					/* Add golds flows to list */
					rmnet_shs_wq_gflow_list_add(hnode, &gflows);
				}
				if (hnode->skb_tport_proto == IPPROTO_TCP) {
					rmnet_shs_wq_ssflow_list_add(hnode, &ssflows);
				}
			} else {
				/* Disable segmentation if userspace gets disconnected connected */
				hnode->node->hstats->segment_enable = 0;
			}
		}
	}
	rmnet_shs_wq_refresh_all_cpu_stats();
	rmnet_shs_wq_refresh_total_stats();
	rmnet_shs_wq_refresh_dl_mrkr_stats();

	if (rmnet_shs_userspace_connected) {
		rm_err("%s", "SHS_UPDATE: Userspace connected, relying on userspace evaluation");
		rmnet_shs_wq_eval_cpus_caps_and_flows(&cpu_caps, &gflows, &ssflows);
		rmnet_shs_wq_cleanup_gold_flow_list(&gflows);
		rmnet_shs_wq_cleanup_ss_flow_list(&ssflows);
		rmnet_shs_wq_cleanup_cpu_caps_list(&cpu_caps);
	} else {
		rm_err("%s", "SHS_UPDATE: shs userspace not connected, using default logic");
		rmnet_shs_wq_eval_suggested_cpu();
	}

	rmnet_shs_wq_refresh_new_flow_list();
	rmnet_shs_wq_filter();
}

void rmnet_shs_wq_process_wq(struct work_struct *work)
{
	unsigned long flags;
	unsigned long jiffies;

	trace_rmnet_shs_wq_high(RMNET_SHS_WQ_PROCESS_WQ,
				RMNET_SHS_WQ_PROCESS_WQ_START,
				0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);

	spin_lock_irqsave(&rmnet_shs_ep_lock, flags);
	rmnet_shs_wq_update_stats();
	spin_unlock_irqrestore(&rmnet_shs_ep_lock, flags);

        /*Invoke after both the locks are released*/
        rmnet_shs_wq_cleanup_hash_tbl(PERIODIC_CLEAN);
        rmnet_shs_wq_debug_print_flows();

	jiffies = msecs_to_jiffies(rmnet_shs_wq_interval_ms);

	queue_delayed_work(rmnet_shs_wq, &rmnet_shs_delayed_wq->wq,
			   jiffies);

	trace_rmnet_shs_wq_high(RMNET_SHS_WQ_PROCESS_WQ,
				RMNET_SHS_WQ_PROCESS_WQ_END,
				0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);
}

void rmnet_shs_wq_clean_ep_tbl(void)
{
	struct rmnet_shs_wq_ep_s *ep = NULL;
	struct list_head *ptr = NULL, *next = NULL;

	list_for_each_safe(ptr, next, &rmnet_shs_wq_ep_tbl) {
		ep = list_entry(ptr, struct rmnet_shs_wq_ep_s, ep_list_id);
		if (!ep)
			continue;

		trace_rmnet_shs_wq_high(RMNET_SHS_WQ_EP_TBL,
					RMNET_SHS_WQ_EP_TBL_CLEANUP,
					0xDEF, 0xDEF, 0xDEF, 0xDEF, ep, NULL);

		rmnet_shs_wq_ep_tbl_remove(ep);
		kfree(ep);
	}
}

void rmnet_shs_wq_exit(void)
{

	/*If Wq is not initialized, nothing to cleanup */
	if (!rmnet_shs_wq || !rmnet_shs_delayed_wq)
		return;

	rmnet_shs_wq_mem_deinit();
	rmnet_shs_genl_send_int_to_userspace_no_info(RMNET_SHS_SYNC_WQ_EXIT);

	trace_rmnet_shs_wq_high(RMNET_SHS_WQ_EXIT, RMNET_SHS_WQ_EXIT_START,
				   0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);

	cancel_delayed_work_sync(&rmnet_shs_delayed_wq->wq);
	drain_workqueue(rmnet_shs_wq);
	destroy_workqueue(rmnet_shs_wq);
	kfree(rmnet_shs_delayed_wq);

	rmnet_shs_delayed_wq = NULL;
	rmnet_shs_wq = NULL;
	rmnet_shs_wq_cleanup_hash_tbl(FORCE_CLEAN);
	rmnet_shs_wq_clean_ep_tbl();
	trace_rmnet_shs_wq_high(RMNET_SHS_WQ_EXIT, RMNET_SHS_WQ_EXIT_END,
				   0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);
}

void rmnet_shs_wq_init_cpu_rx_flow_tbl(void)
{
	u8 cpu_num;
	struct rmnet_shs_wq_cpu_rx_pkt_q_s *rx_flow_tbl_p;

	for (cpu_num = 0; cpu_num < MAX_CPUS; cpu_num++) {

		trace_rmnet_shs_wq_high(RMNET_SHS_WQ_CPU_HSTAT_TBL,
					RMNET_SHS_WQ_CPU_HSTAT_TBL_INIT,
					cpu_num, 0xDEF, 0xDEF, 0xDEF,
					NULL, NULL);

		rx_flow_tbl_p = &rmnet_shs_rx_flow_tbl.cpu_list[cpu_num];
		INIT_LIST_HEAD(&rx_flow_tbl_p->hstat_id);
		rx_flow_tbl_p->cpu_num = cpu_num;
	}

}

void rmnet_shs_wq_pause(void)
{
	int cpu;

	if (rmnet_shs_wq && rmnet_shs_delayed_wq)
		cancel_delayed_work_sync(&rmnet_shs_delayed_wq->wq);

	for (cpu = 0; cpu < MAX_CPUS; cpu++)
		rmnet_shs_cpu_rx_filter_flows[cpu] = 0;
}

void rmnet_shs_wq_restart(void)
{
	if (rmnet_shs_wq && rmnet_shs_delayed_wq)
		queue_delayed_work(rmnet_shs_wq, &rmnet_shs_delayed_wq->wq, 0);
}

void rmnet_shs_wq_init(struct net_device *dev)
{
	/*If the workqueue is already initialized we should not be
	 *initializing again
	 */
	if (rmnet_shs_wq)
		return;

	if (!dev) {
		rmnet_shs_crit_err[RMNET_SHS_NETDEV_ERR]++;
		return;
	}

	rmnet_shs_wq_mem_init();

	trace_rmnet_shs_wq_high(RMNET_SHS_WQ_INIT, RMNET_SHS_WQ_INIT_START,
				0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);
	rmnet_shs_wq = alloc_workqueue("rmnet_shs_wq",
					WQ_MEM_RECLAIM | WQ_CPU_INTENSIVE, 1);
	if (!rmnet_shs_wq) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_ALLOC_WQ_ERR]++;
		return;
	}

	rmnet_shs_delayed_wq = kmalloc(sizeof(struct rmnet_shs_delay_wq_s),
				       GFP_ATOMIC);

	if (!rmnet_shs_delayed_wq) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_ALLOC_DEL_WQ_ERR]++;
		rmnet_shs_wq_exit();
		return;
	}

	/*All hstat nodes allocated during Wq init will be held for ever*/
	rmnet_shs_wq_hstat_alloc_nodes(RMNET_SHS_MIN_HSTAT_NODES_REQD, 1);
	rmnet_shs_wq_init_cpu_rx_flow_tbl();
	INIT_DEFERRABLE_WORK(&rmnet_shs_delayed_wq->wq,
			     rmnet_shs_wq_process_wq);

	trace_rmnet_shs_wq_high(RMNET_SHS_WQ_INIT, RMNET_SHS_WQ_INIT_END,
				0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);
}

int rmnet_shs_wq_get_num_cpu_flows(u16 cpu)
{
	int flows = -1;

	if (cpu >= MAX_CPUS) {
		rmnet_shs_crit_err[RMNET_SHS_INVALID_CPU_ERR]++;
		return flows;
	}
	flows = rmnet_shs_rx_flow_tbl.cpu_list[cpu].flows;

	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_CPU_STATS,
			       RMNET_SHS_WQ_CPU_STATS_GET_CPU_FLOW,
				cpu, flows, 0xDEF, 0xDEF, NULL, NULL);

	return flows;
}

int rmnet_shs_wq_get_max_flows_per_core(void)
{
	u16 cpu;
	int max_flows = -1;
	int cpu_flows;

	for (cpu = 0; cpu < MAX_CPUS; cpu++) {
		cpu_flows = rmnet_shs_wq_get_num_cpu_flows(cpu);
		if (cpu_flows > max_flows)
			max_flows = cpu_flows;

	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_CPU_STATS,
			       RMNET_SHS_WQ_CPU_STATS_GET_MAX_CPU_FLOW,
				cpu, cpu_flows, max_flows, 0xDEF, NULL, NULL);
	}

	return max_flows;
}

int rmnet_shs_wq_get_max_flows_per_cluster(u16 cpu)
{
	u32 big_cluster_mask = 1<<4;
	u32 core_mask = 1;
	u16 start_core = 0;
	u16 end_core = 4;
	int max_flows = -1;
	int cpu_flows;

	if (cpu > MAX_CPUS) {
		rmnet_shs_crit_err[RMNET_SHS_INVALID_CPU_ERR]++;
		return max_flows;
	}

	core_mask <<= cpu;
	if (core_mask >= big_cluster_mask) {
		start_core = 4;
		end_core = MAX_CPUS;
	}

	for (start_core; start_core < end_core; start_core++) {
		cpu_flows = rmnet_shs_wq_get_num_cpu_flows(start_core);
		if (cpu_flows > max_flows)
			max_flows = cpu_flows;
	}

	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_CPU_STATS,
			       RMNET_SHS_WQ_CPU_STATS_MAX_FLOW_IN_CLUSTER,
			       start_core, end_core, cpu, max_flows,
			       NULL, NULL);
	return max_flows;
}

void rmnet_shs_wq_inc_cpu_flow(u16 cpu)
{
	if (cpu >= MAX_CPUS) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_CPU_ERR]++;
		return;
	}

	rmnet_shs_rx_flow_tbl.cpu_list[cpu].flows++;

	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_CPU_STATS,
			       RMNET_SHS_WQ_CPU_STATS_INC_CPU_FLOW,
			       cpu, rmnet_shs_rx_flow_tbl.cpu_list[cpu].flows,
			       0xDEF, 0xDEF, NULL, NULL);
}

void rmnet_shs_wq_dec_cpu_flow(u16 cpu)
{
	if (cpu >= MAX_CPUS) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_CPU_ERR]++;
		return;
	}

	if (rmnet_shs_rx_flow_tbl.cpu_list[cpu].flows > 0)
		rmnet_shs_rx_flow_tbl.cpu_list[cpu].flows--;

	trace_rmnet_shs_wq_low(RMNET_SHS_WQ_CPU_STATS,
			       RMNET_SHS_WQ_CPU_STATS_DEC_CPU_FLOW,
			       cpu, rmnet_shs_rx_flow_tbl.cpu_list[cpu].flows,
			       0xDEF, 0xDEF, NULL, NULL);
}

u64 rmnet_shs_wq_get_max_allowed_pps(u16 cpu)
{

	if (cpu >= MAX_CPUS) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_CPU_ERR]++;
		return 0;
	}

	return rmnet_shs_cpu_rx_max_pps_thresh[cpu];
}

void rmnet_shs_wq_ep_lock_bh(void)
{
	spin_lock_bh(&rmnet_shs_ep_lock);
}

void rmnet_shs_wq_ep_unlock_bh(void)
{
	spin_unlock_bh(&rmnet_shs_ep_lock);
}
