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
#include "sysmanupd.h"
#include "utility/scope_exit.h"
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <igsc_lib.h>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

constexpr char MEI_DEVICE_PATH[] = "/dev/mei0";
constexpr size_t IMAGE_SIZE = 64;

struct IgscMockState
{
	int initResult = IGSC_SUCCESS;
	int versionResult = IGSC_SUCCESS;
	int closeResult = IGSC_SUCCESS;
	int iteratorCreateResult = IGSC_SUCCESS;
	uint32_t firmwareStatus = 0;
	igsc_fw_version fwVersion{};
	int initCalls = 0;
	int versionCalls = 0;
	int statusCalls = 0;
	int closeCalls = 0;
	int iteratorDestroyCalls = 0;
	bool returnIteratorOnCreateFailure = false;
	int imageInitResult = IGSC_SUCCESS;
	int imageVersionResult = IGSC_SUCCESS;
	int deviceVersionResult = IGSC_SUCCESS;
	igsc_fwdata_version2 imageVersion{};
	igsc_fwdata_version2 deviceVersion{};
	int imageInitCalls = 0;
	uint32_t imageInitLength = 0;
	const void *imageInitData = nullptr;
	const char *swapFileOnInit = nullptr; // file that another image replaces while the first one is parsed
	int imageReleaseCalls = 0;
	ze_result_t flashResult = ZE_RESULT_SUCCESS;
	int flashCalls = 0;
	uint32_t flashedSize = 0;
	const void *flashedData = nullptr;
	char flashedFirstByte = 0;
};

IgscMockState mock;
alignas(std::max_align_t) std::byte fakeContextStorage;
alignas(std::max_align_t) std::byte fakeIteratorStorage;
alignas(std::max_align_t) std::byte fakeImageStorage;
alignas(std::max_align_t) std::byte fakeFirmwareStorage;
igsc_lib_ctx *const fakeContext = reinterpret_cast<igsc_lib_ctx *>(&fakeContextStorage);

void resetMock() { mock = {}; }

/**
 * @brief Builds a GFX_DATA version as IGSC reports it, with no unfinished update recorded
 *
 * @param [in] format Version format, 1 or 2
 * @param [in] major  Major version, the product the data is for
 * @param [in] oem    OEM manufacturing data version
 * @param [in] vcn    Major VCN
 * @param [in] svn    ARB SVN
 * @return igsc_fwdata_version2 The version
 */
igsc_fwdata_version2 fwDataVersion(uint32_t format, uint16_t major, uint32_t oem, uint16_t vcn, uint32_t svn)
{
	igsc_fwdata_version2 version{};
	version.format_version = format;
	version.oem_manuf_data_version = oem;
	version.major_version = major;
	version.major_vcn = vcn;
	version.data_arb_svn = svn;
	return version;
}

/**
 * @brief The installed GFX_DATA version most cases below start from
 *
 * @return igsc_fwdata_version2 Version format 2, major version 208, OEM version 15, major VCN 0, ARB SVN 1
 */
igsc_fwdata_version2 installedVersion() { return fwDataVersion(2, 208, 15, 0, 1); }

/**
 * @brief Marks a device version as recording an unfinished GFX_DATA update
 *
 * @param [in] version Version of the installed GFX data firmware
 * @param [in] oem     OEM manufacturing data version of the unfinished update
 * @param [in] svn     ARB SVN of the unfinished update
 * @return igsc_fwdata_version2 The device version with the unfinished update recorded
 */
igsc_fwdata_version2 withUnfinishedUpdate(igsc_fwdata_version2 version, uint32_t oem, uint32_t svn)
{
	version.flags = IGSC_FWDATA_FITB_VALID_MASK;
	version.oem_manuf_data_version_fitb = oem;
	version.data_arb_svn_fitb = svn;
	return version;
}

/** @brief A GFX_DATA update of a device with no hardware behind it, with the image already in the buffer */
struct GfxDataUpdate
{
	device dev;
	firmwareInfo info{};

