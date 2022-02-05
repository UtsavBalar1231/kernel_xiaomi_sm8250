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
 */

#ifndef __DWC_ETH_QOS__RGMII_IO_MACRO_REGACC__H__

#define __DWC_ETH_QOS__RGMII_IO_MACRO_REGACC__H__

/* Read Write register operations for EMAC_RGMII_IO_MACRO_CONFIG */

extern ULONG dwc_rgmii_io_csr_base_addr;
#define RGMII_IO_BASE_ADDRESS dwc_rgmii_io_csr_base_addr

#define RGMII_IO_MACRO_CONFIG_RGOFFADDR_OFFSET (0x00000000)
#define RGMII_IO_MACRO_CONFIG_RGOFFADDR ((volatile ULONG *) (RGMII_IO_BASE_ADDRESS + RGMII_IO_MACRO_CONFIG_RGOFFADDR_OFFSET))

#define RGMII_IO_MACRO_CONFIG_RGWR(data) do {\
		iowrite32(data, (void *)RGMII_IO_MACRO_CONFIG_RGOFFADDR);\
} while (0)

#define RGMII_IO_MACRO_CONFIG_RGRD(data) do {\
		(data) = ioread32((void *)RGMII_IO_MACRO_CONFIG_RGOFFADDR);\
} while (0)

#define RGMII_FUNC_CLK_EN_MASK (ULONG)(0x1)

#define RGMII_FUNC_CLK_EN_WR_MASK (ULONG)(0xbfffffff)

#define RGMII_FUNC_CLK_EN_UDFWR(data) do {\
		ULONG v;\
		RGMII_IO_MACRO_CONFIG_RGRD(v);\
		v = ((v & RGMII_FUNC_CLK_EN_WR_MASK) | ((data & RGMII_FUNC_CLK_EN_MASK) << 30));\
		RGMII_IO_MACRO_CONFIG_RGWR(v);\
} while (0)

#define RGMII_FUNC_CLK_EN_UDFRD(data) do {\
		RGMII_IO_MACRO_CONFIG_RGRD(data);\
		data = ((data >> 30) & RGMII_FUNC_CLK_EN_MASK);\
} while (0)

#define RGMII_GPIO_CFG_RX_INT_MASK (ULONG)(0x3)

#define RGMII_GPIO_CFG_RX_INT_WR_MASK (ULONG)(0xffe7ffff)

#define RGMII_GPIO_CFG_RX_INT_UDFWR(data) do {\
		ULONG v;\
		RGMII_IO_MACRO_CONFIG_RGRD(v);\
		v = ((v & RGMII_GPIO_CFG_RX_INT_WR_MASK) | ((data & RGMII_GPIO_CFG_RX_INT_MASK) << 19));\
		RGMII_IO_MACRO_CONFIG_RGWR(v);\
} while (0)

#define RGMII_GPIO_CFG_RX_INT_UDFRD(data) do {\
		RGMII_IO_MACRO_CONFIG_RGRD(data);\
		data = ((data >> 19) & RGMII_GPIO_CFG_RX_INT_MASK);\
} while (0)

#define RGMII_GPIO_CFG_TX_INT_MASK (ULONG)(0x3)

#define RGMII_GPIO_CFG_TX_INT_WR_MASK (ULONG)(0xfff9ffff)

#define RGMII_GPIO_CFG_TX_INT_UDFWR(data) do {\
		ULONG v;\
		RGMII_IO_MACRO_CONFIG_RGRD(v);\
		v = ((v & RGMII_GPIO_CFG_TX_INT_WR_MASK) | ((data & RGMII_GPIO_CFG_TX_INT_MASK) << 17));\
		RGMII_IO_MACRO_CONFIG_RGWR(v);\
} while (0)

#define RGMII_GPIO_CFG_TX_INT_UDFRD(data) do {\
		RGMII_IO_MACRO_CONFIG_RGRD(data);\
		data = ((data >> 17) & RGMII_GPIO_CFG_TX_INT_MASK);\
} while (0)

#define RGMII_MAX_SPD_PRG_9_MASK (ULONG)(0x1ff)

#define RGMII_MAX_SPD_PRG_9_WR_MASK (ULONG)(0xfffe00ff)

#define RGMII_MAX_SPD_PRG_9_UDFWR(data) do {\
		ULONG v;\
		RGMII_IO_MACRO_CONFIG_RGRD(v);\
		v = ((v & RGMII_MAX_SPD_PRG_9_WR_MASK) | ((data & RGMII_MAX_SPD_PRG_9_MASK) << 8));\
		RGMII_IO_MACRO_CONFIG_RGWR(v);\
} while (0)

#define RGMII_MAX_SPD_PRG_9_UDFRD(data) do {\
		RGMII_IO_MACRO_CONFIG_RGRD(data);\
		data = ((data >> 8) & RGMII_MAX_SPD_PRG_9_MASK);\
} while (0)

#define RGMII_MAX_SPD_PRG_2_MASK (ULONG)(0x3)

#define RGMII_MAX_SPD_PRG_2_WR_MASK (ULONG)(0xffffff3f)

