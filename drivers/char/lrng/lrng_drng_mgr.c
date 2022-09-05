// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * LRNG DRNG management
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/lrng.h>
#include <linux/fips.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/wait.h>

#include "lrng_drng_atomic.h"
#include "lrng_drng_chacha20.h"
#include "lrng_drng_drbg.h"
#include "lrng_drng_kcapi.h"
#include "lrng_drng_mgr.h"
#include "lrng_es_aux.h"
#include "lrng_es_mgr.h"
#include "lrng_interface_random_kernel.h"
#include "lrng_numa.h"
#include "lrng_sha.h"

/*
 * Maximum number of seconds between DRNG reseed intervals of the DRNG. Note,
 * this is enforced with the next request of random numbers from the
 * DRNG. Setting this value to zero implies a reseeding attempt before every
 * generated random number.
 */
int lrng_drng_reseed_max_time = 600;

/*
 * Is LRNG for general-purpose use (i.e. is at least the lrng_drng_init
 * fully allocated)?
 */
static atomic_t lrng_avail = ATOMIC_INIT(0);

/* Guard protecting all crypto callback update operation of all DRNGs. */
DEFINE_MUTEX(lrng_crypto_cb_update);

/*
 * Default hash callback that provides the crypto primitive right from the
 * kernel start. It must not perform any memory allocation operation, but
 * simply perform the hash calculation.
 */
const struct lrng_hash_cb *lrng_default_hash_cb = &lrng_sha_hash_cb;

/*
 * Default DRNG callback that provides the crypto primitive which is
 * allocated either during late kernel boot stage. So, it is permissible for
 * the callback to perform memory allocation operations.
 */
const struct lrng_drng_cb *lrng_default_drng_cb =
#if defined(CONFIG_LRNG_DFLT_DRNG_CHACHA20)
	&lrng_cc20_drng_cb;
#elif defined(CONFIG_LRNG_DFLT_DRNG_DRBG)
	&lrng_drbg_cb;
#elif defined(CONFIG_LRNG_DFLT_DRNG_KCAPI)
	&lrng_kcapi_drng_cb;
#else
#error "Unknown default DRNG selected"
#endif

/* DRNG for non-atomic use cases */
static struct lrng_drng lrng_drng_init = {
	LRNG_DRNG_STATE_INIT(lrng_drng_init, NULL, NULL, NULL,
			     &lrng_sha_hash_cb),
	.lock = __MUTEX_INITIALIZER(lrng_drng_init.lock),
};

static u32 max_wo_reseed = LRNG_DRNG_MAX_WITHOUT_RESEED;
#ifdef CONFIG_LRNG_RUNTIME_MAX_WO_RESEED_CONFIG
module_param(max_wo_reseed, uint, 0444);
MODULE_PARM_DESC(max_wo_reseed,
		 "Maximum number of DRNG generate operation without full reseed\n");
#endif

/* Wait queue to wait until the LRNG is initialized - can freely be used */
DECLARE_WAIT_QUEUE_HEAD(lrng_init_wait);

/********************************** Helper ************************************/

bool lrng_get_available(void)
{
	return likely(atomic_read(&lrng_avail));
}

struct lrng_drng *lrng_drng_init_instance(void)
{
	return &lrng_drng_init;
}

struct lrng_drng *lrng_drng_node_instance(void)
{
	struct lrng_drng **lrng_drng = lrng_drng_instances();
	int node = numa_node_id();

	if (lrng_drng && lrng_drng[node])
		return lrng_drng[node];

	return lrng_drng_init_instance();
}

void lrng_drng_reset(struct lrng_drng *drng)
{
	atomic_set(&drng->requests, LRNG_DRNG_RESEED_THRESH);
	atomic_set(&drng->requests_since_fully_seeded, 0);
	drng->last_seeded = jiffies;
	drng->fully_seeded = false;
	drng->force_reseed = true;
	pr_debug("reset DRNG\n");
}

