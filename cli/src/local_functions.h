/* 
 *  Copyright (C) 2021-2023 Intel Corporation
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

const int COMPONET_TYE_DRIVER = 1;
const int COMPONET_TYE_GPU = 2;
const int COMPONET_TYE_CPU = 3;

const int ERROR_CATEGORY_KMD = 1;
const int ERROR_CATEGORY_UMD = 2;
const int ERROR_CATEGORY_HARDWARE = 4;

const int ERROR_SEVERITY_LOW = 1;
const int ERROR_SEVERITY_MEDIUM = 2;
const int ERROR_SEVERITY_HIGH = 4;
const int ERROR_SEVERITY_CIRTICAL = 8;

const int processor_count = std::thread::hardware_concurrency();

struct ErrorPattern {
    std::string pattern;
    std::string filter;
    int error_category;
    int error_severity;
    int target_type;
};

struct ComponentInfo {
    int type;
    std::string status;
    int category;
    int severity;
    int id; // cpu physical id
    std::string bdf;
    std::string time;
};

const std::vector<ErrorPattern> error_patterns = {
        {".*(GPU HANG).*", "", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*(GuC initialization failed).*", "", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*ERROR.*GUC.*", "", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*(IO: IOMMU catastrophic error).*", "", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*(LMEM not initialized by firmware).*", "", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
    
        // i915/drm error
        {".*i915.*drm.*ERROR.*", "", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_DRIVER},
        {".*i915.* ERROR .*", "", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_DRIVER},
        {".*drm.*ERROR.*", "i915", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        // cpu error
        {".*(mce|mca).*err.*", "", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_CPU},
        {".*caterr.*", "", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_CPU}
};

const std::string error_types = 
R"(
GuC disabled                        #	Hardware            #	Critical
GuC error                           #	Hardware            #	Critical
GuC initialization failed           #	Hardware            #	Critical
IOMMU catastrophic error            #	Hardware            #	Critical
LMEM not initialized by firmware    #	Hardware            #	Critical
PCIe error                          #	Hardware            #	Critical
drm error                           #	Kernel Mode Driver  #	Critical
GPU HANG                            #	Kernel Mode Driver  #	Critical
i915 error                          #	Kernel Mode Driver  #	Critical
i915 unloaded                       #	Kernel Mode Driver  #	Critical
Level Zero sysman inited error      #	User Mode Driver    #	Critical
HuC disabled                        #	Hardware            #	High
HuC loadable                        #	Hardware            #	High
Level Zero metrics inited error     #	User Mode Driver    #	High
)";

// The order of the vector impacts how error patterns are matched. It starts from special patterns to general patterns.
const std::vector<std::string> targeted_words = {"hang", "guc", "iommu", "lmem", 
                                "i915", "drm", 
                                "mce", "mca", "caterr"};

std::string componentTypeToStr(int component_type);

std::string errorCategoryToStr(int category);

std::string errorSeverityToStr(int severity);

std::string extractLastNChars(std::string const &str, int n);

size_t findCaseInsensitive(std::string data, std::string toSearch, size_t pos = 0);

void updateErrorComponentInfo(ComponentInfo& cinfo, std::string status, int category, int severity, std::string time = "");

std::string zeInitResultToString(const int result);

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

bool getFirmwareVersion(FirmwareVersion& firmware_version, std::string bdf);  
bool getBdfListFromLspci(std::vector<std::string> &list);
bool getPciDeviceData(PciDeviceData &data, const std::string &bdf);
bool getPciPath(std::vector<std::string> &pciPath, const std::string &bdf);

std::unique_ptr<nlohmann::json> getPreCheckInfo(bool onlyGPU, bool rawJson, std::string sinceTime);

bool isATSMPlatform(std::string str);

bool isDriversAutoprobeEnabled(const std::string &bdfAddress);

std::unique_ptr<nlohmann::json> getPreCheckErrorTypes();
std::unique_ptr<nlohmann::json> addKernelParam();

}

