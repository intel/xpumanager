/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

/**
 * @file smi_test.cpp
 * @brief Unit tests for the Name slot of the default xpu-smi view (cmd_smi.cpp).
 *
 * The GPU table is rendered from SmiDeviceStats filled in here, so no GPU or Level Zero
 * handle is needed. The TEST_CASE names state each covered behaviour.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef INFO
#undef INFO
#endif

#include <string>
std::string progName = "test";

#include "cmd_smi.h"
#include <algorithm>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#ifdef INFO
#undef INFO
#endif

namespace {

/**
 * @brief Lines of one GPU table row, split by column.
 */
struct DeviceRow
{
	std::vector<std::string> col0;
	std::vector<std::string> col1;
	std::vector<std::string> col2;
};

/**
 * @brief Split a rendered table line into its cells.
 *
 * @param[in] line One line of TableBuilder output.
 * @return The text of each cell without the padding space after its left border and
 *         without trailing spaces; empty when @p line is not a row of cells.
 */
std::vector<std::string> cellsOf(std::string_view line)
{
	std::vector<std::string> cells;
	if (!line.starts_with('|')) {
		return cells;
	}
	size_t start = 1;
	for (size_t end = line.find('|', start); end != std::string_view::npos; end = line.find('|', start)) {
		std::string_view cell = line.substr(start, end - start);
		cell.remove_prefix(std::min<size_t>(cell.size(), 1));
		cell = cell.substr(0, cell.find_last_not_of(' ') + 1);
		cells.emplace_back(cell);
		start = end + 1;
	}
	return cells;
}

/**
 * @brief Render the GPU table for devices with the given names and return their rows.
 *
 * @param[in] names Device name of each device, in device index order (at most 10).
 * @return One DeviceRow per device that the rendered table contains.
 */
std::vector<DeviceRow> renderRows(const std::vector<std::string> &names)
{
	std::vector<SmiDeviceStats> stats(names.size());
	for (size_t i = 0; i < names.size(); ++i) {
		stats[i].devIndex = static_cast<uint32_t>(i);
		stats[i].name = names[i];
		stats[i].pciBdf = "0000:0" + std::to_string(i) + ":00.0";
	}
	const std::string table = cmdSmi::buildGpuTable(stats, "1.0").toString();

	std::vector<DeviceRow> rows;
	bool inRow = false;
	for (size_t start = 0, end = 0; start < table.size(); start = end + 1) {
		end = std::min(table.find('\n', start), table.size());
		const std::vector<std::string> cells = cellsOf(std::string_view(table).substr(start, end - start));
		if (cells.size() != 3) {
			inRow = false; // a border, separator or banner line
			continue;
		}
		if (cells[1].starts_with("0000:")) {
			rows.emplace_back();
			inRow = true;
		}
		if (inRow) {
			rows.back().col0.push_back(cells[0]);
			rows.back().col1.push_back(cells[1]);
			rows.back().col2.push_back(cells[2]);
		}
	}
	return rows;
}

/**
 * @brief Build the first column-0 line expected for a device.
 *
 * @param[in] index Device index, 0-9.
 * @param[in] name  Short name, ASCII, at most 22 characters.
 * @return The index, the name padded to the 22-column slot, then "Off".
 */
std::string nameLine(uint32_t index, std::string_view name)
{
	return "  " + std::to_string(index) + "  " + std::string(name) + std::string(22 - name.size(), ' ') + "  Off";
}

/**
 * @brief Find the code point column at which "Off" ends a line.
 *
 * @param[in] line UTF-8 line ending in "Off".
 * @return Number of code points before the final "Off".
 */
