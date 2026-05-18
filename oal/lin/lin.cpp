/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <os.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <debug.h>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <grp.h>
#include <iostream>
#include <pwd.h>
#include <spawn.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <syncstream>
#include <termios.h>
#include <unistd.h>
#include <vector>

#include "lin.h"
#include "pci_database.h"
#include "dmi_reader.h"
#include "bdf.h"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include <hwloc.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/**
 * @brief A class to encapsulate the result of executing a system command
 *
 * This class stores both the output and exit status of a system command
 * execution, providing a clean interface to access command results.
 * It's used by the execCommand function to return structured results.
 */

/**
 * @brief Execute a system command and capture its output
 *
 * Spawns /bin/sh -c to execute the command string via posix_spawn.
 * The shell is still in the execution path, so callers must ensure command
 * components are not derived from untrusted input. All current call sites
 * use hardcoded paths and hardware-derived values (hex BDF addresses).
 *
 * @param command The shell command string to execute
 * @return SystemCommandResult containing the command output and exit status
 */
SystemCommandResult execCommand(const std::string &command)
{
	std::array<int, 2> pipeFds{};
	if (pipe(pipeFds.data()) != 0) {
		return {"Failed to create pipe", -1};
	}

	posix_spawn_file_actions_t actions;
	const int initResult = posix_spawn_file_actions_init(&actions);
	int fileActionsResult = initResult;
	if (fileActionsResult == 0) {
		fileActionsResult = posix_spawn_file_actions_adddup2(&actions, pipeFds[1], STDOUT_FILENO);
	}
	if (fileActionsResult == 0) {
		fileActionsResult = posix_spawn_file_actions_adddup2(&actions, pipeFds[1], STDERR_FILENO);
	}
	if (fileActionsResult == 0) {
		fileActionsResult = posix_spawn_file_actions_addclose(&actions, pipeFds[0]);
	}
	if (fileActionsResult == 0) {
		fileActionsResult = posix_spawn_file_actions_addclose(&actions, pipeFds[1]);
	}
	if (fileActionsResult != 0) {
		if (initResult == 0) {
			posix_spawn_file_actions_destroy(&actions);
		}
		close(pipeFds[0]);
		close(pipeFds[1]);
		return {"posix_spawn file actions setup failed", -1};
	}

	// posix_spawn requires a mutable argv array
	char shPath[] = "/bin/sh";
	char shFlag[] = "-c";
	std::string cmdCopy = command;
	char *argv[] = {shPath, shFlag, cmdCopy.data(), nullptr}; // NOLINT(cppcoreguidelines-avoid-c-arrays)

	pid_t pid = -1;
	const int spawnResult = posix_spawn(&pid, "/bin/sh", &actions, nullptr, argv, environ);
	posix_spawn_file_actions_destroy(&actions);

	close(pipeFds[1]);

	if (spawnResult != 0) {
		close(pipeFds[0]);
		return {"posix_spawn failed", -1};
	}

	std::string result;
	constexpr size_t kReadBufferSize = 65536;
	std::vector<char> buffer(kReadBufferSize);
	bool readError = false;
	while (true) {
		const ssize_t bytesRead = read(pipeFds[0], buffer.data(), buffer.size());
		if (bytesRead > 0) {
			result.append(buffer.data(), static_cast<size_t>(bytesRead));
		} else if (bytesRead == 0) {
			break;
		} else if (errno != EINTR) {
			ERR("read() failed during command execution: %s\n", strerror(errno));
			readError = true;
			break;
		}
	}
	close(pipeFds[0]);

	int status = 0;
	pid_t waitResult = -1;
	do {
		waitResult = waitpid(pid, &status, 0);
	} while (waitResult < 0 && errno == EINTR);
	if (waitResult < 0) {
		ERR("waitpid failed: %s\n", strerror(errno));
		return {result, -1};
	}
	const int exitcode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

	return {result, readError ? -1 : exitcode};
}

/**
 * @brief Gets a single character from standard input without echo
 *
 * This function reads a single character from standard input without waiting
 * for Enter and without echoing the character to the terminal. It temporarily
 * modifies terminal attributes to achieve this behavior.
 *
 * @return char The character read from standard input
 */
char getch()
{
	char buf = 0;
	struct termios old = {};
	if (tcgetattr(STDIN_FILENO, &old) < 0)
		perror("tcsetattr()");
	old.c_lflag &= ~ICANON;
	old.c_lflag &= ~ECHO;
	old.c_cc[VMIN] = 1;
	old.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &old) < 0)
		perror("tcsetattr ICANON");
	if (read(STDIN_FILENO, &buf, 1) < 0)
		perror("read()");
	old.c_lflag |= ICANON;
	old.c_lflag |= ECHO;
	if (tcsetattr(STDIN_FILENO, TCSADRAIN, &old) < 0)
		perror("tcsetattr ~ICANON");
	return buf;
}

/**
 * @brief Restores terminal settings to canonical mode with echo enabled
 *
 * This function ensures the terminal is returned to its normal state,
 * particularly useful after detaching input threads that may have left
 * the terminal in raw mode. Only operates on TTY devices.
 */
void restoreTerminal()
{
	if (isatty(STDIN_FILENO)) {
		struct termios term;
		if (tcgetattr(STDIN_FILENO, &term) == 0) {
			term.c_lflag |= ICANON | ECHO;
			tcsetattr(STDIN_FILENO, TCSANOW, &term);
		}
	}
}

/**
 * @brief Creates a new thread.
 *
 * @param thread Function pointer to the thread function.
 * @param args Arguments to pass to the thread function.
 * @return A pointer to the thread_id object representing the created thread.
 */
thread_id *create_thread(funcptr thread, void *args)
{
	pthread_t threadHdl;
	pthread_create(&threadHdl, NULL, thread, args);

	DBG("{}: thread handle is {}\n", __func__, threadHdl);
	thread_id *newThreadId = new thread_id(threadHdl);
	return newThreadId;
}

/**
 * @brief Waits for the specified thread to complete.
 *
 * @param tid Pointer to the thread_id object representing the thread to wait for.
 */
void wait_for_thread(thread_id *tid)
{
	if (tid->ret_thread_uid()) {
		DBG("{}: thread handle is {}\n", __func__, tid->ret_thread_uid());
		pthread_join(tid->ret_thread_uid(), NULL);
		delete tid; // Clean up the thread_id object after waiting
	}
}

/**
 * @brief Checks if the current user has privilege to access XPUM resources
 *
 * This function verifies whether the current user has the necessary privileges
 * to access XPUM resources. It checks if the user is root or belongs to the
 * 'xpum' group, which grants the required permissions.
 *
 * @return bool True if user has privileges, false otherwise
 */
