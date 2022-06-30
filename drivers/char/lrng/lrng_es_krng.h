/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#ifndef _LRNG_ES_RANDOM_H
#define _LRNG_ES_RANDOM_H

#include "lrng_es_mgr_cb.h"

#ifdef CONFIG_LRNG_KERNEL_RNG

extern struct lrng_es_cb lrng_es_krng;

#endif /* CONFIG_LRNG_KERNEL_RNG */

#endif /* _LRNG_ES_RANDOM_H */
