#ifndef _CMD_PS_H
#define _CMD_PS_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API cmdPs : public cmds
{

public:
	cmdPs() { STRCPY_S(name, MAX_PATH, "ps"); };
	~cmdPs() {};
	void help(list<help_cmd *> *help_list);
	int run(sysinfo *sys);
};

#endif