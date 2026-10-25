/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "watchdog_os.h"

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <system_error>
#include <windows.h>
#include <tlhelp32.h>

namespace {

// Longest one raw write waits for a stalled reader. The expiry path writes only a few times, so
// this bounds how late the exit is.
constexpr auto WRITE_WAIT = std::chrono::milliseconds{250};
constexpr auto WRITE_RETRY = std::chrono::milliseconds{10};

// A stream whose last write timed out is not waited on again; a write that completes clears this.
std::atomic<bool> stdoutStalled{false}; // NOLINT(*-avoid-non-const-global-variables)
std::atomic<bool> stderrStalled{false}; // NOLINT(*-avoid-non-const-global-variables)

/**
 * @brief Writes @p text to stdout or stderr, giving up on a pipe reader that does not drain it.
 *
 * Waits at most WRITE_WAIT in total, or not at all after a timed-out write to the same pipe.
 *
 * @param[in] stdHandle  STD_OUTPUT_HANDLE or STD_ERROR_HANDLE.
 * @param[in] text       Bytes to write.
 */
void writeAll(DWORD stdHandle, std::string_view text) noexcept
{
	const HANDLE h = GetStdHandle(stdHandle);
	if (h == INVALID_HANDLE_VALUE || h == nullptr || text.empty()) {
		return;
	}
	std::atomic<bool> &stalled = stdHandle == STD_OUTPUT_HANDLE ? stdoutStalled : stderrStalled;
	// WriteFile on a pipe whose reader stopped waits for room indefinitely. In PIPE_NOWAIT mode
	// it returns at once with what fitted, so the wait can be bounded; the mode is put back after.
	// A console cannot be made non-blocking, so a console paused by a selection still blocks.
	DWORD noWait = PIPE_READMODE_BYTE | PIPE_NOWAIT;
	const bool pipeNoWait =
		GetFileType(h) == FILE_TYPE_PIPE && SetNamedPipeHandleState(h, &noWait, nullptr, nullptr) != FALSE;
	const auto waitFor = pipeNoWait && stalled.load() ? std::chrono::milliseconds::zero() : WRITE_WAIT;
	const auto deadline = std::chrono::steady_clock::now() + waitFor;
	while (!text.empty()) {
		DWORD written = 0;
		if (WriteFile(h, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) == FALSE) {
			break;
		}
		if (written == 0) {
			if (!pipeNoWait || std::chrono::steady_clock::now() >= deadline) {
				stalled.store(pipeNoWait);
				break;
			}
			Sleep(static_cast<DWORD>(WRITE_RETRY.count()));
			continue;
		}
		text.remove_prefix(written);
		if (text.empty()) {
			stalled.store(false);
		}
	}
	if (pipeNoWait) {
		DWORD wait = PIPE_READMODE_BYTE | PIPE_WAIT;
		SetNamedPipeHandleState(h, &wait, nullptr, nullptr);
	}
}

} // namespace

/**
 * @brief Writes @p text to stdout with WriteFile, retrying partial writes; on a pipe for at most 250 ms.
 * @param[in] text  Bytes to write.
 */
void writeStdoutRaw(std::string_view text) noexcept { writeAll(STD_OUTPUT_HANDLE, text); }

/**
 * @brief Writes @p text to stderr with WriteFile, retrying partial writes; on a pipe for at most 250 ms.
 * @param[in] text  Bytes to write.
 */
void writeStderrRaw(std::string_view text) noexcept { writeAll(STD_ERROR_HANDLE, text); }

/**
 * @brief Writes "  thread <tid>" for every thread of this process to stderr.
 */
void writeThreadReport() noexcept
{
	const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (snapshot == INVALID_HANDLE_VALUE) {
		return;
	}
	const DWORD pid = GetCurrentProcessId();
	THREADENTRY32 entry{};
	entry.dwSize = sizeof(entry);
	for (BOOL ok = Thread32First(snapshot, &entry); ok != FALSE; ok = Thread32Next(snapshot, &entry)) {
		if (entry.th32OwnerProcessID != pid) {
			continue;
		}
		std::array<char, 16> digits{};
		const auto [end, ec] = std::to_chars(digits.data(), digits.data() + digits.size(), entry.th32ThreadID);
		if (ec != std::errc{}) {
			continue;
		}
		writeAll(STD_ERROR_HANDLE, "  thread ");
		writeAll(STD_ERROR_HANDLE, std::string_view{digits.data(), static_cast<std::size_t>(end - digits.data())});
		writeAll(STD_ERROR_HANDLE, "\n");
	}
	CloseHandle(snapshot);
}
