/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file utility.h
 */

#pragma once

#include <string>

namespace xpum::cli {

typedef enum linux_os_release_t {
    LINUX_OS_RELEASE_UBUNTU,
    LINUX_OS_RELEASE_CENTOS,
    LINUX_OS_RELEASE_SLES,
    LINUX_OS_RELEASE_RHEL,
    LINUX_OS_RELEASE_DEBIAN,
    LINUX_OS_RELEASE_UNKNOWN,
} linux_os_release_t;

bool isNumber(const std::string &str);

bool isInteger(const std::string &str);

bool isValidDeviceId(const std::string &str);

bool isValidTileId(const std::string &str);

bool isBDF(const std::string &str);

bool isShortBDF(const std::string &str);

std::string to_hex_string(uint64_t val, int width = 0);

std::string add_two_hex_string(std::string str1, std::string str2);

std::string toString(const std::vector<int> vec);

std::string trim(const std::string& str, const std::string& toRemove);

linux_os_release_t getOsRelease();

bool isFileExists(const char* path);

} // end namespace xpum::cli
