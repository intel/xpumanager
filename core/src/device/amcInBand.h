/* 
 *  Copyright (C) 2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file amcInBand.h
 */

#include <string>
namespace xpum {

uint32_t access_device_memory(std::string hex_base);

bool getDeviceRegion(std::string bdf, std::string& region_base);

bool getAMCFirmwareVersionInBand(std::string& amc_version, std::string bdf);

} // end namespace xpum