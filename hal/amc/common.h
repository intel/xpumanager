/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef __COMMON_H
#define __COMMON_H

#include "debug.h"
#include <iostream>
#include <os.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum
{
	BYTE_0,
	BYTE_1,
	BYTE_2,
	BYTE_3,
	BYTE_4,
	BYTE_5,
	BYTE_6,
	BYTE_7,
	BYTE_8,
	BYTE_9,
	BYTE_10,
	BYTE_11,
	BYTE_12,
	BYTE_13,
	BYTE_14,
	BYTE_15,
};

#define RETRY_COUNT 3

uint8_t crc8Smbus(const uint8_t *data, size_t len);
void hexdump(const uint8_t buff[], unsigned int len);
unsigned int bytesToInt(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4);

#endif // __COMMON_H
