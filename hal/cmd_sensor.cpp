#include "cmd_sensor.h"
#include "debug.h"

void cmdSensor::help(list<help_cmd *> *help_list)
{
	help_list->push_back(new help_cmd(NO_GAP, "Show sensor information"));
}

int cmdSensor::run(sysinfo *sys)
{
	TRACING();
	UNUSED(sys);
	return 0;
}
