/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef TABLE_BUILDER_H
#define TABLE_BUILDER_H

#include "nlohmann/json_fwd.hpp"
#include <algorithm>
#include <format>
#include <functional>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <cstdio>
#include <string_view>
#include <array>
#include <stdexcept>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <tuple>

namespace detail {
using json = nlohmann::ordered_json;
} // namespace detail

enum class Align
{
	Left,
	Right,
	Center
};
enum class OutputFormat
{
	Table,
	JSON
};
enum class BorderStyle
{
	Normal,
	Heavy,
	Double,
	None
};

// Transparent hash and comparator for heterogeneous string lookup
struct StringHash
{
	using is_transparent = void;
	size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
	size_t operator()(const std::string &s) const { return std::hash<std::string>{}(s); }
};

struct StringEqual
{
	using is_transparent = void;
	bool operator()(std::string_view a, std::string_view b) const { return a == b; }
};

/**
 * @brief Validation result for table structure
 */
struct TableValidationResult
{
	bool valid = true;
	std::vector<std::string> errors;
	std::vector<std::string> warnings;

	explicit operator bool() const { return valid; }
};

/**
 * @brief A production-ready table builder for creating formatted tables and JSON output
 *
 * This class provides a fluent API for building tables that can be output as either
 * formatted ASCII tables or JSON. It handles proper alignment, auto-sizing, and
 * validation of table structure.
 *
 * Example usage:
 * @code
 * TableBuilder()
 *     .addColumn("Name", 20, Align::Left)
 *     .addColumn("Value", 10, Align::Right)
 *     .addRow("Temperature", "75.5")
 *     .addRow("Power", "150")
 *     .enableAutoSizing()
 *     .print();
 * @endcode
 */
class TableBuilder
{
public:
	struct Row
	{
		std::vector<std::string> cells;
		std::unordered_map<std::string, std::string> jsonMetadata; // Additional fields for JSON output only

		// GPU monitoring style features
		std::vector<std::vector<std::string>> multiLineCells; // Multi-line cell content (overrides cells if non-empty)
		std::optional<std::string> spanText;				  // Text spanning all columns
		Align spanAlign = Align::Center;					  // Alignment for span text
		BorderStyle borderStyle = BorderStyle::Normal;		  // Custom border for this row
		bool isSeparator = false;							  // Row is just a separator line

		Row() = default;
		Row(const Row &other) = default;
		Row(Row &&other) noexcept = default;
		Row &operator=(const Row &other) = default;
		Row &operator=(Row &&other) noexcept = default;
		~Row() = default;

		template <typename... Args>
		explicit Row(Args &&...args)
			requires(sizeof...(args) > 0 && !(std::is_same_v<std::decay_t<Args>, Row> || ...))
		{
			cells.reserve(sizeof...(args));
			(cells.emplace_back(toString(std::forward<Args>(args))), ...);
		}

		template <typename T> static std::string toString(T &&arg)
		{
			using DecayT = std::decay_t<T>;

			if constexpr (std::is_same_v<DecayT, std::string>) {
				return std::forward<T>(arg);
			} else if constexpr (std::is_convertible_v<T, std::string_view>) {
				return std::string{std::forward<T>(arg)};
			} else if constexpr (std::is_same_v<DecayT, bool>) {
				return std::forward<T>(arg) ? "true" : "false";
			} else if constexpr (std::is_floating_point_v<DecayT>) {
				// Format floats with reasonable precision
				return std::format("{:.3f}", std::forward<T>(arg));
			} else {
				return std::format("{}", std::forward<T>(arg));
			}
		}

		/**
		 * @brief Add JSON-only metadata (won't appear in table)
		 */
		Row &addJsonField(std::string_view key, std::string_view value)
		{
			jsonMetadata[std::string{key}] = value;
			return *this;
		}

		/**
		 * @brief Add JSON-only metadata with any convertible type
		 */
		template <typename T> Row &addJsonField(std::string_view key, T &&value)
		{
			jsonMetadata[std::string{key}] = toString(std::forward<T>(value));
			return *this;
		}

