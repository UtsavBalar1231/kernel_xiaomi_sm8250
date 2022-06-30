// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * LRNG User and kernel space interfaces
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/random.h>
#include <linux/slab.h>
#include <linux/sched/signal.h>

#include "lrng_drng_mgr.h"
#include "lrng_es_aux.h"
#include "lrng_es_mgr.h"
#include "lrng_interface_dev_common.h"

DECLARE_WAIT_QUEUE_HEAD(lrng_write_wait);
static struct fasync_struct *fasync;

static bool lrng_seed_hw = true;	/* Allow HW to provide seed */
static bool lrng_seed_user = true;	/* Allow user space to provide seed */

/********************************** Helper ***********************************/

static u32 lrng_get_aux_ent(void)
{
	return lrng_es[lrng_ext_es_aux]->curr_entropy(0);
}

/* Is the DRNG seed level too low? */
bool lrng_need_entropy(void)
{
	return (lrng_get_aux_ent() < lrng_write_wakeup_bits);
}

void lrng_writer_wakeup(void)
{
	if (lrng_need_entropy() && wq_has_sleeper(&lrng_write_wait)) {
		wake_up_interruptible(&lrng_write_wait);
		kill_fasync(&fasync, SIGIO, POLL_OUT);
	}
}

void lrng_init_wakeup_dev(void)
{
	kill_fasync(&fasync, SIGIO, POLL_IN);
}

/* External entropy provider is allowed to provide seed data */
bool lrng_state_exseed_allow(enum lrng_external_noise_source source)
{
	if (source == lrng_noise_source_hw)
		return lrng_seed_hw;
	return lrng_seed_user;
}

/* Enable / disable external entropy provider to furnish seed */
void lrng_state_exseed_set(enum lrng_external_noise_source source, bool type)
{
	/*
	 * If the LRNG is not yet operational, allow all entropy sources
	 * to deliver data unconditionally to get fully seeded asap.
	 */
	if (!lrng_state_operational())
		return;

	if (source == lrng_noise_source_hw)
		lrng_seed_hw = type;
	else
		lrng_seed_user = type;
}

void lrng_state_exseed_allow_all(void)
{
	lrng_state_exseed_set(lrng_noise_source_hw, true);
	lrng_state_exseed_set(lrng_noise_source_user, true);
}

/************************ LRNG user output interfaces *************************/

ssize_t lrng_read_common(char __user *buf, size_t nbytes)
{
	ssize_t ret = 0;
	u8 tmpbuf[LRNG_DRNG_BLOCKSIZE] __aligned(LRNG_KCAPI_ALIGN);
	u8 *tmp_large = NULL, *tmp = tmpbuf;
	u32 tmplen = sizeof(tmpbuf);

	if (nbytes == 0)
		return 0;

	/*
	 * Satisfy large read requests -- as the common case are smaller
	 * request sizes, such as 16 or 32 bytes, avoid a kmalloc overhead for
	 * those by using the stack variable of tmpbuf.
	 */
	if (!CONFIG_BASE_SMALL && (nbytes > sizeof(tmpbuf))) {
		tmplen = min_t(u32, nbytes, LRNG_DRNG_MAX_REQSIZE);
		tmp_large = kmalloc(tmplen + LRNG_KCAPI_ALIGN, GFP_KERNEL);
		if (!tmp_large)
			tmplen = sizeof(tmpbuf);
		else
			tmp = PTR_ALIGN(tmp_large, LRNG_KCAPI_ALIGN);
	}

	while (nbytes) {
		u32 todo = min_t(u32, nbytes, tmplen);
		int rc = 0;

		/* Reschedule if we received a large request. */
		if ((tmp_large) && need_resched()) {
			if (signal_pending(current)) {
				if (ret == 0)
					ret = -ERESTARTSYS;
				break;
			}
			schedule();
		}

		rc = lrng_drng_get_sleep(tmp, todo);
		if (rc <= 0) {
			if (rc < 0)
				ret = rc;
			break;
		}
		if (copy_to_user(buf, tmp, rc)) {
			ret = -EFAULT;
			break;
		}

		nbytes -= rc;
		buf += rc;
		ret += rc;
	}

	/* Wipe data just returned from memory */
	if (tmp_large)
		kfree_sensitive(tmp_large);
	else
		memzero_explicit(tmpbuf, sizeof(tmpbuf));

	return ret;
}

