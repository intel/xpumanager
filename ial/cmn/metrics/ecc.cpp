/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * ECC metrics: current ECC mode, correctable/uncorrectable/aggregate hardware error totals,
 * and individual RAS error category counters (reset, programming, driver, cache, non-compute).
 * ecc.mode.current is a Static metric that queries the ECC HAL directly at getter time.
 * All error-count metrics are Live metrics read from the same RAS states as `xpu-smi stats -r`.
 */

#include "ecc.h"
#include "device.h"
#include "metrics_registry.h"
#include "ze_api.h"
#include "zes_api.h"
#include <ras.h>
#include <array>
#include "utility/compat/format.h"
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace metrics::ecc {

bool isHardwareRasCategory(zes_ras_error_category_exp_t category) noexcept
{
	return category != ZES_RAS_ERROR_CATEGORY_EXP_RESET && category != ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS &&
		   category != ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS;
}

std::optional<uint64_t> sumRasErrors(const RasStates &states, RasTypes types, RasCategoryFilter filter) noexcept
{
	std::optional<uint64_t> sum;
	for (const auto &[type, entries] : states) {
		const bool included = types == RasTypes::BOTH ||
							  (types == RasTypes::CORRECTABLE && type == ZES_RAS_ERROR_TYPE_CORRECTABLE) ||
							  (types == RasTypes::UNCORRECTABLE && type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE);
		if (!included) {
			continue;
		}
		for (const auto &entry : entries) {
			if (filter(entry.category)) {
				sum = sum.value_or(0) + entry.errorCount;
			}
		}
	}
	return sum;
}

std::optional<std::string_view> eccModeName(std::optional<zes_device_ecc_state_t> sysmanState,
											std::optional<bool> eccFlag) noexcept
{
	if (sysmanState) {
		switch (*sysmanState) {
		case ZES_DEVICE_ECC_STATE_ENABLED:
			return "Enabled";
		case ZES_DEVICE_ECC_STATE_DISABLED:
			return "Disabled";
		default:
			return "Unavailable";
		}
	}
	if (eccFlag) {
		return *eccFlag ? "Enabled" : "Disabled";
	}
	return std::nullopt;
}

namespace {

/**
 * @brief Reads the current ECC mode, resolved the same way as discovery and config.
 *
 * Uses the state from the ECC HAL and, when that cannot be read, whether the core device
 * properties carry @c ZE_DEVICE_PROPERTY_FLAG_ECC; see eccModeName().
 *
 * @param[in]  d   Device to query.
 * @param[out] out On success, set to @c "Enabled", @c "Disabled", or @c "Unavailable".
 *                 Unchanged on failure.
 * @param      unused Unused metric cache parameter; ECC state is always queried live.
 *
 * @retval ZE_RESULT_SUCCESS                   The ECC mode was written to @p out.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE Neither the ECC HAL nor the core device properties
 *                                             could be read.
 */
ze_result_t eccModeGetter(devInfo &d, MetricValue &out, const MetricCache & /*unused*/)
{
	std::optional<zes_device_ecc_state_t> sysmanState;
	if (auto *ecc = d.dev->getECC(); ecc != nullptr) {
		zes_device_ecc_properties_t props{};
		props.stype = ZES_STRUCTURE_TYPE_DEVICE_ECC_PROPERTIES;
		if (ecc->getState(d.zesDeviceHdl, &props) == ZE_RESULT_SUCCESS) {
			sysmanState = props.currentState;
		}
	}
	std::optional<bool> eccFlag;
	if (!sysmanState) {
		ze_device_properties_t devProps{};
		devProps.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
		if (d.dev->getDevProps(d.deviceHdl, &devProps) == ZE_RESULT_SUCCESS) {
			eccFlag = (devProps.flags & ZE_DEVICE_PROPERTY_FLAG_ECC) != 0;
		}
	}
	const auto name = eccModeName(sysmanState, eccFlag);
	if (!name) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	out = std::string{*name};
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Category filter matching exactly one RAS category.
 *
 * @tparam CAT Category to match
 * @param [in] category Category to test
 * @return true when @p category is @p CAT
 */
template <zes_ras_error_category_exp_t CAT> bool isRasCategory(zes_ras_error_category_exp_t category) noexcept
{
	return category == CAT;
}

/**
 * @brief Getter for a RAS error counter: the TYPES error sets' counts for the categories FILTER selects.
 *
 * Reads the same experimental RAS states as `xpu-smi stats -r`, so both commands report the same counts.
 *
 * @tparam TYPES  Error set types to include.
 * @tparam FILTER Categories to include.
 * @param[in]  d      Device to query.
 * @param[out] out    On success, set to the decimal sum. Unchanged on failure.
 * @param      unused Unused metric cache parameter.
 *
 * @retval ZE_RESULT_SUCCESS                   The sum was written to @p out.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE The device exposes no RAS error sets, or none of them
 *                                             reports a selected category.
 * @retval Other                               Any other error returned while reading the RAS states.
 */
template <RasTypes TYPES, RasCategoryFilter FILTER>
ze_result_t rasErrorGetter(devInfo &d, MetricValue &out, const MetricCache & /*unused*/)
{
	auto *rasHal = d.dev->getRAS();
	if (rasHal == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	RasStates states;
	if (const auto res = rasHal->getErrorsPerTileRasExp(states); res != ZE_RESULT_SUCCESS) {
		return res;
	}
	const auto sum = sumRasErrors(states, TYPES, FILTER);
	if (!sum) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	out = xpum::compat::format("{}", *sum);
	return ZE_RESULT_SUCCESS;
}

constexpr auto ECC_METRICS = std::to_array<QueryMetric>({
	{
		.name = "ecc.mode.current",
		.unit = "",
		.description = "Current ECC state (Enabled/Disabled/Unavailable)",
		.source = MetricSource::Static,
		.groups = MetricGroup::ECC,
		.getter = eccModeGetter,
	},
	{
		.name = "ecc.errors.corrected.aggregate.total",
		.unit = "",
		.description = "Correctable errors across all hardware RAS categories",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = rasErrorGetter<RasTypes::CORRECTABLE, isHardwareRasCategory>,
	},
	{
		.name = "ecc.errors.uncorrected.aggregate.total",
		.unit = "",
		.description = "Uncorrectable errors across all hardware RAS categories",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = rasErrorGetter<RasTypes::UNCORRECTABLE, isHardwareRasCategory>,
	},
	{
		.name = "ecc.errors.aggregate.total",
		.unit = "",
		.description = "Total errors across all hardware RAS categories (correctable + uncorrectable)",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = rasErrorGetter<RasTypes::BOTH, isHardwareRasCategory>,
	},
	{
		.name = "ras.reset",
		.unit = "",
		.description = "GPU reset count",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = rasErrorGetter<RasTypes::BOTH, isRasCategory<ZES_RAS_ERROR_CATEGORY_EXP_RESET>>,
	},
	{
		.name = "ras.programming.errors",
		.unit = "",
		.description = "Programming error count",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = rasErrorGetter<RasTypes::BOTH, isRasCategory<ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS>>,
	},
	{
		.name = "ras.driver.errors",
		.unit = "",
		.description = "Driver error count",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = rasErrorGetter<RasTypes::BOTH, isRasCategory<ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS>>,
	},
	{
		.name = "ras.cache.errors.correctable",
		.unit = "",
		.description = "Correctable cache ECC errors",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = rasErrorGetter<RasTypes::CORRECTABLE, isRasCategory<ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS>>,
	},
	{
		.name = "ras.cache.errors.uncorrectable",
		.unit = "",
		.description = "Uncorrectable cache ECC errors",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = rasErrorGetter<RasTypes::UNCORRECTABLE, isRasCategory<ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS>>,
	},
	{
		.name = "ras.non_compute.errors.correctable",
		.unit = "",
		.description = "Correctable non-compute ECC errors",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = rasErrorGetter<RasTypes::CORRECTABLE, isRasCategory<ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS>>,
	},
	{
		.name = "ras.non_compute.errors.uncorrectable",
		.unit = "",
		.description = "Uncorrectable non-compute ECC errors",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = rasErrorGetter<RasTypes::UNCORRECTABLE, isRasCategory<ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS>>,
	},
	{
		.name = "ras.non_compute.errors.total",
		.unit = "",
		.description = "Non-compute error count (correctable + uncorrectable)",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = rasErrorGetter<RasTypes::BOTH, isRasCategory<ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS>>,
	},
});
} // namespace

std::span<const QueryMetric> getEccMetrics() noexcept { return ECC_METRICS; }

} // namespace metrics::ecc
