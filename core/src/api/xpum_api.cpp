/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file xpum_api.cpp
 */

#include "xpum_api.h"

#include <algorithm>
#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>
#include <dlfcn.h>
#include <math.h>
#include <regex>

#include "api_types.h"
#include "core/core.h"
#include "device/device.h"
#include "device/memoryEcc.h"
#include "device/power.h"
#include "infrastructure/configuration.h"
#include "infrastructure/device_process.h"
#include "infrastructure/device_util_by_proc.h"
#include "infrastructure/device_property.h"
#include "infrastructure/exception/level_zero_initialization_exception.h"
#include "infrastructure/version.h"
#include "infrastructure/perf_measurement_data.h"
#include "internal_api.h"
#include "ext-include/igsc_lib.h"

namespace xpum {

extern const char *getXpumDevicePropertyNameString(xpum_device_property_name_t name) {
    switch (name) {
        case XPUM_DEVICE_PROPERTY_DEVICE_TYPE:
            return "DEVICE_TYPE";
        case XPUM_DEVICE_PROPERTY_DEVICE_NAME:
            return "DEVICE_NAME";
        case XPUM_DEVICE_PROPERTY_VENDOR_NAME:
            return "VENDOR_NAME";
        case XPUM_DEVICE_PROPERTY_UUID:
            return "UUID";
        case XPUM_DEVICE_PROPERTY_PCI_DEVICE_ID:
            return "PCI_DEVICE_ID";
        case XPUM_DEVICE_PROPERTY_PCI_VENDOR_ID:
            return "PCI_VENDOR_ID";
        case XPUM_DEVICE_PROPERTY_PCI_BDF_ADDRESS:
            return "PCI_BDF_ADDRESS";
        case XPUM_DEVICE_PROPERTY_DRM_DEVICE:
            return "DRM_DEVICE";
        case XPUM_DEVICE_PROPERTY_PCI_SLOT:
            return "PCI_SLOT";
        case XPUM_DEVICE_PROPERTY_PCIE_GENERATION:
            return "PCIE_GENERATION";
        case XPUM_DEVICE_PROPERTY_PCIE_MAX_LINK_WIDTH:
            return "PCIE_MAX_LINK_WIDTH";
        case XPUM_DEVICE_PROPERTY_DEVICE_STEPPING:
            return "DEVICE_STEPPING";
        case XPUM_DEVICE_PROPERTY_DRIVER_VERSION:
            return "DRIVER_VERSION";
        case XPUM_DEVICE_PROPERTY_GFX_FIRMWARE_NAME:
            return "GFX_FIRMWARE_NAME";
        case XPUM_DEVICE_PROPERTY_GFX_FIRMWARE_VERSION:
            return "GFX_FIRMWARE_VERSION";
        case XPUM_DEVICE_PROPERTY_GFX_DATA_FIRMWARE_NAME:
            return "GFX_DATA_FIRMWARE_NAME";
        case XPUM_DEVICE_PROPERTY_GFX_DATA_FIRMWARE_VERSION:
            return "GFX_DATA_FIRMWARE_VERSION";
        case XPUM_DEVICE_PROPERTY_AMC_FIRMWARE_NAME:
            return "AMC_FIRMWARE_NAME";
        case XPUM_DEVICE_PROPERTY_AMC_FIRMWARE_VERSION:
            return "AMC_FIRMWARE_VERSION";
        case XPUM_DEVICE_PROPERTY_SERIAL_NUMBER:
            return "SERIAL_NUMBER";
        case XPUM_DEVICE_PROPERTY_CORE_CLOCK_RATE_MHZ:
            return "CORE_CLOCK_RATE_MHZ";
        case XPUM_DEVICE_PROPERTY_MEMORY_PHYSICAL_SIZE_BYTE:
            return "MEMORY_PHYSICAL_SIZE_BYTE";
        case XPUM_DEVICE_PROPERTY_MEMORY_FREE_SIZE_BYTE:
            return "MEMORY_FREE_SIZE_BYTE";
        case XPUM_DEVICE_PROPERTY_MAX_MEM_ALLOC_SIZE_BYTE:
            return "MAX_MEM_ALLOC_SIZE_BYTE";
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_MEMORY_CHANNELS:
            return "NUMBER_OF_MEMORY_CHANNELS";
        case XPUM_DEVICE_PROPERTY_MEMORY_BUS_WIDTH:
            return "MEMORY_BUS_WIDTH";
        case XPUM_DEVICE_PROPERTY_MAX_HARDWARE_CONTEXTS:
            return "MAX_HARDWARE_CONTEXTS";
        case XPUM_DEVICE_PROPERTY_MAX_COMMAND_QUEUE_PRIORITY:
            return "MAX_COMMAND_QUEUE_PRIORITY";
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_EUS:
            return "NUMBER_OF_EUS";
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_TILES:
            return "NUMBER_OF_TILES";
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_SLICES:
            return "NUMBER_OF_SLICES";
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_SUB_SLICES_PER_SLICE:
            return "NUMBER_OF_SUB_SLICES_PER_SLICE";
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_EUS_PER_SUB_SLICE:
            return "NUMBER_OF_EUS_PER_SUB_SLICE";
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_THREADS_PER_EU:
            return "NUMBER_OF_THREADS_PER_EU";
        case XPUM_DEVICE_PROPERTY_PHYSICAL_EU_SIMD_WIDTH:
            return "PHYSICAL_EU_SIMD_WIDTH";
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_MEDIA_ENGINES:
            return "NUMBER_OF_MEDIA_ENGINES";
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_MEDIA_ENH_ENGINES:
            return "NUMBER_OF_MEDIA_ENH_ENGINES";
        case XPUM_DEVICE_PROPERTY_FABRIC_PORT_NUMBER:
            return "NUMBER_OF_FABRIC_PORTS";
        case XPUM_DEVICE_PROPERTY_FABRIC_PORT_MAX_SPEED:
            return "MAX_FABRIC_PORT_SPEED";
        case XPUM_DEVICE_PROPERTY_FABRIC_PORT_LANES_NUMBER:
            return "NUMBER_OF_LANES_PER_FABRIC_PORT";
        case XPUM_DEVICE_PROPERTY_LINUX_KERNEL_VERSION:
            return "KERNEL_VERSION";
        default:
            return "";
    }
}

xpum_result_t validateDeviceId(xpum_device_id_t deviceId) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    auto pDevice = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (pDevice == nullptr)
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    return XPUM_OK;
}

xpum_result_t validateDeviceIdAndTileId(xpum_device_id_t deviceId, xpum_device_tile_id_t tileId) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    auto pDevice = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (pDevice == nullptr)
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    Property prop;
    pDevice->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_TILES, prop);
    if (tileId < 0 || tileId >= prop.getValueInt())
        return XPUM_RESULT_TILE_NOT_FOUND;
    return XPUM_OK;
}

xpum_result_t xpumGetEngineCount(xpum_device_id_t deviceId,
                                 xpum_device_tile_id_t tileId,
                                 xpum_engine_type_t type,
                                 uint32_t *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    auto pDevice = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (pDevice == nullptr)
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    *count = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId))->getEngineCount(tileId, Utility::toZESEngineType(type));
    return XPUM_OK;
}

std::vector<EngineCount> getDeviceAndTileEngineCount(xpum_device_id_t deviceId) {
    auto res = std::vector<EngineCount>();
    auto pDevice = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (pDevice == nullptr)
        return res;
    Property prop;
    pDevice->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_TILES, prop);
    auto tileCount = prop.getValueInt();
    if (tileCount == 1) {
        EngineCount ec;
        ec.isTileLevel = false;
        for (int engineType = 0; engineType < XPUM_ENGINE_TYPE_UNKNOWN; engineType++) {
            int c = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId))->getEngineCount(-1, Utility::toZESEngineType((xpum_engine_type_t)engineType));
            EngineCountData data;
            data.count = c;
            data.engineType = (xpum_engine_type_t)engineType;
            ec.engineCountList.push_back(data);
        }
        res.push_back(ec);
    } else {
        for (int tileId = 0; tileId < tileCount; tileId++) {
            EngineCount ec;
            ec.isTileLevel = true;
            ec.tileId = tileId;
            for (int engineType = 0; engineType < XPUM_ENGINE_TYPE_UNKNOWN; engineType++) {
                int c = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId))->getEngineCount(tileId, Utility::toZESEngineType((xpum_engine_type_t)engineType));
                EngineCountData data;
                data.count = c;
                data.engineType = (xpum_engine_type_t)engineType;
                ec.engineCountList.push_back(data);
            }
            res.push_back(ec);
        }
    }
    return res;
}

std::vector<FabricCount> getDeviceAndTileFabricCount(xpum_device_id_t deviceId) {
    auto res = std::vector<FabricCount>();
    auto pDevice = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (pDevice == nullptr)
        return res;

    uint32_t count;
    Core::instance().getDataLogic()->getFabricLinkInfo(deviceId, nullptr, &count);
    if (count <= 0)
        return res;
    std::vector<FabricLinkInfo> info(count);
    Core::instance().getDataLogic()->getFabricLinkInfo(deviceId, info.data(), &count);

    Property prop;
    pDevice->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_TILES, prop);
    uint32_t tileCount = prop.getValueInt();
    if (tileCount == 1) {
        FabricCount fc;
        fc.isTileLevel = false;
        for (auto d : info) {
            fc.dataList.push_back(d);
        }
        res.push_back(fc);
    } else {
        for (uint32_t tileId = 0; tileId < tileCount; tileId++) {
            FabricCount fc;
            fc.isTileLevel = true;
            fc.tileId = tileId;
            for (auto d : info) {
                if (d.tile_id == tileId)
                    fc.dataList.push_back(d);
            }
            if (fc.dataList.size() > 0)
                res.push_back(fc);
        }
    }
    return res;
}

xpum_result_t xpumInit() {
    try {
        Logger::init();
        XPUM_LOG_INFO("XPU Manager:\t{}", Version::getVersion());
        XPUM_LOG_INFO("Build:\t\t{}", Version::getVersionGit());
        XPUM_LOG_INFO("Level Zero:\t{}", Version::getZeLibVersion());
        Core::instance().init();
    } catch (LevelZeroInitializationException &e) {
        XPUM_LOG_ERROR("xpumInit LevelZeroInitializationException");
        XPUM_LOG_ERROR("Failed to init xpum core: {}", e.what());
        Core::instance().setZeInitialized(false);
        return XPUM_LEVEL_ZERO_INITIALIZATION_ERROR;
    } catch (BaseException &e) {
        XPUM_LOG_ERROR("Failed to init xpum core: {}", e.what());
        return XPUM_GENERIC_ERROR;
    } catch (std::exception &e) {
        XPUM_LOG_ERROR("Failed to init xpum core: {}", e.what());
        return XPUM_GENERIC_ERROR;
    }
    Core::instance().setZeInitialized(true);

    XPUM_LOG_INFO("xpumd is providing services");
    return XPUM_OK;
}

xpum_result_t xpumShutdown() {
    Core::instance().close();
    XPUM_LOG_INFO("xpumd stopped");
    return XPUM_OK;
}

xpum_result_t xpumVersionInfo(xpum_version_info versionInfoList[], int *count) {
    if (!versionInfoList) {
        *count = 3;
        return XPUM_OK;
    }

    if (*count < 3) {
        *count = 3;
        return XPUM_BUFFER_TOO_SMALL;
    }

    std::string xpumVersion = Version::getVersion();
    std::string xpumVersionGit = Version::getVersionGit();
    std::string levelZeroVersion = Version::getZeLibVersion();

    versionInfoList[0].version = XPUM_VERSION;
    xpumVersion.copy(versionInfoList[0].versionString, xpumVersion.size());
    versionInfoList[0].versionString[xpumVersion.size()] = '\0';

    versionInfoList[1].version = XPUM_VERSION_GIT;
    xpumVersionGit.copy(versionInfoList[1].versionString, xpumVersionGit.size());
    versionInfoList[1].versionString[xpumVersionGit.size()] = '\0';

    versionInfoList[2].version = XPUM_VERSION_LEVEL_ZERO;
    levelZeroVersion.copy(versionInfoList[2].versionString, levelZeroVersion.size());
    versionInfoList[2].versionString[levelZeroVersion.size()] = '\0';

    return XPUM_OK;
}