#define RGMII_MAX_SPD_PRG_2_UDFWR(data) do {\
		ULONG v;\
		RGMII_IO_MACRO_CONFIG_RGRD(v);\
		v = ((v & RGMII_MAX_SPD_PRG_2_WR_MASK) | ((data & RGMII_MAX_SPD_PRG_2_MASK) << 6));\
		RGMII_IO_MACRO_CONFIG_RGWR(v);\
} while (0)

#define RGMII_MAX_SPD_PRG_2_UDFRD(data) do {\
		RGMII_IO_MACRO_CONFIG_RGRD(data);\
		data = ((data >> 6) & RGMII_MAX_SPD_PRG_2_MASK);\
} while (0)

#define RGMII_INTF_SEL_MASK (ULONG)(0x3)

#define RGMII_INTF_SEL_WR_MASK (ULONG)(0xffffffcf)

#define RGMII_INTF_SEL_UDFWR(data) do {\
		ULONG v;\
		RGMII_IO_MACRO_CONFIG_RGRD(v);\
		v = ((v & RGMII_INTF_SEL_WR_MASK) | ((data & RGMII_INTF_SEL_MASK) << 4));\
		RGMII_IO_MACRO_CONFIG_RGWR(v);\
} while (0)

#define RGMII_INTF_SEL_UDFRD(data) do {\
		RGMII_IO_MACRO_CONFIG_RGRD(data);\
		data = ((data >> 4) & RGMII_INTF_SEL_MASK);\
} while (0)

#define RGMII_POS_NEG_DATA_SEL_MASK (ULONG)(0x1)

#define RGMII_POS_NEG_DATA_SEL_WR_MASK (ULONG)(0xff7fffff)

#define RGMII_POS_NEG_DATA_SEL_UDFWR(data) do {\
		ULONG v;\
		RGMII_IO_MACRO_CONFIG_RGRD(v);\
		v = ((v & RGMII_POS_NEG_DATA_SEL_WR_MASK) | ((data & RGMII_POS_NEG_DATA_SEL_MASK) << 23));\
		RGMII_IO_MACRO_CONFIG_RGWR(v);\
} while (0)

#define RGMII_POS_NEG_DATA_SEL_UDFRD(data) do {\
		RGMII_IO_MACRO_CONFIG_RGRD(data);\
		data = ((data >> 23) & RGMII_POS_NEG_DATA_SEL_MASK);\
} while (0)

#define RGMII_BYPASS_TX_ID_EN_MASK (ULONG)(0x1)

#define RGMII_BYPASS_TX_ID_EN_WR_MASK (ULONG)(0xfffffff7)

#define RGMII_BYPASS_TX_ID_EN_UDFWR(data) do {\
		ULONG v;\
		RGMII_IO_MACRO_CONFIG_RGRD(v);\
		v = ((v & RGMII_BYPASS_TX_ID_EN_WR_MASK) | ((data & RGMII_BYPASS_TX_ID_EN_MASK) << 3));\
		RGMII_IO_MACRO_CONFIG_RGWR(v);\
} while (0)

#define RGMII_BYPASS_TX_ID_EN_UDFRD(data) do {\
		RGMII_IO_MACRO_CONFIG_RGRD(data);\
		data = ((data >> 3) & RGMII_BYPASS_TX_ID_EN_MASK);\
} while (0)

#define RGMII_LOOPBACK_EN_MASK (ULONG)(0x1)

#define RGMII_LOOPBACK_EN_WR_MASK (ULONG)(0xfffffffb)

#define RGMII_LOOPBACK_EN_UDFWR(data) do {\
		ULONG v;\
		RGMII_IO_MACRO_CONFIG_RGRD(v);\
		v = ((v & RGMII_LOOPBACK_EN_WR_MASK) | ((data & RGMII_LOOPBACK_EN_MASK) << 2));\
		RGMII_IO_MACRO_CONFIG_RGWR(v);\
} while (0)

#define RGMII_LOOPBACK_EN_UDFRD(data) do {\
		RGMII_IO_MACRO_CONFIG_RGRD(data);\
		data = ((data >> 2) & RGMII_LOOPBACK_EN_MASK);\
} while (0)

#define RGMII_PROG_SWAP_MASK (ULONG)(0x1)

#define RGMII_PROG_SWAP_WR_MASK (ULONG)(0xfffffffd)

#define RGMII_PROG_SWAP_UDFWR(data) do {\
		ULONG v;\
		RGMII_IO_MACRO_CONFIG_RGRD(v);\
		v = ((v & RGMII_PROG_SWAP_WR_MASK) | ((data & RGMII_PROG_SWAP_MASK) << 1));\
		RGMII_IO_MACRO_CONFIG_RGWR(v);\
} while (0)

#define RGMII_PROG_SWAP_UDFRD(data) do {\
		RGMII_IO_MACRO_CONFIG_RGRD(data);\
		data = ((data >> 1) & RGMII_PROG_SWAP_MASK);\
} while (0)

#define RGMII_DDR_MODE_MASK (ULONG)(0x1)

#define RGMII_DDR_MODE_WR_MASK (ULONG)(0xfffffffe)

