/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "ras.h"
#include "bounded_call.h"
#include <chrono>
#include <map>
#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

namespace {

/**
 * @brief Logs a failed RAS read.
 *
 * A device or driver without RAS support answers UNSUPPORTED_FEATURE, and a read rasCalls() cut
 * short reads CALL_ABANDONED; callers report both as no value with a reason, so they are logged
 * at debug level. Anything else is logged as an error.
 *
 * @param[in] what   What could not be read, e.g. "Failed to get RAS state".
 * @param[in] result The Level Zero result.
 */
void logRasReadFailure(std::string_view what, ze_result_t result)
{
	if (result == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE || result == CALL_ABANDONED) {
		DBG("{}. 0x{:X} ({})\n", what, static_cast<uint32_t>(result), l0_error_to_string(result));
	} else {
		ERR("{}. 0x{:X} ({})\n", what, static_cast<uint32_t>(result), l0_error_to_string(result));
	}
}

// How long one RAS call may take. The first enumeration in a process reads the counters of
// every RAS node in the system, about 1.4 s on a host with 7 CRI cards.
constexpr auto RAS_CALL_LIMIT = std::chrono::seconds{2};

/**
 * @brief The breaker every RAS call of the process runs through.
 *
 * On CRI the runtime reads the counters of every RAS node in the system, for any device, and
 * caches them only once all have answered. When the driver ends a read with an error, as it does
 * for a card whose sysctrl mailbox stays busy, the runtime waits for a reply that never comes.
 * Every later RAS call, for any device, would wait on the same node, so the calls of all devices
 * form one group.
 *
 * Making the calls one at a time also avoids a CRI failure: a process that reads the RAS state of
 * two devices at the same time gets an error for one of them.
 */
[[nodiscard]] xpum::CircuitBreaker &rasCalls()
{
	static xpum::CircuitBreaker breaker{"RAS", RAS_CALL_LIMIT, logBoundedCallEvent};
	return breaker;
}

/** A device's RAS error sets, as one enumeration. */
struct EnumeratedSets
{
	ze_result_t status{};
	std::vector<zes_ras_handle_t> handles{};
};

/**
 * @brief Lists a device's RAS error sets; made through rasCalls().
 *
 * @param[in] device The device.
 *
 * @retval {ZE_RESULT_SUCCESS, handles} The device's error sets, possibly none.
 * @retval {error, empty}                What zesDeviceEnumRasErrorSets() returned.
 */
[[nodiscard]] EnumeratedSets enumerateRasSets(zes_device_handle_t device)
{
	uint32_t count = 0;
	ze_result_t status = zesDeviceEnumRasErrorSets(device, &count, nullptr);
	if (status != ZE_RESULT_SUCCESS || count == 0) {
		return {.status = status};
	}
	std::vector<zes_ras_handle_t> handles(count);
	status = zesDeviceEnumRasErrorSets(device, &count, handles.data());
	if (status != ZE_RESULT_SUCCESS) {
		return {.status = status};
	}
	handles.resize(count);
	return {.status = status, .handles = std::move(handles)};
}

/** The experimental per-category states of a device's error sets, as one read. */
struct ExpStates
{
	ze_result_t status{};
	std::map<zes_ras_error_type_t, std::vector<ras_state_exp_t>> states{};
};

/**
 * @brief Clears every category of every error set in @p handles; made through rasCalls().
 *
 * See ras::clearErrors(). Stops at the first call that fails.
 *
 * @param[in] handles The device's error sets.
 *
 * @retval ZE_RESULT_SUCCESS Every category of every error set was cleared, or there was none.
 * @retval error             What the first failing zesRasGetProperties(), zesRasGetStateExp() or
 *                           zesRasClearStateExp() returned.
 */
[[nodiscard]] ze_result_t clearRasCounters(const std::vector<zes_ras_handle_t> &handles)
{
	for (const auto handle : handles) {
		zes_ras_properties_t properties = {};
		ze_result_t result = zesRasGetProperties(handle, &properties);
		if (result != ZE_RESULT_SUCCESS) {
			logRasReadFailure("Failed to get RAS properties", result);
			return result;
		}
		uint32_t categoryCount = 0;
		result = zesRasGetStateExp(handle, &categoryCount, nullptr);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get RAS state exp count for handle {}. 0x{:X} ({})\n", properties.type, result,
				l0_error_to_string(result));
			return result;
		}

