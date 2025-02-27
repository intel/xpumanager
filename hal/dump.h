#ifndef _DUMP_H
#define _DUMP_H

#include <cmds.h>
#include <cstring>

class dump: public cmds {

	public:
		dump() { strcpy(name, "dump"); };
		~dump() { };
		void help(list<help_cmd *> *help_list);
		int run();
};

#endif