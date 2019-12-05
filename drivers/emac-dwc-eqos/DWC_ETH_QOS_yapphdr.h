/* Copyright (c) 2017,2019, The Linux Foundation. All rights reserved.
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
 * ========================================================================= */

#ifndef __DWC_ETH_QOS_YAPPHDR_H__

#define __DWC_ETH_QOS_YAPPHDR_H__

#define DWC_ETH_QOS_MAX_TX_QUEUE_CNT 8
#define DWC_ETH_QOS_MAX_RX_QUEUE_CNT 8

#define CONFIG_PPS_OUTPUT   // for PPS Output

/* Private IOCTL for handling device specific task */
#define DWC_ETH_QOS_PRV_IOCTL	SIOCDEVPRIVATE
#define DWC_ETH_QOS_PRV_IOCTL_IPA	SIOCDEVPRIVATE+1
/* IOCTL cmd to eMAC to register the RX/TX properties with VLAN hdr*/
enum{
 DWC_ETH_QOS_IPA_VLAN_DISABLE_CMD= 0,
 DWC_ETH_QOS_IPA_VLAN_ENABLE_CMD=1,
};

#define DWC_ETH_QOS_POWERUP_MAGIC_CMD	1
#define DWC_ETH_QOS_POWERDOWN_MAGIC_CMD	2
#define DWC_ETH_QOS_POWERUP_REMOTE_WAKEUP_CMD	3
#define DWC_ETH_QOS_POWERDOWN_REMOTE_WAKEUP_CMD	4

/* for TX and RX threshold configures */
#define DWC_ETH_QOS_RX_THRESHOLD_CMD	5
#define DWC_ETH_QOS_TX_THRESHOLD_CMD	6

/* for TX and RX Store and Forward mode configures */
#define DWC_ETH_QOS_RSF_CMD	7
#define DWC_ETH_QOS_TSF_CMD	8

/* for TX DMA Operate on Second Frame mode configures */
#define DWC_ETH_QOS_OSF_CMD	9

/* for TX and RX PBL configures */
#define DWC_ETH_QOS_TX_PBL_CMD	10
#define DWC_ETH_QOS_RX_PBL_CMD	11

/* INCR and INCRX mode */
#define DWC_ETH_QOS_INCR_INCRX_CMD	12

/* for MAC Double VLAN Processing config */
#define DWC_ETH_QOS_DVLAN_TX_PROCESSING_CMD		13
#define DWC_ETH_QOS_DVLAN_RX_PROCESSING_CMD		14
#define DWC_ETH_QOS_SVLAN_CMD				15

/* Manju: Remove the below defines */
/* RX/TX VLAN */
/* #define DWC_ETH_QOS_RX_OUTER_VLAN_STRIPPING_CMD	13 */
/* #define DWC_ETH_QOS_RX_INNER_VLAN_STRIPPING_CMD	14 */
/* #define DWC_ETH_QOS_TX_VLAN_DESC_CMD	15 */
/* #define DWC_ETH_QOS_TX_VLAN_REG_CMD	16 */

/* SA on TX */
#define DWC_ETH_QOS_SA0_DESC_CMD	17
#define DWC_ETH_QOS_SA1_DESC_CMD	18
#define DWC_ETH_QOS_SA0_REG_CMD		19
#define DWC_ETH_QOS_SA1_REG_CMD		20

/* CONTEX desc setup control */
#define DWC_ETH_QOS_SETUP_CONTEXT_DESCRIPTOR 21

/* Packet generation */
#define DWC_ETH_QOS_PG_TEST		22

/* TX/RX channel/queue count */
#define DWC_ETH_QOS_GET_TX_QCNT		23
#define DWC_ETH_QOS_GET_RX_QCNT		24

/* Line speed */
#define DWC_ETH_QOS_GET_CONNECTED_SPEED		25

/* DCB/AVB algorithm */
#define DWC_ETH_QOS_DCB_ALGORITHM		26
#define DWC_ETH_QOS_AVB_ALGORITHM		27

/* RX split header */
#define DWC_ETH_QOS_RX_SPLIT_HDR_CMD		28

