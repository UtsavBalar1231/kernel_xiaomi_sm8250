/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#ifndef _LRNG_DRNG_ATOMIC_H
#define _LRNG_DRNG_ATOMIC_H

#include "lrng_drng_mgr.h"

#ifdef CONFIG_LRNG_DRNG_ATOMIC
void lrng_drng_atomic_reset(void);
void lrng_drng_atomic_seed_drng(struct lrng_drng *drng);
void lrng_drng_atomic_seed_es(void);
void lrng_drng_atomic_force_reseed(void);
#else /* CONFIG_LRNG_DRNG_ATOMIC */
static inline void lrng_drng_atomic_reset(void) { }
static inline void lrng_drng_atomic_seed_drng(struct lrng_drng *drng) { }
static inline void lrng_drng_atomic_seed_es(void) { }
static inline void lrng_drng_atomic_force_reseed(void) { }
#endif /* CONFIG_LRNG_DRNG_ATOMIC */

#endif /* _LRNG_DRNG_ATOMIC_H */
