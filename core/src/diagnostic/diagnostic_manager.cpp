/* 
 *  Copyright (C) 2021-2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file diagnostic_manager.cpp
 */

#include "diagnostic_manager.h"

#include <algorithm>
#include <thread>
#include <tuple>
#include <numeric>
#include <limits>
#include <array>
#include <sstream>

#include "firmware/firmware_manager.h"
#include "device/gpu/gpu_device_stub.h"
#include "device/amcInBand.h"
#include "infrastructure/configuration.h"
#include "infrastructure/handle_lock.h"
#include "infrastructure/logger.h"
#include "infrastructure/xpum_config.h"
#include "infrastructure/utility.h"
#include "api/device_model.h"
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include "helper.h"
#define ALL_GPU_ID -1

namespace xpum {

DiagnosticManager::DiagnosticManager(std::shared_ptr<DeviceManagerInterface> &p_device_manager,
                                     std::shared_ptr<DataLogicInterface> &p_data_logic,
                                     std::shared_ptr<FirmwareManager> &p_firmware_manager)
    : p_device_manager(p_device_manager), p_data_logic(p_data_logic), p_firmware_manager(p_firmware_manager) {
    this->p_device_manager->getDeviceList(this->devices);
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
            XPUM_LOG_DEBUG("device {} device_pci_id {}", (void *)device->getDeviceHandle(), device_pci_id);
        }
    }
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
std::string DiagnosticManager::MEDIA_CODER_TOOLS_PATH = "/usr/bin/";
std::string DiagnosticManager::MEDIA_CODER_TOOLS_1080P_FILE = "test_stream_1080p.265";
std::string DiagnosticManager::MEDIA_CODER_TOOLS_4K_FILE = "test_stream_4K.265";
std::string DiagnosticManager::MEDIA_CODEC_TOOLS_LIGHT_FILE = "test_stream.264";
uint64_t DiagnosticManager::GPU_TEMPERATURE_THRESHOLD = 80;
int DiagnosticManager::ZE_COMMAND_QUEUE_SYNCHRONIZE_TIMEOUT = 600;
float DiagnosticManager::MEMORY_USE_PERCENTAGE_FOR_ERROR_TEST = 0.9;
float DiagnosticManager::XE_LINK_THROUGHPUT_USAGE_PERCENTAGE = 0.7;
int DiagnosticManager::REF_XE_LINK_THROUGHPUT_ONE_TILE_DEVICE = 23;
int DiagnosticManager::REF_XE_LINK_THROUGHPUT_TWO_TILE_DEVICE = 19;
float DiagnosticManager::XE_LINK_ALL_TO_ALL_THROUGHPUT_MIN_RATIO_OF_REF = 0.8;
int DiagnosticManager::REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X2_ONE_TILE_DEVICE = 117;
int DiagnosticManager::REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X4_ONE_TILE_DEVICE = 55;
int DiagnosticManager::REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X8_ONE_TILE_DEVICE = 51;
int DiagnosticManager::REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X2_TWO_TILE_DEVICE = 303;
int DiagnosticManager::REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X4_TWO_TILE_DEVICE = 116;
int DiagnosticManager::REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X8_TWO_TILE_DEVICE = 67;
const std::string DiagnosticManager::COMPONENT_TYPE_NOT_SUPPORTED = "Not supported";
const std::string DiagnosticManager::COMPONENT_TYPE_NOT_SUPPORTED_ON_PF = "Not supported on physical functions.";
std::map<uint32_t, int32_t> DiagnosticManager::fabric_id_convert_to_device_id;
std::map<int32_t, std::set<int32_t>> DiagnosticManager::device_id_link_to_device_ids;
std::string DiagnosticManager::PVC_FW_MINIMUM_VERSION = "PVC2_1.23423";
std::string DiagnosticManager::PVC_AMC_MINIMUM_VERSION = "6.7.0.0";
std::string DiagnosticManager::ATSM150_FW_MINIMUM_VERSION = "DG02_1.3271";
std::string DiagnosticManager::ATSM75_FW_MINIMUM_VERSION = "DG02_2.2277";

