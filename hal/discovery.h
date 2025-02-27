#ifndef _DISCOVERY_H
#define _DISCOVERY_H

#include <cmds.h>

class discovery: public cmds {

	public:
		discovery() { };
		~discovery() { };
		void help(help_cmd *help_msg, int *lines_filled);
		int run();
};

#endif