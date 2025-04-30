#ifndef _CMD_CONFIG_H
#define _CMD_CONFIG_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API cmdConfig : public cmds
{

public:
	cmdConfig() { STRCPY_S(name, MAX_PATH, "config"); };
	~cmdConfig() {};
	void help(list<help_cmd *> *help_list);
	int run(sysinfo *sys);
};
#endif
