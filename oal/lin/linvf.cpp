/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <os.h>
#include <osvf.h>
#include <debug.h>
#include <fstream>
#include "lin.h"
#include <sstream>
#include <pciaccess.h>
#include <dirent.h>
#include <file_io.h>

#define OFFSET_TOTAL_VF 0x0E
#define OFFSET_INITIAL_VF 0x0C
#define OFFSET_NUM_VF 0x10
#define OFFSET_VF_STRIDE 0x14
#define OFFSET_VF_DEVICE_ID 0x1A
#define OFFSET_FIRST_VF_OFFSET 0x18
#define OFFSET_VF_BAR 0x24
#define PCI_EXT_CAP_START 0x100
#define PCI_EXT_CAP_ID_SRIOV 0x0010

/**
 * @brief Initialize PCI access system
 * @return true if successful, false otherwise
 */
static bool initializePciSystem()
{
	static bool pciInit = false;

	if (!pciInit) {
		if (pci_system_init() != 0) {
			ERR("Failed to initialize PCI system\n");
			return false;
		}
		pciInit = true;
	}
	return true;
}

/**
 * @brief Read SRIOV capability from PCI config space
 * @param[in] dev PCI device
 * @param[out] info PCI device info to populate
 */
static void readSriovCapability(struct pci_device *dev, PciDeviceInfo &info)
{
	uint16_t capPtr = PCI_EXT_CAP_START;
	uint16_t capReg = 0;
	uint32_t capHeader = 0;
	int maxExtCapWalk = 256;

	// Walk PCIe extended capability list (starting from 0x100) looking for SR-IOV capability.
	while (capPtr != 0 && capPtr < 0x1000 && maxExtCapWalk-- > 0) {
		if (pci_device_cfg_read_u32(dev, &capHeader, capPtr) != 0) {
			break;
		}

		if (capHeader == 0 || capHeader == 0xFFFFFFFF) {
			break;
		}

		uint16_t capId = static_cast<uint16_t>(capHeader & 0xFFFF);
		uint16_t nextCapPtr = static_cast<uint16_t>((capHeader >> 20) & 0x0FFF);

		if (capId == PCI_EXT_CAP_ID_SRIOV) {
			info.isSriovCapable = true;

			// Read SRIOV capabilities
			// Total VFs (offset 0x0E from capability base)
			if (pci_device_cfg_read_u16(dev, &info.totalVFs, capPtr + OFFSET_TOTAL_VF) != 0) {
				info.totalVFs = 0;
			}

			// Initial VFs (offset 0x0C from capability base)
			if (pci_device_cfg_read_u16(dev, &info.initialVFs, capPtr + OFFSET_INITIAL_VF) != 0) {
				info.initialVFs = 0;
			}

			// Current NumVFs (offset 0x10 from capability base)
			if (pci_device_cfg_read_u16(dev, &info.numVFs, capPtr + OFFSET_NUM_VF) != 0) {
				info.numVFs = 0;
			}

			// VF Stride (offset 0x14 from capability base)
			if (pci_device_cfg_read_u16(dev, &capReg, capPtr + OFFSET_VF_STRIDE) != 0) {
				info.vfStride = 0;
			} else {
				info.vfStride = capReg;
			}

			// VF Device ID (offset 0x1A from capability base)
			if (pci_device_cfg_read_u16(dev, &info.vfDeviceId, capPtr + OFFSET_VF_DEVICE_ID) != 0) {
				info.vfDeviceId = 0;
			}

			// First VF Offset (offset 0x18 from capability base)
			if (pci_device_cfg_read_u16(dev, &capReg, capPtr + OFFSET_FIRST_VF_OFFSET) != 0) {
				info.firstVFOffset = 0;
			} else {
				info.firstVFOffset = capReg;
			}

			// VF BARs (offsets 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38)
			for (int i = 0; i < 6; i++) {
				if (pci_device_cfg_read_u32(dev, &info.vfBarSize[i], capPtr + OFFSET_VF_BAR + (i * 4)) != 0) {
					info.vfBarSize[i] = 0;
				}
			}

			break;
		}

		if (nextCapPtr == capPtr || nextCapPtr == 0 || nextCapPtr < PCI_EXT_CAP_START) {
			break;
		}

		capPtr = nextCapPtr;
	}
}

