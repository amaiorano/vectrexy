#pragma once

#include "core/FileSystem.h"
#include <cassert>
#include <map>
#include <string>
#include <variant>
#include <vector>

class Options {
public:
    using OptionType =
        std::variant<int, float, bool, std::string, std::vector<int>, std::vector<float>,
                     std::vector<bool>, std::vector<std::string>>;

    // Make sure to add options before loading file
    template <typename T>
    void Add(const char* name, T defaultValue = {}) {
        m_options[name] = defaultValue;
    }

    void SetFilePath(fs::path path) { m_filePath = path; }
    void Load();
    void Save();

    template <typename T>
    T Get(const char* name) const {
        if (auto iter = m_options.find(name); iter != m_options.end()) {
            return std::get<T>(iter->second);
        }
        assert(false);
        return {};
    }

    template <typename T>
    void Set(const char* name, T value) {
        auto iter = m_options.find(name);
        assert(iter != m_options.end());
        iter->second = value;
    }

private:
    std::map<std::string, OptionType> m_options;
    fs::path m_filePath;
};
