/*
******************************************************************************
*
*   Copyright (C) 2002, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*   For licensing terms see the ICU X License:
*   http://oss.software.ibm.com/cvs/icu/~checkout~/icu/license.html
*
******************************************************************************
*   file name:  bocu1.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2002jan24
*   created by: Markus W. Scherer
*
*   This is a sample implementation of encoder and decoder functions for BOCU-1,
*   a MIME-compatible Binary Ordered Compression for Unicode.
*/
#include "stdafx.h"
#include <stdio.h>
#include <string.h>

/*
 * Standard ICU header.
 * - Includes inttypes.h or defines its types.
 * - Defines UChar for UTF-16 as an unsigned 16-bit type (wchar_t or uint16_t).
 * - Defines UTF* macros to handle reading and writing
 *   of in-process UTF-8/16 strings.
 */
//#include "unicode/utypes.h"

#include "bocu1.h"

using namespace BOCU1;

/* BOCU-1 implementation functions ------------------------------------------ */

/**
 * Compute the next "previous" value for differencing
 * from the current code point.
 *
 * @param c current code point, 0..0x10ffff
 * @return "previous code point" state value
 */
int32_t bocu1Prev(int32_t c)
{
    /* compute new prev */
    if(0x3040<=c && c<=0x309f) {
        /* Hiragana is not 128-aligned */
        return 0x3070;
    } else if(0x4e00<=c && c<=0x9fa5) {
        /* CJK Unihan */
        return 0x4e00-BOCU1_REACH_NEG_2;
    } else if(0xac00<=c && c<=0xd7a3) {
        /* Korean Hangul */
        return (0xd7a3+0xac00)/2;
    } else {
        /* mostly small scripts */
        return (c&~0x7f)+BOCU1_ASCII_PREV;
    }
}

/**
 * Encode a difference -0x10ffff..0x10ffff in 1..4 bytes
 * and return a packed integer with them.
 *
 * The encoding favors small absolut differences with short encodings
 * to compress runs of same-script characters.
 *
 * @param diff difference value -0x10ffff..0x10ffff
 * @return
 *      0x010000zz for 1-byte sequence zz
 *      0x0200yyzz for 2-byte sequence yy zz
 *      0x03xxyyzz for 3-byte sequence xx yy zz
 *      0xwwxxyyzz for 4-byte sequence ww xx yy zz (ww>0x03)
 */
/*U_CFUNC*/ int32_t BOCU1::packDiff(int32_t diff)
{
    int32_t result, m, lead, count, shift;

    if(diff>=BOCU1_REACH_NEG_1) {
        /* mostly positive differences, and single-byte negative ones */
        if(diff<=BOCU1_REACH_POS_1) {
            /* single byte */
            return 0x01000000|(BOCU1_MIDDLE+diff);
        } else if(diff<=BOCU1_REACH_POS_2) {
            /* two bytes */
            diff-=BOCU1_REACH_POS_1+1;
            lead=BOCU1_START_POS_2;
            count=1;
        } else if(diff<=BOCU1_REACH_POS_3) {
            /* three bytes */
            diff-=BOCU1_REACH_POS_2+1;
            lead=BOCU1_START_POS_3;
            count=2;
        } else {
            /* four bytes */
            diff-=BOCU1_REACH_POS_3+1;
            lead=BOCU1_START_POS_4;
            count=3;
        }
    } else {
        /* two- and four-byte negative differences */
        if(diff>=BOCU1_REACH_NEG_2) {
            /* two bytes */
            diff-=BOCU1_REACH_NEG_1;
            lead=BOCU1_START_NEG_2;
            count=1;
        } else if(diff>=BOCU1_REACH_NEG_3) {
            /* three bytes */
            diff-=BOCU1_REACH_NEG_2;
            lead=BOCU1_START_NEG_3;
            count=2;
        } else {
            /* four bytes */
            diff-=BOCU1_REACH_NEG_3;
            lead=BOCU1_START_NEG_4;
            count=3;
        }
    }

    /* encode the length of the packed result */
    if(count<3) {
        result=(count+1)<<24;
    } else /* count==3, MSB used for the lead byte */ {
        result=0;
    }

    /* calculate trail bytes like digits in itoa() */
    shift=0;
    do {
        NEGDIVMOD(diff, BOCU1_TRAIL_COUNT, m);
        result|=BOCU1_TRAIL_TO_BYTE(m)<<shift;
        shift+=8;
    } while(--count>0);

    /* add lead byte */
    result|=(lead+diff)<<shift;

    return result;
}

