/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "fwupd.h"
#include "utility/compat/format.h"
#include "utility/scope_exit.h"
#include <fstream>
#include <string_view>
#include <sys/stat.h>
#include <thread>

/**
 * @brief Reads firmware image content from a file
 *
 * This function reads the entire contents of a firmware image file into
 * a vector buffer. It performs basic file validation to ensure the file
 * exists and is a regular file before reading.
 *
 * @param filePath Path to the firmware image file to read
 * @return std::vector<char> Buffer containing the file contents, empty if error
 */
std::vector<char> fwupd::readImageContent(const char *filePath)
{
	struct stat s;
	if (stat(filePath, &s) != 0 || !(s.st_mode & S_IFREG))
		return std::vector<char>();
	std::ifstream is(std::string(filePath), std::ifstream::binary);
	if (!is) {
		return std::vector<char>();
	}
	// get length of file:
	is.seekg(0, is.end);
	int length = (int)is.tellg();
	is.seekg(0, is.beg);

	std::vector<char> buffer(length);

	is.read(buffer.data(), length);
	is.close();
	return buffer;
}

/**
 * @brief query and prints the firmware update progress percentage
 *
 * This function fetches the firmware progress update from zesFirmwareGetFlashProgress
 * and updates the screen with completed percentage of firmware flash
 *
 * @param flashData the pointer to structure containing firmware handle and mutex for synchronization
 */
static void trackFirmwareFlashProgress(firmwareProgressInfo *flashData)
{
	bool loopContinue = true;
	constexpr uint32_t sleepTimeInMillisec = 250;
	uint32_t progressPercent = 0;
	do {
		progressPercent = 0;
		ze_result_t result = zesFirmwareGetFlashProgress(flashData->firmwareHandle, &progressPercent);
		if (result != ZE_RESULT_SUCCESS) {
			break;
		}

		if (flashData->totalThreads > 0) {
			SETPROGRESSLABEL((int)flashData->deviceIndex, (int)flashData->curThread, (int)flashData->totalThreads,
							 progressPercent, flashData->label);
		} else {
			PRINT("\rFirmware Flash Progress: {} %", progressPercent);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeInMillisec));

		flashData->firmwareProgressMutex.lock();
		loopContinue = !flashData->flashComplete;
		flashData->firmwareProgressMutex.unlock();

	} while (loopContinue);
	progressPercent = 0;
	ze_result_t result = zesFirmwareGetFlashProgress(flashData->firmwareHandle, &progressPercent);
	if (result != ZE_RESULT_SUCCESS) {
		return;
	}

	if (flashData->totalThreads > 0) {
		SETPROGRESSLABEL((int)flashData->deviceIndex, (int)flashData->curThread, (int)flashData->totalThreads,
						 progressPercent, flashData->label);
	} else {
		PRINT("\rFirmware Flash Progress: {} %\n", progressPercent);
	}
}

/**
 * @brief Names the image being flashed, for use in progress output and error messages
 *
 * A composite update flashes several images out of one package, so the file path alone does not
 * identify which one a message refers to. Falls back to the file path when no component label was
 * set, which is the case for every single-image update.
 *
 * @param fwInfo Pointer to firmware information structure containing update details
 * @return std::string Human-readable description of the image being flashed
 */
static std::string flashTarget(const firmwareInfo *fwInfo)
{
	if (!fwInfo->imageLabel.empty()) {
		return fwInfo->imageLabel + " from '" + fwInfo->filePath + "'";
	}
	return "'" + fwInfo->filePath + "'";
}

/**
 * @brief Updates firmware using the provided firmware information
 *
 * This function performs the actual firmware update operation by reading
 * the firmware image file and flashing it to the device using the Level Zero
 * Sysman firmware flash API.
 *
 * When fwInfo->imagePreloaded is set the buffer already holds the image to flash -- a component
 * extracted from a composite package, or a GFX_DATA image that updateGfxData() read to check its
 * version -- and no file is read.
 *
 * @param fwInfo Pointer to firmware information structure containing update details
 * @return ze_result_t ZE_RESULT_SUCCESS if update successful, error code otherwise
 */
