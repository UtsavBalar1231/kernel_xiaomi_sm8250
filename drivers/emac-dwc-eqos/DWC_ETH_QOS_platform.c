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
/*!@file: DWC_ETH_QOS.c
 * @brief: Driver functions.
 */
#include "DWC_ETH_QOS_yheader.h"
#include "DWC_ETH_QOS_ipa.h"

static UCHAR dev_addr[6] = {0, 0x55, 0x7b, 0xb5, 0x7d, 0xf7};
struct DWC_ETH_QOS_res_data dwc_eth_qos_res_data = {0, };
static struct msm_bus_scale_pdata *emac_bus_scale_vec = NULL;

ULONG dwc_eth_qos_base_addr;
ULONG dwc_rgmii_io_csr_base_addr;
struct DWC_ETH_QOS_prv_data *gDWC_ETH_QOS_prv_data;
struct emac_emb_smmu_cb_ctx emac_emb_smmu_ctx = {0};

int ipa_offload_en = 1;
module_param(ipa_offload_en, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(ipa_offload_en,
		 "Enable IPA offload [0-DISABLE, 1-ENABLE]");

void DWC_ETH_QOS_init_all_fptrs(struct DWC_ETH_QOS_prv_data *pdata)
{
	DWC_ETH_QOS_init_function_ptrs_dev(&pdata->hw_if);
	DWC_ETH_QOS_init_function_ptrs_desc(&pdata->desc_if);
}

#ifdef PER_CH_INT
static int DWC_ETH_QOS_get_per_ch_config(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *resource;

	/* Per channel Tx interrtups */
	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"tx-ch0-intr");
	if (!resource) {
		EMACERR("get tx-ch0-intr resource failed\n");
		ret = -ENODEV;
		goto err_out;
	}
	dwc_eth_qos_res_data.tx_ch_intr[0] = resource->start;
	EMACDBG("tx-ch0-intr = %d\n", dwc_eth_qos_res_data.tx_ch_intr[0]);

	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"tx-ch1-intr");
	if (!resource) {
		EMACERR("get tx-ch1-intr resource failed\n");
		ret = -ENODEV;
		goto err_out;
	}
	dwc_eth_qos_res_data.tx_ch_intr[1] = resource->start;
	EMACDBG("tx-ch1-intr = %d\n", dwc_eth_qos_res_data.tx_ch_intr[1]);

	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"tx-ch2-intr");
	if (!resource) {
		EMACERR("get tx-ch2-intr resource failed\n");
		ret = -ENODEV;
		goto err_out;
	}
	dwc_eth_qos_res_data.tx_ch_intr[2] = resource->start;
	EMACDBG("tx-ch2-intr = %d\n", dwc_eth_qos_res_data.tx_ch_intr[2]);

	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"tx-ch3-intr");
	if (!resource) {
		EMACERR("get tx-ch3-intr resource failed\n");
		ret = -ENODEV;
		goto err_out;
	}
	dwc_eth_qos_res_data.tx_ch_intr[3] = resource->start;
	EMACDBG("tx-ch3-intr = %d\n", dwc_eth_qos_res_data.tx_ch_intr[3]);

	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"tx-ch4-intr");
	if (!resource) {
		EMACERR("get tx-ch4-intr resource failed\n");
		ret = -ENODEV;
		goto err_out;
	}
	dwc_eth_qos_res_data.tx_ch_intr[4] = resource->start;
	EMACDBG("tx-ch4-intr = %d\n", dwc_eth_qos_res_data.tx_ch_intr[4]);

	/* Per channel Rx Interrupts */
	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"rx-ch0-intr");
	if (!resource) {
		EMACERR("get rx-ch0-intr resource failed\n");
		ret = -ENODEV;
		goto err_out;
	}
	dwc_eth_qos_res_data.rx_ch_intr[0] = resource->start;
	EMACDBG("rx-ch0-intr = %d\n", dwc_eth_qos_res_data.rx_ch_intr[0]);

	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"rx-ch1-intr");
	if (!resource) {
		EMACERR("get rx-ch1-intr resource failed\n");
		ret = -ENODEV;
		goto err_out;
	}
	dwc_eth_qos_res_data.rx_ch_intr[1] = resource->start;
	EMACDBG("rx-ch1-intr = %d\n", dwc_eth_qos_res_data.rx_ch_intr[1]);

	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"rx-ch2-intr");
	if (!resource) {
		EMACERR("get rx-ch2-intr resource failed\n");
		ret = -ENODEV;
		goto err_out;
	}
	dwc_eth_qos_res_data.rx_ch_intr[2] = resource->start;
	EMACDBG("rx-ch2-intr = %d\n", dwc_eth_qos_res_data.rx_ch_intr[2]);

	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"rx-ch3-intr");
	if (!resource) {
		EMACERR("get rx-ch3-intr resource failed\n");
		ret = -ENODEV;
		goto err_out;
	}
	dwc_eth_qos_res_data.rx_ch_intr[3] = resource->start;
	EMACDBG("rx-ch3-intr = %d\n", dwc_eth_qos_res_data.rx_ch_intr[3]);

err_out:
	return ret;
}
#endif

#ifndef DWC_ETH_QOS_EMULATION_PLATFORM
static int DWC_ETH_QOS_get_bus_config(struct platform_device *pdev)
{
	int out_cnt, in_cnt;
	emac_bus_scale_vec = msm_bus_cl_get_pdata(pdev);
	if (!emac_bus_scale_vec) {
		EMACERR("unable to get bus scaling vector\n");
		return -1;
	}

	EMACDBG("bus name: %s\n", emac_bus_scale_vec->name);
	EMACDBG("num of paths: %d\n", emac_bus_scale_vec->usecase->num_paths);
	EMACDBG("num of use cases: %d\n", emac_bus_scale_vec->num_usecases);

	for (out_cnt=0; out_cnt<emac_bus_scale_vec->num_usecases; out_cnt++) {
		EMACDBG("use case[%d] parameters:\n", out_cnt);
		for (in_cnt=0; in_cnt<emac_bus_scale_vec->usecase->num_paths; in_cnt++)
			EMACDBG("src_port:%d dst_port:%d ab:%llu ib:%llu \n",
				emac_bus_scale_vec->usecase[out_cnt].vectors[in_cnt].src,
				emac_bus_scale_vec->usecase[out_cnt].vectors[in_cnt].dst,
				emac_bus_scale_vec->usecase[out_cnt].vectors[in_cnt].ab,
				emac_bus_scale_vec->usecase[out_cnt].vectors[in_cnt].ib);
	}

	return 0;
}
#endif

static int DWC_ETH_QOS_get_io_macro_config(struct platform_device *pdev)
{
	int ret = 0;
	const char *io_macro_phy_intf = NULL;
	struct device_node *dev_node = NULL;

	dev_node = of_find_node_by_name(pdev->dev.of_node, "io-macro-info");
	if (dev_node == NULL) {
		EMACERR("Failed to find io-macro-info node from device tree\n");
		goto err_out;
	}

	ret = of_property_read_u32(
		dev_node, "io-macro-bypass-mode",
		&dwc_eth_qos_res_data.io_macro_tx_mode_non_id);
	if (ret < 0) {
		EMACERR("Unable to read bypass mode value from device tree\n");
		goto err_out;
	}
	EMACDBG("io-macro-bypass-mode = %d\n", dwc_eth_qos_res_data.io_macro_tx_mode_non_id);

	ret = of_property_read_string(dev_node, "io-interface", &io_macro_phy_intf);
	if (ret < 0) {
		EMACERR("Failed to read io mode cfg from device tree\n");
		goto err_out;
	}
	if (strcasecmp(io_macro_phy_intf, "rgmii") == 0) {
		dwc_eth_qos_res_data.io_macro_phy_intf = RGMII_MODE;
		EMACDBG("io_macro_phy_intf = %d\n", dwc_eth_qos_res_data.io_macro_phy_intf);
	} else if (strcasecmp(io_macro_phy_intf, "rmii") == 0) {
		dwc_eth_qos_res_data.io_macro_phy_intf = RMII_MODE;
		EMACDBG("io_macro_phy_intf = %d\n", dwc_eth_qos_res_data.io_macro_phy_intf);
	} else if (strcasecmp(io_macro_phy_intf, "mii") == 0) {
		dwc_eth_qos_res_data.io_macro_phy_intf = MII_MODE;
		EMACDBG("io_macro_phy_intf = %d\n", dwc_eth_qos_res_data.io_macro_phy_intf);
	} else {
		dwc_eth_qos_res_data.io_macro_phy_intf = RGMII_MODE;
		EMACERR("Incorrect io mode cfg set in device tree, setting it to RGMII\n");
	}

err_out:
	return ret;
}

