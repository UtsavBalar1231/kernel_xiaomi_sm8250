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

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
#define KMSG_COMPONENT "ExtM"
#else
#define KMSG_COMPONENT "zram"
#endif
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bio.h>
#include <linux/bitops.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/device.h>
#include <linux/genhd.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/backing-dev.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/err.h>
#include <linux/idr.h>
#include <linux/sysfs.h>
#include <linux/debugfs.h>
#include <linux/cpuhotplug.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/jiffies.h>
#include <linux/vmstat.h>
#include <linux/statfs.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/compat.h>
#include <uapi/linux/falloc.h>
#include <uapi/linux/sched/types.h>

#include "zram_drv.h"
#include "../loop.h"

static DEFINE_IDR(zram_index_idr);
/* idr index must be protected */
static DEFINE_MUTEX(zram_index_mutex);

static int zram_major;
static const char *default_compressor = CONFIG_ZRAM_DEF_COMP;

/* Module params (documentation at end) */
static unsigned int num_devices = 1;
/*
 * Pages that compress to sizes equals or greater than this are stored
 * uncompressed in memory.
 */
static size_t huge_class_size;

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
#define TIME_DIFF_MS   200U
/* default_time_list for page life statics and the unit is seconds */
static  int default_time_list[] = {60, 120, 180, 300, 600};
#endif

static void zram_free_page(struct zram *zram, size_t index);
static int zram_bvec_read(struct zram *zram, struct bio_vec *bvec,
				u32 index, int offset, struct bio *bio);


static int zram_slot_trylock(struct zram *zram, u32 index)
{
	return bit_spin_trylock(ZRAM_LOCK, &zram->table[index].flags);
}

static void zram_slot_lock(struct zram *zram, u32 index)
{
	bit_spin_lock(ZRAM_LOCK, &zram->table[index].flags);
}

static void zram_slot_unlock(struct zram *zram, u32 index)
{
	bit_spin_unlock(ZRAM_LOCK, &zram->table[index].flags);
}

static inline bool init_done(struct zram *zram)
{
	return zram->disksize;
}

static inline struct zram *dev_to_zram(struct device *dev)
{
	return (struct zram *)dev_to_disk(dev)->private_data;
}

static struct zram_entry *zram_get_entry(struct zram *zram, u32 index)
{
	return zram->table[index].entry;
}

static void zram_set_entry(struct zram *zram, u32 index,
			struct zram_entry *entry)
{
	zram->table[index].entry = entry;
}

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
static inline unsigned long zram_get_idle_count(struct zram *zram, u32 index)
{
	return zram->table[index].flags >> ZRAM_WB_IDLE_SHIFT;
}

static inline void zram_clear_idle_count(struct zram *zram, u32 index)
{
	zram->table[index].flags &= (BIT(ZRAM_WB_IDLE_SHIFT) - 1);
}

static inline void zram_set_idle_count(struct zram *zram, u32 index,
		unsigned long idle_count)
{
	zram_clear_idle_count(zram, index);

	zram->table[index].flags |= (idle_count << ZRAM_WB_IDLE_SHIFT);
}

static inline void zram_inc_idle_count(struct zram *zram, u32 index)
{
	unsigned long idle_count = zram_get_idle_count(zram, index);

	if (idle_count < ZRAM_WB_IDLE_MAX)
		zram_set_idle_count(zram, index, idle_count + 1);
}
#endif

/* flag operations require table entry bit_spin_lock() being held */
static bool zram_test_flag(struct zram *zram, u32 index,
			enum zram_pageflags flag)
{
	return zram->table[index].flags & BIT(flag);
}

static void zram_set_flag(struct zram *zram, u32 index,
			enum zram_pageflags flag)
{
	zram->table[index].flags |= BIT(flag);
}

static void zram_clear_flag(struct zram *zram, u32 index,
			enum zram_pageflags flag)
{
	zram->table[index].flags &= ~BIT(flag);
}

static inline void zram_set_element(struct zram *zram, u32 index,
			unsigned long element)
{
	zram->table[index].element = element;
}

static unsigned long zram_get_element(struct zram *zram, u32 index)
{
	return zram->table[index].element;
}

static size_t zram_get_obj_size(struct zram *zram, u32 index)
{
	return zram->table[index].flags & (BIT(ZRAM_FLAG_SHIFT) - 1);
}

static void zram_set_obj_size(struct zram *zram,
					u32 index, size_t size)
{
	unsigned long flags = zram->table[index].flags >> ZRAM_FLAG_SHIFT;

	zram->table[index].flags = (flags << ZRAM_FLAG_SHIFT) | size;
}

static inline bool zram_allocated(struct zram *zram, u32 index)
{
	return zram_get_obj_size(zram, index) ||
			zram_test_flag(zram, index, ZRAM_SAME) ||
			zram_test_flag(zram, index, ZRAM_WB);
}

#if PAGE_SIZE != 4096
static inline bool is_partial_io(struct bio_vec *bvec)
{
	return bvec->bv_len != PAGE_SIZE;
}
#else
static inline bool is_partial_io(struct bio_vec *bvec)
{
	return false;
}
#endif

/*
 * Check if request is within bounds and aligned on zram logical blocks.
 */
static inline bool valid_io_request(struct zram *zram,
		sector_t start, unsigned int size)
{
	u64 end, bound;

	/* unaligned request */
	if (unlikely(start & (ZRAM_SECTOR_PER_LOGICAL_BLOCK - 1)))
		return false;
	if (unlikely(size & (ZRAM_LOGICAL_BLOCK_SIZE - 1)))
		return false;

	end = start + (size >> SECTOR_SHIFT);
	bound = zram->disksize >> SECTOR_SHIFT;
	/* out of range range */
	if (unlikely(start >= bound || end > bound || start > end))
		return false;

	/* I/O request is valid */
	return true;
}

static void update_position(u32 *index, int *offset, struct bio_vec *bvec)
{
	*index  += (*offset + bvec->bv_len) / PAGE_SIZE;
	*offset = (*offset + bvec->bv_len) % PAGE_SIZE;
}

static inline void update_used_max(struct zram *zram,
					const unsigned long pages)
{
	unsigned long old_max, cur_max;

	old_max = atomic_long_read(&zram->stats.max_used_pages);

	do {
		cur_max = old_max;
		if (pages > cur_max)
			old_max = atomic_long_cmpxchg(
				&zram->stats.max_used_pages, cur_max, pages);
	} while (old_max != cur_max);
}

static inline void zram_fill_page(void *ptr, unsigned long len,
					unsigned long value)
{
	WARN_ON_ONCE(!IS_ALIGNED(len, sizeof(unsigned long)));
	memset_l(ptr, value, len / sizeof(unsigned long));
}

static bool page_same_filled(void *ptr, unsigned long *element)
{
	unsigned int pos;
	unsigned long *page;
	unsigned long val;

	page = (unsigned long *)ptr;
	val = page[0];

	for (pos = 1; pos < PAGE_SIZE / sizeof(*page); pos++) {
		if (val != page[pos])
			return false;
	}

	*element = val;

	return true;
}

static ssize_t initstate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u32 val;
	struct zram *zram = dev_to_zram(dev);

	down_read(&zram->init_lock);
	val = init_done(zram);
	up_read(&zram->init_lock);

	return scnprintf(buf, PAGE_SIZE, "%u\n", val);
}

static ssize_t disksize_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zram *zram = dev_to_zram(dev);

	return scnprintf(buf, PAGE_SIZE, "%llu\n", zram->disksize);
}

static ssize_t mem_limit_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	u64 limit;
	char *tmp;
	struct zram *zram = dev_to_zram(dev);

	limit = memparse(buf, &tmp);
	if (buf == tmp) /* no chars parsed, invalid input */
		return -EINVAL;

	down_write(&zram->init_lock);
	zram->limit_pages = PAGE_ALIGN(limit) >> PAGE_SHIFT;
	up_write(&zram->init_lock);

	return len;
}

static ssize_t mem_used_max_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int err;
	unsigned long val;
	struct zram *zram = dev_to_zram(dev);

	err = kstrtoul(buf, 10, &val);
	if (err || val != 0)
		return -EINVAL;

	down_read(&zram->init_lock);
	if (init_done(zram)) {
		atomic_long_set(&zram->stats.max_used_pages,
				zs_get_total_pages(zram->mem_pool));
	}
	up_read(&zram->init_lock);

	return len;
}

static ssize_t idle_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	unsigned long nr_pages = zram->disksize >> PAGE_SHIFT;
	int index;
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	int mark_nr = 0;
#endif

	if (!sysfs_streq(buf, "all"))
		return -EINVAL;

	down_read(&zram->init_lock);
	if (!init_done(zram)) {
		up_read(&zram->init_lock);
		return -EINVAL;
	}

	for (index = 0; index < nr_pages; index++) {
		/*
		 * Do not mark ZRAM_UNDER_WB slot as ZRAM_IDLE to close race.
		 * See the comment in writeback_store.
		 */
		zram_slot_lock(zram, index);
		if (zram_allocated(zram, index) &&
				!zram_test_flag(zram, index, ZRAM_UNDER_WB)) {
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
			zram_inc_idle_count(zram, index);
			if (!zram_test_flag(zram, index, ZRAM_IDLE)) {
				zram_set_flag(zram, index, ZRAM_IDLE);
				mark_nr++;
			}
#else
			zram_set_flag(zram, index, ZRAM_IDLE);
#endif
		}
		zram_slot_unlock(zram, index);
	}

	up_read(&zram->init_lock);

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	pr_info("Mark IDLE finished. Mark %d pages\n", mark_nr);
#endif
	return len;
}

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
static ssize_t new_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	unsigned long nr_pages = zram->disksize >> PAGE_SHIFT;
	unsigned int index;

	if (!sysfs_streq(buf, "all"))
		return -EINVAL;

	down_read(&zram->init_lock);

	if (!init_done(zram)) {
		up_read(&zram->init_lock);
		return -EINVAL;
	}

	for (index = 0; index < nr_pages; index++) {
		zram_slot_lock(zram, index);
		zram_clear_flag(zram, index, ZRAM_IDLE);
		zram_clear_idle_count(zram, index);
		zram_slot_unlock(zram, index);
	}

	up_read(&zram->init_lock);

	return len;
}
#endif

#ifdef CONFIG_ZRAM_WRITEBACK
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
static int zram_wbd(void *);
static struct zram *g_zram;

#define F2FS_IOCTL_MAGIC	0xf5
#define F2FS_IOC_SET_PIN_FILE	_IOW(F2FS_IOCTL_MAGIC, 13, __u32)
#define F2FS_SET_PIN_FILE	1
static int zram_pin_backing_file(struct zram *zram)
{
	struct loop_device *lo = zram->bdev->bd_disk->private_data;
	struct file *file = lo->lo_backing_file;
	unsigned int cmd = F2FS_IOC_SET_PIN_FILE;
	int __user *buf;
	int set = F2FS_SET_PIN_FILE;
	int ret;

	buf = compat_alloc_user_space(sizeof(*buf));
	if (!buf) {
		pr_info("%s failed to compat_alloc_user_space\n", __func__);
		return -ENOMEM;
	}
	copy_to_user(buf, &set, sizeof(int));
	ret = file->f_op->unlocked_ioctl(file, cmd, (unsigned long)buf);
	pr_info("%s ioctl to pin file returned %d\n", __func__, ret);

	return ret;
}

static void fallocate_block(struct zram *zram, unsigned long blk_idx)
{
	struct block_device *bdev = zram->bdev;

	if (!bdev)
		return;

	mutex_lock(&zram->blk_bitmap_lock);
	/* check 2MB block bitmap. if unset, fallocate 2MB block at once */
	if (!test_and_set_bit(blk_idx / NR_FALLOC_PAGES, zram->blk_bitmap)) {
		struct loop_device *lo = bdev->bd_disk->private_data;
		struct file *file = lo->lo_backing_file;
		loff_t pos = (blk_idx & FALLOC_ALIGN_MASK) << PAGE_SHIFT;
		loff_t len = NR_FALLOC_PAGES << PAGE_SHIFT;
		int mode = FALLOC_FL_KEEP_SIZE;
		int ret;

		file_start_write(file);
		ret = file->f_op->fallocate(file, mode, pos, len);
		if (ret)
			pr_err("%s pos %lx failed %d\n", __func__, pos, ret);
		file_end_write(file);
	}
	mutex_unlock(&zram->blk_bitmap_lock);
}

static int init_lru_writeback(struct zram *zram)
{
	struct sched_param param = { .sched_priority = 0 };
	int ret = 0;
	int bitmap_sz;

	init_waitqueue_head(&zram->wbd_wait);
	zram->wb_table = kvzalloc(sizeof(u8) * zram->nr_pages, GFP_KERNEL);
	if (!zram->wb_table) {
		ret = -ENOMEM;
		return ret;
	}
	/* bitmap for 2MB block */
	bitmap_sz = (BITS_TO_LONGS(zram->nr_pages) * sizeof(long)) / NR_FALLOC_PAGES;
	zram->blk_bitmap = kvzalloc(bitmap_sz, GFP_KERNEL);
	if (!zram->blk_bitmap) {
		ret = -ENOMEM;
		goto out;
	}
	if (zram_pin_backing_file(zram)) {
		ret = -EINVAL;
		goto out;
	}

	bitmap_sz = BITS_TO_LONGS(zram->nr_pages) * sizeof(long) / NR_ZWBS;
	/* backing dev should be large enough for chunk writeback */
	if (!bitmap_sz)
		return -EINVAL;
	zram->chunk_bitmap = kvzalloc(bitmap_sz, GFP_KERNEL);
	if (!zram->chunk_bitmap) {
		ret = -ENOMEM;
		goto out;
	}

	zram->wbd = kthread_run(zram_wbd, zram, "%s_wbd", zram->disk->disk_name);
	if (IS_ERR(zram->wbd)) {
		ret = PTR_ERR(zram->wbd);
		goto out;
	}

	g_zram = zram;
	zram->wb_limit_enable = true;
	sched_setscheduler(zram->wbd, SCHED_IDLE, &param);

	return ret;
out:
	if (zram->chunk_bitmap) {
		kvfree(zram->chunk_bitmap);
		zram->chunk_bitmap = NULL;
	}
	if (zram->blk_bitmap) {
		kvfree(zram->blk_bitmap);
		zram->blk_bitmap = NULL;
	}
	kvfree(zram->wb_table);
	zram->wb_table = NULL;
	return ret;
}

static void stop_lru_writeback(struct zram *zram)
{
	if (!IS_ERR_OR_NULL(zram->wbd)) {
		g_zram = NULL;
		kthread_stop(zram->wbd);
		zram->wbd = NULL;
	}
}

static void deinit_lru_writeback(struct zram *zram)
{
	unsigned long flags;

	stop_lru_writeback(zram);
	if (zram->chunk_bitmap) {
		kvfree(zram->chunk_bitmap);
		zram->chunk_bitmap = NULL;
	}
	if (zram->blk_bitmap) {
		kvfree(zram->blk_bitmap);
		zram->blk_bitmap = NULL;
	}
	spin_lock_irqsave(&zram->wb_table_lock, flags);
	if (zram->wb_table) {
		kvfree(zram->wb_table);
		zram->wb_table = NULL;
	}
	spin_unlock_irqrestore(&zram->wb_table_lock, flags);
}
#endif

static ssize_t writeback_limit_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	u64 val;
	ssize_t ret = -EINVAL;

	if (kstrtoull(buf, 10, &val))
		return ret;

	down_read(&zram->init_lock);
	spin_lock(&zram->wb_limit_lock);
	zram->wb_limit_enable = val;
	spin_unlock(&zram->wb_limit_lock);
	up_read(&zram->init_lock);
	ret = len;

	return ret;
}

