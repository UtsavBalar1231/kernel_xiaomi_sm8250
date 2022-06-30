// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * LRNG Slow Entropy Source: Scheduler-based data collection
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <asm/irq_regs.h>
#include <asm/ptrace.h>
#include <linux/lrng.h>
#include <crypto/hash.h>
#include <linux/module.h>
#include <linux/random.h>

#include "lrng_es_aux.h"
#include "lrng_es_sched.h"
#include "lrng_es_timer_common.h"
#include "lrng_health.h"
#include "lrng_numa.h"
#include "lrng_testing.h"

/*
 * Number of scheduler-based context switches to be recorded to assume that
 * DRNG security strength bits of entropy are received.
 * Note: a value below the DRNG security strength should not be defined as this
 *	 may imply the DRNG can never be fully seeded in case other noise
 *	 sources are unavailable.
 */
#define LRNG_SCHED_ENTROPY_BITS	\
	LRNG_UINT32_C(CONFIG_LRNG_SCHED_ENTROPY_RATE)

/* Number of events required for LRNG_DRNG_SECURITY_STRENGTH_BITS entropy */
static u32 lrng_sched_entropy_bits = LRNG_SCHED_ENTROPY_BITS;

static u32 sched_entropy __read_mostly = LRNG_SCHED_ENTROPY_BITS;
#ifdef CONFIG_LRNG_RUNTIME_ES_CONFIG
module_param(sched_entropy, uint, 0444);
MODULE_PARM_DESC(sched_entropy,
		 "How many scheduler-based context switches must be collected for obtaining 256 bits of entropy\n");
#endif

/* Per-CPU array holding concatenated entropy events */
static DEFINE_PER_CPU(u32 [LRNG_DATA_ARRAY_SIZE], lrng_sched_array)
						__aligned(LRNG_KCAPI_ALIGN);
static DEFINE_PER_CPU(u32, lrng_sched_array_ptr) = 0;
static DEFINE_PER_CPU(atomic_t, lrng_sched_array_events) = ATOMIC_INIT(0);

static void __init lrng_sched_check_compression_state(void)
{
	/* One pool should hold sufficient entropy for disabled compression */
	u32 max_ent = min_t(u32, lrng_get_digestsize(),
			    lrng_data_to_entropy(LRNG_DATA_NUM_VALUES,
						 lrng_sched_entropy_bits));
	if (max_ent < lrng_security_strength()) {
		pr_devel("Scheduler entropy source will never provide %u bits of entropy required for fully seeding the DRNG all by itself\n",
			lrng_security_strength());
	}
}

void __init lrng_sched_es_init(bool highres_timer)
{
	/* Set a minimum number of scheduler events that must be collected */
	sched_entropy = max_t(u32, LRNG_SCHED_ENTROPY_BITS, sched_entropy);

	if (highres_timer) {
		lrng_sched_entropy_bits = sched_entropy;
	} else {
		u32 new_entropy = sched_entropy * LRNG_ES_OVERSAMPLING_FACTOR;

		lrng_sched_entropy_bits = (sched_entropy < new_entropy) ?
					  new_entropy : sched_entropy;
		pr_warn("operating without high-resolution timer and applying oversampling factor %u\n",
			LRNG_ES_OVERSAMPLING_FACTOR);
	}

	lrng_sched_check_compression_state();
}

static u32 lrng_sched_avail_pool_size(void)
{
	u32 max_pool = lrng_get_digestsize(),
	    max_size = min_t(u32, max_pool, LRNG_DATA_NUM_VALUES);
	int cpu;

	for_each_online_cpu(cpu)
		max_size += max_pool;

	return max_size;
}

