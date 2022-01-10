#include "pcie_manager.h"
#include "infrastructure/logger.h"
#include "infrastructure/exception/base_exception.h"
#include "infrastructure/configuration.h"

namespace xpum {

PCIeManager::PCIeManager() {
    XPUM_LOG_DEBUG("PCIeManager()");
}

PCIeManager::~PCIeManager() {
    // XPUM_LOG_DEBUG("~PCIeManager()");
}

void PCIeManager::init() {
    XPUM_LOG_DEBUG("start PCIeManager init");
    std::atomic<bool> initialized(false);
    auto pcie_thread = std::thread([this, &initialized]() {
        const std::string pcm_command{"/usr/local/bin/pcm-iio-gpu.x"};
        try {
            std::ifstream infile(pcm_command);
            if (!infile.is_open()) {
                initialized.store(true);
                XPUM_LOG_ERROR("pcm-iio-gpu.x not found");
                infile.close();
                return;
            }
            infile.close();
            std::array<char, 256> buffer;
            std::string result;
            FILE *pipe = popen(pcm_command.c_str(), "r");
            while (fgets(buffer.data(), 256, pipe) != nullptr) {
                initialized.store(true);
                result = buffer.data();
                std::stringstream sstream(result);
                std::vector<std::string> line;
                std::string item;
                while (getline(sstream, item, ',')) {
                    auto pos = item.find('=');
                    line.push_back(item.substr(pos + 1));
                }
                pcie_read_throughputs[line[1]] = std::stoull(line[2].c_str());
                pcie_write_throughputs[line[1]] = std::stoull(line[3].c_str());
            }
            pclose(pipe);
        } catch (std::exception& e) {
            interrupted = true;
            XPUM_LOG_ERROR("error occurred in pcm-iio-gpu.x: {}", e.what());
        }
    });
    pcie_thread.detach();
    while (!initialized.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    XPUM_LOG_DEBUG("PCIeManager init done");
}

void PCIeManager::close() {
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
}