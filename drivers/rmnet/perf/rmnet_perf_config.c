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
 *
 * RMNET core functionalities. Common helpers
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include "rmnet_perf_core.h"
#include "rmnet_perf_opt.h"
#include "rmnet_perf_config.h"
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_map.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_handlers.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_config.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_private.h>

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
	enum rmnet_perf_resource_management_e return_val;
	struct rmnet_perf_core_64k_buff_pool *pool = perf->core_meta->buff_pool;

	return_val = RMNET_PERF_RESOURCE_MGMT_SUCCESS;
	memset(pool, 0, sizeof(struct rmnet_perf_core_64k_buff_pool));
	pool->index = 0;
	for (i = 0; i < RMNET_PERF_NUM_64K_BUFFS; i++) {
		skbn = alloc_skb(RMNET_PERF_CORE_RECYCLE_SKB_SIZE, GFP_ATOMIC);
		if (!skbn) {
			int j;

			return_val = RMNET_PERF_RESOURCE_MGMT_FAIL;
			/* If one skb fails to allocate, dont use feature */
			for (j = i - 1; j >= 0; j--) {
				if (pool->available[j]) {
					kfree_skb(pool->available[j]);
					pool->available[j] = NULL;
				}
			}
			return return_val;
		}
		pool->available[i] = skbn;
	}

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
	 * and will naturally free the sk_buff when it is done. Available[0]
	 * being not null means that all indexes of available are filled by
	 * SKBs from module initialization
	 */
	if (buff_pool->available[0]) {
		for (i = 0; i < RMNET_PERF_NUM_64K_BUFFS; i++)
			kfree_skb(buff_pool->available[i]);
	}
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
rmnet_perf_config_free_resources(struct rmnet_perf *perf)
{
	if (!perf) {
		pr_err("%s(): Cannot free resources without proper perf\n",
		       __func__);
		return RMNET_PERF_RESOURCE_MGMT_FAIL;
	}

	/* Free everything flow nodes currently hold */
	rmnet_perf_opt_flush_all_flow_nodes();

	/* Get rid of 64k sk_buff cache */
	rmnet_perf_config_free_64k_buffs(perf);
	/* Before we free tcp_opt's structures, make sure we arent holding
	 * any SKB's hostage
	 */
	rmnet_perf_core_free_held_skbs();

	/* Clean up any remaining nodes in the flow table before freeing */
	rmnet_perf_free_hash_table();

	/* Since we allocated in one chunk, we will also free in one chunk */
	kfree(perf);

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
	struct rmnet_perf_opt_meta *opt_meta;
	struct rmnet_perf_core_meta *core_meta;
	struct rmnet_perf *local_perf;

	int perf_size = sizeof(**perf);
	int opt_meta_size = sizeof(struct rmnet_perf_opt_meta);
	int flow_node_pool_size =
			sizeof(struct rmnet_perf_opt_flow_node_pool);
	int bm_state_size = sizeof(struct rmnet_perf_core_burst_marker_state);
	int flow_node_size = sizeof(struct rmnet_perf_opt_flow_node);
	int core_meta_size = sizeof(struct rmnet_perf_core_meta);
	int skb_list_size = sizeof(struct rmnet_perf_core_skb_list);
	int skb_buff_pool_size = sizeof(struct rmnet_perf_core_64k_buff_pool);

	int total_size = perf_size + opt_meta_size + flow_node_pool_size +
			(flow_node_size * RMNET_PERF_NUM_FLOW_NODES) +
			core_meta_size + skb_list_size + skb_buff_pool_size
			+ bm_state_size;

	/* allocate all the memory in one chunk for cache coherency sake */
	buffer_head = kmalloc(total_size, GFP_KERNEL);
	*perf = buffer_head;
	if (!buffer_head)
		return RMNET_PERF_RESOURCE_MGMT_FAIL;

	local_perf = *perf;
	buffer_head += perf_size;

	local_perf->opt_meta = buffer_head;
	opt_meta = local_perf->opt_meta;
	buffer_head += opt_meta_size;

	/* assign the node pool */
	opt_meta->node_pool = buffer_head;
	opt_meta->node_pool->num_flows_in_use = 0;
	opt_meta->node_pool->flow_recycle_counter = 0;
	buffer_head += flow_node_pool_size;

	/* assign the individual flow nodes themselves */
	for (i = 0; i < RMNET_PERF_NUM_FLOW_NODES; i++) {
		struct rmnet_perf_opt_flow_node **flow_node;

		flow_node = &opt_meta->node_pool->node_list[i];
		*flow_node = buffer_head;
		buffer_head += flow_node_size;
		(*flow_node)->num_pkts_held = 0;
		(*flow_node)->len = 0;
	}

	local_perf->core_meta = buffer_head;
	core_meta = local_perf->core_meta;
	buffer_head += core_meta_size;

	/* Assign common (not specific to something like opt) structures */
	core_meta->skb_needs_free_list = buffer_head;
	core_meta->skb_needs_free_list->num_skbs_held = 0;
	buffer_head += skb_list_size;

	/* allocate buffer pool struct (also not specific to opt) */
	core_meta->buff_pool = buffer_head;
	buffer_head += skb_buff_pool_size;

	/* assign the burst marker state */
	core_meta->bm_state = buffer_head;
	core_meta->bm_state->curr_seq = 0;
	core_meta->bm_state->expect_packets = 0;
	core_meta->bm_state->wait_for_start = true;
	core_meta->bm_state->callbacks_valid = false;
	buffer_head += bm_state_size;

	return RMNET_PERF_RESOURCE_MGMT_SUCCESS;
}

