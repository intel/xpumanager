/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file pci_address.h
 */

#pragma once

#include <stdint.h>
#include "..\ipmi\compiler_specific.h"

#pragma warning(disable: 4324)

#if defined(__cplusplus)
namespace xpum {
extern "C" {
#endif

#if (_MSC_VER < 1910)
typedef struct {
#else
typedef struct ALIGNAS(16) {
#endif
	uint8_t bus;
	uint8_t device;
	uint8_t function;
} pci_address_t;

#if defined(__cplusplus)
}
}
#endif