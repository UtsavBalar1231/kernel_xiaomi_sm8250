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

/*!@file: DWC_ETH_QOS_eee.c
 * @brief: Driver functions.
 */
#include "DWC_ETH_QOS_yheader.h"

/* Clause 22 registers to access clause 45 register set */
#define MMD_CTRL_REG		0x0D	/* MMD Access Control Register */
#define MMD_ADDR_DATA_REG	0x0E	/* MMD Access Address Data Register */

/* MMD Access Control register fields */
#define MMD_CTRL_FUNC_ADDR		0x0000	/* address */
#define MMD_CTRL_FUNC_DATA_NOINCR	0x4000	/* data, no post increment */
/* data, post increment on reads & writes */
#define MMD_CTRL_FUNC_DATA_INCR_ON_RDWT	0x8000

/* data, post increment on writes only */
#define MMD_CTRL_FUNC_DATA_INCR_ON_WT	0xC000

/* Clause 45 expansion register */
#define CL45_PCS_EEE_ABLE 0x14	/* EEE Capability register */
#define CL45_ADV_EEE_REG 0x3C   /* EEE advertisement */
#define CL45_AN_EEE_LPABLE_REG	0x3D	/* EEE Link Partner ability reg */
#define CL45_CLK_STOP_EN_REG 0x0 /* Clock Stop enable reg */

/* Clause 45 expansion registers fields */
/* LP EEE capabilities status */
#define CL45_LP_ADV_EEE_STATS_1000BASE_T 0x0004

#define CL45_CLK_STOP_EN	0x400 /* Enable xMII Clock Stop */

#define AR8035_SMART_EEE_CTRL_3 0x805D
#define AR8035_SMART_EEE_EN (1<<8)

#define PHY_RX_CLOCK_STOPPABLE_EN 1
#define PHY_RX_CLOCK_STOPPABLE_DIS 0

void DWC_ETH_QOS_enable_eee_mode(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct DWC_ETH_QOS_tx_wrapper_descriptor *tx_desc_data = NULL;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int tx_idle = 0, QINX;

	DBGPR_EEE("-->DWC_ETH_QOS_enable_eee_mode\n");

	for (QINX = 0; pdata->tx_queue_cnt > QINX ; QINX++) {
		tx_desc_data = GET_TX_WRAPPER_DESC(QINX);

		if ((tx_desc_data->dirty_tx == tx_desc_data->cur_tx) &&
		    (!pdata->tx_path_in_lpi_mode)) {
			tx_idle = 1;
		} else {
			tx_idle = 0;
			break;
		}
	}

	if (tx_idle)
		hw_if->set_eee_mode();

	DBGPR_EEE("<--DWC_ETH_QOS_enable_eee_mode\n");
}

void DWC_ETH_QOS_disable_eee_mode(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;

	DBGPR_EEE("-->DWC_ETH_QOS_disable_eee_mode\n");

	hw_if->reset_eee_mode();
	del_timer_sync(&pdata->eee_ctrl_timer);
	pdata->tx_path_in_lpi_mode = false;
	pdata->eee_active = 0;

	DBGPR_EEE("-->DWC_ETH_QOS_disable_eee_mode\n");
}

/*!
* \brief API to control EEE mode.
*
* \details This function will move the MAC transmitter in LPI mode
* if there is no data transfer and MAC is not already in LPI state.
*
* \param[in] data - data hook
*
* \return void
*/

static void DWC_ETH_QOS_eee_ctrl_timer(unsigned long data)
{
	struct DWC_ETH_QOS_prv_data *pdata =
		(struct DWC_ETH_QOS_prv_data *)data;

	DBGPR_EEE("-->DWC_ETH_QOS_eee_ctrl_timer\n");

	DWC_ETH_QOS_enable_eee_mode(pdata);

	DBGPR_EEE("<--DWC_ETH_QOS_eee_ctrl_timer\n");
}

