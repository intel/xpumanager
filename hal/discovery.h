#ifndef _DISCOVERY_H
#define _DISCOVERY_H

#include <cmds.h>

class discovery: public cmds {

	public:
		discovery() { };
		~discovery() { };
		void help(list<help_cmd *> *help_list);
		int run();
};

#endif