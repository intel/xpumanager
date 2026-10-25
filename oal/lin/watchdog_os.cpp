/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "watchdog_os.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <initializer_list>
#include <poll.h>
#include <span>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <system_error>
#include <unistd.h>

namespace {

// Longest one raw write waits for a stalled reader: a full pipe nobody drains, or a terminal
// stopped with ^S. The expiry path writes only a few times, so this bounds how late the exit is.
constexpr auto WRITE_WAIT = std::chrono::milliseconds{250};

// A stream whose last write timed out is not waited on again, so the thread report does not
// wait once per line; a write that completes clears this.
std::array<std::atomic<bool>, 3> streamStalled{}; // NOLINT(*-avoid-non-const-global-variables)

/**
 * @brief Opens a private non-blocking description of stdout or stderr when it is a pipe or terminal.
 *
 * A write to a blocking pipe or terminal waits for room, however long that takes. Setting
 * O_NONBLOCK on the inherited descriptor would also change it for every other process sharing
 * it, so pipes and terminals are reopened as a private non-blocking description instead.
 * Regular files are not reopened: the new description would start at offset 0.
 *
 * @param[in] fd  STDOUT_FILENO or STDERR_FILENO.
 *
 * @retval >=0 The reopened descriptor; the caller closes it.
 * @retval -1  @p fd is not a pipe or terminal, or the reopen failed.
 */
[[nodiscard]] int openNonBlocking(int fd) noexcept
{
	struct stat st = {};
	if (::fstat(fd, &st) != 0) {
		return -1;
	}
	if (!S_ISFIFO(st.st_mode) && !(S_ISCHR(st.st_mode) && ::isatty(fd) == 1)) {
		return -1;
	}
	const char *path = fd == STDOUT_FILENO ? "/proc/self/fd/1" : "/proc/self/fd/2";
	return ::open(path, O_WRONLY | O_NONBLOCK | O_NOCTTY | O_CLOEXEC);
}

/**
 * @brief Writes @p text to stdout or stderr, giving up on a reader that does not drain it.
 *
 * Waits at most WRITE_WAIT in total, or not at all after a timed-out write. Writes go out in
 * chunks of PIPE_BUF so that, should the reopen fail, a blocking write after POLLOUT still fits
 * the room poll reported on a pipe.
 *
 * @param[in] fd    STDOUT_FILENO or STDERR_FILENO.
 * @param[in] text  Bytes to write.
 */
void writeAll(int fd, std::string_view text) noexcept
{
	if (text.empty()) {
		return;
	}
	std::atomic<bool> &stalled = streamStalled[static_cast<std::size_t>(fd)];
	const int reopened = openNonBlocking(fd);
	const int out = reopened >= 0 ? reopened : fd;
	const auto deadline =
		std::chrono::steady_clock::now() + (stalled.load() ? std::chrono::milliseconds::zero() : WRITE_WAIT);
	while (!text.empty()) {
		const auto left =
			std::max(std::chrono::ceil<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()),
					 std::chrono::milliseconds::zero());
		pollfd ready{.fd = out, .events = POLLOUT, .revents = 0};
		const int polled = ::poll(&ready, 1, static_cast<int>(left.count()));
		if (polled < 0 && errno == EINTR) {
			continue;
		}
		if (polled == 0) {
			stalled.store(true);
			break;
		}
		if (polled < 0 || (ready.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
			break;
		}
		const ssize_t written = ::write(out, text.data(), std::min<std::size_t>(text.size(), PIPE_BUF));
		if (written < 0) {
			if (errno == EINTR) {
				continue;
			}
			// A terminal can report POLLOUT yet take nothing; poll alone would then spin.
			if ((errno == EAGAIN || errno == EWOULDBLOCK) && left > std::chrono::milliseconds::zero()) {
				continue;
			}
			stalled.store(errno == EAGAIN || errno == EWOULDBLOCK);
			break;
		}
		text.remove_prefix(static_cast<std::size_t>(written));
		if (text.empty()) {
			stalled.store(false);
		}
	}
	if (reopened >= 0) {
		::close(reopened);
	}
}

/**
 * @brief Reads a small /proc file into @p buf without allocating.
 *
 * @param[in]  dirFd  Directory the path is relative to.
 * @param[in]  path   File to read.
 * @param[out] buf    Receives the contents, cut to its size.
 *
 * @retval The bytes read, without a trailing newline; empty when the file cannot be read.
 */
[[nodiscard]] std::string_view readSmall(int dirFd, const char *path, std::span<char> buf) noexcept
{
	const int fd = ::openat(dirFd, path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		return {};
	}
	const ssize_t n = ::read(fd, buf.data(), buf.size());
	::close(fd);
	if (n <= 0) {
		return {};
	}
	std::string_view text{buf.data(), static_cast<std::size_t>(n)};
	while (!text.empty() && (text.back() == '\n' || text.back() == '\0')) {
		text.remove_suffix(1);
	}
	return text;
}

/**
 * @brief Extracts the thread state, field 3, from a /proc/<tid>/stat line.
 *
 * The command name (field 2) may contain spaces and parentheses, so the state is found after
 * the last ')'.
 *
 * @param[in] stat  Contents of the stat file.
 *
 * @retval The one-letter state, such as "S" or "D"; "?" when the line cannot be parsed.
 */
[[nodiscard]] std::string_view stateFromStat(std::string_view stat) noexcept
{
	const std::size_t close = stat.rfind(')');
	if (close == std::string_view::npos || close + 2 >= stat.size()) {
		return "?";
	}
	return stat.substr(close + 2, 1);
}

/**
 * @brief Writes one thread's id, state and wait channel to stderr as one line.
 *
 * @param[in] taskFd  Open /proc/self/task directory.
 * @param[in] tid     The thread's directory name in it.
 */
void reportThread(int taskFd, const char *tid) noexcept
{
	std::array<char, 64> path{};
	std::array<char, 512> statBuf{};
	std::array<char, 128> wchanBuf{};
	const std::size_t tidLen = std::strlen(tid);
	if (tidLen + sizeof("/wchan") > path.size()) {
		return;
	}
	std::memcpy(path.data(), tid, tidLen);
	std::memcpy(path.data() + tidLen, "/stat", sizeof("/stat"));
	const std::string_view state = stateFromStat(readSmall(taskFd, path.data(), statBuf));
	std::memcpy(path.data() + tidLen, "/wchan", sizeof("/wchan"));
	std::string_view wchan = readSmall(taskFd, path.data(), wchanBuf);
	// A running thread has no wait channel; the kernel reports "0".
	if (wchan.empty() || wchan == "0") {
		wchan = "-";
	}

	// One write per line, so a stalled stream costs one wait for the whole report.
	std::array<char, 256> line{};
	std::size_t used = 0;
	for (const std::string_view part : {std::string_view{"  thread "}, std::string_view{tid, tidLen},
										std::string_view{" state "}, state, std::string_view{" wchan "}, wchan}) {
		const std::size_t n = std::min(part.size(), line.size() - 1 - used);
		std::memcpy(line.data() + used, part.data(), n);
		used += n;
	}
	line[used++] = '\n';
	writeAll(STDERR_FILENO, std::string_view{line.data(), used});
}

} // namespace

