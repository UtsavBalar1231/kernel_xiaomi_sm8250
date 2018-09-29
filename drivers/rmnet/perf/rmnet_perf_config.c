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
 * RMNET core functionalities. Common helpers
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include "rmnet_perf_core.h"
#include "rmnet_perf_tcp_opt.h"
#include "rmnet_perf_config.h"
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_map.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_private.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_handlers.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_config.h>

MODULE_LICENSE("GPL v2");

unsigned int temp_debug __read_mostly = 1;
module_param(temp_debug, uint, 0644);
MODULE_PARM_DESC(temp_debug, "temp_debug");

/* global to rmnet_perf. Used to hold meta_data */
struct rmnet_perf *perf;

struct rmnet_perf *rmnet_perf_config_get_perf(void)
{
	return perf;
}

/* rmnet_perf_config_alloc_64k_buffs() - Recycled buffer allocation on rmnet
 * init
 * @perf: allows access to our required global structures
 *
 * Allocate RMNET_NUM_64K_BUFFS many buffers. Assign them to the available
 * array. This only occurs on rmnet init
 *
 * Return:
 *		- return_val:
 *			- RMNET_PERF_RESOURCE_MGMT_FAIL: Ran out of memory
 *			- RMNET_PERF_RESOURCE_MGMT_SUCCESS: No problems
 **/
static enum rmnet_perf_resource_management_e
rmnet_perf_config_alloc_64k_buffs(struct rmnet_perf *perf)
{
	int i;
	struct sk_buff *skbn;
	struct rmnet_perf_core_64k_buff_pool *pool = perf->core_meta->buff_pool;
	enum rmnet_perf_resource_management_e return_val;

	return_val = RMNET_PERF_RESOURCE_MGMT_SUCCESS;

	for (i = 0; i < RMNET_PERF_NUM_64K_BUFFS; i++) {
		skbn = alloc_skb(RMNET_PERF_CORE_RECYCLE_SKB_SIZE, GFP_ATOMIC);
		if (!skbn)
			return_val = RMNET_PERF_RESOURCE_MGMT_FAIL;
		pool->available[i] = skbn;
	}
	pool->index = 0;
	return return_val;
}

/* rmnet_perf_config_free_64k_buffs() - Recycled buffer free on rmnet teardown
 * @perf: allows access to our required global structures
 *
 * Free RMNET_NUM_64K_BUFFS many buffers. This only occurs on rmnet
 * teardown.
 *
 * Return:
 *		- void
 **/
static void rmnet_perf_config_free_64k_buffs(struct rmnet_perf *perf)
{
	int i;
	struct rmnet_perf_core_64k_buff_pool *buff_pool;

	buff_pool = perf->core_meta->buff_pool;

	/* Free both busy and available because if its truly busy,
	 * we will simply decrement the users count... This means NW stack
	 * will still have opportunity to process the packet as it wishes
	 * and will naturally free the sk_buff when it is done
	 */

	for (i = 0; i < RMNET_PERF_NUM_64K_BUFFS; i++)
		kfree_skb(buff_pool->available[i]);
}

/* rmnet_perf_config_free_resources() - on rmnet teardown free all the
 *		related meta data structures
 * @perf: allows access to our required global structures
 *
 * Free the recycled 64k buffers, all held SKBs that came from physical
 * device, and free the meta data structure itself.
 *
 * Return:
 *		- status of the freeing dependent on the validity of the perf
 **/
static enum rmnet_perf_resource_management_e
rmnet_perf_config_free_resources(struct rmnet_perf *perf,
				 struct net_device *dev)
{
	if (!perf)
		return RMNET_PERF_RESOURCE_MGMT_FAIL;

	/* Free everything tcp_opt currently holds */
	rmnet_perf_tcp_opt_flush_all_flow_nodes(perf);
	/* Get rid of 64k sk_buff cache */
	rmnet_perf_config_free_64k_buffs(perf);
	/* Before we free tcp_opt's structures, make sure we arent holding
	 * any SKB's hostage
	 */
	rmnet_perf_core_free_held_skbs(perf);

