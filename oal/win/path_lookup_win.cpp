/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Windows implementation of the $PATH executable lookup helper. It lives in its
 * own translation unit, guarded so that on non-Windows toolchains it reduces to
 * nothing. The rest of win.cpp uses the Windows API unconditionally and cannot
 * be parsed off Windows; keeping this helper separate and self-guarded lets it
 * be analyzed on any platform.
 */

#include <cstdlib>
#include <string>
#include <vector>

#if defined(_WIN32) && defined(_MSC_VER)

#include <os.h>
#include <debug.h>

/**
 * @brief Report whether an executable named `name` is reachable via %PATH%.
 *
 * Uses SearchPath, which walks the standard search order (including the
 * directories in %PATH%) without spawning a process. Only files that match the
 * executable/script policy count: %PATHEXT% when the environment sets it, or a
 * built-in fallback (which includes .ps1) otherwise. A name that already
 * carries an extension is accepted only when that extension is part of the
 * policy, so a data file such as "tool.txt" is not treated as executable; a
 * bare name is tried against each policy extension so "iclg" matches
 * "iclg.exe" or "iclg.ps1". Note a .ps1 match cannot be launched directly by
 * CreateProcess; a caller that finds one must invoke it through powershell.exe.
 *
 * @param name Program name or path.
 * @return true if a matching executable or script is found, false otherwise.
 */
bool isExecutableInPath(const std::string &name)
{
	TRACING();
	if (name.empty()) {
		return false;
	}

	// The executable/script policy: %PATHEXT% when the environment sets it,
	// otherwise a built-in fallback that also covers PowerShell scripts. A
	// present %PATHEXT% reflects the machine's policy and takes precedence.
	std::string pathext = ".COM;.EXE;.BAT;.CMD;.PS1";
	char *value = nullptr;
	size_t length = 0;
	if (_dupenv_s(&value, &length, "PATHEXT") == 0 && value != nullptr) {
		pathext = value;
		free(value); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
	}

	std::vector<std::string> extensions;
	for (std::string::size_type start = 0; start <= pathext.size();) {
		const std::string::size_type end = pathext.find(';', start);
		std::string ext = pathext.substr(start, end == std::string::npos ? std::string::npos : end - start);
		if (!ext.empty()) {
			extensions.push_back(std::move(ext));
		}
		if (end == std::string::npos) {
			break;
		}
		start = end + 1;
	}

	// SearchPathA returns the length copied (or the required buffer size) on a
	// hit and 0 when nothing matches, so a positive result means found
	// regardless of whether the fixed buffer was large enough.
	auto onPath = [&name](const char *ext) {
		char buffer[MAX_PATH];
		return SearchPathA(nullptr, name.c_str(), ext, MAX_PATH, buffer, nullptr) > 0;
	};

	// An extension on the name counts only when it belongs to the policy, so
	// "tool.txt" is rejected while "iclg.exe" is searched verbatim. The dot must
	// be in the final component and not trailing.
	const std::string::size_type sep = name.find_last_of("\\/");
	const std::string::size_type dot = name.find_last_of('.');
	const bool hasExtension =
		dot != std::string::npos && dot + 1 < name.size() && (sep == std::string::npos || dot > sep);
	if (hasExtension) {
		const std::string ext = name.substr(dot);
		for (const std::string &candidate : extensions) {
			if (_stricmp(candidate.c_str(), ext.c_str()) == 0) {
				return onPath(nullptr);
			}
		}
		return false;
	}

	// A bare name: try each policy extension. An extensionless file on PATH is
	// deliberately not matched, as Windows cannot launch it as a program.
	for (const std::string &ext : extensions) {
		if (onPath(ext.c_str())) {
			return true;
		}
	}
	return false;
}

#endif // defined(_WIN32) && defined(_MSC_VER)