void readTemperatureTask(std::atomic<bool>& subtask_done, uint64_t& max_temperature_value, const ze_device_handle_t& ze_device, const zes_device_handle_t& zes_device) {
    while (!subtask_done.load()) {
        try {
            std::shared_ptr<MeasurementData> temp = GPUDeviceStub::toGetTemperature(ze_device, zes_device, zes_temp_sensors_t::ZES_TEMP_SENSORS_GPU);
            uint64_t current_temperature_value = 0;
            if (temp->hasDataOnDevice()) {
                current_temperature_value = temp->getCurrent() / Configuration::DEFAULT_MEASUREMENT_DATA_SCALE;
            } else if (temp->hasSubdeviceData()) {
                current_temperature_value = temp->getSubdeviceDataCurrent(0) / Configuration::DEFAULT_MEASUREMENT_DATA_SCALE;
            }
            if (current_temperature_value > 0) {
                XPUM_LOG_DEBUG("diagnostic: current temperature value: {}", current_temperature_value);
                if (current_temperature_value > max_temperature_value) {
                    max_temperature_value = current_temperature_value;
                    XPUM_LOG_DEBUG("diagnostic: update max temperature value: {}", max_temperature_value);
                }
            }
        } catch (...) {
            XPUM_LOG_ERROR("Failed to get gpu temperature");
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

xpum_result_t DiagnosticManager::runDiagnosticsCore(xpum_device_id_t deviceId, xpum_diag_level_t level, xpum_diag_task_type_t types[], int count) {
    std::unique_lock<std::mutex> lock(this->mutex);

    bool isPVCPlatform = false;
    if (devices.empty())
        return XPUM_RESULT_DEVICE_NOT_FOUND;

    for (auto device : devices) {
        if (device->getDeviceModel() == XPUM_DEVICE_MODEL_PVC) {
            isPVCPlatform = true;
            break;
        }
    }

    if (deviceId != ALL_GPU_ID) {
        if (diagnostic_task_infos.find(deviceId) != diagnostic_task_infos.end() && diagnostic_task_infos.at(deviceId)->finished == false) {
            return XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE;
        }
        if (stress_task_map.find(deviceId) != stress_task_map.end() &&
                stress_task_map.at(deviceId)->finished == false) {
            return XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE;
        }

        diagnostic_task_infos.erase(deviceId);
        std::shared_ptr<xpum_diag_task_info_t> p_task_info = std::make_shared<xpum_diag_task_info_t>();
        initDiagTaskInfo(p_task_info, deviceId, level, count, types, isPVCPlatform);
        diagnostic_task_infos[deviceId] = p_task_info;
    } else {
        for (auto diagnostic_task_info : diagnostic_task_infos)
            if (diagnostic_task_info.second->finished == false)
                return XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE;

        for (auto stress_task : stress_task_map)
            if (stress_task.second->finished == false)
                return XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE;
        
        diagnostic_task_infos.clear();
        for (auto device : devices) {
            std::shared_ptr<xpum_diag_task_info_t> p_task_info = std::make_shared<xpum_diag_task_info_t>();
            initDiagTaskInfo(p_task_info, std::stoi(device->getId()), level, count, types, isPVCPlatform);
            diagnostic_task_infos[std::stoi(device->getId())] = p_task_info;     
        }
    }

    try {
        readConfigFile(XPUM_GLOBAL_CONFIG_FILE);
        readConfigFile(DIAG_CONFIG_THRESHOLD_CONIG_FILE);
    } catch (BaseException &e) {
        XPUM_LOG_DEBUG("fail to read xpum.conf and diagnostics.conf");
    }

    std::thread task(&DiagnosticManager::doDiagnosticCore, this, deviceId);
    task.detach();
    return XPUM_OK;
}

void DiagnosticManager::initDiagTaskInfo(std::shared_ptr<xpum::xpum_diag_task_info_t> &p_task_info, xpum::xpum_device_id_t deviceId, xpum::xpum_diag_level_t level, int count, xpum::xpum_diag_task_type_t types[], bool isPVCPlatform) {
    p_task_info->deviceId = deviceId;
    p_task_info->level = level;
    if (level != XPUM_DIAG_LEVEL_MAX) {
        p_task_info->targetTypeCount = 0;
        for(auto& item : p_task_info->targetTypes)
            item = XPUM_DIAG_TASK_TYPE_MAX;
        int pos = 0;
        for (int index = XPUM_DIAG_SOFTWARE_ENV_VARIABLES; index < XPUM_DIAG_TASK_TYPE_MAX; index++) {
            if (level == XPUM_DIAG_LEVEL_1 && index == XPUM_DIAG_INTEGRATION_PCIE)
                break;
            if (level == XPUM_DIAG_LEVEL_2 && index == XPUM_DIAG_PERFORMANCE_COMPUTATION)
                break; 
            if (!isLevelDiagnosticType(static_cast<xpum_diag_task_type_t>(index)))
                continue;
            if (isPVCPlatform && (index == XPUM_DIAG_MEDIA_CODEC || index == XPUM_DIAG_LIGHT_CODEC))
                continue;
            if (!isPVCPlatform && index == XPUM_DIAG_XE_LINK_THROUGHPUT)
                continue;
            p_task_info->targetTypes[pos++] = static_cast<xpum_diag_task_type_t>(index);         
        }
        p_task_info->targetTypeCount = pos;
        XPUM_LOG_INFO("deviceId: {}, level: {}, targetTypeCount: {}", deviceId, level, p_task_info->targetTypeCount);
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
        component.message[0] = '\0';
    }
}

xpum_result_t DiagnosticManager::runLevelDiagnostics(xpum_device_id_t deviceId, xpum_diag_level_t level) {
    if (deviceId != ALL_GPU_ID && this->p_device_manager->getDevice(std::to_string(deviceId)) == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    if (level < xpum_diag_level_t::XPUM_DIAG_LEVEL_1 || level > xpum_diag_level_t::XPUM_DIAG_LEVEL_3) {
        return XPUM_RESULT_DIAGNOSTIC_INVALID_LEVEL;
    }
    return runDiagnosticsCore(deviceId, level, nullptr, 0);
}

xpum_result_t DiagnosticManager::runMultipleSpecificDiagnostics(xpum_device_id_t deviceId, xpum_diag_task_type_t types[], int count) {
    if (deviceId != ALL_GPU_ID && this->p_device_manager->getDevice(std::to_string(deviceId)) == nullptr) {
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

bool DiagnosticManager::isLevelDiagnosticType(xpum_diag_task_type_t type) {
    if (type == xpum_diag_task_type_t::XPUM_DIAG_LIGHT_CODEC || type == xpum_diag_task_type_t::XPUM_DIAG_HARDWARE_SYSMAN || type == xpum_diag_task_type_t::XPUM_DIAG_XE_LINK_ALL_TO_ALL_THROUGHPUT)
        return false;
    if (type >= xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_ENV_VARIABLES && type < xpum_diag_task_type_t::XPUM_DIAG_TASK_TYPE_MAX)
        return true;
    
    return false;
}

xpum_result_t DiagnosticManager::getDiagnosticsResult(xpum_device_id_t deviceId, xpum_diag_task_info_t *result) {
    if (deviceId != ALL_GPU_ID && this->p_device_manager->getDevice(std::to_string(deviceId)) == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    if (diagnostic_task_infos.empty())
        return XPUM_RESULT_DIAGNOSTIC_TASK_NOT_FOUND;

    std::unique_lock<std::mutex> lock(this->mutex);
    if (deviceId != ALL_GPU_ID && diagnostic_task_infos.find(deviceId) == diagnostic_task_infos.end()) {
        return XPUM_RESULT_DIAGNOSTIC_TASK_NOT_FOUND;
    }

    if (deviceId == ALL_GPU_ID) {
        combineMultiDeviceDiagnosticInfo(result);
    } else {
        result->deviceId = deviceId;
        auto task_info = diagnostic_task_infos.at(deviceId);
        result->level = task_info->level;
        result->targetTypeCount = task_info->targetTypeCount;
        result->finished = task_info->finished;
        result->count = task_info->count;
        result->startTime = task_info->startTime;
        result->endTime = task_info->endTime;
        result->result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;
        updateMessage(result->message, std::string(diagnostic_task_infos.at(deviceId)->message));

        for (int i = 0; i < result->count; i++) {
            xpum_diag_component_info_t &component = result->componentList[i];
            component.type = task_info->targetTypes[i];
            component.finished = task_info->componentList[component.type].finished;
            component.result = task_info->componentList[component.type].result;
            if (task_info->componentList[component.type].result == xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL) {
                result->result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
            }
            updateMessage(component.message, std::string(task_info->componentList[component.type].message));
        }

        if (result->finished && result->result == xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN) {
            result->result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
        }
    }
    return XPUM_OK;
}

void DiagnosticManager::combineMultiDeviceDiagnosticInfo(xpum::xpum_diag_task_info_t * result) {
    result->deviceId = ALL_GPU_ID;

    xpum_device_id_t firstId = diagnostic_task_infos.begin()->first;
    xpum_device_id_t lastId = diagnostic_task_infos.rbegin()->first;

    result->level = diagnostic_task_infos.at(firstId)->level;
    result->targetTypeCount = diagnostic_task_infos.at(firstId)->targetTypeCount;
    
    result->count = diagnostic_task_infos.at(lastId)->count;
    result->finished = diagnostic_task_infos.at(lastId)->finished;
    
    result->startTime = diagnostic_task_infos.at(firstId)->startTime;
    result->endTime = diagnostic_task_infos.at(lastId)->endTime;
    result->result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;

    for (int index = xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_ENV_VARIABLES; index < xpum_diag_task_type_t::XPUM_DIAG_TASK_TYPE_MAX; index++) {
        xpum_diag_component_info_t &component =  result->componentList[index];
        component.type = static_cast<xpum_diag_task_type_t>(index);
        component.finished = false;
        component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;
        component.message[0] = '\0';
    }

    updateMessage(result->message, std::string("Doing diagnostics"));
    for (auto diagnostic_task_info : diagnostic_task_infos) {
        auto currentId = diagnostic_task_info.first;
        auto diag_task_info = diagnostic_task_info.second;
    
        for (int i = 0; i < result->count; i++) {
            xpum_diag_component_info_t &component = result->componentList[i];
            component.type = diag_task_info->targetTypes[i];
            component.finished = diag_task_info->componentList[component.type].finished;
            if (component.result != XPUM_DIAG_RESULT_FAIL)
                component.result = diag_task_info->componentList[component.type].result;
            if (component.result == XPUM_DIAG_RESULT_FAIL)
                result->result = XPUM_DIAG_RESULT_FAIL;
    
            std::string mesg(currentId == 0 ? "" : component.message);
            if (diag_task_info->componentList[component.type].result == XPUM_DIAG_RESULT_FAIL || std::string(diag_task_info->componentList[component.type].message).find("Warning") != std::string::npos) {
                mesg += "\n GPU " + std::to_string(currentId) + " : " + std::string(diag_task_info->componentList[component.type].message);
                if (component.type == XPUM_DIAG_SOFTWARE_EXCLUSIVE) {
                    for (auto process : diagnostic_exclusive_processes[currentId]) {
                        mesg += "\n  PID: " +  process.first + " Command: " + process.second;
                    }                    
                }
                if (component.type == XPUM_DIAG_XE_LINK_THROUGHPUT) {
                    for (auto& per_device : xe_link_throughput_datas)
                        for (auto& per_link : per_device.second) {                        
                            if (per_link.srcDeviceId != currentId && per_link.dstDeviceId != currentId)
                                continue;
                            std::string line = "  GPU " + std::to_string(per_link.srcDeviceId) + "/" 
                                            + std::to_string(per_link.srcTileId) + " port " + std::to_string(per_link.srcPortId) 
                                            + " to GPU " + std::to_string(per_link.dstDeviceId) + "/" 
                                            + std::to_string(per_link.dstTileId) + " port " + std::to_string(per_link.dstPortId) 
                                            + ": " + roundDouble(per_link.currentSpeed, 3) + " GBPS. Threshold: " + roundDouble(per_link.threshold, 3) + " GBPS.";                            
                            mesg += "\n" + line;
                        }                            
                }
            }	
            updateMessage(component.message, mesg);
        }
    }

    if (result->finished) {
        updateMessage(result->message, std::string("All diagnostics done"));
        if (result->result == xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN)
            result->result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
    }

    std::vector<double> perf_pcie_bandwidth_datas;
    std::vector<double> perf_gflops_datas;
    std::vector<double> perf_memory_bandwidth_datas;
    std::vector<double> perf_peak_powers;
    std::vector<double> perf_xe_link_throughput_datas;
    std::vector<double> perf_xe_link_all_to_all_throughput_datas;

    for (auto diagnostic_perf_data : diagnostic_perf_datas) {
        perf_pcie_bandwidth_datas.push_back(diagnostic_perf_data.second.pcie_bandwidth);
        perf_gflops_datas.push_back(diagnostic_perf_data.second.gflops);
        perf_memory_bandwidth_datas.push_back(diagnostic_perf_data.second.memory_bandwidth);
        perf_peak_powers.push_back(diagnostic_perf_data.second.peak_power);
        perf_xe_link_throughput_datas.insert(perf_xe_link_throughput_datas.end(),
                                            diagnostic_perf_data.second.xe_link_throughtput.begin(),
                                            diagnostic_perf_data.second.xe_link_throughtput.end());
        perf_xe_link_all_to_all_throughput_datas.push_back(diagnostic_perf_data.second.xe_link_all_to_all_throughtput);
    }
        
    double pcie_bandwidth_mean = calculateMean(perf_pcie_bandwidth_datas);
    double gflops_mean = calculateMean(perf_gflops_datas);
    double memory_bandwidth_mean = calculateMean(perf_memory_bandwidth_datas);
    double peak_power_mean = calculateMean(perf_peak_powers);
    double xe_link_throughput_mean = calculateMean(perf_xe_link_throughput_datas);
    double xe_link_all_to_all_throughput_total = std::accumulate(perf_xe_link_all_to_all_throughput_datas.begin(), perf_xe_link_all_to_all_throughput_datas.end(), 0.0);

    double pcie_bandwidth_variance = calcaulateVariance(perf_pcie_bandwidth_datas);
    double gflops_variance = calcaulateVariance(perf_gflops_datas);
    double memory_bandwidth_variance = calcaulateVariance(perf_memory_bandwidth_datas);
    double peak_power_variance = calcaulateVariance(perf_peak_powers);
    double xe_link_throughput_variance = calcaulateVariance(perf_xe_link_throughput_datas);

    int pcie_bandwidth_ref = diagnostic_perf_datas.begin()->second.reference_pcie_bandwidth;
    int gflops_ref = diagnostic_perf_datas.begin()->second.reference_gflops;
    int memory_bandwidth_ref = diagnostic_perf_datas.begin()->second.reference_memory_bandwidth;
    int peak_power_ref = diagnostic_perf_datas.begin()->second.reference_peak_power;
    int xe_link_throughput_ref = diagnostic_perf_datas.begin()->second.reference_xe_link_throughtput;
    int xe_link_all_to_all_throughput_ref = diagnostic_perf_datas.begin()->second.reference_xe_link_all_to_all_throughtput * perf_xe_link_all_to_all_throughput_datas.size();

    for (int i = 0; i < result->count; i++) {
        xpum_diag_component_info_t &component = result->componentList[i];
        if (std::string(component.message) == "") {
            std::string desc;
            switch (component.type) {
                case XPUM_DIAG_SOFTWARE_ENV_VARIABLES:
                    desc = "Pass to check environment variables.";
                    break;
                case XPUM_DIAG_SOFTWARE_LIBRARY:
                    desc = "Pass to check libraries.";
                    break;
                case XPUM_DIAG_SOFTWARE_PERMISSION:
                    desc = "Pass to check permission.";
                    break;
                case XPUM_DIAG_SOFTWARE_EXCLUSIVE:
                    desc = "Pass to check the software exclusive.";
                    break;
                case XPUM_DIAG_LIGHT_COMPUTATION:
                    desc = "Pass to check computation.";
                    break;
                case XPUM_DIAG_LIGHT_CODEC:
                    desc = "Pass to check Media transcode functionality.";
                    break;
                case XPUM_DIAG_HARDWARE_SYSMAN:
                    desc = "Pass to do hardware sysman diagnostics.";
                    break;
                case XPUM_DIAG_INTEGRATION_PCIE:
                    desc = "Pass to check PCIe bandwidth. \n Mean: " + roundDouble(pcie_bandwidth_mean, 3) + " GBPS. Var: " + roundDouble(pcie_bandwidth_variance, 3) + ".";
                    if (pcie_bandwidth_ref > 0)
                        desc += " Ref: " + std::to_string(pcie_bandwidth_ref) + " GBPS.";
                    break;
                case XPUM_DIAG_MEDIA_CODEC:
                    desc = "Pass to check Media transcode performance.";
                    break;
                case XPUM_DIAG_PERFORMANCE_COMPUTATION:
                    desc = "Pass to check computation performance. \n Mean: " + roundDouble(gflops_mean, 3) + " GFLOPS. Var: " + roundDouble(gflops_variance, 3) + ".";
                    if (gflops_ref > 0)
                        desc += " Ref: " + std::to_string(gflops_ref) + " GFLOPS.";
                    break;
                case XPUM_DIAG_PERFORMANCE_POWER:
                    desc = "Pass to check stress power. \n Mean: " + roundDouble(peak_power_mean, 3) + " W. Var: " + roundDouble(peak_power_variance, 3) + ".";
                    if (peak_power_ref > 0)
                        desc += " Ref: " + std::to_string(peak_power_ref) + " W.";
                    break;
                case XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH:
                    desc = "Pass to check memory bandwidth. \n Mean: " + roundDouble(memory_bandwidth_mean, 3) + " GBPS. Var: " + roundDouble(memory_bandwidth_variance, 3) + ".";
                    if (memory_bandwidth_ref > 0)
                        desc += " Ref: " + std::to_string(memory_bandwidth_ref) + " GBPS.";
                    break;
                case XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION:
                    desc = "Pass to check memory allocation.";
                    break;
                case XPUM_DIAG_MEMORY_ERROR:
                    desc = "Pass to check memory error.";
                    break;
                case XPUM_DIAG_XE_LINK_THROUGHPUT:
                    desc = "Pass to check Xe Link throughput.";
                    desc += " \n Mean: " + roundDouble(xe_link_throughput_mean, 3) + " GBPS.";
                    desc += " Var: " + roundDouble(xe_link_throughput_variance, 3) + ".";
                    desc += " Ref: " + std::to_string(xe_link_throughput_ref) + " GBPS.";
                    break;
                case XPUM_DIAG_XE_LINK_ALL_TO_ALL_THROUGHPUT:
                    desc = "Pass to check Xe Link all-to-all throughput.";
                    desc += " \n Throughput: " + roundDouble(xe_link_all_to_all_throughput_total, 3) + " GBPS.";
                    desc += " Ref: " + std::to_string(xe_link_all_to_all_throughput_ref) + " GBPS.";
                    break;
                default:
                    break;
            }
            updateMessage(component.message, desc);  
        }
    }
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

void DiagnosticManager::doDiagnosticExceptionHandle(xpum_diag_task_type_t type, std::string error, std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
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
        case XPUM_DIAG_LIGHT_COMPUTATION:
            type_str = "XPUM_DIAG_LIGHT_COMPUTATION";
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
        case XPUM_DIAG_XE_LINK_THROUGHPUT:
            type_str = "XPUM_DIAG_XE_LINK_THROUGHPUT";
            break;
        case XPUM_DIAG_XE_LINK_ALL_TO_ALL_THROUGHPUT:
            type_str = "XPUM_DIAG_XE_LINK_ALL_TO_ALL_THROUGHPUT";
            break;
        default:
            break;
    }
    std::string desc = "Error in " + error;
    XPUM_LOG_ERROR("Error in diagnostics {} : {}", type_str, error);
    updateMessage(component.message, desc);
    component.finished = true;
}

void DiagnosticManager::doDiagnosticCore(xpum_device_id_t deviceId) {
    if (diagnostic_task_infos.empty()) {
        XPUM_LOG_DEBUG("DiagnosticManager::doDiagnosticCore - device not found");
    }
    
    auto targetCnt = diagnostic_task_infos.begin()->second->targetTypeCount;
    for (int i = 0; i < targetCnt; i++) {
        for (auto device : devices) {
            xpum_device_id_t currentId = std::stoi(device->getId());
            // only diag the designated device if not -1
            if (deviceId != ALL_GPU_ID && currentId != deviceId)
                continue;
            ze_device_handle_t ze_device = device->getDeviceZeHandle(); 
            zes_device_handle_t zes_device = device->getDeviceHandle(); 
            ze_driver_handle_t ze_driver = device->getDriverHandle(); 
            auto p_task_info = diagnostic_task_infos.at(currentId);
            switch (p_task_info->targetTypes[i])
            {
                case XPUM_DIAG_SOFTWARE_ENV_VARIABLES:
                    XPUM_LOG_INFO("device: {} - start environment variables diagnostic", currentId);
                    try {
                        doDiagnosticEnvironmentVariables(p_task_info);
                    } catch (BaseException &e) {
                        doDiagnosticExceptionHandle(XPUM_DIAG_SOFTWARE_ENV_VARIABLES, e.what(), p_task_info);
                    }
                    break;
                case XPUM_DIAG_SOFTWARE_LIBRARY:
                    XPUM_LOG_INFO("device: {} - start libraries diagnostic", currentId);
                    try {
                        doDiagnosticLibraries(devices, p_task_info);
                    } catch (BaseException &e) {
                        doDiagnosticExceptionHandle(XPUM_DIAG_SOFTWARE_LIBRARY, e.what(), p_task_info);
                    }
                    break;
                case XPUM_DIAG_SOFTWARE_PERMISSION:
                    XPUM_LOG_INFO("device: {} - start permission diagnostic", currentId);
                    try {
                        doDiagnosticPermission(devices.size(), p_task_info);
                    } catch (BaseException &e) {
                        doDiagnosticExceptionHandle(XPUM_DIAG_SOFTWARE_PERMISSION, e.what(), p_task_info);
                    }
                    break;
                case XPUM_DIAG_SOFTWARE_EXCLUSIVE:
                    XPUM_LOG_INFO("device: {} - start exclusive diagnostic", currentId);
                    try {
                        doDiagnosticExclusive(zes_device, diagnostic_exclusive_processes, p_task_info);
                    } catch (BaseException &e) {
                        doDiagnosticExceptionHandle(XPUM_DIAG_SOFTWARE_EXCLUSIVE, e.what(), p_task_info);
                    }
                    break;
                case XPUM_DIAG_LIGHT_COMPUTATION:
                    XPUM_LOG_INFO("device: {} - tart computation check diagnostic", currentId);
                    try {
                        doDiagnosticPeformanceComputation(ze_device, zes_device, ze_driver, diagnostic_perf_datas, p_task_info, true);
                    } catch (BaseException &e) {
                        doDiagnosticExceptionHandle(XPUM_DIAG_LIGHT_COMPUTATION, e.what(), p_task_info);
                    }
                    break;
                case XPUM_DIAG_LIGHT_CODEC:
                    XPUM_LOG_INFO("device: {} - start media codec check diagnostic", currentId);
                    try {
                        doDiagnosticMediaCodec(ze_device, zes_device, p_task_info, media_codec_perf_datas, true);
                    } catch (BaseException &e) {
                        doDiagnosticExceptionHandle(XPUM_DIAG_LIGHT_CODEC, e.what(), p_task_info);
                    }
                    break;
                case XPUM_DIAG_HARDWARE_SYSMAN:
                    XPUM_LOG_INFO("device: {} - start hardware sysmam diagnostic", currentId);
                    try {
                        doDiagnosticHardwareSysman(zes_device, p_task_info);
                    } catch (BaseException &e) {
                        doDiagnosticExceptionHandle(XPUM_DIAG_HARDWARE_SYSMAN, e.what(), p_task_info);
                    }
                    break;
                case XPUM_DIAG_INTEGRATION_PCIE:
                    XPUM_LOG_INFO("device: {} - start integration diagnostic", currentId);
                    try {
                        doDiagnosticIntegration(ze_device, zes_device, ze_driver, diagnostic_perf_datas, p_task_info);
                    } catch (BaseException &e) {
                        doDiagnosticExceptionHandle(XPUM_DIAG_INTEGRATION_PCIE, e.what(), p_task_info);
                    }
                    break;
                case XPUM_DIAG_MEDIA_CODEC:
                    XPUM_LOG_INFO("device: {} - start mediacodec diagnostic", currentId);
                    try {
                        doDiagnosticMediaCodec(ze_device, zes_device, p_task_info, media_codec_perf_datas, false);
                    } catch (BaseException &e) {
                        doDiagnosticExceptionHandle(XPUM_DIAG_MEDIA_CODEC, e.what(), p_task_info);
                    }
                    break;
                case XPUM_DIAG_PERFORMANCE_COMPUTATION:
                    XPUM_LOG_INFO("device: {} - start computation diagnostic", currentId);
                    try {
                        doDiagnosticPeformanceComputation(ze_device, zes_device, ze_driver, diagnostic_perf_datas, p_task_info, false);
                    } catch (BaseException &e) {
                        doDiagnosticExceptionHandle(XPUM_DIAG_PERFORMANCE_COMPUTATION, e.what(), p_task_info);
                    }
                    break;
                case XPUM_DIAG_PERFORMANCE_POWER:
                    XPUM_LOG_INFO("device: {} - start power diagnostic", currentId);
                    try {
                        doDiagnosticPeformancePower(ze_device, zes_device, ze_driver, diagnostic_perf_datas, p_task_info);
                    } catch (BaseException &e) {
                        doDiagnosticExceptionHandle(XPUM_DIAG_PERFORMANCE_POWER, e.what(), p_task_info);
                    }
                    break;
                case XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH:
                    XPUM_LOG_INFO("device: {} - start memory bandwidth diagnostic", currentId);
                    try {
                        doDiagnosticPeformanceMemoryBandwidth(ze_device, zes_device, ze_driver, diagnostic_perf_datas, p_task_info);
                    } catch (BaseException &e) {
                        doDiagnosticExceptionHandle(XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH, e.what(), p_task_info);
                    }
                    break;
                case XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION:
                    XPUM_LOG_INFO("device: {} - start memory allocation diagnostic", currentId);
                    try {
                        doDiagnosticPeformanceMemoryAllocation(ze_device, zes_device, ze_driver, p_task_info);
                    } catch (BaseException &e) {
                        doDiagnosticExceptionHandle(XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION, e.what(), p_task_info);
                    }
                    break;
                case XPUM_DIAG_MEMORY_ERROR:
                    XPUM_LOG_INFO("device: {} - start memory error diagnostic", currentId);
                    try {
                        doDiagnosticMemoryError(ze_device, zes_device, ze_driver, p_task_info);
                    } catch (BaseException &e) {
                        doDiagnosticExceptionHandle(XPUM_DIAG_MEMORY_ERROR, e.what(), p_task_info);
                    }
                    break;
                case XPUM_DIAG_XE_LINK_THROUGHPUT:
                    XPUM_LOG_INFO("device: {} - start xe link throughput diagnostic", currentId);
                    try {
                        doDiagnosticXeLinkThroughput(ze_device, zes_device, ze_driver, p_task_info, devices, diagnostic_perf_datas, xe_link_throughput_datas);
                    } catch (BaseException &e) {
                        doDiagnosticExceptionHandle(XPUM_DIAG_XE_LINK_THROUGHPUT, e.what(), p_task_info);
                    }
                    break;
                case XPUM_DIAG_XE_LINK_ALL_TO_ALL_THROUGHPUT:
                    if (deviceId == ALL_GPU_ID && currentId == 0) {
                        XPUM_LOG_INFO("device: all - start xe link all-to-all throughput diagnostic");
                        try {
                            doDiagnosticXeLinkAllToAllThroughput(ze_driver, devices, diagnostic_task_infos, diagnostic_perf_datas);
                        } catch (BaseException &e) {
                            doDiagnosticExceptionHandle(XPUM_DIAG_XE_LINK_THROUGHPUT, e.what(), p_task_info);
                        }
                    }
                    break;
                default:
                    break;
            }

            if (i == targetCnt - 1) {
                p_task_info->endTime = Utility::getCurrentMillisecond();
                p_task_info->finished = true;
                updateMessage(p_task_info->message, std::string("All diagnostics done"));
                XPUM_LOG_INFO("device: {}, all diagnostics done", device->getId()); 
            }            
        }
    }
}

void DiagnosticManager::doDiagnosticEnvironmentVariables(std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    std::string details;
    // DIAGNOSTIC_SOFTWARE_ENV
    xpum_diag_component_info_t &component1 = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_ENV_VARIABLES];
    p_task_info->count += 1;
    updateMessage(component1.message, std::string("Running"));
    std::vector<std::string> check_env_varibles;
    if (std::any_of(Configuration::getEnabledMetrics().begin(), Configuration::getEnabledMetrics().end(),
                    [](const MeasurementType type) { return type == METRIC_EU_ACTIVE || type == METRIC_EU_IDLE || type == METRIC_EU_STALL; })) {
        check_env_varibles.push_back(std::string("ZET_ENABLE_METRICS"));
    }

    bool find_env_varibles = true;
    for (auto it = check_env_varibles.begin(); it != check_env_varibles.end(); it++) {
        std::string check_env_var = *it;
        if (check_env_var.size() > 0 && std::getenv(check_env_var.c_str()) == nullptr) {
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

void DiagnosticManager::doDiagnosticLibraries(std::vector<std::shared_ptr<Device>> devices, std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
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
        void *handle;
        handle = dlopen((*it).c_str(), RTLD_NOW);
        if (!handle) {
            find_libs = false;
            details = (*it);
            break;
        }
        dlclose(handle);
    }

    if (find_libs) {
        component2.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
        updateMessage(component2.message, std::string("Pass to check libraries."));
    } else {
        component2.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        std::string desc = "Fail to check libraries. " + details + " is missing.";
        updateMessage(component2.message, desc);
    }
    if (component2.result != xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS) {
        component2.finished = true;
        return;
    }

    std::string fw_version_check_message;
    std::set<std::string> gfx_versions;
    std::set<std::string> amc_inband_versions;
    for (auto device : devices) {
        Property property;
        if (device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS, property)) {
            XPUM_LOG_DEBUG("device: {}, bdf address: {}", device->getId(), property.getValue());
        }
        std::string bdf = property.getValue();
        if (bdf.empty()) {
            XPUM_LOG_DEBUG("device: {} has no bdf address, skip it", device->getId());
            continue; 
        }
        if (!GPUDeviceStub::isPhysicalFunctionDevice(bdf)) {
            XPUM_LOG_DEBUG("device: {} is VF, not check its firmware version", device->getId());
            continue;
        }
        std::string gfx_version;
        std::string amc_inband_version;
        // gfx version
        if (device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_GFX_FIRMWARE_VERSION, property)) {
            XPUM_LOG_DEBUG("device: {}, gfx version: {}", device->getId(), property.getValue());
        }
        gfx_version = property.getValue();
        gfx_versions.insert(gfx_version);
    
        // amc version inband for PVC
        if (device->getDeviceModel() == XPUM_DEVICE_MODEL_PVC) {
            getAMCFirmwareVersionInBand(amc_inband_version, bdf);
            XPUM_LOG_DEBUG("device: {}, amc version: {}", device->getId(), amc_inband_version);
            if (amc_inband_version.compare("0.0.0.0") != 0) {
                amc_inband_versions.insert(amc_inband_version);
            }
        }

        if (device->getDeviceModel() == XPUM_DEVICE_MODEL_ATS_M_1 || device->getDeviceModel() == XPUM_DEVICE_MODEL_ATS_M_3) { 
            std::string minimum_gfx_version = device->getDeviceModel() == XPUM_DEVICE_MODEL_ATS_M_1 ? ATSM150_FW_MINIMUM_VERSION : ATSM75_FW_MINIMUM_VERSION;
            XPUM_LOG_DEBUG("device: {}, gfx_version: {}, minimum_gfx_version: {}", device->getId(), gfx_version, minimum_gfx_version);
            if (gfx_version < minimum_gfx_version) {
                fw_version_check_message += " GPU " + device->getId() + ": GFX version: " + gfx_version + " Minimum version: " + minimum_gfx_version + ".";
            }
            
        } else if (device->getDeviceModel() == XPUM_DEVICE_MODEL_PVC) {
            XPUM_LOG_DEBUG("device: {}, gfx_version: {}, minimum_gfx_version: {}", device->getId(), gfx_version, PVC_FW_MINIMUM_VERSION);
            if (gfx_version < PVC_FW_MINIMUM_VERSION) {
                fw_version_check_message += " GPU " + device->getId() + ": GFX version: " + gfx_version + " Minimum version: " + PVC_FW_MINIMUM_VERSION + ".";
            }

            if (amc_inband_version.compare("0.0.0.0") != 0 && amc_inband_version < PVC_AMC_MINIMUM_VERSION) {
                fw_version_check_message += " GPU " + device->getId() + ": AMC version: " + amc_inband_version + " Minimum version: " + PVC_AMC_MINIMUM_VERSION + ".";
            }
        }
    }
    if (fw_version_check_message.size() > 0 || gfx_versions.size() > 1 || amc_inband_versions.size() > 1) {
        std::string desc = "Fail to check libraries.";
        component2.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        if (fw_version_check_message.size() > 0) 
            desc += fw_version_check_message;
        if (gfx_versions.size() > 1) {
            desc += " All GPUs do not have the same GFX version.";            
        }
        if (amc_inband_versions.size() > 1) {
            desc += " All GPUs do not have the same AMC version.";       
        }
        updateMessage(component2.message, desc);
    }
    
    component2.finished = true;
}

void DiagnosticManager::doDiagnosticPermission(int gpu_total_count, std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
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

void DiagnosticManager::doDiagnosticExclusive(const zes_device_handle_t &device, std::map<xpum_device_id_t, std::vector<std::pair<std::string, std::string>>> &diagnostic_exclusive_processes, std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    std::string details;
    // DIAGNOSTIC_SOFTWARE_EXCLUSIVE
    xpum_diag_component_info_t &component4 = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_EXCLUSIVE];
    p_task_info->count += 1;
    updateMessage(component4.message, std::string("Running"));
    uint32_t process_count = 0;
    ze_result_t ret;
    XPUM_ZE_HANDLE_LOCK(device, ret = zesDeviceProcessesGetState(device, &process_count, nullptr));
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zesDeviceProcessesGetState()[" + zeResultErrorCodeStr(ret) + "]");
    }
    std::vector<zes_process_state_t> processes(process_count);
    XPUM_ZE_HANDLE_LOCK(device, ret = zesDeviceProcessesGetState(device, &process_count, processes.data()));
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zesDeviceProcessesGetState()[" + zeResultErrorCodeStr(ret) + "]");
    }
    
    diagnostic_exclusive_processes[p_task_info->deviceId].clear();
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
        diagnostic_exclusive_processes[p_task_info->deviceId].push_back({std::to_string(process.processId), command_name_str});
        XPUM_LOG_DEBUG("process pid : {}, process name : {}", process.processId, command_name_str);
    }
    if (process_count > 1) {
        component4.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
        std::string desc = "Warning: " + std::to_string(process_count) + " processses are using the device.";
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

void DiagnosticManager::doDiagnosticHardwareSysman(const zes_device_handle_t &zes_device,
                                                         std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    xpum_diag_component_info_t &component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_HARDWARE_SYSMAN];
    p_task_info->count += 1;
    updateMessage(component.message, std::string("Running"));
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;
    // disable hardware diagnostics due to instability
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
    updateMessage(component.message, std::string("Fail to find test suites for hardware sysman diagnostics."));
    component.finished = true;
}

static std::string getDevicePath(const zes_pci_properties_t& pci_props) {
    char buf[128];
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
        UEvent uevent;
        if (Utility::getUEvent(uevent, pdirent->d_name) == false) {
            continue;
        }
        len = snprintf(buf, 128, "%04d:%02x:%02x.%x",
                pci_props.address.domain, pci_props.address.bus,
                pci_props.address.device, pci_props.address.function);
        if (len > 0 && strstr(uevent.bdf.c_str(), buf) != NULL) {
            ret = "/dev/dri/";
            ret += pdirent->d_name;
            break;
        }
    }
    closedir(pdir);
    return ret;
}

void DiagnosticManager::doDiagnosticMediaCodec(const ze_device_handle_t &ze_device, const zes_device_handle_t &zes_device, std::shared_ptr<xpum_diag_task_info_t> p_task_info,
                                                    std::map<xpum_device_id_t, std::vector<xpum_diag_media_codec_metrics_t>>& media_codec_perf_datas, bool checkOnly) {
    xpum_diag_component_info_t &component = p_task_info->componentList[
        checkOnly ?
        xpum_diag_task_type_t::XPUM_DIAG_LIGHT_CODEC :
        xpum_diag_task_type_t::XPUM_DIAG_MEDIA_CODEC
    ];
    p_task_info->count += 1;
    if (Utility::isPVCPlatform(zes_device)) {
        component.result = XPUM_DIAG_RESULT_FAIL;
        component.finished = true;
        updateMessage(component.message, COMPONENT_TYPE_NOT_SUPPORTED);
        return;
    }
    updateMessage(component.message, std::string("Running"));
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;

    ze_result_t ret;
    zes_pci_properties_t pci_props;
    pci_props.stype = ZES_STRUCTURE_TYPE_PCI_PROPERTIES;
    pci_props.pNext = nullptr;
    XPUM_ZE_HANDLE_LOCK(zes_device, ret = zesDevicePciGetProperties(zes_device, &pci_props));
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zesDevicePciGetProperties()[" + zeResultErrorCodeStr(ret) + "]");
    }
    std::string device_path = getDevicePath(pci_props);
    XPUM_LOG_DEBUG("device path for media codec : {}", device_path);
    if (device_path.size() > 0) {
        std::string mediadata_folder = std::string(XPUM_RESOURCES_DIR) + std::string("mediadata/");
        if (!isPathExist(mediadata_folder)) {
            char exe_path[XPUM_MAX_PATH_LEN];
            ssize_t len = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path));
            if (len < 0 || len >= XPUM_MAX_PATH_LEN) {
                throw BaseException("readlink returns error");
            }
            exe_path[len] = '\0';
            std::string  current_file = exe_path;
            mediadata_folder = current_file.substr(0, current_file.find_last_of('/')) + "/../lib/" + Configuration::getXPUMMode() + "/resources/mediadata/";
            if (!isPathExist(mediadata_folder))
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
        
        std::atomic<bool> subtask_done(false);
        uint64_t max_temperature_value = 0;
        std::thread read_temperature_thread(readTemperatureTask, std::ref(subtask_done), std::ref(max_temperature_value), std::ref(ze_device), std::ref(zes_device));
        XPUM_LOG_INFO("start read temperature thread");
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
        subtask_done.store(true);
        read_temperature_thread.join();
        XPUM_LOG_INFO("read temperature thread end");
        if (max_temperature_value >= GPU_TEMPERATURE_THRESHOLD) {
            std::string desc(component.message);
            desc += " GPU " + std::to_string(p_task_info->deviceId) + " temperature is " + std::to_string(max_temperature_value) + " Celsius degree and the threshold is " + std::to_string(GPU_TEMPERATURE_THRESHOLD) + ".";
            updateMessage(component.message, desc);
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

std::string getOneLineFileContent(std::string file_path, std::string file_name) {
    std::string line;
    std::ifstream file(file_path + "/" + file_name);
    if (file.is_open()) {
        getline(file, line);
        file.close();
    }
    return line;
}

std::string checkDowngradedPCIe(const ze_device_handle_t &ze_device, const zes_device_handle_t &zes_device) {
    std::string ret;

    ze_result_t res;
    zes_pci_properties_t pci_props = {};
    XPUM_ZE_HANDLE_LOCK(zes_device, res = zesDevicePciGetProperties(zes_device, &pci_props));
    if (res != ZE_RESULT_SUCCESS) {
        return ret;
    }
    std::ostringstream os;
    os << std::setfill('0') << std::setw(4) << std::hex << pci_props.address.domain << std::string(":")
       << std::setw(2) << pci_props.address.bus << std::string(":")
       << std::setw(2) << pci_props.address.device << std::string(".") << pci_props.address.function;
    std::string bdf_address = os.str();

    // not check VF PCIe downgraded status, it's always unknown
    if (!GPUDeviceStub::isPhysicalFunctionDevice(bdf_address))
        return ret;
    
    char link_path[PATH_MAX];
    DIR *pdir = NULL;
    struct dirent *pdirent = NULL;
    int len = 0;
    std::string card_full_path = "";

    pdir = opendir("/sys/class/drm");
    if (pdir == NULL) {
        return ret;
    }

    while ((pdirent = readdir(pdir)) != NULL) {
        if (pdirent->d_name[0] == '.') {
            continue;
        }
        if (strncmp(pdirent->d_name, "card", 4) != 0) {
            continue;
        }
        if (strstr(pdirent->d_name, "-") != NULL) {
            continue;
        }
        len = snprintf(link_path, PATH_MAX, "/sys/class/drm/%s", pdirent->d_name);
        if (len <= 0 || len >= PATH_MAX) {
            break;
        }
        char full_path[PATH_MAX];
        ssize_t full_len = ::readlink(link_path, full_path, sizeof(full_path));
        if (full_len < 0) {
            full_len = 0;
        }
        if (full_len >= PATH_MAX) {
            full_len = PATH_MAX -1;
        }
        full_path[full_len] = '\0';

        if (strstr(full_path, bdf_address.c_str()) != NULL) {
            card_full_path = "/sys/class/drm/" + std::string(full_path);
            break;
        }
    }
    closedir(pdir);
    std::string cs = "current_link_speed", cw = "current_link_width", 
                ms = "max_link_speed", mw = "max_link_width";
    while (card_full_path != "/sys/class/drm") { 
        if (isPathExist(card_full_path + "/" + cs) && isPathExist(card_full_path + "/" + cw)
            && isPathExist(card_full_path + "/" + ms) && isPathExist(card_full_path + "/" + mw)) {
            XPUM_LOG_DEBUG("check current speed and width on: {}", card_full_path);
            std::string current_bridge = card_full_path.substr(card_full_path.find_last_of('/') + 1);
            if (getOneLineFileContent(card_full_path, cw) != getOneLineFileContent(card_full_path, mw)) {
                ret = "Width on " + current_bridge + " downgraded to x" +  getOneLineFileContent(card_full_path, cw) + ".";
            }
            std::string current_link_speed = getOneLineFileContent(card_full_path, cs);
            std::string max_link_speed = getOneLineFileContent(card_full_path, ms);
            if (current_link_speed != max_link_speed) {
                float parsed_speed = -1;
                try {
                    parsed_speed = std::stof(current_link_speed.substr(0, current_link_speed.find_first_of(' ')));
                } catch(...) {
                    XPUM_LOG_ERROR("fail to parse current_link_speed on {}", current_bridge);
                }
                XPUM_LOG_DEBUG("check current_link_speed on {}: current_link_speed: {}, max_link_speed: {}, parsed_speed: {}", 
                    current_bridge, current_link_speed, max_link_speed, parsed_speed);
                // For ATS-M, check downgraded link speed only when current_link_speed < 16.0 GT/s
                // For PVC, check downgraded link speed only when current_link_speed < 32.0 GT/s
                if (parsed_speed > 0) {
                    if (Utility::isATSMPlatform(zes_device) && parsed_speed < 16)
                        ret = "Speed on " + current_bridge + " downgraded to " +  current_link_speed + ".";
                    if (Utility::isPVCPlatform(zes_device) && parsed_speed < 32)
                        ret = "Speed on " + current_bridge + " downgraded to " +  current_link_speed + ".";
                }
            }
        }
        
        while(card_full_path.back() != '/')
            card_full_path.pop_back();
        card_full_path.pop_back();
    }

    return ret;
}

std::vector<int> DiagnosticManager::getDeviceAvailableCopyEngingGroups(const ze_device_handle_t& ze_device, bool onlyComputeMainCopy) {
    std::vector<int> availableGroups;
    uint32_t numQueueGroups = 0;
    ze_result_t ret;
    
    XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeDeviceGetCommandQueueGroupProperties(ze_device, &numQueueGroups, nullptr));
    if (ret != ZE_RESULT_SUCCESS || numQueueGroups == 0) {
        throw BaseException("zeDeviceGetCommandQueueGroupProperties()[" + zeResultErrorCodeStr(ret) + "]");
    }
    std::vector<ze_command_queue_group_properties_t> queueProperties;
    queueProperties.resize(numQueueGroups);
    XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeDeviceGetCommandQueueGroupProperties(ze_device, &numQueueGroups, queueProperties.data()));
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeDeviceGetCommandQueueGroupProperties()[" + zeResultErrorCodeStr(ret) + "]");
    }

    for (uint32_t i = 0; i < numQueueGroups; i++) {
        if (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            availableGroups.push_back(i);
            XPUM_LOG_DEBUG("Group {} (compute): {} queues", i, queueProperties[i].numQueues);
        } else if ((queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) == 0 &&
                (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY)) {
            availableGroups.push_back(i);
            XPUM_LOG_DEBUG("Group {} (copy): {} queues", i, queueProperties[i].numQueues);
        }
        if (onlyComputeMainCopy && availableGroups.size() == 2)
            break;
    }
    return availableGroups;
}


void DiagnosticManager::doDiagnosticIntegration(const ze_device_handle_t &ze_device,
                                                      const zes_device_handle_t &zes_device, 
                                                      const ze_driver_handle_t &ze_driver,
                                                      std::map<xpum_device_id_t, PerfDatas> &diagnostic_perf_datas,
                                                      std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    xpum_diag_component_info_t &component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_INTEGRATION_PCIE];
    p_task_info->count += 1;
    updateMessage(component.message, std::string("Running"));
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;

    std::atomic<bool> subtask_done(false);
    uint64_t max_temperature_value = 0;
    std::thread read_temperature_thread(readTemperatureTask, std::ref(subtask_done), std::ref(max_temperature_value), std::ref(ze_device), std::ref(zes_device));
    XPUM_LOG_INFO("start read temperature thread");
    
    std::vector<int> copyEngineGroupIds = getDeviceAvailableCopyEngingGroups(ze_device, true);
    // show compute engine group perf data if all groups pass 
    std::reverse(copyEngineGroupIds.begin(), copyEngineGroupIds.end()); 
    for (auto copyEngineGroupId : copyEngineGroupIds) {
        ze_result_t ret;
        std::vector<ze_device_handle_t> device_handles;
        std::vector<long double> all_bandwidth;
        std::vector<std::thread> bandwidth_threads;
        std::vector<std::string> error_messages;
        uint32_t subdevice_count = 0;
        ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeDeviceGetSubDevices()[" + zeResultErrorCodeStr(ret) + "]");
        }

        if (subdevice_count == 0) {
            device_handles.push_back(ze_device);
            all_bandwidth.push_back(0);
            error_messages.push_back("");
        } else {
            std::vector<ze_device_handle_t> subdevices(subdevice_count);
            ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, subdevices.data());
            if (ret != ZE_RESULT_SUCCESS) {
                throw BaseException("zeDeviceGetSubDevices()[" + zeResultErrorCodeStr(ret) + "]");
            }
            for (auto &subdevice : subdevices) {
                device_handles.push_back(subdevice);
                all_bandwidth.push_back(0);
                error_messages.push_back("");
            }
        }
        for (std::size_t i = 0; i < device_handles.size(); i++) {
            bandwidth_threads.push_back(std::thread([&all_bandwidth, &error_messages, i, &device_handles, &ze_driver, copyEngineGroupId]() {
                try {
                    ze_context_handle_t context;
                    contextCreate(ze_driver, &context);
                    ze_command_list_handle_t command_list;
                    commandListCreate(context, device_handles[i], copyEngineGroupId, &command_list);
                    ze_command_queue_handle_t command_queue;
                    commandQueueCreate(context, device_handles[i], copyEngineGroupId, 0, &command_queue);

                    long double total_bandwidth = 0.0;
                    long double total_latency = 0.0;

                    // DCGM PCIE_STR_INTS_PER_COPY 10000000.0 * 4 bytes = 40Mb
                    std::size_t size = 1 << 28;
                    void *device_buffer;
                    void *host_buffer;

                    memoryAlloc(context, device_handles[i], size, 1, &device_buffer);
                    memoryAllocHost(context, size, 1, &host_buffer);

                    uint32_t number_iterations = 500;
                    long double total_time_nsec = 0.0;
                    std::size_t element_size = sizeof(uint8_t);
                    std::size_t buffer_size = element_size * size;
                    commandListAppendMemoryCopy(command_list, device_buffer, host_buffer, buffer_size);
                    commandListClose(command_list);
                    std::chrono::high_resolution_clock::time_point time_start, time_end;
                    time_start = std::chrono::high_resolution_clock::now();
                    for (uint32_t i = 0; i < number_iterations; i++) {
                        commandQueueExecuteCommandLists(command_queue, command_list);
                        commandQueueSynchronize(command_queue);
                    }
                    time_end = std::chrono::high_resolution_clock::now();
                    total_time_nsec = std::chrono::duration<long double, std::chrono::nanoseconds::period>(time_end - time_start).count();

                    commandListDestroy(command_list);
                    commandQueueDestroy(command_queue);
                    memoryFree(context, device_buffer);
                    memoryFree(context, host_buffer);
                    contextDestroy(context);
                    calculateBandwidthLatency(total_time_nsec, static_cast<long double>(size * number_iterations), total_bandwidth, total_latency, number_iterations);
                    all_bandwidth[i] = total_bandwidth;
                } catch (BaseException &e) {
                    XPUM_LOG_DEBUG("Error in integration diagnostic {}",  e.what());
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
        XPUM_LOG_DEBUG("PCIe bandwidth on copy engine group {}, {} GBPS", copyEngineGroupId, roundDouble(total_bandwidth, 3));
        std::string bandwidth_detail = " Its bandwidth is " + roundDouble(total_bandwidth, 3) + " GBPS.";
        auto bandwidth_threshold = 0;
        auto ref_bandwidth = 0;
        if (device_names.find(zes_device) != device_names.end()) {
            bandwidth_threshold = thresholds[device_names[zes_device]]["PCIE_BANDWIDTH_MIN_GBPS"];
            ref_bandwidth = thresholds[device_names[zes_device]]["REF_PCIE_BANDWIDTH_GBPS"];
        }
        diagnostic_perf_datas[p_task_info->deviceId].pcie_bandwidth = total_bandwidth;
        diagnostic_perf_datas[p_task_info->deviceId].reference_pcie_bandwidth = ref_bandwidth;
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

        if (component.result == xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL) {
            std::string desc(component.message);
            desc += " Fail on copy engine group " + std::to_string(copyEngineGroupId) + ".";
            updateMessage(component.message, desc);
            break;
        }
    }
    subtask_done.store(true);
    read_temperature_thread.join();
    XPUM_LOG_INFO("read temperature thread end");
    if (max_temperature_value >= GPU_TEMPERATURE_THRESHOLD) {
        std::string desc(component.message);
        desc += " GPU " + std::to_string(p_task_info->deviceId) + " temperature is " + std::to_string(max_temperature_value) + " Celsius degree and the threshold is " + std::to_string(GPU_TEMPERATURE_THRESHOLD) + ".";
        updateMessage(component.message, desc);
    }
    std::string pcieInfo = checkDowngradedPCIe(ze_device, zes_device);
    if (pcieInfo.size() != 0) {
        std::string desc(component.message);
        desc += " " + pcieInfo;
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

void DiagnosticManager::doDiagnosticPeformanceMemoryAllocation(const ze_device_handle_t &ze_device, const zes_device_handle_t &zes_device,
                                                                     const ze_driver_handle_t &ze_driver,
                                                                     std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    xpum_diag_component_info_t &component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION];
    p_task_info->count += 1;
    if (GPUDeviceStub::hasVirtualFunctionOnDevice(zes_device)) {
        updateMessage(component.message, COMPONENT_TYPE_NOT_SUPPORTED_ON_PF);
        component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        return;
    }
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
                    throw BaseException("zeDeviceGetProperties()[" + zeResultErrorCodeStr(ret) + "]");
                }
                ze_context_handle_t context;
                contextCreate(ze_driver, &context);
                uint64_t target_test_memory_size = device_properties.maxMemAllocSize;
                struct sysinfo info;
                if (sysinfo(&info) == 0) {
                    uint64_t host_memory_size = (uint64_t) info.totalram / info.mem_unit;
                    if (target_test_memory_size > host_memory_size) {
                        XPUM_LOG_DEBUG("host memory size: {}, target test memory size: {}", host_memory_size, target_test_memory_size);
                        target_test_memory_size = host_memory_size;
                    }
                }
                uint64_t max_allocation_size = workgroup_size_x_ * (target_test_memory_size / workgroup_size_x_) * memory_use;
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
                        void *memory_input = nullptr;
                        memoryAllocHost(context, one_case_allocation_count, 8, &memory_input);
                        input_allocation = (uint8_t *)memory_input;
                        void *memory_output = nullptr;
                        memoryAllocHost(context, one_case_allocation_count, 8, &memory_output);
                        output_allocation = (uint8_t *)memory_output;

                    } else if (memory_type == "DEVICE") {
                        void *memory_input = nullptr;
                        memoryAlloc(context, ze_device, one_case_allocation_count, 8, &memory_input);
                        input_allocation = (uint8_t *)memory_input;
                        void *memory_output = nullptr;
                        memoryAlloc(context, ze_device, one_case_allocation_count, 8, &memory_output);
                        output_allocation = (uint8_t *)memory_output;
                    } else {
                        void *memory_input = nullptr;
                        memoryAllocShared(context, ze_device, one_case_allocation_count, 8, &memory_input);
                        input_allocation = (uint8_t *)memory_input;

                        void *memory_output = nullptr;
                        memoryAllocShared(context, ze_device, one_case_allocation_count, 8, &memory_output);
                        output_allocation = (uint8_t *)memory_output;
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
                ze_module_handle_t module_handle = nullptr;
                moduleCreate(context, ze_device, binary_file, &module_handle);
                dispatchKernelsForMemoryTest(ze_device, module_handle, input_allocations, output_allocations,
                                             data_out_vector, test_kernel_names, number_of_dispatch, one_case_allocation_count, context);
                for (auto each_allocation : input_allocations) {
                    memoryFree(context, each_allocation);
                }
                for (auto each_allocation : output_allocations) {
                    memoryFree(context, each_allocation);
                }
                moduleDestroy(module_handle);
                contextDestroy(context);
            }
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
    updateMessage(component.message, std::string("Pass to check memory allocation."));
    XPUM_LOG_INFO("Pass to check memory allocation.");
    component.finished = true;
}

std::vector<uint8_t> DiagnosticManager::loadBinaryFile(const std::string &file_path) {
    std::string folder = std::string(XPUM_RESOURCES_DIR) + std::string("kernels/");
    if (!isPathExist(folder)) {
        char exe_path[XPUM_MAX_PATH_LEN];
        ssize_t len = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path));
        if (len < 0 || len >= XPUM_MAX_PATH_LEN) {
            throw BaseException("readlink returns error");
        }
        exe_path[len] = '\0';
        std::string current_file = exe_path;
        folder = current_file.substr(0, current_file.find_last_of('/')) + "/../lib/" + Configuration::getXPUMMode() + "/resources/kernels/";
        if (!isPathExist(folder))
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

void DiagnosticManager::doDiagnosticMemoryError(const ze_device_handle_t &ze_device, const zes_device_handle_t &zes_device,
                                                                     const ze_driver_handle_t &ze_driver,
                                                                     std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    xpum_diag_component_info_t &component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_MEMORY_ERROR];
    p_task_info->count += 1;
    if (GPUDeviceStub::hasVirtualFunctionOnDevice(zes_device)) {
        updateMessage(component.message, COMPONENT_TYPE_NOT_SUPPORTED_ON_PF);
        component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        return;
    }
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
        throw BaseException("zeDeviceGetProperties()[" + zeResultErrorCodeStr(ret) + "]");
    }

    uint64_t physical_size = 0;
    uint32_t mem_module_count = 0;
    XPUM_ZE_HANDLE_LOCK(zes_device, ret = zesDeviceEnumMemoryModules(zes_device, &mem_module_count, nullptr));
    std::vector<zes_mem_handle_t> mems(mem_module_count);
    XPUM_ZE_HANDLE_LOCK(zes_device, ret = zesDeviceEnumMemoryModules(zes_device, &mem_module_count, mems.data()));
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

    uint64_t target_test_memory_size = physical_size * 1.0 * memory_use_percentage_for_error_test;
    XPUM_LOG_DEBUG("memory physical size: {}, target test memory size: {}", physical_size, target_test_memory_size);
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        uint64_t host_memory_size = (uint64_t) info.totalram / info.mem_unit;
        if (target_test_memory_size > host_memory_size) {
            XPUM_LOG_DEBUG("host memory size: {}, target test memory size: {}", host_memory_size, target_test_memory_size);
            target_test_memory_size = host_memory_size;
        }
    }
    ze_context_handle_t context;
    contextCreate(ze_driver, &context);
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
        throw BaseException("zeDeviceGetSubDevices()[" + zeResultErrorCodeStr(ret) + "]");
    }

    if (subdevice_count == 0) {
        device_handles.push_back(ze_device);
    } else {
        std::vector<ze_device_handle_t> subdevices(subdevice_count);
        ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, subdevices.data());
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeDeviceGetSubDevices()[" + zeResultErrorCodeStr(ret) + "]");
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
            memoryAlloc(context, device, one_case_allocation_count, 8, &memory_input);
            input_allocation = (uint8_t *)memory_input;
            void *memory_output = nullptr;
            memoryAlloc(context, device, one_case_allocation_count, 8, &memory_output);
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

        ze_module_handle_t module_handle = nullptr;
        moduleCreate(context, device, binary_file, &module_handle);
        dispatchKernelsForMemoryTest(device, module_handle, input_allocations, output_allocations,
                                        data_out_vector, test_kernel_names, number_of_dispatch, one_case_allocation_count, context);
        for (auto each_allocation : input_allocations) {
            memoryFree(context, each_allocation);
        }
        for (auto each_allocation : output_allocations) {
            memoryFree(context, each_allocation);
        }

        moduleDestroy(module_handle);
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
    contextDestroy(context);
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
    std::vector<ze_kernel_handle_t> test_functions;
    uint32_t workgroup_size_x_ = 8;
    uint8_t init_value_2_ = 0xAA; // 1010 1010
    uint8_t init_value_3_ = 0x55; // 0101 0101

    ze_command_list_handle_t command_list = nullptr;
    commandListCreate(context, device, 0, &command_list);
    for (uint64_t dispatch_id = 0; dispatch_id < number_of_dispatch; dispatch_id++) {
        uint8_t *src_allocation = src_allocations[dispatch_id];
        uint8_t *dst_allocation = dst_allocations[dispatch_id];
        ze_kernel_handle_t test_function = nullptr;
        kernelCreate(module, test_kernel_names[dispatch_id], &test_function);
        kernelSetGroupSize(test_function, workgroup_size_x_, 1, 1);
        kernelSetArgumentValue(test_function, 0, sizeof(src_allocation), &src_allocation);
        kernelSetArgumentValue(test_function, 1, sizeof(dst_allocation), &dst_allocation);

        uint32_t group_count_x = one_case_allocation_count / workgroup_size_x_;
        ze_group_count_t thread_group_dimensions = {group_count_x, 1, 1};

        commandListAppendMemoryFill(command_list, src_allocation, &init_value_2_, sizeof(uint8_t),
                                            one_case_allocation_count * sizeof(uint8_t));
        commandListAppendBarrier(command_list);
        commandListAppendMemoryFill(command_list, dst_allocation, &init_value_3_, sizeof(uint8_t),
                                            one_case_allocation_count * sizeof(uint8_t));
        commandListAppendBarrier(command_list);
        commandListAppendLaunchKernel(command_list, test_function, &thread_group_dimensions);
        commandListAppendBarrier(command_list);
        commandListAppendMemoryCopy(command_list, data_out[dispatch_id].data(), dst_allocation, data_out[dispatch_id].size() * sizeof(uint8_t));
        commandListAppendBarrier(command_list);
        test_functions.push_back(test_function);
    }
    commandListClose(command_list);
    ze_command_queue_handle_t command_queue;
    commandQueueCreate(context, device, 0, 0, &command_queue);
    commandQueueExecuteCommandLists(command_queue, command_list);
    commandQueueSynchronize(command_queue);
    commandQueueDestroy(command_queue);
    commandListDestroy(command_list);
    for (uint64_t dispatch_id = 0; dispatch_id < test_functions.size(); dispatch_id++) {
        kernelDestroy(test_functions[dispatch_id]);
    }
}

