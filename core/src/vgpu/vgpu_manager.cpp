/* 
 *  Copyright (C) 2026 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file vgpu_manager.cpp
 */


#include <fstream>
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <dirent.h>
#include <algorithm>
#include <sys/stat.h>
#include <iomanip>
#include <dlfcn.h>

#include "./vgpu_manager.h"
#include "core/core.h"
#include "infrastructure/xpum_config.h"
#include "infrastructure/configuration.h"
#include "infrastructure/utility.h"
#include "infrastructure/handle_lock.h"
#include "xpum_api.h"

namespace xpum {

static bool is_path_exist(const std::string &s) {
  struct stat buffer;
  return (stat(s.c_str(), &buffer) == 0);
}

static uint32_t getVFMaxNumberByPciDeviceId(int deviceId) {
    switch (deviceId) {
        case 0x56c0:
        case 0x56c1:
        case 0x56c2:
            return 31;
        case 0x0bd4:
        case 0x0bd5:
        case 0x0bd6:
            return 62;
        case 0x0bda:
        case 0x0bdb:
        case 0x0b6e:
            return 63;
        case 0xe211:
        case 0xe212:
        case 0xe222:
            return 24;
        case 0xe223:
            return 12;
        default:
            return 0;
    }
}

static void readFile(const std::string& path, std::string& content);

xpum_result_t VgpuManager::createVf(xpum_device_id_t deviceId, xpum_vgpu_config_t* param) {
    XPUM_LOG_DEBUG("vgpuCreateVf, {}, {}, {}", deviceId, param->numVfs, param->lmemPerVf);
    auto res = vgpuValidateDevice(deviceId);
    if (res != XPUM_OK) {
        return res;
    }

    DeviceSriovInfo deviceInfo;
    if (!loadSriovData(deviceId, deviceInfo)) {
        return XPUM_VGPU_SYSFS_ERROR;
    }

    std::unique_lock<std::mutex> lock(mutex);

    std::string numVfsString;
    std::stringstream numVfsPath;
    numVfsPath << "/sys/class/drm/" << deviceInfo.drmPath << "/device/sriov_numvfs";
    try {
        readFile(numVfsPath.str(), numVfsString);
    } catch (std::ios::failure &e) {
        return XPUM_VGPU_SYSFS_ERROR;
    }
    if (std::stoi(numVfsString) > 0) {
        return XPUM_VGPU_DIRTY_PF;
    }

    Property prop;
    Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId))->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_DEVICE_ID, prop);
    int pciDeviceId = std::stoi(prop.getValue().substr(2), nullptr, 16);
    if (param->numVfs == 0 || param->numVfs > getVFMaxNumberByPciDeviceId(pciDeviceId)){
        XPUM_LOG_ERROR("Configuration item for {} VFs out of range", param->numVfs);
        return XPUM_VGPU_INVALID_NUMVFS;
    }
    if (deviceInfo.numTiles > 1 && param->numVfs > 1 && param->numVfs % 2 != 0) {
        XPUM_LOG_ERROR("Configuration item for {} VFs invalid for two-tiles cards", param->numVfs);
        return XPUM_VGPU_INVALID_NUMVFS;
    }

    AttrFromConfigFile attrs = {};
    bool readFlag = readConfigFromFile(deviceId, param->numVfs, attrs);
    if (!readFlag) {
        return XPUM_VGPU_NO_CONFIG_FILE;
    }
    if (attrs.vfLmem == 0) {
        XPUM_LOG_ERROR("Configuration item for {} VFs not found", param->numVfs);
        return XPUM_VGPU_INVALID_NUMVFS;
    }

    uint64_t lmemToUse = 0;
    if (param->lmemPerVf > 0) {
        lmemToUse = param->lmemPerVf;
    } else if (deviceInfo.eccState == XPUM_ECC_STATE_ENABLED) {
        lmemToUse = attrs.vfLmemEcc;
    } else {
        lmemToUse = attrs.vfLmem;
    }

    if (deviceInfo.lmemSizeFree < lmemToUse * param->numVfs) {
        XPUM_LOG_ERROR("LMEM size too large");
        return XPUM_VGPU_INVALID_LMEM;
    }
    return createVfInternal(deviceInfo, attrs, param->numVfs, lmemToUse) ? XPUM_OK : XPUM_VGPU_CREATE_VF_FAILED;
}

/*
 *  1. Get number of VFs
 *  2. Get intersted value in the path of PF and each VF
 */
xpum_result_t VgpuManager::getFunctionList(xpum_device_id_t deviceId, std::vector<xpum_vgpu_function_info_t> &result) {
    XPUM_LOG_DEBUG("getFunctionList, device id: {}", deviceId);
    auto res = vgpuValidateDevice(deviceId);
    if (res != XPUM_OK) {
        return res;
    }
    
    DeviceSriovInfo deviceInfo;
    if (!loadSriovData(deviceId, deviceInfo)) {
        return XPUM_VGPU_SYSFS_ERROR;
    }
    std::string numVfsString;
    std::string devicePath = std::string("/sys/class/drm/") + deviceInfo.drmPath;
    XPUM_LOG_DEBUG("device Path: {}", devicePath);
    try {
        readFile(devicePath + "/device/sriov_numvfs", numVfsString);
    } catch (std::ios::failure &e) {
        return XPUM_VGPU_SYSFS_ERROR;
    }

    int numVfs = std::stoi(numVfsString);
    XPUM_LOG_DEBUG("{} VF detected.", numVfs);
    std::string debugfsPath = std::string("/sys/kernel/debug/dri/") + deviceInfo.bdfAddress;
    /*
     *  Put PF info into index 0, and VF1..n into index 1..n respectively
     */
    for (int functionIndex = 0; functionIndex < numVfs + 1; functionIndex++) {
        xpum_vgpu_function_info_t info = {};
        info.functionType = (functionIndex == 0 ? DEVICE_FUNCTION_TYPE_PHYSICAL : DEVICE_FUNCTION_TYPE_VIRTUAL);
        std::string lmemString, uevent, lmemPath, ueventPath;
        if (deviceInfo.deviceModel == XPUM_DEVICE_MODEL_ATS_M_1 || deviceInfo.deviceModel == XPUM_DEVICE_MODEL_ATS_M_3 || deviceInfo.deviceModel == XPUM_DEVICE_MODEL_ATS_M_1G) {
            if (functionIndex == 0) {
                lmemPath = devicePath + "/iov/pf/gt/available/lmem_free";
            } else {
                lmemPath = devicePath + "/iov/vf" + std::to_string(functionIndex) + "/gt/lmem_quota";
            }
            try {
                readFile(lmemPath, lmemString);
            } catch (std::ios::failure &e) {
                return XPUM_VGPU_SYSFS_ERROR;
            }
            info.lmemSize = std::stoul(lmemString);
        } else if (deviceInfo.deviceModel == XPUM_DEVICE_MODEL_PVC) {
            for (uint32_t tile = 0; tile < deviceInfo.numTiles; tile++) {
                std::string gtNum = std::to_string(tile);
                if (functionIndex == 0) {
                    lmemPath = devicePath + "/iov/pf/gt" + gtNum + "/available/lmem_free";
                } else {
                    lmemPath = devicePath + "/iov/vf" + std::to_string(functionIndex) + "/gt" + gtNum + "/lmem_quota";
                }
                try {
                    readFile(lmemPath, lmemString);
                } catch (std::ios::failure &e) {
                    return XPUM_VGPU_SYSFS_ERROR;
                }
                info.lmemSize += std::stoul(lmemString);
            }
        } else if (deviceInfo.deviceModel >= XPUM_DEVICE_MODEL_BMG) {
            for (uint32_t tile = 0; tile < deviceInfo.numTiles; tile++) {
                std::string gtNum = std::to_string(tile);
                if (functionIndex == 0) {
                    lmemPath = debugfsPath + "/gt" + gtNum + "/pf/" + "lmem_spare";
                } else {
                    lmemPath = debugfsPath + "/gt" + gtNum + "/vf" + std::to_string(functionIndex) + "/lmem_quota";
                }
                try {
                    readFile(lmemPath, lmemString);
                } catch (std::ios::failure &e) {
                    return XPUM_VGPU_SYSFS_ERROR;
                }
                info.lmemSize += std::stoul(lmemString);
            }
        } else {
            return XPUM_VGPU_UNSUPPORTED_DEVICE_MODEL;
        }

        if (deviceInfo.deviceModel >= XPUM_DEVICE_MODEL_BMG) {
            if (functionIndex == 0) {
                ueventPath = devicePath + "/device/uevent";
            } else {
                ueventPath = devicePath + "/device/virtfn" + std::to_string(functionIndex - 1) + "/uevent";
            }

        } else {
            if (functionIndex == 0) {
                ueventPath = devicePath + "/iov/pf/device/uevent";
            } else {
                ueventPath = devicePath + "/iov/vf" + std::to_string(functionIndex) + "/device/uevent";
            }
        }
        info.bdfAddress[0] = 0;
        std::ifstream ifs(ueventPath);
        std::string line;
        while (std::getline(ifs, line)) {
            if (line.length() >= XPUM_MAX_STR_LENGTH) {
                return XPUM_VGPU_SYSFS_ERROR;
            }
            sscanf(line.c_str(), "PCI_SLOT_NAME=%s", info.bdfAddress);
            if (info.bdfAddress[0] != 0) {
                XPUM_LOG_DEBUG("BDF Address: {}", info.bdfAddress);
                break;
            }
        }
        std::vector<std::shared_ptr<Device>> deviceList;
        Core::instance().getDeviceManager()->getDeviceList(deviceList);
        auto target = std::find_if(deviceList.begin(), deviceList.end(), [&info](std::shared_ptr<Device> d) {
            Property prop;
            d->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS, prop);
            return prop.getValue().compare(info.bdfAddress) == 0;
        });
        info.deviceId = target == deviceList.end() ? -1 : std::stoi((*target)->getId());
        result.push_back(info);
    }
    return XPUM_OK;
}

