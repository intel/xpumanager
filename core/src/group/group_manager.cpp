/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file group_manager.cpp
 */

#include "group_manager.h"

#include <vector>

#include "infrastructure/device_property.h"
#include "infrastructure/logger.h"
#include "topology/pci_database.h"
#include "topology/topology.h"

namespace xpum {

GroupManager::GroupManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                           std::shared_ptr<DataLogicInterface>& p_data_logic)
    : p_devicemanager(p_device_manager), p_datalogic(p_data_logic), groupSequence(1), internalSequence(1) {
    XPUM_LOG_TRACE("GroupManager()");
}

GroupManager::~GroupManager() {
    XPUM_LOG_TRACE("~GroupManager()");
    groupMap.clear();
}

xpum_result_t GroupManager::createGroup(const char* pGroupName, xpum_group_id_t* pGroupId, bool buildIn) {
    std::unique_lock<std::mutex> lock(this->mutex);
    xpum_result_t ret = XPUM_GENERIC_ERROR;
    xpum_group_id_t groupId;
    std::shared_ptr<GroupUnit> pGroupInfo;
    std::string name = std::string(pGroupName);
    std::size_t buildInCount = internalSequence - 1;

    XPUM_LOG_TRACE("GroupManager::createGroup");

    if (pGroupName == nullptr) {
        XPUM_LOG_DEBUG("GroupManager::createGroup-groupName is nullptr.");
        return ret;
    }

    if (pGroupId == nullptr) {
        XPUM_LOG_DEBUG("GroupManager::createGroup-pGroupId is nullptr.");
        return ret;
    }

    if (groupMap.size() - buildInCount >= XPUM_MAX_NUM_GROUPS) {
        XPUM_LOG_DEBUG("GroupManager::createGroup-group number exceed XPUM_MAX_NUM_GROUPS. all_groups[{}] build_in_groups[{}]",
                       groupMap.size(), buildInCount);
        return ret;
    }
    if (buildIn) {
        groupId = internalSequence++;
        name += std::to_string(groupId);
        groupId |= BUILD_IN_GROUP_MASK;
    } else {
        groupId = groupSequence++;
    }

    groupMap.insert({groupId, std::make_shared<GroupUnit>(name, groupId)});
    *pGroupId = groupId;

    return XPUM_OK;
}

xpum_result_t GroupManager::destroyGroup(xpum_group_id_t groupId) {
    std::unique_lock<std::mutex> lock(this->mutex);
    std::shared_ptr<GroupUnit> pGroupInfo;

    XPUM_LOG_TRACE("GroupManager::destroyGroup");

    if ((groupId & BUILD_IN_GROUP_MASK) == BUILD_IN_GROUP_MASK) {
        XPUM_LOG_DEBUG("GroupManager::destroyGroup- can not destory build-in group {}", groupId);
        return XPUM_GROUP_CHANGE_NOT_ALLOWED;
    }

    GroupMap::iterator groupIterator = groupMap.find(groupId);
    if (groupIterator == groupMap.end()) {
        XPUM_LOG_DEBUG("GroupManager::destroyGroup-not able to find the group {}", groupId);
        return XPUM_RESULT_GROUP_NOT_FOUND;
    } else {
        groupMap.erase(groupId);
        XPUM_LOG_DEBUG("GroupManager::destroyGroup-group {}", groupId);
    }

    return XPUM_OK;
}