void DiagnosticManager::doDiagnosticPeformanceComputation(const ze_device_handle_t &ze_device, const zes_device_handle_t &zes_device, const ze_driver_handle_t &ze_driver, std::map<xpum_device_id_t, PerfDatas> &diagnostic_perf_datas, std::shared_ptr<xpum_diag_task_info_t> p_task_info, bool checkOnly) {
    int comp_index = 0;
    if (checkOnly == true) {
        comp_index = xpum_diag_task_type_t::XPUM_DIAG_LIGHT_COMPUTATION;
    } else {
        comp_index = xpum_diag_task_type_t::XPUM_DIAG_PERFORMANCE_COMPUTATION;
    }
    xpum_diag_component_info_t &compute_component = p_task_info->componentList[comp_index];
    p_task_info->count += 1;
    if (comp_index == xpum_diag_task_type_t::XPUM_DIAG_PERFORMANCE_COMPUTATION && GPUDeviceStub::hasVirtualFunctionOnDevice(zes_device)) {
        updateMessage(compute_component.message, COMPONENT_TYPE_NOT_SUPPORTED_ON_PF);
        compute_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        return;
    }

    ze_result_t ret;
    std::vector<ze_device_handle_t> device_handles;
    std::vector<long double> all_gflops;
    std::vector<std::thread> compute_threads;
    std::vector<std::string> error_messages;

    uint32_t subdevice_count = 0;
    ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, nullptr);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeDeviceGetSubDevices()[" + zeResultErrorCodeStr(ret) + "]");
    }
    if (subdevice_count == 0) {
        device_handles.push_back(ze_device);
        all_gflops.push_back(0);
        error_messages.push_back("");
    } else {
        std::vector<ze_device_handle_t> subdevices(subdevice_count);
        ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, subdevices.data());
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeDeviceGetSubDevices()[" + zeResultErrorCodeStr(ret) + "]");
        }
        for (auto &subdevice : subdevices) {
            device_handles.push_back(subdevice);
            all_gflops.push_back(0);
            error_messages.push_back("");
        }
    }
    std::atomic<bool> subtask_done(false);
    uint64_t max_temperature_value = 0;
    std::thread read_temperature_thread(readTemperatureTask, std::ref(subtask_done), std::ref(max_temperature_value), std::ref(ze_device), std::ref(zes_device));
    XPUM_LOG_INFO("start read temperature thread");
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
                    throw BaseException("zeDeviceGetProperties()[" + zeResultErrorCodeStr(ret) + "]");
                }

                ze_device_compute_properties_t device_compute_properties;
                device_compute_properties.pNext = nullptr;
                device_compute_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeDeviceGetComputeProperties(device_handles[i], &device_compute_properties));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeDeviceGetComputeProperties()[" + zeResultErrorCodeStr(ret) + "]");
                }
                ze_context_handle_t context;
                contextCreate(ze_driver, &context);
                std::vector<uint8_t> binary_file = loadBinaryFile("ze_sp_compute.spv");
                ze_module_handle_t module_handle;
                moduleCreate(context, device_handles[i], binary_file, &module_handle);
                uint64_t max_work_items = (uint64_t)device_properties.numSlices *
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
                memoryAlloc(context, device_handles[i], sizeof(float), 1, &device_input_value);
                void *device_output_buffer;
                memoryAlloc(context, device_handles[i], static_cast<std::size_t>((number_of_work_items * sizeof(float))), 1, &device_output_buffer);
                ze_command_list_handle_t command_list;
                commandListCreate(context, device_handles[i], 0, &command_list, ZE_COMMAND_LIST_FLAG_EXPLICIT_ONLY);
                ze_command_queue_handle_t command_queue;
                commandQueueCreate(context, device_handles[i], 0, 0, &command_queue, ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY);
                commandListAppendMemoryCopy(command_list, device_input_value, &input_value, sizeof(float));
                commandListAppendBarrier(command_list);
                commandListClose(command_list);
                commandQueueExecuteCommandLists(command_queue, command_list);
                commandQueueSynchronize(command_queue);
                commandListReset(command_list);
                ze_kernel_handle_t compute_sp_v1;
                setupFunction(module_handle, compute_sp_v1, "compute_sp_v1", device_input_value, device_output_buffer);

                timed = 0;
                long double current;
                // Vector width 1
                timed = runKernel(command_queue, command_list, compute_sp_v1, workgroup_info, XPUM_DIAG_PERFORMANCE_COMPUTATION, checkOnly);
                current = calculateGbps(timed, number_of_work_items * flops_per_work_item);
                all_gflops[i] = std::max(all_gflops[i], current);
                XPUM_LOG_INFO("compute sp vector width 1 done");
                kernelDestroy(compute_sp_v1);
                commandListDestroy(command_list);
                commandQueueDestroy(command_queue);
                memoryFree(context, device_input_value);
                memoryFree(context, device_output_buffer);
                moduleDestroy(module_handle);
                contextDestroy(context);
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
    auto ref_gflops = 0;
    if (device_names.find(zes_device) != device_names.end()) {
        gflops_threshold = thresholds[device_names[zes_device]]["SINGLE_PRECISION_MIN_GFLOPS"];
        ref_gflops = thresholds[device_names[zes_device]]["REF_SINGLE_PRECISION_GFLOPS"];
    }
    diagnostic_perf_datas[p_task_info->deviceId].gflops = all_gflops_value;
    diagnostic_perf_datas[p_task_info->deviceId].reference_gflops = ref_gflops;
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
                    "Pass to check computation.");
        } else {
            std::string desc = "Pass to check computation performance.";
            desc += " " + compute_detail;
            updateMessage(compute_component.message, desc);
        }
    }
    subtask_done.store(true);
    read_temperature_thread.join();
    XPUM_LOG_INFO("read temperature thread end");
    if (max_temperature_value >= GPU_TEMPERATURE_THRESHOLD) {
        std::string desc(compute_component.message);
        desc += " GPU " + std::to_string(p_task_info->deviceId) + " temperature is " + std::to_string(max_temperature_value) + " Celsius degree and the threshold is " + std::to_string(GPU_TEMPERATURE_THRESHOLD) + ".";
        updateMessage(compute_component.message, desc);
    }
    compute_component.finished = true;
}