enum rmnet_perf_resource_management_e
rmnet_perf_config_register_callbacks(struct net_device *dev,
				     struct rmnet_port *port)
{
	struct rmnet_map_dl_ind *dl_ind;
	struct qmi_rmnet_ps_ind *ps_ind;
	enum rmnet_perf_resource_management_e rc =
					RMNET_PERF_RESOURCE_MGMT_SUCCESS;

	perf->core_meta->dev = dev;
	/* register for DL marker */
	dl_ind = kzalloc(sizeof(struct rmnet_map_dl_ind), GFP_ATOMIC);
	perf->core_meta->dl_ind = dl_ind;
	if (dl_ind) {
		dl_ind->priority = RMNET_PERF;
		if (port->data_format & RMNET_INGRESS_FORMAT_DL_MARKER_V2) {
			dl_ind->dl_hdr_handler_v2 =
				&rmnet_perf_core_handle_map_control_start_v2;
			dl_ind->dl_trl_handler_v2 =
				&rmnet_perf_core_handle_map_control_end_v2;
		} else {
			dl_ind->dl_hdr_handler =
				&rmnet_perf_core_handle_map_control_start;
			dl_ind->dl_trl_handler =
				&rmnet_perf_core_handle_map_control_end;
		}

		if (rmnet_map_dl_ind_register(port, dl_ind)) {
			kfree(dl_ind);
			pr_err("%s(): Failed to register dl_ind\n", __func__);
			perf->core_meta->dl_ind = NULL;
			rc = RMNET_PERF_RESOURCE_MGMT_FAIL;
		}
	} else {
		pr_err("%s(): Failed to allocate dl_ind\n", __func__);
		rc = RMNET_PERF_RESOURCE_MGMT_FAIL;
	}

	/* register for PS mode indications */
	ps_ind = kzalloc(sizeof(struct qmi_rmnet_ps_ind), GFP_ATOMIC);
	perf->core_meta->ps_ind = ps_ind;
	if (ps_ind) {
		ps_ind->ps_on_handler = &rmnet_perf_core_ps_on;
		ps_ind->ps_off_handler = &rmnet_perf_core_ps_off;
		if (qmi_rmnet_ps_ind_register(port, ps_ind)) {
			kfree(ps_ind);
			rc = RMNET_PERF_RESOURCE_MGMT_FAIL;
			perf->core_meta->ps_ind = NULL;
			pr_err("%s(): Failed to register ps_ind\n", __func__);
		}
	} else {
		rc = RMNET_PERF_RESOURCE_MGMT_FAIL;
		pr_err("%s(): Failed to allocate ps_ind\n", __func__);
	}

	if (rc == RMNET_PERF_RESOURCE_MGMT_SUCCESS)
		perf->core_meta->bm_state->callbacks_valid = true;

	return rc;
}

static enum rmnet_perf_resource_management_e rmnet_perf_netdev_down(void)
{
	return rmnet_perf_config_free_resources(perf);
}

static int rmnet_perf_netdev_up(struct net_device *real_dev,
				struct rmnet_port *port)
{
	enum rmnet_perf_resource_management_e rc;

	rc = rmnet_perf_config_allocate_resources(&perf);
	if (rc == RMNET_PERF_RESOURCE_MGMT_FAIL) {
		pr_err("Failed to allocate tcp_opt and core resources\n");
		return RMNET_PERF_RESOURCE_MGMT_FAIL;
	}

	/* structs to contain these have already been allocated. Here we are
	 * simply allocating the buffers themselves
	 */
	rc = rmnet_perf_config_alloc_64k_buffs(perf);
	if (rc == RMNET_PERF_RESOURCE_MGMT_FAIL) {
		/* Since recycling buffers isnt a feature we use, refrain
		 * from returning with a return failure status
		 */
		pr_err("%s(): Failed to allocate 64k buffers for recycling\n",
		       __func__);
	}

	rc = rmnet_perf_config_register_callbacks(real_dev, port);
	if (rc == RMNET_PERF_RESOURCE_MGMT_FAIL) {
		pr_err("%s(): Failed to register for required "
			"callbacks\n", __func__);
		return RMNET_PERF_RESOURCE_MGMT_SEMI_FAIL;
	}

	return RMNET_PERF_RESOURCE_MGMT_SUCCESS;
}

