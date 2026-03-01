/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
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

typedef union
{
	uint16_t value;
	struct
	{
		uint16_t bit0 : 1;
		uint16_t bit1 : 1;
		uint16_t bit2 : 1;
		uint16_t bit3 : 1;
		uint16_t bit4 : 1;
		uint16_t bit5 : 1;
		uint16_t bit6 : 1;
		uint16_t bit7 : 1;
		uint16_t bit8 : 1;
		uint16_t bit9 : 1;
		uint16_t bit10 : 1;
		uint16_t bit11 : 1;
		uint16_t bit12 : 1;
		uint16_t bit13 : 1;
		uint16_t bit14 : 1;
		uint16_t bit15 : 1;
	} bits;
} bitfield16_t;

typedef union
{
	uint32_t value;
} bitfield32_t;
#endif /* __PLDM_TYPES_H */
