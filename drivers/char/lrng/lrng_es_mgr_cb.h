/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 *
 * Definition of an entropy source.
 */

#ifndef _LRNG_ES_MGR_CB_H
#define _LRNG_ES_MGR_CB_H

#include <linux/lrng.h>

#include "lrng_definitions.h"
#include "lrng_drng_mgr.h"

enum lrng_internal_es {
#ifdef CONFIG_LRNG_IRQ
	lrng_int_es_irq,			/* IRQ-based entropy source */
#endif
#ifdef CONFIG_LRNG_SCHED
	lrng_int_es_sched,			/* Scheduler entropy source */
#endif
	lrng_int_es_last,			/* MUST be the last entry */
};

enum lrng_external_es {
	lrng_ext_link = lrng_int_es_last - 1,	/* Link entry */
#ifdef CONFIG_LRNG_JENT
	lrng_ext_es_jitter,			/* Jitter RNG */
#endif
#ifdef CONFIG_LRNG_CPU
	lrng_ext_es_cpu,			/* CPU-based, e.g. RDSEED */
#endif
#ifdef CONFIG_LRNG_KERNEL_RNG
	lrng_ext_es_krng,			/* random.c */
#endif
	lrng_ext_es_aux,			/* MUST BE LAST ES! */
	lrng_ext_es_last			/* MUST be the last entry */
};

struct entropy_buf {
	u8 e[lrng_ext_es_last][LRNG_DRNG_INIT_SEED_SIZE_BYTES];
	u32 now, e_bits[lrng_ext_es_last];
};

/*
 * struct lrng_es_cb - callback defining an entropy source
 * @name: Name of the entropy source.
 * @get_ent: Fetch entropy into the entropy_buf. The ES shall only deliver
 *	     data if its internal initialization is complete, including any
 *	     SP800-90B startup testing or similar.
 * @curr_entropy: Return amount of currently available entropy.
 * @max_entropy: Maximum amount of entropy the entropy source is able to
 *		 maintain.
 * @state: Buffer with human-readable ES state.
 * @reset: Reset entropy source (drop all entropy and reinitialize).
 *	   This callback may be NULL.
 * @switch_hash: callback to switch from an old hash callback definition to
 *		 a new one. This callback may be NULL.
 */
struct lrng_es_cb {
	const char *name;
	void (*get_ent)(struct entropy_buf *eb, u32 requested_bits,
			bool fully_seeded);
	u32 (*curr_entropy)(u32 requested_bits);
	u32 (*max_entropy)(void);
	void (*state)(unsigned char *buf, size_t buflen);
	void (*reset)(void);
	int (*switch_hash)(struct lrng_drng *drng, int node,
			   const struct lrng_hash_cb *new_cb, void *new_hash,
			   const struct lrng_hash_cb *old_cb);
};

/* Allow entropy sources to tell the ES manager that new entropy is there */
void lrng_es_add_entropy(void);

/* Cap to maximum entropy that can ever be generated with given hash */
#define lrng_cap_requested(__digestsize_bits, __requested_bits)		\
	do {								\
		if (__digestsize_bits < __requested_bits) {		\
			pr_debug("Cannot satisfy requested entropy %u due to insufficient hash size %u\n",\
				 __requested_bits, __digestsize_bits);	\
			__requested_bits = __digestsize_bits;		\
		}							\
	} while (0)

#endif /* _LRNG_ES_MGR_CB_H */
