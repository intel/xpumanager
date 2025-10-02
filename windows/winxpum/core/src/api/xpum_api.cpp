/*
 *  Copyright (C) 2021-2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file xpum_api.cpp
 */



#include "pch.h"
#include "xpum_api.h"
#include "internal_api.h"
#include "device_model.h"
#include "infrastructure/logger.h"
#include "infrastructure/exception/level_zero_initialization_exception.h"
#include "infrastructure/version.h"
#include "infrastructure/property.h"
#include "infrastructure/utility.h"
#include "core/core.h"

#include <map>
#include <fstream>
#pragma warning(disable : 4996)

namespace xpum {

const char* getXpumDevicePropertyNameString(xpum_device_property_name_t name) {
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
        case XPUM_DEVICE_PROPERTY_OAM_SOCKET_ID:
            return "OAM_SOCKET_ID";
        case XPUM_DEVICE_PROPERTY_PCIE_GENERATION:
            return "PCIE_GENERATION";
        case XPUM_DEVICE_PROPERTY_PCIE_MAX_LINK_WIDTH:
            return "PCIE_MAX_LINK_WIDTH";
        case XPUM_DEVICE_PROPERTY_PCIE_MAX_BANDWIDTH:
            return "PCIE_MAX_BANDWIDTH";
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
        case XPUM_DEVICE_PROPERTY_GFX_PSCBIN_FIRMWARE_NAME:
            return "GFX_PSCBIN_FIRMWARE_NAME";
        case XPUM_DEVICE_PROPERTY_GFX_PSCBIN_FIRMWARE_VERSION:
            return "GFX_PSCBIN_FIRMWARE_VERSION";
        case XPUM_DEVICE_PROPERTY_MEMORY_ECC_STATE:
            return "MEMORY_ECC_STATE";
        case XPUM_DEVICE_PROPERTY_GFX_FIRMWARE_STATUS:
            return "GFX_FIRMWARE_STATUS";
        case XPUM_DEVICE_PROPERTY_SKU_TYPE:
            return "SKU_TYPE";
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

xpum_result_t xpumInit() {
    try {
        Logger::init();
        XPUM_LOG_INFO("XPU Manager:\t{}", Version::getVersion());
        XPUM_LOG_INFO("Build:\t\t{}", Version::getVersionGit());
        XPUM_LOG_INFO("Level Zero:\t{}", Version::getZeLibVersion());
        Core::instance().init();
    } catch (LevelZeroInitializationException& e) {
        XPUM_LOG_ERROR("xpumInit LevelZeroInitializationException");
        XPUM_LOG_ERROR("Failed to init xpum core: {}", e.what());
        Core::instance().setZeInitialized(false);
        return XPUM_LEVEL_ZERO_INITIALIZATION_ERROR;
    } catch (BaseException& e) {
        XPUM_LOG_ERROR("Failed to init xpum core: {}", e.what());
        return XPUM_GENERIC_ERROR;
    } catch (std::exception& e) {
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

xpum_result_t xpumVersionInfo(xpum_version_info versionInfoList[], int* count) {
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

xpum_result_t xpumGetDeviceList(xpum_device_basic_info deviceList[], int* count) {
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
        auto& p_device = devices[i];
        auto& info = deviceList[i];
        info.deviceId = stoi(p_device->getId());
        info.type = GPU;
        std::vector<Property> properties;
        p_device->getProperties(properties);

        for (Property& prop : properties) {
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
                case XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_FUNCTION_TYPE:
                    info.functionType = static_cast<xpum_device_function_type_t>(prop.getValueInt());
                    break;
                default:
                    break;
            }
        }
    }
    *count = deviceCount;

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
        case XPUM_DEVICE_PROPERTY_OAM_SOCKET_ID:
            return XPUM_DEVICE_PROPERTY_INTERNAL_OAM_SOCKET_ID;
        case XPUM_DEVICE_PROPERTY_PCIE_GENERATION:
            return XPUM_DEVICE_PROPERTY_INTERNAL_PCIE_GENERATION;
        case XPUM_DEVICE_PROPERTY_PCIE_MAX_LINK_WIDTH:
            return XPUM_DEVICE_PROPERTY_INTERNAL_PCIE_MAX_LINK_WIDTH;
        case XPUM_DEVICE_PROPERTY_PCIE_MAX_BANDWIDTH:
            return XPUM_DEVICE_PROPERTY_INTERNAL_PCIE_MAX_BANDWIDTH;
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
        case XPUM_DEVICE_PROPERTY_GFX_DATA_FIRMWARE_VERSION:
            return XPUM_DEVICE_PROPERTY_INTERNAL_GFX_DATA_FIRMWARE_VERSION;
        case XPUM_DEVICE_PROPERTY_AMC_FIRMWARE_NAME:
            return XPUM_DEVICE_PROPERTY_INTERNAL_AMC_FIRMWARE_NAME;
        case XPUM_DEVICE_PROPERTY_AMC_FIRMWARE_VERSION:
            return XPUM_DEVICE_PROPERTY_INTERNAL_AMC_FIRMWARE_VERSION;
        case XPUM_DEVICE_PROPERTY_GFX_PSCBIN_FIRMWARE_NAME:
            return XPUM_DEVICE_PROPERTY_INTERNAL_GFX_PSCBIN_FIRMWARE_NAME;
        case XPUM_DEVICE_PROPERTY_GFX_PSCBIN_FIRMWARE_VERSION:
            return XPUM_DEVICE_PROPERTY_INTERNAL_GFX_PSCBIN_FIRMWARE_VERSION;
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
        case XPUM_DEVICE_PROPERTY_SKU_TYPE:
            return XPUM_DEVICE_PROPERTY_INTERNAL_SKU_TYPE;
        default:
            return XPUM_DEVICE_PROPERTY_INTERNAL_MAX;
    }
}

std::string eccStateToString(xpum_ecc_state_t state) {
    if (state == XPUM_ECC_STATE_UNAVAILABLE) {
        return "";
    }
    if (state == XPUM_ECC_STATE_ENABLED) {
        return "enabled";
    }
    if (state == XPUM_ECC_STATE_DISABLED) {
        return "disabled";
    }
    return "";
}

xpum_result_t xpumGetDeviceProperties(xpum_device_id_t deviceId, xpum_device_properties_t* pXpumProperties) {
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

    if (Core::instance().getFirmwareManager()) {
        Core::instance().getFirmwareManager()->updateFWVersionProps();
    }

    std::vector<std::shared_ptr<Device>> devices;
    Core::instance().getDeviceManager()->getDeviceList(devices);

    for (auto& p_device : devices) {
        if (deviceId == stoi(p_device->getId())) {
            pXpumProperties->deviceId = deviceId;
            std::vector<Property> properties;
            p_device->getProperties(properties);

            std::map<xpum_device_internal_property_name_t, Property> prop_map;

            for (size_t i = 0; i < properties.size(); i++) {
                auto& prop = properties[i];
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
                auto& prop = prop_map[propNameInternal];
                std::string value = prop.getValue();

                if (propName == XPUM_DEVICE_PROPERTY_GFX_FIRMWARE_VERSION) {
                    value.erase(remove_if(value.begin(), value.end(), invalidChar), value.end());
                }
                auto& copy = pXpumProperties->properties[propertyLen++];
                copy.name = propName;
                strcpy_s(copy.value, value.c_str());
            }
            {
                bool available;
                bool configurable;
                xpum_ecc_state_t current, pending;
                xpum_ecc_action_t action;
                res = xpumGetEccState(deviceId, &available, &configurable, &current, &pending, &action);
                auto& copy = pXpumProperties->properties[propertyLen++];
                copy.name = XPUM_DEVICE_PROPERTY_MEMORY_ECC_STATE;
                std::string value = (current == XPUM_ECC_STATE_ENABLED) ? "enabled" : "disabled";
                strcpy_s(copy.value, value.c_str());
            }

            {
                auto& copy = pXpumProperties->properties[propertyLen++];
                copy.name = XPUM_DEVICE_PROPERTY_GFX_FIRMWARE_STATUS;
                std::string fw_status_str;
                if (Core::instance().getFirmwareManager()) {
                    auto fw_status = Core::instance().getFirmwareManager()->getGfxFwStatus(deviceId);
                    fw_status_str = FirmwareManager::transGfxFwStatusToString(fw_status);
                } else {
                    fw_status_str = "";
                }
                strcpy_s(copy.value, fw_status_str.c_str());
            }

            pXpumProperties->propertyLen = propertyLen;

            return XPUM_OK;
        }
    }

    return XPUM_RESULT_DEVICE_NOT_FOUND;
}

xpum_result_t xpumGetDeviceIdByBDF(const char* bdf, xpum_device_id_t* deviceId) {
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

    *deviceId = std::stoi(device->getId());
    return XPUM_OK;
}

xpum_result_t xpumGetAMCFirmwareVersions(xpum_amc_fw_version_t versionList[], int* count, const char* username, const char* password) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }
    auto fwMgr = Core::instance().getFirmwareManager();
    if (fwMgr == nullptr) {
        return XPUM_RESULT_FW_MGMT_NOT_INIT;
    }
    std::vector<std::string> versions;
    res = fwMgr->getAmcFwVersions(&versions, username, password);
    if (res != XPUM_OK) {
        *count = 0;
        return res;
    } else {
        if (*count >= versions.size()) {
            for (int i = 0; i < versions.size(); i++) {
                std::strncpy(versionList[i].version, versions.at(i).c_str(), versions.at(i).size());
            }
            *count = versions.size();
        } else {
            return XPUM_BUFFER_TOO_SMALL;
        }
    }
    return XPUM_OK;
}