/**
 * BOCU-1 encoder function.
 *
 * @param pPrev pointer to the integer that holds
 *        the "previous code point" state;
 *        the initial value should be 0 which
 *        encodeBocu1 will set to the actual BOCU-1 initial state value
 * @param c the code point to encode
 * @return the packed 1/2/3/4-byte encoding, see packDiff(),
 *         or 0 if an error occurs
 *
 * @see packDiff
 */
/*U_CFUNC*/ int32_t BOCU1::encodeBocu1(int32_t *pPrev, int32_t c)
{
    int32_t prev;

    if(pPrev==NULL || c<0 || c>0x10ffff) {
        /* illegal argument */
        return 0;
    }

    prev=*pPrev;
    if(prev==0) {
        /* lenient handling of initial value 0 */
        prev=*pPrev=BOCU1_ASCII_PREV;
    }

    if(c<=0x20) {
        /*
         * ISO C0 control & space:
         * Encode directly for MIME compatibility,
         * and reset state except for space, to not disrupt compression.
         */
        if(c!=0x20) {
            *pPrev=BOCU1_ASCII_PREV;
        }
        return 0x01000000|c;
    }

    /*
     * all other Unicode code points c==U+0021..U+10ffff
     * are encoded with the difference c-prev
     *
     * a new prev is computed from c,
     * placed in the middle of a 0x80-block (for most small scripts) or
     * in the middle of the Unihan and Hangul blocks
     * to statistically minimize the following difference
     */
    *pPrev=bocu1Prev(c);
    return packDiff(c-prev);
}

/**
 * Function for BOCU-1 decoder; handles multi-byte lead bytes.
 *
 * @param pRx pointer to the decoder state structure
 * @param b lead byte;
 *          BOCU1_MIN<=b<BOCU1_START_NEG_2 or BOCU1_START_POS_2<=b<=BOCU1_MAX_LEAD
 * @return -1 (state change only)
 *
 * @see decodeBocu1
 */
int32_t decodeBocu1LeadByte(Bocu1Rx *pRx, uint8_t b) {
    int32_t c, count;

    if(b>=BOCU1_START_NEG_2) {
        /* positive difference */
        if(b<BOCU1_START_POS_3) {
            /* two bytes */
            c=((int32_t)b-BOCU1_START_POS_2)*BOCU1_TRAIL_COUNT+BOCU1_REACH_POS_1+1;
            count=1;
        } else if(b<BOCU1_START_POS_4) {
            /* three bytes */
            c=((int32_t)b-BOCU1_START_POS_3)*BOCU1_TRAIL_COUNT*BOCU1_TRAIL_COUNT+BOCU1_REACH_POS_2+1;
            count=2;
        } else {
            /* four bytes */
            c=BOCU1_REACH_POS_3+1;
            count=3;
        }
    } else {
        /* negative difference */
        if(b>=BOCU1_START_NEG_3) {
            /* two bytes */
            c=((int32_t)b-BOCU1_START_NEG_2)*BOCU1_TRAIL_COUNT+BOCU1_REACH_NEG_1;
            count=1;
        } else if(b>BOCU1_MIN) {
            /* three bytes */
            c=((int32_t)b-BOCU1_START_NEG_3)*BOCU1_TRAIL_COUNT*BOCU1_TRAIL_COUNT+BOCU1_REACH_NEG_2;
            count=2;
        } else {
            /* four bytes */
            c=-BOCU1_TRAIL_COUNT*BOCU1_TRAIL_COUNT*BOCU1_TRAIL_COUNT+BOCU1_REACH_NEG_3;
            count=3;
        }
    }

    /* set the state for decoding the trail byte(s) */
    pRx->diff=c;
    pRx->count=count;
    return -1;
}

/**
 * Function for BOCU-1 decoder; handles multi-byte trail bytes.
 *
 * @param pRx pointer to the decoder state structure
 * @param b trail byte
 * @return result value, same as decodeBocu1
 *
 * @see decodeBocu1
 */