static int DWC_ETH_QOS_get_phy_intr_config(struct platform_device *pdev)
{
	int ret = 0;
	EMACDBG("Enter\n");

	dwc_eth_qos_res_data.phy_intr = platform_get_irq_byname(pdev, "phy-intr");

	EMACDBG("Exit\n");
	return ret;
}

static void DWC_ETH_QOS_configure_gpio_pins(struct platform_device *pdev)
{
	struct pinctrl *pinctrl;
	struct pinctrl_state *mdc_state;
	struct pinctrl_state *mdio_state;

	struct pinctrl_state *rgmii_txd0_state;
	struct pinctrl_state *rgmii_txd1_state;
	struct pinctrl_state *rgmii_txd2_state;
	struct pinctrl_state *rgmii_txd3_state;
	struct pinctrl_state *rgmii_txc_state;
	struct pinctrl_state *rgmii_tx_ctl_state;

	struct pinctrl_state *rgmii_rxd0_state;
	struct pinctrl_state *rgmii_rxd1_state;
	struct pinctrl_state *rgmii_rxd2_state;
	struct pinctrl_state *rgmii_rxd3_state;
	struct pinctrl_state *rgmii_rxc_state;
	struct pinctrl_state *rgmii_rx_ctl_state;

	int ret = 0;

	EMACDBG("Enter\n");

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		EMACERR("Failed to get pinctrl, err = %d\n", ret);
		return;
	}
	EMACDBG("get pinctrl succeed\n");

	/* MDIO Pin ctlrs*/
	mdc_state = pinctrl_lookup_state(pinctrl, EMAC_MDC);
	if (IS_ERR_OR_NULL(mdc_state)) {
		ret = PTR_ERR(mdc_state);
		EMACERR("Failed to get mdc_state, err = %d\n", ret);
		return;
	}

	EMACDBG("Get mdc_state succeed\n");
	ret = pinctrl_select_state(pinctrl, mdc_state);
	if (ret)
		EMACERR("Unable to set mdc_state state, err = %d\n", ret);
	else
		EMACDBG("Set mdc_state succeed\n");

	mdio_state = pinctrl_lookup_state(pinctrl, EMAC_MDIO);
	if (IS_ERR_OR_NULL(mdc_state)) {
		ret = PTR_ERR(mdc_state);
		EMACERR("Failed to get mdio_state, err = %d\n", ret);
		return;
	}
	EMACDBG("Get mdio_state succeed\n");
	ret = pinctrl_select_state(pinctrl, mdio_state);
	if (ret)
		EMACERR("Unable to set mdio_state state, err = %d\n", ret);
	else
		EMACDBG("Set mdio_state succeed\n");


	/* RGMII Tx Pin ctlrs */
	rgmii_txd0_state = pinctrl_lookup_state(pinctrl, EMAC_RGMII_TXD0);
	if (IS_ERR_OR_NULL(rgmii_txd0_state)) {
		ret = PTR_ERR(rgmii_txd0_state);
		EMACERR("Failed to get rgmii_txd0_state, err = %d\n", ret);
		return;
	}
	EMACDBG("Get rgmii_txd0_state succeed\n");
	ret = pinctrl_select_state(pinctrl, rgmii_txd0_state);
	if (ret)
		EMACERR("Unable to set rgmii_txd0_state state, err = %d\n", ret);
	else
		EMACDBG("Set rgmii_txd0_state succeed\n");

	rgmii_txd1_state = pinctrl_lookup_state(pinctrl, EMAC_RGMII_TXD1);
	if (IS_ERR_OR_NULL(rgmii_txd1_state)) {
		ret = PTR_ERR(rgmii_txd1_state);
		EMACERR("Failed to get rgmii_txd1_state, err = %d\n", ret);
		return;
	}
	EMACDBG("Get rgmii_txd1_state succeed\n");
	ret = pinctrl_select_state(pinctrl, rgmii_txd1_state);
	if (ret)
		EMACERR("Unable to set rgmii_txd1_state state, err = %d\n", ret);
	else
		EMACDBG("Set rgmii_txd1_state succeed\n");

	rgmii_txd2_state = pinctrl_lookup_state(pinctrl, EMAC_RGMII_TXD2);
	if (IS_ERR_OR_NULL(rgmii_txd2_state)) {
		ret = PTR_ERR(rgmii_txd2_state);
		EMACERR("Failed to get rgmii_txd2_state, err = %d\n", ret);
		return;
	}
	EMACDBG("Get rgmii_txd2_state succeed\n");
	ret = pinctrl_select_state(pinctrl, rgmii_txd2_state);
	if (ret)
		EMACERR("Unable to set rgmii_txd2_state state, err = %d\n", ret);
	else
		EMACDBG("Set rgmii_txd2_state succeed\n");

	rgmii_txd3_state = pinctrl_lookup_state(pinctrl, EMAC_RGMII_TXD3);
	if (IS_ERR_OR_NULL(rgmii_txd3_state)) {
		ret = PTR_ERR(rgmii_txd3_state);
		EMACERR("Failed to get rgmii_txd3_state, err = %d\n", ret);
		return;
	}
	EMACDBG("Get rgmii_txd3_state succeed\n");
	ret = pinctrl_select_state(pinctrl, rgmii_txd3_state);
	if (ret)
		EMACERR("Unable to set rgmii_txd3_state state, err = %d\n", ret);
	else
		EMACDBG("Set rgmii_txd3_state succeed\n");

	rgmii_txc_state = pinctrl_lookup_state(pinctrl, EMAC_RGMII_TXC);
	if (IS_ERR_OR_NULL(rgmii_txc_state)) {
		ret = PTR_ERR(rgmii_txc_state);
		EMACERR("Failed to get rgmii_txc_state, err = %d\n", ret);
		return;
	}
	EMACDBG("Get rgmii_txc_state succeed\n");
	ret = pinctrl_select_state(pinctrl, rgmii_txc_state);
	if (ret)
		EMACERR("Unable to set rgmii_txc_state state, err = %d\n", ret);
	else
		EMACDBG("Set rgmii_txc_state succeed\n");

	rgmii_tx_ctl_state = pinctrl_lookup_state(pinctrl, EMAC_RGMII_TX_CTL);
	if (IS_ERR_OR_NULL(rgmii_tx_ctl_state)) {
		ret = PTR_ERR(rgmii_tx_ctl_state);
		EMACERR("Failed to get rgmii_tx_ctl_state, err = %d\n", ret);
		return;
	}
	EMACDBG("Get rgmii_tx_ctl_state succeed\n");
	ret = pinctrl_select_state(pinctrl, rgmii_tx_ctl_state);
	if (ret)
		EMACERR("Unable to set rgmii_tx_ctl_state state, err = %d\n", ret);
	else
		EMACDBG("Set rgmii_tx_ctl_state succeed\n");

	/* RGMII Rx Pin ctlrs */
	rgmii_rxd0_state = pinctrl_lookup_state(pinctrl, EMAC_RGMII_RXD0);
	if (IS_ERR_OR_NULL(rgmii_rxd0_state)) {
		ret = PTR_ERR(rgmii_rxd0_state);
		EMACERR("Failed to get rgmii_rxd0_state, err = %d\n", ret);
		return;
	}
	EMACDBG("Get rgmii_rxd0_state succeed\n");
	ret = pinctrl_select_state(pinctrl, rgmii_rxd0_state);
	if (ret)
		EMACERR("Unable to set rgmii_rxd0_state state, err = %d\n", ret);
	else
		EMACDBG("Set rgmii_rxd0_state succeed\n");

	rgmii_rxd1_state = pinctrl_lookup_state(pinctrl, EMAC_RGMII_RXD1);
	if (IS_ERR_OR_NULL(rgmii_rxd1_state)) {
		ret = PTR_ERR(rgmii_rxd1_state);
		EMACERR("Failed to get rgmii_rxd1_state, err = %d\n", ret);
		return;
	}
	EMACDBG("Get rgmii_rxd1_state succeed\n");
	ret = pinctrl_select_state(pinctrl, rgmii_rxd1_state);
	if (ret)
		EMACERR("Unable to set rgmii_rxd1_state state, err = %d\n", ret);
	else
		EMACDBG("Set rgmii_rxd1_state succeed\n");

	rgmii_rxd2_state = pinctrl_lookup_state(pinctrl, EMAC_RGMII_RXD2);
	if (IS_ERR_OR_NULL(rgmii_rxd2_state)) {
		ret = PTR_ERR(rgmii_rxd2_state);
		EMACERR("Failed to get rgmii_rxd2_state, err = %d\n", ret);
		return;
	}
	EMACDBG("Get rgmii_rxd2_state succeed\n");
	ret = pinctrl_select_state(pinctrl, rgmii_rxd2_state);
	if (ret)
		EMACERR("Unable to set rgmii_rxd2_state state, err = %d\n", ret);
	else
		EMACDBG("Set rgmii_rxd2_state succeed\n");

	rgmii_rxd3_state = pinctrl_lookup_state(pinctrl, EMAC_RGMII_RXD3);
	if (IS_ERR_OR_NULL(rgmii_rxd3_state)) {
		ret = PTR_ERR(rgmii_rxd3_state);
		EMACERR("Failed to get rgmii_rxd3_state, err = %d\n", ret);
		return;
	}
	EMACDBG("Get rgmii_rxd3_state succeed\n");
	ret = pinctrl_select_state(pinctrl, rgmii_rxd3_state);
	if (ret)
		EMACERR("Unable to set rgmii_rxd3_state state, err = %d\n", ret);
	else
		EMACDBG("Set rgmii_rxd3_state succeed\n");

	rgmii_rxc_state = pinctrl_lookup_state(pinctrl, EMAC_RGMII_RXC);
	if (IS_ERR_OR_NULL(rgmii_rxc_state)) {
		ret = PTR_ERR(rgmii_rxc_state);
		EMACERR("Failed to get rgmii_rxc_state, err = %d\n", ret);
		return;
	}
	EMACDBG("Get rgmii_rxc_state succeed\n");
	ret = pinctrl_select_state(pinctrl, rgmii_rxc_state);
	if (ret)
		EMACERR("Unable to set rgmii_rxc_state state, err = %d\n", ret);
	else
		EMACDBG("Set rgmii_rxc_state succeed\n");

	rgmii_rx_ctl_state = pinctrl_lookup_state(pinctrl, EMAC_RGMII_RX_CTL);
	if (IS_ERR_OR_NULL(rgmii_rx_ctl_state)) {
		ret = PTR_ERR(rgmii_rx_ctl_state);
		EMACERR("Failed to get rgmii_rx_ctl_state, err = %d\n", ret);
		return;
	}
	EMACDBG("Get rgmii_rx_ctl_state succeed\n");
	ret = pinctrl_select_state(pinctrl, rgmii_rx_ctl_state);
	if (ret)
		EMACERR("Unable to set rgmii_rx_ctl_state state, err = %d\n", ret);
	else
		EMACDBG("Set rgmii_rx_ctl_state succeed\n");

	EMACDBG("Exit\n");

	return;
}

