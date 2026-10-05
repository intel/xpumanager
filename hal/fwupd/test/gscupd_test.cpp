/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef INFO
#undef INFO
#endif

#include "gscupd.h"
#include <cstddef>
#include <igsc_lib.h>
#include <string>

namespace {

constexpr char MEI_DEVICE_PATH[] = "/dev/mei0";

struct IgscMockState
{
	int initResult = IGSC_SUCCESS;
	int versionResult = IGSC_SUCCESS;
	int closeResult = IGSC_SUCCESS;
	int iteratorCreateResult = IGSC_SUCCESS;
	uint32_t firmwareStatus = 0;
	int initCalls = 0;
	int versionCalls = 0;
	int statusCalls = 0;
	int closeCalls = 0;
	int iteratorDestroyCalls = 0;
	bool returnIteratorOnCreateFailure = false;
};

IgscMockState mock;
alignas(std::max_align_t) std::byte fakeContextStorage;
alignas(std::max_align_t) std::byte fakeIteratorStorage;
igsc_lib_ctx *const fakeContext = reinterpret_cast<igsc_lib_ctx *>(&fakeContextStorage);

void resetMock() { mock = {}; }

} // namespace

extern "C" int __wrap_igsc_device_init_by_device(igsc_device_handle *handle, const char *)
{
	++mock.initCalls;
	if (mock.initResult == IGSC_SUCCESS) {
		handle->ctx = fakeContext;
	}
	return mock.initResult;
}

extern "C" int __wrap_igsc_device_fw_version(igsc_device_handle *, igsc_fw_version *)
{
	++mock.versionCalls;
	return mock.versionResult;
}

extern "C" uint32_t __wrap_igsc_get_last_firmware_status(igsc_device_handle *)
{
	++mock.statusCalls;
	return mock.firmwareStatus;
}

extern "C" int __wrap_igsc_device_close(igsc_device_handle *handle)
{
	++mock.closeCalls;
	handle->ctx = nullptr;
	return mock.closeResult;
}

extern "C" int __wrap_igsc_device_iterator_create(igsc_device_iterator **iter)
{
	if (mock.iteratorCreateResult == IGSC_SUCCESS || mock.returnIteratorOnCreateFailure) {
		*iter = reinterpret_cast<igsc_device_iterator *>(&fakeIteratorStorage);
	} else {
		*iter = nullptr;
	}
	return mock.iteratorCreateResult;
}

extern "C" void __wrap_igsc_device_iterator_destroy(igsc_device_iterator *) { ++mock.iteratorDestroyCalls; }

TEST_CASE("GFX firmware status is unknown when no MEI device path is available")
{
	resetMock();
	gscupd updater;

	CHECK(updater.getGfxFirmwareStatus("") == "unknown");
	CHECK(mock.initCalls == 0);
	CHECK(mock.versionCalls == 0);
	CHECK(mock.statusCalls == 0);
	CHECK(mock.closeCalls == 0);
}

TEST_CASE("GFX firmware status is unknown when IGSC device initialization fails")
{
	resetMock();
	mock.initResult = IGSC_ERROR_DEVICE_NOT_FOUND;
	gscupd updater;

	CHECK(updater.getGfxFirmwareStatus(MEI_DEVICE_PATH) == "unknown");
	CHECK(mock.initCalls == 1);
	CHECK(mock.versionCalls == 0);
	CHECK(mock.statusCalls == 0);
	CHECK(mock.closeCalls == 0);
}

TEST_CASE("GFX firmware status is unknown when the firmware version cannot be read")
{
	resetMock();
	mock.versionResult = IGSC_ERROR_TIMEOUT;
	gscupd updater;

	CHECK(updater.getGfxFirmwareStatus(MEI_DEVICE_PATH) == "unknown");
	CHECK(mock.initCalls == 1);
	CHECK(mock.versionCalls == 1);
	CHECK(mock.statusCalls == 0);
	CHECK(mock.closeCalls == 1);
}

TEST_CASE("GFX firmware status is normal only after a successful firmware version read")
{
	resetMock();
	gscupd updater;

	CHECK(updater.getGfxFirmwareStatus(MEI_DEVICE_PATH) == "normal");
	CHECK(mock.initCalls == 1);
	CHECK(mock.versionCalls == 1);
	CHECK(mock.statusCalls == 1);
	CHECK(mock.closeCalls == 1);
}

TEST_CASE("GFX firmware status is unknown when IGSC cleanup fails")
{
	resetMock();
	mock.closeResult = IGSC_ERROR_INTERNAL;
	gscupd updater;

	CHECK(updater.getGfxFirmwareStatus(MEI_DEVICE_PATH) == "unknown");
	CHECK(mock.initCalls == 1);
	CHECK(mock.versionCalls == 1);
	CHECK(mock.statusCalls == 1);
	CHECK(mock.closeCalls == 1);
}

TEST_CASE("IGSC iterator returned with a create error is released")
{
	resetMock();
	mock.iteratorCreateResult = IGSC_ERROR_INTERNAL;
	mock.returnIteratorOnCreateFailure = true;
	gscupd updater;

	CHECK(updater.getPCIAddrAndMeiDevices().empty());
	CHECK(mock.iteratorDestroyCalls == 1);
}