/**
 * @brief Clean up PCI system resources
 *
 * Call this when you're done using PCI functions to clean up resources.
 * Note: This will affect the entire process, so only call if you're sure
 * no other code is using libpciaccess.
 */
static void cleanupPciSystem()
{
	TRACING();
	pci_system_cleanup();
}

/**
 * @brief Query PCI device information using libpciaccess with SRIOV support
 *
 * @param[in] domain PCI domain (usually 0)
 * @param[in] bus PCI bus number
 * @param[in] device PCI device number
 * @param[in] function PCI function number
 * @return PciDeviceInfo structure with device information
 */
static PciDeviceInfo queryPciDevice(uint16_t domain, uint8_t bus, uint8_t device, uint8_t function)
{
	PciDeviceInfo info = {};
	info.valid = false;

	if (!initializePciSystem()) {
		return info;
	}

	// Find the device
	struct pci_device *dev = pci_device_find_by_slot(domain, bus, device, function);
	if (!dev) {
		ERR("PCI device {:04x}:{:02x}:{:02x}.{} not found\n", domain, bus, device, function);
		cleanupPciSystem();
		return info;
	}

	// Probe the device to get detailed information
	if (pci_device_probe(dev) != 0) {
		ERR("Failed to probe PCI device {:04x}:{:02x}:{:02x}.{}\n", domain, bus, device, function);
		cleanupPciSystem();
		return info;
	}

	info.valid = true;
	// Populate basic device information
	info.domain = dev->domain;
	info.bus = dev->bus;
	info.dev = dev->dev;
	info.func = dev->func;
	info.vendorId = dev->vendor_id;
	info.deviceId = dev->device_id;
	info.subvendorId = dev->subvendor_id;
	info.subdeviceId = dev->subdevice_id;
	info.deviceClass = dev->device_class;
	info.revision = dev->revision;

	DBG("PCI Device {:04x}:{:02x}:{:02x}.{}\n", info.domain, info.bus, info.dev, info.func);
	DBG("  Vendor ID: 0x{:04x}\n", info.vendorId);
	DBG("  Device ID: 0x{:04x}\n", info.deviceId);
	DBG("  Class: 0x{:06x}\n", info.deviceClass);
	// Read SRIOV capability
	readSriovCapability(dev, info);

	// Print SRIOV information if available
	if (info.isSriovCapable) {
		DBG("  SRIOV Capable: Yes\n");
		DBG("  Total VFs: {}\n", info.totalVFs);
		DBG("  Initial VFs: {}\n", info.initialVFs);
		DBG("  Current VFs: {}\n", info.numVFs);
		DBG("  VF Stride: {}\n", info.vfStride);
		DBG("  VF Device ID: 0x{:04x}\n", info.vfDeviceId);
		DBG("  First VF Offset: {}\n", info.firstVFOffset);

		for (int i = 0; i < 6; i++) {
			if (info.vfBarSize[i] != 0) {
				DBG("  VF BAR{} Size: 0x{:08x}\n", i, info.vfBarSize[i]);
			}
		}
	} else {
		DBG("  SRIOV Capable: No\n");
	}

	cleanupPciSystem();
	return info;
}

/**
 * @brief Parse BDF string and query PCI device with SRIOV info
 *
 * @param[in] bdf_string BDF string in format "XXXX:XX:XX.X" or "XX:XX.X"
 * @return PciDeviceInfo structure with device information
 */
