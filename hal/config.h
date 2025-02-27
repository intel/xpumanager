#ifndef _CONFIG_H
#define _CONFIG_H

#include <cmds.h>
#include <cstring>

class config: public cmds {

	public:
		config() { strcpy(name, "config"); };
		~config() { };
		void help(list<help_cmd *> *help_list);
		int run();
};
#endif
