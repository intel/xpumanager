#ifndef _LZ_H
#define _LZ_H

#include <string>
#include <list>
#include <vector>
#include "pdev.h"

using namespace std;

class lz {
protected:
	ze_driver_handle_t p_driver;
	ze_context_handle_t context;
	bool init;

	bool init_ze();
	bool find_dev(ze_driver_handle_t p_dr,
		ze_device_type_t type, p_dev *devs, int found_devs);
public:
	lz() { init = false; }
	~lz();
	ze_result_t initialize(p_dev *devs, int found_devs);
	void cleanup(p_dev *devs, int found_devs);
	void print_loader_versions();
	bool is_init() { return init; }
};

#endif
