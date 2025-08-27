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

#include "os.h"
#include <algorithm>
#include <debug.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <sys/mman.h>
#include <vector>
#include <termios.h>
#include <unistd.h>
#include <format>
#include <filesystem>
#include "pci_database.h"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include <hwloc.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

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
 * @brief Creates a new thread.
 *
 * @param thread Function pointer to the thread function.
 * @param args Arguments to pass to the thread function.
 * @return A pointer to the thread_id object representing the created thread.
 */
thread_id *create_thread(funcptr thread, void *args)
{
	pthread_t thread_hdl;
	pthread_create(&thread_hdl, NULL, thread, args);

	DBG("%s: thread handle is %ld\n", __func__, thread_hdl);
	thread_id *new_thread_id = new thread_id(thread_hdl);
	return new_thread_id;
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
	std::string xpum_grp("xpum");
	bool has_privilege = false;
	for (int i = 0; i < ngroups; i++) {
		struct group *gr = getgrgid(groups[i]);
		if (gr == NULL) {
			ERR("getgrgid error\n");
			return false;
		}
		std::string grp_name(gr->gr_name);
		if (grp_name == xpum_grp) {
			has_privilege = true;
		}
	}

	return has_privilege;
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
	char path[255];
	sprintf(path, "/proc/%d/cmdline", processId);
	pinfo.open(path);
	if (pinfo.is_open()) {
		std::getline(pinfo, processName);
		pinfo.close();
	}
	return processName;
}

/**
 * @brief Discovers and enumerates available I2C devices on the system
 *
 * This function scans the Linux system for available I2C (Inter-Integrated Circuit)
 * devices by examining the /dev directory. It provides device discovery capabilities
 * for hardware monitoring and sensor access operations on Intel graphics devices.
 *
 * @return std::vector<std::string> Vector containing paths to available I2C devices
 */
std::vector<std::string> findI2CDevices()
{
	std::vector<std::string> devices;
	std::string devicesPath = "/sys/bus/i2c/devices";

	// Check if the directory exists
	if (!std::filesystem::exists(devicesPath)) {
		ERR("I2C devices directory does not exist: %s\n", devicesPath.c_str());
		return devices;
	}

	// Iterate through all entries in the directory
	for (const auto &entry : std::filesystem::directory_iterator(devicesPath)) {
		if (entry.is_directory()) {
			std::string dirname = entry.path().filename().string();
			devices.push_back(entry.path().string());
		}
	}

	return devices;
}

/**
 * @brief Retrieves the name of an I2C device from its device path
 *
 * This function reads the name attribute of an I2C device by accessing the
 * 'name' file in the device's sysfs directory. It provides device identification
 * capabilities for I2C hardware discovery and sensor management operations.
 *
 * @param devicePath String reference containing the full path to the I2C device directory in sysfs
 * @return std::string Device name if found, empty string if name file cannot be read
 */
std::string getI2CDeviceName(const std::string &devicePath)
{
	std::string name;
	std::ifstream nameFile(devicePath + "/name");
	if (nameFile.is_open()) {
		std::getline(nameFile, name);
		nameFile.close();
	}
	return name;
}

/**
 * @brief Opens an I2C device for communication and returns file descriptor
 *
 * This function establishes communication with the specified I2C device by
 * opening the device file and returning a file descriptor. It provides low-level
 * I2C access capabilities for direct hardware communication and sensor operations.
 *
 * @param deviceName String reference containing the name of the I2C device to open
 * @return long long File descriptor if successful, negative value on error
 */
long long openI2C(const std::string &deviceName)
{
	std::vector<std::string> devices = findI2CDevices();
	for (const auto &device : devices) {
		std::string name = getI2CDeviceName(device);
		if (name == deviceName) {
			// Found the device with the specified name, the first character of the path
			// Example: "/sys/bus/i2c/devices/7-0040" -> "7"
			// This needs to be added to i2c- so that we can open the device
			// Example: "/dev/i2c-7"
			std::string i2cDevice = "/dev/i2c-" + device.substr(device.find_last_of('/') + 1, 1);
			// Open the I2C device
			return (long long)open(i2cDevice.c_str(), O_RDWR | O_NONBLOCK);
		}
	}
	return -1;
}

/**
 * @brief Closes an I2C device file descriptor
 *
 * This function safely closes the specified I2C device file descriptor,
 * releasing the system resources and terminating the communication channel
 * with the I2C device. It provides proper cleanup for I2C operations.
 *
 * @param fd File descriptor of the I2C device to close
 * @return int 0 on success, -1 on error (per close() system call)
 */
int closeI2C(long long fd) { return close((int)fd); }

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
	std::string timestamp_str = "";
	time_t now;
	struct tm *time_info;

	// Get current time in seconds since the epoch
	now = time(NULL);

	time_info = localtime(&now);
	if (time_info == NULL) {
		ERR("localtime failed\n");
		return timestamp_str;
	}

	struct timeval tv;
	if (gettimeofday(&tv, NULL) == -1) {
		ERR("gettimeofday failed\n");
		return timestamp_str;
	}
	timestamp_str = std::format("{:02d}:{:02d}:{:02d}.{:03d}", time_info->tm_hour, time_info->tm_min, time_info->tm_sec,
								(int)tv.tv_usec / 1000);
	return timestamp_str;
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
std::string getSysfsLine(std::string bdf, std::string suffix)
{
	TRACING();
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
std::string getLocalCpus(std::string bdf)
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
std::string getCpuList(std::string bdf)
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
	int device_id = obj->attr->pcidev.device_id;
	const PcieDevice *pDevice = PciDatabase::instance().getDevice(vendorId, device_id);
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
std::string getDevicePath(const std::string &bdf_address)
{
	std::string devicePath = "/sys/devices";
	std::string result;
	namespace stdfs = std::filesystem;
	const stdfs::recursive_directory_iterator end{};

	for (stdfs::recursive_directory_iterator iter{devicePath}; iter != end; ++iter) {
		if (iter->path().string().find(bdf_address, 0) == std::string::npos) {
			continue;
		}
		if (stdfs::is_directory(*iter)) {
			result = iter->path().string();
			break;
		}
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
void getSwitchDevicePath(hwloc_obj_t par_obj, std::string *switchDevicePath)
{
	hwloc_obj_t obj = par_obj->parent;
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
					std::string address = pci2RegxString(obj);
					if (address.length() > 0) {
						std::string path = getDevicePath(address);
						if (path.length() > 0) {
							*switchDevicePath = path;
						}
						count++;
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
