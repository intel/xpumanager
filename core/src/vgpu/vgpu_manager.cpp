/* 
 *  Copyright (C) 2023 Intel Corporation
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
        default:
            return 0;
    }
}

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
        } else {
            return XPUM_VGPU_UNSUPPORTED_DEVICE_MODEL;
        }

        if (functionIndex == 0) {
            ueventPath = devicePath + "/iov/pf/device/uevent";
        } else {
            ueventPath = devicePath + "/iov/vf" + std::to_string(functionIndex) + "/device/uevent";
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

    /*
     *  Disable all VFs by setting sriov_numvfs to 0
     */
    numvfsPath << "/sys/bus/pci/devices/" << deviceInfo.bdfAddress << "/sriov_numvfs";
    try {
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
    } else {
        XPUM_LOG_ERROR("Failed to open directory {}", iovPath.str());
        return XPUM_VGPU_REMOVE_VF_FAILED;
    }
    closedir(dir);
    return XPUM_OK;
}

static xpum_realtime_metric_type_t engineToMetricType(zes_engine_group_t engine) {
    xpum_realtime_metric_type_t metricType = XPUM_STATS_MAX;
    switch (engine) {
        case ZES_ENGINE_GROUP_ALL:
            metricType = XPUM_STATS_GPU_UTILIZATION;
            break;
        case ZES_ENGINE_GROUP_COMPUTE_ALL:
            metricType = XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION;
            break;
        case ZES_ENGINE_GROUP_MEDIA_ALL:
            metricType = XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION;
            break;
        case ZES_ENGINE_GROUP_COPY_ALL:
            metricType = XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION;
            break;
        case ZES_ENGINE_GROUP_RENDER_ALL:
            metricType = XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION;
            break;
        default:
            break;
    }
    return metricType;
}

typedef ze_result_t (*pfnZesEngineGetActivityExt_t)(
    zes_engine_handle_t hEngine,
    uint32_t *pCount, zes_engine_stats_t *pStats);

static bool getEngineStats(std::map<zes_engine_group_t, 
    std::vector<zes_engine_stats_t>> &snap, 
    std::vector<zes_engine_handle_t> engines,
    pfnZesEngineGetActivityExt_t pfnZesEngineGetActivityExt) {
    ze_result_t res = ZE_RESULT_SUCCESS;
    for (auto &engine : engines) {
        zes_engine_properties_t props = {};
        props.stype = ZES_STRUCTURE_TYPE_ENGINE_PROPERTIES;
        props.pNext = nullptr;
        XPUM_ZE_HANDLE_LOCK(engine, res = zesEngineGetProperties(engine, 
            &props));
        if (res != ZE_RESULT_SUCCESS) {
            XPUM_LOG_ERROR("zesDeviceEnumEngineGroups returns {}", res);
            return false;
        }
        if (props.type == ZES_ENGINE_GROUP_ALL ||
            props.type == ZES_ENGINE_GROUP_COMPUTE_ALL ||
            props.type == ZES_ENGINE_GROUP_MEDIA_ALL ||
            props.type == ZES_ENGINE_GROUP_COPY_ALL ||
            props.type == ZES_ENGINE_GROUP_RENDER_ALL) {
            uint32_t stats_count = 0;
            XPUM_ZE_HANDLE_LOCK(engine, res = pfnZesEngineGetActivityExt(
                        engine, &stats_count, nullptr));
            if (res != ZE_RESULT_SUCCESS || stats_count <= 1) {
                XPUM_LOG_ERROR("zesEngineGetActivityExt returns {} stats_count = {}",
                               res, stats_count);
                return false;
            }
            std::vector<zes_engine_stats_t> stats(stats_count);
            XPUM_ZE_HANDLE_LOCK(engine, res = pfnZesEngineGetActivityExt(
                        engine, &stats_count, stats.data()));
            if (res != ZE_RESULT_SUCCESS) {
                XPUM_LOG_ERROR("zesEngineGetActivityExt returns {}", res);
                return false;
            }
            snap.insert({props.type, stats});
        }
    }
    return true;
}

