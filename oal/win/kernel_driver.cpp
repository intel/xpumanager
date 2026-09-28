/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include <windows.h>
// initguid.h must precede devpkey.h so the DEVPKEY_* property keys get storage
// in this translation unit rather than being declared extern.
#include <initguid.h>
#include <devpkey.h>
#include <cfgmgr32.h>
#include <setupapi.h>
#include <array>
#include <string>
#include "os.h"
#include "utility/compat/format.h"
#include "utility/logger/logger.h"
#include "utility/scope_exit.h"

namespace {

constexpr ULONG PCI_ADDRESS_DEVICE_SHIFT = 16;
constexpr ULONG PCI_ADDRESS_FUNCTION_MASK = 0x7;

/**
 * @brief Returns the "0000:bb:dd.f" address of a PCI devnode, or "" if it is not one.
 *
 * Windows exposes bus and "address" (device in the high half, function in the low
 * three bits) as separate devnode properties, with no domain: single-segment is
 * assumed, matching the rest of the Windows paths in this layer.
 *
 * @param[in] devInst  Devnode to inspect.
 *
 * @retval bdf  "0000:bb:dd.f" for a PCI devnode.
 * @retval ""   @p devInst is not enumerated by PCI, or its bus/address properties
 *              could not be read.
 */
[[nodiscard]] std::string devnodeBdf(DEVINST devInst)
{
	std::array<wchar_t, MAX_DEVICE_ID_LEN> deviceId{};
	if (CM_Get_Device_IDW(devInst, deviceId.data(), static_cast<ULONG>(deviceId.size()), 0) != CR_SUCCESS) {
		return {};
	}
	if (wcsncmp(deviceId.data(), L"PCI\\", 4) != 0) {
		return {};
	}

	ULONG busNumber = 0;
	ULONG address = 0;
	ULONG len = sizeof(ULONG);
	if (CM_Get_DevNode_Registry_PropertyW(devInst, CM_DRP_BUSNUMBER, nullptr, &busNumber, &len, 0) != CR_SUCCESS) {
		return {};
	}
	len = sizeof(ULONG);
	if (CM_Get_DevNode_Registry_PropertyW(devInst, CM_DRP_ADDRESS, nullptr, &address, &len, 0) != CR_SUCCESS) {
		return {};
	}

	const ULONG device = (address >> PCI_ADDRESS_DEVICE_SHIFT) & 0xFFU;
	const ULONG function = address & PCI_ADDRESS_FUNCTION_MASK;
	return xpum::compat::format("0000:{:02x}:{:02x}.{}", busNumber & 0xFFU, device, function);
}

/**
 * @brief Reads a string-typed property of a devnode.
 *
 * @param[in] devInst  Devnode to query.
 * @param[in] key      Property key, e.g. DEVPKEY_Device_DriverVersion.
 *
 * @retval value  The property, converted from UTF-16 and without its terminator.
 * @retval ""     The property is absent, empty, or not of type DEVPROP_TYPE_STRING.
 */
[[nodiscard]] std::string devnodeStringProperty(DEVINST devInst, const DEVPROPKEY &key)
{
	DEVPROPTYPE type = 0;
	ULONG size = 0;
	// First call sizes the buffer; CR_BUFFER_SMALL is the documented success path.
	if (CM_Get_DevNode_PropertyW(devInst, &key, &type, nullptr, &size, 0) != CR_BUFFER_SMALL || size == 0) {
		return {};
	}
	if (type != DEVPROP_TYPE_STRING) {
		return {};
	}

	std::wstring value(size / sizeof(wchar_t), L'\0');
	// reinterpret_cast is required: CM_Get_DevNode_PropertyW takes a byte buffer for
	// every property type, and the caller is responsible for matching DEVPROPTYPE
	// (checked above) to the buffer's real type.
	if (CM_Get_DevNode_PropertyW(devInst, &key, &type, reinterpret_cast<PBYTE>(value.data()), &size, 0) != CR_SUCCESS) {
		return {};
	}
	// The property is NUL-terminated inside the buffer; drop the terminator and any
	// slack so the result compares equal to the version string users see.
	if (const size_t nul = value.find(L'\0'); nul != std::wstring::npos) {
		value.resize(nul);
	}
	return wtos(value.c_str());
}

} // namespace

/**
 * @brief Returns a version string identifying the kernel-mode driver behind a GPU.
 *
 * Level Zero cannot answer this on Windows: sysman's WddmGlobalOperationsImp
 * leaves zes_device_properties_t.driverVersion empty, so the value has to come
 * from the OS. The Plug and Play manager holds it as the display adapter's
 * DriverVersion property -- the four-part number users quote from Device Manager
 * and from driver release notes (e.g. "32.0.101.6458") -- which is also what ties
 * a running driver to the package it was installed from.
 *
 * Enumeration is restricted to the PCI enumerator and matched on @p bdf, because
 * a system can have several display adapters on different driver versions.
 *
 * @param[in] bdf  PCI address of the target GPU, e.g. "0000:04:00.0". An empty
 *                 string matches the first PCI display adapter found.
 *
 * @retval version  The four-part driver version, e.g. "32.0.101.6458".
 * @retval ""       No devnode matched @p bdf, or it exposes no driver version;
 *                  callers treat this as unavailable rather than failing.
 */
std::string getKernelDriverVersion(const std::string &bdf)
{
	HDEVINFO devInfo = SetupDiGetClassDevsW(nullptr, L"PCI", nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
	if (devInfo == INVALID_HANDLE_VALUE) {
		DBG("SetupDiGetClassDevs failed: {}\n", GetLastError());
		return {};
	}
	ScopeExit cleanup{[devInfo]() { SetupDiDestroyDeviceInfoList(devInfo); }};

	SP_DEVINFO_DATA devData{};
	devData.cbSize = sizeof(SP_DEVINFO_DATA);
	for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devData) != 0; i++) {
		const std::string devBdf = devnodeBdf(devData.DevInst);
		if (devBdf.empty()) {
			continue;
		}
		if (bdf.empty()) {
			// Without a BDF to match, the only safe filter is the device class:
			// the PCI enumerator returns every device on the bus, and the first
			// one is not a GPU.
			if (devnodeStringProperty(devData.DevInst, DEVPKEY_Device_Class) != "Display") {
				continue;
			}
		} else if (devBdf != bdf) {
			continue;
		}

		std::string version = devnodeStringProperty(devData.DevInst, DEVPKEY_Device_DriverVersion);
		if (version.empty()) {
			DBG("No DriverVersion property for {}\n", devBdf);
		}
		return version;
	}

	DBG("No PCI devnode matched {}\n", bdf);
	return {};
}