		if (categoryCount == 0) {
			continue;
		}

		std::vector<zes_ras_state_exp_t> states(categoryCount);
		result = zesRasGetStateExp(handle, &categoryCount, states.data());
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get RAS state exp for handle {}. 0x{:X} ({})\n", properties.type, result,
				l0_error_to_string(result));
			return result;
		}
		// The second call may report fewer categories than the first.
		states.resize(categoryCount);

		for (const auto &state : states) {
			result = zesRasClearStateExp(handle, state.category);
			if (result != ZE_RESULT_SUCCESS) {
				ERR("Failed to clear RAS state exp category {} for handle {}. 0x{:X} ({})\n", state.category,
					properties.type, result, l0_error_to_string(result));
				return result;
			}
		}

		DBG("Cleared {} RAS error categories for handle {}.\n", categoryCount, properties.type);
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Reads the experimental per-category states of every error set in @p handles; made
 *        through rasCalls().
 *
 * See ras::getErrorsPerTileRasExp(). Error sets that are neither correctable nor uncorrectable
 * are skipped. Stops at the first call that fails.
 *
 * @param[in] handles The device's error sets.
 *
 * @retval {ZE_RESULT_SUCCESS, states} The states, keyed by error type; empty if no error set
 *                                     reports a category.
 * @retval {error, empty}              What the first failing zesRasGetProperties(),
 *                                     zesRasGetSupportedCategoriesExp() or zesRasGetStateExp2()
 *                                     returned.
 */
[[nodiscard]] ExpStates readExpStates(const std::vector<zes_ras_handle_t> &handles)
{
	ExpStates read{.status = ZE_RESULT_SUCCESS};
	for (const auto handle : handles) {
		zes_ras_properties_t properties = {};
		ze_result_t result = zesRasGetProperties(handle, &properties);
		if (result != ZE_RESULT_SUCCESS) {
			logRasReadFailure("Failed to get RAS properties", result);
			return {.status = result};
		}

		if (properties.type != ZES_RAS_ERROR_TYPE_CORRECTABLE && properties.type != ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
			continue; // Skip non-correctable/uncorrectable types for now
		}

		uint32_t categoryCount = 0;
		result = zesRasGetSupportedCategoriesExp(handle, &categoryCount, nullptr);
		if (result != ZE_RESULT_SUCCESS) {
			logRasReadFailure("Failed to get supported RAS categories count", result);
			return {.status = result};
		}
		DBG("RAS handle supports {} categories.\n", categoryCount);

		if (categoryCount == 0) {
			continue;
		}

		std::vector<zes_ras_error_category_exp_t> categories(categoryCount);
		result = zesRasGetSupportedCategoriesExp(handle, &categoryCount, categories.data());
		if (result != ZE_RESULT_SUCCESS) {
			logRasReadFailure("Failed to get supported RAS categories", result);
			return {.status = result};
		}
		// The second call may report fewer categories than the first.
		categories.resize(categoryCount);

		std::vector<zes_ras_state_exp2_t> states(
			categoryCount, {.stype = ZES_STRUCTURE_TYPE_RAS_STATE_EXP2, .pNext = nullptr, .errorCounter = 0});

		result = zesRasGetStateExp2(handle, categoryCount, categories.data(), states.data());
		if (result != ZE_RESULT_SUCCESS) {
			logRasReadFailure("Failed to get RAS state", result);
			return {.status = result};
		}

		const uint32_t tileId = properties.onSubdevice ? properties.subdeviceId : 0;
		auto &typeStates = read.states[properties.type];
		for (std::size_t j = 0; j < categories.size(); ++j) {
			typeStates.push_back({.category = categories[j], .errorCount = states[j].errorCounter, .tileId = tileId});
		}
	}

	return read;
}

} // namespace

