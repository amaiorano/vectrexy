#pragma once

#include "core/Base.h"

enum class TimerMode { FreeRunning, OneShot, PulseCounting };
inline const char* TimerModeToString(TimerMode mode) {
    constexpr char* v[] = {"FreeRunning", "OneShot", "PulseCounting"};
    return v[static_cast<int>(mode)];
}

// Timer 1 is used mainly for drawing.
// Supports timed interrupt each time t1 is loaded (one-shot), or continuous interrupts
// (free-running) in which it auto-reloads initial count when it reaches 0.
class Timer1 {
public:
    void SetTimerMode(TimerMode mode) {
        ASSERT_MSG(mode == TimerMode::OneShot, "Only supports one-shot mode for now");
    }
    TimerMode Mode() const { return TimerMode::OneShot; }

    void WriteCounterLow(uint8_t value) { m_latchLow = value; }

    void WriteCounterHigh(uint8_t value) {
        m_latchHigh = value;
        // Transfer contents of both latches to counter and reset interrupt flag
        m_counter = m_latchHigh << 8 | m_latchLow;
        m_interruptFlag = false;

        //@TODO: This should happen 1 cycle later
        if (m_pb7Flag) {
            m_pb7SignalLow = true;
        }
    }

    uint8_t ReadCounterLow() const {
        m_interruptFlag = false;
        return static_cast<uint8_t>(m_counter & 0xFF);
    }

    uint8_t ReadCounterHigh() const { return static_cast<uint8_t>(m_counter >> 8); }

    void WriteLatchLow(uint8_t value) { WriteCounterLow(value); }
    void WriteLatchHigh(uint8_t value) { m_latchHigh = value; }
    uint8_t ReadLatchLow() const { return m_latchLow; }
    uint8_t ReadLatchHigh() const { return m_latchHigh; }

    void Update(cycles_t cycles) {
        bool expired = cycles >= m_counter;
        m_counter -= checked_static_cast<uint16_t>(cycles);
        if (expired) {
            m_interruptFlag = true;
            //@TODO: When do we set this back to false? What is it used for?
            // m_interruptSignalLow = true;
            m_pb7SignalLow = false;
        }
    }

    void SetInterruptFlag(bool enabled) { m_interruptFlag = enabled; }
    bool InterruptFlag() const { return m_interruptFlag; }

    void SetPB7Flag(bool enabled) { m_pb7Flag = enabled; }
    bool PB7Flag() const { return m_pb7Flag; }
    bool PB7SignalLow() const { return m_pb7SignalLow; }

private:
    uint8_t m_latchLow = 0;
    uint8_t m_latchHigh = 0;
    uint16_t m_counter = 0;

    mutable bool m_interruptFlag = false;
    // bool m_interruptSignalLow = false;

    bool m_pb7Flag = false;
    bool m_pb7SignalLow = false;
};

// Timer 2 is used mainly as a 50Hz game frame timer.
class Timer2 {
public:
    void SetTimerMode(TimerMode mode) {
        ASSERT_MSG(mode == TimerMode::OneShot, "Only supports one-shot mode for now");
    }
    TimerMode Mode() const { return TimerMode::OneShot; }

    void WriteCounterLow(uint8_t value) { m_latchLow = value; }

    void WriteCounterHigh(uint8_t value) {
        // Transfer contents of counter high and low-order latch to counter and reset interrupt flag
        m_counter = value << 8 | m_latchLow;
        m_interruptFlag = false;
    }

    uint8_t ReadCounterLow() const {
        m_interruptFlag = false;
        return static_cast<uint8_t>(m_counter & 0xFF);
    }

    uint8_t ReadCounterHigh() const { return static_cast<uint8_t>(m_counter >> 8); }

    void Update(cycles_t cycles) {
        bool expired = cycles >= m_counter;
        m_counter -= checked_static_cast<uint16_t>(cycles);
        if (expired) {
            m_interruptFlag = true;
        }
    }

    void SetInterruptFlag(bool enabled) { m_interruptFlag = enabled; }
    bool InterruptFlag() const { return m_interruptFlag; }

private:
    uint8_t m_latchLow = 0; // Note: Timer2 has no high-order latch
    uint16_t m_counter = 0;
    mutable bool m_interruptFlag = false;
};
