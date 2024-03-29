/*
 *  Copyright (C) 2022-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file helper.h
 */
#pragma once
#include <string>
#include <vector>

namespace xpum {

    bool isPathExist(const std::string &s);

    void readConfigFile();

    double calculateMean(const std::vector<double>& data);

    double calcaulateVariance(const std::vector<double>& data);

} // namespace xpum