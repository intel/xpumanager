/*
 * Copyright � 2024 Intel Corporation
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
#include <windows.h>

#ifdef LIBXPUM_EXPORTS
#define LIBXPUM_API __declspec(dllexport)
#else
#define LIBXPUM_API __declspec(dllimport)
#endif

class thread_id {

protected:
	HANDLE thread_hdl;

public:
	thread_id(HANDLE hdl) { thread_hdl = hdl; }
	HANDLE ret_thread_uid() { return thread_hdl; }

};

#define UNUSED(x)                          (void)(x)
#define TWO_MB                             (2 * 1024 * 1024)
#define STRCPY_S(dest, sz, src)            strcpy_s(dest, sz, src)
#define STRNCPY_S(dest, src, sz)           strncpy_s(dest, src, sz)
#define STRTOK_S(str, delimiters, context) strtok_s(str, delimiters, context)
#define STRCASECMP                         _stricmp
#define THREAD_RET                         DWORD WINAPI
#define GET_PCI_DEV(devs)                  (1)
#define PCI_CLEANUP(devs, found_dev)
#define GETOPT                             getopt

typedef DWORD(WINAPI* funcptr)(void* input_params);
extern char* optarg;
extern int   optind;
int getopt(int argc, char* argv[], char* optstring);
void *align_alloc(size_t size);
thread_id* create_thread(funcptr thread, void* args);
void wait_for_thread(thread_id* tid);

#endif