void DiagnosticManager::doDiagnosticPeformancePower(const ze_device_handle_t &ze_device, const zes_device_handle_t &zes_device, const ze_driver_handle_t &ze_driver, std::map<xpum_device_id_t, PerfDatas> &diagnostic_perf_datas, std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    std::atomic<bool> computation_done(false);
    xpum_diag_component_info_t &power_component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_PERFORMANCE_POWER];
    p_task_info->count += 1;
    if (GPUDeviceStub::hasVirtualFunctionOnDevice(zes_device)) {
        updateMessage(power_component.message, COMPONENT_TYPE_NOT_SUPPORTED_ON_PF);
        power_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        return;
    }
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
        throw BaseException("zeDeviceGetSubDevices()[" + zeResultErrorCodeStr(ret) + "]");
    }
    if (subdevice_count == 0) {
        device_handles.push_back(ze_device);
        all_gflops.push_back(0);
        error_messages.push_back("");
    } else {
        std::vector<ze_device_handle_t> subdevices(subdevice_count);
        ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, subdevices.data());
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeDeviceGetSubDevices()[" + zeResultErrorCodeStr(ret) + "]");
        }
        for (auto &subdevice : subdevices) {
            device_handles.push_back(subdevice);
            all_gflops.push_back(0);
            error_messages.push_back("");
        }
    }

    int max_power_value = 0;
    uint64_t max_temperature_value = 0;
    std::thread read_temperature_power_thread = std::thread([&computation_done, &max_power_value, &max_temperature_value, ze_device, zes_device]() {
        while (!computation_done.load()) {
            try {
                std::shared_ptr<MeasurementData> temp = GPUDeviceStub::toGetTemperature(ze_device, zes_device, zes_temp_sensors_t::ZES_TEMP_SENSORS_GPU);
                uint64_t current_temperature_value = 0;
                if (temp->hasDataOnDevice()) {
                    current_temperature_value = temp->getCurrent() / Configuration::DEFAULT_MEASUREMENT_DATA_SCALE;
                } else if (temp->hasSubdeviceData()) {
                    current_temperature_value = temp->getSubdeviceDataCurrent(0) / Configuration::DEFAULT_MEASUREMENT_DATA_SCALE;
                }
                if (current_temperature_value > 0) {
                    XPUM_LOG_DEBUG("diagnostic: current temperature value: {}", current_temperature_value);
                    if (current_temperature_value > max_temperature_value) {
                        max_temperature_value = current_temperature_value;
                        XPUM_LOG_DEBUG("diagnostic: update max temperature value: {}", max_temperature_value);
                    }
                }
            } catch(...) {
                XPUM_LOG_ERROR("Failed to get temperature");
            }

            try {
                auto current_device_max_power_domain_value = 0;
                auto current_sub_device_power_value_sum = 0;
                ze_result_t res;
                uint32_t power_domain_count = 0;
                XPUM_ZE_HANDLE_LOCK(zes_device, res = zesDeviceEnumPowerDomains(zes_device, &power_domain_count, nullptr));
                std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
                XPUM_ZE_HANDLE_LOCK(zes_device, res = zesDeviceEnumPowerDomains(zes_device, &power_domain_count, power_handles.data()));
                if (res == ZE_RESULT_SUCCESS) {
                    for (auto &power : power_handles) {
                        zes_power_properties_t props = {};
                        zes_power_ext_properties_t ext_props = {};
                        props.pNext = &ext_props;
                        props.stype = ZES_STRUCTURE_TYPE_POWER_PROPERTIES;
                        ext_props.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
                        XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetProperties(power, &props));
                        if (res != ZE_RESULT_SUCCESS) {
                            continue;
                        }
                        if (ext_props.domain != ZES_POWER_DOMAIN_PACKAGE) continue;
                        zes_power_energy_counter_t snap1, snap2;
                        XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetEnergyCounter(power, &snap1));
                        if (res == ZE_RESULT_SUCCESS) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE * 2));
                            XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetEnergyCounter(power, &snap2));
                            if (res == ZE_RESULT_SUCCESS) {
                                int value = std::ceil((snap2.energy - snap1.energy) * 1.0 / (snap2.timestamp - snap1.timestamp));
                                if (props.onSubdevice) {
                                    current_sub_device_power_value_sum += value;
                                } else if (value > current_device_max_power_domain_value) {
                                    current_device_max_power_domain_value = value;
                                }
                            }
                        }
                    }
                }
                XPUM_LOG_DEBUG("diagnostic: current device max power domain value: {}", current_device_max_power_domain_value);
                XPUM_LOG_DEBUG("diagnostic: current sum of sub-device power values: {}", current_sub_device_power_value_sum);
                auto current_power_value = std::max(current_device_max_power_domain_value, current_sub_device_power_value_sum);
                if (current_power_value > max_power_value) {
                    max_power_value = current_power_value;
                    XPUM_LOG_DEBUG("update peak power value: {}", max_power_value);
                }
            } catch (...) {
                XPUM_LOG_ERROR("Failed to get power");
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        }
    });
    XPUM_LOG_INFO("start read power and temperature thread");
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
                    throw BaseException("zeDeviceGetProperties()[" + zeResultErrorCodeStr(ret) + "]");
                }

                ze_device_compute_properties_t device_compute_properties;
                device_compute_properties.pNext = nullptr;
                device_compute_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeDeviceGetComputeProperties(device_handles[i], &device_compute_properties));
                if (ret != ZE_RESULT_SUCCESS) {
                    throw BaseException("zeDeviceGetComputeProperties()[" + zeResultErrorCodeStr(ret) + "]");
                }
                ze_context_handle_t context;
                contextCreate(ze_driver, &context);
                std::vector<uint8_t> binary_file = loadBinaryFile("ze_int_compute.spv");
                ze_module_handle_t module_handle;
                moduleCreate(context, device_handles[i], binary_file, &module_handle);
                uint64_t max_work_items = (uint64_t)device_properties.numSlices *
                                          device_properties.numSubslicesPerSlice *
                                          device_properties.numEUsPerSubslice *
                                          device_compute_properties.maxGroupCountX * 2048;

                uint64_t max_number_of_allocated_items = device_properties.maxMemAllocSize / sizeof(int);
                uint64_t number_of_work_items = std::min(max_number_of_allocated_items, (max_work_items * sizeof(int)));
                number_of_work_items = setWorkgroups(device_compute_properties, number_of_work_items, &workgroup_info);

                void *device_input_value;
                memoryAlloc(context, device_handles[i], sizeof(int), 1, &device_input_value);
                void *device_output_buffer;
                memoryAlloc(context, device_handles[i], static_cast<std::size_t>((number_of_work_items * sizeof(int))), 1, &device_output_buffer);
                ze_command_list_handle_t command_list;
                commandListCreate(context, device_handles[i], 0, &command_list, ZE_COMMAND_LIST_FLAG_EXPLICIT_ONLY);
                ze_command_queue_handle_t command_queue;
                commandQueueCreate(context, device_handles[i], 0, 0, &command_queue, ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY);
                commandListAppendMemoryCopy(command_list, device_input_value, &input_value, sizeof(int));
                commandListAppendBarrier(command_list);
                commandListClose(command_list);
                commandQueueExecuteCommandLists(command_queue, command_list);
                commandQueueSynchronize(command_queue);
                commandListReset(command_list);
                ze_kernel_handle_t compute_int_v1;
                setupFunction(module_handle, compute_int_v1, "compute_int_v1", device_input_value, device_output_buffer);

                timed = 0;
                long double current;
                timed = runKernel(command_queue, command_list, compute_int_v1, workgroup_info, XPUM_DIAG_PERFORMANCE_POWER);
                current = calculateGbps(timed, number_of_work_items * flops_per_work_item);
                all_gflops[i] = std::max(all_gflops[i], current);
                XPUM_LOG_INFO("compute int vector width 1 done");

                kernelDestroy(compute_int_v1);
                commandListDestroy(command_list);
                commandQueueDestroy(command_queue);
                memoryFree(context, device_input_value);
                memoryFree(context, device_output_buffer);
                moduleDestroy(module_handle);
                contextDestroy(context);
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
    read_temperature_power_thread.join();
    XPUM_LOG_INFO("read power and temperature thread end");

    std::string power_detail = "Its stress power is " + std::to_string(max_power_value) + " W.";
    auto power_threshold = 0;
    auto ref_power = 0;
    if (device_names.find(zes_device) != device_names.end()) {
        power_threshold = thresholds[device_names[zes_device]]["POWER_MIN_STRESS_WATT"];
        ref_power = thresholds[device_names[zes_device]]["REF_POWER_STRESS_WATT"];
    }
    diagnostic_perf_datas[p_task_info->deviceId].peak_power = max_power_value;
    diagnostic_perf_datas[p_task_info->deviceId].reference_peak_power = ref_power;
    if (power_threshold <= 0) {
        power_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        std::string desc = "Fail to check stress power.";
        desc += " " + power_detail;
        desc += "  Unconfigured or invalid threshold.";
        updateMessage(power_component.message, desc);
    } else if (max_power_value < power_threshold) {
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
    if (max_temperature_value >= GPU_TEMPERATURE_THRESHOLD) {
        std::string desc(power_component.message);
        desc += " GPU " + std::to_string(p_task_info->deviceId) + " temperature is " + std::to_string(max_temperature_value) + " Celsius degree and the threshold is " + std::to_string(GPU_TEMPERATURE_THRESHOLD) + ".";
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
    kernelCreate(module_handle, name, &function);
    kernelSetArgumentValue(function, 0, sizeof(input), &input);
    kernelSetArgumentValue(function, 1, sizeof(output), &output);
}

long double DiagnosticManager::runKernel(ze_command_queue_handle_t command_queue, ze_command_list_handle_t command_list,
                                         ze_kernel_handle_t &function,
                                         struct ZeWorkGroups &workgroup_info, xpum_diag_task_type_t type, bool checkOnly) {
    long double timed = 0;

    kernelSetGroupSize(function, workgroup_info.group_size_x, workgroup_info.group_size_y, workgroup_info.group_size_z);
    ze_group_count_t thread_group_dimensions;
    thread_group_dimensions.groupCountX = workgroup_info.group_count_x;
    thread_group_dimensions.groupCountY = workgroup_info.group_count_y;
    thread_group_dimensions.groupCountZ = workgroup_info.group_count_z;
    commandListAppendLaunchKernel(command_list, function, &thread_group_dimensions);
    commandListClose(command_list);

    // 1 round is good enough if it is not perf diag
    if (checkOnly == true) {
        commandQueueExecuteCommandLists(command_queue, command_list);   
        commandQueueSynchronize(command_queue);
        return 0;
    }

    // Consistent with ze_peak
    uint32_t warmup_iterations = 5;
    uint32_t iters = 20;
    for (uint32_t i = 0; i < warmup_iterations; i++) {
        commandQueueExecuteCommandLists(command_queue, command_list);
        commandQueueSynchronize(command_queue);
    }

    std::chrono::high_resolution_clock::time_point time_start, time_end;
    time_start = std::chrono::high_resolution_clock::now();
    for (uint32_t i = 0; i < iters; i++) {
        commandQueueExecuteCommandLists(command_queue, command_list);
        commandQueueSynchronize(command_queue);
    }
    time_end = std::chrono::high_resolution_clock::now();
    timed = std::chrono::duration<long double, std::chrono::nanoseconds::period>(time_end - time_start).count();
    XPUM_LOG_DEBUG("runKernel - type: {}, iters: {}, total time: {}", type, iters, timed);
    commandListReset(command_list);
    return (timed / static_cast<long double>(iters));
}

long double DiagnosticManager::calculateGbps(long double period, long double total_gbps) {
    if (period != 0.0L)
        return total_gbps / period;
    else {
        XPUM_LOG_WARN("calculate gbps error - total_gbps:{} period:{}", total_gbps, period);
        return std::numeric_limits<double>::max();
    }
}

void DiagnosticManager::updateMessage(char *arr, std::string str) {
    int index = 0;
    while (index < (int)str.size() && index < XPUM_MAX_STR_LENGTH - 1) {
        arr[index] = str[index];
        index++;
    }
    arr[index] = 0;
    // when length reach XPUM_MAX_STR_LENGTH, make it ends with "..."
    if (index == XPUM_MAX_STR_LENGTH - 1)
        arr[index - 1] = arr[index - 2] = arr[index - 3] = '.';
}

std::string DiagnosticManager::roundDouble(double r, int precision) {
    std::stringstream buffer;
    buffer << std::fixed << std::setprecision(precision) << r;
    return buffer.str();
}

void DiagnosticManager::doDiagnosticPeformanceMemoryBandwidth(const ze_device_handle_t &ze_device,
                                                                    const zes_device_handle_t &zes_device,
                                                                    const ze_driver_handle_t &ze_driver,
                                                                    std::map<xpum_device_id_t, PerfDatas> &diagnostic_perf_datas,
                                                                    std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
    xpum_diag_component_info_t &memorybandwidth_component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH];
    p_task_info->count += 1;
    if (GPUDeviceStub::hasVirtualFunctionOnDevice(zes_device)) {
        updateMessage(memorybandwidth_component.message, COMPONENT_TYPE_NOT_SUPPORTED_ON_PF);
        memorybandwidth_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        return;
    }
    updateMessage(memorybandwidth_component.message, std::string("Running"));
    memorybandwidth_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;

    std::atomic<bool> subtask_done(false);
    uint64_t max_temperature_value = 0;
    std::thread read_temperature_thread(readTemperatureTask, std::ref(subtask_done), std::ref(max_temperature_value), std::ref(ze_device), std::ref(zes_device));
    XPUM_LOG_INFO("start read temperature thread");
    std::vector<int> copyEngineGroupIds = getDeviceAvailableCopyEngingGroups(ze_device, true);
    // show compute engine group perf data if all groups pass 
    std::reverse(copyEngineGroupIds.begin(), copyEngineGroupIds.end());
    for (auto copyEngineGroupId : copyEngineGroupIds) {
        ze_result_t ret;
        std::vector<ze_device_handle_t> device_handles;
        std::vector<long double> all_gbps;
        std::vector<std::thread> memorybandwidth_threads;
        std::vector<std::string> error_messages;
        uint32_t subdevice_count = 0;
        ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeDeviceGetSubDevices()[" + zeResultErrorCodeStr(ret) + "]");
        }

        if (subdevice_count == 0) {
            device_handles.push_back(ze_device);
            all_gbps.push_back(0);
            error_messages.push_back("");
        } else {
            std::vector<ze_device_handle_t> subdevices(subdevice_count);
            ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, subdevices.data());
            if (ret != ZE_RESULT_SUCCESS) {
                throw BaseException("zeDeviceGetSubDevices()[" + zeResultErrorCodeStr(ret) + "]");
            }
            for (auto &subdevice : subdevices) {
                device_handles.push_back(subdevice);
                all_gbps.push_back(0);
                error_messages.push_back("");
            }
        }
        for (std::size_t i = 0; i < device_handles.size(); i++) {
            memorybandwidth_threads.push_back(std::thread([&all_gbps, &error_messages, i, &device_handles, &ze_driver, copyEngineGroupId]() {
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
                        throw BaseException("zeDeviceGetProperties()[" + zeResultErrorCodeStr(ret) + "]");
                    }
                    ze_device_compute_properties_t device_compute_properties;
                    device_compute_properties.pNext = nullptr;
                    device_compute_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES;
                    XPUM_ZE_HANDLE_LOCK(device_handles[i], ret = zeDeviceGetComputeProperties(device_handles[i], &device_compute_properties));
                    if (ret != ZE_RESULT_SUCCESS) {
                        throw BaseException("zeDeviceGetComputeProperties()[" + zeResultErrorCodeStr(ret) + "]");
                    }
                    ze_context_handle_t context;
                    contextCreate(ze_driver, &context);
                    std::vector<uint8_t> binary_file = loadBinaryFile("ze_global_bw.spv");
                    ze_module_handle_t module_handle;
                    moduleCreate(context, device_handles[i], binary_file, &module_handle);
                    uint64_t max_items = device_properties.maxMemAllocSize / sizeof(float) / 2;
                    uint64_t num_items = std::min(max_items, (uint64_t)(1 << 29));
                    uint64_t base = (uint64_t)device_compute_properties.maxGroupSizeX * 16 * 16;
                    num_items = (num_items / base) * base;

                    std::vector<float> arr(static_cast<uint32_t>(num_items));
                    for (uint32_t i = 0; i < num_items; i++) {
                        arr[i] = static_cast<float>(i);
                    }

                    num_items = setWorkgroups(device_compute_properties, num_items, &workgroup_info);

                    void *inputBuf;
                    memoryAlloc(context, device_handles[i], static_cast<size_t>((num_items * sizeof(float))), 1, &inputBuf);
                    void *outputBuf;
                    memoryAlloc(context, device_handles[i], static_cast<size_t>((num_items * sizeof(float))), 1, &outputBuf);
                    ze_command_list_handle_t copy_command_list;
                    commandListCreate(context, device_handles[i], copyEngineGroupId, &copy_command_list, ZE_COMMAND_LIST_FLAG_EXPLICIT_ONLY);
                    ze_command_queue_handle_t copy_command_queue;
                    commandQueueCreate(context, device_handles[i], copyEngineGroupId, 0, &copy_command_queue, ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY);
                    commandListAppendMemoryCopy(copy_command_list, inputBuf, arr.data(), (arr.size() * sizeof(float)));
                    commandListAppendBarrier(copy_command_list);
                    commandListClose(copy_command_list);
                    commandQueueExecuteCommandLists(copy_command_queue, copy_command_list);
                    commandQueueSynchronize(copy_command_queue);
                    commandListReset(copy_command_list);

                    ze_command_list_handle_t command_list;
                    commandListCreate(context, device_handles[i], 0, &command_list, ZE_COMMAND_LIST_FLAG_EXPLICIT_ONLY);
                    ze_command_queue_handle_t command_queue;
                    commandQueueCreate(context, device_handles[i], 0, 0, &command_queue, ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY);
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

                    kernelDestroy(local_offset_v1);
                    kernelDestroy(global_offset_v1);
                    kernelDestroy(local_offset_v2);
                    kernelDestroy(global_offset_v2);
                    kernelDestroy(local_offset_v4);
                    kernelDestroy(global_offset_v4);
                    kernelDestroy(local_offset_v8);
                    kernelDestroy(global_offset_v8);
                    kernelDestroy(local_offset_v16);
                    kernelDestroy(global_offset_v16);
                    commandListDestroy(command_list);
                    commandQueueDestroy(command_queue);
                    commandListDestroy(copy_command_list);
                    commandQueueDestroy(copy_command_queue);
                    memoryFree(context, inputBuf);
                    memoryFree(context, outputBuf);
                    moduleDestroy(module_handle);
                    contextDestroy(context);
                } catch (BaseException &e) {
                    XPUM_LOG_DEBUG("Error in memory bandwidth diagnostic {}", e.what());
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
            all_gbps_value += all_gbps[i];
        }
        XPUM_LOG_DEBUG("memory bandwidth on copy engine group {}, {} GBPS", copyEngineGroupId, roundDouble(all_gbps_value, 3));

        std::string memorybandwidth_detail = "Its memory bandwidth is " + roundDouble(all_gbps_value, 3) + " GBPS.";
        auto memorybandwidth_threshold = 0;
        auto ref_memorybandwidth = 0;
        if (device_names.find(zes_device) != device_names.end()) {
            memorybandwidth_threshold = thresholds[device_names[zes_device]]["MEMORY_BANDWIDTH_MIN_GBPS"];
            ref_memorybandwidth = thresholds[device_names[zes_device]]["REF_MEMORY_BANDWIDTH_GBPS"];
        }
        diagnostic_perf_datas[p_task_info->deviceId].memory_bandwidth = all_gbps_value;
        diagnostic_perf_datas[p_task_info->deviceId].reference_memory_bandwidth = ref_memorybandwidth;
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

        if (memorybandwidth_component.result == xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL) {
            std::string desc(memorybandwidth_component.message);
            desc += " Fail on copy engine group " + std::to_string(copyEngineGroupId) + ".";
            updateMessage(memorybandwidth_component.message, desc);
            break;
        }
    }
    subtask_done.store(true);
    read_temperature_thread.join();
    XPUM_LOG_INFO("read temperature thread end");
    if (max_temperature_value >= GPU_TEMPERATURE_THRESHOLD) {
        std::string desc(memorybandwidth_component.message);
        desc += " GPU " + std::to_string(p_task_info->deviceId) + " temperature is " + std::to_string(max_temperature_value) + " Celsius degree and the threshold is " + std::to_string(GPU_TEMPERATURE_THRESHOLD) + ".";
        updateMessage(memorybandwidth_component.message, desc);
    }
    memorybandwidth_component.finished = true;
}

