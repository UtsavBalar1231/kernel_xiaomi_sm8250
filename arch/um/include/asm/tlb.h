/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __UM_TLB_H
#define __UM_TLB_H

#include <asm/tlbflush.h>
#include <asm-generic/cacheflush.h>
#include <asm-generic/tlb.h>

static inline void
tlb_flush_pmd_range(struct mmu_gather *tlb, unsigned long address,
		    unsigned long size)
{
	tlb->need_flush = 1;

	if (tlb->start > address)
		tlb->start = address;
	if (tlb->end < address + size)
		tlb->end = address + size;
}

#endif
