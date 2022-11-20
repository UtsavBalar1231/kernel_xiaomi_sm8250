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
/*!@file: DWC_ETH_QOS.c
 * @brief: Driver functions.
 */
#include "DWC_ETH_QOS_yheader.h"
#include "DWC_ETH_QOS_ipa.h"

void *ipc_emac_log_ctxt;

static UCHAR dev_addr[6] = {0, 0x55, 0x7b, 0xb5, 0x7d, 0xf7};
struct DWC_ETH_QOS_res_data dwc_eth_qos_res_data = {0, };
static struct msm_bus_scale_pdata *emac_bus_scale_vec = NULL;

ULONG dwc_eth_qos_base_addr;
ULONG dwc_rgmii_io_csr_base_addr;
struct DWC_ETH_QOS_prv_data *gDWC_ETH_QOS_prv_data;
struct emac_emb_smmu_cb_ctx emac_emb_smmu_ctx = {0};

#define INVALID_MODULE_PARAM_VAL 0xFFFFFFFF
static struct qmp_pkt pkt;
static char qmp_buf[MAX_QMP_MSG_SIZE + 1] = {0};
extern int create_pps_interrupt_info_device_node(dev_t *pps_dev_t,
	struct cdev** pps_cdev, struct class** pps_class,
	char *pps_dev_node_name);
extern int remove_pps_interrupt_info_device_node(struct DWC_ETH_QOS_prv_data *pdata);

