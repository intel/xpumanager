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

#include "./vgpu_manager.h"
#include "core/core.h"
#include "infrastructure/xpum_config.h"
#include "infrastructure/configuration.h"
#include "infrastructure/utility.h"
#include "xpum_api.h"

namespace xpum {

static bool is_path_exist(const std::string &s) {
  struct stat buffer;
  return (stat(s.c_str(), &buffer) == 0);
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
        if (deviceInfo.deviceModel == XPUM_DEVICE_MODEL_ATS_M_1 || deviceInfo.deviceModel == XPUM_DEVICE_MODEL_ATS_M_3) {
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
                if (deviceInfo.deviceModel == XPUM_DEVICE_MODEL_ATS_M_1 || deviceInfo.deviceModel == XPUM_DEVICE_MODEL_ATS_M_3) {
                    writeVfAttrToSysfs(iovPath.str() + ent->d_name + "/gt", zeroAttr, 0);
                } else if (deviceInfo.deviceModel == XPUM_DEVICE_MODEL_PVC) {
                    for (uint32_t tile = 0; tile < deviceInfo.numTiles; tile++) {
                        writeVfAttrToSysfs(iovPath.str() + ent->d_name + "/gt" + std::to_string(tile), zeroAttr, 0);
                    }
                }
            } catch(std::ios::failure &e) {
                return XPUM_VGPU_REMOVE_VF_FAILED;
            }
        }
    } else {
        XPUM_LOG_ERROR("Failed to open directory {}", iovPath.str());
        return XPUM_VGPU_REMOVE_VF_FAILED;
    }
    return XPUM_OK;
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
    if (data.deviceModel == XPUM_DEVICE_MODEL_ATS_M_1 || data.deviceModel == XPUM_DEVICE_MODEL_ATS_M_3) {
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
                auto nameList = Utility::split(value, ',');
                for (auto nameItem: nameList) {
                    sscanf(nameItem.c_str(), "%4sN%d", s, &i);
                    if (strcmp(s, devicePciId.c_str()) == 0 && i == numVfs) {
                        target = true;
                        break;
                    } else {
                        target = false;
                    }
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
    std::vector<int> supportedDevices{0x56c0, 0x56c1, 0x0bd5, 0x0bd6, 0x0bda, 0x0bdb};
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
        if (deviceInfo.deviceModel == XPUM_DEVICE_MODEL_ATS_M_1 || deviceInfo.deviceModel == XPUM_DEVICE_MODEL_ATS_M_3) {
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