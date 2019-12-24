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

/*!@file: DWC_ETH_QOS_mdio.c
 * @brief: Driver functions.
 */
#include "DWC_ETH_QOS_yheader.h"
#include "DWC_ETH_QOS_ipa.h"

/*!
 * \brief read MII PHY register, function called by the driver alone
 *
 * \details Read MII registers through the API read_phy_reg where the
 * related MAC registers can be configured.
 *
 * \param[in] pdata - pointer to driver private data structure.
 * \param[in] phyaddr - the phy address to read
 * \param[in] phyreg - the phy regiester id to read
 * \param[out] phydata -
 *		pointer to the value that is read from the phy registers
 *
 * \return int
 *
 * \retval  0 - successfully read data from register
 * \retval -1 - error occurred
 * \retval  1 - if the feature is not defined.
 */

INT DWC_ETH_QOS_mdio_read_direct(struct DWC_ETH_QOS_prv_data *pdata,
				 int phyaddr, int phyreg, int *phydata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int phy_reg_read_status;

	DBGPR_MDIO("--> DWC_ETH_QOS_mdio_read_direct\n");

	if (hw_if->read_phy_regs) {
		phy_reg_read_status =
		    hw_if->read_phy_regs(phyaddr, phyreg, phydata);
	} else {
		phy_reg_read_status = 1;
		pr_alert("%s: hw_if->read_phy_regs not defined", DEV_NAME);
	}

	DBGPR_MDIO("<-- DWC_ETH_QOS_mdio_read_direct\n");

	return phy_reg_read_status;
}

/*!
 * \brief write MII PHY register, function called by the driver alone
 *
 * \details Writes MII registers through the API write_phy_reg where the
 * related MAC registers can be configured.
 *
 * \param[in] pdata - pointer to driver private data structure.
 * \param[in] phyaddr - the phy address to write
 * \param[in] phyreg - the phy regiester id to write
 * \param[out] phydata - actual data to be written into the phy registers
 *
 * \return void
 *
 * \retval  0 - successfully read data from register
 * \retval -1 - error occurred
 * \retval  1 - if the feature is not defined.
 */

INT DWC_ETH_QOS_mdio_write_direct(struct DWC_ETH_QOS_prv_data *pdata,
				  int phyaddr, int phyreg, int phydata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int phy_reg_write_status;

	DBGPR_MDIO("--> DWC_ETH_QOS_mdio_write_direct\n");

	if (hw_if->write_phy_regs) {
		phy_reg_write_status =
		    hw_if->write_phy_regs(phyaddr, phyreg, phydata);
	} else {
		phy_reg_write_status = 1;
		pr_alert("%s: hw_if->write_phy_regs not defined", DEV_NAME);
	}

	DBGPR_MDIO("<-- DWC_ETH_QOS_mdio_write_direct\n");

	return phy_reg_write_status;
}

/*!
 * \brief write MII MMD register, function called by the driver alone
 *
 * \details Writes MII MMD registers through the API write_phy_reg where the
 * related MAC registers can be configured.
 *
 * \param[in] pdata - pointer to driver private data structure.
 * \param[in] phyaddr - the phy address to write
 * \param[in] phyreg - the phy regiester id to write
 * \param[out] phydata - actual data to be written into the phy registers
 *
 */
void DWC_ETH_QOS_mdio_mmd_register_write_direct(struct DWC_ETH_QOS_prv_data *pdata,
				 int phyaddr, int devaddr, int offset, u16 phydata)
{
	int write_data = 0;
	DWC_ETH_QOS_mdio_write_direct(pdata, pdata->phyaddr,
				DWC_ETH_QOS_MICREL_PHY_DEBUG_PORT_ADDR_OFFSET,
				devaddr);

	DWC_ETH_QOS_mdio_write_direct(pdata, pdata->phyaddr,
				DWC_ETH_QOS_MICREL_PHY_DEBUG_PORT_DATAPORT,
				offset);

	DWC_ETH_QOS_mdio_write_direct(pdata, pdata->phyaddr,
				DWC_ETH_QOS_MICREL_PHY_DEBUG_PORT_ADDR_OFFSET,
				(devaddr | 1 << 14));

	write_data = phydata;
	EMACDBG("Writing 0x%x to Dev address 0x%x ,  MMD register offset 0x%x",phydata,devaddr, offset);
	DWC_ETH_QOS_mdio_write_direct(pdata, pdata->phyaddr,
				DWC_ETH_QOS_MICREL_PHY_DEBUG_PORT_DATAPORT,
				write_data);

	return;
}
/*!
 * \brief Read MII MMD register, function called by the driver alone
 *
 * \details Reads MII MMD registers through the API write_phy_reg where the
 * related MAC registers can be configured.
 *
 * \param[in] pdata - pointer to driver private data structure.
 * \param[in] phyaddr - the phy address to write
 * \param[in] phyreg - the phy regiester id to write
 * \param[out] phydata - actual data to be written into the phy registers
 *
 */
void DWC_ETH_QOS_mdio_mmd_register_read_direct(struct DWC_ETH_QOS_prv_data *pdata,
				 int phyaddr, int devaddr, int offset, u16 *phydata)
{
	int read_data = 0;
	DWC_ETH_QOS_mdio_write_direct(pdata, pdata->phyaddr,
				DWC_ETH_QOS_MICREL_PHY_DEBUG_PORT_ADDR_OFFSET,
				devaddr);

	DWC_ETH_QOS_mdio_write_direct(pdata, pdata->phyaddr,
				DWC_ETH_QOS_MICREL_PHY_DEBUG_PORT_DATAPORT,
				offset);

	DWC_ETH_QOS_mdio_write_direct(pdata, pdata->phyaddr,
				DWC_ETH_QOS_MICREL_PHY_DEBUG_PORT_ADDR_OFFSET,
				(devaddr | 1 << 14));

	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr,
				DWC_ETH_QOS_MICREL_PHY_DEBUG_PORT_DATAPORT,
				&read_data);
	memcpy((void *)phydata,(void *)&read_data, sizeof(u16));

	//EMACDBG("Read 0x%x from Dev address 0x%x ,  MMD register offset 0x%x",*phydata,devaddr, offset);

	return;
}

/*!
 * \brief read MII PHY register.
 *
 * \details Read MII registers through the API read_phy_reg where the
 * related MAC registers can be configured.
 *
 * \param[in] bus - points to the mii_bus structure
 * \param[in] phyaddr - the phy address to write
 * \param[in] phyreg - the phy register offset to write
 *
 * \return int
 *
 * \retval  - value read from given phy register
 */