ssize_t lrng_read_common_block(int nonblock, char __user *buf, size_t nbytes)
{
	int ret;

	if (nbytes == 0)
		return 0;

	ret = lrng_drng_sleep_while_nonoperational(nonblock);
	if (ret)
		return ret;

	return lrng_read_common(buf, nbytes);
}

ssize_t lrng_drng_read_block(struct file *file, char __user *buf, size_t nbytes,
			     loff_t *ppos)
{
	return lrng_read_common_block(file->f_flags & O_NONBLOCK, buf, nbytes);
}

__poll_t lrng_random_poll(struct file *file, poll_table *wait)
{
	__poll_t mask;

	poll_wait(file, &lrng_init_wait, wait);
	poll_wait(file, &lrng_write_wait, wait);
	mask = 0;
	if (lrng_state_operational())
		mask |= EPOLLIN | EPOLLRDNORM;
	if (lrng_need_entropy() ||
	    lrng_state_exseed_allow(lrng_noise_source_user)) {
		lrng_state_exseed_set(lrng_noise_source_user, false);
		mask |= EPOLLOUT | EPOLLWRNORM;
	}
	return mask;
}

ssize_t lrng_drng_write_common(const char __user *buffer, size_t count,
			       u32 entropy_bits)
{
	ssize_t ret = 0;
	u8 buf[64] __aligned(LRNG_KCAPI_ALIGN);
	const char __user *p = buffer;
	u32 orig_entropy_bits = entropy_bits;

	if (!lrng_get_available()) {
		ret = lrng_drng_initalize();
		if (!ret)
			return ret;
	}

	count = min_t(size_t, count, INT_MAX);
	while (count > 0) {
		size_t bytes = min_t(size_t, count, sizeof(buf));
		u32 ent = min_t(u32, bytes<<3, entropy_bits);

		if (copy_from_user(&buf, p, bytes))
			return -EFAULT;
		/* Inject data into entropy pool */
		lrng_pool_insert_aux(buf, bytes, ent);

		count -= bytes;
		p += bytes;
		ret += bytes;
		entropy_bits -= ent;

		cond_resched();
	}

	/* Force reseed of DRNG during next data request. */
	if (!orig_entropy_bits)
		lrng_drng_force_reseed();

	return ret;
}

ssize_t lrng_drng_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *ppos)
{
	return lrng_drng_write_common(buffer, count, 0);
}

long lrng_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	u32 digestsize_bits;
	int size, ent_count_bits;
	int __user *p = (int __user *)arg;

	switch (cmd) {
	case RNDGETENTCNT:
		ent_count_bits = lrng_avail_entropy();
		if (put_user(ent_count_bits, p))
			return -EFAULT;
		return 0;
	case RNDADDTOENTCNT:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if (get_user(ent_count_bits, p))
			return -EFAULT;
		ent_count_bits = (int)lrng_get_aux_ent() + ent_count_bits;
		if (ent_count_bits < 0)
			ent_count_bits = 0;
		digestsize_bits = lrng_get_digestsize();
		if (ent_count_bits > digestsize_bits)
			ent_count_bits = digestsize_bits;
		lrng_pool_set_entropy(ent_count_bits);
		return 0;
	case RNDADDENTROPY:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if (get_user(ent_count_bits, p++))
			return -EFAULT;
		if (ent_count_bits < 0)
			return -EINVAL;
		if (get_user(size, p++))
			return -EFAULT;
		if (size < 0)
			return -EINVAL;
		/* there cannot be more entropy than data */
		ent_count_bits = min(ent_count_bits, size<<3);
		return lrng_drng_write_common((const char __user *)p, size,
					      ent_count_bits);
	case RNDZAPENTCNT:
	case RNDCLEARPOOL:
		/* Clear the entropy pool counter. */
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		lrng_pool_set_entropy(0);
		return 0;
	case RNDRESEEDCRNG:
		/*
		 * We leave the capability check here since it is present
		 * in the upstream's RNG implementation. Yet, user space
		 * can trigger a reseed as easy as writing into /dev/random
		 * or /dev/urandom where no privilege is needed.
		 */
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		/* Force a reseed of all DRNGs */
		lrng_drng_force_reseed();
		return 0;
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL(lrng_ioctl);

int lrng_fasync(int fd, struct file *filp, int on)
{
	return fasync_helper(fd, filp, on, &fasync);
}