xpum_result_t VgpuManager::getVfMetrics(xpum_device_id_t deviceId,
    std::vector<xpum_vf_metric_t> &metrics, uint32_t *count) {
    xpum_result_t ret = XPUM_GENERIC_ERROR;
    auto xdev = Core::instance().getDeviceManager()->getDevice(
        std::to_string(deviceId));
    if (xdev == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    void *handle = dlopen("libze_loader.so.1", RTLD_NOW);
    if (handle == nullptr) {
        return XPUM_LEVEL_ZERO_INITIALIZATION_ERROR;
    }
    pfnZesEngineGetActivityExt_t pfnZesEngineGetActivityExt = 
        reinterpret_cast<pfnZesEngineGetActivityExt_t>(dlsym(handle, 
        "zesEngineGetActivityExt"));
    if (pfnZesEngineGetActivityExt == nullptr) {
        XPUM_LOG_ERROR("dlsym zesEngineGetActivityExt returns NULL");
        dlclose(handle);
        return XPUM_API_UNSUPPORTED;
    }
    auto device = xdev->getDeviceHandle();
    ze_result_t res = ZE_RESULT_SUCCESS;
    uint32_t engine_count = 0;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumEngineGroups(device,
                                    &engine_count, nullptr));
    std::vector<zes_engine_handle_t> engines(engine_count);
    std::map<zes_engine_group_t, std::vector<zes_engine_stats_t>> snap;
    std::map<zes_engine_group_t, std::vector<zes_engine_stats_t>>::iterator it; 
    if (res != ZE_RESULT_SUCCESS || engine_count == 0) {
        XPUM_LOG_ERROR("zesDeviceEnumEngineGroups returns {} engine_count = {}", 
            res, engine_count);
        goto RTN;
    }
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumEngineGroups(device,
                                    &engine_count, engines.data()));
    if (res != ZE_RESULT_SUCCESS || engine_count == 0) {
        XPUM_LOG_ERROR("zesDeviceEnumEngineGroups returns {} engine_count = {}", 
            res, engine_count);
        goto RTN;
    }
    if (getEngineStats(snap, engines, pfnZesEngineGetActivityExt) == false) {
        XPUM_LOG_ERROR("getEngineStats return false");
        goto RTN;
    }
    //check count only
    if (count != nullptr) {
        for (it = snap.begin(); it != snap.end(); it++) {
            *count += it->second.size() - 1;
        }
        XPUM_LOG_DEBUG("check count returns {}", *count);
        ret = XPUM_OK;
        goto RTN;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(
        Configuration::VF_METRICS_INTERVAL));
    for (auto &engine : engines) {
        zes_engine_properties_t props = {};
        props.stype = ZES_STRUCTURE_TYPE_ENGINE_PROPERTIES;
        props.pNext = nullptr;
        XPUM_ZE_HANDLE_LOCK(engine, res = zesEngineGetProperties(engine, 
            &props));
        if (res != ZE_RESULT_SUCCESS) {
            XPUM_LOG_ERROR("zesDeviceEnumEngineGroups returns {}", res);
            goto RTN;
        }
        if (props.type == ZES_ENGINE_GROUP_ALL || 
            props.type == ZES_ENGINE_GROUP_COMPUTE_ALL || 
            props.type == ZES_ENGINE_GROUP_MEDIA_ALL || 
            props.type == ZES_ENGINE_GROUP_COPY_ALL ||
            props.type == ZES_ENGINE_GROUP_RENDER_ALL) {
            it = snap.find(props.type);
            if (it == snap.end()) {
                XPUM_LOG_ERROR("Engine stats not found");
                goto RTN;
            }
            auto stats0 = it->second;
            uint32_t stats_count1 = 0;
            XPUM_ZE_HANDLE_LOCK(engine, res = pfnZesEngineGetActivityExt(
                        engine, &stats_count1, nullptr));
            std::vector<zes_engine_stats_t> stats1(stats_count1);
            if (res != ZE_RESULT_SUCCESS || stats_count1 <= 1) {
                XPUM_LOG_ERROR("zesEngineGetActivityExt returns {} stats_count1 = {}", 
                    res, stats_count1);
                goto RTN;
            }
            XPUM_ZE_HANDLE_LOCK(engine, res = pfnZesEngineGetActivityExt(
                        engine, &stats_count1, stats1.data()));
            if (res != ZE_RESULT_SUCCESS || stats_count1 != stats0.size()) {
                XPUM_LOG_ERROR("zesEngineGetActivityExt returns {} stats_count1 = {} stats0.size() = {}", 
                    res, stats_count1, stats0.size());
                goto RTN;
            }
            for (uint32_t i = 1; i < stats_count1; i++) {
                xpum_vf_metric_t vfm = {};
                XPUM_LOG_DEBUG("engine type {} VF index {} stats0 activeTime {} timestamp {} stats1 activeTime {} timestamp {}", 
                        props.type, i, stats0[i].activeTime, stats0[i].timestamp, 
                        stats1[i].activeTime, stats1[i].timestamp);
                if (stats1[i].timestamp == stats0[i].timestamp) {
                    XPUM_LOG_DEBUG("NA: engine type {} VF index {} stats0 activeTime {} timestamp {} stats1 activeTime {} timestamp {}", 
                        props.type, i, stats0[i].activeTime, stats0[i].timestamp, 
                        stats1[i].activeTime, stats1[i].timestamp);
                    continue;
                }
                vfm.vfIndex = i;
                if (getVfBdf(vfm.bdfAddress, XPUM_MAX_STR_LENGTH, i, deviceId)
                        == false) {
                    XPUM_LOG_ERROR("getVfBdf returns false at vf index {}", i);
                    goto RTN;
                }
                uint64_t val = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * 
                    100 * (stats1[i].activeTime - 
                        stats0[i].activeTime) / (stats1[i].timestamp -
                            stats0[i].timestamp);
                vfm.deviceId = deviceId;
                auto metricType = engineToMetricType(props.type);
                if (metricType == XPUM_STATS_MAX) {
                    XPUM_LOG_ERROR("Unsupported engine type {}", props.type);
                    goto RTN;
                }
                vfm.metric.metricsType = metricType;
                vfm.metric.value = val;
                vfm.metric.scale = 
                    Configuration::DEFAULT_MEASUREMENT_DATA_SCALE;
                metrics.push_back(vfm);
            }
        }
    }
    ret = XPUM_OK;
