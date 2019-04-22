#undef TRACE_SYSTEM
#define TRACE_SYSTEM mm_event

#if !defined(_TRACE_MM_EVENT_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MM_EVENT_H

#include <linux/types.h>
#include <linux/tracepoint.h>
#include <linux/mm.h>

struct mm_event_task;

#define show_mm_event_type(type)					\
	__print_symbolic(type,                                          \
	{ MM_MIN_FAULT, "min_flt" },                                    \
	{ MM_MAJ_FAULT, "maj_flt" },                                    \
	{ MM_COMPACTION, "compaction" },                                \
	{ MM_RECLAIM, "reclaim" },					\
	{ MM_SWP_FAULT, "swp_flt" })

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

#endif /* _TRACE_MM_EVENT_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
