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

#ifndef _HELP_CMD_H
#define _HELP_CMD_H

#include <os.h>

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

enum GAP
{
	NO_GAP,
	SMALL_GAP = 2,
	LARGE_GAP = 30,
	XLARGE_GAP = 37,
};

struct help_cmd
{
	char line[MAX_PATH];
	int char_gap;

	// Default constructor
	help_cmd()
	{
		memset(line, 0, MAX_PATH);
	}

	// Copy constructor
	help_cmd(GAP gap, const char *other)
	{
		char_gap = (int)gap;
		STRNCPY_S(line, other, MAX_PATH);
	}
};

#endif
