/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file group_manager_interface.h
 */

#pragma once

#include "../include/xpum_structs.h"
#include "infrastructure/init_close_interface.h"

namespace xpum {

class GroupManagerInterface : public InitCloseInterface {
   public:
    virtual ~GroupManagerInterface() {}

    virtual xpum_result_t createGroup(const char *pGroupName, xpum_group_id_t *pGroupId, bool buildIn = false) = 0;

    virtual xpum_result_t destroyGroup(xpum_group_id_t groupId) = 0;

    virtual xpum_result_t addDeviceToGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId) = 0;

    virtual xpum_result_t removeDeviceFromGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId) = 0;

    virtual xpum_result_t getGroupInfo(xpum_group_id_t groupId, xpum_group_info_t *pGroupInfo) = 0;

    virtual xpum_result_t getAllGroupIds(xpum_group_id_t groupIds[XPUM_MAX_NUM_GROUPS], int *count) = 0;
};
} // end namespace xpum
