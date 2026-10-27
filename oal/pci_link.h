/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * PCIe link facts the OS knows and Level Zero does not report: the root port's link
 * capability, the AER error counters and the AtomicOp capabilities.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

/** The fastest and widest link a PCIe port supports. Each part is empty when unknown. */
struct PcieLinkMax
{
	std::optional<int> generation; ///< e.g. 5 for 32 GT/s
	std::optional<int> width;	   ///< lanes
};

/**
 * @brief Reads the link capability of the root port a PCI function sits under.
 *
 * The root port is the CPU's end of the path to the function; a switch in between can limit the
 * link further.
 *
 * @param[in] bdf PCI address of the function, in canonical DDDD:BB:DD.F form.
 *
 * @retval link Each part as the root port reports it, empty where it does not; both empty when
 *              @p bdf is malformed, has no root port above it, or the OS has no such data.
 */
[[nodiscard]] PcieLinkMax getRootPortMaxLink(std::string_view bdf);

/** PCIe Advanced Error Reporting counts of one function since it was enumerated. */
struct PcieAerCounts
{
	uint64_t correctable = 0;	 ///< All correctable errors
	uint64_t nonFatal = 0;		 ///< Uncorrectable non-fatal errors
	uint64_t fatal = 0;			 ///< Uncorrectable fatal errors
	uint64_t replayRollover = 0; ///< REPLAY_NUM rollovers, a correctable error
	uint64_t replayTimeout = 0;	 ///< Replay timer timeouts, a correctable error
};

/**
 * @brief Reads the AER error counters of a PCI function.
 *
 * @param[in] bdf PCI address of the function, in canonical DDDD:BB:DD.F form.
 *
 * @retval counts  The function's counters.
 * @retval nullopt @p bdf is malformed, the function has no AER counters, or the OS has no such data.
 */
[[nodiscard]] std::optional<PcieAerCounts> getPcieAerCounts(std::string_view bdf);

/** The AtomicOp support a PCIe function advertises in Device Capabilities 2 and Control 2. */
struct PcieAtomicOps
{
	bool completer32 = false;	   ///< Completes 32-bit FetchAdd, Swap and CAS requests
	bool completer64 = false;	   ///< Completes 64-bit FetchAdd, Swap and CAS requests
	bool completer128Cas = false;  ///< Completes 128-bit CAS requests
	bool requesterEnabled = false; ///< May send AtomicOp requests
};

/**
 * @brief Reads the AtomicOp capabilities of a PCI function from its configuration space.
 *
 * @param[in] bdf PCI address of the function, in canonical DDDD:BB:DD.F form.
 *
 * @retval ops     The function's AtomicOp support.
 * @retval nullopt @p bdf is malformed, the function has no PCI Express capability, or its
 *                 capabilities cannot be read (on Linux beyond the first 64 bytes needs root).
 */
[[nodiscard]] std::optional<PcieAtomicOps> getPcieAtomicOps(std::string_view bdf);
