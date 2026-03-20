/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef LOGGER_FILESTREAM_SINK_H
#define LOGGER_FILESTREAM_SINK_H

#include "log_record.h"
#include "log_level.h"
#include "sink_base.h"

#include <chrono>
#include <format>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <string>

/**
 * @brief Sink that owns a std::ofstream opened at construction.
 *
 * Prefer OStreamSink(std::cout) / OStreamSink(std::cerr) for stdout/stderr.
 * Use this when you need to write to a file path and want the sink to own
 * the handle:
 * @code
 *     Logger::instance().setSink(
 *         std::make_shared<FileStreamSink>("/var/log/xpu-smi.log"));
 * @endcode
 *
 * The file is opened in append mode so log lines survive process restarts.
 * emit() and sync() throw std::runtime_error on write/flush failure; the
 * exception is caught by Sink::log() and written directly to stderr
 * (Logger cannot safely be called from inside a sink).
 *
 * @param showTimestamp  Prepend "[HH:MM:SS.mmm] " to every line.
 *                       Default true (production file logging).
 * @param showThreadId   Prepend "[<thread-id>] " after the timestamp.
 *                       Default true (production file logging).
 */
class FileStreamSink final : public Sink
{
	std::ofstream file;
	bool mShowTimestamp;
	bool mShowThreadId;

public:
	explicit FileStreamSink(const char *path, bool showTimestamp = true, bool showThreadId = true)
		: file(path, std::ios::app), mShowTimestamp(showTimestamp), mShowThreadId(showThreadId)
	{}

	explicit FileStreamSink(const std::string &path, bool showTimestamp = true, bool showThreadId = true)
		: file(path, std::ios::app), mShowTimestamp(showTimestamp), mShowThreadId(showThreadId)
	{}

	[[nodiscard]] bool isOpen() const noexcept { return file.is_open(); }

	void emit(const LogRecord &record) override
	{
		if (!file.is_open()) {
			throw std::runtime_error("FileStreamSink: file is not open");
		}
		if (mShowTimestamp) {
			file << std::format("[{:%H:%M:%S}] ", std::chrono::floor<std::chrono::milliseconds>(record.timestamp));
		}
		if (mShowThreadId) {
			file << '[' << record.threadId << "] ";
		}
		if (record.level == LogLevel::ERR) {
			file << record.prefix << record.loc.file_name() << ':' << record.loc.line() << ": ";
		} else if (!record.prefix.empty()) {
			file << record.prefix;
		}
		file << record.msg;
		const bool callerManagesLine =
			!record.msg.empty() && (record.msg.back() == '\n' || record.msg.find('\r') != std::string_view::npos);
		if (!callerManagesLine) {
			file << '\n';
		}
		if (file.fail()) {
			throw std::runtime_error("FileStreamSink: write error");
		}
	}

	void sync() override
	{
		file.flush();
		if (file.fail()) {
			throw std::runtime_error("FileStreamSink: flush error");
		}
	}
};

#endif /* LOGGER_FILESTREAM_SINK_H */
