/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * $PATH executable lookup, split out of lin.cpp so its logic (field
 * splitting, empty-field-as-cwd, and the regular-file guard) can be unit
 * tested standalone without linking the rest of the OS abstraction layer.
 */

#include "lin.h"
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <unistd.h>

/**
 * @brief Reports whether an executable named @p name is reachable via $PATH.
 *
 * Answers the same question as `command -v <name>` for an external program,
 * but without spawning a shell: it walks $PATH and tests each candidate with
 * access(X_OK). A @p name that contains a '/' is treated as a path and tested
 * directly rather than searched for.
 *
 * @param name Program name or path.
 * @return true if an executable is found, false otherwise.
 */
bool isExecutableInPath(const std::string &name)
{
	if (name.empty()) {
		return false;
	}
	// A name containing a slash is a path, not a PATH lookup. It still has to be
	// a regular file: a directory carries the search (execute) bit and would
	// otherwise pass access(X_OK), the same way the PATH-search branch rejects
	// directories below.
	if (name.find('/') != std::string::npos) {
		std::error_code ec;
		return access(name.c_str(), X_OK) == 0 && std::filesystem::is_regular_file(name, ec);
	}

	const char *pathEnv = secure_getenv("PATH");
	if (pathEnv == nullptr) {
		return false;
	}

	const std::string path = pathEnv;
	std::string::size_type start = 0;
	while (start <= path.size()) {
		const std::string::size_type end = path.find(':', start);
		const std::string dir = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
		// An empty field means the current directory, per POSIX.
		const std::string candidate = (dir.empty() ? std::string(".") : dir) + "/" + name;
		std::error_code ec;
		if (access(candidate.c_str(), X_OK) == 0 && std::filesystem::is_regular_file(candidate, ec)) {
			return true;
		}
		if (end == std::string::npos) {
			break;
		}
		start = end + 1;
	}
	return false;
}
