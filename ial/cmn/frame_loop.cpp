/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "frame_loop.h"
#include "debug.h"
#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdint>
#include <numeric>
#include <span>
#include <thread>

namespace {

/** Longest uninterrupted sleep between frames, so a stop event is noticed promptly. */
constexpr auto POLL_SLICE = std::chrono::milliseconds{100};

// Set by the signal handler, which may do nothing more than a lock-free atomic store.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<bool> stopRequested{false};

void onStopSignal(int /*signum*/) { stopRequested.store(true, std::memory_order_relaxed); }

/**
 * Turns SIGINT and SIGTERM into a stop request for as long as it lives, then puts the
 * previous handlers back. Only one may be alive at a time.
 */
class StopSignalScope
{
	using Handler = void (*)(int);

public:
	StopSignalScope()
		: previousInterrupt(std::signal(SIGINT, onStopSignal)), previousTerminate(std::signal(SIGTERM, onStopSignal))
	{
		stopRequested.store(false, std::memory_order_relaxed);
	}

	~StopSignalScope()
	{
		if (previousInterrupt != SIG_ERR) {
			std::signal(SIGINT, previousInterrupt);
		}
		if (previousTerminate != SIG_ERR) {
			std::signal(SIGTERM, previousTerminate);
		}
	}

	StopSignalScope(const StopSignalScope &) = delete;
	StopSignalScope &operator=(const StopSignalScope &) = delete;
	StopSignalScope(StopSignalScope &&) = delete;
	StopSignalScope &operator=(StopSignalScope &&) = delete;

private:
	Handler previousInterrupt;
	Handler previousTerminate;
};

/**
 * Silences diagnostics for as long as it lives. A message printed while a screen is being edited
 * in place lands on whatever row the cursor is on and scrolls the rows under it out of place.
 * A user who asked for debug output has chosen to live with that, so it is left alone then.
 */
class QuietLogs
{
public:
	explicit QuietLogs(bool quiet) : saved(getDbgLvl()), active(quiet && saved < LogLevel::DBG)
	{
		if (active) {
			setDbgLvl(LogLevel::NO_PRINT);
		}
	}

	~QuietLogs()
	{
		if (active) {
			setDbgLvl(saved);
		}
	}

	QuietLogs(const QuietLogs &) = delete;
	QuietLogs &operator=(const QuietLogs &) = delete;
	QuietLogs(QuietLogs &&) = delete;
	QuietLogs &operator=(QuietLogs &&) = delete;

private:
	LogLevel saved;
	bool active;
};

/// The lines of @p frame, without their newlines. Counts the same lines as rowsNeeded().
[[nodiscard]] std::vector<std::string> splitLines(std::string_view frame)
{
	std::vector<std::string> lines;
	while (!frame.empty()) {
		const auto end = frame.find('\n');
		lines.emplace_back(frame.substr(0, end));
		if (end == std::string_view::npos) {
			break;
		}
		frame.remove_prefix(end + 1);
	}
	return lines;
}

/// Escape sequence that puts the cursor at the start of a 1-based screen row.
[[nodiscard]] std::string moveToRow(std::size_t row) { return xpum::compat::format("\033[{};1H", row); }

[[nodiscard]] std::size_t totalRows(std::span<const std::size_t> layout)
{
	return std::accumulate(layout.begin(), layout.end(), std::size_t{0});
}

/**
 * @brief Counts the rows one line takes up on a screen @p columns wide.
 *
 * A UTF-8 character is never wider on screen than its bytes, so a line no longer in bytes than
 * the window is one row. A longer line is counted exactly only when it is printable ASCII; for
 * anything else only the terminal knows where it wraps.
 *
 * @retval rows    The line's rows; one when @p columns is zero (unknown).
 * @retval nullopt The line is wider than the window and its width on screen is not known.
 */
[[nodiscard]] std::optional<std::size_t> rowsOfLine(std::string_view line, std::size_t columns)
{
	if (columns == 0 || line.size() <= columns) {
		return 1;
	}
	if (!std::ranges::all_of(line, [](char c) { return c >= ' ' && c <= '~'; })) {
		return std::nullopt;
	}
	return (line.size() + columns - 1) / columns;
}

/**
 * Keeps one frame on the alternate screen and brings it up to date by rewriting only the lines
 * that differ, so the rest of the screen is never blanked or redrawn.
 */
class InPlaceScreen
{
public:
	/**
	 * @brief Shows @p frame, drawing it whole the first time or after a resize, and only the lines
	 *        that changed otherwise.
	 *
	 * @param[in] frame Text to show; one line per row.
	 * @param[in] size  The terminal window, if known.
	 *
	 * @retval true  The frame is on screen.
	 * @retval false The frame does not fit the window; nothing was drawn.
	 */
	[[nodiscard]] bool show(std::string_view frame, const std::optional<TerminalSize> &size)
	{
		const std::size_t columns = size ? size->columns : 0;
		auto layout = lineRows(frame, columns);
		if (!layout || (size && totalRows(*layout) >= size->rows)) {
			return false;
		}
		std::vector<std::string> lines = splitLines(frame);
		std::string out;
		std::size_t row = 1;
		if (!onAlternateScreen || size != drawnFor) {
			if (!onAlternateScreen) {
				out += "\033[?1049h\033[?25l"; // enter the alternate screen, hide the cursor
				onAlternateScreen = true;
			}
			out += "\033[H\033[J"; // cursor home + erase to end
			out += frame;
			if (!frame.ends_with('\n')) {
				out += '\n';
			}
		} else {
			// Once a line takes a different number of rows than before, every line below it has moved,
			// so from there on each line is rewritten.
			bool shifted = false;
			for (std::size_t i = 0; i < lines.size(); ++i) {
				shifted = shifted || i >= shown.size() || shownRows[i] != (*layout)[i];
				if (shifted || shown[i] != lines[i]) {
					// Clear the line's rows before writing it. Erasing after the text instead would need
					// to know which column it ends in, which bytes do not tell for non-ASCII text, and
					// would remove the last character of a line that ends on the last column.
					for (std::size_t r = 0; r < (*layout)[i]; ++r) {
						out += moveToRow(row + r);
						out += "\033[2K";
					}
					out += moveToRow(row);
					out += lines[i];
				}
				row += (*layout)[i];
			}
			if (shifted || row - 1 < totalRows(shownRows)) {
				out += moveToRow(row);
				out += "\033[J"; // erase the rows an earlier, longer frame used
			}
			out += moveToRow(row);
		}
		// The log sink adds a newline to text that has no carriage return, and a newline here would
		// scroll the screen when the frame fills it. A carriage return only moves to column 1.
		out += '\r';
		PRINT("{}", out);
		shown = std::move(lines);
		shownRows = std::move(*layout);
		drawnFor = size;
		return true;
	}

