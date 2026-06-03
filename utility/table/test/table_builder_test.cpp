/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "table_builder.h"
#include <sstream>
#include <chrono>

TEST_CASE("TableBuilder basic construction") {
    TableBuilder table;
    CHECK(table.empty() == true);
    CHECK(table.columnCount() == 0);
    CHECK(table.rowCount() == 0);
}

TEST_CASE("TableBuilder add columns") {
    TableBuilder table;
    table.addColumn("Name", 20, Align::Left).addColumn("Value", 10, Align::Right);

    CHECK(table.columnCount() == 2);
    CHECK(table.getColumnHeader(0) == "Name");
    CHECK(table.getColumnHeader(1) == "Value");
}

TEST_CASE("TableBuilder add column with alignment only") {
    TableBuilder table;
    table.addColumn("Header", Align::Center);
    CHECK(table.columnCount() == 1);
}

TEST_CASE("TableBuilder add rows") {
    TableBuilder table;
    table.addColumn("Name").addColumn("Value").addRow("Temperature", "75.5").addRow("Power", "150");

    CHECK(table.rowCount() == 2);
    CHECK(table.getCell(0, 0) == "Temperature");
    CHECK(table.getCell(0, 1) == "75.5");
    CHECK(table.getCell(1, 0) == "Power");
    CHECK(table.getCell(1, 1) == "150");
}

TEST_CASE("TableBuilder add row with numeric types") {
    TableBuilder table;
    table.addColumn("Int").addColumn("Float").addColumn("Bool").addRow(42, 3.14, true);

    CHECK(table.getCell(0, 0) == "42");
    CHECK(table.getCell(0, 1) == "3.140");
    CHECK(table.getCell(0, 2) == "true");
}

TEST_CASE("TableBuilder find column by name") {
    TableBuilder table;
    table.addColumn("Name").addColumn("Value");

    auto idx = table.findColumn("Name");
    CHECK(idx.has_value() == true);
    CHECK(*idx == 0);

    auto missing = table.findColumn("Missing");
    CHECK(missing.has_value() == false);
}

TEST_CASE("TableBuilder find column case insensitive") {
    TableBuilder table;
    table.addColumn("Name");

    auto idx = table.findColumnIcase("name");
    CHECK(idx.has_value() == true);
    CHECK(*idx == 0);

    auto idx2 = table.findColumnIcase("NAME");
    CHECK(idx2.has_value() == true);
}

TEST_CASE("TableBuilder get cell by column name") {
    TableBuilder table;
    table.addColumn("Name").addColumn("Value").addRow("Test", "123");

    CHECK(table.getCell(0, "Name") == "Test");
    CHECK(table.getCell(0, "Value") == "123");
}

TEST_CASE("TableBuilder try_get_cell") {
    TableBuilder table;
    table.addColumn("A").addColumn("B").addRow("X", "Y");

    auto cell = table.tryGetCell(0, 0);
    CHECK(cell.has_value() == true);
    CHECK(*cell == "X");

    auto missing = table.tryGetCell(10, 10);
    CHECK(missing.has_value() == false);
}

TEST_CASE("TableBuilder set cell") {
    TableBuilder table;
    table.addColumn("A").addColumn("B").addRow("X", "Y");

    table.setCell(0, 0, "Z");
    CHECK(table.getCell(0, 0) == "Z");
}

TEST_CASE("TableBuilder auto sizing") {
    TableBuilder table;
    table.addColumn("Short").addColumn("Long").addRow("A", "VeryLongValue").enableAutoSizing();

    auto str = table.toString();
    CHECK_FALSE(str.empty());
}

TEST_CASE("TableBuilder output format") {
    TableBuilder table;
    table.addColumn("Name");

    CHECK(table.getOutputFormat() == OutputFormat::Table);

    table.enableJsonOutput();
    CHECK(table.getOutputFormat() == OutputFormat::JSON);

    table.enableTableOutput();
    CHECK(table.getOutputFormat() == OutputFormat::Table);
}

