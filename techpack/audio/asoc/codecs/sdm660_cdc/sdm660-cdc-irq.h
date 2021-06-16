/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2018, 2020, The Linux Foundation. All rights reserved.
 */
#ifndef __WCD9XXX_SPMI_IRQ_H__
#define __WCD9XXX_SPMI_IRQ_H__

#include <sound/soc.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/pm_qos.h>

extern void wcd9xxx_spmi_enable_irq(int irq);
extern void wcd9xxx_spmi_disable_irq(int irq);
extern int wcd9xxx_spmi_request_irq(int irq, irq_handler_t handler,
				const char *name, void *priv);
extern int wcd9xxx_spmi_free_irq(int irq, void *priv);
extern void wcd9xxx_spmi_set_codec(struct snd_soc_component *component);
extern void wcd9xxx_spmi_set_dev(struct platform_device *spmi, int i);
extern int wcd9xxx_spmi_irq_init(void);
extern void wcd9xxx_spmi_irq_exit(void);
extern int wcd9xxx_spmi_suspend(pm_message_t pmesg);
extern int wcd9xxx_spmi_resume(void);
bool wcd9xxx_spmi_lock_sleep(void);
void wcd9xxx_spmi_unlock_sleep(void);

#endif
