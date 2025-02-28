#ifndef _DIAG_H
#define _DIAG_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API diag: public cmds {

	public:
		diag() { STRCPY_S(name, MAX_PATH, "diag"); };
		~diag() { };
		void help(list<help_cmd *> *help_list);
		int run();
};

#endif