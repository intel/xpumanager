/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "table_builder.h"
#include <nlohmann/json.hpp>
#include <string_view>
#include <string>
#include <cstddef>
#include <algorithm>
#include <format>
#include <unordered_set>
#include <stdexcept>
#include <vector>
#include <utility>
#include <optional>
#include <cctype>
#include <iostream>
#include <ostream>

int TableBuilder::displayWidth(std::string_view text) const
{
	if (auto it = widthCache.find(text); it != widthCache.end()) {
		return it->second;
	}

	// Simple ASCII fast path
	bool hasMultibyte = false;
	for (unsigned char const c : text) {
		if (c > 127) {
			hasMultibyte = true;
			break;
		}
	}

	int width = 0;
	if (!hasMultibyte) {
		width = static_cast<int>(text.length());
	} else {
		// Count non-continuation bytes for basic UTF-8 width estimation
		width = 0;
		for (unsigned char const c : text) {
			// UTF-8 continuation bytes start with 10xxxxxx
			if ((c & 0xC0) != 0x80) {
				++width;
			}
		}
	}

	// Cache the result with proper size limit
	if (widthCache.size() >= 1000) {
		widthCache.clear(); // Simple eviction strategy
	}
	widthCache[std::string(text)] = width;

	return width;
}

void TableBuilder::calculateWidths() const
{
	if (!autoSize || columns.empty() || widthsCalculated) {
		return;
	}

	// Initialize with header widths (considering extra header lines)
	for (const auto &column : columns) {
		column.width = displayWidth(column.header);
		for (const auto &extraHdr : column.extraHeaders) {
			column.width = std::max(column.width, displayWidth(extraHdr));
		}
	}

	for (const auto &row : rows) {
		// Handle multi-line cells
		if (!row.multiLineCells.empty()) {
			for (size_t col = 0; col < std::min(columns.size(), row.multiLineCells.size()); ++col) {
				for (const auto &line : row.multiLineCells[col]) {
					int const cellWidth = displayWidth(line);
					columns[col].width = std::max(columns[col].width, cellWidth);
				}
			}
		} else {
			// Regular single-line cells
			for (size_t col = 0; col < std::min(columns.size(), row.cells.size()); ++col) {
				int const cellWidth = displayWidth(row.cells[col]);
				columns[col].width = std::max(columns[col].width, cellWidth);
			}
		}
	}

	// Apply limits and padding
	for (const auto &col : columns) {
		col.width = std::min(col.width, config.maxCellWidth - 2);
		col.width += 2; // Add padding
	}

	// Expand last column if any span text (pre-header or regular) is wider than the
	// total column content area — otherwise alignTextDirect will truncate the banner.
	auto computeContentWidth = [&]() {
		int w = 0;
		for (const auto &col : columns) {
			w += col.width + 3;
		}
		return w - 3;
	};
	auto ensureFits = [&](std::string_view text) {
		int textW = displayWidth(text);
		int deficit = textW - computeContentWidth();
		if (deficit > 0) {
			columns.back().width += deficit;
		}
	};
	for (const auto &row : preHeaderRows) {
		if (row.spanText) {
			ensureFits(*row.spanText);
		}
	}
	for (const auto &row : rows) {
		if (row.spanText) {
			ensureFits(*row.spanText);
		}
	}

	widthsCalculated = true;
}

TableBuilder::BorderChars TableBuilder::getBorderChars(BorderStyle style) const
{
	switch (style) {
	case BorderStyle::Heavy:
		return {"=", "|", "+"};
	case BorderStyle::Double:
		// Use regular characters for compatibility
		return {"=", "|", "+"}; // Could use "═", "║", "╬" if terminal supports UTF-8
	case BorderStyle::None:
		return {" ", " ", " "};
	case BorderStyle::Normal:
	default:
		return {std::string(1, config.borderChar), std::string(1, config.verticalChar),
				std::string(1, config.cornerChar)};
	}
}