static ssize_t writeback_limit_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	bool val;
	struct zram *zram = dev_to_zram(dev);

	down_read(&zram->init_lock);
	spin_lock(&zram->wb_limit_lock);
	val = zram->wb_limit_enable;
	spin_unlock(&zram->wb_limit_lock);
	up_read(&zram->init_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t writeback_limit_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	u64 val;
	ssize_t ret = -EINVAL;

	if (kstrtoull(buf, 10, &val))
		return ret;

	down_read(&zram->init_lock);
	spin_lock(&zram->wb_limit_lock);
	zram->bd_wb_limit = val;
	spin_unlock(&zram->wb_limit_lock);
	up_read(&zram->init_lock);
	ret = len;

	return ret;
}

static ssize_t writeback_limit_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u64 val;
	struct zram *zram = dev_to_zram(dev);

	down_read(&zram->init_lock);
	spin_lock(&zram->wb_limit_lock);
	val = zram->bd_wb_limit;
	spin_unlock(&zram->wb_limit_lock);
	up_read(&zram->init_lock);

	return scnprintf(buf, PAGE_SIZE, "%llu\n", val);
}

static void reset_bdev(struct zram *zram)
{
	struct block_device *bdev;

	if (!zram->backing_dev)
		return;

	bdev = zram->bdev;
	if (zram->old_block_size)
		set_blocksize(bdev, zram->old_block_size);
	blkdev_put(bdev, FMODE_READ|FMODE_WRITE|FMODE_EXCL);
	/* hope filp_close flush all of IO */
	filp_close(zram->backing_dev, NULL);
	zram->backing_dev = NULL;
	zram->old_block_size = 0;
	zram->bdev = NULL;
	zram->disk->queue->backing_dev_info->capabilities |=
				BDI_CAP_SYNCHRONOUS_IO;
	kvfree(zram->bitmap);
	zram->bitmap = NULL;
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	deinit_lru_writeback(zram);
#endif
}

static ssize_t backing_dev_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct file *file;
	struct zram *zram = dev_to_zram(dev);
	char *p;
	ssize_t ret;

	down_read(&zram->init_lock);
	file = zram->backing_dev;
	if (!file) {
		memcpy(buf, "none\n", 5);
		up_read(&zram->init_lock);
		return 5;
	}

	p = file_path(file, buf, PAGE_SIZE - 1);
	if (IS_ERR(p)) {
		ret = PTR_ERR(p);
		goto out;
	}

	ret = strlen(p);
	memmove(buf, p, ret);
	buf[ret++] = '\n';
out:
	up_read(&zram->init_lock);
	return ret;
}

static ssize_t backing_dev_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	char *file_name;
	size_t sz;
	struct file *backing_dev = NULL;
	struct inode *inode;
	struct address_space *mapping;
	unsigned int bitmap_sz, old_block_size = 0;
	unsigned long nr_pages, *bitmap = NULL;
	struct block_device *bdev = NULL;
	int err;
	struct zram *zram = dev_to_zram(dev);

	file_name = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!file_name)
		return -ENOMEM;

	down_write(&zram->init_lock);
	if (init_done(zram)) {
		pr_info("Can't setup backing device for initialized device\n");
		err = -EBUSY;
		goto out;
	}

	strlcpy(file_name, buf, PATH_MAX);
	/* ignore trailing newline */
	sz = strlen(file_name);
	if (sz > 0 && file_name[sz - 1] == '\n')
		file_name[sz - 1] = 0x00;

	backing_dev = filp_open(file_name, O_RDWR|O_LARGEFILE, 0);
	if (IS_ERR(backing_dev)) {
		err = PTR_ERR(backing_dev);
		backing_dev = NULL;
		goto out;
	}

	mapping = backing_dev->f_mapping;
	inode = mapping->host;

	/* Support only block device in this moment */
	if (!S_ISBLK(inode->i_mode)) {
		err = -ENOTBLK;
		goto out;
	}

	bdev = bdgrab(I_BDEV(inode));
	err = blkdev_get(bdev, FMODE_READ | FMODE_WRITE | FMODE_EXCL, zram);
	if (err < 0) {
		bdev = NULL;
		goto out;
	}

	nr_pages = i_size_read(inode) >> PAGE_SHIFT;
	bitmap_sz = BITS_TO_LONGS(nr_pages) * sizeof(long);
	bitmap = kvzalloc(bitmap_sz, GFP_KERNEL);
	if (!bitmap) {
		err = -ENOMEM;
		goto out;
	}

	old_block_size = block_size(bdev);
	err = set_blocksize(bdev, PAGE_SIZE);
	if (err)
		goto out;

	reset_bdev(zram);

	zram->old_block_size = old_block_size;
	zram->bdev = bdev;
	zram->backing_dev = backing_dev;
	zram->bitmap = bitmap;
	zram->nr_pages = nr_pages;
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	err = init_lru_writeback(zram);
	if (err)
		goto init_lru_writeback_fail;
#endif
	/*
	 * With writeback feature, zram does asynchronous IO so it's no longer
	 * synchronous device so let's remove synchronous io flag. Othewise,
	 * upper layer(e.g., swap) could wait IO completion rather than
	 * (submit and return), which will cause system sluggish.
	 * Furthermore, when the IO function returns(e.g., swap_readpage),
	 * upper layer expects IO was done so it could deallocate the page
	 * freely but in fact, IO is going on so finally could cause
	 * use-after-free when the IO is really done.
	 */
	zram->disk->queue->backing_dev_info->capabilities &=
			~BDI_CAP_SYNCHRONOUS_IO;
	up_write(&zram->init_lock);

	pr_info("setup backing device %s\n", file_name);
	kfree(file_name);

	return len;
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
init_lru_writeback_fail:
	zram->old_block_size = 0;
	zram->bdev = NULL;
	zram->backing_dev = NULL;
	zram->bitmap = NULL;
	zram->nr_pages = 0;
#endif
out:
	if (bitmap)
		kvfree(bitmap);

	if (bdev)
		blkdev_put(bdev, FMODE_READ | FMODE_WRITE | FMODE_EXCL);

	if (backing_dev)
		filp_close(backing_dev, NULL);

	up_write(&zram->init_lock);

	kfree(file_name);

	return err;
}

#ifdef CONFIG_ZRAM_LRU_WRITEBACK
static unsigned long chunk_to_blk_idx(unsigned long idx)
{
	return idx * NR_ZWBS;
}
static unsigned long blk_to_chunk_idx(unsigned long idx)
{
	return idx / NR_ZWBS;
}

static unsigned long alloc_chunk_bdev(struct zram *zram)
{
	unsigned long chunk_idx = 1;
	unsigned long max_idx = zram->nr_pages / NR_ZWBS;
	unsigned long blk_idx;
	unsigned long flags;
	int i;
retry:
	/* skip 0 bit to confuse zram.handle = 0 */
	chunk_idx = find_next_zero_bit(zram->chunk_bitmap, max_idx, chunk_idx);
	if (chunk_idx == max_idx)
		return 0;

	spin_lock_irqsave(&zram->bitmap_lock, flags);
	if (test_and_set_bit(chunk_idx, zram->chunk_bitmap)) {
		spin_unlock_irqrestore(&zram->bitmap_lock, flags);
		goto retry;
	}
	blk_idx = chunk_to_blk_idx(chunk_idx);
	for (i = 0; i < NR_ZWBS; i++)
		BUG_ON(test_and_set_bit(blk_idx + i, zram->bitmap));
	spin_unlock_irqrestore(&zram->bitmap_lock, flags);
	atomic64_add(NR_ZWBS, &zram->stats.bd_count);
	count_vm_events(SQZR_COUNT, NR_ZWBS);
	return blk_idx;
}

static unsigned long alloc_block_bdev(struct zram *zram)
{
	unsigned long blk_idx = 1;
	unsigned long flags;
retry:
	/* skip 0 bit to confuse zram.handle = 0 */
	blk_idx = find_next_zero_bit(zram->bitmap, zram->nr_pages, blk_idx);
	if (blk_idx == zram->nr_pages)
		return 0;

	spin_lock_irqsave(&zram->bitmap_lock, flags);
	if (test_and_set_bit(blk_idx, zram->bitmap)) {
		spin_unlock_irqrestore(&zram->bitmap_lock, flags);
		goto retry;
	}
	set_bit(blk_to_chunk_idx(blk_idx), zram->chunk_bitmap);
	spin_unlock_irqrestore(&zram->bitmap_lock, flags);
	atomic64_inc(&zram->stats.bd_count);
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	update_wb_pages_max(zram,
		atomic64_read(&zram->stats.bd_count));
#endif
	count_vm_events(SQZR_COUNT, 1);
	return blk_idx;
}

static unsigned long try_alloc_block_bdev(struct zram *zram, int *nr_pages)
{
	unsigned long blk_idx;

	/* found free chunk, return blk_idx */
	if (*nr_pages == NR_ZWBS) {
		blk_idx = alloc_chunk_bdev(zram);
		if (blk_idx)
			return blk_idx;
	}
	*nr_pages = 1;
	return alloc_block_bdev(zram);
}

static void free_chunk_bdev(struct zram *zram, unsigned long chunk_idx)
{
	unsigned long blk_idx;
	unsigned long flags;
	int i;

	blk_idx = chunk_to_blk_idx(chunk_idx);
	spin_lock_irqsave(&zram->bitmap_lock, flags);
	for (i = 0; i < NR_ZWBS; i++) {
		if (test_bit(blk_idx + i, zram->bitmap)) {
			spin_unlock_irqrestore(&zram->bitmap_lock, flags);
			return;
		}
	}
	clear_bit(chunk_idx, zram->chunk_bitmap);
	spin_unlock_irqrestore(&zram->bitmap_lock, flags);
}

static void free_block_bdev(struct zram *zram, unsigned long blk_idx, bool ppr)
{
	int was_set;
	unsigned long flags;

	spin_lock_irqsave(&zram->wb_table_lock, flags);
	if (!zram->wb_table || zram->wb_table[blk_idx] == 0)
		goto out;
	zram->wb_table[blk_idx]--;
	atomic64_dec(&zram->stats.bd_objcnt);
	count_vm_events(SQZR_OBJCNT, -1);
	if (ppr)
		atomic64_dec(&zram->stats.bd_ppr_objcnt);
	if (zram->wb_table[blk_idx] > 0) {
		spin_unlock_irqrestore(&zram->wb_table_lock, flags);
		return;
	}
out:
	spin_unlock_irqrestore(&zram->wb_table_lock, flags);
	was_set = test_and_clear_bit(blk_idx, zram->bitmap);
	WARN_ON_ONCE(!was_set);
	atomic64_dec(&zram->stats.bd_count);
	count_vm_events(SQZR_COUNT, -1);
	if (ppr)
		atomic64_dec(&zram->stats.bd_ppr_count);
	free_chunk_bdev(zram, blk_to_chunk_idx(blk_idx));
}
#else
static unsigned long alloc_block_bdev(struct zram *zram)
{
	unsigned long blk_idx = 1;
retry:
	/* skip 0 bit to confuse zram.handle = 0 */
	blk_idx = find_next_zero_bit(zram->bitmap, zram->nr_pages, blk_idx);
	if (blk_idx == zram->nr_pages)
		return 0;

	if (test_and_set_bit(blk_idx, zram->bitmap))
		goto retry;

	atomic64_inc(&zram->stats.bd_count);
	return blk_idx;
}

static void free_block_bdev(struct zram *zram, unsigned long blk_idx)
{
	int was_set;

	was_set = test_and_clear_bit(blk_idx, zram->bitmap);
	WARN_ON_ONCE(!was_set);
	atomic64_dec(&zram->stats.bd_count);
}
#endif

static void zram_page_end_io(struct bio *bio)
{
	struct page *page = bio_first_page_all(bio);

	page_endio(page, op_is_write(bio_op(bio)),
			blk_status_to_errno(bio->bi_status));
	bio_put(bio);
}

/*
 * Returns 1 if the submission is successful.
 */
static int read_from_bdev_async(struct zram *zram, struct bio_vec *bvec,
			unsigned long entry, struct bio *parent)
{
	struct bio *bio;

	bio = bio_alloc(GFP_ATOMIC, 1);
	if (!bio)
		return -ENOMEM;

	bio->bi_iter.bi_sector = entry * (PAGE_SIZE >> 9);
	bio_set_dev(bio, zram->bdev);
	if (!bio_add_page(bio, bvec->bv_page, bvec->bv_len, bvec->bv_offset)) {
		bio_put(bio);
		return -EIO;
	}

	if (!parent) {
		bio->bi_opf = REQ_OP_READ;
		bio->bi_end_io = zram_page_end_io;
	} else {
		bio->bi_opf = parent->bi_opf;
		bio_chain(bio, parent);
	}

	submit_bio(bio);
	return 1;
}

#ifdef CONFIG_ZRAM_LRU_WRITEBACK
static int zram_balance_threshold = 25;	/* min swap-used threshold */
static int zram_balance_ratio = 25;	/* nand writeback ratio */
module_param(zram_balance_threshold, int, 0644);
module_param(zram_balance_ratio, int, 0644);

static bool is_bdev_avail(struct zram *zram)
{
	struct loop_device *lo;
	struct inode *inode;
	struct dentry *root;
	struct kstatfs statbuf;
	u64 min_free_blocks;
	int ret;

	if (!zram->bdev->bd_disk)
		return false;

	lo = zram->bdev->bd_disk->private_data;
	if (!lo || !lo->lo_backing_file)
		return false;

	inode = lo->lo_backing_file->f_mapping->host;
	root = inode->i_sb->s_root;
	if (!root->d_sb->s_op->statfs)
		return false;

	ret = root->d_sb->s_op->statfs(root, &statbuf);
	if (ret)
		return false;
	/*
	 * To guarantee "reserved block(133MB on Q-os)" for system,
	 * SQZR is triggered only when devices have enough storage free space
	 * more than SZ_1G or reserved block * 2.
	 */
	min_free_blocks = max_t(u64, SZ_1G / statbuf.f_bsize,
			(statbuf.f_bfree - statbuf.f_bavail) * 2);
	if (statbuf.f_bavail < min_free_blocks)
		return false;

	return true;
}

static bool zram_wb_available(struct zram *zram)
{
	if (!zram->wb_table)
		return false;
	spin_lock(&zram->wb_limit_lock);
	if (zram->wb_limit_enable && !zram->bd_wb_limit) {
		spin_unlock(&zram->wb_limit_lock);
		return false;
	}
	spin_unlock(&zram->wb_limit_lock);

	return true;
}

static u32 entry_to_index(struct zram *zram, struct zram_table_entry *entry)
{
	return (u32)(((unsigned long)entry - (unsigned long)zram->table) /
			sizeof(struct zram_table_entry));
}

#define SKIP 1
#define ABORT 2
static int zram_try_mark_page(struct zram *zram, u32 index)
{
	/* invalid index */
	if (index >= (zram->disksize >> PAGE_SHIFT))
		return ABORT;

	if (!zram_slot_trylock(zram, index))
		return SKIP;

	if (!zram_allocated(zram, index) ||
			zram_test_flag(zram, index, ZRAM_UNDER_PPR)) {
		zram_slot_unlock(zram, index);
		return ABORT;
	} else if (zram_test_flag(zram, index, ZRAM_UNDER_WB)) {
		zram_slot_unlock(zram, index);
		return SKIP;
	}
	zram_set_flag(zram, index, ZRAM_IDLE);
	zram_slot_unlock(zram, index);
	return 0;
}

void free_zwbs(struct zwbs **zwbs)
{
	int i;

	for (i = 0; i < NR_ZWBS; i++) {
		if (!zwbs[i])
			return;
		if (zwbs[i]->page)
			__free_page(zwbs[i]->page);
		kfree(zwbs[i]);
	}
}

int alloc_zwbs(struct zwbs **zwbs)
{
	int i;

	for (i = 0; i < NR_ZWBS; i++) {
		zwbs[i] = kzalloc(sizeof(struct zwbs), GFP_KERNEL);
		if (!zwbs[i])
			goto out;
		zwbs[i]->page = alloc_page(GFP_KERNEL);
		if (!zwbs[i]->page)
			goto out;
	}
	return 0;
out:
	free_zwbs(zwbs);
	return -ENOMEM;
}