#define RGMII_DDR_MODE_UDFWR(data) do {\
		ULONG v;\
		RGMII_IO_MACRO_CONFIG_RGRD(v);\
		v = ((v & RGMII_DDR_MODE_WR_MASK) | ((data & RGMII_DDR_MODE_MASK) << 0));\
		RGMII_IO_MACRO_CONFIG_RGWR(v);\
} while (0)

#define RGMII_DDR_MODE_UDFRD(data) do {\
		RGMII_IO_MACRO_CONFIG_RGRD(data);\
		data = ((data >> 0) & RGMII_DDR_MODE_MASK);\
} while (0)

/* Read Write register operations for EMAC_SDCC_HC_REG_DLL_CONFIG */

#define SDCC_HC_REG_DLL_CONFIG_RGOFFADDR_OFFSET (0x00000004)
#define SDCC_HC_REG_DLL_CONFIG_RGOFFADDR ((volatile ULONG *)(RGMII_IO_BASE_ADDRESS + SDCC_HC_REG_DLL_CONFIG_RGOFFADDR_OFFSET))

#define SDCC_HC_REG_DLL_CONFIG_RGWR(data) do {\
		iowrite32(data, (void *)SDCC_HC_REG_DLL_CONFIG_RGOFFADDR);\
} while (0)

#define SDCC_HC_REG_DLL_CONFIG_RGRD(data) do {\
		(data) = ioread32((void *)SDCC_HC_REG_DLL_CONFIG_RGOFFADDR);\
} while (0)

#define SDCC_HC_DLL_RST_MASK (ULONG)(0x1)

#define SDCC_HC_DLL_RST_WR_MASK (ULONG)(0xbfffffff)

#define SDCC_HC_DLL_RST_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DLL_CONFIG_RGRD(v);\
		v = ((v & SDCC_HC_DLL_RST_WR_MASK) | ((data & SDCC_HC_DLL_RST_MASK) << 30));\
		SDCC_HC_REG_DLL_CONFIG_RGWR(v);\
} while (0)

#define SDCC_HC_DLL_RST_UDFRD(data) do {\
		SDCC_HC_REG_DLL_CONFIG_RGRD(data);\
		data = ((data >> 30) & SDCC_HC_DLL_RST_MASK);\
} while (0)

#define SDCC_HC_PDN_MASK (ULONG)(0x1)

#define SDCC_HC_PDN_WR_MASK (ULONG)(0xdfffffff)

#define SDCC_HC_PDN_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DLL_CONFIG_RGRD(v);\
		v = ((v & SDCC_HC_PDN_WR_MASK) | ((data & SDCC_HC_PDN_MASK) << 29));\
		SDCC_HC_REG_DLL_CONFIG_RGWR(v);\
} while (0)

#define SDCC_HC_PDN_UDFRD(data) do {\
		SDCC_HC_REG_DLL_CONFIG_RGRD(data);\
		data = ((data >> 29) & SDCC_HC_PDN_MASK);\
} while (0)

#define SDCC_HC_MCLK_FREQ_MASK (ULONG)(0x7)

#define SDCC_HC_MCLK_FREQ_WR_MASK (ULONG)(0xf8ffffff)

#define SDCC_HC_MCLK_FREQ_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DLL_CONFIG_RGRD(v);\
		v = ((v & SDCC_HC_MCLK_FREQ_WR_MASK) | ((data & SDCC_HC_MCLK_FREQ_MASK) << 24));\
		SDCC_HC_REG_DLL_CONFIG_RGWR(v);\
} while (0)

#define SDCC_HC_MCLK_FREQ_UDFRD(data) do {\
		SDCC_HC_REG_DLL_CONFIG_RGRD(data);\
		data = ((data >> 24) & SDCC_HC_MCLK_FREQ_MASK);\
} while (0)

#define SDCC_HC_CDR_SELEXT_MASK (ULONG)(0xf)

#define SDCC_HC_CDR_SELEXT_WR_MASK (ULONG)(0xff0fffff)

#define SDCC_HC_CDR_SELEXT_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DLL_CONFIG_RGRD(v);\
		v = ((v & SDCC_HC_CDR_SELEXT_WR_MASK) | ((data & SDCC_HC_CDR_SELEXT_MASK) << 20));\
		SDCC_HC_REG_DLL_CONFIG_RGWR(v);\
} while (0)

#define SDCC_HC_CDR_SELEXT_UDFRD(data) do {\
		SDCC_HC_REG_DLL_CONFIG_RGRD(data);\
		data = ((data >> 20) & SDCC_HC_CDR_SELEXT_MASK);\
} while (0)

#define SDCC_HC_CDR_EXT_EN_MASK (ULONG)(0x1)

#define SDCC_HC_CDR_EXT_EN_WR_MASK (ULONG)(0xfff7ffff)

#define SDCC_HC_CDR_EXT_EN_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DLL_CONFIG_RGRD(v);\
		v = ((v & SDCC_HC_CDR_EXT_EN_WR_MASK) | ((data & SDCC_HC_CDR_EXT_EN_MASK) << 19));\
		SDCC_HC_REG_DLL_CONFIG_RGWR(v);\
} while (0)

