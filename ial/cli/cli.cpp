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
#include <sysinfo.h>
#include <memory>
#include <functional>
#include <list>
#include <debug.h>
#include <iostream>
#include "version.h"

using namespace std;

#define STRINGIFY(x) #x
#define CONCATENATE_WITH_DOT(a, b) "v" STRINGIFY(a) "." STRINGIFY(b)
#define CONCATENATE_WITH_DOTS(a, b, c, d) STRINGIFY(a) "." STRINGIFY(b) "." STRINGIFY(c) "." STRINGIFY(d)
#define GET_SHORT_VERSION() CONCATENATE_WITH_DOT(MAJOR, MINOR)
#define GET_FULL_VERSION() CONCATENATE_WITH_DOTS(MAJOR, MINOR, PATCH, BUILD_NUMBER)

enum HELP {
	SHORT_HELP,
	FULL_HELP,
};

template <typename T>
void delete_list(list<T *> *generic_list)
{
	for(auto& it : *generic_list) {
		delete it;
	}
	delete generic_list;
}

void print_version(sysinfo *sys)
{
	PRINT("%-*sCLI:\n", NO_GAP, "");
	PRINT("%-*sVersion: %s\n", SMALL_GAP, "", GET_FULL_VERSION());
	PRINT("%-*sBuild ID: 8389eee7\n", SMALL_GAP, "");
	PRINT("%-*s\n", NO_GAP, "");
	PRINT("%-*sService:\n", NO_GAP, "");
	PRINT("%-*sVersion: %s\n", SMALL_GAP, "", GET_FULL_VERSION());
	PRINT("%-*sBuild ID: 8389eee7\n", SMALL_GAP, "");
	sys->print_lz_version();
}

void print_subcommand(cmds *it, HELP help_type)
{
	list<help_cmd *> *help_list = new list<help_cmd *>;
	it->help(help_list);

	if(help_list->size() < 1) {
		ERR("No help commands found\n");
		return;
	}

	if(help_type == SHORT_HELP) {
		/* Just print the first line of each subcommand's help because it contains the description */
		for(auto& it2 : *help_list) {
			PRINT("  %-*s%s\n", 28, it->get_name(), it2->line);
			break;
		}
	} else {
		for(auto& it2 : *help_list) {
			PRINT("%-*s%s\n", it2->char_gap, "", it2->line);
		}
	}

	delete_list(help_list);
}

void print_subcommands(list<cmds *> *cmd_list)
{
	for(auto& it : *cmd_list) {
		print_subcommand(it, SHORT_HELP);
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

// Enum to represent OS types
enum class OSTYPE {
	Windows,
	Linux,
	Both,
};

/* Function to create an instance of a class */
template <typename T>
cmds* create_instance()
{
	return new T();
}

/* Structure to hold function and OS type */
struct function_entry {
	std::function<cmds* ()> create_func;
	OSTYPE os_type;
};

int main(int argc, char *argv[])
{
	TRACING();
	bool found = false;

	/* First and foremost, let's find out if there are any GPUs on this system */
	sysinfo *sys = new sysinfo();
	if(!sys->is_init()) {
		ERR("Failed to initialize sysinfo. Couldn't find any GPUs\n");
		delete sys;
		return -1;
	}

	/* Create a list of commands */
	list<cmds *> *cmd_list = new list<cmds *>;

	std::vector<function_entry> function_table = {
		{ create_instance<discovery>, OSTYPE::Both  },
		{ create_instance<topology>,  OSTYPE::Linux },
		{ create_instance<diag>,      OSTYPE::Linux },
		{ create_instance<health>,    OSTYPE::Linux },
		{ create_instance<updatefw>,  OSTYPE::Both  },
		{ create_instance<config>,    OSTYPE::Both  },
		{ create_instance<ps>,        OSTYPE::Linux },
		{ create_instance<vgpu>,      OSTYPE::Linux },
		{ create_instance<stats>,     OSTYPE::Both  },
		{ create_instance<dump>,      OSTYPE::Both  },
		{ create_instance<logs>,      OSTYPE::Linux },
	};

	OSTYPE current_os = is_windows ? OSTYPE::Windows : OSTYPE::Linux;

	for (const auto& entry : function_table) {
		if (entry.os_type == OSTYPE::Both || entry.os_type == current_os) {
			cmd_list->push_back(entry.create_func());
		}
	}

	/* If no command line args are provided, just print help message and exit */
	if(argc == 1) {
		help(cmd_list);
		delete_list(cmd_list);
		delete sys;
		return 0;
	}

	/* Print out version info if -v command line arg specified */
	if(!STRCASECMP(argv[1], "-v") || !STRCASECMP(argv[1], "--version")) {
		print_version(sys);
		delete_list(cmd_list);
		delete sys;
		return 0;
	}

	/* Parse command line and run the command that the user wants */
	for(auto& it : *cmd_list) {
		if(!STRCASECMP(it->get_name(), argv[1])) {
			/* If the second argument is -h or --help, then just print their help */
			if(argc == 2 || (argc > 2 && (!STRCASECMP(argv[2], "-h") || !STRCASECMP(argv[2], "--help")))) {
				print_subcommand(it, FULL_HELP);
				delete_list(cmd_list);
				delete sys;
				return 0;
			}
			/* Run the command */
			it->run();
			found = true;
		}
	}

	/* If we can't parse the user's command line, then print help */
	if(!found) {
		help(cmd_list);
	}

	delete_list(cmd_list);
	delete sys;
	return 0;
}
