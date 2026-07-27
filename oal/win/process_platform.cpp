/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "process_platform.h"
#include <vector>

// Windows has no /proc/fdinfo equivalent; zesDeviceProcessesGetState values are used as-is.

std::vector<std::string> deviceNodesForBdf([[maybe_unused]] const std::string &bdf,
										   [[maybe_unused]] const std::filesystem::path &sysRoot)
{
	return {};
}

FdinfoResult vramFromFdinfo([[maybe_unused]] uint32_t pid, [[maybe_unused]] const std::vector<std::string> &deviceNodes,
							[[maybe_unused]] const std::string &procRoot,
							[[maybe_unused]] std::unordered_set<uint64_t> *globalSeenIds)
{
	return {};
}

void fixProcessMemSize([[maybe_unused]] const std::string &bdf,
					   [[maybe_unused]] std::vector<zes_process_state_t> *processList)
{}
