/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file pcie_manager.cpp
 */

#include "pcie_manager.h"

#include "infrastructure/configuration.h"
#include "infrastructure/exception/base_exception.h"
#include "infrastructure/logger.h"
#include "pcm-iio-gpu.h"

namespace xpum {

PCIeManager::PCIeManager() {
    this->initialized.store(false);
    this->interrupted.store(false);
    this->stopped.store(false);
    XPUM_LOG_DEBUG("PCIeManager()");
}

PCIeManager::~PCIeManager() {
    // XPUM_LOG_DEBUG("~PCIeManager()");
}

void PCIeManager::init() {
    XPUM_LOG_DEBUG("start PCIeManager init");
    if (std::system("modprobe msr") != 0) {
        XPUM_LOG_ERROR("Failed to load msr kernel module");
    }
    auto pcie_thread = std::thread([this]() {
        try {
            int res = pcm_iio_gpu_init();
            if (res != 0) {
                interrupted.store(true);
                stopped.store(true);
                XPUM_LOG_ERROR("Failed to init pcm-iio-gpu");
                return;
            }
            std::vector<std::string> datas = pcm_iio_gpu_query();
            while (!interrupted.load() && !datas.empty()) {
                for (auto data : datas) {
                    std::stringstream sstream(data);
                    std::vector<std::string> line;
                    std::string item;
                    while (getline(sstream, item, ',')) {
                        auto pos = item.find('=');
                        line.push_back(item.substr(pos + 1));
                    }
                    // Note : sample interval is 100ms and unit is B/s
                    auto read_value = std::stoull(line[2].c_str());
                    pcie_read_throughputs[line[1]] = read_value / 1000;
                    if (pcie_reads.find(line[1]) == pcie_reads.end()) {
                        pcie_reads[line[1]] = 0;
                    }
                    pcie_reads[line[1]] += read_value * 0.1;

                    auto write_value = std::stoull(line[3].c_str());
                    pcie_write_throughputs[line[1]] = write_value / 1000;
                    if (pcie_writes.find(line[1]) == pcie_writes.end()) {
                        pcie_writes[line[1]] = 0;
                    }
                    pcie_writes[line[1]] += write_value * 0.1;
                }
                if (!initialized.load())
                    initialized.store(true);
                datas = pcm_iio_gpu_query();
            }
        } catch (std::exception& e) {
            interrupted.store(true);
            XPUM_LOG_ERROR("error occurred in pcm-iio-gpu : {}", e.what());
        }
        stopped.store(true);
    });
    pcie_thread.detach();
    while (!stopped.load() && !interrupted.load() && !initialized.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    XPUM_LOG_DEBUG("PCIeManager init done");
}

void PCIeManager::close() {
    if (!initialized.load()) {
        return;
    }
    interrupted.store(true);
    while (!stopped.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    pcie_read_throughputs.clear();
    pcie_write_throughputs.clear();
}

uint64_t PCIeManager::getLatestPCIeReadThroughput(std::string bdf) {
    if (this->interrupted == true ||
        pcie_read_throughputs.find(bdf) == pcie_read_throughputs.end()) {
        throw BaseException("get PCIe read throughput error");
    }

    return pcie_read_throughputs[bdf];
}

uint64_t PCIeManager::getLatestPCIeWriteThroughput(std::string bdf) {
    if (this->interrupted == true ||
        pcie_write_throughputs.find(bdf) == pcie_write_throughputs.end()) {
        throw BaseException("get PCIe write throughput error");
    }

    return pcie_write_throughputs[bdf];
}

uint64_t PCIeManager::getLatestPCIeRead(std::string bdf) {
    if (this->interrupted == true ||
        pcie_reads.find(bdf) == pcie_reads.end()) {
        throw BaseException("get PCIe read error");
    }

    return pcie_reads[bdf];
}

uint64_t PCIeManager::getLatestPCIeWrite(std::string bdf) {
    if (this->interrupted == true ||
        pcie_writes.find(bdf) == pcie_writes.end()) {
        throw BaseException("get PCIe write error");
    }

    return pcie_writes[bdf];
}
} // namespace xpum