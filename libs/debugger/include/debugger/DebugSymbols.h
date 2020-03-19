#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <unordered_map>

struct SourceLocation {
    bool operator==(const SourceLocation& rhs) const {
        return file == rhs.file && line == rhs.line;
    }
    bool operator!=(const SourceLocation& rhs) const { return !(*this == rhs); }

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

    void AddSymbol(Symbol symbol) { m_symbolsByAddress.try_emplace(symbol.address, symbol); }

    const Symbol* GetSymbolByName(const std::string& name) const {
        // Find the first symbol with the input name. There may be more than one.
        for (auto& [address, symbol] : m_symbolsByAddress) {
            if (symbol.name == name)
                return &symbol;
        }
        return {};
    }

    const Symbol* GetSymbolByAddress(uint16_t address) const {
        auto iter = m_symbolsByAddress.find(address);
        if (iter != m_symbolsByAddress.end()) {
            return &iter->second;
        }
        return {};
    }

private:
    // Store source location info for every possible address
    // Address -> Source Location
    std::array<SourceLocation, 64 * 1024> m_sourceLocations;

    // Address -> Symbol
    // Note that multiple addresses may map to the same symbol name,
    // i.e. constants from headers included from multiple cpp files
    std::unordered_map<uint16_t, Symbol> m_symbolsByAddress;
};
