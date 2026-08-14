/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file logger.h
 */

#pragma once

#include "spdlog/cfg/env.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/bundled/core.h"

template<typename T, typename = std::enable_if<std::is_enum<T>::value, bool>>
auto format_as(T t) -> typename std::underlying_type<T>::type {
    return fmt::underlying(t);
}
namespace xpum {
    template<typename T, typename = std::enable_if<std::is_enum<T>::value, bool>>
    auto format_as(T t) -> typename std::underlying_type<T>::type {
        return fmt::underlying(t);
    }
}

#define XPUM_LOG_INFO(...) spdlog::info(__VA_ARGS__)
#define XPUM_LOG_WARN(...) spdlog::warn(__VA_ARGS__)
#define XPUM_LOG_ERROR(...) spdlog::error(__VA_ARGS__)
#define XPUM_LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)
#define XPUM_LOG_TRACE(...) spdlog::trace(__VA_ARGS__)
#define XPUM_LOG_FATAL(...) spdlog::critical(__VA_ARGS__)

// spdlog rethrows exceptions it does not recognise (see SPDLOG_LOGGER_CATCH in
// spdlog/logger.h), and formatting the arguments can allocate. Use these
// variants from inside a catch handler of a noexcept function, where a second
// exception would escape and terminate the process.
#define XPUM_LOG_ERROR_NOEXCEPT(...)     \
    do {                                 \
        try {                            \
            spdlog::error(__VA_ARGS__);  \
        } catch (...) {                  \
        }                                \
    } while (0)

#define XPUM_LOG_DEBUG_NOEXCEPT(...)     \
    do {                                 \
        try {                            \
            spdlog::debug(__VA_ARGS__);  \
        } catch (...) {                  \
        }                                \
    } while (0)

namespace xpum {

class Logger {
   public:
    static void init() {
        spdlog::cfg::load_env_levels();
    }
};
} // end namespace xpum
