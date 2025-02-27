#ifndef _CONFIG_H
#define _CONFIG_H

#include <cmds.h>

class config: public cmds {

	public:
		config() { };
		~config() { };
		void help(help_cmd *help_msg, int *lines_filled);
		int run();
};
#endif
