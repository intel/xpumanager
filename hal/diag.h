#ifndef _DIAG_H
#define _DIAG_H

#include <cmds.h>
#include <cstring>

class diag: public cmds {

	public:
		diag() { strcpy(name, "diag"); };
		~diag() { };
		void help(list<help_cmd *> *help_list);
		int run();
};

#endif