/* L3/L4 filter */
#define DWC_ETH_QOS_L3_L4_FILTER_CMD		29
/* IPv4/6 and TCP/UDP filtering */
#define DWC_ETH_QOS_IPV4_FILTERING_CMD		30
#define DWC_ETH_QOS_IPV6_FILTERING_CMD		31
#define DWC_ETH_QOS_UDP_FILTERING_CMD		32
#define DWC_ETH_QOS_TCP_FILTERING_CMD		33
/* VLAN filtering */
#define DWC_ETH_QOS_VLAN_FILTERING_CMD		34
/* L2 DA filtering */
#define DWC_ETH_QOS_L2_DA_FILTERING_CMD		35
/* ARP Offload */
#define DWC_ETH_QOS_ARP_OFFLOAD_CMD		36
/* for AXI PBL configures */
#define DWC_ETH_QOS_AXI_PBL_CMD			37
/* for AXI Write Outstanding Request Limit configures */
#define DWC_ETH_QOS_AXI_WORL_CMD		38
/* for AXI Read Outstanding Request Limit configures */
#define DWC_ETH_QOS_AXI_RORL_CMD		39
/* for MAC LOOPBACK configuration */
#define DWC_ETH_QOS_MAC_LOOPBACK_MODE_CMD	40
/* PFC(Priority Based Flow Control) mode */
#define DWC_ETH_QOS_PFC_CMD			41
/* for PTP OFFLOADING configuration */
#define DWC_ETH_QOS_PTPOFFLOADING_CMD			42

/* To configure PPS output */
#ifdef CONFIG_PPS_OUTPUT
#define DWC_ETH_QOS_CONFIG_PTPCLK_CMD 43
#define DWC_ETH_QOS_CONFIG_PPSOUT_CMD 44
#endif

#define DWC_ETH_QOS_RWK_FILTER_LENGTH	8

/* List of command errors driver can set */
#define	DWC_ETH_QOS_NO_HW_SUPPORT	-1
#define	DWC_ETH_QOS_CONFIG_FAIL	-3
#define	DWC_ETH_QOS_CONFIG_SUCCESS	0

/* RX THRESHOLD operations */
#define DWC_ETH_QOS_RX_THRESHOLD_32	0x1
#define DWC_ETH_QOS_RX_THRESHOLD_64	0x0
#define DWC_ETH_QOS_RX_THRESHOLD_96	0x2
#define DWC_ETH_QOS_RX_THRESHOLD_128	0x3

/* TX THRESHOLD operations */
#define DWC_ETH_QOS_TX_THRESHOLD_32	0x1
#define DWC_ETH_QOS_TX_THRESHOLD_64	0x0
#define DWC_ETH_QOS_TX_THRESHOLD_96	0x2
#define DWC_ETH_QOS_TX_THRESHOLD_128	0x3
#define DWC_ETH_QOS_TX_THRESHOLD_192	0x4
#define DWC_ETH_QOS_TX_THRESHOLD_256	0x5
#define DWC_ETH_QOS_TX_THRESHOLD_384	0x6
#define DWC_ETH_QOS_TX_THRESHOLD_512	0x7

/* TX and RX Store and Forward Mode operations */
#define DWC_ETH_QOS_RSF_DISABLE	0x0
#define DWC_ETH_QOS_RSF_ENABLE	0x1

#define DWC_ETH_QOS_TSF_DISABLE	0x0
#define DWC_ETH_QOS_TSF_ENABLE	0x1

/* TX DMA Operate on Second Frame operations */
#define DWC_ETH_QOS_OSF_DISABLE	0x0
#define DWC_ETH_QOS_OSF_ENABLE	0x1

/* INCR and INCRX mode */
#define DWC_ETH_QOS_INCR_ENABLE		0x1
#define DWC_ETH_QOS_INCRX_ENABLE	0x0

/* TX and RX PBL operations */
#define DWC_ETH_QOS_PBL_1	1
#define DWC_ETH_QOS_PBL_2	2
#define DWC_ETH_QOS_PBL_4	4
#define DWC_ETH_QOS_PBL_8	8
#define DWC_ETH_QOS_PBL_16	16
#define DWC_ETH_QOS_PBL_32	32
#define DWC_ETH_QOS_PBL_64	64	/* 8 x 8 */
#define DWC_ETH_QOS_PBL_128	128	/* 8 x 16 */
#define DWC_ETH_QOS_PBL_256	256	/* 8 x 32 */

