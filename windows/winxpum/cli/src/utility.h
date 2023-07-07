/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file utility.h
 */

#pragma once

#include <string>

namespace xpum::cli {

	bool isNumber(const std::string& str);

	bool isInteger(const std::string& str);

	bool isValidDeviceId(const std::string& str);

	bool isValidTileId(const std::string& str);

	bool isBDF(const std::string& str);

	bool isShortBDF(const std::string& str);

	bool isATSMPlatform(std::string str);

} // end namespace xpum::cli
