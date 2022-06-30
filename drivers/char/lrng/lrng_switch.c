// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * LRNG DRNG switching support
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/lrng.h>

#include "lrng_es_aux.h"
#include "lrng_es_mgr.h"
#include "lrng_numa.h"

static int __maybe_unused
lrng_hash_switch(struct lrng_drng *drng_store, const void *cb, int node)
{
	const struct lrng_hash_cb *new_cb = (const struct lrng_hash_cb *)cb;
	const struct lrng_hash_cb *old_cb = drng_store->hash_cb;
	unsigned long flags;
	u32 i;
	void *new_hash = new_cb->hash_alloc();
	void *old_hash = drng_store->hash;
	int ret;

	if (IS_ERR(new_hash)) {
		pr_warn("could not allocate new LRNG pool hash (%ld)\n",
			PTR_ERR(new_hash));
		return PTR_ERR(new_hash);
	}

	if (new_cb->hash_digestsize(new_hash) > LRNG_MAX_DIGESTSIZE) {
		pr_warn("digest size of newly requested hash too large\n");
		new_cb->hash_dealloc(new_hash);
		return -EINVAL;
	}

	write_lock_irqsave(&drng_store->hash_lock, flags);

	/* Trigger the switch for each entropy source */
	for_each_lrng_es(i) {
		if (!lrng_es[i]->switch_hash)
			continue;
		ret = lrng_es[i]->switch_hash(drng_store, node, new_cb,
					      new_hash, old_cb);
		if (ret) {
			u32 j;

			/* Revert all already executed operations */
			for (j = 0; j < i; j++) {
				if (!lrng_es[j]->switch_hash)
					continue;
				WARN_ON(lrng_es[j]->switch_hash(drng_store,
								node, old_cb,
								old_hash,
								new_cb));
			}
			goto err;
		}
	}

	drng_store->hash = new_hash;
	drng_store->hash_cb = new_cb;
	old_cb->hash_dealloc(old_hash);
	pr_info("Conditioning function allocated for DRNG for NUMA node %d\n",
		node);

err:
	write_unlock_irqrestore(&drng_store->hash_lock, flags);
	return ret;
}

static int __maybe_unused
lrng_drng_switch(struct lrng_drng *drng_store, const void *cb, int node)
{
	const struct lrng_drng_cb *new_cb = (const struct lrng_drng_cb *)cb;
	const struct lrng_drng_cb *old_cb = drng_store->drng_cb;
	int ret;
	u8 seed[LRNG_DRNG_SECURITY_STRENGTH_BYTES];
	void *new_drng = new_cb->drng_alloc(LRNG_DRNG_SECURITY_STRENGTH_BYTES);
	void *old_drng = drng_store->drng;
	u32 current_security_strength;
	bool reset_drng = !lrng_get_available();

	if (IS_ERR(new_drng)) {
		pr_warn("could not allocate new DRNG for NUMA node %d (%ld)\n",
			node, PTR_ERR(new_drng));
		return PTR_ERR(new_drng);
	}

	current_security_strength = lrng_security_strength();
	mutex_lock(&drng_store->lock);

	/*
	 * Pull from existing DRNG to seed new DRNG regardless of seed status
	 * of old DRNG -- the entropy state for the DRNG is left unchanged which
	 * implies that als the new DRNG is reseeded when deemed necessary. This
	 * seeding of the new DRNG shall only ensure that the new DRNG has the
	 * same entropy as the old DRNG.
	 */
	ret = old_cb->drng_generate(old_drng, seed, sizeof(seed));
	mutex_unlock(&drng_store->lock);

	if (ret < 0) {
		reset_drng = true;
		pr_warn("getting random data from DRNG failed for NUMA node %d (%d)\n",
			node, ret);
	} else {
		/* seed new DRNG with data */
		ret = new_cb->drng_seed(new_drng, seed, ret);
		memzero_explicit(seed, sizeof(seed));
		if (ret < 0) {
			reset_drng = true;
			pr_warn("seeding of new DRNG failed for NUMA node %d (%d)\n",
				node, ret);
		} else {
			pr_debug("seeded new DRNG of NUMA node %d instance from old DRNG instance\n",
				 node);
		}
	}

	mutex_lock(&drng_store->lock);

	if (reset_drng)
		lrng_drng_reset(drng_store);

	drng_store->drng = new_drng;
	drng_store->drng_cb = new_cb;

	/* Reseed if previous LRNG security strength was insufficient */
	if (current_security_strength < lrng_security_strength())
		drng_store->force_reseed = true;

	/* Force oversampling seeding as we initialize DRNG */
	if (IS_ENABLED(CONFIG_CRYPTO_FIPS))
		lrng_unset_fully_seeded(drng_store);

	if (lrng_state_min_seeded())
		lrng_set_entropy_thresh(lrng_get_seed_entropy_osr(
						drng_store->fully_seeded));

	old_cb->drng_dealloc(old_drng);

	pr_info("DRNG of NUMA node %d switched\n", node);

	mutex_unlock(&drng_store->lock);
	return ret;
}

