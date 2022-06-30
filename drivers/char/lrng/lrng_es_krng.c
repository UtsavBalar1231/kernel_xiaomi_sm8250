// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * LRNG Fast Entropy Source: Linux kernel RNG (random.c)
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fips.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/types.h>

#include "lrng_es_aux.h"
#include "lrng_es_krng.h"

static u32 krng_entropy = CONFIG_LRNG_KERNEL_RNG_ENTROPY_RATE;
#ifdef CONFIG_LRNG_RUNTIME_ES_CONFIG
module_param(krng_entropy, uint, 0644);
MODULE_PARM_DESC(krng_entropy, "Entropy in bits of 256 data bits from the kernel RNG noise source");
#endif

static atomic_t lrng_krng_initial_rate = ATOMIC_INIT(0);

static struct random_ready_callback lrng_krng_ready = {
	.owner = THIS_MODULE,
	.func = NULL,
};

static u32 lrng_krng_fips_entropylevel(u32 entropylevel)
{
	return fips_enabled ? 0 : entropylevel;
}

static void lrng_krng_adjust_entropy(struct random_ready_callback *rdy)
{
	u32 entropylevel;

	krng_entropy = atomic_read_u32(&lrng_krng_initial_rate);

	entropylevel = lrng_krng_fips_entropylevel(krng_entropy);
	pr_debug("Kernel RNG is fully seeded, setting entropy rate to %u bits of entropy\n",
		 entropylevel);
	lrng_drng_force_reseed();
	if (entropylevel)
		lrng_es_add_entropy();
}

static u32 lrng_krng_entropylevel(u32 requested_bits)
{
	if (lrng_krng_ready.func == NULL) {
		int err;

		lrng_krng_ready.func = lrng_krng_adjust_entropy;

		err = add_random_ready_callback(&lrng_krng_ready);
		switch (err) {
		case 0:
			atomic_set(&lrng_krng_initial_rate, krng_entropy);
			krng_entropy = 0;
			pr_debug("Kernel RNG is not yet seeded, setting entropy rate to 0 bits of entropy\n");
			break;

		case -EALREADY:
			pr_debug("Kernel RNG is fully seeded, setting entropy rate to %u bits of entropy\n",
				 lrng_krng_fips_entropylevel(krng_entropy));
			break;
		default:
			lrng_krng_ready.func = NULL;
			return 0;
		}
	}

	return lrng_fast_noise_entropylevel(
		lrng_krng_fips_entropylevel(krng_entropy), requested_bits);
}

static u32 lrng_krng_poolsize(void)
{
	return lrng_krng_entropylevel(lrng_security_strength());
}

/*
 * lrng_krng_get() - Get kernel RNG entropy
 *
 * @eb: entropy buffer to store entropy
 * @requested_bits: requested entropy in bits
 */
static void lrng_krng_get(struct entropy_buf *eb, u32 requested_bits,
			  bool __unused)
{
	u32 ent_bits = lrng_krng_entropylevel(requested_bits);

	get_random_bytes(eb->e[lrng_ext_es_krng], requested_bits >> 3);

	pr_debug("obtained %u bits of entropy from kernel RNG noise source\n",
		 ent_bits);

	eb->e_bits[lrng_ext_es_krng] = ent_bits;
}

static void lrng_krng_es_state(unsigned char *buf, size_t buflen)
{
	snprintf(buf, buflen,
		 " Available entropy: %u\n"
		 " Entropy Rate per 256 data bits: %u\n",
		 lrng_krng_poolsize(),
		 lrng_krng_entropylevel(256));
}

struct lrng_es_cb lrng_es_krng = {
	.name			= "KernelRNG",
	.get_ent		= lrng_krng_get,
	.curr_entropy		= lrng_krng_entropylevel,
	.max_entropy		= lrng_krng_poolsize,
	.state			= lrng_krng_es_state,
	.reset			= NULL,
	.switch_hash		= NULL,
};
