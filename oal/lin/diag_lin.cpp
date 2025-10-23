/*
 * Copyright © 2025 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <iostream>
#include <fstream>
#include <array>
#include <filesystem>
#include <dirent.h>
#include <vector>
#include <debug.h>
#include <cstring>
#include <unistd.h>
#include <cerrno>
#include <algorithm>

/**
 * @brief Checks if the entry name is a renderD related device entry
 *
 * This function determines whether the provided entry name is related
 * to a renderD device entry
 *
 * @param entry_name Pointer to entry string
 * @return true if entry_name is related to renderD, false otherwise
 */
bool countDevEntry(const std::string &entry_name)
{
	constexpr std::string_view prefix = "renderD";

	if (!entry_name.starts_with(prefix)) {
		return false;
	}

	return std::all_of(entry_name.begin() + prefix.size(), entry_name.end(), [](char ch) { return std::isdigit(ch); });
}

/**
 * @brief check Permission
 *
 * This function check access permission for renderD related folders under /dev/dri.
 * @param null
 * @return true if has the access , false otherwise
 */
bool checkPermission()
{
	constexpr std::string_view dir_name = "/dev/dri";

	std::error_code ec;
	if (!std::filesystem::exists(dir_name, ec) || ec) {
		return false;
	}

	try {
		for (const auto &entry : std::filesystem::directory_iterator(dir_name, ec)) {
			if (ec) {
				return false;
			}

			const std::string entry_name = entry.path().filename().string();
			if (countDevEntry(entry_name)) {
				if (access(entry.path().c_str(), R_OK) != 0) {
					return false;
				}
			}
		}
		return true;
	} catch (const std::filesystem::filesystem_error &) {
		return false;
	}
}
/**
 * @brief check process exclusive
 *
 * This function check process exclusive for a specific process under /proc.
 * @param uint32_t processId
 * @return true if the process is ready, false otherwise
 */
bool checkProcessExclusive(uint32_t processId)
{
	std::ifstream file("/proc/" + std::to_string(processId) + "/cmdline");
	if (!file.good()) {
		return false;
	} else {
		return true;
	}
}