static INT DWC_ETH_QOS_mdio_read(struct mii_bus *bus, int phyaddr, int phyreg)
{
	struct net_device *dev = bus->priv;
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int phydata;

	DBGPR_MDIO("--> DWC_ETH_QOS_mdio_read: phyaddr = %d, phyreg = %d\n",
		   phyaddr, phyreg);

	if (hw_if->read_phy_regs)
		hw_if->read_phy_regs(phyaddr, phyreg, &phydata);
	else
		pr_alert("%s: hw_if->read_phy_regs not defined", DEV_NAME);

	DBGPR_MDIO("<-- DWC_ETH_QOS_mdio_read: phydata = %#x\n", phydata);

	return phydata;
}

/*!
 * \brief API to write MII PHY register
 *
 * \details This API is expected to write MII registers with the value being
 * passed as the last argument which is done in write_phy_regs API
 * called by this function.
 *
 * \param[in] bus - points to the mii_bus structure
 * \param[in] phyaddr - the phy address to write
 * \param[in] phyreg - the phy register offset to write
 * \param[in] phydata - the register value to write with
 *
 * \return 0 on success and -ve number on failure.
 */

static INT DWC_ETH_QOS_mdio_write(struct mii_bus *bus, int phyaddr, int phyreg,
				  u16 phydata)
{
	struct net_device *dev = bus->priv;
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	INT ret = Y_SUCCESS;

	DBGPR_MDIO("--> DWC_ETH_QOS_mdio_write\n");

	if (hw_if->write_phy_regs) {
		hw_if->write_phy_regs(phyaddr, phyreg, phydata);
	} else {
		ret = -1;
		pr_alert("%s: hw_if->write_phy_regs not defined", DEV_NAME);
	}

	DBGPR_MDIO("<-- DWC_ETH_QOS_mdio_write\n");

	return ret;
}

/*!
 * \brief API to reset PHY
 *
 * \details This API is issue soft reset to PHY core and waits
 * until soft reset completes.
 *
 * \param[in] bus - points to the mii_bus structure
 *
 * \return 0 on success and -ve number on failure.
 */

static INT DWC_ETH_QOS_mdio_reset(struct mii_bus *bus)
{
	struct net_device *dev = bus->priv;
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	INT phydata;

	DBGPR_MDIO("-->DWC_ETH_QOS_mdio_reset: phyaddr : %d\n", pdata->phyaddr);

	if (pdata->res_data->early_eth_en)
		return 0;

#if 0 /* def DWC_ETH_QOS_CONFIG_PGTEST */
	pr_alert("PHY Programming for Autoneg disable\n");
	hw_if->read_phy_regs(pdata->phyaddr, MII_BMCR, &phydata);
	phydata &= ~(1 << 12);
	hw_if->write_phy_regs(pdata->phyaddr, MII_BMCR, phydata);
#endif

	hw_if->read_phy_regs(pdata->phyaddr, MII_BMCR, &phydata);

	if (phydata < 0)
		return 0;

	/* issue soft reset to PHY */
	phydata |= BMCR_RESET;
	hw_if->write_phy_regs(pdata->phyaddr, MII_BMCR, phydata);

	/* wait until software reset completes */
	do {
		hw_if->read_phy_regs(pdata->phyaddr, MII_BMCR, &phydata);
	} while ((phydata >= 0) && (phydata & BMCR_RESET));

#if 0 /* def DWC_ETH_QOS_CONFIG_PGTEST */
	pr_alert("PHY Programming for Loopback\n");
	hw_if->read_phy_regs(pdata->phyaddr, MII_BMCR, &phydata);
	phydata |= (1 << 14);
	hw_if->write_phy_regs(pdata->phyaddr, MII_BMCR, phydata);
#endif

	DBGPR_MDIO("<--DWC_ETH_QOS_mdio_reset\n");

	return 0;
}

/*!
 * \details This function is invoked by other functions to get the PHY register
 * dump. This function is used during development phase for debug purpose.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return 0
 */

void dump_phy_registers(struct DWC_ETH_QOS_prv_data *pdata)
{
	int phydata = 0;

	pr_alert("\n************* PHY Reg dump *************************\n");
	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, MII_BMCR, &phydata);
	pr_alert(
	    "Phy Control Reg(Basic Mode Control Reg) (%#x) = %#x\n",
	     MII_BMCR, phydata);

	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, MII_BMSR, &phydata);
	pr_alert(
		"Phy Status Reg(Basic Mode Status Reg) (%#x) = %#x\n",
		MII_BMSR, phydata);

	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, MII_PHYSID1,
				     &phydata);
	pr_alert(
		"Phy Id (PHYS ID 1) (%#x)= %#x\n",
		MII_PHYSID1, phydata);

	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, MII_PHYSID2,
				     &phydata);
	pr_alert(
		"Phy Id (PHYS ID 2) (%#x)= %#x\n",
		MII_PHYSID2, phydata);

	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, MII_ADVERTISE,
				     &phydata);
	pr_alert(
		"Auto-nego Adv (Advertisement Control Reg)(%#x) = %#x\n",
		MII_ADVERTISE, phydata);

	/* read Phy Control Reg */
	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, MII_LPA,
				     &phydata);
	pr_alert(
		"Auto-nego Lap (Link Partner Ability Reg)(%#x)= %#x\n",
		MII_LPA, phydata);

	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, MII_EXPANSION,
				     &phydata);
	pr_alert(
		"Auto-nego Exp (Extension Reg)(%#x) = %#x\n",
		MII_EXPANSION, phydata);

	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr,
				     DWC_ETH_QOS_AUTO_NEGO_NP, &phydata);
	pr_alert(
		"Auto-nego Np (%#x) = %#x\n",
	    DWC_ETH_QOS_AUTO_NEGO_NP, phydata);

	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, MII_ESTATUS,
				     &phydata);
	pr_alert(
		"Extended Status Reg (%#x) = %#x\n", MII_ESTATUS, phydata);

	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, MII_CTRL1000,
				     &phydata);
	pr_alert(
		"1000 Ctl Reg (1000BASE-T Control Reg)(%#x) = %#x\n",
		MII_CTRL1000, phydata);

	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, MII_STAT1000,
				     &phydata);
	pr_alert(
		"1000 Sts Reg (1000BASE-T Status)(%#x) = %#x\n",
		MII_STAT1000, phydata);

	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, DWC_ETH_QOS_PHY_CTL,
				     &phydata);
	pr_alert(
		"PHY Ctl Reg (%#x) = %#x\n", DWC_ETH_QOS_PHY_CTL, phydata);

	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr,
				     DWC_ETH_QOS_PHY_STS, &phydata);
	pr_alert(
		"PHY Sts Reg (%#x) = %#x\n", DWC_ETH_QOS_PHY_STS, phydata);

	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr,
				     DWC_ETH_QOS_PHY_INTR_STATUS, &phydata);
	pr_alert(
		"PHY Intr Status Reg (%#x) = %#x\n", DWC_ETH_QOS_PHY_INTR_STATUS, phydata);

	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr,
					 DWC_ETH_QOS_PHY_INTR_EN, &phydata);
	pr_alert(
		"PHY Intr EN Reg (%#x) = %#x\n", DWC_ETH_QOS_PHY_INTR_EN, phydata);

	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, DWC_ETH_QOS_PHY_SMART_SPEED, &phydata);
	pr_alert( "Smart Speed Reg (%#x) = %#x\n", DWC_ETH_QOS_PHY_SMART_SPEED, phydata);

	if ((pdata->phydev->phy_id & pdata->phydev->drv->phy_id_mask) == MICREL_PHY_ID) {
		int i = 0;
		u16 mmd_phydata = 0;
		for(i=0;i<=8;i++){
			DWC_ETH_QOS_mdio_mmd_register_read_direct(pdata, pdata->phyaddr,
				DWC_ETH_QOS_MICREL_PHY_DEBUG_MMD_DEV_ADDR, i, &mmd_phydata);
			EMACDBG("Read %#x from offset %#x", mmd_phydata, i);
		}
	}

	pr_alert("\n****************************************************\n");
}

