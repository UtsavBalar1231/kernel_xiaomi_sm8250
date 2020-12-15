/*
 * Copyright (C) 2015-2019 Jonathan Senkerik
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/time.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/srandom.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/kthread.h>

/*
 * Size of Array.
 * Must be >= 64.
 * (actual size used will be 64
 * anything greater is thrown away).
 * Recommended prime.
 */
#define arr_RND_SIZE 67
/*
 * Number of 512b Array
 * (Must be power of 2)
 */
#define num_arr_RND  16
/*
 * Dev name as it appears in /proc/devices
 */
#define sDEVICE_NAME "srandom"
#define AppVERSION "1.38.0"
/*
 * Amount of time worker thread should sleep between each operation.
 * Recommended prime
 */
#define THREAD_SLEEP_VALUE 7
#define PAID 0
#define COPY_TO_USER raw_copy_to_user
#define COPY_FROM_USER raw_copy_from_user
#define KTIME_GET_NS ktime_get_real_ts64
#define TIMESPEC timespec64

/*
 * Prototypes
 */
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static uint64_t xorshft64(void);
static uint64_t xorshft128(void);
static int nextbuffer(void);
static void update_sarray(int);
static void seed_PRND_s0(void);
static void seed_PRND_s1(void);
static void seed_PRND_x(void);
static int proc_read(struct seq_file *m, void *v);
static int proc_open(struct inode *inode, struct  file *file);
static int work_thread(void *data);

/*
 * Global variables are declared as static, so are global within the file.
 */
const struct file_operations sfops = {
	.owner   = THIS_MODULE,
	.open	= device_open,
	.read	= sdevice_read,
	.write   = sdevice_write,
	.release = device_release
};

static struct miscdevice srandom_dev = {
	MISC_DYNAMIC_MINOR,
	"srandom",
	&sfops
};


static const struct file_operations proc_fops = {
	.owner   = THIS_MODULE,
	.read	= seq_read,
	.open	= proc_open,
	.llseek  = seq_lseek,
	.release = single_release,
};

static struct mutex UpArr_mutex;
static struct mutex Open_mutex;
static struct mutex ArrBusy_mutex;
static struct mutex UpPos_mutex;

static struct task_struct *kthread;

/*
 * Global variables
 */
/* Used for xorshft64 */
uint64_t x;
/* Used for xorshft128 */
uint64_t s[2];
/* Array of Array of SECURE RND numbers */
uint64_t (*sarr_RND)[num_arr_RND + 1];
/* Binary Flags for Busy Arrays */
uint16_t CC_Busy_Flags;
/* Array reserved to determine which buffer to use */
int CC_buffer_position;

uint64_t tm_seed;
struct TIMESPEC tsp;

/*
 * Global counters
 */
int16_t sdev_open; /* srandom device current open count */
int32_t sdev_openCount;	/* srandom device total open count */
uint64_t PRNGCount; /* Total generated (512byte) */

/*
 * This function is called when the module is loaded
 */
int mod_init(void)
{
	int16_t C, CC;
	int ret;

	sdev_open = 0;
	sdev_openCount = 0;
	PRNGCount = 0;

	mutex_init(&UpArr_mutex);
	mutex_init(&Open_mutex);
	mutex_init(&ArrBusy_mutex);
	mutex_init(&UpPos_mutex);

	/*
	 * Entropy Initialize #1
	 */
	KTIME_GET_NS(&tsp);
	x = (uint64_t)tsp.tv_nsec;
	s[0] = xorshft64();
	s[1] = xorshft64();

	/*
	 * Register char device
	 */
	ret = misc_register(&srandom_dev);
	if (ret)
		pr_debug("/dev/srandom registration failed..\n");
	else
		pr_debug("/dev/srandom registered..\n");

	/*
	 * Create /proc/srandom
	 */
	if (!proc_create("srandom", 0, NULL, &proc_fops))
		pr_debug("/proc/srandom registration failed..\n");
	else
		pr_debug("/proc/srandom registration registered..\n");

	pr_debug("Module version: "AppVERSION"\n");

	sarr_RND = kzalloc((num_arr_RND + 1) * arr_RND_SIZE * sizeof(uint64_t),
	GFP_KERNEL);
	while (!sarr_RND) {
		pr_debug("kzalloc failed to allocate initial memory. retrying...\n");
		sarr_RND = kzalloc((num_arr_RND + 1) *
			arr_RND_SIZE * sizeof(uint64_t), GFP_KERNEL);
	}

	/*
	 * Entropy Initialize #2
	 */
	seed_PRND_s0();
	seed_PRND_s1();
	seed_PRND_x();

	/*
	 * Init the sarray
	 */
	for (CC = 0; num_arr_RND >= CC; CC++) {
		for (C = 0; arr_RND_SIZE >= C; C++)
			sarr_RND[CC][C] = xorshft128();
		update_sarray(CC);
	}

	kthread = kthread_create(work_thread, NULL, "mykthread");
	wake_up_process(kthread);

	return 0;
}