/* Return entropy of unused scheduler events present in all per-CPU pools. */
static u32 lrng_sched_avail_entropy(u32 __unused)
{
	u32 digestsize_events, events = 0;
	int cpu;

	/* Only deliver entropy when SP800-90B self test is completed */
	if (!lrng_sp80090b_startup_complete_es(lrng_int_es_sched))
		return 0;

	/* Obtain the cap of maximum numbers of scheduler events we count */
	digestsize_events = lrng_entropy_to_data(lrng_get_digestsize(),
						 lrng_sched_entropy_bits);
	/* Cap to max. number of scheduler events the array can hold */
	digestsize_events = min_t(u32, digestsize_events, LRNG_DATA_NUM_VALUES);

	for_each_online_cpu(cpu) {
		events += min_t(u32, digestsize_events,
			atomic_read_u32(per_cpu_ptr(&lrng_sched_array_events,
					cpu)));
	}

	/* Consider oversampling rate */
	return lrng_reduce_by_osr(
			lrng_data_to_entropy(events, lrng_sched_entropy_bits));
}

/*
 * Reset all per-CPU pools - reset entropy estimator but leave the pool data
 * that may or may not have entropy unchanged.
 */
static void lrng_sched_reset(void)
{
	int cpu;

	/* Trigger GCD calculation anew. */
	lrng_gcd_set(0);

	for_each_online_cpu(cpu)
		atomic_set(per_cpu_ptr(&lrng_sched_array_events, cpu), 0);
}

/*
 * Hash all per-CPU arrays and return the digest to be used as seed data for
 * seeding a DRNG. The caller must guarantee backtracking resistance.
 * The function will only copy as much data as entropy is available into the
 * caller-provided output buffer.
 *
 * This function handles the translation from the number of received scheduler
 * events into an entropy statement. The conversion depends on
 * LRNG_SCHED_ENTROPY_BITS which defines how many scheduler events must be
 * received to obtain 256 bits of entropy. With this value, the function
 * lrng_data_to_entropy converts a given data size (received scheduler events,
 * requested amount of data, etc.) into an entropy statement.
 * lrng_entropy_to_data does the reverse.
 *
 * @eb: entropy buffer to store entropy
 * @requested_bits: Requested amount of entropy
 * @fully_seeded: indicator whether LRNG is fully seeded
 */
