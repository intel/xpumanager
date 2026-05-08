/* 
 *  Copyright (C) 2023-2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file amcInBand.cpp
 */
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <sstream>
#include <regex>
#include <iomanip>
#include "amcInBand.h"
#include "logger.h"

namespace xpum {

uint64_t access_device_memory(std::string hex_base, uint64_t width) {
    int fd = -1;
    void *map_base, *virt_addr; 
    uint64_t read_result = 0;
    off_t target = strtoul(hex_base.c_str(), 0, 0);

    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
        return 0;
    }

    int map_size = 4096UL;
    int map_mask = map_size - 1;
    map_base = mmap(0, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, target & ~map_mask);
    
    virt_addr = (char *)map_base + (target & map_mask);
    switch (width) {
        case 8:
            read_result = *((uint8_t *) virt_addr);
            break;
        case 16:
            read_result = *((uint16_t *) virt_addr);
            break;
        case 32:
            read_result = *((uint32_t *) virt_addr);
            break;
        case 64:
            read_result = *((uint64_t *) virt_addr);
            break;
        default:
            read_result = *((uint32_t *) virt_addr);
    }
    
    if (munmap(map_base, map_size) == -1) {
        close(fd);
        return 0;
    }

    close(fd);

    return read_result;
}

bool getDeviceRegion(std::string bdf, std::string& region_base){
    std::string cmd = "lspci -vvv -s " + bdf + " | egrep \"size=[0-9]{1,2}M\" 2>/dev/null";
    FILE* f = popen(cmd.c_str(), "r");
    if (f == NULL) {
        return false;
    }
    char c_line[1024];
    while (fgets(c_line, 1024, f) != NULL) {
        std::string line(c_line);
        if (line.find("Region") == std::string::npos) {
            continue;
        }

        if (line.find("disabled") != std::string::npos) {
            std::string enable_commad = "setpci -s " + bdf + " COMMAND=0x02";
            if (!system(enable_commad.c_str())) {
                pclose(f);
                return false;
            }
        }

        std::regex reg("[0-9a-fA-F]{10,16}");
        std::smatch match;
        if(std::regex_search(line, match, reg)) {
            region_base = match.str();
        }
    }
    pclose(f);
    if (region_base.empty()) {
        return false;
    }
    region_base = "0x" + region_base;
    return true;
}

std::string to_hex_string(uint64_t val, int width) {
    std::stringstream s;
    if (width == 0)
        s << std::string("0x") << std::hex << val;
    else
        s << std::string("0x") << std::setfill('0') << std::setw(width) << std::hex << val;
    return s.str();
}

std::string add_two_hex_string(std::string str1, std::string str2) {
    uint64_t u1 = std::stoul(str1.c_str(), 0, 16);
    uint64_t u2 = std::stoul(str2.c_str(), 0, 16);
    return to_hex_string(u1 + u2);
}

bool getAMCFirmwareVersionInBand(std::string& amc_version, std::string bdf) {
    std::string region_base;
    if (!getDeviceRegion(bdf, region_base)){
        return false;
    }
    uint32_t amc_version_offset_beg = 0x281C24;
    uint32_t amc_version_offset_end = 0x281C34;
    uint32_t multiply_address = 0x4;
    std::vector<int> vec;
    for (uint32_t offset = amc_version_offset_beg; offset < amc_version_offset_end; offset += multiply_address){
        std::string temp = add_two_hex_string(region_base, to_hex_string(offset));
        uint32_t val = access_device_memory(temp);
        vec.push_back(val);
    }
    std::vector<int>::const_iterator it;
    std::stringstream s;
    for (std::vector<int>::iterator it = vec.begin() ; it != vec.end(); ++it) {
        if (it != vec.begin())
            s << ".";
        s << *it;
    }
    amc_version = s.str();
    XPUM_LOG_DEBUG("getAMCFirmwareVersionInBand amc_version:{}", amc_version);
    return true;
}


} // end namespace xpum