std::string TableBuilder::getBorderLine(int rowNumWidth, BorderStyle style) const
{
	auto borderChars = getBorderChars(style);

	std::string border;
	size_t totalLen = 1; // corner_char

	if (config.showRowNumbers) {
		totalLen += static_cast<size_t>(rowNumWidth) + 3;
	}

	for (const auto &col : columns) {
		totalLen += static_cast<size_t>(col.width) + 3;
	}
	border.reserve(totalLen + borderChars.horizontal.size() * totalLen);

	border += borderChars.corner;

	if (config.showRowNumbers) {
		for (int i = 0; i < rowNumWidth + 2; ++i) {
			border += borderChars.horizontal;
		}
		border += borderChars.corner;
	}

	for (const auto &col : columns) {
		for (int i = 0; i < col.width + 2; ++i) {
			border += borderChars.horizontal;
		}
		border += borderChars.corner;
	}
	border += '\n';

	return border;
}

void TableBuilder::alignTextDirect(std::string &result, std::string_view text, int width, Align alignment) const
{
	int const textWidth = displayWidth(text);

	if (textWidth > width) {
		if (width >= 3) {
			// Truncate and add ellipsis
			size_t charCount = 0;
			int accumulatedWidth = 0;
			for (size_t i = 0; i < text.length(); ++i) {
				unsigned char const c = text[i];
				if ((c & 0xC0) != 0x80) { // Not a continuation byte
					if (accumulatedWidth >= width - 3) {
						break;
					}
					++accumulatedWidth;
				}
				++charCount;
			}
			result.append(text.substr(0, charCount));
			result += "...";
		} else {
			result.append(text.substr(0, std::min(static_cast<size_t>(width), text.length())));
		}
		return;
	}

	int const padding = width - textWidth;

	switch (alignment) {
	case Align::Left:
		result += text;
		result.append(padding, ' ');
		break;
	case Align::Right:
		result.append(padding, ' ');
		result += text;
		break;
	case Align::Center: {
		int const leftPad = padding / 2;
		int const rightPad = padding - leftPad;
		result.append(leftPad, ' ');
		result += text;
		result.append(rightPad, ' ');
		break;
	}
	}
}

