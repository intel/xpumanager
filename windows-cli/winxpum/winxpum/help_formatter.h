/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file help_formatter.h
 */

#pragma once

#include "CLI/App.hpp"
#include "CLI/Formatter.hpp"


class HelpFormatter : public CLI::Formatter {
public:
    std::string make_option_opts(const CLI::Option*) const override;

    std::string make_usage(const CLI::App* app, std::string name) const override;
};