xpum_result_t DiagnosticManager::getDiagnosticsXeLinkThroughputResult(xpum_device_id_t deviceId, xpum_diag_xe_link_throughput_t resultList[], int *count) {
    if (this->p_device_manager->getDevice(std::to_string(deviceId)) == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    std::unique_lock<std::mutex> lock(this->mutex);
    std::vector<int32_t> related_device_ids;
    related_device_ids.push_back(deviceId);
    related_device_ids.insert(related_device_ids.end(), device_id_link_to_device_ids[deviceId].begin(), device_id_link_to_device_ids[deviceId].end());
    
    bool find = false;
    for (auto id : related_device_ids) {
        if (xe_link_throughput_datas.find(id) != xe_link_throughput_datas.end()) {
            find = true;
            break;
        }
    }
    if (!find) {
        return XPUM_RESULT_DIAGNOSTIC_TASK_NOT_FOUND;
    }

    int val = 0;
    for (auto id : related_device_ids)
        for (int i = 0; i < (int)xe_link_throughput_datas[id].size(); i++) {
            if (xe_link_throughput_datas[id][i].srcDeviceId != deviceId && xe_link_throughput_datas[id][i].dstDeviceId != deviceId)
                continue;
            val += 1;
        }

    if (resultList == NULL) {
        *count = val;
        return XPUM_OK;
    }

    if ((*count) < val) {
        return XPUM_BUFFER_TOO_SMALL;
    }

    *count = val;
    int pos = 0;
    for (auto id : related_device_ids) {
        for (int i = 0; i < (int)xe_link_throughput_datas[id].size(); i++) {
            xpum_diag_xe_link_throughput_t& item = xe_link_throughput_datas[id][i];
            XPUM_LOG_DEBUG("xe link throughput data - deviceId: {}, srcDeviceId:{}, srcTileId: {}, srcPortId: {}, dstDeviceId:{}, dstTileId: {}, dstPortId: {}, currentSpeed: {}, maxSpeed: {}, threshold: {}", 
                            item.deviceId, item.srcDeviceId, item.srcTileId, item.srcPortId, 
                            item.dstDeviceId, item.dstTileId, item.dstPortId, 
                            item.currentSpeed, item.maxSpeed, item.threshold);
            if (item.srcDeviceId != deviceId && item.dstDeviceId != deviceId)
                continue;
            resultList[pos].deviceId = deviceId;
            resultList[pos].srcDeviceId = item.srcDeviceId;
            resultList[pos].srcTileId = item.srcTileId;
            resultList[pos].srcPortId = item.srcPortId;
            resultList[pos].dstDeviceId = item.dstDeviceId;
            resultList[pos].dstTileId = item.dstTileId;
            resultList[pos].dstPortId = item.dstPortId;
            resultList[pos].currentSpeed = item.currentSpeed;    // GBPS  
            resultList[pos].maxSpeed = item.maxSpeed;            // GBPS
            resultList[pos].threshold = item.threshold;          // GBPS  
            pos += 1;
        }    
    }
    XPUM_LOG_DEBUG("count of failed xe link port throughputs: {}", val);
    return XPUM_OK;
}

void DiagnosticManager::getXeLinkPortTransmitCounters(const zes_device_handle_t& zes_device, int32_t device_id, std::map<std::vector<int32_t>, uint64_t>& tx_counters, double& max_speed) {
    ze_result_t ret;
    uint32_t fabric_port_count = 0;
    XPUM_ZE_HANDLE_LOCK(zes_device, ret = zesDeviceEnumFabricPorts(zes_device, &fabric_port_count, nullptr));
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zesDeviceEnumFabricPorts()[" + zeResultErrorCodeStr(ret) + "]");
    }
    std::vector<zes_fabric_port_handle_t> fabric_ports(fabric_port_count);
    XPUM_ZE_HANDLE_LOCK(zes_device, ret = zesDeviceEnumFabricPorts(zes_device, &fabric_port_count, fabric_ports.data()));
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zesDeviceEnumFabricPorts()[" + zeResultErrorCodeStr(ret) + "]");
    }
    if (fabric_ports.size() == 0) {
        return;
    }
    for (auto& fabric_port: fabric_ports) {
        zes_fabric_port_properties_t properties;
        properties.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES;
        properties.pNext = nullptr;
        XPUM_ZE_HANDLE_LOCK(zes_device, ret = zesFabricPortGetProperties(fabric_port, &properties));
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zesFabricPortGetProperties()[" + zeResultErrorCodeStr(ret) + "]");
        }

        zes_fabric_port_state_t state;
        state.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_STATE;
        state.pNext = nullptr;
        XPUM_ZE_HANDLE_LOCK(zes_device, ret = zesFabricPortGetState(fabric_port, &state));
        if (ret != ZE_RESULT_SUCCESS) {
            // workaround for zesFabricPortGetState not ready
            XPUM_LOG_DEBUG("failed to invoke zesFabricPortGetState() on deviceId: {}, tileId: {}, portId: {}, result: {}", 
                        fabric_id_convert_to_device_id[properties.portId.fabricId],
                        (int32_t)properties.portId.attachId, 
                        (int32_t)properties.portId.portNumber,
                        ret);
            if (ret == ZE_RESULT_NOT_READY) {
                int cnt = 0;
                while (ret != ZE_RESULT_SUCCESS && cnt < 2) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    XPUM_ZE_HANDLE_LOCK(zes_device, ret = zesFabricPortGetState(fabric_port, &state));
                    cnt += 1;
                }
            } 
            if (ret != ZE_RESULT_SUCCESS) {
                XPUM_LOG_DEBUG("skip the port after retries on deviceId: {}, tileId: {}, portId: {}, result: {}", 
                            fabric_id_convert_to_device_id[properties.portId.fabricId],
                            (int32_t)properties.portId.attachId, 
                            (int32_t)properties.portId.portNumber,
                            ret);
                continue;
            }
        }

        zes_fabric_port_throughput_t fabric_port_throughput;
        XPUM_ZE_HANDLE_LOCK(zes_device, ret = zesFabricPortGetThroughput(fabric_port, &fabric_port_throughput));
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zesFabricPortGetThroughput()[" + zeResultErrorCodeStr(ret) + "]");
        }
        if (state.status != ZES_FABRIC_PORT_STATUS_HEALTHY && state.status != ZES_FABRIC_PORT_STATUS_DEGRADED) {
            // remote id is invalid, so skip to the port
            continue;
        }
        
        std::vector<int32_t> key = {fabric_id_convert_to_device_id[properties.portId.fabricId], (int32_t)properties.portId.attachId, (int32_t)properties.portId.portNumber,
                            fabric_id_convert_to_device_id[state.remotePortId.fabricId], (int32_t)state.remotePortId.attachId, (int32_t)state.remotePortId.portNumber};

        std::string peer_ports = std::to_string(key[0]) + "-"
            + std::to_string(key[1]) + "-"
            + std::to_string(key[2]) + " >> "
            + std::to_string(key[3]) + "-"
            + std::to_string(key[4]) + "-"
            + std::to_string(key[5]);
        tx_counters[key] = fabric_port_throughput.txCounter;
        max_speed = properties.maxTxSpeed.bitRate * properties.maxTxSpeed.width / 8.0 / 1000 / 1000 / 1000; //GBPS
        max_speed = round(max_speed * 1000) / 1000;
        XPUM_LOG_DEBUG("fabric port {}, max_speed: {} GBPS, tx_counter: {}", peer_ports, max_speed, fabric_port_throughput.txCounter);
    }
}

