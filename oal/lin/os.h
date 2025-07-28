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
#include <cstdint>
#include <cstring>
#include <getopt.h>
#include <pthread.h>
#include <string>
#include <unistd.h>
#include <sys/time.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef MAX_PATH
#define MAX_PATH 256
#endif
#define LIBXPUM_API
#define UNUSED_VAR(x) (void)(x)
#define UNUSED [[maybe_unused]]
#define STRCPY_S(dest, sz, src) strcpy(dest, src)
#define STRNCPY_S(dest, src, sz) strncpy(dest, src, sz)
#define STRTOK_S(str, delimiters, context) strtok(str, delimiters)
#define STRCASECMP strcasecmp
#define THREAD_RET void *
#define GETOPT getopt
#define GETOPT_LONG getopt_long
#define GETGFXFWSTATUS(meiPath) getGfxFwStatus(meiPath)
#define PRIVILEGECHECK() privilegeCheck()
#define GETPROCESSNAME(processId) getProcessName(processId)
#define OPENI2C openI2C
#define CLOSEI2C closeI2C
#define SETENV(name, value) setenv(name, value, 1)
#define MSLEEP(ms) usleep(ms * 1000) // Convert milliseconds to microseconds
#define GETCH getch
#define TIMESTAMP timestamp
#define GET_LOCAL_CPUS(bdf) getLocalCpus(bdf)
#define GET_CPU_LIST(bdf) getCpuList(bdf)

typedef void *(*funcptr)(void *input_params);

class thread_id
{

protected:
	pthread_t thread_hdl;

public:
	thread_id(pthread_t hdl) { thread_hdl = hdl; }
	pthread_t ret_thread_uid() { return thread_hdl; }
};

thread_id *create_thread(funcptr thread, void *args);
void wait_for_thread(thread_id *tid);
bool privilegeCheck();
std::string getProcessName(uint32_t processId);
long long openI2C(const std::string &deviceName);
int closeI2C(long long fd);
char getch();
std::string timestamp();
std::string getLocalCpus(std::string bdf);
std::string getCpuList(std::string bdf);

#endif
