/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "pldm_fwupdate.h"
#include "pldm.h"
#include <string>

/**
 * @brief Execute pldm firmware update command
 *
 * Sends a pldm firmware update command to the device. Manages pldm instance ID,
 * constructs the command packet, and handles the communication protocol.
 *
 * @param cmd Firmware update command code to execute
 * @param size Size of the command payload in bytes
 *
 * @return uint8_t Status of the command execution
 * @retval PLDM_SUCCESS Command executed successfully
 * @retval PLDM_ERROR_* Various error conditions
 *
 * @note Automatically manages pldm instance ID wrapping at maximum value
 */
uint8_t pldm::fwUpdCmd(uint8_t cmd, uint8_t size)
{
	TRACING();
	mFwuCmdLen = size;
	mI2cMultiResp = false;
	uint8_t *wptr = (uint8_t *)mI2cPldmWrite;
	uint8_t *rptr = (uint8_t *)mI2cPldmRead;

	if (instanceID == PLDM_INSTANCE_ID_MAX) {
		instanceID = 1;
	}

	// Fill the payload based on command
	if (pldmFwUpdFillPayload(cmd, mFwuCmdLen) != PLDM_SUCCESS) {
		ERR("FWU : Failed to fill payload for command 0x{:02x}\n", cmd);
		return PLDM_ERROR;
	}

	// UA will calculate this PAK_SEQ for multi packet messages, currently we are not using multi packet messages
	commandConstruction(&mI2cPldmWrite->mctpSmbusHdr, MCTP_SOM, MCTP_EOM, MCTP_PAK_SEQ, (mFwuCmdLen - 3),
						MCTP_INTEGRITY_CHECK);
	mI2cPldmWrite->mctpSmbusHdr.msgType = PLDM_OVER_MCTP;
	pldmHdrConstruction(&mI2cPldmWrite->pldmHdr, instanceID, PLDM_FIRMWARE_UPDATE, cmd, PLDM_ASYNC_REQUEST_NOTIFY,
						PLDM_REQUEST);

	DBG("pldm TX  :: ");
	hexdump((uint8_t *)mI2cPldmWrite, mFwuCmdLen);

	if (i2cobj->writeAmc(wptr + 1, mFwuCmdLen) != true) {
		ERR("FWU : I2C Write failure\n");
		return PLDM_ERROR;
	}

	// For ActivateFirmware command don't do I2C read, post AMC fw update this cmd will
	// reboot the AMC, I2C channel will be lost and we won't get any response
	if (cmd != ACTIVATE_FIRMWARE) {
		constexpr auto kResponseTimeout = std::chrono::seconds(5);
		auto readDeadline = std::chrono::steady_clock::now() + kResponseTimeout;
		while (true) {
			// Wait for I2C event
			MSLEEP(I2C_EVENT_WAIT_PERIOD_MS);

			if (i2cobj->readAmc(rptr + 1, PLDM_MAX_RESPONSE_SIZE) != true) {
				ERR("FWU : I2C Read failure\n");
				return PLDM_ERROR;
			}

			// Skip "device not ready" (cmdCode != 0x0F) - keep polling until
			// a real MCTP frame arrives or the deadline elapses.
			if (mI2cPldmRead->mctpSmbusHdr.cmdCode != MCTP_CMD_CODE) {
				if (std::chrono::steady_clock::now() > readDeadline) {
					ERR("FWU : Timed out waiting for MCTP response to cmd 0x{:02x} "
						"(>{} of \"device not ready\")\n",
						cmd, kResponseTimeout);
					return PLDM_ERROR;
				}
				continue;
			}

			if (fwUpdateResp(cmd, instanceID) != PLDM_SUCCESS) {
				return PLDM_ERROR;
			}

			if (mI2cMultiResp == false) {
				// If not multi response, break the loop
				break;
			}
		}
	}

	instanceID++;
	return PLDM_SUCCESS;
}