		/**
		 * @brief Get the number of lines this row will occupy
		 */
		size_t lineCount() const
		{
			if (spanText || isSeparator) {
				return 1;
			}
			if (multiLineCells.empty()) {
				return 1;
			}

			size_t maxLines = 0;
			for (const auto &cellLines : multiLineCells) {
				maxLines = std::max(maxLines, cellLines.size());
			}
			return std::max(size_t{1}, maxLines);
		}
	};

	struct TableConfig
	{
		char borderChar = '-';
		char cornerChar = '+';
		char verticalChar = '|';
		std::string emptyCellText{}; // NOLINT (readability-redundant-member-init) // Redundant default to empty in
									 // order to use partial designated initializers
		std::string noDataText = "No data";
		bool showRowNumbers = false;
		int maxCellWidth = 50; // Prevent extremely wide columns
	};

	struct TableStats
	{
		size_t totalCells;
		size_t emptyCells;
		size_t totalCharacters;
		double fillRatio;
	};

private:
	struct Column
	{
		std::string header;
		std::vector<std::string> extraHeaders; // Additional header lines (second and further header rows)
		mutable int width;
		Align alignment;

		Column(std::string h, int w, Align a) : header(std::move(h)), width(w), alignment(a) {}
	};

	std::vector<Column> columns;
	std::vector<Row> rows;
	std::vector<Row> preHeaderRows; // Rendered inside the table border, before column headers
	bool autoSize = false;
	bool suppressHeaderSep = false;	   // Suppress the border line drawn after column headers
	bool suppressHeaderColSep = false; // Suppress inner | between column headers
	bool suppressDataColSep = false;   // Suppress inner | between data cells
	mutable bool widthsCalculated = false;
	OutputFormat outputFormat = OutputFormat::Table;
	TableConfig config;

	mutable std::unordered_map<std::string, int, StringHash, StringEqual> widthCache;

	// Streaming mode state
	bool streamingMode = false;
	int lastTableLines = 0;

	/**
	 * @brief Calculate display width considering basic UTF-8 characters
	 */
	int displayWidth(std::string_view text) const;

	void calculateWidths() const;

	/**
	 * @brief Get border characters based on style
	 */
	struct BorderChars
	{
		std::string horizontal;
		std::string vertical;
		std::string corner;
	};

	BorderChars getBorderChars(BorderStyle style) const;

	std::string getBorderLine(int rowNumWidth = 0, BorderStyle style = BorderStyle::Normal) const;

	void alignTextDirect(std::string &result, std::string_view text, int width, Align alignment) const;

	std::string toTableString() const;

	std::string toJsonString(bool compact = false) const;

	/**
	 * @brief Move cursor up by specified number of lines
	 */
	static void moveCursorUp(int lines);

	/**
	 * @brief Clear from cursor to end of screen
	 */
	static void clearToEnd();

	/**
	 * @brief Count lines in a string
	 */
	static int countLines(const std::string &str);

public:
	TableBuilder()
	{
		columns.reserve(8);
		rows.reserve(32);
	}

	/**
	 * @brief Add a column to the table
	 * @param header Column header text
	 * @param width Initial column width (may be adjusted with auto-sizing)
	 * @param alignment Text alignment within the column
	 * @return Reference to this TableBuilder for chaining
	 */
	TableBuilder &addColumn(std::string_view header, int width = 10, Align alignment = Align::Left);

	/**
	 * @brief Add a column with only header and alignment (uses minimal width)
	 * Convenient when auto-sizing is enabled - width will be calculated automatically
	 * @param header Column header text
	 * @param alignment Text alignment within the column
	 * @return Reference to this TableBuilder for chaining
	 */
	TableBuilder &addColumn(std::string_view header, Align alignment);

