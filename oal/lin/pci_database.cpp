/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file pci_database.cpp
 */

#include <unistd.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include "debug.h"
#include "os.h"
#include "pci_database.h"
#include "lin.h"

/**
 * @brief Constructor for PciDatabase class
 *
 * Initializes the PCI database object with uninitialized state.
 * The actual initialization is performed lazily when the instance is first accessed.
 */
PciDatabase::PciDatabase() { bInitialized = false; }

/**
 * @brief Returns the singleton instance of PciDatabase
 *
 * This function implements the singleton pattern to ensure only one instance
 * of PciDatabase exists. It performs lazy initialization of the database
 * on first access, loading PCI device information from configuration files.
 *
 * @return PciDatabase& Reference to the singleton instance
 */
PciDatabase &PciDatabase::instance()
{
	static PciDatabase instance;
	std::unique_lock<std::mutex> lock(instance.mutex);

	if (!instance.bInitialized) {
		if (!instance.init()) {
			ERR("Failed to initialize PciDatabase, Device topology function does not work!\n");
		}
	}
	instance.bInitialized = true;
	return instance;
}

/**
 * @brief Initializes the PCI database by loading configuration files
 *
 * This function loads PCI device information from two configuration files:
 * - PCI IDs file containing standard PCI vendor and device information
 * - Device configuration file containing custom device classifications
 * It attempts to find these files in the resources/config directory.
 *
 * @return bool true if initialization was successful, false otherwise
 */
bool PciDatabase::init()
{
	std::ifstream infile;
	std::string fileName;
	bool ret = true;

	// Try to find pci.ids file
	fileName = findResourceFile("resources/config/" + std::string(PCI_IDS_FILE));
	infile.open(fileName.data());

	if (infile.is_open()) {
		if (!parsePciDevice(infile)) {
			ERR("PciDatabase::init()- parsePciDevice error.\n");
			ret = false;
		}
		infile.close();
	} else {
		ERR("PciDatabase::init()- open file %s error.\n", fileName.c_str());
		ret = false;
	}

	// Try to find pci.conf file
	fileName = findResourceFile("resources/config/" + std::string(PCI_IDS_CONFIG));
	infile.open(fileName.data());

	if (infile.is_open()) {
		parseDeviceConfig(infile);
		infile.close();
	} else {
		ret = false;
		ERR("PciDatabase::init()- open file %s error.\n", fileName.c_str());
	}

	return ret;
}

/**
 * @brief Checks if a line is a comment
 *
 * This function checks if a given line is a comment by looking for
 * a '#' character at the beginning of the line (after any leading
 * whitespace).
 *
 * @param line The line to check
 * @return true if the line is a comment, false otherwise
 */
bool PciDatabase::lineIsComment(const std::string &line)
{
	std::size_t idx = 0;
	std::size_t len = line.length();
	bool comment = false;

	while (idx < len) {
		if (isBlankSpace(line.at(idx))) {
			idx++;
		} else if ((line.at(idx) == '#')) {
			comment = true;
			break;
		} else {
			break;
		}
	}
	return comment ? true : false;
}

/**
 * @brief Parses the main PCI device database file
 *
 * This function reads and parses the standard PCI IDs file which contains
 * hierarchical information about PCI vendors, devices, and subsystems.
 * The file format uses indentation levels to represent the hierarchy:
 * - Level 0: Vendor information
 * - Level 1: Device information
 * - Level 2: Subsystem information
 *
 * @param fstream Input file stream to the PCI IDs file
 * @return bool True if parsing completed, false on error
 */
