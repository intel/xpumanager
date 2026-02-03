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

	return 0;
}
