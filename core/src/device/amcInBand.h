/* 
 *  Copyright (C) 2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file amcInBand.h
 */

#include <string>
#include <cstdint>
namespace xpum {

std::string add_two_hex_string(std::string str1, std::string str2);

std::string to_hex_string(uint64_t val, int width = 0);

uint64_t access_device_memory(std::string hex_base, uint64_t width=32);

bool getDeviceRegion(std::string bdf, std::string& region_base);

bool getAMCFirmwareVersionInBand(std::string& amc_version, std::string bdf);

} // end namespace xpum