/**
 * @brief Fill payload data for pldm firmware update commands
 *
 * Constructs the appropriate payload data based on the specified firmware update
 * command. Different commands require different payload structures and data.
 *
 * @param cmd Firmware update command code that determines payload structure
 * @param size Expected size of the payload in bytes
 *
 * @return uint8_t Status of payload construction
 * @retval PLDM_SUCCESS Payload filled successfully
 * @retval PLDM_ERROR_UNSUPPORTED_CMD Unsupported command type
 * @retval PLDM_ERROR Invalid size or other error
 *
 * @note Some commands like QUERY_DEVICE_IDENTIFIERS require no payload data
 */
uint8_t pldm::pldmFwUpdFillPayload(uint8_t cmd, uint8_t size)
{
	TRACING();
	uint8_t ret = PLDM_SUCCESS;

	switch (cmd) {
	case QUERY_DEVICE_IDENTIFIERS:
	case GET_FIRMWARE_PARAMETERS:
	case CANCEL_UPDATE_COMPONENT:
	case CANCEL_UPDATE:
	case GET_STATUS:
		// Initialize payload buffer to prevent reading uninitialized memory during CRC calculation
		memset(mI2cPldmWrite->respPayload, 0, size);
		// No Payload data, CRC is calculated over the payload excluding the CRC byte itself
		mI2cPldmWrite->respPayload[BYTE_0] = crc8Smbus(mI2cPldmWrite->respPayload, size - 1);
		break;

	case ACTIVATE_FIRMWARE:
		mI2cPldmWrite->respPayload[BYTE_0] = 0x01; // TRUE - this will activate AMC to update firmware
		mI2cPldmWrite->respPayload[BYTE_1] = crc8Smbus(mI2cPldmWrite->respPayload, size - 1);
		break;

	case REQUEST_UPDATE:
		if (fwReqUpdate() != PLDM_SUCCESS) {
			ERR("FWU : Failed to fill payload for REQUEST_UPDATE\n");
			return PLDM_ERROR;
		}
		break;

	case PASS_COMPONENT_TABLE:
		if (fwupdPassCompTable() != PLDM_SUCCESS) {
			ERR("FWU : Failed to fill payload for PASS_COMPONENT_TABLE\n");
			return PLDM_ERROR;
		}
		break;

	case UPDATE_COMPONENT:
		if (fwUpdComp() != PLDM_SUCCESS) {
			ERR("FWU : Failed to fill payload for UPDATE_COMPONENT\n");
			return PLDM_ERROR;
		}
		break;

	default:
		ERR("FWU : Invalid Firmware Update Command\n");
		ret = PLDM_ERROR;
		break;
	}

	return ret;
}

/**
 * @brief Initialize pldm firmware update process
 *
 * Parses the firmware package file and executes the complete firmware update
 * sequence for all components. This includes device identification, parameter
 * retrieval, update requests, component table passing, and firmware activation.
 *
 * @param pkgFilePath Path to the firmware package file to process
 *
 * @return uint8_t Status of the firmware update initialization
 * @retval PLDM_SUCCESS All firmware update operations completed successfully
 * @retval PLDM_ERROR File access error, parsing failure, or command execution error
 *
 * @note This function performs the complete firmware update workflow
 * @note Automatically sends CANCEL_UPDATE commands on failure for cleanup
 * @note Manages file operations and closes file handle before returning
 */