ze_result_t fwupd::updateFW(firmwareInfo *fwInfo)
{
	ze_result_t result = ZE_RESULT_SUCCESS;

	// read image file, unless the caller already supplied the image to flash
	if (!fwInfo->imagePreloaded) {
		fwInfo->buffer = readImageContent(fwInfo->filePath.c_str());
	}
	if (fwInfo->buffer.empty()) {
		ERR("Firmware image {} is empty or unreadable.\n", flashTarget(fwInfo).c_str());
		return ZE_RESULT_ERROR_INVALID_SIZE;
	}

	zes_firmware_properties_t firmwareProps = {};
	firmwareProps.stype = ZES_STRUCTURE_TYPE_FIRMWARE_PROPERTIES;
	firmwareProps.pNext = nullptr;
	result = zesFirmwareGetProperties(fwInfo->firmwareHandle, &firmwareProps);
	if (result == ZE_RESULT_ERROR_UNINITIALIZED) {
		ERR("Firmware interface is not initialized\n");
		return result;
	} else if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to query firmware interface: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	// start progress thread
	firmwareProgressInfo progressData = {};
	progressData.flashComplete = false;
	progressData.firmwareHandle = fwInfo->firmwareHandle;
	progressData.deviceIndex = fwInfo->deviceIndex;
	progressData.curThread = fwInfo->curThread;
	progressData.totalThreads = fwInfo->totalThreads;
	progressData.label = fwInfo->imageLabel.empty() ? nullptr : fwInfo->imageLabel.c_str();
	std::thread progressThread(trackFirmwareFlashProgress, &progressData);

	result = zesFirmwareFlash(fwInfo->firmwareHandle, fwInfo->buffer.data(), (uint32_t)fwInfo->buffer.size());
	if (result != ZE_RESULT_SUCCESS) {
		// stop progress thread
		progressData.firmwareProgressMutex.lock();
		progressData.flashComplete = true;
		progressData.firmwareProgressMutex.unlock();
		progressThread.join();

		if (result == ZE_RESULT_ERROR_INVALID_ARGUMENT || result == ZE_RESULT_ERROR_INVALID_SIZE) {
			ERR("Firmware image {} may be invalid/incompatible.\n", flashTarget(fwInfo).c_str());
		} else if (result == ZE_RESULT_ERROR_UNINITIALIZED) {
			ERR("Firmware image {} may be invalid/Firmware interface is not initialized.\n",
				flashTarget(fwInfo).c_str());
		} else {
			ERR("Failed to flash firmware: 0x{:X} ({})\n", result, l0_error_to_string(result));
		}

		return result;
	}

	// stop progress thread
	progressData.firmwareProgressMutex.lock();
	progressData.flashComplete = true;
	progressData.firmwareProgressMutex.unlock();
	progressThread.join();

	DBG("Firmware flash successful.\n");
	return ZE_RESULT_SUCCESS;
};

/**
 * @brief Names the device value that a GFX_DATA image was compared with, for a refusal message
 *
 * IGSC compares with the values of an unfinished GFX_DATA update when the device records one (FITB valid)
 * and with the installed values otherwise. The first case names both, so the message can still be matched
 * with the GFX data firmware version that discovery shows.
 *
 * @param [in] what      Name of the value, e.g. "version" or "ARB SVN"
 * @param [in] installed Value of the installed GFX data firmware
 * @param [in] pending   Value of the unfinished GFX_DATA update
 * @param [in] fitbValid true when the device records an unfinished GFX_DATA update
 * @return std::string The value with its origin, e.g. "the installed version 15"
 */
static std::string comparedDeviceValue(std::string_view what, uint32_t installed, uint32_t pending, bool fitbValid)
{
	if (fitbValid) {
		return xpum::compat::format("{} {} of an unfinished GFX_DATA update recorded on the device (installed {} {})",
									what, pending, what, installed);
	}
	return xpum::compat::format("the installed {} {}", what, installed);
}

