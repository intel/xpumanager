#include "cmd_topdown.h"
#include "debug.h"

void cmdTopdown::help(list<help_cmd *> *help_list)
{
	help_list->push_back(new help_cmd(NO_GAP, "Show topdown information"));
}

int cmdTopdown::run(sysinfo *sys)
{
	TRACING();
	UNUSED(sys);
	return 0;
}