/*!
 * \brief API to enable or disable PHY hibernation mode
 *
 * \details Write to PHY debug registers at 0x0B bit[15]
 *
 * \param[in] pdata - pointer to platform data, mode
 * enable or disable values.
 *
 * \return void
 *
 * \retval none
 */
static void DWC_ETH_QOS_set_phy_hibernation_mode(struct DWC_ETH_QOS_prv_data *pdata,
								uint mode)
{
	u32 phydata = 0;
	EMACDBG("Enter\n");

	DWC_ETH_QOS_mdio_write_direct(pdata, pdata->phyaddr,
				DWC_ETH_QOS_PHY_DEBUG_PORT_ADDR_OFFSET,
				DWC_ETH_QOS_PHY_HIB_CTRL);
	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr,
				DWC_ETH_QOS_PHY_DEBUG_PORT_DATAPORT,
				&phydata);

	EMACDBG("value read 0x%x\n", phydata);

	phydata = ((phydata & DWC_ETH_QOS_PHY_HIB_CTRL_PS_HIB_EN_WR_MASK)
			   | ((DWC_ETH_QOS_PHY_HIB_CTRL_PS_HIB_EN_MASK & mode) << 15));
	DWC_ETH_QOS_mdio_write_direct(pdata, pdata->phyaddr,
				DWC_ETH_QOS_PHY_DEBUG_PORT_DATAPORT,
				phydata);

	DWC_ETH_QOS_mdio_write_direct(pdata, pdata->phyaddr,
				DWC_ETH_QOS_PHY_DEBUG_PORT_ADDR_OFFSET,
				DWC_ETH_QOS_PHY_HIB_CTRL);
	DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr,
				DWC_ETH_QOS_PHY_DEBUG_PORT_DATAPORT,
				&phydata);

	EMACDBG("Exit value written 0x%x\n", phydata);
}

/*!
 * \brief API to enable or disable RX/TX delay in PHY.
 *
 * \details Write to PHY debug registers at 0x0 and 0x5
 * offsets to enable and disable Rx/Tx delay
 *
 * \param[in] pdata - pointer to platform data, rx/tx delay
 * enable or disable values.
 *
 * \return void
 *
 * \retval none
 */

