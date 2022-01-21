#include <linux/string.h>
#include "wlan_hdd_dynamic_nss.h"
#include "wlan_hdd_ioctl.h"


int wlan_hdd_set_nss_and_antenna_mode(struct hdd_adapter *adapter, int nss, int mode)
{
	QDF_STATUS status;

	hdd_debug("NSS = %d, antenna mode = %d", nss, mode);

	if ((nss > 2) || (nss <= 0)) {
		hdd_err("Invalid NSS: %d", nss);
		return -EINVAL;
	}

	if ((mode > HDD_ANTENNA_MODE_2X2) || (mode < HDD_ANTENNA_MODE_1X1)) {
		hdd_err("Invalid antenna mode: %d", mode);
		return -EINVAL;
	}

	if (((nss == NSS_1x1_MODE) && (mode == HDD_ANTENNA_MODE_2X2))
		|| ((nss == NSS_2x2_MODE) && (mode == HDD_ANTENNA_MODE_1X1))) {
		hdd_err("nss and antenna mode is not match: nss = %d, mode = %d", nss, mode);
		return -EINVAL;
	}

	//iwpriv wlan0 nss 1/2
	status = hdd_update_nss(adapter, nss, nss);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("cfg set nss failed, value %d status %d", nss, status);
		return qdf_status_to_os_return(status);
	}

	//wpa_cli -i wlan0 driver SET ANTENNAMODE 1/2
	status = hdd_set_antenna_mode(adapter, adapter->hdd_ctx, mode);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("cfg set antenna mode failed, value %d status %d", mode, status);
		return qdf_status_to_os_return(status);
	}

	hdd_exit();

	return 0;
}