uint8_t pldm::fwUpdInitialize(const char *pkgFilePath)
{
	TRACING();
	uint8_t ret = PLDM_ERROR;

	fwupd_cmds_t cmdTable[] = {{"QueryDeviceIdentifiers", QUERY_DEVICE_IDENTIFIERS, QUERY_DEVICE_IDENTIFIERS_SIZE},
							   {"GetFirmwareParameters", GET_FIRMWARE_PARAMETERS, GET_FIRMWARE_PARAMETERS_SIZE},
							   {"RequestUpdate", REQUEST_UPDATE, FWU_COMMAND_BASE_SIZE},
							   {"PassComponentTable", PASS_COMPONENT_TABLE, FWU_COMMAND_BASE_SIZE},
							   {"UpdateComponent", UPDATE_COMPONENT, FWU_COMMAND_BASE_SIZE}};

	// Initializing pldm Firmware Update variables to Zero
	mReqUpdate = {};
	mPassCompTable = {};
	mUpdComp = {};
	mLastFwuCompletionCode = PLDM_SUCCESS;

	// Track whether an update session has been established on the device
	// (RequestUpdate succeeded). When true, we must send CancelUpdate on any
	// early return so the device does not stay stuck in update mode.
	bool updateStarted = false;
	uint8_t finalStatus = PLDM_ERROR;

	// Closes the package file, frees the parsed package, and sends CancelUpdate
	// if an update session was started but the flow did not complete successfully.
	auto cleanupAndReturn = [&](uint8_t status) -> uint8_t {
		finalStatus = status;
		if (updateStarted && status != PLDM_SUCCESS) {
			if (mLastFwuCompletionCode != NOT_IN_UPDATE_MODE) {
				DBG(">>> Send CANCEL UPDATE\n");
				if (fwUpdCmd(CANCEL_UPDATE, FWU_COMMAND_BASE_SIZE) == PLDM_SUCCESS) {
					DBG("<<< CANCEL UPDATE Success\n");
				} else {
					DBG("<<< CANCEL UPDATE Failed\n");
				}
			}
		}
		pkg.reset();
		if (mCompFp != NULL) {
			fclose(mCompFp);
			mCompFp = NULL;
		}
		return finalStatus;
	};

	DBG("\n=====================AMC Firmware File Parser===============================\n");
	if (FOPEN_S(&mCompFp, pkgFilePath, "rb") != 0 || mCompFp == NULL) {
		ERR("Failed to open package file: {}\n", pkgFilePath);
		return PLDM_ERROR;
	}

	uint8_t result = fwpkgParseInfo(pkgFilePath);
	if (result != PLDM_SUCCESS) {
		ERR("Failed to parse firmware package info from file: {}\n", pkgFilePath);
		return cleanupAndReturn(PLDM_ERROR);
	} else {
		DBG("Firmware package info parsed successfully from file: {}\n", pkgFilePath);
	}
	DBG("Firmware package info parsed successfully from AMC img file for card : {:02}\n", mCardNum);

	// If the user requested --force, make sure every component image in the package
	// advertises the ForceUpdate capability (bit 0 of ComponentOptions per DSP0267).
	// Fail fast before any PLDM traffic is sent so the caller gets a clear reason.
	if (mForceUpdate) {
		for (int i = 0; i < pkg->compImagesInfo.compImageCount; i++) {
			if (!pkg->compImagesInfo.compImages[i].compOptions.bits.bit0) {
				ERR("Force update requested for card {:02} but firmware image '{}' is not "
					"downgradable: ForceUpdate bit is not set in ComponentOptions for component {}.\n",
					mCardNum, pkgFilePath, i);
				return cleanupAndReturn(PLDM_ERROR);
			}
		}
		DBG("Force update requested and all {} component(s) advertise the ForceUpdate capability.\n",
			pkg->compImagesInfo.compImageCount);
	}

	for (int i = 0; i < pkg->compImagesInfo.compImageCount; i++) {
		DBG("Component Number = {}\n", i + 1);
		DBG("Offset = {} & Size = {}\n", pkg->compImagesInfo.compImages[i].compLocOffset,
			pkg->compImagesInfo.compImages[i].compSize);
	}

	// Firmware Update Inventory Commands
	for (uint8_t n = 0; n < pkg->compImagesInfo.compImageCount; n++) {
		DBG("\n========= Firmware Update Inventory for Component :: {} =========\n", n + 1);
		mCurComp = n;
		for (uint8_t i = 0; i < ARRAY_SIZE(cmdTable); i++) {
			DBG(">>> Send {}\n", cmdTable[i].name.c_str());
			ret = fwUpdCmd((uint8_t)cmdTable[i].cmd, cmdTable[i].size);

			// If RequestUpdate fails because the device is already in update
			// mode (0x81), it is carrying a stale session from a previous
			// attempt. Cancel that session and retry RequestUpdate once - the
			// same recovery amctool performs.
			if (ret != PLDM_SUCCESS && (uint8_t)cmdTable[i].cmd == REQUEST_UPDATE &&
				mLastFwuCompletionCode == ALREADY_IN_UPDATE_MODE) {
				DBG("FWU : RequestUpdate reported ALREADY_IN_UPDATE_MODE; cancelling stale session and retrying\n");
				(void)fwUpdCmd(CANCEL_UPDATE, FWU_COMMAND_BASE_SIZE);
				ret = fwUpdCmd((uint8_t)cmdTable[i].cmd, cmdTable[i].size);
			}

			if (ret != PLDM_SUCCESS) {
				ERR("FWU : {} Failed!!! \n", cmdTable[i].name.c_str());
				return cleanupAndReturn(ret);
			}
			DBG("<<< {} Success...\n", cmdTable[i].name.c_str());

			// After a successful RequestUpdate the device is in update mode
			// and must receive CancelUpdate on any early return from here on.
			if ((uint8_t)cmdTable[i].cmd == REQUEST_UPDATE) {
				updateStarted = true;
			}
		}

		DBG("Firmware Inventory commands completed, starting firmware update now for card : {:02}\n", mCardNum);

		DBG("\n========= Firmware Update Read from FirmwareDevice(FD) for Component :: {} =========\n", n + 1);
		if (readFromFD() != PLDM_SUCCESS) {
			return cleanupAndReturn(PLDM_ERROR);
		}
	}

	DBG(">>> Send GetStatus\n");
	if (fwUpdCmd(GET_STATUS, FWU_COMMAND_BASE_SIZE) != PLDM_SUCCESS) {
		ERR("FWU : GetStatus Failed!!!\n");
		return cleanupAndReturn(PLDM_ERROR);
	}
	DBG("<<< GetStatus Success...\n");

	DBG("\n========= Activate Firmware for all Components =========\n");
	DBG(">>> Send ActivateFirmware\n");
	if (fwUpdCmd(ACTIVATE_FIRMWARE, ACTIVATE_FIRMWARE_SIZE) != PLDM_SUCCESS) {
		ERR("FWU : Failed!!! Activate Firmware\n");
		return cleanupAndReturn(PLDM_ERROR);
	}
	DBG("<<< ActivateFirmware Success...\n");
	DBG("Firmware Update and Activation completed successfully for card : {:02}\n", mCardNum);

	// ActivateFirmware succeeded (or the device already self-activated) - the
	// update session is finished so no CancelUpdate is required.
	updateStarted = false;
	return cleanupAndReturn(PLDM_SUCCESS);
}