static void DWC_ETH_QOS_mmd_phy_indirect(struct mii_bus *bus,
					 int REGADDR,
					 int DEVADDR,
					 int PHYADDR)
{
	/* Write the desired MMD devAddr */
	bus->write(bus, PHYADDR, MMD_CTRL_REG, DEVADDR);

	/* Write the desired MMD regAddr */
	bus->write(bus, PHYADDR, MMD_ADDR_DATA_REG, REGADDR);

	/* Select the Function : DATA with no post increment */
	bus->write(bus, PHYADDR, MMD_CTRL_REG,
		(DEVADDR | MMD_CTRL_FUNC_DATA_NOINCR));
}

/*!
* \brief API to read data from the MMD registers.
*
* \details This function will read data from the MMD(clause 45) registers
* using clause 22 registers. The procedure to read MMD registers is,
* 1. Write the desired MMD device addr into reg 13
* 2. Write the desired MMD reg addr into reg 14
* 3. Select the desired Function - MMD data command by writing in reg 13
* 4. Read the content of the MMD's selected reg through reg 14
*
* \param[in] bus - the target MII bus
* \param[in] regAddr - desired MMD reg addr to be read
* \param[in] devAddr - desired MMD address
* \param[in] phyAddr - PHY addr/id on the MII bus
*
* \return integer
*/
static int DWC_ETH_QOS_phy_read_mmd_indirect(struct mii_bus *bus,
					     int REGADDR,
					     int DEVADDR,
					     int PHYADDR)
{
	u32 ret;

	DBGPR_EEE("-->DWC_ETH_QOS_phy_read_mmd_indirect\n");

	DWC_ETH_QOS_mmd_phy_indirect(bus, REGADDR, DEVADDR, PHYADDR);
	/* read the content of the MMD's selected register */
	ret = bus->read(bus, PHYADDR, MMD_ADDR_DATA_REG);

	DBGPR_EEE("<--DWC_ETH_QOS_phy_read_mmd_indirect\n");

	return ret;
}

/*!
 * \brief API to write data into the MMD registers.
 *
 * \details This function will write data into MMD(clause 45) registers
 * using clause 22 registers. The procedure to write MMD registers is,
 * 1. Write the desired MMD device addr into reg 13
 * 2. Write the desired MMD reg addr into reg 14
 * 3. Select the desired Function - MMD data command by writing in reg 13
 * 4. Write the data into MMD's selected reg through reg 14
 *
 * \param[in] bus - the target MII bus
 * \param[in] regAddr - desired MMD reg addr to be written
 * \param[in] devAddr - desired MMD address
 * \param[in] phyAddr - PHY addr/id on the MII bus
 * \param[in] data - data to write into the MMD register
 *
 * \return void
 */
static void DWC_ETH_QOS_phy_write_mmd_indirect(struct mii_bus *bus,
					       int REGADDR,
					     int DEVADDR,
					     int PHYADDR,
					     u32 data)
{
	DBGPR_EEE("-->DWC_ETH_QOS_phy_write_mmd_indirect\n");

	DWC_ETH_QOS_mmd_phy_indirect(bus, REGADDR, DEVADDR, PHYADDR);
	/* Write the data into MMD's selected register */
	bus->write(bus, PHYADDR, MMD_ADDR_DATA_REG, data);

	DBGPR_EEE("<--DWC_ETH_QOS_phy_write_mmd_indirect\n");
}

#if 0

#define MDIO_EEE_100TX		0x0002	/* EEE is supported for 100BASE-TX */
#define MDIO_EEE_1000T		0x0004	/* EEE is supported for 1000BASE-T */
#define MDIO_EEE_10GT		0x0008	/* EEE is supported for 10GBASE-T */
#define MDIO_EEE_1000KX		0x0010	/* EEE is supported for 1000BASE-KX */
#define MDIO_EEE_10GKX4		0x0020	/* EEE is supported for 10GBASE-KX4 */
#define MDIO_EEE_10GKR		0x0040	/* EEE is supported for 10GBASE KR */

/* A small helper function that translates MMD EEE Capability (3.20) bits
 * to ethtool supported settings.
 */
