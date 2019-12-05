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
 */

/*!@file: DWC_ETH_QOS_rgmii_io_macro.c
 * @brief: RGMII IO MACRO functions.
 */
#include "DWC_ETH_QOS_yheader.h"
#include "DWC_ETH_QOS_yrgmii_io_macro_regacc.h"

/* RGMII IO MACRO power on reset values */
#define RGMII_IO_MACRO_CONFIG_POR_ARR_INDEX 0
#define SDCC_HC_REG_DLL_CONFIG_POR_ARR_INDEX 1
#define SDCC_HC_REG_DDR_CONFIG_POR_ARR_INDEX 2
#define SDCC_HC_REG_DLL_CONFIG_2_POR_ARR_INDEX 3
#define SDCC_USR_CTL_POR_ARR_INDEX 4
#define RGMII_IO_MACRO_CONFIG_2_POR_ARR_INDEX 5

#define RGMII_IO_MACRO_DLL_POR_NUM_OF_REGS 6

#define RGMII_IO_MACRO_DLL_POR_REG_OFFSET_INDEX 0
#define RGMII_IO_MACRO_DLL_POR_REG_DATA_INDEX 1

ULONG rgmii_io_macro_dll_por_values
   [EMAC_HW_vMAX][RGMII_IO_MACRO_DLL_POR_NUM_OF_REGS][2] = {
	[EMAC_HW_None] = {
		[RGMII_IO_MACRO_CONFIG_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_RGOFFADDR_OFFSET, 0x0 },
		[SDCC_HC_REG_DLL_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_RGOFFADDR_OFFSET, 0x0 },
		[SDCC_HC_REG_DDR_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DDR_CONFIG_RGOFFADDR_OFFSET, 0x0 },
		[SDCC_HC_REG_DLL_CONFIG_2_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_2_RGOFFADDR_OFFSET, 0x0 },
		[SDCC_USR_CTL_POR_ARR_INDEX] = { SDCC_USR_CTL_RGOFFADDR_OFFSET, 0x0 },
		[RGMII_IO_MACRO_CONFIG_2_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_2_RGOFFADDR_OFFSET, 0x0 },
	},
	[EMAC_HW_v2_0_0] = {
		[RGMII_IO_MACRO_CONFIG_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_RGOFFADDR_OFFSET, 0x40C01343 },
		[SDCC_HC_REG_DLL_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_RGOFFADDR_OFFSET, 0x2004642C },
		[SDCC_HC_REG_DDR_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DDR_CONFIG_RGOFFADDR_OFFSET, 0x00000000 },
		[SDCC_HC_REG_DLL_CONFIG_2_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_2_RGOFFADDR_OFFSET, 0x00200000 },
		[SDCC_USR_CTL_POR_ARR_INDEX] = { SDCC_USR_CTL_RGOFFADDR_OFFSET, 0x00000000 },
		[RGMII_IO_MACRO_CONFIG_2_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_2_RGOFFADDR_OFFSET, 0x00002060 },
	},
	[EMAC_HW_v2_1_0] = {
		[RGMII_IO_MACRO_CONFIG_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_RGOFFADDR_OFFSET, 0x40C01343 },
		[SDCC_HC_REG_DLL_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_RGOFFADDR_OFFSET, 0x2004642C },
		[SDCC_HC_REG_DDR_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DDR_CONFIG_RGOFFADDR_OFFSET, 0x00000000 },
		[SDCC_HC_REG_DLL_CONFIG_2_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_2_RGOFFADDR_OFFSET, 0x00200000 },
		[SDCC_USR_CTL_POR_ARR_INDEX] = { SDCC_USR_CTL_RGOFFADDR_OFFSET, 0x00010800 },
		[RGMII_IO_MACRO_CONFIG_2_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_2_RGOFFADDR_OFFSET, 0x00002060 },
	},
	[EMAC_HW_v2_1_1] = {
		[RGMII_IO_MACRO_CONFIG_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_RGOFFADDR_OFFSET, 0x40C01343 },
		[SDCC_HC_REG_DLL_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_RGOFFADDR_OFFSET, 0x2004642C },
		[SDCC_HC_REG_DDR_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DDR_CONFIG_RGOFFADDR_OFFSET, 0x00000000 },
		[SDCC_HC_REG_DLL_CONFIG_2_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_2_RGOFFADDR_OFFSET, 0x00200000 },
		[SDCC_USR_CTL_POR_ARR_INDEX] = { SDCC_USR_CTL_RGOFFADDR_OFFSET, 0x00000000 },
		[RGMII_IO_MACRO_CONFIG_2_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_2_RGOFFADDR_OFFSET, 0x00002060 },
	},
	[EMAC_HW_v2_1_2] = {
		[RGMII_IO_MACRO_CONFIG_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_RGOFFADDR_OFFSET, 0x40C01343 },
		[SDCC_HC_REG_DLL_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_RGOFFADDR_OFFSET, 0x2004642C },
		[SDCC_HC_REG_DDR_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DDR_CONFIG_RGOFFADDR_OFFSET, 0x00000000 },
		[SDCC_HC_REG_DLL_CONFIG_2_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_2_RGOFFADDR_OFFSET, 0x00200000 },
		[SDCC_USR_CTL_POR_ARR_INDEX] = { SDCC_USR_CTL_RGOFFADDR_OFFSET, 0x00000000 },
		[RGMII_IO_MACRO_CONFIG_2_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_2_RGOFFADDR_OFFSET, 0x00002060 },
	},
	[EMAC_HW_v2_2_0] = {
		[RGMII_IO_MACRO_CONFIG_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_RGOFFADDR_OFFSET, 0x00C01343 },
		[SDCC_HC_REG_DLL_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_RGOFFADDR_OFFSET, 0x6004642C },
		[SDCC_HC_REG_DDR_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DDR_CONFIG_RGOFFADDR_OFFSET, 0x00000000 },
		[SDCC_HC_REG_DLL_CONFIG_2_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_2_RGOFFADDR_OFFSET, 0x00200000 },
		[SDCC_USR_CTL_POR_ARR_INDEX] = { SDCC_USR_CTL_RGOFFADDR_OFFSET, 0x00010800 },
		[RGMII_IO_MACRO_CONFIG_2_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_2_RGOFFADDR_OFFSET, 0x00002060 },
	},
	[EMAC_HW_v2_3_0] = {
		[RGMII_IO_MACRO_CONFIG_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_RGOFFADDR_OFFSET, 0x00C01343 },
		[SDCC_HC_REG_DLL_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_RGOFFADDR_OFFSET, 0x2004642C },
		[SDCC_HC_REG_DDR_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DDR_CONFIG_RGOFFADDR_OFFSET, 0x00000000 },
		[SDCC_HC_REG_DLL_CONFIG_2_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_2_RGOFFADDR_OFFSET, 0x00200000 },
		[SDCC_USR_CTL_POR_ARR_INDEX] = { SDCC_USR_CTL_RGOFFADDR_OFFSET, 0x00010800 },
		[RGMII_IO_MACRO_CONFIG_2_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_2_RGOFFADDR_OFFSET, 0x00002060 },
	},
	[EMAC_HW_v2_3_1] = {
		[RGMII_IO_MACRO_CONFIG_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_RGOFFADDR_OFFSET, 0x40C01343 },
		[SDCC_HC_REG_DLL_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_RGOFFADDR_OFFSET, 0x2004642C },
		[SDCC_HC_REG_DDR_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DDR_CONFIG_RGOFFADDR_OFFSET, 0x00000000 },
		[SDCC_HC_REG_DLL_CONFIG_2_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_2_RGOFFADDR_OFFSET, 0x00200000 },
		[SDCC_USR_CTL_POR_ARR_INDEX] = { SDCC_USR_CTL_RGOFFADDR_OFFSET, 0x00000000 },
		[RGMII_IO_MACRO_CONFIG_2_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_2_RGOFFADDR_OFFSET, 0x00002060 },
	},
	[EMAC_HW_v2_3_2] = {
		[RGMII_IO_MACRO_CONFIG_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_RGOFFADDR_OFFSET, 0x40C01343 },
		[SDCC_HC_REG_DLL_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_RGOFFADDR_OFFSET, 0x2004642C },
		[SDCC_HC_REG_DDR_CONFIG_POR_ARR_INDEX] = { SDCC_HC_REG_DDR_CONFIG_RGOFFADDR_OFFSET, 0x00000000 },
		[SDCC_HC_REG_DLL_CONFIG_2_POR_ARR_INDEX] = { SDCC_HC_REG_DLL_CONFIG_2_RGOFFADDR_OFFSET, 0x00200000 },
		[SDCC_USR_CTL_POR_ARR_INDEX] = { SDCC_USR_CTL_RGOFFADDR_OFFSET, 0x00000000 },
		[RGMII_IO_MACRO_CONFIG_2_POR_ARR_INDEX] = { RGMII_IO_MACRO_CONFIG_2_RGOFFADDR_OFFSET, 0x00002060 },
	},
};

