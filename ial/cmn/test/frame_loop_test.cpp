/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

/**
 * @file frame_loop_test.cpp
 * @brief Unit tests for runFrameLoop(): frame counting, failure handling, screen control
 *        sequences and stop-event handling. No GPU or Level Zero handle is needed.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef INFO
#undef INFO
#endif

#include <string>
std::string progName = "test";

#include "frame_loop.h"
#include "utility/logger/logger.h"
#include "utility/logger/ostream_sink.h"
#include <chrono>
#include <csignal>
#include <memory>
#include <sstream>

#ifdef INFO
#undef INFO
#endif

namespace {

constexpr std::string_view ENTER_ALTERNATE_SCREEN = "\033[?1049h";
constexpr std::string_view LEAVE_ALTERNATE_SCREEN = "\033[?1049l";
constexpr std::string_view CLEAR_FRAME = "\033[H\033[J";

/// Collects everything PRINT() writes while it is alive, then restores the default sinks.
class PrintCapture
{
	std::ostringstream captured;

public:
	PrintCapture() { Logger::instance().setSink(std::make_shared<OStreamSink>(captured)); }
	~PrintCapture() { Logger::instance().setSink(nullptr); }
	PrintCapture(const PrintCapture &) = delete;
	PrintCapture &operator=(const PrintCapture &) = delete;
	PrintCapture(PrintCapture &&) = delete;
	PrintCapture &operator=(PrintCapture &&) = delete;

	[[nodiscard]] std::string text() const { return captured.str(); }
};

[[nodiscard]] std::size_t countOf(const std::string &haystack, std::string_view needle)
{
	std::size_t found = 0;
	for (auto pos = haystack.find(needle); pos != std::string::npos; pos = haystack.find(needle, pos + 1)) {
		++found;
	}
	return found;
}

constexpr auto SHORT_INTERVAL = std::chrono::milliseconds{1};

} // namespace

TEST_CASE("a zero interval renders exactly one frame whatever the count")
{
	PrintCapture capture;
	int calls = 0;
	const int rc = runFrameLoop(
		[&calls] {
			++calls;
			return RenderedFrame{ZE_RESULT_SUCCESS, "frame\n"};
		},
		{.interval = std::chrono::milliseconds{0}, .count = 5, .tty = true});

	CHECK(rc == 0);
	CHECK(calls == 1);
	CHECK(capture.text() == "frame\n");
}

TEST_CASE("the loop stops after the requested number of frames")
{
	PrintCapture capture;
	int calls = 0;
	const int rc = runFrameLoop(
		[&calls] {
			++calls;
			return RenderedFrame{ZE_RESULT_SUCCESS, "frame\n"};
		},
		{.interval = SHORT_INTERVAL, .count = 3, .tty = false});

	CHECK(rc == 0);
	CHECK(calls == 3);
	CHECK(countOf(capture.text(), "frame\n") == 3);
}

TEST_CASE("without a tty no escape sequence is written")
{
	PrintCapture capture;
	const int rc = runFrameLoop([] { return RenderedFrame{ZE_RESULT_SUCCESS, "frame\n"}; },
								{.interval = SHORT_INTERVAL, .count = 2, .tty = false});

	CHECK(rc == 0);
	CHECK(capture.text().find('\033') == std::string::npos);
}

TEST_CASE("with a tty each frame is cleared and the normal screen is restored last")
{
	PrintCapture capture;
	const int rc = runFrameLoop([] { return RenderedFrame{ZE_RESULT_SUCCESS, "frame\n"}; },
								{.interval = SHORT_INTERVAL, .count = 2, .tty = true});
	const std::string out = capture.text();

	CHECK(rc == 0);
	CHECK(out.starts_with(ENTER_ALTERNATE_SCREEN));
	CHECK(countOf(out, ENTER_ALTERNATE_SCREEN) == 1);
	CHECK(countOf(out, CLEAR_FRAME) == 2);
	CHECK(out.rfind(LEAVE_ALTERNATE_SCREEN) > out.rfind("frame\n"));
}

TEST_CASE("with a writer the frames go only to it, and diagnostics stay on the print sinks")
{
	for (const auto interval : {std::chrono::milliseconds{0}, SHORT_INTERVAL}) {
		PrintCapture capture;
		std::string written;
		FrameLoopOptions options{.interval = interval, .count = 2, .tty = false};
		options.write = [&written](std::string_view text) { written += text; };
		const int rc = runFrameLoop(
			[] {
				ERR("noise\n");
				return RenderedFrame{ZE_RESULT_SUCCESS, "frame\n"};
			},
			options);

		CHECK(rc == 0);
		CHECK(written == (interval.count() == 0 ? "frame\n" : "frame\nframe\n"));
		CHECK(capture.text().find("frame") == std::string::npos);
		CHECK(capture.text().find("noise") != std::string::npos);
	}
}

TEST_CASE("a renderer failure ends the loop with that code and a message")
{
	PrintCapture capture;
	int calls = 0;
	const int rc = runFrameLoop(
		[&calls] {
			++calls;
			if (calls == 2) {
				return RenderedFrame{ZE_RESULT_ERROR_DEVICE_LOST, {}};
			}
			return RenderedFrame{ZE_RESULT_SUCCESS, "frame\n"};
		},
		{.interval = SHORT_INTERVAL, .count = 0, .tty = false});

	CHECK(rc == static_cast<int>(ZE_RESULT_ERROR_DEVICE_LOST));
	CHECK(calls == 2);
	CHECK(capture.text().find("No GPU devices found.") != std::string::npos);
}

TEST_CASE("other failures are reported by code")
{
	PrintCapture capture;
	const int rc = runFrameLoop([] { return RenderedFrame{ZE_RESULT_ERROR_UNINITIALIZED, {}}; },
								{.interval = SHORT_INTERVAL, .count = 0, .tty = false});

	CHECK(rc == static_cast<int>(ZE_RESULT_ERROR_UNINITIALIZED));
	CHECK(capture.text().find("Device error: 0x") != std::string::npos);
}

TEST_CASE("the normal screen is restored before the failure is reported")
{
	PrintCapture capture;
	const int rc = runFrameLoop([] { return RenderedFrame{ZE_RESULT_ERROR_DEVICE_LOST, {}}; },
								{.interval = SHORT_INTERVAL, .count = 0, .tty = true});
	const std::string out = capture.text();

	CHECK(rc == static_cast<int>(ZE_RESULT_ERROR_DEVICE_LOST));
	CHECK(out.find(LEAVE_ALTERNATE_SCREEN) < out.find("No GPU devices found."));
}

TEST_CASE("a stop event ends an unbounded loop without waiting out the interval")
{
	PrintCapture capture;
	int calls = 0;
	const auto started = std::chrono::steady_clock::now();
	const int rc = runFrameLoop(
		[&calls] {
			++calls;
			raise(SIGINT);
			return RenderedFrame{ZE_RESULT_SUCCESS, "frame\n"};
		},
		{.interval = std::chrono::seconds{30}, .count = 0, .tty = false});

	CHECK(rc == 0);
	CHECK(calls == 1);
	CHECK(std::chrono::steady_clock::now() - started < std::chrono::seconds{10});
}

namespace {
void noopHandler(int /*signum*/) {}
} // namespace

TEST_CASE("the previous SIGINT and SIGTERM handlers are back once the loop ends")
{
	using Handler = void (*)(int);
	const Handler keepInterrupt = std::signal(SIGINT, noopHandler);
	const Handler keepTerminate = std::signal(SIGTERM, noopHandler);

	{
		PrintCapture capture;
		const int rc = runFrameLoop([] { return RenderedFrame{ZE_RESULT_SUCCESS, "frame\n"}; },
									{.interval = SHORT_INTERVAL, .count = 2, .tty = false});
		CHECK(rc == 0);
	}

	const Handler afterInterrupt = std::signal(SIGINT, keepInterrupt);
	const Handler afterTerminate = std::signal(SIGTERM, keepTerminate);
	CHECK(afterInterrupt == noopHandler);
	CHECK(afterTerminate == noopHandler);
}
