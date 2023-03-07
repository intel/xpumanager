/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file level_zero_initialization_exception.h
 */

#include "base_exception.h"

namespace xpum {

class LevelZeroInitializationException : public BaseException {
   public:
    LevelZeroInitializationException(const std::string& msg) : BaseException(msg) {
    }

    LevelZeroInitializationException(ErrorCode code, const std::string& msg) : BaseException(code, msg) {
    }
};
} // end namespace xpum