/* Initialize the DRNG, except the mutex lock */
int lrng_drng_alloc_common(struct lrng_drng *drng,
			   const struct lrng_drng_cb *drng_cb)
{
	if (!drng || !drng_cb)
		return -EINVAL;

	drng->drng_cb = drng_cb;
	drng->drng = drng_cb->drng_alloc(LRNG_DRNG_SECURITY_STRENGTH_BYTES);
	if (IS_ERR(drng->drng))
		return PTR_ERR(drng->drng);

	lrng_drng_reset(drng);
	return 0;
}

/* Initialize the default DRNG during boot and perform its seeding */
int lrng_drng_initalize(void)
{
	int ret;

	if (lrng_get_available())
		return 0;

	/* Catch programming error */
	WARN_ON(lrng_drng_init.hash_cb != lrng_default_hash_cb);

	mutex_lock(&lrng_drng_init.lock);
	if (lrng_get_available()) {
		mutex_unlock(&lrng_drng_init.lock);
		return 0;
	}

	ret = lrng_drng_alloc_common(&lrng_drng_init, lrng_default_drng_cb);
	mutex_unlock(&lrng_drng_init.lock);
	if (ret)
		return ret;

	pr_debug("LRNG for general use is available\n");
	atomic_set(&lrng_avail, 1);

	/* Seed the DRNG with any entropy available */
	if (!lrng_pool_trylock()) {
		pr_info("Initial DRNG initialized triggering first seeding\n");
		lrng_drng_seed_work(NULL);
	} else {
		pr_info("Initial DRNG initialized without seeding\n");
	}

	return 0;
}

static int __init lrng_drng_make_available(void)
{
	return lrng_drng_initalize();
}
late_initcall(lrng_drng_make_available);

bool lrng_sp80090c_compliant(void)
{
	/* SP800-90C compliant oversampling is only requested in FIPS mode */
	return fips_enabled;
}

/************************* Random Number Generation ***************************/

/* Inject a data buffer into the DRNG - caller must hold its lock */
void lrng_drng_inject(struct lrng_drng *drng, const u8 *inbuf, u32 inbuflen,
		      bool fully_seeded, const char *drng_type)
{
	BUILD_BUG_ON(LRNG_DRNG_RESEED_THRESH > INT_MAX);
	pr_debug("seeding %s DRNG with %u bytes\n", drng_type, inbuflen);
	if (drng->drng_cb->drng_seed(drng->drng, inbuf, inbuflen) < 0) {
		pr_warn("seeding of %s DRNG failed\n", drng_type);
		drng->force_reseed = true;
	} else {
		int gc = LRNG_DRNG_RESEED_THRESH - atomic_read(&drng->requests);

		pr_debug("%s DRNG stats since last seeding: %lu secs; generate calls: %d\n",
			 drng_type,
			 (time_after(jiffies, drng->last_seeded) ?
			  (jiffies - drng->last_seeded) : 0) / HZ, gc);

		/* Count the numbers of generate ops since last fully seeded */
		if (fully_seeded)
			atomic_set(&drng->requests_since_fully_seeded, 0);
		else
			atomic_add(gc, &drng->requests_since_fully_seeded);

		drng->last_seeded = jiffies;
		atomic_set(&drng->requests, LRNG_DRNG_RESEED_THRESH);
		drng->force_reseed = false;

		if (!drng->fully_seeded) {
			drng->fully_seeded = fully_seeded;
			if (drng->fully_seeded)
				pr_debug("%s DRNG fully seeded\n", drng_type);
		}
	}
}

/* Perform the seeding of the DRNG with data from entropy source */
static void lrng_drng_seed_es(struct lrng_drng *drng)
{
	struct entropy_buf seedbuf __aligned(LRNG_KCAPI_ALIGN);

	lrng_fill_seed_buffer(&seedbuf,
			      lrng_get_seed_entropy_osr(drng->fully_seeded));

	mutex_lock(&drng->lock);
	lrng_drng_inject(drng, (u8 *)&seedbuf, sizeof(seedbuf),
			 lrng_fully_seeded_eb(drng->fully_seeded, &seedbuf),
			 "regular");
	mutex_unlock(&drng->lock);

	/* Set the seeding state of the LRNG */
	lrng_init_ops(&seedbuf);

	memzero_explicit(&seedbuf, sizeof(seedbuf));
}

