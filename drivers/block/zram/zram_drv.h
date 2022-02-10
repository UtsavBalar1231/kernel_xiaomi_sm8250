/*
 * Compressed RAM block device
 *
 * Copyright (C) 2008, 2009, 2010  Nitin Gupta
 *               2012, 2013 Minchan Kim
 *
 * This code is released using a dual license strategy: BSD/GPL
 * You can choose the licence that better fits your requirements.
 *
 * Released under the terms of 3-clause BSD License
 * Released under the terms of GNU General Public License Version 2.0
 *
 */

#ifndef _ZRAM_DRV_H_
#define _ZRAM_DRV_H_

#include <linux/rwsem.h>
#include <linux/zsmalloc.h>
#include <linux/crypto.h>
#include <linux/mm.h>
#include <linux/spinlock.h>

#include "zcomp.h"
#include "zram_dedup.h"

#define SECTORS_PER_PAGE_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define SECTORS_PER_PAGE	(1 << SECTORS_PER_PAGE_SHIFT)
#define ZRAM_LOGICAL_BLOCK_SHIFT 12
#define ZRAM_LOGICAL_BLOCK_SIZE	(1 << ZRAM_LOGICAL_BLOCK_SHIFT)
#define ZRAM_SECTOR_PER_LOGICAL_BLOCK	\
	(1 << (ZRAM_LOGICAL_BLOCK_SHIFT - SECTOR_SHIFT))


/*
 * The lower ZRAM_FLAG_SHIFT bits of table.flags is for
 * object size (excluding header), the higher bits is for
 * zram_pageflags.
 *
 * zram is mainly used for memory efficiency so we want to keep memory
 * footprint small so we can squeeze size and flags into a field.
 * The lower ZRAM_FLAG_SHIFT bits is for object size (excluding header),
 * the higher bits is for zram_pageflags.
 */
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
#define ZRAM_FLAG_SHIFT (PAGE_SHIFT + 1)
#else
#define ZRAM_FLAG_SHIFT 24
#endif

/* Flags for zram pages (table[page_no].flags) */
enum zram_pageflags {
	/* zram slot is locked */
	ZRAM_LOCK = ZRAM_FLAG_SHIFT,
	ZRAM_SAME,	/* Page consists the same element */
	ZRAM_WB,	/* page is stored on backing_device */
	ZRAM_UNDER_WB,	/* page is under writeback */
	ZRAM_HUGE,	/* Incompressible page */
	ZRAM_IDLE,	/* not accessed page since last idle marking */
	ZRAM_EXPIRE,
	ZRAM_READ_BDEV,
	ZRAM_PPR,
	ZRAM_UNDER_PPR,

	__NR_ZRAM_PAGEFLAGS,
};

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
#define ZRAM_WB_IDLE_SHIFT (__NR_ZRAM_PAGEFLAGS)

#define ZRAM_WB_IDLE_BITS_LEN (4U)

#define ZRAM_WB_IDLE_MIN (1U)
#define ZRAM_WB_IDLE_MAX (10U)

#define ZRAM_WB_IDLE_DEFAULT ZRAM_WB_IDLE_MIN
#endif

/*-- Data structures */

struct zram_entry {
	struct rb_node rb_node;
	u32 len;
	u32 checksum;
	unsigned long refcount;
	unsigned long handle;
};

/* Allocated for each disk page */
struct zram_table_entry {
	union {
		struct zram_entry *entry;
		unsigned long element;
	};
	unsigned long flags;
#if defined(CONFIG_ZRAM_MEMORY_TRACKING) || IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	ktime_t ac_time;
#endif
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	struct list_head lru_list;
#endif
};

