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
 * RMNET Data Smart Hash Workqueue Generic Netlink Functions
 *
 */

#include "rmnet_shs_wq_genl.h"
#include <net/sock.h>
#include <linux/skbuff.h>

MODULE_LICENSE("GPL v2");

static struct net *last_net;
static u32 last_snd_portid;

uint32_t rmnet_shs_genl_seqnum;
int rmnet_shs_userspace_connected;

/* Static Functions and Definitions */
static struct nla_policy rmnet_shs_genl_attr_policy[RMNET_SHS_GENL_ATTR_MAX + 1] = {
	[RMNET_SHS_GENL_ATTR_INT] = { .type = NLA_S32 },
	[RMNET_SHS_GENL_ATTR_SUGG] = { .len = sizeof(struct rmnet_shs_wq_sugg_info) },
	[RMNET_SHS_GENL_ATTR_SEG] = { .len = sizeof(struct rmnet_shs_wq_seg_info) },
	[RMNET_SHS_GENL_ATTR_STR] = { .type = NLA_NUL_STRING },
};

#define RMNET_SHS_GENL_OP(_cmd, _func)			\
	{						\
		.cmd	= _cmd,				\
		.policy	= rmnet_shs_genl_attr_policy,	\
		.doit	= _func,			\
		.dumpit	= NULL,				\
		.flags	= 0,				\
	}

static const struct genl_ops rmnet_shs_genl_ops[] = {
	RMNET_SHS_GENL_OP(RMNET_SHS_GENL_CMD_INIT_DMA,
			  rmnet_shs_genl_dma_init),
	RMNET_SHS_GENL_OP(RMNET_SHS_GENL_CMD_TRY_TO_MOVE_FLOW,
			  rmnet_shs_genl_try_to_move_flow),
	RMNET_SHS_GENL_OP(RMNET_SHS_GENL_CMD_SET_FLOW_SEGMENTATION,
			  rmnet_shs_genl_set_flow_segmentation),
	RMNET_SHS_GENL_OP(RMNET_SHS_GENL_CMD_MEM_SYNC,
			  rmnet_shs_genl_mem_sync),
};

struct genl_family rmnet_shs_genl_family = {
	.hdrsize = 0,
	.name    = RMNET_SHS_GENL_FAMILY_NAME,
	.version = RMNET_SHS_GENL_VERSION,
	.maxattr = RMNET_SHS_GENL_ATTR_MAX,
	.ops     = rmnet_shs_genl_ops,
	.n_ops   = ARRAY_SIZE(rmnet_shs_genl_ops),
};

int rmnet_shs_genl_send_int_to_userspace(struct genl_info *info, int val)
{
	struct sk_buff *skb;
	void *msg_head;
	int rc;

	skb = genlmsg_new(NLMSG_GOODSIZE, GFP_ATOMIC);
	if (skb == NULL)
		goto out;

	msg_head = genlmsg_put(skb, 0, info->snd_seq+1, &rmnet_shs_genl_family,
			       0, RMNET_SHS_GENL_CMD_INIT_DMA);
	if (msg_head == NULL) {
		rc = -ENOMEM;
		goto out;
	}
	rc = nla_put_u32(skb, RMNET_SHS_GENL_ATTR_INT, val);
	if (rc != 0)
		goto out;

	genlmsg_end(skb, msg_head);

	rc = genlmsg_unicast(genl_info_net(info), skb, info->snd_portid);
	if (rc != 0)
		goto out;

	rm_err("SHS_GNL: Successfully sent int %d\n", val);
	return 0;

out:
	/* TODO: Need to free skb?? */
	rm_err("SHS_GNL: FAILED to send int %d\n", val);
	return -1;
}

int rmnet_shs_genl_send_int_to_userspace_no_info(int val)
{
	struct sk_buff *skb;
	void *msg_head;
	int rc;

	if (last_net == NULL) {
		rm_err("SHS_GNL: FAILED to send int %d - last_net is NULL\n",
		       val);
		return -1;
	}

	skb = genlmsg_new(NLMSG_GOODSIZE, GFP_ATOMIC);
	if (skb == NULL)
		goto out;

	msg_head = genlmsg_put(skb, 0, rmnet_shs_genl_seqnum++, &rmnet_shs_genl_family,
			       0, RMNET_SHS_GENL_CMD_INIT_DMA);
	if (msg_head == NULL) {
		rc = -ENOMEM;
		goto out;
	}
	rc = nla_put_u32(skb, RMNET_SHS_GENL_ATTR_INT, val);
	if (rc != 0)
		goto out;

	genlmsg_end(skb, msg_head);

	rc = genlmsg_unicast(last_net, skb, last_snd_portid);
	if (rc != 0)
		goto out;

	rm_err("SHS_GNL: Successfully sent int %d\n", val);
	return 0;

out:
	/* TODO: Need to free skb?? */
	rm_err("SHS_GNL: FAILED to send int %d\n", val);
	rmnet_shs_userspace_connected = 0;
	return -1;
}


