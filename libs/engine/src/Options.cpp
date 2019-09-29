#include "engine/Options.h"
#include "core/StringUtil.h"
#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>

namespace {
    const char* LIST_SEPARATOR = ":";

    template <typename... Types>
    std::string ToString(const std::variant<Types...>& option) {
        auto fromVector = [](auto& values, auto convertFunc) {
            std::vector<std::string> strings;
            std::transform(values->begin(), values->end(), std::back_inserter(strings),
                           convertFunc);
            return StringUtil::Join(strings, LIST_SEPARATOR);
        };

        if (auto value = std::get_if<int>(&option)) {
            return std::to_string(*value);
        }
        if (auto value = std::get_if<float>(&option)) {
            return std::to_string(*value);
        }
        if (auto value = std::get_if<bool>(&option)) {
            return *value ? "true" : "false";
        }
        if (auto value = std::get_if<std::string>(&option)) {
            return *value;
        }
        if (auto value = std::get_if<std::vector<int>>(&option)) {
            return fromVector(value, [](auto v) { return std::to_string(v); });
        }
        if (auto value = std::get_if<std::vector<float>>(&option)) {
            return fromVector(value, [](auto v) { return std::to_string(v); });
        }
        if (auto value = std::get_if<std::vector<bool>>(&option)) {
            return fromVector(value, [](auto v) { return v ? "true" : "false"; });
        }
        if (auto value = std::get_if<std::vector<std::string>>(&option)) {
            return fromVector(value, [](auto v) { return v; });
        }
        assert(false);
        return {};
    }

    template <typename... Types>
    void FromString(const std::string& s, std::variant<Types...>& option) {
        auto toVector = [](const std::string& s, auto& value, auto convertFunc) {
            std::vector<std::string> strings =
                StringUtil::Split(s, LIST_SEPARATOR, StringUtil::KeepEmptyEntries::True);
            value->clear();
            std::transform(strings.begin(), strings.end(), std::back_inserter(*value), convertFunc);
        };

        if (auto value = std::get_if<int>(&option)) {
            *value = std::stoi(s);
            return;
        }
        if (auto value = std::get_if<float>(&option)) {
            *value = std::stof(s);
            return;
        }
        if (auto value = std::get_if<bool>(&option)) {
            *value = s == "true";
            return;
        }
        if (auto value = std::get_if<std::string>(&option)) {
            *value = s;
            return;
        }
        if (auto value = std::get_if<std::vector<int>>(&option)) {
            toVector(s, value, [](auto& v) { return std::stoi(v); });
            return;
        }
        if (auto value = std::get_if<std::vector<float>>(&option)) {
            toVector(s, value, [](auto& v) { return std::stof(v); });
            return;
        }
        if (auto value = std::get_if<std::vector<bool>>(&option)) {
            toVector(s, value, [](auto& v) { return v == "true"; });
            return;
        }
        if (auto value = std::get_if<std::vector<std::string>>(&option)) {
            toVector(s, value, [](auto& v) { return v; });
            return;
        }
        assert(false);
    }
} // namespace

void Options::Load() {
    assert(!m_filePath.empty());
    std::ifstream fin(m_filePath);
    if (!fin) {
        std::cerr << "No options file \"" << m_filePath << "\" found, using default values"
                  << std::endl;
    } else {
        std::string line;
        while (std::getline(fin, line)) {
            line = StringUtil::Trim(line);

            auto tokens = StringUtil::Trim(StringUtil::Split(line, "="));
            if (tokens.size() < 2)
                continue;

            if (auto iter = m_options.find(tokens[0]); iter != m_options.end()) {
                FromString(tokens[1], iter->second);
            } else {
                std::cerr << "Unknown option: " << tokens[0] << std::endl;
            }
        }
        fin.close();
    }

    // Always write out options file with default/loaded values
    Save();
}

void Options::Save() {
    assert(!m_filePath.empty());
    std::ofstream fout(m_filePath);
    for (auto& [name, option] : m_options) {
        auto s = ToString(option);
        fout << name << " = " << s << std::endl;
    }
}