bool PciDatabase::parsePciDevice(std::ifstream &fstream)
{
	bool bResult = false;
	int vendorId = 0, deviceId = 0, subVendorId = 0, subDeviceId = 0;
	std::string vendorName, deviceName, subsystemName;
	id_type idtype = ID_UNKNOWN;
	std::string info;

	while (std::getline(fstream, info)) {
		int level = 0;
		std::size_t idx = 0;
		std::size_t len = info.length();

		if (len == 0) {
			continue;
		}

		if (lineIsComment(info)) {
			continue;
		}

		idx = 0;
		while (idx < len) {
			if (info.at(idx) == '\t') {
				idx++;
				level++;
			} else {
				break;
			}
		}

		if (level == 0) {
			deviceId = subVendorId = subDeviceId = -1;
			bResult = parseLevel0(info, (int)len, &idtype, &vendorId, &idx);
			std::string name = info.substr(idx);
			vendorName = name.erase(0, name.find_first_not_of(' '));
		} else if (level == 1) {
			deviceId = subVendorId = subDeviceId = -1;
			bResult = parseLevel1(info, (int)len, &idtype, &deviceId, &idx);
			std::string name = info.substr(idx);
			deviceName = name.erase(0, name.find_first_not_of(' '));

			if (idtype != ID_KNOWN_D_CLASS) {
				addSwitchDevice(vendorId, deviceId, deviceName, subVendorId, subDeviceId, subsystemName);
			}
		} else if (level == 2) {
			subVendorId = subDeviceId = -1;
			bResult = parseLevel2(info, (int)len, &idtype, &subVendorId, &subDeviceId, &idx);
			std::string name = info.substr(idx);
			subsystemName = name.erase(0, name.find_first_not_of(' '));
			if (idtype != ID_KNOWN_D_CLASS) {
				addSwitchDevice(vendorId, deviceId, deviceName, subVendorId, subDeviceId, subsystemName);
			}
		} else {
			// level 3 if not defined, the code should not be here, just ignore this line
			continue;
		}
	}

	return bResult;
}

/**
 * @brief Parses level 0 (vendor) entries from PCI database
 *
 * This function parses the top-level entries in the PCI database file,
 * which represent vendor information or device class information.
 * Level 0 entries are not indented and can be vendor IDs (4 hex digits)
 * or device class identifiers (starting with 'C').
 *
 * @param info The line of text to parse
 * @param len Length of the input line
 * @param type Pointer to store the parsed entry type
 * @param vendorId Pointer to store the parsed vendor ID
 * @param idx Pointer to store the index where the name starts
 * @return bool True if parsing succeeded, false otherwise
 */
bool PciDatabase::parseLevel0(const std::string &info, int len, id_type *type, int *vendorId, std::size_t *idx)
{
	bool bResult = false;
	int minLevel0Len = 5;
	if (info.at(0) == 'C' && len >= 2 && isBlankSpace(info.at(1))) {
		// known device classes, subclasses and programming interfaces
		if (len > minLevel0Len) {
			*vendorId = std::stoi(info.substr(2, 2).c_str(), nullptr, 16);
			if (*vendorId >= 0) {
				*type = ID_KNOWN_D_CLASS;
				*idx = minLevel0Len;
				bResult = true;
			}
		}
	} else if (info.at(0) >= 'A' && info.at(0) <= 'Z' && len >= 2 && isBlankSpace(info.at(1))) {
		*type = ID_UNKNOWN;
		bResult = true;
	} else {
		if (len > minLevel0Len) {
			*vendorId = std::stoi(info.substr(0, 4).c_str(), nullptr, 16);
			if (*vendorId >= 0 && isBlankSpace(info.at(4))) {
				*type = ID_VENDOR;
				*idx = minLevel0Len;
				bResult = true;
			}
		}
	}
	return bResult;
}

/**
 * @brief Parses level 1 (device) entries from PCI database
 *
 * This function parses the second-level entries in the PCI database file,
 * which represent device information under a vendor or subclass information
 * under a device class. Level 1 entries are indented with one tab.
 *
 * @param info The line of text to parse
 * @param len Length of the input line
 * @param type Pointer to the current entry type (input/output)
 * @param device_id Pointer to store the parsed device ID
 * @param idx Pointer to store the index where the name starts
 * @return bool True if parsing succeeded, false otherwise
 */