/*
 * Switch the existing DRNG and hash instances with new using the new crypto
 * callbacks. The caller must hold the lrng_crypto_cb_update lock.
 */
static int lrng_switch(const void *cb,
		       int(*switcher)(struct lrng_drng *drng_store,
				      const void *cb, int node))
{
	struct lrng_drng **lrng_drng = lrng_drng_instances();
	struct lrng_drng *lrng_drng_init = lrng_drng_init_instance();
	int ret = 0;

	if (lrng_drng) {
		u32 node;

		for_each_online_node(node) {
			if (lrng_drng[node])
				ret = switcher(lrng_drng[node], cb, node);
		}
	} else {
		ret = switcher(lrng_drng_init, cb, 0);
	}

	return 0;
}

/*
 * lrng_set_drng_cb - Register new cryptographic callback functions for DRNG
 * The registering implies that all old DRNG states are replaced with new
 * DRNG states.
 *
 * drng_cb: Callback functions to be registered -- if NULL, use the default
 *	    callbacks defined at compile time.
 *
 * Return:
 * * 0 on success
 * * < 0 on error
 */
int lrng_set_drng_cb(const struct lrng_drng_cb *drng_cb)
{
	struct lrng_drng *lrng_drng_init = lrng_drng_init_instance();
	int ret;

	if (!IS_ENABLED(CONFIG_LRNG_SWITCH_DRNG))
		return -EOPNOTSUPP;

	if (!drng_cb)
		drng_cb = lrng_default_drng_cb;

	mutex_lock(&lrng_crypto_cb_update);

	/*
	 * If a callback other than the default is set, allow it only to be
	 * set back to the default callback. This ensures that multiple
	 * different callbacks can be registered at the same time. If a
	 * callback different from the current callback and the default
	 * callback shall be set, the current callback must be deregistered
	 * (e.g. the kernel module providing it must be unloaded) and the new
	 * implementation can be registered.
	 */
	if ((drng_cb != lrng_default_drng_cb) &&
	    (lrng_drng_init->drng_cb != lrng_default_drng_cb)) {
		pr_warn("disallow setting new DRNG callbacks, unload the old callbacks first!\n");
		ret = -EINVAL;
		goto out;
	}

	ret = lrng_switch(drng_cb, lrng_drng_switch);
	/* The swtich may imply new entropy due to larger DRNG sec strength. */
	if (!ret)
		lrng_es_add_entropy();

out:
	mutex_unlock(&lrng_crypto_cb_update);
	return ret;
}
EXPORT_SYMBOL(lrng_set_drng_cb);

/*
 * lrng_set_hash_cb - Register new cryptographic callback functions for hash
 * The registering implies that all old hash states are replaced with new
 * hash states.
 *
 * @hash_cb: Callback functions to be registered -- if NULL, use the default
 *	     callbacks defined at compile time.
 *
 * Return:
 * * 0 on success
 * * < 0 on error
 */
int lrng_set_hash_cb(const struct lrng_hash_cb *hash_cb)
{
	struct lrng_drng *lrng_drng_init = lrng_drng_init_instance();
	int ret;

	if (!IS_ENABLED(CONFIG_LRNG_SWITCH_HASH))
		return -EOPNOTSUPP;

	if (!hash_cb)
		hash_cb = lrng_default_hash_cb;

	mutex_lock(&lrng_crypto_cb_update);

	/* Comment from lrng_set_drng_cb applies. */
	if ((hash_cb != lrng_default_hash_cb) &&
	    (lrng_drng_init->hash_cb != lrng_default_hash_cb)) {
		pr_warn("disallow setting new hash callbacks, unload the old callbacks first!\n");
		ret = -EINVAL;
		goto out;
	}

	ret = lrng_switch(hash_cb, lrng_hash_switch);
	/* The swtich may imply new entropy due to larger digest size. */
	if (!ret)
		lrng_es_add_entropy();

out:
	mutex_unlock(&lrng_crypto_cb_update);
	return ret;
}
EXPORT_SYMBOL(lrng_set_hash_cb);
