/* Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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

#ifndef _RMNET_SHS_WQ_GENL_H_
#define _RMNET_SHS_WQ_GENL_H_

#include "rmnet_shs.h"
#include <net/genetlink.h>

/* Generic Netlink Definitions */
#define RMNET_SHS_GENL_VERSION 1
#define RMNET_SHS_GENL_FAMILY_NAME "RMNET_SHS"
#define RMNET_SHS_SYNC_RESP_INT 828 /* Any number, sent after mem update */
#define RMNET_SHS_SYNC_WQ_EXIT  42

extern int rmnet_shs_userspace_connected;

enum {
	RMNET_SHS_GENL_CMD_UNSPEC,
	RMNET_SHS_GENL_CMD_INIT_DMA,
	RMNET_SHS_GENL_CMD_TRY_TO_MOVE_FLOW,
	RMNET_SHS_GENL_CMD_SET_FLOW_SEGMENTATION,
	RMNET_SHS_GENL_CMD_MEM_SYNC,
	__RMNET_SHS_GENL_CMD_MAX,
};

enum {
	RMNET_SHS_GENL_ATTR_UNSPEC,
	RMNET_SHS_GENL_ATTR_STR,
	RMNET_SHS_GENL_ATTR_INT,
	RMNET_SHS_GENL_ATTR_SUGG,
	RMNET_SHS_GENL_ATTR_SEG,
	__RMNET_SHS_GENL_ATTR_MAX,
};
#define RMNET_SHS_GENL_ATTR_MAX (__RMNET_SHS_GENL_ATTR_MAX - 1)

struct rmnet_shs_wq_sugg_info {
	uint32_t hash_to_move;
	uint32_t sugg_type;
	uint16_t cur_cpu;
	uint16_t dest_cpu;
};

struct rmnet_shs_wq_seg_info {
	uint32_t hash_to_set;
	uint32_t segment_enable;
};

/* Function Prototypes */
int rmnet_shs_genl_dma_init(struct sk_buff *skb_2, struct genl_info *info);
int rmnet_shs_genl_try_to_move_flow(struct sk_buff *skb_2, struct genl_info *info);
int rmnet_shs_genl_set_flow_segmentation(struct sk_buff *skb_2, struct genl_info *info);
int rmnet_shs_genl_mem_sync(struct sk_buff *skb_2, struct genl_info *info);

int rmnet_shs_genl_send_int_to_userspace(struct genl_info *info, int val);

int rmnet_shs_genl_send_int_to_userspace_no_info(int val);

int rmnet_shs_genl_send_msg_to_userspace(void);

int rmnet_shs_wq_genl_init(void);

int rmnet_shs_wq_genl_deinit(void);

#endif /*_RMNET_SHS_WQ_GENL_H_*/