TEST_CASE("TableBuilder JSON output") {
    TableBuilder table;
    table.addColumn("Name").addColumn("Value").addRow("Test", "123").enableJsonOutput();

    auto json = table.toString();
    CHECK(json.find("Name") != std::string::npos);
    CHECK(json.find("Value") != std::string::npos);
    CHECK(json.find("Test") != std::string::npos);
    CHECK(json.find("123") != std::string::npos);
}

TEST_CASE("TableBuilder JSON with metadata") {
    TableBuilder table;
    table.addColumn("Visible").addRow("Data").addJsonToLastRow("hidden_field", "secret");

    auto jsonStr = table.asJson();
    CHECK(jsonStr.find("hidden_field") != std::string::npos);
    CHECK(jsonStr.find("secret") != std::string::npos);
}

TEST_CASE("TableBuilder sort by column") {
    TableBuilder table;
    table.addColumn("Name").addRow("Charlie").addRow("Alice").addRow("Bob").sortByColumn(0, true);

    CHECK(table.getCell(0, 0) == "Alice");
    CHECK(table.getCell(1, 0) == "Bob");
    CHECK(table.getCell(2, 0) == "Charlie");
}

TEST_CASE("TableBuilder sort descending") {
    TableBuilder table;
    table.addColumn("Num").addRow("1").addRow("3").addRow("2").sortByColumn(0, false);

    CHECK(table.getCell(0, 0) == "3");
    CHECK(table.getCell(1, 0) == "2");
    CHECK(table.getCell(2, 0) == "1");
}

TEST_CASE("TableBuilder sort by column name") {
    TableBuilder table;
    table.addColumn("Value").addRow("Z").addRow("A").sortByColumn("Value");

    CHECK(table.getCell(0, 0) == "A");
    CHECK(table.getCell(1, 0) == "Z");
}

TEST_CASE("TableBuilder remove column by index") {
    TableBuilder table;
    table.addColumn("A").addColumn("B").addColumn("C").addRow("1", "2", "3");

    table.removeColumn(1);
    CHECK(table.columnCount() == 2);
    CHECK(table.getColumnHeader(0) == "A");
    CHECK(table.getColumnHeader(1) == "C");
    CHECK(table.getCell(0, 0) == "1");
    CHECK(table.getCell(0, 1) == "3");
}

TEST_CASE("TableBuilder remove column by name") {
    TableBuilder table;
    table.addColumn("Keep").addColumn("Remove").addRow("A", "B");

    table.removeColumn("Remove");
    CHECK(table.columnCount() == 1);
    CHECK(table.getColumnHeader(0) == "Keep");
}

TEST_CASE("TableBuilder set column alignment") {
    TableBuilder table;
    table.addColumn("A", Align::Left).setColumnAlignment(0, Align::Right);

    // No direct way to verify, but ensure it doesn't throw
    CHECK(table.columnCount() == 1);
}

TEST_CASE("TableBuilder set column width") {
    TableBuilder table;
    table.addColumn("A", 10).setColumnWidth(0, 20);

    CHECK(table.columnCount() == 1);
}

TEST_CASE("TableBuilder validation - empty") {
    TableBuilder table;
    auto result = table.validate();
    CHECK(result.valid == false);
    CHECK_FALSE(result.errors.empty());
}

TEST_CASE("TableBuilder validation - with columns") {
    TableBuilder table;
    table.addColumn("Test");
    auto result = table.validate();
    CHECK(result.valid == true);
}

TEST_CASE("TableBuilder validation - duplicate headers") {
    TableBuilder table;
    table.addColumn("Dup").addColumn("Dup");
    auto result = table.validate();
    CHECK_FALSE(result.warnings.empty());
}

TEST_CASE("TableBuilder statistics") {
    TableBuilder table;
    table.addColumn("A").addColumn("B").addRow("X", "Y").addRow("", "Z");

    auto stats = table.getStats();
    CHECK(stats.totalCells == 4);
    CHECK(stats.emptyCells == 1);
    CHECK(stats.fillRatio > 0.0);
    CHECK(stats.fillRatio < 1.0);
}

