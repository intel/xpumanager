/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef HAL_H
#define HAL_H

#include <zes_api.h>
#include <string_view>

namespace hal {

/// Translates a Level Zero result code to its symbolic name.
std::string_view resultToString(ze_result_t result);

} // namespace hal

#endif // HAL_H
