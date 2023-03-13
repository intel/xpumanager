/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file util.h
 */
#pragma once
#include <string>
#include <regex>

namespace xpum {

std::string getDmiDecodeOutput();

int doCmd(std::string cmd, std::string& output);

unsigned short toCidr(const char* ipAddress);

std::string search_by_regex(std::string content, std::regex pattern);

}