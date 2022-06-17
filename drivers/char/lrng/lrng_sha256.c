// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * Backend for the LRNG providing the SHA-256 implementation that can be used
 * without the kernel crypto API available including during early boot and in
 * atomic contexts.
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/lrng.h>
#include <linux/sha256.h>

#include "lrng_sha.h"

static u32 lrng_sha256_hash_digestsize(void *hash)
{
	return SHA256_DIGEST_SIZE;
}

static int lrng_sha256_hash_init(struct shash_desc *shash, void *hash)
{
	/*
	 * We do not need a TFM - we only need sufficient space for
	 * struct sha256_state on the stack.
	 */
	sha256_init(shash_desc_ctx(shash));
	return 0;
}

static int lrng_sha256_hash_update(struct shash_desc *shash,
				   const u8 *inbuf, u32 inbuflen)
{
	sha256_update(shash_desc_ctx(shash), inbuf, inbuflen);
	return 0;
}

static int lrng_sha256_hash_final(struct shash_desc *shash, u8 *digest)
{
	sha256_final(shash_desc_ctx(shash), digest);
	return 0;
}

static const char *lrng_sha256_hash_name(void)
{
	return "SHA-256";
}

static void lrng_sha256_hash_desc_zero(struct shash_desc *shash)
{
	memzero_explicit(shash_desc_ctx(shash), sizeof(struct sha256_state));
}

static void *lrng_sha256_hash_alloc(void)
{
	pr_info("Hash %s allocated\n", lrng_sha256_hash_name());
	return NULL;
}

static void lrng_sha256_hash_dealloc(void *hash) { }

const struct lrng_hash_cb lrng_sha_hash_cb = {
	.hash_name		= lrng_sha256_hash_name,
	.hash_alloc		= lrng_sha256_hash_alloc,
	.hash_dealloc		= lrng_sha256_hash_dealloc,
	.hash_digestsize	= lrng_sha256_hash_digestsize,
	.hash_init		= lrng_sha256_hash_init,
	.hash_update		= lrng_sha256_hash_update,
	.hash_final		= lrng_sha256_hash_final,
	.hash_desc_zero		= lrng_sha256_hash_desc_zero,
};
