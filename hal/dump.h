#ifndef _DUMP_H
#define _DUMP_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API dump: public cmds {

	public:
		dump() { STRCPY_S(name, MAX_PATH, "dump"); };
		~dump() { };
		void help(list<help_cmd *> *help_list);
		int run(sysinfo *sys);
};

#endif