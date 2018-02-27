#pragma once

#include "Line.h"
#include "MemoryBus.h"
#include "MemoryMap.h"
#include "ShiftRegister.h"
#include "Timers.h"
#include "Vector2.h"
#include <array>

class Input;
struct RenderContext;

// Implementation of the 6522 Versatile Interface Adapter (VIA)
// Used to control all of the Vectrex peripherals, such as keypads, vector generator, DAC, sound
// chip, etc.

class Via : public IMemoryBusDevice {
public:
    void Init(MemoryBus& memoryBus);
    void Reset();
    void Update(cycles_t cycles, const Input& input, RenderContext& renderContext);

    bool IrqEnabled() const;

private:
    uint8_t Read(uint16_t address) const override;
    void Write(uint16_t address, uint8_t value) override;
    uint8_t GetInterruptFlagValue() const;

    // Registers
    uint8_t m_portB;
    uint8_t m_portA;
    uint8_t m_dataDirB;
    uint8_t m_dataDirA;
    uint8_t m_periphCntl;
    uint8_t m_interruptEnable;

    // Render state
    Vector2 m_pos;
    Vector2 m_velocity;
    float m_xyOffset = 0.f;
    float m_brightness = 0.f;
    bool m_blank = false;

    Timer1 m_timer1;
    Timer2 m_timer2;
    ShiftRegister m_shiftRegister;
    uint8_t m_joystickButtonState;
    int8_t m_joystickPot{};
};
