/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_CRASHLOG_H
#define _CMD_CRASHLOG_H

#include "cmds.h"
#include "printer.h"

/**
 * @brief Text renderer for the crashlog command result.
 *
 * Renders the per-device result array built by cmdCrashlog into
 * human-readable status lines.
 */
class CrashlogTextPrinter : public TextPrinter
{
public:
	CrashlogTextPrinter();
	void print(nlohmann::ordered_json *jsonObj) override;
};

/**
 * @brief Drives the Intel Crash Log CLI (iclg) for crash log management.
 *
 * The command resolves an xpu-smi device index or PCI BDF to the
 * pmt:<bdf> source string that iclg expects, then shells out to iclg for
 * enable, disable, trigger, and extract operations.
 */
class cmdCrashlog : public cmds
{
public:
	cmdCrashlog() { name = "crashlog"; }
	~cmdCrashlog() {}

	void help(HELP helpType = FULL_HELP) override;
	int run(arg_struct *args) override;

	enum class Action
	{
		None,
		Enable,
		Disable,
		Trigger,
		Clear,
		Extract,
	};

	/// @brief iclg subcommand verb for @p action, or "" for Action::None.
	static const char *verbForAction(Action action);
};

#endif
