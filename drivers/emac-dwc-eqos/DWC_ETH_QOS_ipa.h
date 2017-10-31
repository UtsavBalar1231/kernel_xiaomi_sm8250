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

#ifdef DWC_ETH_QOS_ENABLE_IPA

#define EMAC_IPA_CAPABLE	1

/* IPA Ready client callback. Called by IPA when its ready */
void DWC_ETH_QOS_ipa_uc_ready_cb(void *user_data);

int DWC_ETH_QOS_enable_ipa_offload(struct DWC_ETH_QOS_prv_data *pdata);
int DWC_ETH_QOS_disable_ipa_offload(struct DWC_ETH_QOS_prv_data *pdata);
int DWC_ETH_QOS_disable_enable_ipa_offload(struct DWC_ETH_QOS_prv_data *pdata,int chInx_tx_ipa,
		int chInx_rx_ipa);

/* Initialize Offload data path and add partial headers */
int DWC_ETH_QOS_ipa_offload_init(struct DWC_ETH_QOS_prv_data *pdata);

/* Cleanup Offload data path */
int DWC_ETH_QOS_ipa_offload_cleanup(struct DWC_ETH_QOS_prv_data *pdata);

/* Connect Offload Data path */
int DWC_ETH_QOS_ipa_offload_connect(struct DWC_ETH_QOS_prv_data *pdata);

/* Disconnect Offload Data path */
int DWC_ETH_QOS_ipa_offload_disconnect(struct DWC_ETH_QOS_prv_data *pdata);

/* Create Debugfs Node */
int DWC_ETH_QOS_ipa_create_debugfs(struct DWC_ETH_QOS_prv_data *pdata);

/* Cleanup Debugfs Node */
int DWC_ETH_QOS_ipa_cleanup_debugfs(struct DWC_ETH_QOS_prv_data *pdata);

void DWC_ETH_QOS_ipa_stats_read(struct DWC_ETH_QOS_prv_data *pdata);
int DWC_ETH_QOS_ipa_offload_suspend(struct DWC_ETH_QOS_prv_data *pdata);
int DWC_ETH_QOS_ipa_offload_resume(struct DWC_ETH_QOS_prv_data *pdata);
int DWC_ETH_QOS_ipa_ready(struct DWC_ETH_QOS_prv_data *pdata);

#else /* DWC_ETH_QOS_ENABLE_IPA */

#define EMAC_IPA_CAPABLE	0

static inline int DWC_ETH_QOS_ipa_offload_init(struct DWC_ETH_QOS_prv_data *pdata)
{
	return -EPERM;
}

static inline void DWC_ETH_QOS_ipa_uc_ready_cb(void *user_data)
{
	return;
}

static inline int DWC_ETH_QOS_enable_ipa_offload(struct DWC_ETH_QOS_prv_data *pdata)
{
	return -EPERM;
}

static inline int DWC_ETH_QOS_disable_ipa_offload(struct DWC_ETH_QOS_prv_data *pdata)
{
	return -EPERM;
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

static inline int DWC_ETH_QOS_ipa_offload_suspend(struct DWC_ETH_QOS_prv_data *pdata)
{
	return -EPERM;
}
static inline int DWC_ETH_QOS_ipa_offload_resume(struct DWC_ETH_QOS_prv_data *pdata)
{
	return -EPERM;
}

static inline int DWC_ETH_QOS_ipa_ready(struct DWC_ETH_QOS_prv_data *pdata)
{
	return -EPERM;
}

#endif /* DWC_ETH_QOS_ENABLE_IPA */

#endif
