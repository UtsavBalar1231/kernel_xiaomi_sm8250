#ifndef __MEM_INTERFACE_H__
#define __MEM_INTERFACE_H__

#include "../../scsi/ufs/ufshcd.h"

#define SD_ASCII_STD true
#define SD_RAW false

enum {
	UFS_LOG_ERR,
	UFS_LOG_INFO,
	UFS_LOG_DEBUG,
};

#define mi_ufs_log(lvl, fmt, ...) 						\
{										\
	do {									\
		if (lvl == UFS_LOG_ERR)						\
			printk(KERN_ERR "[MI-MEMORY]:"fmt, ##__VA_ARGS__);	\
		else if (lvl == UFS_LOG_INFO)					\
			printk(KERN_INFO "[MI-MEMORY]:"fmt, ##__VA_ARGS__);	\
	} while (0);								\
}

struct ufs_data {
	struct ufs_hba *hba;
	struct scsi_device *sdev;
};

u8 memblock_mem_size_in_gb(void);

void set_ufs_hba_data(struct scsi_device *sdev);
void send_ufs_hba_data(struct ufs_hba **mi_hba, struct scsi_device **sdev);

int ufs_get_string_desc(void *buf, int size, enum device_desc_param pname,
	bool ascii_std);

int ufs_read_desc_param(enum desc_idn desc_id, u8 desc_index, u8 param_offset,
		void *buf, u8 param_size);
int ufshcd_read_desc_mi(struct ufs_hba *hba, enum desc_idn desc_id,
		int desc_index, void *buf, u32 size);
extern int ufshcd_read_desc_param(struct ufs_hba *hba, enum desc_idn desc_id,
	int desc_index, u8 param_offset, u8 *param_read_buf, u8 param_size);
extern int ufshcd_read_string_desc(struct ufs_hba *hba, int desc_index,
		u8 *buf, u32 size, bool ascii);
#endif
