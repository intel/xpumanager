/* 
 *  Copyright (C) 2021-2025 Intel Corporation
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
                    std::string item;
                    std::map<std::string, std::string> line;
                    while (getline(sstream, item, ',')) {
                        auto pos = item.find('=');
                        line[item.substr(0, pos)] = item.substr(pos + 1);
                    }
                    // Note : sample interval is 100ms and unit is B/s
                    std::string bdf = line["bdf"];
                    auto read_value = std::stoull(line["IB read"].c_str());
                    pcie_read_throughputs[bdf] = read_value / 1000;
                    if (pcie_reads.find(bdf) == pcie_reads.end()) {
                        pcie_reads[bdf] = 0;
                    }
                    pcie_reads[bdf] += read_value * 0.1;

                    auto write_value = std::stoull(line["IB write"].c_str());
                    pcie_write_throughputs[bdf] = write_value / 1000;
                    if (pcie_writes.find(bdf) == pcie_writes.end()) {
                        pcie_writes[bdf] = 0;
                    }
                    pcie_writes[bdf] += write_value * 0.1;
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