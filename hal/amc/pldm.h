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

#ifndef __PLDM_H
#define __PLDM_H

#include "mctp.h"
#include "pldm_constants.h"
#include "pldm_fwpackage.h"
#include "pldm_fwupdate.h"
#include <i2c_interface.h>
#include <mutex>

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
struct _pldm_version_info
{
	uint8_t type;
	uint8_t major;
	uint8_t minor;
	uint8_t update;
	uint8_t alpha;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct _pldm_response_data
{
	uint8_t supportedTypes;
	uint8_t totalSupportedTypes;
	uint8_t tid;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct _fwu_requestupdate
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
} i2cdata_pldminfo;
#pragma pack(pop)

class pldm : public mctp
{
private:
	struct _pldm_response_data mPldmRespInfo;
	struct _pldm_version_info versionInfo[8];
	i2cdata_pldminfo *mI2cPldmRead, *mI2cPldmWrite;
	std::mutex *progMutex;

	uint8_t mFwuCmdLen;
	uint8_t instanceID;
	bool mI2cMultiResp;
	int mCardNum;

	// pldm Firmware Update datastructures
	struct _fwu_requestupdate mReqUpdate;
	struct fwuPassCompTable mPassCompTable;
	struct fwUpdComp mUpdComp;
	uint8_t m_fwu_current_state;

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
	struct compParseData *m_comp_parse_data;
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

public:
	pldm(const std::string &devpath, int cardnum) : mctp(devpath), mCardNum(cardnum) { pldminit(); }
	~pldm() { cleanup(); }

	// pldm Base APIs
	int initialize();
	int fwupd(const char *pkgFilePath);
	uint8_t oemVrsyncCmd(uint8_t cmd);
	int fwupdProgress() { return mProgPercent; }
	int mProgPercent;
};

#endif // __PLDM_H
