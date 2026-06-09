/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef LOGGER_LOGGER_H
#define LOGGER_LOGGER_H

#include "log_level.h"
#include "ostream_sink.h"
#include "sink_base.h"

#include <atomic>
#include <cstdio>
#include <exception>
#include <format>
#include <iostream>
#include <memory>
#include <source_location>
#include <string_view>
#include <utility>

namespace detail {
static constexpr bool DEBUG_ENABLED = true;
} // namespace detail

/**
 * @brief Singleton logger.  Routes diagnostic output through the active Sink.
 *
 * ERR/INFO/DBG/TRACE go through the sink (defaults to stderr).
 * PRINT() goes through a separate printSink (defaults to stdout).
 *
 * Sink selection (defaults to OStreamSink(std::cerr)):
 * @code
 *     Logger::instance().setSink(
 *         std::make_shared<FileStreamSink>("/var/log/xpu-smi.log"));
 *     Logger::instance().setSink(
 *         std::make_shared<OStreamSink>(std::cout));
 * @endcode
 */
class Logger final
{
	std::atomic<std::shared_ptr<Sink>> sink;
	std::atomic<std::shared_ptr<Sink>> printSink;

	/// Current log level — relaxed ordering is sufficient: a momentarily
	/// stale read can suppress or emit at most one extra line.
	std::atomic<LogLevel> level{LogLevel::INFO};

	/// Active error callback; nullptr → write to stderr.
	std::atomic<std::shared_ptr<LogErrorCallback>> onError;

	/// Policy for std::vformat errors; default is Report.
	std::atomic<FormatErrorPolicy> fmtPolicy{FormatErrorPolicy::Report};

	// OStreamSink borrows std::cerr by reference; std::cerr outlives the
	// singleton because <iostream> guarantees ios_base::Init construction.
	// Diagnostic output (ERR/INFO/DBG/TRACE) goes to stderr so it doesn't
	// corrupt stdout which carries user-facing data (JSON, CSV, tables).
	// PRINT() uses a separate stdout-backed sink.
	Logger() : sink(std::make_shared<OStreamSink>(std::cerr)), printSink(std::make_shared<OStreamSink>(std::cout)) {}

	void invokeOnError(std::string_view what) noexcept
	{
		auto cb = onError.load(std::memory_order_relaxed);
		if (cb && *cb) {
			try {
				(*cb)(what);
			} catch (...) {
			}
		} else {
			std::fprintf(stderr, "[xpu-smi logger error] %.*s\n", static_cast<int>(what.size()), what.data());
		}
	}

	void handleFormatError(std::string_view what, LogLevel lvl, const char *prefix, std::source_location loc) noexcept
	{
		switch (fmtPolicy.load(std::memory_order_relaxed)) {
		case FormatErrorPolicy::Report:
			invokeOnError(what);
			break;
		case FormatErrorPolicy::Substitute:
			try {
				sink.load(std::memory_order_acquire)->log(lvl, loc, prefix, std::format("[format error: {}]", what));
			} catch (...) {
			}
			break;
		case FormatErrorPolicy::Ignore:
			break;
		case FormatErrorPolicy::Terminate:
			std::terminate();
		}
	}

public:
	static Logger &instance() noexcept
	{
		static Logger inst;
		return inst;
	}

	Logger(const Logger &) = delete;
	Logger &operator=(const Logger &) = delete;

	/// Atomically replace the active sink; nullptr resets to OStreamSink(cerr).
	void setSink(std::shared_ptr<Sink> s) noexcept
	{
		if (s) {
			printSink.store(s);
			sink.store(std::move(s));
		} else {
			sink.store(std::make_shared<OStreamSink>(std::cerr));
			printSink.store(std::make_shared<OStreamSink>(std::cout));
		}
	}

	std::shared_ptr<Sink> getSink() noexcept { return sink.load(std::memory_order_acquire); }

	std::shared_ptr<Sink> getPrintSink() noexcept { return printSink.load(std::memory_order_acquire); }

	/// Install an error callback; pass LogErrorCallback{} to silence errors.
	/// @note Must not call Logger — re-entrant use is undefined.
	void setOnError(LogErrorCallback cb)
	{
		onError.store(std::make_shared<LogErrorCallback>(std::move(cb)), std::memory_order_relaxed);
	}

	void setLevel(LogLevel lvl) noexcept { level.store(lvl, std::memory_order_relaxed); }

	[[nodiscard]] LogLevel getLevel() const noexcept { return level.load(std::memory_order_relaxed); }

	[[nodiscard]] bool isEnabled([[maybe_unused]] LogLevel lvl) const noexcept
	{
		if constexpr (!detail::DEBUG_ENABLED) {
			return false;
		}
		return level.load(std::memory_order_relaxed) >= lvl;
	}

	void setFormatErrorPolicy(FormatErrorPolicy p) noexcept { fmtPolicy.store(p, std::memory_order_relaxed); }

