/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef __PLDM_H
#define __PLDM_H

#include "mctp.h"
#include "pldm_constants.h"
#include "pldm_file_transfer.h"
#include "pldm_fru.h"
#include "pldm_fwpackage.h"
#include "pldm_fwupdate.h"
#include "pldm_platform.h"
#include "pldm_pdr_manager.h"
#include "pldm_amc_gpu_reset.h"
#include <i2c_interface.h>
#include <mutex>

// Transfer Operation Flags
enum pldmTransferOpFlag
{
	PLDM_GET_FIRSTPART = 0x01,
	PLDM_GET_NEXTPART = 0x00
};

enum pldmFileTransferOpFlag
{
	PLDM_XFER_FIRST_PART = 0x00,
	PLDM_XFER_NEXT_PART = 0x01,
	PLDM_XFER_ABORT = 0x02,
	PLDM_XFER_COMPLETE = 0x03,
	PLDM_XFER_CURRENT_PART = 0x04
};

// Transfer Flags
enum pldmTransferFlag
{
	PLDM_START = 0x01,
	PLDM_MIDDLE = 0x02,
	PLDM_END = 0x04,
	PLDM_START_AND_END = 0x05
};

#pragma pack(push, 1)
struct pldmHdr
{
	// Byte1
	uint8_t instanceID : 5;
	uint8_t reserved : 1;
	uint8_t datagram : 1;
	uint8_t request : 1;

	// Byte2
	uint8_t cmdType : 6;
	uint8_t headerVer : 2;