/* AXI operations */
#define DWC_ETH_QOS_AXI_PBL_4	0x2
#define DWC_ETH_QOS_AXI_PBL_8	0x6
#define DWC_ETH_QOS_AXI_PBL_16	0xE
#define DWC_ETH_QOS_AXI_PBL_32	0x1E
#define DWC_ETH_QOS_AXI_PBL_64	0x3E
#define DWC_ETH_QOS_AXI_PBL_128	0x7E
#define DWC_ETH_QOS_AXI_PBL_256	0xFE

#define DWC_ETH_QOS_MAX_AXI_WORL 31
#define DWC_ETH_QOS_MAX_AXI_RORL 31

/* RX VLAN operations */
/* Do not strip VLAN tag from received pkt */
#define DWC_ETH_QOS_RX_NO_VLAN_STRIP	0x0
/* Strip VLAN tag if received pkt pass VLAN filter */
#define DWC_ETH_QOS_RX_VLAN_STRIP_IF_FILTER_PASS  0x1
/* Strip VLAN tag if received pkt fial VLAN filter */
#define DWC_ETH_QOS_RX_VLAN_STRIP_IF_FILTER_FAIL  0x2
/* Strip VALN tag always from received pkt */
#define DWC_ETH_QOS_RX_VLAN_STRIP_ALWAYS	0x3

/* TX VLAN operations */
/* Do not add a VLAN tag dring pkt transmission */
#define DWC_ETH_QOS_TX_VLAN_TAG_NONE	0x0
/* Remove the VLAN tag from the pkt before transmission */
#define DWC_ETH_QOS_TX_VLAN_TAG_DELETE	0x1
/* Insert the VLAN tag into pkt to be transmitted */
#define DWC_ETH_QOS_TX_VLAN_TAG_INSERT	0x2
/* Replace the VLAN tag into pkt to be transmitted */
#define DWC_ETH_QOS_TX_VLAN_TAG_REPLACE	0x3

/* RX split header operations */
#define DWC_ETH_QOS_RX_SPLIT_HDR_DISABLE 0x0
#define DWC_ETH_QOS_RX_SPLIT_HDR_ENABLE 0x1

/* L3/L4 filter operations */
#define DWC_ETH_QOS_L3_L4_FILTER_DISABLE 0x0
#define DWC_ETH_QOS_L3_L4_FILTER_ENABLE 0x1

/* Loopback mode */
#define DWC_ETH_QOS_MAC_LOOPBACK_DISABLE 0x0
#define DWC_ETH_QOS_MAC_LOOPBACK_ENABLE 0x1

/* PFC(Priority Based Flow Control) mode */
#define DWC_ETH_QOS_PFC_DISABLE 0x0
#define DWC_ETH_QOS_PFC_ENABLE 0x1

#define DWC_ETH_QOS_MAC0REG 0
#define DWC_ETH_QOS_MAC1REG 1

#define DWC_ETH_QOS_SA0_NONE		((DWC_ETH_QOS_MAC0REG << 2) | 0) /* Do not include the SA */
#define DWC_ETH_QOS_SA0_DESC_INSERT	((DWC_ETH_QOS_MAC0REG << 2) | 1) /* Include/Insert the SA with value given in MAC Addr 0 Reg */
#define DWC_ETH_QOS_SA0_DESC_REPLACE	((DWC_ETH_QOS_MAC0REG << 2) | 2) /* Replace the SA with the value given in MAC Addr 0 Reg */
#define DWC_ETH_QOS_SA0_REG_INSERT	((DWC_ETH_QOS_MAC0REG << 2) | 2) /* Include/Insert the SA with value given in MAC Addr 0 Reg */
#define DWC_ETH_QOS_SA0_REG_REPLACE	((DWC_ETH_QOS_MAC0REG << 2) | 3) /* Replace the SA with the value given in MAC Addr 0 Reg */

#define DWC_ETH_QOS_SA1_NONE		((DWC_ETH_QOS_MAC1REG << 2) | 0) /* Do not include the SA */
#define DWC_ETH_QOS_SA1_DESC_INSERT	((DWC_ETH_QOS_MAC1REG << 2) | 1) /* Include/Insert the SA with value given in MAC Addr 1 Reg */
#define DWC_ETH_QOS_SA1_DESC_REPLACE	((DWC_ETH_QOS_MAC1REG << 2) | 2) /* Replace the SA with the value given in MAC Addr 1 Reg */
#define DWC_ETH_QOS_SA1_REG_INSERT	((DWC_ETH_QOS_MAC1REG << 2) | 2) /* Include/Insert the SA with value given in MAC Addr 1 Reg */
#define DWC_ETH_QOS_SA1_REG_REPLACE	((DWC_ETH_QOS_MAC1REG << 2) | 3) /* Replace the SA with the value given in MAC Addr 1 Reg */

