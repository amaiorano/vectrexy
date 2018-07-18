#pragma once

#include "Base.h"
#include "BitOps.h"
#include <array>
#include <memory>

struct AudioContext;

// Timer used by Tone and Noise Generators
class Timer {
public:
    Timer(uint32_t period = 0) { SetPeriod(period); }

    // Resets time
    void SetPeriod(uint32_t period) {
        m_period = period;
        Reset();
    }
    uint32_t Period() const { return m_period; }

    void Reset() { m_time = 0; }

    // Returns true when timer expires (and auto-resets)
    bool Clock() {
        if ((m_period > 0) && (++m_time == m_period)) {
            Reset();
            return true;
        }
        return false;
    }

private:
    uint32_t m_period{};
    uint32_t m_time{}; // Time in period
};

class ToneGenerator {
public:
    void SetPeriodHigh(uint8_t high) {
        assert(high <= 0xf); // Only 4 bits should be set
        m_period = (high << 8) | (m_period & 0x00ff);
        OnPeriodUpdated();
    }
    void SetPeriodLow(uint8_t low) {
        m_period = (m_period & 0xff00) | low;
        OnPeriodUpdated();
    }

    uint8_t PeriodHigh() const {
        return checked_static_cast<uint8_t>(m_period >> 8); // Top 4 bits
    }
    uint8_t PeriodLow() const { return checked_static_cast<uint8_t>(m_period & 0xff); }

    void Clock() {
        if (/*m_period > 0 &&*/ m_timer.Clock()) {
            m_value = (m_value == 0 ? 1 : 0);
        }
    }

    uint32_t Value() const { return m_value; }

private:
    void OnPeriodUpdated() {
        auto duty = std::max<uint32_t>(1, m_period / 2);
        m_timer.SetPeriod(duty);
        m_value = 0;
    }

    Timer m_timer;
    uint32_t m_period{}; // 12 bit value [0,4095]
    uint32_t m_value{};  // 0 or 1
};

