/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#include "DWC_ETH_QOS_ipa.h"

extern ULONG dwc_eth_qos_base_addr;
extern struct DWC_ETH_QOS_res_data dwc_eth_qos_res_data;

#define IPA_PIPE_MIN_BW 0
#define NTN_IPA_DBG_MAX_MSG_LEN 3000
static char buf[3000];

#define IPA_LOCK() mutex_lock(&pdata->prv_ipa.ipa_lock)
#define IPA_UNLOCK() mutex_unlock(&pdata->prv_ipa.ipa_lock)

static const char* IPA_OFFLOAD_EVENT_string[] = {
	"EV_INVALID",
	"EV_DEV_OPEN",
	"EV_DEV_CLOSE",
	"EV_IPA_READY",
	"EV_IPA_UC_READY",
	"EV_PHY_LINK_UP",
	"EV_PHY_LINK_DOWN",
	"EV_DPM_SUSPEND",
	"EV_DPM_RESUME",
	"EV_USR_SUSPEND",
	"EV_USR_RESUME",
	"EV_IPA_OFFLOAD_MAX"
};

static int DWC_ETH_QOS_ipa_ready(struct DWC_ETH_QOS_prv_data *pdata);
static void DWC_ETH_QOS_ipa_ready_cb(void *user_data);
static int DWC_ETH_QOS_ipa_uc_ready(struct DWC_ETH_QOS_prv_data *pdata);
static void DWC_ETH_QOS_ipa_uc_ready_cb(void *user_data);

static int DWC_ETH_QOS_ipa_offload_resume(struct DWC_ETH_QOS_prv_data *pdata);
static int DWC_ETH_QOS_ipa_offload_suspend(struct DWC_ETH_QOS_prv_data *pdata);
static int DWC_ETH_QOS_enable_ipa_offload(struct DWC_ETH_QOS_prv_data *pdata);
static int DWC_ETH_QOS_disable_ipa_offload(struct DWC_ETH_QOS_prv_data *pdata);

/* Initialize Offload data path and add partial headers */
static int DWC_ETH_QOS_ipa_offload_init(struct DWC_ETH_QOS_prv_data *pdata);

/* Cleanup Offload data path */
static int DWC_ETH_QOS_ipa_offload_cleanup(struct DWC_ETH_QOS_prv_data *pdata);

/* Connect Offload Data path */
static int DWC_ETH_QOS_ipa_offload_connect(struct DWC_ETH_QOS_prv_data *pdata);

/* Disconnect Offload Data path */
static int DWC_ETH_QOS_ipa_offload_disconnect(struct DWC_ETH_QOS_prv_data *pdata);

/* Create Debugfs Node */
static int DWC_ETH_QOS_ipa_create_debugfs(struct DWC_ETH_QOS_prv_data *pdata);

/* Cleanup Debugfs Node */
static int DWC_ETH_QOS_ipa_cleanup_debugfs(struct DWC_ETH_QOS_prv_data *pdata);

/* Generic Bit descirption; reset = 0, set = 1*/
static const char *bit_status_string[] = {
	"Reset",
	"Set",
};

/* Generic Bit Mask Description; masked = 0, enabled = 1*/
static const char *bit_mask_string[] = {
	"Masked",
	"Enable",
};

#define IPA_ETH_RX_SOFTIRQ_THRESH	16


void DWC_ETH_QOS_ipa_offload_event_handler(
   struct DWC_ETH_QOS_prv_data *pdata, IPA_OFFLOAD_EVENT ev)
{


	IPA_LOCK();

	EMACDBG("Enter: event=%s\n", IPA_OFFLOAD_EVENT_string[ev]);
	EMACDBG("PHY_link=%d\n"
	"emac_dev_ready=%d\n"
	"ipa_ready=%d\n"
	"ipa_uc_ready=%d\n"
	"ipa_offload_init=%d\n"
	"ipa_offload_conn=%d\n"
	"ipa_debugfs_exists=%d\n"
	"ipa_offload_susp=%d\n"
	"ipa_offload_link_down=%d\n",
	DWC_ETH_QOS_is_phy_link_up(pdata),
    pdata->prv_ipa.emac_dev_ready,
	pdata->prv_ipa.ipa_ready,
	pdata->prv_ipa.ipa_uc_ready,
	pdata->prv_ipa.ipa_offload_init,
	pdata->prv_ipa.ipa_offload_conn,
	pdata->prv_ipa.ipa_debugfs_exists,
	pdata->prv_ipa.ipa_offload_susp,
	pdata->prv_ipa.ipa_offload_link_down);

	switch (ev) {
	case EV_PHY_LINK_DOWN:
		{
			if (!pdata->prv_ipa.emac_dev_ready
					|| !pdata->prv_ipa.ipa_uc_ready
					|| pdata->prv_ipa.ipa_offload_link_down
					|| pdata->prv_ipa.ipa_offload_susp
					|| !pdata->prv_ipa.ipa_offload_conn)
				break;

			if (!DWC_ETH_QOS_ipa_offload_suspend(pdata))
				pdata->prv_ipa.ipa_offload_link_down = true;
		}
		break;
	case EV_PHY_LINK_UP:
		{
			if (!pdata->prv_ipa.emac_dev_ready
					|| !pdata->prv_ipa.ipa_uc_ready
					|| pdata->prv_ipa.ipa_offload_susp)
				break;

			/* Link up event is expected only after link down */
			if (pdata->prv_ipa.ipa_offload_link_down) {
				DWC_ETH_QOS_ipa_offload_resume(pdata);
			}
			/* This link up is expected only after boot up with cable disconnected */
			/* FIX ME for back to back link up events */
			else if(pdata->prv_ipa.emac_dev_ready
					  && pdata->prv_ipa.ipa_uc_ready) {
				DWC_ETH_QOS_enable_ipa_offload(pdata);
			}

			pdata->prv_ipa.ipa_offload_link_down = false;
		}
		break;
	case EV_DEV_OPEN:
		{
			pdata->prv_ipa.emac_dev_ready = true;

			if (!pdata->prv_ipa.ipa_ready)
				DWC_ETH_QOS_ipa_ready(pdata);

			if (pdata->prv_ipa.ipa_ready) {
				if (!pdata->prv_ipa.ipa_offload_init) {
					if(!DWC_ETH_QOS_ipa_offload_init(pdata))
						pdata->prv_ipa.ipa_offload_init = true;
				}

				if (!pdata->prv_ipa.ipa_uc_ready)
					DWC_ETH_QOS_ipa_uc_ready(pdata);

				if (pdata->prv_ipa.ipa_uc_ready
					&& DWC_ETH_QOS_is_phy_link_up(pdata))
						DWC_ETH_QOS_enable_ipa_offload(pdata);
			}
		}
		break;
	case EV_IPA_READY:
		{
				pdata->prv_ipa.ipa_ready = true;

				if (!pdata->prv_ipa.ipa_offload_init) {
					if(!DWC_ETH_QOS_ipa_offload_init(pdata))
						pdata->prv_ipa.ipa_offload_init = true;
				}

				if (!pdata->prv_ipa.ipa_uc_ready) {
					DWC_ETH_QOS_ipa_uc_ready(pdata);
				}

				if (pdata->prv_ipa.ipa_uc_ready
					&& DWC_ETH_QOS_is_phy_link_up(pdata))
						DWC_ETH_QOS_enable_ipa_offload(pdata);
		}
		break;
	case EV_IPA_UC_READY:
		{
			pdata->prv_ipa.ipa_uc_ready = true;
			EMACDBG("%s:%d ipa uC is ready\n", __func__, __LINE__);

			if (!pdata->prv_ipa.emac_dev_ready)
				break;

			if (DWC_ETH_QOS_is_phy_link_up(pdata))
				DWC_ETH_QOS_enable_ipa_offload(pdata);
		}
		break;
	case EV_DEV_CLOSE:
		{
			pdata->prv_ipa.emac_dev_ready = false;

			if(pdata->prv_ipa.ipa_uc_ready)
				ipa_uc_offload_dereg_rdyCB(IPA_UC_NTN);

			DWC_ETH_QOS_disable_ipa_offload(pdata);

			/* reset link down on dev close */
			pdata->prv_ipa.ipa_offload_link_down = 0;
		}
		break;
	case EV_DPM_SUSPEND:
		break;
	case EV_USR_SUSPEND:
		{
			if(!pdata->prv_ipa.ipa_offload_susp && !pdata->prv_ipa.ipa_offload_link_down)
				if(!DWC_ETH_QOS_ipa_offload_suspend(pdata))
					pdata->prv_ipa.ipa_offload_susp = true;
		}
		break;
	case EV_DPM_RESUME:
		{
			if(pdata->prv_ipa.ipa_offload_susp) {
				if (DWC_ETH_QOS_is_phy_link_up(pdata)) {
					if(!DWC_ETH_QOS_ipa_offload_resume(pdata))
						pdata->prv_ipa.ipa_offload_susp = false;
				} else {
					/* Reset flag here to allow connection of pipes on next PHY link up */
					pdata->prv_ipa.ipa_offload_susp = false;
					/* PHY link is down at resume */
					/* Reset flag here to allow connection of pipes on next PHY link up */
					pdata->prv_ipa.ipa_offload_link_down = true;
				}
			}
		}
		break;
	case EV_USR_RESUME:
		{
			if(pdata->prv_ipa.ipa_offload_susp) {
				if(!DWC_ETH_QOS_ipa_offload_resume(pdata))
					pdata->prv_ipa.ipa_offload_susp = false;
			}
		}
		break;
	case EV_INVALID:
	default:
		{

		}
		break;
	}

