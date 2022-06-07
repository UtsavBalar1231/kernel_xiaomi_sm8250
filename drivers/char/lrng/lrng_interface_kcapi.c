// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * LRNG interface with the RNG framework of the kernel crypto API
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#include <linux/lrng.h>
#include <linux/module.h>
#include <crypto/internal/rng.h>

#include "lrng_drng_mgr.h"
#include "lrng_es_aux.h"

static int lrng_kcapi_if_init(struct crypto_tfm *tfm)
{
	return 0;
}

static void lrng_kcapi_if_cleanup(struct crypto_tfm *tfm) { }

static int lrng_kcapi_if_reseed(const u8 *src, unsigned int slen)
{
	int ret;

	if (!slen)
		return 0;

	/* Insert caller-provided data without crediting entropy */
	ret = lrng_pool_insert_aux((u8 *)src, slen, 0);
	if (ret)
		return ret;

	/* Make sure the new data is immediately available to DRNG */
	lrng_drng_force_reseed();

	return 0;
}

static int lrng_kcapi_if_random(struct crypto_rng *tfm,
				const u8 *src, unsigned int slen,
				u8 *rdata, unsigned int dlen)
{
	int ret = lrng_kcapi_if_reseed(src, slen);

	if (!ret)
		lrng_get_random_bytes_full(rdata, dlen);

	return ret;
}

static int lrng_kcapi_if_reset(struct crypto_rng *tfm,
			       const u8 *seed, unsigned int slen)
{
	return lrng_kcapi_if_reseed(seed, slen);
}

static struct rng_alg lrng_alg = {
	.generate		= lrng_kcapi_if_random,
	.seed			= lrng_kcapi_if_reset,
	.seedsize		= 0,
	.base			= {
		.cra_name               = "stdrng",
		.cra_driver_name        = "lrng",
		.cra_priority           = 500,
		.cra_ctxsize            = 0,
		.cra_module             = THIS_MODULE,
		.cra_init               = lrng_kcapi_if_init,
		.cra_exit               = lrng_kcapi_if_cleanup,

	}
};

#ifdef CONFIG_LRNG_DRNG_ATOMIC
static int lrng_kcapi_if_random_atomic(struct crypto_rng *tfm,
				       const u8 *src, unsigned int slen,
				       u8 *rdata, unsigned int dlen)
{
	int ret = lrng_kcapi_if_reseed(src, slen);

	if (!ret)
		lrng_get_random_bytes(rdata, dlen);

	return ret;
}

static struct rng_alg lrng_alg_atomic = {
	.generate		= lrng_kcapi_if_random_atomic,
	.seed			= lrng_kcapi_if_reset,
	.seedsize		= 0,
	.base			= {
		.cra_name               = "lrng_atomic",
		.cra_driver_name        = "lrng_atomic",
		.cra_priority           = 100,
		.cra_ctxsize            = 0,
		.cra_module             = THIS_MODULE,
		.cra_init               = lrng_kcapi_if_init,
		.cra_exit               = lrng_kcapi_if_cleanup,

	}
};
#endif /* CONFIG_LRNG_DRNG_ATOMIC */

static int __init lrng_kcapi_if_mod_init(void)
{
	return
#ifdef CONFIG_LRNG_DRNG_ATOMIC
	       crypto_register_rng(&lrng_alg_atomic) ?:
#endif
	       crypto_register_rng(&lrng_alg);
}

static void __exit lrng_kcapi_if_mod_exit(void)
{
	crypto_unregister_rng(&lrng_alg);
#ifdef CONFIG_LRNG_DRNG_ATOMIC
	crypto_unregister_rng(&lrng_alg_atomic);
#endif
}

module_init(lrng_kcapi_if_mod_init);
module_exit(lrng_kcapi_if_mod_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Stephan Mueller <smueller@chronox.de>");
MODULE_DESCRIPTION("Entropy Source and DRNG Manager kernel crypto API RNG framework interface");
MODULE_ALIAS_CRYPTO("lrng");
MODULE_ALIAS_CRYPTO("lrng_atomic");
MODULE_ALIAS_CRYPTO("stdrng");
