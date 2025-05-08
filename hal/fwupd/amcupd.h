#ifndef _AMCUPD_H
#define _AMCUPD_H

#include "fwupd.h"

class amcupd : public fwupd
{

public:
	amcupd() {}
	~amcupd() {}
	ze_result_t updateAMC(firmwareInfo *fwInfo) override;
};

#endif