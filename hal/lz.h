#ifndef _LZ_H
#define _LZ_H

#include <string>
#include <ze_api.h>
#include <zet_api.h>
#include <list>
#include <vector>
#include "pdev.h"

using namespace std;

class lz {
protected:
	ze_driver_handle_t p_driver;
	ze_device_handle_t p_device[MAX_DEVS];
	int total_devs;
	ze_context_handle_t context;
	ze_device_properties_t device_properties;
	bool init;

	bool init_ze();
	bool find_dev(ze_driver_handle_t p_dr,
		ze_device_type_t type);
public:
	lz() { init = false; }
	~lz() { };
	ze_result_t initialize();
	void print_loader_versions();
	bool is_init() { return init; }
};

#endif
