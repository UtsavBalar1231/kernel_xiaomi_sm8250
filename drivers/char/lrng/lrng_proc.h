/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#ifndef _LRNG_PROC_H
#define _LRNG_PROC_H

#ifdef CONFIG_SYSCTL
void lrng_pool_inc_numa_node(void);
#else
static inline void lrng_pool_inc_numa_node(void) { }
#endif

#endif /* _LRNG_PROC_H */
