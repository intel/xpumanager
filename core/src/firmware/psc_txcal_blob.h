/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file psc_txcal_blob.h
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace xpum {

std::vector<uint8_t> getPSCData(std::vector<uint8_t>& blob_data);

std::vector<uint8_t> getTxCalBlobByMeiDevice(const std::string& meiDeviceName);

std::string getTxCalDateByMeiDevice(const std::string& meiDeviceName);

std::string getMeiDeviceNameFromPath(const std::string& meiDevicePath);
} // namespace xpum