void DiagnosticManager::copyMemoryDataAndCalculateXeLinkThroughput(const ze_driver_handle_t &ze_driver, std::vector<std::tuple<ze_device_handle_t, zes_device_handle_t, int32_t, ze_device_handle_t, zes_device_handle_t, int32_t>> test_pairs,
                                             std::map<xpum_device_id_t, PerfDatas> &diagnostic_perf_datas,
                                             std::map<xpum_device_id_t, std::vector<xpum_diag_xe_link_throughput_t>>& xe_link_throughput_datas, int copyEngineGroupId) {
    for (auto test_pair : test_pairs) {
        size_t mem_size = 268435456; /* 256 MiB. Consistent with ze_peer.*/
        ze_context_handle_t context;
        contextCreate(ze_driver, &context);
        DeviceInstance src_device_instance, dst_device_instance;
        src_device_instance.device = std::get<0>(test_pair);
        src_device_instance.driver = ze_driver;
        dst_device_instance.device = std::get<3>(test_pair);
        dst_device_instance.driver = ze_driver;

        src_device_instance.src_region = nullptr;
        memoryAlloc(context, src_device_instance.device, mem_size, 1, &src_device_instance.src_region);
        dst_device_instance.dst_region = nullptr;
        memoryAlloc(context, dst_device_instance.device, mem_size, 1, &dst_device_instance.dst_region);
        commandListCreate(context, src_device_instance.device, copyEngineGroupId, &src_device_instance.cmd_list);
        commandQueueCreate(context, src_device_instance.device, copyEngineGroupId, 0, &src_device_instance.cmd_queue);

        // cory core
        void *memory = nullptr;
        memoryAllocHost(context, mem_size, 1, &memory);
        uint8_t *src = static_cast<uint8_t *>(memory);
        for (uint32_t i = 0; i < mem_size; i++) {
            src[i] = i & 0xff;
        }

        commandListAppendMemoryCopy(src_device_instance.cmd_list, src_device_instance.src_region, memory, mem_size);
        commandListClose(src_device_instance.cmd_list);
        commandQueueExecuteCommandLists(src_device_instance.cmd_queue, src_device_instance.cmd_list);
        commandQueueSynchronize(src_device_instance.cmd_queue);
        commandListReset(src_device_instance.cmd_list);
        commandListAppendMemoryCopy(src_device_instance.cmd_list, 
                                                static_cast<void *>(static_cast<uint8_t *>(dst_device_instance.dst_region)),
                                                static_cast<void *>(static_cast<uint8_t *>(src_device_instance.src_region)),
                                                mem_size);
        commandListClose(src_device_instance.cmd_list);
        // warm up
        for (int i = 1; i <= 10; i++) {
            commandQueueExecuteCommandLists(src_device_instance.cmd_queue, src_device_instance.cmd_list);
            commandQueueSynchronize(src_device_instance.cmd_queue);     
        }
        // key: src_device_id, src_tile_id, src_port_id, dst_device_id, dst_tile_id, dst_port_id
        std::map<std::vector<int32_t>, uint64_t> tx_counters1;
        double max_speed = -1;
        getXeLinkPortTransmitCounters(std::get<1>(test_pair), std::get<2>(test_pair), tx_counters1, max_speed);
        auto start_time = std::chrono::high_resolution_clock::now();
        // Using a large loops for xpum dump to observe xe link throughput
        for (int i = 1; i <= 1000; i++) {
            commandQueueExecuteCommandLists(src_device_instance.cmd_queue, src_device_instance.cmd_list);
            commandQueueSynchronize(src_device_instance.cmd_queue);
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        std::map<std::vector<int32_t>, uint64_t> tx_counters2;
        getXeLinkPortTransmitCounters(std::get<1>(test_pair), std::get<2>(test_pair), tx_counters2, max_speed);

        commandListReset(src_device_instance.cmd_list);
        commandListClose(src_device_instance.cmd_list);
        commandQueueDestroy(src_device_instance.cmd_queue);
        commandListDestroy(src_device_instance.cmd_list);
        memoryFree(context, src_device_instance.src_region);
        memoryFree(context, dst_device_instance.dst_region);
        memoryFree(context, memory);
        contextDestroy(context);

        if (tx_counters1.empty() || tx_counters2.empty())
            continue;
        auto total_time_nsec = std::chrono::duration<long double, std::chrono::nanoseconds::period>(end_time - start_time).count();
        diagnostic_perf_datas[std::get<2>(test_pair)].xe_link_throughtput.clear();
        for (auto tx_data : tx_counters2) {
            auto it = tx_counters1.find(tx_data.first);
            if (it != tx_counters1.end()) {
                xpum_diag_xe_link_throughput_t xe_link_data_tx = {
                    std::get<2>(test_pair),
                    tx_data.first[0], tx_data.first[1], tx_data.first[2],
                    tx_data.first[3], tx_data.first[4], tx_data.first[5],
                    round((tx_data.second - it->second) * 1000.0 / total_time_nsec) / 1000,
                    max_speed,
                    round(max_speed * DiagnosticManager::XE_LINK_THROUGHPUT_USAGE_PERCENTAGE * 1000.0) / 1000
                };
                std::string peer_ports = std::to_string(tx_data.first[0]) + "-"
                    + std::to_string(tx_data.first[1]) + "-"
                    + std::to_string(tx_data.first[2]) + " >> "
                    + std::to_string(tx_data.first[3]) + "-"
                    + std::to_string(tx_data.first[4]) + "-"
                    + std::to_string(tx_data.first[5]);
                if (xe_link_data_tx.dstDeviceId == std::get<5>(test_pair)) {
                    diagnostic_perf_datas[std::get<2>(test_pair)].xe_link_throughtput.push_back(xe_link_data_tx.currentSpeed);
                    // when select compute engine group, check all tile ports, otherwise check tile 0 ports becasue only copy engines from stack-0 are used with implicit scaling.
                    if (xe_link_data_tx.currentSpeed < xe_link_data_tx.threshold && (copyEngineGroupId == 0 || tx_data.first[1] == 0)) {
                        xe_link_throughput_datas[std::get<2>(test_pair)].push_back(xe_link_data_tx);
                        XPUM_LOG_DEBUG("failed test on copy engine group {} - fabric port {}, max_speed: {} GBPS, current_speed: {} GBPS, threshold: {} GBPS", copyEngineGroupId, peer_ports
                        , xe_link_data_tx.maxSpeed, xe_link_data_tx.currentSpeed, xe_link_data_tx.threshold);
                    } else {
                        XPUM_LOG_DEBUG("passed test on copy engine group {} - fabric port {}, max_speed: {} GBPS, current_speed: {} GBPS, threshold: {} GBPS", copyEngineGroupId, peer_ports
                        , xe_link_data_tx.maxSpeed, xe_link_data_tx.currentSpeed, xe_link_data_tx.threshold);                    
                    }
                }
            }
        }
    }  
}

void DiagnosticManager::doDiagnosticXeLinkThroughput(const ze_device_handle_t &ze_device,
                                                    const zes_device_handle_t &zes_device,
                                                    const ze_driver_handle_t &ze_driver,
                                                    std::shared_ptr<xpum_diag_task_info_t> p_task_info,
                                                    std::vector<std::shared_ptr<Device>> devices,
                                                    std::map<xpum_device_id_t, PerfDatas> &diagnostic_perf_datas,
                                                    std::map<xpum_device_id_t, std::vector<xpum_diag_xe_link_throughput_t>>& xe_link_throughput_datas) {
    xpum_diag_component_info_t &xe_link_throughput_component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_XE_LINK_THROUGHPUT];
    if (p_task_info->level == XPUM_DIAG_LEVEL_MAX)
        p_task_info->count += 1;
    int device_id = p_task_info->deviceId;
    ze_result_t ret;
    std::vector<ze_device_handle_t> subdevices;
    uint32_t subdevice_count = 0;
    ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, nullptr);
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zeDeviceGetSubDevices()[" + zeResultErrorCodeStr(ret) + "]");
    }
    if (subdevice_count > 1)
        diagnostic_perf_datas[p_task_info->deviceId].reference_xe_link_throughtput = REF_XE_LINK_THROUGHPUT_TWO_TILE_DEVICE;
    else
        diagnostic_perf_datas[p_task_info->deviceId].reference_xe_link_throughtput = REF_XE_LINK_THROUGHPUT_ONE_TILE_DEVICE;
    uint32_t fabric_port_count = 0;
    XPUM_ZE_HANDLE_LOCK(zes_device, ret = zesDeviceEnumFabricPorts(zes_device, &fabric_port_count, nullptr));
    if (fabric_port_count == 0) {
        XPUM_LOG_DEBUG("Target device GPU {} xe link port not found", device_id);
    }
    if (!Utility::isPVCPlatform(zes_device) || devices.size() < 2 || fabric_port_count == 0) {
        xe_link_throughput_component.result = XPUM_DIAG_RESULT_FAIL;
        xe_link_throughput_component.finished = true;
        updateMessage(xe_link_throughput_component.message, COMPONENT_TYPE_NOT_SUPPORTED);
        return;
    }
    std::vector<zes_fabric_port_handle_t> fabric_ports(fabric_port_count);
    XPUM_ZE_HANDLE_LOCK(zes_device, ret = zesDeviceEnumFabricPorts(zes_device, &fabric_port_count, fabric_ports.data()));
    if (ret != ZE_RESULT_SUCCESS) {
        throw BaseException("zesDeviceEnumFabricPorts()[" + zeResultErrorCodeStr(ret) + "]");
    }
    std::string failed_port_status_message;
    for (auto& fabric_port: fabric_ports) {
        zes_fabric_port_state_t state;
        state.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_STATE;
        state.pNext = nullptr;
        XPUM_ZE_HANDLE_LOCK(zes_device, ret = zesFabricPortGetState(fabric_port, &state));
        if (ret == ZE_RESULT_SUCCESS) {
            XPUM_LOG_DEBUG("GPU {} fabric port status {}", device_id, state.status);
            if (state.status != ZES_FABRIC_PORT_STATUS_HEALTHY && state.status != ZES_FABRIC_PORT_STATUS_DEGRADED) {
                failed_port_status_message = "GPU " + std::to_string(device_id) + " port status is probably failed, disabled or unknown."; 
                break;
            }
        }
    }
    if (!failed_port_status_message.empty()) {
        xe_link_throughput_component.result = XPUM_DIAG_RESULT_FAIL;
        xe_link_throughput_component.finished = true;
        updateMessage(xe_link_throughput_component.message, failed_port_status_message);
        return;
    }

    std::vector<std::tuple<ze_device_handle_t, zes_device_handle_t, int32_t, ze_device_handle_t, zes_device_handle_t, int32_t>> test_pairs;
    for (auto device : devices) {
        ze_device_handle_t peer_ze_device = device->getDeviceZeHandle();
        zes_device_handle_t peer_zes_device = device->getDeviceHandle();
        int peer_device_id = std::stoi(device->getId());
        ze_result_t ret;
        uint32_t fabric_port_count = 0;
        XPUM_ZE_HANDLE_LOCK(peer_zes_device, ret = zesDeviceEnumFabricPorts(peer_zes_device, &fabric_port_count, nullptr));
        if (ret != ZE_RESULT_SUCCESS) {
            continue;
        }
        std::vector<zes_fabric_port_handle_t> fabric_ports(fabric_port_count);
        XPUM_ZE_HANDLE_LOCK(peer_zes_device, ret = zesDeviceEnumFabricPorts(peer_zes_device, &fabric_port_count, fabric_ports.data()));
        if (ret != ZE_RESULT_SUCCESS) {
            continue;
        }
        if (fabric_ports.size() == 0) {
            XPUM_LOG_DEBUG("Peer device GPU {} xe link port not found", peer_device_id);
            continue;
        }
        for (auto& fabric_port: fabric_ports) {
            zes_fabric_port_properties_t properties;
            properties.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES;
            properties.pNext = nullptr;
            XPUM_ZE_HANDLE_LOCK(peer_zes_device, ret = zesFabricPortGetProperties(fabric_port, &properties));
            if (ret != ZE_RESULT_SUCCESS) {
                continue;
            } else {
                fabric_id_convert_to_device_id[properties.portId.fabricId] = peer_device_id;
                break;
            }
        }

        if (peer_device_id == device_id)
            continue;
        
        ze_bool_t can_access;
        XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeDeviceCanAccessPeer(ze_device, peer_ze_device, &can_access));
        if (ret == ZE_RESULT_SUCCESS) {
            if (can_access == 1) {
                test_pairs.push_back(std::make_tuple(ze_device, zes_device, device_id, peer_ze_device, peer_zes_device, peer_device_id));
                device_id_link_to_device_ids[device_id].insert(peer_device_id);
                test_pairs.push_back(std::make_tuple(peer_ze_device, peer_zes_device, peer_device_id, ze_device, zes_device, device_id));
                device_id_link_to_device_ids[peer_device_id].insert(device_id);
                XPUM_LOG_DEBUG("GPU {} <-> GPU {} : Reachable", device_id, peer_device_id);
                std::string failed_port_status_message;
                for (auto& fabric_port: fabric_ports) {
                    zes_fabric_port_state_t state;
                    state.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_STATE;
                    state.pNext = nullptr;
                    XPUM_ZE_HANDLE_LOCK(peer_ze_device, ret = zesFabricPortGetState(fabric_port, &state));
                    if (ret == ZE_RESULT_SUCCESS) {
                        XPUM_LOG_DEBUG("Peer GPU {} fabric port status {}", peer_device_id, state.status);
                        if (state.status != ZES_FABRIC_PORT_STATUS_HEALTHY && state.status != ZES_FABRIC_PORT_STATUS_DEGRADED) {
                            failed_port_status_message = "Peer GPU " + std::to_string(peer_device_id) + " port status is probably failed, disabled or unknown.";
                            break;
                        }
                    }
                }
                if (!failed_port_status_message.empty()) {
                    xe_link_throughput_component.result = XPUM_DIAG_RESULT_FAIL;
                    xe_link_throughput_component.finished = true;
                    updateMessage(xe_link_throughput_component.message, failed_port_status_message);
                    return;
                }
            }
            else
                XPUM_LOG_DEBUG("GPU {} <-> GPU {} : Unreachable", device_id, peer_device_id);
        }
    }
    for (auto fd : fabric_id_convert_to_device_id) {
        XPUM_LOG_DEBUG("GPU {} fabric id: {}", fd.second, fd.first);
    }
    if (test_pairs.empty()) {
        xe_link_throughput_component.result = XPUM_DIAG_RESULT_FAIL;
        xe_link_throughput_component.finished = true;
        updateMessage(xe_link_throughput_component.message, COMPONENT_TYPE_NOT_SUPPORTED);
        return;
    }
    if (p_task_info->level != XPUM_DIAG_LEVEL_MAX)
        p_task_info->count += 1;
    updateMessage(xe_link_throughput_component.message, std::string("Running"));
    xe_link_throughput_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;
    std::atomic<bool> subtask_done(false);
    uint64_t max_temperature_value = 0;
    std::thread read_temperature_thread(readTemperatureTask, std::ref(subtask_done), std::ref(max_temperature_value), std::ref(ze_device), std::ref(zes_device));
    XPUM_LOG_INFO("start read temperature thread");

    std::vector<int> copyEngineGroupIds = getDeviceAvailableCopyEngingGroups(ze_device, true);
    // show compute engine group perf data if all groups pass 
    std::reverse(copyEngineGroupIds.begin(), copyEngineGroupIds.end());
    for (auto copyEngineGroupId : copyEngineGroupIds) {
        xe_link_throughput_datas.clear();

        copyMemoryDataAndCalculateXeLinkThroughput(ze_driver, test_pairs, diagnostic_perf_datas, xe_link_throughput_datas, copyEngineGroupId);
        
        bool find_failed_port = false;
        for (auto data : xe_link_throughput_datas) {
            for (auto item : data.second) {
                if (item.srcDeviceId == device_id || item.dstDeviceId == device_id) {
                    find_failed_port = true;
                    break;
                }
            }
            if (find_failed_port)
                break;
        }
        
        if (!find_failed_port) {
            updateMessage(xe_link_throughput_component.message, std::string("Pass to check Xe Link throughput."));
            xe_link_throughput_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
        } else {
            std::string desc = "Some Xe Link throughput is low. Fail on copy engine group " + std::to_string(copyEngineGroupId) + "."; 
            updateMessage(xe_link_throughput_component.message, desc);
            xe_link_throughput_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
            break;   
        }
    } 
    subtask_done.store(true);
    read_temperature_thread.join();
    XPUM_LOG_INFO("read temperature thread end");
    if (max_temperature_value >= GPU_TEMPERATURE_THRESHOLD) {
        std::string desc(xe_link_throughput_component.message);
        desc += " GPU " + std::to_string(p_task_info->deviceId) + " temperature is " + std::to_string(max_temperature_value) + " Celsius degree and the threshold is " + std::to_string(GPU_TEMPERATURE_THRESHOLD) + ".";
        updateMessage(xe_link_throughput_component.message, desc);
    }
    xe_link_throughput_component.finished = true;    
}

