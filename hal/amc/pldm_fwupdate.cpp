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

#include "pldm_fwupdate.h"
#include "pldm.h"

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
		ERR("FWU : Failed to fill payload for command 0x%02x\n", cmd);
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
		while (true) {
			// Wait for I2C event
			MSLEEP(I2C_EVENT_WAIT_PERIOD_MS);

			if (i2cobj->readAmc(rptr + 1, PLDM_MAX_RESPONSE_SIZE) != true) {
				ERR("FWU : I2C Read failure\n");
				return PLDM_ERROR;
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

	DBG("\n=====================AMC Firmware File Parser===============================\n");
	if (FOPEN_S(&mCompFp, pkgFilePath, "rb") != 0 || mCompFp == NULL) {
		ERR("Failed to open package file: %s\n", pkgFilePath);
		return PLDM_ERROR;
	}

	uint8_t result = fwpkgParseInfo(pkgFilePath);
	if (result != PLDM_SUCCESS) {
		ERR("Failed to parse firmware package info from file: %s\n", pkgFilePath);
		return PLDM_ERROR;
	} else {
		DBG("Firmware package info parsed successfully from file: %s\n", pkgFilePath);
	}
	DBG("Firmware package info parsed successfully from AMC img file for card : %02d\n", mCardNum);

	for (int i = 0; i < pkg->compImagesInfo.compImageCount; i++) {
		DBG("Component Number = %d\n", i + 1);
		DBG("Offset = %u & Size = %u\n", pkg->compImagesInfo.compImages[i].compLocOffset,
			pkg->compImagesInfo.compImages[i].compSize);
	}

	// Firmware Update Inventory Commands
	for (uint8_t n = 0; n < pkg->compImagesInfo.compImageCount; n++) {
		DBG("\n========= Firmware Update Inventory for Component :: %d =========\n", n + 1);
		mCurComp = n;
		for (uint8_t i = 0; i < ARRAY_SIZE(cmdTable); i++) {
			DBG(">>> Send %s\n", cmdTable[i].name.c_str());
			ret = fwUpdCmd((uint8_t)cmdTable[i].cmd, cmdTable[i].size);
			if (ret != PLDM_SUCCESS) {
				ERR("FWU : %s Failed!!! \n", cmdTable[i].name.c_str());
				if (((uint8_t)cmdTable[i].cmd == REQUEST_UPDATE) ||
					((uint8_t)cmdTable[i].cmd == PASS_COMPONENT_TABLE) ||
					((uint8_t)cmdTable[i].cmd == UPDATE_COMPONENT)) {
					// Send CANCEL for these three commands only
					PRINT(">>> Send CANCEL UPDATE\n");
					if (fwUpdCmd(CANCEL_UPDATE, FWU_COMMAND_BASE_SIZE) == PLDM_SUCCESS) {
						DBG("<<< CANCEL UPDATE Success\n");
					} else {
						DBG("<<< CANCEL UPDATE Failed\n");
					}
				}
				fclose(mCompFp);
				return ret;
			}
			DBG("<<< %s Success...\n", cmdTable[i].name.c_str());
		}

		DBG("Firmware Inventory commands completed, starting firmware update now for card : %02d\n", mCardNum);

		DBG("\n========= Firmware Update Read from FirmwareDevice(FD) for Component :: %d =========\n", n + 1);
		if (readFromFD() != PLDM_SUCCESS) {
			PRINT(">>> Send CANCEL UPDATE\n");
			// Send CANCEL commands to pldm
			if (fwUpdCmd(CANCEL_UPDATE, FWU_COMMAND_BASE_SIZE) == PLDM_SUCCESS) {
				DBG("<<< CANCEL UPDATE Success\n");
			} else {
				DBG("<<< CANCEL UPDATE Failed\n");
			}
			fclose(mCompFp);
			return PLDM_ERROR;
		}
	}

	// GET_STATUS
	DBG(">>> Send GetStatus\n");
	if (fwUpdCmd(GET_STATUS, FWU_COMMAND_BASE_SIZE) != PLDM_SUCCESS) {
		ERR("FWU : GetStatus Failed!!!\n");
		fclose(mCompFp);
		return ret;
	}
	DBG("<<< GetStatus Success...\n");

	DBG("\n========= Activate Firmware for all Components =========\n");
	DBG(">>> Send ActivateFirmware\n");
	if (fwUpdCmd(ACTIVATE_FIRMWARE, ACTIVATE_FIRMWARE_SIZE) != PLDM_SUCCESS) {
		ERR("FWU : Failed!!! Activate Firmware\n");
		fclose(mCompFp);
		return ret;
	}
	DBG("<<< ActivateFirmware Success...\n");
	DBG("Firmware Update and Activation completed successfully for card : %02d\n", mCardNum);

	fclose(mCompFp);
	return PLDM_SUCCESS;
}