static void lrng_drng_seed(struct lrng_drng *drng)
{
	BUILD_BUG_ON(LRNG_MIN_SEED_ENTROPY_BITS >
		     LRNG_DRNG_SECURITY_STRENGTH_BITS);

	if (lrng_get_available()) {
		/* (Re-)Seed DRNG */
		lrng_drng_seed_es(drng);
		/* (Re-)Seed atomic DRNG from regular DRNG */
		lrng_drng_atomic_seed_drng(drng);
	} else {
		/*
		 * If no-one is waiting for the DRNG, seed the atomic DRNG
		 * directly from the entropy sources.
		 */
		if (!wq_has_sleeper(&lrng_init_wait) &&
		    !lrng_ready_chain_has_sleeper())
			lrng_drng_atomic_seed_es();
		else
			lrng_init_ops(NULL);
	}
}

static void _lrng_drng_seed_work(struct lrng_drng *drng, u32 node)
{
	pr_debug("reseed triggered by system events for DRNG on NUMA node %d\n",
		 node);
	lrng_drng_seed(drng);
	if (drng->fully_seeded) {
		/* Prevent reseed storm */
		drng->last_seeded += node * 100 * HZ;
	}
}

/*
 * DRNG reseed trigger: Kernel thread handler triggered by the schedule_work()
 */
void lrng_drng_seed_work(struct work_struct *dummy)
{
	struct lrng_drng **lrng_drng = lrng_drng_instances();
	u32 node;

	if (lrng_drng) {
		for_each_online_node(node) {
			struct lrng_drng *drng = lrng_drng[node];

			if (drng && !drng->fully_seeded) {
				_lrng_drng_seed_work(drng, node);
				goto out;
			}
		}
	} else {
		if (!lrng_drng_init.fully_seeded) {
			_lrng_drng_seed_work(&lrng_drng_init, 0);
			goto out;
		}
	}

	lrng_pool_all_numa_nodes_seeded(true);

out:
	/* Allow the seeding operation to be called again */
	lrng_pool_unlock();
}

/* Force all DRNGs to reseed before next generation */
void lrng_drng_force_reseed(void)
{
	struct lrng_drng **lrng_drng = lrng_drng_instances();
	u32 node;

	/*
	 * If the initial DRNG is over the reseed threshold, allow a forced
	 * reseed only for the initial DRNG as this is the fallback for all. It
	 * must be kept seeded before all others to keep the LRNG operational.
	 */
	if (!lrng_drng ||
	    (atomic_read_u32(&lrng_drng_init.requests_since_fully_seeded) >
	     LRNG_DRNG_RESEED_THRESH)) {
		lrng_drng_init.force_reseed = lrng_drng_init.fully_seeded;
		pr_debug("force reseed of initial DRNG\n");
		return;
	}
	for_each_online_node(node) {
		struct lrng_drng *drng = lrng_drng[node];

		if (!drng)
			continue;

		drng->force_reseed = drng->fully_seeded;
		pr_debug("force reseed of DRNG on node %u\n", node);
	}
	lrng_drng_atomic_force_reseed();
}
EXPORT_SYMBOL(lrng_drng_force_reseed);

static bool lrng_drng_must_reseed(struct lrng_drng *drng)
{
	return (atomic_dec_and_test(&drng->requests) ||
		drng->force_reseed ||
		time_after(jiffies,
			   drng->last_seeded + lrng_drng_reseed_max_time * HZ));
}

/*
 * lrng_drng_get() - Get random data out of the DRNG which is reseeded
 * frequently.
 *
 * @drng: DRNG instance
 * @outbuf: buffer for storing random data
 * @outbuflen: length of outbuf
 *
 * Return:
 * * < 0 in error case (DRNG generation or update failed)
 * * >=0 returning the returned number of bytes
 */
