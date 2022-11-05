/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
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

#ifndef _RMNET_SHS_WQ_H_
#define _RMNET_SHS_WQ_H_

#include "rmnet_shs_config.h"
#include "rmnet_shs.h"

#define RMNET_SHS_DEBUG 0

#define rm_err(fmt, ...)  \
	do { if (RMNET_SHS_DEBUG) pr_err(fmt, __VA_ARGS__); } while (0)

#define MAX_SUPPORTED_FLOWS_DEBUG 16

#define RMNET_SHS_RX_BPNSEC_TO_BPSEC(x) ((x)*1000000000)
#define RMNET_SHS_SEC_TO_NSEC(x) ((x)*1000000000)
#define RMNET_SHS_NSEC_TO_SEC(x) ((x)/1000000000)
#define RMNET_SHS_BYTE_TO_BIT(x) ((x)*8)
#define RMNET_SHS_MIN_HSTAT_NODES_REQD 16
#define RMNET_SHS_WQ_INTERVAL_MS  100

extern unsigned long long rmnet_shs_cpu_rx_max_pps_thresh[MAX_CPUS]__read_mostly;
extern unsigned long long rmnet_shs_cpu_rx_min_pps_thresh[MAX_CPUS]__read_mostly;

extern struct list_head rmnet_shs_wq_ep_tbl;

/* stores wq and end point details */

struct rmnet_shs_wq_ep_s {
	u64 tcp_rx_bps;
	u64 udp_rx_bps;
	struct list_head ep_list_id;
	struct net_device *ep;
	int  new_lo_core[MAX_CPUS];
	int  new_hi_core[MAX_CPUS];
	u16 default_core_msk;
	u16 pri_core_msk;
	u16 rps_config_msk;
	u8 is_ep_active;
	int  new_lo_idx;
	int  new_hi_idx;
	int  new_lo_max;
	int  new_hi_max;
};

struct rmnet_shs_wq_ep_list_s {
struct list_head ep_id;
	struct rmnet_shs_wq_ep_s ep;
};

/* Types of suggestions made by shs wq */
enum rmnet_shs_wq_suggestion_type {
	RMNET_SHS_WQ_SUGG_NONE,
	RMNET_SHS_WQ_SUGG_SILVER_TO_GOLD,
	RMNET_SHS_WQ_SUGG_GOLD_TO_SILVER,
	RMNET_SHS_WQ_SUGG_GOLD_BALANCE,
	RMNET_SHS_WQ_SUGG_MAX,
};

struct rmnet_shs_wq_hstat_s {
	unsigned long int rmnet_shs_wq_suggs[RMNET_SHS_WQ_SUGG_MAX];
	struct list_head cpu_node_id;
	struct list_head hstat_node_id;
	struct rmnet_shs_skbn_s *node; //back pointer to node
	time_t c_epoch; /*current epoch*/
	time_t l_epoch; /*last hash update epoch*/
	time_t inactive_duration;
	u64 rx_skb;
	u64 rx_bytes;
	u64 rx_pps; /*pkts per second*/
	u64 rx_bps; /*bits per second*/
	u64 last_pps;
	u64 avg_pps;
	u64 last_rx_skb;
	u64 last_rx_bytes;
	u32 rps_config_msk; /*configured rps mask for net device*/
	u32 current_core_msk; /*mask where the current core's bit is set*/
	u32 def_core_msk; /*(little cluster) avaialble core mask*/
	u32 pri_core_msk; /* priority cores availability mask*/
	u32 available_core_msk; /* other available cores for this flow*/
	u32 hash; /*skb hash*/
	int stat_idx; /*internal used for datatop*/
	u16 suggested_cpu; /* recommended CPU to stamp pkts*/
	u16 current_cpu; /* core where the flow is being processed*/
	u16 skb_tport_proto;
	u8 in_use;
	u8 is_perm;
	u8 is_new_flow;
	u8 segs_per_skb; /* segments per skb */
};

struct rmnet_shs_wq_cpu_rx_pkt_q_s {
	struct list_head hstat_id;
	time_t l_epoch; /*last epoch update for this structure*/
	u64 last_rx_skbs;
	u64 last_rx_bytes;
	u64 rx_skbs;
	u64 rx_bytes;
	u64 rx_pps; /* pkts per second*/
	u64 rx_bps; /*bits per second*/
	u64 last_rx_pps; /* pkts per second*/
	u64 last_rx_bps; /* bits per second*/
	u64 avg_pps;
	u64 rx_bps_est; /*estimated bits per second*/
	u32 qhead;          /* queue head */
	u32 last_qhead;     /* last queue head */
	u32 qhead_diff; /* diff in pp in last tick*/
	u32 qhead_start; /* start mark of total pp*/
	u32 qhead_total; /* end mark of total pp*/
	int flows;
	u16 cpu_num;
};