	//rmnet_perf_core_timer_exit(perf->core_meta);
	/* Since we allocated in one chunk, we will also free in one chunk */
	kfree(perf->tcp_opt_meta);

	return RMNET_PERF_RESOURCE_MGMT_SUCCESS;
}

/* rmnet_perf_config_allocate_resources() - Allocates and assigns all tcp_opt
 *		required meta data
 * @perf: allows access to our required global structures
 *
 * Prepares node pool, the nodes themselves, the skb list from the
 * physical device, and the recycled skb pool
 * TODO separate out things which are not tcp_opt specific
 *
 * Return:
 *		- status of the freeing dependent on the validity of the perf
 **/
static int rmnet_perf_config_allocate_resources(struct rmnet_perf **perf)
{
	int i;
	void *buffer_head;
	struct rmnet_perf_tcp_opt_meta *tcp_opt_meta;
	struct rmnet_perf_core_meta *core_meta;
	struct rmnet_perf *local_perf;

	int perf_size = sizeof(**perf);
	int tcp_opt_meta_size = sizeof(struct rmnet_perf_tcp_opt_meta);
	int flow_node_pool_size =
			sizeof(struct rmnet_perf_tcp_opt_flow_node_pool);
	int bm_state_size = sizeof(struct rmnet_perf_core_burst_marker_state);
	int flow_node_size = sizeof(struct rmnet_perf_tcp_opt_flow_node);
	int core_meta_size = sizeof(struct rmnet_perf_core_meta);
	int skb_list_size = sizeof(struct rmnet_perf_core_skb_list);
	int skb_buff_pool_size = sizeof(struct rmnet_perf_core_64k_buff_pool);

	int total_size = perf_size + tcp_opt_meta_size + flow_node_pool_size +
			(flow_node_size * RMNET_PERF_NUM_FLOW_NODES) +
			core_meta_size + skb_list_size + skb_buff_pool_size;

	/* allocate all the memory in one chunk for cache coherency sake */
	buffer_head = kmalloc(total_size, GFP_KERNEL);
	if (!buffer_head)
		return RMNET_PERF_RESOURCE_MGMT_FAIL;

	*perf = buffer_head;
	local_perf = *perf;
	buffer_head += perf_size;

	local_perf->tcp_opt_meta = buffer_head;
	tcp_opt_meta = local_perf->tcp_opt_meta;
	buffer_head += tcp_opt_meta_size;

	/* assign the node pool */
	tcp_opt_meta->node_pool = buffer_head;
	tcp_opt_meta->node_pool->num_flows_in_use = 0;
	tcp_opt_meta->node_pool->flow_recycle_counter = 0;
	buffer_head += flow_node_pool_size;

	/* assign the individual flow nodes themselves */
	for (i = 0; i < RMNET_PERF_NUM_FLOW_NODES; i++) {
		struct rmnet_perf_tcp_opt_flow_node **flow_node;

		flow_node = &tcp_opt_meta->node_pool->node_list[i];
		*flow_node = buffer_head;
		buffer_head += flow_node_size;
		(*flow_node)->num_pkts_held = 0;
	}

	local_perf->core_meta = buffer_head;
	core_meta = local_perf->core_meta;
	//rmnet_perf_core_timer_init(core_meta);
	buffer_head += core_meta_size;

	/* Assign common (not specific to something like tcp_opt) structures */
	core_meta->skb_needs_free_list = buffer_head;
	core_meta->skb_needs_free_list->num_skbs_held = 0;
	buffer_head += skb_list_size;

	/* allocate buffer pool struct (also not specific to tcp_opt) */
	core_meta->buff_pool = buffer_head;
	buffer_head += skb_buff_pool_size;

	/* assign the burst marker state */
	core_meta->bm_state = buffer_head;
	core_meta->bm_state->curr_seq = 0;
	core_meta->bm_state->expect_packets = 0;
	core_meta->bm_state->wait_for_start = true;
	buffer_head += bm_state_size;

	return RMNET_PERF_RESOURCE_MGMT_SUCCESS;
}

