#ifndef _CMDS_H
#define _CMDS_H

#include "help_cmd.h"

class cmds {
	public:
		cmds() { };
		virtual ~cmds() = 0;
		virtual void help(help_cmd *help_msg, int *lines_filled) = 0;
		virtual int run() = 0;
};

typedef void (cmds:: *help_func) (help_cmd *help_msg, int *lines_filled);
typedef int (cmds:: *run_func) ();

struct cmd_struct
{
	help_func hf;
	run_func rf;
};

#endif