bool privilegeCheck()
{
	uid_t uid = getuid();
	if (uid == 0) {
		return true;
	}
	struct passwd *pw = getpwuid(uid);
	if (pw == NULL) {
		ERR("getpwuid error\n");
		return false;
	}
	int ngroups = 0;
	getgrouplist(pw->pw_name, pw->pw_gid, NULL, &ngroups);
	if (ngroups == 0) {
		return false;
	}
	std::vector<gid_t> groups(ngroups);
	int rc = getgrouplist(pw->pw_name, pw->pw_gid, groups.data(), &ngroups);
	if (rc < 0) {
		ERR("getgrouplist ngroups is too small\n");
		return false;
	}
	std::string xpumGrp("xpum");
	bool hasPrivilege = false;
	for (int i = 0; i < ngroups; i++) {
		struct group *gr = getgrgid(groups[i]);
		if (gr == NULL) {
			ERR("getgrgid error\n");
			return false;
		}
		std::string grpName(gr->gr_name);
		if (grpName == xpumGrp) {
			hasPrivilege = true;
		}
	}

	return hasPrivilege;
}

/**
 * @brief Retrieves the process name for a given process ID
 *
 * This function reads the command line information from the Linux /proc filesystem
 * to obtain the process name associated with the specified process ID. It provides
 * process identification capabilities for system monitoring and management operations.
 *
 * @param processId 32-bit unsigned integer representing the target process ID
 * @return std::string Process name if found, empty string if process not found or error occurs
 */
std::string getProcessName(uint32_t processId)
{
	std::string processName = "";
	std::ifstream pinfo;
	std::string path = std::format("/proc/{}/cmdline", processId);
	pinfo.open(path);
	if (pinfo.is_open()) {
		std::getline(pinfo, processName);
		pinfo.close();
	}
	return processName;
}

/**
 * @brief Generates a timestamp string for logging and diagnostic purposes
 *
 * This function creates a formatted timestamp string using the current system
 * time. It provides consistent time formatting for logging, debugging, and
 * diagnostic operations throughout the XPUM system.
 *
 * @return std::string Formatted timestamp string representing current system time
 */
std::string timestamp()
{
	std::string timestampStr = "";
	time_t now;
	struct tm *timeInfo;

	// Get current time in seconds since the epoch
	now = time(NULL);

	timeInfo = localtime(&now);
	if (timeInfo == NULL) {
		ERR("localtime failed\n");
		return timestampStr;
	}

	struct timeval tv;
	if (gettimeofday(&tv, NULL) == -1) {
		ERR("gettimeofday failed\n");
		return timestampStr;
	}
	timestampStr = std::format("{:02d}:{:02d}:{:02d}.{:03d}", timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec,
							   (int)tv.tv_usec / 1000);
	return timestampStr;
}

/**
 * @brief Reads a line from a sysfs file for a given PCI device
 *
 * This function constructs a path to a sysfs file for a specific PCI device
 * using the Bus:Device:Function (BDF) address and a suffix, then reads the
 * first line from that file. It provides generic sysfs access capabilities
 * for PCI device information retrieval.
 *
 * @param bdf String containing the PCI Bus:Device:Function address
 * @param suffix String containing the sysfs file suffix to append to the device path
 * @return std::string Content of the first line from the sysfs file, empty string if file cannot be read
 */
std::string getSysfsLine(const std::string &bdf, const std::string &suffix)
{
	TRACING();
	if (!isValidBdf(bdf)) {
		ERR("Rejecting suspicious BDF address: {}\n", bdf.c_str());
		return "";
	}
	std::string affinity = "";
	std::ifstream infile;
	std::string file = std::string("/sys/bus/pci/devices/") + bdf + suffix;

	infile.open(file.data());
	if (infile.is_open()) {
		std::getline(infile, affinity);
	}

	infile.close();
	return affinity;
}

/**
 * @brief Retrieves the local CPU affinity mask for a PCI device
 *
 * This function reads the local_cpus sysfs attribute for a given PCI device,
 * which contains a CPU affinity mask indicating which CPUs are local to the
 * device. It provides NUMA topology information for optimal CPU-device affinity.
 *
 * @param bdf String containing the PCI Bus:Device:Function address
 * @return std::string CPU affinity mask as hexadecimal string, empty string if not available
 */
std::string getLocalCpus(const std::string &bdf)
{
	TRACING();
	return getSysfsLine(bdf, "/local_cpus");
}

/**
 * @brief Retrieves the local CPU list for a PCI device
 *
 * This function reads the local_cpulist sysfs attribute for a given PCI device,
 * which contains a human-readable list of CPU IDs that are local to the device.
 * It provides NUMA topology information in a more readable format than the CPU mask.
 *
 * @param bdf String containing the PCI Bus:Device:Function address
 * @return std::string Comma-separated list of local CPU IDs, empty string if not available
 */
std::string getCpuList(const std::string &bdf)
{
	TRACING();
	return getSysfsLine(bdf, "/local_cpulist");
}

/**
 * @brief Checks if a hardware object represents a PCIe switch device
 *
 * This function determines whether the given hwloc object represents a PCIe switch
 * by looking up its vendor ID and device ID in the PCI device database. PCIe switches
 * are used to expand the number of PCIe slots and enable complex topologies.
 *
 * @param obj Hardware locality object representing a PCI device
 * @return bool True if the object represents a PCIe switch device, false otherwise
 */
bool isSwitchDevice(hwloc_obj_t obj)
{
	int vendorId = obj->attr->pcidev.vendor_id;
	int deviceId = obj->attr->pcidev.device_id;
	const PcieDevice *pDevice = PciDatabase::instance().getDevice(vendorId, deviceId);
	return (pDevice != nullptr);
}

/**
 * @brief Counts the number of PCIe switches in the path to a device
 *
 * This function traverses the hardware topology hierarchy from a given PCI device
 * up to the root complex, counting the number of PCIe switches encountered along
 * the path. It helps determine the complexity of the device's connectivity path.
 *
 * @param pcidev Hardware locality object representing the target PCI device
 * @return int Number of PCIe switches between the device and root complex
 */
int getSwitchCount(hwloc_obj_t pcidev)
{
	hwloc_obj_t obj = pcidev->parent;
	int count = 0;
	uint32_t preVendorId = -1, preDeviceId = -1;
	while (obj != nullptr) {
		if (obj->type == HWLOC_OBJ_BRIDGE) {
			/* only host->pci and pci->pci bridge supported so far */
			if (obj->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_HOST) {
				assert(obj->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);
				obj = obj->parent;
				continue;
			} else {
				assert(obj->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_PCI);
				assert(obj->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);

				if (preVendorId == obj->attr->bridge.upstream.pci.vendor_id &&
					preDeviceId == obj->attr->bridge.upstream.pci.device_id) {
					obj = obj->parent;
					continue;
				}
				if (isSwitchDevice(obj)) {
					preVendorId = obj->attr->bridge.upstream.pci.vendor_id;
					preDeviceId = obj->attr->bridge.upstream.pci.device_id;
					count++;
					DBG("Found Switch count {}.\n", count);
				}
			}
		} else {
			DBG("Unknown hwloc-obj type  {}.\n", obj->type);
		}
		obj = obj->parent;
	}
	return count;
}

