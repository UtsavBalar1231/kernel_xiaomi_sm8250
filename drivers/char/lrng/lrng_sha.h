/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * LRNG SHA definition usable in atomic contexts right from the start of the
 * kernel.
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#ifndef _LRNG_SHA_H
#define _LRNG_SHA_H

extern const struct lrng_hash_cb lrng_sha_hash_cb;

#endif /* _LRNG_SHA_H */
