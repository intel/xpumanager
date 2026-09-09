/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "firmware.h"
#include <amcupd.h>
#include <gscupd.h>
#include <pldmextraction.h>
#include <sysmanupd.h>
#include <ze_api.h>

/**
 * @brief One PLDM ComponentIdentifier and the firmware type it is flashed as
 *
 * Together these rows are the only oracle for what a component in a composite package is: the
 * package carries no other hint about which endpoint an image belongs to. A component whose
 * identifier is absent from this table is never flashed.
 */
struct compositeComponentMap
{
	uint16_t id;			// ComponentIdentifier from the Component Image Information Area
	const char *name;		// identifier name, for progress lines and debug output
	int fw;					// firmware type to flash it as, MAX_FW_TYPE when there is no endpoint
	bool optIn;				// only flashed when the user explicitly asked for it (--fdo)
	const char *skipReason; // why it is not flashed by default, null when it is
};

// Every identifier a composite package can carry. Recovery images and the SVN table have no
// endpoint this stack can flash, so they are listed with MAX_FW_TYPE and skipped by name rather
// than falling into the "unknown identifier" case.
static const compositeComponentMap compositeComponents[] = {
	{PLDM_COMPONENT_ID_SVN_TABLE, "SVN_TABLE", MAX_FW_TYPE, false, "no firmware endpoint accepts an SVN table"},
	{PLDM_COMPONENT_ID_IFWI, "IFWI", FDO, true, "it is a device recovery image, pass --fdo to flash it"},
	{PLDM_COMPONENT_ID_AMC_RECOVERY, "AMC_RECOVERY", MAX_FW_TYPE, false, "recovery images are not flashed"},
	{PLDM_COMPONENT_ID_AMC, "AMC", AMC, false, nullptr},
	{PLDM_COMPONENT_ID_VR_CONFIG_RECOVERY, "VR_CONFIG_RECOVERY", MAX_FW_TYPE, false, "recovery images are not flashed"},
	{PLDM_COMPONENT_ID_VR_CONFIG, "VR_CONFIG", VR_CONFIG, false, nullptr},
	{PLDM_COMPONENT_ID_GFX_CODE_RECOVERY, "GFX_CODE_RECOVERY", MAX_FW_TYPE, false, "recovery images are not flashed"},
	{PLDM_COMPONENT_ID_GFX_CODE, "GFX_CODE", GFX, false, nullptr},
	{PLDM_COMPONENT_ID_GFX_DATA_RECOVERY, "GFX_DATA_RECOVERY", MAX_FW_TYPE, false, "recovery images are not flashed"},
	{PLDM_COMPONENT_ID_GFX_DATA, "GFX_DATA", GFX_DATA, false, nullptr},
};

firmware::firmware() : firmwareCount(0), firmwareList(nullptr), propertiesList(nullptr), fwupdArray(nullptr)
{
	updateFWCmds = new updateFWCmdStruct[MAX_FW_TYPE]{
		{GFX, TOSTR(GFX), FWUPD_PREFERENCE_SYSMAN, &fwupd::preUpdateGfx, &fwupd::updateGfx, &fwupd::postUpdateGfx,
		 nullptr, "", ""},
		{GFX_DATA, TOSTR(GFX_DATA), FWUPD_PREFERENCE_SYSMAN, &fwupd::preUpdateGfxData, &fwupd::updateGfxData,
		 &fwupd::postUpdateGfxData, nullptr, "", ""},
		{GFX_CODE_DATA, TOSTR(GFX_CODE_DATA), FWUPD_PREFERENCE_SYSMAN, &fwupd::preUpdateGfxData, &fwupd::updateGfxData,
		 &fwupd::postUpdateGfxData, nullptr, "", ""},
		{OP_CODE, TOSTR(OP_CODE), FWUPD_PREFERENCE_SYSMAN, &fwupd::preUpdateOpCode, &fwupd::updateOpCode,
		 &fwupd::postUpdateOpCode, nullptr, "", ""},
		{OP_DATA, TOSTR(OP_DATA), FWUPD_PREFERENCE_SYSMAN, &fwupd::preUpdateOpData, &fwupd::updateOpData,
		 &fwupd::postUpdateOpData, nullptr, "", ""},
		{GFX_PSCBIN, TOSTR(GFX_PSCBIN), FWUPD_PREFERENCE_SYSMAN, &fwupd::preUpdateGfxPscBin, &fwupd::updateGfxPscBin,
		 &fwupd::postUpdateGfxPscBin, nullptr, "", ""},
		{FAN_TABLE, TOSTR(FAN_TABLE), FWUPD_PREFERENCE_SYSMAN, &fwupd::preUpdateFanTable, &fwupd::updateFanTable,
		 &fwupd::postUpdateFanTable, nullptr, "", ""},
		{VR_CONFIG, TOSTR(VR_CONFIG), FWUPD_PREFERENCE_SYSMAN, &fwupd::preUpdateVrConfig, &fwupd::updateVrConfig,
		 &fwupd::postUpdateVrConfig, nullptr, "", ""},
		{AMC, TOSTR(AMC), FWUPD_PREFERENCE_AMC, &fwupd::preUpdateAMC, &fwupd::updateAMC, &fwupd::postUpdateAMC, nullptr,
		 "", ""},
		{FDO, TOSTR(FDO), FWUPD_PREFERENCE_SYSMAN, &fwupd::preUpdateFdo, &fwupd::updateFdo, &fwupd::postUpdateFdo,
		 nullptr, "", ""},
		// COMPOSITE is not a firmware endpoint and so has no handlers of its own: updateFW() routes it
		// to updateComposite(), which unwraps the package and re-enters the rows above per component.
		{COMPOSITE, TOSTR(COMPOSITE), FWUPD_PREFERENCE_SYSMAN, nullptr, nullptr, nullptr, nullptr, "", ""},
	};
}