/* SDCDC DLL initialization */
/*!
 * \brief Initialize the SDCDC
 *
 * \details This function will write to the various fields in
 * SDCC_HC_REG_DLL_CONFIG register for the DLL
 * initialization
 *\return Y_SUCCESS
 */
int DWC_ETH_QOS_rgmii_io_macro_sdcdc_init(struct DWC_ETH_QOS_prv_data *pdata)
{
	int reg_val = 0;
	ULONG RETRYCOUNT = 1000;
	ULONG current_cnt = 0;
	volatile ULONG VARDLL_LOCK;
	EMACDBG("Enter\n");

	/* Write 1 to DLL_RST bit of SDCC_HC_REG_DLL_CONFIG register */
	SDCC_HC_DLL_RST_UDFWR(0x1);

	/* Write 1 to PDN bit of SDCC_HC_REG_DLL_CONFIG register */
	SDCC_HC_PDN_UDFWR(0x1);

	/* Write 0 to DLL_RST bit of SDCC_HC_REG_DLL_CONFIG register */
	SDCC_HC_DLL_RST_UDFWR(0x0);

	/* Write 0 to PDN bit of SDCC_HC_REG_DLL_CONFIG register */
	SDCC_HC_PDN_UDFWR(0x0);

	/* DLL RST=0 and PDN=0 are sufficient for RGMII_ID 10/100 Mbps mode */
	if (pdata->speed == SPEED_100 || pdata->speed == SPEED_10) {
		EMACDBG("Bail out\n");
		return Y_SUCCESS;
	}
	/* Write 1 to DLL_EN */
	SDCC_HC_DLL_EN_UDFWR(0x1);

	/* Write 1 to CK_OUT_EN */
	SDCC_HC_CK_OUT_EN_UDFWR(0x1);

	SDCC_USR_CTL_RGRD(reg_val);
	reg_val &= ~(0x7UL<<24);
	reg_val |= (0x4UL<<24);
	SDCC_USR_CTL_RGWR(reg_val);

	/* Wait until DLL_LOCK bit of SDC4_STATUS register is 1 */
	while (1) {
		if (current_cnt > RETRYCOUNT)
			return -Y_FAILURE;

		SDC4_STATUS_DLL_LOCK_STS_UDFRD(VARDLL_LOCK);
		if (VARDLL_LOCK == 1) {
			EMACDBG("DLL lock status bit set. DLL init successful\n");
			break;
		}

		current_cnt++;
		mdelay(1);
	}

	EMACDBG("Exit\n");
	return Y_SUCCESS;
}

