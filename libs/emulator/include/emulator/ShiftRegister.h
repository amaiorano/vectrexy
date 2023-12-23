#pragma once

#include "core/Base.h"

enum class ShiftRegisterMode {
    // There are actually more modes, but I think Vectrex only uses these ones
    Disabled,
    ShiftOutUnder02,
};


// The VIA's shift register, mainly responsible for driving the drawing of line patterns. It can be
// loaded with an 8 bit mask that represents the pattern to be drawn, and although it's called a
// "shift" register, it actually rotates its values so the pattern will repeat.
class ShiftRegister {
public:
    void SetMode(ShiftRegisterMode mode) { m_mode = mode; }
    ShiftRegisterMode Mode() const { return m_mode; }

    void SetValue(uint8_t value);
    uint8_t ReadValue() const;
    bool CB2Active() const { return m_cb2Active; }
    void Update(cycles_t cycles);

    void SetInterruptFlag(bool enabled) { m_interruptFlag = enabled; }
    bool InterruptFlag() const { return m_interruptFlag; }

private:
    ShiftRegisterMode m_mode = ShiftRegisterMode::Disabled;
    uint8_t m_value = 0;
    mutable int m_shiftCyclesLeft = 0;
    bool m_cb2Active = false;
    mutable bool m_interruptFlag = false;
};
