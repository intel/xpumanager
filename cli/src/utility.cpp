/*
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file utility.cpp
 */

#include <stdarg.h>
#include <syslog.h>
#include <unistd.h>
#include <termios.h>

#include <cstdio>
#include <string>
#include <regex>
#include <sstream>
#include <iomanip>
#include <fstream>

#include "utility.h"

namespace xpum::cli {

bool isNumber(const std::string &str) {
    return str.size() && str.find_first_not_of("0123456789") == std::string::npos;
}

bool isInteger(const std::string &str) {
    const std::string bdfFormat = "^-?\\d+$";
    return std::regex_match(str, std::regex(bdfFormat));
}

bool isValidDeviceId(const std::string &str) {
    if (!isNumber(str)) {
        return false;
    }

    int value;
    try {
        value = std::stoi(str);
    } catch (const std::out_of_range &oor) {
        return false;
    }
    if (value < 0) {
        return false;
    }

    return true;
}

bool isValidTileId(const std::string &str) {
    if (!isNumber(str)) {
        return false;
    }

    int value;
    try {
        value = std::stoi(str);
    } catch (const std::out_of_range &oor) {
        return false;
    }
    if (value < 0 || value > 1) {
        return false;
    }

    return true;
}

bool isBDF(const std::string &str) {
    const std::string bdfFormat = "[a-f0-9]{4}:[a-f0-9]{2}:[a-f0-9]{2}\\.[a-f0-9]{1}";
    return std::regex_match(str, std::regex(bdfFormat));
}

bool isShortBDF(const std::string &str) {
    const std::string bdfFormat = "[a-f0-9]{2}:[a-f0-9]{2}\\.[a-f0-9]{1}";
    return std::regex_match(str, std::regex(bdfFormat));
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

std::string toString(const std::vector<int> vec) {
    if (vec.empty()) {
        return "";
    }
    std::stringstream ss;
    for (size_t i = 0; i < vec.size(); i++) {
        if (i != 0) {
            ss << ", ";
        }
        ss << vec[i];
    }
    return ss.str();
}

std::string trim(const std::string& str, const std::string& toRemove) {
    const auto strBegin = str.find_first_not_of(toRemove);
    if (strBegin == std::string::npos)
        return "";

    const auto strEnd = str.find_last_not_of(toRemove);
    const auto strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}

linux_os_release_t getOsRelease() {
    /*
     *  Refer https://www.linux.org/docs/man5/os-release.html
     */
    std::ifstream ifs("/etc/os-release");
    if (!ifs.is_open()) {
        return LINUX_OS_RELEASE_UNKNOWN;
    }
    std::string line;
    while (std::getline(ifs, line)) {
        line = trim(line, " \t");
        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value) && key.compare("ID") == 0) {
            if (value.find("ubuntu") != std::string::npos) {
                return LINUX_OS_RELEASE_UBUNTU;
            } else if (value.find("centos") != std::string::npos) {
                return LINUX_OS_RELEASE_CENTOS;
            } else if (value.find("sles") != std::string::npos) {
                return LINUX_OS_RELEASE_SLES;
            } else if (value.find("rhel") != std::string::npos) {
                return LINUX_OS_RELEASE_RHEL;
            } else if (value.find("debian") != std::string::npos) {
                return LINUX_OS_RELEASE_DEBIAN;
            } else if (value.find("openEuler") != std::string::npos) {
                return LINUX_OS_RELEASE_OPEN_EULER;
            } else {
                return LINUX_OS_RELEASE_UNKNOWN;
            }
        }
    }
    return LINUX_OS_RELEASE_UNKNOWN;
}

bool isFileExists(const char* path) {
    std::ifstream ifs(path);
    return ifs.good();
}

bool isXeDevice() {
    return isFileExists("/sys/module/xe/srcversion");
}

std::string roundDouble(double r, int precision) {
    std::stringstream buffer;
    buffer << std::fixed << std::setprecision(precision) << r;
    return buffer.str();
}

std::string getKeyNumberValue(std::string key, const nlohmann::json &item) {
    auto sub = item.find(key);
    if (sub != item.end()) {
        return std::to_string((uint32_t)sub.value());
    }
    return "";
}

std::string getKeyStringValue(std::string key, const nlohmann::json &item) {
    auto sub = item.find(key);
    if (sub != item.end()) {
        return sub.value();
    }
    return "";
}

int getChar() {
        char ch = 0;
        struct termios oldTermios = {0};
        if (tcgetattr(0, &oldTermios) < 0){
            perror("error in tcgetattr()");
            return -1;
        }
        oldTermios.c_lflag &= ~ICANON;
        oldTermios.c_lflag &= ~ECHO;
        oldTermios.c_cc[VMIN] = 1;
        oldTermios.c_cc[VTIME] = 0;
        if (tcsetattr(0, TCSANOW, &oldTermios) < 0){
            perror("error in tcsetattr()");
            return -1;
        }

        if (read(0, &ch, 1) < 0){
            perror ("error in read()");
        }
        oldTermios.c_lflag |= ICANON;
        oldTermios.c_lflag |= ECHO;
        if (tcsetattr(0, TCSADRAIN, &oldTermios) < 0){
            perror ("error in tcsetattr()");
        }
        return (ch);
}

}// end namespace xpum::cli
