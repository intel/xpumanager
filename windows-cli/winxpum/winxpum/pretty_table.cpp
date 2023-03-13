/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file pretty_table.cpp
 */

#include "pretty_table.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

static std::string join(std::vector<std::string> strs) {
    if (strs.size() == 0) return "";
    std::string res = strs[0];
    for (std::size_t i = 1; i < strs.size(); i++) {
        res += "\n" + strs[i];
    }
    return res;
}


void Table::add_row(std::vector<std::string> row) {
    assert((int)row.size() == this->column_num);

    std::vector<std::vector<std::string>> columns(column_num);

    for (int i = 0; i < this->column_num; i++) {
        std::stringstream content(row[i]);

        std::string segment;

        std::vector<std::string> seglist;

        while (std::getline(content, segment)) {
            seglist.push_back(segment);
            width_list[i] = std::max(width_list[i], (int)segment.size());
        }
        columns[i] = seglist;
    }
    this->rows.push_back(columns);
}

void Table::add_augmented_row(std::vector<std::vector<std::string>> row) {
    std::vector<std::string> r;
    for (auto c : row) {
        r.push_back(join(c));
    }
    add_row(r);
}

void Table::printHorizontalGrid() {
    out << "+";
    for (std::size_t i = 0; i < width_list.size(); i++) {
        int width = width_list[i];
        out << std::string(width + 2, '-');
        out << "+";
    }
    out << std::endl;
}

void Table::show() {
    char separator = ' ';
    printHorizontalGrid();
    for (auto& row : rows) {
        int max_segs = 0;
        for (int i = 0; i < column_num; i++) {
            auto& column = row[i];
            max_segs = std::max((int)column.size(), max_segs);
        }
        for (int j = 0; j < max_segs; j++) {
            out << "|";
            for (int i = 0; i < column_num; i++) {
                int width = width_list[i];
                auto& column = row[i];
                if (j < (int)column.size()) {
                    out << " " << std::left << std::setw(width) << std::setfill(separator) << column[j] << " |";
                }
                else {
                    out << " " << std::left << std::setw(width) << std::setfill(separator) << "" << " |";
                }
            }
            out << std::endl;
        }
        printHorizontalGrid();
    }
}