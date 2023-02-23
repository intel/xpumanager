/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file ipmi_mock.cpp
 */
#include "ipmi_mock.h"

#include "infrastructure/logger.h"

#include <chrono>
#include <thread>
#include <vector>

namespace xpum {
int cmd_firmware_mock(const char* file, unsigned int versions[4]) {
    XPUM_LOG_INFO("Faking update amc fw...");
    std::this_thread::sleep_for(std::chrono::milliseconds(20000));
    return 0;
}

std::vector<std::string> cmd_get_amc_firmware_versions_mock() {
    return {"0.0.0.0"};
}
} // namespace xpum