/* SDCDC low power mode */
/*!
 * \brief Put SDCDC in the lowest power consumption mode
 *
 * \details This function will write to the PDN field in
 * SDCC_HC_REG_DLL_CONFIG register to enable low power mode
 *
 *\return 0 on success and -ve on failure.
 */
int DWC_ETH_QOS_rgmii_io_macro_sdcdc_enable_lp_mode(void)
{
	EMACDBG("Enter\n");
	/* Write 1 to PDN bit of SDCC_HC_REG_DLL_CONFIG register */
	SDCC_HC_PDN_UDFWR(0x1);
	EMACDBG("Exit\n");
	return Y_SUCCESS;
}

/* SDCDC DLL configuration */
/*!
 * \brief Configure SDCDC DLL config
 *
 * \details This function will write to the various fields in
 * SDCC_HC_REG_DLL_CONFIG and SDCC_HC_REG_DLL_CONFIG_2
 * registers for DLL configuration
 *
 *\return 0 on success and -1 on failure.
 */
int DWC_ETH_QOS_rgmii_io_macro_sdcdc_config(struct DWC_ETH_QOS_prv_data *pdata)
{

	ULONG RETRYCOUNT = 1000;
	ULONG current_cnt = 0;
	volatile ULONG VARCK_OUT_EN;
	EMACDBG("Enter\n");

	if (pdata->emac_hw_version_type == EMAC_HW_v2_3_0) {
		/* Set CDR_EN bit to 1 */
		SDCC_HC_CDR_EN_UDFWR(0x1);
	} else {
		/* Set CDR_EN bit to 0 */
		SDCC_HC_CDR_EN_UDFWR(0x0);
	}

	/* Set CDR_EXT_EN bit to 1 */
	SDCC_HC_CDR_EXT_EN_UDFWR(0x1);

	/* Set CK_OUT_EN bit to 0 */
	SDCC_HC_CK_OUT_EN_UDFWR(0x0);

	/* Set DLL_EN bit to 1 */
	SDCC_HC_DLL_EN_UDFWR(0x1);

	if (pdata->emac_hw_version_type == EMAC_HW_v2_3_0) {
		/* Set MCLK_GATING_ENABLE bit to 0 */
		SDCC_HC_MCLK_GATING_ENABLE_UDFWR(0x0);

		/* Set CDR_FINE_PHASE bit to 0 */
		SDCC_HC_CDR_FINE_PHASE_UDFWR(0x0);
	}

	/* Wait until CK_OUT_EN bit of SDCC_HC_REG_DLL_CONFIG register
	 * is 0
	 */
	while (1) {
		if (current_cnt > RETRYCOUNT)
			return -Y_FAILURE;

		SDCC_HC_CK_OUT_EN_UDFRD(VARCK_OUT_EN);
		if (VARCK_OUT_EN == 0)
			break;

		current_cnt++;
		mdelay(1);
	}

	/* Set CK_OUT_EN bit to 1 */
	SDCC_HC_CK_OUT_EN_UDFWR(0x1);

	/* Wait until CK_OUT_EN bit of SDCC_HC_REG_DLL_CONFIG register
	 * is 1
	 */
	current_cnt = 0;
	while (1) {
		if (current_cnt > RETRYCOUNT)
			return -Y_FAILURE;

		SDCC_HC_CK_OUT_EN_UDFRD(VARCK_OUT_EN);
		if (VARCK_OUT_EN == 1)
			break;

		current_cnt++;
		mdelay(1);
	}

	/* Write 1 to DDR_CAL_EN bit of SDCC_HC_REG_DLL_CONFIG_2
	 *  register
	 */
	SDCC_HC_CFG_2_DDR_CAL_EN_UDFWR(0x1);

	if (pdata->emac_hw_version_type == EMAC_HW_v2_3_0) {
		/* Set DLL_CLOCK_DISABLE bit to 0 */
		SDCC_HC_CFG_2_DLL_CLOCK_DISABLE_UDFWR(0x0);

		/* Set MCLK_FREQ_CALC bit to 26 */
		SDCC_HC_CFG_2_MCLK_FREQ_CALC_UDFWR(0x1A);

		/* Set DDR_TRAFFIC_INIT_SEL bit to 0 */
		SDCC_HC_CFG_2_DDR_TRAFFIC_INIT_SEL_UDFWR(0x1);

		/* Set DDR_TRAFFIC_INIT_SW bit to 0 */
		SDCC_HC_CFG_2_DDR_TRAFFIC_INIT_SW_UDFWR(0x1);
	}


	EMACDBG("Exit\n");
	return Y_SUCCESS;
}

