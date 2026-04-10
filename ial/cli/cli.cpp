/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cli.h"
#include "version.h"
#include <cmd_agentset.h>
#include <cmd_amc.h>
#include <cmd_listgpu.h>
#include <cmd_config.h>
#include <cmd_diag.h>
#include <cmd_discovery.h>
#include <cmd_dump.h>
#include <cmd_group.h>
#include <cmd_health.h>
#include <cmd_log.h>
#include <cmd_policy.h>
#include <cmd_ps.h>
#include <cmd_stats.h>
#include <cmd_topdown.h>
#include <cmd_topology.h>
#include <cmd_updatefw.h>
#include <cmd_vgpu.h>
#include <cmd_smi.h>
#include <debug.h>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <os.h>
#include <vector>
#include <format>
#include <CLI/CLI.hpp>
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
	const std::string fullVersion = std::format("{}.{}.{}.{}", MAJOR, MINOR, PATCH, BUILD_NUMBER);
	std::string lzVersion;

	PRINT("{0:<{1}}CLI:\n", "", static_cast<int>(TITLE));
	PRINT("{0:<{1}}Version: {2}\n", "", static_cast<int>(HEADING), fullVersion.c_str());
	PRINT("{0:<{1}}Build ID: 8389eee7\n", "", static_cast<int>(HEADING));
	PRINT("{0:<{1}}\n", "", static_cast<int>(TITLE));
	PRINT("{0:<{1}}Service:\n", "", static_cast<int>(TITLE));
	PRINT("{0:<{1}}Version: {2}\n", "", static_cast<int>(HEADING), fullVersion.c_str());
	PRINT("{0:<{1}}Build ID: 8389eee7\n", "", static_cast<int>(HEADING));
	arg->sm.getLoaderVersion(&lzVersion);
	PRINT("{0:<{1}}Level Zero Version: {2}\n", "", static_cast<int>(HEADING), lzVersion.c_str());
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
	const std::string shortVersion = std::format("v{}.{}", MAJOR, MINOR);
	PRINT("Intel XPU Manager Command Line Interface -- {}\n", shortVersion.c_str());
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
	const std::string shortVersion = std::format("v{}.{}", MAJOR, MINOR);
	PRINT("Intel XPU System Management Interface -- {}\n", shortVersion.c_str());
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

	PRINT("Supported devices:\n");
	PRINT(" - Intel Arc B series GPU\n\n");

	PRINT("Usage: {} [Options]\n", progName.c_str());
	PRINT("  {} -v\n", progName.c_str());
	PRINT("  {} -h\n", progName.c_str());
	PRINT("  {} help\n", progName.c_str());
	PRINT("  {} discovery\n\n", progName.c_str());

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
void setPrintLvl(arg_struct *arg, LogLevel lvl)
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
	LogLevel dbglvl;
	bool priv = PRIVILEGECHECK();
	UNUSED_VAR(priv);

	// Get current debug level
	dbglvl = getDbgLvl();

	// If we are in < debug mode, set the debug level to NO_PRINT.
	// That's because we don't want to see all the sysman initialization messages in release mode
	if (dbglvl < LogLevel::DBG) {
		setPrintLvl(&arg, LogLevel::NO_PRINT);
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
		{createInstance<cmdGroup>, DAEMONCAP::DAEMON, OSTYPE::LINUX},
		{createInstance<cmdPolicy>, DAEMONCAP::DAEMON, OSTYPE::LINUX},
		{createInstance<cmdTopdown>, DAEMONCAP::DAEMON, OSTYPE::LINUX},
		{createInstance<cmdAgentSet>, DAEMONCAP::DAEMON, OSTYPE::LINUX},
		{createInstance<cmdAmc>, DAEMONCAP::BOTH, OSTYPE::LINUX},
		{createInstance<cmdListgpu>, DAEMONCAP::BOTH, OSTYPE::LINUX},
	};

	OSTYPE currentOS = is_windows ? OSTYPE::WINDOWS : OSTYPE::LINUX;
	DBG("Is Daemon mode: {}\n", (curDaemonMode == DAEMONCAP::DAEMON) ? "true" : "false");
	arg.argc = argc;
	arg.argv = argv;

	for (const auto &entry : functionTable) {
		if ((entry.osType == OSTYPE::BOTH || entry.osType == currentOS) &&
			(entry.daemonCap == DAEMONCAP::BOTH || entry.daemonCap == curDaemonMode)) {
			/* Create an instance of the command and add it to the list */
			cmds *cmd = entry.createFunc();
			DBG("Adding {} to command list\n", cmd->getName());
			cmdList->push_back(cmd);
		}
	}

	/* If no command line args are provided, show GPU status in xpu-smi style,
	 * or fall back to help text in daemon (xpumcli) mode. */
	if (argc == 1) {
		if (curDaemonMode == DAEMONCAP::DAEMONLESS) {
			cmdSmi smi;
			smi.run(&arg);
		} else {
			help(cmdList);
		}
		deleteList(cmdList);
		return 0;
	}

	/* Handle the "help" keyword (no hyphen) as a special top-level alias for --help */
	if (!STRCASECMP(argv[1], "help")) {
		if (argc > 2) {
			PRINT("The following argument was not expected: '{}'.\n", argv[2]);
			help(cmdList);
			deleteList(cmdList);
			return 1;
		}
		help(cmdList);
		deleteList(cmdList);
		return 0;
	}

	/* Use CLI11 to parse top-level flags (-h/--help, -v/--version).
	 * Subcommand names are not flags and are handled by the dispatch loop below. */
	if (argv[1][0] == '-') {
		const std::string shortVersion = std::format("v{}.{}", MAJOR, MINOR);
		CLI::App app{"", progName};
		app.set_help_flag("-h,--help", "Print this help message and exit");
		bool versionFlag = false;
		app.add_flag("-v,--version", versionFlag, "Display version information and exit");

		try {
			app.parse(std::vector<std::string>{argv[1]});
		} catch (const CLI::CallForHelp &) {
			if (argc > 2) {
				PRINT("The following argument was not expected: '{}'.\n", argv[2]);
				help(cmdList);
				deleteList(cmdList);
				return 1;
			}
			help(cmdList);
			deleteList(cmdList);
			return 0;
		} catch (const CLI::ParseError &) {
			PRINT("The following argument was not expected: '{}'.\n", argv[1]);
			help(cmdList);
			deleteList(cmdList);
			return 1;
		}

		if (versionFlag) {
			printVersion(&arg);
			deleteList(cmdList);
			return 0;
		}

		/* Unrecognised flag – report and exit */
		PRINT("The following argument was not expected: '{}'.\n", argv[1]);
		help(cmdList);
		deleteList(cmdList);
		return 1;
	}

	int exitCode = 0;

	/* Parse command line and run the command that the user wants */
	for (const auto &it : *cmdList) {
		if (!STRCASECMP(it->getName(), argv[1])) {
			/* Run the command; it handles its own --help via CLI11 */
			exitCode = (it->run(&arg) != 0) ? 1 : 0;
			found = true;
			/* Exit the loop once the command is found */
			break;
		}
	}

	/* If we can't parse the user's command line, then print help and signal error */
	if (!found) {
		help(cmdList);
		exitCode = 1;
	}

	deleteList(cmdList);
	return exitCode;
}
