#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace xpum::cli {
class Table {
   private:
    std::vector<int> width_list;

    std::vector<std::vector<std::vector<std::string>>> rows;

    int column_num;

    std::ostream &out;

   public:
    Table(std::ostream &out, int column_num) : column_num(column_num), width_list(column_num), out(out){};
    Table(std::ostream &out) : column_num(2), width_list(2), out(out){};

    void add_row(std::vector<std::string> row);

    void show();

   private:
    void printHorizontalGrid();
};
} // namespace xpum::cli