/**
 * @brief Converts a hardware object's PCI information to a BDF string representation
 *
 * This function extracts the PCI Bus:Device:Function (BDF) information from a
 * hardware locality object and formats it as a standardized string in the format
 * "DDDD:BB:DD.F" where D=domain, B=bus, D=device, F=function (all in hexadecimal).
 *
 * @param obj Hardware locality object containing PCI device information
 * @return std::string Formatted BDF string representation
 */
std::string pci2RegxString(hwloc_obj_t obj)
{
	std::ostringstream os;
	os << std::setfill('0') << std::setw(4) << std::hex << (uint32_t)obj->attr->pcidev.domain << std::string(":")
	   << std::setw(2) << (uint32_t)obj->attr->pcidev.bus << std::string(":") << std::setw(2)
	   << (uint32_t)obj->attr->pcidev.dev << std::string(".") << (uint32_t)obj->attr->pcidev.func;
	return os.str();
}

/**
 * @brief Finds the sysfs device path for a given BDF address
 *
 * This function searches the Linux sysfs filesystem (/sys/devices) to locate
 * the device directory corresponding to a specific PCI Bus:Device:Function address.
 * It performs a recursive search through the device hierarchy to find the matching path.
 *
 * @param bdf_address String containing the PCI BDF address to search for
 * @return std::string Full path to the device directory in sysfs, empty string if not found
 */
std::string getDevicePath(const std::string &bdfAddress)
{
	std::string result;
	namespace stdfs = std::filesystem;

	try {
		for (stdfs::recursive_directory_iterator iter{"/sys/devices", stdfs::directory_options::skip_permission_denied};
			 iter != stdfs::recursive_directory_iterator{}; ++iter) {
			// Match on the exact path component, not a substring, to prevent
			// partial BDF collisions (e.g. "0000:02:00.0" vs "0000:02:00.00")
			if (stdfs::is_directory(*iter) && iter->path().filename().string() == bdfAddress) {
				result = iter->path().string();
				break;
			}
		}
	} catch (const stdfs::filesystem_error &e) {
		ERR("Error searching /sys/devices for {}: {}\n", bdfAddress.c_str(), e.what());
	}

	return result;
}

/**
 * @brief Retrieves the device path for the first PCIe switch in the topology
 *
 * This function traverses the hardware topology hierarchy from a given PCI device
 * up towards the root complex and finds the sysfs device path for the first PCIe
 * switch encountered. This is useful for accessing switch-specific information
 * and configuration.
 *
 * @param par_obj Hardware locality object representing the starting PCI device
 * @param switchDevicePath Pointer to string where the switch device path will be stored
 */
void getSwitchDevicePath(hwloc_obj_t parObj, std::string *switchDevicePath)
{
	hwloc_obj_t obj = parObj->parent;
	uint32_t preVendorId = -1, preDeviceId = -1;
	while (obj != nullptr) {
		if (obj->type == HWLOC_OBJ_BRIDGE) {
			/* only host->pci and pci->pci bridge supported so far */
			if (obj->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_HOST) {
				assert(obj->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);
				obj = obj->parent;
				continue;
			} else {
				assert(obj->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_PCI);
				assert(obj->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);
				if (preVendorId == obj->attr->bridge.upstream.pci.vendor_id &&
					preDeviceId == obj->attr->bridge.upstream.pci.device_id) {
					obj = obj->parent;
					continue;
				}
				if (isSwitchDevice(obj)) {
					preVendorId = obj->attr->bridge.upstream.pci.vendor_id;
					preDeviceId = obj->attr->bridge.upstream.pci.device_id;
					std::string address = pci2RegxString(obj);
					if (address.length() > 0) {
						std::string path = getDevicePath(address);
						if (path.length() > 0) {
							*switchDevicePath = path;
						}
					}
				}
			}
		} else {
			ERR("Unknown hwloc-obj type  {}.\n", obj->type);
		}
		obj = obj->parent;
	}
}

/**
 * @brief Analyzes system topology and counts PCIe switches for a specific device
 *
 * This function initializes the hardware locality (hwloc) topology, searches for
 * a specific PCI device using its domain:bus:device:function coordinates, and
 * counts the number of PCIe switches in the path from that device to the root
 * complex. It provides topology analysis capabilities for performance optimization.
 *
 * @param bdf BDF ID structure containing the PCI coordinates of the target device
 * @return int Number of PCIe switches in the path to the specified device
 */
int getTopology(bdfID bdf, std::string *switchDevicePath)
{
	// Initialize the topology object
	hwloc_topology_t topology;
	SETENV("HWLOC_COMPONENTS", "linux,stop");
	hwloc_topology_init(&topology);
	hwloc_topology_set_io_types_filter(topology, HWLOC_TYPE_FILTER_KEEP_ALL);
	hwloc_topology_load(topology);
	int switchCount = 0;

	// Get the first PCI device
	hwloc_obj_t pcidev = hwloc_get_next_pcidev(topology, nullptr);

	// Iterate over all PCI devices
	while (pcidev != nullptr) {

		if (pcidev->attr->pcidev.domain == bdf.domain && pcidev->attr->pcidev.bus == bdf.bus &&
			pcidev->attr->pcidev.dev == bdf.device && pcidev->attr->pcidev.func == bdf.function) {
			switchCount = getSwitchCount(pcidev);
			if (switchCount > 0) {
				getSwitchDevicePath(pcidev, switchDevicePath);
			}
			break;
		}

		// Get the next PCI device
		pcidev = hwloc_get_next_pcidev(topology, pcidev);
	}

	// Destroy the topology object
	hwloc_topology_destroy(topology);
	return switchCount;
}

/**
 * @brief Gets the DRM device path for a specific BDF
 *
 * This function searches the /sys/class/drm directory to find the DRM device
 * path that corresponds to the given PCI Bus:Device:Function (BDF) address.
 * It looks for DRM card entries and matches their associated PCI device to the
 * specified BDF.
 *
 * @param bdf BDF string (e.g., "0000:02:00.0")
 * @return std::string DRM device path (e.g., "/dev/dri/card0")
 */
std::string getDrmPath(const std::string &bdf)
{
	// Go through /sys/class/drm to find all card that do not have a '-'
	// in their names (e.g. card0, card1, etc.)
	std::string drmPath = "";
	try {
		for (const auto &entry : std::filesystem::directory_iterator("/sys/class/drm")) {
			std::string cardName = entry.path().filename().string();
			// cardName must start with "card"
			if (cardName.find("card") != 0) {
				continue;
			}
			// and cardName must not contain a '-'
			if (cardName.find('-') == std::string::npos) {
				// Found a card without a '-' in its name
				drmPath = entry.path().string();

				// Once found, go in this directory and get the basename of the "device" symlink
				std::string deviceLink = drmPath + "/device";
				if (std::filesystem::exists(deviceLink) && std::filesystem::is_symlink(deviceLink)) {
					std::string devicePath = std::filesystem::read_symlink(deviceLink).string();
					std::string deviceBdf = std::filesystem::path(devicePath).filename().string();
					if (deviceBdf == bdf) {
						// Return /dev/dri/cardX
						drmPath = "/dev/dri/" + cardName;
						return drmPath;
					}
				}
			}
		}
	} catch (const std::filesystem::filesystem_error &e) {
		// Handle filesystem errors (e.g. permission denied)
		ERR("Error accessing /sys/class/drm: {}\n", e.what());
	}

	return drmPath;
}

