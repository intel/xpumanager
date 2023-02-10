/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file pci_address.h
 */

#pragma once

#include <stdint.h>
namespace xpum {

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
} pci_address_t;
} // namespace xpum
