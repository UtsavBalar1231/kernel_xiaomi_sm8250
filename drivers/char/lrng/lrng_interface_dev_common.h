/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#ifndef _LRNG_INTERFACE_DEV_COMMON_H
#define _LRNG_INTERFACE_DEV_COMMON_H

#include <linux/poll.h>
#include <linux/wait.h>

/******************* Upstream functions hooked into the LRNG ******************/
enum lrng_external_noise_source {
	lrng_noise_source_hw,
	lrng_noise_source_user
};

#ifdef CONFIG_LRNG_COMMON_DEV_IF
void lrng_writer_wakeup(void);
void lrng_init_wakeup_dev(void);
void lrng_state_exseed_set(enum lrng_external_noise_source source, bool type);
void lrng_state_exseed_allow_all(void);
#else /* CONFIG_LRNG_COMMON_DEV_IF */
static inline void lrng_writer_wakeup(void) { }
static inline void lrng_init_wakeup_dev(void) { }
static inline void
lrng_state_exseed_set(enum lrng_external_noise_source source, bool type) { }
static inline void lrng_state_exseed_allow_all(void) { }
#endif /* CONFIG_LRNG_COMMON_DEV_IF */

/****** Downstream service functions to actual interface implementations ******/

bool lrng_state_exseed_allow(enum lrng_external_noise_source source);
int lrng_fasync(int fd, struct file *filp, int on);
long lrng_ioctl(struct file *f, unsigned int cmd, unsigned long arg);
ssize_t lrng_drng_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *ppos);
ssize_t lrng_drng_write_common(const char __user *buffer, size_t count,
			       u32 entropy_bits);
__poll_t lrng_random_poll(struct file *file, poll_table *wait);
ssize_t lrng_read_common_block(int nonblock, char __user *buf, size_t nbytes);
ssize_t lrng_drng_read_block(struct file *file, char __user *buf, size_t nbytes,
			     loff_t *ppos);
ssize_t lrng_read_common(char __user *buf, size_t nbytes);
bool lrng_need_entropy(void);

extern struct wait_queue_head lrng_write_wait;
extern struct wait_queue_head lrng_init_wait;

#endif /* _LRNG_INTERFACE_DEV_COMMON_H */
