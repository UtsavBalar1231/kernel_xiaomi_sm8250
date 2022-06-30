/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#ifndef _LRNG_HEALTH_H
#define _LRNG_HEALTH_H

#include "lrng_es_mgr.h"

enum lrng_health_res {
	lrng_health_pass,		/* Health test passes on time stamp */
	lrng_health_fail_use,		/* Time stamp unhealthy, but mix in */
	lrng_health_fail_drop		/* Time stamp unhealthy, drop it */
};

#ifdef CONFIG_LRNG_HEALTH_TESTS
bool lrng_sp80090b_startup_complete_es(enum lrng_internal_es es);
bool lrng_sp80090b_compliant(enum lrng_internal_es es);

enum lrng_health_res lrng_health_test(u32 now_time, enum lrng_internal_es es);
void lrng_health_disable(void);
#else	/* CONFIG_LRNG_HEALTH_TESTS */
static inline bool lrng_sp80090b_startup_complete_es(enum lrng_internal_es es)
{
	return true;
}

static inline bool lrng_sp80090b_compliant(enum lrng_internal_es es)
{
	return false;
}

static inline enum lrng_health_res
lrng_health_test(u32 now_time, enum lrng_internal_es es)
{
	return lrng_health_pass;
}
static inline void lrng_health_disable(void) { }
#endif	/* CONFIG_LRNG_HEALTH_TESTS */

#endif /* _LRNG_HEALTH_H */
