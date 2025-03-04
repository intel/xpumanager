#ifndef _CMDS_H
#define _CMDS_H

#include <list>
#include "help_cmd.h"
#include "sysinfo.h"

using namespace std;

class LIBXPUM_API cmds {
	protected:
		char name[MAX_PATH];

	public:
		cmds() { };
		char *get_name() { return name; }
		virtual ~cmds() = 0;
		virtual void help(list<help_cmd *> *help_list) = 0;
		virtual int run(sysinfo *sys) = 0;
};

typedef void (cmds:: *help_func) (list<help_cmd *> *help_list);
typedef int (cmds:: *run_func) ();

struct cmd_struct
{
	help_func hf;
	run_func rf;
};

#endif
