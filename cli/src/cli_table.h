#include <nlohmann/json.hpp>
#include <string>

namespace xpum::cli {
class CharTableConfig {
    public:
    inline CharTableConfig() {
    }
};

class CharTable {
	private:
	CharTableConfig& config;

	public:
	inline CharTable(CharTableConfig& tableConfig):
        config(tableConfig) {
	}
};
} // namespace xpumn::cli
