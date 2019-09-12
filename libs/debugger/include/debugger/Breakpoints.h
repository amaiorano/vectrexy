#pragma once

#include <cstdint>
#include <map>
#include <optional>

struct Breakpoint {
    enum class Type {
        Instruction,
        Read,
        Write,
        ReadWrite,
    };
    static const char* TypeToString(Type type) {
        switch (type) {
        case Type::Instruction:
            return "Instruction";
        case Type::Read:
            return "Read";
        case Type::Write:
            return "Write";
        case Type::ReadWrite:
            return "ReadWrite";
        }
        return "INVALID";
    }

    Type type = Type::Instruction;
    uint16_t address = 0;
    bool enabled = true;
    bool autoDelete = false;
};

class Breakpoints {
public:
    void Reset() { m_breakpoints.clear(); }

    Breakpoint* Add(Breakpoint::Type type, uint16_t address) {
        auto& bp = m_breakpoints[address];
        bp.type = type;
        bp.address = address;
        return &bp;
    }

    std::optional<Breakpoint> Remove(uint16_t address) {
        auto iter = m_breakpoints.find(address);
        if (iter != m_breakpoints.end()) {
            auto bp = iter->second;
            m_breakpoints.erase(iter);
            return bp;
        }
        return {};
    }

    std::optional<Breakpoint> RemoveAtIndex(size_t index) {
        auto iter = GetBreakpointIterAtIndex(index);
        if (iter != m_breakpoints.end()) {
            auto bp = iter->second;
            m_breakpoints.erase(iter);
            return bp;
        }
        return {};
    }

    void RemoveAll() { m_breakpoints.clear(); }

    Breakpoint* Get(uint16_t address) {
        // @HACK: Temporary optimization to make debug builds faster when no breakpoints have been
        // added. Need to make breakpoints generally faster (with or without breakpoints set).
        if (Num() == 0)
            return nullptr;

        auto iter = m_breakpoints.find(address);
        if (iter != m_breakpoints.end()) {
            return &iter->second;
        }
        return nullptr;
    }

    Breakpoint* GetAtIndex(size_t index) {
        auto iter = GetBreakpointIterAtIndex(index);
        if (iter != m_breakpoints.end()) {
            return &iter->second;
        }
        return nullptr;
    }

    std::optional<size_t> GetIndex(uint16_t address) {
        auto iter = m_breakpoints.find(address);
        if (iter != m_breakpoints.end()) {
            return std::distance(m_breakpoints.begin(), iter);
        }
        return {};
    }

    size_t Num() const { return m_breakpoints.size(); }

private:
    std::map<uint16_t, Breakpoint> m_breakpoints;

    using IterType = decltype(m_breakpoints.begin());
    IterType GetBreakpointIterAtIndex(size_t index) {
        auto iter = m_breakpoints.begin();
        if (iter != m_breakpoints.end())
            std::advance(iter, index);
        return iter;
    }
};