int rmnet_shs_genl_send_msg_to_userspace(void)
{
	struct sk_buff *skb;
	void *msg_head;
	int rc;
	int val = rmnet_shs_genl_seqnum++;

	rm_err("SHS_GNL: Trying to send msg %d\n", val);
	skb = genlmsg_new(NLMSG_GOODSIZE, GFP_ATOMIC);
	if (skb == NULL)
		goto out;

	msg_head = genlmsg_put(skb, 0, rmnet_shs_genl_seqnum++, &rmnet_shs_genl_family,
			       0, RMNET_SHS_GENL_CMD_INIT_DMA);
	if (msg_head == NULL) {
		rc = -ENOMEM;
		goto out;
	}
	rc = nla_put_u32(skb, RMNET_SHS_GENL_ATTR_INT, val);
	if (rc != 0)
		goto out;

	genlmsg_end(skb, msg_head);

	genlmsg_multicast(&rmnet_shs_genl_family, skb, 0, 0, GFP_ATOMIC);

	rm_err("SHS_GNL: Successfully sent int %d\n", val);
	return 0;

out:
	/* TODO: Need to free skb?? */
	rm_err("SHS_GNL: FAILED to send int %d\n", val);
	rmnet_shs_userspace_connected = 0;
	return -1;
}

/* Currently unused - handles message from userspace to initialize the shared memory,
 * memory is inited by kernel wq automatically
 */
int rmnet_shs_genl_dma_init(struct sk_buff *skb_2, struct genl_info *info)
{
	rm_err("%s", "SHS_GNL: rmnet_shs_genl_dma_init");

	if (info == NULL) {
		rm_err("%s", "SHS_GNL: an error occured - info is null");
		return -1;
	}

	return 0;
}


int rmnet_shs_genl_set_flow_segmentation(struct sk_buff *skb_2, struct genl_info *info)
{
	struct nlattr *na;
	struct rmnet_shs_wq_seg_info seg_info;
	int rc = 0;

	rm_err("%s", "SHS_GNL: rmnet_shs_genl_set_flow_segmentation");

	if (info == NULL) {
		rm_err("%s", "SHS_GNL: an error occured - info is null");
		return -1;
	}

	na = info->attrs[RMNET_SHS_GENL_ATTR_SEG];
	if (na) {
		if (nla_memcpy(&seg_info, na, sizeof(seg_info)) > 0) {
			rm_err("SHS_GNL: recv segmentation req "
			       "hash_to_set = 0x%x segs_per_skb = %u",
			       seg_info.hash_to_set,
			       seg_info.segs_per_skb);

			rc = rmnet_shs_wq_set_flow_segmentation(seg_info.hash_to_set,
								seg_info.segs_per_skb);

			if (rc == 1) {
				rmnet_shs_genl_send_int_to_userspace(info, 0);
				trace_rmnet_shs_wq_high(RMNET_SHS_WQ_SHSUSR,
					RMNET_SHS_WQ_FLOW_SEG_SET_PASS,
					seg_info.hash_to_set, seg_info.segs_per_skb,
					0xDEF, 0xDEF, NULL, NULL);
			} else {
				rmnet_shs_genl_send_int_to_userspace(info, -1);
				trace_rmnet_shs_wq_high(RMNET_SHS_WQ_SHSUSR,
					RMNET_SHS_WQ_FLOW_SEG_SET_FAIL,
					seg_info.hash_to_set, seg_info.segs_per_skb,
					0xDEF, 0xDEF, NULL, NULL);
				return 0;
			}
		} else {
			rm_err("SHS_GNL: nla_memcpy failed %d\n",
			       RMNET_SHS_GENL_ATTR_SEG);
			rmnet_shs_genl_send_int_to_userspace(info, -1);
			return 0;
		}
	} else {
		rm_err("SHS_GNL: no info->attrs %d\n",
		       RMNET_SHS_GENL_ATTR_SEG);
		rmnet_shs_genl_send_int_to_userspace(info, -1);
		return 0;
	}

	return 0;
}

