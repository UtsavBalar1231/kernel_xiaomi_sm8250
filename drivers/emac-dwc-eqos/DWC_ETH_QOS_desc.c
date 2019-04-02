/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

/*!@file: DWC_ETH_QOS_desc.c
 * @brief: Driver functions.
 */
#include "DWC_ETH_QOS_yheader.h"
#include "DWC_ETH_QOS_desc.h"
#include "DWC_ETH_QOS_yregacc.h"

/*!
 * \brief API to free the transmit descriptor memory.
 *
 * \details This function is used to free the transmit descriptor memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \retval void.
 */

static void DWC_ETH_QOS_tx_desc_free_mem(struct DWC_ETH_QOS_prv_data *pdata,
					 UINT tx_qcnt)
{
	struct DWC_ETH_QOS_tx_wrapper_descriptor *desc_data = NULL;
	UINT qinx;

	DBGPR("-->DWC_ETH_QOS_tx_desc_free_mem: tx_qcnt = %d\n", tx_qcnt);

	for (qinx = 0; qinx < tx_qcnt; qinx++) {
		desc_data = GET_TX_WRAPPER_DESC(qinx);

		if (GET_TX_DESC_PTR(qinx, 0)) {
			dma_free_coherent(
			   GET_MEM_PDEV_DEV,
			   (sizeof(struct s_TX_NORMAL_DESC) * pdata->tx_queue[qinx].desc_cnt),
			   GET_TX_DESC_PTR(qinx, 0),
			   GET_TX_DESC_DMA_ADDR(qinx, 0));
			GET_TX_DESC_PTR(qinx, 0) = NULL;
		}
	}

	DBGPR("<--DWC_ETH_QOS_tx_desc_free_mem\n");
}

/*!
 * \brief API to free the receive descriptor memory.
 *
 * \details This function is used to free the receive descriptor memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \retval void.
 */

static void DWC_ETH_QOS_rx_desc_free_mem(struct DWC_ETH_QOS_prv_data *pdata,
					 UINT rx_qcnt)
{
	struct DWC_ETH_QOS_rx_wrapper_descriptor *desc_data = NULL;
	UINT qinx = 0;

	DBGPR("-->DWC_ETH_QOS_rx_desc_free_mem: rx_qcnt = %d\n", rx_qcnt);

	for (qinx = 0; qinx < rx_qcnt; qinx++) {
		desc_data = GET_RX_WRAPPER_DESC(qinx);

		if (GET_RX_DESC_PTR(qinx, 0)) {
			dma_free_coherent(
			   GET_MEM_PDEV_DEV,
			   (sizeof(struct s_RX_NORMAL_DESC) * pdata->rx_queue[qinx].desc_cnt),
			   GET_RX_DESC_PTR(qinx, 0),
			   GET_RX_DESC_DMA_ADDR(qinx, 0));
			GET_RX_DESC_PTR(qinx, 0) = NULL;
		}
	}

	DBGPR("<--DWC_ETH_QOS_rx_desc_free_mem\n");
}

static int DWC_ETH_QOS_alloc_rx_queue_struct(struct DWC_ETH_QOS_prv_data *pdata)
{
	int ret = 0, chInx, cnt;
	struct DWC_ETH_QOS_rx_wrapper_descriptor *rx_desc_data;

	DBGPR("rx_queue_cnt = %d\n", pdata->rx_queue_cnt);

	pdata->rx_queue =
		kzalloc(sizeof(struct DWC_ETH_QOS_rx_queue) * pdata->rx_queue_cnt,
		GFP_KERNEL);
	if (pdata->rx_queue == NULL) {
		EMACERR("ERROR: Unable to allocate Rx queue structure\n");
		ret = -ENOMEM;
		goto err_out_rx_q_alloc_failed;
	}
	for (chInx = 0; chInx < pdata->rx_queue_cnt; chInx++) {
		pdata->rx_queue[chInx].desc_cnt = RX_DESC_CNT;

		if (pdata->ipa_enabled && chInx == IPA_DMA_RX_CH)
			pdata->rx_queue[chInx].desc_cnt = IPA_RX_DESC_CNT;

		rx_desc_data = &pdata->rx_queue[chInx].rx_desc_data;

		/* Alocate rx_desc_ptrs */
		rx_desc_data->rx_desc_ptrs =
			kzalloc(sizeof(struct s_RX_NORMAL_DESC *) * pdata->rx_queue[chInx].desc_cnt,
					GFP_KERNEL);
		if (rx_desc_data->rx_desc_ptrs == NULL) {
			EMACERR("ERROR: Unable to allocate Rx Desc ptrs\n");
			ret = -ENOMEM;
			goto err_out_rx_desc_ptrs_failed;
		}

		for (cnt = 0; cnt <pdata->rx_queue[chInx].desc_cnt; cnt++) {
			rx_desc_data->rx_desc_ptrs[cnt] = rx_desc_data->rx_desc_ptrs[0] +
				(sizeof(struct s_RX_NORMAL_DESC *) * cnt);
		}

		/* Alocate rx_desc_dma_addrs */
		rx_desc_data->rx_desc_dma_addrs =
			kzalloc(sizeof(dma_addr_t) *pdata->rx_queue[chInx].desc_cnt,
					GFP_KERNEL);
		if (rx_desc_data->rx_desc_dma_addrs == NULL) {
			EMACERR("ERROR: Unable to allocate Rx Desc dma addr\n");
			ret = -ENOMEM;
			goto err_out_rx_desc_dma_addrs_failed;
		}

		for (cnt = 0; cnt <pdata->rx_queue[chInx].desc_cnt; cnt++) {
			rx_desc_data->rx_desc_dma_addrs[cnt] = rx_desc_data->rx_desc_dma_addrs[0] +
				(sizeof(dma_addr_t) * cnt);
		}

		/* Alocate rx_buf_ptrs */
		rx_desc_data->rx_buf_ptrs =
			kzalloc(sizeof(struct DWC_ETH_QOS_rx_buffer *) * pdata->rx_queue[chInx].desc_cnt,
					GFP_KERNEL);
		if (rx_desc_data->rx_buf_ptrs == NULL) {
			EMACERR("ERROR: Unable to allocate Rx Desc dma addr\n");
			ret = -ENOMEM;
			goto err_out_rx_buf_ptrs_failed;
		}

		for (cnt = 0; cnt <pdata->rx_queue[chInx].desc_cnt; cnt++) {
			rx_desc_data->rx_buf_ptrs[cnt] = rx_desc_data->rx_buf_ptrs[0] +
				(sizeof(struct DWC_ETH_QOS_rx_buffer *) * cnt);
		}

		if (pdata->ipa_enabled) {
			/* Allocate ipa_rx_buff_pool_va_addrs_base */
			rx_desc_data->ipa_rx_buff_pool_va_addrs_base =
				kzalloc(sizeof(void *) * pdata->rx_queue[chInx].desc_cnt, GFP_KERNEL);
			if (rx_desc_data->ipa_rx_buff_pool_va_addrs_base == NULL) {
				EMACERR("ERROR: Unable to allocate Rx ipa buff addrs\n");
				ret = -ENOMEM;
				goto err_out_rx_ipa_buff_addrs_failed;
			}
		}
	}

	DBGPR("<--DWC_ETH_QOS_alloc_rx_queue_struct\n");
	return ret;

err_out_rx_ipa_buff_addrs_failed:
	for (chInx = 0; chInx < pdata->rx_queue_cnt; chInx++) {
		rx_desc_data = &pdata->rx_queue[chInx].rx_desc_data;
		if (rx_desc_data->rx_buf_ptrs) {
			kfree(rx_desc_data->rx_buf_ptrs);
			rx_desc_data->rx_buf_ptrs = NULL;
		}
	}

err_out_rx_buf_ptrs_failed:
	for (chInx = 0; chInx < pdata->rx_queue_cnt; chInx++) {
		rx_desc_data = &pdata->rx_queue[chInx].rx_desc_data;
		if (rx_desc_data->rx_desc_dma_addrs) {
			kfree(rx_desc_data->rx_desc_dma_addrs);
			rx_desc_data->rx_desc_dma_addrs = NULL;
		}
	}

err_out_rx_desc_dma_addrs_failed:
	for (chInx = 0; chInx < pdata->rx_queue_cnt; chInx++) {
		rx_desc_data = &pdata->rx_queue[chInx].rx_desc_data;
		if (rx_desc_data->rx_desc_ptrs) {
			kfree(rx_desc_data->rx_desc_ptrs);
			rx_desc_data->rx_desc_ptrs = NULL;
		}
	}

err_out_rx_desc_ptrs_failed:
	kfree(pdata->rx_queue);
	pdata->rx_queue = NULL;

err_out_rx_q_alloc_failed:
	return ret;
}

static void DWC_ETH_QOS_free_rx_queue_struct(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct DWC_ETH_QOS_rx_wrapper_descriptor *rx_desc_data;
	int chInx;

	for (chInx = 0; chInx < pdata->rx_queue_cnt; chInx++) {
		rx_desc_data = &pdata->rx_queue[chInx].rx_desc_data;
		if (rx_desc_data->rx_desc_dma_addrs) {
			kfree(rx_desc_data->rx_desc_dma_addrs);
			rx_desc_data->rx_desc_dma_addrs = NULL;
		}

		if (rx_desc_data->rx_desc_ptrs) {
			kfree(rx_desc_data->rx_desc_ptrs);
			rx_desc_data->rx_desc_ptrs = NULL;
		}

		if (pdata->ipa_enabled && chInx == IPA_DMA_RX_CH) {
			if (rx_desc_data->ipa_rx_buff_pool_va_addrs_base){
				kfree(rx_desc_data->ipa_rx_buff_pool_va_addrs_base);
				rx_desc_data->ipa_rx_buff_pool_va_addrs_base = NULL;
			}
		}
	}
}

