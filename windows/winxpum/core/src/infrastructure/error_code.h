/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file error_code.h
 */

#pragma once
namespace xpum {
    enum ErrorCode {
        OK = 0,

        UNKNOWN = 0x00000001,

        ILEGAL_PARAM = 0x00000002,

        CORE_NOT_INITIALIZED = 0x00000003,

        OPERATION_FAILED = 0x00000004
    };
} // end namespace xpum