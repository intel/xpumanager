#ifndef _STATS_H
#define _STATS_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API stats: public cmds {

	public:
		stats() { STRCPY_S(name, MAX_PATH, "stats"); };
		~stats() { };
		void help(list<help_cmd *> *help_list);
		int run();
};

#endif