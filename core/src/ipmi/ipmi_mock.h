/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file ipmi_mock.h
 */
#include <vector>
#include <string>

namespace xpum {
int cmd_firmware_mock(const char* file, unsigned int versions[4]);

std::vector<std::string> cmd_get_amc_firmware_versions_mock();
} // namespace xpum