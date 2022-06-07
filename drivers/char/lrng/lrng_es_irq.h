/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#ifndef _LRNG_ES_IRQ_H
#define _LRNG_ES_IRQ_H

#include <linux/lrng.h>

#include "lrng_es_mgr_cb.h"

#ifdef CONFIG_LRNG_IRQ
void lrng_irq_es_init(bool highres_timer);
void lrng_irq_array_add_u32(u32 data);

extern struct lrng_es_cb lrng_es_irq;

#else /* CONFIG_LRNG_IRQ */
static inline void lrng_irq_es_init(bool highres_timer) { }
static inline void lrng_irq_array_add_u32(u32 data) { }
#endif /* CONFIG_LRNG_IRQ */

#endif /* _LRNG_ES_IRQ_H */
