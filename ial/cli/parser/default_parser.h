/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef DEFAULT_PARSER_H
#define DEFAULT_PARSER_H

#include "cli.h"
#include "cli_parser.h"
#include <string_view>
#include <vector>
#include <optional>
#include "cmds.h"
#include <memory>

// Default CLI parser — matches the existing xpu-smi / xpumcli behaviour.
// When no "compat" prefix is present this parser is selected.
class DefaultParser
{
	// Owns the rebuilt argv storage when subcommand dispatch requires flag stripping.
	// Must outlive any call to handleTopLevel so that args->argv remains valid until
	// the subcommand's run() returns in runCli().
	struct OwnedArgv
	{
		std::vector<std::string> args;
		std::vector<char *> argv;
	};
	OwnedArgv ownedArgv_;

public:
	// Display name used in usage lines and forwarded to the progName global.
	static std::string_view progName();

	// Banner printed at the top of --help output.
	static void printBanner();

	// Commands available through this parser. Diverges from CompatParser
	// once the two CLIs need different command sets or flags.
	static std::vector<function_entry> commandTable();

	// Handle top-level args that are not subcommand names.
	// Returns exit code if consumed; std::nullopt to fall through to dispatch.
	std::optional<int> handleTopLevel(arg_struct *args, const std::vector<std::unique_ptr<cmds>> &cmdList);

	// Extra option lines appended to the Options section of full help output.
	static void printExtraOptions();
};

static_assert(CliParser<DefaultParser>, "DefaultParser must satisfy the CliParser concept");

#endif // DEFAULT_PARSER_H