/**
 * @brief Explains in plain words why IGSC refuses a GFX_DATA image
 *
 * @param [in] cmp           Result of igsc_fwdata_version_compare2() for the two versions below
 * @param [in] image         GFX_DATA version of the image
 * @param [in] deviceVersion GFX_DATA version of the device
 * @return std::string The reason, or an empty string when cmp is not a refusal: IGSC accepts the image, the
 *         image only has an older VCN than the device, or the result is unknown
 */
static std::string fwDataRejectReason(uint8_t cmp, const igsc_fwdata_version2 &image,
									  const igsc_fwdata_version2 &deviceVersion)
{
	const bool fitbValid = (deviceVersion.flags & IGSC_FWDATA_FITB_VALID_MASK) != 0;

	switch (cmp) {
	case IGSC_FWDATA_VERSION_REJECT_OEM_MANUF_DATA_VERSION:
		if (image.format_version == IGSC_FWDATA_FORMAT_VERSION_1) {
			return xpum::compat::format("its GFX_DATA OEM manufacturing data version {} is not higher than {}; the "
										"device only accepts a higher one",
										image.oem_manuf_data_version,
										comparedDeviceValue("version", deviceVersion.oem_manuf_data_version,
															deviceVersion.oem_manuf_data_version_fitb, fitbValid));
		}
		return xpum::compat::format("its GFX_DATA OEM manufacturing data version {} is the same as {}; the device "
									"only accepts a different one",
									image.oem_manuf_data_version,
									comparedDeviceValue("version", deviceVersion.oem_manuf_data_version,
														deviceVersion.oem_manuf_data_version_fitb, fitbValid));
	case IGSC_FWDATA_VERSION_REJECT_ARB_SVN:
		return xpum::compat::format(
			"its GFX_DATA anti-rollback security version (ARB SVN) {} is lower than {}", image.data_arb_svn,
			comparedDeviceValue("ARB SVN", deviceVersion.data_arb_svn, deviceVersion.data_arb_svn_fitb, fitbValid));
	case IGSC_FWDATA_VERSION_REJECT_DIFFERENT_PROJECT:
		return xpum::compat::format("it is built for a different product: its GFX_DATA major version {} does not "
									"match the installed major version {}",
									image.major_version, deviceVersion.major_version);
	case IGSC_FWDATA_VERSION_REJECT_VCN:
		return xpum::compat::format("its GFX_DATA major VCN {} is higher than the installed major VCN {}",
									image.major_vcn, deviceVersion.major_vcn);
	case IGSC_FWDATA_VERSION_REJECT_WRONG_FORMAT:
		if (image.format_version != deviceVersion.format_version) {
			return xpum::compat::format("its GFX_DATA version format {} does not match the installed version format {}",
										image.format_version, deviceVersion.format_version);
		}
		return xpum::compat::format("its GFX_DATA version, in format {}, is not compatible with the installed one",
									image.format_version);
	default:
		return {};
	}
}

/**
 * @brief Tells why the device would refuse a GFX_DATA image, by the rules IGSC applies before an update
 *
 * Reads the GFX_DATA version of the image and of the device and compares them with
 * igsc_fwdata_version_compare2(). The device firmware refuses, for example, an image whose OEM manufacturing
 * data version is not higher (version format 1) or not different (format 2) than the installed one, and Level
 * Zero reports such a refusal only as ZE_RESULT_ERROR_UNKNOWN, so the reason has to be worked out beforehand.
 *
 * An image that cannot be judged is never refused: when the image cannot be parsed or the device version
 * cannot be read, which includes an empty meiPath, the result is empty and the flash decides as before.
 *
 * @param [in] meiPath IGSC device path of the GPU, see pci::getMeiDevicePath()
 * @param [in] image   GFX_DATA image to check
 * @return std::string Why the device would refuse the image; empty when IGSC accepts it, when the image only
 *         has an older VCN than the device, or when either version cannot be read
 */
