#include <filesystem>
#include <functional>
#include <string_view>

class StabsParser {
public:
    bool Parse(const std::filesystem::path& file);
};
