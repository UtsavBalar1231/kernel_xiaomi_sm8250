/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * RMNET Data Smart Hash solution
 *
 */

#ifndef _RMNET_SHS_FREQ_H_
#define _RMNET_SHS_FREQ_H_

int rmnet_shs_freq_init(void);
int rmnet_shs_freq_exit(void);
void rmnet_shs_boost_cpus(void);
void rmnet_shs_reset_cpus(void);

#endif