static int DWC_ETH_QOS_alloc_tx_queue_struct(struct DWC_ETH_QOS_prv_data *pdata)
{
	int ret = 0, chInx, cnt;
	struct DWC_ETH_QOS_tx_wrapper_descriptor *tx_desc_data;

	DBGPR("tx_queue_cnt = %d", pdata->tx_queue_cnt);

	pdata->tx_queue =
		kzalloc(sizeof(struct DWC_ETH_QOS_tx_queue) * pdata->tx_queue_cnt,
		GFP_KERNEL);
	if (pdata->tx_queue == NULL) {
		EMACERR("ERROR: Unable to allocate Tx queue structure\n");
		ret = -ENOMEM;
		goto err_out_tx_q_alloc_failed;
	}
	for (chInx = 0; chInx < pdata->tx_queue_cnt; chInx++) {
		pdata->tx_queue[chInx].desc_cnt = TX_DESC_CNT;

		if (pdata->ipa_enabled && chInx == IPA_DMA_TX_CH)
			pdata->tx_queue[chInx].desc_cnt = IPA_TX_DESC_CNT;

		tx_desc_data = &pdata->tx_queue[chInx].tx_desc_data;

		/* Allocate tx_desc_ptrs */
		tx_desc_data->tx_desc_ptrs =
			kzalloc(sizeof(struct s_TX_NORMAL_DESC *) * pdata->tx_queue[chInx].desc_cnt,
					GFP_KERNEL);
		if (tx_desc_data->tx_desc_ptrs == NULL) {
			EMACERR("ERROR: Unable to allocate Tx Desc ptrs\n");
			ret = -ENOMEM;
			goto err_out_tx_desc_ptrs_failed;
		}
		for (cnt = 0; cnt < pdata->tx_queue[chInx].desc_cnt; cnt++) {
			tx_desc_data->tx_desc_ptrs[cnt] = tx_desc_data->tx_desc_ptrs[0] +
				(sizeof(struct s_TX_NORMAL_DESC *) * cnt);
		}

		/* Allocate tx_desc_dma_addrs */
		tx_desc_data->tx_desc_dma_addrs =
			kzalloc(sizeof(dma_addr_t) * pdata->tx_queue[chInx].desc_cnt,
					GFP_KERNEL);
		if (tx_desc_data->tx_desc_dma_addrs == NULL) {
			EMACERR("ERROR: Unable to allocate Tx Desc dma addrs\n");
			ret = -ENOMEM;
			goto err_out_tx_desc_dma_addrs_failed;
		}
		for (cnt = 0; cnt < pdata->tx_queue[chInx].desc_cnt; cnt++) {
			tx_desc_data->tx_desc_dma_addrs[cnt] = tx_desc_data->tx_desc_dma_addrs[0] +
				(sizeof(dma_addr_t) * cnt);
		}

		/* Allocate tx_buf_ptrs */
		tx_desc_data->tx_buf_ptrs =
			kzalloc(sizeof(struct DWC_ETH_QOS_tx_buffer *) * pdata->tx_queue[chInx].desc_cnt,
					GFP_KERNEL);
		if (tx_desc_data->tx_buf_ptrs == NULL) {
			EMACERR("ERROR: Unable to allocate Tx buff ptrs\n");
			ret = -ENOMEM;
			goto err_out_tx_buf_ptrs_failed;
		}
		for (cnt = 0; cnt < pdata->tx_queue[chInx].desc_cnt; cnt++) {
			tx_desc_data->tx_buf_ptrs[cnt] = tx_desc_data->tx_buf_ptrs[0] +
				(sizeof(struct DWC_ETH_QOS_tx_buffer *) * cnt);
		}

		if (pdata->ipa_enabled) {
			/* Allocate ipa_tx_buff_pool_va_addrs_base */
			tx_desc_data->ipa_tx_buff_pool_va_addrs_base =
				kzalloc(sizeof(void *) * pdata->tx_queue[chInx].desc_cnt,GFP_KERNEL);
			if (tx_desc_data->ipa_tx_buff_pool_va_addrs_base == NULL) {
				EMACERR("ERROR: Unable to allocate Tx ipa buff addrs\n");
				ret = -ENOMEM;
				goto err_out_tx_ipa_buff_addrs_failed;
			}
		}
	}

	DBGPR("<--DWC_ETH_QOS_alloc_tx_queue_struct\n");
	return ret;

err_out_tx_ipa_buff_addrs_failed:
	for (chInx = 0; chInx < pdata->tx_queue_cnt; chInx++) {
		tx_desc_data = &pdata->tx_queue[chInx].tx_desc_data;
		if (tx_desc_data->tx_buf_ptrs) {
			kfree(tx_desc_data->tx_buf_ptrs);
			tx_desc_data->tx_buf_ptrs = NULL;
		}
	}

err_out_tx_buf_ptrs_failed:
	for (chInx = 0; chInx < pdata->tx_queue_cnt; chInx++) {
		tx_desc_data = &pdata->tx_queue[chInx].tx_desc_data;
		if (tx_desc_data->tx_desc_dma_addrs) {
			kfree(tx_desc_data->tx_desc_dma_addrs);
			tx_desc_data->tx_desc_dma_addrs = NULL;
		}
	}

err_out_tx_desc_dma_addrs_failed:
	for (chInx = 0; chInx < pdata->tx_queue_cnt; chInx++) {
		tx_desc_data = &pdata->tx_queue[chInx].tx_desc_data;
		if (tx_desc_data->tx_desc_ptrs) {
			kfree(tx_desc_data->tx_desc_ptrs);
			tx_desc_data->tx_desc_ptrs = NULL;
		}
	}

err_out_tx_desc_ptrs_failed:
	kfree(pdata->tx_queue);
	pdata->tx_queue = NULL;

err_out_tx_q_alloc_failed:
	return ret;
}

static void DWC_ETH_QOS_free_tx_queue_struct(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct DWC_ETH_QOS_tx_wrapper_descriptor *tx_desc_data;
	int chInx;

	for (chInx = 0; chInx < pdata->tx_queue_cnt; chInx++) {
		tx_desc_data = &pdata->tx_queue[chInx].tx_desc_data;

		if (tx_desc_data->tx_buf_ptrs) {
			kfree(tx_desc_data->tx_buf_ptrs);
			tx_desc_data->tx_buf_ptrs = NULL;
		}

		if (tx_desc_data->tx_desc_dma_addrs) {
			kfree(tx_desc_data->tx_desc_dma_addrs);
			tx_desc_data->tx_desc_dma_addrs = NULL;
		}

		if (tx_desc_data->tx_desc_ptrs) {
			kfree(tx_desc_data->tx_desc_ptrs);
			tx_desc_data->tx_desc_ptrs = NULL;
		}

		if (pdata->ipa_enabled && chInx == IPA_DMA_TX_CH) {
			if (tx_desc_data->ipa_tx_buff_pool_va_addrs_base){
				kfree(tx_desc_data->ipa_tx_buff_pool_va_addrs_base);
				tx_desc_data->ipa_tx_buff_pool_va_addrs_base = NULL;
			}
		}
	}
}

/*!
* \brief API to alloc the queue memory.
*
* \details This function allocates the queue structure memory.
*
* \param[in] pdata - pointer to private data structure.
*
* \return integer
*
* \retval 0 on success & -ve number on failure.
*/

static int DWC_ETH_QOS_alloc_queue_struct(struct DWC_ETH_QOS_prv_data *pdata)
{
	int ret = 0;

	ret = DWC_ETH_QOS_alloc_tx_queue_struct(pdata);
	if (ret)
		return ret;

	ret = DWC_ETH_QOS_alloc_rx_queue_struct(pdata);
	if (ret){
		DWC_ETH_QOS_free_tx_queue_struct(pdata);
		return ret;
	}

	return ret;
}

// @RK: IPA_INTEG We do not need this function now, but are we missing something from this in new implemenation

#if 0

/*!
 * \brief API to alloc the queue memory.
 *
 * \details This function allocates the queue structure memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \return integer
 *
 * \retval 0 on success & -ve number on failure.
 */

static int DWC_ETH_QOS_alloc_queue_struct(struct DWC_ETH_QOS_prv_data *pdata)
{
	int ret = 0;

	DBGPR("%s: tx_queue_cnt = %d, rx_queue_cnt = %d\n",
	      __func__, pdata->tx_queue_cnt, pdata->rx_queue_cnt);

	pdata->tx_queue =
		kzalloc(
		   sizeof(struct DWC_ETH_QOS_tx_queue) * pdata->tx_queue_cnt,
			GFP_KERNEL);
	if (!pdata->tx_queue) {
		EMACERR("ERROR: Unable to allocate Tx queue structure\n");
		ret = -ENOMEM;
		goto err_out_tx_q_alloc_failed;
	}

	pdata->rx_queue = kzalloc(
		   sizeof(struct DWC_ETH_QOS_rx_queue) * pdata->rx_queue_cnt,
			GFP_KERNEL);
	if (!pdata->rx_queue) {
		EMACERR("ERROR: Unable to allocate Rx queue structure\n");
		ret = -ENOMEM;
		goto err_out_rx_q_alloc_failed;
	}

	DBGPR("<--DWC_ETH_QOS_alloc_queue_struct\n");

	return ret;

err_out_rx_q_alloc_failed:
	kfree(pdata->tx_queue);

err_out_tx_q_alloc_failed:
	return ret;
}
#endif

/*!
 * \brief API to free the queue memory.
 *
 * \details This function free the queue structure memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \return void
 */

