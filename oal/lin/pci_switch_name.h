/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef OAL_LIN_PCI_SWITCH_NAME_H
#define OAL_LIN_PCI_SWITCH_NAME_H

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

/**
 * @brief Tests whether a PCI device name identifies it as a PCIe switch.
 *
 * Matches the whole word "switch" (case-insensitive) anywhere in @p name,
 * requiring a word boundary on both sides so that e.g. "SwitchNIC" is not
 * mistaken for a switch. Unlike the previous " Switch " (space-delimited)
 * test, this also matches names that *end* in "Switch" — such as the
 * Broadcom "PEX890xx PCIe Gen 5 Switch" (1000:c030) — which the old check
 * silently dropped, leaving those bridges uncounted in `topology` output.
 *
 * @param name Device or subsystem name from the PCI IDs database.
 * @return true if @p name contains "switch" as a standalone word.
 */
inline bool nameIndicatesSwitch(const std::string &name)
{
	static constexpr std::string_view kWord = "switch";
	const auto isWordChar = [](unsigned char c) { return (std::isalnum(c) != 0) || c == '_'; };

	std::string lower(name.size(), '\0');
	std::transform(name.begin(), name.end(), lower.begin(),
				   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	for (std::size_t pos = lower.find(kWord); pos != std::string::npos; pos = lower.find(kWord, pos + 1)) {
		const bool leftBoundary = (pos == 0) || !isWordChar(static_cast<unsigned char>(lower[pos - 1]));
		const std::size_t after = pos + kWord.size();
		const bool rightBoundary = (after >= lower.size()) || !isWordChar(static_cast<unsigned char>(lower[after]));
		if (leftBoundary && rightBoundary) {
			return true;
		}
	}
	return false;
}

#endif // OAL_LIN_PCI_SWITCH_NAME_H