static void lrng_sched_pool_hash(struct entropy_buf *eb, u32 requested_bits,
				 bool fully_seeded)
{
	SHASH_DESC_ON_STACK(shash, NULL);
	const struct lrng_hash_cb *hash_cb;
	struct lrng_drng *drng = lrng_drng_node_instance();
	u8 digest[LRNG_MAX_DIGESTSIZE];
	unsigned long flags;
	u32 found_events, collected_events = 0, collected_ent_bits,
	    requested_events, returned_ent_bits, digestsize, digestsize_events;
	int ret, cpu;
	void *hash;

	/* Only deliver entropy when SP800-90B self test is completed */
	if (!lrng_sp80090b_startup_complete_es(lrng_int_es_sched)) {
		eb->e_bits[lrng_int_es_sched] = 0;
		return;
	}

	/* Lock guarding replacement of per-NUMA hash */
	read_lock_irqsave(&drng->hash_lock, flags);
	hash_cb = drng->hash_cb;
	hash = drng->hash;

	/* The hash state of filled with all per-CPU pool hashes. */
	ret = hash_cb->hash_init(shash, hash);
	if (ret)
		goto err;

	digestsize = hash_cb->hash_digestsize(shash);

	/* Cap to maximum entropy that can ever be generated with given hash */
	lrng_cap_requested(digestsize << 3, requested_bits);
	requested_events = lrng_entropy_to_data(requested_bits +
						lrng_compress_osr(),
						lrng_sched_entropy_bits);
	digestsize_events = lrng_entropy_to_data(digestsize << 3,
						 lrng_sched_entropy_bits);

	/*
	 * Harvest entropy from each per-CPU hash state - even though we may
	 * have collected sufficient entropy, we will hash all per-CPU pools.
	 */
	for_each_online_cpu(cpu) {
		u32 unused_events = 0;

		ret = hash_cb->hash_update(shash,
				(u8 *)per_cpu_ptr(lrng_sched_array, cpu),
				LRNG_DATA_ARRAY_SIZE * sizeof(u32));

		/* Store all not-yet compressed data in data array into hash */
		ret = hash_cb->hash_update(shash, digest, digestsize);
		if (ret)
			goto err;

		/* Obtain entropy statement like for the entropy pool */
		found_events = atomic_xchg_relaxed(
				per_cpu_ptr(&lrng_sched_array_events, cpu), 0);
		/* Cap to maximum amount of data we can hold in hash */
		found_events = min_t(u32, found_events, digestsize_events);
		/* Cap to maximum amount of data we can hold in array */
		found_events = min_t(u32, found_events, LRNG_DATA_NUM_VALUES);

		collected_events += found_events;
		if (collected_events > requested_events) {
			unused_events = collected_events - requested_events;
			atomic_add_return_relaxed(unused_events,
				per_cpu_ptr(&lrng_sched_array_events, cpu));
			collected_events = requested_events;
		}
		pr_debug("%u scheduler-based events used from entropy array of CPU %d, %u scheduler-based events remain unused\n",
			 found_events - unused_events, cpu, unused_events);
	}

	ret = hash_cb->hash_final(shash, digest);
	if (ret)
		goto err;

	collected_ent_bits = lrng_data_to_entropy(collected_events,
						  lrng_sched_entropy_bits);
	/* Apply oversampling: discount requested oversampling rate */
	returned_ent_bits = lrng_reduce_by_osr(collected_ent_bits);

	pr_debug("obtained %u bits by collecting %u bits of entropy from scheduler-based noise source\n",
		 returned_ent_bits, collected_ent_bits);

	/*
	 * Truncate to available entropy as implicitly allowed by SP800-90B
	 * section 3.1.5.1.1 table 1 which awards truncated hashes full
	 * entropy.
	 *
	 * During boot time, we read requested_bits data with
	 * returned_ent_bits entropy. In case our conservative entropy
	 * estimate underestimates the available entropy we can transport as
	 * much available entropy as possible.
	 */
	memcpy(eb->e[lrng_int_es_sched], digest,
	       fully_seeded ? returned_ent_bits >> 3 : requested_bits >> 3);
	eb->e_bits[lrng_int_es_sched] = returned_ent_bits;

out:
	hash_cb->hash_desc_zero(shash);
	read_unlock_irqrestore(&drng->hash_lock, flags);
	memzero_explicit(digest, sizeof(digest));
	return;

err:
	eb->e_bits[lrng_int_es_sched] = 0;
	goto out;
}

/*
 * Concatenate full 32 bit word at the end of time array even when current
 * ptr is not aligned to sizeof(data).
 */
static void lrng_sched_array_add_u32(u32 data)
{
	/* Increment pointer by number of slots taken for input value */
	u32 pre_ptr, mask, ptr = this_cpu_add_return(lrng_sched_array_ptr,
						     LRNG_DATA_SLOTS_PER_UINT);
	unsigned int pre_array;

	lrng_data_split_u32(&ptr, &pre_ptr, &mask);

	/* MSB of data go into previous unit */
	pre_array = lrng_data_idx2array(pre_ptr);
	/* zeroization of slot to ensure the following OR adds the data */
	this_cpu_and(lrng_sched_array[pre_array], ~(0xffffffff & ~mask));
	this_cpu_or(lrng_sched_array[pre_array], data & ~mask);

	/*
	 * Continuous compression is not allowed for scheduler noise source,
	 * so do not call lrng_sched_array_to_hash here.
	 */

	/* LSB of data go into current unit */
	this_cpu_write(lrng_sched_array[lrng_data_idx2array(ptr)],
		       data & mask);
}

