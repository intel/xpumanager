/* 
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file local_functions.h
 */


#pragma once

#include <string>
#include <thread>
#include <vector>
#include <unordered_set>
#include <nlohmann/json.hpp>

namespace xpum::cli {

#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

struct FirmwareVersion {
    std::string gfx_fw_version;
    std::string gfx_data_fw_version;
}; 

struct PciDeviceData {
    std::string name;
    std::string vendorId;
    std::string pciDeviceId;
};

struct UEvent {
    std::string pciId;
    std::string bdf;
};

bool getFirmwareVersion(FirmwareVersion& firmware_version, std::string bdf);  
bool getBdfListFromLspci(std::vector<std::string> &list);
bool getPciDeviceData(PciDeviceData &data, const std::string &bdf);
bool getPciPath(std::vector<std::string> &pciPath, const std::string &bdf);
bool getUEvent(UEvent &uevent, const char *d_name);

bool isATSMPlatform(std::string str);
bool isATSM3(std::string &pciId);
bool isATSM1(std::string &pciId);
bool isSG1(std::string &pciId);

bool isATSMPlatformFromSysFile();

bool isOamPlatform(std::string str);

bool isDriversAutoprobeEnabled(const std::string &bdfAddress);

std::unique_ptr<nlohmann::json> addKernelParam();

bool isPhysicalFunctionDevice(std::string pci_addr);

bool setSurvivabilityMode(bool enable, std::string &error, bool &modified);

bool recoverable();
}

