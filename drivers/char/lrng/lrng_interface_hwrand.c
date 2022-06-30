// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * LRNG interface with the HW-Random framework
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#include <linux/lrng.h>
#include <linux/hw_random.h>
#include <linux/module.h>

static int lrng_hwrand_if_random(struct hwrng *rng, void *buf, size_t max,
				 bool wait)
{
	/*
	 * lrng_get_random_bytes_full not called as we cannot block.
	 *
	 * Note: We should either adjust .quality below depending on
	 * rng_is_initialized() or block here, but neither is not supported by
	 * the hw_rand framework.
	 */
	lrng_get_random_bytes(buf, max);
	return (int)max;
}

static struct hwrng lrng_hwrand = {
	.name		= "lrng",
	.init		= NULL,
	.cleanup	= NULL,
	.read		= lrng_hwrand_if_random,

	/*
	 * We set .quality only in case the LRNG does not provide the common
	 * interfaces or does not use the legacy RNG as entropy source. This
	 * shall avoid that the LRNG automatically spawns the hw_rand
	 * framework's hwrng kernel thread to feed data into
	 * add_hwgenerator_randomness. When the LRNG implements the common
	 * interfaces, this function feeds the data directly into the LRNG.
	 * If the LRNG uses the legacy RNG as entropy source,
	 * add_hwgenerator_randomness is implemented by the legacy RNG, but
	 * still eventually feeds the data into the LRNG. We should avoid such
	 * circular loops.
	 *
	 * We can specify full entropy here, because the LRNG is designed
	 * to provide full entropy.
	 */
#if !defined(CONFIG_LRNG_RANDOM_IF) && \
    !defined(CONFIG_LRNG_KERNEL_RNG)
	.quality	= 1024,
#endif
};

static int __init lrng_hwrand_if_mod_init(void)
{
	return hwrng_register(&lrng_hwrand);
}

static void __exit lrng_hwrand_if_mod_exit(void)
{
	hwrng_unregister(&lrng_hwrand);
}

module_init(lrng_hwrand_if_mod_init);
module_exit(lrng_hwrand_if_mod_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Stephan Mueller <smueller@chronox.de>");
MODULE_DESCRIPTION("Entropy Source and DRNG Manager HW-Random Interface");
