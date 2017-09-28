#pragma once

#include "MemoryBus.h"
#include "MemoryMap.h"
#include "Timers.h"
#include <array>

struct Vector2 {
    float x = 0.f;
    float y = 0.f;

    void operator+=(const Vector2& rhs) {
        x += rhs.x;
        y += rhs.y;
    }
};

inline Vector2 operator+(const Vector2& lhs, const Vector2& rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

struct Line {
    Vector2 p0;
    Vector2 p1;
};

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
    void UpdateShift(cycles_t cycles);

    // Registers
    uint8_t m_portB;    // 0x0
    uint8_t m_portA;    // 0x1
    uint8_t m_dataDirB; // 0x2
    uint8_t m_dataDirA; // 0x3
    // uint8_t m_timer1Low;       // 0x4
    // uint8_t m_timer1High;      // 0x5
    // uint8_t m_timer1LatchLow;  // 0x6
    // uint8_t m_timer1LatchHigh; // 0x7
    // uint8_t m_timer2Low;  // 0x8
    // uint8_t m_timer2High; // 0x9
    uint8_t m_shift; // 0xA
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
    int m_shiftCyclesLeft = 0;

public:
    Timer1 m_timer1;
    Timer2 m_timer2;
    std::vector<Line> m_lines;
};
