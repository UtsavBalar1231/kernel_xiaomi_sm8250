/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#ifndef _LRNG_H
#define _LRNG_H

#include <crypto/hash.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/types.h>

#define kfree_sensitive kzfree

/*
 * struct lrng_drng_cb - cryptographic callback functions defining a DRNG
 * @drng_name		Name of DRNG
 * @drng_alloc:		Allocate DRNG -- the provided integer should be used for
 *			sanity checks.
 *			return: allocated data structure or PTR_ERR on error
 * @drng_dealloc:	Deallocate DRNG
 * @drng_seed:		Seed the DRNG with data of arbitrary length drng: is
 *			pointer to data structure allocated with drng_alloc
 *			return: >= 0 on success, < 0 on error
 * @drng_generate:	Generate random numbers from the DRNG with arbitrary
 *			length
 */
struct lrng_drng_cb {
	const char *(*drng_name)(void);
	void *(*drng_alloc)(u32 sec_strength);
	void (*drng_dealloc)(void *drng);
	int (*drng_seed)(void *drng, const u8 *inbuf, u32 inbuflen);
	int (*drng_generate)(void *drng, u8 *outbuf, u32 outbuflen);
};

/*
 * struct lrng_hash_cb - cryptographic callback functions defining a hash
 * @hash_name		Name of Hash used for reading entropy pool arbitrary
 *			length
 * @hash_alloc:		Allocate the hash for reading the entropy pool
 *			return: allocated data structure (NULL is success too)
 *				or ERR_PTR on error
 * @hash_dealloc:	Deallocate Hash
 * @hash_digestsize:	Return the digestsize for the used hash to read out
 *			entropy pool
 *			hash: is pointer to data structure allocated with
 *			      hash_alloc
 *			return: size of digest of hash in bytes
 * @hash_init:		Initialize hash
 *			hash: is pointer to data structure allocated with
 *			      hash_alloc
 *			return: 0 on success, < 0 on error
 * @hash_update:	Update hash operation
 *			hash: is pointer to data structure allocated with
 *			      hash_alloc
 *			return: 0 on success, < 0 on error
 * @hash_final		Final hash operation
 *			hash: is pointer to data structure allocated with
 *			      hash_alloc
 *			return: 0 on success, < 0 on error
 * @hash_desc_zero	Zeroization of hash state buffer
 *
 * Assumptions:
 *
 * 1. Hash operation will not sleep
 * 2. The hash' volatile state information is provided with *shash by caller.
 */
struct lrng_hash_cb {
	const char *(*hash_name)(void);
	void *(*hash_alloc)(void);
	void (*hash_dealloc)(void *hash);
	u32 (*hash_digestsize)(void *hash);
	int (*hash_init)(struct shash_desc *shash, void *hash);
	int (*hash_update)(struct shash_desc *shash, const u8 *inbuf,
			   u32 inbuflen);
	int (*hash_final)(struct shash_desc *shash, u8 *digest);
	void (*hash_desc_zero)(struct shash_desc *shash);
};

/* Register cryptographic backend */
#ifdef CONFIG_LRNG_SWITCH
int lrng_set_drng_cb(const struct lrng_drng_cb *cb);
int lrng_set_hash_cb(const struct lrng_hash_cb *cb);
#else	/* CONFIG_LRNG_SWITCH */
static inline int
lrng_set_drng_cb(const struct lrng_drng_cb *cb) { return -EOPNOTSUPP; }
static inline int
lrng_set_hash_cb(const struct lrng_hash_cb *cb) { return -EOPNOTSUPP; }
#endif	/* CONFIG_LRNG_SWITCH */

/* Callback to feed events to the scheduler entropy source */
#ifdef CONFIG_LRNG_SCHED
extern void add_sched_randomness(const struct task_struct *p, int cpu);
#else
static inline void
add_sched_randomness(const struct task_struct *p, int cpu) { }
#endif

/*
 * lrng_get_random_bytes() - Provider of cryptographic strong random numbers
 * for kernel-internal usage.
 *
 * This function is appropriate for in-kernel use cases operating in atomic
 * contexts. It will always use the ChaCha20 DRNG and it may be the case that
 * it is not fully seeded when being used.
 *
 * @buf: buffer to store the random bytes
 * @nbytes: size of the buffer
 */
#ifdef CONFIG_LRNG_DRNG_ATOMIC
void lrng_get_random_bytes(void *buf, int nbytes);
#endif

/*
 * lrng_get_random_bytes_full() - Provider of cryptographic strong
 * random numbers for kernel-internal usage from a fully initialized LRNG.
 *
 * This function will always return random numbers from a fully seeded and
 * fully initialized LRNG.
 *
 * This function is appropriate only for non-atomic use cases as this
 * function may sleep. It provides access to the full functionality of LRNG
 * including the switchable DRNG support, that may support other DRNGs such
 * as the SP800-90A DRBG.
 *
 * @buf: buffer to store the random bytes
 * @nbytes: size of the buffer
 */
#ifdef CONFIG_LRNG
void lrng_get_random_bytes_full(void *buf, int nbytes);
#endif

/*
 * lrng_get_random_bytes_min() - Provider of cryptographic strong
 * random numbers for kernel-internal usage from at least a minimally seeded
 * LRNG, which is not necessarily fully initialized yet (e.g. SP800-90C
 * oversampling applied in FIPS mode is not applied yet).
 *
 * This function is appropriate only for non-atomic use cases as this
 * function may sleep. It provides access to the full functionality of LRNG
 * including the switchable DRNG support, that may support other DRNGs such
 * as the SP800-90A DRBG.
 *
 * @buf: buffer to store the random bytes
 * @nbytes: size of the buffer
 */
#ifdef CONFIG_LRNG
void lrng_get_random_bytes_min(void *buf, int nbytes);
#endif

#endif /* _LRNG_H */