TEST_CASE("TableBuilder clear rows") {
    TableBuilder table;
    table.addColumn("A").addRow("X").addRow("Y");

    CHECK(table.rowCount() == 2);
    table.clearRows();
    CHECK(table.rowCount() == 0);
    CHECK(table.columnCount() == 1);
}

TEST_CASE("TableBuilder clear all") {
    TableBuilder table;
    table.addColumn("A").addRow("X");

    table.clear();
    CHECK(table.columnCount() == 0);
    CHECK(table.rowCount() == 0);
}

TEST_CASE("TableBuilder add separator") {
    TableBuilder table;
    table.addColumn("A").addRow("X").addSeparator().addRow("Y");

    CHECK(table.rowCount() == 3);
}

TEST_CASE("TableBuilder add span row") {
    TableBuilder table;
    table.addColumn("A").addColumn("B").addSpanRow("Spanning Text");

    CHECK(table.rowCount() == 1);
}

TEST_CASE("TableBuilder multi-line row") {
    TableBuilder table;
    table.addColumn("A").addColumn("B").addMultiLineRow({{"Line1A", "Line2A"}, {"Line1B", "Line2B"}});

    CHECK(table.rowCount() == 1);
}

TEST_CASE("TableBuilder show row numbers") {
    TableBuilder table;
    table.addColumn("A").addRow("X").showRowNumbers(true);

    auto str = table.toString();
    CHECK(str.find("#") != std::string::npos);
}

TEST_CASE("TableBuilder configure") {
    TableBuilder table;
    TableBuilder::TableConfig config;
    config.borderChar = '=';
    config.cornerChar = '*';
    config.noDataText = "Empty";

    table.addColumn("A").configure(config);
    auto str = table.toString();
    CHECK(str.find("*") != std::string::npos);
    CHECK(str.find("=") != std::string::npos);
}

TEST_CASE("TableBuilder max cell width") {
    TableBuilder table;
    table.addColumn("A").setMaxCellWidth(10).enableAutoSizing().addRow("VeryLongTextThatExceedsTheMaxWidth");

    auto str = table.toString();
    CHECK_FALSE(str.empty());
}

TEST_CASE("TableBuilder last_row access") {
    TableBuilder table;
    table.addColumn("A").addRow("X");

    auto& row = table.lastRow();
    CHECK_FALSE(row.cells.empty());
    CHECK(row.cells[0] == "X");
}

TEST_CASE("TableBuilder filter rows") {
    TableBuilder table;
    table.addColumn("Value").addRow("10").addRow("20").addRow("30").filterRows(
        [](const TableBuilder::Row& row) { return row.cells[0] != "20"; });

    CHECK(table.rowCount() == 2);
    CHECK(table.getCell(0, 0) == "10");
    CHECK(table.getCell(1, 0) == "30");
}

TEST_CASE("TableBuilder print to stream") {
    TableBuilder table;
    table.addColumn("A").addRow("X");

    std::ostringstream oss;
    table.printTo(oss);
    CHECK_FALSE(oss.str().empty());
}

TEST_CASE("TableBuilder as_table vs as_json") {
    auto table = TableBuilder::bordered();
    table.addColumn("A").addRow("X");

    auto tableStr = table.asTable();
    auto jsonStr = table.asJson();

    CHECK(tableStr != jsonStr);
    CHECK(tableStr.find("+") != std::string::npos);
    CHECK(jsonStr.find("{") != std::string::npos);
}

TEST_CASE("TableBuilder compact JSON") {
    TableBuilder table;
    table.addColumn("A").addRow("X");

    auto compact = table.asJson(true);
    auto pretty = table.asJson(false);

    CHECK(compact.length() < pretty.length());
}

TEST_CASE("TableBuilder alignment - left") {
    TableBuilder table;
    table.addColumn("Left", 10, Align::Left).addRow("ABC").enableAutoSizing();

    auto str = table.toString();
    CHECK_FALSE(str.empty());
}

