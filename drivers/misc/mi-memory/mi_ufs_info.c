
#include <linux/string.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <asm/unaligned.h>

#include <scsi/scsi.h>
#include "mi_memory_sysfs.h"
#include "mem_interface.h"

enum field_width {
	BYTE	= 1,
	WORD	= 2,
	DWORD   = 4,
};

struct desc_field_offset {
	char *name;
	int offset;
	enum field_width width_byte;
};

u16 get_ufs_id(void)
{
	u16 ufs_id = 0;

	ufs_read_desc_param(QUERY_DESC_IDN_DEVICE, 0, DEVICE_DESC_PARAM_MANF_ID, &ufs_id, 2);

	return ufs_id;
}

static ssize_t dump_health_desc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u32 value = 0;
	int count = 0, i = 0;
	int buff_len = 0;
	u8 *buff_desc = NULL;
	struct ufs_hba *hba = NULL;
	struct scsi_device *sdev = NULL;

	struct desc_field_offset health_desc_field_name[] = {
		{"bLength", HEALTH_DESC_PARAM_LEN, BYTE},
		{"bDescriptorType", HEALTH_DESC_PARAM_TYPE, BYTE},
		{"bPreEOLInfo",	HEALTH_DESC_PARAM_EOL_INFO, BYTE},
		{"bDeviceLifeTimeEstA", HEALTH_DESC_PARAM_LIFE_TIME_EST_A, BYTE},
		{"bDeviceLifeTimeEstB", HEALTH_DESC_PARAM_LIFE_TIME_EST_B, BYTE},
	};

	send_ufs_hba_data(&hba, &sdev);

	/*obtain the length of health report*/
	ufs_read_desc_param(QUERY_DESC_IDN_HEALTH, 0, HEALTH_DESC_PARAM_LEN, &buff_len, BYTE);

	buff_desc = kzalloc(buff_len, GFP_KERNEL);
	if (!buff_desc) {
		count += snprintf((buf + count), PAGE_SIZE, "get health info fail\n");
		return count;
	}

	ufshcd_read_desc_mi(hba, QUERY_DESC_IDN_HEALTH, 0, buff_desc, buff_len);


	for (i = 0; i < ARRAY_SIZE(health_desc_field_name); ++i) {
		void *ptr = NULL;
		struct desc_field_offset *tmp = NULL;

		tmp = &health_desc_field_name[i];
		ptr = buff_desc + tmp->offset;

		switch (tmp->width_byte) {
		case BYTE:
			value = *((u8 *)ptr);
			break;
		case WORD:
			value = *((u16 *)ptr);
			break;
		case DWORD:
			value = *((u32 *)ptr);
			break;
		default:
			count += snprintf((buf + count), PAGE_SIZE,
				"Device Descriptor[Byte offset 0x%x]: length of %s data is out of range.\n",
				tmp->offset, tmp->name);
			break;
		}

		count += snprintf((buf + count), PAGE_SIZE,
			"Device Descriptor[Byte offset 0x%x]: %s = 0x%x\n",
			tmp->offset, tmp->name, value);
	}

	return count;

}
static DEVICE_ATTR_RO(dump_health_desc);

static ssize_t dump_string_desc_serial_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u8 ser_number[128] = { 0 };
	int i = 0, count = 0;

	ufs_get_string_desc(&ser_number, sizeof(ser_number), DEVICE_DESC_PARAM_SN, SD_RAW);

	count += snprintf((buf + count), PAGE_SIZE, "serial:");

	for (i = 2; i <  ser_number[QUERY_DESC_LENGTH_OFFSET]; i += 2)
		count += snprintf((buf + count), PAGE_SIZE, "%02x%02x",
			ser_number[i], ser_number[i+1]);

	count += snprintf((buf + count), PAGE_SIZE, "\n");

	return count;
}
static DEVICE_ATTR_RO(dump_string_desc_serial);

