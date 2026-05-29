/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * ECC metrics: current ECC mode, correctable/uncorrectable/aggregate cache error totals,
 * and individual RAS error category counters (reset, programming, driver, cache, non-compute).
 * ecc.mode.current is a Static metric that queries the ECC HAL directly at getter time.
 * All error-count metrics are Live metrics queried from the RAS HAL.
 */

#include "ecc.h"
#include "device.h"
#include "metrics_registry.h"
#include "ze_api.h"
#include "zes_api.h"
#include <ras.h>
#include <array>
#include <format>
#include <span>

namespace metrics::ecc {

namespace {

/**
 * @brief Reads the current ECC enable/disable state from the device ECC HAL.
 *
 * Queries the ECC HAL for the current device state and maps it to a human-readable
 * string: @c "Enabled", @c "Disabled", or @c "Unavailable" for any unrecognised state.
 *
 * @param[in]  d   Device to query. Must have been initialised; returns
 *                 @c ZE_RESULT_ERROR_UNSUPPORTED_FEATURE if the ECC HAL sub-object
 *                 is @c nullptr.
 * @param[out] out On success, set to @c "Enabled", @c "Disabled", or @c "Unavailable".
 *                 Unchanged on failure.
 * @param      unused Unused metric cache parameter; ECC state is always queried live.
 *
 * @retval ZE_RESULT_SUCCESS               The ECC state was read and written to @p out.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE The ECC HAL sub-object is @c nullptr.
 * @retval Other                           Any error code propagated from the ECC HAL
 *                                         @c getState() call.
 *
 * @throws None — all HAL calls return @c ze_result_t error codes; no exceptions
 *                are thrown by this function.
 */
ze_result_t eccModeGetter(devInfo &d, MetricValue &out, const MetricCache & /*unused*/)
{
	auto *ecc = d.dev->getECC();
	if (ecc == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	zes_device_ecc_properties_t props{};
	props.stype = ZES_STRUCTURE_TYPE_DEVICE_ECC_PROPERTIES;
	auto const r = ecc->getState(d.zesDeviceHdl, &props);
	if (r != ZE_RESULT_SUCCESS) {
		return r;
	}
	switch (props.currentState) {
	case ZES_DEVICE_ECC_STATE_ENABLED:
		out = "Enabled";
		break;
	case ZES_DEVICE_ECC_STATE_DISABLED:
		out = "Disabled";
		break;
	default:
		out = "Unavailable";
		break;
	}
	return ZE_RESULT_SUCCESS;
}

// Sum only the cache-error category for correctable/uncorrectable typed totals.
// The HAL only type-filters cache errors; including other categories would make
// both totals identical (and therefore misleading).
/**
 * @brief Sums RAS errors for a specific cache-error type (correctable or uncorrectable).
 *
 * Queries the RAS HAL for the @c ZES_RAS_ERROR_CAT_CACHE_ERRORS category filtered by
 * @p cacheErrorType. Only cache errors are type-filtered; including other categories
 * would make correctable and uncorrectable totals identical and therefore misleading.
 *
 * @param[in]  d              Device to query. Returns @c ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
 *                            if the RAS HAL sub-object is @c nullptr.
 * @param[in]  cacheErrorType The error type filter: @c ZES_RAS_ERROR_TYPE_CORRECTABLE
 *                            or @c ZES_RAS_ERROR_TYPE_UNCORRECTABLE.
 * @param[out] out            On success, set to the decimal string representation of
 *                            the error count. Unchanged on failure.
 *
 * @retval ZE_RESULT_SUCCESS               The count was read and written to @p out.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE The RAS HAL sub-object is @c nullptr.
 * @retval Other                           Any error code propagated from the RAS HAL
 *                                         @c getErrors() call.
 *
 * @throws None — all HAL calls return @c ze_result_t error codes; no exceptions
 *                are thrown by this function.
 */
ze_result_t aggregateRasErrors(devInfo &d, zes_ras_error_type_t cacheErrorType, MetricValue &out)
{
	auto *ras = d.dev->getRAS();
	if (ras == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	uint64_t n = 0;
	auto const res = ras->getErrors(ZES_RAS_ERROR_CAT_CACHE_ERRORS, cacheErrorType, &n);
	if (res == ZE_RESULT_SUCCESS) {
		out = std::format("{}", n);
	}
	return res;
}

// Sum all categories for the untyped grand total, querying correctable and
// uncorrectable separately to avoid the undefined ZES_RAS_ERROR_TYPE_FORCE_UINT32 sentinel.
// Categories that return UNSUPPORTED_FEATURE for one type (e.g. RESET is always
// uncorrectable) are treated as zero for that type so accumulation continues.
ze_result_t aggregateAllRasErrors(devInfo &d, MetricValue &out)
{
	auto *ras = d.dev->getRAS();
	if (ras == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	static constexpr auto cats =
		std::to_array<zes_ras_error_cat_t>({ZES_RAS_ERROR_CAT_CACHE_ERRORS, ZES_RAS_ERROR_CAT_COMPUTE_ERRORS,
											ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS, ZES_RAS_ERROR_CAT_DISPLAY_ERRORS});
	uint64_t total = 0;
	for (const auto cat : cats) {
		uint64_t corr = 0, uncorr = 0;
		const auto rc = ras->getErrors(cat, ZES_RAS_ERROR_TYPE_CORRECTABLE, &corr);
		if (rc != ZE_RESULT_SUCCESS && rc != ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
			return rc;
		}
		const auto ru = ras->getErrors(cat, ZES_RAS_ERROR_TYPE_UNCORRECTABLE, &uncorr);
		if (ru != ZE_RESULT_SUCCESS && ru != ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
			return ru;
		}
		total += corr + uncorr;
	}
	out = std::format("{}", total);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Getter for @c ecc.errors.corrected.aggregate.total — correctable cache ECC errors.
 *
 * Delegates to aggregateRasErrors() with @c ZES_RAS_ERROR_TYPE_CORRECTABLE.
 *
 * @param[in]  d      Device to query.
 * @param[out] out    On success, set to the decimal string of the correctable error count.
 * @param      unused Unused metric cache parameter.
 *
 * @return @c ZE_RESULT_SUCCESS on success; error code on failure.
 */
ze_result_t errCorrectedTotalGetter(devInfo &d, MetricValue &out, const MetricCache & /*unused*/)
{
	return aggregateRasErrors(d, ZES_RAS_ERROR_TYPE_CORRECTABLE, out);
}

/**
 * @brief Getter for @c ecc.errors.uncorrected.aggregate.total — uncorrectable cache ECC errors.
 *
 * Delegates to aggregateRasErrors() with @c ZES_RAS_ERROR_TYPE_UNCORRECTABLE.
 *
 * @param[in]  d      Device to query.
 * @param[out] out    On success, set to the decimal string of the uncorrectable error count.
 * @param      unused Unused metric cache parameter.
 *
 * @return @c ZE_RESULT_SUCCESS on success; error code on failure.
 */
ze_result_t errUncorrectedTotalGetter(devInfo &d, MetricValue &out, const MetricCache & /*unused*/)
{
	return aggregateRasErrors(d, ZES_RAS_ERROR_TYPE_UNCORRECTABLE, out);
}

/**
 * @brief Getter for @c ecc.errors.aggregate.total — total RAS errors across all categories.
 *
 * Delegates to aggregateAllRasErrors().
 *
 * @param[in]  d      Device to query.
 * @param[out] out    On success, set to the decimal string of the total error count.
 * @param      unused Unused metric cache parameter.
 *
 * @return @c ZE_RESULT_SUCCESS on success; error code on failure.
 */
ze_result_t errAggregateTotalGetter(devInfo &d, MetricValue &out, const MetricCache & /*unused*/)
{
	return aggregateAllRasErrors(d, out);
}

// Generic getter for a single RAS error category and explicit type (CORRECTABLE or UNCORRECTABLE).
template <zes_ras_error_cat_t CAT, zes_ras_error_type_t TYPE>
ze_result_t rasErrorGetter(devInfo &d, MetricValue &out, const MetricCache & /*unused*/)
{
	static_assert(TYPE == ZES_RAS_ERROR_TYPE_CORRECTABLE || TYPE == ZES_RAS_ERROR_TYPE_UNCORRECTABLE,
				  "Use rasAllTypesGetter for unfiltered totals; do not pass FORCE_UINT32");
	auto *r = d.dev->getRAS();
	if (r == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	uint64_t n = 0;
	auto const res = r->getErrors(CAT, TYPE, &n);
	if (res == ZE_RESULT_SUCCESS) {
		out = std::format("{}", n);
	}
	return res;
}

// Getter for an unfiltered per-category total: sums correctable + uncorrectable.
// Categories that only define one type (e.g. RESET) return UNSUPPORTED for the
// other type; that is treated as zero so the sum is still correct.
template <zes_ras_error_cat_t CAT>
ze_result_t rasAllTypesGetter(devInfo &d, MetricValue &out, const MetricCache & /*unused*/)
{
	auto *r = d.dev->getRAS();
	if (r == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	uint64_t corr = 0, uncorr = 0;
	const auto rc = r->getErrors(CAT, ZES_RAS_ERROR_TYPE_CORRECTABLE, &corr);
	if (rc != ZE_RESULT_SUCCESS && rc != ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
		return rc;
	}
	const auto ru = r->getErrors(CAT, ZES_RAS_ERROR_TYPE_UNCORRECTABLE, &uncorr);
	if (ru != ZE_RESULT_SUCCESS && ru != ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
		return ru;
	}
	out = std::format("{}", corr + uncorr);
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
		.description = "Correctable (single-bit) cache ECC errors",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = errCorrectedTotalGetter,
	},
	{
		.name = "ecc.errors.uncorrected.aggregate.total",
		.unit = "",
		.description = "Uncorrectable (double-bit) cache ECC errors",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = errUncorrectedTotalGetter,
	},
	{
		.name = "ecc.errors.aggregate.total",
		.unit = "",
		.description = "Total ECC errors across all categories (correctable + uncorrectable)",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = errAggregateTotalGetter,
	},
	{
		.name = "ras.reset",
		.unit = "",
		.description = "GPU reset count",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = rasAllTypesGetter<ZES_RAS_ERROR_CAT_RESET>,
	},
	{
		.name = "ras.programming.errors",
		.unit = "",
		.description = "Programming error count",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = rasAllTypesGetter<ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS>,
	},
	{
		.name = "ras.driver.errors",
		.unit = "",
		.description = "Driver error count",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = rasAllTypesGetter<ZES_RAS_ERROR_CAT_DRIVER_ERRORS>,
	},
	{
		.name = "ras.cache.errors.correctable",
		.unit = "",
		.description = "Correctable cache ECC errors",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = rasErrorGetter<ZES_RAS_ERROR_CAT_CACHE_ERRORS, ZES_RAS_ERROR_TYPE_CORRECTABLE>,
	},
	{
		.name = "ras.cache.errors.uncorrectable",
		.unit = "",
		.description = "Uncorrectable cache ECC errors",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = rasErrorGetter<ZES_RAS_ERROR_CAT_CACHE_ERRORS, ZES_RAS_ERROR_TYPE_UNCORRECTABLE>,
	},
	{
		.name = "ras.non_compute.errors.total",
		.unit = "",
		.description = "Non-compute error count (correctable + uncorrectable; HAL does not differentiate)",
		.source = MetricSource::Live,
		.groups = MetricGroup::ECC,
		.getter = rasAllTypesGetter<ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS>,
	},
});
} // namespace

std::span<const QueryMetric> getEccMetrics() noexcept { return ECC_METRICS; }

} // namespace metrics::ecc