	/**
	 * @brief Add a row of data to the table
	 * @param args Cell values (must match number of columns)
	 * @return Reference to this TableBuilder for chaining
	 * @throws std::invalid_argument if cell count doesn't match column count
	 */
	template <typename... Args> TableBuilder &addRow(Args &&...args)
	{
		static_assert(sizeof...(args) > 0, "Row must have at least one cell");

		if (!columns.empty() && sizeof...(args) != columns.size()) {
			throw std::invalid_argument(
				std::format("Row has {} cells but table has {} columns", sizeof...(args), columns.size()));
		}

		rows.emplace_back(std::forward<Args>(args)...);
		widthsCalculated = false;
		return *this;
	}

	/**
	 * @brief Add a row with multi-line cells (GPU monitoring style)
	 * @param cellLines Vector of vectors, where each inner vector contains lines for that cell
	 * @return Reference to this TableBuilder for chaining
	 */
	TableBuilder &addMultiLineRow(const std::vector<std::vector<std::string>> &cellLines);

	/**
	 * @brief Add a spanning row (text spans all columns)
	 * @param text Text to display across all columns
	 * @param style Border style for this row
	 * @return Reference to this TableBuilder for chaining
	 */
	TableBuilder &addSpanRow(std::string_view text, BorderStyle style = BorderStyle::Normal);

	/**
	 * @brief Add a span row that renders BEFORE the column header row (but inside the table border).
	 *
	 * Use this for section banners (e.g. tool version / driver info) that should appear
	 * at the very top of the table, above the column names.
	 * A separator line (using the given style) is automatically inserted between the span
	 * text and the column header row.
	 *
	 * @param text    Text to center across the full table width
	 * @param style   Border style for the separator that follows the span row
	 * @return Reference to this TableBuilder for chaining
	 */
	TableBuilder &addPreHeaderSpanRow(std::string_view text, BorderStyle style = BorderStyle::Normal,
									  Align align = Align::Center);
	TableBuilder &suppressHeaderSeparator() noexcept;
	TableBuilder &suppressHeaderColumnSeparators() noexcept;
	TableBuilder &suppressDataColumnSeparators() noexcept;

	/**
	 * @brief Add a separator line with custom border style
	 * @param style Border style (Heavy for '=', Double for '═', etc.)
	 * @return Reference to this TableBuilder for chaining
	 */
	TableBuilder &addSeparator(BorderStyle style = BorderStyle::Heavy);

	/**
	 * @brief Get the last added row (for adding JSON metadata or modifying)
	 * @return Reference to the last row
	 * @throws std::out_of_range if no rows exist
	 */
	[[nodiscard]] Row &lastRow();

	/**
	 * @brief Add JSON-only field to the last row (convenient for chaining after addRow)
	 */
	TableBuilder &addJsonToLastRow(std::string_view key, std::string_view value);

	/**
	 * @brief Add JSON-only field to the last row with type conversion
	 */
	template <typename T> TableBuilder &addJsonToLastRow(std::string_view key, T &&value)
	{
		lastRow().addJsonField(key, std::forward<T>(value));
		return *this;
	}

	/**
	 * @brief Add multiple rows from a container
	 */
	template <typename Container> TableBuilder &addRows(const Container &rowData)
	{
		for (const auto &row : rowData) {
			if constexpr (requires { std::apply([](auto &&...args) { (void)sizeof...(args); }, row); }) {
				// Handle tuple-like objects
				std::apply([this](auto &&...args) { this->addRow(args...); }, row);
			} else {
				// Handle containers of values
				addRowFromContainer(row);
			}
		}
		return *this;
	}

	/**
	 * @brief Add row from any container (vector, array, etc.)
	 */
	template <typename Container> TableBuilder &addRowFromContainer(const Container &container)
	{
		Row row;
		row.cells.reserve(std::size(container));
		for (const auto &item : container) {
			row.cells.emplace_back(Row::toString(item));
		}

		if (!columns.empty() && row.cells.size() != columns.size()) {
			throw std::invalid_argument(
				std::format("Row has {} cells but table has {} columns", row.cells.size(), columns.size()));
		}

		rows.push_back(std::move(row));
		widthsCalculated = false;
		return *this;
	}

