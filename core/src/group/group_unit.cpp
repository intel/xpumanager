/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file group_unit.cpp
 */

#include "group_unit.h"

#include "infrastructure/logger.h"

namespace xpum {

GroupUnit::GroupUnit(std::string groupname, xpum_group_id_t groupId)
    : topoLevel(0) {
    XPUM_LOG_TRACE("GroupUnit");
    name = groupname;
    id = groupId;
}

GroupUnit::~GroupUnit() {
    XPUM_LOG_TRACE("~GroupUnit");
    deviceList.clear();
}

xpum_group_id_t GroupUnit::getId() {
    return id;
}

unsigned int GroupUnit::getDeviceCount() {
    return deviceList.size();
}

void GroupUnit::getName(char groupname[XPUM_MAX_STR_LENGTH]) {
    std::size_t length = name.copy(groupname, XPUM_MAX_STR_LENGTH);
    groupname[length] = '\0';
}

void GroupUnit::getDeviceList(xpum_device_id_t device_List[XPUM_MAX_NUM_DEVICES]) {
    for (unsigned int idx = 0; idx < deviceList.size(); idx++) {
        device_List[idx] = deviceList[idx];
    }
}

xpum_result_t GroupUnit::addDevice(xpum_device_id_t deviceId) {
    XPUM_LOG_TRACE("GroupUnit::addDevice");

    for (unsigned int i = 0; i < deviceList.size(); i++) {
        if (deviceList[i] == deviceId) {
            XPUM_LOG_ERROR(std::string("GroupUnit::addDevice- device id ") + std::string(std::to_string(deviceId)) + std::string(" was already in the group."));
            return XPUM_GROUP_DEVICE_DUPLICATED;
        }
    }

    deviceList.push_back(deviceId);

    return XPUM_OK;
}

xpum_result_t GroupUnit::removeDevice(const std::shared_ptr<DeviceManagerInterface>& p_devicemanager,
                                      xpum_group_id_t groupId, xpum_device_id_t deviceId) {
    XPUM_LOG_TRACE("GroupUnit::removeDevice");
    //xpum_result_t ret = XPUM_GENERIC_ERROR;

    for (unsigned int i = 0; i < deviceList.size(); i++) {
        if (deviceList[i] == deviceId) {
            deviceList.erase(deviceList.begin() + i);
            return XPUM_OK;
        }
    }

    XPUM_LOG_ERROR(std::string("GroupUnit::removeDevice- device id ") + std::to_string(deviceId) + std::string(" not in the group."));

    return XPUM_RESULT_DEVICE_NOT_FOUND;
}

void GroupUnit::setPcieTopo(std::vector<zes_pci_address_t>& pcieTop) {
    std::copy(pcieTop.begin(), pcieTop.end(), std::back_inserter(pcieTopology));
}

bool GroupUnit::deviceInGroup(std::vector<zes_pci_address_t>& pcieTop) {
    if (topoLevel != 0) {
        if (pcieTop.size() > topoLevel) {
            if (pcieTop.at(topoLevel).domain == pcieTopology.at(topoLevel).domain &&
                pcieTop.at(topoLevel).bus == pcieTopology.at(topoLevel).bus &&
                pcieTop.at(topoLevel).device != pcieTopology.at(topoLevel).device) {
                return true;
            }
        }
    } else {
        if (pcieTop.size() != pcieTopology.size()) {
            return false;
        }

        for (std::size_t i = 0; i < pcieTop.size(); i++) {
            if (pcieTop.at(i).domain == pcieTopology.at(i).domain &&
                pcieTop.at(i).bus == pcieTopology.at(i).bus &&
                pcieTop.at(i).device != pcieTopology.at(i).device) {
                topoLevel = i;
                return true;
            }
        }
    }
    return false;
}

} // end namespace xpum
