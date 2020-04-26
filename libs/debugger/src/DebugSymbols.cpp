#include "debugger/DebugSymbols.h"
#include <cassert>

void DebugSymbols::AddSourceLocation(uint16_t address, SourceLocation location) {
    // TODO: if there's already a location object at address, validate that it's the same as the
    // input one.
    assert(m_sourceLocations[address].file.empty() || m_sourceLocations[address] == location);

    // First time we add this source location, add its address.
    // This assumes we add in order from first address for a given source location
    auto iter = m_locationToAddress.find(location);
    if (iter == m_locationToAddress.end()) {
        m_locationToAddress[location] = address;
    } else {
        // Make sure the address we stored is the first
        assert(iter->second <= address);
    }

    // Store address to source location
    m_sourceLocations[address] = std::move(location);
}

SourceLocation* DebugSymbols::GetSourceLocation(uint16_t address) {
    auto& location = m_sourceLocations[address];
    if (!location.file.empty())
        return &location;
    return nullptr;
}

std::optional<uint16_t>
DebugSymbols::GetAddressBySourceLocation(const SourceLocation& location) const {
    auto iter = m_locationToAddress.find(location);
    if (iter != m_locationToAddress.end()) {
        return iter->second;
    }
    return {};
}

void DebugSymbols::AddSymbol(Symbol symbol) {
    m_symbolsByAddress.try_emplace(symbol.address, symbol);
}

const Symbol* DebugSymbols::GetSymbolByName(const std::string& name) const {
    // Find the first symbol with the input name. There may be more than one.
    for (auto& [address, symbol] : m_symbolsByAddress) {
        if (symbol.name == name)
            return &symbol;
    }
    return {};
}

const Symbol* DebugSymbols::GetSymbolByAddress(uint16_t address) const {
    auto iter = m_symbolsByAddress.find(address);
    if (iter != m_symbolsByAddress.end()) {
        return &iter->second;
    }
    return {};
}