static PciDeviceInfo queryPciDeviceByBdf(const std::string &bdfString)
{
	uint16_t domain = 0;
	uint8_t bus, device, function;

	// Parse BDF string
	size_t colon1 = bdfString.find(':');
	if (colon1 == std::string::npos) {
		ERR("Invalid BDF format: {} (expected XXXX:XX:XX.X or XX:XX.X)\n", bdfString.c_str());
		return {};
	}

	size_t colon2 = bdfString.find(':', colon1 + 1);
	size_t dot = bdfString.find('.');

	try {
		if (colon2 != std::string::npos && dot != std::string::npos) {
			// Format: XXXX:XX:XX.X
			domain = static_cast<uint16_t>(std::stoul(bdfString.substr(0, colon1), nullptr, 16));
			bus = static_cast<uint8_t>(std::stoul(bdfString.substr(colon1 + 1, colon2 - colon1 - 1), nullptr, 16));
			device = static_cast<uint8_t>(std::stoul(bdfString.substr(colon2 + 1, dot - colon2 - 1), nullptr, 16));
			function = static_cast<uint8_t>(std::stoul(bdfString.substr(dot + 1), nullptr, 16));
		} else if (dot != std::string::npos) {
			// Format: XX:XX.X (domain assumed to be 0)
			domain = 0;
			bus = static_cast<uint8_t>(std::stoul(bdfString.substr(0, colon1), nullptr, 16));
			device = static_cast<uint8_t>(std::stoul(bdfString.substr(colon1 + 1, dot - colon1 - 1), nullptr, 16));
			function = static_cast<uint8_t>(std::stoul(bdfString.substr(dot + 1), nullptr, 16));
		} else {
			ERR("Invalid BDF format: {} (expected XXXX:XX:XX.X or XX:XX.X)\n", bdfString.c_str());
			return {};
		}
	} catch (const std::exception &e) {
		ERR("Error parsing BDF string {}: {}\n", bdfString.c_str(), e.what());
		return {};
	}

	return queryPciDevice(domain, bus, device, function);
}

/**
 * @brief Get the sysfs SR-IOV admin path for a device
 *
 * @param[in] drmCardName DRM card name (e.g., "card0")
 * @return std::string The sriov_admin sysfs base path, or empty string if not available
 */
static std::string getSriovAdminPath(const std::string &drmCardName)
{
	std::string sysfsPath = "/sys/class/drm/" + drmCardName + "/device/sriov_admin";
	if (fileExists(sysfsPath)) {
		DBG("Using sysfs SR-IOV admin path: %s\n", sysfsPath.c_str());
		return sysfsPath;
	}
	INFO("sysfs sriov_admin not found for %s\n", drmCardName.c_str());
	return "";
}

/**
 * @brief Round a positive integer down to the nearest power of two
 *
 * @param[in] value Value to round down
 * @return uint64_t Rounded power-of-two value, or 0 when value is 0
 */
static uint64_t roundDownPowOfTwo(uint64_t value)
{
	uint64_t rounded = 0;
	while (value != 0) {
		rounded = value;
		value &= (value - 1);
	}
	return rounded;
}

/**
 * @brief Parse a VRAM debugfs line into bytes
 *
 * Accepts raw byte values as well as KiB, MiB, or GiB suffixed values.
 *
 * @param[in] line Debugfs line containing a numeric VRAM value
 * @return uint64_t Parsed size in bytes, or 0 on failure
 */
static uint64_t parseVramValueInBytes(const std::string &line)
{
	size_t valueStart = line.find_first_of("0123456789");
	if (valueStart == std::string::npos) {
		return 0;
	}

	size_t valueEnd = line.find_first_not_of("0123456789", valueStart);
	uint64_t value = 0;
	try {
		value = std::stoull(line.substr(valueStart, valueEnd - valueStart));
	} catch (const std::exception &) {
		return 0;
	}

	if (line.find("GiB") != std::string::npos) {
		return value * 1024ULL * ONE_MB_IN_BYTES;
	}
	if (line.find("MiB") != std::string::npos) {
		return value * ONE_MB_IN_BYTES;
	}
	if (line.find("KiB") != std::string::npos) {
		return value * 1024ULL;
	}
	return value;
}

/**
 * @brief Read available VRAM size from xe debugfs
 *
 * Reads the "size" entry from tile0/vram_mm and returns the parsed value.
 *
 * @param[in] bdfAddress PCI BDF address (e.g., "0000:03:00.0")
 * @return uint64_t Available VRAM size in bytes, or 0 on failure
 */
static uint64_t readAvailableVram(const std::string &bdfAddress)
{
	std::string mmPath = "/sys/kernel/debug/dri/" + bdfAddress + "/tile0/vram_mm";
	std::ifstream ifs(mmPath);
	std::string line;

	if (!ifs.is_open()) {
		ERR("{} {}\n", errno == EACCES ? "Permission denied opening" : "Failed to open", mmPath.c_str());
		return 0;
	}

	while (std::getline(ifs, line)) {
		if (line.length() >= MAX_PATH) {
			break;
		}
		if (line.find("size") == std::string::npos) {
			continue;
		}

		return parseVramValueInBytes(line);
	}

	ERR("Failed to parse available VRAM from %s\n", mmPath.c_str());
	return 0;
}

