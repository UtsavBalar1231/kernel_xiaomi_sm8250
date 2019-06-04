/* Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
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
 * RMNET Data Smart Hash solution
 *
 */

#include <linux/skbuff.h>
#include "rmnet_shs_wq.h"

#ifndef _RMNET_SHS_H_
#define _RMNET_SHS_H_

#include "rmnet_shs_freq.h"

#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_config.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_map.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_private.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_handlers.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_trace.h>

#include <../include/soc/qcom/qmi_rmnet.h>

#define RMNET_SHS_HT rmnet_shs_ht
#define RMNET_SHS_HT_SIZE 9
#define RMNET_SHS_MAX_SKB_INACTIVE_TSEC 30
#define MAX_SILVER_CORES 4
#define MAX_CPUS  8
#define PERF_MASK 0xF0

/* RPS mask change's Default core for orphaned CPU flows */
#define MAIN_CORE 0
#define UPDATE_MASK 0xFF
#define MAX_FLOWS 700

/* Different max inactivity based on # of flows */
#define FLOW_LIMIT1 70
#define INACTIVE_TSEC1  10
#define FLOW_LIMIT2 140
#define INACTIVE_TSEC2  2


//#define RMNET_SHS_MAX_UDP_SILVER_CORE_DATA_RATE 1073741824 //1.0Gbps
//#define RMNET_SHS_MAX_UDP_SILVER_CORE_DATA_RATE 320787200 //320 Mbps
//#define RMNET_SHS_MAX_UDP_GOLD_CORE_DATA_RATE 3650722201 //3.4 Gbps
//#define RMNET_SHS_UDP_PPS_SILVER_CORE_UPPER_THRESH 90000
//#define RMNET_SHS_TCP_PPS_SILVER_CORE_UPPER_THRESH 90000

#define SHS_TRACE_ERR(...) if (rmnet_shs_debug) \
	trace_rmnet_shs_err(__VA_ARGS__)

#define SHS_TRACE_HIGH(...) if (rmnet_shs_debug) \
	trace_rmnet_shs_high(__VA_ARGS__)

#define SHS_TRACE_LOW(...) if (rmnet_shs_debug) \
	trace_rmnet_shs_low(__VA_ARGS__)

#define RMNET_SHS_MAX_SILVER_CORE_BURST_CAPACITY  204800

#define RMNET_SHS_TCP_COALESCING_RATIO 23 //Heuristic
#define RMNET_SHS_UDP_PPS_LPWR_CPU_UTHRESH 80000
#define RMNET_SHS_TCP_PPS_LPWR_CPU_UTHRESH (80000*RMNET_SHS_TCP_COALESCING_RATIO)

#define RMNET_SHS_UDP_PPS_PERF_CPU_UTHRESH 210000
#define RMNET_SHS_TCP_PPS_PERF_CPU_UTHRESH (210000*RMNET_SHS_TCP_COALESCING_RATIO)

//50% of MAX SILVER THRESHOLD
#define RMNET_SHS_UDP_PPS_LPWR_CPU_LTHRESH 0
#define RMNET_SHS_UDP_PPS_PERF_CPU_LTHRESH 40000
#define RMNET_SHS_TCP_PPS_PERF_CPU_LTHRESH (40000*RMNET_SHS_TCP_COALESCING_RATIO)

struct core_flush_s {
	struct  hrtimer core_timer;
	struct work_struct work;
	struct timespec coretime;
	int coresum;
	u8 core;
};

struct rmnet_shs_cfg_s {
	struct	hrtimer hrtimer_shs;
	struct rmnet_map_dl_ind dl_mrk_ind_cb;
	struct qmi_rmnet_ps_ind rmnet_idl_ind_cb;
	struct rmnet_port *port;
	struct  core_flush_s core_flush[MAX_CPUS];
	u64 core_skbs[MAX_CPUS];
	long int num_bytes_parked;
	long int num_pkts_parked;
	u32 is_reg_dl_mrk_ind;
	u16 num_flows;
	u8 is_pkt_parked;
	u8 is_timer_init;
	u8 force_flush_state;
	u8 rmnet_shs_init_complete;
	u8 dl_ind_state;
	u8 map_mask;
	u8 map_len;

};

struct rmnet_shs_skb_list {
	struct sk_buff *head;
	struct sk_buff *tail;
	u64 num_parked_bytes;
	u32 num_parked_skbs;
	u32 skb_load;
};

struct rmnet_shs_skbn_s {
	struct list_head node_id;
	/*list head for per cpu flow table*/
	struct net_device *dev;
	struct rmnet_shs_wq_hstat_s *hstats;
	/*stats meta data*/
	struct rmnet_shs_skb_list skb_list;
	/*list to park packets*/
	struct hlist_node list;
	/*list head for hash table*/
	u64 num_skb;
	/* num skbs received*/
	u64 num_skb_bytes;
	/* num bytes received*/
	u32 queue_head;
	/* n/w stack CPU pkt processing queue head */
	u32 hash;
	/*incoming hash*/
	u16 map_index;
	/* rps map index assigned*/
	u16 map_cpu;
	/* rps cpu for this flow*/
	u16 skb_tport_proto;
	/* Transport protocol associated with this flow*/
	u8 is_shs_enabled;
	/*Is SHS enabled for this flow*/
};

