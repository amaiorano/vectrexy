#pragma once

#include "MemoryBus.h"
#include "MemoryMap.h"
#include <array>

struct Vector2 {
    float x = 0.f;
    float y = 0.f;
};

struct Line {
    Vector2 p0;
    Vector2 p1;
};

enum class TimerMode { FreeRunning, OneShot, PulseCounting };

// Timer 1 is used mainly for drawing.
// Supports timed interrupt each time t1 is loaded (one-shot), or continuous interrupts
// (free-running) in which it auto-reloads initial count when it reaches 0.
class Timer1 {
public:
    void SetMode(TimerMode mode) {
        ASSERT_MSG(mode == TimerMode::OneShot, "Only supports one-shot mode for now");
    }
    TimerMode TimerMode() const { return TimerMode::OneShot; }

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
        return static_cast<uint8_t>(m_counter & 0x0F);
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
            m_interruptSignalLow = true;
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
    bool m_interruptSignalLow = false;

    bool m_pb7Flag = false;
    bool m_pb7SignalLow = false;
};

// Timer 2 is used mainly as a 50Hz game frame timer.
class Timer2 {};

// Implementation of the 6522 Versatile Interface Adapter (VIA)
// Used to control all of the Vectrex peripherals, such as keypads, vector generator, DAC, sound
// chip, etc.

class Via : public IMemoryBusDevice {
public:
    void Init(MemoryBus& memoryBus);
    void Update(cycles_t cycles);

private:
    uint8_t Read(uint16_t address) const override;
    void Write(uint16_t address, uint8_t value) override;

    // Registers
    uint8_t m_portB;    // 0x0
    uint8_t m_portA;    // 0x1
    uint8_t m_dataDirB; // 0x2
    uint8_t m_dataDirA; // 0x3
    // uint8_t m_timer1Low;       // 0x4
    // uint8_t m_timer1High;      // 0x5
    // uint8_t m_timer1LatchLow;  // 0x6
    // uint8_t m_timer1LatchHigh; // 0x7
    uint8_t m_timer2Low;  // 0x8
    uint8_t m_timer2High; // 0x9
    uint8_t m_shift;      // 0xA
    // uint8_t m_auxCntl;         // 0xB
    uint8_t m_periphCntl; // 0xC
    // uint8_t m_interruptFlag;   // 0xD
    uint8_t m_interruptEnable; // 0xE
                               // NOTE: 0xF is port A without handshake

    // State
    Vector2 m_pos;
    Vector2 m_velocity;
    float m_xyOffset = 0.f;
    float m_brightness = 0.f;
    bool m_blank = false;

public:
    Timer1 m_timer1;
    std::vector<Line> m_lines;
};
