/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file group_unit.h
 */

#pragma once
#include <string>
#include <vector>

#include "../include/xpum_structs.h"
#include "control/device_manager_interface.h"

namespace xpum {

class GroupUnit : public std::enable_shared_from_this<GroupUnit> {
   public:
    GroupUnit(std::string groupname, xpum_group_id_t groupId);
    ~GroupUnit();

    xpum_group_id_t getId();
    xpum_device_type_t getDeviceType();
    unsigned int getDeviceCount();
    void getName(char groupname[XPUM_MAX_STR_LENGTH]);
    void getDeviceList(xpum_device_id_t device_List[XPUM_MAX_NUM_DEVICES]);

    xpum_result_t addDevice(xpum_device_id_t deviceId);

    xpum_result_t removeDevice(const std::shared_ptr<DeviceManagerInterface>& p_devicemanager,
                               xpum_group_id_t groupId, xpum_device_id_t deviceId);

    void setPcieTopo(std::vector<zes_pci_address_t>& pcieTop);
    bool deviceInGroup(std::vector<zes_pci_address_t>& pcieTop);

   private:
    xpum_group_id_t id;
    std::string name;
    std::vector<xpum_device_id_t> deviceList;
    std::vector<zes_pci_address_t> pcieTopology;
    std::size_t topoLevel;
};
} // end namespace xpum