#define SDCC_HC_CDR_EXT_EN_UDFRD(data) do {\
		SDCC_HC_REG_DLL_CONFIG_RGRD(data);\
		data = ((data >> 19) & SDCC_HC_CDR_EXT_EN_MASK);\
} while (0)

#define SDCC_HC_CK_OUT_EN_MASK (ULONG)(0x1)

#define SDCC_HC_CK_OUT_EN_WR_MASK (ULONG)(0xfffbffff)

#define SDCC_HC_CK_OUT_EN_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DLL_CONFIG_RGRD(v);\
		v = ((v & SDCC_HC_CK_OUT_EN_WR_MASK) | ((data & SDCC_HC_CK_OUT_EN_MASK) << 18));\
		SDCC_HC_REG_DLL_CONFIG_RGWR(v);\
} while (0)

#define SDCC_HC_CK_OUT_EN_UDFRD(data) do {\
		SDCC_HC_REG_DLL_CONFIG_RGRD(data);\
		data = ((data >> 18) & SDCC_HC_CK_OUT_EN_MASK);\
} while (0)

#define SDCC_HC_CDR_EN_MASK (ULONG)(0x1)

#define SDCC_HC_CDR_EN_WR_MASK (ULONG)(0xfffdffff)

#define SDCC_HC_CDR_EN_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DLL_CONFIG_RGRD(v);\
		v = ((v & SDCC_HC_CDR_EN_WR_MASK) | ((data & SDCC_HC_CDR_EN_MASK) << 17));\
		SDCC_HC_REG_DLL_CONFIG_RGWR(v);\
} while (0)

#define SDCC_HC_CDR_EN_UDFRD(data) do {\
		SDCC_HC_REG_DLL_CONFIG_RGRD(data);\
		data = ((data >> 17) & SDCC_HC_CDR_EN_MASK);\
} while (0)

#define SDCC_HC_MCLK_GATING_ENABLE_MASK (ULONG)(0x1)

#define SDCC_HC_MCLK_GATING_ENABLE_WR_MASK (ULONG)(0xffffffdf)

#define SDCC_HC_MCLK_GATING_ENABLE_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DLL_CONFIG_RGRD(v);\
		v = ((v & SDCC_HC_MCLK_GATING_ENABLE_WR_MASK) | ((data & SDCC_HC_MCLK_GATING_ENABLE_MASK) << 5));\
		SDCC_HC_REG_DLL_CONFIG_RGWR(v);\
} while (0)

#define SDCC_HC_MCLK_GATING_ENABLE_UDFRD(data) do {\
		SDCC_HC_REG_DLL_CONFIG_RGRD(data);\
		data = ((data >> 5) & SDCC_HC_MCLK_GATING_ENABLE_MASK);\
} while (0)

#define SDCC_HC_CDR_FINE_PHASE_MASK (ULONG)(0x1)

#define SDCC_HC_CDR_FINE_PHASE_WR_MASK (ULONG)(0xfffffff3)

#define SDCC_HC_CDR_FINE_PHASE_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DLL_CONFIG_RGRD(v);\
		v = ((v & SDCC_HC_CDR_FINE_PHASE_WR_MASK) | ((data & SDCC_HC_CDR_FINE_PHASE_MASK) << 2));\
		SDCC_HC_REG_DLL_CONFIG_RGWR(v);\
} while (0)

#define SDCC_HC_CDR_FINE_PHASE_UDFRD(data) do {\
		SDCC_HC_REG_DLL_CONFIG_RGRD(data);\
		data = ((data >> 2) & SDCC_HC_CDR_FINE_PHASE_MASK);\
} while (0)

#define SDCC_HC_DLL_EN_MASK (ULONG)(0x1)

#define SDCC_HC_DLL_EN_WR_MASK (ULONG)(0xfffeffff)

#define SDCC_HC_DLL_EN_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DLL_CONFIG_RGRD(v);\
		v = ((v & SDCC_HC_DLL_EN_WR_MASK) | ((data & SDCC_HC_DLL_EN_MASK) << 16));\
		SDCC_HC_REG_DLL_CONFIG_RGWR(v);\
} while (0)

#define SDCC_HC_DLL_EN_UDFRD(data) do {\
		SDCC_HC_REG_DLL_CONFIG_RGRD(data);\
		data = ((data >> 16) & SDCC_HC_DLL_EN_MASK);\
} while (0)

/* Read Write register operations for EMAC_SDCC_HC_REG_DDR_CONFIG */
#define SDCC_HC_REG_DDR_CONFIG_RGOFFADDR_OFFSET (0x0000000C)
#define SDCC_HC_REG_DDR_CONFIG_RGOFFADDR ((volatile ULONG *)(RGMII_IO_BASE_ADDRESS + SDCC_HC_REG_DDR_CONFIG_RGOFFADDR_OFFSET))

#define SDCC_HC_REG_DDR_CONFIG_RGWR(data) do {\
		iowrite32(data, (void *)SDCC_HC_REG_DDR_CONFIG_RGOFFADDR);\
} while (0)

