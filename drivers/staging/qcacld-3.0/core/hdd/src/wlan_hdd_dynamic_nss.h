/*
 * Copyright (c) 2014, 2017 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _WLAN_HDD_DYNAMIC_NSS_H
#define _WLAN_HDD_DYNAMIC_NSS_H
/**
 * DOC: wlan_hdd_dynamic_nss.h
 *
 * This module describes dynamic Nss strategy for power saving.
 */

#include "wlan_hdd_main.h"

/*
 * Preprocessor Definitions and Constants
 */

int wlan_hdd_set_nss_and_antenna_mode(struct hdd_adapter *adapter, int nss, int mode);
#endif /* #ifndef _WLAN_HDD_DYNAMIC_NSS_H */

