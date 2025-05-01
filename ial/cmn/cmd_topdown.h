#ifndef _CMD_TOPDOWN_H
#define _CMD_TOPDOWN_H

#include "cmds.h"
#include <os.h>

class cmdTopdown : public cmds
{
public:
	cmdTopdown() { STRCPY_S(name, MAX_PATH, "topdown"); };
	~cmdTopdown() {};
	void help(list<help_cmd *> *help_list);
	int run(sysinfo *sys);
};

#endif