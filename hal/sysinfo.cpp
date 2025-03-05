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

	if (TESTING) {
		init = 1;
		return;
	}

	found_dev = GET_PCI_DEV(devs);
	if (found_dev) {
		lz_obj = new lz();
		lz_obj->initialize(devs, found_dev);
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
		lz_obj->cleanup(devs, found_dev);
		delete lz_obj;
		lz_obj = NULL;
	}
	PCI_CLEANUP(devs, found_dev);
}

void sysinfo::print_base_discovery()
{
	char uuidStr[37] = {};

	for (int i = 0; i < found_dev; i++) {
		PRINT("Device Name: Intel(R) Graphics [0x%04x]\n", GET_DEV_ID(devs[i].dev));
		PRINT("Vendor Name: Intel(R) Corporation\n");
		uint8_t *uuid_buf = devs[i].device_properties.uuid.id;
		SPRINTF_S(uuidStr, 37,
			"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			uuid_buf[15], uuid_buf[14], uuid_buf[13], uuid_buf[12], uuid_buf[11], uuid_buf[10], uuid_buf[9], uuid_buf[8],
			uuid_buf[7], uuid_buf[6], uuid_buf[5], uuid_buf[4], uuid_buf[3], uuid_buf[2], uuid_buf[1], uuid_buf[0]);
		PRINT("SOC UUID: %s\n", uuidStr);
		PRINT("PCI BDF Address: 0000:%02d.%02d.%d\n", GET_BUS(devs[i].dev), GET_DEV(devs[i].dev), GET_FUNC(devs[i].dev));
		PRINT("DRM Device: /dev/dri/card0\n");
		PRINT("Function Type: physical\n");
		PRINT("\n");
	}
}