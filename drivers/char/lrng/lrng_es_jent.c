// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * LRNG Fast Entropy Source: Jitter RNG
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fips.h>
#include <linux/module.h>
#include <linux/types.h>
#include <crypto/internal/jitterentropy.h>

#include "lrng_definitions.h"
#include "lrng_es_aux.h"
#include "lrng_es_jent.h"

/*
 * Estimated entropy of data is a 16th of LRNG_DRNG_SECURITY_STRENGTH_BITS.
 * Albeit a full entropy assessment is provided for the noise source indicating
 * that it provides high entropy rates and considering that it deactivates
 * when it detects insufficient hardware, the chosen under estimation of
 * entropy is considered to be acceptable to all reviewers.
 */
static u32 jent_entropy = CONFIG_LRNG_JENT_ENTROPY_RATE;
#ifdef CONFIG_LRNG_RUNTIME_ES_CONFIG
module_param(jent_entropy, uint, 0644);
MODULE_PARM_DESC(jent_entropy, "Entropy in bits of 256 data bits from Jitter RNG noise source");
#endif

static bool lrng_jent_initialized = false;
static struct rand_data *lrng_jent_state;

static int __init lrng_jent_initialize(void)
{
	/* Initialize the Jitter RNG after the clocksources are initialized. */
	if (jent_entropy_init() ||
	    (lrng_jent_state = jent_entropy_collector_alloc(1, 0)) == NULL) {
		jent_entropy = 0;
		pr_info("Jitter RNG unusable on current system\n");
		return 0;
	}
	lrng_jent_initialized = true;
	pr_debug("Jitter RNG working on current system\n");

	/*
	 * In FIPS mode, the Jitter RNG is defined to have full of entropy
	 * unless a different value has been specified at the command line
	 * (i.e. the user overrides the default), and the default value is
	 * larger than zero (if it is zero, it is assumed that an RBG2(P) or
	 * RBG2(NP) construction is attempted that intends to exclude the
	 * Jitter RNG).
	 */
	if (fips_enabled &&
	    CONFIG_LRNG_JENT_ENTROPY_RATE > 0 &&
	    jent_entropy == CONFIG_LRNG_JENT_ENTROPY_RATE)
		jent_entropy = LRNG_DRNG_SECURITY_STRENGTH_BITS;

	lrng_drng_force_reseed();
	if (jent_entropy)
		lrng_es_add_entropy();

	return 0;
}
device_initcall(lrng_jent_initialize);

static u32 lrng_jent_entropylevel(u32 requested_bits)
{
	return lrng_fast_noise_entropylevel(lrng_jent_initialized ?
					    jent_entropy : 0, requested_bits);
}

static u32 lrng_jent_poolsize(void)
{
	return lrng_jent_entropylevel(lrng_security_strength());
}

/*
 * lrng_get_jent() - Get Jitter RNG entropy
 *
 * @eb: entropy buffer to store entropy
 * @requested_bits: requested entropy in bits
 */
static void lrng_jent_get(struct entropy_buf *eb, u32 requested_bits,
			  bool __unused)
{
	int ret;
	u32 ent_bits = lrng_jent_entropylevel(requested_bits);
	unsigned long flags;
	static DEFINE_SPINLOCK(lrng_jent_lock);

	spin_lock_irqsave(&lrng_jent_lock, flags);

	if (!lrng_jent_initialized) {
		spin_unlock_irqrestore(&lrng_jent_lock, flags);
		goto err;
	}

	ret = jent_read_entropy(lrng_jent_state, eb->e[lrng_ext_es_jitter],
				requested_bits >> 3);
	spin_unlock_irqrestore(&lrng_jent_lock, flags);

	if (ret) {
		pr_debug("Jitter RNG failed with %d\n", ret);
		goto err;
	}

	pr_debug("obtained %u bits of entropy from Jitter RNG noise source\n",
		 ent_bits);

	eb->e_bits[lrng_ext_es_jitter] = ent_bits;
	return;

err:
	eb->e_bits[lrng_ext_es_jitter] = 0;
}

static void lrng_jent_es_state(unsigned char *buf, size_t buflen)
{
	snprintf(buf, buflen,
		 " Available entropy: %u\n"
		 " Enabled: %s\n",
		 lrng_jent_poolsize(),
		 lrng_jent_initialized ? "true" : "false");
}

struct lrng_es_cb lrng_es_jent = {
	.name			= "JitterRNG",
	.get_ent		= lrng_jent_get,
	.curr_entropy		= lrng_jent_entropylevel,
	.max_entropy		= lrng_jent_poolsize,
	.state			= lrng_jent_es_state,
	.reset			= NULL,
	.switch_hash		= NULL,
};