static int DWC_ETH_QOS_get_dts_config(struct platform_device *pdev)
{
	struct resource *resource = NULL;
	int ret = 0;

	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"emac-base");
	if (!resource) {
		EMACERR("get emac-base resource failed\n");
		ret = -ENODEV;
		goto err_out;
	}
	dwc_eth_qos_res_data.emac_mem_base = resource->start;
	dwc_eth_qos_res_data.emac_mem_size = resource_size(resource);
	EMACDBG("emac-base = 0x%x, size = 0x%x\n",
			dwc_eth_qos_res_data.emac_mem_base,
			dwc_eth_qos_res_data.emac_mem_size);

	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"rgmii-base");
	if (!resource) {
		EMACERR("get rgmii-base resource failed\n");
		ret = -ENODEV;
		goto err_out;
	}
	dwc_eth_qos_res_data.rgmii_mem_base = resource->start;
	dwc_eth_qos_res_data.rgmii_mem_size = resource_size(resource);
	EMACDBG("rgmii-base = 0x%x, size = 0x%x\n",
			dwc_eth_qos_res_data.rgmii_mem_base,
			dwc_eth_qos_res_data.rgmii_mem_size);

	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"sbd-intr");
	if (!resource) {
		EMACERR("get sbd-intr resource failed\n");
		ret = -ENODEV;
		goto err_out;
	}
	dwc_eth_qos_res_data.sbd_intr = resource->start;
	EMACDBG("sbd-intr = %d\n", dwc_eth_qos_res_data.sbd_intr);

	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"lpi-intr");
	if (!resource) {
		EMACERR("get lpi-intr resource failed\n");
		ret = -ENODEV;
		goto err_out;
	}
	dwc_eth_qos_res_data.lpi_intr = resource->start;
	EMACDBG("lpi-intr = %d\n", dwc_eth_qos_res_data.lpi_intr);

	/* Read emac core version value from dtsi */
	ret = of_property_read_u32(pdev->dev.of_node, "emac-core-version",
				&dwc_eth_qos_res_data.emac_hw_version_type);
	if (ret) {
		EMACDBG(":resource emac-hw-ver! not present in dtsi\n");
		dwc_eth_qos_res_data.emac_hw_version_type = EMAC_HW_None;
	}
	EMACDBG(": emac_core_version = %d\n", dwc_eth_qos_res_data.emac_hw_version_type);

	ret = DWC_ETH_QOS_get_io_macro_config(pdev);
	if (ret)
		goto err_out;

#ifndef DWC_ETH_QOS_EMULATION_PLATFORM
	ret = DWC_ETH_QOS_get_bus_config(pdev);
	if (ret)
		goto err_out;
#endif

	ret = DWC_ETH_QOS_get_phy_intr_config(pdev);
	if (ret)
		goto err_out;

#ifdef PER_CH_INT
	ret = DWC_ETH_QOS_get_per_ch_config(pdev);
#endif

	if (of_property_read_bool(pdev->dev.of_node, "qcom,phy-intr-redirect")) {
		dwc_eth_qos_res_data.is_gpio_phy_intr_redirect = true;
		EMACDBG("qcom,phy-intr-redirect 124 present\n");
	}

	if (of_property_read_bool(pdev->dev.of_node, "qcom,phy-reset")) {
		dwc_eth_qos_res_data.is_gpio_phy_reset = true;
		EMACDBG("qcom,phy-reset present\n");
	}

	if (of_property_read_bool(pdev->dev.of_node, "pinctrl-names")) {
		dwc_eth_qos_res_data.is_pinctrl_names = true;
		EMACDBG("qcom,pinctrl-names present\n");
	}

	return ret;

err_out:
	if (emac_bus_scale_vec)
		msm_bus_cl_clear_pdata(emac_bus_scale_vec);
	return ret;
}

static int DWC_ETH_QOS_ioremap(void)
{
	int ret = 0;

	dwc_eth_qos_base_addr = (ULONG)ioremap(
	   dwc_eth_qos_res_data.emac_mem_base,
	   dwc_eth_qos_res_data.emac_mem_size);
	if ((void __iomem *)dwc_eth_qos_base_addr == NULL) {
		EMACERR("cannot map emac reg memory, aborting\n");
		ret = -EIO;
		goto err_out_map_failed;
	}
	EMACDBG("ETH_QOS_BASE_ADDR = %#lx\n",
			dwc_eth_qos_base_addr);

	dwc_rgmii_io_csr_base_addr = (ULONG)ioremap(
	   dwc_eth_qos_res_data.rgmii_mem_base,
	   dwc_eth_qos_res_data.rgmii_mem_size);
	if ((void __iomem *)dwc_rgmii_io_csr_base_addr == NULL) {
		EMACERR("cannot map rgmii reg memory, aborting\n");
		ret = -EIO;
		goto err_out_rgmii_map_failed;
	}
	EMACDBG("ETH_QOS_RGMII_IO_BASE_ADDR = %#lx\n",
			dwc_rgmii_io_csr_base_addr);

	return ret;

err_out_rgmii_map_failed:
		iounmap((void __iomem *)dwc_eth_qos_base_addr);

err_out_map_failed:
	return ret;
}


void DWC_ETH_QOS_scale_clks(struct DWC_ETH_QOS_prv_data *pdata, int speed)
{
	u32 vote_idx = VOTE_IDX_0MBPS;

	EMACDBG("Enter\n");

	if (pdata->bus_hdl) {
		switch (speed) {
		case SPEED_1000:
			vote_idx = VOTE_IDX_1000MBPS;
			break;
		case SPEED_100:
			vote_idx = VOTE_IDX_100MBPS;
			break;
		case SPEED_10:
			vote_idx = VOTE_IDX_10MBPS;
			break;
		default:
			vote_idx = VOTE_IDX_0MBPS;
			break;
		}

		if (msm_bus_scale_client_update_request(
			  pdata->bus_hdl, vote_idx)) WARN_ON(1);
	}

	EMACDBG("Exit\n");
}

void DWC_ETH_QOS_disable_clks(void)
{
	if (dwc_eth_qos_res_data.axi_clk)
		clk_disable_unprepare(dwc_eth_qos_res_data.axi_clk);

	dwc_eth_qos_res_data.axi_clk = NULL;

	if (dwc_eth_qos_res_data.ahb_clk)
		clk_disable_unprepare(dwc_eth_qos_res_data.ahb_clk);

	dwc_eth_qos_res_data.ahb_clk = NULL;

	if (dwc_eth_qos_res_data.ptp_clk)
		clk_disable_unprepare(dwc_eth_qos_res_data.ptp_clk);

	dwc_eth_qos_res_data.ptp_clk = NULL;

	if (dwc_eth_qos_res_data.rgmii_clk)
		clk_disable_unprepare(dwc_eth_qos_res_data.rgmii_clk);

	dwc_eth_qos_res_data.rgmii_clk = NULL;

}

