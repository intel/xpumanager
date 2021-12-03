#pragma once
#include <string>

#include "spdlog/cfg/env.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

namespace xpum::daemon {

#define XPUM_LOG_INFO(...) spdlog::info(__VA_ARGS__)
#define XPUM_LOG_WARN(...) spdlog::warn(__VA_ARGS__)
#define XPUM_LOG_ERROR(...) spdlog::error(__VA_ARGS__)
#define XPUM_LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)
#define XPUM_LOG_TRACE(...) spdlog::trace(__VA_ARGS__)

class Logger {
   public:
    static void init(const char* log_file_name = nullptr, std::size_t max_size = 10 * 1024 * 1024, std::size_t max_files = 3, const std::string log_level = "info") {
        std::vector<spdlog::sink_ptr> sinks;
        sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        if (log_file_name != nullptr) {
            sinks.emplace_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_file_name, max_size, max_files, false));
        }
        auto logger = std::make_shared<spdlog::logger>("daemon", begin(sinks), end(sinks));
        logger->flush_on(spdlog::level::warn);
        spdlog::flush_every(std::chrono::seconds(3));
        spdlog::set_default_logger(logger);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [%P-%t] %v");
        spdlog::cfg::load_env_levels();
    }

    static void flush(){
        spdlog::default_logger()->flush();
    }
    
    static void close() {
        spdlog::shutdown();
    }
};
} // namespace xpum::daemon