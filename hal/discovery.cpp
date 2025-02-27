#include <discovery.h>
#include <debug.h>
#include <assert.h>

void discovery::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);
}

int discovery::run()
{
	TRACING();
	return 0;
}