xpum_result_t xpumGetAMCFirmwareVersionsErrorMsg(char* buffer, int* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetSerialNumberAndAmcFwVersion(xpum_device_id_t deviceId, const char* username, const char* password, char serialNumber[XPUM_MAX_STR_LENGTH], char amcFwVersion[XPUM_MAX_STR_LENGTH]) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetDeviceStandbys(xpum_device_id_t deviceId,
                                    xpum_standby_data_t dataArray[], uint32_t* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumSetDeviceStandby(xpum_device_id_t deviceId,
                                   const xpum_standby_data_t standby) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetDevicePowerProps(xpum_device_id_t deviceId,
                                      xpum_power_prop_data_t dataArray[], uint32_t* count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }
    if (Core::instance().getDeviceManager() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }
    res = validateDeviceId(deviceId);
    if (res != XPUM_OK) {
        return res;
    }
    bool s_supported;
    int32_t max_limit;
    Core::instance().getDeviceManager()->getDevicePowerMaxLimit(std::to_string(deviceId), max_limit, s_supported);

    if (*count < 1) {
        return XPUM_BUFFER_TOO_SMALL;
    }

    *count = 1;

    if (dataArray == nullptr) {
        return XPUM_OK;
    }
    dataArray[0].on_subdevice = false;
    dataArray[0].can_control = false;
    dataArray[0].subdevice_Id = -1;
    dataArray[0].min_limit = 1;
    dataArray[0].max_limit = max_limit / 1000;
    dataArray[0].default_limit = dataArray[0].max_limit;
    return XPUM_OK;
}

xpum_result_t xpumGetDevicePowerLimitsExt(xpum_device_id_t deviceId,
                                          int32_t tileId,
					  std::vector<xpum_power_domain_ext_t>& power_domains_ext) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }
    if (Core::instance().getDeviceManager() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }
    res = validateDeviceId(deviceId);
    if (res != XPUM_OK) {
        return res;
    }    
    return Core::instance().getDeviceManager()->getDevicePowerLimitsExt(std::to_string(deviceId), power_domains_ext);
}