xpum_result_t xpumGetDeviceList(xpum_device_basic_info deviceList[], int *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    if (Core::instance().getDeviceManager() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }

    std::vector<std::shared_ptr<Device>> devices;
    Core::instance().getDeviceManager()->getDeviceList(devices);
    const int deviceCount = devices.size();
    if (deviceList == nullptr) {
        *count = deviceCount;
        return XPUM_OK;
    }

    if (deviceCount > *count) {
        return XPUM_BUFFER_TOO_SMALL;
    }

    for (int i = 0; i < deviceCount; i++) {
        auto &p_device = devices[i];
        auto &info = deviceList[i];
        info.deviceId = stoi(p_device->getId());
        info.type = GPU;
        std::vector<Property> properties;
        p_device->getProperties(properties);

        for (Property &prop : properties) {
            auto internal_name = prop.getName();
            std::string value = prop.getValue();
            switch (internal_name) {
                case XPUM_DEVICE_PROPERTY_INTERNAL_UUID:
                    value.copy(info.uuid, value.size());
                    info.uuid[value.size()] = 0;
                    break;
                case XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_NAME:
                    value.copy(info.deviceName, value.size());
                    info.deviceName[value.size()] = 0;
                    break;
                case XPUM_DEVICE_PROPERTY_INTERNAL_PCI_DEVICE_ID:
                    value.copy(info.PCIDeviceId, value.size());
                    info.PCIDeviceId[value.size()] = 0;
                    break;
                case XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS:
                    value.copy(info.PCIBDFAddress, value.size());
                    info.PCIBDFAddress[value.size()] = 0;
                    break;
                case XPUM_DEVICE_PROPERTY_INTERNAL_VENDOR_NAME:
                    value.copy(info.VendorName, value.size());
                    info.VendorName[value.size()] = 0;
                    break;
                case XPUM_DEVICE_PROPERTY_INTERNAL_DRM_DEVICE:
                    value.copy(info.drmDevice, value.size());
                    info.drmDevice[value.size()] = 0;
                    break;
                default:
                    break;
            }
        }
    }
    *count = deviceCount;

    return XPUM_OK;
}

xpum_result_t xpumGetAMCFirmwareVersions(xpum_amc_fw_version_t versionList[], int *count, const char *username, const char *password) {
    AmcCredential credential;
    credential.username = std::string(username);
    credential.password = std::string(password);
    std::vector<std::string> versions;
    auto result = Core::instance().getFirmwareManager()->getAMCFirmwareVersions(versions, credential);
    if (result != XPUM_OK) 
        return result;
    if (versionList == nullptr) {
        *count = versions.size();
        return XPUM_OK;
    }
    if (*count < (int)versions.size()) {
        return XPUM_BUFFER_TOO_SMALL;
    }
    *count = versions.size();
    for (int i = 0; i < *count; i++) {
        std::string version = versions[i];
        std::strcpy(versionList[i].version, version.c_str());
    }
    return XPUM_OK;
}

xpum_result_t xpumGetAMCFirmwareVersionsErrorMsg(char *buffer, int *count) {
    auto errMsg = Core::instance().getFirmwareManager()->getGetAmcFwErrMsg();
    if (buffer == nullptr) {
        *count = errMsg.length() + 1;
        return XPUM_OK;
    }
    if (*count < (int) errMsg.length() + 1) {
        return XPUM_BUFFER_TOO_SMALL;
    }
    std::strcpy(buffer, errMsg.c_str());
    buffer[errMsg.length() + 1] = '\0';
    return XPUM_OK;
}

xpum_result_t xpumGetSerialNumberAndAmcFwVersion(xpum_device_id_t deviceId,
                                  const char *username,
                                  const char *password,
                                  char serialNumber[XPUM_MAX_STR_LENGTH],
                                  char amcFwVersion[XPUM_MAX_STR_LENGTH]) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    if (Core::instance().getDeviceManager() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }

    res = validateDeviceId(deviceId);
    if (res != XPUM_OK)
        return res;

    auto pDevice = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));

    std::vector<Property> properties;
    pDevice->getProperties(properties);

    std::string pciSlot;

    for (size_t i = 0; i < properties.size(); i++) {
        auto &prop = properties[i];
        xpum_device_internal_property_name_t name = prop.getName();
        if (name == XPUM_DEVICE_PROPERTY_INTERNAL_PCI_SLOT) {
            pciSlot = prop.getValue();
            break;
        }
    }

    auto systemInfo = Core::instance().getDeviceManager()->getSystemInfo();

    std::vector<SlotSerialNumberAndFwVersion> serialNumberList;
    Core::instance().getFirmwareManager()->getAMCSlotSerialNumbers({username, password}, serialNumberList);

    int systemSlotId = -1;

    if (systemInfo.manufacturer.compare("Supermicro") == 0) {
        if (systemInfo.productName.compare("SYS-420GP-TNR") == 0) {
            // SMC 4U
            std::regex pattern("SLOT(\\d+)\\s", std::regex_constants::icase);
            std::smatch sm;
            if (std::regex_search(pciSlot, sm, pattern)) {
                int riserSlotId = std::stoi(sm[1].str());
                systemSlotId = riserSlotId;
            }
        } else if (systemInfo.productName.compare("SYS-620C-TN12R") == 0) {
            // SMC 2U
            if (pciSlot.find("RSC-D2-668G4") != pciSlot.npos) {
                std::regex pattern("RSC-D2-668G4\\sSLOT(\\d+)\\s", std::regex_constants::icase);
                std::smatch sm;
                if (std::regex_search(pciSlot, sm, pattern)) {
                    int riserSlotId = std::stoi(sm[1].str());
                    systemSlotId = riserSlotId;
                }
            } else if (pciSlot.find("RSC-D2R-668G4") != pciSlot.npos) {
                std::regex pattern("RSC-D2R-668G4\\sSLOT(\\d+)\\s", std::regex_constants::icase);
                std::smatch sm;
                if (std::regex_search(pciSlot, sm, pattern)) {
                    int riserSlotId = std::stoi(sm[1].str());
                    switch (riserSlotId) {
                        case 1:
                            systemSlotId = 4;
                            break;
                        case 2:
                            systemSlotId = 5;
                            break;
                        case 3:
                            systemSlotId = 6;
                            break;
                        default:
                            systemSlotId = -1;
                    }
                }
            }
        }
    }

    for (auto slotSN : serialNumberList) {
        if (slotSN.slotId == systemSlotId) {
            std::size_t length = slotSN.serialNumber.copy(serialNumber, slotSN.serialNumber.length());
            serialNumber[length] = '\0';
            length = slotSN.firmwareVersion.copy(amcFwVersion, slotSN.firmwareVersion.length());
            amcFwVersion[length] = '\0';
            return XPUM_OK;
        }
    }
    serialNumber[0] = '\0';
    amcFwVersion[0] = '\0';
    return XPUM_OK;
}

static xpum_result_t validateFwImagePath(xpum_firmware_flash_job *job) {
    if (job->filePath == nullptr)
        return XPUM_UPDATE_FIRMWARE_IMAGE_FILE_NOT_FOUND;

    std::ifstream fwFile(job->filePath);
    if (!fwFile.is_open()) {
        XPUM_LOG_INFO("invalid file");
        fwFile.close();
        return XPUM_UPDATE_FIRMWARE_IMAGE_FILE_NOT_FOUND;
    }

    fwFile.close();

    return XPUM_OK;
}

xpum_result_t xpumRunFirmwareFlash(xpum_device_id_t deviceId, xpum_firmware_flash_job *job, const char *username, const char *password) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    res = validateFwImagePath(job);
    if (res != XPUM_OK)
        return res;

    if (deviceId == XPUM_DEVICE_ID_ALL_DEVICES) {
        xpum_result_t rc;

        if (job->type == xpum_firmware_type_t::XPUM_DEVICE_FIRMWARE_GFX) {
            rc = XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_ALL;
            return rc;
        }

        // check if same model
        std::vector<std::shared_ptr<Device>> devices;
        Core::instance().getDeviceManager()->getDeviceList(devices);

        std::string previousModel;
        for (std::shared_ptr<Device> device : devices) {
            // p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_NAME, std::string(props.core.name)));
            Property model;
            device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_NAME, model);
            if (previousModel.empty()) {
                previousModel = model.getValue();
            } else {
                if (previousModel != model.getValue()) {
                    XPUM_LOG_ERROR("Upgrade all AMC fail, inconsistent model:{}, {}", previousModel, model.getValue());
                    return XPUM_UPDATE_FIRMWARE_MODEL_INCONSISTENCE;
                } else {
                    // nothing yet
                }
            }
        }
        AmcCredential credential;
        credential.username = username ? std::string(username) : "";
        credential.password = password ? std::string(password) : "";
        rc = Core::instance().getFirmwareManager()->runAMCFirmwareFlash(job->filePath, credential);
        return rc;
    } else {
        if (job->type == xpum_firmware_type_t::XPUM_DEVICE_FIRMWARE_GFX) {
            res = validateDeviceId(deviceId);
            if (res != XPUM_OK)
                return res;
            return Core::instance().getFirmwareManager()->runGSCFirmwareFlash(deviceId, job->filePath);
        } else if (job->type == xpum_firmware_type_t::XPUM_DEVICE_FIRMWARE_GFX_DATA) {
            res = validateDeviceId(deviceId);
            if (res != XPUM_OK)
                return res;
            return Core::instance().getFirmwareManager()->runFwDataFlash(deviceId, job->filePath);
        } else {
            return XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC_SINGLE;
        }
    }
}

xpum_result_t xpumGetFirmwareFlashResult(xpum_device_id_t deviceId,
                                         xpum_firmware_type_t firmwareType,
                                         xpum_firmware_flash_task_result_t *result) {
    xpum_result_t ret = Core::instance().apiAccessPreCheck();
    if (ret != XPUM_OK) {
        return ret;
    }

    if (deviceId == XPUM_DEVICE_ID_ALL_DEVICES) {
        if (firmwareType != XPUM_DEVICE_FIRMWARE_AMC)
            return XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_ALL;
        AmcCredential credential;
        return Core::instance().getFirmwareManager()->getAMCFirmwareFlashResult(result, credential);
    }

    if(firmwareType == XPUM_DEVICE_FIRMWARE_AMC){
        return XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC_SINGLE;
    }

    xpum_result_t res = validateDeviceId(deviceId);
    if (res != XPUM_OK)
        return res;

    if (firmwareType == XPUM_DEVICE_FIRMWARE_GFX)
        Core::instance().getFirmwareManager()->getGSCFirmwareFlashResult(deviceId, result);
    else if (firmwareType == XPUM_DEVICE_FIRMWARE_GFX_DATA)
        Core::instance().getFirmwareManager()->getFwDataFlashResult(deviceId, result);
    else
        return XPUM_GENERIC_ERROR;

    return XPUM_OK;
}

xpum_result_t xpumGetFirmwareFlashErrorMsg(char *buffer, int *count) {
    auto errMsg = Core::instance().getFirmwareManager()->getFlashFwErrMsg();
    if (buffer == nullptr) {
        *count = errMsg.length() + 1;
        return XPUM_OK;
    }
    if (*count < (int) errMsg.length() + 1) {
        return XPUM_BUFFER_TOO_SMALL;
    }
    std::strcpy(buffer, errMsg.c_str());
    buffer[errMsg.length() + 1] = '\0';
    return XPUM_OK;
}

static bool invalidChar(char c) {
    return !(c >= 32 && c < 128);
}