static void DWC_ETH_QOS_free_queue_struct(struct DWC_ETH_QOS_prv_data *pdata)
{
	DBGPR("-->DWC_ETH_QOS_free_queue_struct\n");
	
	DWC_ETH_QOS_free_tx_queue_struct(pdata);
	DWC_ETH_QOS_free_rx_queue_struct(pdata);
	
	kfree(pdata->tx_queue);
	pdata->tx_queue = NULL;

	kfree(pdata->rx_queue);
	pdata->rx_queue = NULL;

	DBGPR("<--DWC_ETH_QOS_free_queue_struct\n");
}

/*!
 * \brief API to allocate the memory for descriptor & buffers.
 *
 * \details This function is used to allocate the memory for device
 * descriptors & buffers
 * which are used by device for data transmission & reception.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \return integer
 *
 * \retval 0 on success & -ENOMEM number on failure.
 */

static INT allocate_buffer_and_desc(struct DWC_ETH_QOS_prv_data *pdata)
{
	INT ret = 0;
	UINT qinx;

	DBGPR("%s: TX_QUEUE_CNT = %d, RX_QUEUE_CNT = %d\n",
	      __func__, DWC_ETH_QOS_TX_QUEUE_CNT, DWC_ETH_QOS_RX_QUEUE_CNT);

	/* Allocate descriptors and buffers memory for all TX queues */
	for (qinx = 0; qinx < DWC_ETH_QOS_TX_QUEUE_CNT; qinx++) {
		/* TX descriptors */
		GET_TX_DESC_PTR(qinx, 0) = dma_alloc_coherent(
		   GET_MEM_PDEV_DEV,
		   (sizeof(struct s_TX_NORMAL_DESC) * pdata->tx_queue[qinx].desc_cnt),
			&(GET_TX_DESC_DMA_ADDR(qinx, 0)),
			GFP_KERNEL);
		if (!GET_TX_DESC_PTR(qinx, 0)) {
			ret = -ENOMEM;
			goto err_out_tx_desc;
		}
		EMACDBG("Tx Queue(%d) desc base dma address: %p\n",
			qinx, (void*)GET_TX_DESC_DMA_ADDR(qinx, 0));
	}

	for (qinx = 0; qinx < DWC_ETH_QOS_TX_QUEUE_CNT; qinx++) {
		/* TX wrapper buffer */
		GET_TX_BUF_PTR(qinx, 0) = kzalloc(
		   (sizeof(struct DWC_ETH_QOS_tx_buffer) * pdata->tx_queue[qinx].desc_cnt),
			GFP_KERNEL);
		if (!GET_TX_BUF_PTR(qinx, 0)) {
			ret = -ENOMEM;
			goto err_out_tx_buf;
		}
	}

	/* Allocate descriptors and buffers memory for all RX queues */
	for (qinx = 0; qinx < DWC_ETH_QOS_RX_QUEUE_CNT; qinx++) {
		/* RX descriptors */
		GET_RX_DESC_PTR(qinx, 0) = dma_alloc_coherent(
		   GET_MEM_PDEV_DEV,
		   (sizeof(struct s_RX_NORMAL_DESC) * pdata->rx_queue[qinx].desc_cnt),
		   &(GET_RX_DESC_DMA_ADDR(qinx, 0)), GFP_KERNEL);
		if (!GET_RX_DESC_PTR(qinx, 0)) {
			ret = -ENOMEM;
			goto rx_alloc_failure;
		}
		EMACDBG("Rx Queue(%d) desc base dma address: %p\n",
			qinx, (void*)GET_RX_DESC_DMA_ADDR(qinx, 0));
	}

	for (qinx = 0; qinx < DWC_ETH_QOS_RX_QUEUE_CNT; qinx++) {
		/* RX wrapper buffer */
		GET_RX_BUF_PTR(qinx, 0) = kzalloc(
		   (sizeof(struct DWC_ETH_QOS_rx_buffer) * pdata->rx_queue[qinx].desc_cnt),
			GFP_KERNEL);
		if (!GET_RX_BUF_PTR(qinx, 0)) {
			ret = -ENOMEM;
			goto err_out_rx_buf;
		}
	}

	DBGPR("<--allocate_buffer_and_desc\n");

	return ret;

 err_out_rx_buf:
	DWC_ETH_QOS_rx_buf_free_mem(pdata, qinx);
	qinx = DWC_ETH_QOS_RX_QUEUE_CNT;

 rx_alloc_failure:
	DWC_ETH_QOS_rx_desc_free_mem(pdata, qinx);
	qinx = DWC_ETH_QOS_TX_QUEUE_CNT;

 err_out_tx_buf:
	DWC_ETH_QOS_tx_buf_free_mem(pdata, qinx);
	qinx = DWC_ETH_QOS_TX_QUEUE_CNT;

 err_out_tx_desc:
	DWC_ETH_QOS_tx_desc_free_mem(pdata, qinx);

	return ret;
}

/*!
 * \brief API to initialize the transmit descriptors.
 *
 * \details This function is used to initialize transmit descriptors.
 * Each descriptors are assigned a buffer. The base/starting address
 * of the descriptors is updated in device register if required & all
 * the private data structure variables related to transmit
 * descriptor handling are updated in this function.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \return void.
 */

static void DWC_ETH_QOS_wrapper_tx_descriptor_init_single_q(
			struct DWC_ETH_QOS_prv_data *pdata,
			UINT qinx)
{
	int i;
	struct DWC_ETH_QOS_tx_wrapper_descriptor *desc_data =
	    GET_TX_WRAPPER_DESC(qinx);
	struct DWC_ETH_QOS_tx_buffer *buffer = GET_TX_BUF_PTR(qinx, 0);
	struct s_TX_NORMAL_DESC *desc = GET_TX_DESC_PTR(qinx, 0);
	dma_addr_t desc_dma = GET_TX_DESC_DMA_ADDR(qinx, 0);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	void *ipa_tx_buf_vaddr;
	dma_addr_t ipa_tx_buf_dma_addr;
	struct sg_table *buff_sgt;
	int ret  = 0;

	DBGPR("%s: qinx = %u\n", __func__, qinx);

	if (pdata->ipa_enabled) {
		/* Allocate TX Buffer Pool Structure */
		if (qinx == IPA_DMA_TX_CH) {
			GET_TX_BUFF_POOL_BASE_ADRR(qinx) =
				dma_zalloc_coherent(
				   GET_MEM_PDEV_DEV,
				   sizeof(dma_addr_t) * pdata->tx_queue[qinx].desc_cnt,
				   &GET_TX_BUFF_POOL_BASE_PADRR(qinx), GFP_KERNEL);
			if (GET_TX_BUFF_POOL_BASE_ADRR(qinx) == NULL)
				EMACERR("ERROR: Unable to allocate IPA \
							  TX Buff structure for TXCH\n");
			else
				EMACDBG("IPA tx_dma_buff_addrs %p\n",
						 GET_TX_BUFF_POOL_BASE_ADRR(qinx));
		}
	}

	for (i = 0; i < pdata->tx_queue[qinx].desc_cnt; i++) {
		GET_TX_DESC_PTR(qinx, i) = &desc[i];
		GET_TX_DESC_DMA_ADDR(qinx, i) =
		    (desc_dma + sizeof(struct s_TX_NORMAL_DESC) * i);
		GET_TX_BUF_PTR(qinx, i) = &buffer[i];
		
		if (pdata->ipa_enabled) {
			/* Create a memory pool for TX offload path */
			/* Currently only IPA_DMA_TX_CH is supported */
			if (qinx == IPA_DMA_TX_CH) {
				ipa_tx_buf_vaddr = dma_alloc_coherent(
				   GET_MEM_PDEV_DEV, DWC_ETH_QOS_ETH_FRAME_LEN_IPA, &ipa_tx_buf_dma_addr, GFP_KERNEL);
				if (ipa_tx_buf_vaddr == NULL) {
					EMACERR("Failed to allocate TX buf for IPA\n");
					return;
				}
				GET_TX_BUFF_LOGICAL_ADDR(qinx, i) = ipa_tx_buf_vaddr;
				GET_TX_BUFF_DMA_ADDR(qinx, i) = ipa_tx_buf_dma_addr;
				buff_sgt = kzalloc(sizeof (*buff_sgt), GFP_KERNEL);
				if (buff_sgt) {
					ret = dma_get_sgtable(GET_MEM_PDEV_DEV, buff_sgt,
						ipa_tx_buf_vaddr, ipa_tx_buf_dma_addr, DWC_ETH_QOS_ETH_FRAME_LEN_IPA);
					if (ret == Y_SUCCESS) {
						GET_TX_BUF_PTR(IPA_DMA_TX_CH, i)->ipa_tx_buff_phy_addr =
							sg_phys(buff_sgt->sgl);
						sg_free_table(buff_sgt);
						} else {
							EMACERR("Failed to get sgtable for allocated RX buffer.\n");
						}
						kfree(buff_sgt);
						buff_sgt = NULL;
				} else {
					EMACERR("Failed to allocate memory for RX buff sgtable.\n");
				}
			}
		}
#ifdef DWC_ETH_QOS_CONFIG_PGTEST
		if (DWC_ETH_QOS_alloc_tx_buf_pg(pdata, GET_TX_BUF_PTR(qinx, i),
						GFP_KERNEL))
			break;
#endif /* end of DWC_ETH_QOS_CONFIG_PGTEST */
	}
	
	if (pdata->ipa_enabled && qinx == IPA_DMA_TX_CH){
		EMACDBG("Created the virtual memory pool address for TX CH for %d desc \n",
			    pdata->tx_queue[IPA_DMA_TX_CH].desc_cnt);
		EMACDBG("DMA MAPed the virtual memory pool address for TX CH for %d descs \n",
			    pdata->tx_queue[IPA_DMA_TX_CH].desc_cnt);
	}

	desc_data->cur_tx = 0;
	desc_data->dirty_tx = 0;
	desc_data->queue_stopped = 0;
	desc_data->tx_pkt_queued = 0;
	desc_data->packet_count = 0;
	desc_data->free_desc_cnt = pdata->tx_queue[qinx].desc_cnt;

#ifdef DWC_ETH_QOS_CONFIG_PGTEST
	hw_if->tx_desc_init_pg(pdata, qinx);
#else
	hw_if->tx_desc_init(pdata, qinx);
#endif
	desc_data->cur_tx = 0;

	DBGPR("<--DWC_ETH_QOS_wrapper_tx_descriptor_init_single_q\n");
}

