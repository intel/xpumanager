/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _OAL_LIN_HWMON_INTERNAL_H
#define _OAL_LIN_HWMON_INTERNAL_H

#include <filesystem>
#include <map>
#include <string>
#include <string_view>

// Internal (non-exported) traversal helpers backing getHwmonLabelPaths() on
// Linux. They are declared here -- rather than as file-local statics -- ONLY so
// the oal/lin unit test can drive the sysfs walk against on-disk fixtures
// without touching the real /sys tree. They are NOT part of the public OAL
// surface (os.h) and carry no LIBXPUM_API export.
namespace oal
{
namespace hwmon_detail
{

// Scan ONE hwmon directory: for every strict "<prefix><N>_label" file that opens
// and yields a non-empty label whose sibling "<prefix><N>_input" file also
// opens, record label -> absolute "<prefix><N>_input" path into `out`. Existing
// keys are never overwritten (first match wins). Every stream open is verified.
void scanHwmonDir(const std::filesystem::path &hwmonDir, std::string_view prefix,
				  std::map<std::string, std::string> &out);

// Scan EVERY "hwmon*" subdirectory under `hwmonBase` (the .../hwmon dir),
// accumulating label -> "<prefix><N>_input" mappings across ALL of them; it does
// not stop at the first hwmon dir. Returns the number of hwmon* subdirs visited.
int scanHwmonRoot(const std::filesystem::path &hwmonBase, std::string_view prefix,
				  std::map<std::string, std::string> &out);

} // namespace hwmon_detail
} // namespace oal

#endif