/**
 * @brief Sets the progress of a firmware update operation.
 *
 * This function updates the progress of a firmware update operation by printing
 * the progress to the console. It uses ANSI escape codes to control the cursor
 * position in the terminal.
 *
 * @param devIndex The device index.
 * @param lineNum The line number to update (1-based).
 * @param totalThreads The total number of threads (or steps) in the update process.
 * @param progress The current progress percentage (0-100).
 */
void setProgress(int devIndex, int lineNum, int totalThreads, uint32_t progress)
{
	TRACING();
	std::lock_guard<std::mutex> lock(progressPrintMutex);

	// Use std::osyncstream for thread-safe output (C++20)
	std::osyncstream syncOut(std::cout);

	// POSIX/Linux ANSI escape sequences
	// Save cursor position
	syncOut << "\033[s";
	// Move cursor up to the correct line
	// We need to move up (totalThreads - lineNum) lines
	int linesUp = totalThreads - lineNum;
	syncOut << "\033[" << linesUp << "A\r";
	syncOut << "Firmware progress for device " << devIndex << ": ";
	for (int j = 0; j < (int)progress; ++j) {
		syncOut << "#";
	}
	syncOut << " " << progress << "%";
	// Restore cursor position
	syncOut << "\033[u";
	syncOut.flush();
}

/**
 * @brief Find a resource file using an explicit allow-list of search roots
 *
 * Searches the following locations in order; no other paths are ever tried:
 *   1. Relative to the current working directory  (in-tree / development builds)
 *   2. Relative to the running executable          (portable / staged installs)
 *   3. Under XPUM_DATA_DIR                         (package-manager installs;
 *                                                   compile-time constant injected
 *                                                   by Meson from datadir/xpum)
 *
 * The exe-relative candidate is canonicalised and checked for containment within
 * the exe directory so that symlinks in the resource tree cannot redirect reads
 * outside that directory.
 *
 * @param relativePath Relative path to the resource (e.g. "resources/config/pci.ids").
 *                     Must not contain ".." components intended to escape a search root.
 * @return Absolute path to the first matching file/directory, or "" if not found.
 *         Callers must treat an empty return as a hard failure.
 */
std::string findResourceFile(const std::string &relativePath)
{
	namespace fs = std::filesystem;
	std::error_code ec;

	// Explicit allow-list of search roots, tried in priority order.
	// If the resource files are not within these locations, they are
	// considered malicious and disregarded.
	// (1) and (2) allow for flexible deployment layouts, while (3) is intended
	// for standard release
	//
	//  1. Current working directory   — development / in-tree builds
	//  2. Executable-relative path    — portable / staged installs
	//  3. XPUM_DATA_DIR               — package-manager install (compile-time constant)

	// --- 1. CWD-relative ---
	fs::path cwdCandidate = fs::current_path(ec) / relativePath;
	if (!ec && fs::exists(cwdCandidate, ec) && !ec) {
		return cwdCandidate.string();
	}

	// --- 2. Exe-relative ---
	try {
		fs::path exeDir = fs::path(fs::read_symlink("/proc/self/exe")).parent_path();
		// operator/ normalises the path
		fs::path candidate = exeDir / relativePath;

		fs::path canonicalCandidate = fs::canonical(candidate, ec);
		fs::path canonicalExeDir = fs::canonical(exeDir, ec);
		if (!ec && fs::exists(canonicalCandidate, ec) && !ec) {
			auto [mismatch_it, _] =
				std::mismatch(canonicalExeDir.begin(), canonicalExeDir.end(), canonicalCandidate.begin());
			if (mismatch_it == canonicalExeDir.end()) {
				return canonicalCandidate.string();
			}
			ERR("findResourceFile: exe-relative path escapes exe directory: {}\n", canonicalCandidate.c_str());
		}
	} catch (...) {
		DBG("Could not read /proc/self/exe to determine executable path\n");
	}

	// --- 3. System data directory (set by Meson at build time) ---
	// Provide a fallback of "" for unconfigured dev builds; if constexpr
	// elides the block when the path is empty so no string literal
	// leaks into the binary or ELF .rodata section.
#ifndef XPUM_DATA_DIR
#define XPUM_DATA_DIR ""
#endif
	static constexpr std::string_view kXpumDataDir = XPUM_DATA_DIR;
	if constexpr (!kXpumDataDir.empty()) {
		fs::path sysCandidate = fs::path(kXpumDataDir) / relativePath;
		if (fs::exists(sysCandidate, ec) && !ec) {
			return sysCandidate.string();
		}
	}

	// Callers should guard against a non-empty string as well as `exists` to handle the case where a file is found but
	// becomes inaccessible (e.g. permissions change, NFS issue, etc.)
	ERR("findResourceFile: resource not found in any allowed location: {}\n", relativePath.c_str());
	return "";
}

/**
 * @brief Get the kernel version string
 *
 * @return std::string Kernel version (e.g., "5.15.0-56-generic") or empty string on failure
 */
std::string getKernelVersion()
{
	struct utsname unameData;
	if (uname(&unameData) == 0) {
		return std::string(unameData.release);
	}
	return "";
}

/**
 * @brief Get the PCI slot label/designation for a device
 *
 * This function attempts to retrieve the physical slot designation for a PCI device
 * using multiple fallback strategies:
 * 1. Read from sysfs /label attribute (fast path, rarely populated)
 * 2. Query SMBIOS/DMI tables via DmiReader (more reliable, works on most systems)
 *
 * @param[in] bdf PCI BDF address string in format "SSSS:BB:DD.F"
 * @return std::string PCI slot designation/label or empty string if not available
 *
 * @note SMBIOS lookup provides slot information on most systems where sysfs label is absent
 * @note DMI reader is instantiated on-demand and may require elevated permissions
 */