std::string TableBuilder::toTableString() const
{
	if (columns.empty()) {
		return "[Empty Table - No Columns]\n";
	}

	calculateWidths();

	// Better size estimation
	size_t totalWidth = 1; // Initial corner_char

	// Row number column width
	int rowNumWidth = 0;
	if (config.showRowNumbers) {
		rowNumWidth = std::max(3, static_cast<int>(std::to_string(rows.size()).length()) + 2);
		totalWidth += rowNumWidth + 3;
	}

	for (const auto &col : columns) {
		totalWidth += col.width + 3; // ' ' + content + ' |'
	}

	size_t const borderLineLen = totalWidth + 1; // +1 for newline
	size_t const rowLineLen = totalWidth + 1;
	size_t const estimatedSize = borderLineLen * 3 + rowLineLen * (rows.size() + 1);

	std::string result;
	result.reserve(estimatedSize);

	const std::string borderLine = getBorderLine(rowNumWidth);

	// Top border
	result += borderLine;

	// Pre-header rows: inside the top border, before the column header line.
	// Each span row is followed by a separator using the row's border style.
	for (const auto &row : preHeaderRows) {
		if (row.spanText) {
			int totalContentWidth = 0;
			if (config.showRowNumbers) {
				totalContentWidth += rowNumWidth + 3;
			}
			for (const auto &col : columns) {
				totalContentWidth += col.width + 3;
			}
			totalContentWidth -= 3;

			result += config.verticalChar;
			result += ' ';
			alignTextDirect(result, *row.spanText, totalContentWidth, row.spanAlign);
			result += std::format(" {}", config.verticalChar);
			result += '\n';
		}
		if (row.borderStyle != BorderStyle::None) {
			result += getBorderLine(rowNumWidth, row.borderStyle);
		}
	}

	// Header row
	result += config.verticalChar;

	// Row number header
	if (config.showRowNumbers) {
		result += ' ';
		alignTextDirect(result, "#", rowNumWidth, Align::Center);
		result += std::format(" {}", config.verticalChar);
	}

	for (size_t colIdx = 0; colIdx < columns.size(); ++colIdx) {
		const auto &col = columns[colIdx];
		result += ' ';
		alignTextDirect(result, col.header, col.width, col.alignment);
		const bool isLast = (colIdx == columns.size() - 1);
		if (!suppressHeaderColSep || isLast) {
			result += std::format(" {}", config.verticalChar);
		} else {
			result += "  "; // two spaces instead of " |"
		}
	}
	result += '\n';

	// Extra header lines (multi-line column headers)
	size_t maxExtraHeaders = 0;
	for (const auto &col : columns) {
		maxExtraHeaders = std::max(maxExtraHeaders, col.extraHeaders.size());
	}
	for (size_t lineIdx = 0; lineIdx < maxExtraHeaders; ++lineIdx) {
		result += config.verticalChar;
		if (config.showRowNumbers) {
			result += ' ';
			alignTextDirect(result, "", rowNumWidth, Align::Left);
			result += std::format(" {}", config.verticalChar);
		}
		for (const auto &col : columns) {
			result += ' ';
			if (lineIdx < col.extraHeaders.size()) {
				alignTextDirect(result, col.extraHeaders[lineIdx], col.width, col.alignment);
			} else {
				alignTextDirect(result, "", col.width, col.alignment);
			}
			result += std::format(" {}", config.verticalChar);
		}
		result += '\n';
	}

	if (!suppressHeaderSep) {
		result += borderLine;
	}

	// Data rows
	if (rows.empty()) {
		// Show "No data" row for empty tables with headers
		result += config.verticalChar;

		int totalContentWidth = 0;
		if (config.showRowNumbers) {
			totalContentWidth += rowNumWidth + 3;
		}
		for (const auto &col : columns) {
			totalContentWidth += col.width + 3;
		}
		totalContentWidth -= 3; // Adjust for formatting

		int const padding = totalContentWidth - static_cast<int>(config.noDataText.length());
		result += ' ';
		result.append(std::max(0, padding / 2), ' ');
		result += config.noDataText;
		result.append(std::max(0, padding - padding / 2), ' ');
		result += std::format(" {}", config.verticalChar);
		result += '\n';
	} else {
		for (size_t rowIdx = 0; rowIdx < rows.size(); ++rowIdx) {
			const auto &row = rows[rowIdx];

			// Handle separator rows
			if (row.isSeparator) {
				result += getBorderLine(rowNumWidth, row.borderStyle);
				continue;
			}

			// Handle spanning text rows
			if (row.spanText) {
				int totalContentWidth = 0;
				if (config.showRowNumbers) {
					totalContentWidth += rowNumWidth + 3;
				}
				for (const auto &col : columns) {
					totalContentWidth += col.width + 3;
				}
				totalContentWidth -= 3; // Adjust for formatting

				result += config.verticalChar;
				result += ' ';
				alignTextDirect(result, *row.spanText, totalContentWidth, Align::Center);
				result += std::format(" {}", config.verticalChar);
				result += '\n';
				continue;
			}

			// Handle multi-line rows
			if (!row.multiLineCells.empty()) {
				size_t const numLines = row.lineCount();
				for (size_t line = 0; line < numLines; ++line) {
					result += config.verticalChar;

					// Row number (only on first line)
					if (config.showRowNumbers) {
						result += ' ';
						if (line == 0) {
							alignTextDirect(result, std::to_string(rowIdx + 1), rowNumWidth, Align::Right);
						} else {
							result.append(rowNumWidth, ' ');
						}
						result += std::format(" {}", config.verticalChar);
					}

					// Multi-line cells
					for (size_t i = 0; i < columns.size(); ++i) {
						result += ' ';
						std::string_view cellContent;
						if (i < row.multiLineCells.size() && line < row.multiLineCells[i].size()) {
							cellContent = row.multiLineCells[i][line];
						}
						alignTextDirect(result, cellContent, columns[i].width, columns[i].alignment);
						const bool isLast = (i == columns.size() - 1);
						if (!suppressDataColSep || isLast) {
							result += std::format(" {}", config.verticalChar);
						} else {
							result += "  ";
						}
					}
					result += '\n';
				}
			} else {
				// Regular single-line row
				result += config.verticalChar;

				// Row number
				if (config.showRowNumbers) {
					result += ' ';
					alignTextDirect(result, std::to_string(rowIdx + 1), rowNumWidth, Align::Right);
					result += std::format(" {}", config.verticalChar);
				}

				for (size_t i = 0; i < columns.size(); ++i) {
					result += ' ';
					std::string_view const cellContent = (i < row.cells.size() && !row.cells[i].empty())
															 ? std::string_view{row.cells[i]}
															 : std::string_view{config.emptyCellText};
					alignTextDirect(result, cellContent, columns[i].width, columns[i].alignment);
					const bool isLast = (i == columns.size() - 1);
					if (!suppressDataColSep || isLast) {
						result += std::format(" {}", config.verticalChar);
					} else {
						result += "  ";
					}
				}
				result += '\n';
			}
		}
	}

	// Bottom border
	result += borderLine;

	return result;
}

