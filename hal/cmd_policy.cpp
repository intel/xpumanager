#include "cmd_policy.h"
#include "debug.h"

void cmdPolicy::help(list<help_cmd *> *help_list)
{
	help_list->push_back(new help_cmd(NO_GAP, "Show policy information"));
}

int cmdPolicy::run(sysinfo *sys)
{
	TRACING();
	UNUSED(sys);
	return 0;
}
