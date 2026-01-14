/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "printer.h"
#include "debug.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

using namespace std;

/**
 * @brief Constructor for the Printer base class.
 */
Printer::Printer() {}

/**
 * @brief Constructor for the JsonPrinter class.
 */
JsonPrinter::JsonPrinter() : Printer() {}

/**
 * @brief Print a JSON object with pretty formatting (4 spaces indentation).
 *
 * @param jsonObj Pointer to the JSON object to print.
 */
void JsonPrinter::print(nlohmann::ordered_json *jsonObj)
{
	PRINT("%s\n", jsonObj->dump(4).c_str()); // Pretty print JSON with 4 spaces
}

/**
 * @brief Constructor for the TextPrinter class.
 */
TextPrinter::TextPrinter() : Printer() {}

/**
 * @brief Print a JSON object as key-value pairs in text format.
 *
 * @param jsonObj Pointer to the JSON object to print.
 */
void TextPrinter::print(nlohmann::ordered_json *jsonObj)
{
	for (auto &item : jsonObj->items()) {
		PRINT("%s: %s\n", item.key().c_str(), item.value().dump().c_str()); // Print key-value pairs
	}
}