/*
 * This function is called when the module is unloaded
 */
void mod_exit(void)
{
	kthread_stop(kthread);
	misc_deregister(&srandom_dev);
	remove_proc_entry("srandom", NULL);
	pr_debug("srandom deregistered..\n");
}


/*
 * This function is alled when a process tries to open the device file.
 * "dd if=/dev/srandom"
 */
static int device_open(struct inode *inode, struct file *file)
{
	while (mutex_lock_interruptible(&Open_mutex))
		;

	sdev_open++;
	sdev_openCount++;
	mutex_unlock(&Open_mutex);

	pr_debug("(current open) :%d\n", sdev_open);
	pr_debug("(total open)   :%d\n", sdev_openCount);

	return 0;
}


/*
 * Called when a process closes the device file.
 */
static int device_release(struct inode *inode, struct file *file)
{
	while (mutex_lock_interruptible(&Open_mutex))
		;

	sdev_open--;
	mutex_unlock(&Open_mutex);

	pr_debug("(current open) :%d\n", sdev_open);

	return 0;
}

/*
 * Called when a process reads from the device.
 */
ssize_t sdevice_read(struct file *file, char *buf,
size_t count, loff_t *ppos)
{
	/* Buffer to hold numbers to send */
	char *new_buf;
	int ret, counter;
	int CC;
	size_t src_counter;

	pr_debug("count:%zu\n", count);

	/*
	 * if requested count is small (<512), then select an array and send it
	 * otherwise, create a new larger buffer to hold it all.
	 */
	if (count <= 512) {
		while (mutex_lock_interruptible(&ArrBusy_mutex))
			;

		CC = nextbuffer();
		while ((CC_Busy_Flags & 1 << CC) == (1 << CC)) {
			CC += 1;
			if (num_arr_RND <= CC)
				CC = 0;
		}

		/*
		 * Mark the Arry as busy by setting the flag
		 */
		CC_Busy_Flags += (1 << CC);
		mutex_unlock(&ArrBusy_mutex);

		/*
		 *  Send array to device
		 */
		ret = COPY_TO_USER(buf, sarr_RND[CC], count);

		/*
		 * Get more RND numbers
		 */
		update_sarray(CC);

		pr_debug("small CC_Busy_Flags:%d CC:%d\n", CC_Busy_Flags, CC);

		/*
		 * Clear CC_Busy_Flag
		 */
		if (mutex_lock_interruptible(&ArrBusy_mutex))
			return -ERESTARTSYS;

		CC_Busy_Flags -= (1 << CC);
		mutex_unlock(&ArrBusy_mutex);
	} else {
		/*
		 * Allocate memory for new_buf
		 */
		long count_remaining = count;

		pr_debug("count_remaining:%ld count:%ld\n",
			count_remaining, count);

		while (count_remaining > 0) {
			pr_debug("count_remaining:%ld count:%ld\n",
				count_remaining, count);

			new_buf = kzalloc((count_remaining + 512) *
				sizeof(uint8_t), GFP_KERNEL);
			while (!new_buf) {
				pr_debug("buffered kzalloc failed to allocate buffer.",
					"retrying...\n");
				new_buf = kzalloc((count_remaining + 512) *
					sizeof(uint8_t), GFP_KERNEL);
			}

			counter = 0;
			src_counter = 512;
			ret = 0;

			/*
			 * Select a RND array
			 */
			while (mutex_lock_interruptible(&ArrBusy_mutex))
				;

			CC = nextbuffer();
			while ((CC_Busy_Flags & 1 << CC) == (1 << CC)) {
				CC = xorshft128() & (num_arr_RND - 1);
				pr_debug("buffered CC_Busy_Flags:%d CC:%d\n",
					CC_Busy_Flags, CC);
			}

			/*
			 * Mark the Arry as busy by setting the flag
			 */
			CC_Busy_Flags += (1 << CC);
			mutex_unlock(&ArrBusy_mutex);

			/*
			 * Loop until we reach count_remaining size.
			 */
			while (counter < (int)count_remaining) {
				/*
				 * Copy RND numbers to new_buf
				 */
				memcpy(new_buf + counter, sarr_RND[CC],
					src_counter);
				update_sarray(CC);

				pr_debug("buffered COPT_TO_USER counter:%d count_remaining:%zu\n",
					counter, count_remaining);

				counter += 512;
			}

			/*
			 * Clear CC_Busy_Flag
			 */
			while (mutex_lock_interruptible(&ArrBusy_mutex))
				;

			CC_Busy_Flags -= (1 << CC);
			mutex_unlock(&ArrBusy_mutex);

			/*
			 * Send new_buf to device
			 */
			ret = COPY_TO_USER(buf, new_buf, count_remaining);

			/*
			 * Free allocated memory
			 */
			kfree(new_buf);

			count_remaining = count_remaining - 1048576;
		}
	}
	/*
	 * return how many chars we sent
	 */
	return count;
}
EXPORT_SYMBOL(sdevice_read);

