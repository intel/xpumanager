/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _PRINT_H
#define _PRINT_H

#include <nlohmann/json.hpp>

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