int32_t decodeBocu1TrailByte(Bocu1Rx *pRx, uint8_t b) {
    int32_t t, c, count;

    if(b<=0x20) {
        /* skip some C0 controls and make the trail byte range contiguous */
        t=bocu1ByteToTrail[b];
        if(t<0) {
            /* illegal trail byte value */
            pRx->prev=BOCU1_ASCII_PREV;
            pRx->count=0;
            return -99;
        }
#if BOCU1_MAX_TRAIL<0xff
    } else if(b>BOCU1_MAX_TRAIL) {
        return -99;
#endif
    } else {
        t=(int32_t)b-BOCU1_TRAIL_BYTE_OFFSET;
    }

    /* add trail byte into difference and decrement count */
    c=pRx->diff;
    count=pRx->count;

    if(count==1) {
        /* final trail byte, deliver a code point */
        c=pRx->prev+c+t;
        if(0<=c && c<=0x10ffff) {
            /* valid code point result */
            pRx->prev=bocu1Prev(c);
            pRx->count=0;
            return c;
        } else {
            /* illegal code point result */
            pRx->prev=BOCU1_ASCII_PREV;
            pRx->count=0;
            return -99;
        }
    }

    /* intermediate trail byte */
    if(count==2) {
        pRx->diff=c+t*BOCU1_TRAIL_COUNT;
    } else /* count==3 */ {
        pRx->diff=c+t*BOCU1_TRAIL_COUNT*BOCU1_TRAIL_COUNT;
    }
    pRx->count=count-1;
    return -1;
}

/**
 * BOCU-1 decoder function.
 *
 * @param pRx pointer to the decoder state structure;
 *        the initial values should be 0 which
 *        decodeBocu1 will set to actual initial state values
 * @param b an input byte
 * @return
 *      0..0x10ffff for a result code point
 *      -1 if only the state changed without code point output
 *     <-1 if an error occurs
 */
int32_t BOCU1::decodeBocu1(Bocu1Rx *pRx, uint8_t b)
{
    int32_t prev, c, count;

    if(pRx==NULL) {
        /* illegal argument */
        return -99;
    }

    prev=pRx->prev;
    if(prev==0) {
        /* lenient handling of initial 0 values */
        prev=pRx->prev=BOCU1_ASCII_PREV;
        count=pRx->count=0;
    } else {
        count=pRx->count;
    }

    if(count==0) {
        /* byte in lead position */
        if(b<=0x20) {
            /*
             * Direct-encoded C0 control code or space.
             * Reset prev for C0 control codes but not for space.
             */
            if(b!=0x20) {
                pRx->prev=BOCU1_ASCII_PREV;
            }
            return b;
        }

        /*
         * b is a difference lead byte.
         *
         * Return a code point directly from a single-byte difference.
         *
         * For multi-byte difference lead bytes, set the decoder state
         * with the partial difference value from the lead byte and
         * with the number of trail bytes.
         *
         * For four-byte differences, the signedness also affects the
         * first trail byte, which has special handling farther below.
         */
        if(b>=BOCU1_START_NEG_2 && b<BOCU1_START_POS_2) {
            /* single-byte difference */
            c=prev+((int32_t)b-BOCU1_MIDDLE);
            pRx->prev=bocu1Prev(c);
            return c;
        } else if(b==BOCU1_RESET) {
            /* only reset the state, no code point */
            pRx->prev=BOCU1_ASCII_PREV;
            return -1;
        } else {
            return decodeBocu1LeadByte(pRx, b);
        }
    } else {
        /* trail byte in any position */
        return decodeBocu1TrailByte(pRx, b);
    }
}

/**
 * \def UPRV_BLOCK_MACRO_BEGIN
 * Defined as the "do" keyword by default.
 * @internal
 */
#ifndef UPRV_BLOCK_MACRO_BEGIN
#define UPRV_BLOCK_MACRO_BEGIN do
#endif

 /**
  * \def UPRV_BLOCK_MACRO_END
  * Defined as "while (FALSE)" by default.
  * @internal
  */
#ifndef UPRV_BLOCK_MACRO_END
#define UPRV_BLOCK_MACRO_END while (FALSE)
#endif

#define U16_NEXT(s, i, length, c) UPRV_BLOCK_MACRO_BEGIN { \
    (c)=(s)[(i)++]; \
    if(U16_IS_LEAD(c)) { \
        uint16_t __c2; \
        if((i)!=(length) && U16_IS_TRAIL(__c2=(s)[(i)])) { \
            ++(i); \
            (c)=U16_GET_SUPPLEMENTARY((c), __c2); \
        } \
    } \
} UPRV_BLOCK_MACRO_END

