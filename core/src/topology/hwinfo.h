/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file hwinfo.h
 */

#pragma once

#include <string>

#include "../include/xpum_structs.h"

namespace xpum {
/**
 * Class used to find the device path of a bdf-address 
 * 
 */
class HWInfo {
   public:
    static std::string getDevicePath(const std::string& bdf_address);
    static bool isPcieDevExist(xpum_device_id_t deviceId);
};
} // end namespace xpum
