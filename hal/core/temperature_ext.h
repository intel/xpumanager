/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef TEMPERATURE_EXT_H
#define TEMPERATURE_EXT_H

#include <zes_api.h>
#include <ze_api.h>

// Fallback for level-zero 1.32.0 (spec v1.17), which omits this enum value.
#if !defined(ZE_API_VERSION_CURRENT_M) || ZE_API_VERSION_CURRENT_M < ZE_MAKE_VERSION(1, 18)
#define ZES_TEMP_SENSORS_COMPOSITE static_cast<zes_temp_sensors_t>(9)
#endif

#endif // TEMPERATURE_EXT_H
