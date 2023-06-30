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
    int target_type;
    int error_id;               // only for gpu and driver, -1 means cpu
    int error_category;
    int error_severity;         
};

struct ComponentInfo {
    int type;
    std::string status;         // Pass or Fail
    int error_category;
    int error_severity;
    int id;                     // only for cpu, cpu physical id
    std::string bdf;            // only for gpu
    std::string time;
    std::string error_detail;
    int error_id;               // only for gpu and driver, -1 means cpu
};

enum error_type {
    GuC_Not_Running = 1,
    GuC_Error = 2,
    GuC_Initialization_Failed = 3,
    IOMMU_Catastrophic_Error = 4,
    LMEM_Not_Initialized_By_Firmware = 5,
    PCIe_Error = 6,
    DRM_Error = 7,
    GPU_Hang = 8,
    i915_Error = 9,
    i915_Not_Loaded = 10,
    Level_Zero_Init_Error = 11,
    HuC_Disabled = 12,
    HuC_Not_Running = 13,
    Level_Zero_Metrics_Init_Error = 14,
};

// error_type_id => {error_type, error_category, error_severity}
const std::map<int, std::tuple<std::string, int, int>> gpu_driver_error_type_lists = {
    {GuC_Not_Running, {"GuC Not Running", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL}},
    {GuC_Error, {"GuC Error", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL}},
    {GuC_Initialization_Failed, {"GuC Initialization Failed", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL}},
    {IOMMU_Catastrophic_Error, {"IOMMU Catastrophic Error", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL}},
    {LMEM_Not_Initialized_By_Firmware, {"LMEM Not Initialized By Firmware", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL}},
    {PCIe_Error, {"PCIe Error", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL}},
    {DRM_Error, {"DRM Error", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL}},
    {GPU_Hang, {"GPU Hang", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL}},
    {i915_Error, {"i915 Error", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL}},
    {i915_Not_Loaded, {"i915 Not Loaded", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL}},
    {Level_Zero_Init_Error, {"Level Zero Init Error", ERROR_CATEGORY_UMD, ERROR_SEVERITY_CIRTICAL}},
    {HuC_Disabled, {"HuC Disabled", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_HIGH}},
    {HuC_Not_Running, {"HuC Not Running", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_HIGH}},
    {Level_Zero_Metrics_Init_Error, {"Level Zero Metrics Init Error", ERROR_CATEGORY_UMD, ERROR_SEVERITY_HIGH}}
};

const std::vector<ErrorPattern> error_patterns = {
        {".*(GPU HANG).*", "", COMPONET_TYE_GPU, GPU_Hang},
        {".*(GuC initialization failed).*", "", COMPONET_TYE_GPU, GuC_Initialization_Failed},
        {".*ERROR.*GUC.*", "", COMPONET_TYE_GPU, GuC_Error},
        {".*(IO: IOMMU catastrophic error).*", "", COMPONET_TYE_GPU, IOMMU_Catastrophic_Error},
        {".*(LMEM not initialized by firmware).*", "", COMPONET_TYE_GPU, LMEM_Not_Initialized_By_Firmware},
    
        // i915/drm error
        {".*i915.*drm.*ERROR.*", "", COMPONET_TYE_DRIVER, i915_Error},
        {".*i915.* ERROR .*", "", COMPONET_TYE_DRIVER, i915_Error},
        {".*drm.*ERROR.*", "i915", COMPONET_TYE_DRIVER, DRM_Error},
        // cpu error
        {".*(mce|mca).*err.*", "", COMPONET_TYE_CPU, -1, ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL},
        {".*caterr.*", "", COMPONET_TYE_CPU, -1, ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL}
};


// The order of the vector impacts how error patterns are matched. It starts from special patterns to general patterns.
const std::vector<std::string> targeted_words = {"hang", "guc", "iommu", "lmem", 
                                "i915", "drm", 
                                "mce", "mca", "caterr"};

std::string componentTypeToStr(int component_type);

std::string errorCategoryToStr(int category);

std::string errorSeverityToStr(int severity);

std::string extractLastNChars(std::string const &str, int n);

size_t findCaseInsensitive(std::string data, std::string toSearch, size_t pos = 0);

void updateErrorComponentInfo(ComponentInfo& cinfo, std::string status, std::string error_detail, int error_id, std::string time = "", int error_category = 0, int error_severity = 0);

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

bool isPhysicalFunctionDevice(std::string pci_addr);

}