size_t offColumn(std::string_view line)
{
	const std::string_view before = line.substr(0, line.rfind("Off"));
	return static_cast<size_t>(
		std::ranges::count_if(before, [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

} // namespace

TEST_SUITE("cmd_smi name slot")
{
	TEST_CASE("each name sits whole on one line, with Off under Persistence-M")
	{
		const auto rows = renderRows({"Intel(R) Arc(TM) Pro B60 Graphics", "Intel(R) Graphics [0xe223]",
									  "Intel(R) Arc(TM) B580 Graphics", "Intel(R) Data Center GPU Max 1550"});
		REQUIRE(rows.size() == 4);
		const std::vector<std::string> expected = {"Arc Pro B60", "Graphics [0xe223]", "Arc B580", "Max 1550"};
		for (uint32_t i = 0; i < 4; ++i) {
			CAPTURE(i);
			REQUIRE(rows[i].col0.size() == 2); // name line + fan/temp/power line
			CHECK(rows[i].col0[0] == nameLine(i, expected[i]));
			CHECK(offColumn(rows[i].col0[0]) == 29);
		}
	}

	TEST_CASE("a name still too wide wraps at a space, and the telemetry stays on the fan line")
	{
		const auto rows = renderRows({"Intel(R) Iris(R) Xe MAX Graphics [0x4905]"});
		REQUIRE(rows.size() == 1);
		REQUIRE(rows[0].col0.size() == 3);
		CHECK(rows[0].col0[0] == nameLine(0, "Iris Xe MAX Graphics"));
		CHECK(rows[0].col0[1] == "     [0x4905]");
		CHECK(rows[0].col1 == std::vector<std::string>{"0000:00:00.0      Off", "", "N/A"});
		CHECK(rows[0].col2.size() == 3);
		CHECK(rows[0].col2[1].empty());
	}

	TEST_CASE("a name of exactly 22 columns fits one line, and a longer word wraps over several")
	{
		const auto rows = renderRows({"Intel(R) Aaaa Bbbb Cccc Dddd Ee", "Intel(R) " + std::string(50, 'X')});
		REQUIRE(rows.size() == 2);
		CHECK(rows[0].col0.size() == 2);
		CHECK(rows[0].col0[0] == nameLine(0, "Aaaa Bbbb Cccc Dddd Ee"));
		REQUIRE(rows[1].col0.size() == 4);
		CHECK(rows[1].col0[0] == nameLine(1, std::string(22, 'X')));
		CHECK(rows[1].col0[2] == "     " + std::string(6, 'X'));
		CHECK(rows[1].col1 == std::vector<std::string>{"0000:01:00.0      Off", "", "", "N/A"});
	}

	TEST_CASE("Off stays under Persistence-M for a non-ASCII name")
	{
		// "\303\211lan" is "Elan" with an acute accent on the E: 4 code points in 5 bytes.
		// "\342\202\254" is the euro sign: 1 code point in 3 bytes.
		const auto rows = renderRows({"Intel(R) Arc(TM) B580 Graphics", "Intel \303\211lan Graphics",
									  "Intel(R) Arc(TM) B580 \342\202\254 Graphics"});
		REQUIRE(rows.size() == 3);
		CHECK(rows[1].col0[0].starts_with("  1  \303\211lan Graphics "));
		CHECK(rows[2].col0[0].starts_with("  2  Arc B580 \342\202\254 "));
		for (const auto &row : rows) {
			CHECK(offColumn(row.col0[0]) == 29);
		}
	}

	TEST_CASE("shortDeviceName drops the words every device name shares")
	{
		const std::vector<std::pair<std::string_view, std::string_view>> cases = {
			{"Intel(R) Arc(TM) Pro B60 Graphics", "Arc Pro B60"},
			{"Intel(R) Arc(TM) Pro A40/A50 Graphics", "Arc Pro A40/A50"},
			{"Intel(R) Arc(TM) A310 LP Graphics", "Arc A310 LP"},
			{"Intel(R) Arc(TM) A770 GPU", "Arc A770"},
			{"Intel(R) Graphics [0xe223]", "Graphics [0xe223]"},
			{"Intel(R) Data Center GPU Max 1550", "Max 1550"},
			{"Intel(R) Data Center GPU Flex 170V", "Flex 170V"},
			{"Intel(R) Data Center GPU", "Data Center GPU"},
			{"Intel(R) Iris(R) Xe MAX 100 Graphics", "Iris Xe MAX 100"},
			{"Intel(R) Server GPU SG-18M", "Server GPU SG-18M"},
			// No model number: the class word is all that says what the device is.
			{"Intel(R) Iris(R) Xe Graphics", "Iris Xe Graphics"},
			{"Intel(R) Arc(TM) Graphics", "Arc Graphics"},
			{"Intel(R) UHD Graphics 770", "UHD Graphics 770"},
			// Nothing but the vendor and the class: keep both.
			{"Intel(R) Graphics", "Intel Graphics"},
			{"Intel(R) GPU", "Intel GPU"},
			{"Intel", "Intel"},
			{"", ""},
		};
		for (const auto &[name, expected] : cases) {
			CAPTURE(name);
			CHECK(cmdSmi::shortDeviceName(name) == expected);
		}
	}

	TEST_CASE("shortDeviceName treats marks, whitespace and control characters only as separators")
	{
		CHECK(cmdSmi::shortDeviceName("Intel(R)Arc(TM) Pro B60 Graphics") == "Arc Pro B60");
		CHECK(cmdSmi::shortDeviceName("  Intel(R)\tArc(TM)  Pro\r\nB60 Graphics ") == "Arc Pro B60");
		CHECK(cmdSmi::shortDeviceName("Intel(R) Arc(TM)\177Pro\033B60 Graphics") == "Arc Pro B60");
		// U+00A0 no-break space, U+0085 and U+009B C1 controls; U+00A9 and a stray 0xC2 are text.
		CHECK(cmdSmi::shortDeviceName("Intel(R)\302\240Arc(TM)\302\205Pro\302\233B60\302\240Graphics") ==
			  "Arc Pro B60");
		CHECK(cmdSmi::shortDeviceName("Arc \302\251 B60 \302A") == "Arc \302\251 B60 \302A");
		CHECK(cmdSmi::shortDeviceName("Arc\302\200B60\302\237Pro\302\241") == "Arc B60 Pro\302\241");
		CHECK(cmdSmi::shortDeviceName("Arc B60\302") == "Arc B60\302");
		CHECK(cmdSmi::shortDeviceName("(R) (TM)") == "");
		CHECK(cmdSmi::shortDeviceName(" \t ") == "");
	}

	TEST_CASE("wrapDeviceName breaks at spaces and splits only a word wider than a line")
	{
		using Lines = std::vector<std::string>;
		CHECK(cmdSmi::wrapDeviceName("Aaaa Bbbb Cccc Dddd Ee", 22) == Lines{"Aaaa Bbbb Cccc Dddd Ee"});
		CHECK(cmdSmi::wrapDeviceName("Aaaa Bbbb Cccc Dddd Eee", 22) == Lines{"Aaaa Bbbb Cccc Dddd", "Eee"});
		CHECK(cmdSmi::wrapDeviceName(std::string(30, 'X'), 22) == Lines{std::string(22, 'X'), std::string(8, 'X')});
		CHECK(cmdSmi::wrapDeviceName("Ab " + std::string(30, 'X') + " Cd", 22) ==
			  Lines{"Ab", std::string(22, 'X'), std::string(8, 'X') + " Cd"});
		CHECK(cmdSmi::wrapDeviceName("", 22) == Lines{""});
		CHECK(cmdSmi::wrapDeviceName("ab", 0) == Lines{"a", "b"});

		// One code point each in 2, 3 and 4 bytes; a split never lands inside one.
		for (const std::string_view unit : {"\303\251", "\342\202\254", "\360\237\230\200"}) {
			CAPTURE(unit);
			std::string text;
			for (int i = 0; i < 23; ++i) {
				text += unit;
			}
			CHECK(cmdSmi::wrapDeviceName(text, 22) == Lines{text.substr(0, 22 * unit.size()), std::string(unit)});
		}
	}
}