#define SDCC_HC_REG_DDR_CONFIG_RGRD(data) do {\
		(data) = ioread32((void *)SDCC_HC_REG_DDR_CONFIG_RGOFFADDR);\
} while (0)

#define SDCC_HC_PRG_RCLK_DLY_MASK (ULONG)(0x1ff)

#define SDCC_HC_PRG_RCLK_DLY_WR_MASK (ULONG)(0xfffffe00)

#define SDCC_HC_PRG_RCLK_DLY_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DDR_CONFIG_RGRD(v);\
		v = ((v & SDCC_HC_PRG_RCLK_DLY_WR_MASK) | ((data & SDCC_HC_PRG_RCLK_DLY_MASK) << 0));\
		SDCC_HC_REG_DDR_CONFIG_RGWR(v);\
} while (0)

#define SDCC_HC_PRG_RCLK_DLY_UDFRD(data) do {\
		SDCC_HC_REG_DDR_CONFIG_RGRD(data);\
		data = ((data >> 0) & SDCC_HC_PRG_RCLK_DLY_MASK);\
} while (0)

#define SDCC_HC_EXT_PRG_RCLK_DLY_MASK (ULONG)(0x3f)

#define SDCC_HC_EXT_PRG_RCLK_DLY_WR_MASK (ULONG)(0xf81fffff)

#define SDCC_HC_EXT_PRG_RCLK_DLY_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DDR_CONFIG_RGRD(v);\
		v = ((v & SDCC_HC_EXT_PRG_RCLK_DLY_WR_MASK) | ((data & SDCC_HC_EXT_PRG_RCLK_DLY_MASK) << 21));\
		SDCC_HC_REG_DDR_CONFIG_RGWR(v);\
} while (0)

#define SDCC_HC_EXT_PRG_RCLK_DLY_UDFRD(data) do {\
		SDCC_HC_REG_DDR_CONFIG_RGRD(data);\
		data = ((data >> 0) & SDCC_HC_EXT_PRG_RCLK_DLY_MASK);\
} while (0)

#define SDCC_HC_EXT_PRG_RCLK_DLY_CODE_MASK (ULONG)(0x7)

#define SDCC_HC_EXT_PRG_RCLK_DLY_CODE_WR_MASK (ULONG)(0xc7ffffff)

#define SDCC_HC_EXT_PRG_RCLK_DLY_CODE_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DDR_CONFIG_RGRD(v);\
		v = ((v & SDCC_HC_EXT_PRG_RCLK_DLY_CODE_WR_MASK) | ((data & SDCC_HC_EXT_PRG_RCLK_DLY_CODE_MASK) << 27));\
		SDCC_HC_REG_DDR_CONFIG_RGWR(v);\
} while (0)

#define SDCC_HC_EXT_PRG_RCLK_DLY_CODE_UDFRD(data) do {\
		SDCC_HC_REG_DDR_CONFIG_RGRD(data);\
		data = ((data >> 0) & SDCC_HC_EXT_PRG_RCLK_DLY_CODE_MASK);\
} while (0)

#define SDCC_HC_EXT_PRG_RCLK_DLY_EN_MASK (ULONG)(0x1)

#define SDCC_HC_EXT_PRG_RCLK_DLY_EN_WR_MASK (ULONG)(0xbfffffff)

#define SDCC_HC_EXT_PRG_RCLK_DLY_EN_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DDR_CONFIG_RGRD(v);\
		v = ((v & SDCC_HC_EXT_PRG_RCLK_DLY_EN_WR_MASK) | ((data & SDCC_HC_EXT_PRG_RCLK_DLY_EN_MASK) << 30));\
		SDCC_HC_REG_DDR_CONFIG_RGWR(v);\
} while (0)

#define SDCC_HC_EXT_PRG_RCLK_DLY_EN_UDFRD(data) do {\
		SDCC_HC_REG_DDR_CONFIG_RGRD(data);\
		data = ((data >> 0) & SDCC_HC_EXT_PRG_RCLK_DLY_EN_MASK);\
} while (0)

/* Read Write register operations for EMAC_SDCC_HC_REG_DLL_CONFIG_2 */

#define SDCC_HC_REG_DLL_CONFIG_2_RGOFFADDR_OFFSET (0x00000010)
#define SDCC_HC_REG_DLL_CONFIG_2_RGOFFADDR ((volatile ULONG *)(RGMII_IO_BASE_ADDRESS + SDCC_HC_REG_DLL_CONFIG_2_RGOFFADDR_OFFSET))

#define SDCC_HC_REG_DLL_CONFIG_2_RGWR(data) do {\
		iowrite32(data, (void *)SDCC_HC_REG_DLL_CONFIG_2_RGOFFADDR);\
} while (0)

#define SDCC_HC_REG_DLL_CONFIG_2_RGRD(data) do {\
		(data) = ioread32((void *)SDCC_HC_REG_DLL_CONFIG_2_RGOFFADDR);\
} while (0)

#define SDCC_HC_CFG_2_MCLK_FREQ_CALC_MASK (ULONG)(0xff)