	EMACDBG("Exit: event=%s\n", IPA_OFFLOAD_EVENT_string[ev]);
	IPA_UNLOCK();
}

int DWC_ETH_QOS_enable_ipa_offload(struct DWC_ETH_QOS_prv_data *pdata)
{
	int ret = Y_SUCCESS;
	struct hw_if_struct *hw_if = &pdata->hw_if;

	if (!pdata->prv_ipa.ipa_offload_init) {
		ret = DWC_ETH_QOS_ipa_offload_init(pdata);
		if (ret) {
			pdata->prv_ipa.ipa_offload_init = false;
			EMACERR("IPA Offload Init Failed \n");
			goto fail;
		}
		EMACDBG("IPA Offload Initialized Successfully \n");
		pdata->prv_ipa.ipa_offload_init = true;
	}

	if (!pdata->prv_ipa.ipa_offload_conn && !pdata->prv_ipa.ipa_offload_susp) {
		ret = DWC_ETH_QOS_ipa_offload_connect(pdata);
		if (ret) {
			EMACERR("IPA Offload Connect Failed \n");
			pdata->prv_ipa.ipa_offload_conn = false;
			goto fail;
		}
		EMACDBG("IPA Offload Connect Successfully\n");
		pdata->prv_ipa.ipa_offload_conn = true;

		/*Initialize DMA CHs for offload*/
		ret = hw_if->init_offload(pdata);
		if (ret) {
			EMACERR("Offload channel Init Failed \n");
			goto fail;
		}
	}

	if (!pdata->prv_ipa.ipa_debugfs_exists) {
		if (!DWC_ETH_QOS_ipa_create_debugfs(pdata)) {
			EMACDBG("eMAC Debugfs created  \n");
			pdata->prv_ipa.ipa_debugfs_exists = true;
		} else EMACERR("eMAC Debugfs failed \n");
	}

	EMACDBG("IPA Offload Enabled successfully\n");
	return ret;

fail:
	if ( pdata->prv_ipa.ipa_offload_conn ) {
		if( DWC_ETH_QOS_ipa_offload_disconnect(pdata) )
			EMACERR("IPA Offload Disconnect Failed \n");
		else
			EMACDBG("IPA Offload Disconnect Successfully \n");
		pdata->prv_ipa.ipa_offload_conn = false;
	}

	if ( pdata->prv_ipa.ipa_offload_init ) {
		if ( DWC_ETH_QOS_ipa_offload_cleanup(pdata ))
			EMACERR("IPA Offload Cleanup Failed \n");
		else
			EMACDBG("IPA Offload Cleanup Success \n");
		pdata->prv_ipa.ipa_offload_init = false;
	}

	return ret;
}

int DWC_ETH_QOS_disable_ipa_offload(struct DWC_ETH_QOS_prv_data *pdata)
{
	int ret = Y_SUCCESS;

	EMACDBG("Enter \n");

	/* De-configure IPA Related Stuff */
	/* Not user requested suspend, do not set ipa_offload_susp */
	if (!pdata->prv_ipa.ipa_offload_susp && pdata->prv_ipa.ipa_offload_conn) {
		ret = DWC_ETH_QOS_ipa_offload_suspend(pdata);
		if (ret) {
			EMACERR("IPA Suspend Failed, err:%d\n", ret);
			return ret;
		}
	}

	if (pdata->prv_ipa.ipa_offload_init) {
		ret = DWC_ETH_QOS_ipa_offload_cleanup(pdata);
		if (ret) {
			EMACERR("IPA Offload Cleanup Failed, err: %d\n", ret);
			return ret;
		}
		EMACDBG("IPA Offload Cleanup Success \n");
		pdata->prv_ipa.ipa_offload_init = false;
	}

	if (pdata->prv_ipa.ipa_debugfs_exists) {
		if (DWC_ETH_QOS_ipa_cleanup_debugfs(pdata))
			EMACERR("Unable to delete IPA debugfs\n");
		else
			pdata->prv_ipa.ipa_debugfs_exists = false;
	}

	EMACDBG("Exit\n");

	return ret;
}

/* This function is called from IOCTL when new RX/TX properties have to
   be registered with IPA e.g VLAN hdr insertion deletion */
int DWC_ETH_QOS_disable_enable_ipa_offload(struct DWC_ETH_QOS_prv_data *pdata, int chInx_tx_ipa, int chInx_rx_ipa)
{
		struct hw_if_struct *hw_if = &(pdata->hw_if);
		int ret = Y_SUCCESS;

		/*stop the IPA owned DMA channels */
		ret = hw_if->stop_dma_rx(chInx_rx_ipa);
		if (ret != Y_SUCCESS) {
			   EMACERR("%s stop_dma_rx failed %d\n", __func__, ret);
			   return ret;
		}

		ret = hw_if->stop_dma_tx(chInx_tx_ipa);
		if (ret != Y_SUCCESS) {
			  EMACERR("%s stop_dma_tx failed %d\n", __func__, ret);
			  return ret;
		}

		/*disable IPA pipe first*/
		ret = DWC_ETH_QOS_disable_ipa_offload(pdata);
		if ( ret ){
					 EMACERR("%s:%d unable to disable ipa offload\n",
							   __func__, __LINE__);
					 return ret;
		}
		else {
			 hw_if->tx_desc_init(pdata, chInx_tx_ipa);
			 hw_if->rx_desc_init(pdata, chInx_rx_ipa);

			 /*if VLAN-id is passed, then make the VLAN+ETH hdr
			   and register the RX/TX properties*/
			 ret = DWC_ETH_QOS_enable_ipa_offload(pdata);
			 if (ret) {
					   EMACERR("%s:%d unable to enable ipa offload\n",
									  __func__, __LINE__);
					   return ret;
			 }
			 else {
				ret = hw_if->start_dma_tx(chInx_tx_ipa);
				if (ret != Y_SUCCESS) {
						EMACERR("%s start_dma_tx failed %d\n", __func__, ret);
						return ret;
				}

				ret = hw_if->start_dma_rx(chInx_rx_ipa);
				if (ret != Y_SUCCESS){
						EMACERR("%s start_dma_rx failed %d\n", __func__, ret);
						return ret;
				}
			 }
		}

		return ret;
}

/**
 * DWC_ETH_QOS_ipa_offload_suspend() - Suspend IPA offload data
 * path.
 *
 * IN: @pdata: NTN dirver private structure.
 * OUT: 0 on success and -1 on failure
 */
static int DWC_ETH_QOS_ipa_offload_suspend(struct DWC_ETH_QOS_prv_data *pdata)
{
	int ret = Y_SUCCESS;
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	struct ipa_perf_profile profile;

	EMACDBG("Suspend/disable IPA offload\n");

	ret = hw_if->stop_dma_rx(IPA_DMA_RX_CH);
	if (ret != Y_SUCCESS) {
		EMACERR("%s stop_dma_rx failed %d\n", __func__, ret);
		return ret;
	}

	/* Disconnect IPA offload */
	if (pdata->prv_ipa.ipa_offload_conn) {
		ret = DWC_ETH_QOS_ipa_offload_disconnect(pdata);
		if (ret) {
			EMACERR("IPA Offload Disconnect Failed, err:%d\n", ret);
			return ret;
		}
		pdata->prv_ipa.ipa_offload_conn = false;
		EMACDBG("IPA Offload Disconnect Successfully \n");
	}

	ret = hw_if->stop_dma_tx(IPA_DMA_TX_CH);

	if (ret != Y_SUCCESS) {
		EMACERR("%s stop_dma_tx failed %d\n", __func__, ret);
		return ret;
	}

	if (pdata->prv_ipa.ipa_uc_ready) {
		profile.max_supported_bw_mbps = IPA_PIPE_MIN_BW;
		profile.client = IPA_CLIENT_ETHERNET_CONS;
		ret = ipa_set_perf_profile(&profile);
		if (ret)
			EMACERR("Err to set BW: IPA_RM_RESOURCE_ETHERNET_CONS err:%d\n",
					ret);
	}

	return ret;
}

