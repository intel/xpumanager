/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "os.h"
#include "progress_block.h"
#include <algorithm>
#include <iostream>
#include <optional>

namespace {

struct progressLine
{
	int devIndex;
	uint32_t progress;
	bool hasLabel;
	std::string label;
};

// The active block of progress bars, one entry per line, top to bottom. Guarded by progressPrintMutex.
std::vector<std::optional<progressLine>> gBlock;

} // namespace

/**
 * @brief Text of one progress bar line
 *
 * Every bar must take exactly one terminal row: setProgress(), progressClearBlock() and the redraw in
 * progressPrintAbove() all count one row per bar, and a line that wraps would throw them off. So the
 * bar shrinks to the terminal width, and the text is cut short if even that does not fit. When the
 * width is not known (stdout is not a terminal) the bar is 100 characters, one per percent.
 *
 * @param devIndex Device index shown in the line
 * @param progress Percentage, 0 to 100
 * @param label Optional text shown after the device index
 *
 * @return std::string The line, without a line break
 */
std::string progressLineText(int devIndex, uint32_t progress, const char *label)
{
	progress = std::min<uint32_t>(progress, 100);
	std::string head = "Firmware progress for device " + std::to_string(devIndex);
	if (label != nullptr) {
		head += " [";
		head += label;
		head += "]";
	}
	head += ": ";
	const std::string tail = " " + std::to_string(progress) + "%";

	const int columns = progressTerminalColumns();
	// One column short of the width: a line that fills the last column can leave the cursor on the next row
	const size_t usable = columns > 1 ? static_cast<size_t>(columns) - 1 : 0;
	size_t barWidth = 100;
	if (columns > 0) {
		barWidth = (usable > head.size() + tail.size()) ? std::min<size_t>(100, usable - head.size() - tail.size()) : 0;
	}

	std::string text = head;
	text.append(progress * barWidth / 100, '#');
	text += tail;
	if (columns > 0 && text.size() > usable) {
		text.resize(usable);
	}
	return text;
}

/**
 * @brief Record what a progress bar shows, so progressPrintAbove() can draw the whole block again
 *
 * Calls for a block other than the active one (totalThreads not matching it) are ignored. The
 * caller holds progressPrintMutex.
 *
 * @param devIndex Device index shown in the line
 * @param lineNum Line of the bar in the block, counted from the top
 * @param totalThreads Number of bars in the block
 * @param progress Percentage, 0 to 100
 * @param label Optional text shown after the device index
 */
void progressRemember(int devIndex, int lineNum, int totalThreads, uint32_t progress, const char *label)
{
	if (totalThreads != static_cast<int>(gBlock.size()) || lineNum < 0 || lineNum >= totalThreads) {
		return;
	}
	gBlock[static_cast<size_t>(lineNum)] =
		progressLine{devIndex, progress, label != nullptr, label != nullptr ? label : ""};
}

/**
 * @brief Start a block of progress bars below the current output
 *
 * Reserves one line per bar. setProgress() draws bar n on line n of the block, counted from the
 * top, and the block always stays directly above the cursor.
 *
 * @param lines Number of progress bars in the block
 */
void progressBegin(int lines)
{
	std::lock_guard<std::mutex> lock(progressPrintMutex);
	gBlock.assign(static_cast<size_t>(std::max(lines, 0)), std::nullopt);
	for (int i = 0; i < lines; i++) {
		std::cout << '\n';
	}
	std::cout.flush();
}

/**
 * @brief End the current block of progress bars; output after it is no longer kept below anything
 */
void progressEnd()
{
	std::lock_guard<std::mutex> lock(progressPrintMutex);
	gBlock.clear();
}

/**
 * @brief Print something without disturbing the progress bars
 *
 * Other output written while bars are on screen would otherwise be overwritten by the next bar
 * update, which redraws relative to the cursor. Here the block is cleared, write() prints in its
 * place, and the bars are drawn again below what it printed.
 *
 * @param write Prints the output, to stdout or stderr
 * @param endsLine False when the output does not end with a line break; one is then added before
 *                 the bars are redrawn, and only then
 */
void progressPrintAbove(const std::function<void()> &write, bool endsLine)
{
	std::lock_guard<std::mutex> lock(progressPrintMutex);
	if (gBlock.empty()) {
		write();
		return;
	}

	std::cout.flush();
	progressClearBlock(static_cast<int>(gBlock.size()));
	write();
	std::cerr.flush();
	if (!endsLine) {
		std::cout << '\n';
	}
	for (const auto &line : gBlock) {
		if (line) {
			std::cout << progressLineText(line->devIndex, line->progress,
										  line->hasLabel ? line->label.c_str() : nullptr);
		}
		std::cout << '\n';
	}
	std::cout.flush();
}
