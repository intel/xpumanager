/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file detect_usb_interface.h
 */

#pragma once

#include <string>

std::string getUsbInterfaceName(std::string idVendor, std::string idProduct);