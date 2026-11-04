/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _PRINT_H
#define _PRINT_H

#include <nlohmann/json.hpp>
#include <string>

/**
 * @brief Serialises JSON for output, replacing any byte that is not valid UTF-8
 *
 * Strings filled from devices and firmware are not guaranteed to be UTF-8, and the default
 * serialiser throws on them, which would end the whole command.
 *
 * @param[in] json   The JSON value
 * @param[in] indent Spaces per level; -1 for a single line
 *
 * @retval The serialised JSON, with U+FFFD in place of each invalid byte sequence
 */
template <typename Json> [[nodiscard]] std::string jsonText(const Json &json, int indent = -1)
{
	return json.dump(indent, ' ', false, Json::error_handler_t::replace);
}

/**
 * @brief Base Print class that accumulates key-value pairs and delegates to child classes
 */
class Printer
{
public:
	Printer();
	virtual ~Printer() = default;
	virtual void print(nlohmann::ordered_json *jsonObj) = 0; // Pure virtual function for printing
};

class JsonPrinter : public Printer
{
public:
	JsonPrinter();
	void print(nlohmann::ordered_json *jsonObj) override;
};

class TextPrinter : public Printer
{
public:
	TextPrinter();
	void print(nlohmann::ordered_json *jsonObj) override;
};

#endif // _PRINT_H