/**
 * @brief Destructor for the firmware class
 *
 * This destructor performs cleanup operations for the firmware management
 * object, releasing allocated memory for firmware update command structures
 * and firmware handle lists to ensure proper resource deallocation.
 */
firmware::~firmware()
{
	if (updateFWCmds) {
		delete[] updateFWCmds;
		updateFWCmds = nullptr;
	}

	if (firmwareList) {
		delete[] firmwareList;
		firmwareList = nullptr;
	}

	if (fwupdArray) {
		for (uint32_t i = 0; i < FWUPD_PREFERENCE_MAX; i++) {
			if (fwupdArray[i]) {
				delete fwupdArray[i];
				fwupdArray[i] = nullptr;
			}
		}
		delete[] fwupdArray;
		fwupdArray = nullptr;
	}
}

/**
 * @brief Enumerates all available firmware modules for a device
 *
 * This function discovers and catalogs all firmware components available
 * on the specified device, including graphics firmware, data firmware,
 * and other system firmware modules.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
ze_result_t firmware::enumFirmwares(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumFirmwares(device, &firmwareCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || firmwareCount == 0) {
		ERR("No firmware found or failed to enumerate firmwares. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Device has {} firmwares.\n", firmwareCount);

	firmwareList = new zes_firmware_handle_t[firmwareCount];
	result = zesDeviceEnumFirmwares(device, &firmwareCount, firmwareList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to retrieve firmware properties.\n");
		return result;
	}

	return result;
}

/**
 * @brief Retrieves properties for a specific firmware handle
 *
 * This function obtains detailed properties and information about a
 * specific firmware component, including version, type, and capabilities.
 *
 * @param firmwareHandle Handle to the specific firmware component
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
ze_result_t firmware::getProperties(zes_firmware_handle_t firmwareHandle)
{
	TRACING();
	int index = -1;
	zes_firmware_properties_t properties;
	ze_result_t result = zesFirmwareGetProperties(firmwareHandle, &properties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get firmware properties. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Firmware Name: {}, Version: {}\n", properties.name, properties.version);
	DBG("Can control: {}\n", properties.canControl);
	if (STRCASECMP(properties.name, "GFX") == 0) {
		index = GFX;
	} else if (STRCASECMP(properties.name, "AMC") == 0) {
		index = AMC;
	} else if (STRCASECMP(properties.name, "GFX_DATA") == 0) {
		index = GFX_DATA;
	} else if (STRCASECMP(properties.name, "FLASH_OVERRIDE") == 0) {
		index = FDO;
	} else if (STRCASECMP(properties.name, "OPTIONROM") == 0) {
		index = OP_CODE;
	} else if (STRCASECMP(properties.name, "PSC") == 0) {
		index = GFX_PSCBIN;
	} else if (STRCASECMP(properties.name, "FANTABLE") == 0) {
		index = FAN_TABLE;
	} else if (STRCASECMP(properties.name, "VRCONFIG") == 0) {
		index = VR_CONFIG;
	} else {
		DBG("Firmware Type: Unknown\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	// Store the firmware handle and properties
	updateFWCmds[index].firmwareHandle = firmwareHandle;
	updateFWCmds[index].name = properties.name;
	updateFWCmds[index].version = properties.version;

	if (index == OP_CODE) {
		updateFWCmds[OP_DATA].firmwareHandle = firmwareHandle;
		updateFWCmds[OP_DATA].name = std::string("OP_DATA");
		updateFWCmds[OP_DATA].version = properties.version;
	}

	return result;
}

/**
 * @brief Gets the version string for a specific firmware type
 *
 * This function retrieves the version information for the specified
 * firmware type and copies it to the provided buffer.
 *
 * @param type The firmware type to query
 * @param bdfStr The BDF string of the device (e.g., "0000:00:02.0")
 * @param version Pointer to buffer to store the version string
 * @param size Size of the version buffer (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if version retrieved, error code otherwise
 */
