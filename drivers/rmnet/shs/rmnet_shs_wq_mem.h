/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
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

#ifndef _RMNET_SHS_WQ_MEM_H_
#define _RMNET_SHS_WQ_MEM_H_

#include "rmnet_shs.h"

/* Shared memory files */
#define RMNET_SHS_PROC_DIR      "shs"
#define RMNET_SHS_PROC_CAPS     "rmnet_shs_caps"
#define RMNET_SHS_PROC_G_FLOWS  "rmnet_shs_flows"
#define RMNET_SHS_PROC_SS_FLOWS "rmnet_shs_ss_flows"

#define RMNET_SHS_MAX_USRFLOWS (128)

struct __attribute__((__packed__)) rmnet_shs_wq_cpu_cap_usr_s {
	u64 pps_capacity;
	u64 avg_pps_capacity;
	u64 bps_capacity;
	u16 cpu_num;
};

struct __attribute__((__packed__)) rmnet_shs_wq_gflows_usr_s {
	u64 rx_pps;
	u64 avg_pps;
	u64 rx_bps;
	u32 hash;
	u16 cpu_num;
};

struct __attribute__((__packed__)) rmnet_shs_wq_ssflows_usr_s {
	u64 rx_pps;
	u64 avg_pps;
	u64 rx_bps;
	u32 hash;
	u16 cpu_num;
};

extern struct list_head gflows;
extern struct list_head ssflows;
extern struct list_head cpu_caps;

/* Buffer size for read and write syscalls */
enum {RMNET_SHS_BUFFER_SIZE = 4096};

struct rmnet_shs_mmap_info {
	char *data;
};

/* Function Definitions */

void rmnet_shs_wq_ssflow_list_add(struct rmnet_shs_wq_hstat_s *hnode,
				  struct list_head *ss_flows);
void rmnet_shs_wq_gflow_list_add(struct rmnet_shs_wq_hstat_s *hnode,
				 struct list_head *gold_flows);

void rmnet_shs_wq_cleanup_gold_flow_list(struct list_head *gold_flows);
void rmnet_shs_wq_cleanup_ss_flow_list(struct list_head *ss_flows);

void rmnet_shs_wq_cpu_caps_list_add(
				struct rmnet_shs_wq_rx_flow_s *rx_flow_tbl_p,
				struct rmnet_shs_wq_cpu_rx_pkt_q_s *cpu_node,
				struct list_head *cpu_caps);

void rmnet_shs_wq_cleanup_cpu_caps_list(struct list_head *cpu_caps);

void rmnet_shs_wq_mem_update_cached_cpu_caps(struct list_head *cpu_caps);

void rmnet_shs_wq_mem_update_cached_sorted_gold_flows(struct list_head *gold_flows);
void rmnet_shs_wq_mem_update_cached_sorted_ss_flows(struct list_head *ss_flows);

void rmnet_shs_wq_mem_init(void);

void rmnet_shs_wq_mem_deinit(void);

#endif /*_RMNET_SHS_WQ_GENL_H_*/
