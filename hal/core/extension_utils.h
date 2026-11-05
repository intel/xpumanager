/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _EXTENSION_UTILS_H
#define _EXTENSION_UTILS_H

#include <zes_api.h>
#include <bit>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zes_ext {
/**
 * @brief Retrieves a typed function pointer for a ZES driver extension function.
 *
 * Looks up the named extension function via zesDriverGetExtensionFunctionAddress
 * and casts the result to the requested function pointer type. Returns
 * ZE_RESULT_ERROR_UNSUPPORTED_FEATURE if the function is not found.
 *
 * @tparam T         Function pointer type of the extension function.
 * @param [in]  driver       The ZES driver handle to query.
 * @param [in]  functionName The name of the extension function to look up.
 *                           Must be a null-terminated string (string literals satisfy this).
 * @param [out] outFunction  Receives the typed function pointer on success.
 * @retval ZE_RESULT_SUCCESS
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE extension unavailable or function not found
 */
template <typename T>
[[nodiscard]] ze_result_t getExtensionFunction(zes_driver_handle_t driver, std::string_view functionName,
											   T *outFunction)
{
	void *extFunction = nullptr;
	ze_result_t result = zesDriverGetExtensionFunctionAddress(driver, std::string(functionName).c_str(), &extFunction);
	if (result != ZE_RESULT_SUCCESS || extFunction == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	static_assert(sizeof(void *) == sizeof(T), "function-pointer size must match data-pointer size");
	*outFunction = std::bit_cast<T>(extFunction);
	return ZE_RESULT_SUCCESS;
}

/** Result of an extension property lookup: distinguishes a driver query failure from the extension simply being absent.
 */
struct ExtensionLookupResult
{
	ze_result_t queryError = ZE_RESULT_SUCCESS; ///< non-success if zesDriverGetExtensionProperties failed
	std::optional<uint32_t> version;			///< set when the extension was found
};

/**
 * @brief Checks whether a named extension is advertised by the driver and returns its version.
 *
 * Uses zesDriverGetExtensionProperties to enumerate extensions and match by name.
 * The version is encoded as ZE_MAKE_VERSION(major, minor); use ZE_MAJOR_VERSION /
 * ZE_MINOR_VERSION to inspect it.
 *
 * @param [in] driver The ZES driver handle to query.
 * @param [in] name   Extension name to look up (e.g. ZES_INFO_LOGS_EXT_NAME).
 * @return ExtensionLookupResult: queryError is non-success on driver failure; version is set when found.
 */
[[nodiscard]] inline ExtensionLookupResult findExtensionVersion(zes_driver_handle_t driver, std::string_view name)
{
	uint32_t count = 0;
	ze_result_t result = zesDriverGetExtensionProperties(driver, &count, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		return {result, std::nullopt};
	}
	if (count == 0) {
		return {ZE_RESULT_SUCCESS, std::nullopt};
	}
	std::vector<zes_driver_extension_properties_t> props(count);
	result = zesDriverGetExtensionProperties(driver, &count, props.data());
	if (result != ZE_RESULT_SUCCESS) {
		return {result, std::nullopt};
	}
	for (const auto &p : props) {
		if (std::string_view{p.name} == name) {
			return {ZE_RESULT_SUCCESS, p.version};
		}
	}
	return {ZE_RESULT_SUCCESS, std::nullopt};
}
} // namespace zes_ext

#endif
