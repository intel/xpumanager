/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Board serial number resolution: Sysman, AMC FRU data and the OEM serial number via IGSC.
 */

#pragma once

#include "ze_api.h"
#include <string>

struct devInfo;

/**
 * @brief Retrieves the OEM serial number via IGSC using the MEI device path.
 *
 * Opens the device at @p meiDevicePath, queries the OEM serial number and keeps
 * the printable ASCII prefix (up to the first null or non-printable byte).
 *
 * @param[in]  meiDevicePath MEI device node path used to open the IGSC device.
 * @param[out] serialNumber  Receives the sanitized OEM serial number; cleared on failure.
 *
 * @retval 0  Successfully retrieved a non-empty OEM serial number.
 * @retval -1 The path is empty, the device could not be opened, the query failed,
 *            or the resulting serial number is empty.
 */
int getOemSerialNumberByMeiPath(const std::string &meiDevicePath, std::string &serialNumber);

/**
 * @brief Resolves the board serial number of a device.
 *
 * Sysman is queried first. When it reports "unknown", the AMC FRU data is tried
 * (only if the device has an AMC), followed by the OEM serial number via IGSC.
 * Shared by discovery and the "serial" query/dump metric so that all commands
 * report the same value.
 *
 * @param[in]  d            Device info structure.
 * @param[out] serialNumber Receives the serial number, or "unknown" if no source provides one.
 *
 * @retval ZE_RESULT_SUCCESS Sysman device properties were read; @p serialNumber is populated.
 * @retval ZE_RESULT_ERROR_* Sysman device properties could not be read.
 */
ze_result_t querySerialNumber(devInfo &d, std::string &serialNumber);
