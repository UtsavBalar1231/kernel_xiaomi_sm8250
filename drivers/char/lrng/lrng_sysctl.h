/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#ifndef _LRNG_SYSCTL_H
#define _LRNG_SYSCTL_H

#ifdef CONFIG_LRNG_SYSCTL
void lrng_sysctl_update_max_write_thresh(u32 new_digestsize);
#else
static inline void lrng_sysctl_update_max_write_thresh(u32 new_digestsize) { }
#endif

#endif /* _LRNG_SYSCTL_H */
