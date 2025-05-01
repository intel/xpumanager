#ifndef _CMD_POLICY_H
#define _CMD_POLICY_H

#include "cmds.h"
#include <os.h>

class cmdPolicy : public cmds
{
public:
	cmdPolicy() { STRCPY_S(name, MAX_PATH, "policy"); };
	~cmdPolicy() {};
	void help(list<help_cmd *> *help_list);
	int run(sysinfo *sys);
};

#endif