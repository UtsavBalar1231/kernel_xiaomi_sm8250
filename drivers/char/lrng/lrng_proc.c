// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * LRNG proc interfaces
 *
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
 */

#include <linux/lrng.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "lrng_drng_mgr.h"
#include "lrng_es_aux.h"
#include "lrng_es_mgr.h"
#include "lrng_proc.h"

/* Number of online DRNGs */
static u32 numa_drngs = 1;

void lrng_pool_inc_numa_node(void)
{
	numa_drngs++;
}

static int lrng_proc_type_show(struct seq_file *m, void *v)
{
	struct lrng_drng *lrng_drng_init = lrng_drng_init_instance();
	unsigned char buf[250];
	u32 i;

	mutex_lock(&lrng_drng_init->lock);
	snprintf(buf, sizeof(buf),
		 "DRNG name: %s\n"
		 "LRNG security strength in bits: %d\n"
		 "Number of DRNG instances: %u\n"
		 "Standards compliance: %s\n"
		 "LRNG minimally seeded: %s\n"
		 "LRNG fully seeded: %s\n",
		 lrng_drng_init->drng_cb->drng_name(),
		 lrng_security_strength(),
		 numa_drngs,
		 lrng_sp80090c_compliant() ? "SP800-90C " : "",
		 lrng_state_min_seeded() ? "true" : "false",
		 lrng_state_fully_seeded() ? "true" : "false");
	seq_write(m, buf, strlen(buf));

	for_each_lrng_es(i) {
		snprintf(buf, sizeof(buf),
			 "Entropy Source %u properties:\n"
			 " Name: %s\n",
			 i, lrng_es[i]->name);
		seq_write(m, buf, strlen(buf));

		buf[0] = '\0';
		lrng_es[i]->state(buf, sizeof(buf));
		seq_write(m, buf, strlen(buf));
	}

	mutex_unlock(&lrng_drng_init->lock);

	return 0;
}

static int __init lrng_proc_type_init(void)
{
	proc_create_single("lrng_type", 0444, NULL, &lrng_proc_type_show);
	return 0;
}

module_init(lrng_proc_type_init);