#define ZRAM_WBD_INTERVAL 10 * HZ
static bool zram_should_writeback(struct zram *zram,
				unsigned long pages, bool trigger)
{
	unsigned long total = zram->disksize >> PAGE_SHIFT;
	unsigned long stored = atomic64_read(&zram->stats.pages_stored);
	unsigned long writtenback = atomic64_read(&zram->stats.bd_objcnt) -
				    atomic64_read(&zram->stats.bd_ppr_objcnt) -
				    atomic64_read(&zram->stats.bd_expire);
	unsigned long min_stored = total * zram_balance_threshold / 100;
	int writtenback_ratio = stored ? (writtenback * 100) / stored : 0;
	int min_writtenback_ratio = zram_balance_ratio;
	int margin = max_t(int, 1, zram_balance_ratio / 10);
	int max_pages = CONFIG_ZRAM_LRU_WRITEBACK_LIMIT;
	static unsigned long time_stamp;
	bool ret = true;

	/* stop thread when writtenback enough */
	if (pages > max_pages)
		return false;

	/* do not trigger again before time interval */
	if (trigger && time_is_after_jiffies(time_stamp))
		return false;

	if (trigger)
		min_writtenback_ratio -= margin;
	else
		min_writtenback_ratio += margin;

	if (min_stored > stored || min_writtenback_ratio < writtenback_ratio)
		ret = false;

	if (trigger && ret == true)
		time_stamp = jiffies + ZRAM_WBD_INTERVAL;

	return ret;
}

static void try_wakeup_zram_wbd(struct zram *zram)
{
	unsigned long bd_count;

	if (zram->backing_dev && !zram->wbd_running &&
			zram_wb_available(zram) &&
			zram_should_writeback(zram, 0, true) &&
			is_bdev_avail(zram)) {
		bd_count = atomic64_read(&zram->stats.bd_count);
		/* wakeup zram_wbd with enough free blocks */
		if (zram->nr_pages - bd_count < NR_ZWBS)
			return;

		zram->wbd_running = true;
		wake_up(&zram->wbd_wait);
	}
}

static void mark_end_of_page(struct zwbs *zwbs)
{
	struct zram_wb_header *zhdr;
	struct page *page = zwbs->page;
	int offset = zwbs->off;
	void *mem;

	if (offset + sizeof(struct zram_wb_header) < PAGE_SIZE) {
		mem = kmap_atomic(page);
		zhdr = (struct zram_wb_header *)(mem + offset);
		zhdr->index = UINT_MAX;
		zhdr->size = 0;
		kunmap_atomic(mem);
	}
}

static int zram_writeback_fill_page(struct zram *zram, u32 index,
				struct zwbs *zwbs)
{
	struct zram_wb_header *zhdr;
	struct page *page = zwbs->page;
	int offset = zwbs->off;
	unsigned long handle;
	void *src, *dst;
	int ret, size;
	int header_sz = 0;

	zram_slot_lock(zram, index);
	if (!zram_allocated(zram, index) ||
			!zram_test_flag(zram, index, ZRAM_IDLE) ||
			zram_test_flag(zram, index, ZRAM_WB) ||
			zram_test_flag(zram, index, ZRAM_SAME) ||
			zram_test_flag(zram, index, ZRAM_UNDER_WB)) {
		zram_slot_unlock(zram, index);
		return 0;
	}
	size = zram_get_obj_size(zram, index);
	if (size != PAGE_SIZE)
		header_sz = sizeof(struct zram_wb_header);

	if (offset + header_sz + size > PAGE_SIZE) {
		zram_slot_unlock(zram, index);
		return -ENOSPC;
	}
	/*
	 * Clearing ZRAM_UNDER_WB is duty of caller.
	 * IOW, zram_free_page never clear it.
	 */
	zram_set_flag(zram, index, ZRAM_UNDER_WB);
	/* Need for hugepage writeback racing */
	zram_set_flag(zram, index, ZRAM_IDLE);

	handle = zram_get_element(zram, index);
	if (!handle) {
		zram_clear_flag(zram, index, ZRAM_UNDER_WB);
		zram_clear_flag(zram, index, ZRAM_IDLE);
		zram_slot_unlock(zram, index);
		return -ENOENT;
	}
	src = zs_map_object(zram->mem_pool, handle, ZS_MM_RO);
	dst = kmap_atomic(page);
	if (size != PAGE_SIZE) {
		zhdr = (struct zram_wb_header *)(dst + offset);
		zhdr->index = index;
		zhdr->size = size;
		dst = (u8 *)(zhdr + 1);
	}
	memcpy(dst, src, size);
	ret = size;
	kunmap_atomic(dst);
	zs_unmap_object(zram->mem_pool, handle);
	zram_slot_unlock(zram, index);

	return ret;
}

static void zram_writeback_clear_flag(struct zram *zram, struct zwbs *zwbs)
{
	struct zram_wb_entry *entry = zwbs->entry;
	unsigned int count = zwbs->cnt;
	unsigned long index;
	int i;

	for (i = 0; i < count; i++) {
		index = entry[i].index;
		zram_slot_lock(zram, index);
		zram_clear_flag(zram, index, ZRAM_UNDER_WB);
		zram_clear_flag(zram, index, ZRAM_IDLE);
		zram_clear_flag(zram, index, ZRAM_UNDER_PPR);
		zram_slot_unlock(zram, index);
	}
}

static void zram_update_max_stats(struct zram *zram)
{
	unsigned long bd_count, bd_size, bd_ppr_count, bd_ppr_size;

	bd_count = atomic64_read(&zram->stats.bd_count);
	if (bd_count <= atomic64_read(&zram->stats.bd_max_count))
		return;

	bd_size = atomic64_read(&zram->stats.bd_size);
	bd_ppr_count = atomic64_read(&zram->stats.bd_ppr_count);
	bd_ppr_size = atomic64_read(&zram->stats.bd_ppr_size);
	atomic64_set(&zram->stats.bd_max_count, bd_count);
	atomic64_set(&zram->stats.bd_max_size, bd_size);
	atomic64_set(&zram->stats.bd_ppr_max_count, bd_ppr_count);
	atomic64_set(&zram->stats.bd_ppr_max_size, bd_ppr_size);
}

static void zram_reset_stats(struct zram *zram)
{
	atomic64_set(&zram->stats.bd_max_count, 0);
	atomic64_set(&zram->stats.bd_max_size, 0);
	atomic64_set(&zram->stats.bd_ppr_max_count, 0);
	atomic64_set(&zram->stats.bd_ppr_max_size, 0);
}

static void zram_writeback_done(struct zram *zram,
		struct zwbs *zwbs, unsigned long blk_idx, bool ppr)
{
	unsigned long index;
	unsigned int offset;
	unsigned int size;
	unsigned int count = zwbs->cnt;
	struct zram_wb_entry *entry = zwbs->entry;
	int i;
	unsigned long flags;

	spin_lock_irqsave(&zram->wb_table_lock, flags);
	if (!zram->wb_table) {
		spin_unlock_irqrestore(&zram->wb_table_lock, flags);
		return;
	}
	zram->wb_table[blk_idx] = count;
	spin_unlock_irqrestore(&zram->wb_table_lock, flags);
	atomic64_add(count, &zram->stats.bd_objwrites);
	atomic64_add(count, &zram->stats.bd_objcnt);
	count_vm_events(SQZR_OBJCNT, count);
	if (ppr)
		atomic64_add(count, &zram->stats.bd_ppr_objcnt);

	for (i = 0; i < count; i++) {
		index = entry[i].index;
		offset = entry[i].offset;
		size = entry[i].size;
		/*
		 * We released zram_slot_lock so need to check if the slot was
		 * changed. If there is freeing for the slot, we can catch it
		 * easily by zram_allocated.
		 * A subtle case is the slot is freed/reallocated/marked as
		 * ZRAM_IDLE again. To close the race, idle_store doesn't
		 * mark ZRAM_IDLE once it found the slot was ZRAM_UNDER_WB.
		 * Thus, we could close the race by checking ZRAM_IDLE bit.
		 */
		zram_slot_lock(zram, index);
		if (!zram_allocated(zram, index) ||
				!zram_test_flag(zram, index, ZRAM_IDLE)) {
			zram_clear_flag(zram, index, ZRAM_UNDER_WB);
			zram_clear_flag(zram, index, ZRAM_IDLE);
			zram_clear_flag(zram, index, ZRAM_UNDER_PPR);
			free_block_bdev(zram, blk_idx, ppr);
			zram_slot_unlock(zram, index);
			continue;
		}

		zram_free_page(zram, index);
		zram_clear_flag(zram, index, ZRAM_UNDER_WB);
		zram_set_flag(zram, index, ZRAM_WB);
		atomic64_add(size, &zram->stats.bd_size);
		if (ppr) {
			zram_set_flag(zram, index, ZRAM_PPR);
			atomic64_add(size, &zram->stats.bd_ppr_size);
		}
		/* record element as "blk_idx|offset|size" */
		if (size == PAGE_SIZE)
			size = 0;
		zram_set_element(zram, index,
				(blk_idx << (PAGE_SHIFT * 2)) | (offset << PAGE_SHIFT) | size);
		zram_slot_unlock(zram, index);
		atomic64_inc(&zram->stats.pages_stored);
	}
}

static void zram_writeback_end_io(struct bio *bio)
{
	if (g_zram && !g_zram->io_complete) {
		g_zram->io_complete = true;
		wake_up(&g_zram->wbd_wait);
	}
}

static int zram_writeback_page(struct zram *zram, struct zwbs **zwbs,
			       int nr_to_write, bool sync, bool ppr)
{
	struct bio bio;
	struct bio_vec *bio_vec;
	unsigned long blk_idx;
	int ret = 0;
	int i, idx = 0;
	int nr_pages = nr_to_write;
retry:
	blk_idx = try_alloc_block_bdev(zram, &nr_pages);
	if (!blk_idx) {
		ret = -ENOSPC;
		goto out;
	}
	/* fallocate 2MB block if not allocated yet */
	fallocate_block(zram, blk_idx);
	if (ppr)
		atomic64_add(nr_pages, &zram->stats.bd_ppr_count);

	bio_vec = kmalloc_array(nr_pages, sizeof(struct bio_vec), GFP_KERNEL);
	if (!bio_vec) {
		ret = -ENOSPC;
		goto out;
	}

	bio_init(&bio, bio_vec, nr_pages);
	bio_set_dev(&bio, zram->bdev);
	bio.bi_iter.bi_sector = blk_idx * (PAGE_SIZE >> 9);
	bio.bi_opf = REQ_OP_WRITE;
	for (i = 0; i < nr_pages; i++)
		bio_add_page(&bio, zwbs[idx + i]->page, PAGE_SIZE, 0);
	if (sync) {
		ret = submit_bio_wait(&bio);
	} else {
		bio.bi_end_io = zram_writeback_end_io;
		zram->io_complete = false;
		submit_bio(&bio);
		wait_event(zram->wbd_wait, zram->io_complete);
		ret = blk_status_to_errno(bio.bi_status);
	}
	kfree(bio_vec);
out:
	if (!ret) {
		for (i = 0; i < nr_pages; i++)
			zram_writeback_done(zram, zwbs[idx + i], blk_idx + i, ppr);

		zram_update_max_stats(zram);
		atomic64_add(nr_pages, &zram->stats.bd_writes);
		count_vm_events(SQZR_WRITE, nr_pages);
		if (ppr)
			atomic64_add(nr_pages, &zram->stats.bd_ppr_writes);
		spin_lock(&zram->wb_limit_lock);
		if (zram->wb_limit_enable) {
			if (zram->bd_wb_limit > nr_pages)
				zram->bd_wb_limit -= nr_pages;
			else
				zram->bd_wb_limit = 0;
		}
		spin_unlock(&zram->wb_limit_lock);

		idx += nr_pages;
		if (idx < nr_to_write)
			goto retry;
	} else {
		if (blk_idx)
			for (i = 0; i < nr_pages; i++)
				free_block_bdev(zram, blk_idx + i, ppr);
		/* free all remaining entries when error */
		for (i = idx; i < nr_to_write; i++)
			zram_writeback_clear_flag(zram, zwbs[i]);
	}
	return ret;
}

static int zram_comp_writeback_index(struct zram *zram, u32 index,
			struct zwbs **zwbs, int *idx, bool sync, bool ppr)
{
	int size, ret = 0;
	int i = *idx, j;
retry:
	size = zram_writeback_fill_page(zram, index, zwbs[i]);
	if (size > 0) {
		struct zram_wb_entry *entry = zwbs[i]->entry;
		entry[zwbs[i]->cnt].index = index;
		entry[zwbs[i]->cnt].offset = zwbs[i]->off;
		entry[zwbs[i]->cnt].size = size;
		zwbs[i]->off += size;
		if (size < PAGE_SIZE)
			zwbs[i]->off += sizeof(struct zram_wb_header);
		zwbs[i]->cnt++;
	}
	/* writeback if page is full/entry is full */
	if (size == -ENOSPC || zwbs[i]->cnt == ZRAM_WB_THRESHOLD) {
		mark_end_of_page(zwbs[i]);
		i = (i + 1) % NR_ZWBS;
		if (i > 0)
			goto retry;
		ret = zram_writeback_page(zram, zwbs, NR_ZWBS, sync, ppr);
		for (j = 0; j < NR_ZWBS; j++) {
			zwbs[j]->cnt = 0;
			zwbs[j]->off = 0;
		}
		if (ret == 0 && size == -ENOSPC)
			goto retry;
	}
	*idx = i;
	return ret;
}

static void zram_comp_writeback(struct zram *zram)
{
	struct zwbs *zwbs[NR_ZWBS];
	unsigned long nr_pages = zram->disksize >> PAGE_SHIFT;
	unsigned long index;
	int idx = 0;

	if (alloc_zwbs(zwbs)) {
		pr_info("%s alloc_zwbs failed", __func__);
		return;
	}

	for (index = 0; index < nr_pages; index++) {
		if (!zram_wb_available(zram))
			break;
		if (zram_comp_writeback_index(zram, index, zwbs, &idx, true, false))
			break;
	}
	free_zwbs(zwbs);
	pr_info("%s done", __func__);
}

static int zram_wbd(void *p)
{
	struct zram *zram = (struct zram *)p;
	struct zram_table_entry *zram_entry, *n;
	struct zwbs *zwbs[NR_ZWBS];
	u32 index;
	int idx = 0;
	int ret;

	set_freezable();

	if (alloc_zwbs(zwbs)) {
		pr_info("%s alloc_zwbs failed", __func__);
		return 0;
	}

	while (!kthread_should_stop()) {
		unsigned long nr_pages = 0;
		wait_event_freezable(zram->wbd_wait,
				zram->wbd_running || kthread_should_stop());
		list_for_each_entry_safe(zram_entry, n, &zram->list, lru_list) {
			if (try_to_freeze() || kthread_should_stop())
				break;
			if (!zram_wb_available(zram))
				break;
			index = entry_to_index(zram, zram_entry);
			ret = zram_try_mark_page(zram, index);
			if (!ret) {
				if (zram_comp_writeback_index(zram, index,
						zwbs, &idx, false, false))
					break;
			} else if (ret == ABORT) {
				n = list_first_entry(&zram->list,
						struct zram_table_entry, lru_list);
			}
			if (!zram_should_writeback(zram, ++nr_pages, false))
				break;
		}
		zram->wbd_running = false;
		pr_info("%s done", __func__);
	}
	free_zwbs(zwbs);

	return 0;
}

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
static inline void update_wb_pages_max(struct zram *zram,
					const s64 wb_pages)
{
	unsigned long old_max, cur_max;

	old_max = atomic_long_read(&zram->stats.wb_pages_max);

	do {
		cur_max = old_max;
		if (wb_pages > cur_max)
			old_max = atomic_long_cmpxchg(
				&zram->stats.wb_pages_max, cur_max, wb_pages);
	} while (old_max != cur_max);
}
#endif

