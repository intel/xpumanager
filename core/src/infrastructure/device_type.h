/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device_type.h
 */

#pragma once

namespace xpum {

enum class DeviceType {
    GPU = 1,
    CPU = 2,
    FPGA = 3,
    MCA = 4
};

} // end namespace xpum