/**
 * @brief Get the amount of free local memory (LMEM) available on the device
 *
 * Reads the visible_avail entry from tile0/vram_mm to determine the
 * amount of visible available memory on the GPU device.
 *
 * @param[in] path The debugfs path for the device (e.g., /sys/kernel/debug/dri/0000:03:00.0)
 * @return uint64_t Free LMEM size in bytes, or 0 if unable to read or parse the information
 */
static uint64_t getFreeLmemSize(const std::string &path, bool isIGPU)
{
	std::string mmPath = path + "/tile0/vram_mm";
	std::ifstream ifs(mmPath);
	std::string line;

	if (!ifs.is_open()) {
		if (isIGPU) {
			DBG("{} {}, continuing with LMEM=0.\n", errno == EACCES ? "Permission denied opening" : "Failed to open",
				mmPath.c_str());
		} else {
			ERR("{} {}\n", errno == EACCES ? "Permission denied opening" : "Failed to open", mmPath.c_str());
		}
		return 0;
	}

	while (std::getline(ifs, line)) {
		if (line.length() >= MAX_PATH) {
			break;
		}
		if (line.find("visible_avail") == std::string::npos) {
			continue;
		}

		return parseVramValueInBytes(line);
	}

	ERR("Failed to parse visible_avail from %s\n", mmPath.c_str());
	return 0;
}

/**
 * @brief Internal function to create SRIOV Virtual Functions
 *
 * Performs the actual VF creation by programming vfN/profile/vram_quota
 * for each requested VF and then enabling the VF count.
 *
 * @param[in] drmPath DRM card name (e.g., "card0")
 * @param[in] numVfs Number of Virtual Functions to create
 * @param[in] lmem Local memory size per VF in bytes
 * @param[in] isIGPU true if the device is an integrated GPU
 * @return bool true on success, false on failure
 */
static bool createVfInternal(const std::string drmPath, uint32_t numVfs, uint64_t lmem, bool isIGPU)
{
	std::string devicePathString = std::string("/sys/class/drm/") + drmPath + "/device";
	std::string sriovAdminPath = getSriovAdminPath(drmPath);
	if (sriovAdminPath.empty()) {
		return false;
	}

	if (writeFile(sriovAdminPath + "/pf/profile/sched_priority", "normal") != 0) {
		return false;
	}
	for (uint32_t vfNum = 1; vfNum <= numVfs; vfNum++) {
		if (!isIGPU) {
			std::string quotaPath = sriovAdminPath + "/vf" + std::to_string(vfNum) + "/profile/vram_quota";
			if (writeFile(quotaPath, std::to_string(lmem)) != 0) {
				return false;
			}
		}
	}

	if (writeFile(devicePathString + "/sriov_drivers_autoprobe", "0") != 0) {
		return false;
	}
	if (writeFile(devicePathString + "/sriov_numvfs", std::to_string(numVfs)) != 0) {
		return false;
	}
	return true;
}

/**
 * @brief Load SRIOV-related data and calculate available resources
 *
 * Reads current SRIOV resource availability from debugfs and updates the
 * DeviceSriovInfo structure with free/spare resource counts for LMEM, GGTT,
 * doorbells, and contexts.
 *
 * @param[in,out] data Pointer to DeviceSriovInfo structure to populate with resource information
 * @return bool true on success, false if unable to read resource files
 */
