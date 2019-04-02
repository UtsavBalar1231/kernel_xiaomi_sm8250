/*Copyright (c) 2019, The Linux Foundation. All rights reserved.

* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.

* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
 */

/*!@file: DWC_ETH_QOS_poll_support.c
 */

#include "DWC_ETH_QOS_yheader.h"

#define AVB_CLASS_A_CHANNEL_NUM 2
#define AVB_CLASS_B_CHANNEL_NUM 3

extern struct DWC_ETH_QOS_prv_data *gDWC_ETH_QOS_prv_data;

struct pps_info {
	int channel_no;
};

bool avb_class_a_msg_wq_flag;
bool avb_class_b_msg_wq_flag;

DECLARE_WAIT_QUEUE_HEAD(avb_class_a_msg_wq);
DECLARE_WAIT_QUEUE_HEAD(avb_class_b_msg_wq);

static ssize_t pps_fops_read(struct file *filp, char __user *buf,
		 size_t count, loff_t *f_pos)
{

	unsigned int len = 0, buf_len = 5000;
	char* temp_buf;
	ssize_t ret_cnt = 0;
	struct pps_info *info;

	info = filp->private_data;

	if (info->channel_no == AVB_CLASS_A_CHANNEL_NUM ) {
		temp_buf = kzalloc(buf_len, GFP_KERNEL);
		if (!temp_buf)
			return -ENOMEM;

		if (gDWC_ETH_QOS_prv_data)
			len = scnprintf(temp_buf, buf_len ,
			"%ld\n", gDWC_ETH_QOS_prv_data->avb_class_a_intr_cnt);
		else
			len = scnprintf(temp_buf, buf_len , "0\n");

		ret_cnt = simple_read_from_buffer(buf, count, f_pos, temp_buf, len);
		kfree(temp_buf);
		if (gDWC_ETH_QOS_prv_data)
			EMACERR("poll pps2intr info=%d sent by kernel\n", gDWC_ETH_QOS_prv_data->avb_class_a_intr_cnt);
	} else if (info->channel_no == AVB_CLASS_B_CHANNEL_NUM ) {
		temp_buf = kzalloc(buf_len, GFP_KERNEL);
		if (!temp_buf)
			return -ENOMEM;

		if (gDWC_ETH_QOS_prv_data)
			len = scnprintf(temp_buf, buf_len ,
			"%ld\n", gDWC_ETH_QOS_prv_data->avb_class_b_intr_cnt);
		else
			len = scnprintf(temp_buf, buf_len , "0\n");

		ret_cnt = simple_read_from_buffer(buf, count, f_pos, temp_buf, len);
		kfree(temp_buf);

	} else {
		EMACERR("invalid channel %d\n",info->channel_no);
	}
	return ret_cnt;

}

static unsigned int pps_fops_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	struct pps_info *info;

	info = file->private_data;
	if (info->channel_no == AVB_CLASS_A_CHANNEL_NUM ){
		EMACDBG("avb_class_a_fops_poll wait\n");

		poll_wait(file, &avb_class_a_msg_wq, wait);

		EMACDBG("avb_class_a_fops_poll exit\n");

		if (avb_class_a_msg_wq_flag == 1) {
			//Sending read mask
			mask |= POLLIN | POLLRDNORM;
			avb_class_a_msg_wq_flag = 0;
		}
	} else if (info->channel_no == AVB_CLASS_B_CHANNEL_NUM) {
		EMACDBG("avb_class_b_fops_poll wait\n");

		poll_wait(file, &avb_class_b_msg_wq, wait);

		EMACDBG("avb_class_b_fops_poll exit\n");

		if (avb_class_b_msg_wq_flag == 1) {
			//Sending read mask
			mask |= POLLIN | POLLRDNORM;
			avb_class_b_msg_wq_flag = 0;
		}
	} else {
		EMACERR("invalid channel %d\n",info->channel_no);
	}
	return mask;
}

