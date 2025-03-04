#ifndef _LOG_H
#define _LOG_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API logs: public cmds {

	public:
		logs() { STRCPY_S(name, MAX_PATH, "log"); };
		~logs() { };
		void help(list<help_cmd *> *help_list);
		int run(sysinfo *sys);
};

#endif