/* SDCC DLL Bypass mode programming */
/*!
 * \brief Enable SDCC DLL Bypass mode programming
 *
 * \details This function will write to the PDN field in
 * SDCC_HC_REG_DLL_CONFIG register and bypass bit in
 * SDCC_DLL_TEST_CTL register
 *
 *\return 0 on success
 */
int DWC_ETH_QOS_sdcc_set_bypass_mode(void)
{
	EMACDBG("Enter\n");
	/* Write 1 to PDN bit of SDCC_HC_REG_DLL_CONFIG register */
	SDCC_HC_PDN_UDFWR(0x1);

	/* Write 1 to bypass bit of SDCC_USR_CTL register */
	SDCC_USR_CTL_BYPASS_MODE_UDFWR(0x1);

	EMACDBG("Exit\n");
	return Y_SUCCESS;
}

/* Programming rgmii_io_macro register for loopback mode */
/*!
 * \brief Initialize the rgmii io macro block
 *
 * \details This function will write to the loopback
 * fields in RGMII_IO_MACRO_CONFIG RGMII_IO_MACRO_CONFIG_2
 * registers to set the RGMII loopback mode
 *
 *\return 0 on success
 */
static int DWC_ETH_QOS_set_rgmii_loopback_mode(UINT lb_mode)
{
	EMACDBG("Enter\n");
	RGMII_CONFIG_2_TX_TO_RX_LOOPBACK_EN_UDFWR(lb_mode);
	EMACDBG("RGMII loopback mode set to = %d\n", lb_mode);

	EMACDBG("Exit\n");
	return Y_SUCCESS;
}

