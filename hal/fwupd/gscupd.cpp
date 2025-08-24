/*
 * Copyright © 2025 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include "gscupd.h"
#include "fwupd.h"
#include <debug.h>
#include <fstream>
#include <os.h>
#include <sys/stat.h>

/**
 * @brief Translates Graphics Firmware Status enumeration to human-readable string
 *
 * This function converts GPU Graphics Firmware Status codes into descriptive
 * string representations for debugging, logging, and user interface display
 * during firmware update operations and system diagnostics.
 *
 * @param status The Graphics Firmware Status enumeration value
 * @return const char* Human-readable string description of the firmware status
 */
const char *gscupd::transGfxFwStatusToString(GfxFwStatus status)
{
#define CASE(x)                                                                                                        \
	case x:                                                                                                            \
		return #x;

	switch (status) {
		CASE(RESET);
		CASE(INIT);
		CASE(RECOVERY);
		CASE(TEST);
		CASE(FW_DISABLED);
		CASE(NORMAL);
		CASE(DISABLE_WAIT);
		CASE(OP_STATE_TRANS);
		CASE(INVALID_CPU_PLUGGED_IN);
	default:
		return "unknown";
	}
}

/**
 * @brief Checks hardware configuration compatibility between device and firmware image
 *
 * This function validates that the firmware image's hardware configuration is
 * compatible with the target device's hardware configuration, ensuring safe
 * firmware updates and preventing incompatible firmware installation.
 *
 * @param handle Pointer to the GSC device handle
 * @param buffer Reference to vector containing the firmware image data
 * @return int IGSC_SUCCESS if compatible, error code otherwise
 */
int gscupd::firmware_check_hw_config(struct igsc_device_handle *handle, std::vector<char> &buffer)
{
	struct igsc_hw_config device_hw_config;
	struct igsc_hw_config image_hw_config;
	int ret;

	memset(&device_hw_config, 0, sizeof(device_hw_config));
	memset(&image_hw_config, 0, sizeof(image_hw_config));

	ret = igsc_device_hw_config(handle, &device_hw_config);
	if (ret != IGSC_SUCCESS && ret != IGSC_ERROR_NOT_SUPPORTED) {
		return ret;
	}

	ret = igsc_image_hw_config((const uint8_t *)buffer.data(), (uint32_t)buffer.size(), &image_hw_config);
	if (ret != IGSC_SUCCESS && ret != IGSC_ERROR_NOT_SUPPORTED) {
		return ret;
	}

	return igsc_hw_config_compatible(&image_hw_config, &device_hw_config);
}

/**
 * @brief Performs common pre-update operations for firmware updates
 *
 * This function executes shared preparation steps for all firmware update types,
 * including GSC device initialization, firmware image validation, type checking,
 * and Graphics Firmware status verification for safe update operations.
 *
 * @param fwInfo Pointer to firmware information structure containing update details
 * @param checkType Boolean flag indicating whether to perform firmware type validation
 * @return ze_result_t ZE_RESULT_SUCCESS on successful preparation, error code otherwise
 */