static enum rmnet_perf_resource_management_e
rmnet_perf_dereg_callbacks(struct net_device *dev,
			   struct rmnet_perf_core_meta *core_meta)
{
	struct rmnet_port *port;
	enum rmnet_perf_resource_management_e return_val =
					RMNET_PERF_RESOURCE_MGMT_SUCCESS;
	enum rmnet_perf_resource_management_e return_val_final =
					RMNET_PERF_RESOURCE_MGMT_SUCCESS;

	port = rmnet_get_port(dev);
	if (!port || !core_meta) {
		pr_err("%s(): rmnet port or core_meta is missing for "
		       "dev = %s\n", __func__, dev->name);
		return RMNET_PERF_RESOURCE_MGMT_FAIL;
	}

	/* Unregister for DL marker */
	return_val = rmnet_map_dl_ind_deregister(port, core_meta->dl_ind);
	return_val_final = return_val;
	if (!return_val)
		kfree(core_meta->dl_ind);
	return_val = qmi_rmnet_ps_ind_deregister(port, core_meta->ps_ind);
	return_val_final |= return_val;
	if (!return_val)
		kfree(core_meta->ps_ind);

	if (return_val_final)
		pr_err("%s(): rmnet_perf related callbacks may "
			"not be deregistered properly\n",
			__func__);

	return return_val_final;
}

static bool rmnet_perf_config_hook_registered(void)
{
	int (*deag_entry)(struct sk_buff *skb);
	void (*frag_entry)(struct rmnet_frag_descriptor *frag_desc,
			   struct rmnet_port *port);

	deag_entry = rcu_dereference(rmnet_perf_deag_entry);
	frag_entry = rcu_dereference(rmnet_perf_desc_entry);

	return deag_entry || frag_entry;
}

/* TODO Needs modifying*/
static int rmnet_perf_config_notify_cb(struct notifier_block *nb,
				       unsigned long event, void *data)
{
	struct rmnet_port *port;
	enum rmnet_perf_resource_management_e return_val =
					RMNET_PERF_RESOURCE_MGMT_SUCCESS;
	struct net_device *dev = netdev_notifier_info_to_dev(data);

	if (!dev)
		return NOTIFY_DONE;

	switch (event) {
	case NETDEV_UNREGISTER:
		if (rmnet_is_real_dev_registered(dev) &&
		    rmnet_perf_config_hook_registered() &&
		    (!strncmp(dev->name, "rmnet_ipa0", 10) ||
		     !strncmp(dev->name, "rmnet_mhi0", 10))) {
			struct rmnet_perf_core_meta *core_meta =
				perf->core_meta;
			pr_info("%s(): rmnet_perf netdevice unregister\n",
				__func__);
			return_val = rmnet_perf_dereg_callbacks(dev, core_meta);
			return_val |= rmnet_perf_netdev_down();
			if (return_val)
				pr_err("%s(): Error on netdev down event\n",
				       __func__);
			RCU_INIT_POINTER(rmnet_perf_deag_entry, NULL);
			RCU_INIT_POINTER(rmnet_perf_desc_entry, NULL);
			RCU_INIT_POINTER(rmnet_perf_chain_end, NULL);
		}
		break;
	case NETDEV_REGISTER:
		pr_info("%s(): rmnet_perf netdevice register, name = %s\n",
			__func__, dev->name);
		/* Check prevents us from allocating resources for every
		 * interface
		 */
		if (!rmnet_perf_config_hook_registered() &&
		    strncmp(dev->name, "rmnet_data", 10) == 0) {
			struct rmnet_priv *priv = netdev_priv(dev);

			port = rmnet_get_port(priv->real_dev);
			return_val = rmnet_perf_netdev_up(priv->real_dev,
							  port);
			if (return_val == RMNET_PERF_RESOURCE_MGMT_FAIL) {
				pr_err("%s(): rmnet_perf allocation "
				       "failed. Falling back on legacy path\n",
					__func__);
				goto exit;
			} else if (return_val ==
				   RMNET_PERF_RESOURCE_MGMT_SEMI_FAIL) {
				pr_err("%s(): rmnet_perf callback registry "
				       "failed. Continue without them\n",
					__func__);
			}
			rmnet_perf_core_set_ingress_hook();
			pr_info("%s(): rmnet_perf registered on name = %s\n",
				__func__, dev->name);
		}
		break;
	default:
		break;
	}
exit:
	return NOTIFY_DONE;
}

static struct notifier_block rmnet_perf_dev_notifier __read_mostly = {
	.notifier_call = rmnet_perf_config_notify_cb,
	.priority = 1,
};

int __init rmnet_perf_init(void)
{
	pr_info("%s(): initializing rmnet_perf\n", __func__);
	return register_netdevice_notifier(&rmnet_perf_dev_notifier);
}

void __exit rmnet_perf_exit(void)
{
	pr_info("%s(): exiting rmnet_perf\n", __func__);
	unregister_netdevice_notifier(&rmnet_perf_dev_notifier);
}

module_init(rmnet_perf_init);
module_exit(rmnet_perf_exit);