xpum_result_t VgpuManager::removeAllVf(xpum_device_id_t deviceId) {
    auto res = vgpuValidateDevice(deviceId);
    if (res != XPUM_OK) {
        return res;
    }
    std::unique_lock<std::mutex> lock(mutex);
    
    DeviceSriovInfo deviceInfo;
    if (!loadSriovData(deviceId, deviceInfo)) {
        return XPUM_VGPU_SYSFS_ERROR;
    }
    std::stringstream iovPath, numvfsPath;
    std::string numVfsString;

    /*
     *  Disable all VFs by setting sriov_numvfs to 0
     */
    numvfsPath << "/sys/bus/pci/devices/" << deviceInfo.bdfAddress << "/sriov_numvfs";
    try {
        readFile(numvfsPath.str(), numVfsString);
        writeFile(numvfsPath.str(), "0");
    } catch (std::ios::failure &e) {
        return XPUM_VGPU_REMOVE_VF_FAILED;
    }

    /*
     *  Then clear all resources allocated to all VFs
     */
    DIR *dir;
    dirent *ent;
    iovPath << "/sys/class/drm/" << deviceInfo.drmPath << "/iov/";
    AttrFromConfigFile zeroAttr = {};
    if ((dir = opendir(iovPath.str().c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (strstr(ent->d_name, "vf") == NULL) {
                continue;
            }
            try {
                if (deviceInfo.deviceModel == XPUM_DEVICE_MODEL_ATS_M_1 || deviceInfo.deviceModel == XPUM_DEVICE_MODEL_ATS_M_3 || deviceInfo.deviceModel == XPUM_DEVICE_MODEL_ATS_M_1G) {
                    writeVfAttrToSysfs(iovPath.str() + ent->d_name + "/gt", zeroAttr, 0);
                } else if (deviceInfo.deviceModel == XPUM_DEVICE_MODEL_PVC) {
                    for (uint32_t tile = 0; tile < deviceInfo.numTiles; tile++) {
                        writeVfAttrToSysfs(iovPath.str() + ent->d_name + "/gt" + std::to_string(tile), zeroAttr, 0);
                    }
                }
            } catch(std::ios::failure &e) {
                closedir(dir);
                return XPUM_VGPU_REMOVE_VF_FAILED;
            }
        }
        closedir(dir);
    } else if (deviceInfo.deviceModel >= XPUM_DEVICE_MODEL_BMG) {
        std::string debugfsPath = std::string("/sys/kernel/debug/dri/") + deviceInfo.bdfAddress;
        int numVfs = std::stoi(numVfsString);

        try {
            for (int functionIndex = 1; functionIndex <= numVfs; functionIndex++) {
                writeVfAttrToGt0Sysfs(debugfsPath + "/gt0/vf" + std::to_string(functionIndex), zeroAttr, 0);
                writeVfAttrToGt1Sysfs(debugfsPath + "/gt1/vf" + std::to_string(functionIndex), zeroAttr);
            }
        } catch(std::ios::failure &e) {
            return XPUM_VGPU_REMOVE_VF_FAILED;
        }
    } else {
        XPUM_LOG_ERROR("Failed to open directory {}", iovPath.str());
        return XPUM_VGPU_REMOVE_VF_FAILED;
    }
    return XPUM_OK;
}


/*
For VF metrics, calling SYSMAN API with dlopen and duplicating definition of
SYSMAN data structures should be a temperatory solution.

Once XPUM is able to break backward compatibility, e.g., 2.0 release,
these code should be refactored:
1. Call SYSMAN API directly instead of dlopen
2. Remove these duplicated data structure definition
*/

typedef struct _zes_vf_handle_t *zes_vf_handle_t;

typedef struct _zes_vf_exp2_capabilities_t
{
    zes_structure_type_t stype;                                             ///< [in] type of this structure
    void* pNext;                                                            ///< [in,out][optional] must be null or a pointer to an extension-specific
                                                                            ///< structure (i.e. contains stype and pNext).
    zes_pci_address_t address;                                              ///< [out] Virtual function BDF address
    uint64_t vfDeviceMemSize;                                               ///< [out] Virtual function memory size in bytes
    uint32_t vfID;                                                          ///< [out] Virtual Function ID

} zes_vf_exp2_capabilities_t;

typedef struct _zes_vf_util_mem_exp2_t
{
    zes_structure_type_t stype;                                             ///< [in] type of this structure
    const void* pNext;                                                      ///< [in][optional] must be null or a pointer to an extension-specific
                                                                            ///< structure (i.e. contains stype and pNext).
    zes_mem_loc_t vfMemLocation;                                            ///< [out] Location of this memory (system, device)
    uint64_t vfMemUtilized;                                                 ///< [out] Free memory size in bytes.

} zes_vf_util_mem_exp2_t;

typedef struct _zes_vf_util_engine_exp2_t
{
    zes_structure_type_t stype;                                             ///< [in] type of this structure
    const void* pNext;                                                      ///< [in][optional] must be null or a pointer to an extension-specific
                                                                            ///< structure (i.e. contains stype and pNext).
    zes_engine_group_t vfEngineType;                                        ///< [out] The engine group.
    uint64_t activeCounterValue;                                            ///< [out] Represents active counter.
    uint64_t samplingCounterValue;                                          ///< [out] Represents counter value when activeCounterValue was sampled.
                                                                            ///< Refer to the formulae above for calculating the utilization percent

} zes_vf_util_engine_exp2_t;

typedef ze_result_t (*pfnZesDeviceEnumEnabledVfExp_t)(
    zes_device_handle_t hDevice, uint32_t *pCount, zes_vf_handle_t *phVFhandle);

typedef ze_result_t (*pfnZesVFManagementGetVFCapabilitiesExp2_t)(
    zes_vf_handle_t hVFhandle, zes_vf_exp2_capabilities_t *pCapability);

typedef ze_result_t (*pfnZesVFManagementGetVFMemoryUtilizationExp2_t)(
    zes_vf_handle_t hVFhandle, uint32_t *pCount, 
    zes_vf_util_mem_exp2_t *pMemUtil);

typedef ze_result_t (*pfnZesVFManagementGetVFEngineUtilizationExp2_t)(
    zes_vf_handle_t hVFhandle, uint32_t *pCount, 
    zes_vf_util_engine_exp2_t *pEngineUtil);

typedef struct {
    pfnZesDeviceEnumEnabledVfExp_t pfnZesDeviceEnumEnabledVfExp;
    pfnZesVFManagementGetVFCapabilitiesExp2_t 
        pfnZesVFManagementGetVFCapabilitiesExp2;
    pfnZesVFManagementGetVFMemoryUtilizationExp2_t
        pfnZesVFManagementGetVFMemoryUtilizationExp2;
    pfnZesVFManagementGetVFEngineUtilizationExp2_t
       pfnZesVFManagementGetVFEngineUtilizationExp2 ;
} VfMgmtApi_t;

static xpum_realtime_metric_type_t engineToMetricType(zes_engine_group_t engine) {
    xpum_realtime_metric_type_t metricType = XPUM_STATS_MAX;
    switch (engine) {
        case ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE:
        case ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE:
        case ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE:
            metricType = XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION;
            break;
        case ZES_ENGINE_GROUP_COMPUTE_SINGLE:
            metricType = XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION;
            break;
        case ZES_ENGINE_GROUP_COPY_SINGLE:
            metricType = XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION;
            break;
        case ZES_ENGINE_GROUP_RENDER_SINGLE:
            metricType = XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION;
            break;
        default:
            break;
    }
    return metricType;
}

bool static findVfMgmtApi(VfMgmtApi_t &vfMgmtApi, void *dlHandle) {
    char* error;

    vfMgmtApi.pfnZesDeviceEnumEnabledVfExp =
        reinterpret_cast<pfnZesDeviceEnumEnabledVfExp_t>(dlsym(dlHandle,
        "zesDeviceEnumEnabledVFExp"));
    if ((error = dlerror()) != NULL) {
        XPUM_LOG_ERROR("dlsym error: {}", error);
        return false;
    }
    vfMgmtApi.pfnZesVFManagementGetVFCapabilitiesExp2 =
        reinterpret_cast<pfnZesVFManagementGetVFCapabilitiesExp2_t>(dlsym(
            dlHandle, "zesVFManagementGetVFCapabilitiesExp2"));
    if ((error = dlerror()) != NULL) {
        XPUM_LOG_ERROR("dlsym error: {}", error);
        return false;
    }
    vfMgmtApi.pfnZesVFManagementGetVFMemoryUtilizationExp2 =
        reinterpret_cast<pfnZesVFManagementGetVFMemoryUtilizationExp2_t>(dlsym(
            dlHandle, "zesVFManagementGetVFMemoryUtilizationExp2"));
    if ((error = dlerror()) != NULL) {
        XPUM_LOG_ERROR("dlsym error: {}", error);
        return false;
    }
    vfMgmtApi.pfnZesVFManagementGetVFEngineUtilizationExp2 =
        reinterpret_cast<pfnZesVFManagementGetVFEngineUtilizationExp2_t>(dlsym(
            dlHandle, "zesVFManagementGetVFEngineUtilizationExp2"));
    if ((error = dlerror()) != NULL) {
        XPUM_LOG_ERROR("dlsym error: {}", error);
        return false;
    }
    return true;
}

typedef struct {
    uint32_t vfid;
    zes_vf_handle_t vfh;
    zes_vf_exp2_capabilities_t cap;
    std::vector<zes_vf_util_engine_exp2_t> vues;
} vf_util_snap_t;

static bool getVfEngineUtilWithSnaps(std::vector<xpum_vf_metric_t> &metrics,
    std::vector<vf_util_snap_t> &snaps, VfMgmtApi_t &vfMgmtApi, 
    xpum_device_id_t deviceId, zes_device_handle_t &dh) {
    ze_result_t res = ZE_RESULT_SUCCESS;

    for (std::size_t i = 0; i < snaps.size(); i++) {
        uint32_t veuc = 0;
        XPUM_ZE_HANDLE_LOCK(dh, res =
            vfMgmtApi.pfnZesVFManagementGetVFEngineUtilizationExp2(
            snaps[i].vfh, &veuc, nullptr));
        if (res != ZE_RESULT_SUCCESS || veuc != snaps[i].vues.size()) {
            XPUM_LOG_DEBUG("pfnZesVFManagementGetVFEngineUtilizationExp2 returns {} veuc = {}", 
                res, veuc);
            return false;
        }
        std::vector<zes_vf_util_engine_exp2_t> vues(veuc);
        XPUM_ZE_HANDLE_LOCK(dh, res =
            vfMgmtApi.pfnZesVFManagementGetVFEngineUtilizationExp2(
            snaps[i].vfh, &veuc, vues.data()));
        if (res != ZE_RESULT_SUCCESS || veuc != snaps[i].vues.size()) {
            XPUM_LOG_DEBUG("pfnZesVFManagementGetVFEngineUtilizationExp2 returns {} veuc = {}", 
                res, veuc);
            return false;
        }
        if (vues.size() != snaps[i].vues.size()) {
            XPUM_LOG_DEBUG("VF engine number changed");
            return false;
        }
        std::vector<xpum_vf_metric_t> singleGroupMetrics;
        // zesVFManagementGetVFEngineUtilizationExp2 returns engine counters 
        // in same order though it is not documented at the time
        for (std::size_t j = 0; j < snaps[i].vues.size(); j++) {
            auto vue = vues[j];
            if (vue.vfEngineType != snaps[i].vues[j].vfEngineType) {
                XPUM_LOG_DEBUG("VF engine type order changed");
                return false;
            }
            if (vue.samplingCounterValue <= 
                snaps[i].vues[j].samplingCounterValue  ||
                vue.activeCounterValue < 
                snaps[i].vues[j].activeCounterValue) {
                XPUM_LOG_DEBUG("pfnZesVFManagementGetVFEngineUtilizationExp2 returns invalid values activeCounterValue {}, samplingCounterValue {} and activeCounterValue {}, samplingCounterValue {}",
                    snaps[i].vues[j].activeCounterValue,
                    snaps[i].vues[j].samplingCounterValue,
                    vue.activeCounterValue,
                    vue.samplingCounterValue);
                return false;
            }
            xpum_vf_metric_t vfm = {};
            vfm.metric.metricsType = engineToMetricType(vue.vfEngineType);
            vfm.metric.value = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE
                * 100 * (vue.activeCounterValue - 
                snaps[i].vues[j].activeCounterValue) / (
                vue.samplingCounterValue - 
                snaps[i].vues[j].samplingCounterValue);
            if (vfm.metric.value > 
                Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * 100) {
                vfm.metric.value = 
                    Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * 100;
            }
            singleGroupMetrics.push_back(vfm);
            XPUM_LOG_TRACE("vfEngineType = {}: activeCounterValue {}, samplingCounterValue {} and activeCounterValue {}, samplingCounterValue {}",
                vue.vfEngineType,
                snaps[i].vues[j].activeCounterValue,
                    snaps[i].vues[j].samplingCounterValue,
                    vue.activeCounterValue,
                    vue.samplingCounterValue);

        }

        // Aggregate (by max) utilization per metrics type
        uint64_t mediaUtil = UINT64_MAX;
        uint64_t copyUtil = UINT64_MAX;
        uint64_t renderUtil = UINT64_MAX;
        uint64_t computeUtil = UINT64_MAX;
        uint64_t allUtil = UINT64_MAX;
        for (auto m : singleGroupMetrics) {
            switch (m.metric.metricsType) {
                case XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION:
                    if (mediaUtil == UINT64_MAX) {
                        mediaUtil = m.metric.value;
                    } else {
                        mediaUtil = mediaUtil > m.metric.value ? 
                            mediaUtil : m.metric.value;
                    }
                    break;
                case XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION:
                    if (renderUtil == UINT64_MAX) {
                        renderUtil = m.metric.value;
                    } else {
                        renderUtil = renderUtil > m.metric.value ? 
                            renderUtil : m.metric.value;
                    }
                    break;
                case XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION:
                    if (computeUtil == UINT64_MAX) {
                        computeUtil = m.metric.value;
                    } else {
                        computeUtil = computeUtil > m.metric.value ? 
                            computeUtil : m.metric.value;
                    }
                    break;
                case XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION:
                    if (copyUtil == UINT64_MAX) {
                        copyUtil = m.metric.value;
                    } else {
                        copyUtil = copyUtil > m.metric.value ?
                            copyUtil : m.metric.value;
                    }
                    break;
                default:
                   XPUM_LOG_DEBUG("unknown VF metric type"); 
                   return false;
            }
        }
        if (mediaUtil != UINT64_MAX) {
            xpum_vf_metric_t vfm = {};
            vfm.deviceId = deviceId;
            vfm.vfIndex = snaps[i].vfid;
            snprintf(vfm.bdfAddress, XPUM_MAX_STR_LENGTH, "%04x:%02x:%02x.%x", 
                snaps[i].cap.address.domain, snaps[i].cap.address.bus, 
                snaps[i].cap.address.device, snaps[i].cap.address.function);
            vfm.metric.scale = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE;
            vfm.metric.value = mediaUtil;
            vfm.metric.metricsType = XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION;
            metrics.push_back(vfm);
            XPUM_LOG_TRACE("media overall {}", mediaUtil);
            if (allUtil == UINT64_MAX) {
                allUtil = mediaUtil;
            } else {
                allUtil = allUtil > mediaUtil ? allUtil : mediaUtil;
            }
        }
        if (renderUtil != UINT64_MAX) {
            xpum_vf_metric_t vfm = {};
            vfm.deviceId = deviceId;
            vfm.vfIndex = snaps[i].vfid;
            snprintf(vfm.bdfAddress, XPUM_MAX_STR_LENGTH, "%04x:%02x:%02x.%x", 
                snaps[i].cap.address.domain, snaps[i].cap.address.bus, 
                snaps[i].cap.address.device, snaps[i].cap.address.function);
            vfm.metric.scale = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE;
            vfm.metric.value = renderUtil;
            vfm.metric.metricsType = XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION;
            metrics.push_back(vfm);
            XPUM_LOG_TRACE("render overall {}", renderUtil);
            if (allUtil == UINT64_MAX) {
                allUtil = renderUtil;
            } else {
                allUtil = allUtil > renderUtil ? allUtil : renderUtil;
            }
        }
        if (computeUtil != UINT64_MAX) {
            xpum_vf_metric_t vfm = {};
            vfm.deviceId = deviceId;
            vfm.vfIndex = snaps[i].vfid;
            snprintf(vfm.bdfAddress, XPUM_MAX_STR_LENGTH, "%04x:%02x:%02x.%x", 
                snaps[i].cap.address.domain, snaps[i].cap.address.bus, 
                snaps[i].cap.address.device, snaps[i].cap.address.function);
            vfm.metric.scale = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE;
            vfm.metric.value = computeUtil;
            vfm.metric.metricsType = XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION;
            metrics.push_back(vfm);
            XPUM_LOG_TRACE("compute overall {}", computeUtil);
            if (allUtil == UINT64_MAX) {
                allUtil = computeUtil;
            } else {
                allUtil = allUtil > computeUtil ? allUtil : computeUtil;
            }
        }
        if (copyUtil != UINT64_MAX) {
            xpum_vf_metric_t vfm = {};
            vfm.deviceId = deviceId;
            vfm.vfIndex = snaps[i].vfid;
            snprintf(vfm.bdfAddress, XPUM_MAX_STR_LENGTH, "%04x:%02x:%02x.%x", 
                snaps[i].cap.address.domain, snaps[i].cap.address.bus, 
                snaps[i].cap.address.device, snaps[i].cap.address.function);
            vfm.metric.scale = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE;
            vfm.metric.value = copyUtil;
            vfm.metric.metricsType = XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION;
            metrics.push_back(vfm);
            XPUM_LOG_TRACE("copy overall {}", copyUtil);
            if (allUtil == UINT64_MAX) {
                allUtil = copyUtil;
            } else {
                allUtil = allUtil > copyUtil ? allUtil : copyUtil;
            }
        }
        if (allUtil != UINT64_MAX) {
            xpum_vf_metric_t vfm = {};
            vfm.deviceId = deviceId;
            vfm.vfIndex = snaps[i].vfid;
            snprintf(vfm.bdfAddress, XPUM_MAX_STR_LENGTH, "%04x:%02x:%02x.%x", 
                snaps[i].cap.address.domain, snaps[i].cap.address.bus, 
                snaps[i].cap.address.device, snaps[i].cap.address.function);
            vfm.metric.scale = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE;
            vfm.metric.value = allUtil;
            vfm.metric.metricsType = XPUM_STATS_GPU_UTILIZATION;
            metrics.push_back(vfm);
            XPUM_LOG_TRACE("GPU overall {}", allUtil);
        }
    }
    return true;
}

static bool getVfId(uint32_t &vfIndex, const char *bdf, uint32_t szBdf,
    xpum_device_id_t deviceId);
static bool getVfBdf(char *bdf, uint32_t szBdf, uint32_t vfIndex,
    xpum_device_id_t deviceId);
#define BDF_SIZE 12

static xpum_result_t getVfBdfInfo(vf_util_snap_t& snap, uint32_t vfIndex,
    xpum_device_id_t deviceId) {
    zes_vf_exp2_capabilities_t cap = {};
    char bdf[BDF_SIZE + 1] = {};

    if (getVfBdf(bdf, BDF_SIZE + 1, vfIndex - 1, deviceId) == false) {
        XPUM_LOG_DEBUG("VF bdf cannot be found for vf index {}", vfIndex);
        return XPUM_GENERIC_ERROR;
    }
    sscanf(bdf, "%04x:%02x:%02x.%x", &cap.address.domain, &cap.address.bus,
        &cap.address.device, &cap.address.function);

    cap.vfID = vfIndex;
    snap.vfid = vfIndex;
    snap.cap = cap;
    return XPUM_OK;
}

static xpum_result_t getVfEngineUtilization(VfMgmtApi_t& vfMgmtApi,
    zes_device_handle_t dh, zes_vf_handle_t vfh, vf_util_snap_t& snap) {
    ze_result_t res;
    uint32_t veuc = 0;

    XPUM_ZE_HANDLE_LOCK(dh, res =
        vfMgmtApi.pfnZesVFManagementGetVFEngineUtilizationExp2(
        vfh, &veuc, nullptr));
    if (res != ZE_RESULT_SUCCESS || veuc == 0) {
        XPUM_LOG_DEBUG("pfnZesVFManagementGetVFEngineUtilizationExp2 returns {} veuc = {}",
            res, veuc);
        return XPUM_GENERIC_ERROR;
    }

    snap.vues = std::vector<zes_vf_util_engine_exp2_t>(veuc);
    XPUM_ZE_HANDLE_LOCK(dh, res =
        vfMgmtApi.pfnZesVFManagementGetVFEngineUtilizationExp2(
        vfh, &veuc, snap.vues.data()));
    if (res != ZE_RESULT_SUCCESS || veuc == 0) {
        XPUM_LOG_DEBUG("pfnZesVFManagementGetVFEngineUtilizationExp2 returns {} veuc = {}",
            res, veuc);
        return XPUM_GENERIC_ERROR;
    }
    return XPUM_OK;
}

static xpum_result_t getVfMemoryUtilization(VfMgmtApi_t& vfMgmtApi,
    xpum_device_id_t deviceId, zes_device_handle_t dh, zes_vf_handle_t vfh,
    std::vector<xpum_vf_metric_t>& metrics, vf_util_snap_t& snap) {
    ze_result_t res;

    zes_vf_exp2_capabilities_t cap = {};
    XPUM_ZE_HANDLE_LOCK(dh, res =
        vfMgmtApi.pfnZesVFManagementGetVFCapabilitiesExp2(vfh, &cap));
    if (res != ZE_RESULT_SUCCESS || cap.vfDeviceMemSize == 0) {
        XPUM_LOG_DEBUG("pfnZesVFManagementGetVFCapabilitiesExp2 returns {}", res);
        return XPUM_GENERIC_ERROR;
    }

    uint32_t mc = 0;
    XPUM_ZE_HANDLE_LOCK(dh, res =
        vfMgmtApi.pfnZesVFManagementGetVFMemoryUtilizationExp2(vfh, &mc,
        nullptr));
    if (res != ZE_RESULT_SUCCESS) {
        XPUM_LOG_DEBUG("pfnZesVFManagementGetVFMemoryUtilizationExp2 returns {}", res);
        return XPUM_GENERIC_ERROR;
    }
    std::vector<zes_vf_util_mem_exp2_t> vums(mc);
    XPUM_ZE_HANDLE_LOCK(dh, res =
        vfMgmtApi.pfnZesVFManagementGetVFMemoryUtilizationExp2(vfh, &mc, vums.data()));
    if (res != ZE_RESULT_SUCCESS) {
        XPUM_LOG_DEBUG("pfnZesVFManagementGetVFMemoryUtilizationExp2 returns {}", res);
        return XPUM_GENERIC_ERROR;
    }
    // vmu: VF Memory Utilized
    uint64_t vmu = UINT64_MAX;
    for (auto mu : vums) {
         if (mu.vfMemLocation == ZES_MEM_LOC_DEVICE) {
             vmu = mu.vfMemUtilized;
         }
    }
    if (vmu == UINT64_MAX) {
        XPUM_LOG_DEBUG("zesVFManagementGetVFMemoryUtilizationExp2 returns no ZES_MEM_LOC_DEVICE");
        return XPUM_GENERIC_ERROR;
    }
    xpum_vf_metric_t vfm = {};
    vfm.deviceId = deviceId;
    snprintf(vfm.bdfAddress, XPUM_MAX_STR_LENGTH, "%04x:%02x:%02x.%x",
             cap.address.domain, cap.address.bus,
             cap.address.device, cap.address.function);
    if (getVfId(vfm.vfIndex, vfm.bdfAddress, XPUM_MAX_STR_LENGTH,
        deviceId) == false) {
        XPUM_LOG_DEBUG("VF index cannot be found for bdf {}", vfm.bdfAddress);
    }

    snap.vfid = vfm.vfIndex;
    snap.cap = cap;
    vfm.metric.metricsType = XPUM_STATS_MEMORY_UTILIZATION;
    vfm.metric.scale = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE;
    vfm.metric.value = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * 100 *
        vmu / cap.vfDeviceMemSize;
    metrics.push_back(vfm);
    return XPUM_OK;
}

xpum_result_t VgpuManager::getVfMetrics(xpum_device_id_t deviceId,
    std::vector<xpum_vf_metric_t> &metrics, uint32_t *count) {
    xpum_result_t ret = XPUM_GENERIC_ERROR;
    std::vector<vf_util_snap_t> snaps;

    auto device = Core::instance().getDeviceManager()->getDevice(
        std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    auto deviceModel = device->getDeviceModel();
    void *handle = dlopen("libze_loader.so.1", RTLD_NOW);
    if (handle == nullptr) {
        return XPUM_LEVEL_ZERO_INITIALIZATION_ERROR;
    }
    VfMgmtApi_t vfMgmtApi;
    if (findVfMgmtApi(vfMgmtApi, handle) == false) {
        dlclose(handle);
        XPUM_LOG_DEBUG("getVfMetrics: findVfMgmtApi returns false");
        return XPUM_API_UNSUPPORTED;
    }

    ze_result_t res = ZE_RESULT_SUCCESS;
    auto dh = device->getDeviceHandle();
    uint32_t vfCount = 0;
    XPUM_ZE_HANDLE_LOCK(dh, res = vfMgmtApi.pfnZesDeviceEnumEnabledVfExp(
                                    dh, &vfCount, nullptr));
    if (res != ZE_RESULT_SUCCESS) {
        XPUM_LOG_DEBUG("pfnZesDeviceEnumEnabledVfExp returns {} vfCount = {}", 
            res, vfCount);
        dlclose(handle);
        return XPUM_GENERIC_ERROR;
    }
    if (vfCount == 0) {
        //check count only
        if (count != nullptr) {
            *count = 0;
            ret = XPUM_OK;
        } else {
            ret = XPUM_GENERIC_ERROR;
            XPUM_LOG_DEBUG("pfnZesDeviceEnumEnabledVfExp vfCount = {}", 
                vfCount);
        }
        dlclose(handle);
        return ret;
    }
    std::vector<zes_vf_handle_t> vfs(vfCount);
    XPUM_ZE_HANDLE_LOCK(dh, res = vfMgmtApi.pfnZesDeviceEnumEnabledVfExp(
                                    dh, &vfCount, vfs.data()));
    if (res != ZE_RESULT_SUCCESS || vfCount == 0) {
        XPUM_LOG_DEBUG("pfnZesDeviceEnumEnabledVfExp returns {} vfCount = {}", 
            res, vfCount);
        goto RTN;
    }

    //check count only
    if (count != nullptr) {
        uint32_t engineUtilCount = 0;
        for (auto vfh : vfs) {
            // veuc: VF Engine Util Count
            uint32_t veuc = 0;
            XPUM_ZE_HANDLE_LOCK(dh, res =
                vfMgmtApi.pfnZesVFManagementGetVFEngineUtilizationExp2(
                vfh, &veuc, nullptr));
            if (res != ZE_RESULT_SUCCESS) {
                XPUM_LOG_DEBUG("pfnZesVFManagementGetVFEngineUtilizationExp2 returns {} veuc = {}", 
                    res, veuc);
                goto RTN;
            }
            std::vector<zes_vf_util_engine_exp2_t> vues(veuc);    
            XPUM_ZE_HANDLE_LOCK(dh, res =
                vfMgmtApi.pfnZesVFManagementGetVFEngineUtilizationExp2(
                vfh, &veuc, vues.data()));
            if (res != ZE_RESULT_SUCCESS) {
                XPUM_LOG_DEBUG("pfnZesVFManagementGetVFEngineUtilizationExp2 returns {} veuc = {}", 
                    res, veuc);
                goto RTN;
            }


            /* 
            According to the doc, the 6 single engine groups below are supported
            by zesVFManagementGetVFEngineUtilizationExp2
            https://docs.intel.com/documents/GfxSWAT/Common/sysman/SysmanAPI.html#engine

            ZES_ENGINE_GROUP_COMPUTE_SINGLE = 4
            ZES_ENGINE_GROUP_RENDER_SINGLE = 5    
            ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE = 6
            ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE = 7
            ZES_ENGINE_GROUP_COPY_SINGLE = 8
            ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE = 9

            The single engine groups would be aggregated (max) to 4 overall 
            engine groups: media, compute, copy, and render. And these 4 
            would be aggreated to overall GPU util, then the expected engine 
            count is 4+1=5 for each VF
            */

            /*
            The varialbe would be set to 1 if the corresponding single engine
            gropup is found. If multiple engine instance for a single engine
            group exists, it is still 1 because the data would be aggregaed
            */
            uint32_t mediaEngine = 0;
            uint32_t computeEngine = 0;
            uint32_t copyEngine = 0;
            uint32_t renderEngine = 0;

            for (auto vue : vues) {
                if (vue.vfEngineType == ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE ||
                    vue.vfEngineType == ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE ||
                    vue.vfEngineType == 
                    ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE) {
                    mediaEngine = 1;
                    continue;
                }
                if (vue.vfEngineType == ZES_ENGINE_GROUP_COMPUTE_SINGLE) {
                    computeEngine = 1;
                    continue;
                }
                if (vue.vfEngineType == ZES_ENGINE_GROUP_RENDER_SINGLE) {
                    renderEngine = 1;
                    continue;
                }
                if (vue.vfEngineType == ZES_ENGINE_GROUP_COPY_SINGLE) {
                    copyEngine = 1;
                    continue;
                }
            }
            uint32_t allEngine = mediaEngine + computeEngine + copyEngine +
                renderEngine;
            if (allEngine > 0) {
                // The four aggregated engine group will be aggregated to 
                // overall GPU util
                allEngine++;
            }
            engineUtilCount += allEngine;
        }
        // Add vfCount because there would be memory util for each VF
        *count = engineUtilCount + vfCount;
        ret = XPUM_OK;
        goto RTN;
    }

    uint32_t vfIndex;
    vfIndex = 1;
    for (auto vfh : vfs) {
        vf_util_snap_t snap;
        snap.vfh = vfh;

        if (deviceModel < XPUM_DEVICE_MODEL_BMG) {
            ret = getVfMemoryUtilization(vfMgmtApi, deviceId, dh, vfh, metrics, snap);
            if (ret != XPUM_OK) {
                XPUM_LOG_DEBUG("getVfMemoryUtilization returns {}", ret);
                goto RTN;
            }
        } else {
            ret = getVfBdfInfo(snap, vfIndex, deviceId);
            if (ret != XPUM_OK) {
                XPUM_LOG_DEBUG("getVfBdfInfo returns {}", ret);
                goto RTN;
            }
        }

        ret = getVfEngineUtilization(vfMgmtApi, dh, vfh, snap);
        if (ret != XPUM_OK) {
            XPUM_LOG_DEBUG("getVfMemoryUtilization returns {}", ret);
            goto RTN;
        }
        snaps.push_back(snap);
        vfIndex++;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(
        Configuration::VF_METRICS_INTERVAL));
    if (getVfEngineUtilWithSnaps(metrics, snaps, vfMgmtApi, deviceId, dh) 
        == true) {
        ret = XPUM_OK;
    }

RTN:
    dlclose(handle);
    return ret;
}

static bool getVfBdf(char *bdf, uint32_t szBdf, uint32_t vfIndex,
        xpum_device_id_t deviceId) {
//BDF Format in uevent is cccc:cc:cc.c
    if (bdf == nullptr || szBdf < BDF_SIZE + 1) {
        return false;
    }
    auto device = Core::instance().getDeviceManager()->getDevice(
            std::to_string(deviceId));
    if (device == nullptr) {
        return false;
    }
    auto deviceModel = device->getDeviceModel();
    Property prop = {};
    if (device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_DRM_DEVICE, prop)
            == false) {
        return false;
    }
    std::string str = prop.getValue();
    std::string::size_type n = str.rfind('/');
    if (n == std::string::npos || n == str.length() - 1) {
        return false;
    }
    str = str.substr(n);
    if (deviceModel >= XPUM_DEVICE_MODEL_BMG) {
        str = "/sys/class/drm/" + str + "/device/virtfn" + std::to_string(vfIndex) +
            "/uevent";
    } else {
        str = "/sys/class/drm/" + str + "/iov/vf" + std::to_string(vfIndex) +
            "/device/uevent";
    }

    std::ifstream ifs(str);
    if (ifs.is_open() == false) {
        XPUM_LOG_DEBUG("cannot open uevent file = {}", str);
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(ifs)), 
            (std::istreambuf_iterator<char>()));
    std::string key = "PCI_SLOT_NAME=";
    n = content.find(key);
    if (n == std::string::npos || 
            n + key.length() + BDF_SIZE > content.length() - 1) {
        XPUM_LOG_DEBUG("uevent offset error");
        return false;
    }
    str = content.substr(n + key.length(), BDF_SIZE);
    str.copy(bdf, BDF_SIZE);
    bdf[BDF_SIZE] = '\0';
    return true;
}

