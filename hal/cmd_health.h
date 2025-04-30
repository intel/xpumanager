#ifndef _CMD_HEALTH_H
#define _CMD_HEALTH_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API cmdHealth : public cmds
{

public:
	cmdHealth() { STRCPY_S(name, MAX_PATH, "health"); };
	~cmdHealth() {};
	void help(list<help_cmd *> *help_list);
	int run(sysinfo *sys);
};

#endif