int lrng_drng_get(struct lrng_drng *drng, u8 *outbuf, u32 outbuflen)
{
	u32 processed = 0;

	if (!outbuf || !outbuflen)
		return 0;

	if (!lrng_get_available())
		return -EOPNOTSUPP;

	outbuflen = min_t(size_t, outbuflen, INT_MAX);

	/* If DRNG operated without proper reseed for too long, block LRNG */
	BUILD_BUG_ON(LRNG_DRNG_MAX_WITHOUT_RESEED < LRNG_DRNG_RESEED_THRESH);
	if (atomic_read_u32(&drng->requests_since_fully_seeded) > max_wo_reseed)
		lrng_unset_fully_seeded(drng);

	while (outbuflen) {
		u32 todo = min_t(u32, outbuflen, LRNG_DRNG_MAX_REQSIZE);
		int ret;

		if (lrng_drng_must_reseed(drng)) {
			if (lrng_pool_trylock()) {
				drng->force_reseed = true;
			} else {
				lrng_drng_seed(drng);
				lrng_pool_unlock();
			}
		}

		mutex_lock(&drng->lock);
		ret = drng->drng_cb->drng_generate(drng->drng,
						   outbuf + processed, todo);
		mutex_unlock(&drng->lock);
		if (ret <= 0) {
			pr_warn("getting random data from DRNG failed (%d)\n",
				ret);
			return -EFAULT;
		}
		processed += ret;
		outbuflen -= ret;
	}

	return processed;
}

int lrng_drng_get_sleep(u8 *outbuf, u32 outbuflen)
{
	struct lrng_drng **lrng_drng = lrng_drng_instances();
	struct lrng_drng *drng = &lrng_drng_init;
	int ret, node = numa_node_id();

	might_sleep();

	if (lrng_drng && lrng_drng[node] && lrng_drng[node]->fully_seeded)
		drng = lrng_drng[node];

	ret = lrng_drng_initalize();
	if (ret)
		return ret;

	return lrng_drng_get(drng, outbuf, outbuflen);
}

/* Reset LRNG such that all existing entropy is gone */
static void _lrng_reset(struct work_struct *work)
{
	struct lrng_drng **lrng_drng = lrng_drng_instances();

	if (!lrng_drng) {
		mutex_lock(&lrng_drng_init.lock);
		lrng_drng_reset(&lrng_drng_init);
		mutex_unlock(&lrng_drng_init.lock);
	} else {
		u32 node;

		for_each_online_node(node) {
			struct lrng_drng *drng = lrng_drng[node];

			if (!drng)
				continue;
			mutex_lock(&drng->lock);
			lrng_drng_reset(drng);
			mutex_unlock(&drng->lock);
		}
	}
	lrng_drng_atomic_reset();
	lrng_set_entropy_thresh(LRNG_INIT_ENTROPY_BITS);

	lrng_reset_state();
}

static DECLARE_WORK(lrng_reset_work, _lrng_reset);

void lrng_reset(void)
{
	schedule_work(&lrng_reset_work);
}

/******************* Generic LRNG kernel output interfaces ********************/

int lrng_drng_sleep_while_nonoperational(int nonblock)
{
	if (likely(lrng_state_operational()))
		return 0;
	if (nonblock)
		return -EAGAIN;
	return wait_event_interruptible(lrng_init_wait,
					lrng_state_operational());
}

int lrng_drng_sleep_while_non_min_seeded(void)
{
	if (likely(lrng_state_min_seeded()))
		return 0;
	return wait_event_interruptible(lrng_init_wait,
					lrng_state_min_seeded());
}

void lrng_get_random_bytes_full(void *buf, int nbytes)
{
	lrng_drng_sleep_while_nonoperational(0);
	lrng_drng_get_sleep((u8 *)buf, (u32)nbytes);
}
EXPORT_SYMBOL(lrng_get_random_bytes_full);

void lrng_get_random_bytes_min(void *buf, int nbytes)
{
	lrng_drng_sleep_while_non_min_seeded();
	lrng_drng_get_sleep((u8 *)buf, (u32)nbytes);
}
EXPORT_SYMBOL(lrng_get_random_bytes_min);