RTN:
    dlclose(handle);
    return ret;
}

bool VgpuManager::getVfBdf(char *bdf, uint32_t szBdf, uint32_t vfIndex, 
        xpum_device_id_t deviceId) {
//BDF Format in uevent is cccc:cc:cc.c
#define BDF_SIZE 12
    if (bdf == nullptr || szBdf < BDF_SIZE + 1) {
        return false;
    }
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
    str = "/sys/class/drm/" + str + "/iov/vf" + std::to_string(vfIndex) + 
        "/device/uevent";
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
    } else {
        return false;
    }
    return true;
}

void VgpuManager::readFile(const std::string& path, std::string& content) {
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
    if (devicePciId == "56c0" || devicePciId == "56c1" || devicePciId == "56c2") {
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
    
    // Now we only need to support ATSM and some of PVC
    std::vector<int> supportedDevices{0x56c0, 0x56c1, 0x56c2, 0x0bd4, 0x0bd5, 0x0bd6, 0x0bda, 0x0bdb, 0x0b6e};
    device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_DEVICE_ID, prop);
    int pciDeviceId = std::stoi(prop.getValue().substr(2), nullptr, 16);
    if (std::find(supportedDevices.begin(), supportedDevices.end(), pciDeviceId) == supportedDevices.end()) {
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
        }
        writeFile(devicePathString + "/device/sriov_drivers_autoprobe", attrs.driversAutoprobe ? "1" : "0");
        writeFile(devicePathString + "/device/sriov_numvfs", std::to_string(numVfs));
    } catch (std::ios::failure &e) {
        return false;
    }
    return true;
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
