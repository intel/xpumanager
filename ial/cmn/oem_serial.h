/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>

int getOemSerialNumberByMeiPath(const std::string &meiDevicePath, std::string &serialNumber);
