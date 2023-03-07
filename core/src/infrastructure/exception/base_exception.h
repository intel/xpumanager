/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file base_exception.h
 */

#pragma once

#include <exception>
#include <stdexcept>
#include <string>

#include "infrastructure/error_code.h"

namespace xpum {

class BaseException : public std::exception {
   public:
    BaseException(const std::string& msg) : error_code(ErrorCode::OK) {
        this->msg = msg;
    }

    BaseException(ErrorCode code, const std::string& msg) : error_code(ErrorCode::OK) {
        this->error_code = code;
        this->msg = msg;
    }

    BaseException(std::exception& e) : error_code(ErrorCode::OK) {
        this->msg = e.what();
    }

    BaseException(ErrorCode code, std::exception& e) : error_code(ErrorCode::OK) {
        this->error_code = code;
        this->msg = e.what();
    }

    BaseException(const BaseException& e) {
        this->msg = e.msg;
    }

    ~BaseException() {
    }

   public:
    ErrorCode getErrorCode() {
        return this->error_code;
    }

    const char* what() const noexcept override {
        return this->msg.c_str();
    }

   private:
    ErrorCode error_code;

    std::string msg;
};

} // end namespace xpum
