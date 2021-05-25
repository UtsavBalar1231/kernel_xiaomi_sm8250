#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>             /* For kalloc */
#include <linux/uaccess.h>          /* For copy_to_user */
#include <linux/miscdevice.h>       /* For misc_register (the /dev/srandom) device */
#include <linux/time.h>             /* For getnstimeofday/ktime_get_real_ts64 */
#include <linux/proc_fs.h>          /* For /proc filesystem */
#include <linux/seq_file.h>         /* For seq_print */
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#define DRIVER_AUTHOR "Jonathan Senkerik <josenk@jintegrate.co>"
#define DRIVER_DESC   "Improved random number generator."
#define arr_RND_SIZE 67             /* Size of Array.  Must be >= 64. (actual size used will be 64, anything greater is thrown away). Recommended prime.*/
#define num_arr_RND  16             /* Number of 512b Array (Must be power of 2) */
#define sDEVICE_NAME "srandom"      /* Dev name as it appears in /proc/devices */
#define AppVERSION "1.38.0"
#define THREAD_SLEEP_VALUE 7        /* Amount of time worker thread should sleep between each operation. Recommended prime */
#define PAID 0
// #define DEBUG 0

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
    #define COPY_TO_USER raw_copy_to_user
    #define COPY_FROM_USER raw_copy_from_user
#else
    #define COPY_TO_USER copy_to_user
    #define COPY_FROM_USER copy_from_user
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,8,0)
    #define KTIME_GET_NS ktime_get_real_ts64
    #define TIMESPEC timespec64
#else
    #define KTIME_GET_NS getnstimeofday
    #define TIMESPEC timespec
#endif


/*
 * Copyright (C) 2015 Jonathan Senkerik
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Prototypes
 */
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t sdevice_read(struct file *, char *, size_t, loff_t *);
static ssize_t sdevice_write(struct file *, const char *, size_t, loff_t *);
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
static struct file_operations sfops = {
        .owner   = THIS_MODULE,
        .open    = device_open,
        .read    = sdevice_read,
        .write   = sdevice_write,
        .release = device_release
};

static struct miscdevice srandom_dev = {
        MISC_DYNAMIC_MINOR,
        "srandom",
        &sfops
};


#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,8,0)
static struct proc_ops proc_fops={
      .proc_open = proc_open,
      .proc_release = single_release,
      .proc_read = seq_read,
      .proc_lseek = seq_lseek
};
#else
static const struct file_operations proc_fops = {
        .owner   = THIS_MODULE,
        .read    = seq_read,
        .open    = proc_open,
        .llseek  = seq_lseek,
        .release = single_release,
};
#endif


static struct mutex UpArr_mutex;
static struct mutex Open_mutex;
static struct mutex ArrBusy_mutex;
static struct mutex UpPos_mutex;

static struct task_struct *kthread;


/*
 * Global variables
 */
uint64_t x;                             /* Used for xorshft64 */
uint64_t s[ 2 ];                        /* Used for xorshft128 */
uint64_t (*sarr_RND)[num_arr_RND + 1];  /* Array of Array of SECURE RND numbers */
uint16_t CC_Busy_Flags = 0;             /* Binary Flags for Busy Arrays */
int      CC_buffer_position = 0;        /* Array reserved to determine which buffer to use */
uint64_t tm_seed;
struct   TIMESPEC ts;

/*
 * Global counters
 */
int16_t  sdev_open;                 /* srandom device current open count */
int32_t  sdev_openCount;            /* srandom device total open count */
uint64_t PRNGCount;                 /* Total generated (512byte) */


/*
 * This function is called when the module is loaded
 */