	/** @brief Points the update at the device and preloads IMAGE_SIZE bytes of image */
	GfxDataUpdate()
	{
		info.dev = &dev;
		info.firmwareHandle = reinterpret_cast<zes_firmware_handle_t>(&fakeFirmwareStorage);
		info.buffer.assign(IMAGE_SIZE, 'D');
		info.imagePreloaded = true;
	}
};

/** @brief A GFX_DATA image file of IMAGE_SIZE bytes, removed when the test ends */
struct ImageFile
{
	std::string path;

	/** @brief Creates the file with mkstemp() and writes the image to it */
	ImageFile()
	{
		char name[] = "/tmp/gscupd_test_XXXXXX";
		const int fd = mkstemp(name);
		REQUIRE(fd >= 0);
		close(fd);
		path = name;
		std::ofstream(path, std::ios::binary) << std::string(IMAGE_SIZE, 'F');
	}

	/** @brief Removes the file */
	~ImageFile() { unlink(path.c_str()); }
};

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

} // namespace

extern "C" int __wrap_igsc_device_init_by_device(igsc_device_handle *handle, const char *)
{
	++mock.initCalls;
	if (mock.initResult == IGSC_SUCCESS) {
		handle->ctx = fakeContext;
	}
	return mock.initResult;
}

extern "C" int __wrap_igsc_device_fw_version(igsc_device_handle *, igsc_fw_version *version)
{
	++mock.versionCalls;
	*version = mock.fwVersion;
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

/** @brief Mock: parses a GFX_DATA image, recording the bytes IGSC was given */
extern "C" int __wrap_igsc_image_fwdata_init(igsc_fwdata_image **img, const uint8_t *data, uint32_t length)
{
	++mock.imageInitCalls;
	mock.imageInitData = data;
	mock.imageInitLength = length;
	if (mock.swapFileOnInit != nullptr) {
		std::ofstream(mock.swapFileOnInit, std::ios::binary) << std::string(IMAGE_SIZE, 'S');
	}
	*img = mock.imageInitResult == IGSC_SUCCESS ? reinterpret_cast<igsc_fwdata_image *>(&fakeImageStorage) : nullptr;
	return mock.imageInitResult;
}

/** @brief Mock: reports the GFX_DATA version of the image; like IGSC, writes nothing on failure */
extern "C" int __wrap_igsc_image_fwdata_version2(igsc_fwdata_image *, igsc_fwdata_version2 *version)
{
	if (mock.imageVersionResult == IGSC_SUCCESS) {
		*version = mock.imageVersion;
	}
	return mock.imageVersionResult;
}

/** @brief Mock: releases a parsed GFX_DATA image */
extern "C" int __wrap_igsc_image_fwdata_release(igsc_fwdata_image *)
{
	++mock.imageReleaseCalls;
	return IGSC_SUCCESS;
}

/** @brief Mock: reports the GFX_DATA version of the device; like IGSC, writes nothing on failure */
extern "C" int __wrap_igsc_device_fwdata_version2(igsc_device_handle *, igsc_fwdata_version2 *version)
{
	if (mock.deviceVersionResult == IGSC_SUCCESS) {
		*version = mock.deviceVersion;
	}
	return mock.deviceVersionResult;
}

/** @brief Mock: Level Zero firmware properties, always available */
extern "C" ze_result_t __wrap_zesFirmwareGetProperties(zes_firmware_handle_t, zes_firmware_properties_t *)
{
	return ZE_RESULT_SUCCESS;
}

/** @brief Mock: Level Zero firmware flash, recording what was flashed */
extern "C" ze_result_t __wrap_zesFirmwareFlash(zes_firmware_handle_t, void *image, uint32_t size)
{
	++mock.flashCalls;
	mock.flashedData = image;
	mock.flashedFirstByte = size > 0 ? *static_cast<const char *>(image) : '\0';
	mock.flashedSize = size;
	return mock.flashResult;
}

/** @brief Mock: Level Zero flash progress; the error ends the progress thread of updateFW() at once */
extern "C" ze_result_t __wrap_zesFirmwareGetFlashProgress(zes_firmware_handle_t, uint32_t *)
{
	return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

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

TEST_CASE("GFX firmware version is read in the same IGSC session as the status")
{
	resetMock();
	mock.fwVersion = {{'D', 'G', '0', '2'}, 3, 2027};
	gscupd updater;

	const auto info = updater.readGfxFirmware(MEI_DEVICE_PATH);
	CHECK(info.version == std::optional<std::string>{"DG02_3.2027"});
	CHECK(info.status == "normal");
	CHECK(mock.initCalls == 1);
	CHECK(mock.versionCalls == 1);
	CHECK(mock.closeCalls == 1);
}

TEST_CASE("GFX firmware version is absent when it cannot be read")
{
	resetMock();
	gscupd updater;

	SUBCASE("no MEI device path") { CHECK_FALSE(updater.readGfxFirmware("").version); }
	SUBCASE("initialization fails")
	{
		mock.initResult = IGSC_ERROR_DEVICE_NOT_FOUND;
		CHECK_FALSE(updater.readGfxFirmware(MEI_DEVICE_PATH).version);
	}
	SUBCASE("version read fails")
	{
		mock.versionResult = IGSC_ERROR_TIMEOUT;
		CHECK_FALSE(updater.readGfxFirmware(MEI_DEVICE_PATH).version);
	}
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

TEST_CASE("GFX_DATA image with the installed OEM manufacturing data version is not flashed")
{
	resetMock();
	mock.imageVersion = installedVersion();
	mock.deviceVersion = installedVersion();
	mock.flashResult = ZE_RESULT_ERROR_UNKNOWN; // what Level Zero reports when the firmware refuses the image
	GfxDataUpdate update;
	update.info.filePath = "fwdata.bin";
	sysmanupd updater;
	auto sink = std::make_shared<CaptureSink>();
	Logger::instance().setSink(sink);
	ScopeExit restoreSink{[] { Logger::instance().setSink(nullptr); }};

	CHECK(updater.updateGfxData(&update.info) == ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
	CHECK(sink->str() ==
		  "[Error] Firmware image 'fwdata.bin' was not flashed to device 0: its GFX_DATA OEM manufacturing "
		  "data version 15 is the same as the installed version 15; the device only accepts a different "
		  "one.\n");
	CHECK(mock.flashCalls == 0);
	CHECK(mock.imageReleaseCalls == 1);
	CHECK(mock.closeCalls == 1);
}

TEST_CASE("A refused GFX_DATA image gets no second, generic error line")
{
	CHECK(fwupd::failureAlreadyReported(ZE_RESULT_ERROR_UNSUPPORTED_VERSION));
	CHECK(fwupd::failureAlreadyReported(ZE_RESULT_ERROR_INVALID_SIZE));
	CHECK_FALSE(fwupd::failureAlreadyReported(ZE_RESULT_ERROR_UNKNOWN));
}

TEST_CASE("GFX_DATA image file is read once and checked before it is flashed")
{
	resetMock();
	mock.deviceVersion = installedVersion();
	ImageFile file;
	GfxDataUpdate update;
	update.info.buffer.clear();
	update.info.imagePreloaded = false;
	update.info.filePath = file.path;
	sysmanupd updater;

	SUBCASE("refused")
	{
		mock.imageVersion = installedVersion();
		CHECK(updater.updateGfxData(&update.info) == ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
		CHECK(mock.flashCalls == 0);
	}
	SUBCASE("accepted")
	{
		mock.imageVersion = fwDataVersion(2, 208, 16, 0, 1);
		mock.swapFileOnInit = file.path.c_str();
		CHECK(updater.updateGfxData(&update.info) == ZE_RESULT_SUCCESS);
		CHECK(mock.flashCalls == 1);
		CHECK(mock.flashedSize == IMAGE_SIZE);
		// the bytes that were checked are flashed, not the file that took their place
		CHECK(mock.flashedData == mock.imageInitData);
		CHECK(mock.flashedFirstByte == 'F');
	}
	CHECK(mock.imageInitLength == IMAGE_SIZE);
	CHECK(update.info.imagePreloaded);
}

TEST_CASE("GFX_DATA image refused by the version rules is not flashed")
{
	resetMock();
	GfxDataUpdate update;
	sysmanupd updater;

	SUBCASE("OEM manufacturing data version not higher, version format 1")
	{
		mock.imageVersion = fwDataVersion(1, 208, 14, 0, 0);
		mock.deviceVersion = fwDataVersion(1, 208, 15, 0, 0);
	}
	SUBCASE("--force does not override the rules")
	{
		mock.imageVersion = installedVersion();
		mock.deviceVersion = installedVersion();
		update.info.forceUpdate = true;
	}
	CHECK(updater.updateGfxData(&update.info) == ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
	CHECK(mock.flashCalls == 0);
}

TEST_CASE("GFX_DATA image is flashed when the version rules accept it or cannot be applied")
{
	resetMock();
	mock.imageVersion = fwDataVersion(2, 208, 14, 0, 1);
	mock.deviceVersion = installedVersion();
	GfxDataUpdate update;
	sysmanupd updater;
	int expectedImageReleases = 1;
	int expectedDeviceCloses = 1;

	SUBCASE("lower but different OEM manufacturing data version, version format 2") {}
	SUBCASE("older VCN than the device") { mock.deviceVersion.major_vcn = 1; }
	SUBCASE("installed version cannot be read")
	{
		mock.imageVersion = installedVersion();
		mock.deviceVersionResult = IGSC_ERROR_BUSY;
	}
	SUBCASE("image cannot be parsed")
	{
		mock.imageInitResult = IGSC_ERROR_BAD_IMAGE;
		expectedImageReleases = 0;
		expectedDeviceCloses = 0;
	}
	SUBCASE("device not known")
	{
		update.info.dev = nullptr;
		expectedImageReleases = 0;
		expectedDeviceCloses = 0;
	}
	CHECK(updater.updateGfxData(&update.info) == ZE_RESULT_SUCCESS);
	CHECK(mock.flashCalls == 1);
	CHECK(mock.flashedSize == IMAGE_SIZE);
	CHECK(mock.imageReleaseCalls == expectedImageReleases);
	CHECK(mock.closeCalls == expectedDeviceCloses);
}

TEST_CASE("GFX_DATA version check explains why the device would refuse an image")
{
	/** @brief An image and device version pair that IGSC refuses, with the reason expected for it */
	struct Refusal
	{
		const char *name;
		igsc_fwdata_version2 image;
		igsc_fwdata_version2 installed;
		const char *reason;
	};
	const Refusal refusals[] = {
		{"same OEM manufacturing data version, version format 2", installedVersion(), installedVersion(),
		 "its GFX_DATA OEM manufacturing data version 15 is the same as the installed version 15; the device only "
		 "accepts a different one"},
		{"OEM manufacturing data version not higher, version format 1", fwDataVersion(1, 208, 14, 0, 0),
		 fwDataVersion(1, 208, 15, 0, 0),
		 "its GFX_DATA OEM manufacturing data version 14 is not higher than the installed version 15; the device "
		 "only accepts a higher one"},
		{"other product", fwDataVersion(2, 203, 16, 0, 1), installedVersion(),
		 "it is built for a different product: its GFX_DATA major version 203 does not match the installed major "
		 "version 208"},
		{"newer VCN", fwDataVersion(2, 208, 16, 1, 1), installedVersion(),
		 "its GFX_DATA major VCN 1 is higher than the installed major VCN 0"},
		{"other version format", fwDataVersion(2, 208, 16, 0, 1), fwDataVersion(1, 208, 15, 0, 0),
		 "its GFX_DATA version format 2 does not match the installed version format 1"},
		{"ARB SVN in version format 1", fwDataVersion(1, 208, 16, 0, 1), fwDataVersion(1, 208, 15, 0, 0),
		 "its GFX_DATA version, in format 1, is not compatible with the installed one"},
		{"lower ARB SVN", fwDataVersion(2, 208, 16, 0, 0), installedVersion(),
		 "its GFX_DATA anti-rollback security version (ARB SVN) 0 is lower than the installed ARB SVN 1"},
		{"OEM manufacturing data version of an unfinished update", fwDataVersion(1, 208, 15, 0, 0),
		 withUnfinishedUpdate(fwDataVersion(1, 208, 10, 0, 0), 20, 0),
		 "its GFX_DATA OEM manufacturing data version 15 is not higher than version 20 of an unfinished GFX_DATA "
		 "update recorded on the device (installed version 10); the device only accepts a higher one"},
		{"ARB SVN of an unfinished update", fwDataVersion(2, 208, 16, 0, 1),
		 withUnfinishedUpdate(fwDataVersion(2, 208, 15, 0, 0), 15, 2),
		 "its GFX_DATA anti-rollback security version (ARB SVN) 1 is lower than ARB SVN 2 of an unfinished GFX_DATA "
		 "update recorded on the device (installed ARB SVN 0)"},
	};

	for (const auto &refusal : refusals) {
		CAPTURE(refusal.name);
		resetMock();
		mock.imageVersion = refusal.image;
		mock.deviceVersion = refusal.installed;

		const std::string reason = fwupd::checkFwDataImage(MEI_DEVICE_PATH, std::vector<char>(IMAGE_SIZE));
		CHECK(reason == refusal.reason);
	}
}

TEST_CASE("GFX_DATA version check has no reason for an image it accepts or cannot judge")
{
	resetMock();
	// The image has the installed version unless a case says otherwise, so a check that compared anyway would refuse
	mock.imageVersion = installedVersion();
	mock.deviceVersion = installedVersion();
	std::vector<char> image(IMAGE_SIZE);

	SUBCASE("accepted") { mock.imageVersion = fwDataVersion(2, 208, 16, 0, 1); }
	SUBCASE("older VCN than the device")
	{
		mock.imageVersion = fwDataVersion(2, 208, 16, 0, 1);
		mock.deviceVersion.major_vcn = 1;
	}
	SUBCASE("image version cannot be read") { mock.imageVersionResult = IGSC_ERROR_BAD_IMAGE; }
	SUBCASE("device cannot be opened") { mock.initResult = IGSC_ERROR_NOMEM; }
	SUBCASE("installed version cannot be read") { mock.deviceVersionResult = IGSC_ERROR_BUSY; }
	SUBCASE("image larger than IGSC accepts") { image.resize(static_cast<size_t>(IGSC_MAX_IMAGE_SIZE) + 1); }
	CHECK(fwupd::checkFwDataImage(MEI_DEVICE_PATH, image).empty());
	// what IGSC handed out is given back, and nothing else
	CHECK(mock.imageReleaseCalls == (mock.imageInitResult == IGSC_SUCCESS ? mock.imageInitCalls : 0));
	CHECK(mock.closeCalls == (mock.initResult == IGSC_SUCCESS ? mock.initCalls : 0));
}

TEST_CASE("GFX_DATA version check leaves an empty MEI device path to IGSC")
{
	resetMock();
	mock.imageVersion = installedVersion();
	mock.deviceVersion = installedVersion();

	CHECK_FALSE(fwupd::checkFwDataImage("", std::vector<char>(IMAGE_SIZE)).empty());
	CHECK(mock.initCalls == 1);
}