static void rmnet_perf_netdev_down(struct net_device *dev)
{
	enum rmnet_perf_resource_management_e config_status;

	config_status = rmnet_perf_config_free_resources(perf, dev);
}

static int rmnet_perf_netdev_up(void)
{
	enum rmnet_perf_resource_management_e alloc_rc;

	alloc_rc = rmnet_perf_config_allocate_resources(&perf);
	if (alloc_rc == RMNET_PERF_RESOURCE_MGMT_FAIL)
		pr_err("Failed to allocate tcp_opt and core resources");

	/* structs to contain these have already been allocated. Here we are
	 * simply allocating the buffers themselves
	 */
	alloc_rc |= rmnet_perf_config_alloc_64k_buffs(perf);
	if (alloc_rc == RMNET_PERF_RESOURCE_MGMT_FAIL)
		pr_err("%s(): Failed to allocate 64k buffers for recycling\n",
		       __func__);
	else
		pr_err("%s(): Allocated 64k buffers for recycling\n",
		       __func__);

	return alloc_rc;
}

/* TODO Needs modifying*/
static int rmnet_perf_config_notify_cb(struct notifier_block *nb,
				       unsigned long event, void *data)
{
	/*Not sure if we need this*/
	struct net_device *dev = netdev_notifier_info_to_dev(data);
	unsigned int return_val;

	if (!dev)
		return NOTIFY_DONE;

	switch (event) {
	case NETDEV_UNREGISTER:
		if (rmnet_is_real_dev_registered(dev) &&
		    !strncmp(dev->name, "rmnet_ipa0", 10)) {
			pr_err("%s(): rmnet_perf netdevice unregister,",
			       __func__);
			/* Unregister for DL marker */
			rmnet_map_dl_ind_deregister(rmnet_get_port(dev),
						    perf->core_meta->dl_ind);
			rmnet_perf_netdev_down(dev);
		}
		break;
	case NETDEV_REGISTER:
		pr_err("%s(): rmnet_perf netdevice register, name = %s,",
		       __func__, dev->name);
		/* Check prevents us from allocating resources for every
		 * interface
		 */
		if (!rmnet_perf_deag_entry) {
			rmnet_perf_netdev_up();
			RCU_INIT_POINTER(rmnet_perf_deag_entry,
					 rmnet_perf_core_deaggregate);
		}
		if (strncmp(dev->name, "rmnet_ipa0", 10) == 0 &&
		    rmnet_perf_deag_entry) {
			struct rmnet_map_dl_ind *dl_ind;

			/* register for DL marker */
			dl_ind = kzalloc(sizeof(struct rmnet_map_dl_ind),
					 GFP_ATOMIC);
			if (dl_ind) {
				dev_net_set(dev, &init_net);
				perf->core_meta->dev = dev;
				
				dl_ind->priority = RMNET_PERF;
				dl_ind->dl_hdr_handler =
					&rmnet_perf_core_handle_map_control_start;
				dl_ind->dl_trl_handler =
					&rmnet_perf_core_handle_map_control_end;
				perf->core_meta->dl_ind = dl_ind;
				return_val =
					rmnet_map_dl_ind_register(rmnet_get_port(dev),
								dl_ind);
			}
		}
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block rmnet_perf_dev_notifier __read_mostly = {
	.notifier_call = rmnet_perf_config_notify_cb,
};

int __init rmnet_perf_init(void)
{
	pr_err("%s(): initializing rmnet_perf, 5\n", __func__);
	return register_netdevice_notifier(&rmnet_perf_dev_notifier);
}

void __exit rmnet_perf_exit(void)
{
	pr_err("%s(): exiting rmnet_perf\n", __func__);
	RCU_INIT_POINTER(rmnet_perf_deag_entry, NULL);
	unregister_netdevice_notifier(&rmnet_perf_dev_notifier);
}

module_init(rmnet_perf_init);
module_exit(rmnet_perf_exit);
