/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef INFO
#undef INFO
#endif

#include "device.h"
#include "firmware.h"
#include "utility/scope_exit.h"
#include <array>
#include <cstddef>
#include <igsc_lib.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr uint32_t DEVICE_INDEX = 3;
constexpr uint8_t DEVICE_BUS = 3; // the mocked GPU is at 0000:03:00.0
constexpr char MEI_DEVICE_PATH[] = "/dev/mei1";
constexpr char MISSING_IMAGE[] = "/nonexistent/xpum-firmware-test/image.bin";

constexpr char IGSC_HINT[] =
	" The GPU runtime needs the shared igsc library to reach the firmware; check that it is installed.";

/** @brief What the mocked Level Zero Sysman and IGSC report about the device */
struct MockState
{
	ze_result_t enumResult = ZE_RESULT_SUCCESS; // result of zesDeviceEnumFirmwares
	std::vector<std::string> names;				// firmware names Sysman lists, in enumeration order
	std::string meiPath = MEI_DEVICE_PATH;		// GSC MEI node that IGSC finds for the GPU, empty for none
	bool meiListed = false;						// the IGSC iterator has returned meiPath
	int gscVersionResult = IGSC_SUCCESS;		// result of IGSC's GFX firmware version query over meiPath
	int gscQueries = 0;							// IGSC sessions opened to query the GSC
	int flashCalls = 0;							// calls of zesFirmwareFlash
};

MockState mock;

// The fake handles point into this storage, and a firmware handle's offset is its index in mock.names.
std::array<char, 8> handleStorage{};
char deviceStorage = 0;
char iteratorStorage = 0;
char contextStorage = 0;

/** @brief Log sink that keeps every message, so a test can check what the user is shown */
class CaptureSink final : public Sink
{
	std::string captured;

public:
	/**
	 * @brief Keeps one message, with its prefix
	 *
	 * @param [in] record The message
	 */
	void emit(const LogRecord &record) override
	{
		captured += std::string(record.prefix);
		captured += std::string(record.msg);
	}

	/** @brief Nothing to flush */
	void sync() noexcept override {}

	/**
	 * @brief All messages so far
	 *
	 * @return const std::string& The messages, in order
	 */
	[[nodiscard]] const std::string &str() const noexcept { return captured; }
};

/**
 * @brief Updates one firmware type on a device whose Sysman and IGSC report what the mock holds
 *
 * The image file does not exist, so an update that gets past the firmware handle check stops when
 * it reads the image, before anything is flashed.
 *
 * @param [in] type Firmware type, as passed to updatefw -t
 * @param [out] errors Every message the update logged
 * @return ze_result_t What firmware::updateFW() returned
 */
ze_result_t updateType(const char *type, std::string &errors)
{
	const auto deviceHandle = reinterpret_cast<zes_device_handle_t>(&deviceStorage);
	device dev;
	// As in device initialization, pci::init() looks the GPU's GSC MEI node up through IGSC.
	REQUIRE(dev.getPCI()->init(deviceHandle) == ZE_RESULT_SUCCESS);
	firmware fw;
	// init() also looks for AMC cards, which may log; only the update's own messages are captured.
	REQUIRE(fw.init(deviceHandle) == ZE_RESULT_SUCCESS);

	auto sink = std::make_shared<CaptureSink>();
	Logger::instance().setSink(sink);
	ScopeExit restoreSink{[] { Logger::instance().setSink(nullptr); }};

	firmwareInfo info{};
	info.firmwareType = type;
	info.filePath = MISSING_IMAGE;
	info.deviceIndex = DEVICE_INDEX;
	info.dev = &dev;
	ze_result_t result = fw.updateFW(&info);
	errors = sink->str();
	return result;
}

} // namespace

/**
 * @brief Mock: Level Zero firmware enumeration, listing the names in the mock
 *
 * @param [in,out] pCount Number of handles phFirmware has room for; set to the number listed
 * @param [out] phFirmware Handles of the listed firmware, or nullptr to only count them
 * @return ze_result_t mock.enumResult, or an error when the mock lists more firmware than it has handles for
 */