void set_up_buffers(const ze_context_handle_t &context, std::vector<uint32_t> device_ids, std::vector<ze_device_handle_t> ze_devices,
            std::vector<void *> &ze_src_buffers, std::vector<void *> &ze_dst_buffers, char * &ze_host_buffer,
            int number_buffer_elements, size_t &buffer_size) {
    size_t element_size = sizeof(char);
    buffer_size = element_size * number_buffer_elements;

    for (auto device_id : device_ids) {
        void *ze_buffer = nullptr;
        memoryAlloc(context, ze_devices[device_id], buffer_size, 1, &ze_buffer);
        ze_src_buffers[device_id] = ze_buffer;

        ze_buffer = nullptr;
        memoryAlloc(context, ze_devices[device_id], buffer_size, 1, &ze_buffer);
        ze_dst_buffers[device_id] = ze_buffer;
    }

    void **host_buffer = reinterpret_cast<void **>(&ze_host_buffer);
    memoryAllocHost(context, buffer_size, 1, host_buffer);
}

void initialize_src_buffer(ze_command_list_handle_t command_list, ze_command_queue_handle_t command_queue,
                                   void *src_buffer, char *host_buffer, size_t buffer_size) {
    commandListAppendMemoryCopy(command_list, src_buffer, host_buffer, buffer_size);
    commandListClose(command_list);
    commandQueueExecuteCommandLists(command_queue, command_list);
    commandQueueSynchronize(command_queue); 
    commandListReset(command_list); 
}

void initialize_buffers(const ze_context_handle_t &context, std::vector<uint32_t> device_ids, std::vector<DeviceCmdQueueAndListPairs> ze_peer_devices, 
                        std::vector<void *> ze_src_buffers, char *host_buffer, size_t buffer_size) {
    host_buffer[0] = 1;
    host_buffer[1] = 2;
    for (size_t k = 2; k < buffer_size; k++) {
        host_buffer[k] = host_buffer[k - 2] + host_buffer[k - 1];
    }

    for (auto device_id : device_ids) {
        ze_command_list_handle_t command_list = ze_peer_devices[device_id].engines[0].second;
        ze_command_queue_handle_t command_queue = ze_peer_devices[device_id].engines[0].first;
        void *src_buffer = ze_src_buffers[device_id];
        initialize_src_buffer(command_list, command_queue, src_buffer, host_buffer, buffer_size);
    }
}

void perform_copy(std::vector<uint32_t> device_ids, std::vector<DeviceCmdQueueAndListPairs> ze_peer_devices, std::vector<uint32_t> queues,
                    std::vector<void *> ze_src_buffers, std::vector<void *> ze_dst_buffers, size_t buffer_size, std::vector<std::vector<bool>> hasXeLink) {
    size_t num_engines = queues.size();
    size_t chunk = buffer_size / num_engines;
    for (auto local_device_id : device_ids) {
        for (auto remote_device_id : device_ids) {
            if (local_device_id == remote_device_id || !hasXeLink[local_device_id][remote_device_id])
                continue;

            void *dst_buffer = ze_dst_buffers[remote_device_id];
            void *src_buffer = ze_src_buffers[local_device_id];

            for (size_t e = 0; e < num_engines; e++) {
                ze_command_list_handle_t command_list = ze_peer_devices[local_device_id].engines[e].second;
                commandListAppendMemoryCopy(command_list,
                    reinterpret_cast<void *>(reinterpret_cast<uint64_t>(dst_buffer) + e * chunk),
                    reinterpret_cast<void *>(reinterpret_cast<uint64_t>(src_buffer) + e * chunk), chunk);
            }
        }
    }

    for (auto local_device_id : device_ids) {
        for (auto remote_device_id : device_ids) {
            if (local_device_id == remote_device_id || !hasXeLink[local_device_id][remote_device_id])
                continue;

            for (size_t e = 0; e < num_engines; e++) {
                ze_command_list_handle_t command_list = ze_peer_devices[local_device_id].engines[e].second;
                commandListClose(command_list);
            }
        }
    }
    int round = 1000;
    //  Not take too long when there are 8+ devices/tiles
    if (device_ids.size() >= 16)
        round = 200;
    else if (device_ids.size() >= 8)
        round = 400;
    XPUM_LOG_INFO("actual round for xe link all-to-all copy: {}", round);
    for (int i = 0; i < round; i++) {
        for (auto local_device_id : device_ids) {
            for (auto remote_device_id : device_ids) {
                if (local_device_id == remote_device_id || !hasXeLink[local_device_id][remote_device_id])
                    continue;

                uint32_t device_id = local_device_id;

                for (size_t e = 0; e < num_engines; e++) {
                    ze_command_list_handle_t command_list = ze_peer_devices[device_id].engines[e].second;
                    ze_command_queue_handle_t command_queue = ze_peer_devices[device_id].engines[e].first;
                    commandQueueExecuteCommandLists(command_queue, command_list);
                }
            }
        }

        for (auto local_device_id : device_ids) {
            for (auto remote_device_id : device_ids) {
                if (local_device_id == remote_device_id || !hasXeLink[local_device_id][remote_device_id])
                    continue;

                for (size_t e = 0; e < num_engines; e++) {
                    ze_command_queue_handle_t command_queue = ze_peer_devices[local_device_id].engines[e].first;
                    commandQueueSynchronize(command_queue);
                }
            }
        }
    }

    for (auto local_device_id : device_ids)
        for (size_t e = 0; e < num_engines; e++) {
            commandListReset(ze_peer_devices[local_device_id].engines[e].second);
        }
}

void free_buffers(const ze_context_handle_t &context, std::vector<uint32_t> device_ids, 
            std::vector<void *> ze_src_buffers, std::vector<void *> ze_dst_buffers, char *ze_host_buffer) {
    for (auto device_id : device_ids) {
        if (ze_src_buffers[device_id]) {
            memoryFree(context, ze_src_buffers[device_id]);
            ze_src_buffers[device_id] = nullptr;
        }
        if (ze_dst_buffers[device_id]) {
            memoryFree(context, ze_dst_buffers[device_id]);
            ze_dst_buffers[device_id] = nullptr;
        }

    }
    memoryFree(context, ze_host_buffer);
}

void xe_link_all_to_all_parallel_copy(const ze_driver_handle_t &ze_driver, std::vector<ze_device_handle_t> all_ze_devices, std::vector<std::vector<bool>> hasXeLink) {
    std::vector<uint32_t> device_ids{};
    for (uint32_t i = 0; i < all_ze_devices.size(); i++) {
        device_ids.push_back(i);
    }

    ze_result_t ret;
    ze_context_handle_t context;
    contextCreate(ze_driver, &context);
    std::vector<void *> ze_src_buffers;
    std::vector<void *> ze_dst_buffers;
    std::vector<DeviceCmdQueueAndListPairs> ze_peer_devices;
    char *ze_host_buffer = nullptr;
    std::vector<uint32_t> queues{};
    
    std::size_t flat_device_count = all_ze_devices.size();
    ze_peer_devices.resize(flat_device_count);
    ze_src_buffers.resize(flat_device_count);
    ze_dst_buffers.resize(flat_device_count);

    for (uint32_t d = 0; d < flat_device_count; d++) {
        uint32_t numQueueGroups = 0;
        XPUM_ZE_HANDLE_LOCK(all_ze_devices[d], ret = zeDeviceGetCommandQueueGroupProperties(all_ze_devices[d], &numQueueGroups, nullptr));
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeDeviceGetCommandQueueGroupProperties()[" + zeResultErrorCodeStr(ret) + "]");
        }
        std::vector<ze_command_queue_group_properties_t> queueProperties;
        queueProperties.resize(numQueueGroups);
        XPUM_ZE_HANDLE_LOCK(all_ze_devices[d], ret = zeDeviceGetCommandQueueGroupProperties(all_ze_devices[d], &numQueueGroups, queueProperties.data()));
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeDeviceGetCommandQueueGroupProperties()[" + zeResultErrorCodeStr(ret) + "]");
        }

        for (uint32_t g = 0; g < numQueueGroups; g++) {
            for (uint32_t q = 0; q < queueProperties[g].numQueues; q++) {
                // g0: compute g1: main copy g2: link copy 
                if (g == 2) {
                    ze_command_queue_handle_t command_queue;
                    commandQueueCreate(context, all_ze_devices[d], g, q, &command_queue);

                    ze_command_list_handle_t command_list;
                    commandListCreate(context, all_ze_devices[d], g, &command_list);

                    auto enginePair = std::make_pair(command_queue, command_list);
                    ze_peer_devices[d].engines.push_back(enginePair);
                }
            }
        }
    }

    for (std::size_t i = 0; i < ze_peer_devices.front().engines.size(); i++)
        queues.push_back(i);

    size_t buffer_size = 0;
    // The buffer size may have impacts on peak throughput and here we adopt the range from 4MiB to 64 MiB.
    for (int step = 4; step <= 64; step *= 2) {
        int number_buffer_elements = step * 1024 * 1024; // MiB
        number_buffer_elements = number_buffer_elements * queues.size();
    
        set_up_buffers(context, device_ids, all_ze_devices, ze_src_buffers, ze_dst_buffers, ze_host_buffer, number_buffer_elements, buffer_size);

        initialize_buffers(context, device_ids, ze_peer_devices, ze_src_buffers, ze_host_buffer, buffer_size);

        perform_copy(device_ids, ze_peer_devices, queues, ze_src_buffers, ze_dst_buffers, buffer_size, hasXeLink);

        free_buffers(context, device_ids, ze_src_buffers, ze_dst_buffers, ze_host_buffer);
    }


    for (auto &device : ze_peer_devices) {
        for (auto enginePair : device.engines) {
            commandQueueDestroy(enginePair.first);
            commandListDestroy(enginePair.second);
        }
    }
    contextDestroy(context);
}