std::string TableBuilder::toJsonString(bool compact) const
{
	if (columns.empty()) {
		return compact ? "[]" : "[\n]\n";
	}

	detail::json j = detail::json::array();

	for (size_t rowIdx = 0; rowIdx < rows.size(); ++rowIdx) {
		const auto &row = rows[rowIdx];
		detail::json rowObj = detail::json::object();
		std::unordered_set<std::string> seenKeys;

		// Add row number if enabled
		if (config.showRowNumbers) {
			rowObj["row_number"] = rowIdx + 1;
			seenKeys.insert("row_number");
		}

		// Add regular columns
		for (size_t colIdx = 0; colIdx < columns.size(); ++colIdx) {
			std::string header = columns[colIdx].header;

			// Handle duplicate header names
			if (seenKeys.contains(header)) {
				int suffix = 1;
				std::string uniqueHeader;
				do {
					uniqueHeader = std::format("{}_{}", header, suffix++);
				} while (seenKeys.contains(uniqueHeader));
				header = uniqueHeader;
			}
			seenKeys.insert(header);

			std::string const cellValue = (colIdx < row.cells.size()) ? row.cells[colIdx] : config.emptyCellText;

			rowObj[header] = cellValue;
		}

		// Add JSON-only metadata fields
		for (const auto &[key, value] : row.jsonMetadata) {
			std::string safeKey = key;

			// Handle key conflicts with columns
			if (seenKeys.contains(safeKey)) {
				int suffix = 1;
				do {
					safeKey = std::format("{}_{}", key, suffix++);
				} while (seenKeys.contains(safeKey));
			}
			seenKeys.insert(safeKey);

			rowObj[safeKey] = value;
		}

		j.push_back(rowObj);
	}

	return compact ? j.dump() : j.dump(2);
}

TableBuilder &TableBuilder::addColumn(std::string_view header, int width, Align alignment)
{
	if (width < 1) {
		throw std::invalid_argument("Column width must be at least 1");
	}
	columns.emplace_back(std::string{header}, width, alignment);
	widthsCalculated = false;
	widthCache.clear();
	return *this;
}

TableBuilder &TableBuilder::addColumn(std::string_view header, Align alignment)
{
	this->enableAutoSizing();
	return addColumn(header, 1, alignment); // Minimal width, will be auto-sized
}