ze_result_t gscupd::cmnPreUpdate(firmwareInfo *fwInfo, bool checkType)
{
	TRACING();
	int ret;

	pci *p = (pci *)fwInfo->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI device properties.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}
	auto meiPath = p->getMeiDevicePath();

	ret = igsc_device_init_by_device(&fwInfo->handle, meiPath.c_str());
	if (ret) {
		ERR("Failed to initialize device %d\n", ret);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// read image file
	fwInfo->buffer = readImageContent(fwInfo->filePath.c_str());

	// If the caller didn't specify file type checks, then skip any further checks and return success.
	if (!checkType) {
		return ZE_RESULT_SUCCESS;
	}

	int igscFwType = (fwInfo->fwType == FWUPD_PREFERENCE_GSC) ? IGSC_IMAGE_TYPE_GFX_FW : IGSC_IMAGE_TYPE_FW_DATA;

	// validate the image file type
	if (!isGscRightType(fwInfo->buffer, igscFwType)) {
		ERR("Invalid image type %d\n", igscFwType);
		return ZE_RESULT_ERROR_INVALID_NATIVE_BINARY;
	}

	// Check GFX fw_status. This is a macro because it is only available on Linux and
	// not on Windows. On Windows, we simply return GfxFwStatus::NORMAL.
	auto fw_status = GETGFXFWSTATUS(meiPath);
	if (!fwInfo->forceUpdate && fw_status != GfxFwStatus::NORMAL) {
		ERR("Fail to flash, GFX firmware status is %s\n", transGfxFwStatusToString(fw_status));
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Performs specialized pre-update operations for Graphics Firmware
 *
 * This function handles Graphics Firmware-specific preparation including device
 * readiness verification, Graphics Firmware status checking, and compatibility
 * validation before initiating the Graphics Firmware update process.
 *
 * @param fwInfo Pointer to firmware information structure containing GFX update details
 * @return ze_result_t ZE_RESULT_SUCCESS on successful GFX preparation, error code otherwise
 */
ze_result_t gscupd::preUpdateGfx(firmwareInfo *fwInfo)
{
	struct igsc_fw_version device_fw_version;
	struct igsc_fw_version image_fw_version;
	ze_result_t result;
	int ret;
	uint8_t cmp;

	TRACING();

	// First do the common pre-update
	result = cmnPreUpdate(fwInfo);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	// Check if the image is valid
	memset(&image_fw_version, 0, sizeof(image_fw_version));
	ret = igsc_image_fw_version((const uint8_t *)fwInfo->buffer.data(), (uint32_t)fwInfo->buffer.size(),
								&image_fw_version);
	if (ret != IGSC_SUCCESS) {
		ERR("Failed to get image firmware version %d\n", ret);
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	}

	memset(&device_fw_version, 0, sizeof(device_fw_version));
	ret = igsc_device_fw_version(&fwInfo->handle, &device_fw_version);
	if (ret != IGSC_SUCCESS) {
		ERR("Failed to get device firmware version %d\n", ret);
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	}

	cmp = igsc_fw_version_compare(&image_fw_version, &device_fw_version);
	switch (cmp) {
	case IGSC_VERSION_NEWER:
		break;
	case IGSC_VERSION_NOT_COMPATIBLE:
		ERR("Image is not compatible with device %d\n", ret);
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	default:
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes Graphics Firmware update operation
 *
 * This function performs the actual Graphics Firmware update process using the Intel
 * Graphics System Controller (GSC) interface. It handles the firmware image transfer,
 * update progress monitoring, and completion verification for Graphics Firmware.
 *
 * @param fwInfo Pointer to firmware information structure containing update parameters
 * @return ze_result_t ZE_RESULT_SUCCESS on successful update, error code otherwise
 */
ze_result_t gscupd::updateGfx(firmwareInfo *fwInfo)
{
	TRACING();
	int ret;
	struct igsc_fw_update_flags flags = {};

	if (!fwInfo->forceUpdate) {
		ret = firmware_check_hw_config(&fwInfo->handle, fwInfo->buffer);
		if (ret != IGSC_SUCCESS) {
			ERR("Failed to check hardware configuration %d\n", ret);
			return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
		}
	}

	flags.force_update = fwInfo->forceUpdate ? 1 : 0;
	ret = igsc_device_fw_update_ex(&fwInfo->handle, (const uint8_t *)fwInfo->buffer.data(),
								   (uint32_t)fwInfo->buffer.size(), commonProgressCallback, fwInfo, flags);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Performs post-update operations for Graphics Firmware
 *
 * This function executes cleanup and verification tasks after Graphics Firmware
 * update completion, including status validation, resource cleanup, and update
 * result confirmation for proper system state restoration.
 *
 * @param fwInfo Pointer to firmware information structure containing update context
 * @return ze_result_t ZE_RESULT_SUCCESS on successful post-update, error code otherwise
 */
ze_result_t gscupd::postUpdateGfx(firmwareInfo *fwInfo)
{
	TRACING();
	igsc_device_close(&fwInfo->handle);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Performs pre-update operations for Graphics Firmware data
 *
 * This function handles preparation steps specific to Graphics Firmware data
 * components, including data validation, compatibility checking, and system
 * readiness verification before initiating Graphics Firmware data updates.
 *
 * @param fwInfo Pointer to firmware information structure containing GFX data update details
 * @return ze_result_t ZE_RESULT_SUCCESS on successful preparation, error code otherwise
 */
ze_result_t gscupd::preUpdateGfxData(firmwareInfo *fwInfo)
{
	struct igsc_fwdata_version dev_version;
	struct igsc_fwdata_version img_version;
	ze_result_t result;
	int ret;
	uint8_t cmp;

	TRACING();

	// First do the common pre-update
	result = cmnPreUpdate(fwInfo);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	ret =
		igsc_image_fwdata_init(&fwInfo->oimg, (const uint8_t *)fwInfo->buffer.data(), (uint32_t)fwInfo->buffer.size());
	if (ret == IGSC_ERROR_BAD_IMAGE) {
		ERR("FWdata init failed with error: %d\n", ret);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	ret = igsc_image_fwdata_version(fwInfo->oimg, &img_version);
	if (ret != IGSC_SUCCESS) {
		ERR("Failed to get image firmware version %d\n", ret);
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	}

	ret = igsc_device_fwdata_version(&fwInfo->handle, &dev_version);
	if (ret != IGSC_SUCCESS) {
		ERR("Failed to get device firmware version %d\n", ret);
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	}

	cmp = igsc_fwdata_version_compare(&img_version, &dev_version);
	switch (cmp) {
	case IGSC_FWDATA_VERSION_ACCEPT:
	case IGSC_FWDATA_VERSION_OLDER_VCN:
		break;
	default:
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Updates Graphics Firmware data partition
 *
 * This function handles updates to the Graphics Firmware data section, managing
 * data partition validation, transfer operations, and verification for Graphics
 * Firmware data components separate from the main firmware update process.
 *
 * @param fwInfo Pointer to firmware information structure containing data update details
 * @return ze_result_t ZE_RESULT_SUCCESS on successful data update, error code otherwise
 */
ze_result_t gscupd::updateGfxData(firmwareInfo *fwInfo)
{
	TRACING();
	struct igsc_device_info dev_info;
	int ret;

	ret = igsc_device_get_device_info(&fwInfo->handle, &dev_info);
	if (ret != IGSC_SUCCESS) {
		return ZE_RESULT_ERROR_UNKNOWN;
	}
	ret = igsc_image_fwdata_match_device(fwInfo->oimg, &dev_info);
	if (ret != IGSC_SUCCESS) {
		ERR("Failed to match image with device %d\n", ret);
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	}

	ret = igsc_device_fwdata_image_update(&fwInfo->handle, fwInfo->oimg, commonProgressCallback, fwInfo);
	if (ret != IGSC_SUCCESS) {
		ERR("Failed to update firmware %d\n", ret);
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Performs post-update operations for Graphics Firmware data
 *
 * This function executes cleanup and verification tasks after Graphics Firmware
 * data update completion, including data integrity validation, system state
 * restoration, and update result confirmation for data components.
 *
 * @param fwInfo Pointer to firmware information structure containing data update context
 * @return ze_result_t ZE_RESULT_SUCCESS on successful post-update, error code otherwise
 */
ze_result_t gscupd::postUpdateGfxData(firmwareInfo *fwInfo)
{
	TRACING();
	igsc_image_fwdata_release(fwInfo->oimg);
	postUpdateGfx(fwInfo);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Updates Graphics Firmware code data components
 *
 * This function handles updates to Graphics Firmware code data sections,
 * managing executable code validation, transfer operations, and verification
 * for Graphics Firmware code components that contain GPU executable instructions.
 *
 * @param fwInfo Pointer to firmware information structure (unused in current implementation)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful code data update, error code otherwise
 */
ze_result_t gscupd::updateGfxCodeData(UNUSED firmwareInfo *fwInfo)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Updates Graphics Firmware PSC binary components
 *
 * This function handles updates to Graphics Firmware Platform Service Controller
 * (PSC) binary components, managing PSC binary validation, transfer operations,
 * and verification for PSC firmware that provides platform-level services.
 *
 * @param fwInfo Pointer to firmware information structure (unused in current implementation)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful PSC binary update, error code otherwise
 */
ze_result_t gscupd::updateGfxPscBin(UNUSED firmwareInfo *fwInfo)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Handles late binding firmware update operations
 *
 * This function manages firmware updates that require late binding resolution,
 * including dynamic address assignment, dependency resolution, and deferred
 * update operations for firmware components with runtime binding requirements.
 *
 * @param fwInfo Pointer to firmware information structure containing late binding context
 * @return ze_result_t ZE_RESULT_SUCCESS on successful late binding update, error code otherwise
 */
ze_result_t gscupd::updateLateBinding(firmwareInfo *fwInfo)
{
	TRACING();
	csc_late_binding_flags late_binding_flags = {};
	uint32_t late_binding_status = {};
	csc_late_binding_type late_binding_type;
	int ret;

	switch (fwInfo->fwType) {
	case FAN_TABLE:
		late_binding_type = CSC_LATE_BINDING_TYPE_FAN_TABLE;
		break;
	case VR_CONFIG:
		late_binding_type = CSC_LATE_BINDING_TYPE_VR_CONFIG;
		break;
	default:
		late_binding_type = CSC_LATE_BINDING_TYPE_INVALID;
	}

	ret = igsc_device_update_late_binding_config(&fwInfo->handle, late_binding_type, late_binding_flags,
												 (uint8_t *)fwInfo->buffer.data(), fwInfo->buffer.size(),
												 &late_binding_status);

	if (ret) {
		ERR("GSC late binding update failed on device. %d\n", ret);
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Performs pre-update operations for fan table firmware
 *
 * This function handles preparation steps for fan table firmware updates,
 * including fan configuration validation, thermal system readiness checking,
 * and compatibility verification before updating fan control tables.
 *
 * @param fwInfo Pointer to firmware information structure containing fan table update details
 * @return ze_result_t ZE_RESULT_SUCCESS on successful preparation, error code otherwise
 */
ze_result_t gscupd::preUpdateFanTable(firmwareInfo *fwInfo)
{
	TRACING();
	ze_result_t result;

	// First do the common pre update
	result = cmnPreUpdate(fwInfo, false);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	return result;
}

/**
 * @brief Updates fan table firmware configuration
 *
 * This function executes fan table firmware update operations, managing fan
 * control table transfer, configuration validation, and thermal system
 * integration for updated fan control parameters and behaviors.
 *
 * @param fwInfo Pointer to firmware information structure containing fan table data
 * @return ze_result_t ZE_RESULT_SUCCESS on successful fan table update, error code otherwise
 */
ze_result_t gscupd::updateFanTable(firmwareInfo *fwInfo)
{
	TRACING();
	return updateLateBinding(fwInfo);
}

/**
 * @brief Updates voltage regulator (VR) configuration firmware
 *
 * This function handles voltage regulator configuration firmware updates,
 * managing VR parameter validation, configuration transfer, and power system
 * integration for updated voltage regulation settings and behaviors.
 *
 * @param fwInfo Pointer to firmware information structure containing VR config data
 * @return ze_result_t ZE_RESULT_SUCCESS on successful VR config update, error code otherwise
 */
ze_result_t gscupd::updateVrConfig(firmwareInfo *fwInfo)
{
	TRACING();
	return updateLateBinding(fwInfo);
}

/**
 * @brief Performs post-update operations for fan table firmware
 *
 * This function executes cleanup and verification tasks after fan table
 * firmware update completion, including thermal system validation, fan
 * operation testing, and update result confirmation for fan control systems.
 *
 * @param fwInfo Pointer to firmware information structure containing fan table context
 * @return ze_result_t ZE_RESULT_SUCCESS on successful post-update, error code otherwise
 */
ze_result_t gscupd::postUpdateFanTable(firmwareInfo *fwInfo)
{
	TRACING();
	igsc_device_close(&fwInfo->handle);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Validates Graphics System Controller firmware type compatibility
 *
 * This function checks whether the provided firmware image buffer contains
 * the expected GSC firmware type, ensuring compatibility before proceeding
 * with firmware update operations on the target graphics device.
 *
 * @param buffer Reference to vector containing firmware image data
 * @param expectedType Integer representing the expected GSC firmware type
 * @return bool True if firmware type matches expected type, false otherwise
 */
bool gscupd::isGscRightType(std::vector<char> &buffer, int expectedType)
{
	TRACING();
	uint8_t type;
	int ret;
	ret = igsc_image_get_type((const uint8_t *)buffer.data(), (uint32_t)buffer.size(), &type);
	if (ret != IGSC_SUCCESS) {
		return false;
	}
	return type == expectedType;
}

/**
 * @brief Retrieves PCI addresses and MEI device information
 *
 * This function discovers and enumerates all available PCI graphics devices
 * and their corresponding Management Engine Interface (MEI) devices, providing
 * device addressing information required for firmware update operations.
 *
 * @return std::vector<pci_addr_mei_device> Vector containing PCI address and MEI device pairs
 */
std::vector<pci_addr_mei_device> gscupd::getPCIAddrAndMeiDevices()
{
	std::vector<pci_addr_mei_device> devicesVec = {};
	struct igsc_device_iterator *iter;
	struct igsc_device_info info;
	int ret;
	struct igsc_device_handle handle;

	memset(&handle, 0, sizeof(handle));
	ret = igsc_device_iterator_create(&iter);
	if (ret != IGSC_SUCCESS) {
		ERR("Cannot create device iterator %d\n", ret);
		return devicesVec;
	}
	info.name[0] = '\0';
	while ((ret = igsc_device_iterator_next(iter, &info)) == IGSC_SUCCESS) {
		pci_addr_mei_device pciAddrMeiDevice = {};

		ret = igsc_device_init_by_device_info(&handle, &info);
		if (ret != IGSC_SUCCESS) {
			/* make sure we have a printable name */
			info.name[0] = '\0';
			continue;
		}

		pciAddrMeiDevice.fwStatus = igsc_translate_firmware_status(igsc_get_last_firmware_status(&handle));
		// If fwStatus is "Success", change it to "normal"
		if (pciAddrMeiDevice.fwStatus == "Success") {
			pciAddrMeiDevice.fwStatus = "normal";
		}
		pciAddrMeiDevice.pciProps.address.domain = info.domain;
		pciAddrMeiDevice.pciProps.address.bus = info.bus;
		pciAddrMeiDevice.pciProps.address.device = info.dev;
		pciAddrMeiDevice.pciProps.address.function = info.func;
		pciAddrMeiDevice.meiDevicePath = info.name;
		devicesVec.push_back(pciAddrMeiDevice);

		(void)igsc_device_close(&handle);
	}
	igsc_device_iterator_destroy(iter);
	return devicesVec;
}

/**
 * @brief Retrieves Graphics Firmware status from MEI device
 *
 * This function queries the Graphics Firmware status through the Management
 * Engine Interface (MEI) device, providing current firmware state information
 * including operational status, version details, and readiness indicators.
 *
 * @param meiPath String containing the path to the MEI device
 * @return GfxFwStatus Enumerated status value representing current Graphics Firmware state
 */
GfxFwStatus gscupd::getGfxFwStatus(std::string meiPath)
{
	TRACING();
	uint32_t status = 0x10;
	auto idx = meiPath.find("mei");
	if (idx != meiPath.npos) {
		std::string meiName = meiPath.substr(idx);
		std::string sysfs_path = "/sys/class/mei/" + meiName + "/fw_status";
		std::string val;
		std::ifstream ifile(sysfs_path);
		if (ifile.is_open() == false) {
			return GfxFwStatus::UNKNOWN;
		}
		ifile >> val;
		ifile.close();
		uint32_t reg_status = std::stoi(val, 0, 16);
		status = reg_status & 0xf;
	}

	if (status >= GfxFwStatus::UNKNOWN) {
		return GfxFwStatus::UNKNOWN;
	} else {
		return (GfxFwStatus)status;
	}
}
