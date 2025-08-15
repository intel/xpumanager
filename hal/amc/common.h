/*
 * Copyright © 2025 Intel Corporation
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
void hexdump(uint8_t buff[], unsigned int len);
unsigned int bytesToInt(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4);

#endif // __COMMON_H
