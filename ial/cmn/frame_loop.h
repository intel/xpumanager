/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef _FRAME_LOOP_H
#define _FRAME_LOOP_H

#include "terminal.h"
#include <ze_api.h>
#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

/** How a frame replaces the previous one on a terminal. */
enum class Redraw
{
	ClearAndPrint, ///< Clear the screen and print the whole frame again.
	ChangedLines,  ///< Print the frame once, then rewrite only the lines whose text changed.
};

/** How runFrameLoop() repeats and presents its frames. */
struct FrameLoopOptions
{
	std::chrono::milliseconds interval{0}; ///< Time between frame starts; 0 renders exactly one frame.
	int count = 0;						   ///< Maximum frames; 0 is unlimited. Ignored when interval is 0.
	bool tty = true;					   ///< Draw on the alternate screen, replacing each frame in place.
	Redraw redraw = Redraw::ClearAndPrint; ///< How a frame replaces the last one when tty is set.
	/// The window stdout is attached to, asked for again on every frame so a resize is noticed.
	/// Empty when unknown. Only ChangedLines uses it.
	std::function<std::optional<TerminalSize>()> screenSize = stdoutSize;
	/// Receives the text of each frame instead of PRINT, so that diagnostics, which go to PRINT's
	/// sinks, stay out of it. Empty prints the frames. Escape sequences always go to PRINT, so set
	/// it only without tty.
	std::function<void(std::string_view)> write{};
};

/** One rendered frame, or the reason none could be rendered. */
struct RenderedFrame
{
	ze_result_t status{ZE_RESULT_SUCCESS};
	std::string text;
};

using FrameRenderer = std::function<RenderedFrame()>;

/// Rows a frame takes up once printed in a terminal @p columns wide; a line wider than that wraps.
[[nodiscard]] std::size_t rowsNeeded(std::string_view frame, std::size_t columns);

/// Whether every line of @p frame can sit on its own row of @p screen, with a row to spare. A line
/// that wraps shifts every row below it, so lines could no longer be rewritten by row number.
/// True when the size is unknown.
[[nodiscard]] bool fitsScreen(std::string_view frame, const std::optional<TerminalSize> &screen);

/// Renders frames until a stop event or the frame limit; see the definition for the contract.
[[nodiscard]] int runFrameLoop(const FrameRenderer &render, const FrameLoopOptions &opts);

#endif // _FRAME_LOOP_H
