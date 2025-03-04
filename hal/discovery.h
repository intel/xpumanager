#ifndef _DISCOVERY_H
#define _DISCOVERY_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API discovery: public cmds {

	public:
		discovery() { STRCPY_S(name, MAX_PATH, "discovery"); };
		~discovery() { };
		void help(list<help_cmd *> *help_list);
		int run(sysinfo *sys);
};

#endif