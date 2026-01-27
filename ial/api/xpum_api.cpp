/*
 *  Copyright (C) 2021-2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file xpum_c_api.cpp
 *
 *  @brief C API implementation for XPUM using modern C++20 and HAL layer
 */

// Include Level Zero headers first (needed for type conversion functions)
#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>

#include "xpum_api.h"
#include "xpum_structs.h"
#include "ial/cli/version.h"
#include "hal/core/debug.h"
#include "hal/core/device.h"
#include "hal/core/driver.h"
#include "hal/fwupd/fwupd.h"
#include "hal/core/firmware.h"
#include "oal/os.h"

#include <cstdio>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <exception>
#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Anonymous namespace for internal implementation details
namespace {

/**************************************************************************/
/** Internal Context Management using C++20 features
 */
/**************************************************************************/

struct APIContext
{
	std::unique_ptr<driver> driverInstance;
	std::vector<devInfo> devices;

	// Group management (IAL-specific, not in HAL)
	struct GroupInfo
	{
		std::string name;
		std::vector<xpum_device_id_t> devices;
	};
	std::unordered_map<xpum_group_id_t, GroupInfo> groups;
	xpum_group_id_t nextGroupId{1};
};

struct APIState
{
	std::mutex mutex;
	std::unique_ptr<APIContext> context;
};

/**
 * @brief Get global API state (thread-safe Meyer's singleton)
 */
APIState &getState()
{
	static APIState state;
	return state;
}

/**************************************************************************/
/** Helper Functions
 */
/**************************************************************************/

/**
 * @brief Check if context is initialized
 */
bool isInitialized(const APIState &state) { return state.context != nullptr; }

/**
 * @brief Format UUID from Level Zero uuid struct
 */
std::string formatUuid(const uint8_t (&id)[16])
{
	return std::format(
		"{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}", id[0],
		id[1], id[2], id[3], id[4], id[5], id[6], id[7], id[8], id[9], id[10], id[11], id[12], id[13], id[14], id[15]);
}

/**
 * @brief Copy/format into fixed-size C string buffer (C++20)
 *
 * Overload 1: Plain string copy (no formatting)
 * Overload 2: Format with arguments using std::format
 */
template <size_t N> void toCstr(char (&dest)[N], std::string_view src)
{
	const size_t len = std::min(src.size(), N - 1);
	std::copy_n(src.data(), len, dest);
	dest[len] = '\0';
}

//clang-format off
// This got merged while failing and seems to continue to fail
template <size_t N, typename... Args>
	requires(sizeof...(Args) > 0)
//clang-format on
void toCstr(char (&dest)[N], std::format_string<Args...> fmt, Args &&...args)
{
	auto result = std::format_to_n(dest, N - 1, fmt, std::forward<Args>(args)...);
	dest[result.size] = '\0';
}

/**
 * @brief Get device by ID with proper error reporting
 * @return XPUM_OK on success, XPUM_NOT_INITIALIZED or XPUM_RESULT_DEVICE_NOT_FOUND on error
 */
xpum_result_t getDevice(const APIState &state, xpum_device_id_t deviceId, device **outDevice)
{
	if (!isInitialized(state)) {
		return XPUM_NOT_INITIALIZED;
	}
	if (deviceId < 0 || static_cast<size_t>(deviceId) >= state.context->devices.size()) {
		return XPUM_RESULT_DEVICE_NOT_FOUND;
	}
	*outDevice = state.context->devices[deviceId].dev;
	return XPUM_OK;
}

/**
 * @brief Convert Level Zero result codes to XPUM result codes
 */
constexpr xpum_result_t toXpumResult(ze_result_t zeResult)
{
	switch (zeResult) {
	case ZE_RESULT_SUCCESS:
		return XPUM_OK;
	case ZE_RESULT_ERROR_UNINITIALIZED:
		return XPUM_NOT_INITIALIZED;
	case ZE_RESULT_ERROR_DEVICE_LOST:
		return XPUM_RESULT_DEVICE_NOT_FOUND;
	case ZE_RESULT_ERROR_INVALID_NULL_POINTER:
	case ZE_RESULT_ERROR_INVALID_ARGUMENT:
	case ZE_RESULT_ERROR_INVALID_SIZE:
		return XPUM_GENERIC_ERROR;
	case ZE_RESULT_ERROR_UNSUPPORTED_FEATURE:
		return XPUM_API_UNSUPPORTED;
	case ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY:
	case ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY:
		return XPUM_GENERIC_ERROR;
	case ZE_RESULT_ERROR_NOT_AVAILABLE:
		return XPUM_METRIC_NOT_SUPPORTED;
	default:
		return XPUM_GENERIC_ERROR;
	}
}

/**
 * @brief Convert XPUM standby mode to Level Zero standby mode
 */
inline zes_standby_promo_mode_t toZesStandbyMode(xpum_standby_mode_t mode)
{
	switch (mode) {
	case XPUM_STANDBY_MODE_DEFAULT:
		return ZES_STANDBY_PROMO_MODE_DEFAULT;
	case XPUM_STANDBY_MODE_NEVER:
		return ZES_STANDBY_PROMO_MODE_NEVER;
	default:
		return ZES_STANDBY_PROMO_MODE_DEFAULT;
	}
}

/**
 * @brief Convert Level Zero ECC state to XPUM ECC state
 */
inline xpum_ecc_state_t toXpumEccState(zes_device_ecc_state_t state)
{
	switch (state) {
	case ZES_DEVICE_ECC_STATE_UNAVAILABLE:
		return XPUM_ECC_STATE_UNAVAILABLE;
	case ZES_DEVICE_ECC_STATE_ENABLED:
		return XPUM_ECC_STATE_ENABLED;
	case ZES_DEVICE_ECC_STATE_DISABLED:
		return XPUM_ECC_STATE_DISABLED;
	default:
		return XPUM_ECC_STATE_UNAVAILABLE;
	}
}

/**
 * @brief Convert XPUM frequency domain to Level Zero frequency domain
 */
inline zes_freq_domain_t toZesFreqDomain(xpum_freq_domain_t domain)
{
	switch (domain) {
	case XPUM_FREQ_DOMAIN_GPU:
		return ZES_FREQ_DOMAIN_GPU;
	case XPUM_FREQ_DOMAIN_MEMORY:
		return ZES_FREQ_DOMAIN_MEMORY;
	default:
		return ZES_FREQ_DOMAIN_GPU;
	}
}

/**
 * @brief Convert Level Zero throttle reasons to XPUM throttle reasons
 */
inline xpum_freq_throttle_reason_flags_t toXpumThrottleReasons(zes_freq_throttle_reason_flags_t reasons)
{
	uint32_t xpumReasons = XPUM_FREQ_THROTTLE_REASON_NONE;
	if (reasons & ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT)
		xpumReasons |= XPUM_FREQ_THROTTLE_REASON_THERMAL_LIMIT;
	if (reasons & ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT)
		xpumReasons |= XPUM_FREQ_THROTTLE_REASON_CURRENT_LIMIT;
#ifdef ZES_FREQ_THROTTLE_REASON_FLAG_POWER_LIMIT
	if (reasons & ZES_FREQ_THROTTLE_REASON_FLAG_POWER_LIMIT)
		xpumReasons |= XPUM_FREQ_THROTTLE_REASON_POWER_LIMIT;
#endif
#ifdef ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_EXCURSION
	if (reasons & ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_EXCURSION)
		xpumReasons |= XPUM_FREQ_THROTTLE_REASON_THERMAL_EXCURSION;
#endif
	if (reasons & ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP)
		xpumReasons |= XPUM_FREQ_THROTTLE_REASON_BURST_POWER_CAP;
	if (reasons & ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP)
		xpumReasons |= XPUM_FREQ_THROTTLE_REASON_AVE_POWER_CAP;
	return static_cast<xpum_freq_throttle_reason_flags_t>(xpumReasons);
}

/**
 * @brief Convert XPUM RAS error category to Level Zero RAS error category
 */
inline zes_ras_error_cat_t toZesRasErrorCat(xpum_ras_error_cat_t category)
{
	switch (category) {
	case XPUM_RAS_ERROR_CAT_RESET:
		return ZES_RAS_ERROR_CAT_RESET;
	case XPUM_RAS_ERROR_CAT_PROGRAMMING:
		return ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS;
	case XPUM_RAS_ERROR_CAT_DRIVER:
		return ZES_RAS_ERROR_CAT_DRIVER_ERRORS;
	case XPUM_RAS_ERROR_CAT_COMPUTE:
		return ZES_RAS_ERROR_CAT_COMPUTE_ERRORS;
	case XPUM_RAS_ERROR_CAT_NON_COMPUTE:
		return ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS;
	case XPUM_RAS_ERROR_CAT_CACHE:
		return ZES_RAS_ERROR_CAT_CACHE_ERRORS;
	case XPUM_RAS_ERROR_CAT_DISPLAY:
		return ZES_RAS_ERROR_CAT_DISPLAY_ERRORS;
	default:
		return ZES_RAS_ERROR_CAT_RESET;
	}
}

/**
 * @brief Convert XPUM RAS error type to Level Zero RAS error type
 */
inline zes_ras_error_type_t toZesRasErrorType(xpum_ras_error_type_t errorType)
{
	switch (errorType) {
	case XPUM_RAS_ERROR_TYPE_CORRECTABLE:
		return ZES_RAS_ERROR_TYPE_CORRECTABLE;
	case XPUM_RAS_ERROR_TYPE_UNCORRECTABLE:
		return ZES_RAS_ERROR_TYPE_UNCORRECTABLE;
	default:
		return ZES_RAS_ERROR_TYPE_CORRECTABLE;
	}
}

} // anonymous namespace

