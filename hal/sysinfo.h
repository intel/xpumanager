#ifndef _SYSINFO_H
#define _SYSINFO_H

#include <os.h>
#include "lz.h"

class LIBXPUM_API sysinfo {

	protected:
		p_dev devs[MAX_DEVS];
		bool init;
		int found_dev;
		lz *lz_obj;
	
	public:
		sysinfo();
		~sysinfo();
		bool is_init() { return init; }
		void print_lz_version() { if (lz_obj) { lz_obj->print_loader_versions(); } }
		void print_base_discovery();
};


#endif