/*
 *  Copyright (C) 2022-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file helper.h
 */
#pragma once
#include <string>

namespace xpum {

    bool isPathExist(const std::string &s);

    void readConfigFile();

} // namespace xpum