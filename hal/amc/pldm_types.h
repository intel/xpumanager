/*
 * Copyright © 2026 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#ifndef __PLDM_TYPES_H
#define __PLDM_TYPES_H

#include <stdint.h>

typedef uint8_t bool8_t;
typedef float real32_t;

// PLDM timestamp structure (104-bit timestamp from PLDM spec)
#pragma pack(push, 1)
typedef struct
{
	uint8_t timeResolution : 4; /* byte 12 bit 3:0 time resolution */
	uint8_t utcResolution : 4;	/* byte 12 bit 7:4 UTC resolution */
	uint16_t year;				/* bytes 11:10 year */
	uint8_t month;				/* byte 9 month */
	uint8_t day;				/* byte 8 day */
	uint8_t hour;				/* byte 7 hour */
	uint8_t minute;				/* byte 6 minute */
	uint8_t second;				/* byte 5 second */
	uint8_t microsecond[3];		/* 24-bit bytes 4:2 microsecond */
	int16_t utcOffset;			/* bytes 1:0 offset in minutes signed */
} timestamp104_t;
#pragma pack(pop)

typedef union
{
	uint8_t byte;
	struct
	{
		uint8_t bit0 : 1;
		uint8_t bit1 : 1;
		uint8_t bit2 : 1;
		uint8_t bit3 : 1;
		uint8_t bit4 : 1;
		uint8_t bit5 : 1;
		uint8_t bit6 : 1;
		uint8_t bit7 : 1;
	} bits;
} bitfield8_t;

#endif /* __PLDM_TYPES_H */
