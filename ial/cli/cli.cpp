#include <discovery.h>
#include <topology.h>
#include <diag.h>
#include <health.h>
#include <updatefw.h>
#include <config.h>
#include <ps.h>
#include <vgpu.h>
#include <stats.h>
#include <dump.h>
#include <log.h>
#include <list>
#include <debug.h>
#include <iostream>
#include "version.h"

using namespace std;

#define STRINGIFY(x) #x
#define CONCATENATE_WITH_DOT(a, b) "v" STRINGIFY(a) "." STRINGIFY(b)
#define GET_SHORT_VERSION() CONCATENATE_WITH_DOT(MAJOR, MINOR)

template <typename T>
void delete_list(list<T *> *generic_list)
{
	for(auto& it : *generic_list) {
		delete it;
	}
	delete generic_list;
}

void print_subcommands(list<cmds *> *cmd_list)
{
	for(auto& it : *cmd_list) {
		list<help_cmd *> *help_list = new list<help_cmd *>;
		it->help(help_list);

		if(help_list->size() < 1) {
			ERR("No help commands found\n");
			return;
		}

		/* Just print the first line of each subcommand's help because it contains the description */
		for(auto& it2 : *help_list) {
			PRINT("  %-*s%s\n", 28, it->get_name(), it2->line);
			break;
		}

		delete_list(help_list);
	}

}

void help(list<cmds *> *cmd_list)
{
	PRINT("Intel XPU System Management Interface -- %s\n", GET_SHORT_VERSION());
	PRINT("Intel XPU System Management Interface provides the Intel datacenter GPU model. "
		"It can also be used to update the firmware.\n");
	PRINT("Intel XPU System Management Interface is based on Intel oneAPI Level Zero. "
		"Before using Intel XPU System Management Interface, the GPU driver and Intel "
		"oneAPI Level Zero should be installed rightly.\n\n");
	PRINT("Supported devcies:\n");
	PRINT(" - Intel Arc B series GPU\n\n");

	PRINT("Usage: xpu-smi [Options]\n");
	PRINT("  xpu-smi -v\n");
	PRINT("  xpu-smi -h\n");
	PRINT("  xpu-smi discovery\n\n");

	PRINT("Options:\n");
	PRINT("  -h,--help                   Print this help message and exit\n");
	PRINT("  -v,--version                Display version information and exit\n\n");

	PRINT("Subcommands:\n");
	print_subcommands(cmd_list);
}

int main(int argc, char *argv[])
{
	TRACING();
	bool found = false;
	/* Create a list of commands */
	list<cmds *> *cmd_list = new list<cmds *>;

	/* Add each class in this list */
	cmd_list->push_back(new discovery());
	cmd_list->push_back(new topology());
	cmd_list->push_back(new diag());
	cmd_list->push_back(new health());
	cmd_list->push_back(new updatefw());
	cmd_list->push_back(new config());
	cmd_list->push_back(new ps());
	cmd_list->push_back(new vgpu());
	cmd_list->push_back(new stats());
	cmd_list->push_back(new dump());
	cmd_list->push_back(new logs());


	if(argc == 1) {
		help(cmd_list);
		delete_list(cmd_list);
		return 0;
	}

	/* Run each command */
	for(auto& it : *cmd_list) {
		if(!STRCASECMP(it->get_name(), argv[1])) {
			it->run();
			found = true;
		}
	}

	if(!found) {
		help(cmd_list);
	}

	delete_list(cmd_list);
	return 0;
}
