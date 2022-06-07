// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * LRNG Fast Entropy Source: CPU-based entropy source
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/lrng.h>
#include <crypto/hash.h>
#include <linux/module.h>
#include <linux/random.h>

#include "lrng_definitions.h"
#include "lrng_es_aux.h"
#include "lrng_es_cpu.h"

/*
 * Estimated entropy of data is a 32th of LRNG_DRNG_SECURITY_STRENGTH_BITS.
 * As we have no ability to review the implementation of those noise sources,
 * it is prudent to have a conservative estimate here.
 */
#define LRNG_ARCHRANDOM_DEFAULT_STRENGTH CONFIG_LRNG_CPU_ENTROPY_RATE
#define LRNG_ARCHRANDOM_TRUST_CPU_STRENGTH LRNG_DRNG_SECURITY_STRENGTH_BITS
#ifdef CONFIG_RANDOM_TRUST_CPU
static u32 cpu_entropy = LRNG_ARCHRANDOM_TRUST_CPU_STRENGTH;
#else
static u32 cpu_entropy = LRNG_ARCHRANDOM_DEFAULT_STRENGTH;
#endif
#ifdef CONFIG_LRNG_RUNTIME_ES_CONFIG
module_param(cpu_entropy, uint, 0644);
MODULE_PARM_DESC(cpu_entropy, "Entropy in bits of 256 data bits from CPU noise source (e.g. RDSEED)");
#endif

static int __init lrng_parse_trust_cpu(char *arg)
{
	int ret;
	bool trust_cpu = false;

	ret = kstrtobool(arg, &trust_cpu);
	if (ret)
		return ret;

	if (trust_cpu) {
		cpu_entropy = LRNG_ARCHRANDOM_TRUST_CPU_STRENGTH;
		lrng_es_add_entropy();
	} else {
		cpu_entropy = LRNG_ARCHRANDOM_DEFAULT_STRENGTH;
	}

	return 0;
}
early_param("random.trust_cpu", lrng_parse_trust_cpu);

static u32 lrng_cpu_entropylevel(u32 requested_bits)
{
	return lrng_fast_noise_entropylevel(cpu_entropy, requested_bits);
}

static u32 lrng_cpu_poolsize(void)
{
	return lrng_cpu_entropylevel(lrng_security_strength());
}

static u32 lrng_get_cpu_data(u8 *outbuf, u32 requested_bits)
{
	u32 i;

	/* operate on full blocks */
	BUILD_BUG_ON(LRNG_DRNG_SECURITY_STRENGTH_BYTES % sizeof(unsigned long));
	BUILD_BUG_ON(LRNG_SEED_BUFFER_INIT_ADD_BITS % sizeof(unsigned long));
	/* ensure we have aligned buffers */
	BUILD_BUG_ON(LRNG_KCAPI_ALIGN % sizeof(unsigned long));

	for (i = 0; i < (requested_bits >> 3);
	     i += sizeof(unsigned long)) {
		if (!arch_get_random_seed_long((unsigned long *)(outbuf + i)) &&
		    !arch_get_random_long((unsigned long *)(outbuf + i))) {
			cpu_entropy = 0;
			return 0;
		}
	}

	return requested_bits;
}

static u32 lrng_get_cpu_data_compress(u8 *outbuf, u32 requested_bits,
				      u32 data_multiplier)
{
	SHASH_DESC_ON_STACK(shash, NULL);
	const struct lrng_hash_cb *hash_cb;
	struct lrng_drng *drng = lrng_drng_node_instance();
	unsigned long flags;
	u32 ent_bits = 0, i, partial_bits = 0, digestsize, digestsize_bits,
	    full_bits;
	void *hash;

	read_lock_irqsave(&drng->hash_lock, flags);
	hash_cb = drng->hash_cb;
	hash = drng->hash;

	digestsize = hash_cb->hash_digestsize(hash);
	digestsize_bits = digestsize << 3;
	/* Cap to maximum entropy that can ever be generated with given hash */
	lrng_cap_requested(digestsize_bits, requested_bits);
	full_bits = requested_bits * data_multiplier;

	/* Calculate oversampling for SP800-90C */
	if (lrng_sp80090c_compliant()) {
		/* Complete amount of bits to be pulled */
		full_bits += LRNG_OVERSAMPLE_ES_BITS * data_multiplier;
		/* Full blocks that will be pulled */
		data_multiplier = full_bits / requested_bits;
		/* Partial block in bits to be pulled */
		partial_bits = full_bits - (data_multiplier * requested_bits);
	}

	if (hash_cb->hash_init(shash, hash))
		goto out;

	/* Hash all data from the CPU entropy source */
	for (i = 0; i < data_multiplier; i++) {
		ent_bits = lrng_get_cpu_data(outbuf, requested_bits);
		if (!ent_bits)
			goto out;

		if (hash_cb->hash_update(shash, outbuf, ent_bits >> 3))
			goto err;
	}

	/* Hash partial block, if applicable */
	ent_bits = lrng_get_cpu_data(outbuf, partial_bits);
	if (ent_bits &&
	    hash_cb->hash_update(shash, outbuf, ent_bits >> 3))
		goto err;

	pr_debug("pulled %u bits from CPU RNG entropy source\n", full_bits);
	ent_bits = requested_bits;

	/* Generate the compressed data to be returned to the caller */
	if (requested_bits < digestsize_bits) {
		u8 digest[LRNG_MAX_DIGESTSIZE];

		if (hash_cb->hash_final(shash, digest))
			goto err;

		/* Truncate output data to requested size */
		memcpy(outbuf, digest, requested_bits >> 3);
		memzero_explicit(digest, digestsize);
	} else {
		if (hash_cb->hash_final(shash, outbuf))
			goto err;
	}

out:
	hash_cb->hash_desc_zero(shash);
	read_unlock_irqrestore(&drng->hash_lock, flags);
	return ent_bits;

err:
	ent_bits = 0;
	goto out;
}

