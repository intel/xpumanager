#ifndef _STATS_H
#define _STATS_H

#include <cmds.h>
#include <cstring>

class stats: public cmds {

	public:
		stats() { strcpy(name, "stats"); };
		~stats() { };
		void help(list<help_cmd *> *help_list);
		int run();
};

#endif