/**
 * @brief Send GET_FIRMWARE_PARAMETERS command and cache the parsed response.
 *
 * Sends PLDM firmware update command GET_FIRMWARE_PARAMETERS (0x02) to the AMC
 * and stores the raw response payload in mFwParamRawData. On success the
 * parsed ActiveComponentImageSetVersionString is stored in mFwParamActiveVersion.
 *
 * The response is assembled by pldmFwGetParamPayload() which appends each
 * received MCTP packet's PLDM payload bytes into mFwParamRawData (SOM and
 * continuation frames are handled there). After fwUpdCmd() returns, this
 * function calls parseFwParamResponse() to decode the accumulated buffer.
 *
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure.
 *
 * @note Follows DSP0267 section 11.2.
 * @note On success mFwParamsInitialized is set to true and mFwParamActiveVersion
 *       holds the active component image set version string.
 */
uint8_t pldm::getFirmwareParameters()
{
	TRACING();

	mFwParamRawData.clear();
	mFwParamsInitialized = false;
	mFwParamActiveVersion.clear();

	DBG("\n========= GetFirmwareParameters CARD {} =========\n", mCardNum);
	DBG(">>> Send GetFirmwareParameters\n");

	if (fwUpdCmd(GET_FIRMWARE_PARAMETERS, GET_FIRMWARE_PARAMETERS_SIZE) != PLDM_SUCCESS) {
		ERR("FWU: GET_FIRMWARE_PARAMETERS command failed on card {}\n", mCardNum);
		return PLDM_ERROR;
	}

	DBG("<<< GetFirmwareParameters received {} bytes of payload\n", mFwParamRawData.size());

	return parseFwParamResponse();
}

