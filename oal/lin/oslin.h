/*
 *
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _OSLIN_H
#define _OSLIN_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <getopt.h>
#include <pthread.h>
#include <string>
#include <unistd.h>
#include <sys/time.h>
#include <vector>
#include <osvf.h>
#include "topology.h"

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

#define LIBXPUM_API
#define STRCPY_S(dest, sz, src) snprintf((dest), (sz), "%s", (src))
#define STRNCPY_S(dest, src, sz) snprintf((dest), (sz), "%s", (src))
#define STRCASECMP strcasecmp
#define THREAD_RET void *
#define GETOPT_LONG getopt_long
#define GETGFXFWSTATUS(meiPath) getGfxFwStatus(meiPath)
#define PRIVILEGECHECK() privilegeCheck()
#define ZERO_MEM(mem, size) memset(mem, 0, size)
#define SETENV(name, value) setenv(name, value, 1)
#define MSLEEP(ms) usleep(ms * 1000) // Convert milliseconds to microseconds
#define GETCH getch
#define RESTORE_TERMINAL() restoreTerminal()
#define STDIN_ISATTY() isatty(STDIN_FILENO)
#define GET_LOCAL_CPUS(bdf) getLocalCpus(bdf)
#define GET_CPU_LIST(bdf) getCpuList(bdf)
#define GET_TOPOLOGY getTopology
#define EXPORT_TOPOLOGY_XML exportTopologyToXml
inline auto GET_SYSTEM_NICS(const SysfsPaths &paths = {})
{
	return discoverNics(paths);
} // NOLINT(readability-identifier-naming) // Match MACRO style while providing a better interface for navigation
inline auto GET_NUMA_NODES(const std::vector<std::string> &bdfs)
{
	return getNumaNodes(bdfs);
} // NOLINT(readability-identifier-naming) // Match MACRO style while providing a better interface for navigation
inline auto GET_PCIE_PATHS(const std::vector<std::string> &bdfs)
{
	return getPciePaths(bdfs);
} // NOLINT(readability-identifier-naming) // Match MACRO style while providing a better interface for navigation
typedef wchar_t TCHAR;
#define GETLOGS(f) getLinLogs(f)
#define GETDRMPATH(bdf) getDrmPath(bdf)
#define CHECKPERMISSION() checkPermission()
#define CHECKPROCESSEXCLUSIVE(processId) checkProcessExclusive(processId)
#define CHECKCPUSTATUS() checkCPUStatus()
#define CREATEVFS(deviceInfoPtr) linCreateVFs(deviceInfoPtr)
#define REMOVEVFS(deviceInfoPtr) removeAllVFs(deviceInfoPtr)
#define LISTVFS(deviceInfoPtr, result) linListVFs(deviceInfoPtr, result)
#define VMXSUPPORT() isVmxSupported()
#define IOMMUSUPPORT() isIommuSupported()
#define SRIOVSUPPORT(deviceInfoPtr) isSriovSupported(deviceInfoPtr)
#define CHECKMEDIACODEC(bdfStr, functionalCheck, finalResult) checkMediaCodec(bdfStr, functionalCheck, finalResult)
#define CHECKHOSTMEMORYSIZE(hostMemorySize) checkHostMemorySize(hostMemorySize)
#define CHECKPCIELINKSTATUS(bdf) checkPCIeLinkStatus(bdf)
#define GETDOWNGRADEDPCIEINFO(bdfStr) getDowngradedPCIeInfo(bdfStr);
#define GETKERNELVERSION() getKernelVersion()
#define GETPCISLOTLABEL(bdf) getPciSlotLabel(bdf)
#define FINDRESOURCEFILE(relativePath) findResourceFile(relativePath)

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
void restoreTerminal();
std::string timestamp();
std::string getLocalCpus(const std::string &bdf);
std::string getCpuList(const std::string &bdf);
int getTopology(bdfID bdf, std::string *switchDevicePath);
int amcCardDiscovery(std::vector<amcCardInfo> *amcDeviceList);
int getLinLogs(const std::string &fileName);
bool checkPermission();
std::string checkCPUStatus();
bool checkProcessExclusive(uint32_t processId);
std::string getDrmPath(const std::string &bdf);
int linCreateVFs(DeviceSriovInfo *di);
int removeAllVFs(DeviceSriovInfo *di);
int linListVFs(DeviceSriovInfo *di, std::vector<DeviceSriovInfo> &result);
bool isVmxSupported();
bool isIommuSupported();
bool isSriovSupported(DeviceSriovInfo *di);
bool checkMediaCodec(std::string &bdfStr, bool functionalCheck, std::string &finalResult);
bool checkHostMemorySize(uint64_t *hostMemorySize);
bool checkPCIeLinkStatus(std::string &bdf);
std::string getKernelVersion();
std::string getPciSlotLabel(const std::string &bdf);
std::string getDowngradedPCIeInfo(std::string &bdfStr);
std::string findResourceFile(const std::string &relativePath);
int coldResetViaSysfs(const std::string &gpuBdf);
std::vector<uint32_t> getGpuProcessesByBdf(const std::string &gpuBdf);
std::vector<std::string> getDevicesSharingSlotWith(const std::string &gpuBdf);

#endif