/**
* Is this code unit a lead surrogate (U+d800..U+dbff)?
* @param c 16-bit code unit
* @return TRUE or FALSE
* @stable ICU 2.4
*/
#define U16_IS_LEAD(c) (((c)&0xfffffc00)==0xd800)

/**
 * Is this code unit a trail surrogate (U+dc00..U+dfff)?
 * @param c 16-bit code unit
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U16_IS_TRAIL(c) (((c)&0xfffffc00)==0xdc00)

typedef int32_t UChar32;

 /**
  * Helper constant for U16_GET_SUPPLEMENTARY.
  * @internal
  */
#define U16_SURROGATE_OFFSET ((0xd800<<10UL)+0xdc00-0x10000)

/**
* Get a supplementary code point value (U+10000..U+10ffff)
* from its lead and trail surrogates.
* The result is undefined if the input values are not
* lead and trail surrogates.
*
* @param lead lead surrogate (U+d800..U+dbff)
* @param trail trail surrogate (U+dc00..U+dfff)
* @return supplementary code point (U+10000..U+10ffff)
* @stable ICU 2.4
*/
#define U16_GET_SUPPLEMENTARY(lead, trail) \
    (((UChar32)(lead)<<10UL)+(UChar32)(trail)-U16_SURROGATE_OFFSET)

/**
* Append a code point to a string, overwriting 1 or 2 code units.
* The offset points to the current end of the string contents
* and is advanced (post-increment).
* "Unsafe" macro, assumes a valid code point and sufficient space in the string.
* Otherwise, the result is undefined.
*
* @param s const UChar * string buffer
* @param i string offset
* @param c code point to append
* @see U16_APPEND
* @stable ICU 2.4
*/
#define U16_APPEND_UNSAFE(s, i, c) UPRV_BLOCK_MACRO_BEGIN { \
    if((uint32_t)(c)<=0xffff) { \
        (s)[(i)++]=(uint16_t)(c); \
    } else { \
        (s)[(i)++]=(uint16_t)(((c)>>10)+0xd7c0); \
        (s)[(i)++]=(uint16_t)(((c)&0x3ff)|0xdc00); \
    } \
} UPRV_BLOCK_MACRO_END

   /**
	* Write a packed BOCU-1 byte sequence into a byte array,
	* without overflow check.
	* Test function.
	*
	* @param packed packed BOCU-1 byte sequence, see packDiff()
	* @param p pointer to byte array
	* @return number of bytes
	*
	* @see packDiff
	*/
int32_t writePacked(int32_t packed, uint8_t *p)
{
	int32_t count = BOCU1_LENGTH_FROM_PACKED(packed);
	switch (count) {
	case 4:
		*p++ = (uint8_t)(packed >> 24);
	case 3:
		*p++ = (uint8_t)(packed >> 16);
	case 2:
		*p++ = (uint8_t)(packed >> 8);
	case 1:
		*p++ = (uint8_t)packed;
	default:
		break;
	}

	return count;
}

/**
 * Encode a UTF-16 string in BOCU-1.
 * Does not check for overflows, but otherwise useful function.
 *
 * @param s input UTF-16 string
 * @param length number of UChar code units in s
 * @param p pointer to output byte array
 * @return number of bytes output
 */
int32_t BOCU1::writeString(const wchar_t *s, int32_t length, uint8_t *p) {
	uint8_t *p0;
	int32_t c, prev, i;

	prev = 0;
	p0 = p;
	i = 0;
	while (i < length) {
		U16_NEXT(s, i, length, c);
		p += writePacked(encodeBocu1(&prev, c), p);
	}
	return (int32_t)(p - p0);
}

/**
 * Decode a BOCU-1 byte sequence to a UTF-16 string.
 * Does not check for overflows, but otherwise useful function.
 *
 * @param p pointer to input BOCU-1 bytes
 * @param length number of input bytes
 * @param s point to output UTF-16 string array
 * @return number of UChar code units output
 */
int32_t BOCU1::readString(const uint8_t *p, int32_t length, wchar_t *s)
{
	Bocu1Rx rx = { 0, 0, 0 };
	int32_t c, i, sLength;

	i = sLength = 0;
	while (i < length) {
		c = decodeBocu1(&rx, p[i++]);
		if (c < -1) {
			//log_err("error: readString detects encoding error at string index %ld\n", i);
			throw std::runtime_error("Error while decoding BOCU-1 string");
			//return -1;
		}
		if (c >= 0) {
			U16_APPEND_UNSAFE(s, sLength, c);
		}
	}
	return sLength;
}
