#ifndef _HEALTH_H
#define _HEALTH_H

#include <cmds.h>
#include <cstring>

class health: public cmds {

	public:
		health() { strcpy(name, "health"); };
		~health() { };
		void help(list<help_cmd *> *help_list);
		int run();
};

#endif