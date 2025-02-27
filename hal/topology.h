#ifndef _TOPOLOGY_H
#define _TOPOLOGY_H

#include <cmds.h>
#include <cstring>

class topology: public cmds {

	public:
		topology() { strcpy(name, "topology"); };
		~topology() { };
		void help(list<help_cmd *> *help_list);
		int run();
};

#endif