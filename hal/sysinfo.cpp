#include "sysinfo.h"
#include "debug.h"

/**
 * @brief Constructor for the sysinfo class.
 */
sysinfo::sysinfo()
{
	TRACING();
	init = 0;
	found_dev = GET_PCI_DEV(devs);
	if (found_dev) {
		lz_obj = new lz();
		lz_obj->initialize();
		if(lz_obj->is_init()) {
			DBG("Driver initialized.\n");
			init = 1;
		}
	}
	
}

/**
 * @brief Destructor for the sysinfo class.
 */
sysinfo::~sysinfo()
{
	TRACING();
	if (lz_obj) {
		delete lz_obj;
		lz_obj = NULL;
	}
	PCI_CLEANUP(devs, found_dev);
}