bool PciDatabase::parseLevel1(const std::string &info, int len, id_type *type, int *deviceId, std::size_t *idx)
{
	bool bResult = false;
	int start = 1;
	int minLevel1Len;
	switch (*type) {
	case ID_UNKNOWN: {
		bResult = true;
	} break;
	case ID_KNOWN_D_CLASS: {
		minLevel1Len = start + 3;
		*deviceId = std::stoi(info.substr(start, 2).c_str(), nullptr, 16);
		if (*deviceId >= 0 && isBlankSpace(info.at(start + 2))) {
			*idx = minLevel1Len;
			bResult = true;
		}
	} break;
	case ID_VENDOR:
	case ID_DEVICE:
	case ID_SUB_SYS: {
		minLevel1Len = start + 5;
		if (len > minLevel1Len) {
			*deviceId = std::stoi(info.substr(start, 4).c_str(), nullptr, 16);
			if (*deviceId >= 0 && isBlankSpace(info.at(start + 4))) {
				*type = ID_DEVICE;
				*idx = minLevel1Len;
				bResult = true;
			}
		}
	} break;
	}
	return bResult;
}

/**
 * @brief Parses level 2 (subsystem) entries from PCI database
 *
 * This function parses the third-level entries in the PCI database file,
 * which represent subsystem information under a device. Level 2 entries
 * are indented with two tabs and contain subsystem vendor and device IDs.
 *
 * @param info The line of text to parse
 * @param len Length of the input line
 * @param type Pointer to the current entry type (input/output)
 * @param subVendorId Pointer to store the parsed subsystem vendor ID
 * @param subDeviceId Pointer to store the parsed subsystem device ID
 * @param idx Pointer to store the index where the name starts
 * @return bool True if parsing succeeded, false otherwise
 */
bool PciDatabase::parseLevel2(const std::string &info, int len, id_type *type, int *subVendorId, int *subDeviceId,
							  std::size_t *idx)
{
	bool bResult = false;
	int start = 2;
	int minLevel2Len;
	switch (*type) {
	case ID_UNKNOWN: {
		bResult = true;
	} break;
	case ID_KNOWN_D_CLASS: {
		minLevel2Len = 5;
		bResult = true;
	} break;
	case ID_DEVICE:
	case ID_SUB_SYS: {
		minLevel2Len = 12;
		if (len > minLevel2Len) {
			*subVendorId = std::stoi(info.substr(start, 4), nullptr, 16);
			if (*subVendorId >= 0 && isBlankSpace(info.at(start + 4))) {
				start = start + 5;
				*subDeviceId = std::stoi(info.substr(start, 4), nullptr, 16);
				if (*subDeviceId >= 0 && isBlankSpace(info.at(start + 4))) {
					*type = ID_SUB_SYS;
					bResult = true;
					*idx = minLevel2Len;
				}
			}
		}
	} break;
	case ID_VENDOR: {
		ERR("PciDatabase::parseLevel2() error- unknown device.\n");
		bResult = true;
	} break;
	}

	return bResult;
}

/**
 * @brief Checks if a character is a whitespace character
 *
 * This utility function determines whether the given character is
 * considered whitespace for parsing purposes (space or tab).
 *
 * @param c The character to check
 * @return bool True if the character is space or tab, false otherwise
 */
bool PciDatabase::isBlankSpace(const char c) { return ((c == ' ') || (c == '\t')); }

/**
 * @brief Parses the device configuration file
 *
 * This function reads and parses a custom device configuration file that
 * contains specific device classifications. The file format contains lines with
 * vendorId, device_id, and type information where type indicates:
 * - 0: Remove device from database
 * - 1: Mark as switch device
 * - 2: Mark as graphics device with additional properties
 *
 * @param fstream Input file stream to the device configuration file
 */
