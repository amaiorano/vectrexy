#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <unordered_map>

struct SourceLocation {
    bool operator==(SourceLocation& rhs) { return file == rhs.file && line == rhs.line; }
    bool operator!=(SourceLocation& rhs) { return !(*this == rhs); }

    // TODO: expensive to store the same file string for multiple entries. Make a table of file
    // names, and store a index or pointer to it.
    std::string file;
    uint32_t line;
};

struct Symbol {
    std::string name;
    uint16_t address{};
};

class DebugSymbols {
public:
    void AddSourceLocation(uint16_t address, SourceLocation location) {
        // TODO: if there's already a location object at address, validate that it's the same as the
        // input one.
        assert(m_sourceLocations[address].file.empty() || m_sourceLocations[address] == location);

        m_sourceLocations[address] = std::move(location);
    }

    SourceLocation* GetSourceLocation(uint16_t address) {
        auto& location = m_sourceLocations[address];
        if (!location.file.empty())
            return &location;
        return nullptr;
    }

    void AddSymbol(Symbol symbol) { m_symbols.try_emplace(symbol.name, symbol); }

    const Symbol* GetSymbol(const std::string& name) const {
        auto iter = m_symbols.find(name);
        if (iter != m_symbols.end()) {
            return &iter->second;
        }
        return {};
    }

private:
    // Store source location info for every possible address
    std::array<SourceLocation, 64 * 1024> m_sourceLocations;

    std::unordered_map<std::string, Symbol> m_symbols;
};
