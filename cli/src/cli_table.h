#include <nlohmann/json.hpp>
#include <vector>
#include <list>
#include <string>
#include <iostream>

#define TABLE_COLUMN_AUTO           -1

namespace xpum::cli {

inline void output_repeat_char(std::ostream& out, const char ch, const unsigned int times) {
    for (unsigned int i=0; i<times; i++) {
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

class CharTableConfigPathElement {
    private:
    const bool is_array;
    const std::string prop_name;

    public:
    CharTableConfigPathElement(const std::string& confValue);

    inline const nlohmann::json apply(const nlohmann::json& obj) const {
        nlohmann::json res = nlohmann::json::array();
        for (auto inm: obj) {
            auto sub = inm.find(prop_name);
            if (sub != inm.end()) {
                if (is_array) {
                    if (sub->is_array()) {
                        for (auto inn: *sub) {
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
        for (auto ele: elements) {
            delete ele;
        }
    }

    inline const nlohmann::json apply(const nlohmann::json& obj) const {
        nlohmann::json ino = nlohmann::json::array();
        if (obj.is_array()) {
            for (auto m: obj) {
                ino.push_back(m);
            }
        } else if (obj.is_object()) {
            ino.push_back(obj);
        }
         nlohmann::json res;
        for (auto ele: elements) {
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

inline void append_json_value(std::string& res, const nlohmann::json& value) {
    if (value.is_string()) {
        res += value;
    } else if (value.is_number_unsigned()) {
        res += std::to_string((unsigned long) value);
    } else if (value.is_number_integer()) {
        res += std::to_string((long) value);
    } else if (value.is_number()) {
        res += std::to_string((double) value);
    } else if (value.is_object() || value.is_array()) {
        res += value.dump();
    }
}

class CharTableConfigCellSingle : public CharTableConfigCellBase {
    private:
    const std::string label;
    const CharTableConfigPath value;

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
        std::string res("");
        if (!label.empty()) {
            res += label;
            res += ": ";
        }
        const nlohmann::json propValue = value.apply(obj);
        if (propValue.is_array()) {
            bool notFirst = false;
            for (auto sVal: propValue) {
                if (notFirst) {
                    res += ",";
                } else {
                    notFirst = true;
                }
                append_json_value(res, sVal);
            }
        } else {
            append_json_value(res, propValue);
        }
        return res;
    }
};

class ChatTableConfigCellMulti : public CharTableConfigCellBase {
    private:
    std::vector<CharTableConfigCellSingle*> cells;

    public:
    ChatTableConfigCellMulti(const nlohmann::json& conf);

    inline virtual ~ChatTableConfigCellMulti() {
        for (auto cell: cells) {
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
    std::vector<CharTableConfigCellBase*> cells;

    public:
    CharTableConfigRowObject(const nlohmann::json& conf);

    inline ~CharTableConfigRowObject() {
        for (auto cell: cells) {
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
        for (auto cell: cells) {
            res = std::max(cell->rowCount(), res);
        }
        return res;
    }
};

class CharTableConfig {
    private:
    const unsigned int width;
    std::vector<unsigned int> colWidthMax;
    std::vector<unsigned int> colWidthSetting;
    std::vector<CharTableConfigColumn*> columns;
    std::vector<CharTableConfigRowObject*> objects;

    public:
    CharTableConfig(const nlohmann::json& conf);

    inline ~CharTableConfig() {
        for (auto col: columns) {
            delete col;
        }
        for (auto rowObj: objects) {
            delete rowObj;
        }
    }

    inline const unsigned int numOfColumns() const {
        return columns.size();
    }

    void setCellValue(CharTableRow& row, const unsigned int col, const std::string& value);

    void addTitleRow(CharTableRow& row);

    void calculateColumnWidth();

    inline const std::vector<unsigned int>& getWidthSetting() {
        return colWidthSetting;
    }

    inline const std::vector<CharTableConfigRowObject*> getObjects() {
        return objects;
    }
};

class CharTableRowBase {
    public:
    inline virtual ~CharTableRowBase() {
    }

    virtual void show(std::ostream& out, const std::vector<unsigned int>& colSetting) = 0;
};

class CharTableRow : public CharTableRowBase {
    private:
    std::vector<std::string*> cells;

    public:
    CharTableRow(const unsigned int colCount);

    inline ~CharTableRow() override {
        for (auto cell: cells) {
            delete cell;
        }
    }

    inline void setCell(unsigned int colId, const std::string& cellValue) {
        delete cells[colId];
        cells[colId] = new std::string(cellValue);
    }

    void show(std::ostream& out, const std::vector<unsigned int>& colSetting) override;
};

class CharTableRowSeparator : public CharTableRowBase {
    public:
    inline CharTableRowSeparator() {
    }

    inline ~CharTableRowSeparator() override {
    }

    void show(std::ostream& out, const std::vector<unsigned int>& colSetting) override;
};

class CharTable {
	private:
	CharTableConfig& config;
    const nlohmann::json& result;
    std::list<CharTableRowBase*> rows;

	public:
	CharTable(CharTableConfig& tableConf, const nlohmann::json& res);

    inline ~CharTable() {
        for (auto row: rows) {
            delete row;
        }
    }

    inline CharTableRow& addRow() {
        rows.push_back(new CharTableRow(config.numOfColumns()));
        return (CharTableRow&) *(rows.back());
    }

    inline CharTableRowSeparator& addSeparator() {
        rows.push_back(new CharTableRowSeparator());
        return (CharTableRowSeparator&) *(rows.back());
    }

    void show(std::ostream& out);
};
} // namespace xpumn::cli
