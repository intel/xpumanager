/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file cli_table.h
 */

#include <math.h>

#include <iomanip>
#include <iostream>
#include <list>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#define TABLE_COLUMN_AUTO -1

namespace xpum::cli {

inline void output_repeat_char(std::ostream& out, const char ch, const unsigned int times) {
    for (unsigned int i = 0; i < times; i++) {
        out << ch;
    }
}

class CharTable;
class CharTableRow;
class CharTableConfigCellSingle;

class CharTableConfigColumn {
   private:
    const int size;
    const std::string title;

   public:
    CharTableConfigColumn(const nlohmann::json& conf);

    inline const std::string& getTitle() {
        return title;
    }

    inline const int getSize() {
        return size;
    }
};

class CharTableConfigObjectFilter {
   private:
    const bool _enabled;
    const std::string prop_name;
    const std::string value_regex_string;
    const std::regex value_regex;

   public:
    inline CharTableConfigObjectFilter(const std::string& confValue) : _enabled(!confValue.empty()),
                                                                       prop_name((_enabled) ? confValue.substr(0, confValue.find("==")) : ""),
                                                                       value_regex_string((_enabled) ? confValue.substr(confValue.find("==") + 2) : ""),
                                                                       value_regex(value_regex_string) {
    }

    inline const bool match(const nlohmann::json& obj) const {
        if (_enabled) {
            std::string prop_value(obj.value(prop_name, ""));
            const bool matched = std::regex_match(prop_value, value_regex);
            return matched;
        }
        return true;
    }
};

class CharTableConfigPathElement {
   private:
    const bool is_array;
    const CharTableConfigObjectFilter object_filter;
    const std::string prop_name;

   public:
    inline CharTableConfigPathElement(const std::string& confValue) : is_array(confValue.find("[") != std::string::npos),
                                                                      object_filter((is_array) ? confValue.substr(confValue.find("[") + 1, confValue.find("]") - confValue.find("[") - 1) : ""),
                                                                      prop_name((is_array) ? confValue.substr(0, confValue.find("[")) : confValue){};

    inline const nlohmann::json apply(const nlohmann::json& obj) const {
        nlohmann::json res = nlohmann::json::array();
        for (auto inm : obj) {
            auto sub = inm.find(prop_name);
            if (sub != inm.end()) {
                if (is_array) {
                    if (sub->is_array()) {
                        for (auto inn : *sub) {
                            if (object_filter.match(inn))
                                res.push_back(inn);
                        }
                    }
                } else {
                    res.push_back(*sub);
                }
            }
        }
        return res;
    }
};

class CharTableConfigPath {
   private:
    std::vector<CharTableConfigPathElement*> elements;

   public:
    CharTableConfigPath(const std::string& confValue);

    ~CharTableConfigPath() {
        for (auto ele : elements) {
            delete ele;
        }
    }

    inline const bool empty() const {
        return elements.empty();
    }

    inline const nlohmann::json apply(const nlohmann::json& obj) const {
        nlohmann::json ino = nlohmann::json::array();
        if (obj.is_array()) {
            for (auto m : obj) {
                ino.push_back(m);
            }
        } else if (obj.is_object()) {
            ino.push_back(obj);
        }
        if (elements.empty()) {
            return ino;
        }
        nlohmann::json res;
        for (auto ele : elements) {
            res = ele->apply(ino);
            ino = res;
        }
        return res;
    }
};

class CharTableConfigCellBase {
   public:
    inline virtual ~CharTableConfigCellBase() {
    }

    virtual const unsigned int rowCount() = 0;