static ssize_t dump_device_desc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i = 0, count = 0;
	u32 value = 0;
	u8 buff_len = 0;
	u8 *buff_desc = NULL;
	struct ufs_hba *hba = NULL;
	struct scsi_device *sdev = NULL;

	struct desc_field_offset device_desc_field_name[] = {
		{"bLength",			DEVICE_DESC_PARAM_LEN,			BYTE},
		{"bDescriptorType",		DEVICE_DESC_PARAM_TYPE,			BYTE},
		{"bDevice",			DEVICE_DESC_PARAM_DEVICE_TYPE,		BYTE},
		{"bDeviceClass",		DEVICE_DESC_PARAM_DEVICE_CLASS,		BYTE},
		{"bDeviceSubClass",		DEVICE_DESC_PARAM_DEVICE_SUB_CLASS,	BYTE},
		{"bProtocol",			DEVICE_DESC_PARAM_PRTCL,		BYTE},
		{"bNumberLU",			DEVICE_DESC_PARAM_NUM_LU,		BYTE},
		{"bNumberWLU",			DEVICE_DESC_PARAM_NUM_WLU,		BYTE},
		{"bBootEnable",			DEVICE_DESC_PARAM_BOOT_ENBL,		BYTE},
		{"bDescrAccessEn",		DEVICE_DESC_PARAM_DESC_ACCSS_ENBL,	BYTE},
		{"bInitPowerMode",		DEVICE_DESC_PARAM_INIT_PWR_MODE,	BYTE},
		{"bHighPriorityLUN",		DEVICE_DESC_PARAM_HIGH_PR_LUN,		BYTE},
		{"bSecureRemovalType",		DEVICE_DESC_PARAM_SEC_RMV_TYPE,		BYTE},
		{"bSecurityLU",			DEVICE_DESC_PARAM_SEC_LU,		BYTE},
		{"Reserved",			DEVICE_DESC_PARAM_BKOP_TERM_LT,		BYTE},
		{"bInitActiveICCLevel",		DEVICE_DESC_PARAM_ACTVE_ICC_LVL,	BYTE},
		{"wSpecVersion",		DEVICE_DESC_PARAM_SPEC_VER,		WORD},
		{"wManufactureDate",		DEVICE_DESC_PARAM_MANF_DATE,		WORD},
		{"iManufactureName",		DEVICE_DESC_PARAM_MANF_NAME,		BYTE},
		{"iProductName",		DEVICE_DESC_PARAM_PRDCT_NAME,		BYTE},
		{"iSerialNumber",		DEVICE_DESC_PARAM_SN,			BYTE},
		{"iOemID",			DEVICE_DESC_PARAM_OEM_ID,		BYTE},
		{"wManufactureID",		DEVICE_DESC_PARAM_MANF_ID,		WORD},
		{"bUD0BaseOffset",		DEVICE_DESC_PARAM_UD_OFFSET,		BYTE},
		{"bUDConfigPLength",		DEVICE_DESC_PARAM_UD_LEN,		BYTE},
		{"bDeviceRTTCap",		DEVICE_DESC_PARAM_RTT_CAP,		BYTE},
		{"wPeriodicRTCUpdate",		DEVICE_DESC_PARAM_FRQ_RTC,		WORD},
		{"bUFSFeaturesSupport",		DEVICE_DESC_PARAM_UFS_FEAT,		BYTE},
		{"bFFUTimeout",			DEVICE_DESC_PARAM_FFU_TMT,		BYTE},
		{"bQueueDepth",			DEVICE_DESC_PARAM_Q_DPTH,		BYTE},
		{"wDeviceVersion",		DEVICE_DESC_PARAM_DEV_VER,		WORD},
		{"bNumSecureWpArea",		DEVICE_DESC_PARAM_NUM_SEC_WPA,		BYTE},
		{"dPSAMaxDataSize",		DEVICE_DESC_PARAM_PSA_MAX_DATA,		DWORD},
		{"bPSAStateTimeout",		DEVICE_DESC_PARAM_PSA_TMT,		BYTE},
		{"iProductRevisionLevel",	DEVICE_DESC_PARAM_PRDCT_REV,		BYTE},
	};

	send_ufs_hba_data(&hba, &sdev);

	ufs_read_desc_param(QUERY_DESC_IDN_DEVICE, 0, DEVICE_DESC_PARAM_LEN, &buff_len, BYTE);

	buff_desc = kzalloc(buff_len, GFP_KERNEL);
	if (!buff_desc) {
		count += snprintf((buf + count), PAGE_SIZE, "get desc info fail\n");
		return count;
	}

	ufshcd_read_desc_mi(hba, QUERY_DESC_IDN_DEVICE, 0, buff_desc, buff_len);

	for (i = 0; i < ARRAY_SIZE(device_desc_field_name); ++i) {
		u8 *ptr = NULL;
		struct desc_field_offset *tmp = NULL;

		tmp = &device_desc_field_name[i];

		ptr = buff_desc + tmp->offset;

		switch (tmp->width_byte) {
		case BYTE:
				value = (u32)(*ptr);
				break;
		case WORD:
				value = (u32)get_unaligned_be16(ptr);
				break;
		case DWORD:
				value = (u32)get_unaligned_be32(ptr);
				break;
		default:
				value = (u32)(*ptr);
				break;
		}

		count += snprintf((buf + count), PAGE_SIZE, "Device Descriptor[Byte offset 0x%x]: %s = 0x%x\n",
					tmp->offset, tmp->name, value);
	}

	kfree(buff_desc);

	return count;
}
static DEVICE_ATTR_RO(dump_device_desc);

