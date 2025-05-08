#include "amcupd.h"
#include <debug.h>
#include <os.h>

ze_result_t amcupd::updateAMC(firmwareInfo *fwInfo)
{
	TRACING();
	UNUSED(fwInfo);
	DBG("Updating AMC firmware...\n");
	return ZE_RESULT_SUCCESS;
}