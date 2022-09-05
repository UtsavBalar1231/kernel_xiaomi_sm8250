/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * LRNG ChaCha20 definitions
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#ifndef _LRNG_CHACHA20_H
#define _LRNG_CHACHA20_H

#include <crypto/chacha.h>

/* State according to RFC 7539 section 2.3 */
struct chacha20_block {
	u32 constants[4];
	union {
#define CHACHA_KEY_SIZE_WORDS (CHACHA_KEY_SIZE / sizeof(u32))
		u32 u[CHACHA_KEY_SIZE_WORDS];
		u8  b[CHACHA_KEY_SIZE];
	} key;
	u32 counter;
	u32 nonce[3];
};

struct chacha20_state {
	struct chacha20_block block;
};

static inline void lrng_cc20_init_rfc7539(struct chacha20_block *chacha20)
{
	chacha_init_consts(chacha20->constants);
}

#define LRNG_CC20_INIT_RFC7539(x) \
	x.constants[0] = 0x61707865, \
	x.constants[1] = 0x3320646e, \
	x.constants[2] = 0x79622d32, \
	x.constants[3] = 0x6b206574,

extern const struct lrng_drng_cb lrng_cc20_drng_cb;

#endif /* _LRNG_CHACHA20_H */
