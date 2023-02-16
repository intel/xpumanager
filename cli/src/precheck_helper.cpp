/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file precheck_helper.cpp
 */


#include "precheck_helper.h"
#include <algorithm>

namespace xpum::cli {

std::string componentTypeToStr(int component_type) {
    if (component_type == 0) {
        return "None";
    } else if (component_type == COMPONET_TYE_DRIVER) {
        return "Driver";
    } else if (component_type == COMPONET_TYE_GPU) {
        return "GPU";
    } else if (component_type == COMPONET_TYE_CPU) {
        return "CPU";
    } else {
        return "Unknown";
    }
}

std::string errorCategoryToStr(int category) {
    if (category == 0) {
        return "None";
    } else if (category == ERROR_CATEGORY_KMD) {
        return "Kernel Mode Driver";
    } else if (category == ERROR_CATEGORY_UMD) {
        return "User Mode Driver";
    } else if (category == ERROR_CATEGORY_HARDWARE) {
        return "Hardware";
    } else {
        return "Unknown";
    }
}

std::string errorSeverityToStr(int severity) {
    if (severity == 0) {
        return "None";
    } else if (severity == ERROR_SEVERITY_LOW) {
        return "Low";
    } else if (severity == ERROR_SEVERITY_MEDIUM) {
        return "Medium";
    } else if (severity == ERROR_SEVERITY_HIGH) {
        return "High";
    } else if (severity == ERROR_SEVERITY_CIRTICAL) {
        return "Critical";
    } else {
        return "Unknown";
    }
}

std::string extractLastNChars(std::string const &str, int n) {
    if ((int)str.size() < n) {
        return str;
    }
 
    return str.substr(str.size() - n);
}

size_t findCaseInsensitive(std::string data, std::string toSearch, size_t pos) {
    std::transform(data.begin(), data.end(), data.begin(), ::tolower);
    std::transform(toSearch.begin(), toSearch.end(), toSearch.begin(), ::tolower);
    return data.find(toSearch, pos);
}

void updateErrorComponentInfo(ComponentInfo& cinfo, std::string status, int category, int severity, std::string time) {
    if (cinfo.status == "Pass") {
        cinfo.status = status;
        cinfo.category = category;
        cinfo.severity = severity;
        cinfo.time = time;
    }
}

std::string zeInitResultToString(const int result) {
    if (result == 0) {
        return "ZE_RESULT_SUCCESS";
    } else if (result == 1) {
        return "ZE_RESULT_NOT_READY";
    } else if (result == 2) {
        return "[0x78000001] ZE_RESULT_ERROR_UNINITIALIZED. Please check if you have root privileges.";
    } else if (result == 3) {
        return "[0x70020000] ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE. Maybe the metrics libraries aren't ready.";
    } else {
        return "Generic error with ze_result_t value: " + std::to_string(static_cast<int>(result));
    }
}

} // end namespace xpum::cli