xpum_result_t GroupManager::addDeviceToGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId) {
    std::unique_lock<std::mutex> lock(this->mutex);

    if ((groupId & BUILD_IN_GROUP_MASK) == BUILD_IN_GROUP_MASK) {
        XPUM_LOG_DEBUG("GroupManager::addDeviceToGroup- can not add to build-in group {}", groupId);
        return XPUM_GROUP_CHANGE_NOT_ALLOWED;
    }

    std::shared_ptr<GroupUnit> pGroupInfo = getGroupById(groupId);
    if (pGroupInfo == nullptr) {
        XPUM_LOG_DEBUG("GroupManager::addDeviceToGroup-invalid group {}", groupId);
        return XPUM_RESULT_GROUP_NOT_FOUND;
    }

    if (p_devicemanager->getDevice(std::to_string(deviceId)) == nullptr) {
        XPUM_LOG_DEBUG("GroupInfo::addDevice-invalid device id {}", deviceId);
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    return pGroupInfo->addDevice(deviceId);
}

xpum_result_t GroupManager::removeDeviceFromGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId) {
    std::unique_lock<std::mutex> lock(this->mutex);

    if ((groupId & BUILD_IN_GROUP_MASK) == BUILD_IN_GROUP_MASK) {
        XPUM_LOG_DEBUG("GroupManager::removeDeviceFromGroup- can not remove from build-in group {}", groupId);
        return XPUM_GROUP_CHANGE_NOT_ALLOWED;
    }

    std::shared_ptr<GroupUnit> pGroupInfo = getGroupById(groupId);
    if (pGroupInfo == nullptr) {
        XPUM_LOG_DEBUG("GroupManager::removeDeviceFromGroup-invalid group {}", groupId);
        return XPUM_RESULT_GROUP_NOT_FOUND;
    }

    return pGroupInfo->removeDevice(p_devicemanager, groupId, deviceId);
}

xpum_result_t GroupManager::getGroupInfo(xpum_group_id_t groupId, xpum_group_info_t* pGroupInfo) {
    std::unique_lock<std::mutex> lock(this->mutex);
    //xpum_result_t ret = XPUM_GENERIC_ERROR;

    std::shared_ptr<GroupUnit> p_GroupInfo = getGroupById(groupId);
    if (p_GroupInfo == nullptr) {
        XPUM_LOG_DEBUG("GroupManager::getGroupInfo-invalid group {}", groupId);
        return XPUM_RESULT_GROUP_NOT_FOUND;
    }

    pGroupInfo->count = p_GroupInfo->getDeviceCount();
    p_GroupInfo->getName(pGroupInfo->groupName);
    p_GroupInfo->getDeviceList(pGroupInfo->deviceList);
    return XPUM_OK;
}

xpum_result_t GroupManager::getAllGroupIds(xpum_group_id_t groupIds[XPUM_MAX_NUM_GROUPS], int* count) {
    std::unique_lock<std::mutex> lock(this->mutex);
    xpum_result_t ret = XPUM_GENERIC_ERROR;

    int nCount = groupMap.size();
    if (groupIds == nullptr) {
        *count = nCount;
        return XPUM_OK;
    } else if (*count < nCount) {
        *count = nCount;
        return XPUM_BUFFER_TOO_SMALL;
    }

    if (groupIds == nullptr) {
        *count = nCount;
        return ret;
    }

    int index = 0;

    GroupMap::iterator iterator;
    for (iterator = groupMap.begin(); iterator != groupMap.end(); iterator++) {
        std::shared_ptr<GroupUnit> pGroupInfo = iterator->second;
        if (pGroupInfo == nullptr) {
            XPUM_LOG_DEBUG("GroupManager::getAllGroupIds- fatal error, nullptr @ group {}", iterator->first);
            return ret;
        }
        groupIds[index++] = pGroupInfo->getId();
    }
    *count = index;

    return XPUM_OK;
}

std::shared_ptr<GroupUnit> GroupManager::getGroupById(xpum_group_id_t groupId) {
    std::shared_ptr<GroupUnit> pGroupInfo;
    GroupMap::iterator groupIterator = groupMap.find(groupId);
    if (groupIterator == groupMap.end()) {
        XPUM_LOG_DEBUG("GroupManager::getGroupById-not able to find group {}", groupId);
        return nullptr;
    } else {
        pGroupInfo = groupIterator->second;
        if (pGroupInfo == nullptr) {
            XPUM_LOG_DEBUG("GroupManager::getGroupById-not able to find group {}", groupId);
            return nullptr;
        }
    }

    return pGroupInfo;
}

