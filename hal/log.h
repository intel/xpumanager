#ifndef _LOG_H
#define _LOG_H

#include <cmds.h>
#include <cstring>

class log: public cmds {

	public:
		log() { strcpy(name, "log"); };
		~log() { };
		void help(list<help_cmd *> *help_list);
		int run();
};

#endif