static int DWC_ETH_QOS_get_clks(struct device *dev)
{
	int ret = 0;
	const char* axi_clock_name;
	const char* ahb_clock_name;
	const char* rgmii_clock_name;
	const char* ptp_clock_name;

	dwc_eth_qos_res_data.axi_clk = NULL;
	dwc_eth_qos_res_data.ahb_clk = NULL;
	dwc_eth_qos_res_data.rgmii_clk = NULL;
	dwc_eth_qos_res_data.ptp_clk = NULL;

	if (dwc_eth_qos_res_data.emac_hw_version_type == EMAC_HW_v2_1_0) {
		/* EMAC core version 2.1.0 clocks */
		axi_clock_name = "emac_axi_clk";
		ahb_clock_name = "emac_slv_ahb_clk";
		rgmii_clock_name = "emac_rgmii_clk";
		ptp_clock_name = "emac_ptp_clk";
	} else {
		/* Default values are for EMAC core version 2.0.0 clocks */
		axi_clock_name = "eth_axi_clk";
		ahb_clock_name = "eth_slave_ahb_clk";
		rgmii_clock_name = "eth_rgmii_clk";
		ptp_clock_name = "eth_ptp_clk";
	}

	dwc_eth_qos_res_data.axi_clk = devm_clk_get(dev, axi_clock_name);
	if (IS_ERR(dwc_eth_qos_res_data.axi_clk)) {
		if (dwc_eth_qos_res_data.axi_clk != ERR_PTR(-EPROBE_DEFER)) {
			EMACERR("unable to get axi clk\n");
			return -EIO;
		}
	}

	dwc_eth_qos_res_data.ahb_clk = devm_clk_get(dev, ahb_clock_name);
	if (IS_ERR(dwc_eth_qos_res_data.ahb_clk)) {
		if (dwc_eth_qos_res_data.ahb_clk != ERR_PTR(-EPROBE_DEFER)) {
			EMACERR("unable to get ahb clk\n");
			return -EIO;
		}
	}

	dwc_eth_qos_res_data.rgmii_clk = devm_clk_get(dev, rgmii_clock_name);
	if (IS_ERR(dwc_eth_qos_res_data.rgmii_clk)) {
		if (dwc_eth_qos_res_data.rgmii_clk != ERR_PTR(-EPROBE_DEFER)) {
			EMACERR("unable to get rgmii clk\n");
			return -EIO;
		}
	}

	dwc_eth_qos_res_data.ptp_clk = devm_clk_get(dev, ptp_clock_name);
	if (IS_ERR(dwc_eth_qos_res_data.ptp_clk)) {
		if (dwc_eth_qos_res_data.ptp_clk != ERR_PTR(-EPROBE_DEFER)) {
			EMACERR("unable to get ptp_clk\n");
			return -EIO;
		}
	}

	ret = clk_prepare_enable(dwc_eth_qos_res_data.axi_clk);

	if (ret) {
		EMACERR("Failed to enable axi_clk\n");
		goto fail_clk;
	}

	ret = clk_prepare_enable(dwc_eth_qos_res_data.ahb_clk);

	if (ret) {
		EMACERR("Failed to enable ahb_clk\n");
		goto fail_clk;
	}

	ret = clk_prepare_enable(dwc_eth_qos_res_data.rgmii_clk);

	if (ret) {
		EMACERR("Failed to enable rgmii_clk\n");
		goto fail_clk;
	}

	ret = clk_prepare_enable(dwc_eth_qos_res_data.ptp_clk);

	if (ret) {
		EMACERR("Failed to enable ptp_clk\n");
		goto fail_clk;
	}

	ret = clk_set_rate(dwc_eth_qos_res_data.ptp_clk, DWC_ETH_QOS_SYSCLOCK);

	if (ret) {
		EMACERR("Failed to set rate for ptp_clk\n");
		goto fail_clk;
	}

	return ret;

fail_clk:
	DWC_ETH_QOS_disable_clks();
	return ret;
}

static int DWC_ETH_QOS_panic_notifier(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	if (gDWC_ETH_QOS_prv_data) {
		DBGPR("DWC_ETH_QOS: 0x%pK\n", gDWC_ETH_QOS_prv_data);
		DWC_ETH_QOS_ipa_stats_read(gDWC_ETH_QOS_prv_data);
		DWC_ETH_QOS_dma_desc_stats_read(gDWC_ETH_QOS_prv_data);
	}
	return NOTIFY_DONE;
}

static struct notifier_block DWC_ETH_QOS_panic_blk = {
	.notifier_call  = DWC_ETH_QOS_panic_notifier,
};

static void DWC_ETH_QOS_disable_regulators(void)
{
	if (dwc_eth_qos_res_data.reg_rgmii)
		regulator_disable(dwc_eth_qos_res_data.reg_rgmii);

	if (dwc_eth_qos_res_data.reg_emac_phy)
		regulator_disable(dwc_eth_qos_res_data.reg_emac_phy);

	if (dwc_eth_qos_res_data.reg_rgmii_io_pads)
		regulator_disable(dwc_eth_qos_res_data.reg_rgmii_io_pads);

	if (dwc_eth_qos_res_data.gdsc_emac)
		regulator_disable(dwc_eth_qos_res_data.gdsc_emac);
}

static int DWC_ETH_QOS_init_regulators(struct device *dev)
{
	int ret = 0;

	if (of_property_read_bool(dev->of_node, "gdsc_emac-supply")) {
		dwc_eth_qos_res_data.gdsc_emac =
			devm_regulator_get(dev, EMAC_GDSC_EMAC_NAME);
		if (IS_ERR(dwc_eth_qos_res_data.gdsc_emac)) {
			EMACERR("Can not get <%s>\n", EMAC_GDSC_EMAC_NAME);
			return PTR_ERR(dwc_eth_qos_res_data.gdsc_emac);
		}

		ret = regulator_enable(dwc_eth_qos_res_data.gdsc_emac);
		if (ret) {
			EMACERR("Can not enable <%s>\n", EMAC_GDSC_EMAC_NAME);
			goto reg_error;
		}
		EMACDBG("Enabled <%s>\n", EMAC_GDSC_EMAC_NAME);
	}

	if (of_property_read_bool(dev->of_node, "vreg_rgmii-supply")) {
		dwc_eth_qos_res_data.reg_rgmii =
			devm_regulator_get(dev, EMAC_VREG_RGMII_NAME);
		if (IS_ERR(dwc_eth_qos_res_data.reg_rgmii)) {
			EMACERR("Can not get <%s>\n", EMAC_VREG_RGMII_NAME);
			return PTR_ERR(dwc_eth_qos_res_data.reg_rgmii);
		}
		ret = regulator_enable(dwc_eth_qos_res_data.reg_rgmii);
		if (ret) {
			EMACERR("Cannot enable <%s>\n", EMAC_VREG_RGMII_NAME);
			goto reg_error;
		}
	}

	if (of_property_read_bool(dev->of_node, "vreg_emac_phy-supply")) {
		dwc_eth_qos_res_data.reg_emac_phy =
			devm_regulator_get(dev, EMAC_VREG_EMAC_PHY_NAME);
		if (IS_ERR(dwc_eth_qos_res_data.reg_emac_phy)) {
			EMACERR("Cannot get <%s>\n", EMAC_VREG_EMAC_PHY_NAME);
			return PTR_ERR(dwc_eth_qos_res_data.reg_emac_phy);
		}
		ret = regulator_enable(dwc_eth_qos_res_data.reg_emac_phy);
		if (ret) {
			EMACERR("Can not enable <%s>\n", EMAC_VREG_EMAC_PHY_NAME);
			goto reg_error;
		}
	}

	if (of_property_read_bool(dev->of_node, "vreg_rgmii_io_pads-supply")) {
		dwc_eth_qos_res_data.reg_rgmii_io_pads =
			devm_regulator_get(dev, EMAC_VREG_RGMII_IO_PADS_NAME);
		if (IS_ERR(dwc_eth_qos_res_data.reg_rgmii_io_pads)) {
			EMACERR("Cannot get <%s>\n", EMAC_VREG_RGMII_IO_PADS_NAME);
			return PTR_ERR(dwc_eth_qos_res_data.reg_rgmii_io_pads);
		}
		ret = regulator_enable(dwc_eth_qos_res_data.reg_rgmii_io_pads);
		if (ret) {
			EMACERR("Can not enable <%s>\n", EMAC_VREG_RGMII_IO_PADS_NAME);
			goto reg_error;
		}
	}

	return ret;

reg_error:
	DWC_ETH_QOS_disable_regulators();
	return ret;
}