xpum_result_t xpumGetDevicePowerLimits(xpum_device_id_t deviceId, int32_t tileId, xpum_power_limits_t* pPowerLimits) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }
    if (Core::instance().getDeviceManager() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }
    res = validateDeviceId(deviceId);
    if (res != XPUM_OK) {
        return res;
    }
    bool Sus_supported;
    int32_t Sus_power;
    Core::instance().getDeviceManager()->getDeviceSusPower(std::to_string(deviceId), Sus_power, Sus_supported);
    pPowerLimits->sustained_limit.power = Sus_power;
    pPowerLimits->sustained_limit.enabled = Sus_supported;
    return XPUM_OK;
}

xpum_result_t xpumSetDevicePowerLimitsExt(xpum_device_id_t device_id, int32_t tile_id,
					  const xpum_power_limit_ext_t& power_limit_ext) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(device_id));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    if (tile_id != -1) {
        xpum_result_t res = validateDeviceIdAndTileId(device_id, tile_id);
        if (res != XPUM_OK) {
            return res;
        }
    } else {
        xpum_result_t res = validateDeviceId(device_id);
        if (res != XPUM_OK) {
            return res;
        }
    }

    Power_limit_ext_t pwr_limit_ext = {WATTS_TO_MILLIWATS(power_limit_ext.limit), power_limit_ext.level};
    return Core::instance().getDeviceManager()->setDevicePowerLimitsExt(std::to_string(device_id), tile_id,
								       pwr_limit_ext);
}