    virtual CharTableConfigCellSingle* getCellConfigAt(const unsigned int row) = 0;
};

inline const std::string get_json_value_string(const nlohmann::json& value) {
    if (value.is_string()) {
        return value;
    } else if (value.is_number_unsigned()) {
        return std::to_string((unsigned long)value);
    } else if (value.is_number_integer()) {
        return std::to_string((long)value);
    } else if (value.is_number()) {
        return std::to_string((double)value);
    } else if (value.is_object() || value.is_array()) {
        return value.dump();
    }
    return "";
}

inline auto scale_double_value(const std::string& value, const double scaleValue) {
    std::string procValue = value;
    try {
        double dv = std::stod(value) / scaleValue;
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << dv;
        procValue = oss.str();
    } catch (...) {
        procValue = value;
    }
    return procValue;
}

template <typename T>
auto fix_value(const std::string& value, std::function<T(double)> conv) {
    std::string procValue = value;
    try {
        T iv = conv(std::stod(value));
        std::ostringstream oss;
        oss << iv;
        procValue = oss.str();
    } catch (...) {
        procValue = value;
    }
    return procValue;
}

class CharTableConfigCellSingleSubItems {
   private:
    const bool _enabled;
    std::vector<CharTableConfigCellSingle*> items;

   public:
    CharTableConfigCellSingleSubItems(const nlohmann::json::array_t* conf);

    ~CharTableConfigCellSingleSubItems();

    inline const bool isEnabled() const {
        return _enabled;
    }

    const bool append_value(std::string& res, const nlohmann::json& obj, const bool notFirst = false) const;

    inline const std::string apply(const nlohmann::json& obj, const std::string& label, const CharTableConfigPath& labelTag, const bool subrow) const {
        std::string res;
        if (obj.is_array()) {
            bool notFirst = false;
            for (auto it : obj) {
                if (notFirst) {
                    if (subrow) {
                        res += "\n";
                    } else {
                        res += "; ";
                    }
                }
                if (!label.empty()) {
                    res += label;
                    if (!labelTag.empty()) {
                        const nlohmann::json& ltag = labelTag.apply(it);
                        if (ltag.is_array()) {
                            bool ltNF = false;
                            for (auto lt : ltag) {
                                if (ltNF) {
                                    res += ",";
                                }
                                res += get_json_value_string(lt);
                                ltNF = true;
                            }
                        } else {
                            res += get_json_value_string(ltag);
                        }
                    }
                    res += ": ";
                }
                notFirst = append_value(res, it, notFirst);
            }
        } else {
            append_value(res, obj);
        }
        return res;
    }
};

class CharTableConfigCellSingle : public CharTableConfigCellBase {
   private:
    const std::string label;
    const CharTableConfigPath label_tag;
    const std::string row_title;
    const CharTableConfigPath value;
    const CharTableConfigCellSingleSubItems subs;
    const bool subrow;
    const std::string suffix;
    const std::string fixer;
    const double scale;

    inline bool append_value(std::string& res, const std::string& value, const bool notFirst = false) {
        bool ret = notFirst;
        if (value.length() > 0) {
            std::string procValue = value;
            if (!isnan(scale)) {
                procValue = scale_double_value(value, scale);
            }
            if (fixer == "round") {
                procValue = fix_value<long>(procValue,
                                            [](double x) -> long {
                                                return (long)round(x);
                                            });
            }
            if (notFirst) {
                res += ", ";
            }
            ret = true;
            bool normalOut = true;
            if (fixer == "negint_novalue") {
                procValue = fix_value<long>(procValue,
                                            [](double x) -> long {
                                                return (x < 0) ? -1 : (long)x;
                                            });
                if (procValue == "-1") {
                    res += "none";
                    normalOut = false;
                }
            }
            if (normalOut) {
                res += procValue;
                res += suffix;
            }
        }
        return ret;
    }

    inline const std::string applyObject(const nlohmann::json& obj) {
        std::string res("");
        if (!row_title.empty()) {
            res = row_title;
            return res;
        }
        if (!label.empty()) {
            res += label;
            res += ": ";
        }
        const nlohmann::json propValue = value.apply(obj);
        if (propValue.is_array()) {
            bool notFirst = false;
            for (auto sVal : propValue) {
                notFirst = append_value(res, get_json_value_string(sVal), notFirst);
            }
        } else {
            append_value(res, get_json_value_string(propValue));
        }
        return res;
    }

