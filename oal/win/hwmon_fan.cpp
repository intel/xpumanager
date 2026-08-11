/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <os.h>

#include <cstdint>
#include <map>
#include <string>

// Windows has no sysfs hwmon tree, so there is no PCI-hwmon fanN_input node to
// discover here. Return an empty map: the fan RPM sysfs fallback then finds no
// paths and simply reports the reading as unavailable, which is the correct
// behavior on Windows.
std::map<uint32_t, std::string> getHwmonInputPaths(const std::string &pciBdf, const std::string &subsystemPrefix)
{
	UNUSED_VAR(pciBdf);
	UNUSED_VAR(subsystemPrefix);
	return {};
}
