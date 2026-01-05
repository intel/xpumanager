/*
 * Copyright © 2026 Intel Corporation
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

#ifndef __PLDM_PLATFORM_H
#define __PLDM_PLATFORM_H

#include <stdint.h>
#include "pldm_types.h"

// DSP0248 Command Codes
enum pldmPlatformCommandCode
{
	PLDM_GET_TERMINUS_UID = 0x03,
	PLDM_SET_NUMERIC_SENSOR_ENABLE = 0x10,
	PLDM_GET_SENSOR_READING = 0x11,
	PLDM_SET_STATE_SENSOR_ENABLE = 0x20,
	PLDM_GET_STATE_SENSOR_READINGS = 0x21,
	PLDM_SET_NUMERIC_EFFECTER_ENABLE = 0x30,
	PLDM_SET_NUMERIC_EFFECTER_VALUE = 0x31,
	PLDM_GET_NUMERIC_EFFECTER_VALUE = 0x32,
	PLDM_SET_STATE_EFFECTER_ENABLE = 0x38,
	PLDM_SET_STATE_EFFECTER_STATES = 0x39,
	PLDM_GET_STATE_EFFECTER_STATES = 0x3A,
	PLDM_GET_PDR_REPOSITORY_INFO = 0x50,
	PLDM_GET_PDR = 0x51,
	PLDM_PLATFORM_EVENT_MESSAGE = 0x0A
};

// PLDM Completion Codes (Common + Platform Specific)
enum pldmPlatformCompletionCode
{
	PLDM_PLATFORM_INVALID_SENSOR_ID = 0x80,
	PLDM_PLATFORM_REARM_UNAVAILABLE_IN_PRESENT_STATE = 0x81,

	PLDM_PLATFORM_INVALID_EFFECTER_ID = 0x80,
	PLDM_PLATFORM_INVALID_STATE_VALUE = 0x81,

	PLDM_PLATFORM_INVALID_DATA_TRANSFER_HANDLE = 0x80,
	PLDM_PLATFORM_INVALID_TRANSFER_OPERATION_FLAG = 0x81,
	PLDM_PLATFORM_INVALID_RECORD_HANDLE = 0x82,
	PLDM_PLATFORM_INVALID_RECORD_CHANGE_NUMBER = 0x83,
	PLDM_PLATFORM_TRANSFER_TIMEOUT = 0x84,

	PLDM_PLATFORM_SET_EFFECTER_UNSUPPORTED_SENSORSTATE = 0x82,
};

// Sensor Operational State
enum pldmSensorOpState
{
	PLDM_SENSOR_ENABLED = 0x00,
	PLDM_SENSOR_DISABLED = 0x01,
	PLDM_SENSOR_UNAVAILABLE = 0x02,
	PLDM_SENSOR_STATUS_UNKNOWN = 0x03,
	PLDM_SENSOR_FAILED = 0x04,
	PLDM_SENSOR_INITIALIZING = 0x05,
	PLDM_SENSOR_SHUTDOWN = 0x06,
	PLDM_SENSOR_IN_TEST = 0x07
};

// Sensor Data Size
enum pldmSensorDataSize
{
	PLDM_SENSOR_DATA_SIZE_UINT8 = 0x00,
	PLDM_SENSOR_DATA_SIZE_SINT8 = 0x01,
	PLDM_SENSOR_DATA_SIZE_UINT16 = 0x02,
	PLDM_SENSOR_DATA_SIZE_SINT16 = 0x03,
	PLDM_SENSOR_DATA_SIZE_UINT32 = 0x04,
	PLDM_SENSOR_DATA_SIZE_SINT32 = 0x05
};

// Effecter Operational State
enum pldmEffecterOpState
{
	PLDM_EFFECTER_ENABLED_UPDATEPENDING = 0x00,
	PLDM_EFFECTER_ENABLED_NOUPDATEPENDING = 0x01,
	PLDM_EFFECTER_DISABLED = 0x02,
	PLDM_EFFECTER_UNAVAILABLE = 0x03,
	PLDM_EFFECTER_STATUS_UNKNOWN = 0x04,
	PLDM_EFFECTER_FAILED = 0x05,
	PLDM_EFFECTER_INITIALIZING = 0x06,
	PLDM_EFFECTER_SHUTDOWN = 0x07,
	PLDM_EFFECTER_IN_TEST = 0x08
};

// PDR Repository State
enum pldmPdrRepositoryState
{
	PLDM_PDR_REPO_AVAILABLE = 0x00,
	PLDM_PDR_REPO_UPDATE_IN_PROGRESS = 0x01,
	PLDM_PDR_REPO_FAILED = 0x02
};

// PDR Types
enum pldmPdrType
{
	PLDM_TERMINUS_LOCATOR_PDR = 1,
	PLDM_NUMERIC_SENSOR_PDR = 2,
	PLDM_NUMERIC_EFFECTER_PDR = 3,
	PLDM_STATE_SENSOR_PDR = 4,
	PLDM_STATE_EFFECTER_PDR = 5,
	PLDM_SENSOR_AUXILIARY_NAMES_PDR = 6,
	PLDM_EFFECTER_AUXILIARY_NAMES_PDR = 7,
	PLDM_PDR_FRU_RECORD_SET = 20,
	PLDM_PDR_ENTITY_ASSOCIATION = 21
};

enum sensorUnits
{
	PLDM_UNIT_UNSPECIFIED = 0x01,
	PLDM_UNIT_DEGREES_C = 0x02,
	PLDM_UNIT_VOLTS = 0x05,
	PLDM_UNIT_CURRENT_AMPS = 0x06,
	PLDM_UNIT_WATTS = 0x07,
	PLDM_UNIT_JOULES = 0x08,
	PLDM_UNIT_COULOMBS = 0x09,
};

struct pldmSensorInfo
{
	uint16_t sensorId;
	uint16_t entityType;
	uint16_t entityInstanceNum;
	uint16_t containerId;
	double reading;
};

// ============================================================================
// Request/Response Structures
// ============================================================================

#pragma pack(push, 1)
// ----------------------------------------------------------------------------
// GetPDRRepositoryInfo (0x50)
// ----------------------------------------------------------------------------
struct pdrRepositoryInfoResp
{
	uint8_t completionCode;
	uint8_t repositoryState;
	timestamp104_t updateTime;
	timestamp104_t oemUpdateTime;
	uint32_t recordCount;
	uint32_t repositorySize;
	uint32_t largestRecordSize;
	uint8_t dataTransferHandleTimeout;
};

// ----------------------------------------------------------------------------
// GetPDR (0x51)
// ----------------------------------------------------------------------------
struct pdrReqPayload
{
	uint32_t recordHandle;
	uint32_t dataTransferHandle;
	uint8_t transferOpFlag;
	uint16_t requestCount;
	uint16_t recordChangeNumber;
};

struct pdrRespPayload
{
	uint8_t completionCode;
	uint32_t nextRecordHandle;
	uint32_t nextDataTransferHandle;
	uint8_t transferFlag;
	uint16_t responseCount;
	uint8_t recordData[1]; // Variable length PDR data
};

// Common PDR Header
struct pdrPayloadHeader
{
	uint32_t recordHandle;
	uint8_t version;
	uint8_t type;
	uint16_t recordChangeNumber;
	uint16_t dataLength;
};

// ----------------------------------------------------------------------------
// SetNumericSensorEnable (0x10)
// ----------------------------------------------------------------------------
struct pldmSetNumericSensorEnableReq
{
	uint16_t sensorId;
	uint8_t sensorOpState;
	uint8_t sensorEventMessageEnable;
};

struct pldmSetNumericSensorEnableResp
{
	uint8_t completionCode;
};

// ----------------------------------------------------------------------------
// GetSensorReading (0x11)
// ----------------------------------------------------------------------------
struct pldmGetSensorReadingReq
{
	uint16_t sensorId;
	uint8_t rearmEventState;
};

struct pldmGetSensorReadingResp
{
	uint8_t completionCode;
	uint8_t sensorDataSize;
	uint8_t sensorOperationalState;
	uint8_t sensorEventMessageEnable;
	uint8_t presentState;
	uint8_t previousState;
	uint8_t eventState;
	uint8_t presentReading[1]; // Variable size based on sensorDataSize
};

typedef union
{
	uint8_t value_u8;
	int8_t value_s8;
	uint16_t value_u16;
	int16_t value_s16;
	uint32_t value_u32;
	int32_t value_s32;
} union_effecter_data_size;

typedef union
{
	uint8_t value_u8;
	int8_t value_s8;
	uint16_t value_u16;
	int16_t value_s16;
	uint32_t value_u32;
	int32_t value_s32;
	real32_t value_f32;
} union_range_field_format;

typedef union
{
	uint8_t value_u8;
	int8_t value_s8;
	uint16_t value_u16;
	int16_t value_s16;
	uint32_t value_u32;
	int32_t value_s32;
} union_sensor_data_size;

struct pldmNumericEffecterValuePdr
{
	struct pdrPayloadHeader hdr;
	uint16_t terminusHandle;
	uint16_t effecterId;
	uint16_t entityType;
	uint16_t entityInstance;
	uint16_t containerId;
	uint16_t effecterSemanticId;
	uint8_t effecterInit;
	bool8_t effecterAuxiliaryNames;
	uint8_t baseUnit;
	int8_t unitModifier;
	uint8_t rateUnit;
	uint8_t baseOemUnitHandle;
	uint8_t auxUnit;
	int8_t auxUnitModifier;
	uint8_t auxRateUnit;
	uint8_t auxOemUnitHandle;
	bool8_t isLinear;
	uint8_t effecterDataSize;
	real32_t resolution;
	real32_t offset;
	uint16_t accuracy;
	uint8_t plusTolerance;
	uint8_t minusTolerance;
	real32_t stateTransitionInterval;
	real32_t transitionInterval;
	union_effecter_data_size maxSettable;
	union_effecter_data_size minSettable;
	uint8_t rangeFieldFormat;
	bitfield8_t rangeFieldSupport;
	union_range_field_format nominalValue;
	union_range_field_format normalMax;
	union_range_field_format normalMin;
	union_range_field_format ratedMax;
	union_range_field_format ratedMin;
};

struct pldmNumericSensorValuePdr
{
	struct pdrPayloadHeader hdr;
	uint16_t terminusHandle;
	uint16_t sensorId;
	uint16_t entityType;
	uint16_t entityInstanceNum;
	uint16_t containerId;
	uint8_t sensorInit;
	bool8_t sensorAuxiliaryNamesPdr;
	uint8_t baseUnit;
	int8_t unitModifier;
	uint8_t rateUnit;
	uint8_t baseOemUnitHandle;
	uint8_t auxUnit;
	int8_t auxUnitModifier;
	uint8_t auxRateUnit;
	uint8_t rel;
	uint8_t auxOemUnitHandle;
	bool8_t isLinear;
	uint8_t sensorDataSize;
	real32_t resolution;
	real32_t offset;
	uint16_t accuracy;
	uint8_t plusTolerance;
	uint8_t minusTolerance;
	union_sensor_data_size hysteresis;
	bitfield8_t supportedThresholds;
	bitfield8_t thresholdAndHysteresisVolatility;
	real32_t stateTransitionInterval;
	real32_t updateInterval;
	union_sensor_data_size maxReadable;
	union_sensor_data_size minReadable;
	uint8_t rangeFieldFormat;
	bitfield8_t rangeFieldSupport;
	union_range_field_format nominalValue;
	union_range_field_format normalMax;
	union_range_field_format normalMin;
	union_range_field_format warningHigh;
	union_range_field_format warningLow;
	union_range_field_format criticalHigh;
	union_range_field_format criticalLow;
	union_range_field_format fatalHigh;
	union_range_field_format fatalLow;
};

#pragma pack(pop)
#endif /* __PLDM_PLATFORM_H */