   public:
    CharTableConfigCellSingle(const nlohmann::json& conf);

    inline virtual ~CharTableConfigCellSingle() {
    }

    inline virtual const unsigned int rowCount() override {
        return 1;
    }

    inline virtual CharTableConfigCellSingle* getCellConfigAt(const unsigned int row) {
        if (row < 1) {
            return this;
        }
        return NULL;
    }

    inline const std::string apply(const nlohmann::json& obj) {
        if (subs.isEnabled()) {
            return subs.apply(value.apply(obj), label, label_tag, subrow);
        }
        return applyObject(obj);
    }

    inline const bool isSubRow() const {
        return subrow;
    }
};

class ChatTableConfigCellMulti : public CharTableConfigCellBase {
   private:
    std::vector<CharTableConfigCellSingle*> cells;

   public:
    ChatTableConfigCellMulti(const nlohmann::json& conf);

    inline virtual ~ChatTableConfigCellMulti() {
        for (auto cell : cells) {
            delete cell;
        }
    }

    inline virtual const unsigned int rowCount() override {
        return cells.size();
    }

    inline virtual CharTableConfigCellSingle* getCellConfigAt(const unsigned int row) {
        if (row < cells.size()) {
            return cells[row];
        }
        return NULL;
    }
};

class CharTableConfigRowObject {
   private:
    const CharTableConfigPath instance;
    const bool in_array_sep;
    std::vector<CharTableConfigCellBase*> cells;

   public:
    CharTableConfigRowObject(const nlohmann::json& conf);

    inline ~CharTableConfigRowObject() {
        for (auto cell : cells) {
            delete cell;
        }
    }

    inline const nlohmann::json getAllInstances(const nlohmann::json& res) {
        return instance.apply(res);
    }

    inline const std::vector<CharTableConfigCellBase*> getCells() {
        return cells;
    }

    inline const unsigned int maxRowCount() {
        unsigned int res = 0;
        for (auto cell : cells) {
            res = std::max(cell->rowCount(), res);
        }
        return res;
    }

    inline const bool inArraySeparator() const {
        return in_array_sep;
    }
};

class CharTableConfig {
   private:
    const unsigned int width;
    const unsigned int indentation;
    const bool show_title_row;
    std::vector<unsigned int> colWidthMax;
    std::vector<unsigned int> colWidthSetting;
    std::vector<CharTableConfigColumn*> columns;
    std::vector<CharTableConfigRowObject*> objects;

   public:
    CharTableConfig(const nlohmann::json& conf);

    inline ~CharTableConfig() {
        for (auto col : columns) {
            delete col;
        }
        for (auto rowObj : objects) {
            delete rowObj;
        }
    }

    inline const unsigned int numOfColumns() const {
        return columns.size();
    }

    inline const unsigned int hangIndentation() const {
        return indentation;
    }

    void setCellValue(CharTableRow& row, const unsigned int col, const std::string& value);

    void calCellWidth(CharTableRow& row, const unsigned int col, const std::string& value);

    void addTitleRow(CharTableRow& row);

    void calTitleRow(CharTableRow& row);

    void calculateColumnWidth();

    inline const std::vector<unsigned int>& getWidthSetting() {
        return colWidthSetting;
    }

    // colIndex == -1 means last column
    inline const int getWidthSetting(int colIndex) {
        if (colIndex < 0) return colWidthSetting[colWidthSetting.size() - 1];
        return colWidthSetting[colIndex];
    }

    inline const std::vector<CharTableConfigRowObject*> getObjects() {
        return objects;
    }

    inline const bool showTitleRow() const {
        return show_title_row;
    }
};

class CharTableRowBase {
   public:
    inline virtual ~CharTableRowBase() {
    }

    virtual inline const int numberOfCells() const = 0;

