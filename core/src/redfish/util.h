/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file util.h
 */
#pragma once
#include <string>

namespace xpum {

std::string getDmiDecodeOutput();

int doCmd(std::string cmd, std::string& output);

unsigned short toCidr(const char* ipAddress);

}