/* Programming rgmii_io_macro register for func_clk_en */
/*!
 * \brief Initialize the rgmii io macro block
 *
 * \details This function will write to the func_clk_en
 * fields in RGMII_IO_MACRO_CONFIG
 *
 *\return 0 on success
 */
int DWC_ETH_QOS_set_rgmii_func_clk_en(void)
{
	EMACDBG("Enter\n");
	RGMII_FUNC_CLK_EN_UDFWR(1);
	EMACDBG("Exit\n");
	return Y_SUCCESS;
}

/* Programming rgmii_io_macro register initialization */
/*!
 * \brief Initialize the rgmii io macro block
 *
 * \details This function will write to the various
 * fields in RGMII_IO_MACRO_CONFIG register to set the RGMII mode
 *
 *\return 0 on success
 */
int DWC_ETH_QOS_rgmii_io_macro_init(struct DWC_ETH_QOS_prv_data *pdata)
{
	uint loopback_mode = 0;
	uint loopback_mode_en = 0;
	uint rgmii_data_divide_clk;
	ULONG data;

	if (pdata->emac_hw_version_type == EMAC_HW_v2_3_0 || (pdata->emac_hw_version_type == EMAC_HW_v2_3_1)
		|| (pdata->emac_hw_version_type == EMAC_HW_v2_1_1)) {
		if(pdata->io_macro_phy_intf == RGMII_MODE)
			loopback_mode_en = 0x1;
		rgmii_data_divide_clk = 0x0;
	} else {
		loopback_mode_en = 0x0;
		rgmii_data_divide_clk = 0x1;
	}

	EMACDBG("Enter\n");

	/* Loopback is disabled */
	DWC_ETH_QOS_set_rgmii_loopback_mode(loopback_mode);

	switch (pdata->io_macro_phy_intf) {
	case RGMII_MODE:
		/* Select RGMII interface */
		RGMII_INTF_SEL_UDFWR(0x0);

		switch (pdata->speed) {
		case SPEED_1000:
			EMACDBG("Set RGMII registers for speed = %d\n", pdata->speed);
			if (pdata->io_macro_tx_mode_non_id){
				EMACDBG(
					"Set registers for Bypass mode = %d\n",
					pdata->io_macro_tx_mode_non_id);
				/* Enable DDR mode*/
				RGMII_DDR_MODE_UDFWR(0x1);
				RGMII_BYPASS_TX_ID_EN_UDFWR(0x1);
				RGMII_POS_NEG_DATA_SEL_UDFWR(0x0);
				RGMII_PROG_SWAP_UDFWR(0x0);
				RGMII_CONFIG_2_DATA_DIVIDE_CLK_SEL_UDFWR(rgmii_data_divide_clk);
				RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x0);
				RGMII_CONFIG_2_RERVED_CONFIG_16_EN_UDFWR(0x0);
				/* Rx Path */
				RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x1);
				RGMII_LOOPBACK_EN_UDFWR(loopback_mode_en);
				if (pdata->emac_hw_version_type == EMAC_HW_v2_1_0 ||
					pdata->emac_hw_version_type == EMAC_HW_v2_1_2 ||
					(pdata->emac_hw_version_type == EMAC_HW_v2_3_1) ||
					pdata->emac_hw_version_type == EMAC_HW_v2_1_1)
					RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x1);
			} else {
				/* Enable DDR mode*/
				RGMII_DDR_MODE_UDFWR(0x1);
				RGMII_BYPASS_TX_ID_EN_UDFWR(0x0);
				RGMII_POS_NEG_DATA_SEL_UDFWR(0x1);
				/* RGMII_TX_POS and RGMII_TX_NEG input pins are swapped
				 * based on the programmable swap control bit
				 */
				RGMII_PROG_SWAP_UDFWR(0x1);
				RGMII_CONFIG_2_DATA_DIVIDE_CLK_SEL_UDFWR(rgmii_data_divide_clk);
				RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x1);
				RGMII_CONFIG_2_RERVED_CONFIG_16_EN_UDFWR(0x0);
				/* If data arrives at positive edge or if data is
				 * delayed by 1.5ns/ 2ns then write 1 to RX_PROG_SWAP
				 * bit of register EMAC_RGMII_IO_MACRO_CONFIG_2
				 */
				RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x1);

				/* Program PRG_RCLK_DLY to 52 ns for a required delay of 2 ns on HANA AU */
				if (pdata->emac_hw_version_type == EMAC_HW_v2_1_0 ||
					pdata->emac_hw_version_type == EMAC_HW_v2_1_2)
					SDCC_HC_PRG_RCLK_DLY_UDFWR(52);
				else if (pdata->emac_hw_version_type == EMAC_HW_v2_3_1)
					SDCC_HC_PRG_RCLK_DLY_UDFWR(104);
				else if (pdata->emac_hw_version_type == EMAC_HW_v2_1_1)
					SDCC_HC_PRG_RCLK_DLY_UDFWR(130);
				else { /* Program PRG_RCLK_DLY to 57 for a required delay of 1.8 ns */
					SDCC_HC_PRG_RCLK_DLY_UDFWR(57);
				}
				SDCC_HC_REG_DDR_CONFIG_RGRD(data);
				data |= (1 << 31);
				SDCC_HC_REG_DDR_CONFIG_RGWR(data);
				RGMII_LOOPBACK_EN_UDFWR(loopback_mode_en);
			}
			break;

		case SPEED_100:
			EMACDBG("Set RGMII registers for speed = %d\n", pdata->speed);

			if (pdata->io_macro_tx_mode_non_id) {
				EMACDBG("Set registers for Bypass mode = %d\n",
					pdata->io_macro_tx_mode_non_id);
				RGMII_DDR_MODE_UDFWR(0x0);
				RGMII_BYPASS_TX_ID_EN_UDFWR(0x1);
				RGMII_POS_NEG_DATA_SEL_UDFWR(0x0);
				RGMII_PROG_SWAP_UDFWR(0x0);
				RGMII_CONFIG_2_DATA_DIVIDE_CLK_SEL_UDFWR(rgmii_data_divide_clk);
				RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x0);

				RGMII_MAX_SPD_PRG_2_UDFWR(0x1);
				RGMII_CONFIG_2_RERVED_CONFIG_16_EN_UDFWR(0x1);
				/* Rx Path */
				RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x0);
				RGMII_LOOPBACK_EN_UDFWR(loopback_mode_en);
				if (pdata->emac_hw_version_type == EMAC_HW_v2_1_0 ||
					pdata->emac_hw_version_type == EMAC_HW_v2_1_2 ||
					(pdata->emac_hw_version_type == EMAC_HW_v2_3_1) ||
					pdata->emac_hw_version_type == EMAC_HW_v2_1_1)
					RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x1);
				if (pdata->emac_hw_version_type == EMAC_HW_v2_1_2 ||
					pdata->emac_hw_version_type == EMAC_HW_v2_1_1)
					RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x1);
			} else{
				RGMII_DDR_MODE_UDFWR(0x1);
				RGMII_BYPASS_TX_ID_EN_UDFWR(0x1);
				RGMII_POS_NEG_DATA_SEL_UDFWR(0x0);
				RGMII_PROG_SWAP_UDFWR(0x0);
				RGMII_CONFIG_2_DATA_DIVIDE_CLK_SEL_UDFWR(rgmii_data_divide_clk);
				RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x1);
				RGMII_MAX_SPD_PRG_2_UDFWR(0x1);
				RGMII_CONFIG_2_RERVED_CONFIG_16_EN_UDFWR(0x0);
				/* Rx Path */
				if (pdata->emac_hw_version_type == EMAC_HW_v2_3_0)
					RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x0);
				else
					RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x1);

				SDCC_HC_EXT_PRG_RCLK_DLY_CODE_UDFWR(0x5);
				SDCC_HC_EXT_PRG_RCLK_DLY_UDFWR(0x3f);
				SDCC_HC_EXT_PRG_RCLK_DLY_EN_UDFWR(0x1);
				RGMII_LOOPBACK_EN_UDFWR(loopback_mode_en);
			}
			break;

		case SPEED_10:
			EMACDBG("Set RGMII registers for speed = %d\n", pdata->speed);

			if (pdata->io_macro_tx_mode_non_id) {
				EMACDBG("Set registers for Bypass mode = %d\n",
					pdata->io_macro_tx_mode_non_id);
				RGMII_DDR_MODE_UDFWR(0x0);
				RGMII_BYPASS_TX_ID_EN_UDFWR(0x1);
				RGMII_POS_NEG_DATA_SEL_UDFWR(0x0);
				RGMII_PROG_SWAP_UDFWR(0x0);
				RGMII_CONFIG_2_DATA_DIVIDE_CLK_SEL_UDFWR(rgmii_data_divide_clk);
				RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x0);
				RGMII_MAX_SPD_PRG_9_UDFWR(0x13);
				RGMII_CONFIG_2_RERVED_CONFIG_16_EN_UDFWR(0x1);
				/* Rx Path */
				if (pdata->emac_hw_version_type == EMAC_HW_v2_3_1)
					RGMII_LOOPBACK_EN_UDFWR(loopback_mode_en);
				RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x0);
				RGMII_LOOPBACK_EN_UDFWR(loopback_mode_en);
				if (pdata->emac_hw_version_type == EMAC_HW_v2_1_0 ||
					pdata->emac_hw_version_type == EMAC_HW_v2_1_2 ||
					(pdata->emac_hw_version_type == EMAC_HW_v2_3_1) ||
					pdata->emac_hw_version_type == EMAC_HW_v2_1_1)
					RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x1);
				if (pdata->emac_hw_version_type == EMAC_HW_v2_1_2 ||
					pdata->emac_hw_version_type == EMAC_HW_v2_1_1)
					RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x1);
			} else{
				RGMII_DDR_MODE_UDFWR(0x1);
				RGMII_BYPASS_TX_ID_EN_UDFWR(0x1);
				RGMII_POS_NEG_DATA_SEL_UDFWR(0x0);
				RGMII_PROG_SWAP_UDFWR(0x0);
				RGMII_CONFIG_2_DATA_DIVIDE_CLK_SEL_UDFWR(rgmii_data_divide_clk);
				RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x1);
				RGMII_MAX_SPD_PRG_9_UDFWR(0x13);
				RGMII_CONFIG_2_RERVED_CONFIG_16_EN_UDFWR(0x0);

				/* Rx Path */
				if (pdata->emac_hw_version_type == EMAC_HW_v2_3_0)
					RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x0);
				else
					RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x1);
				SDCC_HC_EXT_PRG_RCLK_DLY_CODE_UDFWR(0x5);
				SDCC_HC_EXT_PRG_RCLK_DLY_UDFWR(0x3f);
				SDCC_HC_EXT_PRG_RCLK_DLY_EN_UDFWR(0x1);
				RGMII_LOOPBACK_EN_UDFWR(loopback_mode_en);
			}
			break;

		default:
			EMACDBG(
				"No RGMII register settings for link speed = %d\n",
				pdata->speed);
			break;
		}
		break;

	case RMII_MODE:
		EMACDBG("Set registers for RMII mode and speed = %d\n", pdata->speed);
		RGMII_INTF_SEL_UDFWR(0x01);
		RGMII_DDR_MODE_UDFWR(0x0);
		RGMII_BYPASS_TX_ID_EN_UDFWR(0x1);
		RGMII_POS_NEG_DATA_SEL_UDFWR(0x0);
		RGMII_PROG_SWAP_UDFWR(0x1);
		RGMII_CONFIG_2_DATA_DIVIDE_CLK_SEL_UDFWR(0x1);
		RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x0);
		RGMII_CONFIG_2_CLK_DIVIDE_SEL_UDFWR(0x1);
		RGMII_MAX_SPD_PRG_2_UDFWR(0x1);
		RGMII_MAX_SPD_PRG_9_UDFWR(0x13);
		RGMII_CONFIG_2_RERVED_CONFIG_16_EN_UDFWR(0x0);
		if (pdata->emac_hw_version_type == EMAC_HW_v2_3_0 || (pdata->emac_hw_version_type == EMAC_HW_v2_3_1))
			RGMII_LOOPBACK_EN_UDFWR(0x0);
		else
			RGMII_LOOPBACK_EN_UDFWR(0x1);

		break;

	case MII_MODE:
		EMACDBG("Set registers for MII mode and speed = %d\n", pdata->speed);
		RGMII_INTF_SEL_UDFWR(0x2);
		RGMII_DDR_MODE_UDFWR(0x0);
		RGMII_BYPASS_TX_ID_EN_UDFWR(0x1);
		RGMII_POS_NEG_DATA_SEL_UDFWR(0x0);
		RGMII_PROG_SWAP_UDFWR(0x0);
		RGMII_CONFIG_2_DATA_DIVIDE_CLK_SEL_UDFWR(0x1);
		RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x0);
		RGMII_CONFIG_2_RERVED_CONFIG_16_EN_UDFWR(0x1);
		if (pdata->emac_hw_version_type == EMAC_HW_v2_1_2 ||
			pdata->emac_hw_version_type == EMAC_HW_v2_1_1)
			RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x1);
		if (pdata->emac_hw_version_type == EMAC_HW_v2_3_1)
			RGMII_LOOPBACK_EN_UDFWR(0x1);
		break;
	}

	EMACDBG("Exit\n");
	return Y_SUCCESS;
}