    virtual const int columnSpaceLeft(const int colWidth, const int colIndex = -1) const = 0;

    virtual void show(std::ostream& out, const std::vector<unsigned int>& colSetting) = 0;
};

class CharTableRow : public CharTableRowBase {
   private:
    std::vector<std::string*> cells;

   public:
    CharTableRow(const unsigned int colCount);

    inline ~CharTableRow() override {
        for (auto cell : cells) {
            delete cell;
        }
    }

    inline const int numberOfCells() const override {
        return cells.size();
    }

    inline void setCell(const std::string& cellValue, const int colIndex = -1) {
        int colId = (colIndex < 0) ? cells.size() - 1 : colIndex;
        delete cells[colId];
        cells[colId] = new std::string(cellValue);
    }

    // colIndex == -1 means last column
    inline const int columnSpaceLeft(const int colWidth, const int colIndex = -1) const override {
        int colId = (colIndex < 0) ? cells.size() - 1 : colIndex;
        const unsigned long rp = cells[colId]->find("\n");
        if (rp != std::string::npos) return -1;
        int rdiff = colWidth - cells[colId]->length();
        return rdiff;
    }

    inline const int getCutPositionForHangRow(const int colWidth, const int indentation, const int colIndex = -1) const {
        int cp = colWidth;
        const std::string& cStr = *(cells[(colIndex < 0) ? cells.size() - 1 : colIndex]);
        const unsigned long nrp = cStr.find('\n');
        if (nrp != std::string::npos) {
            if (nrp <= (unsigned long)cp) return nrp;
        }
        const std::string dels(", \t/");
        while (cp > 0) {
            const char cc = cStr[cp - 1];
            if (dels.find(cc) != std::string::npos) {
                break;
            }
            cp--;
        }
        if (cp > indentation) return cp;
        return colWidth;
    }

    inline const bool isNewRow(const unsigned int index, const int colIndex = -1) const {
        const int ci = (colIndex < 0) ? cells.size() - 1 : colIndex;
        return index >= 0 && index < cells[ci]->length() && cells[ci]->at(index) == '\n';
    }

    inline std::string cutCellContentAt(const int len, const bool newRow, int colIndex = -1) {
        int ci = (colIndex < 0) ? cells.size() - 1 : colIndex;
        std::string* oStr = cells[ci];
        cells[ci] = new std::string(oStr->substr(0, len));
        int lp = len;
        if (newRow) {
            lp++;
        }
        std::string rs(oStr->substr(lp));
        delete oStr;
        return rs;
    }

    void show(std::ostream& out, const std::vector<unsigned int>& colSetting) override;
};

class CharTableRowSeparator : public CharTableRowBase {
   public:
    inline CharTableRowSeparator() {
    }

    inline const int numberOfCells() const override {
        return 0;
    }

    inline ~CharTableRowSeparator() override {
    }

    // colIndex == -1 means last column
    inline const int columnSpaceLeft(const int colWidth, const int colIndex = -1) const override {
        return 0;
    }

    void show(std::ostream& out, const std::vector<unsigned int>& colSetting) override;
};

class CharTable {
   private:
    CharTableConfig& config;
    const nlohmann::json& result;
    std::list<CharTableRowBase*> rows;

    void calculateHangRows();

   public:
    CharTable(CharTableConfig& tableConf, const nlohmann::json& res, const bool cont = false);

    inline ~CharTable() {
        for (auto row : rows) {
            delete row;
        }
    }

    inline CharTableRow& addRow() {
        rows.push_back(new CharTableRow(config.numOfColumns()));
        return (CharTableRow&)*(rows.back());
    }

    inline void removeLatestRow() {
        rows.pop_back();
    }

    inline CharTableRowSeparator& addSeparator() {
        rows.push_back(new CharTableRowSeparator());
        return (CharTableRowSeparator&)*(rows.back());
    }

    void show(std::ostream& out);
};
} // namespace xpum::cli
