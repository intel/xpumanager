/* 
 *  Copyright (C) 2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file precheck.h
 */

#pragma once

#include "../include/xpum_structs.h"

namespace xpum {

xpum_result_t vgpuPrecheck(xpum_device_id_t deviceId, xpum_vgpu_precheck_result_t* result);

}
