/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

/**
 * @file frame_loop_test.cpp
 * @brief Unit tests for runFrameLoop(): frame counting, failure handling, screen control
 *        sequences, in-place updates of changed lines, window resizes and stop-event handling.
 *        No GPU or Level Zero handle is needed.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef INFO
#undef INFO
#endif

#include <string>
std::string progName = "test";

#include "debug.h"
#include "frame_loop.h"
#include "utility/logger/logger.h"
#include "utility/logger/ostream_sink.h"
#include <chrono>
#include <csignal>
#include <memory>
#include <sstream>
#include <vector>

#ifdef INFO
#undef INFO
#endif

namespace {

constexpr std::string_view ENTER_ALTERNATE_SCREEN = "\033[?1049h";
constexpr std::string_view LEAVE_ALTERNATE_SCREEN = "\033[?1049l";
constexpr std::string_view CLEAR_FRAME = "\033[H\033[J";
constexpr std::string_view NOTICE = "cannot be kept up to date";

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

using ScreenSize = std::function<std::optional<TerminalSize>()>;

[[nodiscard]] ScreenSize fixedScreen(unsigned rows, unsigned columns)
{
	return [rows, columns] { return std::optional<TerminalSize>{TerminalSize{.rows = rows, .columns = columns}}; };
}

/// A frame of @p lines distinct lines, "row 0" to "row N-1".
[[nodiscard]] std::string frameOfLines(std::size_t lines)
{
	std::string frame;
	for (std::size_t i = 0; i < lines; ++i) {
		frame += "row " + std::to_string(i) + "\n";
	}
	return frame;
}

/// Renders the given frames in turn, then the last one for as long as it is asked.
[[nodiscard]] FrameRenderer playing(std::vector<std::string> frames, int *calls = nullptr)
{
	return [frames = std::move(frames), calls, next = std::size_t{0}]() mutable {
		if (calls != nullptr) {
			++*calls;
		}
		const auto &frame = frames[std::min(next, frames.size() - 1)];
		++next;
		return RenderedFrame{ZE_RESULT_SUCCESS, frame};
	};
}

/// What an in-place update writes to replace the line that starts on 1-based @p row and takes @p rows rows.
[[nodiscard]] std::string rewrite(std::size_t row, std::string_view text, std::size_t rows = 1)
{
	std::string out;
	for (std::size_t r = 0; r < rows; ++r) {
		out += "\033[" + std::to_string(row + r) + ";1H\033[2K";
	}
	return out + "\033[" + std::to_string(row) + ";1H" + std::string{text};
}

FrameLoopOptions inPlaceOptions(int count)
{
	return {.interval = SHORT_INTERVAL, .count = count, .tty = true, .redraw = Redraw::ChangedLines};
}

} // namespace

// ---------------------------------------------------------------------------------------
// Counting, failures and plain output
// ---------------------------------------------------------------------------------------

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

TEST_CASE("without a tty no escape sequence is written, whatever the redraw mode")
{
	for (const Redraw redraw : {Redraw::ClearAndPrint, Redraw::ChangedLines}) {
		PrintCapture capture;
		const int rc = runFrameLoop(
			[] { return RenderedFrame{ZE_RESULT_SUCCESS, "frame\n"}; },
			{.interval = SHORT_INTERVAL, .count = 2, .tty = false, .redraw = redraw, .screenSize = fixedScreen(2, 80)});

		CHECK(rc == 0);
		CHECK(capture.text() == "frame\nframe\n");
	}
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

// ---------------------------------------------------------------------------------------
// ClearAndPrint: the whole frame is redrawn each time
// ---------------------------------------------------------------------------------------

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

TEST_CASE("the normal screen is restored before the failure is reported")
{
	PrintCapture capture;
	const int rc = runFrameLoop([] { return RenderedFrame{ZE_RESULT_ERROR_DEVICE_LOST, {}}; },
								{.interval = SHORT_INTERVAL, .count = 0, .tty = true});
	const std::string out = capture.text();

	CHECK(rc == static_cast<int>(ZE_RESULT_ERROR_DEVICE_LOST));
	CHECK(out.find(LEAVE_ALTERNATE_SCREEN) < out.find("No GPU devices found."));
}

TEST_CASE("ClearAndPrint ignores the window size, as the default view always has")
{
	PrintCapture capture;
	const std::string frame = frameOfLines(10);
	const int rc =
		runFrameLoop([&frame] { return RenderedFrame{ZE_RESULT_SUCCESS, frame}; },
					 {.interval = SHORT_INTERVAL, .count = 3, .tty = true, .screenSize = fixedScreen(4, 80)});

	CHECK(rc == 0);
	CHECK(countOf(capture.text(), CLEAR_FRAME) == 3);
	CHECK(countOf(capture.text(), "row 0\n") == 3);
}

// ---------------------------------------------------------------------------------------
// ChangedLines: only what changed is rewritten
// ---------------------------------------------------------------------------------------

TEST_CASE("the first frame is drawn whole after the screen is entered and the cursor hidden")
{
	PrintCapture capture;
	auto options = inPlaceOptions(1);
	options.screenSize = fixedScreen(50, 120);
	const int rc = runFrameLoop(playing({"A\nB\nC\n"}), options);
	const std::string out = capture.text();

	CHECK(rc == 0);
	CHECK(out.starts_with("\033[?1049h\033[?25l\033[H\033[JA\nB\nC\n"));
	CHECK(out.find("\033[?25h") != std::string::npos);
	CHECK(out.rfind(LEAVE_ALTERNATE_SCREEN) > out.rfind("C\n"));
}

TEST_CASE("a later frame rewrites only the lines that changed")
{
	PrintCapture capture;
	auto options = inPlaceOptions(2);
	options.screenSize = fixedScreen(50, 120);
	const int rc = runFrameLoop(playing({"A\nB\nC\n", "A\nX\nC\n"}), options);
	const std::string out = capture.text();

	CHECK(rc == 0);
	CHECK(countOf(out, CLEAR_FRAME) == 1);
	const auto update = out.substr(out.find("\033[2;1H"));
	CHECK(update.starts_with(rewrite(2, "X")));
	CHECK(update.find('A') == std::string::npos);
	CHECK(update.find('C') == std::string::npos);
	CHECK(update.find("\033[1;1H") == std::string::npos);
	CHECK(update.find("\033[3;1H") == std::string::npos);
}

TEST_CASE("an unchanged frame rewrites nothing")
{
	PrintCapture capture;
	auto options = inPlaceOptions(3);
	options.screenSize = fixedScreen(50, 120);
	const int rc = runFrameLoop(playing({"A\nB\nC\n"}), options);

	CHECK(rc == 0);
	CHECK(countOf(capture.text(), "\033[2K") == 0);
	CHECK(countOf(capture.text(), "A\nB\nC\n") == 1);
}

TEST_CASE("a longer frame writes its new rows and a shorter one erases the rows it no longer uses")
{
	{
		PrintCapture capture;
		auto options = inPlaceOptions(2);
		options.screenSize = fixedScreen(50, 120);
		const int rc = runFrameLoop(playing({"A\nB\n", "A\nB\nnew\n"}), options);
		CHECK(rc == 0);
		CHECK(capture.text().find(rewrite(3, "new")) != std::string::npos);
	}
	{
		PrintCapture capture;
		auto options = inPlaceOptions(2);
		options.screenSize = fixedScreen(50, 120);
		const int rc = runFrameLoop(playing({"A\nB\nC\n", "A\nB\n"}), options);
		CHECK(rc == 0);
		CHECK(capture.text().find("\033[3;1H\033[J") != std::string::npos);
	}
}

TEST_CASE("an update never ends in a newline, which would scroll a screen the frame fills")
{
	PrintCapture capture;
	auto options = inPlaceOptions(2);
	options.screenSize = fixedScreen(4, 120); // three lines, one row to spare
	const int rc = runFrameLoop(playing({"A\nB\nC\n", "A\nX\nC\n"}), options);
	const std::string out = capture.text();

	CHECK(rc == 0);
	const auto update = out.substr(out.find("\033[2;1H"));
	CHECK(update.find('\n') == std::string::npos);
}

TEST_CASE("a window that changes size gets the whole frame again")
{
	PrintCapture capture;
	int asked = 0;
	auto options = inPlaceOptions(3);
	options.screenSize = [&asked] {
		return std::optional<TerminalSize>{TerminalSize{.rows = ++asked <= 2 ? 50U : 60U, .columns = 120}};
	};
	const int rc = runFrameLoop(playing({"A\nB\nC\n"}), options);

	CHECK(rc == 0);
	CHECK(countOf(capture.text(), CLEAR_FRAME) == 2);
	CHECK(countOf(capture.text(), ENTER_ALTERNATE_SCREEN) == 1);
}

TEST_CASE("diagnostics are held back once the screen is being edited, but not before the first frame")
{
	PrintCapture capture;
	auto options = inPlaceOptions(3);
	options.screenSize = fixedScreen(50, 120);
	const int rc = runFrameLoop(
		[] {
			ERR("noise\n");
			return RenderedFrame{ZE_RESULT_SUCCESS, "A\nB\n"};
		},
		options);

	CHECK(rc == 0);
	CHECK(countOf(capture.text(), "noise") == 1);
	CHECK(capture.text().find("noise") < capture.text().find(ENTER_ALTERNATE_SCREEN));
	CHECK(getDbgLvl() == LogLevel::INFO); // the level is put back
}

TEST_CASE("a first frame that fails never switches screens")
{
	PrintCapture capture;
	auto options = inPlaceOptions(0);
	options.screenSize = fixedScreen(50, 120);
	const int rc = runFrameLoop([] { return RenderedFrame{ZE_RESULT_ERROR_DEVICE_LOST, {}}; }, options);

	CHECK(rc == static_cast<int>(ZE_RESULT_ERROR_DEVICE_LOST));
	CHECK(capture.text().find('\033') == std::string::npos);
	CHECK(capture.text().find("No GPU devices found.") != std::string::npos);
}

TEST_CASE("an unknown window size keeps the frame on the alternate screen")
{
	PrintCapture capture;
	auto options = inPlaceOptions(2);
	options.screenSize = [] { return std::optional<TerminalSize>{}; };
	const int rc = runFrameLoop(playing({frameOfLines(500)}), options);

	CHECK(rc == 0);
	CHECK(countOf(capture.text(), ENTER_ALTERNATE_SCREEN) == 1);
}

// ---------------------------------------------------------------------------------------
// ChangedLines: a frame the window cannot hold
// ---------------------------------------------------------------------------------------

// Cutting the frame to fit would hide data and appending it every interval grows without
// bound, so it is printed once and the loop waits for the window to grow.

TEST_CASE("a frame too tall for the window is printed once whole, and the loop keeps going")
{
	PrintCapture capture;
	int calls = 0;
	auto options = inPlaceOptions(5);
	options.screenSize = fixedScreen(4, 120);
	const std::string frame = frameOfLines(10);
	const int rc = runFrameLoop(playing({frame}, &calls), options);
	const std::string out = capture.text();

	CHECK(rc == 0);
	CHECK(calls == 5);
	CHECK(out.starts_with(frame));
	CHECK(countOf(out, "row 0\n") == 1);
	CHECK(countOf(out, NOTICE) == 1);
	CHECK(out.find('\033') == std::string::npos);
}

TEST_CASE("a line that wraps is kept live, and the lines below it are found on their real rows")
{
	PrintCapture capture;
	auto options = inPlaceOptions(2);
	options.screenSize = fixedScreen(50, 40);
	const std::string wide(100, 'x'); // three rows at 40 columns
	const int rc = runFrameLoop(playing({wide + "\nB\nC\n", wide + "\nB\nZ\n"}), options);
	const std::string out = capture.text();

	CHECK(rc == 0);
	CHECK(countOf(out, NOTICE) == 0);
	CHECK(countOf(out, CLEAR_FRAME) == 1);
	const auto update = out.substr(out.rfind(wide) + wide.size());
	CHECK(update.find(rewrite(5, "Z")) != std::string::npos);
	CHECK(update.find("\033[4;1H") == std::string::npos);
}

TEST_CASE("a changed line that wraps onto a different number of rows rewrites every line below it")
{
	PrintCapture capture;
	auto options = inPlaceOptions(2);
	options.screenSize = fixedScreen(50, 40);
	const int rc = runFrameLoop(playing({"A\nshort\nB\nC\n", "A\n" + std::string(50, 'y') + "\nB\nC\n"}), options);
	const std::string out = capture.text();

	CHECK(rc == 0);
	const auto update = out.substr(out.find("\033[2;1H"));
	CHECK(update.find(rewrite(4, "B")) != std::string::npos);
	CHECK(update.find(rewrite(5, "C")) != std::string::npos);
	CHECK(update.find("\033[6;1H\033[J") != std::string::npos);
	CHECK(update.find("\033[1;1H") == std::string::npos);
}

TEST_CASE("a rewritten line clears every row it takes before it is written, so nothing follows its last column")
{
	PrintCapture capture;
	auto options = inPlaceOptions(2);
	options.screenSize = fixedScreen(50, 40);
	const int rc = runFrameLoop(playing({std::string(80, 'a') + "\n", std::string(80, 'b') + "\n"}), options);
	const std::string out = capture.text();

	CHECK(rc == 0);
	const auto update = out.substr(out.find("\033[1;1H"));
	CHECK(update.starts_with(rewrite(1, std::string(80, 'b'), 2) + "\033[3;1H"));
}

TEST_CASE("shorter non-ASCII text that fills the row in bytes still clears the old text")
{
	PrintCapture capture;
	auto options = inPlaceOptions(2);
	options.screenSize = fixedScreen(50, 9);
	// "\u00e9\u00e9\u00e9" is 6 bytes but 3 cells; "abc" keeps the line 9 bytes, so it fills the row in bytes only.
	const int rc = runFrameLoop(playing({"abcdefghi\nB\n", "abc\u00e9\u00e9\u00e9\nB\n"}), options);

	CHECK(rc == 0);
	CHECK(capture.text().find(rewrite(1, "abc\u00e9\u00e9\u00e9")) != std::string::npos);
}

TEST_CASE("a long line whose width only the terminal knows is printed once, with the reason")
{
	PrintCapture capture;
	auto options = inPlaceOptions(2);
	options.screenSize = fixedScreen(50, 40);
	std::string wide;
	for (int i = 0; i < 20; ++i) {
		wide += "\u00e9t\u00e9"; // 5 bytes, 3 characters: 100 bytes, 60 characters
	}
	const int rc = runFrameLoop(playing({wide + "\nB\n"}), options);

	CHECK(rc == 0);
	CHECK(countOf(capture.text(), "only the terminal knows") == 1);
	CHECK(capture.text().find(ENTER_ALTERNATE_SCREEN) == std::string::npos);
}

TEST_CASE("a non-ASCII line that fits in bytes is one row and stays live")
{
	PrintCapture capture;
	auto options = inPlaceOptions(2);
	options.screenSize = fixedScreen(50, 40);
	const int rc = runFrameLoop(playing({"\u00e9t\u00e9\nB\n", "\u00e9t\u00e9\nZ\n"}), options);

	CHECK(rc == 0);
	CHECK(capture.text().find(rewrite(2, "Z")) != std::string::npos);
}

TEST_CASE("a window that grows enough starts live updates, with the whole frame drawn at once")
{
	PrintCapture capture;
	int asked = 0;
	int calls = 0;
	auto options = inPlaceOptions(4);
	options.screenSize = [&asked] {
		return std::optional<TerminalSize>{TerminalSize{.rows = ++asked <= 2 ? 4U : 50U, .columns = 120}};
	};
	const std::string frame = frameOfLines(10);
	const int rc = runFrameLoop(playing({frame}, &calls), options);
	const std::string out = capture.text();

	CHECK(rc == 0);
	CHECK(calls == 4);
	CHECK(countOf(out, NOTICE) == 1);
	CHECK(countOf(out, ENTER_ALTERNATE_SCREEN) == 1);
	CHECK(out.find(NOTICE) < out.find(ENTER_ALTERNATE_SCREEN));
	CHECK(out.find(std::string{CLEAR_FRAME} + frame) != std::string::npos);
}

TEST_CASE("a window that shrinks below the frame leaves the alternate screen and prints it once")
{
	PrintCapture capture;
	int asked = 0;
	auto options = inPlaceOptions(4);
	options.screenSize = [&asked] {
		return std::optional<TerminalSize>{TerminalSize{.rows = ++asked <= 2 ? 50U : 4U, .columns = 120}};
	};
	const std::string frame = frameOfLines(10);
	const int rc = runFrameLoop(playing({frame}), options);
	const std::string out = capture.text();

	CHECK(rc == 0);
	CHECK(countOf(out, ENTER_ALTERNATE_SCREEN) == 1);
	CHECK(countOf(out, LEAVE_ALTERNATE_SCREEN) == 1);
	CHECK(countOf(out, NOTICE) == 1);
	CHECK(out.find(LEAVE_ALTERNATE_SCREEN) < out.find(NOTICE));
	CHECK(out.rfind("row 9\n") > out.find(LEAVE_ALTERNATE_SCREEN)); // printed on the normal screen
}

TEST_CASE("diagnostics are held back while the loop waits for room, so they do not pile up")
{
	PrintCapture capture;
	auto options = inPlaceOptions(4);
	options.screenSize = fixedScreen(4, 120);
	const int rc = runFrameLoop(
		[] {
			ERR("noise\n");
			return RenderedFrame{ZE_RESULT_SUCCESS, frameOfLines(10)};
		},
		options);

	CHECK(rc == 0);
	CHECK(countOf(capture.text(), "noise") == 1);
}

// ---------------------------------------------------------------------------------------
// Fitting
// ---------------------------------------------------------------------------------------

TEST_CASE("rowsNeeded: one row per line, and a long line wraps")
{
	CHECK(rowsNeeded("", 80) == 0);
	CHECK(rowsNeeded("a", 80) == 1);
	CHECK(rowsNeeded("a\n", 80) == 1);
	CHECK(rowsNeeded("a\nb\n", 80) == 2);
	CHECK(rowsNeeded("a\n\nb\n", 80) == 3);
	CHECK(rowsNeeded("a\nb", 80) == 2);
	CHECK(rowsNeeded(std::string(80, 'x') + "\n", 80) == 1);
	CHECK(rowsNeeded(std::string(81, 'x') + "\n", 80) == 2);
	CHECK(rowsNeeded(std::string(160, 'x') + "\n", 80) == 2);
	CHECK(rowsNeeded(std::string(161, 'x') + "\n", 80) == 3);
	CHECK(rowsNeeded(std::string(500, 'x') + "\n", 0) == 1);
}

TEST_CASE("fitsScreen: a row must be left free, counting the rows a long line wraps onto")
{
	const TerminalSize screen{.rows = 4, .columns = 80};
	CHECK(fitsScreen(frameOfLines(3), screen));
	CHECK_FALSE(fitsScreen(frameOfLines(4), screen));
	CHECK_FALSE(fitsScreen(frameOfLines(5), screen));
	CHECK(fitsScreen(std::string(81, 'x') + "\n", screen));
	CHECK(fitsScreen(std::string(80, 'x') + "\nB\n", screen));
	CHECK_FALSE(fitsScreen(std::string(161, 'x') + "\nB\n", screen));
	CHECK_FALSE(fitsScreen(std::string(81, '\t') + "\n", TerminalSize{.rows = 50, .columns = 80}));
	CHECK(fitsScreen(frameOfLines(1000), std::nullopt));
}

TEST_CASE("lineRows: printable ASCII wraps exactly, other text only when it fits in bytes")
{
	using Rows = std::vector<std::size_t>;
	CHECK(lineRows("", 80) == Rows{});
	CHECK(lineRows("a\n\nb", 80) == Rows{1, 1, 1});
	CHECK(lineRows(std::string(80, 'x') + "\n" + std::string(81, 'x') + "\n", 80) == Rows{1, 2});
	CHECK(lineRows(std::string(161, 'x'), 80) == Rows{3});
	CHECK(lineRows("\u00e9t\u00e9", 5) == Rows{1});
	CHECK_FALSE(lineRows("\u00e9t\u00e9", 4));
	CHECK_FALSE(lineRows("a\tb", 2));
	CHECK(lineRows(std::string(500, '\t'), 0) == Rows{1});
}

// ---------------------------------------------------------------------------------------
// Stop events
// ---------------------------------------------------------------------------------------

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
