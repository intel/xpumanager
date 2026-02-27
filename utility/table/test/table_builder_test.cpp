/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "table_builder.h"
#include <boost/ut.hpp>
#include <sstream>

using namespace boost::ut;

int main()
{
	"TableBuilder basic construction"_test = [] {
		TableBuilder table;
		expect(table.empty() == true);
		expect(table.columnCount() == 0_u);
		expect(table.rowCount() == 0_u);
	};

	"TableBuilder add columns"_test = [] {
		TableBuilder table;
		table.addColumn("Name", 20, Align::Left).addColumn("Value", 10, Align::Right);

		expect(table.columnCount() == 2_u);
		expect(table.getColumnHeader(0) == "Name");
		expect(table.getColumnHeader(1) == "Value");
	};

	"TableBuilder add column with alignment only"_test = [] {
		TableBuilder table;
		table.addColumn("Header", Align::Center);
		expect(table.columnCount() == 1_u);
	};

	"TableBuilder add rows"_test = [] {
		TableBuilder table;
		table.addColumn("Name").addColumn("Value").addRow("Temperature", "75.5").addRow("Power", "150");

		expect(table.rowCount() == 2_u);
		expect(table.getCell(0, 0) == "Temperature");
		expect(table.getCell(0, 1) == "75.5");
		expect(table.getCell(1, 0) == "Power");
		expect(table.getCell(1, 1) == "150");
	};

	"TableBuilder add row with numeric types"_test = [] {
		TableBuilder table;
		table.addColumn("Int").addColumn("Float").addColumn("Bool").addRow(42, 3.14, true);

		expect(table.getCell(0, 0) == "42");
		expect(table.getCell(0, 1) == "3.140");
		expect(table.getCell(0, 2) == "true");
	};

	"TableBuilder find column by name"_test = [] {
		TableBuilder table;
		table.addColumn("Name").addColumn("Value");

		auto idx = table.findColumn("Name");
		expect(idx.has_value() == true);
		expect(*idx == 0_u);

		auto missing = table.findColumn("Missing");
		expect(missing.has_value() == false);
	};

	"TableBuilder find column case insensitive"_test = [] {
		TableBuilder table;
		table.addColumn("Name");

		auto idx = table.findColumnIcase("name");
		expect(idx.has_value() == true);
		expect(*idx == 0_u);

		auto idx2 = table.findColumnIcase("NAME");
		expect(idx2.has_value() == true);
	};

	"TableBuilder get cell by column name"_test = [] {
		TableBuilder table;
		table.addColumn("Name").addColumn("Value").addRow("Test", "123");

		expect(table.getCell(0, "Name") == "Test");
		expect(table.getCell(0, "Value") == "123");
	};

	"TableBuilder try_get_cell"_test = [] {
		TableBuilder table;
		table.addColumn("A").addColumn("B").addRow("X", "Y");

		auto cell = table.tryGetCell(0, 0);
		expect(cell.has_value() == true);
		expect(*cell == "X");

		auto missing = table.tryGetCell(10, 10);
		expect(missing.has_value() == false);
	};

	"TableBuilder set cell"_test = [] {
		TableBuilder table;
		table.addColumn("A").addColumn("B").addRow("X", "Y");

		table.setCell(0, 0, "Z");
		expect(table.getCell(0, 0) == "Z");
	};

	"TableBuilder auto sizing"_test = [] {
		TableBuilder table;
		table.addColumn("Short").addColumn("Long").addRow("A", "VeryLongValue").enableAutoSizing();

		auto str = table.toString();
		expect(!str.empty());
	};

	"TableBuilder output format"_test = [] {
		TableBuilder table;
		table.addColumn("Name");

		expect(table.getOutputFormat() == OutputFormat::Table);

		table.enableJsonOutput();
		expect(table.getOutputFormat() == OutputFormat::JSON);

		table.enableTableOutput();
		expect(table.getOutputFormat() == OutputFormat::Table);
	};

	"TableBuilder JSON output"_test = [] {
		TableBuilder table;
		table.addColumn("Name").addColumn("Value").addRow("Test", "123").enableJsonOutput();

		auto json = table.toString();
		expect(json.find("Name") != std::string::npos);
		expect(json.find("Value") != std::string::npos);
		expect(json.find("Test") != std::string::npos);
		expect(json.find("123") != std::string::npos);
	};

	"TableBuilder JSON with metadata"_test = [] {
		TableBuilder table;
		table.addColumn("Visible").addRow("Data").addJsonToLastRow("hidden_field", "secret");

		auto jsonStr = table.asJson();
		expect(jsonStr.find("hidden_field") != std::string::npos);
		expect(jsonStr.find("secret") != std::string::npos);
	};

	"TableBuilder sort by column"_test = [] {
		TableBuilder table;
		table.addColumn("Name").addRow("Charlie").addRow("Alice").addRow("Bob").sortByColumn(0, true);

		expect(table.getCell(0, 0) == "Alice");
		expect(table.getCell(1, 0) == "Bob");
		expect(table.getCell(2, 0) == "Charlie");
	};

	"TableBuilder sort descending"_test = [] {
		TableBuilder table;
		table.addColumn("Num").addRow("1").addRow("3").addRow("2").sortByColumn(0, false);

		expect(table.getCell(0, 0) == "3");
		expect(table.getCell(1, 0) == "2");
		expect(table.getCell(2, 0) == "1");
	};

	"TableBuilder sort by column name"_test = [] {
		TableBuilder table;
		table.addColumn("Value").addRow("Z").addRow("A").sortByColumn("Value");

		expect(table.getCell(0, 0) == "A");
		expect(table.getCell(1, 0) == "Z");
	};

	"TableBuilder remove column by index"_test = [] {
		TableBuilder table;
		table.addColumn("A").addColumn("B").addColumn("C").addRow("1", "2", "3");

		table.removeColumn(1);
		expect(table.columnCount() == 2_u);
		expect(table.getColumnHeader(0) == "A");
		expect(table.getColumnHeader(1) == "C");
		expect(table.getCell(0, 0) == "1");
		expect(table.getCell(0, 1) == "3");
	};

	"TableBuilder remove column by name"_test = [] {
		TableBuilder table;
		table.addColumn("Keep").addColumn("Remove").addRow("A", "B");

		table.removeColumn("Remove");
		expect(table.columnCount() == 1_u);
		expect(table.getColumnHeader(0) == "Keep");
	};

	"TableBuilder set column alignment"_test = [] {
		TableBuilder table;
		table.addColumn("A", Align::Left).setColumnAlignment(0, Align::Right);

		// No direct way to verify, but ensure it doesn't throw
		expect(table.columnCount() == 1_u);
	};

	"TableBuilder set column width"_test = [] {
		TableBuilder table;
		table.addColumn("A", 10).setColumnWidth(0, 20);

		expect(table.columnCount() == 1_u);
	};

	"TableBuilder validation - empty"_test = [] {
		TableBuilder table;
		auto result = table.validate();
		expect(result.valid == false);
		expect(!result.errors.empty());
	};

	"TableBuilder validation - with columns"_test = [] {
		TableBuilder table;
		table.addColumn("Test");
		auto result = table.validate();
		expect(result.valid == true);
	};

	"TableBuilder validation - duplicate headers"_test = [] {
		TableBuilder table;
		table.addColumn("Dup").addColumn("Dup");
		auto result = table.validate();
		expect(!result.warnings.empty());
	};

	"TableBuilder statistics"_test = [] {
		TableBuilder table;
		table.addColumn("A").addColumn("B").addRow("X", "Y").addRow("", "Z");

		auto stats = table.getStats();
		expect(stats.totalCells == 4_u);
		expect(stats.emptyCells == 1_u);
		expect(stats.fillRatio > 0.0 && stats.fillRatio < 1.0);
	};

	"TableBuilder clear rows"_test = [] {
		TableBuilder table;
		table.addColumn("A").addRow("X").addRow("Y");

		expect(table.rowCount() == 2_u);
		table.clearRows();
		expect(table.rowCount() == 0_u);
		expect(table.columnCount() == 1_u);
	};

	"TableBuilder clear all"_test = [] {
		TableBuilder table;
		table.addColumn("A").addRow("X");

		table.clear();
		expect(table.columnCount() == 0_u);
		expect(table.rowCount() == 0_u);
	};

	"TableBuilder add separator"_test = [] {
		TableBuilder table;
		table.addColumn("A").addRow("X").addSeparator().addRow("Y");

		expect(table.rowCount() == 3_u);
	};

	"TableBuilder add span row"_test = [] {
		TableBuilder table;
		table.addColumn("A").addColumn("B").addSpanRow("Spanning Text");

		expect(table.rowCount() == 1_u);
	};

	"TableBuilder multi-line row"_test = [] {
		TableBuilder table;
		table.addColumn("A").addColumn("B").addMultiLineRow({{"Line1A", "Line2A"}, {"Line1B", "Line2B"}});

		expect(table.rowCount() == 1_u);
	};

	"TableBuilder show row numbers"_test = [] {
		TableBuilder table;
		table.addColumn("A").addRow("X").showRowNumbers(true);

		auto str = table.toString();
		expect(str.find("#") != std::string::npos);
	};

	"TableBuilder configure"_test = [] {
		TableBuilder table;
		TableBuilder::TableConfig config;
		config.borderChar = '=';
		config.cornerChar = '*';
		config.noDataText = "Empty";

		table.addColumn("A").configure(config);
		auto str = table.toString();
		expect(str.find("*") != std::string::npos);
		expect(str.find("=") != std::string::npos);
	};

	"TableBuilder max cell width"_test = [] {
		TableBuilder table;
		table.addColumn("A").setMaxCellWidth(10).enableAutoSizing().addRow("VeryLongTextThatExceedsTheMaxWidth");

		auto str = table.toString();
		expect(!str.empty());
	};

	"TableBuilder last_row access"_test = [] {
		TableBuilder table;
		table.addColumn("A").addRow("X");

		auto &row = table.lastRow();
		expect(!row.cells.empty());
		expect(row.cells[0] == "X");
	};

	"TableBuilder filter rows"_test = [] {
		TableBuilder table;
		table.addColumn("Value").addRow("10").addRow("20").addRow("30").filterRows(
			[](const TableBuilder::Row &row) { return row.cells[0] != "20"; });

		expect(table.rowCount() == 2_u);
		expect(table.getCell(0, 0) == "10");
		expect(table.getCell(1, 0) == "30");
	};

	"TableBuilder print to stream"_test = [] {
		TableBuilder table;
		table.addColumn("A").addRow("X");

		std::ostringstream oss;
		table.printTo(oss);
		expect(!oss.str().empty());
	};

	"TableBuilder as_table vs as_json"_test = [] {
		TableBuilder table;
		table.addColumn("A").addRow("X");

		auto tableStr = table.asTable();
		auto jsonStr = table.asJson();

		expect(tableStr != jsonStr);
		expect(tableStr.find("+") != std::string::npos);
		expect(jsonStr.find("{") != std::string::npos);
	};

	"TableBuilder compact JSON"_test = [] {
		TableBuilder table;
		table.addColumn("A").addRow("X");

		auto compact = table.asJson(true);
		auto pretty = table.asJson(false);

		expect(compact.length() < pretty.length());
	};

	"TableBuilder alignment - left"_test = [] {
		TableBuilder table;
		table.addColumn("Left", 10, Align::Left).addRow("ABC").enableAutoSizing();

		auto str = table.toString();
		expect(!str.empty());
	};

	"TableBuilder alignment - right"_test = [] {
		TableBuilder table;
		table.addColumn("Right", 10, Align::Right).addRow("ABC").enableAutoSizing();

		auto str = table.toString();
		expect(!str.empty());
	};

	"TableBuilder alignment - center"_test = [] {
		TableBuilder table;
		table.addColumn("Center", 10, Align::Center).addRow("ABC").enableAutoSizing();

		auto str = table.toString();
		expect(!str.empty());
	};

	"TableBuilder error - invalid column width"_test = [] {
		TableBuilder table;
		expect(throws([&] { table.addColumn("A", 0); }));
	};

	"TableBuilder error - column index out of range"_test = [] {
		TableBuilder table;
		table.addColumn("A");
		expect(throws([&] { (void)table.getColumnHeader(10); }));
	};

	"TableBuilder error - row mismatch"_test = [] {
		TableBuilder table;
		table.addColumn("A").addColumn("B");
		expect(throws([&] { table.addRow("X"); }));
	};

	"TableBuilder error - cell out of range"_test = [] {
		TableBuilder table;
		table.addColumn("A");
		expect(throws([&] { (void)table.getCell(10, 0); }));
	};

	"TableBuilder error - no rows for last_row"_test = [] {
		TableBuilder table;
		table.addColumn("A");
		expect(throws([&] { (void)table.lastRow(); }));
	};

	"TableBuilder border styles"_test = [] {
		TableBuilder table;
		table.addColumn("A")
			.addRow("X")
			.addSeparator(BorderStyle::Heavy)
			.addRow("Y")
			.addSeparator(BorderStyle::Double)
			.addRow("Z");

		auto str = table.toString();
		expect(str.find("=") != std::string::npos);
	};

	"TableBuilder empty table output"_test = [] {
		TableBuilder table;
		table.addColumn("Empty");

		auto str = table.toString();
		expect(str.find("No data") != std::string::npos);
	};

	"TableBuilder UTF-8 characters"_test = [] {
		TableBuilder table;
		table.addColumn("Unicode").addRow("Hello 世界").enableAutoSizing();

		auto str = table.toString();
		expect(str.find("Hello") != std::string::npos);
	};

	"TableBuilder Row toString with bool"_test = [] {
		auto result = TableBuilder::Row::toString(true);
		expect(result == "true");

		result = TableBuilder::Row::toString(false);
		expect(result == "false");
	};

	"TableBuilder Row toString with numbers"_test = [] {
		auto result = TableBuilder::Row::toString(42);
		expect(result == "42");

		result = TableBuilder::Row::toString(3.14);
		expect(result.find("3.14") != std::string::npos);
	};

	"TableBuilder add_row_from_container"_test = [] {
		TableBuilder table;
		table.addColumn("A").addColumn("B");

		std::vector<std::string> row = {"X", "Y"};
		table.addRowFromContainer(row);

		expect(table.rowCount() == 1_u);
		expect(table.getCell(0, 0) == "X");
		expect(table.getCell(0, 1) == "Y");
	};

	"TableBuilder chaining methods"_test = [] {
		auto result = TableBuilder()
						  .addColumn("A")
						  .addColumn("B")
						  .addRow("1", "2")
						  .enableAutoSizing()
						  .enableTableOutput()
						  .toString();

		expect(!result.empty());
	};

	// ── New TableBuilder features ──────────────────────────────────────────────

	// Helper: count occurrences of a char on the line containing needle
	auto pipeCountOnLine = [](const std::string &str, std::string_view needle) -> int {
		auto pos = str.find(needle);
		if (pos == std::string::npos) return -1;
		auto lineStart = str.rfind('\n', pos);
		lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
		auto lineEnd = str.find('\n', pos);
		if (lineEnd == std::string::npos) lineEnd = str.size();
		return static_cast<int>(std::count(str.begin() + lineStart, str.begin() + lineEnd, '|'));
	};

	"TableBuilder setColumnExtraHeaders appears in output"_test = [&] {
		TableBuilder table;
		table.addColumn("Col1", Align::Left)
			.addColumn("Col2", Align::Left)
			.setColumnExtraHeaders(0, {"Sub-A"})
			.setColumnExtraHeaders(1, {"Sub-B"})
			.addRow("X", "Y")
			.enableAutoSizing();

		auto str = table.toString();
		expect(str.find("Sub-A") != std::string::npos) << "extra header Sub-A missing";
		expect(str.find("Sub-B") != std::string::npos) << "extra header Sub-B missing";
		expect(str.find("Col1") != std::string::npos) << "primary header Col1 missing";
		// Extra headers appear after the primary header in the output
		expect(str.find("Sub-A") > str.find("Col1")) << "extra header should follow primary header";
	};

	"TableBuilder setColumnExtraHeaders drives column width"_test = [] {
		// Extra header 19 chars wide; primary header is only 1 char — auto-size must use wider
		TableBuilder table;
		table.addColumn("A", Align::Left)
			.setColumnExtraHeaders(0, {"VeryLongExtraHeader"})
			.addRow("x")
			.enableAutoSizing();

		int w = table.getTotalWidth();
		// Column width >= 19 chars; total is at least " VeryLongExtraHeader |" = 1+19+2 = 22 + outer |
		expect(w >= 23) << "column not widened by extra header";
	};

	"TableBuilder setColumnExtraHeaders out-of-range throws"_test = [] {
		TableBuilder table;
		table.addColumn("A");
		expect(throws([&] { table.setColumnExtraHeaders(5, {"X"}); }));
	};

	"TableBuilder addPreHeaderSpanRow center appears before column headers"_test = [] {
		TableBuilder table;
		table.addColumn("A").addColumn("B")
			.addPreHeaderSpanRow("BannerText")
			.addRow("1", "2")
			.enableAutoSizing();

		auto str = table.toString();
		expect(str.find("BannerText") != std::string::npos) << "banner text missing";
		// Banner must appear before the column headers
		expect(str.find("BannerText") < str.find(" A ")) << "banner should appear before column header A";
	};

	"TableBuilder addPreHeaderSpanRow left alignment"_test = [] {
		TableBuilder table;
		table.addColumn("X").addColumn("Y")
			.addPreHeaderSpanRow("LeftBanner", BorderStyle::None, Align::Left)
			.addRow("a", "b")
			.enableAutoSizing();

		auto str = table.toString();
		expect(str.find("LeftBanner") != std::string::npos) << "left-aligned banner missing";
	};

	"TableBuilder addPreHeaderSpanRow BorderStyle::None omits inner separator"_test = [] {
		// With Normal style there's a +---+ between banner and headers; None omits it
		TableBuilder withSep;
		withSep.addColumn("A").addPreHeaderSpanRow("X", BorderStyle::Normal).addRow("v").enableAutoSizing();

		TableBuilder noSep;
		noSep.addColumn("A").addPreHeaderSpanRow("X", BorderStyle::None).addRow("v").enableAutoSizing();

		auto countCorners = [](const std::string &s) {
			return std::count(s.begin(), s.end(), '+');
		};
		expect(countCorners(noSep.toString()) < countCorners(withSep.toString()))
			<< "BorderStyle::None should produce fewer separator corners";
	};

	"TableBuilder suppressHeaderSeparator removes border line after headers"_test = [] {
		TableBuilder normal;
		normal.addColumn("A").addRow("X").enableAutoSizing();

		TableBuilder suppressed;
		suppressed.addColumn("A").addRow("X").suppressHeaderSeparator().enableAutoSizing();

		auto countLines = [](const std::string &s) {
			return std::count(s.begin(), s.end(), '\n');
		};
		expect(countLines(suppressed.toString()) < countLines(normal.toString()))
			<< "suppressed table should have fewer lines (missing header-separator line)";
	};

	"TableBuilder suppressHeaderColumnSeparators reduces pipes on header line"_test = [&] {
		// 3-column table: normal header has 4 pipes (|A|B|C|), suppressed has 2 (|A  B  C|)
		TableBuilder normal;
		normal.addColumn("Alpha").addColumn("Beta").addColumn("Gamma").addRow("1","2","3").enableAutoSizing();

		TableBuilder suppressed;
		suppressed.addColumn("Alpha").addColumn("Beta").addColumn("Gamma")
			.addRow("1","2","3").suppressHeaderColumnSeparators().enableAutoSizing();

		expect(pipeCountOnLine(normal.toString(), "Alpha") == 4)
			<< "normal 3-col header should have 4 pipes";
		expect(pipeCountOnLine(suppressed.toString(), "Alpha") == 2)
			<< "suppressed 3-col header should have only outer 2 pipes";
	};

	"TableBuilder suppressDataColumnSeparators reduces pipes on data line"_test = [&] {
		// 3-column table: normal data row has 4 pipes, suppressed has 2
		TableBuilder normal;
		normal.addColumn("A").addColumn("B").addColumn("C").addRow("foo","bar","baz").enableAutoSizing();

		TableBuilder suppressed;
		suppressed.addColumn("A").addColumn("B").addColumn("C")
			.addRow("foo","bar","baz").suppressDataColumnSeparators().enableAutoSizing();

		expect(pipeCountOnLine(normal.toString(), "foo") == 4)
			<< "normal 3-col data row should have 4 pipes";
		expect(pipeCountOnLine(suppressed.toString(), "foo") == 2)
			<< "suppressed 3-col data row should have only outer 2 pipes";
	};

	"TableBuilder suppressDataColumnSeparators multiline row"_test = [&] {
		TableBuilder suppressed;
		suppressed.addColumn("A").addColumn("B")
			.addMultiLineRow({{"line1a", "line2a"}, {"line1b", "line2b"}})
			.suppressDataColumnSeparators()
			.enableAutoSizing();

		// 2-col multi-line row: suppressed should have 2 pipes, not 3
		expect(pipeCountOnLine(suppressed.toString(), "line1a") == 2)
			<< "suppressed 2-col multi-line data row should have only outer 2 pipes";
	};

	"TableBuilder getTotalWidth is positive and grows with columns"_test = [] {
		TableBuilder narrow;
		narrow.addColumn("A").addRow("x").enableAutoSizing();

		TableBuilder wide;
		wide.addColumn("A").addColumn("B").addColumn("C").addRow("x","y","z").enableAutoSizing();

		expect(narrow.getTotalWidth() > 0) << "total width must be positive";
		expect(wide.getTotalWidth() > narrow.getTotalWidth()) << "more columns = wider table";
	};

	"TableBuilder padToWidth expands last column"_test = [] {
		TableBuilder table;
		table.addColumn("A").addColumn("B").addRow("x","y").enableAutoSizing();

		int original = table.getTotalWidth();
		int target   = original + 20;
		table.padToWidth(target);

		expect(table.getTotalWidth() == target) << "table width should equal target after padToWidth";
	};

	"TableBuilder padToWidth is a no-op when already wide enough"_test = [] {
		TableBuilder table;
		table.addColumn("A").addRow("x").enableAutoSizing();

		int original = table.getTotalWidth();
		table.padToWidth(original - 5);

		expect(table.getTotalWidth() == original) << "padToWidth must not shrink the table";
	};

	"TableBuilder clear also clears preHeaderRows"_test = [] {
		TableBuilder table;
		table.addColumn("A")
			.addPreHeaderSpanRow("MyBanner")
			.addRow("x")
			.enableAutoSizing();

		expect(table.toString().find("MyBanner") != std::string::npos) << "banner must appear before clear";

		table.clear();
		table.addColumn("B").addRow("y").enableAutoSizing();

		expect(table.toString().find("MyBanner") == std::string::npos)
			<< "banner must not appear after clear()";
	};

	return 0;
}