#define DWC_ETH_QOS_MAX_WFQ_WEIGHT	0X7FFF /* value for bandwidth calculation */

#define DWC_ETH_QOS_MAX_INT_FRAME_SIZE (1024 * 16)

typedef enum {
	EDWC_ETH_QOS_DMA_TX_FP = 0,
	EDWC_ETH_QOS_DMA_TX_WSP = 1,
	EDWC_ETH_QOS_DMA_TX_WRR = 2,
} e_DWC_ETH_QOS_dma_tx_arb_algo;

typedef enum {
	EDWC_ETH_QOS_DCB_WRR = 0,
	EDWC_ETH_QOS_DCB_WFQ = 1,
	EDWC_ETH_QOS_DCB_DWRR = 2,
	EDWC_ETH_QOS_DCB_SP = 3,
} e_DWC_ETH_QOS_dcb_algorithm;

typedef enum {
	EDWC_ETH_QOS_AVB_SP = 0,
	EDWC_ETH_QOS_AVB_CBS = 1,
} e_DWC_ETH_QOS_avb_algorithm;

typedef enum {
	EDWC_ETH_QOS_QDISABLED = 0x0,
	EDWC_ETH_QOS_QAVB,
	EDWC_ETH_QOS_QDCB,
	EDWC_ETH_QOS_QGENERIC
} EDWC_ETH_QOS_QUEUE_OPERATING_MODE;

/* common data structure between driver and application for
 * sharing info through ioctl
 * */
struct ifr_data_struct {
	unsigned int flags;
	unsigned int qinx; /* dma channel no to be configured */
	unsigned int cmd;
	unsigned int context_setup;
	unsigned int connected_speed;
	unsigned int rwk_filter_values[DWC_ETH_QOS_RWK_FILTER_LENGTH];
	unsigned int rwk_filter_length;
	int command_error;
	int test_done;
	void *ptr;
};

struct ifr_data_struct_ipa {
	unsigned int chInx_tx_ipa;
	unsigned int chInx_rx_ipa;
	unsigned int cmd;
	unsigned short vlan_id;
};

struct DWC_ETH_QOS_dcb_algorithm {
	unsigned int qinx;
	unsigned int algorithm;
	unsigned int weight;
	EDWC_ETH_QOS_QUEUE_OPERATING_MODE op_mode;
};

struct DWC_ETH_QOS_avb_algorithm_params {
	unsigned int idle_slope;
	unsigned int send_slope;
	unsigned int hi_credit;
	unsigned int low_credit;
};

struct DWC_ETH_QOS_avb_algorithm {
	unsigned int qinx;
	unsigned int algorithm;
	unsigned int cc;
	struct DWC_ETH_QOS_avb_algorithm_params speed100params;
	struct DWC_ETH_QOS_avb_algorithm_params speed1000params;
	EDWC_ETH_QOS_QUEUE_OPERATING_MODE op_mode;
};

struct DWC_ETH_QOS_l3_l4_filter {
	/* 0, 1,2,3,4,5,6 or 7*/
	unsigned int filter_no;
	/* 0 - disable and 1 - enable */
	int filter_enb_dis;
	/* 0 - src addr/port and 1- dst addr/port match */
	int src_dst_addr_match;
	/* 0 - perfect and 1 - inverse match filtering */
	int perfect_inverse_match;
	/* To hold source/destination IPv4 addresses */
	unsigned char ip4_addr[4];
	/* holds single IPv6 addresses */
	unsigned short ip6_addr[8];

	/* TCP/UDP src/dst port number */
	unsigned short port_no;
};

struct DWC_ETH_QOS_vlan_filter {
	/* 0 - disable and 1 - enable */
	int filter_enb_dis;
	/* 0 - perfect and 1 - hash filtering */
	int perfect_hash;
	/* 0 - perfect and 1 - inverse matching */
	int perfect_inverse_match;
};

struct DWC_ETH_QOS_l2_da_filter {
	/* 0 - perfect and 1 - hash filtering */
	int perfect_hash;
	/* 0 - perfect and 1 - inverse matching */
	int perfect_inverse_match;
};

struct DWC_ETH_QOS_arp_offload {
	unsigned char ip_addr[4];
};