static bool getVfId(uint32_t &vfIndex, const char *bdf, uint32_t szBdf,
    xpum_device_id_t deviceId) {
//BDF Format in uevent is cccc:cc:cc.c
    if (bdf == nullptr || szBdf < BDF_SIZE + 1) {
        return false;
    }
    bool ret = false;
    auto device = Core::instance().getDeviceManager()->getDevice(
            std::to_string(deviceId));
    if (device == nullptr) {
        return false;
    }
    Property prop = {};
    if (device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_DRM_DEVICE, prop)
            == false) {
        return false;
    }
    std::string str = prop.getValue();
    std::string::size_type n = str.rfind('/');
    if (n == std::string::npos || n == str.length() - 1) {
        return false;
    }
    str = str.substr(n);
    str = "/sys/class/drm/" + str + "/iov/";
    DIR *pdir = NULL;
    struct dirent *pdirent = NULL;
    pdir = opendir(str.c_str());
    if (pdir != NULL) {
        while ((pdirent = readdir(pdir)) != NULL) {
            if (pdirent->d_name[0] == '.') {
                continue;
            }
            if (strncmp(pdirent->d_name, "vf", 2) != 0) {
                continue;
            }
            std::string ueventFN = str + pdirent->d_name + "/device/uevent";
            std::ifstream ifs(ueventFN);
            if (ifs.is_open() == false) {
                XPUM_LOG_DEBUG("cannot open uevent file = {}", ueventFN);
                continue;
            }
            std::string content((std::istreambuf_iterator<char>(ifs)), 
                (std::istreambuf_iterator<char>()));
            if (content.find(bdf) != std::string::npos) {
                sscanf(pdirent->d_name, "vf%d", &vfIndex);
                ret = true;
                break;
            }
        }
        closedir(pdir);
    }
    return ret;
}