TableBuilder &TableBuilder::addMultiLineRow(const std::vector<std::vector<std::string>> &cellLines)
{
	Row row;
	row.multiLineCells = cellLines;
	rows.push_back(std::move(row));
	widthsCalculated = false;
	return *this;
}

TableBuilder &TableBuilder::addPreHeaderSpanRow(std::string_view text, BorderStyle style, Align align)
{
	Row row;
	row.spanText = std::string{text};
	row.spanAlign = align;
	row.borderStyle = style;
	preHeaderRows.push_back(std::move(row));
	widthsCalculated = false;
	return *this;
}

TableBuilder &TableBuilder::suppressHeaderSeparator() noexcept
{
	suppressHeaderSep = true;
	return *this;
}

TableBuilder &TableBuilder::suppressHeaderColumnSeparators() noexcept
{
	suppressHeaderColSep = true;
	return *this;
}

TableBuilder &TableBuilder::suppressDataColumnSeparators() noexcept
{
	suppressDataColSep = true;
	return *this;
}

TableBuilder &TableBuilder::addSpanRow(std::string_view text, BorderStyle style)
{
	Row row;
	row.spanText = std::string{text};
	row.borderStyle = style;
	rows.push_back(std::move(row));
	widthsCalculated = false;
	return *this;
}

TableBuilder &TableBuilder::addSeparator(BorderStyle style)
{
	Row row;
	row.isSeparator = true;
	row.borderStyle = style;
	rows.push_back(std::move(row));
	return *this;
}

TableBuilder::Row &TableBuilder::lastRow()
{
	if (rows.empty()) {
		throw std::out_of_range("No rows in table");
	}
	return rows.back();
}

TableBuilder &TableBuilder::addJsonToLastRow(std::string_view key, std::string_view value)
{
	lastRow().addJsonField(key, value);
	return *this;
}

TableBuilder &TableBuilder::enableAutoSizing() noexcept
{
	autoSize = true;
	widthsCalculated = false;
	return *this;
}

TableBuilder &TableBuilder::disableAutoSizing() noexcept
{
	autoSize = false;
	return *this;
}

TableBuilder &TableBuilder::setOutputFormat(OutputFormat format) noexcept
{
	outputFormat = format;
	return *this;
}

TableBuilder &TableBuilder::enableJsonOutput() noexcept
{
	outputFormat = OutputFormat::JSON;
	return *this;
}

TableBuilder &TableBuilder::enableTableOutput() noexcept
{
	outputFormat = OutputFormat::Table;
	return *this;
}

OutputFormat TableBuilder::getOutputFormat() const noexcept { return outputFormat; }

TableBuilder &TableBuilder::configure(const TableConfig &newConfig)
{
	this->config = newConfig;
	widthsCalculated = false;
	widthCache.clear();
	return *this;
}

TableBuilder &TableBuilder::showRowNumbers(bool show) noexcept
{
	config.showRowNumbers = show;
	return *this;
}

TableBuilder &TableBuilder::setMaxCellWidth(int maxWidth)
{
	if (maxWidth < 3) {
		throw std::invalid_argument("Max cell width must be at least 3");
	}
	config.maxCellWidth = maxWidth;
	widthsCalculated = false;
	return *this;
}

TableBuilder &TableBuilder::setColumnAlignment(size_t colIndex, Align alignment)
{
	if (colIndex >= columns.size()) {
		throw std::out_of_range("Column index out of range");
	}
	columns[colIndex].alignment = alignment;
	return *this;
}

TableBuilder &TableBuilder::setColumnWidth(size_t colIndex, int width)
{
	if (colIndex >= columns.size()) {
		throw std::out_of_range("Column index out of range");
	}
	if (width < 1) {
		throw std::invalid_argument("Column width must be at least 1");
	}
	columns[colIndex].width = width;
	widthsCalculated = false;
	widthCache.clear();
	return *this;
}