ze_result_t firmware::getFWversion(fwType type, const char *bdfStr, char *version, UNUSED uint32_t size)
{
	TRACING();

	ze_result_t result = ZE_RESULT_SUCCESS;
	if (type < GFX || type >= MAX_FW_TYPE) {
		ERR("Invalid firmware type: {}\n", type);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// AMC fw version is retrieved differently because we have to ask the AMC card itself
	if (type == fwType::AMC) {
		int amc = -1;
		std::string amcSerialNum, amcVersion;

		if (fwupdArray && fwupdArray[FWUPD_PREFERENCE_AMC]) {
			amcupd *a = static_cast<amcupd *>(fwupdArray[FWUPD_PREFERENCE_AMC]);
			// Along with the index, let's also find out the serial number and version of the AMC card
			amc = a->amcGetCardInfo(bdfStr, amcSerialNum, amcVersion);

			// If we received a valid AMC index back, then store the serial number and version in updateFWCmds array
			if (amc != -1) {
				updateFWCmds[AMC].version = amcVersion;
				updateFWCmds[AMC].name = amcSerialNum;
			}
		}
	}

	STRCPY_S(version, size, updateFWCmds[type].version.c_str());

	return result;
}

/**
 * @brief Retrieve the AMC serial number for the GPU identified by bdfStr.
 *
 * Uses the already-initialized amcupd singleton (via fwupdArray[FWUPD_PREFERENCE_AMC]).
 * amcupd::init() is guarded by std::call_once, so AMC initialization only ever
 * happens once regardless of how many times this function is called.
 *
 * @param[in]  bdfStr     PCI BDF string of the GPU whose AMC serial number is requested.
 * @param[out] serialNum  Buffer to receive the null-terminated serial number string.
 * @param[in]  size       Size of serialNum buffer in bytes.
 * @return ZE_RESULT_SUCCESS on success, ZE_RESULT_ERROR_UNINITIALIZED when no AMC
 *         is registered for this device, ZE_RESULT_ERROR_NOT_AVAILABLE on lookup failure.
 */
ze_result_t firmware::getAmcSerialNumber(const char *bdfStr, char *serialNum, uint32_t size)
{
	TRACING();

	if (!fwupdArray || !fwupdArray[FWUPD_PREFERENCE_AMC]) {
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	if (!bdfStr || !serialNum || size == 0) {
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	amcupd *a = static_cast<amcupd *>(fwupdArray[FWUPD_PREFERENCE_AMC]);
	if (a == nullptr) {
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}
	std::string sn, ver;
	if (a->amcGetCardInfo(std::string(bdfStr), sn, ver) == -1) {
		return ZE_RESULT_ERROR_NOT_AVAILABLE;
	}

	STRCPY_S(serialNum, size, sn.c_str());
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Retrieves the AMC FRU part number for the GPU identified by bdfStr.
 *
 * @param[in]  bdfStr   PCI BDF string of the GPU whose part number is requested.
 * @param[out] partNum  Buffer to receive the null-terminated part number string.
 * @param[in]  size     Size of the partNum buffer in bytes.
 *
 * @retval ZE_RESULT_SUCCESS                Part number retrieved successfully.
 * @retval ZE_RESULT_ERROR_UNINITIALIZED    No AMC entry present in fwupdArray or amcupd cast is null.
 * @retval ZE_RESULT_ERROR_INVALID_ARGUMENT Null pointer or zero-size buffer passed.
 * @retval ZE_RESULT_ERROR_NOT_AVAILABLE    AMC init failure, BDF not found, or part number retrieval failed.
 */
ze_result_t firmware::getAmcPartNumber(const char *bdfStr, char *partNum, uint32_t size)
{
	TRACING();

	if (!fwupdArray || !fwupdArray[FWUPD_PREFERENCE_AMC]) {
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	if (!bdfStr || !partNum || size == 0) {
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	amcupd *a = static_cast<amcupd *>(fwupdArray[FWUPD_PREFERENCE_AMC]);
	if (a == nullptr) {
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	std::string pn;
	if (a->amcGetPartNumberByBdf(std::string(bdfStr), pn) == -1) {
		return ZE_RESULT_ERROR_NOT_AVAILABLE;
	}

	STRCPY_S(partNum, size, pn.c_str());
	return ZE_RESULT_SUCCESS;
}
/*
 * @brief Runs the pre-update, update and post-update handlers of one firmware type
 *
 * The post-update handler is always given a chance to run, so a failed pre-update or update
 * still releases whatever the pre-update acquired.
 *
 * @param cmd The firmware update command row to run
 * @param fwInfo Pointer to firmware information structure containing update details
 * @return ze_result_t ZE_RESULT_SUCCESS if update successful, error code otherwise
 */
ze_result_t firmware::runUpdateSequence(updateFWCmdStruct &cmd, firmwareInfo *fwInfo)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;

	if (cmd.preference < 0 || cmd.preference >= FWUPD_PREFERENCE_MAX) {
		ERR("Invalid firmware update preference.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	if (cmd.preUpdateFunc == nullptr || cmd.updateFunc == nullptr || cmd.postUpdateFunc == nullptr) {
		ERR("Firmware type {} cannot be updated directly.\n", cmd.fwName.c_str());
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	// The firmware update class is based on the update preference
	fwupd *fw = fwupdArray[cmd.preference];

	if ((fwInfo->fwType != fwType::AMC) && fwInfo->firmwareHandle == nullptr) {
		ERR("Failed to find firmware handle 0x{:X} ({})\n", ZE_RESULT_ERROR_UNKNOWN,
			l0_error_to_string(ZE_RESULT_ERROR_UNKNOWN));
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	// Call the corresponding pre-update, firmware update and post-update functions in the hal
	result = (fw->*cmd.preUpdateFunc)(fwInfo);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to pre-update firmware 0x{:X} ({})\n", result, l0_error_to_string(result));
		(fw->*cmd.postUpdateFunc)(fwInfo);
		return result;
	}
	result = (fw->*cmd.updateFunc)(fwInfo);
	if (result != ZE_RESULT_SUCCESS) {
		if (result != ZE_RESULT_ERROR_UNINITIALIZED && result != ZE_RESULT_ERROR_INVALID_ARGUMENT &&
			result != ZE_RESULT_ERROR_INVALID_SIZE) {
			ERR("Failed to update firmware 0x{:X} ({})\n", result, l0_error_to_string(result));
		}
		(fw->*cmd.postUpdateFunc)(fwInfo);
		return result;
	}

	return (fw->*cmd.postUpdateFunc)(fwInfo);
}

/**
 * @brief Retrieve the Thermal Design Power (TDP) for the GPU identified by bdfStr.
 *
 * TDP is read from the FRU OTHER_INFORMATION field via the AMC FRU table (per DSP0257).
 *
 * @param[in]  bdfStr     PCI BDF string of the GPU whose TDP is requested.
 * @param[out] tdp        Buffer to receive the null-terminated TDP string, or nullptr to query length.
 * @param[in,out] bufferSize Pointer to buffer size; updated with required length on output.
 * @return ZE_RESULT_SUCCESS on success, ZE_RESULT_ERROR_UNINITIALIZED when no AMC is available,
 *         ZE_RESULT_ERROR_NOT_AVAILABLE when TDP is not found in FRU data.
 */
ze_result_t firmware::getAmcTdp(const char *bdfStr, char *tdp, size_t *bufferSize)
{
	TRACING();

	if (!fwupdArray || !fwupdArray[FWUPD_PREFERENCE_AMC]) {
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	if (!bdfStr || !bufferSize) {
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	amcupd *a = static_cast<amcupd *>(fwupdArray[FWUPD_PREFERENCE_AMC]);
	if (a == nullptr) {
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	if (a->amcGetTdp(std::string(bdfStr), tdp, bufferSize) != AMC_SUCCESS) {
		return ZE_RESULT_ERROR_NOT_AVAILABLE;
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Updates firmware with the provided firmware information
 *
 * This function performs a firmware update operation using the information
 * provided in the firmware info structure. It handles the complete update
 * process including pre-update, update, and post-update operations.
 *
 * @param fwInfo Pointer to firmware information structure containing update details
 * @return ze_result_t ZE_RESULT_SUCCESS if update successful, error code otherwise
 */
ze_result_t firmware::updateFW(firmwareInfo *fwInfo)
{
	TRACING();

	// A composite package is not a single image: it is unwrapped and its components are flashed
	// through the firmware types below.
	if (!STRCASECMP(fwInfo->firmwareType.c_str(), TOSTR(COMPOSITE))) {
		fwInfo->fwType = COMPOSITE;
		return updateComposite(fwInfo);
	}

	for (uint32_t i = 0; i < MAX_FW_TYPE; i++) {
		// Find the matching firmware type
		if (!STRCASECMP(fwInfo->firmwareType.c_str(), updateFWCmds[i].fwName.c_str())) {
			fwInfo->fwType = updateFWCmds[i].fw;
			fwInfo->firmwareHandle = updateFWCmds[i].firmwareHandle;
			return runUpdateSequence(updateFWCmds[i], fwInfo);
		}
	}

	ERR("Invalid firmware type: {}\n", fwInfo->firmwareType.c_str());
	return ZE_RESULT_ERROR_UNKNOWN;
}

/**
 * @brief Decides whether a component of a composite package is flashed by this call
 *
 * A component is selected only when it maps to a firmware type, the user opted in to it if it
 * needs opting in, the update pass matches the component's transport, and the device actually
 * exposes the endpoint. Anything else is skipped without affecting the result of the update, as
 * a composite package is built for a family of boards rather than for one device.
 *
 * @param identifier ComponentIdentifier read from the package
 * @param fwInfo Pointer to firmware information structure containing update details
 * @param logSkips true to explain every skip at debug level, false to decide quietly
 * @return const compositeComponentMap* The mapping row to flash, or nullptr to skip the component
 */
const compositeComponentMap *firmware::selectComponent(uint16_t identifier, const firmwareInfo *fwInfo, bool logSkips)
{
	const compositeComponentMap *map = nullptr;

	for (const auto &entry : compositeComponents) {
		if (entry.id == identifier) {
			map = &entry;
			break;
		}
	}

	if (map == nullptr) {
		if (logSkips) {
			DBG("Skipping component 0x{:04X}: unknown component identifier.\n", identifier);
		}
		return nullptr;
	}

	if (map->fw == MAX_FW_TYPE) {
		if (logSkips) {
			DBG("Skipping component {} (0x{:04X}): {}.\n", map->name, identifier, map->skipReason);
		}
		return nullptr;
	}

	// --fdo flashes the opt-in component and nothing else; without it the opt-in component is the
	// only one left out.
	if (map->optIn != fwInfo->fdoOnly) {
		if (logSkips) {
			if (map->optIn) {
				DBG("Skipping component {} (0x{:04X}): {}.\n", map->name, identifier, map->skipReason);
			} else {
				DBG("Skipping component {} (0x{:04X}): --fdo flashes the recovery image only.\n", map->name,
					identifier);
			}
		}
		return nullptr;
	}

	// The AMC is shared by every GPU on the card, so it is flashed by its own pass rather than
	// once per attached device.
	bool amcPass = (fwInfo->scope == COMPOSITE_SCOPE_AMC);
	if ((map->fw == AMC) != amcPass) {
		if (logSkips) {
			DBG("Skipping component {} (0x{:04X}): not part of this update pass.\n", map->name, identifier);
		}
		return nullptr;
	}

	if (map->fw != AMC && updateFWCmds[map->fw].firmwareHandle == nullptr) {
		if (logSkips) {
			DBG("Skipping component {} (0x{:04X}): device {} has no {} firmware to flash it to.\n", map->name,
				identifier, fwInfo->deviceIndex, updateFWCmds[map->fw].fwName.c_str());
		}
		return nullptr;
	}

	return map;
}

/**
 * @brief Updates every applicable component of a PLDM DSP0267 Type 5 firmware package
 *
 * The package is validated, its Component Image Information Area is read, and each component the
 * device can take is flashed in package table order. Components reachable through sysman are
 * extracted here and handed to zesFirmwareFlash as an in-memory image; the AMC component is left
 * in the package and pulled out by the AMC itself over PLDM, so only its identifier is passed on.
 *
 * Components that do not apply are skipped without failing the update, but a component that has an
 * endpoint and fails to flash aborts the remaining components on this device.
 *
 * @param fwInfo Pointer to firmware information structure containing update details
 * @return ze_result_t ZE_RESULT_SUCCESS if every applicable component was flashed, error code otherwise
 */
ze_result_t firmware::updateComposite(firmwareInfo *fwInfo)
{
	TRACING();

	if (fwupdArray == nullptr || fwupdArray[FWUPD_PREFERENCE_SYSMAN] == nullptr) {
		ERR("Firmware update interfaces are not initialized.\n");
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	std::vector<char> package = fwupdArray[FWUPD_PREFERENCE_SYSMAN]->readImageContent(fwInfo->filePath.c_str());
	if (package.empty()) {
		ERR("Firmware package '{}' is empty or unreadable.\n", fwInfo->filePath.c_str());
		return ZE_RESULT_ERROR_INVALID_SIZE;
	}

	const uint8_t *data = reinterpret_cast<const uint8_t *>(package.data());
	const size_t len = package.size();
	const char *formatName = nullptr;

	if (!pldm_fw::isPldmType5(data, len, &formatName)) {
		ERR("Firmware package '{}' is not a PLDM firmware update package.\n", fwInfo->filePath.c_str());
		return ZE_RESULT_ERROR_INVALID_NATIVE_BINARY;
	}
	DBG("Composite package '{}' is {}.\n", fwInfo->filePath.c_str(), formatName);

	// The package checksums cover the header and the payload, so a corrupt file is caught before
	// anything is written to the hardware.
	pldm_fw::ChecksumStatus checksums;
	if (pldm_fw::verifyChecksums(data, len, &checksums) != pldm_fw::Result::Success) {
		ERR("Firmware package '{}' is corrupt: header checksum 0x{:08X} computed 0x{:08X}, payload checksum 0x{:08X} "
			"computed 0x{:08X}.\n",
			fwInfo->filePath.c_str(), checksums.storedHeader, checksums.computedHeader, checksums.storedPayload,
			checksums.computedPayload);
		return ZE_RESULT_ERROR_INVALID_NATIVE_BINARY;
	}

	std::vector<pldm_fw::ComponentInfo> components;
	pldm_fw::Result rc = pldm_fw::getComponentList(data, len, components);
	if (rc != pldm_fw::Result::Success) {
		ERR("Failed to read the component list of '{}': {}\n", fwInfo->filePath.c_str(), pldm_fw::resultToString(rc));
		return ZE_RESULT_ERROR_INVALID_NATIVE_BINARY;
	}

	// Count what applies before flashing anything, so each component can be labelled "n of total".
	uint32_t total = 0;
	for (const auto &component : components) {
		if (selectComponent(component.identifier, fwInfo, false) != nullptr) {
			total++;
		}
	}

	uint32_t current = 0;
	for (const auto &component : components) {
		const compositeComponentMap *map = selectComponent(component.identifier, fwInfo, true);
		if (map == nullptr) {
			continue;
		}

		updateFWCmdStruct &cmd = updateFWCmds[map->fw];
		firmwareInfo componentInfo = *fwInfo;

		current++;
		componentInfo.fwType = map->fw;
		componentInfo.firmwareType = cmd.fwName;
		componentInfo.firmwareHandle = cmd.firmwareHandle;
		componentInfo.imageLabel = std::string(map->name) + " " + std::to_string(current) + "/" + std::to_string(total);

		if (map->fw == AMC) {
			// The AMC takes the package file itself and is told which component to pick out of it.
			componentInfo.pldmComponentId = component.identifier;
		} else {
			componentInfo.buffer.resize(component.requiredSize);
			size_t written = 0;
			rc = pldm_fw::extractComponent(data, len, component.identifier,
										   reinterpret_cast<uint8_t *>(componentInfo.buffer.data()),
										   componentInfo.buffer.size(), &written, component.occurrence);
			if (rc != pldm_fw::Result::Success) {
				ERR("Failed to extract component {} (0x{:04X}) from '{}': {}\n", map->name, component.identifier,
					fwInfo->filePath.c_str(), pldm_fw::resultToString(rc));
				return ZE_RESULT_ERROR_INVALID_NATIVE_BINARY;
			}
			componentInfo.buffer.resize(written);
			componentInfo.imagePreloaded = true;
		}

		DBG("Flashing component {} (0x{:04X}, {} bytes, version '{}') as {} on device {}.\n", map->name,
			component.identifier, component.requiredSize, component.version, cmd.fwName.c_str(), fwInfo->deviceIndex);

		ze_result_t result = runUpdateSequence(cmd, &componentInfo);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to update {} on device {}, the remaining components of '{}' were not applied.\n",
				componentInfo.imageLabel.c_str(), fwInfo->deviceIndex, fwInfo->filePath.c_str());
			return result;
		}
	}

	if (current == 0) {
		DBG("Package '{}' carries no component that applies to device {}.\n", fwInfo->filePath.c_str(),
			fwInfo->deviceIndex);
	}

	return ZE_RESULT_SUCCESS;
}

/*
 * @brief Checks if the device has an AMC card associated with a specific GPU BDF
 *
 * This function determines if there is an AMC card linked to the GPU identified
 * by the provided Bus-Device-Function (BDF) string.
 *
 * @param gpuBdfStr The BDF string of the GPU to check (e.g., "0000:00:02.0")
 * @return Index of the AMC card in amcDeviceList if found, -1 if not found
 */
int firmware::getAmcIndex(std::string gpuBdfStr)
{
	int amc = -1;
	if (fwupdArray && fwupdArray[FWUPD_PREFERENCE_AMC]) {
		amcupd *a = static_cast<amcupd *>(fwupdArray[FWUPD_PREFERENCE_AMC]);
		// Along with the index, let's also find out the serial number and version of the AMC card
		amc = a->amcGetIndex(gpuBdfStr);
	}

	return amc;
}

/**
 * @brief Checks if AMC firmware was enumerated by Level Zero
 *
 * This function checks whether the device has AMC firmware by examining
 * the firmware handles that were enumerated by Level Zero. This is a
 * reliable way to detect AMC capability without hardcoding device IDs.
 *
 * @return bool True if AMC firmware is available, false otherwise
 */
bool firmware::hasAmcFirmware()
{
	if (!propertiesList || firmwareCount == 0) {
		return false;
	}

	// Check if any enumerated firmware has "AMC" in its name
	for (uint32_t i = 0; i < firmwareCount; i++) {
		if (strstr(propertiesList[i].name, "AMC")) {
			return true;
		}
	}
	return false;
}

/**
 * @brief Initializes the firmware management subsystem for a device
 *
 * This function initializes firmware management capabilities for the
 * specified device, including enumeration of available firmware modules
 * and preparation for firmware operations.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t firmware::init(zes_device_handle_t device)
{
	ze_result_t result = enumFirmwares(device);

	// Create a fwupd array that holds all different types of firmware updates
	fwupdArray = new fwupd *[FWUPD_PREFERENCE_MAX];
	fwupdArray[FWUPD_PREFERENCE_GSC] = new gscupd();
	fwupdArray[FWUPD_PREFERENCE_SYSMAN] = new sysmanupd();
	fwupdArray[FWUPD_PREFERENCE_AMC] = new amcupd();

	for (uint32_t i = 0; i < firmwareCount; i++) {
		result = getProperties(firmwareList[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
	}

	return result;
}

/**
 * @brief Executes firmware-related runtime operations
 *
 * This function is called during runtime operations for firmware management.
 * Currently serves as a placeholder for future firmware runtime functionality.
 *
 * @param device Handle to the Level Zero Sysman device (currently unused)
 * @return ze_result_t Always returns ZE_RESULT_SUCCESS
 */
ze_result_t firmware::zesRun(UNUSED zes_device_handle_t device) { return ZE_RESULT_SUCCESS; }