static u32 DWC_ETH_QOS_mmd_eee_cap_to_ethtool_sup_t(u16 eee_cap)
{
	u32 supported = 0;

	if (eee_cap & MDIO_EEE_100TX)
		supported |= SUPPORTED_100baseT_Full;
	if (eee_cap & MDIO_EEE_1000T)
		supported |= SUPPORTED_1000baseT_Full;
	if (eee_cap & MDIO_EEE_10GT)
		supported |= SUPPORTED_10000baseT_Full;
	if (eee_cap & MDIO_EEE_1000KX)
		supported |= SUPPORTED_1000baseKX_Full;
	if (eee_cap & MDIO_EEE_10GKX4)
		supported |= SUPPORTED_10000baseKX4_Full;
	if (eee_cap & MDIO_EEE_10GKR)
		supported |= SUPPORTED_10000baseKR_Full;

	return supported;
}

/* A small helper function that translates the MMD EEE Advertisment (7.60)
 * and MMD EEE Link Partner Ability (7.61) bits to ethtool advertisement
 * settings.
 */
static inline u32 DWC_ETH_QOS_mmd_eee_adv_to_ethtool_adv_t(u16 eee_adv)
{
	u32 adv = 0;

	if (eee_adv & MDIO_EEE_100TX)
		adv |= ADVERTISED_100baseT_Full;
	if (eee_adv & MDIO_EEE_1000T)
		adv |= ADVERTISED_1000baseT_Full;
	if (eee_adv & MDIO_EEE_10GT)
		adv |= ADVERTISED_10000baseT_Full;
	if (eee_adv & MDIO_EEE_1000KX)
		adv |= ADVERTISED_1000baseKX_Full;
	if (eee_adv & MDIO_EEE_10GKX4)
		adv |= ADVERTISED_10000baseKX4_Full;
	if (eee_adv & MDIO_EEE_10GKR)
		adv |= ADVERTISED_10000baseKR_Full;

	return adv;
}
#endif

/*!
* \brief API to disable EEE mode.
*
* \details This function disable smart EEE
* \param[in] phydev - pointer to target phy_device structure
*/

static void DWC_ETH_QOS_disable_smart_eee(struct phy_device *phydev)
{
	u32 smart_eee;

	smart_eee = DWC_ETH_QOS_phy_read_mmd_indirect(
		phydev->mdio.bus, AR8035_SMART_EEE_CTRL_3,
		MDIO_MMD_PCS, phydev->mdio.addr);

	smart_eee &= ~AR8035_SMART_EEE_EN;
	DWC_ETH_QOS_phy_write_mmd_indirect(
		phydev->mdio.bus, AR8035_SMART_EEE_CTRL_3,
		MDIO_MMD_PCS,phydev->mdio.addr, smart_eee);

}

