// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * LRNG Kernel space interfaces API/ABI compliant to linux/random.h
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hw_random.h>
#include <linux/kthread.h>
#include <linux/lrng.h>
#include <linux/random.h>

#define CREATE_TRACE_POINTS
#include <trace/events/random.h>

#include "lrng_es_aux.h"
#include "lrng_es_irq.h"
#include "lrng_es_mgr.h"
#include "lrng_interface_dev_common.h"
#include "lrng_interface_random_kernel.h"

static LIST_HEAD(lrng_ready_list);
static DEFINE_SPINLOCK(lrng_ready_list_lock);

/********************************** Helper ***********************************/

int __init rand_initialize(void)
{
	return lrng_rand_initialize();
}

bool lrng_ready_list_has_sleeper(void)
{
	return list_empty(&lrng_ready_list);
}

/*
 * lrng_process_ready_list() - Ping all kernel internal callers waiting until
 * the DRNG is completely initialized to inform that the DRNG reached that
 * seed level.
 *
 * When the SP800-90B testing is enabled, the ping only happens if the SP800-90B
 * startup health tests are completed. This implies that kernel internal
 * callers always have an SP800-90B compliant noise source when being
 * pinged.
 */
void lrng_process_ready_list(void)
{
	unsigned long flags;
	struct random_ready_callback *rdy, *tmp;

	if (!lrng_state_operational())
		return;

	spin_lock_irqsave(&lrng_ready_list_lock, flags);
	list_for_each_entry_safe(rdy, tmp, &lrng_ready_list, list) {
		struct module *owner = rdy->owner;

		list_del_init(&rdy->list);
		rdy->func(rdy);
		module_put(owner);
	}
	spin_unlock_irqrestore(&lrng_ready_list_lock, flags);
}

/************************ LRNG kernel input interfaces ************************/

/*
 * add_hwgenerator_randomness() - Interface for in-kernel drivers of true
 * hardware RNGs.
 *
 * Those devices may produce endless random bits and will be throttled
 * when our pool is full.
 *
 * @buffer: buffer holding the entropic data from HW noise sources to be used to
 *	    insert into entropy pool.
 * @count: length of buffer
 * @entropy_bits: amount of entropy in buffer (value is in bits)
 */
void add_hwgenerator_randomness(const char *buffer, size_t count,
				size_t entropy_bits)
{
	/*
	 * Suspend writing if we are fully loaded with entropy.
	 * We'll be woken up again once below lrng_write_wakeup_thresh,
	 * or when the calling thread is about to terminate.
	 */
	wait_event_interruptible(lrng_write_wait,
				lrng_need_entropy() ||
				lrng_state_exseed_allow(lrng_noise_source_hw) ||
				kthread_should_stop());
	lrng_state_exseed_set(lrng_noise_source_hw, false);
	lrng_pool_insert_aux(buffer, count, entropy_bits);
}
EXPORT_SYMBOL_GPL(add_hwgenerator_randomness);

/*
 * add_bootloader_randomness() - Handle random seed passed by bootloader.
 *
 * If the seed is trustworthy, it would be regarded as hardware RNGs. Otherwise
 * it would be regarded as device data.
 * The decision is controlled by CONFIG_RANDOM_TRUST_BOOTLOADER.
 *
 * @buf: buffer holding the entropic data from HW noise sources to be used to
 *	 insert into entropy pool.
 * @size: length of buffer
 */
void add_bootloader_randomness(const void *buf, unsigned int size)
{
	lrng_pool_insert_aux(buf, size,
			     IS_ENABLED(CONFIG_RANDOM_TRUST_BOOTLOADER) ?
			     size * 8 : 0);
}
EXPORT_SYMBOL_GPL(add_bootloader_randomness);

/*
 * Callback for HID layer -- use the HID event values to stir the entropy pool
 */
void add_input_randomness(unsigned int type, unsigned int code,
			  unsigned int value)
{
	static unsigned char last_value;

	/* ignore autorepeat and the like */
	if (value == last_value)
		return;

	last_value = value;

	lrng_irq_array_add_u32((type << 4) ^ code ^ (code >> 4) ^ value);
}
EXPORT_SYMBOL_GPL(add_input_randomness);

/*
 * add_device_randomness() - Add device- or boot-specific data to the entropy
 * pool to help initialize it.
 *
 * None of this adds any entropy; it is meant to avoid the problem of
 * the entropy pool having similar initial state across largely
 * identical devices.
 *
 * @buf: buffer holding the entropic data from HW noise sources to be used to
 *	 insert into entropy pool.
 * @size: length of buffer
 */
void add_device_randomness(const void *buf, unsigned int size)
{
	lrng_pool_insert_aux((u8 *)buf, size, 0);
}
EXPORT_SYMBOL(add_device_randomness);

