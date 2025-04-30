#include "sysinfo.h"
#include "debug.h"

/**
 * @brief Constructor for the sysinfo class.
 */
sysinfo::sysinfo()
{
	TRACING();
	init = 0;
	memset(devs, 0, sizeof(p_dev) * MAX_DEVS);
	lz_obj = NULL;

	if (TESTING)
	{
		init = 1;
		return;
	}

	found_dev = GET_PCI_DEV(devs);
	if (found_dev)
	{
		lz_obj = new lz();
		lz_obj->initialize(devs, found_dev);
		if (lz_obj->is_init())
		{
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
	if (lz_obj)
	{
		lz_obj->cleanup(devs, found_dev);
		delete lz_obj;
		lz_obj = NULL;
	}
	PCI_CLEANUP(devs, found_dev);
}