/*!
* \brief API to initialize and check EEE mode.
*
* \details This function checks if the EEE is supported by
* looking at the MMD registers and it also programs the MMD
* register 3.0 setting the "Clock stop enable" bit if required.
*
* \param[in] phydev - pointer to target phy_device structure
* \param[in] clk_stop_enable - PHY may stop the clock during LPI
*
* \return integer
*
* \retval zero if EEE is supported else return -ve number.
*/
static int DWC_ETH_QOS_phy_init_eee(struct phy_device *phydev,
				    bool clk_stop_enable)
{
	int ret = -EPROTONOSUPPORT;

	DBGPR_EEE("-->DWC_ETH_QOS_phy_init_eee\n");

	/* According to 802.3az,the EEE is supported only in full duplex-mode.
	 * Also EEE feature is active when core is operating with MII, GMII,
	 * SGMII or RGMII.
	 */
	if ((phydev->duplex == DUPLEX_FULL) &&
	    ((phydev->interface == PHY_INTERFACE_MODE_MII) ||
	    (phydev->interface == PHY_INTERFACE_MODE_GMII) ||
	    (phydev->interface == PHY_INTERFACE_MODE_SGMII) ||
	    (phydev->interface == PHY_INTERFACE_MODE_RGMII))) {
		int eee_lp, eee_cap, eee_adv;
		/*u32 cap,lp , adv;*/
		int status;/*, idx;*/

		/* Read phy status to properly get the right settings */
		status = phy_read_status(phydev);
		if (status)
			return status;

		/* First check if the EEE ability is supported */
		eee_cap = DWC_ETH_QOS_phy_read_mmd_indirect(
			phydev->mdio.bus,
			CL45_PCS_EEE_ABLE, MDIO_MMD_PCS,phydev->mdio.addr);
		if (eee_cap < 0)
			return eee_cap;
		if (eee_cap == 0)
			return -1;

/*		cap = DWC_ETH_QOS_mmd_eee_cap_to_ethtool_sup_t(eee_cap);
 *		if (!cap)
 *			goto eee_exit;
 */
		/* check whether link Partner support EEE or not */
		eee_lp = DWC_ETH_QOS_phy_read_mmd_indirect(
			phydev->mdio.bus,
			CL45_AN_EEE_LPABLE_REG, MDIO_MMD_AN,phydev->mdio.addr);
		if (eee_lp < 0)
			return eee_lp;
		if (eee_lp == 0)
			return -1;

		eee_adv = DWC_ETH_QOS_phy_read_mmd_indirect(
			phydev->mdio.bus,
			CL45_ADV_EEE_REG, MDIO_MMD_AN,phydev->mdio.addr);
		if (eee_adv < 0)
			return eee_adv;
		if (eee_adv == 0)
			return -1;

		/* TODO:check this
		 * adv = DWC_ETH_QOS_mmd_eee_adv_to_ethtool_adv_t(eee_adv);
		 * lp = DWC_ETH_QOS_mmd_eee_adv_to_ethtool_adv_t(eee_lp);
		 * idx = phy_find_setting(phydev->speed, phydev->duplex);
		 * if ((lp & adv & settings[idx].setting))
				goto eee_exit;
		 */
		if (clk_stop_enable) {
			/* Configure the PHY to stop receiving xMII
			 * clock while it is signaling LPI.
			 */
			int val = DWC_ETH_QOS_phy_read_mmd_indirect(phydev->mdio.bus,
					CL45_CLK_STOP_EN_REG, MDIO_MMD_PCS,
					phydev->mdio.addr);
			if (val < 0)
				return val;

			val |= CL45_CLK_STOP_EN;
			DWC_ETH_QOS_phy_write_mmd_indirect(
				phydev->mdio.bus, CL45_CLK_STOP_EN_REG, MDIO_MMD_PCS,
				phydev->mdio.addr, val);
		}

		/* Disable smart EEE feature in AR8035*/
		if (phydev->phy_id == ATH8035_PHY_ID || phydev->phy_id == ATH8030_PHY_ID) {
			DWC_ETH_QOS_disable_smart_eee(phydev);
		}

		ret = 0; /* EEE supported */
	}

	DBGPR_EEE("<--DWC_ETH_QOS_phy_init_eee\n");

/*eee_exit:*/
	return ret;
}