/**
 * DWC_ETH_QOS_ipa_offload_resume() - Resumes IPA offload data
 * path.
 *
 * IN: @pdata: NTN dirver private structure.
 * OUT: 0 on success and -1 on failure
 */
static int DWC_ETH_QOS_ipa_offload_resume(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	int ret = Y_SUCCESS;
	struct ipa_perf_profile profile;

	EMACDBG("Enter\n");

	/* Initialze descriptors before IPA connect */
	/* Set IPA owned DMA channels to reset state */
	hw_if->tx_desc_init(pdata, IPA_DMA_TX_CH);
	hw_if->rx_desc_init(pdata, IPA_DMA_RX_CH);

	EMACDBG("DWC_ETH_QOS_ipa_offload_connect\n");
	ret = DWC_ETH_QOS_ipa_offload_connect(pdata);
	if (ret != Y_SUCCESS)
		return ret;
	else
		pdata->prv_ipa.ipa_offload_conn = true;

	profile.max_supported_bw_mbps = pdata->speed;
	profile.client = IPA_CLIENT_ETHERNET_CONS;
	ret = ipa_set_perf_profile(&profile);
	if (ret)
		EMACERR("Err to set BW: IPA_RM_RESOURCE_ETHERNET_CONS err:%d\n",
				ret);
	/*Initialize DMA CHs for offload*/
	ret = hw_if->init_offload(pdata);
	if (ret) {
		EMACERR("Offload channel Init Failed \n");
		return ret;
	}

	EMACDBG("Exit\n");

	return ret;
}

static int DWC_ETH_QOS_ipa_ready(struct DWC_ETH_QOS_prv_data *pdata)
{
	int ret = 0 ;

	EMACDBG("Enter \n");

	if (pdata->prv_ipa.ipa_ver >= IPA_HW_v3_0) {
		ret = ipa_register_ipa_ready_cb(DWC_ETH_QOS_ipa_ready_cb,
										(void *)pdata);
		if (ret == -ENXIO) {
			EMACDBG("%s: IPA driver context is not even ready\n", __func__);
			return ret;
		}

		if (ret != -EEXIST) {
			EMACDBG("%s:%d register ipa ready cb\n", __func__, __LINE__);
			return ret;
		}
	}

	pdata->prv_ipa.ipa_ready = true;

	EMACDBG("Exit \n");

	return ret;
}

static int DWC_ETH_QOS_ipa_uc_ready(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct ipa_uc_ready_params param;
	int ret;

	EMACDBG("Enter \n");

	param.is_uC_ready = false;
	param.priv = pdata;
	param.notify = DWC_ETH_QOS_ipa_uc_ready_cb;
	param.proto = IPA_UC_NTN;

	ret = ipa_uc_offload_reg_rdyCB(&param);
	if (ret == 0 && param.is_uC_ready) {
		EMACDBG("%s:%d ipa uc ready\n", __func__, __LINE__);
		pdata->prv_ipa.ipa_uc_ready = true;
	}

	EMACDBG("Exit \n");
	return ret;
}

static void DWC_ETH_QOS_ipa_ready_wq(struct work_struct *work)
{
	struct DWC_ETH_QOS_prv_ipa_data *ntn_ipa = container_of(work,
				struct DWC_ETH_QOS_prv_ipa_data, ntn_ipa_rdy_work);
	struct DWC_ETH_QOS_prv_data *pdata = container_of(ntn_ipa,
					struct DWC_ETH_QOS_prv_data, prv_ipa);

	EMACDBG("%s:%d iDWC_ETH_QOS_ipa_ready_wq\n", __func__, __LINE__);
	DWC_ETH_QOS_ipa_offload_event_handler(pdata, EV_IPA_READY);
}

static void DWC_ETH_QOS_ipaUcRdy_wq(struct work_struct *work)
{
	struct DWC_ETH_QOS_prv_ipa_data *ntn_ipa = container_of(work,
					struct DWC_ETH_QOS_prv_ipa_data, ntn_ipa_rdy_work);
	struct DWC_ETH_QOS_prv_data *pdata = container_of(ntn_ipa,
					struct DWC_ETH_QOS_prv_data, prv_ipa);

	EMACDBG("%s:%d iDWC_ETH_QOS_ipa_uc_ready_wq\n", __func__, __LINE__);
	DWC_ETH_QOS_ipa_offload_event_handler(pdata, EV_IPA_UC_READY);

}
/**
 * DWC_ETH_QOS_ipa_ready_cb() - Callback register with IPA to
 * indicate if IPA is ready to receive configuration commands.
 * If IPA is not ready no IPA configuration commands should be
 * set.
 *
 * IN: @pdata: NTN private structure handle that will be passed by IPA.
 * OUT: NULL
 */
static void DWC_ETH_QOS_ipa_ready_cb(void *user_data)
{
	struct DWC_ETH_QOS_prv_data *pdata = (struct DWC_ETH_QOS_prv_data *)user_data;
	struct DWC_ETH_QOS_prv_ipa_data *ntn_ipa = &pdata->prv_ipa;

	if(!pdata) {
		EMACERR("%s Null Param pdata %p \n", __func__, pdata);
		return;
	}

	EMACDBG("%s Received IPA ready callback\n",__func__);
	INIT_WORK(&ntn_ipa->ntn_ipa_rdy_work, DWC_ETH_QOS_ipa_ready_wq);
	queue_work(system_unbound_wq, &ntn_ipa->ntn_ipa_rdy_work);
}

/**
 * DWC_ETH_QOS_ipa_uc_ready_cb() - Callback register with IPA to indicate
 * if IPA (and IPA uC) is ready to receive configuration commands.
 * If IPA is not ready no IPA configuration commands should be set.
 *
 * IN: @pdata: NTN private structure handle that will be passed by IPA.
 * OUT: NULL
 */
static void DWC_ETH_QOS_ipa_uc_ready_cb(void *user_data)
{
	struct DWC_ETH_QOS_prv_data *pdata = (struct DWC_ETH_QOS_prv_data *)user_data;
	struct DWC_ETH_QOS_prv_ipa_data *ntn_ipa = &pdata->prv_ipa;

	if(!pdata) {
		EMACERR("%s Null Param pdata %p \n", __func__, pdata);
		return;
	}

	EMACDBG("%s Received IPA UC ready callback\n",__func__);
	INIT_WORK(&ntn_ipa->ntn_ipa_rdy_work, DWC_ETH_QOS_ipaUcRdy_wq);
	queue_work(system_unbound_wq, &ntn_ipa->ntn_ipa_rdy_work);

	return;
}

/**
 * ntn_ipa_notify_cb() - Callback registered with IPA to handle data packets.
 *
 * IN: @priv:             Priv data passed to IPA at the time of registration
 * IN: @ipa_dp_evt_type:  IPA event type
 * IN: @data:             Data to be handled for this event.
 * OUT: NULL
 */
static void ntn_ipa_notify_cb(void *priv, enum ipa_dp_evt_type evt,
				unsigned long data)
{
	struct DWC_ETH_QOS_prv_data *pdata = (struct DWC_ETH_QOS_prv_data *)priv;
	struct DWC_ETH_QOS_prv_ipa_data *ntn_ipa = &pdata->prv_ipa;
	struct sk_buff *skb = (struct sk_buff *)data;
	struct iphdr *ip_hdr = NULL;
	int stat = NET_RX_SUCCESS;

	if(!pdata || !skb) {
		EMACERR("Null Param %s pdata %p skb %p \n", __func__, pdata, skb);
		return;
	}

	if(!ntn_ipa) {
		EMACERR( "Null Param %s ntn_ipa %p \n", __func__, ntn_ipa);
		return;
	}

	if (!ntn_ipa->ipa_offload_conn) {
		EMACERR("ipa_cb before offload is ready %s ipa_offload_conn %d  \n", __func__, ntn_ipa->ipa_offload_conn);
		return;
	}

	if (evt == IPA_RECEIVE) {
		/*Exception packets to network stack*/
		skb->dev = pdata->dev;
		skb_record_rx_queue(skb, IPA_DMA_RX_CH);
		skb->protocol = eth_type_trans(skb, skb->dev);
		ip_hdr = (struct iphdr *)(skb_mac_header(skb) + ETH_HLEN);

		/* Submit packet to network stack */
		/* If its a ping packet submit it via rx_ni else use rx */
		/* If NAPI is enabled call receive_skb */
		if(ipa_get_lan_rx_napi()){
			stat = netif_receive_skb(skb);
		} else {
			if (ip_hdr->protocol == IPPROTO_ICMP) {
				stat = netif_rx_ni(skb);
			} else if ((pdata->dev->stats.rx_packets %
				IPA_ETH_RX_SOFTIRQ_THRESH) == 0){
				stat = netif_rx_ni(skb);
			} else {
				stat = netif_rx(skb);
			}
		}
		if(stat == NET_RX_DROP) {
			pdata->dev->stats.rx_dropped++;
		} else {
			/* Update Statistics */
			pdata->ipa_stats.ipa_ul_exception++;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
			pdata->dev->last_rx = jiffies;
#endif
			pdata->dev->stats.rx_packets++;
			pdata->dev->stats.rx_bytes += skb->len;
		}
	} else {
		EMACERR("Unhandled Evt %d ",evt);
		dev_kfree_skb_any(skb);
		skb = NULL;
		pdata->dev->stats.tx_dropped++;
	}
}

