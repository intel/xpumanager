/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file diagnostic_manager.cpp
 */

#include "diagnostic_manager.h"

#include <algorithm>
#include <thread>

#include "device/gpu/gpu_device_stub.h"
#include "infrastructure/configuration.h"
#include "infrastructure/handle_lock.h"
#include "infrastructure/logger.h"
#include "infrastructure/xpum_config.h"
#include "infrastructure/utility.h"
#include <sys/stat.h>

namespace xpum {

DiagnosticManager::DiagnosticManager(std::shared_ptr<DeviceManagerInterface> &p_device_manager,
                                     std::shared_ptr<DataLogicInterface> &p_data_logic)
    : p_device_manager(p_device_manager), p_data_logic(p_data_logic) {
    XPUM_LOG_TRACE("DiagnosticManager()");
}

DiagnosticManager::~DiagnosticManager() {
    XPUM_LOG_TRACE("~DiagnosticManager()");
}

void DiagnosticManager::init() {
}

void DiagnosticManager::close() {
}

std::map<std::string, std::map<std::string, int>> DiagnosticManager::thresholds;
std::map<ze_device_handle_t, std::string> DiagnosticManager::device_names;
std::string DiagnosticManager::MEDIA_CODER_TOOLS_PATH = "/usr/share/mfx/samples/";
std::string DiagnosticManager::MEDIA_CODER_TOOLS_1080P_FILE = "test_stream_1080p.265";
std::string DiagnosticManager::MEDIA_CODER_TOOLS_4K_FILE = "test_stream_4K.265";
std::string DiagnosticManager::MEDIA_CODEC_TOOLS_LIGHT_FILE = "test_stream.264";
int DiagnosticManager::ZE_COMMAND_QUEUE_SYNCHRONIZE_TIMEOUT = 600;
float DiagnosticManager::MEMORY_USE_PERCENTAGE_FOR_ERROR_TEST = 0.9;
const std::string DiagnosticManager::COMPONENT_TYPE_NOT_SUPPORTED = "Not supported";

static bool is_path_exist(const std::string &s) {
  struct stat buffer;
  return (stat(s.c_str(), &buffer) == 0);
}

void DiagnosticManager::readConfigFile() {
    thresholds.clear();
    std::string file_name = std::string(XPUM_CONFIG_DIR) + std::string("diagnostics.conf");
    if (!is_path_exist(file_name)) {
        char exe_path[XPUM_MAX_PATH_LEN];
        ssize_t len = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path));
        exe_path[len] = '\0';
        std::string current_file = exe_path;
        file_name = current_file.substr(0, current_file.find_last_of('/')) + "/../lib/" + Configuration::getXPUMMode() + "/config/" + std::string("diagnostics.conf");
        if (!is_path_exist(file_name))
            file_name = current_file.substr(0, current_file.find_last_of('/')) + "/../lib64/" + Configuration::getXPUMMode() + "/config/" + std::string("diagnostics.conf");
    }
    std::ifstream conf_file(file_name);
    if (conf_file.is_open()) {
        std::string line;
        std::string current_device;
        while (getline(conf_file, line)) {
            line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
            if (line[0] == '#' || line.empty())
                continue;
            auto delimiter_pos = line.find("=");
            auto name = line.substr(0, delimiter_pos);
            auto value = line.substr(delimiter_pos + 1);
            if (value.find("#") != std::string::npos)
                value = value.substr(0, value.find("#"));
            if (name == "MEDIA_CODER_TOOLS_PATH") {
                if (value == "/usr/bin/" || value == "/usr/share/mfx/samples/") {
                    MEDIA_CODER_TOOLS_PATH = value;
                }
            } else if (name == "MEDIA_CODER_TOOLS_1080P_FILE") {
                MEDIA_CODER_TOOLS_1080P_FILE = value;
            } else if (name == "MEDIA_CODER_TOOLS_4K_FILE") {
                MEDIA_CODER_TOOLS_4K_FILE = value;
            } else if (name == "ZE_COMMAND_QUEUE_SYNCHRONIZE_TIMEOUT") {
                try {
                    int val = std::stoi(value);
                    if (val > 0)
                        ZE_COMMAND_QUEUE_SYNCHRONIZE_TIMEOUT = val;
                } catch(...) { }
            } else if (name == "MEMORY_USE_PERCENTAGE_FOR_ERROR_TEST") {
                try {
                    float val = std::stof(value);
                    if (val > 0 && val < 1)
                        MEMORY_USE_PERCENTAGE_FOR_ERROR_TEST = val;
                } catch(...) { }
            } else if (name == "NAME") {
                current_device = value;
            } else {
                thresholds[current_device][name] = atoi(value.c_str());
            }
        }
        conf_file.close();
    } else {
        XPUM_LOG_ERROR("couldn't open config file for diagnostics: {}", file_name);
    }
}

xpum_result_t DiagnosticManager::runDiagnosticsCore(xpum_device_id_t deviceId, xpum_diag_level_t level, xpum_diag_task_type_t types[], int count) {
    std::unique_lock<std::mutex> lock(this->mutex);
    if (diagnostic_task_infos.find(deviceId) != diagnostic_task_infos.end() && diagnostic_task_infos.at(deviceId)->finished == false) {
        return XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE;
    }

    diagnostic_task_infos.erase(deviceId);

    std::shared_ptr<xpum_diag_task_info_t> p_task_info = std::make_shared<xpum_diag_task_info_t>();
    p_task_info->deviceId = deviceId;
    p_task_info->level = level;
    if (level != XPUM_DIAG_LEVEL_MAX) {
        p_task_info->targetTypeCount = 0;
        for(auto& item : p_task_info->targetTypes)
            item = XPUM_DIAG_TASK_TYPE_MAX;
    } else {
        p_task_info->targetTypeCount = count;
        for(auto& item : p_task_info->targetTypes)
            item = XPUM_DIAG_TASK_TYPE_MAX;
        for (int i = 0; i < count; i++)
            p_task_info->targetTypes[i] = types[i];
    }
    p_task_info->result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;
    p_task_info->finished = false;
    p_task_info->count = 0;
    p_task_info->startTime = Utility::getCurrentMillisecond();

    updateMessage(p_task_info->message, std::string("Doing diagnostics"));
    for (int index = xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_ENV_VARIABLES; index < xpum_diag_task_type_t::XPUM_DIAG_TASK_TYPE_MAX; index++) {
        xpum_diag_component_info_t &component = p_task_info->componentList[index];
        component.type = static_cast<xpum_diag_task_type_t>(index);
        component.finished = false;
        component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;
    }

    diagnostic_task_infos.insert(std::pair<xpum_device_id_t, std::shared_ptr<xpum_diag_task_info_t>>(deviceId, p_task_info));

    std::vector<std::shared_ptr<Device>> devices;
    this->p_device_manager->getDeviceList(devices);
    for (auto device : devices) {
        Property property;
        if (device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_DEVICE_ID, property)) {
            std::string value = property.getValue();
            std::string device_pci_id = value;
            if (value.substr(0, 2) == "0x")
                device_pci_id = value.substr(2);

            if (device_pci_id.size() < 4)
                device_pci_id = std::string(4 - device_pci_id.size(), '0') + device_pci_id;

            // Format for device name is "Intel(R)Graphics[0xXXXX]" which is consistent with diagnostics.conf after removing spaces. 
            device_names.insert(std::pair<ze_device_handle_t, std::string>(device->getDeviceHandle(), "Intel(R)Graphics[0x" + device_pci_id + "]"));
        }
    }
    try {
        readConfigFile();
    } catch (BaseException &e) {
        XPUM_LOG_DEBUG("fail to read diagnostics.conf");
    }
    int gpu_total_count = devices.size();
    std::thread task(level != XPUM_DIAG_LEVEL_MAX ? DiagnosticManager::doDeviceLevelDiagnosticCore : DiagnosticManager::doDeviceMultipleSpecificDiagnosticCore,
                     this->p_device_manager->getDevice(std::to_string(deviceId))->getDeviceZeHandle(),
                     this->p_device_manager->getDevice(std::to_string(deviceId))->getDriverHandle(),
                     p_task_info, gpu_total_count, std::ref(this->media_codec_perf_datas));
    task.detach();
    return XPUM_OK;
}

