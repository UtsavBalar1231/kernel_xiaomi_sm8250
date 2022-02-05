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
 */
#include <linux/skbuff.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_config.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_map.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_descriptor.h>

#ifndef _RMNET_PERF_CORE_H_
#define _RMNET_PERF_CORE_H_

#define RMNET_PERF_NUM_64K_BUFFS              50
#define RMNET_PERF_CORE_RECYCLE_SKB_SIZE    65600//33000//32768//65600

struct rmnet_perf {
	struct rmnet_perf_opt_meta *opt_meta;
	struct rmnet_perf_core_meta *core_meta;
	struct rmnet_port *rmnet_port;
};

/* Identifying info for the current packet being deaggregated
 * this is so we don't have to redundantly check things in the
 * header of the packet. Also prevents excessive parameters
 */
struct rmnet_perf_pkt_info {
	/* True if this is the first packet being put into a flow node. */
	bool first_packet;
	bool csum_valid;

	/* Header protocols */
	u8 ip_proto;
	u8 trans_proto;

	/* Header lengths */
	u16 ip_len;
	u16 trans_len;

	/* Data length */
	u16 payload_len;

	/* Hash over standard 5 tuple */
	u32 hash_key;

	/* TCP timestamp */
	u32 curr_timestamp;

	/* Headers */
	union {
		struct iphdr *v4hdr;
		struct ipv6hdr *v6hdr;
	} ip_hdr;
	union {
		struct tcphdr *tp;
		struct udphdr *up;
	} trans_hdr;

	struct rmnet_endpoint *ep;

	/* The base packet itself */
	union {
		struct sk_buff *skb;
		struct rmnet_frag_descriptor *frag_desc;
	};
};

struct rmnet_perf_core_64k_buff_pool {
	u8 index;
	struct sk_buff *available[RMNET_PERF_NUM_64K_BUFFS];
};

struct rmnet_perf_core_burst_marker_state {
	bool wait_for_start;
	/* If the callbacks fail to register, then we want to flush at the
	 * end of every chain
	 */
	bool callbacks_valid;
	u32 curr_seq;
	u32 expect_packets;
};

//list of SKB's which we will free after some set amount of data.
//currently sized for 400k of data (266 1500 byte packets rounded to 350
//for safety)
struct rmnet_perf_core_skb_list {
	u16 num_skbs_held;
	struct sk_buff *head;
	struct sk_buff *tail;
};

struct rmnet_perf_core_meta {
	/* skbs from physical device */
	struct rmnet_perf_core_skb_list *skb_needs_free_list;
	/* recycled buffer pool */
	struct rmnet_perf_core_64k_buff_pool *buff_pool;
	struct net_device *dev;
	struct rmnet_perf_core_burst_marker_state *bm_state;
	struct rmnet_map_dl_ind *dl_ind;
	struct qmi_rmnet_ps_ind *ps_ind;
};

enum rmnet_perf_core_flush_reasons {
	RMNET_PERF_CORE_IPA_ZERO_FLUSH,
	RMNET_PERF_CORE_SK_BUFF_HELD_LIMIT,
	RMNET_PERF_CORE_DL_MARKER_FLUSHES,
	RMNET_PERF_CORE_PS_MODE_ON,
	RMNET_PERF_CORE_NUM_CONDITIONS
};

enum rmnet_perf_core_pkt_size_e {
	RMNET_PERF_CORE_50000_PLUS,
	RMNET_PERF_CORE_30000_PLUS, //32k full bucket
	RMNET_PERF_CORE_23000_PLUS, //24k full bucket
	RMNET_PERF_CORE_14500_PLUS, //16k full bucket
	RMNET_PERF_CORE_7000_PLUS, //8k full bucket
	RMNET_PERF_CORE_1400_PLUS,
	RMNET_PERF_CORE_0_PLUS,
	RMNET_PERF_CORE_DEBUG_BUCKETS_MAX
};

enum rmnet_perf_trace_func {
	RMNET_PERF_MODULE,
};

enum rmnet_perf_trace_evt {
	RMNET_PERF_START_DL_MRK,
	RMNET_PERF_END_DL_MRK,
	RMNET_PERF_DEAG_PKT,
};


void rmnet_perf_core_grab_lock(void);
void rmnet_perf_core_release_lock(void);
void rmnet_perf_core_ps_on(void *port);
void rmnet_perf_core_ps_off(void *port);
bool rmnet_perf_core_is_deag_mode(void);
void rmnet_perf_core_set_ingress_hook(void);
void rmnet_perf_core_reset_recycled_skb(struct sk_buff *skb);
struct sk_buff *rmnet_perf_core_elligible_for_cache_skb(u32 len);
void rmnet_perf_core_free_held_skbs(void);
void rmnet_perf_core_send_skb(struct sk_buff *skb, struct rmnet_endpoint *ep);
void rmnet_perf_core_send_desc(struct rmnet_frag_descriptor *frag_desc);
void rmnet_perf_core_flush_curr_pkt(struct rmnet_perf_pkt_info *pkt_info,
				    u16 packet_len, bool flush_shs,
				    bool skip_hash);
void rmnet_perf_core_deaggregate(struct sk_buff *skb,
				 struct rmnet_port *port);
void rmnet_perf_core_desc_entry(struct rmnet_frag_descriptor *frag_desc,
				struct rmnet_port *port);
u32 rmnet_perf_core_compute_flow_hash(struct rmnet_perf_pkt_info *pkt_info);
void rmnet_perf_core_flush_single_gro_flow(u32 hash_key);
void
rmnet_perf_core_handle_map_control_end_v2(struct rmnet_map_dl_ind_trl *dltrl,
				struct rmnet_map_control_command_header *qcmd);
void
rmnet_perf_core_handle_map_control_start_v2(struct rmnet_map_dl_ind_hdr *dlhdr,
				struct rmnet_map_control_command_header *qcmd);
void
rmnet_perf_core_handle_map_control_end(struct rmnet_map_dl_ind_trl *dltrl);
void
rmnet_perf_core_handle_map_control_start(struct rmnet_map_dl_ind_hdr *dlhdr);

#endif /* _RMNET_PERF_CORE_H_ */