/* Programming IO macro and DLL registers with POR values */
/*!
 * \brief Reset the IO MACRO and DLL blocks
 *
 * \details This function will write POR values to RGMII and
 * DLL registers.
 *
 *\return Y_SUCCESS
 */
int DWC_ETH_QOS_rgmii_io_macro_dll_reset(struct DWC_ETH_QOS_prv_data *pdata)
{
	int index_of_reg;
	ULONG data, address;
	EMACDBG("Enter\n");

	for (index_of_reg = 0 ; index_of_reg < RGMII_IO_MACRO_DLL_POR_NUM_OF_REGS; index_of_reg++) {
		address = RGMII_IO_BASE_ADDRESS
			+ rgmii_io_macro_dll_por_values
			[pdata->emac_hw_version_type]
			[index_of_reg]
			[RGMII_IO_MACRO_DLL_POR_REG_OFFSET_INDEX];
		data = rgmii_io_macro_dll_por_values
				[pdata->emac_hw_version_type]
				[index_of_reg]
				[RGMII_IO_MACRO_DLL_POR_REG_DATA_INDEX];
		iowrite32(data, (void *)address);
	}

	DWC_ETH_QOS_set_rgmii_func_clk_en();

	EMACDBG("Exit\n");
	return Y_SUCCESS;
}