/**
 * DWC_ETH_QOS_ipa_offload_init() - Called from NTN driver to initialize IPA
 * offload data path.
 * This function will add partial headers and register the interface with IPA.
 *
 * IN: @pdata: NTN private structure handle that will be passed by IPA.
 * OUT: 0 on success and -1 on failure
 */
static int DWC_ETH_QOS_ipa_offload_init(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct ipa_uc_offload_intf_params in;
	struct ipa_uc_offload_out_params out;
	struct DWC_ETH_QOS_prv_ipa_data *ntn_ipa = &pdata->prv_ipa;
	struct ethhdr eth_l2_hdr_v4;
	struct ethhdr eth_l2_hdr_v6;
#ifdef DWC_ETH_QOS_ENABLE_VLAN_TAG
	struct vlan_ethhdr eth_vlan_hdr_v4;
	struct vlan_ethhdr eth_vlan_hdr_v6;
#endif
	bool ipa_vlan_mode;
	int ret;

	if(!pdata) {
		EMACERR("%s: Null Param\n", __func__);
		return -1;
	}

	ret = ipa_is_vlan_mode(IPA_VLAN_IF_EMAC, &ipa_vlan_mode);
	if (ret) {
		EMACERR("Could not read ipa_vlan_mode\n");
		/* In case of failure, fallback to non vlan mode */
		ipa_vlan_mode = 0;
	}

	EMACDBG("IPA VLAN mode %d\n", ipa_vlan_mode);

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	/* Building ETH Header */
	if ( !pdata->prv_ipa.vlan_id || !ipa_vlan_mode) {
		memset(&eth_l2_hdr_v4, 0, sizeof(eth_l2_hdr_v4));
		memset(&eth_l2_hdr_v6, 0, sizeof(eth_l2_hdr_v6));
		memcpy(&eth_l2_hdr_v4.h_source, pdata->dev->dev_addr, ETH_ALEN);
		eth_l2_hdr_v4.h_proto = htons(ETH_P_IP);
		memcpy(&eth_l2_hdr_v6.h_source, pdata->dev->dev_addr, ETH_ALEN);
		eth_l2_hdr_v6.h_proto = htons(ETH_P_IPV6);
		in.hdr_info[0].hdr = (u8 *)&eth_l2_hdr_v4;
		in.hdr_info[0].hdr_len = ETH_HLEN;
		in.hdr_info[1].hdr = (u8 *)&eth_l2_hdr_v6;
		in.hdr_info[1].hdr_len = ETH_HLEN;
	}

#ifdef DWC_ETH_QOS_ENABLE_VLAN_TAG
	if ( (pdata->prv_ipa.vlan_id > MIN_VLAN_ID && pdata->prv_ipa.vlan_id <= MAX_VLAN_ID)
		 || ipa_vlan_mode) {
		memset(&eth_vlan_hdr_v4, 0, sizeof(eth_vlan_hdr_v4));
		memset(&eth_vlan_hdr_v6, 0, sizeof(eth_vlan_hdr_v6));
		memcpy(&eth_vlan_hdr_v4.h_source, pdata->dev->dev_addr, ETH_ALEN);
		eth_vlan_hdr_v4.h_vlan_proto = htons(ETH_P_8021Q);
		eth_vlan_hdr_v4.h_vlan_encapsulated_proto = htons(ETH_P_IP);
		in.hdr_info[0].hdr = (u8 *)&eth_vlan_hdr_v4;
		in.hdr_info[0].hdr_len = VLAN_ETH_HLEN;
		memcpy(&eth_vlan_hdr_v6.h_source, pdata->dev->dev_addr, ETH_ALEN);
		eth_vlan_hdr_v6.h_vlan_proto = htons(ETH_P_8021Q);
		eth_vlan_hdr_v6.h_vlan_encapsulated_proto = htons(ETH_P_IPV6);
		in.hdr_info[1].hdr = (u8 *)&eth_vlan_hdr_v6;
		in.hdr_info[1].hdr_len = VLAN_ETH_HLEN;
	}
#endif

	/* Building IN params */
	in.netdev_name = pdata->dev->name;
	in.priv = pdata;
	in.notify = ntn_ipa_notify_cb;
	in.proto = IPA_UC_NTN;
	in.hdr_info[0].dst_mac_addr_offset = 0;
	in.hdr_info[0].hdr_type = IPA_HDR_L2_ETHERNET_II;
	in.hdr_info[1].dst_mac_addr_offset = 0;
	in.hdr_info[1].hdr_type = IPA_HDR_L2_ETHERNET_II;

	ret = ipa_uc_offload_reg_intf(&in, &out);
	if (ret) {
		EMACERR("Could not register offload interface ret %d \n",ret);
		return -1;
	}
	ntn_ipa->ipa_client_hndl = out.clnt_hndl;
	EMACDBG("Recevied IPA Offload Client Handle %d",ntn_ipa->ipa_client_hndl);
	return 0;
}

/**
 * DWC_ETH_QOS_ipa_offload_cleanup() - Called from NTN driver to cleanup IPA
 * offload data path.
 *
 * IN: @pdata: NTN dirver private structure.
 * OUT: 0 on success and -1 on failure
 */
int DWC_ETH_QOS_ipa_offload_cleanup(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct DWC_ETH_QOS_prv_ipa_data *ntn_ipa = &pdata->prv_ipa;
	int ret = 0;

	EMACDBG("%s - begin\n", __func__);

	if (!pdata) {
		EMACERR("Null Param %s \n", __func__);
		return -1;
	}

	if (!ntn_ipa->ipa_client_hndl) {
		EMACERR("DWC_ETH_QOS_ipa_offload_cleanup called with NULL"
				" IPA client handle \n");
		return -1;
	}

	ret = ipa_uc_offload_cleanup(ntn_ipa->ipa_client_hndl);
	if (ret) {
		EMACERR("Could not cleanup IPA Offload ret %d\n",ret);
		return -1;
	}

	EMACDBG("%s - end\n", __func__);

	return 0;
}

/**
 * DWC_ETH_QOS_set_ul_dl_smmu_ipa_params() - This will set the
 * UL params in ipa_ntn_setup_info structure to be used in the
 * IPA connect
 * IN: @pdata: NTN private structure handle that will be passed
 * by IPA.
 * IN: @ul: pointer to ipa_ntn_setup_info uplink param.
 * OUT: 0 on success and -1 on failure
 */
int DWC_ETH_QOS_set_ul_dl_smmu_ipa_params(struct DWC_ETH_QOS_prv_data *pdata,
	struct ipa_ntn_setup_info *ul, struct ipa_ntn_setup_info *dl)
{
	int ret = 0;

	if(!pdata) {
		EMACERR( "Null Param %s \n", __func__);
		return -1;
	}

	if(!ul || !dl) {
		EMACERR( "Null UL DL params %s \n", __func__);
		return -1;
	}

	ul->ring_base_sgt = kzalloc(sizeof(ul->ring_base_sgt), GFP_KERNEL);
	if (!ul->ring_base_sgt) {
	EMACERR("Failed to allocate memory for IPA UL ring sgt\n");
	return -ENOMEM;
	}

