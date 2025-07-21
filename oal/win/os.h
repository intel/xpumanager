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

#ifndef _OS_H
#define _OS_H

#include <cstddef>
#define NOMINMAX
#include <string>
#include <windows.h>
#include <process.h>
#include <conio.h>

#ifdef LIBXPUM_EXPORTS
#define LIBXPUM_API __declspec(dllexport)
#else
#define LIBXPUM_API __declspec(dllimport)
#endif

class thread_id
{

protected:
	HANDLE thread_hdl;

public:
	thread_id(HANDLE hdl) { thread_hdl = hdl; }
	HANDLE ret_thread_uid() { return thread_hdl; }
};

struct option
{
	const char *name;
	int has_arg;
	int *flag;
	int val;
};

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

#define AMC_PATH L"\\\\.\\NF_I2C_BUS_00_0x0040"
#define UNUSED_VAR(x) (void)(x)
#define UNUSED [[maybe_unused]]

#define TWO_MB (2 * 1024 * 1024)
#define STRCPY_S(dest, sz, src) strcpy_s(dest, sz, src)
#define STRNCPY_S(dest, src, sz) strncpy_s(dest, src, sz)
#define STRTOK_S(str, delimiters, context) strtok_s(str, delimiters, context)
#define STRCASECMP _stricmp
#define THREAD_RET DWORD WINAPI
#define GETOPT getopt
#define GETOPT_LONG getopt_long
#define no_argument 0
#define required_argument 1
#define optional_argument 2
#define GETGFXFWSTATUS(meiPath) GfxFwStatus::NORMAL
#define PRIVILEGECHECK() true
#define GETPROCESSNAME(processId) getProcessName(processId)
#define OPENI2C openI2C
#define CLOSEI2C closeI2C
#define SETENV(name, value) _putenv_s(name, value)
#define MSLEEP(ms) Sleep(ms)
#define GETCH (char) _getch
#define TIMESTAMP timestamp

typedef DWORD(WINAPI *funcptr)(void *input_params);
extern LIBXPUM_API char *optarg;
extern LIBXPUM_API int optind;
int getopt(int argc, char *argv[], char *optstring);
LIBXPUM_API int getopt_long(int argc, char *const argv[], const char *optstring, const struct option *longopts,
							int *longindex);
void *align_alloc(size_t size);
thread_id *create_thread(funcptr thread, void *args);
void wait_for_thread(thread_id *tid);
std::string getProcessName(uint32_t processId);
long long openI2C(const std::string &deviceName);
int closeI2C(long long fd);
std::string timestamp();

#endif