static uint64_t getFreeLmemSize(const std::string& path) {
    std::ifstream ifs(path + "/vram0_mm");
    std::string line;
    uint64_t freeSize;

    while (std::getline(ifs, line)) {
        if (line.length() >= XPUM_MAX_STR_LENGTH) {
            return 0;
        }
        if (strstr(line.c_str(), "visible_avail") == NULL) {
            continue;
        }
        sscanf(line.c_str(), "%*[^0-9]%lu", &freeSize);
        return freeSize * 1024 * 1024; //MiB to bytes
    }
    return 0;
}

bool VgpuManager::loadSriovData(xpum_device_id_t deviceId, DeviceSriovInfo &data) {
    auto device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    Property prop;

    data.deviceModel = device->getDeviceModel();

    device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_DRM_DEVICE, prop);
    char drm[XPUM_MAX_STR_LENGTH];
    if (prop.getValue().length() >= XPUM_MAX_STR_LENGTH) {
        return false;
    }
    sscanf(prop.getValue().c_str(), "/dev/dri/%s", drm);
    data.drmPath = std::string(drm);
    
    device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS, prop);
    data.bdfAddress = prop.getValue();

    device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_TILES, prop);
    data.numTiles = prop.getValueInt();
    
    bool available;
    bool configurable;
    xpum_ecc_state_t current, pending;
    xpum_ecc_action_t action;
    xpumGetEccState(stoi(device->getId()), &available, &configurable, &current, &pending, &action);
    data.eccState = current;
    
    std::string lmem, ggtt, doorbell, context;
    if (data.deviceModel == XPUM_DEVICE_MODEL_ATS_M_1 || data.deviceModel == XPUM_DEVICE_MODEL_ATS_M_3 || data.deviceModel == XPUM_DEVICE_MODEL_ATS_M_1G) {
        std::string pfIovPath = std::string("/sys/class/drm/") + drm + "/iov/pf/gt/available/";
        try {
            readFile(pfIovPath + "lmem_free", lmem);
            readFile(pfIovPath + "ggtt_free", ggtt);
            readFile(pfIovPath + "doorbells_free", doorbell);
            readFile(pfIovPath + "contexts_free", context);
        } catch (std::ios::failure &e) {
            return false;
        }
        data.lmemSizeFree = std::stoul(lmem);
        data.ggttSizeFree = std::stoul(ggtt);
        data.contextFree = std::stoi(context);
        data.doorbellFree = std::stoi(doorbell);
    } else if (data.deviceModel == XPUM_DEVICE_MODEL_PVC) {
        for (uint32_t tile = 0; tile < data.numTiles; tile++) {
            std::string pfIovPath = std::string("/sys/class/drm/") + drm + "/iov/pf/gt" + std::to_string(tile) + "/available/";
            try {
                readFile(pfIovPath + "lmem_free", lmem);
                readFile(pfIovPath + "ggtt_free", ggtt);
                readFile(pfIovPath + "doorbells_free", doorbell);
                readFile(pfIovPath + "contexts_free", context);
            } catch (std::ios::failure &e) {
                return false;
            }
            data.lmemSizeFree += std::stoul(lmem);
            data.ggttSizeFree += std::stoul(ggtt);
            data.contextFree += std::stoi(context);
            data.doorbellFree += std::stoi(doorbell);
        }
    } else if (data.deviceModel >= XPUM_DEVICE_MODEL_BMG) {
        std::string debugfsPath = std::string("/sys/kernel/debug/dri/") + data.bdfAddress;
        data.lmemSizeFree = getFreeLmemSize(debugfsPath);
    } else {
        return false;
    }
    return true;
}