std::string getPciSlotLabel(const std::string &bdf)
{
	if (!isValidBdf(bdf)) {
		ERR("Rejecting suspicious BDF address: {}\n", bdf.c_str());
		return "";
	}

	// Try sysfs label first (fast path but rarely populated)
	std::string labelPath = std::format("/sys/bus/pci/devices/{}/label", bdf);
	std::ifstream labelFile(labelPath);

	if (labelFile.is_open()) {
		std::string label;
		std::getline(labelFile, label);
		if (!label.empty()) {
			return label;
		}
	}

	// Fallback to SMBIOS/DMI tables
	try {
		static DmiReader dmiReader; // Only read tables once

		if (dmiReader.isValid()) {
			if (auto slot = dmiReader.findSlotForDevice(bdf)) {
				return slot->designation;
			}
		}
	} catch (const std::exception &e) {
		// DMI reader initialization failed, continue with empty result
		ERR("DMI reader failed for {}: {}\n", bdf.c_str(), e.what());
	}

	return "";
}

/**
 * @brief Derive PCIe generation number from link speed in GT/s
 *
 * Maps the raw transfer rate advertised by sysfs max_link_speed to the
 * corresponding PCIe generation number per the PCIe Base Specification.
 *
 * @param speedGTs Transfer rate in GT/s (e.g. 32.0 for Gen 5)
 * @return int PCIe generation (1–6), or 0 if the speed is unrecognised
 */
static int pcieSpeedToGeneration(double speedGTs)
{
	if (speedGTs >= 64.0)
		return 6;
	if (speedGTs >= 32.0)
		return 5;
	if (speedGTs >= 16.0)
		return 4;
	if (speedGTs >= 8.0)
		return 3;
	if (speedGTs >= 5.0)
		return 2;
	if (speedGTs >= 2.5)
		return 1;
	return 0;
}

/**
 * @brief Calculate PCIe maximum unidirectional bandwidth in GB/s
 *
 * Uses the per-lane transfer rate and the link width to compute the
 * theoretical peak bandwidth, accounting for the encoding overhead:
 *  - PCIe 1.x / 2.x : 8b/10b -> 80 % efficiency
 *  - PCIe 3.x and above : 128b/130b -> ~98.5 % efficiency
 *
 * @param[in] speedGTs Transfer rate per lane in GT/s
 * @param[in] width    Number of lanes (e.g. 16)
 * @return double  Peak bandwidth in GB/s
 */
static double pcieBandwidthGBps(double speedGTs, int width)
{
	if (speedGTs <= 0.0 || width <= 0)
		return 0.0;
	const double encoding = (speedGTs >= 8.0) ? (128.0 / 130.0) : (8.0 / 10.0);
	return speedGTs * static_cast<double>(width) * encoding / 8.0;
}

/**
 * @brief Discover xe-bound PCI devices and collect their PCIe properties
 *
 * Scans /sys/bus/pci/drivers/xe for symlinks representing devices bound to
 * the "xe" kernel driver.  For every valid BDF address found the function
 * reads the following properties directly from sysfs / SMBIOS:
 *
 *  - PCI slot label   : /sys/bus/pci/devices/<bdf>/label  (fallback: SMBIOS)
 *  - PCIe generation  : derived from upstream bridge's max_link_speed in sysfs
 *  - Max link width   : upstream bridge's max_link_width in sysfs
 *  - Max bandwidth    : computed from speed × width × encoding efficiency
 *
 * Example values for a Gen-5 ×16 slot:
 *   pciSlot        = "PCIEX16(G5)"
 *   pcieGeneration = 5
 *   maxLinkWidth   = 16
 *   maxBandwidthGBps = 63.01
 *
 * @param pciPropsList [out] Vector to which one xeDevPciInfo entry per
 *                          discovered device is appended.
 * @return int  0 on success, -1 if /sys/bus/pci/drivers/xe cannot be accessed
 */
int getXeDevPciProps(std::vector<xeDevPciInfo> *pciPropsList)
{
	constexpr std::string_view kXeDriverPath = "/sys/bus/pci/drivers/xe";

	namespace fs = std::filesystem;
	std::error_code ec;

	try {
		for (const auto &entry : fs::directory_iterator(kXeDriverPath, fs::directory_options::skip_permission_denied)) {
			if (!fs::is_symlink(entry.symlink_status(ec)) || ec)
				continue;

			// Consume the symlink to confirm it resolves; ignore errors
			auto targetPath = fs::read_symlink(entry.path(), ec);
			if (ec || targetPath.empty())
				continue;

			const std::string bdf = entry.path().filename().string();
			if (!isValidBdf(bdf))
				continue;

			xeDevPciInfo info;
			info.bdf = bdf;

			// --- PCI slot label ------------------------------------------------
			info.pciSlot = getPciSlotLabel(bdf);

			// --- Resolve root port BDF for accurate PCIe link attributes -----
			// Intel Xe GPU endpoints (and PCIe switch downstream ports) may
			// report incorrect Gen/width in their own LnkCap registers.  The
			// root port — first BDF in the symlink path — is always configured
			// correctly by BIOS for the physical slot.  Walk the raw symlink
			// target components to find it:
			//   ../../../../devices/pci0000:00/0000:00:06.0/0000:02:00.0/0000:03:01.0/0000:04:00.0
			//                                               ^^^^^^^^^^^^  ← root port (first valid BDF)
			std::string linkBdf = bdf;
			for (const auto &component : targetPath) {
				const std::string name = component.string();
				if (isValidBdf(name) && name != bdf) {
					linkBdf = name;
					DBG("Using root port BDF %s for link attributes of %s\n", linkBdf.c_str(), bdf.c_str());
					break;
				}
			}

			// --- PCIe max link speed -------------------------------------------
			// sysfs format: "32.0 GT/s PCIe"  →  stod stops at the first
			// non-numeric character, giving us the GT/s value directly.
			const std::string speedStr = getSysfsLine(linkBdf, "/max_link_speed");
			double speedGTs = 0.0;
			if (!speedStr.empty()) {
				try {
					speedGTs = std::stod(speedStr);
				} catch (const std::exception &) {
					ERR("Failed to parse max_link_speed for %s: \"%s\"\n", linkBdf.c_str(), speedStr.c_str());
				}
			}
			info.pcieGeneration = pcieSpeedToGeneration(speedGTs);

			// --- PCIe max link width -------------------------------------------
			// sysfs format: "16"
			const std::string widthStr = getSysfsLine(linkBdf, "/max_link_width");
			info.maxLinkWidth = 0;
			if (!widthStr.empty()) {
				try {
					info.maxLinkWidth = std::stoi(widthStr);
				} catch (const std::exception &) {
					ERR("Failed to parse max_link_width for %s: \"%s\"\n", linkBdf.c_str(), widthStr.c_str());
				}
			}

			// --- Max bandwidth (derived) ---------------------------------------
			info.maxBandwidthGBps = pcieBandwidthGBps(speedGTs, info.maxLinkWidth);

			pciPropsList->push_back(std::move(info));
		}
	} catch (const fs::filesystem_error &e) {
		ERR("Error accessing %s: %s\n", kXeDriverPath.data(), e.what());
		return -1;
	}

	return 0;
}

// PCI Express Capability ID
static constexpr uint8_t PCI_CAP_ID_EXP = 0x10;

// Offset of Slot Capabilities register within the PCIe Capability structure
static constexpr uint8_t PCI_EXP_SLTCAP_OFFSET = 0x14;

