/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file precheck_helper.h
 */

#ifndef _XPUM_PRECHECK_HELPER_H_
#define _XPUM_PRECHECK_HELPER_H_

#include <string>
#include <thread>
#include <vector>
#include <unordered_set>

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
        // i915 error
        {".*ERROR.*i915.*", "", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_DRIVER},
        {".*i915.*drm.*ERROR.*", "", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_DRIVER},
        // gpu error
        {".*ERROR.*drm.*", "i915", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*ERROR.*GUC.*", "", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*(GuC initialization failed).*", "", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*(LMEM not initialized by firmware).*", "", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*(task).*(blocked for more than).*(seconds).*", "", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*([Hardware Error]).*(Hardware error from APEI Generic Hardware Error Source).*", "", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*(kmemleak).*(new suspected memory leaks).*", "", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*(perf: interrupt took too long).*(lowering kernel.perf_event_max_sample_rate).*", "", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*(Out of memory).*", "", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*(GPU HANG).*", "", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*(segfault).*(lib).*", "", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*(Kernel panic).*", "", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*(gdm-x-session).*(Server terminated with error).*", "", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*(traps:).*", "", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*(IO: IOMMU catastrophic error).*", "", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        {".*(PCIe error).*", "", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_GPU},
        // cpu error
        {".*(mce|mca).*err.*", "", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_CPU},
        {".*caterr.*", "", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL, COMPONET_TYE_CPU}
        };

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

std::string to_hex_string(uint64_t val, int width = 0);

std::string add_two_hex_string(std::string str1, std::string str2);

std::string to_firmware_hex_version(std::string hex_str);

uint32_t access_device_memory(std::string hex_base, std::string hex_val = "");

bool getFirmwareVersion(FirmwareVersion& firmware_version, std::string bdf);   

} // end namespace xpum::cli

#endif