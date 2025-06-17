#ifndef _CMD_OOB_H
#define _CMD_OOB_H

#include "cmds.h"
#include <os.h>
#include <string>

class cmdOOB : public cmds
{
public:
	cmdOOB() { STRCPY_S(name, MAX_PATH, "oob"); }
	~cmdOOB() {}

	void help(HELP helpType = FULL_HELP) override;
	int run(arg_struct *args) override;
};

#endif