#pragma once

#include <syslog.h>

#define XPUM_LOG_AUDIT(fmt, ...) syslog(LOG_DEBUG, fmt, __VA_ARGS__)

namespace xpum::cli {

class Logger {
   public:
    static void init() {
        // LOG_CONS - Write directly to system console if there is an error while sending to system logger
        // LOG_PID - Include PID with each message
        // LOG_USER - Generic user-level messages
        openlog("xpumcli", LOG_CONS | LOG_PID, LOG_USER);
    }
};
} // end namespace xpum::cli
