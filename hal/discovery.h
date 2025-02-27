#ifndef _DISCOVERY_H
#define _DISCOVERY_H

#include <cmds.h>
#include <cstring>

class discovery: public cmds {

	public:
		discovery() { strcpy(name, "discovery"); };
		~discovery() { };
		void help(list<help_cmd *> *help_list);
		int run();
};

#endif