TEST_CASE("TableBuilder alignment - right") {
    TableBuilder table;
    table.addColumn("Right", 10, Align::Right).addRow("ABC").enableAutoSizing();

    auto str = table.toString();
    CHECK_FALSE(str.empty());
}

TEST_CASE("TableBuilder alignment - center") {
    TableBuilder table;
    table.addColumn("Center", 10, Align::Center).addRow("ABC").enableAutoSizing();

    auto str = table.toString();
    CHECK_FALSE(str.empty());
}

TEST_CASE("TableBuilder error - invalid column width") {
    TableBuilder table;
    CHECK_THROWS(table.addColumn("A", 0));
}

TEST_CASE("TableBuilder error - column index out of range") {
    TableBuilder table;
    table.addColumn("A");
    CHECK_THROWS((void)table.getColumnHeader(10));
}

TEST_CASE("TableBuilder error - row mismatch") {
    TableBuilder table;
    table.addColumn("A").addColumn("B");
    CHECK_THROWS(table.addRow("X"));
}

TEST_CASE("TableBuilder error - cell out of range") {
    TableBuilder table;
    table.addColumn("A");
    CHECK_THROWS((void)table.getCell(10, 0));
}

TEST_CASE("TableBuilder error - no rows for last_row") {
    TableBuilder table;
    table.addColumn("A");
    CHECK_THROWS((void)table.lastRow());
}

TEST_CASE("TableBuilder border styles") {
    TableBuilder table;
    table.addColumn("A")
        .addRow("X")
        .addSeparator(BorderStyle::Heavy)
        .addRow("Y")
        .addSeparator(BorderStyle::Double)
        .addRow("Z");

    auto str = table.toString();
    CHECK(str.find("=") != std::string::npos);
}

TEST_CASE("TableBuilder empty table output") {
    TableBuilder table;
    table.addColumn("Empty");

    auto str = table.toString();
    CHECK(str.find("No data") != std::string::npos);
}

TEST_CASE("TableBuilder UTF-8 characters") {
    TableBuilder table;
    table.addColumn("Unicode").addRow("Hello 世界").enableAutoSizing();

    auto str = table.toString();
    CHECK(str.find("Hello") != std::string::npos);
}

TEST_CASE("TableBuilder Row toString with bool") {
    auto result = TableBuilder::Row::toString(true);
    CHECK(result == "true");

    result = TableBuilder::Row::toString(false);
    CHECK(result == "false");
}

TEST_CASE("TableBuilder Row toString with numbers") {
    auto result = TableBuilder::Row::toString(42);
    CHECK(result == "42");

    result = TableBuilder::Row::toString(3.14);
    CHECK(result.find("3.14") != std::string::npos);
}

TEST_CASE("TableBuilder add_row_from_container") {
    TableBuilder table;
    table.addColumn("A").addColumn("B");

    std::vector<std::string> row = {"X", "Y"};
    table.addRowFromContainer(row);

    CHECK(table.rowCount() == 1);
    CHECK(table.getCell(0, 0) == "X");
    CHECK(table.getCell(0, 1) == "Y");
}

TEST_CASE("TableBuilder chaining methods") {
    auto result = TableBuilder()
                      .addColumn("A")
                      .addColumn("B")
                      .addRow("1", "2")
                      .enableAutoSizing()
                      .enableTableOutput()
                      .toString();

    CHECK_FALSE(result.empty());
}

// Helper function for counting pipes on a line
namespace {
    int pipeCountOnLine(const std::string& str, std::string_view needle) {
        auto pos = str.find(needle);
        if (pos == std::string::npos) return -1;
        auto lineStart = str.rfind('\n', pos);
        lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
        auto lineEnd = str.find('\n', pos);
        if (lineEnd == std::string::npos) lineEnd = str.size();
        return static_cast<int>(std::count(str.begin() + lineStart, str.begin() + lineEnd, '|'));
    }
}

