/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file util.cpp
 */

#include "util.h"

namespace xpum {

int doCmd(std::string cmd, std::string& output) {
    char buffer[1024];
    std::string result;
    int ret = -1;

    cmd += " 2>&1";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe != nullptr) {
        try {
            std::size_t bytesread;
            while ((bytesread = std::fread(buffer, 1, 1024, pipe)) != 0) {
                result += std::string(buffer, bytesread);
            }
        } catch (...) {
            pclose(pipe);
        }
        ret = pclose(pipe);
    }
    output = result;
    return ret;
}

std::string getDmiDecodeOutput() {
    // dmidecode -t 42
    std::string output;
    doCmd("dmidecode -t42", output);
    return output;
}

unsigned short toCidr(const char* ipAddress) {
    unsigned short netmask_cidr;
    int ipbytes[4];

    netmask_cidr = 0;
    sscanf(ipAddress, "%d.%d.%d.%d", &ipbytes[0], &ipbytes[1], &ipbytes[2], &ipbytes[3]);

    for (int i = 0; i < 4; i++) {
        switch (ipbytes[i]) {
            case 0x80:
                netmask_cidr += 1;
                break;

            case 0xC0:
                netmask_cidr += 2;
                break;

            case 0xE0:
                netmask_cidr += 3;
                break;

            case 0xF0:
                netmask_cidr += 4;
                break;

            case 0xF8:
                netmask_cidr += 5;
                break;

            case 0xFC:
                netmask_cidr += 6;
                break;

            case 0xFE:
                netmask_cidr += 7;
                break;

            case 0xFF:
                netmask_cidr += 8;
                break;

            default:
                return netmask_cidr;
                break;
        }
    }

    return netmask_cidr;
}

} // namespace xpum