	// Byte3
	uint8_t cmdCode;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pldmVersionInfo
{
	uint8_t type;
	uint8_t major;
	uint8_t minor;
	uint8_t update;
	uint8_t alpha;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pldmResponseData
{
	uint8_t supportedTypes;
	uint8_t totalSupportedTypes;
	uint8_t tid;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct fwuRequestUpdate
{
	uint32_t maxTransferSize;
	uint16_t numComp;
	uint8_t maxOutstandXferReqs;
	uint16_t pkgDataLen;
	uint8_t compImgSetVerStrType;
	uint8_t compImgSetVerStrLen;
	uint8_t compImgSetVerStr[PLDM_FWU_COMP_VER_STR_SIZE_MAX];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct fwuPassCompTable
{
	uint8_t xferFlag;
	uint16_t compClass;
	uint16_t id;
	uint8_t compClassIdx;
	uint32_t comparisonStamp;
	uint8_t verStrType;
	uint8_t verStrLen;
	uint8_t verStr[PLDM_FWU_COMP_VER_STR_SIZE_MAX];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct fwUpdComp
{
	uint16_t compClass;
	uint16_t id;
	uint8_t compClassIdx;
	uint32_t comparisonStamp;
	uint32_t componentimagesize;
	uint32_t updateoptionflags;
	uint8_t verStrType;
	uint8_t verStrLen;
	uint8_t verStr[PLDM_FWU_COMP_VER_STR_SIZE_MAX];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct compParseData
{
	uint32_t locOffset;
	uint32_t size;
};
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct i2cdata
{
	struct mctpSmbusI2cHdr mctpSmbusHdr;
	struct pldmHdr pldmHdr;
	uint8_t respPayload[128];
} i2cdataPldmInfo;
#pragma pack(pop)

class pldm : public mctp
{
private:
	PdrManager mPdrManager;
	struct pldmResponseData mPldmRespInfo;
	struct pldmVersionInfo versionInfo[8];
	i2cdataPldmInfo *mI2cPldmRead, *mI2cPldmWrite;
	std::mutex *progMutex;

	uint8_t mFwuCmdLen;
	uint8_t instanceID;
	bool mI2cMultiResp;
	int mCardNum;

	// pldm Firmware Update datastructures
	struct fwuRequestUpdate mReqUpdate;
	struct fwuPassCompTable mPassCompTable;
	struct fwUpdComp mUpdComp;
	uint8_t mFwuCurrentState;

	// PLDM FRU datastructures
	struct fruGetTableRequest mFruTableRequest;
	struct fruTableResponse mFruTableResponse;
	struct fruTableMetadata mFruMetadata;
	struct fruTable mFruTable;
	uint16_t mFruCurrentDataLength;
	bool mFruTableInitialized;

	// PLDM Platform datastructures
	struct pdrRepositoryInfoResp pfPdrRepoInfo;
	struct pdrReqPayload pfPdrReq;
	struct pdrRespPayload pfPdrResp;
	struct pldmGetSensorReadingReq pfSensorReadingReq;
	struct pldmGetSensorReadingResp pfSensorReadingResp;
	sensorReadingValue mSensorReading;
	std::vector<pldmSensorInfo> mSensorInfoList;

	// PLDM File Transfer datastructures
	struct pldm_file_df_open_req mDfOpenReq;
	struct pldm_file_df_open_resp mDfOpenResp;
	struct pldm_file_df_close_req mDfCloseReq;
	struct pldm_file_df_close_resp mDfCloseResp;
	struct pldm_file_df_heartbeat_req mDfHeartbeatReq;
	struct pldm_file_df_heartbeat_resp mDfHeartbeatResp;
	struct pldm_file_df_read_req mDfReadReq;
	struct pldm_file_df_read_resp mDfReadResp;
	struct pldm_base_multipart_receive_req mMultipartReceiveReq;
	struct pldm_base_multipart_receive_resp mMultipartReceiveResp;
	std::vector<uint8_t> mMultipartReceiveData;
	std::vector<uint8_t> mRxAssembledFrame;
	std::vector<uint8_t> mRxAssembledPayload;

	// pldm Base APIs
	int pldminit();
	void cleanup();
	uint8_t pldmHdrConstruction(struct pldmHdr *pldmHdr, uint8_t instanceID, uint8_t cmdType, uint8_t cmd,
								uint8_t async, uint8_t reqresp);

	//============== pldm Discovery ================
	// pldm Discovery Command
	uint8_t pldmDiscInitialize();
	uint8_t discoveryCmd(uint8_t cmd, uint8_t size, uint8_t pldmVersionType);
	uint8_t pldmDiscoveryFillPayload(uint8_t cmd, uint8_t pldmCmdLen);
	void pldmDiscoveryGetVersionPayload(uint8_t msg_type, uint8_t pldmCmdLen);
	void pldmDiscoveryGetCmdPayload(uint8_t msg_type, uint8_t pldmCmdLen);

	// pldm Discovery Response
	uint8_t discoveryResponse(uint8_t cmd, uint8_t id, uint8_t pldmVersionType);
	uint8_t discoveryResponsePayload(uint8_t cmd, uint8_t id);
	uint8_t getbitposition(uint8_t num);

	//============== pldm Firmware Update ===========
	// pldm FwPackage Parse
	FILE *mCompFp;
	struct fwPkg *pkg;
	struct compParseData *mCompParseData;
	uint8_t mCurComp;

	uint8_t fwpkgParseInfo(const char *pkgFilePath);
	uint8_t dumpPldmFwpkgInfo();

	// pldm FwUpdate Command
	uint8_t fwUpdInitialize(const char *pkgFilePath);
	uint8_t fwUpdCmd(uint8_t cmd, uint8_t size);
	uint8_t pldmFwUpdFillPayload(uint8_t cmd, uint8_t size);

	// pldm FwUpdate Command Helper
	uint8_t fwReqUpdate();
	uint8_t fwupdPassCompTable();
	uint8_t fwUpdComp();

	// pldm FwUpdate Response
	uint8_t fwUpdateResp(uint8_t cmd, uint8_t id);
	uint8_t pldmFwUpdateRespPayload(uint8_t cmd, uint8_t id);
	uint8_t pldmFwGetParamPayload(uint8_t cmd, uint8_t id);
	uint8_t readFromFD();
	uint8_t writeToAMCCompXfer(uint32_t offset, uint32_t lenToSend, uint8_t completioncode);
	uint8_t respondtoamc(uint8_t completioncode);

	// pldm OEM VRSync API's
	uint8_t oemVrsyncResp(uint8_t cmd, uint8_t id);
	uint8_t oemVrsyncRespPayload(uint8_t cmd, uint8_t id);

	//============== PLDM FRU ====================
	// PLDM FRU Command
	uint8_t pldmFruInitialize();
	uint8_t pldmFruCommand(uint8_t cmd, uint8_t size);
	uint8_t pldmFruFillPayload(uint8_t cmd, uint8_t size);
	uint8_t getFruRecordTableMetadata();
	uint8_t getFruRecordTable(uint32_t dataTransferHandle, uint8_t transferOperationFlag);

	// PLDM FRU Response
	uint8_t pldmFruResponse(uint8_t cmd, uint8_t id);
	uint8_t pldmFruResponsePayload(uint8_t cmd, uint8_t id);

	// PLDM FRU Helper
	bool parseTimestamp104(const uint8_t *timestampData, uint8_t fieldLength, timestamp104_t *timestamp);
	uint8_t parseFruTable(const uint8_t *fruData, size_t dataLength);
	uint8_t parseFruField(const uint8_t *fieldData, uint8_t fieldType, uint8_t recordType, uint8_t encodingType,
						  uint8_t fieldLength);
	void clearFruTableData();

	// PLDM Platform APIs
	uint8_t pfMonCtrlInitialize();
	uint8_t pfMonCtrlCmd(uint8_t cmd, uint8_t size);
	uint8_t processPlatformResponse(uint8_t cmd, uint8_t id);
	uint8_t pfGetPdrRepositoryInfo();
	uint8_t pfFillPayload(uint8_t cmd, uint8_t size);
	uint8_t pfPdrRepoRespPayload();
	uint8_t pfPdrRespPayload();
	uint8_t pfSensorReadingRespPayload();
	uint8_t pfGetTotalPdrs();
	uint8_t pfGetSensorValuesByUnit(sensorUnits unit);
	uint8_t pfGetSensorValuesById(uint16_t sensorId);
	uint8_t pfGetSensorValue(const pldmNumericSensorValuePdr *sensor);
	void pfBuildPdrSensorCache() { mPdrManager.buildSensorCache(); }

	// PLDM File Transfer APIs
	uint8_t pldmFileTransferCmd(uint8_t cmd, uint8_t size);
	uint8_t PldmFillFileTxPayload(uint8_t cmd, uint8_t size);
	uint8_t pldmFileTransferResp(uint8_t cmd, uint8_t id);
	uint8_t pldmFileTransferRespPayload(const uint8_t *respPayload, size_t respPayloadLen, const uint8_t cmd,
										const uint8_t id);
	uint8_t handleDfOpenResp(const uint8_t *respPayload, size_t respPayloadLen);
	uint8_t handleDfCloseResp(const uint8_t *respPayload, size_t respPayloadLen);
	uint8_t handleDfHeartbeatResp(const uint8_t *respPayload, size_t respPayloadLen);
	uint8_t handleDfReadResp(const uint8_t *respPayload, size_t respPayloadLen, size_t totalFrameSize);
	uint8_t handleMultipartReceiveResp(const uint8_t *respPayload, size_t respPayloadLen, size_t totalFrameSize);
	uint8_t pldmDfOpenCommand(uint16_t fileIdentifier, uint16_t &fileDescriptor);
	uint8_t pldmDfCloseCommand(uint16_t fileDescriptor);
	uint8_t pldmMultipartReceiveCommand(uint16_t fileDescriptor, std::vector<uint8_t> &fileData);
	const PdrRecord *getFilePdrById(uint16_t filePdrId);

public:
	pldm(const std::string &devpath, int cardnum)
		: mctp(devpath), mI2cPldmRead(nullptr), mI2cPldmWrite(nullptr), progMutex(nullptr), instanceID(1),
		  mI2cMultiResp(false), mCardNum(cardnum), mFruTableInitialized(false)
	{
		pldminit();
	}
	uint8_t getFruSerialNum(char *serialNumber, size_t *bufferSize);
	uint8_t getAmcVersion(char *version, size_t *bufferSize);
	~pldm() { cleanup(); }

	// pldm Base APIs
	int initialize();
	int fwupd(const char *pkgFilePath);
	uint8_t getSensorInfoById(uint16_t sensorId);
	uint8_t getSensorInfoByUnit(sensorUnits unit);
	uint8_t getFile(uint16_t filePdrId, std::vector<uint8_t> &fileData);
	std::vector<pldmSensorInfo> &getSensorInfoList() { return mSensorInfoList; }
	uint8_t oemVrsyncCmd(uint8_t cmd);
	int fwupdProgress() { return mProgPercent; }
	int mProgPercent;
	uint8_t amcGpuReset();
};

#endif // __PLDM_H