static void set_phy_rx_tx_delay(struct DWC_ETH_QOS_prv_data *pdata,
								uint rx_delay, uint tx_delay)
{
	EMACDBG("Enter\n");

	if ((pdata->phydev->phy_id & pdata->phydev->drv->phy_id_mask) == MICREL_PHY_ID) {
		u16 phydata = 0;
		u16 rx_clk = 0;
		if (pdata->emac_hw_version_type == EMAC_HW_v2_3_1) {
			if(!pdata->io_macro_tx_mode_non_id){
				EMACDBG("No PHY delay settings required for ID mode for "
						"EMAC core version 2.3.1 \n");
				return;
			}
			rx_clk = 22;
			/* RX_CLK to 0*/
			DWC_ETH_QOS_mdio_mmd_register_read_direct(pdata, pdata->phyaddr,
					DWC_ETH_QOS_MICREL_PHY_DEBUG_MMD_DEV_ADDR,0x8,&phydata);
			phydata &= ~(0x1F<<5);
			phydata |= (rx_clk << 5);
			DWC_ETH_QOS_mdio_mmd_register_write_direct(pdata, pdata->phyaddr,
						DWC_ETH_QOS_MICREL_PHY_DEBUG_MMD_DEV_ADDR,0x8,phydata);

			DWC_ETH_QOS_mdio_mmd_register_read_direct(pdata, pdata->phyaddr,
						DWC_ETH_QOS_MICREL_PHY_DEBUG_MMD_DEV_ADDR,0x8,&phydata);
			EMACDBG("Read 0x%x from offset 0x8\n",phydata);
		} else {
			rx_clk = 0x1F;
			/* RX_CLK to 0*/
			DWC_ETH_QOS_mdio_mmd_register_read_direct(pdata, pdata->phyaddr,
					DWC_ETH_QOS_MICREL_PHY_DEBUG_MMD_DEV_ADDR,0x8,&phydata);
			phydata &= ~(0x1F);
			phydata |= rx_clk;
			DWC_ETH_QOS_mdio_mmd_register_write_direct(pdata, pdata->phyaddr,
					DWC_ETH_QOS_MICREL_PHY_DEBUG_MMD_DEV_ADDR,0x8,phydata);

			DWC_ETH_QOS_mdio_mmd_register_read_direct(pdata, pdata->phyaddr,
					DWC_ETH_QOS_MICREL_PHY_DEBUG_MMD_DEV_ADDR,0x8,&phydata);
			EMACDBG("Read 0x%x from offset 0x8\n",phydata);
			phydata = 0;

		if (pdata->emac_hw_version_type == EMAC_HW_v2_1_2
			|| pdata->emac_hw_version_type == EMAC_HW_v2_1_1) {
			u16 tx_clk = 0xE;
			/* Provide TX_CLK  delay of -0.06nsec */
			DWC_ETH_QOS_mdio_mmd_register_read_direct(pdata, pdata->phyaddr,
						DWC_ETH_QOS_MICREL_PHY_DEBUG_MMD_DEV_ADDR, 0x8, &phydata);
			phydata |= (tx_clk << 5);
			DWC_ETH_QOS_mdio_mmd_register_write_direct(pdata, pdata->phyaddr,
						DWC_ETH_QOS_MICREL_PHY_DEBUG_MMD_DEV_ADDR, 0x8, phydata);

			DWC_ETH_QOS_mdio_mmd_register_read_direct(pdata, pdata->phyaddr,
						DWC_ETH_QOS_MICREL_PHY_DEBUG_MMD_DEV_ADDR, 0x8, &phydata);
			EMACDBG("Read 0x%x from offset 0x8\n",phydata);
			phydata = 0;
		}

		/*RXD0 = 15,RXD1 = 15,RXD2 = 0,RXD3 = 2*/
		DWC_ETH_QOS_mdio_mmd_register_read_direct(pdata, pdata->phyaddr,
					DWC_ETH_QOS_MICREL_PHY_DEBUG_MMD_DEV_ADDR,0x5,&phydata);
		phydata &= ~(0xFF);
		if (pdata->emac_hw_version_type == EMAC_HW_v2_1_2 ||
			pdata->emac_hw_version_type == EMAC_HW_v2_1_1)
			phydata |= ((0x2 << 12) | (0x2 << 8) | (0x2 << 4) | 0x2);
		else
			/* Default settings for EMAC_HW_v2_1_0 */
			phydata |= ((0x0 << 12) | (0x0 << 8) | (0x0 << 4) | 0x0);

			DWC_ETH_QOS_mdio_mmd_register_write_direct(pdata, pdata->phyaddr,
					DWC_ETH_QOS_MICREL_PHY_DEBUG_MMD_DEV_ADDR,0x5,phydata);
			DWC_ETH_QOS_mdio_mmd_register_read_direct(pdata, pdata->phyaddr,
					DWC_ETH_QOS_MICREL_PHY_DEBUG_MMD_DEV_ADDR,0x5,&phydata);
			EMACDBG("Read 0x%x from offset 0x5\n",phydata);
			phydata = 0;

			/*RX_CTL to 9*/
			DWC_ETH_QOS_mdio_mmd_register_read_direct(pdata, pdata->phyaddr,
					DWC_ETH_QOS_MICREL_PHY_DEBUG_MMD_DEV_ADDR,0x4,&phydata);
		phydata &= ~(0xF << 4);
		if (pdata->emac_hw_version_type == EMAC_HW_v2_1_2 ||
			pdata->emac_hw_version_type == EMAC_HW_v2_1_1)
			phydata |= (0x2 << 4);
		else
			/* Default settings for EMAC_HW_v2_1_0 */
			phydata |= (0x0 << 4);
		DWC_ETH_QOS_mdio_mmd_register_write_direct(pdata, pdata->phyaddr,
					DWC_ETH_QOS_MICREL_PHY_DEBUG_MMD_DEV_ADDR,0x4,phydata);
			DWC_ETH_QOS_mdio_mmd_register_read_direct(pdata, pdata->phyaddr,
					DWC_ETH_QOS_MICREL_PHY_DEBUG_MMD_DEV_ADDR,0x4,&phydata);
			EMACDBG("Read 0x%x from offset 0x4\n",phydata);
			phydata = 0;
		}
	} else {
		/* Default values are for PHY AR8035 */
		int phydata = 0;
		DWC_ETH_QOS_mdio_write_direct(pdata, pdata->phyaddr,
					DWC_ETH_QOS_PHY_DEBUG_PORT_ADDR_OFFSET,
					DWC_ETH_QOS_PHY_TX_DELAY);
		DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr,
					DWC_ETH_QOS_PHY_DEBUG_PORT_DATAPORT,
					&phydata);
		phydata = ((phydata & DWC_ETH_QOS_PHY_TX_DELAY_WR_MASK) |
					((tx_delay & DWC_ETH_QOS_PHY_TX_DELAY_MASK) << 8));
		DWC_ETH_QOS_mdio_write_direct(pdata, pdata->phyaddr,
					DWC_ETH_QOS_PHY_DEBUG_PORT_DATAPORT,
					phydata);
		EMACDBG("Setting TX delay %#x in PHY\n", phydata);
		DWC_ETH_QOS_mdio_write_direct(pdata, pdata->phyaddr,
					DWC_ETH_QOS_PHY_DEBUG_PORT_ADDR_OFFSET,
					DWC_ETH_QOS_PHY_RX_DELAY);
		DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr,
					DWC_ETH_QOS_PHY_DEBUG_PORT_DATAPORT,
					&phydata);
		phydata = ((phydata & DWC_ETH_QOS_PHY_RX_DELAY_WR_MASK) |
				((rx_delay & DWC_ETH_QOS_PHY_RX_DELAY_MASK) << 15));
		DWC_ETH_QOS_mdio_write_direct(pdata, pdata->phyaddr,
					DWC_ETH_QOS_PHY_DEBUG_PORT_DATAPORT,
					phydata);
		EMACDBG("Setting RX delay %#x in PHY\n", phydata);
	}

	EMACDBG("Exit\n");
}

/*!
 * \brief Determine whether or not to enable or disable
 * RX/TX delay in PHY.
 *
 * \details Determine whether or not to enable or disable
 * the RX/TX delay using the phy interface mode info and
 * speed.
 *
 * \param[in] pdata - pointer to platform data
 *
 * \return void
 *
 * \retval none
 */