enum rmnet_shs_tmr_force_flush_state_e {
	RMNET_SHS_FLUSH_OFF,
	RMNET_SHS_FLUSH_ON,
	RMNET_SHS_FLUSH_DONE
};

enum rmnet_shs_switch_reason_e {
	RMNET_SHS_SWITCH_INSTANT_RATE,
	RMNET_SHS_SWITCH_WQ_RATE,
	RMNET_SHS_OOO_PACKET_SWITCH,
	RMNET_SHS_OOO_PACKET_TOTAL,
	RMNET_SHS_SWITCH_MAX_REASON
};

enum rmnet_shs_dl_ind_state {
	RMNET_SHS_HDR_PENDING,
	RMNET_SHS_END_PENDING,
	RMNET_SHS_IND_COMPLETE,
	RMNET_SHS_DL_IND_MAX_STATE
};


enum rmnet_shs_flush_reason_e {
	RMNET_SHS_FLUSH_PKT_LIMIT,
	RMNET_SHS_FLUSH_BYTE_LIMIT,
	RMNET_SHS_FLUSH_TIMER_EXPIRY,
	RMNET_SHS_FLUSH_RX_DL_TRAILER,
	RMNET_SHS_FLUSH_INV_DL_IND,
	RMNET_SHS_FLUSH_WQ_FB_FLUSH,
	RMNET_SHS_FLUSH_WQ_CORE_FLUSH,
	RMNET_SHS_FLUSH_PSH_PKT_FLUSH,
	RMNET_SHS_FLUSH_MAX_REASON
};

struct flow_buff {
	struct sk_buff *skb;
	struct flow_buff *next;
};

struct rmnet_shs_flush_work {
	struct work_struct work;
	struct rmnet_port *port;
};

struct rmnet_shs_cpu_node_s {
	struct list_head node_list_id;
	u32 qhead;
	u32 qtail;
	u32 qdiff;
	u32 parkedlen;
	u8 prio;
	u8 wqprio;
};

enum rmnet_shs_trace_func {
	RMNET_SHS_MODULE,
	RMNET_SHS_CPU_NODE,
	RMNET_SHS_SKB_STAMPING,
	RMNET_SHS_SKB_CAN_GRO,
	RMNET_SHS_DELIVER_SKB,
	RMNET_SHS_CORE_CFG,
	RMNET_SHS_HASH_MAP,
	RMNET_SHS_ASSIGN,
	RMNET_SHS_FLUSH,
	RMNET_SHS_DL_MRK,
};

enum rmnet_shs_flush_context {
	RMNET_RX_CTXT,
	RMNET_WQ_CTXT,
	RMNET_MAX_CTXT
};


