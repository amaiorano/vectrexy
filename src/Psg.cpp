#include "Psg.h"
#include "BitOps.h"

namespace {
    namespace Register {
        enum Type {
            ChannelALow = 0,
            ChannelAHigh = 1,
            ChannelBLow = 2,
            ChannelBHigh = 3,
            ChannelCLow = 4,
            ChannelCHigh = 5,

            MixerControl = 7,

            AmplitudeA = 10,
            AmplitudeB = 11,
            AmplitudeC = 12,
        };
    }

    namespace MixerControl {
        enum Type {
            ToneA = BITS(0),
            ToneB = BITS(1),
            ToneC = BITS(2),
            NoiseA = BITS(3),
            NoiseB = BITS(4),
            NoiseC = BITS(5),
            // Bits 6 and 7 are to control IO ports A and B, but we don't use this on Vectrex
        };

        bool IsEnabled(uint8_t reg, Type type) {
            // Enabled when bit is 0
            return !TestBits(reg, type);
        }

    } // namespace MixerControl

    namespace AmplitudeControl {
        enum Type {
            FixedVolume = BITS(0, 1, 2, 3),
            EnvelopeMode = BITS(4),
            Unused = BITS(5, 6, 7),
        };

        enum class Mode { Fixed, Envelope };
        Mode GetMode(uint8_t reg) {
            return TestBits(reg, EnvelopeMode) ? Mode::Envelope : Mode::Fixed;
        }

        float GetFixedVolumeRatio(uint8_t reg) {
            assert(GetMode(reg) == Mode::Fixed);
            return ReadBits(reg, FixedVolume) / 16.f;
        }

    } // namespace AmplitudeControl
} // namespace

void Psg::Init() {
    Reset();
}

void Psg::WriteDA(uint8_t value) {
    m_DA = value;
}

uint8_t Psg::ReadDA() {
    return m_DA;
}

void Psg::Reset() {
    m_mode = {};
    m_DA = {};
    m_registers.fill(0);
    m_masterDivider.Reset();
    m_squareWaveGenerators = {};
}

void Psg::Update(cycles_t cycles) {
    for (cycles_t cycle = 0; cycle < cycles; ++cycle) {
        Clock();
    }
}

void Psg::Clock() {
    auto ModeFromBDIRandBC1 = [](bool BDIR, bool BC1) -> Psg::PsgMode {
        uint8_t value{};
        SetBits(value, 1, BDIR);
        SetBits(value, 0, BC1);
        return static_cast<Psg::PsgMode>(value);
    };

    const auto lastMode = m_mode;
    m_mode = ModeFromBDIRandBC1(m_BDIR, m_BC1);

    switch (m_mode) {
    case PsgMode::Inactive:
        break;
    case PsgMode::Read:
        if (lastMode == PsgMode::Inactive) {
            m_DA = Read(m_latchedAddress);
        }
        break;
    case PsgMode::Write:
        if (lastMode == PsgMode::Inactive) {
            Write(m_latchedAddress, m_DA);
        }
        break;
    case PsgMode::LatchAddress:
        if (lastMode == PsgMode::Inactive) {
            m_latchedAddress = ReadBits(m_DA, 0b1111);
        }
        break;
    }

    // Clock generators every 16 input clocks
    if (m_masterDivider.Clock()) {
        for (auto& swg : m_squareWaveGenerators) {
            swg.Clock();
        }
    }

    auto SampleChannel = [this](Register::Type amplitudeRegister,
                                MixerControl::Type toneMixerControl,
                                MixerControl::Type noiseMixerControl) -> float {
        assert(amplitudeRegister >= Register::AmplitudeA &&
               amplitudeRegister <= Register::AmplitudeC);

        assert(toneMixerControl == MixerControl::ToneA || toneMixerControl == MixerControl::ToneB ||
               toneMixerControl == MixerControl::ToneC);

        assert(noiseMixerControl == MixerControl::NoiseA ||
               noiseMixerControl == MixerControl::NoiseB ||
               noiseMixerControl == MixerControl::NoiseC);

        const int toneGeneratorIndex = amplitudeRegister - Register::AmplitudeA;

        auto& reg = m_registers[amplitudeRegister];

        const float volume = [&reg] {
            if (AmplitudeControl::GetMode(reg) == AmplitudeControl::Mode::Fixed) {
                return AmplitudeControl::GetFixedVolumeRatio(reg);
            } else {
                //@TODO: envelope volume...
                return 0.f;
            }
        }();

        if (volume == 0.f)
            return 0.f;

        float sample = 0;
        float numValues = 0;
        if (MixerControl::IsEnabled(reg, toneMixerControl)) {
            sample += m_squareWaveGenerators[toneGeneratorIndex].Value();
            ++numValues;
        }

        if (MixerControl::IsEnabled(reg, noiseMixerControl)) {
            //@TODO:
            // sample += m_noiseGenerator.Value();
            //++numValues;
        }

        // sample = numValues > 0 ? sample / numValues : sample;

        return sample * volume;
    };

    auto SampleAllChannels = [this, &SampleChannel] {
        float sample = 0.f;
        sample += SampleChannel(Register::AmplitudeA, MixerControl::ToneA, MixerControl::NoiseA);
        sample += SampleChannel(Register::AmplitudeB, MixerControl::ToneB, MixerControl::NoiseB);
        sample += SampleChannel(Register::AmplitudeC, MixerControl::ToneC, MixerControl::NoiseC);
        sample /= 6;
        return sample;
    };
}

uint8_t Psg::Read(uint16_t address) {
    switch (m_latchedAddress) {
    case Register::ChannelAHigh:
        return m_squareWaveGenerators[0].PeriodHigh();
    case Register::ChannelALow:
        return m_squareWaveGenerators[0].PeriodLow();
    case Register::ChannelBHigh:
        return m_squareWaveGenerators[1].PeriodHigh();
    case Register::ChannelBLow:
        return m_squareWaveGenerators[1].PeriodLow();
    case Register::ChannelCHigh:
        return m_squareWaveGenerators[2].PeriodHigh();
    case Register::ChannelCLow:
        return m_squareWaveGenerators[2].PeriodLow();
    }

    return m_registers[address];
}

void Psg::Write(uint16_t address, uint8_t value) {
    switch (m_latchedAddress) {
    case Register::ChannelAHigh:
        return m_squareWaveGenerators[0].SetPeriodHigh(value);
    case Register::ChannelALow:
        return m_squareWaveGenerators[0].SetPeriodLow(value);
    case Register::ChannelBHigh:
        return m_squareWaveGenerators[1].SetPeriodHigh(value);
    case Register::ChannelBLow:
        return m_squareWaveGenerators[1].SetPeriodLow(value);
    case Register::ChannelCHigh:
        return m_squareWaveGenerators[2].SetPeriodHigh(value);
    case Register::ChannelCLow:
        return m_squareWaveGenerators[2].SetPeriodLow(value);
    case Register::MixerControl:
        ASSERT_MSG(ReadBits(value, 0b1100'0000) == 0, "Not supporting I/O ports on PSG");
        break;
    }

    m_registers[address] = value;
}
