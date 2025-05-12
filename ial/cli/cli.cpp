/*
 * Copyright © 2025 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include <cmd_discovery.h>
#include <cmd_topology.h>
#include <cmd_diag.h>
#include <cmd_health.h>
#include <cmd_updatefw.h>
#include <cmd_config.h>
#include <cmd_ps.h>
#include <cmd_vgpu.h>
#include <cmd_stats.h>
#include <cmd_dump.h>
#include <cmd_log.h>
#include <cmd_group.h>
#include <cmd_policy.h>
#include <cmd_topdown.h>
#include <cmd_sensor.h>
#include <cmd_agentsensor.h>
#include <memory>
#include <functional>
#include <list>
#include <debug.h>
#include <os.h>
#include <iostream>
#include <vector>
#include "version.h"
#include "cli.h"

/* Function to create an instance of a class */
template <typename T>
cmds *createInstance()
{
	return new T();
}

/* Function to delete a list of pointers */
template <typename T>
void deleteList(list<T *> *generic_list)
{
	for (auto &it : *generic_list)
	{
		delete it;
	}
	delete generic_list;
}

void printVersion()
{
	PRINT("%-*sCLI:\n", NO_GAP, "");
	PRINT("%-*sVersion: %s\n", SMALL_GAP, "", GET_FULL_VERSION());
	PRINT("%-*sBuild ID: 8389eee7\n", SMALL_GAP, "");
	PRINT("%-*s\n", NO_GAP, "");
	PRINT("%-*sService:\n", NO_GAP, "");
	PRINT("%-*sVersion: %s\n", SMALL_GAP, "", GET_FULL_VERSION());
	PRINT("%-*sBuild ID: 8389eee7\n", SMALL_GAP, "");
	// sys->print_lz_version();
}

void printSubCommand(cmds *it, HELP help_type)
{
	list<helpCmd *> *helpList = new list<helpCmd *>;
	it->help(helpList);

	if (helpList->size() < 1)
	{
		ERR("No help commands found\n");
		deleteList(helpList);
		return;
	}

	if (help_type == SHORT_HELP)
	{
		/* Just print the first line of each subcommand's help because it contains the description */
		for (auto &it2 : *helpList)
		{
			PRINT("  %-*s%s\n", 28, it->get_name(), it2->line);
			break;
		}
	}
	else
	{
		for (auto &it2 : *helpList)
		{
			PRINT("%-*s%s\n", it2->char_gap, "", it2->line);
		}
	}

	deleteList(helpList);
}

void printSubCommands(list<cmds *> *cmd_list)
{
	for (auto &it : *cmd_list)
	{
		printSubCommand(it, SHORT_HELP);
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
	printSubCommands(cmd_list);
}

int main(int argc, char *argv[])
{
	TRACING();
	bool found = false;
	arg_struct arg;
	bool priv = PRIVILEGECHECK();
	UNUSED(priv);

	// Create sysman driver instance
	ze_result_t result = arg.sm.init();
	switch (result)
	{
	case ZE_RESULT_SUCCESS:
		PRINT("Sysman driver initialized successfully.\n");
		break;
	default:
		ERR("sysman driver initialization failed.\n");
		return -1;
		break;
	}

	if (arg.sm.run() != ZE_RESULT_SUCCESS)
	{
		ERR("sysman driver run failed.\n");
		return -1;
	}

	/* Create a list of commands */
	list<cmds *> *cmd_list = new list<cmds *>;

	vector<function_entry> function_table = {
		{createInstance<cmdDiscovery>, DAEMONCAP::BOTH, OSTYPE::Both},
		{createInstance<cmdTopology>, DAEMONCAP::BOTH, OSTYPE::Linux},
		{createInstance<cmdDiag>, DAEMONCAP::BOTH, OSTYPE::Linux},
		{createInstance<cmdHealth>, DAEMONCAP::BOTH, OSTYPE::Linux},
		{createInstance<cmdUpdateFW>, DAEMONCAP::BOTH, OSTYPE::Both},
		{createInstance<cmdConfig>, DAEMONCAP::BOTH, OSTYPE::Both},
		{createInstance<cmdPs>, DAEMONCAP::BOTH, OSTYPE::Linux},
		{createInstance<cmdVgpu>, DAEMONCAP::BOTH, OSTYPE::Linux},
		{createInstance<cmdStats>, DAEMONCAP::BOTH, OSTYPE::Both},
		{createInstance<cmdDump>, DAEMONCAP::BOTH, OSTYPE::Both},
		{createInstance<cmdLogs>, DAEMONCAP::BOTH, OSTYPE::Linux},
		{createInstance<cmdGroup>, DAEMONCAP::DAEMON, OSTYPE::Linux},
		{createInstance<cmdPolicy>, DAEMONCAP::DAEMON, OSTYPE::Linux},
		{createInstance<cmdTopdown>, DAEMONCAP::DAEMON, OSTYPE::Linux},
		{createInstance<cmdSensor>, DAEMONCAP::DAEMON, OSTYPE::Linux},
		{createInstance<cmdAgentSensor>, DAEMONCAP::DAEMON, OSTYPE::Linux},
	};

	OSTYPE current_os = is_windows ? OSTYPE::Windows : OSTYPE::Linux;
	DBG("Is Daemon mode: %s\n", (curDaemonMode == DAEMONCAP::DAEMON) ? "true" : "false");
	arg.argc = argc;
	arg.argv = argv;

	for (const auto &entry : function_table)
	{
		if ((entry.os_type == OSTYPE::Both || entry.os_type == current_os) &&
			(entry.daemon_cap == DAEMONCAP::BOTH || entry.daemon_cap == curDaemonMode))
		{
			/* Create an instance of the command and add it to the list */
			cmds *cmd = entry.create_func();
			DBG("Adding %s to command list\n", cmd->get_name());
			cmd_list->push_back(cmd);
		}
	}

	/* If no command line args are provided, just print help message and exit */
	if (argc == 1)
	{
		help(cmd_list);
		deleteList(cmd_list);
		return 0;
	}

	/* Print out version info if -v command line arg specified */
	if (!STRCASECMP(argv[1], "-v") || !STRCASECMP(argv[1], "--version"))
	{
		printVersion();
		deleteList(cmd_list);
		return 0;
	}

	/* Parse command line and run the command that the user wants */
	for (auto &it : *cmd_list)
	{
		if (!STRCASECMP(it->get_name(), argv[1]))
		{
			/* If the second argument is -h or --help, then just print their help */
			if (argc > 2 && (!STRCASECMP(argv[2], "-h") || !STRCASECMP(argv[2], "--help")))
			{
				printSubCommand(it, FULL_HELP);
				deleteList(cmd_list);
				return 0;
			}
			/* Run the command */
			it->run(&arg);
			found = true;
			/* Exit the loop once the command is found */
			break;
		}
	}

	/* If we can't parse the user's command line, then print help */
	if (!found)
	{
		help(cmd_list);
	}

	deleteList(cmd_list);
	return 0;
}
