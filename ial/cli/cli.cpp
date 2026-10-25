/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "parser/default_parser.h"
#include <bounded_call.h>
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
#include <cmd_query.h>
#include <cmd_stats.h>
#include <cmd_topology.h>
#include <cmd_updatefw.h>
#include <cmd_vgpu.h>
#include <debug.h>
#include <terminal.h>
#include <process_exit.h>
#include <watchdog_os.h>
#include <version_report.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
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
#include "utility/watchdog/timeout_option.h"
#include "utility/watchdog/watchdog.h"
std::string progName = "xpu-smi";

namespace {

// Filled once in main() before the watchdog thread starts, read only by the expiry handler, which
// must not allocate.
constinit std::array<char, 64> commandLabelText{};
constinit std::size_t commandLabelSize = 0;

/**
 * @brief Returns the command name the watchdog messages name, without allocating.
 *
 * @retval The label set by setCommandLabel(), empty before it is set.
 */
[[nodiscard]] std::string_view commandLabel() noexcept { return {commandLabelText.data(), commandLabelSize}; }

/**
 * @brief Stores the command name for the watchdog messages, cut to fit the fixed buffer.
 *
 * @param[in] label  Command name, such as "discovery".
 */
void setCommandLabel(std::string_view label) noexcept
{
	commandLabelSize = std::min(label.size(), commandLabelText.size());
	std::copy_n(label.data(), commandLabelSize, commandLabelText.data());
}

/**
 * @brief Reports a watchdog expiry on stderr and, on a timeout, ends the process with exit code 124.
 *
 * Runs on the watchdog thread while the main thread may be stuck holding the heap, stdio or
 * logger locks, so it only formats into a stack buffer and writes raw.
 *
 * @param[in] expiry  The pretimeout warning or the timeout.
 */
void onWatchdogExpiry(const xpum::watchdog::Expiry &expiry) noexcept
{
	std::array<char, 512> buf{};
	const bool fatal = expiry.kind == xpum::watchdog::ExpiryKind::Timeout;
	// The message must land on the normal screen, where it survives the exit, and the shell
	// must get back line input and echo if an interactive dump was waiting for a key.
	if (fatal) {
		leaveAlternateScreenForExit();
		restoreInputModeForExit();
	}
	writeStderrRaw(xpum::watchdog::formatMessage(expiry, commandLabel(), buf));
	if (fatal) {
		writeThreadReport();
		exitNow(xpum::watchdog::TIMEOUT_EXIT_CODE);
	}
}

} // namespace

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
		{.createFunc = createInstance<cmdQuery>, .osType = OSTYPE::BOTH},
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
 * @param[in,out] arg          Argument structure owning the driver instance.
 * @param[in]     mode         How much of Level Zero to initialize.
 * @param[in]     recoverable  A failure is not fatal: the command reaches devices without the driver.
 *
 * @retval std::nullopt  @p mode is DriverMode::None, init succeeded, or it failed for a @p recoverable command.
 * @retval -1            Init failed; the exit code main() should return.
 */
std::optional<int> requireDriver(arg_struct *arg, DriverMode mode, bool recoverable)
{
	if (mode == DriverMode::None) {
		return std::nullopt;
	}
	const auto phase = xpum::watchdog::enter("driver init");

	const LogLevel dbglvl = getDbgLvl();
	if (dbglvl < LogLevel::DBG) {
		setPrintLvl(arg, LogLevel::NO_PRINT);
	}
	const ze_result_t result = arg->sm.init(mode == DriverMode::SysmanOnly);
	setPrintLvl(arg, dbglvl);

	if (result != ZE_RESULT_SUCCESS) {
		if (recoverable) {
			INFO("Level Zero initialization failed; continuing with the device found on the PCI bus.\n");
			return std::nullopt;
		}
		PRINT("Sysman driver initialization failed.\n");
		return -1;
	}
	DBG("Sysman driver initialized successfully.\n");
	// The log's banner carries the host half of the -v report; the driver half
	// can only be read once init has run, since the loader reports no component
	// versions before then.
	if (getDbgLvl() >= LogLevel::DBG) {
		VersionReport report;
		addDriverVersionInfo(report, arg->sm);
		DBG("Runtime, after driver init:\n{}", formatDriverVersionReport(report));
	}
	return std::nullopt;
}

using ArgvView = std::span<char *const>;

/**
 * @brief Flushes the output and ends the process without running static destructors.
 *
 * For a process with a Level Zero call still abandoned on a worker thread: tearing the runtime
 * down under that call is undefined behaviour.
 *
 * @param[in] exitCode The process exit status.
 */
[[noreturn]] void exitWithoutTeardown(int exitCode) noexcept
{
	// The log sinks flush each record as they write it, under their own lock: an abandoned call
	// may still be logging, so they are not flushed from here.
	std::cout.flush();
	std::cerr.flush();
	std::fflush(nullptr);
	exitNow(exitCode);
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
	// Before anything that can reach the driver, so a hang in driver init is covered too.
	xpum::watchdog::TimeoutOption timeoutOption =
		xpum::watchdog::extractTimeoutOption(ArgvView{argv, static_cast<std::size_t>(argc)});
	if (timeoutOption.error) {
		ERR("{}\n", *timeoutOption.error);
		ERR("Run with --help for more information.\n");
		return 2;
	}
	std::vector<char *> argvStorage;
	argvStorage.reserve(timeoutOption.remaining.size() + 1);
	for (std::string &argument : timeoutOption.remaining) {
		argvStorage.push_back(argument.data());
	}
	argvStorage.push_back(nullptr);
	argc = static_cast<int>(timeoutOption.remaining.size());
	argv = argvStorage.data();
	setCommandLabel(argc > 1 ? std::string_view{argv[1]} : std::string_view{"default view"});
	const xpum::watchdog::Watchdog watchdog{timeoutOption.timeout, onWatchdogExpiry};

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

	// Debug logs are usually read away from the machine that wrote them; open each
	// one with the build and runtime that produced it.
	Logger::instance().setBanner(formatHostVersionReport(collectHostVersionReport()));

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

	const auto phase = xpum::watchdog::enter("command");
	const int exitCode = runCli(parser, &arg, currentOS);
	if (boundedCallsAbandoned()) {
		exitWithoutTeardown(exitCode);
	}
	return exitCode;
}
