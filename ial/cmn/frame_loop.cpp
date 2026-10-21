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
 * @brief Renders frames once or repeatedly at a fixed interval.
 *
 * With a zero interval one frame is rendered and printed. Otherwise frames are rendered
 * back to back, each starting one interval after the previous one started, until a stop
 * event arrives or the frame limit is reached. A stop event is SIGINT or SIGTERM. With
 * opts.tty set, the frames are drawn on the alternate screen so each one replaces the last;
 * otherwise they are appended with no escape sequences. The normal screen is restored before
 * any diagnostic is printed, including after a renderer failure and after a stop event.
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
		const auto [status, text] = render();
		if (status != ZE_RESULT_SUCCESS) {
			return reportFrameError(status);
		}
		emit(text);
		return 0;
	}

	int iterations = 0;
	ze_result_t loopResult = ZE_RESULT_SUCCESS;
	{
		const StopSignalScope stopSignals;
		if (opts.tty) {
			PRINT("\033[?1049h"); // enter alternate screen buffer
		}

		while (!stopRequested.load(std::memory_order_relaxed) && (opts.count <= 0 || iterations < opts.count)) {
			// Start the deadline before rendering so the interval includes the time the
			// renderer spends sampling.
			const auto deadline = std::chrono::steady_clock::now() + opts.interval;
			const auto [status, text] = render();
			if (status != ZE_RESULT_SUCCESS) {
				loopResult = status;
				break;
			}
			if (opts.tty) {
				PRINT("\033[H\033[J"); // cursor home + erase to end, clearing stale rows
			}
			emit(text);
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

		if (opts.tty) {
			PRINT("\033[?1049l"); // restore normal screen buffer before any diagnostic output
		}
	}

	return loopResult == ZE_RESULT_SUCCESS ? 0 : reportFrameError(loopResult);
}
