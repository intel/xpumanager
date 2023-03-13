/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file ipmi_interface.h
 */

#pragma once

#ifdef __linux__
#include <linux/ipmi.h>
#endif

#include <stdint.h>

#include "bsmc_interface.h"