xpum_device_internal_property_name_t getDeviceInternalProperty(xpum_device_property_name_t propName) {
    switch (propName) {
        case XPUM_DEVICE_PROPERTY_DEVICE_TYPE:
            return XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_TYPE;
        case XPUM_DEVICE_PROPERTY_DEVICE_NAME:
            return XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_NAME;
        case XPUM_DEVICE_PROPERTY_VENDOR_NAME:
            return XPUM_DEVICE_PROPERTY_INTERNAL_VENDOR_NAME;
        case XPUM_DEVICE_PROPERTY_UUID:
            return XPUM_DEVICE_PROPERTY_INTERNAL_UUID;
        case XPUM_DEVICE_PROPERTY_PCI_DEVICE_ID:
            return XPUM_DEVICE_PROPERTY_INTERNAL_PCI_DEVICE_ID;
        case XPUM_DEVICE_PROPERTY_PCI_VENDOR_ID:
            return XPUM_DEVICE_PROPERTY_INTERNAL_PCI_VENDOR_ID;
        case XPUM_DEVICE_PROPERTY_PCI_BDF_ADDRESS:
            return XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS;
        case XPUM_DEVICE_PROPERTY_DRM_DEVICE:
            return XPUM_DEVICE_PROPERTY_INTERNAL_DRM_DEVICE;
        case XPUM_DEVICE_PROPERTY_PCI_SLOT:
            return XPUM_DEVICE_PROPERTY_INTERNAL_PCI_SLOT;
        case XPUM_DEVICE_PROPERTY_PCIE_GENERATION:
            return XPUM_DEVICE_PROPERTY_INTERNAL_PCIE_GENERATION;
        case XPUM_DEVICE_PROPERTY_PCIE_MAX_LINK_WIDTH:
            return XPUM_DEVICE_PROPERTY_INTERNAL_PCIE_MAX_LINK_WIDTH;
        case XPUM_DEVICE_PROPERTY_DEVICE_STEPPING:
            return XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_STEPPING;
        case XPUM_DEVICE_PROPERTY_DRIVER_VERSION:
            return XPUM_DEVICE_PROPERTY_INTERNAL_DRIVER_VERSION;
        case XPUM_DEVICE_PROPERTY_GFX_FIRMWARE_NAME:
            return XPUM_DEVICE_PROPERTY_INTERNAL_GFX_FIRMWARE_NAME;
        case XPUM_DEVICE_PROPERTY_GFX_FIRMWARE_VERSION:
            return XPUM_DEVICE_PROPERTY_INTERNAL_GFX_FIRMWARE_VERSION;
        case XPUM_DEVICE_PROPERTY_GFX_DATA_FIRMWARE_NAME:
            return XPUM_DEVICE_PROPERTY_INTERNAL_GFX_DATA_FIRMWARE_NAME;
        case XPUM_DEVICE_PROPERTY_AMC_FIRMWARE_NAME:
            return XPUM_DEVICE_PROPERTY_INTERNAL_AMC_FIRMWARE_NAME;
        case XPUM_DEVICE_PROPERTY_AMC_FIRMWARE_VERSION:
            return XPUM_DEVICE_PROPERTY_INTERNAL_AMC_FIRMWARE_VERSION;
        case XPUM_DEVICE_PROPERTY_GFX_DATA_FIRMWARE_VERSION:
            return XPUM_DEVICE_PROPERTY_INTERNAL_GFX_DATA_FIRMWARE_VERSION;
        case XPUM_DEVICE_PROPERTY_SERIAL_NUMBER:
            return XPUM_DEVICE_PROPERTY_INTERNAL_SERIAL_NUMBER;
        case XPUM_DEVICE_PROPERTY_CORE_CLOCK_RATE_MHZ:
            return XPUM_DEVICE_PROPERTY_INTERNAL_CORE_CLOCK_RATE_MHZ;
        case XPUM_DEVICE_PROPERTY_MEMORY_PHYSICAL_SIZE_BYTE:
            return XPUM_DEVICE_PROPERTY_INTERNAL_MEMORY_PHYSICAL_SIZE_BYTE;
        case XPUM_DEVICE_PROPERTY_MEMORY_FREE_SIZE_BYTE:
            return XPUM_DEVICE_PROPERTY_INTERNAL_MEMORY_FREE_SIZE_BYTE;
        case XPUM_DEVICE_PROPERTY_MAX_MEM_ALLOC_SIZE_BYTE:
            return XPUM_DEVICE_PROPERTY_INTERNAL_MAX_MEM_ALLOC_SIZE_BYTE;
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_MEMORY_CHANNELS:
            return XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_MEMORY_CHANNELS;
        case XPUM_DEVICE_PROPERTY_MEMORY_BUS_WIDTH:
            return XPUM_DEVICE_PROPERTY_INTERNAL_MEMORY_BUS_WIDTH;
        case XPUM_DEVICE_PROPERTY_MAX_HARDWARE_CONTEXTS:
            return XPUM_DEVICE_PROPERTY_INTERNAL_MAX_HARDWARE_CONTEXTS;
        case XPUM_DEVICE_PROPERTY_MAX_COMMAND_QUEUE_PRIORITY:
            return XPUM_DEVICE_PROPERTY_INTERNAL_MAX_COMMAND_QUEUE_PRIORITY;
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_EUS:
            return XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_EUS;
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_TILES:
            return XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_TILES;
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_SLICES:
            return XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_SLICES;
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_SUB_SLICES_PER_SLICE:
            return XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_SUB_SLICES_PER_SLICE;
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_EUS_PER_SUB_SLICE:
            return XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_EUS_PER_SUB_SLICE;
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_THREADS_PER_EU:
            return XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_THREADS_PER_EU;
        case XPUM_DEVICE_PROPERTY_PHYSICAL_EU_SIMD_WIDTH:
            return XPUM_DEVICE_PROPERTY_INTERNAL_PHYSICAL_EU_SIMD_WIDTH;
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_MEDIA_ENGINES:
            return XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_MEDIA_ENGINES;
        case XPUM_DEVICE_PROPERTY_NUMBER_OF_MEDIA_ENH_ENGINES:
            return XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_MEDIA_ENH_ENGINES;
        case XPUM_DEVICE_PROPERTY_FABRIC_PORT_NUMBER:
            return XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_NUMBER;
        case XPUM_DEVICE_PROPERTY_FABRIC_PORT_MAX_SPEED:
            return XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_MAX_RX_SPEED;
        case XPUM_DEVICE_PROPERTY_FABRIC_PORT_LANES_NUMBER:
            return XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_RX_LANES_NUMBER;
        case XPUM_DEVICE_PROPERTY_LINUX_KERNEL_VERSION:
            return XPUM_DEVICE_PROPERTY_INTERNAL_LINUX_KERNEL_VERSION;
        default:
            return XPUM_DEVICE_PROPERTY_INTERNAL_MAX;
    }
}

xpum_result_t xpumGetDeviceProperties(xpum_device_id_t deviceId, xpum_device_properties_t *pXpumProperties) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    if (Core::instance().getDeviceManager() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }

    res = validateDeviceId(deviceId);
    if (res != XPUM_OK)
        return res;

    std::vector<std::shared_ptr<Device>> devices;
    Core::instance().getDeviceManager()->getDeviceList(devices);

    for (auto &p_device : devices) {
        if (deviceId == stoi(p_device->getId())) {
            pXpumProperties->deviceId = deviceId;
            std::vector<Property> properties;
            p_device->getProperties(properties);

            std::map<xpum_device_internal_property_name_t, Property> prop_map;

            for (size_t i = 0; i < properties.size(); i++) {
                auto &prop = properties[i];
                xpum_device_internal_property_name_t name = prop.getName();
                prop_map[name] = prop;
            }
            int propertyLen = 0;
            for (int i = 0; i < XPUM_DEVICE_PROPERTY_MAX; i++) {
                xpum_device_property_name_t propName = static_cast<xpum_device_property_name_t>(i);
                auto propNameInternal = getDeviceInternalProperty(propName);
                if (prop_map.find(propNameInternal) == prop_map.end()) {
                    continue;
                }
                auto &prop = prop_map[propNameInternal];
                std::string value = prop.getValue();

                if (propName == XPUM_DEVICE_PROPERTY_GFX_FIRMWARE_VERSION) {
                    value.erase(remove_if(value.begin(), value.end(), invalidChar), value.end());
                }
                auto &copy = pXpumProperties->properties[propertyLen++];
                copy.name = propName;
                strcpy(copy.value, value.c_str());
            }

            pXpumProperties->propertyLen = propertyLen;

            return XPUM_OK;
        }
    }

    return XPUM_RESULT_DEVICE_NOT_FOUND;
}

xpum_result_t xpumGetDeviceIdByBDF(const char *bdf, xpum_device_id_t *deviceId) {
    if (bdf == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    if (Core::instance().getDeviceManager() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }

    auto device = Core::instance().getDeviceManager()->getDevicebyBDF(std::string(bdf));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    *deviceId = stoi(device->getId());
    return XPUM_OK;    
}

xpum_result_t xpumGroupCreate(const char *groupName, xpum_group_id_t *pGroupId) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    return Core::instance().getGroupManager()->createGroup(groupName, pGroupId);
}

xpum_result_t xpumGroupDestroy(xpum_group_id_t groupId) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    return Core::instance().getGroupManager()->destroyGroup(groupId);
}

xpum_result_t xpumGroupAddDevice(xpum_group_id_t groupId, xpum_device_id_t deviceId) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    return Core::instance().getGroupManager()->addDeviceToGroup(groupId, deviceId);
}

xpum_result_t xpumGroupRemoveDevice(xpum_group_id_t groupId, xpum_device_id_t deviceId) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    return Core::instance().getGroupManager()->removeDeviceFromGroup(groupId, deviceId);
}

xpum_result_t xpumGroupGetInfo(xpum_group_id_t groupId, xpum_group_info_t *pGroupInfo) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    return Core::instance().getGroupManager()->getGroupInfo(groupId, pGroupInfo);
}

xpum_result_t xpumGetAllGroupIds(xpum_group_id_t groupIds[XPUM_MAX_NUM_GROUPS], int *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    return Core::instance().getGroupManager()->getAllGroupIds(groupIds, count);
}

xpum_result_t xpumGetStats(xpum_device_id_t deviceId,
                           xpum_device_stats_t dataList[],
                           uint32_t *count,
                           uint64_t *begin,
                           uint64_t *end,
                           uint64_t sessionId) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    if (Core::instance().getDataLogic() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }
    res = validateDeviceId(deviceId);
    if (res != XPUM_OK) {
        return res;
    }

    if (sessionId >= Configuration::MAX_STATISTICS_SESSION_NUM) {
        return XPUM_UNSUPPORTED_SESSIONID;
    }

    char* env = std::getenv("XPUM_DISABLE_PERIODIC_METRIC_MONITOR");
    std::string xpum_disable_periodic_metric_monitor{env != NULL ? env : ""};
    if (xpum_disable_periodic_metric_monitor == "1") {
        if (!Core::instance().getMonitorManager()->initOneTimeMetricMonitorTasks(MeasurementType::METRIC_MAX)) {
            return XPUM_GENERIC_ERROR;
        }
    }

    return Core::instance().getDataLogic()->getMetricsStatistics(deviceId, dataList, count, begin, end, sessionId);
}

xpum_result_t xpumGetEngineStats(xpum_device_id_t deviceId,
                                 xpum_device_engine_stats_t dataList[],
                                 uint32_t *count,
                                 uint64_t *begin,
                                 uint64_t *end,
                                 uint64_t sessionId) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    if (Core::instance().getDataLogic() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }
    res = validateDeviceId(deviceId);
    if (res != XPUM_OK) {
        return res;
    }

    if (sessionId >= Configuration::MAX_STATISTICS_SESSION_NUM) {
        return XPUM_UNSUPPORTED_SESSIONID;
    }

    char* env = std::getenv("XPUM_DISABLE_PERIODIC_METRIC_MONITOR");
    std::string xpum_disable_periodic_metric_monitor{env != NULL ? env : ""};
    if (xpum_disable_periodic_metric_monitor == "1") {
        if (!Core::instance().getMonitorManager()->initOneTimeMetricMonitorTasks(MeasurementType::METRIC_ENGINE_UTILIZATION)) {
            return XPUM_GENERIC_ERROR;
        }
    }

    return Core::instance().getDataLogic()->getEngineStatistics(deviceId, dataList, count, begin, end, sessionId);
}

