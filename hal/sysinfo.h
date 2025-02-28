#ifndef _SYSINFO_H
#define _SYSINFO_H

#include <os.h>
#include "pdev.h"

class LIBXPUM_API sysinfo {

	protected:
		p_dev devs[MAX_DEVS];
		bool init;
		int found_dev;
	
	public:
		sysinfo();
		~sysinfo();
		bool is_init() { return init; }

};


#endif