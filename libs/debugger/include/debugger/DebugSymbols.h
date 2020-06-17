#pragma once

#include "core/Base.h"
#include "core/StrongType.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>

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

namespace std {
    template <>
    struct hash<SourceLocation> {
        std::size_t operator()(const SourceLocation& location) const {
            std::string s = location.file + std::to_string(location.line);
            return std::hash<std::string>{}(s);
        }
    };
} // namespace std

struct Type {
    virtual ~Type() = default;
    virtual size_t Size() const = 0;

    std::string name;
};

struct UnresolvedType : Type {
    size_t Size() const override {
        FAIL();
        return 0;
    }

    std::string id{};
};

struct PrimitiveType : Type {
    size_t Size() const override { return byteSize; }

    enum class Format { Int, Char, Float };
    Format format{};
    size_t byteSize{};
    bool isSigned{};
};

struct EnumType : Type {
    size_t Size() const override { return byteSize; }

    std::unordered_map<ssize_t, std::string> valueToId;
    size_t byteSize{};
    bool isSigned{};
};

struct ArrayType : Type {
    size_t Size() const override { return type->Size() * numElems; }

    std::shared_ptr<Type> type; // Element type
    size_t numElems;
};

struct StructType : Type {
    size_t Size() const override { return byteSize; }

    struct Member {
        std::string name;
        size_t offsetBits;
        size_t sizeBits;
        std::shared_ptr<Type> type;
    };

    size_t byteSize{};
    std::vector<Member> members;
};

struct IndirectType : Type {
    size_t Size() const override {
        // Pointers/references are 2 bytes on the Vectrex (6809 CPU)
        return 2;
    }

    std::shared_ptr<Type> type;
};

struct Variable {
    std::string name;
    std::shared_ptr<Type> type;

    using NoLocation = StrongType<uint16_t, struct NoLocationType>;
    using StackOffset = StrongType<uint16_t, struct StackOffsetType>; // bytes
    std::variant<NoLocation, StackOffset> location;
};

struct Scope : std::enable_shared_from_this<Scope> {
    std::shared_ptr<Scope> Parent() {
        if (parent) {
            return parent->shared_from_this();
        }
        return {};
    }

    void AddChild(std::shared_ptr<Scope> child) {
        child->parent = this;
        children.push_back(std::move(child));
    }

    // Returns true if this scope contains/encompasses the input address
    bool Contains(uint16_t address) const {
        return address >= range.first && address < range.second;
    }

    Scope* parent{};
    std::vector<std::shared_ptr<Scope>> children;
    std::vector<std::shared_ptr<Variable>> variables;
    std::pair<uint16_t, uint16_t> range; // [first address, last address[
};

// TODO: Create a TreeNode base, and move this into it
template <typename Callback, typename NodePtr>
void Traverse(NodePtr node, Callback callback) {
    if (node) {
        callback(node);
        for (auto& c : node->children) {
            Traverse(c, callback);
        }
    }
}

template <typename Callback>
void Traverse(std::shared_ptr<const Scope> node, Callback callback) {
    return Traverse(node, callback);
}

struct Function {
    Function(std::string name, uint16_t address)
        : name(std::move(name))
        , address(address) {}

    std::string name;
    uint16_t address{};

    // May be nullptr if this function has no variables
    std::shared_ptr<Scope> scope;
};

struct Symbol {
    std::string name;
    uint16_t address{};
};

class DebugSymbols {
public:
    void AddSourceLocation(uint16_t address, SourceLocation location);
    SourceLocation* GetSourceLocation(uint16_t address);
    std::optional<uint16_t> GetAddressBySourceLocation(const SourceLocation& location) const;

    void AddSymbol(Symbol symbol);
    const Symbol* GetSymbolByName(const std::string& name) const;
    const Symbol* GetSymbolByAddress(uint16_t address) const;

    void AddFunction(std::shared_ptr<Function> function) {
        // TODO: Handle if function already exists at this address
        m_addressToFunction[function->address] = function;
    }
    std::shared_ptr<const Function> GetFunctionByAddress(uint16_t address) const {
        auto iter = m_addressToFunction.find(address);
        if (iter != m_addressToFunction.end())
            return iter->second;
        return {};
    }

    void AddType(std::shared_ptr<Type> type) { m_types.push_back(std::move(type)); }

    void ResolveTypes(const std::function<std::shared_ptr<Type>(std::string id)>& resolver);

private:
    // Store source location info for every possible address
    // Address -> Source Location
    std::array<SourceLocation, 64 * 1024> m_sourceLocations;

    // Address -> Symbol
    // Note that multiple addresses may map to the same symbol name,
    // i.e. constants from headers included from multiple cpp files
    std::unordered_map<uint16_t, Symbol> m_symbolsByAddress;

    // Source Location -> first address of instruction for this location
    std::unordered_map<SourceLocation, uint16_t> m_locationToAddress;

    std::unordered_map<uint16_t, std::shared_ptr<Function>> m_addressToFunction;
    std::vector<std::shared_ptr<Type>> m_types;
};
