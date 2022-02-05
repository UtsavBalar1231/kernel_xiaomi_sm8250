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
 * RMNET Data Smart Hash stamping solution
 *
 */
#include <linux/module.h>
#include "rmnet_shs.h"
#include "rmnet_shs_freq.h"

#include <linux/cpufreq.h>
#include <linux/cpu.h>

#define MAX_FREQ INT_MAX
#define MIN_FREQ 0
#define BOOST_FREQ MAX_FREQ

struct cpu_freq {
	unsigned int freq_floor;
	unsigned int freq_ceil;

};

unsigned int rmnet_shs_freq_enable __read_mostly = 1;
module_param(rmnet_shs_freq_enable, uint, 0644);
MODULE_PARM_DESC(rmnet_shs_freq_enable, "Enable/disable freq boost feature");

struct workqueue_struct *shs_boost_wq;
static DEFINE_PER_CPU(struct cpu_freq, cpu_boosts);
static struct work_struct boost_cpu;

static int rmnet_shs_freq_notify(struct notifier_block *nb,
				 unsigned long val,
				 void *data)
{
	struct cpufreq_policy *policy = data;
	unsigned int cpu = policy->cpu;
	struct cpu_freq *boost = &per_cpu(cpu_boosts, cpu);

	switch (val) {
	case CPUFREQ_ADJUST:
		if (rmnet_shs_freq_enable) {
			cpufreq_verify_within_limits(policy,
						     boost->freq_floor,
						     MAX_FREQ);
			trace_rmnet_freq_update(cpu, policy->min,
						policy->max);
		}
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block freq_boost_nb = {
	.notifier_call = rmnet_shs_freq_notify,
};

static void update_cpu_policy(struct work_struct *work)
{
	unsigned int i;

	get_online_cpus();
	for_each_online_cpu(i) {
		cpufreq_update_policy(i);
	}

	put_online_cpus();
}

void rmnet_shs_reset_freq(void)
{
	struct cpu_freq *boost;
	int i;

	for_each_possible_cpu(i) {
		boost = &per_cpu(cpu_boosts, i);
		boost->freq_floor = MIN_FREQ;
		boost->freq_ceil = MAX_FREQ;
	}
}

void rmnet_shs_boost_cpus(void)
{
	struct cpu_freq *boost;
	int i;

	for_each_possible_cpu(i) {

		if ((1 << i) & PERF_MASK)
			continue;
		boost = &per_cpu(cpu_boosts, i);
		boost->freq_floor = BOOST_FREQ;
		boost->freq_ceil = MAX_FREQ;
		trace_rmnet_freq_boost(i, boost->freq_floor);
	}

	if (work_pending(&boost_cpu))
		return;

	if (shs_boost_wq)
		queue_work(shs_boost_wq, &boost_cpu);
}

void rmnet_shs_reset_cpus(void)
{
	struct cpu_freq *boost;
	int i;

	for_each_possible_cpu(i) {

		if ((1 << i) & PERF_MASK)
			continue;
		boost = &per_cpu(cpu_boosts, i);
		boost->freq_floor = MIN_FREQ;
		boost->freq_ceil = MAX_FREQ;
		trace_rmnet_freq_reset(i, boost->freq_floor);
	}
	if (work_pending(&boost_cpu))
		return;

	if (shs_boost_wq)
		queue_work(shs_boost_wq, &boost_cpu);
}

int rmnet_shs_freq_init(void)
{

	if (!shs_boost_wq)
		shs_boost_wq = alloc_workqueue("shs_boost_wq", WQ_HIGHPRI, 0);

	if (!shs_boost_wq)
		return -EFAULT;
	INIT_WORK(&boost_cpu, update_cpu_policy);

	if (rmnet_shs_freq_enable)
		cpufreq_register_notifier(&freq_boost_nb,
					  CPUFREQ_POLICY_NOTIFIER);
	rmnet_shs_reset_freq();
	return 0;
}

int rmnet_shs_freq_exit(void)
{
	rmnet_shs_reset_freq();
	cancel_work_sync(&boost_cpu);

	if (shs_boost_wq) {
		destroy_workqueue(shs_boost_wq);
		shs_boost_wq = NULL;
	}

	if (rmnet_shs_freq_enable)
		cpufreq_unregister_notifier(&freq_boost_nb,
					    CPUFREQ_POLICY_NOTIFIER);
	return 0;
}