/*
 * Called when someone tries to write to /dev/srandom device
 */
ssize_t sdevice_write(struct file *file,
const char __user *buf, size_t count, loff_t *ppos)
{
	char *newdata;
	int  ret;

	pr_debug("count:%zu\n", count);

	/*
	 * Allocate memory to read from device
	 */
	newdata = kzalloc(count, GFP_KERNEL);
	while (!newdata)
		newdata = kzalloc(count, GFP_KERNEL);

	ret = COPY_FROM_USER(newdata, buf, count);

	/*
	 * Free memory
	 */
	kfree(newdata);

	pr_debug("COPT_FROM_USER count:%zu\n", count);

	return count;
}



/*
 * Update the sarray with new random numbers
 */
void update_sarray(int CC)
{
	int16_t C;
	int64_t X, Y, Z1, Z2, Z3;

	/*
	 * This function must run exclusivly
	 */
	while (mutex_lock_interruptible(&UpArr_mutex))
		;

	PRNGCount++;

	Z1 = xorshft64();
	Z2 = xorshft64();
	Z3 = xorshft64();
	if ((Z1 & 1) == 0) {
		pr_debug("0\n");
		for (C = 0; C < (arr_RND_SIZE - 4) ; C = C + 4) {
			X = xorshft128();
			Y = xorshft128();
			sarr_RND[CC][C]	 = sarr_RND[CC][C + 1] ^ X ^ Y;
			sarr_RND[CC][C + 1] = sarr_RND[CC][C + 2] ^ Y ^ Z1;
			sarr_RND[CC][C + 2] = sarr_RND[CC][C + 3] ^ X ^ Z2;
			sarr_RND[CC][C + 3] = X ^ Y ^ Z3;
		}
	} else {
		pr_debug("1\n");
		for (C = 0; C < (arr_RND_SIZE - 4) ; C = C + 4) {
			X = xorshft128();
			Y = xorshft128();
			sarr_RND[CC][C]	 = sarr_RND[CC][C + 1] ^ X ^ Z2;
			sarr_RND[CC][C + 1] = sarr_RND[CC][C + 2] ^ X ^ Y;
			sarr_RND[CC][C + 2] = sarr_RND[CC][C + 3] ^ Y ^ Z3;
			sarr_RND[CC][C + 3] = X ^ Y ^ Z1;
		}
	}

	mutex_unlock(&UpArr_mutex);

	pr_debug("CC:%d, X:%llu, Y:%llu, Z1:%llu, Z2:%llu, Z3:%llu,\n",
		CC, X, Y, Z1, Z2, Z3);
}
EXPORT_SYMBOL(sdevice_write);

/*
 *  Seeding the xorshft's
 */
void seed_PRND_s0(void)
{
	 KTIME_GET_NS(&tsp);
	 s[0] = (s[0] << 31) ^ (uint64_t)tsp.tv_nsec;
	 pr_debug("x:%llu, s[0]:%llu, s[1]:%llu\n",
		x, s[0], s[1]);
}

void seed_PRND_s1(void)
{
	KTIME_GET_NS(&tsp);
	s[1] = (s[1] << 24) ^ (uint64_t)tsp.tv_nsec;
	pr_debug("x:%llu, s[0]:%llu, s[1]:%llu\n",
		x, s[0], s[1]);
}

