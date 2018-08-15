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
int DWC_ETH_QOS_rgmii_io_macro_sdcdc_config(void)
{

	ULONG RETRYCOUNT = 1000;
	ULONG current_cnt = 0;
	volatile ULONG VARCK_OUT_EN;
	EMACDBG("Enter\n");

	/* Set CDR_EN bit to 0 */
	SDCC_HC_CDR_EN_UDFWR(0x0);

	/* Set CDR_EXT_EN bit to 1 */
	SDCC_HC_CDR_EXT_EN_UDFWR(0x1);

	/* Set CK_OUT_EN bit to 0 */
	SDCC_HC_CK_OUT_EN_UDFWR(0x0);

	/* Set DLL_EN bit to 1 */
	SDCC_HC_DLL_EN_UDFWR(0x1);

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
	uint loopback_mode = 0x0;
	ULONG data;

	EMACDBG("Enter\n");

	if (pdata->emac_hw_version_type == EMAC_HW_v2_3_0)
		loopback_mode = 0x1;

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
				RGMII_CONFIG_2_DATA_DIVIDE_CLK_SEL_UDFWR(0x1);
				RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x0);
				RGMII_CONFIG_2_RERVED_CONFIG_16_EN_UDFWR(0x0);
				/* Rx Path */
				RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x1);
				RGMII_LOOPBACK_EN_UDFWR(0x0);
				if (pdata->emac_hw_version_type == EMAC_HW_v2_1_0)
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
				RGMII_CONFIG_2_DATA_DIVIDE_CLK_SEL_UDFWR(0x1);
				RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x1);
				RGMII_CONFIG_2_RERVED_CONFIG_16_EN_UDFWR(0x0);
				/* If data arrives at positive edge or if data is
				 * delayed by 1.5ns/ 2ns then write 1 to RX_PROG_SWAP
				 * bit of register EMAC_RGMII_IO_MACRO_CONFIG_2
				 */
				RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x1);

				/* Program PRG_RCLK_DLY to 52 ns for a required delay of 2 ns
				on EMAC core version 2.1.0 */
				if (pdata->emac_hw_version_type == EMAC_HW_v2_1_0)
					SDCC_HC_PRG_RCLK_DLY_UDFWR(52);
				else { /* Program PRG_RCLK_DLY to 57 for a required delay of 1.8 ns */
					SDCC_HC_PRG_RCLK_DLY_UDFWR(57);
				}
				SDCC_HC_REG_DDR_CONFIG_RGRD(data);
				data |= (1 << 31);
				SDCC_HC_REG_DDR_CONFIG_RGWR(data);
				RGMII_LOOPBACK_EN_UDFWR(0x0);
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
				RGMII_CONFIG_2_DATA_DIVIDE_CLK_SEL_UDFWR(0x1);
				RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x0);

				RGMII_MAX_SPD_PRG_2_UDFWR(0x1);
				RGMII_CONFIG_2_RERVED_CONFIG_16_EN_UDFWR(0x1);
				/* Rx Path */
				RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x0);
				RGMII_LOOPBACK_EN_UDFWR(0x0);
				if (pdata->emac_hw_version_type == EMAC_HW_v2_1_0)
					RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x1);

			} else{
				RGMII_DDR_MODE_UDFWR(0x1);
				RGMII_BYPASS_TX_ID_EN_UDFWR(0x1);
				RGMII_POS_NEG_DATA_SEL_UDFWR(0x0);
				RGMII_PROG_SWAP_UDFWR(0x0);
				RGMII_CONFIG_2_DATA_DIVIDE_CLK_SEL_UDFWR(0x1);
				RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x1);
				RGMII_MAX_SPD_PRG_2_UDFWR(0x1);
				RGMII_CONFIG_2_RERVED_CONFIG_16_EN_UDFWR(0x0);
				/* Rx Path */
				RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x1);
				SDCC_HC_EXT_PRG_RCLK_DLY_CODE_UDFWR(0x5);
				SDCC_HC_EXT_PRG_RCLK_DLY_UDFWR(0x3f);
				SDCC_HC_EXT_PRG_RCLK_DLY_EN_UDFWR(0x1);
				RGMII_LOOPBACK_EN_UDFWR(0x0);
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
				RGMII_CONFIG_2_DATA_DIVIDE_CLK_SEL_UDFWR(0x1);
				RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x0);
				RGMII_MAX_SPD_PRG_9_UDFWR(0x13);
				RGMII_CONFIG_2_RERVED_CONFIG_16_EN_UDFWR(0x1);
				/* Rx Path */
				RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x0);
				if (pdata->emac_hw_version_type == EMAC_HW_v2_1_0)
					RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x1);
			} else{
				RGMII_DDR_MODE_UDFWR(0x1);
				RGMII_BYPASS_TX_ID_EN_UDFWR(0x1);
				RGMII_POS_NEG_DATA_SEL_UDFWR(0x0);
				RGMII_PROG_SWAP_UDFWR(0x0);
				RGMII_CONFIG_2_DATA_DIVIDE_CLK_SEL_UDFWR(0x1);
				RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(0x1);
				RGMII_MAX_SPD_PRG_9_UDFWR(0x13);
				RGMII_CONFIG_2_RERVED_CONFIG_16_EN_UDFWR(0x0);

				/* Rx Path */
				RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(0x1);
				SDCC_HC_EXT_PRG_RCLK_DLY_CODE_UDFWR(0x5);
				SDCC_HC_EXT_PRG_RCLK_DLY_UDFWR(0x3f);
				SDCC_HC_EXT_PRG_RCLK_DLY_EN_UDFWR(0x1);
				RGMII_LOOPBACK_EN_UDFWR(0x0);
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
		if (pdata->emac_hw_version_type == EMAC_HW_v2_3_0)
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
int DWC_ETH_QOS_rgmii_io_macro_dll_reset(void)
{
	EMACDBG("Enter\n");
	RGMII_IO_MACRO_CONFIG_RGWR(EMAC_RGMII_IO_MACRO_CONFIG_POR);
	RGMII_IO_MACRO_CONFIG_2_RGWR(EMAC_RGMII_IO_MACRO_CONFIG_2_POR);
	SDCC_HC_REG_DLL_CONFIG_RGWR(EMAC_SDCC_HC_REG_DLL_CONFIG_POR);
	SDCC_HC_REG_DDR_CONFIG_RGWR(EMAC_SDCC_HC_REG_DDR_CONFIG_POR);
	SDCC_HC_REG_DLL_CONFIG_2_RGWR(EMAC_SDCC_HC_REG_DLL_CONFIG_2_POR);
	SDCC_USR_CTL_RGWR(EMAC_SDCC_USR_CTL_POR);

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
