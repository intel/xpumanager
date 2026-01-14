/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "pldm_platform.h"
#include "pldm.h"
#include "pldm_constants.h"
#include <sstream>
#include <cstring>

/**
 * @brief Fills the payload for PLDM platform commands
 *
 * Constructs the payload for various PLDM platform commands based on the command code.
 * The PDR header is based on DSP0248 specification.
 *
 * @param[in] cmd The PLDM command code
 * @param[in] size The size of the payload
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::pfFillPayload(uint8_t cmd, uint8_t size)
{
	TRACING();
	uint8_t offset = 0;
	uint8_t *payloadPtr = mI2cPldmWrite->respPayload;
	memset(mI2cPldmWrite->respPayload, 0, size);
	switch (cmd) {
	case PLDM_GET_PDR_REPOSITORY_INFO:
		DBG("PLDM Platform: Get PDR Repository Info payload filled\n");
		payloadPtr[0] = crc8Smbus(payloadPtr, size - 1);
		break;
	case PLDM_GET_PDR:
		offset = 0;
		memcpy(payloadPtr + offset, &pfPdrReq.recordHandle, sizeof(pfPdrReq.recordHandle));
		offset += sizeof(pfPdrReq.recordHandle);
		memcpy(payloadPtr + offset, &pfPdrReq.dataTransferHandle, sizeof(pfPdrReq.dataTransferHandle));
		offset += sizeof(pfPdrReq.dataTransferHandle);
		payloadPtr[offset] = pfPdrReq.transferOpFlag;
		offset += sizeof(pfPdrReq.transferOpFlag);
		memcpy(payloadPtr + offset, &pfPdrReq.requestCount, sizeof(pfPdrReq.requestCount));
		offset += sizeof(pfPdrReq.requestCount);
		memcpy(payloadPtr + offset, &pfPdrReq.recordChangeNumber, sizeof(pfPdrReq.recordChangeNumber));
		DBG("PLDM Platform: Get PDR payload filled\n");
		offset += sizeof(pfPdrReq.recordChangeNumber);
		payloadPtr[offset] = crc8Smbus(payloadPtr, offset - 1);
		break;
	case PLDM_GET_SENSOR_READING:
		offset = 0;
		memcpy(payloadPtr + offset, &pfSensorReadingReq.sensorId, sizeof(pfSensorReadingReq.sensorId));
		offset += sizeof(pfSensorReadingReq.sensorId);
		payloadPtr[offset] = pfSensorReadingReq.rearmEventState;
		offset += sizeof(pfSensorReadingReq.rearmEventState);
		DBG("PLDM Platform: Get Sensor Reading payload filled\n");
		payloadPtr[offset] = crc8Smbus(payloadPtr, offset - 1);
		break;
	default:
		ERR("PLDM Platform: Unknown command payload fill\n");
		return PLDM_ERROR;
	}
	return PLDM_SUCCESS;
}

/**
 * @brief Sends a PLDM Platform Monitoring and Control command
 *
 * Sends a PLDM command related to platform monitoring and control,
 * such as GetPDRRepositoryInfo, GetPDR, GetSensorReading. Handles multi-part responses
 * for commands like GetPDR.
 *
 * @param[in] cmd The PLDM command code
 * @param[in] size The size of the command payload
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::pfMonCtrlCmd(uint8_t cmd, uint8_t size)
{
	TRACING();

	uint8_t ret = PLDM_SUCCESS;
	uint8_t *rptr = (uint8_t *)mI2cPldmRead;
	uint8_t *wptr = (uint8_t *)mI2cPldmWrite;

	// Exclude first 3 bytes of MCTP header while calculating byte count
	// sizeof(destSlaveAddr) + sizeof(cmdCode) + sizeof(byteCount) = 3 bytes
	uint8_t byteCount = size - 3;
	ret = commandConstruction(&mI2cPldmWrite->mctpSmbusHdr, MCTP_SOM, MCTP_EOM, MCTP_PAK_SEQ, byteCount,
							  MCTP_INTEGRITY_CHECK);
	if (ret != PLDM_SUCCESS) {
		ERR("PFMonCtrl : MCTP command construction failed\n");
		return PLDM_ERROR;
	}
	mI2cPldmWrite->mctpSmbusHdr.msgType = PLDM_OVER_MCTP;
	bool infLoop = (cmd == PLDM_GET_PDR) ? true : false;
	do {
		if (instanceID == PLDM_INSTANCE_ID_MAX) {
			instanceID = 1;
		}
		ret = pldmHdrConstruction(&mI2cPldmWrite->pldmHdr, instanceID, PLDM_PLATFORM_MONITORING, cmd,
								  PLDM_ASYNC_REQUEST_NOTIFY, PLDM_REQUEST);
		if (ret != PLDM_SUCCESS) {
			ERR("PFMonCtrl : PLDM header construction failed\n");
			return PLDM_ERROR;
		}
		ret = pfFillPayload(cmd, size);
		if (ret != PLDM_SUCCESS) {
			ERR("PFMonCtrl : Payload fill failed\n");
			return PLDM_ERROR;
		}
		if (i2cobj->writeAmc(wptr + 1, size) != true) {
			ERR("PFMonCtrl : I2C Write failure\n");
			return PLDM_ERROR;
		}
		DBG("PFMonCtrl TX  :: ");
		hexdump(wptr, size);
		MSLEEP(I2C_EVENT_WAIT_PERIOD_MS);
		if (i2cobj->readAmc(rptr + 1, PLDM_MAX_RESPONSE_SIZE) != true) {
			ERR("PFMonCtrl : I2C Read failure\n");
			return PLDM_ERROR;
		}
		DBG("PFMonCtrl RX  :: ");
		hexdump(rptr, PLDM_MAX_RESPONSE_SIZE);
		if (processPlatformResponse(cmd, instanceID) != PLDM_SUCCESS) {
			return PLDM_ERROR;
		}
		instanceID++;
		if (cmd == PLDM_GET_PDR) {
			pfPdrReq.recordHandle = pfPdrResp.nextRecordHandle;
			pfPdrReq.dataTransferHandle = pfPdrResp.nextDataTransferHandle;

			if (pfPdrResp.transferFlag == PLDM_END || pfPdrResp.transferFlag == PLDM_START_AND_END) {
				infLoop = false;
				mPdrManager.finishPdrRecord();
				// Reset for next PDR
				pfPdrReq.transferOpFlag = PLDM_GET_FIRSTPART;
				pfPdrReq.dataTransferHandle = 0;
			} else {
				pfPdrReq.transferOpFlag = PLDM_GET_NEXTPART;
			}
		}
	} while (infLoop);
	return PLDM_SUCCESS;
}

/**
 * @brief Handles the response payload for Get PDR Repository Info command
 *
 * Parses the response payload for the Get PDR Repository Info command
 * and updates the internal state with repository information.
 *
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::pfPdrRepoRespPayload()
{
	DBG("Handling PDR Repository Info response\n");
	if (mI2cPldmRead->respPayload[0] != PLDM_SUCCESS) {
		ERR("PLDM Platform: Get PDR Repository Info command failed with code 0x%02x\n", mI2cPldmRead->respPayload[0]);
		return PLDM_ERROR;
	}
	memcpy(&pfPdrRepoInfo, &mI2cPldmRead->respPayload, sizeof(pdrRepositoryInfoResp));
	DBG("PDR Repository Info - Repository Size: %u, Record Count: %u\n", pfPdrRepoInfo.repositorySize,
		pfPdrRepoInfo.recordCount);
	if (pfPdrRepoInfo.repositorySize == 0 || pfPdrRepoInfo.recordCount == 0) {
		ERR("PLDM Platform: Invalid PDR Repository Info received\n");
		return PLDM_ERROR;
	}
	if (pfPdrRepoInfo.repositoryState != PLDM_PDR_REPO_AVAILABLE) {
		DBG("PLDM Platform: PDR Repository not available, state: 0x%02x\n", pfPdrRepoInfo.repositoryState);
		return PLDM_ERROR;
	}
	return PLDM_SUCCESS;
}

/**
 * @brief Handles the response payload for Get PDR command
 *
 * Parses the response payload for the Get PDR command
 * and appends the received PDR data to the PDR manager.
 *
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::pfPdrRespPayload()
{
	DBG("Handling PDR response\n");
	if (mI2cPldmRead->respPayload[0] != PLDM_SUCCESS) {
		ERR("PLDM Platform: Get PDR command failed with code 0x%02x\n", mI2cPldmRead->respPayload[0]);
		return PLDM_ERROR;
	}

	pdrRespPayload *resp = (pdrRespPayload *)mI2cPldmRead->respPayload;

	pfPdrResp.completionCode = resp->completionCode;
	pfPdrResp.nextRecordHandle = resp->nextRecordHandle;
	pfPdrResp.nextDataTransferHandle = resp->nextDataTransferHandle;
	pfPdrResp.transferFlag = resp->transferFlag;
	pfPdrResp.responseCount = resp->responseCount;

	uint16_t respCount = resp->responseCount;
	mPdrManager.appendPdrData(resp->recordData, respCount);

	return PLDM_SUCCESS;
}

/**
 * @brief Handles the response payload for Get Sensor Reading command
 *
 * Parses the response payload for the Get Sensor Reading command
 * and updates the internal state with the sensor reading.
 *
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::pfSensorReadingRespPayload()
{
	DBG("Handling Sensor Reading response\n");
	if (mI2cPldmRead->respPayload[0] != PLDM_SUCCESS) {
		ERR("PLDM Platform: Get Sensor Reading command failed with code 0x%02x\n", mI2cPldmRead->respPayload[0]);
		return PLDM_ERROR;
	}

	pldmGetSensorReadingResp *resp = (pldmGetSensorReadingResp *)mI2cPldmRead->respPayload;

	pfSensorReadingResp.completionCode = resp->completionCode;
	pfSensorReadingResp.sensorDataSize = resp->sensorDataSize;
	pfSensorReadingResp.sensorOperationalState = resp->sensorOperationalState;
	pfSensorReadingResp.sensorEventMessageEnable = resp->sensorEventMessageEnable;
	pfSensorReadingResp.presentState = resp->presentState;
	pfSensorReadingResp.previousState = resp->previousState;
	pfSensorReadingResp.eventState = resp->eventState;

	// Extract raw value based on sensorDataSize
	uint8_t *valPtr = resp->presentReading;
	mSensorReading.sensorDataSize = resp->sensorDataSize;

	switch (mSensorReading.sensorDataSize) {
	case PLDM_SENSOR_DATA_SIZE_UINT8:
		mSensorReading.data.value_u8 = *valPtr;
		break;
	case PLDM_SENSOR_DATA_SIZE_SINT8:
		mSensorReading.data.value_s8 = (int8_t)*valPtr;
		break;
	case PLDM_SENSOR_DATA_SIZE_UINT16:
		mSensorReading.data.value_u16 = *(uint16_t *)valPtr;
		break;
	case PLDM_SENSOR_DATA_SIZE_SINT16:
		mSensorReading.data.value_s16 = *(int16_t *)valPtr;
		break;
	case PLDM_SENSOR_DATA_SIZE_UINT32:
		mSensorReading.data.value_u32 = *(uint32_t *)valPtr;
		break;
	case PLDM_SENSOR_DATA_SIZE_SINT32:
		mSensorReading.data.value_s32 = *(int32_t *)valPtr;
		break;
	default:
		return PLDM_ERROR;
	}
	return PLDM_SUCCESS;
}

/**
 * @brief Processes the response for PLDM Platform commands
 *
 * Routes the response processing to the appropriate handler based on the command code.
 *
 * @param[in] cmd The PLDM command code
 * @param[in] id The instance ID of the command
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::processPlatformResponse(uint8_t cmd, uint8_t id)
{
	TRACING();
	switch (cmd) {
	case PLDM_GET_PDR_REPOSITORY_INFO:
		return pfPdrRepoRespPayload();
	case PLDM_GET_PDR:
		return pfPdrRespPayload();
	case PLDM_GET_SENSOR_READING:
		return pfSensorReadingRespPayload();
	default:
		ERR("PLDM Platform: Unknown command response\n");
		return PLDM_ERROR;
	}
	UNUSED_VAR(id);
	return PLDM_ERROR;
}

/**
 * @brief Sends the Get PDR Repository Info command
 *
 * Initiates the Get PDR Repository Info command to retrieve information
 * about the PDR repository.
 *
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::pfGetPdrRepositoryInfo()
{
	TRACING();
	uint8_t cmd = PLDM_GET_PDR_REPOSITORY_INFO;
	uint8_t size = sizeof(mctpSmbusI2cHdr) + sizeof(pldmHdr);

	if (pfMonCtrlCmd(cmd, size) != 0) {
		ERR("Failed to send PFMonCtrl command\n");
		return PLDM_ERROR;
	}

	return PLDM_SUCCESS;
}

/**
 * @brief Retrieves all PDRs from the PDR repository
 *
 * Sends repeated Get PDR commands to retrieve all PDRs from the repository
 * and stores them in the PDR manager.
 *
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::pfGetTotalPdrs()
{
	TRACING();
	mPdrManager.clear();
	uint8_t cmd = PLDM_GET_PDR;
	// Total bytes to write for GetPDR command
	uint8_t size = sizeof(mctpSmbusI2cHdr) + sizeof(pldmHdr) + sizeof(pdrReqPayload);
	pfPdrReq.recordHandle = 0;
	pfPdrReq.dataTransferHandle = 0;
	pfPdrReq.transferOpFlag = PLDM_GET_FIRSTPART;
	pfPdrReq.requestCount = static_cast<uint16_t>(pfPdrRepoInfo.largestRecordSize);
	pfPdrReq.recordChangeNumber = 0;
	for (uint32_t i = 0; i < pfPdrRepoInfo.recordCount; i++) {
		pfMonCtrlCmd(cmd, size);
	}
	return PLDM_SUCCESS;
}

/**
 * @brief Retrieves and processes the sensor value for a given sensor PDR
 *
 * Sends the Get Sensor Reading command for the specified sensor PDR
 * and processes the returned sensor value.
 *
 * @param[in] sensor Pointer to the numeric sensor PDR
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::pfGetSensorValue(const pldmNumericSensorValuePdr *sensor)
{
	TRACING();
	uint8_t cmd = PLDM_GET_SENSOR_READING;
	uint8_t size = sizeof(mctpSmbusI2cHdr) + sizeof(pldmHdr) + sizeof(pldmGetSensorReadingReq);

	pfSensorReadingReq.sensorId = sensor->sensorId;
	pfSensorReadingReq.rearmEventState = 0;

	if (pfMonCtrlCmd(cmd, size) != 0) {
		ERR("Failed to send PFMonCtrl command\n");
		return PLDM_ERROR;
	}

	auto optValue = mPdrManager.convertReading(sensor, mSensorReading);
	if (!optValue.has_value()) {
		ERR("Failed to convert sensor reading for Sensor ID: %u\n", sensor->sensorId);
		return PLDM_ERROR;
	}
	double value = optValue.value();
	mSensorInfoList.push_back(
		{sensor->sensorId, sensor->entityType, sensor->entityInstanceNum, sensor->containerId, value});
	return PLDM_SUCCESS;
}

/**
 * @brief Retrieves sensor values by sensor ID
 *
 * Searches for the sensor PDR with the specified sensor ID
 * and retrieves its value.
 *
 * @param[in] sensorId The ID of the sensor to retrieve
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::pfGetSensorValuesById(uint16_t sensorId)
{
	TRACING();

	for (const auto &record : mPdrManager.getPdrRecords()) {
		const pldmNumericSensorValuePdr *sensorPdr =
			reinterpret_cast<const pldmNumericSensorValuePdr *>(record.data.data());
		if (sensorPdr->sensorId != sensorId) {
			continue;
		}
		return pfGetSensorValue(sensorPdr);
	}

	return PLDM_ERROR;
}

/**
 * @brief Retrieves sensor values by sensor unit type
 *
 * Searches for all sensor PDRs with the specified unit type
 * and retrieves their values.
 *
 * @param[in] unit The sensor unit type to filter by
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::pfGetSensorValuesByUnit(sensorUnits unit)
{
	TRACING();
	const auto &sensorRecords = mPdrManager.getSensorRecords(unit);
	if (sensorRecords.empty()) {
		ERR("No sensors found in PDRs for the specified unit\n");
		return PLDM_ERROR;
	}
	for (const auto *record : sensorRecords) {
		DBG("Found Sensor Handle: %u\n", record->header.recordHandle);
		const pldmNumericSensorValuePdr *sensor =
			reinterpret_cast<const pldmNumericSensorValuePdr *>(record->data.data());

		if (pfGetSensorValue(sensor) != PLDM_SUCCESS) {
			ERR("Failed to get sensor value for handle: %u\n", record->header.recordHandle);
			continue;
		}
	}
	return PLDM_SUCCESS;
}

/**
 * @brief Initializes the PLDM Platform Monitoring and Control
 *
 * Retrieves PDR repository information and total PDRs
 * to prepare for sensor monitoring and control operations.
 *
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::pfMonCtrlInitialize()
{
	TRACING();
	if (pfGetPdrRepositoryInfo() != PLDM_SUCCESS) {
		ERR("Failed to get PDR Repository Info\n");
		return PLDM_ERROR;
	}

	if (pfGetTotalPdrs() != PLDM_SUCCESS) {
		ERR("Failed to get total PDRs\n");
		return PLDM_ERROR;
	}

	return PLDM_SUCCESS;
}

/**
 * @brief Retrieves sensor information by sensor ID
 *
 * Initializes the platform monitoring and control,
 * then retrieves the sensor value for the specified sensor ID.
 *
 * @param[in] sensorId The ID of the sensor to retrieve
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::getSensorInfo(uint16_t sensorId)
{
	TRACING();

	uint8_t ret = pfMonCtrlInitialize();
	if (ret != PLDM_SUCCESS) {
		ERR("Failed to initialize PFMonCtrl\n");
		return PLDM_ERROR;
	}
	pfBuildPdrSensorCache();
	ret = pfGetSensorValuesById(sensorId);
	if (ret != PLDM_SUCCESS) {
		ERR("Failed to get sensor values\n");
		return PLDM_ERROR;
	}
	return PLDM_SUCCESS;
}