// Slot Capabilities register bits
static constexpr uint32_t PCI_EXP_SLTCAP_PCP = 0x0002;	   // Power Controller Present (bit 1)
static constexpr uint32_t PCI_EXP_SLTCAP_HPC = 0x0040;	   // Hot-Plug Capable (bit 6)
static constexpr uint32_t PCI_EXP_SLTCAP_PSN = 0xFFF80000; // Physical Slot Number (bits 31:19)

/**
 * @brief Finds the root port BDF for a given GPU PCI device by walking the sysfs topology
 *
 * Traverses parent directories in /sys/bus/pci/devices/<gpuBdf>/ upward until
 * reaching the root port (the bridge whose parent is a PCI domain root).
 *
 * @param gpuBdf BDF string of the GPU device (e.g., "0000:4d:00.0")
 * @param rootPortBdf Output string to store the root port BDF
 * @return true if the root port was found, false otherwise
 */
static bool findRootPortBdf(const std::string &gpuBdf, std::string &rootPortBdf)
{
	if (!isValidBdf(gpuBdf)) {
		ERR("Rejecting invalid BDF address: {}\n", gpuBdf);
		return false;
	}

	std::error_code ec;
	std::filesystem::path devPath = "/sys/bus/pci/devices/" + gpuBdf;

	if (!std::filesystem::exists(devPath, ec)) {
		ERR("GPU sysfs path not found: {}\n", devPath.string());
		return false;
	}

	// Resolve symlink to physical path, e.g. /sys/devices/pci0000:00/0000:00:01.0/0000:4d:00.0
	std::filesystem::path realPath = std::filesystem::canonical(devPath, ec);
	if (ec) {
		ERR("Failed to resolve sysfs path for {}: {}\n", gpuBdf, ec.message());
		return false;
	}

	// Walk up from the GPU device through parent bridges.
	// The root port is the first bridge whose parent directory name starts with "pci"
	std::filesystem::path current = realPath;
	std::filesystem::path candidate;

	while (current.has_parent_path() && current != current.parent_path()) {
		std::filesystem::path parent = current.parent_path();
		std::string parentName = parent.filename().string();

		if (parentName.starts_with("pci")) {
			candidate = current;
			break;
		}
		std::string currentName = current.filename().string();
		if (isValidBdf(currentName)) {
			candidate = current;
		}
		current = parent;
	}

	if (candidate.empty()) {
		ERR("Could not find root port for {}\n", gpuBdf);
		return false;
	}

	rootPortBdf = candidate.filename().string();
	DBG("Root port for {} is {}\n", gpuBdf, rootPortBdf);
	return true;
}

/**
 * @brief Reads the PCIe Slot Capabilities register from a root port's config space
 *
 * Walks the PCI capability list to find the PCIe Capability (ID 0x10),
 * then reads the Slot Capabilities register at offset (+0x14) within that capability.
 *
 * @param rootPortBdf BDF string of the root port
 * @param slotCap Output for the 32-bit Slot Capabilities register value
 * @return true if the register was read successfully, false otherwise
 */
static bool readSlotCapabilities(const std::string &rootPortBdf, uint32_t &slotCap)
{
	std::string configPath = "/sys/bus/pci/devices/" + rootPortBdf + "/config";

	std::ifstream config(configPath, std::ios::binary);
	if (!config) {
		ERR("Cannot open PCI config space: {}\n", configPath);
		return false;
	}

	uint8_t capPtr = 0;
	config.seekg(PCI_CAPABILITY_LIST);
	config.read(reinterpret_cast<char *>(&capPtr), sizeof(capPtr));
	if (!config) {
		ERR("Failed to read capability pointer from {}\n", configPath);
		return false;
	}

	// PCI spec requires capability pointers to be DWORD-aligned (bits 1:0 = 0)
	capPtr &= 0xFC;

	// Standard PCI config space is 256 bytes; cap list cannot have more than ~48 entries.
	// Bound iterations to detect corrupted/looping capability lists.
	constexpr int maxCapEntries = 48;
	int iterations = 0;

	while (capPtr != 0 && iterations++ < maxCapEntries) {
		uint8_t capId = 0;
		config.seekg(capPtr);
		config.read(reinterpret_cast<char *>(&capId), sizeof(capId));
		if (!config) {
			ERR("Failed to read capability ID at offset {:#04x}\n", capPtr);
			return false;
		}

		if (capId == PCI_CAP_ID_EXP) {
			// Found PCIe Capability - read Slot Capabilities register
			config.seekg(capPtr + PCI_EXP_SLTCAP_OFFSET);
			config.read(reinterpret_cast<char *>(&slotCap), sizeof(slotCap));
			if (!config) {
				ERR("Failed to read Slot Capabilities register\n");
				return false;
			}
			return true;
		}

		uint8_t nextPtr = 0;
		config.seekg(capPtr + 1);
		config.read(reinterpret_cast<char *>(&nextPtr), sizeof(nextPtr));
		if (!config) {
			return false;
		}
		capPtr = nextPtr & 0xFC;
	}

	if (iterations >= maxCapEntries) {
		ERR("Capability list walk exceeded {} entries for {} (corrupted?)\n", maxCapEntries, rootPortBdf);
	} else {
		ERR("PCIe Capability not found for {}\n", rootPortBdf);
	}
	return false;
}

// Cold reset timing constants
static constexpr int POWER_CYCLE_DELAY_US = 1000000; // 1s for power rail discharge
static constexpr int POLL_INTERVAL_MS = 200;		 // Re-enumeration poll interval
static constexpr int REENUM_TIMEOUT_MS = 10000;		 // 10s max wait for re-enumeration
static constexpr int BIND_TIMEOUT_MS = 5000;		 // 5s max wait for driver auto-bind

/**
 * @brief Resolves the kernel driver currently bound to a PCI device
 *
 * Reads the symlink at /sys/bus/pci/devices/<bdf>/driver and returns its
 * basename (e.g. "xe", "i915").
 *
 * @param bdf PCI BDF string
 * @return Driver name on success, empty string if no driver is bound
 */
static std::string getPciDriverName(const std::string &bdf)
{
	std::error_code ec;
	std::filesystem::path link = "/sys/bus/pci/devices/" + bdf + "/driver";
	std::filesystem::path target = std::filesystem::read_symlink(link, ec);
	if (ec) {
		return "";
	}
	return target.filename().string();
}

/**
 * @brief Writes a PCI BDF to a driver sysfs control file (bind or unbind)
 *
 * @param driverName Kernel driver name (e.g. "xe")
 * @param action     "bind" or "unbind"
 * @param bdf        PCI BDF to write
 * @return true on success
 */
