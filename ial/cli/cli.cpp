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

#include "cli.h"
#include "version.h"
#include <cmd_agentset.h>
#include <cmd_amcsensor.h>
#include <cmd_config.h>
#include <cmd_diag.h>
#include <cmd_discovery.h>
#include <cmd_dump.h>
#include <cmd_group.h>
#include <cmd_health.h>
#include <cmd_log.h>
#include <cmd_oob.h>
#include <cmd_policy.h>
#include <cmd_ps.h>
#include <cmd_stats.h>
#include <cmd_topdown.h>
#include <cmd_topology.h>
#include <cmd_updatefw.h>
#include <cmd_vgpu.h>
#include <debug.h>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <os.h>
#include <vector>

#ifdef DAEMONMODE
DAEMONCAP curDaemonMode = DAEMON;
string progName = "xpumcli";
#else
DAEMONCAP curDaemonMode = DAEMONLESS;
string progName = "xpu-smi";
#endif

#ifdef DEBUGON
int dbg_lvl = DBG;
#else
int dbg_lvl = INFO;
#endif

/**
 * @brief Creates an instance of a command class
 * @tparam T The command class type to instantiate
 * @return A pointer to the newly created command instance
 */
template <typename T> cmds *createInstance() { return new T(); }

/**
 * @brief Deletes all elements in a list of pointers and the list itself
 * @tparam T The type of objects in the list
 * @param generic_list The list of pointers to delete
 */
template <typename T> void deleteList(list<T *> *generic_list)
{
	for (auto &it : *generic_list) {
		delete it;
	}
	delete generic_list;
}

/**
 * @brief Prints the version information for both CLI and service components
 */
void printVersion()
{
	PRINT("%-*sCLI:\n", TITLE, "");
	PRINT("%-*sVersion: %s\n", HEADING, "", GET_FULL_VERSION());
	PRINT("%-*sBuild ID: 8389eee7\n", HEADING, "");
	PRINT("%-*s\n", TITLE, "");
	PRINT("%-*sService:\n", TITLE, "");
	PRINT("%-*sVersion: %s\n", HEADING, "", GET_FULL_VERSION());
	PRINT("%-*sBuild ID: 8389eee7\n", HEADING, "");
	// sys->print_lz_version();
}

/**
 * @brief Prints all available subcommands with short help text
 * @param cmd_list List of command objects to display
 */
void printSubCommands(list<cmds *> *cmd_list)
{
	for (auto &it : *cmd_list) {
		it->help(SHORT_HELP);
	}
}

/**
 * @brief Displays help information for daemon mode (xpumcli)
 */
void helpcli()
{
	PRINT("Intel XPU Manager Command Line Interface -- %s\n", GET_SHORT_VERSION());
	PRINT("Intel XPU Manager Command Line Interface provides the Intel data center GPU model and monitoring "
		  "capabilities.\n");
	PRINT("It can also be used to change the Intel data center GPU settings and update the firmware.\n");
	PRINT("Intel XPU Manager is based on Intel oneAPI Level Zero. Before using Intel XPU Manager,\n");
	PRINT("the GPU driver and Intel oneAPI Level Zero should be correctly installed.\n\n");
}

/**
 * @brief Displays help information for daemonless mode (xpu-smi)
 */
void helpsmi()
{
	PRINT("Intel XPU System Management Interface -- %s\n", GET_SHORT_VERSION());
	PRINT("Intel XPU System Management Interface provides the Intel data center GPU model."
		  " It can also be used to update the firmware.\n");
	PRINT("Intel XPU System Management Interface is based on Intel oneAPI Level Zero.\n");
	PRINT("Before using Intel XPU System Management Interface, the GPU driver and Intel "
		  "oneAPI Level Zero should be correctly installed.\n\n");
}

/**
 * @brief Displays comprehensive help information including usage, options, and subcommands
 * @param cmd_list List of command objects to include in help output
 */
