#pragma once

#include <cassert>
#include <map>
#include <variant>

class Options {
public:
    using OptionType = std::variant<int, float, bool>;

    template <typename T>
    void Add(const char* name, T defaultValue = {}) {
        m_options[name] = defaultValue;
    }

    template <typename T>
    T Get(const char* name) {
        if (auto iter = m_options.find(name); iter != m_options.end()) {
            return std::get<T>(iter->second);
        }
        assert(false);
        return {};
    }

    void LoadOptionsFile(const char* file);

private:
    std::map<std::string, OptionType> m_options;
};
