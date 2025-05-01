#ifndef _CMD_VGPU_H
#define _CMD_VGPU_H

#include "cmds.h"
#include <os.h>

class cmdVgpu : public cmds
{

public:
	cmdVgpu() { STRCPY_S(name, MAX_PATH, "vgpu"); };
	~cmdVgpu() {};
	void help(list<help_cmd *> *help_list);
	int run(sysinfo *sys);
};

#endif