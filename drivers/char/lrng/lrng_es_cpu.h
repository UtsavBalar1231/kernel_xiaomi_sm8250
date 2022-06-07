/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#ifndef _LRNG_ES_CPU_H
#define _LRNG_ES_CPU_H

#include "lrng_es_mgr_cb.h"

#ifdef CONFIG_LRNG_CPU

extern struct lrng_es_cb lrng_es_cpu;

#endif /* CONFIG_LRNG_CPU */

#endif /* _LRNG_ES_CPU_H */