/* Concatenate data of max LRNG_DATA_SLOTSIZE_MASK at the end of time array */
static void lrng_sched_array_add_slot(u32 data)
{
	/* Get slot */
	u32 ptr = this_cpu_inc_return(lrng_sched_array_ptr) &
							LRNG_DATA_WORD_MASK;
	unsigned int array = lrng_data_idx2array(ptr);
	unsigned int slot = lrng_data_idx2slot(ptr);

	/* zeroization of slot to ensure the following OR adds the data */
	this_cpu_and(lrng_sched_array[array],
		     ~(lrng_data_slot_val(0xffffffff & LRNG_DATA_SLOTSIZE_MASK,
					  slot)));
	/* Store data into slot */
	this_cpu_or(lrng_sched_array[array], lrng_data_slot_val(data, slot));

	/*
	 * Continuous compression is not allowed for scheduler noise source,
	 * so do not call lrng_sched_array_to_hash here.
	 */
}

static void
lrng_time_process_common(u32 time, void(*add_time)(u32 data))
{
	enum lrng_health_res health_test;

	if (lrng_raw_sched_hires_entropy_store(time))
		return;

	health_test = lrng_health_test(time, lrng_int_es_sched);
	if (health_test > lrng_health_fail_use)
		return;

	if (health_test == lrng_health_pass)
		atomic_inc_return(this_cpu_ptr(&lrng_sched_array_events));

	add_time(time);

	/*
	 * We cannot call lrng_es_add_entropy() as this would call a schedule
	 * operation that is not permissible in scheduler context.
	 * As the scheduler ES provides a high bandwidth of entropy, we assume
	 * that other reseed triggers happen to pick up the scheduler ES
	 * entropy in due time.
	 */
}

/* Batching up of entropy in per-CPU array */
static void lrng_sched_time_process(void)
{
	u32 now_time = random_get_entropy();

	if (unlikely(!lrng_gcd_tested())) {
		/* When GCD is unknown, we process the full time stamp */
		lrng_time_process_common(now_time, lrng_sched_array_add_u32);
		lrng_gcd_add_value(now_time);
	} else {
		/* GCD is known and applied */
		lrng_time_process_common((now_time / lrng_gcd_get()) &
					 LRNG_DATA_SLOTSIZE_MASK,
					 lrng_sched_array_add_slot);
	}

	lrng_sched_perf_time(now_time);
}

void add_sched_randomness(const struct task_struct *p, int cpu)
{
	if (lrng_highres_timer()) {
		lrng_sched_time_process();
	} else {
		u32 tmp = cpu;

		tmp ^= lrng_raw_sched_pid_entropy_store(p->pid) ?
							0 : (u32)p->pid;
		tmp ^= lrng_raw_sched_starttime_entropy_store(p->start_time) ?
							0 : (u32)p->start_time;
		tmp ^= lrng_raw_sched_nvcsw_entropy_store(p->nvcsw) ?
							0 : (u32)p->nvcsw;

		lrng_sched_time_process();
		lrng_sched_array_add_u32(tmp);
	}
}
EXPORT_SYMBOL(add_sched_randomness);

static void lrng_sched_es_state(unsigned char *buf, size_t buflen)
{
	const struct lrng_drng *lrng_drng_init = lrng_drng_init_instance();

	/* Assume the lrng_drng_init lock is taken by caller */
	snprintf(buf, buflen,
		 " Hash for operating entropy pool: %s\n"
		 " Available entropy: %u\n"
		 " per-CPU scheduler event collection size: %u\n"
		 " Standards compliance: %s\n"
		 " High-resolution timer: %s\n",
		 lrng_drng_init->hash_cb->hash_name(),
		 lrng_sched_avail_entropy(0),
		 LRNG_DATA_NUM_VALUES,
		 lrng_sp80090b_compliant(lrng_int_es_sched) ? "SP800-90B " : "",
		 lrng_highres_timer() ? "true" : "false");
}

struct lrng_es_cb lrng_es_sched = {
	.name			= "Scheduler",
	.get_ent		= lrng_sched_pool_hash,
	.curr_entropy		= lrng_sched_avail_entropy,
	.max_entropy		= lrng_sched_avail_pool_size,
	.state			= lrng_sched_es_state,
	.reset			= lrng_sched_reset,
	.switch_hash		= NULL,
};
