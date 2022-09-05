/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#ifndef _LRNG_INTERFACE_RANDOM_H
#define _LRNG_INTERFACE_RANDOM_H

#ifdef CONFIG_LRNG_RANDOM_IF
void lrng_process_ready_list(void);
bool lrng_ready_chain_has_sleeper(void);
void invalidate_batched_entropy(void);
#else /* CONFIG_LRNG_RANDOM_IF */
static inline bool lrng_ready_chain_has_sleeper(void) { return false; }
static inline void lrng_process_ready_list(void) { }
static inline void invalidate_batched_entropy(void) { }
#endif /* CONFIG_LRNG_RANDOM_IF */

#endif /* _LRNG_INTERFACE_RANDOM_H */