static void configure_phy_rx_tx_delay(struct DWC_ETH_QOS_prv_data *pdata)
{
	EMACDBG("Enter\n");
	if ((pdata->emac_hw_version_type == EMAC_HW_v2_3_1)
		&& (pdata->io_macro_phy_intf == RMII_MODE)) {
		EMACDBG("phy rx tx delay setting not required for RMII mode for 2.3.1\n");
		return;
	}

	switch (pdata->speed) {
	case SPEED_1000:
		if (pdata->io_macro_tx_mode_non_id) {
			/* Settings for Non-ID mode */
			set_phy_rx_tx_delay(pdata, ENABLE_RX_DELAY, ENABLE_TX_DELAY);
		} else {
			/* Settings for RGMII ID mode.
			Not applicable for EMAC core version 2.1.0, 2.1.2 and 2.1.1 */
			if (pdata->emac_hw_version_type != EMAC_HW_v2_1_0 &&
				pdata->emac_hw_version_type != EMAC_HW_v2_1_2 &&
				pdata->emac_hw_version_type != EMAC_HW_v2_1_1)
				set_phy_rx_tx_delay(pdata, DISABLE_RX_DELAY, DISABLE_TX_DELAY);
		}
		break;

	case SPEED_100:
	case SPEED_10:
		if (pdata->emac_hw_version_type == EMAC_HW_v2_1_0 ||
			pdata->emac_hw_version_type == EMAC_HW_v2_1_2) {
			if (pdata->io_macro_tx_mode_non_id)
				set_phy_rx_tx_delay(pdata, DISABLE_RX_DELAY, ENABLE_TX_DELAY);
		} else {

			if (pdata->io_macro_tx_mode_non_id ||
				pdata->io_macro_phy_intf == MII_MODE) {
				/* Settings for Non-ID mode or MII mode */
				set_phy_rx_tx_delay(pdata, DISABLE_RX_DELAY, ENABLE_TX_DELAY);
			} else {
				/* Settings for RGMII ID mode */
				/* Not applicable for EMAC core version 2.1.0, 2.1.2 and 2.1.1 */
				if (pdata->emac_hw_version_type != EMAC_HW_v2_1_0 &&
					pdata->emac_hw_version_type != EMAC_HW_v2_1_2 &&
					pdata->emac_hw_version_type != EMAC_HW_v2_1_1)
					set_phy_rx_tx_delay(pdata, DISABLE_RX_DELAY, DISABLE_TX_DELAY);
			}
		}
		break;
	}
	EMACDBG("Exit\n");
}

/*!
 * \brief Function to set RGMII clock and enable bus
 * scaling.
 *
 * \details Function to set RGMII clock and enable bus
 * scaling.
 *
 * \param[in] pdata - pointer to platform data
 *
 * \return void
 *
 * \retval Y_SUCCESS on success and Y_FAILURE on failure.
 */
void DWC_ETH_QOS_set_clk_and_bus_config(struct DWC_ETH_QOS_prv_data *pdata, int speed)
{
	EMACDBG("Enter\n");

	switch (pdata->io_macro_phy_intf) {
	case RGMII_MODE:
		switch (speed) {

		case SPEED_1000:
			pdata->rgmii_clk_rate = RGMII_1000_NOM_CLK_FREQ;
			break;

		case SPEED_100:
			if (pdata->io_macro_tx_mode_non_id)
				pdata->rgmii_clk_rate = RGMII_NON_ID_MODE_100_LOW_SVS_CLK_FREQ;
			else
				pdata->rgmii_clk_rate = RGMII_ID_MODE_100_LOW_SVS_CLK_FREQ;
			break;

		case SPEED_10:
			if (pdata->io_macro_tx_mode_non_id)
				pdata->rgmii_clk_rate = RGMII_NON_ID_MODE_10_LOW_SVS_CLK_FREQ;
			else
				pdata->rgmii_clk_rate = RGMII_ID_MODE_10_LOW_SVS_CLK_FREQ;
			break;
		}
		break;

	case RMII_MODE:
		switch (speed) {
		case SPEED_100:
			pdata->rgmii_clk_rate = RMII_100_LOW_SVS_CLK_FREQ;
			break;

		case SPEED_10:
			pdata->rgmii_clk_rate = RMII_10_LOW_SVS_CLK_FREQ;
			break;
		}
		break;

	case MII_MODE:
		switch (speed) {
		case SPEED_100:
			pdata->rgmii_clk_rate = MII_100_LOW_SVS_CLK_FREQ;
			break;
		case SPEED_10:
			pdata->rgmii_clk_rate = MII_10_LOW_SVS_CLK_FREQ;
			break;
		}
		break;
	}

	switch (speed) {
		case SPEED_1000:
			pdata->vote_idx = VOTE_IDX_1000MBPS;
			break;
		case SPEED_100:
			pdata->vote_idx = VOTE_IDX_100MBPS;
			break;
		case SPEED_10:
			pdata->vote_idx = VOTE_IDX_10MBPS;
			break;
		case 0:
			pdata->vote_idx = VOTE_IDX_0MBPS;
			pdata->rgmii_clk_rate = 0;
			break;
	}

	if (pdata->bus_hdl) {
		if (msm_bus_scale_client_update_request(pdata->bus_hdl, pdata->vote_idx))
			WARN_ON(1);
	}

	if (pdata->res_data->rgmii_clk)
		clk_set_rate(pdata->res_data->rgmii_clk, pdata->rgmii_clk_rate);

	EMACDBG("Exit\n");

}
/*!
 * \brief Function to configure IO macro and DLL settings.
 *
 * \details Function to configure IO macro and DLL settings based
 * on speed and phy interface.
 *
 * \param[in] pdata - pointer to platform data
 *
 * \return void
 *
 * \retval Y_SUCCESS on success and Y_FAILURE on failure.
 */

static inline int DWC_ETH_QOS_configure_io_macro_dll_settings(
			struct DWC_ETH_QOS_prv_data *pdata)
{
	int ret = Y_SUCCESS;

	EMACDBG("Enter\n");

#ifndef DWC_ETH_QOS_EMULATION_PLATFORM
	DWC_ETH_QOS_rgmii_io_macro_dll_reset(pdata);

	/* For RGMII ID mode with internal delay*/
	if (pdata->io_macro_phy_intf == RGMII_MODE && !pdata->io_macro_tx_mode_non_id) {
		EMACDBG("Initialize and configure SDCC DLL\n");
		ret = DWC_ETH_QOS_rgmii_io_macro_sdcdc_init(pdata);
		if (ret < 0) {
			EMACERR("DLL init failed \n");
			return ret;
		}
		if (pdata->speed == SPEED_1000) {
			ret = DWC_ETH_QOS_rgmii_io_macro_sdcdc_config(pdata);
			if (ret < 0) {
				EMACERR("DLL config failed \n");
				return ret;
			}
		}
	} else {
		/* For RGMII Non ID (i.e external delay), RMII and MII modes set DLL bypass */
		DWC_ETH_QOS_sdcc_set_bypass_mode();
	}
#endif
	DWC_ETH_QOS_rgmii_io_macro_init(pdata);

	EMACDBG("Exit\n");
	return ret;
}


/*!
 * \brief Set link parameters for QCA8337.
 *
 * \details This function configures the MAC in 1000
 * Mbps full duplex mode.
 * \param[in] dev - pointer to net_device structure
 *
 * \return 0 on success
 */

