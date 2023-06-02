/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file init_close_interface.h
 */

#pragma once

namespace xpum {

    class InitCloseInterface {
    public:
        virtual ~InitCloseInterface() {}

        virtual void init() = 0;

        virtual void close() = 0;
    };

} // end namespace xpum
