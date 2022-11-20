/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2017, 2020, The Linux Foundation. All rights reserved.
 */
#ifndef MSM_DIGITAL_CDC_H
#define MSM_DIGITAL_CDC_H

#define HPHL_PA_DISABLE (0x01 << 1)
#define HPHR_PA_DISABLE (0x01 << 2)
#define SPKR_PA_DISABLE (0x01 << 3)

#define NUM_DECIMATORS	5
/* Codec supports 1 compander */
enum {
	COMPANDER_NONE = 0,
	COMPANDER_1, /* HPHL/R */
	COMPANDER_MAX,
};

/* Number of output I2S port */
enum {
	MSM89XX_RX1 = 0,
	MSM89XX_RX2,
	MSM89XX_RX3,
	MSM89XX_RX_MAX,
};

struct tx_mute_work {
	struct msm_dig_priv *dig_cdc;
	u32 decimator;
	struct delayed_work dwork;
};

struct msm_dig_priv {
	struct snd_soc_component *component;
	u32 comp_enabled[MSM89XX_RX_MAX];
	int (*codec_hph_comp_gpio)(bool enable,
					struct snd_soc_component *component);
	s32 dmic_1_2_clk_cnt;
	s32 dmic_3_4_clk_cnt;
	bool dec_active[NUM_DECIMATORS];
	int version;
	/* Entry for version info */
	struct snd_info_entry *entry;
	struct snd_info_entry *version_entry;
	char __iomem *dig_base;
	struct regmap *regmap;
	struct notifier_block nblock;
	u32 mute_mask;
	int dapm_bias_off;
	void *handle;
	void (*set_compander_mode)(void *handle, int val);
	void (*update_clkdiv)(void *handle, int val);
	int (*get_cdc_version)(void *handle);
	int (*register_notifier)(void *handle,
				 struct notifier_block *nblock,
				 bool enable);
	struct tx_mute_work tx_mute_dwork[NUM_DECIMATORS];
};

struct dig_ctrl_platform_data {
	void *handle;
	void (*set_compander_mode)(void *handle, int val);
	void (*update_clkdiv)(void *handle, int val);
	int (*get_cdc_version)(void *handle);
	int (*register_notifier)(void *handle,
				 struct notifier_block *nblock,
				 bool enable);
};

struct hpf_work {
	struct msm_dig_priv *dig_cdc;
	u32 decimator;
	u8 tx_hpf_cut_of_freq;
	struct delayed_work dwork;
};

/* Codec supports 5 bands */
enum {
	BAND1 = 0,
	BAND2,
	BAND3,
	BAND4,
	BAND5,
	BAND_MAX,
};

#if IS_ENABLED(CONFIG_SND_SOC_DIGITAL_CDC)
extern void msm_dig_cdc_hph_comp_cb(
		int (*codec_hph_comp_gpio)(
			bool enable, struct snd_soc_component *component),
		struct snd_soc_component *component);
int msm_dig_codec_info_create_codec_entry(struct snd_info_entry *codec_root,
					  struct snd_soc_component *component);
#else /* CONFIG_SND_SOC_DIGITAL_CDC */
static inline void msm_dig_cdc_hph_comp_cb(
		int (*codec_hph_comp_gpio)(
			bool enable, struct snd_soc_component *component),
		struct snd_soc_component *component)
{

}
static inline int msm_dig_codec_info_create_codec_entry(
				struct snd_info_entry *codec_root,
				struct snd_soc_component *component)
{
	return 0;
}
#endif /* CONFIG_SND_SOC_DIGITAL_CDC */
#endif
