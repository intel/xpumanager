#ifndef _CMD_DUMP_H
#define _CMD_DUMP_H

#include "cmds.h"
#include <os.h>

class cmdDump : public cmds
{

public:
	cmdDump() { STRCPY_S(name, MAX_PATH, "dump"); };
	~cmdDump() {};
	void help(list<help_cmd *> *help_list);
	int run(sysinfo *sys);
};

#endif