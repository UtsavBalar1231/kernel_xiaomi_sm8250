/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#ifndef _LRNG_ES_SCHED_H
#define _LRNG_ES_SCHED_H

#include "lrng_es_mgr_cb.h"

#ifdef CONFIG_LRNG_SCHED
void lrng_sched_es_init(bool highres_timer);

extern struct lrng_es_cb lrng_es_sched;

#else /* CONFIG_LRNG_SCHED */
static inline void lrng_sched_es_init(bool highres_timer) { }
#endif /* CONFIG_LRNG_SCHED */

#endif /* _LRNG_ES_SCHED_H */