void seed_PRND_x(void)
{
	KTIME_GET_NS(&tsp);
	x = (x << 32) ^ (uint64_t)tsp.tv_nsec;
	pr_debug("x:%llu, s[0]:%llu, s[1]:%llu\n",
		x, s[0], s[1]);
}

/*
 * PRNG functions
 */
uint64_t xorshft64(void)
{
	uint64_t z = (x += 0x9E3779B97F4A7C15ULL);

	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	return z ^ (z >> 31);
}

uint64_t xorshft128(void)
{
	uint64_t s1 = s[0];
	const uint64_t s0 = s[1];

	s[0] = s0;
	s1 ^= s1 << 23;
	return (s[1] = (s1 ^ s0 ^ (s1 >> 17) ^ (s0 >> 26))) + s0;
}

/*
 *  This function returns the next sarray to use/read.
 */
int nextbuffer(void)
{
	uint8_t position = (int)((CC_buffer_position * 4) / 64);
	uint8_t roll = CC_buffer_position % 16;
	uint8_t nextbuffer = (sarr_RND[num_arr_RND][position] >> (roll * 4))
		& (num_arr_RND - 1);

	pr_debug("raw:%lld",
			"position:%d",
			"roll:%d",
			"%s:%d",
			"CC_buffer_position:%d\n",
			sarr_RND[num_arr_RND][position],
			position,
			roll,
			__func__,
			nextbuffer,
			CC_buffer_position);

	while (mutex_lock_interruptible(&UpPos_mutex))
		;
	CC_buffer_position++;
	mutex_unlock(&UpPos_mutex);

	if (CC_buffer_position >= 1021) {
		while (mutex_lock_interruptible(&UpPos_mutex))
			;
		CC_buffer_position = 0;
		mutex_unlock(&UpPos_mutex);
		update_sarray(num_arr_RND);
	}

	return nextbuffer;
}

/*
 *  The Kernel thread doing background tasks.
 */
int work_thread(void *data)
{
	int interation = 0;

	while (!kthread_should_stop()) {
		if (interation <= num_arr_RND)
			update_sarray(interation);
		else if (interation == num_arr_RND + 1)
			seed_PRND_s0();
		else if (interation == num_arr_RND + 2)
			seed_PRND_s1();
		else if (interation == num_arr_RND + 3)
			seed_PRND_x();
		else
			interation = -1;

		interation++;
		ssleep(THREAD_SLEEP_VALUE);
	}

	do_exit(0);
	return 0;
}

/*
 * This function is called when reading /proc filesystem
 */
int proc_read(struct seq_file *m, void *v)
{
	seq_puts(m, "-----------------------:----------------------\n");
	seq_puts(m, "Device                 : /dev/"sDEVICE_NAME"\n");
	seq_puts(m, "Module version         : "AppVERSION"\n");
	seq_printf(m, "Current open count     : %d\n", sdev_open);
	seq_printf(m, "Total open count       : %d\n", sdev_openCount);
	seq_printf(m, "Total K bytes          : %llu\n", PRNGCount / 2);
	if (PAID == 0) {
		seq_puts(m, "-----------------------:----------------------\n");
		seq_puts(m, "Please support my work and efforts contributing\n");
		seq_puts(m, "to the Linux community.  A $25 payment per\n");
		seq_puts(m, "server would be highly appreciated.\n");
	}
	seq_puts(m, "-----------------------:----------------------\n");
	seq_puts(m, "Author                 : Jonathan Senkerik\n");
	seq_puts(m, "Website                : http://www.jintegrate.co\n");
	seq_puts(m, "github                 : http://github.com/josenk/srandom\n");
	if (PAID == 0) {
		seq_puts(m, "Paypal                 : josenk@jintegrate.co\n");
		seq_puts(m, "Bitcoin                : 1GEtkAm97DphwJbJTPyywv6NbqJKLMtDzA\n");
		seq_puts(m, "Commercial Invoice     : Avail on request.\n");
	}
	return 0;
}

int proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, proc_read, NULL);
}

module_init(mod_init);
module_exit(mod_exit);

/*
 *  Module license information
 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jonathan Senkerik <josenk@jintegrate.co>");
MODULE_DESCRIPTION("Improved random number generator.");
MODULE_SUPPORTED_DEVICE("/dev/srandom");
