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
}

/**
 * @brief Destructor for the sysinfo class.
 */
sysinfo::~sysinfo()
{
	TRACING();
}