static ssize_t show_hba_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct ufs_hba *hba = NULL;
	struct scsi_device *sdev = NULL;
	uint32_t count = 0;

	send_ufs_hba_data(&hba, &sdev);

	count += snprintf((buf + count), PAGE_SIZE,
		"hba->outstanding_tasks = 0x%x\n", (u32)hba->outstanding_tasks);
	count += snprintf((buf + count), PAGE_SIZE,
		"hba->outstanding_reqs = 0x%x\n", (u32)hba->outstanding_reqs);

	count += snprintf((buf + count), PAGE_SIZE,
		"hba->capabilities = 0x%x\n", hba->capabilities);
	count += snprintf((buf + count), PAGE_SIZE,
		"hba->nutrs = %d\n", hba->nutrs);
	count += snprintf((buf + count), PAGE_SIZE,
		"hba->nutmrs = %d\n", hba->nutmrs);
	count += snprintf((buf + count), PAGE_SIZE,
		"hba->ufs_version = 0x%x\n", hba->ufs_version);
	count += snprintf((buf + count), PAGE_SIZE,
		"hba->irq = 0x%x\n", hba->irq);
	count += snprintf((buf + count), PAGE_SIZE,
		"hba->auto_bkops_enabled = %d\n", hba->auto_bkops_enabled);

	count += snprintf((buf + count), PAGE_SIZE,
		"hba->ufshcd_state = 0x%x\n", hba->ufshcd_state);
	count += snprintf((buf + count), PAGE_SIZE,
		"hba->clk_gating.state = 0x%x\n", hba->clk_gating.state);
	count += snprintf((buf + count), PAGE_SIZE,
		"hba->eh_flags = 0x%x\n", hba->eh_flags);
	count += snprintf((buf + count), PAGE_SIZE,
		"hba->intr_mask = 0x%x\n", hba->intr_mask);
	count += snprintf((buf + count), PAGE_SIZE,
		"hba->ee_ctrl_mask = 0x%x\n", hba->ee_ctrl_mask);

	/* HBA Errors */
	count += snprintf((buf + count), PAGE_SIZE,
		"hba->errors = 0x%x\n", hba->errors);
	count += snprintf((buf + count), PAGE_SIZE,
		"hba->uic_error = 0x%x\n", hba->uic_error);
	count += snprintf((buf + count), PAGE_SIZE,
		"hba->saved_err = 0x%x\n", hba->saved_err);
	count += snprintf((buf + count), PAGE_SIZE,
		"hba->saved_uic_err = 0x%x\n", hba->saved_uic_err);

	count += snprintf((buf + count), PAGE_SIZE,
		"hibern8_exit_cnt = %d\n", hba->ufs_stats.hibern8_exit_cnt);
	count += snprintf((buf + count), PAGE_SIZE,
		"pa_err_cnt_total = %d\n",
			hba->ufs_stats.pa_err_cnt_total);
	count += snprintf((buf + count), PAGE_SIZE,
		"pa_lane_0_err_cnt = %d\n",
			hba->ufs_stats.pa_err_cnt[UFS_EC_PA_LANE_0]);
	count += snprintf((buf + count), PAGE_SIZE,
		"pa_lane_1_err_cnt = %d\n",
			hba->ufs_stats.pa_err_cnt[UFS_EC_PA_LANE_1]);
	count += snprintf((buf + count), PAGE_SIZE,
		"pa_line_reset_err_cnt = %d\n",
			hba->ufs_stats.pa_err_cnt[UFS_EC_PA_LINE_RESET]);
	count += snprintf((buf + count), PAGE_SIZE,
		"dl_err_cnt_total = %d\n",
			hba->ufs_stats.dl_err_cnt_total);
	count += snprintf((buf + count), PAGE_SIZE,
		"dl_nac_received_err_cnt = %d\n",
			hba->ufs_stats.dl_err_cnt[UFS_EC_DL_NAC_RECEIVED]);
	count += snprintf((buf + count), PAGE_SIZE,
		"dl_tcx_replay_timer_expired_err_cnt = %d\n",
	hba->ufs_stats.dl_err_cnt[UFS_EC_DL_TCx_REPLAY_TIMER_EXPIRED]);
	count += snprintf((buf + count), PAGE_SIZE,
		"dl_afcx_request_timer_expired_err_cnt = %d\n",
	hba->ufs_stats.dl_err_cnt[UFS_EC_DL_AFCx_REQUEST_TIMER_EXPIRED]);
	count += snprintf((buf + count), PAGE_SIZE,
		"dl_fcx_protection_timer_expired_err_cnt = %d\n",
	hba->ufs_stats.dl_err_cnt[UFS_EC_DL_FCx_PROTECT_TIMER_EXPIRED]);
	count += snprintf((buf + count), PAGE_SIZE,
		"dl_crc_err_cnt = %d\n",
			hba->ufs_stats.dl_err_cnt[UFS_EC_DL_CRC_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE,
		"dll_rx_buffer_overflow_err_cnt = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_RX_BUFFER_OVERFLOW]);
	count += snprintf((buf + count), PAGE_SIZE,
		"dl_max_frame_length_exceeded_err_cnt = %d\n",
		hba->ufs_stats.dl_err_cnt[UFS_EC_DL_MAX_FRAME_LENGTH_EXCEEDED]);
	count += snprintf((buf + count), PAGE_SIZE,
		"dl_wrong_sequence_number_err_cnt = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_WRONG_SEQUENCE_NUMBER]);
	count += snprintf((buf + count), PAGE_SIZE,
		"dl_afc_frame_syntax_err_cnt = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_AFC_FRAME_SYNTAX_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE,
		"dl_nac_frame_syntax_err_cnt = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_NAC_FRAME_SYNTAX_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE,
		"dl_eof_syntax_err_cnt = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_EOF_SYNTAX_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE,
		"dl_frame_syntax_err_cnt = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_FRAME_SYNTAX_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE,
		"dl_bad_ctrl_symbol_type_err_cnt = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_BAD_CTRL_SYMBOL_TYPE]);
	count += snprintf((buf + count), PAGE_SIZE,
		"dl_pa_init_err_cnt = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_PA_INIT_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE,
		"dl_pa_error_ind_received = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_PA_ERROR_IND_RECEIVED]);
	count += snprintf((buf + count), PAGE_SIZE,
		"dme_err_cnt = %d\n", hba->ufs_stats.dme_err_cnt);

	return count;
}
static DEVICE_ATTR_RO(show_hba);

/**
 * get toshiba hr inquiry
 */
static int mi_scsi_hr_inquiry(struct scsi_device *sdev, char *hr_inq, int len)
{
	int result;
	unsigned char cmd[16] = {0};

	if (!hr_inq)
		return -EINVAL;

	cmd[0] = INQUIRY;
	cmd[1] = 0x69;		/* EVPD */
	cmd[2] = 0xC0;
	cmd[3] = len >> 8;
	cmd[4] = len & 0xff;
	cmd[5] = 0;		/* Control byte */

	result = scsi_execute_req(sdev, cmd, DMA_FROM_DEVICE, hr_inq,
				  len, NULL, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: get hr_inquiry result error\n");
		return -EIO;
	}

	/* Sanity check that we got the page back that we asked for */
	if (hr_inq[1] != 0xC0)
		pr_err("ufs: hr_inruiry data error\n");

	return 0;
}

/**
 * get sandisk device report
 */
static int mi_scsi_sdr(struct scsi_device *sdev, char *sdr, int len)
{
	int result;
	unsigned char cmd[16] = {0};

	if (!sdr)
		return -EINVAL;

	cmd[0] = READ_BUFFER;
	cmd[1] = 0x01;		/* mode vendor specific*/
	cmd[2] = 0x01;		/* buffer ID*/

	cmd[3] = 0x7D;
	cmd[4] = 0x9C;
	cmd[5] = 0x69;

	cmd[6] = 0x00;
	cmd[7] = 0x02;
	cmd[8] = 0x00;

	cmd[9] = 0;		/* Control byte */

	result = scsi_execute_req(sdev, cmd, DMA_FROM_DEVICE, sdr,
				  len, NULL, 30 * HZ, 3, NULL);

	if (result) {
		pr_err("ufs: get sdr result error\n");
		return -EIO;
	}

	return 0;
}

/**
 * get micron hr
 */
static int mi_scsi_mhr(struct scsi_device *sdev, char *hr, int len)
{
	int result;
	unsigned char write_buffer[16] = {
		0x3B, 0xE1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2C, 0x00};
	unsigned char read_buffer[16] = {
		0x3C, 0xC1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00};

	char VU[0x2c] = {0};

	VU[0] = 0xFE;
	VU[1] = 0x40;
	VU[3] = 0x10;
	VU[4] = 0x01;

	if (!hr)
		return -EINVAL;

	result = scsi_execute_req(sdev, write_buffer, DMA_TO_DEVICE, VU,
				  0x2c, NULL, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: hr write buffer  error\n");
		return -EIO;
	}
	result = scsi_execute_req(sdev, read_buffer, DMA_FROM_DEVICE, hr,
				  len, NULL, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: hr read buffer  error\n");
		return -EIO;
	}

	return 0;
}

/**
 * get samsung osv
 */
static int mi_scsi_osv(struct scsi_device *sdev, char *osv, int len)
{
	int result;
	unsigned char cmd[16] = {0};

	if (!osv)
		return -EINVAL;

	cmd[0] = 0xc0; /*VENDOR_SPECIFIC_CDB;*/
	cmd[1] = 0x40;

	cmd[4] = 0x01;
	cmd[5] = 0x0c;

	cmd[15] = 0x1c;

	result = scsi_execute(sdev, cmd, DMA_FROM_DEVICE, osv,
				  len, NULL, NULL, 30 * HZ, 3, 0, RQF_PM, NULL);

	if (result) {
		pr_err("ufs: get osv result error\n");
		return -EIO;
	}

	return 0;
}

static int scsi_ss_set_pwd(struct scsi_device *sdev)
{
	int result;
	unsigned char cmd[16] = {0};

	cmd[0] = 0xc0; /*VENDOR_SPECIFIC_CDB;*/
	cmd[1] = 0x03;

	/*password*/
	cmd[2] = 'g';
	cmd[3] = 'h';
	cmd[4] = 'r';
	cmd[5] = 0;

	result = scsi_execute_req(sdev, cmd, DMA_NONE, 0,
				  0, NULL, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: scsi_ss_set_pwd error\n");
	}
	return result;
}


static int scsi_ss_enter_vendor_mode(struct scsi_device *sdev)
{
	int result;
	unsigned char cmd[16] = {0};

	cmd[0] = 0xc0; /*VENDOR_SPECIFIC_CDB;*/
	cmd[1] = 0;

	cmd[2] = 0x5C;
	cmd[3] = 0x38;
	cmd[4] = 0x23;
	cmd[5] = 0xAE;

	/*password*/
	cmd[6] = 'g';
	cmd[7] = 'h';
	cmd[8] = 'r';
	cmd[9] = 0;

	result = scsi_execute_req(sdev, cmd, DMA_NONE, 0,
				  0, NULL, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: scsi_ss_enter_vendor_mode error\n");
	}
	return result;
}

static int scsi_ss_exit_vendor_mode(struct scsi_device *sdev)
{
	int result;
	unsigned char cmd[16] = {0};

	cmd[0] = 0xc0; /*VENDOR_SPECIFIC_CDB;*/
	cmd[1] = 0x01;

	result = scsi_execute_req(sdev, cmd, DMA_NONE, 0,
				  0, NULL, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: scsi_ss_enter_vendor_mode error\n");
	}
	return result;
}


static int scsi_ss_nandinfo(struct scsi_device *sdev, char *osv, int len)
{
	int result;
	unsigned char cmd[16] = {0};

	if (!osv)
		return -EINVAL;

	cmd[0] = 0xc0; /*VENDOR_SPECIFIC_CDB;*/
	cmd[1] = 0x40;

	cmd[4] = 0x01;
	cmd[5] = 0x0A;

	cmd[15] = 0x4C;

	len = 0x4C;
	result = scsi_execute_req(sdev, cmd, DMA_FROM_DEVICE, osv,
				  len, NULL, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: get osv result error\n");
		return -EIO;
	}

	return 0;
}

static int scsi_ss_hr(struct scsi_device *sdev, char *osv, int len)
{
	int result = 0;

	result = scsi_ss_enter_vendor_mode(sdev);
	if (result) {
		pr_err("ufs: enter vendor mode fail, program key and try again\n");

		result = scsi_ss_set_pwd(sdev);
		if (result) {
			pr_err("ufs: set pwd fail\n");
			goto out;
		} else {
			result = scsi_ss_enter_vendor_mode(sdev);
			if (result) {
				pr_err("ufs: enter vendor mode fail\n");
				goto out;
			}
		}
	}

	result = scsi_ss_nandinfo(sdev, osv, len);
	if (result) {
		pr_err("ufs: ger hr fail fail\n");
	}

	result = scsi_ss_exit_vendor_mode(sdev);
	if (result) {
		pr_err("ufs: exit vendor mode fail\n");
	}

out:
	return result;
}


static int ufs_get_hynix_hr(struct ufs_hba *hba, u8 *buf, u32 size)
{
	size = QUERY_DESC_HEALTH_DEF_SIZE;
	return ufshcd_read_desc_mi(hba, QUERY_DESC_IDN_HEALTH, 0, buf, size);
}

static ssize_t hr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int err = 0, i = 0;
	int len = 512; /*0x200*/
	char *hr;
	struct ufs_hba *hba = NULL;
	uint32_t count = 0;
	struct scsi_device *sdev;

	send_ufs_hba_data(&hba, &sdev);

	hr =  kzalloc(len, GFP_KERNEL);
	if (!hr) {
		pr_err("kzalloc fail\n");
		return -ENOMEM;
	}

	if (!strncmp(sdev->vendor, "WDC", 3)) {
		err = mi_scsi_sdr(sdev, hr, len);
	} else if (!strncmp(sdev->vendor, "TOSHIBA", 7)) {
		err = mi_scsi_hr_inquiry(sdev, hr, len);
	} else if (!strncmp(sdev->vendor, "SAMSUNG", 7)) {
		err = mi_scsi_osv(sdev, hr, 0x1c);/*0x200 is the same with 0x1c*/
	} else if (!strncmp(sdev->vendor, "MICRON", 6)) {
		err = mi_scsi_mhr(sdev, hr, len);
	} else if (!strncmp(sdev->vendor, "SKhynix", 7)) {
		err = ufs_get_hynix_hr(hba, hr, len);
	} else {
		count += snprintf((buf + count),  PAGE_SIZE, "NOT SUPPORTED %s\n", sdev->vendor);
		goto out;
	}

	if (err) {
		count += snprintf((buf + count),  PAGE_SIZE, "Fail to get hr, err is: %d\n", err);
	} else {
		for (i = 0; i < len; i++)
			count += snprintf((buf + count), PAGE_SIZE, "%02x", hr[i]);
		count += snprintf((buf + count), PAGE_SIZE, "\n");
	}

	if (!strncmp(sdev->vendor, "SAMSUNG", 7)) {
		memset(hr, 0, 512);
		err = scsi_ss_hr(sdev, hr, 0x4c);
		if (!err) {
			for (i = 0; i < 0x4c; i++)
				snprintf(buf + 128*2 + i*2, PAGE_SIZE, "%02x", hr[i]);
		}
	}


out:
	kfree(hr);
	return count;
}

static DEVICE_ATTR_RO(hr);

#define BUFF_LINE_SIZE 16 /* Must be a multiplication of sizeof(u32) */
#define TAB_CHARS 8
/*
 * tags status about tags_stats
 */
static ssize_t tag_stats_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = NULL;
	struct scsi_device *sdev = NULL;
	struct ufs_stats *ufs_stats;
	int i, j;
	int max_depth;
	bool is_tag_empty = true;
	unsigned long flags;
	uint32_t count = 0;
	char *sep = " | * | ";


	send_ufs_hba_data(&hba, &sdev);
	if (!hba)
		goto exit;

	ufs_stats = &hba->ufs_stats;

	if (!ufs_stats->enabled) {
		count += snprintf((buf + count), PAGE_SIZE, "ufs statistics are disabled\n");
		goto exit;
	}

	max_depth = hba->nutrs;

	spin_lock_irqsave(hba->host->host_lock, flags);
	/* Header */
	count += snprintf((buf + count), PAGE_SIZE,
	" Tag Stat\t\t%s Number of pending reqs upon issue (Q fullness)\n",
	sep);
	for (i = 0; i < TAB_CHARS * (TS_NUM_STATS + 4); i++) {
		count += snprintf((buf + count), PAGE_SIZE, "-");
		if (i == (TAB_CHARS * 3 - 1))
			count += snprintf((buf + count), PAGE_SIZE, "%s", sep);
	}
	count += snprintf((buf + count), PAGE_SIZE,
	"\n #\tnum uses\t%s\t #\tAll\tRead\tWrite\tUrg.R\tUrg.W\tFlush\n", sep);

	/* values */
	for (i = 0; i < max_depth; i++) {
		if (ufs_stats->tag_stats[i][TS_TAG] <= 0 &&
				ufs_stats->tag_stats[i][TS_READ] <= 0 &&
				ufs_stats->tag_stats[i][TS_WRITE] <= 0 &&
				ufs_stats->tag_stats[i][TS_URGENT_READ] <= 0 &&
				ufs_stats->tag_stats[i][TS_URGENT_WRITE] <= 0 &&
				ufs_stats->tag_stats[i][TS_FLUSH] <= 0)
			continue;

		is_tag_empty = false;
		count += snprintf((buf + count), PAGE_SIZE, " %d\t ", i);
		for (j = 0; j < TS_NUM_STATS; j++) {
			count += snprintf((buf + count), PAGE_SIZE, "%llu\t",
				ufs_stats->tag_stats[i][j]);
			if (j != 0)
				continue;
			count += snprintf((buf + count), PAGE_SIZE, "\t%s\t %d\t%llu\t", sep, i,
				ufs_stats->tag_stats[i][TS_READ] +
				ufs_stats->tag_stats[i][TS_WRITE] +
				ufs_stats->tag_stats[i][TS_URGENT_READ] +
				ufs_stats->tag_stats[i][TS_URGENT_WRITE] +
				ufs_stats->tag_stats[i][TS_FLUSH]);
		}
		count += snprintf((buf + count), PAGE_SIZE, "\n");
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (is_tag_empty)
		count += snprintf((buf + count), PAGE_SIZE,
			"%s: All tags statistics are empty\n", __func__);
exit:
	return count;
}

static ssize_t tag_stats_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{

	struct ufs_stats *ufs_stats = NULL;
	struct ufs_hba *hba = NULL;
	struct scsi_device *sdev = NULL;
	unsigned long val = 0;
	int ret = -1, bit = 0;
	unsigned long flags;

	send_ufs_hba_data(&hba, &sdev);
	if (!hba)
		return ret;

	ufs_stats = &hba->ufs_stats;

	ret = kstrtoul(buf, 0, &val);
	if (ret) {
		dev_err(hba->dev, "%s: Invalid argument\n", __func__);
		return ret;
	}

	spin_lock_irqsave(hba->host->host_lock, flags);

	if (val == 0) {
		ufs_stats->enabled = false;
		dev_info(hba->dev, "%s: Disabling UFS tag statistics\n", __func__);
	} else if (val == 1) {
		ufs_stats->enabled = true;
		dev_info(hba->dev, "%s: Enabling & Resetting UFS tag statistics\n", __func__);
		memset(hba->ufs_stats.tag_stats[0], 0, sizeof(**hba->ufs_stats.tag_stats) * TS_NUM_STATS * hba->nutrs);

		/* initialize current queue depth */
		ufs_stats->q_depth = 0;
		for_each_set_bit_from(bit, &hba->outstanding_reqs, hba->nutrs)
			ufs_stats->q_depth++;
		dev_info(hba->dev, "%s: Enabled UFS tag statistics\n", __func__);
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return count;
}
static DEVICE_ATTR_RW(tag_stats);

/*
 * export err_occurred information
 */
static ssize_t err_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = NULL;
	struct scsi_device *sdev = NULL;
	int count = 0;

	send_ufs_hba_data(&hba, &sdev);

	count += snprintf(buf + count, PAGE_SIZE, "%d\n", hba->debugfs_files.err_occurred);

	return count;
}
static DEVICE_ATTR_RO(err_state);

static ssize_t err_reason_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = NULL;
	struct scsi_device *sdev = NULL;
	int count = 0;
	int i = 0;

	send_ufs_hba_data(&hba, &sdev);

	while (i < UFS_ERR_MAX)
		count += snprintf(buf + count, PAGE_SIZE, "%d ", hba->ufs_stats.err_stats[i++]);
	count += snprintf(buf + count, PAGE_SIZE, "\n");

	return count;
}
static DEVICE_ATTR_RO(err_reason);

static ssize_t req_stats_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int i;
	unsigned long flags;
	struct ufs_hba *hba = NULL;
	struct scsi_device *sdev = NULL;
	struct ufs_stats *ufs_stats = NULL;
	int count = 0;

	send_ufs_hba_data(&hba, &sdev);
	if (!hba)
		goto exit;

	ufs_stats = &hba->ufs_stats;
	if (!ufs_stats->req_stats_enabled) {
		count += snprintf((buf + count), PAGE_SIZE, "req stats disabled.\n");
		return count;
	}

	/* Header */
	count += snprintf((buf + count), PAGE_SIZE,
		"\t%-10s %-10s %-10s %-10s %-10s %-10s",
		"All", "Write", "Read", "Read(urg)", "Write(urg)", "Flush");

	spin_lock_irqsave(hba->host->host_lock, flags);

	count += snprintf((buf + count), PAGE_SIZE, "\n%s:\t", "Min");

	for (i = 0; i < TS_NUM_STATS; i++)
		count += snprintf((buf + count), PAGE_SIZE, "%-10llu ",
		hba->ufs_stats.req_stats[i].min);

	count += snprintf((buf + count), PAGE_SIZE, "\n%s:\t", "Max");

	for (i = 0; i < TS_NUM_STATS; i++)
		count += snprintf((buf + count), PAGE_SIZE, "%-10llu ",
		hba->ufs_stats.req_stats[i].max);

	count += snprintf((buf + count), PAGE_SIZE, "\n%s:\t", "Avg.");

	for (i = 0; i < TS_NUM_STATS; i++)
		count += snprintf((buf + count), PAGE_SIZE, "%-10llu ",
			div64_u64(hba->ufs_stats.req_stats[i].sum,
				hba->ufs_stats.req_stats[i].count));
	count += snprintf((buf + count), PAGE_SIZE, "\n%s:\t", "Count");

	for (i = 0; i < TS_NUM_STATS; i++)
		count += snprintf((buf + count), PAGE_SIZE, "%-10llu ",
		hba->ufs_stats.req_stats[i].count);
	count += snprintf((buf + count), PAGE_SIZE, "\n");

	spin_unlock_irqrestore(hba->host->host_lock, flags);

exit:
	return count;
}

static ssize_t req_stats_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{

	struct ufs_stats *ufs_stats = NULL;
	struct ufs_hba *hba = NULL;
	struct scsi_device *sdev = NULL;
	unsigned long val = 0;
	int ret = -1;
	unsigned long flags;

	send_ufs_hba_data(&hba, &sdev);
	if (!hba)
		return ret;

	ufs_stats = &hba->ufs_stats;

	ret = kstrtoul(buf, 0, &val);
	if (ret) {
		dev_err(hba->dev, "%s: Invalid argument\n", __func__);
		return ret;
	}

	spin_lock_irqsave(hba->host->host_lock, flags);

	if (val == 0) {
		ufs_stats->req_stats_enabled = false;
		dev_info(hba->dev, "%s: Disabling UFS req stats", __func__);
	} else if (val == 1) {
		dev_info(hba->dev, "%s: Enabling & Resetting UFS req stats\n", __func__);
		memset(&(hba->ufs_stats.req_stats[0]), 0, sizeof(struct ufshcd_req_stat) * TS_NUM_STATS);
		ufs_stats->req_stats_enabled = true;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return count;
}
static DEVICE_ATTR_RW(req_stats);

static struct attribute *ufshcd_sysfs[] = {
	&dev_attr_dump_health_desc.attr,
	&dev_attr_dump_string_desc_serial.attr,
	&dev_attr_dump_device_desc.attr,
	&dev_attr_show_hba.attr,
	&dev_attr_hr.attr,
	&dev_attr_tag_stats.attr,
	&dev_attr_err_state.attr,
	&dev_attr_err_reason.attr,
	&dev_attr_req_stats.attr,
	NULL,
};

const struct attribute_group ufshcd_sysfs_group = {
	.name = "ufshcd0",
	.attrs = ufshcd_sysfs,
};
