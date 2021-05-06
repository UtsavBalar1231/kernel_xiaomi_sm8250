#include <linux/memblock.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/pm_runtime.h>
#include <asm/unaligned.h>
#include "mem_interface.h"

static struct ufs_hba *hba;
static struct ufs_data *ufs_data_ptr;

void set_ufs_hba_data(struct scsi_device *sdev)
{
	if (!ufs_data_ptr) {
		ufs_data_ptr = kzalloc(sizeof(struct ufs_data), GFP_KERNEL);

		if (!ufs_data_ptr)
			return;

		ufs_data_ptr->sdev = sdev;
		ufs_data_ptr->hba = shost_priv(sdev->host);
		hba = shost_priv(sdev->host);
	}
}
EXPORT_SYMBOL(set_ufs_hba_data);

void send_ufs_hba_data(struct ufs_hba **mi_hba, struct scsi_device **mi_sdev)
{
	if (!ufs_data_ptr) {
		mi_ufs_log(UFS_LOG_ERR, "ufs_data is NULL.\n");
		return;
	}
	*mi_hba = ufs_data_ptr->hba;
	*mi_sdev = ufs_data_ptr->sdev;
}

/*obtain ddr size*/
u8 memblock_mem_size_in_gb(void)
{
	return (u8)((memblock_phys_mem_size() + memblock_reserved_size()) / 1024 / 1024 / 1024);
}
EXPORT_SYMBOL(memblock_mem_size_in_gb);

int ufshcd_read_desc_mi(struct ufs_hba *hba, enum desc_idn desc_id,
		int desc_index, void *buf, u32 size)
{
	int ret = 0;

	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_read_desc_param(hba, desc_id, desc_index, 0, buf, size);
	pm_runtime_put_sync(hba->dev);

	return ret;

}
EXPORT_SYMBOL(ufshcd_read_desc_mi);

int ufs_get_string_desc(void *buf, int size, enum device_desc_param pname, bool ascii_std)
{
	u8 index;
	int ret = 0;
	int desc_len = QUERY_DESC_MAX_SIZE;
	u8 *desc_buf;

	desc_buf = kzalloc(QUERY_DESC_MAX_SIZE, GFP_KERNEL);
	if (!desc_buf)
		return -ENOMEM;
	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_query_descriptor_retry(hba,
		UPIU_QUERY_OPCODE_READ_DESC, QUERY_DESC_IDN_DEVICE,
		0, 0, desc_buf, &desc_len);
	if (ret) {
		ret = -EINVAL;
		goto out;
	}
	index = desc_buf[pname];

	memset(desc_buf, 0, QUERY_DESC_MAX_SIZE);

	ret = ufshcd_read_string_desc(hba, index, desc_buf, desc_len, ascii_std);
	if (ret < 0)
		goto out;

	if (ascii_std)
		memcpy(buf, desc_buf + QUERY_DESC_HDR_SIZE, size);
	else
		memcpy(buf, desc_buf, size);
out:
	pm_runtime_put_sync(hba->dev);
	kfree(desc_buf);
	return ret;
}
EXPORT_SYMBOL(ufs_get_string_desc);

int ufs_read_desc_param(enum desc_idn desc_id, u8 desc_index, u8 param_offset,
			void *buf, u8 param_size)
{
	u8 desc_buf[8] = {0};
	int ret;

	if (param_size > 8)
		return -EINVAL;

	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_read_desc_param(hba, desc_id, desc_index,
				param_offset, desc_buf, param_size);
	pm_runtime_put_sync(hba->dev);

	if (ret)
		return -EINVAL;
	switch (param_size) {
	case 1:
		*(u8 *)buf = *desc_buf;
		break;
	case 2:
		*(u16 *)buf = get_unaligned_be16(desc_buf);
		break;
	case 4:
		*(u32 *)buf =  get_unaligned_be32(desc_buf);
		break;
	case 8:
		*(u64 *)buf = get_unaligned_be64(desc_buf);
		break;
	default:
		*(u8 *)buf = *desc_buf;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(ufs_read_desc_param);
