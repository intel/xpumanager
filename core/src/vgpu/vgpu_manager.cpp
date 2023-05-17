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

#include "./vgpu_manager.h"
#include "core/core.h"
#include "infrastructure/xpum_config.h"
#include "infrastructure/configuration.h"
#include "xpum_api.h"

namespace xpum {

static bool is_path_exist(const std::string &s) {
  struct stat buffer;
  return (stat(s.c_str(), &buffer) == 0);
}


xpum_result_t VgpuManager::createVf(xpum_device_id_t deviceId, xpum_vgpu_config_t* param) {
    XPUM_LOG_DEBUG("vgpuCreateVf, {}, {}, {}", deviceId, param->numVfs, param->lmemPerVf);

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

    AttrFromConfigFile attrs;
    bool readFlag = readConfigFromFile(deviceId, param->numVfs, attrs);
    if (!readFlag) {
        return XPUM_VGPU_NO_CONFIG_FILE;
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

    std::stringstream devicePath;
    devicePath << "/sys/class/drm/" << deviceInfo.drmPath;
    std::string devicePathString = devicePath.str();
    try {
        writeFile(std::string(devicePathString).append("/iov/pf/gt/exec_quantum_ms"), std::to_string(attrs.pfExec));
        writeFile(std::string(devicePathString).append("/iov/pf/gt/preempt_timeout_us"), std::to_string(attrs.pfPreempt));
        writeFile(std::string(devicePathString).append("/iov/pf/gt/policies/sched_if_idle"), attrs.schedIfIdle ? "1": "0");
        for (uint32_t i = 0; i < param->numVfs; i++) {
            std::stringstream vfPath;
            vfPath << devicePath.str() << "/iov/vf" << i + 1 << "/gt/";
            std::string vfPathString = vfPath.str();
            writeFile(std::string(vfPathString).append("exec_quantum_ms"), std::to_string(attrs.vfExec));
            writeFile(std::string(vfPathString).append("preempt_timeout_us"), std::to_string(attrs.vfPreempt));
            writeFile(std::string(vfPathString).append("lmem_quota"), std::to_string(lmemToUse));
            writeFile(std::string(vfPathString).append("ggtt_quota"), std::to_string(attrs.vfGgtt));
            writeFile(std::string(vfPathString).append("doorbells_quota"), std::to_string(attrs.vfDoorbells));
            writeFile(std::string(vfPathString).append("contexts_quota"), std::to_string(attrs.vfContexts));
        }
        writeFile(std::string(devicePathString).append("/device/sriov_drivers_autoprobe"), attrs.driversAutoprobe ? "1" : "0");
        writeFile(std::string(devicePathString).append("/device/sriov_numvfs"), std::to_string(param->numVfs));
    } catch (std::ios::failure &e) {
        return XPUM_VGPU_CREATE_VF_FAILED;
    }

    return XPUM_OK;
}

/*
 *  1. Get number of VFs
 *  2. Get intersted value in the path of PF and each VF
 */
xpum_result_t VgpuManager::getFunctionList(xpum_device_id_t deviceId, std::vector<xpum_vgpu_function_info_t> &result) {
    XPUM_LOG_DEBUG("getFunctionList, device id: {}", deviceId);
    
    DeviceSriovInfo deviceInfo;
    if (!loadSriovData(deviceId, deviceInfo)) {
        return XPUM_VGPU_SYSFS_ERROR;
    }
    std::string numVfsString;
    std::stringstream devicePath;

    devicePath << "/sys/class/drm/" << deviceInfo.drmPath;
    XPUM_LOG_DEBUG("device Path: {}", devicePath.str());
    try {
        readFile(devicePath.str().append("/device/sriov_numvfs"), numVfsString);
    } catch (std::ios::failure &e) {
        return XPUM_VGPU_SYSFS_ERROR;
    }

    int numVfs = std::stoi(numVfsString);
    XPUM_LOG_DEBUG("{} VF detected.", numVfs);
    /*
     *  Put PF info into index 0, and VF1..n into index 1..n respectively
     */
    for (int i = 0; i < numVfs + 1; i++) {
        std::string lmemString, uevent;
        std::stringstream lmemPath, ueventPath;
        if (i == 0) {
            lmemPath << devicePath.str() << "/iov/pf/gt/available/lmem_free";
            ueventPath << devicePath.str() << "/iov/pf/device/uevent";
        } else {
            lmemPath << devicePath.str() << "/iov/vf" << i << "/gt/lmem_quota";
            ueventPath << devicePath.str() << "/iov/vf" << i << "/device/uevent";
        }

        try {
            readFile(lmemPath.str(), lmemString);
        } catch (std::ios::failure &e) {
            return XPUM_VGPU_SYSFS_ERROR;
        }
        xpum_vgpu_function_info_t info;
        info.lmemSize = std::stoul(lmemString);
        info.functionType = (i == 0 ? DEVICE_FUNCTION_TYPE_PHYSICAL : DEVICE_FUNCTION_TYPE_VIRTUAL);
        
        info.bdfAddress[0] = 0;
        std::ifstream ifs(ueventPath.str());
        std::string line;
        while (std::getline(ifs, line)) {
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
    if ((dir = opendir(iovPath.str().c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (strstr(ent->d_name, "vf") == NULL) {
                continue;
            }
            std::string vfPath = iovPath.str().append(ent->d_name).append("/gt/");
            try {
                writeFile(std::string(vfPath).append("doorbells_quota"), "0");
                writeFile(std::string(vfPath).append("contexts_quota"), "0");
                writeFile(std::string(vfPath).append("ggtt_quota"), "0");
                writeFile(std::string(vfPath).append("lmem_quota"), "0");
                writeFile(std::string(vfPath).append("exec_quantum_ms"), "0");
                writeFile(std::string(vfPath).append("preempt_timeout_us"), "0");
            } catch (std::ios::failure &e) {
                return XPUM_VGPU_REMOVE_VF_FAILED;
            }
        }
    } else {
        XPUM_LOG_ERROR("Failed to open directory {}", iovPath.str());
    }
    return XPUM_OK;
}


bool VgpuManager::loadSriovData(xpum_device_id_t deviceId, DeviceSriovInfo &data) {
    auto device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    Property prop;

    data.deviceModel = device->getDeviceModel();

    device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_DRM_DEVICE, prop);
    char drm[256];
    sscanf(prop.getValue().c_str(), "/dev/dri/%s", drm);
    data.drmPath = std::string(drm);
    
    device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS, prop);
    data.bdfAddress = prop.getValue();
    
    bool available;
    bool configurable;
    xpum_ecc_state_t current, pending;
    xpum_ecc_action_t action;
    xpumGetEccState(stoi(device->getId()), &available, &configurable, &current, &pending, &action);
    data.eccState = current;
    
    std::string lmem, ggtt, doorbell, context;
    std::stringstream lmemPath, ggttPath, doorbellPath, contextPath;
    lmemPath << "/sys/class/drm/" << drm << "/iov/pf/gt/available/lmem_free";
    ggttPath << "/sys/class/drm/" << drm << "/iov/pf/gt/available/ggtt_free";
    doorbellPath << "/sys/class/drm/" << drm << "/iov/pf/gt/available/doorbells_free";
    contextPath << "/sys/class/drm/" << drm << "/iov/pf/gt/available/contexts_free";
    try {
        readFile(lmemPath.str(), lmem);
        readFile(ggttPath.str(), ggtt);
        readFile(doorbellPath.str(), doorbell);
        readFile(contextPath.str(), context);
    } catch (std::ios::failure &e) {
        return false;
    }
    data.lmemSizeFree = std::stoul(lmem);
    data.ggttSizeFree = std::stoul(ggtt);
    data.contextFree = std::stoi(context);
    data.doorbellFree = std::stoi(doorbell);
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

void VgpuManager::writeFile(const std::string& path, const std::string& content) {
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

bool VgpuManager::readConfigFromFile(xpum_device_id_t deviceId, uint32_t numVfs, AttrFromConfigFile &attrs) {
    std::string fileName = std::string(XPUM_CONFIG_DIR) + std::string("vgpu.conf");
    if (!is_path_exist(fileName)) {
        char exe_path[XPUM_MAX_PATH_LEN];
        ssize_t len = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path));
        exe_path[len] = '\0';
        std::string current_file = exe_path;
        fileName = current_file.substr(0, current_file.find_last_of('/')) + "/../lib/" + Configuration::getXPUMMode() + "/config/" + std::string("vgpu.conf");
        if (!is_path_exist(fileName))
            fileName = current_file.substr(0, current_file.find_last_of('/')) + "/../lib64/" + Configuration::getXPUMMode() + "/config/" + std::string("vgpu.conf");
    }

    auto deviceModel = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId))->getDeviceModel();
    std::string devicePciId;
    if (deviceModel == XPUM_DEVICE_MODEL_ATS_M_1) {
        devicePciId = "56c0";
    } else if (deviceModel == XPUM_DEVICE_MODEL_ATS_M_3) {
        devicePciId = "56c1";
    }

    std::ifstream ifs(fileName);
    if (ifs.fail()) {
        return false;
    }
    std::string line;
    bool target = false;
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
                sscanf(value.c_str(), "%4sN%d", s, &i);
                if (strcmp(s, devicePciId.c_str()) == 0 && i == numVfs) {
                    target = true;
                } else {
                    target = false;
                }
            } else if (strcmp(key.c_str(), "VF_LMEM") == 0 && target) {
                attrs.vfLmem = std::stoul(value);
            } else if (strcmp(key.c_str(), "VF_LMEM_ECC") == 0 && target) {
                attrs.vfLmemEcc = std::stoul(value);
            } else if (strcmp(key.c_str(), "VF_CONTEXTS") == 0 && target) {
                attrs.vfContexts = std::stoi(value);
            } else if (strcmp(key.c_str(), "VF_DOORBELLS") == 0 && target) {
                attrs.vfDoorbells = std::stoi(value);
            } else if (strcmp(key.c_str(), "VF_GGTT") == 0 && target) {
                attrs.vfGgtt = std::stoul(value);
            } else if (strcmp(key.c_str(), "VF_EXEC_QUANT_MS") == 0 && target) {
                attrs.vfExec = std::stoul(value);
            } else if (strcmp(key.c_str(), "VF_PREEMPT_TIMEOUT_US") == 0 && target) {
                attrs.vfPreempt = std::stoul(value);
            } else if (strcmp(key.c_str(), "PF_EXEC_QUANT_MS") == 0 && target) {
                attrs.pfExec = std::stoul(value);
            } else if (strcmp(key.c_str(), "PF_PREEMPT_TIMEOUT") == 0 && target) {
                attrs.pfPreempt = std::stoul(value);
            } else if (strcmp(key.c_str(), "SCHED_IF_IDLE") == 0 && target) {
                attrs.schedIfIdle = (bool)std::stoi(value);
            } else if (strcmp(key.c_str(), "DRIVERS_AUTOPROBE") == 0 && target) {
                attrs.driversAutoprobe = (bool)std::stoi(value);
            }
        }
    }
    return true;
}

}