#define DWC_ETH_QOS_VIA_REG	0
#define DWC_ETH_QOS_VIA_DESC	1

/* for MAC Double VLAN Processing config */
#define DWC_ETH_QOS_DVLAN_OUTER	(1)
#define DWC_ETH_QOS_DVLAN_INNER	(1 << 1)
#define DWC_ETH_QOS_DVLAN_BOTH	(DWC_ETH_QOS_DVLAN_OUTER | DWC_ETH_QOS_DVLAN_INNER)

#define DWC_ETH_QOS_DVLAN_NONE	0
#define DWC_ETH_QOS_DVLAN_DELETE	1
#define DWC_ETH_QOS_DVLAN_INSERT	2
#define DWC_ETH_QOS_DVLAN_REPLACE	3

#define DWC_ETH_QOS_DVLAN_DISABLE	0
#define DWC_ETH_QOS_DVLAN_ENABLE	1

struct DWC_ETH_QOS_config_dvlan {
	int inner_vlan_tag;
	int outer_vlan_tag;
	/* op_type will be
	 * 0/1/2/3 for none/delet/insert/replace respectively
	 * */
	int op_type;
	/* in_out will be
	 * 1/2/3 for outer/inner/both respectively.
	 * */
	int in_out;
	/* 0 for via registers and 1 for via descriptor */
	int via_reg_or_desc;
};

/* for PTP offloading configuration */
#define DWC_ETH_QOS_PTP_OFFLOADING_DISABLE		0
#define DWC_ETH_QOS_PTP_OFFLOADING_ENABLE			1

#define DWC_ETH_QOS_PTP_ORDINARY_SLAVE			1
#define DWC_ETH_QOS_PTP_ORDINARY_MASTER			2
#define DWC_ETH_QOS_PTP_TRASPARENT_SLAVE			3
#define DWC_ETH_QOS_PTP_TRASPARENT_MASTER			4
#define DWC_ETH_QOS_PTP_PEER_TO_PEER_TRANSPARENT	5

struct DWC_ETH_QOS_config_ptpoffloading {
	int en_dis;
	int mode;
	int domain_num;
    int mc_uc;
};

#ifdef CONFIG_PPS_OUTPUT
struct ETH_PPS_Config
{
	unsigned int ptpclk_freq;
	unsigned int ppsout_freq;
	unsigned int ppsout_ch;
	unsigned int ppsout_duty;
	unsigned int ppsout_start;
};
#endif

#ifdef DWC_ETH_QOS_CONFIG_PGTEST

/* uncomment below macro to enable application
 * to record all run reports to file */
/* #define PGTEST_LOGFILE */

/* TX DMA CHANNEL Weights */
#define DWC_ETH_QOS_TX_CH_WEIGHT1	0x0
#define DWC_ETH_QOS_TX_CH_WEIGHT2	0x1
#define DWC_ETH_QOS_TX_CH_WEIGHT3	0x2
#define DWC_ETH_QOS_TX_CH_WEIGHT4	0x3
#define DWC_ETH_QOS_TX_CH_WEIGHT5	0x4
#define DWC_ETH_QOS_TX_CH_WEIGHT6	0x5
#define DWC_ETH_QOS_TX_CH_WEIGHT7	0x6
#define DWC_ETH_QOS_TX_CH_WEIGHT8	0x7

/* PG test sub commands macro's */
#define DWC_ETH_QOS_PG_SET_CONFIG	0x1
#define DWC_ETH_QOS_PG_CONFIG_HW	  0x2
#define DWC_ETH_QOS_PG_RUN_TEST		0x3
#define DWC_ETH_QOS_PG_GET_RESULT	0x4
#define DWC_ETH_QOS_PG_TEST_DONE	0x5

/* DMA channel bandwidth allocation parameters */
struct DWC_ETH_QOS_pg_user_ch_input {
	unsigned char ch_arb_weight;	/* Channel weights(1/2/3/4/5/6/7/8) for arbitration */
	unsigned int ch_fr_size;	/* Channel Frame size */
	unsigned char ch_bw_alloc;	/* The percentage bandwidth allocation for ch */

	unsigned char ch_use_slot_no_check;	/* Should Ch use slot number checking ? */
	unsigned char ch_use_adv_slot_no_check;
	unsigned char ch_slot_count_to_use;	/* How many slot used to report pg bits per slot value */