int ipa_offload_en = 1;
module_param(ipa_offload_en, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(ipa_offload_en,
		 "Enable IPA offload [0-DISABLE, 1-ENABLE]");

static char *phy_intf = "";
module_param(phy_intf, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(phy_intf, "phy interface [rgmii, rmii, mii]");

static uint phy_intf_bypass_mode = INVALID_MODULE_PARAM_VAL;
module_param(phy_intf_bypass_mode, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(phy_intf_bypass_mode,
		 "Phy interface bypass mode [1-Non-ID, 0-ID]");

int phy_interrupt_en = 1;
module_param(phy_interrupt_en, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(phy_interrupt_en,
		"Enable PHY interrupt [0-DISABLE, 1-ENABLE]");

struct ip_params pparams = {};
#ifdef DWC_ETH_QOS_BUILTIN
/*!
 * \brief API to extract MAC Address from given string
 *
 * \param[in] pointer to MAC Address string
 *
 * \return None
 */
void DWC_ETH_QOS_extract_macid(char *mac_addr)
{
	char *input = NULL;
	int i = 0;
	UCHAR mac_id = 0;

	if (!mac_addr)
		return;

	/* Extract MAC ID byte by byte */
	input = strsep(&mac_addr, ":");
	while(input != NULL && i < DWC_ETH_QOS_MAC_ADDR_LEN) {
		sscanf(input, "%x", &mac_id);
		pparams.mac_addr[i++] = mac_id;
		input = strsep(&mac_addr, ":");
	}
	if (!is_valid_ether_addr(pparams.mac_addr)) {
		EMACERR("Invalid Mac address programmed: %s\n", mac_addr);
		return;
	} else
		pparams.is_valid_mac_addr = true;

	return;
}

static int __init set_early_ethernet_ipv4(char *ipv4_addr_in)
{
	int ret = 1;
	pparams.is_valid_ipv4_addr = false;

	if (!ipv4_addr_in)
		return ret;

	strlcpy(pparams.ipv4_addr_str, ipv4_addr_in, sizeof(pparams.ipv4_addr_str));
	EMACDBG("Early ethernet IPv4 addr: %s\n", pparams.ipv4_addr_str);

	ret = in4_pton(pparams.ipv4_addr_str, -1,
				(u8*)&pparams.ipv4_addr.s_addr, -1, NULL);
	if (ret != 1 || pparams.ipv4_addr.s_addr == 0) {
		EMACERR("Invalid ipv4 address programmed: %s\n", ipv4_addr_in);
		return ret;
	}

	pparams.is_valid_ipv4_addr = true;
	return ret;
}
__setup("eipv4=", set_early_ethernet_ipv4);

static int __init set_early_ethernet_ipv6(char* ipv6_addr_in)
{
	int ret = 1;
	pparams.is_valid_ipv6_addr = false;

	if (!ipv6_addr_in)
		return ret;

	strlcpy(pparams.ipv6_addr_str, ipv6_addr_in, sizeof(pparams.ipv6_addr_str));
	EMACDBG("Early ethernet IPv6 addr: %s\n", pparams.ipv6_addr_str);

	ret = in6_pton(pparams.ipv6_addr_str, -1,
				   (u8 *)&pparams.ipv6_addr.ifr6_addr.s6_addr32, -1, NULL);
	if (ret != 1 || pparams.ipv6_addr.ifr6_addr.s6_addr32 == 0)  {
		EMACERR("Invalid ipv6 address programmed: %s\n", ipv6_addr_in);
		return ret;
	}

	pparams.is_valid_ipv6_addr = true;
	return ret;
}
__setup("eipv6=", set_early_ethernet_ipv6);

static int __init set_early_ethernet_mac(char* mac_addr)
{
	int ret = 1;
	char temp_mac_addr[DWC_ETH_QOS_MAC_ADDR_STR_LEN];
	pparams.is_valid_mac_addr = false;

	if(!mac_addr)
		return ret;

	strlcpy(temp_mac_addr, mac_addr, sizeof(temp_mac_addr));
	EMACDBG("Early ethernet MAC address assigned: %s\n", temp_mac_addr);
	temp_mac_addr[DWC_ETH_QOS_MAC_ADDR_STR_LEN-1] = '\0';

	DWC_ETH_QOS_extract_macid(temp_mac_addr);
	return ret;
}
__setup("ermac=", set_early_ethernet_mac);
#endif

static ssize_t read_phy_reg_dump(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct DWC_ETH_QOS_prv_data *pdata = file->private_data;
	unsigned int len = 0, buf_len = 2000;
	char* buf;
	ssize_t ret_cnt;
	int phydata = 0;
	int i = 0;

	if (!pdata || !pdata->phydev) {
		EMACERR(" %s NULL Pointer \n",__func__);
		return -EINVAL;
	}

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, buf_len - len,
					 "\n************* PHY Reg dump *************\n");

	for (i = 0; i < 32; i++) {
		DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, i, &phydata);
		len += scnprintf(buf + len, buf_len - len,
					 "MII Register (%#x) = %#x\n",
					 i, phydata);
	}

	if ((pdata->phydev->phy_id & pdata->phydev->drv->phy_id_mask) == MICREL_PHY_ID) {
		int i = 0;
		u16 mmd_phydata = 0;
		for(i=0;i<=8;i++){
			DWC_ETH_QOS_mdio_mmd_register_read_direct(pdata, pdata->phyaddr,
				DWC_ETH_QOS_MICREL_PHY_DEBUG_MMD_DEV_ADDR, i, &mmd_phydata);
			EMACDBG("Read %#x from offset %#x", mmd_phydata, i);
			len += scnprintf(buf + len, buf_len - len,
				"Micrel PHY MMD Register (%#x) = %#x\n", i, mmd_phydata);
		}
	}

	if (len > buf_len) {
		EMACERR(" %s (len > buf_len) buffer not sufficient\n",__func__);
		len = buf_len;
	}

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret_cnt;
}

static const struct file_operations fops_phy_reg_dump = {
	.read = read_phy_reg_dump,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

int DWC_ETH_QOS_create_debugfs(struct DWC_ETH_QOS_prv_data *pdata)
{
	static struct dentry *phy_reg_dump = NULL;

	if(!pdata) {
		EMACERR( "Null Param %s \n", __func__);
		return -1;
	}

	pdata->debugfs_dir = debugfs_create_dir("eth", NULL);

	if (!pdata->debugfs_dir) {
		EMACERR( "Cannot create debugfs dir %d \n", (int)pdata->debugfs_dir);
		return -ENOMEM;
	}

	phy_reg_dump = debugfs_create_file("phy_reg_dump", S_IRUSR, pdata->debugfs_dir,
				pdata, &fops_phy_reg_dump);
	if (!phy_reg_dump || IS_ERR(phy_reg_dump)) {
		EMACERR( "Cannot create debugfs phy_reg_dump %d \n", (int)phy_reg_dump);
		goto fail;
	}

	return 0;

fail:
	debugfs_remove_recursive(pdata->debugfs_dir);
	return -ENOMEM;
}

int DWC_ETH_QOS_cleanup_debugfs(struct DWC_ETH_QOS_prv_data *pdata)
{
	if(!pdata) {
		EMACERR("Null Param %s \n", __func__);
		return -1;
	}

	if (pdata->debugfs_dir) {
		debugfs_remove_recursive(pdata->debugfs_dir);
		pdata->debugfs_dir = NULL;
	}

	EMACDBG("EMAC debugfs Deleted Successfully \n");
	return 0;
}

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
	const char *io_macro_phy_intf = phy_intf;
	struct device_node *dev_node = NULL;
	dwc_eth_qos_res_data.io_macro_tx_mode_non_id = phy_intf_bypass_mode;

	dev_node = of_find_node_by_name(pdev->dev.of_node, "io-macro-info");
	if (dev_node == NULL) {
		EMACERR("Failed to find io-macro-info node from device tree\n");
		ret = -EIO;
		goto err_out;
	}

	if (phy_intf_bypass_mode != 0 && phy_intf_bypass_mode != 1) {
		ret = of_property_read_u32(
			dev_node, "io-macro-bypass-mode",
			&dwc_eth_qos_res_data.io_macro_tx_mode_non_id);
		if (ret < 0) {
			EMACERR("Unable to read bypass mode value from device tree\n");
			goto err_out;
		}
	}
	EMACDBG("io-macro-bypass-mode = %d\n", dwc_eth_qos_res_data.io_macro_tx_mode_non_id);

	if (strlen(io_macro_phy_intf) == 0) {
		ret = of_property_read_string(dev_node, "io-interface", &io_macro_phy_intf);
		if (ret < 0) {
			EMACERR("Failed to read io mode cfg from device tree\n");
			goto err_out;
		}
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

	EMACDBG("Received IRQ number:%d\n",dwc_eth_qos_res_data.phy_intr);

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
	struct pinctrl_state *emac_phy_reset_state;
	struct pinctrl_state *emac_phy_intr_state;
	struct pinctrl_state *emac_pps_0;

	int ret = 0;

	EMACDBG("Enter\n");

	if (dwc_eth_qos_res_data.is_pinctrl_names) {

		pinctrl = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR_OR_NULL(pinctrl)) {
			ret = PTR_ERR(pinctrl);
			EMACERR("Failed to get pinctrl, err = %d\n", ret);
			return;
		}
		EMACDBG("get pinctrl succeed\n");
		dwc_eth_qos_res_data.pinctrl = pinctrl;

		if (dwc_eth_qos_res_data.emac_hw_version_type == EMAC_HW_v2_2_0 ||
			dwc_eth_qos_res_data.emac_hw_version_type == EMAC_HW_v2_1_2 ||
			dwc_eth_qos_res_data.emac_hw_version_type == EMAC_HW_v2_3_1) {
			/* PPS0 pin */
			emac_pps_0 = pinctrl_lookup_state(pinctrl, EMAC_PIN_PPS0);
			if (IS_ERR_OR_NULL(emac_pps_0)) {
				ret = PTR_ERR(emac_pps_0);
				EMACERR("Failed to get emac_pps_0, err = %d\n", ret);
				return;
			}
			EMACDBG("Get emac_pps_0 succeed\n");
			ret = pinctrl_select_state(pinctrl, emac_pps_0);
			if (ret) EMACERR("Unable to set emac_pps_0 state, err = %d\n", ret);
			else EMACDBG("Set emac_pps_0 succeed\n");

			if (dwc_eth_qos_res_data.emac_hw_version_type == EMAC_HW_v2_2_0) {
				return;
			}
		}

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

		dwc_eth_qos_res_data.rgmii_rxc_suspend_state =
			pinctrl_lookup_state(pinctrl, EMAC_RGMII_RXC_SUSPEND);
		if (IS_ERR_OR_NULL(dwc_eth_qos_res_data.rgmii_rxc_suspend_state)) {
			ret = PTR_ERR(dwc_eth_qos_res_data.rgmii_rxc_suspend_state);
			EMACERR("Failed to get rgmii_rxc_suspend_state, err = %d\n", ret);
			dwc_eth_qos_res_data.rgmii_rxc_suspend_state = NULL;
		}
		else {
			EMACDBG("Get rgmii_rxc_suspend_state succeed\n");
		}

		dwc_eth_qos_res_data.rgmii_rxc_resume_state =
			pinctrl_lookup_state(pinctrl, EMAC_RGMII_RXC_RESUME);
		if (IS_ERR_OR_NULL(dwc_eth_qos_res_data.rgmii_rxc_resume_state)) {
			ret = PTR_ERR(dwc_eth_qos_res_data.rgmii_rxc_resume_state);
			EMACERR("Failed to get rgmii_rxc_resume_state, err = %d\n", ret);
			dwc_eth_qos_res_data.rgmii_rxc_resume_state = NULL;
		}
		else {
			EMACDBG("Get rgmii_rxc_resume_state succeed\n");
		}

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

		emac_phy_intr_state = pinctrl_lookup_state(pinctrl, EMAC_PHY_INTR);
		if (IS_ERR_OR_NULL(emac_phy_intr_state)) {
			ret = PTR_ERR(emac_phy_intr_state);
			EMACERR("Failed to get emac_phy_intr_state, err = %d\n", ret);
			return;
		}
		EMACDBG("Get emac_phy_intr_state succeed\n");
		ret = pinctrl_select_state(pinctrl, emac_phy_intr_state);
		if (ret)
			EMACERR("Unable to set emac_phy_intr_state state, err = %d\n", ret);
		else
			EMACDBG("Set emac_phy_intr_state succeed\n");

		emac_phy_reset_state = pinctrl_lookup_state(pinctrl, EMAC_PHY_RESET);
		if (IS_ERR_OR_NULL(emac_phy_reset_state)) {
			ret = PTR_ERR(emac_phy_reset_state);
			EMACERR("Failed to get emac_phy_reset_state, err = %d\n", ret);
			return;
		}
		EMACDBG("Get emac_phy_reset_state succeed\n");
		ret = pinctrl_select_state(pinctrl, emac_phy_reset_state);
		if (ret)
			EMACERR("Unable to set emac_phy_reset_state state, err = %d\n", ret);
		else
			EMACDBG("Set emac_phy_reset_state succeed\n");
	}

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

	if (dwc_eth_qos_res_data.emac_hw_version_type == EMAC_HW_v2_3_1 ||
		dwc_eth_qos_res_data.emac_hw_version_type == EMAC_HW_v2_1_2)
		dwc_eth_qos_res_data.pps_lpass_conn_en = true;

	if (dwc_eth_qos_res_data.emac_hw_version_type == EMAC_HW_v2_3_1) {

		resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"ptp_pps_irq_0");
		if (!resource) {
			EMACERR("get ptp_pps_irq_0 resource failed\n");
			ret = -ENODEV;
			goto err_out;
		}
		dwc_eth_qos_res_data.ptp_pps_avb_class_a_irq = resource->start;
		EMACDBG("ptp_pps_avb_class_a_irq = %d\n",
			dwc_eth_qos_res_data.ptp_pps_avb_class_a_irq);

		resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
				"ptp_pps_irq_1");
		if (!resource) {
			EMACERR("get ptp_pps_irq_1 resource failed\n");
			ret = -ENODEV;
			goto err_out;
		}
		dwc_eth_qos_res_data.ptp_pps_avb_class_b_irq = resource->start;
		EMACDBG("ptp_pps_avb_class_b_irq = %d\n", dwc_eth_qos_res_data.ptp_pps_avb_class_b_irq);

	}

	dwc_eth_qos_res_data.early_eth_en = 0;
	if(pparams.is_valid_mac_addr &&
	   (pparams.is_valid_ipv4_addr || pparams.is_valid_ipv6_addr)) {
		/* For 1000BASE-T mode, auto-negotiation is required and
			always used to establish a link.
			Configure phy and MAC in 100Mbps mode with autoneg disable
			as link up takes more time with autoneg enabled  */
		dwc_eth_qos_res_data.early_eth_en = 1;
		EMACINFO("Early ethernet is enabled\n");
	}

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
	dwc_eth_qos_res_data.phy_addr = -1;
	if (of_property_read_bool(pdev->dev.of_node, "emac-phy-addr")) {
		ret = of_property_read_u32(pdev->dev.of_node, "emac-phy-addr",
			&dwc_eth_qos_res_data.phy_addr);
		if (ret) {
			EMACINFO("Pphy_addr not specified, using dynamic phy detection\n");
			dwc_eth_qos_res_data.phy_addr = -1;
		}
		EMACINFO("phy_addr = %d\n", dwc_eth_qos_res_data.phy_addr);
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

int DWC_ETH_QOS_qmp_mailbox_init(struct DWC_ETH_QOS_prv_data *pdata)
{
	pdata->qmp_mbox_client = devm_kzalloc(
	   &pdata->pdev->dev, sizeof(*pdata->qmp_mbox_client), GFP_KERNEL);

	if (pdata->qmp_mbox_client == NULL || IS_ERR(pdata->qmp_mbox_client)){
		EMACERR("qmp alloc client failed\n");
		return -1;
	}

	pdata->qmp_mbox_client->dev = &pdata->pdev->dev;
	pdata->qmp_mbox_client->tx_block = true;
	pdata->qmp_mbox_client->tx_tout = 1000;
	pdata->qmp_mbox_client->knows_txdone = false;

	pdata->qmp_mbox_chan = mbox_request_channel(pdata->qmp_mbox_client, 0);

	if (IS_ERR(pdata->qmp_mbox_chan)) {
		EMACERR("qmp reuest channel failed\n");
		return -1;
	}

	return 0;
}

int DWC_ETH_QOS_qmp_mailbox_send_message(struct DWC_ETH_QOS_prv_data *pdata)
{
	int ret = 0;

	memset(&qmp_buf[0], 0, MAX_QMP_MSG_SIZE + 1);

	snprintf(qmp_buf, MAX_QMP_MSG_SIZE, "{class:ctile, pc:0}");

	pkt.size = ((size_t)strlen(qmp_buf) + 0x3) & ~0x3;
	pkt.data = qmp_buf;

	ret = mbox_send_message(pdata->qmp_mbox_chan, (void*)&pkt);

	EMACDBG("qmp mbox_send_message ret = %d \n", ret);

	if (ret < 0) {
		EMACERR("Disabling c-tile power collapse failed\n");
		return ret;
	}

	EMACINFO("Disabling c-tile power collapse succeded");

	return 0;
}

/**
 *  DWC_ETH_QOS_qmp_mailbox_work - Scheduled from probe
 *  @work: work_struct
 */
void DWC_ETH_QOS_qmp_mailbox_work(struct work_struct *work)
{
	struct DWC_ETH_QOS_prv_data *pdata =
		container_of(work, struct DWC_ETH_QOS_prv_data, qmp_mailbox_work);

	EMACDBG("Enter\n");

	/* Send QMP message to disable c-tile power collapse */
	DWC_ETH_QOS_qmp_mailbox_send_message(pdata);

	EMACDBG("Exit\n");
}


int DWC_ETH_QOS_enable_ptp_clk(struct device *dev)
{
	int ret;
	const char* ptp_clock_name;

	if (dwc_eth_qos_res_data.emac_hw_version_type == EMAC_HW_v2_1_0
	    || dwc_eth_qos_res_data.emac_hw_version_type == EMAC_HW_v2_1_2
	    || dwc_eth_qos_res_data.emac_hw_version_type == EMAC_HW_v2_1_1)
		ptp_clock_name = "emac_ptp_clk";
	else
		ptp_clock_name = "eth_ptp_clk";

	/* valid value of dwc_eth_qos_res_data.ptp_clk indicates that clock is enabled */
	if (!dwc_eth_qos_res_data.ptp_clk) {

		dwc_eth_qos_res_data.ptp_clk = devm_clk_get(dev, ptp_clock_name);

		if (IS_ERR(dwc_eth_qos_res_data.ptp_clk)) {
			dwc_eth_qos_res_data.ptp_clk = NULL;
			if (dwc_eth_qos_res_data.ptp_clk != ERR_PTR(-EPROBE_DEFER)) {
				EMACERR("unable to get %s\n", ptp_clock_name);
				return -EIO;
			}
		}

		ret = clk_prepare_enable(dwc_eth_qos_res_data.ptp_clk);

		if (ret) {
			EMACERR("Failed to enable %s\n", ptp_clock_name);
			goto ptp_clk_fail;
		}

		ret = clk_set_rate(dwc_eth_qos_res_data.ptp_clk, DWC_ETH_QOS_SYSCLOCK);

		if (ret) {
			EMACERR("Failed to set rate for %s\n", ptp_clock_name);
			goto ptp_clk_fail;
		}
	}

	return 0;

ptp_clk_fail:

	DWC_ETH_QOS_disable_ptp_clk(dev);
	return ret;
}

void DWC_ETH_QOS_disable_ptp_clk(struct device* dev)
{
	if (dwc_eth_qos_res_data.ptp_clk){
		clk_set_rate(dwc_eth_qos_res_data.ptp_clk, 0);
		clk_disable_unprepare(dwc_eth_qos_res_data.ptp_clk);
		devm_clk_put(dev, dwc_eth_qos_res_data.ptp_clk);
	}

	dwc_eth_qos_res_data.ptp_clk = NULL;
}

void DWC_ETH_QOS_resume_clks(struct DWC_ETH_QOS_prv_data *pdata)
{
	EMACDBG("Enter\n");

	if (dwc_eth_qos_res_data.axi_clk)
		clk_prepare_enable(dwc_eth_qos_res_data.axi_clk);

	if (dwc_eth_qos_res_data.ahb_clk)
		clk_prepare_enable(dwc_eth_qos_res_data.ahb_clk);

	if (dwc_eth_qos_res_data.rgmii_clk)
		clk_prepare_enable(dwc_eth_qos_res_data.rgmii_clk);

	if (DWC_ETH_QOS_is_phy_link_up(pdata))
		DWC_ETH_QOS_set_clk_and_bus_config(pdata, pdata->speed);
	else
		DWC_ETH_QOS_set_clk_and_bus_config(pdata, SPEED_10);

#ifdef DWC_ETH_QOS_CONFIG_PTP
	if (dwc_eth_qos_res_data.ptp_clk)
		clk_prepare_enable(dwc_eth_qos_res_data.ptp_clk);
#endif

	pdata->clks_suspended = 0;

	if (pdata->phy_intr_en)
		complete_all(&pdata->clk_enable_done);

	EMACDBG("Exit\n");
}

void DWC_ETH_QOS_suspend_clks(struct DWC_ETH_QOS_prv_data *pdata)
{
	EMACDBG("Enter\n");

	if (pdata->phy_intr_en)
		reinit_completion(&pdata->clk_enable_done);

	pdata->clks_suspended = 1;

	DWC_ETH_QOS_set_clk_and_bus_config(pdata, 0);

	if (dwc_eth_qos_res_data.axi_clk)
		clk_disable_unprepare(dwc_eth_qos_res_data.axi_clk);

	if (dwc_eth_qos_res_data.ahb_clk)
		clk_disable_unprepare(dwc_eth_qos_res_data.ahb_clk);

	if (dwc_eth_qos_res_data.rgmii_clk)
		clk_disable_unprepare(dwc_eth_qos_res_data.rgmii_clk);

#ifdef DWC_ETH_QOS_CONFIG_PTP
	if (dwc_eth_qos_res_data.ptp_clk)
		clk_disable_unprepare(dwc_eth_qos_res_data.ptp_clk);
#endif

	EMACDBG("Exit\n");
}

void DWC_ETH_QOS_disable_clks(struct device* dev)
{
	if (dwc_eth_qos_res_data.axi_clk){
		clk_disable_unprepare(dwc_eth_qos_res_data.axi_clk);
		devm_clk_put(dev, dwc_eth_qos_res_data.axi_clk);
	}

	dwc_eth_qos_res_data.axi_clk = NULL;

	if (dwc_eth_qos_res_data.ahb_clk){
		clk_disable_unprepare(dwc_eth_qos_res_data.ahb_clk);
		devm_clk_put(dev, dwc_eth_qos_res_data.ahb_clk);
	}

	dwc_eth_qos_res_data.ahb_clk = NULL;

	if (dwc_eth_qos_res_data.rgmii_clk){
		clk_disable_unprepare(dwc_eth_qos_res_data.rgmii_clk);
		devm_clk_put(dev, dwc_eth_qos_res_data.rgmii_clk);
	}

	dwc_eth_qos_res_data.rgmii_clk = NULL;

}

static int DWC_ETH_QOS_get_clks(struct device *dev)
{
	int ret = 0;
	const char* axi_clock_name;
	const char* ahb_clock_name;
	const char* rgmii_clock_name;

	dwc_eth_qos_res_data.axi_clk = NULL;
	dwc_eth_qos_res_data.ahb_clk = NULL;
	dwc_eth_qos_res_data.rgmii_clk = NULL;
	dwc_eth_qos_res_data.ptp_clk = NULL;

	if (dwc_eth_qos_res_data.emac_hw_version_type == EMAC_HW_v2_1_0
	    || dwc_eth_qos_res_data.emac_hw_version_type == EMAC_HW_v2_1_2
	    || dwc_eth_qos_res_data.emac_hw_version_type == EMAC_HW_v2_1_1) {
		/* EMAC core version 2.1.0 clocks */
		axi_clock_name = "emac_axi_clk";
		ahb_clock_name = "emac_slv_ahb_clk";
		rgmii_clock_name = "emac_rgmii_clk";
	} else {
		/* Default values are for EMAC core version 2.0.0 clocks */
		axi_clock_name = "eth_axi_clk";
		ahb_clock_name = "eth_slave_ahb_clk";
		rgmii_clock_name = "eth_rgmii_clk";
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

	return ret;

fail_clk:
	DWC_ETH_QOS_disable_clks(dev);
	return ret;
}

static int DWC_ETH_QOS_panic_notifier(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	if (gDWC_ETH_QOS_prv_data) {
		EMACINFO("gDWC_ETH_QOS_prv_data 0x%p\n", gDWC_ETH_QOS_prv_data);
		DWC_ETH_QOS_ipa_stats_read(gDWC_ETH_QOS_prv_data);
		DWC_ETH_QOS_dma_desc_stats_read(gDWC_ETH_QOS_prv_data);

		gDWC_ETH_QOS_prv_data->iommu_domain = emac_emb_smmu_ctx.iommu_domain;
		EMACINFO("emac iommu domain 0x%p\n", gDWC_ETH_QOS_prv_data->iommu_domain);

		gDWC_ETH_QOS_prv_data->emac_reg_base_address =
			(unsigned int *)kzalloc(dwc_eth_qos_res_data.emac_mem_size, GFP_KERNEL);
		EMACINFO("emac register mem 0x%p\n", gDWC_ETH_QOS_prv_data->emac_reg_base_address);
		if (gDWC_ETH_QOS_prv_data->emac_reg_base_address != NULL)
			memcpy(gDWC_ETH_QOS_prv_data->emac_reg_base_address, (void*)dwc_eth_qos_base_addr,
				   dwc_eth_qos_res_data.emac_mem_size);

		gDWC_ETH_QOS_prv_data->rgmii_reg_base_address =
			(unsigned int *)kzalloc(dwc_eth_qos_res_data.rgmii_mem_size, GFP_KERNEL);
		EMACINFO("rgmii register mem 0x%p\n", gDWC_ETH_QOS_prv_data->rgmii_reg_base_address);
		if (gDWC_ETH_QOS_prv_data->rgmii_reg_base_address != NULL)
			memcpy(gDWC_ETH_QOS_prv_data->rgmii_reg_base_address, (void*)dwc_rgmii_io_csr_base_addr,
				   dwc_eth_qos_res_data.rgmii_mem_size);
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

	if (dwc_eth_qos_res_data.is_gpio_phy_reset &&
		!dwc_eth_qos_res_data.early_eth_en) {
		ret = setup_gpio_output_common(
			dev, EMAC_GPIO_PHY_RESET_NAME,
			&dwc_eth_qos_res_data.gpio_phy_reset, PHY_RESET_GPIO_LOW);

		if (ret) {
			EMACERR("Failed to setup <%s> gpio\n",
					EMAC_GPIO_PHY_RESET_NAME);
			goto gpio_error;
		}
		mdelay(1);

		gpio_set_value(dwc_eth_qos_res_data.gpio_phy_reset, PHY_RESET_GPIO_HIGH);
		EMACDBG("PHY is out of reset successfully\n");
		/* Add delay of 50ms so that phy should get sufficient time*/
		mdelay(50);
	}

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

void is_ipv6_NW_stack_ready(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct DWC_ETH_QOS_prv_data *pdata;
	int ret;

	EMACDBG("\n");
	dwork = container_of(work, struct delayed_work, work);
	pdata = container_of(dwork, struct DWC_ETH_QOS_prv_data, ipv6_addr_assign_wq);

	ret = DWC_ETH_QOS_add_ipv6addr(pdata);
	if (ret)
		return;

	cancel_delayed_work_sync(&pdata->ipv6_addr_assign_wq);
	flush_delayed_work(&pdata->ipv6_addr_assign_wq);
	return;
}

int DWC_ETH_QOS_add_ipv6addr(struct DWC_ETH_QOS_prv_data *pdata)
{
	int ret;
#ifdef DWC_ETH_QOS_BUILTIN
	struct in6_ifreq ir6;
	char* prefix;
	struct ip_params *ip_info = &pparams;
	struct net *net = dev_net(pdata->dev);

	EMACDBG("\n");
	if (!net || !net->genl_sock || !net->genl_sock->sk_socket)
		EMACERR("Sock is null, unable to assign ipv6 address\n");

	if (!net->ipv6.devconf_dflt) {
		EMACDBG("ipv6.devconf_dflt is null, schedule wq\n");
		schedule_delayed_work(&pdata->ipv6_addr_assign_wq, msecs_to_jiffies(1000));
		return -EFAULT;
	}

	/*For valid IPv6 address*/
	memset(&ir6, 0, sizeof(ir6));
	memcpy(&ir6, &ip_info->ipv6_addr, sizeof(struct in6_ifreq));
	ir6.ifr6_ifindex = pdata->dev->ifindex;

	if ((prefix = strchr(ip_info->ipv6_addr_str, '/')) == NULL)
		ir6.ifr6_prefixlen = 0;
	else {
		ir6.ifr6_prefixlen = simple_strtoul(prefix + 1, NULL, 0);
		if (ir6.ifr6_prefixlen > 128)
			ir6.ifr6_prefixlen = 0;
	}

	ret = inet6_ioctl(net->genl_sock->sk_socket, SIOCSIFADDR, (unsigned long)(void *)&ir6);
	if (ret)
		EMACERR("Can't setup IPv6 address!\r\n");
	else
		EMACDBG("Assigned IPv6 address: %s\r\n", ip_info->ipv6_addr_str);
#else
	ret = -EFAULT;
#endif
	return ret;
}

int DWC_ETH_QOS_add_ipaddr(struct DWC_ETH_QOS_prv_data *pdata)
{
	int ret=0;
#ifdef DWC_ETH_QOS_BUILTIN
	struct ip_params *ip_info = &pparams;
	struct ifreq ir;
	struct sockaddr_in *sin = (void *) &ir.ifr_ifru.ifru_addr;
	struct net *net = dev_net(pdata->dev);

	if (!net || !net->genl_sock || !net->genl_sock->sk_socket)
		EMACERR("Sock is null, unable to assign ipv4 address\n");

	/*For valid Ipv4 address*/
	memset(&ir, 0, sizeof(ir));
	memcpy(&sin->sin_addr.s_addr, &ip_info->ipv4_addr,
		   sizeof(sin->sin_addr.s_addr));
	strlcpy(ir.ifr_ifrn.ifrn_name, pdata->dev->name, sizeof(ir.ifr_ifrn.ifrn_name) + 1);
	sin->sin_family = AF_INET;
	sin->sin_port = 0;

	ret = inet_ioctl(net->genl_sock->sk_socket, SIOCSIFADDR, (unsigned long)(void *)&ir);
	if (ret)
		EMACERR( "Can't setup IPv4 address!: %d\r\n", ret);
	else
		EMACDBG("Assigned IPv4 address: %s\r\n", ip_info->ipv4_addr_str);
#endif
	return ret;
}

u32 l3mdev_fib_table1 (const struct net_device *dev)
{
	return RT_TABLE_LOCAL;
}

const struct l3mdev_ops l3mdev_op1 = {.l3mdev_fib_table = l3mdev_fib_table1};

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

	if (pparams.is_valid_mac_addr == true)
		ether_addr_copy(dev_addr, pparams.mac_addr);

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

#ifdef CONFIG_NET_L3_MASTER_DEV
	if (pdata->res_data->early_eth_en && pdata->emac_hw_version_type == EMAC_HW_v2_3_1) {
		EMACDBG("l3mdev_op1 set \n");
		dev->priv_flags = IFF_L3MDEV_MASTER;
		dev->l3mdev_ops = &l3mdev_op1;
	}
#endif


	/* Scale the clocks to 10Mbps speed */
	if (pdata->res_data->early_eth_en) {
		pdata->speed = SPEED_100;
		DWC_ETH_QOS_set_clk_and_bus_config(pdata, SPEED_100);
	}
	else {
		pdata->speed = SPEED_10;
		DWC_ETH_QOS_set_clk_and_bus_config(pdata, SPEED_10);
	}

	DWC_ETH_QOS_set_rgmii_func_clk_en();

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

	EMACDBG("EMAC IPA enabled: %d\n", pdata->ipa_enabled);
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

	pdata->interface = DWC_ETH_QOS_get_io_macro_phy_interface(pdata);

	pdata->enable_phy_intr = phy_interrupt_en;

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
			  (NAPI_PER_QUEUE_POLL_BUDGET * DWC_ETH_QOS_RX_QUEUE_CNT));
	}

	dev->ethtool_ops = DWC_ETH_QOS_get_ethtool_ops();
	DWC_ETH_QOS_dma_desc_stats_init(pdata);

	if (pdata->hw_feat.tso_en) {
		dev->hw_features = NETIF_F_TSO;
		dev->hw_features |= NETIF_F_TSO6;
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
	/*default ptp clock frequency set to 50Mhz*/
	pdata->ptpclk_freq = DWC_ETH_QOS_DEFAULT_PTP_CLOCK;
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

	if (!pdata->always_on_phy)
		DWC_ETH_QOS_set_clk_and_bus_config(pdata, SPEED_10);

	if (pdata->emac_hw_version_type == EMAC_HW_v2_3_1) {
		create_pps_interrupt_info_device_node(&pdata->avb_class_a_dev_t,
			&pdata->avb_class_a_cdev, &pdata->avb_class_a_class, AVB_CLASS_A_POLL_DEV_NODE_NAME);

		create_pps_interrupt_info_device_node(&pdata->avb_class_b_dev_t,
			&pdata->avb_class_b_cdev ,&pdata->avb_class_b_class, AVB_CLASS_B_POLL_DEV_NODE_NAME);
	}

	DWC_ETH_QOS_create_debugfs(pdata);

	if (EMAC_HW_v2_0_0 == pdata->emac_hw_version_type)
		pdata->disable_ctile_pc = 1;

	if (pdata->disable_ctile_pc && !DWC_ETH_QOS_qmp_mailbox_init(pdata)){
		INIT_WORK(&pdata->qmp_mailbox_work, DWC_ETH_QOS_qmp_mailbox_work);
		queue_work(system_wq, &pdata->qmp_mailbox_work);
	}

	if (pdata->res_data->early_eth_en) {
		if (pparams.is_valid_ipv4_addr)
			ret = DWC_ETH_QOS_add_ipaddr(pdata);

		if (pparams.is_valid_ipv6_addr) {
			INIT_DELAYED_WORK(&pdata->ipv6_addr_assign_wq, is_ipv6_NW_stack_ready);
			ret = DWC_ETH_QOS_add_ipv6addr(pdata);
			if (ret)
				schedule_delayed_work(&pdata->ipv6_addr_assign_wq, msecs_to_jiffies(1000));
		}

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

	emac_emb_smmu_ctx.iommu_domain =
		iommu_get_domain_for_dev(&emac_emb_smmu_ctx.smmu_pdev->dev);

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
#ifdef CONFIG_MSM_BOOT_TIME_MARKER
	place_marker("M - Ethernet probe start");
#endif
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

#if defined DWC_ETH_QOS_BUILTIN && defined CONFIG_MSM_BOOT_TIME_MARKER
	place_marker("M - Ethernet probe end");
#endif

	return ret;

 err_out_dev_failed:
	DWC_ETH_QOS_disable_clks(&pdev->dev);

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

	DWC_ETH_QOS_cleanup_debugfs(pdata);

	if (pdata->emac_hw_version_type == EMAC_HW_v2_3_1)
		remove_pps_interrupt_info_device_node(pdata);

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

	if (dwc_eth_qos_res_data.emac_hw_version_type == EMAC_HW_v2_3_1) {
		if (dwc_eth_qos_res_data.ptp_pps_avb_class_a_irq != 0) {
			free_irq(dwc_eth_qos_res_data.ptp_pps_avb_class_a_irq, pdata);
			dwc_eth_qos_res_data.ptp_pps_avb_class_a_irq = 0;
		}
		if (dwc_eth_qos_res_data.ptp_pps_avb_class_b_irq != 0) {
			free_irq(dwc_eth_qos_res_data.ptp_pps_avb_class_b_irq, pdata);
			dwc_eth_qos_res_data.ptp_pps_avb_class_b_irq = 0;
		}
	}

	if (pdata->phy_intr_en && pdata->phy_irq)
		cancel_work_sync(&pdata->emac_phy_work);

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

	DWC_ETH_QOS_set_clk_and_bus_config(pdata, 0);

	if (pdata->bus_hdl){
		msm_bus_scale_unregister_client(pdata->bus_hdl);
		pdata->bus_hdl = 0;
	}

	free_netdev(dev);

	platform_set_drvdata(pdev, NULL);

	DWC_ETH_QOS_disable_clks(&pdev->dev);
	DWC_ETH_QOS_disable_ptp_clk(&pdev->dev);
	DWC_ETH_QOS_disable_regulators();
	DWC_ETH_QOS_free_gpios();

	EMACDBG("<-- DWC_ETH_QOS_remove\n");

	return 0;
}

static void DWC_ETH_QOS_shutdown(struct platform_device *pdev)
{
	pr_info("qcom-emac-dwc-eqos: DWC_ETH_QOS_shutdown\n");
#ifdef DWC_ETH_QOS_BUILTIN
	if (gDWC_ETH_QOS_prv_data->dev->flags & IFF_UP) {
		gDWC_ETH_QOS_prv_data->dev->netdev_ops->ndo_stop(gDWC_ETH_QOS_prv_data->dev);
		gDWC_ETH_QOS_prv_data->dev->flags &= ~IFF_UP;
	}
	DWC_ETH_QOS_remove(pdev);
#endif
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

static INT DWC_ETH_QOS_suspend(struct device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = gDWC_ETH_QOS_prv_data;
	struct net_device *net_dev = pdata->dev;
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

	if (of_device_is_compatible(dev->of_node, "qcom,emac-smmu-embedded")) {
		EMACDBG("<--DWC_ETH_QOS_suspend smmu return\n");
		return 0;
	}

	if ((pdata->ipa_enabled && pdata->prv_ipa.ipa_offload_conn)) {
		pdata->power_down_type |= DWC_ETH_QOS_EMAC_INTR_WAKEUP;
		enable_irq_wake(pdata->irq_number);

		return 0;
	}

	if (!net_dev || !netif_running(net_dev)) {
		return -EINVAL;
	}

	if (pdata->hw_feat.rwk_sel && (pdata->wolopts & WAKE_UCAST)) {
		pmt_flags |= DWC_ETH_QOS_REMOTE_WAKEUP;
		hw_if->configure_rwk_filter(rwk_filter_values, 8);
	}

	if (pdata->hw_feat.mgk_sel && (pdata->wolopts & WAKE_MAGIC))
		pmt_flags |= DWC_ETH_QOS_MAGIC_WAKEUP;

	ret = DWC_ETH_QOS_powerdown(net_dev, pmt_flags, DWC_ETH_QOS_DRIVER_CONTEXT);

	DWC_ETH_QOS_suspend_clks(pdata);

	/* Suspend the PHY RXC clock. */
	if (dwc_eth_qos_res_data.is_pinctrl_names &&
		(dwc_eth_qos_res_data.rgmii_rxc_suspend_state != NULL)) {
		/* Remove RXC clock source from Phy.*/
		ret = pinctrl_select_state(dwc_eth_qos_res_data.pinctrl,
				dwc_eth_qos_res_data.rgmii_rxc_suspend_state);
		if (ret)
			EMACERR("Unable to set rgmii_rxc_suspend_state state, err = %d\n", ret);
		else
			EMACDBG("Set rgmii_rxc_suspend_state succeed\n");
	}

	EMACDBG("<--DWC_ETH_QOS_suspend ret = %d\n", ret);
#ifdef CONFIG_MSM_BOOT_TIME_MARKER
	pdata->print_kpi = 0;
#endif
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

static INT DWC_ETH_QOS_resume(struct device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = gDWC_ETH_QOS_prv_data;
	struct net_device *net_dev = pdata->dev;
	INT ret;

	EMACDBG("-->DWC_ETH_QOS_resume\n");
	if (of_device_is_compatible(dev->of_node, "qcom,emac-smmu-embedded"))
		return 0;

	if (!net_dev || !netif_running(net_dev)) {
		EMACERR("<--DWC_ETH_QOS_dev_resume\n");
		return -EINVAL;
	}

	if (pdata->ipa_enabled && pdata->prv_ipa.ipa_offload_conn) {
		if (pdata->power_down_type & DWC_ETH_QOS_EMAC_INTR_WAKEUP) {
			disable_irq_wake(pdata->irq_number);
			pdata->power_down_type &= ~DWC_ETH_QOS_EMAC_INTR_WAKEUP;
		}

		if (pdata->power_down_type & DWC_ETH_QOS_PHY_INTR_WAKEUP) {
			disable_irq_wake(pdata->phy_irq);
			pdata->power_down_type &= ~DWC_ETH_QOS_PHY_INTR_WAKEUP;
		}

		/* Wakeup reason can be PHY link event or a RX packet */
		/* Set a wakeup event to ensure enough time for processing */
		pm_wakeup_event(dev, 5000);
		return 0;
	}

	/* Resume the PhY RXC clock. */
	if (dwc_eth_qos_res_data.is_pinctrl_names &&
		(dwc_eth_qos_res_data.rgmii_rxc_resume_state != NULL)) {

		/* Enable RXC clock source from Phy.*/
		ret = pinctrl_select_state(dwc_eth_qos_res_data.pinctrl,
				dwc_eth_qos_res_data.rgmii_rxc_resume_state);
		if (ret)
			EMACERR("Unable to set rgmii_rxc_resume_state state, err = %d\n", ret);
		else
			EMACDBG("Set rgmii_rxc_resume_state succeed\n");
	}

	DWC_ETH_QOS_resume_clks(pdata);

	ret = DWC_ETH_QOS_powerup(net_dev, DWC_ETH_QOS_DRIVER_CONTEXT);

	if (pdata->ipa_enabled)
		DWC_ETH_QOS_ipa_offload_event_handler(pdata, EV_DPM_RESUME);

	/* Wakeup reason can be PHY link event or a RX packet */
	/* Set a wakeup event to ensure enough time for processing */
	pm_wakeup_event(dev, 5000);

	EMACDBG("<--DWC_ETH_QOS_resume\n");

	return ret;
}

#endif /* CONFIG_PM */

static int DWC_ETH_QOS_hib_restore(struct device *dev) {
	struct DWC_ETH_QOS_prv_data *pdata = gDWC_ETH_QOS_prv_data;
	int ret = 0;

	if (of_device_is_compatible(dev->of_node, "qcom,emac-smmu-embedded"))
		return 0;

	EMACINFO(" start\n");

        ret = DWC_ETH_QOS_init_regulators(dev);
	if (ret)
		return ret;

	ret = DWC_ETH_QOS_init_gpios(dev);
	if (ret)
		return ret;

	ret = DWC_ETH_QOS_get_clks(dev);
	if (ret)
		return ret;

	DWC_ETH_QOS_set_clk_and_bus_config(pdata, pdata->speed);

	DWC_ETH_QOS_set_rgmii_func_clk_en();

#ifdef DWC_ETH_QOS_CONFIG_PTP
	DWC_ETH_QOS_ptp_init(pdata);
#endif /* end of DWC_ETH_QOS_CONFIG_PTP */

	/* issue software reset to device */
	pdata->hw_if.exit();

	/* Bypass PHYLIB for TBI, RTBI and SGMII interface */
	if (pdata->hw_feat.sma_sel == 1) {
		ret = DWC_ETH_QOS_mdio_register(pdata->dev);
		if (ret < 0) {
			EMACERR("MDIO bus (id %d) registration failed\n",
					  pdata->bus_id);
			return ret;
		}
	}

	if (!(pdata->dev->flags & IFF_UP)) {
		pdata->dev->netdev_ops->ndo_open(pdata->dev);
		pdata->dev->flags |= IFF_UP;
	}

	EMACINFO("end\n");

	return ret;
}

static int DWC_ETH_QOS_hib_freeze(struct device *dev) {
	struct DWC_ETH_QOS_prv_data *pdata = gDWC_ETH_QOS_prv_data;
	int ret = 0;

	if (of_device_is_compatible(dev->of_node, "qcom,emac-smmu-embedded"))
		return 0;

	EMACINFO(" start\n");
	if (pdata->dev->flags & IFF_UP) {
		pdata->dev->netdev_ops->ndo_stop(pdata->dev);
		pdata->dev->flags &= ~IFF_UP;
	}

	if (pdata->hw_feat.sma_sel == 1)
		DWC_ETH_QOS_mdio_unregister(pdata->dev);

#ifdef DWC_ETH_QOS_CONFIG_PTP
	DWC_ETH_QOS_ptp_remove(pdata);
#endif /* end of DWC_ETH_QOS_CONFIG_PTP */

	DWC_ETH_QOS_disable_clks(dev);

	DWC_ETH_QOS_disable_regulators();

	DWC_ETH_QOS_free_gpios();

	EMACINFO("end\n");

	return ret;
}

static const struct dev_pm_ops DWC_ETH_QOS_pm_ops = {
	.freeze = DWC_ETH_QOS_hib_freeze,
	.restore = DWC_ETH_QOS_hib_restore,
	.thaw = DWC_ETH_QOS_hib_restore,
#ifdef CONFIG_PM
	.suspend = DWC_ETH_QOS_suspend,
	.resume = DWC_ETH_QOS_resume,
#endif
};

static struct platform_driver DWC_ETH_QOS_plat_drv = {
	.probe = DWC_ETH_QOS_probe,
	.remove = DWC_ETH_QOS_remove,
	.shutdown = DWC_ETH_QOS_shutdown,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = DWC_ETH_QOS_plat_drv_match,
		.pm = &DWC_ETH_QOS_pm_ops,
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

	EMACDBG("-->DWC_ETH_QOS_init_module\n");

	ret = platform_driver_register(&DWC_ETH_QOS_plat_drv);
	if (ret < 0) {
		pr_err("qcom-emac-dwc-eqos: Driver registration failed");
		return ret;
	}

	ipc_emac_log_ctxt = ipc_log_context_create(IPCLOG_STATE_PAGES,"emac", 0);
	if (!ipc_emac_log_ctxt)
		EMACERR("Error creating logging context for emac\n");
	else
		EMACDBG("IPC logging has been enabled for emac\n");

#ifdef DWC_ETH_QOS_CONFIG_DEBUGFS
	create_debug_files();
#endif

	EMACDBG("<--DWC_ETH_QOS_init_module\n");

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

	if (ipc_emac_log_ctxt != NULL)
		ipc_log_context_destroy(ipc_emac_log_ctxt);

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
