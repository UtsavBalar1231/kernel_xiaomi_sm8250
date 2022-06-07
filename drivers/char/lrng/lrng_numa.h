/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#ifndef _LRNG_NUMA_H
#define _LRNG_NUMA_H

#ifdef CONFIG_NUMA
struct lrng_drng **lrng_drng_instances(void);
#else	/* CONFIG_NUMA */
static inline struct lrng_drng **lrng_drng_instances(void) { return NULL; }
#endif /* CONFIG_NUMA */

#endif /* _LRNG_NUMA_H */
