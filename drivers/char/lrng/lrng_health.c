// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * Entropy Source and DRNG Manager (LRNG) Health Testing
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fips.h>
#include <linux/module.h>

#include "lrng_es_mgr.h"
#include "lrng_health.h"

/* Stuck Test */
struct lrng_stuck_test {
	u32 last_time;		/* Stuck test: time of previous IRQ */
	u32 last_delta;		/* Stuck test: delta of previous IRQ */
	u32 last_delta2;	/* Stuck test: 2. time derivation of prev IRQ */
};

/* Repetition Count Test */
struct lrng_rct {
	atomic_t rct_count;	/* Number of stuck values */
};

/* Adaptive Proportion Test */
struct lrng_apt {
	/* Data window size */
#define LRNG_APT_WINDOW_SIZE	512
	/* LSB of time stamp to process */
#define LRNG_APT_LSB		16
#define LRNG_APT_WORD_MASK	(LRNG_APT_LSB - 1)
	atomic_t apt_count;		/* APT counter */
	atomic_t apt_base;		/* APT base reference */

	atomic_t apt_trigger;
	bool apt_base_set;	/* Is APT base set? */
};

/* Health data collected for one entropy source */
struct lrng_health_es_state {
	struct lrng_rct rct;
	struct lrng_apt apt;

	/* SP800-90B startup health tests */
#define LRNG_SP80090B_STARTUP_SAMPLES  1024
#define LRNG_SP80090B_STARTUP_BLOCKS   ((LRNG_SP80090B_STARTUP_SAMPLES + \
					 LRNG_APT_WINDOW_SIZE - 1) /    \
					LRNG_APT_WINDOW_SIZE)
	bool sp80090b_startup_done;
	atomic_t sp80090b_startup_blocks;
};

#define LRNG_HEALTH_ES_INIT(x) \
	x.rct.rct_count = ATOMIC_INIT(0), \
	x.apt.apt_count = ATOMIC_INIT(0), \
	x.apt.apt_base = ATOMIC_INIT(-1), \
	x.apt.apt_trigger = ATOMIC_INIT(LRNG_APT_WINDOW_SIZE), \
	x.apt.apt_base_set = false, \
	x.sp80090b_startup_blocks = ATOMIC_INIT(LRNG_SP80090B_STARTUP_BLOCKS), \
	x.sp80090b_startup_done = false,

/* The health test code must operate lock-less */
struct lrng_health {
	bool health_test_enabled;
	struct lrng_health_es_state es_state[lrng_int_es_last];
};

static struct lrng_health lrng_health = {
	.health_test_enabled = true,

#ifdef CONFIG_LRNG_IRQ
	LRNG_HEALTH_ES_INIT(.es_state[lrng_int_es_irq])
#endif
#ifdef CONFIG_LRNG_SCHED
	LRNG_HEALTH_ES_INIT(.es_state[lrng_int_es_sched])
#endif
};

static DEFINE_PER_CPU(struct lrng_stuck_test[lrng_int_es_last],
		      lrng_stuck_test_array);

static bool lrng_sp80090b_health_requested(void)
{
	/* Health tests are only requested in FIPS mode */
	return fips_enabled;
}

static bool lrng_sp80090b_health_enabled(void)
{
	struct lrng_health *health = &lrng_health;

	return lrng_sp80090b_health_requested() && health->health_test_enabled;
}

/***************************************************************************
 * SP800-90B Compliance
 *
 * If the Linux-RNG is booted into FIPS mode, the following interfaces
 * provide an SP800-90B compliant noise source:
 *
 * * /dev/random
 * * getrandom(2)
 * * get_random_bytes_full
 *
 * All other interfaces, including /dev/urandom or get_random_bytes without
 * the add_random_ready_callback cannot claim to use an SP800-90B compliant
 * noise source.
 ***************************************************************************/

/*
 * Perform SP800-90B startup testing
 */
static void lrng_sp80090b_startup(struct lrng_health *health,
				  enum lrng_internal_es es)
{
	struct lrng_health_es_state *es_state = &health->es_state[es];

	if (!es_state->sp80090b_startup_done &&
	    atomic_dec_and_test(&es_state->sp80090b_startup_blocks)) {
		es_state->sp80090b_startup_done = true;
		pr_info("SP800-90B startup health tests for internal entropy source %u completed\n",
			es);
		lrng_drng_force_reseed();

		/*
		 * We cannot call lrng_es_add_entropy() as this may cause a
		 * schedule operation while in scheduler context for the
		 * scheduler ES.
		 */
	}
}

/*
 * Handle failure of SP800-90B startup testing
 */
