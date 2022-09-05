// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * Backend for the LRNG providing the SHA-1 implementation that can be used
 * without the kernel crypto API available including during early boot and in
 * atomic contexts.
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/lrng.h>
#include <crypto/sha1.h>
#include <crypto/sha1_base.h>

#include "lrng_sha.h"

/*
 * If the SHA-256 support is not compiled, we fall back to SHA-1 that is always
 * compiled and present in the kernel.
 */
static u32 lrng_sha1_hash_digestsize(void *hash)
{
	return SHA1_DIGEST_SIZE;
}

static void lrng_sha1_block_fn(struct sha1_state *sctx, const u8 *src,
			       int blocks)
{
	u32 temp[SHA1_WORKSPACE_WORDS];

	while (blocks--) {
		sha_transform(sctx->state, src, temp);
		src += SHA1_BLOCK_SIZE;
	}
	memzero_explicit(temp, sizeof(temp));
}

static int lrng_sha1_hash_init(struct shash_desc *shash, void *hash)
{
	/*
	 * We do not need a TFM - we only need sufficient space for
	 * struct sha1_state on the stack.
	 */
	sha1_base_init(shash);
	return 0;
}

static int lrng_sha1_hash_update(struct shash_desc *shash,
				 const u8 *inbuf, u32 inbuflen)
{
	return sha1_base_do_update(shash, inbuf, inbuflen, lrng_sha1_block_fn);
}

static int lrng_sha1_hash_final(struct shash_desc *shash, u8 *digest)
{
	return sha1_base_do_finalize(shash, lrng_sha1_block_fn) ?:
	       sha1_base_finish(shash, digest);
}

static const char *lrng_sha1_hash_name(void)
{
	return "SHA-1";
}

static void lrng_sha1_hash_desc_zero(struct shash_desc *shash)
{
	memzero_explicit(shash_desc_ctx(shash), sizeof(struct sha1_state));
}

static void *lrng_sha1_hash_alloc(void)
{
	pr_info("Hash %s allocated\n", lrng_sha1_hash_name());
	return NULL;
}

static void lrng_sha1_hash_dealloc(void *hash) { }

const struct lrng_hash_cb lrng_sha_hash_cb = {
	.hash_name		= lrng_sha1_hash_name,
	.hash_alloc		= lrng_sha1_hash_alloc,
	.hash_dealloc		= lrng_sha1_hash_dealloc,
	.hash_digestsize	= lrng_sha1_hash_digestsize,
	.hash_init		= lrng_sha1_hash_init,
	.hash_update		= lrng_sha1_hash_update,
	.hash_final		= lrng_sha1_hash_final,
	.hash_desc_zero		= lrng_sha1_hash_desc_zero,
};
