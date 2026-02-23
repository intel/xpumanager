/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <os.h>
#include <algorithm>
#include <debug.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <vector>
#include <termios.h>
#include <unistd.h>
#include <format>
#include <filesystem>
#include <iostream>
#include <syncstream>

#include "lin.h"
#include "pci_database.h"
#include "dmi_reader.h"

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
 * This function executes a shell command using popen and captures both
 * the output and exit status. It provides a safer alternative to system()
 * by capturing output and providing proper error handling with RAII.
 *
 * @param command The shell command to execute
 * @return SystemCommandResult containing the command output and exit status
 *
 * @note Uses RAII with custom deleter for automatic pipe cleanup
 * @note Provides enhanced error handling for I/O operations
 * @note Uses std::fread for efficient buffer reading
 */
SystemCommandResult execCommand(const std::string &command)
{
	std::vector<char> buffer(65536); // 64 KB heap buffer; avoids large stack allocation
	std::string result;
	// Custom deleter for pipe(), ensures it is always closed automatically on error
	struct PipeDeleter
	{
		int *pExitCode;
		explicit PipeDeleter(int *ec) : pExitCode(ec) {}
		void operator()(FILE *f) const noexcept
		{
			if (f && pExitCode) {
				*pExitCode = pclose(f);
			}
		}
	};
	using safePipe = std::unique_ptr<FILE, PipeDeleter>;
	int exitcode = -1;
	safePipe pipe(popen(command.c_str(), "r"), PipeDeleter(&exitcode));
	if (pipe != nullptr) {
		try {
			std::size_t bytesRead;
			while ((bytesRead = std::fread(buffer.data(), sizeof(char), buffer.size(), pipe.get())) != 0) {
				result += std::string(buffer.data(), bytesRead);
			}
			// std::fread returns 0 both when EOF is reached and when some I/O error occured. Check for completeness
			if (std::ferror(pipe.get())) {
				return SystemCommandResult{"Command output read I/O error", -1}; // Not sure;
			} else if (!std::feof(pipe.get())) {
				return SystemCommandResult("Command output read terminated unexpectedly", -1); // Not sure
			}
		} catch (...) {
			return SystemCommandResult("Exception occurred while reading command output", -1);
		}
	}
	// RAII/custom deleter will close the pipe and reset the exitcode
	pipe.reset();
	return SystemCommandResult(result, WEXITSTATUS(exitcode));
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

	DBG("%s: thread handle is %ld\n", __func__, threadHdl);
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
		DBG("%s: thread handle is %ld\n", __func__, tid->ret_thread_uid());
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
 * @brief Validates that a BDF address string is well-formed
 *
 * BDF addresses must match the canonical format DDDD:BB:DD.F using only
 * hexadecimal digits and the expected separators. This check prevents
 * path-traversal attacks when the BDF is embedded in sysfs paths.
 *
 * @param bdf The BDF string to validate
 * @return bool True if the string is a valid BDF address, false otherwise
 */
static bool isValidBdf(const std::string &bdf)
{
	// BDF format: DDDD:BB:DD.F — exactly 12 characters
	if (bdf.length() != 12) {
		return false;
	}
	const auto isHex = [](unsigned char c) { return std::isxdigit(c) != 0; };
	return isHex(bdf[0]) && isHex(bdf[1]) && isHex(bdf[2]) && isHex(bdf[3]) && bdf[4] == ':' && isHex(bdf[5]) &&
		   isHex(bdf[6]) && bdf[7] == ':' && isHex(bdf[8]) && isHex(bdf[9]) && bdf[10] == '.' && isHex(bdf[11]);
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
		ERR("Rejecting suspicious BDF address: %s\n", bdf.c_str());
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
					DBG("Found Switch count %d.\n", count);
				}
			}
		} else {
			DBG("Unknown hwloc-obj type  %d.\n", obj->type);
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
		ERR("Error searching /sys/devices for %s: %s\n", bdfAddress.c_str(), e.what());
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
			ERR("Unknown hwloc-obj type  %d.\n", obj->type);
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
		ERR("Error accessing /sys/class/drm: %s\n", e.what());
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
			ERR("findResourceFile: exe-relative path escapes exe directory: %s\n", canonicalCandidate.c_str());
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
	ERR("findResourceFile: resource not found in any allowed location: %s\n", relativePath.c_str());
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
		ERR("Rejecting suspicious BDF address: %s\n", bdf.c_str());
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
		ERR("DMI reader failed for %s: %s\n", bdf.c_str(), e.what());
	}

	return "";
}
