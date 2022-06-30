// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * Backend for providing the hash primitive using the kernel crypto API.
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/lrng.h>
#include <crypto/hash.h>
#include <linux/module.h>


static char *lrng_hash_name = "sha512";

/* The parameter must be r/o in sysfs as otherwise races appear. */
module_param(lrng_hash_name, charp, 0444);
MODULE_PARM_DESC(lrng_hash_name, "Kernel crypto API hash name");

struct lrng_hash_info {
	struct crypto_shash *tfm;
};

static const char *lrng_kcapi_hash_name(void)
{
	return lrng_hash_name;
}

static void _lrng_kcapi_hash_free(struct lrng_hash_info *lrng_hash)
{
	struct crypto_shash *tfm = lrng_hash->tfm;

	crypto_free_shash(tfm);
	kfree(lrng_hash);
}

static void *lrng_kcapi_hash_alloc(const char *name)
{
	struct lrng_hash_info *lrng_hash;
	struct crypto_shash *tfm;
	int ret;

	if (!name) {
		pr_err("Hash name missing\n");
		return ERR_PTR(-EINVAL);
	}

	tfm = crypto_alloc_shash(name, 0, 0);
	if (IS_ERR(tfm)) {
		pr_err("could not allocate hash %s\n", name);
		return ERR_CAST(tfm);
	}

	ret = sizeof(struct lrng_hash_info);
	lrng_hash = kmalloc(ret, GFP_KERNEL);
	if (!lrng_hash) {
		crypto_free_shash(tfm);
		return ERR_PTR(-ENOMEM);
	}

	lrng_hash->tfm = tfm;

	pr_info("Hash %s allocated\n", name);

	return lrng_hash;
}

static void *lrng_kcapi_hash_name_alloc(void)
{
	return lrng_kcapi_hash_alloc(lrng_kcapi_hash_name());
}

static u32 lrng_kcapi_hash_digestsize(void *hash)
{
	struct lrng_hash_info *lrng_hash = (struct lrng_hash_info *)hash;
	struct crypto_shash *tfm = lrng_hash->tfm;

	return crypto_shash_digestsize(tfm);
}

static void lrng_kcapi_hash_dealloc(void *hash)
{
	struct lrng_hash_info *lrng_hash = (struct lrng_hash_info *)hash;

	_lrng_kcapi_hash_free(lrng_hash);
	pr_info("Hash deallocated\n");
}

static int lrng_kcapi_hash_init(struct shash_desc *shash, void *hash)
{
	struct lrng_hash_info *lrng_hash = (struct lrng_hash_info *)hash;
	struct crypto_shash *tfm = lrng_hash->tfm;

	shash->tfm = tfm;
	return crypto_shash_init(shash);
}

static int lrng_kcapi_hash_update(struct shash_desc *shash, const u8 *inbuf,
			   u32 inbuflen)
{
	return crypto_shash_update(shash, inbuf, inbuflen);
}

static int lrng_kcapi_hash_final(struct shash_desc *shash, u8 *digest)
{
	return crypto_shash_final(shash, digest);
}

static void lrng_kcapi_hash_zero(struct shash_desc *shash)
{
	shash_desc_zero(shash);
}

static const struct lrng_hash_cb lrng_kcapi_hash_cb = {
	.hash_name		= lrng_kcapi_hash_name,
	.hash_alloc		= lrng_kcapi_hash_name_alloc,
	.hash_dealloc		= lrng_kcapi_hash_dealloc,
	.hash_digestsize	= lrng_kcapi_hash_digestsize,
	.hash_init		= lrng_kcapi_hash_init,
	.hash_update		= lrng_kcapi_hash_update,
	.hash_final		= lrng_kcapi_hash_final,
	.hash_desc_zero		= lrng_kcapi_hash_zero,
};

static int __init lrng_kcapi_init(void)
{
	return lrng_set_hash_cb(&lrng_kcapi_hash_cb);
}

static void __exit lrng_kcapi_exit(void)
{
	lrng_set_hash_cb(NULL);
}

late_initcall(lrng_kcapi_init);
module_exit(lrng_kcapi_exit);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Stephan Mueller <smueller@chronox.de>");
MODULE_DESCRIPTION("Entropy Source and DRNG Manager - Kernel crypto API hash backend");
