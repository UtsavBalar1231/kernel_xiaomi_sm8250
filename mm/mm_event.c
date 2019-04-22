#include <linux/mm.h>
#include <linux/mm_event.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>

#define CREATE_TRACE_POINTS
#include <trace/events/mm_event.h>

void mm_event_task_init(struct task_struct *tsk)
{
	memset(tsk->mm_event, 0, sizeof(tsk->mm_event));
	tsk->next_period = 0;
}

static void record_stat(void)
{
	if (time_is_before_eq_jiffies(current->next_period)) {
		int i;

		for (i = 0; i < MM_TYPE_NUM; i++) {
			if (current->mm_event[i].count == 0)
				continue;

			trace_mm_event_record(i, &current->mm_event[i]);
			memset(&current->mm_event[i], 0,
					sizeof(struct mm_event_task));
		}
		current->next_period = jiffies + (HZ >> 1);
	}
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

static struct dentry *mm_event_root;

static int __init mm_event_init(void)
{
	mm_event_root = debugfs_create_dir("mm_event", NULL);
	if (!mm_event_root) {
		pr_warn("debugfs dir <mm_event> creation failed\n");
		return PTR_ERR(mm_event_root);
	}

	return 0;
}
subsys_initcall(mm_event_init);