TEST_CASE("TableBuilder setColumnExtraHeaders appears in output") {
    TableBuilder table;
    table.addColumn("Col1", Align::Left)
        .addColumn("Col2", Align::Left)
        .setColumnExtraHeaders(0, {"Sub-A"})
        .setColumnExtraHeaders(1, {"Sub-B"})
        .addRow("X", "Y")
        .enableAutoSizing();

    auto str = table.toString();
    CHECK_MESSAGE(str.find("Sub-A") != std::string::npos, "extra header Sub-A missing");
    CHECK_MESSAGE(str.find("Sub-B") != std::string::npos, "extra header Sub-B missing");
    CHECK_MESSAGE(str.find("Col1") != std::string::npos, "primary header Col1 missing");
    CHECK_MESSAGE(str.find("Sub-A") > str.find("Col1"), "extra header should follow primary header");
}

TEST_CASE("TableBuilder setColumnExtraHeaders drives column width") {
    TableBuilder table;
    table.addColumn("A", Align::Left)
        .setColumnExtraHeaders(0, {"VeryLongExtraHeader"})
        .addRow("x")
        .enableAutoSizing();

    int w = table.getTotalWidth();
    CHECK_MESSAGE(w >= 23, "column not widened by extra header");
}

TEST_CASE("TableBuilder setColumnExtraHeaders out-of-range throws") {
    TableBuilder table;
    table.addColumn("A");
    CHECK_THROWS(table.setColumnExtraHeaders(5, {"X"}));
}

TEST_CASE("TableBuilder addPreHeaderSpanRow center appears before column headers") {
    TableBuilder table;
    table.addColumn("A").addColumn("B")
        .addPreHeaderSpanRow("BannerText")
        .addRow("1", "2")
        .enableAutoSizing();

    auto str = table.toString();
    CHECK_MESSAGE(str.find("BannerText") != std::string::npos, "banner text missing");
    CHECK_MESSAGE(str.find("BannerText") < str.find(" A "), "banner should appear before column header A");
}

TEST_CASE("TableBuilder addPreHeaderSpanRow left alignment") {
    TableBuilder table;
    table.addColumn("X").addColumn("Y")
        .addPreHeaderSpanRow("LeftBanner", BorderStyle::None, Align::Left)
        .addRow("a", "b")
        .enableAutoSizing();

    auto str = table.toString();
    CHECK_MESSAGE(str.find("LeftBanner") != std::string::npos, "left-aligned banner missing");
}

TEST_CASE("TableBuilder addPreHeaderSpanRow BorderStyle::None omits inner separator") {
    auto withSep = TableBuilder::bordered();
    withSep.addColumn("A").addPreHeaderSpanRow("X", BorderStyle::Normal).addRow("v").enableAutoSizing();

    auto noSep = TableBuilder::bordered();
    noSep.addColumn("A").addPreHeaderSpanRow("X", BorderStyle::None).addRow("v").enableAutoSizing();

    auto countCorners = [](const std::string& s) {
        return std::count(s.begin(), s.end(), '+');
    };
    CHECK_MESSAGE(countCorners(noSep.toString()) < countCorners(withSep.toString()),
                  "BorderStyle::None should produce fewer separator corners");
}

TEST_CASE("TableBuilder suppressHeaderSeparator removes border line after headers") {
    auto normal = TableBuilder::bordered();
    normal.addColumn("A").addRow("X").enableAutoSizing();

    auto suppressed = TableBuilder::bordered();
    suppressed.addColumn("A").addRow("X").suppressHeaderSeparator().enableAutoSizing();

    auto countLines = [](const std::string& s) {
        return std::count(s.begin(), s.end(), '\n');
    };
    CHECK_MESSAGE(countLines(suppressed.toString()) < countLines(normal.toString()),
                  "suppressed table should have fewer lines (missing header-separator line)");
}