	ret = dma_get_sgtable(GET_MEM_PDEV_DEV, ul->ring_base_sgt,
				GET_RX_DESC_PTR(IPA_DMA_RX_CH, 0),
				GET_RX_DESC_DMA_ADDR(IPA_DMA_RX_CH, 0),
				(sizeof(struct s_RX_NORMAL_DESC) *
				pdata->rx_queue[IPA_DMA_RX_CH].desc_cnt));
	if (ret) {
		EMACERR("Failed to get IPA UL ring sgtable.\n");
		kfree(ul->ring_base_sgt);
		ul->ring_base_sgt = NULL;
		return -1;
	} else {
		ul->ring_base_pa = sg_phys(ul->ring_base_sgt->sgl);
	}

	ul->buff_pool_base_sgt = kzalloc(sizeof(ul->buff_pool_base_sgt), GFP_KERNEL);
	if (!ul->buff_pool_base_sgt) {
		EMACERR("Failed to allocate memory for IPA UL buff pool sgt\n");
		return -ENOMEM;
	}

	ret = dma_get_sgtable(GET_MEM_PDEV_DEV, ul->buff_pool_base_sgt,
				GET_RX_BUFF_POOL_BASE_ADRR(IPA_DMA_RX_CH),
				GET_RX_BUFF_POOL_BASE_PADRR(IPA_DMA_RX_CH),
				(sizeof(struct s_RX_NORMAL_DESC) *
				pdata->rx_queue[IPA_DMA_RX_CH].desc_cnt));
	if (ret) {
		EMACERR("Failed to get IPA UL buff pool sgtable.\n");
		kfree(ul->buff_pool_base_sgt);
		ul->buff_pool_base_sgt = NULL;
		return -1;
	} else {
		ul->buff_pool_base_pa = sg_phys(ul->buff_pool_base_sgt->sgl);
	}

	dl->ring_base_sgt = kzalloc(sizeof (dl->ring_base_sgt), GFP_KERNEL);
	if (!dl->ring_base_sgt) {
		EMACERR("Failed to allocate memory for IPA DL ring sgt\n");
		return -ENOMEM;
	}

	ret = dma_get_sgtable(GET_MEM_PDEV_DEV, dl->ring_base_sgt,
				GET_TX_DESC_PTR(IPA_DMA_TX_CH, 0),
				GET_TX_DESC_DMA_ADDR(IPA_DMA_TX_CH, 0),
				(sizeof(struct s_TX_NORMAL_DESC) *
				pdata->tx_queue[IPA_DMA_TX_CH].desc_cnt));
	if (ret) {
		EMACERR("Failed to get IPA DL ring sgtable.\n");
		kfree(dl->ring_base_sgt);
		dl->ring_base_sgt = NULL;
		return -1;
	} else {
		dl->ring_base_pa = sg_phys(dl->ring_base_sgt->sgl);
	}

	dl->buff_pool_base_sgt = kzalloc(sizeof (dl->buff_pool_base_sgt), GFP_KERNEL);
	if (!dl->buff_pool_base_sgt) {
		EMACERR("Failed to allocate memory for IPA DL buff pool sgt\n");
		return -ENOMEM;
	}
	ret = dma_get_sgtable(GET_MEM_PDEV_DEV, dl->buff_pool_base_sgt,
				GET_TX_BUFF_POOL_BASE_ADRR(IPA_DMA_TX_CH),
				GET_TX_BUFF_POOL_BASE_PADRR(IPA_DMA_TX_CH),
				(sizeof(struct s_TX_NORMAL_DESC) *
				pdata->tx_queue[IPA_DMA_TX_CH].desc_cnt));
	if (ret) {
		EMACERR("Failed to get IPA DL buff pool sgtable.\n");
		kfree(dl->buff_pool_base_sgt);
		dl->buff_pool_base_sgt = NULL;
		return -1;
	} else {
		dl->buff_pool_base_pa = sg_phys(dl->buff_pool_base_sgt->sgl);
	}
	return ret;
}

/**
 * DWC_ETH_QOS_ipa_offload_connect() - Called from NTN driver to connect IPA
 * offload data path. This function should be called from NTN driver after
 * allocation of rings and resources required for offload data path.
 *
 * After this function is called host driver should be ready to receive
 * any packets send by IPA.
 *
 * IN: @pdata: NTN private structure handle that will be passed by IPA.
 * OUT: 0 on success and -1 on failure
 */