static void readFile(const std::string& path, std::string& content) {
    std::ifstream ifs;
    std::stringstream ss;
    ifs.exceptions(std::ios::failbit | std::ios::badbit);
    try {
        ifs.open(path);
        ss << ifs.rdbuf();
    } catch (std::ios::failure &e) {
        XPUM_LOG_ERROR("read: {} {} failed", path, content);
        ifs.close();
        throw e;
    } 
    content.assign(ss.str());
    ifs.close();
    XPUM_LOG_DEBUG("read: {} {}", path, ss.str());
}

void VgpuManager::writeFile(const std::string path, const std::string& content) {
    std::ofstream ofs;
    ofs.exceptions(std::ios::failbit | std::ios::badbit);
    try {
        ofs.open(path, std::ios::out | std::ios::trunc);
        ofs << content;
        ofs.flush();
    } catch (std::ios::failure &e) {
        XPUM_LOG_ERROR("write: {} {} failed", path, content);
        ofs.close();
        throw e;
    }
    ofs.close();
    XPUM_LOG_DEBUG("write: {} {}", path, content);
}

static bool caseInsensitiveMatch(std::string s1, std::string s2) {
   std::transform(s1.begin(), s1.end(), s1.begin(), ::tolower);
   std::transform(s2.begin(), s2.end(), s2.begin(), ::tolower);
   return s1.compare(s2) == 0;
}

