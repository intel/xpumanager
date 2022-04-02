/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file logger.h
 */

#pragma once

#include <string>

#define XPUM_LOG_AUDIT(fmt, ...) xpum::cli::audit_log(fmt, __VA_ARGS__)

namespace xpum::cli {

void init_logger();
void audit_log(const char* fmt, ...);
} // end namespace xpum::cli