struct zram_stats {
	atomic64_t compr_data_size;	/* compressed size of pages stored */
	atomic64_t num_reads;	/* failed + successful */
	atomic64_t num_writes;	/* --do-- */
	atomic64_t failed_reads;	/* can happen when memory is too low */
	atomic64_t failed_writes;	/* can happen when memory is too low */
	atomic64_t invalid_io;	/* non-page-aligned I/O requests */
	atomic64_t notify_free;	/* no. of swap slot free notifications */
	atomic64_t same_pages;		/* no. of same element filled pages */
	atomic64_t huge_pages;		/* no. of huge pages */
	atomic64_t pages_stored;	/* no. of pages currently stored */
	atomic_long_t max_used_pages;	/* no. of maximum pages stored */
	atomic64_t writestall;		/* no. of write slow paths */
	atomic64_t miss_free;		/* no. of missed free */
#ifdef  CONFIG_ZRAM_WRITEBACK
	atomic64_t bd_count;		/* no. of pages in backing device */
	atomic64_t bd_reads;		/* no. of reads from backing device */
	atomic64_t bd_writes;		/* no. of writes from backing device */
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	atomic64_t wb_pages_max;	/* no. of max pages in backing device */
#endif
#endif
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	atomic64_t origin_pages_max;	/* no. of maximum origin pages stored */
#endif
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	atomic64_t bd_expire;
	atomic64_t bd_objcnt;
	atomic64_t bd_size;
	atomic64_t bd_max_count;
	atomic64_t bd_max_size;
	atomic64_t bd_ppr_count;
	atomic64_t bd_ppr_reads;
	atomic64_t bd_ppr_writes;
	atomic64_t bd_ppr_objcnt;
	atomic64_t bd_ppr_size;
	atomic64_t bd_ppr_max_count;
	atomic64_t bd_ppr_max_size;
	atomic64_t bd_objreads;
	atomic64_t bd_objwrites;
#endif
	atomic64_t dup_data_size;	/*
					 * compressed size of pages
					 * duplicated
					 */
	atomic64_t meta_data_size;	/* size of zram_entries */
};

#ifdef CONFIG_ZRAM_LRU_WRITEBACK
#define ZRAM_WB_THRESHOLD 32
#define NR_ZWBS 16
#define NR_FALLOC_PAGES 512
#define FALLOC_ALIGN_MASK (~(NR_FALLOC_PAGES - 1))
struct zram_wb_header {
	u32 index;
	u32 size;
};

struct zram_wb_work {
	struct work_struct work;
	struct page *src_page;
	struct page *dst_page;
	struct bio *bio;
	struct zram *zram;
	unsigned long handle;
};

struct zram_wb_entry {
	unsigned long index;
	unsigned int offset;
	unsigned int size;
};

struct zwbs {
	struct zram_wb_entry entry[ZRAM_WB_THRESHOLD];
	struct page *page;
	u32 cnt;
	u32 off;
};

void free_zwbs(struct zwbs **);
int alloc_zwbs(struct zwbs **);
bool zram_is_app_launch(void);
int is_writeback_entry(swp_entry_t);
void swap_add_to_list(struct list_head *, swp_entry_t);
void swap_writeback_list(struct zwbs **, struct list_head *);
#endif

struct zram_hash {
	spinlock_t lock;
	struct rb_root rb_root;
};

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
struct zram_pages_life {
	unsigned int time_nr;
	int *time_list;
	unsigned long *lifes;
	struct rcu_head rcu;
};
#endif

struct zram {
	struct zram_table_entry *table;
	struct zs_pool *mem_pool;
	struct zcomp *comp;
	struct gendisk *disk;
	struct zram_hash *hash;
	size_t hash_size;
	/* Prevent concurrent execution of device init */
	struct rw_semaphore init_lock;
	/*
	 * the number of pages zram can consume for storing compressed data
	 */
	unsigned long limit_pages;

	struct zram_stats stats;
	/*
	 * This is the limit on amount of *uncompressed* worth of data
	 * we can store in a disk.
	 */
	u64 disksize;	/* bytes */
	char compressor[CRYPTO_MAX_ALG_NAME];
	/*
	 * zram is claimed so open request will be failed
	 */
	bool claim; /* Protected by bdev->bd_mutex */
	bool use_dedup;
	struct file *backing_dev;
#ifdef CONFIG_ZRAM_WRITEBACK
	spinlock_t wb_limit_lock;
	bool wb_limit_enable;
	u64 bd_wb_limit;
	struct block_device *bdev;
	unsigned int old_block_size;
	unsigned long *bitmap;
	unsigned long nr_pages;
#endif
#ifdef CONFIG_ZRAM_MEMORY_TRACKING
	struct dentry *debugfs_dir;
#endif
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	struct zram_pages_life __rcu *pages_life;
	ktime_t first_time;
	ktime_t last_time;
	atomic64_t avg_size;
#endif
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	struct task_struct *wbd;
	wait_queue_head_t wbd_wait;
	u8 *wb_table;
	unsigned long *chunk_bitmap;
	bool wbd_running;
	bool io_complete;
	struct list_head list;
	spinlock_t list_lock;
	spinlock_t wb_table_lock;
	spinlock_t bitmap_lock;
	unsigned long *blk_bitmap;
	struct mutex blk_bitmap_lock;
#endif
};

static inline bool zram_dedup_enabled(struct zram *zram)
{
#ifdef CONFIG_ZRAM_DEDUP
	return zram->use_dedup;
#else
	return false;
#endif
}

void zram_entry_free(struct zram *zram, struct zram_entry *entry);
#endif
