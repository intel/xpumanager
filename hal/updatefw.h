#ifndef _UPDATEFW_H
#define _UPDATEFW_H

#include <cmds.h>
#include <cstring>

class updatefw: public cmds {

	public:
		updatefw() { strcpy(name, "updatefw"); };
		~updatefw() { };
		void help(list<help_cmd *> *help_list);
		int run();
};

#endif