static bool writeToPciDriverFile(const std::string &driverName, const std::string &action, const std::string &bdf)
{
	std::string path = "/sys/bus/pci/drivers/" + driverName + "/" + action;
	std::ofstream f(path, std::ios::trunc);
	if (!f) {
		ERR("Failed to open {} for writing\n", path);
		return false;
	}
	f << bdf;
	if (!f) {
		ERR("Failed to write '{}' to {}\n", bdf, path);
		return false;
	}
	return true;
}

/**
 * @brief Performs a cold reset on a GPU via the sysfs slot power interface
 *
 * This function:
 * 1. Finds the root port for the GPU
 * 2. Reads the PCIe Slot Capabilities register to check for hot-plug and power control
 * 3. Derives the physical slot number from bits 31:19
 * 4. Power-cycles the slot via /sys/bus/pci/slots/<slot>/power (echo 0, then echo 1)
 *
 * @param gpuBdf BDF string of the GPU device (e.g., "0000:4d:00.0")
 * @return 0 on success, -1 if sysfs cold reset is not supported (caller may
 *         fall back to AMC), 1 if supported but the operation failed
 */
int coldResetViaSysfs(const std::string &gpuBdf)
{
	std::string rootPortBdf;
	if (!findRootPortBdf(gpuBdf, rootPortBdf)) {
		return 1;
	}

	uint32_t slotCap = 0;
	if (!readSlotCapabilities(rootPortBdf, slotCap)) {
		return 1;
	}

	bool pwrCtrl = (slotCap & PCI_EXP_SLTCAP_PCP) != 0;
	bool hotPlug = (slotCap & PCI_EXP_SLTCAP_HPC) != 0;
	uint32_t slotNum = (slotCap & PCI_EXP_SLTCAP_PSN) >> 19;

	DBG("Root port {}: PwrCtrl={}, HotPlug={}, SlotNum={}\n", rootPortBdf, pwrCtrl, hotPlug, slotNum);

	if (!pwrCtrl || !hotPlug) {
		INFO("Cold reset via sysfs not supported for {} (PwrCtrl={}, HotPlug={})\n", gpuBdf, pwrCtrl, hotPlug);
		return -1;
	}

	std::string slotDir = "/sys/bus/pci/slots/" + std::to_string(slotNum);
	std::string slotPowerPath = slotDir + "/power";

	std::error_code ec;
	if (!std::filesystem::exists(slotPowerPath, ec)) {
		ERR("Slot power sysfs path not found: {}\n", slotPowerPath);
		return -1;
	}

	// Verify the sysfs slot belongs to our root port by checking the address file.
	// This prevents rare case of power-cycling the wrong slot when Physical Slot
	// Number defaults to 0 or collides with another slot.
	{
		std::string slotAddrPath = slotDir + "/address";
		std::ifstream addrFile(slotAddrPath);
		std::string slotAddr;
		if (!addrFile || !std::getline(addrFile, slotAddr)) {
			ERR("Cannot read slot address from {}\n", slotAddrPath);
			return 1;
		}
		// rootPortBdf is "DDDD:BB:DD.F", slot address is "DDDD:BB:DD" (no function)
		std::string rootPortPrefix = rootPortBdf.substr(0, rootPortBdf.rfind('.'));
		if (slotAddr != rootPortPrefix) {
			ERR("Slot {} address mismatch: expected {}, got {}\n", slotNum, rootPortPrefix, slotAddr);
			return 1;
		}
	}

	INFO("Performing cold reset on {} via slot {} power cycle\n", gpuBdf, slotNum);

	// Unbind the kernel driver before power-off
	std::string driverName = getPciDriverName(gpuBdf);
	if (!driverName.empty()) {
		INFO("Unbinding driver '{}' from {} before reset\n", driverName, gpuBdf);
		if (!writeToPciDriverFile(driverName, "unbind", gpuBdf)) {
			ERR("Driver unbind failed for {}; aborting cold reset\n", gpuBdf);
			return 1;
		}
	} else {
		DBG("No driver bound to {}; skipping unbind\n", gpuBdf);
	}

	// Power off
	{
		std::ofstream powerFile(slotPowerPath, std::ios::trunc);
		if (!powerFile) {
			ERR("Failed to open {} for writing\n", slotPowerPath);
			return 1;
		}
		powerFile << "0";
		if (!powerFile) {
			ERR("Failed to write '0' to {}\n", slotPowerPath);
			return 1;
		}
	}

	// Wait for power rails to fully discharge.
	// PCIe spec requires minimum 100ms for PCIe Reset, but 1s is recommended
	// by platform vendors for full power rail discharge.
	usleep(POWER_CYCLE_DELAY_US);

	// Confirm power-off by reading back the slot power state
	{
		std::ifstream powerRead(slotPowerPath);
		std::string state;
		if (powerRead && std::getline(powerRead, state) && state == "0") {
			DBG("Slot {} power confirmed off\n", slotNum);
		} else {
			ERR("Slot {} power state not confirmed off (read: '{}')\n", slotNum, state);
			return 1;
		}
	}

	// Power on
	{
		std::ofstream powerFile(slotPowerPath, std::ios::trunc);
		if (!powerFile) {
			ERR("Failed to open {} for writing\n", slotPowerPath);
			return 1;
		}
		powerFile << "1";
		if (!powerFile) {
			ERR("Failed to write '1' to {}\n", slotPowerPath);
			return 1;
		}
	}

	// Wait for device to re-enumerate after power-on.
	// The device needs time for link training and PCI enumeration.
	std::string gpuSysfsPath = "/sys/bus/pci/devices/" + gpuBdf;
	int elapsed = 0;

	while (elapsed < REENUM_TIMEOUT_MS) {
		usleep(POLL_INTERVAL_MS * 1000);
		elapsed += POLL_INTERVAL_MS;

		std::error_code devEc;
		if (std::filesystem::exists(gpuSysfsPath, devEc)) {
			INFO("Device {} re-enumerated after {} ms\n", gpuBdf, elapsed);
			break;
		}
	}

	if (elapsed >= REENUM_TIMEOUT_MS) {
		ERR("Device {} did not re-enumerate within {} ms after power-on\n", gpuBdf, REENUM_TIMEOUT_MS);
		return 1;
	}

	// After re-enumeration, the PCI subsystem normally auto-binds the matching
	// driver. If it does not happen within the timeout, fall back
	// to an explicit bind. Skipped if no driver was bound originally.
	if (!driverName.empty()) {
		int bindElapsed = 0;
		std::string boundDriver;
		while (bindElapsed < BIND_TIMEOUT_MS) {
			boundDriver = getPciDriverName(gpuBdf);
			if (!boundDriver.empty()) {
				break;
			}
			usleep(POLL_INTERVAL_MS * 1000);
			bindElapsed += POLL_INTERVAL_MS;
		}

		if (boundDriver.empty()) {
			INFO("Driver '{}' did not auto-bind to {}; performing explicit bind\n", driverName, gpuBdf);
			if (!writeToPciDriverFile(driverName, "bind", gpuBdf)) {
				ERR("Explicit driver bind failed for {}\n", gpuBdf);
				return 1;
			}
		} else {
			DBG("Driver '{}' bound to {} after {} ms\n", boundDriver, gpuBdf, bindElapsed);
		}
	}

	INFO("Cold reset completed for {} via slot {}\n", gpuBdf, slotNum);
	return 0;
}

