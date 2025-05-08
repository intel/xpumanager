#ifndef _FWUPD_H_
#define _FWUPD_H_

#include <zes_api.h>
#include <string>
#include <os.h>

using namespace std;

enum fwupdPreference
{
	FWUPD_PREFERENCE_GSC,
	FWUPD_PREFERENCE_SYSMAN,
	FWUPD_PREFERENCE_AMC,
};

struct firmwareInfo
{
	bool jsonOutput;
	bool assumeYes;
	bool forceUpdate;
	bool recoveryMode;
	string deviceId;
	string firmwareType;
	string filePath;
	string username;
	string password;
	ze_device_handle_t deviceHdl;
	fwupdPreference preference;
};

class fwupd
{

public:
	fwupd() {}
	virtual ~fwupd() {}
	virtual ze_result_t updateAMC(firmwareInfo* fwInfo) { UNUSED(fwInfo);  return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateGfx(firmwareInfo *fwInfo) { UNUSED(fwInfo); return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateGfxData(firmwareInfo *fwInfo) { UNUSED(fwInfo); return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateGfxCodeData(firmwareInfo *fwInfo) { UNUSED(fwInfo); return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateGfxPscBin(firmwareInfo *fwInfo) { UNUSED(fwInfo); return ZE_RESULT_SUCCESS; };
};

#endif