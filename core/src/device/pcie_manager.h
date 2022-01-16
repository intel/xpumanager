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

namespace xpum {

class PCIeManager : InitCloseInterface {
public:
    PCIeManager();

    virtual ~PCIeManager();

    void init() override;

    void close() override;

    uint64_t getLatestPCIeReadThroughput(std::string bdf);

    uint64_t getLatestPCIeWriteThroughput(std::string bdf);

private:
    std::map<std::string, uint64_t> pcie_read_throughputs;
    std::map<std::string, uint64_t> pcie_write_throughputs;
    bool interrupted;
    std::atomic<bool> initialized;
};
}