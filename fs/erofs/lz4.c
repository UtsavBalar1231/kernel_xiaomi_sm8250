/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * linux/drivers/staging/erofs/lz4.c
 *
 * Copyright (C) 2018 HUAWEI, Inc.
 *             http://www.huawei.com/
 * Created by Gao Xiang <gaoxiang25@huawei.com>
 *
 * Original code taken from 'linux/lib/lz4/lz4_decompress.c'
 */

/*
 * LZ4 - Fast LZ compression algorithm
 * Copyright (C) 2011 - 2016, Yann Collet.
 * BSD 2 - Clause License (http://www.opensource.org/licenses/bsd - license.php)
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *	* Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *	* Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * You can contact the author at :
 *	- LZ4 homepage : http://www.lz4.org
 *	- LZ4 source repository : https://github.com/lz4/lz4
 *
 *	Changed for kernel usage by:
 *	Sven Schmidt <4sschmid@informatik.uni-hamburg.de>
 */
#include "internal.h"
#include <asm/unaligned.h>
#include <linux/string.h>	 /* memset, memcpy */

#include "lz4armv8/lz4accel.h"

#define FORCE_INLINE __always_inline

/*-************************************
 *	Basic Types
 **************************************/
#include <linux/types.h>

typedef uint8_t BYTE;
typedef uint16_t U16;
typedef uint32_t U32;
typedef int32_t S32;
typedef uint64_t U64;
typedef uintptr_t uptrval;

/*-************************************
 *	Architecture specifics
 **************************************/
#if defined(CONFIG_64BIT)
#define LZ4_ARCH64 1
#else
#define LZ4_ARCH64 0
#endif

#if defined(__LITTLE_ENDIAN)
#define LZ4_LITTLE_ENDIAN 1
#else
#define LZ4_LITTLE_ENDIAN 0
#endif

/*-************************************
 *	Constants
 **************************************/
#define MINMATCH 4

#define WILDCOPYLENGTH 8
#define LASTLITERALS 5
#define MFLIMIT (WILDCOPYLENGTH + MINMATCH)

/*
 * ensure it's possible to write 2 x wildcopyLength
 * without overflowing output buffer
 */
#define MATCH_SAFEGUARD_DISTANCE  ((2 * WILDCOPYLENGTH) - MINMATCH)

#define HASH_UNIT sizeof(size_t)

#define KB (1 << 10)
#define MB (1 << 20)
#define GB (1U << 30)

#define MAXD_LOG 16
#define MAX_DISTANCE ((1 << MAXD_LOG) - 1)

#define ML_BITS	4
#define ML_MASK	((1U << ML_BITS) - 1)
#define RUN_BITS (8 - ML_BITS)
#define RUN_MASK ((1U << RUN_BITS) - 1)

/*-************************************
 *	Reading and writing into memory
 **************************************/
static FORCE_INLINE U16 LZ4_readLE16(const void *memPtr)
{
	return get_unaligned_le16(memPtr);
}

static FORCE_INLINE void LZ4_copy8(void *dst, const void *src)
{
#if LZ4_ARCH64
	U64 a = get_unaligned((const U64 *)src);

	put_unaligned(a, (U64 *)dst);
#else
	U32 a = get_unaligned((const U32 *)src);
	U32 b = get_unaligned((const U32 *)src + 1);

	put_unaligned(a, (U32 *)dst);
	put_unaligned(b, (U32 *)dst + 1);
#endif
}

/*
 * customized variant of memcpy,
 * which can overwrite up to 7 bytes beyond dstEnd
 */
static FORCE_INLINE void LZ4_wildCopy(void *dstPtr,
	const void *srcPtr, void *dstEnd)
{
	BYTE *d = (BYTE *)dstPtr;
	const BYTE *s = (const BYTE *)srcPtr;
	BYTE *const e = (BYTE *)dstEnd;

	do {
		LZ4_copy8(d, s);
		d += 8;
		s += 8;
	} while (d < e);
}

#define DEBUGLOG(l, ...) {}	/* disabled */
#define LZ4_STATIC_ASSERT(c)    BUILD_BUG_ON(!(c))

/*
 * no public solution to solve our requirement yet.
 * see: <required buffer size for LZ4_decompress_safe_partial>
 *      https://groups.google.com/forum/#!topic/lz4c/_3kkz5N6n00
 */