xpum_result_t DiagnosticManager::runLevelDiagnostics(xpum_device_id_t deviceId, xpum_diag_level_t level) {
    if (this->p_device_manager->getDevice(std::to_string(deviceId)) == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    if (level < xpum_diag_level_t::XPUM_DIAG_LEVEL_1 || level > xpum_diag_level_t::XPUM_DIAG_LEVEL_3) {
        return XPUM_RESULT_DIAGNOSTIC_INVALID_LEVEL;
    }
    return runDiagnosticsCore(deviceId, level, nullptr, 0);
}

xpum_result_t DiagnosticManager::runMultipleSpecificDiagnostics(xpum_device_id_t deviceId, xpum_diag_task_type_t types[], int count) {
    if (this->p_device_manager->getDevice(std::to_string(deviceId)) == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    if (count <= 0 || count >= xpum_diag_task_type_t::XPUM_DIAG_TASK_TYPE_MAX) {
        return XPUM_RESULT_DIAGNOSTIC_INVALID_TASK_TYPE;
    }
    for (int i = 0; i < count; i++)
        if (types[i] < xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_ENV_VARIABLES || types[i] >= xpum_diag_task_type_t::XPUM_DIAG_TASK_TYPE_MAX) {
            return XPUM_RESULT_DIAGNOSTIC_INVALID_TASK_TYPE;
        }

    return runDiagnosticsCore(deviceId, xpum_diag_level_t::XPUM_DIAG_LEVEL_MAX, types, count);
}

bool DiagnosticManager::isDiagnosticsRunning(xpum_device_id_t deviceId) {
    if (this->p_device_manager->getDevice(std::to_string(deviceId)) == nullptr) {
        return false;
    }

    std::unique_lock<std::mutex> lock(this->mutex);
    if (diagnostic_task_infos.find(deviceId) != diagnostic_task_infos.end() && diagnostic_task_infos.at(deviceId)->finished == false) {
        return true;
    }

    return false;
}

xpum_result_t DiagnosticManager::getDiagnosticsResult(xpum_device_id_t deviceId, xpum_diag_task_info_t *result) {
    if (this->p_device_manager->getDevice(std::to_string(deviceId)) == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    std::unique_lock<std::mutex> lock(this->mutex);
    if (diagnostic_task_infos.find(deviceId) == diagnostic_task_infos.end()) {
        return XPUM_RESULT_DIAGNOSTIC_TASK_NOT_FOUND;
    }

    result->deviceId = deviceId;
    result->level = diagnostic_task_infos.at(deviceId)->level;
    result->targetTypeCount = diagnostic_task_infos.at(deviceId)->targetTypeCount;
    result->finished = diagnostic_task_infos.at(deviceId)->finished;
    result->count = diagnostic_task_infos.at(deviceId)->count;
    result->startTime = diagnostic_task_infos.at(deviceId)->startTime;
    result->endTime = diagnostic_task_infos.at(deviceId)->endTime;
    result->result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;
    updateMessage(result->message, std::string(diagnostic_task_infos.at(deviceId)->message));

    if (result->level == XPUM_DIAG_LEVEL_MAX) {
        for (int i = 0; i < result->targetTypeCount; i++) {
            xpum_diag_component_info_t &component = result->componentList[i];
            component.type = diagnostic_task_infos.at(deviceId)->targetTypes[i];
            component.finished = diagnostic_task_infos.at(deviceId)->componentList[component.type].finished;
            component.result = diagnostic_task_infos.at(deviceId)->componentList[component.type].result;
            if (diagnostic_task_infos.at(deviceId)->componentList[component.type].result == xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL 
                    && component.type != XPUM_DIAG_HARDWARE_SYSMAN) {
                result->result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
            }
            updateMessage(component.message, std::string(diagnostic_task_infos.at(deviceId)->componentList[component.type].message));
        }
    } else {
        int pos = 0;
        for (int index = xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_ENV_VARIABLES; index < xpum_diag_task_type_t::XPUM_DIAG_TASK_TYPE_MAX; index++) {
            if (strncmp(diagnostic_task_infos.at(deviceId)->componentList[index].message, COMPONENT_TYPE_NOT_SUPPORTED.c_str(), COMPONENT_TYPE_NOT_SUPPORTED.size()) == 0) {
                result->count -= 1;
                continue;
            }
            xpum_diag_component_info_t &component = result->componentList[pos];
            component.type = diagnostic_task_infos.at(deviceId)->componentList[index].type;
            component.finished = diagnostic_task_infos.at(deviceId)->componentList[index].finished;
            component.result = diagnostic_task_infos.at(deviceId)->componentList[index].result;
            if (diagnostic_task_infos.at(deviceId)->componentList[index].result == xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL 
                    && index != XPUM_DIAG_HARDWARE_SYSMAN) {
                result->result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
            }
            updateMessage(component.message, std::string(diagnostic_task_infos.at(deviceId)->componentList[index].message));
            pos += 1;
        }
    }
    if (result->finished && result->result == xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN) {
        result->result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
    }
    return XPUM_OK;
}

xpum_result_t DiagnosticManager::getDiagnosticsMediaCodecResult(xpum_device_id_t deviceId, xpum_diag_media_codec_metrics_t resultList[], int *count) {
    if (this->p_device_manager->getDevice(std::to_string(deviceId)) == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    std::unique_lock<std::mutex> lock(this->mutex);
    if (media_codec_perf_datas.find(deviceId) == media_codec_perf_datas.end()) {
        return XPUM_RESULT_DIAGNOSTIC_TASK_NOT_FOUND;
    }

    if (resultList == NULL) {
        *count = media_codec_perf_datas[deviceId].size();
        return XPUM_OK;
    }

    if ((*count) < (int)media_codec_perf_datas[deviceId].size()) {
        *count = media_codec_perf_datas[deviceId].size();
        return XPUM_BUFFER_TOO_SMALL;
    }

    for (int i = 0; i < (int)media_codec_perf_datas[deviceId].size(); i++) {
        resultList[i].deviceId = deviceId;
        resultList[i].format = media_codec_perf_datas[deviceId][i].format;
        resultList[i].resolution = media_codec_perf_datas[deviceId][i].resolution;
        std::strcpy(resultList[i].fps, media_codec_perf_datas[deviceId][i].fps);
    }
    *count = media_codec_perf_datas[deviceId].size();
    return XPUM_OK;
}

void DiagnosticManager::doDeviceDiagnosticExceptionHandle(xpum_diag_task_type_t type, std::string error, std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    xpum_diag_component_info_t &component = p_task_info->componentList[type];
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
    std::string type_str;
    switch (type) {
        case XPUM_DIAG_SOFTWARE_ENV_VARIABLES:
            type_str = "XPUM_DIAG_SOFTWARE_ENV_VARIABLES";
            break;
        case XPUM_DIAG_SOFTWARE_LIBRARY:
            type_str = "XPUM_DIAG_SOFTWARE_LIBRARY";
            break;
        case XPUM_DIAG_SOFTWARE_PERMISSION:
            type_str = "XPUM_DIAG_SOFTWARE_PERMISSION";
            break;
        case XPUM_DIAG_SOFTWARE_EXCLUSIVE:
            type_str = "XPUM_DIAG_SOFTWARE_EXCLUSIVE";
            break;
        case XPUM_DIAG_COMPUTATION:
            type_str = "XPUM_DIAG_COMPUTATION";
            break;
        case XPUM_DIAG_LIGHT_CODEC:
            type_str = "XPUM_DIAG_LIGHT_CODEC";
            break;
        case XPUM_DIAG_HARDWARE_SYSMAN:
            type_str = "XPUM_DIAG_HARDWARE_SYSMAN";
            break;
        case XPUM_DIAG_INTEGRATION_PCIE:
            type_str = "XPUM_DIAG_INTEGRATION_PCIE";
            break;
        case XPUM_DIAG_MEDIA_CODEC:
            type_str = "XPUM_DIAG_MEDIA_CODEC";
            break;
        case XPUM_DIAG_PERFORMANCE_COMPUTATION:
            type_str = "XPUM_DIAG_PERFORMANCE_COMPUTATION";
            break;
        case XPUM_DIAG_PERFORMANCE_POWER:
            type_str = "XPUM_DIAG_PERFORMANCE_POWER";
            break;
        case XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH:
            type_str = "XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH";
            break;
        case XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION:
            type_str = "XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION";
            break;
        case XPUM_DIAG_MEMORY_ERROR:
            type_str = "XPUM_DIAG_MEMORY_ERROR";
            break;
        default:
            break;
    }
    std::string desc = "Error in " + error;
    XPUM_LOG_ERROR("Error in diagnostics {} : {}", type_str, error);
    updateMessage(component.message, desc);
    component.finished = true;
}

void DiagnosticManager::doDeviceLevelDiagnosticCore(const ze_device_handle_t &ze_device, const ze_driver_handle_t &ze_driver,
                                               std::shared_ptr<xpum_diag_task_info_t> p_task_info, int gpu_total_count,
                                               std::map<xpum_device_id_t, std::vector<xpum_diag_media_codec_metrics_t>>& media_codec_perf_datas) {
    bool find_error = false;
    std::string error_details;
    try {
        zes_device_handle_t zes_device = (zes_device_handle_t)ze_device;
        if (p_task_info->level >= XPUM_DIAG_LEVEL_1) {
            XPUM_LOG_INFO("start environment variables diagnostic");
            try {
                doDeviceDiagnosticEnvironmentVariables(p_task_info);
            } catch (BaseException &e) {
                doDeviceDiagnosticExceptionHandle(XPUM_DIAG_SOFTWARE_ENV_VARIABLES, e.what(), p_task_info);
            }
            
            XPUM_LOG_INFO("start libraries diagnostic");
            try {
                doDeviceDiagnosticLibraries(p_task_info);
            } catch (BaseException &e) {
                doDeviceDiagnosticExceptionHandle(XPUM_DIAG_SOFTWARE_LIBRARY, e.what(), p_task_info);
            }

            XPUM_LOG_INFO("start permission diagnostic");
            try {
                doDeviceDiagnosticPermission(gpu_total_count, p_task_info);
            } catch (BaseException &e) {
                doDeviceDiagnosticExceptionHandle(XPUM_DIAG_SOFTWARE_PERMISSION, e.what(), p_task_info);
            }

            XPUM_LOG_INFO("start exclusive diagnostic");
            try {
                doDeviceDiagnosticExclusive(zes_device, p_task_info);
            } catch (BaseException &e) {
                doDeviceDiagnosticExceptionHandle(XPUM_DIAG_SOFTWARE_EXCLUSIVE, e.what(), p_task_info);
            }

            XPUM_LOG_INFO("start computation check diagnostic");
            try {
                doDeviceDiagnosticPeformanceComputation(ze_device, ze_driver, p_task_info, true);
            } catch (BaseException &e) {
                doDeviceDiagnosticExceptionHandle(XPUM_DIAG_COMPUTATION, e.what(), p_task_info);
            }
        }

        /*
        if (p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_EXCLUSIVE].result == xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL) {
            p_task_info->finished = true;
            p_task_info->endTime = Utility::getCurrentMillisecond();
            updateMessage(p_task_info->message, std::string("Aborted! Other GPU processes are running"));
            XPUM_LOG_ERROR("aborted! other GPU processes are running");
            return;
        }
        */

        if (p_task_info->level >= XPUM_DIAG_LEVEL_2) {
            XPUM_LOG_INFO("start hardware sysmam diagnostic");
            try {
                doDeviceDiagnosticHardwareSysman(zes_device, p_task_info);
            } catch (BaseException &e) {
                doDeviceDiagnosticExceptionHandle(XPUM_DIAG_HARDWARE_SYSMAN, e.what(), p_task_info);
            }
        
            XPUM_LOG_INFO("start integration diagnostic");
            try {
                doDeviceDiagnosticIntegration(ze_device, ze_driver, p_task_info);
            } catch (BaseException &e) {
                doDeviceDiagnosticExceptionHandle(XPUM_DIAG_INTEGRATION_PCIE, e.what(), p_task_info);
            }

            XPUM_LOG_INFO("start mediacodec diagnostic");
            try {
                doDeviceDiagnosticMediaCodec(zes_device, p_task_info, media_codec_perf_datas, false);
            } catch (BaseException &e) {
                doDeviceDiagnosticExceptionHandle(XPUM_DIAG_MEDIA_CODEC, e.what(), p_task_info);
            }
        }

        if (p_task_info->level >= XPUM_DIAG_LEVEL_3) {
            XPUM_LOG_INFO("start computation diagnostic");
            try {
                doDeviceDiagnosticPeformanceComputation(ze_device, ze_driver, p_task_info, false);
            } catch (BaseException &e) {
                doDeviceDiagnosticExceptionHandle(XPUM_DIAG_PERFORMANCE_COMPUTATION, e.what(), p_task_info);
            }

            XPUM_LOG_INFO("start power diagnostic");
            try {
                doDeviceDiagnosticPeformancePower(ze_device, ze_driver, p_task_info);
            } catch (BaseException &e) {
                doDeviceDiagnosticExceptionHandle(XPUM_DIAG_PERFORMANCE_POWER, e.what(), p_task_info);
            }

            XPUM_LOG_INFO("start memory bandwidth diagnostic");
            try {
                doDeviceDiagnosticPeformanceMemoryBandwidth(ze_device, ze_driver, p_task_info);
            } catch (BaseException &e) {
                doDeviceDiagnosticExceptionHandle(XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH, e.what(), p_task_info);
            }

            XPUM_LOG_INFO("start memory allocation diagnostic ");
            try {
                doDeviceDiagnosticPeformanceMemoryAllocation(ze_device, ze_driver, p_task_info);
            } catch (BaseException &e) {
                doDeviceDiagnosticExceptionHandle(XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION, e.what(), p_task_info);
            }

            XPUM_LOG_INFO("start memory error diagnostic ");
            try {
                doDeviceDiagnosticMemoryError(ze_device, ze_driver, p_task_info);
            } catch (BaseException &e) {
                doDeviceDiagnosticExceptionHandle(XPUM_DIAG_MEMORY_ERROR, e.what(), p_task_info);
            }
        }
    } catch (std::exception &e) {
        find_error = true;
        error_details = "Aborted! " + std::string(e.what());
    }

    p_task_info->endTime = Utility::getCurrentMillisecond();
    p_task_info->finished = true;
    if (!find_error) {
        updateMessage(p_task_info->message, std::string("All diagnostics done"));
    } else {
        for (int index = xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_ENV_VARIABLES; index < xpum_diag_task_type_t::XPUM_DIAG_TASK_TYPE_MAX; index++) {
            xpum_diag_component_info_t &component = p_task_info->componentList[index];
            if (component.finished == false) {
                updateMessage(component.message, std::string(""));
            }
        }
        updateMessage(p_task_info->message, error_details);
    }
    XPUM_LOG_INFO("all diagnostics done");
}

void DiagnosticManager::doDeviceMultipleSpecificDiagnosticCore(const ze_device_handle_t &ze_device, const ze_driver_handle_t &ze_driver,
                                               std::shared_ptr<xpum_diag_task_info_t> p_task_info, int gpu_total_count,
                                               std::map<xpum_device_id_t, std::vector<xpum_diag_media_codec_metrics_t>>& media_codec_perf_datas) {
    bool find_error = false;
    std::string error_details;

    try {
        zes_device_handle_t zes_device = (zes_device_handle_t)ze_device;
        for (int i = 0; i < p_task_info->targetTypeCount; i++) {
            switch (p_task_info->targetTypes[i])
            {
            case XPUM_DIAG_SOFTWARE_ENV_VARIABLES:
                XPUM_LOG_INFO("start environment variables diagnostic");
                try {
                    doDeviceDiagnosticEnvironmentVariables(p_task_info);
                } catch (BaseException &e) {
                    doDeviceDiagnosticExceptionHandle(XPUM_DIAG_SOFTWARE_ENV_VARIABLES, e.what(), p_task_info);
                }
                break;
            case XPUM_DIAG_SOFTWARE_LIBRARY:
                XPUM_LOG_INFO("start libraries diagnostic");
                try {
                    doDeviceDiagnosticLibraries(p_task_info);
                } catch (BaseException &e) {
                    doDeviceDiagnosticExceptionHandle(XPUM_DIAG_SOFTWARE_LIBRARY, e.what(), p_task_info);
                }
                break;
            case XPUM_DIAG_SOFTWARE_PERMISSION:
                XPUM_LOG_INFO("start permission diagnostic");
                try {
                    doDeviceDiagnosticPermission(gpu_total_count, p_task_info);
                } catch (BaseException &e) {
                    doDeviceDiagnosticExceptionHandle(XPUM_DIAG_SOFTWARE_PERMISSION, e.what(), p_task_info);
                }
                break;
            case XPUM_DIAG_SOFTWARE_EXCLUSIVE:
                XPUM_LOG_INFO("start exclusive diagnostic");
                try {
                    doDeviceDiagnosticExclusive(zes_device, p_task_info);
                } catch (BaseException &e) {
                    doDeviceDiagnosticExceptionHandle(XPUM_DIAG_SOFTWARE_EXCLUSIVE, e.what(), p_task_info);
                }
                break;
            case XPUM_DIAG_COMPUTATION:
                XPUM_LOG_INFO("start computation check diagnostic");
                try {
                    doDeviceDiagnosticPeformanceComputation(ze_device, ze_driver, p_task_info, true);
                } catch (BaseException &e) {
                    doDeviceDiagnosticExceptionHandle(XPUM_DIAG_COMPUTATION, e.what(), p_task_info);
                }
                break;
            case XPUM_DIAG_LIGHT_CODEC:
                XPUM_LOG_INFO("start media codec check diagnostic");
                try {
                    doDeviceDiagnosticMediaCodec(zes_device, p_task_info, media_codec_perf_datas, true);
                } catch (BaseException &e) {
                    doDeviceDiagnosticExceptionHandle(XPUM_DIAG_LIGHT_CODEC, e.what(), p_task_info);
                }
                break;
            case XPUM_DIAG_HARDWARE_SYSMAN:
                XPUM_LOG_INFO("start hardware sysmam diagnostic");
                try {
                    doDeviceDiagnosticHardwareSysman(zes_device, p_task_info);
                } catch (BaseException &e) {
                    doDeviceDiagnosticExceptionHandle(XPUM_DIAG_HARDWARE_SYSMAN, e.what(), p_task_info);
                }
                break;
            case XPUM_DIAG_INTEGRATION_PCIE:
                XPUM_LOG_INFO("start integration diagnostic");
                try {
                    doDeviceDiagnosticIntegration(ze_device, ze_driver, p_task_info);
                } catch (BaseException &e) {
                    doDeviceDiagnosticExceptionHandle(XPUM_DIAG_INTEGRATION_PCIE, e.what(), p_task_info);
                }
                break;
            case XPUM_DIAG_MEDIA_CODEC:
                XPUM_LOG_INFO("start mediacodec diagnostic");
                try {
                    doDeviceDiagnosticMediaCodec(zes_device, p_task_info, media_codec_perf_datas, false);
                } catch (BaseException &e) {
                    doDeviceDiagnosticExceptionHandle(XPUM_DIAG_MEDIA_CODEC, e.what(), p_task_info);
                }
                break;
            case XPUM_DIAG_PERFORMANCE_COMPUTATION:
                XPUM_LOG_INFO("start computation diagnostic");
                try {
                    doDeviceDiagnosticPeformanceComputation(ze_device, ze_driver, p_task_info, false);
                } catch (BaseException &e) {
                    doDeviceDiagnosticExceptionHandle(XPUM_DIAG_PERFORMANCE_COMPUTATION, e.what(), p_task_info);
                }
                break;
            case XPUM_DIAG_PERFORMANCE_POWER:
                XPUM_LOG_INFO("start power diagnostic");
                try {
                    doDeviceDiagnosticPeformancePower(ze_device, ze_driver, p_task_info);
                } catch (BaseException &e) {
                    doDeviceDiagnosticExceptionHandle(XPUM_DIAG_PERFORMANCE_POWER, e.what(), p_task_info);
                }
                break;
            case XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH:
                XPUM_LOG_INFO("start memory bandwidth diagnostic");
                try {
                    doDeviceDiagnosticPeformanceMemoryBandwidth(ze_device, ze_driver, p_task_info);
                } catch (BaseException &e) {
                    doDeviceDiagnosticExceptionHandle(XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH, e.what(), p_task_info);
                }
                break;
            case XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION:
                XPUM_LOG_INFO("start memory allocation diagnostic ");
                try {
                    doDeviceDiagnosticPeformanceMemoryAllocation(ze_device, ze_driver, p_task_info);
                } catch (BaseException &e) {
                    doDeviceDiagnosticExceptionHandle(XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION, e.what(), p_task_info);
                }
                break;
            case XPUM_DIAG_MEMORY_ERROR:
                XPUM_LOG_INFO("start memory error diagnostic ");
                try {
                    doDeviceDiagnosticMemoryError(ze_device, ze_driver, p_task_info);
                } catch (BaseException &e) {
                    doDeviceDiagnosticExceptionHandle(XPUM_DIAG_MEMORY_ERROR, e.what(), p_task_info);
                }
                break;
            default:
                break;
            }
        }
    } catch (std::exception &e) {
        find_error = true;
        error_details = "Aborted! " + std::string(e.what());
    }

    p_task_info->endTime = Utility::getCurrentMillisecond();
    p_task_info->finished = true;
    if (!find_error) {
        updateMessage(p_task_info->message, std::string("specific diagnostics done"));
    } else {
        for (int index = xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_ENV_VARIABLES; index < xpum_diag_task_type_t::XPUM_DIAG_TASK_TYPE_MAX; index++) {
            xpum_diag_component_info_t &component = p_task_info->componentList[index];
            if (component.finished == false) {
                updateMessage(component.message, std::string(""));
            }
        }
        updateMessage(p_task_info->message, error_details);
    }
    XPUM_LOG_INFO("specific diagnostic done");
}

void DiagnosticManager::doDeviceDiagnosticEnvironmentVariables(std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    std::string details;
    // DIAGNOSTIC_SOFTWARE_ENV
    xpum_diag_component_info_t &component1 = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_ENV_VARIABLES];
    p_task_info->count += 1;
    updateMessage(component1.message, std::string("Running"));
    std::vector<std::string> check_env_varibles;
    check_env_varibles.push_back(std::string("ZES_ENABLE_SYSMAN"));
    if (std::any_of(Configuration::getEnabledMetrics().begin(), Configuration::getEnabledMetrics().end(),
                    [](const MeasurementType type) { return type == METRIC_EU_ACTIVE || type == METRIC_EU_IDLE || type == METRIC_EU_STALL; })) {
        check_env_varibles.push_back(std::string("ZET_ENABLE_METRICS"));
    }

    bool find_env_varibles = true;
    for (auto it = check_env_varibles.begin(); it != check_env_varibles.end(); it++) {
        std::string check_env_var = *it;
        if (std::getenv(check_env_var.c_str()) == nullptr) {
            find_env_varibles = false;
            details = check_env_var;
            break;
        }
    }

    if (find_env_varibles) {
        component1.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
        updateMessage(component1.message, std::string("Pass to check environment variables."));
    } else {
        component1.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        std::string desc = "Fail to check environment variables. " + details + " is missing.";
        updateMessage(component1.message, desc);
    }
    component1.finished = true;
}

void DiagnosticManager::doDeviceDiagnosticLibraries(std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    std::string details;
    // DIAGNOSTIC_SOFTWARE_LIBRARY
    xpum_diag_component_info_t &component2 = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_LIBRARY];
    p_task_info->count += 1;
    updateMessage(component2.message, std::string("Running"));
    std::vector<std::string> libs;
    libs.push_back("libze_loader.so.1");
    libs.push_back("libze_intel_gpu.so.1");

    bool find_libs = true;
    for (auto it = libs.begin(); it != libs.end(); it++) {
        if (!find_libs) {
            break;
        }
        void *handle;
        handle = dlopen((*it).c_str(), RTLD_NOW);
        if (!handle) {
            find_libs = false;
            details = (*it);
            break;
        }
    }

    if (find_libs) {
        component2.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
        updateMessage(component2.message, std::string("Pass to check libraries."));
    } else {
        component2.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        std::string desc = "Fail to check libraries. " + details + " is missing.";
        updateMessage(component2.message, desc);
    }
    component2.finished = true;
}

void DiagnosticManager::doDeviceDiagnosticPermission(int gpu_total_count, std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    std::string details;
    xpum_diag_component_info_t &component3 = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_PERMISSION];
    p_task_info->count += 1;
    updateMessage(component3.message, std::string("Running"));
    int device_count = 0;
    DIR *dir;
    struct dirent *ent;
    std::string dir_name = "/dev/dri";
    dir = opendir(dir_name.c_str());
    if (nullptr != dir) {
        bool has_permission = true;
        ent = readdir(dir);
        while (nullptr != ent) {
            std::string entry_name = ent->d_name;
            if (countDevEntry(entry_name)) {
                device_count++;
                std::stringstream ss;
                ss << dir_name << "/" << entry_name;
                int ret = access(ss.str().c_str(), 4);
                if (ret != 0) {
                    has_permission = false;
                    details = ss.str();
                    break;
                }
            }
            ent = readdir(dir);
        }
        closedir(dir);

        if (has_permission && device_count == gpu_total_count) {
            component3.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
            updateMessage(component3.message, std::string("Pass to check permission."));
        } else if (device_count != gpu_total_count) {
            component3.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
            updateMessage(component3.message, std::string("Fail to check device count."));
        } else if (!has_permission) {
            component3.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
            std::string desc = "Fail to check permission. " + details + " is failed.";
            updateMessage(component3.message, desc);
        }
    } else {
        component3.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        updateMessage(component3.message, std::string("Fail to check permission."));
    }
    component3.finished = true;
}

void DiagnosticManager::doDeviceDiagnosticExclusive(const zes_device_handle_t &device, std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    std::string details;
    // DIAGNOSTIC_SOFTWARE_EXCLUSIVE
    xpum_diag_component_info_t &component4 = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_EXCLUSIVE];
    p_task_info->count += 1;
    updateMessage(component4.message, std::string("Running"));
    uint32_t process_count = 0;
    ze_result_t ret;
    XPUM_ZE_HANDLE_LOCK(device, ret = zesDeviceProcessesGetState(device, &process_count, nullptr));
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zesDeviceProcessesGetState()");
    }
    std::vector<zes_process_state_t> processes(process_count);
    XPUM_ZE_HANDLE_LOCK(device, ret = zesDeviceProcessesGetState(device, &process_count, processes.data()));
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zesDeviceProcessesGetState()");
    }

    for (auto process : processes) {
        std::ifstream file("/proc/" + std::to_string(process.processId) + "/cmdline");
        if (!file.good()) {
            process_count -= 1;
            XPUM_LOG_DEBUG("process pid : {}, process name : unkown", process.processId);
            continue;
        }
        std::string command_name;
        std::getline(file, command_name);
        std::string command_name_str;
        for (std::size_t index = 0; index < command_name.size(); index++) {
            if (command_name[index] != 0)
                command_name_str.push_back(command_name[index]);
        }
        XPUM_LOG_DEBUG("process pid : {}, process name : {}", process.processId, command_name_str);
    }
    if (process_count > 1) {
        component4.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        std::string desc = "Fail to check the software exclusive. " + std::to_string(process_count) + " processses are using the device.";
        updateMessage(component4.message, desc);
    } else {
        component4.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
        updateMessage(component4.message, std::string("Pass to check the software exclusive."));
    }
    component4.finished = true;
}