struct rmnet_shs_wq_rx_flow_s {
	struct rmnet_shs_wq_cpu_rx_pkt_q_s cpu_list[MAX_CPUS];
	time_t l_epoch; /*last epoch update for this flow*/
	u64 dl_mrk_last_rx_bytes;
	u64 dl_mrk_last_rx_pkts;
	u64 dl_mrk_rx_bytes; /*rx bytes as observed in DL marker*/
	u64 dl_mrk_rx_pkts; /*rx pkts as observed in DL marker*/
	u64 dl_mrk_rx_pps; /*rx pkts per sec as observed in DL marker*/
	u64 dl_mrk_rx_bps; /*rx bits per sec as observed in DL marker*/
	u64 last_rx_skbs;
	u64 last_rx_bytes;
	u64 last_rx_pps; /*rx pkts per sec*/
	u64 last_rx_bps; /*rx bits per sec*/
	u64 rx_skbs;
	u64 rx_bytes;
	u64 rx_pps; /*rx pkts per sec*/
	u64 rx_bps; /*rx bits per sec*/
	u32 rps_config_msk; /*configured rps mask for net device*/
	u32 def_core_msk; /*(little cluster) avaialble core mask*/
	u32 pri_core_msk; /* priority cores availability mask*/
	u32 available_core_msk; /* other available cores for this flow*/
	int  new_lo_core[MAX_CPUS];
	int  new_hi_core[MAX_CPUS];
	int  new_lo_idx;
	int  new_hi_idx;
	int  new_lo_max;
	int  new_hi_max;
	int flows;
	u8 cpus;
};

struct rmnet_shs_delay_wq_s {
	struct delayed_work wq;
};

/* Structures to be used for creating sorted versions of flow and cpu lists */
struct rmnet_shs_wq_cpu_cap_s {
	struct list_head cpu_cap_list;
	u64 pps_capacity;
	u64 avg_pps_capacity;
	u64 bps;
	u16 cpu_num;
};

struct rmnet_shs_wq_gold_flow_s {
	struct list_head gflow_list;
	u64 rx_pps;
	u64 avg_pps;
	u32 hash;
	u16 cpu_num;
};

struct rmnet_shs_wq_ss_flow_s {
	struct list_head ssflow_list;
	u64 rx_pps;
	u64 avg_pps;
	u64 rx_bps;
	u32 hash;
	u16 cpu_num;
};

/* Tracing Definitions */
enum rmnet_shs_wq_trace_func {
	RMNET_SHS_WQ_INIT,
	RMNET_SHS_WQ_PROCESS_WQ,
	RMNET_SHS_WQ_EXIT,
	RMNET_SHS_WQ_EP_TBL,
	RMNET_SHS_WQ_HSTAT_TBL,
	RMNET_SHS_WQ_CPU_HSTAT_TBL,
	RMNET_SHS_WQ_FLOW_STATS,
	RMNET_SHS_WQ_CPU_STATS,
	RMNET_SHS_WQ_TOTAL_STATS,
	RMNET_SHS_WQ_SHSUSR,
};

