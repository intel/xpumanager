#ifndef _UPDATEFW_H
#define _UPDATEFW_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API updatefw: public cmds {

	public:
		updatefw() { STRCPY_S(name, MAX_PATH, "updatefw"); };
		~updatefw() { };
		void help(list<help_cmd *> *help_list);
		int run(sysinfo *sys);
};

#endif