/**
 * The vGPUScheduler is based on vGPUProfilesV2_V2.5
 * For Intel Data Center Flex GPUs, vGPUScheduler has three options to meet various application scenarios:
 * [1] Flexible_30fps_GPUTimeSlicing
 * - ScheduleIfIdle = False
 * - PFExecutionQuantum = 20
 * - PFPreemptionTimeout = 20000
 * - VFExecutionQuantum = max( 32 // VFCount, 1)
 * - VFPreemptionTimeout = 128000 if (VFCount == 1) else max( 64000 // VFCount, 16000)
 * [2] Fixed_30fps_GPUTimeSlicing
 * - ScheduleIfIdle = true
 * - PFExecutionQuantum = 20
 * - PFPreemptionTimeout = 20000
 * - VFExecutionQuantum = max( 32 // VFCount, 1)
 * - VFPreemptionTimeout = 128000 if (VFCount == 1) else max( 64000 // VFCount, 16000)
 * [3] Flexible_BurstableQoS_GPUTimeSlicing
 * - ScheduleIfIdle = False
 * - PFExecutionQuantum = 20
 * - PFPreemptionTimeout = 20000
 * - VFExecutionQuantum = min((2000 // max(VFCount-1,1)*0.5, 50))
 * - VFPreemptionTimeout = (2000 // max(VFCount-1,1) - min((2000 // max(VFCount-1,1))*0.5, 50))*1000
 * The vGPUScheduler is Flexible_30fps_GPUTimeSlicing by default if not set or set incorrectly
 * For Intel Data Center Max GPUs, vGPUScheduler only has one effective option and other settings will not take effect:
 * [1] Flexible_BurstableQoS_GPUTimeSlicing
 * - ScheduleIfIdle = False
 * - PFExecutionQuantum = 64
 * - PFPreemptionTimeout = 128000
 * - VFExecutionQuantum = min((2000 // max(VFCount-1,1)*0.5, 50))
 * - VFPreemptionTimeout = (2000 // max(VFCount-1,1) - min((2000 // max(VFCount-1,1))*0.5, 50))*1000
 * 
 */