extern "C" ze_result_t __wrap_zesDeviceEnumFirmwares(zes_device_handle_t, uint32_t *pCount,
													 zes_firmware_handle_t *phFirmware)
{
	if (mock.enumResult != ZE_RESULT_SUCCESS) {
		return mock.enumResult;
	}
	if (mock.names.size() > handleStorage.size()) {
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}
	if (phFirmware != nullptr) {
		for (size_t i = 0; i < *pCount && i < mock.names.size(); i++) {
			phFirmware[i] = reinterpret_cast<zes_firmware_handle_t>(&handleStorage[i]);
		}
	}
	*pCount = static_cast<uint32_t>(mock.names.size());
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Mock: Level Zero firmware properties, named after the handle's entry in the mock
 *
 * @param [in] hFirmware Handle returned by the enumeration mock
 * @param [out] pProperties Properties with the firmware's name and a fixed version
 * @return ze_result_t ZE_RESULT_SUCCESS, or an error for a handle the enumeration mock did not return
 */
extern "C" ze_result_t __wrap_zesFirmwareGetProperties(zes_firmware_handle_t hFirmware,
													   zes_firmware_properties_t *pProperties)
{
	const auto index = static_cast<size_t>(reinterpret_cast<char *>(hFirmware) - handleStorage.data());
	if (index >= mock.names.size()) {
		return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
	}
	*pProperties = {};
	mock.names[index].copy(pProperties->name, sizeof(pProperties->name) - 1);
	std::string_view("1.0").copy(pProperties->version, sizeof(pProperties->version) - 1);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Mock: Level Zero firmware flash, which no test may reach
 *
 * @return ze_result_t Always an error, after counting the call
 */
extern "C" ze_result_t __wrap_zesFirmwareFlash(zes_firmware_handle_t, void *, uint32_t)
{
	++mock.flashCalls;
	return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

/**
 * @brief Mock: Level Zero PCI properties of the GPU
 *
 * @param [out] pProperties Properties whose address is the mocked GPU's
 * @return ze_result_t Always ZE_RESULT_SUCCESS
 */
extern "C" ze_result_t __wrap_zesDevicePciGetProperties(zes_device_handle_t, zes_pci_properties_t *pProperties)
{
	pProperties->address = {};
	pProperties->address.bus = DEVICE_BUS;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Mock: IGSC device iterator creation
 *
 * @param [out] iter The fake iterator
 * @return int Always IGSC_SUCCESS
 */
extern "C" int __wrap_igsc_device_iterator_create(igsc_device_iterator **iter)
{
	mock.meiListed = false;
	*iter = reinterpret_cast<igsc_device_iterator *>(&iteratorStorage);
	return IGSC_SUCCESS;
}

/**
 * @brief Mock: IGSC device iteration, returning the GPU's GSC MEI node once when the mock has one
 *
 * @param [out] info The GSC MEI node and the GPU's PCI address
 * @return int IGSC_SUCCESS for the node, IGSC_ERROR_DEVICE_NOT_FOUND when there is no further one
 */
extern "C" int __wrap_igsc_device_iterator_next(igsc_device_iterator *, igsc_device_info *info)
{
	if (mock.meiPath.empty() || mock.meiListed) {
		return IGSC_ERROR_DEVICE_NOT_FOUND;
	}
	mock.meiListed = true;
	*info = {};
	mock.meiPath.copy(info->name, sizeof(info->name) - 1);
	info->bus = DEVICE_BUS;
	return IGSC_SUCCESS;
}

/** @brief Mock: IGSC device iterator release, nothing to release */
extern "C" void __wrap_igsc_device_iterator_destroy(igsc_device_iterator *) {}

/**
 * @brief Mock: IGSC device context creation, which like IGSC does not talk to the GSC yet
 *
 * @param [out] handle Handle given a fake context
 * @return int Always IGSC_SUCCESS
 */
extern "C" int __wrap_igsc_device_init_by_device(igsc_device_handle *handle, const char *)
{
	++mock.gscQueries;
	handle->ctx = reinterpret_cast<igsc_lib_ctx *>(&contextStorage);
	return IGSC_SUCCESS;
}

/**
 * @brief Mock: IGSC GFX firmware version query, the first request that reaches the GSC
 *
 * @return int mock.gscVersionResult
 */
extern "C" int __wrap_igsc_device_fw_version(igsc_device_handle *, igsc_fw_version *) { return mock.gscVersionResult; }

/**
 * @brief Mock: status of the last GSC request
 *
 * @return uint32_t Always 0, the firmware's success status
 */
extern "C" uint32_t __wrap_igsc_get_last_firmware_status(igsc_device_handle *) { return 0; }

/**
 * @brief Mock: IGSC device context release
 *
 * @param [in,out] handle Handle whose fake context is dropped
 * @return int Always IGSC_SUCCESS
 */
extern "C" int __wrap_igsc_device_close(igsc_device_handle *handle)
{
	handle->ctx = nullptr;
	return IGSC_SUCCESS;
}

TEST_CASE("An update on a device whose Sysman lists no firmware points at the igsc library")
{
	// What Sysman reports for a card without a flash override when it cannot load igsc
	mock = {};
	std::string errors;

	const ze_result_t result = updateType("GFX", errors);
	CHECK(result == ZE_RESULT_ERROR_UNKNOWN);
	CHECK(errors == std::string("[Error] Level Zero Sysman exposes no GFX firmware on device 3.") + IGSC_HINT + "\n");
	CHECK(mock.gscQueries == 1);
}

TEST_CASE("An update on a device whose Sysman lists only the flash override points at the igsc library")
{
	// What Sysman reports for a card with a flash override when it cannot load igsc
	mock = {};
	mock.names = {"Flash_Override"};
	std::string errors;

	const ze_result_t result = updateType("GFX_DATA", errors);
	CHECK(result == ZE_RESULT_ERROR_UNKNOWN);
	CHECK(errors ==
		  std::string("[Error] Level Zero Sysman exposes no GFX_DATA firmware on device 3.") + IGSC_HINT + "\n");
	CHECK(mock.gscQueries == 1);
}

TEST_CASE("A firmware type the device lacks gets no igsc hint when Sysman reaches the GSC")
{
	mock = {};
	mock.names = {"GFX", "GFX_DATA", "FanTable"};
	std::string errors;

	const ze_result_t result = updateType("OP_CODE", errors);
	CHECK(result == ZE_RESULT_ERROR_UNKNOWN);
	CHECK(errors == "[Error] Level Zero Sysman exposes no OP_CODE firmware on device 3.\n");
	CHECK(mock.gscQueries == 0);
}

TEST_CASE("A missing GFX firmware gets no igsc hint when Sysman lists other firmware it reaches through igsc")
{
	for (const char *name : {"GFX_DATA", "OptionROM", "PSC", "FanTable", "VRConfig"}) {
		mock = {};
		mock.names = {name};
		std::string errors;

		const ze_result_t result = updateType("GFX", errors);
		CHECK_MESSAGE(result == ZE_RESULT_ERROR_UNKNOWN, name);
		CHECK_MESSAGE(errors == "[Error] Level Zero Sysman exposes no GFX firmware on device 3.\n", name);
		CHECK_MESSAGE(mock.gscQueries == 0, name);
	}
}

TEST_CASE("A device without a GSC, such as an integrated GPU, gets no igsc hint")
{
	// Sysman lists no firmware for it even with a working igsc, and igsc finds no GSC MEI node for it
	mock = {};
	mock.meiPath.clear();
	std::string errors;

	const ze_result_t result = updateType("GFX", errors);
	CHECK(result == ZE_RESULT_ERROR_UNKNOWN);
	CHECK(errors == "[Error] Level Zero Sysman exposes no GFX firmware on device 3.\n");
	CHECK(mock.gscQueries == 0);
}

TEST_CASE("A GSC that does not answer gets no igsc hint")
{
	// For example in a container without the MEI device node: sysfs still lists the node, but Sysman
	// lists no firmware with a working igsc either, as no request reaches the GSC
	mock = {};
	mock.gscVersionResult = IGSC_ERROR_DEVICE_NOT_FOUND;
	std::string errors;

	const ze_result_t result = updateType("GFX", errors);
	CHECK(result == ZE_RESULT_ERROR_UNKNOWN);
	CHECK(errors == "[Error] Level Zero Sysman exposes no GFX firmware on device 3.\n");
	CHECK(mock.gscQueries == 1);
}

TEST_CASE("A missing flash override gets no igsc hint, as Sysman does not reach it through igsc")
{
	mock = {};
	std::string errors;

	const ze_result_t result = updateType("FDO", errors);
	CHECK(result == ZE_RESULT_ERROR_UNKNOWN);
	CHECK(errors == "[Error] Level Zero Sysman exposes no FDO firmware on device 3.\n");
	CHECK(mock.gscQueries == 0);
}

TEST_CASE("A failed firmware enumeration is reported with its error and gets no igsc hint")
{
	mock = {};
	mock.enumResult = ZE_RESULT_ERROR_UNINITIALIZED;
	std::string errors;

	const ze_result_t result = updateType("GFX", errors);
	CHECK(result == ZE_RESULT_ERROR_UNKNOWN);
	CHECK(errors == "[Error] Failed to enumerate firmwares. 0x78000001 (ZE_RESULT_ERROR_UNINITIALIZED)\n"
					"[Error] Level Zero Sysman exposes no GFX firmware on device 3.\n");
	CHECK(mock.gscQueries == 0);
}

TEST_CASE("An update of firmware the device exposes still goes on to read the image")
{
	mock = {};
	mock.names = {"GFX"};
	std::string errors;

	const ze_result_t result = updateType("GFX", errors);
	CHECK(result == ZE_RESULT_ERROR_INVALID_SIZE);
	CHECK(errors == std::string("[Error] Firmware image '") + MISSING_IMAGE + "' is empty or unreadable.\n");
	CHECK(mock.flashCalls == 0);
	CHECK(mock.gscQueries == 0);
}
