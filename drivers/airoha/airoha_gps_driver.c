/* Copyright Statement:
 *
 * (C) 2017  Airoha Technology Corp. All rights reserved.
 *
 * This software/firmware and related documentation ("Airoha Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to Airoha Technology Corp. ("Airoha") and/or its licensors.
 * Without the prior written permission of Airoha and/or its licensors,
 * any reproduction, modification, use or disclosure of Airoha Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 * You may only use, reproduce, modify, or distribute (as applicable) Airoha Software
 * if you have agreed to and been bound by the applicable license agreement with
 * Airoha ("License Agreement") and been granted explicit permission to do so within
 * the License Agreement ("Permitted User").  If you are not a Permitted User,
 * please cease any access or use of Airoha Software immediately.
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT AIROHA SOFTWARE RECEIVED FROM AIROHA AND/OR ITS REPRESENTATIVES
 * ARE PROVIDED TO RECEIVER ON AN "AS-IS" BASIS ONLY. AIROHA EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES AIROHA PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH AIROHA SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN AIROHA SOFTWARE. AIROHA SHALL ALSO NOT BE RESPONSIBLE FOR ANY AIROHA
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND AIROHA'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO AIROHA SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT AIROHA'S OPTION, TO REVISE OR REPLACE AIROHA SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * AIROHA FOR SUCH AIROHA SOFTWARE AT ISSUE.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/mod_devicetable.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <asm/atomic.h>
#include <linux/semaphore.h>
#include "airoha_gps_driver.h"
#include <linux/pinctrl/consumer.h>
#include <linux/ioport.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/tty_flip.h>
#include <linux/sysrq.h>
#include <linux/serial_core.h>


#define GPS_MAJOR 1
#define GPS_MINOR 2
#define AIROHA_LDO_PIN 1146
#define AIROHA_GPS_DEVICE_NAME "airoha_gps"
#define AIROHA_LOG_TAG "[airoha_gps]"

#define AIROHA_INFO(fmt, args...) \
	pr_debug("%s %s %d:" fmt, AIROHA_LOG_TAG, __func__, __LINE__, ##args)

#define UNUSED(obj) \
	(void(obj))

/* GLOBAL SYMBOL */
static dev_t gps_dev_number;
static struct class *gps_class;
struct cdev *gps_dev;

static wait_queue_head_t gps_wait_queue;
static atomic_t is_interrupt_happen;
static struct semaphore gps_file_lock;
static struct semaphore gps_file_operation_lock;
static int open_num;
struct pinctrl *pctrl;
struct pinctrl_state *pctrl_mode_active, *pctrl_mode_idle;

/* GLOBAL SYMBOL END */

static int airoha_gps_open(struct inode *inode, struct file *file_p)
{
	down(&gps_file_lock);
	open_num++;
	AIROHA_INFO("gps_open,count:%d\n", open_num);
	up(&gps_file_lock);

	return 0;
}

static int airoha_gps_release(struct inode *inode, struct file *file_p)
{
	down(&gps_file_lock);
	open_num--;
	AIROHA_INFO("gps_release,count:%d\n", open_num);
	up(&gps_file_lock);
	return 0;
}

static ssize_t airoha_gps_read(struct file *file_p, char __user *user,
	size_t len, loff_t *offset)
{
	int result = 0;
	int copy_len = 0;
	char buffer[20] = "INTERRUPT\n";

	wait_event(gps_wait_queue, atomic_read(&is_interrupt_happen) > 0);
	down(&gps_file_operation_lock);

	if (len > 20)
		copy_len = 20;
	else
		copy_len = len;
	result = copy_to_user(user, buffer, copy_len);
	atomic_dec(&is_interrupt_happen);
	up(&gps_file_operation_lock);
	return len;

}

static ssize_t  airoha_gps_write(struct file *file_p, const char __user *user,
	size_t len, loff_t *offset)
{
	char buffer[21] = {0};
	int result = 0;
	int copy_len;

	if (len > 20)
		copy_len = 20;
	else
		copy_len = len;
	result = copy_from_user(buffer, user, copy_len);
	AIROHA_INFO("%s\n", buffer);

	if (strnstr(buffer, "OPEN", sizeof(buffer)) != NULL)
		gps_chip_enable(1);
	else if (strnstr(buffer, "CLOSE", sizeof(buffer)) != NULL)
		gps_chip_enable(0);
	else if (strnstr(buffer, "DI", sizeof(buffer)) != NULL) {
		result = pinctrl_select_state(pctrl, pctrl_mode_idle);
		if (result < 0)
			AIROHA_INFO("%s : change gps chip idle failed!\n", __func__);
	} else if (stnrstr(buffer, "DF", sizeof(buffer)) != NULL) {
		result = pinctrl_select_state(pctrl, pctrl_mode_active);
		if (result < 0)
			AIROHA_INFO("%s : change gps chip active failed!\n", __func__);
	}

	return copy_len;
}
static const struct file_operations gps_cdev_ops = {
	.open = airoha_gps_open,
	.write = airoha_gps_write,
	.read = airoha_gps_read,
	.release = airoha_gps_release,

	.owner = THIS_MODULE,
};