static void updateVgpuSchedulerConfigParamerters(std::string devicePciId, int numVfs, std::string scheduler, std::map<uint32_t, AttrFromConfigFile>& data) {
    if (devicePciId == "56c0" || devicePciId == "56c1" || devicePciId == "56c2" ||
        devicePciId == "e211" || devicePciId == "e212" || devicePciId == "e223") {
        data[numVfs].pfExec = 20;
        data[numVfs].pfPreempt = 20000;
        if (caseInsensitiveMatch(scheduler, "Flexible_BurstableQoS_GPUTimeSlicing")) {
            data[numVfs].schedIfIdle = false;
            data[numVfs].vfExec = std::min(int(2000 / std::max(numVfs - 1, 1) * 0.5), 50);
            data[numVfs].vfPreempt = (2000 / std::max(numVfs - 1, 1) - std::min(int((2000 / std::max(numVfs - 1 , 1)) * 0.5), 50)) * 1000;
        } else if (caseInsensitiveMatch(scheduler, "Fixed_30fps_GPUTimeSlicing")) {
            data[numVfs].schedIfIdle = true;
            data[numVfs].vfExec = std::max(32 / numVfs, 1);
            data[numVfs].vfPreempt = (numVfs == 1 ? 128000 : std::max(64000 / numVfs, 16000));
        } else { //Flexible_30fps_GPUTimeSlicing
            data[numVfs].schedIfIdle = false;
            data[numVfs].vfExec = std::max(32 / numVfs, 1);
            data[numVfs].vfPreempt = (numVfs == 1 ? 128000 : std::max(64000 / numVfs, 16000));
        }
    } else {
        data[numVfs].pfExec = 64;
        data[numVfs].pfPreempt = 128000;
        data[numVfs].schedIfIdle = false;
        data[numVfs].vfExec = std::min(int(2000 / std::max(numVfs - 1, 1) * 0.5), 50);
        data[numVfs].vfPreempt = (2000 / std::max(numVfs - 1, 1) - std::min(int((2000 / std::max(numVfs - 1 , 1)) * 0.5), 50)) * 1000;
    }
    XPUM_LOG_DEBUG("vgpu scheduler: {}, numVfs: {}, vfExec: {}, vfPreempt: {}, pfExec: {}, pfPreempt: {}, schedIfIdle: {}", scheduler, numVfs,
    data[numVfs].vfExec, data[numVfs].vfPreempt, data[numVfs].pfExec, data[numVfs].pfPreempt, data[numVfs].schedIfIdle);
}

static AttrFromConfigFile combineAttrConfig(std::map<uint32_t, AttrFromConfigFile> data, uint32_t numVfs, std::string devicePciId, std::string scheduler) {
    if (data.size() == 1 && data.find(numVfs) != data.end())
        return data.begin()->second;
    if (data.find(numVfs) == data.end()) {
        data[numVfs].driversAutoprobe = data[XPUM_MAX_VF_NUM].driversAutoprobe;
        data[numVfs].schedIfIdle = data[XPUM_MAX_VF_NUM].schedIfIdle;
    }
    if (data[numVfs].vfLmem == 0 && data[XPUM_MAX_VF_NUM].vfLmem != 0)
        data[numVfs].vfLmem = data[XPUM_MAX_VF_NUM].vfLmem / static_cast<uint64_t>(numVfs);
    if (data[numVfs].vfLmemEcc == 0 && data[XPUM_MAX_VF_NUM].vfLmemEcc != 0)
        data[numVfs].vfLmemEcc = data[XPUM_MAX_VF_NUM].vfLmemEcc / static_cast<uint64_t>(numVfs);
    if (data[numVfs].vfContexts == 0)
        data[numVfs].vfContexts = data[XPUM_MAX_VF_NUM].vfContexts;
    if (data[numVfs].vfDoorbells == 0 && data[XPUM_MAX_VF_NUM].vfDoorbells != 0)
        data[numVfs].vfDoorbells = data[XPUM_MAX_VF_NUM].vfDoorbells / static_cast<uint64_t>(numVfs);
    if (data[numVfs].vfGgtt == 0 && data[XPUM_MAX_VF_NUM].vfGgtt != 0)
        data[numVfs].vfGgtt = data[XPUM_MAX_VF_NUM].vfGgtt / static_cast<uint64_t>(numVfs);
    updateVgpuSchedulerConfigParamerters(devicePciId, numVfs, scheduler, data);
    return data[numVfs];
}

bool VgpuManager::readConfigFromFile(xpum_device_id_t deviceId, uint32_t numVfs, AttrFromConfigFile &attrs) {
    std::string fileName = std::string(XPUM_CONFIG_DIR) + std::string("vgpu.conf");
    if (!is_path_exist(fileName)) {
        char exe_path[XPUM_MAX_PATH_LEN];
        ssize_t len = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path));
        if (len < 0 || len >= XPUM_MAX_PATH_LEN) {
            throw BaseException("readlink returns error");
        }
        exe_path[len] = '\0';
        std::string current_file = exe_path;
        fileName = current_file.substr(0, current_file.find_last_of('/')) + "/../lib/" + Configuration::getXPUMMode() + "/config/" + std::string("vgpu.conf");
        if (!is_path_exist(fileName))
            fileName = current_file.substr(0, current_file.find_last_of('/')) + "/../lib64/" + Configuration::getXPUMMode() + "/config/" + std::string("vgpu.conf");
    }
    XPUM_LOG_DEBUG("read vgpu.conf: {}", fileName);
    Property prop;
    Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId))->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_DEVICE_ID, prop);
    std::ostringstream oss;
    oss << std::setw(4) << std::setfill('0') << prop.getValue().substr(2);
    std::string devicePciId = oss.str();

    std::ifstream ifs(fileName);
    if (ifs.fail()) {
        return false;
    }
    std::string line;
    std::map<uint32_t, AttrFromConfigFile> data;
    uint32_t currentNameId = 0;
    std::string default_vgpu_scheduler;
    while (std::getline(ifs, line)) {
        line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
        if (line[0] == '#' || line.empty()) {
            continue;
        }
        std::istringstream lineStream(line);
        std::string key, value;
        if (std::getline(lineStream, key, '=') && std::getline(lineStream, value)) {
            if (strcmp(key.c_str(), "NAME") == 0) {
                char s[16];
                uint32_t i;
                auto nameList = Utility::split(value, ',');
                for (auto nameItem: nameList) {
                    if (nameItem.find("DEF") != std::string::npos) {
                        sscanf(nameItem.c_str(), "%4sDEF", s);
                        if (strcmp(s, devicePciId.c_str()) == 0) {
                            currentNameId = XPUM_MAX_VF_NUM;
                            break;
                        } else {
                            currentNameId = 0;
                        }
                    } else {
                        sscanf(nameItem.c_str(), "%4sN%d", s, &i);
                        if (strcmp(s, devicePciId.c_str()) == 0 && i == numVfs) {
                            currentNameId = i;
                            break;
                        } else {
                            currentNameId = 0;
                        }
                    }
                }
            } else if (strcmp(key.c_str(), "VF_LMEM") == 0 && currentNameId) {
                data[currentNameId].vfLmem = std::stoul(value);
            } else if (strcmp(key.c_str(), "VF_LMEM_ECC") == 0 && currentNameId) {
                data[currentNameId].vfLmemEcc = std::stoul(value);
            } else if (strcmp(key.c_str(), "VF_CONTEXTS") == 0 && currentNameId) {
                data[currentNameId].vfContexts = std::stoi(value);
            } else if (strcmp(key.c_str(), "VF_DOORBELLS") == 0 && currentNameId) {
                data[currentNameId].vfDoorbells = std::stoi(value);
            } else if (strcmp(key.c_str(), "VF_GGTT") == 0 && currentNameId) {
                data[currentNameId].vfGgtt = std::stoul(value);
            } else if (strcmp(key.c_str(), "VGPU_SCHEDULER") == 0 && currentNameId) {
                updateVgpuSchedulerConfigParamerters(devicePciId, currentNameId, value, data);
                if (currentNameId == XPUM_MAX_VF_NUM)
                    default_vgpu_scheduler = value;
            } else if (strcmp(key.c_str(), "DRIVERS_AUTOPROBE") == 0 && currentNameId) {
                data[currentNameId].driversAutoprobe = (bool)std::stoi(value);
                if (currentNameId != XPUM_MAX_VF_NUM) {
                    XPUM_LOG_DEBUG("find predefined vgpu configration from vgpu.conf");
                    break;
                }
            }
        }
    }
    if (data.size())
        attrs = combineAttrConfig(data, numVfs, devicePciId, default_vgpu_scheduler);
    return true;
}