int is_writeback_entry(swp_entry_t entry)
{
	struct zram *zram;
	struct swap_info_struct *sis = swp_swap_info(entry);
	unsigned long index = swp_offset(entry);
	int ret = 0;

	if (!(sis->flags & SWP_BLKDEV))
		return 0;

	zram = sis->bdev->bd_disk->private_data;
	zram_slot_lock(zram, index);
	if (zram_allocated(zram, index) &&
			zram_test_flag(zram, index, ZRAM_WB))
		ret = 1;
	zram_slot_unlock(zram, index);

	return ret;
}

void swap_add_to_list(struct list_head *list, swp_entry_t entry)
{
	struct zram *zram;
	struct swap_info_struct *sis = swp_swap_info(entry);
	unsigned long index = swp_offset(entry);
	unsigned long flags;

	if (!(sis->flags & SWP_BLKDEV))
		return;

	zram = sis->bdev->bd_disk->private_data;
	if (zram != g_zram)
		return;

	if (!is_bdev_avail(zram))
		return;

	if (!zram_wb_available(zram))
		return;

	if (!zram_slot_trylock(zram, index))
		return;

	if (zram_allocated(zram, index) &&
			!zram_test_flag(zram, index, ZRAM_IDLE) &&
			!zram_test_flag(zram, index, ZRAM_WB) &&
			!zram_test_flag(zram, index, ZRAM_SAME) &&
			!zram_test_flag(zram, index, ZRAM_UNDER_WB) &&
			!zram_test_flag(zram, index, ZRAM_UNDER_PPR)) {
		zram_set_flag(zram, index, ZRAM_IDLE);
		zram_set_flag(zram, index, ZRAM_UNDER_PPR);
		spin_lock_irqsave(&zram->list_lock, flags);
		if (!list_empty(&zram->table[index].lru_list))
			list_move(&zram->table[index].lru_list, list);
		spin_unlock_irqrestore(&zram->list_lock, flags);
	}
	zram_slot_unlock(zram, index);
}

void swap_writeback_list(struct zwbs **zwbs, struct list_head *list)
{
	struct zram *zram = g_zram;
	struct zram_table_entry *zram_entry;
	u32 index;
	static int idx = 0;
	unsigned long flags;
	bool skip = false;

	if (list == NULL) {
		if (idx > 0 || zwbs[idx]->cnt > 0) {
			mark_end_of_page(zwbs[idx]);
			if (zwbs[idx]->cnt > 0)
				idx++;
			zram_writeback_page(zram, zwbs, idx, true, true);
		}
		idx = 0;
		return;
	}

	while (!list_empty(list)) {
		zram_entry = list_first_entry(list,
				typeof(struct zram_table_entry), lru_list);
		index = entry_to_index(zram, zram_entry);
		if (!skip) {
			if (!zram_wb_available(zram))
				skip = true;
			else if (zram_comp_writeback_index(zram, index,
					zwbs, &idx, true, true))
				skip = true;
		}
		zram_slot_lock(zram, index);
		zram_clear_flag(zram, index, ZRAM_UNDER_PPR);
		spin_lock_irqsave(&zram->list_lock, flags);
		if (!list_empty(&zram->table[index].lru_list))
			list_del_init(&zram->table[index].lru_list);
		spin_unlock_irqrestore(&zram->list_lock, flags);
		zram_slot_unlock(zram, index);
	}
}
#endif

#define HUGE_WRITEBACK 1
#define IDLE_WRITEBACK 2

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
/* Returns true on success, false on parsing error. */
static bool writeback_parse_input(const char *buf,
			unsigned long *wb_max, unsigned int *wb_idle_min)
{
	char *argbuf, *args, *arg;
	bool ret = false;

	args = argbuf = kstrndup(buf, 32, GFP_KERNEL);
	if (!args)
		return false;

	arg = strsep(&args, " ");
	if (!sysfs_streq(arg, "idle"))
		goto err;

	/* get @wb_max */
	arg = strsep(&args, " ");
	if (arg) {
		if (kstrtoul(arg, 10, wb_max))
			goto err;

		/* get @wb_idle_min */
		arg = strsep(&args, " ");
		if (arg) {
			if (kstrtouint(arg, 10, wb_idle_min))
				goto err;

			if (strsep(&args, " "))
				goto err;

			if (*wb_idle_min > ZRAM_WB_IDLE_MAX)
				*wb_idle_min = ZRAM_WB_IDLE_MAX;
		}
	}

	ret = true;
	pr_info("Parse succeed. wb_max: %lu, wb_idle_min: %u\n", *wb_max, *wb_idle_min);
err:
	kfree(argbuf);
	return ret;
}
#endif

static ssize_t writeback_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	unsigned long nr_pages = zram->disksize >> PAGE_SHIFT;
	unsigned long index;
	struct bio bio;
	struct bio_vec bio_vec;
	struct page *page;
	ssize_t ret;
	int mode;
	unsigned long blk_idx = 0;
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	unsigned long wb_max = ULONG_MAX;
	unsigned long wb_pages_nr = 0;
	unsigned int wb_idle_min = ZRAM_WB_IDLE_DEFAULT;
#endif

#if !IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	if (sysfs_streq(buf, "idle"))
#else
	if (writeback_parse_input(buf, &wb_max, &wb_idle_min))
		mode = IDLE_WRITEBACK;
	else if (sysfs_streq(buf, "idle"))
#endif
		mode = IDLE_WRITEBACK;
	else if (sysfs_streq(buf, "huge"))
		mode = HUGE_WRITEBACK;
	else
		return -EINVAL;

	down_read(&zram->init_lock);
	if (!init_done(zram)) {
		ret = -EINVAL;
		goto release_init_lock;
	}

	if (!zram->backing_dev) {
		ret = -ENODEV;
		goto release_init_lock;
	}

	page = alloc_page(GFP_KERNEL);
	if (!page) {
		ret = -ENOMEM;
		goto release_init_lock;
	}

#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	if (mode == IDLE_WRITEBACK) {
		if (is_bdev_avail(zram))
			zram_comp_writeback(zram);
		ret = len;
		__free_page(page);
		goto release_init_lock;
	}
#endif
	for (index = 0; index < nr_pages; index++) {
		struct bio_vec bvec;

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
		if (wb_pages_nr >= wb_max)
			break;
#endif

		/*
		 * If the writeback thread is running and we receive the
		 * SCREEN_ON event, we will send SIGUSR1 singnal to teriminate
		 * the writeback thread. So if there is a SIGUSR1 signal in
		 * current thread, stop writeback.
		 */
		if (signal_pending(current) &&
		    (sigismember(&current->signal->shared_pending.signal, SIGUSR1) ||
		     sigismember(&current->pending.signal, SIGUSR1))) {
			pr_info("Stop writeback, because SIGUSR1 is received\n");
			ret = -EINTR;
			break;
		}

		bvec.bv_page = page;
		bvec.bv_len = PAGE_SIZE;
		bvec.bv_offset = 0;

		spin_lock(&zram->wb_limit_lock);
		if (zram->wb_limit_enable && !zram->bd_wb_limit) {
			spin_unlock(&zram->wb_limit_lock);
			ret = -EIO;
			break;
		}
		spin_unlock(&zram->wb_limit_lock);

		if (!blk_idx) {
			blk_idx = alloc_block_bdev(zram);
			if (!blk_idx) {
				ret = -ENOSPC;
				break;
			}
		}

		zram_slot_lock(zram, index);
		if (!zram_allocated(zram, index))
			goto next;

		if (zram_test_flag(zram, index, ZRAM_WB) ||
				zram_test_flag(zram, index, ZRAM_SAME) ||
				zram_test_flag(zram, index, ZRAM_UNDER_WB))
			goto next;

		if (mode == IDLE_WRITEBACK &&
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
			  (!zram_test_flag(zram, index, ZRAM_IDLE) ||
			   zram_get_idle_count(zram, index) < wb_idle_min))
#else
			  !zram_test_flag(zram, index, ZRAM_IDLE))
#endif
			goto next;
		if (mode == HUGE_WRITEBACK &&
			  !zram_test_flag(zram, index, ZRAM_HUGE))
			goto next;
		/*
		 * Clearing ZRAM_UNDER_WB is duty of caller.
		 * IOW, zram_free_page never clear it.
		 */
		zram_set_flag(zram, index, ZRAM_UNDER_WB);
		/* Need for hugepage writeback racing */
		zram_set_flag(zram, index, ZRAM_IDLE);
		zram_slot_unlock(zram, index);
		if (zram_bvec_read(zram, &bvec, index, 0, NULL)) {
			zram_slot_lock(zram, index);
			zram_clear_flag(zram, index, ZRAM_UNDER_WB);
			zram_clear_flag(zram, index, ZRAM_IDLE);
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
			zram_clear_idle_count(zram, index);
#endif
			zram_slot_unlock(zram, index);
			continue;
		}

		bio_init(&bio, &bio_vec, 1);
		bio_set_dev(&bio, zram->bdev);
		bio.bi_iter.bi_sector = blk_idx * (PAGE_SIZE >> 9);
		bio.bi_opf = REQ_OP_WRITE | REQ_SYNC;

		bio_add_page(&bio, bvec.bv_page, bvec.bv_len,
				bvec.bv_offset);
		/*
		 * XXX: A single page IO would be inefficient for write
		 * but it would be not bad as starter.
		 */
		ret = submit_bio_wait(&bio);
		if (ret) {
			zram_slot_lock(zram, index);
			zram_clear_flag(zram, index, ZRAM_UNDER_WB);
			zram_clear_flag(zram, index, ZRAM_IDLE);
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
			zram_clear_idle_count(zram, index);
#endif
			zram_slot_unlock(zram, index);
			continue;
		}

		atomic64_inc(&zram->stats.bd_writes);
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
		count_vm_event(SQZR_WRITE);
#endif
		/*
		 * We released zram_slot_lock so need to check if the slot was
		 * changed. If there is freeing for the slot, we can catch it
		 * easily by zram_allocated.
		 * A subtle case is the slot is freed/reallocated/marked as
		 * ZRAM_IDLE again. To close the race, idle_store doesn't
		 * mark ZRAM_IDLE once it found the slot was ZRAM_UNDER_WB.
		 * Thus, we could close the race by checking ZRAM_IDLE bit.
		 */
		zram_slot_lock(zram, index);
		if (!zram_allocated(zram, index) ||
			  !zram_test_flag(zram, index, ZRAM_IDLE)) {
			zram_clear_flag(zram, index, ZRAM_UNDER_WB);
			zram_clear_flag(zram, index, ZRAM_IDLE);
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
			zram_clear_idle_count(zram, index);
#endif
			goto next;
		}

		zram_free_page(zram, index);
		zram_clear_flag(zram, index, ZRAM_UNDER_WB);
		zram_set_flag(zram, index, ZRAM_WB);
		zram_set_element(zram, index, blk_idx << (PAGE_SHIFT * 2));
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
		wb_pages_nr++;
#endif
		blk_idx = 0;
		atomic64_inc(&zram->stats.pages_stored);
		atomic64_inc(&zram->stats.bd_objcnt);
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
		count_vm_event(SQZR_OBJCNT);
#endif
		spin_lock(&zram->wb_limit_lock);
		if (zram->wb_limit_enable && zram->bd_wb_limit > 0)
			zram->bd_wb_limit -=  1UL << (PAGE_SHIFT - 12);
		spin_unlock(&zram->wb_limit_lock);
next:
		zram_slot_unlock(zram, index);
	}

	if (blk_idx)
		free_block_bdev(zram, blk_idx, false);
	ret = len;
	__free_page(page);
release_init_lock:
	up_read(&zram->init_lock);

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	pr_info("Flush finished. Mode %d, flush %lu pages\n", mode, wb_pages_nr);
	return ret ? ret : len;
#else
	return ret;
#endif
}

struct zram_work {
	struct work_struct work;
	struct zram *zram;
	unsigned long entry;
	struct bio *bio;
	struct bio_vec bvec;
};

#if PAGE_SIZE != 4096
static void zram_sync_read(struct work_struct *work)
{
	struct zram_work *zw = container_of(work, struct zram_work, work);
	struct zram *zram = zw->zram;
	unsigned long entry = zw->entry;
	struct bio *bio = zw->bio;

	read_from_bdev_async(zram, &zw->bvec, entry, bio);
}

/*
 * Block layer want one ->make_request_fn to be active at a time
 * so if we use chained IO with parent IO in same context,
 * it's a deadlock. To avoid, it, it uses worker thread context.
 */
static int read_from_bdev_sync(struct zram *zram, struct bio_vec *bvec,
				unsigned long entry, struct bio *bio)
{
	struct zram_work work;

	work.bvec = *bvec;
	work.zram = zram;
	work.entry = entry;
	work.bio = bio;

	INIT_WORK_ONSTACK(&work.work, zram_sync_read);
	queue_work(system_unbound_wq, &work.work);
	flush_work(&work.work);
	destroy_work_on_stack(&work.work);

	return 1;
}
#else
static int read_from_bdev_sync(struct zram *zram, struct bio_vec *bvec,
				unsigned long entry, struct bio *bio)
{
	WARN_ON(1);
	return -EIO;
}
#endif

static int read_from_bdev(struct zram *zram, struct bio_vec *bvec,
			unsigned long entry, struct bio *parent, bool sync)
{
	atomic64_inc(&zram->stats.bd_reads);
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	count_vm_event(SQZR_READ);
#endif
	if (sync)
		return read_from_bdev_sync(zram, bvec, entry, parent);
	else
		return read_from_bdev_async(zram, bvec, entry, parent);
}

#ifdef CONFIG_ZRAM_LRU_WRITEBACK
static void zram_handle_remain(struct zram *zram, struct page *page,
				unsigned int blk_idx)
{
	struct zram_wb_header *zhdr;
	unsigned long alloced_pages;
	unsigned long handle;
	unsigned int offset = 0;
	unsigned int size;
	u32 index;
	u8 *mem, *src, *dst;

	mem = kmap_atomic(page);
	while (offset + sizeof(struct zram_wb_header) < PAGE_SIZE) {
		zhdr = (struct zram_wb_header *)(mem + offset);
		index = zhdr->index;
		size = zhdr->size;

		/* invalid index */
		if (index >= (zram->disksize >> PAGE_SHIFT))
			break;

		if (!zram_slot_trylock(zram, index))
			goto next;

		if (!zram_allocated(zram, index) ||
			!zram_test_flag(zram, index, ZRAM_WB) ||
			zram_test_flag(zram, index, ZRAM_READ_BDEV)) {
			zram_slot_unlock(zram, index);
			goto next;
		}
		handle = zram_get_element(zram, index);
		if ((handle >> (PAGE_SHIFT * 2)) != blk_idx ||
			((handle >> PAGE_SHIFT) & (PAGE_SIZE - 1)) != offset ||
			(handle & (PAGE_SIZE - 1)) != size) {
			zram_slot_unlock(zram, index);
			goto next;
		}
		atomic64_inc(&zram->stats.bd_objreads);

		handle = zs_malloc(zram->mem_pool, size,
				__GFP_KSWAPD_RECLAIM |
				__GFP_NOWARN |
				__GFP_HIGHMEM |
				__GFP_MOVABLE);
		if (!handle) {
			zram_slot_unlock(zram, index);
			break;
		}
		alloced_pages = zs_get_total_pages(zram->mem_pool);
		update_used_max(zram, alloced_pages);

		dst = zs_map_object(zram->mem_pool, handle, ZS_MM_WO);
		src = (u8 *)(zhdr + 1);
		memcpy(dst, src, size);
		zs_unmap_object(zram->mem_pool, handle);

		atomic64_add(size, &zram->stats.compr_data_size);
		zram_free_page(zram, index);
		zram_set_element(zram, index, handle);
		zram_set_obj_size(zram, index, size);
		zram_slot_unlock(zram, index);
		atomic64_inc(&zram->stats.pages_stored);
next:
		offset += (size + sizeof(struct zram_wb_header));
	}
	kunmap_atomic(mem);
	free_block_bdev(zram, blk_idx, false);
	atomic64_inc(&zram->stats.bd_objcnt);
	count_vm_event(SQZR_OBJCNT);
}

