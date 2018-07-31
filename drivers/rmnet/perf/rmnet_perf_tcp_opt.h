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
 */
#include <net/tcp.h>
#include "rmnet_perf_core.h"
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_config.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_map.h>

#ifndef _RMNET_PERF_TCP_OPT_H_
#define _RMNET_PERF_TCP_OPT_H_

#define RMNET_PERF_FLOW_HASH_TABLE_BITS        4
#define RMNET_PERF_FLOW_HASH_TABLE_BUCKETS    16
#define RMNET_PERF_NUM_FLOW_NODES              8
#define RMNET_PERF_TCP_OPT_HEADER_PKT          1
#define RMNET_PERF_TCP_OPT_PAYLOAD_PKT         0

enum rmnet_perf_tcp_opt_merge_check_rc {
	/* merge pkt into flow node */
	RMNET_PERF_TCP_OPT_MERGE_SUCCESS,
	/* flush flow nodes, and also flush the current pkt */
	RMNET_PERF_TCP_OPT_FLUSH_ALL,
	/* flush flow nodes, but insert pkt into newly empty flow */
	RMNET_PERF_TCP_OPT_FLUSH_SOME,
	/* flush flow nodes, but insert pkt into newly empty flow, flush GRO*/
	RMNET_PERF_TCP_OPT_FLUSH_SOME_GRO,
};

enum rmnet_perf_tcp_opt_flush_reasons {
	RMNET_PERF_TCP_OPT_TCP_FLUSH_FORCE,
	RMNET_PERF_TCP_OPT_TIMESTAMP_MISMATCH,
	RMNET_PERF_TCP_OPT_64K_LIMIT,
	RMNET_PERF_TCP_OPT_NO_SPACE_IN_NODE,
	RMNET_PERF_TCP_OPT_FLOW_NODE_SHORTAGE,
	RMNET_PERF_TCP_OPT_OUT_OF_ORDER_SEQ,
	RMNET_PERF_TCP_OPT_CHECKSUM_ERR,
	RMNET_PERF_TCP_OPT_PACKET_CORRUPT_ERROR,
	RMNET_PERF_TCP_OPT_NUM_CONDITIONS
};

struct rmnet_perf_tcp_opt_pkt_node {
	unsigned char *ip_start; /* This is simply used for debug purposes */
	unsigned char *data_start;
	unsigned char *data_end;
};

struct rmnet_perf_tcp_opt_ip_flags {
	u8 ip_ttl;
	u8 ip_tos;
	u16 ip_frag_off;
};

struct rmnet_perf_tcp_opt_flow_node {
	u8 mux_id;
	u8 protocol;
	u8 num_pkts_held;
	union {
		struct rmnet_perf_tcp_opt_ip_flags ip4_flags;
		__be32 first_word;
	} ip_flags;
	u32 timestamp;
	__be32	next_seq;
	u32 len;
	u32 hash_value;

	__be16	src_port;
	__be16	dest_port;
	union {
		__be32	saddr4;
		struct in6_addr saddr6;
	} saddr;
	union {
		__be32	daddr4;
		struct in6_addr daddr6;
	} daddr;

	struct hlist_node list;
	struct rmnet_perf_tcp_opt_pkt_node pkt_list[50];
};

struct rmnet_perf_tcp_opt_flow_node_pool {
	u8 num_flows_in_use;
	u16 flow_recycle_counter;
	struct rmnet_perf_tcp_opt_flow_node *
		node_list[RMNET_PERF_NUM_FLOW_NODES];
};

struct rmnet_perf_tcp_opt_meta {
	struct rmnet_perf_tcp_opt_flow_node_pool *node_pool;
};

void rmnet_perf_tcp_opt_dealloc_64k_buffs(struct rmnet_perf *perf);

enum rmnet_perf_resource_management_e
rmnet_perf_tcp_opt_alloc_64k_buffs(struct rmnet_perf *perf);

void rmnet_perf_tcp_opt_deaggregate(struct sk_buff *skb,
				    struct rmnet_perf *perf,
				    unsigned int more);

enum rmnet_perf_resource_management_e
rmnet_perf_tcp_opt_config_free_resources(struct rmnet_perf *perf);

void rmnet_perf_tcp_opt_free_held_skbs(struct rmnet_perf *perf);
void rmnet_perf_tcp_opt_flush_all_flow_nodes(struct rmnet_perf *perf);
void rmnet_perf_tcp_opt_ingress(struct rmnet_perf *perf, struct sk_buff *skb,
				struct rmnet_perf_pkt_info *pkt_info);
#endif /* _RMNET_PERF_TCP_OPT_H_ */