static int setup_gpio_input_common
	(struct device *dev, const char *name, int *gpio)
{
	int ret = 0;

	if (of_find_property(dev->of_node, name, NULL)) {
		*gpio = ret = of_get_named_gpio(dev->of_node, name, 0);
		if (ret >= 0) {
			ret = gpio_request(*gpio, name);
			if (ret) {
				EMACERR("%s: Can't get GPIO %s, ret = %d\n",
						 __func__, name, *gpio);
				*gpio = -1;
				return ret;
			}

			ret = gpio_direction_input(*gpio);
			if (ret) {
				EMACERR(
				   "%s: Can't set GPIO %s direction, ret = %d\n",
				   __func__, name, ret);
				return ret;
			}
		} else {
			if (ret == -EPROBE_DEFER)
				EMACERR("get EMAC_GPIO probe defer\n");
			else
				EMACERR("can't get gpio %s ret %d", name, ret);
			return ret;
		}
	} else {
		EMACERR("can't find gpio %s", name);
		ret = -EINVAL;
	}

	return ret;
}

static int setup_gpio_output_common
	(struct device *dev, const char *name, int *gpio, int value)
{
	int ret = 0;

	if (of_find_property(dev->of_node, name, NULL)) {
		*gpio = ret = of_get_named_gpio(dev->of_node, name, 0);
		if (ret >= 0) {
			ret = gpio_request(*gpio, name);
			if (ret) {
				EMACERR(
				   "%s: Can't get GPIO %s, ret = %d\n",
				   __func__, name, *gpio);
				*gpio = -1;
				return ret;
			}

			ret = gpio_direction_output(*gpio, value);
			if (ret) {
				EMACERR(
				   "%s: Can't set GPIO %s direction, ret = %d\n",
				   __func__, name, ret);
				return ret;
			}
		} else {
			if (ret == -EPROBE_DEFER)
				EMACERR("get EMAC_GPIO probe defer\n");
			else
				EMACERR("can't get gpio %s ret %d", name, ret);
			return ret;
		}
	} else {
		EMACERR("can't find gpio %s", name);
		ret = -EINVAL;
	}

	return ret;
}

static void DWC_ETH_QOS_free_gpios(void)
{
	if (gpio_is_valid(dwc_eth_qos_res_data.gpio_phy_intr_redirect))
		gpio_free(dwc_eth_qos_res_data.gpio_phy_intr_redirect);
	dwc_eth_qos_res_data.gpio_phy_intr_redirect = -1;

	if (gpio_is_valid(dwc_eth_qos_res_data.gpio_phy_reset))
		gpio_free(dwc_eth_qos_res_data.gpio_phy_reset);
	dwc_eth_qos_res_data.gpio_phy_reset = -1;
}

static int DWC_ETH_QOS_init_gpios(struct device *dev)
{
	int ret = 0;

	dwc_eth_qos_res_data.gpio_phy_intr_redirect = -1;
	dwc_eth_qos_res_data.gpio_phy_reset = -1;

	if (dwc_eth_qos_res_data.is_gpio_phy_intr_redirect) {
		ret = setup_gpio_input_common(
			dev, EMAC_GPIO_PHY_INTR_REDIRECT_NAME,
			&dwc_eth_qos_res_data.gpio_phy_intr_redirect);

		if (ret) {
			EMACERR("Failed to setup <%s> gpio\n",
					EMAC_GPIO_PHY_INTR_REDIRECT_NAME);
			goto gpio_error;
		}
	}

	if (dwc_eth_qos_res_data.is_gpio_phy_reset) {
		ret = setup_gpio_output_common(
			dev, EMAC_GPIO_PHY_RESET_NAME,
			&dwc_eth_qos_res_data.gpio_phy_reset, PHY_RESET_GPIO_LOW);

		if (ret) {
			EMACERR("Failed to setup <%s> gpio\n",
					EMAC_GPIO_PHY_RESET_NAME);
			goto gpio_error;
		}
		EMACDBG("PHY is out of reset successfully\n");
	}

	mdelay(1);

	gpio_set_value(dwc_eth_qos_res_data.gpio_phy_reset, PHY_RESET_GPIO_HIGH);

	return ret;

gpio_error:
	DWC_ETH_QOS_free_gpios();
	return ret;
}

static struct of_device_id DWC_ETH_QOS_plat_drv_match[] = {
	{ .compatible = "qcom,emac-dwc-eqos", },
	{ .compatible = "qcom,emac-smmu-embedded", },
	{}
};