xpum_result_t xpumSetDevicePowerSustainedLimits(xpum_device_id_t deviceId, int32_t tileId, const xpum_power_sustained_limit_t sustained_limit) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }
    if (Core::instance().getDeviceManager() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }
    res = validateDeviceId(deviceId);
    if (res != XPUM_OK) {
        return res;
    }
    bool ret = Core::instance().getDeviceManager()->setDevicePowerSustainedLimits(std::to_string(deviceId), sustained_limit.power * 1000);
    if (ret == true) {
        return XPUM_OK;
    } else {
        return XPUM_GENERIC_ERROR;
    }
}

xpum_result_t xpumGetDeviceFrequencyRanges(xpum_device_id_t deviceId, xpum_frequency_range_t* dataArray, uint32_t* count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }
    if (Core::instance().getDeviceManager() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }
    res = validateDeviceId(deviceId);
    if (res != XPUM_OK) {
        return res;
    }

    double min, max;
    std::string clocks;
    bool freq_supported;

    for (int i = 0; i < *count; ++i) {
        Core::instance().getDeviceManager()->getDeviceFrequencyRange(std::to_string(deviceId), dataArray[i].subdevice_Id, min, max, clocks, freq_supported);
        dataArray[i].min = min;
        dataArray[i].max = max;
        dataArray[i].type = XPUM_GPU_FREQUENCY;
    }
    return XPUM_OK;
}

