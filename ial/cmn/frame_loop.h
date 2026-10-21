/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef _FRAME_LOOP_H
#define _FRAME_LOOP_H

#include <ze_api.h>
#include <chrono>
#include <functional>
#include <string>
#include <string_view>

/** How runFrameLoop() repeats and presents its frames. */
struct FrameLoopOptions
{
	std::chrono::milliseconds interval{0}; ///< Time between frame starts; 0 renders exactly one frame.
	int count = 0;						   ///< Maximum frames; 0 is unlimited. Ignored when interval is 0.
	bool tty = true;					   ///< Draw on the alternate screen, replacing each frame in place.
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

/// Renders frames until a stop event or the frame limit; see the definition for the contract.
[[nodiscard]] int runFrameLoop(const FrameRenderer &render, const FrameLoopOptions &opts);

#endif // _FRAME_LOOP_H
