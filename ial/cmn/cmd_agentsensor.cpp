#include "cmd_agentsensor.h"
#include "debug.h"

void cmdAgentSensor::help(list<help_cmd *> *help_list)
{
	help_list->push_back(new help_cmd(NO_GAP, "Get or change some XPU Manager settings."));
}

int cmdAgentSensor::run(sysinfo *sys)
{
	TRACING();
	UNUSED(sys);
	return 0;
}