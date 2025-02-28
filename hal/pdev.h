#ifndef _PDEV_H
#define _PDEV_H

#include <cstdint>

#define MAX_DEVS                           5

struct p_dev {
	void *dev;
	char resource_name[256];
	uint8_t *bar;
};

#endif