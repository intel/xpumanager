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
#include <cstdlib>
#include <stdlib.h>
#include <cstdio>
#include <cstring>
#include <getopt.h> // NOLINT(misc-include-cleaner)
#include <pthread.h>
#include <string>
#include <unistd.h>
#include <sys/time.h>
#include <vector>
#include <osvf.h>
#include "topology.h"
#include "crashlog_lin.h"

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
// True only when stdin is an interactive terminal AND this process is in the foreground
// process group of that terminal.  Unlike STDIN_ISATTY(), this returns false for
// backgrounded processes, preventing tcsetattr / GETCH calls from triggering SIGTTOU.
#define STDIN_IS_FOREGROUND() (isatty(STDIN_FILENO) && (tcgetpgrp(STDIN_FILENO) == getpgrp()))
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
#define CRASHLOG_AVAILABLE() linCrashlogAvailable()
#define CRASHLOG_LIST_SOURCES(bdfs, out) linCrashlogListSources(bdfs, out)
#define CRASHLOG_CONTROL(verb, bdf, out) linCrashlogControl(verb, bdf, out)
#define CRASHLOG_EXTRACT(bdf, dir, files, out) linCrashlogExtract(bdf, dir, files, out)
#define CRASHLOG_DECODE(in, json, out) linCrashlogDecode(in, json, out)
#define GETDRMPATH(bdf) getDrmPath(bdf)
#define CREATEVFS(deviceInfoPtr) linCreateVFs(deviceInfoPtr)
#define REMOVEVFS(deviceInfoPtr) removeAllVFs(deviceInfoPtr)
#define LISTVFS(deviceInfoPtr, result) linListVFs(deviceInfoPtr, result)
#define VMXSUPPORT() isVmxSupported()
#define IOMMUSUPPORT() isIommuSupported()
#define SRIOVSUPPORT(deviceInfoPtr) isSriovSupported(deviceInfoPtr)
#define GETKERNELVERSION() getKernelVersion()
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define GETPCISLOTLABEL(bdf) getPciSlotLabel(bdf)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FINDRESOURCEFILE(relativePath) findResourceFile(relativePath)

inline bool hasEnv(const char *name) { return secure_getenv(name) != nullptr; } // NOLINT(misc-include-cleaner)

// NOLINTNEXTLINE(readability-identifier-naming)
static inline int fopen_s_def(FILE **pFile, const char *filename, const char *mode)
{
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
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
struct bdfID;
struct amcCardInfo;
int getTopology(bdfID bdf, std::string *switchDevicePath);
int amcCardDiscovery(std::vector<amcCardInfo> *amcDeviceList);
int getLinLogs(const std::string &fileName);
std::string getDrmPath(const std::string &bdf);
int linCreateVFs(DeviceSriovInfo *di);
int removeAllVFs(DeviceSriovInfo *di);
int linListVFs(DeviceSriovInfo *di, std::vector<DeviceSriovInfo> &result);
bool isVmxSupported();
bool isIommuSupported();
bool isSriovSupported(DeviceSriovInfo *di);
std::string getKernelVersion();
bool isLgciXeDebugKernel(const std::string &release);
bool euMetricsSafeOnThisKernel(std::string *unsafeKernelRelease = nullptr);
std::string getPciSlotLabel(const std::string &bdf);
std::string findResourceFile(const std::string &relativePath);
int coldResetViaSysfs(const std::string &gpuBdf);
std::vector<uint32_t> getGpuProcessesByBdf(const std::string &gpuBdf);
std::vector<std::string> getDevicesSharingSlotWith(const std::string &gpuBdf);

#endif