static FORCE_INLINE int __lz4_decompress_safe_partial(
	uint8_t         *dst_ptr,
	const uint8_t   *src_ptr,
	BYTE            *dst,
	int             outputSize,
	const void      *src,
	int             inputSize,
	bool            trusted)
{
	/* Local Variables */
	const BYTE *ip = (const BYTE *) src_ptr;
	const BYTE *const iend = src + inputSize;

	BYTE *op = (BYTE *) dst_ptr;
	BYTE *const oend = dst + outputSize;
	BYTE *cpy;

	static const unsigned int inc32table[] = { 0, 1, 2, 1, 0, 4, 4, 4 };
	static const int dec64table[] = { 0, 0, 0, -1, -4, 1, 2, 3 };

	/* Set up the "end" pointers for the shortcut. */
	const BYTE *const shortiend = iend - 14 /*maxLL*/ - 2 /*offset*/;
	const BYTE *const shortoend = oend - 14 /*maxLL*/ - 18 /*maxML*/;

	DEBUGLOG(5, "%s (srcSize:%i, dstSize:%i)", __func__,
		 inputSize, outputSize);

	/* Empty output buffer */
	if (unlikely(!outputSize))
		return ((inputSize == 1) && (*ip == 0)) ? 0 : -EINVAL;

	if (unlikely(!inputSize))
		return -EINVAL;

	/* Main Loop : decode sequences */
	while (1) {
		size_t length;
		const BYTE *match;
		size_t offset;

		/* get literal length */
		unsigned int const token = *ip++;
		length = token >> ML_BITS;

		/* ip < iend before the increment */
		DBG_BUGON(ip > iend);
		/*
		 * A two-stage shortcut for the most common case:
		 * 1) If the literal length is 0..14, and there is enough
		 * space, enter the shortcut and copy 16 bytes on behalf
		 * of the literals (in the fast mode, only 8 bytes can be
		 * safely copied this way).
		 * 2) Further if the match length is 4..18, copy 18 bytes
		 * in a similar manner; but we ensure that there's enough
		 * space in the output for those 18 bytes earlier, upon
		 * entering the shortcut (in other words, there is a
		 * combined check for both stages).
		 */
		if (length != RUN_MASK &&
		    /*
		     * strictly "less than" on input, to re-enter
		     * the loop with at least one byte
		     */
		    likely((ip < shortiend) & (op <= shortoend))) {

			/* Copy the literals */
			memcpy(op, ip, 16);
			op += length;
			ip += length;

			/*
			 * The second stage:
			 * prepare for match copying, decode full info.
			 * If it doesn't work out, the info won't be wasted.
			 */
			length = token & ML_MASK; /* match length */
			offset = LZ4_readLE16(ip);
			ip += 2;
			match = op - offset;
			DBG_BUGON(match > op); /* check overflow */

			/* Do not deal with overlapping matches. */
			if ((length != ML_MASK) &&
			    (offset >= 8) && (match >= dst)) {
				/* Copy the match. */
				LZ4_copy8(op + 0, match + 0);
				LZ4_copy8(op + 8, match + 8);
				memcpy(op + 16, match + 16, 2);
				op += length + MINMATCH;
				/* Both stages worked, load the next token. */
				continue;
			}

			/*
			 * The second stage didn't work out, but the info
			 * is ready. Propel it right to the point of match
			 * copying.
			 */
			goto _copy_match;
		}

		/* decode literal length */
		if (length == RUN_MASK) {
			unsigned int s;

			if (unlikely(!trusted && ip >= iend - RUN_MASK)) {
				/* overflow detection */
				goto _output_error;
			}

			do {
				s = *ip++;
				length += s;
			} while (likely(ip < iend - RUN_MASK) & (s == 255));

			if (!trusted) {
				if (unlikely((uptrval)(op) + length < (uptrval)op))
					/* overflow detection */
					goto _output_error;
				if (unlikely((uptrval)(ip) + length < (uptrval)ip))
					/* overflow detection */
					goto _output_error;
			}
		}

		/* copy literals */
		cpy = op + length;
		LZ4_STATIC_ASSERT(MFLIMIT >= WILDCOPYLENGTH);

		if ((cpy > oend - MFLIMIT)
			|| (ip + length > iend - (2 + 1 + LASTLITERALS))) {
			if (cpy > oend) {
				/*
				 * Partial decoding :
				 * stop in the middle of literal segment
				 */
				cpy = oend;
				length = oend - op;
			}

			if (!trusted && ip + length > iend) {
				/*
				 * Error :
				 * read attempt beyond
				 * end of input buffer
				 */
				goto _output_error;
			}

			memcpy(op, ip, length);
			ip += length;
			op += length;

			/* Necessarily EOF, due to parsing restrictions */
			if (cpy == oend)
				break;
		} else {
			/* may overwrite up to WILDCOPYLENGTH beyond cpy */
			LZ4_wildCopy(op, ip, cpy);
			ip += length;
			op = cpy;
		}

		/* get offset */
		offset = LZ4_readLE16(ip);
		ip += 2;
		match = op - offset;

		/* get matchlength */
		length = token & ML_MASK;

_copy_match:
		if (length == ML_MASK) {
			unsigned int s;

			do {
				s = *ip++;

				if (!trusted && ip > iend - LASTLITERALS)
					goto _output_error;

				length += s;
			} while (s == 255);

			if (unlikely(!trusted &&
				     (uptrval)(op) + length < (uptrval)op)) {
				/* overflow detection */
				goto _output_error;
			}
		}

		length += MINMATCH;

		/* copy match within block */
		cpy = op + length;

		/*
		 * partialDecoding :
		 * may not respect endBlock parsing restrictions
		 */
		DBG_BUGON(op > oend);
		if (cpy > oend - MATCH_SAFEGUARD_DISTANCE) {
			size_t const mlen = min(length, (size_t)(oend - op));
			const BYTE * const matchEnd = match + mlen;
			BYTE * const copyEnd = op + mlen;

			if (matchEnd > op) {
				/* overlap copy */
				while (op < copyEnd)
					*op++ = *match++;
			} else {
				memcpy(op, match, mlen);
			}
			op = copyEnd;
			if (op == oend)
				break;
			continue;
		}

		if (unlikely(offset < 8)) {
			op[0] = match[0];
			op[1] = match[1];
			op[2] = match[2];
			op[3] = match[3];
			match += inc32table[offset];
			memcpy(op + 4, match, 4);
			match -= dec64table[offset];
		} else {
			LZ4_copy8(op, match);
			match += 8;
		}

		op += 8;
		if (unlikely(cpy > oend - MATCH_SAFEGUARD_DISTANCE)) {
			BYTE * const oCopyLimit = oend - (WILDCOPYLENGTH - 1);

			if (!trusted && cpy > oend - LASTLITERALS) {
				/*
				 * Error : last LASTLITERALS bytes
				 * must be literals (uncompressed)
				 */
				goto _output_error;
			}

			if (op < oCopyLimit) {
				LZ4_wildCopy(op, match, oCopyLimit);
				match += oCopyLimit - op;
				op = oCopyLimit;
			}
			while (op < cpy)
				*op++ = *match++;
		} else {
			LZ4_copy8(op, match);
			if (length > 16)
				LZ4_wildCopy(op + 8, match + 8, cpy);
		}
		op = cpy; /* wildcopy correction */
	}

	/* end of decoding */
	/* Nb of output bytes decoded */
	return (int) (((BYTE *)op) - dst);

	/* Overflow error detected */
_output_error:
	return -ERANGE;
}

