/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file const.h
 */

#pragma once

#include <functional>
#include <memory>

#include "infrastructure/exception/base_exception.h"

namespace xpum {

typedef long long Timestamp_t;

// template <typename T, typename... Args>
// std::unique_ptr<T> make_unique(Args&&... args) {
//     return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
// }

} // end namespace xpum