static bool loadSriovData(DeviceSriovInfo *data)
{
	std::string lmem, ggtt, doorbell, context;
	std::string debugfsPath = std::string("/sys/kernel/debug/dri/") + data->bdfAddress;
	data->lmemSizeFree = getFreeLmemSize(debugfsPath, data->isIGPU);

	std::string pfIovPath = debugfsPath + "/gt" + std::to_string(0) + "/pf/";
	if (data->isIGPU) {
		lmem = "0";
		std::ifstream lmemFile(pfIovPath + "lmem_spare");
		if (lmemFile.is_open()) {
			std::string tmp;
			if (std::getline(lmemFile, tmp) && !tmp.empty()) {
				lmem = tmp;
			}
		}
	} else if (readFile(pfIovPath + "lmem_spare", lmem) != 0) {
		return false;
	}
	if (readFile(pfIovPath + "ggtt_spare", ggtt) != 0) {
		return false;
	}
	if (readFile(pfIovPath + "doorbells_spare", doorbell) != 0) {
		return false;
	}
	if (readFile(pfIovPath + "contexts_spare", context) != 0) {
		return false;
	}
	const uint64_t lmemValue = std::stoul(lmem);
	data->lmemSizeFree = (data->lmemSizeFree >= lmemValue) ? (data->lmemSizeFree - lmemValue) : 0;
	data->ggttSizeFree += std::stoul(ggtt);
	data->contextFree += static_cast<uint32_t>(std::stoul(context));
	data->doorbellFree += static_cast<uint32_t>(std::stoul(doorbell));
	DBG("   lmemSizeFree: {}\n   ggttSizeFree: {}\n   contextFree: {}\n   doorbellFree: {}\n", data->lmemSizeFree,
		data->ggttSizeFree, data->contextFree, data->doorbellFree);

	return true;
}

/**
 * @brief Extract card name from DRM path
 *
 * Parses the DRM device path to extract the card name (e.g., "card0").
 *
 * @param[in] drmPath Full DRM device path (e.g., "/dev/dri/card0")
 * @return std::string Extracted card name
 */
static std::string getCardNameFromDrmPath(const std::string &drmPath)
{
	size_t lastSlash = drmPath.find_last_of('/');
	// Extract card name from drmPath (e.g., "/dev/dri/card1" -> "card1")
	if (lastSlash != std::string::npos && lastSlash + 1 < drmPath.length()) {
		return drmPath.substr(lastSlash + 1);
	} else {
		// Fallback: use the entire drmPath if parsing fails
		return drmPath;
	}
}

/**
 * @brief Create SRIOV Virtual Functions on Linux platform
 *
 * Main entry point for VF creation. Validates the device supports SRIOV,
 * reads configuration, checks resource availability, and creates the
 * requested number of Virtual Functions with appropriate resource allocation.
 *
 * @param[in,out] di Pointer to DeviceSriovInfo structure containing device
 *                      information including BDF address, DRM path, and VF requirements
 * @return int 0 on success, -1 on failure
 */
int linCreateVFs(DeviceSriovInfo *di)
{
	TRACING();
	std::string numVfsString;
	std::string numVfsPath;
	uint64_t availableVram = 0;
	uint64_t maxVfLmem = 0;
	std::string cardName = getCardNameFromDrmPath(di->drmPath);

	DBG("Creating {} VFs with {} MiB memory\n", di->vGpuNumber, di->vGpuMemorySize);
	di->vGpuMemorySize *= ONE_MB_IN_BYTES;

	const bool sriovDataLoaded = loadSriovData(di);
	if (!sriovDataLoaded) {
		if (!di->isIGPU) {
			ERR("Failed to load SRIOV data. Is this system in SRIOV mode?\n");
			return -1;
		} else {
			DBG("SR-IOV resource telemetry unavailable; continuing with minimal VF creation checks.\n");
		}
	}

	numVfsPath = "/sys/class/drm/" + cardName + "/device/sriov_numvfs";
	if (readFile(numVfsPath, numVfsString) != 0) {
		return -1;
	}

	const int currentNumVfs = std::stoi(numVfsString);
	if (currentNumVfs > 0) {
		ERR("VFs are already enabled (sriov_numvfs={}). Remove existing VFs before creating {} VFs.\n", currentNumVfs,
			di->vGpuNumber);
		return -1;
	}

	// Query a specific PCI device by BDF for SRIOV info
	PciDeviceInfo info = {};
	if (di->isIGPU) {
		std::string totalVfsString;
		const std::string totalVfsPath = "/sys/bus/pci/devices/" + di->bdfAddress + "/sriov_totalvfs";
		if (readFile(totalVfsPath, totalVfsString) == 0) {
			const int totalVfs = std::stoi(totalVfsString);
			if (di->vGpuNumber == 0 || static_cast<int>(di->vGpuNumber) > totalVfs) {
				ERR("Number of VFs specified ({}) out of range. Maximum permitted for this device is {}\n",
					di->vGpuNumber, totalVfs);
				return -1;
			}
		}
	} else {
		info = queryPciDeviceByBdf(di->bdfAddress);
		if (info.valid) {
			if (info.isSriovCapable) {
				if (di->vGpuNumber == 0 || di->vGpuNumber > info.totalVFs) {
					ERR("Number of VFs specified ({}) are out of range. Total permitted for this device is {}\n",
						di->vGpuNumber, info.totalVFs);
					return -1;
				}
			} else {
				ERR("Device does not support SRIOV\n");
				return -1;
			}
		}
	}

	if (!di->isIGPU) {
		availableVram = readAvailableVram(di->bdfAddress);
		if (availableVram == 0) {
			return -1;
		}

		maxVfLmem = roundDownPowOfTwo(availableVram / (1 + di->vGpuNumber));
		if (maxVfLmem == 0 || di->vGpuMemorySize > maxVfLmem) {
			ERR("Requested VF LMEM {} exceeds the maximum allowed {} for {} VFs\n", di->vGpuMemorySize, maxVfLmem,
				di->vGpuNumber);
			return -1;
		}
	}

	return createVfInternal(cardName, di->vGpuNumber, di->vGpuMemorySize, di->isIGPU) ? 0 : -1;
}

