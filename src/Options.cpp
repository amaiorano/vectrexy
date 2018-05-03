#include "Options.h"
#include "StringHelpers.h"
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
        assert(false);
    }
} // namespace

void Options::LoadOptionsFile(const char* file) {
    std::ifstream fin(file);
    if (!fin) {
        std::cerr << "No options file \"" << file << "\" found, using default values" << std::endl;
    } else {
        std::string line;
        while (std::getline(fin, line)) {
            line = Trim(line);

            auto tokens = Trim(Split(line, "="));
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
    std::ofstream fout(file);
    for (auto & [ name, option ] : m_options) {
        fout << name << " = " << ToString(option) << std::endl;
    }
}