bool DiagnosticManager::countDevEntry(const std::string &entry_name) {
    if (entry_name.compare(0, 7, "renderD") == 0) {
        for (std::size_t i = 7; i < entry_name.size(); i++) {
            if (!isdigit(entry_name.at(i))) {
                return false;
            }
        }
        return true;
    }
    return false;
}

void DiagnosticManager::doDeviceDiagnosticHardwareSysman(const zes_device_handle_t &zes_device,
                                                         std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    xpum_diag_component_info_t &component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_HARDWARE_SYSMAN];
    p_task_info->count += 1;
    updateMessage(component.message, std::string("Running"));
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;
    // disable hardware diagnostics due to instability
    // uint32_t test_suite_count = 0;
    // ze_result_t res;
    bool find_test_suite = false;
    bool pass_test_suite = true;
    // XPUM_ZE_HANDLE_LOCK(zes_device, res = zesDeviceEnumDiagnosticTestSuites(zes_device, &test_suite_count, nullptr));
    // if (res != ZE_RESULT_SUCCESS) {
    //     throw BaseException("zesDeviceEnumDiagnosticTestSuites()");
    // }
    // if (test_suite_count > 0) {
    //     find_test_suite = true;
    //     std::vector<zes_diag_handle_t> test_suites(test_suite_count);
    //     XPUM_ZE_HANDLE_LOCK(zes_device, res = zesDeviceEnumDiagnosticTestSuites(zes_device, &test_suite_count, test_suites.data()));
    //     if (res != ZE_RESULT_SUCCESS) {
    //         throw BaseException("zesDeviceEnumDiagnosticTestSuites()");
    //     }
    //     for (auto &test_suite : test_suites) {
    //         zes_diag_result_t result;
    //         XPUM_ZE_HANDLE_LOCK(test_suite, res = zesDiagnosticsRunTests(test_suite, ZES_DIAG_FIRST_TEST_INDEX, ZES_DIAG_LAST_TEST_INDEX, &result));
    //         if (res == ZE_RESULT_SUCCESS) {
    //             if (result != zes_diag_result_t::ZES_DIAG_RESULT_NO_ERRORS) {
    //                 pass_test_suite = false;
    //                 break;
    //             }
    //         }
    //     }
    // }
    if (find_test_suite == true && pass_test_suite == true) {
        component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
        updateMessage(component.message, std::string("Pass to do hardware sysman diagnostics."));
    } else if (find_test_suite == true && pass_test_suite == false) {
        component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        updateMessage(component.message, std::string("Fail to do hardware sysman diagnostics."));
    } else {
        component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        updateMessage(component.message, std::string("Fail to find test suites for hardware sysman diagnostics."));
    }
    component.finished = true;
}

static std::string getDevicePath(const zes_pci_properties_t& pci_props) {
    char path[PATH_MAX];
    char buf[128];
    char uevent[1024];
    DIR *pdir = NULL;
    struct dirent *pdirent = NULL;
    int len = 0;
    std::string ret = "";

    pdir = opendir("/sys/class/drm");
    if (pdir == NULL) {
        return ret;
    }

    while ((pdirent = readdir(pdir)) != NULL) {
        if (pdirent->d_name[0] == '.') {
            continue;
        }
        if (strncmp(pdirent->d_name, "render", 6) != 0) {
            continue;
        }
        if (strstr(pdirent->d_name, "-") != NULL) {
            continue;
        }
        len = snprintf(path, PATH_MAX, "/sys/class/drm/%s/device/uevent",
                pdirent->d_name);
        if (len <= 0 || len >= PATH_MAX) {
            break;
        }
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            break;
        }
        int szRead = read(fd, uevent, 1024);
        close(fd);
        if (szRead < 0 || szRead >= 1024) {
            break;
        }
        uevent[szRead] = 0;
        len = snprintf(buf, 128, "%04d:%02x:%02x.%x",
                pci_props.address.domain, pci_props.address.bus,
                pci_props.address.device, pci_props.address.function);
        if (strstr(uevent, buf) != NULL) {
            ret = "/dev/dri/";
            ret += pdirent->d_name;
            break;
        }
    }
    closedir(pdir);
    return ret;
}

void DiagnosticManager::doDeviceDiagnosticMediaCodec(const zes_device_handle_t &device, std::shared_ptr<xpum_diag_task_info_t> p_task_info,
                                                    std::map<xpum_device_id_t, std::vector<xpum_diag_media_codec_metrics_t>>& media_codec_perf_datas, bool checkOnly) {
    xpum_diag_component_info_t &component = p_task_info->componentList[
        checkOnly ?
        xpum_diag_task_type_t::XPUM_DIAG_LIGHT_CODEC :
        xpum_diag_task_type_t::XPUM_DIAG_MEDIA_CODEC
    ];
    p_task_info->count += 1;
    if (!Utility::isATSMPlatform(device)) {
        component.result = XPUM_DIAG_RESULT_FAIL;
        component.finished = true;
        updateMessage(component.message, COMPONENT_TYPE_NOT_SUPPORTED);
        return;
    }
    updateMessage(component.message, std::string("Running"));
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;

    ze_result_t ret;
    zes_pci_properties_t pci_props;
    XPUM_ZE_HANDLE_LOCK(device, ret = zesDevicePciGetProperties(device, &pci_props));
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zesDevicePciGetProperties()");
    }
    std::string device_path = getDevicePath(pci_props);
    XPUM_LOG_DEBUG("device path for media codec : {}", device_path);
    if (device_path.size() > 0) {
        std::string mediadata_folder = std::string(XPUM_RESOURCES_DIR) + std::string("mediadata/");
        if (!is_path_exist(mediadata_folder)) {
            char exe_path[XPUM_MAX_PATH_LEN];
            ssize_t len = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path));
            exe_path[len] = '\0';
            std::string  current_file = exe_path;
            mediadata_folder = current_file.substr(0, current_file.find_last_of('/')) + "/../lib/" + Configuration::getXPUMMode() + "/resources/mediadata/";
            if (!is_path_exist(mediadata_folder))
                mediadata_folder = current_file.substr(0, current_file.find_last_of('/')) + "/../lib64/" + Configuration::getXPUMMode() + "/resources/mediadata/";
        }
        bool sample_multi_transcode_tool_exist = true;
        std::ifstream file_transcode(DiagnosticManager::MEDIA_CODER_TOOLS_PATH + "sample_multi_transcode");
        if (!file_transcode.good()) {
            sample_multi_transcode_tool_exist = false;
        }

        bool h265_1080p_file_exist = true;
        std::ifstream file_h265_1080p(mediadata_folder + DiagnosticManager::MEDIA_CODER_TOOLS_1080P_FILE);
        if (!file_h265_1080p.good()) {
            h265_1080p_file_exist = false;
        }

        bool h265_4k_file_exist = true;
        std::ifstream file_h265_4k(mediadata_folder + DiagnosticManager::MEDIA_CODER_TOOLS_4K_FILE);
        if (!file_h265_4k.good()) {
            h265_4k_file_exist = false;
        }

        bool h264_light_file_exist = std::ifstream(mediadata_folder + DiagnosticManager::MEDIA_CODEC_TOOLS_LIGHT_FILE).good();
        
        if (sample_multi_transcode_tool_exist) {
            if (
                (!checkOnly && !h265_1080p_file_exist && !h265_4k_file_exist)
                || (checkOnly && !h264_light_file_exist)
            ) {
                component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
                updateMessage(component.message, std::string("No Media test file."));
            } else {
                std::string test_command;
                std::string result;
                int fps = 0;
                if (checkOnly && h264_light_file_exist) {
                    test_command = DiagnosticManager::MEDIA_CODER_TOOLS_PATH + "sample_multi_transcode -device " + device_path +
                        " -hw -i::h264 " + mediadata_folder + DiagnosticManager::MEDIA_CODEC_TOOLS_LIGHT_FILE + " -o::h264 null -n 2 2>&1";
                    XPUM_LOG_INFO("Transcoding capability check command: {}", test_command);
                    result += getCommandResult(test_command, fps);
                }
                if (!checkOnly && h265_1080p_file_exist) {
                    test_command = DiagnosticManager::MEDIA_CODER_TOOLS_PATH + "sample_multi_transcode -device " + device_path +
                                            " -hw -i::h265 " + mediadata_folder + DiagnosticManager::MEDIA_CODER_TOOLS_1080P_FILE + " -o::h265 /tmp/" + MEDIA_CODER_TOOLS_1080P_FILE + " 2>&1";
                    XPUM_LOG_INFO("Transcoding capability check command: {}", test_command);
                    result += getCommandResult(test_command, fps);
                }
                if (!checkOnly && h265_4k_file_exist) {
                    test_command = DiagnosticManager::MEDIA_CODER_TOOLS_PATH + "sample_multi_transcode -device " + device_path +
                                        " -hw -i::h265 " + mediadata_folder + DiagnosticManager::MEDIA_CODER_TOOLS_4K_FILE + " -o::h265 /tmp/test_stream_4K.265 2>&1";
                    XPUM_LOG_INFO("Transcoding capability check command: {}", test_command);
                    result += getCommandResult(test_command, fps);
                }
                if (result.find("ERR_UNSUPPORTED") != std::string::npos) {
                    XPUM_LOG_ERROR("detailed error message:\n {}", result);
                    std::string desc = "Fail to check Media transcode performance.";
                    test_command = DiagnosticManager::MEDIA_CODER_TOOLS_PATH + "sample_decode h265 -device " + device_path +
                                        " -hw -i " + mediadata_folder + DiagnosticManager::MEDIA_CODER_TOOLS_1080P_FILE + " 2>&1";
                    XPUM_LOG_INFO("Decoding capability check command: {}", test_command);
                    result = getCommandResult(test_command, fps);
                    if (result.find("Decoding finished") != std::string::npos)
                        desc += " Encoding is unsupported.";
                    else {
                        XPUM_LOG_ERROR("detail error message:\n {}", result);
                        desc += " Transcoding is unsupported.";
                    }
                    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
                    updateMessage(component.message, desc);
                } else if (result.find("ERROR") != std::string::npos) { 
                    XPUM_LOG_ERROR("detailed error message:\n {}", result);
                    std::string desc = "Fail to check Media transcode performance.";
                    if (result.find("MFX_ERR_DEVICE_FAILED") != std::string::npos)
                        desc += " Hardware device unexpected errors.";
                    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
                    updateMessage(component.message, desc);
                } else {
                    if (!checkOnly) {
                        media_codec_perf_datas[p_task_info->deviceId] = getMediaCodecMetricsData(
                            p_task_info->deviceId,
                            device_path,
                            h265_1080p_file_exist,
                            h265_4k_file_exist
                        );
                    }
                    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
                    updateMessage(component.message, std::string(
                        checkOnly ?
                        "Pass to check Media transcode functionality." :
                        "Pass to check Media transcode performance."
                    ));
                }
            }   
        } else {
            component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
            updateMessage(component.message, std::string("No sample_multi_transcode tool."));
        }

    } else {
        component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        updateMessage(component.message, std::string("Can't find the graphics device."));
    }
    component.finished = true;
}

std::string DiagnosticManager::getCommandResult(std::string command, int& fps) {
    std::array<char, 128> buffer;
    std::string result;
    FILE *pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        return "Failed to execute command";
    }
    std::string key = "frames, ";
    while (fgets(buffer.data(), 128, pipe) != nullptr) {
        std::string str(buffer.data());
        auto pos = str.find(key);
        if (pos != std::string::npos) {
            std::string data = str.substr(pos + key.size());
            fps = stoi(data);
        }
        result += str;
    }
    pclose(pipe);
    std::string::size_type pos;
    while ((pos = result.find("\n\n", 0)) != std::string::npos) {
        result.erase(pos, 1);
    }
    pos = result.find_last_of("\n");
    if (pos == result.size() - 1)
        result.erase(pos, 1);
    return result;
}

std::vector<xpum_diag_media_codec_metrics_t> DiagnosticManager::getMediaCodecMetricsData(xpum_device_id_t deviceId, std::string device_path, bool h265_1080p_file_exist, bool h265_4k_file_exist) {
    
    std::map<xpum_media_format_t, std::string> format_to_filename_1080p = {{XPUM_MEDIA_FORMAT_H264,"test_stream_1080p.264"}, {XPUM_MEDIA_FORMAT_AV1,"test_stream_1080p.av1"}};
    std::map<xpum_media_format_t, std::string> format_to_filename_4k = {{XPUM_MEDIA_FORMAT_H264,"test_stream_4K.264"}, {XPUM_MEDIA_FORMAT_AV1, "test_stream_4K.av1"}};
    int fps = -1;

    std::string convert_command;
    std::string result;
    if (h265_1080p_file_exist) {
        for (auto target : format_to_filename_1080p) {
            convert_command = DiagnosticManager::MEDIA_CODER_TOOLS_PATH + "sample_multi_transcode -device " + device_path +
                            " -hw -i::h265 /tmp/" + MEDIA_CODER_TOOLS_1080P_FILE + " -o::" + (target.first == XPUM_MEDIA_FORMAT_H264 ? "h264":"av1") + " /tmp/" + target.second + " 2>&1";
            XPUM_LOG_DEBUG("Format convert command: {}", convert_command);
            result = getCommandResult(convert_command, fps);
            XPUM_LOG_DEBUG(result);
        }
    }
    if (h265_4k_file_exist) {
        for (auto target : format_to_filename_4k) {
            convert_command = DiagnosticManager::MEDIA_CODER_TOOLS_PATH + "sample_multi_transcode -device " + device_path +
                            " -hw -i::h265 /tmp/" + MEDIA_CODER_TOOLS_4K_FILE + " -o::" + (target.first == XPUM_MEDIA_FORMAT_H264 ? "h264":"av1") + " /tmp/" + target.second + " 2>&1";
            XPUM_LOG_DEBUG("Format convert command: {}", convert_command);
            result = getCommandResult(convert_command, fps);
            XPUM_LOG_DEBUG("Format convert result: {}", result);
        }
    }

    format_to_filename_1080p[XPUM_MEDIA_FORMAT_H265] = MEDIA_CODER_TOOLS_1080P_FILE;
    format_to_filename_4k[XPUM_MEDIA_FORMAT_H265] = MEDIA_CODER_TOOLS_4K_FILE;
    std::string transcode_command;
    std::vector<xpum_diag_media_codec_metrics_t> datas;
    for (int r = XPUM_MEDIA_RESOLUTION_1080P; r <= XPUM_MEDIA_RESOLUTION_4K; r++) {
        if (r == XPUM_MEDIA_RESOLUTION_1080P && !h265_1080p_file_exist)
            continue;
        if (r == XPUM_MEDIA_RESOLUTION_4K && !h265_4k_file_exist)
            continue;
        
        for (int f = XPUM_MEDIA_FORMAT_H265; f <= XPUM_MEDIA_FORMAT_AV1; f++) {
            xpum_diag_media_codec_metrics_t data;
            data.deviceId = deviceId;
            data.resolution = static_cast<xpum_media_resolution_t>(r);
            data.format = static_cast<xpum_media_format_t>(f);

            std::string target_src_file;
            std::string target_dst_file;
            if (r == XPUM_MEDIA_RESOLUTION_1080P) {
                target_src_file = "/tmp/" + format_to_filename_1080p[data.format];
                target_dst_file = "/tmp/out_" + format_to_filename_1080p[data.format];
            } else {
                target_src_file = "/tmp/" + format_to_filename_4k[data.format];
                target_dst_file = "/tmp/out_" + format_to_filename_4k[data.format];
            }

            std::string format;
            std::string async_para = " -async 5";
            if (f == XPUM_MEDIA_FORMAT_H265) {
                format = "h265";
            }
            else if (f == XPUM_MEDIA_FORMAT_H264) {
                format = "h264";
            }
            else {
                format = "av1";
            }

            transcode_command = DiagnosticManager::MEDIA_CODER_TOOLS_PATH + "sample_multi_transcode -device " + device_path +
                    " -hw" + async_para + " -i::" + format + " " + target_src_file + " -o::" + format + " " + target_dst_file + " 2>&1";
            XPUM_LOG_INFO("Transcoding command: {}", transcode_command);
            int metric_fps = -1;
            
            for (int round = 1; round <= 3; round++) {
                fps = -1;
                result = getCommandResult(transcode_command, fps);
                XPUM_LOG_DEBUG("Transcoding round {} result: {}", round, result);
                metric_fps = std::max(metric_fps, fps);
            }
            if (metric_fps > 0) {
                updateMessage(data.fps, std::to_string(metric_fps) + " FPS");
                datas.push_back(data);
            }
        }
    }
    return datas;
}