int mod_init(void)
{
        int16_t C,CC;
        int     ret;

        sdev_open      = 0;
        sdev_openCount = 0;
        PRNGCount      = 0;

        mutex_init(&UpArr_mutex);
        mutex_init(&Open_mutex);
        mutex_init(&ArrBusy_mutex);
        mutex_init(&UpPos_mutex);

        /*
         * Entropy Initialize #1
         */
        KTIME_GET_NS(&ts);
        x    = (uint64_t)ts.tv_nsec;
        s[0] = xorshft64();
        s[1] = xorshft64();

        /*
         * Register char device
         */
        ret = misc_register(&srandom_dev);
        if (ret)
                printk(KERN_INFO "[srandom] mod_init /dev/srandom driver registion failed..\n");
        else
                printk(KERN_INFO "[srandom] mod_init /dev/srandom driver registered..\n");

        /*
         * Create /proc/srandom
         */
        // if (! proc_create("srandom", 0, NULL, &proc_fops))
        if (! proc_create("srandom", 0, NULL, &proc_fops))
                printk(KERN_INFO "[srandom] mod_init /proc/srandom registion failed..\n");
        else
                printk(KERN_INFO "[srandom] mod_init /proc/srandom registion regisered..\n");

        printk(KERN_INFO "[srandom] mod_init Module version         : "AppVERSION"\n");
        if (PAID == 0) {
                printk(KERN_INFO "-----------------------:----------------------\n");
                printk(KERN_INFO "Please support my work and efforts contributing\n");
                printk(KERN_INFO "to the Linux community.  A $25 payment per\n");
                printk(KERN_INFO "server would be highly appreciated.\n");
        }
        printk(KERN_INFO "-----------------------:----------------------\n");
        printk(KERN_INFO "Author                 : Jonathan Senkerik\n");
        printk(KERN_INFO "Website                : http://www.jintegrate.co\n");
        printk(KERN_INFO "github                 : http://github.com/josenk/srandom\n");
        if (PAID == 0) {
                printk(KERN_INFO "Paypal                 : josenk@jintegrate.co\n");
                printk(KERN_INFO "Bitcoin                : 1MTNg7SqcEWs5uwLKwNiAfYqBfnKFJu65p\n");
                printk(KERN_INFO "Commercial Invoice     : Avail on request.\n");
        }


        sarr_RND = kmalloc((num_arr_RND + 1) * arr_RND_SIZE * sizeof(uint64_t), GFP_KERNEL);
        while (!sarr_RND) {
                printk(KERN_INFO "[srandom] mod_init kmalloc failed to allocate initial memory.  retrying...\n");
                sarr_RND = kmalloc((num_arr_RND + 1) * arr_RND_SIZE * sizeof(uint64_t), GFP_KERNEL);
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
        for (CC = 0;CC <= num_arr_RND ;CC++) {
                for (C = 0;C <= arr_RND_SIZE;C++) {
                        sarr_RND[CC][C] = xorshft128();
                }
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

        printk(KERN_INFO "[srandom] mod_exit srandom deregisered..\n");
}


/*
 * This function is alled when a process tries to open the device file. "dd if=/dev/srandom"
 */
static int device_open(struct inode *inode, struct file *file)
{
        while (mutex_lock_interruptible(&Open_mutex));

        sdev_open++;
        sdev_openCount++;
        mutex_unlock(&Open_mutex);

        #ifdef DEBUG
        printk(KERN_INFO "[srandom] device_open (current open) :%d\n",sdev_open);
        printk(KERN_INFO "[srandom] device_open (total open)   :%d\n",sdev_openCount);
        #endif

        return 0;
}


/*
 * Called when a process closes the device file.
 */
static int device_release(struct inode *inode, struct file *file)
{
        while (mutex_lock_interruptible(&Open_mutex));

        sdev_open--;
        mutex_unlock(&Open_mutex);

        #ifdef DEBUG
        printk(KERN_INFO "[srandom] device_release (current open) :%d\n", sdev_open);
        #endif

        return 0;
}

/*
 * Called when a process reads from the device.
 */
static ssize_t sdevice_read(struct file * file, char * buf, size_t count, loff_t *ppos)
{
        char *new_buf;                 /* Buffer to hold numbers to send */
        int ret, counter;
        int CC;
        size_t src_counter;

        #ifdef DEBUG
        printk(KERN_INFO "[srandom] sdevice_read count:%zu\n", count);
        #endif

        /*
         * if requested count is small (<512), then select an array and send it
         * otherwise, create a new larger buffer to hold it all.
         */
        if (count <= 512) {

                while (mutex_lock_interruptible(&ArrBusy_mutex));

                CC = nextbuffer();
                while ((CC_Busy_Flags & 1 << CC) == (1 << CC)) {
                        CC += 1;
                        if (CC >= num_arr_RND) {
                                CC = 0;
                        }
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

                #ifdef DEBUG2
                printk(KERN_INFO "[srandom] small CC_Busy_Flags:%d CC:%d\n", CC_Busy_Flags, CC);
                #endif

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
                #ifdef DEBUG4
                printk(KERN_INFO "[srandom] count_remaining:%ld count:%ld\n", count_remaining, count);
                #endif

                while (count_remaining > 0) {
                        #ifdef DEBUG4
                        printk(KERN_INFO "[srandom] count_remaining:%ld count:%ld\n", count_remaining, count);
                        #endif

                        new_buf = kmalloc((count_remaining + 512) * sizeof(uint8_t), GFP_KERNEL);
                        while (!new_buf) {
                                printk(KERN_INFO "[srandom] buffered kmalloc failed to allocate buffer.  retrying...\n");
                                new_buf = kmalloc((count_remaining + 512) * sizeof(uint8_t), GFP_KERNEL);
                        }

                        counter = 0;
                        src_counter = 512;
                        ret = 0;

                        /*
                         * Select a RND array
                         */
                        while (mutex_lock_interruptible(&ArrBusy_mutex));

                        CC = nextbuffer();
                        while ((CC_Busy_Flags & 1 << CC) == (1 << CC)) {
                                CC = xorshft128() & (num_arr_RND -1);
                                #ifdef DEBUG2
                                printk(KERN_INFO "[srandom] buffered CC_Busy_Flags:%d CC:%d\n", CC_Busy_Flags, CC);
                                #endif
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
                                memcpy(new_buf + counter, sarr_RND[CC], src_counter);
                                update_sarray(CC);

                                #ifdef DEBUG2
                                printk(KERN_INFO "[srandom] buffered COPT_TO_USER counter:%d count_remaining:%zu \n",\
                                 counter, count_remaining);
                                #endif

                                counter += 512;
                        }

                        /*
                         * Clear CC_Busy_Flag
                         */
                        while (mutex_lock_interruptible(&ArrBusy_mutex)) ;

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


/*
 * Called when someone tries to write to /dev/srandom device
 */
static ssize_t sdevice_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{

        char *newdata;
        int  ret;

        #ifdef DEBUG
        printk(KERN_INFO "[srandom] sdevice_write count:%zu\n", count);
        #endif

        /*
         * Allocate memory to read from device
         */
        newdata = kmalloc(count, GFP_KERNEL);
        while (!newdata) {
                newdata = kmalloc(count, GFP_KERNEL);
        }

        ret = COPY_FROM_USER(newdata, buf, count);

        /*
         * Free memory
         */
        kfree(newdata);

        #ifdef DEBUG2
        printk(KERN_INFO "[srandom] sdevice_write COPT_FROM_USER count:%zu \n", count);
        #endif

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
        while (mutex_lock_interruptible(&UpArr_mutex));

        PRNGCount++;

        Z1 = xorshft64();
        Z2 = xorshft64();
        Z3 = xorshft64();
        if ((Z1 & 1) == 0) {
                #ifdef DEBUG
                printk(KERN_INFO "[srandom] update_sarray 0\n");
                #endif

                for (C = 0;C < (arr_RND_SIZE -4) ;C = C + 4) {
                        X=xorshft128();
                        Y=xorshft128();
                        sarr_RND[CC][C]     = sarr_RND[CC][C + 1] ^ X ^ Y;
                        sarr_RND[CC][C + 1] = sarr_RND[CC][C + 2] ^ Y ^ Z1;
                        sarr_RND[CC][C + 2] = sarr_RND[CC][C + 3] ^ X ^ Z2;
                        sarr_RND[CC][C + 3] = X ^ Y ^ Z3;
                }
        } else {
                #ifdef DEBUG
                printk(KERN_INFO "[srandom] update_sarray 1\n");
                #endif

                for (C = 0;C < (arr_RND_SIZE -4) ;C = C + 4) {
                        X=xorshft128();
                        Y=xorshft128();
                        sarr_RND[CC][C]     = sarr_RND[CC][C + 1] ^ X ^ Z2;
                        sarr_RND[CC][C + 1] = sarr_RND[CC][C + 2] ^ X ^ Y;
                        sarr_RND[CC][C + 2] = sarr_RND[CC][C + 3] ^ Y ^ Z3;
                        sarr_RND[CC][C + 3] = X ^ Y ^ Z1;
                }
        }

        mutex_unlock(&UpArr_mutex);

        #ifdef DEBUG
        printk(KERN_INFO "[srandom] update_sarray CC:%d, X:%llu, Y:%llu, Z1:%llu, Z2:%llu, Z3:%llu,\n", CC, X, Y, Z1, Z2, Z3);
        #endif

}


/*
 *  Seeding the xorshft's
 */
 void seed_PRND_s0(void)
 {
         KTIME_GET_NS(&ts);
         s[0] = (s[0] << 31) ^ (uint64_t)ts.tv_nsec;
         #ifdef DEBUG
         printk(KERN_INFO "[srandom] seed_PRNG_s0 x:%llu, s[0]:%llu, s[1]:%llu\n", x, s[0], s[1]);
         #endif
 }
void seed_PRND_s1(void)
{
        KTIME_GET_NS(&ts);
        s[1] = (s[1] << 24) ^ (uint64_t)ts.tv_nsec;
        #ifdef DEBUG
        printk(KERN_INFO "[srandom] seed_PRNG_s1 x:%llu, s[0]:%llu, s[1]:%llu\n", x, s[0], s[1]);
        #endif
}
void seed_PRND_x(void)
{
        KTIME_GET_NS(&ts);
        x = (x << 32) ^ (uint64_t)ts.tv_nsec;
        #ifdef DEBUG
        printk(KERN_INFO "[srandom] seed_PRNG_x x:%llu, s[0]:%llu, s[1]:%llu\n", x, s[0], s[1]);
        #endif
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
        return (s[ 1 ] = (s1 ^ s0 ^ (s1 >> 17) ^ (s0 >> 26))) + s0;
}

/*
 *  This function returns the next sarray to use/read.
 */
int nextbuffer(void)
{
        uint8_t position = (int)((CC_buffer_position * 4) / 64 );
        uint8_t roll = CC_buffer_position % 16;
        uint8_t nextbuffer = (sarr_RND[num_arr_RND][position] >> (roll * 4)) & (num_arr_RND -1);

        #ifdef DEBUG3
        printk(KERN_INFO "[srandom] nextbuffer raw:%lld, position:%d, roll:%d, nextbuffer:%d,  CC_buffer_position:%d\n", sarr_RND[num_arr_RND][position], position, roll, nextbuffer, CC_buffer_position);
        #endif

        while (mutex_lock_interruptible(&UpPos_mutex));
        CC_buffer_position ++;
        mutex_unlock(&UpPos_mutex);

        if (CC_buffer_position >= 1021) {
                while (mutex_lock_interruptible(&UpPos_mutex));
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
        int interation;

        interation = 0;

        while (!kthread_should_stop()) {

                 if (interation <= num_arr_RND) {
                   update_sarray(interation);
                 }
                 else if (interation == num_arr_RND + 1) {
                   seed_PRND_s0();
                 }
                 else if (interation == num_arr_RND + 2) {
                   seed_PRND_s1();
                 }
                 else if (interation == num_arr_RND + 3) {
                   seed_PRND_x();
                 }
                 else {
                   interation = -1;
                 }

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
        seq_printf(m, "-----------------------:----------------------\n");
        seq_printf(m, "Device                 : /dev/"sDEVICE_NAME"\n");
        seq_printf(m, "Module version         : "AppVERSION"\n");
        seq_printf(m, "Current open count     : %d\n",sdev_open);
        seq_printf(m, "Total open count       : %d\n",sdev_openCount);
        seq_printf(m, "Total K bytes          : %llu\n",PRNGCount / 2);
        if (PAID == 0) {
                seq_printf(m, "-----------------------:----------------------\n");
                seq_printf(m, "Please support my work and efforts contributing\n");
                seq_printf(m, "to the Linux community.  A $25 payment per\n");
                seq_printf(m, "server would be highly appreciated.\n");
        }
        seq_printf(m, "-----------------------:----------------------\n");
        seq_printf(m, "Author                 : Jonathan Senkerik\n");
        seq_printf(m, "Website                : http://www.jintegrate.co\n");
        seq_printf(m, "github                 : http://github.com/josenk/srandom\n");
        if (PAID == 0) {
                seq_printf(m, "Paypal                 : josenk@jintegrate.co\n");
                seq_printf(m, "Bitcoin                : 1GEtkAm97DphwJbJTPyywv6NbqJKLMtDzA\n");
                seq_printf(m, "Commercial Invoice     : Avail on request.\n");
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
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_SUPPORTED_DEVICE("/dev/srandom");
