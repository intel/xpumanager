#ifndef _CMD_DISCOVERY_H
#define _CMD_DISCOVERY_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API cmdDiscovery : public cmds
{

public:
	cmdDiscovery() { STRCPY_S(name, MAX_PATH, "discovery"); };
	~cmdDiscovery() {};
	void help(list<help_cmd *> *help_list);
	int run(sysinfo *sys);
};

#endif