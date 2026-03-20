/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <fstream>
#include <sstream>
#include <debug.h>
#include "file_io.h"

/**
 * @brief Helper function to read  text file contents, with trailing whitespace trimmed
 *
 * @param[in] path Full path to the file
 * @param[out] content String to store the file contents
 * @return 0 if successful, -1 on failure
 */
int readFile(const std::string &path, std::string &content)
{
	std::ifstream ifs;
	std::stringstream ss;

	ifs.open(path);
	if (!ifs) {
		ERR("read: {} open failed\n", path.c_str());
		ifs.close();
		return -1;
	}
	ss << ifs.rdbuf();
	content.assign(ss.str());

	// Trim trailing whitespace (including newlines)
	content.erase(content.find_last_not_of(" \t\n\r\f\v") + 1);

	ifs.close();
	DBG("read: {} {}\n", path.c_str(), content.c_str());
	return 0;
}

/**
 * @brief Helper function to (over)write files
 *
 * @param[in] path Full path to the file
 * @param[in] content String to write to the file
 * @return 0 if successful, -1 on failure
 */
int writeFile(const std::string &path, const std::string &content)
{
	std::ofstream ofs;

	ofs.open(path, std::ios::out | std::ios::trunc);
	if (!ofs) {
		ERR("write: {} open failed\n", path.c_str());
		ofs.close();
		return -1;
	}
	ofs << content;
	ofs.flush();
	ofs.close();
	return 0;
}

/**
 * @brief Split a string into tokens using a delimiter
 *
 * Utility function to tokenize a string by splitting on the specified delimiter.
 * Empty tokens are filtered out from the result.
 *
 * @param[in] s Input string to split
 * @param[in] delim Delimiter character to split on
 * @return std::vector<std::string> Vector of non-empty tokens
 */
std::vector<std::string> split(const std::string &s, char delim)
{
	std::vector<std::string> result;
	std::stringstream ss(s);
	std::string item;
	while (getline(ss, item, delim)) {
		if (item.size() > 0)
			result.push_back(item);
	}
	return result;
}