xpum_result_t xpumGetMetrics(xpum_device_id_t deviceId,
                             xpum_device_metrics_t dataList[],
                             int *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    if (Core::instance().getDataLogic() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }
    Core::instance().getDataLogic()->getLatestMetrics(deviceId, dataList, count);
    return xpum_result_t::XPUM_OK;
}

xpum_result_t xpumGetEngineUtilizations(xpum_device_id_t deviceId,
                                        xpum_device_engine_metric_t dataList[],
                                        uint32_t *count) {
    if (Core::instance().getDataLogic() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }
    return Core::instance().getDataLogic()->getEngineUtilizations(deviceId, dataList, count);
}

xpum_result_t xpumGetFabricThroughputStats(xpum_device_id_t deviceId,
                                           xpum_device_fabric_throughput_stats_t dataList[],
                                           uint32_t *count,
                                           uint64_t *begin,
                                           uint64_t *end,
                                           uint64_t sessionId) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    if (Core::instance().getDataLogic() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }
    res = validateDeviceId(deviceId);
    if (res != XPUM_OK) {
        return res;
    }

    if (sessionId >= Configuration::MAX_STATISTICS_SESSION_NUM) {
        return XPUM_UNSUPPORTED_SESSIONID;
    }

    auto metric_types = Configuration::getEnabledMetrics();
    if (metric_types.find(METRIC_FABRIC_THROUGHPUT) == metric_types.end()) {
        *count = 0;
        return XPUM_METRIC_NOT_ENABLED;
    }
    std::vector<xpum::DeviceCapability> capabilities;
    Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId))->getCapability(capabilities);
    for (auto metric = metric_types.begin(); metric != metric_types.end();) {
        if (std::none_of(capabilities.begin(), capabilities.end(), [metric](xpum::DeviceCapability cap) { return (cap == Utility::capabilityFromMeasurementType(*metric)); })) {
            metric = metric_types.erase(metric);
        } else {
            metric++;
        }
    }
    if (metric_types.find(METRIC_FABRIC_THROUGHPUT) == metric_types.end()) {
        *count = 0;
        return XPUM_METRIC_NOT_SUPPORTED;
    }

    char* env = std::getenv("XPUM_DISABLE_PERIODIC_METRIC_MONITOR");
    std::string xpum_disable_periodic_metric_monitor{env != NULL ? env : ""};
    if (xpum_disable_periodic_metric_monitor == "1") {
        if (!Core::instance().getMonitorManager()->initOneTimeMetricMonitorTasks(MeasurementType::METRIC_FABRIC_THROUGHPUT)) {
            return XPUM_GENERIC_ERROR;
        }
    }
    
    return Core::instance().getDataLogic()->getFabricThroughputStatistics(deviceId, dataList, count, begin, end, sessionId);
}

xpum_result_t xpumGetFabricThroughput(xpum_device_id_t deviceId,
                                      xpum_device_fabric_throughput_metric_t dataList[],
                                      uint32_t *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    if (Core::instance().getDataLogic() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }
    res = validateDeviceId(deviceId);
    if (res != XPUM_OK)
        return res;
    return Core::instance().getDataLogic()->getFabricThroughput(deviceId, dataList, count);
}

xpum_result_t xpumGetMetricsByGroup(xpum_group_id_t groupId,
                                    xpum_device_metrics_t dataList[],
                                    int *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    xpum_group_info_t groupInfo;
    int currentCount = 0, totalCount = 0;
    xpum_device_metrics_t *pStatus = dataList;

    if (Core::instance().getGroupManager()->getGroupInfo(groupId, &groupInfo) != XPUM_OK) {
        return XPUM_GENERIC_ERROR;
    }

    for (int i = 0; i < groupInfo.count; i++) {
        currentCount = *count - totalCount;
        Core::instance().getDataLogic()->getLatestMetrics(groupInfo.deviceList[i], pStatus,
                                                          &currentCount);
        totalCount += currentCount;
        pStatus += currentCount;
        if (*count < totalCount) {
            return XPUM_BUFFER_TOO_SMALL;
        }
    }

    *count = totalCount;
    return XPUM_OK;
}

xpum_result_t xpumStartCollectMetricsRawDataTask(xpum_device_id_t deviceId,
                                                 xpum_stats_type_t metricsTypeList[],
                                                 int count,
                                                 xpum_dump_task_id_t *taskId) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    if (Core::instance().getDataLogic() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }
    std::vector<MeasurementType> types;
    for (int i = 0; i < count; ++i) {
        types.push_back(Utility::measurementTypeFromXpumStatsType(metricsTypeList[i]));
    }
    uint32_t id = Core::instance().getDataLogic()->startRawDataCollectionTask(deviceId, types);
    if (id == Configuration::RAW_DATA_COLLECTION_TASK_NUM_MAX) {
        return xpum_result_t::XPUM_GENERIC_ERROR;
    } else {
        *taskId = id;
        return xpum_result_t::XPUM_OK;
    }
}

xpum_result_t xpumStopCollectMetricsRawDataTask(xpum_dump_task_id_t taskId) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    Core::instance().getDataLogic()->stopRawDataCollectionTask(taskId);
    return xpum_result_t::XPUM_OK;
}

xpum_result_t xpumGetMetricsRawDataByTask(xpum_dump_task_id_t taskId, xpum_metrics_raw_data_t dataList[], int *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    if (Core::instance().getDataLogic() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }

    int item_count = 0;
    std::vector<std::deque<MeasurementCacheData>> datas = Core::instance().getDataLogic()->getCachedRawData(taskId);
    std::vector<std::deque<MeasurementCacheData>>::iterator iter = datas.begin();
    while (iter != datas.end()) {
        std::deque<MeasurementCacheData>::iterator iter_cache_data = (*iter).begin();
        while (iter_cache_data != (*iter).end()) {
            if (dataList == nullptr) {
                item_count++;
            } else {
                xpum_metrics_raw_data_t t;
                t.deviceId = std::stoi(iter_cache_data->getDeviceId());
                MeasurementType type = iter_cache_data->getType();
                t.metricsType = Utility::xpumStatsTypeFromMeasurementType(type);
                t.isTileData = iter_cache_data->onSubdevice();
                t.tileId = t.isTileData ? iter_cache_data->getSubdeviceID() : -1;
                t.timestamp = iter_cache_data->getTime();
                t.value = iter_cache_data->getData();
                if (item_count >= *count) {
                    return XPUM_BUFFER_TOO_SMALL;
                }
                dataList[item_count++] = t;
            }
            ++iter_cache_data;
        }
        ++iter;
    }
    *count = item_count;
    return XPUM_OK;
}

xpum_result_t xpumGetStatsByGroup(xpum_group_id_t groupId,
                                  xpum_device_stats_t dataList[],
                                  uint32_t *count,
                                  uint64_t *begin,
                                  uint64_t *end,
                                  uint64_t sessionId) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    xpum_group_info_t groupInfo;
    uint32_t currentCount = 0, totalCount = 0;
    xpum_device_stats_t *pStatus = dataList;

    res = Core::instance().getGroupManager()->getGroupInfo(groupId, &groupInfo);

    if (res != XPUM_OK) {
        return res;
    }

    if (pStatus == nullptr) {
        for (int i = 0; i < groupInfo.count; i++) {
            currentCount = *count - totalCount;
            res = Core::instance().getDataLogic()->getMetricsStatistics(groupInfo.deviceList[i], nullptr,
                                                                        &currentCount, begin, end, sessionId);
            if (res != XPUM_OK) {
                break;
            }
            totalCount += currentCount;
        }
    } else {
        for (int i = 0; i < groupInfo.count; i++) {
            currentCount = *count - totalCount;
            res = Core::instance().getDataLogic()->getMetricsStatistics(groupInfo.deviceList[i], pStatus,
                                                                        &currentCount, begin, end, sessionId);

            if (currentCount > *count - totalCount) {
                res = XPUM_BUFFER_TOO_SMALL;
                break;
            }
            if (res != XPUM_OK) {
                break;
            }
            totalCount += currentCount;
            pStatus += currentCount;
        }
    }

    *count = totalCount;
    return res;
}

std::set<int64_t> monitor_freq_set{100, 200, 500, 1000};

xpum_result_t xpumSetAgentConfig(xpum_agent_config_t key, void *value) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    if (Core::instance().getMonitorManager() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }
    switch (key) {
        case xpum_agent_config_t::XPUM_AGENT_CONFIG_SAMPLE_INTERVAL: {
            int64_t freq = *((int64_t *)value);
            if (monitor_freq_set.find(freq) == monitor_freq_set.end()) {
                return XPUM_RESULT_AGENT_SET_INVALID_VALUE;
            }
            Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE = freq;
            Core::instance().getMonitorManager()->resetMetricTasksFrequency();
            Core::instance().getDumpRawDataManager()->resetDumpFrequency();
            Core::instance().getPolicyManager()->resetCheckFrequency();
            return XPUM_OK;
        }
        default:
            break;
    }
    return XPUM_RESULT_UNKNOWN_AGENT_CONFIG_KEY;
}

xpum_result_t xpumGetAgentConfig(xpum_agent_config_t key, void *value) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    switch (key) {
        case xpum_agent_config_t::XPUM_AGENT_CONFIG_SAMPLE_INTERVAL:
            *((int64_t *)value) = (int64_t)Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE;
            return XPUM_OK;
        default:
            break;
    }
    return XPUM_RESULT_UNKNOWN_AGENT_CONFIG_KEY;
}

xpum_result_t xpumSetHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    return Core::instance().getHealthManager()->setHealthConfig(deviceId, key, value);
}

xpum_result_t xpumSetHealthConfigByGroup(xpum_group_id_t groupId, xpum_health_config_type_t key, void *value) {
    xpum_result_t ret = Core::instance().apiAccessPreCheck();
    if (ret != XPUM_OK) {
        return ret;
    }

    xpum_group_info_t xpum_group_info;
    ret = xpumGroupGetInfo(groupId, &xpum_group_info);
    if (ret != XPUM_OK)
        return ret;

    for (int i = 0; i < xpum_group_info.count; i++) {
        ret = Core::instance().getHealthManager()->setHealthConfig(xpum_group_info.deviceList[i], key, value);
        if (ret != XPUM_OK)
            return ret;
    }
    return ret;
}

xpum_result_t xpumGetHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    return Core::instance().getHealthManager()->getHealthConfig(deviceId, key, value);
}

xpum_result_t xpumGetHealthConfigByGroup(xpum_group_id_t groupId,
                                         xpum_health_config_type_t key,
                                         xpum_device_id_t deviceIdList[],
                                         void *valueList[],
                                         int *count) {
    xpum_result_t ret = Core::instance().apiAccessPreCheck();
    if (ret != XPUM_OK) {
        return ret;
    }

    xpum_group_info_t xpum_group_info;
    ret = xpumGroupGetInfo(groupId, &xpum_group_info);
    if (ret != XPUM_OK)
        return ret;

    if (deviceIdList == nullptr || valueList == nullptr) {
        *count = xpum_group_info.count;
        return XPUM_OK;
    }

    if (xpum_group_info.count > (*count)) {
        return XPUM_BUFFER_TOO_SMALL;
    }

    for (int i = 0; i < xpum_group_info.count; i++) {
        deviceIdList[i] = xpum_group_info.deviceList[i];
        ret = Core::instance().getHealthManager()->getHealthConfig(xpum_group_info.deviceList[i], key, valueList[i]);
        if (ret != XPUM_OK)
            return ret;
    }
    *count = xpum_group_info.count;

    return ret;
}

xpum_result_t xpumGetHealth(xpum_device_id_t deviceId, xpum_health_type_t type, xpum_health_data_t *data) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    return Core::instance().getHealthManager()->getHealth(deviceId, type, data);
}