void DiagnosticManager::doDeviceDiagnosticIntegration(const ze_device_handle_t &ze_device,
                                                      const ze_driver_handle_t &ze_driver,
                                                      std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    xpum_diag_component_info_t &component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_INTEGRATION_PCIE];
    p_task_info->count += 1;
    updateMessage(component.message, std::string("Running"));
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;

    ze_result_t ret;
    std::vector<ze_device_handle_t> device_handles;
    std::vector<long double> all_bandwidth;
    std::vector<std::thread> bandwidth_threads;
    std::vector<std::string> error_messages;
    uint32_t subdevice_count = 0;
    ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, nullptr);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeDeviceGetSubDevices()");
    }

    if (subdevice_count == 0) {
        device_handles.push_back(ze_device);
        all_bandwidth.push_back(0);
        error_messages.push_back("");
    } else {
        std::vector<ze_device_handle_t> subdevices(subdevice_count);
        ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, subdevices.data());
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeDeviceGetSubDevices()");
        }
        for (auto &subdevice : subdevices) {
            device_handles.push_back(subdevice);
            all_bandwidth.push_back(0);
            error_messages.push_back("");
        }
    }
    for (std::size_t i = 0; i < device_handles.size(); i++) {
        bandwidth_threads.push_back(std::thread([&all_bandwidth, &error_messages, i, &device_handles, &ze_driver]() {
            try {
                ze_result_t ret;
                ze_context_handle_t context;
                ze_context_desc_t context_desc = {
                        ZE_STRUCTURE_TYPE_CONTEXT_DESC,
                        nullptr, 
                        0
                };
                XPUM_ZE_HANDLE_LOCK(ze_driver, ret = zeContextCreate(ze_driver, &context_desc, &context));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeContextCreate()");
                }

                ze_command_queue_handle_t command_queue;
                ze_command_queue_desc_t command_queue_description{};
                command_queue_description.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
                command_queue_description.pNext = nullptr;
                command_queue_description.ordinal = 0;
                command_queue_description.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeCommandQueueCreate(context, device_handles[i], &command_queue_description, &command_queue));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandQueueCreate()");
                }

                ze_command_list_handle_t command_list;
                ze_command_list_desc_t command_list_description{};
                command_list_description.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
                command_list_description.pNext = nullptr;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeCommandListCreate(context, device_handles[i], &command_list_description, &command_list));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListCreate()");
                }
                long double total_bandwidth = 0.0;
                long double total_latency = 0.0;

                // DCGM PCIE_STR_INTS_PER_COPY 10000000.0 * 4 bytes = 40Mb
                std::size_t size = 1 << 28;
                void *device_buffer;
                void *host_buffer;

                ze_device_mem_alloc_desc_t device_desc;
                device_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
                device_desc.pNext = nullptr;
                device_desc.ordinal = 0;
                device_desc.flags = 0;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeMemAllocDevice(context, &device_desc, size, 1, device_handles[i], &device_buffer));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemAllocDevice()");
                }

                ze_host_mem_alloc_desc_t host_desc;
                host_desc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
                host_desc.pNext = nullptr;
                host_desc.flags = 0;
                ret = zeMemAllocHost(context, &host_desc, size, 1, &host_buffer);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemAllocHost()");
                }

                uint32_t number_iterations = 500;
                long double total_time_nsec = 0.0;
                std::size_t element_size = sizeof(uint8_t);
                std::size_t buffer_size = element_size * size;
                ret = zeCommandListAppendMemoryCopy(command_list, device_buffer, host_buffer, buffer_size, nullptr, 0, nullptr);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListAppendMemoryCopy()");
                }
                ret = zeCommandListClose(command_list);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListClose()");
                }
                std::chrono::high_resolution_clock::time_point time_start, time_end;
                time_start = std::chrono::high_resolution_clock::now();
                for (uint32_t i = 0; i < number_iterations; i++) {
                    ret = zeCommandQueueExecuteCommandLists(command_queue, 1, &command_list, nullptr);
                    if (ret != ZE_RESULT_SUCCESS) {
                        throw BaseException("zeCommandQueueExecuteCommandLists()");
                    }
                    waitForCommandQueueSynchronize(command_queue, "zeCommandQueueSynchronize()");
                }
                time_end = std::chrono::high_resolution_clock::now();
                total_time_nsec = std::chrono::duration<long double, std::chrono::nanoseconds::period>(time_end - time_start).count();

                ret = zeCommandListDestroy(command_list);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListDestroy()");
                }
                ret = zeCommandQueueDestroy(command_queue);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandQueueDestroy()");
                }
                ret = zeMemFree(context, const_cast<void *>(device_buffer));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemFree()");
                }
                ret = zeMemFree(context, const_cast<void *>(host_buffer));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemFree()");
                }
                ret = zeContextDestroy(context);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeContextDestroy()");
                }
                calculateBandwidthLatency(total_time_nsec, static_cast<long double>(size * number_iterations), total_bandwidth, total_latency, number_iterations);
                all_bandwidth[i] = total_bandwidth;
            } catch (BaseException &e) {
                XPUM_LOG_DEBUG("Error in integration diagnostic");
                all_bandwidth[i] = -1;
                error_messages[i] = e.what();
            } catch (...) {
                XPUM_LOG_DEBUG("Error in integration diagnostic");
                all_bandwidth[i] = -1;
            }
        }));
    }
    for (std::size_t i = 0; i < bandwidth_threads.size(); i++) {
        bandwidth_threads[i].join();
    }

    long double total_bandwidth = 0;
    for (std::size_t i = 0; i < bandwidth_threads.size(); i++) {
        if (all_bandwidth[i] < 0) {
            if (error_messages[i].length() == 0)
                throw BaseException("unknown reasons");
            else
                throw BaseException(error_messages[i]);
        }
        total_bandwidth += all_bandwidth[i];
    }
    std::string bandwidth_detail = " Its bandwidth is " + roundDouble(total_bandwidth, 3) + " GBPS.";
    auto bandwidth_threshold = 0;
    if (device_names.find(ze_device) != device_names.end()) {
        bandwidth_threshold = thresholds[device_names[ze_device]]["PCIE_BANDWIDTH_MIN_GBPS"];
    }

    if (bandwidth_threshold <= 0) {
        std::string desc = "Fail to check PCIe bandwidth.";
        desc += bandwidth_detail;
        desc += "  Unconfigured or invalid threshold.";
        component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        updateMessage(component.message, desc);
    } else if (total_bandwidth < bandwidth_threshold) {
        std::string desc = "Fail to check PCIe bandwidth.";
        desc += bandwidth_detail;
        desc += " Threshold is " + std::to_string(bandwidth_threshold) + " GBPS.";
        component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        updateMessage(component.message, desc);
    } else {
        std::string desc = "Pass to check PCIe bandwidth.";
        desc += bandwidth_detail;
        component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
        updateMessage(component.message, desc);
    }
    component.finished = true;
}

void DiagnosticManager::calculateBandwidthLatency(long double total_time_nsec, long double total_data_transfer,
                                                  long double &total_bandwidth, long double &total_latency, std::size_t number_iterations) {
    long double total_time_s;
    total_time_s = total_time_nsec / 1e9;                 /* Units in Seconds */
    total_data_transfer /= static_cast<long double>(1e9); /* Units in Gigabytes */

    total_bandwidth = total_data_transfer / total_time_s;
    total_latency = total_time_nsec / (1e3 * number_iterations);
}

void DiagnosticManager::showResultsHost2device(std::size_t buffer_size, long double total_bandwidth, long double total_latency) {
    std::cout << "Host->Device[" << std::fixed << std::setw(10) << buffer_size
              << "]:  BW = " << std::setw(9) << std::setprecision(6)
              << total_bandwidth << " GBPS  Latency = " << std::setw(9)
              << std::setprecision(2) << total_latency << " usec" << std::endl;
}

void DiagnosticManager::doDeviceDiagnosticPeformanceMemoryAllocation(const ze_device_handle_t &ze_device,
                                                                     const ze_driver_handle_t &ze_driver,
                                                                     std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    xpum_diag_component_info_t &component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION];
    p_task_info->count += 1;
    updateMessage(component.message, std::string("Running"));
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;

    ze_result_t ret;
    uint64_t one_MB = 1024UL * 1024UL;
    uint64_t one_GB = 1UL * 1024UL * 1024UL * 1024UL;
    uint32_t workgroup_size_x_ = 8;
    uint32_t number_of_kernel_args_ = 2;
    uint32_t number_of_kernels_in_module_ = 10;

    std::vector<float> memory_uses = {0.1}; // {0.1, 0.5, 0.9, 1}
    std::vector<uint64_t> allocate_sizes = {one_MB, one_GB};
    std::vector<std::string> memory_types = {"DEVICE", "SHARED"};

    for (auto &memory_use : memory_uses)
        for (auto &allocate_size : allocate_sizes)
            for (auto &memory_type : memory_types) {
                ze_device_properties_t device_properties;
                device_properties.pNext = nullptr;
                device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeDeviceGetProperties(ze_device, &device_properties));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeDeviceGetProperties()");
                }
                ze_context_desc_t context_desc = {
                        ZE_STRUCTURE_TYPE_CONTEXT_DESC,
                        nullptr, 
                        0
                };
                ze_context_handle_t context;
                XPUM_ZE_HANDLE_LOCK(ze_driver, ret = zeContextCreate(ze_driver, &context_desc, &context));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeContextCreate()");
                }
                uint64_t max_allocation_size = workgroup_size_x_ * (device_properties.maxMemAllocSize / workgroup_size_x_) * memory_use;
                uint64_t one_case_requested_allocation_size = allocate_size * number_of_kernel_args_;

                if (one_case_requested_allocation_size > max_allocation_size) {
                    one_case_requested_allocation_size = max_allocation_size;
                }

                std::size_t one_case_allocation_count = one_case_requested_allocation_size / (number_of_kernel_args_ * sizeof(uint8_t));
                std::uint64_t number_of_dispatch = max_allocation_size / one_case_requested_allocation_size;
                std::vector<uint8_t *> input_allocations;
                std::vector<uint8_t *> output_allocations;
                std::vector<std::vector<uint8_t>> data_out_vector;
                std::vector<std::string> test_kernel_names;

                for (uint64_t dispatch_id = 0; dispatch_id < number_of_dispatch; dispatch_id++) {
                    uint8_t *input_allocation;
                    uint8_t *output_allocation;
                    if (memory_type == "HOST") {
                        ze_host_mem_alloc_desc_t host_desc_input = {};
                        host_desc_input.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
                        host_desc_input.flags = 0;
                        host_desc_input.pNext = nullptr;
                        void *memory_input = nullptr;
                        ret = zeMemAllocHost(context, &host_desc_input, one_case_allocation_count, 8, &memory_input);
                        if (ret != ZE_RESULT_SUCCESS) {
                            throw BaseException("zeMemAllocHost()");
                        }
                        input_allocation = (uint8_t *)memory_input;

                        ze_host_mem_alloc_desc_t host_desc_output = {};
                        host_desc_output.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
                        host_desc_output.flags = 0;
                        host_desc_output.pNext = nullptr;
                        void *memory_output = nullptr;
                        ret = zeMemAllocHost(context, &host_desc_output, one_case_allocation_count, 8, &memory_output);
                        if (ret != ZE_RESULT_SUCCESS) {
                            throw BaseException("zeMemAllocHost()");
                        }
                        output_allocation = (uint8_t *)memory_output;

                    } else if (memory_type == "DEVICE") {
                        void *memory_input = nullptr;
                        ze_device_mem_alloc_desc_t device_desc_input = {};
                        device_desc_input.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
                        device_desc_input.ordinal = 0;
                        device_desc_input.flags = 0;
                        device_desc_input.pNext = nullptr;
                        XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeMemAllocDevice(context, &device_desc_input, one_case_allocation_count, 8, ze_device, &memory_input));
                        if (ret != ZE_RESULT_SUCCESS) {
                            throw BaseException("zeMemAllocDevice()");
                        }
                        input_allocation = (uint8_t *)memory_input;

                        void *memory_output = nullptr;
                        ze_device_mem_alloc_desc_t device_desc_output = {};
                        device_desc_output.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
                        device_desc_output.ordinal = 0;
                        device_desc_output.flags = 0;
                        device_desc_output.pNext = nullptr;
                        XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeMemAllocDevice(context, &device_desc_output, one_case_allocation_count, 8, ze_device, &memory_output));
                        if (ret != ZE_RESULT_SUCCESS) {
                            throw BaseException("zeMemAllocDevice()");
                        }
                        output_allocation = (uint8_t *)memory_output;
                    } else {
                        void *memory_input = nullptr;
                        ze_device_mem_alloc_desc_t device_desc_input = {};
                        device_desc_input.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
                        device_desc_input.ordinal = 0;
                        device_desc_input.flags = 0;
                        device_desc_input.pNext = nullptr;

                        ze_host_mem_alloc_desc_t host_desc_input = {};
                        host_desc_input.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
                        host_desc_input.flags = 0;
                        host_desc_input.pNext = nullptr;
                        XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeMemAllocShared(context, &device_desc_input, &host_desc_input, one_case_allocation_count, 8, ze_device, &memory_input));
                        if (ret != ZE_RESULT_SUCCESS) {
                            throw BaseException("zeMemAllocShared()");
                        }
                        input_allocation = (uint8_t *)memory_input;

                        void *memory_output = nullptr;
                        ze_device_mem_alloc_desc_t device_desc_output = {};
                        device_desc_output.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
                        device_desc_output.ordinal = 0;
                        device_desc_output.flags = 0;
                        device_desc_output.pNext = nullptr;

                        ze_host_mem_alloc_desc_t host_desc_output = {};
                        host_desc_output.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
                        host_desc_output.flags = 0;
                        host_desc_output.pNext = nullptr;
                        XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeMemAllocShared(context, &device_desc_output, &host_desc_output, one_case_allocation_count, 8, ze_device, &memory_output));
                        output_allocation = (uint8_t *)memory_output;
                        if (ret != ZE_RESULT_SUCCESS) {
                            throw BaseException("zeMemAllocShared()");
                        }
                    }
                    input_allocations.push_back(input_allocation);
                    output_allocations.push_back(output_allocation);
                    std::vector<uint8_t> data_out(one_case_allocation_count, 0);
                    data_out_vector.push_back(data_out);
                    std::string kernel_name;
                    kernel_name = "test_device_memory" + std::to_string((dispatch_id % number_of_kernels_in_module_) + 1);
                    test_kernel_names.push_back(kernel_name);
                }

                const std::vector<uint8_t> binary_file = loadBinaryFile("test_multiple_memory_allocations.spv");
                ze_module_desc_t module_description = {};
                module_description.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;
                module_description.pNext = nullptr;
                module_description.format = ZE_MODULE_FORMAT_IL_SPIRV;
                module_description.inputSize = static_cast<uint32_t>(binary_file.size());
                module_description.pInputModule = binary_file.data();
                module_description.pBuildFlags = nullptr;

                ze_module_handle_t module_handle = nullptr;
                XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeModuleCreate(context, ze_device, &module_description, &module_handle, nullptr));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeModuleCreate()");
                }
                dispatchKernelsForMemoryTest(ze_device, module_handle, input_allocations, output_allocations,
                                             data_out_vector, test_kernel_names, number_of_dispatch, one_case_allocation_count, context);
                for (auto each_allocation : input_allocations) {
                    ret = zeMemFree(context, each_allocation);
                    if (ret != ZE_RESULT_SUCCESS) {
                        throw BaseException("zeMemFree()");
                    }
                }
                for (auto each_allocation : output_allocations) {
                    ret = zeMemFree(context, each_allocation);
                    if (ret != ZE_RESULT_SUCCESS) {
                        throw BaseException("zeMemFree()");
                    }
                }

                ret = zeModuleDestroy(module_handle);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeModuleDestroy()");
                }
                ret = zeContextDestroy(context);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeContextDestroy()");
                }
            }
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
    updateMessage(component.message, std::string("Pass to check memory allocation."));
    XPUM_LOG_INFO("Pass to check memory allocation.");
    component.finished = true;
}