	[[nodiscard]] FormatErrorPolicy getFormatErrorPolicy() const noexcept
	{
		return fmtPolicy.load(std::memory_order_relaxed);
	}

	/// PRINT() output goes to stdout via the printSink.  When a custom sink
	/// is installed (via setSink), PRINT routes through it too — this allows
	/// test capture and file redirection while defaulting to stdout.
	template <typename... Args> void print(std::format_string<Args...> fmt, Args... args) noexcept
	{
		try {
			printSink.load(std::memory_order_acquire)
				->log(LogLevel::TRACE, {}, "", std::vformat(fmt.get(), std::make_format_args(args...)));
		} catch (const std::exception &e) {
			handleFormatError(e.what(), LogLevel::TRACE, "", {});
		} catch (...) {
			handleFormatError("unknown format error", LogLevel::TRACE, "", {});
		}
	}

	template <typename... Args>
	void write(LogLevel lvl, const char *prefix, std::source_location loc, std::format_string<Args...> fmt,
			   Args... args) noexcept
	{
		if (!isEnabled(lvl)) {
			return;
		}
		try {
			sink.load(std::memory_order_acquire)
				->log(lvl, loc, prefix, std::vformat(fmt.get(), std::make_format_args(args...)));
		} catch (const std::exception &e) {
			handleFormatError(e.what(), lvl, prefix, loc);
		} catch (...) {
			handleFormatError("unknown format error", lvl, prefix, loc);
		}
	}
};

// ── Logging API ───────────────────────────────────────────────────────────────
//
// Uses CTAD structs rather than macros so that source_location is captured as
// a trailing default argument in the constructor (function templates cannot
// have a default after a parameter pack).
//
//   ERR("failed: {}", result);   // identical call site to old macros

// NOLINTBEGIN(readability-identifier-naming) — intentional macro-name style

template <typename... Args> struct PRINT
{
	explicit PRINT(std::format_string<Args...> fmt, Args... args) noexcept
	{
		Logger::instance().print(fmt, std::move(args)...);
	}
};
template <typename... Args> PRINT(std::format_string<Args...>, Args...) -> PRINT<Args...>;

template <typename... Args> struct ERR // NOLINT(readability-identifier-naming)
{
	explicit ERR(std::format_string<Args...> fmt, Args... args,
				 std::source_location loc = std::source_location::current()) noexcept
	{
		if (!Logger::instance().isEnabled(LogLevel::ERR)) {
			return;
		}
		Logger::instance().write(LogLevel::ERR, "[Error] ", loc, fmt, std::move(args)...);
	}
};
template <typename... Args> ERR(std::format_string<Args...>, Args...) -> ERR<Args...>;

template <typename... Args> struct INFO // NOLINT(readability-identifier-naming)
{
	explicit INFO(std::format_string<Args...> fmt, Args... args,
				  std::source_location loc = std::source_location::current()) noexcept
	{
		if (!Logger::instance().isEnabled(LogLevel::INFO)) {
			return;
		}
		Logger::instance().write(LogLevel::INFO, "[Info] ", loc, fmt, std::move(args)...);
	}
};
template <typename... Args> INFO(std::format_string<Args...>, Args...) -> INFO<Args...>;

template <typename... Args> struct DBG // NOLINT(readability-identifier-naming)
{
	explicit DBG(std::format_string<Args...> fmt, Args... args,
				 std::source_location loc = std::source_location::current()) noexcept
	{
		if (!Logger::instance().isEnabled(LogLevel::DBG)) {
			return;
		}
		Logger::instance().write(LogLevel::DBG, "[DBG] ", loc, fmt, std::move(args)...);
	}
};
template <typename... Args> DBG(std::format_string<Args...>, Args...) -> DBG<Args...>;

template <typename... Args> struct TRACE // NOLINT(readability-identifier-naming)
{
	explicit TRACE(std::format_string<Args...> fmt, Args... args,
				   std::source_location loc = std::source_location::current()) noexcept
	{
		if (!Logger::instance().isEnabled(LogLevel::TRACE)) {
			return;
		}
		Logger::instance().write(LogLevel::TRACE, "", loc, fmt, std::move(args)...);
	}
};
template <typename... Args> TRACE(std::format_string<Args...>, Args...) -> TRACE<Args...>;

// NOLINTEND(readability-identifier-naming)

// ── Tracer / TRACING() ────────────────────────────────────────────────────────

/** Instantiate in any function body to log entry and exit at TRACE level. */
#define TRACING()                                                                                                      \
	Tracer trace {}

class Tracer
{
	const char *func;

public:
	template <typename Loc = std::source_location>
	explicit Tracer(Loc loc = std::source_location::current()) : func(loc.function_name())
	{
		TRACE(">>> {}\n", func);
	}

	~Tracer() { TRACE("<<< {}\n", func); }

	Tracer(const Tracer &) = delete;
	Tracer &operator=(const Tracer &) = delete;
	Tracer(Tracer &&) = delete;
	Tracer &operator=(Tracer &&) = delete;
};

#endif /* LOGGER_LOGGER_H */
