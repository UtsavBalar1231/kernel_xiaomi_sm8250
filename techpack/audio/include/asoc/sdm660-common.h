/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2019, 2020, The Linux Foundation. All rights reserved.
 */

#ifndef __MSM_COMMON
#define __MSM_COMMON

#include <sound/soc.h>
#include <dsp/q6afe-v2.h>
#include <asoc/wcd-mbhc-v2.h>

#define DEFAULT_MCLK_RATE 9600000
#define NATIVE_MCLK_RATE 11289600

#define SAMPLING_RATE_8KHZ      8000
#define SAMPLING_RATE_11P025KHZ 11025
#define SAMPLING_RATE_16KHZ     16000
#define SAMPLING_RATE_22P05KHZ  22050
#define SAMPLING_RATE_32KHZ     32000
#define SAMPLING_RATE_44P1KHZ   44100
#define SAMPLING_RATE_48KHZ     48000
#define SAMPLING_RATE_88P2KHZ   88200
#define SAMPLING_RATE_96KHZ     96000
#define SAMPLING_RATE_176P4KHZ  176400
#define SAMPLING_RATE_192KHZ    192000
#define SAMPLING_RATE_352P8KHZ  352800
#define SAMPLING_RATE_384KHZ    384000

#define TDM_CHANNEL_MAX 16
#define TDM_SLOT_OFFSET_MAX 32

enum {
	TDM_0 = 0,
	TDM_1,
	TDM_2,
	TDM_3,
	TDM_4,
	TDM_5,
	TDM_6,
	TDM_7,
	TDM_PORT_MAX,
};

enum {
	TDM_PRI = 0,
	TDM_SEC,
	TDM_TERT,
	TDM_QUAT,
	TDM_QUIN,
	TDM_INTERFACE_MAX,
};

struct tdm_port {
	u32 mode;
	u32 channel;
};

struct dev_config {
	u32 sample_rate;
	u32 bit_format;
	u32 channels;
};

enum {
	PRIM_MI2S = 0,
	SEC_MI2S,
	TERT_MI2S,
	QUAT_MI2S,
	QUIN_MI2S,
	MI2S_MAX,
};

enum {
	DIG_CDC,
	ANA_CDC,
	CODECS_MAX,
};

extern const struct snd_kcontrol_new msm_common_snd_controls[];
extern bool codec_reg_done;
struct sdm660_codec {
	void* (*get_afe_config_fn)(struct snd_soc_codec *codec,
				   enum afe_config_type config_type);
};

enum {
	INT_SND_CARD,
	EXT_SND_CARD_TASHA,
	EXT_SND_CARD_TAVIL,
};

struct msm_snd_interrupt {
	void __iomem *mpm_wakeup;
	void __iomem *intr1_cfg_apps;
	void __iomem *lpi_gpio_intr_cfg;
	void __iomem *lpi_gpio_cfg;
	void __iomem *lpi_gpio_inout;
};

struct msm_asoc_mach_data {
	int us_euro_gpio; /* used by gpio driver API */
	int usbc_en2_gpio; /* used by gpio driver API */
	int hph_en1_gpio;
	int hph_en0_gpio;
	struct device_node *us_euro_gpio_p; /* used by pinctrl API */
	struct pinctrl *usbc_en2_gpio_p; /* used by pinctrl API */
	struct device_node *hph_en1_gpio_p; /* used by pinctrl API */
	struct device_node *hph_en0_gpio_p; /* used by pinctrl API */
	struct device_node *pdm_gpio_p; /* used by pinctrl API */
	struct device_node *comp_gpio_p; /* used by pinctrl API */
	struct device_node *dmic_gpio_p; /* used by pinctrl API */
	struct device_node *ext_spk_gpio_p; /* used by pinctrl API */
	struct device_node *mi2s_gpio_p[MI2S_MAX]; /* used by pinctrl API */
	struct snd_soc_codec *codec;
	struct sdm660_codec sdm660_codec_fn;
	struct snd_info_entry *codec_root;
	int spk_ext_pa_gpio;
	int mclk_freq;
	bool native_clk_set;
	int lb_mode;
	int snd_card_val;
	u8 micbias1_cap_mode;
	u8 micbias2_cap_mode;
	atomic_t int_mclk0_rsc_ref;
	atomic_t int_mclk0_enabled;
	struct mutex cdc_int_mclk0_mutex;
	struct delayed_work disable_int_mclk0_work;
	struct afe_clk_set digital_cdc_core_clk;
	struct msm_snd_interrupt msm_snd_intr_lpi;
};

int msm_common_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				  struct snd_pcm_hw_params *params);
int msm_aux_pcm_snd_startup(struct snd_pcm_substream *substream);
void msm_aux_pcm_snd_shutdown(struct snd_pcm_substream *substream);
int msm_mi2s_snd_startup(struct snd_pcm_substream *substream);
void msm_mi2s_snd_shutdown(struct snd_pcm_substream *substream);
int msm_common_snd_controls_size(void);
void msm_set_codec_reg_done(bool done);
int msm_tdm_snd_startup(struct snd_pcm_substream *substream);
void msm_tdm_snd_shutdown(struct snd_pcm_substream *substream);
int msm_tdm_snd_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params);
int msm_tdm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				      struct snd_pcm_hw_params *params);
#endif