enum rmnet_shs_wq_trace_evt {
	RMNET_SHS_WQ_EP_TBL_START,
	RMNET_SHS_WQ_EP_TBL_ADD,
	RMNET_SHS_WQ_EP_TBL_DEL,
	RMNET_SHS_WQ_EP_TBL_CLEANUP,
	RMNET_SHS_WQ_EP_TBL_INIT,
	RMNET_SHS_WQ_EP_TBL_END,
	RMNET_SHS_WQ_HSTAT_TBL_START,
	RMNET_SHS_WQ_HSTAT_TBL_ADD,
	RMNET_SHS_WQ_HSTAT_TBL_DEL,
	RMNET_SHS_WQ_HSTAT_TBL_NODE_RESET,
	RMNET_SHS_WQ_HSTAT_TBL_NODE_NEW_REQ,
	RMNET_SHS_WQ_HSTAT_TBL_NODE_REUSE,
	RMNET_SHS_WQ_HSTAT_TBL_NODE_DYN_ALLOCATE,
	RMNET_SHS_WQ_HSTAT_TBL_END,
	RMNET_SHS_WQ_CPU_HSTAT_TBL_START,
	RMNET_SHS_WQ_CPU_HSTAT_TBL_INIT,
	RMNET_SHS_WQ_CPU_HSTAT_TBL_ADD,
	RMNET_SHS_WQ_CPU_HSTAT_TBL_MOVE,
	RMNET_SHS_WQ_CPU_HSTAT_TBL_DEL,
	RMNET_SHS_WQ_CPU_HSTAT_TBL_END,
	RMNET_SHS_WQ_FLOW_STATS_START,
	RMNET_SHS_WQ_FLOW_STATS_UPDATE_MSK,
	RMNET_SHS_WQ_FLOW_STATS_UPDATE_NEW_CPU,
	RMNET_SHS_WQ_FLOW_STATS_SUGGEST_NEW_CPU,
	RMNET_SHS_WQ_FLOW_STATS_ERR,
	RMNET_SHS_WQ_FLOW_STATS_FLOW_INACTIVE,
	RMNET_SHS_WQ_FLOW_STATS_FLOW_INACTIVE_TIMEOUT,
	RMNET_SHS_WQ_FLOW_STATS_END,
	RMNET_SHS_WQ_CPU_STATS_START,
	RMNET_SHS_WQ_CPU_STATS_CURRENT_UTIL,
	RMNET_SHS_WQ_CPU_STATS_INC_CPU_FLOW,
	RMNET_SHS_WQ_CPU_STATS_DEC_CPU_FLOW,
	RMNET_SHS_WQ_CPU_STATS_GET_CPU_FLOW,
	RMNET_SHS_WQ_CPU_STATS_GET_MAX_CPU_FLOW,
	RMNET_SHS_WQ_CPU_STATS_MAX_FLOW_IN_CLUSTER,
	RMNET_SHS_WQ_CPU_STATS_UPDATE,
	RMNET_SHS_WQ_CPU_STATS_CORE2SWITCH_START,
	RMNET_SHS_WQ_CPU_STATS_CORE2SWITCH_FIND,
	RMNET_SHS_WQ_CPU_STATS_CORE2SWITCH_EVAL_CPU,
	RMNET_SHS_WQ_CPU_STATS_CORE2SWITCH_END,
	RMNET_SHS_WQ_CPU_STATS_NEW_FLOW_LIST_LO,
	RMNET_SHS_WQ_CPU_STATS_NEW_FLOW_LIST_HI,
	RMNET_SHS_WQ_CPU_STATS_END,
	RMNET_SHS_WQ_TOTAL_STATS_START,
	RMNET_SHS_WQ_TOTAL_STATS_UPDATE,
	RMNET_SHS_WQ_TOTAL_STATS_END,
	RMNET_SHS_WQ_PROCESS_WQ_START,
	RMNET_SHS_WQ_PROCESS_WQ_END,
	RMNET_SHS_WQ_PROCESS_WQ_ERR,
	RMNET_SHS_WQ_INIT_START,
	RMNET_SHS_WQ_INIT_END,
	RMNET_SHS_WQ_EXIT_START,
	RMNET_SHS_WQ_EXIT_END,
	RMNET_SHS_WQ_TRY_PASS,
	RMNET_SHS_WQ_TRY_FAIL,
	RMNET_SHS_WQ_SHSUSR_SYNC_START,
	RMNET_SHS_WQ_SHSUSR_SYNC_END,
	RMNET_SHS_WQ_FLOW_STATS_SET_FLOW_SEGMENTATION,
	RMNET_SHS_WQ_FLOW_SEG_SET_PASS,
	RMNET_SHS_WQ_FLOW_SEG_SET_FAIL,
};

extern struct rmnet_shs_cpu_node_s rmnet_shs_cpu_node_tbl[MAX_CPUS];

void rmnet_shs_wq_init(struct net_device *dev);
void rmnet_shs_wq_exit(void);
void rmnet_shs_wq_restart(void);
void rmnet_shs_wq_pause(void);

void rmnet_shs_update_cfg_mask(void);

u64 rmnet_shs_wq_get_max_pps_among_cores(u32 core_msk);
void rmnet_shs_wq_create_new_flow(struct rmnet_shs_skbn_s *node_p);
int rmnet_shs_wq_get_least_utilized_core(u16 core_msk);
int rmnet_shs_wq_get_lpwr_cpu_new_flow(struct net_device *dev);
int rmnet_shs_wq_get_perf_cpu_new_flow(struct net_device *dev);
u64 rmnet_shs_wq_get_max_allowed_pps(u16 cpu);
void rmnet_shs_wq_inc_cpu_flow(u16 cpu);
void rmnet_shs_wq_dec_cpu_flow(u16 cpu);
void rmnet_shs_hstat_tbl_delete(void);
void rmnet_shs_wq_set_ep_active(struct net_device *dev);
void rmnet_shs_wq_reset_ep_active(struct net_device *dev);
void rmnet_shs_wq_refresh_new_flow_list(void);

int rmnet_shs_wq_try_to_move_flow(u16 cur_cpu, u16 dest_cpu, u32 hash_to_move,
				  u32 sugg_type);

int rmnet_shs_wq_set_flow_segmentation(u32 hash_to_set, u8 segs_per_skb);

void rmnet_shs_wq_ep_lock_bh(void);

void rmnet_shs_wq_ep_unlock_bh(void);

#endif /*_RMNET_SHS_WQ_H_*/
