#ifndef _CMD_GROUP_H
#define _CMD_GROUP_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API cmdGroup : public cmds
{
public:
	cmdGroup() { STRCPY_S(name, MAX_PATH, "group"); };
	~cmdGroup() {};
	void help(list<help_cmd *> *help_list);
	int run(sysinfo *sys);
};

#endif