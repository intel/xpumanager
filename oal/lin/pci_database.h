/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file pci_database.h
 */

#pragma once

#include <map>
#include <mutex>
#include <string>

#define PCI_IDS_FILE "pci.ids"
#define PCI_IDS_CONFIG "pci.conf"

/**
 * Class to parse "pci.ids" and "pci.conf" file, save PCIe switch and built-in device info.
 *
 */

enum DeviceType
{
	DV_UNKNOWN = 0,
	DV_SWITCH = 1,
	DV_GRAPHIC = 2
};

struct PcieDevice
{
	DeviceType type;
	bool grouped;
	int32_t vendorId;
	int32_t device_id;
	int32_t subVendorId;
	int32_t subDeviceId;
	std::string deviceName;
	std::string tostring()
	{
		return std::string("verdor_id:") + std::to_string(vendorId) + std::string(" device_id:") +
			   std::to_string(device_id) + std::string(" subVendorId:") + std::to_string(subVendorId) +
			   std::string(" subDeviceId:") + std::to_string(subDeviceId);
	}
};

enum id_type
{
	ID_UNKNOWN,
	ID_VENDOR,
	ID_DEVICE,
	ID_SUB_SYS,
	ID_KNOWN_D_CLASS
};

class PciDatabase
{
public:
	static PciDatabase &instance();

	const PcieDevice *getDevice(int32_t vendorId, int32_t device_id);

private:
	PciDatabase();
	~PciDatabase() {};

	PciDatabase &operator=(const PciDatabase &) = delete;
	PciDatabase(const PciDatabase &) = delete;
	bool init();
	bool parsePciDevice(std::ifstream &fstream);
	bool parseLevel0(const std::string &info, int len, id_type *type, int *vendorId, std::size_t *idx);
	bool parseLevel1(const std::string &info, int len, id_type *type, int *device_id, std::size_t *idx);
	bool parseLevel2(const std::string &info, int len, id_type *type, int *subVendorId, int *subDeviceId,
					 std::size_t *idx);
	void parseDeviceConfig(std::ifstream &fstream);
	static bool isBlankSpace(const char c);
	bool lineIsComment(const std::string &line);
	void addSwitchDevice(int32_t vendorId, int32_t device_id, const std::string &deviceName, int32_t subVendorId,
						 int32_t subDeviceId, const std::string &subsystemName);

	bool bInitialized;
	std::mutex mutex;
	typedef std::pair<int32_t, int32_t> vendorDevice;
	typedef std::map<vendorDevice, PcieDevice> device_map;

	device_map devices;
};
