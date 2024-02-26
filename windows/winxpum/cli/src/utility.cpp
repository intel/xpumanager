/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file utility.cpp
 */

#include <cstdio>
#include <string>
#include <regex>
#include <iomanip>

#include "utility.h"

namespace xpum::cli {

    bool isNumber(const std::string& str) {
        return str.find_first_not_of("0123456789") == std::string::npos;
    }

    bool isInteger(const std::string& str) {
        const std::string bdfFormat = "^-?\\d+$";
        return std::regex_match(str, std::regex(bdfFormat));
    }

    bool isValidDeviceId(const std::string& str) {
        if (!isNumber(str)) {
            return false;
        }

        int value;
        try {
            value = std::stoi(str);
        }
        catch (const std::out_of_range& oor) {
            return false;
        }
        if (value < 0) {
            return false;
        }

        return true;
    }

    bool isValidTileId(const std::string& str) {
        if (!isNumber(str)) {
            return false;
        }

        int value;
        try {
            value = std::stoi(str);
        }
        catch (const std::out_of_range& oor) {
            return false;
        }
        if (value < 0 || value > 1) {
            return false;
        }

        return true;
    }

    bool isBDF(const std::string& str) {
        const std::string bdfFormat = "[a-f0-9]{4}:[a-f0-9]{2}:[a-f0-9]{2}\\.[a-f0-9]{1}";
        return std::regex_match(str, std::regex(bdfFormat));
    }

    bool isShortBDF(const std::string& str) {
        const std::string bdfFormat = "[a-f0-9]{2}:[a-f0-9]{2}\\.[a-f0-9]{1}";
        return std::regex_match(str, std::regex(bdfFormat));
    }

    bool isATSMPlatform(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str.find("56c0") != std::string::npos || str.find("56c1") != std::string::npos || str.find("56c2") != std::string::npos;
    }

}// end namespace xpum::cli
