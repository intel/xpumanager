#ifndef _CMD_TOPOLOGY_H
#define _CMD_TOPOLOGY_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API cmdTopology : public cmds
{

public:
	cmdTopology() { STRCPY_S(name, MAX_PATH, "topology"); };
	~cmdTopology() {};
	void help(list<help_cmd *> *help_list);
	int run(sysinfo *sys);
};

#endif