static int xiaomi_uart_probe(struct platform_device *pdev)
{
	char request_ldo_pin;
	int result;

	AIROHA_INFO("====GPIO init Begin======\n");
	request_ldo_pin = gpio_request(AIROHA_LDO_PIN, "airoha_gps_ldo_pin");
	AIROHA_INFO("pin info:\n");
	AIROHA_INFO("ldo...%d\n", request_ldo_pin);
	gpio_direction_output(AIROHA_LDO_PIN, 0);
	AIROHA_INFO("====GPIO init done!!======\n");
	AIROHA_INFO("====Device init...\n");
	result = alloc_chrdev_region(&gps_dev_number, 0, 1, "airoha_gps_dev");

	gps_dev = cdev_alloc();
	if (!gps_dev) {
		AIROHA_INFO("cdev alloc error!\n");
		return -EPERM;
	}

	gps_dev->owner = THIS_MODULE;
	gps_dev->ops = &gps_cdev_ops;
	cdev_init(gps_dev, &gps_cdev_ops);
	cdev_add(gps_dev, gps_dev_number, 1);
	gps_class = class_create(THIS_MODULE, AIROHA_GPS_DEVICE_NAME);
	device_create(gps_class, NULL, gps_dev_number, 0, AIROHA_GPS_DEVICE_NAME);

	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl)) {
		AIROHA_INFO("%s: Unable to allocate pinctrl: %d\n",
				__FILE__, PTR_ERR(pctrl));
		return PTR_ERR(pctrl);
	}

	pctrl_mode_active = pinctrl_lookup_state(pctrl, "gps_enable_active");
	if (IS_ERR(pctrl_mode_active)) {
		AIROHA_INFO("%s: Unable to find pinctrl_state_mode_spi: %d\n",
			__FILE__, PTR_ERR(pctrl_mode_active));
		return PTR_ERR(pctrl_mode_active);
	}

	pctrl_mode_idle = pinctrl_lookup_state(pctrl, "gps_enable_suspend");
	if (IS_ERR(pctrl_mode_idle)) {
		AIROHA_INFO("%s: Unable to find pinctrl_state_mode_idle: %d\n",
			__FILE__, PTR_ERR(pctrl_mode_idle));
		return  PTR_ERR(pctrl_mode_idle);
	}

	AIROHA_INFO("[dsc]%s : pinctrl initialized\n", __func__);
	AIROHA_INFO("====Device init Done\n");
	AIROHA_INFO("Please check /dev/%s\n", AIROHA_GPS_DEVICE_NAME);
	AIROHA_INFO("Wait....\n");

	/* Init wait queue */
	init_waitqueue_head(&gps_wait_queue);
	atomic_set(&is_interrupt_happen, 0);
	sema_init(&gps_file_lock, 1);
	sema_init(&gps_file_operation_lock, 1);

	return 0;
}

static int gps_chip_enable(bool enable)
{
	if (enable)
		gpio_direction_output(AIROHA_LDO_PIN, 1);
	else
		gpio_direction_output(AIROHA_LDO_PIN, 0);

	return 0;
}

static int xiaomi_uart_remove(struct platform_device *pdev)
{

	device_destroy(gps_class, gps_dev_number);
	class_destroy(gps_class);
	unregister_chrdev_region(gps_dev_number, 1);
	gpio_free(AIROHA_LDO_PIN);

	AIROHA_INFO("====gps driver exit======\n");
	return 0;
}


static const struct of_device_id match_table[] = {
	{ .compatible = "bcm4775",},
	{},
};


/*
 * platform driver stuff
 */
static struct platform_driver xiaomi_uart_platform_driver = {
	.probe	= xiaomi_uart_probe,
	.remove	= xiaomi_uart_remove,
	.driver	= {
		.name  = "bcm4775",
		.of_match_table = match_table,
	},
};

static int __init xiaomi_tty_init(void)
{
	int ret;

	ret = platform_driver_register(&xiaomi_uart_platform_driver);

	return ret;
}

static void __exit xiaomi_tty_exit(void)
{
	platform_driver_unregister(&xiaomi_uart_platform_driver);
}

MODULE_LICENSE("GPL");
module_init(xiaomi_tty_init);
module_exit(xiaomi_tty_exit);