class NoiseGenerator {
public:
    void SetPeriod(uint8_t period) {
        assert(period < 32);
        m_period = std::max<uint8_t>(1, period & 0b0001'1111);
        OnPeriodUpdated();
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

    void OnPeriodUpdated() {
        m_timer.SetPeriod(m_period);
        // m_value = 0;
    }

    Timer m_timer;
    uint32_t m_period{};          // 5 bit value [0,32]
    uint32_t m_shiftRegister = 1; // Must be initialized to a non-zero value
    uint32_t m_value{};           // 0 or 1
};

class EnvelopeGenerator {
public:
    void SetPeriodHigh(uint8_t high) {
        m_period = (high << 8) | (m_period & 0x00ff);
        OnPeriodUpdated();
    }
    void SetPeriodLow(uint8_t low) {
        m_period = (m_period & 0xff00) | low;
        OnPeriodUpdated();
    }

    void SetShape(uint8_t shape) {
        assert(shape < 16);
        m_shape = shape;
        m_currShapeIndex = 0;
        UpdateValue();
    }

    uint8_t PeriodHigh() const { return checked_static_cast<uint8_t>(m_period >> 8); }

    uint8_t PeriodLow() const { return checked_static_cast<uint8_t>(m_period & 0xff); }

    uint8_t Shape() { return m_shape; }

    void Clock() {
        if (/*m_period > 0 &&*/ m_divider.Clock() && m_timer.Clock()) {
            UpdateValue();
        }
    }

    uint32_t Value() const { return m_value; }

private:
    void OnPeriodUpdated() {
        //@TODO: why am I dividing by 16 here?
        auto timeToIncrementShapeIndex = std::max<uint32_t>(1, m_period / 16);
        m_timer.SetPeriod(timeToIncrementShapeIndex);
        UpdateValue();
    }

    void UpdateValue() {
        using Shape = std::array<uint32_t, 32>;
        using Table = std::array<Shape, 16>;

        // 4-bit pattern where bits:
        // Bit 3: continue
        // Bit 2: attack
        // Bit 1: alternate
        // Bit 0: hold
        // Instead of a state machine, we use a lookup table.
        // This table contains 2 cycles worth of values.
        static const Table table = {
            // clang-format off
            // 0000 to 0011
            Shape{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
            Shape{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
            Shape{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
            Shape{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
            // 0100 to 0111
            Shape{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
            Shape{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
            Shape{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
            Shape{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
            // 1000
            Shape{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 },
            // 1001
            Shape{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
            // 1010
            Shape{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 },
            // 1011
            Shape{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15 },
            // 1100
            Shape{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 },
            // 1101
            Shape{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15 },
            // 1110
            Shape{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 },
            // 1111
            Shape{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
            // clang-format on
        };

        const Shape& shape = table[m_shape];

        m_value = shape[m_currShapeIndex];

        // If we're on the last value, then check if we should hold this last value or restart the
        // shape.
        const bool holdCurrentIndex = [this, &shape] {
            if (m_currShapeIndex < (shape.size() - 1))
                return false;

            bool continuePattern = TestBits(m_shape, 0b1000);
            if (!continuePattern)
                return true;

            bool holdPattern = TestBits(m_shape, 0b0001);
            return holdPattern;
        }();

        if (!holdCurrentIndex) {
            m_currShapeIndex = (m_currShapeIndex + 1) % shape.size();
        }
    }

    Timer m_divider{16}; // Envelope further divides by 16 (so it produces values every 256 cycles)
    Timer m_timer;
    uint32_t m_period{}; // 16 bit value
    uint32_t m_value{};  // 0 or 1

    uint8_t m_shape{};          // 4 bit shape index
    uint8_t m_currShapeIndex{}; // Index into current shape (0-31)
};

enum class AmplitudeMode { Fixed, Envelope };

class AmplitudeControl {
public:
    AmplitudeControl(EnvelopeGenerator& envelopeGenerator)
        : m_envelopeGenerator(envelopeGenerator) {}

    void SetMode(AmplitudeMode mode) { m_mode = mode; }
    void SetFixedVolume(uint32_t volume) { m_fixedVolume = volume; }

    // Returns volume in [0,1]
    float Volume() const {
        uint32_t volume = 0;
        switch (m_mode) {
        case AmplitudeMode::Fixed:
            volume = m_fixedVolume;
            break;
        case AmplitudeMode::Envelope:
            volume = m_envelopeGenerator.Value();
            break;
        }

        assert(volume < 16);

        // There's a bug in the Vectrex BIOS Clear_Sound routine ($F272) that is suppposed to
        // initialize the PSG registers to 0, but instead, only does so for one register, and
        // sets the rest to 1. This makes it so that we hear some noise when we reset. We can
        // "fix" this hear by considering volume 1 to be silent.
        if (volume <= 1)
            return 0.f;

        // The volume is non-linear. Below formula does comply with the PSG datasheet, and does
        // more or less match the voltages measured on the CPCs speaker (the voltages on the
        // CPCs stereo connector seem to be slightly different though). amplitude = max /
        // sqrt(2)^(15-nn) eg. 15 --> max / 1, 14 --> max / 1.414, 13 --> max / 2, etc.
        // http://www.cpcwiki.eu/index.php/PSG#0Ah_-_Channel_C_Volume_.280-0Fh.3Dvolume.2C_10h.3Duse_envelope_instead.29
        return 1.f / ::powf(::sqrtf(2), 15.f - volume);
    }

private:
    AmplitudeMode m_mode = AmplitudeMode::Fixed;
    uint32_t m_fixedVolume{};
    EnvelopeGenerator& m_envelopeGenerator;
};

class PsgChannel {
public:
    PsgChannel(ToneGenerator& toneGenerator, NoiseGenerator& noiseGenerator,
               EnvelopeGenerator& envelopeGenerator)
        : m_toneGenerator(toneGenerator)
        , m_noiseGenerator(noiseGenerator)
        , m_amplitudeControl(envelopeGenerator) {}

    bool ToneEnabled() const { return m_toneEnabled; }
    bool NoiseEnabled() const { return m_noiseEnabled; }
    void SetToneEnabled(bool enabled) { m_toneEnabled = enabled; }
    void SetNoiseEnabled(bool enabled) { m_noiseEnabled = enabled; }
    AmplitudeControl& GetAmplitudeControl() { return m_amplitudeControl; }
    const ToneGenerator& GetToneGenerator() const { return m_toneGenerator; }
    const NoiseGenerator& GetNoiseGenerator() const { return m_noiseGenerator; }

    bool OverrideToneEnabled = true;
    bool OverrideNoiseEnabled = true;

    float Sample() const {
        float volume = m_amplitudeControl.Volume();

        // If both Tone and Noise are disabled on a channel, then a constant HIGH level is output
        // (useful for digitized speech). If both Tone and Noise are enabled on the same channel,
        // then the signals are ANDed (the signals aren't ADDed) (ie. HIGH is output only if both
        // are HIGH).
        // http://www.cpcwiki.eu/index.php/PSG#07h_-_Mixer_Control_Register

        uint32_t sample = 0;
        const bool toneEnabled = m_toneEnabled && OverrideToneEnabled;
        const bool noiseEnabled = m_noiseEnabled && OverrideNoiseEnabled;

        if (toneEnabled && noiseEnabled) {
            sample = m_toneGenerator.Value() & m_noiseGenerator.Value();
        } else if (toneEnabled) {
            sample = m_toneGenerator.Value();
        } else if (noiseEnabled) {
            sample = m_noiseGenerator.Value();
        } else {
            return 0.f; // No sound, return "center" value
        }

        // Convert int sample [0,1] to float [-1,1]
        auto finalSample = (sample * 2.f) - 1.f;

        // Apply volume
        finalSample *= volume;

        return finalSample;
    }

private:
    bool m_toneEnabled{};
    bool m_noiseEnabled{};
    ToneGenerator& m_toneGenerator;
    NoiseGenerator& m_noiseGenerator;
    AmplitudeControl m_amplitudeControl;
};

// Implementation of the AY-3-8912 Programmable Sound Generator (PSG)

class Psg {
public:
    Psg();
    void Init();

    void SetBDIR(bool enable) { m_BDIR = enable; }
    void SetBC1(bool enable) { m_BC1 = enable; }
    bool BDIR() const { return m_BDIR; }
    bool BC1() const { return m_BC1; }

    void WriteDA(uint8_t value);
    uint8_t ReadDA();

    void Reset();
    void Update(cycles_t cycles);

    bool IsProducingSound() const;
    float Sample() const;

    void FrameUpdate();

private:
    void Clock();

    uint8_t Read(uint16_t address);
    void Write(uint16_t address, uint8_t value);

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
    Timer m_masterDivider{16}; // Input clock divided by 16
    std::array<ToneGenerator, 3> m_toneGenerators{};
    NoiseGenerator m_noiseGenerator{};
    EnvelopeGenerator m_envelopeGenerator{};
    std::array<PsgChannel, 3> m_channels;
};