/**
 * @brief Enumerates processes using a GPU device by scanning /proc for open DRM FDs
 *
 * Maps the GPU's PCI BDF to its /dev/dri/ device nodes (card<N>, renderD<N>),
 * then scans every process in /proc/ checking /proc/<pid>/fd/ symlinks for
 * references to those device nodes. This approach works without depending on
 * Level Zero (which may fail on a wedged GPU).
 *
 * The calling process is excluded from the result, since xpu-smi itself
 * holds Level Zero handles on the GPU.
 *
 * @param gpuBdf BDF string of the GPU device (e.g., "0000:4d:00.0")
 * @return Vector of PIDs (other than the caller) using the device (empty on error)
 */
std::vector<uint32_t> getGpuProcessesByBdf(const std::string &gpuBdf)
{
	std::vector<uint32_t> pids;
	namespace fs = std::filesystem;

	if (!isValidBdf(gpuBdf)) {
		ERR("Rejecting invalid BDF: {}\n", gpuBdf);
		return pids;
	}

	// Find all /dev/dri/ device node names belonging to this BDF.
	// /sys/class/drm/card<N> is a symlink whose resolved path contains the BDF.
	std::vector<std::string> deviceNodes; // e.g. {"/dev/dri/card0", "/dev/dri/renderD128"}
	std::error_code ec;

	for (const auto &entry : fs::directory_iterator("/sys/class/drm", ec)) {
		std::string name = entry.path().filename().string();
		if (name.rfind("card", 0) != 0 || name.find('-') != std::string::npos)
			continue;

		auto resolved = fs::canonical(entry.path(), ec);
		if (ec || resolved.string().find(gpuBdf) == std::string::npos)
			continue;

		// Found the card - enumerate device/drm/ for all DRM node names
		fs::path devDrmDir = entry.path() / "device" / "drm";
		if (fs::is_directory(devDrmDir, ec)) {
			for (const auto &node : fs::directory_iterator(devDrmDir, ec)) {
				std::string nodeName = node.path().filename().string();
				if (nodeName.rfind("card", 0) == 0 || nodeName.rfind("renderD", 0) == 0) {
					deviceNodes.push_back("/dev/dri/" + nodeName);
				}
			}
		}
		break;
	}

	if (deviceNodes.empty()) {
		ERR("No DRM device nodes found for BDF {}\n", gpuBdf);
		return pids;
	}

	DBG("Device nodes for {}:", gpuBdf);
	for (const auto &dn : deviceNodes) {
		DBG("  {}", dn);
	}

	// Scan /proc/ for all PIDs, then check their open FDs.
	const pid_t selfPid = getpid();
	for (const auto &procEntry : fs::directory_iterator("/proc", ec)) {
		std::string pidName = procEntry.path().filename().string();

		// Only numeric directories are PIDs
		if (pidName.empty() || !std::isdigit(static_cast<unsigned char>(pidName[0])))
			continue;

		uint32_t pid = 0;
		try {
			unsigned long parsed = std::stoul(pidName);
			if (parsed > UINT32_MAX)
				continue;
			pid = static_cast<uint32_t>(parsed);
		} catch (...) {
			continue;
		}

		if (static_cast<pid_t>(pid) == selfPid)
			continue;

		// Scan /proc/<pid>/fd/ for symlinks pointing to our device nodes
		fs::path fdDir = procEntry.path() / "fd";
		std::error_code fdEc;
		bool found = false;

		for (const auto &fdEntry : fs::directory_iterator(fdDir, fs::directory_options::skip_permission_denied, fdEc)) {
			fs::path target = fs::read_symlink(fdEntry.path(), fdEc);
			if (fdEc)
				continue;

			std::string targetStr = target.string();
			for (const auto &devNode : deviceNodes) {
				if (targetStr == devNode) {
					pids.push_back(pid);
					found = true;
					break;
				}
			}
			if (found)
				break;
		}
	}

	DBG("Found {} processes using GPU {}\n", pids.size(), gpuBdf);
	return pids;
}

/**
 * @brief Checks whether a PCI device is a PCI/PCIe bridge
 *
 * Reads the class code from sysfs. PCI base class 0x06 covers all bridge
 * types.
 *
 * @param bdf BDF string of the PCI device to check
 * @return true if the device is a bridge, false otherwise (or on read error)
 */
static bool isPciBridge(const std::string &bdf)
{
	std::ifstream classFile("/sys/bus/pci/devices/" + bdf + "/class");
	std::string classStr;
	if (!classFile || !std::getline(classFile, classStr)) {
		return false;
	}

	return classStr.length() >= 4 && classStr.substr(0, 4) == "0x06";
}

/**
 * @brief Enumerates other PCI endpoints that share a PCIe slot with the given GPU
 *
 * A PCIe slot is rooted at a single root port. Any endpoint reachable below
 * that root port tied to the same slot power rail and would be reset by a slot
 * power cycle.
 *
 * This function walks the sysfs subtree under the GPU's root port, returning
 * BDFs of every non-bridge device other than the GPU itself.
 *
 * @param gpuBdf BDF string of the GPU device (e.g., "0000:4d:00.0")
 * @return Vector of sibling endpoint BDFs sharing the slot (empty if none or on error)
 */
std::vector<std::string> getDevicesSharingSlotWith(const std::string &gpuBdf)
{
	std::vector<std::string> siblings;
	namespace fs = std::filesystem;

	std::string rootPortBdf;
	if (!findRootPortBdf(gpuBdf, rootPortBdf)) {
		return siblings;
	}

	std::error_code ec;
	fs::path rootPortLink = "/sys/bus/pci/devices/" + rootPortBdf;
	fs::path rootPortPath = fs::canonical(rootPortLink, ec);
	if (ec) {
		ERR("Failed to resolve root port path {}: {}\n", rootPortLink.string(), ec.message());
		return siblings;
	}

	// Recursively collect all non-bridge BDFs under the root port
	std::function<void(const fs::path &)> collectEndpointsBelow = [&](const fs::path &dir) {
		std::error_code wec;
		for (const auto &entry : fs::directory_iterator(dir, wec)) {
			std::string name = entry.path().filename().string();
			if (!isValidBdf(name)) {
				continue;
			}
			if (!fs::is_directory(entry.symlink_status(wec))) {
				continue;
			}

			if (isPciBridge(name)) {
				collectEndpointsBelow(entry.path());
			} else if (name != gpuBdf) {
				siblings.push_back(name);
			}
		}
	};
	collectEndpointsBelow(rootPortPath);

	DBG("Found {} sibling endpoints sharing slot with {}\n", siblings.size(), gpuBdf);
	return siblings;
}
