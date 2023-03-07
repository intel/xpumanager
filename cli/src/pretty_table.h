/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file pretty_table.h
 */

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace xpum::cli {
class Table {
   private:
    int column_num;

    std::vector<int> width_list;

    std::ostream &out;

    std::vector<std::vector<std::vector<std::string>>> rows;

   public:
    Table(std::ostream &out, int column_num) : column_num(column_num), width_list(column_num), out(out){};
    Table(std::ostream &out) : column_num(2), width_list(2), out(out){};

    void add_row(std::vector<std::string> row);

    void add_augmented_row(std::vector<std::vector<std::string>> row);

    void show();

   private:
    void printHorizontalGrid();
};
} // namespace xpum::cli