#include "emulator/ShiftRegister.h"
#include "core/BitOps.h"

void ShiftRegister::SetValue(uint8_t value) {
    m_value = value;
    m_shiftCyclesLeft = 18;
    m_interruptFlag = false;
    Update(2);
}

uint8_t ShiftRegister::ReadValue() const {
    m_shiftCyclesLeft = 18;
    m_interruptFlag = false;
    return m_value;
}

void ShiftRegister::Update(cycles_t cycles) {
    for (int i = 0; i < cycles; ++i) {
        if (m_shiftCyclesLeft > 0) {
            if (m_shiftCyclesLeft % 2 == 1) {
                bool isLastShiftCycle = m_shiftCyclesLeft == 1;
                if (isLastShiftCycle) {
                    // For the last (9th) shift cycle, we output the same bit that was output for
                    // the 8th, which is now in bit position 0. We also don't shift (is that
                    // correct?)
                    uint8_t bit = TestBits01(m_value, BITS(0));
                    m_cb2Active = bit == 0;
                } else {
                    uint8_t bit = TestBits01(m_value, BITS(7));
                    m_cb2Active = bit == 0;
                    m_value = (m_value << 1) | bit;
                }
            }
            --m_shiftCyclesLeft;

            // Interrupt enable once we're done shifting
            if (m_shiftCyclesLeft == 0)
                m_interruptFlag = true;
        }
    }
}
