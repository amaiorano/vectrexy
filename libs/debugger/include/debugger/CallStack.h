#pragma once

#include "core/Base.h"
#include <optional>
#include <vector>

class StackFrame {
public:
    StackFrame(uint16_t calleeAddress, uint16_t frameAddress, uint16_t returnAddress,
               uint16_t stackPointer)
        : calleeAddress(calleeAddress)
        , frameAddress(frameAddress)
        , returnAddress(returnAddress)
        , stackPointer(stackPointer) {}

    std::string ToString() const {
        return FormattedString("Callee=$%04x Frame=$%04x Return=$%04x Stack=$%04x", calleeAddress,
                               frameAddress, returnAddress, stackPointer)
            .Value();
    }

    // Address of instruction that called this frame's function.
    // TODO: rename to callerAddress
    uint16_t calleeAddress{};
    // Address of the first instruction of this frame's function.
    // TODO: rename to calleeAddress?
    uint16_t frameAddress{};
    // Address to return to once this function ends.
    uint16_t returnAddress{};
    // Value that was stored in the CPU stack pointer (S) just before the function was called.
    // Normally, the CPU stack should only contain this value again when this frame's function
    // returns.
    uint16_t stackPointer{};
};

class CallStack {
public:
    const std::vector<StackFrame>& Frames() const { return m_frames; }

    void Clear() { m_frames.clear(); }

    [[nodiscard]] bool Empty() const { return m_frames.empty(); }

    void Push(StackFrame frame) { m_frames.push_back(std::move(frame)); }

    void Pop() { m_frames.pop_back(); }

    std::optional<StackFrame> Top() const {
        if (!m_frames.empty()) {
            return m_frames.back();
        }
        return {};
    }

    bool IsLastReturnAddress(uint16_t address) const {
        return !m_frames.empty() && address == m_frames.back().returnAddress;
    };

    std::optional<uint16_t> LastStackPointer() const {
        if (!m_frames.empty())
            return m_frames.back().stackPointer;
        return {};
    }

    std::optional<uint16_t> GetLastCalleeAddress() const {
        if (!m_frames.empty())
            return m_frames.back().calleeAddress;
        return {};
    }

private:
    std::vector<StackFrame> m_frames;
};
