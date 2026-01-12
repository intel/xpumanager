/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef FILE_IO_H
#define FILE_IO_H

#include <string>
#include <vector>
#include <filesystem>

int readFile(const std::string &path, std::string &content);
int writeFile(const std::string &path, const std::string &content);
inline bool fileExists(const std::string &s) { return std::filesystem::exists(s); }
std::vector<std::string> split(const std::string &s, char delim);

#endif /* FILE_IO_H */