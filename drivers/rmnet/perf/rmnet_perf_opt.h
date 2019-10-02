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

#ifndef _RMNET_PERF_OPT_H_
#define _RMNET_PERF_OPT_H_

#include <linux/skbuff.h>
#include "rmnet_perf_core.h"

#define RMNET_PERF_FLOW_HASH_TABLE_BITS        4
#define RMNET_PERF_NUM_FLOW_NODES              8

struct rmnet_perf_opt_pkt_node {
	unsigned char *header_start;
	unsigned char *data_start;
	struct rmnet_frag_descriptor *frag_desc;
	u16 data_len;
};

struct rmnet_perf_opt_ip_flags {
	u8 ip_ttl;
	u8 ip_tos;
	u16 ip_frag_off;
};

struct rmnet_perf_opt_flow_node {
	/* Header lengths */
	u8 ip_len;
	u8 trans_len;

	/* Header protocols */
	u8 ip_proto;
	u8 trans_proto;

	/* Ports */
	__be16	src_port;
	__be16	dest_port;

	/* IP addresses */
	union {
		__be32	saddr4;
		struct in6_addr saddr6;
	} saddr;
	union {
		__be32	daddr4;
		struct in6_addr daddr6;
	} daddr;

	/* IP flags */
	union {
		struct rmnet_perf_opt_ip_flags ip4_flags;
		__be32 first_word;
	} ip_flags;

	/* TCP metadata */
	__be32 next_seq;

	/* GSO metadata */
	u32 gso_len;

	/* Perf metadata */
	u8 num_pkts_held;
	u32 len;
	u32 hash_value;
	struct rmnet_endpoint *ep;
	struct hlist_node list;

	/* The packets we're holding */
	struct rmnet_perf_opt_pkt_node pkt_list[50];
};

struct rmnet_perf_opt_flow_node_pool {
	u8 num_flows_in_use;
	u16 flow_recycle_counter;
	struct rmnet_perf_opt_flow_node *
		node_list[RMNET_PERF_NUM_FLOW_NODES];
};

struct rmnet_perf_opt_meta {
	struct rmnet_perf_opt_flow_node_pool *node_pool;
};

enum rmnet_perf_opt_flush_reasons {
	RMNET_PERF_OPT_PACKET_CORRUPT_ERROR,
	RMNET_PERF_OPT_CHAIN_END,
	RMNET_PERF_OPT_NUM_CONDITIONS
};

void
rmnet_perf_opt_update_flow(struct rmnet_perf_opt_flow_node *flow_node,
			   struct rmnet_perf_pkt_info *pkt_info);
void rmnet_perf_opt_flush_single_flow_node(
				struct rmnet_perf_opt_flow_node *flow_node);
void rmnet_perf_opt_flush_flow_by_hash(u32 hash_val);
void rmnet_perf_opt_flush_all_flow_nodes(void);
void rmnet_perf_opt_chain_end(void);
void rmnet_perf_opt_insert_pkt_in_flow(
			struct rmnet_perf_opt_flow_node *flow_node,
			struct rmnet_perf_pkt_info *pkt_info);
bool rmnet_perf_opt_ingress(struct rmnet_perf_pkt_info *pkt_info);
void rmnet_perf_free_hash_table(void);

#endif /* _RMNET_PERF_OPT_H_ */