static void lrng_sp80090b_startup_failure(struct lrng_health *health,
					  enum lrng_internal_es es)
{
	struct lrng_health_es_state *es_state = &health->es_state[es];

	/* Reset of LRNG and its entropy - NOTE: we are in atomic context */
	lrng_reset();

	/*
	 * Reset the SP800-90B startup test.
	 *
	 * NOTE SP800-90B section 4.3 bullet 4 does not specify what
	 * exactly is to be done in case of failure! Thus, we do what
	 * makes sense, i.e. restarting the health test and thus gating
	 * the output function of /dev/random and getrandom(2).
	 */
	atomic_set(&es_state->sp80090b_startup_blocks,
		   LRNG_SP80090B_STARTUP_BLOCKS);
}

/*
 * Handle failure of SP800-90B runtime testing
 */
static void lrng_sp80090b_runtime_failure(struct lrng_health *health,
					  enum lrng_internal_es es)
{
	struct lrng_health_es_state *es_state = &health->es_state[es];

	lrng_sp80090b_startup_failure(health, es);
	es_state->sp80090b_startup_done = false;
}

static void lrng_sp80090b_failure(struct lrng_health *health,
				  enum lrng_internal_es es)
{
	struct lrng_health_es_state *es_state = &health->es_state[es];

	if (es_state->sp80090b_startup_done) {
		pr_err("SP800-90B runtime health test failure for internal entropy source %u - invalidating all existing entropy and initiate SP800-90B startup\n", es);
		lrng_sp80090b_runtime_failure(health, es);
	} else {
		pr_err("SP800-90B startup test failure for internal entropy source %u - resetting\n", es);
		lrng_sp80090b_startup_failure(health, es);
	}
}

bool lrng_sp80090b_startup_complete_es(enum lrng_internal_es es)
{
	struct lrng_health *health = &lrng_health;
	struct lrng_health_es_state *es_state = &health->es_state[es];

	if (!lrng_sp80090b_health_enabled())
		return true;

	return es_state->sp80090b_startup_done;
}

bool lrng_sp80090b_compliant(enum lrng_internal_es es)
{
	struct lrng_health *health = &lrng_health;
	struct lrng_health_es_state *es_state = &health->es_state[es];

	return lrng_sp80090b_health_enabled() &&
	       es_state->sp80090b_startup_done;
}

/***************************************************************************
 * Adaptive Proportion Test
 *
 * This test complies with SP800-90B section 4.4.2.
 ***************************************************************************/

/*
 * Reset the APT counter
 *
 * @health [in] Reference to health state
 */
static void lrng_apt_reset(struct lrng_apt *apt, unsigned int time_masked)
{
	/* Reset APT */
	atomic_set(&apt->apt_count, 0);
	atomic_set(&apt->apt_base, time_masked);
}

static void lrng_apt_restart(struct lrng_apt *apt)
{
	atomic_set(&apt->apt_trigger, LRNG_APT_WINDOW_SIZE);
}

/*
 * Insert a new entropy event into APT
 *
 * This function does is void as it does not decide about the fate of a time
 * stamp. An APT failure can only happen at the same time of a stuck test
 * failure. Thus, the stuck failure will already decide how the time stamp
 * is handled.
 *
 * @health [in] Reference to health state
 * @now_time [in] Time stamp to process
 */
static void lrng_apt_insert(struct lrng_health *health,
			    unsigned int now_time, enum lrng_internal_es es)
{
	struct lrng_health_es_state *es_state = &health->es_state[es];
	struct lrng_apt *apt = &es_state->apt;

	if (!lrng_sp80090b_health_requested())
		return;

	now_time &= LRNG_APT_WORD_MASK;

	/* Initialization of APT */
	if (!apt->apt_base_set) {
		atomic_set(&apt->apt_base, now_time);
		apt->apt_base_set = true;
		return;
	}

	if (now_time == (unsigned int)atomic_read(&apt->apt_base)) {
		u32 apt_val = (u32)atomic_inc_return_relaxed(&apt->apt_count);

		if (apt_val >= CONFIG_LRNG_APT_CUTOFF)
			lrng_sp80090b_failure(health, es);
	}

	if (atomic_dec_and_test(&apt->apt_trigger)) {
		lrng_apt_restart(apt);
		lrng_apt_reset(apt, now_time);
		lrng_sp80090b_startup(health, es);
	}
}

