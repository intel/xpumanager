/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file cli_table.cpp
 */

#include "cli_table.h"

#include <thread>

namespace xpum::cli {

#define TABLE_DEFAULT_WIDTH 100
#define TABLE_DEFAULT_IDENTATION 2
#define TABLE_DEFAULT_IDENTATION_LEVEL 0
#define TABLE_DEFAULT_SHOW_TITLE true
#define TABLE_DEFAULT_SUBITEM_ROW false
#define TABLE_DEFAULT_ARRAY_ITEM_SEP true
#define TABLE_DEFAULT_COLUMN_TITLE "TITLE"

#define KEY_TABLE_WIDTH "width"
#define KEY_TABLE_INDENTATION "indentation"
#define KEY_TABLE_SHOW_TITLE_ROW "showTitleRow"
#define KEY_TABLE_COLUMNS "columns"
#define KEY_TABLE_COLUMN_SIZE "size"
#define KEY_TABLE_COLUMN_TITLE "title"
#define KEY_TABLE_ROW_INSTANCE "instance"
#define KEY_TABLE_ARRAY_ITEM_SEP "in_array_sep"
#define KEY_TABLE_ROWS "rows"
#define KEY_TABLE_CELLS "cells"
#define KEY_TABLE_CELL_ROW_TITLE "rowTitle"
#define KEY_TABLE_CELL_LABEL "label"
#define KEY_TABLE_CELL_LABEL_TAG "label_tag"
#define KEY_TABLE_CELL_VALUE "value"
#define KEY_TABLE_CELL_SUB_ITEMS "subs"
#define KEY_TABLE_CELL_SUBITEM_ROW "subrow"
#define KEY_TABLE_CELL_SUFFIX "suffix"
#define KEY_TABLE_CELL_FIXER "fixer"
#define KEY_TABLE_CELL_SCALE "scale"

#define PATH_DELIMITER '.'

unsigned int count_elements(const std::string& str, const char deli) {
    unsigned int res = 0;
    for (unsigned long i = 0; i < str.length(); i++) {
        if (str[i] == deli) {
            res++;
        }
    }
    return res + 1;
}

CharTableConfigColumn::CharTableConfigColumn(const nlohmann::json& conf) : size(conf.value(KEY_TABLE_COLUMN_SIZE, TABLE_COLUMN_AUTO)),
                                                                           title(conf.value(KEY_TABLE_COLUMN_TITLE, TABLE_DEFAULT_COLUMN_TITLE)) {
}

CharTableConfigPath::CharTableConfigPath(const std::string& confValue) : elements(confValue.empty() ? 0 : count_elements(confValue, PATH_DELIMITER)) {
    if (elements.size() > 0) {
        unsigned long i = 0;
        unsigned int c = 0;
        while (i < confValue.size()) {
            unsigned long j = 0;
            j = confValue.find(PATH_DELIMITER, i);
            if (j == std::string::npos) {
                j = confValue.length();
            }
            std::string eleStr = confValue.substr(i, j - i);
            elements[c] = new CharTableConfigPathElement(eleStr);
            c++;
            i = j + 1;
        }
    }
}

CharTableConfigCellSingleSubItems::CharTableConfigCellSingleSubItems(const nlohmann::json::array_t* conf) : _enabled(conf != NULL),
                                                                                                            items((_enabled) ? conf->size() : 0) {
    if (_enabled) {
        for (unsigned int i = 0; i < conf->size(); i++) {
            items[i] = new CharTableConfigCellSingle(conf->at(i));
        }
    }
}

CharTableConfigCellSingleSubItems::~CharTableConfigCellSingleSubItems() {
    for (auto it : items) {
        delete it;
    }
}

const bool CharTableConfigCellSingleSubItems::append_value(std::string& res, const nlohmann::json& obj, const bool notFirst) const {
    bool itNF = false;
    for (auto it : items) {
        if (itNF) {
            res += ", ";
        }
        res += it->apply(obj);
        itNF = true;
    }
    return true;
}

static const std::string removeSpaceAfterBk(const std::string& labelConf) {
    const long ls = labelConf.length();
    if (ls < 3) return labelConf;
    if (labelConf[ls - 1] == ' ' && labelConf[ls - 2] == ')') {
        return labelConf.substr(0, ls - 1);
    }
    return labelConf;
}

CharTableConfigCellSingle::CharTableConfigCellSingle(const nlohmann::json& conf) : label(removeSpaceAfterBk(conf.is_object() ? conf.value(KEY_TABLE_CELL_LABEL, "") : "")),
                                                                                   label_tag(conf.is_object() ? conf.value(KEY_TABLE_CELL_LABEL_TAG, "") : ""),
                                                                                   row_title(conf.is_object() ? conf.value(KEY_TABLE_CELL_ROW_TITLE, "") : ""),
                                                                                   value(conf.is_object() ? conf.value(KEY_TABLE_CELL_VALUE, "") : (std::string)conf),
                                                                                   subs((conf.is_object() && conf.contains(KEY_TABLE_CELL_SUB_ITEMS)) ? conf[KEY_TABLE_CELL_SUB_ITEMS].get_ptr<const nlohmann::json::array_t*>() : NULL),
                                                                                   subrow(conf.is_object() ? conf.value(KEY_TABLE_CELL_SUBITEM_ROW, TABLE_DEFAULT_SUBITEM_ROW) : TABLE_DEFAULT_SUBITEM_ROW),
                                                                                   suffix(conf.is_object() ? conf.value(KEY_TABLE_CELL_SUFFIX, "") : ""),
                                                                                   fixer(conf.is_object() ? conf.value(KEY_TABLE_CELL_FIXER, "") : ""),
                                                                                   scale(conf.is_object() ? conf.value(KEY_TABLE_CELL_SCALE, NAN) : NAN) {
}

ChatTableConfigCellMulti::ChatTableConfigCellMulti(const nlohmann::json& conf) : cells(conf.size()) {
    for (unsigned long i = 0; i < cells.size(); i++) {
        auto cellDef = conf.at(i);
        if (cellDef.is_object()) {
            cells[i] = new CharTableConfigCellSingle(cellDef);
        }
    }
}

CharTableConfigRowObject::CharTableConfigRowObject(const nlohmann::json& conf) : instance(conf.value(KEY_TABLE_ROW_INSTANCE, "")),
                                                                                 in_array_sep(conf.value(KEY_TABLE_ARRAY_ITEM_SEP, TABLE_DEFAULT_ARRAY_ITEM_SEP)),
                                                                                 cells(conf.find(KEY_TABLE_CELLS)->size()) {
    auto cellsDef = conf.find(KEY_TABLE_CELLS);
    for (unsigned long i = 0; i < cells.size(); i++) {
        auto cellDef = cellsDef->at(i);
        if (cellDef.is_string() || cellDef.is_object()) {
            // a path
            cells[i] = new CharTableConfigCellSingle(cellDef);
        } else if (cellDef.is_array()) {
            // rows of key value pairs
            cells[i] = new ChatTableConfigCellMulti(cellDef);
        }
    }
}

CharTableConfig::CharTableConfig(const nlohmann::json& conf) : width(conf.value(KEY_TABLE_WIDTH, TABLE_DEFAULT_WIDTH)),
                                                               indentation(conf.value(KEY_TABLE_INDENTATION, TABLE_DEFAULT_IDENTATION)),
                                                               show_title_row(conf.value(KEY_TABLE_SHOW_TITLE_ROW, TABLE_DEFAULT_SHOW_TITLE)),
                                                               colWidthMax(conf.find(KEY_TABLE_COLUMNS)->size()),
                                                               colWidthSetting(colWidthMax.size()),
                                                               columns(colWidthMax.size()),
                                                               objects(conf.find(KEY_TABLE_ROWS)->size()) {
    auto colDef = conf.find(KEY_TABLE_COLUMNS);
    for (unsigned long i = 0; i < columns.size(); i++) {
        columns[i] = new CharTableConfigColumn(colDef->at(i));
        colWidthMax[i] = 0;
    }
    auto objDef = conf.find(KEY_TABLE_ROWS);
    for (unsigned long i = 0; i < objects.size(); i++) {
        objects[i] = new CharTableConfigRowObject(objDef->at(i));
    }
}

void CharTableConfig::setCellValue(CharTableRow& row, const unsigned int col, const std::string& value) {
    unsigned int valLen = value.length();
    if (valLen > colWidthMax[col]) {
        colWidthMax[col] = valLen;
    }
    row.setCell(value, col);
}

void CharTableConfig::addTitleRow(CharTableRow& row) {
    for (unsigned int i = 0; i < columns.size(); i++) {
        setCellValue(row, i, columns[i]->getTitle());
    }
}

void CharTableConfig::calCellWidth(CharTableRow& row, const unsigned int col, const std::string& value) {
    unsigned int valLen = value.length();
    if (valLen > colWidthMax[col]) {
        colWidthMax[col] = valLen;
    }
}

void CharTableConfig::calTitleRow(CharTableRow& row) {
    for (unsigned int i = 0; i < columns.size(); i++) {
        calCellWidth(row, i, columns[i]->getTitle());
    }
}

static const unsigned int margin = 1;
static const unsigned int line = 1;

void CharTableConfig::calculateColumnWidth() {
    unsigned int leftWidth = width - line;
    const unsigned int lastColId = colWidthMax.size() - 1;
    for (unsigned int i = 0; i <= lastColId; i++) {
        CharTableConfigColumn& col = *(columns[i]);
        int confSize = col.getSize();
        unsigned int colW;
        if (confSize == TABLE_COLUMN_AUTO) {
            colW = colWidthMax[i];
        } else {
            colW = confSize;
        }
        colWidthSetting[i] = colW;
        leftWidth -= (colW + margin * 2 + line);
    }
    if (leftWidth != 0) {
        colWidthSetting[lastColId] += leftWidth;
    }
}

CharTableRow::CharTableRow(const unsigned int colCount) : cells(colCount) {
    for (unsigned int i = 0; i < cells.size(); i++) {
        cells[i] = new std::string("");
    }
}

void CharTableRow::show(std::ostream& out, const std::vector<unsigned int>& colSetting) {
    output_repeat_char(out, '|', line);
    for (unsigned int i = 0; i < colSetting.size(); i++) {
        const std::string& cStr = *(cells[i]);
        const unsigned int colWidth = colSetting[i];
        output_repeat_char(out, ' ', margin);
        out << cStr;
        int csLeft = columnSpaceLeft(colWidth, i);
        if (csLeft > 0) {
            output_repeat_char(out, ' ', csLeft);
        }
        output_repeat_char(out, ' ', margin);
        output_repeat_char(out, '|', line);
    }
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    out << std::endl;
}

void CharTableRowSeparator::show(std::ostream& out, const std::vector<unsigned int>& colSetting) {
    output_repeat_char(out, '+', line);
    for (unsigned int colWidth : colSetting) {
        output_repeat_char(out, '-', margin);
        output_repeat_char(out, '-', colWidth);
        output_repeat_char(out, '-', margin);
        output_repeat_char(out, '+', line);
    }
    out << std::endl;
}

void CharTable::calculateHangRows() {
    const unsigned int iw = config.hangIndentation();
    auto row = rows.begin();
    while (row != rows.end()) {
        const int cols = (*row)->numberOfCells();
        CharTableRow* nRow = (CharTableRow*)(*row);
        row++;
        int ciw = 0;
        bool hasNewRow;
        do {
            hasNewRow = false;
            CharTableRow* sRow = nRow;
            for (int i = 0; i < cols; i++) {
                int cw = config.getWidthSetting(i);
                int csl = sRow->columnSpaceLeft(cw, i);
                if (csl < 0) {
                    // need to add hang row
                    int cp = sRow->getCutPositionForHangRow(cw, ciw, i);
                    const bool newItem = sRow->isNewRow(cp, i);
                    std::string lStr(sRow->cutCellContentAt(cp, newItem, i));
                    if (!hasNewRow) {
                        nRow = new CharTableRow(config.numOfColumns());
                        rows.insert(row, nRow);
                        hasNewRow = true;
                    }
                    std::string iStr("");
                    if (!newItem) {
                        for (unsigned int i = 0; i < iw; i++) {
                            iStr.append(" ");
                        }
                    }
                    nRow->setCell(iStr + lStr, i);
                }
            }
            if (hasNewRow) {
                ciw = iw;
            }
        } while (hasNewRow);
    }
}

CharTable::CharTable(CharTableConfig& tableConf, const nlohmann::json& res, const bool cont) : config(tableConf),
                                                                                               result(res),
                                                                                               rows() {
    if (config.showTitleRow() && !cont) {
        addSeparator();
        CharTableRow& titleRow = addRow();
        config.addTitleRow(titleRow);
    } else {
        CharTableRow& titleRow = addRow();
        config.calTitleRow(titleRow);
        removeLatestRow();
    }
    if (!cont) addSeparator();

    for (auto objConf : config.getObjects()) {
        const unsigned int objRows = objConf->maxRowCount();
        const nlohmann::json allObjs = objConf->getAllInstances(res);

        for (auto objIns : allObjs) {
            for (unsigned int i = 0; i < objRows; i++) {
                CharTableRow& dataRow = addRow();
                const std::vector<CharTableConfigCellBase*> objCellsConf = objConf->getCells();
                bool rowHasNoData = true;
                for (unsigned int j = 0; j < objCellsConf.size(); j++) {
                    auto objCol = objCellsConf[j];
                    auto cellConf = objCol->getCellConfigAt(i);
                    if (cellConf != NULL) {
                        const std::string cellValue = cellConf->apply(objIns);
                        if (!(cellConf->isSubRow() && cellValue.empty())) {
                            rowHasNoData = false;
                            config.setCellValue(dataRow, j, cellValue);
                        }
                    }
                }
                if (rowHasNoData) {
                    rows.pop_back();
                }
            }

            if (objConf->inArraySeparator()) addSeparator();
        }
        if (!objConf->inArraySeparator()) addSeparator();
    }

    config.calculateColumnWidth();
    calculateHangRows();
}

void CharTable::show(std::ostream& out) {
    const std::vector<unsigned int>& widthSetting = config.getWidthSetting();
    for (auto row : rows) {
        row->show(out, widthSetting);
    }
}

} // namespace xpum::cli
