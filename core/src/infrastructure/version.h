/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file version.h
 */

#pragma once

#include <string>

namespace xpum {

class Version {
   public:
    static std::string getVersion();
    static std::string getVersionGit();
    static std::string getZeLibVersion();
};
} // end namespace xpum