xpum_result_t xpumGetHealthByGroup(xpum_group_id_t groupId,
                                   xpum_health_type_t type,
                                   xpum_health_data_t dataList[],
                                   int *count) {
    xpum_result_t ret = Core::instance().apiAccessPreCheck();
    if (ret != XPUM_OK) {
        return ret;
    }

    xpum_group_info_t xpum_group_info;
    ret = xpumGroupGetInfo(groupId, &xpum_group_info);
    if (ret != XPUM_OK)
        return ret;

    if (dataList == nullptr) {
        *count = xpum_group_info.count;
        return XPUM_OK;
    }

    if (xpum_group_info.count > (*count)) {
        return XPUM_BUFFER_TOO_SMALL;
    }

    for (int i = 0; i < xpum_group_info.count; i++) {
        ret = Core::instance().getHealthManager()->getHealth(xpum_group_info.deviceList[i], type, &dataList[i]);
        if (ret != XPUM_OK)
            return ret;
    }
    *count = xpum_group_info.count;

    return ret;
}

xpum_result_t xpumRunDiagnostics(xpum_device_id_t deviceId, xpum_diag_level_t level) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    return Core::instance().getDiagnosticManager()->runDiagnostics(deviceId, level);
}

xpum_result_t xpumRunDiagnosticsByGroup(xpum_group_id_t groupId, xpum_diag_level_t level) {
    xpum_result_t ret = Core::instance().apiAccessPreCheck();
    if (ret != XPUM_OK) {
        return ret;
    }

    xpum_group_info_t xpum_group_info;
    ret = xpumGroupGetInfo(groupId, &xpum_group_info);
    if (ret != XPUM_OK)
        return ret;

    for (int i = 0; i < xpum_group_info.count; i++) {
        bool isRunning = Core::instance().getDiagnosticManager()->isDiagnosticsRunning(xpum_group_info.deviceList[i]);
        if (isRunning) {
            return XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE;
        }
    }

    for (int i = 0; i < xpum_group_info.count; i++) {
        ret = Core::instance().getDiagnosticManager()->runDiagnostics(xpum_group_info.deviceList[i], level);
        if (ret != XPUM_OK)
            return ret;
    }

    return ret;
}

xpum_result_t xpumGetDiagnosticsResult(xpum_device_id_t deviceId, xpum_diag_task_info_t *result) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    return Core::instance().getDiagnosticManager()->getDiagnosticsResult(deviceId, result);
}

xpum_result_t xpumGetDiagnosticsResultByGroup(xpum_group_id_t groupId,
                                              xpum_diag_task_info_t resultList[],
                                              int *count) {
    xpum_result_t ret = Core::instance().apiAccessPreCheck();
    if (ret != XPUM_OK) {
        return ret;
    }

    xpum_group_info_t xpum_group_info;
    ret = xpumGroupGetInfo(groupId, &xpum_group_info);
    if (ret != XPUM_OK)
        return ret;

    if (resultList == nullptr) {
        *count = xpum_group_info.count;
        return XPUM_OK;
    }

    if (xpum_group_info.count > (*count)) {
        return XPUM_BUFFER_TOO_SMALL;
    }

    for (int i = 0; i < xpum_group_info.count; i++) {
        ret = Core::instance().getDiagnosticManager()->getDiagnosticsResult(xpum_group_info.deviceList[i], &resultList[i]);
        if (ret != XPUM_OK)
            return ret;
    }
    *count = xpum_group_info.count;

    return ret;
}

xpum_result_t xpumGetDiagnosticsMediaCodecResult(xpum_device_id_t deviceId,
                                                xpum_diag_media_codec_metrics_t resultList[],
                                                int *count) {
    xpum_result_t ret = Core::instance().apiAccessPreCheck();
    if (ret != XPUM_OK) {
        return ret;
    }

    ret = validateDeviceId(deviceId);
    if (ret != XPUM_OK) {
        return ret;
    }
    
    return Core::instance().getDiagnosticManager()->getDiagnosticsMediaCodecResult(deviceId, resultList, count);
}

void convertStandbyData(Standby &src, xpum_standby_data_t *des) {
    des->type = (xpum_standby_type_t)src.getType();
    des->mode = (xpum_standby_mode_t)src.getMode();
    des->on_subdevice = src.onSubdevice();
    des->subdevice_Id = src.getSubdeviceId();
}

void convertFrequencyData(Frequency &freq, xpum_frequency_range_t *des) {
    des->type = static_cast<xpum_frequency_type_t>(freq.getTypeValue());
    des->subdevice_Id = freq.getSubdeviceId();
    des->min = freq.getMin();
    des->max = freq.getMax();
}

void convertScheduleData(Scheduler &src, xpum_scheduler_data_t *des) {
    des->engine_types = (xpum_engine_type_flags_t)src.getEngineTypes();
    des->supported_modes = (xpum_scheduler_mode_t)src.getSupportedModes();
    des->mode = (xpum_scheduler_mode_t)src.getCurrentMode();
    des->can_control = src.canControl();
    des->on_subdevice = src.onSubdevice();
    des->subdevice_Id = src.getSubdeviceId();
    des->val1 = src.getVal1();
    des->val2 = src.getVal2();
}

xpum_result_t xpumGetDeviceStandbys(xpum_device_id_t deviceId,
                                    xpum_standby_data_t *dataArray, uint32_t *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    std::vector<Standby> standbys;

    Core::instance().getDeviceManager()->getDeviceStandbys(std::to_string(deviceId), standbys);

    if (standbys.size() > *count && dataArray != nullptr) {
        return XPUM_BUFFER_TOO_SMALL;
    } else {
        *count = standbys.size();
    }

    if (dataArray == nullptr) {
        return XPUM_OK;
    }

    int i = 0;

    for (auto &standby : standbys) {
        convertStandbyData(standby, dataArray + i);
        i++;
    }

    return XPUM_OK;
}

xpum_result_t xpumSetDeviceStandby(xpum_device_id_t deviceId,
                                   const xpum_standby_data_t standby) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    res = validateDeviceIdAndTileId(deviceId, standby.subdevice_Id);
    if (res != XPUM_OK) {
        return res;
    }

    Standby s((zes_standby_type_t)standby.type, standby.on_subdevice, standby.subdevice_Id, (zes_standby_promo_mode_t)standby.mode);
    if (Core::instance().getDeviceManager()->setDeviceStandby(std::to_string(deviceId), s)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}

xpum_result_t xpumGetDevicePowerLimits(xpum_device_id_t deviceId,
                                       int32_t tileId,
                                       xpum_power_limits_t *pPowerLimits) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    if (pPowerLimits == nullptr) {
        return XPUM_BUFFER_TOO_SMALL;
    }

    Power_limits_t limits;
    Core::instance().getDeviceManager()->getDevicePowerLimits(std::to_string(deviceId), limits.sustained_limit, limits.burst_limit, limits.peak_limit);

    pPowerLimits->sustained_limit.enabled = limits.sustained_limit.enabled;
    pPowerLimits->sustained_limit.interval = limits.sustained_limit.interval;
    pPowerLimits->sustained_limit.power = limits.sustained_limit.power;
    //typedef struct xpum_power_limits_t {
    //xpum_power_sustained_limit_t sustained_limit;
    //xpum_power_burst_limit_t burst_limit;
    //xpum_power_peak_limit_t peak_limit;
    //} xpum_power_limits_t;

    //pPowerLimits->burst_limit.enabled = limits.burst_limit.enabled;
    //pPowerLimits->burst_limit.power = limits.burst_limit.power;

    //pPowerLimits->peak_limit.power_AC = limits.peak_limit.power_AC;
    //pPowerLimits->peak_limit.power_DC = limits.peak_limit.power_DC;
    return XPUM_OK;
}

xpum_result_t xpumSetDevicePowerSustainedLimits(xpum_device_id_t deviceId,
                                                int32_t tileId,
                                                const xpum_power_sustained_limit_t sustained_limit) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    if (tileId != -1) {
        xpum_result_t res = validateDeviceIdAndTileId(deviceId, tileId);
        if (res != XPUM_OK) {
            return res;
        }
    } else {
        xpum_result_t res = validateDeviceId(deviceId);
        if (res != XPUM_OK) {
            return res;
        }
    }

    Power_sustained_limit_t sustainedLimit;
    sustainedLimit.enabled = sustained_limit.enabled;
    sustainedLimit.interval = sustained_limit.interval;
    sustainedLimit.power = sustained_limit.power;
    if (Core::instance().getDeviceManager()->setDevicePowerSustainedLimits(std::to_string(deviceId), tileId, sustainedLimit)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}
/*
xpum_result_t xpumSetDevicePowerBurstLimits(xpum_device_id_t deviceId,
                                            int32_t tileId,
                                            const xpum_power_burst_limit_t burst_limit) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    xpum_result_t res = validateDeviceIdAndTileId(deviceId, tileId);
    if (res != XPUM_OK) {
        return res;
    }
    Power_burst_limit_t burstLimit;
    burstLimit.enabled = burst_limit.enabled;
    burstLimit.power = burst_limit.power;
    if (Core::instance().getDeviceManager()->setDevicePowerBurstLimits(std::to_string(deviceId), burstLimit)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}

xpum_result_t xpumSetDevicePowerPeakLimits(xpum_device_id_t deviceId,
                                           int32_t tileId,
                                           const xpum_power_peak_limit_t peak_limit) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    xpum_result_t res = validateDeviceIdAndTileId(deviceId, tileId);
    if (res != XPUM_OK) {
        return res;
    }
    Power_peak_limit_t peakLimit;
    peakLimit.power_AC = peak_limit.power_AC;
    peakLimit.power_DC = peak_limit.power_DC;
    if (Core::instance().getDeviceManager()->setDevicePowerPeakLimits(std::to_string(deviceId), peakLimit)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}
*/
xpum_result_t xpumGetDeviceFrequencyRanges(xpum_device_id_t deviceId,
                                           xpum_frequency_range_t *dataArray, uint32_t *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    std::vector<Frequency> frequencies;
    Core::instance().getDeviceManager()->getDeviceFrequencyRanges(std::to_string(deviceId), frequencies);

    if (frequencies.size() > *count && dataArray != nullptr) {
        return XPUM_BUFFER_TOO_SMALL;
    } else {
        *count = frequencies.size();
    }

    if (dataArray == nullptr) {
        return XPUM_OK;
    }

    int i = 0;
    for (auto &freq : frequencies) {
        convertFrequencyData(freq, dataArray + i);
        i++;
    }
    return XPUM_OK;
}

xpum_result_t xpumSetDeviceFrequencyRange(xpum_device_id_t deviceId,
                                          const xpum_frequency_range_t frequency) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    res = validateDeviceIdAndTileId(deviceId, frequency.subdevice_Id);
    if (res != XPUM_OK) {
        return res;
    }

    Frequency freq((zes_freq_domain_t)frequency.type, frequency.subdevice_Id, frequency.min, frequency.max);
    if (Core::instance().getDeviceManager()->setDeviceFrequencyRange(std::to_string(deviceId), freq)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}

xpum_result_t xpumGetDeviceSchedulers(xpum_device_id_t deviceId,
                                      xpum_scheduler_data_t *dataArray, uint32_t *count) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    std::vector<Scheduler> schedulers;
    Core::instance().getDeviceManager()->getDeviceSchedulers(std::to_string(deviceId), schedulers);

    if (schedulers.size() > *count && dataArray != nullptr) {
        return XPUM_BUFFER_TOO_SMALL;
    } else {
        *count = schedulers.size();
    }

    if (dataArray == nullptr) {
        return XPUM_OK;
    }

    int i = 0;
    for (auto &sche : schedulers) {
        convertScheduleData(sche, dataArray + i);
        i++;
    }
    return XPUM_OK;
}

int32_t defaultMaxPowerLimit(std::string device_name) {
   if (device_name == "Intel(R) Graphics [0x56c0]"){
    return 120 *1000;
   }
   if (device_name == "Intel(R) Graphics [0x56c1]"){
    return 23 *1000;
   }
   if (device_name == "Intel(R) Graphics [0x0bd0]"){
    return 600 *1000;
   }
   if (device_name == "Intel(R) Graphics [0x0bd5]"){
    return 600 *1000;
   }
   if (device_name == "Intel(R) Graphics [0x0bd6]"){
    return 600 *1000;
   }
   if (device_name == "Intel(R) Graphics [0x0bd7]"){
    return 450 *1000;
   }
   if (device_name == "Intel(R) Graphics [0x0bd8]"){
    return 450 *1000;
   }
   if (device_name == "Intel(R) Graphics [0x0bd9]"){
    return 300 *1000;
   }
   if (device_name == "Intel(R) Graphics [0x0bda]"){
    return 300 *1000;
   }
   if (device_name == "Intel(R) Graphics [0x0bdb]"){
    return 300 *1000;
   }
   if (device_name == "Intel(R) Graphics [0x0be5]"){
    return 600 *1000;
   }
   return -1;
}

