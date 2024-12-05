#pragma once

#include "MemoryBus.h"
#include "MemoryMap.h"
#include "Psg.h"
#include "Screen.h"
#include "ShiftRegister.h"
#include "core/Line.h"
#include "core/MathUtil.h"
#include "emulator/Timers.h"

class Input;
struct RenderContext;
struct AudioContext;

// Implementation of the 6522 Versatile Interface Adapter (VIA)
// Used to control all of the Vectrex peripherals, such as keypads, vector generator, DAC, sound
// chip, etc.

class Via : public IMemoryBusDevice {
public:
    void Init(MemoryBus& memoryBus);
    void Reset();

    void SetSyncContext(const Input& input, RenderContext& renderContext,
                        AudioContext& audioContext) {
        m_syncContext = {&input, &renderContext, &audioContext};
    }

    void FrameUpdate(double frameTime);

    bool IrqEnabled() const;
    bool FirqEnabled() const;

    Screen& GetScreen() { return m_screen; }

private:
    uint8_t Read(uint16_t address) const override;
    void Write(uint16_t address, uint8_t value) override;
    void Sync(cycles_t cycles) override;
    void DoSync(cycles_t cycles, const Input& input, RenderContext& renderContext,
                AudioContext& audioContext);
    uint8_t GetInterruptFlagValue() const;

    struct SyncContext {
        const Input* input{};
        RenderContext* renderContext{};
        AudioContext* audioContext{};
    } m_syncContext;

    // Registers
    uint8_t m_portB{};
    uint8_t m_portA{};
    uint8_t m_dataDirB{};
    uint8_t m_dataDirA{};
    uint8_t m_periphCntl{};
    uint8_t m_interruptEnable{};

    Screen m_screen; // TODO: Move to Emulator
    Psg m_psg;
    Timer1 m_timer1;
    Timer2 m_timer2;
    ShiftRegister m_shiftRegister;
    uint8_t m_joystickButtonState{};
    int8_t m_joystickPot{};
    bool m_ca1Enabled{};
    mutable bool m_ca1InterruptFlag{};
    bool m_firqEnabled{};
    float m_elapsedAudioCycles{};
    MathUtil::AverageValue m_directAudioSamples;
    MathUtil::AverageValue m_psgAudioSamples;
};
