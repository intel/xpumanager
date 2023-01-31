/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file file_util.h
 */

#pragma once

#include <stdint.h>

namespace xpum {
uint8_t *read_file(const char *path, size_t *read_size);
bool write_file(const char *path, const uint8_t *buffer, size_t buffer_size);
bool compare_with_file(const char *path, const uint8_t *buffer, size_t buffer_size);
} // namespace xpum