#ifdef CONFIG_BLOCK
void rand_initialize_disk(struct gendisk *disk) { }
void add_disk_randomness(struct gendisk *disk) { }
EXPORT_SYMBOL(add_disk_randomness);
#endif

#ifndef CONFIG_LRNG_IRQ
void add_interrupt_randomness(int irq, int irq_flg) { }
EXPORT_SYMBOL(add_interrupt_randomness);
#endif

/*
 * unregister_random_ready_notifier() - Delete a previously registered readiness
 * callback function.
 *
 * @rdy: callback definition that was registered initially
 */
void del_random_ready_callback(struct random_ready_callback *rdy)
{
	unsigned long flags;
	struct module *owner = NULL;

	spin_lock_irqsave(&lrng_ready_list_lock, flags);
	if (!list_empty(&rdy->list)) {
		list_del_init(&rdy->list);
		owner = rdy->owner;
	}
	spin_unlock_irqrestore(&lrng_ready_list_lock, flags);

	module_put(owner);
}
EXPORT_SYMBOL(del_random_ready_callback);

/*
 * add_random_ready_callback() - Add a callback function that will be
 * invoked when the DRNG is fully initialized and seeded.
 *
 * @rdy: callback definition to be invoked when the LRNG is seeded
 *
 * Return:
 * * 0 if callback is successfully added
 * * -EALREADY if pool is already initialised (callback not called)
 * * -ENOENT if module for callback is not alive
 */
int add_random_ready_callback(struct random_ready_callback *rdy)
{
	struct module *owner;
	unsigned long flags;
	int err = -EALREADY;

	if (likely(lrng_state_operational()))
		return err;

	owner = rdy->owner;
	if (!try_module_get(owner))
		return -ENOENT;

	spin_lock_irqsave(&lrng_ready_list_lock, flags);
	if (lrng_state_operational())
		goto out;

	owner = NULL;

	list_add(&rdy->list, &lrng_ready_list);
	err = 0;

out:
	spin_unlock_irqrestore(&lrng_ready_list_lock, flags);

	module_put(owner);

	return err;
}
EXPORT_SYMBOL(add_random_ready_callback);

/*********************** LRNG kernel output interfaces ************************/

/*
 * get_random_bytes() - Provider of cryptographic strong random numbers for
 * kernel-internal usage.
 *
 * This function is appropriate for all in-kernel use cases. However,
 * it will always use the ChaCha20 DRNG.
 *
 * @buf: buffer to store the random bytes
 * @nbytes: size of the buffer
 */
void get_random_bytes(void *buf, int nbytes)
{
	lrng_get_random_bytes(buf, nbytes);
}
EXPORT_SYMBOL(get_random_bytes);

/*
 * wait_for_random_bytes() - Wait for the LRNG to be seeded and thus
 * guaranteed to supply cryptographically secure random numbers.
 *
 * This applies to: the /dev/urandom device, the get_random_bytes function,
 * and the get_random_{u32,u64,int,long} family of functions. Using any of
 * these functions without first calling this function forfeits the guarantee
 * of security.
 *
 * Return:
 * * 0 if the LRNG has been seeded.
 * * -ERESTARTSYS if the function was interrupted by a signal.
 */
int wait_for_random_bytes(void)
{
	return lrng_drng_sleep_while_non_min_seeded();
}
EXPORT_SYMBOL(wait_for_random_bytes);

/*
 * get_random_bytes_arch() - This function will use the architecture-specific
 * hardware random number generator if it is available.
 *
 * The arch-specific hw RNG will almost certainly be faster than what we can
 * do in software, but it is impossible to verify that it is implemented
 * securely (as opposed, to, say, the AES encryption of a sequence number using
 * a key known by the NSA).  So it's useful if we need the speed, but only if
 * we're willing to trust the hardware manufacturer not to have put in a back
 * door.
 *
 * @buf: buffer allocated by caller to store the random data in
 * @nbytes: length of outbuf
 *
 * Return: number of bytes filled in.
 */
int __must_check get_random_bytes_arch(void *buf, int nbytes)
{
	u8 *p = buf;

	while (nbytes) {
		unsigned long v;
		int chunk = min_t(int, nbytes, sizeof(unsigned long));

		if (!arch_get_random_long(&v))
			break;

		memcpy(p, &v, chunk);
		p += chunk;
		nbytes -= chunk;
	}

	if (nbytes)
		lrng_get_random_bytes(p, nbytes);

	return nbytes;
}
EXPORT_SYMBOL(get_random_bytes_arch);

/*
 * Returns whether or not the LRNG has been seeded.
 *
 * Returns: true if the urandom pool has been seeded.
 *          false if the urandom pool has not been seeded.
 */
bool rng_is_initialized(void)
{
	return lrng_state_operational();
}
EXPORT_SYMBOL(rng_is_initialized);