static ssize_t lz4_decompress(const void *source,
			      void *dest,
			      size_t inputSize,
			      size_t outputSize,
			      bool accel, bool dip)
{
        uint8_t         *dstPtr = dest;
        const uint8_t   *srcPtr = source;
        ssize_t         ret;

#ifdef __ARCH_HAS_LZ4_ACCELERATOR
        /* Go fast if we can, keeping away from the end of buffers */
        if (outputSize > LZ4_FAST_MARGIN && inputSize > LZ4_FAST_MARGIN &&
            accel && lz4_decompress_accel_enable()) {
                ret = lz4_decompress_asm(&dstPtr, dest,
                                         dest + outputSize - LZ4_FAST_MARGIN,
                                         &srcPtr,
                                         source + inputSize - LZ4_FAST_MARGIN,
                                         dip);
                if (ret)
                        return -1;
        }
#endif
        /* Finish in safe */
	return __lz4_decompress_safe_partial(dstPtr, srcPtr, dest, outputSize,
					     source, inputSize, false);
}


int z_erofs_lz4_decompress_partial(const char *in, char *out,
				   unsigned int inlen, unsigned int outlen,
				   bool accel, bool dip)
{
	int ret = lz4_decompress(in, out, inlen, outlen, accel, dip);

	if (ret >= 0)
		return ret;

	return -EIO;
}

