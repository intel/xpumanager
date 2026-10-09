/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * ECC / RAS metrics: ECC mode, aggregate error counts, and per-category RAS counters.
 */

#pragma once

#include "metrics_registry.h"
#include "zes_api.h"
#include <ras.h>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace metrics::ecc {

/** @brief Per-error-set-type RAS states, as returned by ras::getErrorsPerTileRasExp(). */
using RasStates = std::map<zes_ras_error_type_t, std::vector<ras_state_exp_t>>;

/** @brief The RAS error sets a counter reads. */
enum class RasTypes : uint8_t
{
	CORRECTABLE,
	UNCORRECTABLE,
	BOTH,
};

/** @brief Selects the RAS error categories a counter adds up. */
using RasCategoryFilter = bool (*)(zes_ras_error_category_exp_t category) noexcept;

/**
 * @brief Whether a RAS category counts hardware errors rather than resets or software-reported errors.
 *
 * @param [in] category Experimental RAS error category
 * @return false for RESET, PROGRAMMING_ERRORS and DRIVER_ERRORS, true for every other category
 */
[[nodiscard]] bool isHardwareRasCategory(zes_ras_error_category_exp_t category) noexcept;

/**
 * @brief Sums RAS error counts over the selected error set types and categories.
 *
 * Counts from every tile are included, as `xpu-smi stats -r` totals them.
 *
 * @param [in] states RAS states to sum
 * @param [in] types  Error set types to include
 * @param [in] filter Categories to include
 * @return The sum, or std::nullopt when no included error set reports an included category
 */
[[nodiscard]] std::optional<uint64_t> sumRasErrors(const RasStates &states, RasTypes types,
												   RasCategoryFilter filter) noexcept;

/**
 * @brief Names the current ECC mode, falling back to the core device ECC flag as discovery and config do.
 *
 * @param [in] sysmanState Current state reported by Sysman, or std::nullopt when Sysman reported none
 * @param [in] eccFlag     Whether the core device properties set ZE_DEVICE_PROPERTY_FLAG_ECC, or std::nullopt
 *                         when they could not be read
 * @return "Enabled", "Disabled" or "Unavailable", or std::nullopt when neither source reported anything
 */
[[nodiscard]] std::optional<std::string_view> eccModeName(std::optional<zes_device_ecc_state_t> sysmanState,
														  std::optional<bool> eccFlag) noexcept;

[[nodiscard]] std::span<const QueryMetric> getEccMetrics() noexcept;

} // namespace metrics::ecc
