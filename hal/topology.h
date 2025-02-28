#ifndef _TOPOLOGY_H
#define _TOPOLOGY_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API topology: public cmds {

	public:
		topology() { STRCPY_S(name, MAX_PATH, "topology"); };
		~topology() { };
		void help(list<help_cmd *> *help_list);
		int run();
};

#endif