static void zram_handle_comp_page(struct work_struct *work)
{
	struct zram_wb_work *zw = container_of(work, struct zram_wb_work, work);
	struct zram_wb_header *zhdr;
	struct zram *zram = zw->zram;
	struct zcomp_strm *zstrm;
	struct page *src_page = zw->src_page;
	struct page *dst_page = zw->dst_page;
	struct bio *bio = zw->bio;
	unsigned long handle;
	unsigned int blk_idx = zw->handle >> (PAGE_SHIFT * 2);
	unsigned int offset = (zw->handle >> PAGE_SHIFT) & (PAGE_SIZE - 1);
	unsigned int size = zw->handle & (PAGE_SIZE - 1);
	u8 *src, *dst;
	int ret;
	unsigned long flags;

	src = kmap_atomic(src_page);
	zhdr = (struct zram_wb_header *)(src + offset);
	handle = zhdr->index;
	BUG_ON(zhdr->size != size);

	dst = kmap_atomic(dst_page);
	zstrm = zcomp_stream_get(zram->comp);
	ret = zcomp_decompress(zstrm,
		src + offset + sizeof(struct zram_wb_header), size, dst);
	zcomp_stream_put(zram->comp);
	if (ret) {
		pr_err("%s Decompression failed! err=%d offset=%u size=%u addr=%p\n",
			__func__, ret, offset, size, src);
		print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 16, 1,
				src, PAGE_SIZE, 1);
		BUG_ON(ret);
	}
	kunmap_atomic(dst);
	kunmap_atomic(src);

	zram_slot_lock(zram, handle);
	zram_clear_flag(zram, handle, ZRAM_READ_BDEV);
	zram_slot_unlock(zram, handle);

	/* increment refcount to prevent freeing block */
	spin_lock_irqsave(&zram->wb_table_lock, flags);
	if (zram->wb_table)
		zram->wb_table[blk_idx]++;
	spin_unlock_irqrestore(&zram->wb_table_lock, flags);

	page_endio(dst_page, op_is_write(bio_op(bio)),
			blk_status_to_errno(bio->bi_status));
	bio_put(bio);

	zram_handle_remain(zram, src_page, blk_idx);
	kfree(zw);
	__free_page(src_page);
}

static void zram_comp_page_end_io(struct bio *bio)
{
	struct page *page = bio->bi_io_vec[0].bv_page;
	struct zram_wb_work *zw = (struct zram_wb_work *)page_private(page);

	INIT_WORK(&zw->work, zram_handle_comp_page);
	schedule_work(&zw->work);
}

static int read_comp_from_bdev(struct zram *zram, struct bio_vec *bvec,
			unsigned long handle, struct bio *parent)
{
	struct zram_wb_work *zw;
	struct bio *bio;
	struct page *page;
	unsigned long blk_idx = handle >> (PAGE_SHIFT * 2);

	atomic64_inc(&zram->stats.bd_reads);
	count_vm_event(SQZR_READ);

	bio = bio_alloc(GFP_ATOMIC, 1);
	if (!bio)
		return -ENOMEM;

	page = alloc_page(GFP_NOIO|__GFP_HIGHMEM);
	if (!page) {
		pr_info("%s failed to alloc page", __func__);
		bio_put(bio);
		return -ENOMEM;
	}
	zw = kzalloc(sizeof(struct zram_wb_work), GFP_ATOMIC);
	if (!zw) {
		__free_page(page);
		bio_put(bio);
		return -ENOMEM;
	}
	zw->src_page = page;
	zw->dst_page = bvec->bv_page;
	zw->zram = zram;
	zw->bio = bio;
	zw->handle = handle;
	set_page_private(page, (unsigned long)zw);

	bio->bi_iter.bi_sector = blk_idx * (PAGE_SIZE >> 9);
	bio_set_dev(bio, zram->bdev);
	if (!bio_add_page(bio, page, PAGE_SIZE, 0)) {
		kfree(zw);
		__free_page(page);
		bio_put(bio);
		return -EIO;
	}

	if (!parent) {
		bio->bi_opf = REQ_OP_READ;
		bio->bi_end_io = zram_comp_page_end_io;
	} else {
		bio->bi_opf = parent->bi_opf;
		bio_chain(bio, parent);
	}

	submit_bio(bio);
	return 1;
}
#endif
#else
static inline void reset_bdev(struct zram *zram) {};
static int read_from_bdev(struct zram *zram, struct bio_vec *bvec,
			unsigned long entry, struct bio *parent, bool sync)
{
	return -EIO;
}

static void free_block_bdev(struct zram *zram, unsigned long blk_idx) {};
#endif

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
static inline void update_origin_pages_max(struct zram *zram,
					const s64 pages)
{
	unsigned long old_max, cur_max;

	old_max = atomic_long_read(&zram->stats.origin_pages_max);

	do {
		cur_max = old_max;
		if (pages > cur_max)
			old_max = atomic_long_cmpxchg(
				&zram->stats.origin_pages_max, cur_max, pages);
	} while (old_max != cur_max);
}

static void average_size(struct zram *zram, const s64 pages_store)
{
	ktime_t cur_time = ktime_get_boottime();
	unsigned long new_avg;
	ktime_t  diff_time;

	if (!zram->first_time) {
		zram->last_time = zram->first_time = cur_time;
		atomic64_set(&zram->avg_size, pages_store);
	} else {
		diff_time = ktime_sub(cur_time, zram->last_time);
		if (ktime_to_ms(diff_time) > TIME_DIFF_MS) {
			new_avg = ((zram->last_time - zram->first_time) * atomic64_read(&zram->avg_size)
				+ diff_time * pages_store) / (cur_time - zram->first_time);
			atomic64_set(&zram->avg_size, new_avg);
			zram->last_time = cur_time;
		}
	}
	return;
}

static void free_pages_life(struct zram_pages_life *pl)
{
	if (!pl || !pl->time_nr)
		return;

	pl->time_nr = 0;
	if (pl->time_list) {
		kfree(pl->time_list);
		pl->time_list = NULL;
	}
	if (pl->time_list) {
		kfree(pl->lifes);
		pl->lifes = NULL;
	}
	if (pl) {
		kfree(pl);
		pl = NULL;
	}
}

static struct zram_pages_life *init_pages_life()
{
	size_t i = 0;
	struct zram_pages_life *pl = NULL;

	pl = kmalloc(sizeof(struct zram_pages_life), GFP_KERNEL);
	if (!pl) {
		return NULL;
	}

	pl->time_nr = ARRAY_SIZE(default_time_list);

	pl->time_list = kmalloc_array(pl->time_nr,
				sizeof(*pl->time_list), GFP_KERNEL);
	pl->lifes = kzalloc((pl->time_nr + 1) *
				sizeof(*pl->lifes), GFP_KERNEL);
	if (!pl->time_list || !pl->lifes) {
		free_pages_life(pl);
		return NULL;
	}

	for (i = 0; i < pl->time_nr; i++) {
		pl->time_list[i] = default_time_list[i];
	}
	return pl;
}

static void zram_record_page_life(struct zram *zram, u32 index)
{
	struct zram_pages_life *pl = NULL;
	ktime_t ac_time, diff;
	int time;
	unsigned int i;

	ac_time = zram->table[index].ac_time;
	if (!ac_time)
		return;

	diff = ktime_get_boottime() - ac_time;
	time = ktime_to_ms(diff) / 1000;

	rcu_read_lock();
	pl =  rcu_dereference(zram->pages_life);
	if (pl) {
		for (i = 0; i < pl->time_nr; i++) {
			if (time <= pl->time_list[i]) {
				pl->lifes[i]++;
				rcu_read_unlock();
				return;
			}
		}
		pl->lifes[i]++;
	}
	rcu_read_unlock();

	return;
}
#endif

#ifdef CONFIG_ZRAM_MEMORY_TRACKING

static struct dentry *zram_debugfs_root;

static void zram_debugfs_create(void)
{
	zram_debugfs_root = debugfs_create_dir("zram", NULL);
}

static void zram_debugfs_destroy(void)
{
	debugfs_remove_recursive(zram_debugfs_root);
}

static void zram_accessed(struct zram *zram, u32 index)
{
	zram_clear_flag(zram, index, ZRAM_IDLE);
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	zram_clear_idle_count(zram, index);
	zram_record_page_life(zram, index);
#endif
	zram->table[index].ac_time = ktime_get_boottime();
}

static ssize_t read_block_state(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	char *kbuf;
	ssize_t index, written = 0;
	struct zram *zram = file->private_data;
	unsigned long nr_pages = zram->disksize >> PAGE_SHIFT;
	struct timespec64 ts;

	kbuf = kvmalloc(count, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	down_read(&zram->init_lock);
	if (!init_done(zram)) {
		up_read(&zram->init_lock);
		kvfree(kbuf);
		return -EINVAL;
	}

	for (index = *ppos; index < nr_pages; index++) {
		int copied;

		zram_slot_lock(zram, index);
		if (!zram_allocated(zram, index))
			goto next;

		ts = ktime_to_timespec64(zram->table[index].ac_time);
		copied = snprintf(kbuf + written, count,
			"%12zd %12lld.%06lu %c%c%c%c\n",
			index, (s64)ts.tv_sec,
			ts.tv_nsec / NSEC_PER_USEC,
			zram_test_flag(zram, index, ZRAM_SAME) ? 's' : '.',
			zram_test_flag(zram, index, ZRAM_WB) ? 'w' : '.',
			zram_test_flag(zram, index, ZRAM_HUGE) ? 'h' : '.',
			zram_test_flag(zram, index, ZRAM_IDLE) ? 'i' : '.');

		if (count <= copied) {
			zram_slot_unlock(zram, index);
			break;
		}
		written += copied;
		count -= copied;
next:
		zram_slot_unlock(zram, index);
		*ppos += 1;
	}

	up_read(&zram->init_lock);
	if (copy_to_user(buf, kbuf, written))
		written = -EFAULT;
	kvfree(kbuf);

	return written;
}

static const struct file_operations proc_zram_block_state_op = {
	.open = simple_open,
	.read = read_block_state,
	.llseek = default_llseek,
};

static void zram_debugfs_register(struct zram *zram)
{
	if (!zram_debugfs_root)
		return;

	zram->debugfs_dir = debugfs_create_dir(zram->disk->disk_name,
						zram_debugfs_root);
	debugfs_create_file("block_state", 0400, zram->debugfs_dir,
				zram, &proc_zram_block_state_op);
}

static void zram_debugfs_unregister(struct zram *zram)
{
	debugfs_remove_recursive(zram->debugfs_dir);
}
#else
static void zram_debugfs_create(void) {};
static void zram_debugfs_destroy(void) {};
static void zram_accessed(struct zram *zram, u32 index)
{
	zram_clear_flag(zram, index, ZRAM_IDLE);
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	zram_clear_idle_count(zram, index);
	zram_record_page_life(zram, index);
	zram->table[index].ac_time = ktime_get_boottime();
#endif
};
static void zram_debugfs_register(struct zram *zram) {};
static void zram_debugfs_unregister(struct zram *zram) {};
#endif

/*
 * We switched to per-cpu streams and this attr is not needed anymore.
 * However, we will keep it around for some time, because:
 * a) we may revert per-cpu streams in the future
 * b) it's visible to user space and we need to follow our 2 years
 *    retirement rule; but we already have a number of 'soon to be
 *    altered' attrs, so max_comp_streams need to wait for the next
 *    layoff cycle.
 */
static ssize_t max_comp_streams_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", num_online_cpus());
}

static ssize_t max_comp_streams_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	return len;
}

static ssize_t comp_algorithm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t sz;
	struct zram *zram = dev_to_zram(dev);

	down_read(&zram->init_lock);
	sz = zcomp_available_show(zram->compressor, buf);
	up_read(&zram->init_lock);

	return sz;
}

static ssize_t comp_algorithm_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	char compressor[ARRAY_SIZE(zram->compressor)];
	size_t sz;

	strlcpy(compressor, buf, sizeof(compressor));
	/* ignore trailing newline */
	sz = strlen(compressor);
	if (sz > 0 && compressor[sz - 1] == '\n')
		compressor[sz - 1] = 0x00;

	if (!zcomp_available_algorithm(compressor))
		return -EINVAL;

	down_write(&zram->init_lock);
	if (init_done(zram)) {
		up_write(&zram->init_lock);
		pr_info("Can't change algorithm for initialized device\n");
		return -EBUSY;
	}

	strcpy(zram->compressor, compressor);
	up_write(&zram->init_lock);
	return len;
}

static ssize_t use_dedup_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	bool val;
	struct zram *zram = dev_to_zram(dev);

	down_read(&zram->init_lock);
	val = zram->use_dedup;
	up_read(&zram->init_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", (int)val);
}

#ifdef CONFIG_ZRAM_DEDUP
static ssize_t use_dedup_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int val;
	struct zram *zram = dev_to_zram(dev);

	if (kstrtoint(buf, 10, &val) || (val != 0 && val != 1))
		return -EINVAL;

	down_write(&zram->init_lock);
	if (init_done(zram)) {
		up_write(&zram->init_lock);
		pr_info("Can't change dedup usage for initialized device\n");
		return -EBUSY;
	}
	zram->use_dedup = val;
	up_write(&zram->init_lock);
	return len;
}
#endif

static ssize_t compact_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);

	down_read(&zram->init_lock);
	if (!init_done(zram)) {
		up_read(&zram->init_lock);
		return -EINVAL;
	}

	zs_compact(zram->mem_pool);
	up_read(&zram->init_lock);

	return len;
}

static ssize_t io_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zram *zram = dev_to_zram(dev);
	ssize_t ret;

	down_read(&zram->init_lock);
	ret = scnprintf(buf, PAGE_SIZE,
			"%8llu %8llu %8llu %8llu\n",
			(u64)atomic64_read(&zram->stats.failed_reads),
			(u64)atomic64_read(&zram->stats.failed_writes),
			(u64)atomic64_read(&zram->stats.invalid_io),
			(u64)atomic64_read(&zram->stats.notify_free));
	up_read(&zram->init_lock);

	return ret;
}

static ssize_t mm_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zram *zram = dev_to_zram(dev);
	struct zs_pool_stats pool_stats;
	u64 orig_size, mem_used = 0;
	long max_used;
	ssize_t ret;

	memset(&pool_stats, 0x00, sizeof(struct zs_pool_stats));

	down_read(&zram->init_lock);
	if (init_done(zram)) {
		mem_used = zs_get_total_pages(zram->mem_pool);
		zs_pool_stats(zram->mem_pool, &pool_stats);
	}

	orig_size = atomic64_read(&zram->stats.pages_stored);
	max_used = atomic_long_read(&zram->stats.max_used_pages);

	ret = scnprintf(buf, PAGE_SIZE,
			"%8llu %8llu %8llu %8lu %8ld %8llu %8lu %8llu %8llu %8llu\n",
			orig_size << PAGE_SHIFT,
			(u64)atomic64_read(&zram->stats.compr_data_size),
			mem_used << PAGE_SHIFT,
			zram->limit_pages << PAGE_SHIFT,
			max_used << PAGE_SHIFT,
			(u64)atomic64_read(&zram->stats.same_pages),
			atomic_long_read(&pool_stats.pages_compacted),
			(u64)atomic64_read(&zram->stats.huge_pages),
			zram_dedup_dup_size(zram),
			zram_dedup_meta_size(zram));
	up_read(&zram->init_lock);

	return ret;
}

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
static ssize_t get_idle_or_new_pages(struct zram *zram,
					char *buf, const bool idle)
{
	unsigned long index, nr_pages = zram->disksize >> PAGE_SHIFT;
	unsigned long pages_nr[ZRAM_WB_IDLE_MAX + 1] = { 0 };
	unsigned int max_idle_count = idle ? ZRAM_WB_IDLE_MAX : 0;
	unsigned int min_idle_count = idle ? 1 : 0;
	unsigned int idle_count, i;
	ssize_t ret = -EINVAL;
	size_t off = 0;

	down_read(&zram->init_lock);

	if (!init_done(zram))
		goto out;

	for (index = 0; index < nr_pages; index++) {
		zram_slot_lock(zram, index);

		if (zram_get_obj_size(zram, index) &&
				!zram_test_flag(zram, index, ZRAM_SAME) &&
				!zram_test_flag(zram, index, ZRAM_WB) &&
				!zram_test_flag(zram, index, ZRAM_UNDER_WB)) {
			idle_count = zram_get_idle_count(zram, index);
			if (idle_count <= max_idle_count)
				pages_nr[idle_count]++;
		}

		zram_slot_unlock(zram, index);
	}

	for (i = min_idle_count; i <= max_idle_count; i++)
		off += scnprintf(buf + off, PAGE_SIZE - off, "%lu ", pages_nr[i]);
	buf[off - 1] = '\n';
	ret = off;

out:
	up_read(&zram->init_lock);
	return ret;
}

