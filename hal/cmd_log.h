#ifndef _CMD_LOG_H
#define _CMD_LOG_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API cmdLogs : public cmds
{

public:
	cmdLogs() { STRCPY_S(name, MAX_PATH, "log"); };
	~cmdLogs() {};
	void help(list<help_cmd *> *help_list);
	int run(sysinfo *sys);
};

#endif