void help(list<cmds *> *cmd_list)
{
	if (curDaemonMode == DAEMONCAP::DAEMON) {
		helpcli();
	} else {
		helpsmi();
	}

	PRINT("Supported devcies:\n");
	PRINT(" - Intel Arc B series GPU\n\n");

	PRINT("Usage: %s [Options]\n", progName.c_str());
	PRINT("  %s -v\n", progName.c_str());
	PRINT("  %s -h\n", progName.c_str());
	PRINT("  %s discovery\n\n", progName.c_str());

	PRINT("Options:\n");
	PRINT("  -h,--help                   Print this help message and exit\n");
	PRINT("  -v,--version                Display version information and exit\n\n");

	PRINT("Subcommands:\n");
	printSubCommands(cmd_list);
}

void setPrintLvl(arg_struct *arg, int lvl)
{
	setDbgLvl(lvl);
	arg->sm.setPrintLvl(lvl);
}

/**
 * @brief Main entry point for the application
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return Exit code (0 for success, non-zero for errors)
 */
int main(int argc, char *argv[])
{
	TRACING();
	bool found = false;
	arg_struct arg;
	int dbgLvl;
	bool priv = PRIVILEGECHECK();
	UNUSED(priv);

	// Get current debug level
	dbgLvl = getDbgLvl();

	// If we are not in debug mode, set the debug level to NO_PRINT.
	// That's because we don't want to see all the sysman initialization messages in release mode
	if (dbgLvl != DBG) {
		setPrintLvl(&arg, NO_PRINT);
	}

	// Create sysman driver instance
	ze_result_t result = arg.sm.init();
	switch (result) {
	case ZE_RESULT_SUCCESS:
		DBG("Sysman driver initialized successfully.\n");
		break;
	default:
		PRINT("Sysman driver initialization failed.\n");
		return -1;
		break;
	}

	if (arg.sm.run() != ZE_RESULT_SUCCESS) {
		PRINT("Sysman driver run failed.\n");
		return -1;
	}

	// Set the debug level back to the original value
	setPrintLvl(&arg, dbgLvl);

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
		{createInstance<cmdOOB>, DAEMONCAP::BOTH, OSTYPE::Linux},
		{createInstance<cmdGroup>, DAEMONCAP::DAEMON, OSTYPE::Linux},
		{createInstance<cmdPolicy>, DAEMONCAP::DAEMON, OSTYPE::Linux},
		{createInstance<cmdTopdown>, DAEMONCAP::DAEMON, OSTYPE::Linux},
		{createInstance<cmdAgentSet>, DAEMONCAP::DAEMON, OSTYPE::Linux},
		{createInstance<cmdAmcSensor>, DAEMONCAP::DAEMON, OSTYPE::Linux},
	};

	OSTYPE current_os = is_windows ? OSTYPE::Windows : OSTYPE::Linux;
	DBG("Is Daemon mode: %s\n", (curDaemonMode == DAEMONCAP::DAEMON) ? "true" : "false");
	arg.argc = argc;
	arg.argv = argv;

	for (const auto &entry : function_table) {
		if ((entry.os_type == OSTYPE::Both || entry.os_type == current_os) &&
			(entry.daemon_cap == DAEMONCAP::BOTH || entry.daemon_cap == curDaemonMode)) {
			/* Create an instance of the command and add it to the list */
			cmds *cmd = entry.create_func();
			DBG("Adding %s to command list\n", cmd->get_name());
			cmd_list->push_back(cmd);
		}
	}

	/* If no command line args are provided, just print help message and exit */
	if (argc == 1) {
		help(cmd_list);
		deleteList(cmd_list);
		return 0;
	}

	/* Print out version info if -v command line arg specified */
	if (!STRCASECMP(argv[1], "-v") || !STRCASECMP(argv[1], "--version")) {
		printVersion();
		deleteList(cmd_list);
		return 0;
	}

	/* Parse command line and run the command that the user wants */
	for (auto &it : *cmd_list) {
		if (!STRCASECMP(it->get_name(), argv[1])) {
			/* If the second argument is -h or --help, then just print their help */
			if (argc > 2 && (!STRCASECMP(argv[2], "-h") || !STRCASECMP(argv[2], "--help"))) {
				it->help(FULL_HELP);
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
	if (!found) {
		help(cmd_list);
	}

	deleteList(cmd_list);
	return 0;
}
