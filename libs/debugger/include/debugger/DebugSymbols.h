#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <string>

struct SourceLocation {
    bool operator==(SourceLocation& rhs) { return file == rhs.file && line == rhs.line; }
    bool operator!=(SourceLocation& rhs) { return !(*this == rhs); }

    // TODO: expensive to store the same file string for multiple entries. Make a table of file
    // names, and store a index or pointer to it.
    std::string file;
    uint32_t line;
};

class DebugSymbols {
public:
    void AddSourceLocation(uint16_t address, SourceLocation location) {
        // TODO: if there's already a location object at address, validate that it's the same as the
        // input one.
        assert(m_sourceLocations[address].file.empty() || m_sourceLocations[address] == location);

        m_sourceLocations[address] = std::move(location);
    }

private:
    // Store source location info for every possible address
    std::array<SourceLocation, 64 * 1024> m_sourceLocations;
};
