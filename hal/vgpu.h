#ifndef _VGPU_H
#define _VGPU_H

#include "cmds.h"
#include <os.h>

class LIBXPUM_API vgpu: public cmds {

	public:
		vgpu() { STRCPY_S(name, MAX_PATH, "vgpu"); };
		~vgpu() { };
		void help(list<help_cmd *> *help_list);
		int run();
};

#endif