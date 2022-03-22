/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file pcie_manager.h
 */

#pragma once

#include "infrastructure/init_close_interface.h"
#include <map>
#include <string>
#include <thread>
#include <array>
#include <iostream>
#include <exception>
#include <atomic>
#include <sstream>
#include <fstream>
#include <cstdlib>

namespace xpum {

class PCIeManager : InitCloseInterface {
public:
    PCIeManager();

    virtual ~PCIeManager();

    void init() override;

    void close() override;

    uint64_t getLatestPCIeReadThroughput(std::string bdf);

    uint64_t getLatestPCIeWriteThroughput(std::string bdf);

    uint64_t getLatestPCIeRead(std::string bdf);

    uint64_t getLatestPCIeWrite(std::string bdf);

private:
    std::map<std::string, uint64_t> pcie_read_throughputs;
    std::map<std::string, uint64_t> pcie_write_throughputs;
    std::map<std::string, uint64_t> pcie_reads;
    std::map<std::string, uint64_t> pcie_writes;
    std::atomic<bool> interrupted;
    std::atomic<bool> initialized;
    std::atomic<bool> stopped;
};
}