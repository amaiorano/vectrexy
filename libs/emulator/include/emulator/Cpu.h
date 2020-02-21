#pragma once

#include "core/Base.h"
#include "core/Pimpl.h"

class MemoryBus;

// Implementation of Motorola 68A09 1.5 MHz 8-Bit Microprocessor

class CpuRegisters {
public:
    struct ConditionCode {
        union {
            struct {
#if BITFIELDS_MSB_TO_LSB
                uint8_t Entire : 1;
                uint8_t FastInterruptMask : 1; // FIRQ
                uint8_t HalfCarry : 1;
                uint8_t InterruptMask : 1; // IRQ
                uint8_t Negative : 1;
                uint8_t Zero : 1;
                uint8_t Overflow : 1; // V
                uint8_t Carry : 1;
#else
                uint8_t Carry : 1;
                uint8_t Overflow : 1; // V
                uint8_t Zero : 1;
                uint8_t Negative : 1;
                uint8_t InterruptMask : 1; // IRQ
                uint8_t HalfCarry : 1;
                uint8_t FastInterruptMask : 1; // FIRQ
                uint8_t Entire : 1;
#endif
            };
            uint8_t Value; // Use only to reset to 0 or serialize
        };
    };
    static_assert(sizeof(ConditionCode) == 1, "");

    // Registers
    uint16_t X;  // index register
    uint16_t Y;  // index register
    uint16_t U;  // user stack pointer
    uint16_t S;  // hardware stack pointer
    uint16_t PC; // program counter
    union        // accumulators
    {
        struct {
#if ENDIANESS_LITTLE
            uint8_t B;
            uint8_t A;
#else
            uint8_t A;
            uint8_t B;
#endif
        };
        uint16_t D;
    };
    uint8_t DP;       // direct page register (msb of zero-page address)
    ConditionCode CC; // condition code register (aka status register)
};

class Cpu {
public:
    static constexpr double Hz = 6'000'000.0 / 4.0; // Frequency of the CPU (cycles/second)

    Cpu();
    ~Cpu();

    void Init(MemoryBus& memoryBus);
    void Reset();
    cycles_t ExecuteInstruction(bool irqEnabled, bool firqEnabled);

    const CpuRegisters& Registers() const;

private:
    pimpl::Pimpl<class CpuImpl, 48> m_impl;
};