static int DWC_ETH_QOS_configure_netdevice(struct platform_device *pdev)
{
	struct DWC_ETH_QOS_prv_data *pdata = NULL;
	struct net_device *dev = NULL;
	int ret = 0, i;
	struct hw_if_struct *hw_if = NULL;
	struct desc_if_struct *desc_if = NULL;
	UCHAR tx_q_count = 0, rx_q_count = 0;

	EMACDBG("--> DWC_ETH_QOS_configure_netdevice\n");

	/* queue count */
	tx_q_count = get_tx_queue_count();
	rx_q_count = get_rx_queue_count();

	dev = alloc_etherdev_mqs(sizeof(struct DWC_ETH_QOS_prv_data),
				 tx_q_count, rx_q_count);
	if (!dev) {
		dev_alert(&pdev->dev, "Unable to alloc new net device\n");
		ret = -ENOMEM;
		goto err_out_dev_failed;
	}
	dev->dev_addr[0] = dev_addr[0];
	dev->dev_addr[1] = dev_addr[1];
	dev->dev_addr[2] = dev_addr[2];
	dev->dev_addr[3] = dev_addr[3];
	dev->dev_addr[4] = dev_addr[4];
	dev->dev_addr[5] = dev_addr[5];

	/* IEMAC TODO: Register base address
	 * dev->base_addr = dwc_eth_qos_base_addr;
	 */
	SET_NETDEV_DEV(dev, &pdev->dev);
	pdata = netdev_priv(dev);
	gDWC_ETH_QOS_prv_data = pdata;
	atomic_notifier_chain_register(&panic_notifier_list,
			&DWC_ETH_QOS_panic_blk);

	EMACDBG("gDWC_ETH_QOS_prv_data 0x%pK \n", gDWC_ETH_QOS_prv_data);
	DWC_ETH_QOS_init_all_fptrs(pdata);
	hw_if = &pdata->hw_if;
	desc_if = &pdata->desc_if;
	pdata->res_data = &dwc_eth_qos_res_data;

	if (emac_bus_scale_vec) {
		pdata->bus_scale_vec = emac_bus_scale_vec;
		pdata->bus_hdl = msm_bus_scale_register_client(pdata->bus_scale_vec);
		if (!pdata->bus_hdl) {
			EMACERR("unable to register bus\n");
			ret = -EIO;
			goto err_bus_reg_failed;
		}
	}

	platform_set_drvdata(pdev, dev);
	pdata->pdev = pdev;
	pdata->dev = dev;
	pdata->tx_queue_cnt = tx_q_count;
	pdata->rx_queue_cnt = rx_q_count;
	pdata->lpi_irq = dwc_eth_qos_res_data.lpi_intr;
	pdata->io_macro_tx_mode_non_id =
		dwc_eth_qos_res_data.io_macro_tx_mode_non_id;
	pdata->io_macro_phy_intf = dwc_eth_qos_res_data.io_macro_phy_intf;

#ifdef DWC_ETH_QOS_CONFIG_DEBUGFS
	/* to give prv data to debugfs */
	DWC_ETH_QOS_get_pdata(pdata);
#endif

	/* store emac hw version in pdata*/
	pdata->emac_hw_version_type = dwc_eth_qos_res_data.emac_hw_version_type;
	/* issue software reset to device */
	hw_if->exit();
	/* IEMAC: Find and Read the IRQ from DTS */
	dev->irq = dwc_eth_qos_res_data.sbd_intr;
	pdata->phy_irq = dwc_eth_qos_res_data.phy_intr;

	/* Check if IPA is supported */
	if (ipa_offload_en == 1)
		pdata->ipa_enabled = EMAC_IPA_CAPABLE;

#ifdef DWC_ETH_QOS_CONFIG_PGTEST
	pdata->ipa_enabled = 0;
#endif

	EMACINFO("EMAC IPA enabled: %d\n", pdata->ipa_enabled);
	if (pdata->ipa_enabled) {
		pdata->prv_ipa.ipa_ver = ipa_get_hw_type();
		device_init_wakeup(&pdev->dev, 1);
		mutex_init(&pdata->prv_ipa.ipa_lock);
	}

	DWC_ETH_QOS_get_all_hw_features(pdata);
	DWC_ETH_QOS_print_all_hw_features(pdata);

	ret = desc_if->alloc_queue_struct(pdata);
	if (ret < 0) {
		dev_alert(&pdev->dev, "ERROR: Unable to alloc Tx/Rx queue\n");
		goto err_out_q_alloc_failed;
	}

	dev->netdev_ops = DWC_ETH_QOS_get_netdev_ops();

	pdata->interface = DWC_ETH_QOS_get_phy_interface(pdata);

	/* Bypass PHYLIB for TBI, RTBI and SGMII interface */
	if (pdata->hw_feat.sma_sel == 1) {
		ret = DWC_ETH_QOS_mdio_register(dev);
		if (ret < 0) {
			dev_alert(&pdev->dev, "MDIO bus (id %d) registration failed\n",
					  pdata->bus_id);
			goto err_out_mdio_reg;
		}
	} else {
		dev_alert(&pdev->dev, "%s: MDIO is not present\n\n", DEV_NAME);
	}

#ifndef DWC_ETH_QOS_CONFIG_PGTEST
	/* enabling and registration of irq with magic wakeup */
	if (pdata->hw_feat.mgk_sel == 1) {
		device_set_wakeup_capable(&pdev->dev, 1);
		pdata->wolopts = WAKE_MAGIC;
		enable_irq_wake(dev->irq);
	}

	for (i = 0; i < DWC_ETH_QOS_RX_QUEUE_CNT; i++) {
		struct DWC_ETH_QOS_rx_queue *rx_queue = GET_RX_QUEUE_PTR(i);

		if (pdata->ipa_enabled && i == IPA_DMA_RX_CH)
			 continue;

		netif_napi_add(dev, &rx_queue->napi, DWC_ETH_QOS_poll_mq,
			  (64 * DWC_ETH_QOS_RX_QUEUE_CNT));
	}

	dev->ethtool_ops = DWC_ETH_QOS_get_ethtool_ops();
	DWC_ETH_QOS_dma_desc_stats_init(pdata);

	if (pdata->hw_feat.tso_en) {
		dev->hw_features = NETIF_F_TSO;
#ifdef DWC_ETH_QOS_CONFIG_UFO
		dev->hw_features |= NETIF_F_UFO;
#endif
		dev->hw_features |= NETIF_F_SG;
		dev->hw_features |= NETIF_F_IP_CSUM;
		dev->hw_features |= NETIF_F_IPV6_CSUM;
		EMACDBG("Supports TSO, SG and TX COE\n");
	} else if (pdata->hw_feat.tx_coe_sel) {
		dev->hw_features = NETIF_F_IP_CSUM;
		dev->hw_features |= NETIF_F_IPV6_CSUM;
		EMACDBG("Supports TX COE\n");
	}

	if (pdata->hw_feat.rx_coe_sel) {
		dev->hw_features |= NETIF_F_RXCSUM;
		dev->hw_features |= NETIF_F_LRO;
		EMACDBG("Supports RX COE and LRO\n");
	}
#ifdef DWC_ETH_QOS_ENABLE_VLAN_TAG
	dev->vlan_features |= dev->hw_features;
	dev->hw_features |= NETIF_F_HW_VLAN_CTAG_RX;
	if (pdata->hw_feat.sa_vlan_ins) {
		dev->hw_features |= NETIF_F_HW_VLAN_CTAG_TX;
		EMACDBG("VLAN Feature enabled\n");
	}
	if (pdata->hw_feat.vlan_hash_en) {
		dev->hw_features |= NETIF_F_HW_VLAN_CTAG_FILTER;
		EMACDBG("VLAN HASH Filtering enabled\n");
	}
#endif /* end of DWC_ETH_QOS_ENABLE_VLAN_TAG */
	dev->features |= dev->hw_features;
	pdata->dev_state |= dev->features;

	DWC_ETH_QOS_init_rx_coalesce(pdata);

#ifdef DWC_ETH_QOS_CONFIG_PTP
	DWC_ETH_QOS_ptp_init(pdata);
#endif /* end of DWC_ETH_QOS_CONFIG_PTP */

#endif /* end of DWC_ETH_QOS_CONFIG_PGTEST */

	spin_lock_init(&pdata->lock);
	mutex_init(&pdata->mlock);
	spin_lock_init(&pdata->tx_lock);
	mutex_init(&pdata->pmt_lock);

#ifdef DWC_ETH_QOS_CONFIG_PGTEST
	init_pg_tx_wq(pdata);

	ret = DWC_ETH_QOS_alloc_pg(pdata);
	if (ret < 0) {
		dev_alert(&pdev->dev, "ERROR:Unable to allocate PG memory\n");
		goto err_out_pg_failed;
	}
	dev_alert(&pdev->dev, "\n");
	dev_alert(&pdev->dev, "/*******************************************\n");
	dev_alert(&pdev->dev, "*\n");
	dev_alert(&pdev->dev, "* PACKET GENERATOR MODULE ENABLED IN DRIVER\n");
	dev_alert(&pdev->dev, "*\n");
	dev_alert(&pdev->dev, "*******************************************/\n");
	dev_alert(&pdev->dev, "\n");
#endif /* end of DWC_ETH_QOS_CONFIG_PGTEST */

	ret = register_netdev(dev);
	if (ret) {
		printk(KERN_ALERT "%s: Net device registration failed\n",
			DEV_NAME);
		goto err_out_netdev_failed;
	}

	if (pdata->hw_feat.pcs_sel) {
		netif_carrier_off(dev);
		dev_alert(&pdev->dev, "carrier off till LINK is up\n");
	}

	EMACDBG("<-- DWC_ETH_QOS_configure_netdevice\n");

	return 0;

 err_out_netdev_failed:
#ifdef DWC_ETH_QOS_CONFIG_PTP
	DWC_ETH_QOS_ptp_remove(pdata);
#endif /* end of DWC_ETH_QOS_CONFIG_PTP */

#ifdef DWC_ETH_QOS_CONFIG_PGTEST
	DWC_ETH_QOS_free_pg(pdata);
 err_out_pg_failed:
#endif
	if (pdata->hw_feat.sma_sel == 1)
		DWC_ETH_QOS_mdio_unregister(dev);

 err_out_mdio_reg:
	desc_if->free_queue_struct(pdata);

 err_out_q_alloc_failed:
	platform_set_drvdata(pdev, NULL);

 err_bus_reg_failed:
	if (pdata->bus_hdl) {
		msm_bus_scale_unregister_client(pdata->bus_hdl);
		emac_bus_scale_vec = NULL;
		pdata->bus_scale_vec = NULL;
	}

	gDWC_ETH_QOS_prv_data = NULL;
	atomic_notifier_chain_unregister(&panic_notifier_list,
			&DWC_ETH_QOS_panic_blk);
	free_netdev(dev);

 err_out_dev_failed:
	EMACERR("<-- DWC_ETH_QOS_configure_netdevice\n");
	return ret;
}

static void emac_emb_smmu_exit(void)
{
	if (emac_emb_smmu_ctx.valid) {
		if (emac_emb_smmu_ctx.smmu_pdev)
			arm_iommu_detach_device(&emac_emb_smmu_ctx.smmu_pdev->dev);
		if (emac_emb_smmu_ctx.mapping)
			arm_iommu_release_mapping(emac_emb_smmu_ctx.mapping);
		emac_emb_smmu_ctx.valid = false;
		emac_emb_smmu_ctx.mapping = NULL;
		emac_emb_smmu_ctx.pdev_master = NULL;
		emac_emb_smmu_ctx.smmu_pdev = NULL;
		EMACDBG("Detach and release iommu mapping\n");
	}
}

