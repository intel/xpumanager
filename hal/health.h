#ifndef _HEALTH_H
#define _HEALTH_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API health: public cmds {

	public:
		health() { STRCPY_S(name, MAX_PATH, "health"); };
		~health() { };
		void help(list<help_cmd *> *help_list);
		int run(sysinfo *sys);
};

#endif