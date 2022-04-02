/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file ilegal_parameter_exception.h
 */

#include "base_exception.h"

namespace xpum {

class IlegalParameterException : public BaseException {
   public:
    IlegalParameterException(const std::string& msg) : BaseException(msg) {
    }

    IlegalParameterException(ErrorCode code, const std::string& msg) : BaseException(code, msg) {
    }
};
} // end namespace xpum