#define SDCC_HC_CFG_2_MCLK_FREQ_CALC_WR_MASK (ULONG)(0xfffc03ff)

#define SDCC_HC_CFG_2_MCLK_FREQ_CALC_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DLL_CONFIG_2_RGRD(v);\
		v = ((v & SDCC_HC_CFG_2_MCLK_FREQ_CALC_WR_MASK) | ((data & SDCC_HC_CFG_2_MCLK_FREQ_CALC_MASK) << 10));\
		SDCC_HC_REG_DLL_CONFIG_2_RGWR(v);\
} while (0)

#define SDCC_HC_CFG_2_MCLK_FREQ_CALC_UDFRD(data) do {\
		SDCC_HC_REG_DLL_CONFIG_2_RGRD(data);\
		data = ((data >> 10) & SDCC_HC_CFG_2_MCLK_FREQ_CALC_MASK);\
} while (0)

#define SDCC_HC_CFG_2_DLL_CLOCK_DISABLE_MASK (ULONG)(0x1)

#define SDCC_HC_CFG_2_DLL_CLOCK_DISABLE_WR_MASK (ULONG)(0xffdfffff)

#define SDCC_HC_CFG_2_DLL_CLOCK_DISABLE_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DLL_CONFIG_2_RGRD(v);\
		v = ((v & SDCC_HC_CFG_2_DLL_CLOCK_DISABLE_WR_MASK) | ((data & SDCC_HC_CFG_2_DLL_CLOCK_DISABLE_MASK) << 21));\
		SDCC_HC_REG_DLL_CONFIG_2_RGWR(v);\
} while (0)

#define SDCC_HC_CFG_2_DLL_CLOCK_DISABLE_UDFRD(data) do {\
		SDCC_HC_REG_DLL_CONFIG_2_RGRD(data);\
		data = ((data >> 21) & SDCC_HC_CFG_2_DLL_CLOCK_DISABLE_MASK);\
} while (0)

#define SDCC_HC_CFG_2_DDR_TRAFFIC_INIT_SEL_MASK (ULONG)(0x1)

#define SDCC_HC_CFG_2_DDR_TRAFFIC_INIT_SEL_WR_MASK (ULONG)(0xfffffff3)

#define SDCC_HC_CFG_2_DDR_TRAFFIC_INIT_SEL_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DLL_CONFIG_2_RGRD(v);\
		v = ((v & SDCC_HC_CFG_2_DDR_TRAFFIC_INIT_SEL_WR_MASK) | ((data & SDCC_HC_CFG_2_DDR_TRAFFIC_INIT_SEL_MASK) << 2));\
		SDCC_HC_REG_DLL_CONFIG_2_RGWR(v);\
} while (0)

#define SDCC_HC_CFG_2_DDR_TRAFFIC_INIT_SEL_UDFRD(data) do {\
		SDCC_HC_REG_DLL_CONFIG_2_RGRD(data);\
		data = ((data >> 2) & SDCC_HC_CFG_2_DDR_TRAFFIC_INIT_SEL_MASK);\
} while (0)


#define SDCC_HC_CFG_2_DDR_CAL_EN_MASK (ULONG)(0x1)

#define SDCC_HC_CFG_2_DDR_CAL_EN_WR_MASK (ULONG)(0xfffffffe)

#define SDCC_HC_CFG_2_DDR_CAL_EN_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DLL_CONFIG_2_RGRD(v);\
		v = ((v & SDCC_HC_CFG_2_DDR_CAL_EN_WR_MASK) | ((data & SDCC_HC_CFG_2_DDR_CAL_EN_MASK) << 0));\
		SDCC_HC_REG_DLL_CONFIG_2_RGWR(v);\
} while (0)

#define SDCC_HC_CFG_2_DDR_CAL_EN_UDFRD(data) do {\
		SDCC_HC_REG_DLL_CONFIG_2_RGRD(data);\
		data = ((data >> 0) & SDCC_HC_CFG_2_DDR_CAL_EN_MASK);\
} while (0)

#define SDCC_HC_CFG_2_DDR_TRAFFIC_INIT_SW_MASK (ULONG)(0x1)

#define SDCC_HC_CFG_2_DDR_TRAFFIC_INIT_SW_WR_MASK (ULONG)(0xfffffffd)

#define SDCC_HC_CFG_2_DDR_TRAFFIC_INIT_SW_UDFWR(data) do {\
		ULONG v;\
		SDCC_HC_REG_DLL_CONFIG_2_RGRD(v);\
		v = ((v & SDCC_HC_CFG_2_DDR_TRAFFIC_INIT_SW_WR_MASK) | ((data & SDCC_HC_CFG_2_DDR_TRAFFIC_INIT_SW_MASK) << 1));\
		SDCC_HC_REG_DLL_CONFIG_2_RGWR(v);\
} while (0)

#define SDCC_HC_CFG_2_DDR_TRAFFIC_INIT_SW_UDFRD(data) do {\
		SDCC_HC_REG_DLL_CONFIG_2_RGRD(data);\
		data = ((data >> 0) & SDCC_HC_CFG_2_DDR_TRAFFIC_INIT_SW_MASK);\
} while (0)

