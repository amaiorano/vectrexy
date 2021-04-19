#include "debugger/DebugSymbols.h"
#include <cassert>
#include <unordered_set>

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

void DebugSymbols::ResolveTypes(
    const std::function<std::shared_ptr<Type>(std::string id)>& resolver) {

    auto resolve = [&](std::string id) {
        auto t = resolver(id);
        ASSERT_MSG(std::dynamic_pointer_cast<UnresolvedType>(t) == nullptr,
                   "Type with id:%s was not resolved!", id.c_str());
        return t;
    };

    // Track types visited to avoid infinite recursion over the Type tree. E.g. if we resolve a
    // recursively defined type, like 'struct Node { Node* next; }', we must make sure not to
    // traverse 'Node' infinitely.
    std::unordered_set<const std::shared_ptr<Type>*> visitedTypes;

    std::function<std::shared_ptr<Type>(const std::shared_ptr<Type>& t)> tryResolve;

    tryResolve = [&](const std::shared_ptr<Type>& t) -> std::shared_ptr<Type> {
        if (visitedTypes.count(&t) != 0)
            return {};
        visitedTypes.insert(&t);

        if (auto ut = std::dynamic_pointer_cast<UnresolvedType>(t)) {
            return resolve(ut->id);

        } else if (auto it = std::dynamic_pointer_cast<IndirectType>(t)) {
            if (auto resolvedType = tryResolve(it->type)) {
                it->SetType(std::move(resolvedType));
            }

        } else if (auto at = std::dynamic_pointer_cast<ArrayType>(t)) {
            if (auto resolvedType = tryResolve(at->type)) {
                at->SetType(std::move(resolvedType));
            }

        } else if (auto st = std::dynamic_pointer_cast<StructType>(t)) {
            for (auto&& m : st->members) {
                if (auto resolvedType = tryResolve(m.type)) {
                    m.type = std::move(resolvedType);
                }
            }
        }

        return {};
    };

    for (auto&& t : m_types) {
        tryResolve(t);
    }

    for (auto&& addressFunctionPair : m_addressToFunction) {
        auto& rootScope = addressFunctionPair.second->scope;
        Traverse(rootScope, [&](std::shared_ptr<Scope> scope) {
            for (auto&& v : scope->variables) {
                tryResolve(v->type);
            }
        });
    }
}