/*!
 * \brief API to initialize the receive descriptors.
 *
 * \details This function is used to initialize receive descriptors.
 * skb buffer is allocated & assigned for each descriptors. The base/starting
 * address of the descriptors is updated in device register if required and
 * all the private data structure variables related to receive descriptor
 * handling are updated in this function.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \return void.
 */

static void DWC_ETH_QOS_wrapper_rx_descriptor_init_single_q(
			struct DWC_ETH_QOS_prv_data *pdata,
			UINT qinx)
{
	int i;
	struct DWC_ETH_QOS_rx_wrapper_descriptor *desc_data =
	    GET_RX_WRAPPER_DESC(qinx);
	struct DWC_ETH_QOS_rx_buffer *buffer = GET_RX_BUF_PTR(qinx, 0);
	struct s_RX_NORMAL_DESC *desc = GET_RX_DESC_PTR(qinx, 0);
	dma_addr_t desc_dma = GET_RX_DESC_DMA_ADDR(qinx, 0);
	struct hw_if_struct *hw_if = &pdata->hw_if;

	DBGPR("%s: qinx = %u\n", __func__, qinx);

	memset(buffer, 0, (sizeof(struct DWC_ETH_QOS_rx_buffer) * pdata->rx_queue[qinx].desc_cnt));
	
	/* Allocate RX Buffer Pool Structure */
	if (pdata->ipa_enabled && qinx == IPA_DMA_RX_CH) {
		if (!GET_RX_BUFF_POOL_BASE_ADRR(qinx)) {
			GET_RX_BUFF_POOL_BASE_ADRR(qinx) =
				dma_zalloc_coherent(
				   GET_MEM_PDEV_DEV,
				   sizeof(dma_addr_t) * pdata->rx_queue[qinx].desc_cnt,
				   &GET_RX_BUFF_POOL_BASE_PADRR(qinx), GFP_KERNEL);
			if (GET_RX_BUFF_POOL_BASE_ADRR(qinx) == NULL)
				EMACERR("ERROR: Unable to allocate IPA \
						 RX Buff structure for RXCH0\n");
			else
				EMACDBG("IPA rx_buff_addrs %p \n",
						GET_RX_BUFF_POOL_BASE_ADRR(qinx));
		}
	}

	for (i = 0; i < pdata->rx_queue[qinx].desc_cnt; i++) {
		GET_RX_DESC_PTR(qinx, i) = &desc[i];
		GET_RX_DESC_DMA_ADDR(qinx, i) =
		    (desc_dma + sizeof(struct s_RX_NORMAL_DESC) * i);
		GET_RX_BUF_PTR(qinx, i) = &buffer[i];
#ifdef DWC_ETH_QOS_CONFIG_PGTEST
		if (DWC_ETH_QOS_alloc_rx_buf_pg(
		   pdata, GET_RX_BUF_PTR(qinx, i), GFP_KERNEL))
			break;
#else
		/* allocate skb & assign to each desc */
		if (pdata->alloc_rx_buf(pdata,
				GET_RX_BUF_PTR(qinx, i), qinx, GFP_KERNEL))
			break;
#endif /* end of DWC_ETH_QOS_CONFIG_PGTEST */
		
		/* Assign the RX memory pool for offload data path */
		if (pdata->ipa_enabled && qinx == IPA_DMA_RX_CH) {
			GET_RX_BUFF_LOGICAL_ADDR(qinx, i) =
				((struct DWC_ETH_QOS_rx_buffer *)GET_RX_BUF_PTR(qinx, i))->ipa_buff_va;
			GET_RX_BUFF_DMA_ADDR(qinx, i) =
				((struct DWC_ETH_QOS_rx_buffer *)GET_RX_BUF_PTR(qinx, i))->dma;
		}

		/* alloc_rx_buf */
		wmb();
	}
	
	EMACDBG("Allocated %d buffers for RX Channel: %d \n", pdata->rx_queue[qinx].desc_cnt,qinx);
	if (pdata->ipa_enabled && qinx == IPA_DMA_RX_CH)
		EMACDBG("Assign virtual memory pool address for RX CH0 for %d desc\n",
					pdata->rx_queue[IPA_DMA_RX_CH].desc_cnt);


	desc_data->cur_rx = 0;
	desc_data->dirty_rx = 0;
	desc_data->skb_realloc_idx = 0;
	desc_data->skb_realloc_threshold = MIN_RX_DESC_CNT;
	desc_data->pkt_received = 0;

#ifdef DWC_ETH_QOS_CONFIG_PGTEST
	hw_if->rx_desc_init_pg(pdata, qinx);
#else
	hw_if->rx_desc_init(pdata, qinx);
#endif
	desc_data->cur_rx = 0;

	DBGPR("<--DWC_ETH_QOS_wrapper_rx_descriptor_init_single_q\n");
}

static void DWC_ETH_QOS_wrapper_tx_descriptor_init(struct DWC_ETH_QOS_prv_data
						   *pdata)
{
	UINT qinx;

	DBGPR("-->DWC_ETH_QOS_wrapper_tx_descriptor_init\n");

	for (qinx = 0; qinx < DWC_ETH_QOS_TX_QUEUE_CNT; qinx++)
		DWC_ETH_QOS_wrapper_tx_descriptor_init_single_q(pdata, qinx);

	DBGPR("<--DWC_ETH_QOS_wrapper_tx_descriptor_init\n");
}

static void DWC_ETH_QOS_wrapper_rx_descriptor_init(struct DWC_ETH_QOS_prv_data
						   *pdata)
{
	struct DWC_ETH_QOS_rx_queue *rx_queue = NULL;
	UINT qinx;

	DBGPR("-->DWC_ETH_QOS_wrapper_rx_descriptor_init\n");

	for (qinx = 0; qinx < DWC_ETH_QOS_RX_QUEUE_CNT; qinx++) {
		rx_queue = GET_RX_QUEUE_PTR(qinx);
		rx_queue->pdata = pdata;

#ifdef DWC_INET_LRO
		/* LRO configuration */
		rx_queue->lro_mgr.dev = pdata->dev;
		memset(&rx_queue->lro_mgr.stats, 0,
		       sizeof(rx_queue->lro_mgr.stats));
		rx_queue->lro_mgr.features =
			LRO_F_NAPI | LRO_F_EXTRACT_VLAN_ID;
		rx_queue->lro_mgr.ip_summed = CHECKSUM_UNNECESSARY;
		rx_queue->lro_mgr.ip_summed_aggr = CHECKSUM_UNNECESSARY;
		rx_queue->lro_mgr.max_desc = DWC_ETH_QOS_MAX_LRO_DESC;
		rx_queue->lro_mgr.max_aggr = (0xffff / pdata->dev->mtu);
		rx_queue->lro_mgr.lro_arr = rx_queue->lro_arr;
		rx_queue->lro_mgr.get_skb_header = DWC_ETH_QOS_get_skb_hdr;
		memset(&rx_queue->lro_arr, 0, sizeof(rx_queue->lro_arr));
		rx_queue->lro_flush_needed = 0;
#endif

		DWC_ETH_QOS_wrapper_rx_descriptor_init_single_q(pdata, qinx);
	}

	DBGPR("<--DWC_ETH_QOS_wrapper_rx_descriptor_init\n");
}

/*!
 * \brief API to free the receive descriptor & buffer memory.
 *
 * \details This function is used to free the
 * receive descriptor & buffer memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \retval void.
 */

static void DWC_ETH_QOS_rx_free_mem(struct DWC_ETH_QOS_prv_data *pdata)
{
	DBGPR("-->DWC_ETH_QOS_rx_free_mem\n");

	/* free RX descriptor */
	DWC_ETH_QOS_rx_desc_free_mem(pdata, DWC_ETH_QOS_RX_QUEUE_CNT);

	/* free RX skb's */
	DWC_ETH_QOS_rx_skb_free_mem(pdata, DWC_ETH_QOS_RX_QUEUE_CNT);

	/* free RX wrapper buffer */
	DWC_ETH_QOS_rx_buf_free_mem(pdata, DWC_ETH_QOS_RX_QUEUE_CNT);

	DBGPR("<--DWC_ETH_QOS_rx_free_mem\n");
}

/*!
 * \brief API to free the transmit descriptor & buffer memory.
 *
 * \details This function is used to free the transmit descriptor
 * & buffer memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \retval void.
 */

static void DWC_ETH_QOS_tx_free_mem(struct DWC_ETH_QOS_prv_data *pdata)
{
	DBGPR("-->DWC_ETH_QOS_tx_free_mem\n");

	/* free TX descriptor */
	DWC_ETH_QOS_tx_desc_free_mem(pdata, DWC_ETH_QOS_TX_QUEUE_CNT);

#ifdef DWC_ETH_QOS_CONFIG_PGTEST
	/* free TX skb's */
	DWC_ETH_QOS_tx_skb_free_mem(pdata, DWC_ETH_QOS_TX_QUEUE_CNT);
#endif /* end of DWC_ETH_QOS_CONFIG_PGTEST */

	/* free TX buffer */
	DWC_ETH_QOS_tx_buf_free_mem(pdata, DWC_ETH_QOS_TX_QUEUE_CNT);

	DBGPR("<--DWC_ETH_QOS_tx_free_mem\n");
}

