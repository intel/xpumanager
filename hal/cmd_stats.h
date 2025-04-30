#ifndef _CMD_STATS_H
#define _CMD_STATS_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API cmdStats : public cmds
{

public:
	cmdStats() { STRCPY_S(name, MAX_PATH, "stats"); };
	~cmdStats() {};
	void help(list<help_cmd *> *help_list);
	int run(sysinfo *sys);
};

#endif