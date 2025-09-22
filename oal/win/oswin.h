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

#ifndef _OSWIN_H
#define _OSWIN_H

#define NOMINMAX
#include <string>
#include <windows.h>
#include <process.h>
#include <conio.h>
#include <vector>

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

#define ZERO_MEM(ptr, size) SecureZeroMemory(ptr, size)
#define AMC_PATH L"\\\\.\\NF_I2C_BUS_00_0x0040"
#define FOPEN_S(pFile, filename, mode) fopen_s((pFile), (filename), (mode))
#define MAX_I2C_BUFFER_SIZE 128
#define FILE_DEVICE_SPB_PERIPHERAL 0x400
#define IOCTL_SPBTESTTOOL_OPEN CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x700, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPBTESTTOOL_CLOSE CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x701, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPBTESTTOOL_LOCK CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x702, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPBTESTTOOL_UNLOCK CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x703, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPBTESTTOOL_WRITEREAD CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x704, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPBTESTTOOL_LOCK_CONNECTION CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x705, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPBTESTTOOL_UNLOCK_CONNECTION                                                                            \
	CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x706, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPBTESTTOOL_SIGNAL_INTERRUPT CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x707, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPBTESTTOOL_WAIT_ON_INTERRUPT                                                                            \
	CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x708, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPBTESTTOOL_SEQUENCE CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x709, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPBTESTTOOL_GPIO CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x710, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPBTESTTOOL_INTENABLE CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x711, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPBTESTTOOL_INTDISABLE CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x712, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPBTESTTOOL_GPIO_SPECIAL CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x713, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPBTESTTOOL_DELAYEDWRITE CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x714, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPBTESTTOOL_DELAYEDWRITESTATUS                                                                           \
	CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x715, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPBTESTTOOL_DELAYEDWRITECANCEL                                                                           \
	CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x716, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPBTESTTOOL_PDI CTL_CODE(FILE_DEVICE_SPB_PERIPHERAL, 0x717, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define STRCPY_S(dest, sz, src) strcpy_s(dest, sz, src)
#define STRNCPY_S(dest, src, sz) strncpy_s(dest, sz, src, _TRUNCATE)
#define STRCASECMP _stricmp
#define THREAD_RET DWORD WINAPI
#define GETOPT_LONG getopt_long
#define no_argument 0
#define required_argument 1
#define optional_argument 2
#define GETGFXFWSTATUS(meiPath) GfxFwStatus::NORMAL
#define PRIVILEGECHECK() true
#define SETENV(name, value) _putenv_s(name, value)
#define MSLEEP(ms) Sleep(ms)
#define GETCH (char)_getch
#define GET_LOCAL_CPUS(bdf) getLocalCpus(bdf)
#define GET_CPU_LIST(bdf) getCpuList(bdf)
#define GET_TOPOLOGY(bdf, e) 0
#define GETLOGS(f) 0
#define GETDRMPATH(bdf) ""

typedef DWORD(WINAPI *funcptr)(void *input_params);
extern char *optarg;
extern int optind;

int getopt(int argc, char *argv[], char *optstring);
int getopt_long(int argc, char *const argv[], const char *optstring, const struct option *longopts, int *longindex);
void *align_alloc(size_t size);
thread_id *create_thread(funcptr thread, void *args);
void wait_for_thread(thread_id *tid);
std::string getProcessName(uint32_t processId);
std::string getLocalCpus(const std::string &bdf);
std::string getCpuList(const std::string &bdf);
std::string timestamp();
int amcCardDiscovery(std::vector<amcCardInfo> *amcDeviceList);

#endif
