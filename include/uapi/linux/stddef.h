/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#include <linux/compiler_types.h>

#ifndef __always_inline
#define __always_inline inline
#endif

/**
 * __DECLARE_FLEX_ARRAY() - Declare a flexible array usable in a union
 *
 * @TYPE: The type of each flexible array element
 * @NAME: The name of the flexible array member
 *
 * In order to have a flexible array member in a union or alone in a
 * struct, it needs to be wrapped in an anonymous struct with at least 1
 * named member, but that member can be empty.
 */
#define __DECLARE_FLEX_ARRAY(TYPE, NAME)	\
	struct { \
		struct { } __empty_ ## NAME; \
		TYPE NAME[]; \
	}