static int DWC_ETH_QOS_config_qca_link(struct DWC_ETH_QOS_prv_data* pdata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int ret = Y_SUCCESS;

	EMACDBG("Enter\n");

	if (pdata->io_macro_phy_intf == RMII_MODE ||
			pdata->io_macro_phy_intf == MII_MODE) {
		hw_if->set_mii_speed_100();
		hw_if->set_full_duplex();
		pdata->vote_idx = VOTE_IDX_100MBPS;
		pdata->speed = SPEED_100;
	} else {
		/* Default setting is for RGMII 1000 Mbps full duplex mode*/
		hw_if->set_gmii_speed();
		hw_if->set_full_duplex();
		pdata->vote_idx = VOTE_IDX_1000MBPS;
		pdata->speed = SPEED_1000;
	}
	pdata->duplex = 1;
	EMACDBG("EMAC configured to speed = %d and full duplex = %d\n",
			pdata->speed,
			pdata->duplex);
	EMACDBG("pdata->interface = %d\n", pdata->interface);
	pdata->oldlink = 1;
	pdata->oldduplex = 1;

	/* Set RGMII clock and bus scale request based on link speed and phy mode */
	DWC_ETH_QOS_set_clk_and_bus_config(pdata, pdata->speed);

	ret = DWC_ETH_QOS_configure_io_macro_dll_settings(pdata);

	EMACDBG("Exit\n");
	return ret;
}

/*!
 * \brief API to adjust link parameters.
 *
 * \details This function will be called by PAL to inform the driver
 * about various link parameters like duplex and speed. This function
 * will configure the MAC based on link parameters.
 *
 * \param[in] dev - pointer to net_device structure
 *
 * \return void
 */

void DWC_ETH_QOS_adjust_link(struct net_device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct phy_device *phydev = pdata->phydev;
	//unsigned long flags;
	int new_state = 0;
	int ret = 0;

	if (!phydev)
		return;

	if (pdata->oldlink == -1 && !phydev->link) {
		pdata->oldlink = phydev->link;
		return;
	}

	DBGPR_MDIO(
		"-->DWC_ETH_QOS_adjust_link. address %d link %d\n",
		phydev->mdio.addr, phydev->link);

	//spin_lock_irqsave(&pdata->lock, flags);

	if (phydev->link) {
		/* Now we make sure that we can be in full duplex mode.
		 * If not, we operate in half-duplex mode
		 */
		if (phydev->duplex != pdata->oldduplex) {
			new_state = 1;
			if (phydev->duplex) {
				hw_if->set_full_duplex();
			} else {
				hw_if->set_half_duplex();
#ifdef DWC_ETH_QOS_CERTIFICATION_PKTBURSTCNT_HALFDUPLEX
				/* For Synopsys testing and debugging only */
				{
					UINT phydata;

					/* setting 'Assert CRS on transmit' */
					phydata = 0;
					DWC_ETH_QOS_mdio_read_direct(
						pdata, pdata->phyaddr,
						DWC_ETH_QOS_PHY_CTL, &phydata);
					phydata |= (1 << 11);
					DWC_ETH_QOS_mdio_write_direct(
						pdata, pdata->phyaddr,
						DWC_ETH_QOS_PHY_CTL, phydata);
				}
#endif
			}
			pdata->oldduplex = phydev->duplex;
		}

		/* FLOW ctrl operation */
		if (phydev->pause || phydev->asym_pause) {
			if (pdata->flow_ctrl != pdata->oldflow_ctrl)
				DWC_ETH_QOS_configure_flow_ctrl(pdata);
		}

		if (phydev->speed != pdata->speed) {
			new_state = 1;
			switch (phydev->speed) {
			case SPEED_1000:
				hw_if->set_gmii_speed();
				break;
			case SPEED_100:
				hw_if->set_mii_speed_100();
				break;
			case SPEED_10:
				hw_if->set_mii_speed_10();
				break;
			}
			pdata->speed = phydev->speed;

			EMACDBG("Bypass mode read from device tree = %d\n",
					pdata->io_macro_tx_mode_non_id);

			/* Set PHY delays here */
			configure_phy_rx_tx_delay(pdata);

			/* Set RGMII clock and bus scale request based on link speed and phy mode */
			if (pdata->io_macro_phy_intf != RMII_MODE) {
				DWC_ETH_QOS_set_clk_and_bus_config(pdata, pdata->speed);

				ret = DWC_ETH_QOS_configure_io_macro_dll_settings(pdata);
				if (ret < 0) {
					EMACERR("Failed to configure IO macro and DLL settings\n");
					return;
				}
			}
		}

		if (!pdata->oldlink || (pdata->oldlink == -1)) {
			new_state = 1;
			pdata->oldlink = 1;
			netif_carrier_on(dev);
		}
	} else if (pdata->oldlink) {
		netif_carrier_off(dev);
		new_state = 1;
		pdata->oldlink = 0;
		pdata->speed = 0;
		pdata->oldduplex = -1;
	}

	if (new_state) {
		phy_print_status(phydev);

#ifdef CONFIG_MSM_BOOT_TIME_MARKER
		if ((phydev->link == 1) && !pdata->print_kpi) {
			place_marker("M - Ethernet is Ready.Link is UP");
			pdata->print_kpi = 1;
		}
#endif

		if (pdata->ipa_enabled && netif_running(dev)) {
			if (phydev->link == 1)
				 DWC_ETH_QOS_ipa_offload_event_handler(pdata, EV_PHY_LINK_UP);
			else if (phydev->link == 0)
				DWC_ETH_QOS_ipa_offload_event_handler(pdata, EV_PHY_LINK_DOWN);
		}

		if (phydev->link == 1)
			pdata->hw_if.start_mac_tx_rx();
		else if (phydev->link == 0 && pdata->io_macro_phy_intf != RMII_MODE)
			DWC_ETH_QOS_set_clk_and_bus_config(pdata, SPEED_10);
	}

	/* At this stage, it could be need to setup the EEE or adjust some
	 * MAC related HW registers.
	 */
	pdata->eee_enabled = DWC_ETH_QOS_eee_init(pdata);

	//spin_unlock_irqrestore(&pdata->lock, flags);

	DBGPR_MDIO("<--DWC_ETH_QOS_adjust_link\n");
}