xpum_result_t xpumSetDeviceFrequencyRange(xpum_device_id_t deviceId, const xpum_frequency_range_t frequency) {

    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }
    if (Core::instance().getDeviceManager() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }
    res = validateDeviceId(deviceId);
    if (res != XPUM_OK) {
        return res;
    }
    bool ret = Core::instance().getDeviceManager()->setDeviceFrequencyRange(std::to_string(deviceId), (int32_t) frequency.subdevice_Id, (double)frequency.min, (double)frequency.max);
    if (ret == true) {
        return XPUM_OK;
    } else {
        return XPUM_GENERIC_ERROR;
    }
}

    xpum_result_t xpumGetDeviceSchedulers(xpum_device_id_t deviceId,
                                          xpum_scheduler_data_t dataArray[], uint32_t* count) {
        return XPUM_API_UNSUPPORTED;
    }

    xpum_result_t xpumSetDeviceSchedulerTimeoutMode(xpum_device_id_t deviceId,
                                                    const xpum_scheduler_timeout_t sched_timeout) {
        return XPUM_API_UNSUPPORTED;
    }

    xpum_result_t xpumSetDeviceSchedulerTimesliceMode(xpum_device_id_t deviceId,
                                                      const xpum_scheduler_timeslice_t sched_timeslice) {
        return XPUM_API_UNSUPPORTED;
    }

    xpum_result_t xpumSetDeviceSchedulerExclusiveMode(xpum_device_id_t deviceId,
                                                      const xpum_scheduler_exclusive_t sched_exclusive) {
        return XPUM_API_UNSUPPORTED;
    }

    xpum_result_t xpumGetFreqAvailableClocks(xpum_device_id_t deviceId, uint32_t tileId, double dataArray[], uint32_t* count) {
        xpum_result_t res = Core::instance().apiAccessPreCheck();
        if (res != XPUM_OK) {
            return res;
        }
        if (Core::instance().getDeviceManager() == nullptr) {
            return XPUM_NOT_INITIALIZED;
        }
        res = validateDeviceId(deviceId);
        if (res != XPUM_OK) {
            return res;
        }

        std::vector<double> clocksList;
        Core::instance().getDeviceManager()->getFreqAvailableClocks(std::to_string(deviceId), tileId, clocksList);
        if (*count < clocksList.size()) {
            return XPUM_BUFFER_TOO_SMALL;
        }

        *count = clocksList.size();

        if (dataArray == nullptr) {
            return XPUM_OK;
        }
        for (int i = 0; i < *count; ++i) {
            dataArray[i] = clocksList.at(i);
        }
        return XPUM_OK;
    }

    xpum_result_t xpumGetPerformanceFactor(xpum_device_id_t deviceId, xpum_device_performancefactor_t dataArray[], uint32_t* count) {
        return XPUM_API_UNSUPPORTED;
    }

    xpum_result_t xpumSetPerformanceFactor(xpum_device_id_t deviceId, xpum_device_performancefactor_t performanceFactor) {
        return XPUM_API_UNSUPPORTED;
    }

    xpum_result_t xpumGetFabricPortConfig(xpum_device_id_t deviceId, xpum_fabric_port_config_t dataArray[], uint32_t* count) {
        return XPUM_API_UNSUPPORTED;
    }

    xpum_result_t xpumSetFabricPortConfig(xpum_device_id_t deviceId, xpum_fabric_port_config_t fabricPortConfig) {
        return XPUM_API_UNSUPPORTED;
    }

    xpum_result_t xpumGetEccState(xpum_device_id_t deviceId, bool* available, bool* configurable,
                                  xpum_ecc_state_t* current, xpum_ecc_state_t* pending, xpum_ecc_action_t* action) {
        xpum_result_t res = Core::instance().apiAccessPreCheck();
        if (res != XPUM_OK) {
            return res;
        }
        if (Core::instance().getDeviceManager() == nullptr) {
            return XPUM_NOT_INITIALIZED;
        }
        if (Core::instance().getFirmwareManager() == nullptr) {
            return XPUM_NOT_INITIALIZED;
        }

        res = validateDeviceId(deviceId);
        if (res != XPUM_OK) {
            return res;
        }

        std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
        if (device == nullptr) {
            return XPUM_RESULT_DEVICE_NOT_FOUND;
        }

        if(Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId))->getDeviceModel() == XPUM_DEVICE_MODEL_PVC){
            *available = true;
            *configurable = false;

            *current = XPUM_ECC_STATE_ENABLED;
            *pending = XPUM_ECC_STATE_ENABLED;

            *action = XPUM_ECC_ACTION_NONE;

            return XPUM_OK;
        }

        if (device->getDeviceModel() < XPUM_DEVICE_MODEL_BMG) {
            auto fwMgr = Core::instance().getFirmwareManager();

            uint8_t currentEcc, pendingEcc;
            currentEcc = 0xFF;
            pendingEcc = 0xFF;
            fwMgr->getSimpleEccState(deviceId, currentEcc, pendingEcc);
            *available = true;
            *configurable = true;
            *action = XPUM_ECC_ACTION_COLD_SYSTEM_REBOOT;
            currentEcc == 1 ? (*current) = XPUM_ECC_STATE_ENABLED : (*current) = XPUM_ECC_STATE_DISABLED;
            pendingEcc == 1 ? (*pending) = XPUM_ECC_STATE_ENABLED : (*pending) = XPUM_ECC_STATE_DISABLED;
            return XPUM_OK;
        }

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
    }

    xpum_result_t xpumSetEccState(xpum_device_id_t deviceId, xpum_ecc_state_t newState, bool* available, bool* configurable,
                                  xpum_ecc_state_t* current, xpum_ecc_state_t* pending, xpum_ecc_action_t* action) {
        xpum_result_t res = Core::instance().apiAccessPreCheck();
        if (res != XPUM_OK) {
            return res;
        }
        if (Core::instance().getDeviceManager() == nullptr) {
            return XPUM_NOT_INITIALIZED;
        }
        if (Core::instance().getFirmwareManager() == nullptr) {
            return XPUM_NOT_INITIALIZED;
        }

        auto fwMgr = Core::instance().getFirmwareManager();

        res = validateDeviceId(deviceId);
        if (res != XPUM_OK) {
            return res;
        }
        uint8_t currentEcc, pendingEcc, requestEcc;
        currentEcc = 0xFF;
        pendingEcc = 0xFF;
        requestEcc = newState == XPUM_ECC_STATE_ENABLED ? 1 : 0;
        *available = true;
        *configurable = true;
        *action = XPUM_ECC_ACTION_COLD_SYSTEM_REBOOT;
        res = fwMgr->setSimpleEccState(deviceId, requestEcc, currentEcc, pendingEcc);
        *current = currentEcc == 1 ? XPUM_ECC_STATE_ENABLED : XPUM_ECC_STATE_DISABLED;
        *pending = pendingEcc == 1 ? XPUM_ECC_STATE_ENABLED : XPUM_ECC_STATE_DISABLED;
        return res;
    }

    xpum_result_t xpumRunFirmwareFlash(xpum_device_id_t deviceId, xpum_firmware_flash_job * job, const char* username, const char* password) {
        return xpumRunFirmwareFlashEx(deviceId, job, username, password, false);
    }

    static xpum_result_t validateFwImagePath(xpum_firmware_flash_job * job) {
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

    xpum_result_t xpumRunFirmwareFlashEx(xpum_device_id_t deviceId, xpum_firmware_flash_job * job, const char* username, const char* password, bool force) {
        xpum_result_t res = Core::instance().apiAccessPreCheck();
        if (res != XPUM_OK) {
            return res;
        }
        auto fwMgr = Core::instance().getFirmwareManager();
        if (fwMgr == nullptr) {
            return XPUM_RESULT_FW_MGMT_NOT_INIT;
        }
        if (deviceId == XPUM_DEVICE_ID_ALL_DEVICES && job->type != XPUM_DEVICE_FIRMWARE_AMC) {
            return XPUM_RESULT_DEVICE_NOT_FOUND;
        }
        if (deviceId != XPUM_DEVICE_ID_ALL_DEVICES) {
            res = validateDeviceId(deviceId);
            if (res != XPUM_OK) {
                return res;
            }
        }
        res = validateFwImagePath(job);
        if (res != XPUM_OK) {
            return res;
        }
        /*
        * TODO: AMC FW update implementation
        */
        switch (job->type) {
            case XPUM_DEVICE_FIRMWARE_GFX:
                return fwMgr->runGSCFirmwareFlash(deviceId, job->filePath, force);
                break;
            case XPUM_DEVICE_FIRMWARE_GFX_DATA:
                return fwMgr->runFwDataFlash(deviceId, job->filePath);
                break;
            case XPUM_DEVICE_FIRMWARE_AMC:
                return fwMgr->runAMCFlash(job->filePath, username, password);
                break;
            default:
                return XPUM_GENERIC_ERROR;
        }
        return XPUM_GENERIC_ERROR;
    }

    xpum_result_t xpumGetFirmwareFlashResult(xpum_device_id_t deviceId,
                                             xpum_firmware_type_t firmwareType,
                                             xpum_firmware_flash_task_result_t * result) {
        xpum_result_t res = Core::instance().apiAccessPreCheck();
        if (res != XPUM_OK) {
            return res;
        }
        auto fwMgr = Core::instance().getFirmwareManager();
        if (fwMgr == nullptr) {
            return XPUM_RESULT_FW_MGMT_NOT_INIT;
        }

        if (firmwareType != XPUM_DEVICE_FIRMWARE_AMC) {
            res = validateDeviceId(deviceId);
            if (res != XPUM_OK) {
                return res;
            }
            result->deviceId = deviceId;
        } else {
            result->deviceId = XPUM_DEVICE_ID_ALL_DEVICES;
        }

        result->type = firmwareType;
        fwMgr->getFlashResult(deviceId, result);
        return XPUM_OK;
    }

    xpum_result_t xpumGetFirmwareFlashErrorMsg(char* buffer, int* count) {
        xpum_result_t res = Core::instance().apiAccessPreCheck();
        if (res != XPUM_OK) {
            return res;
        }
        auto fwMgr = Core::instance().getFirmwareManager();
        if (fwMgr == nullptr) {
            return XPUM_RESULT_FW_MGMT_NOT_INIT;
        }
        auto errMsg = Core::instance().getFirmwareManager()->getFlashFwErrMsg();
        if (buffer == nullptr) {
            *count = errMsg.length() + 1;
            return XPUM_OK;
        }
        if (*count < (int)errMsg.length() + 1) {
            return XPUM_BUFFER_TOO_SMALL;
        }
        strcpy_s(buffer, (size_t)(*count), errMsg.c_str());
        buffer[errMsg.length()] = '\0';
        return XPUM_OK;
    }

    xpum_result_t xpumGetRealtimeMetrics(xpum_device_id_t deviceId, xpum_device_realtime_metrics_t dataList[], uint32_t * count) {
        xpum_result_t res = Core::instance().apiAccessPreCheck();
        if (res != XPUM_OK) {
            return res;
        }

        if (Core::instance().getDeviceManager() == nullptr) {
            return XPUM_NOT_INITIALIZED;
        }

        auto pDevice = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
        if (pDevice == nullptr)
            return XPUM_RESULT_DEVICE_NOT_FOUND;

        Property prop;
        Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId))->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_SUBDEVICE, prop);
        uint32_t num_subdevice = prop.getValueInt();
        if (dataList == nullptr) {
            *count = num_subdevice + 1;
            return XPUM_OK;
        }

        std::map<MeasurementType, std::shared_ptr<MeasurementData>> m_datas = pDevice->getRealtimeMetrics();
        auto datas_iter = m_datas.begin();
        xpum_device_realtime_metrics_t device_realtime_metrics {};
        device_realtime_metrics.deviceId = deviceId;
        device_realtime_metrics.isTileData = false;
        device_realtime_metrics.count = 0;

        while (datas_iter != m_datas.end()) {
            if (datas_iter->second->hasDataOnDevice()) {
                xpum_device_realtime_metric_t metric;
                MeasurementType type = datas_iter->first;
                metric.metricsType = Utility::xpumStatsTypeFromMeasurementType(type);
                metric.scale = datas_iter->second->getScale();
                if (Utility::isCounterMetric(type)) {
                    metric.isCounter = true;

                } else {
                    metric.isCounter = false;
                }
                metric.value = datas_iter->second->getCurrent();
                device_realtime_metrics.dataList[device_realtime_metrics.count++] = metric;
            }
            ++datas_iter;
        }

        uint32_t index = 0;
        if (index >= *count) {
            return XPUM_BUFFER_TOO_SMALL;
        }
        dataList[index++] = device_realtime_metrics;

        for (uint32_t i = 0; i < num_subdevice; i++) {
            xpum_device_realtime_metrics_t subdevice_realtime_metrics {};
            subdevice_realtime_metrics.deviceId = deviceId;
            subdevice_realtime_metrics.tileId = i;
            subdevice_realtime_metrics.isTileData = true;
            subdevice_realtime_metrics.count = 0;
            datas_iter = m_datas.begin();
            while (datas_iter != m_datas.end()) {
                if (datas_iter->second->hasSubdeviceData() && datas_iter->second->getSubdeviceDatas()->find(i) != datas_iter->second->getSubdeviceDatas()->end() && datas_iter->second->getSubdeviceDataCurrent(i) != UINT64_MAX) {
                    xpum_device_realtime_metric_t metric;
                    MeasurementType type = datas_iter->first;
                    metric.metricsType = Utility::xpumStatsTypeFromMeasurementType(type);
                    metric.scale = datas_iter->second->getScale();
                    if (Utility::isCounterMetric(type)) {
                        metric.isCounter = true;
                    } else {
                        metric.isCounter = false;
                    }
                    metric.value = datas_iter->second->getSubdeviceDataCurrent(i);
                    subdevice_realtime_metrics.dataList[subdevice_realtime_metrics.count++] = metric;
                }
                ++datas_iter;
            }
            if (index >= *count) {
                return XPUM_BUFFER_TOO_SMALL;
            }
            dataList[index++] = subdevice_realtime_metrics;
        }
        *count = index;
        return XPUM_OK;
    }

    xpum_result_t xpumGetRealtimeMetricsEx(xpum_device_id_t deviceIdList[], uint32_t deviceCount, xpum_device_realtime_metrics_t dataList[], uint32_t * count) {
        xpum_result_t res = Core::instance().apiAccessPreCheck();
        if (res != XPUM_OK) {
            return res;
        }

        for (uint32_t i = 0; i < deviceCount; i++) {
            xpum_device_id_t deviceId = deviceIdList[i];
            res = validateDeviceId(deviceId);
            if (res != XPUM_OK) {
                return res;
            }
        }

        if (dataList == nullptr) {
            *count = 0;
            for (uint32_t i = 0; i < deviceCount; i++) {
                xpum_device_id_t deviceId = deviceIdList[i];
                uint32_t count_ = 0;
                res = xpumGetRealtimeMetrics(deviceId, dataList, &count_);
                if (res != XPUM_OK) {
                    return res;
                }
                *count += count_;
            }
            return XPUM_OK;
        } else {
            uint32_t used = 0;
            uint32_t count_;
            for (uint32_t i = 0; i < deviceCount; i++) {
                xpum_device_id_t deviceId = deviceIdList[i];
                count_ = *count - used;
                if (count_ <= 0) {
                    return XPUM_BUFFER_TOO_SMALL;
                }
                res = xpumGetRealtimeMetrics(deviceId, dataList + used, &count_);
                if (res != XPUM_OK) {
                    return res;
                }
                used += count_;
            }
            *count = used;
            return XPUM_OK;
        }
    }

    xpum_result_t xpumGetSiblingDevices(xpum_device_id_t deviceId,
                                        xpum_device_id_t deviceList[], uint32_t * count) {
        xpum_result_t res = Core::instance().apiAccessPreCheck();
        if (res != XPUM_OK) {
            return res;
        }
        std::vector<int> devices =
            Core::instance().getFirmwareManager()->getSiblingDevices(deviceId);
        if (deviceList == nullptr) {
            *count = (uint32_t)devices.size();
        } else {
            if (*count < devices.size()) {
                return XPUM_BUFFER_TOO_SMALL;
            }
            int i = 0;
            for (auto id : devices) {
                deviceList[i] = id;
                i++;
            }
        }
        return XPUM_OK;
    }
} // end namespace xpum
