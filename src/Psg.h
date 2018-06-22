#pragma once

#include "Base.h"
#include <array>
#include <memory>

struct SquareWaveGenerator {
    uint32_t m_period{}; // 12 bit value [0,4095]
    uint32_t m_duty{};   // Period / 2 (number of clocks before oscilating)
    uint32_t m_time{};   // Time in period
    uint16_t m_value{};  // 0 or 1

    static const uint32_t PeriodScale = 16; // Psg hardware always counts min 16

    void SetPeriodHigh(uint8_t high) {
        assert(high <= 0xf); // Only 4 bits should be set
        m_period |= high << 16;
        UpdateDuty();
    }
    void SetPeriodLow(uint8_t low) {
        m_period |= low;
        UpdateDuty();
    }

    uint8_t PeriodHigh() const {
        return checked_static_cast<uint8_t>(m_period >> 16); // Top 12 bits
    }
    uint8_t PeriodLow() const { return checked_static_cast<uint8_t>(m_period & 0xff); }

    void Clock() {
        if (--m_time == 0) {
            m_value = m_value == 0 ? 1 : 0;
            m_time = m_duty;
        }
    }

    uint16_t Value() const { m_value; }

private:
    void UpdateDuty() {
        m_duty = (std::max<uint32_t>(1, m_period) * PeriodScale) / 2;
        assert(m_duty > 0);
        // Should we reset time?
    }
};

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

    enum class Channel { A, B, C };

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
    std::array<SquareWaveGenerator, 3> m_squareWaveGenerators;
};