static void DWC_ETH_QOS_request_phy_wol(struct DWC_ETH_QOS_prv_data *pdata)
{
	pdata->phy_wol_supported = 0;
	pdata->phy_wol_wolopts = 0;

	/* Check if phydev is valid*/
	/* Check and enable Wake-on-LAN functionality in PHY*/
	if (pdata->phydev) {
		struct ethtool_wolinfo wol = {.cmd = ETHTOOL_GWOL};
		wol.supported = 0;
		wol.wolopts= 0;

		phy_ethtool_get_wol(pdata->phydev, &wol);
		pdata->phy_wol_supported = wol.supported;

		/* Try to enable supported Wake-on-LAN features in PHY*/
		if (wol.supported) {

			device_set_wakeup_capable(&pdata->pdev->dev, 1);

			wol.cmd = ETHTOOL_SWOL;
			wol.wolopts = wol.supported;

			if (!phy_ethtool_set_wol(pdata->phydev, &wol)){
				pdata->phy_wol_wolopts = wol.wolopts;

				enable_irq_wake(pdata->phy_irq);

				device_set_wakeup_enable(&pdata->pdev->dev, 1);
				EMACDBG("Enabled WoL[0x%x] in %s\n", wol.wolopts,
						 pdata->phydev->drv->name);
			}
		}
	}
}

bool DWC_ETH_QOS_is_phy_link_up(struct DWC_ETH_QOS_prv_data *pdata)
{
	/* PHY driver initializes phydev->link=1.
	 * So, phydev->link is 1 even on booup with no PHY connected.
	 * phydev->link is valid only after adjust_link is called once.
	 * Use (pdata->oldlink != -1) to indicate phy link is not up */
	return pdata->always_on_phy ? 1 :
		((pdata->oldlink != -1) && pdata->phydev && pdata->phydev->link);
}

/*!
 * \brief API to initialize PHY.
 *
 * \details This function will initializes the driver's PHY state and attaches
 * the PHY to the MAC driver.
 *
 * \param[in] dev - pointer to net_device structure
 *
 * \return integer
 *
 * \retval 0 on success & negative number on failure.
 */

static int DWC_ETH_QOS_init_phy(struct net_device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct phy_device *phydev = NULL;
	char phy_id_fmt[MII_BUS_ID_SIZE + 3];
	char bus_id[MII_BUS_ID_SIZE];
	u32 phydata = 0;
	int ret = 0;

	DBGPR_MDIO("-->DWC_ETH_QOS_init_phy\n");

	pdata->oldlink = -1;
	pdata->speed = 0;
	pdata->oldduplex = -1;

	snprintf(bus_id, MII_BUS_ID_SIZE, "dwc_phy-%x", pdata->bus_id);

	snprintf(phy_id_fmt, MII_BUS_ID_SIZE + 3, PHY_ID_FMT, bus_id,
		 pdata->phyaddr);

	DBGPR_MDIO("trying to attach to %s\n", phy_id_fmt);

	phydev = phy_connect(dev, phy_id_fmt, &DWC_ETH_QOS_adjust_link,
			     pdata->interface);

	if (IS_ERR(phydev)) {
		pr_alert("%s: Could not attach to PHY\n", dev->name);
		return PTR_ERR(phydev);
	}

	if (phydev->phy_id == 0) {
		phy_disconnect(phydev);
		return -ENODEV;
	}

#ifndef DWC_ETH_QOS_EMULATION_PLATFORM
	if ((pdata->enable_phy_intr && ((phydev->phy_id == ATH8031_PHY_ID)
		|| (phydev->phy_id == ATH8035_PHY_ID)
		|| ((phydev->phy_id & phydev->drv->phy_id_mask) == MICREL_PHY_ID)))) {
		pdata->phy_intr_en = true;
		EMACDBG("Phy interrupt enabled\n");
	} else
		EMACDBG("Phy polling enabled\n");
#endif


	if (pdata->interface == PHY_INTERFACE_MODE_GMII || pdata->interface == PHY_INTERFACE_MODE_RGMII) {
		phy_set_max_speed(phydev, SPEED_1000);
		/* Half duplex not supported */
		phydev->supported &= ~(SUPPORTED_10baseT_Half | SUPPORTED_100baseT_Half | SUPPORTED_1000baseT_Half);
	} else if ((pdata->interface == PHY_INTERFACE_MODE_MII) || (pdata->interface == PHY_INTERFACE_MODE_RMII)) {
		phy_set_max_speed(phydev, SPEED_100);
		/* Half duplex is not supported */
		phydev->supported &= ~(SUPPORTED_10baseT_Half | SUPPORTED_100baseT_Half);
	}
	phydev->advertising = phydev->supported;

	if (pdata->res_data->early_eth_en ) {
		phydev->autoneg = AUTONEG_DISABLE;
		phydev->speed = SPEED_100;
		phydev->duplex = DUPLEX_FULL;
		phydev->advertising = phydev->supported;
		phydev->advertising &= ~(SUPPORTED_1000baseT_Full);
		EMACDBG("Set max speed to SPEED_100 as early ethernet enabled\n");
	}

	pdata->phydev = phydev;

	/* Disable smart speed function for AR8035*/
	if (phydev->phy_id == ATH8035_PHY_ID) {
		DBGPR_MDIO("%s: attached to PHY (UID 0x%x) Link = %d\n", dev->name,
			   phydev->phy_id, phydev->link);

		DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, DWC_ETH_QOS_PHY_SMART_SPEED, &phydata);
		phydata &= ~(1<<5);

		DWC_ETH_QOS_mdio_write_direct(pdata, pdata->phyaddr, DWC_ETH_QOS_PHY_SMART_SPEED, phydata);

		DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, MII_BMCR, &phydata);

		phydata |= (1 << 15);

		DWC_ETH_QOS_mdio_write_direct(pdata, pdata->phyaddr, MII_BMCR, phydata);

		DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, DWC_ETH_QOS_PHY_SMART_SPEED, &phydata);
		DBGPR_MDIO( "Smart Speed Reg (%#x) = %#x\n", DWC_ETH_QOS_PHY_SMART_SPEED, phydata);

		DWC_ETH_QOS_set_phy_hibernation_mode(pdata, 0);
	}

	if (pdata->phy_intr_en) {

		INIT_WORK(&pdata->emac_phy_work, DWC_ETH_QOS_defer_phy_isr_work);
		init_completion(&pdata->clk_enable_done);

		ret = request_irq(pdata->phy_irq, DWC_ETH_QOS_PHY_ISR,
						IRQF_SHARED, DEV_NAME, pdata);
		if (ret) {
			pr_alert("Unable to register PHY IRQ %d\n", pdata->phy_irq);
			return ret;
		}

		phydev->irq = PHY_IGNORE_INTERRUPT;
		phydev->interrupts =  PHY_INTERRUPT_ENABLED;

		if (phydev->drv->config_intr &&
			!phydev->drv->config_intr(phydev)){
			DWC_ETH_QOS_request_phy_wol(pdata);
		} else {
			EMACERR("Failed to configure PHY interrupts");
			BUG();
		}
	}

	phy_start(pdata->phydev);

	DBGPR_MDIO("<--DWC_ETH_QOS_init_phy\n");

	return 0;
}

