/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file logger.h
 */

#pragma once

#include "spdlog/cfg/env.h"
#include "spdlog/spdlog.h"

#define XPUM_LOG_INFO(...) spdlog::info(__VA_ARGS__)
#define XPUM_LOG_WARN(...) spdlog::warn(__VA_ARGS__)
#define XPUM_LOG_ERROR(...) spdlog::error(__VA_ARGS__)
#define XPUM_LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)
#define XPUM_LOG_TRACE(...) spdlog::trace(__VA_ARGS__)
#define XPUM_LOG_FATAL(...) spdlog::critical(__VA_ARGS__)

namespace xpum {

class Logger {
   public:
    static void init() {
        spdlog::cfg::load_env_levels();
    }
};
} // end namespace xpum
