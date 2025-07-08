/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file pcie_manager.h
 */

#pragma once

#include "level_zero/ze_api.h"
#include "level_zero/zes_api.h"

#include <sstream>
#include <string>
#include <thread>
#include <map>

namespace xpum {

class PCIeManager {
    public:

    uint64_t getLatestPCIeReadThroughput(const zes_device_handle_t& device);

    uint64_t getLatestPCIeWriteThroughput(const zes_device_handle_t& device);

    uint64_t getLatestPCIeRead(const zes_device_handle_t& device);

    uint64_t getLatestPCIeWrite(const zes_device_handle_t& device);

    private:

    std::map<zes_device_handle_t, zes_pci_stats_t> prevReadCounter;

    std::map<zes_device_handle_t, zes_pci_stats_t> prevWriteCounter;

    static const uint64_t BYTES_TO_KILOBYTES = 1024;

    static const uint64_t MICROSECONDS_TO_SECONDS = 1000000;
};
} // namespace xpum
