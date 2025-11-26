/* 
 *  Copyright (C) 2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file precheck.cpp
 */

#include <dirent.h>
#include <fstream>

#include "precheck.h"
#include "core/core.h"
#include "firmware/system_cmd.h"
#include "infrastructure/logger.h"

namespace xpum {

    
std::string readFileSingleLine(const std::string& path) {
    std::ifstream sysfs(path);
    std::string value;
    if (sysfs.is_open()) {
        getline(sysfs, value);
        sysfs.close();
    }
    return value;
}

bool isIommuDeviceFound() {
    DIR *dir;
    struct dirent *ent;
    /*
     *   The devices managed by IOMMU will be listed here: /sys/class/iommu/<iommu instance>/devices
     *   So /sys/class/iommu/ non-empty means IOMMU enabled.
     *   https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-class-iommu
     */
    if ((dir = opendir(std::string("/sys/class/iommu").c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
                closedir(dir);
                return true;
            }
        }
    } else {
        XPUM_LOG_ERROR("Failed to open directory /sys/class/iommu");
    }
    closedir(dir);
    return false;
}

xpum_result_t vgpuPrecheck(xpum_device_id_t deviceId,
    xpum_vgpu_precheck_result_t* result) {
    /*
    *   VMX flag: lscpu
    *   iommu status: /sys/class/iommu
    *   sr-iov status /sys/bus/pci/devices/[device BDF address]/sriov_totalvfs
    */
    result->iommuMessage[0] = 0;
    result->sriovMessage[0] = 0;
    result->vmxMessage[0] = 0;
    auto cmdRes = execCommand("lscpu");
    XPUM_LOG_DEBUG("Checking VMX flag, result: {}", cmdRes.output());
    if (cmdRes.exitStatus()) {
        result->vmxFlag = false;
        std::string msg = "Command lscpu failed.";
        strncpy(result->vmxMessage, msg.c_str(), msg.size() + 1);
    } else if (cmdRes.output().find("vmx") != std::string::npos) {
        /*
        *   VMX flag detected by lscpu
        */
        result->vmxFlag = true;
    } else {
        result->vmxFlag = false;
        std::string msg = "No VMX flag, Please ensure Intel VT enabled in BIOS";
        strncpy(result->vmxMessage, msg.c_str(), msg.size() + 1);
    }

    bool iommuFound = isIommuDeviceFound();
    XPUM_LOG_DEBUG("Checking IOMMU stauts, IOMMU device{} found", iommuFound ? "": " not");
    if (iommuFound) {
        result->iommuStatus = true;
    } else {
        result->iommuStatus = false;
        std::string msg = "IOMMU is disabled. Please set the related BIOS settings and kernel command line parameters.";
        strncpy(result->iommuMessage, msg.c_str(), msg.size() + 1);
    }

    Property prop;
    auto device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    device->getProperty(xpum_device_internal_property_name_enum::XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_FUNCTION_TYPE, prop);
    if (static_cast<xpum_device_function_type_t>(prop.getValueInt()) != DEVICE_FUNCTION_TYPE_PHYSICAL) {
        return XPUM_VGPU_VF_UNSUPPORTED_OPERATION;
    }

    device->getProperty(
        xpum_device_internal_property_name_enum::XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS,
        prop
    );
    const std::string& deviceBdfAddr = prop.getValue();
    std::string totalVfs = readFileSingleLine("/sys/bus/pci/devices/" + deviceBdfAddr + "/sriov_totalvfs");
    XPUM_LOG_DEBUG("Checking SR-IOV status, /sys/bus/pci/devices/{}/sriov_totalvfs report {}", deviceBdfAddr, totalVfs);
    /*
     *   SR-IOV is enabled by i915, so all of the cards should have either totalvfs > 0 or totalvfs == 0
     */
    if (totalVfs.size() == 0) {
        result->sriovStatus = false;
        std::string msg = "Failed to read sriov_totalvfs.";
        strncpy(result->sriovMessage, msg.c_str(), msg.size() + 1);
    } else if (stoi(totalVfs) > 0) {
        result->sriovStatus = true;
    } else {
        result->sriovStatus = false;
        std::string msg = "SR-IOV is disabled or sriov_totalvfs is 0. Please set the related BIOS settings and kernel command line parameters.";
        strncpy(result->sriovMessage, msg.c_str(), msg.size() + 1);
    }
    return XPUM_OK;
}


}