/***************************************************************************
 * Repetition Count Test
 *
 * The LRNG uses an enhanced version of the Repetition Count Test
 * (RCT) specified in SP800-90B section 4.4.1. Instead of counting identical
 * back-to-back values, the input to the RCT is the counting of the stuck
 * values while filling the entropy pool.
 *
 * The RCT is applied with an alpha of 2^-30 compliant to FIPS 140-2 IG 9.8.
 *
 * During the counting operation, the LRNG always calculates the RCT
 * cut-off value of C. If that value exceeds the allowed cut-off value,
 * the LRNG will invalidate all entropy for the entropy pool which implies
 * that no data can be extracted from the entropy pool unless new entropy
 * is received.
 ***************************************************************************/

/*
 * Hot code path - Insert data for Repetition Count Test
 *
 * @health: Reference to health information
 * @stuck: Decision of stuck test
 */
static void lrng_rct(struct lrng_health *health, enum lrng_internal_es es,
		     int stuck)
{
	struct lrng_health_es_state *es_state = &health->es_state[es];
	struct lrng_rct *rct = &es_state->rct;

	if (!lrng_sp80090b_health_requested())
		return;

	if (stuck) {
		u32 rct_count = atomic_add_return_relaxed(1, &rct->rct_count);

		/*
		 * The cutoff value is based on the following consideration:
		 * alpha = 2^-30 as recommended in FIPS 140-2 IG 9.8.
		 * In addition, we imply an entropy value H of 1 bit as this
		 * is the minimum entropy required to provide full entropy.
		 *
		 * Note, rct_count (which equals to value B in the
		 * pseudo code of SP800-90B section 4.4.1) starts with zero.
		 * Hence we need to subtract one from the cutoff value as
		 * calculated following SP800-90B.
		 */
		if (rct_count >= CONFIG_LRNG_RCT_CUTOFF) {
			atomic_set(&rct->rct_count, 0);

			/*
			 * APT must start anew as we consider all previously
			 * recorded data to contain no entropy.
			 */
			lrng_apt_restart(&es_state->apt);

			lrng_sp80090b_failure(health, es);
		}
	} else {
		atomic_set(&rct->rct_count, 0);
	}
}

/***************************************************************************
 * Stuck Test
 *
 * Checking the:
 *      1st derivative of the event occurrence (time delta)
 *      2nd derivative of the event occurrence (delta of time deltas)
 *      3rd derivative of the event occurrence (delta of delta of time deltas)
 *
 * All values must always be non-zero. The stuck test is only valid disabled if
 * high-resolution time stamps are identified after initialization.
 ***************************************************************************/

static u32 lrng_delta(u32 prev, u32 next)
{
	/*
	 * Note that this (unsigned) subtraction does yield the correct value
	 * in the wraparound-case, i.e. when next < prev.
	 */
	return (next - prev);
}

/*
 * Hot code path
 *
 * @health: Reference to health information
 * @now: Event time
 * @return: 0 event occurrence not stuck (good time stamp)
 *	    != 0 event occurrence stuck (reject time stamp)
 */
static int lrng_irq_stuck(enum lrng_internal_es es, u32 now_time)
{
	struct lrng_stuck_test *stuck = this_cpu_ptr(lrng_stuck_test_array);
	u32 delta = lrng_delta(stuck[es].last_time, now_time);
	u32 delta2 = lrng_delta(stuck[es].last_delta, delta);
	u32 delta3 = lrng_delta(stuck[es].last_delta2, delta2);

	stuck[es].last_time = now_time;
	stuck[es].last_delta = delta;
	stuck[es].last_delta2 = delta2;

	if (!delta || !delta2 || !delta3)
		return 1;

	return 0;
}

/***************************************************************************
 * Health test interfaces
 ***************************************************************************/

/*
 * Disable all health tests
 */
void lrng_health_disable(void)
{
	struct lrng_health *health = &lrng_health;

	health->health_test_enabled = false;

	if (lrng_sp80090b_health_requested())
		pr_warn("SP800-90B compliance requested but the Linux RNG is NOT SP800-90B compliant\n");
}

/*
 * Hot code path - Perform health test on time stamp received from an event
 *
 * @now_time Time stamp
 */
enum lrng_health_res lrng_health_test(u32 now_time, enum lrng_internal_es es)
{
	struct lrng_health *health = &lrng_health;
	int stuck;

	if (!health->health_test_enabled)
		return lrng_health_pass;

	lrng_apt_insert(health, now_time, es);

	stuck = lrng_irq_stuck(es, now_time);
	lrng_rct(health, es, stuck);
	if (stuck) {
		/* SP800-90B disallows using a failing health test time stamp */
		return lrng_sp80090b_health_requested() ?
			lrng_health_fail_drop : lrng_health_fail_use;
	}

	return lrng_health_pass;
}
