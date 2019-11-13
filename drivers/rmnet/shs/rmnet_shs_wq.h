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

#ifndef _RMNET_SHS_WQ_H_
#define _RMNET_SHS_WQ_H_

#include "rmnet_shs_config.h"
#include "rmnet_shs.h"

#define MAX_SUPPORTED_FLOWS_DEBUG 16

#define RMNET_SHS_RX_BPNSEC_TO_BPSEC(x) ((x)*1000000000)
#define RMNET_SHS_SEC_TO_NSEC(x) ((x)*1000000000)
#define RMNET_SHS_NSEC_TO_SEC(x) ((x)/1000000000)
#define RMNET_SHS_BYTE_TO_BIT(x) ((x)*8)
#define RMNET_SHS_MIN_HSTAT_NODES_REQD 16
#define RMNET_SHS_WQ_DELAY_TICKS  10

/* stores wq and end point details */

struct rmnet_shs_wq_ep_s {
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

struct rmnet_shs_wq_hstat_s {
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
	u64 last_rx_skb;
	u64 last_rx_bytes;
	u32 rps_config_msk; /*configured rps mask for net device*/
	u32 current_core_msk; /*mask where the current core's bit is set*/
	u32 def_core_msk; /*(little cluster) avaialble core mask*/
	u32 pri_core_msk; /* priority cores availability mask*/
	u32 available_core_msk; /* other available cores for this flow*/
	u32 hash; /*skb hash*/
	u16 suggested_cpu; /* recommended CPU to stamp pkts*/
	u16 current_cpu; /* core where the flow is being processed*/
	u16 skb_tport_proto;
	int stat_idx; /*internal used for datatop*/
	u8 in_use;
	u8 is_perm;
	u8 is_new_flow;
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
#endif /*_RMNET_SHS_WQ_H_*/
