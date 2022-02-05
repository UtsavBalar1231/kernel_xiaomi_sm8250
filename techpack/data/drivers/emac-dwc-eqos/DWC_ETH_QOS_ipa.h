/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of The Linux Foundation nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
*/

#ifndef __DWC_ETH_QOS_IPA_H__
#define __DWC_ETH_QOS_IPA_H__

#include <linux/ipa.h>
#include <linux/ipa_uc_offload.h>
#include <asm/io.h>
#include <linux/debugfs.h>
#include <linux/in.h>
#include <linux/ip.h>

#include "DWC_ETH_QOS_yheader.h"
#include "DWC_ETH_QOS_yregacc.h"
#include "DWC_ETH_QOS_yrgmii_io_macro_regacc.h"

typedef enum {
	EV_INVALID = 0,
	EV_DEV_OPEN,
	EV_DEV_CLOSE,
	EV_IPA_READY,
	EV_IPA_UC_READY,
	EV_PHY_LINK_UP,
	EV_PHY_LINK_DOWN,
	EV_DPM_SUSPEND,
	EV_DPM_RESUME,
	EV_USR_SUSPEND,
	EV_USR_RESUME,
	EV_IPA_OFFLOAD_MAX,
} IPA_OFFLOAD_EVENT;

#ifdef DWC_ETH_QOS_ENABLE_IPA

#define EMAC_IPA_CAPABLE	1

void DWC_ETH_QOS_ipa_offload_event_handler(
   struct DWC_ETH_QOS_prv_data *pdata, IPA_OFFLOAD_EVENT ev);
int DWC_ETH_QOS_disable_enable_ipa_offload(struct DWC_ETH_QOS_prv_data *pdata,int chInx_tx_ipa,
		int chInx_rx_ipa);
void DWC_ETH_QOS_ipa_stats_read(struct DWC_ETH_QOS_prv_data *pdata);

#else /* DWC_ETH_QOS_ENABLE_IPA */

#define EMAC_IPA_CAPABLE	0

static inline void DWC_ETH_QOS_ipa_offload_event_handler(
   struct DWC_ETH_QOS_prv_data *pdata, IPA_OFFLOAD_EVENT ev)
{
	return;
}

static inline int DWC_ETH_QOS_disable_enable_ipa_offload(struct DWC_ETH_QOS_prv_data *pdata,
					int chInx_tx_ipa, int chInx_rx_ipa)
{
	return -EPERM;
}

static inline void DWC_ETH_QOS_ipa_stats_read(struct DWC_ETH_QOS_prv_data *pdata)
{
	return;
}

#endif /* DWC_ETH_QOS_ENABLE_IPA */

#endif
