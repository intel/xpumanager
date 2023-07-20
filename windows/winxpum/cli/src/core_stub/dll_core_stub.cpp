/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file dll_core_stub.cpp
 */
#pragma warning(disable: 4996)

#include "dll_core_stub.h"

#include <cstdlib>
#include "xpum_api.h"
#include "xpum_structs.h"
#include "internal_api.h"

namespace xpum::cli {

    DllCoreStub::DllCoreStub(bool initCore) {
        char* env = std::getenv("SPDLOG_LEVEL");
        if (!env) {
            _putenv(const_cast<char*>("SPDLOG_LEVEL=OFF"));
        }
        this->initCore = initCore;
        if (this->initCore)
            xpumInit();
    }

    DllCoreStub::~DllCoreStub() {
        if (this->initCore)
            xpumShutdown();
    }

    std::unique_ptr<nlohmann::json> DllCoreStub::getVersion() {
        auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        const std::string notDetected = "Not Detected";

        (*json)["xpum_version"] = notDetected;
        (*json)["xpum_version_git"] = notDetected;
        (*json)["level_zero_version"] = notDetected;

        const int XPUM_MAX_VERSIONS = 3;

        int count{ XPUM_MAX_VERSIONS };
        xpum_version_info versions[XPUM_MAX_VERSIONS];
        xpum_result_t res = xpumVersionInfo(versions, &count);
        if (res == XPUM_OK) {
            for (int i{ 0 }; i < count; ++i) {
                switch (versions[i].version) {
                case XPUM_VERSION:
                    (*json)["xpum_version"] = versions[i].versionString;
                    break;
                case XPUM_VERSION_GIT:
                    (*json)["xpum_version_git"] = versions[i].versionString;
                    break;
                case XPUM_VERSION_LEVEL_ZERO:
                    (*json)["level_zero_version"] = versions[i].versionString;
                    break;
                default:
                    assert(0);
                }
            }
        }
        return json;
    }

    std::unique_ptr<nlohmann::json> DllCoreStub::getDeviceList() {
        auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

        int count{ XPUM_MAX_NUM_DEVICES };
        xpum_device_basic_info devices[XPUM_MAX_NUM_DEVICES];

        xpum_result_t res = xpumGetDeviceList(devices, &count);
        if (res == XPUM_OK) {
            nlohmann::json deviceJsonList;
            for (int i{ 0 }; i < count; ++i) {
                auto deviceJson = nlohmann::json();
                deviceJson["device_id"] = devices[i].deviceId;
                deviceJson["device_type"] = devices[i].type == 0 ? "GPU" : "Unknown";
                deviceJson["uuid"] = devices[i].uuid;
                deviceJson["device_name"] = devices[i].deviceName;
                deviceJson["pci_device_id"] = devices[i].PCIDeviceId;
                deviceJson["pci_bdf_address"] = devices[i].PCIBDFAddress;
                deviceJson["vendor_name"] = devices[i].VendorName;
                deviceJsonList.push_back(deviceJson);
            }
            (*json)["device_list"] = deviceJsonList;
        }
        else {
            switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
            }
            (*json)["errno"] = errorNumTranslate(res);
        }