/**
 * @brief Remove all Virtual Functions from a SRIOV device
 *
 * Disables all VFs by setting sriov_numvfs to 0, which automatically
 * deallocates all VF resources and makes them available to the PF again.
 *
 * @param[in] devInfo Pointer to DeviceSriovInfo structure containing device information
 * @return int 0 on success, -1 on failure
 */
int removeAllVFs(DeviceSriovInfo *devInfo)
{
	TRACING();
	std::stringstream numvfsPath;
	std::string numVfsString;

	// Disable all VFs by setting sriov_numvfs to 0
	numvfsPath << "/sys/bus/pci/devices/" << devInfo->bdfAddress << "/sriov_numvfs";
	if (readFile(numvfsPath.str(), numVfsString) != 0) {
		return -1;
	}
	if (writeFile(numvfsPath.str(), "0") != 0) {
		return -1;
	}

	return 0;
}

/**
 * @brief List all SRIOV Virtual Functions for a device
 *
 * Enumerates the Physical Function and its associated Virtual Functions,
 * retrieving their BDF addresses and resource allocations.
 *
 * @param[in] di Pointer to DeviceSriovInfo structure containing device information
 * @param[out] result Reference to vector to populate with DeviceSriovInfo for each VF
 * @return int 0 on success, -1 on failure
 */
