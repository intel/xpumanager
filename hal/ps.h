#ifndef _PS_H
#define _PS_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API ps: public cmds {

	public:
		ps() { STRCPY_S(name, MAX_PATH, "ps"); };
		~ps() { };
		void help(list<help_cmd *> *help_list);
		int run();
};

#endif