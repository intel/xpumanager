/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file sensor_reading.h
 */

#pragma once

#include "sdr.h"
#include "tool.h"

#if defined(__cplusplus)
namespace xpum {
extern "C" {
#endif

int get_sdr_list(nrv_card& card);

#if defined(__cplusplus)
}
}
#endif