	/// Whether the alternate screen is showing, so diagnostics must be held back.
	[[nodiscard]] bool active() const noexcept { return onAlternateScreen; }

	/// Returns to the normal screen and shows the cursor again. Does nothing if never entered.
	void leave()
	{
		if (onAlternateScreen) {
			PRINT("\033[?25h\033[?1049l\r");
			onAlternateScreen = false;
		}
	}

private:
	std::vector<std::string> shown;
	std::vector<std::size_t> shownRows; ///< Screen rows each line of shown takes up.
	std::optional<TerminalSize> drawnFor;
	bool onAlternateScreen = false;
};

/**
 * @brief Tells the user why no frame could be drawn.
 *
 * @param[in] result Failure reported by the renderer.
 *
 * @return @p result, as the process exit code.
 */
int reportFrameError(ze_result_t result)
{
	if (result == ZE_RESULT_ERROR_DEVICE_LOST) {
		PRINT("No GPU devices found.\n");
	} else {
		PRINT("Device error: 0x{:X}\n", static_cast<uint32_t>(result));
	}
	return static_cast<int>(result);
}

} // namespace

/**
 * @brief Counts the terminal rows a frame occupies.
 *
 * @param[in] frame   Text as it will be printed.
 * @param[in] columns Width of the terminal; zero means unknown, and no line is taken to wrap.
 *
 * @return One row per line, plus the rows a line wider than @p columns wraps onto. Widths are
 *         counted in bytes, which can only overestimate.
 */
std::size_t rowsNeeded(std::string_view frame, std::size_t columns)
{
	std::size_t rows = 0;
	while (!frame.empty()) {
		const auto end = frame.find('\n');
		const std::size_t width = end == std::string_view::npos ? frame.size() : end;
		rows += (columns > 0 && width > columns) ? (width + columns - 1) / columns : 1;
		if (end == std::string_view::npos) {
			break;
		}
		frame.remove_prefix(end + 1);
	}
	return rows;
}

/**
 * @brief Works out where each line of a frame lands on a screen, wrapping included.
 *
 * @param[in] frame   Text as it will be printed.
 * @param[in] columns Width of the terminal; zero means unknown, and no line is taken to wrap.
 *
 * @retval rows    The screen rows each line takes up, in order.
 * @retval nullopt A line is wider than the window and holds characters other than printable
 *                 ASCII, so where it wraps is up to the terminal.
 */
std::optional<std::vector<std::size_t>> lineRows(std::string_view frame, std::size_t columns)
{
	std::vector<std::size_t> layout;
	for (const auto &line : splitLines(frame)) {
		const auto rows = rowsOfLine(line, columns);
		if (!rows) {
			return std::nullopt;
		}
		layout.push_back(*rows);
	}
	return layout;
}

/**
 * @brief Tells whether a frame can be kept up to date in place on a screen.
 *
 * @param[in] frame  Text as it will be printed.
 * @param[in] screen The terminal window, if known.
 *
 * @retval true  The frame, wrapped lines included, leaves at least one row free, or the size
 *               is unknown.
 * @retval false The frame would scroll, or a line wraps where only the terminal knows.
 */
bool fitsScreen(std::string_view frame, const std::optional<TerminalSize> &screen)
{
	if (!screen) {
		return true;
	}
	const auto layout = lineRows(frame, screen->columns);
	return layout && totalRows(*layout) < screen->rows;
}

