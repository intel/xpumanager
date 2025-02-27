#ifndef _PS_H
#define _PS_H

#include <cmds.h>
#include <cstring>

class ps: public cmds {

	public:
		ps() { strcpy(name, "ps"); };
		~ps() { };
		void help(list<help_cmd *> *help_list);
		int run();
};

#endif