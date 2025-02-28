#ifndef _CONFIG_H
#define _CONFIG_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API config: public cmds {

	public:
		config() { STRCPY_S(name, MAX_PATH, "config"); };
		~config() { };
		void help(list<help_cmd *> *help_list);
		int run();
};
#endif
