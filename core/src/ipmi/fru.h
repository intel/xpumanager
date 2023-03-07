/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file fru.h
 */

#pragma once

#include "tool.h"
#include <string>

namespace xpum {

int get_sn_number(uint8_t riser, uint8_t slot, std::string &sn_number);
}