/*
 * If CPU entropy source requires does not return full entropy, return the
 * multiplier of how much data shall be sampled from it.
 */
static u32 lrng_cpu_multiplier(void)
{
	static u32 data_multiplier = 0;
	unsigned long v;

	if (data_multiplier > 0)
		return data_multiplier;

	if (IS_ENABLED(CONFIG_X86) && !arch_get_random_seed_long(&v)) {
		/*
		 * Intel SPEC: pulling 512 blocks from RDRAND ensures
		 * one reseed making it logically equivalent to RDSEED.
		 */
		data_multiplier = 512;
	} else if (IS_ENABLED(CONFIG_PPC)) {
		/*
		 * PowerISA defines DARN to deliver at least 0.5 bits of
		 * entropy per data bit.
		 */
		data_multiplier = 2;
	} else if (IS_ENABLED(CONFIG_RISCV)) {
		/*
		 * riscv-crypto-spec-scalar-1.0.0-rc6.pdf section 4.2 defines
		 * this requirement.
		 */
		data_multiplier = 2;
	} else {
		/* CPU provides full entropy */
		data_multiplier = CONFIG_LRNG_CPU_FULL_ENT_MULTIPLIER;
	}
	return data_multiplier;
}

static int
lrng_cpu_switch_hash(struct lrng_drng *drng, int node,
		     const struct lrng_hash_cb *new_cb, void *new_hash,
		     const struct lrng_hash_cb *old_cb)
{
	u32 digestsize, multiplier;

	if (!IS_ENABLED(CONFIG_LRNG_SWITCH))
		return -EOPNOTSUPP;

	digestsize = lrng_get_digestsize();
	multiplier = lrng_cpu_multiplier();

	/*
	 * It would be security violation if the new digestsize is smaller than
	 * the set CPU entropy rate.
	 */
	WARN_ON(multiplier > 1 && digestsize < cpu_entropy);
	cpu_entropy = min_t(u32, digestsize, cpu_entropy);
	return 0;
}

/*
 * lrng_get_arch() - Get CPU entropy source entropy
 *
 * @eb: entropy buffer to store entropy
 * @requested_bits: requested entropy in bits
 */
static void lrng_cpu_get(struct entropy_buf *eb, u32 requested_bits,
			 bool __unused)
{
	u32 ent_bits, data_multiplier = lrng_cpu_multiplier();

	if (data_multiplier <= 1) {
		ent_bits = lrng_get_cpu_data(eb->e[lrng_ext_es_cpu],
					     requested_bits);
	} else {
		ent_bits = lrng_get_cpu_data_compress(eb->e[lrng_ext_es_cpu],
						      requested_bits,
						      data_multiplier);
	}

	ent_bits = lrng_cpu_entropylevel(ent_bits);
	pr_debug("obtained %u bits of entropy from CPU RNG entropy source\n",
		 ent_bits);
	eb->e_bits[lrng_ext_es_cpu] = ent_bits;
}

static void lrng_cpu_es_state(unsigned char *buf, size_t buflen)
{
	const struct lrng_drng *lrng_drng_init = lrng_drng_init_instance();
	u32 data_multiplier = lrng_cpu_multiplier();

	/* Assume the lrng_drng_init lock is taken by caller */
	snprintf(buf, buflen,
		 " Hash for compressing data: %s\n"
		 " Available entropy: %u\n"
		 " Data multiplier: %u\n",
		 (data_multiplier <= 1) ?
			"N/A" : lrng_drng_init->hash_cb->hash_name(),
		 lrng_cpu_poolsize(),
		 data_multiplier);
}

struct lrng_es_cb lrng_es_cpu = {
	.name			= "CPU",
	.get_ent		= lrng_cpu_get,
	.curr_entropy		= lrng_cpu_entropylevel,
	.max_entropy		= lrng_cpu_poolsize,
	.state			= lrng_cpu_es_state,
	.reset			= NULL,
	.switch_hash		= lrng_cpu_switch_hash,
};
