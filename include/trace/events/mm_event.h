#undef TRACE_SYSTEM
#define TRACE_SYSTEM mm_event

#if !defined(_TRACE_MM_EVENT_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MM_EVENT_H

#include <linux/types.h>
#include <linux/tracepoint.h>
#include <linux/mm.h>
#include <linux/mm_event.h>

struct mm_event_task;
struct mm_event_vmstat;

#define show_mm_event_type(type)					\
	__print_symbolic(type,                                          \
	{ MM_MIN_FAULT, "min_flt" },                                    \
	{ MM_MAJ_FAULT, "maj_flt" },                                    \
	{ MM_READ_IO,	"read_io" },                                    \
	{ MM_COMPACTION, "compaction" },                                \
	{ MM_RECLAIM, "reclaim" },					\
	{ MM_SWP_FAULT, "swp_flt" },					\
	{ MM_KERN_ALLOC, "kern_alloc" },                                \
	{ BLK_READ_SUBMIT_BIO, "blk_read_submit_bio" },                 \
	{ UFS_READ_QUEUE_CMD, "ufs_read_queue_cmd" },                   \
	{ UFS_READ_SEND_CMD, "ufs_read_send_cmd" },                     \
	{ UFS_READ_COMPL_CMD, "ufs_read_compl_cmd" },                   \
	{ F2FS_READ_DATA, "f2fs_read_data" })

TRACE_EVENT(mm_event_record,

	TP_PROTO(enum mm_event_type type, struct mm_event_task *record),

	TP_ARGS(type, record),

	TP_STRUCT__entry(
		__field(enum mm_event_type, type)
		__field(unsigned int,	count)
		__field(unsigned int,	avg_lat)
		__field(unsigned int,	max_lat)
	),

	TP_fast_assign(
		__entry->type		= type;
		__entry->count		= record->count;
		__entry->avg_lat	= record->accm_lat / record->count;
		__entry->max_lat	= record->max_lat;
	),

	TP_printk("%s count=%d avg_lat=%u max_lat=%u",
					show_mm_event_type(__entry->type),
					__entry->count, __entry->avg_lat,
					__entry->max_lat)
);

TRACE_EVENT(mm_event_vmstat_record,

	TP_PROTO(struct mm_event_vmstat *vmstat),

	TP_ARGS(vmstat),

	TP_STRUCT__entry(
		__field(unsigned long, free)
		__field(unsigned long, file)
		__field(unsigned long, anon)
		__field(unsigned long, ion)
		__field(unsigned long, slab)
		__field(unsigned long, ws_refault)
		__field(unsigned long, ws_activate)
		__field(unsigned long, mapped)
		__field(unsigned long, pgin)
		__field(unsigned long, pgout)
		__field(unsigned long, swpin)
		__field(unsigned long, swpout)
		__field(unsigned long, reclaim_steal)
		__field(unsigned long, reclaim_scan)
		__field(unsigned long, compact_scan)
	),

	TP_fast_assign(
		__entry->free		= vmstat->free;
		__entry->file		= vmstat->file;
		__entry->anon		= vmstat->anon;
		__entry->ion		= vmstat->ion;
		__entry->slab		= vmstat->slab;
		__entry->ws_refault	= vmstat->ws_refault;
		__entry->ws_activate	= vmstat->ws_activate;
		__entry->mapped		= vmstat->mapped;
		__entry->pgin		= vmstat->pgin;
		__entry->pgout		= vmstat->pgout;
		__entry->swpin		= vmstat->swpin;
		__entry->swpout		= vmstat->swpout;
		__entry->reclaim_steal	= vmstat->reclaim_steal;
		__entry->reclaim_scan	= vmstat->reclaim_scan;
		__entry->compact_scan	= vmstat->compact_scan;
	),

	TP_printk("free=%lu file=%lu anon=%lu ion=%lu slab=%lu ws_refault=%lu "
		  "ws_activate=%lu mapped=%lu pgin=%lu pgout=%lu swpin=%lu "
		  "swpout=%lu reclaim_steal=%lu reclaim_scan=%lu compact_scan=%lu",
			__entry->free, __entry->file,
			__entry->anon, __entry->ion, __entry->slab,
			__entry->ws_refault, __entry->ws_activate,
			__entry->mapped, __entry->pgin, __entry->pgout,
			__entry->swpin, __entry->swpout,
			__entry->reclaim_steal, __entry->reclaim_scan,
			__entry->compact_scan)
);

#endif /* _TRACE_MM_EVENT_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
