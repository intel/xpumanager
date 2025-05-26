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

#ifndef _CLI_H
#define _CLI_H

#include <cmds.h>
#include <functional>

using namespace std;

#define STRINGIFY(x) #x
#define CONCATENATE_WITH_DOT(a, b) "v" STRINGIFY(a) "." STRINGIFY(b)
#define CONCATENATE_WITH_DOTS(a, b, c, d) STRINGIFY(a) "." STRINGIFY(b) "." STRINGIFY(c) "." STRINGIFY(d)
#define GET_SHORT_VERSION() CONCATENATE_WITH_DOT(MAJOR, MINOR)
#define GET_FULL_VERSION() CONCATENATE_WITH_DOTS(MAJOR, MINOR, PATCH, BUILD_NUMBER)

// Enum to represent OS types
enum class OSTYPE
{
	Windows,
	Linux,
	Both,
};

/* Structure to hold function and OS type */
struct function_entry
{
	function<cmds *()> create_func;
	DAEMONCAP daemon_cap;
	OSTYPE os_type;
};

#endif