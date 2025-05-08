#ifndef _SYSMANUPD_H
#define _SYSMANUPD_H

#include "fwupd.h"

class sysmanupd : public fwupd
{
public:
	sysmanupd() {}
	~sysmanupd() {}
	ze_result_t updateGfx(firmwareInfo *fwInfo);
	ze_result_t updateGfxData(firmwareInfo *fwInfo);
	ze_result_t updateGfxCodeData(firmwareInfo *fwInfo);
	ze_result_t updateGfxPscBin(firmwareInfo *fwInfo);
};

#endif