std::string fwupd::checkFwDataImage(const std::string &meiPath, const std::vector<char> &image)
{
	// IGSC refuses larger images anyway, and the bound keeps the length below lossless as a uint32_t
	if (image.empty() || image.size() > static_cast<size_t>(IGSC_MAX_IMAGE_SIZE)) {
		DBG("GFX_DATA version check skipped: the image size {} is outside what IGSC accepts.\n", image.size());
		return {};
	}

	igsc_fwdata_image *fwDataImage = nullptr;
	int ret = igsc_image_fwdata_init(&fwDataImage, reinterpret_cast<const uint8_t *>(image.data()),
									 static_cast<uint32_t>(image.size()));
	if (ret != IGSC_SUCCESS) {
		DBG("GFX_DATA version check skipped: IGSC cannot parse the image ({}).\n", ret);
		return {};
	}
	ScopeExit releaseImage{[fwDataImage] { igsc_image_fwdata_release(fwDataImage); }};

	igsc_fwdata_version2 imageVersion{};
	ret = igsc_image_fwdata_version2(fwDataImage, &imageVersion);
	if (ret != IGSC_SUCCESS) {
		DBG("GFX_DATA version check skipped: cannot read the image version ({}).\n", ret);
		return {};
	}

	igsc_device_handle handle{};
	ret = igsc_device_init_by_device(&handle, meiPath.c_str());
	if (ret != IGSC_SUCCESS) {
		DBG("GFX_DATA version check skipped: cannot open IGSC device '{}' ({}).\n", meiPath.c_str(), ret);
		return {};
	}
	ScopeExit closeDevice{[&handle] { igsc_device_close(&handle); }};

	igsc_fwdata_version2 deviceVersion{};
	ret = igsc_device_fwdata_version2(&handle, &deviceVersion);
	if (ret != IGSC_SUCCESS) {
		DBG("GFX_DATA version check skipped: cannot read the installed version from '{}' ({}).\n", meiPath.c_str(),
			ret);
		return {};
	}

	DBG("GFX_DATA image: format {}, major {}, OEM {}, VCN {}, ARB SVN {}; device: format {}, major {}, OEM {}, VCN {}, "
		"ARB SVN {}, flags 0x{:X}, FITB OEM {}, FITB ARB SVN {}\n",
		imageVersion.format_version, imageVersion.major_version, imageVersion.oem_manuf_data_version,
		imageVersion.major_vcn, imageVersion.data_arb_svn, deviceVersion.format_version, deviceVersion.major_version,
		deviceVersion.oem_manuf_data_version, deviceVersion.major_vcn, deviceVersion.data_arb_svn, deviceVersion.flags,
		deviceVersion.oem_manuf_data_version_fitb, deviceVersion.data_arb_svn_fitb);

	return fwDataRejectReason(igsc_fwdata_version_compare2(&imageVersion, &deviceVersion), imageVersion, deviceVersion);
}

/**
 * @brief Updates GFX data firmware through Level Zero Sysman, after checking the image version
 *
 * The device firmware refuses a GFX_DATA image whose version it does not accept, and Level Zero reports that
 * only as ZE_RESULT_ERROR_UNKNOWN. The image is therefore compared with the installed version first, see
 * checkFwDataImage(), and an image the device would refuse is not flashed; --force does not change that.
 *
 * The image file is read here, once, and fwInfo->imagePreloaded is set, so updateFW() flashes the bytes that
 * were checked.
 *
 * @param [in,out] fwInfo Pointer to firmware information structure containing update details; its buffer
 *                 holds the image and imagePreloaded is set on return
 * @return ze_result_t ZE_RESULT_ERROR_UNSUPPORTED_VERSION if the device would refuse the image, which is then
 *         not flashed, otherwise the result of updateFW()
 */
ze_result_t fwupd::updateGfxData(firmwareInfo *fwInfo)
{
	if (!fwInfo->imagePreloaded) {
		fwInfo->buffer = readImageContent(fwInfo->filePath.c_str());
		fwInfo->imagePreloaded = true;
	}

	if (fwInfo->dev != nullptr && !fwInfo->buffer.empty()) {
		const std::string reason = checkFwDataImage(fwInfo->dev->getPCI()->getMeiDevicePath(), fwInfo->buffer);
		if (!reason.empty()) {
			ERR("Firmware image {} was not flashed to device {}: {}.\n", flashTarget(fwInfo).c_str(),
				fwInfo->deviceIndex, reason.c_str());
			return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
		}
	}

	return updateFW(fwInfo);
}
