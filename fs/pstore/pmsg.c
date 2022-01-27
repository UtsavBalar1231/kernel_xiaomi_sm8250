/*
 * Copyright 2014  Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/uio.h>
#include "internal.h"

static DEFINE_MUTEX(pmsg_lock);

#define LOGGER_MAGIC		'l'
#define LOG_ID_EVENTS		2

enum android_log_priority {
	ANDROID_LOG_UNKNOWN = 0,
	ANDROID_LOG_DEFAULT,
	ANDROID_LOG_VERBOSE,
	ANDROID_LOG_DEBUG,
	ANDROID_LOG_INFO,
	ANDROID_LOG_WARN,
	ANDROID_LOG_ERROR,
	ANDROID_LOG_FATAL,
	ANDROID_LOG_SILENT,
};

static const char log_characters[] = "XXVDIWEF";

struct log_time {
	uint32_t tv_sec;
	uint32_t tv_nsec;
} __attribute__((__packed__));

struct android_pmsg_log_header {
	uint8_t magic;
	uint16_t len;
	uint16_t uid;
	uint16_t pid;
} __attribute__((__packed__));

struct android_log_header {
	uint8_t id;
	uint16_t tid;
	struct log_time realtime;
} __attribute__((__packed__));

struct android_pmsg_prio_header {
	uint8_t prio;
} __attribute__((__packed__));

struct android_pmsg_tag_header {
	uint32_t tag;
} __attribute__((__packed__));

static ssize_t pmsg_write(char *buf, size_t count)
{
	struct pstore_record record;
	int ret;

	pstore_record_init(&record, psinfo);
	record.type = PSTORE_TYPE_PMSG;
	record.size = count;
	record.buf = buf;

	mutex_lock(&pmsg_lock);
	ret = psinfo->write(&record);
	mutex_unlock(&pmsg_lock);
	return ret ? ret : count;
}

static ssize_t pmsg_write_user(const char __user *buf, size_t count)
{
	struct pstore_record record;
	int ret;

	pstore_record_init(&record, psinfo);
	record.type = PSTORE_TYPE_PMSG;
	record.size = count;

	mutex_lock(&pmsg_lock);
	ret = psinfo->write_user(&record, buf);
	mutex_unlock(&pmsg_lock);
	return ret ? ret : count;
}

#define pmsg_write_arr(arr) pmsg_write(arr, sizeof(arr) - 1)

static void pmsg_write_iovec_str(const struct iovec *iov)
{
	void __user *buf = iov->iov_base;
	size_t len = iov->iov_len - 1;

	if (len <= 0)
		return;

	pmsg_write_user(buf, len);
}

static bool pmsg_is_pmsg_header(unsigned long part, const struct iovec *iov)
{
	struct android_pmsg_log_header pmsg_header;
	void __user *buf = iov->iov_base;
	size_t len = iov->iov_len;

	int ret;

	if (part != 0)
		return false;

	if (len != sizeof(pmsg_header))
		return false;

	ret = __copy_from_user(&pmsg_header, buf, len);
	if (ret)
		return false;

	if (pmsg_header.magic != LOGGER_MAGIC)
		return false;

	return true;
}

static bool pmsg_is_header(unsigned long part, const struct iovec *iov)
{
	struct android_log_header header;
	void __user *buf = iov->iov_base;
	size_t len = iov->iov_len;
	int ret;

	if (part != 1)
		return false;

	if (len != sizeof(struct android_log_header))
		return false;

	ret = __copy_from_user(&header, buf, len);
	if (ret)
		return false;

	if (header.id == LOG_ID_EVENTS)
		return false;

	return true;
}

static bool pmsg_is_prio(unsigned long part, const struct iovec *iov,
			 char *priority)
{
	struct android_pmsg_prio_header prio_header;
	void __user *buf = iov->iov_base;
	size_t len = iov->iov_len;
	int ret;

	if (part != 2)
		return false;

	if (len != sizeof(struct android_pmsg_prio_header))
		return false;

	ret = __copy_from_user(&prio_header, buf, len);
	if (ret)
		return false;

	if (prio_header.prio >= ANDROID_LOG_SILENT)
		prio_header.prio = ANDROID_LOG_FATAL;

	priority[0] = log_characters[prio_header.prio];

	return true;
}

static bool pmsg_is_tag(unsigned long part, const struct iovec *iov)
{
	size_t len = iov->iov_len;

	if (part != 2)
		return false;

	if (len != sizeof(struct android_pmsg_tag_header))
		return false;

	return true;
}

static bool pmsg_is_name_tag(unsigned long part)
{
	return part == 3;
}

static bool pmsg_is_message(unsigned long part)
{
	return part == 4;
}

char name_tag_end[] = ": ";
char message_end[] = "\n";

static ssize_t pmsg_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	size_t count = iov_iter_count(from);
	unsigned long i;
	char priority[] = "# ";

	if (!iter_is_iovec(from))
		return -EINVAL;

	for (i = 0; i < from->nr_segs; i++) {
		const struct iovec *iov = &from->iov[i];

		if (pmsg_is_pmsg_header(i, iov) ||
		    pmsg_is_header(i, iov) ||
		    pmsg_is_tag(i, iov)) {
			/* continue */
		} else if (pmsg_is_prio(i, iov, priority)) {
			pmsg_write_arr(priority);
		} else if (pmsg_is_name_tag(i)) {
			pmsg_write_iovec_str(iov);
			pmsg_write_arr(name_tag_end);
		} else if (pmsg_is_message(i)) {
			pmsg_write_iovec_str(iov);
			pmsg_write_arr(message_end);
		} else {
			break;
		}
	}

	return count;
}

static const struct file_operations pmsg_fops = {
	.owner		= THIS_MODULE,
	.llseek		= noop_llseek,
	.write_iter = pmsg_write_iter,
};

static struct class *pmsg_class;
static int pmsg_major;
#define PMSG_NAME "pmsg"
#undef pr_fmt
#define pr_fmt(fmt) PMSG_NAME ": " fmt

static char *pmsg_devnode(struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0220;
	return NULL;
}

void pstore_register_pmsg(void)
{
	struct device *pmsg_device;

	pmsg_major = register_chrdev(0, PMSG_NAME, &pmsg_fops);
	if (pmsg_major < 0) {
		pr_err("register_chrdev failed\n");
		goto err;
	}

	pmsg_class = class_create(THIS_MODULE, PMSG_NAME);
	if (IS_ERR(pmsg_class)) {
		pr_err("device class file already in use\n");
		goto err_class;
	}
	pmsg_class->devnode = pmsg_devnode;

	pmsg_device = device_create(pmsg_class, NULL, MKDEV(pmsg_major, 0),
					NULL, "%s%d", PMSG_NAME, 0);
	if (IS_ERR(pmsg_device)) {
		pr_err("failed to create device\n");
		goto err_device;
	}
	return;

err_device:
	class_destroy(pmsg_class);
err_class:
	unregister_chrdev(pmsg_major, PMSG_NAME);
err:
	return;
}

void pstore_unregister_pmsg(void)
{
	device_destroy(pmsg_class, MKDEV(pmsg_major, 0));
	class_destroy(pmsg_class);
	unregister_chrdev(pmsg_major, PMSG_NAME);
}