static int DWC_ETH_QOS_ipa_offload_connect(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct DWC_ETH_QOS_prv_ipa_data *ntn_ipa = &pdata->prv_ipa;
	struct ipa_uc_offload_conn_in_params in;
	struct ipa_uc_offload_conn_out_params out;
	struct ipa_ntn_setup_info rx_setup_info = {0};
	struct ipa_ntn_setup_info tx_setup_info = {0};
	struct ipa_perf_profile profile;
	int ret = 0;
	int i = 0;
	u32 reg_val;


	EMACDBG("%s - begin\n", __func__);

	if(!pdata) {
		EMACERR( "Null Param %s \n", __func__);
		return -1;
	}

	/* Configure interrupt route for EMAC TX DMA channel to IPA */
	RGMII_GPIO_CFG_TX_INT_UDFWR(IPA_DMA_TX_CH);

	/* Configure interrupt route for EMAC RX DMA channel to IPA */
	RGMII_GPIO_CFG_RX_INT_UDFWR(IPA_DMA_RX_CH);

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));
	memset(&profile, 0, sizeof(profile));

	in.clnt_hndl = ntn_ipa->ipa_client_hndl;
	/* Uplink Setup */
	if (emac_emb_smmu_ctx.valid) {
		rx_setup_info.smmu_enabled = true;
	} else {
		rx_setup_info.smmu_enabled = false;
	}
	rx_setup_info.client = IPA_CLIENT_ETHERNET_PROD;
	if (!rx_setup_info.smmu_enabled)
		rx_setup_info.ring_base_pa = (phys_addr_t)GET_RX_DESC_DMA_ADDR(IPA_DMA_RX_CH, 0);
	rx_setup_info.ring_base_iova = GET_RX_DESC_DMA_ADDR(IPA_DMA_RX_CH, 0);
	rx_setup_info.ntn_ring_size = pdata->rx_queue[IPA_DMA_RX_CH].desc_cnt;
	if (!rx_setup_info.smmu_enabled)
		rx_setup_info.buff_pool_base_pa = GET_RX_BUFF_POOL_BASE_PADRR(IPA_DMA_RX_CH);
	rx_setup_info.buff_pool_base_iova = GET_RX_BUFF_POOL_BASE_PADRR(IPA_DMA_RX_CH);
	rx_setup_info.num_buffers = pdata->rx_queue[IPA_DMA_RX_CH].desc_cnt - 1;
	rx_setup_info.data_buff_size = DWC_ETH_QOS_ETH_FRAME_LEN_IPA;

	/* Base address here is the address of EMAC_DMA_CH0_CONTROL in EMAC resgister space */
	rx_setup_info.ntn_reg_base_ptr_pa = (phys_addr_t)(((ULONG)((ULONG)DMA_CR0_RGOFFADDR - BASE_ADDRESS))
	  + (ULONG)dwc_eth_qos_res_data.emac_mem_base);

	/* Downlink Setup */
	if (emac_emb_smmu_ctx.valid) {
		tx_setup_info.smmu_enabled = true;
	} else {
		tx_setup_info.smmu_enabled = false;
	}
	tx_setup_info.client = IPA_CLIENT_ETHERNET_CONS;
	if (!tx_setup_info.smmu_enabled) {
		tx_setup_info.ring_base_pa = (phys_addr_t)GET_TX_DESC_DMA_ADDR(IPA_DMA_TX_CH, 0);
	}
	tx_setup_info.ring_base_iova = GET_TX_DESC_DMA_ADDR(IPA_DMA_TX_CH, 0);
	tx_setup_info.ntn_ring_size = pdata->tx_queue[IPA_DMA_TX_CH].desc_cnt;
	if (!tx_setup_info.smmu_enabled)
		tx_setup_info.buff_pool_base_pa = GET_TX_BUFF_POOL_BASE_PADRR(IPA_DMA_TX_CH);
	tx_setup_info.buff_pool_base_iova = GET_TX_BUFF_POOL_BASE_PADRR(IPA_DMA_TX_CH);
	tx_setup_info.num_buffers = pdata->tx_queue[IPA_DMA_TX_CH].desc_cnt - 1;
	tx_setup_info.data_buff_size = DWC_ETH_QOS_ETH_FRAME_LEN_IPA;

	/* Base address here is the address of EMAC_DMA_CH0_CONTROL in EMAC resgister space */
	tx_setup_info.ntn_reg_base_ptr_pa = (phys_addr_t)  (((ULONG)((ULONG)DMA_CR0_RGOFFADDR - BASE_ADDRESS))
	  + (ULONG)dwc_eth_qos_res_data.emac_mem_base);

	rx_setup_info.data_buff_list = kcalloc(rx_setup_info.num_buffers,
				sizeof(struct ntn_buff_smmu_map), GFP_KERNEL);
	if (rx_setup_info.data_buff_list == NULL) {
		EMACERR("Failed to allocate mem for Rx data_buff_list");
		ret = -ENOMEM;
		goto mem_free;
	}
	tx_setup_info.data_buff_list = kcalloc(tx_setup_info.num_buffers,
				sizeof(struct ntn_buff_smmu_map), GFP_KERNEL);
	if (tx_setup_info.data_buff_list == NULL) {
		EMACERR("Failed to allocate mem for Tx data_buff_list");
		ret = -ENOMEM;
		goto mem_free;
	}

	for (i = 0; i < rx_setup_info.num_buffers; i++) {
		rx_setup_info.data_buff_list[i].iova = GET_RX_BUFF_DMA_ADDR(IPA_DMA_RX_CH, i);
		if (!rx_setup_info.smmu_enabled)
			rx_setup_info.data_buff_list[i].pa = rx_setup_info.data_buff_list[i].iova;
		else
			rx_setup_info.data_buff_list[i].pa = GET_RX_BUF_PTR(IPA_DMA_RX_CH, i)->ipa_rx_buff_phy_addr;
	}
	for (i = 0; i < tx_setup_info.num_buffers; i++) {
		tx_setup_info.data_buff_list[i].iova = GET_TX_BUFF_DMA_ADDR(IPA_DMA_TX_CH, i);
		if (!tx_setup_info.smmu_enabled)
			tx_setup_info.data_buff_list[i].pa = tx_setup_info.data_buff_list[i].iova;
		else
			tx_setup_info.data_buff_list[i].pa = GET_TX_BUF_PTR(IPA_DMA_TX_CH, i)->ipa_tx_buff_phy_addr;
	}

	if (emac_emb_smmu_ctx.valid) {
		ret = DWC_ETH_QOS_set_ul_dl_smmu_ipa_params(pdata, &rx_setup_info, &tx_setup_info);
		if (ret) {
			EMACERR("Failed to build UL DL ipa_ntn_setup_info err:%d\n", ret);
			ret = -1;
			goto mem_free;
		}
	}

	/* Dump UL and DL Setups */
	EMACDBG("IPA Offload UL client %d ring_base_pa 0x%x ntn_ring_size %d buff_pool_base_pa 0x%x num_buffers %d data_buff_size %d ntn_reg_base_ptr_pa 0x%x\n",
		rx_setup_info.client, rx_setup_info.ring_base_pa, rx_setup_info.ntn_ring_size, rx_setup_info.buff_pool_base_pa, rx_setup_info.num_buffers, rx_setup_info.data_buff_size, rx_setup_info.ntn_reg_base_ptr_pa);
	EMACDBG("IPA Offload DL client %d ring_base_pa 0x%x ntn_ring_size %d buff_pool_base_pa 0x%x num_buffers %d data_buff_size %d ntn_reg_base_ptr_pa 0x%x\n",
		tx_setup_info.client, tx_setup_info.ring_base_pa, tx_setup_info.ntn_ring_size, tx_setup_info.buff_pool_base_pa, tx_setup_info.num_buffers, tx_setup_info.data_buff_size, tx_setup_info.ntn_reg_base_ptr_pa);

	in.u.ntn.ul = rx_setup_info;
	in.u.ntn.dl = tx_setup_info;

	ret = ipa_uc_offload_conn_pipes(&in, &out);
	if (ret) {
		EMACERR("Could not connect IPA Offload Pipes %d\n", ret);
		ret = -1;
		goto mem_free;
	}

    /* Mapped RX queue 0 to DMA channel 0 on successful IPA offload connect */
    MTL_RQDCM0R_RGWR(0x3020100);

	/* Mapped RX queue 0 to DMA channel 0 on successful IPA offload connect */
	if (pdata->res_data->early_eth_en) {
		MTL_RQDCM0R_RGRD(reg_val);
		reg_val &= ~IPA_RX_TO_DMA_CH_MAP_NUM;
		MTL_RQDCM0R_RGWR(reg_val);
	}

    ntn_ipa->uc_db_rx_addr = out.u.ntn.ul_uc_db_pa;
    ntn_ipa->uc_db_tx_addr = out.u.ntn.dl_uc_db_pa;

	/* Set Perf Profile For PROD/CONS Pipes */
	profile.max_supported_bw_mbps = pdata->speed;
	profile.client = IPA_CLIENT_ETHERNET_PROD;
	ret = ipa_set_perf_profile (&profile);
	if (ret) {
		EMACERR("Err to set BW: IPA_RM_RESOURCE_ETHERNET_PROD err:%d\n",
				ret);
		ret = -1;
		goto mem_free;
	}

	profile.client = IPA_CLIENT_ETHERNET_CONS;
	ret = ipa_set_perf_profile (&profile);
	if (ret) {
		EMACERR("Err to set BW: IPA_RM_RESOURCE_ETHERNET_CONS err:%d\n",
				ret);
		ret = -1;
		goto mem_free;
	}

 mem_free:
	if (rx_setup_info.data_buff_list) {
		kfree(rx_setup_info.data_buff_list);
		rx_setup_info.data_buff_list = NULL;
	}
	if (tx_setup_info.data_buff_list) {
		kfree(tx_setup_info.data_buff_list);
		tx_setup_info.data_buff_list = NULL;
	}
	if (emac_emb_smmu_ctx.valid) {
		if (rx_setup_info.ring_base_sgt) {
			sg_free_table(rx_setup_info.ring_base_sgt);
			kfree(rx_setup_info.ring_base_sgt);
			rx_setup_info.ring_base_sgt = NULL;
		}
		if (tx_setup_info.ring_base_sgt) {
			sg_free_table(tx_setup_info.ring_base_sgt);
			kfree(tx_setup_info.ring_base_sgt);
			tx_setup_info.ring_base_sgt = NULL;
		}
		if (rx_setup_info.buff_pool_base_sgt) {
			sg_free_table(rx_setup_info.buff_pool_base_sgt);
			kfree(rx_setup_info.buff_pool_base_sgt);
			rx_setup_info.buff_pool_base_sgt = NULL;
		}
		if (tx_setup_info.buff_pool_base_sgt) {
			sg_free_table(tx_setup_info.buff_pool_base_sgt);
			kfree(tx_setup_info.buff_pool_base_sgt);
			tx_setup_info.buff_pool_base_sgt = NULL;
		}
	}

	EMACDBG("%s - end \n", __func__);
	return 0;
}

/**
 * DWC_ETH_QOS_ipa_offload_disconnect() - Called from NTN driver to disconnect IPA
 * offload data path. This function should be called from NTN driver before
 * de-allocation of any ring resources.
 *
 * After this function is successful, NTN is free to de-allocate the IPA controlled
 * DMA rings.
 *
 * IN: @pdata: NTN dirver private structure.
 * OUT: 0 on success and -1 on failure
 */
static int DWC_ETH_QOS_ipa_offload_disconnect(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct DWC_ETH_QOS_prv_ipa_data *ntn_ipa = &pdata->prv_ipa;
	int ret = 0;

	EMACDBG("%s - begin \n", __func__);

	if(!pdata) {
		EMACERR( "Null Param %s \n", __func__);
		return -1;
	}

	ret = ipa_uc_offload_disconn_pipes(ntn_ipa->ipa_client_hndl);
	if (ret) {
		EMACERR("Could not cleanup IPA Offload ret %d\n",ret);
		return ret;
	}

	EMACDBG("%s - end \n", __func__);
	return 0;
}

/**
 * read_ipa_stats() - Debugfs read command for IPA statistics
 *
 */
static ssize_t read_ipa_stats(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct DWC_ETH_QOS_prv_data *pdata = file->private_data;
	struct DWC_ETH_QOS_prv_ipa_data *ntn_ipa = &pdata->prv_ipa;
	char *buf;
	unsigned int len = 0, buf_len = 2000;
	ssize_t ret_cnt;

	if (!pdata || !ntn_ipa) {
		EMACERR(" %s NULL Pointer \n",__func__);
		return -EINVAL;
	}

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, buf_len - len, "\n \n");
	len += scnprintf(buf + len, buf_len - len, "%25s\n",
		"NTN IPA Stats");
	len += scnprintf(buf + len, buf_len - len, "%25s\n\n",
		"==================================================");

	len += scnprintf(buf + len, buf_len - len, "%-25s %10llu\n",
		"IPA RX Packets: ", pdata->ipa_stats.ipa_ul_exception);
	len += scnprintf(buf + len, buf_len - len, "\n");

	if (len > buf_len)
		len = buf_len;

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret_cnt;
}

