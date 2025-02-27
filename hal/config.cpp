#include <config.h>
#include <debug.h>
#include <assert.h>

void config::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);
}

int config::run()
{
	TRACING();
	return 0;
}
