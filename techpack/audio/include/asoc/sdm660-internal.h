/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2017, 2020, The Linux Foundation. All rights reserved.
 */

#ifndef __SDM660_INTERNAL
#define __SDM660_INTERNAL

#include <sound/soc.h>

#if IS_ENABLED(CONFIG_SND_SOC_INT_CODEC)
int msm_int_cdc_init(struct platform_device *pdev,
		     struct msm_asoc_mach_data *pdata,
		     struct snd_soc_card **card,
		     struct wcd_mbhc_config *mbhc_cfg);
#else
int msm_int_cdc_init(struct platform_device *pdev,
		     struct msm_asoc_mach_data *pdata,
		     struct snd_soc_card **card,
		     struct wcd_mbhc_config *mbhc_cfg)
{
	return 0;
}
#endif
#endif