void DWC_ETH_QOS_ipa_stats_read(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct DWC_ETH_QOS_ipa_stats *dma_stats = &pdata->ipa_stats;
	UINT data;

	if (!pdata->rx_queue || !pdata->tx_queue)
		return;

	dma_stats->ipa_rx_Desc_Ring_Base = GET_RX_DESC_DMA_ADDR(IPA_DMA_RX_CH, 0);
	dma_stats->ipa_rx_Desc_Ring_Size = pdata->rx_queue[IPA_DMA_RX_CH].desc_cnt;
	dma_stats->ipa_rx_Buff_Ring_Base = GET_RX_BUFF_POOL_BASE_PADRR(IPA_DMA_RX_CH);
	dma_stats->ipa_rx_Buff_Ring_Size = pdata->rx_queue[IPA_DMA_RX_CH].desc_cnt - 1;
	
	//@RK: IPA_INTEG Need Rx db received cnt from IPA uC  
	dma_stats->ipa_rx_Db_Int_Raised = 0;

	DMA_CHRDR_RGRD(IPA_DMA_RX_CH, data);
	dma_stats->ipa_rx_Cur_Desc_Ptr_Indx = GET_RX_DESC_IDX(IPA_DMA_RX_CH, data);

	DMA_RDTP_RPDR_RGRD(IPA_DMA_RX_CH, data);
	dma_stats->ipa_rx_Tail_Ptr_Indx = GET_RX_DESC_IDX(IPA_DMA_RX_CH, data);

	DMA_SR_RGRD(IPA_DMA_RX_CH, data);
	dma_stats->ipa_rx_DMA_Status = data;

	dma_stats->ipa_rx_DMA_Ch_underflow =
		GET_VALUE(data, DMA_SR_RBU_LPOS, DMA_SR_RBU_HPOS);

	dma_stats->ipa_rx_DMA_Ch_stopped =
		GET_VALUE(data, DMA_SR_RPS_LPOS, DMA_SR_RPS_HPOS);

	dma_stats->ipa_rx_DMA_Ch_complete =
		GET_VALUE(data, DMA_SR_RI_LPOS, DMA_SR_RI_HPOS);

	DMA_IER_RGRD(IPA_DMA_RX_CH, dma_stats->ipa_rx_Int_Mask); 
	DMA_IER_RIE_UDFRD(IPA_DMA_RX_CH,
			dma_stats->ipa_rx_Transfer_Complete_irq);

	DMA_IER_RSE_UDFRD(IPA_DMA_RX_CH,
			dma_stats->ipa_rx_Transfer_Stopped_irq);

	DMA_IER_RBUE_UDFRD(IPA_DMA_RX_CH, dma_stats->ipa_rx_Underflow_irq);
	DMA_IER_ETIE_UDFRD(IPA_DMA_RX_CH,
				dma_stats->ipa_rx_Early_Trans_Comp_irq);

	dma_stats->ipa_tx_Desc_Ring_Base = GET_TX_DESC_DMA_ADDR(IPA_DMA_TX_CH, 0);
	dma_stats->ipa_tx_Desc_Ring_Size = pdata->tx_queue[IPA_DMA_TX_CH].desc_cnt;
	dma_stats->ipa_tx_Buff_Ring_Base = GET_TX_BUFF_POOL_BASE_PADRR(IPA_DMA_TX_CH);
	dma_stats->ipa_tx_Buff_Ring_Size = pdata->tx_queue[IPA_DMA_TX_CH].desc_cnt - 1;
	
	//@RK: IPA_INTEG Need Tx db received cnt from IPA uC
	dma_stats->ipa_tx_Db_Int_Raised = 0;
						  
	DMA_CHTDR_RGRD(IPA_DMA_TX_CH, data);
	dma_stats->ipa_tx_Curr_Desc_Ptr_Indx = GET_TX_DESC_IDX(IPA_DMA_TX_CH, data);

	DMA_TDTP_TPDR_RGRD(IPA_DMA_TX_CH, data);
	dma_stats->ipa_tx_Tail_Ptr_Indx = GET_TX_DESC_IDX(IPA_DMA_TX_CH, data);

	DMA_SR_RGRD(IPA_DMA_TX_CH, data);
	dma_stats->ipa_tx_DMA_Status = data;
	
	dma_stats->ipa_tx_DMA_Ch_underflow =
		GET_VALUE(data, DMA_SR_TBU_LPOS, DMA_SR_TBU_HPOS);

	dma_stats->ipa_tx_DMA_Transfer_stopped =
		GET_VALUE(data, DMA_SR_TPS_LPOS, DMA_SR_TPS_HPOS);

	dma_stats->ipa_tx_DMA_Transfer_complete =
		GET_VALUE(data, DMA_SR_TI_LPOS, DMA_SR_TI_HPOS);

	DMA_IER_RGRD(IPA_DMA_TX_CH, dma_stats->ipa_tx_Int_Mask);
	DMA_IER_TIE_UDFRD(IPA_DMA_TX_CH, dma_stats->ipa_tx_Transfer_Complete_irq);  

	DMA_IER_TXSE_UDFRD(IPA_DMA_TX_CH, dma_stats->ipa_tx_Transfer_Stopped_irq);

	DMA_IER_TBUE_UDFRD(IPA_DMA_TX_CH,
				dma_stats->ipa_tx_Underflow_irq);

	DMA_IER_ETIE_UDFRD(IPA_DMA_TX_CH, dma_stats->ipa_tx_Early_Trans_Cmp_irq);
	DMA_IER_FBEE_UDFRD(IPA_DMA_TX_CH, dma_stats->ipa_tx_Fatal_err_irq);
	DMA_IER_CDEE_UDFRD(IPA_DMA_TX_CH, dma_stats->ipa_tx_Desc_Err_irq);
}

/**
 * read_ntn_dma_stats() - Debugfs read command for NTN DMA statistics
 * Only read DMA Stats for IPA Control Channels
 *
 */