/**
 * @brief Renders frames once or repeatedly at a fixed interval.
 *
 * With a zero interval one frame is rendered and printed. Otherwise frames are rendered
 * back to back, each starting one interval after the previous one started, until a stop
 * event arrives or the frame limit is reached. A stop event is SIGINT or SIGTERM.
 *
 * Without opts.tty the frames are appended with no escape sequences. With it they use the
 * alternate screen, and opts.redraw says how a frame replaces the last. ClearAndPrint clears
 * the screen and prints the whole frame. ChangedLines prints the first frame whole and then
 * rewrites only the lines whose text changed, so unchanged data stays on screen. A line wider
 * than the window wraps onto more rows, and once a line wraps differently every line below it
 * is rewritten. A frame taller than the window cannot be kept up to date without hiding part
 * of it, nor can one with a long line whose wrapping only the terminal knows. It is then printed once on the normal
 * screen, with a notice, and later frames print nothing until the window has grown enough to show one in place, which
 * it then does. A window shrunk below that goes back to printing the frame once. Diagnostics are held back while the
 * screen is edited in place or waiting for room.
 *
 * The normal screen is restored before any diagnostic is printed, including after a renderer
 * failure and after a stop event.
 *
 * @param[in] render Produces one frame. It runs on the calling thread.
 * @param[in] opts   Interval, frame limit and presentation.
 *
 * @retval 0                           Every rendered frame was printed.
 * @retval ZE_RESULT_ERROR_DEVICE_LOST The renderer found no GPU devices.
 * @retval ZE_RESULT_ERROR_*           The failure the renderer reported.
 */
int runFrameLoop(const FrameRenderer &render, const FrameLoopOptions &opts)
{
	const auto emit = [&opts](std::string_view text) {
		if (opts.write) {
			opts.write(text);
		} else {
			PRINT("{}", text);
		}
	};
	if (opts.interval.count() <= 0) {
		RenderedFrame frame;
		{
			const QuietLogs quiet{opts.quietRender};
			frame = render();
		}
		const auto &[status, text] = frame;
		if (status != ZE_RESULT_SUCCESS) {
			return reportFrameError(status);
		}
		emit(text);
		return 0;
	}

	const bool inPlace = opts.tty && opts.redraw == Redraw::ChangedLines;
	InPlaceScreen screen;
	bool waitingForRoom = false; // the report was printed once because the window is too small for it
	int iterations = 0;
	ze_result_t loopResult = ZE_RESULT_SUCCESS;
	{
		const StopSignalScope stopSignals;
		if (opts.tty && !inPlace) {
			PRINT("\033[?1049h"); // enter alternate screen buffer
		}

		while (!stopRequested.load(std::memory_order_relaxed) && (opts.count <= 0 || iterations < opts.count)) {
			// Start the deadline before rendering so the interval includes the time the
			// renderer spends sampling.
			const auto deadline = std::chrono::steady_clock::now() + opts.interval;
			RenderedFrame frame;
			{
				const QuietLogs quiet{opts.quietRender || screen.active() || waitingForRoom};
				frame = render();
			}
			if (frame.status != ZE_RESULT_SUCCESS) {
				loopResult = frame.status;
				break;
			}
			if (inPlace) {
				const auto size = opts.screenSize();
				if (screen.show(frame.text, size)) {
					waitingForRoom = false;
				} else if (!waitingForRoom) {
					// Show it whole once, so nothing is hidden, then stay quiet rather than append it
					// every interval. Live updates start when the window is big enough.
					screen.leave();
					emit(frame.text);
					const std::size_t columns = size ? size->columns : 0;
					if (lineRows(frame.text, columns)) {
						INFO("The report needs {} rows and the terminal has {}, so it cannot be kept up to "
							 "date in place without hiding part of it. It is printed once; live updates start "
							 "when the window is large enough. Select fewer sections with --display to fit it "
							 "now.\n",
							 rowsNeeded(frame.text, columns), size ? size->rows : 0);
					} else {
						INFO("A line of the report is wider than the {}-column terminal and holds characters "
							 "whose width only the terminal knows, so it cannot be kept up to date in place. It "
							 "is printed once; live updates start when the window is wide enough.\n",
							 columns);
					}
					waitingForRoom = true;
				}
			} else {
				if (opts.tty) {
					PRINT("\033[H\033[J"); // cursor home + erase to end, clearing stale rows
				}
				emit(frame.text);
			}
			++iterations;
			if (opts.count > 0 && iterations >= opts.count) {
				break;
			}

			while (!stopRequested.load(std::memory_order_relaxed) && std::chrono::steady_clock::now() < deadline) {
				const auto remaining =
					std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
				std::this_thread::sleep_for(std::min(remaining, POLL_SLICE));
			}
		}

		if (opts.tty && !inPlace) {
			PRINT("\033[?1049l"); // restore normal screen buffer before any diagnostic output
		}
		screen.leave();
	}

	return loopResult == ZE_RESULT_SUCCESS ? 0 : reportFrameError(loopResult);
}
