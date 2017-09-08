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

// Timer 1 is used mainly for drawing
class Timer1 {
public:
    void SetCounterLow(uint8_t value) { m_latchLow = value; }

    void SetCounterHigh(uint8_t value) {
        m_latchHigh = value;
        // Transfer contents of both latches to counter
        m_counter = m_latchHigh << 8 | m_latchLow;
        // Reset interrupt
        m_interruptEnabled = false;
        m_pb7Enabled = true;
    }

    void Update(cycles_t cycles) {
        bool expired = cycles >= m_counter;
        m_counter -= checked_static_cast<uint16_t>(cycles);
        if (expired) {
            m_interruptEnabled = true;
            m_pb7Enabled = false;
        }
    }

    bool InterruptEnabled() const { return m_interruptEnabled; }
    bool PB7Enabled() const { return m_pb7Enabled; }

private:
    uint8_t m_latchLow = 0;
    uint8_t m_latchHigh = 0;
    uint16_t m_counter;
    bool m_interruptEnabled = false; // Enabled means signal low (enabled once expired)
    bool m_pb7Enabled = false;       // Enabled means signal low (enabled while counting down)
};

// Timer 2 is used mainly as a 50Hz game frame timer
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
    uint8_t B;
    uint8_t A;

    uint8_t DataDirB;
    uint8_t DataDirA;

    uint8_t Timer1Low;
    uint8_t Timer1High;

    uint8_t Timer1LatchLow;
    uint8_t Timer1LatchHigh;

    uint8_t Timer2Low;
    uint8_t Timer2High;

    uint8_t Shift;

    uint8_t AuxCntl;
    uint8_t PeriphCntl;

    uint8_t InterruptFlag;
    uint8_t InterruptEnable;

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