/**
 * @brief Enumerates available RAS error sets for a device
 *
 * This function discovers and catalogs all RAS error sets available on the
 * specified device. RAS error sets provide hardware reliability monitoring
 * and error detection capabilities for different device components.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
ze_result_t ras::enumRasErrorSets(zes_device_handle_t device)
{
	auto [result, handles] = boundedCall(rasCalls(), [device] { return enumerateRasSets(device); });
	if (result != ZE_RESULT_SUCCESS) {
		logRasReadFailure("Failed to enumerate RAS error sets", result);
		rasHandles.clear();
		return result;
	}
	rasHandles = std::move(handles);
	if (rasHandles.empty()) {
		DBG("The device exposes no RAS error sets.\n");
	} else {
		DBG("Found {} RAS domains.\n", rasHandles.size());
	}
	return result;
}

/**
 * @brief Gets properties for a specific RAS error set
 *
 * This function retrieves detailed properties and characteristics for a
 * specific RAS error set, including error types, subdevice association,
 * and monitoring capabilities for reliability assessment.
 *
 * @param rasHandle Handle to the specific RAS error set
 * @param properties Pointer to structure to store RAS properties
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
ze_result_t ras::getProperties(zes_ras_handle_t rasHandle, zes_ras_properties_t *properties)
{
	const ze_result_t result = boundedGet<zesRasGetProperties>(rasCalls(), properties, rasHandle);
	if (result != ZE_RESULT_SUCCESS) {
		logRasReadFailure("Failed to get RAS properties", result);
		return result;
	}

	DBG("RAS properties retrieved successfully.\n");
	DBG("  onSubdevice: {}\n", properties->onSubdevice);
	DBG("  subdeviceId: {}\n", properties->subdeviceId);

	switch (properties->type) {
	case ZES_RAS_ERROR_TYPE_CORRECTABLE:
		DBG("  Type: Correctable\n");
		break;
	case ZES_RAS_ERROR_TYPE_UNCORRECTABLE:
		DBG("  Type: Uncorrectable\n");
		break;
	default:
		DBG("  Type: Unknown\n");
		break;
	}
	return result;
}

/**
 * @brief Gets the configuration for a RAS error set
 *
 * This function retrieves the current configuration settings for a specific
 * RAS error set, including error detection thresholds and reporting
 * configuration for reliability monitoring management.
 *
 * @param rasHandle Handle to the specific RAS error set
 * @param config Pointer to structure to store RAS configuration
 * @return ze_result_t ZE_RESULT_SUCCESS on successful configuration retrieval, error code otherwise
 */
ze_result_t ras::getConfig(zes_ras_handle_t rasHandle, zes_ras_config_t *config)
{
	const ze_result_t result = boundedGet<zesRasGetConfig>(rasCalls(), config, rasHandle);
	if (result != ZE_RESULT_SUCCESS) {
		logRasReadFailure("Failed to get RAS config", result);
		return result;
	}

	DBG("RAS config retrieved successfully.\n");
	DBG("  Total Threshold: {}\n", config->totalThreshold);
	DBG("  Detailed Thresholds:\n");
	for (uint32_t i = 0; i < ZES_MAX_RAS_ERROR_CATEGORY_COUNT; ++i) {
		DBG("    Category {}: {}\n", i, config->detailedThresholds.category[i]);
	}
	return result;
}

/**
 * @brief Gets the current state for a RAS error set
 *
 * This function retrieves the current error state and statistics for a specific
 * RAS error set, including error counts and categories for reliability
 * monitoring and system health assessment.
 *
 * @param rasHandle Handle to the specific RAS error set
 * @param state Pointer to structure to store RAS state information
 * @return ze_result_t ZE_RESULT_SUCCESS on successful state retrieval, error code otherwise
 */
ze_result_t ras::getState(zes_ras_handle_t rasHandle, zes_ras_state_t *state)
{
	constexpr ze_bool_t keepCounters = 0;
	const ze_result_t result = boundedGet<zesRasGetState>(rasCalls(), state, rasHandle, keepCounters);
	if (result != ZE_RESULT_SUCCESS) {
		logRasReadFailure("Failed to get RAS state", result);
		return result;
	}

	DBG("RAS state retrieved successfully.\n");
	for (uint32_t i = 0; i < ZES_MAX_RAS_ERROR_CATEGORY_COUNT; ++i) {
		DBG("    Category {}: {}\n", i, state->category[i]);
	}

	return result;
}

