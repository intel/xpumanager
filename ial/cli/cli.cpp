/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "parser/default_parser.h"
#include "version.h"
#include <cmd_amc.h>
#include <cmd_listpciinfo.h>
#include <cmd_config.h>
#include <cmd_crashlog.h>
#include <cmd_discovery.h>
#include <cmd_dump.h>
#include <cmd_health.h>
#include <cmd_raslog.h>
#include <cmd_log.h>
#include <cmd_ps.h>
#include <cmd_stats.h>
#include <cmd_topology.h>
#include <cmd_updatefw.h>
#include <cmd_vgpu.h>
#include <debug.h>
#include <cctype>
#include <iostream>
#include <memory>
#include <optional>
#include <os.h>
#include <ranges>
#include <string>
#include <vector>
#include "utility/compat/format.h"
#include "utility/logger/filestream_sink.h"
#include "utility/logger/logger.h"
#include <string>
#include <vector>
std::string progName = "xpu-smi";

/**
 * @brief Prints the version information for both CLI and service components
 */
void printVersion(arg_struct *arg)
{
	const std::string fullVersion = xpum::compat::format("{}.{}.{}.{}", MAJOR, MINOR, PATCH, BUILD_NUMBER);
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
void printSubCommands(const std::vector<std::unique_ptr<cmds>> &cmdList)
{
	for (const auto &cmd : cmdList) {
		cmd->help(SHORT_HELP);
	}
}

/**
 * @brief Displays help information for daemon mode (xpumcli)
 */

// ---- Shared command table -----------------------------------------------
// Both parsers delegate here initially; each overrides commandTable() when
// they need a different command set.
std::vector<function_entry> defaultCommandTable()
{
	return {
		{.createFunc = createInstance<cmdDiscovery>, .osType = OSTYPE::BOTH},
		{.createFunc = createInstance<cmdTopology>, .osType = OSTYPE::BOTH},
		{.createFunc = createInstance<cmdUpdateFW>, .osType = OSTYPE::BOTH},
		{.createFunc = createInstance<cmdConfig>, .osType = OSTYPE::BOTH},
		{.createFunc = createInstance<cmdPs>, .osType = OSTYPE::LINUX},
		{.createFunc = createInstance<cmdVgpu>, .osType = OSTYPE::LINUX},
		{.createFunc = createInstance<cmdStats>, .osType = OSTYPE::BOTH},
		{.createFunc = createInstance<cmdDump>, .osType = OSTYPE::BOTH},
		{.createFunc = createInstance<cmdLogs>, .osType = OSTYPE::LINUX},
		{.createFunc = createInstance<cmdRasLog>, .osType = OSTYPE::LINUX},
		{.createFunc = createInstance<cmdHealth>, .osType = OSTYPE::LINUX},
		{.createFunc = createInstance<cmdAmc>, .osType = OSTYPE::LINUX},
		{.createFunc = createInstance<cmdListpciinfo>, .osType = OSTYPE::LINUX},
		{.createFunc = createInstance<cmdCrashlog>, .osType = OSTYPE::LINUX},
	};
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
 * @brief Initializes Level Zero to the extent a command declared it needs.
 *
 * Call at most once per process (driver::init is not idempotent), immediately
 * before the code that uses arg->sm, so help, AMC and other driverless paths
 * still work when Level Zero is absent or fails to initialize. Driver output is
 * suppressed during init unless the log level is DBG or higher.
 *
 * @param[in,out] arg   Argument structure owning the driver instance.
 * @param[in]     mode  How much of Level Zero to initialize.
 *
 * @retval std::nullopt  @p mode is DriverMode::None, or init succeeded.
 * @retval -1            Init failed; the exit code main() should return.
 */
std::optional<int> requireDriver(arg_struct *arg, DriverMode mode)
{
	if (mode == DriverMode::None) {
		return std::nullopt;
	}

	const LogLevel dbglvl = getDbgLvl();
	if (dbglvl < LogLevel::DBG) {
		setPrintLvl(arg, LogLevel::NO_PRINT);
	}
	const ze_result_t result = arg->sm.init(mode == DriverMode::SysmanOnly);
	setPrintLvl(arg, dbglvl);

	if (result != ZE_RESULT_SUCCESS) {
		PRINT("Sysman driver initialization failed.\n");
		return -1;
	}
	DBG("Sysman driver initialized successfully.\n");
	return std::nullopt;
}

using ArgvView = std::span<char *const>;

/**
 * @brief Main entry point for the application
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return Exit code (0 for success, non-zero for errors)
 */
int main(int argc, char *argv[])
{
	TRACING();
	const ArgvView args{argv, static_cast<std::size_t>(argc)};
	arg_struct arg;
	if (auto sv = getEnv("XPU_SMI_LOG_LEVEL")) {
		std::ranges::transform(*sv, sv->begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
		const std::string_view level{*sv};
		if (level == "TRACE") {
			Logger::instance().setLevel(LogLevel::TRACE);
		} else if (level == "DBG" || level == "DEBUG") {
			Logger::instance().setLevel(LogLevel::DBG);
		} else if (level == "INFO") {
			Logger::instance().setLevel(LogLevel::INFO);
		} else if (level == "ERR" || level == "ERROR") {
			Logger::instance().setLevel(LogLevel::ERR);
		}
	}
	bool priv = PRIVILEGECHECK();
	UNUSED_VAR(priv);

	// Sync the library's log level now; Level Zero itself is brought up per
	// command by requireDriver().
	setPrintLvl(&arg, getDbgLvl());

	const OSTYPE currentOS = is_windows ? OSTYPE::WINDOWS : OSTYPE::LINUX;
	/* Detect "compat" subparser prefix: xpu-smi compat <subcommand> [args...]
	 * Shift argv left to strip "compat" so the parser sees a normal argv. */
	if (args.size() >= 2 && std::string_view{args[1]} == "compat") {
		PRINT("compat is currently unimplemented");
		return 1;
	}

	arg.argc = argc;
	arg.argv = argv;
	DefaultParser parser;

	// Configure file logging for DefaultParser if -f/--file option was provided
	// Note: the logFilePath will be set during handleTopLevel() in runCli(),
	// but we need a hook to configure the sink after parsing.
	// For now, we'll configure it in the commands themselves if they need it.

	return runCli(parser, &arg, currentOS);
}
