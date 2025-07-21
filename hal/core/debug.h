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

#ifndef _DEBUG_H
#define _DEBUG_H

#include <stdio.h>

// Check for C++20 source_location support
#if __cplusplus >= 202002L && __has_include(<source_location>)
#include <source_location>
#define HAS_SOURCE_LOCATION 1
#else
#define HAS_SOURCE_LOCATION 0
#endif

#ifdef _WIN32
static int is_windows = 1;
#else
static int is_windows __attribute__((unused)) = 0;
#endif

enum
{
	NO_PRINT = -1,
	ERR,
	INFO,
	DBG,
	TRACE,
};

/**
 * Set it to 0 or higher with 0 being lowest number of messages
 * 0 = Only errors show up
 * 1 = Errors + Info messages
 * 2 = Errors + Info messages + Debug messages
 * 3 = Errors + Info messages + Debug messages + Trace calls
 */
extern int dbgLvl;

#define _PRINT(prefix, fmt, ...)                                                                                       \
	if (1) {                                                                                                           \
		printf("%s", prefix);                                                                                          \
		printf(fmt, ##__VA_ARGS__);                                                                                    \
		fflush(stdout);                                                                                                \
	}

#define PRINT(fmt, ...) _PRINT("", fmt, ##__VA_ARGS__)
#define ERR(fmt, ...)                                                                                                  \
	if (dbgLvl >= ERR)                                                                                                 \
	_PRINT("[Error] ", fmt, ##__VA_ARGS__)
#define INFO(fmt, ...)                                                                                                 \
	if (dbgLvl >= INFO)                                                                                                \
	_PRINT("[Info] ", fmt, ##__VA_ARGS__)
#define DBG(fmt, ...)                                                                                                  \
	if (dbgLvl >= DBG)                                                                                                 \
	_PRINT("[DBG] ", fmt, ##__VA_ARGS__)
#define TRACE(fmt, ...)                                                                                                \
	if (dbgLvl >= TRACE)                                                                                               \
	PRINT(fmt, ##__VA_ARGS__)

#ifdef __cplusplus
#if HAS_SOURCE_LOCATION
#define TRACING() tracer trace{};
#else
#define TRACING() tracer trace(__FUNCTION__);
#endif
#else
#define TRACING()
#endif

#ifdef __cplusplus
class tracer
{
private:
	const char *m_func_name;

public:
#if HAS_SOURCE_LOCATION
	// C++20 source_location constructor
	explicit tracer(const std::source_location &loc = std::source_location::current())
		: m_func_name(loc.function_name())
	{
		TRACE(">>> %s\n", m_func_name);
	}
#endif

	// Fallback constructor for older C++ standards
	explicit tracer(const char *func_name) : m_func_name(func_name) { TRACE(">>> %s\n", m_func_name); }

	~tracer() { TRACE("<<< %s\n", m_func_name); }

	// Prevents additional room for dangling references
	tracer(const tracer &) = delete;
	tracer &operator=(const tracer &) = delete;
	tracer(tracer &&) = delete;
	tracer &operator=(tracer &&) = delete;
};
#endif

inline int setDbgLvl(int lvl)
{
	int ret = 1;
	if (lvl >= NO_PRINT && lvl <= TRACE) {
		dbgLvl = lvl;
		ret = 0;
	}
	return ret;
}

inline int getDbgLvl() { return dbgLvl; }

#endif