xpum_result_t xpumGetDevicePowerProps(xpum_device_id_t deviceId, xpum_power_prop_data_t *dataArray, uint32_t *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    Property prop;
    int32_t default_maxLimit = -1;
    Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId))->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_NAME, prop);

    default_maxLimit = defaultMaxPowerLimit(prop.getValue());

    std::vector<Power> powers;
    Core::instance().getDeviceManager()->getDevicePowerProps(std::to_string(deviceId), powers);

    if (powers.size() > *count && dataArray != nullptr) {
        return XPUM_BUFFER_TOO_SMALL;
    } else {
        *count = powers.size();
    }
    if (dataArray != nullptr) {
        int i = 0;
        for (auto &power : powers) {
            dataArray[i].on_subdevice = power.onSubdevice();
            dataArray[i].subdevice_Id = power.getSubdeviceId();
            dataArray[i].can_control = power.canControl();
            dataArray[i].is_energy_threshold_supported = power.isEnergyThresholdSupported();
            dataArray[i].default_limit = power.getDefaultLimit();
            dataArray[i].min_limit = power.getMinLimit();
            if (power.getMaxLimit() == -1) {
                dataArray[i].max_limit = default_maxLimit;
            } else {
                dataArray[i].max_limit = power.getMaxLimit();
            }
            
            i++;
        }
    }
    return XPUM_OK;
}

xpum_result_t xpumSetDeviceSchedulerTimeoutMode(xpum_device_id_t deviceId,
                                                const xpum_scheduler_timeout_t sched_timeout) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    res = validateDeviceIdAndTileId(deviceId, sched_timeout.subdevice_Id);
    if (res != XPUM_OK) {
        return res;
    }

    SchedulerTimeoutMode mode;
    mode.subdevice_Id = sched_timeout.subdevice_Id;
    mode.mode_setting.watchdogTimeout = sched_timeout.watchdog_timeout;

    if (Core::instance().getDeviceManager()->setDeviceSchedulerTimeoutMode(std::to_string(deviceId), mode)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}

xpum_result_t xpumSetDeviceSchedulerTimesliceMode(xpum_device_id_t deviceId,
                                                  const xpum_scheduler_timeslice_t sched_timeslice) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    res = validateDeviceIdAndTileId(deviceId, sched_timeslice.subdevice_Id);
    if (res != XPUM_OK) {
        return res;
    }

    SchedulerTimesliceMode mode;
    mode.subdevice_Id = sched_timeslice.subdevice_Id;
    mode.mode_setting.interval = sched_timeslice.interval;
    mode.mode_setting.yieldTimeout = sched_timeslice.yield_timeout;

    if (Core::instance().getDeviceManager()->setDeviceSchedulerTimesliceMode(std::to_string(deviceId), mode)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}

xpum_result_t xpumSetDeviceSchedulerExclusiveMode(xpum_device_id_t deviceId,
                                                  const xpum_scheduler_exclusive_t sched_exclusive) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    res = validateDeviceIdAndTileId(deviceId, sched_exclusive.subdevice_Id);
    if (res != XPUM_OK) {
        return res;
    }

    SchedulerExclusiveMode mode;
    mode.subdevice_Id = sched_exclusive.subdevice_Id;

    if (Core::instance().getDeviceManager()->setDeviceSchedulerExclusiveMode(std::to_string(deviceId), mode)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}
/**
 * @brief Reset the device
 * @details This function is used to reset the device
 *
 * @param deviceId          IN: The device Id
 * @param force             IN: force to reset the device or not
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_UPDATE_FIRMWARE_TASK_RUNNING    if device is updating firmware
 */
//xpum_result_t xpumResetDevice(xpum_device_id_t deviceId, bool force);

xpum_result_t xpumResetDevice(xpum_device_id_t deviceId, bool force) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    if (device->isUpgradingFw()) {
        return XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
    }
    if (Core::instance().getFirmwareManager()->isUpgradingFw()) {
        return XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
    }

    if (Core::instance().getDeviceManager()->resetDevice(std::to_string(deviceId), force)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}

xpum_result_t xpumGetFreqAvailableClocks(xpum_device_id_t deviceId, uint32_t tileId, double *dataArray, uint32_t *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    res = validateDeviceIdAndTileId(deviceId, tileId);
    if (res != XPUM_OK) {
        return res;
    }
    std::vector<double> clocks;
    Core::instance().getDeviceManager()->getFreqAvailableClocks(std::to_string(deviceId), tileId, clocks);

    if (clocks.size() > *count && dataArray != nullptr) {
        return XPUM_BUFFER_TOO_SMALL;
    } else {
        *count = clocks.size();
    }
    if (dataArray != nullptr) {
        int i = 0;
        for (auto &clock : clocks) {
            dataArray[i] = clock;
            i++;
        }
    }
    return XPUM_OK;
}

xpum_result_t xpumGetDeviceProcessState(xpum_device_id_t deviceId, xpum_device_process_t *dataArray, uint32_t *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    std::vector<device_process> process;
    Core::instance().getDeviceManager()->getDeviceProcessState(std::to_string(deviceId), process);

    if (process.size() > *count && dataArray != nullptr) {
        return XPUM_BUFFER_TOO_SMALL;
    } else {
        *count = process.size();
    }

    if (dataArray != nullptr) {
        int i = 0;
        for (auto &proc : process) {
            dataArray[i].processId = proc.getProcessId();
            dataArray[i].memSize = proc.getMemSize();
            dataArray[i].sharedSize = proc.getSharedSize();
            dataArray[i].engine = (xpum_engine_type_flags_t)proc.getEngine();
            strcpy(dataArray[i].processName, proc.getProcessName().c_str());
            i++;
        }
    }
    return XPUM_OK;
}

xpum_result_t xpumGetDeviceComponentOccupancyRatio(xpum_device_id_t deviceId, 
                                                   xpum_device_tile_id_t tileId,
                                                   xpum_sampling_interval_t samplingInterval,
                                                   xpum_device_components_ratio_t *dataArray, 
                                                   uint32_t *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    if (Core::instance().getDeviceManager() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }

    if (tileId == -1) {
        res = validateDeviceId(deviceId);
    } else {
        res = validateDeviceIdAndTileId(deviceId, tileId);
    }
    
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    Property prop;
    device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_TILES, prop);
    uint32_t tileCount = prop.getValueInt();

    if (*count > 0 && *count < tileCount && dataArray != nullptr) {
        return XPUM_BUFFER_TOO_SMALL;
    } else {
        *count = tileCount;
    }

    if (dataArray == nullptr) {
        return XPUM_OK;
    }

    std::string device_id = std::to_string(deviceId);
    if (samplingInterval != -1 && samplingInterval > 0) {
        Configuration::EU_ACTIVE_STALL_IDLE_STREAMER_SAMPLING_PERIOD = samplingInterval * 1000000;
    }

    auto p_data = Core::instance().getDeviceManager()->getRealtimeMeasurementData(METRIC_PERF, device_id);
    std::shared_ptr<PerfMeasurementData> p_measurement_data = std::static_pointer_cast<PerfMeasurementData>(p_data);

    uint32_t engineUtilRawDataSize = 0;
    Core::instance().getDataLogic()->getEngineUtilizations(deviceId, nullptr, &engineUtilRawDataSize);
    std::vector<xpum_device_engine_metric_t> engineUtilRawDataList(engineUtilRawDataSize);
    Core::instance().getDataLogic()->getEngineUtilizations(deviceId, engineUtilRawDataList.data(), &engineUtilRawDataSize);

    /* calculate engine utilization of current device */
    std::float_t engineCompute = 0;
    std::float_t engineRender = 0;
    std::int16_t countRenderEngine = 0;
    std::int16_t countComputeEngine = 0;
    std::float_t engineUsage = 0;
    std::int16_t scale = 100;

    for (auto engineUtilRawData : engineUtilRawDataList) {
        if (engineUtilRawData.type == XPUM_ENGINE_TYPE_COMPUTE && engineUtilRawData.value > 0) {
            countComputeEngine++;
            engineCompute += engineUtilRawData.value;
            scale = engineUtilRawData.scale;
        } else if (engineUtilRawData.type == XPUM_ENGINE_TYPE_RENDER) {
            countRenderEngine++;
            engineRender += engineUtilRawData.value;
            scale = engineUtilRawData.scale;
        }
    }
    engineUsage = std::max(engineCompute / countComputeEngine, engineRender / countRenderEngine);
    engineUsage /= scale;

    auto p_perf_datas = p_measurement_data->getDatas();
    if (p_perf_datas->size() <= 0) {
        return XPUM_METRIC_NOT_SUPPORTED;
    }

    /*  calculate the component occupancy ratio of each tile in current device */
    for (size_t i = 0; i < p_perf_datas->size(); i++) {
        std::float_t active = 0;
        std::float_t stall = 0;
        std::float_t inUse = 0;
        
        std::float_t occupancy = 0;
        std::float_t stallALU = 0;
        std::float_t stallSFU = 0;
        std::float_t stallSB = 0;
        std::float_t stallSEND = 0;
        std::float_t stallDep = 0;
        std::float_t stallOther = 0;
        std::float_t stallBarrier = 0;
        std::float_t stallInstFetch = 0;
        std::float_t fpuActive = 0;
        std::float_t emActive = 0;
        std::float_t xmxActive = 0;
        std::float_t xmxOnly = 0;
        std::float_t fpuWithoutXMX = 0;
        std::float_t fpuOnly = 0;
        std::float_t emIntOnly = 0;
        std::float_t emFpuActive = 0;
        std::float_t xmxFpuActive = 0;
        std::float_t aluActive = 0;
        std::float_t other = 0;
        std::float_t stallTotal = 0;
        std::float_t notInUse = 0;
        std::float_t hypoInUse = 0;
        std::float_t engine = 0;
        std::float_t workload = 0;
        std::float_t nonOccupancy = 0;
        std::float_t remaining = 0;
        std::float_t stallRatio = 0;

        for (auto group_data : (*p_perf_datas)[i]->data) {
            for (auto metric_data : group_data.data) {
                if (std::strcmp(metric_data.name.c_str(), "XveActive") == 0) {
                    active = metric_data.average;
                } 
                if (std::strcmp(metric_data.name.c_str(), "XveStall") == 0) {
                    stall = metric_data.average;
                }
                if (std::strcmp(metric_data.name.c_str(), "EmActive") == 0) {
                    emActive = metric_data.average;
                }
                if (std::strcmp(metric_data.name.c_str(), "XmxActive") == 0) {
                    xmxActive = metric_data.average;
                }
                if (std::strcmp(metric_data.name.c_str(), "FpuActive") == 0) {
                    fpuActive = metric_data.average;
                }
                if (std::strcmp(metric_data.name.c_str(), "XveFpuEmActive") == 0) {
                    emFpuActive = metric_data.average;
                }
                if (std::strcmp(metric_data.name.c_str(), "XveFpuXmxActive") == 0) {
                    xmxFpuActive = metric_data.average;
                }
                if (std::strcmp(metric_data.name.c_str(), "XveThreadOccupancy") == 0) {
                    occupancy = metric_data.average;
                }
                if (metric_data.name.find("ALUWR") != std::string::npos) {
                    stallALU += metric_data.average;
                }
                if (metric_data.name.find("BARRIER") != std::string::npos) {
                    stallBarrier += metric_data.average;
                }
                if (metric_data.name.find("SHARED_FUNCTION") != std::string::npos) {
                    stallSFU += metric_data.average;
                }
                if (metric_data.name.find("SBID") != std::string::npos) {
                    stallSB += metric_data.average;
                }
                if (metric_data.name.find("SENDWR") != std::string::npos) {
                    stallSEND += metric_data.average;
                }
                if (metric_data.name.find("OTHER") != std::string::npos) {
                    stallOther += metric_data.average;
                }
                if (metric_data.name.find("INSTFETCH") != std::string::npos) {
                    stallInstFetch += metric_data.average;
                }
            }
        }

        inUse = active + stall;
        notInUse = 100 - inUse;
        hypoInUse = inUse * 100 / engineUsage;
        if (hypoInUse > 100) {
            hypoInUse = 100;
        }

        engine = hypoInUse - inUse;
        if (engine < 0 || isnan(engine)) {
            engine = 0;
        } 
        workload = notInUse - engine;
        if (workload < 0) {
            workload = 0;
        }

        if (inUse != 0) {
            if (inUse > 0) {
                stallRatio = stall / inUse;
            }
            if (occupancy > 0) {
                nonOccupancy = (stallRatio - std::pow(stallRatio, inUse / occupancy)) * inUse;
            }
            if (nonOccupancy < 0) {
                nonOccupancy = 0;
            }
            remaining = stall - nonOccupancy;
            if (remaining < 0) {
                remaining = 0;
            }

            stallDep = stallSB;
            if (stallDep < stallSFU) {
                stallDep = stallSFU;
            }
            stallTotal = stallALU + stallBarrier + stallDep + stallOther + stallInstFetch;

            remaining /= stallTotal;
            stallALU *= remaining;
            stallBarrier *= remaining;
            stallDep *= remaining;
            stallOther *= remaining;
            stallInstFetch *= remaining;
                
            aluActive = emActive + fpuActive - emFpuActive + xmxActive - xmxFpuActive;
            xmxOnly = xmxActive - xmxFpuActive;
            fpuWithoutXMX = fpuActive - xmxFpuActive;
            fpuOnly = fpuActive - xmxFpuActive - emFpuActive;
            emIntOnly = emActive - emFpuActive;
            other = active - aluActive;
        }

        std::vector<std::pair<std::string, std::double_t>> components_ratios;
        components_ratios.push_back(std::pair<std::string, std::double_t>("notInUse", notInUse));
        components_ratios.push_back(std::pair<std::string, std::double_t>("workload", workload));
        components_ratios.push_back(std::pair<std::string, std::double_t>("engine", engine));
        components_ratios.push_back(std::pair<std::string, std::double_t>("inUse", inUse));
        components_ratios.push_back(std::pair<std::string, std::double_t>("active", active));
        components_ratios.push_back(std::pair<std::string, std::double_t>("aluActive", aluActive));
        components_ratios.push_back(std::pair<std::string, std::double_t>("xmxActive", xmxActive));
        components_ratios.push_back(std::pair<std::string, std::double_t>("xmxOnly", xmxOnly));
        components_ratios.push_back(std::pair<std::string, std::double_t>("xmxFpuActive", xmxFpuActive));
        components_ratios.push_back(std::pair<std::string, std::double_t>("fpuWithoutXMX", fpuWithoutXMX));
        components_ratios.push_back(std::pair<std::string, std::double_t>("fpuOnly", fpuOnly));
        components_ratios.push_back(std::pair<std::string, std::double_t>("emFpuActive", emFpuActive));
        components_ratios.push_back(std::pair<std::string, std::double_t>("emIntOnly", emIntOnly));
        components_ratios.push_back(std::pair<std::string, std::double_t>("other", other));
        components_ratios.push_back(std::pair<std::string, std::double_t>("stall", stall));
        components_ratios.push_back(std::pair<std::string, std::double_t>("nonOccupancy", nonOccupancy));
        components_ratios.push_back(std::pair<std::string, std::double_t>("stallALU", stallALU));
        components_ratios.push_back(std::pair<std::string, std::double_t>("stallBarrier", stallBarrier));
        components_ratios.push_back(std::pair<std::string, std::double_t>("stallDep", stallDep));
        components_ratios.push_back(std::pair<std::string, std::double_t>("stallOther", stallOther));
        components_ratios.push_back(std::pair<std::string, std::double_t>("stallInstFetch", stallInstFetch));

        dataArray[i].componentNum = components_ratios.size();
        int idx = 0;
        for (auto it = components_ratios.begin(); it != components_ratios.end(); it++) {
            std::strcpy(dataArray[i].ratios[idx].occupancyName, (*it).first.c_str());
            dataArray[i].ratios[idx].value = (*it).second;
            idx++;
        }
    }

    return XPUM_OK;
}