/*!
 * \details This function is invoked by other function to free
 * the tx socket buffers.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */

static void DWC_ETH_QOS_tx_skb_free_mem_single_q(
	struct DWC_ETH_QOS_prv_data *pdata, UINT qinx)
{
	UINT i;

	DBGPR("-->DWC_ETH_QOS_tx_skb_free_mem_single_q: qinx = %u\n", qinx);

	for (i = 0; i < pdata->tx_queue[qinx].desc_cnt; i++)
		DWC_ETH_QOS_unmap_tx_skb(pdata, GET_TX_BUF_PTR(qinx, i));

	DBGPR("<--DWC_ETH_QOS_tx_skb_free_mem_single_q\n");
}

/*!
 * \brief API to free the transmit descriptor skb memory.
 *
 * \details This function is used to free the transmit descriptor skb memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \retval void.
 */

static void DWC_ETH_QOS_tx_skb_free_mem(struct DWC_ETH_QOS_prv_data *pdata,
					UINT tx_qcnt)
{
	UINT qinx;

	DBGPR("-->DWC_ETH_QOS_tx_skb_free_mem: tx_qcnt = %d\n", tx_qcnt);

	for (qinx = 0; qinx < tx_qcnt; qinx++)
		DWC_ETH_QOS_tx_skb_free_mem_single_q(pdata, qinx);

	DBGPR("<--DWC_ETH_QOS_tx_skb_free_mem\n");
}

#ifdef DWC_ETH_QOS_CONFIG_PGTEST
/*!
 * \details This function is used to release Rx socket buffer.
 *
 * \param[in] pdata – pointer to private device structure.
 * \param[in] buffer – pointer to rx wrapper buffer structure.
 *
 * \return void
 */
static void DWC_ETH_QOS_unmap_rx_skb_pg(struct DWC_ETH_QOS_prv_data *pdata,
					struct DWC_ETH_QOS_rx_buffer *buffer)
{
	DBGPR("-->DWC_ETH_QOS_unmap_rx_skb_pg\n");

	/* unmap the first buffer */
	if (buffer->dma) {
		dma_unmap_single(GET_MEM_PDEV_DEV, buffer->dma,
				 DWC_ETH_QOS_PG_FRAME_SIZE, DMA_FROM_DEVICE);
		buffer->dma = 0;
	}

	if (buffer->skb) {
		dev_kfree_skb_any(buffer->skb);
		buffer->skb = NULL;
	}

	/* DBGPR("<--DWC_ETH_QOS_unmap_rx_skb_pg\n"); */
}
#endif

/*!
 * \details This function is invoked by other function to free
 * the rx socket buffers.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */

static void DWC_ETH_QOS_rx_skb_free_mem_single_q(
	struct DWC_ETH_QOS_prv_data *pdata, UINT qinx)
{
	struct DWC_ETH_QOS_rx_wrapper_descriptor *desc_data =
	    GET_RX_WRAPPER_DESC(qinx);
	UINT i;

	DBGPR("-->DWC_ETH_QOS_rx_skb_free_mem_single_q: qinx = %u\n", qinx);

	if (pdata->ipa_enabled && qinx == IPA_DMA_RX_CH) {
		for (i = 0; i < pdata->rx_queue[qinx].desc_cnt; i++) {
			dma_free_coherent(
			   GET_MEM_PDEV_DEV, DWC_ETH_QOS_ETH_FRAME_LEN_IPA,
			   GET_RX_BUFF_LOGICAL_ADDR(qinx, i), GET_RX_BUFF_DMA_ADDR(qinx, i));
		}
	} else {
		for (i = 0; i < pdata->rx_queue[qinx].desc_cnt; i++) {
	#ifdef DWC_ETH_QOS_CONFIG_PGTEST
			DWC_ETH_QOS_unmap_rx_skb_pg(pdata, GET_RX_BUF_PTR(qinx, i));
	#else
			DWC_ETH_QOS_unmap_rx_skb(pdata, GET_RX_BUF_PTR(qinx, i));
	#endif
		}

		/* there are also some cached data from a chained rx */
		if (desc_data->skb_top)
			dev_kfree_skb_any(desc_data->skb_top);

		desc_data->skb_top = NULL;
	}

	DBGPR("<--DWC_ETH_QOS_rx_skb_free_mem_single_q\n");
}

/*!
 * \brief API to free the receive descriptor skb memory.
 *
 * \details This function is used to free the receive descriptor skb memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \retval void.
 */

static void DWC_ETH_QOS_rx_skb_free_mem(struct DWC_ETH_QOS_prv_data *pdata,
					UINT rx_qcnt)
{
	UINT qinx;

	DBGPR("-->DWC_ETH_QOS_rx_skb_free_mem: rx_qcnt = %d\n", rx_qcnt);

	for (qinx = 0; qinx < rx_qcnt; qinx++)
		DWC_ETH_QOS_rx_skb_free_mem_single_q(pdata, qinx);

	DBGPR("<--DWC_ETH_QOS_rx_skb_free_mem\n");
}

/*!
 * \brief API to free the transmit descriptor wrapper buffer memory.
 *
 * \details This function is used to free the transmit
 * descriptor wrapper buffer memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \retval void.
 */

static void DWC_ETH_QOS_tx_buf_free_mem(struct DWC_ETH_QOS_prv_data *pdata,
					UINT tx_qcnt)
{
	UINT qinx;
	UINT i = 0;
	
	DBGPR("-->DWC_ETH_QOS_tx_buf_free_mem: tx_qcnt = %d\n", tx_qcnt);

	for (qinx = 0; qinx < tx_qcnt; qinx++) {
		/* free TX buffer */
		if (GET_TX_BUF_PTR(qinx, 0)) {
			kfree(GET_TX_BUF_PTR(qinx, 0));
			GET_TX_BUF_PTR(qinx, 0) = NULL;
		}
		
		if (pdata->ipa_enabled) {
			/* Free memory pool for TX offload path */
			/* Currently only IPA_DMA_TX_CH is supported */
			if (qinx == IPA_DMA_TX_CH) {
				for (i = 0; i < pdata->tx_queue[qinx].desc_cnt; i++) {
					dma_free_coherent(GET_MEM_PDEV_DEV,
									  DWC_ETH_QOS_ETH_FRAME_LEN_IPA,
									  GET_TX_BUFF_LOGICAL_ADDR(qinx, i),
									  GET_TX_BUFF_DMA_ADDR(qinx, i));
				}
				EMACINFO("Freed the memory allocated for IPA_DMA_TX_CH for IPA \n");
				/* De-Allocate TX DMA Buffer Pool Structure */
				if (GET_TX_BUFF_POOL_BASE_ADRR(qinx)) {
					dma_free_coherent(GET_MEM_PDEV_DEV,
									  GET_TX_BUFF_POOL_BASE_ADRR_SIZE(qinx),
									  GET_TX_BUFF_POOL_BASE_ADRR(qinx),
									  GET_TX_BUFF_POOL_BASE_PADRR(qinx));
					GET_TX_BUFF_POOL_BASE_ADRR(qinx) = NULL;
					GET_TX_BUFF_POOL_BASE_PADRR(qinx) = (dma_addr_t)NULL;
					EMACINFO("Freed the TX Buffer Pool Structure for IPA_DMA_TX_CH for IPA \n");
				} else {
					EMACERR("Unable to DeAlloc TX Buff structure\n");
				}
			}
		}
	}
	DBGPR("<--DWC_ETH_QOS_tx_buf_free_mem\n");
}

/*!
 * \brief API to free the receive descriptor wrapper buffer memory.
 *
 * \details This function is used to free the receive
 * descriptor wrapper buffer memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \retval void.
 */

static void DWC_ETH_QOS_rx_buf_free_mem(struct DWC_ETH_QOS_prv_data *pdata,
					UINT rx_qcnt)
{
	UINT qinx = 0;

	DBGPR("-->DWC_ETH_QOS_rx_buf_free_mem: rx_qcnt = %d\n", rx_qcnt);

	for (qinx = 0; qinx < rx_qcnt; qinx++) {
		if (GET_RX_BUF_PTR(qinx, 0)) {
			kfree(GET_RX_BUF_PTR(qinx, 0));
			GET_RX_BUF_PTR(qinx, 0) = NULL;
		}
		/* Deallocate RX Buffer Pool Structure */
		if (pdata->ipa_enabled && qinx == IPA_DMA_RX_CH) {
			/* Free memory pool for RX offload path */
			/* Currently only IPA_DMA_RX_CH is supported */
			if (GET_RX_BUFF_POOL_BASE_ADRR(qinx)) {
				dma_free_coherent(GET_MEM_PDEV_DEV,
								  GET_RX_BUFF_POOL_BASE_ADRR_SIZE(qinx),
								  GET_RX_BUFF_POOL_BASE_ADRR(qinx),
								  GET_RX_BUFF_POOL_BASE_PADRR(qinx));
				GET_RX_BUFF_POOL_BASE_ADRR(qinx) = NULL;
				GET_RX_BUFF_POOL_BASE_PADRR(qinx) = (dma_addr_t)NULL;
				EMACINFO("Freed the RX Buffer Pool Structure for IPA_DMA_RX_CH for IPA \n");
			} else {
				EMACERR("Unable to DeAlloc RX Buff structure\n");
			}
		}
	}

	DBGPR("<--DWC_ETH_QOS_rx_buf_free_mem\n");
}

