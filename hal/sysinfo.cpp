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
		init = 1;
	}
}

/**
 * @brief Destructor for the sysinfo class.
 */
sysinfo::~sysinfo()
{
	TRACING();
	PCI_CLEANUP(devs, found_dev);
}

