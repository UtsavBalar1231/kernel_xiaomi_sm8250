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

#ifndef __DWC_ETH_QOS_DESC_H__

#define __DWC_ETH_QOS_DESC_H__

static INT allocate_buffer_and_desc(struct DWC_ETH_QOS_prv_data *);

static void DWC_ETH_QOS_wrapper_tx_descriptor_init(struct DWC_ETH_QOS_prv_data
						   *pdata);

static void DWC_ETH_QOS_wrapper_tx_descriptor_init_single_q(
	struct DWC_ETH_QOS_prv_data *pdata, UINT);

static void DWC_ETH_QOS_wrapper_rx_descriptor_init(struct DWC_ETH_QOS_prv_data
						   *pdata);

static void DWC_ETH_QOS_wrapper_rx_descriptor_init_single_q(
	struct DWC_ETH_QOS_prv_data *pdata, UINT);

#ifdef DWC_INET_LRO
static int DWC_ETH_QOS_get_skb_hdr(struct sk_buff *skb, void **iphdr,
				   void **tcph, u64 *hdr_flags, void *priv);
#endif

static void DWC_ETH_QOS_tx_free_mem(struct DWC_ETH_QOS_prv_data *);

static void DWC_ETH_QOS_rx_free_mem(struct DWC_ETH_QOS_prv_data *);

static unsigned int DWC_ETH_QOS_map_skb(struct net_device *, struct sk_buff *);

static void DWC_ETH_QOS_unmap_tx_skb(struct DWC_ETH_QOS_prv_data *,
				     struct DWC_ETH_QOS_tx_buffer *);

static void DWC_ETH_QOS_unmap_rx_skb(struct DWC_ETH_QOS_prv_data *,
				     struct DWC_ETH_QOS_rx_buffer *);

static void DWC_ETH_QOS_re_alloc_skb(
	struct DWC_ETH_QOS_prv_data *pdata, UINT);

static void DWC_ETH_QOS_tx_desc_free_mem(struct DWC_ETH_QOS_prv_data *pdata,
					 UINT tx_q_cnt);

static void DWC_ETH_QOS_tx_buf_free_mem(struct DWC_ETH_QOS_prv_data *pdata,
					UINT tx_q_cnt);

static void DWC_ETH_QOS_rx_desc_free_mem(struct DWC_ETH_QOS_prv_data *pdata,
					 UINT rx_q_cnt);

static void DWC_ETH_QOS_rx_buf_free_mem(struct DWC_ETH_QOS_prv_data *pdata,
					UINT rx_q_cnt);

static void DWC_ETH_QOS_rx_skb_free_mem(
	struct DWC_ETH_QOS_prv_data *pdata, UINT);

static void DWC_ETH_QOS_tx_skb_free_mem(struct DWC_ETH_QOS_prv_data *pdata,
					UINT);
#endif
