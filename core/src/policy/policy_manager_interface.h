/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file policy_manager_interface.h
 */

#pragma once

#include <string>
#include <vector>

#include "../include/xpum_structs.h"
#include "infrastructure/init_close_interface.h"

namespace xpum {

class PolicyManagerInterface : public InitCloseInterface {
   public:
    virtual ~PolicyManagerInterface() {}

    virtual xpum_result_t xpumSetPolicy(xpum_device_id_t deviceId, xpum_policy_t policy) = 0;
    virtual xpum_result_t xpumSetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t policy) = 0;
    virtual xpum_result_t xpumGetPolicy(xpum_device_id_t deviceId, xpum_policy_t resultList[], int *count) = 0;
    virtual xpum_result_t xpumGetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t resultList[], int *count) = 0;
    virtual void resetCheckFrequency() = 0;
};
} // end namespace xpum
