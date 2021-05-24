#include <filesystem>
#include <functional>
#include <string_view>

class StabsParser {
public:
    bool ParseFile(const std::filesystem::path& file);
    bool Parse(std::string_view source, std::string_view sourceFileName = "NoFile");

private:
    bool Parse(std::istream& in, std::string_view sourceFileName);
};