/*!
* \brief API to initialize EEE mode.
*
* \details This function enables the LPI state and start the timer
* to verify whether the tx path can enter in LPI state if
* a. GMAC supports EEE mode &
* b. phy can also manage EEE.
*
* \param[in] pdata - pointer to private data structure
*
* \return bool
*
* \retval true on success & false on failure.
*/
bool DWC_ETH_QOS_eee_init(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct hw_if_struct *hw_if;
	bool ret = false;

	EMACDBG("Enter\n");

	hw_if = &pdata->hw_if;

	/* Disable smart EEE & EEE for ATH8030*/
	if ((pdata->emac_hw_version_type == EMAC_HW_v2_3_1)
		&& (pdata->io_macro_phy_intf == RMII_MODE) &&
		pdata->phydev->phy_id == ATH8030_PHY_ID) {
		//disable smart EEE
		DWC_ETH_QOS_disable_smart_eee(pdata->phydev);
		EMACDBG("disable smart EEE for 8030\n");
	}

	/* For RMII mode EEE is not supported */
	if (pdata->io_macro_phy_intf == RMII_MODE)
		goto phy_eee_failed;

	/* HW supports the EEE feature */
	if (pdata->hw_feat.eee_sel) {
#ifndef DWC_ETH_QOS_CUSTOMIZED_EEE_TEST
		/* check if the PHY supports EEE */
		if (!pdata->phydev || !pdata->phydev->link
			|| DWC_ETH_QOS_phy_init_eee(pdata->phydev, PHY_RX_CLOCK_STOPPABLE_DIS))
			goto phy_eee_failed;
#endif /* DWC_ETH_QOS_CUSTOMIZED_EEE_TEST */

		if (!pdata->eee_active) {
			pdata->eee_active = 1;
			if (pdata->use_lpi_auto_entry_timer) {
				pdata->hw_if.set_lpi_us_tic_counter(CLOCK_AHB_MHZ);
				hw_if->set_lpi_tx_auto_entry_timer(DWC_ETH_QOS_DEFAULT_LPI_LPIET_TIMER);
				hw_if->set_eee_timer(DWC_ETH_QOS_DEFAULT_LPI_LS_TIMER,
				DWC_ETH_QOS_DEFAULT_LPI_TWT_TIMER);
				hw_if->set_lpi_tx_automate();
				hw_if->set_lpi_tx_auto_entry_timer_en();
				hw_if->set_eee_mode();
				hw_if->set_eee_pls(pdata->phydev->link);
			} else {
				init_timer(&pdata->eee_ctrl_timer);
				pdata->eee_ctrl_timer.function =
					DWC_ETH_QOS_eee_ctrl_timer;
				pdata->eee_ctrl_timer.data = (unsigned long)pdata;
				pdata->eee_ctrl_timer.expires =
					DWC_ETH_QOS_LPI_TIMER(
						DWC_ETH_QOS_DEFAULT_LPI_TIMER);
				add_timer(&pdata->eee_ctrl_timer);

				hw_if->set_eee_timer(DWC_ETH_QOS_DEFAULT_LPI_LS_TIMER,
				DWC_ETH_QOS_DEFAULT_LPI_TWT_TIMER);

				if (pdata->use_lpi_tx_automate)
					hw_if->set_lpi_tx_automate();
			}
		} else {
			/* When EEE has been already initialized we have to
			 * modify the PLS bit in MAC_LPI_Control_Status reg
			 * according to PHY link status.
			 */
			hw_if->set_eee_pls(pdata->phydev->link);
		}

		DBGPR_EEE("EEE initialized\n");

		ret = true;
	}

	return ret;

#ifndef DWC_ETH_QOS_CUSTOMIZED_EEE_TEST
 phy_eee_failed:
	 /* In case of failure, reset the PHY link status in MAC_LPI_Control_Status reg */
	 hw_if->set_eee_pls(0);

	 /* Disable EEE mode */
	 if (pdata->eee_active) {
		 hw_if->reset_eee_mode();
		 pdata->eee_active = 0;
	 }
#endif

	EMACDBG("Exit\n");
	return ret;
}

#define MAC_LPS_TLPIEN 0x00000001
#define MAC_LPS_TLPIEX 0x00000002
#define MAC_LPS_RLPIEN 0x00000004
#define MAC_LPS_RLPIEX 0x00000008
void DWC_ETH_QOS_handle_eee_interrupt(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	u32 lpi_status;

	DBGPR_EEE("-->DWC_ETH_QOS_handle_eee_interrupt\n");

	lpi_status = hw_if->get_lpi_status();
	DBGPR_EEE("MAC_LPI_Control_Status = %#x\n", lpi_status);

	if (lpi_status & MAC_LPS_TLPIEN) {
		pdata->tx_path_in_lpi_mode = 1;
		pdata->xstats.tx_path_in_lpi_mode_irq_n++;
		DBGPR_EEE("MAC Transmitter has entered the LPI state\n");
	}

	if (lpi_status & MAC_LPS_TLPIEX) {
		pdata->tx_path_in_lpi_mode = 0;
		pdata->xstats.tx_path_exit_lpi_mode_irq_n++;
		DBGPR_EEE("MAC Transmitter has exited the LPI state\n");
	}

	if (lpi_status & MAC_LPS_RLPIEN) {
		pdata->xstats.rx_path_in_lpi_mode_irq_n++;
		DBGPR_EEE("MAC Receiver has entered the LPI state\n");
	}

	if (lpi_status & MAC_LPS_RLPIEX) {
		pdata->xstats.rx_path_exit_lpi_mode_irq_n++;
		DBGPR_EEE("MAC Receiver has exited the LPI state\n");
	}

	DBGPR_EEE("<--DWC_ETH_QOS_handle_eee_interrupt\n");
}