/**
 * @brief Writes @p text to stdout with write(2), retrying partial writes, for at most 250 ms.
 * @param[in] text  Bytes to write.
 */
void writeStdoutRaw(std::string_view text) noexcept { writeAll(STDOUT_FILENO, text); }

/**
 * @brief Writes @p text to stderr with write(2), retrying partial writes, for at most 250 ms.
 * @param[in] text  Bytes to write.
 */
void writeStderrRaw(std::string_view text) noexcept { writeAll(STDERR_FILENO, text); }

/**
 * @brief Writes "  thread <tid> state <S> wchan <symbol>" for every thread to stderr.
 *
 * Uses getdents64 on /proc/self/task into a stack buffer because opendir() allocates.
 */
void writeThreadReport() noexcept
{
	const int taskFd = ::open("/proc/self/task", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (taskFd < 0) {
		return;
	}
	alignas(dirent64) std::array<char, 4096> buf{};
	while (true) {
		const long n = ::syscall(SYS_getdents64, taskFd, buf.data(), buf.size());
		if (n <= 0) {
			break;
		}
		for (long offset = 0; offset < n;) {
			// getdents64(2) fills the buffer with variable-length dirent64 records.
			const auto *entry = reinterpret_cast<const dirent64 *>(buf.data() + offset);
			if (entry->d_name[0] != '.') {
				reportThread(taskFd, entry->d_name);
			}
			offset += entry->d_reclen;
		}
	}
	::close(taskFd);
}