xpum_result_t VgpuManager::vgpuValidateDevice(xpum_device_id_t deviceId) {
    auto device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    
    Property prop;
    device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_FUNCTION_TYPE, prop);
    if (static_cast<xpum_device_function_type_t>(prop.getValueInt()) != DEVICE_FUNCTION_TYPE_PHYSICAL) {
        return XPUM_VGPU_VF_UNSUPPORTED_OPERATION;
    }
    
    device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_DEVICE_ID, prop);
    int pciDeviceId = std::stoi(prop.getValue().substr(2), nullptr, 16);
    if (getVFMaxNumberByPciDeviceId(pciDeviceId) == 0) {
        return XPUM_VGPU_UNSUPPORTED_DEVICE_MODEL;
    }

    return XPUM_OK;
}

bool VgpuManager::createVfInternal(const DeviceSriovInfo& deviceInfo, AttrFromConfigFile& attrs, uint32_t numVfs, uint64_t lmem) {
    std::string devicePathString = std::string("/sys/class/drm/") + deviceInfo.drmPath;
    try {
        if (deviceInfo.deviceModel == XPUM_DEVICE_MODEL_ATS_M_1 || deviceInfo.deviceModel == XPUM_DEVICE_MODEL_ATS_M_3 || deviceInfo.deviceModel == XPUM_DEVICE_MODEL_ATS_M_1G) {
            writeFile(devicePathString + "/iov/pf/gt/exec_quantum_ms", std::to_string(attrs.pfExec));
            writeFile(devicePathString + "/iov/pf/gt/preempt_timeout_us", std::to_string(attrs.pfPreempt));
            writeFile(devicePathString + "/iov/pf/gt/policies/sched_if_idle", attrs.schedIfIdle ? "1": "0");
            for (uint32_t vfNum = 1; vfNum <= numVfs; vfNum++) {
                writeVfAttrToSysfs(devicePathString + "/iov/vf" + std::to_string(vfNum) + "/gt", attrs, lmem);
            }
        } else if (deviceInfo.deviceModel == XPUM_DEVICE_MODEL_PVC) {
            for (uint32_t tile = 0; tile < deviceInfo.numTiles; tile++) {
                std::string gtNum = std::to_string(tile);
                writeFile(devicePathString + "/iov/pf/gt" + gtNum + "/exec_quantum_ms", std::to_string(attrs.pfExec));
                writeFile(devicePathString + "/iov/pf/gt" + gtNum + "/preempt_timeout_us", std::to_string(attrs.pfPreempt));
                writeFile(devicePathString + "/iov/pf/gt" + gtNum + "/policies/sched_if_idle", attrs.schedIfIdle ? "1": "0");
                // Each VF should be mapped to only one tile, except the case of 1 VF on 2 tiles
                if (numVfs == 1 && deviceInfo.numTiles > 1) {
                    std::string vfResPath = devicePathString + "/iov/vf1/gt" + gtNum + "/";
                    attrs.vfGgtt /= deviceInfo.numTiles;
                    attrs.vfDoorbells /= deviceInfo.numTiles;
                    attrs.vfContexts /= deviceInfo.numTiles;
                    writeVfAttrToSysfs(devicePathString + "/iov/vf1/gt" + gtNum, attrs, lmem / deviceInfo.numTiles);
                } else {
                    for (uint32_t vfNum = 1; vfNum <= numVfs; vfNum++) {
                        if (vfNum % deviceInfo.numTiles != tile) {
                            continue;
                        }
                        writeVfAttrToSysfs(devicePathString + "/iov/vf" + std::to_string(vfNum) + "/gt" + gtNum, attrs, lmem);
                    }
                }   
            }
        } else if (deviceInfo.deviceModel >= XPUM_DEVICE_MODEL_BMG) {
            std::string debugfsPath = std::string("/sys/kernel/debug/dri/") + deviceInfo.bdfAddress;

            writeFile(debugfsPath + "/gt0/pf/exec_quantum_ms", std::to_string(attrs.pfExec));
            writeFile(debugfsPath + "/gt0/pf/preempt_timeout_us", std::to_string(attrs.pfPreempt));
            writeFile(debugfsPath + "/gt0/pf/sched_if_idle", attrs.schedIfIdle ? "1": "0");
            writeFile(debugfsPath + "/gt1/pf/exec_quantum_ms", std::to_string(attrs.pfExec));
            writeFile(debugfsPath + "/gt1/pf/preempt_timeout_us", std::to_string(attrs.pfPreempt));
            writeFile(debugfsPath + "/gt1/pf/sched_if_idle", attrs.schedIfIdle ? "1": "0");

            for (uint32_t vfNum = 1; vfNum <= numVfs; vfNum++) {
                std::string vfResPath = debugfsPath + "/gt0/vf" + std::to_string(vfNum);
                writeVfAttrToGt0Sysfs(vfResPath, attrs, lmem);
                vfResPath = debugfsPath + "/gt1/vf" + std::to_string(vfNum);
                writeVfAttrToGt1Sysfs(vfResPath, attrs);
            }
        }
        writeFile(devicePathString + "/device/sriov_drivers_autoprobe", attrs.driversAutoprobe ? "1" : "0");
        writeFile(devicePathString + "/device/sriov_numvfs", std::to_string(numVfs));
    } catch (std::ios::failure &e) {
        return false;
    }
    return true;
}


void VgpuManager::writeVfAttrToGt0Sysfs(std::string vfDir, AttrFromConfigFile attrs, uint64_t lmem) {
    writeVfAttrToSysfs(vfDir, attrs, lmem);
}

void VgpuManager::writeVfAttrToGt1Sysfs(std::string vfDir, AttrFromConfigFile attrs) {
    writeFile(vfDir + "/exec_quantum_ms", std::to_string(attrs.vfExec));
    writeFile(vfDir + "/preempt_timeout_us", std::to_string(attrs.vfPreempt));
    writeFile(vfDir + "/doorbells_quota", std::to_string(attrs.vfDoorbells));
    writeFile(vfDir + "/contexts_quota", std::to_string(attrs.vfContexts));
}

void VgpuManager::writeVfAttrToSysfs(std::string vfDir, AttrFromConfigFile attrs, uint64_t lmem) {
    writeFile(vfDir + "/exec_quantum_ms", std::to_string(attrs.vfExec));
    writeFile(vfDir + "/preempt_timeout_us", std::to_string(attrs.vfPreempt));
    writeFile(vfDir + "/lmem_quota", std::to_string(lmem));
    writeFile(vfDir + "/ggtt_quota", std::to_string(attrs.vfGgtt));
    writeFile(vfDir + "/doorbells_quota", std::to_string(attrs.vfDoorbells));
    writeFile(vfDir + "/contexts_quota", std::to_string(attrs.vfContexts));
}

}