/**************************************************************************/
/** C API Implementation - Basic APIs
 */
/**************************************************************************/

extern "C" {

XPUM_API xpum_result_t xpumInit()
{
	try {
		INFO("xpumInit called\n");
		auto &state = getState();
		std::scoped_lock const lock(state.mutex);

		// Already initialized
		if (isInitialized(state)) {
			INFO("xpumInit: Already initialized\n");
			return XPUM_OK;
		}

		// Create new context
		state.context = std::make_unique<APIContext>();

		// Initialize Level Zero driver
		state.context->driverInstance = std::make_unique<driver>();
		if (toXpumResult(state.context->driverInstance->init()) != XPUM_OK) {
			ERR("xpumInit: Failed to initialize Level Zero driver\n");
			state.context.reset();
			return XPUM_NOT_INITIALIZED;
		}

		// Enumerate all devices using findDevice with nullptr to get all
		if (toXpumResult(state.context->driverInstance->findDevice(nullptr, &state.context->devices)) != XPUM_OK) {
			ERR("xpumInit: Failed to enumerate devices\n");
			state.context.reset();
			return XPUM_NOT_INITIALIZED;
		}

		INFO("xpumInit: Successfully initialized with %zu devices\n", state.context->devices.size());
		return XPUM_OK;

	} catch (const std::exception &e) {
		ERR("xpumInit: Exception: %s\n", e.what());
		return XPUM_GENERIC_ERROR;
	}
}

XPUM_API xpum_result_t xpumShutdown(void)
{
	try {
		INFO("xpumShutdown called\n");
		auto &state = getState();
		std::scoped_lock const lock(state.mutex);

		if (!isInitialized(state)) {
			INFO("xpumShutdown: Not initialized\n");
			return XPUM_NOT_INITIALIZED;
		}

		state.context.reset();
		INFO("xpumShutdown: Successfully shutdown\n");
		return XPUM_OK;

	} catch (const std::exception &e) {
		ERR("xpumShutdown: Exception: %s\n", e.what());
		return XPUM_GENERIC_ERROR;
	}
}

XPUM_API xpum_result_t xpumVersionInfo(XpumVersionInfo versionInfoList[], int *count)
{
	try {
		if (count == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}

		constexpr int versionCount = 2;

		if (versionInfoList == nullptr) {
			*count = versionCount;
			return XPUM_OK;
		}

		if (*count < versionCount) {
			return XPUM_RESULT_INVALID_DIR;
		}

		// XPUM IAL library version
		versionInfoList[0].version = XPUM_VERSION;
		toCstr(versionInfoList[0].versionString, XPUM_VERSION_STRING);

		// Level Zero version
		versionInfoList[1].version = XPUM_VERSION_LEVEL_ZERO;
		toCstr(versionInfoList[1].versionString, "1.23.0");

		*count = versionCount;
		return XPUM_OK;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

/**************************************************************************/
/** C API Implementation - Device APIs
 */
/**************************************************************************/

XPUM_API xpum_result_t xpumGetDeviceList(XpumDeviceBasicInfo deviceList[], int *count)
{
	try {
		if (count == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		if (!isInitialized(state)) {
			return XPUM_NOT_INITIALIZED;
		}

		const int deviceCount = static_cast<int>(state.context->devices.size());

		// First call: return count
		if (deviceList == nullptr) {
			*count = deviceCount;
			return XPUM_OK;
		}

		// Check buffer size
		if (*count < deviceCount) {
			return XPUM_RESULT_INVALID_DIR;
		}

		// Fill device list
		for (int i = 0; i < deviceCount; ++i) {
			auto &devInfo = state.context->devices[i];
			auto *dev = devInfo.dev;

			deviceList[i].deviceId = i;
			deviceList[i].type = GPU;
			deviceList[i].functionType = XPUM_DEVICE_FUNCTION_TYPE_PHYSICAL;

			// Get device properties using HAL
			ze_device_properties_t props{};
			props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
			if (dev->getDevProps(devInfo.deviceHdl, &props) == ZE_RESULT_SUCCESS) {
				toCstr(deviceList[i].deviceName, props.name);

				// Format UUID
				auto uuid = formatUuid(props.uuid.id);
				toCstr(deviceList[i].uuid, uuid);
			}

			// Get PCI BDF using HAL helper
			auto bdfStr = dev->getBDFStr();
			toCstr(deviceList[i].pcibdfAddress, bdfStr);
			toCstr(deviceList[i].pciDeviceId, bdfStr);

			toCstr(deviceList[i].vendorName, "Intel(R) Corporation");
		}

		*count = deviceCount;
		return XPUM_OK;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

XPUM_API xpum_result_t xpumGetDeviceProperties(xpum_device_id_t deviceId, xpum_device_properties_t *pXpumProperties)
{
	try {
		if (pXpumProperties == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		// Initialize structure
		pXpumProperties->deviceId = deviceId;
		pXpumProperties->propertyLen = 0;

		// Helper lambda to add property
		auto addProperty = [&](xpum_device_property_name_t name, std::string_view value) {
			if (pXpumProperties->propertyLen < XPUM_MAX_NUM_PROPERTIES) {
				auto &prop = pXpumProperties->properties[pXpumProperties->propertyLen++];
				prop.name = name;
				toCstr(prop.value, value);
			}
		};

		ze_device_handle_t zeDevice = state.context->devices[deviceId].deviceHdl;

		// Get basic device properties using HAL
		ze_device_properties_t props{};
		props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
		if (toXpumResult(dev->getDevProps(zeDevice, &props)) == XPUM_OK) {
			addProperty(XPUM_DEVICE_PROPERTY_DEVICE_NAME, props.name);
			addProperty(XPUM_DEVICE_PROPERTY_VENDOR_NAME, "Intel(R) Corporation");

			addProperty(XPUM_DEVICE_PROPERTY_UUID, formatUuid(props.uuid.id));

			addProperty(XPUM_DEVICE_PROPERTY_PCI_DEVICE_ID, std::format("{:x}", props.deviceId));
			addProperty(XPUM_DEVICE_PROPERTY_PCI_VENDOR_ID, std::format("{:x}", props.vendorId));
			addProperty(XPUM_DEVICE_PROPERTY_DEVICE_STEPPING, std::format("{}", props.deviceId & 0xFF));
			addProperty(XPUM_DEVICE_PROPERTY_NUMBER_OF_TILES, std::format("{}", props.numSlices));
		}

		// Get PCI properties using HAL helper
		addProperty(XPUM_DEVICE_PROPERTY_PCI_BDF_ADDRESS, dev->getBDFStr());

		// Get PCI details through HAL's PCI class
		auto *pci = dev->getPCI();
		if (pci != nullptr) {
			zes_pci_properties_t pciProps{};
			if (toXpumResult(pci->getProperties(state.context->devices[deviceId].zesDeviceHdl, &pciProps)) == XPUM_OK) {
				addProperty(XPUM_DEVICE_PROPERTY_PCIE_GENERATION, std::format("{}", pciProps.maxSpeed.gen));
				addProperty(XPUM_DEVICE_PROPERTY_PCIE_MAX_LINK_WIDTH, std::format("{}", pciProps.maxSpeed.width));
			}
		}

		// Get memory properties using HAL
		ze_device_memory_properties_t *memProps = nullptr;
		uint32_t memCount = 0;
		if (toXpumResult(dev->getMemProps(zeDevice, &memProps, &memCount)) == XPUM_OK && memCount > 0 &&
			(memProps != nullptr)) {
			uint64_t totalSize = 0;
			for (uint32_t i = 0; i < memCount; ++i) {
				totalSize += memProps[i].totalSize;
			}
			addProperty(XPUM_DEVICE_PROPERTY_MEMORY_PHYSICAL_SIZE_BYTE, std::format("{}", totalSize));

			// Memory bus width from first memory module
			if (memCount > 0) {
				addProperty(XPUM_DEVICE_PROPERTY_MEMORY_BUS_WIDTH, std::format("{}", memProps[0].maxBusWidth));
			}
		}

		// Get firmware versions using HAL
		if (auto *firmware = dev->getFirmware()) {
			char fwVersion[256] = {};
			std::string const bdf = dev->getBDFStr();

			// GFX firmware
			if (toXpumResult(firmware->getFWversion(GFX, bdf.c_str(), fwVersion, sizeof(fwVersion))) == XPUM_OK) {
				addProperty(XPUM_DEVICE_PROPERTY_GFX_FIRMWARE_VERSION, fwVersion);
			}

			// GFX Data firmware
			if (toXpumResult(firmware->getFWversion(GFX_DATA, bdf.c_str(), fwVersion, sizeof(fwVersion))) == XPUM_OK) {
				addProperty(XPUM_DEVICE_PROPERTY_GFX_DATA_FIRMWARE_VERSION, fwVersion);
			}

			// AMC firmware (if device has AMC)
			if (dev->getAmcIndex() >= 0) {
				if (toXpumResult(firmware->getFWversion(AMC, bdf.c_str(), fwVersion, sizeof(fwVersion))) == XPUM_OK) {
					addProperty(XPUM_DEVICE_PROPERTY_AMC_FIRMWARE_VERSION, fwVersion);
				}
			}
		}

		return XPUM_OK;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

XPUM_API xpum_result_t xpumGetDeviceIdByBDF(const char *bdf, xpum_device_id_t *deviceId)
{
	try {
		if (bdf == nullptr || deviceId == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		if (!isInitialized(state)) {
			return XPUM_NOT_INITIALIZED;
		}

		// Search for matching device using HAL's BDF matching
		for (size_t i = 0; i < state.context->devices.size(); ++i) {
			auto *dev = state.context->devices[i].dev;
			if ((dev != nullptr) && dev->isBDF(bdf)) {
				*deviceId = static_cast<xpum_device_id_t>(i);
				return XPUM_OK;
			}
		}

		return XPUM_RESULT_DEVICE_NOT_FOUND;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

XPUM_API xpum_result_t xpumGetAMCFirmwareVersions(UNUSED xpum_amc_fw_version_t versionList[], int *count,
												  UNUSED const char *username, UNUSED const char *password)
{
	// TODO: Implement AMC firmware version retrieval using HAL
	if (count == nullptr) {
		return XPUM_RESULT_INVALID_DIR;
	}

	*count = 0;
	return XPUM_METRIC_NOT_SUPPORTED;
}

XPUM_API xpum_result_t xpumGetAMCFirmwareVersionsErrorMsg(UNUSED char *buffer, int *count)
{
	// TODO: Implement error message retrieval
	if (count == nullptr) {
		return XPUM_RESULT_INVALID_DIR;
	}

	*count = 0;
	return XPUM_METRIC_NOT_SUPPORTED;
}

XPUM_API xpum_result_t xpumGetSerialNumberAndAmcFwVersion(xpum_device_id_t deviceId, UNUSED const char *username,
														  UNUSED const char *password,
														  char serialNumber[XPUM_MAX_STR_LENGTH],
														  char amcFwVersion[XPUM_MAX_STR_LENGTH])
{
	try {
		if (serialNumber == nullptr || amcFwVersion == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		// Get AMC firmware version if device has AMC
		if (dev->getAmcIndex() >= 0) {
			auto *firmware = dev->getFirmware();
			if (firmware != nullptr) {
				char fwVer[256] = {};
				std::string const bdf = dev->getBDFStr();

				if (toXpumResult(firmware->getFWversion(AMC, bdf.c_str(), fwVer, sizeof(fwVer))) == XPUM_OK) {
					std::strncpy(amcFwVersion, fwVer, XPUM_MAX_STR_LENGTH - 1);
					amcFwVersion[XPUM_MAX_STR_LENGTH - 1] = '\0';
				} else {
					amcFwVersion[0] = '\0';
				}
			} else {
				amcFwVersion[0] = '\0';
			}
		} else {
			amcFwVersion[0] = '\0';
		}

		// Serial number not available through standard Level Zero API
		serialNumber[0] = '\0';

		return XPUM_OK;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

/**************************************************************************/
/** C API Implementation - Device Configuration and Control APIs
 */
/**************************************************************************/

/**
 * @brief Reset a device
 */
XPUM_API xpum_result_t xpumResetDevice(xpum_device_id_t deviceId, UNUSED bool force)
{
	try {
		INFO("xpumResetDevice: deviceId=%d\n", deviceId);
		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			if (result == XPUM_RESULT_DEVICE_NOT_FOUND) {
				ERR("xpumResetDevice: Invalid device ID %d\n", deviceId);
			}
			return result;
		}

		ze_device_handle_t zeDevice = state.context->devices[deviceId].deviceHdl;
		xpum_result_t resetResult = toXpumResult(dev->resetDevice(zeDevice));
		if (resetResult == XPUM_OK) {
			INFO("xpumResetDevice: Successfully reset device %d\n", deviceId);
		} else {
			ERR("xpumResetDevice: Failed to reset device %d, result=%d\n", deviceId, resetResult);
		}
		return resetResult;

	} catch (const std::exception &e) {
		ERR("xpumResetDevice: Exception: %s\n", e.what());
		return XPUM_GENERIC_ERROR;
	}
}

/**
 * @brief Set power limit
 */
XPUM_API xpum_result_t xpumSetPowerLimit(xpum_device_id_t deviceId, int32_t tileId, uint32_t powerLimit,
										 UNUSED uint32_t intervalWindow)
{
	try {
		INFO("xpumSetPowerLimit: deviceId=%d, tileId=%d, limit=%uW\n", deviceId, tileId, powerLimit);
		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		auto *power = dev->getPower();
		if (power == nullptr) {
			return XPUM_METRIC_NOT_SUPPORTED;
		}

		// Convert watts to milliwatts
		uint32_t limitMw = powerLimit * 1000;
		xpum_result_t setPowerResult = toXpumResult(power->setSustainedLimit(limitMw, tileId));
		if (setPowerResult == XPUM_OK) {
			INFO("xpumSetPowerLimit: Successfully set power limit\n");
		} else {
			ERR("xpumSetPowerLimit: Failed, result=%d\n", setPowerResult);
		}
		return setPowerResult;

	} catch (const std::exception &e) {
		ERR("xpumSetPowerLimit: Exception: %s\n", e.what());
		return XPUM_GENERIC_ERROR;
	}
}

/**
 * @brief Set frequency range
 */
XPUM_API xpum_result_t xpumSetFrequencyRange(xpum_device_id_t deviceId, int32_t tileId, double minFreq, double maxFreq)
{
	try {
		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		auto *freq = dev->getFrequency();
		if (freq == nullptr) {
			return XPUM_METRIC_NOT_SUPPORTED;
		}

		if (tileId < 0) {
			return toXpumResult(freq->setFrequencyRangeForAll(minFreq, maxFreq));
		} else {
			return toXpumResult(freq->setFrequencyRange(minFreq, maxFreq, tileId));
		}

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

/**
 * @brief Set standby mode
 */
XPUM_API xpum_result_t xpumSetStandby(xpum_device_id_t deviceId, xpum_standby_mode_t mode)
{
	try {
		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		auto *standby = dev->getStandby();
		if (standby == nullptr) {
			return XPUM_METRIC_NOT_SUPPORTED;
		}

		return toXpumResult(standby->setMode(toZesStandbyMode(mode)));

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

/**
 * @brief Set scheduler mode
 */
XPUM_API xpum_result_t xpumSetSchedulerTimeoutMode(xpum_device_id_t deviceId, uint32_t subdeviceId, uint64_t timeout)
{
	try {
		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		auto *freq = dev->getFrequency();
		if (freq == nullptr) {
			return XPUM_METRIC_NOT_SUPPORTED;
		}

		return toXpumResult(freq->setSchedulerTimeoutMode(subdeviceId, timeout));

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

/**
 * @brief Set scheduler timeslice mode
 */
XPUM_API xpum_result_t xpumSetSchedulerTimesliceMode(xpum_device_id_t deviceId, uint32_t subdeviceId, uint64_t interval,
													 uint64_t yieldTimeout)
{
	try {
		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		auto *freq = dev->getFrequency();
		if (freq == nullptr) {
			return XPUM_METRIC_NOT_SUPPORTED;
		}

		return toXpumResult(freq->setSchedulerTimesliceMode(subdeviceId, interval, yieldTimeout));

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

/**
 * @brief Set scheduler exclusive mode
 */
XPUM_API xpum_result_t xpumSetSchedulerExclusiveMode(xpum_device_id_t deviceId, uint32_t subdeviceId)
{
	try {
		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		auto *freq = dev->getFrequency();
		if (freq == nullptr) {
			return XPUM_METRIC_NOT_SUPPORTED;
		}

		return toXpumResult(freq->setSchedulerExclusiveMode(subdeviceId));

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

/**
 * @brief Get ECC state
 */
XPUM_API xpum_result_t xpumGetEccState(xpum_device_id_t deviceId, bool *enabled, bool *configurable,
									   xpum_ecc_state_t *currentState, xpum_ecc_state_t *pendingState)
{
	try {
		if (enabled == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		auto *ecc = dev->getECC();
		if (ecc == nullptr) {
			return XPUM_METRIC_NOT_SUPPORTED;
		}

		zes_device_handle_t zesDevice = state.context->devices[deviceId].zesDeviceHdl;

		zes_device_ecc_properties_t eccProps = {};
		result = toXpumResult(ecc->getState(zesDevice, &eccProps));
		if (result == XPUM_OK) {
			*enabled = (eccProps.currentState == ZES_DEVICE_ECC_STATE_ENABLED);
			if (configurable != nullptr) {
				*configurable = ecc->configurable(zesDevice);
			}
			if (currentState != nullptr) {
				*currentState = toXpumEccState(eccProps.currentState);
			}
			if (pendingState != nullptr) {
				*pendingState = toXpumEccState(eccProps.pendingState);
			}
		}

		return result;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

/**
 * @brief Set ECC state
 */
XPUM_API xpum_result_t xpumSetEccState(xpum_device_id_t deviceId, bool enable)
{
	try {
		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		auto *ecc = dev->getECC();
		if (ecc == nullptr) {
			return XPUM_METRIC_NOT_SUPPORTED;
		}

		zes_device_handle_t zesDevice = state.context->devices[deviceId].zesDeviceHdl;
		return toXpumResult(ecc->setState(zesDevice, enable));

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

/**************************************************************************/
/** C API Implementation - Telemetry/Stats APIs
 */
/**************************************************************************/

/**
 * @brief Get current temperature readings
 */
XPUM_API xpum_result_t xpumGetTemperature(xpum_device_id_t deviceId, double *coreTemp, double *memTemp)
{
	try {
		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		auto *temp = dev->getTemperature();
		if (temp == nullptr) {
			return XPUM_METRIC_NOT_SUPPORTED;
		}

		xpum_result_t tempResult = XPUM_OK;

		if (coreTemp != nullptr) {
			tempResult = toXpumResult(temp->getCoreTemp(coreTemp));
			if (tempResult != XPUM_OK) {
				return tempResult;
			}
		}

		if (memTemp != nullptr) {
			tempResult = toXpumResult(temp->getMemoryTemp(memTemp));
		}

		return tempResult;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

/**
 * @brief Get current power consumption
 */
XPUM_API xpum_result_t xpumGetPower(xpum_device_id_t deviceId, uint64_t *power, uint64_t *timestamp)
{
	try {
		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		auto *pwr = dev->getPower();
		if (pwr == nullptr) {
			return XPUM_METRIC_NOT_SUPPORTED;
		}

		return toXpumResult(pwr->getEnergy(power, timestamp, true));

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

/**
 * @brief Get current frequency
 */
XPUM_API xpum_result_t xpumGetFrequency(xpum_device_id_t deviceId, double *currentFreq, xpum_freq_domain_t domain)
{
	try {
		if (currentFreq == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		auto *freq = dev->getFrequency();
		if (freq == nullptr) {
			return XPUM_METRIC_NOT_SUPPORTED;
		}

		return toXpumResult(freq->getCurFreq(currentFreq, toZesFreqDomain(domain)));

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

/**
 * @brief Get frequency throttle reasons
 */
XPUM_API xpum_result_t xpumGetFrequencyThrottleReasons(xpum_device_id_t deviceId,
													   xpum_freq_throttle_reason_flags_t *reasons)
{
	try {
		if (reasons == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		auto *freq = dev->getFrequency();
		if (freq == nullptr) {
			return XPUM_METRIC_NOT_SUPPORTED;
		}

		zes_freq_throttle_reason_flags_t zesReasons;
		result = toXpumResult(freq->getThrottleReason(&zesReasons));
		if (result == XPUM_OK) {
			*reasons = toXpumThrottleReasons(zesReasons);
		}
		return result;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

/**
 * @brief Get memory usage statistics
 */
XPUM_API xpum_result_t xpumGetMemoryUsage(xpum_device_id_t deviceId, uint64_t *usedMemory, double *utilization)
{
	try {
		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		auto *mem = dev->getMemory();
		if (mem == nullptr) {
			return XPUM_METRIC_NOT_SUPPORTED;
		}

		return toXpumResult(mem->getMemoryUsed(usedMemory, utilization));

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

/**
 * @brief Get memory bandwidth statistics
 */
XPUM_API xpum_result_t xpumGetMemoryBandwidth(xpum_device_id_t deviceId, uint64_t *readBandwidth,
											  uint64_t *writeBandwidth, uint64_t *maxBandwidth, uint64_t *timestamp)
{
	try {
		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		auto *mem = dev->getMemory();
		if (mem == nullptr) {
			return XPUM_METRIC_NOT_SUPPORTED;
		}

		return toXpumResult(mem->getMemoryRW(readBandwidth, writeBandwidth, maxBandwidth, timestamp));

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

/**
 * @brief Get RAS error counts
 */
XPUM_API xpum_result_t xpumGetRASErrors(xpum_device_id_t deviceId, xpum_ras_error_cat_t category,
										xpum_ras_error_type_t errorType, uint64_t *errorCount)
{
	try {
		if (errorCount == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		auto *ras = dev->getRAS();
		if (ras == nullptr) {
			return XPUM_METRIC_NOT_SUPPORTED;
		}

		return toXpumResult(ras->getErrors(toZesRasErrorCat(category), toZesRasErrorType(errorType), errorCount));

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

/** C API Implementation - Group APIs
 */
/**************************************************************************/

XPUM_API xpum_result_t xpumGroupCreate(const char *groupName, xpum_group_id_t *pGroupId)
{
	try {
		if (groupName == nullptr || pGroupId == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		if (!isInitialized(state)) {
			return XPUM_NOT_INITIALIZED;
		}

		xpum_group_id_t const newGroupId = state.context->nextGroupId++;
		state.context->groups[newGroupId] = APIContext::GroupInfo{.name = groupName, .devices = {}};

		*pGroupId = newGroupId;
		return XPUM_OK;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

XPUM_API xpum_result_t xpumGroupDestroy(xpum_group_id_t groupId)
{
	try {
		auto &state = getState();
		std::lock_guard const lock(state.mutex);

		if (!isInitialized(state)) {
			return XPUM_NOT_INITIALIZED;
		}

		const auto iter = state.context->groups.find(groupId);
		if (iter == state.context->groups.end()) {
			return XPUM_RESULT_INVALID_DIR;
		}

		state.context->groups.erase(iter);
		return XPUM_OK;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

XPUM_API xpum_result_t xpumGroupAddDevice(xpum_group_id_t groupId, xpum_device_id_t deviceId)
{
	try {
		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		if (!isInitialized(state)) {
			return XPUM_NOT_INITIALIZED;
		}

		if (!state.context->groups.contains(groupId)) {
			return XPUM_RESULT_INVALID_DIR;
		}

		// Validate device exists
		device *dev = nullptr;
		if (getDevice(state, deviceId, &dev) != XPUM_OK) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &devices = state.context->groups[groupId].devices;
		if (std::ranges::find(devices, deviceId) == devices.end()) {
			devices.push_back(deviceId);
		}

		return XPUM_OK;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

XPUM_API xpum_result_t xpumGroupRemoveDevice(xpum_group_id_t groupId, xpum_device_id_t deviceId)
{
	try {
		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		if (!isInitialized(state)) {
			return XPUM_NOT_INITIALIZED;
		}

		if (!state.context->groups.contains(groupId)) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &devices = state.context->groups[groupId].devices;
		std::erase(devices, deviceId);
		return XPUM_OK;
	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

XPUM_API xpum_result_t xpumGroupGetInfo(xpum_group_id_t groupId, xpum_group_info_t *pGroupInfo)
{
	try {
		if (pGroupInfo == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}
		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		if (!isInitialized(state)) {
			return XPUM_NOT_INITIALIZED;
		}

		if (auto iter = state.context->groups.find(groupId); iter != state.context->groups.end()) {
			toCstr(pGroupInfo->groupName, iter->second.name);
			pGroupInfo->count = static_cast<int>(iter->second.devices.size());

			// Copy device IDs
			const size_t maxDevices = std::min<size_t>(iter->second.devices.size(), XPUM_MAX_NUM_DEVICES);
			std::copy_n(iter->second.devices.begin(), maxDevices, pGroupInfo->deviceList);
		} else {
			return XPUM_RESULT_INVALID_DIR;
		}

		return XPUM_OK;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

XPUM_API xpum_result_t xpumGetAllGroupIds(xpum_group_id_t groupIds[], int *count)
{
	try {
		if (count == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		if (!isInitialized(state)) {
			return XPUM_NOT_INITIALIZED;
		}

		const int groupCount = static_cast<int>(state.context->groups.size());

		if (groupIds == nullptr) {
			*count = groupCount;
			return XPUM_OK;
		}

		if (*count < groupCount) {
			return XPUM_RESULT_INVALID_DIR;
		}

		int idx = 0;
		for (const auto &[group_id, _] : state.context->groups) {
			groupIds[idx++] = group_id;
		}

		*count = idx;
		return XPUM_OK;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

/**************************************************************************/
/** C API Implementation - Health APIs
 */
/**************************************************************************/

// Health configuration thresholds stored per device
struct HealthConfig
{
	uint64_t coreThermalLimit = 105;  // Celsius
	uint64_t memoryThermalLimit = 85; // Celsius
	uint64_t powerLimit = 300;		  // Watts
};

// Store health configs in context
namespace {
std::unordered_map<xpum_device_id_t, HealthConfig> gHealthConfigs;
std::mutex gHealthMutex;
} // namespace

XPUM_API xpum_result_t xpumSetHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value)
{
	try {
		if (value == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &state = getState();
		const std::scoped_lock lock(state.mutex, gHealthMutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		auto &config = gHealthConfigs[deviceId];
		auto *threshold = static_cast<uint64_t *>(value);

		switch (key) {
		case XPUM_HEALTH_CORE_THERMAL_LIMIT:
			if (*threshold == 0 || *threshold > 200) {
				return XPUM_RESULT_INVALID_DIR;
			}
			config.coreThermalLimit = *threshold;
			break;

		case XPUM_HEALTH_MEMORY_THERMAL_LIMIT:
			if (*threshold == 0 || *threshold > 200) {
				return XPUM_RESULT_INVALID_DIR;
			}
			config.memoryThermalLimit = *threshold;
			break;

		case XPUM_HEALTH_POWER_LIMIT:
			if (*threshold == 0) {
				return XPUM_RESULT_INVALID_DIR;
			}
			config.powerLimit = *threshold;
			break;

		default:
			return XPUM_RESULT_INVALID_DIR;
		}

		return XPUM_OK;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

XPUM_API xpum_result_t xpumSetHealthConfigByGroup(xpum_group_id_t groupId, xpum_health_config_type_t key, void *value)
{
	try {
		if (value == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		if (!isInitialized(state)) {
			return XPUM_NOT_INITIALIZED;
		}

		if (!state.context->groups.contains(groupId)) {
			return XPUM_RESULT_INVALID_DIR;
		}

		// Apply to all devices in group
		for (auto deviceId : state.context->groups[groupId].devices) {
			auto result = xpumSetHealthConfig(deviceId, key, value);
			if (result != XPUM_OK) {
				return result;
			}
		}

		return XPUM_OK;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

XPUM_API xpum_result_t xpumGetHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value)
{
	try {
		if (value == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &state = getState();
		const std::scoped_lock lock(state.mutex, gHealthMutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		const auto &config = gHealthConfigs[deviceId];
		auto *threshold = static_cast<uint64_t *>(value);

		switch (key) {
		case XPUM_HEALTH_CORE_THERMAL_LIMIT:
			*threshold = config.coreThermalLimit;
			break;

		case XPUM_HEALTH_MEMORY_THERMAL_LIMIT:
			*threshold = config.memoryThermalLimit;
			break;

		case XPUM_HEALTH_POWER_LIMIT:
			*threshold = config.powerLimit;
			break;

		default:
			return XPUM_RESULT_INVALID_DIR;
		}

		return XPUM_OK;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

XPUM_API xpum_result_t xpumGetHealthConfigByGroup(xpum_group_id_t groupId, xpum_health_config_type_t key, void *value)
{
	try {
		if (value == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		if (!isInitialized(state)) {
			return XPUM_NOT_INITIALIZED;
		}

		if (!state.context->groups.contains(groupId)) {
			return XPUM_RESULT_INVALID_DIR;
		}

		// Return config from first device in group
		if (!state.context->groups[groupId].devices.empty()) {
			return xpumGetHealthConfig(state.context->groups[groupId].devices[0], key, value);
		}

		return XPUM_RESULT_INVALID_DIR;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

XPUM_API xpum_result_t xpumGetHealth(xpum_device_id_t deviceId, xpum_health_type_t type, xpum_health_data_t *data)
{
	try {
		DBG("xpumGetHealth: deviceId=%d, type=%d\n", deviceId, type);
		if (data == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &state = getState();
		const std::scoped_lock lock(state.mutex, gHealthMutex);

		device *dev = nullptr;
		xpum_result_t result = getDevice(state, deviceId, &dev);
		if (result != XPUM_OK) {
			return result;
		}

		data->description[0] = '\0';

		auto &config = gHealthConfigs[deviceId];

		switch (type) {
		case XPUM_HEALTH_CORE_THERMAL: {
			auto *temp = dev->getTemperature();
			if (!temp)
				return XPUM_METRIC_NOT_SUPPORTED;

			double coreTemp = 0;
			if (toXpumResult(temp->getCoreTemp(&coreTemp)) == XPUM_OK) {
				data->throttleThreshold = config.coreThermalLimit;
				data->shutdownThreshold = config.coreThermalLimit + 25;
				data->status = (coreTemp >= static_cast<double>(config.coreThermalLimit)) ? XPUM_HEALTH_STATUS_CRITICAL
																						  : XPUM_HEALTH_STATUS_OK;
				toCstr(data->description, "{:.1f}°C", coreTemp);
			} else {
				toCstr(data->description, "Unable to read");
			}
			break;
		}

		case XPUM_HEALTH_MEMORY_THERMAL: {
			auto *temp = dev->getTemperature();
			if (!temp)
				return XPUM_METRIC_NOT_SUPPORTED;

			double memTemp = 0;
			if (toXpumResult(temp->getMemoryTemp(&memTemp)) == XPUM_OK) {
				data->throttleThreshold = config.memoryThermalLimit;
				data->shutdownThreshold = config.memoryThermalLimit + 15;
				data->status = (memTemp >= static_cast<double>(config.memoryThermalLimit)) ? XPUM_HEALTH_STATUS_CRITICAL
																						   : XPUM_HEALTH_STATUS_OK;
				toCstr(data->description, "{:.1f}°C", memTemp);
			} else {
				toCstr(data->description, "Unable to read");
			}
			break;
		}

		case XPUM_HEALTH_POWER: {
			auto *pwr = dev->getPower();
			if (!pwr)
				return XPUM_METRIC_NOT_SUPPORTED;

			uint64_t powerMw = 0, timestamp = 0;
			if (toXpumResult(pwr->getEnergy(&powerMw, &timestamp, true)) == XPUM_OK) {
				uint64_t const powerW = powerMw / 1000;
				data->throttleThreshold = config.powerLimit;
				data->shutdownThreshold = config.powerLimit + 50;

				if (static_cast<double>(powerW) >= static_cast<double>(config.powerLimit)) {
					data->status = XPUM_HEALTH_STATUS_CRITICAL;
					toCstr(data->description, "Power: {}W (limit: {}W)", powerW, config.powerLimit);
				} else if (static_cast<double>(powerW) >= static_cast<double>(config.powerLimit) * 0.8) {
					data->status = XPUM_HEALTH_STATUS_WARNING;
					toCstr(data->description, "Power: {}W (limit: {}W)", powerW, config.powerLimit);
				} else {
					data->status = XPUM_HEALTH_STATUS_OK;
					toCstr(data->description, "Power: {}W", powerW);
				}
			}
			break;
		}

		case XPUM_HEALTH_MEMORY: {
			auto *mem = dev->getMemory();
			if (!mem)
				return XPUM_METRIC_NOT_SUPPORTED;

			zes_mem_health_t health;
			if (toXpumResult(mem->getMemoryHealth(&health)) == XPUM_OK) {
				switch (health) {
				case ZES_MEM_HEALTH_OK:
					data->status = XPUM_HEALTH_STATUS_OK;
					toCstr(data->description, "OK");
					break;
				case ZES_MEM_HEALTH_DEGRADED:
					data->status = XPUM_HEALTH_STATUS_WARNING;
					toCstr(data->description, "Degraded");
					break;
				case ZES_MEM_HEALTH_CRITICAL:
				case ZES_MEM_HEALTH_REPLACE:
					data->status = XPUM_HEALTH_STATUS_CRITICAL;
					toCstr(data->description, health == ZES_MEM_HEALTH_REPLACE ? "Replace" : "Critical");
					break;
				default:
					toCstr(data->description, "Unknown");
					break;
				}
			}
			break;
		}

		case XPUM_HEALTH_FREQUENCY: {
			auto *freq = dev->getFrequency();
			if (!freq)
				return XPUM_METRIC_NOT_SUPPORTED;

			zes_freq_throttle_reason_flags_t reasons = 0;
			if (toXpumResult(freq->getThrottleReason(&reasons)) == XPUM_OK) {
				if (reasons == 0) {
					data->status = XPUM_HEALTH_STATUS_OK;
					toCstr(data->description, "No throttling");
				} else {
					data->status = XPUM_HEALTH_STATUS_WARNING;
					toCstr(data->description, "Throttled: {:#x}", static_cast<unsigned int>(reasons));
				}
			}
			break;
		}

		case XPUM_HEALTH_FABRIC_PORT: {
			auto *fabric = dev->getFabric();
			if (!fabric)
				return XPUM_METRIC_NOT_SUPPORTED;

			// TODO: Implement fabric port health via HAL
			data->status = XPUM_HEALTH_STATUS_OK;
			toCstr(data->description, "OK");
			break;
		}

		default:
			return XPUM_RESULT_INVALID_DIR;
		}

		return XPUM_OK;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

XPUM_API xpum_result_t xpumGetHealthByGroup(xpum_group_id_t groupId, xpum_health_type_t type, xpum_health_data_t *data)
{
	try {
		if (data == nullptr) {
			return XPUM_RESULT_INVALID_DIR;
		}

		auto &state = getState();
		const std::scoped_lock lock(state.mutex);

		if (!isInitialized(state)) {
			return XPUM_NOT_INITIALIZED;
		}

		if (!state.context->groups.contains(groupId)) {
			return XPUM_RESULT_INVALID_DIR;
		}

		// Return worst health status from all devices in group
		xpum_health_status_t worstStatus = XPUM_HEALTH_STATUS_OK;
		xpum_health_data_t worstData = {};

		for (auto deviceId : state.context->groups[groupId].devices) {
			xpum_health_data_t deviceData = {};
			auto result = xpumGetHealth(deviceId, type, &deviceData);

			if (result == XPUM_OK && deviceData.status > worstStatus) {
				worstStatus = deviceData.status;
				worstData = deviceData;
			}
		}

		*data = worstData;
		return XPUM_OK;

	} catch (const std::exception &e) {
		return XPUM_GENERIC_ERROR;
	}
}

} // extern "C"