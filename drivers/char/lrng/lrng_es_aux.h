/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#ifndef _LRNG_ES_AUX_H
#define _LRNG_ES_AUX_H

#include "lrng_drng_mgr.h"
#include "lrng_es_mgr_cb.h"

u32 lrng_get_digestsize(void);
void lrng_pool_set_entropy(u32 entropy_bits);
int lrng_pool_insert_aux(const u8 *inbuf, u32 inbuflen, u32 entropy_bits);

extern struct lrng_es_cb lrng_es_aux;

/****************************** Helper code ***********************************/

/* Obtain the security strength of the LRNG in bits */
static inline u32 lrng_security_strength(void)
{
	/*
	 * We use a hash to read the entropy in the entropy pool. According to
	 * SP800-90B table 1, the entropy can be at most the digest size.
	 * Considering this together with the last sentence in section 3.1.5.1.2
	 * the security strength of a (approved) hash is equal to its output
	 * size. On the other hand the entropy cannot be larger than the
	 * security strength of the used DRBG.
	 */
	return min_t(u32, LRNG_FULL_SEED_ENTROPY_BITS, lrng_get_digestsize());
}

static inline u32 lrng_get_seed_entropy_osr(bool fully_seeded)
{
	u32 requested_bits = lrng_security_strength();

	/* Apply oversampling during initialization according to SP800-90C */
	if (lrng_sp80090c_compliant() && !fully_seeded)
		requested_bits += LRNG_SEED_BUFFER_INIT_ADD_BITS;
	return requested_bits;
}

#endif /* _LRNG_ES_AUX_H */
