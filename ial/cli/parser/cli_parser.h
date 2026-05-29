/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef _CLI_PARSER_H
#define _CLI_PARSER_H

#include "../cli.h"
#include <debug.h>
#include <algorithm>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Forward-declare shared utilities defined in cli.cpp.
void printSubCommands(const std::vector<std::unique_ptr<cmds>> &cmdList);
void printVersion(arg_struct *arg);
std::vector<function_entry> defaultCommandTable();

template <typename T> std::unique_ptr<cmds> createInstance() { return std::make_unique<T>(); }

// ---- CliParser concept --------------------------------------------------
// Defines the minimal interface each CLI parser variant must satisfy.
// Parsers are independent types — no inheritance required.
//
// Prefer concepts over CRTP here: parsers are open/independent structs;
// the interface is visible in one place; misimplementations produce clean
// static_assert messages rather than deep template instantiation noise.
template <typename P>
concept CliParser = requires(P &parser, arg_struct *args, const std::vector<std::unique_ptr<cmds>> &cmdList) {
	// Display name forwarded to the global progName (used by command help text)
	{ parser.progName() } -> std::convertible_to<std::string_view>;
	// Banner paragraph printed at the top of full --help output
	{ parser.printBanner() } -> std::same_as<void>;
	// Ordered list of commands this parser exposes (may differ per parser)
	{ parser.commandTable() } -> std::same_as<std::vector<function_entry>>;
	// Handle args that are not subcommand names (-h, --help, -v, "help", no-arg default).
	// Returns exit code if the argument was consumed; std::nullopt to fall through to dispatch.
	{ parser.handleTopLevel(args, cmdList) } -> std::same_as<std::optional<int>>;
	// Extra option lines appended to the Options section of full help output.
	{ parser.printExtraOptions() } -> std::same_as<void>;
};

// ---- Shared: build cmdList + dispatchMap --------------------------------
template <CliParser P> auto buildDispatch(P &parser, OSTYPE currentOS)
{
	std::vector<std::unique_ptr<cmds>> cmdList;
	std::unordered_map<std::string_view, cmds *> dispatchMap;
	const auto table = parser.commandTable();
	dispatchMap.reserve(table.size() * 2);

	for (const auto &entry : table) {
		if (entry.osType == OSTYPE::BOTH || entry.osType == currentOS) {
			auto cmd = entry.createFunc();
			dispatchMap.emplace(cmd->getName(), cmd.get());
			cmdList.push_back(std::move(cmd));
		}
	}
	return std::pair{std::move(cmdList), std::move(dispatchMap)};
}

// ---- Shared: full help output -------------------------------------------
// Called by handleTopLevel implementations and by runCli on unknown subcommands.
template <CliParser P> void printFullHelp(P &parser, const std::vector<std::unique_ptr<cmds>> &cmdList)
{
	parser.printBanner();
	PRINT("Supported devices:\n");
	PRINT(" - Intel Arc B series GPU\n\n");

	const std::string name{parser.progName()};
	PRINT("Usage: {} [Options]\n", name);
	PRINT("  {} -v\n", name);
	PRINT("  {} -h\n", name);
	PRINT("  {} help\n", name);
	PRINT("  {} discovery\n\n", name);

	PRINT("Options:\n");
	PRINT("  -h,--help                   Print this help message and exit\n");
	PRINT("  -v,--version                Display version information and exit\n");
	parser.printExtraOptions();
	PRINT("\n");

	PRINT("Subcommands:\n");
	printSubCommands(cmdList);
}

// ---- Shared: full dispatch loop -----------------------------------------
// All parser variants share this loop. The only runtime variation is in
// handleTopLevel() and commandTable(), which each parser implements itself.
template <CliParser P> int runCli(P &parser, arg_struct *args, OSTYPE currentOS)
{
	// Keep the global in sync so command help text uses the right name.
	progName = std::string{parser.progName()};

	auto [cmdList, dispatchMap] = buildDispatch(parser, currentOS);

	// Let the parser handle flags, "help" keyword, and no-arg default.
	if (auto rc = parser.handleTopLevel(args, cmdList); rc.has_value()) {
		return *rc;
	}

	// Normalize to lowercase; alias keys are stored lowercase.
	std::string subcmd{args->argv[1]};
	std::ranges::transform(subcmd, subcmd.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	if (auto match = dispatchMap.find(subcmd); match != dispatchMap.end()) {
		return (match->second->run(args) != 0) ? 1 : 0;
	}

	// Unknown subcommand — show help and signal error.
	printFullHelp(parser, cmdList);
	return 1;
}

#endif // _CLI_PARSER_H
