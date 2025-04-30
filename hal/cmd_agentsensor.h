#ifndef _CMD_AGENTSENSOR_H
#define _CMD_AGENTSENSOR_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API cmdAgentSensor : public cmds
{
public:
	cmdAgentSensor() { STRCPY_S(name, MAX_PATH, "agentsensor"); };
	~cmdAgentSensor() {};
	void help(list<help_cmd *> *help_list);
	int run(sysinfo *sys);
};

#endif
