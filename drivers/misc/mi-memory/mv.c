#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel_stat.h>
#include <linux/cpu.h>
#include <linux/memblock.h>
#include <linux/byteorder/generic.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include "mv.h"
#include "mi_memory_sysfs.h"
#include "mem_interface.h"

struct mv_ufs mvufs;
int ddr_id;

static void mv_ufs_init(void)
{
	u64 raw_device_capacity = 0;
	u16 ufs_id = 0;

	/*sizeof-1 only to string can end of \0*/
	ufs_get_string_desc(mvufs.product_name, sizeof(mvufs.product_name) - 1,
		DEVICE_DESC_PARAM_PRDCT_NAME, SD_ASCII_STD);
	ufs_get_string_desc(mvufs.product_revision,
			sizeof(mvufs.product_revision) - 1,
			DEVICE_DESC_PARAM_PRDCT_REV, SD_ASCII_STD);

	ufs_read_desc_param(QUERY_DESC_IDN_DEVICE, 0,
		DEVICE_DESC_PARAM_MANF_ID, &ufs_id, 2);

	mvufs.vendor_id = ufs_id;

	ufs_read_desc_param(QUERY_DESC_IDN_GEOMETRY, 0,
		GEOMETRY_DESC_PARAM_DEV_CAP, &raw_device_capacity, 8);
	raw_device_capacity = (raw_device_capacity * 512) / 1024 / 1024 / 1024;
	if (raw_device_capacity > 512 && raw_device_capacity <= 1024) {
		mvufs.density = 1024;
	} else if (raw_device_capacity > 256) {
		mvufs.density = 512;
	} else if (raw_device_capacity > 128) {
		mvufs.density = 256;
	} else if (raw_device_capacity > 64) {
		mvufs.density = 128;
	} else if (raw_device_capacity > 32) {
		mvufs.density = 64;
	} else if (raw_device_capacity > 16) {
		mvufs.density = 32;
	} else if (raw_device_capacity > 8) {
		mvufs.density = 8;
	} else {
		mvufs.density = 0;
		pr_info("mv unkonwn ufs size %d\n", raw_device_capacity);
	}
}

static void mv_get_ddr(void)
{
	int ddrid = 0;

	switch (mvufs.vendor_id) {
	case 0x01ce:/*for samsung*/
		ddrid = 0x01;
		break;
	case 0x01ad:/*for hynix*/
		ddrid = 0x06;
		break;
	case 0x012c:/*for micron*/
		ddrid = 0xff;
		break;
	case  0x0145:/*for sandisk*/
		ddrid = 0xfe;/*0xfe is not really true, only for distinguish with hynix*/
		break;
	default:
		pr_err("this device not support.\n");
		break;
	}

	ddr_id = ddrid;
}

static int mv_proc_show(struct seq_file *m, void *v)
{
	mv_ufs_init();
	mv_get_ddr();

	seq_printf(m, "D: 0x%02x %d\n", ddr_id, get_ddr_size());
	seq_printf(m, "U: 0x%04x %d %s %s\n", mvufs.vendor_id, mvufs.density,
		mvufs.product_name, mvufs.product_revision);

	return 0;
}

static int mv_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mv_proc_show, NULL);
}

static const struct file_operations mv_proc_fops = {
	.open		= mv_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int add_proc_mv_node(void)
{
	proc_create(MV_NAME, 0555, NULL, &mv_proc_fops);
	return 0;
}

int remove_proc_mv_node(void)
{
	remove_proc_entry(MV_NAME, NULL);
	return 0;
}