std::vector<uint8_t> DiagnosticManager::loadBinaryFile(const std::string &file_path) {
    std::string folder = std::string(XPUM_RESOURCES_DIR) + std::string("kernels/");
    if (!is_path_exist(folder)) {
        char exe_path[XPUM_MAX_PATH_LEN];
        ssize_t len = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path));
        exe_path[len] = '\0';
        std::string current_file = exe_path;
        folder = current_file.substr(0, current_file.find_last_of('/')) + "/../lib/" + Configuration::getXPUMMode() + "/resources/kernels/";
        if (!is_path_exist(folder))
            folder = current_file.substr(0, current_file.find_last_of('/')) + "/../lib64/" + Configuration::getXPUMMode() + "/resources/kernels/";
    }
    std::string absolute_file_path = folder + file_path;
    std::ifstream stream(absolute_file_path, std::ios::in | std::ios::binary);
    std::vector<uint8_t> binary_file;

    if (!stream.good()) {
        throw BaseException("load kernel file");
    }

    std::size_t length = 0;
    stream.seekg(0, stream.end);
    length = static_cast<std::size_t>(stream.tellg());
    stream.seekg(0, stream.beg);
    binary_file.resize(length);
    stream.read(reinterpret_cast<char *>(binary_file.data()), length);

    return binary_file;
}

void DiagnosticManager::doDeviceDiagnosticMemoryError(const ze_device_handle_t &ze_device,
                                                                     const ze_driver_handle_t &ze_driver,
                                                                     std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    xpum_diag_component_info_t &component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_MEMORY_ERROR];
    p_task_info->count += 1;
    updateMessage(component.message, std::string("Running"));
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;
    ze_result_t ret;
    uint64_t one_MB = 1024UL * 1024UL;
    uint32_t workgroup_size_x_ = 8;
    uint32_t number_of_kernel_args_ = 2;
    uint32_t number_of_kernels_in_module_ = 10;
    uint8_t init_value_1_ = 0;
    uint8_t init_value_2_ = 0xAA; // 1010 1010
    float memory_use_percentage_for_error_test = MEMORY_USE_PERCENTAGE_FOR_ERROR_TEST;

    XPUM_LOG_DEBUG("memory use_percentage for memory error test: {}", MEMORY_USE_PERCENTAGE_FOR_ERROR_TEST);
    uint32_t error_count = 0;
    ze_device_properties_t device_properties;
    device_properties.pNext = nullptr;
    device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeDeviceGetProperties(ze_device, &device_properties));
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeDeviceGetProperties()");
    }

    uint64_t physical_size = 0;
    uint32_t mem_module_count = 0;
    XPUM_ZE_HANDLE_LOCK(ze_device, ret = zesDeviceEnumMemoryModules(ze_device, &mem_module_count, nullptr));
    std::vector<zes_mem_handle_t> mems(mem_module_count);
    XPUM_ZE_HANDLE_LOCK(ze_device, ret = zesDeviceEnumMemoryModules(ze_device, &mem_module_count, mems.data()));
    if (ret == ZE_RESULT_SUCCESS) {
        for (auto& mem : mems) {
            uint64_t mem_module_physical_size = 0;
            zes_mem_properties_t props;
            props.pNext = nullptr;
            props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
            XPUM_ZE_HANDLE_LOCK(mem, ret = zesMemoryGetProperties(mem, &props));
            if (ret == ZE_RESULT_SUCCESS) {
                mem_module_physical_size = props.physicalSize;
            }

            zes_mem_state_t sysman_memory_state = {};
            sysman_memory_state.stype = ZES_STRUCTURE_TYPE_MEM_STATE;
            sysman_memory_state.pNext = nullptr;
            XPUM_ZE_HANDLE_LOCK(mem, ret = zesMemoryGetState(mem, &sysman_memory_state));
            if (ret == ZE_RESULT_SUCCESS) {
                if (props.physicalSize == 0) {
                    mem_module_physical_size = sysman_memory_state.size;
                }
                physical_size = mem_module_physical_size;
            }
        }
    }

    uint64_t target_test_memory_size = (std::min(physical_size, std::max(physical_size, device_properties.maxMemAllocSize)) * 1.0 * memory_use_percentage_for_error_test);
    XPUM_LOG_DEBUG("memory physical size: {}, max mem alloc size: {}, target test size: {}", physical_size, device_properties.maxMemAllocSize, target_test_memory_size);
    ze_context_desc_t context_desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_context_handle_t context;
    XPUM_ZE_HANDLE_LOCK(ze_driver, ret = zeContextCreate(ze_driver, &context_desc, &context));
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeContextCreate()");
    }
    uint64_t max_allocation_size = workgroup_size_x_ * (target_test_memory_size / workgroup_size_x_);
    uint64_t one_case_requested_allocation_size = one_MB * number_of_kernel_args_;

    if (one_case_requested_allocation_size > max_allocation_size) {
        one_case_requested_allocation_size = max_allocation_size;
    }
    
    std::size_t one_case_allocation_count = one_case_requested_allocation_size / (number_of_kernel_args_ * sizeof(uint8_t));
    std::uint64_t number_of_dispatch = max_allocation_size / one_case_requested_allocation_size;

    std::vector<ze_device_handle_t> device_handles;
    uint32_t subdevice_count = 0;
    ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, nullptr);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeDeviceGetSubDevices()");
    }

    if (subdevice_count == 0) {
        device_handles.push_back(ze_device);
    } else {
        std::vector<ze_device_handle_t> subdevices(subdevice_count);
        ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, subdevices.data());
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeDeviceGetSubDevices()");
        }
        for (auto &subdevice : subdevices) {
            device_handles.push_back(subdevice);
        }
    }

    for (auto& device : device_handles) {
        std::vector<uint8_t *> input_allocations;
        std::vector<uint8_t *> output_allocations;
        std::vector<std::vector<uint8_t>> data_out_vector;
        std::vector<std::string> test_kernel_names;

        for (uint64_t dispatch_id = 0; dispatch_id < number_of_dispatch; dispatch_id++) {
            uint8_t *input_allocation;
            uint8_t *output_allocation;
            void *memory_input = nullptr;
            ze_device_mem_alloc_desc_t device_desc_input = {};
            device_desc_input.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
            device_desc_input.ordinal = 0;
            device_desc_input.flags = 0;
            device_desc_input.pNext = nullptr;
            XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeMemAllocDevice(context, &device_desc_input, one_case_allocation_count, 8, device, &memory_input));
            if (ret != ZE_RESULT_SUCCESS) {
                throw BaseException("zeMemAllocDevice()");
            }
            input_allocation = (uint8_t *)memory_input;

            void *memory_output = nullptr;
            ze_device_mem_alloc_desc_t device_desc_output = {};
            device_desc_output.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
            device_desc_output.ordinal = 0;
            device_desc_output.flags = 0;
            device_desc_output.pNext = nullptr;
            XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeMemAllocDevice(context, &device_desc_output, one_case_allocation_count, 8, device, &memory_output));
            if (ret != ZE_RESULT_SUCCESS) {
                throw BaseException("zeMemAllocDevice()");
            }
            output_allocation = (uint8_t *)memory_output;
            
            input_allocations.push_back(input_allocation);
            output_allocations.push_back(output_allocation);
            std::vector<uint8_t> data_out(one_case_allocation_count, init_value_1_);
            data_out_vector.push_back(data_out);
            std::string kernel_name;
            kernel_name = "test_device_memory" + std::to_string((dispatch_id % number_of_kernels_in_module_) + 1);
            test_kernel_names.push_back(kernel_name);
        }

        const std::vector<uint8_t> binary_file = loadBinaryFile("test_multiple_memory_allocations.spv");
        ze_module_desc_t module_description = {};
        module_description.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;
        module_description.pNext = nullptr;
        module_description.format = ZE_MODULE_FORMAT_IL_SPIRV;
        module_description.inputSize = static_cast<uint32_t>(binary_file.size());
        module_description.pInputModule = binary_file.data();
        module_description.pBuildFlags = nullptr;

        ze_module_handle_t module_handle = nullptr;
        XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeModuleCreate(context, device, &module_description, &module_handle, nullptr));
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeModuleCreate()");
        }
        dispatchKernelsForMemoryTest(device, module_handle, input_allocations, output_allocations,
                                        data_out_vector, test_kernel_names, number_of_dispatch, one_case_allocation_count, context);
        for (auto each_allocation : input_allocations) {
            ret = zeMemFree(context, each_allocation);
            if (ret != ZE_RESULT_SUCCESS) {
                throw BaseException("zeMemFree()");
            }
        }
        for (auto each_allocation : output_allocations) {
            ret = zeMemFree(context, each_allocation);
            if (ret != ZE_RESULT_SUCCESS) {
                throw BaseException("zeMemFree()");
            }
        }

        ret = zeModuleDestroy(module_handle);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeModuleDestroy()");
        }
        int tile_level_error_count = 0;
        for (auto each_data_out : data_out_vector) {
            for (uint32_t i = 0; i < one_case_allocation_count; i++) {
                if (init_value_2_ != each_data_out[i]) {
                    tile_level_error_count += 1;
                }
            }
        }
        if (tile_level_error_count <= 1)
            XPUM_LOG_INFO("{} error was found", tile_level_error_count);
        else
            XPUM_LOG_INFO("{} errors were found", tile_level_error_count);
        error_count += tile_level_error_count;
    }
    ret = zeContextDestroy(context);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeContextDestroy()");
    }
    if (error_count == 0) {
        component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
        updateMessage(component.message, std::string("Pass to check memory error."));
    } else {
        component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        std::string desc = "Fail to check memory error. ";
        if (error_count == 1) {
            desc += std::to_string(error_count) + " error was found.";
        } else  {
            desc += std::to_string(error_count) + " errors were found.";
        }
        updateMessage(component.message, desc);
    }
    component.finished = true;
}

void DiagnosticManager::dispatchKernelsForMemoryTest(const ze_device_handle_t device,
                                                     ze_module_handle_t module,
                                                     std::vector<uint8_t *> src_allocations,
                                                     std::vector<uint8_t *> dst_allocations,
                                                     std::vector<std::vector<uint8_t>> &data_out,
                                                     const std::vector<std::string> &test_kernel_names,
                                                     uint64_t number_of_dispatch,
                                                     uint64_t one_case_allocation_count,
                                                     ze_context_handle_t context) {
    ze_result_t ret;
    ze_command_list_desc_t command_list_description = {};
    command_list_description.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
    command_list_description.pNext = nullptr;
    std::vector<ze_kernel_handle_t> test_functions;
    uint32_t workgroup_size_x_ = 8;
    uint8_t init_value_2_ = 0xAA; // 1010 1010
    uint8_t init_value_3_ = 0x55; // 0101 0101

    ze_command_list_handle_t command_list = nullptr;
    XPUM_ZE_HANDLE_LOCK(device, ret = zeCommandListCreate(context, device, &command_list_description, &command_list));
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeCommandListCreate()");
    }
    for (uint64_t dispatch_id = 0; dispatch_id < number_of_dispatch; dispatch_id++) {
        uint8_t *src_allocation = src_allocations[dispatch_id];
        uint8_t *dst_allocation = dst_allocations[dispatch_id];

        ze_kernel_desc_t test_function_description = {};
        test_function_description.stype = ZE_STRUCTURE_TYPE_KERNEL_DESC;
        test_function_description.pNext = nullptr;
        test_function_description.flags = 0;
        test_function_description.pKernelName = test_kernel_names[dispatch_id].c_str();
        ze_kernel_handle_t test_function = nullptr;

        ret = zeKernelCreate(module, &test_function_description, &test_function);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeKernelCreate()");
        }
        ret = zeKernelSetGroupSize(test_function, workgroup_size_x_, 1, 1);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeKernelSetGroupSize()");
        }
        ret = zeKernelSetArgumentValue(test_function, 0, sizeof(src_allocation), &src_allocation);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeKernelSetArgumentValue()");
        }
        ret = zeKernelSetArgumentValue(test_function, 1, sizeof(dst_allocation), &dst_allocation);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeKernelSetArgumentValue()");
        }
        uint32_t group_count_x = one_case_allocation_count / workgroup_size_x_;
        ze_group_count_t thread_group_dimensions = {group_count_x, 1, 1};

        ret = zeCommandListAppendMemoryFill(command_list, src_allocation,
                                            &init_value_2_, sizeof(uint8_t),
                                            one_case_allocation_count *
                                                sizeof(uint8_t),
                                            nullptr, 0, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandListAppendMemoryFill()");
        }
        ret = zeCommandListAppendBarrier(command_list, nullptr, 0, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandListAppendBarrier()");
        }
        ret = zeCommandListAppendMemoryFill(command_list, dst_allocation,
                                            &init_value_3_, sizeof(uint8_t),
                                            one_case_allocation_count *
                                                sizeof(uint8_t),
                                            nullptr, 0, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandListAppendMemoryFill()");
        }
        ret = zeCommandListAppendBarrier(command_list, nullptr, 0, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandListAppendBarrier()");
        }
        ret = zeCommandListAppendLaunchKernel(command_list, test_function, &thread_group_dimensions, nullptr, 0, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandListAppendLaunchKernel()");
        }
        ret = zeCommandListAppendBarrier(command_list, nullptr, 0, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandListAppendBarrier()");
        }
        ret = zeCommandListAppendMemoryCopy(command_list, data_out[dispatch_id].data(), dst_allocation,
                                            data_out[dispatch_id].size() * sizeof(uint8_t), nullptr, 0, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandListAppendMemoryCopy()");
        }

        ret = zeCommandListAppendBarrier(command_list, nullptr, 0, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandListAppendBarrier()");
        }
        test_functions.push_back(test_function);
    }
    ret = zeCommandListClose(command_list);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeCommandListClose()");
    }
    ze_command_queue_handle_t command_queue;
    ze_command_queue_desc_t command_queue_description = {};
    command_queue_description.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    command_queue_description.pNext = nullptr;
    command_queue_description.ordinal = 0;
    command_queue_description.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    command_queue_description.flags = 0;
    XPUM_ZE_HANDLE_LOCK(device, ret = zeCommandQueueCreate(context, device, &command_queue_description, &command_queue));
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeCommandQueueCreate()");
    }
    ret = zeCommandQueueExecuteCommandLists(command_queue, 1, &command_list, nullptr);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeCommandQueueExecuteCommandLists()");
    }
    waitForCommandQueueSynchronize(command_queue, "zeCommandQueueSynchronize()");
    ret = zeCommandQueueDestroy(command_queue);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeCommandQueueDestroy()");
    }
    ret = zeCommandListDestroy(command_list);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeCommandListDestroy()");
    }
    for (uint64_t dispatch_id = 0; dispatch_id < test_functions.size(); dispatch_id++) {
        ret = zeKernelDestroy(test_functions[dispatch_id]);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeKernelDestroy()");
        }
    }
}