TableBuilder &TableBuilder::setColumnExtraHeaders(size_t colIndex, std::vector<std::string> headers)
{
	if (colIndex >= columns.size()) {
		throw std::out_of_range("Column index out of range");
	}
	columns[colIndex].extraHeaders = std::move(headers);
	widthsCalculated = false;
	widthCache.clear();
	return *this;
}

int TableBuilder::getTotalWidth() const
{
	calculateWidths();
	int total = 1; // left border char
	if (config.showRowNumbers) {
		int rw = std::max(3, static_cast<int>(std::to_string(rows.size()).length()) + 2);
		total += rw + 3;
	}
	for (const auto &col : columns) {
		total += col.width + 3; // ' ' content ' |'
	}
	return total;
}

TableBuilder &TableBuilder::padToWidth(int targetWidth)
{
	if (columns.empty()) {
		return *this;
	}
	int deficit = targetWidth - getTotalWidth();
	if (deficit > 0) {
		columns.back().width += deficit;
		widthCache.clear();
	}
	return *this;
}

TableBuilder &TableBuilder::removeColumn(size_t colIndex)
{
	if (colIndex >= columns.size()) {
		throw std::out_of_range("Column index out of range");
	}

	columns.erase(columns.begin() + static_cast<std::vector<Column>::difference_type>(colIndex));

	// Remove corresponding cells from all rows
	for (auto &row : rows) {
		if (colIndex < row.cells.size()) {
			row.cells.erase(row.cells.begin() + static_cast<std::vector<std::string>::difference_type>(colIndex));
		}
	}

	widthsCalculated = false;
	widthCache.clear();
	return *this;
}

TableBuilder &TableBuilder::removeColumn(std::string_view header)
{
	auto colIdx = findColumn(header);
	if (!colIdx) {
		throw std::invalid_argument(std::format("Column '{}' not found", header));
	}
	return removeColumn(*colIdx);
}

const std::string &TableBuilder::getColumnHeader(size_t colIndex) const
{
	if (colIndex >= columns.size()) {
		throw std::out_of_range("Column index out of range");
	}
	return columns[colIndex].header;
}

size_t TableBuilder::columnCount() const noexcept { return columns.size(); }

size_t TableBuilder::rowCount() const noexcept { return rows.size(); }

bool TableBuilder::empty() const noexcept { return columns.empty() || rows.empty(); }

std::optional<size_t> TableBuilder::findColumn(std::string_view header) const
{
	for (size_t i = 0; i < columns.size(); ++i) {
		if (columns[i].header == header) {
			return i;
		}
	}
	return std::nullopt;
}

