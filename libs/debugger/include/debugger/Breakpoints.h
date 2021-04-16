#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>

// TODO: rename to LocationBreakpoint
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

    Breakpoint(Type type, uint16_t address)
        : type(type)
        , address(address) {}

    Breakpoint& Enabled(bool set = true) {
        enabled = set;
        return *this;
    }

    Breakpoint& Once(bool set = true) {
        once = set;
        return *this;
    }

    const Type type;
    const uint16_t address;
    bool enabled = true;
    bool once = false;
};

class Breakpoints {
public:
    void Reset() { m_breakpoints.clear(); }

    Breakpoint& Add(Breakpoint::Type type, uint16_t address) {
        Breakpoint& bp = m_breakpoints.try_emplace(address, type, address).first->second;
        return bp;
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

    // Removes all elements for which the predicate function returns true.
    // Predicate signature: bool p(Breakpoint&)
    template <typename P>
    void RemoveAllIf(P predicate) {
        for (size_t i = 0; i < Num(); ++i) {
            auto bp = *GetAtIndex(i);
            if (predicate(bp)) {
                RemoveAtIndex(i);
                --i;
            }
        }
    }

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

struct ConditionalBreakpoint {
    using ConditionFunc = std::function<bool()>;

    ConditionalBreakpoint(ConditionFunc conditionFunc)
        : conditionFunc(std::move(conditionFunc)) {}

    ConditionalBreakpoint& Once(bool set = true) {
        once = set;
        return *this;
    }

    ConditionFunc conditionFunc;
    bool once = false;
};

class ConditionalBreakpoints {
public:
    using ConditionFunc = ConditionalBreakpoint::ConditionFunc;

    template <typename Func>
    ConditionalBreakpoint& Add(Func&& conditionFunc) {
        return m_conditionalBreakpoints.emplace_back(std::forward<Func>(conditionFunc));
    }

    std::vector<ConditionalBreakpoint>& Breakpoints() { return m_conditionalBreakpoints; }

    void RemoveAll() { m_conditionalBreakpoints.clear(); }

private:
    std::vector<ConditionalBreakpoint> m_conditionalBreakpoints;
};
