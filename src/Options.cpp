#include "Options.h"
#include "StringUtil.h"
#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>

namespace {
    template <typename... Types>
    std::string ToString(const std::variant<Types...>& option) {
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
        assert(false);
        return {};
    }

    template <typename... Types>
    void FromString(const std::string& s, std::variant<Types...>& option) {
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

    // Always write out g_options file with default/loaded values
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