bool deviceInGroup(std::string bdfAddress, std::shared_ptr<GroupUnit> pGroupInfo) {
    std::vector<zes_pci_address_t> pcieTop;
    Topology::getPcieTopo(bdfAddress, pcieTop);
    return pGroupInfo->deviceInGroup(pcieTop);
}

void GroupManager::createBuildInGroup(bool bBuildInDevice, int vendorId, int deviceId, std::string devID, std::string bdfAddress) {
    GroupMap::iterator iterator;
    if (bBuildInDevice) {
        //std::unique_lock<std::mutex> lock(this->mutex);
        for (iterator = groupMap.begin(); iterator != groupMap.end(); iterator++) {
            std::shared_ptr<GroupUnit> pGroupInfo = iterator->second;
            if (pGroupInfo == nullptr) {
                continue;
            }
            if ((pGroupInfo->getId() & BUILD_IN_GROUP_MASK) != BUILD_IN_GROUP_MASK) {
                continue;
            }

            if (deviceInGroup(bdfAddress, pGroupInfo)) {
                pGroupInfo->addDevice(std::stoi(devID));
                return;
            }
        }
    }

    xpum_group_id_t groupId;
    if (XPUM_OK != createGroup("card-", &groupId, true)) {
        XPUM_LOG_DEBUG("GroupManager::createBuildInGroup error");
        return;
    }

    std::shared_ptr<GroupUnit> pInfo = getGroupById(groupId);
    if (pInfo == nullptr) {
        XPUM_LOG_DEBUG("GroupManager::createBuildInGroup error");
        return;
    }
    pInfo->addDevice(std::stoi(devID));

    if (bBuildInDevice) {
        std::vector<zes_pci_address_t> pcieTop;
        Topology::getPcieTopo(bdfAddress, pcieTop);

        pInfo->setPcieTopo(pcieTop);
    }
}

void GroupManager::createBuildInGroup() {
    std::vector<std::shared_ptr<Device>> devices;
    std::map<std::string, std::vector<zes_pci_address_t>> pcieMap;
    //xpum_group_id_t groupId;
    bool bBuildInGroup = false, bGroupDevice = false;
    if (p_devicemanager == nullptr) {
        return;
    }
    if (BUILD_IN_GROUP) {
        bBuildInGroup = true;
    }

    p_devicemanager->getDeviceList(devices);
    for (std::size_t i = 0; i < devices.size(); i++) {
        int vendorId = -1, deviceId = -1;
        std::string bdfAddress;
        auto& p_device = devices[i];
        std::vector<Property> properties;
        p_device->getProperties(properties);

        for (Property& prop : properties) {
            xpum_device_internal_property_name_t name = prop.getName();
            std::string value = prop.getValue();

            switch (name) {
                case XPUM_DEVICE_PROPERTY_INTERNAL_PCI_VENDOR_ID:
                    vendorId = std::stoi(value, nullptr, 16);
                    break;
                case XPUM_DEVICE_PROPERTY_INTERNAL_PCI_DEVICE_ID:
                    deviceId = std::stoi(value, nullptr, 16);
                    break;
                case XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS:
                    bdfAddress = value;
                    break;
                default:
                    break;
            }
        }

        if (vendorId == -1 || deviceId == -1 || bdfAddress.empty()) {
            XPUM_LOG_DEBUG("GroupManager::createBuildInGroup vendorId:{} deviceId:{} bdfAddress:{}.",
                           vendorId, deviceId, bdfAddress);
            continue;
        }

        const PcieDevice* pcie = PciDatabase::instance().getDevice(vendorId, deviceId);
        if (pcie != nullptr) {
            bGroupDevice = pcie->grouped;
        }
        if (bBuildInGroup || bGroupDevice) {
            createBuildInGroup(bGroupDevice, vendorId, deviceId, p_device->getId(), bdfAddress);
        }
    }
}

void GroupManager::init() {
    createBuildInGroup();
}

void GroupManager::close() {
}
} // end namespace xpum