static int emac_emb_smmu_cb_probe(struct platform_device *pdev)
{
	int result;
	u32 iova_ap_mapping[2];
	struct device *dev = &pdev->dev;
	int atomic_ctx = 1;
	int fast = 1;
	int bypass = 1;

	EMACDBG("EMAC EMB SMMU CB probe: smmu pdev=%p\n", pdev);

	result = of_property_read_u32_array(dev->of_node, "qcom,iova-mapping",
		iova_ap_mapping, 2);
	if (result) {
		EMACERR("Failed to read EMB start/size iova addresses\n");
		return result;
	}
	emac_emb_smmu_ctx.va_start = iova_ap_mapping[0];
	emac_emb_smmu_ctx.va_size = iova_ap_mapping[1];
	emac_emb_smmu_ctx.va_end =
		emac_emb_smmu_ctx.va_start + emac_emb_smmu_ctx.va_size;
	EMACDBG("EMB va_start=0x%x va_size=0x%x\n",
			emac_emb_smmu_ctx.va_start, emac_emb_smmu_ctx.va_size);

	emac_emb_smmu_ctx.smmu_pdev = pdev;

	if (dma_set_mask(dev, DMA_BIT_MASK(32)) ||
		dma_set_coherent_mask(dev, DMA_BIT_MASK(32))) {
		EMACERR("DMA set 32bit mask failed\n");
		return -EOPNOTSUPP;
	}

	emac_emb_smmu_ctx.mapping = arm_iommu_create_mapping(dev->bus,
			emac_emb_smmu_ctx.va_start,emac_emb_smmu_ctx.va_size);
	if (IS_ERR_OR_NULL(emac_emb_smmu_ctx.mapping)) {
		EMACDBG("Fail to create mapping\n");
		/* assume this failure is because iommu driver is not ready */
		return -EPROBE_DEFER;
	}
	EMACDBG("Successfully Created SMMU mapping\n");
	emac_emb_smmu_ctx.valid = true;

	if (of_property_read_bool(dev->of_node, "qcom,smmu-s1-bypass")) {
		if (iommu_domain_set_attr(emac_emb_smmu_ctx.mapping->domain,
					DOMAIN_ATTR_S1_BYPASS,
					&bypass)) {
			EMACERR("Couldn't set SMMU S1 bypass\n");
			result = -EIO;
			goto err_smmu_probe;
		}
		EMACDBG("SMMU S1 BYPASS set\n");
	} else {
		if (iommu_domain_set_attr(emac_emb_smmu_ctx.mapping->domain,
					DOMAIN_ATTR_ATOMIC,
					&atomic_ctx)) {
			EMACERR("Couldn't set SMMU domain as atomic\n");
			result = -EIO;
			goto err_smmu_probe;
		}
		EMACDBG("SMMU atomic set\n");
		if (iommu_domain_set_attr(emac_emb_smmu_ctx.mapping->domain,
					DOMAIN_ATTR_FAST,
					&fast)) {
			EMACERR("Couldn't set FAST SMMU\n");
			result = -EIO;
			goto err_smmu_probe;
		}
		EMACDBG("SMMU fast map set\n");
	}

	result = arm_iommu_attach_device(&emac_emb_smmu_ctx.smmu_pdev->dev,
						emac_emb_smmu_ctx.mapping);
	if (result) {
		EMACERR("couldn't attach to IOMMU ret=%d\n", result);
		goto err_smmu_probe;
	}

	EMACDBG("Successfully attached to IOMMU\n");
	if (emac_emb_smmu_ctx.pdev_master) {
		result = DWC_ETH_QOS_configure_netdevice(emac_emb_smmu_ctx.pdev_master);
		if (result)
			emac_emb_smmu_exit();
		goto smmu_probe_done;
	}

err_smmu_probe:
	if (emac_emb_smmu_ctx.mapping)
		arm_iommu_release_mapping(emac_emb_smmu_ctx.mapping);
	emac_emb_smmu_ctx.valid = false;

smmu_probe_done:
	emac_emb_smmu_ctx.ret = result;
	return result;
}

/*!
 * \brief API to initialize the device.
 *
 * \details This probing function gets called (during execution of
 * platform_register_driver() for already existing devices or later if a
 * new device gets inserted) for all PCI devices which match the ID table
 * and are not "owned" by the other drivers yet. This function gets passed
 * a "struct platform_devic *" for each device whose entry in the ID table
 * matches the device. The probe function returns zero when the driver
 * chooses to take "ownership" of the device or an error code
 * (negative number) otherwise.
 * The probe function always gets called from process context, so it can sleep.
 *
 * \param[in] pdev - pointer to platform_dev structure.
 * \param[in] id   - pointer to table of device ID/ID's the driver is inerested.
 *
 * \return integer
 *
 * \retval 0 on success & -ve number on failure.
 */
static int DWC_ETH_QOS_probe(struct platform_device *pdev)
{
	int ret = 0;

	EMACDBG("--> DWC_ETH_QOS_probe\n");

	if (of_device_is_compatible(pdev->dev.of_node, "qcom,emac-smmu-embedded"))
		return emac_emb_smmu_cb_probe(pdev);

	ret = DWC_ETH_QOS_get_dts_config(pdev);
	if (ret)
		goto err_out_map_failed;

	if (dwc_eth_qos_res_data.is_pinctrl_names)
		DWC_ETH_QOS_configure_gpio_pins(pdev);

	ret = DWC_ETH_QOS_ioremap();
	if (ret)
		goto err_out_map_failed;

	ret = DWC_ETH_QOS_init_regulators(&pdev->dev);
	if (ret)
		goto err_out_power_failed;

	ret = DWC_ETH_QOS_init_gpios(&pdev->dev);
	if (ret)
		goto err_out_gpio_failed;

	ret = DWC_ETH_QOS_get_clks(&pdev->dev);
	if (ret)
		goto err_get_clk_failed;

	if (of_property_read_bool(pdev->dev.of_node, "qcom,arm-smmu")) {
		emac_emb_smmu_ctx.pdev_master = pdev;
		EMACDBG("<-- DWC_ETH_QOS_probe SMMU enabled\n");
		ret = of_platform_populate(pdev->dev.of_node,
					DWC_ETH_QOS_plat_drv_match, NULL, &pdev->dev);
		if (ret) {
			EMACERR("Failed to populate EMAC platform\n");
			goto err_out_dev_failed;
		}
		if (emac_emb_smmu_ctx.ret) {
			EMACERR("smmu probe failed: %d\n", emac_emb_smmu_ctx.ret);
			of_platform_depopulate(&pdev->dev);
			ret = emac_emb_smmu_ctx.ret;
			emac_emb_smmu_ctx.ret = 0;
			goto err_out_dev_failed;
		}
	} else {
		ret = DWC_ETH_QOS_configure_netdevice(pdev);
		if (ret)
			goto err_out_dev_failed;
	}
	EMACDBG("<-- DWC_ETH_QOS_probe\n");
	return ret;

 err_out_dev_failed:
	DWC_ETH_QOS_disable_clks();

 err_get_clk_failed:
	DWC_ETH_QOS_free_gpios();

 err_out_gpio_failed:
	DWC_ETH_QOS_disable_regulators();

 err_out_power_failed:
	iounmap((void __iomem *)dwc_eth_qos_base_addr);
	iounmap((void __iomem *)dwc_rgmii_io_csr_base_addr);

 err_out_map_failed:
	EMACERR("<-- DWC_ETH_QOS_probe\n");
	return ret;
}

/*!
 * \brief API to release all the resources from the driver.
 *
 * \details The remove function gets called whenever a device being handled
 * by this driver is removed (either during deregistration of the driver or
 * when it is manually pulled out of a hot-pluggable slot). This function
 * should reverse operations performed at probe time. The remove function
 * always gets called from process context, so it can sleep.
 *
 * \param[in] pdev - pointer to platform_device structure.
 *
 * \return void
 */

int DWC_ETH_QOS_remove(struct platform_device *pdev)
{
	struct net_device *dev;
	struct DWC_ETH_QOS_prv_data *pdata;
	struct desc_if_struct *desc_if;
	int qinx;

	EMACDBG("--> DWC_ETH_QOS_remove\n");

	if (!pdev) {
		EMACERR("<-- DWC_ETH_QOS_remove\n");
		return -ENODEV;
	}

	if (of_device_is_compatible(pdev->dev.of_node, "qcom,emac-smmu-embedded")) {
		EMACDBG("<-- DWC_ETH_QOS_remove\n");
		return 0;
	}

	dev = platform_get_drvdata(pdev);
	if (!dev) {
		EMACERR("<-- DWC_ETH_QOS_remove\n");
		return -ENODEV;
	}
	pdata = netdev_priv(dev);
	if (!pdata) {
		EMACERR("<-- DWC_ETH_QOS_remove\n");
		return -1;
	}
	desc_if = &pdata->desc_if;
	if (!desc_if) {
		EMACERR("<-- DWC_ETH_QOS_remove\n");
		return -1;
	}
#ifdef PER_CH_INT
	DWC_ETH_QOS_deregister_per_ch_intr(pdata);
#endif
	if (pdata->irq_number != 0) {
		free_irq(pdata->irq_number, pdata);
		pdata->irq_number = 0;
	}

	if (pdata->phy_irq != 0) {
		free_irq(pdata->phy_irq, pdata);
		pdata->phy_irq = 0;
	}

	if (pdata->hw_feat.sma_sel == 1)
		DWC_ETH_QOS_mdio_unregister(dev);

#ifdef DWC_ETH_QOS_CONFIG_PTP
	DWC_ETH_QOS_ptp_remove(pdata);
#endif /* end of DWC_ETH_QOS_CONFIG_PTP */

	unregister_netdev(dev);

	/* If NAPI is enabled, delete any references to NAPI struct. */
	for (qinx = 0; qinx < DWC_ETH_QOS_RX_QUEUE_CNT; qinx++) {
		if (pdata->ipa_enabled && (qinx == IPA_DMA_RX_CH))
			continue;
		netif_napi_del(&pdata->rx_queue[qinx].napi);
	}

#ifdef DWC_ETH_QOS_CONFIG_PGTEST
	DWC_ETH_QOS_free_pg(pdata);
#endif /* end of DWC_ETH_QOS_CONFIG_PGTEST */

	desc_if->free_queue_struct(pdata);

	gDWC_ETH_QOS_prv_data = NULL;
	atomic_notifier_chain_unregister(&panic_notifier_list,
			&DWC_ETH_QOS_panic_blk);

	emac_emb_smmu_exit();

	free_netdev(dev);

	platform_set_drvdata(pdev, NULL);
	DWC_ETH_QOS_disable_clks();

	DWC_ETH_QOS_disable_regulators();
	DWC_ETH_QOS_free_gpios();

	EMACDBG("<-- DWC_ETH_QOS_remove\n");

	return 0;
}

