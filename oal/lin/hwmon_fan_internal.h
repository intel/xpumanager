/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _OAL_LIN_HWMON_FAN_INTERNAL_H
#define _OAL_LIN_HWMON_FAN_INTERNAL_H

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>

// Internal (non-exported) traversal helpers backing getHwmonInputPaths() on
// Linux. They are declared here -- rather than as file-local statics -- ONLY so
// the oal/lin unit test can drive the sysfs walk against on-disk fixtures
// without touching the real /sys tree. They are NOT part of the public OAL
// surface (os.h) and carry no LIBXPUM_API export.
//
// Fan hwmon differs from temperature hwmon: fanN_input is published DIRECTLY,
// with no fanN_label sibling, so the walk keys by the parsed sensor index rather
// than by a label string. This header is fan-specific (mirrors the temperature
// patch's hwmon_internal.h) so the two independent branches never collide.
namespace oal
{
namespace hwmon_fan_detail
{

// Scan ONE hwmon directory: for every strict "<prefix><N>_input" file (N >= 1)
// record (N - 1) -> absolute "<prefix>N_input" path into `out`. The index is
// made zero-based ("fan1_input" -> 0). Names are validated strictly via
// starts_with/ends_with + std::from_chars over the digit span; near-miss names
// ("fan_input", "fanX_input", "fan1_label", "temp1_input") and indices that
// would overflow a uint32 once made zero-based are rejected. Existing keys are
// never overwritten (first match wins).
void scanHwmonInputDir(const std::filesystem::path &hwmonDir, std::string_view prefix,
					   std::map<uint32_t, std::string> &out);

// Walk every "hwmon*" subdirectory under `hwmonBase` (the .../hwmon dir) and
// accept the FIRST one that yields a non-empty "<prefix>N_input" mapping into
// `out`, stopping there. Returns the number of hwmon* subdirs visited. A device
// can expose several hwmon nodes (e.g. one for temperatures, one for fans) and
// the fan node is not guaranteed to be the first, so a productive later node is
// still found; an unproductive node never shadows it.
int scanHwmonInputRoot(const std::filesystem::path &hwmonBase, std::string_view prefix,
					   std::map<uint32_t, std::string> &out);

} // namespace hwmon_fan_detail
} // namespace oal

#endif