std::optional<size_t> TableBuilder::findColumnIcase(std::string_view header) const
{
	auto toLower = [](std::string_view s) {
		std::string lower;
		lower.reserve(s.length());
		for (char const c : s) {
			lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		return lower;
	};

	std::string const headerLower = toLower(header);
	for (size_t i = 0; i < columns.size(); ++i) {
		if (toLower(columns[i].header) == headerLower) {
			return i;
		}
	}
	return std::nullopt;
}

const std::string &TableBuilder::getCell(size_t rowIndex, size_t colIndex) const
{
	if (rowIndex >= rows.size()) {
		throw std::out_of_range("Row index out of range");
	}
	if (colIndex >= columns.size()) {
		throw std::out_of_range("Column index out of range");
	}
	if (colIndex >= rows[rowIndex].cells.size()) {
		throw std::out_of_range("Cell does not exist in row");
	}
	return rows[rowIndex].cells[colIndex];
}

std::optional<std::string> TableBuilder::tryGetCell(size_t rowIndex, size_t colIndex) const noexcept
{
	if (rowIndex >= rows.size() || colIndex >= columns.size()) {
		return std::nullopt;
	}
	if (colIndex >= rows[rowIndex].cells.size() || rows[rowIndex].cells[colIndex].empty()) {
		return std::nullopt;
	}
	return rows[rowIndex].cells[colIndex];
}

const std::string &TableBuilder::getCell(size_t rowIndex, std::string_view columnHeader) const
{
	auto colIdx = findColumn(columnHeader);
	if (!colIdx) {
		throw std::invalid_argument(std::format("Column '{}' not found", columnHeader));
	}
	return getCell(rowIndex, *colIdx);
}

TableBuilder &TableBuilder::setCell(size_t rowIndex, size_t colIndex, std::string_view value)
{
	if (rowIndex >= rows.size()) {
		throw std::out_of_range("Row index out of range");
	}
	if (colIndex >= columns.size()) {
		throw std::out_of_range("Column index out of range");
	}

	// Extend row if necessary
	if (colIndex >= rows[rowIndex].cells.size()) {
		rows[rowIndex].cells.resize(columns.size());
	}

	rows[rowIndex].cells[colIndex] = value;
	widthsCalculated = false;
	widthCache.clear(); // Invalidate cache
	return *this;
}

TableBuilder &TableBuilder::sortByColumn(size_t colIndex, bool ascending)
{
	if (colIndex >= columns.size()) {
		throw std::out_of_range("Column index out of range");
	}

	std::sort(rows.begin(), rows.end(), [colIndex, ascending](const Row &a, const Row &b) {
		const std::string &valA = (colIndex < a.cells.size()) ? a.cells[colIndex] : "";
		const std::string &valB = (colIndex < b.cells.size()) ? b.cells[colIndex] : "";

		return ascending ? valA < valB : valA > valB;
	});

	return *this;
}

TableBuilder &TableBuilder::sortByColumn(std::string_view columnHeader, bool ascending)
{
	auto colIdx = findColumn(columnHeader);
	if (!colIdx) {
		throw std::invalid_argument(std::format("Column '{}' not found", columnHeader));
	}
	return sortByColumn(*colIdx, ascending);
}

TableValidationResult TableBuilder::validate() const
{
	TableValidationResult result;

	if (columns.empty()) {
		result.errors.emplace_back("No columns defined");
		result.valid = false;
	}

	// Check for duplicate headers
	std::unordered_set<std::string> seenHeaders;
	for (const auto &col : columns) {
		if (seenHeaders.contains(col.header)) {
			result.warnings.push_back(std::format("Duplicate column header: '{}'", col.header));
		}
		seenHeaders.insert(col.header);
	}

	// Check for empty headers
	for (size_t i = 0; i < columns.size(); ++i) {
		if (columns[i].header.empty()) {
			result.warnings.push_back(std::format("Column {} has empty header", i));
		}
	}

	// Check row consistency
	for (size_t i = 0; i < rows.size(); ++i) {
		if (rows[i].cells.size() != columns.size()) {
			result.warnings.push_back(
				std::format("Row {} has {} cells but table has {} columns", i, rows[i].cells.size(), columns.size()));
		}
	}

	if (rows.empty()) {
		result.warnings.emplace_back("Table has no data rows");
	}

	return result;
}

TableBuilder::TableStats TableBuilder::getStats() const
{
	TableStats stats{};
	stats.totalCells = columns.size() * rows.size();

	for (const auto &row : rows) {
		for (size_t i = 0; i < columns.size(); ++i) {
			if (i >= row.cells.size() || row.cells[i].empty()) {
				++stats.emptyCells;
			} else {
				stats.totalCharacters += row.cells[i].length();
			}
		}
	}

	stats.fillRatio = stats.totalCells > 0 ? static_cast<double>(stats.totalCells - stats.emptyCells) /
												 static_cast<double>(stats.totalCells)
										   : 0.0;
	return stats;
}
void TableBuilder::print() const noexcept
{
	if (columns.empty()) {
		std::cout << "[Empty Table - No Columns]\n";
		return;
	}
	std::cout << toString();
}

void TableBuilder::printTo(std::ostream &os) const noexcept
{
	if (columns.empty()) {
		os << "[Empty Table - No Columns]\n";
		return;
	}
	os << toString();
}

std::string TableBuilder::toString() const
{
	return (outputFormat == OutputFormat::JSON) ? toJsonString() : toTableString();
}

std::string TableBuilder::asTable() const { return toTableString(); }

std::string TableBuilder::asJson(bool compact) const { return toJsonString(compact); }

TableBuilder &TableBuilder::lockWidths()
{
	calculateWidths();
	widthsCalculated = true;
	return *this;
}

std::string TableBuilder::headerLine(LineStyle style) const
{
	const bool pad = (style == LineStyle::Aligned);
	const std::string_view sep = pad ? "  " : ", ";
	calculateWidths();
	if (columns.empty()) {
		return {};
	}
	std::string result;
	for (size_t i = 0; i < columns.size(); ++i) {
		if (i > 0) {
			result += sep;
		}
		if (pad) {
			alignTextDirect(result, columns[i].header, columns[i].width, columns[i].alignment);
		} else {
			result += columns[i].header;
		}
	}
	return result;
}

std::string TableBuilder::rowLine(const std::vector<std::string> &values, LineStyle style) const
{
	const bool pad = (style == LineStyle::Aligned);
	const std::string_view sep = pad ? "  " : ", ";
	calculateWidths();
	if (columns.empty()) {
		return {};
	}
	std::string result;
	for (size_t i = 0; i < columns.size(); ++i) {
		if (i > 0) {
			result += sep;
		}
		const std::string_view cellContent = (i < values.size() && !values[i].empty())
												 ? std::string_view{values[i]}
												 : std::string_view{config.emptyCellText};
		if (pad) {
			alignTextDirect(result, cellContent, columns[i].width, columns[i].alignment);
		} else {
			result += cellContent;
		}
	}
	return result;
}

TableBuilder &TableBuilder::clearRows() noexcept
{
	rows.clear();
	widthsCalculated = false;
	widthCache.clear();
	return *this;
}

TableBuilder &TableBuilder::clear() noexcept
{
	columns.clear();
	rows.clear();
	preHeaderRows.clear();
	widthsCalculated = false;
	widthCache.clear();
	return *this;
}

void TableBuilder::moveCursorUp(int lines)
{
	if (lines > 0) {
		std::cout << "\033[" << lines << "A";
		std::cout.flush();
	}
}

void TableBuilder::clearToEnd()
{
	std::cout << "\033[J"; // Clear from cursor to end of screen
	std::cout.flush();
}

int TableBuilder::countLines(const std::string &str)
{
	int count = 0;
	for (char const c : str) {
		if (c == '\n') {
			++count;
		}
	}
	return count;
}

void TableBuilder::clearScreen()
{
	std::cout << "\033[2J\033[H";
	std::cout.flush();
}

TableBuilder &TableBuilder::startStreaming()
{
	streamingMode = true;
	lastTableLines = 0;
	return *this;
}

TableBuilder &TableBuilder::endStreaming()
{
	streamingMode = false;
	lastTableLines = 0;
	return *this;
}

void TableBuilder::printStreaming() const
{
	if (!streamingMode) {
		throw std::runtime_error("printStreaming() called outside of streaming mode. Call startStreaming() first.");
	}

	// Move cursor up to overwrite previous table (except on first print)
	if (lastTableLines > 0) {
		moveCursorUp(lastTableLines);
		clearToEnd();
	}

	// Print the table
	std::string const output = toString();
	std::cout << output;
	std::cout.flush();

	// Update line count for next iteration
	const_cast<TableBuilder *>(this)->lastTableLines = countLines(output);
}

bool TableBuilder::isStreaming() const noexcept { return streamingMode; }