/* Trace events and functions */
enum rmnet_shs_trace_evt {
	RMNET_SHS_MODULE_INIT,
	RMNET_SHS_MODULE_INIT_WQ,
	RMNET_SHS_MODULE_GOING_DOWN,
	RMNET_SHS_MODULE_EXIT,
	RMNET_SHS_CPU_NODE_FUNC_START,
	RMNET_SHS_CPU_NODE_FUNC_ADD,
	RMNET_SHS_CPU_NODE_FUNC_MOVE,
	RMNET_SHS_CPU_NODE_FUNC_REMOVE,
	RMNET_SHS_CPU_NODE_FUNC_END,
	RMNET_SHS_SKB_STAMPING_START,
	RMNET_SHS_SKB_STAMPING_END,
	RMNET_SHS_SKB_CAN_GRO_START,
	RMNET_SHS_SKB_CAN_GRO_END,
	RMNET_SHS_DELIVER_SKB_START,
	RMNET_SHS_DELIVER_SKB_END,
	RMNET_SHS_CORE_CFG_START,
	RMNET_SHS_CORE_CFG_NUM_LO_CORES,
	RMNET_SHS_CORE_CFG_NUM_HI_CORES,
	RMNET_SHS_CORE_CFG_CHK_HI_CPU,
	RMNET_SHS_CORE_CFG_CHK_LO_CPU,
	RMNET_SHS_CORE_CFG_GET_QHEAD,
	RMNET_SHS_CORE_CFG_GET_QTAIL,
	RMNET_SHS_CORE_CFG_GET_CPU_PROC_PARAMS,
	RMNET_SHS_CORE_CFG_END,
	RMNET_SHS_HASH_MAP_START,
	RMNET_SHS_HASH_MAP_IDX_TO_STAMP,
	RMNET_SHS_HASH_MAP_FORM_HASH,
	RMNET_SHS_HASH_MAP_END,
	RMNET_SHS_ASSIGN_START,
	RMNET_SHS_ASSIGN_GET_NEW_FLOW_CPU,
	RMNET_SHS_ASSIGN_MATCH_FLOW_NODE_START,
	RMNET_SHS_ASSIGN_MATCH_FLOW_COMPLETE,
	RMNET_SHS_ASSIGN_PARK_PKT_COMPLETE,
	RMNET_SHS_ASSIGN_PARK_TMR_START,
	RMNET_SHS_ASSIGN_PARK_TMR_CANCEL,
	RMNET_SHS_ASSIGN_MASK_CHNG,
	RMNET_SHS_ASSIGN_CRIT_ERROR_NO_MSK_SET,
	RMNET_SHS_ASSIGN_CRIT_ERROR_NO_SHS_REQD,
	RMNET_SHS_ASSIGN_END,
	RMNET_SHS_FLUSH_START,
	RMNET_SHS_FLUSH_PARK_TMR_EXPIRY,
	RMNET_SHS_FLUSH_PARK_TMR_RESTART,
	RMNET_SHS_FLUSH_DELAY_WQ_TRIGGER,
	RMNET_SHS_FLUSH_DELAY_WQ_START,
	RMNET_SHS_FLUSH_DELAY_WQ_END,
	RMNET_SHS_FLUSH_FORCE_TRIGGER,
	RMNET_SHS_FLUSH_BYTE_LIMIT_TRIGGER,
	RMNET_SHS_FLUSH_PKT_LIMIT_TRIGGER,
	RMNET_SHS_FLUSH_DL_MRK_TRLR_HDLR_START,
	RMNET_SHS_FLUSH_DL_MRK_TRLR_HDLR_END,
	RMNET_SHS_FLUSH_CHK_AND_FLUSH_NODE_START,
	RMNET_SHS_FLUSH_NODE_START,
	RMNET_SHS_FLUSH_CHK_NODE_CAN_FLUSH,
	RMNET_SHS_FLUSH_NODE_CORE_SWITCH,
	RMNET_SHS_FLUSH_NODE_END,
	RMNET_SHS_FLUSH_CHK_AND_FLUSH_NODE_END,
	RMNET_SHS_FLUSH_END,
	RMNET_SHS_DL_MRK_START,
	RMNET_SHS_DL_MRK_HDR_HDLR_START,
	RMNET_SHS_DL_MRK_HDR_HDLR_END,
	RMNET_SHS_DL_MRK_TRLR_START,
	RMNET_SHS_DL_MRK_TRLR_HDLR_END,
	RMNET_SHS_DL_MRK_TRLR_END,
	RMNET_SHS_DL_MRK_END,
};

extern struct rmnet_shs_flush_work shs_delayed_work;
extern spinlock_t rmnet_shs_ht_splock;
extern struct hlist_head RMNET_SHS_HT[1 << (RMNET_SHS_HT_SIZE)];

/* rmnet based functions that we rely on*/
extern void rmnet_deliver_skb(struct sk_buff *skb,
			      struct rmnet_port *port);
extern int (*rmnet_shs_skb_entry)(struct sk_buff *skb,
				  struct rmnet_port *port);
int rmnet_shs_is_lpwr_cpu(u16 cpu);
void rmnet_shs_cancel_table(void);
void rmnet_shs_rx_wq_init(void);
void rmnet_shs_rx_wq_exit(void);
int rmnet_shs_get_mask_len(u8 mask);

int rmnet_shs_chk_and_flush_node(struct rmnet_shs_skbn_s *node,
				 u8 force_flush, u8 ctxt);
void rmnet_shs_dl_hdr_handler_v2(struct rmnet_map_dl_ind_hdr *dlhdr,
			      struct rmnet_map_control_command_header *qcmd);
void rmnet_shs_dl_trl_handler_v2(struct rmnet_map_dl_ind_trl *dltrl,
			      struct rmnet_map_control_command_header *qcmd);
void rmnet_shs_dl_hdr_handler(struct rmnet_map_dl_ind_hdr *dlhdr);
void rmnet_shs_dl_trl_handler(struct rmnet_map_dl_ind_trl *dltrl);
void rmnet_shs_assign(struct sk_buff *skb, struct rmnet_port *port);
void rmnet_shs_flush_table(u8 is_force_flush, u8 ctxt);
void rmnet_shs_cpu_node_remove(struct rmnet_shs_skbn_s *node);
void rmnet_shs_init(struct net_device *dev, struct net_device *vnd);
void rmnet_shs_exit(void);
void rmnet_shs_ps_on_hdlr(void *port);
void rmnet_shs_ps_off_hdlr(void *port);
void rmnet_shs_update_cpu_proc_q_all_cpus(void);
void rmnet_shs_clear_node(struct rmnet_shs_skbn_s *node, u8 ctxt);

u32 rmnet_shs_get_cpu_qhead(u8 cpu_num);
#endif /* _RMNET_SHS_H_ */
