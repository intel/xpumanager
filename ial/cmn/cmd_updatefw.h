#ifndef _CMD_UPDATEFW_H
#define _CMD_UPDATEFW_H

#include "cmds.h"
#include <os.h>

class cmdUpdateFW : public cmds
{

public:
	cmdUpdateFW() { STRCPY_S(name, MAX_PATH, "updatefw"); };
	~cmdUpdateFW() {};
	void help(list<help_cmd *> *help_list);
	int run(sysinfo *sys);
};

#endif