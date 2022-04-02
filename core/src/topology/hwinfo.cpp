/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file hwinfo.cpp
 */

#include "hwinfo.h"

#include <experimental/filesystem>

#include "core/core.h"
#include "hwloc.h"
#include "infrastructure/exception/ilegal_parameter_exception.h"
#include "infrastructure/property.h"

namespace xpum {

std::string HWInfo::getDevicePath(const std::string& bdf_address) {
    std::string devicePath = "/sys/devices";
    std::string result;
    namespace stdfs = std::experimental::filesystem;
    const stdfs::recursive_directory_iterator end{};

    for (stdfs::recursive_directory_iterator iter{devicePath}; iter != end; ++iter) {
        if (iter->path().string().find(bdf_address, 0) == std::string::npos) {
            continue;
        }
        if (stdfs::is_directory(*iter)) {
            result = iter->path().string();
            break;
        }
    }

    return result;
}

bool HWInfo::isPcieDevExist(xpum_device_id_t deviceId) {
    Property prop;
    std::shared_ptr<Device> p_device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (p_device == nullptr) {
        XPUM_LOG_ERROR("isPcieDevExist, device {} not exist", deviceId);
        throw IlegalParameterException("device does not exist");
    }
    if (!p_device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS, prop)) {
        throw BaseException(ErrorCode::UNKNOWN, "PCI_BDF_ADDRESS not exist");
    }
    bool find = false;
    std::string bdfAddress = prop.getValue();
    std::size_t start = 0, pos = 0;
    int32_t domain = std::stoi(bdfAddress.substr(start), &pos, 16);
    start += pos + 1;
    pos = 0;
    int32_t bus = std::stoi(bdfAddress.substr(start), &pos, 16);
    start += pos + 1;
    pos = 0;
    int32_t device = std::stoi(bdfAddress.substr(start), &pos, 16);
    start += pos + 1;
    pos = 0;
    int32_t function = std::stoi(bdfAddress.substr(start), &pos, 16);

    hwloc_topology_t hwtopology;
    hwloc_obj_t obj = nullptr;
    hwloc_topology_init(&hwtopology);
    hwloc_topology_set_io_types_filter(hwtopology, HWLOC_TYPE_FILTER_KEEP_ALL);
    hwloc_topology_load(hwtopology);
    while ((obj = hwloc_get_next_pcidev(hwtopology, obj)) != nullptr) {
        assert(obj->type == HWLOC_OBJ_PCI_DEVICE);
        if (obj->attr->pcidev.domain == domain && obj->attr->pcidev.bus == bus && obj->attr->pcidev.dev == device && obj->attr->pcidev.func == function) {
            find = true;
            break;
        }
    }
    hwloc_topology_destroy(hwtopology);
    return find;
}

} // end namespace xpum