static ssize_t idle_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_idle_or_new_pages(dev_to_zram(dev), buf, true);
}

static ssize_t new_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_idle_or_new_pages(dev_to_zram(dev), buf, false);
}
#endif

#ifdef CONFIG_ZRAM_WRITEBACK
#define FOUR_K(x) ((x) * (1 << (PAGE_SHIFT - 12)))
static ssize_t bd_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zram *zram = dev_to_zram(dev);
	ssize_t ret;

	down_read(&zram->init_lock);
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	ret = scnprintf(buf, PAGE_SIZE,
		"%8llu %8llu %8llu %8llu %8llu %8llu %8llu %8llu %8llu "
		"%8llu %8llu %8llu %8llu %8llu %8llu %8llu %8llu\n",
			FOUR_K((u64)atomic64_read(&zram->stats.bd_expire)),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_count)),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_reads)),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_writes)),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_objcnt)),
			(u64)(atomic64_read(&zram->stats.bd_size) >> PAGE_SHIFT),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_max_count)),
			(u64)(atomic64_read(&zram->stats.bd_max_size) >> PAGE_SHIFT),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_ppr_count)),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_ppr_reads)),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_ppr_writes)),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_ppr_objcnt)),
			(u64)(atomic64_read(&zram->stats.bd_ppr_size) >> PAGE_SHIFT),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_ppr_max_count)),
			(u64)(atomic64_read(&zram->stats.bd_ppr_max_size) >> PAGE_SHIFT),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_objreads)),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_objwrites)));
#else
	ret = scnprintf(buf, PAGE_SIZE,
		"%8llu %8llu %8llu\n",
			FOUR_K((u64)atomic64_read(&zram->stats.bd_count)),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_reads)),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_writes)));
#endif
	up_read(&zram->init_lock);

	return ret;
}

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
static ssize_t wb_pages_max_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zram *zram = dev_to_zram(dev);
	ssize_t ret;

	down_read(&zram->init_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%8lld\n",
			atomic64_read(&zram->stats.wb_pages_max));
	up_read(&zram->init_lock);

	return ret;
}
#endif

#ifdef CONFIG_ZRAM_LRU_WRITEBACK
static ssize_t bd_stat_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	zram_reset_stats(zram);
	return len;
}
#endif
#endif

static ssize_t debug_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int version = 1;
	struct zram *zram = dev_to_zram(dev);
	ssize_t ret;

	down_read(&zram->init_lock);
	ret = scnprintf(buf, PAGE_SIZE,
			"version: %d\n%8llu %8llu\n",
			version,
			(u64)atomic64_read(&zram->stats.writestall),
			(u64)atomic64_read(&zram->stats.miss_free));
	up_read(&zram->init_lock);

	return ret;
}

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
static ssize_t origin_pages_max_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zram *zram = dev_to_zram(dev);
	ssize_t ret;

	down_read(&zram->init_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%8lld\n",
			atomic64_read(&zram->stats.origin_pages_max));
	up_read(&zram->init_lock);

	return ret;
}

static size_t print_time_list(struct zram_pages_life *pl, char *buf)
{
	size_t off = 0;
	unsigned int i;

	if (!pl->time_nr)
		goto out;

	for (i = 0; i < pl->time_nr; i++)
		off += scnprintf(buf + off, PAGE_SIZE - off,
				"%d\t", pl->time_list[i]);

	off += scnprintf(buf + off, PAGE_SIZE - off,
			">%d\n",  pl->time_list[i - 1]);

out:
	return off;
}

static size_t print_pages_life(struct zram_pages_life *pl, char *buf)
{
	size_t off = 0;
	unsigned int i;

	if (!pl->time_nr)
		goto out;

	off = print_time_list(pl, buf);

	for (i = 0; i < pl->time_nr + 1; i++)
		off += scnprintf(buf + off, PAGE_SIZE - off,
				"%lu\t", pl->lifes[i]);
	buf[off - 1] = '\n';

out:
	return off;
}

static ssize_t time_list_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zram *zram = dev_to_zram(dev);
	ssize_t ret = -EINVAL;
	struct zram_pages_life *pl = NULL;

	down_read(&zram->init_lock);

	if (!init_done(zram))
		goto out;

	pl = rcu_dereference(zram->pages_life);
	if (pl) {
		ret = print_time_list(pl, buf);
	}
out:
	up_read(&zram->init_lock);
	return ret;
}
static void clean_pages_life(struct rcu_head *rcu)
{
	struct zram_pages_life *pl = container_of(rcu, struct zram_pages_life, rcu);
	free_pages_life(pl);
}

static int renew_pages_life(struct zram *zram, unsigned int *time_list, unsigned int time_nr)
{
	struct zram_pages_life *pl_old = zram->pages_life;
	struct zram_pages_life *pl_new = NULL;

	if (!time_nr || !time_list) {
		return -ENOMEM;
	}

	pl_new =  kmalloc(sizeof(struct zram_pages_life), GFP_KERNEL);
	if (!pl_new) {
		return -ENOMEM;
	}
	pl_new->time_nr  = time_nr;
	pl_new->time_list = kmalloc_array(time_nr,
				sizeof(*pl_new->time_list), GFP_KERNEL);

	pl_new->lifes = kzalloc((time_nr + 1) *
				sizeof(*pl_new->lifes), GFP_KERNEL);

	if (!pl_new->time_list || !pl_new->lifes) {
		free_pages_life(pl_new);
		return -ENOMEM;
	}

	memcpy(pl_new->time_list, time_list, sizeof(*pl_new->time_list)*time_nr);
	rcu_assign_pointer(zram->pages_life, pl_new);
	if (pl_old) {
		call_rcu(&pl_old->rcu, clean_pages_life);
		pl_old = NULL;
	}

	return time_nr;
}

static ssize_t time_list_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	ssize_t ret = -EINVAL;
	int *time_list;
	int time;
	char *arg, *args, *arg_buf;
	unsigned int time_nr, i;

	args = arg_buf = kstrndup(buf, 32, GFP_KERNEL);
	arg = strsep(&args, " ");
	if (!arg || kstrtouint(arg, 10, &time_nr))
		goto free_arg_buf;

	if (!time_nr) {
		ret = 0;
		goto free_arg_buf;
	}

	time_list = kmalloc_array(time_nr,
				sizeof(*time_list), GFP_KERNEL);
	if (!time_list) {
		ret = -ENOMEM;
		goto free_arg_buf;
	}
	for (i = 0; i < time_nr; i++) {
		/* get time */
		arg = strsep(&args, " ");
		if (!arg || kstrtoint(arg, 10, &time))
			goto free_time;

		time_list[i] = time;
	}

	down_read(&zram->init_lock);
	if (!init_done(zram))
		goto out;
	ret = renew_pages_life(zram, time_list, time_nr);
out:
	up_read(&zram->init_lock);
free_time:
	kfree(time_list);
free_arg_buf:
	kfree(arg_buf);
	return ret ? ret : len;
}

static ssize_t pages_life_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zram *zram = dev_to_zram(dev);
	ssize_t ret = -EINVAL;
	struct zram_pages_life *pl = NULL;

	rcu_read_lock();
	pl = rcu_dereference(zram->pages_life);
	if (pl)
		ret = print_pages_life(pl, buf);
	rcu_read_unlock();

	return ret;
}

static ssize_t avg_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zram *zram = dev_to_zram(dev);
	return scnprintf(buf, PAGE_SIZE, "%8llu\n", (u64)atomic_read(&zram->avg_size));
}
#endif

static DEVICE_ATTR_RO(io_stat);
static DEVICE_ATTR_RO(mm_stat);
#ifdef CONFIG_ZRAM_WRITEBACK
static DEVICE_ATTR_RW(bd_stat);
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
static DEVICE_ATTR_RO(wb_pages_max);
#endif
#endif
static DEVICE_ATTR_RO(debug_stat);
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
static DEVICE_ATTR_RO(idle_stat);
static DEVICE_ATTR_RO(new_stat);
static DEVICE_ATTR_RW(time_list);
static DEVICE_ATTR_RO(pages_life);
static DEVICE_ATTR_RO(avg_size);
static DEVICE_ATTR_RO(origin_pages_max);
#endif

static unsigned long zram_entry_handle(struct zram *zram,
		struct zram_entry *entry)
{
	if (zram_dedup_enabled(zram))
		return entry->handle;
	else
		return (unsigned long)entry;
}

static struct zram_entry *zram_entry_alloc(struct zram *zram,
					   unsigned int len, gfp_t flags)
{
	struct zram_entry *entry;
	unsigned long handle;

	handle = zs_malloc(zram->mem_pool, len, flags);
	if (!handle)
		return NULL;

	if (!zram_dedup_enabled(zram))
		return (struct zram_entry *)handle;

	entry = kzalloc(sizeof(*entry),
			flags & ~(__GFP_HIGHMEM|__GFP_MOVABLE|__GFP_CMA));
	if (!entry) {
		zs_free(zram->mem_pool, handle);
		return NULL;
	}

	zram_dedup_init_entry(zram, entry, handle, len);
	atomic64_add(sizeof(*entry), &zram->stats.meta_data_size);

	return entry;
}

void zram_entry_free(struct zram *zram, struct zram_entry *entry)
{
	if (!zram_dedup_put_entry(zram, entry))
		return;

	zs_free(zram->mem_pool, zram_entry_handle(zram, entry));

	if (!zram_dedup_enabled(zram))
		return;

	kfree(entry);

	atomic64_sub(sizeof(*entry), &zram->stats.meta_data_size);
}

static void zram_meta_free(struct zram *zram, u64 disksize)
{
	size_t num_pages = disksize >> PAGE_SHIFT;
	size_t index;

	/* Free all pages that are still in this zram device */
	for (index = 0; index < num_pages; index++)
		zram_free_page(zram, index);

	zs_destroy_pool(zram->mem_pool);
	zram_dedup_fini(zram);
	vfree(zram->table);
}

static bool zram_meta_alloc(struct zram *zram, u64 disksize)
{
	size_t num_pages;
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	int i;
#endif

	num_pages = disksize >> PAGE_SHIFT;
	zram->table = vzalloc(array_size(num_pages, sizeof(*zram->table)));
	if (!zram->table)
		return false;

#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	for (i = 0; i < num_pages; i++)
		INIT_LIST_HEAD(&zram->table[i].lru_list);
#endif
	zram->mem_pool = zs_create_pool(zram->disk->disk_name);
	if (!zram->mem_pool) {
		vfree(zram->table);
		return false;
	}

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	rcu_assign_pointer(zram->pages_life, init_pages_life());
#endif

	if (!huge_class_size)
		huge_class_size = zs_huge_class_size(zram->mem_pool);

	if (zram_dedup_init(zram, num_pages)) {
		vfree(zram->table);
		zs_destroy_pool(zram->mem_pool);
		return false;
	}

	return true;
}

/*
 * To protect concurrent access to the same index entry,
 * caller should hold this table index entry's bit_spinlock to
 * indicate this index entry is accessing.
 */
static void zram_free_page(struct zram *zram, size_t index)
{
	struct zram_entry *entry;
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	unsigned long handle;
	unsigned long flags;
#endif

#ifdef CONFIG_ZRAM_MEMORY_TRACKING
	zram->table[index].ac_time = 0;
#endif
	if (zram_test_flag(zram, index, ZRAM_IDLE)) {
		zram_clear_flag(zram, index, ZRAM_IDLE);
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
		zram_clear_idle_count(zram, index);
#endif
	}

	if (zram_test_flag(zram, index, ZRAM_HUGE)) {
		zram_clear_flag(zram, index, ZRAM_HUGE);
		atomic64_dec(&zram->stats.huge_pages);
	}

	if (zram_test_flag(zram, index, ZRAM_WB)) {
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
		int size;
		bool ppr = zram_test_flag(zram, index, ZRAM_PPR);
		handle = zram_get_element(zram, index);
		size = handle & (PAGE_SIZE - 1);
		if (size == 0)
			size = PAGE_SIZE;
		atomic64_sub(size, &zram->stats.bd_size);
		if (ppr) {
			zram_clear_flag(zram, index, ZRAM_PPR);
			atomic64_sub(size, &zram->stats.bd_ppr_size);
		}
		if (zram_test_flag(zram, index, ZRAM_EXPIRE)) {
			zram_clear_flag(zram, index, ZRAM_EXPIRE);
			atomic64_dec(&zram->stats.bd_expire);
		}
		zram_clear_flag(zram, index, ZRAM_WB);
		free_block_bdev(zram, handle >> (PAGE_SHIFT * 2), ppr);
#else
		zram_clear_flag(zram, index, ZRAM_WB);
		free_block_bdev(zram, zram_get_element(zram, index) >> (PAGE_SHIFT * 2));
#endif
		goto out;
	}

	/*
	 * No memory is allocated for same element filled pages.
	 * Simply clear same page flag.
	 */
	if (zram_test_flag(zram, index, ZRAM_SAME)) {
		zram_clear_flag(zram, index, ZRAM_SAME);
		atomic64_dec(&zram->stats.same_pages);
		goto out;
	}

	entry = zram_get_entry(zram, index);
	if (!entry)
		return;

	zram_entry_free(zram, entry);

	atomic64_sub(zram_get_obj_size(zram, index),
			&zram->stats.compr_data_size);
out:
	atomic64_dec(&zram->stats.pages_stored);
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	average_size(zram, atomic64_read(&zram->stats.pages_stored));
#endif
	zram_set_entry(zram, index, NULL);
	zram_set_obj_size(zram, index, 0);
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	if (zram_test_flag(zram, index, ZRAM_UNDER_PPR))
		zram_clear_flag(zram, index, ZRAM_UNDER_PPR);
	spin_lock_irqsave(&zram->list_lock, flags);
	if (!list_empty(&zram->table[index].lru_list))
		list_del_init(&zram->table[index].lru_list);
	spin_unlock_irqrestore(&zram->list_lock, flags);
#endif
	WARN_ON_ONCE(zram->table[index].flags &
		~(1UL << ZRAM_LOCK | 1UL << ZRAM_UNDER_WB));
}

static int __zram_bvec_read(struct zram *zram, struct page *page, u32 index,
				struct bio *bio, bool partial_io)
{
	int ret;
	struct zram_entry *entry;
	unsigned int size;
	void *src, *dst;
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	unsigned long flags;
#endif

