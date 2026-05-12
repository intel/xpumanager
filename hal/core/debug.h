/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef DEBUG_H
#define DEBUG_H

// ── C++ path: pull in the modular logger headers ──────────────────────────────
#ifdef __cplusplus
#include "logger/log_level.h"
#include "logger/log_record.h"
#include "logger/formatters.h"
#include "logger/sink_base.h"
#include "logger/ostream_sink.h"
#include "logger/filestream_sink.h"
#include "logger/logger.h"
#else
// ── C fallback — no Logger, no source_location ────────────────────────────────
#include <stdio.h>

/* Plain integer enum for C code; values match C++ LogLevel. */
enum
{
	NO_PRINT = -1,
	ERR,
	INFO,
	DBG,
	TRACE
};

extern int dbgLvl;

#define PRINT(fmt, ...)                                                                                                \
	do {                                                                                                               \
		printf(fmt, ##__VA_ARGS__);                                                                                    \
		fflush(stdout);                                                                                                \
	} while (0)
#define ERR(fmt, ...)                                                                                                  \
	do {                                                                                                               \
		if (dbgLvl >= ERR) {                                                                                           \
			fprintf(stderr, "[Error] ");                                                                               \
			fprintf(stderr, fmt, ##__VA_ARGS__);                                                                       \
			fflush(stderr);                                                                                            \
		}                                                                                                              \
	} while (0)
#define INFO(fmt, ...)                                                                                                 \
	do {                                                                                                               \
		if (dbgLvl >= INFO) {                                                                                          \
			fprintf(stderr, "[Info] ");                                                                                \
			fprintf(stderr, fmt, ##__VA_ARGS__);                                                                       \
			fflush(stderr);                                                                                            \
		}                                                                                                              \
	} while (0)
#define DBG(fmt, ...)                                                                                                  \
	do {                                                                                                               \
		if (dbgLvl >= DBG) {                                                                                           \
			fprintf(stderr, "[DBG] ");                                                                                 \
			fprintf(stderr, fmt, ##__VA_ARGS__);                                                                       \
			fflush(stderr);                                                                                            \
		}                                                                                                              \
	} while (0)
#define TRACE(fmt, ...)                                                                                                \
	do {                                                                                                               \
		if (dbgLvl >= TRACE) {                                                                                         \
			fprintf(stderr, fmt, ##__VA_ARGS__);                                                                       \
			fflush(stderr);                                                                                            \
		}                                                                                                              \
	} while (0)
#define TRACING()
#endif /* __cplusplus */

// ── Windows helpers ───────────────────────────────────────────────────────────
#ifdef _WIN32
static int is_windows = 1;
#else
static int is_windows __attribute__((unused)) = 0; // NOLINT(readability-identifier-naming)
#endif

// ── Level helpers — unchanged interface ──────────────────────────────────────
#ifdef __cplusplus
inline int setDbgLvl(LogLevel lvl)
{
	Logger::instance().setLevel(lvl);
	return 0;
}

[[nodiscard]] inline LogLevel getDbgLvl() { return Logger::instance().getLevel(); }
#else
inline int setDbgLvl(int lvl)
{
	int ret = 1;
	if (lvl >= NO_PRINT && lvl <= TRACE) {
		dbgLvl = lvl;
		ret = 0;
	}
	return ret;
}

inline int getDbgLvl() { return dbgLvl; }
#endif

// Enhanced debug level functions for cross-module synchronization
int setDbgLvlExtended(int lvl);
int getDbgLvlExtended();

#endif /* DEBUG_H */
