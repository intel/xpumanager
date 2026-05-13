/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _OSWIN_H
#define _OSWIN_H

#define NOMINMAX
#include <string>
#include <windows.h>
#include <process.h>
#include <conio.h>
#include <io.h>
#include <vector>
#include <optional>
#include <unordered_map>

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

/**
 * @brief Convert a wide string (UTF-16LE) to a UTF-8 std::string.
 *
 * Intended for logging only — wide-character values must not be passed
 * directly to the logger's narrow std::format strings.
 *
 * @param ws Wide-character string to convert.  May be null; returns "(null)".
 * @return UTF-8 encoded std::string.
 */
[[nodiscard]] inline std::string wtos(const wchar_t *ws)
{
	if (!ws)
		return "(null)";
	if (*ws == L'\0')
		return {};
	const int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
	if (len <= 0)
		return "(invalid wide string)";
	std::string out(static_cast<std::size_t>(len - 1), '\0');
	WideCharToMultiByte(CP_UTF8, 0, ws, -1, out.data(), len, nullptr, nullptr);
	return out;
}
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
#define RESTORE_TERMINAL()
#define STDIN_ISATTY() _isatty(_fileno(stdin))
#define GET_LOCAL_CPUS(bdf) (UNUSED_VAR(bdf), std::string(""))
#define GET_CPU_LIST(bdf) (UNUSED_VAR(bdf), std::string(""))
#define GET_TOPOLOGY(bdf, e) (UNUSED_VAR(bdf), UNUSED_VAR(e), 0)
#define EXPORT_TOPOLOGY_XML(filename, gpuDevices) (UNUSED_VAR(filename), UNUSED_VAR(gpuDevices), 0)
#define GETLOGS(f) 0
#define CHECKPERMISSION() 0
#define CHECKPROCESSEXCLUSIVE(processId) (UNUSED_VAR(processId), false)
static constexpr std::string CHECKCPUSTATUS() { return std::string{"0"}; }
#define CHECKMEDIACODEC(bdfStr, functionalCheck, finalResult)                                                          \
	(UNUSED_VAR(bdfStr), UNUSED_VAR(functionalCheck), UNUSED_VAR(finalResult), false)
#define GETDRMPATH(bdf) ""
#define CREATEVFS(deviceInfoPtr) (UNUSED_VAR(deviceInfoPtr), 0)
#define REMOVEVFS(deviceInfoPtr) (UNUSED_VAR(deviceInfoPtr), 0)
#define LISTVFS(deviceInfoPtr, result) (UNUSED_VAR(deviceInfoPtr), UNUSED_VAR(result), 0)
#define VMXSUPPORT() 0
#define IOMMUSUPPORT() 0
#define SRIOVSUPPORT(deviceInfoPtr) (UNUSED_VAR(deviceInfoPtr), 0)
#define CHECKHOSTMEMORYSIZE(hostMemorySize) (UNUSED_VAR(hostMemorySize), 0)
#define CHECKPCIELINKSTATUS(bdf) (UNUSED_VAR(bdf), false)
#define GETKERNELVERSION() std::string("")
#define GETPCISLOTLABEL(bdf) (UNUSED_VAR(bdf), std::string(""))
static constexpr std::string FINDRESOURCEFILE(UNUSED const std::string &relativePath) { return std::string{}; }
static constexpr std::string GETDOWNGRADEDPCIEINFO(UNUSED const std::string &bdfStr) { return std::string{}; }
static inline int coldResetViaSysfs(UNUSED const std::string &gpuBdf) { return -1; }
static inline std::vector<uint32_t> getGpuProcessesByBdf(UNUSED const std::string &gpuBdf) { return {}; }
static inline std::vector<std::string> getDevicesSharingSlotWith(UNUSED const std::string &gpuBdf) { return {}; }

typedef DWORD(WINAPI *funcptr)(void *input_params);
extern char *optarg;
extern int optind;

// Topology types (Linux implementations not available on Windows)
struct NicInfo
{
	std::string name;
	std::string bdfAddress;
	std::string cpuAffinity;
};

// Forward declaration for topology (command not available on Windows)
struct GpuDeviceInfo
{
	int deviceIndex;
	std::string bdfAddress;
	std::string cpuAffinity;
};

inline std::optional<std::vector<NicInfo>> GET_SYSTEM_NICS() { return std::nullopt; }
inline std::unordered_map<std::string, std::optional<int>> GET_NUMA_NODES(const std::vector<std::string> &)
{
	return {};
} // NOLINT(readability-identifier-naming) // Match MACRO style while providing a better interface for navigation
struct PcieBridgeLink
{
	std::string bdf{};
	bool isHostBridge{};
};
using PciePath = std::vector<PcieBridgeLink>;
inline std::unordered_map<std::string, PciePath> GET_PCIE_PATHS(const std::vector<std::string> &)
{
	return {};
} // NOLINT(readability-identifier-naming) // Match MACRO style while providing a better interface for navigation

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
