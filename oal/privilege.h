/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * What the process may do, so a value it cannot read can say what is missing.
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

/** A Linux capability a read can need. */
enum class Capability : uint8_t
{
	SysAdmin, ///< CAP_SYS_ADMIN: PCI configuration space past its first 64 bytes, among others
	Perfmon,  ///< CAP_PERFMON: the engine busyness counters
};

/**
 * @brief The capability's name as the kernel spells it, e.g. "CAP_SYS_ADMIN".
 *
 * @param[in] cap The capability.
 *
 * @retval name The name.
 */
[[nodiscard]] constexpr std::string_view capabilityName(Capability cap) noexcept
{
	return cap == Capability::SysAdmin ? "CAP_SYS_ADMIN" : "CAP_PERFMON";
}

/**
 * @brief Whether the process holds @p cap in its effective set.
 *
 * As the kernel does, CAP_SYS_ADMIN also grants what CAP_PERFMON does.
 *
 * @param[in] cap The capability.
 *
 * @retval true  The process holds it, or the OS has no such capability (Windows).
 * @retval false The process lacks it.
 */
[[nodiscard]] bool hasCapability(Capability cap) noexcept;

/**
 * @brief Whether the process may open @p path for reading and writing, as a device node needs.
 *
 * @param[in] path The file.
 *
 * @retval true  It may, or the check does not apply (Windows).
 * @retval false It may not, or the file does not exist.
 */
[[nodiscard]] bool canReadWrite(const std::string &path) noexcept;