TEST_CASE("TableBuilder suppressHeaderColumnSeparators reduces pipes on header line") {
    auto normal = TableBuilder::bordered();
    normal.addColumn("Alpha").addColumn("Beta").addColumn("Gamma").addRow("1", "2", "3").enableAutoSizing();

    auto suppressed = TableBuilder::bordered();
    suppressed.addColumn("Alpha").addColumn("Beta").addColumn("Gamma")
        .addRow("1", "2", "3").suppressHeaderColumnSeparators().enableAutoSizing();

    CHECK_MESSAGE(pipeCountOnLine(normal.toString(), "Alpha") == 4,
                  "normal 3-col header should have 4 pipes");
    CHECK_MESSAGE(pipeCountOnLine(suppressed.toString(), "Alpha") == 2,
                  "suppressed 3-col header should have only outer 2 pipes");
}

TEST_CASE("TableBuilder suppressDataColumnSeparators reduces pipes on data line") {
    auto normal = TableBuilder::bordered();
    normal.addColumn("A").addColumn("B").addColumn("C").addRow("foo", "bar", "baz").enableAutoSizing();

    auto suppressed = TableBuilder::bordered();
    suppressed.addColumn("A").addColumn("B").addColumn("C")
        .addRow("foo", "bar", "baz").suppressDataColumnSeparators().enableAutoSizing();

    CHECK_MESSAGE(pipeCountOnLine(normal.toString(), "foo") == 4,
                  "normal 3-col data row should have 4 pipes");
    CHECK_MESSAGE(pipeCountOnLine(suppressed.toString(), "foo") == 2,
                  "suppressed 3-col data row should have only outer 2 pipes");
}

TEST_CASE("TableBuilder suppressDataColumnSeparators multiline row") {
    auto suppressed = TableBuilder::bordered();
    suppressed.addColumn("A").addColumn("B")
        .addMultiLineRow({{"line1a", "line2a"}, {"line1b", "line2b"}})
        .suppressDataColumnSeparators()
        .enableAutoSizing();

    CHECK_MESSAGE(pipeCountOnLine(suppressed.toString(), "line1a") == 2,
                  "suppressed 2-col multi-line data row should have only outer 2 pipes");
}

TEST_CASE("TableBuilder getTotalWidth is positive and grows with columns") {
    TableBuilder narrow;
    narrow.addColumn("A").addRow("x").enableAutoSizing();

    TableBuilder wide;
    wide.addColumn("A").addColumn("B").addColumn("C").addRow("x", "y", "z").enableAutoSizing();

    CHECK_MESSAGE(narrow.getTotalWidth() > 0, "total width must be positive");
    CHECK_MESSAGE(wide.getTotalWidth() > narrow.getTotalWidth(), "more columns = wider table");
}

TEST_CASE("TableBuilder padToWidth expands last column") {
    TableBuilder table;
    table.addColumn("A").addColumn("B").addRow("x", "y").enableAutoSizing();

    int original = table.getTotalWidth();
    int target = original + 20;
    table.padToWidth(target);

    CHECK_MESSAGE(table.getTotalWidth() == target, "table width should equal target after padToWidth");
}

TEST_CASE("TableBuilder padToWidth is a no-op when already wide enough") {
    TableBuilder table;
    table.addColumn("A").addRow("x").enableAutoSizing();

    int original = table.getTotalWidth();
    table.padToWidth(original - 5);

    CHECK_MESSAGE(table.getTotalWidth() == original, "padToWidth must not shrink the table");
}

TEST_CASE("TableBuilder clear also clears preHeaderRows") {
    TableBuilder table;
    table.addColumn("A")
        .addPreHeaderSpanRow("MyBanner")
        .addRow("x")
        .enableAutoSizing();

    CHECK_MESSAGE(table.toString().find("MyBanner") != std::string::npos,
                  "banner must appear before clear");

    table.clear();
    table.addColumn("B").addRow("y").enableAutoSizing();

    CHECK_MESSAGE(table.toString().find("MyBanner") == std::string::npos,
                  "banner must not appear after clear()");
}