	/**
	 * @brief Enable automatic column width sizing based on content
	 */
	TableBuilder &enableAutoSizing() noexcept;

	/**
	 * @brief Disable automatic column width sizing
	 */
	TableBuilder &disableAutoSizing() noexcept;

	/**
	 * @brief Set output format (Table or JSON)
	 */
	TableBuilder &setOutputFormat(OutputFormat format) noexcept;

	/**
	 * @brief Enable JSON output format
	 */
	TableBuilder &enableJsonOutput() noexcept;

	/**
	 * @brief Enable table output format
	 */
	TableBuilder &enableTableOutput() noexcept;

	/**
	 * @brief Get current output format
	 */
	[[nodiscard]] OutputFormat getOutputFormat() const noexcept;

	/**
	 * @brief Configure table appearance
	 */
	TableBuilder &configure(const TableConfig &newConfig);

	/**
	 * @brief Enable row numbers
	 */
	TableBuilder &showRowNumbers(bool show = true) noexcept;

	/**
	 * @brief Set maximum cell width (for auto-sizing)
	 */
	TableBuilder &setMaxCellWidth(int maxWidth);

	/**
	 * @brief Set column alignment after creation
	 */
	TableBuilder &setColumnAlignment(size_t colIndex, Align alignment);

	/**
	 * @brief Set column width after creation
	 */
	TableBuilder &setColumnWidth(size_t colIndex, int width);
	TableBuilder &setColumnExtraHeaders(size_t colIndex, std::vector<std::string> headers);

	/**
	 * @brief Get the total rendered width of the table in characters (border-to-border).
	 * Triggers auto-size calculation if enabled. Useful for equalising two tables.
	 */
	[[nodiscard]] int getTotalWidth() const;

	/**
	 * @brief Stretch the last column so the table reaches @p targetWidth characters wide.
	 * Has no effect when the table is already at least that wide.
	 * Must be called after all rows have been added (uses current auto-sized widths).
	 */
	TableBuilder &padToWidth(int targetWidth);

	/**
	 * @brief Remove a column by index
	 */
	TableBuilder &removeColumn(size_t colIndex);

	/**
	 * @brief Remove a column by header name
	 */
	TableBuilder &removeColumn(std::string_view header);

	/**
	 * @brief Get column header by index
	 */
	[[nodiscard]] const std::string &getColumnHeader(size_t colIndex) const;

	/**
	 * @brief Find column index by header name (case-sensitive)
	 */
	[[nodiscard]] std::optional<size_t> findColumn(std::string_view header) const;

	/**
	 * @brief Find column index by header name (case-insensitive)
	 */
	[[nodiscard]] std::optional<size_t> findColumnIcase(std::string_view header) const;

	/**
	 * @brief Get cell value by row and column index
	 * @throws std::out_of_range if indices are invalid
	 */
	[[nodiscard]] const std::string &getCell(size_t rowIndex, size_t colIndex) const;

	/**
	 * @brief Try to get cell value, returns nullopt if not available
	 */
	[[nodiscard]] std::optional<std::string> tryGetCell(size_t rowIndex, size_t colIndex) const noexcept;

	/**
	 * @brief Get cell value by row index and column header
	 */
	[[nodiscard]] const std::string &getCell(size_t rowIndex, std::string_view columnHeader) const;

	/**
	 * @brief Update cell value
	 */
	TableBuilder &setCell(size_t rowIndex, size_t colIndex, std::string_view value);

	/**
	 * @brief Sort table by column
	 */
	TableBuilder &sortByColumn(size_t colIndex, bool ascending = true);

	/**
	 * @brief Sort by column header name
	 */
	TableBuilder &sortByColumn(std::string_view columnHeader, bool ascending = true);

	/**
	 * @brief Filter rows based on predicate
	 * Predicate receives const Row& and should return true to keep the row
	 */
	template <typename Predicate> TableBuilder &filterRows(Predicate &&pred)
	{
		auto it = std::remove_if(rows.begin(), rows.end(), [&pred](const Row &row) { return !pred(row); });
		rows.erase(it, rows.end());
		widthsCalculated = false;
		widthCache.clear();
		return *this;
	}