	unsigned char ch_use_credit_shape;	/* Should Ch use Credid shape algorithm for traffic shaping ? */
	unsigned char CH_CREDITCONTROL;	/* Sould Ch use Credit Control algorithm for traffic shaping ? */

	unsigned char ch_tx_desc_slot_no_start;
	unsigned char ch_tx_desc_slot_no_skip;
	unsigned char ch_operating_mode;
	unsigned long CH_AVGBITS;
	unsigned long CH_AVGBITS_INTERRUPT_COUNT;
	unsigned char ch_avb_algorithm;
	unsigned char ch_debug_mode; /* enable/disable debug mode */
	unsigned int ch_max_tx_frame_cnt; /* maximum pkts to be sent on this channel, can be used for debug purpose */
};

struct DWC_ETH_QOS_pg_user_input {
	unsigned char duration_of_exp;
	/* enable bits for DMA. bit0=>ch0, bit1=>ch1, bit2=>ch2 */
	unsigned char dma_ch_en;

	unsigned char ch_tx_rx_arb_scheme;	/* Should Ch use Weighted RR policy with Rx:Tx/Tx:Rx or Fixed Priority */
	unsigned char ch_use_tx_high_prio;	/* Should Ch Tx have High priority over Rx */
	unsigned char ch_tx_rx_prio_ratio;	/* For RR what is the ratio between Tx:Rx/Rx:Tx */
	unsigned char dma_tx_arb_algo; /* Refer DMA Mode register TAA field */

	unsigned char queue_dcb_algorithm;

	unsigned char mac_lb_mode; /* 0 => No MAC Loopback; 1 => MAC Loopback On */
	unsigned int speed_100M_1G; /* 0 => No MAC Loopback; 1 => MAC Loopback On */

	struct DWC_ETH_QOS_pg_user_ch_input ch_input[DWC_ETH_QOS_MAX_TX_QUEUE_CNT];
};

#define copy_pg_ch_input_members(to, from) do { \
	(to)->interrupt_prints = (from)->interrupt_prints; \
	(to)->tx_interrupts = (from)->tx_interrupts; \
	(to)->ch_arb_weight = (from)->ch_arb_weight; \
	(to)->ch_queue_weight = (from)->ch_queue_weight; \
	(to)->ch_bw = (from)->ch_bw; \
	(to)->ch_frame_size = (from)->ch_frame_size; \
	(to)->CH_ENABLESLOTCHECK = (from)->CH_ENABLESLOTCHECK; \
	(to)->CH_ENABLEADVSLOTCHECK = (from)->CH_ENABLEADVSLOTCHECK; \
	(to)->ch_avb_algorithm = (from)->ch_avb_algorithm; \
	(to)->CH_SLOTCOUNT = (from)->CH_SLOTCOUNT; \
	(to)->CH_AVGBITS = (from)->CH_AVGBITS; \
	(to)->CH_AVGBITS_INTERRUPT_COUNT = (from)->CH_AVGBITS_INTERRUPT_COUNT; \
	(to)->CH_CREDITCONTROL = (from)->CH_CREDITCONTROL; \
	(to)->ch_tx_desc_slot_no_start = (from)->ch_tx_desc_slot_no_start; \
	(to)->ch_tx_desc_slot_no_skip = (from)->ch_tx_desc_slot_no_skip; \
	(to)->CH_SENDSLOPE = (from)->CH_SENDSLOPE; \
	(to)->CH_IDLESLOPE = (from)->CH_IDLESLOPE; \
	(to)->CH_HICREDIT = (from)->CH_HICREDIT; \
	(to)->CH_LOCREDIT = (from)->CH_LOCREDIT; \
	(to)->CH_FRAMECOUNTTX = (from)->CH_FRAMECOUNTTX; \
	(to)->CH_FRAMECOUNTRX = (from)->CH_FRAMECOUNTRX; \
	(to)->ch_operating_mode = (from)->ch_operating_mode; \
	(to)->ch_debug_mode = (from)->ch_debug_mode;\
	(to)->ch_max_tx_frame_cnt = (from)->ch_max_tx_frame_cnt;\
} while (0)

struct DWC_ETH_QOS_pg_ch_input {
	unsigned int interrupt_prints;
	unsigned int tx_interrupts;
	unsigned char ch_arb_weight;
	unsigned int ch_queue_weight;
	unsigned char ch_bw;
	unsigned int ch_frame_size;
	unsigned char CH_ENABLESLOTCHECK;	/* Enable checking of slot numbers programmed in the Tx Desc */
	unsigned char CH_ENABLEADVSLOTCHECK;	/* When Set Data fetched for current slot and for next 2 slots in advance
						When reset data fetched for current slot and in advance for next slot*/

