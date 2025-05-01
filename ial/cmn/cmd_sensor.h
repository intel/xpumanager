#ifndef _CMD_SENSOR_H
#define _CMD_SENSOR_H

#include "cmds.h"
#include <os.h>

class cmdSensor : public cmds
{
public:
	cmdSensor() { STRCPY_S(name, MAX_PATH, "sensor"); };
	~cmdSensor() {};
	void help(list<help_cmd *> *help_list);
	int run(sysinfo *sys);
};

#endif