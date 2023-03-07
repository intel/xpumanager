/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file ilegal_state_exception.h
 */

#include "base_exception.h"

namespace xpum {

class IlegalStateException : public BaseException {
   public:
    IlegalStateException(const std::string& msg) : BaseException(msg) {
    }

    IlegalStateException(ErrorCode code, const std::string& msg) : BaseException(code, msg) {
    }
};
} // end namespace xpum