	zram_slot_lock(zram, index);
	if (zram_test_flag(zram, index, ZRAM_WB)) {
		struct bio_vec bvec;

		bvec.bv_page = page;
		bvec.bv_len = PAGE_SIZE;
		bvec.bv_offset = 0;
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
		atomic64_inc(&zram->stats.bd_objreads);
		if (zram_test_flag(zram, index, ZRAM_PPR))
			atomic64_inc(&zram->stats.bd_ppr_reads);
		if (!zram_test_flag(zram, index, ZRAM_EXPIRE)) {
			zram_set_flag(zram, index, ZRAM_EXPIRE);
			atomic64_inc(&zram->stats.bd_expire);
		}
		if ((zram_get_element(zram, index) & (PAGE_SIZE - 1)) != 0) {
			zram_set_flag(zram, index, ZRAM_READ_BDEV);
			zram_slot_unlock(zram, index);
			return read_comp_from_bdev(zram, &bvec,
					zram_get_element(zram, index), bio);
		}
#endif
		zram_slot_unlock(zram, index);
		return read_from_bdev(zram, &bvec,
				zram_get_element(zram, index) >> (PAGE_SHIFT * 2),
				bio, partial_io);
	}

	entry = zram_get_entry(zram, index);
	if (!entry || zram_test_flag(zram, index, ZRAM_SAME)) {
		unsigned long value;
		void *mem;

		value = entry ? zram_get_element(zram, index) : 0;
		mem = kmap_atomic(page);
		zram_fill_page(mem, PAGE_SIZE, value);
		kunmap_atomic(mem);
		zram_slot_unlock(zram, index);
		return 0;
	}

	size = zram_get_obj_size(zram, index);

	src = zs_map_object(zram->mem_pool,
			    zram_entry_handle(zram, entry), ZS_MM_RO);
	if (size == PAGE_SIZE) {
		dst = kmap_atomic(page);
		memcpy(dst, src, PAGE_SIZE);
		kunmap_atomic(dst);
		ret = 0;
	} else {
		struct zcomp_strm *zstrm = zcomp_stream_get(zram->comp);

		dst = kmap_atomic(page);
		ret = zcomp_decompress(zstrm, src, size, dst);

		/* Should NEVER happen. BUG() if it does. */
		if (unlikely(ret)) {
#ifdef CONFIG_PGTABLE_MAPPING
			unsigned long pa_start = 0, pa_end = 0;

			if (is_vmalloc_addr(src)) {
				void *src_last;

				src_last = src + size - 1;
				pa_start = (vmalloc_to_pfn(src) << PAGE_SHIFT);
				pa_start |= (unsigned long)src & ~PAGE_MASK;
				pa_end = vmalloc_to_pfn(src_last) << PAGE_SHIFT;
				pa_end |= (unsigned long)src_last & ~PAGE_MASK;
				pa_end += 1;
			} else {
				pa_start = virt_addr_valid(src) ? virt_to_phys(src) : 0;
				pa_end = pa_start + size;
			}
			pr_err("%s Decompression failed! err=%d, page=%u, len=%u, vaddr=0x%px, paddr=0x%lx--0x%lx\n",
			       zram->compressor, ret, index, size, src, pa_start, pa_end);
#else
			pr_err("%s Decompression failed! err=%d, page=%u, len=%u, vaddr=0x%px\n",
			       zram->compressor, ret, index, size, src);
#endif
			print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 16, 1, src, size, 1);
			BUG();
		}
		kunmap_atomic(dst);
		zcomp_stream_put(zram->comp);
	}
	zs_unmap_object(zram->mem_pool, zram_entry_handle(zram, entry));
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	if (zram_test_flag(zram, index, ZRAM_UNDER_PPR))
		zram_clear_flag(zram, index, ZRAM_UNDER_PPR);
	spin_lock_irqsave(&zram->list_lock, flags);
	if (!list_empty(&zram->table[index].lru_list))
		list_del_init(&zram->table[index].lru_list);
	spin_unlock_irqrestore(&zram->list_lock, flags);
#endif
	zram_slot_unlock(zram, index);

	return ret;
}

static int zram_bvec_read(struct zram *zram, struct bio_vec *bvec,
				u32 index, int offset, struct bio *bio)
{
	int ret;
	struct page *page;

	page = bvec->bv_page;
	if (is_partial_io(bvec)) {
		/* Use a temporary buffer to decompress the page */
		page = alloc_page(GFP_NOIO|__GFP_HIGHMEM);
		if (!page)
			return -ENOMEM;
	}

	ret = __zram_bvec_read(zram, page, index, bio, is_partial_io(bvec));
	if (unlikely(ret))
		goto out;

	if (is_partial_io(bvec)) {
		void *dst = kmap_atomic(bvec->bv_page);
		void *src = kmap_atomic(page);

		memcpy(dst + bvec->bv_offset, src + offset, bvec->bv_len);
		kunmap_atomic(src);
		kunmap_atomic(dst);
	}
out:
	if (is_partial_io(bvec))
		__free_page(page);

	return ret;
}

static int __zram_bvec_write(struct zram *zram, struct bio_vec *bvec,
				u32 index, struct bio *bio)
{
	int ret = 0;
	unsigned long alloced_pages;
	struct zram_entry *entry = NULL;
	unsigned int comp_len = 0;
	void *src, *dst, *mem;
	struct zcomp_strm *zstrm;
	struct page *page = bvec->bv_page;
	u32 checksum;
	unsigned long element = 0;
	enum zram_pageflags flags = 0;
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	unsigned long irq_flags;
#endif

	mem = kmap_atomic(page);
	if (page_same_filled(mem, &element)) {
		kunmap_atomic(mem);
		/* Free memory associated with this sector now. */
		flags = ZRAM_SAME;
		atomic64_inc(&zram->stats.same_pages);
		goto out;
	}
	kunmap_atomic(mem);

	entry = zram_dedup_find(zram, page, &checksum);
	if (entry) {
		comp_len = entry->len;
		goto out;
	}

compress_again:
	zstrm = zcomp_stream_get(zram->comp);
	src = kmap_atomic(page);
	ret = zcomp_compress(zstrm, src, &comp_len);
	kunmap_atomic(src);

	if (unlikely(ret)) {
		zcomp_stream_put(zram->comp);
		pr_err("Compression failed! err=%d\n", ret);
		if (entry)
			zram_entry_free(zram, entry);
		return ret;
	}

	if (comp_len >= huge_class_size)
		comp_len = PAGE_SIZE;
	/*
	 * entry allocation has 2 paths:
	 * a) fast path is executed with preemption disabled (for
	 *  per-cpu streams) and has __GFP_DIRECT_RECLAIM bit clear,
	 *  since we can't sleep;
	 * b) slow path enables preemption and attempts to allocate
	 *  the page with __GFP_DIRECT_RECLAIM bit set. we have to
	 *  put per-cpu compression stream and, thus, to re-do
	 *  the compression once entry is allocated.
	 *
	 * if we have a 'non-null' entry here then we are coming
	 * from the slow path and entry has already been allocated.
	 */
	if (!entry)
		entry = zram_entry_alloc(zram, comp_len,
				__GFP_KSWAPD_RECLAIM |
				__GFP_NOWARN |
				__GFP_HIGHMEM |
				__GFP_MOVABLE |
				__GFP_CMA);
	if (!entry) {
		zcomp_stream_put(zram->comp);
		atomic64_inc(&zram->stats.writestall);
		entry = zram_entry_alloc(zram, comp_len,
				GFP_NOIO | __GFP_HIGHMEM |
				__GFP_MOVABLE | __GFP_CMA);
		if (entry)
			goto compress_again;
		return -ENOMEM;
	}

	alloced_pages = zs_get_total_pages(zram->mem_pool);
	update_used_max(zram, alloced_pages);

	if (zram->limit_pages && alloced_pages > zram->limit_pages) {
		zcomp_stream_put(zram->comp);
		zram_entry_free(zram, entry);
		return -ENOMEM;
	}

	dst = zs_map_object(zram->mem_pool,
			    zram_entry_handle(zram, entry), ZS_MM_WO);

	src = zstrm->buffer;
	if (comp_len == PAGE_SIZE)
		src = kmap_atomic(page);
	memcpy(dst, src, comp_len);
	if (comp_len == PAGE_SIZE)
		kunmap_atomic(src);

	zcomp_stream_put(zram->comp);
	zs_unmap_object(zram->mem_pool, zram_entry_handle(zram, entry));
	atomic64_add(comp_len, &zram->stats.compr_data_size);
	zram_dedup_insert(zram, entry, checksum);
out:
	/*
	 * Free memory associated with this sector
	 * before overwriting unused sectors.
	 */
	zram_slot_lock(zram, index);
	zram_free_page(zram, index);

	if (comp_len == PAGE_SIZE) {
		zram_set_flag(zram, index, ZRAM_HUGE);
		atomic64_inc(&zram->stats.huge_pages);
	}

	if (flags) {
		zram_set_flag(zram, index, flags);
		zram_set_element(zram, index, element);
	}  else {
		zram_set_entry(zram, index, entry);
		zram_set_obj_size(zram, index, comp_len);
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
		spin_lock_irqsave(&zram->list_lock, irq_flags);
		list_add_tail(&zram->table[index].lru_list, &zram->list);
		spin_unlock_irqrestore(&zram->list_lock, irq_flags);
#endif
	}
	zram_slot_unlock(zram, index);

	/* Update stats */
	atomic64_inc(&zram->stats.pages_stored);
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	average_size(zram, atomic64_read(&zram->stats.pages_stored));
	update_origin_pages_max(zram,
		atomic64_read(&zram->stats.pages_stored));
#endif
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	try_wakeup_zram_wbd(zram);
#endif
	return ret;
}

static int zram_bvec_write(struct zram *zram, struct bio_vec *bvec,
				u32 index, int offset, struct bio *bio)
{
	int ret;
	struct page *page = NULL;
	void *src;
	struct bio_vec vec;

	vec = *bvec;
	if (is_partial_io(bvec)) {
		void *dst;
		/*
		 * This is a partial IO. We need to read the full page
		 * before to write the changes.
		 */
		page = alloc_page(GFP_NOIO|__GFP_HIGHMEM);
		if (!page)
			return -ENOMEM;

		ret = __zram_bvec_read(zram, page, index, bio, true);
		if (ret)
			goto out;

		src = kmap_atomic(bvec->bv_page);
		dst = kmap_atomic(page);
		memcpy(dst + offset, src + bvec->bv_offset, bvec->bv_len);
		kunmap_atomic(dst);
		kunmap_atomic(src);

		vec.bv_page = page;
		vec.bv_len = PAGE_SIZE;
		vec.bv_offset = 0;
	}

	ret = __zram_bvec_write(zram, &vec, index, bio);
out:
	if (is_partial_io(bvec))
		__free_page(page);
	return ret;
}

/*
 * zram_bio_discard - handler on discard request
 * @index: physical block index in PAGE_SIZE units
 * @offset: byte offset within physical block
 */
static void zram_bio_discard(struct zram *zram, u32 index,
			     int offset, struct bio *bio)
{
	size_t n = bio->bi_iter.bi_size;

	/*
	 * zram manages data in physical block size units. Because logical block
	 * size isn't identical with physical block size on some arch, we
	 * could get a discard request pointing to a specific offset within a
	 * certain physical block.  Although we can handle this request by
	 * reading that physiclal block and decompressing and partially zeroing
	 * and re-compressing and then re-storing it, this isn't reasonable
	 * because our intent with a discard request is to save memory.  So
	 * skipping this logical block is appropriate here.
	 */
	if (offset) {
		if (n <= (PAGE_SIZE - offset))
			return;

		n -= (PAGE_SIZE - offset);
		index++;
	}

	while (n >= PAGE_SIZE) {
		zram_slot_lock(zram, index);
		zram_free_page(zram, index);
		zram_slot_unlock(zram, index);
		atomic64_inc(&zram->stats.notify_free);
		index++;
		n -= PAGE_SIZE;
	}
}

/*
 * Returns errno if it has some problem. Otherwise return 0 or 1.
 * Returns 0 if IO request was done synchronously
 * Returns 1 if IO request was successfully submitted.
 */
static int zram_bvec_rw(struct zram *zram, struct bio_vec *bvec, u32 index,
			int offset, unsigned int op, struct bio *bio)
{
	unsigned long start_time = jiffies;
	struct request_queue *q = zram->disk->queue;
	int ret;

	generic_start_io_acct(q, op, bvec->bv_len >> SECTOR_SHIFT,
			&zram->disk->part0);

	if (!op_is_write(op)) {
		atomic64_inc(&zram->stats.num_reads);
		ret = zram_bvec_read(zram, bvec, index, offset, bio);
		flush_dcache_page(bvec->bv_page);
	} else {
		atomic64_inc(&zram->stats.num_writes);
		ret = zram_bvec_write(zram, bvec, index, offset, bio);
	}

	generic_end_io_acct(q, op, &zram->disk->part0, start_time);

	zram_slot_lock(zram, index);
	zram_accessed(zram, index);
	zram_slot_unlock(zram, index);

	if (unlikely(ret < 0)) {
		if (!op_is_write(op))
			atomic64_inc(&zram->stats.failed_reads);
		else
			atomic64_inc(&zram->stats.failed_writes);
	}

	return ret;
}

static void __zram_make_request(struct zram *zram, struct bio *bio)
{
	int offset;
	u32 index;
	struct bio_vec bvec;
	struct bvec_iter iter;

	index = bio->bi_iter.bi_sector >> SECTORS_PER_PAGE_SHIFT;
	offset = (bio->bi_iter.bi_sector &
		  (SECTORS_PER_PAGE - 1)) << SECTOR_SHIFT;

	switch (bio_op(bio)) {
	case REQ_OP_DISCARD:
	case REQ_OP_WRITE_ZEROES:
		zram_bio_discard(zram, index, offset, bio);
		bio_endio(bio);
		return;
	default:
		break;
	}

	bio_for_each_segment(bvec, bio, iter) {
		struct bio_vec bv = bvec;
		unsigned int unwritten = bvec.bv_len;

		do {
			bv.bv_len = min_t(unsigned int, PAGE_SIZE - offset,
							unwritten);
			if (zram_bvec_rw(zram, &bv, index, offset,
					 bio_op(bio), bio) < 0)
				goto out;

			bv.bv_offset += bv.bv_len;
			unwritten -= bv.bv_len;

			update_position(&index, &offset, &bv);
		} while (unwritten);
	}

	bio_endio(bio);
	return;

out:
	bio_io_error(bio);
}

/*
 * Handler function for all zram I/O requests.
 */
static blk_qc_t zram_make_request(struct request_queue *queue, struct bio *bio)
{
	struct zram *zram = queue->queuedata;

	if (!valid_io_request(zram, bio->bi_iter.bi_sector,
					bio->bi_iter.bi_size)) {
		atomic64_inc(&zram->stats.invalid_io);
		goto error;
	}

	__zram_make_request(zram, bio);
	return BLK_QC_T_NONE;

error:
	bio_io_error(bio);
	return BLK_QC_T_NONE;
}

static void zram_slot_free_notify(struct block_device *bdev,
				unsigned long index)
{
	struct zram *zram;

	zram = bdev->bd_disk->private_data;

	atomic64_inc(&zram->stats.notify_free);
	if (!zram_slot_trylock(zram, index)) {
		atomic64_inc(&zram->stats.miss_free);
		return;
	}

	zram_free_page(zram, index);
	zram_slot_unlock(zram, index);
}

static int zram_rw_page(struct block_device *bdev, sector_t sector,
		       struct page *page, unsigned int op)
{
	int offset, ret;
	u32 index;
	struct zram *zram;
	struct bio_vec bv;

	if (PageTransHuge(page))
		return -ENOTSUPP;
	zram = bdev->bd_disk->private_data;

	if (!valid_io_request(zram, sector, PAGE_SIZE)) {
		atomic64_inc(&zram->stats.invalid_io);
		ret = -EINVAL;
		goto out;
	}

	index = sector >> SECTORS_PER_PAGE_SHIFT;
	offset = (sector & (SECTORS_PER_PAGE - 1)) << SECTOR_SHIFT;

	bv.bv_page = page;
	bv.bv_len = PAGE_SIZE;
	bv.bv_offset = 0;

