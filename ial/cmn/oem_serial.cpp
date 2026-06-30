/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "oem_serial.h"

#include <debug.h>

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
#include <memory>
#include <ranges>
#include <span>

namespace {

struct IgscDeviceCloser
{
	void operator()(igsc_device_handle *h) const noexcept { igsc_device_close(h); }
};

constexpr unsigned char ASCII_PRINTABLE_MIN = 0x20;
constexpr unsigned char ASCII_PRINTABLE_MAX = 0x7E;

} // namespace

/**
 * @brief Retrieves an OEM serial number using a MEI device path.
 *
 * Initializes a device context for the provided path, queries the OEM serial
 * number, and sanitizes it to printable ASCII characters up to the first null
 * or non-printable byte.
 *
 * @param meiDevicePath MEI device path used to open the target device.
 * @param serialNumber Output string receiving the sanitized OEM serial number.
 * @return 0 on success, -1 on failure.
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

	std::ranges::copy(
	    std::span{serial.sn, maxLen}
	        | std::views::take_while([](unsigned char c) {
	              return c >= ASCII_PRINTABLE_MIN && c <= ASCII_PRINTABLE_MAX;
	          })
	        | std::views::transform([](unsigned char c) { return static_cast<char>(c); }),
	    std::back_inserter(serialNumber));

	if (serialNumber.empty()) {
		DBG("OEM serial number is empty after processing\n");
		return -1;
	}

	return 0;
}
