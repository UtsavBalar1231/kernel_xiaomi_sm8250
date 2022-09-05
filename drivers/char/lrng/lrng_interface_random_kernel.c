// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * LRNG Kernel space interfaces API/ABI compliant to linux/random.h
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/blkdev.h>
#include <linux/hw_random.h>
#include <linux/kthread.h>
#include <linux/lrng.h>
#include <linux/random.h>

#include "lrng_es_aux.h"
#include "lrng_es_irq.h"
#include "lrng_es_mgr.h"
#include "lrng_interface_dev_common.h"
#include "lrng_interface_random_kernel.h"

static RAW_NOTIFIER_HEAD(lrng_ready_chain);
static DEFINE_SPINLOCK(lrng_ready_chain_lock);
static unsigned int lrng_ready_chain_used = 0;

/********************************** Helper ***********************************/

int __init random_init(const char *command_line)
{
	int ret = lrng_rand_initialize();

	lrng_pool_insert_aux(command_line, strlen(command_line), 0);
	return ret;
}
early_initcall(rand_initialize);

bool lrng_ready_chain_has_sleeper(void)
{
	return !!lrng_ready_chain_used;
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

	if (!lrng_state_operational())
		return;

	spin_lock_irqsave(&lrng_ready_chain_lock, flags);
	raw_notifier_call_chain(&lrng_ready_chain, 0, NULL);
	spin_unlock_irqrestore(&lrng_ready_chain_lock, flags);
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
void add_hwgenerator_randomness(const void *buffer, size_t count,
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
void add_bootloader_randomness(const void *buf, size_t size)
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
void add_device_randomness(const void *buf, size_t size)
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
void add_interrupt_randomness(int irq) { }
EXPORT_SYMBOL(add_interrupt_randomness);
#endif

/*
 * unregister_random_ready_notifier() - Delete a previously registered readiness
 * callback function.
 *
 * @nb: callback definition that was registered initially
 */
int unregister_random_ready_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&lrng_ready_chain_lock, flags);
	ret = raw_notifier_chain_unregister(&lrng_ready_chain, nb);
	spin_unlock_irqrestore(&lrng_ready_chain_lock, flags);

	if (!ret && lrng_ready_chain_used)
		lrng_ready_chain_used--;

	return ret;
}
EXPORT_SYMBOL(unregister_random_ready_notifier);

/*
 * register_random_ready_notifier() - Add a callback function that will be
 * invoked when the DRNG is fully initialized and seeded.
 *
 * @nb: callback definition to be invoked when the LRNG is seeded
 *
 * Return:
 * * 0 if callback is successfully added
 * * -EALREADY if pool is already initialised (callback not called)
 */
int register_random_ready_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int err = -EALREADY;

	if (likely(lrng_state_operational()))
		return err;

	spin_lock_irqsave(&lrng_ready_chain_lock, flags);
	if (!lrng_state_operational())
		err = raw_notifier_chain_register(&lrng_ready_chain, nb);
	spin_unlock_irqrestore(&lrng_ready_chain_lock, flags);

	if (!err)
		lrng_ready_chain_used++;

	return err;
}
EXPORT_SYMBOL(register_random_ready_notifier);

#if IS_ENABLED(CONFIG_VMGENID)
static BLOCKING_NOTIFIER_HEAD(lrng_vmfork_chain);

/*
 * Handle a new unique VM ID, which is unique, not secret, so we
 * don't credit it, but we do immediately force a reseed after so
 * that it's used by the crng posthaste.
 */
void add_vmfork_randomness(const void *unique_vm_id, size_t size)
{
	add_device_randomness(unique_vm_id, size);
	if (lrng_state_operational())
		lrng_drng_force_reseed();
	blocking_notifier_call_chain(&lrng_vmfork_chain, 0, NULL);
}
#if IS_MODULE(CONFIG_VMGENID)
EXPORT_SYMBOL_GPL(add_vmfork_randomness);
#endif

int register_random_vmfork_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&lrng_vmfork_chain, nb);
}
EXPORT_SYMBOL_GPL(register_random_vmfork_notifier);

int unregister_random_vmfork_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&lrng_vmfork_chain, nb);
}
EXPORT_SYMBOL_GPL(unregister_random_vmfork_notifier);
#endif

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
void get_random_bytes(void *buf, size_t nbytes)
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
size_t __must_check get_random_bytes_arch(void *buf, size_t nbytes)
{
	size_t left = nbytes;
	u8 *p = buf;

	while (left) {
		unsigned long v;
		size_t chunk = min_t(size_t, left, sizeof(unsigned long));

		if (!arch_get_random_long(&v))
			break;

		memcpy(p, &v, chunk);
		p += chunk;
		left -= chunk;
	}

	return nbytes - left;
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