int rmnet_shs_genl_try_to_move_flow(struct sk_buff *skb_2, struct genl_info *info)
{
	struct nlattr *na;
	struct rmnet_shs_wq_sugg_info sugg_info;
	int rc = 0;

	rm_err("%s", "SHS_GNL: rmnet_shs_genl_try_to_move_flow");

	if (info == NULL) {
		rm_err("%s", "SHS_GNL: an error occured - info is null");
		return -1;
	}

	na = info->attrs[RMNET_SHS_GENL_ATTR_SUGG];
	if (na) {
		if (nla_memcpy(&sugg_info, na, sizeof(sugg_info)) > 0) {
			rm_err("SHS_GNL: cur_cpu =%u dest_cpu = %u "
			       "hash_to_move = 0x%x sugg_type = %u",
			       sugg_info.cur_cpu,
			       sugg_info.dest_cpu,
			       sugg_info.hash_to_move,
			       sugg_info.sugg_type);
			rc = rmnet_shs_wq_try_to_move_flow(sugg_info.cur_cpu,
							   sugg_info.dest_cpu,
							   sugg_info.hash_to_move,
							   sugg_info.sugg_type);
			if (rc == 1) {
				rmnet_shs_genl_send_int_to_userspace(info, 0);
				trace_rmnet_shs_wq_high(RMNET_SHS_WQ_SHSUSR, RMNET_SHS_WQ_TRY_PASS,
				   sugg_info.cur_cpu, sugg_info.dest_cpu,
				   sugg_info.hash_to_move, sugg_info.sugg_type, NULL, NULL);

			} else {
				rmnet_shs_genl_send_int_to_userspace(info, -1);
				trace_rmnet_shs_wq_high(RMNET_SHS_WQ_SHSUSR, RMNET_SHS_WQ_TRY_FAIL,
				   sugg_info.cur_cpu, sugg_info.dest_cpu,
				   sugg_info.hash_to_move, sugg_info.sugg_type, NULL, NULL);
				return 0;
			}
		} else {
			rm_err("SHS_GNL: nla_memcpy failed %d\n",
			       RMNET_SHS_GENL_ATTR_SUGG);
			rmnet_shs_genl_send_int_to_userspace(info, -1);
			return 0;
		}
	} else {
		rm_err("SHS_GNL: no info->attrs %d\n",
		       RMNET_SHS_GENL_ATTR_SUGG);
		rmnet_shs_genl_send_int_to_userspace(info, -1);
		return 0;
	}

	return 0;
}

int rmnet_shs_genl_mem_sync(struct sk_buff *skb_2, struct genl_info *info)
{
	rm_err("%s", "SHS_GNL: rmnet_shs_genl_mem_sync");

	if (!rmnet_shs_userspace_connected)
		rmnet_shs_userspace_connected = 1;

	/* Todo: detect when userspace is disconnected. If we dont get
	 * a sync message in the next 2 wq ticks, we got disconnected
	 */

	trace_rmnet_shs_wq_high(RMNET_SHS_WQ_SHSUSR, RMNET_SHS_WQ_SHSUSR_SYNC_START,
				0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);

	if (info == NULL) {
		rm_err("%s", "SHS_GNL: an error occured - info is null");
		return -1;
	}

	last_net = genl_info_net(info);
	last_snd_portid = info->snd_portid;
	return 0;
}

/* register new generic netlink family */
int rmnet_shs_wq_genl_init(void)
{
	int ret;

	rmnet_shs_userspace_connected = 0;
	ret = genl_register_family(&rmnet_shs_genl_family);
	if (ret != 0) {
		rm_err("SHS_GNL: register family failed: %i", ret);
		genl_unregister_family(&rmnet_shs_genl_family);
		return -1;
	}

	rm_err("SHS_GNL: successfully registered generic netlink familiy: %s",
	       RMNET_SHS_GENL_FAMILY_NAME);

	return 0;
}

/* Unregister the generic netlink family */
int rmnet_shs_wq_genl_deinit(void)
{
	int ret;

	rmnet_shs_genl_send_int_to_userspace_no_info(RMNET_SHS_SYNC_WQ_EXIT);

	ret = genl_unregister_family(&rmnet_shs_genl_family);
	if(ret != 0){
		rm_err("SHS_GNL: unregister family failed: %i\n",ret);
	}
	rmnet_shs_userspace_connected = 0;
	return 0;
}
