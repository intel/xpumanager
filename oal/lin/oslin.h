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

#ifndef _OSLIN_H
#define _OSLIN_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <getopt.h>
#include <pthread.h>
#include <string>
#include <unistd.h>
#include <sys/time.h>
#include <vector>

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

#define LIBXPUM_API
#define STRCPY_S(dest, sz, src) strcpy(dest, src)
#define STRNCPY_S(dest, src, sz) strncpy(dest, src, sz)
#define STRCASECMP strcasecmp
#define THREAD_RET void *
#define GETOPT_LONG getopt_long
#define GETGFXFWSTATUS(meiPath) getGfxFwStatus(meiPath)
#define PRIVILEGECHECK() privilegeCheck()
#define ZERO_MEM(mem, size) memset(mem, 0, size)
#define SETENV(name, value) setenv(name, value, 1)
#define MSLEEP(ms) usleep(ms * 1000) // Convert milliseconds to microseconds
#define GETCH getch
#define GET_LOCAL_CPUS(bdf) getLocalCpus(bdf)
#define GET_CPU_LIST(bdf) getCpuList(bdf)
#define GET_TOPOLOGY getTopology
typedef wchar_t TCHAR;
#define GETLOGS(f) getLinLogs(f)
#define GETDRMPATH(bdf) getDrmPath(bdf)

static inline int fopen_s_def(FILE **pFile, const char *filename, const char *mode)
{
	*(pFile) = fopen(filename, mode);
	return (*(pFile) != NULL) ? 0 : errno;
}

#define FOPEN_S(pFile, filename, mode) fopen_s_def((pFile), (filename), (mode))

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
char getch();
std::string timestamp();
std::string getLocalCpus(const std::string &bdf);
std::string getCpuList(const std::string &bdf);
int getTopology(bdfID bdf, std::string *switchDevicePath);
int amcCardDiscovery(std::vector<amcCardInfo> *amcDeviceList);
int getLinLogs(const std::string &fileName);
std::string getDrmPath(const std::string &bdf);

#endif
