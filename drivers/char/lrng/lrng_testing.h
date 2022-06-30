/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#ifndef _LRNG_TESTING_H
#define _LRNG_TESTING_H

#ifdef CONFIG_LRNG_RAW_HIRES_ENTROPY
bool lrng_raw_hires_entropy_store(u32 value);
#else	/* CONFIG_LRNG_RAW_HIRES_ENTROPY */
static inline bool lrng_raw_hires_entropy_store(u32 value) { return false; }
#endif	/* CONFIG_LRNG_RAW_HIRES_ENTROPY */

#ifdef CONFIG_LRNG_RAW_JIFFIES_ENTROPY
bool lrng_raw_jiffies_entropy_store(u32 value);
#else	/* CONFIG_LRNG_RAW_JIFFIES_ENTROPY */
static inline bool lrng_raw_jiffies_entropy_store(u32 value) { return false; }
#endif	/* CONFIG_LRNG_RAW_JIFFIES_ENTROPY */

#ifdef CONFIG_LRNG_RAW_IRQ_ENTROPY
bool lrng_raw_irq_entropy_store(u32 value);
#else	/* CONFIG_LRNG_RAW_IRQ_ENTROPY */
static inline bool lrng_raw_irq_entropy_store(u32 value) { return false; }
#endif	/* CONFIG_LRNG_RAW_IRQ_ENTROPY */

#ifdef CONFIG_LRNG_RAW_RETIP_ENTROPY
bool lrng_raw_retip_entropy_store(u32 value);
#else	/* CONFIG_LRNG_RAW_RETIP_ENTROPY */
static inline bool lrng_raw_retip_entropy_store(u32 value) { return false; }
#endif	/* CONFIG_LRNG_RAW_RETIP_ENTROPY */

#ifdef CONFIG_LRNG_RAW_REGS_ENTROPY
bool lrng_raw_regs_entropy_store(u32 value);
#else	/* CONFIG_LRNG_RAW_REGS_ENTROPY */
static inline bool lrng_raw_regs_entropy_store(u32 value) { return false; }
#endif	/* CONFIG_LRNG_RAW_REGS_ENTROPY */

#ifdef CONFIG_LRNG_RAW_ARRAY
bool lrng_raw_array_entropy_store(u32 value);
#else	/* CONFIG_LRNG_RAW_ARRAY */
static inline bool lrng_raw_array_entropy_store(u32 value) { return false; }
#endif	/* CONFIG_LRNG_RAW_ARRAY */

#ifdef CONFIG_LRNG_IRQ_PERF
bool lrng_perf_time(u32 start);
#else /* CONFIG_LRNG_IRQ_PERF */
static inline bool lrng_perf_time(u32 start) { return false; }
#endif /*CONFIG_LRNG_IRQ_PERF */

#ifdef CONFIG_LRNG_RAW_SCHED_HIRES_ENTROPY
bool lrng_raw_sched_hires_entropy_store(u32 value);
#else	/* CONFIG_LRNG_RAW_SCHED_HIRES_ENTROPY */
static inline bool
lrng_raw_sched_hires_entropy_store(u32 value) { return false; }
#endif	/* CONFIG_LRNG_RAW_SCHED_HIRES_ENTROPY */

#ifdef CONFIG_LRNG_RAW_SCHED_PID_ENTROPY
bool lrng_raw_sched_pid_entropy_store(u32 value);
#else	/* CONFIG_LRNG_RAW_SCHED_PID_ENTROPY */
static inline bool
lrng_raw_sched_pid_entropy_store(u32 value) { return false; }
#endif	/* CONFIG_LRNG_RAW_SCHED_PID_ENTROPY */

#ifdef CONFIG_LRNG_RAW_SCHED_START_TIME_ENTROPY
bool lrng_raw_sched_starttime_entropy_store(u32 value);
#else	/* CONFIG_LRNG_RAW_SCHED_START_TIME_ENTROPY */
static inline bool
lrng_raw_sched_starttime_entropy_store(u32 value) { return false; }
#endif	/* CONFIG_LRNG_RAW_SCHED_START_TIME_ENTROPY */

#ifdef CONFIG_LRNG_RAW_SCHED_NVCSW_ENTROPY
bool lrng_raw_sched_nvcsw_entropy_store(u32 value);
#else	/* CONFIG_LRNG_RAW_SCHED_NVCSW_ENTROPY */
static inline bool
lrng_raw_sched_nvcsw_entropy_store(u32 value) { return false; }
#endif	/* CONFIG_LRNG_RAW_SCHED_NVCSW_ENTROPY */

#ifdef CONFIG_LRNG_SCHED_PERF
bool lrng_sched_perf_time(u32 start);
#else /* CONFIG_LRNG_SCHED_PERF */
static inline bool lrng_sched_perf_time(u32 start) { return false; }
#endif /*CONFIG_LRNG_SCHED_PERF */

#endif /* _LRNG_TESTING_H */
