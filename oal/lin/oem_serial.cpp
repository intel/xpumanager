/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <os.h>

#include <algorithm>
#include <cctype>
#include <dlfcn.h>
#include <debug.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <igsc_lib.h>

namespace {

#define IGSC_MAX_OEM_SN_LENGTH_LOCAL 512

struct IgscOemSerialNumberLocal
{
	uint16_t length;
	uint8_t sn[IGSC_MAX_OEM_SN_LENGTH_LOCAL];
};

struct IgscLibGuard
{
	void *handle = nullptr;

	/**
	 * @brief Closes the dynamically loaded IGSC library handle.
	 */
	~IgscLibGuard()
	{
		if (handle)
			dlclose(handle);
	}

	IgscLibGuard(const IgscLibGuard &) = delete;
	IgscLibGuard &operator=(const IgscLibGuard &) = delete;

	/**
	 * @brief Creates an empty library guard.
	 */
	IgscLibGuard() = default;

	/**
	 * @brief Attempts to load the IGSC shared library from a path.
	 *
	 * @param path Shared library path.
	 * @return true if a library handle is available after this call, false otherwise.
	 */
	bool tryLoad(const char *path)
	{
		if (!handle)
			handle = dlopen(path, RTLD_NOW);
		return handle != nullptr;
	}

	/**
	 * @brief Resolves a symbol from the loaded IGSC shared library.
	 *
	 * @tparam F Function pointer type to cast the symbol to.
	 * @param name Symbol name.
	 * @return Resolved symbol cast to type @p F.
	 */
	template <typename F> F sym(const char *name) const { return reinterpret_cast<F>(dlsym(handle, name)); }

	/**
	 * @brief Checks whether a library handle has been loaded.
	 *
	 * @return true if the handle is valid, false otherwise.
	 */
	explicit operator bool() const noexcept { return handle != nullptr; }
};

struct IgscDeviceGuard
{
	igsc_device_handle handle = {};
	int (*closeFn)(igsc_device_handle *) = nullptr;
	bool valid = false;

	/**
	 * @brief Initializes an IGSC device handle for a given MEI path.
	 *
	 * @param initFn IGSC device init function.
	 * @param path MEI device path.
	 * @param closeFnIn IGSC device close function.
	 */
	IgscDeviceGuard(int (*initFn)(igsc_device_handle *, const char *), const char *path,
					int (*closeFnIn)(igsc_device_handle *)) noexcept
		: closeFn(closeFnIn), valid(initFn(&handle, path) == IGSC_SUCCESS)
	{}

	/**
	 * @brief Closes the IGSC device handle when it is valid.
	 */
	~IgscDeviceGuard()
	{
		if (valid && closeFn)
			closeFn(&handle);
	}

	IgscDeviceGuard(const IgscDeviceGuard &) = delete;
	IgscDeviceGuard &operator=(const IgscDeviceGuard &) = delete;

	/**
	 * @brief Checks whether device initialization succeeded.
	 *
	 * @return true if the device is valid, false otherwise.
	 */
	explicit operator bool() const noexcept { return valid; }
};

} // namespace

/**
 * @brief Retrieves an OEM serial number using a Linux MEI device path.
 *
 * Loads libigsc dynamically, initializes a device context for the provided path,
 * queries the OEM serial number, and sanitizes it to printable characters.
 *
 * @param meiDevicePath Linux MEI device path used to open the target device.
 * @param serialNumber Output string receiving the sanitized OEM serial number.
 * @return 0 on success, -1 on failure.
 */
int getOemSerialNumberByMeiPath(const std::string &meiDevicePath, std::string &serialNumber)
{
	const std::string igscLibPath{"libigsc.so.1"};
	const std::string igscLibPathFallback{"libigsc.so"};
	const size_t maxSerialLen = IGSC_MAX_OEM_SN_LENGTH_LOCAL;

	serialNumber.clear();
	if (meiDevicePath.empty()) {
		DBG("MEI device path is empty\n");
		return -1;
	}

	IgscLibGuard lib;
	if (!lib.tryLoad(igscLibPath.c_str()))
		lib.tryLoad(igscLibPathFallback.c_str());
	if (!lib) {
		DBG("Failed to load igsc library\n");
		return -1;
	}

	auto igscDeviceInitByDevice =
		lib.sym<int (*)(struct igsc_device_handle *, const char *)>("igsc_device_init_by_device");
	auto igscDeviceClose = lib.sym<int (*)(struct igsc_device_handle *)>("igsc_device_close");
	auto igscDeviceOemSerialNumber = lib.sym<int (*)(struct igsc_device_handle *, struct IgscOemSerialNumberLocal *)>(
		"igsc_device_oem_serial_number");

	if (igscDeviceInitByDevice == nullptr || igscDeviceClose == nullptr || igscDeviceOemSerialNumber == nullptr) {
		DBG("Failed to get required igsc function symbols\n");
		return -1;
	}

	IgscDeviceGuard device{igscDeviceInitByDevice, meiDevicePath.c_str(), igscDeviceClose};
	if (!device) {
		DBG("Failed to initialize igsc device for path: %s\n", meiDevicePath.c_str());
		return -1;
	}

	struct IgscOemSerialNumberLocal serial = {};
	const int ret = igscDeviceOemSerialNumber(&device.handle, &serial);
	if (ret != IGSC_SUCCESS || serial.length == 0) {
		DBG("Failed to get OEM serial number, ret=%d, serial.length=%u\n", ret, serial.length);
		return -1;
	}

	size_t actualLen = 0;
	const size_t maxLen = std::min<uint16_t>(serial.length, sizeof(serial.sn));
	char oemSerialNumber[sizeof(serial.sn) + 1] = {0};
	for (size_t i = 0; i < maxLen && actualLen < (maxSerialLen - 1); i++) {
		if (serial.sn[i] == 0 || !std::isprint(static_cast<unsigned char>(serial.sn[i])))
			break;
		oemSerialNumber[actualLen++] = static_cast<char>(serial.sn[i]);
	}
	oemSerialNumber[actualLen] = '\0';

	serialNumber.assign(oemSerialNumber, actualLen);
	if (serialNumber.empty()) {
		DBG("OEM serial number is empty after processing\n");
		return -1;
	}
	return 0;
}