void DiagnosticManager::doDiagnosticXeLinkAllToAllThroughput(const ze_driver_handle_t &ze_driver,
                                                            std::vector<std::shared_ptr<Device>> devices, 
                                                            std::map<xpum_device_id_t, std::shared_ptr<xpum_diag_task_info_t>>& diagnostic_task_infos, 
                                                            std::map<xpum_device_id_t, PerfDatas> &diagnostic_perf_datas) {
    for (auto& diagnostic_task_info : diagnostic_task_infos) {
        diagnostic_task_info.second->count += 1;
        xpum_diag_component_info_t &xe_link_throughput_component = diagnostic_task_info.second->componentList[xpum_diag_task_type_t::XPUM_DIAG_XE_LINK_ALL_TO_ALL_THROUGHPUT];
        xe_link_throughput_component.finished = false;
        updateMessage(xe_link_throughput_component.message, std::string("Running"));
        xe_link_throughput_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;
    }

    std::vector<ze_device_handle_t> ze_devices;
    std::vector<zes_device_handle_t> zes_devices;
    bool isPVCPlatform = false;
    std::string failed_port_status_message;
    // (local_fabric_id, local_tile_id) -> [(remote_fabric_id, remote_tile_id)]
    std::map<std::pair<uint32_t, uint32_t>, std::set<std::pair<uint32_t, uint32_t>>> connectedFabricAttachIds;
    std::vector<std::vector<bool>> hasXeLink(16, std::vector<bool>(16, false));

    ze_result_t ret;
    for (auto device : devices) {
        if (device->getDeviceModel() == XPUM_DEVICE_MODEL_PVC) {
            isPVCPlatform = true;
        }
        ze_device_handle_t ze_device = device->getDeviceZeHandle();
        zes_device_handle_t zes_device = device->getDeviceHandle();
        ze_devices.emplace_back(ze_device);
        zes_devices.emplace_back(zes_device);

        uint32_t fabric_port_count = 0;
        XPUM_ZE_HANDLE_LOCK(zes_device, ret = zesDeviceEnumFabricPorts(zes_device, &fabric_port_count, nullptr));
        if (fabric_port_count == 0) {
            failed_port_status_message = "Xe Link port not found";
            break;
        }
        std::vector<zes_fabric_port_handle_t> fabric_ports(fabric_port_count);
        XPUM_ZE_HANDLE_LOCK(zes_device, ret = zesDeviceEnumFabricPorts(zes_device, &fabric_port_count, fabric_ports.data()));
        if (ret != ZE_RESULT_SUCCESS) {
            failed_port_status_message = "Xe Link port not found";
            break;
        }
        for (auto& fabric_port: fabric_ports) {
            zes_fabric_port_properties_t properties;
            properties.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES;
            properties.pNext = nullptr;
            XPUM_ZE_HANDLE_LOCK(fabric_port, ret = zesFabricPortGetProperties(fabric_port, &properties));
            if (ret == ZE_RESULT_SUCCESS) {
                fabric_id_convert_to_device_id[properties.portId.fabricId] = std::stoi(device->getId());
            }
            zes_fabric_port_state_t state;
            state.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_STATE;
            state.pNext = nullptr;
            XPUM_ZE_HANDLE_LOCK(zes_device, ret = zesFabricPortGetState(fabric_port, &state));
            if (ret == ZE_RESULT_SUCCESS) {
                XPUM_LOG_DEBUG("GPU {} fabric port status {}", device->getId(), state.status);
                if (state.status != ZES_FABRIC_PORT_STATUS_HEALTHY && state.status != ZES_FABRIC_PORT_STATUS_DEGRADED) {
                    failed_port_status_message = "GPU " + device->getId() + " port status is probably failed, disabled or unknown."; 
                    break;
                }
            }
            auto local = std::make_pair(properties.portId.fabricId, properties.portId.attachId);
            auto remote = std::make_pair(state.remotePortId.fabricId, state.remotePortId.attachId);
            connectedFabricAttachIds[local].insert(remote);
        }
        if (failed_port_status_message.size() > 0)
            break;
    }
    bool canAccessAll = zeDeviceCanAccessAllPeer(ze_devices);

    if (!isPVCPlatform || ze_devices.size() < 2 || failed_port_status_message.size() > 0 || !canAccessAll) {
        for (auto& diagnostic_task_info : diagnostic_task_infos) {
            xpum_diag_component_info_t &xe_link_throughput_component = diagnostic_task_info.second->componentList[xpum_diag_task_type_t::XPUM_DIAG_XE_LINK_ALL_TO_ALL_THROUGHPUT];
            xe_link_throughput_component.finished = true;
            if (isPVCPlatform && failed_port_status_message.size() > 0)
                updateMessage(xe_link_throughput_component.message, failed_port_status_message);
            else
                updateMessage(xe_link_throughput_component.message, COMPONENT_TYPE_NOT_SUPPORTED);
            xe_link_throughput_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        }
        return;      
    }

    uint32_t root_device_count = ze_devices.size();
    bool hasTile = false;
    std::vector<ze_device_handle_t> ze_tile_devices;
    for (auto& ze_device : ze_devices) {
        uint32_t subdevice_count = 0;
        XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, nullptr));
        if (subdevice_count == 0) {
            break;
        } else {
            hasTile = true;
            std::vector<ze_device_handle_t> subdevices(subdevice_count);
            XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeDeviceGetSubDevices(ze_device, &subdevice_count, subdevices.data()));
            for (auto &subdevice : subdevices) {
                ze_tile_devices.push_back(subdevice);
            }
        }
    }

    std::vector<ze_device_handle_t> all_ze_devices = ze_devices;
    if (hasTile) {
        all_ze_devices = ze_tile_devices;
    }
    for (auto& connectedFabricAttachId : connectedFabricAttachIds) {
        uint32_t localFabricId = connectedFabricAttachId.first.first;
        uint32_t localTileId = connectedFabricAttachId.first.second;
        for (auto& remote : connectedFabricAttachId.second) {
            uint32_t remoteFabricId = remote.first;
            uint32_t remoteTileId = remote.second;
            if (fabric_id_convert_to_device_id.count(localFabricId) > 0
                && fabric_id_convert_to_device_id.count(remoteFabricId) > 0) {
                int localId = fabric_id_convert_to_device_id[localFabricId];
                int remotetId = fabric_id_convert_to_device_id[remoteFabricId];
                if (hasTile) {
                    if (localTileId == 0) {
                        localId *= 2;
                    } else {
                        localId = localId * 2 + 1;
                    }
                    if (remoteTileId == 0) {
                        remotetId *= 2;
                    } else {
                        remotetId = remotetId * 2 + 1;
                    }
                }
                hasXeLink[localId][remotetId] = true;
            }
        }
    }
    for(std::size_t i = 0; i < all_ze_devices.size(); i++) {
        for(std::size_t j = 0; j < all_ze_devices.size(); j++) {
            if (hasXeLink[i][j])
                XPUM_LOG_INFO("local id {} - remote id {} have xe link", i, j);
        }
    }
    int REF_XE_LINK_ALL_TO_ALL_THROUGHPUT = -1;

    if (hasTile) {
        if (root_device_count == 2) {
            REF_XE_LINK_ALL_TO_ALL_THROUGHPUT = REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X2_TWO_TILE_DEVICE;
        } else if (root_device_count == 4) {
            REF_XE_LINK_ALL_TO_ALL_THROUGHPUT = REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X4_TWO_TILE_DEVICE;
        } else if (root_device_count == 8) {
            REF_XE_LINK_ALL_TO_ALL_THROUGHPUT = REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X8_TWO_TILE_DEVICE;
        }
    } else {
        if (root_device_count == 2) {
            REF_XE_LINK_ALL_TO_ALL_THROUGHPUT = REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X2_ONE_TILE_DEVICE;
        } else if (root_device_count == 4) {
            REF_XE_LINK_ALL_TO_ALL_THROUGHPUT = REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X4_ONE_TILE_DEVICE;
        } else if (root_device_count == 8) {
            REF_XE_LINK_ALL_TO_ALL_THROUGHPUT = REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X8_ONE_TILE_DEVICE;
        }
    }

    std::atomic<bool> all_to_all_copy_done(false);
    std::vector<uint64_t> temperatures(root_device_count);
    std::vector<double> allToallThroughputs(root_device_count);
    std::thread read_temperature_and_rwbandwidth_thread = std::thread([&all_to_all_copy_done, &ze_devices, &zes_devices, &temperatures, &allToallThroughputs]() {
        std::map<std::string, uint64_t> TxCnts, RxCnts, Timestamps;
        while (!all_to_all_copy_done.load()) {
            for (std::size_t i = 0; i < zes_devices.size(); i++) {
                try {
                    std::shared_ptr<MeasurementData> temp = GPUDeviceStub::toGetTemperature(ze_devices[i], zes_devices[i], zes_temp_sensors_t::ZES_TEMP_SENSORS_GPU);
                    uint64_t current_temperature_value = 0;
                    if (temp->hasDataOnDevice()) {
                        current_temperature_value = temp->getCurrent() / Configuration::DEFAULT_MEASUREMENT_DATA_SCALE;
                    } else if (temp->hasSubdeviceData()) {
                        current_temperature_value = temp->getSubdeviceDataCurrent(0) / Configuration::DEFAULT_MEASUREMENT_DATA_SCALE;
                    }
                    if (current_temperature_value > 0) {
                        if (current_temperature_value > temperatures[i]) {
                            temperatures[i] = current_temperature_value;
                            XPUM_LOG_DEBUG("diagnostic: update max temperature value: {} on GPU {}", temperatures[i], i);
                        }
                    }
                } catch (...) {
                    XPUM_LOG_ERROR("Failed to get gpu temperature on device {}", i);
                    break;
                }
            }
            try {
                std::map<std::string, uint64_t> currentTxCnts, currentRxCnts, currentTimestamps;
                ze_result_t ret;
                for (std::size_t i = 0; i < zes_devices.size(); i++) {
                    zes_device_handle_t zes_device = zes_devices[i];
                    int deviceId = i;

                    uint32_t fabric_port_count = 0;
                    XPUM_ZE_HANDLE_LOCK(zes_device, ret = zesDeviceEnumFabricPorts(zes_device, &fabric_port_count, nullptr));
                    if (ret != ZE_RESULT_SUCCESS || fabric_port_count == 0) {
                        continue;
                    }
                    std::vector<zes_fabric_port_handle_t> fabric_ports(fabric_port_count);
                    XPUM_ZE_HANDLE_LOCK(zes_device, ret = zesDeviceEnumFabricPorts(zes_device, &fabric_port_count, fabric_ports.data()));
                    if (ret != ZE_RESULT_SUCCESS) {
                        continue;
                    }
                    for (auto& fabric_port: fabric_ports) {
                        zes_fabric_port_properties_t properties;
                        properties.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES;
                        properties.pNext = nullptr;
                        XPUM_ZE_HANDLE_LOCK(fabric_port, ret = zesFabricPortGetProperties(fabric_port, &properties));

                        zes_fabric_port_state_t state;
                        state.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_STATE;
                        state.pNext = nullptr;
                        XPUM_ZE_HANDLE_LOCK(fabric_port, ret = zesFabricPortGetState(fabric_port, &state));

                        if (state.status != ZES_FABRIC_PORT_STATUS_HEALTHY && state.status != ZES_FABRIC_PORT_STATUS_DEGRADED) {
                            continue;
                        }

                        zes_fabric_port_throughput_t fabric_port_throughput;
                        XPUM_ZE_HANDLE_LOCK(fabric_port, ret = zesFabricPortGetThroughput(fabric_port, &fabric_port_throughput));

                        std::string peer_port = std::to_string(deviceId) + "-" + std::to_string(properties.portId.attachId) + "-" + std::to_string(properties.portId.portNumber);
                        currentTxCnts[peer_port] = fabric_port_throughput.txCounter;
                        currentRxCnts[peer_port] = fabric_port_throughput.rxCounter;
                        currentTimestamps[peer_port] = fabric_port_throughput.timestamp;
                    }
                }
                if (TxCnts.size() > 0 && RxCnts.size() > 0 && Timestamps.size() > 0) {
                    std::vector<double> currentAllToallThroughputs(zes_devices.size(), 0.0);
                    double txThroughtput = 0;
                    for (auto port : currentTxCnts) {
                        int32_t deviceId = std::stoi(port.first.substr(0, 1));
                        if (TxCnts.count(port.first) > 0 && currentTimestamps.count(port.first) > 0 && Timestamps.count(port.first) 
                                && port.second > TxCnts[port.first] && currentTimestamps[port.first] > Timestamps[port.first]) {
                            txThroughtput = 1000000.0 * (port.second - TxCnts[port.first]) / (currentTimestamps[port.first] - Timestamps[port.first]) / 1000000000;
                            // only count per GPU tx throughput
                            currentAllToallThroughputs[deviceId] += txThroughtput;
                            XPUM_LOG_DEBUG("peer_port : {} txThroughtput(GB/s): {}",  port.first, txThroughtput);
                        }
                    }
                    double rxThroughtput = 0;
                    for (auto port : currentRxCnts) {
                        if (RxCnts.count(port.first) > 0 && currentTimestamps.count(port.first) > 0 && Timestamps.count(port.first)
                                && port.second > RxCnts[port.first] && currentTimestamps[port.first] > Timestamps[port.first]) {
                            rxThroughtput = 1000000.0 * (port.second - RxCnts[port.first]) / (currentTimestamps[port.first] - Timestamps[port.first]) / 1000000000;
                            // log rx throughput info for debug 
                            XPUM_LOG_DEBUG("peer_port : {} rxThroughtput(GB/s): {}",  port.first, rxThroughtput);
                        }
                    }
                    double currentSum = std::accumulate(currentAllToallThroughputs.begin(), currentAllToallThroughputs.end(), 0.0);
                    if (currentSum > std::accumulate(allToallThroughputs.begin(), allToallThroughputs.end(), 0.0)) {
                        allToallThroughputs = currentAllToallThroughputs;
                        XPUM_LOG_DEBUG("diagnostic: update max xe link all-to-all throughput {}", currentSum);
                    }
                }

                TxCnts = currentTxCnts;
                RxCnts = currentRxCnts;
                Timestamps = currentTimestamps;
            } catch (...) {
                XPUM_LOG_ERROR("Failed to update Xe Link all-to-all throughput");
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    xe_link_all_to_all_parallel_copy(ze_driver, all_ze_devices, hasXeLink);

    all_to_all_copy_done.store(true);
    read_temperature_and_rwbandwidth_thread.join();
    
    for (auto& diagnostic_task_info : diagnostic_task_infos) {
        xpum_diag_component_info_t &xe_link_throughput_component = diagnostic_task_info.second->componentList[xpum_diag_task_type_t::XPUM_DIAG_XE_LINK_ALL_TO_ALL_THROUGHPUT];
        xe_link_throughput_component.finished = true;

        std::string desc;
        int32_t all_to_all_bandwidth_threshold = XE_LINK_ALL_TO_ALL_THROUGHPUT_MIN_RATIO_OF_REF * REF_XE_LINK_ALL_TO_ALL_THROUGHPUT;
        if (all_to_all_bandwidth_threshold <= 0) {
            desc = "Fail to check Xe Link all-to-all throughput.";
            desc += "  Unconfigured or invalid threshold.";
            xe_link_throughput_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        } else if (allToallThroughputs[diagnostic_task_info.first] < all_to_all_bandwidth_threshold) {
            desc = "Fail to check Xe Link all-to-all throughput.";
            desc += " Its all-to-all bandwidth is " + roundDouble(allToallThroughputs[diagnostic_task_info.first], 3) + " GBPS.";
            desc += " Threshold is " + std::to_string(all_to_all_bandwidth_threshold) + " GBPS.";
            xe_link_throughput_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        } else {
            std::string desc = "Pass to check Xe Link all-to-all throughput.";
            xe_link_throughput_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
        }
        if (temperatures[diagnostic_task_info.first] >= GPU_TEMPERATURE_THRESHOLD)
            desc += " GPU " + std::to_string(diagnostic_task_info.first) + " temperature is " + std::to_string(temperatures[diagnostic_task_info.first]) + " Celsius degree and the threshold is " + std::to_string(GPU_TEMPERATURE_THRESHOLD) + ".";
        updateMessage(xe_link_throughput_component.message, desc);
        diagnostic_perf_datas[diagnostic_task_info.first].xe_link_all_to_all_throughtput = allToallThroughputs[diagnostic_task_info.first];
        diagnostic_perf_datas[diagnostic_task_info.first].reference_xe_link_all_to_all_throughtput = REF_XE_LINK_ALL_TO_ALL_THROUGHPUT;
    }
}

#define SCORE_VECTOR_MAX 1024 * 1024
void DiagnosticManager::stressThreadFunc(int stress_time,
                                          const ze_device_handle_t &ze_device,
                                          const ze_driver_handle_t &ze_driver,
                                          std::shared_ptr<xpum_diag_task_info_t> p_task_info,
                                          std::mutex *p_mutex,
                                          std::map<xpum_device_id_t, std::shared_ptr<std::vector<double>>> *p_stress_score_map) {
    
    xpum_device_id_t device_id = p_task_info->deviceId;
    try {
        ze_result_t ret;
        struct ZeWorkGroups workgroup_info;
        int input_value = 4;
        size_t flops_per_work_item = 2048;

        ze_device_properties_t device_properties;
        device_properties.pNext = nullptr;
        device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
        XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeDeviceGetProperties(ze_device, &device_properties));
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeDeviceGetProperties()[" + zeResultErrorCodeStr(ret) + "]");
        }
        ze_device_compute_properties_t device_compute_properties;
        device_compute_properties.pNext = nullptr;
        device_compute_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES;
        XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeDeviceGetComputeProperties(ze_device, &device_compute_properties));
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeDeviceGetComputeProperties()[" + zeResultErrorCodeStr(ret) + "]");
        }
        ze_context_handle_t context;
        contextCreate(ze_driver, &context);
        std::vector<uint8_t> binary_file = loadBinaryFile("ze_int_compute.spv");
        ze_module_handle_t module_handle;
        moduleCreate(context, ze_device, binary_file, &module_handle);
        uint64_t max_work_items = (uint64_t)device_properties.numSlices *
                                    device_properties.numSubslicesPerSlice *
                                    device_properties.numEUsPerSubslice *
                                    device_compute_properties.maxGroupCountX * 2048;

        uint64_t available_memory = device_properties.maxMemAllocSize;
        zes_device_handle_t zes_device = (zes_device_handle_t)ze_device;
        uint32_t mem_module_count = 0;
        ret = zesDeviceEnumMemoryModules(zes_device, &mem_module_count, nullptr);
        if (ret == ZE_RESULT_SUCCESS && mem_module_count > 0) {
            std::vector<zes_mem_handle_t> mems(mem_module_count);
            ret = zesDeviceEnumMemoryModules(zes_device, &mem_module_count, mems.data());
            if (ret == ZE_RESULT_SUCCESS) {
                uint64_t total_free_memory = 0;
                for (auto &mem : mems) {
                    zes_mem_state_t memory_state = {};
                    memory_state.stype = ZES_STRUCTURE_TYPE_MEM_STATE;
                    memory_state.pNext = nullptr;
                    XPUM_ZE_HANDLE_LOCK(mem, ret = zesMemoryGetState(mem, &memory_state));
                    if (ret == ZE_RESULT_SUCCESS) {
                        total_free_memory += memory_state.free;
                    } else {
                        XPUM_LOG_WARN("Stress test: zesMemoryGetState failed for memory module {}: {}",
                                      (void *)mem, zeResultErrorCodeStr(ret));
                    }
                }
                if (total_free_memory > 0) {
                    // Use 90% of free memory to leave headroom for system operations
                    available_memory = std::min(available_memory, (total_free_memory * 9) / 10);
                }
            }
        }

        uint64_t max_number_of_allocated_items = available_memory / sizeof(int);
        uint64_t number_of_work_items = std::min(max_number_of_allocated_items, max_work_items);
        number_of_work_items = setWorkgroups(device_compute_properties, number_of_work_items, &workgroup_info);

        void *device_input_value;
        memoryAlloc(context, ze_device, sizeof(int), 1, &device_input_value);
        void *device_output_buffer;
        memoryAlloc(context, ze_device, static_cast<std::size_t>((number_of_work_items * sizeof(int))), 1, &device_output_buffer);
        ze_command_list_handle_t command_list;
        commandListCreate(context, ze_device, 0, &command_list, ZE_COMMAND_LIST_FLAG_EXPLICIT_ONLY);
        ze_command_queue_handle_t command_queue;
        commandQueueCreate(context, ze_device, 0, 0, &command_queue, ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY);
        commandListAppendMemoryCopy(command_list, device_input_value, &input_value, sizeof(int));
        commandListAppendBarrier(command_list);
        commandListClose(command_list);
        commandQueueExecuteCommandLists(command_queue, command_list);
        commandQueueSynchronize(command_queue);
        commandListReset(command_list);
        ze_kernel_handle_t compute_int_v1;
        setupFunction(module_handle, compute_int_v1, "compute_int_v1", device_input_value, device_output_buffer);

        //runKernel stuff
        kernelSetGroupSize(compute_int_v1, workgroup_info.group_size_x, workgroup_info.group_size_y, workgroup_info.group_size_z);
        ze_group_count_t thread_group_dimensions;
        thread_group_dimensions.groupCountX = workgroup_info.group_count_x;
        thread_group_dimensions.groupCountY = workgroup_info.group_count_y;
        thread_group_dimensions.groupCountZ = workgroup_info.group_count_z;
        commandListAppendLaunchKernel(command_list, compute_int_v1, &thread_group_dimensions);
        commandListClose(command_list);

        #define KERN_TIMES 5
        long double workTime = 0;
        while (true) {
            auto begin = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < KERN_TIMES; i++) {
                commandQueueExecuteCommandLists(command_queue, command_list);
                commandQueueSynchronize(command_queue);
            }
            auto end = std::chrono::high_resolution_clock::now();
            long double timed = std::chrono::duration<long double, 
                std::chrono::nanoseconds::period>(end - begin).count();
            workTime += timed / 1000 / 1000;
            timed /= KERN_TIMES;
            auto iops = calculateGbps(timed, number_of_work_items * flops_per_work_item);
            {
                std::unique_lock<std::mutex> lock(*p_mutex);
                auto iter = p_stress_score_map->find(device_id);
                if (iter == p_stress_score_map->end()) {
                    auto scores = std::make_shared<std::vector<double>>();
                    scores->push_back(iops);
                    (*p_stress_score_map)[device_id] = scores;
                } else {
                    if (iter->second->size() > SCORE_VECTOR_MAX) {
                        iter->second->clear();
                    }
                    iter->second->push_back(iops);
                }
            }
            XPUM_LOG_DEBUG("a stress round is done with score {}", iops);
            if (stress_time != 0 && stress_time <= workTime / (60 * 1000)) {
                break;
            }
        }

        commandListReset(command_list);
        //end of runKernel

        kernelDestroy(compute_int_v1);
        commandListDestroy(command_list);
        commandQueueDestroy(command_queue);
        memoryFree(context, device_input_value);
        memoryFree(context, device_output_buffer);
        moduleDestroy(module_handle);
        contextDestroy(context);

    } catch (BaseException &e) {
        p_task_info->result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        XPUM_LOG_ERROR("Error in stress test with BaseException");
        XPUM_LOG_ERROR(e.what());
    } catch (...) {
        p_task_info->result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL;
        XPUM_LOG_ERROR("Error in stress test");
    }

    p_task_info->finished = true;
    return;
}

xpum_result_t DiagnosticManager::runStress(xpum_device_id_t deviceId, uint32_t stressTime) {
    readConfigFile(XPUM_GLOBAL_CONFIG_FILE);
    readConfigFile(DIAG_CONFIG_THRESHOLD_CONIG_FILE);
    std::unique_lock<std::mutex> lock(this->mutex);
    std::vector<std::shared_ptr<Device>> devices;
    if (deviceId == -1) {
        for (auto task : stress_task_map) {
            if (task.second->finished == false) {
                return XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE;
            }
        }
        for (auto task: diagnostic_task_infos) {
            if (task.second->finished == false) {
                return XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE;
            }
        }
        stress_task_map.clear();
        this->p_device_manager->getDeviceList(devices);
    } else {
        if (this->p_device_manager->getDevice(std::to_string(deviceId)) == 
                nullptr) {
            return XPUM_RESULT_DEVICE_NOT_FOUND;
        }
        if (stress_task_map.find(deviceId) != stress_task_map.end() && 
                stress_task_map.at(deviceId)->finished == false) {
            return XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE;
        }
        if (diagnostic_task_infos.find(deviceId) != 
                diagnostic_task_infos.end() && 
                diagnostic_task_infos.at(deviceId)->finished == false) {
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
                           device->getDriverHandle(), p_task_info, &this->mutex, 
                           &this->stress_score_map);
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
    std::vector<double> allScores;
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
        for (auto iter = stress_score_map.begin(); 
            iter != stress_score_map.end(); iter++) {
            allScores.insert(allScores.end(), iter->second->begin(), 
                iter->second->end());
            iter->second->clear();
        }
    } else {
        if (*count < 1) {
            return XPUM_BUFFER_TOO_SMALL;
        }
        if (stress_task_map.find(deviceId) == stress_task_map.end()) {
            return XPUM_RESULT_DEVICE_NOT_FOUND;
        }
        if (stress_task_map.at(deviceId)->result == xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL) {
            return XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE;
        }
        resultList[0].deviceId = deviceId;
        resultList[0].finished = stress_task_map.at(deviceId)->finished;
        resultList[0].startTime = stress_task_map.at(deviceId)->startTime;
        resultList[0].endTime = stress_task_map.at(deviceId)->endTime;
        *count = 1;
        auto score = stress_score_map.find(deviceId);
        if (score != stress_score_map.end()) {
            allScores.insert(allScores.end(), score->second->begin(), 
                score->second->end());
            score->second->clear();
        }
    }
    // this feature does not consider diff GPU in same server node
    auto device = p_device_manager->getDevice("0");
    if (stress_task_map.size() > 0 && allScores.size() > 0 && 
        device != nullptr) {
        double mean = calculateMean(allScores);
        double variance = calcaulateVariance(allScores);
        int ref = thresholds[device_names[device->getDeviceHandle()]]["REF_INT_GFLOPS"];
        std::string msg = "Integer compute: Mean: " + roundDouble(mean, 3) + " GIOPS. Var: ";
        msg += roundDouble(variance, 3) + ". Ref: " + std::to_string(ref) + " GIOPS.";
        updateMessage(resultList[0].message, msg);
    }
    return XPUM_OK;
}

} // end namespace xpum