/**
 * @brief Gets specific RAS error counts by category and type
 *
 * This function retrieves error counts for a specific error category and type
 * across all RAS error sets, providing targeted reliability monitoring for
 * specific hardware components and error conditions.
 *
 * This is a convenience wrapper around getErrorsPerTile() that returns just
 * the total count without per-tile breakdown.
 *
 * @param [in] type The RAS error category to query
 * @param [in] errorType The specific error type (correctable or uncorrectable)
 * @param [out] rasCounter Pointer to store the accumulated error count
 * @return ze_result_t ZE_RESULT_SUCCESS on successful error count retrieval, error code otherwise
 */
ze_result_t ras::getErrors(zes_ras_error_cat_t type, zes_ras_error_type_t errorType, uint64_t *rasCounter)
{
	TRACING();

	if (rasCounter == nullptr) {
		ERR("Invalid argument: rasCounter is null\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}

	// Use getErrorsPerTile and just return the total
	std::map<uint32_t, uint64_t> countersPerTile;
	ze_result_t result = getErrorsPerTile(type, errorType, countersPerTile, rasCounter);

	if (result == ZE_RESULT_SUCCESS) {
		DBG("RAS error count for category {}: {}\n", type, *rasCounter);
	}

	return result;
}

/**
 * @brief Gets RAS error counts per tile for a specific category and type
 *
 * This function retrieves error counts for each tile/subdevice separately,
 * along with the total count across all tiles. This enables proper multi-tile
 * device monitoring where each tile's RAS errors can be tracked independently.
 *
 * @param [in] type The RAS error category to query
 * @param [in] errorType The specific error type (correctable or uncorrectable)
 * @param [out] countersPerTile Map to store per-tile error counts (tile_id -> count)
 * @param [out] totalCounter Pointer to store the total error count across all tiles
 * @return ze_result_t ZE_RESULT_SUCCESS on successful error count retrieval, error code otherwise
 */
ze_result_t ras::getErrorsPerTile(zes_ras_error_cat_t type, zes_ras_error_type_t errorType,
								  std::map<uint32_t, uint64_t> &countersPerTile, uint64_t *totalCounter)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_ras_properties_t properties = {};
	zes_ras_state_t states = {};

	if (type < ZES_RAS_ERROR_CAT_RESET || type > ZES_RAS_ERROR_CAT_DISPLAY_ERRORS) {
		ERR("Invalid RAS error category type: {}\n", type);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (totalCounter == nullptr) {
		ERR("Invalid argument: totalCounter is null\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}

	countersPerTile.clear();
	*totalCounter = 0;

	const auto [enumResult, handles] = errorSets();
	if (enumResult != ZE_RESULT_SUCCESS) {
		return enumResult;
	}
	for (const auto handle : handles) {
		result = getProperties(handle, &properties);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getState(handle, &states);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		// For cache errors, filter by properties.type to get correctable/uncorrectable separately
		// For other categories, sum all handles (legacy behavior)
		if (type == ZES_RAS_ERROR_CAT_CACHE_ERRORS && errorType != ZES_RAS_ERROR_TYPE_FORCE_UINT32 &&
			properties.type != errorType) {
			continue;
		}

		uint64_t errorCount = states.category[type];

		uint32_t tileId = properties.onSubdevice ? properties.subdeviceId : 0;

		countersPerTile[tileId] += errorCount;
		*totalCounter += errorCount;
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Clears all RAS error counters for all error categories
 *
 * This function clears the error counters for every supported RAS error
 * category across all enumerated RAS handles using the experimental
 * zesRasGetStateExp / zesRasClearStateExp APIs.
 *
 * @return ze_result_t ZE_RESULT_SUCCESS on successful clear, error code otherwise
 */
ze_result_t ras::clearErrors()
{
	TRACING();
	auto [enumResult, handles] = errorSets();
	if (enumResult != ZE_RESULT_SUCCESS) {
		return enumResult;
	}

	if (handles.empty()) {
		ERR("No RAS handles available to clear errors.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	return boundedCall(rasCalls(), [sets = std::move(handles)] { return clearRasCounters(sets); });
}

/**
 * @brief Retrieves per-tile RAS information using the experimental RAS API.
 *
 * This function queries the supported error categories for each enumerated RAS
 * handle and populates @p rasErrStates for the corresponding @c zes_ras_error_type_t
 * values. It uses the standard Level Zero experimental RAS entry points
 * @c zesRasGetSupportedCategoriesExp and @c zesRasGetStateExp2 directly.
 *
 * The returned map is keyed by RAS error type and each value contains the per-tile
 * experimental RAS state entries collected for that type.
 *
 * @param [out] rasErrStates Map of RAS error type to the per-tile experimental RAS states
 *                           collected for that type.
 * @return ze_result_t @c ZE_RESULT_SUCCESS on successful retrieval, or an error code otherwise.
 */
ze_result_t ras::getErrorsPerTileRasExp(std::map<zes_ras_error_type_t, std::vector<ras_state_exp_t>> &rasErrStates)
{
	TRACING();
	rasErrStates.clear();
	auto [enumResult, handles] = errorSets();
	if (enumResult != ZE_RESULT_SUCCESS) {
		return enumResult;
	}

	if (handles.empty()) {
		// A device without RAS error sets is normal, not a failure.
		DBG("No RAS handles available to query experimental RAS state.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	auto [result, states] = boundedCall(rasCalls(), [sets = std::move(handles)] { return readExpStates(sets); });
	rasErrStates = std::move(states);
	return result;
}

/**
 * @brief Initializes the RAS management module for a device
 *
 * This function performs initial setup of RAS monitoring capabilities by
 * enumerating all available RAS error sets on the specified device for
 * subsequent reliability monitoring and error detection operations.
 *
 * @param device Handle to the device for RAS initialization
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t ras::init(zes_device_handle_t device)
{
	TRACING();
	zesDevice = device;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief The device's RAS error sets, enumerated the first time they are needed.
 *
 * A failed enumeration is logged by enumRasErrorSets(), and its result is kept so callers can
 * tell a failed read from a device without error sets; it is not retried. A device init() never
 * ran on has none. An enumeration rasCalls() cut short is retried once the breaker has reset,
 * since the runtime then has what it waited for.
 *
 * @retval {ZE_RESULT_SUCCESS, handles} The device's error sets, possibly none.
 * @retval {CALL_ABANDONED, empty}       The enumeration did not return in time; the device's
 *                                       error sets are unknown.
 * @retval {error, empty}                What the enumeration returned, e.g. DEVICE_LOST.
 */
ras::ErrorSets ras::errorSets()
{
	const std::lock_guard lock{enumLock.mutex};
	if (!enumerated || (enumStatus == CALL_ABANDONED && !rasCalls().isOpen())) {
		enumerated = true;
		// A device object that init() never ran on has no handle to enumerate with.
		const ze_result_t result = zesDevice != nullptr ? enumRasErrorSets(zesDevice) : ZE_RESULT_SUCCESS;
		enumStatus = result;
	}
	return {.status = enumStatus, .handles = rasHandles};
}

/**
 * @brief Performs comprehensive RAS monitoring runtime operations
 *
 * This function executes a complete RAS monitoring cycle including property
 * retrieval, configuration checking, and state monitoring for all RAS error
 * sets to provide comprehensive reliability and serviceability assessment.
 *
 * @param device Handle to the device (unused in current implementation)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful execution, error code otherwise
 */
ze_result_t ras::zesRun(UNUSED zes_device_handle_t device)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_ras_properties_t properties = {};
	zes_ras_config_t config = {};
	zes_ras_state_t state = {};

	const auto [enumResult, handles] = errorSets();
	if (enumResult != ZE_RESULT_SUCCESS) {
		return enumResult;
	}
	for (const auto handle : handles) {
		result = getProperties(handle, &properties);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getConfig(handle, &config);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getState(handle, &state);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
	}

	return result;
}