void PciDatabase::parseDeviceConfig(std::ifstream &fstream)
{
	int vendorId = 0, deviceId = 0;
	std::string info;

	while (std::getline(fstream, info)) {
		bool comment = false;
		std::size_t start = 0, pos = 0;
		std::size_t len = info.length();

		if (len == 0) {
			continue;
		}
		while (start < len) {
			if (isBlankSpace(info.at(start))) {
				start++;
			} else if (info.at(start) == '#') {
				comment = true;
				break;
			} else {
				break;
			}
		}
		if (comment) {
			continue;
		}

		start = 0;
		vendorId = std::stoi(info.substr(start), &pos, 16);
		start += pos + 1;
		pos = 0;
		if (start < len) {
			deviceId = std::stoi(info.substr(start), &pos, 16);
			start += pos + 1;
			if (start < len) {
				PcieDevice device = {DV_UNKNOWN, false, vendorId, deviceId, 0, 0, ""};

				if (info.at(start) == '0') {
					auto ret = devices.erase(std::make_pair(vendorId, deviceId));
					DBG("PciDatabase::parse_switch_config()- remove d_id:v_id = [0x%X:0x%X] count:%zu\n", vendorId,
						deviceId, ret);
				} else if (info.at(start) == '1') {
					device.type = DV_SWITCH;
					devices[std::make_pair(vendorId, deviceId)] = device;
				} else if (info.at(start) == '2') {
					start++;
					device.type = DV_GRAPHIC;

					while (start < len) {
						if (isBlankSpace(info.at(start))) {
							start++;
						} else {
							break;
						}
					}

					if (info.at(start) != '0') {
						device.grouped = true;
					}
					start++;
					while (start < len) {
						if (isBlankSpace(info.at(start))) {
							start++;
						} else {
							break;
						}
					}
					if (start < len) {
						device.deviceName = info.substr(start);
						DBG("PciDatabase::parse_switch_config()- deviceName: %s\n", device.deviceName.c_str());
					}
					devices[std::make_pair(vendorId, deviceId)] = device;
				} else {
					ERR("PciDatabase::parse_switch_config() error- unknown value.\n");
				}
			}
		}
	}
}

/**
 * @brief Adds a PCIe switch device to the database
 *
 * This function identifies and adds PCIe switch devices to the database
 * based on device names containing the word "Switch". It can handle both
 * device-level and subsystem-level switch identification.
 *
 * @param vendorId PCI vendor ID
 * @param device_id PCI device ID
 * @param deviceName Device name string to search for switch keyword
 * @param subVendorId Subsystem vendor ID
 * @param subDeviceId Subsystem device ID
 * @param subsystemName Subsystem name string to search for switch keyword
 */
void PciDatabase::addSwitchDevice(int32_t vendorId, int32_t deviceId, const std::string &deviceName,
								  int32_t subVendorId, int32_t subDeviceId, const std::string &subsystemName)
{
	std::string switchString = std::string(" Switch ");
	PcieDevice device = {DV_SWITCH, false, vendorId, deviceId, subVendorId, subDeviceId, ""};

	if (subVendorId >= 0 && subDeviceId >= 0 && !subsystemName.empty()) {
		if (subsystemName.find(switchString) != std::string::npos) {
			devices[std::make_pair(vendorId, deviceId)] = device;
		}
	} else if (vendorId >= 0 && deviceId >= 0 && !deviceName.empty()) {
		if (deviceName.find(switchString) != std::string::npos) {
			devices[std::make_pair(vendorId, deviceId)] = device;
		}
	} else {
		ERR("PciDatabase::addSwitchDevice() error- unknown device %s.\n", device.tostring().c_str());
	}
}

/**
 * @brief Retrieves device information for a given vendor and device ID
 *
 * This function searches the PCI device database for a device matching
 * the specified vendor ID and device ID combination. It provides thread-safe
 * access to the device database using mutex protection.
 *
 * @param vendorId PCI vendor ID to search for
 * @param device_id PCI device ID to search for
 * @return const PcieDevice* Pointer to device information if found, nullptr otherwise
 */
const PcieDevice *PciDatabase::getDevice(int32_t vendorId, int32_t deviceId)
{
	std::unique_lock<std::mutex> lock(mutex);

	device_map::iterator it = devices.find(std::make_pair(vendorId, deviceId));

	if (it != devices.end()) {
		return &it->second;
	}

	return nullptr;
}