static bool DWC_ETH_QOS_phy_id_match(u32 phy_id)
{
	int i;
	EMACDBG("Enter\n");

	for (i = 0; i < ARRAY_SIZE(qca8337_phy_ids); i++) {
		if (phy_id == qca8337_phy_ids[i]) {
			pr_alert("qca8337: PHY_ID at %s: id:%08x\n", DEV_NAME, phy_id);
			return true;
		}
	}

	EMACDBG("Exit\n");
	return false;
}

/*!
 * \brief API to register mdio.
 *
 * \details This function will allocate mdio bus and register it
 * phy layer.
 *
 * \param[in] dev - pointer to net_device structure
 *
 * \return 0 on success and -ve on failure.
 */

int DWC_ETH_QOS_mdio_register(struct net_device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct mii_bus *new_bus = NULL;
	int phyaddr = 0;
	unsigned short phy_detected = 0;
	int ret = Y_SUCCESS;
	int phy_reg_read_status, mii_status;
	u32 phy_id, phy_id1, phy_id2;
	u32 phydata = 0;

	DBGPR_MDIO("-->DWC_ETH_QOS_mdio_register\n");

	/* find the phy ID or phy address which is connected to our MAC */
	for (phyaddr = 0; phyaddr < 32; phyaddr++) {

		phy_reg_read_status =
		    DWC_ETH_QOS_mdio_read_direct(pdata, phyaddr, MII_BMSR,
						 &mii_status);
		if (phy_reg_read_status == 0) {
			if (mii_status != 0x0000 && mii_status != 0xffff) {
				pr_alert
				("%s: Phy detected at ID/ADDR %d\n",
				DEV_NAME, phyaddr);
				phy_detected = 1;
				break;
			}
		} else if (phy_reg_read_status < 0) {
			pr_alert(
			"%s: Error reading the phy register MII_BMSR for phy ID/ADDR %d\n",
			    DEV_NAME, phyaddr);
		}
	}

	if (!phy_detected) {
		pr_alert("%s: No phy could be detected\n", DEV_NAME);
		return -ENOLINK;
	}

	pdata->phyaddr = phyaddr;
	pdata->bus_id = 0x1;
	pdata->phy_intr_en = false;
	pdata->always_on_phy = false;

	if(pdata->res_data->early_eth_en) {
		EMACDBG("Updated speed to 100 in emac\n");
		pdata->hw_if.set_mii_speed_100();

		phydata = BMCR_SPEED100;
		phydata |= BMCR_FULLDPLX;
		EMACDBG("Updated speed to 100 and autoneg disable\n");
		pdata->hw_if.write_phy_regs(pdata->phyaddr,
				MII_BMCR, phydata);
	}

	DBGPHY_REGS(pdata);

	new_bus = mdiobus_alloc();
	if (!new_bus) {
		pr_alert("Unable to allocate mdio bus\n");
		return -ENOMEM;
	}

	new_bus->name = "dwc_phy";
	new_bus->read = DWC_ETH_QOS_mdio_read;
	new_bus->write = DWC_ETH_QOS_mdio_write;
	new_bus->reset = DWC_ETH_QOS_mdio_reset;
	snprintf(new_bus->id, MII_BUS_ID_SIZE, "%s-%x", new_bus->name,
		 pdata->bus_id);
	new_bus->priv = dev;
	new_bus->phy_mask = 0;
	new_bus->parent = &pdata->pdev->dev;
	ret = mdiobus_register(new_bus);
	if (ret != 0) {
		pr_alert("%s: Cannot register as MDIO bus\n", new_bus->name);
		mdiobus_free(new_bus);
		return ret;
	}
	pdata->mii = new_bus;

	/* Check for QCA8337 phy chip id */
	phy_reg_read_status = DWC_ETH_QOS_mdio_read_direct(
	   pdata, phyaddr, MII_PHYSID1, &phy_id1);
	phy_reg_read_status = DWC_ETH_QOS_mdio_read_direct(
	   pdata, phyaddr, MII_PHYSID2, &phy_id2);
	if (phy_reg_read_status != 0) {
		EMACERR("unable to read phy id's: %d\n", phy_reg_read_status);
		goto err_out_phy_connect;
	}
	if (pdata->io_macro_phy_intf == RMII_MODE) {
		pdata->speed = SPEED_100; //Default speed
		DWC_ETH_QOS_set_clk_and_bus_config(pdata, pdata->speed);
		ret = DWC_ETH_QOS_configure_io_macro_dll_settings(pdata);
		if (ret < 0) {
			EMACERR("Failed to configure IO macro and DLL settings\n");
			goto err_out_phy_connect;
		}
	}
	phy_id = phy_id1 << 16;
	phy_id |= phy_id2;
	if (DWC_ETH_QOS_phy_id_match(phy_id) == true) {
		EMACDBG("QCA8337 detected\n");
		ret = DWC_ETH_QOS_config_qca_link(pdata);
		if (unlikely(ret)) {
			EMACERR("Failed to configure link in 1 Gbps/full duplex mode"
				" (error: %d)\n", ret);
			goto err_out_phy_connect;
		} else{
			pdata->always_on_phy = true;
			goto mdio_alloc_done;
		}
	}

	ret = DWC_ETH_QOS_init_phy(dev);
	if (unlikely(ret)) {
		pr_alert("Cannot attach to PHY (error: %d)\n", ret);
		goto err_out_phy_connect;
	}

 mdio_alloc_done:
	DBGPR_MDIO("<--DWC_ETH_QOS_mdio_register\n");

	return ret;

 err_out_phy_connect:
	DWC_ETH_QOS_mdio_unregister(dev);
	return ret;
}

/*!
 * \brief API to unregister mdio.
 *
 * \details This function will unregister mdio bus and free's the memory
 * allocated to it.
 *
 * \param[in] dev - pointer to net_device structure
 *
 * \return void
 */

void DWC_ETH_QOS_mdio_unregister(struct net_device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);

	DBGPR_MDIO("-->DWC_ETH_QOS_mdio_unregister\n");

	if (pdata->phydev) {
		phy_stop(pdata->phydev);
		phy_disconnect(pdata->phydev);
		pdata->phydev = NULL;
	}

	mdiobus_unregister(pdata->mii);
	pdata->mii->priv = NULL;
	mdiobus_free(pdata->mii);
	pdata->mii = NULL;

	DBGPR_MDIO("<--DWC_ETH_QOS_mdio_unregister\n");
}