xpum_result_t xpumGetDeviceUtilizationByProcess(xpum_device_id_t deviceId,
        uint32_t utilInterval, xpum_device_util_by_process_t dataArray[],
        uint32_t *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }
    std::shared_ptr<Device> device =
        Core::instance().getDeviceManager()->getDevice(
                std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    if (utilInterval == 0 || utilInterval > 1000 * 1000) {
        return XPUM_INTERVAL_INVALID;
    }
    if (dataArray == nullptr || count == nullptr || *count <= 0) {
        return XPUM_BUFFER_TOO_SMALL;
    }

    std::vector<std::vector<device_util_by_proc>> utils;
    Core::instance().getDeviceManager()->getDeviceUtilByProcess(
            std::to_string(deviceId), utilInterval, utils);
    uint32_t i = 0;
    auto iter = utils.begin();
    while (iter != utils.end()) {
        for (auto &util : *iter) {
            dataArray[i].processId = util.getProcessId();
            dataArray[i].deviceId = util.getDeviceId();
            dataArray[i].memSize = util.getMemSize();
            dataArray[i].sharedMemSize = util.getSharedMemSize();
            strncpy(dataArray[i].processName, util.getProcessName().c_str(),
                    util.getProcessName().length() + 1);
            dataArray[i].renderingEngineUtil = util.getRenderingEngineUtil();
            dataArray[i].computeEngineUtil = util.getComputeEngineUtil();
            dataArray[i].copyEngineUtil = util.getCopyEngineUtil();
            dataArray[i].mediaEngineUtil = util.getMediaEnigineUtil();
            dataArray[i].mediaEnhancementUtil = util.getMediaEnhancementUtil();
            i++;
            if (i >= *count) {
                return XPUM_BUFFER_TOO_SMALL;
            }
        }
        iter++;
    }
    *count = i;
    return XPUM_OK;
}

xpum_result_t xpumGetAllDeviceUtilizationByProcess(uint32_t utilInterval, 
        xpum_device_util_by_process_t dataArray[], 
        uint32_t *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }
    if (utilInterval == 0 || utilInterval > 1000 * 1000) {
        return XPUM_INTERVAL_INVALID;
    }

    if (dataArray == nullptr || count == nullptr || *count <= 0) {
        return XPUM_BUFFER_TOO_SMALL;
    }

    std::vector<std::vector<device_util_by_proc>> utils;
    Core::instance().getDeviceManager()->getDeviceUtilByProcess(
            "", utilInterval, utils);
    uint32_t i = 0;
    auto iter = utils.begin();
    while (iter != utils.end()) {
        for (auto &util : *iter) {
            dataArray[i].processId = util.getProcessId();
            dataArray[i].deviceId = util.getDeviceId();
            dataArray[i].memSize = util.getMemSize();
            dataArray[i].sharedMemSize = util.getSharedMemSize();
            strncpy(dataArray[i].processName, util.getProcessName().c_str(),
                    util.getProcessName().length() + 1);
            dataArray[i].renderingEngineUtil = util.getRenderingEngineUtil();
            dataArray[i].computeEngineUtil = util.getComputeEngineUtil();
            dataArray[i].copyEngineUtil = util.getCopyEngineUtil();
            dataArray[i].mediaEngineUtil = util.getMediaEnigineUtil();
            dataArray[i].mediaEnhancementUtil = util.getMediaEnhancementUtil();
            i++;
            if (i >= *count) {
                return XPUM_BUFFER_TOO_SMALL;
            }
        }
        iter++;
    }
    *count = i;
    return XPUM_OK;
}

xpum_result_t xpumGetPerformanceFactor(xpum_device_id_t deviceId, xpum_device_performancefactor_t *dataArray, uint32_t *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    res = validateDeviceId(deviceId);
    if (res != XPUM_OK) {
        return res;
    }
    std::vector<PerformanceFactor> pf;
    Core::instance().getDeviceManager()->getPerformanceFactor(std::to_string(deviceId), pf);

    if (pf.size() > *count && dataArray != nullptr) {
        return XPUM_BUFFER_TOO_SMALL;
    } else {
        *count = pf.size();
    }

    if (dataArray != nullptr) {
        int i = 0;
        for (auto &p : pf) {
            dataArray[i].engine = (xpum_engine_type_flags_t)p.getEngine();
            dataArray[i].factor = p.getFactor();
            dataArray[i].on_subdevice = p.onSubdevice();
            dataArray[i].subdevice_id = p.getSubdeviceId();
            i++;
        }
    }
    return XPUM_OK;
}

xpum_result_t xpumSetPerformanceFactor(xpum_device_id_t deviceId, xpum_device_performancefactor_t performanceFactor) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    res = validateDeviceIdAndTileId(deviceId, performanceFactor.subdevice_id);
    if (res != XPUM_OK) {
        return res;
    }

    PerformanceFactor pf(performanceFactor.on_subdevice,
                         performanceFactor.subdevice_id, (zes_engine_type_flags_t)performanceFactor.engine, performanceFactor.factor);
    //(bool on_subdevice, uint32_t subdevice_id, zes_engine_type_flags_t engine, double factor)

    if (Core::instance().getDeviceManager()->setPerformanceFactor(std::to_string(deviceId), pf)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}
xpum_result_t xpumGetFabricPortConfig(xpum_device_id_t deviceId, xpum_fabric_port_config_t *dataArray, uint32_t *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    res = validateDeviceId(deviceId);
    if (res != XPUM_OK) {
        return res;
    }
    //std::vector<xpum_fabric_port_config_t> portConfig;
    std::vector<port_info> pi;

    Core::instance().getDeviceManager()->getFabricPorts(std::to_string(deviceId), pi);

    if (pi.size() > *count && dataArray != nullptr) {
        return XPUM_BUFFER_TOO_SMALL;
    } else {
        *count = pi.size();
    }

    if (dataArray != nullptr) {
        int i = 0;
        for (auto &item : pi) {
            dataArray[i].onSubdevice = item.portProps.onSubdevice;
            dataArray[i].subdeviceId = item.portProps.subdeviceId;
            dataArray[i].fabricId = item.portProps.portId.fabricId;
            dataArray[i].attachId = item.portProps.portId.attachId;
            dataArray[i].portNumber = item.portProps.portId.portNumber;
            dataArray[i].enabled = item.portConf.enabled;
            dataArray[i].beaconing = item.portConf.beaconing;
            dataArray[i].setting_enabled = false;
            dataArray[i].setting_beaconing = false;
            i++;
        }
    }
    return XPUM_OK;
}

xpum_result_t xpumSetFabricPortConfig(xpum_device_id_t deviceId, xpum_fabric_port_config_t fabricPortConfig) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    res = validateDeviceIdAndTileId(deviceId, fabricPortConfig.subdeviceId);
    if (res != XPUM_OK) {
        return res;
    }

    port_info_set pis;
    pis.onSubdevice = fabricPortConfig.onSubdevice;
    pis.subdeviceId = fabricPortConfig.subdeviceId;
    pis.portId.fabricId = fabricPortConfig.fabricId;
    pis.portId.attachId = fabricPortConfig.attachId;
    pis.portId.portNumber = fabricPortConfig.portNumber;
    pis.enabled = fabricPortConfig.enabled;
    pis.beaconing = fabricPortConfig.beaconing;
    pis.setting_enabled = fabricPortConfig.setting_enabled;
    pis.setting_beaconing = fabricPortConfig.setting_beaconing;

    if (Core::instance().getDeviceManager()->setFabricPorts(std::to_string(deviceId), pis)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}