void DiagnosticManager::doDeviceDiagnosticPeformanceComputation(const ze_device_handle_t &ze_device, const ze_driver_handle_t &ze_driver, std::shared_ptr<xpum_diag_task_info_t> p_task_info, bool checkOnly) {
    int comp_index = 0;
    if (checkOnly == true) {
        comp_index = xpum_diag_task_type_t::XPUM_DIAG_COMPUTATION;
    } else {
        comp_index = xpum_diag_task_type_t::XPUM_DIAG_PERFORMANCE_COMPUTATION;
    }
    xpum_diag_component_info_t &compute_component = p_task_info->componentList[comp_index];
    p_task_info->count += 1;
    updateMessage(compute_component.message, std::string("Running"));
    compute_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;

    ze_result_t ret;
    std::vector<ze_device_handle_t> device_handles;
    std::vector<long double> all_gflops;
    std::vector<std::thread> compute_threads;
    std::vector<std::string> error_messages;

    uint32_t subdevice_count = 0;
    ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, nullptr);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeDeviceGetSubDevices()");
    }
    if (subdevice_count == 0) {
        device_handles.push_back(ze_device);
        all_gflops.push_back(0);
        error_messages.push_back("");
    } else {
        std::vector<ze_device_handle_t> subdevices(subdevice_count);
        ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, subdevices.data());
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeDeviceGetSubDevices()");
        }
        for (auto &subdevice : subdevices) {
            device_handles.push_back(subdevice);
            all_gflops.push_back(0);
            error_messages.push_back("");
        }
    }

    for (std::size_t i = 0; i < device_handles.size(); i++) {
        compute_threads.push_back(std::thread([&all_gflops, &error_messages, i, &device_handles, &ze_driver, checkOnly]() {
            try {
                ze_result_t ret;
                long double timed;
                std::size_t flops_per_work_item = 4096;
                struct ZeWorkGroups workgroup_info;
                float input_value = 1.3f;

                ze_device_properties_t device_properties;
                device_properties.pNext = nullptr;
                device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeDeviceGetProperties(device_handles[i], &device_properties));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeDeviceGetProperties()");
                }

                ze_device_compute_properties_t device_compute_properties;
                device_compute_properties.pNext = nullptr;
                device_compute_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeDeviceGetComputeProperties(device_handles[i], &device_compute_properties));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeDeviceGetComputeProperties()");
                }
                ze_context_handle_t context;
                ze_context_desc_t context_desc = {
                        ZE_STRUCTURE_TYPE_CONTEXT_DESC,
                        nullptr, 
                        0
                };
                XPUM_ZE_HANDLE_LOCK(ze_driver, ret = zeContextCreate(ze_driver, &context_desc, &context));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeContextCreate()");
                }
                ze_module_handle_t module_handle;
                ze_module_desc_t module_description = {};
                std::vector<uint8_t> binary_file = loadBinaryFile("ze_sp_compute.spv");
                module_description.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;
                module_description.pNext = nullptr;
                module_description.format = ZE_MODULE_FORMAT_IL_SPIRV;
                module_description.inputSize = static_cast<uint32_t>(binary_file.size());
                module_description.pInputModule = binary_file.data();
                module_description.pBuildFlags = nullptr;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeModuleCreate(context, device_handles[i], &module_description, &module_handle, nullptr));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeModuleCreate()");
                }
                uint64_t max_work_items = device_properties.numSlices *
                                          device_properties.numSubslicesPerSlice *
                                          device_properties.numEUsPerSubslice *
                                          device_compute_properties.maxGroupCountX * 2048;

                uint64_t max_number_of_allocated_items = device_properties.maxMemAllocSize / sizeof(float);
                uint64_t number_of_work_items = std::min(max_number_of_allocated_items, (max_work_items * sizeof(float)));
                if (checkOnly) {
                    number_of_work_items = 10000;
                }
                number_of_work_items = setWorkgroups(device_compute_properties, number_of_work_items, &workgroup_info);

                void *device_input_value;
                ze_device_mem_alloc_desc_t in_device_desc = {};
                in_device_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
                in_device_desc.pNext = nullptr;
                in_device_desc.ordinal = 0;
                in_device_desc.flags = 0;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeMemAllocDevice(context, &in_device_desc, sizeof(float), 1, device_handles[i], &device_input_value));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemAllocDevice()");
                }
                void *device_output_buffer;
                ze_device_mem_alloc_desc_t out_device_desc = {};
                out_device_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
                out_device_desc.pNext = nullptr;
                out_device_desc.ordinal = 0;
                out_device_desc.flags = 0;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeMemAllocDevice(context, &out_device_desc, static_cast<std::size_t>((number_of_work_items * sizeof(float))),
                                                                              1, device_handles[i], &device_output_buffer));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemAllocDevice()");
                }
                ze_command_list_handle_t command_list;
                ze_command_list_desc_t command_list_description{};
                command_list_description.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
                command_list_description.pNext = nullptr;
                command_list_description.flags = ZE_COMMAND_LIST_FLAG_EXPLICIT_ONLY;
                command_list_description.commandQueueGroupOrdinal = 0;

                ze_command_queue_handle_t command_queue;
                ze_command_queue_desc_t command_queue_description{};
                command_queue_description.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
                command_queue_description.pNext = nullptr;
                command_queue_description.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
                command_queue_description.flags = ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY;
                command_queue_description.ordinal = 0;
                command_queue_description.index = 0;

                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeCommandListCreate(context, device_handles[i], &command_list_description, &command_list));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListCreate()");
                }
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeCommandQueueCreate(context, device_handles[i], &command_queue_description, &command_queue));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandQueueCreate()");
                }
                ret = zeCommandListAppendMemoryCopy(command_list, device_input_value, &input_value, sizeof(float), nullptr, 0, nullptr);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListAppendMemoryCopy()");
                }
                ret = zeCommandListAppendBarrier(command_list, nullptr, 0, nullptr);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListAppendBarrier()");
                }
                ret = zeCommandListClose(command_list);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListClose()");
                }
                ret = zeCommandQueueExecuteCommandLists(command_queue, 1, &command_list, nullptr);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandQueueExecuteCommandLists()");
                }
                waitForCommandQueueSynchronize(command_queue, "zeCommandQueueSynchronize()");
                ret = zeCommandListReset(command_list);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListReset()");
                }
                ze_kernel_handle_t compute_sp_v1;
                ze_kernel_handle_t compute_sp_v2;
                ze_kernel_handle_t compute_sp_v4;
                ze_kernel_handle_t compute_sp_v8;
                ze_kernel_handle_t compute_sp_v16;

                setupFunction(module_handle, compute_sp_v1, "compute_sp_v1", device_input_value, device_output_buffer);
                setupFunction(module_handle, compute_sp_v2, "compute_sp_v2", device_input_value, device_output_buffer);
                setupFunction(module_handle, compute_sp_v4, "compute_sp_v4", device_input_value, device_output_buffer);
                setupFunction(module_handle, compute_sp_v8, "compute_sp_v8", device_input_value, device_output_buffer);
                setupFunction(module_handle, compute_sp_v16, "compute_sp_v16", device_input_value, device_output_buffer);

                timed = 0;
                long double current;
                // Vector width 1
                timed = runKernel(command_queue, command_list, compute_sp_v1, workgroup_info, XPUM_DIAG_PERFORMANCE_COMPUTATION, checkOnly);
                current = calculateGbps(timed, number_of_work_items * flops_per_work_item);
                all_gflops[i] = std::max(all_gflops[i], current);
                XPUM_LOG_INFO("compute sp vector width 1 done");

                // disable these kernel functions to be compatible with ATS-M
                /* 
                timed = 0;
                // Vector width 2
                timed = runKernel(command_queue, command_list, compute_sp_v2, workgroup_info, XPUM_DIAG_PERFORMANCE_COMPUTATION);
                current = calculateGbps(timed, number_of_work_items * flops_per_work_item);
                gflops = std::max(gflops, current);
                XPUM_LOG_INFO("compute sp vector width 2 done");

                timed = 0;
                // Vector width 4
                timed = runKernel(command_queue, command_list, compute_sp_v4, workgroup_info, XPUM_DIAG_PERFORMANCE_COMPUTATION);
                current = calculateGbps(timed, number_of_work_items * flops_per_work_item);
                gflops = std::max(gflops, current);
                XPUM_LOG_INFO("compute sp vector width 4 done");

                timed = 0;
                // Vector width 8
                timed = runKernel(command_queue, command_list, compute_sp_v8, workgroup_info, XPUM_DIAG_PERFORMANCE_COMPUTATION);
                current = calculateGbps(timed, number_of_work_items * flops_per_work_item);
                gflops = std::max(gflops, current);
                XPUM_LOG_INFO("compute sp vector width 8 done");

                timed = 0;
                // Vector width 16
                timed = runKernel(command_queue, command_list, compute_sp_v16, workgroup_info, XPUM_DIAG_PERFORMANCE_COMPUTATION);
                current = calculateGbps(timed, number_of_work_items * flops_per_work_item);
                gflops = std::max(gflops, current);
                XPUM_LOG_INFO("compute sp vector width 16 done");
                */
                ret = zeKernelDestroy(compute_sp_v1);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelDestroy()");
                }
                ret = zeKernelDestroy(compute_sp_v2);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelDestroy()");
                }
                ret = zeKernelDestroy(compute_sp_v4);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelDestroy()");
                }
                ret = zeKernelDestroy(compute_sp_v8);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelDestroy()");
                }
                ret = zeKernelDestroy(compute_sp_v16);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelDestroy()");
                }
                ret = zeMemFree(context, device_input_value);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemFree()");
                }
                ret = zeMemFree(context, device_output_buffer);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemFree()");
                }
                ret = zeModuleDestroy(module_handle);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeModuleDestroy()");
                }
                ret = zeContextDestroy(context);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeContextDestroy()");
                }
            } catch (BaseException &e) {
                XPUM_LOG_DEBUG("Error in computation diagnostic");
                XPUM_LOG_DEBUG(e.what());
                all_gflops[i] = -1;
                error_messages[i] = e.what();
            } catch (...) {
                XPUM_LOG_DEBUG("Error in computation diagnostic");
                all_gflops[i] = -1;
            }
        }));
    }

    for (std::size_t i = 0; i < compute_threads.size(); i++) {
        compute_threads[i].join();
    }

    long double all_gflops_value = 0;
    for (size_t i = 0; i < all_gflops.size(); i++) {
        if (all_gflops[i] < 0) {
            if (error_messages[i].length() == 0)
                throw BaseException("unknown reasons");
            else
                throw BaseException(error_messages[i]);
        }
        XPUM_LOG_DEBUG("single precision compute: {} GFLOPS", all_gflops[i]);
        all_gflops_value += all_gflops[i];
    }

    std::string compute_detail = "Its single-precision GFLOPS is " + roundDouble(all_gflops_value, 3) + ".";
    auto gflops_threshold = 0;
    if (device_names.find(ze_device) != device_names.end()) {
        gflops_threshold = thresholds[device_names[ze_device]]["SINGLE_PRECISION_MIN_GFLOPS"];
    }
    if (gflops_threshold <= 0) {
        compute_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        std::string desc = "Fail to check computation performance.";
        desc += " " + compute_detail;
        desc += "  Unconfigured or invalid threshold.";
        updateMessage(compute_component.message, desc);
    } else if (all_gflops_value < gflops_threshold) {
        compute_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        std::string desc = "Fail to check computation performance.";
        desc += " " + compute_detail;
        desc += " Threshold is " + std::to_string(gflops_threshold) + " GFLOPS.";
        updateMessage(compute_component.message, desc);
    } else {
        compute_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
        if (checkOnly == true) {
            updateMessage(compute_component.message, 
                    "Pass to check computation");
        } else {
            std::string desc = "Pass to check computation performance.";
            desc += " " + compute_detail;
            updateMessage(compute_component.message, desc);
        }
    }
    compute_component.finished = true;
}

void DiagnosticManager::doDeviceDiagnosticPeformancePower(const ze_device_handle_t &ze_device, const ze_driver_handle_t &ze_driver, std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    std::atomic<bool> computation_done(false);
    xpum_diag_component_info_t &power_component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_PERFORMANCE_POWER];
    p_task_info->count += 1;
    updateMessage(power_component.message, std::string("Running"));
    power_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;

    ze_result_t ret;
    std::vector<ze_device_handle_t> device_handles;
    std::vector<long double> all_gflops;
    std::vector<std::thread> compute_threads;
    std::vector<std::string> error_messages;

    uint32_t subdevice_count = 0;
    ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, nullptr);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeDeviceGetSubDevices()");
    }
    if (subdevice_count == 0) {
        device_handles.push_back(ze_device);
        all_gflops.push_back(0);
        error_messages.push_back("");
    } else {
        std::vector<ze_device_handle_t> subdevices(subdevice_count);
        ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, subdevices.data());
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeDeviceGetSubDevices()");
        }
        for (auto &subdevice : subdevices) {
            device_handles.push_back(subdevice);
            all_gflops.push_back(0);
            error_messages.push_back("");
        }
    }

    int power_value = 0;
    zes_device_handle_t device = (zes_device_handle_t)ze_device;
    std::thread read_power_thread = std::thread([&computation_done, &power_value, device]() {
        while (!computation_done.load()) {
            try {
                auto current_device_value = 0;
                auto current_sub_device_value_sum = 0;
                ze_result_t res;
                uint32_t power_domain_count = 0;
                XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr));
                std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
                XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data()));
                if (res == ZE_RESULT_SUCCESS) {
                    for (auto &power : power_handles) {
                        zes_power_properties_t props = {};
                        props.stype = ZES_STRUCTURE_TYPE_POWER_PROPERTIES;
                        props.pNext = nullptr;
                        XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetProperties(power, &props));
                        if (res != ZE_RESULT_SUCCESS) {
                            continue;
                        }
                        zes_power_energy_counter_t snap1, snap2;
                        XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetEnergyCounter(power, &snap1));
                        if (res == ZE_RESULT_SUCCESS) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE * 2));
                            XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetEnergyCounter(power, &snap2));
                            if (res == ZE_RESULT_SUCCESS) {
                                int value = std::ceil((snap2.energy - snap1.energy) * 1.0 / (snap2.timestamp - snap1.timestamp));
                                if (!props.onSubdevice) {
                                    current_device_value = value;
                                } else {
                                    current_sub_device_value_sum += value;
                                }
                            }
                        }
                    }
                }
                XPUM_LOG_DEBUG("diagnostic: current device power value: {}", current_device_value);
                XPUM_LOG_DEBUG("diagnostic: current sum of sub-device power values: {}", current_sub_device_value_sum);
                auto current_value = std::max(current_device_value, current_sub_device_value_sum);
                if (current_value > power_value) {
                    power_value = current_value;
                    XPUM_LOG_DEBUG("update peak power value: {}", power_value);
                }
            } catch (...) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        }
    });

    for (std::size_t i = 0; i < device_handles.size(); i++) {
        compute_threads.push_back(std::thread([&all_gflops, &error_messages, i, &device_handles, &ze_driver]() {
            try {
                ze_result_t ret;
                long double timed;
                std::size_t flops_per_work_item = 2048;
                struct ZeWorkGroups workgroup_info;
                int input_value = 4;

                ze_device_properties_t device_properties;
                device_properties.pNext = nullptr;
                device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeDeviceGetProperties(device_handles[i], &device_properties));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeDeviceGetProperties()");
                }

                ze_device_compute_properties_t device_compute_properties;
                device_compute_properties.pNext = nullptr;
                device_compute_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeDeviceGetComputeProperties(device_handles[i], &device_compute_properties));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeDeviceGetComputeProperties()");
                }
                ze_context_handle_t context;
                ze_context_desc_t context_desc = {
                        ZE_STRUCTURE_TYPE_CONTEXT_DESC,
                        nullptr, 
                        0
                };
                XPUM_ZE_HANDLE_LOCK(ze_driver, ret = zeContextCreate(ze_driver, &context_desc, &context));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeContextCreate()");
                }
                ze_module_handle_t module_handle;
                ze_module_desc_t module_description = {};
                std::vector<uint8_t> binary_file = loadBinaryFile("ze_int_compute.spv");
                module_description.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;
                module_description.pNext = nullptr;
                module_description.format = ZE_MODULE_FORMAT_IL_SPIRV;
                module_description.inputSize = static_cast<uint32_t>(binary_file.size());
                module_description.pInputModule = binary_file.data();
                module_description.pBuildFlags = nullptr;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeModuleCreate(context, device_handles[i], &module_description, &module_handle, nullptr));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeModuleCreate()");
                }
                uint64_t max_work_items = device_properties.numSlices *
                                          device_properties.numSubslicesPerSlice *
                                          device_properties.numEUsPerSubslice *
                                          device_compute_properties.maxGroupCountX * 2048;

                uint64_t max_number_of_allocated_items = device_properties.maxMemAllocSize / sizeof(int);
                uint64_t number_of_work_items = std::min(max_number_of_allocated_items, (max_work_items * sizeof(int)));
                number_of_work_items = setWorkgroups(device_compute_properties, number_of_work_items, &workgroup_info);

                void *device_input_value;
                ze_device_mem_alloc_desc_t in_device_desc = {};
                in_device_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
                in_device_desc.pNext = nullptr;
                in_device_desc.ordinal = 0;
                in_device_desc.flags = 0;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeMemAllocDevice(context, &in_device_desc, sizeof(int), 1, device_handles[i], &device_input_value));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemAllocDevice()");
                }
                void *device_output_buffer;
                ze_device_mem_alloc_desc_t out_device_desc = {};
                out_device_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
                out_device_desc.pNext = nullptr;
                out_device_desc.ordinal = 0;
                out_device_desc.flags = 0;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeMemAllocDevice(context, &out_device_desc, static_cast<std::size_t>((number_of_work_items * sizeof(int))),
                                                                              1, device_handles[i], &device_output_buffer));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemAllocDevice()");
                }
                ze_command_list_handle_t command_list;
                ze_command_list_desc_t command_list_description{};
                command_list_description.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
                command_list_description.pNext = nullptr;
                command_list_description.flags = ZE_COMMAND_LIST_FLAG_EXPLICIT_ONLY;
                command_list_description.commandQueueGroupOrdinal = 0;

                ze_command_queue_handle_t command_queue;
                ze_command_queue_desc_t command_queue_description{};
                command_queue_description.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
                command_queue_description.pNext = nullptr;
                command_queue_description.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
                command_queue_description.flags = ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY;
                command_queue_description.ordinal = 0;
                command_queue_description.index = 0;

                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeCommandListCreate(context, device_handles[i], &command_list_description, &command_list));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListCreate()");
                }
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeCommandQueueCreate(context, device_handles[i], &command_queue_description, &command_queue));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandQueueCreate()");
                }
                ret = zeCommandListAppendMemoryCopy(command_list, device_input_value, &input_value, sizeof(int), nullptr, 0, nullptr);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListAppendMemoryCopy()");
                }
                ret = zeCommandListAppendBarrier(command_list, nullptr, 0, nullptr);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListAppendBarrier()");
                }
                ret = zeCommandListClose(command_list);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListClose()");
                }
                ret = zeCommandQueueExecuteCommandLists(command_queue, 1, &command_list, nullptr);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandQueueExecuteCommandLists()");
                }
                waitForCommandQueueSynchronize(command_queue, "zeCommandQueueSynchronize()");
                ret = zeCommandListReset(command_list);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListReset()");
                }
                ze_kernel_handle_t compute_int_v1;
                setupFunction(module_handle, compute_int_v1, "compute_int_v1", device_input_value, device_output_buffer);

                timed = 0;
                long double current;
                timed = runKernel(command_queue, command_list, compute_int_v1, workgroup_info, XPUM_DIAG_PERFORMANCE_POWER);
                current = calculateGbps(timed, number_of_work_items * flops_per_work_item);
                all_gflops[i] = std::max(all_gflops[i], current);
                XPUM_LOG_INFO("compute int vector width 1 done");

                ret = zeKernelDestroy(compute_int_v1);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelDestroy()");
                }
                ret = zeMemFree(context, device_input_value);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemFree()");
                }
                ret = zeMemFree(context, device_output_buffer);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemFree()");
                }
                ret = zeModuleDestroy(module_handle);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeModuleDestroy()");
                }
                ret = zeContextDestroy(context);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeContextDestroy()");
                }
            } catch (BaseException &e) {
                XPUM_LOG_DEBUG("Error in power diagnostic");
                XPUM_LOG_DEBUG(e.what());
                all_gflops[i] = -1;
                error_messages[i] = e.what();
            } catch (...) {
                XPUM_LOG_DEBUG("Error in power diagnostic");
                all_gflops[i] = -1;
            }
        }));
    }

    for (std::size_t i = 0; i < compute_threads.size(); i++) {
        compute_threads[i].join();
    }
    computation_done.store(true);
    read_power_thread.join();

    std::string power_detail = "Its stress power is " + std::to_string(power_value) + " W.";
    auto power_threshold = 0;
    if (device_names.find(ze_device) != device_names.end()) {
        power_threshold = thresholds[device_names[ze_device]]["POWER_MIN_STRESS_WATT"];
    }

    if (power_threshold <= 0) {
        power_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        std::string desc = "Fail to check stress power.";
        desc += " " + power_detail;
        desc += "  Unconfigured or invalid threshold.";
        updateMessage(power_component.message, desc);
    } else if (power_value < power_threshold) {
        power_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        std::string desc = "Fail to check stress power.";
        desc += " " + power_detail;
        desc += " Threshold is " + std::to_string(power_threshold) + " W.";
        updateMessage(power_component.message, desc);
    } else {
        power_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
        std::string desc = "Pass to check stress power.";
        desc += " " + power_detail;
        updateMessage(power_component.message, desc);
    }
    power_component.finished = true;
}

