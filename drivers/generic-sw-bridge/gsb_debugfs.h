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
#include <linux/ipc_logging.h>

#define IPCLOG_STATE_PAGES 2
#define __FILENAME__ (strrchr(__FILE__, '/') ? \
	strrchr(__FILE__, '/') + 1 : __FILE__)

static void *ipc_gsb_log_ctxt;
static void *ipc_gsb_log_ctxt_low;


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
	if (ipc_gsb_log_ctxt) { \
		ipc_log_string(ipc_gsb_log_ctxt, \
		"%s: %s[%u]:[GSB] ERROR:" s, __FILENAME__ , \
		__func__, __LINE__, ##__VA_ARGS__); \
		} \
} while (0)
#endif

#if (DEBUG_LEVEL < 2)
#define DEBUG_WARN(s, ...)
#else
#define DEBUG_WARN(s, ...) \
do { \
	printk("%s[%u] GSB:", __func__,__LINE__); \
	printk(s, ##__VA_ARGS__); \
	if (ipc_gsb_log_ctxt) { \
		ipc_log_string(ipc_gsb_log_ctxt, \
		"%s: %s[%u]: GSB:" s, __FILENAME__ , \
		__func__, __LINE__, ##__VA_ARGS__); \
		} \
} while (0)
#endif

#if (DEBUG_LEVEL < 3)
#define DEBUG_INFO(s, ...)
#else
#define DEBUG_INFO(s, ...) \
do { \
	printk("%s[%u][GSB]:", __func__,__LINE__); \
	printk(s, ##__VA_ARGS__); \
	if (ipc_gsb_log_ctxt) { \
		ipc_log_string(ipc_gsb_log_ctxt, \
		"%s: %s[%u]: GSB:" s, __FILENAME__ , \
		__func__, __LINE__, ##__VA_ARGS__); \
		} \
} while (0)
#endif

#if (DEBUG_LEVEL < 5)
#define DEBUG_TRACE(s, ...)
#else
#define DEBUG_TRACE(s, ...) \
do { \
	printk("%s[%u]:", __func__,__LINE__); \
	printk(s, ##__VA_ARGS__); \
	if (ipc_gsb_log_ctxt) { \
		ipc_log_string(ipc_gsb_log_ctxt, \
		"%s: %s[%u]: TRACE:" s, __FILENAME__ , \
		__func__, __LINE__, ##__VA_ARGS__); \
		} \
} while (0)
#endif

#if (DEBUG_LEVEL < 4)
#define DUMP_PACKET(s, ...)
#else
#define DUMP_PACKET(s, ...) \
do { \
	printk(s, ##__VA_ARGS__); \
	if (ipc_gsb_log_ctxt) { \
		ipc_log_string(ipc_gsb_log_ctxt, \
		"%s: %s[%u]: DUMP_PACKET:" s, __FILENAME__ , \
		__func__, __LINE__, ##__VA_ARGS__); \
		} \
} while (0)

#endif

#define IPC_ERROR_LOW(s, ...) \
do { \
	if (ipc_gsb_log_ctxt_low) { \
		ipc_log_string(ipc_gsb_log_ctxt_low, \
		"%s: %s[%u]:[GSB] IPC ERROR LOW:" s, __FILENAME__ , \
		__func__, __LINE__, ##__VA_ARGS__); \
		} \
} while (0)

#define IPC_WARN_LOW(s, ...) \
do { \
	if (ipc_gsb_log_ctxt_low) { \
		ipc_log_string(ipc_gsb_log_ctxt_low, \
		"%s: %s[%u]:[GSB] IPC WARN LOW" s, __FILENAME__ , \
		__func__, __LINE__, ##__VA_ARGS__); \
	} \
} while (0)

#define IPC_INFO_LOW(s, ...) \
do { \
	if (ipc_gsb_log_ctxt_low) { \
		ipc_log_string(ipc_gsb_log_ctxt_low, \
		"%s: %s[%u]:[GSB] IPC INFO LOW" s, __FILENAME__ , \
		__func__, __LINE__, ##__VA_ARGS__); \
	} \
} while (0)


#define IPC_TRACE_LOW(s, ...) \
do { \
	if (ipc_gsb_log_ctxt_low) { \
		ipc_log_string(ipc_gsb_log_ctxt_low, \
		"%s: %s[%u]:[GSB] IPC TRACE LOW" s, __FILENAME__ , \
		__func__, __LINE__, ##__VA_ARGS__); \
	} \
} while (0)

#define IPC_DUMP_PACKET_LOW(s, ...) \
do { \
	if (ipc_gsb_log_ctxt_low) { \
		ipc_log_string(ipc_gsb_log_ctxt_low, \
		"%s: %s[%u]:[GSB] IPC DUMP_PACKET LOW" s, __FILENAME__ , \
		__func__, __LINE__, ##__VA_ARGS__); \
	} \
} while (0)


#endif