int linListVFs(DeviceSriovInfo *di, std::vector<DeviceSriovInfo> &result)
{
	TRACING();
	std::string numVfsString;
	std::string cardName = getCardNameFromDrmPath(di->drmPath);
	std::string devicePath = std::string("/sys/class/drm/") + cardName;
	std::string sriovAdminPath = getSriovAdminPath(cardName);
	std::stringstream numvfsPath;
	const std::string gtNum = std::to_string(0); // Assuming single GT (gt0)
	int numVfs = 0;

	if (!di->isIGPU && !loadSriovData(di)) {
		ERR("Failed to load SRIOV data for listing VFs. Check: debugfs mounted, SR-IOV enabled, sufficient "
			"privileges.\n");
		return -1;
	}

	DBG("device Path: {}\n", devicePath.c_str());
	numvfsPath << "/sys/bus/pci/devices/" << di->bdfAddress << "/sriov_numvfs";
	if (readFile(numvfsPath.str(), numVfsString) != 0) {
		return -1;
	}

	try {
		numVfs = std::stoi(numVfsString);
	} catch (std::invalid_argument &) {
		return -1;
	}
	DBG("{} VFs detected.", numVfs);
	std::string debugfsPath = std::string("/sys/kernel/debug/dri/") + di->bdfAddress;
	/*
	 *  Put PF info into index 0, and VF1..n into index 1..n respectively
	 */
	for (int functionIndex = 0; functionIndex <= numVfs; functionIndex++) {
		std::string lmemString, lmemPath, ueventPath;
		DeviceSriovInfo info = {};

		if (functionIndex == 0) {
			lmemPath = debugfsPath + "/gt" + gtNum + "/pf/" + "lmem_spare";
		} else if (!sriovAdminPath.empty()) {
			lmemPath = sriovAdminPath + "/vf" + std::to_string(functionIndex) + "/profile/vram_quota";
		} else {
			lmemPath = debugfsPath + "/gt" + gtNum + "/vf" + std::to_string(functionIndex) + "/lmem_quota";
		}

		// iGPU platforms may not expose LMEM quota. Default to 0 unless a value is readable.
		lmemString = "0";
		{
			std::ifstream lmemFile(lmemPath);
			if (lmemFile.is_open()) {
				std::string tmp;
				if (std::getline(lmemFile, tmp) && !tmp.empty()) {
					lmemString = tmp;
				}
			} else if (!di->isIGPU) {
				ERR("Failed to read LMEM quota for function {}: {}\n", functionIndex, lmemPath.c_str());
				return -1;
			}
		}
		info.functionType = (functionIndex == 0) ? DEVICE_FUNCTION_TYPE_PHYSICAL : DEVICE_FUNCTION_TYPE_VIRTUAL;
		info.vGpuNumber = static_cast<uint32_t>(functionIndex);
		info.vGpuMemorySize = std::stoul(lmemString);

		if (functionIndex == 0) {
			ueventPath = devicePath + "/device/uevent";
		} else {
			ueventPath = devicePath + "/device/virtfn" + std::to_string(functionIndex - 1) + "/uevent";
		}

		std::ifstream ifs(ueventPath);
		std::string line;
		while (std::getline(ifs, line)) {
			if (line.length() >= MAX_PATH) {
				ERR("Invalid line length in {}", ueventPath.c_str());
				return -1;
			}
			char bdfBuffer[MAX_PATH] = {0};
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg) // sscanf is required for PCI_SLOT_NAME parsing
			sscanf(line.c_str(), "PCI_SLOT_NAME=%s", bdfBuffer);
			if (bdfBuffer[0] != 0) {
				info.bdfAddress = bdfBuffer;
				DBG("BDF Address: {}\n", bdfBuffer);
				break;
			}
		}
		result.push_back(info);
	}
	return 0;
}

/**
 * @brief Check if the CPU supports Intel VT-x (VMX) virtualization
 *
 * Reads /proc/cpuinfo to determine if the "vmx" flag is present,
 * indicating support for Intel VT-x.
 *
 * @return bool true if VMX is supported, false otherwise
 */
bool isVmxSupported()
{
	std::ifstream cpuinfo("/proc/cpuinfo");
	std::string line;

	while (std::getline(cpuinfo, line)) {
		if (line.find("flags") == 0) { // Line starts with "flags"
			return line.find(" vmx ") != std::string::npos || line.find(" vmx\t") != std::string::npos;
		}
	}
	return false;
}

/**
 * @brief Check if an IOMMU device is present on the system
 *
 * Scans the /sys/class/iommu directory to see if any IOMMU instances
 * are listed, indicating that IOMMU is enabled on the system.
 *
 * @return bool true if an IOMMU device is found, false otherwise
 */
bool isIommuSupported()
{
	DIR *dir;
	struct dirent *ent;
	bool result = false;
	/*
	 *   The devices managed by IOMMU will be listed here: /sys/class/iommu/<iommu instance>/devices
	 *   So /sys/class/iommu/ non-empty means IOMMU enabled.
	 *   https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-class-iommu
	 */
	if ((dir = opendir(std::string("/sys/class/iommu").c_str())) != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
				result = true;
				break;
			}
		}
		closedir(dir);
	} else {
		ERR("Failed to open directory /sys/class/iommu\n");
	}
	return result;
}

/**
 * @brief Check if SRIOV is supported on the specified device
 *
 * Reads the sriov_totalvfs sysfs file to determine if the device
 * supports SRIOV by checking if the total number of VFs is greater than zero.
 *
 * @param[in] di Pointer to DeviceSriovInfo structure containing device information
 * @return bool true if SRIOV is supported, false otherwise
 */
bool isSriovSupported(DeviceSriovInfo *di)
{
	TRACING();
	std::string numVfsString;
	std::stringstream numvfsPath;

	numvfsPath << "/sys/bus/pci/devices/" << di->bdfAddress << "/sriov_totalvfs";
	if (readFile(numvfsPath.str(), numVfsString) != 0) {
		return false;
	}

	return std::stoi(numVfsString) > 0 ? true : false;
}