void dump_rgmii_io_macro_registers(void)
{
	int reg_val;

	pr_alert(
			"\n************* RGMII IO Macro Reg dump *************************\n");

	RGMII_IO_MACRO_CONFIG_RGRD(reg_val);
	pr_alert(
			"RGMII_IO_MACRO_CONFIG (0x%p) = %#x\n",
			RGMII_IO_MACRO_CONFIG_RGOFFADDR, reg_val);

	SDCC_HC_REG_DLL_CONFIG_RGRD(reg_val);
	pr_alert(
			"SDCC_HC_REG_DLL_CONFIG (0x%p) = %#x\n",
			SDCC_HC_REG_DLL_CONFIG_RGOFFADDR, reg_val);

	SDCC_HC_REG_DDR_CONFIG_RGRD(reg_val);
	pr_alert(
			"SDCC_HC_REG_DDR_CONFIG (0x%p) = %#x\n",
			SDCC_HC_REG_DDR_CONFIG_RGOFFADDR, reg_val);

	SDCC_HC_REG_DLL_CONFIG_2_RGRD(reg_val);
	pr_alert(
			"SDCC_HC_REG_DLL_CONFIG_2 (0x%p) = %#x\n",
			SDCC_HC_REG_DLL_CONFIG_2_RGOFFADDR, reg_val);

	RGMII_IO_MACRO_CONFIG_2_RGRD(reg_val);
	pr_alert(
			"RGMII_IO_MACRO_CONFIG_2 (0x%p) = %#x\n",
			RGMII_IO_MACRO_CONFIG_2_RGOFFADDR, reg_val);

	RGMII_IO_MACRO_DEBUG_1_RGRD(reg_val);
	pr_alert(
			"RGMII_IO_MACRO_DEBUG_1 (0x%p) = %#x\n",
			RGMII_IO_MACRO_DEBUG_1_RGOFFADDR, reg_val);

	SDC4_STATUS_RGRD(reg_val);
	pr_alert(
			"SDC4_STATUS_RGRD (0x%p) = %#x\n",
			SDC4_STATUS_RGOFFADDR, reg_val);

	SDCC_USR_CTL_RGRD(reg_val);
	pr_alert(
			"SDCC_USR_CTL_RGRD (0x%p) = %#x\n",
			SDCC_USR_CTL_RGOFFADDR, reg_val);

	EMAC_SYSTEM_LOW_POWER_DEBUG_RGRD(reg_val);
	pr_alert(
			"EMAC_SYSTEM_LOW_POWER_DEBUG_RGRD (0x%p) = %#x\n",
			EMAC_SYSTEM_LOW_POWER_DEBUG_RGOFFADDR, reg_val);

	pr_alert("\n****************************************************\n");
}
