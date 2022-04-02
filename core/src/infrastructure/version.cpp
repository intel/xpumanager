/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file version.cpp
 */

#include "version.h"

#include <link.h>
#include <unistd.h>

#include "xpum_config.h"

namespace xpum {

std::string Version::getVersion() {
    return std::string(XPUM_VERSION);
}

std::string Version::getVersionGit() {
    return std::string(XPUM_VERSION_GIT);
}

std::string Version::getZeLibVersion() {
    static std::string zeLibVersion;

    dl_iterate_phdr([](struct dl_phdr_info *info, std::size_t size, void *data) -> int {
        auto zeLibVersion = (std::string *)data;

        if (zeLibVersion->length()) {
            // zeLibVersion has been set
            return 0;
        }

        constexpr ssize_t bufferSize = 100;
        char buffer[bufferSize];

        std::string libName = info->dlpi_name;
        std::size_t pos;
        if ((pos = libName.find("/libze_loader.so")) == std::string::npos) {
            return 0;
        }
        ssize_t nchars = readlink(info->dlpi_name, buffer, bufferSize);
        if (nchars > 0 && nchars < bufferSize) {
            auto realname = std::string(buffer, nchars);
            if ((pos = realname.find("so.")) != std::string::npos) {
                *zeLibVersion = realname.substr(pos + 3);
            }
        }
        return 0;
    },
                    &zeLibVersion);

    return zeLibVersion;
}
} // end namespace xpum
