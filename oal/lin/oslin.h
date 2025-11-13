/*
 *
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
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
#include <osvf.h>

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
#define CHECKPERMISSION() checkPermission()
#define CHECKPROCESSEXCLUSIVE(processId) checkProcessExclusive(processId)
#define CREATEVFS(deviceInfoPtr) linCreateVFs(deviceInfoPtr)
#define REMOVEVFS(deviceInfoPtr) removeAllVFs(deviceInfoPtr)
#define LISTVFS(deviceInfoPtr, result) linListVFs(deviceInfoPtr, result)
#define VMXSUPPORT() isVmxSupported()
#define IOMMUSUPPORT() isIommuSupported()
#define SRIOVSUPPORT(deviceInfoPtr) isSriovSupported(deviceInfoPtr)
#define CHECKMEDIACODEC(bdfStr, functionalCheck, finalResult) checkMediaCodec(bdfStr, functionalCheck, finalResult)

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
bool checkPermission();
bool checkProcessExclusive(uint32_t processId);
std::string getDrmPath(const std::string &bdf);
int linCreateVFs(DeviceSriovInfo *di);
int removeAllVFs(DeviceSriovInfo *di);
int linListVFs(DeviceSriovInfo *di, std::vector<DeviceSriovInfo> &result);
bool isVmxSupported();
bool isIommuSupported();
bool isSriovSupported(DeviceSriovInfo *di);
bool checkMediaCodec(std::string &bdfStr, bool functionalCheck, std::string &finalResult);

#endif
