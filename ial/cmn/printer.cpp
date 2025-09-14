/*
 * Copyright © 2025 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
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
