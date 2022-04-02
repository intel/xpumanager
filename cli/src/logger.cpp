/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file logger.cpp
 */

#include <stdarg.h>
#include <syslog.h>
#include <unistd.h>

#include <cstdio>
#include <string>

namespace xpum::cli {

void init_logger() {
    openlog("xpumcli_audit", LOG_CONS | LOG_PID, LOG_USER);
}

void audit_log(const char* fmt, ...) {
    // get username and uid
    char username[64];
    getlogin_r(username, 64);
    uid_t uid = getuid();
    // create format string based on the incoming fmt to add username & uid
    const char* fmt_fmt = "[%s:%d] %s";
    char fmt_[1 + snprintf(NULL, 0, fmt_fmt, username, uid, fmt)];
    snprintf(fmt_, sizeof(fmt_), fmt_fmt, username, uid, fmt);
    // forward format and args to syslog()
    va_list args1;
    va_start(args1, fmt);
    va_list args2;
    va_copy(args2, args1);
    char buf[1 + vsnprintf(NULL, 0, fmt_, args1)];
    va_end(args1);
    vsnprintf(buf, sizeof(buf), fmt_, args2);
    va_end(args2);
    syslog(LOG_INFO, "%s", buf);
}
} // end namespace xpum::cli