static void DWC_ETH_QOS_shutdown(struct platform_device *pdev)
{
	pr_info("qcom-emac-dwc-eqos: DWC_ETH_QOS_shutdown\n");
}

#ifdef CONFIG_PM

/*!
 * \brief Routine to put the device in suspend mode
 *
 * \details This function gets called by PCI core when the device is being
 * suspended. The suspended state is passed as input argument to it.
 * Following operations are performed in this function,
 * - stop the phy.
 * - detach the device from stack.
 * - stop the queue.
 * - Disable napi.
 * - Stop DMA TX and RX process.
 * - Enable power down mode using PMT module or disable MAC TX and RX process.
 * - Save the pci state.
 *
 * \param[in] pdev – pointer to pci device structure.
 * \param[in] state – suspend state of device.
 *
 * \return int
 *
 * \retval 0
 */

static INT DWC_ETH_QOS_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	INT ret, pmt_flags = 0;
	unsigned int rwk_filter_values[] = {
		/* for filter 0 CRC is computed on 0 - 7 bytes from offset */
		0x000000ff,

		/* for filter 1 CRC is computed on 0 - 7 bytes from offset */
		0x000000ff,

		/* for filter 2 CRC is computed on 0 - 7 bytes from offset */
		0x000000ff,

		/* for filter 3 CRC is computed on 0 - 31 bytes from offset */
		0x000000ff,

		/* filter 0, 1 independently enabled and would apply for
		 * unicast packet only filter 3, 2 combined as,
		 * "Filter-3 pattern AND NOT Filter-2 pattern"
		 */
		0x03050101,

		/* filter 3, 2, 1 and 0 offset is 50, 58, 66, 74 bytes
		 * from start
		 */
		0x4a423a32,

		/* pattern for filter 1 and 0, "0x55", "11", repeated 8 times */
		0xe7b77eed,

		/* pattern for filter 3 and 4, "0x44", "33", repeated 8 times */
		0x9b8a5506,
	};

	EMACDBG("-->DWC_ETH_QOS_suspend\n");

	if (of_device_is_compatible(pdev->dev.of_node, "qcom,emac-smmu-embedded")) {
		EMACDBG("<--DWC_ETH_QOS_suspend smmu return\n");
		return 0;
	}

	if (!dev || !netif_running(dev)) {
		return -EINVAL;
	}

	if (pdata->hw_feat.rwk_sel && (pdata->wolopts & WAKE_UCAST)) {
		pmt_flags |= DWC_ETH_QOS_REMOTE_WAKEUP;
		hw_if->configure_rwk_filter(rwk_filter_values, 8);
	}

	if (pdata->hw_feat.mgk_sel && (pdata->wolopts & WAKE_MAGIC))
		pmt_flags |= DWC_ETH_QOS_MAGIC_WAKEUP;

	if (pdata->phy_intr_en && pdata->phy_irq && pdata->phy_wol_wolopts)
		pmt_flags |= DWC_ETH_QOS_PHY_INTR_WAKEUP;

	if (pdata->ipa_enabled && !pdata->prv_ipa.ipa_offload_susp)
		pmt_flags |= DWC_ETH_QOS_EMAC_INTR_WAKEUP;

	ret = DWC_ETH_QOS_powerdown(dev, pmt_flags, DWC_ETH_QOS_DRIVER_CONTEXT);

	EMACDBG("<--DWC_ETH_QOS_suspend ret = %d\n", ret);

	if (pdata->ipa_enabled)
		DWC_ETH_QOS_ipa_offload_event_handler(pdata, EV_DPM_SUSPEND);

	return ret;
}

/*!
 * \brief Routine to resume device operation
 *
 * \details This function gets called by PCI core when the device is being
 * resumed. It is always called after suspend has been called. These function
 * reverse operations performed at suspend time. Following operations are
 * performed in this function,
 * - restores the saved pci power state.
 * - Wakeup the device using PMT module if supported.
 * - Starts the phy.
 * - Enable MAC and DMA TX and RX process.
 * - Attach the device to stack.
 * - Enable napi.
 * - Starts the queue.
 *
 * \param[in] pdev – pointer to pci device structure.
 *
 * \return int
 *
 * \retval 0
 */

static INT DWC_ETH_QOS_resume(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	INT ret;

	DBGPR("-->DWC_ETH_QOS_resume\n");
	if (of_device_is_compatible(pdev->dev.of_node, "qcom,emac-smmu-embedded"))
		return 0;

	if (!dev || !netif_running(dev)) {
		DBGPR("<--DWC_ETH_QOS_dev_resume\n");
		return -EINVAL;
	}

	DWC_ETH_QOS_scale_clks(pdata, pdata->speed);

	if (pdata->ipa_enabled)
		DWC_ETH_QOS_ipa_offload_event_handler(pdata, EV_DPM_RESUME);

	ret = DWC_ETH_QOS_powerup(dev, DWC_ETH_QOS_DRIVER_CONTEXT);

	DBGPR("<--DWC_ETH_QOS_resume\n");

	return ret;
}

#endif /* CONFIG_PM */

static struct platform_driver DWC_ETH_QOS_plat_drv = {
	.probe = DWC_ETH_QOS_probe,
	.remove = DWC_ETH_QOS_remove,
	.shutdown = DWC_ETH_QOS_shutdown,
#ifdef CONFIG_PM
	.suspend = DWC_ETH_QOS_suspend,
	.resume = DWC_ETH_QOS_resume,
#endif
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = DWC_ETH_QOS_plat_drv_match,
	},
};

/*!
 * \brief API to register the driver.
 *
 * \details This is the first function called when the driver is loaded.
 * It register the driver with PCI sub-system
 *
 * \return void.
 */

static int DWC_ETH_QOS_init_module(void)
{
	INT ret = 0;

	DBGPR("-->DWC_ETH_QOS_init_module\n");

	ret = platform_driver_register(&DWC_ETH_QOS_plat_drv);
	if (ret < 0) {
		pr_err("qcom-emac-dwc-eqos: Driver registration failed");
		return ret;
	}

#ifdef DWC_ETH_QOS_CONFIG_DEBUGFS
	create_debug_files();
#endif

	DBGPR("<--DWC_ETH_QOS_init_module\n");

	return ret;
}

/*!
 * \brief API to unregister the driver.
 *
 * \details This is the first function called when the driver is removed.
 * It unregister the driver from PCI sub-system
 *
 * \return void.
 */

static void __exit DWC_ETH_QOS_exit_module(void)
{
	DBGPR("-->DWC_ETH_QOS_exit_module\n");

#ifdef DWC_ETH_QOS_CONFIG_DEBUGFS
	remove_debug_files();
#endif

	platform_driver_unregister(&DWC_ETH_QOS_plat_drv);

	DBGPR("<--DWC_ETH_QOS_exit_module\n");
}

/*!
 * \brief Macro to register the driver registration function.
 *
 * \details A module always begin with either the init_module or the function
 * you specify with module_init call. This is the entry function for modules;
 * it tells the kernel what functionality the module provides and sets up the
 * kernel to run the module's functions when they're needed. Once it does this,
 * entry function returns and the module does nothing until the kernel wants
 * to do something with the code that the module provides.
 */
module_init(DWC_ETH_QOS_init_module);

/*!
 * \brief Macro to register the driver un-registration function.
 *
 * \details All modules end by calling either cleanup_module or the function
 * you specify with the module_exit call. This is the exit function for modules;
 * it undoes whatever entry function did. It unregisters the functionality
 * that the entry function registered.
 */
module_exit(DWC_ETH_QOS_exit_module);

/*!
 * \brief Macro to declare the module author.
 *
 * \details This macro is used to declare the module's authore.
 */
MODULE_AUTHOR("Synopsys India Pvt Ltd");

/*!
 * \brief Macro to describe what the module does.
 *
 * \details This macro is used to describe what the module does.
 */
MODULE_DESCRIPTION("DWC_ETH_QOS Driver");

/*!
 * \brief Macro to describe the module license.
 *
 * \details This macro is used to describe the module license.
 */
MODULE_LICENSE("GPL");