	ret = zram_bvec_rw(zram, &bv, index, offset, op, NULL);
out:
	/*
	 * If I/O fails, just return error(ie, non-zero) without
	 * calling page_endio.
	 * It causes resubmit the I/O with bio request by upper functions
	 * of rw_page(e.g., swap_readpage, __swap_writepage) and
	 * bio->bi_end_io does things to handle the error
	 * (e.g., SetPageError, set_page_dirty and extra works).
	 */
	if (unlikely(ret < 0))
		return ret;

	switch (ret) {
	case 0:
		page_endio(page, op_is_write(op), 0);
		break;
	case 1:
		ret = 0;
		break;
	default:
		WARN_ON(1);
	}
	return ret;
}

static void zram_reset_device(struct zram *zram)
{
	struct zcomp *comp;
	u64 disksize;

	down_write(&zram->init_lock);

	zram->limit_pages = 0;

	if (!init_done(zram)) {
		up_write(&zram->init_lock);
		return;
	}

	comp = zram->comp;
	disksize = zram->disksize;
	zram->disksize = 0;

	set_capacity(zram->disk, 0);
	part_stat_set_all(&zram->disk->part0, 0);

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	free_pages_life(zram->pages_life);
#endif

	up_write(&zram->init_lock);
	/* I/O operation under all of CPU are done so let's free */
	zram_meta_free(zram, disksize);
	memset(&zram->stats, 0, sizeof(zram->stats));
	zcomp_destroy(comp);
	reset_bdev(zram);
}

static ssize_t disksize_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	u64 disksize;
	struct zcomp *comp;
	struct zram *zram = dev_to_zram(dev);
	int err;

	disksize = memparse(buf, NULL);
	if (!disksize)
		return -EINVAL;

	down_write(&zram->init_lock);
	if (init_done(zram)) {
		pr_info("Cannot change disksize for initialized device\n");
		err = -EBUSY;
		goto out_unlock;
	}

	disksize = PAGE_ALIGN(disksize);
	if (!zram_meta_alloc(zram, disksize)) {
		err = -ENOMEM;
		goto out_unlock;
	}

	comp = zcomp_create(zram->compressor);
	if (IS_ERR(comp)) {
		pr_err("Cannot initialise %s compressing backend\n",
				zram->compressor);
		err = PTR_ERR(comp);
		goto out_free_meta;
	}

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	zram->first_time = zram->last_time = 0;
#endif
	zram->comp = comp;
	zram->disksize = disksize;
	set_capacity(zram->disk, zram->disksize >> SECTOR_SHIFT);

	revalidate_disk(zram->disk);
	up_write(&zram->init_lock);

	return len;

out_free_meta:
	zram_meta_free(zram, disksize);
out_unlock:
	up_write(&zram->init_lock);
	return err;
}

static ssize_t reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int ret;
	unsigned short do_reset;
	struct zram *zram;
	struct block_device *bdev;

	ret = kstrtou16(buf, 10, &do_reset);
	if (ret)
		return ret;

	if (!do_reset)
		return -EINVAL;

	zram = dev_to_zram(dev);
	bdev = bdget_disk(zram->disk, 0);
	if (!bdev)
		return -ENOMEM;

	mutex_lock(&bdev->bd_mutex);
	/* Do not reset an active device or claimed device */
	if (bdev->bd_openers || zram->claim) {
		mutex_unlock(&bdev->bd_mutex);
		bdput(bdev);
		return -EBUSY;
	}

	/* From now on, anyone can't open /dev/zram[0-9] */
	zram->claim = true;
	mutex_unlock(&bdev->bd_mutex);
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	stop_lru_writeback(zram);
#endif
	/* Make sure all the pending I/O are finished */
	fsync_bdev(bdev);
	zram_reset_device(zram);
	revalidate_disk(zram->disk);
	bdput(bdev);

	mutex_lock(&bdev->bd_mutex);
	zram->claim = false;
	mutex_unlock(&bdev->bd_mutex);

	return len;
}

static int zram_open(struct block_device *bdev, fmode_t mode)
{
	int ret = 0;
	struct zram *zram;

	WARN_ON(!mutex_is_locked(&bdev->bd_mutex));

	zram = bdev->bd_disk->private_data;
	/* zram was claimed to reset so open request fails */
	if (zram->claim)
		ret = -EBUSY;

	return ret;
}

static const struct block_device_operations zram_devops = {
	.open = zram_open,
	.swap_slot_free_notify = zram_slot_free_notify,
	.rw_page = zram_rw_page,
	.owner = THIS_MODULE
};

static DEVICE_ATTR_WO(compact);
static DEVICE_ATTR_RW(disksize);
static DEVICE_ATTR_RO(initstate);
static DEVICE_ATTR_WO(reset);
static DEVICE_ATTR_WO(mem_limit);
static DEVICE_ATTR_WO(mem_used_max);
static DEVICE_ATTR_WO(idle);
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
static DEVICE_ATTR_WO(new);
#endif
static DEVICE_ATTR_RW(max_comp_streams);
static DEVICE_ATTR_RW(comp_algorithm);
#ifdef CONFIG_ZRAM_WRITEBACK
static DEVICE_ATTR_RW(backing_dev);
static DEVICE_ATTR_WO(writeback);
static DEVICE_ATTR_RW(writeback_limit);
static DEVICE_ATTR_RW(writeback_limit_enable);
#endif
#ifdef CONFIG_ZRAM_DEDUP
static DEVICE_ATTR_RW(use_dedup);
#else
static DEVICE_ATTR_RO(use_dedup);
#endif

static struct attribute *zram_disk_attrs[] = {
	&dev_attr_disksize.attr,
	&dev_attr_initstate.attr,
	&dev_attr_reset.attr,
	&dev_attr_compact.attr,
	&dev_attr_mem_limit.attr,
	&dev_attr_mem_used_max.attr,
	&dev_attr_idle.attr,
	&dev_attr_max_comp_streams.attr,
	&dev_attr_comp_algorithm.attr,
#ifdef CONFIG_ZRAM_WRITEBACK
	&dev_attr_backing_dev.attr,
	&dev_attr_writeback.attr,
	&dev_attr_writeback_limit.attr,
	&dev_attr_writeback_limit_enable.attr,
#endif
	&dev_attr_use_dedup.attr,
	&dev_attr_io_stat.attr,
	&dev_attr_mm_stat.attr,
#ifdef CONFIG_ZRAM_WRITEBACK
	&dev_attr_bd_stat.attr,
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	&dev_attr_wb_pages_max.attr,
#endif
#endif
	&dev_attr_debug_stat.attr,
#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	&dev_attr_new.attr,
	&dev_attr_idle_stat.attr,
	&dev_attr_new_stat.attr,
	&dev_attr_time_list.attr,
	&dev_attr_pages_life.attr,
	&dev_attr_avg_size.attr,
	&dev_attr_origin_pages_max.attr,
#endif
	NULL,
};

static const struct attribute_group zram_disk_attr_group = {
	.attrs = zram_disk_attrs,
};

static const struct attribute_group *zram_disk_attr_groups[] = {
	&zram_disk_attr_group,
	NULL,
};

/*
 * Allocate and initialize new zram device. the function returns
 * '>= 0' device_id upon success, and negative value otherwise.
 */
static int zram_add(void)
{
	struct zram *zram;
	struct request_queue *queue;
	int ret, device_id;

	zram = kzalloc(sizeof(struct zram), GFP_KERNEL);
	if (!zram)
		return -ENOMEM;

	ret = idr_alloc(&zram_index_idr, zram, 0, 0, GFP_KERNEL);
	if (ret < 0)
		goto out_free_dev;
	device_id = ret;

	init_rwsem(&zram->init_lock);
#ifdef CONFIG_ZRAM_WRITEBACK
	spin_lock_init(&zram->wb_limit_lock);
#endif
#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	INIT_LIST_HEAD(&zram->list);
	spin_lock_init(&zram->list_lock);
	spin_lock_init(&zram->wb_table_lock);
	spin_lock_init(&zram->bitmap_lock);
	mutex_init(&zram->blk_bitmap_lock);
#endif
	queue = blk_alloc_queue(GFP_KERNEL);
	if (!queue) {
		pr_err("Error allocating disk queue for device %d\n",
			device_id);
		ret = -ENOMEM;
		goto out_free_idr;
	}

	blk_queue_make_request(queue, zram_make_request);

	/* gendisk structure */
	zram->disk = alloc_disk(1);
	if (!zram->disk) {
		pr_err("Error allocating disk structure for device %d\n",
			device_id);
		ret = -ENOMEM;
		goto out_free_queue;
	}

	zram->disk->major = zram_major;
	zram->disk->first_minor = device_id;
	zram->disk->fops = &zram_devops;
	zram->disk->queue = queue;
	zram->disk->queue->queuedata = zram;
	zram->disk->private_data = zram;
	snprintf(zram->disk->disk_name, 16, "zram%d", device_id);

	/* Actual capacity set using syfs (/sys/block/zram<id>/disksize */
	set_capacity(zram->disk, 0);
	/* zram devices sort of resembles non-rotational disks */
	blk_queue_flag_set(QUEUE_FLAG_NONROT, zram->disk->queue);
	blk_queue_flag_clear(QUEUE_FLAG_ADD_RANDOM, zram->disk->queue);

	/*
	 * To ensure that we always get PAGE_SIZE aligned
	 * and n*PAGE_SIZED sized I/O requests.
	 */
	blk_queue_physical_block_size(zram->disk->queue, PAGE_SIZE);
	blk_queue_logical_block_size(zram->disk->queue,
					ZRAM_LOGICAL_BLOCK_SIZE);
	blk_queue_io_min(zram->disk->queue, PAGE_SIZE);
	blk_queue_io_opt(zram->disk->queue, PAGE_SIZE);
	zram->disk->queue->limits.discard_granularity = PAGE_SIZE;
	blk_queue_max_discard_sectors(zram->disk->queue, UINT_MAX);
	blk_queue_flag_set(QUEUE_FLAG_DISCARD, zram->disk->queue);

	/*
	 * zram_bio_discard() will clear all logical blocks if logical block
	 * size is identical with physical block size(PAGE_SIZE). But if it is
	 * different, we will skip discarding some parts of logical blocks in
	 * the part of the request range which isn't aligned to physical block
	 * size.  So we can't ensure that all discarded logical blocks are
	 * zeroed.
	 */
	if (ZRAM_LOGICAL_BLOCK_SIZE == PAGE_SIZE)
		blk_queue_max_write_zeroes_sectors(zram->disk->queue, UINT_MAX);

	zram->disk->queue->backing_dev_info->capabilities |=
			(BDI_CAP_STABLE_WRITES | BDI_CAP_SYNCHRONOUS_IO);
	disk_to_dev(zram->disk)->groups = zram_disk_attr_groups;
	add_disk(zram->disk);

	strlcpy(zram->compressor, default_compressor, sizeof(zram->compressor));

	zram_debugfs_register(zram);
	pr_info("Added device: %s\n", zram->disk->disk_name);
	return device_id;

out_free_queue:
	blk_cleanup_queue(queue);
out_free_idr:
	idr_remove(&zram_index_idr, device_id);
out_free_dev:
	kfree(zram);
	return ret;
}

static int zram_remove(struct zram *zram)
{
	struct block_device *bdev;

	bdev = bdget_disk(zram->disk, 0);
	if (!bdev)
		return -ENOMEM;

	mutex_lock(&bdev->bd_mutex);
	if (bdev->bd_openers || zram->claim) {
		mutex_unlock(&bdev->bd_mutex);
		bdput(bdev);
		return -EBUSY;
	}

	zram->claim = true;
	mutex_unlock(&bdev->bd_mutex);

#ifdef CONFIG_ZRAM_LRU_WRITEBACK
	stop_lru_writeback(zram);
#endif
	zram_debugfs_unregister(zram);
	/* Make sure all the pending I/O are finished */
	fsync_bdev(bdev);
	zram_reset_device(zram);
	bdput(bdev);

	pr_info("Removed device: %s\n", zram->disk->disk_name);

	del_gendisk(zram->disk);
	blk_cleanup_queue(zram->disk->queue);
	put_disk(zram->disk);
	kfree(zram);
	return 0;
}

/* zram-control sysfs attributes */

/*
 * NOTE: hot_add attribute is not the usual read-only sysfs attribute. In a
 * sense that reading from this file does alter the state of your system -- it
 * creates a new un-initialized zram device and returns back this device's
 * device_id (or an error code if it fails to create a new device).
 */
static ssize_t hot_add_show(struct class *class,
			struct class_attribute *attr,
			char *buf)
{
	int ret;

	mutex_lock(&zram_index_mutex);
	ret = zram_add();
	mutex_unlock(&zram_index_mutex);

	if (ret < 0)
		return ret;
	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);
}
static struct class_attribute class_attr_hot_add =
	__ATTR(hot_add, 0400, hot_add_show, NULL);

static ssize_t hot_remove_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	struct zram *zram;
	int ret, dev_id;

	/* dev_id is gendisk->first_minor, which is `int' */
	ret = kstrtoint(buf, 10, &dev_id);
	if (ret)
		return ret;
	if (dev_id < 0)
		return -EINVAL;

	mutex_lock(&zram_index_mutex);

	zram = idr_find(&zram_index_idr, dev_id);
	if (zram) {
		ret = zram_remove(zram);
		if (!ret)
			idr_remove(&zram_index_idr, dev_id);
	} else {
		ret = -ENODEV;
	}

	mutex_unlock(&zram_index_mutex);
	return ret ? ret : count;
}
static CLASS_ATTR_WO(hot_remove);

static struct attribute *zram_control_class_attrs[] = {
	&class_attr_hot_add.attr,
	&class_attr_hot_remove.attr,
	NULL,
};
ATTRIBUTE_GROUPS(zram_control_class);

static struct class zram_control_class = {
	.name		= "zram-control",
	.owner		= THIS_MODULE,
	.class_groups	= zram_control_class_groups,
};

static int zram_remove_cb(int id, void *ptr, void *data)
{
	zram_remove(ptr);
	return 0;
}

static void destroy_devices(void)
{
	class_unregister(&zram_control_class);
	idr_for_each(&zram_index_idr, &zram_remove_cb, NULL);
	zram_debugfs_destroy();
	idr_destroy(&zram_index_idr);
	unregister_blkdev(zram_major, "zram");
	cpuhp_remove_multi_state(CPUHP_ZCOMP_PREPARE);
}

static int __init zram_init(void)
{
	int ret;

	ret = cpuhp_setup_state_multi(CPUHP_ZCOMP_PREPARE, "block/zram:prepare",
				      zcomp_cpu_up_prepare, zcomp_cpu_dead);
	if (ret < 0)
		return ret;

#if IS_ENABLED(CONFIG_MIUI_ZRAM_MEMORY_TRACKING)
	BUILD_BUG_ON(ZRAM_WB_IDLE_SHIFT + ZRAM_WB_IDLE_BITS_LEN > BITS_PER_LONG);
#endif

	ret = class_register(&zram_control_class);
	if (ret) {
		pr_err("Unable to register zram-control class\n");
		cpuhp_remove_multi_state(CPUHP_ZCOMP_PREPARE);
		return ret;
	}

	zram_debugfs_create();
	zram_major = register_blkdev(0, "zram");
	if (zram_major <= 0) {
		pr_err("Unable to get major number\n");
		class_unregister(&zram_control_class);
		cpuhp_remove_multi_state(CPUHP_ZCOMP_PREPARE);
		return -EBUSY;
	}

	while (num_devices != 0) {
		mutex_lock(&zram_index_mutex);
		ret = zram_add();
		mutex_unlock(&zram_index_mutex);
		if (ret < 0)
			goto out_error;
		num_devices--;
	}
	return 0;

out_error:
	destroy_devices();
	return ret;
}

static void __exit zram_exit(void)
{
	destroy_devices();
}

module_init(zram_init);
module_exit(zram_exit);

module_param(num_devices, uint, 0);
MODULE_PARM_DESC(num_devices, "Number of pre-created zram devices");

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Nitin Gupta <ngupta@vflare.org>");
MODULE_DESCRIPTION("Compressed RAM Block Device");
