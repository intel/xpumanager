/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file version.cpp
 */

#include "pch.h"
#include "version.h"

#include "../../resource.h"
#include "level_zero/loader/ze_loader.h"
#include "level_zero/ze_api.h"

#include <vector>s

namespace xpum {

    std::string Version::getVersion() {
        return VER_VERSION_MAJORMINORPATCH_STR;
    }

    std::string Version::getVersionGit() {
        std::string version_git = VER_COMMIT_VERSION;
        return version_git.substr(0, 8);
    }

    std::string Version::getZeLibVersion() {
        std::string lib_version = "Not Detected";
        size_t size = 0;
        ze_result_t res = zelLoaderGetVersions(&size, nullptr);
        if (res == ZE_RESULT_SUCCESS) {
            std::vector<zel_component_version_t> versions(size);
            res = zelLoaderGetVersions(&size, versions.data());
            if (res == ZE_RESULT_SUCCESS) {
                if (versions.size() > 0) {
                    std::string str = std::to_string(versions[0].component_lib_version.major) + "." + std::to_string(versions[0].component_lib_version.minor) + "." + std::to_string(versions[0].component_lib_version.patch);
                    lib_version = str;
                }
            }
        }
        return lib_version;
    }
} // end namespace xpum
