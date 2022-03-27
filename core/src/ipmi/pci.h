/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file pci.h
 */

#pragma once

#include "pci_address.h"

namespace xpum {

#if defined(_MSC_VER)
#define PCI_DEVFN(slot, func) ((((slot)&0x1f) << 3) | ((func)&0x07))
#define PCI_SLOT(devfn) (((devfn) >> 3) & 0x1f)
#define PCI_FUNC(devfn) ((devfn)&0x07)
#endif

#define BDF_FORMAT "0000:%02x:%02x.%1x"

bool check_pci_device(const pci_address_t *address);
int reset_pci_device(const pci_address_t *address);
int get_pci_device_list(pci_address_t *list, int list_size, int *out_count);
int get_pci_device_by_bar0_address(uint32_t bar0_address, pci_address_t *pci_address);
} // namespace xpum