uint64_t DiagnosticManager::setWorkgroups(ze_device_compute_properties_t &device_compute_properties,
                                          const uint64_t total_work_items_requested,
                                          struct ZeWorkGroups *workgroup_info) {
    auto group_size_x = std::min(total_work_items_requested, uint64_t(device_compute_properties.maxGroupSizeX));
    auto group_size_y = 1;
    auto group_size_z = 1;
    auto group_size = group_size_x * group_size_y * group_size_z;

    auto group_count_x = total_work_items_requested / group_size;
    group_count_x = std::min(group_count_x, uint64_t(device_compute_properties.maxGroupCountX));
    auto remaining_items = total_work_items_requested - group_count_x * group_size;

    uint64_t group_count_y = remaining_items / (group_count_x * group_size);
    group_count_y = std::min(group_count_y, uint64_t(device_compute_properties.maxGroupCountY));
    group_count_y = std::max(group_count_y, uint64_t(1));
    remaining_items = total_work_items_requested - group_count_x * group_count_y * group_size;

    uint64_t group_count_z = remaining_items / (group_count_x * group_count_y * group_size);
    group_count_z = std::min(group_count_z, uint64_t(device_compute_properties.maxGroupCountZ));
    group_count_z = std::max(group_count_z, uint64_t(1));

    auto final_work_items = group_count_x * group_count_y * group_count_z * group_size;
    remaining_items = total_work_items_requested - final_work_items;

    workgroup_info->group_size_x = static_cast<uint32_t>(group_size_x);
    workgroup_info->group_size_y = static_cast<uint32_t>(group_size_y);
    workgroup_info->group_size_z = static_cast<uint32_t>(group_size_z);
    workgroup_info->group_count_x = static_cast<uint32_t>(group_count_x);
    workgroup_info->group_count_y = static_cast<uint32_t>(group_count_y);
    workgroup_info->group_count_z = static_cast<uint32_t>(group_count_z);

    return final_work_items;
}

void DiagnosticManager::setupFunction(ze_module_handle_t &module_handle, ze_kernel_handle_t &function,
                                      const char *name, void *input, void *output) {
    ze_result_t ret;
    ze_kernel_desc_t function_description = {};
    function_description.stype = ZE_STRUCTURE_TYPE_KERNEL_DESC;
    function_description.pNext = nullptr;
    function_description.flags = 0;
    function_description.pKernelName = name;
    ret = zeKernelCreate(module_handle, &function_description, &function);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeKernelCreate()");
    }
    ret = zeKernelSetArgumentValue(function, 0, sizeof(input), &input);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeKernelSetArgumentValue()");
    }
    ret = zeKernelSetArgumentValue(function, 1, sizeof(output), &output);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeKernelSetArgumentValue()");
    }
}

long double DiagnosticManager::runKernel(ze_command_queue_handle_t command_queue, ze_command_list_handle_t command_list,
                                         ze_kernel_handle_t &function,
                                         struct ZeWorkGroups &workgroup_info, xpum_diag_task_type_t type, bool checkOnly) {
    ze_result_t ret;
    long double timed = 0;

    ret = zeKernelSetGroupSize(function, workgroup_info.group_size_x, workgroup_info.group_size_y, workgroup_info.group_size_z);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeKernelSetGroupSize()");
    }
    ze_group_count_t thread_group_dimensions;
    thread_group_dimensions.groupCountX = workgroup_info.group_count_x;
    thread_group_dimensions.groupCountY = workgroup_info.group_count_y;
    thread_group_dimensions.groupCountZ = workgroup_info.group_count_z;
    ret = zeCommandListAppendLaunchKernel(command_list, function, &thread_group_dimensions, nullptr, 0, nullptr);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeCommandListAppendLaunchKernel()");
    }
    ret = zeCommandListClose(command_list);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeCommandListClose()");
    }

    // 1 round is good enough if it is not perf diag
    if (checkOnly == true) {
        ret = zeCommandQueueExecuteCommandLists(command_queue, 1, &command_list, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandQueueExecuteCommandLists()");
        }
        waitForCommandQueueSynchronize(command_queue, "zeCommandQueueSynchronize()");
        return 0;
    }

    for (uint32_t i = 0; i < 10; i++) {
        ret = zeCommandQueueExecuteCommandLists(command_queue, 1, &command_list, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandQueueExecuteCommandLists()");
        }
    }
    waitForCommandQueueSynchronize(command_queue, "zeCommandQueueSynchronize()");

    std::chrono::high_resolution_clock::time_point time_start, time_end;
    time_start = std::chrono::high_resolution_clock::now();
    for (uint32_t i = 0; i < 50; i++) {
        ret = zeCommandQueueExecuteCommandLists(command_queue, 1, &command_list, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandQueueExecuteCommandLists()");
        }
    }

    if (type == XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH) {
        ret = zeCommandQueueSynchronize(command_queue, UINT64_MAX);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandQueueSynchronize()");
        }
    } else {
        waitForCommandQueueSynchronize(command_queue, "zeCommandQueueSynchronize()");
    }
    time_end = std::chrono::high_resolution_clock::now();
    timed = std::chrono::duration<long double, std::chrono::nanoseconds::period>(time_end - time_start).count();
    ret = zeCommandListReset(command_list);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeCommandListReset()");
    }
    return (timed / static_cast<long double>(50));
}

long double DiagnosticManager::calculateGbps(long double period, long double total_gbps) {
    return total_gbps / period / 1.0;
}

void DiagnosticManager::updateMessage(char *arr, std::string str) {
    int index = 0;
    while (index < (int)str.size() && index < XPUM_MAX_STR_LENGTH - 1) {
        arr[index] = str[index];
        index++;
    }
    arr[index] = 0;
}

std::string DiagnosticManager::roundDouble(double r, int precision) {
    std::stringstream buffer;
    buffer << std::fixed << std::setprecision(precision) << r;
    return buffer.str();
}

void DiagnosticManager::waitForCommandQueueSynchronize(ze_command_queue_handle_t command_queue, std::string info) {
    ze_result_t ret;
    int command_queue_synchronize_maximum_round = ZE_COMMAND_QUEUE_SYNCHRONIZE_TIMEOUT;
    int command_queue_synchronize_step_duration = 1; //seconds
    ret = zeCommandQueueSynchronize(command_queue, 100 * 1000);
    int current_round = 0;
    while (ret == ZE_RESULT_NOT_READY && current_round < command_queue_synchronize_maximum_round) {
        std::this_thread::sleep_for(std::chrono::seconds(command_queue_synchronize_step_duration));
        ret = zeCommandQueueSynchronize(command_queue, 0);
        current_round += 1;
    }
    if (ret == ZE_RESULT_NOT_READY)
        throw BaseException(info + std::string(" timeout"));
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException(info);
    }
}

void DiagnosticManager::doDeviceDiagnosticPeformanceMemoryBandwidth(const ze_device_handle_t &ze_device,
                                                                    const ze_driver_handle_t &ze_driver,
                                                                    std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    xpum_diag_component_info_t &memorybandwidth_component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH];
    p_task_info->count += 1;
    updateMessage(memorybandwidth_component.message, std::string("Running"));
    memorybandwidth_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;

    ze_result_t ret;
    std::vector<ze_device_handle_t> device_handles;
    std::vector<long double> all_gbps;
    std::vector<std::thread> memorybandwidth_threads;
    std::vector<std::string> error_messages;
    uint32_t subdevice_count = 0;
    ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, nullptr);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeDeviceGetSubDevices()");
    }

    if (subdevice_count == 0) {
        device_handles.push_back(ze_device);
        all_gbps.push_back(0);
        error_messages.push_back("");
    } else {
        std::vector<ze_device_handle_t> subdevices(subdevice_count);
        ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, subdevices.data());
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeDeviceGetSubDevices()");
        }
        for (auto &subdevice : subdevices) {
            device_handles.push_back(subdevice);
            all_gbps.push_back(0);
            error_messages.push_back("");
        }
    }
    for (std::size_t i = 0; i < device_handles.size(); i++) {
        memorybandwidth_threads.push_back(std::thread([&all_gbps, &error_messages, i, &device_handles, &ze_driver]() {
            try {
                ze_result_t ret;
                long double timed_lo, timed_go, timed, gbps;
                struct ZeWorkGroups workgroup_info;
                uint64_t temp_global_size;
                ze_device_properties_t device_properties;
                device_properties.pNext = nullptr;
                device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeDeviceGetProperties(device_handles[i], &device_properties));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeDeviceGetProperties()");
                }
                ze_device_compute_properties_t device_compute_properties;
                device_compute_properties.pNext = nullptr;
                device_compute_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeDeviceGetComputeProperties(device_handles[i], &device_compute_properties));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeDeviceGetComputeProperties()");
                }
                ze_context_handle_t context;
                ze_context_desc_t context_desc = {
                        ZE_STRUCTURE_TYPE_CONTEXT_DESC,
                        nullptr, 
                        0
                };
                XPUM_ZE_HANDLE_LOCK(ze_driver, ret = zeContextCreate(ze_driver, &context_desc, &context));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeContextCreate()");
                }
                ze_module_handle_t module_handle;
                ze_module_desc_t module_description = {};
                std::vector<uint8_t> binary_file = loadBinaryFile("ze_global_bw.spv");
                module_description.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;
                module_description.pNext = nullptr;
                module_description.format = ZE_MODULE_FORMAT_IL_SPIRV;
                module_description.inputSize = static_cast<uint32_t>(binary_file.size());
                module_description.pInputModule = binary_file.data();
                module_description.pBuildFlags = nullptr;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeModuleCreate(context, device_handles[i], &module_description, &module_handle, nullptr));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeModuleCreate()");
                }
                uint64_t max_items = device_properties.maxMemAllocSize / sizeof(float) / 2;
                uint64_t num_items = std::min(max_items, (uint64_t)(1 << 29));
                uint64_t base = device_compute_properties.maxGroupSizeX * 16 * 16;
                num_items = (num_items / base) * base;

                std::vector<float> arr(static_cast<uint32_t>(num_items));
                for (uint32_t i = 0; i < num_items; i++) {
                    arr[i] = static_cast<float>(i);
                }

                num_items = setWorkgroups(device_compute_properties, num_items, &workgroup_info);

                void *inputBuf;
                ze_device_mem_alloc_desc_t in_device_desc = {};
                in_device_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
                in_device_desc.pNext = nullptr;
                in_device_desc.ordinal = 0;
                in_device_desc.flags = 0;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeMemAllocDevice(context, &in_device_desc, static_cast<size_t>((num_items * sizeof(float))), 1, device_handles[i], &inputBuf));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemAllocDevice()");
                }
                void *outputBuf;
                ze_device_mem_alloc_desc_t out_device_desc = {};
                out_device_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
                out_device_desc.pNext = nullptr;
                out_device_desc.ordinal = 0;
                out_device_desc.flags = 0;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeMemAllocDevice(context, &out_device_desc, static_cast<size_t>((num_items * sizeof(float))), 1, device_handles[i], &outputBuf));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemAllocDevice()");
                }

                ze_command_list_handle_t command_list;
                ze_command_list_desc_t command_list_description{};
                command_list_description.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
                command_list_description.pNext = nullptr;
                command_list_description.flags = ZE_COMMAND_LIST_FLAG_EXPLICIT_ONLY;
                command_list_description.commandQueueGroupOrdinal = 0;

                ze_command_queue_handle_t command_queue;
                ze_command_queue_desc_t command_queue_description{};
                command_queue_description.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
                command_queue_description.pNext = nullptr;
                command_queue_description.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
                command_queue_description.flags = ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY;
                command_queue_description.ordinal = 0;
                command_queue_description.index = 0;

                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeCommandListCreate(context, device_handles[i], &command_list_description, &command_list));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListCreate()");
                }
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeCommandQueueCreate(context, device_handles[i], &command_queue_description, &command_queue));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandQueueCreate()");
                }
                ret = zeCommandListAppendMemoryCopy(command_list, inputBuf, arr.data(), (arr.size() * sizeof(float)), nullptr, 0, nullptr);
                ret = zeCommandListAppendBarrier(command_list, nullptr, 0, nullptr);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListAppendBarrier()");
                }
                ret = zeCommandListClose(command_list);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListClose()");
                }
                ret = zeCommandQueueExecuteCommandLists(command_queue, 1, &command_list, nullptr);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandQueueExecuteCommandLists()");
                }
                waitForCommandQueueSynchronize(command_queue, "zeCommandQueueSynchronize()");
                ret = zeCommandListReset(command_list);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListReset()");
                }
                ze_kernel_handle_t local_offset_v1;
                ze_kernel_handle_t local_offset_v2;
                ze_kernel_handle_t local_offset_v4;
                ze_kernel_handle_t local_offset_v8;
                ze_kernel_handle_t local_offset_v16;
                ze_kernel_handle_t global_offset_v1;
                ze_kernel_handle_t global_offset_v2;
                ze_kernel_handle_t global_offset_v4;
                ze_kernel_handle_t global_offset_v8;
                ze_kernel_handle_t global_offset_v16;

                setupFunction(module_handle, local_offset_v1, "global_bandwidth_v1_local_offset", inputBuf, outputBuf);
                setupFunction(module_handle, global_offset_v1, "global_bandwidth_v1_global_offset", inputBuf, outputBuf);
                setupFunction(module_handle, local_offset_v2, "global_bandwidth_v2_local_offset", inputBuf, outputBuf);
                setupFunction(module_handle, global_offset_v2, "global_bandwidth_v2_global_offset", inputBuf, outputBuf);
                setupFunction(module_handle, local_offset_v4, "global_bandwidth_v4_local_offset", inputBuf, outputBuf);
                setupFunction(module_handle, global_offset_v4, "global_bandwidth_v4_global_offset", inputBuf, outputBuf);
                setupFunction(module_handle, local_offset_v8, "global_bandwidth_v8_local_offset", inputBuf, outputBuf);
                setupFunction(module_handle, global_offset_v8, "global_bandwidth_v8_global_offset", inputBuf, outputBuf);
                setupFunction(module_handle, local_offset_v16, "global_bandwidth_v16_local_offset", inputBuf, outputBuf);
                setupFunction(module_handle, global_offset_v16, "global_bandwidth_v16_global_offset", inputBuf, outputBuf);

                timed = 0;
                timed_lo = 0;
                timed_go = 0;
                temp_global_size = (num_items / 16);
                setWorkgroups(device_compute_properties, temp_global_size, &workgroup_info);
                timed_lo = runKernel(command_queue, command_list, local_offset_v1, workgroup_info, XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH);
                timed_go = runKernel(command_queue, command_list, global_offset_v1, workgroup_info, XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH);
                timed = (timed_lo < timed_go) ? timed_lo : timed_go;
                gbps = calculateGbps(timed, num_items * sizeof(float));
                all_gbps[i] = std::max(all_gbps[i], gbps);

                timed = 0;
                timed_lo = 0;
                timed_go = 0;
                temp_global_size = (num_items / 2 / 16);
                setWorkgroups(device_compute_properties, temp_global_size, &workgroup_info);
                timed_lo = runKernel(command_queue, command_list, local_offset_v2, workgroup_info, XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH);
                timed_go = runKernel(command_queue, command_list, global_offset_v2, workgroup_info, XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH);
                timed = (timed_lo < timed_go) ? timed_lo : timed_go;
                gbps = calculateGbps(timed, num_items * sizeof(float));
                all_gbps[i] = std::max(all_gbps[i], gbps);

                timed = 0;
                timed_lo = 0;
                timed_go = 0;
                temp_global_size = (num_items / 4 / 16);
                setWorkgroups(device_compute_properties, temp_global_size, &workgroup_info);
                timed_lo = runKernel(command_queue, command_list, local_offset_v4, workgroup_info, XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH);
                timed_go = runKernel(command_queue, command_list, global_offset_v4, workgroup_info, XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH);
                timed = (timed_lo < timed_go) ? timed_lo : timed_go;
                gbps = calculateGbps(timed, num_items * sizeof(float));
                all_gbps[i] = std::max(all_gbps[i], gbps);

                timed = 0;
                timed_lo = 0;
                timed_go = 0;
                temp_global_size = (num_items / 8 / 16);
                setWorkgroups(device_compute_properties, temp_global_size, &workgroup_info);
                timed_lo = runKernel(command_queue, command_list, local_offset_v8, workgroup_info, XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH);
                timed_go = runKernel(command_queue, command_list, global_offset_v8, workgroup_info, XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH);
                timed = (timed_lo < timed_go) ? timed_lo : timed_go;
                gbps = calculateGbps(timed, num_items * sizeof(float));
                all_gbps[i] = std::max(all_gbps[i], gbps);

                timed = 0;
                timed_lo = 0;
                timed_go = 0;
                temp_global_size = (num_items / 16 / 16);
                setWorkgroups(device_compute_properties, temp_global_size, &workgroup_info);
                timed_lo = runKernel(command_queue, command_list, local_offset_v16, workgroup_info, XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH);
                timed_go = runKernel(command_queue, command_list, global_offset_v16, workgroup_info, XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH);
                timed = (timed_lo < timed_go) ? timed_lo : timed_go;
                gbps = calculateGbps(timed, num_items * sizeof(float));
                all_gbps[i] = std::max(all_gbps[i], gbps);

                ret = zeKernelDestroy(local_offset_v1);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelDestroy()");
                }
                ret = zeKernelDestroy(global_offset_v1);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelDestroy()");
                }
                ret = zeKernelDestroy(local_offset_v2);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelDestroy()");
                }
                ret = zeKernelDestroy(global_offset_v2);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelDestroy()");
                }
                ret = zeKernelDestroy(local_offset_v4);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelDestroy()");
                }
                ret = zeKernelDestroy(global_offset_v4);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelDestroy()");
                }
                ret = zeKernelDestroy(local_offset_v8);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelDestroy()");
                }
                ret = zeKernelDestroy(global_offset_v8);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelDestroy()");
                }
                ret = zeKernelDestroy(local_offset_v16);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelDestroy()");
                }
                ret = zeKernelDestroy(global_offset_v16);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelDestroy()");
                }
                ret = zeMemFree(context, inputBuf);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemFree()");
                }
                ret = zeMemFree(context, outputBuf);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemFree()");
                }
                ret = zeModuleDestroy(module_handle);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeModuleDestroy()");
                }
                ret = zeContextDestroy(context);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeContextDestroy()");
                }
            } catch (BaseException &e) {
                XPUM_LOG_DEBUG("Error in memory bandwidth diagnostic");
                all_gbps[i] = -1;
                error_messages[i] = e.what();
            } catch (...) {
                XPUM_LOG_DEBUG("Error in memory bandwidth diagnostic");
                all_gbps[i] = -1;
            }
        }));
    }
    for (std::size_t i = 0; i < memorybandwidth_threads.size(); i++) {
        memorybandwidth_threads[i].join();
    }
    long double all_gbps_value = 0;
    for (std::size_t i = 0; i < all_gbps.size(); i++) {
        if (all_gbps[i] < 0) {
            if (error_messages[i].length() == 0)
                throw BaseException("unknown reasons");
            else
                throw BaseException(error_messages[i]);
        }
        XPUM_LOG_DEBUG("memory bandwidth: {} GBPS", all_gbps[i]);
        all_gbps_value += all_gbps[i];
    }

    std::string memorybandwidth_detail = "Its memory bandwidth is " + roundDouble(all_gbps_value, 3) + " GBPS.";
    auto memorybandwidth_threshold = 0;
    if (device_names.find(ze_device) != device_names.end()) {
        memorybandwidth_threshold = thresholds[device_names[ze_device]]["MEMORY_BANDWIDTH_MIN_GBPS"];
    }

    if (memorybandwidth_threshold <= 0) {
        memorybandwidth_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        std::string desc = "Fail to check memory bandwidth.";
        desc += " " + memorybandwidth_detail;
        desc += "  Unconfigured or invalid threshold.";
        updateMessage(memorybandwidth_component.message, desc);
    } else if (all_gbps_value < memorybandwidth_threshold) {
        memorybandwidth_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        std::string desc = "Fail to check memory bandwidth.";
        desc += " " + memorybandwidth_detail;
        desc += " Threshold is " + std::to_string(memorybandwidth_threshold) + " GBPS.";
        updateMessage(memorybandwidth_component.message, desc);
    } else {
        memorybandwidth_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
        std::string desc = "Pass to check memory bandwidth.";
        desc += " " + memorybandwidth_detail;
        updateMessage(memorybandwidth_component.message, desc);
    }
    memorybandwidth_component.finished = true;
}

