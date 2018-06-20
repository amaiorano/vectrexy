#pragma once

#include "Base.h"
#include <array>
#include <memory>

// Implementation of the AY-3-8912 Programmable Sound Generator (PSG)

class Psg {
public:
    void Init();

    void SetBDIR(bool enable) { m_BDIR = enable; }
    void SetBC1(bool enable) { m_BC1 = enable; }
    bool BDIR() const { return m_BDIR; }
    bool BC1() const { return m_BC1; }

    void WriteDA(uint8_t value);
    uint8_t ReadDA();

    void Reset();
    void Update(cycles_t cycles);

private:
    void Clock();

    uint8_t Read(uint16_t address);
    void Write(uint16_t address, uint8_t value);

    enum class Channel { A, B, C, NumTypes };

    enum class PsgMode {
        // Selected from BDIR (bit 1) and BC1 (bit 0) values
        Inactive,    // BDIR off BC1 off
        Read,        // BDIR off BC1 on
        Write,       // BDIR on  BC1 off
        LatchAddress // BDIR on  BC1 on
    };

    PsgMode m_mode = PsgMode::Inactive;

    bool m_BDIR{};
    bool m_BC1{};
    uint8_t m_DA{}; // Data/Address bus (DA7-DA0)
    uint8_t m_latchedAddress{};
    std::array<uint8_t, 16> m_registers;

    std::unique_ptr<struct SquareWaveGenerator> m_squareWaveGenerators[(size_t)Channel::NumTypes];
};
