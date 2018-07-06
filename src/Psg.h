#pragma once

#include "Base.h"
#include "BitOps.h"
#include <array>
#include <memory>

struct AudioContext;

//@TODO: get rid of this and just use Timer
class Divider {
public:
    Divider(uint32_t period = 0) {
        SetPeriod(period);
        Reset();
    }
    void SetPeriod(uint32_t period) { m_period = period; }
    void Reset() { m_counter = m_period; }
    bool Clock() {
        if (m_period > 0 && --m_counter == 0) {
            m_counter = m_period;
            return true;
        }
        return false;
    }

private:
    uint32_t m_period{};
    uint32_t m_counter{};
};

// Timer used by Tone and Noise Generators
class Timer {
public:
    Timer(uint32_t period = 0) { SetPeriod(period); }

    // Resets time
    void SetPeriod(uint32_t period) {
        m_period = period;
        ResetTime();
    }
    uint32_t Period() const { return m_period; }

    // Returns true when timer expires (and auto-resets)
    bool Clock() {
        if (m_time > 0 && --m_time == 0) {
            ResetTime();
            return true;
        }
        return false;
    }

private:
    void ResetTime() { m_time = m_period; }

    uint32_t m_period{};
    uint32_t m_time{}; // Time in period
};

class ToneGenerator {
public:
    void SetPeriodHigh(uint8_t high) {
        assert(high <= 0xf); // Only 4 bits should be set
        m_period = (high << 8) | (m_period & 0x00ff);
        UpdateTimer();
    }
    void SetPeriodLow(uint8_t low) {
        m_period = (m_period & 0xff00) | low;
        UpdateTimer();
    }

    uint8_t PeriodHigh() const {
        return checked_static_cast<uint8_t>(m_period >> 8); // Top 4 bits
    }
    uint8_t PeriodLow() const { return checked_static_cast<uint8_t>(m_period & 0xff); }

    void Clock() {
        if (m_timer.Clock()) {
            m_value = m_value == 0 ? 1 : 0;
        }
    }

    uint32_t Value() const { return m_value; }

private:
    void UpdateTimer() {
        auto duty = std::max<uint32_t>(1, m_period / 2);
        m_timer.SetPeriod(duty);
    }

    Timer m_timer;
    uint32_t m_period{}; // 12 bit value [0,4095]
    uint32_t m_value{};  // 0 or 1
};

class NoiseGenerator {
public:
    void SetPeriod(uint8_t period) {
        assert(period < 32);
        period = std::max<uint8_t>(1, period & 0b0001'1111);
        m_timer.SetPeriod(period);
    }

    uint8_t Period() const { return static_cast<uint8_t>(m_timer.Period()); }

    void Clock() {
        if (m_timer.Clock()) {
            ClockShiftRegister();
        }
    }

    uint32_t Value() const { return m_value; }

private:
    void ClockShiftRegister() {
        // From http://www.cpcwiki.eu/index.php/PSG#06h_-_Noise_Frequency_.285bit.29
        // noise_level = noise_level XOR shiftreg.bit0
        // newbit = shiftreg.bit0 XOR shiftreg.bit3
        // shiftreg = (shiftreg SHR 1) + (newbit SHL 16)
        uint32_t bit0 = ReadBits(m_shiftRegister, BITS(0));
        uint32_t bit3 = ReadBits(m_shiftRegister, BITS(3)) >> 3;
        m_value = m_value ^ bit0;
        uint32_t newBit = bit0 ^ bit3;
        m_shiftRegister = (m_shiftRegister >> 1) | (newBit << 16);
        ASSERT(m_shiftRegister < BITS(18));
    }

    Timer m_timer;
    uint32_t m_shiftRegister = 1; // Must be initialized to a non-zero value
    uint32_t m_value{};           // 0 or 1
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
    void Update(cycles_t cycles, AudioContext& audioContext);

private:
    void Clock(AudioContext& audioContext);

    uint8_t Read(uint16_t address);
    void Write(uint16_t address, uint8_t value);

    float SampleChannelsAndMix();

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
    std::array<uint8_t, 16> m_registers{};
    Divider m_masterDivider{16}; // Input clock divided by 16
    std::array<ToneGenerator, 3> m_toneGenerators{};
    NoiseGenerator m_noiseGenerator{};

    float m_sampleSum{};
    float m_numSamples{};
};
