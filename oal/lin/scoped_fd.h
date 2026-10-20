/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <system_error>
#include <utility>

/**
 * @brief Minimal RAII owner for a file descriptor, so that no early return can leak it
 */
struct ScopedFd
{
	int fd = -1;

	ScopedFd() noexcept = default;
	explicit ScopedFd(int rawFd) noexcept : fd(rawFd) {}
	~ScopedFd() noexcept
	{
		if (fd >= 0) {
			close(fd);
		}
	}
	ScopedFd(const ScopedFd &) = delete;
	ScopedFd &operator=(const ScopedFd &) = delete;
	ScopedFd(ScopedFd &&o) noexcept : fd(std::exchange(o.fd, -1)) {}
	ScopedFd &operator=(ScopedFd &&other) noexcept
	{
		if (this != &other) {
			if (fd >= 0) {
				close(fd);
			}
			fd = std::exchange(other.fd, -1);
		}
		return *this;
	}
	[[nodiscard]] bool valid() const noexcept { return fd >= 0; }
	[[nodiscard]] int get() const noexcept { return fd; }
	int release() noexcept { return std::exchange(fd, -1); }
};

/**
 * @brief Returns a human-readable message for an errno value
 *
 * std::system_category() is thread safe, unlike strerror().
 *
 * @param[in] err errno value
 * @retval The message for @p err
 */
[[nodiscard]] inline std::string errnoText(int err) { return std::system_category().message(err); }

/**
 * @brief Checks that an open file is of the given type, owned by @p owner and not writable by group or others
 *
 * Checked through the descriptor, so the answer is about the object actually opened rather than
 * whatever a path names now.
 *
 * @param[in] fd    Open descriptor to check
 * @param[in] type  Expected file type bits (S_IFDIR, S_IFREG, ...)
 * @param[in] owner Expected owning user
 *
 * @retval true  @p fd is a @p type owned by @p owner that only its owner can write
 * @retval false fstat() failed, or the type, owner or mode do not match
 */
[[nodiscard]] inline bool isPrivateTo(int fd, mode_t type, uid_t owner)
{
	struct stat st = {};
	return fstat(fd, &st) == 0 && (st.st_mode & S_IFMT) == type && st.st_uid == owner &&
		   (st.st_mode & (S_IWGRP | S_IWOTH)) == 0;
}
