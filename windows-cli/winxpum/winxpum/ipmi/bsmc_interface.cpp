/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file bsmc_interface.cpp
 */

#ifdef __linux__
#include <linux/ipmi.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include "bsmc_interface.h"
#include "tool.h"

namespace xpum {
bsmc_hal_t *bsmc_hal;
extern bsmc_hal_t ipmi_hal;

int bsmc_interface_init(bsmc_interface_t iface) {
    switch (iface) {
        case IPMI:
            bsmc_hal = &ipmi_hal;
            break;

        default:
            bsmc_hal = &ipmi_hal;
    }
    return bsmc_hal->init();
}
} // namespace xpum