static ssize_t read_ntn_dma_stats(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct DWC_ETH_QOS_prv_data *pdata = file->private_data;
	struct DWC_ETH_QOS_prv_ipa_data *ntn_ipa = &pdata->prv_ipa;
	struct DWC_ETH_QOS_ipa_stats *dma_stats = &pdata->ipa_stats;
	char *buf;
	unsigned int len = 0, buf_len = 3000;
	ssize_t ret_cnt;

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	DWC_ETH_QOS_ipa_stats_read(pdata);

	len += scnprintf(buf + len, buf_len - len, "\n \n");
	len += scnprintf(buf + len, buf_len - len, "%25s\n",
		"NTN DMA Stats");
	len += scnprintf(buf + len, buf_len - len, "%25s\n\n",
		"==================================================");

	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
		"RX Desc Ring Base: ", dma_stats->ipa_rx_Desc_Ring_Base);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10d\n",
		"RX Desc Ring Size: ", dma_stats->ipa_rx_Desc_Ring_Size);
	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
		"RX Buff Ring Base: ", dma_stats->ipa_rx_Buff_Ring_Base);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10d\n",
		"RX Buff Ring Size: ", dma_stats->ipa_rx_Buff_Ring_Size);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10u\n",
		"RX Doorbell Interrupts Raised: ", dma_stats->ipa_rx_Db_Int_Raised);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10d\n",
		"RX Current Desc Pointer Index: ", dma_stats->ipa_rx_Cur_Desc_Ptr_Indx);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10d\n",
		"RX Tail Pointer Index: ", dma_stats->ipa_rx_Tail_Ptr_Indx);
	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
		"RX Doorbell Address: ", ntn_ipa->uc_db_rx_addr);
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
		"RX DMA Status: ", dma_stats->ipa_rx_DMA_Status);

	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
		"RX DMA Status - RX DMA Underflow : ",
			bit_status_string[dma_stats->ipa_rx_DMA_Ch_underflow]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
		"RX DMA Status - RX DMA Stopped : ",
			bit_status_string[dma_stats->ipa_rx_DMA_Ch_stopped]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
		"RX DMA Status - RX DMA Complete : ",
			bit_status_string[dma_stats->ipa_rx_DMA_Ch_complete]);
	len += scnprintf(buf + len, buf_len - len, "\n");


	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
		"RX DMA CH0 INT Mask: ", dma_stats->ipa_rx_Int_Mask);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
		"RXDMACH0 INTMASK - Transfer Complete IRQ : ",
			bit_mask_string[dma_stats->ipa_rx_Transfer_Complete_irq]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
		"RXDMACH0 INTMASK - Transfer Stopped IRQ : ",
			bit_mask_string[dma_stats->ipa_rx_Transfer_Stopped_irq]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
		"RXDMACH0 INTMASK - Underflow IRQ : ",
			bit_mask_string[dma_stats->ipa_rx_Underflow_irq]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
		"RXDMACH0 INTMASK - Early Transmit Complete IRQ : ",
			bit_mask_string[dma_stats->ipa_rx_Early_Trans_Comp_irq]);
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
		"TX Desc Ring Base: ", dma_stats->ipa_tx_Desc_Ring_Base);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10d\n",
		"TX Desc Ring Size: ", dma_stats->ipa_tx_Desc_Ring_Size);
	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
		"TX Buff Ring Base: ", dma_stats->ipa_tx_Buff_Ring_Base);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10d\n",
		"TX Buff Ring Size: ", dma_stats->ipa_tx_Buff_Ring_Size);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10u\n",
		"TX Doorbell Interrupts Raised: ", dma_stats->ipa_tx_Db_Int_Raised);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10lu\n",
		"TX Current Desc Pointer Index: ", dma_stats->ipa_tx_Curr_Desc_Ptr_Indx);

	len += scnprintf(buf + len, buf_len - len, "%-50s %10lu\n",
		"TX Tail Pointer Index: ", dma_stats->ipa_tx_Tail_Ptr_Indx);
	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
		"TX Doorbell Address: ", ntn_ipa->uc_db_tx_addr);
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
		"TX DMA Status: ", dma_stats->ipa_tx_DMA_Status);

	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
		"TX DMA Status - TX DMA Underflow : ",
				bit_status_string[dma_stats->ipa_tx_DMA_Ch_underflow]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
		"TX DMA Status - TX DMA Transfer Stopped : ",
				bit_status_string[dma_stats->ipa_tx_DMA_Transfer_stopped]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
		"TX DMA Status - TX DMA Transfer Complete : ",
				bit_status_string[dma_stats->ipa_tx_DMA_Transfer_complete]);
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
		"TX DMA CH2 INT Mask: ", dma_stats->ipa_tx_Int_Mask);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
		"TXDMACH2 INTMASK - Transfer Complete IRQ : ",
				bit_mask_string[dma_stats->ipa_tx_Transfer_Complete_irq]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
		"TXDMACH2 INTMASK - Transfer Stopped IRQ : ",
				bit_mask_string[dma_stats->ipa_tx_Transfer_Stopped_irq]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
		"TXDMACH2 INTMASK - Underflow IRQ : ", bit_mask_string[dma_stats->ipa_tx_Underflow_irq]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
		"TXDMACH2 INTMASK - Early Transmit Complete IRQ : ",
				bit_mask_string[dma_stats->ipa_tx_Early_Trans_Cmp_irq]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
		"TXDMACH2 INTMASK - Fatal Bus Error IRQ : ",
				bit_mask_string[dma_stats->ipa_tx_Fatal_err_irq]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
		"TXDMACH2 INTMASK - CNTX Desc Error IRQ : ",
				bit_mask_string[dma_stats->ipa_tx_Desc_Err_irq]);

	if (len > buf_len)
		len = buf_len;

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret_cnt;
}

static ssize_t read_ipa_offload_status(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	unsigned int len = 0, buf_len = NTN_IPA_DBG_MAX_MSG_LEN;
	struct DWC_ETH_QOS_prv_data *pdata = file->private_data;

	if (DWC_ETH_QOS_is_phy_link_up(pdata)) {
		if (pdata->prv_ipa.ipa_offload_susp)
			len += scnprintf(buf + len, buf_len - len, "IPA Offload suspended\n");
		else
			len += scnprintf(buf + len, buf_len - len, "IPA Offload enabled\n");
	} else {
		len += scnprintf(buf + len, buf_len - len, "Cannot read status, No PHY link\n");
	}

	if (len > buf_len)
		len = buf_len;

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t suspend_resume_ipa_offload(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	s8 option = 0;
	char in_buf[2];
	unsigned long ret;
	struct DWC_ETH_QOS_prv_data *pdata = file->private_data;

	if (sizeof(in_buf) < 2)
		return -EFAULT;

	ret = copy_from_user(in_buf, user_buf, 1);
	if (ret)
		return -EFAULT;

	in_buf[1] = '\0';
	if (kstrtos8(in_buf, 0, &option))
		return -EFAULT;

	if (DWC_ETH_QOS_is_phy_link_up(pdata)) {
		if (option == 1) DWC_ETH_QOS_ipa_offload_event_handler(pdata, EV_USR_SUSPEND);
		else if (option == 0) DWC_ETH_QOS_ipa_offload_event_handler(pdata, EV_USR_RESUME);
	} else {
		EMACERR("Operation not permitted, No PHY link");
	}

	return count;
}


static const struct file_operations fops_ipa_stats = {
	.read = read_ipa_stats,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct file_operations fops_ntn_dma_stats = {
	.read = read_ntn_dma_stats,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct file_operations fops_ntn_ipa_offload_en = {
	.read = read_ipa_offload_status,
	.write = suspend_resume_ipa_offload,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};
/**
 * DWC_ETH_QOS_ipa_create_debugfs() - Called from NTN driver to create debugfs node
 * for offload data path debugging.
 *
 * IN: @pdata: NTN dirver private structure.
 * OUT: 0 on success and -1 on failure
 */
int DWC_ETH_QOS_ipa_create_debugfs(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct DWC_ETH_QOS_prv_ipa_data *ntn_ipa = &pdata->prv_ipa;

	if(!pdata || !pdata->debugfs_dir) {
		EMACERR( "Null Param %s \n", __func__);
		return -1;
	}

	ntn_ipa->debugfs_ipa_stats =
		debugfs_create_file("ipa_stats", S_IRUSR, pdata->debugfs_dir,
				pdata, &fops_ipa_stats);
	if (!ntn_ipa->debugfs_ipa_stats || IS_ERR(ntn_ipa->debugfs_ipa_stats)) {
		EMACERR( "Cannot create debugfs ipa_stats %d \n",
				 (int)ntn_ipa->debugfs_ipa_stats);
		goto fail;
	}

	ntn_ipa->debugfs_dma_stats =
		debugfs_create_file("dma_stats", S_IRUSR, pdata->debugfs_dir,
				pdata, &fops_ntn_dma_stats);
	if (!ntn_ipa->debugfs_dma_stats || IS_ERR(ntn_ipa->debugfs_dma_stats)) {
		EMACERR( "Cannot create debugfs dma_stats %d \n",
				 (int)ntn_ipa->debugfs_dma_stats);
		goto fail;
	}

	ntn_ipa->debugfs_suspend_ipa_offload =
		debugfs_create_file("suspend_ipa_offload", (S_IRUSR|S_IWUSR),
				pdata->debugfs_dir, pdata, &fops_ntn_ipa_offload_en);
	if (!ntn_ipa->debugfs_suspend_ipa_offload
		|| IS_ERR(ntn_ipa->debugfs_suspend_ipa_offload)) {
		EMACERR( "Cannot create debugfs ipa_offload_en %d \n",
				 (int)ntn_ipa->debugfs_suspend_ipa_offload);
		goto fail;
	}
	return 0;

fail:
	DWC_ETH_QOS_ipa_cleanup_debugfs(pdata);
	return -ENOMEM;
}

/**
 * DWC_ETH_QOS_ipa_cleanup_debugfs() - Called from NTN driver to cleanup debugfs node
 * for offload data path debugging.
 *
 * IN: @pdata: NTN dirver private structure.
 * OUT: 0 on success and -1 on failure
 */
int DWC_ETH_QOS_ipa_cleanup_debugfs(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct DWC_ETH_QOS_prv_ipa_data *ntn_ipa= &pdata->prv_ipa;

	if(!pdata || !ntn_ipa) {
		EMACERR("Null Param %s \n", __func__);
		return -1;
	}

	if (pdata->debugfs_dir) {
		if (ntn_ipa->debugfs_ipa_stats) {
			debugfs_remove(ntn_ipa->debugfs_ipa_stats);
			ntn_ipa->debugfs_ipa_stats = NULL;
		}

		if (ntn_ipa->debugfs_dma_stats) {
			debugfs_remove(ntn_ipa->debugfs_dma_stats);
			ntn_ipa->debugfs_dma_stats = NULL;

		}

		if (ntn_ipa->debugfs_suspend_ipa_offload) {
			debugfs_remove(ntn_ipa->debugfs_suspend_ipa_offload);
			ntn_ipa->debugfs_suspend_ipa_offload = NULL;
		}
	}

	EMACDBG("IPA debugfs Deleted Successfully \n");
	return 0;
}

