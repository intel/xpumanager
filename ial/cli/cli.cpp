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
std::string progName = "xpumcli";
#else
DAEMONCAP curDaemonMode = DAEMONLESS;
std::string progName = "xpu-smi";
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
 * @param genericList The list of pointers to delete
 */
template <typename T> void deleteList(std::list<T *> *genericList)
{
	for (auto &it : *genericList) {
		delete it;
	}
	delete genericList;
}

/**
 * @brief Prints the version information for both CLI and service components
 */
void printVersion(arg_struct *arg)
{
	std::string lzVersion;
	PRINT("%-*sCLI:\n", TITLE, "");
	PRINT("%-*sVersion: %s\n", HEADING, "", GET_FULL_VERSION());
	PRINT("%-*sBuild ID: 8389eee7\n", HEADING, "");
	PRINT("%-*s\n", TITLE, "");
	PRINT("%-*sService:\n", TITLE, "");
	PRINT("%-*sVersion: %s\n", HEADING, "", GET_FULL_VERSION());
	PRINT("%-*sBuild ID: 8389eee7\n", HEADING, "");
	arg->sm.getLoaderVersion(&lzVersion);
	PRINT("%-*sLevel Zero Version: %s\n", HEADING, "", lzVersion.c_str());
}

/**
 * @brief Prints all available subcommands with short help text
 * @param cmdList List of command objects to display
 */
void printSubCommands(std::list<cmds *> *cmdList)
{
	for (const auto &it : *cmdList) {
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
 * @param cmdList List of command objects to include in help output
 */
void help(std::list<cmds *> *cmdList)
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
	printSubCommands(cmdList);
}

/**
 * @brief Enhanced print level setting that works across different build systems
 *
 * This function sets the debug level for both the CLI and library components.
 * For meson builds where CLI and library are separate modules, it implements
 * aggressive synchronization to ensure both modules use the same debug level.
 *
 * @param arg Pointer to argument structure containing system manager instance
 * @param lvl Debug level to set (e.g., DBG, INFO, WARN, ERR, NO_PRINT)
 */
void setPrintLvl(arg_struct *arg, int lvl)
{
	// Set the CLI's debug level first using the enhanced function
	setDbgLvlExtended(lvl);

	// Set the library's debug level using standard method
	arg->sm.setPrintLvl(lvl);

	// For meson builds, we need additional synchronization for critical levels
	if (lvl == NO_PRINT) {
		// Use the forced synchronization method for NO_PRINT level
		// This is especially important during initialization when we want
		// to suppress library initialization messages
		arg->sm.forceDebugSync(lvl);

		// Also force the CLI level multiple times to ensure consistency
		setDbgLvlExtended(lvl);

		// Verify synchronization by checking if both levels match
		int cliLevel = getDbgLvlExtended();
		int libLevel = arg->sm.getPrintLvl();

		// If levels don't match, try one more aggressive sync
		if (cliLevel != libLevel && cliLevel != NO_PRINT) {
			arg->sm.forceDebugSync(NO_PRINT);
			setDbgLvlExtended(NO_PRINT);
		}
	}
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
	int dbglvl;
	bool priv = PRIVILEGECHECK();
	UNUSED_VAR(priv);

	// Get current debug level
	dbglvl = getDbgLvl();

	// If we are in < debug mode, set the debug level to NO_PRINT.
	// That's because we don't want to see all the sysman initialization messages in release mode
	if (dbglvl < DBG) {
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

	// Set the debug level back to the original value
	setPrintLvl(&arg, dbglvl);

	/* Create a list of commands */
	std::list<cmds *> *cmdList = new std::list<cmds *>;

	std::vector<function_entry> functionTable = {
		{createInstance<cmdDiscovery>, DAEMONCAP::BOTH, OSTYPE::BOTH},
		{createInstance<cmdTopology>, DAEMONCAP::BOTH, OSTYPE::BOTH},
		{createInstance<cmdDiag>, DAEMONCAP::BOTH, OSTYPE::LINUX},
		{createInstance<cmdHealth>, DAEMONCAP::BOTH, OSTYPE::LINUX},
		{createInstance<cmdUpdateFW>, DAEMONCAP::BOTH, OSTYPE::BOTH},
		{createInstance<cmdConfig>, DAEMONCAP::BOTH, OSTYPE::BOTH},
		{createInstance<cmdPs>, DAEMONCAP::BOTH, OSTYPE::LINUX},
		{createInstance<cmdVgpu>, DAEMONCAP::BOTH, OSTYPE::LINUX},
		{createInstance<cmdStats>, DAEMONCAP::BOTH, OSTYPE::BOTH},
		{createInstance<cmdDump>, DAEMONCAP::BOTH, OSTYPE::BOTH},
		{createInstance<cmdLogs>, DAEMONCAP::BOTH, OSTYPE::LINUX},
		{createInstance<cmdOOB>, DAEMONCAP::BOTH, OSTYPE::LINUX},
		{createInstance<cmdGroup>, DAEMONCAP::DAEMON, OSTYPE::LINUX},
		{createInstance<cmdPolicy>, DAEMONCAP::DAEMON, OSTYPE::LINUX},
		{createInstance<cmdTopdown>, DAEMONCAP::DAEMON, OSTYPE::LINUX},
		{createInstance<cmdAgentSet>, DAEMONCAP::DAEMON, OSTYPE::LINUX},
		{createInstance<cmdAmcSensor>, DAEMONCAP::DAEMON, OSTYPE::LINUX},
	};

	OSTYPE currentOS = is_windows ? OSTYPE::WINDOWS : OSTYPE::LINUX;
	DBG("Is Daemon mode: %s\n", (curDaemonMode == DAEMONCAP::DAEMON) ? "true" : "false");
	arg.argc = argc;
	arg.argv = argv;

	for (const auto &entry : functionTable) {
		if ((entry.osType == OSTYPE::BOTH || entry.osType == currentOS) &&
			(entry.daemonCap == DAEMONCAP::BOTH || entry.daemonCap == curDaemonMode)) {
			/* Create an instance of the command and add it to the list */
			cmds *cmd = entry.createFunc();
			DBG("Adding %s to command list\n", cmd->getName());
			cmdList->push_back(cmd);
		}
	}

	/* If no command line args are provided, just print help message and exit */
	if (argc == 1) {
		help(cmdList);
		deleteList(cmdList);
		return 0;
	}

	/* Print out version info if -v command line arg specified */
	if (!STRCASECMP(argv[1], "-v") || !STRCASECMP(argv[1], "--version")) {
		printVersion(&arg);
		deleteList(cmdList);
		return 0;
	}

	/* Parse command line and run the command that the user wants */
	for (const auto &it : *cmdList) {
		if (!STRCASECMP(it->getName(), argv[1])) {
			/* If the second argument is -h or --help, then just print their help */
			if (argc > 2 && (!STRCASECMP(argv[2], "-h") || !STRCASECMP(argv[2], "--help"))) {
				it->help(FULL_HELP);
				deleteList(cmdList);
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
		help(cmdList);
	}

	deleteList(cmdList);
	return 0;
}