#ifdef DWC_INET_LRO
/*!
 * \brief Assigns the network and tcp header pointers
 *
 * \details This function gets the ip and tcp header pointers of the packet
 * in the skb and assigns them to the corresponding arguments passed to the
 * function. It also sets some flags indicating that the packet to be receieved
 * is an ipv4 packet and that the protocol is tcp.
 *
 * \param[in] *skb - pointer to the sk buffer,
 * \param[in] **iph - pointer to be pointed to the ip header,
 * \param[in] **tcph - pointer to be pointed to the tcp header,
 * \param[in] *hdr_flags - flags to be set
 * \param[in] *pdata - private data structure
 *
 * \return integer
 *
 * \retval -1 if the packet does not conform to ip protocol = TCP, else 0
 */

static int DWC_ETH_QOS_get_skb_hdr(struct sk_buff *skb, void **iph,
				   void **tcph, u64 *flags, void *ptr)
{
	struct DWC_ETH_QOS_prv_data *pdata = ptr;

	DBGPR("-->DWC_ETH_QOS_get_skb_hdr\n");

	if (!pdata->tcp_pkt)
		return -EPERM;

	skb_reset_network_header(skb);
	skb_set_transport_header(skb, ip_hdrlen(skb));
	*iph = ip_hdr(skb);
	*tcph = tcp_hdr(skb);
	*flags = LRO_IPV4 | LRO_TCP;

	DBGPR("<--DWC_ETH_QOS_get_skb_hdr\n");

	return 0;
}
#endif

/*!
 * \brief api to tcp_udp_hdrlen
 *
 * \details This function is invoked by DWC_ETH_QOS_handle_tso. This function
 * will get the header type and return the header length.
 *
 * \param[in] skb – pointer to socket buffer structure.
 *
 * \return integer
 *
 * \retval tcp or udp header length
 *
 */
static unsigned int tcp_udp_hdrlen(struct sk_buff *skb)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	struct iphdr *network_header;
	network_header = (struct iphdr *)skb_network_header(skb);

	if (network_header->protocol == IPPROTO_UDP)
#else
	if (skb_shinfo(skb)->gso_type & (SKB_GSO_UDP))
#endif
		return sizeof(struct udphdr);
	else
		return tcp_hdrlen(skb);
}

/*!
 * \brief api to handle tso
 *
 * \details This function is invoked by start_xmit functions. This function
 * will get all the tso details like MSS(Maximum Segment Size),
 * packet header length, * packet pay load length and tcp header length etc
 * if the given skb has tso * packet and store it in other wrapper
 * tx structure for later usage.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] skb – pointer to socket buffer structure.
 *
 * \return integer
 *
 * \retval 1 on success, -ve no failure and 0 if not tso pkt
 *
 */
static int DWC_ETH_QOS_handle_tso(struct net_device *dev,
				  struct sk_buff *skb)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct s_tx_pkt_features *tx_pkt_features = GET_TX_PKT_FEATURES_PTR;
	int ret = 1;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	struct iphdr *network_header;
#endif

	DBGPR("-->DWC_ETH_QOS_handle_tso\n");

	if (skb_is_gso(skb) == 0) {
		DBGPR("This is not a TSO/LSO/GSO packet\n");
		return 0;
	}

	DBGPR("Got TSO packet\n");

	if (skb_header_cloned(skb)) {
		ret = pskb_expand_head(skb, 0, 0, GFP_ATOMIC);
		if (ret)
			return ret;
	}

	/* get TSO or UFO details */
	tx_pkt_features->hdr_len =
		skb_transport_offset(skb) + tcp_udp_hdrlen(skb);
	tx_pkt_features->tcp_udp_hdr_len = tcp_udp_hdrlen(skb);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	network_header = (struct iphdr *)skb_network_header(skb);
	if (network_header->protocol == IPPROTO_UDP) {
#else
	if (skb_shinfo(skb)->gso_type & (SKB_GSO_UDP)) {
#endif
		tx_pkt_features->mss =
			skb_shinfo(skb)->gso_size - sizeof(struct udphdr);
	} else {
		tx_pkt_features->mss = skb_shinfo(skb)->gso_size;
	}

	tx_pkt_features->pay_len = (skb->len - tx_pkt_features->hdr_len);

	DBGPR("mss		= %lu\n", tx_pkt_features->mss);
	DBGPR("hdr_len		= %lu\n", tx_pkt_features->hdr_len);
	DBGPR("pay_len		= %lu\n", tx_pkt_features->pay_len);
	DBGPR("tcp_udp_hdr_len	= %lu\n", tx_pkt_features->tcp_udp_hdr_len);

	DBGPR("<--DWC_ETH_QOS_handle_tso\n");

	return ret;
}