void DiagnosticManager::stressThreadFunc(int stress_time,
                                          const ze_device_handle_t &ze_device,
                                          const ze_driver_handle_t &ze_driver,
                                          std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    ze_result_t ret;
    uint32_t subdevice_count = 0;
    std::vector<ze_device_handle_t> device_handles;
    std::vector<std::thread> compute_threads;
    ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, nullptr);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeDeviceGetSubDevices()");
    }
    if (subdevice_count == 0) {
        device_handles.push_back(ze_device);
    } else {
        std::vector<ze_device_handle_t> subdevices(subdevice_count);
        ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, subdevices.data());
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeDeviceGetSubDevices()");
        }
        for (auto &subdevice : subdevices) {
            device_handles.push_back(subdevice);
        }
    }

    for (std::size_t i = 0; i < device_handles.size(); i++) {
        compute_threads.push_back(std::thread([stress_time, i, &device_handles, &ze_driver]() {
            try {
                ze_result_t ret;
                struct ZeWorkGroups workgroup_info;
                int input_value = 4;

                ze_device_properties_t device_properties;
                device_properties.pNext = nullptr;
                device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeDeviceGetProperties(device_handles[i], &device_properties));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeDeviceGetProperties()");
                }

                ze_device_compute_properties_t device_compute_properties;
                device_compute_properties.pNext = nullptr;
                device_compute_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeDeviceGetComputeProperties(device_handles[i], &device_compute_properties));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeDeviceGetComputeProperties()");
                }
                ze_context_handle_t context;
                ze_context_desc_t context_desc = {
                        ZE_STRUCTURE_TYPE_CONTEXT_DESC,
                        nullptr, 
                        0
                };
                XPUM_ZE_HANDLE_LOCK(ze_driver, ret = zeContextCreate(ze_driver, &context_desc, &context));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeContextCreate()");
                }
                ze_module_handle_t module_handle;
                ze_module_desc_t module_description = {};
                std::vector<uint8_t> binary_file = loadBinaryFile("ze_int_compute.spv");
                module_description.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;
                module_description.pNext = nullptr;
                module_description.format = ZE_MODULE_FORMAT_IL_SPIRV;
                module_description.inputSize = static_cast<uint32_t>(binary_file.size());
                module_description.pInputModule = binary_file.data();
                module_description.pBuildFlags = nullptr;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeModuleCreate(context, device_handles[i], &module_description, &module_handle, nullptr));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeModuleCreate()");
                }
                uint64_t max_work_items = device_properties.numSlices *
                                          device_properties.numSubslicesPerSlice *
                                          device_properties.numEUsPerSubslice *
                                          device_compute_properties.maxGroupCountX * 2048;

                uint64_t max_number_of_allocated_items = device_properties.maxMemAllocSize / sizeof(int);
                uint64_t number_of_work_items = std::min(max_number_of_allocated_items, (max_work_items * sizeof(int)));
                number_of_work_items = setWorkgroups(device_compute_properties, number_of_work_items, &workgroup_info);

                void *device_input_value;
                ze_device_mem_alloc_desc_t in_device_desc = {};
                in_device_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
                in_device_desc.pNext = nullptr;
                in_device_desc.ordinal = 0;
                in_device_desc.flags = 0;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeMemAllocDevice(context, &in_device_desc, sizeof(int), 1, device_handles[i], &device_input_value));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemAllocDevice()");
                }
                void *device_output_buffer;
                ze_device_mem_alloc_desc_t out_device_desc = {};
                out_device_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
                out_device_desc.pNext = nullptr;
                out_device_desc.ordinal = 0;
                out_device_desc.flags = 0;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeMemAllocDevice(context, &out_device_desc, static_cast<std::size_t>((number_of_work_items * sizeof(int))),
                                                                              1, device_handles[i], &device_output_buffer));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemAllocDevice()");
                }
                ze_command_list_handle_t command_list;
                ze_command_list_desc_t command_list_description{};
                command_list_description.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
                command_list_description.pNext = nullptr;
                command_list_description.flags = ZE_COMMAND_LIST_FLAG_EXPLICIT_ONLY;
                command_list_description.commandQueueGroupOrdinal = 0;

                ze_command_queue_handle_t command_queue;
                ze_command_queue_desc_t command_queue_description{};
                command_queue_description.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
                command_queue_description.pNext = nullptr;
                command_queue_description.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
                command_queue_description.flags = ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY;
                command_queue_description.ordinal = 0;
                command_queue_description.index = 0;

                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeCommandListCreate(context, device_handles[i], &command_list_description, &command_list));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListCreate()");
                }
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeCommandQueueCreate(context, device_handles[i], &command_queue_description, &command_queue));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandQueueCreate()");
                }
                ret = zeCommandListAppendMemoryCopy(command_list, device_input_value, &input_value, sizeof(int), nullptr, 0, nullptr);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListAppendMemoryCopy()");
                }
                ret = zeCommandListAppendBarrier(command_list, nullptr, 0, nullptr);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListAppendBarrier()");
                }
                ret = zeCommandListClose(command_list);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListClose()");
                }
                ret = zeCommandQueueExecuteCommandLists(command_queue, 1, &command_list, nullptr);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandQueueExecuteCommandLists()");
                }
                waitForCommandQueueSynchronize(command_queue, "zeCommandQueueSynchronize()");
                ret = zeCommandListReset(command_list);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListReset()");
                }
                ze_kernel_handle_t compute_int_v1;
                setupFunction(module_handle, compute_int_v1, "compute_int_v1", device_input_value, device_output_buffer);

                //runKernel stuff
                ret = zeKernelSetGroupSize(compute_int_v1, workgroup_info.group_size_x, workgroup_info.group_size_y, workgroup_info.group_size_z);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelSetGroupSize()");
                }
                ze_group_count_t thread_group_dimensions;
                thread_group_dimensions.groupCountX = workgroup_info.group_count_x;
                thread_group_dimensions.groupCountY = workgroup_info.group_count_y;
                thread_group_dimensions.groupCountZ = workgroup_info.group_count_z;
                ret = zeCommandListAppendLaunchKernel(command_list, compute_int_v1, &thread_group_dimensions, nullptr, 0, nullptr);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListAppendLaunchKernel()");
                }
                ret = zeCommandListClose(command_list);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListClose()");
                }

                #define KERN_TIMES 5
                int workTime = 0;
                while (true) {
                    auto before = std::chrono::system_clock::now();
                    for (int i = 0; i < KERN_TIMES; i++) {
                        ret = zeCommandQueueExecuteCommandLists(command_queue, 1, &command_list, nullptr);
                        if (ret != ZE_RESULT_SUCCESS) {
                            throw BaseException("zeCommandQueueExecuteCommandLists()");
                        }
                        ret = zeCommandQueueSynchronize(command_queue, UINT64_MAX);
                        if (ret != ZE_RESULT_SUCCESS) {
                            throw BaseException("zeCommandQueueSynchronize()");
                        }
                    }
                    auto after = std::chrono::system_clock::now();
                    workTime += std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count();
                    if (stress_time != 0 && stress_time <= workTime / (60 * 1000)) {
                        break;
                    }
                }

                ret = zeCommandListReset(command_list);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeCommandListReset()");
                }
                //end of runKernel

                ret = zeKernelDestroy(compute_int_v1);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeKernelDestroy()");
                }
                ret = zeMemFree(context, device_input_value);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemFree()");
                }
                ret = zeMemFree(context, device_output_buffer);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeMemFree()");
                }
                ret = zeModuleDestroy(module_handle);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeModuleDestroy()");
                }
                ret = zeContextDestroy(context);
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeContextDestroy()");
                }
            } catch (BaseException &e) {
                XPUM_LOG_DEBUG("Error in stress test with BaseException");
                XPUM_LOG_DEBUG(e.what());
            } catch (...) {
                XPUM_LOG_DEBUG("Error in stress test");
            }
        }));
    }

    for (std::size_t i = 0; i < compute_threads.size(); i++) {
        compute_threads[i].join();
    }
    p_task_info->finished = true;
    return;
}

xpum_result_t DiagnosticManager::runStress(xpum_device_id_t deviceId, uint32_t stressTime) {
    std::unique_lock<std::mutex> lock(this->mutex);
    std::vector<std::shared_ptr<Device>> devices;
    if (deviceId == -1) {
        if (stress_task_map.size() > 0) {
            for (auto task : stress_task_map) {
                if (task.second->finished == false) {
                    return XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE;
                }
            }
            stress_task_map.clear();
        }
        this->p_device_manager->getDeviceList(devices);
    } else {
        if (this->p_device_manager->getDevice(std::to_string(deviceId)) == nullptr) {
            return XPUM_RESULT_DEVICE_NOT_FOUND;
        }
        if (stress_task_map.find(deviceId) != stress_task_map.end() && stress_task_map.at(deviceId)->finished == false) {
            return XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE;
        }
        stress_task_map.erase(deviceId);
        devices.push_back(this->p_device_manager->getDevice(std::to_string(deviceId)));
    }

    for (auto device : devices) {
        std::shared_ptr<xpum_diag_task_info_t> p_task_info = std::make_shared<xpum_diag_task_info_t>();
        
        p_task_info->deviceId = std::stoi(device->getId());
        p_task_info->result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;
        p_task_info->finished = false;
        p_task_info->count = 0;
        p_task_info->startTime = Utility::getCurrentMillisecond();
        updateMessage(p_task_info->message, std::string("Doing stress"));
        stress_task_map.insert(std::pair<xpum_device_id_t, std::shared_ptr<xpum_diag_task_info_t>>(p_task_info->deviceId, p_task_info));
        std::thread thread(DiagnosticManager::stressThreadFunc, stressTime,
                           device->getDeviceZeHandle(),
                           device->getDriverHandle(), p_task_info);
        thread.detach();
    }
    return XPUM_OK;
}

xpum_result_t DiagnosticManager::checkStress(xpum_device_id_t deviceId, xpum_diag_task_info_t resultList[], int *count) {
    std::unique_lock<std::mutex> lock(this->mutex);
    if (resultList == nullptr) {
        *count = stress_task_map.size();
        return XPUM_OK;
    }
    if (deviceId == -1) {
        if (*count < (int)stress_task_map.size()) {
            return XPUM_BUFFER_TOO_SMALL;
        }
        int i = 0;
        for (auto task : stress_task_map) {
            resultList[i].deviceId = task.second->deviceId;
            resultList[i].finished = task.second->finished;
            resultList[i].startTime = task.second->startTime;
            resultList[i].endTime = task.second->endTime;
            i++;
        }
        *count = i;

    } else {
        if (*count < 1) {
            return XPUM_BUFFER_TOO_SMALL;
        }
        if (stress_task_map.find(deviceId) == stress_task_map.end()) {
            return XPUM_RESULT_DEVICE_NOT_FOUND;
        }
        resultList[0].deviceId = deviceId;
        resultList[0].finished = stress_task_map.at(deviceId)->finished;
        resultList[0].startTime = stress_task_map.at(deviceId)->startTime;
        resultList[0].endTime = stress_task_map.at(deviceId)->endTime;
        *count = 1;
    }
    return XPUM_OK;
}

} // end namespace xpum