/* Read register operations for EMAC_SDC4_STATUS */

#define SDC4_STATUS_RGOFFADDR ((volatile ULONG *)(RGMII_IO_BASE_ADDRESS + 0x00000014))

#define SDC4_STATUS_RGRD(data) do {\
		(data) = ioread32((void *)SDC4_STATUS_RGOFFADDR);\
} while (0)

#define SDC4_STATUS_DLL_LOCK_STS_MASK (ULONG)(0x1)

#define SDC4_STATUS_DLL_LOCK_STS_UDFRD(data) do {\
		SDC4_STATUS_RGRD(data);\
		data = ((data >> 7) & SDC4_STATUS_DLL_LOCK_STS_MASK);\
} while (0)

/* Read register operations for EMAC_SDCC_USR_CTL */

#define SDCC_USR_CTL_RGOFFADDR_OFFSET (0x00000018)
#define SDCC_USR_CTL_RGOFFADDR ((volatile ULONG *)(RGMII_IO_BASE_ADDRESS + SDCC_USR_CTL_RGOFFADDR_OFFSET))

#define SDCC_USR_CTL_BYPASS_MODE_MASK (ULONG)(0x1)

#define SDCC_USR_CTL_BYPASS_MODE_WR_MASK (ULONG)(0xbfffffff)

#define SDCC_USR_CTL_RGRD(data) do {\
		(data) = ioread32((void *)SDCC_USR_CTL_RGOFFADDR);\
} while (0)

#define SDCC_USR_CTL_RGWR(data) do {\
		iowrite32(data, (void *)SDCC_USR_CTL_RGOFFADDR);\
} while (0)

#define SDCC_USR_CTL_BYPASS_MODE_UDFRD(data) do {\
		SDCC_USR_CTL_RGRD(data);\
		data = ((data >> 30) & SDCC_USR_CTL_BYPASS_MODE_MASK);\
} while (0)

#define SDCC_USR_CTL_BYPASS_MODE_UDFWR(data) do {\
		ULONG v;\
		SDCC_USR_CTL_RGRD(v);\
		v = ((v & SDCC_USR_CTL_BYPASS_MODE_WR_MASK) | ((data & SDCC_USR_CTL_BYPASS_MODE_MASK) << 30));\
		SDCC_USR_CTL_RGWR(v);\
} while (0)

/* Read Write register operations for
 * EMAC_RGMII_IO_MACRO_CONFIG_2
 */

#define RGMII_IO_MACRO_CONFIG_2_RGOFFADDR_OFFSET (0x0000001C)
#define RGMII_IO_MACRO_CONFIG_2_RGOFFADDR ((volatile ULONG *)(RGMII_IO_BASE_ADDRESS + RGMII_IO_MACRO_CONFIG_2_RGOFFADDR_OFFSET))

#define RGMII_IO_MACRO_CONFIG_2_RGWR(data) do {\
		iowrite32(data, (void *)RGMII_IO_MACRO_CONFIG_2_RGOFFADDR);\
} while (0)

#define RGMII_IO_MACRO_CONFIG_2_RGRD(data) do {\
		(data) = ioread32((void *)RGMII_IO_MACRO_CONFIG_2_RGOFFADDR);\
} while (0)

#define RGMII_DATA_DIVIDE_CLK_SEL_MASK (ULONG)(0x1)

#define RGMII_DATA_DIVIDE_CLK_SEL_WR_MASK (ULONG)(0xffffffbf)

#define RGMII_CONFIG_2_DATA_DIVIDE_CLK_SEL_UDFWR(data) do {\
		ULONG v;\
		RGMII_IO_MACRO_CONFIG_2_RGRD(v);\
		v = ((v & RGMII_DATA_DIVIDE_CLK_SEL_WR_MASK) | ((data & RGMII_DATA_DIVIDE_CLK_SEL_MASK) << 6));\
		RGMII_IO_MACRO_CONFIG_2_RGWR(v);\
} while (0)

#define RGMII_CONFIG_2_DATA_DIVIDE_CLK_SEL_UDFRD(data) do {\
		RGMII_IO_MACRO_CONFIG_2_RGRD(data);\
		data = ((data >> 6) & RGMII_DATA_DIVIDE_CLK_SEL_MASK);\
} while (0)

#define RGMII_TX_CLK_PHASE_SHIFT_EN_MASK (ULONG)(0x1)

#define RGMII_TX_CLK_PHASE_SHIFT_EN_WR_MASK (ULONG)(0xffffffdf)

#define RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFWR(data) do {\
		ULONG v;\
		RGMII_IO_MACRO_CONFIG_2_RGRD(v);\
		v = ((v & RGMII_TX_CLK_PHASE_SHIFT_EN_WR_MASK) | ((data & RGMII_TX_CLK_PHASE_SHIFT_EN_MASK) << 5));\
		RGMII_IO_MACRO_CONFIG_2_RGWR(v);\
} while (0)

#define RGMII_CONFIG_2_TX_CLK_PHASE_SHIFT_EN_UDFRD(data) do {\
		RGMII_IO_MACRO_CONFIG_2_RGRD(data);\
		data = ((data >> 5) & RGMII_TX_CLK_PHASE_SHIFT_EN_MASK);\
} while (0)

