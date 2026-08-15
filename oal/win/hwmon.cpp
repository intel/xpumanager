/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <os.h>

#include <map>
#include <string>

// Windows has no sysfs hwmon tree, so there is no PCI-hwmon temperature node to
// discover here. Return an empty map: callers (e.g. the temperature sysfs
// fallback) then find no labels and simply skip the fallback, which is the
// correct behavior on Windows.
std::map<std::string, std::string> getHwmonLabelPaths(const std::string &pciBdf, const std::string &subsystemPrefix)
{
	UNUSED_VAR(pciBdf);
	UNUSED_VAR(subsystemPrefix);
	return {};
}
