#include <linux/mm.h>
#include <linux/mm_event.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>

#define CREATE_TRACE_POINTS
#include <trace/events/mm_event.h>
/* msec */
static unsigned long period_ms __read_mostly = 500;
static unsigned long vmstat_period_ms __read_mostly = 1000;
static unsigned long vmstat_next_period;

static DEFINE_SPINLOCK(vmstat_lock);
static DEFINE_RWLOCK(period_lock);

void mm_event_task_init(struct task_struct *tsk)
{
	memset(tsk->mm_event, 0, sizeof(tsk->mm_event));
	tsk->next_period = 0;
}

static void record_vmstat(void)
{
	int cpu;
	struct mm_event_vmstat vmstat;

	if (time_is_after_jiffies(vmstat_next_period))
		return;

	/* Need double check under the lock */
	spin_lock(&vmstat_lock);
	if (time_is_after_jiffies(vmstat_next_period)) {
		spin_unlock(&vmstat_lock);
		return;
	}
	vmstat_next_period = jiffies + msecs_to_jiffies(vmstat_period_ms);
	spin_unlock(&vmstat_lock);

	memset(&vmstat, 0, sizeof(vmstat));
	vmstat.free = global_zone_page_state(NR_FREE_PAGES);
	vmstat.slab = global_node_page_state(NR_SLAB_RECLAIMABLE) +
			global_node_page_state(NR_SLAB_UNRECLAIMABLE);

	vmstat.file = global_node_page_state(NR_ACTIVE_FILE) +
			global_node_page_state(NR_INACTIVE_FILE);
	vmstat.anon = global_node_page_state(NR_ACTIVE_ANON) +
			global_node_page_state(NR_INACTIVE_ANON);
	vmstat.ion = global_node_page_state(NR_ION_HEAP);

	vmstat.ws_refault = global_node_page_state(WORKINGSET_REFAULT);
	vmstat.ws_activate = global_node_page_state(WORKINGSET_ACTIVATE);
	vmstat.mapped = global_node_page_state(NR_FILE_MAPPED);

	for_each_online_cpu(cpu) {
		struct vm_event_state *this = &per_cpu(vm_event_states, cpu);

		/* sectors to kbytes for PGPGIN/PGPGOUT */
		vmstat.pgin += this->event[PGPGIN] / 2;
		vmstat.pgout += this->event[PGPGOUT] / 2;
		vmstat.swpin += this->event[PSWPIN];
		vmstat.swpout += this->event[PSWPOUT];
		vmstat.reclaim_steal += this->event[PGSTEAL_DIRECT] +
					this->event[PGSTEAL_KSWAPD];
		vmstat.reclaim_scan += this->event[PGSCAN_DIRECT] +
					this->event[PGSCAN_KSWAPD];
		vmstat.compact_scan += this->event[COMPACTFREE_SCANNED] +
					this->event[COMPACTMIGRATE_SCANNED];
	}
	trace_mm_event_vmstat_record(&vmstat);
}

static void record_stat(void)
{
	int i;
	bool need_vmstat = false;

	if (time_is_after_jiffies(current->next_period))
		return;

	read_lock(&period_lock);
	current->next_period = jiffies + msecs_to_jiffies(period_ms);
	read_unlock(&period_lock);

	for (i = 0; i < MM_TYPE_NUM; i++) {
		if (current->mm_event[i].count == 0)
			continue;
		if (i == MM_COMPACTION || i == MM_RECLAIM)
			need_vmstat = true;
		trace_mm_event_record(i, &current->mm_event[i]);
		memset(&current->mm_event[i], 0,
				sizeof(struct mm_event_task));
	}

	if (need_vmstat)
		record_vmstat();
}

void mm_event_start(ktime_t *time)
{
	*time = ktime_get();
}

void mm_event_end(enum mm_event_type event, ktime_t start)
{
	s64 elapsed = ktime_us_delta(ktime_get(), start);

	current->mm_event[event].count++;
	current->mm_event[event].accm_lat += elapsed;
	if (elapsed > current->mm_event[event].max_lat)
		current->mm_event[event].max_lat = elapsed;
	record_stat();
}

void mm_event_count(enum mm_event_type event, int count)
{
	current->mm_event[event].count += count;
	record_stat();
}

static struct dentry *mm_event_root;

static int period_ms_set(void *data, u64 val)
{
	if (val < 1 || val > ULONG_MAX)
		return -EINVAL;

	write_lock(&period_lock);
	period_ms = (unsigned long)val;
	write_unlock(&period_lock);
	return 0;
}

static int period_ms_get(void *data, u64 *val)
{
	read_lock(&period_lock);
	*val = period_ms;
	read_unlock(&period_lock);

	return 0;
}

static int vmstat_period_ms_set(void *data, u64 val)
{
	if (val < 1 || val > ULONG_MAX)
		return -EINVAL;

	spin_lock(&vmstat_lock);
	vmstat_period_ms = (unsigned long)val;
	spin_unlock(&vmstat_lock);
	return 0;
}

static int vmstat_period_ms_get(void *data, u64 *val)
{
	spin_lock(&vmstat_lock);
	*val = vmstat_period_ms;
	spin_unlock(&vmstat_lock);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(period_ms_operations, period_ms_get,
			period_ms_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(vmstat_period_ms_operations, vmstat_period_ms_get,
			vmstat_period_ms_set, "%llu\n");

static int __init mm_event_init(void)
{
	struct dentry *entry;

	mm_event_root = debugfs_create_dir("mm_event", NULL);
	if (!mm_event_root) {
		pr_warn("debugfs dir <mm_event> creation failed\n");
		return PTR_ERR(mm_event_root);
	}

	entry = debugfs_create_file("period_ms", 0644,
			mm_event_root, NULL, &period_ms_operations);

	if (IS_ERR(entry)) {
		pr_warn("debugfs file mm_event_task creation failed\n");
		debugfs_remove_recursive(mm_event_root);
		return PTR_ERR(entry);
	}

	entry = debugfs_create_file("vmstat_period_ms", 0644,
			mm_event_root, NULL, &vmstat_period_ms_operations);
	if (IS_ERR(entry)) {
		pr_warn("debugfs file vmstat_mm_event_task creation failed\n");
		debugfs_remove_recursive(mm_event_root);
		return PTR_ERR(entry);
	}

	return 0;
}
subsys_initcall(mm_event_init);
