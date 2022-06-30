// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * LRNG user space device file interface
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#include <linux/miscdevice.h>
#include <linux/module.h>

#include "lrng_interface_dev_common.h"

static const struct file_operations lrng_fops = {
	.read  = lrng_drng_read_block,
	.write = lrng_drng_write,
	.poll  = lrng_random_poll,
	.unlocked_ioctl = lrng_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
	.fasync = lrng_fasync,
	.llseek = noop_llseek,
};

static struct miscdevice lrng_miscdev = {
	.minor          = MISC_DYNAMIC_MINOR,
	.name           = "lrng",
	.nodename       = "lrng",
	.fops           = &lrng_fops,
	.mode		= 0666
};

static int __init lrng_dev_if_mod_init(void)
{
	return misc_register(&lrng_miscdev);
}
device_initcall(lrng_dev_if_mod_init);