int pps_open(struct inode *inode, struct file *file)
{
	struct pps_info *info;

	EMACDBG("pps_open enter\n");

	info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	if (!strncmp(file->f_path.dentry->d_iname, AVB_CLASS_A_POLL_DEV_NODE_NAME , strlen (AVB_CLASS_A_POLL_DEV_NODE_NAME))) {
		EMACDBG("pps_open file name =%s \n",file->f_path.dentry->d_iname);
		info->channel_no = AVB_CLASS_A_CHANNEL_NUM;
	} else if (!strncmp(file->f_path.dentry->d_iname, AVB_CLASS_B_POLL_DEV_NODE_NAME , strlen (AVB_CLASS_B_POLL_DEV_NODE_NAME))) {
		EMACDBG("pps_open file name =%s \n",file->f_path.dentry->d_iname);
		info->channel_no = AVB_CLASS_B_CHANNEL_NUM;
	}  else {

		EMACDBG("stsrncmp failed for %s\n",file->f_path.dentry->d_iname);
	}
	file->private_data = info;
	return 0;
}

int pps_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}


 static const struct file_operations pps_fops = {
	.owner = THIS_MODULE,
	.open = pps_open,
	.release = pps_release,
	.read = pps_fops_read,
	.poll = pps_fops_poll,
};

int create_pps_interrupt_info_device_node(dev_t *pps_dev_t, struct cdev** pps_cdev,
	struct class** pps_class, char *pps_dev_node_name)
{
	int ret;
	EMACDBG("create_pps_interrupt_info_device_node enter \n");

	ret = alloc_chrdev_region(pps_dev_t, 0, 1,
							pps_dev_node_name);
	if (ret) {
		EMACERR("alloc_chrdev_region error for node %s \n", pps_dev_node_name);
		goto alloc_chrdev1_region_fail;
	}

	*pps_cdev = cdev_alloc();
	if(!*pps_cdev) {
		ret = -ENOMEM;
		EMACERR("failed to alloc cdev\n");
		goto fail_alloc_cdev;
	}
	cdev_init(*pps_cdev, &pps_fops);

	ret = cdev_add(*pps_cdev, *pps_dev_t, 1);
	if (ret < 0) {
		EMACERR(":cdev_add err=%d\n", -ret);
		goto cdev1_add_fail;
	}

	*pps_class = class_create(THIS_MODULE, pps_dev_node_name);
	if(!*pps_class) {
		ret = -ENODEV;
		EMACERR("failed to create class\n");
		goto fail_create_class;
	}

	if (!device_create(*pps_class, NULL,
		*pps_dev_t, NULL, pps_dev_node_name)) {
		ret = -EINVAL;
		EMACERR("failed to create device_create\n");
		goto fail_create_device;
	}

	EMACDBG("create_pps_interrupt_info_device_node exit successfuly \n");

	return 0;

	fail_create_device:
		class_destroy(*pps_class);
	fail_create_class:
		cdev_del(*pps_cdev);
	cdev1_add_fail:
	fail_alloc_cdev:
		unregister_chrdev_region(*pps_dev_t, 1);
	alloc_chrdev1_region_fail:
		return ret;

}

int remove_pps_interrupt_info_device_node(struct DWC_ETH_QOS_prv_data *pdata)
{
	cdev_del(pdata->avb_class_a_cdev);
	device_destroy(pdata->avb_class_a_class, pdata->avb_class_a_dev_t);
	class_destroy(pdata->avb_class_a_class);
	unregister_chrdev_region(pdata->avb_class_a_dev_t, 1);
	pdata->avb_class_a_cdev = NULL;
	pdata->avb_class_a_class = NULL;

	cdev_del(pdata->avb_class_b_cdev);
	device_destroy(pdata->avb_class_b_class, pdata->avb_class_b_dev_t);
	class_destroy(pdata->avb_class_b_class);
	unregister_chrdev_region(pdata->avb_class_b_dev_t, 1);
	pdata->avb_class_b_cdev = NULL;
	pdata->avb_class_b_class = NULL;
	return 0;
}


