#include "cli_table.h"


namespace xpum::cli {

#define TABLE_DEFAULT_WIDTH         100
#define TABLE_DEFAULT_COLUMN_TITLE  "TITLE"

#define KEY_TABLE_WIDTH             "width"
#define KEY_TABLE_COLUMNS           "columns"
#define KEY_TABLE_COLUMN_SIZE       "size"
#define KEY_TABLE_COLUMN_TITLE      "title"
#define KEY_TABLE_ROW_INSTANCE      "instance"
#define KEY_TABLE_ROWS              "rows"
#define KEY_TABLE_CELLS             "cells"
#define KEY_TABLE_CELL_LABEL        "label"
#define KEY_TABLE_CELL_VALUE        "value"

#define PATH_DELIMITER '.'

unsigned int count_elements(const std::string& str, const char deli) {
    unsigned int res = 0;
    for (unsigned long i=0; i< str.length(); i++) {
        if (str[i] == deli) {
            res ++;
        }
    }
    return res+1;
}

CharTableConfigColumn::CharTableConfigColumn(const nlohmann::json& conf):
size(conf.value(KEY_TABLE_COLUMN_SIZE, TABLE_COLUMN_AUTO)),
title(conf.value(KEY_TABLE_COLUMN_TITLE, TABLE_DEFAULT_COLUMN_TITLE)) {
}

CharTableConfigPathElement::CharTableConfigPathElement(const std::string& confValue):
is_array(confValue.rfind("[]")==(confValue.length()-2)),
prop_name((is_array)?confValue.substr(0,confValue.length()-2):confValue) {
}

CharTableConfigPath::CharTableConfigPath(const std::string& confValue):
elements(count_elements(confValue, PATH_DELIMITER)) {
    unsigned long i = 0;
    unsigned int c = 0;
    while (i < confValue.size()) {
        unsigned long j = 0;
        j = confValue.find(PATH_DELIMITER, i);
        if (j == std::string::npos) {
            j = confValue.length();
        }
        std::string eleStr = confValue.substr(i, j-i);
        elements[c] = new CharTableConfigPathElement(eleStr);
        c ++;
        i = j + 1;
    }
}

CharTableConfigCellSingle::CharTableConfigCellSingle(const nlohmann::json& conf):
label(conf.is_object() ? conf.value(KEY_TABLE_CELL_LABEL, "") : ""),
value(conf.is_object() ? conf.value(KEY_TABLE_CELL_VALUE, "") : (std::string) conf) {
}

ChatTableConfigCellMulti::ChatTableConfigCellMulti(const nlohmann::json& conf):
cells(conf.size()) {
    for (unsigned long i=0; i< cells.size(); i++) {
        auto cellDef = conf.at(i);
        if (cellDef.is_object()) {
            cells[i] = new CharTableConfigCellSingle(cellDef);
        }
    }
}

CharTableConfigRowObject::CharTableConfigRowObject(const nlohmann::json& conf):
instance(conf.value(KEY_TABLE_ROW_INSTANCE, "")),
cells(conf.find(KEY_TABLE_CELLS)->size()) {
    auto cellsDef = conf.find(KEY_TABLE_CELLS);
    for (unsigned long i=0; i< cells.size(); i++) {
        auto cellDef = cellsDef->at(i);
        if (cellDef.is_string()) {
            // a path
            cells[i] = new CharTableConfigCellSingle(cellDef);
        } else if (cellDef.is_array()) {
            // rows of key value pairs
            cells[i] = new ChatTableConfigCellMulti(cellDef);
        }
    }
}

CharTableConfig::CharTableConfig(const nlohmann::json& conf):
width(conf.value(KEY_TABLE_WIDTH, TABLE_DEFAULT_WIDTH)),
colWidthMax(conf.find(KEY_TABLE_COLUMNS)->size()),
colWidthSetting(colWidthMax.size()),
columns(colWidthMax.size()),
objects(conf.find(KEY_TABLE_ROWS)->size()) {
    auto colDef = conf.find(KEY_TABLE_COLUMNS);
    for (unsigned long i=0; i< columns.size(); i++) {
        columns[i] = new CharTableConfigColumn(colDef->at(i));
        colWidthMax[i] = 0;
    }
    auto objDef = conf.find(KEY_TABLE_ROWS);
    for (unsigned long i=0; i< objects.size(); i++) {
        objects[i] = new CharTableConfigRowObject(objDef->at(i));
    }
}

void CharTableConfig::setCellValue(CharTableRow& row, const unsigned int col, const std::string& value) {
    unsigned int valLen = value.length();
    if (valLen > colWidthMax[col]) {
        colWidthMax[col] = valLen;
    }
    row.setCell(col, value);
}

void CharTableConfig::addTitleRow(CharTableRow& row) {
    for (unsigned int i=0; i<columns.size(); i++) {
        setCellValue(row, i, columns[i]->getTitle());
    }
}

static const unsigned int margin = 1;
static const unsigned int line = 1;

void CharTableConfig::calculateColumnWidth() {
    unsigned int leftWidth = width - margin - line;
    const unsigned int lastColId = colWidthMax.size() - 1;
    for (unsigned int i=0; i<=lastColId; i++) {
        CharTableConfigColumn& col = *(columns[i]);
        int confSize = col.getSize();
        unsigned int colW;
        if (confSize == TABLE_COLUMN_AUTO) {
            colW = colWidthMax[i];
        } else {
            colW = confSize;
        }
        colWidthSetting[i] = colW;
        leftWidth -= (colW + margin + line);
    }
    if (leftWidth != 0) {
        colWidthSetting[lastColId] += leftWidth;
    }
}

CharTableRow::CharTableRow(const unsigned int colCount):
cells(colCount) {
    for (unsigned int i=0; i<cells.size(); i++) {
        cells[i] = new std::string("");
    }
}

void CharTableRow::show(std::ostream& out, const std::vector<unsigned int>& colSetting) {
    output_repeat_char(out, '|', line);
    for (unsigned int i=0; i<colSetting.size(); i++) {
        const std::string& cStr = *(cells[i]);
        const unsigned int cLen = cStr.length();
        const unsigned int colWidth = colSetting[i];
        output_repeat_char(out, ' ', margin);
        out << cStr;
        output_repeat_char(out, ' ', colWidth-cLen+margin);
        output_repeat_char(out, '|', line);
    }
    out << std::endl;
}

void CharTableRowSeparator::show(std::ostream& out, const std::vector<unsigned int>& colSetting) {
    output_repeat_char(out, '+', line);
    for (unsigned int colWidth: colSetting) {
        output_repeat_char(out, '-', margin);
        output_repeat_char(out, '-', colWidth);
        output_repeat_char(out, '-', margin);
        output_repeat_char(out, '+', line);
    }
    out << std::endl;
}

CharTable::CharTable(CharTableConfig& tableConf, const nlohmann::json& res):
config(tableConf),
result(res),
rows() {
    addSeparator();
    CharTableRow& titleRow = addRow();
    config.addTitleRow(titleRow);
    addSeparator();

    const unsigned int colCount = config.numOfColumns();

    for (auto objConf: config.getObjects()) {
        const nlohmann::json objIns = objConf->getAllInstances(res);
        const unsigned int objRows = objConf->maxRowCount();

        const nlohmann::json allObjs = objConf->getAllInstances(res);

        for (auto objIns: allObjs) {
            for (unsigned int i=0; i<objRows; i++) {
                CharTableRow& dataRow = addRow();
                const std::vector<CharTableConfigCellBase*> objCellsConf = objConf->getCells();
                for (unsigned int j=0; j<objCellsConf.size(); j++) {
                    auto objCol = objCellsConf[j];
                    auto cellConf = objCol->getCellConfigAt(i);
                    if (cellConf != NULL) {
                        const std::string cellValue = cellConf->apply(objIns);
                        config.setCellValue(dataRow, j, cellValue);
                    }
                }
            }

            addSeparator();
        }
    }

    config.calculateColumnWidth();
}

void CharTable::show(std::ostream& out) {
    const std::vector<unsigned int>& widthSetting = config.getWidthSetting();
    for (auto row: rows) {
        row->show(out, widthSetting);
    }
}

} // namespace xpum::cli

