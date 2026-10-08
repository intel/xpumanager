/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "oem_serial.h"

#include <debug.h>
#include <device.h>
#include <firmware.h>
#include <pci.h>
#include "zes_api.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4200)
#endif
#include <igsc_lib.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <algorithm>
#include <array>
#include <memory>
#include <ranges>
#include <span>
#include <string_view>
#include <cstring>

namespace {

struct IgscDeviceCloser
{
	void operator()(igsc_device_handle *h) const noexcept { igsc_device_close(h); }
};

constexpr unsigned char ASCII_PRINTABLE_MIN = 0x20;
constexpr unsigned char ASCII_PRINTABLE_MAX = 0x7E;

/** Value Sysman reports when the board serial number is not available. */
constexpr std::string_view SYSMAN_UNKNOWN_SERIAL = "unknown";

/**
 * @brief Fetches the serial number of the given device from the AMC FRU data.
 *
 * @param[in]  d            Device info structure.
 * @param[out] serialNumber Populated with the FRU serial number on success.
 *
 * @retval true  A non-empty serial number was retrieved.
 * @retval false The device has no AMC, or the AMC query failed.
 */
bool querySerialNumberFromAMC(devInfo &d, std::string &serialNumber)
{
	// hasAmc() returns false when no AMC is present, avoiding any I2C traffic.
	if (!d.dev->hasAmc()) {
		return false;
	}

	firmware *fw = d.dev->getFirmware();
	if (fw == nullptr) {
		return false;
	}

	std::array<char, 256> serialNum{};
	const auto bdf = d.dev->getPCI()->getBDFStr();
	const ze_result_t result =
		fw->getAmcSerialNumber(bdf.c_str(), serialNum.data(), static_cast<uint32_t>(serialNum.size()));
	if (result != ZE_RESULT_SUCCESS || serialNum[0] == '\0') {
		DBG("Failed to get serial number from AMC for device {} (result: 0x{:X})\n", bdf.c_str(), result);
		return false;
	}

	serialNumber.assign(serialNum.data(), strnlen(serialNum.data(), serialNum.size()));
	return true;
}

} // namespace

/**
 * @brief Retrieves an OEM serial number using a MEI device path.
 *
 * Initializes a device context for the provided path, queries the OEM serial
 * number, and sanitizes it to printable ASCII characters up to the first null
 * or non-printable byte.
 *
 * @param[in]  meiDevicePath MEI device path used to open the target device.
 * @param[out] serialNumber  Receives the sanitized OEM serial number.
 *
 * @retval 0  Success.
 * @retval -1 Failure.
 */
int getOemSerialNumberByMeiPath(const std::string &meiDevicePath, std::string &serialNumber)
{
	serialNumber.clear();

	if (meiDevicePath.empty()) {
		DBG("MEI device path is empty\n");
		return -1;
	}

	igsc_device_handle handle{};
	if (igsc_device_init_by_device(&handle, meiDevicePath.c_str()) != IGSC_SUCCESS) {
		DBG("Failed to initialize igsc device for path: {}\n", meiDevicePath.c_str());
		return -1;
	}
	const std::unique_ptr<igsc_device_handle, IgscDeviceCloser> device{&handle};

	igsc_oem_serial_number serial{};
	const int snRet = igsc_device_oem_serial_number(device.get(), &serial);
	if (snRet != IGSC_SUCCESS) {
		DBG("igsc_device_oem_serial_number failed: {}\n", snRet);
		return -1;
	}
	if (serial.length == 0) {
		DBG("igsc_device_oem_serial_number returned length 0\n");
		return -1;
	}

	const size_t maxLen = std::min<size_t>(serial.length, sizeof(serial.sn));

	std::ranges::copy(std::span{serial.sn, maxLen} | std::views::take_while([](unsigned char c) {
						  return c >= ASCII_PRINTABLE_MIN && c <= ASCII_PRINTABLE_MAX;
					  }) | std::views::transform([](unsigned char c) { return static_cast<char>(c); }),
					  std::back_inserter(serialNumber));

	if (serialNumber.empty()) {
		DBG("OEM serial number is empty after processing\n");
		return -1;
	}

	return 0;
}

/**
 * @brief Resolves the board serial number of a device.
 *
 * Sysman is queried first. When it reports "unknown", the AMC FRU data is
 * tried (if the device has an AMC), followed by the OEM serial number via IGSC.
 *
 * @param[in]  d            Device info structure.
 * @param[out] serialNumber Receives the serial number, or "unknown" if no source provides one.
 *
 * @retval ZE_RESULT_SUCCESS Sysman device properties were read; @p serialNumber is populated.
 * @retval ZE_RESULT_ERROR_* Sysman device properties could not be read.
 */
ze_result_t querySerialNumber(devInfo &d, std::string &serialNumber)
{
	zes_device_properties_t props{};
	props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
	const auto result = d.dev->zesGetDevProps(d.zesDeviceHdl, &props);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}
	serialNumber = props.serialNumber;
	if (serialNumber != SYSMAN_UNKNOWN_SERIAL) {
		return ZE_RESULT_SUCCESS;
	}

	std::string fallback;
	if (querySerialNumberFromAMC(d, fallback)) {
		DBG("Successfully retrieved serial number from AMC: {}\n", fallback.c_str());
		serialNumber = std::move(fallback);
		return ZE_RESULT_SUCCESS;
	}

	auto *pci = d.dev->getPCI();
	if (pci != nullptr && getOemSerialNumberByMeiPath(pci->getMeiDevicePath(), fallback) == 0) {
		DBG("Successfully retrieved OEM serial number from IGSC: {}\n", fallback.c_str());
		serialNumber = std::move(fallback);
	} else {
		DBG("Failed to get OEM serial number from IGSC or No IGSC Available\n");
	}

	return ZE_RESULT_SUCCESS;
}
