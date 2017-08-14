/* Copyright (c) 2017, The Linux Foundation. All rights reserved. */
/*
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _GSB_DEBUGFS_H_
#define _GSB_DEBUGFS_H_
#include <linux/debugfs.h>
#include <linux/fs.h>
/*
 * Debug output verbosity level.
 */
#define DEBUG_LEVEL 3

#if (DEBUG_LEVEL < 1)
#define DEBUG_ERROR(s, ...)
#else
#define DEBUG_ERROR(s, ...) \
do { \
	printk("%s[%u]:[GSB] ERROR: ", __func__,__LINE__); \
	printk(s, ##__VA_ARGS__); \
} while (0)
#endif

#if (DEBUG_LEVEL < 2)
#define DEBUG_WARN(s, ...)
#else
#define DEBUG_WARN(s, ...) \
do { \
	printk("%s[%u] GSB:", __func__,__LINE__); \
	printk(s, ##__VA_ARGS__); \
} while (0)
#endif

#if (DEBUG_LEVEL < 3)
#define DEBUG_INFO(s, ...)
#else
#define DEBUG_INFO(s, ...) \
do { \
	printk("%s[%u][GSB]:", __func__,__LINE__); \
	printk(s, ##__VA_ARGS__); \
} while (0)
#endif

#if (DEBUG_LEVEL < 5)
#define DEBUG_TRACE(s, ...)
#else
#define DEBUG_TRACE(s, ...) \
do { \
	printk("%s[%u]:", __func__,__LINE__); \
	printk(s, ##__VA_ARGS__); \
} while (0)
#endif

#if (DEBUG_LEVEL < 4)
#define DUMP_PACKET(s, ...)
#else
#define DUMP_PACKET(s, ...) \
do { \
	printk(s, ##__VA_ARGS__); \
} while (0)
#endif

#endif
