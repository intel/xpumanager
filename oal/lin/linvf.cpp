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
#include <iomanip>
#include <pciaccess.h>
#include <filesystem>
#include <map>
#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <cinttypes>
#include <file_io.h>

static constexpr std::string_view VGPU_CONF_FILE = "resources/config/vgpu.conf";
#define XPUM_MAX_VF_NUM 128
#define PF_EXEC_QUANTUM_MS 64
#define PF_PREEMPT_TIMEOUT_US 128000
#define VF_EXEC_QUANTUM_US 2000
#define MAX_VF_EXEC_QUANTUM_MS 50
#define VF_EXEC_RATIO 0.5
#define ONE_MS_IN_US 1000
#define OFFSET_TOTAL_VF 0x0E
#define OFFSET_INITIAL_VF 0x0C
#define OFFSET_NUM_VF 0x10
#define OFFSET_VF_STRIDE 0x14
#define OFFSET_VF_DEVICE_ID 0x1A
#define OFFSET_FIRST_VF_OFFSET 0x18
#define OFFSET_VF_BAR 0x24

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
	uint8_t capPtr = 0;
	uint8_t capId = 0;
	uint16_t capReg = 0;

	// Find capabilities pointer
	if (pci_device_cfg_read_u8(dev, &capPtr, PCI_CAPABILITY_LIST) != 0) {
		return;
	}

	// Walk through capability list looking for SRIOV (0x10)
	while (capPtr != 0) {
		if (pci_device_cfg_read_u8(dev, &capId, capPtr) != 0) {
			break;
		}

		if (capId == PCI_CAP_ID_SRIOV) { // SRIOV capability
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

		// Move to next capability
		if (pci_device_cfg_read_u8(dev, &capPtr, capPtr + 1) != 0) {
			break;
		}
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
		ERR("PCI device %04x:%02x:%02x.%d not found\n", domain, bus, device, function);
		cleanupPciSystem();
		return info;
	}

	// Probe the device to get detailed information
	if (pci_device_probe(dev) != 0) {
		ERR("Failed to probe PCI device %04x:%02x:%02x.%d\n", domain, bus, device, function);
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

	DBG("PCI Device %04x:%02x:%02x.%d\n", info.domain, info.bus, info.dev, info.func);
	DBG("  Vendor ID: 0x%04x\n", info.vendorId);
	DBG("  Device ID: 0x%04x\n", info.deviceId);
	DBG("  Class: 0x%06x\n", info.deviceClass);
	// Read SRIOV capability
	readSriovCapability(dev, info);

	// Print SRIOV information if available
	if (info.isSriovCapable) {
		DBG("  SRIOV Capable: Yes\n");
		DBG("  Total VFs: %d\n", info.totalVFs);
		DBG("  Initial VFs: %d\n", info.initialVFs);
		DBG("  Current VFs: %d\n", info.numVFs);
		DBG("  VF Stride: %d\n", info.vfStride);
		DBG("  VF Device ID: 0x%04x\n", info.vfDeviceId);
		DBG("  First VF Offset: %d\n", info.firstVFOffset);

		for (int i = 0; i < 6; i++) {
			if (info.vfBarSize[i] != 0) {
				DBG("  VF BAR%d Size: 0x%08x\n", i, info.vfBarSize[i]);
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
		ERR("Invalid BDF format: %s (expected XXXX:XX:XX.X or XX:XX.X)\n", bdfString.c_str());
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
			ERR("Invalid BDF format: %s (expected XXXX:XX:XX.X or XX:XX.X)\n", bdfString.c_str());
			return {};
		}
	} catch (const std::exception &e) {
		ERR("Error parsing BDF string %s: %s\n", bdfString.c_str(), e.what());
		return {};
	}

	return queryPciDevice(domain, bus, device, function);
}

/**
 * @brief Get the amount of free local memory (LMEM) available on the device
 *
 * Reads the vram0_mm file from the debugfs path to determine the amount of
 * visible available memory on the GPU device.
 *
 * @param[in] path The debugfs path for the device (e.g., /sys/kernel/debug/dri/0000:03:00.0)
 * @return uint64_t Free LMEM size in bytes, or 0 if unable to read or parse the information
 */
static uint64_t getFreeLmemSize(const std::string &path)
{
	std::ifstream ifs(path + "/vram0_mm");
	std::string line;
	uint64_t freeSize = 0;

	if (!ifs.is_open()) {
		ERR("Failed to open %s/vram0_mm\n", path.c_str());
		return 0;
	}

	while (std::getline(ifs, line)) {
		if (line.length() >= MAX_PATH) {
			break;
		}
		if (strstr(line.c_str(), "visible_avail") == NULL) {
			continue;
		}
		sscanf(line.c_str(), "%*[^0-9]%lu", &freeSize);
		freeSize = freeSize * ONE_MB_IN_BYTES; // MiB to bytes
		break;
	}
	ifs.close();
	return freeSize;
}

/**
 * @brief Write VF (Virtual Function) attributes to sysfs
 *
 * Configures a specific VF by writing its resource quotas and scheduling
 * parameters to the appropriate sysfs files in debugfs.
 *
 * @param[in] vfDir The VF directory path in debugfs (e.g., /sys/kernel/debug/dri/.../gt0/vf1)
 * @param[in] attrs Configuration attributes from the config file
 * @param[in] lmem Local memory quota to assign to this VF in bytes
 * @return int 0 on success, -1 on failure
 */
static int writeVfAttrToSysfs(std::string vfDir, AttrFromConfigFile attrs, uint64_t lmem)
{
	if (writeFile(vfDir + "/exec_quantum_ms", std::to_string(attrs.vfExec)) != 0) {
		return -1;
	}
	if (writeFile(vfDir + "/preempt_timeout_us", std::to_string(attrs.vfPreempt)) != 0) {
		return -1;
	}
	if (writeFile(vfDir + "/lmem_quota", std::to_string(lmem)) != 0) {
		return -1;
	}
	if (writeFile(vfDir + "/ggtt_quota", std::to_string(attrs.vfGgtt)) != 0) {
		return -1;
	}
	if (writeFile(vfDir + "/doorbells_quota", std::to_string(attrs.vfDoorbells)) != 0) {
		return -1;
	}
	if (writeFile(vfDir + "/contexts_quota", std::to_string(attrs.vfContexts)) != 0) {
		return -1;
	}
	return 0;
}

/**
 * @brief Internal function to create SRIOV Virtual Functions
 *
 * Performs the actual VF creation by:
 * 1. Setting PF (Physical Function) scheduling parameters
 * 2. Configuring each VF with resource quotas
 * 3. Enabling driver autoprobe if configured
 * 4. Setting the number of VFs to create
 *
 * @param[in] drmPath DRM device path (e.g., "card0")
 * @param[in] bdfAddress PCI Bus:Device.Function address (e.g., "0000:03:00.0")
 * @param[in,out] attrs Configuration attributes from config file
 * @param[in] numVfs Number of Virtual Functions to create
 * @param[in] lmem Local memory size per VF in bytes
 * @return bool true on success, false on failure
 */
static bool createVfInternal(std::string drmPath, std::string bdfAddress, AttrFromConfigFile &attrs, uint32_t numVfs,
							 uint64_t lmem)
{
	std::string devicePathString = std::string("/sys/class/drm/") + drmPath;
	std::string debugfsPath = std::string("/sys/kernel/debug/dri/") + bdfAddress;
	std::string gtNum = std::to_string(0); // Assuming single GT (gt0)
	if (writeFile(debugfsPath + "/gt" + gtNum + "/pf/exec_quantum_ms", std::to_string(attrs.pfExec)) != 0) {
		return false;
	}
	if (writeFile(debugfsPath + "/gt" + gtNum + "/pf/preempt_timeout_us", std::to_string(attrs.pfPreempt)) != 0) {
		return false;
	}
	if (writeFile(debugfsPath + "/gt" + gtNum + "/pf/sched_if_idle", attrs.schedIfIdle ? "1" : "0") != 0) {
		return false;
	}

	for (uint32_t vfNum = 1; vfNum <= numVfs; vfNum++) {
		std::string vfResPath = debugfsPath + "/gt" + gtNum + "/vf" + std::to_string(vfNum);
		if (writeVfAttrToSysfs(vfResPath, attrs, lmem) != 0) {
			return false;
		}
	}
	if (writeFile(devicePathString + "/device/sriov_drivers_autoprobe", attrs.driversAutoprobe ? "1" : "0") != 0) {
		return false;
	}
	if (writeFile(devicePathString + "/device/sriov_numvfs", std::to_string(numVfs)) != 0) {
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
	data->lmemSizeFree = getFreeLmemSize(debugfsPath);

	std::string pfIovPath = debugfsPath + "/gt" + std::to_string(0) + "/pf/";
	if (readFile(pfIovPath + "lmem_spare", lmem) != 0) {
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
	data->lmemSizeFree -= std::stoul(lmem);
	data->ggttSizeFree += std::stoul(ggtt);
	data->contextFree += std::stoi(context);
	data->doorbellFree += std::stoi(doorbell);
	DBG("   lmemSizeFree: %lu\n   ggttSizeFree: %lu\n   contextFree: %u\n   doorbellFree: %u\n", data->lmemSizeFree,
		data->ggttSizeFree, data->contextFree, data->doorbellFree);

	return true;
}

/**
 * @brief Update VGPU scheduler configuration parameters based on device type
 *
 * Sets device-specific scheduling parameters (execution quantum, preemption timeout,
 * etc.) based on the GPU device ID and requested scheduler type. Different
 * configurations are applied for different Intel GPU generations.
 *
 * @param[in] numVfs Number of Virtual Functions being configured
 * @param[in,out] data Reference to map containing configuration data to update
 */
static void updateVgpuSchedulerConfigParameters(int numVfs, std::map<uint32_t, AttrFromConfigFile> &data)
{

	data[numVfs].pfExec = PF_EXEC_QUANTUM_MS;
	data[numVfs].pfPreempt = PF_PREEMPT_TIMEOUT_US;
	data[numVfs].schedIfIdle = false;
	data[numVfs].vfExec =
		std::min(int(VF_EXEC_QUANTUM_US / std::max(numVfs - 1, 1) * VF_EXEC_RATIO), MAX_VF_EXEC_QUANTUM_MS);
	data[numVfs].vfPreempt =
		(VF_EXEC_QUANTUM_US / std::max(numVfs - 1, 1) -
		 std::min(int((VF_EXEC_QUANTUM_US / std::max(numVfs - 1, 1)) * VF_EXEC_RATIO), MAX_VF_EXEC_QUANTUM_MS)) *
		ONE_MS_IN_US;
}

/*
 * @brief Combine and finalize VF configuration attributes
 *
 * Merges device-specific and default configuration attributes for the requested
 * number of VFs. Calculates per-VF resource quotas based on total available
 * resources and updates scheduling parameters based on device type.
 *
 * @param[in] data Map of configuration attributes indexed by number of VFs
 * @param[in] numVfs Number of Virtual Functions being configured
 * @return AttrFromConfigFile Finalized configuration attributes for the specified number of VFs
 */
static AttrFromConfigFile combineAttrConfig(std::map<uint32_t, AttrFromConfigFile> data, uint32_t numVfs)
{
	if (data.size() == 1)
		return data.begin()->second;
	if (data.find(numVfs) == data.end()) {
		data[numVfs].driversAutoprobe = data[XPUM_MAX_VF_NUM].driversAutoprobe;
		data[numVfs].schedIfIdle = data[XPUM_MAX_VF_NUM].schedIfIdle;
	}
	if (data[numVfs].vfLmem == 0 && data[XPUM_MAX_VF_NUM].vfLmem != 0)
		data[numVfs].vfLmem = data[XPUM_MAX_VF_NUM].vfLmem / static_cast<uint64_t>(numVfs);
	if (data[numVfs].vfLmemEcc == 0 && data[XPUM_MAX_VF_NUM].vfLmemEcc != 0)
		data[numVfs].vfLmemEcc = data[XPUM_MAX_VF_NUM].vfLmemEcc / static_cast<uint64_t>(numVfs);
	if (data[numVfs].vfContexts == 0)
		data[numVfs].vfContexts = data[XPUM_MAX_VF_NUM].vfContexts;
	if (data[numVfs].vfDoorbells == 0 && data[XPUM_MAX_VF_NUM].vfDoorbells != 0)
		data[numVfs].vfDoorbells = data[XPUM_MAX_VF_NUM].vfDoorbells / static_cast<uint64_t>(numVfs);
	if (data[numVfs].vfGgtt == 0 && data[XPUM_MAX_VF_NUM].vfGgtt != 0)
		data[numVfs].vfGgtt = data[XPUM_MAX_VF_NUM].vfGgtt / static_cast<uint64_t>(numVfs);
	updateVgpuSchedulerConfigParameters(numVfs, data);
	return data[numVfs];
}

/**
 * @brief Read VF configuration from the vgpu.conf file
 *
 * Parses the VGPU configuration file to extract device-specific settings
 * for the requested number of VFs. Handles both device-specific configurations
 * and default fallback values.
 *
 * @param[in] deviceId PCI device ID to look up configuration for
 * @param[in] numVfs Number of VFs being configured
 * @param[out] attrs Reference to structure to populate with configuration
 * @return bool true if configuration was successfully read, false on error
 */
static bool readVfConfigFromFile(uint32_t deviceId, uint32_t numVfs, AttrFromConfigFile &attrs)
{
	TRACING();

	std::string line;
	std::map<uint32_t, AttrFromConfigFile> data;
	uint32_t currentNameId = 0;
	std::string defaultVgpuScheduler;

	std::string fileName = findResourceFile(std::string(VGPU_CONF_FILE));
	if (!fileExists(fileName)) {
		ERR("%s file does not exist\n", fileName.c_str());
		return false;
	}

	std::ostringstream oss;
	oss << std::hex << std::setw(4) << std::setfill('0') << deviceId;
	std::string deviceIdStr = oss.str();

	std::ifstream ifs(fileName);
	if (ifs.fail()) {
		ERR("Unable to open %s\n", fileName.c_str());
		return false;
	}

	while (std::getline(ifs, line)) {
		line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
		if (line[0] == '#' || line.empty()) {
			continue;
		}

		std::istringstream lineStream(line);
		std::string key, value;
		if (std::getline(lineStream, key, '=') && std::getline(lineStream, value)) {
			if (strcmp(key.c_str(), "NAME") == 0) {
				char s[16];
				uint32_t i;
				auto nameList = split(value, ',');
				for (auto nameItem : nameList) {
					if (nameItem.find("DEF") != std::string::npos) {
						// Parse format: "56c0DEF" -> extract first 4 chars before "DEF"
						if (nameItem.length() >= 7 && nameItem.substr(4, 3) == "DEF") {
							strncpy(s, nameItem.c_str(), 4);
							s[4] = '\0';
							if (strcmp(s, deviceIdStr.c_str()) == 0) {
								currentNameId = XPUM_MAX_VF_NUM;
								break;
							}
						}
						currentNameId = 0;
					} else {
						// Parse format: "56c0N4" -> extract device ID and number
						size_t nPos = nameItem.find('N');
						if (nPos != std::string::npos && nPos == 4) {
							strncpy(s, nameItem.c_str(), 4);
							s[4] = '\0';
							i = std::stoi(nameItem.substr(nPos + 1));
							if (strcmp(s, deviceIdStr.c_str()) == 0 && i == numVfs) {
								currentNameId = i;
								break;
							}
						}
						currentNameId = 0;
					}
				}
			} else if (strcmp(key.c_str(), "VF_LMEM") == 0 && currentNameId) {
				data[currentNameId].vfLmem = std::stoul(value);
			} else if (strcmp(key.c_str(), "VF_LMEM_ECC") == 0 && currentNameId) {
				data[currentNameId].vfLmemEcc = std::stoul(value);
			} else if (strcmp(key.c_str(), "VF_CONTEXTS") == 0 && currentNameId) {
				data[currentNameId].vfContexts = std::stoi(value);
			} else if (strcmp(key.c_str(), "VF_DOORBELLS") == 0 && currentNameId) {
				data[currentNameId].vfDoorbells = std::stoi(value);
			} else if (strcmp(key.c_str(), "VF_GGTT") == 0 && currentNameId) {
				data[currentNameId].vfGgtt = std::stoul(value);
			} else if (strcmp(key.c_str(), "VGPU_SCHEDULER") == 0 && currentNameId) {
				updateVgpuSchedulerConfigParameters(currentNameId, data);
				if (currentNameId == XPUM_MAX_VF_NUM)
					defaultVgpuScheduler = value;
			} else if (strcmp(key.c_str(), "DRIVERS_AUTOPROBE") == 0 && currentNameId) {
				data[currentNameId].driversAutoprobe = (bool)std::stoi(value);
				if (currentNameId != XPUM_MAX_VF_NUM) {
					DBG("Found predefined vgpu configuration from vgpu.conf\n");
					break;
				}
			}
		}
	}

	if (data.size()) {
		attrs = combineAttrConfig(data, numVfs);
	}

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
	std::string cardName = getCardNameFromDrmPath(di->drmPath);

	DBG("Creating %d VFs with %" PRIu64 " MB memory\n", di->vGpuNumber, di->vGpuMemorySize);
	di->vGpuMemorySize *= ONE_MB_IN_BYTES;

	if (!loadSriovData(di)) {
		ERR("Failed to load SRIOV data. Is this system in SRIOV mode?\n");
		return -1;
	}

	numVfsPath = "/sys/class/drm/" + cardName + "/device/sriov_numvfs";

	if (readFile(numVfsPath, numVfsString) != 0) {
		return -1;
	}

	if (std::stoi(numVfsString) > 0) {
		return -1;
	}

	// Query a specific PCI device by BDF for SRIOV info
	PciDeviceInfo info = queryPciDeviceByBdf(di->bdfAddress);
	if (info.valid) {
		if (info.isSriovCapable) {
			if (di->vGpuNumber == 0 || di->vGpuNumber > info.totalVFs) {
				ERR("Number of VFs specified (%d) are out of range. Total permitted for this device is %d\n",
					di->vGpuNumber, info.totalVFs);
				return -1;
			}
		} else {
			ERR("Device does not support SRIOV\n");
			return -1;
		}
	}

	AttrFromConfigFile attrs = {};
	bool readFlag = readVfConfigFromFile(info.deviceId, di->vGpuNumber, attrs);
	if (!readFlag) {
		return -1;
	}

	if (attrs.vfLmem == 0) {
		ERR("Configuration item for %d VFs not found\n", di->vGpuNumber);
		return -1;
	}
	uint64_t lmemToUse = 0;
	if (di->vGpuMemorySize > 0) {
		lmemToUse = di->vGpuMemorySize;
	} else if (di->eccState == ZES_DEVICE_ECC_STATE_ENABLED) {
		lmemToUse = attrs.vfLmemEcc;
	} else {
		lmemToUse = attrs.vfLmem;
	}

	if (di->lmemSizeFree < lmemToUse * di->vGpuNumber) {
		ERR("LMEM size too large\n");
		return -1;
	}
	return createVfInternal(cardName, di->bdfAddress, attrs, di->vGpuNumber, di->vGpuMemorySize) ? 0 : -1;
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
	std::stringstream iovPath, numvfsPath;
	std::string numVfsString;
	AttrFromConfigFile zeroAttr = {};

	// Disable all VFs by setting sriov_numvfs to 0
	numvfsPath << "/sys/bus/pci/devices/" << devInfo->bdfAddress << "/sriov_numvfs";
	if (readFile(numvfsPath.str(), numVfsString) != 0) {
		return -1;
	}
	if (writeFile(numvfsPath.str(), "0") != 0) {
		return -1;
	}

	std::string debugfsPath = std::string("/sys/kernel/debug/dri/") + devInfo->bdfAddress;
	int numVfs = std::stoi(numVfsString);

	try {
		for (int functionIndex = 1; functionIndex <= numVfs; functionIndex++) {
			writeVfAttrToSysfs(debugfsPath + "/gt0/vf" + std::to_string(functionIndex), zeroAttr, 0);
		}
	} catch (std::ios::failure &) {
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
	std::stringstream numvfsPath;
	std::string gtNum = std::to_string(0); // Assuming single GT (gt0)
	int numVfs;

	if (!loadSriovData(di)) {
		return -1;
	}

	DBG("device Path: %s\n", devicePath.c_str());
	numvfsPath << "/sys/bus/pci/devices/" << di->bdfAddress << "/sriov_numvfs";
	if (readFile(numvfsPath.str(), numVfsString) != 0) {
		return -1;
	}

	try {
		numVfs = std::stoi(numVfsString);
	} catch (std::invalid_argument &) {
		return -1;
	}
	DBG("%d VFs detected.", numVfs);
	std::string debugfsPath = std::string("/sys/kernel/debug/dri/") + di->bdfAddress;
	/*
	 *  Put PF info into index 0, and VF1..n into index 1..n respectively
	 */
	for (int functionIndex = 0; functionIndex <= numVfs; functionIndex++) {
		std::string lmemString, uevent, lmemPath, ueventPath;
		DeviceSriovInfo info = {};

		if (functionIndex == 0) {
			lmemPath = debugfsPath + "/gt" + gtNum + "/pf/" + "lmem_spare";
		} else {
			lmemPath = debugfsPath + "/gt" + gtNum + "/vf" + std::to_string(functionIndex) + "/lmem_quota";
		}

		if (readFile(lmemPath.c_str(), lmemString) != 0) {
			return -1;
		}
		info.functionType = (functionIndex == 0) ? DEVICE_FUNCTION_TYPE_PHYSICAL : DEVICE_FUNCTION_TYPE_VIRTUAL;
		info.vGpuNumber = functionIndex;
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
				ERR("Invalid line length in %s", ueventPath.c_str());
				return -1;
			}
			char bdfBuffer[MAX_PATH] = {0};
			sscanf(line.c_str(), "PCI_SLOT_NAME=%s", bdfBuffer);
			if (bdfBuffer[0] != 0) {
				info.bdfAddress = bdfBuffer;
				DBG("BDF Address: %s\n", bdfBuffer);
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