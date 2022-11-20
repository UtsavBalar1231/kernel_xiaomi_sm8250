/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
 * This file contain content copied from Synopsis driver,
 * provided under the license below
 */
/* =========================================================================
 * The Synopsys DWC ETHER QOS Software Driver and documentation (hereinafter
 * "Software") is an unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto.  Permission is hereby granted,
 * free of charge, to any person obtaining a copy of this software annotated
 * with this license and the Software, to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * =========================================================================
 */

#ifndef __DWC_ETH_QOS_DRV_H__

#define __DWC_ETH_QOS_DRV_H__

static int DWC_ETH_QOS_open(struct net_device *);

static int DWC_ETH_QOS_close(struct net_device *);

static void DWC_ETH_QOS_set_rx_mode(struct net_device *);

static int DWC_ETH_QOS_start_xmit(struct sk_buff *, struct net_device *);

static void DWC_ETH_QOS_tx_interrupt(struct net_device *,
				     struct DWC_ETH_QOS_prv_data *, UINT);

static struct net_device_stats *DWC_ETH_QOS_get_stats(struct net_device *);

#ifdef CONFIG_NET_POLL_CONTROLLER
static void DWC_ETH_QOS_poll_controller(struct net_device *);
#endif				/*end of CONFIG_NET_POLL_CONTROLLER */

static int DWC_ETH_QOS_set_features(
	struct net_device *dev, netdev_features_t features);

static netdev_features_t DWC_ETH_QOS_fix_features(
	struct net_device *dev, netdev_features_t features);

INT DWC_ETH_QOS_configure_remotewakeup(struct net_device *dev,
				       struct ifr_data_struct *req);

static void DWC_ETH_QOS_program_dcb_algorithm(
	struct DWC_ETH_QOS_prv_data *pdata, struct ifr_data_struct *req);

static void DWC_ETH_QOS_program_avb_algorithm(
	struct DWC_ETH_QOS_prv_data *pdata, struct ifr_data_struct *req);

static void DWC_ETH_QOS_config_tx_pbl(struct DWC_ETH_QOS_prv_data *pdata,
				      UINT tx_pbl, UINT ch_no);
static void DWC_ETH_QOS_config_rx_pbl(struct DWC_ETH_QOS_prv_data *pdata,
				      UINT rx_pbl, UINT ch_no);

static int DWC_ETH_QOS_handle_prv_ioctl(struct DWC_ETH_QOS_prv_data *pdata,
					struct ifr_data_struct *req);

static int DWC_ETH_QOS_handle_prv_ioctl_ipa(struct DWC_ETH_QOS_prv_data *pdata,
					struct ifreq *ifr);

static int DWC_ETH_QOS_ioctl(struct net_device *, struct ifreq *, int);

static INT DWC_ETH_QOS_change_mtu(struct net_device *dev, INT new_mtu);

static int DWC_ETH_QOS_clean_split_hdr_rx_irq(
		struct DWC_ETH_QOS_prv_data *pdata, int quota, UINT);

static int DWC_ETH_QOS_clean_jumbo_rx_irq(struct DWC_ETH_QOS_prv_data *pdata,
					  int quota, UINT);

static int DWC_ETH_QOS_clean_rx_irq(struct DWC_ETH_QOS_prv_data *pdata,
				    int quota, UINT);

static void DWC_ETH_QOS_consume_page(struct DWC_ETH_QOS_rx_buffer *buffer,
				     struct sk_buff *skb,
				     u16 length, u16 buf2_len);

static void DWC_ETH_QOS_receive_skb(struct DWC_ETH_QOS_prv_data *pdata,
				    struct net_device *dev,
				    struct sk_buff *skb,
				    UINT);

static void DWC_ETH_QOS_configure_rx_fun_ptr(struct DWC_ETH_QOS_prv_data
					     *pdata);

static int DWC_ETH_QOS_alloc_split_hdr_rx_buf(
	struct DWC_ETH_QOS_prv_data *pdata,
	struct DWC_ETH_QOS_rx_buffer *buffer,
	UINT qinx, gfp_t gfp);

static int DWC_ETH_QOS_alloc_jumbo_rx_buf(struct DWC_ETH_QOS_prv_data *pdata,
					  struct DWC_ETH_QOS_rx_buffer *buffer,  UINT qinx,
					  gfp_t gfp);

static int DWC_ETH_QOS_alloc_rx_buf(struct DWC_ETH_QOS_prv_data *pdata,
				    struct DWC_ETH_QOS_rx_buffer *buffer, UINT qinx,
				    gfp_t gfp);

static void DWC_ETH_QOS_default_common_confs(struct DWC_ETH_QOS_prv_data
					     *pdata);
static void DWC_ETH_QOS_default_tx_confs(struct DWC_ETH_QOS_prv_data *pdata);
static void DWC_ETH_QOS_default_tx_confs_single_q(struct DWC_ETH_QOS_prv_data
						  *pdata, UINT);
static void DWC_ETH_QOS_default_rx_confs(struct DWC_ETH_QOS_prv_data *pdata);
static void DWC_ETH_QOS_default_rx_confs_single_q(struct DWC_ETH_QOS_prv_data
						  *pdata, UINT);

int DWC_ETH_QOS_poll(struct DWC_ETH_QOS_prv_data *pdata, int budget, int qinx);

static void DWC_ETH_QOS_mmc_setup(struct DWC_ETH_QOS_prv_data *pdata);
inline unsigned int DWC_ETH_QOS_reg_read(volatile ULONG * ptr);

#ifdef DWC_ETH_QOS_QUEUE_SELECT_ALGO
u16	DWC_ETH_QOS_select_queue(struct net_device *dev, struct sk_buff *skb,
	void *accel_priv, select_queue_fallback_t fallback);
#endif

static int DWC_ETH_QOS_vlan_rx_add_vid(
	struct net_device *dev, __be16 proto, u16 vid);
static int DWC_ETH_QOS_vlan_rx_kill_vid(struct net_device *dev,
					__be16 proto, u16 vid);
#endif
