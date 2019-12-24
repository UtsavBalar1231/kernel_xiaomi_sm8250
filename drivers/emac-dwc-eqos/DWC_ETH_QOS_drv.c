/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

/*!@file: DWC_ETH_QOS_drv.c
 * @brief: Driver functions.
 */

#include "DWC_ETH_QOS_yheader.h"
#include "DWC_ETH_QOS_yapphdr.h"
#include "DWC_ETH_QOS_drv.h"
#include "DWC_ETH_QOS_ipa.h"

extern ULONG dwc_eth_qos_base_addr;
extern bool avb_class_b_msg_wq_flag;
extern bool avb_class_a_msg_wq_flag;
extern wait_queue_head_t avb_class_a_msg_wq;
extern wait_queue_head_t avb_class_b_msg_wq;


#include "DWC_ETH_QOS_yregacc.h"
#define DEFAULT_START_TIME 0x1900

static INT DWC_ETH_QOS_GSTATUS;

/* SA(Source Address) operations on TX */
unsigned char mac_addr0[6] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 };
unsigned char mac_addr1[6] = { 0x00, 0x66, 0x77, 0x88, 0x99, 0xaa };

/* module parameters for configuring the queue modes
 * set default mode as GENERIC
 *
 */
static int q_op_mode[DWC_ETH_QOS_MAX_TX_QUEUE_CNT] = {
	DWC_ETH_QOS_Q_GENERIC,
	DWC_ETH_QOS_Q_GENERIC,
	DWC_ETH_QOS_Q_GENERIC,
	DWC_ETH_QOS_Q_GENERIC,
	DWC_ETH_QOS_Q_GENERIC,
	DWC_ETH_QOS_Q_GENERIC,
	DWC_ETH_QOS_Q_GENERIC,
	DWC_ETH_QOS_Q_GENERIC
};
module_param_array(q_op_mode, int, NULL, 0644);
MODULE_PARM_DESC(q_op_mode,
		 "MTL queue operation mode [0-DISABLED, 1-AVB, 2-DCB, 3-GENERIC]");

#ifdef PER_CH_INT
void DWC_ETH_QOS_dis_en_ch_intr(struct DWC_ETH_QOS_prv_data *pdata,
      bool enable)
{
	int cnt;

	if (!pdata->per_ch_intr_en)
		return;

	for (cnt=0; cnt <= pdata->hw_feat.tx_ch_cnt; cnt++) {
		if (enable)
			enable_irq(pdata->res_data->tx_ch_intr[cnt]);
		else
			disable_irq(pdata->res_data->tx_ch_intr[cnt]);
   }

	for (cnt=0; cnt <= pdata->hw_feat.rx_ch_cnt; cnt++) {
		if (enable)
			enable_irq(pdata->res_data->rx_ch_intr[cnt]);
		else
			disable_irq(pdata->res_data->rx_ch_intr[cnt]);
   }
}

void DWC_ETH_QOS_deregister_per_ch_intr(struct DWC_ETH_QOS_prv_data *pdata)
{
	int cnt;

	if (!pdata->per_ch_intr_en)
		return;

	for (cnt=0; cnt <= pdata->hw_feat.tx_ch_cnt; cnt++)
		free_irq(pdata->res_data->tx_ch_intr[cnt],
		   pdata);

	for (cnt=0; cnt <= pdata->hw_feat.rx_ch_cnt; cnt++)
		free_irq(pdata->res_data->rx_ch_intr[cnt],
		   pdata);

	DMA_BMR_INTMWR(0x0);
}

irqreturn_t DWC_ETH_QOS_PER_CH_ISR(int irq, void *dev_data)
{
	struct DWC_ETH_QOS_prv_data *pdata =
	    (struct DWC_ETH_QOS_prv_data *)dev_data;
	int chinx;

	for (chinx = 0; chinx <= pdata->hw_feat.tx_ch_cnt; chinx++) {
		if(pdata->res_data->tx_ch_intr[chinx] == irq) {
			EMACDBG("Received irq: %d for Tx ch: %d\n", irq, chinx);
			DWC_ETH_QOS_handle_DMA_Int(pdata, chinx, true);
		}
	}
	for (chinx = 0; chinx <= pdata->hw_feat.rx_ch_cnt; chinx++) {
		if(pdata->res_data->rx_ch_intr[chinx] == irq) {
			EMACDBG("Received irq: %d for Rx ch: %d\n", irq, chinx);
			DWC_ETH_QOS_handle_DMA_Int(pdata, chinx, true);
		}
	}

	return IRQ_HANDLED;
}

int DWC_ETH_QOS_register_per_ch_intr(struct DWC_ETH_QOS_prv_data *pdata, int intm)
{
	int ret, cnt;
	unsigned long flags = IRQF_SHARED;
	if (intm == 0x0) {
		EMACDBG("Registering per channel interrupt as edge\n");
		flags = IRQF_TRIGGER_RISING;
	}

	for (cnt=0; cnt <= pdata->hw_feat.tx_ch_cnt; cnt++) {
		ret = request_irq(pdata->res_data->tx_ch_intr[cnt],
			DWC_ETH_QOS_PER_CH_ISR, flags, DEV_NAME, pdata);
		if (ret != 0) {
			EMACDBG("Unable to reg Tx Channel[%d] IRQ\n",
				   cnt);
			return -EBUSY;
		}
		else
			EMACDBG("Registered Tx_CH[%d]:[%d] IRQ\n",
				   cnt, pdata->res_data->tx_ch_intr[cnt]);
	}

	for (cnt=0; cnt <= pdata->hw_feat.rx_ch_cnt; cnt++) {
		ret = request_irq(pdata->res_data->rx_ch_intr[cnt],
			DWC_ETH_QOS_PER_CH_ISR, flags, DEV_NAME, pdata);
		if (ret != 0) {
			EMACDBG("Unable to reg Rx Channel[%d] IRQ\n",
				   cnt);
			return -EBUSY;
		}
		else
			EMACDBG("Registered Rx_CH[%d]:[%d] IRQ\n",
				   cnt, pdata->res_data->rx_ch_intr[cnt]);
	}

	DMA_BMR_INTMWR(intm);
	pdata->per_ch_intr_en = true;
	return 0;
}
#endif

void DWC_ETH_QOS_stop_all_ch_tx_dma(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	UINT qinx;

	DBGPR("-->DWC_ETH_QOS_stop_all_ch_tx_dma\n");

	for (qinx = 0; qinx < DWC_ETH_QOS_TX_QUEUE_CNT; qinx++) {
		if (pdata->ipa_enabled && (qinx == IPA_DMA_TX_CH))
			continue;
		hw_if->stop_dma_tx(qinx);
	}

	DBGPR("<--DWC_ETH_QOS_stop_all_ch_tx_dma\n");
}

static void DWC_ETH_QOS_stop_all_ch_rx_dma(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	UINT qinx;

	DBGPR("-->DWC_ETH_QOS_stop_all_ch_rx_dma\n");

	for (qinx = 0; qinx < DWC_ETH_QOS_RX_QUEUE_CNT; qinx++) {
		if (pdata->ipa_enabled && (qinx == IPA_DMA_RX_CH))
			continue;
		hw_if->stop_dma_rx(qinx);
	}

	DBGPR("<--DWC_ETH_QOS_stop_all_ch_rx_dma\n");
}

static void DWC_ETH_QOS_start_all_ch_tx_dma(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	UINT i;

	DBGPR("-->DWC_ETH_QOS_start_all_ch_tx_dma\n");

	for (i = 0; i < DWC_ETH_QOS_TX_QUEUE_CNT; i++) {
		if (pdata->ipa_enabled && (i == IPA_DMA_TX_CH))
			continue;
		hw_if->start_dma_tx(i);
	}

	DBGPR("<--DWC_ETH_QOS_start_all_ch_tx_dma\n");
}

static void DWC_ETH_QOS_start_all_ch_rx_dma(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	UINT i;

	DBGPR("-->DWC_ETH_QOS_start_all_ch_rx_dma\n");

	for (i = 0; i < DWC_ETH_QOS_RX_QUEUE_CNT; i++) {
		if (pdata->ipa_enabled && (i == IPA_DMA_RX_CH))
			continue;
		hw_if->start_dma_rx(i);
	}

	DBGPR("<--DWC_ETH_QOS_start_all_ch_rx_dma\n");
}

static void DWC_ETH_QOS_napi_enable_mq(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct DWC_ETH_QOS_rx_queue *rx_queue = NULL;
	int qinx;

	DBGPR("-->DWC_ETH_QOS_napi_enable_mq\n");

	for (qinx = 0; qinx < DWC_ETH_QOS_RX_QUEUE_CNT; qinx++) {
		if (pdata->ipa_enabled && (qinx == IPA_DMA_RX_CH))
			continue;
		rx_queue = GET_RX_QUEUE_PTR(qinx);
		napi_enable(&rx_queue->napi);
	}

	DBGPR("<--DWC_ETH_QOS_napi_enable_mq\n");
}

static void DWC_ETH_QOS_all_ch_napi_disable(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct DWC_ETH_QOS_rx_queue *rx_queue = NULL;
	int qinx;

	DBGPR("-->DWC_ETH_QOS_all_ch_napi_disable\n");

	for (qinx = 0; qinx < DWC_ETH_QOS_RX_QUEUE_CNT; qinx++) {
		if (pdata->ipa_enabled && (qinx == IPA_DMA_RX_CH))
			continue;
		rx_queue = GET_RX_QUEUE_PTR(qinx);
		napi_disable(&rx_queue->napi);
	}

	DBGPR("<--DWC_ETH_QOS_all_ch_napi_disable\n");
}

/*!
 * \details This function is invoked to stop device operation
 * Following operations are performed in this function.
 * - Stop the queue.
 * - Stops DMA TX and RX.
 * - Free the TX and RX skb's.
 * - Issues soft reset to device.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */

static void DWC_ETH_QOS_stop_dev(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct desc_if_struct *desc_if = &pdata->desc_if;

	DBGPR("-->DWC_ETH_QOS_stop_dev\n");

	netif_tx_disable(pdata->dev);

	DWC_ETH_QOS_all_ch_napi_disable(pdata);

	/* stop DMA TX/RX */
	DWC_ETH_QOS_stop_all_ch_tx_dma(pdata);
	DWC_ETH_QOS_stop_all_ch_rx_dma(pdata);

	/* issue software reset to device */
	hw_if->exit();

	/* free tx skb's */
	desc_if->tx_skb_free_mem(pdata, DWC_ETH_QOS_TX_QUEUE_CNT);
	/* free rx skb's */
	desc_if->rx_skb_free_mem(pdata, DWC_ETH_QOS_RX_QUEUE_CNT);

	DBGPR("<--DWC_ETH_QOS_stop_dev\n");
}

static void DWC_ETH_QOS_tx_desc_mang_ds_dump(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct DWC_ETH_QOS_tx_wrapper_descriptor *tx_desc_data = NULL;
	struct s_TX_NORMAL_DESC *tx_desc = NULL;
	int qinx, i;

#ifndef YDEBUG
	return;
#endif
	dev_alert(&pdata->pdev->dev, "/**** TX DESC MANAGEMENT DATA STRUCTURE DUMP ****/\n");

	dev_alert(&pdata->pdev->dev, "TX_DESC_QUEUE_CNT = %d\n", DWC_ETH_QOS_TX_QUEUE_CNT);
	for (qinx = 0; qinx < DWC_ETH_QOS_TX_QUEUE_CNT; qinx++) {
		tx_desc_data = GET_TX_WRAPPER_DESC(qinx);

		dev_alert(&pdata->pdev->dev, "DMA CHANNEL = %d\n", qinx);

		dev_alert(&pdata->pdev->dev, "\tcur_tx           = %d\n",
			  tx_desc_data->cur_tx);
		dev_alert(&pdata->pdev->dev, "\tdirty_tx         = %d\n",
			  tx_desc_data->dirty_tx);
		dev_alert(&pdata->pdev->dev, "\tfree_desc_cnt    = %d\n",
			  tx_desc_data->free_desc_cnt);
		dev_alert(&pdata->pdev->dev, "\ttx_pkt_queued    = %d\n",
			  tx_desc_data->tx_pkt_queued);
		dev_alert(&pdata->pdev->dev, "\tqueue_stopped    = %d\n",
			  tx_desc_data->queue_stopped);
		dev_alert(&pdata->pdev->dev, "\tpacket_count     = %d\n",
			  tx_desc_data->packet_count);
		dev_alert(&pdata->pdev->dev, "\ttx_threshold_val = %d\n",
			  tx_desc_data->tx_threshold_val);
		dev_alert(&pdata->pdev->dev, "\ttsf_on           = %d\n",
			  tx_desc_data->tsf_on);
		dev_alert(&pdata->pdev->dev, "\tosf_on           = %d\n",
			  tx_desc_data->osf_on);
		dev_alert(&pdata->pdev->dev, "\ttx_pbl           = %d\n",
			  tx_desc_data->tx_pbl);

		dev_alert(&pdata->pdev->dev, "\t[<desc_add> <index >] = <TDES0> : <TDES1> : <TDES2> : <TDES3>\n");
		for (i = 0; i < pdata->tx_queue[qinx].desc_cnt; i++) {
			tx_desc = GET_TX_DESC_PTR(qinx, i);
			dev_alert(&pdata->pdev->dev, "\t[%4p %03d] = %#x : %#x : %#x : %#x\n",
				  tx_desc, i, tx_desc->TDES0, tx_desc->TDES1,
				tx_desc->TDES2, tx_desc->TDES3);
		}
	}

	dev_alert(&pdata->pdev->dev, "/************************************************/\n");
}

static void DWC_ETH_QOS_rx_desc_mang_ds_dump(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct DWC_ETH_QOS_rx_wrapper_descriptor *rx_desc_data = NULL;
	struct s_RX_NORMAL_DESC *rx_desc = NULL;
	int qinx, i;
	dma_addr_t *base_ptr = GET_RX_BUFF_POOL_BASE_ADRR(IPA_DMA_RX_CH);
	struct DWC_ETH_QOS_rx_buffer *rx_buf_ptrs = NULL;

#ifndef YDEBUG
	return;
#endif
	dev_alert(&pdata->pdev->dev, "/**** RX DESC MANAGEMENT DATA STRUCTURE DUMP ****/\n");

	dev_alert(&pdata->pdev->dev, "RX_DESC_QUEUE_CNT = %d\n", DWC_ETH_QOS_RX_QUEUE_CNT);
	for (qinx = 0; qinx < DWC_ETH_QOS_RX_QUEUE_CNT; qinx++) {
		rx_desc_data = GET_RX_WRAPPER_DESC(qinx);

		dev_alert(&pdata->pdev->dev, "DMA CHANNEL = %d\n", qinx);

		dev_alert(&pdata->pdev->dev, "\tcur_rx                = %d\n",
			  rx_desc_data->cur_rx);
		dev_alert(&pdata->pdev->dev, "\tdirty_rx              = %d\n",
			  rx_desc_data->dirty_rx);
		dev_alert(&pdata->pdev->dev, "\tpkt_received          = %d\n",
			  rx_desc_data->pkt_received);
		dev_alert(&pdata->pdev->dev, "\tskb_realloc_idx       = %d\n",
			  rx_desc_data->skb_realloc_idx);
		dev_alert(&pdata->pdev->dev, "\tskb_realloc_threshold = %d\n",
			  rx_desc_data->skb_realloc_threshold);
		dev_alert(&pdata->pdev->dev, "\tuse_riwt              = %d\n",
			  rx_desc_data->use_riwt);
		dev_alert(&pdata->pdev->dev, "\trx_riwt               = %d\n",
			  rx_desc_data->rx_riwt);
		dev_alert(&pdata->pdev->dev, "\trx_coal_frames        = %d\n",
			  rx_desc_data->rx_coal_frames);
		dev_alert(&pdata->pdev->dev, "\trx_threshold_val      = %d\n",
			  rx_desc_data->rx_threshold_val);
		dev_alert(&pdata->pdev->dev, "\trsf_on                = %d\n",
			  rx_desc_data->rsf_on);
		dev_alert(&pdata->pdev->dev, "\trx_pbl                = %d\n",
			  rx_desc_data->rx_pbl);
		if (pdata->ipa_enabled) {
			dev_alert(&pdata->pdev->dev, "\tRX Base Ring     = %p\n",
				     &GET_RX_BUFF_POOL_BASE_ADRR(qinx));
		}

		dev_alert(&pdata->pdev->dev, "\t[<desc_add> <index >] = <RDES0> : <RDES1> : <RDES2> : <RDES3>\n");
		for (i = 0; i < pdata->rx_queue[qinx].desc_cnt; i++) {
			rx_desc = GET_RX_DESC_PTR(qinx, i);
			dev_alert(&pdata->pdev->dev, "\t[%4p %03d] = %#x : %#x : %#x : %#x\n",
				  rx_desc, i, rx_desc->RDES0, rx_desc->RDES1,
				rx_desc->RDES2, rx_desc->RDES3);
			if (pdata->ipa_enabled) {
				rx_buf_ptrs = GET_RX_BUF_PTR(qinx, i);
				if ((rx_buf_ptrs != NULL)  &&
				     (GET_RX_BUFF_POOL_BASE_ADRR(qinx) != NULL)) {
					dev_alert(&pdata->pdev->dev, "\t skb mempool %p skb rx buf %p ,"
						    "skb len %d skb dma %p base %p\n",
						    (void *)GET_RX_BUFF_DMA_ADDR(qinx, i),
						    rx_buf_ptrs->skb, rx_buf_ptrs->len,
						    (void *)rx_buf_ptrs->dma, (void *)(base_ptr + i));
				}
			}
		}
	}

	dev_alert(&pdata->pdev->dev, "/************************************************/\n");
}

static void DWC_ETH_QOS_restart_phy(struct DWC_ETH_QOS_prv_data *pdata)
{
	DBGPR("-->DWC_ETH_QOS_restart_phy\n");

	pdata->oldlink = 0;
	pdata->speed = 0;
	pdata->oldduplex = -1;

	if (pdata->phydev)
		phy_start_aneg(pdata->phydev);

	DBGPR("<--DWC_ETH_QOS_restart_phy\n");
}

/*!
 * \details This function is invoked to start the device operation
 * Following operations are performed in this function.
 * - Initialize software states
 * - Initialize the TX and RX descriptors queue.
 * - Initialize the device to know state
 * - Start the queue.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */

static void DWC_ETH_QOS_start_dev(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct desc_if_struct *desc_if = &pdata->desc_if;

	DBGPR("-->DWC_ETH_QOS_start_dev\n");

	/* reset all variables */
	DWC_ETH_QOS_default_common_confs(pdata);
	DWC_ETH_QOS_default_tx_confs(pdata);
	DWC_ETH_QOS_default_rx_confs(pdata);

	DWC_ETH_QOS_configure_rx_fun_ptr(pdata);

	DWC_ETH_QOS_napi_enable_mq(pdata);

	/* reinit descriptor */
	desc_if->wrapper_tx_desc_init(pdata);
	desc_if->wrapper_rx_desc_init(pdata);

	DWC_ETH_QOS_tx_desc_mang_ds_dump(pdata);
	DWC_ETH_QOS_rx_desc_mang_ds_dump(pdata);

	/* initializes MAC and DMA */
	hw_if->init(pdata);

	if (pdata->vlan_hash_filtering)
		hw_if->update_vlan_hash_table_reg(pdata->vlan_ht_or_id);
	else
		hw_if->update_vlan_id(pdata->vlan_ht_or_id);

	DWC_ETH_QOS_restart_phy(pdata);

	pdata->eee_enabled = DWC_ETH_QOS_eee_init(pdata);

	netif_tx_wake_all_queues(pdata->dev);

	DBGPR("<--DWC_ETH_QOS_start_dev\n");
}

/*!
 * \details This function is invoked by isr handler when device issues an FATAL
 * bus error interrupt.  Following operations are performed in this function.
 * - Stop the device.
 * - Start the device
 *
 * \param[in] pdata – pointer to private data structure.
 * \param[in] qinx – queue number.
 *
 * \return void
 */

static void DWC_ETH_QOS_restart_dev(struct DWC_ETH_QOS_prv_data *pdata,
				    UINT qinx)
{
	struct desc_if_struct *desc_if = &pdata->desc_if;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct DWC_ETH_QOS_rx_queue *rx_queue = NULL;
	int reg_val;

	DBGPR("-->DWC_ETH_QOS_restart_dev\n");

	EMACERR("FBE received for queue = %d\n", qinx);
	DMA_CHTDR_CURTDESAPTR_UDFRD(qinx, reg_val);
	EMACERR("EMAC_DMA_CHi_CURRENT_APP_TXDESC = %#x\n", reg_val);
	DMA_CHRDR_CURRDESAPTR_UDFRD(qinx, reg_val);
	EMACERR("EMAC_DMA_CHi_CURRENT_APP_RXDESC = %#x\n", reg_val);
	DMA_CHTBAR_CURTBUFAPTR_UDFRD(qinx, reg_val);
	EMACERR("EMAC_DMA_CHi_CURRENT_APP_TXBUFFER = %#x\n", reg_val);
	DMA_CHRBAR_CURRBUFAPTR_UDFRD(qinx, reg_val);
	EMACERR("EMAC_DMA_CHi_CURRENT_APP_RXBUFFER = %#x\n", reg_val);

	netif_stop_subqueue(pdata->dev, qinx);

	/* stop DMA TX */
	hw_if->stop_dma_tx(qinx);

	/* free tx skb's */
	desc_if->tx_skb_free_mem_single_q(pdata, qinx);

	if (qinx < DWC_ETH_QOS_RX_QUEUE_CNT) {
		rx_queue = GET_RX_QUEUE_PTR(qinx);

		if (!(pdata->ipa_enabled && (qinx == IPA_DMA_RX_CH)))
			napi_disable(&rx_queue->napi);

		/* stop DMA RX */
		hw_if->stop_dma_rx(qinx);
		/* free rx skb's */
		desc_if->rx_skb_free_mem_single_q(pdata, qinx);
	}

	if ((DWC_ETH_QOS_TX_QUEUE_CNT == 0) &&
	    (DWC_ETH_QOS_RX_QUEUE_CNT == 0)) {
		/* issue software reset to device */
		hw_if->exit();

		DWC_ETH_QOS_configure_rx_fun_ptr(pdata);
		DWC_ETH_QOS_default_common_confs(pdata);
	}
	/* reset all Tx variables */
	DWC_ETH_QOS_default_tx_confs_single_q(pdata, qinx);

	/* reinit Tx descriptor */
	desc_if->wrapper_tx_desc_init_single_q(pdata, qinx);

	if (qinx < DWC_ETH_QOS_RX_QUEUE_CNT) {
		/* reset all Rx variables */
		DWC_ETH_QOS_default_rx_confs_single_q(pdata, qinx);
		/* reinit Rx descriptor */
		desc_if->wrapper_rx_desc_init_single_q(pdata, qinx);

		if (!(pdata->ipa_enabled && (qinx == IPA_DMA_RX_CH)))
			napi_enable(&rx_queue->napi);
	}

	/* initializes MAC and DMA
	 * NOTE : Do we need to init only one channel
	 * which generate FBE
	 */
	hw_if->init(pdata);

	DWC_ETH_QOS_restart_phy(pdata);

	netif_wake_subqueue(pdata->dev, qinx);

	DBGPR("<--DWC_ETH_QOS_restart_dev\n");
}

void DWC_ETH_QOS_disable_all_ch_rx_interrpt(
			struct DWC_ETH_QOS_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	UINT qinx;

	DBGPR("-->DWC_ETH_QOS_disable_all_ch_rx_interrpt\n");

	for (qinx = 0; qinx < DWC_ETH_QOS_RX_QUEUE_CNT; qinx++) {
		  if (pdata->ipa_enabled && qinx == IPA_DMA_RX_CH)
			 continue;
		hw_if->disable_rx_interrupt(qinx);
	}

	DBGPR("<--DWC_ETH_QOS_disable_all_ch_rx_interrpt\n");
}

void DWC_ETH_QOS_enable_all_ch_rx_interrpt(
			struct DWC_ETH_QOS_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	UINT qinx;

	DBGPR("-->DWC_ETH_QOS_enable_all_ch_rx_interrpt\n");

	for (qinx = 0; qinx < DWC_ETH_QOS_RX_QUEUE_CNT; qinx++) {
	   if (pdata->ipa_enabled && qinx == IPA_DMA_RX_CH)
		  continue;
		hw_if->enable_rx_interrupt(qinx);
	}

	DBGPR("<--DWC_ETH_QOS_enable_all_ch_rx_interrpt\n");
}

#ifdef PER_CH_INT
/*!
 * \brief Per Channel Interrupt Service Routine
 * \details Per Channel Interrupt Service Routine
 *
 * \param[in] irq         - interrupt number for particular device
 * \param[in] device_id   - pointer to device structure
 * \return returns positive integer
 * \retval IRQ_HANDLED
 */

void DWC_ETH_QOS_handle_DMA_Int(struct DWC_ETH_QOS_prv_data *pdata, int chinx, bool per_ch)
{
	ULONG VARDMA_SR, VARDMA_IER;
	UINT qinx = chinx;
	int napi_sched = 0;
	struct DWC_ETH_QOS_rx_queue *rx_queue = NULL;
	struct net_device *dev = pdata->dev;

	if (qinx < DWC_ETH_QOS_RX_QUEUE_CNT)
		rx_queue = GET_RX_QUEUE_PTR(qinx);

	DMA_SR_RGRD(qinx, VARDMA_SR);

	/* Mask RI/TI interrupts */
	if (per_ch)
		VARDMA_SR &= DMA_PER_CH_TI_RI_INT_MASK;
	else
		VARDMA_SR &= DMA_CH_TX_RX_INT_MASK;

	/* clear interrupts */
	DMA_SR_RGWR(qinx, VARDMA_SR);

	DMA_IER_RGRD(qinx, VARDMA_IER);
	/* handle only those DMA interrupts which are enabled */
	VARDMA_SR = (VARDMA_SR & VARDMA_IER);

	EMACDBG("DMA_SR[%d] = %#lx\n", qinx, VARDMA_SR);

	if (VARDMA_SR == 0) return;

	if (qinx < DWC_ETH_QOS_RX_QUEUE_CNT &&
		((GET_VALUE(VARDMA_SR, DMA_SR_RI_LPOS, DMA_SR_RI_HPOS) & 1) ||
		(GET_VALUE(VARDMA_SR, DMA_SR_RBU_LPOS, DMA_SR_RBU_HPOS) & 1))) {
		if (!napi_sched) {
			napi_sched = 1;
			if (likely(napi_schedule_prep(&rx_queue->napi))) {
				DWC_ETH_QOS_disable_all_ch_rx_interrpt(pdata);
				__napi_schedule(&rx_queue->napi);
			} else {
				dev_alert(&pdata->pdev->dev, "driver bug! Rx interrupt while in poll\n");
				DWC_ETH_QOS_disable_all_ch_rx_interrpt(pdata);
			}

			if ((GET_VALUE(VARDMA_SR, DMA_SR_RI_LPOS, DMA_SR_RI_HPOS) & 1)) pdata->xstats.rx_normal_irq_n[qinx]++;
			else pdata->xstats.rx_buf_unavailable_irq_n[qinx]++;
		}
	}
	if (GET_VALUE(VARDMA_SR, DMA_SR_TI_LPOS, DMA_SR_TI_HPOS) & 1) {
		pdata->xstats.tx_normal_irq_n[qinx]++;
		DWC_ETH_QOS_tx_interrupt(dev, pdata, qinx);
	}
	if (GET_VALUE(VARDMA_SR, DMA_SR_TPS_LPOS, DMA_SR_TPS_HPOS) & 1) {
		pdata->xstats.tx_process_stopped_irq_n[qinx]++;
		DWC_ETH_QOS_GSTATUS = -E_DMA_SR_TPS;
	}
	if (GET_VALUE(VARDMA_SR, DMA_SR_TBU_LPOS, DMA_SR_TBU_HPOS) & 1) {
		pdata->xstats.tx_buf_unavailable_irq_n[qinx]++;
		DWC_ETH_QOS_GSTATUS = -E_DMA_SR_TBU;
	}
	if (qinx < DWC_ETH_QOS_RX_QUEUE_CNT) {
		if (GET_VALUE(VARDMA_SR, DMA_SR_RPS_LPOS, DMA_SR_RPS_HPOS) & 1) {
			pdata->xstats.rx_process_stopped_irq_n[qinx]++;
			DWC_ETH_QOS_GSTATUS = -E_DMA_SR_RPS;
		}
		if (GET_VALUE(VARDMA_SR, DMA_SR_RWT_LPOS, DMA_SR_RWT_HPOS) & 1) {
			pdata->xstats.rx_watchdog_irq_n++;
			DWC_ETH_QOS_GSTATUS = S_DMA_SR_RWT;
		}
	}
	if (GET_VALUE(VARDMA_SR, DMA_SR_FBE_LPOS, DMA_SR_FBE_HPOS) & 1) {
		pdata->xstats.fatal_bus_error_irq_n++;
		DWC_ETH_QOS_GSTATUS = -E_DMA_SR_FBE;
		DWC_ETH_QOS_restart_dev(pdata, qinx);

	}
}
#endif

/**
 * DWC_ETH_QOS_defer_phy_isr_work - Scheduled by the phy isr
 *  @work: work_struct
 */
void DWC_ETH_QOS_defer_phy_isr_work(struct work_struct *work)
{
	struct DWC_ETH_QOS_prv_data *pdata =
		container_of(work, struct DWC_ETH_QOS_prv_data, emac_phy_work);

	EMACDBG("Enter\n");

	if (pdata->clks_suspended)
		wait_for_completion(&pdata->clk_enable_done);

	DWC_ETH_QOS_handle_phy_interrupt(pdata);

	EMACDBG("Exit\n");
}

/*!
 * \brief Interrupt Service Routine
 * \details Interrupt Service Routine for PHY interrupt
 * \param[in] irq         - PHY interrupt number for particular
 * device
 * \param[in] dev_data   - pointer to device structure
 * \return returns positive integer
 * \retval IRQ_HANDLED
 */

irqreturn_t DWC_ETH_QOS_PHY_ISR(int irq, void *dev_data)
{
	struct DWC_ETH_QOS_prv_data *pdata =
		(struct DWC_ETH_QOS_prv_data *)dev_data;

	/* Set a wakeup event to ensure enough time for processing */
	pm_wakeup_event(&pdata->pdev->dev, 5000);

	/* Queue the work in system_wq */
	queue_work(system_wq, &pdata->emac_phy_work);

	return IRQ_HANDLED;
}

/*!
 * \brief Handle PHY interrupt
 * \details
 * \param[in] pdata - pointer to driver private structure
 * \return none
 */

void DWC_ETH_QOS_handle_phy_interrupt(struct DWC_ETH_QOS_prv_data *pdata)
{

	int phy_intr_status = 0;
	int micrel_intr_status = 0;
	EMACDBG("Enter\n");

	if ((pdata->phydev->phy_id & pdata->phydev->drv->phy_id_mask) == MICREL_PHY_ID) {
		DWC_ETH_QOS_mdio_read_direct(
			pdata, pdata->phyaddr, DWC_ETH_QOS_BASIC_STATUS, &phy_intr_status);
		EMACDBG(
			"Basic Status Reg (%#x) = %#x\n", DWC_ETH_QOS_BASIC_STATUS, phy_intr_status);

		DWC_ETH_QOS_mdio_read_direct(
			pdata, pdata->phyaddr, DWC_ETH_QOS_MICREL_PHY_INTCS, &micrel_intr_status);
		EMACDBG(
			"MICREL PHY Intr EN Reg (%#x) = %#x\n", DWC_ETH_QOS_MICREL_PHY_INTCS, micrel_intr_status);

		/* Call ack interrupt to clear the WOL interrupt status fields */
		if (pdata->phydev->drv->ack_interrupt)
			pdata->phydev->drv->ack_interrupt(pdata->phydev);

		/* Interrupt received for link state change */
		if (phy_intr_status & LINK_STATE_MASK) {
			EMACDBG("Interrupt received for link UP state\n");
			phy_mac_interrupt(pdata->phydev, LINK_UP);
		} else if (!(phy_intr_status & LINK_STATE_MASK)) {
			EMACDBG("Interrupt received for link DOWN state\n");
			phy_mac_interrupt(pdata->phydev, LINK_DOWN);
		} else if (!(phy_intr_status & AUTONEG_STATE_MASK)) {
			EMACDBG("Interrupt received for link down with"
					" auto-negotiation error\n");
		}
	} else {
		DWC_ETH_QOS_mdio_read_direct(
		pdata, pdata->phyaddr, DWC_ETH_QOS_PHY_INTR_STATUS, &phy_intr_status);
		EMACDBG("Phy Interrupt status Reg at offset 0x13 = %#x\n", phy_intr_status);
		/* Interrupt received for link state change */
		if (phy_intr_status & LINK_UP_STATE) {
			pdata->hw_if.stop_mac_tx_rx();
			EMACDBG("Interrupt received for link UP state\n");
			phy_mac_interrupt(pdata->phydev, LINK_UP);
		} else if (phy_intr_status & LINK_DOWN_STATE) {
			EMACDBG("Interrupt received for link DOWN state\n");
			phy_mac_interrupt(pdata->phydev, LINK_DOWN);
		} else if (phy_intr_status & AUTO_NEG_ERROR) {
			EMACDBG("Interrupt received for link down with"
				" auto-negotiation error\n");
		} else if (phy_intr_status & PHY_WOL) {
			EMACDBG("Interrupt received for WoL packet\n");
		}
	}

	EMACDBG("Exit\n");
	return;
}

/*!
 * \brief Interrupt Service Routine
 * \details Interrupt Service Routine
 *
 * \param[in] irq         - interrupt number for particular device
 * \param[in] dev_data   - pointer to device structure
 * \return returns positive integer
 * \retval IRQ_HANDLED
 */

irqreturn_t DWC_ETH_QOS_ISR_SW_DWC_ETH_QOS(int irq, void *dev_data)
{
	ULONG VARDMA_ISR;
	ULONG VARMAC_ISR;
	ULONG VARMAC_IMR;
	ULONG VARMAC_PMTCSR;
	struct DWC_ETH_QOS_prv_data *pdata =
	    (struct DWC_ETH_QOS_prv_data *)dev_data;
	struct net_device *dev = pdata->dev;
	struct hw_if_struct *hw_if = &pdata->hw_if;
#ifndef PER_CH_INT
	ULONG VARDMA_SR;
	ULONG VARDMA_IER;
	UINT qinx;
	int napi_sched = 0;
	struct DWC_ETH_QOS_rx_queue *rx_queue = NULL;
#endif
	ULONG VARMAC_ANS = 0;
	ULONG VARMAC_PCS = 0;
	ULONG VARMAC_PHYIS = 0;

	DBGPR("-->DWC_ETH_QOS_ISR_SW_DWC_ETH_QOS\n");

	DMA_ISR_RGRD(VARDMA_ISR);
	if (VARDMA_ISR == 0x0)
		return IRQ_NONE;

	MAC_ISR_RGRD(VARMAC_ISR);

	DBGPR("DMA_ISR = %#lx, MAC_ISR = %#lx\n", VARDMA_ISR, VARMAC_ISR);
#ifdef PER_CH_INT
	if (GET_VALUE(VARDMA_ISR, DMA_ISR_DC0IS_LPOS, DMA_ISR_DC0IS_HPOS) & 1)
		DWC_ETH_QOS_handle_DMA_Int(pdata, DMA_TX_CH0, false);

	if (GET_VALUE(VARDMA_ISR, DMA_ISR_DC1IS_LPOS, DMA_ISR_DC1IS_HPOS) & 1)
		DWC_ETH_QOS_handle_DMA_Int(pdata, DMA_TX_CH1, false);

	if (GET_VALUE(VARDMA_ISR, DMA_ISR_DC2IS_LPOS, DMA_ISR_DC2IS_HPOS) & 1)
		DWC_ETH_QOS_handle_DMA_Int(pdata, DMA_TX_CH2, false);

	if (GET_VALUE(VARDMA_ISR, DMA_ISR_DC3IS_LPOS, DMA_ISR_DC3IS_HPOS) & 1)
		DWC_ETH_QOS_handle_DMA_Int(pdata, DMA_TX_CH3, false);

	if (GET_VALUE(VARDMA_ISR, DMA_ISR_DC4IS_LPOS, DMA_ISR_DC4IS_HPOS) & 1)
		DWC_ETH_QOS_handle_DMA_Int(pdata, DMA_TX_CH4, false);
#else
	/* Handle DMA interrupts */
	for (qinx = 0; qinx < DWC_ETH_QOS_TX_QUEUE_CNT; qinx++) {
		if (qinx < DWC_ETH_QOS_RX_QUEUE_CNT)
			rx_queue = GET_RX_QUEUE_PTR(qinx);

		DMA_SR_RGRD(qinx, VARDMA_SR);
		/* clear interrupts */
		DMA_SR_RGWR(qinx, VARDMA_SR);

		DMA_IER_RGRD(qinx, VARDMA_IER);
		/* handle only those DMA interrupts which are enabled */
		VARDMA_SR = (VARDMA_SR & VARDMA_IER);

		DBGPR("DMA_SR[%d] = %#lx\n", qinx, VARDMA_SR);

		if (pdata->ipa_enabled && qinx == IPA_DMA_RX_CH) {
			pr_debug("ISR is routed to IPA uC for RXCH0. Skip for this channel \n");
			continue;
		}

		if (VARDMA_SR == 0)
			continue;

		if ((qinx < DWC_ETH_QOS_RX_QUEUE_CNT) &&
			((GET_VALUE(VARDMA_SR, DMA_SR_RI_LPOS, DMA_SR_RI_HPOS) & 1) ||
		    (GET_VALUE(VARDMA_SR, DMA_SR_RBU_LPOS, DMA_SR_RBU_HPOS) & 1))) {
			if (!napi_sched) {
				napi_sched = 1;
				if (likely(napi_schedule_prep(&rx_queue->napi))) {
					DWC_ETH_QOS_disable_all_ch_rx_interrpt(pdata);
					__napi_schedule(&rx_queue->napi);
				} else {
					dev_alert(&pdata->pdev->dev, "driver bug! Rx interrupt while in poll\n");
					DWC_ETH_QOS_disable_all_ch_rx_interrpt(pdata);
				}

				if ((GET_VALUE(VARDMA_SR, DMA_SR_RI_LPOS, DMA_SR_RI_HPOS) & 1))
					pdata->xstats.rx_normal_irq_n[qinx]++;
				else
					pdata->xstats.rx_buf_unavailable_irq_n[qinx]++;
			}
		}
		if (GET_VALUE(VARDMA_SR, DMA_SR_TI_LPOS, DMA_SR_TI_HPOS) & 1) {
			if (pdata->ipa_enabled && qinx == IPA_DMA_TX_CH) {
				EMACDBG("TI should not be set for IPA offload Tx channel\n");
			} else {
				pdata->xstats.tx_normal_irq_n[qinx]++;
				DWC_ETH_QOS_tx_interrupt(dev, pdata, qinx);
			}
		}
		if (GET_VALUE(VARDMA_SR, DMA_SR_TPS_LPOS, DMA_SR_TPS_HPOS) & 1) {
			pdata->xstats.tx_process_stopped_irq_n[qinx]++;
			DWC_ETH_QOS_GSTATUS = -E_DMA_SR_TPS;
		}
		if (GET_VALUE(VARDMA_SR, DMA_SR_TBU_LPOS, DMA_SR_TBU_HPOS) & 1) {
			pdata->xstats.tx_buf_unavailable_irq_n[qinx]++;
			DWC_ETH_QOS_GSTATUS = -E_DMA_SR_TBU;
		}
		if (qinx < DWC_ETH_QOS_RX_QUEUE_CNT) {
			if (GET_VALUE(VARDMA_SR, DMA_SR_RPS_LPOS, DMA_SR_RPS_HPOS) & 1) {
				pdata->xstats.rx_process_stopped_irq_n[qinx]++;
				DWC_ETH_QOS_GSTATUS = -E_DMA_SR_RPS;
			}
			if (GET_VALUE(VARDMA_SR, DMA_SR_RWT_LPOS, DMA_SR_RWT_HPOS) & 1) {
				pdata->xstats.rx_watchdog_irq_n++;
				DWC_ETH_QOS_GSTATUS = S_DMA_SR_RWT;
			}
		}
		if (GET_VALUE(VARDMA_SR, DMA_SR_FBE_LPOS, DMA_SR_FBE_HPOS) & 1) {
			pdata->xstats.fatal_bus_error_irq_n++;
			DWC_ETH_QOS_GSTATUS = -E_DMA_SR_FBE;
			DWC_ETH_QOS_restart_dev(pdata, qinx);
		}
	}
#endif
	/* Handle MAC interrupts */
	if (GET_VALUE(VARDMA_ISR, DMA_ISR_MACIS_LPOS, DMA_ISR_MACIS_HPOS) & 1) {
		/* handle only those MAC interrupts which are enabled */
		MAC_IMR_RGRD(VARMAC_IMR);
		VARMAC_ISR = (VARMAC_ISR & VARMAC_IMR);

		/* PMT interrupt */
		if (GET_VALUE(VARMAC_ISR, MAC_ISR_PMTIS_LPOS, MAC_ISR_PMTIS_HPOS) & 1) {
			pdata->xstats.pmt_irq_n++;
			DWC_ETH_QOS_GSTATUS = S_MAC_ISR_PMTIS;
			MAC_PMTCSR_RGRD(VARMAC_PMTCSR);
			if (pdata->power_down)
				DWC_ETH_QOS_powerup(pdata->dev, DWC_ETH_QOS_IOCTL_CONTEXT);
		}

		/* RGMII/SMII interrupt */
		if (GET_VALUE(VARMAC_ISR, MAC_ISR_RGSMIIS_LPOS, MAC_ISR_RGSMIIS_HPOS) & 1) {
			MAC_PCS_RGRD(VARMAC_PCS);
			dev_alert(&pdata->pdev->dev, "RGMII/SMII interrupt: MAC_PCS = %#lx\n", VARMAC_PCS);
			if ((VARMAC_PCS & 0x80000) == 0x80000) {
				pdata->pcs_link = 1;
				netif_carrier_on(dev);
				if ((VARMAC_PCS & 0x10000) == 0x10000) {
					pdata->pcs_duplex = 1;
					hw_if->set_full_duplex(); /* TODO: may not be required */
				} else {
					pdata->pcs_duplex = 0;
					hw_if->set_half_duplex(); /* TODO: may not be required */
				}

				if ((VARMAC_PCS & 0x60000) == 0x0) {
					pdata->pcs_speed = SPEED_10;
					hw_if->set_mii_speed_10(); /* TODO: may not be required */
				} else if ((VARMAC_PCS & 0x60000) == 0x20000) {
					pdata->pcs_speed = SPEED_100;
					hw_if->set_mii_speed_100(); /* TODO: may not be required */
				} else if ((VARMAC_PCS & 0x60000) == 0x30000) {
					pdata->pcs_speed = SPEED_1000;
					hw_if->set_gmii_speed(); /* TODO: may not be required */
				}
				dev_alert(&pdata->pdev->dev, "Link is UP:%dMbps & %s duplex\n",
					  pdata->pcs_speed, pdata->pcs_duplex ? "Full" : "Half");
			} else {
				dev_alert(&pdata->pdev->dev, "Link is Down\n");
				pdata->pcs_link = 0;
				netif_carrier_off(dev);
			}
		}

		/* PCS Link Status interrupt */
		if (GET_VALUE(VARMAC_ISR, MAC_ISR_PCSLCHGIS_LPOS, MAC_ISR_PCSLCHGIS_HPOS) & 1) {
			dev_alert(&pdata->pdev->dev, "PCS Link Status interrupt\n");
			MAC_ANS_RGRD(VARMAC_ANS);
			if (GET_VALUE(VARMAC_ANS, MAC_ANS_LS_LPOS, MAC_ANS_LS_HPOS) & 1) {
				dev_alert(&pdata->pdev->dev, "Link: Up\n");
				netif_carrier_on(dev);
				pdata->pcs_link = 1;
			} else {
				dev_alert(&pdata->pdev->dev, "Link: Down\n");
				netif_carrier_off(dev);
				pdata->pcs_link = 0;
			}
		}

		/* PCS Auto-Negotiation Complete interrupt */
		if (GET_VALUE(VARMAC_ISR, MAC_ISR_PCSANCIS_LPOS, MAC_ISR_PCSANCIS_HPOS) & 1) {
			dev_alert(&pdata->pdev->dev, "PCS Auto-Negotiation Complete interrupt\n");
			MAC_ANS_RGRD(VARMAC_ANS);
		}

		/* EEE interrupts */
		if (GET_VALUE(VARMAC_ISR, MAC_ISR_LPI_LPOS, MAC_ISR_LPI_HPOS) & 1)
			DWC_ETH_QOS_handle_eee_interrupt(pdata);

		/* PHY interrupt */
		if (GET_VALUE(VARMAC_ISR, MAC_ISR_PHYIS_LPOS, MAC_ISR_PHYIS_HPOS) & 1) {
			MAC_ISR_PHYIS_UDFRD(VARMAC_PHYIS);
			DWC_ETH_QOS_handle_phy_interrupt(pdata);
		}
	}

	DBGPR("<--DWC_ETH_QOS_ISR_SW_DWC_ETH_QOS\n");

	return IRQ_HANDLED;
}

/*!
 * \brief API to get all hw features.
 *
 * \details This function is used to check what are all the different
 * features the device supports.
 *
 * \param[in] pdata - pointer to driver private structure
 *
 * \return none
 */

void DWC_ETH_QOS_get_all_hw_features(struct DWC_ETH_QOS_prv_data *pdata)
{
	unsigned int VARMAC_HFR0;
	unsigned int VARMAC_HFR1;
	unsigned int VARMAC_HFR2;

	DBGPR("-->DWC_ETH_QOS_get_all_hw_features\n");

	MAC_HFR0_RGRD(VARMAC_HFR0);
	MAC_HFR1_RGRD(VARMAC_HFR1);
	MAC_HFR2_RGRD(VARMAC_HFR2);

	memset(&pdata->hw_feat, 0, sizeof(pdata->hw_feat));
	pdata->hw_feat.mii_sel = ((VARMAC_HFR0 >> 0) & MAC_HFR0_MIISEL_MASK);
	pdata->hw_feat.gmii_sel = ((VARMAC_HFR0 >> 1) & MAC_HFR0_GMIISEL_MASK);
	pdata->hw_feat.hd_sel = ((VARMAC_HFR0 >> 2) & MAC_HFR0_HDSEL_MASK);
	pdata->hw_feat.pcs_sel = ((VARMAC_HFR0 >> 3) & MAC_HFR0_PCSSEL_MASK);
	pdata->hw_feat.vlan_hash_en =
	    ((VARMAC_HFR0 >> 4) & MAC_HFR0_VLANHASEL_MASK);
	pdata->hw_feat.sma_sel = ((VARMAC_HFR0 >> 5) & MAC_HFR0_SMASEL_MASK);
	pdata->hw_feat.rwk_sel = ((VARMAC_HFR0 >> 6) & MAC_HFR0_RWKSEL_MASK);
	pdata->hw_feat.mgk_sel = ((VARMAC_HFR0 >> 7) & MAC_HFR0_MGKSEL_MASK);
	pdata->hw_feat.mmc_sel = ((VARMAC_HFR0 >> 8) & MAC_HFR0_MMCSEL_MASK);
	pdata->hw_feat.arp_offld_en =
	    ((VARMAC_HFR0 >> 9) & MAC_HFR0_ARPOFFLDEN_MASK);
	pdata->hw_feat.ts_sel =
	    ((VARMAC_HFR0 >> 12) & MAC_HFR0_TSSSEL_MASK);
	pdata->hw_feat.eee_sel = ((VARMAC_HFR0 >> 13) & MAC_HFR0_EEESEL_MASK);
	pdata->hw_feat.tx_coe_sel =
	    ((VARMAC_HFR0 >> 14) & MAC_HFR0_TXCOESEL_MASK);
	pdata->hw_feat.rx_coe_sel =
	    ((VARMAC_HFR0 >> 16) & MAC_HFR0_RXCOE_MASK);
	pdata->hw_feat.mac_addr16_sel =
	    ((VARMAC_HFR0 >> 18) & MAC_HFR0_ADDMACADRSEL_MASK);
	pdata->hw_feat.mac_addr32_sel =
	    ((VARMAC_HFR0 >> 23) & MAC_HFR0_MACADR32SEL_MASK);
	pdata->hw_feat.mac_addr64_sel =
	    ((VARMAC_HFR0 >> 24) & MAC_HFR0_MACADR64SEL_MASK);
	pdata->hw_feat.tsstssel =
	    ((VARMAC_HFR0 >> 25) & MAC_HFR0_TSINTSEL_MASK);
	pdata->hw_feat.sa_vlan_ins =
	    ((VARMAC_HFR0 >> 27) & MAC_HFR0_SAVLANINS_MASK);
	pdata->hw_feat.act_phy_sel =
	    ((VARMAC_HFR0 >> 28) & MAC_HFR0_ACTPHYSEL_MASK);

	pdata->hw_feat.rx_fifo_size =
	    ((VARMAC_HFR1 >> 0) & MAC_HFR1_RXFIFOSIZE_MASK);
	    /* 8; */
	pdata->hw_feat.tx_fifo_size =
	    ((VARMAC_HFR1 >> 6) & MAC_HFR1_TXFIFOSIZE_MASK);
	    /* 8; */
	pdata->hw_feat.adv_ts_hword =
	    ((VARMAC_HFR1 >> 13) & MAC_HFR1_ADVTHWORD_MASK);
	pdata->hw_feat.dcb_en = ((VARMAC_HFR1 >> 16) & MAC_HFR1_DCBEN_MASK);
	pdata->hw_feat.sph_en = ((VARMAC_HFR1 >> 17) & MAC_HFR1_SPHEN_MASK);
	pdata->hw_feat.tso_en = ((VARMAC_HFR1 >> 18) & MAC_HFR1_TSOEN_MASK);
	pdata->hw_feat.dma_debug_gen =
	    ((VARMAC_HFR1 >> 19) & MAC_HFR1_DMADEBUGEN_MASK);
	pdata->hw_feat.av_sel = ((VARMAC_HFR1 >> 20) & MAC_HFR1_AVSEL_MASK);
	pdata->hw_feat.lp_mode_en =
	    ((VARMAC_HFR1 >> 23) & MAC_HFR1_LPMODEEN_MASK);
	pdata->hw_feat.hash_tbl_sz =
	    ((VARMAC_HFR1 >> 24) & MAC_HFR1_HASHTBLSZ_MASK);
	pdata->hw_feat.l3l4_filter_num =
	    ((VARMAC_HFR1 >> 27) & MAC_HFR1_L3L4FILTERNUM_MASK);

	pdata->hw_feat.rx_q_cnt = ((VARMAC_HFR2 >> 0) & MAC_HFR2_RXQCNT_MASK);
	pdata->hw_feat.tx_q_cnt = ((VARMAC_HFR2 >> 6) & MAC_HFR2_TXQCNT_MASK);
	pdata->hw_feat.rx_ch_cnt =
	    ((VARMAC_HFR2 >> 12) & MAC_HFR2_RXCHCNT_MASK);
	pdata->hw_feat.tx_ch_cnt =
	    ((VARMAC_HFR2 >> 18) & MAC_HFR2_TXCHCNT_MASK);
	pdata->hw_feat.pps_out_num =
	    ((VARMAC_HFR2 >> 24) & MAC_HFR2_PPSOUTNUM_MASK);
	pdata->hw_feat.aux_snap_num =
	    ((VARMAC_HFR2 >> 28) & MAC_HFR2_AUXSNAPNUM_MASK);

	DBGPR("<--DWC_ETH_QOS_get_all_hw_features\n");
}

/*!
 * \brief API to print all hw features.
 *
 * \details This function is used to print all the device feature.
 *
 * \param[in] pdata - pointer to driver private structure
 *
 * \return none
 */

void DWC_ETH_QOS_print_all_hw_features(struct DWC_ETH_QOS_prv_data *pdata)
{
	char *str = NULL;

	DBGPR("-->DWC_ETH_QOS_print_all_hw_features\n");

	EMACDBG("\n");
	EMACDBG("=====================================================/\n");
	EMACDBG("\n");
	EMACDBG("10/100 Mbps Support                         : %s\n",
		  pdata->hw_feat.mii_sel ? "YES" : "NO");
	EMACDBG("1000 Mbps Support                           : %s\n",
		  pdata->hw_feat.gmii_sel ? "YES" : "NO");
	EMACDBG("Half-duplex Support                         : %s\n",
		  pdata->hw_feat.hd_sel ? "YES" : "NO");
	EMACDBG("PCS Registers(TBI/SGMII/RTBI PHY interface) : %s\n",
		  pdata->hw_feat.pcs_sel ? "YES" : "NO");
	EMACDBG("VLAN Hash Filter Selected                   : %s\n",
		  pdata->hw_feat.vlan_hash_en ? "YES" : "NO");
	pdata->vlan_hash_filtering = pdata->hw_feat.vlan_hash_en;
	EMACDBG("SMA (MDIO) Interface                        : %s\n",
		  pdata->hw_feat.sma_sel ? "YES" : "NO");
	EMACDBG("PMT Remote Wake-up Packet Enable            : %s\n",
		  pdata->hw_feat.rwk_sel ? "YES" : "NO");
	EMACDBG("PMT Magic Packet Enable                     : %s\n",
		  pdata->hw_feat.mgk_sel ? "YES" : "NO");
	EMACDBG("RMON/MMC Module Enable                      : %s\n",
		  pdata->hw_feat.mmc_sel ? "YES" : "NO");
	EMACDBG("ARP Offload Enabled                         : %s\n",
		  pdata->hw_feat.arp_offld_en ? "YES" : "NO");
	EMACDBG("IEEE 1588-2008 Timestamp Enabled            : %s\n",
		  pdata->hw_feat.ts_sel ? "YES" : "NO");
	EMACDBG("Energy Efficient Ethernet Enabled           : %s\n",
		  pdata->hw_feat.eee_sel ? "YES" : "NO");
	EMACDBG("Transmit Checksum Offload Enabled           : %s\n",
		  pdata->hw_feat.tx_coe_sel ? "YES" : "NO");
	EMACDBG("Receive Checksum Offload Enabled            : %s\n",
		  pdata->hw_feat.rx_coe_sel ? "YES" : "NO");
	EMACDBG("MAC Addresses 16–31 Selected                : %s\n",
		  pdata->hw_feat.mac_addr16_sel ? "YES" : "NO");
	EMACDBG("MAC Addresses 32–63 Selected                : %s\n",
		  pdata->hw_feat.mac_addr32_sel ? "YES" : "NO");
	EMACDBG("MAC Addresses 64–127 Selected               : %s\n",
		  pdata->hw_feat.mac_addr64_sel ? "YES" : "NO");
	EMACDBG("IPA Feature Enabled                          : %s\n",
		pdata->ipa_enabled ? "YES" : "NO");

	if (pdata->hw_feat.mac_addr64_sel)
		pdata->max_addr_reg_cnt = 128;
	else if (pdata->hw_feat.mac_addr32_sel)
		pdata->max_addr_reg_cnt = 64;
	else if (pdata->hw_feat.mac_addr16_sel)
		pdata->max_addr_reg_cnt = 32;
	else
		pdata->max_addr_reg_cnt = 1;

	switch (pdata->hw_feat.tsstssel) {
	case 0:
		str = "RESERVED";
		break;
	case 1:
		str = "INTERNAL";
		break;
	case 2:
		str = "EXTERNAL";
		break;
	case 3:
		str = "BOTH";
		break;
	}
	EMACDBG("Timestamp System Time Source                : %s\n",
		  str);
	EMACDBG("Source Address or VLAN Insertion Enable     : %s\n",
		  pdata->hw_feat.sa_vlan_ins ? "YES" : "NO");

	switch (pdata->hw_feat.act_phy_sel) {
	case 0:
		str = "GMII/MII";
		break;
	case 1:
		str = "RGMII";
		break;
	case 2:
		str = "SGMII";
		break;
	case 3:
		str = "TBI";
		break;
	case 4:
		str = "RMII";
		break;
	case 5:
		str = "RTBI";
		break;
	case 6:
		str = "SMII";
		break;
	case 7:
		str = "RevMII";
		break;
	default:
		str = "RESERVED";
	}
	EMACDBG("Active PHY Selected                         : %s\n",
		  str);

	switch (pdata->hw_feat.rx_fifo_size) {
	case 0:
		str = "128 bytes";
		break;
	case 1:
		str = "256 bytes";
		break;
	case 2:
		str = "512 bytes";
		break;
	case 3:
		str = "1 KBytes";
		break;
	case 4:
		str = "2 KBytes";
		break;
	case 5:
		str = "4 KBytes";
		break;
	case 6:
		str = "8 KBytes";
		break;
	case 7:
		str = "16 KBytes";
		break;
	case 8:
		str = "32 kBytes";
		break;
	case 9:
		str = "64 KBytes";
		break;
	case 10:
		str = "128 KBytes";
		break;
	case 11:
		str = "256 KBytes";
		break;
	default:
		str = "RESERVED";
	}
	EMACDBG("MTL Receive FIFO Size                       : %s\n",
		  str);

	switch (pdata->hw_feat.tx_fifo_size) {
	case 0:
		str = "128 bytes";
		break;
	case 1:
		str = "256 bytes";
		break;
	case 2:
		str = "512 bytes";
		break;
	case 3:
		str = "1 KBytes";
		break;
	case 4:
		str = "2 KBytes";
		break;
	case 5:
		str = "4 KBytes";
		break;
	case 6:
		str = "8 KBytes";
		break;
	case 7:
		str = "16 KBytes";
		break;
	case 8:
		str = "32 kBytes";
		break;
	case 9:
		str = "64 KBytes";
		break;
	case 10:
		str = "128 KBytes";
		break;
	case 11:
		str = "256 KBytes";
		break;
	default:
		str = "RESERVED";
	}
	EMACDBG("MTL Transmit FIFO Size                       : %s\n",
		  str);
	EMACDBG("IEEE 1588 High Word Register Enable          : %s\n",
		  pdata->hw_feat.adv_ts_hword ? "YES" : "NO");
	EMACDBG("DCB Feature Enable                           : %s\n",
		  pdata->hw_feat.dcb_en ? "YES" : "NO");
	EMACDBG("Split Header Feature Enable                  : %s\n",
		  pdata->hw_feat.sph_en ? "YES" : "NO");
	EMACDBG("TCP Segmentation Offload Enable              : %s\n",
		  pdata->hw_feat.tso_en ? "YES" : "NO");
	EMACDBG("DMA Debug Registers Enabled                  : %s\n",
		  pdata->hw_feat.dma_debug_gen ? "YES" : "NO");
	EMACDBG("AV Feature Enabled                           : %s\n",
		  pdata->hw_feat.av_sel ? "YES" : "NO");
	EMACDBG("Low Power Mode Enabled                       : %s\n",
		  pdata->hw_feat.lp_mode_en ? "YES" : "NO");

	switch (pdata->hw_feat.hash_tbl_sz) {
	case 0:
		str = "No hash table selected";
		pdata->max_hash_table_size = 0;
		break;
	case 1:
		str = "64";
		pdata->max_hash_table_size = 64;
		break;
	case 2:
		str = "128";
		pdata->max_hash_table_size = 128;
		break;
	case 3:
		str = "256";
		pdata->max_hash_table_size = 256;
		break;
	}
	EMACDBG("Hash Table Size                              : %s\n",
		  str);
	EMACDBG("Total number of L3 or L4 Filters             : %d L3/L4 Filter\n",
		  pdata->hw_feat.l3l4_filter_num);
	EMACDBG("Number of MTL Receive Queues                 : %d\n",
		  (pdata->hw_feat.rx_q_cnt + 1));
	EMACDBG("Number of MTL Transmit Queues                : %d\n",
		  (pdata->hw_feat.tx_q_cnt + 1));
	EMACDBG("Number of DMA Receive Channels               : %d\n",
		  (pdata->hw_feat.rx_ch_cnt + 1));
	EMACDBG("Number of DMA Transmit Channels              : %d\n",
		  (pdata->hw_feat.tx_ch_cnt + 1));

	switch (pdata->hw_feat.pps_out_num) {
	case 0:
		str = "No PPS output";
		break;
	case 1:
		str = "1 PPS output";
		break;
	case 2:
		str = "2 PPS output";
		break;
	case 3:
		str = "3 PPS output";
		break;
	case 4:
		str = "4 PPS output";
		break;
	default:
		str = "RESERVED";
	}
	EMACDBG("Number of PPS Outputs                        : %s\n",
		  str);

	switch (pdata->hw_feat.aux_snap_num) {
	case 0:
		str = "No auxiliary input";
		break;
	case 1:
		str = "1 auxiliary input";
		break;
	case 2:
		str = "2 auxiliary input";
		break;
	case 3:
		str = "3 auxiliary input";
		break;
	case 4:
		str = "4 auxiliary input";
		break;
	default:
		str = "RESERVED";
	}
	EMACDBG("Number of Auxiliary Snapshot Inputs          : %s",
		  str);

	EMACDBG("\n");
	EMACDBG("=====================================================/\n");

	DBGPR("<--DWC_ETH_QOS_print_all_hw_features\n");
}

static const struct net_device_ops DWC_ETH_QOS_netdev_ops = {
	.ndo_open = DWC_ETH_QOS_open,
	.ndo_stop = DWC_ETH_QOS_close,
	.ndo_start_xmit = DWC_ETH_QOS_start_xmit,
	.ndo_get_stats = DWC_ETH_QOS_get_stats,
	.ndo_set_rx_mode = DWC_ETH_QOS_set_rx_mode,

#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller = DWC_ETH_QOS_poll_controller,
#endif				/*end of CONFIG_NET_POLL_CONTROLLER */
	.ndo_set_features = DWC_ETH_QOS_set_features,
	.ndo_fix_features = DWC_ETH_QOS_fix_features,
	.ndo_do_ioctl = DWC_ETH_QOS_ioctl,
	.ndo_change_mtu = DWC_ETH_QOS_change_mtu,
#ifdef DWC_ETH_QOS_QUEUE_SELECT_ALGO
	.ndo_select_queue = DWC_ETH_QOS_select_queue,
#endif
	.ndo_vlan_rx_add_vid = DWC_ETH_QOS_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid = DWC_ETH_QOS_vlan_rx_kill_vid,
	.ndo_set_mac_address    = eth_mac_addr,
};

struct net_device_ops *DWC_ETH_QOS_get_netdev_ops(void)
{
	return (struct net_device_ops *)&DWC_ETH_QOS_netdev_ops;
}


/*!
 * \brief allcation of Rx skb's for split header feature.
 *
 * \details This function is invoked by other api's for
 * allocating the Rx skb's if split header feature is enabled.
 *
 * \param[in] pdata – pointer to private data structure.
 * \param[in] buffer – pointer to wrapper receive buffer data structure.
 * \param[in] gfp – the type of memory allocation.
 *
 * \return int
 *
 * \retval 0 on success and -ve number on failure.
 */

static int DWC_ETH_QOS_alloc_split_hdr_rx_buf(
		struct DWC_ETH_QOS_prv_data *pdata,
		struct DWC_ETH_QOS_rx_buffer *buffer,
		UINT qinx, gfp_t gfp)
{
	struct sk_buff *skb = buffer->skb;

	DBGPR("-->DWC_ETH_QOS_alloc_split_hdr_rx_buf\n");

	if (skb) {
		skb_trim(skb, 0);
		goto check_page;
	}

	buffer->rx_hdr_size = DWC_ETH_QOS_MAX_HDR_SIZE;
	/* allocate twice the maximum header size */
	skb = __netdev_alloc_skb_ip_align(pdata->dev,
					  (2 * buffer->rx_hdr_size),
			gfp);
	if (!skb) {
		dev_alert(&pdata->pdev->dev, "Failed to allocate skb\n");
		return -ENOMEM;
	}
	buffer->skb = skb;
	DBGPR("Maximum header buffer size allocated = %d\n",
	      buffer->rx_hdr_size);
 check_page:
	if (!buffer->dma)
		buffer->dma = dma_map_single(GET_MEM_PDEV_DEV,
					buffer->skb->data,
					(2 * buffer->rx_hdr_size),
					DMA_FROM_DEVICE);
	buffer->len = buffer->rx_hdr_size;

	/* allocate a new page if necessary */
	if (!buffer->page2) {
		buffer->page2 = alloc_page(gfp);
		if (unlikely(!buffer->page2)) {
			dev_alert(&pdata->pdev->dev,
				  "Failed to allocate page for second buffer\n");
			return -ENOMEM;
		}
	}
	if (!buffer->dma2)
		buffer->dma2 = dma_map_page(GET_MEM_PDEV_DEV,
				    buffer->page2, 0,
				    PAGE_SIZE, DMA_FROM_DEVICE);
	buffer->len2 = PAGE_SIZE;
	buffer->mapped_as_page = Y_TRUE;

	DBGPR("<--DWC_ETH_QOS_alloc_split_hdr_rx_buf\n");

	return 0;
}

/*!
 * \brief allcation of Rx skb's for jumbo frame.
 *
 * \details This function is invoked by other api's for
 * allocating the Rx skb's if jumbo frame is enabled.
 *
 * \param[in] pdata – pointer to private data structure.
 * \param[in] buffer – pointer to wrapper receive buffer data structure.
 * \param[in] gfp – the type of memory allocation.
 *
 * \return int
 *
 * \retval 0 on success and -ve number on failure.
 */

static int DWC_ETH_QOS_alloc_jumbo_rx_buf(struct DWC_ETH_QOS_prv_data *pdata,
					  struct DWC_ETH_QOS_rx_buffer *buffer,
					  UINT qinx, gfp_t gfp)
{
	struct sk_buff *skb = buffer->skb;
	unsigned int bufsz = (256 - 16);	/* for skb_reserve */

	DBGPR("-->DWC_ETH_QOS_alloc_jumbo_rx_buf\n");

	if (skb) {
		skb_trim(skb, 0);
		goto check_page;
	}

	skb = __netdev_alloc_skb_ip_align(pdata->dev, bufsz, gfp);
	if (!skb) {
		dev_alert(&pdata->pdev->dev, "Failed to allocate skb\n");
		return -ENOMEM;
	}
	buffer->skb = skb;
 check_page:
	/* allocate a new page if necessary */
	if (!buffer->page) {
		buffer->page = alloc_page(gfp);
		if (unlikely(!buffer->page)) {
			dev_alert(&pdata->pdev->dev, "Failed to allocate page\n");
			return -ENOMEM;
		}
	}
	if (!buffer->dma)
		buffer->dma = dma_map_page(GET_MEM_PDEV_DEV,
					   buffer->page, 0,
					   PAGE_SIZE, DMA_FROM_DEVICE);
	buffer->len = PAGE_SIZE;

	if (!buffer->page2) {
		buffer->page2 = alloc_page(gfp);
		if (unlikely(!buffer->page2)) {
			dev_alert(&pdata->pdev->dev,
				  "Failed to allocate page for second buffer\n");
			return -ENOMEM;
		}
	}
	if (!buffer->dma2)
		buffer->dma2 = dma_map_page(GET_MEM_PDEV_DEV,
					    buffer->page2, 0,
					    PAGE_SIZE, DMA_FROM_DEVICE);
	buffer->len2 = PAGE_SIZE;

	buffer->mapped_as_page = Y_TRUE;

	DBGPR("<--DWC_ETH_QOS_alloc_jumbo_rx_buf\n");

	return 0;
}

/*!
 * \brief allcation of Rx skb's for default rx mode.
 *
 * \details This function is invoked by other api's for
 * allocating the Rx skb's with default Rx mode ie non-jumbo
 * and non-split header mode.
 *
 * \param[in] pdata – pointer to private data structure.
 * \param[in] buffer – pointer to wrapper receive buffer data structure.
 * \param[in] gfp – the type of memory allocation.
 *
 * \return int
 *
 * \retval 0 on success and -ve number on failure.
 */

static int DWC_ETH_QOS_alloc_rx_buf(struct DWC_ETH_QOS_prv_data *pdata,
				    struct DWC_ETH_QOS_rx_buffer *buffer, UINT qinx,
				    gfp_t gfp)
{
	struct sk_buff *skb = buffer->skb;
	unsigned int rx_buffer_len = pdata->rx_buffer_len;
	dma_addr_t ipa_rx_buf_dma_addr;
	struct sg_table *buff_sgt;
	int ret = 0;

	DBGPR("-->DWC_ETH_QOS_alloc_rx_buf\n");

	if (pdata->ipa_enabled && qinx == IPA_DMA_RX_CH) {
		rx_buffer_len = DWC_ETH_QOS_ETH_FRAME_LEN_IPA;
		buffer->ipa_buff_va = dma_alloc_coherent(
		   GET_MEM_PDEV_DEV, rx_buffer_len,
		   &ipa_rx_buf_dma_addr, GFP_KERNEL);

		if (!buffer->ipa_buff_va) {
			dev_alert(&pdata->pdev->dev, "Failed to allocate RX dma buf for IPA\n");
			return -ENOMEM;
		}

		buffer->len = rx_buffer_len;
		buffer->dma = ipa_rx_buf_dma_addr;

		buff_sgt = kzalloc(sizeof (*buff_sgt), GFP_KERNEL);
		if (buff_sgt) {
			ret = dma_get_sgtable(GET_MEM_PDEV_DEV, buff_sgt,
						buffer->ipa_buff_va, ipa_rx_buf_dma_addr,
						rx_buffer_len);
			if (ret == Y_SUCCESS) {
				buffer->ipa_rx_buff_phy_addr = sg_phys(buff_sgt->sgl);
				sg_free_table(buff_sgt);
			} else {
				EMACERR("Failed to get sgtable for allocated RX buffer.\n");
			}
			kfree(buff_sgt);
			buff_sgt = NULL;
		} else {
			EMACERR("Failed to allocate memory for RX buff sgtable.\n");
		}
		return 0;
	} else {

		if (skb) {
			skb_trim(skb, 0);
			goto map_skb;
		}

		skb = __netdev_alloc_skb_ip_align(pdata->dev, rx_buffer_len, gfp);
		if (!skb) {
			dev_alert(&pdata->pdev->dev, "Failed to allocate skb\n");
			return -ENOMEM;
		}
		buffer->skb = skb;
		buffer->len = rx_buffer_len;

 map_skb:
		buffer->dma = dma_map_single(GET_MEM_PDEV_DEV, skb->data,
							rx_buffer_len, DMA_FROM_DEVICE);
		if (dma_mapping_error(&pdata->pdev->dev, buffer->dma))
			dev_alert(&pdata->pdev->dev, "failed to do the RX dma map\n");

		buffer->mapped_as_page = Y_FALSE;
	}

	DBGPR("<--DWC_ETH_QOS_alloc_rx_buf\n");

	return 0;
}

/*!
 * \brief api to configure Rx function pointer after reset.
 *
 * \details This function will initialize the receive function pointers
 * which are used for allocating skb's and receiving the packets based
 * Rx mode - default/jumbo/split header.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */

static void DWC_ETH_QOS_configure_rx_fun_ptr(struct DWC_ETH_QOS_prv_data *pdata)
{
	DBGPR("-->DWC_ETH_QOS_configure_rx_fun_ptr\n");

	if (pdata->rx_split_hdr) {
		pdata->clean_rx = DWC_ETH_QOS_clean_split_hdr_rx_irq;
		pdata->alloc_rx_buf = DWC_ETH_QOS_alloc_split_hdr_rx_buf;
	} else if (pdata->dev->mtu > DWC_ETH_QOS_ETH_FRAME_LEN) {
		pdata->clean_rx = DWC_ETH_QOS_clean_jumbo_rx_irq;
		pdata->alloc_rx_buf = DWC_ETH_QOS_alloc_jumbo_rx_buf;
	} else {
		pdata->rx_buffer_len = DWC_ETH_QOS_ETH_FRAME_LEN;
		pdata->clean_rx = DWC_ETH_QOS_clean_rx_irq;
		pdata->alloc_rx_buf = DWC_ETH_QOS_alloc_rx_buf;
	}

	DBGPR("<--DWC_ETH_QOS_configure_rx_fun_ptr\n");
}

/*!
 * \brief api to initialize default values.
 *
 * \details This function is used to initialize differnet parameters to
 * default values which are common parameters between Tx and Rx path.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */

static void DWC_ETH_QOS_default_common_confs(struct DWC_ETH_QOS_prv_data *pdata)
{
	DBGPR("-->DWC_ETH_QOS_default_common_confs\n");

	pdata->drop_tx_pktburstcnt = 1;
	pdata->mac_enable_count = 0;
	pdata->incr_incrx = DWC_ETH_QOS_INCR_ENABLE;
	pdata->flow_ctrl = DWC_ETH_QOS_FLOW_CTRL_TX_RX;
	pdata->oldflow_ctrl = DWC_ETH_QOS_FLOW_CTRL_TX_RX;
	pdata->power_down = 0;
	pdata->tx_sa_ctrl_via_desc = DWC_ETH_QOS_SA0_NONE;
	pdata->tx_sa_ctrl_via_reg = DWC_ETH_QOS_SA0_NONE;
	pdata->hwts_tx_en = 0;
	pdata->hwts_rx_en = 0;
	pdata->l3_l4_filter = 0;
	pdata->l2_filtering_mode = !!pdata->hw_feat.hash_tbl_sz;
	pdata->tx_path_in_lpi_mode = 0;
	pdata->use_lpi_tx_automate = true;
	pdata->use_lpi_auto_entry_timer = true;

	pdata->one_nsec_accuracy = 1;

	DBGPR("<--DWC_ETH_QOS_default_common_confs\n");
}

/*!
 * \brief api to initialize Tx parameters.
 *
 * \details This function is used to initialize all Tx
 * parameters to default values on reset.
 *
 * \param[in] pdata – pointer to private data structure.
 * \param[in] qinx – DMA channel/queue number to be initialized.
 *
 * \return void
 */

static void DWC_ETH_QOS_default_tx_confs_single_q(
		struct DWC_ETH_QOS_prv_data *pdata,
		UINT qinx)
{
	struct DWC_ETH_QOS_tx_queue *queue_data = GET_TX_QUEUE_PTR(qinx);
	struct DWC_ETH_QOS_tx_wrapper_descriptor *desc_data =
		GET_TX_WRAPPER_DESC(qinx);

	DBGPR("-->DWC_ETH_QOS_default_tx_confs_single_q\n");

	queue_data->q_op_mode = q_op_mode[qinx];

	desc_data->tx_threshold_val = DWC_ETH_QOS_TX_THRESHOLD_32;
	desc_data->tsf_on = DWC_ETH_QOS_TSF_ENABLE;
	desc_data->osf_on = DWC_ETH_QOS_OSF_ENABLE;
	desc_data->tx_pbl = DWC_ETH_QOS_PBL_16;
	desc_data->tx_vlan_tag_via_reg = Y_FALSE;
	desc_data->tx_vlan_tag_ctrl = DWC_ETH_QOS_TX_VLAN_TAG_INSERT;
	desc_data->vlan_tag_present = 0;
	desc_data->context_setup = 0;
	desc_data->default_mss = 0;

	DBGPR("<--DWC_ETH_QOS_default_tx_confs_single_q\n");
}

/*!
 * \brief api to initialize Rx parameters.
 *
 * \details This function is used to initialize all Rx
 * parameters to default values on reset.
 *
 * \param[in] pdata – pointer to private data structure.
 * \param[in] qinx – DMA queue/channel number to be initialized.
 *
 * \return void
 */

static void DWC_ETH_QOS_default_rx_confs_single_q(
		struct DWC_ETH_QOS_prv_data *pdata,
		UINT qinx)
{
	struct DWC_ETH_QOS_rx_wrapper_descriptor *desc_data =
		GET_RX_WRAPPER_DESC(qinx);

	DBGPR("-->DWC_ETH_QOS_default_rx_confs_single_q\n");

	desc_data->rx_threshold_val = DWC_ETH_QOS_RX_THRESHOLD_64;
	desc_data->rsf_on = DWC_ETH_QOS_RSF_DISABLE;
	desc_data->rx_pbl = DWC_ETH_QOS_PBL_16;
	desc_data->rx_outer_vlan_strip = DWC_ETH_QOS_RX_VLAN_STRIP_ALWAYS;
	desc_data->rx_inner_vlan_strip = DWC_ETH_QOS_RX_VLAN_STRIP_ALWAYS;

	DBGPR("<--DWC_ETH_QOS_default_rx_confs_single_q\n");
}

static void DWC_ETH_QOS_default_tx_confs(struct DWC_ETH_QOS_prv_data *pdata)
{
	UINT qinx;

	DBGPR("-->DWC_ETH_QOS_default_tx_confs\n");

	for (qinx = 0; qinx < DWC_ETH_QOS_TX_QUEUE_CNT; qinx++)
		DWC_ETH_QOS_default_tx_confs_single_q(pdata, qinx);

	DBGPR("<--DWC_ETH_QOS_default_tx_confs\n");
}

static void DWC_ETH_QOS_default_rx_confs(struct DWC_ETH_QOS_prv_data *pdata)
{
	UINT qinx;

	DBGPR("-->DWC_ETH_QOS_default_rx_confs\n");

	for (qinx = 0; qinx < DWC_ETH_QOS_RX_QUEUE_CNT; qinx++)
		DWC_ETH_QOS_default_rx_confs_single_q(pdata, qinx);

	DBGPR("<--DWC_ETH_QOS_default_rx_confs\n");
}

/*!
 * \brief API to open a device for data transmission & reception.
 *
 * \details Opens the interface. The interface is opned whenever
 * ifconfig activates it. The open method should register any
 * system resource it needs like I/O ports, IRQ, DMA, etc,
 * turn on the hardware, and perform any other setup your device requires.
 *
 * \param[in] dev - pointer to net_device structure
 *
 * \return integer
 *
 * \retval 0 on success & negative number on failure.
 */
static int DWC_ETH_QOS_open(struct net_device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	int ret = Y_SUCCESS;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct desc_if_struct *desc_if = &pdata->desc_if;

	EMACDBG("-->DWC_ETH_QOS_open\n");

#ifdef DWC_ETH_QOS_CONFIG_PGTEST
	if (pdata->irq_number == 0) {
		ret = request_irq(dev->irq, DWC_ETH_QOS_ISR_SW_DWC_ETH_QOS_pg,
			  IRQF_SHARED, DEV_NAME, pdata);
	}
#else
	if (pdata->irq_number == 0) {
		ret = request_irq(dev->irq, DWC_ETH_QOS_ISR_SW_DWC_ETH_QOS,
				IRQF_SHARED, DEV_NAME, pdata);
	}
#endif /* end of DWC_ETH_QOS_CONFIG_PGTEST */
	if (ret != 0) {
		dev_alert(&pdata->pdev->dev, "Unable to register IRQ %d\n",
			  pdata->irq_number);
		ret = -EBUSY;
		goto err_irq_0;
	}
	pdata->irq_number = dev->irq;
#ifdef PER_CH_INT
	ret = DWC_ETH_QOS_register_per_ch_intr(pdata, 0x1);
	if (ret != 0) {
		ret = -EBUSY;
		goto err_out_desc_buf_alloc_failed;
	}
#endif

	ret = desc_if->alloc_buff_and_desc(pdata);
	if (ret < 0) {
		dev_alert(&pdata->pdev->dev,
			  "failed to allocate buffer/descriptor memory\n");
		ret = -ENOMEM;
		goto err_out_desc_buf_alloc_failed;
	}

	/* default configuration */
#ifdef DWC_ETH_QOS_CONFIG_PGTEST
	DWC_ETH_QOS_default_confs(pdata);
#else
	DWC_ETH_QOS_default_common_confs(pdata);
	DWC_ETH_QOS_default_tx_confs(pdata);
	DWC_ETH_QOS_default_rx_confs(pdata);
	DWC_ETH_QOS_configure_rx_fun_ptr(pdata);

	DWC_ETH_QOS_napi_enable_mq(pdata);
#endif /* end of DWC_ETH_QOS_CONFIG_PGTEST */

	DWC_ETH_QOS_set_rx_mode(dev);
	desc_if->wrapper_tx_desc_init(pdata);
	desc_if->wrapper_rx_desc_init(pdata);

	DWC_ETH_QOS_tx_desc_mang_ds_dump(pdata);
	DWC_ETH_QOS_rx_desc_mang_ds_dump(pdata);

	DWC_ETH_QOS_mmc_setup(pdata);

	/* initializes MAC and DMA */
	hw_if->init(pdata);

	if (pdata->hw_feat.pcs_sel)
		hw_if->control_an(1, 0);

#ifdef DWC_ETH_QOS_CONFIG_PGTEST
	hw_if->prepare_dev_pktgen(pdata);
#endif

	if (pdata->phydev)
		phy_start(pdata->phydev);

	pdata->eee_enabled = DWC_ETH_QOS_eee_init(pdata);

#ifndef DWC_ETH_QOS_CONFIG_PGTEST
	netif_tx_start_all_queues(dev);

	if (pdata->ipa_enabled) {
		DWC_ETH_QOS_ipa_offload_event_handler(pdata, EV_DEV_OPEN);
	}
#else
	netif_tx_disable(dev);
#endif /* end of DWC_ETH_QOS_CONFIG_PGTEST */

	EMACDBG("<--DWC_ETH_QOS_open\n");

	return ret;

 err_out_desc_buf_alloc_failed:
#ifdef PER_CH_INT
	DWC_ETH_QOS_deregister_per_ch_intr(pdata);
#endif
	free_irq(pdata->irq_number, pdata);

 err_irq_0:
	pdata->irq_number = 0;
 DBGPR("<--DWC_ETH_QOS_open\n");
	return ret;
}

/*!
 * \brief API to close a device.
 *
 * \details Stops the interface. The interface is stopped when it is brought
 * down. This function should reverse operations performed at open time.
 *
 * \param[in] dev - pointer to net_device structure
 *
 * \return integer
 *
 * \retval 0 on success & negative number on failure.
 */

static int DWC_ETH_QOS_close(struct net_device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct desc_if_struct *desc_if = &pdata->desc_if;
	int qinx = 0;

	DBGPR("-->DWC_ETH_QOS_close\n");

	if (pdata->eee_enabled) {
		del_timer_sync(&pdata->eee_ctrl_timer);
		pdata->eee_active = 0;
	}

	if (pdata->phydev)
		phy_stop(pdata->phydev);

#ifndef DWC_ETH_QOS_CONFIG_PGTEST
	/* Stop SW TX before DMA TX in HW */
	netif_tx_disable(dev);
	DWC_ETH_QOS_stop_all_ch_tx_dma(pdata);

	if (pdata->ipa_enabled)
		DWC_ETH_QOS_ipa_offload_event_handler(pdata, EV_DEV_CLOSE);

	/* Disable MAC TX/RX */
	hw_if->stop_mac_tx_rx();

	/* Stop SW RX after DMA RX in HW */
	DWC_ETH_QOS_stop_all_ch_rx_dma(pdata);
	DWC_ETH_QOS_all_ch_napi_disable(pdata);

#endif /* end of DWC_ETH_QOS_CONFIG_PGTEST */

#ifdef DWC_ETH_QOS_TXPOLLING_MODE_ENABLE
    for (qinx = 0; qinx < DWC_ETH_QOS_TX_QUEUE_CNT; qinx++) {
		/* check for tx descriptor status */
		DWC_ETH_QOS_tx_interrupt(pdata->dev, pdata, qinx);
    }
#endif

	for (qinx = 0; qinx < DWC_ETH_QOS_RX_QUEUE_CNT; qinx++) {
		if (pdata->ipa_enabled && (qinx == IPA_DMA_RX_CH))
			continue;

		(void)pdata->clean_rx(pdata, NAPI_PER_QUEUE_POLL_BUDGET, qinx);
	}

	/* issue software reset to device */
	hw_if->exit();

    DWC_ETH_QOS_restart_phy(pdata);

	desc_if->tx_free_mem(pdata);
	desc_if->rx_free_mem(pdata);
#ifdef PER_CH_INT
	DWC_ETH_QOS_deregister_per_ch_intr(pdata);
#endif
	if (pdata->irq_number != 0) {
		free_irq(pdata->irq_number, pdata);
		pdata->irq_number = 0;
	}
#ifdef DWC_ETH_QOS_CONFIG_PGTEST
	del_timer(&pdata->pg_timer);
#endif /* end of DWC_ETH_QOS_CONFIG_PGTEST */

	DBGPR("<--DWC_ETH_QOS_close\n");

	return Y_SUCCESS;
}

/*!
 * \brief API to configure the multicast address in device.
 *
 * \details This function collects all the multicast addresse
 * and updates the device.
 *
 * \param[in] dev - pointer to net_device structure.
 *
 * \retval 0 if perfect filtering is seleted & 1 if hash
 * filtering is seleted.
 */
static int DWC_ETH_QOS_prepare_mc_list(struct net_device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	u32 mc_filter[DWC_ETH_QOS_HTR_CNT];
	struct netdev_hw_addr *ha = NULL;
	int crc32_val = 0;
	int ret = 0, i = 1;

	DBGPR_FILTER("-->DWC_ETH_QOS_prepare_mc_list\n");

	if (pdata->l2_filtering_mode) {
		DBGPR_FILTER("select HASH FILTERING for mc addresses: mc_count = %d\n",
			     netdev_mc_count(dev));
		ret = 1;
		memset(mc_filter, 0, sizeof(mc_filter));

		if (pdata->max_hash_table_size == 64) {
			netdev_for_each_mc_addr(ha, dev) {
				DBGPR_FILTER("mc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n", i++,
					     ha->addr[0], ha->addr[1], ha->addr[2],
						ha->addr[3], ha->addr[4], ha->addr[5]);
				/* The upper 6 bits of the calculated CRC are used to
				 * index the content of the Hash Table Reg 0 and 1.
				 */
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 26);
				/* The most significant bit determines the register
				 * to use (Hash Table Reg X, X = 0 and 1) while the
				 * other 5(0x1F) bits determines the bit within the
				 * selected register
				 */
				mc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
			}
		} else if (pdata->max_hash_table_size == 128) {
			netdev_for_each_mc_addr(ha, dev) {
				DBGPR_FILTER("mc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n", i++,
					     ha->addr[0], ha->addr[1], ha->addr[2],
						ha->addr[3], ha->addr[4], ha->addr[5]);
				/* The upper 7 bits of the calculated CRC are used to
				 * index the content of the Hash Table Reg 0,1,2 and 3.
				 */
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 25);

				dev_alert(&pdata->pdev->dev, "crc_le = %#x, crc_be = %#x\n",
					  bitrev32(~crc32_le(~0, ha->addr, 6)),
						bitrev32(~crc32_be(~0, ha->addr, 6)));

				/* The most significant 2 bits determines the register
				 * to use (Hash Table Reg X, X = 0,1,2 and 3) while the
				 * other 5(0x1F) bits determines the bit within the
				 * selected register
				 */
				mc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
			}
		} else if (pdata->max_hash_table_size == 256) {
			netdev_for_each_mc_addr(ha, dev) {
				DBGPR_FILTER("mc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n", i++,
					     ha->addr[0], ha->addr[1], ha->addr[2],
						ha->addr[3], ha->addr[4], ha->addr[5]);
				/* The upper 8 bits of the calculated CRC are used to
				 * index the content of the Hash Table Reg 0,1,2,3,4,
				 * 5,6, and 7.
				 */
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 24);
				/* The most significant 3 bits determines the register
				 * to use (Hash Table Reg X, X = 0,1,2,3,4,5,6 and 7) while
				 * the other 5(0x1F) bits determines the bit within the
				 * selected register
				 */
				mc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
			}
		}

		for (i = 0; i < DWC_ETH_QOS_HTR_CNT; i++)
			hw_if->update_hash_table_reg(i, mc_filter[i]);

	} else {
		DBGPR_FILTER("select PERFECT FILTERING for mc addresses, mc_count = %d, max_addr_reg_cnt = %d\n",
			     netdev_mc_count(dev), pdata->max_addr_reg_cnt);

		netdev_for_each_mc_addr(ha, dev) {
			DBGPR_FILTER("mc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n", i,
				     ha->addr[0], ha->addr[1], ha->addr[2],
					ha->addr[3], ha->addr[4], ha->addr[5]);
			if (i < 32)
				hw_if->update_mac_addr1_31_low_high_reg(i, ha->addr);
			else
				hw_if->update_mac_addr32_127_low_high_reg(i, ha->addr);
			i++;
		}
	}

	DBGPR_FILTER("<--DWC_ETH_QOS_prepare_mc_list\n");

	return ret;
}

/*!
 * \brief API to configure the unicast address in device.
 *
 * \details This function collects all the unicast addresses
 * and updates the device.
 *
 * \param[in] dev - pointer to net_device structure.
 *
 * \retval 0 if perfect filtering is seleted  & 1 if hash
 * filtering is seleted.
 */
static int DWC_ETH_QOS_prepare_uc_list(struct net_device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	u32 uc_filter[DWC_ETH_QOS_HTR_CNT];
	struct netdev_hw_addr *ha = NULL;
	int crc32_val = 0;
	int ret = 0, i = 1;

	DBGPR_FILTER("-->DWC_ETH_QOS_prepare_uc_list\n");

	if (pdata->l2_filtering_mode) {
		DBGPR_FILTER("select HASH FILTERING for uc addresses: uc_count = %d\n",
			     netdev_uc_count(dev));
		ret = 1;
		memset(uc_filter, 0, sizeof(uc_filter));

		if (pdata->max_hash_table_size == 64) {
			netdev_for_each_uc_addr(ha, dev) {
				DBGPR_FILTER("uc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n", i++,
					     ha->addr[0], ha->addr[1], ha->addr[2],
						ha->addr[3], ha->addr[4], ha->addr[5]);
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 26);
				uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
			}
		} else if (pdata->max_hash_table_size == 128) {
			netdev_for_each_uc_addr(ha, dev) {
				DBGPR_FILTER("uc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n", i++,
					     ha->addr[0], ha->addr[1], ha->addr[2],
						ha->addr[3], ha->addr[4], ha->addr[5]);
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 25);
				uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
			}
		} else if (pdata->max_hash_table_size == 256) {
			netdev_for_each_uc_addr(ha, dev) {
				DBGPR_FILTER("uc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n", i++,
					     ha->addr[0], ha->addr[1], ha->addr[2],
						ha->addr[3], ha->addr[4], ha->addr[5]);
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 24);
				uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
			}
		}

		/* configure hash value of real/default interface also */
		DBGPR_FILTER("real/default dev_addr = %#x:%#x:%#x:%#x:%#x:%#x\n",
			     dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
				dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);

		if (pdata->max_hash_table_size == 64) {
			crc32_val =
				(bitrev32(~crc32_le(~0, dev->dev_addr, 6)) >> 26);
			uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
		} else if (pdata->max_hash_table_size == 128) {
			crc32_val =
				(bitrev32(~crc32_le(~0, dev->dev_addr, 6)) >> 25);
			uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));

		} else if (pdata->max_hash_table_size == 256) {
			crc32_val =
				(bitrev32(~crc32_le(~0, dev->dev_addr, 6)) >> 24);
			uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
		}

		for (i = 0; i < DWC_ETH_QOS_HTR_CNT; i++)
			hw_if->update_hash_table_reg(i, uc_filter[i]);

	} else {
		DBGPR_FILTER("select PERFECT FILTERING for uc addresses: uc_count = %d\n",
			     netdev_uc_count(dev));

		netdev_for_each_uc_addr(ha, dev) {
			DBGPR_FILTER("uc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n", i,
				     ha->addr[0], ha->addr[1], ha->addr[2],
					ha->addr[3], ha->addr[4], ha->addr[5]);
			if (i < 32)
				hw_if->update_mac_addr1_31_low_high_reg(i, ha->addr);
			else
				hw_if->update_mac_addr32_127_low_high_reg(i, ha->addr);
			i++;
		}
	}

	DBGPR_FILTER("<--DWC_ETH_QOS_prepare_uc_list\n");

	return ret;
}

/*!
 * \brief API to set the device receive mode
 *
 * \details The set_multicast_list function is called when the multicast list
 * for the device changes and when the flags change.
 *
 * \param[in] dev - pointer to net_device structure.
 *
 * \return void
 */
static void DWC_ETH_QOS_set_rx_mode(struct net_device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned long flags;
	unsigned char pr_mode = 0;
	unsigned char huc_mode = 0;
	unsigned char hmc_mode = 0;
	unsigned char pm_mode = 0;
	unsigned char hpf_mode = 0;
	int mode, i;

	DBGPR_FILTER("-->DWC_ETH_QOS_set_rx_mode\n");

	spin_lock_irqsave(&pdata->lock, flags);

#ifdef DWC_ETH_QOS_CONFIG_PGTEST
	DBGPR("PG Test running, no parameters will be changed\n");
	spin_unlock_irqrestore(&pdata->lock, flags);
	return;
#endif

	if (dev->flags & IFF_PROMISC) {
		DBGPR_FILTER("PROMISCUOUS MODE (Accept all packets irrespective of DA)\n");
		pr_mode = 1;
	} else if ((dev->flags & IFF_ALLMULTI) ||
			(netdev_mc_count(dev) > (pdata->max_hash_table_size))) {
		DBGPR_FILTER("pass all multicast pkt\n");
		pm_mode = 1;
		if (pdata->max_hash_table_size) {
			for (i = 0; i < DWC_ETH_QOS_HTR_CNT; i++)
				hw_if->update_hash_table_reg(i, 0xffffffff);
		}
	} else if (!netdev_mc_empty(dev)) {
		DBGPR_FILTER("pass list of multicast pkt\n");
		if ((netdev_mc_count(dev) > (pdata->max_addr_reg_cnt - 1)) &&
		    (!pdata->max_hash_table_size)) {
			/* switch to PROMISCUOUS mode */
			pr_mode = 1;
		} else {
			mode = DWC_ETH_QOS_prepare_mc_list(dev);
			if (mode) {
				/* Hash filtering for multicast */
				hmc_mode = 1;
			} else {
				/* Perfect filtering for multicast */
				hmc_mode = 0;
				hpf_mode = 1;
			}
		}
	}

	/* Handle multiple unicast addresses */
	if ((netdev_uc_count(dev) > (pdata->max_addr_reg_cnt - 1)) &&
	    (!pdata->max_hash_table_size)) {
		/* switch to PROMISCUOUS mode */
		pr_mode = 1;
	} else if (!netdev_uc_empty(dev)) {
		mode = DWC_ETH_QOS_prepare_uc_list(dev);
		if (mode) {
			/* Hash filtering for unicast */
			huc_mode = 1;
		} else {
			/* Perfect filtering for unicast */
			huc_mode = 0;
			hpf_mode = 1;
		}
	}

	hw_if->config_mac_pkt_filter_reg(pr_mode, huc_mode,
		hmc_mode, pm_mode, hpf_mode);

	spin_unlock_irqrestore(&pdata->lock, flags);

	DBGPR("<--DWC_ETH_QOS_set_rx_mode\n");
}

/*!
 * \brief API to calculate number of descriptor.
 *
 * \details This function is invoked by start_xmit function. This function
 * calculates number of transmit descriptor required for a given transfer.
 *
 * \param[in] pdata - pointer to private data structure
 * \param[in] skb - pointer to sk_buff structure
 * \param[in] qinx - Queue number.
 *
 * \return integer
 *
 * \retval number of descriptor required.
 */

UINT DWC_ETH_QOS_get_total_desc_cnt(struct DWC_ETH_QOS_prv_data *pdata,
				    struct sk_buff *skb, UINT qinx)
{
	UINT count = 0, size = 0;
	INT length = 0, i = 0;
#ifdef DWC_ETH_QOS_ENABLE_VLAN_TAG
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct s_tx_pkt_features *tx_pkt_features = GET_TX_PKT_FEATURES_PTR;
	struct DWC_ETH_QOS_tx_wrapper_descriptor *desc_data =
	    GET_TX_WRAPPER_DESC(qinx);
#endif

	/* SG fragment count */
	DBGPR("No of frags : %d \n",skb_shinfo(skb)->nr_frags);
	for ( i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		struct skb_frag_struct *frag = &skb_shinfo(skb)->frags[i];
		length = frag->size;
		while (length) {
			size =
				min_t(unsigned int, length, DWC_ETH_QOS_MAX_DATA_PER_TXD);
			length -= size;
			count ++;
		}
	}

	length = 0;
	/* descriptors required based on data limit per descriptor */
	length = (skb->len - skb->data_len);
	while (length) {
		size = min(length, (INT)DWC_ETH_QOS_MAX_DATA_PER_TXD);
		count++;
		length = length - size;
	}

	/* we need one context descriptor to carry tso details */
	if (skb_shinfo(skb)->gso_size != 0)
		count++;

#ifdef DWC_ETH_QOS_ENABLE_VLAN_TAG
	desc_data->vlan_tag_present = 0;
	if (skb_vlan_tag_present(skb)) {
		USHORT vlan_tag = skb_vlan_tag_get(skb);

		vlan_tag |= (qinx << 13);
		desc_data->vlan_tag_present = 1;
		if (vlan_tag != desc_data->vlan_tag_id ||
		    desc_data->context_setup == 1) {
			desc_data->vlan_tag_id = vlan_tag;
			if (desc_data->tx_vlan_tag_via_reg == Y_TRUE) {
				dev_alert(&pdata->pdev->dev, "VLAN control info update via register\n\n");
				hw_if->enable_vlan_reg_control(desc_data);
			} else {
				hw_if->enable_vlan_desc_control(pdata);
				TX_PKT_FEATURES_PKT_ATTRIBUTES_VLAN_PKT_MLF_WR
				    (tx_pkt_features->pkt_attributes, 1);
				TX_PKT_FEATURES_VLAN_TAG_VT_MLF_WR
					(tx_pkt_features->vlan_tag, vlan_tag);
				/* we need one context descriptor to carry vlan tag info */
				count++;
			}
		}
		pdata->xstats.tx_vlan_pkt_n++;
	}
#endif
#ifdef DWC_ETH_QOS_ENABLE_DVLAN
	if (pdata->via_reg_or_desc == DWC_ETH_QOS_VIA_DESC) {
		/* we need one context descriptor to carry vlan tag info */
		count++;
	}
#endif /* End of DWC_ETH_QOS_ENABLE_DVLAN */

	return count;
}

inline UINT DWC_ETH_QOS_cal_int_mod(struct sk_buff *skb, UINT eth_type,
	struct DWC_ETH_QOS_prv_data *pdata)
{
	UINT ret = DEFAULT_INT_MOD;
	bool is_udp;

#ifdef DWC_ETH_QOS_CONFIG_PTP
	if (eth_type == ETH_P_1588)
		ret = PTP_INT_MOD;
	else
#endif
	if (eth_type == ETH_P_TSN) {
		ret = AVB_INT_MOD;
	} else if (eth_type == ETH_P_IP || eth_type == ETH_P_IPV6) {
#ifdef DWC_ETH_QOS_CONFIG_PTP
		is_udp = (eth_type == ETH_P_IP && ip_hdr(skb)->protocol == IPPROTO_UDP)
						|| (eth_type == ETH_P_IPV6 && ipv6_hdr(skb)->nexthdr == IPPROTO_UDP);

		if (is_udp && (udp_hdr(skb)->dest == htons(PTP_UDP_EV_PORT)
			|| udp_hdr(skb)->dest == htons(PTP_UDP_GEN_PORT))) {
			ret = PTP_INT_MOD;
		} else
#endif
		ret = IP_PKT_INT_MOD;
	}

	return ret;
}

/*!
 * \brief API to transmit the packets
 *
 * \details The start_xmit function initiates the transmission of a packet.
 * The full packet (protocol headers and all) is contained in a socket buffer
 * (sk_buff) structure.
 *
 * \param[in] skb - pointer to sk_buff structure
 * \param[in] dev - pointer to net_device structure
 *
 * \return integer
 *
 * \retval 0
 */

static int DWC_ETH_QOS_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	UINT qinx = skb_get_queue_mapping(skb);
	struct DWC_ETH_QOS_tx_wrapper_descriptor *desc_data =
	    GET_TX_WRAPPER_DESC(qinx);
	struct s_tx_pkt_features *tx_pkt_features = GET_TX_PKT_FEATURES_PTR;
	unsigned long flags;
	unsigned int desc_count = 0;
	unsigned int count = 0;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct desc_if_struct *desc_if = &pdata->desc_if;
	INT retval = NETDEV_TX_OK;
#ifdef DWC_ETH_QOS_ENABLE_VLAN_TAG
	UINT varvlan_pkt;
#endif
	int tso;
	struct netdev_queue *devq = netdev_get_tx_queue(dev, qinx);
	UINT int_mod = 1;
	UINT eth_type = 0;


	DBGPR("-->DWC_ETH_QOS_start_xmit: skb->len = %d, qinx = %u\n",
	      skb->len, qinx);

	if (pdata->ipa_enabled && qinx == IPA_DMA_TX_CH) {
		EMACERR("TX Channel [%d] is not a valid for SW path \n", qinx);
		BUG();
	}

	spin_lock_irqsave(&pdata->tx_lock, flags);

#ifdef DWC_ETH_QOS_CONFIG_PGTEST
	retval = NETDEV_TX_BUSY;
	goto tx_netdev_return;
#endif

	if (skb->len <= 0) {
		dev_kfree_skb_any(skb);
		pr_alert("%s : Empty skb received from stack\n",
			 dev->name);
		goto tx_netdev_return;
	}

	if ((skb_shinfo(skb)->gso_size == 0) &&
	    (skb->len > DWC_ETH_QOS_MAX_SUPPORTED_MTU)) {
		pr_alert("%s : big packet = %d\n", dev->name,
			 (u16)skb->len);
		dev_kfree_skb_any(skb);
		dev->stats.tx_dropped++;
		goto tx_netdev_return;
	}

	if ((pdata->eee_enabled) && (pdata->tx_path_in_lpi_mode) &&
	    (!pdata->use_lpi_tx_automate) && (!pdata->use_lpi_auto_entry_timer))
		DWC_ETH_QOS_disable_eee_mode(pdata);

	memset(&pdata->tx_pkt_features, 0, sizeof(pdata->tx_pkt_features));

	/* check total number of desc required for current xfer */
	desc_count = DWC_ETH_QOS_get_total_desc_cnt(pdata, skb, qinx);
	if (desc_data->free_desc_cnt < desc_count) {
		desc_data->queue_stopped = 1;
		netif_stop_subqueue(dev, qinx);
		DBGPR("stopped TX queue(%d) since there are no sufficient descriptor available for the current transfer\n",
		      qinx);
		retval = NETDEV_TX_BUSY;
		goto tx_netdev_return;
	}

	/* check for hw tstamping */
	if (pdata->hw_feat.tsstssel && pdata->hwts_tx_en) {
		if (skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) {
			/* declare that device is doing timestamping */
			skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
			TX_PKT_FEATURES_PKT_ATTRIBUTES_PTP_ENABLE_MLF_WR(tx_pkt_features->pkt_attributes, 1);
			DBGPR_PTP("Got PTP pkt to transmit [qinx = %d, cur_tx = %d]\n",
				  qinx, desc_data->cur_tx);
		}
	}

	tso = desc_if->handle_tso(dev, skb);
	if (tso < 0) {
		dev_alert(&pdata->pdev->dev, "Unable to handle TSO\n");
		dev_kfree_skb_any(skb);
		retval = NETDEV_TX_OK;
		goto tx_netdev_return;
	}
	if (tso) {
		pdata->xstats.tx_tso_pkt_n++;
		TX_PKT_FEATURES_PKT_ATTRIBUTES_TSO_ENABLE_MLF_WR(tx_pkt_features->pkt_attributes, 1);
	} else if (skb->ip_summed == CHECKSUM_PARTIAL) {
		TX_PKT_FEATURES_PKT_ATTRIBUTES_CSUM_ENABLE_MLF_WR(tx_pkt_features->pkt_attributes, 1);
	}

	count = desc_if->map_tx_skb(dev, skb);
	if (count == 0) {
		dev_kfree_skb_any(skb);
		retval = NETDEV_TX_OK;
		goto tx_netdev_return;
	}

	desc_data->packet_count = count;

	if (tso && (desc_data->default_mss != tx_pkt_features->mss))
		count++;

	devq->trans_start = jiffies;

#ifdef DWC_ETH_QOS_ENABLE_VLAN_TAG
	TX_PKT_FEATURES_PKT_ATTRIBUTES_VLAN_PKT_MLF_RD
		(tx_pkt_features->pkt_attributes, varvlan_pkt);
	if (varvlan_pkt == 0x1)
		count++;
#endif
#ifdef DWC_ETH_QOS_ENABLE_DVLAN
	if (pdata->via_reg_or_desc == DWC_ETH_QOS_VIA_DESC)
		count++;
#endif /* End of DWC_ETH_QOS_ENABLE_DVLAN */

	/*Check if count is greater than free_desc_count to avoid integer overflow
	  count cannot be greater than free_desc-count as it is already checked in
	  the starting of this function*/
	if (count > desc_data->free_desc_cnt) {
		EMACERR("count : %u cannot be greater than free_desc_count: %u",count,
				desc_data->free_desc_cnt);
		BUG();
	}
	desc_data->free_desc_cnt -= count;
	desc_data->tx_pkt_queued += count;

#ifdef DWC_ETH_QOS_ENABLE_TX_PKT_DUMP
	print_pkt(skb, skb->len, 1, (desc_data->cur_tx - 1));
#endif

	/* fallback to software time stamping if core doesn't
	 * support hardware time stamping
	 */
	if ((pdata->hw_feat.tsstssel == 0) || (pdata->hwts_tx_en == 0))
		skb_tx_timestamp(skb);

	eth_type = GET_ETH_TYPE(skb->data);
	/*For TSO packets, IOC bit is to be set to 1 in order to avoid data stall*/
	if (!tso)
		int_mod = DWC_ETH_QOS_cal_int_mod(skb, eth_type, pdata);

	if (eth_type == ETH_P_IP || eth_type == ETH_P_IPV6)
		skb_orphan(skb);
	/* configure required descriptor fields for transmission */
	hw_if->pre_xmit(pdata, qinx, int_mod);

tx_netdev_return:
	spin_unlock_irqrestore(&pdata->tx_lock, flags);

	DBGPR("<--DWC_ETH_QOS_start_xmit\n");

	return retval;
}

static void DWC_ETH_QOS_print_rx_tstamp_info(struct s_RX_NORMAL_DESC *rxdesc,
					     unsigned int qinx)
{
	u32 ptp_status = 0;
	u32 pkt_type = 0;
	char *tstamp_dropped = NULL;
	char *tstamp_available = NULL;
	char *ptp_version = NULL;
	char *ptp_pkt_type = NULL;
	char *ptp_msg_type = NULL;

	DBGPR_PTP("-->DWC_ETH_QOS_print_rx_tstamp_info\n");

	/* status in RDES1 is not valid */
	if (!(rxdesc->RDES3 & DWC_ETH_QOS_RDESC3_RS1V))
		return;

	ptp_status = rxdesc->RDES1;
	tstamp_dropped = ((ptp_status & 0x8000) ? "YES" : "NO");
	tstamp_available = ((ptp_status & 0x4000) ? "YES" : "NO");
	ptp_version = ((ptp_status & 0x2000) ? "v2 (1588-2008)" : "v1 (1588-2002)");
	ptp_pkt_type = ((ptp_status & 0x1000) ? "ptp over Eth" : "ptp over IPv4/6");

	pkt_type = ((ptp_status & 0xF00) >> 8);
	switch (pkt_type) {
	case 0:
		ptp_msg_type = "NO PTP msg received";
		break;
	case 1:
		ptp_msg_type = "SYNC";
		break;
	case 2:
		ptp_msg_type = "Follow_Up";
		break;
	case 3:
		ptp_msg_type = "Delay_Req";
		break;
	case 4:
		ptp_msg_type = "Delay_Resp";
		break;
	case 5:
		ptp_msg_type = "Pdelay_Req";
		break;
	case 6:
		ptp_msg_type = "Pdelay_Resp";
		break;
	case 7:
		ptp_msg_type = "Pdelay_Resp_Follow_up";
		break;
	case 8:
		ptp_msg_type = "Announce";
		break;
	case 9:
		ptp_msg_type = "Management";
		break;
	case 10:
		ptp_msg_type = "Signaling";
		break;
	case 11:
	case 12:
	case 13:
	case 14:
		ptp_msg_type = "Reserved";
		break;
	case 15:
		ptp_msg_type = "PTP pkr with Reserved Msg Type";
		break;
	}

	DBGPR_PTP("Rx timestamp detail for queue %d\n"
			"tstamp dropped    = %s\n"
			"tstamp available  = %s\n"
			"PTP version       = %s\n"
			"PTP Pkt Type      = %s\n"
			"PTP Msg Type      = %s\n",
			qinx, tstamp_dropped, tstamp_available,
			ptp_version, ptp_pkt_type, ptp_msg_type);

	DBGPR_PTP("<--DWC_ETH_QOS_print_rx_tstamp_info\n");
}

/*!
 * \brief API to get rx time stamp value.
 *
 * \details This function will read received packet's timestamp from
 * the descriptor and pass it to stack and also perform some sanity checks.
 *
 * \param[in] pdata - pointer to private data structure.
 * \param[in] skb - pointer to sk_buff structure.
 * \param[in] desc_data - pointer to wrapper receive descriptor structure.
 * \param[in] qinx - Queue/Channel number.
 *
 * \return integer
 *
 * \retval 0 if no context descriptor
 * \retval 1 if timestamp is valid
 * \retval 2 if time stamp is corrupted
 */

static unsigned char DWC_ETH_QOS_get_rx_hwtstamp(
	struct DWC_ETH_QOS_prv_data *pdata,
	struct sk_buff *skb,
	struct DWC_ETH_QOS_rx_wrapper_descriptor *desc_data,
	unsigned int qinx)
{
	struct s_RX_NORMAL_DESC *rx_normal_desc =
		GET_RX_DESC_PTR(qinx, desc_data->cur_rx);
	struct s_RX_CONTEXT_DESC *rx_context_desc = NULL;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct skb_shared_hwtstamps *shhwtstamp = NULL;
	u64 ns;
	int retry, ret;

	DBGPR_PTP("-->DWC_ETH_QOS_get_rx_hwtstamp\n");

	DWC_ETH_QOS_print_rx_tstamp_info(rx_normal_desc, qinx);

	desc_data->dirty_rx++;
	INCR_RX_DESC_INDEX(desc_data->cur_rx, 1, pdata->rx_queue[qinx].desc_cnt);
	rx_context_desc = (void *)GET_RX_DESC_PTR(qinx, desc_data->cur_rx);

	DBGPR_PTP("\nRX_CONTEX_DESC[%d %4p %d RECEIVED FROM DEVICE] = %#x:%#x:%#x:%#x",
		  qinx, rx_context_desc, desc_data->cur_rx, rx_context_desc->RDES0,
			rx_context_desc->RDES1,
			rx_context_desc->RDES2, rx_context_desc->RDES3);

	/* check rx tsatmp */
	for (retry = 0; retry < 10; retry++) {
		ret = hw_if->get_rx_tstamp_status(rx_context_desc);
		if (ret == 1) {
			/* time stamp is valid */
			break;
		} else if (ret == 0) {
			dev_alert(&pdata->pdev->dev, "Device has not yet updated the context desc to hold Rx time stamp(retry = %d)\n",
				  retry);
		} else {
			dev_alert(&pdata->pdev->dev, "Error: Rx time stamp is corrupted(retry = %d)\n", retry);
			return 2;
		}
	}

	if (retry == 10) {
		dev_alert(&pdata->pdev->dev,
			  "Device has not yet updated the context desc to hold Rx time stamp(retry = %d)\n",
			retry);
		desc_data->dirty_rx--;
		DECR_RX_DESC_INDEX(desc_data->cur_rx, pdata->rx_queue[qinx].desc_cnt);
		return 0;
	}

	pdata->xstats.rx_timestamp_captured_n++;
	/* get valid tstamp */
	ns = hw_if->get_rx_tstamp(rx_context_desc);

	shhwtstamp = skb_hwtstamps(skb);
	memset(shhwtstamp, 0, sizeof(struct skb_shared_hwtstamps));
	shhwtstamp->hwtstamp = ns_to_ktime(ns);

	DBGPR_PTP("<--DWC_ETH_QOS_get_rx_hwtstamp\n");

	return 1;
}

/*!
 * \brief API to get tx time stamp value.
 *
 * \details This function will read timestamp from the descriptor
 * and pass it to stack and also perform some sanity checks.
 *
 * \param[in] pdata - pointer to private data structure.
 * \param[in] txdesc - pointer to transmit descriptor structure.
 * \param[in] skb - pointer to sk_buff structure.
 *
 * \return integer
 *
 * \retval 1 if time stamp is taken
 * \retval 0 if time stamp in not taken/valid
 */

static unsigned int DWC_ETH_QOS_get_tx_hwtstamp(
	struct DWC_ETH_QOS_prv_data *pdata,
	struct s_TX_NORMAL_DESC *txdesc,
	struct sk_buff *skb)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct skb_shared_hwtstamps shhwtstamp;
	u64 ns;

	DBGPR_PTP("-->DWC_ETH_QOS_get_tx_hwtstamp\n");

	if (hw_if->drop_tx_status_enabled() == 0) {
		DBGPR_PTP("drop_tx_status_enabled 0\n");
		/* check tx tstamp status */
		if (!hw_if->get_tx_tstamp_status(txdesc)) {
			dev_alert(&pdata->pdev->dev, "tx timestamp is not captured for this packet\n");
			return 0;
		}

		/* get the valid tstamp */
		ns = hw_if->get_tx_tstamp(txdesc);
	} else {
		DBGPR_PTP("drop_tx_status_enabled 1 - read from reg\n");
		/* drop tx status mode is enabled, hence read time
		 * stamp from register instead of descriptor
		 */

		/* check tx tstamp status */
		if (!hw_if->get_tx_tstamp_status_via_reg()) {
			dev_alert(&pdata->pdev->dev, "tx timestamp is not captured for this packet\n");
			return 0;
		}

		/* get the valid tstamp */
		ns = hw_if->get_tx_tstamp_via_reg();
	}

	pdata->xstats.tx_timestamp_captured_n++;
	memset(&shhwtstamp, 0, sizeof(struct skb_shared_hwtstamps));
	shhwtstamp.hwtstamp = ns_to_ktime(ns);
	/* pass tstamp to stack */
	skb_tstamp_tx(skb, &shhwtstamp);

	DBGPR_PTP("<--DWC_ETH_QOS_get_tx_hwtstamp\n");

	return 1;
}

/*!
 * \brief API to update the tx status.
 *
 * \details This function is called in isr handler once after getting
 * transmit complete interrupt to update the transmited packet status
 * and it does some house keeping work like updating the
 * private data structure variables.
 *
 * \param[in] dev - pointer to net_device structure
 * \param[in] pdata - pointer to private data structure.
 *
 * \return void
 */

static void DWC_ETH_QOS_tx_interrupt(struct net_device *dev,
				     struct DWC_ETH_QOS_prv_data *pdata,
				     UINT qinx)
{
	struct DWC_ETH_QOS_tx_wrapper_descriptor *desc_data =
	    GET_TX_WRAPPER_DESC(qinx);
	struct s_TX_NORMAL_DESC *txptr = NULL;
	struct DWC_ETH_QOS_tx_buffer *buffer = NULL;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct desc_if_struct *desc_if = &pdata->desc_if;
#ifndef DWC_ETH_QOS_CERTIFICATION_PKTBURSTCNT
	int err_incremented;
#endif
	unsigned int tstamp_taken = 0;
	unsigned long flags;

	DBGPR("-->DWC_ETH_QOS_tx_interrupt: desc_data->tx_pkt_queued = %d dirty_tx = %d, qinx = %u\n",
	      desc_data->tx_pkt_queued, desc_data->dirty_tx, qinx);

	if (pdata->ipa_enabled && qinx == IPA_DMA_TX_CH) {
		EMACDBG("TX status interrupts are handled by IPA uc for TXCH \
					skip for the host \n");
		return;
	}

	spin_lock_irqsave(&pdata->tx_lock, flags);

	pdata->xstats.tx_clean_n[qinx]++;
	while (desc_data->tx_pkt_queued > 0) {
		txptr = GET_TX_DESC_PTR(qinx, desc_data->dirty_tx);
		buffer = GET_TX_BUF_PTR(qinx, desc_data->dirty_tx);
		tstamp_taken = 0;

		if (!hw_if->tx_complete(txptr))
			break;

#ifdef DWC_ETH_QOS_ENABLE_TX_DESC_DUMP
		dump_tx_desc(pdata, desc_data->dirty_tx, desc_data->dirty_tx,
			     0, qinx);
#endif

#ifndef DWC_ETH_QOS_CERTIFICATION_PKTBURSTCNT
		/* update the tx error if any by looking at last segment
		 * for NORMAL descriptors
		 */
		if ((hw_if->get_tx_desc_ls(txptr)) && !(hw_if->get_tx_desc_ctxt(txptr))) {
			if (buffer->skb) {
				/* check whether skb support hw tstamp */
				if ((pdata->hw_feat.tsstssel) &&
				    (skb_shinfo(buffer->skb)->tx_flags & SKBTX_IN_PROGRESS)) {
					tstamp_taken = DWC_ETH_QOS_get_tx_hwtstamp(pdata,
										   txptr, buffer->skb);
					if (tstamp_taken) {
						/* dump_tx_desc(pdata, desc_data->dirty_tx, desc_data->dirty_tx, */
						/* 0, qinx); */
						DBGPR_PTP("passed tx timestamp to stack[qinx = %d, dirty_tx = %d]\n",
							  qinx, desc_data->dirty_tx);
					}
				}
			}

			err_incremented = 0;
			if (hw_if->tx_window_error) {
				if (hw_if->tx_window_error(txptr)) {
					err_incremented = 1;
					dev->stats.tx_window_errors++;
				}
			}
			if (hw_if->tx_aborted_error) {
				if (hw_if->tx_aborted_error(txptr)) {
					err_incremented = 1;
					dev->stats.tx_aborted_errors++;
					if (hw_if->tx_handle_aborted_error)
						hw_if->tx_handle_aborted_error(txptr);
				}
			}
			if (hw_if->tx_carrier_lost_error) {
				if (hw_if->tx_carrier_lost_error(txptr)) {
					err_incremented = 1;
					dev->stats.tx_carrier_errors++;
				}
			}
			if (hw_if->tx_fifo_underrun) {
				if (hw_if->tx_fifo_underrun(txptr)) {
					err_incremented = 1;
					dev->stats.tx_fifo_errors++;
					if (hw_if->tx_update_fifo_threshold)
						hw_if->tx_update_fifo_threshold(txptr);
				}
			}
			if (hw_if->tx_get_collision_count)
				dev->stats.collisions +=
				    hw_if->tx_get_collision_count(txptr);

			if (err_incremented == 1)
				dev->stats.tx_errors++;

			pdata->xstats.q_tx_pkt_n[qinx]++;
			pdata->xstats.tx_pkt_n++;
			dev->stats.tx_packets++;
#ifdef DWC_ETH_QOS_BUILTIN
			if (dev->stats.tx_packets == 1)
				EMACINFO("Transmitted First Rx packet\n");
#endif
#ifdef CONFIG_MSM_BOOT_TIME_MARKER
	if ( dev->stats.tx_packets == 1) {
		place_marker("M - Ethernet first packet transmitted");
	}
#endif
		}
#else
		if ((hw_if->get_tx_desc_ls(txptr)) && !(hw_if->get_tx_desc_ctxt(txptr))) {
			if (buffer->skb) {
				/* check whether skb support hw tstamp */
				if ((pdata->hw_feat.tsstssel) &&
				    (skb_shinfo(buffer->skb)->tx_flags & SKBTX_IN_PROGRESS)) {
					tstamp_taken = DWC_ETH_QOS_get_tx_hwtstamp(pdata,
										   txptr, buffer->skb);
					if (tstamp_taken) {
						dump_tx_desc(pdata, desc_data->dirty_tx, desc_data->dirty_tx,
							     0, qinx);
						DBGPR_PTP("passed tx timestamp to stack[qinx = %d, dirty_tx = %d]\n",
							  qinx, desc_data->dirty_tx);
					}
				}
			}
		}
#endif
		dev->stats.tx_bytes += buffer->len;
		dev->stats.tx_bytes += buffer->len2;
		desc_if->unmap_tx_skb(pdata, buffer);

		/* reset the descriptor so that driver/host can reuse it */
		hw_if->tx_desc_reset(desc_data->dirty_tx, pdata, qinx);

		INCR_TX_DESC_INDEX(desc_data->dirty_tx, 1, pdata->tx_queue[qinx].desc_cnt);
		desc_data->free_desc_cnt++;
		desc_data->tx_pkt_queued--;
	}

	if ((desc_data->queue_stopped == 1) && (desc_data->free_desc_cnt > 0)) {
		desc_data->queue_stopped = 0;
		netif_wake_subqueue(dev, qinx);
	}
#ifdef DWC_ETH_QOS_CERTIFICATION_PKTBURSTCNT
	/* DMA has finished Transmitting data to MAC Tx-Fifo */
	MAC_MCR_TE_UDFWR(1);
#endif

	if ((pdata->eee_enabled) && (!pdata->tx_path_in_lpi_mode) &&
	    (!pdata->use_lpi_tx_automate) && (!pdata->use_lpi_auto_entry_timer)) {
		DWC_ETH_QOS_enable_eee_mode(pdata);
		mod_timer(&pdata->eee_ctrl_timer,
			  DWC_ETH_QOS_LPI_TIMER(DWC_ETH_QOS_DEFAULT_LPI_TIMER));
	}

	spin_unlock_irqrestore(&pdata->tx_lock, flags);

	DBGPR("<--DWC_ETH_QOS_tx_interrupt: desc_data->tx_pkt_queued = %d\n",
	      desc_data->tx_pkt_queued);
}

#ifdef YDEBUG_FILTER
static void DWC_ETH_QOS_check_rx_filter_status(struct s_RX_NORMAL_DESC *RX_NORMAL_DESC)
{
	u32 rdes2 = RX_NORMAL_DESC->RDES2;
	u32 rdes3 = RX_NORMAL_DESC->RDES3;

	/* Receive Status RDES2 Valid ? */
	if ((rdes3 & 0x8000000) == 0x8000000) {
		if ((rdes2 & 0x400) == 0x400)
			EMACDBG("ARP pkt received\n");
		if ((rdes2 & 0x800) == 0x800)
			EMACDBG("ARP reply not generated\n");
		if ((rdes2 & 0x8000) == 0x8000)
			EMACDBG("VLAN pkt passed VLAN filter\n");
		if ((rdes2 & 0x10000) == 0x10000)
			EMACDBG("SA Address filter fail\n");
		if ((rdes2 & 0x20000) == 0x20000)
			EMACDBG("DA Addess filter fail\n");
		if ((rdes2 & 0x40000) == 0x40000)
			EMACDBG("pkt passed the HASH filter in MAC and HASH value = %#x\n",
				  (rdes2 >> 19) & 0xff);
		if ((rdes2 & 0x8000000) == 0x8000000)
			EMACDBG("L3 filter(%d) Match\n", ((rdes2 >> 29) & 0x7));
		if ((rdes2 & 0x10000000) == 0x10000000)
			EMACDBG("L4 filter(%d) Match\n", ((rdes2 >> 29) & 0x7));
	}
}
#endif /* YDEBUG_FILTER */

/* pass skb to upper layer */
static void DWC_ETH_QOS_receive_skb(struct DWC_ETH_QOS_prv_data *pdata,
				    struct net_device *dev, struct sk_buff *skb,
				    UINT qinx)
{
#ifdef DWC_ETH_QOS_BUILTIN
	static int cnt_ipv4 = 0, cnt_ipv6 = 0;
#endif

	struct DWC_ETH_QOS_rx_queue *rx_queue = GET_RX_QUEUE_PTR(qinx);

	skb_record_rx_queue(skb, qinx);
	skb->dev = dev;
	skb->protocol = eth_type_trans(skb, dev);

#ifdef DWC_ETH_QOS_BUILTIN
	if (skb->protocol == htons(ETH_P_IPV6) && (cnt_ipv6++ == 1)) {
		EMACINFO("Received first ipv6 packet\n");
	}
	if (skb->protocol == htons(ETH_P_IP) && (cnt_ipv4++ == 1))
		EMACINFO("Received first ipv4 packet\n");
#endif
	if (dev->features & NETIF_F_GRO) {
		napi_gro_receive(&rx_queue->napi, skb);
	}
#ifdef DWC_INET_LRO
	else if ((dev->features & NETIF_F_LRO) &&
		(skb->ip_summed == CHECKSUM_UNNECESSARY)) {
		lro_receive_skb(&rx_queue->lro_mgr, skb, (void *)pdata);
		rx_queue->lro_flush_needed = 1;
	}
#endif
	else {
		netif_receive_skb(skb);
	}
}

static void DWC_ETH_QOS_consume_page(struct DWC_ETH_QOS_rx_buffer *buffer,
				     struct sk_buff *skb,
				     u16 length, u16 buf2_used)
{
	buffer->page = NULL;
	if (buf2_used)
		buffer->page2 = NULL;
	skb->len += length;
	skb->data_len += length;
	skb->truesize += length;
}

static void DWC_ETH_QOS_consume_page_split_hdr(
				struct DWC_ETH_QOS_rx_buffer *buffer,
				struct sk_buff *skb,
				u16 length,
				USHORT page2_used)
{
	if (page2_used)
		buffer->page2 = NULL;
		if (skb != NULL) {
			skb->len += length;
			skb->data_len += length;
			skb->truesize += length;
		}
}

/* Receive Checksum Offload configuration */
static inline void DWC_ETH_QOS_config_rx_csum(struct DWC_ETH_QOS_prv_data *pdata,
					      struct sk_buff *skb,
		struct s_RX_NORMAL_DESC *rx_normal_desc)
{
	UINT VARRDES1;

	skb->ip_summed = CHECKSUM_NONE;

	if ((pdata->dev_state & NETIF_F_RXCSUM) == NETIF_F_RXCSUM) {
		/* Receive Status RDES1 Valid ? */
		if ((rx_normal_desc->RDES3 & DWC_ETH_QOS_RDESC3_RS1V)) {
			/* check(RDES1.IPCE bit) whether device has done csum correctly or not */
			RX_NORMAL_DESC_RDES1_ML_RD(rx_normal_desc->RDES1, VARRDES1);
			if ((VARRDES1 & 0xC8) == 0x0)
				skb->ip_summed = CHECKSUM_UNNECESSARY;	/* csum done by device */
		}
	}
}

static inline void DWC_ETH_QOS_get_rx_vlan(struct DWC_ETH_QOS_prv_data *pdata,
					   struct sk_buff *skb,
			struct s_RX_NORMAL_DESC *rx_normal_desc)
{
	USHORT vlan_tag = 0;
	ULONG vlan_tag_strip = 0;

	/* Receive Status RDES0 Valid ? */
	if ((pdata->dev_state & NETIF_F_HW_VLAN_CTAG_RX) == NETIF_F_HW_VLAN_CTAG_RX) {
		if (rx_normal_desc->RDES3 & DWC_ETH_QOS_RDESC3_RS0V) {
			/* device received frame with VLAN Tag or
			 * double VLAN Tag ?
			 */
			if (((rx_normal_desc->RDES3 & DWC_ETH_QOS_RDESC3_LT) == 0x40000)
				|| ((rx_normal_desc->RDES3 & DWC_ETH_QOS_RDESC3_LT) == 0x50000)) {
				/* read the VLAN tag stripping register */
				MAC_VLANTR_EVLS_UDFRD(vlan_tag_strip);
				 if ( vlan_tag_strip ) {
					 vlan_tag = rx_normal_desc->RDES0 & 0xffff;
					 /* insert VLAN tag into skb */
#ifdef DWC_ETH_QOS_ENABLE_VLAN_TAG
					 __vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), vlan_tag);
#endif
				}
				pdata->xstats.rx_vlan_pkt_n++;
			}
		}
	}
}

/* This api check for payload type and returns
 * 1 if payload load is TCP else returns 0;
 */
static int DWC_ETH_QOS_check_for_tcp_payload(struct s_RX_NORMAL_DESC *rxdesc)
{
		u32 pt_type = 0;
		int ret = 0;

		if (rxdesc->RDES3 & DWC_ETH_QOS_RDESC3_RS1V) {
			pt_type = rxdesc->RDES1 & DWC_ETH_QOS_RDESC1_PT;
			if (pt_type == DWC_ETH_QOS_RDESC1_PT_TCP)
				ret = 1;
		}

		return ret;
}

/*!
 * \brief API to pass the Rx packets to stack if split header
 * feature is enabled.
 *
 * \details This function is invoked by main NAPI function if RX
 * split header feature is enabled. This function checks the device
 * descriptor for the packets and passes it to stack if any packets
 * are received by device.
 *
 * \param[in] pdata - pointer to private data structure.
 * \param[in] quota - maximum no. of packets that we are allowed to pass
 * to into the kernel.
 * \param[in] qinx - DMA channel/queue no. to be checked for packet.
 *
 * \return integer
 *
 * \retval number of packets received.
 */

static int DWC_ETH_QOS_clean_split_hdr_rx_irq(
			struct DWC_ETH_QOS_prv_data *pdata,
			int quota,
			UINT qinx)
{
	struct DWC_ETH_QOS_rx_wrapper_descriptor *desc_data =
	    GET_RX_WRAPPER_DESC(qinx);
	struct net_device *dev = pdata->dev;
	struct desc_if_struct *desc_if = &pdata->desc_if;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct sk_buff *skb = NULL;
	int received = 0;
	struct DWC_ETH_QOS_rx_buffer *buffer = NULL;
	struct s_RX_NORMAL_DESC *RX_NORMAL_DESC = NULL;
	u16 pkt_len;
	unsigned short hdr_len = 0;
	unsigned short payload_len = 0;
	unsigned char intermediate_desc_cnt = 0;
	unsigned char buf2_used = 0;
	int ret = 0;

	DBGPR("-->DWC_ETH_QOS_clean_split_hdr_rx_irq: qinx = %u, quota = %d\n",
	      qinx, quota);

	while (received < quota) {
		buffer = GET_RX_BUF_PTR(qinx, desc_data->cur_rx);
		RX_NORMAL_DESC = GET_RX_DESC_PTR(qinx, desc_data->cur_rx);

		/* check for data availability */
		if (!(RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_OWN)) {
#ifdef DWC_ETH_QOS_ENABLE_RX_DESC_DUMP
			dump_rx_desc(qinx, RX_NORMAL_DESC, desc_data->cur_rx);
#endif
			/* assign it to new skb */
			skb = buffer->skb;
			buffer->skb = NULL;

			/* first buffer pointer */
			dma_unmap_single(GET_MEM_PDEV_DEV, buffer->dma,
					 (2 * buffer->rx_hdr_size), DMA_FROM_DEVICE);
			buffer->dma = 0;

			/* second buffer pointer */
			dma_unmap_page(GET_MEM_PDEV_DEV, buffer->dma2,
				       PAGE_SIZE, DMA_FROM_DEVICE);
			buffer->dma2 = 0;

			/* get the packet length */
			pkt_len =
			    (RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_PL);

			/* FIRST desc and Receive Status RDES2 Valid ? */
			if ((RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_FD) &&
			    (RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_RS2V)) {
				/* get header length */
				hdr_len = (RX_NORMAL_DESC->RDES2 & DWC_ETH_QOS_RDESC2_HL);
				DBGPR("Device has %s HEADER SPLIT: hdr_len = %d\n",
				      (hdr_len ? "done" : "not done"), hdr_len);
				if (hdr_len)
					pdata->xstats.rx_split_hdr_pkt_n++;
			}

			/* check for bad packet,
			 * error is valid only for last descriptor(OWN + LD bit set).
			 */
			if ((RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_ES) &&
			    (RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_LD)) {
				DBGPR("Error in rcved pkt, failed to pass it to upper layer\n");
				dump_rx_desc(qinx, RX_NORMAL_DESC, desc_data->cur_rx);
				dev->stats.rx_errors++;
				DWC_ETH_QOS_update_rx_errors(dev,
							     RX_NORMAL_DESC->RDES3);

				/* recycle both page/buff and skb */
				buffer->skb = skb;
				if (desc_data->skb_top)
					dev_kfree_skb_any(desc_data->skb_top);

				desc_data->skb_top = NULL;
				goto next_desc;
			}

			if (!(RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_LD)) {
				intermediate_desc_cnt++;
				buf2_used = 1;
				/* this descriptor is only the beginning/middle */
				if (RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_FD) {
					/* this is the beginning of a chain */

					/* here skb/skb_top may contain
					 * if (device done split header)
					 *	only header
					 * else
					 *	header/(header + payload)
					 */
					desc_data->skb_top = skb;
					/* page2 always contain only payload */
					if (hdr_len) {
						/* add header len to first skb->len */
						skb_put(skb, hdr_len);
						payload_len = pdata->rx_buffer_len;
						skb_fill_page_desc(skb, 0,
								   buffer->page2, 0,
							payload_len);
					} else {
						/* add header len to first skb->len */
						skb_put(skb, buffer->rx_hdr_size);
						/* No split header, hence
						 * pkt_len = (payload + hdr_len)
						 */
						payload_len = (pkt_len - buffer->rx_hdr_size);
						skb_fill_page_desc(skb, 0,
								   buffer->page2, 0,
							payload_len);
					}
				} else {
					/* this is the middle of a chain */
					payload_len = pdata->rx_buffer_len;
					if (desc_data->skb_top != NULL)
						skb_fill_page_desc(desc_data->skb_top,skb_shinfo(desc_data->skb_top)->nr_frags,buffer->page2, 0,payload_len);
					/* re-use this skb, as consumed only the page */
					buffer->skb = skb;
				}
				if (desc_data->skb_top != NULL)
						DWC_ETH_QOS_consume_page_split_hdr(buffer,
								   desc_data->skb_top,
							 payload_len, buf2_used);
				goto next_desc;
			} else {
				if (!(RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_FD)) {
					buf2_used = 1;
					/* end of the chain */
					if (hdr_len) {
						payload_len = (pkt_len -
							(pdata->rx_buffer_len * intermediate_desc_cnt) -
							hdr_len);
					} else {
						payload_len = (pkt_len -
							(pdata->rx_buffer_len * intermediate_desc_cnt) -
							buffer->rx_hdr_size);
					}
					if (desc_data->skb_top != NULL) {
						skb_fill_page_desc(desc_data->skb_top,skb_shinfo(desc_data->skb_top)->nr_frags,buffer->page2, 0,payload_len);
						/* re-use this skb, as consumed only the page */
						buffer->skb = skb;
						skb = desc_data->skb_top;
					}
					desc_data->skb_top = NULL;
					if (skb != NULL)
						DWC_ETH_QOS_consume_page_split_hdr(buffer, skb,
									   payload_len, buf2_used);
				} else {
					/* no chain, got both FD + LD together */
					if (hdr_len) {
						buf2_used = 1;
						/* add header len to first skb->len */
						skb_put(skb, hdr_len);

						payload_len = pkt_len - hdr_len;
						skb_fill_page_desc(skb, 0,
								   buffer->page2, 0,
							payload_len);
					} else {
						/* No split header, hence
						 * payload_len = (payload + hdr_len)
						 */
						if (pkt_len > buffer->rx_hdr_size) {
							buf2_used = 1;
							/* add header len to first skb->len */
							skb_put(skb,
								buffer->rx_hdr_size);

							payload_len =
								(pkt_len - buffer->rx_hdr_size);
							skb_fill_page_desc(skb, 0,
									   buffer->page2, 0,
								payload_len);
						} else {
							buf2_used = 0;
							/* add header len to first skb->len */
							skb_put(skb, pkt_len);
							payload_len = 0; /* no data in page2 */
						}
					}
					DWC_ETH_QOS_consume_page_split_hdr(buffer,
									   skb, payload_len,
							buf2_used);
				}
				/* reset for next new packet/frame */
				intermediate_desc_cnt = 0;
				hdr_len = 0;
			}

			if (skb != NULL) {
				DWC_ETH_QOS_config_rx_csum(pdata, skb, RX_NORMAL_DESC);

#ifdef DWC_ETH_QOS_ENABLE_VLAN_TAG
				DWC_ETH_QOS_get_rx_vlan(pdata, skb, RX_NORMAL_DESC);
#endif
			}

#ifdef YDEBUG_FILTER
			DWC_ETH_QOS_check_rx_filter_status(RX_NORMAL_DESC);
#endif

			if ((pdata->hw_feat.tsstssel) && (pdata->hwts_rx_en)) {
				/* get rx tstamp if available */
				if (hw_if->rx_tstamp_available(RX_NORMAL_DESC)) {
					if (skb != NULL )
						ret = DWC_ETH_QOS_get_rx_hwtstamp(pdata,
									  skb, desc_data, qinx);
					if (ret == 0) {
						/* device has not yet updated the CONTEXT desc to hold the
						 * time stamp, hence delay the packet reception
						 */
						buffer->skb = skb;
						if (skb != NULL)
							buffer->dma = dma_map_single(GET_MEM_PDEV_DEV, skb->data,
								pdata->rx_buffer_len, DMA_FROM_DEVICE);
						if (dma_mapping_error(GET_MEM_PDEV_DEV, buffer->dma))
							dev_alert(&pdata->pdev->dev, "failed to do the RX dma map\n");

						goto rx_tstmp_failed;
					}
				}
			}

			if (!(dev->features & NETIF_F_GRO) &&
			    (dev->features & NETIF_F_LRO)) {
				pdata->tcp_pkt =
					DWC_ETH_QOS_check_for_tcp_payload(RX_NORMAL_DESC);
			}
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
			dev->last_rx = jiffies;
#endif
			/* update the statistics */
			dev->stats.rx_packets++;
			if ( skb != NULL) {
				dev->stats.rx_bytes += skb->len;
				DWC_ETH_QOS_receive_skb(pdata, dev, skb, qinx);
			}
			received++;
 next_desc:
			desc_data->dirty_rx++;
			if (desc_data->dirty_rx >= desc_data->skb_realloc_threshold)
				desc_if->realloc_skb(pdata, qinx);

			INCR_RX_DESC_INDEX(desc_data->cur_rx, 1, pdata->rx_queue[qinx].desc_cnt);
			buf2_used = 0;
		} else {
			/* no more data to read */
			break;
		}
	}

rx_tstmp_failed:

	if (desc_data->dirty_rx)
		desc_if->realloc_skb(pdata, qinx);

	DBGPR("<--DWC_ETH_QOS_clean_split_hdr_rx_irq: received = %d\n",
	      received);

	return received;
}

/*!
 * \brief API to pass the Rx packets to stack if jumbo frame
 * is enabled.
 *
 * \details This function is invoked by main NAPI function if Rx
 * jumbe frame is enabled. This function checks the device descriptor
 * for the packets and passes it to stack if any packets are received
 * by device.
 *
 * \param[in] pdata - pointer to private data structure.
 * \param[in] quota - maximum no. of packets that we are allowed to pass
 * to into the kernel.
 * \param[in] qinx - DMA channel/queue no. to be checked for packet.
 *
 * \return integer
 *
 * \retval number of packets received.
 */

static int DWC_ETH_QOS_clean_jumbo_rx_irq(struct DWC_ETH_QOS_prv_data *pdata,
					  int quota,
					  UINT qinx)
{
	struct DWC_ETH_QOS_rx_wrapper_descriptor *desc_data =
	    GET_RX_WRAPPER_DESC(qinx);
	struct net_device *dev = pdata->dev;
	struct desc_if_struct *desc_if = &pdata->desc_if;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct sk_buff *skb = NULL;
	int received = 0;
	struct DWC_ETH_QOS_rx_buffer *buffer = NULL;
	struct s_RX_NORMAL_DESC *RX_NORMAL_DESC = NULL;
	u16 pkt_len;
	UCHAR intermediate_desc_cnt = 0;
	unsigned int buf2_used;
	int ret = 0 ;

	DBGPR("-->DWC_ETH_QOS_clean_jumbo_rx_irq: qinx = %u, quota = %d\n",
	      qinx, quota);

	while (received < quota) {
		buffer = GET_RX_BUF_PTR(qinx, desc_data->cur_rx);
		RX_NORMAL_DESC = GET_RX_DESC_PTR(qinx, desc_data->cur_rx);

		/* check for data availability */
		if (!(RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_OWN)) {
#ifdef DWC_ETH_QOS_ENABLE_RX_DESC_DUMP
			dump_rx_desc(qinx, RX_NORMAL_DESC, desc_data->cur_rx);
#endif
			/* assign it to new skb */
			skb = buffer->skb;
			buffer->skb = NULL;

			/* first buffer pointer */
			dma_unmap_page(GET_MEM_PDEV_DEV, buffer->dma,
				       PAGE_SIZE, DMA_FROM_DEVICE);
			buffer->dma = 0;

			/* second buffer pointer */
			dma_unmap_page(GET_MEM_PDEV_DEV, buffer->dma2,
				       PAGE_SIZE, DMA_FROM_DEVICE);
			buffer->dma2 = 0;

			/* get the packet length */
			pkt_len =
				(RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_PL);

			/* check for bad packet,
			 * error is valid only for last descriptor (OWN + LD bit set).
			 */
			if ((RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_ES) &&
			    (RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_LD)) {
				DBGPR("Error in rcved pkt, failed to pass it to upper layer\n");
				dump_rx_desc(qinx, RX_NORMAL_DESC, desc_data->cur_rx);
				dev->stats.rx_errors++;
				DWC_ETH_QOS_update_rx_errors(dev,
							     RX_NORMAL_DESC->RDES3);

				/* recycle both page and skb */
				buffer->skb = skb;
				if (desc_data->skb_top)
					dev_kfree_skb_any(desc_data->skb_top);

				desc_data->skb_top = NULL;
				goto next_desc;
			}

			if (!(RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_LD)) {
				intermediate_desc_cnt++;
				buf2_used = 1;
				/* this descriptor is only the beginning/middle */
				if (RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_FD) {
					/* this is the beginning of a chain */
					desc_data->skb_top = skb;
					skb_fill_page_desc(skb, 0,
							   buffer->page, 0,
						pdata->rx_buffer_len);

					DBGPR("RX: pkt in second buffer pointer\n");
					skb_fill_page_desc(
						desc_data->skb_top,
						skb_shinfo(desc_data->skb_top)->nr_frags,
						buffer->page2, 0,
						pdata->rx_buffer_len);
				} else {
					/* this is the middle of a chain */
					if (desc_data->skb_top != NULL) {
						skb_fill_page_desc(desc_data->skb_top,
							   skb_shinfo(desc_data->skb_top)->nr_frags,
						buffer->page, 0,
						pdata->rx_buffer_len);
						DBGPR("RX: pkt in second buffer pointer\n");
						skb_fill_page_desc(desc_data->skb_top,
							   skb_shinfo(desc_data->skb_top)->nr_frags,
						buffer->page2, 0,
						pdata->rx_buffer_len);
					}
					/* re-use this skb, as consumed only the page */
					buffer->skb = skb;
				}
				if (desc_data->skb_top != NULL )
					DWC_ETH_QOS_consume_page(buffer,
							 desc_data->skb_top,
							 (pdata->rx_buffer_len * 2),
							 buf2_used);
				goto next_desc;
			} else {
				if (!(RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_FD)) {
					/* end of the chain */
					pkt_len =
						(pkt_len - (pdata->rx_buffer_len * intermediate_desc_cnt));
					if (pkt_len > pdata->rx_buffer_len) {
						if (desc_data->skb_top != NULL) {
							skb_fill_page_desc(desc_data->skb_top,
								   skb_shinfo(desc_data->skb_top)->nr_frags,
							buffer->page, 0,
							pdata->rx_buffer_len);
							DBGPR("RX: pkt in second buffer pointer\n");
							skb_fill_page_desc(desc_data->skb_top,
								   skb_shinfo(desc_data->skb_top)->nr_frags,
							buffer->page2, 0,
							(pkt_len - pdata->rx_buffer_len));
						}
						buf2_used = 1;
					} else {
						if (desc_data->skb_top != NULL)
							skb_fill_page_desc(desc_data->skb_top,
								   skb_shinfo(desc_data->skb_top)->nr_frags,
							buffer->page, 0,
							pkt_len);
						buf2_used = 0;
					}
					/* re-use this skb, as consumed only the page */
					buffer->skb = skb;
					if (desc_data->skb_top != NULL)
						skb = desc_data->skb_top;
					desc_data->skb_top = NULL;
					if (skb != NULL)
						DWC_ETH_QOS_consume_page(buffer, skb,
								 pkt_len,
								 buf2_used);
				} else {
					/* no chain, got both FD + LD together */

					/* code added for copybreak, this should improve
					 * performance for small pkts with large amount
					 * of reassembly being done in the stack
					 */
					if ((pkt_len <= DWC_ETH_QOS_COPYBREAK_DEFAULT)
					    && (skb_tailroom(skb) >= pkt_len)) {
						u8 *vaddr;

						vaddr =
						    kmap_atomic(buffer->page);
						memcpy(skb_tail_pointer(skb),
						       vaddr, pkt_len);
						kunmap_atomic(vaddr);
						/* re-use the page, so don't erase buffer->page/page2 */
						skb_put(skb, pkt_len);
					} else {
						if (pkt_len > pdata->rx_buffer_len) {
							skb_fill_page_desc(skb,
									   0, buffer->page,
								0,
								pdata->rx_buffer_len);

							DBGPR("RX: pkt in second buffer pointer\n");
							skb_fill_page_desc(skb,
									   skb_shinfo(skb)->nr_frags, buffer->page2,
								0,
								(pkt_len - pdata->rx_buffer_len));
							buf2_used = 1;
						} else {
							skb_fill_page_desc(skb,
									   0, buffer->page,
								0,
								pkt_len);
							buf2_used = 0;
						}
						DWC_ETH_QOS_consume_page(buffer,
									 skb,
								pkt_len,
								buf2_used);
					}
				}
				intermediate_desc_cnt = 0;
			}

			if (skb != NULL) {
				DWC_ETH_QOS_config_rx_csum(pdata, skb, RX_NORMAL_DESC);

#ifdef DWC_ETH_QOS_ENABLE_VLAN_TAG
				DWC_ETH_QOS_get_rx_vlan(pdata, skb, RX_NORMAL_DESC);
#endif
			}

#ifdef YDEBUG_FILTER
			DWC_ETH_QOS_check_rx_filter_status(RX_NORMAL_DESC);
#endif

			if ((pdata->hw_feat.tsstssel) && (pdata->hwts_rx_en)) {
				/* get rx tstamp if available */
				if (hw_if->rx_tstamp_available(RX_NORMAL_DESC)) {
					if (skb != NULL)
						ret = DWC_ETH_QOS_get_rx_hwtstamp(pdata, skb, desc_data, qinx);
					if (ret == 0) {
						/* device has not yet updated the CONTEXT desc to hold the
						 * time stamp, hence delay the packet reception
						 */
						buffer->skb = skb;
						if (skb != NULL)
							buffer->dma = dma_map_single(GET_MEM_PDEV_DEV, skb->data, pdata->rx_buffer_len, DMA_FROM_DEVICE);

						if (dma_mapping_error(GET_MEM_PDEV_DEV, buffer->dma))
							dev_alert(&pdata->pdev->dev, "failed to do the RX dma map\n");

						goto rx_tstmp_failed;
					}
				}
			}

			if (!(dev->features & NETIF_F_GRO) &&
			    (dev->features & NETIF_F_LRO)) {
				pdata->tcp_pkt =
					DWC_ETH_QOS_check_for_tcp_payload(RX_NORMAL_DESC);
			}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
			dev->last_rx = jiffies;
#endif
			/* update the statistics */
			dev->stats.rx_packets++;
			if (skb != NULL) {
				dev->stats.rx_bytes += skb->len;
				/* eth type trans needs skb->data to point to something */
				if (!pskb_may_pull(skb, ETH_HLEN)) {
					dev_alert(&pdata->pdev->dev, "pskb_may_pull failed\n");
					dev_kfree_skb_any(skb);
					goto next_desc;
				}
				DWC_ETH_QOS_receive_skb(pdata, dev, skb, qinx);
			}
			received++;
 next_desc:
			desc_data->dirty_rx++;
			if (desc_data->dirty_rx >= desc_data->skb_realloc_threshold)
				desc_if->realloc_skb(pdata, qinx);

			INCR_RX_DESC_INDEX(desc_data->cur_rx, 1, pdata->rx_queue[qinx].desc_cnt);
		} else {
			/* no more data to read */
			break;
		}
	}

rx_tstmp_failed:

	if (desc_data->dirty_rx)
		desc_if->realloc_skb(pdata, qinx);

	DBGPR("<--DWC_ETH_QOS_clean_jumbo_rx_irq: received = %d\n", received);

	return received;
}

/*!
 * \brief API to pass the Rx packets to stack if default mode
 * is enabled.
 *
 * \details This function is invoked by main NAPI function in default
 * Rx mode(non jumbo and non split header). This function checks the
 * device descriptor for the packets and passes it to stack if any packets
 * are received by device.
 *
 * \param[in] pdata - pointer to private data structure.
 * \param[in] quota - maximum no. of packets that we are allowed to pass
 * to into the kernel.
 * \param[in] qinx - DMA channel/queue no. to be checked for packet.
 *
 * \return integer
 *
 * \retval number of packets received.
 */

static int DWC_ETH_QOS_clean_rx_irq(struct DWC_ETH_QOS_prv_data *pdata,
				    int quota,
				    UINT qinx)
{
	struct DWC_ETH_QOS_rx_wrapper_descriptor *desc_data =
	    GET_RX_WRAPPER_DESC(qinx);
	struct net_device *dev = pdata->dev;
	struct desc_if_struct *desc_if = &pdata->desc_if;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct sk_buff *skb = NULL;
	int received = 0;
	struct DWC_ETH_QOS_rx_buffer *buffer = NULL;
	struct s_RX_NORMAL_DESC *RX_NORMAL_DESC = NULL;
	UINT pkt_len;
	int ret;

	DBGPR("-->DWC_ETH_QOS_clean_rx_irq: qinx = %u, quota = %d\n",
	      qinx, quota);

	while (received < quota) {
		buffer = GET_RX_BUF_PTR(qinx, desc_data->cur_rx);
		RX_NORMAL_DESC = GET_RX_DESC_PTR(qinx, desc_data->cur_rx);

		/* check for data availability */
		if (!(RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_OWN)) {
#ifdef DWC_ETH_QOS_ENABLE_RX_DESC_DUMP
			dump_rx_desc(qinx, RX_NORMAL_DESC, desc_data->cur_rx);
#endif
			/* assign it to new skb */
			skb = buffer->skb;
			buffer->skb = NULL;
			dma_unmap_single(GET_MEM_PDEV_DEV, buffer->dma,
					 pdata->rx_buffer_len, DMA_FROM_DEVICE);
			buffer->dma = 0;

			/* get the packet length */
			pkt_len =
			    (RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_PL);

#ifdef DWC_ETH_QOS_ENABLE_RX_PKT_DUMP
			print_pkt(skb, pkt_len, 0, (desc_data->cur_rx));
#endif
			/* check for bad/oversized packet,
			 * error is valid only for last descriptor (OWN + LD bit set).
			 */
			if (!(RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_ES) &&
			    (RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_LD)) {
				/* pkt_len = pkt_len - 4; */ /* CRC stripping */
#ifdef DWC_ETH_QOS_COPYBREAK_ENABLED
				/* code added for copybreak, this should improve
				 * performance for small pkts with large amount
				 * of reassembly being done in the stack
				 */
				if (pkt_len < DWC_ETH_QOS_COPYBREAK_DEFAULT) {
					struct sk_buff *new_skb =
					    netdev_alloc_skb_ip_align(dev,
								      pkt_len);
					if (new_skb) {
						skb_copy_to_linear_data_offset(new_skb,
									       -NET_IP_ALIGN,
							(skb->data - NET_IP_ALIGN),
							(pkt_len + NET_IP_ALIGN));
						/* recycle actual desc skb */
						buffer->skb = skb;
						skb = new_skb;
					} else {
						/* just continue with the old skb */
					}
				}
#endif
				skb_put(skb, pkt_len);

				DWC_ETH_QOS_config_rx_csum(pdata, skb,
							   RX_NORMAL_DESC);

#ifdef DWC_ETH_QOS_ENABLE_VLAN_TAG
				DWC_ETH_QOS_get_rx_vlan(pdata, skb, RX_NORMAL_DESC);
#endif

#ifdef YDEBUG_FILTER
				DWC_ETH_QOS_check_rx_filter_status(
				   RX_NORMAL_DESC);
#endif

				if ((pdata->hw_feat.tsstssel) &&
				    (pdata->hwts_rx_en)) {
					/* get rx tstamp if available */
					if (hw_if->rx_tstamp_available(
					   RX_NORMAL_DESC)) {
						ret =
						DWC_ETH_QOS_get_rx_hwtstamp(pdata,
									    skb, desc_data, qinx);
						if (ret == 0) {
							/* device has not yet updated the CONTEXT
							 * desc to hold the time stamp, hence delay
							 * the packet reception
							 */
							buffer->skb = skb;
							buffer->dma =
								dma_map_single(GET_MEM_PDEV_DEV, skb->data,
									       pdata->rx_buffer_len, DMA_FROM_DEVICE);
							if (dma_mapping_error(GET_MEM_PDEV_DEV, buffer->dma))
								dev_alert(&pdata->pdev->dev,
									  "failed to do the RX dma map\n");

							goto rx_tstmp_failed;
						}
					}
				}

				if (!(dev->features & NETIF_F_GRO) &&
				    (dev->features & NETIF_F_LRO)) {
					pdata->tcp_pkt =
						DWC_ETH_QOS_check_for_tcp_payload(
						   RX_NORMAL_DESC);
				}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
			dev->last_rx = jiffies;
#endif
				/* update the statistics */
				dev->stats.rx_packets++;
				dev->stats.rx_bytes += skb->len;
#ifdef DWC_ETH_QOS_BUILTIN
				if (dev->stats.rx_packets == 1)
					EMACINFO("Received First Rx packet\n");
#endif
				DWC_ETH_QOS_receive_skb(pdata, dev, skb, qinx);
				received++;
#ifdef CONFIG_MSM_BOOT_TIME_MARKER
				if ( dev->stats.rx_packets == 1) {
					place_marker("M - Ethernet first packet received");
				}
#endif
			} else {
				dump_rx_desc(qinx, RX_NORMAL_DESC, desc_data->cur_rx);
				if (!(RX_NORMAL_DESC->RDES3 &
					  DWC_ETH_QOS_RDESC3_LD))
					DBGPR("Received oversized pkt, spanned across multiple desc\n");

				if (RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_DRIBBLE_ERR)
					EMACERR("Received Dribble Error(19)\n");
				if (RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_RECEIVE_ERR)
					EMACERR("Received Receive Error(20)\n");
				if (RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_OVERFLOW_ERR)
					EMACERR("Received Overflow Error(21)\n");
				if (RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_WTO_ERR)
					EMACERR("Received Watchdog Timeout Error(22)\n");
				if (RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_GAINT_PKT_ERR)
					EMACERR("Received Gaint Packet Error(23)\n");
				if (RX_NORMAL_DESC->RDES3 & DWC_ETH_QOS_RDESC3_CRC_ERR)
					EMACERR("Received CRC Error(24)\n");

				/* recycle skb */
				buffer->skb = skb;
				dev->stats.rx_errors++;
				DWC_ETH_QOS_update_rx_errors(dev,
							     RX_NORMAL_DESC->RDES3);
			}

			desc_data->dirty_rx++;
			if (desc_data->dirty_rx >=
				desc_data->skb_realloc_threshold)
				desc_if->realloc_skb(pdata, qinx);

			INCR_RX_DESC_INDEX(desc_data->cur_rx, 1, pdata->rx_queue[qinx].desc_cnt);
		} else {
			/* no more data to read */
			break;
		}
	}

rx_tstmp_failed:

	if (desc_data->dirty_rx)
		desc_if->realloc_skb(pdata, qinx);

	DBGPR("<--DWC_ETH_QOS_clean_rx_irq: received = %d\n", received);

	return received;
}

/*!
 * \brief API to update the rx status.
 *
 * \details This function is called in poll function to update the
 * status of received packets.
 *
 * \param[in] dev - pointer to net_device structure.
 * \param[in] rx_status - value of received packet status.
 *
 * \return void.
 */

void DWC_ETH_QOS_update_rx_errors(struct net_device *dev,
				  unsigned int rx_status)
{
	DBGPR("-->DWC_ETH_QOS_update_rx_errors\n");

	/* received pkt with crc error */
	if ((rx_status & DWC_ETH_QOS_RDESC3_CRC_ERR))
		dev->stats.rx_crc_errors++;

	/* received frame alignment */
	if ((rx_status & 0x100000))
		dev->stats.rx_frame_errors++;

	/* receiver fifo overrun */
	if ((rx_status & 0x200000))
		dev->stats.rx_fifo_errors++;

	DBGPR("<--DWC_ETH_QOS_update_rx_errors\n");
}

/*!
 * \brief API to pass the received packets to stack
 *
 * \details This function is provided by NAPI-compliant drivers to operate
 * the interface in a polled mode, with interrupts disabled.
 *
 * \param[in] napi - pointer to napi_struct structure.
 * \param[in] budget - maximum no. of packets that we are allowed to pass
 * to into the kernel.
 *
 * \return integer
 *
 * \retval number of packets received.
 */

int DWC_ETH_QOS_poll_mq(struct napi_struct *napi, int budget)
{
	struct DWC_ETH_QOS_rx_queue *rx_queue =
		container_of(napi, struct DWC_ETH_QOS_rx_queue, napi);
	struct DWC_ETH_QOS_prv_data *pdata = rx_queue->pdata;
	/* divide the budget evenly among all the queues */
	int per_q_budget = budget / DWC_ETH_QOS_RX_QUEUE_CNT;
	int qinx = 0;
	int received = 0, per_q_received = 0;
	unsigned long flags;

	DBGPR("-->DWC_ETH_QOS_poll_mq: budget = %d\n", budget);

	pdata->xstats.napi_poll_n++;
	for (qinx = 0; qinx < DWC_ETH_QOS_RX_QUEUE_CNT; qinx++) {
		rx_queue = GET_RX_QUEUE_PTR(qinx);

#ifdef DWC_ETH_QOS_TXPOLLING_MODE_ENABLE
		/* check for tx descriptor status */
		DWC_ETH_QOS_tx_interrupt(pdata->dev, pdata, qinx);
#endif

	if (pdata->ipa_enabled && qinx == IPA_DMA_RX_CH)
		continue;

#ifdef DWC_INET_LRO
		rx_queue->lro_flush_needed = 0;
#endif

#ifdef RX_OLD_CODE
		per_q_received = DWC_ETH_QOS_poll(pdata, per_q_budget, qinx);
#else
		per_q_received = pdata->clean_rx(pdata, per_q_budget, qinx);
#endif
		received += per_q_received;
		pdata->xstats.rx_pkt_n += per_q_received;
		pdata->xstats.q_rx_pkt_n[qinx] += per_q_received;
#ifdef DWC_INET_LRO
		if (rx_queue->lro_flush_needed)
			lro_flush_all(&rx_queue->lro_mgr);
#endif
	}

	/* If we processed all pkts, we are done;
	 * tell the kernel & re-enable interrupt
	 */
	if (received < budget) {
		if (pdata->dev->features & NETIF_F_GRO) {
			/* to turn off polling */
			napi_complete(napi);
			spin_lock_irqsave(&pdata->lock, flags);
			/* Enable all ch RX interrupt */
			DWC_ETH_QOS_enable_all_ch_rx_interrpt(pdata);
			spin_unlock_irqrestore(&pdata->lock, flags);
		} else {

			spin_lock_irqsave(&pdata->lock, flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
			__napi_complete(napi);
#else
			napi_complete_done(napi, received);
#endif
			/* Enable all ch RX interrupt */
			DWC_ETH_QOS_enable_all_ch_rx_interrpt(pdata);
			spin_unlock_irqrestore(&pdata->lock, flags);
		}
	}

	DBGPR("<--DWC_ETH_QOS_poll_mq\n");

	return received;
}

/*!
 * \brief API to return the device/interface status.
 *
 * \details The get_stats function is called whenever an application needs to
 * get statistics for the interface. For example, this happened when ifconfig
 * or netstat -i is run.
 *
 * \param[in] dev - pointer to net_device structure.
 *
 * \return net_device_stats structure
 *
 * \retval net_device_stats - returns pointer to net_device_stats structure.
 */

static struct net_device_stats *DWC_ETH_QOS_get_stats(struct net_device *dev)
{
	return &dev->stats;
}

#ifdef CONFIG_NET_POLL_CONTROLLER

/*!
 * \brief API to receive packets in polling mode.
 *
 * \details This is polling receive function used by netconsole and other
 * diagnostic tool to allow network i/o with interrupts disabled.
 *
 * \param[in] dev - pointer to net_device structure
 *
 * \return void
 */

static void DWC_ETH_QOS_poll_controller(struct net_device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);

	DBGPR("-->DWC_ETH_QOS_poll_controller\n");

	disable_irq(pdata->irq_number);
#ifdef PER_CH_INT
	DWC_ETH_QOS_dis_en_ch_intr(pdata, false);
#endif
	DWC_ETH_QOS_ISR_SW_DWC_ETH_QOS(pdata->irq_number, pdata);
	enable_irq(pdata->irq_number);
#ifdef PER_CH_INT
	DWC_ETH_QOS_dis_en_ch_intr(pdata, true);
#endif

	DBGPR("<--DWC_ETH_QOS_poll_controller\n");
}

#endif	/*end of CONFIG_NET_POLL_CONTROLLER */

/*!
 * \brief User defined parameter setting API
 *
 * \details This function is invoked by kernel to update the device
 * configuration to new features. This function supports enabling and
 * disabling of TX and RX csum features.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] features – device feature to be enabled/disabled.
 *
 * \return int
 *
 * \retval 0
 */

static int DWC_ETH_QOS_set_features(
	struct net_device *dev, netdev_features_t features)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	UINT dev_rxcsum_enable;
#ifdef DWC_ETH_QOS_ENABLE_VLAN_TAG
	UINT dev_rxvlan_enable, dev_txvlan_enable;
#endif

	if (pdata->hw_feat.rx_coe_sel) {
		dev_rxcsum_enable = !!(pdata->dev_state & NETIF_F_RXCSUM);

		if (((features & NETIF_F_RXCSUM) == NETIF_F_RXCSUM)
		    && !dev_rxcsum_enable) {
			hw_if->enable_rx_csum();
			pdata->dev_state |= NETIF_F_RXCSUM;
			dev_alert(&pdata->pdev->dev, "State change - rxcsum enable\n");
		} else if (((features & NETIF_F_RXCSUM) == 0)
			   && dev_rxcsum_enable) {
			hw_if->disable_rx_csum();
			pdata->dev_state &= ~NETIF_F_RXCSUM;
			dev_alert(&pdata->pdev->dev, "State change - rxcsum disable\n");
		}
	}
#ifdef DWC_ETH_QOS_ENABLE_VLAN_TAG
	dev_rxvlan_enable = !!(pdata->dev_state & NETIF_F_HW_VLAN_CTAG_RX);
	if (((features & NETIF_F_HW_VLAN_CTAG_RX) == NETIF_F_HW_VLAN_CTAG_RX)
	    && !dev_rxvlan_enable) {
		pdata->dev_state |= NETIF_F_HW_VLAN_CTAG_RX;
		hw_if->config_rx_outer_vlan_stripping(
		   DWC_ETH_QOS_RX_VLAN_STRIP_ALWAYS);
		dev_alert(&pdata->pdev->dev, "State change - rxvlan enable\n");
	} else if (((features & NETIF_F_HW_VLAN_CTAG_RX) == 0) &&
			dev_rxvlan_enable) {
		pdata->dev_state &= ~NETIF_F_HW_VLAN_CTAG_RX;
		hw_if->config_rx_outer_vlan_stripping(
		   DWC_ETH_QOS_RX_NO_VLAN_STRIP);
		dev_alert(&pdata->pdev->dev, "State change - rxvlan disable\n");
	}

	dev_txvlan_enable = !!(pdata->dev_state & NETIF_F_HW_VLAN_CTAG_TX_BIT);
	if (((features & NETIF_F_HW_VLAN_CTAG_TX_BIT) ==
		 NETIF_F_HW_VLAN_CTAG_TX_BIT)
	    && !dev_txvlan_enable) {
		pdata->dev_state |= NETIF_F_HW_VLAN_CTAG_TX_BIT;
		dev_alert(&pdata->pdev->dev, "State change - txvlan enable\n");
	} else if (((features & NETIF_F_HW_VLAN_CTAG_TX_BIT) == 0) &&
			dev_txvlan_enable) {
		pdata->dev_state &= ~NETIF_F_HW_VLAN_CTAG_TX_BIT;
		dev_alert(&pdata->pdev->dev, "State change - txvlan disable\n");
	}
#endif	/* DWC_ETH_QOS_ENABLE_VLAN_TAG */

	DBGPR("<--DWC_ETH_QOS_set_features\n");

	return 0;
}

/*!
 * \brief User defined parameter setting API
 *
 * \details This function is invoked by kernel to adjusts the requested
 * feature flags according to device-specific constraints, and returns the
 * resulting flags. This API must not modify the device state.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] features – device supported features.
 *
 * \return u32
 *
 * \retval modified flag
 */
static netdev_features_t DWC_ETH_QOS_fix_features(
	struct net_device *dev, netdev_features_t features)
{
#ifdef DWC_ETH_QOS_ENABLE_VLAN_TAG
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);

	DBGPR("-->DWC_ETH_QOS_fix_features: %#llx\n", features);

	if (pdata->rx_split_hdr) {
		/* The VLAN tag stripping must be set for the split function.
		 * For instance, the DMA separates the header and payload of
		 * an untagged packet only. Hence, when a tagged packet is
		 * received, the QOS must be programmed such that the VLAN
		 * tags are deleted/stripped from the received packets.
		 *
		 */
		features |= NETIF_F_HW_VLAN_CTAG_RX;
	}
#endif /* end of DWC_ETH_QOS_ENABLE_VLAN_TAG */

	DBGPR("<--DWC_ETH_QOS_fix_features: %#llx\n", features);

	return features;
}

/*!
 * \details This function is invoked by ioctl function when user issues
 * an ioctl command to enable/disable receive split header mode.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] flags – flag to indicate whether RX split to be
 *                  enabled/disabled.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int DWC_ETH_QOS_config_rx_split_hdr_mode(struct net_device *dev,
						unsigned int flags)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned int qinx;
	int ret = 0;

	DBGPR("-->DWC_ETH_QOS_config_rx_split_hdr_mode\n");

	if (flags && pdata->rx_split_hdr) {
		dev_alert(&pdata->pdev->dev,
			  "Rx Split header mode is already enabled\n");
		return -EINVAL;
	}

	if (!flags && !pdata->rx_split_hdr) {
		dev_alert(&pdata->pdev->dev,
			  "Rx Split header mode is already disabled\n");
		return -EINVAL;
	}

	DWC_ETH_QOS_stop_dev(pdata);

	/* If split header mode is disabled(ie flags == 0)
	 * then RX will be in default/jumbo mode based on MTU
	 *
	 */
	pdata->rx_split_hdr = !!flags;

	DWC_ETH_QOS_start_dev(pdata);

	hw_if->config_header_size(DWC_ETH_QOS_MAX_HDR_SIZE);
	/* enable/disable split header for all RX DMA channel */
	for (qinx = 0; qinx < DWC_ETH_QOS_RX_QUEUE_CNT; qinx++)
		hw_if->config_split_header_mode(qinx, pdata->rx_split_hdr);

	dev_alert(&pdata->pdev->dev, "Successfully %s Rx Split header mode\n",
		  (flags ? "enabled" : "disabled"));

	DBGPR("<--DWC_ETH_QOS_config_rx_split_hdr_mode\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues
 * an ioctl command to enable/disable L3/L4 filtering.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] flags – flag to indicate whether L3/L4 filtering to be
 *                  enabled/disabled.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int DWC_ETH_QOS_config_l3_l4_filtering(struct net_device *dev,
					      unsigned int flags)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int ret = 0;

	DBGPR_FILTER("-->DWC_ETH_QOS_config_l3_l4_filtering\n");

	if (flags && pdata->l3_l4_filter) {
		dev_alert(&pdata->pdev->dev,
			  "L3/L4 filtering is already enabled\n");
		return -EINVAL;
	}

	if (!flags && !pdata->l3_l4_filter) {
		dev_alert(&pdata->pdev->dev,
			  "L3/L4 filtering is already disabled\n");
		return -EINVAL;
	}

	pdata->l3_l4_filter = !!flags;
	hw_if->config_l3_l4_filter_enable(pdata->l3_l4_filter);

	DBGPR_FILTER("Successfully %s L3/L4 filtering\n",
		     (flags ? "ENABLED" : "DISABLED"));

	DBGPR_FILTER("<--DWC_ETH_QOS_config_l3_l4_filtering\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues an
 * ioctl command to configure L3(IPv4) filtering. This function does following,
 * - enable/disable IPv4 filtering.
 * - select source/destination address matching.
 * - select perfect/inverse matching.
 * - Update the IPv4 address into MAC register.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] req – pointer to IOCTL specific structure.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int DWC_ETH_QOS_config_ip4_filters(struct net_device *dev,
					  struct ifr_data_struct *req)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct DWC_ETH_QOS_l3_l4_filter *u_l3_filter =
		(struct DWC_ETH_QOS_l3_l4_filter *)req->ptr;
	struct DWC_ETH_QOS_l3_l4_filter l_l3_filter;
	int ret = 0;

	DBGPR_FILTER("-->DWC_ETH_QOS_config_ip4_filters\n");

	if (pdata->hw_feat.l3l4_filter_num == 0)
		return DWC_ETH_QOS_NO_HW_SUPPORT;

	if (copy_from_user(&l_l3_filter, u_l3_filter,
			   sizeof(struct DWC_ETH_QOS_l3_l4_filter)))
		return -EFAULT;

	if ((l_l3_filter.filter_no + 1) > pdata->hw_feat.l3l4_filter_num ||
		l_l3_filter.filter_no > (UINT_MAX - pdata->hw_feat.l3l4_filter_num)) {
		dev_alert(&pdata->pdev->dev, "%d filter is not supported in the HW\n",
			  l_l3_filter.filter_no);
		return DWC_ETH_QOS_NO_HW_SUPPORT;
	}

	if (!pdata->l3_l4_filter) {
		hw_if->config_l3_l4_filter_enable(1);
		pdata->l3_l4_filter = 1;
	}

	/* configure the L3 filters */
	hw_if->config_l3_filters(l_l3_filter.filter_no,
			l_l3_filter.filter_enb_dis, 0,
			l_l3_filter.src_dst_addr_match,
			l_l3_filter.perfect_inverse_match);

	if (!l_l3_filter.src_dst_addr_match)
		hw_if->update_ip4_addr0(l_l3_filter.filter_no,
				l_l3_filter.ip4_addr);
	else
		hw_if->update_ip4_addr1(l_l3_filter.filter_no,
				l_l3_filter.ip4_addr);

	DBGPR_FILTER("Successfully %s IPv4 %s %s addressing filtering on %d filter\n",
		     (l_l3_filter.filter_enb_dis ? "ENABLED" : "DISABLED"),
		(l_l3_filter.perfect_inverse_match ? "INVERSE" : "PERFECT"),
		(l_l3_filter.src_dst_addr_match ? "DESTINATION" : "SOURCE"),
		l_l3_filter.filter_no);

	DBGPR_FILTER("<--DWC_ETH_QOS_config_ip4_filters\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues an
 * ioctl command to configure L3(IPv6) filtering. This function does following,
 * - enable/disable IPv6 filtering.
 * - select source/destination address matching.
 * - select perfect/inverse matching.
 * - Update the IPv6 address into MAC register.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] req – pointer to IOCTL specific structure.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int DWC_ETH_QOS_config_ip6_filters(struct net_device *dev,
					  struct ifr_data_struct *req)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct DWC_ETH_QOS_l3_l4_filter *u_l3_filter =
		(struct DWC_ETH_QOS_l3_l4_filter *)req->ptr;
	struct DWC_ETH_QOS_l3_l4_filter l_l3_filter;
	int ret = 0;

	DBGPR_FILTER("-->DWC_ETH_QOS_config_ip6_filters\n");

	if (pdata->hw_feat.l3l4_filter_num == 0)
		return DWC_ETH_QOS_NO_HW_SUPPORT;

	if (copy_from_user(&l_l3_filter, u_l3_filter,
			   sizeof(struct DWC_ETH_QOS_l3_l4_filter)))
		return -EFAULT;

	if ((l_l3_filter.filter_no + 1) > pdata->hw_feat.l3l4_filter_num ||
		l_l3_filter.filter_no > (UINT_MAX - pdata->hw_feat.l3l4_filter_num)) {
		dev_alert(&pdata->pdev->dev, "%d filter is not supported in the HW\n",
			  l_l3_filter.filter_no);
		return DWC_ETH_QOS_NO_HW_SUPPORT;
	}

	if (!pdata->l3_l4_filter) {
		hw_if->config_l3_l4_filter_enable(1);
		pdata->l3_l4_filter = 1;
	}

	/* configure the L3 filters */
	hw_if->config_l3_filters(l_l3_filter.filter_no,
			l_l3_filter.filter_enb_dis, 1,
			l_l3_filter.src_dst_addr_match,
			l_l3_filter.perfect_inverse_match);

	hw_if->update_ip6_addr(l_l3_filter.filter_no,
			l_l3_filter.ip6_addr);

	DBGPR_FILTER("Successfully %s IPv6 %s %s addressing filtering on %d filter\n",
		     (l_l3_filter.filter_enb_dis ? "ENABLED" : "DISABLED"),
		(l_l3_filter.perfect_inverse_match ? "INVERSE" : "PERFECT"),
		(l_l3_filter.src_dst_addr_match ? "DESTINATION" : "SOURCE"),
		l_l3_filter.filter_no);

	DBGPR_FILTER("<--DWC_ETH_QOS_config_ip6_filters\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues an
 * ioctl command to configure L4(TCP/UDP) filtering.
 * This function does following,
 * - enable/disable L4 filtering.
 * - select TCP/UDP filtering.
 * - select source/destination port matching.
 * - select perfect/inverse matching.
 * - Update the port number into MAC register.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] req – pointer to IOCTL specific structure.
 * \param[in] tcp_udp – flag to indicate TCP/UDP filtering.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int DWC_ETH_QOS_config_tcp_udp_filters(struct net_device *dev,
					      struct ifr_data_struct *req,
		int tcp_udp)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct DWC_ETH_QOS_l3_l4_filter *u_l4_filter =
		(struct DWC_ETH_QOS_l3_l4_filter *)req->ptr;
	struct DWC_ETH_QOS_l3_l4_filter l_l4_filter;
	int ret = 0;

	DBGPR_FILTER("-->DWC_ETH_QOS_config_tcp_udp_filters\n");

	if (pdata->hw_feat.l3l4_filter_num == 0)
		return DWC_ETH_QOS_NO_HW_SUPPORT;

	if (copy_from_user(&l_l4_filter, u_l4_filter,
			   sizeof(struct DWC_ETH_QOS_l3_l4_filter)))
		return -EFAULT;

	if ((l_l4_filter.filter_no + 1) > pdata->hw_feat.l3l4_filter_num ||
		l_l4_filter.filter_no > (UINT_MAX - pdata->hw_feat.l3l4_filter_num)) {
		dev_alert(&pdata->pdev->dev, "%d filter is not supported in the HW\n",
			  l_l4_filter.filter_no);
		return DWC_ETH_QOS_NO_HW_SUPPORT;
	}

	if (!pdata->l3_l4_filter) {
		hw_if->config_l3_l4_filter_enable(1);
		pdata->l3_l4_filter = 1;
	}

	/* configure the L4 filters */
	hw_if->config_l4_filters(l_l4_filter.filter_no,
			l_l4_filter.filter_enb_dis,
			tcp_udp,
			l_l4_filter.src_dst_addr_match,
			l_l4_filter.perfect_inverse_match);

	if (l_l4_filter.src_dst_addr_match)
		hw_if->update_l4_da_port_no(l_l4_filter.filter_no,
				l_l4_filter.port_no);
	else
		hw_if->update_l4_sa_port_no(l_l4_filter.filter_no,
				l_l4_filter.port_no);

	DBGPR_FILTER("Successfully %s %s %s %s Port number filtering on %d filter\n",
		     (l_l4_filter.filter_enb_dis ? "ENABLED" : "DISABLED"),
		(tcp_udp ? "UDP" : "TCP"),
		(l_l4_filter.perfect_inverse_match ? "INVERSE" : "PERFECT"),
		(l_l4_filter.src_dst_addr_match ? "DESTINATION" : "SOURCE"),
		l_l4_filter.filter_no);

	DBGPR_FILTER("<--DWC_ETH_QOS_config_tcp_udp_filters\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues an
 * ioctl command to configure VALN filtering. This function does following,
 * - enable/disable VLAN filtering.
 * - select perfect/hash filtering.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] req – pointer to IOCTL specific structure.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int DWC_ETH_QOS_config_vlan_filter(struct net_device *dev,
					  struct ifr_data_struct *req)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct DWC_ETH_QOS_vlan_filter *u_vlan_filter =
		(struct DWC_ETH_QOS_vlan_filter *)req->ptr;
	struct DWC_ETH_QOS_vlan_filter l_vlan_filter;
	int ret = 0;

	DBGPR_FILTER("-->DWC_ETH_QOS_config_vlan_filter\n");

	if (copy_from_user(&l_vlan_filter, u_vlan_filter,
			   sizeof(struct DWC_ETH_QOS_vlan_filter)))
		return -EFAULT;

	if ((l_vlan_filter.perfect_hash) &&
	    (pdata->hw_feat.vlan_hash_en == 0)) {
		dev_alert(&pdata->pdev->dev, "VLAN HASH filtering is not supported\n");
		return DWC_ETH_QOS_NO_HW_SUPPORT;
	}

	/* configure the vlan filter */
	hw_if->config_vlan_filtering(l_vlan_filter.filter_enb_dis,
					l_vlan_filter.perfect_hash,
					l_vlan_filter.perfect_inverse_match);
	pdata->vlan_hash_filtering = l_vlan_filter.perfect_hash;

	DBGPR_FILTER("Successfully %s VLAN %s filtering and %s matching\n",
		     (l_vlan_filter.filter_enb_dis ? "ENABLED" : "DISABLED"),
		(l_vlan_filter.perfect_hash ? "HASH" : "PERFECT"),
		(l_vlan_filter.perfect_inverse_match ? "INVERSE" : "PERFECT"));

	DBGPR_FILTER("<--DWC_ETH_QOS_config_vlan_filter\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues an
 * ioctl command to enable/disable ARP offloading feature.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] req – pointer to IOCTL specific structure.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int DWC_ETH_QOS_config_arp_offload(struct net_device *dev,
					  struct ifr_data_struct *req)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct DWC_ETH_QOS_arp_offload *u_arp_offload =
		(struct DWC_ETH_QOS_arp_offload *)req->ptr;
	struct DWC_ETH_QOS_arp_offload l_arp_offload;
	int ret = 0;

	dev_alert(&pdata->pdev->dev, "-->DWC_ETH_QOS_config_arp_offload\n");

	if (pdata->hw_feat.arp_offld_en == 0)
		return DWC_ETH_QOS_NO_HW_SUPPORT;

	if (copy_from_user(&l_arp_offload, u_arp_offload,
			   sizeof(struct DWC_ETH_QOS_arp_offload)))
		return -EFAULT;

	/* configure the L3 filters */
	hw_if->config_arp_offload(req->flags);
	hw_if->update_arp_offload_ip_addr(l_arp_offload.ip_addr);
	pdata->arp_offload = req->flags;

	dev_alert(&pdata->pdev->dev, "Successfully %s arp Offload\n",
		  (req->flags ? "ENABLED" : "DISABLED"));

	dev_alert(&pdata->pdev->dev, "<--DWC_ETH_QOS_config_arp_offload\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues an
 * ioctl command to configure L2 destination addressing filtering mode. This
 * function dose following,
 * - selects perfect/hash filtering.
 * - selects perfect/inverse matching.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] req – pointer to IOCTL specific structure.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int DWC_ETH_QOS_confing_l2_da_filter(struct net_device *dev,
					    struct ifr_data_struct *req)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct DWC_ETH_QOS_l2_da_filter *u_l2_da_filter =
	  (struct DWC_ETH_QOS_l2_da_filter *)req->ptr;
	struct DWC_ETH_QOS_l2_da_filter l_l2_da_filter;
	int ret = 0;

	DBGPR_FILTER("-->DWC_ETH_QOS_confing_l2_da_filter\n");

	if (copy_from_user(&l_l2_da_filter, u_l2_da_filter,
			   sizeof(struct DWC_ETH_QOS_l2_da_filter)))
		return -EFAULT;

	if (l_l2_da_filter.perfect_hash) {
		if (pdata->hw_feat.hash_tbl_sz > 0)
			pdata->l2_filtering_mode = 1;
		else
			ret = DWC_ETH_QOS_NO_HW_SUPPORT;
	} else {
		if (pdata->max_addr_reg_cnt > 1)
			pdata->l2_filtering_mode = 0;
		else
			ret = DWC_ETH_QOS_NO_HW_SUPPORT;
	}

	/* configure L2 DA perfect/inverse_matching */
	hw_if->config_l2_da_perfect_inverse_match(
	   l_l2_da_filter.perfect_inverse_match);

	DBGPR_FILTER("Successfully selected L2 %s filtering and\n",
		     (l_l2_da_filter.perfect_hash ? "HASH" : "PERFECT"));
	DBGPR_FILTER(" %s DA matching\n",
		     (l_l2_da_filter.perfect_inverse_match ? "INVERSE" : "PERFECT"));

	DBGPR_FILTER("<--DWC_ETH_QOS_confing_l2_da_filter\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues
 * an ioctl command to enable/disable mac loopback mode.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] flags – flag to indicate whether mac loopback mode to be
 *                  enabled/disabled.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int DWC_ETH_QOS_config_mac_loopback_mode(struct net_device *dev,
						unsigned int flags)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int ret = 0;

	DBGPR("-->DWC_ETH_QOS_config_mac_loopback_mode\n");

	if (flags && pdata->mac_loopback_mode) {
		dev_alert(&pdata->pdev->dev,
			  "MAC loopback mode is already enabled\n");
		return -EINVAL;
	}
	if (!flags && !pdata->mac_loopback_mode) {
		dev_alert(&pdata->pdev->dev,
			  "MAC loopback mode is already disabled\n");
		return -EINVAL;
	}
	pdata->mac_loopback_mode = !!flags;
	hw_if->config_mac_loopback_mode(flags);

	dev_alert(&pdata->pdev->dev, "Successfully %s MAC loopback mode\n",
		  (flags ? "enabled" : "disabled"));

	DBGPR("<--DWC_ETH_QOS_config_mac_loopback_mode\n");

	return ret;
}

#ifdef DWC_ETH_QOS_ENABLE_DVLAN
static INT config_tx_dvlan_processing_via_reg(
	struct DWC_ETH_QOS_prv_data *pdata, UINT flags)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;

	dev_alert(&pdata->pdev->dev, "--> config_tx_dvlan_processing_via_reg()\n");

	if (pdata->in_out & DWC_ETH_QOS_DVLAN_OUTER)
		hw_if->config_tx_outer_vlan(pdata->op_type,
					pdata->outer_vlan_tag);

	if (pdata->in_out & DWC_ETH_QOS_DVLAN_INNER)
		hw_if->config_tx_inner_vlan(pdata->op_type,
					pdata->inner_vlan_tag);

	if (flags == DWC_ETH_QOS_DVLAN_DISABLE)
		 /* restore default configurations */
		hw_if->config_mac_for_vlan_pkt();
	else
		hw_if->config_dvlan(1);

	dev_alert(&pdata->pdev->dev, "<-- config_tx_dvlan_processing_via_reg()\n");

	return Y_SUCCESS;
}

static int config_tx_dvlan_processing_via_desc(
	struct DWC_ETH_QOS_prv_data *pdata, UINT flags)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;

	dev_alert(&pdata->pdev->dev, "-->config_tx_dvlan_processing_via_desc\n");

	if (flags == DWC_ETH_QOS_DVLAN_DISABLE) {
		/* restore default configurations */
		hw_if->config_mac_for_vlan_pkt();
		pdata->via_reg_or_desc = 0;
	} else {
		hw_if->config_dvlan(1);
	}

	if (pdata->in_out & DWC_ETH_QOS_DVLAN_INNER)
		MAC_IVLANTIRR_VLTI_UDFWR(1);

	if (pdata->in_out & DWC_ETH_QOS_DVLAN_OUTER)
		MAC_VLANTIRR_VLTI_UDFWR(1);

	dev_alert(&pdata->pdev->dev, "<--config_tx_dvlan_processing_via_desc\n");

	return Y_SUCCESS;
}

/*!
 * \details This function is invoked by ioctl function when user issues
 * an ioctl command to configure mac double vlan processing feature.
 *
 * \param[in] pdata - pointer to private data structure.
 * \param[in] flags – Each bit in this variable carry some information related
 *		      double vlan processing.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int DWC_ETH_QOS_config_tx_dvlan_processing(
		struct DWC_ETH_QOS_prv_data *pdata,
		struct ifr_data_struct *req)
{
	struct DWC_ETH_QOS_config_dvlan l_config_doubule_vlan,
					  *u_config_doubule_vlan = req->ptr;
	int ret = 0;

	DBGPR("-->DWC_ETH_QOS_config_tx_dvlan_processing\n");

	if (copy_from_user(&l_config_doubule_vlan, u_config_doubule_vlan,
			   sizeof(struct DWC_ETH_QOS_config_dvlan))) {
		dev_alert(&pdata->pdev->dev, "Failed to fetch Double vlan Struct info from user\n");
		return DWC_ETH_QOS_CONFIG_FAIL;
	}

	pdata->inner_vlan_tag = l_config_doubule_vlan.inner_vlan_tag;
	pdata->outer_vlan_tag = l_config_doubule_vlan.outer_vlan_tag;
	pdata->op_type = l_config_doubule_vlan.op_type;
	pdata->in_out = l_config_doubule_vlan.in_out;
	pdata->via_reg_or_desc = l_config_doubule_vlan.via_reg_or_desc;

	if (pdata->via_reg_or_desc == DWC_ETH_QOS_VIA_REG)
		ret = config_tx_dvlan_processing_via_reg(pdata, req->flags);
	else
		ret = config_tx_dvlan_processing_via_desc(pdata, req->flags);

	DBGPR("<--DWC_ETH_QOS_config_tx_dvlan_processing\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues
 * an ioctl command to configure mac double vlan processing feature.
 *
 * \param[in] pdata - pointer to private data structure.
 * \param[in] flags – Each bit in this variable carry some information related
 *		      double vlan processing.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int DWC_ETH_QOS_config_rx_dvlan_processing(
		struct DWC_ETH_QOS_prv_data *pdata, unsigned int flags)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int ret = 0;

	DBGPR("-->DWC_ETH_QOS_config_rx_dvlan_processing\n");

	hw_if->config_dvlan(1);
	if (flags == DWC_ETH_QOS_DVLAN_NONE) {
		hw_if->config_dvlan(0);
		hw_if->config_rx_outer_vlan_stripping(
		   DWC_ETH_QOS_RX_NO_VLAN_STRIP);
		hw_if->config_rx_inner_vlan_stripping(
		   DWC_ETH_QOS_RX_NO_VLAN_STRIP);
	} else if (flags == DWC_ETH_QOS_DVLAN_INNER) {
		hw_if->config_rx_outer_vlan_stripping(
		   DWC_ETH_QOS_RX_NO_VLAN_STRIP);
		hw_if->config_rx_inner_vlan_stripping(
		   DWC_ETH_QOS_RX_VLAN_STRIP_ALWAYS);
	} else if (flags == DWC_ETH_QOS_DVLAN_OUTER) {
		hw_if->config_rx_outer_vlan_stripping(
		   DWC_ETH_QOS_RX_VLAN_STRIP_ALWAYS);
		hw_if->config_rx_inner_vlan_stripping(
		   DWC_ETH_QOS_RX_NO_VLAN_STRIP);
	} else if (flags == DWC_ETH_QOS_DVLAN_BOTH) {
		hw_if->config_rx_outer_vlan_stripping(
		   DWC_ETH_QOS_RX_VLAN_STRIP_ALWAYS);
		hw_if->config_rx_inner_vlan_stripping(
		   DWC_ETH_QOS_RX_VLAN_STRIP_ALWAYS);
	} else {
		dev_alert(&pdata->pdev->dev,
			  "ERROR : double VLAN Rx configuration - Invalid argument");
		ret = DWC_ETH_QOS_CONFIG_FAIL;
	}

	DBGPR("<--DWC_ETH_QOS_config_rx_dvlan_processing\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues
 * an ioctl command to configure mac double vlan (svlan) processing feature.
 *
 * \param[in] pdata - pointer to private data structure.
 * \param[in] flags – Each bit in this variable carry some information related
 *		      double vlan processing.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int DWC_ETH_QOS_config_svlan(struct DWC_ETH_QOS_prv_data *pdata,
				    unsigned int flags)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int ret = 0;

	DBGPR("-->DWC_ETH_QOS_config_svlan\n");

	ret = hw_if->config_svlan(flags);
	if (ret == Y_FAILURE)
		ret = DWC_ETH_QOS_CONFIG_FAIL;

	DBGPR("<--DWC_ETH_QOS_config_svlan\n");

	return ret;
}
#endif /* end of DWC_ETH_QOS_ENABLE_DVLAN */

static VOID DWC_ETH_QOS_config_timer_registers(
				struct DWC_ETH_QOS_prv_data *pdata)
{
		struct timespec now;
		struct hw_if_struct *hw_if = &pdata->hw_if;

		DBGPR("-->DWC_ETH_QOS_config_timer_registers\n");

	pdata->ptpclk_freq = DWC_ETH_QOS_DEFAULT_PTP_CLOCK;
	/* program default addend */
	hw_if->config_default_addend(pdata, DWC_ETH_QOS_DEFAULT_PTP_CLOCK);
		/* program Sub Second Increment Reg */
		hw_if->config_sub_second_increment(DWC_ETH_QOS_DEFAULT_PTP_CLOCK);
		/* initialize system time */
		getnstimeofday(&now);
		hw_if->init_systime(now.tv_sec, now.tv_nsec);

		DBGPR("-->DWC_ETH_QOS_config_timer_registers\n");
}

/*!
 * \details This function is invoked by ioctl function when user issues
 * an ioctl command to configure PTP offloading feature.
 *
 * \param[in] pdata - pointer to private data structure.
 * \param[in] flags – Each bit in this variable carry some information related
 *		      double vlan processing.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int DWC_ETH_QOS_config_ptpoffload(
		struct DWC_ETH_QOS_prv_data *pdata,
		struct DWC_ETH_QOS_config_ptpoffloading *u_conf_ptp)
{
	UINT pto_cntrl;
	UINT VARMAC_TCR;
	struct DWC_ETH_QOS_config_ptpoffloading l_conf_ptp;
	struct hw_if_struct *hw_if = &pdata->hw_if;

	if (copy_from_user(&l_conf_ptp, u_conf_ptp,
			   sizeof(struct DWC_ETH_QOS_config_ptpoffloading))) {
		dev_alert(&pdata->pdev->dev, "Failed to fetch Double vlan Struct info from user\n");
		return DWC_ETH_QOS_CONFIG_FAIL;
	}

	dev_alert(&pdata->pdev->dev, "%s - %d\n", __func__, l_conf_ptp.mode);

	pto_cntrl = MAC_PTOCR_PTOEN; /* enable ptp offloading */
	VARMAC_TCR = MAC_TCR_TSENA | MAC_TCR_TSIPENA | MAC_TCR_TSVER2ENA
			| MAC_TCR_TSCFUPDT | MAC_TCR_TSCTRLSSR;
	if (l_conf_ptp.mode == DWC_ETH_QOS_PTP_ORDINARY_SLAVE) {
		VARMAC_TCR |= MAC_TCR_TSEVENTENA;
		pdata->ptp_offloading_mode = DWC_ETH_QOS_PTP_ORDINARY_SLAVE;

	} else if (l_conf_ptp.mode == DWC_ETH_QOS_PTP_TRASPARENT_SLAVE) {
		pto_cntrl |= MAC_PTOCR_APDREQEN;
		VARMAC_TCR |= MAC_TCR_TSEVENTENA;
		VARMAC_TCR |= MAC_TCR_SNAPTYPSEL_1;
		pdata->ptp_offloading_mode =
			DWC_ETH_QOS_PTP_TRASPARENT_SLAVE;

	} else if (l_conf_ptp.mode == DWC_ETH_QOS_PTP_ORDINARY_MASTER) {
		pto_cntrl |= MAC_PTOCR_ASYNCEN;
		VARMAC_TCR |= MAC_TCR_TSEVENTENA;
		VARMAC_TCR |= MAC_TCR_TSMASTERENA;
		pdata->ptp_offloading_mode = DWC_ETH_QOS_PTP_ORDINARY_MASTER;

	} else if (l_conf_ptp.mode == DWC_ETH_QOS_PTP_TRASPARENT_MASTER) {
		pto_cntrl |= MAC_PTOCR_ASYNCEN | MAC_PTOCR_APDREQEN;
		VARMAC_TCR |= MAC_TCR_SNAPTYPSEL_1;
		VARMAC_TCR |= MAC_TCR_TSEVENTENA;
		VARMAC_TCR |= MAC_TCR_TSMASTERENA;
		pdata->ptp_offloading_mode =
			DWC_ETH_QOS_PTP_TRASPARENT_MASTER;

	} else if (l_conf_ptp.mode ==
			 DWC_ETH_QOS_PTP_PEER_TO_PEER_TRANSPARENT) {
		pto_cntrl |= MAC_PTOCR_APDREQEN;
		VARMAC_TCR |= MAC_TCR_SNAPTYPSEL_3;
		pdata->ptp_offloading_mode =
			DWC_ETH_QOS_PTP_PEER_TO_PEER_TRANSPARENT;
	}

	pdata->ptp_offload = 1;
	if (l_conf_ptp.en_dis == DWC_ETH_QOS_PTP_OFFLOADING_DISABLE) {
		pto_cntrl = 0;
		VARMAC_TCR = 0;
		pdata->ptp_offload = 0;
	}

	pto_cntrl |= (l_conf_ptp.domain_num << 8);
	hw_if->config_hw_time_stamping(VARMAC_TCR);
	DWC_ETH_QOS_config_timer_registers(pdata);
	hw_if->config_ptpoffload_engine(pto_cntrl, l_conf_ptp.mc_uc);

	dev_alert(&pdata->pdev->dev, "<--DWC_ETH_QOS_config_ptpoffload\n");

	return Y_SUCCESS;
}

/*!
 * \details This function is invoked by ioctl function when user issues
 * an ioctl command to enable/disable pfc.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] flags – flag to indicate whether pfc to be enabled/disabled.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int DWC_ETH_QOS_config_pfc(struct net_device *dev,
				  unsigned int flags)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int ret = 0;

	DBGPR("-->DWC_ETH_QOS_config_pfc\n");

	if (!pdata->hw_feat.dcb_en) {
		dev_alert(&pdata->pdev->dev, "PFC is not supported\n");
		return DWC_ETH_QOS_NO_HW_SUPPORT;
	}

	hw_if->config_pfc(flags);

	dev_alert(&pdata->pdev->dev, "Successfully %s PFC(Priority Based Flow Control)\n",
		  (flags ? "enabled" : "disabled"));

	DBGPR("<--DWC_ETH_QOS_config_pfc\n");

	return ret;
}

#ifdef CONFIG_PPS_OUTPUT
/*!
 * \brief This function confiures the PTP clock frequency.
 * \param[in] pdata : pointer to private data structure.
 * \param[in] req : pointer to ioctl structure.
 *
 * \retval 0: Success, -1 : Failure
 * */
static int ETH_PTPCLK_Config(struct DWC_ETH_QOS_prv_data *pdata, struct ifr_data_struct *req)
{
	struct ETH_PPS_Config *eth_pps_cfg = (struct ETH_PPS_Config *)req->ptr;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int ret = 0;

	if ((eth_pps_cfg->ppsout_ch < 0) ||
		(eth_pps_cfg->ppsout_ch >= pdata->hw_feat.pps_out_num))
	{
		EMACERR("PPS: PPS output channel %u is invalid \n", eth_pps_cfg->ppsout_ch);
		return  -EOPNOTSUPP;
	}

	if (eth_pps_cfg->ptpclk_freq > DWC_ETH_QOS_SYSCLOCK){
		EMACDBG("PPS: PTPCLK_Config: freq=%dHz is too high. Cannot config it\n",
			eth_pps_cfg->ptpclk_freq );
		return -1;
	}

	pdata->ptpclk_freq = eth_pps_cfg->ptpclk_freq;
	ret = hw_if->config_default_addend(pdata, (ULONG)eth_pps_cfg->ptpclk_freq);
	ret |= hw_if->config_sub_second_increment( (ULONG)eth_pps_cfg->ptpclk_freq);

	return ret;
}

irqreturn_t DWC_ETH_QOS_PPS_avb_class_a(int irq, void *dev_id)
{
	struct DWC_ETH_QOS_prv_data *pdata =
		(struct DWC_ETH_QOS_prv_data *)dev_id;

	pdata->avb_class_a_intr_cnt++;
	avb_class_a_msg_wq_flag = 1;
	wake_up_interruptible(&avb_class_a_msg_wq);
	return IRQ_HANDLED;
}
irqreturn_t DWC_ETH_QOS_PPS_avb_class_b(int irq, void *dev_id)
{
	struct DWC_ETH_QOS_prv_data *pdata =
		(struct DWC_ETH_QOS_prv_data *)dev_id;

	pdata->avb_class_b_intr_cnt++;
	avb_class_b_msg_wq_flag = 1;
	wake_up_interruptible(&avb_class_b_msg_wq);
	return IRQ_HANDLED;
}


void Register_PPS_ISR(struct DWC_ETH_QOS_prv_data *pdata, int ch)
{
	int ret;

	if (ch == DWC_ETH_QOS_PPS_CH_2 ) {
		ret = request_irq(pdata->res_data->ptp_pps_avb_class_a_irq, DWC_ETH_QOS_PPS_avb_class_a,
						IRQF_TRIGGER_RISING, DEV_NAME, pdata);
		if (ret) {
			EMACERR("Req ptp_pps_avb_class_a_irq Failed ret=%d\n",ret);
		} else {
			EMACERR("Req ptp_pps_avb_class_a_irq pass \n");
		}
	} else if (ch == DWC_ETH_QOS_PPS_CH_3) {
		ret = request_irq(pdata->res_data->ptp_pps_avb_class_b_irq, DWC_ETH_QOS_PPS_avb_class_b,
						IRQF_TRIGGER_RISING, DEV_NAME, pdata);
		if (ret) {
			EMACERR("Req ptp_pps_avb_class_b_irq Failed ret=%d\n",ret);
		} else {
			EMACERR("Req ptp_pps_avb_class_b_irq pass \n");
		}
	} else
		EMACERR("Invalid channel %d\n", ch);
}

void Unregister_PPS_ISR(struct DWC_ETH_QOS_prv_data *pdata, int ch)
{
	if (ch == DWC_ETH_QOS_PPS_CH_2) {
		if (pdata->res_data->ptp_pps_avb_class_a_irq != 0) {
			free_irq(pdata->res_data->ptp_pps_avb_class_a_irq, pdata);
		}
	} else if (ch == DWC_ETH_QOS_PPS_CH_3) {
		if (pdata->res_data->ptp_pps_avb_class_b_irq != 0) {
			free_irq(pdata->res_data->ptp_pps_avb_class_b_irq, pdata);
		}
	} else
		EMACERR("Invalid channel %d\n", ch);
}

static void configure_target_time_reg(u32 ch)
{
	u32 data = 0x0;

	MAC_PPS_TTNS_RGWR(ch,0x0);
	MAC_PPS_TTNS_TTSL0_UDFWR(ch, DEFAULT_START_TIME);
	do {
		MAC_PPS_TTNS_TRGTBUSY0_UDFRD(ch,data);
	} while (data == 0x1); // Wait until bit is clear
}

void DWC_ETH_QOS_pps_timer_init(struct ifr_data_struct *req)
{
	u32 data = 0x0;

	/* Enable timestamping. This is required to start system time generator.*/
	MAC_TCR_TSENA_UDFWR(0x1);
	MAC_TCR_TSUPDT_UDFWR(0x1);
	MAC_TCR_TSCFUPDT_UDFWR(0x1); // Fine Timestamp Update method.

	/* Initialize MAC System Time Update register */
	MAC_STSUR_TSS_UDFWR(0x0); // MAC system time in seconds

	MAC_STNSUR_TSSS_UDFWR(0x0); // The time value is added in sub seconds with the contents of the update register.
	MAC_STNSUR_ADDSUB_UDFWR(0x0); // The time value is added in seconds with the contents of the update register.

	/* Initialize system timestamp by setting TSINIT bit. */
	MAC_TCR_TSINIT_UDFWR(0x1);
	do {
		MAC_TCR_TSINIT_UDFWR(data);
	} while (data == 0x1); // Wait until TSINIT is clear

}

void stop_pps(int ch)
{
	u32 pps_mode_select = 3;

	if (ch == DWC_ETH_QOS_PPS_CH_0) {
		EMACDBG("stop pps with channel %d\n", ch);
		MAC_PPSC_PPSEN0_UDFWR(0x1);
		MAC_PPSC_TRGTMODSEL0_UDFWR(pps_mode_select);
		MAC_PPSC_PPSCTRL0_UDFWR(0x5);
	} else if (ch == DWC_ETH_QOS_PPS_CH_1) {
		EMACDBG("stop pps with channel %d\n", ch);
		MAC_PPSC_PPSEN0_UDFWR(0x1);
		MAC_PPSC_TRGTMODSEL1_UDFWR(pps_mode_select);
		MAC_PPSC_PPSCMD1_UDFWR(0x5);
	} else if (ch == DWC_ETH_QOS_PPS_CH_2) {
		EMACDBG("stop pps with channel %d\n", ch);
		MAC_PPSC_PPSEN0_UDFWR(0x1);
		MAC_PPSC_TRGTMODSEL2_UDFWR(pps_mode_select);
		MAC_PPSC_PPSCMD2_UDFWR(0x5);
	} else if (ch == DWC_ETH_QOS_PPS_CH_3) {
		EMACDBG("stop pps with channel %d\n", ch);
		MAC_PPSC_PPSEN0_UDFWR(0x1);
		MAC_PPSC_TRGTMODSEL3_UDFWR(pps_mode_select);
		MAC_PPSC_PPSCMD3_UDFWR(0x5);
	} else {
		EMACERR("Invalid channel %d\n", ch);
	}

}


/*!
 * \brief This function confiures the PPS output.
 * \param[in] pdata : pointer to private data structure.
 * \param[in] req : pointer to ioctl structure.
 *
 * \retval 0: Success, -1 : Failure
 * */
int ETH_PPSOUT_Config(struct DWC_ETH_QOS_prv_data *pdata, struct ifr_data_struct *req)
{
	struct ETH_PPS_Config *eth_pps_cfg = (struct ETH_PPS_Config *)req->ptr;
	unsigned int val;
	int interval, width;
	struct hw_if_struct *hw_if = &pdata->hw_if;

	/* For lpass we need 19.2Mhz PPS frequency for PPS0.
	   If lpass is enabled don't allow to change the PTP clock
	   becuase if we change PTP clock then addend & subsecond increament
	   will change & We will not see 19.2Mhz for PPS0.
	*/
	if (pdata->res_data->pps_lpass_conn_en ) {
		eth_pps_cfg->ptpclk_freq = DWC_ETH_QOS_DEFAULT_PTP_CLOCK;
		EMACDBG("using default ptp clock \n");
	}

	if ((eth_pps_cfg->ppsout_ch < 0) ||
		(eth_pps_cfg->ppsout_ch >= pdata->hw_feat.pps_out_num))
	{
		EMACERR("PPS: PPS output channel %u is invalid \n", eth_pps_cfg->ppsout_ch);
		return  -EOPNOTSUPP;
	}

	if(eth_pps_cfg->ppsout_duty <= 0) {
		printk("PPS: PPSOut_Config: duty cycle is invalid. Using duty=1\n");
		eth_pps_cfg->ppsout_duty = 1;
	}
	else if(eth_pps_cfg->ppsout_duty >= 100) {
		printk("PPS: PPSOut_Config: duty cycle is invalid. Using duty=99\n");
		eth_pps_cfg->ppsout_duty = 99;
	}

	/* Configure increment values */
	hw_if->config_sub_second_increment(eth_pps_cfg->ptpclk_freq);

	/* Configure addent value as Fine Timestamp method is used */
	hw_if->config_default_addend(pdata, eth_pps_cfg->ptpclk_freq);

	if(0 < eth_pps_cfg->ptpclk_freq) {
		pdata->ptpclk_freq = eth_pps_cfg->ptpclk_freq;
		interval = (eth_pps_cfg->ptpclk_freq + eth_pps_cfg->ppsout_freq/2)
											/ eth_pps_cfg->ppsout_freq;
	} else {
		interval = (pdata->ptpclk_freq + eth_pps_cfg->ppsout_freq/2)
											/ eth_pps_cfg->ppsout_freq;
	}
	width = ((interval * eth_pps_cfg->ppsout_duty) + 50)/100 - 1;
	if (width >= interval) width = interval - 1;
	if (width < 0) width = 0;

	EMACDBG("PPS: PPSOut_Config: freq=%dHz, ch=%d, duty=%d\n",
				eth_pps_cfg->ppsout_freq,
				eth_pps_cfg->ppsout_ch,
				eth_pps_cfg->ppsout_duty);
	EMACDBG(" PPS: with PTP Clock freq=%dHz\n", pdata->ptpclk_freq);

	EMACDBG("PPS: PPSOut_Config: interval=%d, width=%d\n", interval, width);

	switch (eth_pps_cfg->ppsout_ch) {
	case DWC_ETH_QOS_PPS_CH_0:
		if (pdata->res_data->pps_lpass_conn_en) {
			if (eth_pps_cfg->ppsout_start == DWC_ETH_QOS_PPS_START) {
				MAC_PPSC_PPSEN0_UDFWR(0x1);
				MAC_PPS_INTVAL_PPSINT0_UDFWR(DWC_ETH_QOS_PPS_CH_0, interval);
				MAC_PPS_WIDTH_PPSWIDTH0_UDFWR(DWC_ETH_QOS_PPS_CH_0, width);
				configure_target_time_reg(DWC_ETH_QOS_PPS_CH_0);
				MAC_PPSC_TRGTMODSEL0_UDFWR(0x2);
				MAC_PPSC_PPSCTRL0_UDFWR(0x2);
			} else if (eth_pps_cfg->ppsout_start == DWC_ETH_QOS_PPS_STOP) {
				EMACDBG("STOP pps for channel 0\n");
				stop_pps(DWC_ETH_QOS_PPS_CH_0);
			}
		} else{
			MAC_PPS_INTVAL0_PPSINT0_UDFWR(interval);  // interval
			MAC_PPS_WIDTH0_PPSWIDTH0_UDFWR(width);
			MAC_STSR_TSS_UDFRD(val);     //PTP seconds      start time value in target resister
			MAC_PPS_TTS_TSTRH0_UDFWR(0, val+1);
			MAC_PPSC_PPSCTRL0_UDFWR(0x2);   //ppscmd
			MAC_PPSC_PPSEN0_UDFWR(0x1);
		}
		break;

	case DWC_ETH_QOS_PPS_CH_1:
		if (eth_pps_cfg->ppsout_start == DWC_ETH_QOS_PPS_START) {
			MAC_PPS_INTVAL_PPSINT0_UDFWR(DWC_ETH_QOS_PPS_CH_1, interval);
			MAC_PPS_WIDTH_PPSWIDTH0_UDFWR(DWC_ETH_QOS_PPS_CH_1, width);
			configure_target_time_reg(DWC_ETH_QOS_PPS_CH_1);
			MAC_PPSC_TRGTMODSEL1_UDFWR(0x2);
			MAC_PPSC_PPSEN0_UDFWR(0x1);
			MAC_PPSC_PPSCMD1_UDFWR(0x2);
		}
		else if (eth_pps_cfg->ppsout_start == DWC_ETH_QOS_PPS_STOP) {
			EMACDBG("STOP pps for channel 1\n");
			stop_pps(DWC_ETH_QOS_PPS_CH_1);
		}
		break;

	case DWC_ETH_QOS_PPS_CH_2:
		if (eth_pps_cfg->ppsout_start == DWC_ETH_QOS_PPS_START) {

			Register_PPS_ISR(pdata, DWC_ETH_QOS_PPS_CH_2);
			MAC_PPS_INTVAL_PPSINT0_UDFWR(DWC_ETH_QOS_PPS_CH_2, interval);
			MAC_PPS_WIDTH_PPSWIDTH0_UDFWR(DWC_ETH_QOS_PPS_CH_2, width);
			configure_target_time_reg(DWC_ETH_QOS_PPS_CH_2);
			MAC_PPSC_TRGTMODSEL2_UDFWR(0x2);
			MAC_PPSC_PPSEN0_UDFWR(0x1);
			MAC_PPSC_PPSCMD2_UDFWR(0x2);
		}
		else if (eth_pps_cfg->ppsout_start == DWC_ETH_QOS_PPS_STOP) {
			Unregister_PPS_ISR(pdata, DWC_ETH_QOS_PPS_CH_2);
			EMACDBG("STOP pps for channel 2\n");
			stop_pps(DWC_ETH_QOS_PPS_CH_2);
		}
		break;
	case DWC_ETH_QOS_PPS_CH_3:
		if (eth_pps_cfg->ppsout_start == DWC_ETH_QOS_PPS_START) {
			Register_PPS_ISR(pdata, DWC_ETH_QOS_PPS_CH_3);
			EMACDBG("start pps for chanel 3\n");
			MAC_PPS_INTVAL_PPSINT0_UDFWR(DWC_ETH_QOS_PPS_CH_3, interval);
			MAC_PPS_WIDTH_PPSWIDTH0_UDFWR(DWC_ETH_QOS_PPS_CH_3, width);
			configure_target_time_reg(DWC_ETH_QOS_PPS_CH_3);
			MAC_PPSC_TRGTMODSEL3_UDFWR(0x2);
			MAC_PPSC_PPSEN0_UDFWR(0x1);
			MAC_PPSC_PPSCMD3_UDFWR(0x2);
		} else if (eth_pps_cfg->ppsout_start == DWC_ETH_QOS_PPS_STOP) {
			Unregister_PPS_ISR(pdata, DWC_ETH_QOS_PPS_CH_3);
			EMACDBG("STOP pps for channel 3\n");
			stop_pps(DWC_ETH_QOS_PPS_CH_3);
		}
		break;
	default:
		EMACDBG("PPS: PPS output channel is invalid (only CH0/CH1/CH2/CH3 is supported).\n");
		return -EOPNOTSUPP;
	}

	return 0;
}
#endif

/*!
 * \brief Driver IOCTL routine
 *
 * \details This function is invoked by main ioctl function when
 * users request to configure various device features like,
 * PMT module, TX and RX PBL, TX and RX FIFO threshold level,
 * TX and RX OSF mode, SA insert/replacement, L2/L3/L4 and
 * VLAN filtering, AVB/DCB algorithm etc.
 *
 * \param[in] pdata – pointer to private data structure.
 * \param[in] req – pointer to ioctl structure.
 *
 * \return int
 *
 * \retval 0 - success
 * \retval negative - failure
 */

static int DWC_ETH_QOS_handle_prv_ioctl(struct DWC_ETH_QOS_prv_data *pdata,
					struct ifr_data_struct *req)
{
	unsigned int qinx = req->qinx;
	struct DWC_ETH_QOS_tx_wrapper_descriptor *tx_desc_data =
	    GET_TX_WRAPPER_DESC(qinx);
	struct DWC_ETH_QOS_rx_wrapper_descriptor *rx_desc_data =
	    GET_RX_WRAPPER_DESC(qinx);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct net_device *dev = pdata->dev;
	int ret = 0;
#ifdef CONFIG_PPS_OUTPUT
	struct ETH_PPS_Config eth_pps_cfg;
#endif

	DBGPR("-->DWC_ETH_QOS_handle_prv_ioctl\n");

	if (qinx > DWC_ETH_QOS_QUEUE_CNT) {
		dev_alert(&pdata->pdev->dev,
			  "Queue number %d is invalid\n", qinx);
		dev_alert(&pdata->pdev->dev,
			  "Hardware has only %d Tx/Rx Queues\n",
			DWC_ETH_QOS_QUEUE_CNT);
		ret = DWC_ETH_QOS_NO_HW_SUPPORT;
		return ret;
	}

	switch (req->cmd) {
	case DWC_ETH_QOS_POWERUP_MAGIC_CMD:
		if (pdata->hw_feat.mgk_sel) {
			ret =
			DWC_ETH_QOS_powerup(dev, DWC_ETH_QOS_IOCTL_CONTEXT);
			if (ret == 0)
				ret = DWC_ETH_QOS_CONFIG_SUCCESS;
			else
				ret = DWC_ETH_QOS_CONFIG_FAIL;
		} else {
			ret = DWC_ETH_QOS_NO_HW_SUPPORT;
		}
		break;

	case DWC_ETH_QOS_POWERDOWN_MAGIC_CMD:
		if (pdata->hw_feat.mgk_sel) {
			ret =
			  DWC_ETH_QOS_powerdown(dev,
						DWC_ETH_QOS_MAGIC_WAKEUP, DWC_ETH_QOS_IOCTL_CONTEXT);
			if (ret == 0)
				ret = DWC_ETH_QOS_CONFIG_SUCCESS;
			else
				ret = DWC_ETH_QOS_CONFIG_FAIL;
		} else {
			ret = DWC_ETH_QOS_NO_HW_SUPPORT;
		}
		break;

	case DWC_ETH_QOS_POWERUP_REMOTE_WAKEUP_CMD:
		if (pdata->hw_feat.rwk_sel) {
			ret =
			DWC_ETH_QOS_powerup(dev, DWC_ETH_QOS_IOCTL_CONTEXT);
			if (ret == 0)
				ret = DWC_ETH_QOS_CONFIG_SUCCESS;
			else
				ret = DWC_ETH_QOS_CONFIG_FAIL;
		} else {
			ret = DWC_ETH_QOS_NO_HW_SUPPORT;
		}
		break;

	case DWC_ETH_QOS_POWERDOWN_REMOTE_WAKEUP_CMD:
		if (pdata->hw_feat.rwk_sel) {
			ret = DWC_ETH_QOS_configure_remotewakeup(dev, req);
			if (ret == 0)
				ret = DWC_ETH_QOS_CONFIG_SUCCESS;
			else
				ret = DWC_ETH_QOS_CONFIG_FAIL;
		} else {
			ret = DWC_ETH_QOS_NO_HW_SUPPORT;
		}
		break;

	case DWC_ETH_QOS_RX_THRESHOLD_CMD:
		rx_desc_data->rx_threshold_val = req->flags;
		hw_if->config_rx_threshold(qinx,
					rx_desc_data->rx_threshold_val);
		dev_alert(&pdata->pdev->dev, "Configured Rx threshold with %d\n",
			  rx_desc_data->rx_threshold_val);
		break;

	case DWC_ETH_QOS_TX_THRESHOLD_CMD:
		tx_desc_data->tx_threshold_val = req->flags;
		hw_if->config_tx_threshold(qinx,
					tx_desc_data->tx_threshold_val);
		dev_alert(&pdata->pdev->dev, "Configured Tx threshold with %d\n",
			  tx_desc_data->tx_threshold_val);
		break;

	case DWC_ETH_QOS_RSF_CMD:
		rx_desc_data->rsf_on = req->flags;
		hw_if->config_rsf_mode(qinx, rx_desc_data->rsf_on);
		dev_alert(&pdata->pdev->dev, "Receive store and forward mode %s\n",
			  (rx_desc_data->rsf_on) ? "enabled" : "disabled");
		break;

	case DWC_ETH_QOS_TSF_CMD:
		tx_desc_data->tsf_on = req->flags;
		hw_if->config_tsf_mode(qinx, tx_desc_data->tsf_on);
		dev_alert(&pdata->pdev->dev, "Transmit store and forward mode %s\n",
			  (tx_desc_data->tsf_on) ? "enabled" : "disabled");
		break;

	case DWC_ETH_QOS_OSF_CMD:
		tx_desc_data->osf_on = req->flags;
		hw_if->config_osf_mode(qinx, tx_desc_data->osf_on);
		dev_alert(&pdata->pdev->dev, "Transmit DMA OSF mode is %s\n",
			  (tx_desc_data->osf_on) ? "enabled" : "disabled");
		break;

	case DWC_ETH_QOS_INCR_INCRX_CMD:
		pdata->incr_incrx = req->flags;
		hw_if->config_incr_incrx_mode(pdata->incr_incrx);
		dev_alert(&pdata->pdev->dev, "%s mode is enabled\n",
			  (pdata->incr_incrx) ? "INCRX" : "INCR");
		break;

	case DWC_ETH_QOS_RX_PBL_CMD:
		rx_desc_data->rx_pbl = req->flags;
		DWC_ETH_QOS_config_rx_pbl(pdata, rx_desc_data->rx_pbl, qinx);
		break;

	case DWC_ETH_QOS_TX_PBL_CMD:
		tx_desc_data->tx_pbl = req->flags;
		DWC_ETH_QOS_config_tx_pbl(pdata, tx_desc_data->tx_pbl, qinx);
		break;

#ifdef DWC_ETH_QOS_ENABLE_DVLAN
	case DWC_ETH_QOS_DVLAN_TX_PROCESSING_CMD:
		if (pdata->hw_feat.sa_vlan_ins) {
			ret =
			DWC_ETH_QOS_config_tx_dvlan_processing(pdata, req);
		} else {
			dev_alert(&pdata->pdev->dev, "No HW support for Single/Double VLAN\n");
			ret = DWC_ETH_QOS_NO_HW_SUPPORT;
		}
		break;
	case DWC_ETH_QOS_DVLAN_RX_PROCESSING_CMD:
		if (pdata->hw_feat.sa_vlan_ins) {
			ret =
			DWC_ETH_QOS_config_rx_dvlan_processing(
			   pdata, req->flags);
		} else {
			dev_alert(&pdata->pdev->dev, "No HW support for Single/Double VLAN\n");
			ret = DWC_ETH_QOS_NO_HW_SUPPORT;
		}
		break;
	case DWC_ETH_QOS_SVLAN_CMD:
		if (pdata->hw_feat.sa_vlan_ins) {
			ret = DWC_ETH_QOS_config_svlan(pdata, req->flags);
		} else {
			dev_alert(&pdata->pdev->dev, "No HW support for Single/Double VLAN\n");
			ret = DWC_ETH_QOS_NO_HW_SUPPORT;
		}
		break;
#endif /* end of DWC_ETH_QOS_ENABLE_DVLAN */
	case DWC_ETH_QOS_PTPOFFLOADING_CMD:
		if (pdata->hw_feat.tsstssel) {
			ret = DWC_ETH_QOS_config_ptpoffload(pdata,
							    req->ptr);
		} else {
			dev_alert(&pdata->pdev->dev, "No HW support for PTP\n");
			ret = DWC_ETH_QOS_NO_HW_SUPPORT;
		}
		break;

	case DWC_ETH_QOS_SA0_DESC_CMD:
		if (pdata->hw_feat.sa_vlan_ins) {
			pdata->tx_sa_ctrl_via_desc = req->flags;
			pdata->tx_sa_ctrl_via_reg = DWC_ETH_QOS_SA0_NONE;
			if (req->flags == DWC_ETH_QOS_SA0_NONE) {
				memcpy(pdata->mac_addr, pdata->dev->dev_addr,
				       DWC_ETH_QOS_MAC_ADDR_LEN);
			} else {
				memcpy(pdata->mac_addr, mac_addr0,
				       DWC_ETH_QOS_MAC_ADDR_LEN);
			}
			hw_if->configure_mac_addr0_reg(pdata->mac_addr);
			hw_if->configure_sa_via_reg(pdata->tx_sa_ctrl_via_reg);
			dev_alert(&pdata->pdev->dev,
				  "SA will use MAC0 with descriptor for configuration %d\n",
			       pdata->tx_sa_ctrl_via_desc);
		} else {
			dev_alert(&pdata->pdev->dev,
				  "Device doesn't supports SA Insertion/Replacement\n");
			ret = DWC_ETH_QOS_NO_HW_SUPPORT;
		}
		break;

	case DWC_ETH_QOS_SA1_DESC_CMD:
		if (pdata->hw_feat.sa_vlan_ins) {
			pdata->tx_sa_ctrl_via_desc = req->flags;
			pdata->tx_sa_ctrl_via_reg = DWC_ETH_QOS_SA1_NONE;
			if (req->flags == DWC_ETH_QOS_SA1_NONE) {
				memcpy(pdata->mac_addr, pdata->dev->dev_addr,
				       DWC_ETH_QOS_MAC_ADDR_LEN);
			} else {
				memcpy(pdata->mac_addr, mac_addr1,
				       DWC_ETH_QOS_MAC_ADDR_LEN);
			}
			hw_if->configure_mac_addr1_reg(pdata->mac_addr);
			hw_if->configure_sa_via_reg(pdata->tx_sa_ctrl_via_reg);
			dev_alert(&pdata->pdev->dev,
				  "SA will use MAC1 with descriptor for configuration %d\n",
			       pdata->tx_sa_ctrl_via_desc);
		} else {
			dev_alert(&pdata->pdev->dev,
				  "Device doesn't supports SA Insertion/Replacement\n");
			ret = DWC_ETH_QOS_NO_HW_SUPPORT;
		}
		break;

	case DWC_ETH_QOS_SA0_REG_CMD:
		if (pdata->hw_feat.sa_vlan_ins) {
			pdata->tx_sa_ctrl_via_reg = req->flags;
			pdata->tx_sa_ctrl_via_desc = DWC_ETH_QOS_SA0_NONE;
			if (req->flags == DWC_ETH_QOS_SA0_NONE) {
				memcpy(pdata->mac_addr, pdata->dev->dev_addr,
				       DWC_ETH_QOS_MAC_ADDR_LEN);
			} else {
				memcpy(pdata->mac_addr, mac_addr0,
				       DWC_ETH_QOS_MAC_ADDR_LEN);
			}
			hw_if->configure_mac_addr0_reg(pdata->mac_addr);
			hw_if->configure_sa_via_reg(pdata->tx_sa_ctrl_via_reg);
			dev_alert(&pdata->pdev->dev,
				  "SA will use MAC0 with register for configuration %d\n",
			       pdata->tx_sa_ctrl_via_desc);
		} else {
			dev_alert(&pdata->pdev->dev,
				  "Device doesn't supports SA Insertion/Replacement\n");
			ret = DWC_ETH_QOS_NO_HW_SUPPORT;
		}
		break;

	case DWC_ETH_QOS_SA1_REG_CMD:
		if (pdata->hw_feat.sa_vlan_ins) {
			pdata->tx_sa_ctrl_via_reg = req->flags;
			pdata->tx_sa_ctrl_via_desc = DWC_ETH_QOS_SA1_NONE;
			if (req->flags == DWC_ETH_QOS_SA1_NONE) {
				memcpy(pdata->mac_addr, pdata->dev->dev_addr,
				       DWC_ETH_QOS_MAC_ADDR_LEN);
			} else {
				memcpy(pdata->mac_addr, mac_addr1,
				       DWC_ETH_QOS_MAC_ADDR_LEN);
			}
			hw_if->configure_mac_addr1_reg(pdata->mac_addr);
			hw_if->configure_sa_via_reg(pdata->tx_sa_ctrl_via_reg);
			dev_alert(&pdata->pdev->dev,
				  "SA will use MAC1 with register for configuration %d\n",
			       pdata->tx_sa_ctrl_via_desc);
		} else {
			dev_alert(&pdata->pdev->dev,
				  "Device doesn't supports SA Insertion/Replacement\n");
			ret = DWC_ETH_QOS_NO_HW_SUPPORT;
		}
		break;

	case DWC_ETH_QOS_SETUP_CONTEXT_DESCRIPTOR:
		if (pdata->hw_feat.sa_vlan_ins) {
			tx_desc_data->context_setup = req->context_setup;
			if (tx_desc_data->context_setup == 1) {
				dev_alert(&pdata->pdev->dev,
					  "Context descriptor will be transmitted\n");
				dev_alert(&pdata->pdev->dev,
					  " with every normal descriptor on %d DMA Channel\n",
					qinx);
			} else {
				dev_alert(&pdata->pdev->dev,
					  "Ctx desc will be setup only if VLAN id changes %d\n",
						qinx);
			}
		} else {
			dev_alert(&pdata->pdev->dev,
				  "Device doesn't support VLAN operations\n");
			ret = DWC_ETH_QOS_NO_HW_SUPPORT;
		}
		break;

	case DWC_ETH_QOS_GET_RX_QCNT:
		req->qinx = DWC_ETH_QOS_RX_QUEUE_CNT;
		break;

	case DWC_ETH_QOS_GET_TX_QCNT:
		req->qinx = DWC_ETH_QOS_TX_QUEUE_CNT;
		break;

	case DWC_ETH_QOS_GET_CONNECTED_SPEED:
		req->connected_speed = pdata->speed;
		break;

	case DWC_ETH_QOS_DCB_ALGORITHM:
		DWC_ETH_QOS_program_dcb_algorithm(pdata, req);
		break;

	case DWC_ETH_QOS_AVB_ALGORITHM:
		DWC_ETH_QOS_program_avb_algorithm(pdata, req);
		break;

	case DWC_ETH_QOS_RX_SPLIT_HDR_CMD:
		if (pdata->hw_feat.sph_en) {
			ret =
			DWC_ETH_QOS_config_rx_split_hdr_mode(dev, req->flags);
			if (ret == 0)
				ret = DWC_ETH_QOS_CONFIG_SUCCESS;
			else
				ret = DWC_ETH_QOS_CONFIG_FAIL;
		} else {
			ret = DWC_ETH_QOS_NO_HW_SUPPORT;
		}
		break;
	case DWC_ETH_QOS_L3_L4_FILTER_CMD:
		if (pdata->hw_feat.l3l4_filter_num > 0) {
			ret =
			DWC_ETH_QOS_config_l3_l4_filtering(dev, req->flags);
			if (ret == 0)
				ret = DWC_ETH_QOS_CONFIG_SUCCESS;
			else
				ret = DWC_ETH_QOS_CONFIG_FAIL;
		} else {
			ret = DWC_ETH_QOS_NO_HW_SUPPORT;
		}
		break;
	case DWC_ETH_QOS_IPV4_FILTERING_CMD:
		ret = DWC_ETH_QOS_config_ip4_filters(dev, req);
		break;
	case DWC_ETH_QOS_IPV6_FILTERING_CMD:
		ret = DWC_ETH_QOS_config_ip6_filters(dev, req);
		break;
	case DWC_ETH_QOS_UDP_FILTERING_CMD:
		ret = DWC_ETH_QOS_config_tcp_udp_filters(dev, req, 1);
		break;
	case DWC_ETH_QOS_TCP_FILTERING_CMD:
		ret = DWC_ETH_QOS_config_tcp_udp_filters(dev, req, 0);
		break;
	case DWC_ETH_QOS_VLAN_FILTERING_CMD:
		ret = DWC_ETH_QOS_config_vlan_filter(dev, req);
		break;
	case DWC_ETH_QOS_L2_DA_FILTERING_CMD:
		ret = DWC_ETH_QOS_confing_l2_da_filter(dev, req);
		break;
	case DWC_ETH_QOS_ARP_OFFLOAD_CMD:
		ret = DWC_ETH_QOS_config_arp_offload(dev, req);
		break;
	case DWC_ETH_QOS_AXI_PBL_CMD:
		pdata->axi_pbl = req->flags;
		hw_if->config_axi_pbl_val(pdata->axi_pbl);
		dev_alert(&pdata->pdev->dev,
			  "AXI PBL value: %d\n", pdata->axi_pbl);
		break;
	case DWC_ETH_QOS_AXI_WORL_CMD:
		pdata->axi_worl = req->flags;
		hw_if->config_axi_worl_val(pdata->axi_worl);
		dev_alert(&pdata->pdev->dev,
			  "AXI WORL value: %d\n", pdata->axi_worl);
		break;
	case DWC_ETH_QOS_AXI_RORL_CMD:
		pdata->axi_rorl = req->flags;
		hw_if->config_axi_rorl_val(pdata->axi_rorl);
		dev_alert(&pdata->pdev->dev,
			  "AXI RORL value: %d\n", pdata->axi_rorl);
		break;
	case DWC_ETH_QOS_MAC_LOOPBACK_MODE_CMD:
		ret = DWC_ETH_QOS_config_mac_loopback_mode(dev, req->flags);
		if (ret == 0)
			ret = DWC_ETH_QOS_CONFIG_SUCCESS;
		else
			ret = DWC_ETH_QOS_CONFIG_FAIL;
		break;
	case DWC_ETH_QOS_PFC_CMD:
		ret = DWC_ETH_QOS_config_pfc(dev, req->flags);
		break;
#ifdef DWC_ETH_QOS_CONFIG_PGTEST
	case DWC_ETH_QOS_PG_TEST:
		ret = DWC_ETH_QOS_handle_pg_ioctl(pdata, (void *)req);
		break;
#endif /* end of DWC_ETH_QOS_CONFIG_PGTEST */

#ifdef CONFIG_PPS_OUTPUT
	case DWC_ETH_QOS_CONFIG_PTPCLK_CMD:

		if (copy_from_user(&eth_pps_cfg, req->ptr,
			sizeof(struct ETH_PPS_Config))) {
			return -EFAULT;
		}
		req->ptr = &eth_pps_cfg;

		if(pdata->hw_feat.pps_out_num == 0)
			ret = -EOPNOTSUPP;
		else
			ret = ETH_PTPCLK_Config(pdata, req);
		break;

	case DWC_ETH_QOS_CONFIG_PPSOUT_CMD:

		if (copy_from_user(&eth_pps_cfg, req->ptr,
			sizeof(struct ETH_PPS_Config))) {
			return -EFAULT;
		}
		req->ptr = &eth_pps_cfg;

		if(pdata->hw_feat.pps_out_num == 0)
			ret = -EOPNOTSUPP;
		else
			ret = ETH_PPSOUT_Config(pdata, req);
		break;
#endif

	default:
		ret = -EOPNOTSUPP;
		dev_alert(&pdata->pdev->dev, "Unsupported command call\n");
	}

	DBGPR("<--DWC_ETH_QOS_handle_prv_ioctl\n");

	return ret;
}

/*!
 * \brief control hw timestamping.
 *
 * \details This function is used to configure the MAC to enable/disable both
 * outgoing(Tx) and incoming(Rx) packets time stamping based on user input.
 *
 * \param[in] pdata – pointer to private data structure.
 * \param[in] ifr – pointer to IOCTL specific structure.
 *
 * \return int
 *
 * \retval 0 - success
 * \retval negative - failure
 */

static int DWC_ETH_QOS_handle_hwtstamp_ioctl(struct DWC_ETH_QOS_prv_data *pdata,
					     struct ifreq *ifr)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct hwtstamp_config config;
	u32 ptp_v2 = 0;
	u32 tstamp_all = 0;
	u32 ptp_over_ipv4_udp = 0;
	u32 ptp_over_ipv6_udp = 0;
	u32 ptp_over_ethernet = 0;
	u32 snap_type_sel = 0;
	u32 ts_master_en = 0;
	u32 ts_event_en = 0;
	u32 av_8021asm_en = 0;
	u32 VARMAC_TCR = 0;
	struct timespec now;

	DBGPR_PTP("-->DWC_ETH_QOS_handle_hwtstamp_ioctl\n");

	if (!pdata->hw_feat.tsstssel) {
		dev_alert(&pdata->pdev->dev, "No hw timestamping is available in this core\n");
		return -EOPNOTSUPP;
	}

	if (copy_from_user(&config, ifr->ifr_data,
			   sizeof(struct hwtstamp_config)))
		return -EFAULT;

	DBGPR_PTP("config.flags = %#x, tx_type = %#x, rx_filter = %#x\n",
		  config.flags, config.tx_type, config.rx_filter);

	/* reserved for future extensions */
	if (config.flags)
		return -EINVAL;

	switch (config.tx_type) {
	case HWTSTAMP_TX_OFF:
		pdata->hwts_tx_en = 0;
		break;
	case HWTSTAMP_TX_ON:
		pdata->hwts_tx_en = 1;
		break;
	default:
		return -ERANGE;
	}

	switch (config.rx_filter) {
	/* time stamp no incoming packet at all */
	case HWTSTAMP_FILTER_NONE:
		config.rx_filter = HWTSTAMP_FILTER_NONE;
		break;

	/* PTP v1, UDP, any kind of event packet */
	case HWTSTAMP_FILTER_PTP_V1_L4_EVENT:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_EVENT;
		/* take time stamp for all event messages */
		snap_type_sel = MAC_TCR_SNAPTYPSEL_1;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		break;

	/* PTP v1, UDP, Sync packet */
	case HWTSTAMP_FILTER_PTP_V1_L4_SYNC:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_SYNC;
		/* take time stamp for SYNC messages only */
		ts_event_en = MAC_TCR_TSEVENTENA;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		break;

	/* PTP v1, UDP, Delay_req packet */
	case HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ;
		/* take time stamp for Delay_Req messages only */
		ts_master_en = MAC_TCR_TSMASTERENA;
		ts_event_en = MAC_TCR_TSEVENTENA;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		break;

	/* PTP v2, UDP, any kind of event packet */
	case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_EVENT;
		ptp_v2 = MAC_TCR_TSVER2ENA;
		/* take time stamp for all event messages */
		snap_type_sel = MAC_TCR_SNAPTYPSEL_1;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		break;

	/* PTP v2, UDP, Sync packet */
	case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_SYNC;
		ptp_v2 = MAC_TCR_TSVER2ENA;
		/* take time stamp for SYNC messages only */
		ts_event_en = MAC_TCR_TSEVENTENA;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		break;

	/* PTP v2, UDP, Delay_req packet */
	case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ;
		ptp_v2 = MAC_TCR_TSVER2ENA;
		/* take time stamp for Delay_Req messages only */
		ts_master_en = MAC_TCR_TSMASTERENA;
		ts_event_en = MAC_TCR_TSEVENTENA;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		break;

	/* PTP v2/802.AS1, any layer, any kind of event packet */
	case HWTSTAMP_FILTER_PTP_V2_EVENT:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
		ptp_v2 = MAC_TCR_TSVER2ENA;
		/* take time stamp for all event messages */
		snap_type_sel = MAC_TCR_SNAPTYPSEL_1;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		ptp_over_ethernet = MAC_TCR_TSIPENA;
		av_8021asm_en = MAC_TCR_AV8021ASMEN;
		break;

	/* PTP v2/802.AS1, any layer, Sync packet */
	case HWTSTAMP_FILTER_PTP_V2_SYNC:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_SYNC;
		ptp_v2 = MAC_TCR_TSVER2ENA;
		/* take time stamp for SYNC messages only */
		ts_event_en = MAC_TCR_TSEVENTENA;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		ptp_over_ethernet = MAC_TCR_TSIPENA;
		av_8021asm_en = MAC_TCR_AV8021ASMEN;
		break;

	/* PTP v2/802.AS1, any layer, Delay_req packet */
	case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_DELAY_REQ;
		ptp_v2 = MAC_TCR_TSVER2ENA;
		/* take time stamp for Delay_Req messages only */
		ts_master_en = MAC_TCR_TSMASTERENA;
		ts_event_en = MAC_TCR_TSEVENTENA;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		ptp_over_ethernet = MAC_TCR_TSIPENA;
		av_8021asm_en = MAC_TCR_AV8021ASMEN;
		break;

	/* time stamp any incoming packet */
	case HWTSTAMP_FILTER_ALL:
		config.rx_filter = HWTSTAMP_FILTER_ALL;
		tstamp_all = MAC_TCR_TSENALL;
		break;

	default:
		return -ERANGE;
	}
	pdata->hwts_rx_en =
		((config.rx_filter == HWTSTAMP_FILTER_NONE) ? 0 : 1);

	if (!pdata->hwts_tx_en && !pdata->hwts_rx_en) {
		/* disable hw time stamping */
		hw_if->config_hw_time_stamping(VARMAC_TCR);
	} else {
		VARMAC_TCR = (MAC_TCR_TSENA | MAC_TCR_TSCFUPDT |
				MAC_TCR_TSCTRLSSR |
				tstamp_all | ptp_v2 | ptp_over_ethernet |
				ptp_over_ipv6_udp | ptp_over_ipv4_udp |
				ts_event_en | ts_master_en |
				snap_type_sel | av_8021asm_en);

		if (!pdata->one_nsec_accuracy)
			VARMAC_TCR &= ~MAC_TCR_TSCTRLSSR;

		hw_if->config_hw_time_stamping(VARMAC_TCR);

		/* program default addend */
		hw_if->config_default_addend(pdata, DWC_ETH_QOS_DEFAULT_PTP_CLOCK);

		/* program Sub Second Increment Reg */
		hw_if->config_sub_second_increment(DWC_ETH_QOS_DEFAULT_PTP_CLOCK);

		/* initialize system time */
		getnstimeofday(&now);
		hw_if->init_systime(now.tv_sec, now.tv_nsec);
	}

	DBGPR_PTP("config.flags = %#x, tx_type = %#x, rx_filter = %#x\n",
		  config.flags, config.tx_type, config.rx_filter);

	DBGPR_PTP("<--DWC_ETH_QOS_handle_hwtstamp_ioctl\n");

	return (copy_to_user(ifr->ifr_data, &config,
			     sizeof(struct hwtstamp_config))) ? -EFAULT : 0;
}

static int DWC_ETH_QOS_handle_prv_ioctl_ipa(struct DWC_ETH_QOS_prv_data *pdata,
					struct ifreq *ifr)
{
		struct ifr_data_struct_ipa *ipa_ioctl_data;
		int ret = 0;
		int chInx_tx_ipa, chInx_rx_ipa;
		unsigned long missing;

		DBGPR("-->DWC_ETH_QOS_handle_prv_ioctl_ipa\n");

		if ( !ifr || !ifr->ifr_ifru.ifru_data  )
			   return -EINVAL;

		ipa_ioctl_data = kzalloc(sizeof(struct ifr_data_struct_ipa), GFP_KERNEL);
		if (!ipa_ioctl_data)
			   return -ENOMEM;

		missing = copy_from_user(ipa_ioctl_data, ifr->ifr_ifru.ifru_data, sizeof(struct ifr_data_struct_ipa));
		if (missing)
		   return -EFAULT;

		chInx_tx_ipa = ipa_ioctl_data->chInx_tx_ipa;
		chInx_rx_ipa = ipa_ioctl_data->chInx_rx_ipa;

		if ( (chInx_tx_ipa != IPA_DMA_TX_CH) ||
			(chInx_rx_ipa != IPA_DMA_RX_CH) )
		{
			EMACERR("the RX/TX channels passed are not owned by IPA,correct channels to \
				pass TX: %d RX: %d \n",IPA_DMA_TX_CH, IPA_DMA_RX_CH);
			return DWC_ETH_QOS_CONFIG_FAIL;
		}

		switch ( ipa_ioctl_data->cmd ){

		case DWC_ETH_QOS_IPA_VLAN_ENABLE_CMD:
		   if (!pdata->prv_ipa.vlan_enable) {
			   if (ipa_ioctl_data->vlan_id > MIN_VLAN_ID && ipa_ioctl_data->vlan_id <= MAX_VLAN_ID){
				   pdata->prv_ipa.vlan_id = ipa_ioctl_data->vlan_id;
				   ret = DWC_ETH_QOS_disable_enable_ipa_offload(pdata,chInx_tx_ipa,chInx_rx_ipa);
				   if (!ret)
					pdata->prv_ipa.vlan_enable = true;
		   }
		   else
			EMACERR("INVALID VLAN-ID: %d passed in the IOCTL cmd \n",ipa_ioctl_data->vlan_id);
		   }
		   break;
		case DWC_ETH_QOS_IPA_VLAN_DISABLE_CMD:
			if (pdata->prv_ipa.vlan_enable) {
				pdata->prv_ipa.vlan_id = 0;
				ret = DWC_ETH_QOS_disable_enable_ipa_offload(pdata,chInx_tx_ipa,chInx_rx_ipa);
				if (!ret)
					pdata->prv_ipa.vlan_enable = false;
			}
		   break;

		default:
		   ret = -EOPNOTSUPP;
		   EMACERR( "Unsupported IPA IOCTL call\n");
		}
		return ret;
}

/*!
 * \brief Driver IOCTL routine
 *
 * \details This function is invoked by kernel when a user request an ioctl
 * which can't be handled by the generic interface code. Following operations
 * are performed in this functions.
 * - Configuring the PMT module.
 * - Configuring TX and RX PBL.
 * - Configuring the TX and RX FIFO threshold level.
 * - Configuring the TX and RX OSF mode.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] ifr – pointer to IOCTL specific structure.
 * \param[in] cmd – IOCTL command.
 *
 * \return int
 *
 * \retval 0 - success
 * \retval negative - failure
 */

static int DWC_ETH_QOS_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct ifr_data_struct req;

	struct mii_ioctl_data *data = if_mii(ifr);
	unsigned int reg_val = 0;
	int ret = 0;

	DBGPR("-->DWC_ETH_QOS_ioctl\n");

#ifndef DWC_ETH_QOS_CONFIG_PGTEST
	if ((!netif_running(dev)) || (!pdata->phydev)) {
		DBGPR("<--DWC_ETH_QOS_ioctl - error\n");
		return -EINVAL;
	}
#endif

	mutex_lock(&pdata->mlock);
	switch (cmd) {
	case SIOCGMIIPHY:
		data->phy_id = pdata->phyaddr;
		dev_alert(&pdata->pdev->dev, "PHY ID: SIOCGMIIPHY\n");
		break;

	case SIOCGMIIREG:
		ret =
		    DWC_ETH_QOS_mdio_read_direct(
			   pdata, pdata->phyaddr,
			   (data->reg_num & 0x1F), &reg_val);
		if (ret)
			ret = -EIO;

		data->val_out = reg_val;
		dev_alert(&pdata->pdev->dev, "PHY ID: SIOCGMIIREG reg:%#x reg_val:%#x\n",
			  (data->reg_num & 0x1F), reg_val);
		break;

	case SIOCSMIIREG:
		dev_alert(&pdata->pdev->dev, "PHY ID: SIOCSMIIPHY\n");
		break;

	case DWC_ETH_QOS_PRV_IOCTL:
	   if (copy_from_user(&req, ifr->ifr_ifru.ifru_data,
			   sizeof(struct ifr_data_struct)))
			return -EFAULT;
		ret = DWC_ETH_QOS_handle_prv_ioctl(pdata, &req);
		req.command_error = ret;

		if (copy_to_user(ifr->ifr_ifru.ifru_data, &req,
			sizeof(struct ifr_data_struct)) != 0)
			ret = -EFAULT ;
		break;

	case DWC_ETH_QOS_PRV_IOCTL_IPA:
		if (!pdata->prv_ipa.ipa_uc_ready ) {
			ret = -EAGAIN;
			EMACDBG("IPA or IPA uc is not ready \n");
			break;
		}
		ret = DWC_ETH_QOS_handle_prv_ioctl_ipa(pdata, ifr);
        break;

	case SIOCSHWTSTAMP:
		ret = DWC_ETH_QOS_handle_hwtstamp_ioctl(pdata, ifr);
		break;

	default:
		ret = -EOPNOTSUPP;
		dev_alert(&pdata->pdev->dev, "Unsupported IOCTL call\n");
	}
	mutex_unlock(&pdata->mlock);

	DBGPR("<--DWC_ETH_QOS_ioctl\n");

	return ret;
}

/*!
 * \brief API to change MTU.
 *
 * \details This function is invoked by upper layer when user changes
 * MTU (Maximum Transfer Unit). The MTU is used by the Network layer
 * to driver packet transmission. Ethernet has a default MTU of
 * 1500Bytes. This value can be changed with ifconfig -
 * ifconfig <interface_name> mtu <new_mtu_value>
 *
 * \param[in] dev - pointer to net_device structure
 * \param[in] new_mtu - the new MTU for the device.
 *
 * \return integer
 *
 * \retval 0 - on success and -ve on failure.
 */

static INT DWC_ETH_QOS_change_mtu(struct net_device *dev, INT new_mtu)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	int max_frame = (new_mtu + ETH_HLEN + ETH_FCS_LEN + VLAN_HLEN);

	DBGPR("-->DWC_ETH_QOS_change_mtu: new_mtu:%d\n", new_mtu);

#ifdef DWC_ETH_QOS_CONFIG_PGTEST
	dev_alert(&pdata->pdev->dev, "jumbo frames not supported with PG test\n");
	return -EOPNOTSUPP;
#endif
	if (dev->mtu == new_mtu) {
		dev_alert(&pdata->pdev->dev, "%s: is already configured to %d mtu\n",
			  dev->name, new_mtu);
		return 0;
	}

	/* Supported frame sizes */
	if ((new_mtu < DWC_ETH_QOS_MIN_SUPPORTED_MTU) ||
	    (max_frame > DWC_ETH_QOS_MAX_SUPPORTED_MTU)) {
		dev_alert(&pdata->pdev->dev,
			  "%s: invalid MTU, min %d and max %d MTU are supported\n",
		       dev->name, DWC_ETH_QOS_MIN_SUPPORTED_MTU,
		       DWC_ETH_QOS_MAX_SUPPORTED_MTU);
		return -EINVAL;
	}

	dev_alert(&pdata->pdev->dev, "changing MTU from %d to %d\n",
		  dev->mtu, new_mtu);

	DWC_ETH_QOS_stop_dev(pdata);

	if (max_frame <= 2048)
		pdata->rx_buffer_len = 2048;
	else
		pdata->rx_buffer_len = PAGE_SIZE;
	/* in case of JUMBO frame,
	 * max buffer allocated is
	 * PAGE_SIZE
	 */

	if ((max_frame == ETH_FRAME_LEN + ETH_FCS_LEN) ||
	    (max_frame == ETH_FRAME_LEN + ETH_FCS_LEN + VLAN_HLEN))
		pdata->rx_buffer_len =
		    DWC_ETH_QOS_ETH_FRAME_LEN;

	dev->mtu = new_mtu;

	DWC_ETH_QOS_start_dev(pdata);

	DBGPR("<--DWC_ETH_QOS_change_mtu\n");

	return 0;
}

#ifdef DWC_ETH_QOS_QUEUE_SELECT_ALGO
u16	DWC_ETH_QOS_select_queue(struct net_device *dev,
	struct sk_buff *skb, void *accel_priv,
	select_queue_fallback_t fallback)
{
	u16 txqueue_select = ALL_OTHER_TRAFFIC_TX_CHANNEL;
	UINT eth_type, priority;
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);

	/* Retrieve ETH type */
	eth_type = GET_ETH_TYPE(skb->data);

	if(eth_type == ETH_P_TSN)
	{
		/* Read VLAN priority field from skb->data */
		priority = GET_VLAN_UCP(skb->data);

		priority >>= VLAN_TAG_UCP_SHIFT;
		if(priority == CLASS_A_TRAFFIC_UCP)
			txqueue_select = CLASS_A_TRAFFIC_TX_CHANNEL;
		else if(priority == CLASS_B_TRAFFIC_UCP)
			txqueue_select = CLASS_B_TRAFFIC_TX_CHANNEL;
		else {
			if (pdata->ipa_enabled)
				txqueue_select = ALL_OTHER_TRAFFIC_TX_CHANNEL;
			else
				txqueue_select = ALL_OTHER_TX_TRAFFIC_IPA_DISABLED;
		}
	}
	else {
		/* VLAN tagged IP packet or any other non vlan packets (PTP)*/
		if (pdata->ipa_enabled)
			txqueue_select = ALL_OTHER_TRAFFIC_TX_CHANNEL;
		else
			txqueue_select = ALL_OTHER_TX_TRAFFIC_IPA_DISABLED;
	}

	if (pdata->ipa_enabled && txqueue_select == IPA_DMA_TX_CH) {
	   EMACERR("TX Channel [%d] is not a valid for SW path \n", txqueue_select);
	   BUG();
	}
	return txqueue_select;
}
#endif

unsigned int crc32_snps_le(
	unsigned int initval, unsigned char *data, unsigned int size)
{
	unsigned int crc = initval;
	unsigned int poly = 0x04c11db7;
	unsigned int temp = 0;
	unsigned char my_data = 0;
	int bit_count;

	for (bit_count = 0; bit_count < size; bit_count++) {
		if ((bit_count % 8) == 0)
			my_data = data[bit_count / 8];
		DBGPR_FILTER("%s my_data = %x crc=%x\n",
			     __func__, my_data, crc);
		temp = ((crc >> 31) ^  my_data) &  0x1;
		crc <<= 1;
		if (temp != 0)
			crc ^= poly;
		my_data >>= 1;
	}
	DBGPR_FILTER("%s my_data = %x crc=%x\n", __func__, my_data, crc);
	return ~crc;
}

/*!
 * \brief API to delete vid to HW filter.
 *
 * \details This function is invoked by upper layer when a VLAN id is removed.
 * This function deletes the VLAN id from the HW filter.
 * vlan id can be removed with vconfig -
 * vconfig rem <interface_name > <vlan_id>
 *
 * \param[in] dev - pointer to net_device structure
 * \param[in] vid - vlan id to be removed.
 *
 * \return void
 */
static int DWC_ETH_QOS_vlan_rx_kill_vid(
	struct net_device *dev, __be16 proto, u16 vid)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned short new_index, old_index;
	int crc32_val = 0;
	unsigned int enb_12bit_vhash;

	dev_alert(&pdata->pdev->dev, "-->DWC_ETH_QOS_vlan_rx_kill_vid: vid = %d\n",
		  vid);

	if (pdata->vlan_hash_filtering) {
		crc32_val =
		(bitrev32(~crc32_le(~0, (unsigned char *)&vid, 2)) >> 28);

		enb_12bit_vhash = hw_if->get_vlan_tag_comparison();
		if (enb_12bit_vhash) {
			/* neget 4-bit crc value
			 * for 12-bit VLAN hash comparison
			 */
			new_index = (1 << (~crc32_val & 0xF));
		} else {
			new_index = (1 << (crc32_val & 0xF));
		}

		old_index = hw_if->get_vlan_hash_table_reg();
		old_index &= ~new_index;
		hw_if->update_vlan_hash_table_reg(old_index);
		pdata->vlan_ht_or_id = old_index;
	} else {
		/* By default, receive only VLAN pkt with VID = 1
		 * because writing 0 will pass all VLAN pkt
		 */
		hw_if->update_vlan_id(1);
		pdata->vlan_ht_or_id = 1;
	}

	dev_alert(&pdata->pdev->dev, "<--DWC_ETH_QOS_vlan_rx_kill_vid\n");
	return 0;
}

/*!
 * \brief API to add vid to HW filter.
 *
 * \details This function is invoked by upper layer when a new VALN id is
 * registered. This function updates the HW filter with new VLAN id.
 * New vlan id can be added with vconfig -
 * vconfig add <interface_name > <vlan_id>
 *
 * \param[in] dev - pointer to net_device structure
 * \param[in] vid - new vlan id.
 *
 * \return void
 */
static int DWC_ETH_QOS_vlan_rx_add_vid(
	struct net_device *dev, __be16 proto, u16 vid)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned short new_index, old_index;
	int crc32_val = 0;
	unsigned int enb_12bit_vhash;

	EMACDBG("-->DWC_ETH_QOS_vlan_rx_add_vid: vid = %d\n", vid);

	if (pdata->vlan_hash_filtering) {
		/* The upper 4 bits of the calculated CRC are used to
		 * index the content of the VLAN Hash Table Reg.
		 *
		 */
		crc32_val =
		(bitrev32(~crc32_le(~0, (unsigned char *)&vid, 2)) >> 28);

		/* These 4(0xF) bits determines the bit within the
		 * VLAN Hash Table Reg 0
		 *
		 */
		enb_12bit_vhash = hw_if->get_vlan_tag_comparison();
		if (enb_12bit_vhash) {
			/* neget 4-bit crc value
			 * for 12-bit VLAN hash comparison
			 */
			new_index = (1 << (~crc32_val & 0xF));
		} else {
			new_index = (1 << (crc32_val & 0xF));
		}

		old_index = hw_if->get_vlan_hash_table_reg();
		old_index |= new_index;
		hw_if->update_vlan_hash_table_reg(old_index);
		pdata->vlan_ht_or_id = old_index;
	} else {
		hw_if->update_vlan_id(vid);
		pdata->vlan_ht_or_id = vid;
	}

	EMACDBG("<--DWC_ETH_QOS_vlan_rx_add_vid\n");
	return 0;
}

/*!
 * \brief API called to put device in powerdown mode
 *
 * \details This function is invoked by ioctl function when the user issues an
 * ioctl command to move the device to power down state. Following operations
 * are performed in this function.
 * - stop the phy.
 * - stop the queue.
 * - Disable napi.
 * - Stop DMA TX and RX process.
 * - Enable power down mode using PMT module.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] wakeup_type – remote wake-on-lan or magic packet.
 * \param[in] caller – netif_detach gets called conditionally based
 *                     on caller, IOCTL or DRIVER-suspend
 *
 * \return int
 *
 * \retval zero on success and -ve number on failure.
 */

INT DWC_ETH_QOS_powerdown(struct net_device *dev, UINT wakeup_type,
			  UINT caller)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;

	DBGPR(KERN_ALERT "-->DWC_ETH_QOS_powerdown\n");

	if (!dev || !netif_running(dev) ||
	    (caller == DWC_ETH_QOS_IOCTL_CONTEXT && pdata->power_down)) {
		dev_alert(&pdata->pdev->dev,
			  "Device is already powered down and will powerup for %s\n",
		       DWC_ETH_QOS_POWER_DOWN_TYPE(pdata));
		DBGPR("<--DWC_ETH_QOS_powerdown\n");
		return -EINVAL;
	}

	if (pdata->phydev)
		phy_stop(pdata->phydev);

	mutex_lock(&pdata->pmt_lock);

	if (caller == DWC_ETH_QOS_DRIVER_CONTEXT)
		netif_device_detach(dev);

	/* Stop SW TX before DMA TX in HW */
	netif_tx_disable(dev);
	DWC_ETH_QOS_stop_all_ch_tx_dma(pdata);

	/* Disable MAC TX/RX */
	hw_if->stop_mac_tx_rx();

	/* Stop SW RX after DMA RX in HW */
	DWC_ETH_QOS_stop_all_ch_rx_dma(pdata);
	DWC_ETH_QOS_all_ch_napi_disable(pdata);

	/* enable power down mode by programming the PMT regs */
	if (wakeup_type & DWC_ETH_QOS_REMOTE_WAKEUP)
		hw_if->enable_remote_pmt();
	if (wakeup_type & DWC_ETH_QOS_MAGIC_WAKEUP)
		hw_if->enable_magic_pmt();

	pdata->power_down_type = wakeup_type;

	if (caller == DWC_ETH_QOS_IOCTL_CONTEXT)
		pdata->power_down = 1;

	mutex_unlock(&pdata->pmt_lock);

	DBGPR("<--DWC_ETH_QOS_powerdown\n");

	return 0;
}

/*!
 * \brief API to powerup the device
 *
 * \details This function is invoked by ioctl function when the user issues an
 * ioctl command to move the device to out of power down state. Following
 * operations are performed in this function.
 * - Wakeup the device using PMT module if supported.
 * - Starts the phy.
 * - Enable MAC and DMA TX and RX process.
 * - Enable napi.
 * - Starts the queue.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] caller – netif_attach gets called conditionally based
 *                     on caller, IOCTL or DRIVER-suspend
 *
 * \return int
 *
 * \retval zero on success and -ve number on failure.
 */

INT DWC_ETH_QOS_powerup(struct net_device *dev, UINT caller)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;

	DBGPR("-->DWC_ETH_QOS_powerup\n");

	if (!dev || !netif_running(dev) ||
	    (caller == DWC_ETH_QOS_IOCTL_CONTEXT && !pdata->power_down)) {
		dev_alert(&pdata->pdev->dev, "Device is already powered up\n");
		DBGPR(KERN_ALERT "<--DWC_ETH_QOS_powerup\n");
		return -EINVAL;
	}

	mutex_lock(&pdata->pmt_lock);

	if (pdata->power_down_type & DWC_ETH_QOS_MAGIC_WAKEUP) {
		hw_if->disable_magic_pmt();
		pdata->power_down_type &= ~DWC_ETH_QOS_MAGIC_WAKEUP;
	}

	if (pdata->power_down_type & DWC_ETH_QOS_REMOTE_WAKEUP) {
		hw_if->disable_remote_pmt();
		pdata->power_down_type &= ~DWC_ETH_QOS_REMOTE_WAKEUP;
	}

	pdata->power_down = 0;

	if (pdata->phydev)
		phy_start(pdata->phydev);

	if (caller == DWC_ETH_QOS_DRIVER_CONTEXT)
		netif_device_attach(dev);

	/* Start RX DMA in HW after SW RX (NAPI) */
	DWC_ETH_QOS_napi_enable_mq(pdata);
	DWC_ETH_QOS_start_all_ch_rx_dma(pdata);

	/* enable MAC TX/RX */
	hw_if->start_mac_tx_rx();

	/* Start TX DMA in HW before SW TX */
	DWC_ETH_QOS_start_all_ch_tx_dma(pdata);
	netif_tx_start_all_queues(dev);

	mutex_unlock(&pdata->pmt_lock);

	DBGPR("<--DWC_ETH_QOS_powerup\n");

	return 0;
}

/*!
 * \brief API to configure remote wakeup
 *
 * \details This function is invoked by ioctl function when the user issues an
 * ioctl command to move the device to power down state using remote wakeup.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] req – pointer to ioctl data structure.
 *
 * \return int
 *
 * \retval zero on success and -ve number on failure.
 */

INT DWC_ETH_QOS_configure_remotewakeup(struct net_device *dev,
				       struct ifr_data_struct *req)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;

	if (!dev || !netif_running(dev) || !pdata->hw_feat.rwk_sel
	    || pdata->power_down) {
		dev_alert(&pdata->pdev->dev,
			  "Device is already powered down and will powerup for %s\n",
		       DWC_ETH_QOS_POWER_DOWN_TYPE(pdata));
		return -EINVAL;
	}

	hw_if->configure_rwk_filter(req->rwk_filter_values,
				    req->rwk_filter_length);

	DWC_ETH_QOS_powerdown(dev, DWC_ETH_QOS_REMOTE_WAKEUP,
			      DWC_ETH_QOS_IOCTL_CONTEXT);

	return 0;
}

/*!
 * \details This function is invoked by ioctl function when the user issues an
 * ioctl command to change the RX DMA PBL value. This function will program
 * the device to configure the user specified RX PBL value.
 *
 * \param[in] pdata – pointer to private data structure.
 * \param[in] rx_pbl – RX DMA pbl value to be programmed.
 *
 * \return void
 *
 * \retval none
 */

static void DWC_ETH_QOS_config_rx_pbl(struct DWC_ETH_QOS_prv_data *pdata,
				      UINT rx_pbl,
				      UINT qinx)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	UINT pblx8_val = 0;

	DBGPR("-->DWC_ETH_QOS_config_rx_pbl: %d\n", rx_pbl);

	switch (rx_pbl) {
	case DWC_ETH_QOS_PBL_1:
	case DWC_ETH_QOS_PBL_2:
	case DWC_ETH_QOS_PBL_4:
	case DWC_ETH_QOS_PBL_8:
	case DWC_ETH_QOS_PBL_16:
	case DWC_ETH_QOS_PBL_32:
		hw_if->config_rx_pbl_val(qinx, rx_pbl);
		hw_if->config_pblx8(qinx, 0);
		break;
	case DWC_ETH_QOS_PBL_64:
	case DWC_ETH_QOS_PBL_128:
	case DWC_ETH_QOS_PBL_256:
		hw_if->config_rx_pbl_val(qinx, rx_pbl / 8);
		hw_if->config_pblx8(qinx, 1);
		pblx8_val = 1;
		break;
	}

	switch (pblx8_val) {
	case 0:
		dev_alert(&pdata->pdev->dev, "Tx PBL[%d] value: %d\n",
			  qinx, hw_if->get_tx_pbl_val(qinx));
		dev_alert(&pdata->pdev->dev, "Rx PBL[%d] value: %d\n",
			  qinx, hw_if->get_rx_pbl_val(qinx));
		break;
	case 1:
		dev_alert(&pdata->pdev->dev, "Tx PBL[%d] value: %d\n",
			  qinx, (hw_if->get_tx_pbl_val(qinx) * 8));
		dev_alert(&pdata->pdev->dev, "Rx PBL[%d] value: %d\n",
			  qinx, (hw_if->get_rx_pbl_val(qinx) * 8));
		break;
	}

	DBGPR("<--DWC_ETH_QOS_config_rx_pbl\n");
}

/*!
 * \details This function is invoked by ioctl function when the user issues an
 * ioctl command to change the TX DMA PBL value. This function will program
 * the device to configure the user specified TX PBL value.
 *
 * \param[in] pdata – pointer to private data structure.
 * \param[in] tx_pbl – TX DMA pbl value to be programmed.
 *
 * \return void
 *
 * \retval none
 */

static void DWC_ETH_QOS_config_tx_pbl(struct DWC_ETH_QOS_prv_data *pdata,
				      UINT tx_pbl,
				      UINT qinx)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	UINT pblx8_val = 0;

	DBGPR("-->DWC_ETH_QOS_config_tx_pbl: %d\n", tx_pbl);

	switch (tx_pbl) {
	case DWC_ETH_QOS_PBL_1:
	case DWC_ETH_QOS_PBL_2:
	case DWC_ETH_QOS_PBL_4:
	case DWC_ETH_QOS_PBL_8:
	case DWC_ETH_QOS_PBL_16:
	case DWC_ETH_QOS_PBL_32:
		hw_if->config_tx_pbl_val(qinx, tx_pbl);
		hw_if->config_pblx8(qinx, 0);
		break;
	case DWC_ETH_QOS_PBL_64:
	case DWC_ETH_QOS_PBL_128:
	case DWC_ETH_QOS_PBL_256:
		hw_if->config_tx_pbl_val(qinx, tx_pbl / 8);
		hw_if->config_pblx8(qinx, 1);
		pblx8_val = 1;
		break;
	}

	switch (pblx8_val) {
	case 0:
		dev_alert(&pdata->pdev->dev, "Tx PBL[%d] value: %d\n",
			  qinx, hw_if->get_tx_pbl_val(qinx));
		dev_alert(&pdata->pdev->dev, "Rx PBL[%d] value: %d\n",
			  qinx, hw_if->get_rx_pbl_val(qinx));
		break;
	case 1:
		dev_alert(&pdata->pdev->dev, "Tx PBL[%d] value: %d\n",
			  qinx, (hw_if->get_tx_pbl_val(qinx) * 8));
		dev_alert(&pdata->pdev->dev, "Rx PBL[%d] value: %d\n",
			  qinx, (hw_if->get_rx_pbl_val(qinx) * 8));
		break;
	}

	DBGPR("<--DWC_ETH_QOS_config_tx_pbl\n");
}

/*!
 * \details This function is invoked by ioctl function when the user issues an
 * ioctl command to select the DCB algorithm.
 *
 * \param[in] pdata – pointer to private data structure.
 * \param[in] req – pointer to ioctl data structure.
 *
 * \return void
 *
 * \retval none
 */

static void DWC_ETH_QOS_program_dcb_algorithm(
	struct DWC_ETH_QOS_prv_data *pdata,
	struct ifr_data_struct *req)
{
	struct DWC_ETH_QOS_dcb_algorithm l_dcb_struct, *u_dcb_struct =
		(struct DWC_ETH_QOS_dcb_algorithm *)req->ptr;
	struct hw_if_struct *hw_if = &pdata->hw_if;

	DBGPR("-->DWC_ETH_QOS_program_dcb_algorithm\n");

	if (copy_from_user(&l_dcb_struct, u_dcb_struct,
			   sizeof(struct DWC_ETH_QOS_dcb_algorithm)))
		dev_alert(&pdata->pdev->dev, "Failed to fetch DCB Struct info from user\n");

	hw_if->set_tx_queue_operating_mode(l_dcb_struct.qinx,
		(UINT)l_dcb_struct.op_mode);
	hw_if->set_dcb_algorithm(l_dcb_struct.algorithm);
	hw_if->set_dcb_queue_weight(l_dcb_struct.qinx, l_dcb_struct.weight);

	DBGPR("<--DWC_ETH_QOS_program_dcb_algorithm\n");
}

/*!
 * \details This function is invoked by ioctl function
 * when the user issues an ioctl command to select the
 * AVB algorithm. This function also configures other
 * parameters like send and idle slope, high and low credit.
 *
 * \param[in] pdata – pointer to private data structure.
 * \param[in] req – pointer to ioctl data structure.
 *
 * \return void
 *
 * \retval none
 */

static void DWC_ETH_QOS_program_avb_algorithm(
	struct DWC_ETH_QOS_prv_data *pdata,
	struct ifr_data_struct *req)
{
	struct DWC_ETH_QOS_avb_algorithm l_avb_struct, *u_avb_struct =
		(struct DWC_ETH_QOS_avb_algorithm *)req->ptr;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct DWC_ETH_QOS_avb_algorithm_params *avb_params;

	DBGPR("-->DWC_ETH_QOS_program_avb_algorithm\n");

	if (copy_from_user(&l_avb_struct, u_avb_struct,
			   sizeof(struct DWC_ETH_QOS_avb_algorithm)))
		dev_alert(&pdata->pdev->dev, "Failed to fetch AVB Struct info from user\n");

	if (pdata->speed == SPEED_1000)
		avb_params = &l_avb_struct.speed1000params;
	else
		avb_params = &l_avb_struct.speed100params;

	/*Application uses 1 for CLASS A traffic and 2 for CLASS B traffic
	  Configure right channel accordingly*/
	if (l_avb_struct.qinx == 1)
		l_avb_struct.qinx = CLASS_A_TRAFFIC_TX_CHANNEL;
	else if (l_avb_struct.qinx == 2)
		l_avb_struct.qinx = CLASS_B_TRAFFIC_TX_CHANNEL;

	hw_if->set_tx_queue_operating_mode(l_avb_struct.qinx,
		(UINT)l_avb_struct.op_mode);
	hw_if->set_avb_algorithm(l_avb_struct.qinx, l_avb_struct.algorithm);
	hw_if->config_credit_control(l_avb_struct.qinx, l_avb_struct.cc);
	hw_if->config_send_slope(l_avb_struct.qinx, avb_params->send_slope);
	hw_if->config_idle_slope(l_avb_struct.qinx, avb_params->idle_slope);
	hw_if->config_high_credit(l_avb_struct.qinx, avb_params->hi_credit);
	hw_if->config_low_credit(l_avb_struct.qinx, avb_params->low_credit);

	DBGPR("<--DWC_ETH_QOS_program_avb_algorithm\n");
}

/*!
 * \brief API to read the registers & prints the value.
 * \details This function will read all the device register except
 * data register & prints the values.
 *
 * \return none
 */
#if 0
void dbgpr_regs(void)
{
	UINT val0;
	UINT val1;
	UINT val2;
	UINT val3;
	UINT val4;
	UINT val5;

	MAC_PMTCSR_RGRD(val0);
	MMC_RXICMP_ERR_OCTETS_RGRD(val1);
	MMC_RXICMP_GD_OCTETS_RGRD(val2);
	MMC_RXTCP_ERR_OCTETS_RGRD(val3);
	MMC_RXTCP_GD_OCTETS_RGRD(val4);
	MMC_RXUDP_ERR_OCTETS_RGRD(val5);

	DBGPR("dbgpr_regs: MAC_PMTCSR:%#x\n"
	      "dbgpr_regs: MMC_RXICMP_ERR_OCTETS:%#x\n"
	      "dbgpr_regs: MMC_RXICMP_GD_OCTETS:%#x\n"
	      "dbgpr_regs: MMC_RXTCP_ERR_OCTETS:%#x\n"
	      "dbgpr_regs: MMC_RXTCP_GD_OCTETS:%#x\n"
	      "dbgpr_regs: MMC_RXUDP_ERR_OCTETS:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	MMC_RXUDP_GD_OCTETS_RGRD(val0);
	MMC_RXIPV6_NOPAY_OCTETS_RGRD(val1);
	MMC_RXIPV6_HDRERR_OCTETS_RGRD(val2);
	MMC_RXIPV6_GD_OCTETS_RGRD(val3);
	MMC_RXIPV4_UDSBL_OCTETS_RGRD(val4);
	MMC_RXIPV4_FRAG_OCTETS_RGRD(val5);

	DBGPR("dbgpr_regs: MMC_RXUDP_GD_OCTETS:%#x\n"
	      "dbgpr_regs: MMC_RXIPV6_NOPAY_OCTETS:%#x\n"
	      "dbgpr_regs: MMC_RXIPV6_HDRERR_OCTETS:%#x\n"
	      "dbgpr_regs: MMC_RXIPV6_GD_OCTETS:%#x\n"
	      "dbgpr_regs: MMC_RXIPV4_UDSBL_OCTETS:%#x\n"
	      "dbgpr_regs: MMC_RXIPV4_FRAG_OCTETS:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	MMC_RXIPV4_NOPAY_OCTETS_RGRD(val0);
	MMC_RXIPV4_HDRERR_OCTETS_RGRD(val1);
	MMC_RXIPV4_GD_OCTETS_RGRD(val2);
	MMC_RXICMP_ERR_PKTS_RGRD(val3);
	MMC_RXICMP_GD_PKTS_RGRD(val4);
	MMC_RXTCP_ERR_PKTS_RGRD(val5);

	DBGPR("dbgpr_regs: MMC_RXIPV4_NOPAY_OCTETS:%#x\n"
	      "dbgpr_regs: MMC_RXIPV4_HDRERR_OCTETS:%#x\n"
	      "dbgpr_regs: MMC_RXIPV4_GD_OCTETS:%#x\n"
	      "dbgpr_regs: MMC_RXICMP_ERR_PKTS:%#x\n"
	      "dbgpr_regs: MMC_RXICMP_GD_PKTS:%#x\n"
	      "dbgpr_regs: MMC_RXTCP_ERR_PKTS:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	MMC_RXTCP_GD_PKTS_RGRD(val0);
	MMC_RXUDP_ERR_PKTS_RGRD(val1);
	MMC_RXUDP_GD_PKTS_RGRD(val2);
	MMC_RXIPV6_NOPAY_PKTS_RGRD(val3);
	MMC_RXIPV6_HDRERR_PKTS_RGRD(val4);
	MMC_RXIPV6_GD_PKTS_RGRD(val5);

	DBGPR("dbgpr_regs: MMC_RXTCP_GD_PKTS:%#x\n"
	      "dbgpr_regs: MMC_RXUDP_ERR_PKTS:%#x\n"
	      "dbgpr_regs: MMC_RXUDP_GD_PKTS:%#x\n"
	      "dbgpr_regs: MMC_RXIPV6_NOPAY_PKTS:%#x\n"
	      "dbgpr_regs: MMC_RXIPV6_HDRERR_PKTS:%#x\n"
	      "dbgpr_regs: MMC_RXIPV6_GD_PKTS:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	MMC_RXIPV4_UBSBL_PKTS_RGRD(val0);
	MMC_RXIPV4_FRAG_PKTS_RGRD(val1);
	MMC_RXIPV4_NOPAY_PKTS_RGRD(val2);
	MMC_RXIPV4_HDRERR_PKTS_RGRD(val3);
	MMC_RXIPV4_GD_PKTS_RGRD(val4);
	MMC_RXCTRLPACKETS_G_RGRD(val5);

	DBGPR("dbgpr_regs: MMC_RXIPV4_UBSBL_PKTS:%#x\n"
	      "dbgpr_regs: MMC_RXIPV4_FRAG_PKTS:%#x\n"
	      "dbgpr_regs: MMC_RXIPV4_NOPAY_PKTS:%#x\n"
	      "dbgpr_regs: MMC_RXIPV4_HDRERR_PKTS:%#x\n"
	      "dbgpr_regs: MMC_RXIPV4_GD_PKTS:%#x\n"
	      "dbgpr_regs: MMC_RXCTRLPACKETS_G:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	MMC_RXRCVERROR_RGRD(val0);
	MMC_RXWATCHDOGERROR_RGRD(val1);
	MMC_RXVLANPACKETS_GB_RGRD(val2);
	MMC_RXFIFOOVERFLOW_RGRD(val3);
	MMC_RXPAUSEPACKETS_RGRD(val4);
	MMC_RXOUTOFRANGETYPE_RGRD(val5);

	DBGPR("dbgpr_regs: MMC_RXRCVERROR:%#x\n"
	      "dbgpr_regs: MMC_RXWATCHDOGERROR:%#x\n"
	      "dbgpr_regs: MMC_RXVLANPACKETS_GB:%#x\n"
	      "dbgpr_regs: MMC_RXFIFOOVERFLOW:%#x\n"
	      "dbgpr_regs: MMC_RXPAUSEPACKETS:%#x\n"
	      "dbgpr_regs: MMC_RXOUTOFRANGETYPE:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	MMC_RXLENGTHERROR_RGRD(val0);
	MMC_RXUNICASTPACKETS_G_RGRD(val1);
	MMC_RX1024TOMAXOCTETS_GB_RGRD(val2);
	MMC_RX512TO1023OCTETS_GB_RGRD(val3);
	MMC_RX256TO511OCTETS_GB_RGRD(val4);
	MMC_RX128TO255OCTETS_GB_RGRD(val5);

	DBGPR("dbgpr_regs: MMC_RXLENGTHERROR:%#x\n"
	      "dbgpr_regs: MMC_RXUNICASTPACKETS_G:%#x\n"
	      "dbgpr_regs: MMC_RX1024TOMAXOCTETS_GB:%#x\n"
	      "dbgpr_regs: MMC_RX512TO1023OCTETS_GB:%#x\n"
	      "dbgpr_regs: MMC_RX256TO511OCTETS_GB:%#x\n"
	      "dbgpr_regs: MMC_RX128TO255OCTETS_GB:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	MMC_RX65TO127OCTETS_GB_RGRD(val0);
	MMC_RX64OCTETS_GB_RGRD(val1);
	MMC_RXOVERSIZE_G_RGRD(val2);
	MMC_RXUNDERSIZE_G_RGRD(val3);
	MMC_RXJABBERERROR_RGRD(val4);
	MMC_RXRUNTERROR_RGRD(val5);

	DBGPR("dbgpr_regs: MMC_RX65TO127OCTETS_GB:%#x\n"
	      "dbgpr_regs: MMC_RX64OCTETS_GB:%#x\n"
	      "dbgpr_regs: MMC_RXOVERSIZE_G:%#x\n"
	      "dbgpr_regs: MMC_RXUNDERSIZE_G:%#x\n"
	      "dbgpr_regs: MMC_RXJABBERERROR:%#x\n"
	      "dbgpr_regs: MMC_RXRUNTERROR:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	MMC_RXALIGNMENTERROR_RGRD(val0);
	MMC_RXCRCERROR_RGRD(val1);
	MMC_RXMULTICASTPACKETS_G_RGRD(val2);
	MMC_RXBROADCASTPACKETS_G_RGRD(val3);
	MMC_RXOCTETCOUNT_G_RGRD(val4);
	MMC_RXOCTETCOUNT_GB_RGRD(val5);

	DBGPR("dbgpr_regs: MMC_RXALIGNMENTERROR:%#x\n"
	      "dbgpr_regs: MMC_RXCRCERROR:%#x\n"
	      "dbgpr_regs: MMC_RXMULTICASTPACKETS_G:%#x\n"
	      "dbgpr_regs: MMC_RXBROADCASTPACKETS_G:%#x\n"
	      "dbgpr_regs: MMC_RXOCTETCOUNT_G:%#x\n"
	      "dbgpr_regs: MMC_RXOCTETCOUNT_GB:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	MMC_RXPACKETCOUNT_GB_RGRD(val0);
	MMC_TXOVERSIZE_G_RGRD(val1);
	MMC_TXVLANPACKETS_G_RGRD(val2);
	MMC_TXPAUSEPACKETS_RGRD(val3);
	MMC_TXEXCESSDEF_RGRD(val4);
	MMC_TXPACKETSCOUNT_G_RGRD(val5);

	DBGPR("dbgpr_regs: MMC_RXPACKETCOUNT_GB:%#x\n"
	      "dbgpr_regs: MMC_TXOVERSIZE_G:%#x\n"
	      "dbgpr_regs: MMC_TXVLANPACKETS_G:%#x\n"
	      "dbgpr_regs: MMC_TXPAUSEPACKETS:%#x\n"
	      "dbgpr_regs: MMC_TXEXCESSDEF:%#x\n"
	      "dbgpr_regs: MMC_TXPACKETSCOUNT_G:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	MMC_TXOCTETCOUNT_G_RGRD(val0);
	MMC_TXCARRIERERROR_RGRD(val1);
	MMC_TXEXESSCOL_RGRD(val2);
	MMC_TXLATECOL_RGRD(val3);
	MMC_TXDEFERRED_RGRD(val4);
	MMC_TXMULTICOL_G_RGRD(val5);

	DBGPR("dbgpr_regs: MMC_TXOCTETCOUNT_G:%#x\n"
	      "dbgpr_regs: MMC_TXCARRIERERROR:%#x\n"
	      "dbgpr_regs: MMC_TXEXESSCOL:%#x\n"
	      "dbgpr_regs: MMC_TXLATECOL:%#x\n"
	      "dbgpr_regs: MMC_TXDEFERRED:%#x\n"
	      "dbgpr_regs: MMC_TXMULTICOL_G:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	MMC_TXSINGLECOL_G_RGRD(val0);
	MMC_TXUNDERFLOWERROR_RGRD(val1);
	MMC_TXBROADCASTPACKETS_GB_RGRD(val2);
	MMC_TXMULTICASTPACKETS_GB_RGRD(val3);
	MMC_TXUNICASTPACKETS_GB_RGRD(val4);
	MMC_TX1024TOMAXOCTETS_GB_RGRD(val5);

	DBGPR("dbgpr_regs: MMC_TXSINGLECOL_G:%#x\n"
	      "dbgpr_regs: MMC_TXUNDERFLOWERROR:%#x\n"
	      "dbgpr_regs: MMC_TXBROADCASTPACKETS_GB:%#x\n"
	      "dbgpr_regs: MMC_TXMULTICASTPACKETS_GB:%#x\n"
	      "dbgpr_regs: MMC_TXUNICASTPACKETS_GB:%#x\n"
	      "dbgpr_regs: MMC_TX1024TOMAXOCTETS_GB:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	MMC_TX512TO1023OCTETS_GB_RGRD(val0);
	MMC_TX256TO511OCTETS_GB_RGRD(val1);
	MMC_TX128TO255OCTETS_GB_RGRD(val2);
	MMC_TX65TO127OCTETS_GB_RGRD(val3);
	MMC_TX64OCTETS_GB_RGRD(val4);
	MMC_TXMULTICASTPACKETS_G_RGRD(val5);

	DBGPR("dbgpr_regs: MMC_TX512TO1023OCTETS_GB:%#x\n"
	      "dbgpr_regs: MMC_TX256TO511OCTETS_GB:%#x\n"
	      "dbgpr_regs: MMC_TX128TO255OCTETS_GB:%#x\n"
	      "dbgpr_regs: MMC_TX65TO127OCTETS_GB:%#x\n"
	      "dbgpr_regs: MMC_TX64OCTETS_GB:%#x\n"
	      "dbgpr_regs: MMC_TXMULTICASTPACKETS_G:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	MMC_TXBROADCASTPACKETS_G_RGRD(val0);
	MMC_TXPACKETCOUNT_GB_RGRD(val1);
	MMC_TXOCTETCOUNT_GB_RGRD(val2);
	MMC_IPC_INTR_RX_RGRD(val3);
	MMC_IPC_INTR_MASK_RX_RGRD(val4);
	MMC_INTR_MASK_TX_RGRD(val5);

	DBGPR("dbgpr_regs: MMC_TXBROADCASTPACKETS_G:%#x\n"
	      "dbgpr_regs: MMC_TXPACKETCOUNT_GB:%#x\n"
	      "dbgpr_regs: MMC_TXOCTETCOUNT_GB:%#x\n"
	      "dbgpr_regs: MMC_IPC_INTR_RX:%#x\n"
	      "dbgpr_regs: MMC_IPC_INTR_MASK_RX:%#x\n"
	      "dbgpr_regs: MMC_INTR_MASK_TX:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	MMC_INTR_MASK_RX_RGRD(val0);
	MMC_INTR_TX_RGRD(val1);
	MMC_INTR_RX_RGRD(val2);
	MMC_CNTRL_RGRD(val3);
	MAC_MA1LR_RGRD(val4);
	MAC_MA1HR_RGRD(val5);

	DBGPR("dbgpr_regs: MMC_INTR_MASK_RX:%#x\n"
	      "dbgpr_regs: MMC_INTR_TX:%#x\n"
	      "dbgpr_regs: MMC_INTR_RX:%#x\n"
	      "dbgpr_regs: MMC_CNTRL:%#x\n"
	      "dbgpr_regs: MAC_MA1LR:%#x\n"
	      "dbgpr_regs: MAC_MA1HR:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	MAC_MA0LR_RGRD(val0);
	MAC_MA0HR_RGRD(val1);
	MAC_GPIOR_RGRD(val2);
	MAC_GMIIDR_RGRD(val3);
	MAC_GMIIAR_RGRD(val4);
	MAC_HFR2_RGRD(val5);

	DBGPR("dbgpr_regs: MAC_MA0LR:%#x\n"
	      "dbgpr_regs: MAC_MA0HR:%#x\n"
	      "dbgpr_regs: MAC_GPIOR:%#x\n"
	      "dbgpr_regs: MAC_GMIIDR:%#x\n"
	      "dbgpr_regs: MAC_GMIIAR:%#x\n"
	      "dbgpr_regs: MAC_HFR2:%#x\n", val0, val1, val2, val3, val4, val5);

	MAC_HFR1_RGRD(val0);
	MAC_HFR0_RGRD(val1);
	MAC_MDR_RGRD(val2);
	MAC_VR_RGRD(val3);
	MAC_HTR7_RGRD(val4);
	MAC_HTR6_RGRD(val5);

	DBGPR("dbgpr_regs: MAC_HFR1:%#x\n"
	      "dbgpr_regs: MAC_HFR0:%#x\n"
	      "dbgpr_regs: MAC_MDR:%#x\n"
	      "dbgpr_regs: MAC_VR:%#x\n"
	      "dbgpr_regs: MAC_HTR7:%#x\n"
	      "dbgpr_regs: MAC_HTR6:%#x\n", val0, val1, val2, val3, val4, val5);

	MAC_HTR5_RGRD(val0);
	MAC_HTR4_RGRD(val1);
	MAC_HTR3_RGRD(val2);
	MAC_HTR2_RGRD(val3);
	MAC_HTR1_RGRD(val4);
	MAC_HTR0_RGRD(val5);

	DBGPR("dbgpr_regs: MAC_HTR5:%#x\n"
	      "dbgpr_regs: MAC_HTR4:%#x\n"
	      "dbgpr_regs: MAC_HTR3:%#x\n"
	      "dbgpr_regs: MAC_HTR2:%#x\n"
	      "dbgpr_regs: MAC_HTR1:%#x\n"
	      "dbgpr_regs: MAC_HTR0:%#x\n", val0, val1, val2, val3, val4, val5);

	DMA_RIWTR7_RGRD(val0);
	DMA_RIWTR6_RGRD(val1);
	DMA_RIWTR5_RGRD(val2);
	DMA_RIWTR4_RGRD(val3);
	DMA_RIWTR3_RGRD(val4);
	DMA_RIWTR2_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_RIWTR7:%#x\n"
	      "dbgpr_regs: DMA_RIWTR6:%#x\n"
	      "dbgpr_regs: DMA_RIWTR5:%#x\n"
	      "dbgpr_regs: DMA_RIWTR4:%#x\n"
	      "dbgpr_regs: DMA_RIWTR3:%#x\n"
	      "dbgpr_regs: DMA_RIWTR2:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_RIWTR1_RGRD(val0);
	DMA_RIWTR0_RGRD(val1);
	DMA_RDRLR7_RGRD(val2);
	DMA_RDRLR6_RGRD(val3);
	DMA_RDRLR5_RGRD(val4);
	DMA_RDRLR4_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_RIWTR1:%#x\n"
	      "dbgpr_regs: DMA_RIWTR0:%#x\n"
	      "dbgpr_regs: DMA_RDRLR7:%#x\n"
	      "dbgpr_regs: DMA_RDRLR6:%#x\n"
	      "dbgpr_regs: DMA_RDRLR5:%#x\n"
	      "dbgpr_regs: DMA_RDRLR4:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_RDRLR3_RGRD(val0);
	DMA_RDRLR2_RGRD(val1);
	DMA_RDRLR1_RGRD(val2);
	DMA_RDRLR0_RGRD(val3);
	DMA_TDRLR7_RGRD(val4);
	DMA_TDRLR6_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_RDRLR3:%#x\n"
	      "dbgpr_regs: DMA_RDRLR2:%#x\n"
	      "dbgpr_regs: DMA_RDRLR1:%#x\n"
	      "dbgpr_regs: DMA_RDRLR0:%#x\n"
	      "dbgpr_regs: DMA_TDRLR7:%#x\n"
	      "dbgpr_regs: DMA_TDRLR6:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_TDRLR5_RGRD(val0);
	DMA_TDRLR4_RGRD(val1);
	DMA_TDRLR3_RGRD(val2);
	DMA_TDRLR2_RGRD(val3);
	DMA_TDRLR1_RGRD(val4);
	DMA_TDRLR0_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_TDRLR5:%#x\n"
	      "dbgpr_regs: DMA_TDRLR4:%#x\n"
	      "dbgpr_regs: DMA_TDRLR3:%#x\n"
	      "dbgpr_regs: DMA_TDRLR2:%#x\n"
	      "dbgpr_regs: DMA_TDRLR1:%#x\n"
	      "dbgpr_regs: DMA_TDRLR0:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_RDTP_RPDR7_RGRD(val0);
	DMA_RDTP_RPDR6_RGRD(val1);
	DMA_RDTP_RPDR5_RGRD(val2);
	DMA_RDTP_RPDR4_RGRD(val3);
	DMA_RDTP_RPDR3_RGRD(val4);
	DMA_RDTP_RPDR2_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_RDTP_RPDR7:%#x\n"
	      "dbgpr_regs: DMA_RDTP_RPDR6:%#x\n"
	      "dbgpr_regs: DMA_RDTP_RPDR5:%#x\n"
	      "dbgpr_regs: DMA_RDTP_RPDR4:%#x\n"
	      "dbgpr_regs: DMA_RDTP_RPDR3:%#x\n"
	      "dbgpr_regs: DMA_RDTP_RPDR2:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_RDTP_RPDR1_RGRD(val0);
	DMA_RDTP_RPDR0_RGRD(val1);
	DMA_TDTP_TPDR7_RGRD(val2);
	DMA_TDTP_TPDR6_RGRD(val3);
	DMA_TDTP_TPDR5_RGRD(val4);
	DMA_TDTP_TPDR4_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_RDTP_RPDR1:%#x\n"
	      "dbgpr_regs: DMA_RDTP_RPDR0:%#x\n"
	      "dbgpr_regs: DMA_TDTP_TPDR7:%#x\n"
	      "dbgpr_regs: DMA_TDTP_TPDR6:%#x\n"
	      "dbgpr_regs: DMA_TDTP_TPDR5:%#x\n"
	      "dbgpr_regs: DMA_TDTP_TPDR4:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_TDTP_TPDR3_RGRD(val0);
	DMA_TDTP_TPDR2_RGRD(val1);
	DMA_TDTP_TPDR1_RGRD(val2);
	DMA_TDTP_TPDR0_RGRD(val3);
	DMA_RDLAR7_RGRD(val4);
	DMA_RDLAR6_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_TDTP_TPDR3:%#x\n"
	      "dbgpr_regs: DMA_TDTP_TPDR2:%#x\n"
	      "dbgpr_regs: DMA_TDTP_TPDR1:%#x\n"
	      "dbgpr_regs: DMA_TDTP_TPDR0:%#x\n"
	      "dbgpr_regs: DMA_RDLAR7:%#x\n"
	      "dbgpr_regs: DMA_RDLAR6:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_RDLAR5_RGRD(val0);
	DMA_RDLAR4_RGRD(val1);
	DMA_RDLAR3_RGRD(val2);
	DMA_RDLAR2_RGRD(val3);
	DMA_RDLAR1_RGRD(val4);
	DMA_RDLAR0_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_RDLAR5:%#x\n"
	      "dbgpr_regs: DMA_RDLAR4:%#x\n"
	      "dbgpr_regs: DMA_RDLAR3:%#x\n"
	      "dbgpr_regs: DMA_RDLAR2:%#x\n"
	      "dbgpr_regs: DMA_RDLAR1:%#x\n"
	      "dbgpr_regs: DMA_RDLAR0:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_TDLAR7_RGRD(val0);
	DMA_TDLAR6_RGRD(val1);
	DMA_TDLAR5_RGRD(val2);
	DMA_TDLAR4_RGRD(val3);
	DMA_TDLAR3_RGRD(val4);
	DMA_TDLAR2_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_TDLAR7:%#x\n"
	      "dbgpr_regs: DMA_TDLAR6:%#x\n"
	      "dbgpr_regs: DMA_TDLAR5:%#x\n"
	      "dbgpr_regs: DMA_TDLAR4:%#x\n"
	      "dbgpr_regs: DMA_TDLAR3:%#x\n"
	      "dbgpr_regs: DMA_TDLAR2:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_TDLAR1_RGRD(val0);
	DMA_TDLAR0_RGRD(val1);
	DMA_IER7_RGRD(val2);
	DMA_IER6_RGRD(val3);
	DMA_IER5_RGRD(val4);
	DMA_IER4_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_TDLAR1:%#x\n"
	      "dbgpr_regs: DMA_TDLAR0:%#x\n"
	      "dbgpr_regs: DMA_IER7:%#x\n"
	      "dbgpr_regs: DMA_IER6:%#x\n"
	      "dbgpr_regs: DMA_IER5:%#x\n"
	      "dbgpr_regs: DMA_IER4:%#x\n", val0, val1, val2, val3, val4, val5);

	DMA_IER3_RGRD(val0);
	DMA_IER2_RGRD(val1);
	DMA_IER1_RGRD(val2);
	DMA_IER0_RGRD(val3);
	MAC_IMR_RGRD(val4);
	MAC_ISR_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_IER3:%#x\n"
	      "dbgpr_regs: DMA_IER2:%#x\n"
	      "dbgpr_regs: DMA_IER1:%#x\n"
	      "dbgpr_regs: DMA_IER0:%#x\n"
	      "dbgpr_regs: MAC_IMR:%#x\n"
	      "dbgpr_regs: MAC_ISR:%#x\n", val0, val1, val2, val3, val4, val5);

	MTL_ISR_RGRD(val0);
	DMA_SR7_RGRD(val1);
	DMA_SR6_RGRD(val2);
	DMA_SR5_RGRD(val3);
	DMA_SR4_RGRD(val4);
	DMA_SR3_RGRD(val5);

	DBGPR("dbgpr_regs: MTL_ISR:%#x\n"
	      "dbgpr_regs: DMA_SR7:%#x\n"
	      "dbgpr_regs: DMA_SR6:%#x\n"
	      "dbgpr_regs: DMA_SR5:%#x\n"
	      "dbgpr_regs: DMA_SR4:%#x\n"
	      "dbgpr_regs: DMA_SR3:%#x\n", val0, val1, val2, val3, val4, val5);

	DMA_SR2_RGRD(val0);
	DMA_SR1_RGRD(val1);
	DMA_SR0_RGRD(val2);
	DMA_ISR_RGRD(val3);
	DMA_DSR2_RGRD(val4);
	DMA_DSR1_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_SR2:%#x\n"
	      "dbgpr_regs: DMA_SR1:%#x\n"
	      "dbgpr_regs: DMA_SR0:%#x\n"
	      "dbgpr_regs: DMA_ISR:%#x\n"
	      "dbgpr_regs: DMA_DSR2:%#x\n"
	      "dbgpr_regs: DMA_DSR1:%#x\n", val0, val1, val2, val3, val4, val5);

	DMA_DSR0_RGRD(val0);
	MTL_Q0RDR_RGRD(val1);
	MTL_Q0ESR_RGRD(val2);
	MTL_Q0TDR_RGRD(val3);
	DMA_CHRBAR7_RGRD(val4);
	DMA_CHRBAR6_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_DSR0:%#x\n"
	      "dbgpr_regs: MTL_Q0RDR:%#x\n"
	      "dbgpr_regs: MTL_Q0ESR:%#x\n"
	      "dbgpr_regs: MTL_Q0TDR:%#x\n"
	      "dbgpr_regs: DMA_CHRBAR7:%#x\n"
	      "dbgpr_regs: DMA_CHRBAR6:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_CHRBAR5_RGRD(val0);
	DMA_CHRBAR4_RGRD(val1);
	DMA_CHRBAR3_RGRD(val2);
	DMA_CHRBAR2_RGRD(val3);
	DMA_CHRBAR1_RGRD(val4);
	DMA_CHRBAR0_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_CHRBAR5:%#x\n"
	      "dbgpr_regs: DMA_CHRBAR4:%#x\n"
	      "dbgpr_regs: DMA_CHRBAR3:%#x\n"
	      "dbgpr_regs: DMA_CHRBAR2:%#x\n"
	      "dbgpr_regs: DMA_CHRBAR1:%#x\n"
	      "dbgpr_regs: DMA_CHRBAR0:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_CHTBAR7_RGRD(val0);
	DMA_CHTBAR6_RGRD(val1);
	DMA_CHTBAR5_RGRD(val2);
	DMA_CHTBAR4_RGRD(val3);
	DMA_CHTBAR3_RGRD(val4);
	DMA_CHTBAR2_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_CHTBAR7:%#x\n"
	      "dbgpr_regs: DMA_CHTBAR6:%#x\n"
	      "dbgpr_regs: DMA_CHTBAR5:%#x\n"
	      "dbgpr_regs: DMA_CHTBAR4:%#x\n"
	      "dbgpr_regs: DMA_CHTBAR3:%#x\n"
	      "dbgpr_regs: DMA_CHTBAR2:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_CHTBAR1_RGRD(val0);
	DMA_CHTBAR0_RGRD(val1);
	DMA_CHRDR7_RGRD(val2);
	DMA_CHRDR6_RGRD(val3);
	DMA_CHRDR5_RGRD(val4);
	DMA_CHRDR4_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_CHTBAR1:%#x\n"
	      "dbgpr_regs: DMA_CHTBAR0:%#x\n"
	      "dbgpr_regs: DMA_CHRDR7:%#x\n"
	      "dbgpr_regs: DMA_CHRDR6:%#x\n"
	      "dbgpr_regs: DMA_CHRDR5:%#x\n"
	      "dbgpr_regs: DMA_CHRDR4:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_CHRDR3_RGRD(val0);
	DMA_CHRDR2_RGRD(val1);
	DMA_CHRDR1_RGRD(val2);
	DMA_CHRDR0_RGRD(val3);
	DMA_CHTDR7_RGRD(val4);
	DMA_CHTDR6_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_CHRDR3:%#x\n"
	      "dbgpr_regs: DMA_CHRDR2:%#x\n"
	      "dbgpr_regs: DMA_CHRDR1:%#x\n"
	      "dbgpr_regs: DMA_CHRDR0:%#x\n"
	      "dbgpr_regs: DMA_CHTDR7:%#x\n"
	      "dbgpr_regs: DMA_CHTDR6:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_CHTDR5_RGRD(val0);
	DMA_CHTDR4_RGRD(val1);
	DMA_CHTDR3_RGRD(val2);
	DMA_CHTDR2_RGRD(val3);
	DMA_CHTDR1_RGRD(val4);
	DMA_CHTDR0_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_CHTDR5:%#x\n"
	      "dbgpr_regs: DMA_CHTDR4:%#x\n"
	      "dbgpr_regs: DMA_CHTDR3:%#x\n"
	      "dbgpr_regs: DMA_CHTDR2:%#x\n"
	      "dbgpr_regs: DMA_CHTDR1:%#x\n"
	      "dbgpr_regs: DMA_CHTDR0:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_SFCSR7_RGRD(val0);
	DMA_SFCSR6_RGRD(val1);
	DMA_SFCSR5_RGRD(val2);
	DMA_SFCSR4_RGRD(val3);
	DMA_SFCSR3_RGRD(val4);
	DMA_SFCSR2_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_SFCSR7:%#x\n"
	      "dbgpr_regs: DMA_SFCSR6:%#x\n"
	      "dbgpr_regs: DMA_SFCSR5:%#x\n"
	      "dbgpr_regs: DMA_SFCSR4:%#x\n"
	      "dbgpr_regs: DMA_SFCSR3:%#x\n"
	      "dbgpr_regs: DMA_SFCSR2:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_SFCSR1_RGRD(val0);
	DMA_SFCSR0_RGRD(val1);
	MAC_IVLANTIRR_RGRD(val2);
	MAC_VLANTIRR_RGRD(val3);
	MAC_VLANHTR_RGRD(val4);
	MAC_VLANTR_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_SFCSR1:%#x\n"
	      "dbgpr_regs: DMA_SFCSR0:%#x\n"
	      "dbgpr_regs: MAC_IVLANTIRR:%#x\n"
	      "dbgpr_regs: MAC_VLANTIRR:%#x\n"
	      "dbgpr_regs: MAC_VLANHTR:%#x\n"
	      "dbgpr_regs: MAC_VLANTR:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_SBUS_RGRD(val0);
	DMA_BMR_RGRD(val1);
	MTL_Q0RCR_RGRD(val2);
	MTL_Q0OCR_RGRD(val3);
	MTL_Q0ROMR_RGRD(val4);
	MTL_Q0QR_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_SBUS:%#x\n"
	      "dbgpr_regs: DMA_BMR:%#x\n"
	      "dbgpr_regs: MTL_Q0RCR:%#x\n"
	      "dbgpr_regs: MTL_Q0OCR:%#x\n"
	      "dbgpr_regs: MTL_Q0ROMR:%#x\n"
	      "dbgpr_regs: MTL_Q0QR:%#x\n", val0, val1, val2, val3, val4, val5);

	MTL_Q0ECR_RGRD(val0);
	MTL_Q0UCR_RGRD(val1);
	MTL_Q0TOMR_RGRD(val2);
	MTL_RQDCM1R_RGRD(val3);
	MTL_RQDCM0R_RGRD(val4);
	MTL_FDDR_RGRD(val5);

	DBGPR("dbgpr_regs: MTL_Q0ECR:%#x\n"
	      "dbgpr_regs: MTL_Q0UCR:%#x\n"
	      "dbgpr_regs: MTL_Q0TOMR:%#x\n"
	      "dbgpr_regs: MTL_RQDCM1R:%#x\n"
	      "dbgpr_regs: MTL_RQDCM0R:%#x\n"
	      "dbgpr_regs: MTL_FDDR:%#x\n", val0, val1, val2, val3, val4, val5);

	MTL_FDACS_RGRD(val0);
	MTL_OMR_RGRD(val1);
	MAC_RQC1R_RGRD(val2);
	MAC_RQC0R_RGRD(val3);
	MAC_TQPM1R_RGRD(val4);
	MAC_TQPM0R_RGRD(val5);

	DBGPR("dbgpr_regs: MTL_FDACS:%#x\n"
	      "dbgpr_regs: MTL_OMR:%#x\n"
	      "dbgpr_regs: MAC_RQC1R:%#x\n"
	      "dbgpr_regs: MAC_RQC0R:%#x\n"
	      "dbgpr_regs: MAC_TQPM1R:%#x\n"
	      "dbgpr_regs: MAC_TQPM0R:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	MAC_RFCR_RGRD(val0);
	MAC_QTFCR7_RGRD(val1);
	MAC_QTFCR6_RGRD(val2);
	MAC_QTFCR5_RGRD(val3);
	MAC_QTFCR4_RGRD(val4);
	MAC_QTFCR3_RGRD(val5);

	DBGPR("dbgpr_regs: MAC_RFCR:%#x\n"
	      "dbgpr_regs: MAC_QTFCR7:%#x\n"
	      "dbgpr_regs: MAC_QTFCR6:%#x\n"
	      "dbgpr_regs: MAC_QTFCR5:%#x\n"
	      "dbgpr_regs: MAC_QTFCR4:%#x\n"
	      "dbgpr_regs: MAC_QTFCR3:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	MAC_QTFCR2_RGRD(val0);
	MAC_QTFCR1_RGRD(val1);
	MAC_Q0TFCR_RGRD(val2);
	DMA_AXI4CR7_RGRD(val3);
	DMA_AXI4CR6_RGRD(val4);
	DMA_AXI4CR5_RGRD(val5);

	DBGPR("dbgpr_regs: MAC_QTFCR2:%#x\n"
	      "dbgpr_regs: MAC_QTFCR1:%#x\n"
	      "dbgpr_regs: MAC_Q0TFCR:%#x\n"
	      "dbgpr_regs: DMA_AXI4CR7:%#x\n"
	      "dbgpr_regs: DMA_AXI4CR6:%#x\n"
	      "dbgpr_regs: DMA_AXI4CR5:%#x\n",
	      val0, val1, val2, val3, val4, val5);

	DMA_AXI4CR4_RGRD(val0);
	DMA_AXI4CR3_RGRD(val1);
	DMA_AXI4CR2_RGRD(val2);
	DMA_AXI4CR1_RGRD(val3);
	DMA_AXI4CR0_RGRD(val4);
	DMA_RCR7_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_AXI4CR4:%#x\n"
	      "dbgpr_regs: DMA_AXI4CR3:%#x\n"
	      "dbgpr_regs: DMA_AXI4CR2:%#x\n"
	      "dbgpr_regs: DMA_AXI4CR1:%#x\n"
	      "dbgpr_regs: DMA_AXI4CR0:%#x\n"
	      "dbgpr_regs: DMA_RCR7:%#x\n", val0, val1, val2, val3, val4, val5);

	DMA_RCR6_RGRD(val0);
	DMA_RCR5_RGRD(val1);
	DMA_RCR4_RGRD(val2);
	DMA_RCR3_RGRD(val3);
	DMA_RCR2_RGRD(val4);
	DMA_RCR1_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_RCR6:%#x\n"
	      "dbgpr_regs: DMA_RCR5:%#x\n"
	      "dbgpr_regs: DMA_RCR4:%#x\n"
	      "dbgpr_regs: DMA_RCR3:%#x\n"
	      "dbgpr_regs: DMA_RCR2:%#x\n"
	      "dbgpr_regs: DMA_RCR1:%#x\n", val0, val1, val2, val3, val4, val5);

	DMA_RCR0_RGRD(val0);
	DMA_TCR7_RGRD(val1);
	DMA_TCR6_RGRD(val2);
	DMA_TCR5_RGRD(val3);
	DMA_TCR4_RGRD(val4);
	DMA_TCR3_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_RCR0:%#x\n"
	      "dbgpr_regs: DMA_TCR7:%#x\n"
	      "dbgpr_regs: DMA_TCR6:%#x\n"
	      "dbgpr_regs: DMA_TCR5:%#x\n"
	      "dbgpr_regs: DMA_TCR4:%#x\n"
	      "dbgpr_regs: DMA_TCR3:%#x\n", val0, val1, val2, val3, val4, val5);

	DMA_TCR2_RGRD(val0);
	DMA_TCR1_RGRD(val1);
	DMA_TCR0_RGRD(val2);
	DMA_CR7_RGRD(val3);
	DMA_CR6_RGRD(val4);
	DMA_CR5_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_TCR2:%#x\n"
	      "dbgpr_regs: DMA_TCR1:%#x\n"
	      "dbgpr_regs: DMA_TCR0:%#x\n"
	      "dbgpr_regs: DMA_CR7:%#x\n"
	      "dbgpr_regs: DMA_CR6:%#x\n"
	      "dbgpr_regs: DMA_CR5:%#x\n", val0, val1, val2, val3, val4, val5);

	DMA_CR4_RGRD(val0);
	DMA_CR3_RGRD(val1);
	DMA_CR2_RGRD(val2);
	DMA_CR1_RGRD(val3);
	DMA_CR0_RGRD(val4);
	MAC_WTR_RGRD(val5);

	DBGPR("dbgpr_regs: DMA_CR4:%#x\n"
	      "dbgpr_regs: DMA_CR3:%#x\n"
	      "dbgpr_regs: DMA_CR2:%#x\n"
	      "dbgpr_regs: DMA_CR1:%#x\n"
	      "dbgpr_regs: DMA_CR0:%#x\n"
	      "dbgpr_regs: MAC_WTR:%#x\n", val0, val1, val2, val3, val4, val5);

	MAC_MPFR_RGRD(val0);
	MAC_MECR_RGRD(val1);
	MAC_MCR_RGRD(val2);

	DBGPR("dbgpr_regs: MAC_MPFR:%#x\n"
	      "dbgpr_regs: MAC_MECR:%#x\n"
	      "dbgpr_regs: MAC_MCR:%#x\n", val0, val1, val2);
}
#endif

/*!
 * \details This function is invoked by DWC_ETH_QOS_start_xmit and
 * DWC_ETH_QOS_tx_interrupt function for dumping the TX descriptor contents
 * which are prepared for packet transmission and which are transmitted by
 * device. It is mainly used during development phase for debug purpose. Use
 * of these function may affect the performance during normal operation.
 *
 * \param[in] pdata – pointer to private data structure.
 * \param[in] first_desc_idx – first descriptor index for the current
 *		transfer.
 * \param[in] last_desc_idx – last descriptor index for the current transfer.
 * \param[in] flag – to indicate from which function it is called.
 *
 * \return void
 */

void dump_tx_desc(struct DWC_ETH_QOS_prv_data *pdata, int first_desc_idx,
		  int last_desc_idx, int flag, UINT qinx)
{
	int i;
	struct s_TX_NORMAL_DESC *desc = NULL;
	UINT VARCTXT;

	if (first_desc_idx == last_desc_idx) {
		desc = GET_TX_DESC_PTR(qinx, first_desc_idx);

		TX_NORMAL_DESC_TDES3_CTXT_MLF_RD(desc->TDES3, VARCTXT);

		dev_alert(&pdata->pdev->dev, "\n%s[%02d %4p %03d %s] = %#x:%#x:%#x:%#x",
			  (VARCTXT == 1) ? "TX_CONTXT_DESC" : "TX_NORMAL_DESC",
		qinx, desc, first_desc_idx,
		((flag == 1) ? "QUEUED FOR TRANSMISSION" :
		((flag == 0) ? "FREED/FETCHED BY DEVICE" : "DEBUG DESC DUMP")),
		desc->TDES0, desc->TDES1,
		desc->TDES2, desc->TDES3);
	} else {
		int lp_cnt;

		if (first_desc_idx > last_desc_idx)
			lp_cnt = last_desc_idx + pdata->tx_queue[qinx].desc_cnt - first_desc_idx;
		else
			lp_cnt = last_desc_idx - first_desc_idx;

		for (i = first_desc_idx; lp_cnt >= 0; lp_cnt--) {
			desc = GET_TX_DESC_PTR(qinx, i);

			TX_NORMAL_DESC_TDES3_CTXT_MLF_RD(desc->TDES3, VARCTXT);

			dev_alert(&pdata->pdev->dev, "\n%s[%02d %4p %03d %s] = %#x:%#x:%#x:%#x",
				  (VARCTXT == 1) ? "TX_CONTXT_DESC" : "TX_NORMAL_DESC",
			 qinx, desc, i,
			 ((flag == 1) ? "QUEUED FOR TRANSMISSION" :
			 "FREED/FETCHED BY DEVICE"), desc->TDES0,
			 desc->TDES1, desc->TDES2, desc->TDES3);
			INCR_TX_DESC_INDEX(i, 1, pdata->tx_queue[qinx].desc_cnt);
		}
	}
}

/*!
 * \details This function is invoked by poll function for dumping the
 * RX descriptor contents. It is mainly used during development phase for
 * debug purpose. Use of these function may affect the performance during
 * normal operation
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */

void dump_rx_desc(UINT qinx, struct s_RX_NORMAL_DESC *desc, int desc_idx)
{
	pr_alert("\nRX_NORMAL_DESC[%02d %4p %03d RECEIVED FROM DEVICE]",
		 qinx, desc, desc_idx);
	pr_alert(" = %#x:%#x:%#x:%#x\n",
		 desc->RDES0, desc->RDES1, desc->RDES2, desc->RDES3);
}

/*!
 * \details This function is invoked by start_xmit and poll function for
 * dumping the content of packet to be transmitted by device or received
 * from device. It is mainly used during development phase for debug purpose.
 * Use of these functions may affect the performance during normal operation.
 *
 * \param[in] skb – pointer to socket buffer structure.
 * \param[in] len – length of packet to be transmitted/received.
 * \param[in] tx_rx – packet to be transmitted or received.
 * \param[in] desc_idx – descriptor index to be used for transmission or
 *			reception of packet.
 *
 * \return void
 */

void print_pkt(struct sk_buff *skb, int len, bool tx_rx, int desc_idx)
{
	int i, j = 0;
	unsigned char *buf = skb->data;

	pr_debug("\n\n/**************************/\n");

	pr_debug("%s pkt of %d Bytes [DESC index = %d]\n\n",
		 (tx_rx ? "TX" : "RX"), len, desc_idx);
	pr_debug("Dst MAC addr(6 bytes)\n");
	for (i = 0; i < 6; i++)
		pr_debug("%#.2x%s", buf[i], (((i == 5) ? "" : ":")));
	pr_debug("\nSrc MAC addr(6 bytes)\n");
	for (i = 6; i <= 11; i++)
		pr_debug("%#.2x%s", buf[i], (((i == 11) ? "" : ":")));
	i = (buf[12] << 8 | buf[13]);
	pr_debug("\nType/Length(2 bytes)\n%#x", i);

	pr_debug("\nPay Load : %d bytes\n", (len - 14));
	for (i = 14, j = 1; i < len; i++, j++) {
		pr_info("%#.2x%s", buf[i], (((i == (len - 1)) ? "" : ":")));
		if ((j % 16) == 0)
			pr_debug(" ");
	}

	pr_debug("/****************************/\n\n");
}

/*!
 * \details This function is invoked by probe function. This function will
 * initialize default receive coalesce parameters and sw timer value and store
 * it in respective receive data structure.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */

void DWC_ETH_QOS_init_rx_coalesce(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct DWC_ETH_QOS_rx_wrapper_descriptor *rx_desc_data = NULL;
	UINT i;

	DBGPR("-->DWC_ETH_QOS_init_rx_coalesce\n");

	for (i = 0; i < DWC_ETH_QOS_RX_QUEUE_CNT; i++) {
		rx_desc_data = GET_RX_WRAPPER_DESC(i);

		rx_desc_data->use_riwt = 1;
		rx_desc_data->rx_coal_frames = DWC_ETH_QOS_RX_MAX_FRAMES;
		if (pdata->ipa_enabled && i == IPA_DMA_RX_CH)
			rx_desc_data->rx_riwt = DWC_ETH_QOS_MAX_DMA_RIWT;
		else
			rx_desc_data->rx_riwt =
				DWC_ETH_QOS_usec2riwt
				(DWC_ETH_QOS_OPTIMAL_DMA_RIWT_USEC, pdata);
	}

	DBGPR("<--DWC_ETH_QOS_init_rx_coalesce\n");
}

/*!
 * \details This function is invoked by open() function. This function will
 * clear MMC structure.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */

static void DWC_ETH_QOS_mmc_setup(struct DWC_ETH_QOS_prv_data *pdata)
{
	DBGPR("-->DWC_ETH_QOS_mmc_setup\n");

	if (pdata->hw_feat.mmc_sel) {
		memset(&pdata->mmc, 0, sizeof(struct DWC_ETH_QOS_mmc_counters));
	} else
		dev_alert(&pdata->pdev->dev,
			  "No MMC/RMON module available in the HW\n");

	DBGPR("<--DWC_ETH_QOS_mmc_setup\n");
}

inline unsigned int DWC_ETH_QOS_reg_read(volatile ULONG *ptr)
{
		return ioread32((void *)ptr);
}

/*!
 * \details This function is invoked by ethtool function when user wants to
 * read MMC counters. This function will read the MMC if supported by core
 * and store it in DWC_ETH_QOS_mmc_counters structure. By default all the
 * MMC are programmed "read on reset" hence all the fields of the
 * DWC_ETH_QOS_mmc_counters are incremented.
 *
 * open() function. This function will
 * initialize MMC control register ie it disable all MMC interrupt and all
 * MMC register are configured to clear on read.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */

void DWC_ETH_QOS_mmc_read(struct DWC_ETH_QOS_mmc_counters *mmc)
{
	DBGPR("-->DWC_ETH_QOS_mmc_read\n");

	/* MMC TX counter registers */
	mmc->mmc_tx_octetcount_gb +=
		DWC_ETH_QOS_reg_read(MMC_TXOCTETCOUNT_GB_RGOFFADDR);
	mmc->mmc_tx_framecount_gb +=
		DWC_ETH_QOS_reg_read(MMC_TXPACKETCOUNT_GB_RGOFFADDR);
	mmc->mmc_tx_broadcastframe_g +=
		DWC_ETH_QOS_reg_read(MMC_TXBROADCASTPACKETS_G_RGOFFADDR);
	mmc->mmc_tx_multicastframe_g +=
		DWC_ETH_QOS_reg_read(MMC_TXMULTICASTPACKETS_G_RGOFFADDR);
	mmc->mmc_tx_64_octets_gb +=
		DWC_ETH_QOS_reg_read(MMC_TX64OCTETS_GB_RGOFFADDR);
	mmc->mmc_tx_65_to_127_octets_gb +=
		DWC_ETH_QOS_reg_read(MMC_TX65TO127OCTETS_GB_RGOFFADDR);
	mmc->mmc_tx_128_to_255_octets_gb +=
		DWC_ETH_QOS_reg_read(MMC_TX128TO255OCTETS_GB_RGOFFADDR);
	mmc->mmc_tx_256_to_511_octets_gb +=
		DWC_ETH_QOS_reg_read(MMC_TX256TO511OCTETS_GB_RGOFFADDR);
	mmc->mmc_tx_512_to_1023_octets_gb +=
		DWC_ETH_QOS_reg_read(MMC_TX512TO1023OCTETS_GB_RGOFFADDR);
	mmc->mmc_tx_1024_to_max_octets_gb +=
		DWC_ETH_QOS_reg_read(MMC_TX1024TOMAXOCTETS_GB_RGOFFADDR);
	mmc->mmc_tx_unicast_gb +=
		DWC_ETH_QOS_reg_read(MMC_TXUNICASTPACKETS_GB_RGOFFADDR);
	mmc->mmc_tx_multicast_gb +=
		DWC_ETH_QOS_reg_read(MMC_TXMULTICASTPACKETS_GB_RGOFFADDR);
	mmc->mmc_tx_broadcast_gb +=
		DWC_ETH_QOS_reg_read(MMC_TXBROADCASTPACKETS_GB_RGOFFADDR);
	mmc->mmc_tx_underflow_error +=
		DWC_ETH_QOS_reg_read(MMC_TXUNDERFLOWERROR_RGOFFADDR);
	mmc->mmc_tx_singlecol_g +=
		DWC_ETH_QOS_reg_read(MMC_TXSINGLECOL_G_RGOFFADDR);
	mmc->mmc_tx_multicol_g +=
		DWC_ETH_QOS_reg_read(MMC_TXMULTICOL_G_RGOFFADDR);
	mmc->mmc_tx_deferred +=
		DWC_ETH_QOS_reg_read(MMC_TXDEFERRED_RGOFFADDR);
	mmc->mmc_tx_latecol +=
		DWC_ETH_QOS_reg_read(MMC_TXLATECOL_RGOFFADDR);
	mmc->mmc_tx_exesscol +=
		DWC_ETH_QOS_reg_read(MMC_TXEXESSCOL_RGOFFADDR);
	mmc->mmc_tx_carrier_error +=
		DWC_ETH_QOS_reg_read(MMC_TXCARRIERERROR_RGOFFADDR);
	mmc->mmc_tx_octetcount_g +=
		DWC_ETH_QOS_reg_read(MMC_TXOCTETCOUNT_G_RGOFFADDR);
	mmc->mmc_tx_framecount_g +=
		DWC_ETH_QOS_reg_read(MMC_TXPACKETSCOUNT_G_RGOFFADDR);
	mmc->mmc_tx_excessdef +=
		DWC_ETH_QOS_reg_read(MMC_TXEXCESSDEF_RGOFFADDR);
	mmc->mmc_tx_pause_frame +=
		DWC_ETH_QOS_reg_read(MMC_TXPAUSEPACKETS_RGOFFADDR);
	mmc->mmc_tx_vlan_frame_g +=
		DWC_ETH_QOS_reg_read(MMC_TXVLANPACKETS_G_RGOFFADDR);
	mmc->mmc_tx_osize_frame_g +=
		DWC_ETH_QOS_reg_read(MMC_TXOVERSIZE_G_RGOFFADDR);

	/* MMC RX counter registers */
	mmc->mmc_rx_framecount_gb +=
		DWC_ETH_QOS_reg_read(MMC_RXPACKETCOUNT_GB_RGOFFADDR);
	mmc->mmc_rx_octetcount_gb +=
		DWC_ETH_QOS_reg_read(MMC_RXOCTETCOUNT_GB_RGOFFADDR);
	mmc->mmc_rx_octetcount_g +=
		DWC_ETH_QOS_reg_read(MMC_RXOCTETCOUNT_G_RGOFFADDR);
	mmc->mmc_rx_broadcastframe_g +=
		DWC_ETH_QOS_reg_read(MMC_RXBROADCASTPACKETS_G_RGOFFADDR);
	mmc->mmc_rx_multicastframe_g +=
		DWC_ETH_QOS_reg_read(MMC_RXMULTICASTPACKETS_G_RGOFFADDR);
	mmc->mmc_rx_crc_errror +=
		DWC_ETH_QOS_reg_read(MMC_RXCRCERROR_RGOFFADDR);
	mmc->mmc_rx_align_error +=
		DWC_ETH_QOS_reg_read(MMC_RXALIGNMENTERROR_RGOFFADDR);
	mmc->mmc_rx_run_error +=
		DWC_ETH_QOS_reg_read(MMC_RXRUNTERROR_RGOFFADDR);
	mmc->mmc_rx_jabber_error +=
		DWC_ETH_QOS_reg_read(MMC_RXJABBERERROR_RGOFFADDR);
	mmc->mmc_rx_undersize_g +=
		DWC_ETH_QOS_reg_read(MMC_RXUNDERSIZE_G_RGOFFADDR);
	mmc->mmc_rx_oversize_g +=
		DWC_ETH_QOS_reg_read(MMC_RXOVERSIZE_G_RGOFFADDR);
	mmc->mmc_rx_64_octets_gb +=
		DWC_ETH_QOS_reg_read(MMC_RX64OCTETS_GB_RGOFFADDR);
	mmc->mmc_rx_65_to_127_octets_gb +=
		DWC_ETH_QOS_reg_read(MMC_RX65TO127OCTETS_GB_RGOFFADDR);
	mmc->mmc_rx_128_to_255_octets_gb +=
		DWC_ETH_QOS_reg_read(MMC_RX128TO255OCTETS_GB_RGOFFADDR);
	mmc->mmc_rx_256_to_511_octets_gb +=
		DWC_ETH_QOS_reg_read(MMC_RX256TO511OCTETS_GB_RGOFFADDR);
	mmc->mmc_rx_512_to_1023_octets_gb +=
		DWC_ETH_QOS_reg_read(MMC_RX512TO1023OCTETS_GB_RGOFFADDR);
	mmc->mmc_rx_1024_to_max_octets_gb +=
		DWC_ETH_QOS_reg_read(MMC_RX1024TOMAXOCTETS_GB_RGOFFADDR);
	mmc->mmc_rx_unicast_g +=
		DWC_ETH_QOS_reg_read(MMC_RXUNICASTPACKETS_G_RGOFFADDR);
	mmc->mmc_rx_length_error +=
		DWC_ETH_QOS_reg_read(MMC_RXLENGTHERROR_RGOFFADDR);
	mmc->mmc_rx_outofrangetype +=
		DWC_ETH_QOS_reg_read(MMC_RXOUTOFRANGETYPE_RGOFFADDR);
	mmc->mmc_rx_pause_frames +=
		DWC_ETH_QOS_reg_read(MMC_RXPAUSEPACKETS_RGOFFADDR);
	mmc->mmc_rx_fifo_overflow +=
		DWC_ETH_QOS_reg_read(MMC_RXFIFOOVERFLOW_RGOFFADDR);
	mmc->mmc_rx_vlan_frames_gb +=
		DWC_ETH_QOS_reg_read(MMC_RXVLANPACKETS_GB_RGOFFADDR);
	mmc->mmc_rx_watchdog_error +=
		DWC_ETH_QOS_reg_read(MMC_RXWATCHDOGERROR_RGOFFADDR);
	mmc->mmc_rx_receive_error +=
		DWC_ETH_QOS_reg_read(MMC_RXRCVERROR_RGOFFADDR);
	mmc->mmc_rx_ctrl_frames_g +=
		DWC_ETH_QOS_reg_read(MMC_RXCTRLPACKETS_G_RGOFFADDR);

	/* IPC */
	mmc->mmc_rx_ipc_intr_mask +=
		DWC_ETH_QOS_reg_read(MMC_IPC_INTR_MASK_RX_RGOFFADDR);
	mmc->mmc_rx_ipc_intr +=
		DWC_ETH_QOS_reg_read(MMC_IPC_INTR_RX_RGOFFADDR);

	/* IPv4 */
	mmc->mmc_rx_ipv4_gd +=
		DWC_ETH_QOS_reg_read(MMC_RXIPV4_GD_PKTS_RGOFFADDR);
	mmc->mmc_rx_ipv4_hderr +=
		DWC_ETH_QOS_reg_read(MMC_RXIPV4_HDRERR_PKTS_RGOFFADDR);
	mmc->mmc_rx_ipv4_nopay +=
		DWC_ETH_QOS_reg_read(MMC_RXIPV4_NOPAY_PKTS_RGOFFADDR);
	mmc->mmc_rx_ipv4_frag +=
		DWC_ETH_QOS_reg_read(MMC_RXIPV4_FRAG_PKTS_RGOFFADDR);
	mmc->mmc_rx_ipv4_udp_csum_disable +=
		DWC_ETH_QOS_reg_read(MMC_RXIPV4_UBSBL_PKTS_RGOFFADDR);

	/* IPV6 */
	mmc->mmc_rx_ipv6_gd +=
		DWC_ETH_QOS_reg_read(MMC_RXIPV6_GD_PKTS_RGOFFADDR);
	mmc->mmc_rx_ipv6_hderr +=
		DWC_ETH_QOS_reg_read(MMC_RXIPV6_HDRERR_PKTS_RGOFFADDR);
	mmc->mmc_rx_ipv6_nopay +=
		DWC_ETH_QOS_reg_read(MMC_RXIPV6_NOPAY_PKTS_RGOFFADDR);

	/* Protocols */
	mmc->mmc_rx_udp_gd +=
		DWC_ETH_QOS_reg_read(MMC_RXUDP_GD_PKTS_RGOFFADDR);
	mmc->mmc_rx_udp_csum_err +=
		DWC_ETH_QOS_reg_read(MMC_RXUDP_ERR_PKTS_RGOFFADDR);
	mmc->mmc_rx_tcp_gd +=
		DWC_ETH_QOS_reg_read(MMC_RXTCP_GD_PKTS_RGOFFADDR);
	mmc->mmc_rx_tcp_csum_err +=
		DWC_ETH_QOS_reg_read(MMC_RXTCP_ERR_PKTS_RGOFFADDR);
	mmc->mmc_rx_icmp_gd +=
		DWC_ETH_QOS_reg_read(MMC_RXICMP_GD_PKTS_RGOFFADDR);
	mmc->mmc_rx_icmp_csum_err +=
		DWC_ETH_QOS_reg_read(MMC_RXICMP_ERR_PKTS_RGOFFADDR);

	/* IPv4 */
	mmc->mmc_rx_ipv4_gd_octets +=
		DWC_ETH_QOS_reg_read(MMC_RXIPV4_GD_OCTETS_RGOFFADDR);
	mmc->mmc_rx_ipv4_hderr_octets +=
		DWC_ETH_QOS_reg_read(MMC_RXIPV4_HDRERR_OCTETS_RGOFFADDR);
	mmc->mmc_rx_ipv4_nopay_octets +=
		DWC_ETH_QOS_reg_read(MMC_RXIPV4_NOPAY_OCTETS_RGOFFADDR);
	mmc->mmc_rx_ipv4_frag_octets +=
		DWC_ETH_QOS_reg_read(MMC_RXIPV4_FRAG_OCTETS_RGOFFADDR);
	mmc->mmc_rx_ipv4_udp_csum_dis_octets +=
		DWC_ETH_QOS_reg_read(MMC_RXIPV4_UDSBL_OCTETS_RGOFFADDR);

	/* IPV6 */
	mmc->mmc_rx_ipv6_gd_octets +=
		DWC_ETH_QOS_reg_read(MMC_RXIPV6_GD_OCTETS_RGOFFADDR);
	mmc->mmc_rx_ipv6_hderr_octets +=
		DWC_ETH_QOS_reg_read(MMC_RXIPV6_HDRERR_OCTETS_RGOFFADDR);
	mmc->mmc_rx_ipv6_nopay_octets +=
		DWC_ETH_QOS_reg_read(MMC_RXIPV6_NOPAY_OCTETS_RGOFFADDR);

	/* Protocols */
	mmc->mmc_rx_udp_gd_octets +=
		DWC_ETH_QOS_reg_read(MMC_RXUDP_GD_OCTETS_RGOFFADDR);
	mmc->mmc_rx_udp_csum_err_octets +=
		DWC_ETH_QOS_reg_read(MMC_RXUDP_ERR_OCTETS_RGOFFADDR);
	mmc->mmc_rx_tcp_gd_octets +=
		DWC_ETH_QOS_reg_read(MMC_RXTCP_GD_OCTETS_RGOFFADDR);
	mmc->mmc_rx_tcp_csum_err_octets +=
		DWC_ETH_QOS_reg_read(MMC_RXTCP_ERR_OCTETS_RGOFFADDR);
	mmc->mmc_rx_icmp_gd_octets +=
		DWC_ETH_QOS_reg_read(MMC_RXICMP_GD_OCTETS_RGOFFADDR);
	mmc->mmc_rx_icmp_csum_err_octets +=
		DWC_ETH_QOS_reg_read(MMC_RXICMP_ERR_OCTETS_RGOFFADDR);

	/* LPI Rx and Tx Transition counters */
	mmc->mmc_emac_rx_lpi_tran_cntr +=
		DWC_ETH_QOS_reg_read(MMC_EMAC_RX_LPI_TRAN_CNTR_RGOFFADDR);
	mmc->mmc_emac_tx_lpi_tran_cntr +=
		DWC_ETH_QOS_reg_read(MMC_EMAC_TX_LPI_TRAN_CNTR_RGOFFADDR);

	DBGPR("<--DWC_ETH_QOS_mmc_read\n");
}

phy_interface_t DWC_ETH_QOS_get_io_macro_phy_interface(
	struct DWC_ETH_QOS_prv_data *pdata)
{
	phy_interface_t ret = PHY_INTERFACE_MODE_MII;

	EMACDBG("-->DWC_ETH_QOS_get_io_macro_phy_interface\n");

	if (pdata->io_macro_phy_intf == RGMII_MODE)
		ret = PHY_INTERFACE_MODE_RGMII;
	else if (pdata->io_macro_phy_intf == RMII_MODE)
		ret = PHY_INTERFACE_MODE_RMII;
	else if (pdata->io_macro_phy_intf == MII_MODE)
		ret = PHY_INTERFACE_MODE_MII;

	EMACDBG("<--DWC_ETH_QOS_get_io_macro_phy_interface\n");

	return ret;
}

phy_interface_t DWC_ETH_QOS_get_phy_interface(
	struct DWC_ETH_QOS_prv_data *pdata)
{
	phy_interface_t ret = PHY_INTERFACE_MODE_MII;

	DBGPR("-->DWC_ETH_QOS_get_phy_interface\n");

	if (pdata->hw_feat.act_phy_sel == DWC_ETH_QOS_GMII_MII) {
		if (pdata->hw_feat.gmii_sel)
			ret = PHY_INTERFACE_MODE_GMII;
		else if (pdata->hw_feat.mii_sel)
			ret = PHY_INTERFACE_MODE_MII;
	} else if (pdata->hw_feat.act_phy_sel == DWC_ETH_QOS_RGMII) {
		ret = PHY_INTERFACE_MODE_RGMII;
	} else if (pdata->hw_feat.act_phy_sel == DWC_ETH_QOS_SGMII) {
		ret = PHY_INTERFACE_MODE_SGMII;
	} else if (pdata->hw_feat.act_phy_sel == DWC_ETH_QOS_TBI) {
		ret = PHY_INTERFACE_MODE_TBI;
	} else if (pdata->hw_feat.act_phy_sel == DWC_ETH_QOS_RMII) {
		ret = PHY_INTERFACE_MODE_RMII;
	} else if (pdata->hw_feat.act_phy_sel == DWC_ETH_QOS_RTBI) {
		ret = PHY_INTERFACE_MODE_RTBI;
	} else if (pdata->hw_feat.act_phy_sel == DWC_ETH_QOS_SMII) {
		ret = PHY_INTERFACE_MODE_SMII;
	} else if (pdata->hw_feat.act_phy_sel == DWC_ETH_QOS_REVMII) {
		/* what to return ? */
	} else {
		dev_alert(&pdata->pdev->dev,
			  "Missing interface support between PHY and MAC\n\n");
		ret = PHY_INTERFACE_MODE_NA;
	}

	DBGPR("<--DWC_ETH_QOS_get_phy_interface\n");

	return ret;
}

/*!
 * \details This function is invoked by ethtool function when user wants to
 * read the DMA descriptor stats.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \return void
 */
void DWC_ETH_QOS_dma_desc_stats_read(struct DWC_ETH_QOS_prv_data *pdata)
{
	int qinx;
	EMACDBG("Enter\n");

	pdata->xstats.dma_ch_intr_status = DWC_ETH_QOS_reg_read(DMA_ISR_RGOFFADDR);
	pdata->xstats.dma_debug_status0 = DWC_ETH_QOS_reg_read(DMA_DSR0_RGOFFADDR);
	pdata->xstats.dma_debug_status1 = DWC_ETH_QOS_reg_read(DMA_DSR1_RGOFFADDR);

	for (qinx = 0; qinx < DWC_ETH_QOS_TX_QUEUE_CNT; qinx++) {
		if (pdata->ipa_enabled && qinx == IPA_DMA_TX_CH)
			continue;
		pdata->xstats.dma_ch_status[qinx] = DWC_ETH_QOS_reg_read(DMA_SR_RGOFFADDRESS(qinx));
		pdata->xstats.dma_ch_intr_enable[qinx] = DWC_ETH_QOS_reg_read(DMA_IER_RGOFFADDRESS(qinx));
		pdata->xstats.dma_ch_tx_control[qinx] = DWC_ETH_QOS_reg_read(DMA_TCR_RGOFFADDRESS(qinx));
		pdata->xstats.dma_ch_txdesc_list_addr[qinx] = DWC_ETH_QOS_reg_read(DMA_TDLAR_RGOFFADDRESS(qinx));
		pdata->xstats.dma_ch_txdesc_ring_len[qinx] = DWC_ETH_QOS_reg_read(DMA_TDRLR_RGOFFADDRESS(qinx));
		pdata->xstats.dma_ch_curr_app_txdesc[qinx] = DWC_ETH_QOS_reg_read(DMA_CHTDR_RGOFFADDRESS(qinx));
		pdata->xstats.dma_ch_txdesc_tail_ptr[qinx] = DWC_ETH_QOS_reg_read(DMA_TDTP_TPDR_RGOFFADDRESS(qinx));
		pdata->xstats.dma_ch_curr_app_txbuf[qinx] = DWC_ETH_QOS_reg_read(DMA_CHTBAR_RGOFFADDRESS(qinx));
	}

	for (qinx = 0; qinx < DWC_ETH_QOS_RX_QUEUE_CNT; qinx++) {
		if (pdata->ipa_enabled && qinx == IPA_DMA_RX_CH)
			continue;
		pdata->xstats.dma_ch_rx_control[qinx] = DWC_ETH_QOS_reg_read(DMA_RCR_RGOFFADDRESS(qinx));
		pdata->xstats.dma_ch_rxdesc_list_addr[qinx] = DWC_ETH_QOS_reg_read(DMA_RDLAR_RGOFFADDRESS(qinx));
		pdata->xstats.dma_ch_rxdesc_ring_len[qinx] = DWC_ETH_QOS_reg_read(DMA_RDRLR_RGOFFADDRESS(qinx));
		pdata->xstats.dma_ch_curr_app_rxdesc[qinx] = DWC_ETH_QOS_reg_read(DMA_CHRDR_RGOFFADDRESS(qinx));
		pdata->xstats.dma_ch_rxdesc_tail_ptr[qinx] = DWC_ETH_QOS_reg_read(DMA_RDTP_RPDR_RGOFFADDRESS(qinx));
		pdata->xstats.dma_ch_curr_app_rxbuf[qinx] = DWC_ETH_QOS_reg_read(DMA_CHRBAR_RGOFFADDRESS(qinx));
		pdata->xstats.dma_ch_miss_frame_count[qinx] = DWC_ETH_QOS_reg_read(DMA_CH_MISS_FRAME_CNT_RGOFFADDRESS(qinx));
	}
	EMACDBG("Exit\n");
}

/*!
 * \details This function is invoked by probe function to
 * initialize the ethtool descriptor stats
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \return void
 */
void DWC_ETH_QOS_dma_desc_stats_init(struct DWC_ETH_QOS_prv_data *pdata)
{
	int qinx;
	EMACDBG("Enter\n");

	pdata->xstats.dma_ch_intr_status = 0;
	pdata->xstats.dma_debug_status0 = 0;
	pdata->xstats.dma_debug_status1 = 0;

	for (qinx = 0; qinx < DWC_ETH_QOS_TX_QUEUE_CNT; qinx++) {
		pdata->xstats.dma_ch_status[qinx] = 0;
		pdata->xstats.dma_ch_intr_enable[qinx] = 0;
		pdata->xstats.dma_ch_tx_control[qinx] = 0;
		pdata->xstats.dma_ch_txdesc_list_addr[qinx] = 0;
		pdata->xstats.dma_ch_txdesc_ring_len[qinx] = 0;
		pdata->xstats.dma_ch_curr_app_txdesc[qinx] = 0;
		pdata->xstats.dma_ch_txdesc_tail_ptr[qinx] = 0;
		pdata->xstats.dma_ch_curr_app_txbuf[qinx] = 0;
	}

	for (qinx = 0; qinx < DWC_ETH_QOS_RX_QUEUE_CNT; qinx++) {
		pdata->xstats.dma_ch_rx_control[qinx] = 0;
		pdata->xstats.dma_ch_rxdesc_list_addr[qinx] = 0;
		pdata->xstats.dma_ch_rxdesc_ring_len[qinx] = 0;
		pdata->xstats.dma_ch_curr_app_rxdesc[qinx] = 0;
		pdata->xstats.dma_ch_rxdesc_tail_ptr[qinx] = 0;
		pdata->xstats.dma_ch_curr_app_rxbuf[qinx] = 0;
		pdata->xstats.dma_ch_miss_frame_count[qinx] = 0;
	}
	EMACDBG("Exit\n");
}
