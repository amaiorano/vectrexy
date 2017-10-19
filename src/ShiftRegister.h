#pragma once

#include "Base.h"

// The VIA's shift register, mainly responsible for driving the drawing of line patterns. It can be
// loaded with an 8 bit mask that represents the pattern to be drawn, and although it's called a
// "shift" register, it actually rotates its values so the pattern will repeat.
class ShiftRegister {
public:
    void SetValue(uint8_t value);
    uint8_t Value() const { return m_value; }
    bool CB2Active() const { return m_cb2Active; }
    void Update(cycles_t cycles);

private:
    uint8_t m_value = 0;
    int m_shiftCyclesLeft = 0;
    bool m_cb2Active = false;
};
