#pragma once

#include "core/Base.h"

// The VIA's shift register, mainly responsible for driving the drawing of line patterns. It can be
// loaded with an 8 bit mask that represents the pattern to be drawn, and although it's called a
// "shift" register, it actually rotates its values so the pattern will repeat.
class ShiftRegister {
public:
    void SetValue(uint8_t value);
    uint8_t ReadValue() const;
    bool CB2Active() const { return m_cb2Active; }
    bool Enabled() const {
        // For now, always return true as it seems the shift register isn't disabled by games on
        // Vectrex. Also, am not sure exactly how to control the enabled state of the shift
        // register; I believe it may be controlled by bit 4 of the VIA's auxiliary control register
        // ($B).
        return true;
    }
    void Update(cycles_t cycles);

    void SetInterruptFlag(bool enabled) { m_interruptFlag = enabled; }
    bool InterruptFlag() const { return m_interruptFlag; }

private:
    uint8_t m_value = 0;
    mutable int m_shiftCyclesLeft = 0;
    bool m_cb2Active = false;
    mutable bool m_interruptFlag = false;
};
