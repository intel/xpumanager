#ifndef _GSCUPD_H
#define _GSCUPD_H

#include "fwupd.h"

class gscupd : public fwupd
{
public:
	void updateFirmware() override;
};

#endif