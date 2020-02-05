#pragma once

#include "core/Base.h"
#include <vector>

class StackFrame {
public:
    StackFrame(uint16_t calleeAddress, uint16_t frameAddress, uint16_t returnAddress)
        : calleeAddress(calleeAddress)
        , frameAddress(frameAddress)
        , returnAddress(returnAddress) {}

    uint16_t calleeAddress{};
    uint16_t frameAddress{};
    uint16_t returnAddress{};
};

class CallStack {
public:
    const std::vector<StackFrame>& Frames() const { return m_frames; }

    void Clear() { m_frames.clear(); }

    [[nodiscard]] bool Empty() const { return m_frames.empty(); }

    void Push(StackFrame frame) { m_frames.push_back(std::move(frame)); }

    void Pop() { m_frames.pop_back(); }

    bool IsLastCalleeAddress(uint16_t address) const {
        return !m_frames.empty() && address == m_frames.back().calleeAddress;
    };

    bool IsLastReturnAddress(uint16_t address) const {
        return !m_frames.empty() && address == m_frames.back().returnAddress;
    };

    std::optional<uint16_t> GetLastCalleeAddress() const {
        if (!m_frames.empty())
            return m_frames.back().calleeAddress;
        return {};
    }

private:
    std::vector<StackFrame> m_frames;
};