	/**
	 * @brief Validate table structure
	 */
	[[nodiscard]] TableValidationResult validate() const;

	/**
	 * @brief Get table statistics
	 */
	[[nodiscard]] TableStats getStats() const;

	/**
	 * @brief Print the table to stdout
	 */
	void print() const noexcept;

	/**
	 * @brief Print the table to a specific output stream
	 */
	void printTo(std::ostream &os) const noexcept;

	/**
	 * @brief Convert to string using current output format
	 */
	[[nodiscard]] std::string toString() const;

	/**
	 * @brief Explicitly convert to table format
	 */
	[[nodiscard]] std::string asTable() const;

	/**
	 * @brief Explicitly convert to JSON format
	 */
	[[nodiscard]] std::string asJson(bool compact = false) const;

	/**
	 * @brief Freeze column widths at their current values.
	 *
	 * Triggers auto-sizing if enabled. After this call, adding more rows will not
	 * recalculate column widths, making the table safe for streaming append use.
	 */
	TableBuilder &lockWidths();

	/**
	 * @brief Controls whether @c headerLine() / @c rowLine() produce aligned or CSV output.
	 */
	enum class LineStyle
	{
		Aligned, ///< Pad each cell to its column width, separate with two spaces.
		Csv,	 ///< Emit raw text with @c ", " separator (no padding).
	};

	/**
	 * @brief Render only the header row as a plain string, without borders.
	 *
	 * @param style  @c LineStyle::Aligned (default) — padded/space-separated.
	 *               @c LineStyle::Csv — raw text with @c ", " separator.
	 * @return Formatted header string (no trailing newline).
	 */
	[[nodiscard]] std::string headerLine(LineStyle style = LineStyle::Aligned) const;

	/**
	 * @brief Render a single row of values as a plain string, without borders.
	 *
	 * @param values  Cell values in column order. Extra values are ignored;
	 *                missing values use the configured @c emptyCellText.
	 * @param style   @c LineStyle::Aligned (default) — padded/space-separated.
	 *                @c LineStyle::Csv — raw text with @c ", " separator.
	 * @return Formatted data row string (no trailing newline).
	 */
	[[nodiscard]] std::string rowLine(const std::vector<std::string> &values,
									  LineStyle style = LineStyle::Aligned) const;

	/**
	 * @brief Get number of columns
	 */
	[[nodiscard]] size_t columnCount() const noexcept;

	/**
	 * @brief Get number of rows
	 */
	[[nodiscard]] size_t rowCount() const noexcept;

	/**
	 * @brief Check if table is empty (no columns or no rows)
	 */
	[[nodiscard]] bool empty() const noexcept;

	/**
	 * @brief Clear all rows but keep column definitions
	 */
	TableBuilder &clearRows() noexcept;

	/**
	 * @brief Clear everything (columns and rows)
	 */
	TableBuilder &clear() noexcept;

	/**
	 * @brief Start streaming mode for live table updates
	 * Enables ANSI escape sequences on Windows and prepares for incremental updates.
	 * Call this once before the update loop.
	 * @return Reference to this TableBuilder for chaining
	 */
	TableBuilder &startStreaming();

	/**
	 * @brief End streaming mode
	 * @return Reference to this TableBuilder for chaining
	 */
	TableBuilder &endStreaming();

	/**
	 * @brief Print table in streaming mode, updating in place
	 * After the first call, subsequent calls will overwrite the previous output.
	 * Must be called between startStreaming() and endStreaming().
	 */
	void printStreaming() const;

	/**
	 * @brief Check if currently in streaming mode
	 * @return true if streaming mode is active
	 */
	[[nodiscard]] bool isStreaming() const noexcept;

	/**
	 * @brief Clear the screen (cross-platform)
	 */
	static void clearScreen();
};

#endif // TABLE_BUILDER_H