/* returns 0 on success and -ve on failure */
static int DWC_ETH_QOS_map_non_page_buffs(struct DWC_ETH_QOS_prv_data *pdata,
					  struct DWC_ETH_QOS_tx_buffer *buffer,
				struct DWC_ETH_QOS_tx_buffer *prev_buffer,
				struct sk_buff *skb,
				unsigned int offset,
				unsigned int size)
{
	void *l_offset;

	DBGPR("-->DWC_ETH_QOS_map_non_page_buffs\n");

	if (size > DWC_ETH_QOS_MAX_DATA_PER_TX_BUF) {
		if (prev_buffer && !prev_buffer->dma2) {
			/* fill the first buffer pointer in prev_buffer->dma2 */
			prev_buffer->dma2 = dma_map_single(
			   GET_MEM_PDEV_DEV,
			   (skb->data + offset),
			   DWC_ETH_QOS_MAX_DATA_PER_TX_BUF, DMA_TO_DEVICE);
			if (dma_mapping_error(
			   GET_MEM_PDEV_DEV, prev_buffer->dma2)) {
				EMACERR("failed to do the dma map\n");
				return -ENOMEM;
			}
			prev_buffer->len2 = DWC_ETH_QOS_MAX_DATA_PER_TX_BUF;
			prev_buffer->buf2_mapped_as_page = Y_FALSE;

			/* fill the second buffer pointer in buffer->dma */
			l_offset = (void *)(skb->data +
				offset + DWC_ETH_QOS_MAX_DATA_PER_TX_BUF);
			buffer->dma = dma_map_single(
			   GET_MEM_PDEV_DEV, l_offset,
				(size - DWC_ETH_QOS_MAX_DATA_PER_TX_BUF),
				DMA_TO_DEVICE);
			if (dma_mapping_error(
			   GET_MEM_PDEV_DEV, buffer->dma)) {
				EMACERR("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len = (size - DWC_ETH_QOS_MAX_DATA_PER_TX_BUF);
			buffer->buf1_mapped_as_page = Y_FALSE;
			buffer->dma2 = 0;
			buffer->len2 = 0;
		} else {
			/* fill the first buffer pointer in buffer->dma */
			buffer->dma = dma_map_single(GET_MEM_PDEV_DEV,
					(skb->data + offset),
					DWC_ETH_QOS_MAX_DATA_PER_TX_BUF,
					DMA_TO_DEVICE);
			if (dma_mapping_error(
			   GET_MEM_PDEV_DEV, buffer->dma)) {
				EMACERR("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len = DWC_ETH_QOS_MAX_DATA_PER_TX_BUF;
			buffer->buf1_mapped_as_page = Y_FALSE;

			/* fill the second buffer pointer in buffer->dma2 */
			l_offset = (void *)(skb->data +
				offset + DWC_ETH_QOS_MAX_DATA_PER_TX_BUF);
			buffer->dma2 = dma_map_single(
			  GET_MEM_PDEV_DEV, l_offset,
			  (size - DWC_ETH_QOS_MAX_DATA_PER_TX_BUF),
			  DMA_TO_DEVICE);
			if (dma_mapping_error(
			   GET_MEM_PDEV_DEV, buffer->dma2)) {
				EMACERR("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len2 = (size - DWC_ETH_QOS_MAX_DATA_PER_TX_BUF);
			buffer->buf2_mapped_as_page = Y_FALSE;
		}
	} else {
		if (prev_buffer && !prev_buffer->dma2) {
			/* fill the first buffer pointer in prev_buffer->dma2 */
			prev_buffer->dma2 = dma_map_single(GET_MEM_PDEV_DEV,
						(skb->data + offset),
						size, DMA_TO_DEVICE);
			if (dma_mapping_error(
			   GET_MEM_PDEV_DEV, prev_buffer->dma2)) {
				EMACERR("failed to do the dma map\n");
				return -ENOMEM;
			}
			prev_buffer->len2 = size;
			prev_buffer->buf2_mapped_as_page = Y_FALSE;

			/* indicate current buffer struct is not used */
			buffer->dma = 0;
			buffer->len = 0;
			buffer->dma2 = 0;
			buffer->len2 = 0;
		} else {
			/* fill the first buffer pointer in buffer->dma */
			buffer->dma = dma_map_single(GET_MEM_PDEV_DEV,
						(skb->data + offset),
						size, DMA_TO_DEVICE);
			if (dma_mapping_error(
			   GET_MEM_PDEV_DEV, buffer->dma)) {
				EMACERR("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len = size;
			buffer->buf1_mapped_as_page = Y_FALSE;
			buffer->dma2 = 0;
			buffer->len2 = 0;
		}
	}

	DBGPR("<--DWC_ETH_QOS_map_non_page_buffs\n");

	return 0;
}

/* returns 0 on success and -ve on failure */
static int DWC_ETH_QOS_map_page_buffs(struct DWC_ETH_QOS_prv_data *pdata,
				      struct DWC_ETH_QOS_tx_buffer *buffer,
			struct DWC_ETH_QOS_tx_buffer *prev_buffer,
			struct skb_frag_struct *frag,
			unsigned int offset,
			unsigned int size)
{
	unsigned long l_offset;

	DBGPR("-->DWC_ETH_QOS_map_page_buffs\n");

	if (size > DWC_ETH_QOS_MAX_DATA_PER_TX_BUF) {
		if (prev_buffer && !prev_buffer->dma2) {
			DBGPR("prev_buffer->dma2 is empty\n");
			/* fill the first buffer pointer in pre_buffer->dma2 */
			prev_buffer->dma2 =
				dma_map_page(
				   GET_MEM_PDEV_DEV,
					frag->page.p,
					(frag->page_offset + offset),
					DWC_ETH_QOS_MAX_DATA_PER_TX_BUF,
					DMA_TO_DEVICE);
			if (dma_mapping_error(GET_MEM_PDEV_DEV,
					      prev_buffer->dma2)) {
				EMACERR("failed to do the dma map\n");
				return -ENOMEM;
			}
			prev_buffer->len2 = DWC_ETH_QOS_MAX_DATA_PER_TX_BUF;
			prev_buffer->buf2_mapped_as_page = Y_TRUE;

			l_offset = (frag->page_offset +
				offset + DWC_ETH_QOS_MAX_DATA_PER_TX_BUF);
			/* fill the second buffer pointer in buffer->dma */
			buffer->dma = dma_map_page(
			 GET_MEM_PDEV_DEV, frag->page.p, l_offset,
			 (size - DWC_ETH_QOS_MAX_DATA_PER_TX_BUF),
			 DMA_TO_DEVICE);
			if (dma_mapping_error(GET_MEM_PDEV_DEV,
					      buffer->dma)) {
				EMACERR("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len = (size - DWC_ETH_QOS_MAX_DATA_PER_TX_BUF);
			buffer->buf1_mapped_as_page = Y_TRUE;
			buffer->dma2 = 0;
			buffer->len2 = 0;
		} else {
			/* fill the first buffer pointer in buffer->dma */
			buffer->dma = dma_map_page(GET_MEM_PDEV_DEV,
						frag->page.p,
						(frag->page_offset + offset),
						DWC_ETH_QOS_MAX_DATA_PER_TX_BUF,
						DMA_TO_DEVICE);
			if (dma_mapping_error(GET_MEM_PDEV_DEV,
					      buffer->dma)) {
				EMACERR("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len = DWC_ETH_QOS_MAX_DATA_PER_TX_BUF;
			buffer->buf1_mapped_as_page = Y_TRUE;

			/* fill the second buffer pointer in buffer->dma2 */
			l_offset = (frag->page_offset +
				offset + DWC_ETH_QOS_MAX_DATA_PER_TX_BUF);
			buffer->dma2 = dma_map_page(
			   GET_MEM_PDEV_DEV, frag->page.p, l_offset,
			   (size - DWC_ETH_QOS_MAX_DATA_PER_TX_BUF),
			   DMA_TO_DEVICE);
			if (dma_mapping_error(GET_MEM_PDEV_DEV,
					      buffer->dma2)) {
				EMACERR("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len2 = (size - DWC_ETH_QOS_MAX_DATA_PER_TX_BUF);
			buffer->buf2_mapped_as_page = Y_TRUE;
		}
	} else {
		if (prev_buffer && !prev_buffer->dma2) {
			DBGPR("prev_buffer->dma2 is empty\n");
			/* fill the first buffer pointer in pre_buffer->dma2 */
			prev_buffer->dma2 = dma_map_page(GET_MEM_PDEV_DEV,
						frag->page.p,
						frag->page_offset + offset,
						size, DMA_TO_DEVICE);
			if (dma_mapping_error(GET_MEM_PDEV_DEV,
					      prev_buffer->dma2)) {
				EMACERR("failed to do the dma map\n");
				return -ENOMEM;
			}
			prev_buffer->len2 = size;
			prev_buffer->buf2_mapped_as_page = Y_TRUE;

			/* indicate current buffer struct is not used */
			buffer->dma = 0;
			buffer->len = 0;
			buffer->dma2 = 0;
			buffer->len2 = 0;
		} else {
			/* fill the first buffer pointer in buffer->dma */
			buffer->dma = dma_map_page(GET_MEM_PDEV_DEV,
						frag->page.p,
						frag->page_offset + offset,
						size, DMA_TO_DEVICE);
			if (dma_mapping_error(GET_MEM_PDEV_DEV,
					      buffer->dma)) {
				EMACERR("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len = size;
			buffer->buf1_mapped_as_page = Y_TRUE;
			buffer->dma2 = 0;
			buffer->len2 = 0;
		}
	}

	DBGPR("<--DWC_ETH_QOS_map_page_buffs\n");

	return 0;
}

/*!
 * \details This function is invoked by start_xmit functions. This function
 * will get the dma/physical address of the packet to be transmitted and
 * its length. All this information about the packet to be transmitted is
 * stored in private data structure and same is used later in the driver to
 * setup the descriptor for transmission.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] skb – pointer to socket buffer structure.
 *
 * \return unsigned int
 *
 * \retval count – number of packet to be programmed in the descriptor or
 * zero on failure.
 */

static unsigned int DWC_ETH_QOS_map_skb(struct net_device *dev,
					struct sk_buff *skb)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	UINT qinx = skb_get_queue_mapping(skb);
	struct DWC_ETH_QOS_tx_wrapper_descriptor *desc_data =
	    GET_TX_WRAPPER_DESC(qinx);
	struct DWC_ETH_QOS_tx_buffer *buffer =
	    GET_TX_BUF_PTR(qinx, desc_data->cur_tx);
	struct DWC_ETH_QOS_tx_buffer *prev_buffer = NULL;
	struct s_tx_pkt_features *tx_pkt_features = GET_TX_PKT_FEATURES_PTR;
	UINT varvlan_pkt;
	int index = (int)desc_data->cur_tx;
	unsigned int frag_cnt = skb_shinfo(skb)->nr_frags;
	unsigned int hdr_len = 0;
	unsigned int i;
	unsigned int count = 0, offset = 0, size;
	unsigned int len;
	int vartso_enable = 0;
	int ret;

	DBGPR("-->DWC_ETH_QOS_map_skb: cur_tx = %d, qinx = %u\n",
	      desc_data->cur_tx, qinx);

	TX_PKT_FEATURES_PKT_ATTRIBUTES_TSO_ENABLE_MLF_RD(
		tx_pkt_features->pkt_attributes, vartso_enable);
	TX_PKT_FEATURES_PKT_ATTRIBUTES_VLAN_PKT_MLF_RD(
			tx_pkt_features->pkt_attributes, varvlan_pkt);
	if (varvlan_pkt == 0x1) {
		DBGPR("Skipped preparing index %d ");
		DBGPR("(VLAN Context descriptor)\n\n", index);
		INCR_TX_DESC_INDEX(index, 1, pdata->tx_queue[qinx].desc_cnt);
		buffer = GET_TX_BUF_PTR(qinx, index);
	} else if ((vartso_enable == 0x1) &&
			   (desc_data->default_mss != tx_pkt_features->mss)) {
		/* keep space for CONTEXT descriptor in the RING */
		INCR_TX_DESC_INDEX(index, 1, pdata->tx_queue[qinx].desc_cnt);
		buffer = GET_TX_BUF_PTR(qinx, index);
	}
#ifdef DWC_ETH_QOS_ENABLE_DVLAN
	if (pdata->via_reg_or_desc) {
		DBGPR("Skipped preparing index %d ");
		DBGPR("(Double VLAN Context descriptor)\n\n", index);
		INCR_TX_DESC_INDEX(index, 1, pdata->tx_queue[qinx].desc_cnt);
		buffer = GET_TX_BUF_PTR(qinx, index);
	}
#endif /* End of DWC_ETH_QOS_ENABLE_DVLAN */

	if (vartso_enable) {
		hdr_len = skb_transport_offset(skb) + tcp_udp_hdrlen(skb);
		len = hdr_len;
	} else {
		len = (skb->len - skb->data_len);
	}

	DBGPR("skb->len: %d\nskb->data_len: %d\n", skb->len, skb->data_len);
	DBGPR("skb->len - skb->data_len = %d, hdr_len = %d\n",
	      len, hdr_len);
	while (len) {
		size = min_t(unsigned int, len, DWC_ETH_QOS_MAX_DATA_PER_TXD);

		buffer = GET_TX_BUF_PTR(qinx, index);
		ret = DWC_ETH_QOS_map_non_page_buffs(pdata, buffer,
						     prev_buffer,
						skb, offset, size);
		if (ret < 0)
			goto err_out_dma_map_fail;

		len -= size;
		offset += size;
		prev_buffer = buffer;
		INCR_TX_DESC_INDEX(index, 1, pdata->tx_queue[qinx].desc_cnt);
		count++;
	}

	/* Process remaining pay load in skb->data in case of TSO packet */
	if (vartso_enable) {
		len = ((skb->len - skb->data_len) - hdr_len);
		while (len > 0) {
			size =
			min_t(unsigned int, len, DWC_ETH_QOS_MAX_DATA_PER_TXD);

			buffer = GET_TX_BUF_PTR(qinx, index);
			ret = DWC_ETH_QOS_map_non_page_buffs(pdata, buffer,
							     prev_buffer,
							skb, offset, size);
			if (ret < 0)
				goto err_out_dma_map_fail;

			len -= size;
			offset += size;
			if (buffer->dma != 0) {
				prev_buffer = buffer;
				INCR_TX_DESC_INDEX(index, 1, pdata->tx_queue[qinx].desc_cnt);
				count++;
			}
		}
	}

	DBGPR("frag_cnt: %d\n", frag_cnt);
	/* Process fragmented skb's */
	for (i = 0; i < frag_cnt; i++) {
		struct skb_frag_struct *frag = &skb_shinfo(skb)->frags[i];

		DBGPR("frag[%d] size: 0x%x\n", i, frag->size);
		len = frag->size;
		offset = 0;
		while (len) {
			size =
			min_t(unsigned int, len, DWC_ETH_QOS_MAX_DATA_PER_TXD);

			buffer = GET_TX_BUF_PTR(qinx, index);
			ret = DWC_ETH_QOS_map_page_buffs(pdata, buffer,
							 prev_buffer, frag,
							offset, size);
			if (ret < 0)
				goto err_out_dma_map_fail;

			len -= size;
			offset += size;
			if (buffer->dma != 0) {
				prev_buffer = buffer;
				INCR_TX_DESC_INDEX(index, 1, pdata->tx_queue[qinx].desc_cnt);
				count++;
			}
		}
	}

	if (prev_buffer != NULL && buffer->dma == 0 && buffer->dma2 == 0)
		prev_buffer->skb = skb;
	else
		buffer->skb = skb;

	DBGPR("<--DWC_ETH_QOS_map_skb: buffer->dma = %#x\n",
	      (UINT)buffer->dma);

	return count;

 err_out_dma_map_fail:
	EMACERR("Tx DMA map failed\n");

	for (; count > 0; count--) {
		DECR_TX_DESC_INDEX(index, pdata->tx_queue[qinx].desc_cnt);
		buffer = GET_TX_BUF_PTR(qinx, index);
		DWC_ETH_QOS_unmap_tx_skb(pdata, buffer);
	}

	return 0;
}

/*!
 * \brief API to release the skb.
 *
 * \details This function is called in *_tx_interrupt function to release
 * the skb for the successfully transmited packets.
 *
 * \param[in] pdata - pointer to private data structure.
 * \param[in] buffer - pointer to *_tx_buffer structure
 *
 * \return void
 */

static void DWC_ETH_QOS_unmap_tx_skb(struct DWC_ETH_QOS_prv_data *pdata,
				     struct DWC_ETH_QOS_tx_buffer *buffer)
{
	DBGPR("-->DWC_ETH_QOS_unmap_tx_skb\n");

#ifdef DWC_ETH_QOS_CONFIG_PGTEST
	buffer->len = DWC_ETH_QOS_PG_FRAME_SIZE;
#endif

	if (buffer->dma) {
		if (buffer->buf1_mapped_as_page == Y_TRUE)
			dma_unmap_page(GET_MEM_PDEV_DEV, buffer->dma,
				       buffer->len, DMA_TO_DEVICE);
		else
			dma_unmap_single(GET_MEM_PDEV_DEV, buffer->dma,
					 buffer->len, DMA_TO_DEVICE);

		buffer->dma = 0;
		buffer->len = 0;
	}

	if (buffer->dma2) {
		if (buffer->buf2_mapped_as_page == Y_TRUE)
			dma_unmap_page(GET_MEM_PDEV_DEV, buffer->dma2,
				       buffer->len2, DMA_TO_DEVICE);
		else
			dma_unmap_single(GET_MEM_PDEV_DEV, buffer->dma2,
					 buffer->len2, DMA_TO_DEVICE);

		buffer->dma2 = 0;
		buffer->len2 = 0;
	}

	if (buffer->skb) {
		dev_kfree_skb_any(buffer->skb);
		buffer->skb = NULL;
	}

	DBGPR("<--DWC_ETH_QOS_unmap_tx_skb\n");
}

/*!
 * \details This function is invoked by other function for releasing the socket
 * buffer which are received by device and passed to upper layer.
 *
 * \param[in] pdata – pointer to private device structure.
 * \param[in] buffer – pointer to rx wrapper buffer structure.
 *
 * \return void
 */

static void DWC_ETH_QOS_unmap_rx_skb(struct DWC_ETH_QOS_prv_data *pdata,
				     struct DWC_ETH_QOS_rx_buffer *buffer)
{
	DBGPR("-->DWC_ETH_QOS_unmap_rx_skb\n");

	/* unmap the first buffer */
	if (buffer->dma) {
		if (pdata->rx_split_hdr) {
			dma_unmap_single(GET_MEM_PDEV_DEV, buffer->dma,
					 (2 * buffer->rx_hdr_size),
					 DMA_FROM_DEVICE);
		} else if (pdata->dev->mtu > DWC_ETH_QOS_ETH_FRAME_LEN) {
			dma_unmap_page(GET_MEM_PDEV_DEV, buffer->dma,
				       PAGE_SIZE, DMA_FROM_DEVICE);
		} else {
			dma_unmap_single(GET_MEM_PDEV_DEV, buffer->dma,
					 pdata->rx_buffer_len, DMA_FROM_DEVICE);
		}
		buffer->dma = 0;
	}

	/* unmap the second buffer */
	if (buffer->dma2) {
		dma_unmap_page(GET_MEM_PDEV_DEV, buffer->dma2,
			       PAGE_SIZE, DMA_FROM_DEVICE);
		buffer->dma2 = 0;
	}

	/* page1 will be present only if JUMBO is enabled */
	if (buffer->page) {
		put_page(buffer->page);
		buffer->page = NULL;
	}
	/* page2 will be present if JUMBO/SPLIT HDR is enabled */
	if (buffer->page2) {
		put_page(buffer->page2);
		buffer->page2 = NULL;
	}

	if (buffer->skb) {
		dev_kfree_skb_any(buffer->skb);
		buffer->skb = NULL;
	}

	DBGPR("<--DWC_ETH_QOS_unmap_rx_skb\n");
}

/*!
 * \brief API to re-allocate the new skb to rx descriptors.
 *
 * \details This function is used to re-allocate & re-assign the new skb to
 * receive descriptors from which driver has read the data. Also ownership bit
 * and other bits are reset so that device can reuse the descriptors.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \return void.
 */

static void DWC_ETH_QOS_re_alloc_skb(struct DWC_ETH_QOS_prv_data *pdata,
				     UINT qinx)
{
	int i;
	struct DWC_ETH_QOS_rx_wrapper_descriptor *desc_data =
	    GET_RX_WRAPPER_DESC(qinx);
	struct DWC_ETH_QOS_rx_buffer *buffer = NULL;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int tail_idx;

	DBGPR("%s: desc_data->skb_realloc_idx = %d qinx = %u\n",
	      __func__, desc_data->skb_realloc_idx, qinx);
		  
	if (pdata->ipa_enabled && qinx == IPA_DMA_RX_CH) {
		EMACINFO("skb re-allocation is not required for RXCH0 for IPA \n");
		return;
	}

	for (i = 0; i < desc_data->dirty_rx; i++) {
		buffer = GET_RX_BUF_PTR(qinx, desc_data->skb_realloc_idx);
		/* allocate skb & assign to each desc */
		if (pdata->alloc_rx_buf(pdata, buffer, qinx, GFP_ATOMIC)) {
			EMACERR("Failed to re allocate skb\n");
			pdata->xstats.q_re_alloc_rx_buf_failed[qinx]++;
			break;
		}

		/* rx_desc_reset */
		wmb();
		hw_if->rx_desc_reset(desc_data->skb_realloc_idx, pdata,
				     buffer->inte, qinx);
		INCR_RX_DESC_INDEX(desc_data->skb_realloc_idx, 1, pdata->rx_queue[qinx].desc_cnt);
	}
	tail_idx = desc_data->skb_realloc_idx;
	DECR_RX_DESC_INDEX(tail_idx, pdata->rx_queue[qinx].desc_cnt);
	hw_if->update_rx_tail_ptr(qinx,
		GET_RX_DESC_DMA_ADDR(qinx, tail_idx));
	desc_data->dirty_rx = 0;

	DBGPR("<--DWC_ETH_QOS_re_alloc_skb\n");
}

/*!
 * \brief API to initialize the function pointers.
 *
 * \details This function is called in probe to initialize all the function
 * pointers which are used in other functions to manage edscriptors.
 *
 * \param[in] desc_if - pointer to desc_if_struct structure.
 *
 * \return void.
 */

void DWC_ETH_QOS_init_function_ptrs_desc(struct desc_if_struct *desc_if)
{
	DBGPR("-->DWC_ETH_QOS_init_function_ptrs_desc\n");

	desc_if->alloc_queue_struct = DWC_ETH_QOS_alloc_queue_struct;
	desc_if->free_queue_struct = DWC_ETH_QOS_free_queue_struct;
	desc_if->alloc_buff_and_desc = allocate_buffer_and_desc;
	desc_if->realloc_skb = DWC_ETH_QOS_re_alloc_skb;
	desc_if->unmap_rx_skb = DWC_ETH_QOS_unmap_rx_skb;
	desc_if->unmap_tx_skb = DWC_ETH_QOS_unmap_tx_skb;
	desc_if->map_tx_skb = DWC_ETH_QOS_map_skb;
	desc_if->tx_free_mem = DWC_ETH_QOS_tx_free_mem;
	desc_if->rx_free_mem = DWC_ETH_QOS_rx_free_mem;
	desc_if->wrapper_tx_desc_init = DWC_ETH_QOS_wrapper_tx_descriptor_init;
	desc_if->wrapper_tx_desc_init_single_q =
	    DWC_ETH_QOS_wrapper_tx_descriptor_init_single_q;
	desc_if->wrapper_rx_desc_init = DWC_ETH_QOS_wrapper_rx_descriptor_init;
	desc_if->wrapper_rx_desc_init_single_q =
	    DWC_ETH_QOS_wrapper_rx_descriptor_init_single_q;

	desc_if->rx_skb_free_mem = DWC_ETH_QOS_rx_skb_free_mem;
	desc_if->rx_skb_free_mem_single_q =
		DWC_ETH_QOS_rx_skb_free_mem_single_q;
	desc_if->tx_skb_free_mem = DWC_ETH_QOS_tx_skb_free_mem;
	desc_if->tx_skb_free_mem_single_q =
		DWC_ETH_QOS_tx_skb_free_mem_single_q;

	desc_if->handle_tso = DWC_ETH_QOS_handle_tso;

	DBGPR("<--DWC_ETH_QOS_init_function_ptrs_desc\n");
}