/**
 * @brief Parse the GET_FIRMWARE_PARAMETERS response buffer per DSP0267 section 11.2.
 *
 * Decodes the PLDM payload accumulated in mFwParamRawData. Walks the
 * ComponentParameterTable (DSP0267 Table 19) and extracts the
 * ActiveComponentVersionString from the first entry whose ComponentClassification
 * is COMP_CLASS_FIRMWARE (0x000A).
 *
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure.
 *
 * @note On success mFwParamsInitialized is set to true and mFwParamActiveVersion
 *       contains the ActiveComponentVersionString of the matched component.
 */
uint8_t pldm::parseFwParamResponse()
{
	TRACING();

	const uint8_t *payload = mFwParamRawData.data();
	const size_t len = mFwParamRawData.size();

	// DSP0267 section 11.2 Table 18 - GET_FIRMWARE_PARAMETERS response layout:
	constexpr size_t kResponseHeaderLen = 11;

	if (len < kResponseHeaderLen) {
		ERR("FWU: GET_FIRMWARE_PARAMETERS response too short ({} bytes, need at least {})\n", len, kResponseHeaderLen);
		return PLDM_ERROR;
	}

	// Byte 0: CompletionCode
	if (payload[0] != PLDM_SUCCESS) {
		ERR("FWU: GET_FIRMWARE_PARAMETERS completion code 0x{:02x}\n", payload[0]);
		return PLDM_ERROR;
	}

	// Bytes 5-6: ComponentCount (little-endian uint16)
	const uint16_t compCount = static_cast<uint16_t>(payload[5]) | (static_cast<uint16_t>(payload[6]) << 8);
	DBG("FWU: ComponentCount = {}\n", compCount);

	if (compCount == 0) {
		ERR("FWU: GET_FIRMWARE_PARAMETERS: no components reported\n");
		return PLDM_ERROR;
	}

	// Bytes 8 and 10: image-set version string lengths — used only to skip to
	// the start of the ComponentParameterTable.
	const uint8_t imgSetActiveLen = payload[8];
	const uint8_t imgSetPendingLen = payload[10];

	// Offset to the first ComponentParameterTable entry
	size_t tableOffset = kResponseHeaderLen + imgSetActiveLen + imgSetPendingLen;

	// DSP0267 Table 19 - ComponentParameterTable entry fixed header size
	constexpr size_t kCompParamEntryFixedLen = 39;

	// Walk each ComponentParameterTable entry (DSP0267 Table 19)
	for (uint16_t i = 0; i < compCount; ++i) {
		if (tableOffset + kCompParamEntryFixedLen > len) {
			ERR("FWU: component parameter table entry {} truncated\n", i);
			return PLDM_ERROR;
		}

		const uint8_t *entry = payload + tableOffset;

		const uint16_t classification = static_cast<uint16_t>(entry[0]) | (static_cast<uint16_t>(entry[1]) << 8);
		const uint16_t identifier = static_cast<uint16_t>(entry[2]) | (static_cast<uint16_t>(entry[3]) << 8);
		const uint8_t activeCompVerStrLen = entry[10];
		const uint8_t pendingCompVerStrLen = entry[24];
		const size_t entrySize = kCompParamEntryFixedLen + activeCompVerStrLen + pendingCompVerStrLen;

		DBG("FWU: component[{}]: classification=0x{:04x} identifier=0x{:04x} "
			"activeCompVerStrLen={}\n",
			i, classification, identifier, activeCompVerStrLen);

		if (tableOffset + entrySize > len) {
			ERR("FWU: component parameter table entry {} version string truncated\n", i);
			return PLDM_ERROR;
		}

		// Match AMC firmware component by classification
		if (classification == COMP_CLASS_FIRMWARE) {
			if (activeCompVerStrLen > 0) {
				mFwParamActiveVersion =
					std::string(reinterpret_cast<const char *>(entry + kCompParamEntryFixedLen), activeCompVerStrLen);
				mFwParamsInitialized = true;
				DBG("FWU: AMC ActiveComponentVersionString (class=0x{:04x}) = \"{}\"\n", classification,
					mFwParamActiveVersion.c_str());
				return PLDM_SUCCESS;
			}
		}

		tableOffset += entrySize;
	}

	ERR("FWU: no firmware component (0x{:04x}) found in "
		"ComponentParameterTable\n",
		COMP_CLASS_FIRMWARE);
	return PLDM_ERROR;
}