	unsigned char ch_avb_algorithm;
	unsigned char CH_SLOTCOUNT;	/* Over which transmiteed bits per slot needs to be computed (Only for Credit based shaping) */
	unsigned long CH_AVGBITS;
	unsigned long CH_AVGBITS_INTERRUPT_COUNT;

	unsigned char CH_CREDITCONTROL;	/* Will be zero (Not used) */

	unsigned char ch_tx_desc_slot_no_start;
	unsigned char ch_tx_desc_slot_no_skip;

	unsigned int CH_SENDSLOPE;
	unsigned int CH_IDLESLOPE;
	unsigned int CH_HICREDIT;
	unsigned int CH_LOCREDIT;

	unsigned long CH_FRAMECOUNTTX;	/* No of Frames Transmitted on Channel 1 */
	unsigned long CH_FRAMECOUNTRX;	/* No of Frames Received on Channel 1 */
	unsigned char ch_operating_mode;

	unsigned char ch_debug_mode; /* enable/disable debug mode */
	unsigned int ch_max_tx_frame_cnt; /* maximum pkts to be sent on this channel, can be used for debug purpose */
	unsigned int ch_desc_prepare; /* max packets which will be reprepared in Tx-interrupt
																	 do not copy contents to app-copy, only driver should use this variable*/
};

#define COPY_PGSTRUCT_MEMBERS(to, from)	do { \
	(to)->CH_SELMASK = (from)->CH_SELMASK; \
	(to)->DURATIONOFEXP = (from)->DURATIONOFEXP; \
	(to)->PRIOTAGFORAV = (from)->PRIOTAGFORAV; \
	(to)->queue_dcb_algorithm = (from)->queue_dcb_algorithm; \
	(to)->ch_tx_rx_arb_scheme = (from)->ch_tx_rx_arb_scheme; \
	(to)->ch_use_tx_high_prio = (from)->ch_use_tx_high_prio; \
	(to)->ch_tx_rx_prio_ratio = (from)->ch_tx_rx_prio_ratio; \
	(to)->dma_tx_arb_algo = (from)->dma_tx_arb_algo; \
	(to)->mac_lb_mode = (from)->mac_lb_mode; \
} while (0)

struct DWC_ETH_QOS_PGSTRUCT {
	/* This gives which DMA channel is enabled and which is disabled
	 * Bit0 for Ch0
	 * Bit1 for Ch1
	 * Bit2 for Ch2 and so on
	 * Bit7 for Ch7
	 * */
	unsigned char CH_SELMASK;

	/* Duration for which experiment should be conducted in minutes - Default 2 Minutes */
	unsigned char DURATIONOFEXP;

	/* Used when more than One channel enabled in Rx path (Not Used)
	 * for only CH1 Enabled:
	 * Frames sith Priority > Value programmed, frames sent to CH1
	 * Frames with priority < Value programmed are sent to CH0
	 *
	 * For both CH1 and CH2 Enabled:
	 * Frames sith Priority > Value programmed, frames sent to CH2
	 * Frames with priority < Value programmed are sent to CH
	 * */
	unsigned char PRIOTAGFORAV;

	unsigned char queue_dcb_algorithm;

	unsigned char ch_tx_rx_arb_scheme;	/* Should Ch use Weighted RR policy with Rx:Tx/Tx:Rx or Fixed Priority */
	unsigned char ch_use_tx_high_prio;	/* Should Ch Tx have High priority over Rx */
	unsigned char ch_tx_rx_prio_ratio;	/* For RR what is the ratio between Tx:Rx/Rx:Tx */
	unsigned char dma_tx_arb_algo; /* Refer DMA Mode register TAA field */

	unsigned char mac_lb_mode; /* 0 => No MAC Loopback; 1 => MAC Loopback On */
    unsigned int speed_100M_1G;
	struct DWC_ETH_QOS_pg_ch_input pg_ch_input[DWC_ETH_QOS_MAX_TX_QUEUE_CNT];
	unsigned char channel_running[DWC_ETH_QOS_MAX_TX_QUEUE_CNT];
};
#endif /* end of DWC_ETH_QOS_CONFIG_PGTEST */

#endif
