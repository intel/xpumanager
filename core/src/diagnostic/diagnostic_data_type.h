/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file diagnostic_data_type.h
 */

#pragma once
#include "../include/xpum_structs.h"

namespace xpum {

struct ZeWorkGroups {
    uint32_t group_count_x; // number of thread groups in X dimension
    uint32_t group_count_y; // number of thread groups in Y dimension
    uint32_t group_count_z; // number of thread groups in Z dimension
    uint32_t group_size_x;
    uint32_t group_size_y;
    uint32_t group_size_z;
};

} // end namespace xpum
