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
 * RMNET Data Smart Hash stamping solution
 *
 */
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_map.h>
#include <../drivers/net/ethernet/qualcomm/rmnet/rmnet_private.h>
#include "rmnet_shs_config.h"
#include "rmnet_shs.h"
#include "rmnet_shs_wq.h"

MODULE_LICENSE("GPL v2");


unsigned int rmnet_shs_debug __read_mostly;
module_param(rmnet_shs_debug, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_debug, "rmnet_shs_debug");

unsigned int rmnet_shs_stats_enabled __read_mostly = 1;
module_param(rmnet_shs_stats_enabled, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_stats_enabled, "Enable Disable stats collection");

unsigned long int rmnet_shs_crit_err[RMNET_SHS_CRIT_ERR_MAX];
module_param_array(rmnet_shs_crit_err, ulong, 0, 0444);
MODULE_PARM_DESC(rmnet_shs_crit_err, "rmnet shs crtical error type");

static int rmnet_shs_dev_notify_cb(struct notifier_block *nb,
				    unsigned long event, void *data);

static struct notifier_block rmnet_shs_dev_notifier __read_mostly = {
	.notifier_call = rmnet_shs_dev_notify_cb,
	.priority = 2,
};

static int rmnet_vnd_total;
/* Enable smart hashing capability upon call to initialize module*/
int __init rmnet_shs_module_init(void)
{
	pr_info("%s(): Starting rmnet SHS module\n", __func__);
	trace_rmnet_shs_high(RMNET_SHS_MODULE, RMNET_SHS_MODULE_INIT,
			    0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);
	return register_netdevice_notifier(&rmnet_shs_dev_notifier);
}

/* Remove smart hashing capability upon call to initialize module */
void __exit rmnet_shs_module_exit(void)
{
	trace_rmnet_shs_high(RMNET_SHS_MODULE, RMNET_SHS_MODULE_EXIT,
			    0xDEF, 0xDEF, 0xDEF, 0xDEF, NULL, NULL);
	unregister_netdevice_notifier(&rmnet_shs_dev_notifier);
	pr_info("%s(): Exiting rmnet SHS module\n", __func__);
}

static int rmnet_shs_dev_notify_cb(struct notifier_block *nb,
				    unsigned long event, void *data)
{

	struct net_device *dev = netdev_notifier_info_to_dev(data);
	struct rmnet_priv *priv;
	struct rmnet_port *port;
	int ret = 0;

	if (!dev) {
		rmnet_shs_crit_err[RMNET_SHS_NETDEV_ERR]++;
		return NOTIFY_DONE;
	}

	if (!(strncmp(dev->name, "rmnet_data", 10) == 0 ||
	      strncmp(dev->name, "r_rmnet_data", 12) == 0))
		return NOTIFY_DONE;

	switch (event) {
	case NETDEV_UNREGISTER:
		rmnet_shs_wq_reset_ep_active(dev);
		rmnet_vnd_total--;

		/* Deinitialize if last vnd is going down or if
		 * phy_dev is going down.
		 */
		if (!rmnet_vnd_total && rmnet_shs_cfg.rmnet_shs_init_complete) {
			pr_info("rmnet_shs deinit %s going down ", dev->name);
			RCU_INIT_POINTER(rmnet_shs_skb_entry, NULL);
			qmi_rmnet_ps_ind_deregister(rmnet_shs_cfg.port,
					    &rmnet_shs_cfg.rmnet_idl_ind_cb);
			rmnet_shs_cancel_table();
			rmnet_shs_rx_wq_exit();
			rmnet_shs_wq_exit();
			rmnet_shs_exit();
			trace_rmnet_shs_high(RMNET_SHS_MODULE,
					     RMNET_SHS_MODULE_INIT_WQ,
					     0xDEF, 0xDEF, 0xDEF,
					     0xDEF, NULL, NULL);
		}
		break;

	case NETDEV_REGISTER:
		rmnet_vnd_total++;

		if (rmnet_vnd_total && !rmnet_shs_cfg.rmnet_shs_init_complete) {
			pr_info("rmnet_shs initializing %s", dev->name);
			priv = netdev_priv(dev);
			port = rmnet_get_port(priv->real_dev);
			if (!port) {
				pr_err("rmnet_shs: invalid rmnet_port");
				break;
			}
			rmnet_shs_init(priv->real_dev, dev);
			rmnet_shs_wq_init(priv->real_dev);
			rmnet_shs_rx_wq_init();

			rmnet_shs_cfg.is_timer_init = 1;
		}
		rmnet_shs_wq_set_ep_active(dev);

		break;
	case NETDEV_UP:
		if (!rmnet_shs_cfg.is_reg_dl_mrk_ind &&
		    rmnet_shs_cfg.rmnet_shs_init_complete) {

			port = rmnet_shs_cfg.port;
			if (!port) {
				pr_err("rmnet_shs: invalid rmnet_cfg_port");
				break;
			}

			rmnet_shs_cfg.dl_mrk_ind_cb.priority =
				RMNET_SHS;
			if (port->data_format & RMNET_INGRESS_FORMAT_DL_MARKER_V2) {
				rmnet_shs_cfg.dl_mrk_ind_cb.dl_hdr_handler_v2 =
					&rmnet_shs_dl_hdr_handler_v2;
				rmnet_shs_cfg.dl_mrk_ind_cb.dl_trl_handler_v2 =
					&rmnet_shs_dl_trl_handler_v2;
			} else {
				rmnet_shs_cfg.dl_mrk_ind_cb.dl_hdr_handler =
					&rmnet_shs_dl_hdr_handler;
				rmnet_shs_cfg.dl_mrk_ind_cb.dl_trl_handler =
					&rmnet_shs_dl_trl_handler;
			}
			rmnet_shs_cfg.rmnet_idl_ind_cb.ps_on_handler =
					&rmnet_shs_ps_on_hdlr;
			rmnet_shs_cfg.rmnet_idl_ind_cb.ps_off_handler =
					&rmnet_shs_ps_off_hdlr;

			ret = rmnet_map_dl_ind_register(port,
						        &rmnet_shs_cfg.dl_mrk_ind_cb);
			if (ret)
				pr_err("%s(): rmnet dl_ind registration fail\n",
				       __func__);

			ret = qmi_rmnet_ps_ind_register(port,
						        &rmnet_shs_cfg.rmnet_idl_ind_cb);
			if (ret)
				pr_err("%s(): rmnet ps_ind registration fail\n",
				       __func__);
			rmnet_shs_update_cfg_mask();
			rmnet_shs_wq_refresh_new_flow_list();
			rmnet_shs_cfg.is_reg_dl_mrk_ind = 1;
			trace_rmnet_shs_high(RMNET_SHS_MODULE,
					     RMNET_SHS_MODULE_INIT_WQ,
					     0xDEF, 0xDEF, 0xDEF,
					     0xDEF, NULL, NULL);
			RCU_INIT_POINTER(rmnet_shs_skb_entry,
					 rmnet_shs_assign);
		}

		break;

	default:
		break;

	}
	return NOTIFY_DONE;
}

module_init(rmnet_shs_module_init);
module_exit(rmnet_shs_module_exit);
