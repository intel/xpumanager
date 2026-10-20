/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Logging from the destructor of a static object that outlives the logger.
 *
 * This cannot be a doctest case: the situation only exists after main() returns. The static
 * below is constructed before the logger, so it is destroyed after it. Its destructor logs with
 * stderr redirected to a temporary file, checks what arrived, and ends the process with the
 * result. Before the fix, logging there used the destroyed logger and the process crashed.
 */

#include "logger.h"

#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

using namespace std::string_literals;

// Everything after the NUL must arrive too
const std::string NUL_MESSAGE = "nul\0kept"s;

constexpr int EXIT_NOT_AFTER_LOGGER = 2;
constexpr int EXIT_OUTPUT_MISSING = 3;

/** Reads everything written to @p file so far. */
[[nodiscard]] std::string readAll(std::FILE *file)
{
	std::string text;
	std::rewind(file);
	for (int c = std::fgetc(file); c != EOF; c = std::fgetc(file)) {
		text += static_cast<char>(c);
	}
	return text;
}

struct LogsAfterLogger
{
	LogsAfterLogger() = default;
	LogsAfterLogger(const LogsAfterLogger &) = delete;
	LogsAfterLogger &operator=(const LogsAfterLogger &) = delete;
	LogsAfterLogger(LogsAfterLogger &&) = delete;
	LogsAfterLogger &operator=(LogsAfterLogger &&) = delete;

	~LogsAfterLogger()
	{
		if (!Logger::isDestroyed()) {
			std::fputs("logger_exit_test: the logger was not destroyed first\n", stderr);
			std::_Exit(EXIT_NOT_AFTER_LOGGER);
		}

		std::FILE *capture = std::tmpfile();
		std::FILE *printCapture = std::tmpfile();
		if (capture == nullptr || printCapture == nullptr) {
			std::_Exit(EXIT_FAILURE);
		}
		std::fflush(stderr);
		std::fflush(stdout);
		const int savedStderr = dup(STDERR_FILENO);
		const int savedStdout = dup(STDOUT_FILENO);
		dup2(fileno(capture), STDERR_FILENO);
		dup2(fileno(printCapture), STDOUT_FILENO);

		ERR("logged by {} after the logger", "ERR");
		DBG("logged by {} after the logger\n", "DBG");
		ERR("{} after the logger", NUL_MESSAGE);
		// Without a trailing newline, as the default print sink would add one
		PRINT("printed {} after the logger", 1);
		PRINT("printed {} after the logger", 2);

		std::fflush(stderr);
		std::fflush(stdout);
		dup2(savedStderr, STDERR_FILENO);
		dup2(savedStdout, STDOUT_FILENO);
		close(savedStderr);
		close(savedStdout);

		const std::string out = readAll(capture);
		const std::string printed = readAll(printCapture);
		const bool ok = out.find("[Error] logged by ERR after the logger\n") != std::string::npos &&
						out.find("logger_exit_test.cpp:") != std::string::npos &&
						out.find(": logged by DBG after the logger\n") != std::string::npos &&
						out.find("[Error] " + NUL_MESSAGE + " after the logger\n") != std::string::npos &&
						printed == "printed 1 after the logger\nprinted 2 after the logger\n";
		if (!ok) {
			std::fprintf(stderr, "logger_exit_test: unexpected output after the logger:\n%s---\n%s", out.c_str(),
						 printed.c_str());
		}
		std::_Exit(ok ? EXIT_SUCCESS : EXIT_OUTPUT_MISSING);
	}
};

// Constructed during static initialization, before main() creates the logger, so it is
// destroyed after the logger.
LogsAfterLogger logsAfterLogger;

} // namespace

int main()
{
	Logger::setLevel(LogLevel::DBG);
	DBG("creates the logger\n");
	return Logger::isDestroyed() ? EXIT_FAILURE : EXIT_SUCCESS;
}