#define RGMII_TX_TO_RX_LOOPBACK_EN_MASK (ULONG)(0x1)

#define RGMII_TX_TO_RX_LOOPBACK_EN_WR_MASK (ULONG)(0xffffdfff)

#define RGMII_CONFIG_2_TX_TO_RX_LOOPBACK_EN_UDFWR(data) do {\
		ULONG v;\
		RGMII_IO_MACRO_CONFIG_2_RGRD(v);\
		v = ((v & RGMII_TX_TO_RX_LOOPBACK_EN_WR_MASK) | ((data & RGMII_TX_TO_RX_LOOPBACK_EN_MASK) << 13));\
		RGMII_IO_MACRO_CONFIG_2_RGWR(v);\
} while (0)

#define RGMII_CONFIG_2_TX_TO_RX_LOOPBACK_EN_UDFRD(data) do {\
		RGMII_IO_MACRO_CONFIG_2_RGRD(data);\
		data = ((data >> 13) & RGMII_TX_TO_RX_LOOPBACK_EN_MASK);\
} while (0)

#define RGMII_RX_PROG_SWAP_MASK (ULONG)(0x1)

#define RGMII_RX_PROG_SWAP_WR_MASK (ULONG)(0xffffff7f)

#define RGMII_CONFIG_2_RX_PROG_SWAP_UDFWR(data) do {\
		ULONG v;\
		RGMII_IO_MACRO_CONFIG_2_RGRD(v);\
		v = ((v & RGMII_RX_PROG_SWAP_WR_MASK) | ((data & RGMII_RX_PROG_SWAP_MASK) << 7));\
		RGMII_IO_MACRO_CONFIG_2_RGWR(v);\
} while (0)

#define RGMII_CONFIG_2_RX_PROG_SWAP_UDFRD(data) do {\
		RGMII_IO_MACRO_CONFIG_2_RGRD(data);\
		data = ((data >> 7) & RGMII_RX_PROG_SWAP_MASK);\
} while (0)

#define RGMII_CLK_DIVIDE_SEL_MASK (ULONG)(0x1)

#define RGMII_CLK_DIVIDE_SEL_WR_MASK (ULONG)(0xffffefff)

#define RGMII_CONFIG_2_CLK_DIVIDE_SEL_UDFWR(data) do {\
		ULONG v;\
		RGMII_IO_MACRO_CONFIG_2_RGRD(v);\
		v = ((v & RGMII_CLK_DIVIDE_SEL_WR_MASK) | ((data & RGMII_CLK_DIVIDE_SEL_MASK) << 12));\
		RGMII_IO_MACRO_CONFIG_2_RGWR(v);\
} while (0)

#define RGMII_CONFIG_2_CLK_DIVIDE_SEL_UDFRD(data) do {\
		RGMII_IO_MACRO_CONFIG_2_RGRD(data);\
		data = ((data >> 7) & RGMII_CLK_DIVIDE_SEL_MASK);\
} while (0)


#define RGMII_RERVED_CONFIG_16_EN_MASK (ULONG)(0xffff)

#define RGMII_RERVED_CONFIG_16_EN_WR_MASK (ULONG)(0x0000ffff)

#define RGMII_CONFIG_2_RERVED_CONFIG_16_EN_UDFWR(data) do {\
		ULONG v;\
		RGMII_IO_MACRO_CONFIG_2_RGRD(v);\
		v = ((v & RGMII_RERVED_CONFIG_16_EN_WR_MASK) | ((data & RGMII_RERVED_CONFIG_16_EN_MASK) << 16));\
		RGMII_IO_MACRO_CONFIG_2_RGWR(v);\
} while (0)

#define RGMII_CONFIG_2_RERVED_CONFIG_16_EN_UDFRD(data) do {\
		RGMII_IO_MACRO_CONFIG_2_RGRD(data);\
		data = ((data >> 16) & RGMII_RERVED_CONFIG_16_EN_MASK);\
} while (0)

/* EMAC_RGMII_IO_MACRO_DEBUG_1 */
#define RGMII_IO_MACRO_DEBUG_1_RGOFFADDR ((volatile ULONG *)(RGMII_IO_BASE_ADDRESS + 0x00000020))

#define RGMII_IO_MACRO_DEBUG_1_RGRD(data) do {\
		(data) = ioread32((void *)RGMII_IO_MACRO_DEBUG_1_RGOFFADDR);\
} while (0)

/* EMAC_SYSTEM_LOW_POWER_DEBUG */
#define EMAC_SYSTEM_LOW_POWER_DEBUG_RGOFFADDR ((volatile ULONG *)(RGMII_IO_BASE_ADDRESS + 0x00000028))

#define EMAC_SYSTEM_LOW_POWER_DEBUG_RGRD(data) do {\
		(data) = ioread32((void *)EMAC_SYSTEM_LOW_POWER_DEBUG_RGOFFADDR);\
} while (0)

void dump_rgmii_io_macro_registers(void);

#endif