bool callIgscMemoryEcc(std::string path, bool getting, uint8_t req, uint8_t* cur, uint8_t* pen) {
    const std::string str_igscLibPath{"libigsc.so"};
    const std::string str_igscLibPath0{"libigsc.so.0"}; //temporary workaround for missing symoblic link libigsc.so -> libigsc.so.0
    const std::string str_igscDeviceInit{"igsc_device_init_by_device"};
    const std::string str_igscDeviceClose{"igsc_device_close"};
    const std::string str_igscDeviceMemEccSet2{"igsc_ecc_config_set"};
    const std::string str_igscDeviceMemEccGet2{"igsc_ecc_config_get"};

    void *handle = NULL;
    char *error;
    struct igsc_device_handle igsc_handle;
    uint8_t cur_ecc_state = *cur = 0xFF;
    uint8_t pen_ecc_state = *pen = 0xFF;
    int ret;
    bool result = false;
    bool device_handle_inited = false;
    memset(&igsc_handle, 0, sizeof(igsc_handle));

    int (*igsc_device_init) (struct igsc_device_handle *handle, const char *device_path);
    int (*igsc_device_close) (struct igsc_device_handle *handle);
    int (*igsc_device_mem_ecc_set) (struct igsc_device_handle *handle, uint8_t req_state, uint8_t* cur_state, uint8_t* pen_state);
    int (*igsc_device_mem_ecc_get) (struct igsc_device_handle *handle, uint8_t* cur_state, uint8_t* pen_state);

    handle = dlopen(str_igscLibPath.c_str(), RTLD_LAZY);
    if (!handle) {
        handle = dlopen(str_igscLibPath0.c_str(), RTLD_LAZY);
        if (!handle) {
            XPUM_LOG_WARN("XPUM can't load igsc library.");
            goto out;
        }
    }

    dlerror();

    igsc_device_close = (int (*) (struct igsc_device_handle *handle)) dlsym(handle, str_igscDeviceClose.c_str());
    error = dlerror();
    if (error || igsc_device_close == NULL) {
        XPUM_LOG_WARN("XPUM can't load find igsc_device_close.");
        //goto out;
    }

    igsc_device_init = (int (*) (struct igsc_device_handle *handle, const char *device_path)) dlsym(handle, str_igscDeviceInit.c_str());
    error = dlerror();
    if (error || igsc_device_init == NULL) {
        XPUM_LOG_WARN("XPUM can't load find igsc_device_init_by_device.");
        //goto out;
    }

    igsc_device_mem_ecc_set = (int (*) (struct igsc_device_handle *handle, uint8_t req_state, uint8_t* cur_state, uint8_t* pen_state)) dlsym(handle, str_igscDeviceMemEccSet2.c_str());
    error = dlerror();
    if (error || igsc_device_mem_ecc_set == NULL) {
        XPUM_LOG_WARN("XPUM can't load find igsc_ecc_config_set.");
        *cur = 0x02; //can't find the interface, the library is too old.
        *pen = 0x02; //can't find the interface, the library is too old.
        goto out;
    }

    igsc_device_mem_ecc_get = (int (*) (struct igsc_device_handle *handle, uint8_t* cur_state, uint8_t* pen_state)) dlsym(handle, str_igscDeviceMemEccGet2.c_str());
    error = dlerror();
    if (error || igsc_device_mem_ecc_get == NULL) {
        XPUM_LOG_WARN("XPUM can't load find igsc_ecc_config_get.");
        *cur = 0x02; //can't find the interface, the library is too old.
        *pen = 0x02; //can't find the interface, the library is too old.
        goto out;
    }

    ret = (igsc_device_init)(&igsc_handle, path.c_str());
    if (ret) {
        XPUM_LOG_WARN("XPUM call igsc_device_init_by_device failed {}", ret);
        goto out;
    }
    device_handle_inited = true;

    if(getting) {
        ret = (igsc_device_mem_ecc_get)(&igsc_handle, &cur_ecc_state, &pen_ecc_state);
        if (ret) {
            XPUM_LOG_WARN("XPUM call igsc_ecc_config_get failed {}", ret);
            goto out;
        }else {
            *cur = cur_ecc_state;
            *pen = pen_ecc_state;
            result = true;
        }
    }else {
        ret = (igsc_device_mem_ecc_set)(&igsc_handle, req, &cur_ecc_state, &pen_ecc_state);
        if (ret) {
            XPUM_LOG_WARN("XPUM call igsc_ecc_config_set failed {}", ret);
            goto out;
        }else {
            *cur = cur_ecc_state;
            *pen = pen_ecc_state;
            result = true;
        }
    }

    out:
    if (device_handle_inited && igsc_device_close != NULL) {
        ret = (igsc_device_close)(&igsc_handle);
        if (ret) {
            XPUM_LOG_WARN("XPUM call igsc_device_close failed {}", ret);
            result = false;
        }
    }
    if (handle != NULL) {
        dlclose(handle);
        error = dlerror();
        if (error) {
            XPUM_LOG_WARN("XPUM can't close igsc library.");
            return false;
        }
    }
    return result;
}

xpum_result_t xpumGetEccState(xpum_device_id_t deviceId, bool* available, bool* configurable,
        xpum_ecc_state_t* current, xpum_ecc_state_t* pending, xpum_ecc_action_t* action) {
    *available = false;
    *configurable = false;
    *current = XPUM_ECC_STATE_UNAVAILABLE;
    *pending = XPUM_ECC_STATE_UNAVAILABLE;
    *action = XPUM_ECC_ACTION_NONE;
    uint8_t cur;
    uint8_t pen;

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    xpum_result_t res = validateDeviceId(deviceId);
    if (res != XPUM_OK) {
        return res;
    }

    std::string meiPath = device->getMeiDevicePath();
    //XPUM_LOG_INFO("XPUM meiPath {}", meiPath);

    if (callIgscMemoryEcc( meiPath, true, 0, &cur, &pen) == true) {
        *available = true;
        *configurable = true;
        if(cur == 0x00) {
            *current = XPUM_ECC_STATE_DISABLED;
        }else if (cur == 0x01) {
            *current = XPUM_ECC_STATE_ENABLED;
        } else {
            *current = XPUM_ECC_STATE_UNAVAILABLE;
        }

        if(pen == 0x00) {
            *pending = XPUM_ECC_STATE_DISABLED;
        }else if (pen == 0x01) {
            *pending = XPUM_ECC_STATE_ENABLED;
        } else {
            *pending = XPUM_ECC_STATE_UNAVAILABLE;
        }
        if (cur != pen) {
           *action = XPUM_ECC_ACTION_COLD_SYSTEM_REBOOT;
        } else {
           *action = XPUM_ECC_ACTION_NONE;
        }
        return XPUM_OK;
    } else {
        if (cur == 0x02 || pen == 0x02) {
            return XPUM_RESULT_MEMORY_ECC_LIB_NOT_SUPPORT;
        } else {
            return XPUM_GENERIC_ERROR;
        }
    }

#if 0    
    MemoryEcc* ecc = new MemoryEcc();
    if (Core::instance().getDeviceManager()->getEccState(std::to_string(deviceId), *ecc)) {
        *available = ecc->getAvailable();
        *configurable = ecc->getConfigurable();
        *current = (xpum_ecc_state_t)(ecc->getCurrent());
        *pending = (xpum_ecc_state_t)(ecc->getPending());
        *action = (xpum_ecc_action_t)(ecc->getAction()); 
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
#endif
}

xpum_result_t xpumSetEccState(xpum_device_id_t deviceId, xpum_ecc_state_t newState, bool* available, bool* configurable,
        xpum_ecc_state_t* current, xpum_ecc_state_t* pending, xpum_ecc_action_t* action) {
    *available = false;
    *configurable = false;
    *current = XPUM_ECC_STATE_UNAVAILABLE;
    *pending = XPUM_ECC_STATE_UNAVAILABLE;
    *action = XPUM_ECC_ACTION_NONE;
    uint8_t cur;
    uint8_t pen;
    uint8_t req;

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    xpum_result_t res = validateDeviceId(deviceId);
    if (res != XPUM_OK) {
        return res;
    }

    std::string meiPath = device->getMeiDevicePath();

    if(newState == XPUM_ECC_STATE_ENABLED) {
        req = 1;
    } else if (newState == XPUM_ECC_STATE_DISABLED){
        req = 0;
    } else {
        return XPUM_GENERIC_ERROR;
    }

    if (callIgscMemoryEcc( meiPath, false, req, &cur, &pen) == true) {
        *available = true;
        *configurable = true;
        if(cur == 0x00) {
            *current = XPUM_ECC_STATE_DISABLED;
        }else if (cur == 0x01) {
            *current = XPUM_ECC_STATE_ENABLED;
        } else {
            *current = XPUM_ECC_STATE_UNAVAILABLE;
        }

        if(pen == 0x00) {
            *pending = XPUM_ECC_STATE_DISABLED;
        }else if (pen == 0x01) {
            *pending = XPUM_ECC_STATE_ENABLED;
        } else {
            *pending = XPUM_ECC_STATE_UNAVAILABLE;
        }
        if (cur != pen) {
           *action = XPUM_ECC_ACTION_COLD_SYSTEM_REBOOT;
        } else {
           *action = XPUM_ECC_ACTION_NONE;
        }
         return XPUM_OK;     
    } else {
        if (cur == 0x02 || pen == 0x02) {
            return XPUM_RESULT_MEMORY_ECC_LIB_NOT_SUPPORT;
        } else {
            return XPUM_GENERIC_ERROR;
        }
    }

#if 0
    ecc_state_t newSt = (ecc_state_t)(newState);

    MemoryEcc* ecc = new MemoryEcc();
    if (Core::instance().getDeviceManager()->setEccState(std::to_string(deviceId), newSt, *ecc)) {
        *available = ecc->getAvailable();
        *configurable = ecc->getConfigurable();
        *current = (xpum_ecc_state_t)(ecc->getCurrent());
        *pending = (xpum_ecc_state_t)(ecc->getPending());
        *action = (xpum_ecc_action_t)(ecc->getAction()); 
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
#endif
}

///////////////////Policy//////////////////////
xpum_result_t xpumSetPolicy(xpum_device_id_t deviceId, xpum_policy_t policy) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    return Core::instance().getPolicyManager()->xpumSetPolicy(deviceId, policy);
}
xpum_result_t xpumSetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t policy) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    return Core::instance().getPolicyManager()->xpumSetPolicyByGroup(groupId, policy);
}
xpum_result_t xpumGetPolicy(xpum_device_id_t deviceId, xpum_policy_t resultList[], int *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    return Core::instance().getPolicyManager()->xpumGetPolicy(deviceId, resultList, count);
}
xpum_result_t xpumGetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t resultList[], int *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    return Core::instance().getPolicyManager()->xpumGetPolicyByGroup(groupId, resultList, count);
}
///////////////////Policy//////////////////////

xpum_result_t xpumStartDumpRawDataTask(xpum_device_id_t deviceId,
                                       xpum_device_tile_id_t tileId,
                                       const xpum_dump_type_t dumpTypeList[],
                                       const int count,
                                       const char *dumpFilePath,
                                       xpum_dump_raw_data_task_t *taskInfo) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    if (tileId == -1)
        res = validateDeviceId(deviceId);
    else
        res = validateDeviceIdAndTileId(deviceId, tileId);
    if (res != XPUM_OK)
        return res;
    return Core::instance().getDumpRawDataManager()->startDumpRawDataTask(deviceId, tileId, dumpTypeList, count, dumpFilePath, taskInfo);
}

xpum_result_t xpumStopDumpRawDataTask(xpum_dump_task_id_t taskId, xpum_dump_raw_data_task_t *taskInfo) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    return Core::instance().getDumpRawDataManager()->stopDumpRawDataTask(taskId, taskInfo);
}

xpum_result_t xpumListDumpRawDataTasks(xpum_dump_raw_data_task_t taskList[], int *count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    return Core::instance().getDumpRawDataManager()->listDumpRawDataTasks(taskList, count);
}

xpum_result_t xpumGetAMCSensorReading(xpum_sensor_reading_t data[], int *count) {
    return Core::instance().getFirmwareManager()->getAMCSensorReading(data, count);
}

} // end namespace xpum
