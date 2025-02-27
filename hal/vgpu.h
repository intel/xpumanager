#ifndef _VGPU_H
#define _VGPU_H

#include <cmds.h>
#include <cstring>

class vgpu: public cmds {

	public:
		vgpu() { strcpy(name, "vgpu"); };
		~vgpu() { };
		void help(list<help_cmd *> *help_list);
		int run();
};

#endif