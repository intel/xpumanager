#ifndef _CMD_DIAG_H
#define _CMD_DIAG_H

#include "cmds.h"
#include <os.h>

class cmdDiag : public cmds
{

public:
	cmdDiag() { STRCPY_S(name, MAX_PATH, "diag"); };
	~cmdDiag() {};
	void help(list<help_cmd *> *help_list);
	int run(sysinfo *sys);
};

#endif