        return json;
    }

    std::unique_ptr<nlohmann::json> DllCoreStub::getDeviceProperties(int deviceId, std::string username, std::string password) {
        auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        xpum_device_properties_t data;
        auto res = xpumGetDeviceProperties(deviceId, &data);
        if (res == XPUM_OK) {
            for (int i = 0; i < data.propertyLen; i++) {
                auto& p = data.properties[i];
                std::string name = xpum::getXpumDevicePropertyNameString(p.name);
                std::transform(name.begin(), name.end(), name.begin(),
                    [](unsigned char c) { return std::tolower(c); });
                (*json)[name] = p.value;
            }
        }
        else {
            switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                (*json)["error"] = "Device not found";
                break;
            default:
                (*json)["error"] = "Error";
                break;
            }
            (*json)["errno"] = errorNumTranslate(res);
            return json;
        }

        (*json)["device_id"] = deviceId;

        return json;
    }

    std::unique_ptr<nlohmann::json> DllCoreStub::getDeviceProperties(const char* bdf, std::string username, std::string password) {
        xpum_device_id_t deviceId = -1;
        // No need to check return value as "-1" covers the failure case
        xpumGetDeviceIdByBDF(bdf, &deviceId);
        return getDeviceProperties(deviceId, username, password);
    }

    std::unique_ptr<nlohmann::json> DllCoreStub::getDeivceIdByBDF(const char* bdf, int* deviceId) {
        auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        xpum_result_t res = xpumGetDeviceIdByBDF(bdf, deviceId);
        if (res != XPUM_OK) {
            switch (res) {
                case XPUM_RESULT_DEVICE_NOT_FOUND:
                    (*json)["error"] = "device not found";
                    (*json)["errno"] = errorNumTranslate(res);
                    break;
                default:
                    (*json)["error"] = "Error";
                    (*json)["errno"] = errorNumTranslate(res);
                    break;
            }
        }
        return json;
    }

    std::string DllCoreStub::getSerailNumberIPMI(int deviceId) {
        char serialNumber[XPUM_MAX_STR_LENGTH];
        char amcFwVersion[XPUM_MAX_STR_LENGTH];
        auto res = xpumGetSerialNumberAndAmcFwVersion(deviceId, "", "", serialNumber, amcFwVersion);
        if (res == XPUM_OK) {
            return std::string(serialNumber);
        }
        else {
            return std::string();
        }
    }

    static std::string getAmcFwErrMsg() {
        // get error message
        int count = 0;
        auto res = xpumGetAMCFirmwareVersionsErrorMsg(nullptr, &count);
        if (res != XPUM_OK)
            return "";
        char buffer[256];
        res = xpumGetAMCFirmwareVersionsErrorMsg(buffer, &count);
        if (res != XPUM_OK)
            return "";
        return std::string(buffer);
    }

    std::unique_ptr<nlohmann::json> DllCoreStub::getAMCFirmwareVersions(std::string username, std::string password) {
        auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        int count = 511;
        std::vector<xpum_amc_fw_version_t> versions(count);
        auto res = xpumGetAMCFirmwareVersions(versions.data(), &count, username.c_str(), password.c_str());
        if (res != XPUM_OK) {
            switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Fail to get AMC firmware versions";
            }
            (*json)["errno"] = errorNumTranslate(res);
            return json;
        }
        for (int i = 0; i < count; i++) {
            (*json)["amc_fw_version"].push_back(versions.at(i).version);
        }
        return json;
    }

    std::string DllCoreStub::getRedfishAmcWarnMsg() {
        return "";
    }

    static std::string getFlashFwErrMsg() {
        // get error message
        int count;
        xpumGetFirmwareFlashErrorMsg(nullptr, &count);
        char *buffer = new char[count];
        xpumGetFirmwareFlashErrorMsg(buffer, &count);
        std::string err(buffer);
        delete [] buffer;
        return err;
    }

    std::unique_ptr<nlohmann::json> DllCoreStub::runFirmwareFlash(int deviceId, unsigned int type, const std::string& filePath, bool force) {
        auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        xpum_firmware_flash_job job;
        job.type = (xpum_firmware_type_t)type;
        job.filePath = filePath.c_str();
        std::string errorMsg;
        auto res = xpumRunFirmwareFlashEx(deviceId, &job, nullptr, nullptr, force);
        if (res == XPUM_OK) {
            (*json)["result"] = "OK";
        } else {
            switch (res) {
                case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC:
                    (*json)["error"] = "Can't find the AMC device. AMC firmware update just works for ATS-P or ATS-M card (ATS-P AMC firmware version is 3.3.0 or later. ATS-M AMC firmware version is 3.6.3 or later) on Intel M50CYP server (BMC firmware version is 2.82 or later) so far.";
                    break;
                case xpum_result_t::XPUM_UPDATE_FIRMWARE_MODEL_INCONSISTENCE:
                    (*json)["error"] = "Device models are inconsistent, failed to upgrade all.";
                    break;
                case xpum_result_t::XPUM_UPDATE_FIRMWARE_IMAGE_FILE_NOT_FOUND:
                    (*json)["error"] = "Firmware image not found.";
                    break;
                case xpum_result_t::XPUM_UPDATE_FIRMWARE_IGSC_NOT_FOUND:
                    (*json)["error"] = "Igsc tool doesn't exit";
                    break;
                case xpum_result_t::XPUM_RESULT_DEVICE_NOT_FOUND:
                    (*json)["error"] = "Device not found.";
                    break;
                case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_ALL:
                    if (type == XPUM_DEVICE_FIRMWARE_GFX)
                        (*json)["error"] = "Updating GFX firmware on all devices is not supported";
                    else if (type == XPUM_DEVICE_FIRMWARE_GFX_DATA)
                        (*json)["error"] = "Updating GFX_DATA firmware on all devices is not supported";
                    else
                        (*json)["error"] = "Updating GFX_PSCBIN firmware on all devices is not supported";
                    break;
                case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC_SINGLE:
                    (*json)["error"] = "Updating AMC firmware on single device is not supported";
                    break;
                case xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING:
                    (*json)["error"] = "Firmware update task already running.";
                    break;
                case xpum_result_t::XPUM_UPDATE_FIRMWARE_INVALID_FW_IMAGE:
                    (*json)["error"] = "The image file is not a right FW image file.";
                    break;
                case xpum_result_t::XPUM_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE:
                    (*json)["error"] = "The image file is a right FW image file, but not proper for the target GPU.";
                    break;
                case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_DATA:
                    (*json)["error"] = "The device doesn't support GFX_DATA firmware update";
                    break;
                case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_PSC:
                    (*json)["error"] = "The device doesn't support PSCBIN firmware update";
                    break;
                case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_PSC_IGSC:
                    (*json)["error"] = "Installed igsc doesn't support PSCBIN firmware update";
                    break;
                default:
                    (*json)["error"] = "Unknown error.";
                    break;
            }
            (*json)["errno"] = errorNumTranslate(res);
        }
        
        if (job.type != XPUM_DEVICE_FIRMWARE_AMC) {
            errorMsg = getFlashFwErrMsg();
            if (errorMsg.size()) {
                (*json)["error"] = errorMsg;
                (*json)["errno"] = errorNumTranslate(res);
                return json;
            }
        }
        return json;
    }

    std::unique_ptr<nlohmann::json> DllCoreStub::getFirmwareFlashResult(int deviceId, unsigned int type) {
        auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        xpum_firmware_flash_task_result_t result;
        auto res = xpumGetFirmwareFlashResult(deviceId, (xpum_firmware_type_t)type, &result);
        if (res != XPUM_OK) {
            switch (res) {
                default:
                    (*json)["error"] = "Fail to get firmware flash result.";
                    break;
            }
            (*json)["errno"] = errorNumTranslate(res);
            return json;
        }
        switch (result.result) {
            case XPUM_DEVICE_FIRMWARE_FLASH_OK:
                (*json)["result"] = "OK";
                break;
            case XPUM_DEVICE_FIRMWARE_FLASH_ERROR:
                (*json)["result"] = "FAILED";
                break;
            case XPUM_DEVICE_FIRMWARE_FLASH_UNSUPPORTED:
                (*json)["result"] = "UNSUPPORTED";
                break;
            case XPUM_DEVICE_FIRMWARE_FLASH_ONGOING:
                (*json)["result"] = "ONGOING";
                (*json)["percentage"] = result.percentage;
                break;
            default:
                (*json)["result"] = "UNSUPPORTED";
                break;
        }
        return json;
    }

    std::vector<int> DllCoreStub::getSiblingDevices(int deviceId) {
        std::vector<int> ret;
        uint32_t count = 0;
        xpum_device_id_t devices[XPUM_MAX_NUM_DEVICES];
        auto res = xpumGetSiblingDevices(deviceId, devices, &count);
        if (res == XPUM_OK) {
            for (int i = 0; i < count; i++) {
                ret.push_back(devices[i]);
            }
        }
        return ret;
    }

    static int32_t getCliScale(xpum_stats_type_t metricsType) {
        switch (metricsType) {
            case XPUM_STATS_ENERGY:
                return 1000;
            case XPUM_STATS_MEMORY_USED:
                return 1048576;
            default:
                return 1;
        }
    }

    std::unique_ptr<nlohmann::json> DllCoreStub::getRealtimeMetrics(int deviceId, bool enableScale) {
        auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        xpum_device_id_t xpum_device_id = deviceId;
        uint32_t count = 5;
        xpum_device_realtime_metrics_t dataList[5];
        xpum_result_t res = xpumGetRealtimeMetrics(xpum_device_id, dataList, &count);
        if (res != XPUM_OK || count < 0) {
            switch (res) {
                case XPUM_RESULT_DEVICE_NOT_FOUND:
                    (*json)["error"] = "device not found";
                    (*json)["errno"] = errorNumTranslate(res);
                    break;
                case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                    (*json)["error"] = "Level Zero Initialization Error";
                    (*json)["errno"] = errorNumTranslate(res);
                    break;
                default:
                    (*json)["error"] = "Error";
                    (*json)["errno"] = errorNumTranslate(res);
                    break;
            }
            return json;
        }

        std::vector<nlohmann::json> deviceLevelStatsDataList;
        std::vector<nlohmann::json> tileLevelStatsDataList;

    for (uint32_t i = 0; i < count; i++) {
            xpum_device_realtime_metrics_t& metrics_info = dataList[i];
            std::vector<nlohmann::json> dataList;
            for (int j = 0; j < metrics_info.count; j++) {
                xpum_device_realtime_metric_t& metric_data = metrics_info.dataList[j];
                auto tmp = nlohmann::json();
                xpum_realtime_metric_type_t metricsType = metric_data.metricsType;
                tmp["metrics_type"] = metricsTypeToString(metricsType);
                int32_t cliScale = getCliScale(metricsType);
                int32_t scale = enableScale ? metric_data.scale * cliScale : metric_data.scale;
                if (scale == 1) {
                    tmp["value"] = metric_data.value;
                } else {
                    tmp["value"] = (double)metric_data.value / scale;
                }
                dataList.push_back(tmp);
            }
            if (metrics_info.isTileData) {
                auto tmp = nlohmann::json();
                tmp["tile_id"] = metrics_info.tileId;
                tmp["data_list"] = dataList;
                tileLevelStatsDataList.push_back(tmp);
            } else {
                deviceLevelStatsDataList.insert(deviceLevelStatsDataList.end(), dataList.begin(), dataList.end());
            }
        }

        (*json)["device_level"] = deviceLevelStatsDataList;
        if (tileLevelStatsDataList.size() > 0)
            (*json)["tile_level"] = tileLevelStatsDataList;

        (*json)["device_id"] = deviceId;

        return json;
    }

    std::unique_ptr<nlohmann::json> DllCoreStub::getDeviceConfig(int deviceId, int tileId) {
        xpum_result_t res;
        auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        std::vector<nlohmann::json> tileJsonList;
        xpum_device_basic_info devices[XPUM_MAX_NUM_DEVICES];
        int count{XPUM_MAX_NUM_DEVICES};
        xpum_device_tile_id_t tileIdList[8];
        uint32_t freqCount{16};
        uint32_t subDeviceCount{8};
        char bdfAddress[255];
        xpum_frequency_range_t freqRange[16];
        xpum_power_limits_t powerLimit;
        
        res = xpumGetDeviceList(devices, &count);
        if (res != XPUM_OK) {
            (*json)["error"] = "fail to get device list";
            return json;
        }
        if (deviceId < 0 || deviceId >= count) {
            (*json)["error"] = "invalid device id";
            return json;
        }
        (*json)["device_id"] = deviceId;
       
        xpum_device_properties_t properties;
        res = xpumGetDeviceProperties(deviceId, &properties);
        if (res != XPUM_OK) {
            switch (res) {
                case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                    (*json)["error"] = "Level Zero Initialization Error";
                    break;
                default:
                    (*json)["error"] = "Error";
                    break;
            }
            (*json)["errno"] = errorNumTranslate(res);
            return json;
        }

        for (int i = 0; i < properties.propertyLen; i++) {
            auto& prop = properties.properties[i];
            if (prop.name != XPUM_DEVICE_PROPERTY_NUMBER_OF_TILES) {
                continue;
            }
            subDeviceCount = atoi(prop.value);
            break;
        }

        for (uint32_t i = 0; i < subDeviceCount; i++) {
            tileIdList[i] = i;
        } 

        if (subDeviceCount > 0 && tileId >= (int)subDeviceCount) {
            (*json)["error"] = "invalid tile id";
            return json;
        }

        if (subDeviceCount == 0) {
            if (tileId != 0 && tileId != -1) {
                (*json)["error"] = "invalid tile id";
                return json;
            }
            tileIdList[0] = 0;
            subDeviceCount = 1;
        } else {
            if (tileId != -1) {
                tileIdList[0] = tileId;
                subDeviceCount = 1;
            }
        }

        res = xpumGetDevicePowerLimits(deviceId, 0, &powerLimit);
        if (res != XPUM_OK) {
            (*json)["error"] = "fail to get device power limit";
            return json;
        }
        if (!powerLimit.sustained_limit.enabled) {
            (*json)["error"] = "unsupported feature or insufficient privilege";
            return json;
        }
        (*json)["power_limit"] = std::to_string(powerLimit.sustained_limit.power);

        xpum_power_prop_data_t powerRangeArray[32];
        uint32_t powerRangeCount = 32;
        res = xpumGetDevicePowerProps(deviceId, powerRangeArray, &powerRangeCount);
        (*json)["power_vaild_range"] = "1 to " + std::to_string(powerRangeArray[0].max_limit);
        
        bool available, configurable;
        xpum_ecc_state_t current, pending;
        xpum_ecc_action_t action;

        res = xpumGetEccState(deviceId, &available, &configurable, &current, &pending, &action); 
        if (res != XPUM_OK) {
            (*json)["error"] = "fail to get device Ecc state";
            return json;
        }
        (*json)["memory_ecc_current_state"] = eccStateToString(current);
        (*json)["memory_ecc_pending_state"] = eccStateToString(pending);
        
        freqCount = subDeviceCount;
        for (int i = 0; i < freqCount; i++) {
            freqRange[i].subdevice_Id = tileIdList[i];
        }
        res = xpumGetDeviceFrequencyRanges(deviceId, freqRange, &freqCount);

        for (int i = 0; i < subDeviceCount; ++i) {
            auto tileJson = nlohmann::json();
            tileJson["tile_id"] = tileIdList[i];
            for (int j = 0; j < freqCount; j++) {
                if (tileIdList[i] == freqRange[j].subdevice_Id) {
                    double dataArray[255];
                    uint32_t freqCount = 255;
                    tileJson["min_frequency"] = int(freqRange[j].min);
                    tileJson["max_frequency"] = int(freqRange[j].max);

                    xpumGetFreqAvailableClocks(deviceId, tileIdList[i], dataArray, &freqCount);
                    std::string str = std::to_string((int)dataArray[0]);
                    for (uint32_t i = 1; i < freqCount; i++) {
                        str = str + ", " + std::to_string((int)(dataArray[i]));
                    }
                    tileJson["gpu_frequency_valid_options"] = str;
                    tileJson["tile_id"] = std::to_string(deviceId) + "/" + std::to_string(tileIdList[i]);
                }
            }
            tileJsonList.push_back(tileJson);
        }
        (*json)["tile_config_data"] = tileJsonList;
        return json;
    }

    std::string DllCoreStub::eccStateToString(uint8_t state) {
        if (state == 1) {
            return "enabled";
        } else if (state == 0) {
            return "disabled";
        } else {
            return "unavailable";
        }
    }

    std::string DllCoreStub::eccStateToString(xpum_ecc_state_t state) {
        if (state == XPUM_ECC_STATE_ENABLED) {
            return "enabled";
        } else if (state == XPUM_ECC_STATE_DISABLED) {
            return "disabled";
        } else {
            return "unavailable";
        }
    }

    std::unique_ptr<nlohmann::json> DllCoreStub::setDevicePowerlimit(int deviceId, int tileId, int powerLimit) {
        auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        xpum_result_t res;
        xpum_power_sustained_limit_t sustained_limit;
        sustained_limit.power = powerLimit;
        sustained_limit.enabled = true;

        xpum_power_prop_data_t powerRangeArray[32];
        uint32_t powerRangeCount = 32;
        res = xpumGetDevicePowerProps(deviceId, powerRangeArray, &powerRangeCount);
        if (res == XPUM_OK) {
            if (powerLimit < 1 || (powerRangeArray[0].max_limit > 0  && powerLimit > powerRangeArray[0].max_limit)) {
                (*json)["error"] = "Invalid power limit value";
                return json;
            }
        }

        res = xpumSetDevicePowerSustainedLimits((xpum_device_id_t)deviceId, 0, sustained_limit);
        if (res != XPUM_OK) {
            (*json)["error"] = "unsupported feature or setting failure";
        } else {
            (*json)["status"] = "OK";
        }
               
        return json;
    }

    std::unique_ptr<nlohmann::json> DllCoreStub::setDeviceFrequencyRange(int deviceId, int tileId, int minFreq, int maxFreq) {
        auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        xpum_result_t res;
        xpum_frequency_range_t frequency;
        frequency.subdevice_Id = tileId;
        frequency.min = minFreq;
        frequency.max = maxFreq;
        res = xpumSetDeviceFrequencyRange(deviceId, frequency);
        if (res != XPUM_OK) {
            (*json)["error"] = "unsupported feature or setting failure";
        } else {
            (*json)["status"] = "OK";
        }
        return json;
    }

    std::unique_ptr<nlohmann::json> DllCoreStub::setMemoryEccState(int deviceId, bool enabled) {
        auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        xpum_result_t res;
        bool available, configurable;
        xpum_ecc_state_t current, pending;
        xpum_ecc_action_t action;
        xpum_ecc_state_t newState;
        newState = enabled == true ? XPUM_ECC_STATE_ENABLED : XPUM_ECC_STATE_DISABLED;
        res = xpumSetEccState(deviceId, newState, &available, &configurable, &current, &pending, &action);
        if (res != XPUM_OK) {
            (*json)["error"] = "unsupported feature or setting failure";
        } else {
            res = xpumGetEccState(deviceId, &available, &configurable, &current, &pending, &action);
            if (res != XPUM_OK) {
                (*json)["error"] = "fail to get device Ecc state";
                return json;
            }
            (*json)["memory_ecc_current_state"] = eccStateToString(current);
            (*json)["memory_ecc_pending_state"] = eccStateToString(pending);
            (*json)["status"] = "OK";
        }
        return json;
    }

 }